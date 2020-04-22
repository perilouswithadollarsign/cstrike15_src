//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef _X360
	#ifdef PROTECTED_THINGS_ENABLE
		#undef PROTECTED_THINGS_ENABLE
	#endif
#endif 

#include "platform.h"

// HACK: Need ShellExecute for PSD updates
#ifdef IS_WINDOWS_PC
#include <windows.h>
#include <shellapi.h>
#pragma comment ( lib, "shell32"  )
#endif

#include "shaderapi/ishaderapi.h"
#include "materialsystem_global.h"
#include "itextureinternal.h"
#include "utlsymbol.h"
#include "time.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "bitmap/imageformat.h"
#include "bitmap/tgaloader.h"
#include "bitmap/tgawriter.h"
#ifdef _WIN32
#include "direct.h"
#endif
#include "colorspace.h"
#include "string.h"
#ifndef _PS3
#include <malloc.h>
#endif
#include <stdlib.h>
#include "utlmemory.h"
#include "IHardwareConfigInternal.h"
#include "filesystem.h"
#include "tier1/strtools.h"
#include "vtf/vtf.h"
#include "materialsystem/materialsystem_config.h"
#include "mempool.h"
#include "texturemanager.h"
#include "utlbuffer.h"
#include "pixelwriter.h"
#include "tier1/callqueue.h"
#include "tier1/UtlStringMap.h"
#include "filesystem/IQueuedLoader.h"
#include "tier2/fileutils.h"
#include "filesystem.h"
#include "tier2/p4helpers.h"
#include "tier2/tier2.h"
#include "p4lib/ip4.h"
#include "ctype.h"
#include "ifilelist.h"
#include "tier0/icommandline.h"
#include "datacache/imdlcache.h"
#include "tier0/vprof.h"

#ifndef _PS3
#define MATSYS_INTERNAL
#endif

#include "cmaterialsystem.h"

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"

// this allows the command line to force the "all mips" flag to on for all textures
bool g_bForceTextureAllMips = false;

#define TEXTURE_FNAME_EXTENSION			PLATFORM_EXT ".vtf"
#define TEXTURE_FNAME_EXTENSION_NORMAL	"_normal" PLATFORM_EXT ".vtf"
#define TEXTURE_FNAME_EXTENSION_LEN		( sizeof( TEXTURE_FNAME_EXTENSION ) - 1 )

ConVar mat_spewalloc( "mat_spewalloc", "0", FCVAR_ARCHIVE );

ConVar mat_exclude_async_update( "mat_exclude_async_update", "1" );

extern CMaterialSystem g_MaterialSystem;

//-----------------------------------------------------------------------------
// Internal texture flags
//-----------------------------------------------------------------------------
enum InternalTextureFlags
{
	TEXTUREFLAGSINTERNAL_ERROR				= 0x00000001,
	TEXTUREFLAGSINTERNAL_ALLOCATED			= 0x00000002,
	TEXTUREFLAGSINTERNAL_PRELOADED			= 0x00000004, // CONSOLE:	textures that went through the preload process
	TEXTUREFLAGSINTERNAL_QUEUEDLOAD			= 0x00000008, // CONSOLE:	load using the queued loader
	TEXTUREFLAGSINTERNAL_EXCLUDED			= 0x00000020, // actual exclusion state
	TEXTUREFLAGSINTERNAL_SHOULDEXCLUDE		= 0x00000040, // desired exclusion state
	TEXTUREFLAGSINTERNAL_TEMPRENDERTARGET	= 0x00000080, // 360:		should only allocate texture bits upon first resolve, destroy at level end
	TEXTUREFLAGSINTERNAL_CACHEABLE			= 0x00000100, // 360:		candidate for cacheing
	TEXTUREFLAGSINTERNAL_REDUCED            = 0x00000200, // CONSOLE: true dimensions forced smaller (i.e. exclusion)
	TEXTUREFLAGSINTERNAL_TEMPEXCLUDED		= 0x00000400, // actual temporary exclusion state
	TEXTUREFLAGSINTERNAL_SHOULDTEMPEXCLUDE	= 0x00000800, // desired temporary exclusion state	
	TEXTUREFLAGSINTERNAL_TEMPEXCLUDE_UPDATE = 0x00001000, // private state bit used by temp exclusions
	TEXTUREFLAGSINTERNAL_FORCED_TO_EXCLUDE  = 0x00002000, // private state bit used to track/undo an artifical exclusion
	TEXTUREFLAGSINTERNAL_ASYNC_DONE			= 0x00004000, // async download for texture is done
};

//-----------------------------------------------------------------------------
// Use Warning to show texture flags.
//-----------------------------------------------------------------------------
static void PrintFlags( unsigned int flags )
{
	if ( flags & TEXTUREFLAGS_NOMIP )
	{
		Warning( "TEXTUREFLAGS_NOMIP|" );
	}
	if ( flags & TEXTUREFLAGS_NOLOD )
	{
		Warning( "TEXTUREFLAGS_NOLOD|" );
	}
	if ( flags & TEXTUREFLAGS_POINTSAMPLE )
	{
		Warning( "TEXTUREFLAGS_POINTSAMPLE|" );
	}
	if ( flags & TEXTUREFLAGS_TRILINEAR )
	{
		Warning( "TEXTUREFLAGS_TRILINEAR|" );
	}
	if ( flags & TEXTUREFLAGS_CLAMPS )
	{
		Warning( "TEXTUREFLAGS_CLAMPS|" );
	}
	if ( flags & TEXTUREFLAGS_CLAMPT )
	{
		Warning( "TEXTUREFLAGS_CLAMPT|" );
	}
	if ( flags & TEXTUREFLAGS_HINT_DXT5 )
	{
		Warning( "TEXTUREFLAGS_HINT_DXT5|" );
	}
	if ( flags & TEXTUREFLAGS_ANISOTROPIC )
	{
		Warning( "TEXTUREFLAGS_ANISOTROPIC|" );
	}
	if ( flags & TEXTUREFLAGS_PROCEDURAL )
	{
		Warning( "TEXTUREFLAGS_PROCEDURAL|" );
	}
	if ( flags & TEXTUREFLAGS_ALL_MIPS )
	{
		Warning( "TEXTUREFLAGS_ALL_MIPS|" );
	}
	if ( flags & TEXTUREFLAGS_MOST_MIPS )
	{
		Warning( "TEXTUREFLAGS_MOST_MIPS|" );
	}
	if ( flags & TEXTUREFLAGS_SINGLECOPY )
	{
		Warning( "TEXTUREFLAGS_SINGLECOPY|" );
	}
}


namespace TextureLodOverride
{
	struct OverrideInfo
	{
		OverrideInfo() : x( 0 ), y( 0 ) {}
		OverrideInfo( int8 x_, int8 y_ ) : x( x_ ), y( y_ ) {}
		int8 x, y;
	};

	// Override map
	typedef CUtlStringMap< OverrideInfo > OverrideMap_t;
	OverrideMap_t s_OverrideMap;

	// Retrieves the override info adjustments
	OverrideInfo Get( char const *szName )
	{
		UtlSymId_t idx = s_OverrideMap.Find( szName );
		if ( idx != s_OverrideMap.InvalidIndex() )
			return s_OverrideMap[ idx ];
		else
			return OverrideInfo();
	}

	// Combines the existing override info adjustments with the given one
	void Add( char const *szName, OverrideInfo oi )
	{
		OverrideInfo oiex = Get( szName );
		oiex.x += oi.x;
		oiex.y += oi.y;
		s_OverrideMap[ szName ] = oiex;
	}

}; // end namespace TextureLodOverride


namespace TextureLodExclude
{
	typedef CUtlStringMap< int > ExcludeMap_t;
	ExcludeMap_t s_ExcludeMap;

	int Get( char const *szName )
	{
		UtlSymId_t idx = s_ExcludeMap.Find( szName );
		if ( idx != s_ExcludeMap.InvalidIndex() )
		{
			return s_ExcludeMap[ idx ];
		}
		else
		{
			return -1;
		}
	}

	void Add( char const *szName, int iOverride )
	{
		UtlSymId_t idx = s_ExcludeMap.Find( szName );
		if ( idx != s_ExcludeMap.InvalidIndex() )
		{
			int &x = s_ExcludeMap[ idx ];
			x = iOverride;
		}
		else
		{
			s_ExcludeMap[ szName ] = iOverride;
		}
	}
}; // end namespace TextureLodExclude




//-----------------------------------------------------------------------------
// Base texture class
//-----------------------------------------------------------------------------

class CTexture : public ITextureInternal
{
public:
	CTexture();
	virtual ~CTexture();

	virtual const char *GetName( void ) const;
	const char *GetTextureGroupName( void ) const;

	// Stats about the texture itself
	virtual ImageFormat GetImageFormat() const;
	virtual int GetMappingWidth() const;
	virtual int GetMappingHeight() const;
	virtual int GetActualWidth() const;
	virtual int GetActualHeight() const;
	virtual int GetNumAnimationFrames() const;
	virtual bool IsTranslucent() const;
	virtual void GetReflectivity( Vector& reflectivity );

	// Reference counting
	virtual void IncrementReferenceCount( );
	virtual void DecrementReferenceCount( );
	virtual int GetReferenceCount() const;

	// Used to modify the texture bits (procedural textures only)
	virtual void SetTextureRegenerator( ITextureRegenerator *pTextureRegen, bool releaseExisting = true );

	// Little helper polling methods
	virtual bool IsNormalMap( ) const;
	virtual bool IsCubeMap( void ) const;
	virtual bool IsRenderTarget( ) const;
	virtual bool IsTempRenderTarget( void ) const;
	virtual bool IsProcedural() const;
	virtual bool IsMipmapped() const;
	virtual bool IsError() const;
	virtual bool IsDefaultPool() const;

	// For volume textures
	virtual bool IsVolumeTexture() const;
	virtual int GetMappingDepth() const;
	virtual int GetActualDepth() const;

	// Various ways of initializing the texture
	void InitFileTexture( const char *pTextureFile, const char *pTextureGroupName );
	void InitProceduralTexture( const char *pTextureName, const char *pTextureGroupName, int w, int h, int d, ImageFormat fmt, int nFlags );

	// Releases the texture's hw memory
	void Release();

	virtual void OnRestore();

	// Sets the filtering modes on the texture we're modifying
	void SetFilteringAndClampingMode();
	void Download( Rect_t *pRect = NULL, int nAdditionalCreationFlags = 0 );

	// Loads up information about the texture 
	virtual void Precache();

	// FIXME: Bogus methods... can we please delete these?
	virtual void GetLowResColorSample( float s, float t, float *color ) const;

	// Gets texture resource data of the specified type.
	// Params:
	//		eDataType		type of resource to retrieve.
	//		pnumBytes		on return is the number of bytes available in the read-only data buffer or is undefined
	// Returns:
	//		pointer to the resource data, or NULL. Note that the data from this pointer can disappear when
	// the texture goes away - you want to copy this data!
	virtual void *GetResourceData( uint32 eDataType, size_t *pNumBytes ) const;

	virtual int GetApproximateVidMemBytes( void ) const;

	// Stretch blit the framebuffer into this texture.
	virtual void CopyFrameBufferToMe( int nRenderTargetID = 0, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL );
	virtual void CopyMeToFrameBuffer( int nRenderTargetID = 0, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL );

	virtual ITexture *GetEmbeddedTexture( int nIndex );

	// Get the shaderapi texture handle associated w/ a particular frame
	virtual ShaderAPITextureHandle_t GetTextureHandle( int nFrame, int nChannel = 0 );

	// Sets the texture as the render target
	virtual void Bind( Sampler_t sampler, TextureBindFlags_t nBindFlags );
	virtual void Bind( Sampler_t sampler1, TextureBindFlags_t nBindFlags, int nFrame, Sampler_t sampler2 = SHADER_SAMPLER_INVALID );
	virtual void BindVertexTexture( VertexTextureSampler_t stage, int nFrame );

	// Set this texture as a render target	
	bool SetRenderTarget( int nRenderTargetID );

	// Set this texture as a render target (optionally set depth texture as depth buffer as well)
	bool SetRenderTarget( int nRenderTargetID, ITexture *pDepthTexture);

	virtual void MarkAsPreloaded( bool bSet );
	virtual bool IsPreloaded() const;

	virtual void MarkAsExcluded( bool bSet, int nDimensionsLimit, bool bMarkAsTrumpedExclude );
	virtual bool UpdateExcludedState();

	// Retrieve the vtf flags mask
	virtual unsigned int GetFlags( void ) const;

	virtual void ForceLODOverride( int iNumLodsOverrideUpOrDown );
	virtual void ForceExcludeOverride( int iExcludeOverride );

	//this isn't a MultiRenderTarget texture, do nothing
	virtual void AddDownsizedSubTarget( const char *pName, int iDownsizePow2, MaterialRenderTargetDepth_t depth ) {}
	virtual void SetActiveSubTarget( const char *pName ) {}

	void GetFilename( char *pOut, int maxLen ) const;
	virtual void ReloadFilesInList( IFileList *pFilesToReload );

#ifdef _PS3
	virtual void Ps3gcmRawBufferAlias( char const *pRTName );
#endif

	virtual bool MarkAsTempExcluded( bool bSet, int nExcludedDimensionLimit );

	virtual bool IsTempExcluded() const;
	virtual bool CanBeTempExcluded() const;

	virtual bool FinishAsyncDownload( AsyncTextureContext_t *pContext, void *pData, int nNumReadBytes, bool bAbort, float flMaxTimeMs );

	virtual bool IsForceExcluded() const;
	virtual bool ClearForceExclusion();

	virtual bool IsAsyncDone() const;

protected:
	bool IsDepthTextureFormat( ImageFormat fmt );
	void ReconstructTexture( void *pSourceData = NULL, int nSourceDataSize = 0 );
	void ReconstructPartialTexture( const Rect_t *pRect );
	bool HasBeenAllocated() const;
	void WriteDataToShaderAPITexture( int nFrameCount, int nFaceCount, int nFirstFace, int nMipCount, IVTFTexture *pVTFTexture, ImageFormat fmt );

	// Initializes/shuts down the texture
	void Init( int w, int h, int d, ImageFormat fmt, int iFlags, int iFrameCount );
	void Shutdown();

	// Sets the texture name
	void SetName( const char* pName );

	// Assigns/releases texture IDs for our animation frames
	void AllocateTextureHandles();
	void ReleaseTextureHandles();

	// Calculates info about whether we can make the texture smaller and by how much
	// Returns the number of skipped mip levels
	int ComputeActualSize( bool bIgnorePicmip = false, IVTFTexture *pVTFTexture = NULL );

	// Computes the actual format of the texture given a desired src format
	ImageFormat ComputeActualFormat( ImageFormat srcFormat );

	// Compute the actual mip count based on the actual size
	int ComputeActualMipCount( ) const;

	// Creates/releases the shader api texture
	virtual bool AllocateShaderAPITextures();
	virtual void FreeShaderAPITextures();

	// Download bits
	void DownloadTexture( Rect_t *pRect, void *pSourceData = NULL, int nSourceDataSize = 0 );
	bool DownloadAsyncTexture( AsyncTextureContext_t *pContext, void *pSourceData, int nSourceDataSize, float flMaxTimeMs );
	void ReconstructTextureBits( Rect_t *pRect );

	// Gets us modifying a particular frame of our texture
	void Modify( int iFrame );

	// Sets the texture clamping state on the currently modified frame
	void SetWrapState( );

	// Sets the texture filtering state on the currently modified frame
	void SetFilterState();

	// Loads the texture bits from a file. Optionally provides absolute path
	IVTFTexture *LoadTexttureBitsFromFileOrData( void *pSourceData, int nSourceDataSize, char **pResolvedFilename );
	IVTFTexture *LoadTextureBitsFromFile( char *pCacheFileName, char **pResolvedFilename );
	IVTFTexture *LoadTextureBitsFromData( char *pCacheFileName, void *pSourceData, int nSourceDataSize );
	IVTFTexture *HandleFileLoadFailedTexture( IVTFTexture *pVTFTexture );

	// Generates the procedural bits
	IVTFTexture *ReconstructProceduralBits( );
	IVTFTexture *ReconstructPartialProceduralBits( const Rect_t *pRect, Rect_t *pActualRect );

	// Sets up debugging texture bits, if appropriate
	bool SetupDebuggingTextures( IVTFTexture *pTexture );

	// Generate a texture that shows the various mip levels
	void GenerateShowMipLevelsTextures( IVTFTexture *pTexture );

	// Generate a RGBA 128 128 128 255 gray texture
	void GenerateGrayTexture( IVTFTexture *pTexture );

	void Cleanup( void );

	// Converts a source image read from disk into its actual format
	bool ConvertToActualFormat( IVTFTexture *pTexture );

	// Builds the low-res image from the texture 
	void LoadLowResTexture( IVTFTexture *pTexture );
	void CopyLowResImageToTexture( IVTFTexture *pTexture );
	
	void GetDownloadFaceCount( int &nFirstFace, int &nFaceCount );
	void ComputeMipLevelSubRect( const Rect_t* pSrcRect, int nMipLevel, Rect_t *pSubRect );

	IVTFTexture *GetScratchVTFTexture();
	IVTFTexture *GetScratchVTFAsyncTexture();

	int GetOptimalReadBuffer( FileHandle_t hFile, int nFileSize, CUtlBuffer &optimalBuffer );
	void FreeOptimalReadBuffer( int nMaxSize );

	void ApplyRenderTargetSizeMode( int &width, int &height, ImageFormat fmt );

	virtual bool IsMultiRenderTarget( void ) { return false; }

	void LoadResourceData( IVTFTexture *pTexture );
	void FreeResourceData();

protected:
#ifdef _DEBUG
	char *m_pDebugName;
#endif

	// Reflectivity vector
	Vector m_vecReflectivity;

	CUtlSymbol m_Name;

	CUtlSymbol m_ExcludedResolvedFileName;
	CUtlSymbol m_ResolvedFileName;
	FSAsyncControl_t m_hAsyncControl;

	// What texture group this texture is in (winds up setting counters based on the group name,
	// then the budget panel views the counters).
	CUtlSymbol m_TextureGroupName;

	unsigned int m_nFlags;
	unsigned int m_nInternalFlags;

	CInterlockedInt m_nRefCount;

	// This is the *desired* image format, which may or may not represent reality
	ImageFormat m_ImageFormat;

	// mappingWidth/Height and actualWidth/Height only differ 
	// if g_config.skipMipLevels != 0, or if the card has a hard limit
	// on the maximum texture size
	// This is the iWidth/iHeight for the data that m_pImageData points to.
	unsigned short m_nMappingWidth;
	unsigned short m_nMappingHeight;
	unsigned short m_nMappingDepth;

	// This is the iWidth/iHeight for whatever is downloaded to the card.
	unsigned short m_nActualWidth;		// needed for procedural
	unsigned short m_nActualHeight;		// needed for procedural
	unsigned short m_nActualDepth;

	unsigned short m_nActualMipCount;	// Mip count when it's actually used
	unsigned short m_nFrameCount;

	unsigned short m_nOriginalRTWidth;	// The values they initially specified. We generated a different width
	unsigned short m_nOriginalRTHeight;	// and height based on screen size and the flags they specify.

	unsigned char m_LowResImageWidth;
	unsigned char m_LowResImageHeight;

	short m_nDesiredDimensionLimit;			// part of texture exclusion
	short m_nDesiredTempDimensionLimit;
	short m_nActualDimensionLimit;			// value not necessarily accurate, but mismatch denotes dirty state
	unsigned short m_nMipSkipCount;

	// The set of texture ids for each animation frame
	ShaderAPITextureHandle_t *m_pTextureHandles;

	// a temporary copy of the texture handles used to support the temporary exclude feature
	ShaderAPITextureHandle_t *m_pTempTextureHandles;

	// lowresimage info - used for getting color data from a texture
	// without having a huge system mem overhead.
	// FIXME: We should keep this in compressed form. .is currently decompressed at load time.
#if !defined( _GAMECONSOLE )
	unsigned char *m_pLowResImage;
#else
	unsigned char m_LowResImageSample[4];
#endif

	ITextureRegenerator *m_pTextureRegenerator;

	// Used to help decide whether or not to recreate the render target if AA changes.
	RenderTargetType_t m_nOriginalRenderTargetType;
	RenderTargetSizeMode_t m_RenderTargetSizeMode;

	// Fixed-size allocator
//	DECLARE_FIXEDSIZE_ALLOCATOR( CTexture );
public:
	void InitRenderTarget( const char *pRTName, int w, int h, RenderTargetSizeMode_t sizeMode, 
		ImageFormat fmt, RenderTargetType_t type, unsigned int textureFlags,
		unsigned int renderTargetFlags );
	
	virtual void DeleteIfUnreferenced();

#if defined( _GAMECONSOLE )
	virtual bool ClearTexture( int r, int g, int b, int a );
#endif

#if defined( _X360 )
	virtual bool CreateRenderTargetSurface( int width, int height, ImageFormat format, bool bSameAsTexture, RTMultiSampleCount360_t multiSampleCount = RT_MULTISAMPLE_NONE );
#endif

	void FixupTexture( const void *pData, int nSize, LoaderError_t loaderError );

	void SwapContents( ITexture *pOther );

protected:
	// private data, generally from VTF resource extensions
	struct DataChunk
	{
		void Allocate( unsigned int numBytes ) 
		{
			m_pvData = new unsigned char[ numBytes ];
			m_numBytes = numBytes;
		}
		void Deallocate() const { delete [] m_pvData; }
		
		unsigned int m_eType;
		unsigned int m_numBytes;
		unsigned char *m_pvData;
	};
	CUtlVector< DataChunk > m_arrDataChunks;

private:
	bool ScheduleExcludeAsyncDownload();
	bool ScheduleAsyncDownload();
};

//a render target that is actually multiple textures that can be swapped out depending on the situation. Mostly to support recursive water in portal
class CTexture_MultipleRenderTarget : public CTexture
{
public:
	typedef CTexture BaseClass;

	CTexture_MultipleRenderTarget() :
	m_nActiveTarget( -1 ),
	m_nQueuedActiveTarget( -1 )
	{

	}

	virtual void Bind( Sampler_t sampler, TextureBindFlags_t nBindFlags )
	{
		Assert( materials->GetRenderContext()->GetCallQueue() == NULL );
		Assert( m_nActiveTarget < m_Targets.Count() );

		if( m_nActiveTarget < 0 )
			return BaseClass::Bind( sampler, nBindFlags );
		
		if( m_nActiveTarget < m_Targets.Count() )
		{
			g_pShaderAPI->BindTexture( sampler, nBindFlags, m_Targets[m_nActiveTarget].handle );
		}
		else
		{
			g_pShaderAPI->BindTexture( sampler, nBindFlags, INVALID_SHADERAPI_TEXTURE_HANDLE );
		}
	}

	virtual void Bind( Sampler_t sampler1, TextureBindFlags_t nBindFlags, int nFrame, Sampler_t sampler2 = SHADER_SAMPLER_INVALID )
	{
		Assert( materials->GetRenderContext()->GetCallQueue() == NULL );
		Assert( m_nActiveTarget < m_Targets.Count() );
		if( m_nActiveTarget < 0 )
			return BaseClass::Bind( sampler1, nBindFlags, nFrame, sampler2 );

		if ( g_pShaderDevice->IsUsingGraphics() )
		{
			if( m_nActiveTarget < m_Targets.Count() )
			{
				g_pShaderAPI->BindTexture( sampler1, nBindFlags, m_Targets[m_nActiveTarget].handle );
			}
			else
			{
				g_pShaderAPI->BindTexture( sampler1, nBindFlags, INVALID_SHADERAPI_TEXTURE_HANDLE );
			}
		}		
	}

	virtual ShaderAPITextureHandle_t GetTextureHandle( int nFrame, int nTextureChannel =0 )
	{
		Assert( materials->GetRenderContext()->GetCallQueue() == NULL );
		Assert( m_nActiveTarget < m_Targets.Count() );

		if( m_nActiveTarget < 0 )
			return BaseClass::GetTextureHandle( nFrame, nTextureChannel );

		if( m_nActiveTarget < m_Targets.Count() )
		{
			return m_Targets[m_nActiveTarget].handle;
		}
		else
		{
			return INVALID_SHADERAPI_TEXTURE_HANDLE;
		}
	}

	virtual void AddDownsizedSubTarget( const char *szName, int iDownsizePow2, MaterialRenderTargetDepth_t depth )
	{
		// normalize and convert to a symbol
		char szCleanName[MAX_PATH];
		SubTarget_t temp;
		temp.name = NormalizeTextureName( szName, szCleanName, sizeof( szCleanName ) );
		temp.iDownSizePow2 = iDownsizePow2;
		temp.handle = INVALID_SHADERAPI_TEXTURE_HANDLE;
		temp.depthHandle = INVALID_SHADERAPI_TEXTURE_HANDLE;
		temp.bHasSeparateDepth = (depth == MATERIAL_RT_DEPTH_SEPARATE) || (depth == MATERIAL_RT_DEPTH_ONLY);

		if( IsX360() )
		{
			if( HasBeenAllocated() )
			{
				//need to initialize these handles now
				extern int GetCreationFlags( int iTextureFlags, int iInternalTextureFlags, ImageFormat fmt ); //defined about 1000 lines down where it makes more logical sense to be
				int nCreateFlags = GetCreationFlags( m_nFlags, m_nInternalFlags, m_ImageFormat );

				// For depth only render target: adjust texture width/height
				// Currently we just leave it the same size, will update with further testing
				int nShaderApiCreateTextureDepth = ( ( m_nFlags & TEXTUREFLAGS_DEPTHRENDERTARGET ) && ( m_nOriginalRenderTargetType == RENDER_TARGET_ONLY_DEPTH ) ) ? 1 : m_nActualDepth;

				
				// Create all animated texture frames in a single call
				g_pShaderAPI->CreateTextures(
					&temp.handle, 1,
					m_nActualWidth / iDownsizePow2, m_nActualHeight / iDownsizePow2, nShaderApiCreateTextureDepth, m_ImageFormat, m_nActualMipCount,
					1, nCreateFlags, GetName(), GetTextureGroupName() );

				// Create the depth render target buffer
				if ( temp.bHasSeparateDepth )
				{
					MEM_ALLOC_CREDIT();

					char debugName[128];
					sprintf( debugName, "%s_ZBuffer", GetName() );

					temp.depthHandle = g_pShaderAPI->CreateDepthTexture( 
						m_ImageFormat, 
						m_nActualWidth / iDownsizePow2, 
						m_nActualHeight / iDownsizePow2,
						debugName,
						( m_nOriginalRenderTargetType == RENDER_TARGET_ONLY_DEPTH ) );
				}


#if defined( PLATFORM_X360 )
				//if ( !( renderTargetFlags & CREATERENDERTARGETFLAGS_NOEDRAM ) )
				{
					// RT surface is expected at end of array
					temp.surfaceHandle = g_pShaderAPI->CreateRenderTargetSurface( m_nActualWidth / iDownsizePow2, m_nActualHeight / iDownsizePow2, m_ImageFormat, RT_MULTISAMPLE_NONE, GetName(), TEXTURE_GROUP_RENDER_TARGET_SURFACE );
				}
#endif
			}
		}
		else
		{
			Assert( HasBeenAllocated() );
		}

		m_Targets.AddToTail( temp );
	}

