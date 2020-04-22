//====== Copyright (c) 1996-2007, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "tier1/strtools.h"
#include <sys/stat.h>
#include "bitmap/bitmap.h"
#include "bitmap/TGALoader.h"
#include "bitmap/psd.h"
#include "bitmap/floatbitmap.h"
#include "bitmap/imageformat.h"
#include "mathlib/mathlib.h"

#ifdef PLATFORM_POSIX
#include <sys/stat.h>
#define _stat stat
#endif

#ifdef PLATFORM_WINDOWS
#include "conio.h"
#include <direct.h>
#include <io.h>
#endif

#include "vtf/vtf.h"
#include "UtlBuffer.h"
#include "tier0/dbg.h"
#include "cmdlib.h"
#include "tier0/icommandline.h"
#ifdef PLATFORM_WINDOWS
#include "windows.h"
#endif
#include "ilaunchabledll.h"
#include "ivtex.h"
#include "appframework/IAppSystemGroup.h"
#include "datamodel/dmelement.h"
#include "materialobjects/dmetexture.h"
#include "tier2/tier2dm.h"
#include "tier2/p4helpers.h"
#include "p4lib/ip4.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "resourcesystem/iresourcecompiler.h"
#include "rendersystem/schema/texture.g.h"
#include "materialobjects/dmeprecompiledtexture.h"
#include "vstdlib/jobthread.h"
#include "tier1/checksum_crc.h"
#include "tier1/keyvalues.h"

#define FF_PROCESS 1
#define FF_TRYAGAIN 2
#define FF_DONTPROCESS 3

#define LOWRESIMAGE_DIM 16

#ifdef PLATFORM_POSIX
#define LOWRES_IMAGE_FORMAT IMAGE_FORMAT_RGBA8888
#else
#define LOWRES_IMAGE_FORMAT IMAGE_FORMAT_DXT1
#endif

//#define DEBUG_NO_COMPRESSION
static bool g_NoPause = false;
static bool g_Quiet = false;
static const char *g_ShaderName = NULL;
static bool g_CreateDir = true;
static bool g_UseGameDir = true;

static bool g_bUseStandardError = false;
static bool g_bWarningsAsErrors = false;

static bool g_bUsedAsLaunchableDLL = false;

static bool g_bNoTga = false;
static bool g_bNoPsd = false;
static bool g_bUsePfm = false;

static bool g_bSupportsXBox360 = false;

static char g_ForcedOutputDir[MAX_PATH];

static bool g_bOldCubemapPath = false;


#define MAX_VMT_PARAMS	16

struct VTexVMTParam_t
{
	const char *m_szParam;
	const char *m_szValue;
};

class SmartIVTFTexture
{
public:
	explicit SmartIVTFTexture( IVTFTexture *pVtf ) : m_p( pVtf ) {}
	~SmartIVTFTexture() { if ( m_p ) DestroyVTFTexture( m_p ); }

private:
	SmartIVTFTexture( SmartIVTFTexture const &x );
	SmartIVTFTexture & operator = ( SmartIVTFTexture const &x );

private:
	SmartIVTFTexture & operator = ( IVTFTexture *pVtf ) { m_p = pVtf; }
	operator IVTFTexture * () const { return m_p; }

public:
	IVTFTexture * Assign( IVTFTexture *pVtfNew ) { IVTFTexture *pOld = m_p; m_p = pVtfNew; return pOld; }
	IVTFTexture * Get() const { return m_p; }
	IVTFTexture * operator->() const { return m_p; }

protected:
	IVTFTexture *m_p;
};

struct OutputTexture_t
{
	IVTFTexture *	pTexture;
	char			dstFileName[ MAX_PATH ];
};

static VTexVMTParam_t g_VMTParams[MAX_VMT_PARAMS];

static int g_NumVMTParams = 0;

static BitmapFileType_t g_eMode = BITMAP_FILE_TYPE_PSD;

// NOTE: these must stay in the same order as CubeMapFaceIndex_t.
static const char *g_CubemapFacingNames[7] = { "rt", "lf", "bk", "ft", "up", "dn", "sph" };

static void Pause( void )
{
	if( !g_NoPause )
	{
		printf( "\nHit a key to continue\n" );
#ifdef PLATFORM_WINDOWS
		getch();
#endif	
	}
}

static bool VTexErrorAborts()
{
	if ( CommandLine()->FindParm( "-crcvalidate" ) )
		return false;

	return true;
}

#if defined( _DEBUG ) && defined( _WIN32 )
#define DebuggerOutput2(x, y) (void)( OutputDebugString( x ), OutputDebugString( y ) )
#else
#define DebuggerOutput2(x, y) (void)( (x), (y) )
#endif

static void VTexError( const char *pFormat, ... )
{
	char str[4096];
	va_list marker;
	va_start( marker, pFormat );
	Q_vsnprintf( str, sizeof( str ), pFormat, marker );
	va_end( marker );

	DebuggerOutput2( "[VTEXDBG] ERROR: ", str );

	if ( !VTexErrorAborts() )
	{
		fprintf( stderr, "ERROR: %s", str );
		return;
	}

	if ( g_bUseStandardError )
	{
		Error( "ERROR: %s", str );
	}
	else
	{
		fprintf( stderr, "ERROR: %s", str );
		Pause();
		exit( 1 );
	}	
}


static void VTexWarning( const char *pFormat, ... )
{
	char str[4096];
	va_list marker;
	va_start( marker, pFormat );
	Q_vsnprintf( str, sizeof( str ), pFormat, marker );
	va_end( marker );

	DebuggerOutput2( "[VTEXDBG] WARNING: ", str );

	if ( g_bWarningsAsErrors )
	{
		VTexError( "%s", str );
	}
	else
	{
		fprintf( stderr, "WARNING: %s", str );
		Pause();
	}	
}

static void VTexWarningNoPause( const char *pFormat, ... )
{
	char str[4096];
	va_list marker;
	va_start( marker, pFormat );
	Q_vsnprintf( str, sizeof( str ), pFormat, marker );
	va_end( marker );

	DebuggerOutput2( "[VTEXDBG] WARNING: ", str );

	if ( g_bWarningsAsErrors )
	{
		VTexError( "%s", str );
	}
	else
	{
		fprintf( stderr, "WARNING: %s", str );
	}	
}

static void VTexMsg( const char *pFormat, ... )
{
	char str[4096];
	va_list marker;
	va_start( marker, pFormat );
	Q_vsnprintf( str, sizeof( str ), pFormat, marker );
	va_end( marker );

	DebuggerOutput2( "[VTEXDBG] MSG: ", str );

	fprintf( stdout, "%s", str );
}

static void VTexMsgEx( FILE *fout, const char *pFormat, ... )
{
	char str[4096];
	va_list marker;
	va_start( marker, pFormat );
	Q_vsnprintf( str, sizeof( str ), pFormat, marker );
	va_end( marker );

	DebuggerOutput2( "[VTEXDBG] MSG: ", str );

	fprintf( fout, "%s", str );
}


struct VTexConfigInfo_t
{
	int m_nStartFrame = -1;
	int m_nEndFrame = -1;
	unsigned int m_nFlags = 0;
	float m_flBumpScale = 1.0;
	bool m_bNormalToDuDv = false;
	bool m_bNormalToDXT5GA = false;
	bool m_bNormalInvertGreen = false;
	bool m_bAlphaToLuminance = false;
	bool m_bDuDv = false;
	float m_flAlphaThreshhold = -1.0f;
	float m_flAlphaHiFreqThreshhold = -1.0f;
	int m_nVolumeTextureDepth = 1;
	float m_pfmscale = 1.0;
	bool m_bStripAlphaChannel = false;
	bool m_bStripColorChannel = false;
	bool m_bIsCubeMap = false;
	bool m_bIsSkyBox = false;
	bool m_bIsCroppedSkyBox = false;
	bool m_bManualMip = false;
	bool m_bDisplacementMap = false;
	bool m_bDisplacementWrinkleMap = false;

	// scaling parameters
	int m_nReduceX = 1;
	int m_nReduceY = 1;

	int m_nMaxDimensionX = -1;
	int m_nMaxDimensionX_360 = -1;
	int m_nMaxDimensionY = -1;
	int m_nMaxDimensionY_360 = -1;

	// may restrict the texture to reading only 3 channels
	int m_numChannelsMax = 4;

	bool m_bAlphaToDistance = false;
	float m_flDistanceSpread = 1.0;					 // how far to stretch out distance range in pixels

	CRC32_t m_uiInputHash;	// Sources hash

	TextureSettingsEx_t m_exSettings0;
	VtfProcessingOptions m_vtfProcOptions;
	CUtlVector<char *> m_pVolumeTextureFileNames;
	
	enum
	{
		// CRC of input files:
		//  txt + tga/pfm
		// or
		//  psd
		VTF_INPUTSRC_CRC = MK_VTF_RSRC_ID( 'C','R','C' )
	};

	char m_SrcName[MAX_PATH];
	
	VTexConfigInfo_t( void )
	{
		memset( &m_exSettings0, 0, sizeof( m_exSettings0 ) );

		memset( &m_vtfProcOptions, 0, sizeof( m_vtfProcOptions ) );
		m_vtfProcOptions.cbSize = sizeof( m_vtfProcOptions );
		
		m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_FILTER_NICE;

		CRC32_Init( &m_uiInputHash );
	}

	bool IsSettings0Valid( void ) const
	{
		TextureSettingsEx_t exSettingsEmpty;
		memset( &exSettingsEmpty, 0, sizeof( exSettingsEmpty ) );
		Assert( sizeof( m_exSettings0 ) == sizeof( exSettingsEmpty ) );
		return !!memcmp( &m_exSettings0, &exSettingsEmpty, sizeof( m_exSettings0 ) );
	}

	// returns false if unrecognized option
	void ParseOptionKey( const char *pKeyName,  const char *pKeyValue );

	void ParseVolumeOption( const char *pKeyValue );

};
 
template < typename T >
static inline T& SetFlagValueT( T &field, T const &flag, int bSetFlag )
{
	if ( bSetFlag )
		field |= flag;
	else
		field &=~flag;

	return field;
}

static inline uint32& SetFlagValue( uint32 &field, uint32 const &flag, int bSetFlag )
{
	return SetFlagValueT<uint32>( field, flag, bSetFlag );
}

void VTexConfigInfo_t::ParseVolumeOption( const char *pKeyValue )
{
	pKeyValue += strspn( pKeyValue, " \t" );
	if ( strchr( pKeyValue, ',' ) == 0 )					// its just a single wor,d not a list of filenames
	{
		m_nVolumeTextureDepth = atoi( pKeyValue );
	}
	else
	{
		V_SplitString( pKeyValue, ",", m_pVolumeTextureFileNames );
		m_nVolumeTextureDepth = m_pVolumeTextureFileNames.Count();
		printf("depth=%d\n", m_pVolumeTextureFileNames.Count() );
	}
	
	
	// FIXME: Volume textures don't currently support NICE filtering
	m_vtfProcOptions.flags0 &= ~VtfProcessingOptions::OPT_FILTER_NICE;
	
	// Volume textures not supported for manual mip painting
	m_bManualMip = false;
}

void VTexConfigInfo_t::ParseOptionKey( const char *pKeyName,  const char *pKeyValue )
{
	int iValue = atoi( pKeyValue ); // To properly have "clamps 0" and not enable the clamping

	if ( !stricmp( pKeyName, "skybox" ) )
	{
		// We're going to treat it like a cubemap until the very end (we have to load and process all cubemap
		// faces at once, so we can match their edges with the texture compression and mipmapping).
		m_bIsSkyBox = iValue ? true : false;
		m_bIsCubeMap = iValue ? true : false;
		if ( !g_Quiet && iValue )
			Msg( "'skybox' detected. Treating skybox like a cubemap for edge-matching purposes.\n" );
	}
	else if ( !stricmp( pKeyName, "skyboxcropped" ) )
	{
		m_bIsCroppedSkyBox = iValue ? true : false;
		if ( !g_Quiet && iValue )
			Msg( "'skyboxcropped' detected. Will output half-height front/back/left/right images and a 4x4 'down' image.\n" );
	}
	else if ( !stricmp( pKeyName, "cubemap" ) )
	{
		m_bIsCubeMap = iValue ? true : false;
		if ( !g_Quiet && iValue )
			Msg( "'cubemap' detected.\n" );
	}
	else if( !stricmp( pKeyName, "startframe" ) )
	{
		m_nStartFrame = atoi( pKeyValue );
	}
	else if( !stricmp( pKeyName, "endframe" ) )
	{
		m_nEndFrame = atoi( pKeyValue );
	}
	else if( !stricmp( pKeyName, "volumetexture" ) )
	{
		ParseVolumeOption( pKeyValue );
	}
	else if( !stricmp( pKeyName, "bumpscale" ) )
	{
		m_flBumpScale = atof( pKeyValue );
	}
	else if( !stricmp( pKeyName, "pointsample" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_POINTSAMPLE, iValue );
	}
	else if( !stricmp( pKeyName, "trilinear" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_TRILINEAR, iValue );
	}
	else if( !stricmp( pKeyName, "clamps" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_CLAMPS, iValue );
	}
	else if( !stricmp( pKeyName, "clampt" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_CLAMPT, iValue );
	}
	else if( !stricmp( pKeyName, "clampu" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_CLAMPU, iValue );
	}
	else if( !stricmp( pKeyName, "border" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_BORDER, iValue );
		// Gets applied to s, t and u   We currently assume black border color
	}
	else if( !stricmp( pKeyName, "anisotropic" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_ANISOTROPIC, iValue );
	}
	else if( !stricmp( pKeyName, "dxt5" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_HINT_DXT5, iValue );
	}
	else if( !stricmp( pKeyName, "nocompress" ) )
	{
		SetFlagValue( m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_NOCOMPRESS, iValue );
	}
	else if( !stricmp( pKeyName, "normal" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_NORMAL, iValue );

		// Normal maps not supported for manual mip painting
		m_bManualMip = false;
	}
	else if( !stricmp( pKeyName, "normalga" ) )
	{
		m_bNormalToDXT5GA = iValue ? true : false;
		SetFlagValue( m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_NORMAL_GA, iValue );
	}
	else if( !stricmp( pKeyName, "invertgreen" ) )
	{
		m_bNormalInvertGreen = iValue ? true : false;
		if ( !g_Quiet && iValue )
			Msg( "'invertgreen' detected, assuming this is a normal map authored in Zbrush, Modo, Crazybump, etc.\n" );
	}
	else if( !stricmp( pKeyName, "ssbump" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_SSBUMP, iValue );
	}
	else if( !stricmp( pKeyName, "nomip" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_NOMIP, iValue );
		m_bManualMip = false;
	}
	else if( !stricmp( pKeyName, "allmips" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_ALL_MIPS, iValue );
	}
	else if( !stricmp( pKeyName, "mostmips" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_MOST_MIPS, iValue );
	}
	else if( !stricmp( pKeyName, "nonice" ) )
	{
		SetFlagValue( m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_FILTER_NICE, !iValue );
	}
	else if( !stricmp( pKeyName, "nolod" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_NOLOD, iValue );
	}
	else if( !stricmp( pKeyName, "procedural" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_PROCEDURAL, iValue );
	}
	else if( !stricmp( pKeyName, "alphatest" ) )
	{
		SetFlagValue( m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_MIP_ALPHATEST, iValue );
	}
	else if( !stricmp( pKeyName, "alphatest_threshhold" ) )
	{
		m_flAlphaThreshhold = atof( pKeyValue );
	}
	else if( !stricmp( pKeyName, "alphatest_hifreq_threshhold" ) )
	{
		m_flAlphaHiFreqThreshhold = atof( pKeyValue );
	}
	else if( !stricmp( pKeyName, "rendertarget" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_RENDERTARGET, iValue );
	}
	else if ( !stricmp( pKeyName, "numchannels" ) )
	{
		m_numChannelsMax = atoi( pKeyValue );
	}
	else if ( !stricmp( pKeyName, "nodebug" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_NODEBUGOVERRIDE, iValue );
	}
	else if ( !stricmp( pKeyName, "singlecopy" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_SINGLECOPY, iValue );
	}
	else if( !stricmp( pKeyName, "oneovermiplevelinalpha" ) )
	{
		SetFlagValue( m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_SET_ALPHA_ONEOVERMIP, iValue );
	}
	else if( !stricmp( pKeyName, "premultalphabymiplevelfraction" ) )
	{
		SetFlagValue( m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_PREMULT_ALPHA_MIPFRACTION, iValue );
	}
	else if( !stricmp( pKeyName, "premultalphabymiplevelfraction_maxalphamiplevel" ) )
	{
		m_vtfProcOptions.fullAlphaAtMipLevel = atoi( pKeyValue );	
	}
	else if( !stricmp( pKeyName, "premultalphabymiplevelfraction_minalphaperpixel" ) )
	{
		m_vtfProcOptions.minAlpha = atoi( pKeyValue );
	}
	else if( !stricmp( pKeyName, "premultcolorbyoneovermiplevel" ) )
	{
		SetFlagValue( m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_PREMULT_COLOR_ONEOVERMIP, iValue );
	}
	else if ( !stricmp( pKeyName, "normaltodudv" ) )
	{
		m_bNormalToDuDv = iValue ? true : false;
		SetFlagValue( m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_NORMAL_DUDV, iValue );
	}
	else if ( !stricmp( pKeyName, "compute2dgradient" ) )
	{
		SetFlagValue( m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_COMPUTE_GRADIENT, iValue ? true : false );
	}
	else if ( !stricmp( pKeyName, "stripalphachannel" ) )
	{
		m_bStripAlphaChannel = iValue ? true : false;
	}
	else if ( !stricmp( pKeyName, "stripcolorchannel" ) )
	{
		m_bStripColorChannel = iValue ? true : false;
	}
	else if ( !stricmp( pKeyName, "normalalphatodudvluminance" ) )
	{
		m_bAlphaToLuminance = iValue ? true : false;
	}
	else if ( !stricmp( pKeyName, "dudv" ) )
	{
		m_bDuDv = iValue ? true : false;
	}
	else if( !stricmp( pKeyName, "reduce" ) )
	{
		m_nReduceX = atoi(pKeyValue);
		m_nReduceY = m_nReduceX;
	}
	else if( !stricmp( pKeyName, "reducex" ) )
	{
		m_nReduceX = atoi(pKeyValue);
	}
	else if( !stricmp( pKeyName, "reducey" ) )
	{
		m_nReduceY = atoi(pKeyValue);
	}
	else if( !stricmp( pKeyName, "maxwidth" ) )
	{
		m_nMaxDimensionX = atoi(pKeyValue);
	}
	else if( !stricmp( pKeyName, "maxwidth_360" ) )
	{
		m_nMaxDimensionX_360 = atoi(pKeyValue);
	}
	else if( !stricmp( pKeyName, "maxheight" ) )
	{
		m_nMaxDimensionY = atoi(pKeyValue);
	}
	else if( !stricmp( pKeyName, "maxheight_360" ) )
	{
		m_nMaxDimensionY_360 = atoi(pKeyValue);
	}
	else if( !stricmp( pKeyName, "alphatodistance" ) )
	{
		m_bAlphaToDistance = iValue ? true : false;
	}
	else if( !stricmp( pKeyName, "distancespread" ) )
	{
		m_flDistanceSpread = atof(pKeyValue);
	}
	else if( !stricmp( pKeyName, "pfmscale" ) )
	{
		m_pfmscale=atof(pKeyValue);
		VTexMsg( "pfmscale = %.2f\n", m_pfmscale );
	}
	else if ( !stricmp( pKeyName, "pfm" ) )
	{
		if ( iValue )
			g_eMode = BITMAP_FILE_TYPE_PFM;
	}
	else if ( !stricmp( pKeyName, "displacementwrinkle" ) )
	{
		m_bDisplacementWrinkleMap = true;
	}
	else if ( !stricmp( pKeyName, "specvar" ) )
	{
		int iDecayChannel = -1;

		if ( !stricmp( pKeyValue, "red" ) || !stricmp( pKeyValue, "r" ) )
			iDecayChannel = 0;
		if ( !stricmp( pKeyValue, "green" ) || !stricmp( pKeyValue, "g" ) )
			iDecayChannel = 1;
		if ( !stricmp( pKeyValue, "blue" ) || !stricmp( pKeyValue, "b" ) )
			iDecayChannel = 2;
		if ( !stricmp( pKeyValue, "alpha" ) || !stricmp( pKeyValue, "a" ) )
			iDecayChannel = 3;

		if ( iDecayChannel >= 0 && iDecayChannel < 4 )
		{
			m_vtfProcOptions.flags0 |= ( VtfProcessingOptions::OPT_DECAY_R | VtfProcessingOptions::OPT_DECAY_EXP_R ) << iDecayChannel;
			m_vtfProcOptions.numNotDecayMips[iDecayChannel] = 0;
			m_vtfProcOptions.clrDecayGoal[iDecayChannel] = 0;
			m_vtfProcOptions.fDecayExponentBase[iDecayChannel] = 0.75;
			m_bManualMip = false;
			SetFlagValue( m_nFlags, TEXTUREFLAGS_ALL_MIPS, 1 );
		}
	}
	else if ( stricmp( pKeyName, "manualmip" ) == 0 )
	{
		if ( ( m_nVolumeTextureDepth == 1 ) && !( m_nFlags & ( TEXTUREFLAGS_NORMAL | TEXTUREFLAGS_NOMIP ) ) )
		{
			m_bManualMip = true;
		}
	}
	else if ( !stricmp( pKeyName, "mipblend" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_ALL_MIPS, 1 );

		// Possible values
		if ( !stricmp( pKeyValue, "detail" ) ) // Skip 2 mips and fade to gray -> (128, 128, 128, -)
		{
			for( int ch = 0; ch < 3; ++ ch )
			{
				m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_DECAY_R << ch;
				// m_vtfProcOptions.flags0 &= ~(VtfProcessingOptions::OPT_DECAY_EXP_R << ch);
				m_vtfProcOptions.numNotDecayMips[ch] = 2;
				m_vtfProcOptions.clrDecayGoal[ch] = 128;
			}
		}
		/*
		else if ( !stricmp( pKeyValue, "additive" ) ) // Skip 2 mips and fade to black -> (0, 0, 0, -)
		{
			for( int ch = 0; ch < 3; ++ ch )
			{
				m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_DECAY_R << ch;
				m_vtfProcOptions.flags0 &= ~(VtfProcessingOptions::OPT_DECAY_EXP_R << ch);
				m_vtfProcOptions.numDecayMips[ch] = 2;
				m_vtfProcOptions.clrDecayGoal[ch] = 0;
			}
		}
		else if ( !stricmp( pKeyValue, "alphablended" ) ) // Skip 2 mips and fade out alpha to 0
		{
			for( int ch = 3; ch < 4; ++ ch )
			{
				m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_DECAY_R << ch;
				m_vtfProcOptions.flags0 &= ~(VtfProcessingOptions::OPT_DECAY_EXP_R << ch);
				m_vtfProcOptions.numDecayMips[ch] = 2;
				m_vtfProcOptions.clrDecayGoal[ch] = 0;
			}
		}
		*/
		else
		{
			// Parse the given value:
			// skip=3:r=255:g=255:b=255:a=255  - linear decay
			// r=0e.75 - exponential decay targeting 0 with exponent base 0.75

			int nSteps = 0; // default
			
			for ( char const *szParse = pKeyValue; szParse; szParse = strchr( szParse, ':' ), szParse ? ++ szParse : 0 )
			{
				if ( char const *sz = StringAfterPrefix( szParse, "skip=" ) )
				{
					szParse = sz;
					nSteps = atoi(sz);
				}
				else if ( StringHasPrefix( szParse, "r=" ) ||
					      StringHasPrefix( szParse, "g=" ) ||
						  StringHasPrefix( szParse, "b=" ) ||
						  StringHasPrefix( szParse, "a=" ) )
				{
					int ch = 0;
					switch ( *szParse )
					{
					case 'g': case 'G': ch = 1; break;
					case 'b': case 'B': ch = 2; break;
					case 'a': case 'A': ch = 3; break;
					}
					
					szParse += 2;
					m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_DECAY_R << ch;
					m_vtfProcOptions.flags0 &= ~(VtfProcessingOptions::OPT_DECAY_EXP_R << ch);
					m_vtfProcOptions.numNotDecayMips[ch] = nSteps;
					m_vtfProcOptions.clrDecayGoal[ch] = atoi( szParse );
					
					while ( V_isdigit( *szParse ) )
						++ szParse;

					// Exponential decay
					if ( ( *szParse == 'e' || *szParse == 'E' ) && ( szParse[1] == '.' ) )
					{
						m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_DECAY_EXP_R << ch;
						m_vtfProcOptions.fDecayExponentBase[ch] = ( float ) atof( szParse + 1 );
					}
				}
				else
				{
					VTexWarning( "invalid mipblend setting \"%s\"\n", pKeyValue );
				}
			}
		}
	}
	else if( !stricmp( pKeyName, "srgb" ) )
	{
		// Do nothing for now...this will be removed shortly
	}
	else
	{
		VTexError("unrecognized option in text file - %s\n", pKeyName );
	}
}