	virtual void SetActiveSubTarget( const char *szName )
	{
		ICallQueue *pCallQueue = materials->GetRenderContext()->GetCallQueue();
		if ( pCallQueue )
		{
			m_nQueuedActiveTarget = -1;
			if( szName == NULL )
			{
				return;
			}

			char szCleanName[MAX_PATH];
			NormalizeTextureName( szName, szCleanName, sizeof( szCleanName ) );

			for( int i = 0; i != m_Targets.Count(); ++i )
			{
				if( m_Targets[i].name == szCleanName )
				{
					m_nQueuedActiveTarget = i;
					break;
				}
			}

			pCallQueue->QueueCall( this, &CTexture_MultipleRenderTarget::SetActiveSubTarget, szName );
			return;
		}
		else
		{
			m_nActiveTarget = -1;
			if( szName == NULL )
			{
				return;
			}

			char szCleanName[MAX_PATH];
			NormalizeTextureName( szName, szCleanName, sizeof( szCleanName ) );
			
			for( int i = 0; i != m_Targets.Count(); ++i )
			{
				if( m_Targets[i].name == szCleanName )
				{
					m_nActiveTarget = i;
					break;
				}
			}
		}
	}


	virtual int GetActualWidth() const
	{
		int iDownSize = 1;
		
		ICallQueue *pCallQueue = materials->GetRenderContext()->GetCallQueue();
		if ( pCallQueue )
		{
			Assert( m_nQueuedActiveTarget < m_Targets.Count() );
			if( (m_nQueuedActiveTarget >= 0) && (m_nQueuedActiveTarget < m_Targets.Count()) )
			{
				iDownSize = m_Targets[m_nQueuedActiveTarget].iDownSizePow2;
			}
		}
		else
		{
			Assert( m_nActiveTarget < m_Targets.Count() );
			if( (m_nActiveTarget >= 0) && (m_nActiveTarget < m_Targets.Count()) )
			{
				iDownSize = m_Targets[m_nActiveTarget].iDownSizePow2;
			}
		}

		return BaseClass::GetActualWidth() / iDownSize;
	}

	virtual int GetActualHeight() const
	{
		int iDownSize = 1;

		ICallQueue *pCallQueue = materials->GetRenderContext()->GetCallQueue();
		if ( pCallQueue )
		{
			Assert( m_nQueuedActiveTarget < m_Targets.Count() );
			if( (m_nQueuedActiveTarget >= 0) && (m_nQueuedActiveTarget < m_Targets.Count()) )
			{
				iDownSize = m_Targets[m_nQueuedActiveTarget].iDownSizePow2;
			}
		}
		else
		{
			Assert( m_nActiveTarget < m_Targets.Count() );
			if( (m_nActiveTarget >= 0) && (m_nActiveTarget < m_Targets.Count()) )
			{
				iDownSize = m_Targets[m_nActiveTarget].iDownSizePow2;
			}
		}

		return BaseClass::GetActualHeight() / iDownSize;
	}

	virtual bool AllocateShaderAPITextures()
	{
		Assert( materials->GetRenderContext()->GetCallQueue() == NULL );
		Assert( !HasBeenAllocated() );

		if ( !BaseClass::AllocateShaderAPITextures() )
			return false;

		if( m_Targets.Count() == 0 )
			return true;

		extern int GetCreationFlags( int iTextureFlags, int iInternalTextureFlags, ImageFormat fmt ); //defined about 1000 lines down where it makes more logical sense to be
		int nCreateFlags = GetCreationFlags( m_nFlags, m_nInternalFlags, m_ImageFormat );

		// For depth only render target: adjust texture width/height
		// Currently we just leave it the same size, will update with further testing
		int nShaderApiCreateTextureDepth = ( ( m_nFlags & TEXTUREFLAGS_DEPTHRENDERTARGET ) && ( m_nOriginalRenderTargetType == RENDER_TARGET_ONLY_DEPTH ) ) ? 1 : m_nActualDepth;

		for( int i = 0; i != m_Targets.Count(); ++i )
		{
			// Create all animated texture frames in a single call
			g_pShaderAPI->CreateTextures(
				&m_Targets[i].handle, 1,
				m_nActualWidth / m_Targets[i].iDownSizePow2, m_nActualHeight / m_Targets[i].iDownSizePow2, nShaderApiCreateTextureDepth, m_ImageFormat, m_nActualMipCount,
				1, nCreateFlags, GetName(), GetTextureGroupName() );

			// Create the depth render target buffer
			if ( m_Targets[i].bHasSeparateDepth )
			{
				MEM_ALLOC_CREDIT();

				char debugName[128];
				sprintf( debugName, "%s_ZBuffer", GetName() );

				m_Targets[i].depthHandle = g_pShaderAPI->CreateDepthTexture( 
					m_ImageFormat, 
					m_nActualWidth / m_Targets[i].iDownSizePow2, 
					m_nActualHeight / m_Targets[i].iDownSizePow2,
					debugName,
					( m_nOriginalRenderTargetType == RENDER_TARGET_ONLY_DEPTH ) );
			}


#if defined( PLATFORM_X360 )
			//if ( !( renderTargetFlags & CREATERENDERTARGETFLAGS_NOEDRAM ) )
			{
				// RT surface is expected at end of array
				m_Targets[i].surfaceHandle = g_pShaderAPI->CreateRenderTargetSurface( m_nActualWidth / m_Targets[i].iDownSizePow2, m_nActualHeight / m_Targets[i].iDownSizePow2, m_ImageFormat, RT_MULTISAMPLE_NONE, GetName(), TEXTURE_GROUP_RENDER_TARGET_SURFACE );
			}
#endif
		}		

		return true;
	}


	virtual void FreeShaderAPITextures()
	{
		Assert( materials->GetRenderContext()->GetCallQueue() == NULL );
		for( int i = 0; i != m_Targets.Count(); ++i )
		{
			if ( g_pShaderAPI->IsTexture( m_Targets[i].handle ) )
			{
				g_pShaderAPI->DeleteTexture( m_Targets[i].handle );
				m_Targets[i].handle = INVALID_SHADERAPI_TEXTURE_HANDLE;
			}

			if ( g_pShaderAPI->IsTexture( m_Targets[i].depthHandle ) )
			{
				g_pShaderAPI->DeleteTexture( m_Targets[i].depthHandle );
				m_Targets[i].depthHandle = INVALID_SHADERAPI_TEXTURE_HANDLE;
			}

#if defined( PLATFORM_X360 )
			if ( g_pShaderAPI->IsTexture( m_Targets[i].surfaceHandle ) )
			{
				g_pShaderAPI->DeleteTexture( m_Targets[i].surfaceHandle );
				m_Targets[i].surfaceHandle = INVALID_SHADERAPI_TEXTURE_HANDLE;
			}
#endif
		}

		BaseClass::FreeShaderAPITextures();
	}

	//-----------------------------------------------------------------------------
	// Set this texture as a render target
	//-----------------------------------------------------------------------------
	virtual bool SetRenderTarget( int nRenderTargetID )
	{
		Assert( materials->GetRenderContext()->GetCallQueue() == NULL );
		return SetRenderTarget( nRenderTargetID, NULL );
	}

	//-----------------------------------------------------------------------------
	// Set this texture as a render target
	// Optionally bind pDepthTexture as depth buffer
	//-----------------------------------------------------------------------------
	bool SetRenderTarget( int nRenderTargetID, ITexture *pDepthTexture )
	{
		Assert( materials->GetRenderContext()->GetCallQueue() == NULL );
		Assert( m_nActiveTarget < m_Targets.Count() );
		if( (m_nActiveTarget < 0) || (m_nActiveTarget >= m_Targets.Count()) )
			return BaseClass::SetRenderTarget( nRenderTargetID, pDepthTexture );

		if ( ( m_nFlags & TEXTUREFLAGS_RENDERTARGET ) == 0 )
			return false;

		// Make sure we've actually allocated the texture handles
		Assert( HasBeenAllocated() );

		ShaderAPITextureHandle_t textureHandle;
#if !defined( PLATFORM_X360 )
		{
			textureHandle = m_Targets[m_nActiveTarget].handle;
		}
#else
		{
			textureHandle = m_Targets[m_nActiveTarget].surfaceHandle;
		}
#endif
		ShaderAPITextureHandle_t depthTextureHandle = (unsigned int)SHADER_RENDERTARGET_DEPTHBUFFER;

		if ( m_Targets[m_nActiveTarget].bHasSeparateDepth )
		{
			depthTextureHandle = m_Targets[m_nActiveTarget].depthHandle;
		} 
		else if ( m_nFlags & TEXTUREFLAGS_NODEPTHBUFFER )
		{
			// GR - render target without depth buffer	
			depthTextureHandle = (unsigned int)SHADER_RENDERTARGET_NONE;
		}

		if ( pDepthTexture)
		{
			depthTextureHandle = static_cast<ITextureInternal *>(pDepthTexture)->GetTextureHandle(0);
		}

		g_pShaderAPI->SetRenderTargetEx( nRenderTargetID, textureHandle, depthTextureHandle );
		return true;
	}

	// Stretch blit the framebuffer into this texture.
	virtual void CopyFrameBufferToMe( int nRenderTargetID = 0, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL )
	{
		Assert( materials->GetRenderContext()->GetCallQueue() == NULL );
		Assert( m_nActiveTarget < m_Targets.Count() );
		if( (m_nActiveTarget < 0) || (m_nActiveTarget >= m_Targets.Count()) )
			return BaseClass::CopyFrameBufferToMe( nRenderTargetID, pSrcRect, pDstRect );

		Assert( m_pTextureHandles && m_nFrameCount >= 1 );

		if ( IsX360() &&
			( m_nInternalFlags & TEXTUREFLAGSINTERNAL_TEMPRENDERTARGET ) &&
			!HasBeenAllocated() )
		{
			//need to create the texture bits now
			//to avoid creating the texture bits previously, we simply skipped this step
			if ( !AllocateShaderAPITextures() )
				return;
		}

		if ( m_pTextureHandles && m_nFrameCount >= 1 )
		{
			g_pShaderAPI->CopyRenderTargetToTextureEx( m_Targets[m_nActiveTarget].handle, nRenderTargetID, pSrcRect, pDstRect );
		}
	}

	virtual bool IsMultiRenderTarget( void ) { return true; }

	struct SubTarget_t
	{
		CUtlSymbol name;
		int iDownSizePow2;
		ShaderAPITextureHandle_t handle;
		ShaderAPITextureHandle_t depthHandle;
#if defined( PLATFORM_X360 )
		ShaderAPITextureHandle_t surfaceHandle;
#endif
		bool bHasSeparateDepth;
	};

	CUtlVector< SubTarget_t > m_Targets;
	int m_nActiveTarget;
	int m_nQueuedActiveTarget;
};

//////////////////////////////////////////////////////////////////////////
//
// CReferenceToHandleTexture is a special implementation of ITexture
// to be used solely for binding the texture handle when rendering.
// It is used when a D3D texture handle is available, but should be used
// at a higher level of abstraction requiring an ITexture or ITextureInternal.
//
//////////////////////////////////////////////////////////////////////////
class CReferenceToHandleTexture : public ITextureInternal
{
public:
	CReferenceToHandleTexture();
	virtual ~CReferenceToHandleTexture();

	virtual const char *GetName( void ) const { return m_Name.String(); }
	const char *GetTextureGroupName( void ) const { return m_TextureGroupName.String(); }

	// Stats about the texture itself
	virtual ImageFormat GetImageFormat() const { return IMAGE_FORMAT_UNKNOWN; }
	virtual int GetMappingWidth() const { return 1; }
	virtual int GetMappingHeight() const { return 1; }
	virtual int GetActualWidth() const { return m_nActualWidth; }
	virtual int GetActualHeight() const { return m_nActualHeight; }
	virtual int GetNumAnimationFrames() const { return 1; }
	virtual bool IsTranslucent() const { return false; }
	virtual void GetReflectivity( Vector& reflectivity ) { reflectivity.Zero(); }

	// Reference counting
	virtual void IncrementReferenceCount( ) { ++ m_nRefCount; }
	virtual void DecrementReferenceCount( ) { -- m_nRefCount; }
	virtual int GetReferenceCount( ) const { return m_nRefCount; }

	// Used to modify the texture bits (procedural textures only)
	virtual void SetTextureRegenerator( ITextureRegenerator *pTextureRegen, bool releaseExisting = true ) { NULL; }

	// Little helper polling methods
	virtual bool IsNormalMap( ) const { return false; }
	virtual bool IsCubeMap( void ) const { return false; }
	virtual bool IsRenderTarget( ) const { return false; }
	virtual bool IsTempRenderTarget( void ) const { return false; }
	virtual bool IsProcedural() const { return true; }
	virtual bool IsMipmapped() const { return false; }
	virtual bool IsError() const { return false; }
	virtual bool IsDefaultPool() const { return false; }

	// For volume textures
	virtual bool IsVolumeTexture() const { return false; }
	virtual int GetMappingDepth() const { return 1; }
	virtual int GetActualDepth() const { return 1; }

	// Releases the texture's hw memory
	void Release() { NULL; }

	virtual void OnRestore() { NULL; }

	// Sets the filtering modes on the texture we're modifying
	void SetFilteringAndClampingMode() { NULL; }
	void Download( Rect_t *pRect = NULL, int nAdditionalCreationFlags = 0 ) { NULL; }

	// Loads up information about the texture 
	virtual void Precache() { NULL; }

	// FIXME: Bogus methods... can we please delete these?
	virtual void GetLowResColorSample( float s, float t, float *color ) const { NULL; }

	// Gets texture resource data of the specified type.
	// Params:
	//		eDataType		type of resource to retrieve.
	//		pnumBytes		on return is the number of bytes available in the read-only data buffer or is undefined
	// Returns:
	//		pointer to the resource data, or NULL. Note that the data from this pointer can disappear when
	// the texture goes away - you want to copy this data!
	virtual void *GetResourceData( uint32 eDataType, size_t *pNumBytes ) const { return NULL; }

	virtual int GetApproximateVidMemBytes( void ) const { return 32; }

	// Stretch blit the framebuffer into this texture.
	virtual void CopyFrameBufferToMe( int nRenderTargetID = 0, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL ) { NULL; }
	virtual void CopyMeToFrameBuffer( int nRenderTargetID = 0, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL ) { NULL; }

	virtual ITexture *GetEmbeddedTexture( int nIndex ) { return ( nIndex == 0 ) ? this : NULL; }

	// Get the shaderapi texture handle associated w/ a particular frame
	virtual ShaderAPITextureHandle_t GetTextureHandle( int nFrame, int nTextureChannel = 0 ) { return m_hTexture; }

	// Bind the texture
	virtual void Bind( Sampler_t sampler, TextureBindFlags_t nBindFlags );
	virtual void Bind( Sampler_t sampler1, TextureBindFlags_t nBindFlags, int nFrame, Sampler_t sampler2 = SHADER_SAMPLER_INVALID );
	virtual void BindVertexTexture( VertexTextureSampler_t stage, int nFrame );

	// Set this texture as a render target	
	bool SetRenderTarget( int nRenderTargetID ) { return SetRenderTarget( nRenderTargetID, NULL ); }

	// Set this texture as a render target (optionally set depth texture as depth buffer as well)
	bool SetRenderTarget( int nRenderTargetID, ITexture *pDepthTexture) { return false; }

	virtual void MarkAsPreloaded( bool bSet ) { NULL; }
	virtual bool IsPreloaded() const { return true; }

	virtual void MarkAsExcluded( bool bSet, int nDimensionsLimit, bool bMarkAsTrumpedExclude ) { NULL; }
	virtual bool UpdateExcludedState() { return true; }

	// Retrieve the vtf flags mask
	virtual unsigned int GetFlags( void ) const { return 0; }

	virtual void ForceLODOverride( int iNumLodsOverrideUpOrDown ) { NULL; }
	virtual void ForceExcludeOverride( int iExcludeOverride ) { NULL; };

	virtual void ReloadFilesInList( IFileList *pFilesToReload ) {}

#ifdef _PS3
	virtual void Ps3gcmRawBufferAlias( char const *pRTName ) {}
#endif

	virtual void AddDownsizedSubTarget( const char *szName, int iDownsizePow2, MaterialRenderTargetDepth_t depth ) { NULL; }
	virtual void SetActiveSubTarget( const char *szName ) { NULL; }
	virtual bool IsMultiRenderTarget( void ) { return false; }

	virtual bool MarkAsTempExcluded( bool bSet, int nExcludedDimensionLimit ) { return false; }

	virtual bool IsTempExcluded() const { return false; }
	virtual bool CanBeTempExcluded() const { return false; }

	virtual bool FinishAsyncDownload( AsyncTextureContext_t *pContext, void *pData, int nNumReadBytes, bool bAbort, float flMaxTimeMs ) { return true; }

	virtual bool IsForceExcluded() const { return false; }
	virtual bool ClearForceExclusion() { return false; }

	virtual bool IsAsyncDone() const { return true; }

protected:
#ifdef _DEBUG
	char *m_pDebugName;
#endif

	CUtlSymbol m_Name;

	// What texture group this texture is in (winds up setting counters based on the group name,
	// then the budget panel views the counters).
	CUtlSymbol m_TextureGroupName;

	// The set of texture ids for each animation frame
	ShaderAPITextureHandle_t m_hTexture;

	// Refcount
	int m_nRefCount;

	int m_nActualWidth;
	int m_nActualHeight;
	int m_nActualDepth;

public:
	virtual void DeleteIfUnreferenced();

#if defined( _GAMECONSOLE )
	virtual bool ClearTexture( int r, int g, int b, int a ) { return false; }
#endif

#if defined( _X360 )	
	virtual bool CreateRenderTargetSurface( int width, int height, ImageFormat format, bool bSameAsTexture, RTMultiSampleCount360_t multiSampleCount = RT_MULTISAMPLE_NONE ) { return false; }
#endif

	void FixupTexture( const void *pData, int nSize, LoaderError_t loaderError ) { NULL; }

	void SwapContents( ITexture *pOther ) { NULL; }

public:
	void SetName( char const *szName );
	void InitFromHandle(
		const char *pTextureName,
		const char *pTextureGroupName,
		ShaderAPITextureHandle_t hTexture );
};

CReferenceToHandleTexture::CReferenceToHandleTexture() :
	m_hTexture( INVALID_SHADERAPI_TEXTURE_HANDLE ),
#ifdef _DEBUG
	m_pDebugName( NULL ),
#endif
	m_nRefCount( 0 ),
	m_nActualWidth( 0 ),
	m_nActualHeight( 0 ),
	m_nActualDepth( 1 )
{
	NULL;
}

CReferenceToHandleTexture::~CReferenceToHandleTexture()
{
#ifdef _DEBUG
	if ( m_nRefCount != 0 )
	{
		Warning( "Reference Count(%d) != 0 in ~CReferenceToHandleTexture for texture \"%s\"\n", m_nRefCount, m_Name.String() );
	}
	if ( m_pDebugName )
	{
		delete [] m_pDebugName;
	}
#endif
}

void CReferenceToHandleTexture::SetName( char const *szName )
{
	// normalize and convert to a symbol
	char szCleanName[MAX_PATH];
	m_Name = NormalizeTextureName( szName, szCleanName, sizeof( szCleanName ) );

#ifdef _DEBUG
	if ( m_pDebugName )
	{
		delete [] m_pDebugName;
	}
	int nLen = V_strlen( szCleanName ) + 1;
	m_pDebugName = new char[nLen];
	V_memcpy( m_pDebugName, szCleanName, nLen );
#endif
}

void CReferenceToHandleTexture::InitFromHandle( const char *pTextureName, const char *pTextureGroupName, ShaderAPITextureHandle_t hTexture )
{
	SetName( pTextureName );
	m_TextureGroupName = pTextureGroupName;
	m_hTexture = hTexture;
	g_pShaderAPI->GetTextureDimensions( hTexture, m_nActualWidth, m_nActualHeight, m_nActualDepth );
}

void CReferenceToHandleTexture::Bind( Sampler_t sampler, TextureBindFlags_t nBindFlags )
{
	if ( g_pShaderDevice->IsUsingGraphics() )
	{
		g_pShaderAPI->BindTexture( sampler, nBindFlags, m_hTexture );
	}
}

void CReferenceToHandleTexture::Bind( Sampler_t sampler1, TextureBindFlags_t nBindFlags, int nFrame, Sampler_t sampler2 /* = -1 */ )
{
	if ( g_pShaderDevice->IsUsingGraphics() )
	{
		g_pShaderAPI->BindTexture( sampler1, nBindFlags, m_hTexture );
	}
}


void CReferenceToHandleTexture::BindVertexTexture( VertexTextureSampler_t sampler, int nFrame )
{
	if ( g_pShaderDevice->IsUsingGraphics() )
	{
		g_pShaderAPI->BindVertexTexture( sampler, m_hTexture );
	}
}

void CReferenceToHandleTexture::DeleteIfUnreferenced()
{
	if ( m_nRefCount > 0 )
		return;

	TextureManager()->RemoveTexture( this );
}


//-----------------------------------------------------------------------------
// Fixed-size allocator
//-----------------------------------------------------------------------------
//DEFINE_FIXEDSIZE_ALLOCATOR( CTexture, 1024, true );


//-----------------------------------------------------------------------------
// Static instance of VTF texture
//-----------------------------------------------------------------------------
static IVTFTexture *s_pVTFTexture = NULL;
static IVTFTexture *s_pVTFAsyncTexture = NULL;

static void *s_pOptimalReadBuffer = NULL;
static int s_nOptimalReadBufferSize = 0;

//-----------------------------------------------------------------------------
// Class factory methods
//-----------------------------------------------------------------------------
ITextureInternal *ITextureInternal::CreateFileTexture( const char *pFileName, const char *pTextureGroupName )
{
	CTexture *pTex = new CTexture;
	pTex->InitFileTexture( pFileName, pTextureGroupName );
	return pTex;
}

ITextureInternal *ITextureInternal::CreateReferenceTextureFromHandle(
	const char *pTextureName,
	const char *pTextureGroupName,
	ShaderAPITextureHandle_t hTexture )
{
	CReferenceToHandleTexture *pTex = new CReferenceToHandleTexture;
	pTex->InitFromHandle( pTextureName, pTextureGroupName, hTexture );
	return pTex;
}

ITextureInternal *ITextureInternal::CreateProceduralTexture( 
	const char			*pTextureName, 
	const char			*pTextureGroupName, 
	int					w, 
	int					h, 
	int					d,
	ImageFormat			fmt, 
	int					nFlags )
{
	CTexture *pTex = new CTexture;
	pTex->InitProceduralTexture( pTextureName, pTextureGroupName, w, h, d, fmt, nFlags );
	pTex->IncrementReferenceCount();
	return pTex;
}

// GR - named RT
ITextureInternal *ITextureInternal::CreateRenderTarget( 
	const char *pRTName, 
	int w, 
	int h, 
	RenderTargetSizeMode_t sizeMode, 
	ImageFormat fmt, 
	RenderTargetType_t type, 
	unsigned int textureFlags, 
	unsigned int renderTargetFlags,
	bool bMultipleTargets )
{
	CTexture *pTex = bMultipleTargets ? new CTexture_MultipleRenderTarget : new CTexture;
	pTex->InitRenderTarget( pRTName, w, h, sizeMode, fmt, type, textureFlags, renderTargetFlags );

	return pTex;
}

//-----------------------------------------------------------------------------
// Rebuild and exisiting render target in place.
//-----------------------------------------------------------------------------
void ITextureInternal::ChangeRenderTarget( 
	ITextureInternal *pTex,
	int w,
	int	h,
	RenderTargetSizeMode_t sizeMode, 
	ImageFormat fmt, 
	RenderTargetType_t type, 
	unsigned int textureFlags, 
	unsigned int renderTargetFlags )
{
	pTex->Release();
	dynamic_cast< CTexture * >(pTex)->InitRenderTarget( pTex->GetName(), w, h, sizeMode, fmt, type, textureFlags, renderTargetFlags );
}

void ITextureInternal::Destroy( ITextureInternal *pTex )
{
	delete pTex;
}

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CTexture::CTexture() : m_ImageFormat( IMAGE_FORMAT_UNKNOWN )
{
	m_nActualMipCount = 0;
	m_nMappingWidth = 0;
	m_nMappingHeight = 0;
	m_nMappingDepth = 1;
	m_nActualWidth = 0;
	m_nActualHeight = 0;
	m_nActualDepth = 1;
	m_nRefCount = 0;
	m_nFlags = 0;
	m_nInternalFlags = 0;
	m_pTextureHandles = NULL;
	m_pTempTextureHandles = NULL;
	m_nFrameCount = 0;
	VectorClear( m_vecReflectivity );
	m_pTextureRegenerator = NULL;
	m_nOriginalRenderTargetType = NO_RENDER_TARGET;
	m_RenderTargetSizeMode = RT_SIZE_NO_CHANGE;
	m_nOriginalRTWidth = m_nOriginalRTHeight = 1;

	m_LowResImageWidth = 0;
	m_LowResImageHeight = 0;
#if !defined( _GAMECONSOLE )
	m_pLowResImage = NULL;
#else
	*(unsigned int *)m_LowResImageSample = 0;
#endif

	m_nDesiredDimensionLimit = 0;
	m_nDesiredTempDimensionLimit = 0;
	m_nActualDimensionLimit = 0;	
	m_hAsyncControl = NULL;

	m_nMipSkipCount = 0;

#ifdef _DEBUG
	m_pDebugName = NULL;
#endif
}

CTexture::~CTexture()
{
#ifdef _DEBUG
	if ( m_nRefCount != 0 )
	{
		Warning( "Reference Count(%d) != 0 in ~CTexture for texture \"%s\"\n", (int)m_nRefCount, m_Name.String() );
	}
	if ( m_pDebugName )
	{
		delete [] m_pDebugName;
	}
#endif

	Shutdown();

	// Deliberately stomp our VTable so that we can detect cases where code tries to access freed materials.
	int *p = (int *)this;
	*p = 0xdeadbeef;
}


//-----------------------------------------------------------------------------
// Initializes the texture
//-----------------------------------------------------------------------------
void CTexture::Init( int w, int h, int d, ImageFormat fmt, int iFlags, int iFrameCount )
{
	Assert( iFrameCount > 0 );

	// This is necessary to prevent blowing away the allocated state,
	// which is necessary for the ReleaseTextureHandles call below to work.
	m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_ERROR;

	// free and release previous data
	// cannot change to new intialization parameters yet
	FreeShaderAPITextures();
	ReleaseTextureHandles();

	// update to new initialization parameters
	// these are the *desired* new values
	m_nMappingWidth = w;
	m_nMappingHeight = h;
	m_nMappingDepth = d;
	m_ImageFormat = fmt;
	m_nFrameCount = iFrameCount;
	// We don't know the actual width and height until we get it ready to render
	m_nActualWidth = m_nActualHeight = 0;
	m_nActualDepth = 1;
	m_nActualMipCount = 0;
	m_nFlags = iFlags;
	m_nMipSkipCount = 0;

	AllocateTextureHandles();
}


//-----------------------------------------------------------------------------
// Shuts down the texture
//-----------------------------------------------------------------------------
void CTexture::Shutdown()
{
	// Clean up the low-res texture
#if !defined( _GAMECONSOLE )
	delete[] m_pLowResImage;
	m_pLowResImage = 0;
#endif

	FreeResourceData();

	// Frees the texture regen class
	if ( m_pTextureRegenerator )
	{
		m_pTextureRegenerator->Release();
		m_pTextureRegenerator = NULL;
	}

	// This deletes the textures
	FreeShaderAPITextures();
	ReleaseTextureHandles();
}

void CTexture::Release()
{
	FreeShaderAPITextures();
}

IVTFTexture *CTexture::GetScratchVTFTexture()
{
	if ( !s_pVTFTexture )
	{
		s_pVTFTexture = CreateVTFTexture();
	}
	return s_pVTFTexture;
}

IVTFTexture *CTexture::GetScratchVTFAsyncTexture()
{
	if (!s_pVTFAsyncTexture)
	{
		s_pVTFAsyncTexture = CreateVTFTexture();
	}
	return s_pVTFAsyncTexture;
}

//-----------------------------------------------------------------------------
// Get an optimal read buffer, persists and avoids excessive allocations
//-----------------------------------------------------------------------------
int CTexture::GetOptimalReadBuffer( FileHandle_t hFile, int nSize, CUtlBuffer &optimalBuffer )
{
	// get an optimal read buffer, only resize if necessary
	int minSize = IsGameConsole() ? 0 : 2 * 1024 * 1024;	// 360 has no min, PC uses 2MB min to avoid fragmentation
	nSize = MAX(nSize, minSize);
	int nBytesOptimalRead = g_pFullFileSystem->GetOptimalReadSize( hFile, nSize );
	if ( nBytesOptimalRead > s_nOptimalReadBufferSize )
	{
		FreeOptimalReadBuffer( 0 );

		s_nOptimalReadBufferSize = nBytesOptimalRead;
		s_pOptimalReadBuffer = g_pFullFileSystem->AllocOptimalReadBuffer( hFile, nSize );
		if ( mat_spewalloc.GetBool() )
		{
			Msg( "Allocated optimal read buffer of %d bytes @ 0x%p\n", s_nOptimalReadBufferSize, s_pOptimalReadBuffer );
		}
	}

	// set external buffer and reset to empty
	optimalBuffer.SetExternalBuffer( s_pOptimalReadBuffer, s_nOptimalReadBufferSize, 0, CUtlBuffer::READ_ONLY );

	// return the optimal read size
	return nBytesOptimalRead;
}

//-----------------------------------------------------------------------------
// Free the optimal read buffer if it grows too large
//-----------------------------------------------------------------------------
void CTexture::FreeOptimalReadBuffer( int nMaxSize )
{
	if ( s_pOptimalReadBuffer && s_nOptimalReadBufferSize >= nMaxSize )
	{
		if ( mat_spewalloc.GetBool() )
		{
			Msg( "Freeing optimal read buffer of %d bytes @ 0x%p\n", s_nOptimalReadBufferSize, s_pOptimalReadBuffer );
		}
		g_pFullFileSystem->FreeOptimalReadBuffer( s_pOptimalReadBuffer );
		s_pOptimalReadBuffer = NULL;
		s_nOptimalReadBufferSize = 0;
	}
}
//-----------------------------------------------------------------------------
//
// Various initialization methods
//
//-----------------------------------------------------------------------------


void CTexture::ApplyRenderTargetSizeMode( int &width, int &height, ImageFormat fmt )
{
	width = m_nOriginalRTWidth;
	height = m_nOriginalRTHeight;

	switch ( m_RenderTargetSizeMode )
	{
		case RT_SIZE_FULL_FRAME_BUFFER:
		{
			MaterialSystem()->GetBackBufferDimensions( width, height );
		}
		break;

		case RT_SIZE_FULL_FRAME_BUFFER_ROUNDED_UP:
		{
			MaterialSystem()->GetBackBufferDimensions( width, height );
		}
		break;

		case RT_SIZE_PICMIP:
		{
			int fbWidth, fbHeight;
			MaterialSystem()->GetBackBufferDimensions( fbWidth, fbHeight );
			int picmip = g_config.skipMipLevels;
			while( picmip > 0 )
			{
				width >>= 1;
				height >>= 1;
				picmip--;
			}

			while( width > fbWidth )
			{
				width >>= 1;
			}
			while( height > fbHeight )
			{
				height >>= 1;
			}
		}
		break;

		case RT_SIZE_DEFAULT:
		{
			// Assume that the input is pow2.
			Assert( ( width & ( width - 1 ) ) == 0 );
			Assert( ( height & ( height - 1 ) ) == 0 );
			int fbWidth, fbHeight;
			MaterialSystem()->GetBackBufferDimensions( fbWidth, fbHeight );
			while( width > fbWidth )
			{
				width >>= 1;
			}
			while( height > fbHeight )
			{
				height >>= 1;
			}
		}
		break;

		case RT_SIZE_HDR:
		{
			MaterialSystem()->GetBackBufferDimensions( width, height );
			width = width / 4;
			height = height / 4;
		}
		break;

		case RT_SIZE_OFFSCREEN:
		{
			int fbWidth, fbHeight;
			MaterialSystem()->GetBackBufferDimensions( fbWidth, fbHeight );

			// On 360, don't do this resizing for formats related to the shadow depth texture
#if defined( _GAMECONSOLE )
			if ( !( (fmt == IMAGE_FORMAT_D16) || (fmt == IMAGE_FORMAT_D24S8) || (fmt == IMAGE_FORMAT_D24FS8) || (fmt == IMAGE_FORMAT_BGR565) || (fmt == IMAGE_FORMAT_D24X8_SHADOW) || (fmt == IMAGE_FORMAT_D16_SHADOW) ) )
#endif
			{
				// Shrink the buffer if it's bigger than back buffer.  Otherwise, don't mess with it.
				while( (width > fbWidth) || (height > fbHeight) )
				{
					width >>= 1;
					height >>= 1;
				}
			}

		}
		break;

		default:
		{
			Assert( m_RenderTargetSizeMode == RT_SIZE_NO_CHANGE );
			
			// Cannot use RT_SIZE_NO_CHANGE if they are sharing the depth buffer.
			Assert( m_nOriginalRenderTargetType != RENDER_TARGET );
		}
		break;
	}
}



//-----------------------------------------------------------------------------
// Creates named render target texture
//-----------------------------------------------------------------------------
void CTexture::InitRenderTarget( 
	const char *pRTName, 
	int w, 
	int h, 
	RenderTargetSizeMode_t sizeMode, 
	ImageFormat fmt, 
	RenderTargetType_t type, 
	unsigned int textureFlags,
	unsigned int renderTargetFlags )
{
	if ( pRTName )
	{
		SetName( pRTName );
	}
	else
	{
		static int id = 0;
		char pName[128];
		Q_snprintf( pName, sizeof( pName ), "__render_target_%d", id );
		++id;
		SetName( pName );
	}

	if ( renderTargetFlags & CREATERENDERTARGETFLAGS_HDR )
	{
		if ( HardwareConfig()->GetHDRType() == HDR_TYPE_FLOAT )
		{
			// slam the format
			fmt = IMAGE_FORMAT_RGBA16161616F;
		}
	}

	int nFrameCount = 1;

	int nFlags = TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_RENDERTARGET;
	nFlags |= textureFlags;

	if ( type == RENDER_TARGET_NO_DEPTH )
	{
		nFlags |= TEXTUREFLAGS_NODEPTHBUFFER;
	}
	else if ( type == RENDER_TARGET_WITH_DEPTH || type == RENDER_TARGET_ONLY_DEPTH || g_pShaderAPI->DoRenderTargetsNeedSeparateDepthBuffer() ) 
	{
		nFlags |= TEXTUREFLAGS_DEPTHRENDERTARGET;
		++nFrameCount;
	}

	if ( IsX360() )
	{
		// 360 RT needs its coupled surface, expected at [nFrameCount-1]
		++nFrameCount;
	}

	if ( renderTargetFlags & CREATERENDERTARGETFLAGS_TEMP )
	{
		m_nInternalFlags |= TEXTUREFLAGSINTERNAL_TEMPRENDERTARGET;
	}

	m_nOriginalRenderTargetType = type;
	m_RenderTargetSizeMode = sizeMode;
	m_nOriginalRTWidth = w;
	m_nOriginalRTHeight = h;

	if ( ImageLoader::ImageFormatInfo(fmt).m_nNumAlphaBits > 1 )
	{
		nFlags  |= TEXTUREFLAGS_EIGHTBITALPHA;
	}
	else if ( ImageLoader::ImageFormatInfo(fmt).m_nNumAlphaBits == 1 )
	{
		nFlags  |= TEXTUREFLAGS_ONEBITALPHA;
	}

#ifdef _X360
	if ( renderTargetFlags & CREATERENDERTARGETFLAGS_ALIASCOLORANDDEPTHSURFACES )
	{
		nFlags |= TEXTUREFLAGS_ALIAS_COLOR_AND_DEPTH_SURFACES;
	}
#endif

	ApplyRenderTargetSizeMode( w, h, fmt );

	Init( w, h, 1, fmt, nFlags, nFrameCount );
	m_TextureGroupName = TEXTURE_GROUP_RENDER_TARGET;
}


void CTexture::OnRestore()
{ 
	// May have to change whether or not we have a depth buffer.
	// Are we a render target?
	if ( IsPC() && ( m_nFlags & TEXTUREFLAGS_RENDERTARGET ) )
	{
		// Did they not ask for a depth buffer?
		if ( m_nOriginalRenderTargetType == RENDER_TARGET )
		{
			// But, did we force them to have one, or should we force them to have one this time around?
			bool bShouldForce = g_pShaderAPI->DoRenderTargetsNeedSeparateDepthBuffer();
			bool bDidForce = ((m_nFlags & TEXTUREFLAGS_DEPTHRENDERTARGET) != 0);
			if ( bShouldForce != bDidForce )
			{
				int nFlags = m_nFlags;
				int iFrameCount = m_nFrameCount;
				if ( bShouldForce )
				{
					Assert( !( nFlags & TEXTUREFLAGS_DEPTHRENDERTARGET ) );
					iFrameCount = 2;
					nFlags |= TEXTUREFLAGS_DEPTHRENDERTARGET;
				}
				else
				{
					Assert( ( nFlags & TEXTUREFLAGS_DEPTHRENDERTARGET ) );
					iFrameCount = 1;
					nFlags &= ~TEXTUREFLAGS_DEPTHRENDERTARGET;
				}

				Shutdown();
				
				int newWidth, newHeight;
				ApplyRenderTargetSizeMode( newWidth, newHeight, m_ImageFormat );
				
				Init( newWidth, newHeight, 1, m_ImageFormat, nFlags, iFrameCount );
				return;
			}
		}

		// If we didn't recreate it up above, then we may need to resize it anyway if the framebuffer
		// got smaller than we are.
		int newWidth, newHeight;
		ApplyRenderTargetSizeMode( newWidth, newHeight, m_ImageFormat );
		if ( newWidth != m_nMappingWidth || newHeight != m_nMappingHeight )
		{
			Shutdown();
			Init( newWidth, newHeight, 1, m_ImageFormat, m_nFlags, m_nFrameCount );
			return;
		}
	}
}


//-----------------------------------------------------------------------------
// Creates a procedural texture
//-----------------------------------------------------------------------------
void CTexture::InitProceduralTexture( const char *pTextureName, const char *pTextureGroupName, int w, int h, int d, ImageFormat fmt, int nFlags )
{
	// Compressed textures aren't allowed for procedural textures, except the runtime ones
	Assert( !ImageLoader::IsCompressed( fmt ) || ImageLoader::IsRuntimeCompressed( fmt ) );

	// We shouldn't be asking for render targets here
	Assert( (nFlags & (TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_DEPTHRENDERTARGET)) == 0 );

	SetName( pTextureName );

	// Eliminate flags that are inappropriate...
	nFlags &= ~TEXTUREFLAGS_HINT_DXT5 | TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA | 
		TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_DEPTHRENDERTARGET;

	// Insert required flags
	nFlags |= TEXTUREFLAGS_PROCEDURAL;
	int nAlphaBits = ImageLoader::ImageFormatInfo(fmt).m_nNumAlphaBits;
	if (nAlphaBits > 1)
	{
		nFlags |= TEXTUREFLAGS_EIGHTBITALPHA;
	}
	else if (nAlphaBits == 1)
	{
		nFlags |= TEXTUREFLAGS_ONEBITALPHA;
	}
	
	// Procedural textures are always one frame only
	Init( w, h, d, fmt, nFlags, 1 );

	m_TextureGroupName = pTextureGroupName;
}


//-----------------------------------------------------------------------------
// Creates a file texture
//-----------------------------------------------------------------------------
void CTexture::InitFileTexture( const char *pTextureFile, const char *pTextureGroupName )
{
	// For files, we only really know about the file name
	// At any time, the file contents could change, and we could have
	// a different size, number of frames, etc.
	SetName( pTextureFile );
	m_TextureGroupName = pTextureGroupName;
}

//-----------------------------------------------------------------------------
// Assigns/releases texture IDs for our animation frames
//-----------------------------------------------------------------------------
void CTexture::AllocateTextureHandles()
{
	Assert( !m_pTextureHandles );

	if ( m_nFrameCount <= 0 )
	{
		AssertMsg( false, "CTexture::AllocateTextureHandles attempted to allocate 0 frames of texture handles!" );
		Warning( "CTexture::AllocateTextureHandles \"%s\" attempted to allocate 0 frames of texture handles!", GetName() );
		m_nFrameCount = 1;
	}

	m_pTextureHandles = new ShaderAPITextureHandle_t[m_nFrameCount];

	if ( m_pTextureHandles == NULL )
	{
		MemOutOfMemory( sizeof(ShaderAPITextureHandle_t) * m_nFrameCount );
	}
	else
	{
		for( int i = 0; i != m_nFrameCount; ++i )
		{
			m_pTextureHandles[i] = INVALID_SHADERAPI_TEXTURE_HANDLE;
		}
	}
}

void CTexture::ReleaseTextureHandles()
{
	if ( m_pTextureHandles )
	{
		delete[] m_pTextureHandles;
		m_pTextureHandles = NULL;
	}
}

int GetCreationFlags( int iTextureFlags, int iInternalTextureFlags, ImageFormat fmt )
{
	int nCreateFlags = 0;
	if ( iTextureFlags & TEXTUREFLAGS_ENVMAP )
	{
		nCreateFlags |= TEXTURE_CREATE_CUBEMAP;
	}

	bool bIsFloat = ( fmt == IMAGE_FORMAT_RGBA16161616F ) || ( fmt == IMAGE_FORMAT_R32F ) || 
					( fmt == IMAGE_FORMAT_RGB323232F ) || ( fmt == IMAGE_FORMAT_RGBA32323232F );
	
	// Don't do sRGB on floating point textures
	if ( ( iTextureFlags & TEXTUREFLAGS_SRGB ) && !bIsFloat )
	{
		nCreateFlags |= TEXTURE_CREATE_SRGB;	// for Posix/GL only
	}
	
	if ( iTextureFlags & TEXTUREFLAGS_ANISOTROPIC )
	{
		nCreateFlags |= TEXTURE_CREATE_ANISOTROPIC;	// for Posix/GL only
	}
	

	if ( iTextureFlags & TEXTUREFLAGS_RENDERTARGET )
	{
		nCreateFlags |= TEXTURE_CREATE_RENDERTARGET;
	}
	else
	{
		// If it's not a render target, use the texture manager in dx
		nCreateFlags |= TEXTURE_CREATE_MANAGED;
	}

	if ( iTextureFlags & TEXTUREFLAGS_DEFAULT_POOL ) 
	{
		// Needs to be created in default pool, and be marked as dynamic.
		nCreateFlags &= ~TEXTURE_CREATE_MANAGED;
		nCreateFlags |= TEXTURE_CREATE_DYNAMIC;
	}

	if ( iTextureFlags & TEXTUREFLAGS_POINTSAMPLE )
	{
		nCreateFlags |= TEXTURE_CREATE_UNFILTERABLE_OK;
	}

	if ( iTextureFlags & TEXTUREFLAGS_VERTEXTEXTURE )
	{
		nCreateFlags |= TEXTURE_CREATE_VERTEXTEXTURE;
	}

	if ( IsGameConsole() )
	{
		if ( iInternalTextureFlags & TEXTUREFLAGSINTERNAL_QUEUEDLOAD )
		{
			// queued load, no d3d bits until data arrival
			nCreateFlags |= TEXTURE_CREATE_NOD3DMEMORY;
		}
		if ( iInternalTextureFlags & TEXTUREFLAGSINTERNAL_REDUCED )
		{
			// propagate this information
			nCreateFlags |= TEXTURE_CREATE_REDUCED;
		}
		if ( iInternalTextureFlags & TEXTUREFLAGSINTERNAL_ERROR )
		{
			// propagate this information
			nCreateFlags |= TEXTURE_CREATE_ERROR;
		}
		if ( iInternalTextureFlags & ( TEXTUREFLAGSINTERNAL_SHOULDEXCLUDE | TEXTUREFLAGSINTERNAL_SHOULDTEMPEXCLUDE ) )
		{
			// propagate this information
			nCreateFlags |= TEXTURE_CREATE_EXCLUDED;
		}

		if ( IsPS3() )
		{
			if ( iTextureFlags & TEXTUREFLAGS_PROCEDURAL )
			{
				nCreateFlags |= TEXTURE_CREATE_DYNAMIC;
			}
		}

		if ( IsX360() )
		{
			if ( iTextureFlags & TEXTUREFLAGS_PROCEDURAL )
			{
				nCreateFlags |= TEXTURE_CREATE_CANCONVERTFORMAT;
			}
			if ( iTextureFlags & TEXTUREFLAGS_PWL_CORRECTED )
			{
				nCreateFlags |= TEXTURE_CREATE_PWLCORRECTED;
			}
			if ( iInternalTextureFlags & TEXTUREFLAGSINTERNAL_CACHEABLE )
			{
				nCreateFlags |= TEXTURE_CREATE_CACHEABLE;
			}
		}
	}

	return nCreateFlags;
}


//-----------------------------------------------------------------------------
// Creates the texture
//-----------------------------------------------------------------------------
bool CTexture::AllocateShaderAPITextures()
{
	Assert( !HasBeenAllocated() );

	if ( !g_pShaderAPI->CanDownloadTextures() )
		return false;

	int nCreateFlags = GetCreationFlags( m_nFlags, m_nInternalFlags, m_ImageFormat );

	int nCount = m_nFrameCount;
	if ( m_nFlags & TEXTUREFLAGS_RENDERTARGET )
	{
		// This here is simply so we can use a different call to
		// create the depth texture below	
		// nCount must be 2 on pc/ps3, must be 3 on 360
		if ( ( m_nFlags & TEXTUREFLAGS_DEPTHRENDERTARGET ) &&
			 ( ( ( IsPC() || IsPS3() ) && (nCount == 2)) || (IsX360() && (nCount == 3)) ) )
		{
			--nCount;
		}
	}
	

	int nCopies = 1;
	if ( IsProcedural() )
	{
		// This is sort of hacky... should we store the # of copies in the VTF?
		if ( !( m_nFlags & TEXTUREFLAGS_SINGLECOPY ) )
		{
			// FIXME: That 6 there is heuristically what I came up with what I
			// need to get eyes not to stall on map alyx3. We need a better way
			// of determining how many copies of the texture we should store.
			nCopies = 6;
		}
	}

	if ( IsGameConsole() )
	{
		if ( IsX360() && ( m_nFlags & TEXTUREFLAGS_RENDERTARGET ) )
		{
			// 360 render targets allocates one additional handle for optional EDRAM surface
			--nCount;
			m_pTextureHandles[m_nFrameCount - 1] = INVALID_SHADERAPI_TEXTURE_HANDLE; 
		}

		if ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_QUEUEDLOAD )
		{
			// Artificially increment reference count (per frame) to ensure
			// a queued texture stays resident until it's wholly finalized.
			m_nRefCount += nCount;
		}
	}	
								   
	// For depth only render target: adjust texture width/height
	// Currently we just leave it the same size, will update with further testing
	int nShaderApiCreateTextureDepth = ( ( m_nFlags & TEXTUREFLAGS_DEPTHRENDERTARGET ) && ( m_nOriginalRenderTargetType == RENDER_TARGET_ONLY_DEPTH ) ) ? 1 : m_nActualDepth;

	if ( m_pTempTextureHandles )
	{
		// send the prior handles (should be available) for expected reuse
		nCreateFlags |= TEXTURE_CREATE_REUSEHANDLES;
		for ( int i = 0; i < m_nFrameCount; i++ )
		{
			m_pTextureHandles[i] = m_pTempTextureHandles[i];
		}
	}

	// Create all animated texture frames in a single call
	g_pShaderAPI->CreateTextures(
		m_pTextureHandles, nCount,
		m_nActualWidth, m_nActualHeight, nShaderApiCreateTextureDepth, m_ImageFormat, m_nActualMipCount,
		nCopies, nCreateFlags, GetName(), GetTextureGroupName() );

	// Create the depth render target buffer
	if ( m_nFlags & TEXTUREFLAGS_DEPTHRENDERTARGET )
	{
		MEM_ALLOC_CREDIT();
		Assert( nCount == 1 );

		char debugName[128];
		sprintf( debugName, "%s_ZBuffer", GetName() );

		bool bAliasColorAndDepthSurfaces360 = false;
#ifdef _X360
		bAliasColorAndDepthSurfaces360 = ( m_nFlags & TEXTUREFLAGS_ALIAS_COLOR_AND_DEPTH_SURFACES ) != 0;
#endif
		m_pTextureHandles[1] = g_pShaderAPI->CreateDepthTexture( 
				m_ImageFormat, 
				m_nActualWidth, 
				m_nActualHeight,
				debugName,
				( m_nOriginalRenderTargetType == RENDER_TARGET_ONLY_DEPTH ),
				bAliasColorAndDepthSurfaces360 );
	}

	m_nInternalFlags |= TEXTUREFLAGSINTERNAL_ALLOCATED;
	return true;
}


//-----------------------------------------------------------------------------
// Releases the texture's hardware memory
//-----------------------------------------------------------------------------
void CTexture::FreeShaderAPITextures()
{
	if ( m_pTextureHandles && HasBeenAllocated() )
	{
		// Release the frames
		for ( int i = m_nFrameCount; --i >= 0; )
		{
			if ( g_pShaderAPI->IsTexture( m_pTextureHandles[i] ) )
			{
#ifdef WIN32
				Assert( _heapchk() == _HEAPOK );
#endif
				g_pShaderAPI->DeleteTexture( m_pTextureHandles[i] );
				m_pTextureHandles[i] = INVALID_SHADERAPI_TEXTURE_HANDLE;
			}
		}
	}
	m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_ALLOCATED;
}


//-----------------------------------------------------------------------------
// Computes the actual format of the texture
//-----------------------------------------------------------------------------
ImageFormat CTexture::ComputeActualFormat( ImageFormat srcFormat )
{
	ImageFormat dstFormat;
	bool bIsCompressed = ImageLoader::IsCompressed( srcFormat );
	if ( g_config.bCompressedTextures && bIsCompressed )
	{
		// for the runtime compressed formats the srcFormat won't equal the dstFormat, and we need to return srcFormat here
		if ( ImageLoader::IsRuntimeCompressed( srcFormat ) )
		{
			return srcFormat;
		}

		// don't do anything since we are already in a compressed format.
		dstFormat = g_pShaderAPI->GetNearestSupportedFormat( srcFormat );
		Assert( dstFormat == srcFormat );
		return dstFormat;
	}

	if ( IsGameConsole() && ( srcFormat == IMAGE_FORMAT_A8 ) )
	{
		// these are the right alpha formats for xbox
		return IMAGE_FORMAT_A8;
	}

#if defined( _X360 )
	if ( srcFormat == IMAGE_FORMAT_LINEAR_I8 )
	{
		return IMAGE_FORMAT_LINEAR_I8;
	}
#endif

	// NOTE: Below this piece of code is only called when compressed textures are
	// turned off, or if the source texture is not compressed.

#ifdef DX_TO_GL_ABSTRACTION
	if ( ( srcFormat == IMAGE_FORMAT_UVWQ8888 ) || ( srcFormat == IMAGE_FORMAT_UV88 ) || ( srcFormat == IMAGE_FORMAT_UVLX8888 )  )
	{
		// Danger, this is going to blow up on the Mac.  You better know what you're
		// doing with these exotic formats...which were introduced in 1999
		Assert( 0 );
	}
#endif

	// We use the TEXTUREFLAGS_EIGHTBITALPHA and TEXTUREFLAGS_ONEBITALPHA flags
	// to decide how many bits of alpha we need; vtex checks the alpha channel
	// for all white, etc.
	if( (srcFormat == IMAGE_FORMAT_UVWQ8888) || ( srcFormat == IMAGE_FORMAT_UV88 ) || 
		( srcFormat == IMAGE_FORMAT_UVLX8888 ) || ( srcFormat == IMAGE_FORMAT_RGBA16161616 ) ||
		( srcFormat == IMAGE_FORMAT_RGBA16161616F ) || ( srcFormat == IMAGE_FORMAT_RGBA32323232F ) ||
		( srcFormat == IMAGE_FORMAT_R32F ) )
	{
#ifdef DX_TO_GL_ABSTRACTION		
		dstFormat = g_pShaderAPI->GetNearestSupportedFormat( srcFormat, false );  // Stupid HACK!
#else
		dstFormat = g_pShaderAPI->GetNearestSupportedFormat( srcFormat, true );  // Stupid HACK!
#endif
	} 
	else if ( m_nFlags & ( TEXTUREFLAGS_EIGHTBITALPHA | TEXTUREFLAGS_ONEBITALPHA ) )
	{
		dstFormat = g_pShaderAPI->GetNearestSupportedFormat( IMAGE_FORMAT_BGRA8888 );
	}
	else if ( srcFormat == IMAGE_FORMAT_I8 )
	{
		dstFormat = g_pShaderAPI->GetNearestSupportedFormat( IMAGE_FORMAT_I8 );
	}
	else
	{
		dstFormat = g_pShaderAPI->GetNearestSupportedFormat( IMAGE_FORMAT_BGR888 );
	}
	return dstFormat;
}