//-----------------------------------------------------------------------------
// Returns the extension
//-----------------------------------------------------------------------------
static const char *GetSourceExtension( void )
{
	switch ( g_eMode )
	{
		case BITMAP_FILE_TYPE_PSD:
			return ".psd";
		case BITMAP_FILE_TYPE_TGA:
			return ".tga";
		case BITMAP_FILE_TYPE_PFM:
			return ".pfm";
		default:
			return ".tga";
	}
}


//-----------------------------------------------------------------------------
// Computes the desired texture format based on flags
//-----------------------------------------------------------------------------
static ImageFormat ComputeDesiredImageFormat( IVTFTexture *pTexture, VTexConfigInfo_t &info )
{
	bool bDUDVTarget = info.m_bNormalToDuDv || info.m_bDuDv;
	bool bCopyAlphaToLuminance = info.m_bNormalToDuDv && info.m_bAlphaToLuminance;

	ImageFormat targetFormat;

	int nFlags = pTexture->Flags();
	if ( info.m_bStripAlphaChannel )
	{
		nFlags &= ~( TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA );
	}

	if ( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_NORMAL_GA )
	{
		return IMAGE_FORMAT_DXT5;
	}

	if ( pTexture->Format() == IMAGE_FORMAT_RGB323232F )
	{
#ifndef DEBUG_NO_COMPRESSION
		if ( g_bUsedAsLaunchableDLL && !( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_NOCOMPRESS ) )
		{
			return IMAGE_FORMAT_BGRA8888;
		}
		else
#endif // #ifndef DEBUG_NO_COMPRESSION
		{
			return IMAGE_FORMAT_RGBA16161616F;
		}
	}
	// Typically used for uncompressed/unquantized displacement maps
	if ( ( pTexture->Format() == IMAGE_FORMAT_R32F ) || ( pTexture->Format() == IMAGE_FORMAT_RGBA32323232F ) )
	{
		return pTexture->Format();
	}

	if ( bDUDVTarget )
	{
		if ( bCopyAlphaToLuminance && ( nFlags & ( TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA ) ) )
		{
			return IMAGE_FORMAT_UVLX8888;
		}
		return IMAGE_FORMAT_UV88;
	}

	if ( info.m_bStripColorChannel )
	{
		return IMAGE_FORMAT_A8;
	}

	// can't compress textures that are smaller than 4x4
	if( (nFlags & TEXTUREFLAGS_PROCEDURAL) ||
		(info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_NOCOMPRESS) ||
		(pTexture->Width() < 4) || (pTexture->Height() < 4) )
	{
		if ( nFlags & ( TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA ) )
		{
			targetFormat = IMAGE_FORMAT_BGRA8888;
		}
		else
		{
			targetFormat = IMAGE_FORMAT_BGR888;
		}
	}
	else if( nFlags & TEXTUREFLAGS_HINT_DXT5 )
	{
#ifdef DEBUG_NO_COMPRESSION
		targetFormat = IMAGE_FORMAT_BGRA8888;
#else
		targetFormat = IsPosix() ? IMAGE_FORMAT_BGRA8888 : IMAGE_FORMAT_DXT5; // No DXT compressor on Posix
#endif
	}
	else if( nFlags & TEXTUREFLAGS_EIGHTBITALPHA )
	{
		// compressed with alpha blending
#ifdef DEBUG_NO_COMPRESSION
		targetFormat = IMAGE_FORMAT_BGRA8888;
#else
		targetFormat = IsPosix() ? IMAGE_FORMAT_BGRA8888 : IMAGE_FORMAT_DXT5; // No DXT compressor on Posix
#endif
	}
	else if ( nFlags & TEXTUREFLAGS_ONEBITALPHA )
	{
		// garymcthack - fixme IMAGE_FORMAT_DXT1_ONEBITALPHA doesn't work yet.
#ifdef DEBUG_NO_COMPRESSION
		targetFormat = IMAGE_FORMAT_BGRA8888;
#else
		//		targetFormat = IMAGE_FORMAT_DXT1_ONEBITALPHA;
		targetFormat = IsPosix() ? IMAGE_FORMAT_BGRA8888 : IMAGE_FORMAT_DXT5; // No DXT compressor on Posix
#endif
	}
	else
	{
#ifdef DEBUG_NO_COMPRESSION
		targetFormat = IMAGE_FORMAT_BGR888;
#else
		targetFormat = IsPosix() ? IMAGE_FORMAT_BGR888 : IMAGE_FORMAT_DXT1; // No DXT compressor on Posix
#endif
	}
	return targetFormat;
} 


//-----------------------------------------------------------------------------
// Computes the desired texture format based on flags
//-----------------------------------------------------------------------------
static ImageFormat ComputeDesiredImageFormat( CDmePrecompiledTexture *pPrecompiledTexture, CDmeTexture *pTexture )
{
	// FIXME: Implement!
//	bool bDUDVTarget = info.m_bNormalToDuDv || info.m_bDuDv;
//	bool bCopyAlphaToLuminance = info.m_bNormalToDuDv && info.m_bAlphaToLuminance;

	ImageFormat targetFormat;

	bool bHasAlphaChannel = ImageLoader::IsTransparent( pTexture->Format() );

//	int nFlags = pTexture->Flags();
//	if ( info.m_bStripAlphaChannel )
//	{
//		nFlags &= ~( TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA );
//	}

//	if ( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_NORMAL_GA )
//	{
//		return IMAGE_FORMAT_DXT5;
//	}

	if ( pTexture->Format() == IMAGE_FORMAT_RGB323232F )
	{
#ifndef DEBUG_NO_COMPRESSION
		if ( g_bUsedAsLaunchableDLL && !pPrecompiledTexture->m_bNoCompression )
			return IMAGE_FORMAT_BGRA8888;
#endif // #ifndef DEBUG_NO_COMPRESSION
		return IMAGE_FORMAT_RGBA16161616F;
	}
	// Typically used for uncompressed/unquantized displacement maps
	if ( ( pTexture->Format() == IMAGE_FORMAT_R32F ) || ( pTexture->Format() == IMAGE_FORMAT_RGBA32323232F ) )
	{
		return pTexture->Format();
	}

//	if ( bDUDVTarget )
//	{
//		if ( bCopyAlphaToLuminance && ( nFlags & ( TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA ) ) )
//		{
//			return IMAGE_FORMAT_UVLX8888;
//		}
//		return IMAGE_FORMAT_UV88;
//	}

//	if ( info.m_bStripColorChannel )
//	{
//		return IMAGE_FORMAT_A8;
//	}

	// Leave UV formats uncompressed
	if ( pTexture->Format() == IMAGE_FORMAT_UV88 || pTexture->Format() == IMAGE_FORMAT_UVLX8888 || pTexture->Format() == IMAGE_FORMAT_UVWQ8888 )
		return pTexture->Format();

	// can't compress textures that are smaller than 4x4
	if( pPrecompiledTexture->m_bNoCompression || ( pTexture->Width() < 4 ) || ( pTexture->Height() < 4 ) )
	{
		if ( bHasAlphaChannel )
		{
			targetFormat = IMAGE_FORMAT_BGRA8888;
		}
		else
		{
			targetFormat = IMAGE_FORMAT_BGRX8888;
		}
	}
	else if( pPrecompiledTexture->m_bHintDxt5Compression )
	{
#ifdef DEBUG_NO_COMPRESSION
		targetFormat = IMAGE_FORMAT_BGRA8888;
#else
		targetFormat = IMAGE_FORMAT_DXT5;
#endif
	}
	else if( bHasAlphaChannel )
	{
		// compressed with alpha blending
#ifdef DEBUG_NO_COMPRESSION
		targetFormat = IMAGE_FORMAT_BGRA8888;
#else
		targetFormat = IsPosix() ? IMAGE_FORMAT_BGRA8888 : IMAGE_FORMAT_DXT5; // No DXT compressor on Posix
#endif
	}
	else
	{
#ifdef DEBUG_NO_COMPRESSION
		targetFormat = IMAGE_FORMAT_BGRX8888;
#else
		targetFormat = IsPosix() ? IMAGE_FORMAT_BGR888 : IMAGE_FORMAT_DXT1; // No DXT compressor on Posix
#endif
	}
	return targetFormat;
} 


//-----------------------------------------------------------------------------
// Computes the low res image size
//-----------------------------------------------------------------------------
void VTFGetLowResImageInfo( int cacheWidth, int cacheHeight, int *lowResImageWidth, int *lowResImageHeight,
						   ImageFormat *imageFormat )
{
	if (cacheWidth > cacheHeight)
	{
		int factor = cacheWidth / LOWRESIMAGE_DIM;
		if (factor > 0)
		{
			*lowResImageWidth = LOWRESIMAGE_DIM;
			*lowResImageHeight = cacheHeight / factor;
		}
		else
		{
			*lowResImageWidth = cacheWidth;
			*lowResImageHeight = cacheHeight;
		}
	}
	else
	{
		int factor = cacheHeight / LOWRESIMAGE_DIM;
		if (factor > 0)
		{
			*lowResImageHeight = LOWRESIMAGE_DIM;
			*lowResImageWidth = cacheWidth / factor;
		}
		else
		{
			*lowResImageWidth = cacheWidth;
			*lowResImageHeight = cacheHeight;
		}
	}

	// Can end up with a dimension of zero for high aspect ration images.
	if( *lowResImageWidth < 1 )
	{
		*lowResImageWidth = 1;
	}
	if( *lowResImageHeight < 1 )
	{
		*lowResImageHeight = 1;
	}
	*imageFormat = LOWRES_IMAGE_FORMAT;
}


//-----------------------------------------------------------------------------
// This method creates the low-res image and hooks it into the VTF Texture
//-----------------------------------------------------------------------------
static void CreateLowResImage( IVTFTexture *pVTFTexture )
{
	int iWidth, iHeight;
	ImageFormat imageFormat;
	VTFGetLowResImageInfo( pVTFTexture->Width(), pVTFTexture->Height(), &iWidth, &iHeight, &imageFormat );

	// Allocate the low-res image data
	pVTFTexture->InitLowResImage( iWidth, iHeight, imageFormat );

	// Generate the low-res image bits
	if (!pVTFTexture->ConstructLowResImage())
	{
		VTexError( "Can't convert image from %s to %s in CalcLowResImage\n",
			ImageLoader::GetName( pVTFTexture->Format() ), ImageLoader::GetName(imageFormat) );
	}
}


//-----------------------------------------------------------------------------
// Computes the source file name
//-----------------------------------------------------------------------------
void MakeSrcFileName( VTexConfigInfo_t &info, const char *pFullNameWithoutExtension, int frameID, int faceID, int mipLevel, int z )
{
	bool bAnimated = !( info.m_nStartFrame == -1 || info.m_nEndFrame == -1 );

	char normalTempBuf[512];
	if( info.m_bNormalToDuDv )
	{
		char *pNormalString = Q_stristr( ( char * )pFullNameWithoutExtension, "_dudv" );
		if( pNormalString )
		{
			Q_strncpy( normalTempBuf, pFullNameWithoutExtension, sizeof( normalTempBuf ) );
			char *pNormalString = Q_stristr( normalTempBuf, "_dudv" );
			Q_strcpy( pNormalString, "_normal" );
			pFullNameWithoutExtension = normalTempBuf;
		}
		else
		{
			Assert( Q_stristr( ( char * )pFullNameWithoutExtension, "_dudv" ) );
		}
	}

	char mipTempBuf[512];
	if ( mipLevel > 0 )
	{
		Q_strncpy( mipTempBuf, pFullNameWithoutExtension, sizeof( mipTempBuf ) );

		char right[16];
		V_StrRight( mipTempBuf, 5, right, sizeof( right ) );
		if ( !V_strstr( right, "_mip0" ) )
		{
			VTexError( "Invalid texture name (%s%s) for 'manualmip' - the top mip file should end in '_mip0'\n", pFullNameWithoutExtension, GetSourceExtension() );
		}

		mipTempBuf[ strlen( mipTempBuf ) - 1 ] = 0;
		sprintf( right, "%d", mipLevel );
		V_strncat( mipTempBuf, right, sizeof( mipTempBuf ) );
		pFullNameWithoutExtension = mipTempBuf;
	}

	if( bAnimated )
	{
		if ( info.m_bIsCubeMap && g_bOldCubemapPath )
		{
			Assert( z == -1 );
			Q_snprintf( info.m_SrcName, ARRAYSIZE(info.m_SrcName), "%s%s%03d%s", pFullNameWithoutExtension, g_CubemapFacingNames[faceID], frameID + info.m_nStartFrame, GetSourceExtension() );
		}
		else
		{
			if ( z == -1 )
			{
				Q_snprintf( info.m_SrcName, ARRAYSIZE(info.m_SrcName), "%s%03d%s", pFullNameWithoutExtension, frameID + info.m_nStartFrame, GetSourceExtension() );
			}
			else
			{
				if ( info.m_pVolumeTextureFileNames.Count() == info.m_nVolumeTextureDepth )
				{
					Q_snprintf( info.m_SrcName, ARRAYSIZE(info.m_SrcName), "%s%03d%s", pFullNameWithoutExtension, frameID + info.m_nStartFrame, GetSourceExtension() );
				}
				else
				{
					Q_snprintf( info.m_SrcName, ARRAYSIZE(info.m_SrcName), "%s%03d_z%03d%s", pFullNameWithoutExtension, z, frameID + info.m_nStartFrame, GetSourceExtension() );
				}
			}
		}
	}
	else
	{
		if ( info.m_bIsCubeMap && g_bOldCubemapPath )
		{
			Assert( z == -1 );
			Q_snprintf( info.m_SrcName, ARRAYSIZE(info.m_SrcName), "%s%s%s", pFullNameWithoutExtension, g_CubemapFacingNames[faceID], GetSourceExtension() );
		}
		else
		{
			if ( z == -1 )
			{
				Q_snprintf( info.m_SrcName, ARRAYSIZE(info.m_SrcName), "%s%s", pFullNameWithoutExtension, GetSourceExtension() );
			}
			else
			{
				if ( info.m_pVolumeTextureFileNames.Count() == info.m_nVolumeTextureDepth )
				{
					Q_snprintf( info.m_SrcName, ARRAYSIZE(info.m_SrcName), info.m_pVolumeTextureFileNames[z] );
				}
				else
				{
					Q_snprintf( info.m_SrcName, ARRAYSIZE(info.m_SrcName), "%s_z%03d%s", pFullNameWithoutExtension, z, GetSourceExtension() );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Computes the source file name
//-----------------------------------------------------------------------------
void MakeSrcFileName( CDmePrecompiledTexture *pTexture, const char *pSrcName, 
	int nFrameID, int nFaceID, int nMipLevel, int z, char *pFullPath, size_t nBufLen )
{
	const char *pExt = Q_GetFileExtension( pSrcName );
	char pFullNameWithoutExtension[MAX_PATH];
	Q_StripExtension( pSrcName, pFullNameWithoutExtension, sizeof(pFullNameWithoutExtension) );

	bool bAnimated = !( pTexture->m_nStartFrame == -1 || pTexture->m_nEndFrame == -1 );

	/*
	char normalTempBuf[512];
	if( info.m_bNormalToDuDv )
	{
		char *pNormalString = Q_stristr( ( char * )pFullNameWithoutExtension, "_dudv" );
		if( pNormalString )
		{
			Q_strncpy( normalTempBuf, pFullNameWithoutExtension, sizeof( normalTempBuf ) );
			char *pNormalString = Q_stristr( normalTempBuf, "_dudv" );
			Q_strcpy( pNormalString, "_normal" );
			pFullNameWithoutExtension = normalTempBuf;
		}
		else
		{
			Assert( Q_stristr( ( char * )pFullNameWithoutExtension, "_dudv" ) );
		}
	}
	*/

	if ( nMipLevel > 0 )
	{
		// Replace the mip digit with the actual mip level
		char pRight[16];
		pFullNameWithoutExtension[ Q_strlen( pFullNameWithoutExtension ) - 1 ] = 0;
		Q_snprintf( pRight, sizeof(pRight), "%d", nMipLevel );
		Q_strncat( pFullNameWithoutExtension, pRight, sizeof( pFullNameWithoutExtension ) );
	}

	if( bAnimated )
	{
		if ( z == -1 )
		{
			Q_snprintf( pFullPath, nBufLen, "%s%03d.%s", pFullNameWithoutExtension, nFrameID + pTexture->m_nStartFrame, pExt );
		}
		else
		{
			Q_snprintf( pFullPath, nBufLen, "%s%03d_z%03d.%s", pFullNameWithoutExtension, z, nFrameID + pTexture->m_nStartFrame, pExt );
		}
	}
	else
	{
		if ( z == -1 )
		{
			Q_snprintf( pFullPath, nBufLen, "%s.%s", pFullNameWithoutExtension, pExt );
		}
		else
		{
			Q_snprintf( pFullPath, nBufLen, "%s_z%03d.%s", pFullNameWithoutExtension, z, pExt );
		}
	}
}


//-----------------------------------------------------------------------------
// Computes the output file name
//-----------------------------------------------------------------------------
void MakeOutputFileName( char *pDstFileName, int maxLen, const char *pOutputDir, const char *pBaseName, const char *pSuffix,
						const VTexConfigInfo_t &info )
{
	Q_snprintf( pDstFileName, maxLen, "%s/%s%s%s.vtf",
			pOutputDir, pBaseName, pSuffix,
			( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_SRGB_PC_TO_360 ) ? ".pwl" : "" );
}

//-----------------------------------------------------------------------------
// Loads a file into a UTLBuffer,
// also computes the hash of the buffer.
//-----------------------------------------------------------------------------
static bool LoadFile( const char *pFileName, CUtlBuffer &buf, bool bFailOnError, CRC32_t *puiHash )
{
	FILE *fp = fopen( pFileName, "rb" );
	if (!fp)
	{
		if ( bFailOnError )
			VTexError( "Can't open: \"%s\"\n", pFileName );

		return false;
	}

	fseek( fp, 0, SEEK_END );
	int nFileLength = ftell( fp );
	fseek( fp, 0, SEEK_SET );

	buf.EnsureCapacity( nFileLength );
	int nBytesRead = fread( buf.Base(), 1, nFileLength, fp );
	fclose( fp );

	buf.SeekPut( CUtlBuffer::SEEK_HEAD, nBytesRead );

	{ CP4AutoAddFile autop4( pFileName ); /* add loaded file to P4 */ }

	// Auto-compute buffer hash if necessary
	if ( puiHash )
		CRC32_ProcessBuffer( puiHash, buf.Base(), nBytesRead );

	return true;
}


//-----------------------------------------------------------------------------
// Extract basic info from an image file
//-----------------------------------------------------------------------------
bool ImageGetInfo( BitmapFileType_t nMode, CUtlBuffer &fileBuffer, int &nWidth, int &nHeight, ImageFormat &imageFormat )
{
	float flSrcGamma;
	switch ( nMode )
	{
		case BITMAP_FILE_TYPE_PSD:
			return PSDGetInfo( fileBuffer, &nWidth, &nHeight, &imageFormat, &flSrcGamma );
		case BITMAP_FILE_TYPE_TGA:
			return TGALoader::GetInfo( fileBuffer, &nWidth, &nHeight, &imageFormat, &flSrcGamma );
		case BITMAP_FILE_TYPE_PFM:
			return PFMGetInfo_AndAdvanceToTextureBits( fileBuffer, nWidth, nHeight, imageFormat );
		default:
			return false;
	}
}

//-----------------------------------------------------------------------------
// For cubemaps, the source image contains all 6 faces, embedded in a 4x3 grid
//-----------------------------------------------------------------------------
void AdjustResForCubemap( const VTexConfigInfo_t &info, int &nWidth, int &nHeight )
{
	if ( info.m_bIsCubeMap && !g_bOldCubemapPath )
	{
		if ( ( nWidth % 4 ) || ( nHeight % 3 ) )
		{
			Error( "TGA is wrong size for cubemap - [%d,%d] after 'reduce' - should be 4x3 grid of squares\n", nWidth, nHeight );
		}
		nWidth  /= 4;
		nHeight /= 3;
	}
}

//-----------------------------------------------------------------------------
// Creates a texture the size of the image stored in the buffer
//-----------------------------------------------------------------------------
static void InitializeSrcTexture( IVTFTexture *pTexture, const char *pInputFileName,
								  CUtlBuffer &fileBuffer, int nDepth, int nFrameCount,
								  const VTexConfigInfo_t &info )
{
	int nWidth, nHeight;
	ImageFormat sourceFormat;
	if ( !ImageGetInfo( g_eMode, fileBuffer, nWidth, nHeight, sourceFormat ) )
	{
		Error( "Cannot read texture %s\n", pInputFileName );
	}

	nWidth  /= info.m_nReduceX;
	nHeight /= info.m_nReduceY;
	AdjustResForCubemap( info, nWidth, nHeight );

	// Wrinkle displacement maps hold three channels of data
	ImageFormat dMapFormat = info.m_bDisplacementWrinkleMap ? IMAGE_FORMAT_RGBA32323232F : IMAGE_FORMAT_R32F;

	ImageFormat textureFormat = ( g_eMode == BITMAP_FILE_TYPE_PFM ) ? info.m_bDisplacementMap ? dMapFormat : IMAGE_FORMAT_RGB323232F : IMAGE_FORMAT_RGBA8888;
	if ( !pTexture->Init( nWidth, nHeight, nDepth, textureFormat, info.m_nFlags, nFrameCount ) )
	{
		Error( "Cannot initialize texture %s\n", pInputFileName );
	}
}

#define DISTANCE_CODE_ALPHA_INOUT_THRESHOLD 10

//-----------------------------------------------------------------------------
// Converts an 8888 image's alpha channel to encode distance-to-silhouette
//-----------------------------------------------------------------------------
void ConvertAlphaToDistance( IVTFTexture *pTexture, const VTexConfigInfo_t &info, const Bitmap_t &source, unsigned char *pDest )
{
	if ( !info.m_bAlphaToDistance )
		return;

	ImageFormatInfo_t fmtInfo = ImageLoader::ImageFormatInfo( pTexture->Format() );
	if ( fmtInfo.m_nNumAlphaBits == 0 )
	{
		VTexWarning( "%s: alpha to distance asked for but no alpha channel.\n", info.m_SrcName );
	}
	else
	{
		float flMaxRad   = info.m_flDistanceSpread*2.0*MAX( info.m_nReduceX, info.m_nReduceY );
		int   nSearchRad = ceil(flMaxRad);
		bool  bWarnEdges = false;

		for ( int x = 0; x < pTexture->Width(); x++ )
		{
			for ( int y = 0; y < pTexture->Height(); y++ )
			{
				// map to original image coords
				int nOrig_x = FLerp( 0, (source.Width() -1),	0, (pTexture->Width() -1),	x);
				int nOrig_y = FLerp( 0, (source.Height()-1),	0, (pTexture->Height()-1),	y);

				uint8 nOrigAlpha = source.GetBits()[ 3 + 4*( nOrig_x + source.Width()*nOrig_y ) ];
				bool  bInOrOut   = nOrigAlpha > DISTANCE_CODE_ALPHA_INOUT_THRESHOLD;

				float flClosest_Dist = 1.0e23f;
				for ( int iy = -nSearchRad; iy <= nSearchRad; iy++ )
				{
					for ( int ix = -nSearchRad; ix <= nSearchRad; ix++ )
					{
						int cx = MAX( 0, MIN( (source.Width()-1), (ix + nOrig_x) ) );
						int cy = MAX( 0, MIN( (source.Height()-1), (iy + nOrig_y) ) );

						int   nOffset    = 3+ 4 * ( cx + cy * source.Width() );
						uint8 alphaValue = source.GetBits()[nOffset];
						bool  bIn        =( alphaValue > DISTANCE_CODE_ALPHA_INOUT_THRESHOLD );
						if ( bInOrOut != bIn )		// transition?
						{
							float flTryDist = sqrt( (float) ( ix*ix + iy*iy ) );
							flClosest_Dist  = MIN(  flClosest_Dist, flTryDist );
						}
					}
				}

				// now, map signed distance to alpha value
				float flOutDist = MIN( 0.5f, FLerp( 0.0f, 0.5f,		0.0f, flMaxRad,		flClosest_Dist ) );
				if ( ! bInOrOut )
				{
					// negative distance
					flOutDist = -flOutDist;
				}
				uint8 &nOutAlpha = pDest[ 3 + 4*( x + pTexture->Width()*y ) ];
				nOutAlpha = MIN( 255.0f, 255.0f*( 0.5f + flOutDist ) );
				if ( ( nOutAlpha != 0 ) && 
					 ( ( x == 0 ) ||
					   ( y == 0 ) ||
					   ( x == pTexture->Width() -1 ) ||
					   ( y == pTexture->Height()-1 ) ) )
				{
					bWarnEdges = true;
					nOutAlpha = 0;					// force it.
				}
			}
		}

		if ( bWarnEdges )
		{
			VTexWarning( "%s: There are non-zero distance pixels along the image edge. You may need"
				" to reduce your distance spread or reduce the image less"
				" or add a border to the image.\n",
				info.m_SrcName );
		}
	}
}

//-----------------------------------------------------------------------------
// Converts a bitmap into a subrect thereof (for cubemaps)
//-----------------------------------------------------------------------------
void ExtractFaceSubrect( Bitmap_t &srcBitmap, Bitmap_t *pDstBitmap, int nFace )
{
	if ( !( ( nFace >= 0 ) && ( nFace < 6 ) ) )
	{
		Assert( 0 );
		return;
	}

	if ( &srcBitmap == pDstBitmap )
	{
		// NOTE: This is no longer valid! Use a new destination bitmap that varies from the source.
		Assert( 0 );
		return;
	}

	int nFaceWidth  = srcBitmap.Width()  / 4;
	int nFaceHeight = srcBitmap.Height() / 3;
	Rect_t faceRects[ 6 ] = { { 3*nFaceWidth, 1*nFaceHeight,  nFaceWidth, nFaceHeight },	// CUBEMAP_FACE_RIGHT
							  { 1*nFaceWidth, 1*nFaceHeight,  nFaceWidth, nFaceHeight },	// CUBEMAP_FACE_LEFT
							  { 2*nFaceWidth, 1*nFaceHeight,  nFaceWidth, nFaceHeight },	// CUBEMAP_FACE_BACK
							  { 0*nFaceWidth, 1*nFaceHeight,  nFaceWidth, nFaceHeight },	// CUBEMAP_FACE_FRONT
							  { 3*nFaceWidth, 0*nFaceHeight,  nFaceWidth, nFaceHeight },	// CUBEMAP_FACE_UP
							  { 3*nFaceWidth, 2*nFaceHeight,  nFaceWidth, nFaceHeight } };	// CUBEMAP_FACE_DOWN
	Rect_t &srcRect = faceRects[ nFace ];

	if ( &srcBitmap != pDstBitmap )
	{
		pDstBitmap->Init( nFaceWidth, nFaceHeight, srcBitmap.Format() );
	}

	// NOTE: Copying lines from top to bottom avoids ordering issues, due to our cubemap layout:
	int nPixelSize = ImageLoader::SizeInBytes( srcBitmap.Format() );
	for ( int y = 0; y < nFaceHeight; y++ )
	{
		unsigned char *pSrc = srcBitmap.GetPixel( srcRect.x, srcRect.y + y );
		unsigned char *pDst = pDstBitmap->GetBits()+ y*nPixelSize * nFaceWidth;
		memcpy( pDst, pSrc, nPixelSize*nFaceWidth );
	}
}

//-----------------------------------------------------------------------------
// Loads a TGA file into a Bitmap_t as RGBA8888 data
//-----------------------------------------------------------------------------
bool TGAReadFileRGBA8888( CUtlBuffer &fileBuffer, Bitmap_t &bitmap, float flGamma )
{
	// Get the information from the file...
	int nWidth, nHeight;
	ImageFormat sourceFormat;
	float flSrcGamma;
	if ( !TGALoader::GetInfo( fileBuffer, &nWidth, &nHeight, &sourceFormat, &flSrcGamma ) )
		return false;

	// Init the bitmap
	bitmap.Init( nWidth, nHeight, IMAGE_FORMAT_RGBA8888 );

	// Read the texels
	bool bNoMipMaps = false;
	fileBuffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	return TGALoader::Load( bitmap.GetBits(), fileBuffer, nWidth, nHeight, IMAGE_FORMAT_RGBA8888, flGamma, bNoMipMaps );
}
									  
bool TGAReadFile( CUtlBuffer &fileBuffer, Bitmap_t &bitmap, ImageFormat fmt, float flGamma )
{					 
	// Get the information from the file...
	int nWidth, nHeight;
	ImageFormat sourceFormat;
	float flSrcGamma;
	if ( !TGALoader::GetInfo( fileBuffer, &nWidth, &nHeight, &sourceFormat, &flSrcGamma ) )
		return false;

	// Init the bitmap
	bitmap.Init( nWidth, nHeight, fmt );
													 
	// Read the texels
	fileBuffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	return TGALoader::Load( bitmap.GetBits(), fileBuffer, nWidth, nHeight, fmt, flGamma, false );
}

//-----------------------------------------------------------------------------



//-----------------------------------------------------------------------------
// Loads a face from an image file
//  - load a subrect if this is a cubemap
//  - resamples if 'reduce' is requested
//  - performs 'alphatodist' conversion if requested
//-----------------------------------------------------------------------------
static bool LoadFaceFromFile(	IVTFTexture *pTexture, CUtlBuffer &fileBuffer, int z, int nFrame, int nFace, int nMipLevel,
								float flGamma, const VTexConfigInfo_t &info )
{
	// Load the image data as one of 2 fixed formats (so we can do resampling, etc)
	Bitmap_t srcBitmap;

	// Wrinkle displacement maps hold three channels of data
	ImageFormat dMapFormat = info.m_bDisplacementWrinkleMap ? IMAGE_FORMAT_RGBA32323232F : IMAGE_FORMAT_R32F;

	ImageFormat bitmapFormat = ( g_eMode == BITMAP_FILE_TYPE_PFM ) ? info.m_bDisplacementMap ? dMapFormat : IMAGE_FORMAT_RGB323232F : IMAGE_FORMAT_RGBA8888;

	// Load the bits
	fileBuffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	bool bOK = false;
	switch ( g_eMode )
	{
		case BITMAP_FILE_TYPE_PSD:
			bOK = PSDReadFileRGBA8888( fileBuffer, srcBitmap );
			break;
		case BITMAP_FILE_TYPE_TGA:
			bOK = TGAReadFileRGBA8888( fileBuffer, srcBitmap, flGamma );
			break;
		case BITMAP_FILE_TYPE_PFM:
			if ( info.m_bDisplacementMap )
			{
				// Displacement wrinkle maps have three channels
				if ( info.m_bDisplacementWrinkleMap )
				{
					bOK = PFMReadFileRGBA32323232F( fileBuffer, srcBitmap, info.m_pfmscale );
				}
				else
				{
					bOK = PFMReadFileR32F( fileBuffer, srcBitmap, info.m_pfmscale );
				}
			}
			else
			{
				bOK = PFMReadFileRGB323232F( fileBuffer, srcBitmap, info.m_pfmscale );
			}
			break;
	}
	if ( !bOK )
		return false;

	Bitmap_t faceBitmap;
	
	// If this is a cubemap, reduce the bitmap to the subrect for the face we're interested in	
	if ( info.m_bIsCubeMap && !g_bOldCubemapPath )
	{
		ExtractFaceSubrect( srcBitmap, &faceBitmap, nFace );
	}
	else
	{
		// Reference the source bitmap below
		faceBitmap.MakeLogicalCopyOf( srcBitmap );		
	}

	// Check that the image is the right size for this mip level
	int nMipLevelWidth, nMipLevelHeight, nMipLevelDepth;
	pTexture->ComputeMipLevelDimensions( nMipLevel, &nMipLevelWidth, &nMipLevelHeight, &nMipLevelDepth );
	if ( ( faceBitmap.Width() != info.m_nReduceX*nMipLevelWidth ) || ( faceBitmap.Height() != info.m_nReduceY*nMipLevelHeight ) )
	{
		VTexError( "'manualmip' source image wrong size for mip %d! (%s)\n", nMipLevel, info.m_SrcName );
	}

	unsigned char *pDestBits = pTexture->ImageData( nFrame, nFace, nMipLevel, 0, 0, z );
	if ( ( info.m_bAlphaToDistance ) || ( info.m_nReduceX != 1 ) || ( info.m_nReduceY != 1 ) )
	{
		int texelSize = ImageLoader::SizeInBytes( faceBitmap.Format() );
		CUtlMemory<uint8> tmpDest( 0, pTexture->Width() * pTexture->Height() * texelSize );

		ImageLoader::ResampleInfo_t resInfo;
		resInfo.m_pSrc			= faceBitmap.GetBits();
		resInfo.m_pDest			= tmpDest.Base();
		resInfo.m_nSrcWidth		= faceBitmap.Width();
		resInfo.m_nSrcHeight	= faceBitmap.Height();
		resInfo.m_nDestWidth	= pTexture->Width();
		resInfo.m_nDestHeight	= pTexture->Height();
		resInfo.m_flSrcGamma	= flGamma;
		resInfo.m_flDestGamma	= flGamma;
		if (info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_FILTER_NICE )
		{
			resInfo.m_nFlags |= ImageLoader::RESAMPLE_NICE_FILTER;
		}

		// Resample
		Assert( ( bitmapFormat == IMAGE_FORMAT_RGBA8888 ) || ( bitmapFormat == IMAGE_FORMAT_RGB323232F ) );
		if ( bitmapFormat == IMAGE_FORMAT_RGBA8888 )
		{
			ResampleRGBA8888( resInfo );
		}
		else if ( bitmapFormat == IMAGE_FORMAT_RGB323232F )
		{
			ResampleRGB323232F( resInfo );
		}

		// Convert alpha to distance
		ConvertAlphaToDistance( pTexture, info, faceBitmap, tmpDest.Base() );

		// now, store in dest
		ImageLoader::ConvertImageFormat( tmpDest.Base(), bitmapFormat, pDestBits,
			pTexture->Format(), pTexture->Width(), pTexture->Height() );
	}
	else
	{
		// Just convert the format
		ImageLoader::ConvertImageFormat( faceBitmap.GetBits(), bitmapFormat, pDestBits,
										 pTexture->Format(), faceBitmap.Width(), faceBitmap.Height() );
	}

	return true;
}

static bool LoadFace( IVTFTexture *pTexture, CUtlBuffer &tgaBuffer, int z, int nFrame, int nFace, int nMipLevel,
					  float flGamma, const VTexConfigInfo_t &info )
{
	if ( !LoadFaceFromFile( pTexture, tgaBuffer, z, nFrame, nFace, nMipLevel, flGamma, info ) )
		return false;

	// Restricting number of channels by painting white into the rest
	if ( info.m_numChannelsMax < 1 || info.m_numChannelsMax > 4 )
	{
		VTexWarning( "%s: Invalid setting restricting number of channels to %d, discarded!\n", info.m_SrcName, info.m_numChannelsMax );
	}
	else if ( info.m_numChannelsMax < 4 )
	{
		if ( 4 != ImageLoader::SizeInBytes( pTexture->Format() ) )
		{
			VTexWarning( "%s: Channels restricted to %d, but cannot fill white"
				" because pixel format is %d (size in bytes %d)!"
				" Proceeding with unmodified channels.\n",
				info.m_SrcName,
				info.m_numChannelsMax, pTexture->Format(), ImageLoader::SizeInBytes( pTexture->Format() ) );
			Assert( 0 );
		}
		else
		{
			// Fill other channels with white

			unsigned char *pDestBits = pTexture->ImageData( nFrame, nFace, 0, 0, 0, z );
			int nWidth = pTexture->Width();
			int nHeight = pTexture->Height();
			
			int nPaintOff = info.m_numChannelsMax;
			int nPaintBytes = 4 - nPaintOff;
			
			pDestBits += nPaintOff;

			for( int j = 0; j < nHeight; ++ j )
			{
				for ( int k = 0; k < nWidth; ++ k, pDestBits += 4 )
				{
					memset( pDestBits, 0xFF, nPaintBytes );
				}
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Loads source image data 
//-----------------------------------------------------------------------------
static bool LoadSourceImages( IVTFTexture *pTexture, const char *pFullNameWithoutExtension,
							  VTexConfigInfo_t &info )
{
	// The input file name here is simply for error reporting
	char *pInputFileName = ( char * )stackalloc( strlen( pFullNameWithoutExtension ) + strlen( GetSourceExtension() ) + 1 );
	strcpy( pInputFileName, pFullNameWithoutExtension );
	strcat( pInputFileName, GetSourceExtension() );

	int nFrameCount;
	bool bAnimated = !( info.m_nStartFrame == -1 || info.m_nEndFrame == -1 );
	if( !bAnimated )
	{
		nFrameCount = 1;
	}
	else
	{
		nFrameCount = info.m_nEndFrame - info.m_nStartFrame + 1;
	}

	bool bIsVolumeTexture = ( info.m_nVolumeTextureDepth > 1 );

	// Iterate over all faces of all frames
	// UNDONE: optimize the below for cubemaps (so it doesn't load+crop the source image 6 times!)
	int nFaceCount = info.m_bIsCubeMap ? CUBEMAP_FACE_COUNT : 1;
	int nMipCount  = 1;
	for( int iFrame = 0; iFrame < nFrameCount; ++iFrame )
	{
		for( int iFace = 0; iFace < nFaceCount; ++iFace )
		{
			for( int iMip = 0; iMip < nMipCount; ++iMip )
			{
				for ( int z = 0; z < info.m_nVolumeTextureDepth; ++z )
				{
					// Generate the filename to load....
					MakeSrcFileName( info, pFullNameWithoutExtension, iFrame, iFace, iMip, bIsVolumeTexture ? z : -1 );
					
					// Don't fail if we run out of 'manualmip' mip levels to load (the rest can be generated).
					// FIXME: upgrade PostProcess to take 'nLoadedMipLevels' instead of 'bLoadedMipLevels', so it generates
					//        only *missing* mip levels in 'manualmip' mode (as it is, missing levels end up opaque white!)
					bool bFailOnError = ( iMip == 0 );

					// Load the image file from disk...
					CUtlBuffer fileBuffer;
					if ( !LoadFile( info.m_SrcName, fileBuffer, bFailOnError,
									( g_eMode != BITMAP_FILE_TYPE_PSD ) ? &info.m_uiInputHash : NULL ) )
					{
						// If we want to fail on error and VTexError didn't abort then
						// simply notify the caller that we failed
						if ( bFailOnError )
							return false;

						continue;
					}

					// Initialize the VTF Texture here if we haven't already....
					// Note that we have to do it here because we have to get the width + height from the file
					if ( !pTexture->ImageData() )
					{
						InitializeSrcTexture( pTexture, info.m_SrcName, fileBuffer, info.m_nVolumeTextureDepth, nFrameCount, info );
						nMipCount = info.m_bManualMip ? pTexture->MipCount() : 1;
					}

					// NOTE: if doing 'manualmip', this loads individual MIPs, otherwise we'll just load
					//       mip 0 and the rest will be generated later, by pVTFTexture::PostProcess
					if ( !LoadFace( pTexture, fileBuffer, z, iFrame, iFace, iMip, 2.2, info ) )
					{
						Error( "Cannot load texture %s\n", pInputFileName );
					}
				}
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Initializes an image 
//-----------------------------------------------------------------------------
static CDmeImageArray *InitializeImageArray( CDmePrecompiledTexture *pPrecompiledTexture, int nBitmapCount, Bitmap_t *pBitmap )
{
	int nWidth = pBitmap[0].Width();
	int nHeight = pBitmap[0].Height();
	ImageFormat fmt = pBitmap[0].Format();

	int nFaceCount = pPrecompiledTexture->m_nTextureArraySize;
	int nDepth = pPrecompiledTexture->m_nVolumeTextureDepth;

	// FIXME: Is there a better way of dealing with gamma?
	float flGamma = pPrecompiledTexture->m_bNormalMap ? 1.0f : 2.2f;

	// Deal with cubemaps, which are authored all in the same image
	Bitmap_t cubeBitmaps[CUBEMAP_FACE_COUNT];
	bool bIsCubemap = ( pPrecompiledTexture->m_nTextureType == DMETEXTURE_TYPE_CUBEMAP );
	if ( bIsCubemap )
	{
		if ( ( nWidth % 4 ) || ( nHeight % 3 ) )
		{
			VTexError( "TGA is wrong size for cubemap - [%d,%d] - should be 4x3 grid of squares!\n", nWidth, nHeight );
			return NULL;
		}
		Assert( nDepth == 1 && nBitmapCount == 1 );
		nWidth  /= 4;
		nHeight /= 3;

		for ( int i = 0; i < CUBEMAP_FACE_COUNT; ++i )
		{
			ExtractFaceSubrect( pBitmap[0], &cubeBitmaps[i], i );
		}
		nBitmapCount = nFaceCount = CUBEMAP_FACE_COUNT;
		pBitmap = cubeBitmaps;
	}

	int nBitmapIndex = 0;
	CDmeImageArray *pImageArray = CreateElement< CDmeImageArray >( "mip", pPrecompiledTexture->GetFileId() );
	for ( int i = 0; i < nFaceCount; ++i )
	{
		CDmeImage *pImage = CreateElement< CDmeImage >( "image", pImageArray->GetFileId() );
		pImage->Init( nWidth, nHeight, nDepth, fmt, flGamma );
		int nSizeToCopy = pImage->ZSliceSizeInBytes();

		CUtlBinaryBlock &buf = pImage->BeginModification();
		uint8 *pBase = (uint8*)buf.Get();
		for ( int j = 0; j < nDepth; ++j, ++nBitmapIndex )
		{
			Q_memcpy( pBase + j * nSizeToCopy, pBitmap[nBitmapIndex].GetBits(), nSizeToCopy );
		}
		pImage->EndModification();

		pImageArray->AddImage( pImage );
	}
	return pImageArray;
}


//-----------------------------------------------------------------------------
// Loads all bitmaps for a slice 
//-----------------------------------------------------------------------------
static bool LoadBitmaps( CDmePrecompiledTexture *pPrecompiledTexture, const char *pFullPath, 
	BitmapFileType_t mode, int nFrame, int nMip, Bitmap_t *pBitmaps, CRC32_t *pInputHash )
{
	bool bIsVolumeTexture = ( pPrecompiledTexture->m_nVolumeTextureDepth > 1 );
	int nFaceCount = pPrecompiledTexture->m_nTextureArraySize;

	int nBitmapIndex = 0;
	for( int iFace = 0; iFace < nFaceCount; ++iFace )
	{
		for ( int z = 0; z < pPrecompiledTexture->m_nVolumeTextureDepth; ++z )
		{
			// Generate the filename to load....
			char pSrcFile[ MAX_PATH ];
			MakeSrcFileName( pPrecompiledTexture, pFullPath, nFrame, iFace, nMip, bIsVolumeTexture ? z : -1, pSrcFile, sizeof(pSrcFile) );

			// Don't fail if we run out of 'manualmip' mip levels to load (the rest can be generated).
			// FIXME: upgrade PostProcess to take 'nLoadedMipLevels' instead of 'bLoadedMipLevels', so it generates
			//        only *missing* mip levels in 'manualmip' mode (as it is, missing levels end up opaque white!)
			bool bFailOnError = ( nMip == 0 );

			// Load the image file from disk...
			CUtlBuffer fileBuffer;
			if ( !LoadFile( pSrcFile, fileBuffer, bFailOnError,	pInputHash ) )
			{
				// If we want to fail on error and VTexError didn't abort then
				// simply notify the caller that we failed
				if ( bFailOnError )
					return false;

				continue;
			}

			// NOTE: if doing 'manualmip', this loads individual MIPs, otherwise we'll just load
			//       mip 0 and the rest will be generated later, by pVTFTexture::PostProcess
			BitmapFileType_t nLoadedMode = LoadBitmapFile( fileBuffer, &pBitmaps[nBitmapIndex] );
			if ( nLoadedMode == BITMAP_FILE_TYPE_UNKNOWN )
			{
				VTexError( "Cannot load texture %s\n", pSrcFile );
				return false;
			}

			if ( nBitmapIndex > 0 )
			{
				if ( pBitmaps[0].Width() != pBitmaps[nBitmapIndex].Width() || 
					pBitmaps[0].Height() != pBitmaps[nBitmapIndex].Height() ||
					pBitmaps[0].Format() != pBitmaps[nBitmapIndex].Format() )
				{
					VTexError( "Found inconsistent sizes or color formats in texture faces\\z slices!\n" );
					return false;
				}
			}
			++nBitmapIndex;
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
// Loads source image data 
//-----------------------------------------------------------------------------
static bool LoadSourceImages( const char *pFullDir, CDmePrecompiledTexture *pPrecompiledTexture, 
	BitmapFileType_t mode, CRC32_t *pInputHash )
{
	// The input file name here is simply for error reporting
	char pFullPath[ MAX_PATH ];
	const char *pInputFileName = pPrecompiledTexture->m_ImageFileName;
	Q_ComposeFileName( pFullDir, pInputFileName, pFullPath, sizeof(pFullPath) );

	bool bAnimated = !( pPrecompiledTexture->m_nStartFrame == -1 || pPrecompiledTexture->m_nEndFrame == -1 );
	int nFrameCount = ( !bAnimated ) ? 1 : pPrecompiledTexture->m_nEndFrame - pPrecompiledTexture->m_nStartFrame + 1;

	// Iterate over all faces of all frames
	// NOTE: Cubemaps are handled specially. Only 1 texture is loaded, but it
	// has all cube side faces in the same texture
	bool bComputedMipCount = false;
	int nMipCount  = 1;
	int nImageCount = pPrecompiledTexture->m_nVolumeTextureDepth * pPrecompiledTexture->m_nTextureArraySize;
	Bitmap_t *pBitmaps = (Bitmap_t*)stackalloc( nImageCount * sizeof(Bitmap_t) );
	memset( pBitmaps, 0, nImageCount * sizeof(Bitmap_t) );
	for( int iFrame = 0; iFrame < nFrameCount; ++iFrame )
	{
		CDmeTextureFrame *pFrame = pPrecompiledTexture->m_pSourceTexture->AddFrame();

		for( int iMip = 0; iMip < nMipCount; ++iMip )
		{
			if ( !LoadBitmaps( pPrecompiledTexture, pFullPath, mode, iFrame, iMip, pBitmaps, pInputHash ) )
				return false;

			/*
			// Check that the image is the right size for this mip level
			int nMipLevelWidth, nMipLevelHeight, nMipLevelDepth;
			pTexture->ComputeMipLevelDimensions( nMipLevel, &nMipLevelWidth, &nMipLevelHeight, &nMipLevelDepth );
			if ( ( bitmap.m_nWidth != info.m_nReduceX*nMipLevelWidth ) || ( bitmap.m_nHeight != info.m_nReduceY*nMipLevelHeight ) )
			{
			VTexError( "'manualmip' source image wrong size for mip %d! (%s)\n", nMipLevel, info.m_SrcName );
			}
			*/

			CDmeImageArray *pMipLevel = InitializeImageArray( pPrecompiledTexture, nImageCount, pBitmaps );
			pFrame->AddMipLevel( pMipLevel );

			// Note that we have to do compute mip count here
			// because we have to get the width + height from the file
			if ( !bComputedMipCount && pPrecompiledTexture->m_bLoadMipLevels )
			{
				nMipCount = ImageLoader::GetNumMipMapLevels( pMipLevel->Width(), pMipLevel->Height(), pMipLevel->Depth() );
				bComputedMipCount = true;
			}
		}
	}
	stackfree( pBitmaps );

	return true;
}


void NormalInvertGreen( IVTFTexture *pTexture )
{
	if ( pTexture->Format() != IMAGE_FORMAT_RGBA8888 )
		VTexError( "Cannot 'invert green', normal map in unexpected format\n" );
	if ( pTexture->Depth() > 1 )
		VTexError( "Cannot 'invert green', normal map is a volume texture?!\n" );
		
	for ( int iFrame = 0; iFrame < pTexture->FrameCount(); iFrame++ )
	{
		for ( int iFace = 0; iFace < pTexture->FaceCount(); iFace++ )
		{
			for ( int iMip = 0; iMip < pTexture->MipCount(); iMip++ )
			{
				int nWidth, nHeight, nDepth;
				pTexture->ComputeMipLevelDimensions( iMip, &nWidth, &nHeight, &nDepth );

				unsigned char *pPixels = pTexture->ImageData( iFrame, iFace, iMip );
				for ( int i = 0; i < nWidth*nHeight*nDepth; i++ )
				{
					pPixels[ 1 ] = 255 - pPixels[ 1 ];
					pPixels += 4;
				}
			}
		}
	}
}


void PreprocessSkyBox( char *pFullNameWithoutExtension, int *iSkyboxFace )
{
	// This is now an old codepath, possibly to be deprecated (though 'buildcubemaps' still depends on it)
	Assert( g_bOldCubemapPath );


	// When we get here, it means that we're processing one face of a skybox, but we're going to 
	// load all the faces and treat it as a cubemap so we can do the edge matching.

	// Since they passed in only one face of the skybox, there's a 2 letter extension we want to get rid of.
	int len = strlen( pFullNameWithoutExtension );
	if ( len >= 3 )
	{
		// Make sure there really is a 2 letter extension.
		char *pEnd = &pFullNameWithoutExtension[ len - 2 ];
		*iSkyboxFace = -1;
		for ( int i=0; i < ARRAYSIZE( g_CubemapFacingNames ); i++ )
		{
			if ( stricmp( pEnd, g_CubemapFacingNames[i] ) == 0 )
			{
				*iSkyboxFace = i;
				break;
			}
		}

		// Cut off the 2 letter extension.
		if ( *iSkyboxFace != -1 )
		{
			pEnd[0] = 0;
			return;
		}
	}

	Error( "PreprocessSkyBox: filename %s doesn't have a proper extension (bk, dn, rt, etc..)\n", pFullNameWithoutExtension );
}

void PostProcessSkyBox( SmartIVTFTexture &pSrcTexture, const char *pDstFileName,
						const char *pOutputDir, const char *pBaseName, VTexConfigInfo_t const &info,
						CUtlVector< OutputTexture_t > &outputTextures, int iSkyboxFace )
{
	// Split the cubemap into 6 separate images, optionally optimizing (cropping) them
	int startFace = 0;
	int numFaces  = 6;
	if ( g_bOldCubemapPath )
	{
		// NOTE: this is the old path, possibly to be deprecated

		// Right now, we've got a full cubemap, and we want to return the one face of the
		// skybox that we're supposed to be processing.
		startFace = iSkyboxFace;
		numFaces  = 1;
	}

	// Input is a cubemap, output is one flat texture per face
	int nFlags = pSrcTexture->Flags();
	Assert( info.m_bIsCubeMap && info.m_bIsSkyBox && ( nFlags & TEXTUREFLAGS_ENVMAP ) );
	nFlags &= ~TEXTUREFLAGS_ENVMAP;

	for ( int iFace = startFace; iFace < ( startFace + numFaces ); iFace++ )
	{
		int nWidth  = pSrcTexture->Width();
		int nHeight = pSrcTexture->Height();
		
		if ( info.m_bIsCroppedSkyBox && ( iFace != CUBEMAP_FACE_UP ) )
		{
			// Crop skybox output images to avoid wasting memory on unseen portions below the horizon
			if ( iFace == CUBEMAP_FACE_DOWN )
			{
				nWidth  = ( nWidth  > 4 ) ? 4 : nWidth;
				nHeight = ( nHeight > 4 ) ? 4 : nHeight;
			}
			else
			{
				nHeight = ( nHeight + 1 ) / 2;
			}
		}

		IVTFTexture *pFaceTexture = CreateVTFTexture();
		if ( !pFaceTexture->Init( nWidth, nHeight, 1, pSrcTexture->Format(), nFlags, pSrcTexture->FrameCount() ) )
			Error( "PostProcessSkyBox: IVTFTexture::Init() failed.\n" );

		// Copy across the data for this face - cropping happens because "destSize < sourceSize"
		// NOTE: This only works because:
		//         (a) we are cropping the bottom half of the image (except for
		//             CUBEMAP_FACE_DOWN, for which output colours don't actually matter)
		//         (b) we assume power-of-two input textures
		//         (c) DXT-compressed mip levels are padded to 4x4 block sizes
		//       There are a few things that don't get copied here, like alpha test
		//       threshold and bumpscale, but we shouldn't need those for skyboxes anyway.
		int nMips = pFaceTexture->MipCount();
		for ( int iMip = 0; iMip < nMips; iMip++ )
		{
			int srcMipSize = pSrcTexture->ComputeMipSize(  iMip );
			int dstMipSize = pFaceTexture->ComputeMipSize( iMip );
			if ( ( srcMipSize != dstMipSize ) &&
				 ( ( srcMipSize < dstMipSize ) || ( ( srcMipSize != 2*dstMipSize ) && ( iFace != CUBEMAP_FACE_DOWN ) ) ) )
			{
				Error( "PostProcessSkyBox: src/dest mipmap size mismatch during skybox cropping (src=%d, dest=%d)\n", srcMipSize, dstMipSize );
			}

			for ( int iFrame = 0; iFrame < pSrcTexture->FrameCount(); iFrame++ )
			{
				unsigned char       *pDst = pFaceTexture->ImageData( iFrame,     0, iMip );
				const unsigned char *pSrc =  pSrcTexture->ImageData( iFrame, iFace, iMip );
				memcpy( pDst, pSrc, dstMipSize );
			}
		}

		// Add this to the list of outputs		
		OutputTexture_t outputTexture;
		outputTexture.pTexture = pFaceTexture;
  		const char *pSuffix = g_bOldCubemapPath ? "" : g_CubemapFacingNames[ iFace ];
		MakeOutputFileName( outputTexture.dstFileName, ARRAYSIZE( outputTexture.dstFileName ), pOutputDir, pBaseName, pSuffix, info );
		outputTextures.AddToTail( outputTexture );
	}

	// Get rid of the full cubemap one and return the single-face one.
	DestroyVTFTexture( pSrcTexture.Get() );
	pSrcTexture.Assign( NULL );
}


//-----------------------------------------------------------------------------
// Does a file exist? (doesn't use search paths)
//-----------------------------------------------------------------------------
bool FileExistsAbsolute( const char *pFileName )
{
	return ( 00 == access( pFileName, 00 ) );
}


void MakeDirHier( const char *pPath )
{
#ifdef PLATFORM_POSIX
#define mkdir(s) mkdir(s, S_IRWXU | S_IRWXG | S_IRWXO )
#endif
	char temp[1024];
	Q_strncpy( temp, pPath, 1024 );
	int i;
	for( i = 0; i < strlen( temp ); i++ )
	{
		if( temp[i] == '/' || temp[i] == '\\' )
		{
			temp[i] = '\0';
			//			DebugOut( "mkdir( %s )\n", temp );
			mkdir( temp );
			temp[i] = '\\';
		}
	}
	//	DebugOut( "mkdir( %s )\n", temp );
	mkdir( temp );
}


static uint8 GetClampingValue( int nClampSize )
{
	if ( nClampSize <= 0 )
		return 30;											// ~1 billion
	int nRet = 0;
	while ( nClampSize > 1 )
	{
		nClampSize >>= 1;
		nRet++;
	}
	return nRet;
}

static void SetTextureLodData( IVTFTexture *pTexture, VTexConfigInfo_t const &info )
{
	if (
		( info.m_nMaxDimensionX > 0 && info.m_nMaxDimensionX < pTexture->Width() ) ||
		( info.m_nMaxDimensionY > 0 && info.m_nMaxDimensionY < pTexture->Height() ) ||
		( info.m_nMaxDimensionX_360 > 0 && info.m_nMaxDimensionX_360 < pTexture->Width() ) ||
		( info.m_nMaxDimensionY_360 > 0 && info.m_nMaxDimensionY_360 < pTexture->Height() )
		)
	{
		TextureLODControlSettings_t lodChunk;
		memset( &lodChunk, 0, sizeof( lodChunk ) );
		lodChunk.m_ResolutionClampX = GetClampingValue( info.m_nMaxDimensionX );
		lodChunk.m_ResolutionClampY = GetClampingValue( info.m_nMaxDimensionY );
		lodChunk.m_ResolutionClampX_360 = GetClampingValue( info.m_nMaxDimensionX_360 );
		lodChunk.m_ResolutionClampY_360 = GetClampingValue( info.m_nMaxDimensionY_360 );
		pTexture->SetResourceData( VTF_RSRC_TEXTURE_LOD_SETTINGS, &lodChunk, sizeof( lodChunk ) );
	}
}


static void AttachShtFile( const char *pFullNameWithoutExtension, IVTFTexture *pTexture, CRC32_t *puiHash )
{
	char shtName[MAX_PATH];
	Q_strncpy( shtName, pFullNameWithoutExtension, sizeof(shtName) );
	Q_SetExtension( shtName, ".sht", sizeof(shtName) );

	if ( !FileExistsAbsolute( shtName ) )
		return;

	VTexMsg( "Attaching .sht file %s.\n", shtName );

	// Ok, the file exists. Read it.
	CUtlBuffer buf;
	if ( !LoadFile( shtName, buf, false, puiHash ) )
		return;

	pTexture->SetResourceData( VTF_RSRC_SHEET, buf.Base(), buf.TellPut() );
}


//-----------------------------------------------------------------------------
// Does the dirty deed and generates a VTF file
//-----------------------------------------------------------------------------
bool ProcessFiles( const char *pFullNameWithoutExtension, const char *pOutputDir, const char *pBaseName, VTexConfigInfo_t &info )
{
	// force clamps/clampt for cube maps
	if( info.m_bIsCubeMap )
	{
		info.m_nFlags |= TEXTUREFLAGS_ENVMAP;
		info.m_nFlags |= TEXTUREFLAGS_CLAMPS;
		info.m_nFlags |= TEXTUREFLAGS_CLAMPT;
	}

	// Create the texture we're gonna store out
	SmartIVTFTexture pVTFTexture( CreateVTFTexture() );

	int iSkyboxFace = 0;
	char fullNameTemp[512];
	if ( info.m_bIsSkyBox && g_bOldCubemapPath )
	{
		Q_strncpy( fullNameTemp, pFullNameWithoutExtension, sizeof( fullNameTemp ) );
		pFullNameWithoutExtension = fullNameTemp;
		PreprocessSkyBox( fullNameTemp, &iSkyboxFace );
	}

	// Load the source images into the texture
	bool bLoadedSourceImages = LoadSourceImages( pVTFTexture.Get(), pFullNameWithoutExtension, info );
	if ( !bLoadedSourceImages )
	{
		VTexError( "Can't load source images for \"%s\"\n", pFullNameWithoutExtension );
		return false;
	}

	// Attach a sheet file if present
	AttachShtFile( pFullNameWithoutExtension, pVTFTexture.Get(), &info.m_uiInputHash );

	// No more file loads, finalize the sources hash
	CRC32_Final( &info.m_uiInputHash );
	pVTFTexture->SetResourceData( VTexConfigInfo_t::VTF_INPUTSRC_CRC, &info.m_uiInputHash, sizeof( info.m_uiInputHash ) );
	CRC32_t crcWritten = info.m_uiInputHash;

	// Name of the destination file
	char dstFileName[1024];
	const char *pSuffix = "";
	MakeOutputFileName( dstFileName, ARRAYSIZE( dstFileName ), pOutputDir, pBaseName, pSuffix, info );

	// Now if we are only validating the CRC
	if( CommandLine()->FindParm( "-crcvalidate" ) )
	{
		CUtlBuffer bufFile;
		bool bLoad = LoadFile( dstFileName, bufFile, false, NULL );
		if ( !bLoad )
		{
			VTexMsgEx( stderr, "LOAD ERROR: %s\n", dstFileName );
			return false;
		}

		SmartIVTFTexture spExistingVtf( CreateVTFTexture() );
		bLoad = spExistingVtf->Unserialize( bufFile );
		if ( !bLoad )
		{
			VTexMsgEx( stderr, "UNSERIALIZE ERROR: %s\n", dstFileName );
			return false;
		}

		size_t numDataBytes;
		void *pCrcData = spExistingVtf->GetResourceData( VTexConfigInfo_t::VTF_INPUTSRC_CRC, &numDataBytes );
		if ( !pCrcData || numDataBytes != sizeof( CRC32_t ) )
		{
			VTexMsgEx( stderr, "OLD TEXTURE FORMAT: %s\n", dstFileName );
			return false;
		}

		CRC32_t crcFile = * reinterpret_cast< CRC32_t const * >( pCrcData );
		if ( crcFile != crcWritten )
		{
			VTexMsgEx( stderr, "CRC MISMATCH: %s\n", dstFileName );
			return false;
		}

		VTexMsgEx( stderr, "OK: %s\n", dstFileName );
		return true;
	}

	// Now if we are not forcing the CRC
	if( !CommandLine()->FindParm( "-crcforce" ) )
	{
		CUtlBuffer bufFile;
		if ( LoadFile( dstFileName, bufFile, false, NULL ) )
		{
			SmartIVTFTexture spExistingVtf( CreateVTFTexture() );
			if ( spExistingVtf->Unserialize( bufFile ) )
			{
				size_t numDataBytes;
				void *pCrcData = spExistingVtf->GetResourceData( VTexConfigInfo_t::VTF_INPUTSRC_CRC, &numDataBytes );
				if ( pCrcData && numDataBytes == sizeof( CRC32_t ) )
				{
					CRC32_t crcFile = * reinterpret_cast< CRC32_t const * >( pCrcData );
					if ( crcFile == crcWritten )
					{
						if( !g_Quiet )
							VTexMsg( "SUCCESS: %s is up-to-date\n", dstFileName );

						if( !CommandLine()->FindParm( "-crcforce" ) )
							return true;
					}
				}
			}
		}
	}

	// Bumpmap scale..
	pVTFTexture->SetBumpScale( info.m_flBumpScale );

	// Alpha test threshold
	pVTFTexture->SetAlphaTestThreshholds( info.m_flAlphaThreshhold, info.m_flAlphaHiFreqThreshhold );

	// Set texture lod data
	SetTextureLodData( pVTFTexture.Get(), info );

	// Get the texture all internally consistent and happy
	bool bAllowFixCubemapOrientation = !info.m_bIsSkyBox;	// Don't let it rotate our pseudo-cubemap faces around if it's a skybox.
	pVTFTexture->SetPostProcessingSettings( &info.m_vtfProcOptions );
	pVTFTexture->PostProcess( false, LOOK_DOWN_Z, bAllowFixCubemapOrientation, info.m_bManualMip );

	// Compute the preferred image format
	ImageFormat vtfImageFormat = ComputeDesiredImageFormat( pVTFTexture.Get(), info );
	
	// Set up the low-res image
 	if ( info.m_bIsCubeMap )
	{
		// "Stage 1" of matching cubemap borders. Sometimes, it has to store off the original image.
		pVTFTexture->MatchCubeMapBorders( 1, vtfImageFormat, info.m_bIsSkyBox );
	}
	else
	{
		if ( !IsPowerOfTwo( pVTFTexture->Width() ) || !IsPowerOfTwo( pVTFTexture->Height() ) || !IsPowerOfTwo( pVTFTexture->Depth() ) )
			VTexError( "Cannot create low-res image for non-power of two texture (did you forget to set 'cubemap 1' or 'skybox 1'?)\n" );
		CreateLowResImage( pVTFTexture.Get() );
	}

	// Invert the green channel for some normal maps (depends which package they were authored in)
	if ( info.m_bNormalInvertGreen )
	{
		if ( pVTFTexture->Flags() & TEXTUREFLAGS_NORMAL )
		{
			NormalInvertGreen( pVTFTexture.Get() );
		}
		else
		{
			VTexWarning( "'invertgreen' specified for texture which is not being processed as a normal map!\n" );
		}
	}

	// DXT5 GA compressor assumes we're coming from IMAGE_FORMAT_ARGB8888 but
	// we want to swap XY (RG) for the shader decode, so we first convert to BGRA here as a trick to flop the channels
	if ( info.m_bNormalToDXT5GA )
	{
		pVTFTexture->ConvertImageFormat( IMAGE_FORMAT_BGRA8888, false, false );
	}

	// Convert to the final format
	pVTFTexture->ConvertImageFormat( vtfImageFormat, info.m_bNormalToDuDv, info.m_bNormalToDXT5GA );

	// Stage 2 of matching cubemap borders.
	pVTFTexture->MatchCubeMapBorders( 2, vtfImageFormat, info.m_bIsSkyBox );


	// Finally, write out the VTF(s)
	CUtlVector< OutputTexture_t > outputTextures;
	if ( info.m_bIsSkyBox )
	{
		// Skyboxes need splitting into multiple images (some of which may be cropped)
		PostProcessSkyBox( pVTFTexture, dstFileName, pOutputDir, pBaseName, info, outputTextures, iSkyboxFace );
	}
	else
	{
		OutputTexture_t singleOutput;
		singleOutput.pTexture = pVTFTexture.Get();
		strncpy( singleOutput.dstFileName, dstFileName, MAX_PATH );
		outputTextures.AddToTail( singleOutput );
		pVTFTexture.Assign( NULL );
	}

	for ( int i = 0; i < outputTextures.Count(); i++ )
	{
		if ( info.IsSettings0Valid() )
		{
			outputTextures[ i ].pTexture->SetResourceData( VTF_RSRC_TEXTURE_SETTINGS_EX, &info.m_exSettings0, sizeof( info.m_exSettings0 ) );
		}

		// Write it!
		if ( g_CreateDir == true )
			MakeDirHier( pOutputDir ); //It'll create it if it doesn't exist.

		// Make sure the CRC hasn't been modified since finalized
		Assert( crcWritten == info.m_uiInputHash );

		CUtlBuffer outputBuf;
		if (!outputTextures[ i ].pTexture->Serialize( outputBuf ))
		{
			VTexError( "\"%s\": Unable to serialize the VTF file!\n", dstFileName );
		}

		CP4AutoEditAddFile autop4( outputTextures[ i ].dstFileName );
		FILE *fp = fopen( outputTextures[ i ].dstFileName, "wb" );
		if( !fp )
		{
			VTexError( "Can't open: %s\n", outputTextures[ i ].dstFileName );
		}
		fwrite( outputBuf.Base(), 1, outputBuf.TellPut(), fp );
		fclose( fp );

		if ( CommandLine()->FindParm( "-source2" ) )
		{
			char pCmdLine[1024];
			Q_snprintf( pCmdLine, sizeof(pCmdLine), "resourcecompiler.exe -i %s\n", outputTextures[ i ].dstFileName );
			system( pCmdLine );
		}
	}
	
	VTexMsg( "SUCCESS: Vtf file created\n" );
	return true;
}

const char *GetPossiblyQuotedWord( const char *pInBuf, char *pOutbuf )
{
	pInBuf += strspn( pInBuf, " \t" );						// skip whitespace

	const char *pWordEnd;
	bool bQuote = false;
	if (pInBuf[0]=='"')
	{
		pInBuf++;
		pWordEnd=strchr(pInBuf,'"');
		bQuote = true;
	}
	else
	{
		pWordEnd=strchr(pInBuf,' ');
		if (! pWordEnd )
			pWordEnd = strchr(pInBuf,'\t' );
		if (! pWordEnd )
			pWordEnd = pInBuf+strlen(pInBuf);
	}
	if ((! pWordEnd ) || (pWordEnd == pInBuf ) )
		return NULL;										// no word found
	memcpy( pOutbuf, pInBuf, pWordEnd-pInBuf );
	pOutbuf[pWordEnd-pInBuf]=0;

	pInBuf = pWordEnd;
	if ( bQuote )
		pInBuf++;
	return pInBuf;
}

// GetKeyValueFromBuffer:
//		fills in "key" and "val" respectively and returns "true" if succeeds.
//		returns false if:
//			a) end-of-buffer is reached (then "val" is empty)
//			b) error occurs (then "val" is the error message)
//
static bool GetKeyValueFromBuffer( CUtlBuffer &buffer, char *key, char *val )
{
	char buf[2048];

	while( buffer.GetBytesRemaining() )
	{
		buffer.GetLine( buf, sizeof( buf ) );

		// Scanning algorithm
		char *pComment = strpbrk( buf, "#\n\r" );
		if ( pComment )
			*pComment = 0;

		pComment = strstr( buf, "//" );
		if ( pComment)
			*pComment = 0;

		const char *scan = buf;
		scan=GetPossiblyQuotedWord( scan, key );
		if ( scan )
		{
			scan=GetPossiblyQuotedWord( scan, val );
			if ( scan )
				return true;
			else
			{
				sprintf( val, "parameter %s has no value", key );
				return false;
			}
		}
	}

	val[0] = 0;
	return false;
}

bool HasSuffix( const char *pFileName, const char *pSuffix )
{
	if ( !pFileName || !*pFileName || !pSuffix || !*pSuffix )
		return false;

	int fileLen   = Q_strlen( pFileName );
	int suffixLen = Q_strlen( pSuffix );
	if ( fileLen <= suffixLen )
		return false;

	return !Q_strnicmp( pFileName + fileLen - suffixLen, pSuffix, suffixLen );
}


//-----------------------------------------------------------------------------
// Loads the config information from a PSD file
//-----------------------------------------------------------------------------
static bool LoadConfigFromPSD( const char *pFileName, CUtlBuffer &buf, bool bMissingConfigOk, CRC32_t *pCRC )
{
	CUtlBuffer bufFile;
	bool bOK = LoadFile( pFileName, bufFile, false, pCRC );
	if ( !bOK )
	{
		VTexError( "VTex: \"%s\" is not a valid PSD file!\n", pFileName );
		return false;
	}

	VTexMsg( "config file %s\n", pFileName );
	bOK = IsPSDFile( bufFile );
	if ( !bOK )
	{
		VTexError( "VTex: \"%s\" is not a valid PSD file!\n", pFileName );
		return true;
	}

	PSDImageResources imgres = PSDGetImageResources( bufFile );
	PSDResFileInfo resFileInfo( imgres.FindElement( PSDImageResources::eResFileInfo ) );
	PSDResFileInfo::ResFileInfoElement descr = resFileInfo.FindElement( PSDResFileInfo::eDescription );
	if ( !descr.m_pvData )
	{
		if ( bMissingConfigOk )
			return true;

		VTexError( "VTex: \"%s\" does not contain vtex configuration info!\n", pFileName );
		return true;
	}

	buf.EnsureCapacity( descr.m_numBytes );
	buf.Put( descr.m_pvData, descr.m_numBytes );
	return true;
}


//-----------------------------------------------------------------------------
// Loads the .psd file or .txt file associated with the .tga and gets out various data
//-----------------------------------------------------------------------------
static bool LoadConfigFile( const char *pFileBaseName, VTexConfigInfo_t &info )
{
	// Tries to load .txt, then .psd
	bool bOK = false;

	int lenBaseName = strlen( pFileBaseName );
	char *pFileName = ( char * )stackalloc( lenBaseName + strlen( ".tga" ) + 1 );
	strcpy( pFileName, pFileBaseName );

	// Try TGA file with config
	memcpy( pFileName + lenBaseName, ".tga", 5 );
	bool bTgaExists = FileExistsAbsolute( pFileName );
	if ( bTgaExists && !g_bNoTga )
	{
		// Look for the TGA's associated TXT file
		g_eMode = BITMAP_FILE_TYPE_TGA;
		
		memcpy( pFileName + lenBaseName, ".txt", 5 );
		CUtlBuffer bufFile( 0, 0, CUtlBuffer::TEXT_BUFFER );
		bOK = LoadFile( pFileName, bufFile, false, &info.m_uiInputHash );
		if ( bOK )
		{
			VTexMsg( "Config file %s\n", pFileName );

			{
				char key[2048];
				char val[2048];
				while( GetKeyValueFromBuffer( bufFile, key, val ) )
				{
					info.ParseOptionKey( key, val );
				}

				if ( val[0] )
				{
					VTexError( "%s: %s\n", pFileName, val );
					return false;
				}

				if ( g_eMode == BITMAP_FILE_TYPE_PFM )
				{
					VTexWarning( "%s specifies PFM, but TGA with same name also exists - possible ambiguity?\n", pFileName );
				}
			}
		}
		else
		{
			memcpy( pFileName + lenBaseName, ".tga", 5 );
			//VTexMsg( "No config file for %s\n", pFileName );
			bOK = true;
		}
	}
	if ( g_bNoTga && bTgaExists  )
	{
		VTexWarningNoPause( "-notga disables \"%s\"\n", pFileName );
	}

	// PSD file attempt
	memcpy( pFileName + lenBaseName, ".psd", 5 );
	bool bPsdExists = FileExistsAbsolute( pFileName );
	if ( !bOK && bPsdExists && !g_bNoPsd && !g_bUsePfm ) // If PSD mode was not disabled
	{
		g_eMode = BITMAP_FILE_TYPE_PSD;

		CUtlBuffer bufDescr( 0, 0, CUtlBuffer::TEXT_BUFFER );
		bOK = LoadConfigFromPSD( pFileName, bufDescr, true, &info.m_uiInputHash );
		if ( bufDescr.TellMaxPut() > 0 )
		{
			char key[2048];
			char val[2048];
			while( GetKeyValueFromBuffer( bufDescr, key, val ) )
			{
				info.ParseOptionKey( key, val );
			}

			if ( val[0] )
			{
				VTexError( "%s: %s\n", pFileName, val );
				return false;
			}
		}
	}
	else if ( bPsdExists && !g_bUsePfm )
	{
		if ( bOK )
		{
			VTexWarningNoPause( "psd file \"%s\" exists, but not used, delete tga and txt files to use psd file directly\n", pFileName );
		}
		else if ( g_bNoPsd )
		{
			VTexWarningNoPause( "-nopsd disables \"%s\"\n", pFileName );
		}
	}

	// PFM file attempt
	memcpy( pFileName + lenBaseName, ".pfm", 5 );
	bool bPfmExists = FileExistsAbsolute( pFileName );
	if ( !bOK && bPfmExists && g_bUsePfm )
	{
		g_eMode = BITMAP_FILE_TYPE_PFM;

		CUtlBuffer bufFile( 0, 0, CUtlBuffer::TEXT_BUFFER );
		bOK = LoadFile( pFileName, bufFile, false, &info.m_uiInputHash );
	}

	// Try TXT file as config again for TGA cubemap / PFM
	memcpy( pFileName + lenBaseName, ".txt", 5 );
	bool bTxtExists = FileExistsAbsolute( pFileName );
	if ( !bOK && bTxtExists )
	{
		g_eMode = BITMAP_FILE_TYPE_TGA;

		CUtlBuffer bufFile( 0, 0, CUtlBuffer::TEXT_BUFFER );
		bOK = LoadFile( pFileName, bufFile, false, &info.m_uiInputHash );
		if ( bOK )
		{
			VTexMsg( "Config file %s\n", pFileName );

			{
				char key[2048];
				char val[2048];
				while( GetKeyValueFromBuffer( bufFile, key, val ) )
				{
					info.ParseOptionKey( key, val );
				}

				if ( val[0] )
				{
					VTexError( "%s: %s\n", pFileName, val );
					return false;
				}
			}
		}
	}

	if ( g_eMode == BITMAP_FILE_TYPE_PFM )
	{
		if ( g_bUsedAsLaunchableDLL && !( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_NOCOMPRESS ) )
		{
			info.m_nFlags |= TEXTUREFLAGS_NOMIP;
		}
		
		if ( g_bUsedAsLaunchableDLL && !HasSuffix( pFileBaseName, "_hdr" ) && !HasSuffix( pFileBaseName, "_disp" ) )
		{
			VTexWarning( "PFM files should be suffixed with '_hdr' or '_disp'\n" );
		}
	}
	else if ( g_bUsedAsLaunchableDLL && HasSuffix( pFileBaseName, "hdr" ) )
	{
		VTexWarning( "Only HDR images (.PFM files) should be suffixed with 'hdr'\n" );
	}

	if ( !bOK )
	{
		VTexWarning( " \"%s\" does not specify valid %s%sPFM+TXT files!\n",
					pFileBaseName,
					g_bNoPsd ? "" : "PSD or ",
					g_bNoTga ? "" : "TGA or " );
		return false;
	}

	if ( ( info.m_bNormalToDuDv || ( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_NORMAL_DUDV ) ) &&
		!( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_PREMULT_COLOR_ONEOVERMIP ) )
	{
		VTexMsg( "Implicitly setting premultcolorbyoneovermiplevel since you are generating a dudv map\n" );
		info.m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_PREMULT_COLOR_ONEOVERMIP;
	}

	if ( ( info.m_bNormalToDuDv || ( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_NORMAL_DUDV ) ) )
	{
		VTexMsg( "Implicitly setting trilinear since you are generating a dudv map\n" );
		info.m_nFlags |= TEXTUREFLAGS_TRILINEAR;
	}

	if ( Q_stristr( pFileBaseName, "_normal" ) )
	{
		if( !( info.m_nFlags & TEXTUREFLAGS_NORMAL ) )
		{
			if( !g_Quiet )
			{
				VTexMsgEx( stderr,
					"Implicitly setting:\n"
					"\t\"normal\" \"1\"\n"
					"since filename ends in \"_normal\"\n"
					);
			}
			info.m_nFlags |= TEXTUREFLAGS_NORMAL;
		}
	}

	if ( Q_stristr( pFileBaseName, "ssbump" ) )
	{
		if( !( info.m_nFlags & TEXTUREFLAGS_SSBUMP ) )
		{
			if( !g_Quiet )
			{
				VTexMsgEx( stderr,
					"Implicitly setting:\n"
					"\t\"ssbump\" \"1\"\n"
					"since filename includes \"ssbump\"\n"
					);
			}
			info.m_nFlags |= TEXTUREFLAGS_SSBUMP;
		}
	}

	if ( Q_stristr( pFileBaseName, "_dudv" ) )
	{
		if( !info.m_bNormalToDuDv && !info.m_bDuDv )
		{
			if( !g_Quiet )
			{
				VTexMsgEx( stderr,
					"Implicitly setting:\n"
					"\t\"dudv\" \"1\"\n"
					"since filename ends in \"_dudv\"\n"
					"If you are trying to convert from a normal map to a dudv map, put \"normaltodudv\" \"1\" in description.\n"
					);
			}
			info.m_bDuDv = true;
		}
	}

	// Displacement map
	if ( Q_stristr( pFileBaseName, "_disp" ) )
	{
		if( !info.m_bDisplacementMap )
		{
			info.m_bDisplacementMap = true;
			if ( ( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_FILTER_NICE ) && !g_Quiet )
			{
				VTexMsgEx( stderr, "Implicitly disabling nice filtering\n" );
			}
			info.m_vtfProcOptions.flags0 &= ~VtfProcessingOptions::OPT_FILTER_NICE;

			if ( !( info.m_nFlags & TEXTUREFLAGS_NOMIP ) && !g_Quiet )
			{
				VTexMsgEx( stderr, "Implicitly disabling mip map generation\n" );
			}
			info.m_nFlags &= ~TEXTUREFLAGS_NOMIP;
		}
	}

	// Turn off nice filtering if we are a cube map (takes too long with buildcubemaps) or
	// if we are a normal map (looks like terd.)
	if ( ( info.m_nFlags & TEXTUREFLAGS_NORMAL ) || info.m_bIsCubeMap )
	{
		if ( ( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_FILTER_NICE ) && !g_Quiet )
		{
			VTexMsgEx( stderr, "Implicitly disabling nice filtering\n" );
		}
		info.m_vtfProcOptions.flags0 &= ~VtfProcessingOptions::OPT_FILTER_NICE;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Loads the .psd file or .txt file associated with the .tga and gets out various data
//-----------------------------------------------------------------------------
static CDmePrecompiledTexture *LoadConfigFile( const char *pFullPath, BitmapFileType_t *pMode, CRC32_t *pInputHash )
{
	// Based on the extension, we do different things
	const char *pExt = Q_GetFileExtension( pFullPath );
	if ( !pExt )
	{
		VTexError( "VTex: Bogus file name \"%s\"!\n", pFullPath );
		return NULL;
	}

	const char *pOrigFullPath = pFullPath;
	bool bConfigFileSpecified = ( !Q_stricmp( pExt, "txt" ) || !Q_stricmp( pExt, "tex" ) );

	CDmePrecompiledTexture *pPrecompiledTexture = NULL;

	*pMode = BITMAP_FILE_TYPE_UNKNOWN;
	char pTexFile[MAX_PATH];
	if ( !Q_stricmp( pExt, "tga" ) || !Q_stricmp( pExt, "pfm" ) )
	{
		*pMode = !Q_stricmp( pExt, "tga" ) ? BITMAP_FILE_TYPE_TGA : BITMAP_FILE_TYPE_PFM;
		Q_strncpy( pTexFile, pFullPath, sizeof(pTexFile) );
		Q_SetExtension( pTexFile, "tex", sizeof(pTexFile) );
		if ( FileExistsAbsolute( pTexFile ) )
		{
			// If we ask for a tga or pfm, and a tex file exists, use the tex loader below
			pFullPath = pTexFile;
			pExt = "tex";
		}
		else
		{
			Q_SetExtension( pTexFile, "txt", sizeof(pTexFile) );
			if ( FileExistsAbsolute( pTexFile ) )
			{
				// If we ask for a tga or pfm, and a tex file exists, use the tex loader below
				pFullPath = pTexFile;
				pExt = "txt";
			}
			else
			{
				pPrecompiledTexture = CreateElement< CDmePrecompiledTexture >( "root", DMFILEID_INVALID );
				pPrecompiledTexture->m_ImageFileName = Q_UnqualifiedFileName( pFullPath );
				pPrecompiledTexture->AddProcessor< CDmeTP_ComputeMipmaps >( "computeMipmaps" );
			}
		}
	}
	
	if ( !Q_stricmp( pExt, "psd" ) )
	{
		CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
		if ( !LoadConfigFromPSD( pFullPath, buf, false, pInputHash ) )
			return NULL;

		if ( buf.TellMaxPut() == 0 )
			return NULL;

		char pTempPath[MAX_PATH];
		Q_strncpy( pTempPath, pFullPath, sizeof(pTempPath) );
		Q_SetExtension( pTempPath, "__psdtxt", sizeof(pTempPath) );
		DmElementHandle_t hElement;
		const char *pEncoding = g_pDataModel->IsDMXFormat( buf ) ? "keyvalues2" : "tex_source1";
		if ( !g_pDataModel->Unserialize( buf, pEncoding, "tex", NULL, pTempPath, CR_FORCE_COPY, hElement ) )
			return NULL;

		pPrecompiledTexture = GetElement< CDmePrecompiledTexture >( hElement );
		pPrecompiledTexture->m_ImageFileName = Q_UnqualifiedFileName( pFullPath );
		*pMode = BITMAP_FILE_TYPE_PSD;
	}
	
	bool bIsImportedFile = !Q_stricmp( pExt, "txt" );
	if ( !Q_stricmp( pExt, "tex" ) || bIsImportedFile )
	{
		CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
		bool bOK = LoadFile( pFullPath, buf, false, pInputHash );
		if ( !bOK )
			return NULL;

		DmElementHandle_t hElement;
		if ( !g_pDataModel->Unserialize( buf, !bIsImportedFile ? "keyvalues2" : "tex_source1", "tex", NULL, pFullPath, CR_FORCE_COPY, hElement ) )
			return NULL;

		pPrecompiledTexture = GetElement< CDmePrecompiledTexture >( hElement );

		char pFileBase[ MAX_PATH ];
		Q_strncpy( pFileBase, Q_UnqualifiedFileName( pOrigFullPath ), sizeof(pFileBase) );

		if ( Q_stricmp( pPrecompiledTexture->m_ImageFileName, "__unspecified_texture" ) )
		{
			if ( !bConfigFileSpecified && Q_stricmp( pPrecompiledTexture->m_ImageFileName, pFileBase ) )
			{
				Warning( ".tex specified a different file name (\"%s\") than the command-line did (\"%s\")!\n",
					pPrecompiledTexture->m_ImageFileName.Get(), pFileBase );
				return NULL;
			}
		}
		else
		{
			if ( bConfigFileSpecified )
			{
				char *pTestExt[3] = { ".tga", ".pfm", ".psd" };
				char pTestFileTest[ MAX_PATH ];
				char pTestFile[ MAX_PATH ];
				Q_strncpy( pTestFile, pFullPath, sizeof(pTestFile) );
				int i;
				for ( i = 0; i < 3; ++i )
				{
					Q_SetExtension( pTestFile, pTestExt[i], sizeof(pTestFile) );
					MakeSrcFileName( pPrecompiledTexture, pTestFile, 0, 0, 0, 
						pPrecompiledTexture->m_nVolumeTextureDepth > 1 ? 0 : -1, pTestFileTest, sizeof(pTestFileTest) );
					if ( FileExistsAbsolute( pTestFileTest ) )
					{
						Q_strncpy( pFileBase, Q_UnqualifiedFileName( pTestFile ), sizeof(pFileBase) );
						break;
					}
				}
				if ( i == 3 )
				{
					Warning( "Unable to find image file associated with file \"%s\"!\n", pFullPath );
					return false;
				}
			}

			pPrecompiledTexture->m_ImageFileName = pFileBase;
		}

		const char *pTextureExt = Q_GetFileExtension( pPrecompiledTexture->m_ImageFileName );
		BitmapFileType_t nTextureMode = BITMAP_FILE_TYPE_UNKNOWN;
		if ( !Q_stricmp( pTextureExt, "tga" ) )
		{
			nTextureMode = BITMAP_FILE_TYPE_TGA;
		}
		else if ( !Q_stricmp( pTextureExt, "pfm" ) )
		{
			nTextureMode = BITMAP_FILE_TYPE_PFM;
		}
		else if ( pTextureExt )
		{
			VTexError( "VTex: Bogus texture file name encountered \"%s\"!\n", pPrecompiledTexture->m_ImageFileName.Get() );
			return NULL;
		}

		if ( *pMode != BITMAP_FILE_TYPE_UNKNOWN && *pMode != nTextureMode )
		{
			VTexError( "VTex: Specified to build file \"%s\", but file \"%s\" is specified in the associated .tex file!\n", (char *)pFullPath, pPrecompiledTexture->m_ImageFileName.Get() );
			return NULL;
		}
	}

	/*
	if ( g_eMode == BITMAP_FILE_TYPE_PFM )
	{
		if ( g_bUsedAsLaunchableDLL && !( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_NOCOMPRESS ) )
		{
			info.m_nFlags |= TEXTUREFLAGS_NOMIP;
		}

		if ( g_bUsedAsLaunchableDLL && !HasSuffix( pFileBaseName, "_hdr" ) && !HasSuffix( pFileBaseName, "_disp" ) )
		{
			VTexWarning( "PFM files should be suffixed with '_hdr' or '_disp'\n" );
		}
	}
	else if ( g_bUsedAsLaunchableDLL && HasSuffix( pFileBaseName, "hdr" ) )
	{
		VTexWarning( "Only HDR images (.PFM files) should be suffixed with 'hdr'\n" );
	}

	if ( ( info.m_bNormalToDuDv || ( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_NORMAL_DUDV ) ) &&
		!( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_PREMULT_COLOR_ONEOVERMIP ) )
	{
		VTexMsg( "Implicitly setting premultcolorbyoneovermiplevel since you are generating a dudv map\n" );
		info.m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_PREMULT_COLOR_ONEOVERMIP;
	}

	if ( ( info.m_bNormalToDuDv || ( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_NORMAL_DUDV ) ) )
	{
		VTexMsg( "Implicitly setting trilinear since you are generating a dudv map\n" );
		info.m_nFlags |= TEXTUREFLAGS_TRILINEAR;
	}

	// Turn off nice filtering if we are a cube map (takes too long with buildcubemaps) or
	// if we are a normal map (looks like terd.)
	if ( ( info.m_nFlags & TEXTUREFLAGS_NORMAL ) || info.m_bIsCubeMap )
	{
		if ( ( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_FILTER_NICE ) && !g_Quiet )
		{
			VTexMsgEx( stderr, "implicitly disabling nice filtering\n" );
		}
		info.m_vtfProcOptions.flags0 &= ~VtfProcessingOptions::OPT_FILTER_NICE;
	}
	*/

	char pTextureName[MAX_PATH];
	GenerateResourceName( pFullPath, NULL, pTextureName, sizeof(pTextureName) );
	pPrecompiledTexture->SetName( pTextureName );
	return pPrecompiledTexture;
}

void Usage( void )
{
	VTexError( 
		"\n"
		"  Usage: vtex [-outdir dir] [-nopause] [-mkdir] [-shader ShaderName] [-vmtparam Param Value] tex1.txt tex2.txt ...\n"
		"\n"
		"  -quiet            : don't print anything out, don't pause for input\n"
		"  -warningsaserrors : treat warnings as errors\n"
		"  -nopause          : don't pause for input\n"
		"  -nomkdir          : don't create destination folder if it doesn't exist\n"
		"  -shader           : make a .vmt for this texture, using this shader (e.g. \"vtex -shader UnlitGeneric blah.tga\")\n"
		"  -vmtparam         : adds parameter and value to the .vmt file\n"
		"  -outdir <dir>     : write output to the specified dir regardless of source filename and vproject\n"
		"  -deducepath       : deduce path of sources by target file names\n"
		"  -extractsrc       : extract approximate src art out of a vtf\n"
		"  -dontbuild        : don't build the input files into VTFs (usually used with extractsrc)\n"
		"  -quickconvert     : use with \"-nop4 -dontusegamedir -quickconvert\" to upgrade old .vmt files\n"
		"  -dontusegamedir   : output files in same folder as inputs (for use with -extractsrc and -quickconvert)\n"
		"  -crcvalidate      : validate .vmt against the sources\n"
		"  -crcforce         : generate a new .vmt even if sources crc matches\n"
		"  -nop4             : don't check files out in Perforce\n"
		"  -nopsd            : skip .psd files (e.g. use this with \"vtex *.*\")\n"
		"  -notga            : skip .tga files (e.g. use this with \"vtex *.*\")\n"
		"  -oldcubepath      : old cubemap method, expects 6 input files, suffixed: 'up', 'dn', 'lf', 'rt', 'ft', 'bk'\n"
		"\n"
		"\teg: -vmtparam $ignorez 1 -vmtparam $translucent 1\n"
		"\n"
		"  Note that you can use wildcards and that you can also chain them\n"
		"  e.g. materialsrc/monster1/*.tga materialsrc/monster2/*.tga\n" );
}

bool GetOutputDir( const char *inputName, char *outputDir )
{
	if ( g_ForcedOutputDir[0] )
	{
		strcpy( outputDir, g_ForcedOutputDir );
	}
	else
	{
		// Is inputName a relative path?
		char buf[MAX_PATH];
		Q_MakeAbsolutePath( buf, sizeof( buf ), inputName, NULL );
		Q_FixSlashes( buf );
		
		const char *pTmp = Q_stristr( buf, "materialsrc\\" );
		if( !pTmp )
		{
			return false;
		}
		pTmp += strlen( "materialsrc/" );
		strcpy( outputDir, gamedir );
		strcat( outputDir, "materials/" );
		strcat( outputDir, pTmp );
		Q_StripFilename( outputDir );
	}
	if( !g_Quiet )
	{
		VTexMsg( "Output directory: %s\n", outputDir );
	}
	return true;
}

bool IsCubeFromFileNames( const char *inputBaseName )
{
	char fileName[MAX_PATH];
	// Do Strcmp for ".hdr" to make sure we aren't ripping too much stuff off.
	Q_StripExtension( inputBaseName, fileName, MAX_PATH );
	const char *pInputExtension = inputBaseName + Q_strlen( fileName );
	Q_strncat( fileName, "rt", MAX_PATH, COPY_ALL_CHARACTERS );
	Q_strncat( fileName, pInputExtension, MAX_PATH, COPY_ALL_CHARACTERS );
	Q_strncat( fileName, GetSourceExtension(), MAX_PATH, COPY_ALL_CHARACTERS );
	struct	_stat buf;
	if( _stat( fileName, &buf ) != -1 )
	{
		return true;
	}
	else
	{
		return false;
	}
}

#ifdef PLATFORM_WINDOWS
int Find_Files( WIN32_FIND_DATA &wfd, HANDLE &hResult, const char *basedir, const char *extension )
{
	char	filename[MAX_PATH] = {0};

	BOOL bMoreFiles = TRUE;
	
	if ( hResult && ( INVALID_HANDLE_VALUE != hResult ) )
	{
		bMoreFiles = FindNextFile( hResult, &wfd);
	}
	else
	{
		memset(&wfd, 0, sizeof(WIN32_FIND_DATA));
		char search[260] = {0};
		sprintf( search, "%s\\*.*", basedir );
		hResult = FindFirstFile( search, &wfd );

		if ( INVALID_HANDLE_VALUE == hResult )
			return 0;
	}

	if ( bMoreFiles )
	{
		// Skip . and ..
		if ( wfd.cFileName[0] == '.' )
		{
			return FF_TRYAGAIN;
		}

		// If it's a subdirectory, just recurse down it
		if ( (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
		{
			char	subdir[MAX_PATH];
			sprintf( subdir, "%s\\%s", basedir, wfd.cFileName );

			// Recurse
			// -- Find_Files( wfd, hResult, basedir, extension );
			return FF_TRYAGAIN;
		}

		// Check that it's a tga
		//

		char fname[_MAX_FNAME] = {0};
		char ext[_MAX_EXT] = {0};

		_splitpath( wfd.cFileName, NULL, NULL, fname, ext );

		// Not the type we want.
		if ( stricmp( ext, extension ) )
			return FF_DONTPROCESS;

		// Check for .vmt
		sprintf( filename, "%s\\%s.vmt", basedir, fname );
		// Exists, so don't overwrite it
		if ( FileExistsAbsolute( filename ) )
			return FF_PROCESS;

		char texturename[ _MAX_PATH ] = {0};
		char *p = ( char * )basedir;

		// Skip over the base path to get a material system relative path
		// p += strlen( wfd.cFileName ) + 1;

		// Construct texture name
		sprintf( texturename, "%s\\%s", p, fname );

		// Convert all to lower case
		strlwr( texturename );
		strlwr( filename );
		
		return FF_PROCESS;
	}
	else
	{
		FindClose( hResult );
		hResult = INVALID_HANDLE_VALUE;
		return 0;
	}
}
#endif // PLATFORM_WINDOWS

char const *GetBareFileName( char const *pszPathName )
{
	const char *pBaseName = &pszPathName[strlen( pszPathName ) - 1];
	while( (pBaseName >= pszPathName) && *pBaseName != '\\' && *pBaseName != '/' )
	{
		pBaseName--;
	}
	pBaseName++;
	return pBaseName;
}

bool Process_File_Internal( char *pInputBaseName, int maxlen, bool bOutputPwlColorConversion, bool bConvertTo360PwlSrgb );
bool Process_File( char *pInputBaseName, int maxlen )
{
	const char *pExtension = V_GetFileExtension( pInputBaseName );
	if ( pExtension != NULL )
	{
		if ( V_stricmp( pExtension, "pfm" ) == 0 )
		{
			g_bNoTga = true;
			g_bNoPsd = true;
			g_bUsePfm = true;
		}
	}

	// Build standard .vtf
	bool ret = Process_File_Internal( pInputBaseName, maxlen, false, false );
	if ( ret == false )
		return ret;

	// Build XBox 360 srgb .pwl.vtf
	if ( g_bSupportsXBox360 == true )
	{
		ret = Process_File_Internal( pInputBaseName, maxlen, true, true );
	}

	return ret;
}

bool Process_File_Internal( char *pInputBaseName, int maxlen, bool bOutputPwlColorConversion, bool bConvertTo360PwlSrgb )
{
	Q_FixSlashes( pInputBaseName, '/' );

	char requestedInputBaseName[1024];
	Q_strncpy( requestedInputBaseName, pInputBaseName, maxlen  );

	char outputDir[1024];
	Q_StripExtension( pInputBaseName, pInputBaseName, maxlen );

	if ( CommandLine()->FindParm( "-deducepath" ) )
	{
		strcpy( outputDir, pInputBaseName );

		// If it is not a full path, try making it a full path
		if ( pInputBaseName[0] != '/' &&
			 pInputBaseName[1] != ':' )
		{
			// Convert to full path
			getcwd( outputDir, sizeof( outputDir ) );
			Q_FixSlashes( outputDir, '/' );
			Q_strncat( outputDir, "/", sizeof( outputDir ) );
			Q_strncat( outputDir, pInputBaseName, sizeof( outputDir ) );
		}

		// If it is pointing inside "/materials/" make it go for "/materialsrc/"
		char *pGame = strstr( outputDir, "/game/" );
		char *pMaterials = strstr( outputDir, "/materials/" );
		if ( pGame && pMaterials && ( pGame < pMaterials ) )
		{
			// "u:/data/game/tf/materials/"  ->  "u:/data/content/tf/materialsrc/"
			int numExtraBytes = strlen( "/content/.../materialsrc/" ) - strlen( "/game/.../materials/" );
			int numConvertBytes = pMaterials + strlen( "/materials/" ) - outputDir;
			memmove( outputDir + numConvertBytes + numExtraBytes, outputDir + numConvertBytes, strlen( outputDir ) - numConvertBytes + 1 );
			
			int numMidBytes = pMaterials - pGame - strlen( "/game" );
			memmove( pGame + strlen( "/content" ), pGame + strlen( "/game" ), numMidBytes );

			memmove( pGame, "/content", strlen( "/content" ) );
			memmove( pGame + strlen( "/content" ) + numMidBytes, "/materialsrc/", strlen( "/materialsrc/" ) );
		}

		Q_strncpy( pInputBaseName, outputDir, maxlen );
	}

	if( !g_Quiet )
	{
		VTexMsg( "\nInput file: %s\n", pInputBaseName );
	}

	if( g_UseGameDir )
	{
		if ( !GetOutputDir( pInputBaseName, outputDir ) )
		{
			VTexError( "Problem figuring out outputdir for %s\n", pInputBaseName );
			return FALSE;
		}
		sprintf( requestedInputBaseName, "%s/%s.vtf", outputDir, GetBareFileName( pInputBaseName ) );
	}
	else // if (!g_UseGameDir)
	{
		strcpy( outputDir, pInputBaseName );
		sprintf( requestedInputBaseName, "%s.vtf", outputDir );
		Q_StripFilename(outputDir);
	}

	// Usage:
	//			vtex -nop4 -dontusegamedir -quickconvert u:\data\game\tf\texture.vtf
	// Will read the old texture format and write the new texture format
	//
	if ( CommandLine()->FindParm( "-quickconvert" ) )
	{
		VTexMsg( "Quick convert of '%s'...\n", pInputBaseName );

		char chFileNameConvert[ 512 ];
		sprintf( chFileNameConvert, "%s.vtf", pInputBaseName );

		IVTFTexture *pVtf = CreateVTFTexture();
		CUtlBuffer bufFile;
		LoadFile( chFileNameConvert, bufFile, true, NULL );
		bool bRes = pVtf->Unserialize( bufFile );
		if ( !bRes )
			VTexError( "Failed to read '%s'!\n", chFileNameConvert );

		// Determine the CRC if it was there
		// CRC32_t uiDataHash = 0;
		// CRC32_t *puiDataHash = &uiDataHash;
		// Assert( sizeof( uiDataHash ) == sizeof( int ) );
		// if ( !pVtf->GetResourceData( VTexConfigInfo_t::VTF_INPUTSRC_CRC, ... ) )

		AttachShtFile( pInputBaseName, pVtf, NULL );

		// Update the CRC
		// if ( puiDataHash )
		// {
		//	pVtf->InitResourceDataSection( VTexConfigInfo_t::VTF_INPUTSRC_CRC, *puiDataHash );
		// }
		// Remove the CRC when quick-converting
		pVtf->SetResourceData( VTexConfigInfo_t::VTF_INPUTSRC_CRC, NULL, 0 );

		bufFile.Clear();
		bRes = pVtf->Serialize( bufFile );
		if ( !bRes )
			VTexError( "Failed to write '%s'!\n", chFileNameConvert );

		DestroyVTFTexture( pVtf );

		if ( FILE *fw = fopen( chFileNameConvert, "wb" ) )
		{
			fwrite( bufFile.Base(), 1, bufFile.TellPut(), fw );
			fclose( fw );
		}
		else
			VTexError( "Failed to open '%s' for writing!\n", chFileNameConvert );

		VTexMsg( "... succeeded.\n" );

		return TRUE;
	}

	VTexConfigInfo_t info;
	bool bConfigLoaded = LoadConfigFile( pInputBaseName, info );

	// Usage:
	//			vtex -nop4 -dontusegamedir -deducepath -extractsrc u:\data\game\tf\texture.vtf
	// Will read the vtf texture and write the texture src files
	//
	if ( CommandLine()->FindParm( "-extractsrc" ) && !bConvertTo360PwlSrgb ) //&& !bConfigLoaded )
	{
		VTexMsg( "Extracting from vtf '%s'.\n", requestedInputBaseName );
		VTexMsg( "Saving extracted src as '%s'.\n", pInputBaseName );

		// Create directory
		char szCreateDirectoryCommand[2048];
		V_snprintf( szCreateDirectoryCommand, 1024, "if not exist %s mkdir %s", pInputBaseName, pInputBaseName );
		V_StripFilename( szCreateDirectoryCommand );
		V_FixSlashes( szCreateDirectoryCommand, '\\' );
		//Msg( "*** system: %s\n", szCreateDirectoryCommand );
		system( szCreateDirectoryCommand );

		// Create the texture and unserialize file data
		SmartIVTFTexture pVtf( CreateVTFTexture() );
		CUtlBuffer bufFile;
		LoadFile( requestedInputBaseName, bufFile, true, NULL );
		bool bRes = pVtf->Unserialize( bufFile );
		if ( !bRes )
		{
			VTexError( "Failed to read '%s'!\n", requestedInputBaseName );
			return FALSE;
		}

		Msg( "vtf width: %d\n", pVtf->Width() );
		Msg( "vtf height: %d\n", pVtf->Height() );
		Msg( "vtf numFrames: %d\n", pVtf->FrameCount() );
		Msg( "vtf numFaces: %d\n", pVtf->FaceCount() );
		Msg( "vtf cubemap: %s\n", pVtf->IsCubeMap() ? "true" : "false" );
		
		Vector vecReflectivity = pVtf->Reflectivity();
		Msg( "vtf reflectivity: %f %f %f\n", vecReflectivity[0], vecReflectivity[1], vecReflectivity[2] );
		
		ImageFormat srcFormat = pVtf->Format();
		char const *szFormatName = ImageLoader::GetName( srcFormat );
		Msg( "vtf format: %s\n", szFormatName );

		if ( pVtf->FrameCount() > 1 )
		{
			VTexError( "Vtf source extraction is not implemented for multiple frames!\n" );
			return FALSE;
		}
		if ( pVtf->FaceCount() > 1 || pVtf->IsCubeMap() )
		{
			VTexError( "Vtf source extraction is not implemented for cubemaps!\n" );
			return FALSE;
		}

		VTexMsg( "Extracting image data '%s.tga'...\n", pInputBaseName );
		{
			char chVtf2TgaCommand[2048];
			Q_snprintf( chVtf2TgaCommand, sizeof( chVtf2TgaCommand ) - 1,
				"vtf2tga.exe -i %s -o %s.tga",
				requestedInputBaseName, pInputBaseName );
			int iSysCall = system( chVtf2TgaCommand );
			if ( iSysCall )
			{
				VTexError( "Failed to extract image data!\n" );
				return FALSE;
			}
			
			char chTgaFile[ MAX_PATH ];
			sprintf( chTgaFile, "%s.tga", pInputBaseName );
			CP4AutoAddFile autop4( chTgaFile );
		}

		// Now create the accompanying text file with texture settings
		char chTxtFileName[1024];
		Q_snprintf( chTxtFileName, sizeof( chTxtFileName ) - 1,
			"%s.txt", pInputBaseName );
		VTexMsg( "Saving text data '%s.txt'...\n", pInputBaseName );

		for ( int i = 0; i < 2; i++ ) // First see if we need to write a txt file, then write it
		{
			FILE *fTxtFile = NULL;
			if ( i == 1 )
			{
				// Try to open for writing without p4 edit
				fTxtFile = fopen( chTxtFileName, "wt" );
				if ( !fTxtFile )
				{
					// p4 edit, then try to open for writing
					CP4AutoEditFile autop4( chTxtFileName );
					fTxtFile = fopen( chTxtFileName, "wt" );
					if ( !fTxtFile )
					{
						// Can't open file for writing
						VTexError( "Failed to create '%s'!\n", chTxtFileName );
						return FALSE;
					}
				}
			}

			if ( strstr( szFormatName, "BGR" ) || strstr( szFormatName, "RGB" ) )
			{
				if ( i == 0 )
				{
					continue;
				}
				else
				{
					fprintf( fTxtFile, "nocompress 1\n" );
				}
			}

			if ( pVtf->Flags() & TEXTUREFLAGS_NOMIP )
			{
				if ( i == 0 )
				{
					continue;
				}
				else
				{
					fprintf( fTxtFile, "nomip 1\n" );
				}
			}

			if ( pVtf->Flags() & TEXTUREFLAGS_NOLOD )
			{
				if ( i == 0 )
				{
					continue;
				}
				else
				{
					fprintf( fTxtFile, "nolod 1\n" );
				}
			}

			if ( pVtf->Flags() & TEXTUREFLAGS_CLAMPS )
			{
				if ( i == 0 )
				{
					continue;
				}
				else
				{
					fprintf( fTxtFile, "clamps 1\n" );
				}
			}

			if ( pVtf->Flags() & TEXTUREFLAGS_CLAMPT )
			{
				if ( i == 0 )
				{
					continue;
				}
				else
				{
					fprintf( fTxtFile, "clampt 1\n" );
				}
			}

			if ( pVtf->Flags() & TEXTUREFLAGS_CLAMPU )
			{
				if ( i == 0 )
				{
					continue;
				}
				else
				{
					fprintf( fTxtFile, "clampu 1\n" );
				}
			}

			if ( pVtf->Flags() & TEXTUREFLAGS_PROCEDURAL )
			{
				if ( i == 0 )
				{
					continue;
				}
				else
				{
					fprintf( fTxtFile, "procedural 1\n" );
				}
			}

			if ( pVtf->Flags() & TEXTUREFLAGS_TRILINEAR )
			{
				if ( i == 0 )
				{
					continue;
				}
				else
				{
					fprintf( fTxtFile, "trilinear 1\n" );
				}
			}

			if ( pVtf->Flags() & TEXTUREFLAGS_POINTSAMPLE )
			{
				if ( i == 0 )
				{
					continue;
				}
				else
				{
					fprintf( fTxtFile, "pointsample 1\n" );
				}
			}

			if ( pVtf->Flags() & TEXTUREFLAGS_ANISOTROPIC )
			{
				if ( i == 0 )
				{
					continue;
				}
				else
				{
					fprintf( fTxtFile, "anisotropic 1\n" );
				}
			}

			if ( pVtf->Flags() & TEXTUREFLAGS_HINT_DXT5 )
			{
				if ( i == 0 )
				{
					continue;
				}
				else
				{
					fprintf( fTxtFile, "dxt5 1\n" );
				}
			}

			if ( pVtf->Flags() & TEXTUREFLAGS_NORMAL )
			{
				if ( i == 0 )
				{
					continue;
				}
				else
				{
					fprintf( fTxtFile, "normal 1\n" );
				}
			}

			if ( pVtf->Flags() & TEXTUREFLAGS_ALL_MIPS )
			{
				if ( i == 0 )
				{
					continue;
				}
				else
				{
					fprintf( fTxtFile, "allmips 1\n" );
				}
			}

			if ( pVtf->Flags() & TEXTUREFLAGS_MOST_MIPS )
			{
				if ( i == 0 )
				{
					continue;
				}
				else
				{
					fprintf( fTxtFile, "mostmips 1\n" );
				}
			}

			if ( i == 1 )
			{
				fclose( fTxtFile );
				CP4AutoAddFile autop4( chTxtFileName );
			}

			// Didn't find anything to write, so quit
			if ( i == 0 )
			{
				break;
			}
		}

		VTexMsg( "'%s' extracted.\n", pInputBaseName );
	}

	if ( CommandLine()->FindParm( "-dontbuild" ) )
		return TRUE;

	if ( !bConfigLoaded )
	{
		bConfigLoaded = LoadConfigFile( pInputBaseName, info );
	}

	if ( !bConfigLoaded )
		return FALSE;

	if ( !g_bOldCubemapPath && !g_bUsePfm )
	{
		// Error out if we find old-style cubemap filenames
		if ( IsCubeFromFileNames( pInputBaseName ) )
		{
			VTexError( "File is old-style cubemap. Convert to new-style cubemap (single 'T-shape' image) or use old path with '-oldcubepath'" );
		}
	}
	else if ( !info.m_bIsCubeMap )
	{
		// Look for old-style cubemap filenames
		info.m_bIsCubeMap = IsCubeFromFileNames( pInputBaseName );
	}

	if ( bConvertTo360PwlSrgb )
	{
		SetFlagValue( info.m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_SRGB_PC_TO_360, 1 );
	}

	if( ( info.m_nStartFrame == -1 && info.m_nEndFrame != -1 ) ||
		( info.m_nStartFrame != -1 && info.m_nEndFrame == -1 ) )
	{
		VTexError( "%s: If you use startframe, you must use endframe, and vice versa.\n", pInputBaseName );
		return FALSE;
	}

	const char *pBaseName = GetBareFileName( pInputBaseName );

	bool bProcessedFilesOK = ProcessFiles( pInputBaseName, outputDir, pBaseName, info );
	if ( !bProcessedFilesOK )
		return FALSE;

	// create vmts if necessary
	if( g_ShaderName )
	{
		char buf[1024];
		sprintf( buf, "%s/%s.vmt", outputDir, pBaseName );
		const char *tmp = Q_stristr( outputDir, "materials" );
		FILE *fp;
		if( tmp )
		{
			// check if the file already exists.
			fp = fopen( buf, "r" );
			if( fp )
			{
				if ( !g_Quiet )
					VTexMsgEx( stderr, "vmt file \"%s\" already exists\n", buf );

				fclose( fp );
			}
			else
			{
				fp = fopen( buf, "w" );
				if( fp )
				{
					if ( !g_Quiet )
						VTexMsgEx( stderr, "Creating vmt file: %s/%s\n", tmp, pBaseName );
					tmp += strlen( "materials/" );
					fprintf( fp, "\"%s\"\n", g_ShaderName );
					fprintf( fp, "{\n" );
					fprintf( fp, "\t\"$baseTexture\" \"%s/%s\"\n", tmp, pBaseName );

					int i;
					for( i=0;i<g_NumVMTParams;i++ )
					{
						fprintf( fp, "\t\"%s\" \"%s\"\n", g_VMTParams[i].m_szParam, g_VMTParams[i].m_szValue );
					}

					fprintf( fp, "}\n" );
					fclose( fp );

					CP4AutoAddFile autop4( buf );
				}
				else
				{
					VTexWarning( "Couldn't open \"%s\" for writing\n", buf );
				}
			}

		}
		else
		{
			VTexWarning( "Couldn't find \"materials/\" in output path\n", buf );
		}
	}

	return TRUE;
}

class CVTexLoggingListener : public ILoggingListener
{
public:
	virtual void Log( const LoggingContext_t *pContext, const tchar *pMessage )
	{
		VTexMsg( "%s", pMessage );
		if ( pContext->m_Severity == LS_ERROR )
		{
			Pause();
		}
	}
};

CVTexLoggingListener g_VTexLoggingListener;

class CVTex : public IVTex, public ILaunchableDLL
{
public:
	int VTex( int argc, char **argv );

	// ILaunchableDLL, used by vtex.exe.
	virtual int main( int argc, char **argv )
	{
		g_bUsedAsLaunchableDLL = true;

		// Run the vtex logic
		int iResult = VTex( argc, argv );

		return iResult;
	}

	virtual int VTex( CreateInterfaceFn fsFactory, const char *pGameDir, int argc, char **argv )
	{
		g_pFileSystem = g_pFullFileSystem = (IFileSystem*)fsFactory( FILESYSTEM_INTERFACE_VERSION, NULL );
		if ( !g_pFileSystem )
		{
			Error( "IVTex3::VTex - fsFactory can't get '%s' interface.", FILESYSTEM_INTERFACE_VERSION );
			return 0;
		}
	
		Q_strncpy( gamedir, pGameDir, sizeof( gamedir ) );
		Q_AppendSlash( gamedir, sizeof( gamedir ) );
		return VTex( argc, argv );
	}
};

static class CSuggestGameDirHelper
{
public:
	static bool SuggestFn( CFSSteamSetupInfo const *pFsSteamSetupInfo, char *pchPathBuffer, int nBufferLength, bool *pbBubbleDirectories );
	bool MySuggestFn( CFSSteamSetupInfo const *pFsSteamSetupInfo, char *pchPathBuffer, int nBufferLength, bool *pbBubbleDirectories );

public:
	CSuggestGameDirHelper() : m_pszInputFiles( NULL ), m_numInputFiles( 0 ) {}

public:
	char const * const *m_pszInputFiles;
	size_t m_numInputFiles;
} g_suggestGameDirHelper;

bool CSuggestGameDirHelper::SuggestFn( CFSSteamSetupInfo const *pFsSteamSetupInfo, char *pchPathBuffer, int nBufferLength, bool *pbBubbleDirectories )
{
	return g_suggestGameDirHelper.MySuggestFn( pFsSteamSetupInfo, pchPathBuffer, nBufferLength, pbBubbleDirectories );
}

bool CSuggestGameDirHelper::MySuggestFn( CFSSteamSetupInfo const *pFsSteamSetupInfo, char *pchPathBuffer, int nBufferLength, bool *pbBubbleDirectories )
{
	if ( !m_numInputFiles || !m_pszInputFiles )
		return false;

	if ( pbBubbleDirectories )
		*pbBubbleDirectories = true;

	for ( int k = 0; k < m_numInputFiles; ++ k )
	{
		Q_MakeAbsolutePath( pchPathBuffer, nBufferLength, m_pszInputFiles[ k ] );
		return true;
	}

	return false;
}

int CVTex::VTex( int argc, char **argv )
{
	CommandLine()->CreateCmdLine( argc, argv );

	if ( g_bUsedAsLaunchableDLL )
	{
		LoggingSystem_PushLoggingState();
		LoggingSystem_RegisterLoggingListener( &g_VTexLoggingListener );
	}

	MathLib_Init(  2.2f, 2.2f, 0.0f, 1.0f, false, false, false, false );
	if( argc < 2 )
	{
		Usage();
	}

	g_UseGameDir = true; // make sure this is initialized to true.

	int i;
	i = 1;
	while( i < argc )
	{
		if( stricmp( argv[i], "-quiet" ) == 0 )
		{
			i++;
			g_Quiet = true;
			g_NoPause = true; // no point in pausing if we aren't going to print anything out.
		}
		else if( stricmp( argv[i], "-nopause" ) == 0 )
		{
			i++;
			g_NoPause = true;
		}
		else if ( stricmp( argv[i], "-WarningsAsErrors" ) == 0 )
		{
			i++;
			g_bWarningsAsErrors = true;
		}
		else if ( stricmp( argv[i], "-UseStandardError" ) == 0 )
		{
			i++;
			g_bUseStandardError = true;
		}
		else if ( stricmp( argv[i], "-nopsd" ) == 0 ) 
		{
			i++;
			g_bNoPsd = true;
		}
		else if ( stricmp( argv[i], "-notga" ) == 0 ) 
		{
			i++;
			g_bNoTga = true;
		}
		else if ( stricmp( argv[i], "-nomkdir" ) == 0 ) 
		{
			i++;
			g_CreateDir = false;
		}
		else if ( stricmp( argv[i], "-mkdir" ) == 0 ) 
		{
			i++;
			g_CreateDir = true;
		}
		else if ( stricmp( argv[i], "-game" ) == 0 )
		{
			i += 2;
		}
		else if ( stricmp( argv[i], "-outdir" ) == 0 )
		{
			strcpy( g_ForcedOutputDir, argv[i+1] );
			i += 2;
		}
		else if ( stricmp( argv[i], "-dontusegamedir" ) == 0)
		{
			++i;
			g_UseGameDir = false;
		}
		else if( stricmp( argv[i], "-shader" ) == 0 )
		{
			i++;
			if( i < argc )
			{
				g_ShaderName = argv[i];
				i++;
			}
		}
		else if( stricmp( argv[i], "-vproject" ) == 0 )
		{
			// skip this one. . we dont' use it internally.
			i += 2;
		}
		else if( stricmp( argv[i], "-vmtparam" ) == 0 )
		{
			if( g_NumVMTParams < MAX_VMT_PARAMS )
			{
				i++;
				if( i < argc - 1 )
				{
					g_VMTParams[g_NumVMTParams].m_szParam = argv[i];
					i++;

					if( i < argc - 1 )
					{
						g_VMTParams[g_NumVMTParams].m_szValue = argv[i];
						i++;
					}
					else
					{
						g_VMTParams[g_NumVMTParams].m_szValue = "";
					}

					if( !g_Quiet )
					{
						VTexMsgEx( stderr, "Adding .vmt parameter: \"%s\"\t\"%s\"\n", 
							g_VMTParams[g_NumVMTParams].m_szParam,
							g_VMTParams[g_NumVMTParams].m_szValue );
					}

					g_NumVMTParams++;
				}
			}
			else
			{
				VTexMsgEx( stderr, "Exceeded max number of vmt parameters, extra ignored ( max %d )\n", MAX_VMT_PARAMS );
			}
		}
		else if ( stricmp( argv[i], "-oldcubepath" ) == 0 )
		{
			// Revert to the old cube/skybox authoring path, which expects 6 suffixed input files (up, dn, lf, rt, ft, bk)
			VTexMsg( "Using old cubemap method. Expecting 6 input files, suffixed: 'up', 'dn', 'lf', 'rt', 'ft', 'bk'\n" );
			g_bOldCubemapPath = true;
			i++;
		}
		else if( argv[i][0] == '-' )
		{
			// Just assuming that these are valid flags with no args
			++ i;
		}
		else
		{
			break;
		}
	}

	// Set the suggest game info directory helper
	g_suggestGameDirHelper.m_pszInputFiles = argv + i;
	g_suggestGameDirHelper.m_numInputFiles = argc - i;
	SetSuggestGameInfoDirFn( CSuggestGameDirHelper::SuggestFn );

	// g_pFileSystem may have been inherited with -inherit_filesystem.
	if (g_UseGameDir && !g_pFileSystem)
	{
		FileSystem_Init( argv[i] );

		Q_FixSlashes( gamedir, '/' );
	}

	// Check if we need to build 360 .pwl.vtf versions
	KeyValues *pKeyValues = new KeyValues( "gameinfo.txt" );
	if ( pKeyValues != NULL )
	{
		if ( g_pFileSystem && pKeyValues->LoadFromFile( g_pFileSystem, "gameinfo.txt" ) )
		{
			g_bSupportsXBox360 = pKeyValues->GetBool( "SupportsXBox360" );
		}
		pKeyValues->deleteThis();
	}

	// Initialize P4
	bool bP4DLLExists = false;
	if ( g_pFullFileSystem )
	{
		bP4DLLExists = g_pFullFileSystem->FileExists( "p4lib.dll", "EXECUTABLE_PATH" );
	}

	if ( g_bUsedAsLaunchableDLL && !CommandLine()->FindParm( "-nop4" ) && bP4DLLExists )
	{
		const char *pModuleName = "p4lib.dll";
		CSysModule *pModule = Sys_LoadModule( pModuleName );
		if ( !pModule )
		{
			VTexMsg( "Can't load %s.\n", pModuleName );
			return -1;
		}
		CreateInterfaceFn fn = Sys_GetFactory( pModule );
		if ( !fn )
		{
			VTexMsg( "Can't get factory from %s.\n", pModuleName );
			Sys_UnloadModule( pModule );
			return -1;
		}
		p4 = (IP4 *)fn( P4_INTERFACE_VERSION, NULL );
		if ( !p4 )
		{
			VTexMsg( "Can't get IP4 interface from %s, proceeding with -nop4.\n", pModuleName );
			g_p4factory->SetDummyMode( true );
		}
		else
		{
			p4->Connect( FileSystem_GetFactory() );
			p4->Init();
		}
	}
	else
	{
		g_p4factory->SetDummyMode( true );
	}

	//
	// Setup p4 factory
	//
	{
		// Set the named changelist
		g_p4factory->SetOpenFileChangeList( "VTex Auto Checkout" );
	}

	// Parse args
	for( ; i < argc; i++ )
	{
		if ( argv[i][0] == '-' )
			continue; // Assuming flags

		char pInputBaseName[MAX_PATH];
		Q_strncpy( pInputBaseName, argv[i], sizeof(pInputBaseName) );
		// int maxlen = Q_strlen( pInputBaseName ) + 1;

		if ( !Q_strstr( pInputBaseName, "*." ) )
		{
			Process_File( pInputBaseName, sizeof(pInputBaseName) );
			continue;
		}

#ifdef PLATFORM_WINDOWS
		char	basedir[MAX_PATH];
		char	ext[_MAX_EXT];
		char    filename[_MAX_FNAME];

		_splitpath( pInputBaseName, NULL, NULL, NULL, ext ); //find extension wanted

		if ( !Q_ExtractFilePath ( pInputBaseName, basedir, sizeof( basedir ) ) )
			strcpy( basedir, ".\\" );

		WIN32_FIND_DATA wfd;
		HANDLE hResult = INVALID_HANDLE_VALUE;
		
		for ( int iFFType;
			( iFFType = Find_Files( wfd, hResult, basedir, ext ) ) != 0; )
		{
			sprintf( filename, "%s%s", basedir, wfd.cFileName );

			if ( wfd.cFileName[0] != '.' && iFFType == FF_PROCESS )
				Process_File( filename, sizeof( filename ) );
		}
#endif

	}

	// Shutdown P4
	if ( g_bUsedAsLaunchableDLL && p4 )
	{
		p4->Shutdown();
		p4->Disconnect();
	}

	Pause();

	if ( g_bUsedAsLaunchableDLL )
	{
		LoggingSystem_PopLoggingState();
	}

	return 0;
}

CVTex g_VTex;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CVTex, IVTex, IVTEX_VERSION_STRING, g_VTex );
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CVTex, ILaunchableDLL, LAUNCHABLE_DLL_INTERFACE_VERSION, g_VTex );


//-----------------------------------------------------------------------------
// New vtex compiler entry point
//-----------------------------------------------------------------------------
class CVTexCompiler : public CTier2DmAppSystem< IResourceCompiler >
{
	typedef CTier2DmAppSystem< IResourceCompiler > BaseClass;

	// Methods of IAppSystem
public:
	virtual InitReturnVal_t Init();
	
	// Methods of IVTexCompiler
public:
	virtual bool CompileResource( const char *pFullPath, IResourceCompilerRegistry *pRegistry, CResourceStream *pPermanentStream, CResourceStream *pDataStream );
	virtual bool CompileResource( CUtlBuffer &buf, const char *pFullPath, IResourceCompilerRegistry *pRegistry, CResourceStream *pPermanentStream, CResourceStream *pDataStream );

private:
	bool CompileResource( CDmElement *pElement, const char *pElementFileName, IResourceCompilerRegistry *pRegistry, CResourceStream *pPermanentStream, CResourceStream *pDataStream );
	CDmeTexture *CompileResource( CDmePrecompiledTexture *pPrecompiledResource );
};


static CVTexCompiler s_VTexCompiler;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CVTexCompiler, IResourceCompiler, RESOURCE_COMPILER_INTERFACE_VERSION, s_VTexCompiler );


//-----------------------------------------------------------------------------
// Init
//-----------------------------------------------------------------------------
InitReturnVal_t CVTexCompiler::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	MathLib_Init(  2.2f, 2.2f, 0.0f, 1.0f, false, false, false, false );
	InstallDmElementFactories();

	// FIXME: Should I use the same datamodel as in resource compiler?
	g_pDataModel->SetUndoEnabled( false );

	g_Quiet = true;
	g_NoPause = true; 
	g_bWarningsAsErrors = false;
	g_CreateDir = true;
	g_UseGameDir = false;
	g_bSupportsXBox360 = true;

	return INIT_OK;
}


//-----------------------------------------------------------------------------
// Creates a texture from a precompiled texture
//-----------------------------------------------------------------------------
static void SetTextureStateFromPrecompiledTexture( CDmePrecompiledTexture *pPrecompiledTexture, CDmeTexture *pTexture )
{
	pTexture->m_bClampS = pPrecompiledTexture->m_bClampS;
	pTexture->m_bClampT = pPrecompiledTexture->m_bClampT;
	pTexture->m_bClampU = pPrecompiledTexture->m_bClampU;
	pTexture->m_bNoDebugOverride = pPrecompiledTexture->m_bNoDebugOverride;
	pTexture->m_bNoLod = pPrecompiledTexture->m_bNoLod;
	pTexture->m_bNormalMap = pPrecompiledTexture->m_bNormalMap;
	pTexture->SetFilterType( (DmeTextureFilter_t)pPrecompiledTexture->m_nFilterType.Get() );
	pTexture->m_flBumpScale = pPrecompiledTexture->m_flBumpScale;
}


//-----------------------------------------------------------------------------
// Loads a resource into a dme element
//-----------------------------------------------------------------------------
bool CVTexCompiler::CompileResource( const char *pFullPath, IResourceCompilerRegistry *pRegistry, CResourceStream *pPermanentStream, CResourceStream *pDataStream )
{
	BitmapFileType_t mode;
	CRC32_t nInputHash;
	CRC32_Init( &nInputHash );

	CDmePrecompiledTexture *pPrecompiledTexture = LoadConfigFile( pFullPath, &mode, &nInputHash );
	if ( !pPrecompiledTexture->ValidateValues() )
		return NULL;																	 

	pPrecompiledTexture->m_pSourceTexture = CreateElement< CDmeTexture >( pPrecompiledTexture->GetName(), DMFILEID_INVALID );

	char pFullDir[ MAX_PATH ];
	Q_strncpy( pFullDir, pFullPath, sizeof( pFullDir ) );
	Q_StripFilename( pFullDir );
	LoadSourceImages( pFullDir, pPrecompiledTexture, mode, &nInputHash );

	CRC32_Final( &nInputHash );
	bool bOK = CompileResource( pPrecompiledTexture, pFullPath, pRegistry, pPermanentStream, pDataStream );
	g_pDataModel->RemoveFileId( pPrecompiledTexture->GetFileId() );
	return bOK;
}

bool CVTexCompiler::CompileResource( CUtlBuffer &buf, const char *pFullPath, IResourceCompilerRegistry *pRegistry, CResourceStream *pPermanentStream, CResourceStream *pDataStream )
{
	DmElementHandle_t hRoot;
	if ( !g_pDataModel->Unserialize( buf, NULL, NULL, NULL, pFullPath, CR_FORCE_COPY, hRoot ) )
		return NULL;

	CDmElement *pElement = GetElement< CDmElement >( hRoot );
	bool bOk = CompileResource( pElement, pFullPath, pRegistry, pPermanentStream, pDataStream );
	g_pDataModel->RemoveFileId( pElement->GetFileId() );
	return bOk;
}


CDmeTexture *CVTexCompiler::CompileResource( CDmePrecompiledTexture *pPrecompiledResource )
{
	CDmeTexture *pTexture = pPrecompiledResource->m_pSourceTexture;
	SetTextureStateFromPrecompiledTexture( pPrecompiledResource, pTexture );

	IThreadPool *pVTexThreadPool = CreateNewThreadPool();
	ThreadPoolStartParams_t startParams;
	startParams.fDistribute = TRS_TRUE;
	pVTexThreadPool->Start( startParams );

	FloatBitMap_t::SetThreadPool( pVTexThreadPool );
	int nCount = pPrecompiledResource->m_Processors.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeTexture *pDestTexture = CreateElement< CDmeTexture >( pTexture->GetName(), pTexture->GetFileId() );
		CDmeTextureProcessor *pProcessor = pPrecompiledResource->m_Processors[i];
		pProcessor->ProcessTexture( pTexture, pDestTexture );
		DestroyElement( pTexture, TD_DEEP );
		pTexture = pDestTexture;
	}

	ImageFormat dstFormat = ComputeDesiredImageFormat( pPrecompiledResource, pTexture );
	if ( !ImageLoader::IsCompressed( dstFormat ) )
	{
		pTexture->ForEachImage( &CDmeImage::ConvertFormat, dstFormat );
	}
	else
	{
		CDmeTexture *pDestTexture = CreateElement< CDmeTexture >( pTexture->GetName(), pTexture->GetFileId() );
		pDestTexture->CompressTexture( pTexture, dstFormat );
		DestroyElement( pTexture, TD_DEEP );
		pTexture = pDestTexture;
	}

	FloatBitMap_t::SetThreadPool( NULL );
	DestroyThreadPool( pVTexThreadPool );

	return pTexture;
}


//-----------------------------------------------------------------------------
// Writes resources
//-----------------------------------------------------------------------------
bool CVTexCompiler::CompileResource( CDmElement *pElement, const char *pElementFileName,
	IResourceCompilerRegistry *pRegistry, CResourceStream *pPermanentStream, CResourceStream *pDataStream )
{
	CDmePrecompiledTexture *pPrecompiledTexture = CastElement< CDmePrecompiledTexture >( pElement );
	if ( !pPrecompiledTexture )
		return false;

	CDmeTexture *pTexture = CompileResource( pPrecompiledTexture );

	const char *pResourceIdString = pPrecompiledTexture->GetName();

	RegisterResourceInfo_t info;
	info.m_nType = RESOURCE_TYPE_TEXTURE;
	info.m_nId = ComputeResourceIdHash( pResourceIdString ); 
	info.m_nDataOffset = pDataStream->Tell();
	info.m_nCompressionType = RESOURCE_COMPRESSION_NONE;
	info.m_nFlags = 0;

	// Deal with permanent data
	info.m_nPermanentDataOffset = pPermanentStream->Tell();
	info.m_nPermanentDataSize = sizeof( TextureHeader_t );

	TextureHeader_t *pSpec = pPermanentStream->Allocate< TextureHeader_t >( 1 );
	memset( pSpec, 0, sizeof(TextureHeader_t) );
	pSpec->m_nWidth = pTexture->Width();
	pSpec->m_nHeight = pTexture->Height();
	pSpec->m_nNumMipLevels = pTexture->MipLevelCount();
	pSpec->m_nDepth = pTexture->Depth();
	pSpec->m_nImageFormat = pTexture->Format();
	pSpec->m_nFlags = 0;
	if ( pTexture->m_bClampS )
	{
		pSpec->m_nFlags |= TSPEC_SUGGEST_CLAMPS;
	}
	if ( pTexture->m_bClampT )
	{
		pSpec->m_nFlags |= TSPEC_SUGGEST_CLAMPT;
	}
	if ( pTexture->m_bClampU )
	{
		pSpec->m_nFlags |= TSPEC_SUGGEST_CLAMPU;
	}
	if ( pTexture->m_bNoLod )
	{
		pSpec->m_nFlags |= TSPEC_NO_LOD;
	}
	pSpec->m_nMultisampleType = RENDER_MULTISAMPLE_NONE;
	pSpec->m_Reflectivity.Init( 1, 1, 1, 1 ); // = pTexture->Reflectivity();

	// Deal with cacheable data
	// Allocate space for the structure, ensuring proper alignment
	pDataStream->Align( 16 );
	uint32 nStart = pDataStream->Tell();
	
	// FIXME: Should store mip levels smallest to largest
	int nMipCount = pTexture->MipLevelCount();
	for ( int m = nMipCount; --m >= 0; )
	{
		int nFrameCount = pTexture->FrameCount();
		for ( int f = 0; f < nFrameCount; ++f )
		{
			CDmeTextureFrame *pFrame = pTexture->GetFrame( f );
			CDmeImageArray *pMipLevel = pFrame->GetMipLevel( m );
			int nImageCount = pMipLevel->ImageCount();
			for ( int i = 0; i < nImageCount; ++i )
			{
				CDmeImage *pImage = pMipLevel->GetImage( i );
				int nSize = pImage->SizeInBytes();
				void *pDest = pDataStream->AllocateBytes( nSize );
				Q_memcpy( pDest, pImage->ImageBits(), nSize );
			}
		}
	}

	info.m_nDataSize = pDataStream->Tell() - nStart;
	info.m_nUncompressedDataSize = info.m_nDataSize;

	pRegistry->RegisterResource( info );
	pRegistry->RegisterUsedType( "TextureBits_t", false );
	pRegistry->RegisterUsedType( "TextureHeader_t", true );

	DestroyElement( pTexture, TD_DEEP );

	return true;
}