//-----------------------------------------------------------------------------
// Compute the actual mip count based on the actual size
//-----------------------------------------------------------------------------
int CTexture::ComputeActualMipCount() const
{
	bool bForceTextureAllMips = g_bForceTextureAllMips; // Init with global set from -forceallmips on the command line

	// If the current hardware doesn't support mipped cubemaps, return 1
	if ( ( m_nFlags & TEXTUREFLAGS_ENVMAP ) && ( !HardwareConfig()->SupportsMipmappedCubemaps() ) )
	{
		return 1;
	}

	// "nomip 1" - If the artists requested no mips in the .txt file of their source art, return 1
	if ( m_nFlags & TEXTUREFLAGS_NOMIP )
	{
		return 1;
	}

	// "allmips 1" - If the artists requested all mips in the .txt file of their source art, load all mips on all platforms
	if ( m_nFlags & TEXTUREFLAGS_ALL_MIPS )
	{
		bForceTextureAllMips = true;
	}

	// "mostmips 1" - If the artists requested most mips in the .txt file of their source art, don't load the bottom mips, ever
	bool bMostMips = false;
	if ( m_nFlags & TEXTUREFLAGS_MOST_MIPS )
	{
		bMostMips = true;
	}

	// OpenGL - Don't ever drop mips
	if ( IsOpenGL() )
	{
		bForceTextureAllMips = true;
		bMostMips = false;
	}

	// If on the PC and running a newer OS than WinXP, then don't drop mips.
	// XP can crash if we run out of paged pool memory since each mip consumes ~1kb of paged pool memory.
	#if defined( WIN32 ) && !defined( _GAMECONSOLE )
	{
		OSVERSIONINFOEX osvi;
		ZeroMemory( &osvi, sizeof( OSVERSIONINFOEX ) );
		osvi.dwOSVersionInfoSize = sizeof( OSVERSIONINFOEX );
		if ( GetVersionEx( ( OSVERSIONINFO * )&osvi ) )
		{
			if ( osvi.dwMajorVersion >= 6 ) // Major version 6 is Windows Vista and Win7
			{
				// Windows Vista or newer, so it's safe to load all mips
				bForceTextureAllMips = true;
			}
		}
	}
	#endif

	if ( IsX360() )
	{
		bForceTextureAllMips  = true;
	}

	bool bIsFlashlightTextureOnGL = false;
#ifdef DX_TO_GL_ABSTRACTION
	 // Hack to only recognize the border bit (for the purposes of truncating the mip chain) on "flashlight" textures on Mac
	const char *pTexName = m_Name.String();
	bIsFlashlightTextureOnGL =  ( m_nFlags & TEXTUREFLAGS_BORDER ) && V_stristr( pTexName, "flashlight" );
#endif
	
	// If we are not loading all mips, then count the number of mips we want to load
	if ( ( !IsOpenGL() && !bForceTextureAllMips ) || bMostMips || bIsFlashlightTextureOnGL )
	{
		// Stop loading mips when width or height is < 32
		int nMaxMipSize = 32; // Default for windows XP
		if ( IsPS3() )
		{
			nMaxMipSize = 4;
		}

		if ( bMostMips )
		{
			// !!! This overrides all other settings !!!
			nMaxMipSize = 32;
		}

		int nNumMipLevels = 1;
		int h = m_nActualWidth;
		int w = m_nActualHeight;
		while (	MIN( w, h ) > nMaxMipSize )
		{
			nNumMipLevels++;
			
			w >>= 1;
			h >>= 1;
		}
		return nNumMipLevels;
	}
	else
	{
		// Load all mips
		return ImageLoader::GetNumMipMapLevels( m_nActualWidth, m_nActualHeight, m_nActualDepth );
	}
}

//-----------------------------------------------------------------------------
// Calculates info about whether we can make the texture smaller and by how much
//-----------------------------------------------------------------------------
int CTexture::ComputeActualSize( bool bIgnorePicmip, IVTFTexture *pVTFTexture )
{
	// Must skip mip levels if the texture is too large for our board to handle
	m_nActualWidth = m_nMappingWidth;
	m_nActualHeight = m_nMappingHeight;
	m_nActualDepth = m_nMappingDepth;

	int nClampX = m_nActualWidth;	// no clamping (clamp to texture dimensions)
	int nClampY = m_nActualHeight;
	int nClampZ = m_nActualDepth;

	//
	// PC:
	// Fetch clamping dimensions from special LOD control settings block
	// or runtime texture lod override.
	//
	if ( IsPC() )
	{
		// Fetch LOD settings from the VTF if available
		TextureLODControlSettings_t lcs;
		memset( &lcs, 0, sizeof( lcs ) );
		TextureLODControlSettings_t const *pLODInfo = NULL;
		if ( pVTFTexture )
		{
			pLODInfo = reinterpret_cast<TextureLODControlSettings_t const *> (
					pVTFTexture->GetResourceData( VTF_RSRC_TEXTURE_LOD_SETTINGS, NULL ) );
			if ( pLODInfo )
				lcs = *pLODInfo;
		}

		// Prepare the default LOD settings (that essentially result in no clamping)
		TextureLODControlSettings_t default_lod_settings;
		memset( &default_lod_settings, 0, sizeof( default_lod_settings ) );
		{
			for ( int w = m_nActualWidth; w > 1; w >>= 1 )
				  ++ default_lod_settings.m_ResolutionClampX;
			for ( int h = m_nActualHeight; h > 1; h >>= 1 )
				  ++ default_lod_settings.m_ResolutionClampY;
		}

		// Check for LOD control override
		{
			TextureLodOverride::OverrideInfo oi = TextureLodOverride::Get( GetName() );
			
			if ( oi.x && oi.y && !pLODInfo )	// If overriding texture that doesn't have lod info yet, then use default
				lcs = default_lod_settings;

			lcs.m_ResolutionClampX += oi.x;
			lcs.m_ResolutionClampY += oi.y;
			if ( int8( lcs.m_ResolutionClampX ) < 0 )
				lcs.m_ResolutionClampX = 0;
			if ( int8( lcs.m_ResolutionClampY ) < 0 )
				lcs.m_ResolutionClampY = 0;
		}

		// Compute the requested mip0 dimensions
		if ( lcs.m_ResolutionClampX && lcs.m_ResolutionClampY )
		{
			nClampX = (1 << lcs.m_ResolutionClampX );
			nClampY = (1 << lcs.m_ResolutionClampY );
		}

		// Check for exclude settings
		{
			int iExclude = TextureLodExclude::Get( GetName() );
			if ( iExclude > 0 )
			{
				// Mip request by exclude rules
				nClampX = MIN( iExclude, nClampX );
				nClampY = MIN( iExclude, nClampY );
			}
			else if ( iExclude == 0 )
			{
				// Texture should be excluded completely
				// we cannot actually exclude it, we need
				// to clamp it down to 4x4 for dxt to work.
				// The texture will never be loaded when honoring
				// the real exclude list rules.
				nClampX = MIN( 4, nClampX );
				nClampY = MIN( 4, nClampY );
			}
		}

		// In case clamp values exceed texture dimensions, then fix up
		// the clamping values
		nClampX = MIN( nClampX, m_nActualWidth );
		nClampY = MIN( nClampY, m_nActualHeight );
	}

	//
	// Honor dimension limit restrictions
	//
	int nDimensionLimit = 0;
	if ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_SHOULDTEMPEXCLUDE )
	{
		nDimensionLimit = m_nDesiredTempDimensionLimit;
	}
	else
	{
		nDimensionLimit = m_nDesiredDimensionLimit;
	}
	if ( nDimensionLimit < 0 )
	{
		nDimensionLimit = 0;
	}

	if ( IsGameConsole() )
	{
		// limiting large textures 
		static int s_nMaxDimensionLimit = 0;
		if ( !s_nMaxDimensionLimit )
		{
			bool bNo256 = ( CommandLine()->FindParm( "-no256" ) != 0 );
			bool bNo512 = ( CommandLine()->FindParm( "-no512" ) != 0 );
			bool bNo1024 = CommandLine()->FindParm( "-no1024" ) && !CommandLine()->FindParm( "-allow1024" );
			if ( bNo256 )
			{
				s_nMaxDimensionLimit = 128;
			}
			else if ( bNo512 )
			{
				s_nMaxDimensionLimit = 256;
			}
			else if ( g_pFullFileSystem->IsDVDHosted() || bNo1024 )
			{
				s_nMaxDimensionLimit = 512;
			}
			else
			{
				s_nMaxDimensionLimit = 1024;
			}
		}
		if ( nDimensionLimit > 0 )
		{
			nDimensionLimit = MIN( nDimensionLimit, s_nMaxDimensionLimit );
		}
		else if ( !( m_nFlags & (TEXTUREFLAGS_NOLOD|TEXTUREFLAGS_NOMIP|TEXTUREFLAGS_PROCEDURAL|TEXTUREFLAGS_RENDERTARGET|TEXTUREFLAGS_DEPTHRENDERTARGET) ) )
		{
			nDimensionLimit = s_nMaxDimensionLimit;
		}
	}
	else if ( IsPlatformOSX() )
	{
		// limiting large textures on OSX to 1024, override with -allow2048 on cl
		static int s_nMaxDimensionLimit = 0;

		if (!s_nMaxDimensionLimit)
		{
			bool bAllow2048 = !!CommandLine()->FindParm( "-allow2048" );

			if ( !bAllow2048 && !( m_nFlags & (TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_DEPTHRENDERTARGET) ) )
			{
				s_nMaxDimensionLimit = 1024;
			}
		}

		if ( !( m_nFlags & (TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_DEPTHRENDERTARGET) ) )
		{
			nDimensionLimit = s_nMaxDimensionLimit;
		}
	}

	// Special case: If the top mipmap level is <= 128KB, and the width is really wide (2048), and its height is <= 64, and we know it's mipmapped, then allow one axis to be > 1024, otherwise just restrict to 1024.
	// This purposely convoluted logic is useful on things like the confetti particle effect's texture (used in sp_a2_column_blocker), which is 2048x64, and is very noticeable when it's cut down to 1024x32.
	if ( nDimensionLimit == 1024 )
	{
		if ( ( ImageLoader::GetMemRequired( m_nActualWidth, m_nActualHeight, 1, m_ImageFormat, false ) <= 128 * 1024 ) &&
			( m_nActualWidth == 2048 ) && ( m_nActualHeight <= 64 ) && 
			( pVTFTexture ) && ( pVTFTexture->MipCount() > 1 ) )
		{
			nDimensionLimit = 2048;
		}
	}

	//
	// Unless ignoring picmip, reflect the global picmip level in clamp dimensions
	//
	if ( !bIgnorePicmip )
	{
		// If picmip requests texture degradation, then honor it
		// for loddable textures only
		if ( !( m_nFlags & TEXTUREFLAGS_NOLOD ) &&
			  ( g_config.skipMipLevels > 0 ) )
		{
			for ( int iDegrade = 0; iDegrade < g_config.skipMipLevels; ++ iDegrade )
			{
				// don't go lower than 4, or dxt textures won't work properly
				if ( nClampX > 4 &&
					 nClampY > 4 )
				{
					nClampX >>= 1;
					nClampY >>= 1;
				}
			}
		}

		// If picmip requests quality upgrade, then always honor it
		if ( g_config.skipMipLevels < 0 )
		{
			for ( int iUpgrade = 0; iUpgrade < - g_config.skipMipLevels; ++ iUpgrade )
			{
				if ( nClampX < m_nActualWidth &&
					 nClampY < m_nActualHeight )
				{
					nClampX <<= 1;
					nClampY <<= 1;
				}
				else
					break;
			}
		}
	}

	// honor dimension limit after picmip downgrade/upgrade
	if ( nDimensionLimit > 0 )
	{
		while ( nClampX > nDimensionLimit ||
				nClampY > nDimensionLimit )
		{
			nClampX >>= 1;
			nClampY >>= 1;
		}
	}

	//
	// Now use hardware settings to clamp our "clamping dimensions"
	//
	int iHwWidth = HardwareConfig()->MaxTextureWidth();
	int iHwHeight = HardwareConfig()->MaxTextureHeight();
	int iHwDepth = HardwareConfig()->MaxTextureDepth();

	nClampX = MIN( nClampX, MAX( iHwWidth, 4 ) );
	nClampY = MIN( nClampY, MAX( iHwHeight, 4 ) );
	nClampZ = MIN( nClampZ, MAX( iHwDepth, 1 ) );

	Assert( nClampZ >= 1 );

	// In case clamp values exceed texture dimensions, then fix up
	// the clamping values.
	nClampX = MIN( nClampX, m_nActualWidth );
	nClampY = MIN( nClampY, m_nActualHeight );
	nClampZ = MIN( nClampZ, m_nActualDepth );
	
	//
	// Clamp to the determined dimensions
	//
	int numMipsSkipped = 0; // will compute now when clamping how many mips we drop
	while ( ( m_nActualWidth  > nClampX ) ||
		    ( m_nActualHeight > nClampY ) ||
			( m_nActualDepth  > nClampZ ) )
	{
		m_nActualWidth  >>= 1;
		m_nActualHeight >>= 1;
		m_nActualDepth  >>= 1;
		if ( m_nActualDepth < 1 )
			m_nActualDepth = 1;

		++ numMipsSkipped;
	}

	Assert( m_nActualWidth > 0 && m_nActualHeight > 0 && m_nActualDepth > 0 );

	// Now that we've got the actual size, we can figure out the mip count
	m_nActualMipCount = ComputeActualMipCount();

	// Returns the number we skipped
	return numMipsSkipped;
}


//-----------------------------------------------------------------------------
// Used to modify the texture bits (procedural textures only)
//-----------------------------------------------------------------------------
void CTexture::SetTextureRegenerator( ITextureRegenerator *pTextureRegen, bool releaseExisting )
{
	// NOTE: These can only be used by procedural textures
	Assert( IsProcedural() );

	if ( m_pTextureRegenerator && releaseExisting )
	{
		m_pTextureRegenerator->Release();
	}
	m_pTextureRegenerator = pTextureRegen; 
}


//-----------------------------------------------------------------------------
// Gets us modifying a particular frame of our texture
//-----------------------------------------------------------------------------
void CTexture::Modify( int iFrame )
{
	Assert( iFrame >= 0 && iFrame < m_nFrameCount );
	Assert( HasBeenAllocated() );

	g_pShaderAPI->ModifyTexture( m_pTextureHandles[iFrame] );
}


//-----------------------------------------------------------------------------
// Sets the texture clamping state on the currently modified frame
//-----------------------------------------------------------------------------
void CTexture::SetWrapState( )
{
	// Border clamp applies to all texture coordinates
	if ( m_nFlags & TEXTUREFLAGS_BORDER )
	{
		g_pShaderAPI->TexWrap( SHADER_TEXCOORD_S, SHADER_TEXWRAPMODE_BORDER );
		g_pShaderAPI->TexWrap( SHADER_TEXCOORD_T, SHADER_TEXWRAPMODE_BORDER );
		g_pShaderAPI->TexWrap( SHADER_TEXCOORD_U, SHADER_TEXWRAPMODE_BORDER );
		return;
	}

	// Clamp mode in S
	if ( m_nFlags & TEXTUREFLAGS_CLAMPS )
	{
		g_pShaderAPI->TexWrap( SHADER_TEXCOORD_S, SHADER_TEXWRAPMODE_CLAMP );
	}
	else
	{
		g_pShaderAPI->TexWrap( SHADER_TEXCOORD_S, SHADER_TEXWRAPMODE_REPEAT );
	}

	// Clamp mode in T
	if ( m_nFlags & TEXTUREFLAGS_CLAMPT )
	{
		g_pShaderAPI->TexWrap( SHADER_TEXCOORD_T, SHADER_TEXWRAPMODE_CLAMP );
	}
	else
	{
		g_pShaderAPI->TexWrap( SHADER_TEXCOORD_T, SHADER_TEXWRAPMODE_REPEAT );
	}

	// Clamp mode in U
	if ( m_nFlags & TEXTUREFLAGS_CLAMPU )
	{
		g_pShaderAPI->TexWrap( SHADER_TEXCOORD_U, SHADER_TEXWRAPMODE_CLAMP );
	}
	else
	{
		g_pShaderAPI->TexWrap( SHADER_TEXCOORD_U, SHADER_TEXWRAPMODE_REPEAT );
	}
}


//-----------------------------------------------------------------------------
// Sets the texture filtering state on the currently modified frame
//-----------------------------------------------------------------------------
void CTexture::SetFilterState()
{
	// Turns off filtering when we're point sampling
	if( m_nFlags & TEXTUREFLAGS_POINTSAMPLE )
	{
		g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_NEAREST );
		g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_NEAREST );
		return;
	}

	// NOTE: config.bMipMapTextures and config.bFilterTextures is handled in ShaderAPIDX8
	bool bEnableMipmapping = ( m_nFlags & TEXTUREFLAGS_NOMIP ) ? false : true;
	if( !bEnableMipmapping )
	{
		g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR );
		g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
		return;
	}

	// Determing the filtering mode
	bool bIsAnisotropic = false; bool bIsTrilinear = false;
	if ( (g_config.m_nForceAnisotropicLevel > 1) && (HardwareConfig()->MaximumAnisotropicLevel() > 1) )
	{
		bIsAnisotropic = true;
	}
	else
	{
		bIsAnisotropic = (( m_nFlags & TEXTUREFLAGS_ANISOTROPIC ) != 0) && (HardwareConfig()->MaximumAnisotropicLevel() > 1);
		bIsTrilinear = ( g_config.m_nForceAnisotropicLevel == 1 ) || ( ( m_nFlags & TEXTUREFLAGS_TRILINEAR ) != 0 );
	}

	if ( bIsAnisotropic )
	{		    
		g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_ANISOTROPIC );
		g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_ANISOTROPIC );
	}
	else
	{
		if ( bIsTrilinear )
		{
			g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR_MIPMAP_LINEAR );
			g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
		}
		else
		{
			g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR_MIPMAP_NEAREST );
			g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
		}
	}
}


//-----------------------------------------------------------------------------
// Download bits main entry point!!
//-----------------------------------------------------------------------------
void CTexture::DownloadTexture( Rect_t *pRect, void *pSourceData, int nSourceDataSize )
{
	// No downloading necessary if there's no graphics
	if ( !g_pShaderDevice->IsUsingGraphics() )
		return;

	if ( m_nInternalFlags & ( TEXTUREFLAGSINTERNAL_SHOULDTEMPEXCLUDE|TEXTUREFLAGSINTERNAL_TEMPEXCLUDED|TEXTUREFLAGSINTERNAL_TEMPEXCLUDE_UPDATE ) )
	{
		// temp exclusions are allowed to occur anytime during gameplay
		// for expected stability, the ShaderAPITextureHandle_t must stay as-is
		// store them off prior to their expected release, so they can be sent back down as a hint to the allocator for the expected re-allocation
		// this allows the underlying d3d bits to be changed, but other system that have stored off the prior handles need not be aware
		if ( m_nFrameCount > 0 )
		{
			m_pTempTextureHandles = new ShaderAPITextureHandle_t[m_nFrameCount];
			for ( int i = 0; i != m_nFrameCount; ++i )
			{
				m_pTempTextureHandles[i] = m_pTextureHandles[i];
			}
		}
	}

	// We don't know the actual size of the texture at this stage...
	if ( !pRect )
	{
		ReconstructTexture( pSourceData, nSourceDataSize );
	}
	else
	{
		ReconstructPartialTexture( pRect );
	}

	// Iterate over all the frames and set the appropriate wrapping + filtering state
	SetFilteringAndClampingMode();

	// texture bits have been updated, update the exclusion state
	if ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_SHOULDEXCLUDE )
	{
		m_nInternalFlags |= TEXTUREFLAGSINTERNAL_EXCLUDED;
	}
	else
	{
		m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_EXCLUDED;
	}

	// texture bits have been picmipped, update the picmip state
	if ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_SHOULDTEMPEXCLUDE )
	{
		m_nInternalFlags |= TEXTUREFLAGSINTERNAL_TEMPEXCLUDED;
		m_nActualDimensionLimit = m_nDesiredTempDimensionLimit;
	}
	else
	{
		m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_TEMPEXCLUDED;
		m_nActualDimensionLimit = m_nDesiredDimensionLimit;
	}

	// any possible temp exclude update is finished
	m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_TEMPEXCLUDE_UPDATE;

	if ( m_pTempTextureHandles )
	{
		// check for handle stability
		// the handles MUST stay the same across a ReconstructTexture(), various threaded systems have already cached/queued off the handles
		for ( int i = 0; i < m_nFrameCount; i++ )
		{
			if ( m_pTextureHandles[i] != m_pTempTextureHandles[i] )
			{
				// crash will be imminent, the handles have changed and they should not have
				// shaderapi will crash on next texture access because stored handles reference the freed texture handles, not the valid allocated ones
				Assert( 0 );
				Warning( "ERROR! - Crash Expected. DownloadTexture(): Texture Handle Difference: %d expected:0x%8.8x actual:0x%8.8x\n", i, m_pTempTextureHandles[i], m_pTextureHandles[i] );
			}
		}

		delete[] m_pTempTextureHandles;
		m_pTempTextureHandles = NULL;
	}
}

//-----------------------------------------------------------------------------
// Download bits main entry point for async textures (based on CTexture::DownloadTexture)
// Very controlled environment: no procedural textures, no render target, console not supported
// The download is done is 2 parts:
//		* Generating the VTF
//		* Using VTF to create the shader api texture (effectively the corresponding d3d resource)
// In order to reduce spikes on the main thread (cf CMaterialSystem::ServiceAsyncTextureLoads), the flMaxTimeMs
// limit has been introduced =>  you can safely exit after generating the VTF and resume it at a later date
// Note that async textures are sharing the same scratch VTF therefore, if the download of an async texture
// has been interuped, it is important not to start downloading a new async texture (that would effectively invalidate
// the VTF of the other texture) - Done in CMaterialSystem::ServiceAsyncTextureLoads)
// Returns true if the download has been completed (ie interrupted after generating the VTF), false otherwise
//-----------------------------------------------------------------------------
bool CTexture::DownloadAsyncTexture( AsyncTextureContext_t *pContext, void *pSourceData, int nSourceDataSize, float flMaxTimeMs )
{
	// No downloading necessary if there's no graphics
	if (!g_pShaderDevice->IsUsingGraphics())
		return true;
	
	Assert( m_nFlags & TEXTUREFLAGS_ASYNC_DOWNLOAD );
	Assert( !IsGameConsole() );
	Assert( !IsRenderTarget() );
	Assert( !IsTempRenderTarget() );
	Assert( !IsProcedural() );

	if ( !pContext->m_pVTFTexture )
	{
		double flStartTime = Plat_FloatTime();

		int oldWidth = m_nActualWidth;
		int oldHeight = m_nActualHeight;
		int oldDepth = m_nActualDepth;
		int oldMipCount = m_nActualMipCount;
		int oldFrameCount = m_nFrameCount;

		pContext->m_pVTFTexture = LoadTexttureBitsFromFileOrData( pSourceData, nSourceDataSize, NULL );

		if (!HasBeenAllocated() ||
			m_nActualWidth != oldWidth ||
			m_nActualHeight != oldHeight ||
			m_nActualDepth != oldDepth ||
			m_nActualMipCount != oldMipCount ||
			m_nFrameCount != oldFrameCount)
		{
			if (HasBeenAllocated())
			{
				// This is necessary for the reload case, we may discover there
				// are more frames of a texture animation, for example, which means
				// we can't rely on having the same number of texture frames.
				FreeShaderAPITextures();
			}

			// Create the shader api textures
			if (!AllocateShaderAPITextures())
				return true;
		}

		// Safe point to interrupt teh texture download
		float flElapsedMs = (Plat_FloatTime() - flStartTime) * 1000.0f;
		if (flElapsedMs > flMaxTimeMs)
		{
			// Running out of time - the shader api texture will be created later (most probably on the next frame)
			return false;
		}
	}

	// Blit down the texture faces, frames, and mips into the board memory
	int nFirstFace, nFaceCount;
	GetDownloadFaceCount( nFirstFace, nFaceCount );

	WriteDataToShaderAPITexture( m_nFrameCount, nFaceCount, nFirstFace, m_nActualMipCount, pContext->m_pVTFTexture, m_ImageFormat );

	// Iterate over all the frames and set the appropriate wrapping + filtering state
	SetFilteringAndClampingMode();

	pContext->m_pVTFTexture = NULL;

	return true;
}

void CTexture::Download( Rect_t *pRect, int nAdditionalCreationFlags /* = 0 */ )
{
	if ( nAdditionalCreationFlags & TEXTUREFLAGS_ASYNC_DOWNLOAD )
	{
		m_nFlags |= nAdditionalCreationFlags;
		if ( ScheduleAsyncDownload() )
		{
			return;
		}
		else
		{
			// failed to find file so remove async download flag
			m_nFlags &= ~TEXTUREFLAGS_ASYNC_DOWNLOAD;
			// and intentionally fall through to normal Download() which will use the error texture
		}
	}

	if ( g_pShaderAPI->CanDownloadTextures() ) // Only download the bits if we can...
	{
		MaterialLock_t hLock = MaterialSystem()->Lock();
		m_nFlags |= nAdditionalCreationFlags; // Path to let stdshaders drive settings like sRGB-ness at creation time
		DownloadTexture( pRect );
		MaterialSystem()->Unlock( hLock );
	}
}

#ifdef _PS3
void CTexture::Ps3gcmRawBufferAlias( char const *pRTName )
{
	ComputeActualSize( true );
	m_nActualDimensionLimit = m_nDesiredDimensionLimit;
	m_nInternalFlags |= TEXTUREFLAGSINTERNAL_ALLOCATED;
	extern ShaderAPITextureHandle_t Ps3gcmGetArtificialTextureHandle( int iHandle );
	if ( !Q_strcmp( pRTName, "^PS3^BACKBUFFER" ) )
		m_pTextureHandles[0] = Ps3gcmGetArtificialTextureHandle( PS3GCM_ARTIFICIAL_TEXTURE_HANDLE_INDEX_BACKBUFFER );
	else if ( !Q_strcmp( pRTName, "^PS3^DEPTHBUFFER" ) )
		m_pTextureHandles[0] = Ps3gcmGetArtificialTextureHandle( PS3GCM_ARTIFICIAL_TEXTURE_HANDLE_INDEX_DEPTHBUFFER );
	else
		Error( "<vitaliy> Unexpected raw buffer alias: %s!\n", pRTName );
}
#endif


void CTexture::Bind( Sampler_t sampler, TextureBindFlags_t nBindFlags )
{
	Bind( sampler, nBindFlags, 0 );
}

//-----------------------------------------------------------------------------
// Binds a particular texture
//-----------------------------------------------------------------------------
void CTexture::Bind( Sampler_t sampler1, TextureBindFlags_t nBindFlags, int nFrame, Sampler_t sampler2 /* = -1 */ )
{
	if ( g_pShaderDevice->IsUsingGraphics() )
	{
		if ( nFrame < 0 || nFrame >= m_nFrameCount )
		{
			// FIXME: Use the well-known 'error' id instead of frame 0
			nFrame = 0;
			//			Assert(0);
		}

		// Make sure we've actually allocated the texture handle
		if ( HasBeenAllocated() )
		{
			g_pShaderAPI->BindTexture( sampler1, nBindFlags, m_pTextureHandles[nFrame] );
		}
		else
		{
			Warning( "Trying to bind texture %s, but texture handles are not valid. Binding a white texture!", GetName() );
			g_pShaderAPI->BindStandardTexture( sampler1, nBindFlags, TEXTURE_WHITE );
		}
	}
}



void CTexture::BindVertexTexture( VertexTextureSampler_t sampler, int nFrame )
{
	if ( g_pShaderDevice->IsUsingGraphics() )
	{
		if ( nFrame < 0 || nFrame >= m_nFrameCount )
		{
			// FIXME: Use the well-known 'error' id instead of frame 0
			nFrame = 0;
			//			Assert(0);
		}

		// Make sure we've actually allocated the texture
		Assert( HasBeenAllocated() );

		g_pShaderAPI->BindVertexTexture( sampler, m_pTextureHandles[nFrame] );
	}
}


//-----------------------------------------------------------------------------
// Set this texture as a render target
//-----------------------------------------------------------------------------
bool CTexture::SetRenderTarget( int nRenderTargetID )
{
	return SetRenderTarget( nRenderTargetID, NULL );
}

//-----------------------------------------------------------------------------
// Set this texture as a render target
// Optionally bind pDepthTexture as depth buffer
//-----------------------------------------------------------------------------
bool CTexture::SetRenderTarget( int nRenderTargetID, ITexture *pDepthTexture )
{
	if ( ( m_nFlags & TEXTUREFLAGS_RENDERTARGET ) == 0 )
		return false;

	// Make sure we've actually allocated the texture handles
	Assert( HasBeenAllocated() );

	ShaderAPITextureHandle_t textureHandle;
	if ( !IsX360() )
	{
		textureHandle = m_pTextureHandles[0];
	}
	else
	{
		Assert( m_nFrameCount > 1 );
		textureHandle = m_pTextureHandles[m_nFrameCount-1];
	}

	ShaderAPITextureHandle_t depthTextureHandle = (ShaderAPITextureHandle_t)SHADER_RENDERTARGET_DEPTHBUFFER;

	if ( m_nFlags & TEXTUREFLAGS_DEPTHRENDERTARGET )
	{
		Assert( m_nFrameCount >= 2 );
		depthTextureHandle = m_pTextureHandles[1];
	} 
	else if ( m_nFlags & TEXTUREFLAGS_NODEPTHBUFFER )
	{
		// GR - render target without depth buffer	
		depthTextureHandle = (ShaderAPITextureHandle_t)SHADER_RENDERTARGET_NONE;
	}

	if ( pDepthTexture)
	{
		depthTextureHandle = static_cast<ITextureInternal *>(pDepthTexture)->GetTextureHandle(0);
	}

	g_pShaderAPI->SetRenderTargetEx( nRenderTargetID, textureHandle, depthTextureHandle );
	return true;
}


//-----------------------------------------------------------------------------
// Reference counting
//-----------------------------------------------------------------------------
void CTexture::IncrementReferenceCount( void )
{
	++m_nRefCount;
}

void CTexture::DecrementReferenceCount( void )
{
	--m_nRefCount;

	/* FIXME: Probably have to remove this from the texture manager too..?
	if (IsProcedural() && (m_nRefCount < 0))
		delete this;
	*/
}

int CTexture::GetReferenceCount() const
{
	return m_nRefCount;
}


//-----------------------------------------------------------------------------
// Various accessor methods
//-----------------------------------------------------------------------------
const char* CTexture::GetName( ) const
{
	return m_Name.String();
}

const char* CTexture::GetTextureGroupName( ) const
{
	return m_TextureGroupName.String();
}

void CTexture::SetName( const char* pName )
{
	// normalize and convert to a symbol
	char szCleanName[MAX_PATH];
	m_Name = NormalizeTextureName( pName, szCleanName, sizeof( szCleanName ) );

#ifdef _DEBUG
	if ( m_pDebugName )
	{
		delete [] m_pDebugName;
	}
	int nLen = V_strlen( szCleanName ) + 1;
	m_pDebugName = new char[nLen];
	V_memcpy( m_pDebugName, szCleanName, nLen );
#endif
}

ImageFormat CTexture::GetImageFormat()	const
{
	return m_ImageFormat;
}

int CTexture::GetMappingWidth()	const
{
	return m_nMappingWidth;
}

int CTexture::GetMappingHeight() const
{
	return m_nMappingHeight;
}

int CTexture::GetMappingDepth() const
{
	return m_nMappingDepth;
}

int CTexture::GetActualWidth() const
{
	return m_nActualWidth;
}

int CTexture::GetActualHeight()	const
{
	return m_nActualHeight;
}

int CTexture::GetActualDepth()	const
{
	return m_nActualDepth;
}

int CTexture::GetNumAnimationFrames() const
{
	return m_nFrameCount;
}

void CTexture::GetReflectivity( Vector& reflectivity )
{
	Precache();
	VectorCopy( m_vecReflectivity, reflectivity );
}

//-----------------------------------------------------------------------------
// Little helper polling methods
//-----------------------------------------------------------------------------
bool CTexture::IsTranslucent() const
{
	return ( m_nFlags & (TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA) ) != 0;
}

bool CTexture::IsNormalMap( void ) const
{
	return ( ( m_nFlags & TEXTUREFLAGS_NORMAL ) != 0 );
}

bool CTexture::IsCubeMap( void ) const
{
	return ( ( m_nFlags & TEXTUREFLAGS_ENVMAP ) != 0 );
}

bool CTexture::IsRenderTarget( void ) const
{
	return ( ( m_nFlags & TEXTUREFLAGS_RENDERTARGET ) != 0 );
}

bool CTexture::IsTempRenderTarget( void ) const
{
	return ( ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_TEMPRENDERTARGET ) != 0 );
}

bool CTexture::IsProcedural() const
{
	return ( (m_nFlags & TEXTUREFLAGS_PROCEDURAL) != 0 );
}

bool CTexture::IsMipmapped() const
{
	return ( (m_nFlags & TEXTUREFLAGS_NOMIP) == 0 );
}

unsigned int CTexture::GetFlags() const
{
	return m_nFlags;
}

void CTexture::ForceLODOverride( int iNumLodsOverrideUpOrDown )
{
	if ( IsGameConsole() )
	{
		// not supporting
		Assert( 0 );
		return;
	}

	TextureLodOverride::OverrideInfo oi( iNumLodsOverrideUpOrDown, iNumLodsOverrideUpOrDown );
	TextureLodOverride::Add( GetName(), oi );
	Download( NULL );
}

void CTexture::ForceExcludeOverride( int iExcludeOverride )
{
	if ( IsGameConsole() )
	{
		Assert( 0 );
		return;
	}

	TextureLodExclude::Add( GetName(), iExcludeOverride );
	Download( NULL );
}


bool CTexture::IsError() const
{
	return ( (m_nInternalFlags & TEXTUREFLAGSINTERNAL_ERROR) != 0 );
}

bool CTexture::IsDefaultPool() const
{
	return ( ( m_nFlags & TEXTUREFLAGS_DEFAULT_POOL ) != 0 );
}

bool CTexture::HasBeenAllocated() const
{
	return ( (m_nInternalFlags & TEXTUREFLAGSINTERNAL_ALLOCATED) != 0 );
}

bool CTexture::IsVolumeTexture() const
{
	return (m_nMappingDepth > 1);
}

//-----------------------------------------------------------------------------
// Sets the filtering + clamping modes on the texture
//-----------------------------------------------------------------------------
void CTexture::SetFilteringAndClampingMode()
{
	if( !HasBeenAllocated() )
		return;

	int nCount = m_nFrameCount;
	if ( IsX360() && IsRenderTarget() )
	{
		// 360 render targets have a reserved surface
		nCount--;
	}

	for ( int iFrame = 0; iFrame < nCount; ++iFrame )
	{
		Modify( iFrame );			// Indicate we're changing state with respect to a particular frame
		SetWrapState();				// Send the appropriate wrap/clamping modes to the shaderapi.
		SetFilterState();			// Set the filtering mode for the texture after downloading it.
									// NOTE: Apparently, the filter state cannot be set until after download
	}
}

//-----------------------------------------------------------------------------
// Loads up the non-fallback information about the texture 
//-----------------------------------------------------------------------------
void CTexture::Precache()
{
	int nHackExtraFlags = 0;

	// We only have to do something in the case of a file texture
	if ( IsRenderTarget() || IsProcedural() )
		return;

	if ( HasBeenAllocated() )
		return;

	// Blow off env_cubemap too...
	if ( !Q_strnicmp( m_Name.String(), "env_cubemap", 12 ))
		return;
	
	if ( IsGameConsole() && m_nFlags )
	{
		// 360 can be assured that precaching has already been done
		return;
	}

	IVTFTexture *pVTFTexture = GetScratchVTFTexture();

	// The texture name doubles as the relative file name
	// It's assumed to have already been set by this point	
	// Compute the cache name
	char pCacheFileName[MATERIAL_MAX_PATH];
	Q_snprintf( pCacheFileName, sizeof( pCacheFileName ), "materials/%s" TEXTURE_FNAME_EXTENSION, m_Name.String() );

#if defined( _GAMECONSOLE )
	// generate native texture
	pVTFTexture->UpdateOrCreate( pCacheFileName );
#endif

	int nVersion = -1;
	if ( IsPC() )
		nVersion = VTF_MAJOR_VERSION;
	else if ( IsX360() )
		nVersion = VTF_X360_MAJOR_VERSION;
	else if ( IsPS3() )
		nVersion = VTF_PS3_MAJOR_VERSION;

	int nHeaderSize = VTFFileHeaderSize( nVersion );
	unsigned char *pMem = (unsigned char *)stackalloc( nHeaderSize );
	CUtlBuffer buf( pMem, nHeaderSize );
	if ( !g_pFullFileSystem->ReadFile( pCacheFileName, NULL, buf, nHeaderSize ) )	
	{
		goto precacheFailed;
	}

	// Unserialize the header only
#if !defined( _GAMECONSOLE )
	if ( !pVTFTexture->Unserialize( buf, true ) )
#else
	if ( !pVTFTexture->UnserializeFromBuffer( buf, true, true, true, 0 ) )
#endif
	{
		Warning( "Error reading material \"%s\"\n", pCacheFileName );
		goto precacheFailed;
	}

	// FIXME: Hack for L4D
	if ( !Q_strnicmp( pCacheFileName, "materials/graffiti/", 19 ) )
	{
		nHackExtraFlags = TEXTUREFLAGS_NOLOD; 
	}

	// NOTE: Don't set the image format in case graphics are active
	VectorCopy( pVTFTexture->Reflectivity(), m_vecReflectivity );
	m_nMappingWidth = pVTFTexture->Width();
	m_nMappingHeight = pVTFTexture->Height();
	m_nMappingDepth = pVTFTexture->Depth();
	m_nFlags = pVTFTexture->Flags() | nHackExtraFlags;
	m_nFrameCount = pVTFTexture->FrameCount();

	return;

precacheFailed:
	m_vecReflectivity.Init( 0, 0, 0 );
	m_nMappingWidth = 32;
	m_nMappingHeight = 32;
	m_nMappingDepth = 1;
	m_nFlags = TEXTUREFLAGS_NOMIP;
	m_nInternalFlags |= TEXTUREFLAGSINTERNAL_ERROR;
	m_nFrameCount = 1;
}



//-----------------------------------------------------------------------------
// Loads the low-res image from the texture 
//-----------------------------------------------------------------------------
void CTexture::LoadLowResTexture( IVTFTexture *pTexture )
{
#if !defined( _GAMECONSOLE )
	delete [] m_pLowResImage;
	m_pLowResImage = NULL;
#endif

	if ( pTexture->LowResWidth() == 0 || pTexture->LowResHeight() == 0 )
	{
		m_LowResImageWidth = m_LowResImageHeight = 0;
		return;
	}

	m_LowResImageWidth = pTexture->LowResWidth();
	m_LowResImageHeight = pTexture->LowResHeight();

#if !defined( _GAMECONSOLE )
	m_pLowResImage = new unsigned char[m_LowResImageWidth * m_LowResImageHeight * 3];
#ifdef _DEBUG
	bool retVal = 
#endif
		ImageLoader::ConvertImageFormat( pTexture->LowResImageData(), pTexture->LowResFormat(), 
			m_pLowResImage, IMAGE_FORMAT_RGB888, m_LowResImageWidth, m_LowResImageHeight );
#ifdef _DEBUG
	Assert( retVal );
#endif
#else
	*(unsigned int*)m_LowResImageSample = *(unsigned int*)pTexture->LowResImageSample();
#endif
}

void *CTexture::GetResourceData( uint32 eDataType, size_t *pnumBytes ) const
{
	for ( DataChunk const *pDataChunk = m_arrDataChunks.Base(),
		  *pDataChunkEnd = pDataChunk + m_arrDataChunks.Count();
		  pDataChunk < pDataChunkEnd; ++pDataChunk )
	{
		if ( ( pDataChunk->m_eType & ~RSRCF_MASK ) == eDataType )
		{
			if ( ( pDataChunk->m_eType & RSRCF_HAS_NO_DATA_CHUNK ) == 0 )
			{
				if ( pnumBytes)
					*pnumBytes = pDataChunk->m_numBytes;
				return pDataChunk->m_pvData;
			}
			else
			{
				if ( pnumBytes )
					*pnumBytes = sizeof( pDataChunk->m_numBytes );

				return ( void *)( &pDataChunk->m_numBytes );
			}
		}
	}
	if ( pnumBytes )
		pnumBytes = 0;
	return NULL;
}

void CTexture::FreeResourceData()
{
	// Clean up the resources data
	for ( DataChunk const *pDataChunk = m_arrDataChunks.Base(),
		*pDataChunkEnd = pDataChunk + m_arrDataChunks.Count();
		pDataChunk < pDataChunkEnd; ++pDataChunk )
	{
		pDataChunk->Deallocate();
	}
	m_arrDataChunks.RemoveAll();
}

void CTexture::LoadResourceData( IVTFTexture *pVTFTexture )
{
	// purge any prior resource data
	FreeResourceData();

	// Load the resources
	if ( unsigned int uiRsrcCount = pVTFTexture->GetResourceTypes( NULL, 0 ) )
	{
		uint32 *arrRsrcTypes = ( uint32 * )stackalloc( uiRsrcCount * sizeof( unsigned int ) );
		pVTFTexture->GetResourceTypes( arrRsrcTypes, uiRsrcCount );

		m_arrDataChunks.EnsureCapacity( uiRsrcCount );
		for ( uint32 *arrRsrcTypesEnd = arrRsrcTypes + uiRsrcCount;
			arrRsrcTypes < arrRsrcTypesEnd; ++arrRsrcTypes )
		{
			switch ( *arrRsrcTypes )
			{
			case VTF_LEGACY_RSRC_LOW_RES_IMAGE:
			case VTF_LEGACY_RSRC_IMAGE:
				// These stock types use specific load routines
				continue;

			default:
				{
					DataChunk dc;
					dc.m_eType = *arrRsrcTypes;
					dc.m_eType &= ~RSRCF_MASK;

					size_t numBytes;
					if ( void *pvData = pVTFTexture->GetResourceData( dc.m_eType, &numBytes ) )
					{
						Assert( numBytes >= sizeof( uint32 ) );
						if ( numBytes == sizeof( dc.m_numBytes ) )
						{
							dc.m_eType |= RSRCF_HAS_NO_DATA_CHUNK;
							dc.m_pvData = NULL;
							memcpy( &dc.m_numBytes, pvData, numBytes );
						}
						else
						{
							dc.Allocate( numBytes );
							memcpy( dc.m_pvData, pvData, numBytes );
						}

						m_arrDataChunks.AddToTail( dc );
					}
				}
			}
		}
	}
}

#pragma pack(1)

struct DXTColBlock
{
	unsigned short col0;
	unsigned short col1;

	// no bit fields - use bytes
	unsigned char row[4];
};

struct DXTAlphaBlock3BitLinear
{
	unsigned char alpha0;
	unsigned char alpha1;

	unsigned char stuff[6];
};

#pragma pack()

static void FillCompressedTextureWithSingleColor( int red, int green, int blue, int alpha, unsigned char *pImageData, 
												 int width, int height, int depth, ImageFormat imageFormat )
{
	Assert( ( width < 4 ) || !( width % 4 ) );
	Assert( ( height < 4 ) || !( height % 4 ) );
	Assert( ( depth < 4 ) || !( depth % 4 ) );

	if ( width < 4 && width > 0 )
	{
		width = 4;
	}
	if ( height < 4 && height > 0 )
	{
		height = 4;
	}
	if ( depth < 4 && depth > 1 )
	{
		depth = 4;
	}
	int numBlocks = ( width * height ) >> 4;
	numBlocks *= depth;
	
	DXTColBlock colorBlock;
	memset( &colorBlock, 0, sizeof( colorBlock ) );
	( ( BGR565_t * )&( colorBlock.col0 ) )->Set( red, green, blue );
	( ( BGR565_t * )&( colorBlock.col1 ) )->Set( red, green, blue );

	switch( imageFormat )
	{
	case IMAGE_FORMAT_DXT1:
	case IMAGE_FORMAT_ATI1N:	// Invalid block data, but correct memory footprint
		{
			int i;
			for( i = 0; i < numBlocks; i++ )
			{
				memcpy( pImageData + i * 8, &colorBlock, sizeof( colorBlock ) );
			}
		}
		break;
	case IMAGE_FORMAT_DXT5:
	case IMAGE_FORMAT_ATI2N:
		{
			int i;
			for( i = 0; i < numBlocks; i++ )
			{
//				memset( pImageData + i * 16, 0, 16 );
				memcpy( pImageData + i * 16 + 8, &colorBlock, sizeof( colorBlock ) );
//				memset( pImageData + i * 16 + 8, 0xffff, 8 ); // alpha block
			}
		}
		break;
	default:
		Assert( 0 );
		break;
	}
}

//-----------------------------------------------------------------------------
// Generate a gray texture
//-----------------------------------------------------------------------------
void CTexture::GenerateGrayTexture( IVTFTexture *pTexture )
{
	if( pTexture->FaceCount() > 1 )
		return;

	if( pTexture->IsCubeMap() )
		return;

	switch( pTexture->Format() )
	{
		// These are formats that we don't bother with
	case IMAGE_FORMAT_RGBA16161616F:
	case IMAGE_FORMAT_R32F:
	case IMAGE_FORMAT_RGB323232F:
	case IMAGE_FORMAT_RGBA32323232F:
	case IMAGE_FORMAT_UV88:
		break;
	default:
		for (int iFrame = 0; iFrame < pTexture->FrameCount(); ++iFrame )
		{
			for (int iFace = 0; iFace < pTexture->FaceCount(); ++iFace )
			{
				for (int iMip = 0; iMip < pTexture->MipCount(); ++iMip )
				{
					int green  =	128;
					int red	   =	128;
					int blue   =	128;

					int nWidth, nHeight, nDepth;
					pTexture->ComputeMipLevelDimensions( iMip, &nWidth, &nHeight, &nDepth );
					if( pTexture->Format() == IMAGE_FORMAT_DXT1  || pTexture->Format() == IMAGE_FORMAT_DXT5 ||
						pTexture->Format() == IMAGE_FORMAT_ATI1N || pTexture->Format() == IMAGE_FORMAT_ATI2N )
					{
						unsigned char *pImageData = pTexture->ImageData( iFrame, iFace, iMip, 0, 0, 0 );
						FillCompressedTextureWithSingleColor( red, green, blue, 255, pImageData, nWidth, nHeight, nDepth, pTexture->Format() );
					}
					else
					{
						for ( int z = 0; z < nDepth; ++z )
						{
							CPixelWriter pixelWriter;
							pixelWriter.SetPixelMemory( pTexture->Format(), 
								pTexture->ImageData( iFrame, iFace, iMip, 0, 0, z ), pTexture->RowSizeInBytes( iMip ) );

							for (int y = 0; y < nHeight; ++y)
							{
								pixelWriter.Seek( 0, y );
								for (int x = 0; x < nWidth; ++x)
								{
									pixelWriter.WritePixel( red, green, blue, 255 );
								}
							}
						}
					}
				}
			}
		}
		break;
	}
}

//-----------------------------------------------------------------------------
// Generate a texture that shows the various mip levels
//-----------------------------------------------------------------------------
void CTexture::GenerateShowMipLevelsTextures( IVTFTexture *pTexture )
{
	if( pTexture->FaceCount() > 1 )
		return;

	switch( pTexture->Format() )
	{
	// These are formats that we don't bother with for generating mip level textures.
	case IMAGE_FORMAT_RGBA16161616F:
	case IMAGE_FORMAT_R32F:
	case IMAGE_FORMAT_RGB323232F:
	case IMAGE_FORMAT_RGBA32323232F:
	case IMAGE_FORMAT_UV88:
		break;
	default:
		for (int iFrame = 0; iFrame < pTexture->FrameCount(); ++iFrame )
		{
			for (int iFace = 0; iFace < pTexture->FaceCount(); ++iFace )
			{
				for (int iMip = 0; iMip < pTexture->MipCount(); ++iMip )
				{
					int green  =	( ( iMip + 1 ) & 1 ) ? 255 : 0;
					int red	   =	( ( iMip + 1 ) & 2 ) ? 255 : 0;
					int blue   =	( ( iMip + 1 ) & 4 ) ? 255 : 0;

					int nWidth, nHeight, nDepth;
					pTexture->ComputeMipLevelDimensions( iMip, &nWidth, &nHeight, &nDepth );
					if( pTexture->Format() == IMAGE_FORMAT_DXT1  || pTexture->Format() == IMAGE_FORMAT_DXT5 ||
					    pTexture->Format() == IMAGE_FORMAT_ATI1N || pTexture->Format() == IMAGE_FORMAT_ATI2N )
					{
						unsigned char *pImageData = pTexture->ImageData( iFrame, iFace, iMip, 0, 0, 0 );
						int alpha = 255;
						FillCompressedTextureWithSingleColor( red, green, blue, alpha, pImageData, nWidth, nHeight, nDepth, pTexture->Format() );
					}
					else
					{
						for ( int z = 0; z < nDepth; ++z )
						{
							CPixelWriter pixelWriter;
							pixelWriter.SetPixelMemory( pTexture->Format(), 
								pTexture->ImageData( iFrame, iFace, iMip, 0, 0, z ), pTexture->RowSizeInBytes( iMip ) );

							for (int y = 0; y < nHeight; ++y)
							{
								pixelWriter.Seek( 0, y );
								for (int x = 0; x < nWidth; ++x)
								{
									pixelWriter.WritePixel( red, green, blue, 255 );
								}
							}
						}
					}
				}
			}
		}
		break;
	}
}


//-----------------------------------------------------------------------------
// Generate a texture that shows the various mip levels
//-----------------------------------------------------------------------------
void CTexture::CopyLowResImageToTexture( IVTFTexture *pTexture )
{
	int nFlags = pTexture->Flags();
	nFlags |= TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_POINTSAMPLE;
	nFlags &= ~(TEXTUREFLAGS_TRILINEAR | TEXTUREFLAGS_ANISOTROPIC | TEXTUREFLAGS_HINT_DXT5);
	nFlags &= ~(TEXTUREFLAGS_NORMAL | TEXTUREFLAGS_ENVMAP);
	nFlags &= ~(TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA);

	Assert( pTexture->FrameCount() == 1 );

	Init( pTexture->Width(), pTexture->Height(), 1, IMAGE_FORMAT_BGR888, nFlags, 1 );
	pTexture->Init( m_LowResImageWidth, m_LowResImageHeight, 1, IMAGE_FORMAT_BGR888, nFlags, 1 );

	// Don't bother computing the actual size; it's actually equal to the low-res size
	// With only one mip level
	m_nActualWidth = m_LowResImageWidth;
	m_nActualHeight = m_LowResImageHeight;
	m_nActualDepth = 1;
	m_nActualMipCount = 1;

	// Copy the row-res image into the VTF Texture
	CPixelWriter pixelWriter;
	pixelWriter.SetPixelMemory( pTexture->Format(), 
		pTexture->ImageData( 0, 0, 0 ), pTexture->RowSizeInBytes( 0 ) );

#if !defined( _GAMECONSOLE )
	unsigned char *pLowResImage = m_pLowResImage;
#else
	unsigned char *pLowResImage = m_LowResImageSample;
#endif
	for ( int y = 0; y < m_LowResImageHeight; ++y )
	{
		pixelWriter.Seek( 0, y );
		for ( int x = 0; x < m_LowResImageWidth; ++x )
		{
			int red = pLowResImage[0];
			int green = pLowResImage[1];
			int blue = pLowResImage[2];
			pLowResImage += 3;

			pixelWriter.WritePixel( red, green, blue, 255 );
		}
	}
}

//-----------------------------------------------------------------------------
// Sets up debugging texture bits, if appropriate
//-----------------------------------------------------------------------------
bool CTexture::SetupDebuggingTextures( IVTFTexture *pVTFTexture )
{
	if ( IsGameConsole() )
	{
		// not supporting
		return false;
	}

	if ( pVTFTexture->Flags() & TEXTUREFLAGS_NODEBUGOVERRIDE )
		return false;

	if ( g_config.bDrawGray )
	{
		GenerateGrayTexture( pVTFTexture );
		return true;
	}

	if ( g_config.nShowMipLevels )
	{
		// mat_showmiplevels 1 means don't do normal maps
		if ( ( g_config.nShowMipLevels == 1 ) && ( pVTFTexture->Flags() & ( TEXTUREFLAGS_NORMAL | TEXTUREFLAGS_SSBUMP ) ) )
			return false;

		// mat_showmiplevels 2 means don't do base textures
		if ( ( g_config.nShowMipLevels == 2 ) && !( pVTFTexture->Flags() & ( TEXTUREFLAGS_NORMAL | TEXTUREFLAGS_SSBUMP ) ) )
			return false;

		// This mode shows the mip levels as different colors
		GenerateShowMipLevelsTextures( pVTFTexture );
		return true;
	}
	else if ( g_config.bShowLowResImage && pVTFTexture->FrameCount() == 1 && 
		pVTFTexture->FaceCount() == 1 && ((pVTFTexture->Flags() & TEXTUREFLAGS_NORMAL) == 0) &&
		m_LowResImageWidth != 0 && m_LowResImageHeight != 0 )
	{
		// This mode just uses the low res texture
		CopyLowResImageToTexture( pVTFTexture );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Converts the texture to the actual format
// Returns true if conversion applied, false otherwise
//-----------------------------------------------------------------------------
bool CTexture::ConvertToActualFormat( IVTFTexture *pVTFTexture )
{
	if ( !g_pShaderDevice->IsUsingGraphics() )
		return false;

	bool bConverted = false;

	ImageFormat fmt = m_ImageFormat;
	ImageFormat dstFormat = ComputeActualFormat( pVTFTexture->Format() );
#ifdef PLATFORM_OSX
	if ( IsVolumeTexture() && ImageLoader::IsCompressed( dstFormat ) )
	{
		// OSX does not support compressed 3d textures
		dstFormat = IMAGE_FORMAT_RGBA8888;
	}
#endif
	if ( fmt != dstFormat )
	{
		Assert( !IsGameConsole() );
		pVTFTexture->ConvertImageFormat( dstFormat, false );
		m_ImageFormat = dstFormat;
		bConverted = true;
	}
#ifndef _PS3
	// No reason to do this conversion on PS3
	else if ( HardwareConfig()->GetHDRType() == HDR_TYPE_INTEGER &&
		     fmt == dstFormat && dstFormat == IMAGE_FORMAT_RGBA16161616F )
	{
		// This is to force at most the precision of int16 for fp16 texture when running the integer path.
		pVTFTexture->ConvertImageFormat( IMAGE_FORMAT_RGBA16161616, false );
		pVTFTexture->ConvertImageFormat( IMAGE_FORMAT_RGBA16161616F, false );
		bConverted = true;
	}
#endif // !_PS3

	return bConverted;
}

void CTexture::GetFilename( char *pOut, int maxLen ) const
{
	const char *pName = m_Name.String();
	bool bIsUNCName = ( pName[0] == '/' && pName[1] == '/' && pName[2] != '/' );

	if ( !bIsUNCName )
	{
		Q_snprintf( pOut, maxLen, 
			"materials/%s" TEXTURE_FNAME_EXTENSION, pName );
	}
	else
	{
		Q_snprintf( pOut, maxLen, "%s" TEXTURE_FNAME_EXTENSION, pName );
	}
}


void CTexture::ReloadFilesInList( IFileList *pFilesToReload )
{
	if ( IsProcedural() || IsRenderTarget() )
		return;

	char filename[MAX_PATH];
	GetFilename( filename, sizeof( filename ) );
	if ( pFilesToReload->IsFileInList( filename ) )
	{
		Download();
	}
}


//-----------------------------------------------------------------------------
// Loads the texture bits from a file or data.
//-----------------------------------------------------------------------------
IVTFTexture *CTexture::LoadTexttureBitsFromFileOrData( void *pSourceData, int nSourceDataSize, char **pResolvedFilename )
{
	char pCacheFileName[MATERIAL_MAX_PATH] = { 0 };
	const char *pName;
	if (m_nInternalFlags & TEXTUREFLAGSINTERNAL_SHOULDEXCLUDE)
	{
#if !defined( _CERT )
		// excluded texture should not be visible, want these to be found during testing
		// use the green checkerboard
		pName = "dev/dev_exclude_error";
#else
		// for shipping (in case it happens) better to use the version meant for momentary rendering
		pName = "dev/dev_temp_exclude";
#endif
	}
	else if ((m_nInternalFlags & TEXTUREFLAGSINTERNAL_SHOULDTEMPEXCLUDE) && m_nDesiredTempDimensionLimit <= 0)
	{
		pName = "dev/dev_temp_exclude";
	}
	else
	{
		pName = m_Name.String();
	}

	bool bIsUNCName = (pName[0] == '/' && pName[1] == '/' && pName[2] != '/');
	if (!bIsUNCName)
	{
		Q_snprintf( pCacheFileName, sizeof( pCacheFileName ), "materials/%s" TEXTURE_FNAME_EXTENSION, pName );
	}
	else
	{
		Q_snprintf( pCacheFileName, sizeof( pCacheFileName ), "%s" TEXTURE_FNAME_EXTENSION, pName );
	}

	if (!pSourceData || !nSourceDataSize)
	{
		// Get the data from disk...
		// NOTE: Reloading the texture bits can cause the texture size, frames, format, pretty much *anything* can change.
		return LoadTextureBitsFromFile( pCacheFileName, pResolvedFilename );
	}
	else
	{
		// use the data provided
		return LoadTextureBitsFromData( pCacheFileName, pSourceData, nSourceDataSize );
	}
}

//-----------------------------------------------------------------------------
// Loads the texture bits from a file.
//-----------------------------------------------------------------------------
IVTFTexture *CTexture::LoadTextureBitsFromFile( char *pCacheFileName, char **ppResolvedFilename )
{
	int nHeaderSize;
	int	nFileSize;

	IVTFTexture *pVTFTexture = ( m_nFlags & TEXTUREFLAGS_ASYNC_DOWNLOAD ) ? GetScratchVTFAsyncTexture() : GetScratchVTFTexture();

	bool bIsCombinedImage = ( pCacheFileName[ 0 ] == '!' ) && g_pMDLCache != NULL;

	CUtlBuffer buf;
	FileHandle_t fileHandle = FILESYSTEM_INVALID_HANDLE;
	int nBytesOptimalRead;	// GCC needs this extra newline due to goto
	int nBytesRead;			// GCC needs this extra newline due to goto

	if ( bIsCombinedImage )
	{
		int		nSize;
		void	*pBuffer = g_pMDLCache->GetCombinedInternalAsset( COMBINED_ASSET_TEXTURE, pCacheFileName, &nSize );

		// buf will not own the memory and thus will not try to dealloc it
		buf.SetExternalBuffer( pBuffer, nSize, nSize );
	}
	else
	{
		while ( fileHandle == FILESYSTEM_INVALID_HANDLE )			// run until found a file or out of rules
		{
#if defined( _GAMECONSOLE )
			// generate native texture
			pVTFTexture->UpdateOrCreate( pCacheFileName );
#endif
			fileHandle = g_pFullFileSystem->OpenEx( pCacheFileName, "rb", 0, MaterialSystem()->GetForcedTextureLoadPathID(), ppResolvedFilename );
			if ( fileHandle == FILESYSTEM_INVALID_HANDLE )
			{
				// try any fallbacks.
				char *pHdrExt = Q_stristr( pCacheFileName, ".hdr" TEXTURE_FNAME_EXTENSION );
				if ( pHdrExt )
				{
					//DevWarning( "A custom HDR cubemap \"%s\": cannot be found on disk.\n"
					//			"This really should have a HDR version, trying a fall back to a non-HDR version.\n", pCacheFileName );
					strcpy( pHdrExt, TEXTURE_FNAME_EXTENSION );
				}
				else
				{
					// no more fallbacks
					break;
				}
			}
		}
	
		if ( fileHandle == FILESYSTEM_INVALID_HANDLE )
		{
			if ( !StringHasPrefix( m_Name.String(), "env_cubemap" ) )
			{
				if ( IsOSX() )
				{
					printf("\n ##### CTexture::LoadTextureBitsFromFile couldn't find %s",pCacheFileName );
				}
				DevWarning( "\"%s\": can't be found on disk\n", pCacheFileName );
			}
			return HandleFileLoadFailedTexture( pVTFTexture );
		}

		int nVersion = -1;
		if ( IsPC() )
			nVersion = VTF_MAJOR_VERSION;
		else if ( IsX360() )
			nVersion = VTF_X360_MAJOR_VERSION;
		else if ( IsPS3() )
			nVersion = VTF_PS3_MAJOR_VERSION;

		nHeaderSize = VTFFileHeaderSize( nVersion );

		// restrict read to the header only!
		// header provides info to avoid reading the entire file
		nBytesOptimalRead = GetOptimalReadBuffer( fileHandle, nHeaderSize, buf );
		nBytesRead = g_pFullFileSystem->ReadEx( buf.Base(), nBytesOptimalRead, Min( nHeaderSize, (int)g_pFullFileSystem->Size(fileHandle) ), fileHandle ); // only read as much as the file has
		nBytesRead = nHeaderSize = ((VTFFileBaseHeader_t *)buf.Base())->headerSize;
		g_pFullFileSystem->Seek( fileHandle, nHeaderSize, FILESYSTEM_SEEK_HEAD );
		buf.SeekPut( CUtlBuffer::SEEK_HEAD, nBytesRead );
	}

	// Unserialize the header only
	// need the header first to determine remainder of data
#if !defined( _GAMECONSOLE )
	if ( !pVTFTexture->Unserialize( buf, true ) )
#else
	if ( !pVTFTexture->UnserializeFromBuffer( buf, true, true, true, 0 ) )
#endif
	{
		Warning( "Error reading texture header \"%s\"\n", pCacheFileName );
		g_pFullFileSystem->Close( fileHandle );
		return HandleFileLoadFailedTexture( pVTFTexture );
	}

	// FIXME: Hack for L4D
	int nHackExtraFlags = 0;
	if ( !Q_strnicmp( pCacheFileName, "materials/graffiti/", 19 ) )
	{
		nHackExtraFlags = TEXTUREFLAGS_NOLOD; 
	}
	
	// OSX hackery
	if ( m_nFlags & TEXTUREFLAGS_SRGB )
	{
		nHackExtraFlags |= TEXTUREFLAGS_SRGB; 		
	}

	// Set from stdshaders cpp code
	if ( m_nFlags & TEXTUREFLAGS_ANISOTROPIC )
	{
		nHackExtraFlags |= TEXTUREFLAGS_ANISOTROPIC;
	}

	// Seek the reading back to the front of the buffer
	buf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );

	// Initialize the texture class with vtf header data before operations
	Init( 
#if !defined( _GAMECONSOLE )
		pVTFTexture->Width(), 
		pVTFTexture->Height(), 
		pVTFTexture->Depth(), 
#else
		// 360 texture might be pre-picmipped, setup as it's original dimensions
		// so picmipping logic calculates correctly, and then fixup
		pVTFTexture->MappingWidth(),
		pVTFTexture->MappingHeight(),
		pVTFTexture->MappingDepth(),
#endif
		pVTFTexture->Format(), 
		pVTFTexture->Flags() | nHackExtraFlags, 
		pVTFTexture->FrameCount() );

	VectorCopy( pVTFTexture->Reflectivity(), m_vecReflectivity );

#if defined( _GAMECONSOLE )
	m_nInternalFlags |= TEXTUREFLAGSINTERNAL_QUEUEDLOAD;
	if ( !g_pQueuedLoader->IsMapLoading() || ( m_nFlags & ( TEXTUREFLAGS_PROCEDURAL|TEXTUREFLAGS_RENDERTARGET|TEXTUREFLAGS_DEPTHRENDERTARGET ) ) )
	{
		// explicitly disabled or not appropriate for texture type
		m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_QUEUEDLOAD;
	}
	else
	{
		if ( pVTFTexture->FileSize( true, 0 ) >= pVTFTexture->FileSize( false, 0 ) )
		{
			// texture is a dwarf, entirely in preload, loads normally
			m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_QUEUEDLOAD;
		}
	}
#endif

	// Compute the actual texture dimensions
	int nMipSkipCount = ComputeActualSize( false, pVTFTexture );

#if defined( _GAMECONSOLE )
	bool bQueuedLoad = ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_QUEUEDLOAD ) != 0;
	nMipSkipCount -= pVTFTexture->MipSkipCount();
	if ( nMipSkipCount < 0 || ( nMipSkipCount >= pVTFTexture->MipCount() ) )
	{
		// the 360 texture was already pre-picmipped or can't be picmipped
		// clamp to the available dimensions
		m_nActualWidth = pVTFTexture->Width();
		m_nActualHeight = pVTFTexture->Height();
		m_nActualDepth = pVTFTexture->Depth();
		m_nActualMipCount = ComputeActualMipCount();
		nMipSkipCount = 0;
	}
	if ( IsX360() && g_config.skipMipLevels == 0 && m_nActualMipCount > 1 && m_nFrameCount == 1 && !( m_nFlags & ( TEXTUREFLAGS_PROCEDURAL|TEXTUREFLAGS_RENDERTARGET|TEXTUREFLAGS_DEPTHRENDERTARGET ) ) )
	{
		// this file based texture is a good candidate for cacheing
		m_nInternalFlags |= TEXTUREFLAGSINTERNAL_CACHEABLE;
	}
	if ( nMipSkipCount )
	{
		// track which textures had their dimensions forcefully reduced
		m_nInternalFlags |= TEXTUREFLAGSINTERNAL_REDUCED;
	}
#endif
	
	m_nMipSkipCount = nMipSkipCount;

#if !defined( _GAMECONSOLE )
	// Determine how much of the file to read in
	nFileSize = pVTFTexture->FileSize( nMipSkipCount );
#else
	// A queued loading texture just gets the preload section
	// and does NOT unserialize the texture bits here
	nFileSize = pVTFTexture->FileSize( bQueuedLoad, nMipSkipCount );
#endif

	// Read only the portion of the file that we care about
	g_pFullFileSystem->Seek( fileHandle, 0, FILESYSTEM_SEEK_HEAD );
	nBytesOptimalRead = GetOptimalReadBuffer( fileHandle, nFileSize, buf );
	nBytesRead = g_pFullFileSystem->ReadEx( buf.Base(), nBytesOptimalRead, nFileSize, fileHandle );
	g_pFullFileSystem->Close( fileHandle );
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, nBytesRead );

	// NOTE: Skipping mip levels here will cause the size to be changed...
#if !defined( _GAMECONSOLE )
	if ( !pVTFTexture->Unserialize( buf, false, nMipSkipCount ) )
#else
	if ( !pVTFTexture->UnserializeFromBuffer( buf, true, false, bQueuedLoad, nMipSkipCount ) )
#endif
	{
		Warning( "Error reading material data \"%s\"\n", pCacheFileName );
		return HandleFileLoadFailedTexture( pVTFTexture );
	}

	// Build the low-res texture
	LoadLowResTexture( pVTFTexture );

	// load resources
	LoadResourceData( pVTFTexture );

	// Try to set up debugging textures, if we're in a debugging mode
	if ( !IsProcedural() && !IsGameConsole() )
	{
		SetupDebuggingTextures( pVTFTexture );
	}

	if ( ConvertToActualFormat( pVTFTexture ) )
	{
		if ( IsGameConsole() )
		{
			// 360 vtf are baked in final formats, no format conversion can or should have occurred
			// otherwise track offender and ensure files are baked correctly
			Error( "\"%s\" not in native format\n", pCacheFileName );
		}
	}

	return pVTFTexture;
}

//-----------------------------------------------------------------------------
// Loads the texture bits from provided data.
//-----------------------------------------------------------------------------
IVTFTexture *CTexture::LoadTextureBitsFromData( char *pCacheFileName, void *pSourceData, int nSourceDataSize )
{
	IVTFTexture *pVTFTexture = ( m_nFlags & TEXTUREFLAGS_ASYNC_DOWNLOAD ) ? GetScratchVTFAsyncTexture() : GetScratchVTFTexture();

	CUtlBuffer buf;
	buf.SetExternalBuffer( pSourceData, nSourceDataSize, nSourceDataSize, CUtlBuffer::READ_ONLY );

	int nVersion = -1;
	if ( IsPC() )
		nVersion = VTF_MAJOR_VERSION;
	else if ( IsX360() )
		nVersion = VTF_X360_MAJOR_VERSION;
	else if ( IsPS3() )
		nVersion = VTF_PS3_MAJOR_VERSION;

	int nHeaderSize = VTFFileHeaderSize( nVersion );
	if ( nSourceDataSize < nHeaderSize )
	{
		Warning( "Error reading texture header \"%s\"\n", pCacheFileName );
		return HandleFileLoadFailedTexture( pVTFTexture );
	}

	// Unserialize the header only
	// need the header first to determine remainder of data
#if !defined( _GAMECONSOLE )
	if ( !pVTFTexture->Unserialize( buf, true ) )
#else
	if ( !pVTFTexture->UnserializeFromBuffer( buf, true, true, true, 0 ) )
#endif
	{
		Warning( "Error reading texture header \"%s\"\n", pCacheFileName );
		return HandleFileLoadFailedTexture( pVTFTexture );
	}

	// FIXME: Hack for L4D
	int nHackExtraFlags = 0;
	if ( !Q_strnicmp( pCacheFileName, "materials/graffiti/", 19 ) )
	{
		nHackExtraFlags = TEXTUREFLAGS_NOLOD; 
	}

	// OSX hackery
	if ( m_nFlags & TEXTUREFLAGS_SRGB )
	{
		nHackExtraFlags |= TEXTUREFLAGS_SRGB; 		
	}

	// Set from stdshaders cpp code
	if ( m_nFlags & TEXTUREFLAGS_ANISOTROPIC )
	{
		nHackExtraFlags |= TEXTUREFLAGS_ANISOTROPIC;
	}

	if ( m_nFlags & TEXTUREFLAGS_ASYNC_DOWNLOAD )
	{
		nHackExtraFlags |= TEXTUREFLAGS_ASYNC_DOWNLOAD;
	}

	// Seek the reading back to the front of the buffer
	buf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );

	// Initialize the texture class with vtf header data before operations
	Init( 
#if !defined( _GAMECONSOLE )
		pVTFTexture->Width(), 
		pVTFTexture->Height(), 
		pVTFTexture->Depth(), 
#else
		// 360 texture might be pre-picmipped, setup as it's original dimensions
		// so picmipping logic calculates correctly, and then fixup
		pVTFTexture->MappingWidth(),
		pVTFTexture->MappingHeight(),
		pVTFTexture->MappingDepth(),
#endif
		pVTFTexture->Format(), 
		pVTFTexture->Flags() | nHackExtraFlags, 
		pVTFTexture->FrameCount() );

	VectorCopy( pVTFTexture->Reflectivity(), m_vecReflectivity );

	// Compute the actual texture dimensions
	int nMipSkipCount = ComputeActualSize( false, pVTFTexture );

#if defined( _GAMECONSOLE )
	bool bQueuedLoad = ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_QUEUEDLOAD ) != 0;
	nMipSkipCount -= pVTFTexture->MipSkipCount();
	if ( nMipSkipCount < 0 || ( nMipSkipCount >= pVTFTexture->MipCount() ) )
	{
		// the 360 texture was already pre-picmipped or can't be picmipped
		// clamp to the available dimensions
		m_nActualWidth = pVTFTexture->Width();
		m_nActualHeight = pVTFTexture->Height();
		m_nActualDepth = pVTFTexture->Depth();
		m_nActualMipCount = ComputeActualMipCount();
		nMipSkipCount = 0;
	}
	if ( IsX360() && g_config.skipMipLevels == 0 && m_nActualMipCount > 1 && m_nFrameCount == 1 && !( m_nFlags & ( TEXTUREFLAGS_PROCEDURAL|TEXTUREFLAGS_RENDERTARGET|TEXTUREFLAGS_DEPTHRENDERTARGET ) ) )
	{
		// this file based texture is a good candidate for cacheing
		m_nInternalFlags |= TEXTUREFLAGSINTERNAL_CACHEABLE;
	}
	if ( nMipSkipCount )
	{
		// track which textures had their dimensions forcefully reduced
		m_nInternalFlags |= TEXTUREFLAGSINTERNAL_REDUCED;
	}
#endif

	m_nMipSkipCount = nMipSkipCount;

	// NOTE: Skipping mip levels here will cause the size to be changed...
#if !defined( _GAMECONSOLE )
	if ( !pVTFTexture->Unserialize( buf, false, nMipSkipCount ) )
#else
	if ( !pVTFTexture->UnserializeFromBuffer( buf, true, false, bQueuedLoad, nMipSkipCount ) )
#endif
	{
		Warning( "Error reading texture data \"%s\"\n", pCacheFileName );
		return HandleFileLoadFailedTexture( pVTFTexture );
	}

	// Build the low-res texture
	LoadLowResTexture( pVTFTexture );

	// Load the resources
	LoadResourceData( pVTFTexture );

	// Try to set up debugging textures, if we're in a debugging mode
	if ( !IsProcedural() && !IsGameConsole() )
	{
		SetupDebuggingTextures( pVTFTexture );
	}

	if ( ConvertToActualFormat( pVTFTexture ) )
	{
		if ( IsGameConsole() )
		{
			// 360 vtf are baked in final formats, no format conversion can or should have occurred
			// otherwise track offender and ensure files are baked correctly
			Error( "\"%s\" not in native format\n", pCacheFileName );
		}
	}

	return pVTFTexture;
}

IVTFTexture *CTexture::HandleFileLoadFailedTexture( IVTFTexture *pVTFTexture )
{
	// create the error texture
#if defined( _GAMECONSOLE )
	// reset botched vtf, ensure checkerboard error texture is created now and maintains bgra8888 format
	pVTFTexture->ReleaseImageMemory();
	m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_QUEUEDLOAD;
	m_nFlags |= TEXTUREFLAGS_EIGHTBITALPHA;
#endif

	// This will make a checkerboard texture to indicate failure
	pVTFTexture->Init( 32, 32, 1, IMAGE_FORMAT_BGRA8888, m_nFlags, 1 );
	Init( pVTFTexture->Width(), pVTFTexture->Height(), pVTFTexture->Depth(), pVTFTexture->Format(), 
		  pVTFTexture->Flags(), pVTFTexture->FrameCount() );
	m_vecReflectivity.Init( 0.5f, 0.5f, 0.5f );

	// NOTE: For mat_picmip to work, we must use the same size (32x32)
	// Which should work since every card can handle textures of that size
	m_nActualWidth = pVTFTexture->Width();
	m_nActualHeight = pVTFTexture->Height();
	m_nActualMipCount = 1;

	// generate the checkerboard
	TextureManager()->GenerateErrorTexture( this, pVTFTexture );
	ConvertToActualFormat( pVTFTexture );

	// Deactivate procedural texture...
	m_nFlags &= ~TEXTUREFLAGS_PROCEDURAL;
	m_nInternalFlags |= TEXTUREFLAGSINTERNAL_ERROR;

	return pVTFTexture;
}

//-----------------------------------------------------------------------------
// Computes subrect for a particular miplevel
//-----------------------------------------------------------------------------
void CTexture::ComputeMipLevelSubRect( const Rect_t* pSrcRect, int nMipLevel, Rect_t *pSubRect )
{
	if (nMipLevel == 0)
	{
		*pSubRect = *pSrcRect;
		return;
	}

	float flInvShrink = 1.0f / (float)(1 << nMipLevel);
	pSubRect->x = pSrcRect->x * flInvShrink;
	pSubRect->y = pSrcRect->y * flInvShrink;
	pSubRect->width = (int)ceil( (pSrcRect->x + pSrcRect->width) * flInvShrink ) - pSubRect->x;
	pSubRect->height = (int)ceil( (pSrcRect->y + pSrcRect->height) * flInvShrink ) - pSubRect->y;
}


//-----------------------------------------------------------------------------
// Computes the face count + first face
//-----------------------------------------------------------------------------
void CTexture::GetDownloadFaceCount( int &nFirstFace, int &nFaceCount )
{
	nFaceCount = 1;
	nFirstFace = 0;
	if ( IsCubeMap() )
	{
		nFaceCount = CUBEMAP_FACE_COUNT;
	}
}

//-----------------------------------------------------------------------------
// Fixup a queue loaded texture with the delayed hi-res data
//-----------------------------------------------------------------------------
void CTexture::FixupTexture( const void *pData, int nSize, LoaderError_t loaderError )
{
	if ( loaderError != LOADERERROR_NONE )
	{
		// mark as invalid
		nSize = 0;
	}

	m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_QUEUEDLOAD;

	// Make sure we've actually allocated the texture handles
	Assert( HasBeenAllocated() );

#if defined( _GAMECONSOLE )
	// hand off the hires data down to the shaderapi to upload directly
	// Purposely bypassing downloading through material system, which is non-reentrant
	// for that operation, to avoid mutexing.

	// NOTE: Strange refcount work here to keep it threadsafe
	int nRefCount = m_nRefCount;
	int nRefCountOld = nRefCount;
	g_pShaderAPI->PostQueuedTexture( 
					pData, 
					nSize, 
					m_pTextureHandles, 
					m_nFrameCount,
					m_nActualWidth,
					m_nActualHeight,
					m_nActualDepth,
					m_nActualMipCount,
					&nRefCount );
	int nDelta = nRefCount - nRefCountOld;
	m_nRefCount += nDelta;
#endif
} 
static void QueuedLoaderCallback( void *pContext, void *pContext2, const void *pData, int nSize, LoaderError_t loaderError )
{
	reinterpret_cast< CTexture * >( pContext )->FixupTexture( pData, nSize, loaderError );
}

//-----------------------------------------------------------------------------
// Generates the procedural bits
//-----------------------------------------------------------------------------
IVTFTexture *CTexture::ReconstructPartialProceduralBits( const Rect_t *pRect, Rect_t *pActualRect )
{
	// Figure out the actual size for this texture based on the current mode
	ComputeActualSize();

	// Figure out how many mip levels we're skipping...
	int nSizeFactor = 1;
	int nWidth = GetActualWidth();
	if ( nWidth != 0 )
	{
		nSizeFactor = GetMappingWidth() / nWidth;
	}
	int nMipSkipCount = 0;
	while (nSizeFactor > 1)
	{
		nSizeFactor >>= 1;
		++nMipSkipCount;
	}

	// Determine a rectangle appropriate for the actual size...
	// It must bound all partially-covered pixels..
	ComputeMipLevelSubRect( pRect, nMipSkipCount, pActualRect );

	if ( IsGameConsole() && !IsDebug() && !m_pTextureRegenerator )
	{
		// no checkerboards in 360 release
		return NULL;
	}

	bool bUsePreallocatedScratchTexture = m_pTextureRegenerator && m_pTextureRegenerator->HasPreallocatedScratchTexture();
	
	// Create the texture
	IVTFTexture *pVTFTexture = bUsePreallocatedScratchTexture ? m_pTextureRegenerator->GetPreallocatedScratchTexture() : GetScratchVTFTexture();

	// Initialize the texture
	pVTFTexture->Init( m_nActualWidth, m_nActualHeight, m_nActualDepth,
		ComputeActualFormat( m_ImageFormat ), m_nFlags, m_nFrameCount );

	// Generate the bits from the installed procedural regenerator
	if ( m_pTextureRegenerator )
	{
		m_pTextureRegenerator->RegenerateTextureBits( this, pVTFTexture, pActualRect );
	}
	else
	{
		// In this case, we don't have one, so just use a checkerboard...
		TextureManager()->GenerateErrorTexture( this, pVTFTexture );
	}

	return pVTFTexture;
}


//-----------------------------------------------------------------------------
// Regenerates the bits of a texture within a particular rectangle
//-----------------------------------------------------------------------------
void CTexture::ReconstructPartialTexture( const Rect_t *pRect )
{
	// FIXME: for now, only procedural textures can handle sub-rect specification.
	Assert( IsProcedural() );

	// Also, we need procedural textures that have only a single copy!!
	// Otherwise this partial upload will not occur on all copies
	Assert( m_nFlags & TEXTUREFLAGS_SINGLECOPY );

	Rect_t vtfRect;
	IVTFTexture *pVTFTexture = ReconstructPartialProceduralBits( pRect, &vtfRect );

	// FIXME: for now, depth textures do not work with this.
	Assert( pVTFTexture->Depth() == 1 );

	// Make sure we've allocated the API textures
	if ( !HasBeenAllocated() )
	{
		if ( !AllocateShaderAPITextures() )
			return;
	}

	if ( IsGameConsole() && !pVTFTexture )
	{
		// 360 inhibited procedural generation
		return;
	}

	int nFaceCount, nFirstFace;
	GetDownloadFaceCount( nFirstFace, nFaceCount );
	
	// Blit down portions of the various VTF frames into the board memory
	int nStride;
	Rect_t mipRect;
	for ( int iFrame = 0; iFrame < m_nFrameCount; ++iFrame )
	{
		Modify( iFrame );

		for ( int iFace = 0; iFace < nFaceCount; ++iFace )
		{
			for ( int iMip = 0; iMip < m_nActualMipCount;  ++iMip )
			{
				pVTFTexture->ComputeMipLevelSubRect( &vtfRect, iMip, &mipRect );
				nStride = pVTFTexture->RowSizeInBytes( iMip );
				unsigned char *pBits = pVTFTexture->ImageData( iFrame, iFace + nFirstFace, iMip, mipRect.x, mipRect.y, 0 );
				g_pShaderAPI->TexSubImage2D( 
					iMip, 
					iFace, 
					mipRect.x, 
					mipRect.y,
					0,
					mipRect.width, 
					mipRect.height, 
					pVTFTexture->Format(), 
					nStride, 
#if defined( _GAMECONSOLE )
					pVTFTexture->IsPreTiled(),
#else
					false,
#endif
					pBits );
			}
		}
	}

#if defined( _GAMECONSOLE )
	if ( IsProcedural() && m_pTextureRegenerator && m_pTextureRegenerator->HasPreallocatedScratchTexture() )
	{
		// nothing to free; we used the pre-allocated scratch texture
	}
	else
	{
		// hint the vtf system to release memory associated with its load
		pVTFTexture->ReleaseImageMemory();
	}
#endif
}


//-----------------------------------------------------------------------------
// Generates the procedural bits
//-----------------------------------------------------------------------------
IVTFTexture *CTexture::ReconstructProceduralBits()
{
	// Figure out the actual size for this texture based on the current mode
	ComputeActualSize();

	if ( IsGameConsole() && !IsDebug() && !m_pTextureRegenerator )
	{
		// no checkerboards in 360 release
		return NULL;
	}

	bool bUsePreallocatedScratchTexture = m_pTextureRegenerator && m_pTextureRegenerator->HasPreallocatedScratchTexture();

	// Create the texture
	IVTFTexture *pVTFTexture = bUsePreallocatedScratchTexture ? m_pTextureRegenerator->GetPreallocatedScratchTexture() : GetScratchVTFTexture();

	// Initialize the texture
	pVTFTexture->Init( m_nActualWidth, m_nActualHeight, m_nActualDepth,
		ComputeActualFormat( m_ImageFormat ), m_nFlags, m_nFrameCount );

	// Generate the bits from the installed procedural regenerator
	if ( m_pTextureRegenerator )
	{
		Rect_t rect;
		rect.x = 0; rect.y = 0;
		rect.width = m_nActualWidth; 
		rect.height = m_nActualHeight; 
		m_pTextureRegenerator->RegenerateTextureBits( this, pVTFTexture, &rect );
	}
	else if ( !ImageLoader::IsFloatFormat( m_ImageFormat ) && !ImageLoader::IsRuntimeCompressed( m_ImageFormat ) )
	{
		// In this case, we don't have one, so just use a checkerboard...
		TextureManager()->GenerateErrorTexture( this, pVTFTexture );
	}

	return pVTFTexture;
}

void CTexture::WriteDataToShaderAPITexture( int nFrameCount, int nFaceCount, int nFirstFace, int nMipCount, IVTFTexture *pVTFTexture, ImageFormat fmt )
{
	for ( int iFrame = 0; iFrame < m_nFrameCount; ++iFrame )
	{
		Modify( iFrame );

		for ( int iFace = 0; iFace < nFaceCount; ++iFace )
		{
			for ( int iMip = 0; iMip < nMipCount; ++iMip )
			{
				unsigned char *pBits;
				int nWidth, nHeight, nDepth;
				pVTFTexture->ComputeMipLevelDimensions( iMip, &nWidth, &nHeight, &nDepth );
				for ( int z = 0; z < nDepth; ++z )
				{
					pBits = pVTFTexture->ImageData( iFrame, iFace + nFirstFace, iMip, 0, 0, z );

					g_pShaderAPI->TexImage2D( iMip, iFace, fmt, z, nWidth, nHeight, pVTFTexture->Format(), false, pBits );
				}
			}
		}
	}
}

bool CTexture::IsDepthTextureFormat( ImageFormat fmt )
{
	return ( ( m_ImageFormat == IMAGE_FORMAT_D16_SHADOW  ) || 
			 ( m_ImageFormat == IMAGE_FORMAT_D24X8_SHADOW ) ||
			 ( m_ImageFormat == IMAGE_FORMAT_D24S8 ) );
}


//-----------------------------------------------------------------------------
// Sets or updates the texture bits
//-----------------------------------------------------------------------------
void CTexture::ReconstructTexture( void *pSourceData, int nSourceDataSize )
{
	int oldWidth = m_nActualWidth;
	int oldHeight = m_nActualHeight;
	int oldDepth = m_nActualDepth;
	int oldMipCount = m_nActualMipCount;
	int oldFrameCount = m_nFrameCount;

	// FIXME: Should RenderTargets be a special case of Procedural?
	char *pResolvedFilename = NULL;
	IVTFTexture *pVTFTexture = NULL;

	if ( IsProcedural() )
	{
		// This will call the installed texture bit regeneration interface
		pVTFTexture = ReconstructProceduralBits();
	}
	else if ( IsRenderTarget() )
	{
		// Compute the actual size + format based on the current mode
		ComputeActualSize( true );
	}
	else
	{
		pVTFTexture = LoadTexttureBitsFromFileOrData( pSourceData, nSourceDataSize, &pResolvedFilename );
	}

	if ( !HasBeenAllocated() ||
		m_nActualWidth != oldWidth ||
		m_nActualHeight != oldHeight ||
		m_nActualDepth != oldDepth ||
		m_nActualMipCount != oldMipCount ||
		m_nFrameCount != oldFrameCount )
	{
		if ( HasBeenAllocated() )
		{
			// This is necessary for the reload case, we may discover there
			// are more frames of a texture animation, for example, which means
			// we can't rely on having the same number of texture frames.
			FreeShaderAPITextures();
		}

		// Create the shader api textures, except temp render targets on 360.
		if ( !( IsX360() && IsTempRenderTarget() ) )
		{
			if ( !AllocateShaderAPITextures() )
				return;
		}
	}

	// Render Targets just need to be cleared, they have no upload
	if ( IsRenderTarget() )
	{
		// Clear the render target to opaque black
#if !defined( _GAMECONSOLE )

		// Only clear if we're not a depth-stencil texture
		if ( !IsDepthTextureFormat( m_ImageFormat ) )
		{
			CMatRenderContextPtr pRenderContext( MaterialSystem() );
			ITexture *pThisTexture = GetEmbeddedTexture( 0 );
			pRenderContext->PushRenderTargetAndViewport( pThisTexture );						// Push this texture on the stack
			g_pShaderAPI->ClearColor4ub( 0, 0, 0, 0xFF );										// Set the clear color to opaque black
			g_pShaderAPI->ClearBuffers( true, false, false, m_nActualWidth, m_nActualHeight );	// Clear the target
			pRenderContext->PopRenderTargetAndViewport();										// Pop back to previous target
		}
#else
		// 360 may not have RT surface during init time
		// avoid complex conditionalizing, just cpu clear it, which always works
		ClearTexture( 0, 0, 0, 0xFF );
#endif
		// no upload
		return;
	}

	if ( IsGameConsole() && IsProcedural() && !pVTFTexture )
	{
		// 360 explicitly inhibited this texture's procedural generation, so no upload needed
		return;
	}

	// Blit down the texture faces, frames, and mips into the board memory
	int nFirstFace, nFaceCount;
	GetDownloadFaceCount( nFirstFace, nFaceCount );
	
	if ( IsPC() )
	{
		WriteDataToShaderAPITexture( m_nFrameCount, nFaceCount, nFirstFace, m_nActualMipCount, pVTFTexture, m_ImageFormat );
	}

#if defined( _GAMECONSOLE )
	bool bDoUpload = true;
	if ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_QUEUEDLOAD )
	{
		// the vtf didn't load any d3d bits, the hires bits will arrive before gameplay
		bDoUpload = false;
	}

	if ( bDoUpload )
	{
		for ( int iFrame = 0; iFrame < m_nFrameCount; ++iFrame )
		{
			Modify( iFrame );
			for ( int iFace = 0; iFace < nFaceCount; ++iFace )
			{
				for ( int iMip = 0; iMip < m_nActualMipCount; ++iMip )
				{
					unsigned char *pBits;
					int nWidth, nHeight, nDepth;
					pVTFTexture->ComputeMipLevelDimensions( iMip, &nWidth, &nHeight, &nDepth );
#ifdef _PS3
					// PS3 textures are pre-swizzled at tool time
					pBits = pVTFTexture->ImageData( iFrame, iFace + nFirstFace, iMip, 0, 0, 0 );
					g_pShaderAPI->TexImage2D( iMip, iFace, m_ImageFormat, nDepth > 1 ? nDepth : 0, nWidth, nHeight,
						pVTFTexture->Format(), false, pBits );
#else // _PS3
					pBits = pVTFTexture->ImageData( iFrame, iFace + nFirstFace, iMip, 0, 0, 0 );
					g_pShaderAPI->TexImage2D( iMip, iFace, m_ImageFormat, 0, nWidth, nHeight, 
						pVTFTexture->Format(), pVTFTexture->IsPreTiled(), pBits );
#endif // !_PS3
				}
			}
		}
	}

#ifdef _X360
	if ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_CACHEABLE )
	{
		// Make sure we've actually allocated the texture handles
		Assert( HasBeenAllocated() );

		// a cacheing texture needs to know how to get its bits back
		g_pShaderAPI->SetCacheableTextureParams( m_pTextureHandles, m_nFrameCount, pResolvedFilename, m_nMipSkipCount );
	}
#endif // _X360

	if ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_QUEUEDLOAD )
	{
		// the empty hires version was setup
		// the hires d3d bits will be delivered before gameplay (or render)
		LoaderPriority_t priority = LOADERPRIORITY_BEFOREPLAY;
		
		// add the job 
		LoaderJob_t loaderJob;
		loaderJob.m_pFilename = pResolvedFilename;
		loaderJob.m_pCallback = QueuedLoaderCallback;
		loaderJob.m_pContext = (void *)this;
		loaderJob.m_Priority =  priority;
		g_pQueuedLoader->AddJob( &loaderJob );
	}

	if ( IsProcedural() && m_pTextureRegenerator && m_pTextureRegenerator->HasPreallocatedScratchTexture() )
	{
		// nothing to free; we used the pre-allocated scratch texture
	}
	else
	{
		// hint the vtf system to release memory associated with its load
		pVTFTexture->ReleaseImageMemory();
	}
#endif // _GAMECONSOLE

	delete [] pResolvedFilename;

	// the 360 does not persist a large buffer
	// the pc can afford to persist a large buffer
	FreeOptimalReadBuffer( IsGameConsole() ? 32*1024 : 6*1024*1024 );
}

// Get the shaderapi texture handle associated w/ a particular frame
ShaderAPITextureHandle_t CTexture::GetTextureHandle( int nFrame, int nTextureChannel )
{
	if ( nFrame < 0 )
	{
		nFrame = 0;
		Warning( "CTexture::GetTextureHandle(): nFrame is < 0!\n" );
	}
	if ( nFrame >= m_nFrameCount )
	{
		// NOTE: This can happen during alt-tab.  If you alt-tab while loading a level then the first local cubemap bind will do this, for example.
		Assert( nFrame < m_nFrameCount );
		return INVALID_SHADERAPI_TEXTURE_HANDLE;
	}
	Assert( nTextureChannel < 2 );

	// Make sure we've actually allocated the texture handles
	Assert( HasBeenAllocated() );
	if ( m_pTextureHandles == NULL || !HasBeenAllocated() )
	{
		return INVALID_SHADERAPI_TEXTURE_HANDLE;
	}

	return m_pTextureHandles[nFrame];
}

void CTexture::GetLowResColorSample( float s, float t, float *color ) const
{
	if ( m_LowResImageWidth <= 0 || m_LowResImageHeight <= 0 )
	{
//		Warning( "Programming error: GetLowResColorSample \"%s\": %dx%d\n", m_pName, ( int )m_LowResImageWidth, ( int )m_LowResImageHeight );
		return;
	}

#if !defined( _GAMECONSOLE )
	// force s and t into [0,1)
	if ( s < 0.0f )
	{
		s = ( 1.0f - ( float )( int )s ) + s;
	}
	if ( t < 0.0f )
	{
		t = ( 1.0f - ( float )( int )t ) + t;
	}
	s = s - ( float )( int )s;
	t = t - ( float )( int )t;
	
	s *= m_LowResImageWidth;
	t *= m_LowResImageHeight;
	
	int wholeS, wholeT;
	wholeS = ( int )s;
	wholeT = ( int )t;
	float fracS, fracT;
	fracS = s - ( float )( int )s;
	fracT = t - ( float )( int )t;
	
	// filter twice in the s dimension.
	float sColor[2][3];
	int wholeSPlusOne = ( wholeS + 1 ) % m_LowResImageWidth;
	int wholeTPlusOne = ( wholeT + 1 ) % m_LowResImageHeight;
	sColor[0][0] = ( 1.0f - fracS ) * ( m_pLowResImage[( wholeS + wholeT * m_LowResImageWidth ) * 3 + 0] * ( 1.0f / 255.0f ) );
	sColor[0][1] = ( 1.0f - fracS ) * ( m_pLowResImage[( wholeS + wholeT * m_LowResImageWidth ) * 3 + 1] * ( 1.0f / 255.0f ) );
	sColor[0][2] = ( 1.0f - fracS ) * ( m_pLowResImage[( wholeS + wholeT * m_LowResImageWidth ) * 3 + 2] * ( 1.0f / 255.0f ) );
	sColor[0][0] += fracS * ( m_pLowResImage[( wholeSPlusOne + wholeT * m_LowResImageWidth ) * 3 + 0] * ( 1.0f / 255.0f ) );
	sColor[0][1] += fracS * ( m_pLowResImage[( wholeSPlusOne + wholeT * m_LowResImageWidth ) * 3 + 1] * ( 1.0f / 255.0f ) );
	sColor[0][2] += fracS * ( m_pLowResImage[( wholeSPlusOne + wholeT * m_LowResImageWidth ) * 3 + 2] * ( 1.0f / 255.0f ) );

	sColor[1][0] = ( 1.0f - fracS ) * ( m_pLowResImage[( wholeS + wholeTPlusOne * m_LowResImageWidth ) * 3 + 0] * ( 1.0f / 255.0f ) );
	sColor[1][1] = ( 1.0f - fracS ) * ( m_pLowResImage[( wholeS + wholeTPlusOne * m_LowResImageWidth ) * 3 + 1] * ( 1.0f / 255.0f ) );
	sColor[1][2] = ( 1.0f - fracS ) * ( m_pLowResImage[( wholeS + wholeTPlusOne * m_LowResImageWidth ) * 3 + 2] * ( 1.0f / 255.0f ) );
	sColor[1][0] += fracS * ( m_pLowResImage[( wholeSPlusOne + wholeTPlusOne * m_LowResImageWidth ) * 3 + 0] * ( 1.0f / 255.0f ) );
	sColor[1][1] += fracS * ( m_pLowResImage[( wholeSPlusOne + wholeTPlusOne * m_LowResImageWidth ) * 3 + 1] * ( 1.0f / 255.0f ) );
	sColor[1][2] += fracS * ( m_pLowResImage[( wholeSPlusOne + wholeTPlusOne * m_LowResImageWidth ) * 3 + 2] * ( 1.0f / 255.0f ) );

	color[0] = sColor[0][0] * ( 1.0f - fracT ) + sColor[1][0] * fracT;
	color[1] = sColor[0][1] * ( 1.0f - fracT ) + sColor[1][1] * fracT;
	color[2] = sColor[0][2] * ( 1.0f - fracT ) + sColor[1][2] * fracT;
#else
	color[0] = (float)m_LowResImageSample[0] * 1.0f/255.0f;
	color[1] = (float)m_LowResImageSample[1] * 1.0f/255.0f;
	color[2] = (float)m_LowResImageSample[2] * 1.0f/255.0f;
#endif
}

int CTexture::GetApproximateVidMemBytes( void ) const
{
	ImageFormat format = GetImageFormat();
	int width = GetActualWidth();
	int height = GetActualHeight();
	int depth = GetActualDepth();
	int numFrames = GetNumAnimationFrames();
	bool isMipmapped = IsMipmapped();

	return numFrames * ImageLoader::GetMemRequired( width, height, depth, format, isMipmapped );
}

void CTexture::CopyFrameBufferToMe( int nRenderTargetID, Rect_t *pSrcRect, Rect_t *pDstRect )
{
	Assert( m_pTextureHandles && m_nFrameCount >= 1 );

	if ( IsX360() &&
		( m_nInternalFlags & TEXTUREFLAGSINTERNAL_TEMPRENDERTARGET ) &&
		!HasBeenAllocated() )
	{
		//need to create the texture bits now
		//to avoid creating the texture bits previously, we simply skipped this step
		if ( !AllocateShaderAPITextures() )
			return;
	}

	if ( m_pTextureHandles && m_nFrameCount >= 1 )
	{
		g_pShaderAPI->CopyRenderTargetToTextureEx( m_pTextureHandles[0], nRenderTargetID, pSrcRect, pDstRect );
	}
}

void CTexture::CopyMeToFrameBuffer( int nRenderTargetID, Rect_t *pSrcRect, Rect_t *pDstRect )
{
	Assert( m_pTextureHandles && m_nFrameCount >= 1 );

	if ( IsX360() &&
		( m_nInternalFlags & TEXTUREFLAGSINTERNAL_TEMPRENDERTARGET ) &&
		!HasBeenAllocated() )
	{
		//need to create the texture bits now
		//to avoid creating the texture bits previously, we simply skipped this step
		if ( !AllocateShaderAPITextures() )
			return;
	}

	if ( m_pTextureHandles && m_nFrameCount >= 1 )
	{
		g_pShaderAPI->CopyTextureToRenderTargetEx( nRenderTargetID, m_pTextureHandles[0], pSrcRect, pDstRect );
	}
}

ITexture *CTexture::GetEmbeddedTexture( int nIndex )
{
	return ( nIndex == 0 ) ? this : NULL;
}

//-----------------------------------------------------------------------------
// Helper method to initialize texture bits in desired state.
//-----------------------------------------------------------------------------
#if defined( _GAMECONSOLE )
bool CTexture::ClearTexture( int r, int g, int b, int a )
{
	Assert( IsProcedural() || IsRenderTarget() );
	if( !HasBeenAllocated() )
		return false;

	if ( m_ImageFormat == IMAGE_FORMAT_D16 || m_ImageFormat == IMAGE_FORMAT_D24S8 || m_ImageFormat == IMAGE_FORMAT_D24FS8 || m_ImageFormat == IMAGE_FORMAT_R32F )
	{
		// not supporting non-rgba textures
		return true;
	}

	CPixelWriter writer;
	g_pShaderAPI->ModifyTexture( m_pTextureHandles[0] );
	if ( !g_pShaderAPI->TexLock( 0, 0, 0, 0, m_nActualWidth, m_nActualHeight, writer ) )
		return false;

	writer.Seek( 0, 0 );
	for ( int j = 0; j < m_nActualHeight; ++j )
	{
		for ( int k = 0; k < m_nActualWidth; ++k )
		{
			writer.WritePixel( r, g, b, a );
		}
	}
	g_pShaderAPI->TexUnlock();

	return true;
}
#endif

#if defined( _X360 )
bool CTexture::CreateRenderTargetSurface( int width, int height, ImageFormat format, bool bSameAsTexture, RTMultiSampleCount360_t multiSampleCount )
{
	Assert( IsRenderTarget() && m_nFrameCount > 1 );

	if ( bSameAsTexture )
	{
		// use RT texture configuration
		width = m_nActualWidth;
		height = m_nActualHeight;
		format = m_ImageFormat;
	}

	// RT surface is expected at end of array
	m_pTextureHandles[m_nFrameCount-1] = g_pShaderAPI->CreateRenderTargetSurface( width, height, format, multiSampleCount, GetName(), TEXTURE_GROUP_RENDER_TARGET_SURFACE );

	return ( m_pTextureHandles[m_nFrameCount-1] != INVALID_SHADERAPI_TEXTURE_HANDLE );
}
#endif

void CTexture::DeleteIfUnreferenced()
{
	if ( m_nRefCount > 0 )
		return;

	TextureManager()->RemoveTexture( this );
}

//Swap everything about a texture except the name. Created to support Portal mod's need for swapping out water render targets in recursive stencil views
void CTexture::SwapContents( ITexture *pOther )
{
	if( (pOther == NULL) || (pOther == this) )
		return;

	ICallQueue *pCallQueue = materials->GetRenderContext()->GetCallQueue();
	if ( pCallQueue )
	{
		pCallQueue->QueueCall( this, &CTexture::SwapContents, pOther );
		return;
	}

	AssertMsg( dynamic_cast<CTexture *>(pOther) != NULL, "Texture swapping broken" );

	CTexture *pOtherAsCTexture = (CTexture *)pOther;

	CTexture *pTemp = (CTexture *)stackalloc( sizeof( CTexture ) );
	
	//swap everything
	memcpy( (void *)pTemp, (void *)this, sizeof( CTexture ) );
	memcpy( (void *)this, (void *)pOtherAsCTexture, sizeof( CTexture ) );
	memcpy( (void *)pOtherAsCTexture, (void *)pTemp, sizeof( CTexture ) );

	//we have the other's name, give it back
	memcpy( &pOtherAsCTexture->m_Name, &m_Name, sizeof( m_Name ) );

	//pTemp still has our name
	memcpy( &m_Name, &pTemp->m_Name, sizeof( m_Name ) );
}

void CTexture::MarkAsPreloaded( bool bSet )
{
	if ( bSet )
	{
		m_nInternalFlags |= TEXTUREFLAGSINTERNAL_PRELOADED;
	}
	else
	{
		m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_PRELOADED;
	}
}

bool CTexture::IsPreloaded() const
{
	return ( ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_PRELOADED ) != 0 );
}

void CTexture::MarkAsExcluded( bool bSet, int nDimensionsLimit, bool bMarkAsTrumpedExclude )
{
	if ( bSet )
	{
		// exclusion trumps picmipping
		m_nInternalFlags |= TEXTUREFLAGSINTERNAL_SHOULDEXCLUDE;
		// unique exclusion state to identify maximum exclusion
		m_nDesiredDimensionLimit = -1;
	}
	else
	{
		// not excluding, but can optionally picmip
		m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_SHOULDEXCLUDE;
		m_nDesiredDimensionLimit = nDimensionsLimit;
	}

	if ( !bSet && nDimensionsLimit > 0 && bMarkAsTrumpedExclude )
	{
		m_nInternalFlags |= TEXTUREFLAGSINTERNAL_FORCED_TO_EXCLUDE;
	}
}

bool CTexture::IsTempExcluded() const
{
	return ( ( m_nInternalFlags & ( TEXTUREFLAGSINTERNAL_SHOULDTEMPEXCLUDE | TEXTUREFLAGSINTERNAL_TEMPEXCLUDED ) ) != 0 );
}

bool CTexture::CanBeTempExcluded() const
{
	return ( m_nRefCount == 1 && 
		m_nFrameCount == 1 && 
		!IsError() && 
		!IsRenderTarget() && 
		!IsProcedural() && 
		!IsCubeMap() );
}

bool CTexture::MarkAsTempExcluded( bool bSet, int nExcludedDimensionLimit )
{
	if ( !CanBeTempExcluded() )
	{
		// not possible to temp exclude these
		return false;
	}

	if ( bSet )
	{
		// temp exclusion can drive the texture to a smaller footprint
		m_nInternalFlags |= TEXTUREFLAGSINTERNAL_SHOULDTEMPEXCLUDE;
		// unique exclusion state to identify exclusion
		m_nDesiredTempDimensionLimit = nExcludedDimensionLimit > 0 ? nExcludedDimensionLimit : -1;
	}
	else
	{
		// no longer temp excluding, default to expected normal exclusion limit
		m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_SHOULDTEMPEXCLUDE;
		m_nDesiredTempDimensionLimit = m_nDesiredDimensionLimit;
	}

	// temp excludes need to be tracked from normal excludes
	m_nInternalFlags |= TEXTUREFLAGSINTERNAL_TEMPEXCLUDE_UPDATE;

	return true;
}

bool CTexture::UpdateExcludedState()
{
	bool bRequiresDownload = false;

	// temp excludes
	bool bDesiredTempExclude = ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_SHOULDTEMPEXCLUDE ) != 0;
	bool bActualTempExclude = ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_TEMPEXCLUDED ) != 0;

	if ( bDesiredTempExclude || bActualTempExclude )
	{
		if ( m_nActualDimensionLimit != m_nDesiredTempDimensionLimit )
		{
			bRequiresDownload = true;
		}
		else
		{
			// temp excludes trump any normal exclude, and normal excludes are ignored until the temp state is cleared
			return false;
		}
	}
	
	// normal excludes
	if ( m_nActualDimensionLimit != m_nDesiredDimensionLimit )
	{
		bRequiresDownload = true;
	}

	if ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_QUEUEDLOAD )
	{
		// already scheduled by the queued loader, the QL wins
		// a fixup will occur later once the QL finishes
		return false;
	}

	if ( bRequiresDownload )
	{
		if ( ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_TEMPEXCLUDE_UPDATE ) && g_MaterialSystem.IsLevelLoadingComplete() && mat_exclude_async_update.GetBool() )
		{
			// temp excludes are async downloaded ONLY in the middle of gameplay, otherwise they do the normal sync download
			// the async download path is !!!ONLY!!! wired for highly constrained temp exclusions, not for general purpose texture downloading
			ScheduleExcludeAsyncDownload();
		}
		else
		{
			// force the texture to re-download, causes the texture bits to match its desired exclusion state
			Download();
		}
	}

	return bRequiresDownload;
}

bool CTexture::IsForceExcluded() const
{
	return ( ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_FORCED_TO_EXCLUDE ) != 0 );
}

bool CTexture::ClearForceExclusion()
{
	if ( !IsForceExcluded() )
		return false;

	// clear the forced exclusion state
	m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_FORCED_TO_EXCLUDE;
	m_nDesiredDimensionLimit = 0;

	return UpdateExcludedState();
}

bool CTexture::IsAsyncDone() const
{
	// we only check for async completion on textures that were async downloaded
	// this function gets called on textures that might be loaded normally sometimes, so it needs to handle that case
	if ( m_nFlags & TEXTUREFLAGS_ASYNC_DOWNLOAD )
	{
		return ( ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_ASYNC_DONE ) != 0 );
	}

	return true;
}

static void IOAsyncCallbackTexture( const FileAsyncRequest_t &asyncRequest, int numReadBytes, FSAsyncStatus_t asyncStatus )
{
	// interpret the async error
	AsyncTextureLoadError_t loaderError;
	switch ( asyncStatus )
	{
	case FSASYNC_OK:
		loaderError = ASYNCTEXTURE_LOADERROR_NONE;
		break;
	case FSASYNC_ERR_FILEOPEN:
		loaderError = ASYNCTEXTURE_LOADERROR_FILEOPEN;
		break;
	default:
		loaderError = ASYNCTEXTRUE_LOADERROR_READING;
	}

	g_MaterialSystem.OnAsyncTextureDataComplete( (AsyncTextureContext_t *)asyncRequest.pContext, asyncRequest.pData, numReadBytes, loaderError );
}

bool CTexture::ScheduleAsyncDownload()
{
	if ( !( m_nFlags & TEXTUREFLAGS_ASYNC_DOWNLOAD ) )
	{
		// only textures flagged of Async Download can be Async downloaded
		Assert( 0 );
		return false;
	}

	if ( m_hAsyncControl )
	{
		// already scheduled
		// this atomically marks when it can be rescheduled
		return false;
	}

	const char *pName;
	pName = m_Name.String();

	char cacheFileName[MATERIAL_MAX_PATH];
	// resolve the relative texture filename to its absolute path
	bool bIsUNCName = ( pName[0] == '/' && pName[1] == '/' && pName[2] != '/' );
	if ( !bIsUNCName )
	{
		Q_snprintf( cacheFileName, sizeof( cacheFileName ), "materials/%s" TEXTURE_FNAME_EXTENSION, pName );
	}
	else
	{
		Q_snprintf( cacheFileName, sizeof( cacheFileName ), "%s" TEXTURE_FNAME_EXTENSION, pName );
	}

	bool bExists = g_pFullFileSystem->FileExists( cacheFileName, MaterialSystem()->GetForcedTextureLoadPathID() );
	if ( !bExists )
	{
		// unexpected failure, file should have existed and was pre-qualified
		// this texture cannot be async downloaded
		return false;
	}

	pName = cacheFileName;
	m_ResolvedFileName = cacheFileName;

	// send down a context that identifies the texture state
	// a texture can only have one outstanding async download operation in flight
	AsyncTextureContext_t *pContext = new AsyncTextureContext_t;
	pContext->m_pTexture = this;
	pContext->m_nInternalFlags = m_nInternalFlags;
	pContext->m_nDesiredTempDimensionLimit = m_nActualDimensionLimit;
	pContext->m_nActualDimensionLimit = m_nActualDimensionLimit;
	pContext->m_pVTFTexture = NULL;

	// schedule the async using what should be the absolute path to the file
	FileAsyncRequest_t asyncRequest;
	asyncRequest.pszFilename = pName;
	asyncRequest.priority = -1;
	asyncRequest.flags = FSASYNC_FLAGS_ALLOCNOFREE;
	asyncRequest.pContext = (void *)pContext;
	asyncRequest.pfnCallback = IOAsyncCallbackTexture;
	g_pFullFileSystem->AsyncRead( asyncRequest, &m_hAsyncControl );

	return true;
}

bool CTexture::ScheduleExcludeAsyncDownload()
{
	if ( !( m_nInternalFlags & TEXTUREFLAGSINTERNAL_TEMPEXCLUDE_UPDATE ) )
	{
		// NOOOOO!!!!! This cannot be used for any textures except highly constrained temp excludes that have been properly qualified.
		// It cannot be re-purposed into pushing textures in.
		Assert( 0 );
		return false;
	}

	if ( m_hAsyncControl )
	{
		// already scheduled
		// this atomically marks when it can be rescheduled
		// the data delivery will adhere to the current desired exclude state (that may change before the data arrives multiple times)
		return false;
	}

	// want to use the prior resolved absolute filename, this causes an i/o penalty hitch once the first time the filesystem search path resolves
	// each additional operation avoids the SP penalty
	const char *pName;
	bool bResolved = false;
	bool bExcluding = false;
	if ( ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_SHOULDEXCLUDE ) ||
		( ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_SHOULDTEMPEXCLUDE ) && m_nDesiredTempDimensionLimit <= 0 ) )
	{
		bExcluding = true;
		if ( m_ExcludedResolvedFileName.IsValid() )
		{
			pName = m_ExcludedResolvedFileName.String();
			bResolved = true;
		}
		else
		{
			// The replacement texture for an excluded resource is solid black.
			// The better thing would be to have unique 8x8 representations.
			// Can cheap out on this because a temp excluded resource (weapon) is not supposed to be rendering
			// for any more than a few frames before it's restored data appears.
			pName = "dev/dev_temp_exclude";
		}
	}
	else
	{
		if ( m_ResolvedFileName.IsValid() )
		{
			pName = m_ResolvedFileName.String();
			bResolved = true;
		}
		else
		{
			pName = m_Name.String();
		}
	}

	char fullPath[MAX_PATH];
	char cacheFileName[MATERIAL_MAX_PATH];
	if ( !bResolved )
	{
		// resolve the relative texture filename to its absolute path
		bool bIsUNCName = ( pName[0] == '/' && pName[1] == '/' && pName[2] != '/' );
		if ( !bIsUNCName )
		{
			Q_snprintf( cacheFileName, sizeof( cacheFileName ), "materials/%s" TEXTURE_FNAME_EXTENSION, pName );
		}
		else
		{
			Q_snprintf( cacheFileName, sizeof( cacheFileName ), "%s" TEXTURE_FNAME_EXTENSION, pName );
		}

		// all 360 files are expected to be in zip, no need to search outside of zip
		g_pFullFileSystem->RelativePathToFullPath( cacheFileName, MaterialSystem()->GetForcedTextureLoadPathID(), fullPath, sizeof( fullPath ), ( IsX360() ? FILTER_CULLNONPACK : FILTER_NONE ) );
		bool bExists = V_IsAbsolutePath( fullPath );
		if ( !bExists )
		{
			// unexpected failure, file should have existed and was pre-qualified
			// this texture cannot be async downloaded
			return false;
		}

		pName = fullPath;
		if ( bExcluding )
		{
			m_ExcludedResolvedFileName = fullPath;
		}
		else
		{
			m_ResolvedFileName = fullPath;
		}
	}

	// send down a context that identifies the texture state
	// the latent async delivery will need to match the state that may have changed
	// a texture can only have one outstanding async download operation in flight
	// although the texture state may thrash, the actual i/o will not
	AsyncTextureContext_t *pContext = new AsyncTextureContext_t;
	pContext->m_pTexture = this;
	pContext->m_nInternalFlags = m_nInternalFlags;
	pContext->m_nDesiredTempDimensionLimit = m_nDesiredTempDimensionLimit;
	pContext->m_nActualDimensionLimit = m_nActualDimensionLimit;
	pContext->m_pVTFTexture = NULL;

	// schedule the async using what should be the absolute path to the file
	FileAsyncRequest_t asyncRequest;
	asyncRequest.pszFilename = pName;
	asyncRequest.priority = -1;
	asyncRequest.flags = FSASYNC_FLAGS_ALLOCNOFREE;
	asyncRequest.pContext = (void *)pContext;
	asyncRequest.pfnCallback = IOAsyncCallbackTexture;
	g_pFullFileSystem->AsyncRead( asyncRequest, &m_hAsyncControl );

	return true;
}


//-----------------------------------------------------------------------------
// Note for async texture:
// -----------------------
//   The download is done is 2 parts:
//		* Generating the VTF
//		* Using VTF to create the shader api texture (effectively the corresponding d3d resource)
//   In order to reduce spikes on the main thread (cf CMaterialSystem::ServiceAsyncTextureLoads), the flMaxTimeMs
//   limit has been introduced =>  you can safely exit after generating the VTF and resume it at a later date
//-----------------------------------------------------------------------------
bool CTexture::FinishAsyncDownload( AsyncTextureContext_t *pContext, void *pData, int nNumReadBytes, bool bAbort, float flMaxTimeMs )
{
	// The temp exclusions/restores are expected/desgined to be serviced at the only safe interval at the end of the frame
	// and end of any current QMS jobs before QMS queues and start on it's next frame. Texture downloading is not thead safe
	// and does not need to be made so. Instead, while the texture access pattens are quiescent and stable, the download
	// (expected to be few in number) is deferred to this safe interval.
	Assert( ThreadInMainThread() );

	// For non-exclude async downloads, never abort
	if ( m_nFlags & TEXTUREFLAGS_ASYNC_DOWNLOAD )
	{
		bAbort = false;
	}

	// aborting just discards the data
	// whatever state the texture bits were in, they stay that way
	bool bDownloadCompleted = true;
	if ( !bAbort && g_pShaderAPI->CanDownloadTextures() )
	{
		// the delayed async nature of this download may have invalidated the original/expected state at the moment of queuing
		// prevent an update of the texture to the wrong state
		// the temp exclusion monitor is responsible for rescheduling
		if ( ( m_nFlags & TEXTUREFLAGS_ASYNC_DOWNLOAD ) || // general async download (non-exclude)
			( ( ( pContext->m_nInternalFlags & TEXTUREFLAGSINTERNAL_SHOULDTEMPEXCLUDE ) == ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_SHOULDTEMPEXCLUDE ) ) &&
			  ( pContext->m_nDesiredTempDimensionLimit == m_nDesiredTempDimensionLimit ) ) )
		{
			// the download will put the texture in the expected state
			MaterialLock_t hLock = MaterialSystem()->Lock();
			if ( m_nFlags & TEXTUREFLAGS_ASYNC_DOWNLOAD )
			{
				bDownloadCompleted = DownloadAsyncTexture( pContext, pData, nNumReadBytes, flMaxTimeMs );
			}
			else
			{
				DownloadTexture( NULL, pData, nNumReadBytes );
			}
			MaterialSystem()->Unlock( hLock );
		}
		else
		{
			// the texture wants to be in a different state than this download can achieve
			bool bDesiredTempExclude = ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_SHOULDTEMPEXCLUDE ) != 0;
			bool bActualTempExclude = ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_TEMPEXCLUDED ) != 0;
			if ( bDesiredTempExclude == bActualTempExclude && m_nDesiredTempDimensionLimit == m_nActualDimensionLimit )
			{
				// the current desired temp exclude state now matches the actual
				// the discarded download does not need to happen
				m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_TEMPEXCLUDE_UPDATE;
			}
		}
	}

	if ( bDownloadCompleted )
	{
		// ownership of the data is expected to have been handed off
		g_pFullFileSystem->FreeOptimalReadBuffer( pData );
		g_pFullFileSystem->AsyncRelease( m_hAsyncControl );
		// texture can be rescheduled
		m_hAsyncControl = NULL;

		delete pContext;

		if (m_nFlags & TEXTUREFLAGS_ASYNC_DOWNLOAD)
		{
			m_nInternalFlags |= TEXTUREFLAGSINTERNAL_ASYNC_DONE; // signal we're done
			m_nFlags &= ~TEXTUREFLAGS_ASYNC_DOWNLOAD; // no longer want this flag
		}
	}

	return bDownloadCompleted;
}


//////////////////////////////////////////////////////////////////////////
//
// Saving all the texture LOD modifications to content
//
//////////////////////////////////////////////////////////////////////////

#ifdef IS_WINDOWS_PC
static bool SetBufferValue( char *chTxtFileBuffer, char const *szLookupKey, char const *szNewValue )
{
	bool bResult = false;
	
	size_t lenTmp = strlen( szNewValue );
	size_t nTxtFileBufferLen = strlen( chTxtFileBuffer );
	
	for ( char *pch = chTxtFileBuffer;
		( NULL != ( pch = strstr( pch, szLookupKey ) ) );
		++ pch )
	{
		char *val = pch + strlen( szLookupKey );
		if ( !V_isspace( *val ) )
			continue;
		else
			++ val;
		char *pValStart = val;

		// Okay, here comes the value
		while ( *val && V_isspace( *val ) )
			++ val;
		while ( *val && !V_isspace( *val ) )
			++ val;

		char *pValEnd = val; // Okay, here ends the value

		memmove( pValStart + lenTmp, pValEnd, chTxtFileBuffer + nTxtFileBufferLen + 1 - pValEnd );
		memcpy( pValStart, szNewValue, lenTmp );

		nTxtFileBufferLen += ( lenTmp - ( pValEnd - pValStart ) );
		bResult = true;
	}

	if ( !bResult )
	{
		char *pchAdd = chTxtFileBuffer + nTxtFileBufferLen;
		strcpy( pchAdd + strlen( pchAdd ), "\n" );
		strcpy( pchAdd + strlen( pchAdd ), szLookupKey );
		strcpy( pchAdd + strlen( pchAdd ), " " );
		strcpy( pchAdd + strlen( pchAdd ), szNewValue );
		strcpy( pchAdd + strlen( pchAdd ), "\n" );
		bResult = true;
	}

	return bResult;
}

// Replaces the first occurrence of "szFindData" with "szNewData"
// Returns the remaining buffer past the replaced data or NULL if
// no replacement occurred.
static char * BufferReplace( char *buf, char const *szFindData, char const *szNewData )
{
	size_t len = strlen( buf ), lFind = strlen( szFindData ), lNew = strlen( szNewData );
	if ( char *pBegin = strstr( buf, szFindData ) )
	{
		memmove( pBegin + lNew, pBegin + lFind, buf + len - ( pBegin + lFind ) );
		memmove( pBegin, szNewData, lNew );
		return pBegin + lNew;
	}
	return NULL;
}


class CP4Requirement
{
public:
	CP4Requirement();
	~CP4Requirement();

protected:
	bool m_bLoadedModule;
	CSysModule *m_pP4Module;
};

CP4Requirement::CP4Requirement() :
	m_bLoadedModule( false ),
	m_pP4Module( NULL )
{
	if ( p4 )
		return;

	// load the p4 lib
	m_pP4Module = Sys_LoadModule( "p4lib" );
	m_bLoadedModule = true;
		
	if ( m_pP4Module )
	{
		CreateInterfaceFn factory = Sys_GetFactory( m_pP4Module );
		if ( factory )
		{
			p4 = ( IP4 * )factory( P4_INTERFACE_VERSION, NULL );

			if ( p4 )
			{
				extern CreateInterfaceFn g_fnMatSystemConnectCreateInterface;
				p4->Connect( g_fnMatSystemConnectCreateInterface );
				p4->Init();
			}
		}
	}

	if ( !p4 )
	{
		Warning( "Can't load p4lib.dll\n" );
	}
}

CP4Requirement::~CP4Requirement()
{
	if ( m_bLoadedModule && m_pP4Module )
	{
		if ( p4 )
		{
			p4->Shutdown();
			p4->Disconnect();
		}

		Sys_UnloadModule( m_pP4Module );
		m_pP4Module = NULL;
		p4 = NULL;
	}
}

static ConVar mat_texture_list_content_path( "mat_texture_list_content_path", "", FCVAR_ARCHIVE, "The content path to the materialsrc directory. If left unset, it'll assume your content directory is next to the currently running game dir." );

CON_COMMAND_F( mat_texture_list_txlod_sync, "'reset' - resets all run-time changes to LOD overrides, 'save' - saves all changes to material content files", FCVAR_DONTRECORD )
{
	using namespace TextureLodOverride;

	if ( args.ArgC() != 2 )
		goto usage;

	char const *szCmd = args.Arg( 1 );
	Msg( "mat_texture_list_txlod_sync %s...\n", szCmd );

	if ( !stricmp( szCmd, "reset" ) )
	{
		for ( int k = 0; k < s_OverrideMap.GetNumStrings(); ++ k )
		{
			char const *szTx = s_OverrideMap.String( k );
			s_OverrideMap[ k ] = OverrideInfo(); // Reset the override info

			// Force the texture LOD override to get re-processed
			if ( ITexture *pTx = materials->FindTexture( szTx, "" ) )
				pTx->ForceLODOverride( 0 );
			else
				Warning( " mat_texture_list_txlod_sync reset - texture '%s' no longer found.\n", szTx );
		}

		s_OverrideMap.Purge();
		Msg("mat_texture_list_txlod_sync reset : completed.\n");
		return;
	}
	else if ( !stricmp( szCmd, "save" ) )
	{
		CP4Requirement p4req;
		if ( !p4 )
			g_p4factory->SetDummyMode( true );

		for ( int k = 0; k < s_OverrideMap.GetNumStrings(); ++ k )
		{
			char const *szTx = s_OverrideMap.String( k );
			OverrideInfo oi = s_OverrideMap[ k ];
			ITexture *pTx = materials->FindTexture( szTx, "" );
			
			if ( !oi.x || !oi.y )
				continue;

			if ( !pTx )
			{
				Warning( " mat_texture_list_txlod_sync save - texture '%s' no longer found.\n", szTx );
				continue;
			}

			int iMaxWidth = pTx->GetActualWidth(), iMaxHeight = pTx->GetActualHeight();
			
			// Save maxwidth and maxheight
			char chMaxWidth[20], chMaxHeight[20];
			sprintf( chMaxWidth, "%d", iMaxWidth ), sprintf( chMaxHeight, "%d", iMaxHeight );

			// We have the texture and path to its content
			char chResolveName[ MAX_PATH ] = {0}, chResolveNameArg[ MAX_PATH ] = {0};
			Q_snprintf( chResolveNameArg, sizeof( chResolveNameArg ) - 1, "materials/%s" TEXTURE_FNAME_EXTENSION, szTx );
			char *szTextureContentPath;
			if ( !mat_texture_list_content_path.GetString()[0] )
			{
				szTextureContentPath = const_cast< char * >( g_pFullFileSystem->RelativePathToFullPath( chResolveNameArg, "game", chResolveName, sizeof( chResolveName ) - 1 ) );

				if ( !szTextureContentPath )
				{
					Warning( " mat_texture_list_txlod_sync save - texture '%s' is not loaded from file system.\n", szTx );
					continue;
				}
				if ( !BufferReplace( szTextureContentPath, "\\game\\", "\\content\\" ) ||
					 !BufferReplace( szTextureContentPath, "\\materials\\", "\\materialsrc\\" ) )
				{
					Warning( " mat_texture_list_txlod_sync save - texture '%s' cannot be mapped to content directory.\n", szTx );
					continue;
				}
			}
			else
			{
				V_strncpy( chResolveName, mat_texture_list_content_path.GetString(), MAX_PATH );
				V_strncat( chResolveName, "/", MAX_PATH );
				V_strncat( chResolveName, szTx, MAX_PATH );
				V_strncat( chResolveName, TEXTURE_FNAME_EXTENSION, MAX_PATH );

				szTextureContentPath = chResolveName;
			}

			// Figure out what kind of source content is there:
			// 1. look for TGA - if found, get the txt file (if txt file missing, create one)
			// 2. otherwise look for PSD - affecting psdinfo
			// 3. else error
			char *pExtPut = szTextureContentPath + strlen( szTextureContentPath ) - strlen( TEXTURE_FNAME_EXTENSION ); // compensating the TEXTURE_FNAME_EXTENSION(.vtf) extension
			
			// 1.tga
			sprintf( pExtPut, ".tga" );
			if ( g_pFullFileSystem->FileExists( szTextureContentPath ) )
			{
				// Have tga - pump in the txt file
				sprintf( pExtPut, ".txt" );
				
				CUtlBuffer bufTxtFileBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
				g_pFullFileSystem->ReadFile( szTextureContentPath, 0, bufTxtFileBuffer );
				for ( int k = 0; k < 1024; ++ k ) bufTxtFileBuffer.PutChar( 0 );

				// Now fix maxwidth/maxheight settings
				SetBufferValue( ( char * ) bufTxtFileBuffer.Base(), "maxwidth", chMaxWidth );
				SetBufferValue( ( char * ) bufTxtFileBuffer.Base(), "maxheight", chMaxHeight );
				bufTxtFileBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, strlen( ( char * ) bufTxtFileBuffer.Base() ) );

				// Check out or add the file
				g_p4factory->SetOpenFileChangeList( "Texture LOD Autocheckout" );
				CP4AutoEditFile autop4_edit( szTextureContentPath );

				// Save the file contents
				if ( g_pFullFileSystem->WriteFile( szTextureContentPath, 0, bufTxtFileBuffer ) )
				{
					Msg(" '%s' : saved.\n", szTextureContentPath );
					CP4AutoAddFile autop4_add( szTextureContentPath );
				}
				else
				{
					Warning( " '%s' : failed to save - set \"maxwidth %d maxheight %d\" manually.\n",
						szTextureContentPath, iMaxWidth, iMaxHeight );
				}

				continue;
			}

			// 2.psd
			sprintf( pExtPut, ".psd" );
			if ( g_pFullFileSystem->FileExists( szTextureContentPath ) )
			{
				char chCommand[MAX_PATH];
				char szTxtFileName[MAX_PATH] = {0};
				GetModSubdirectory( "tmp_lod_psdinfo.txt", szTxtFileName, sizeof( szTxtFileName ) );
				sprintf( chCommand, "/C psdinfo \"%s\" > \"%s\"", szTextureContentPath, szTxtFileName);
				ShellExecute( NULL, NULL, "cmd.exe", chCommand, NULL, SW_HIDE );
				Sleep( 200 );

				CUtlBuffer bufTxtFileBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
				g_pFullFileSystem->ReadFile( szTxtFileName, 0, bufTxtFileBuffer );
				for ( int k = 0; k < 1024; ++ k ) bufTxtFileBuffer.PutChar( 0 );

				// Now fix maxwidth/maxheight settings
				SetBufferValue( ( char * ) bufTxtFileBuffer.Base(), "maxwidth", chMaxWidth );
				SetBufferValue( ( char * ) bufTxtFileBuffer.Base(), "maxheight", chMaxHeight );
				bufTxtFileBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, strlen( ( char * ) bufTxtFileBuffer.Base() ) );

				// Check out or add the file
				// Save the file contents
				if ( g_pFullFileSystem->WriteFile( szTxtFileName, 0, bufTxtFileBuffer ) )
				{
					g_p4factory->SetOpenFileChangeList( "Texture LOD Autocheckout" );
					CP4AutoEditFile autop4_edit( szTextureContentPath );

					sprintf( chCommand, "/C psdinfo -write \"%s\" < \"%s\"", szTextureContentPath, szTxtFileName );
					Sleep( 200 );
					ShellExecute( NULL, NULL, "cmd.exe", chCommand, NULL, SW_HIDE );
					Sleep( 200 );

					Msg(" '%s' : saved.\n", szTextureContentPath );
					CP4AutoAddFile autop4_add( szTextureContentPath );
				}
				else
				{
					Warning( " '%s' : failed to save - set \"maxwidth %d maxheight %d\" manually.\n",
						szTextureContentPath, iMaxWidth, iMaxHeight );
				}

				continue;
			}

			// 3. - error
			sprintf( pExtPut, "" );
			{
				Warning( " '%s' : doesn't specify a valid TGA or PSD file!\n", szTextureContentPath );
				continue;
			}
		}

		Msg("mat_texture_list_txlod_sync save : completed.\n");
		return;
	}
	else
		goto usage;

	return;

usage:
	Warning(
		"Usage:\n"
		"  mat_texture_list_txlod_sync reset - resets all run-time changes to LOD overrides;\n"
		"  mat_texture_list_txlod_sync save  - saves all changes to material content files.\n"
		);
}

ConVar mat_texture_list_exclude_editing( "mat_texture_list_exclude_editing", "0" );

CON_COMMAND_F( mat_texture_list_exclude, "'load' - loads the exclude list file, 'reset' - resets all loaded exclude information, 'save' - saves exclude list file", FCVAR_DONTRECORD )
{
	using namespace TextureLodOverride;
	using namespace TextureLodExclude;

	if ( args.ArgC() < 2 )
		goto usage;

	char const *szCmd = args.Arg( 1 );

	if ( !stricmp( szCmd, "load" ) )
	{
		if ( args.ArgC() < 3 )
			goto usage;

		char const *szFile = args.Arg( 2 );
		Msg( "mat_texture_list_exclude loading '%s'...\n", szFile );

		// Read the file buffer
		CUtlInplaceBuffer bufFile( 0, 0, CUtlBuffer::TEXT_BUFFER );
		if ( !g_pFullFileSystem->ReadFile( szFile, NULL, bufFile ) )
		{
			Warning( "Error: failed to load exclude file '%s'!\n", szFile );
			return;
		}

		// Process the file
		while ( char *pszLine = bufFile.InplaceGetLinePtr() )
		{
			// Skip empty lines
			if ( !*pszLine || V_isspace( *pszLine ) ||
				!V_isalnum( *pszLine ) )
				continue;

			// If the line starts with a digit, then it's LOD override
			int nLodOverride = atoi( pszLine );
			while ( V_isdigit( *pszLine ) )
				++ pszLine;
			while ( V_isspace( *pszLine ) )
				++ pszLine;
			
			// Skip malformed lines
			if ( !V_isalpha( *pszLine ) )
				continue;

			// Record the exclude element
			TextureLodExclude::Add( pszLine, nLodOverride );
		}

		for ( int k = 0; k < s_ExcludeMap.GetNumStrings(); ++ k )
		{
			char const *szTx = s_ExcludeMap.String( k );

			// Force the texture LOD override to get re-processed
			if ( ITexture *pTx = materials->FindTexture( szTx, "" ) )
				pTx->Download( NULL );
		}

		Msg( "mat_texture_list_exclude loaded '%s'.\n", szFile );
		
		// Set the var to designate exclude mode
		int iMode = mat_texture_list_exclude_editing.GetInt();
		mat_texture_list_exclude_editing.SetValue( MAX( 0, iMode ) + 1 );
		
		return;
	}
	else if ( !stricmp( szCmd, "reset" ) )
	{
		Msg( "mat_texture_list_exclude reset...\n" );

		CUtlStringMap< int > lstReload;

		for ( int k = 0; k < s_OverrideMap.GetNumStrings(); ++ k )
		{
			char const *szTx = s_OverrideMap.String( k );
			lstReload[ szTx ] = 1;
		}
		s_OverrideMap.Purge();

		for ( int k = 0; k < s_ExcludeMap.GetNumStrings(); ++ k )
		{
			char const *szTx = s_ExcludeMap.String( k );
			lstReload[ szTx ] = 1;
		}
		s_ExcludeMap.Purge();

		for ( int k = 0; k < lstReload.GetNumStrings(); ++ k )
		{
			char const *szTx = lstReload.String( k );

			// Force the texture LOD override to get re-processed
			if ( ITexture *pTx = materials->FindTexture( szTx, "" ) )
				pTx->Download( NULL );
		}

		Msg( "mat_texture_list_exclude reset : completed.\n" );

		mat_texture_list_exclude_editing.SetValue( 0 );

		return;
	}
	else if ( !stricmp( szCmd, "save" ) )
	{
		if ( args.ArgC() < 3 )
			goto usage;

		char const *szFile = args.Arg( 2 );
		Msg( "mat_texture_list_exclude saving '%s'...\n", szFile );

		// Read the file buffer
		CUtlInplaceBuffer bufFile( 0, 0, CUtlBuffer::TEXT_BUFFER );

		// Write the buffer file (full excludes)
		for ( int k = 0; k < s_ExcludeMap.GetNumStrings(); ++ k )
		{
			char const *szTx = s_ExcludeMap.String( k );
			int x = s_ExcludeMap[ k ];

			if ( !( x == 0 ) ) continue; // first pass, skip mips

			bufFile.Printf( "%s\n", szTx );
		}

		// empty line
		bufFile.Printf( "\n" );

		// Write the buffer file (mips)
		for ( int k = 0; k < s_ExcludeMap.GetNumStrings(); ++ k )
		{
			char const *szTx = s_ExcludeMap.String( k );
			int x = s_ExcludeMap[ k ];

			if ( !( x > 0 ) ) continue;

			bufFile.Printf( "%d %s\n", x, szTx );
		}

		// Save out the buffer to file
		if ( !g_pFullFileSystem->WriteFile( szFile, NULL, bufFile ) )
		{
			Warning( "Error: failed to save exclude file '%s'!\n", szFile );
			return;
		}

		Msg( "mat_texture_list_exclude saved '%s'.\n", szFile );
		return;
	}

	return;

usage:
	Warning(
		"Usage:\n"
		"  mat_texture_list_exclude load excludelistfile.lst - loads exclude list file;\n"
		"  mat_texture_list_exclude reset - resets loaded exclude list information;\n"
		"  mat_texture_list_exclude save excludelistfile.lst - saves exclude list file.\n"
		);
}

#endif



bool CTextureImpl_GetTextureInformation( char const *szTextureName, MaterialTextureInfo_t &info )
{
#ifdef IS_WINDOWS_PC

	info.iExcludeInformation = TextureLodExclude::Get( szTextureName );
	return true;

#else

	return false;

#endif
}

