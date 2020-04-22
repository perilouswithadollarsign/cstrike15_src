//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Color correction system
//
//==============================================================================

#if defined (WIN32) && !defined( _X360 )
#include <windows.h>
#elif defined( POSIX )
#define _cdecl
#endif
#include "materialsystem/IColorCorrection.h"
#include "materialsystem_global.h"
#include "shaderapi/ishaderapi.h"
#include "texturemanager.h"
#include "utlvector.h"
#include "generichash.h"
#include "filesystem.h"
#include "filesystem/IQueuedLoader.h"
#include "tier2/fileutils.h"
#include "pixelwriter.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class ITextureInternal;

//-----------------------------------------------------------------------------
// Color correction textures are procedural. Handle the download
//-----------------------------------------------------------------------------
class CColorCorrectionRegen : public ITextureRegenerator
{
public:
	CColorCorrectionRegen( ColorCorrectionHandle_t handle ) : m_ColorCorrectionHandle( handle )
	{
	}

	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect )
	{
		int nWidth = pVTFTexture->Width();
		int nHeight = pVTFTexture->Height();
		int nDepth = pVTFTexture->Depth();
		Assert( nWidth == COLOR_CORRECTION_TEXTURE_SIZE && nHeight == COLOR_CORRECTION_TEXTURE_SIZE && nDepth == COLOR_CORRECTION_TEXTURE_SIZE );

		for ( int z = 0; z < nDepth; ++z )
		{
			CPixelWriter pixelWriter;
			pixelWriter.SetPixelMemory( pVTFTexture->Format(), 
				pVTFTexture->ImageData( 0, 0, 0, 0, 0, z ), pVTFTexture->RowSizeInBytes( 0 ) );

			for ( int y = 0; y < nHeight; ++y )
			{
				pixelWriter.Seek( 0, y );
				for (int x = 0; x < nWidth; ++x)
				{
					RGBX5551_t inColor;
					inColor.r = x;
					inColor.g = y;
					inColor.b = z;

					color24 col = ColorCorrectionSystem()->GetLookup( m_ColorCorrectionHandle, inColor );
					pixelWriter.WritePixel( col.r, col.g, col.b, 255 );
				}
			}
		}

		if ( IsX360() )
		{
			g_pColorCorrectionSystem->OnProceduralRegenComplete( m_ColorCorrectionHandle );
		}
	}

	virtual void Release() 
	{
		delete this;
	}

private:
	ColorCorrectionHandle_t	m_ColorCorrectionHandle;
};

//-----------------------------------------------------------------------------
//	Holds all the necessary information for a single color correction lookup
//  function
//-----------------------------------------------------------------------------
struct ColorCorrectionLookup_t
{
	ColorCorrectionLookup_t( ColorCorrectionHandle_t handle );
	~ColorCorrectionLookup_t();

	void					ReleaseTexture();
	void					RestoreTexture();
	void					AllocTexture();

	ColorCorrectionHandle_t m_Handle;
	ITextureInternal	   *m_pColorCorrectionTexture;
	color24					*m_pColorCorrection;
	bool					m_bLocked;
	float					m_flWeight;
	bool					m_bResetable;
};

ColorCorrectionLookup_t::ColorCorrectionLookup_t( ColorCorrectionHandle_t handle )
{
	m_Handle = handle;
	m_bLocked = false;
	m_flWeight = 1.0f;
	m_bResetable = true;

	// these are the backing bits for the volume, either the file gets pushed into this or a caller can procedurally build
	m_pColorCorrection = new color24[COLOR_CORRECTION_TEXTURE_SIZE*COLOR_CORRECTION_TEXTURE_SIZE*COLOR_CORRECTION_TEXTURE_SIZE];

	AllocTexture();
}

ColorCorrectionLookup_t::~ColorCorrectionLookup_t()
{
	// We must remove the texture from the active color correction
	// textures, otherwise we may leak a reference
	if ( m_pColorCorrectionTexture )
	{
		for ( int i=0; i<COLOR_CORRECTION_MAX_TEXTURES; i++ )
		{
			if ( m_pColorCorrectionTexture == TextureManager()->ColorCorrectionTexture( i ) )
			{
				TextureManager()->SetColorCorrectionTexture( i, NULL );
			}
		}

		m_pColorCorrectionTexture->DecrementReferenceCount();
		ITextureInternal::Destroy( m_pColorCorrectionTexture );
		m_pColorCorrectionTexture = NULL;
	}

	delete [] m_pColorCorrection;
}

void ColorCorrectionLookup_t::AllocTexture()
{
	char szFilename[MAX_PATH];
	char szShortName[MAX_PATH];
	g_pFullFileSystem->String( (FileNameHandle_t)m_Handle, szFilename, sizeof( szFilename ) );
	V_FileBase( szFilename, szShortName, sizeof( szShortName ) );

	char szName[128];
	V_snprintf( szName, sizeof( szName ), "ColorCorrection (%s)", szShortName );

	m_pColorCorrectionTexture = ITextureInternal::CreateProceduralTexture( szName, TEXTURE_GROUP_OTHER,
		COLOR_CORRECTION_TEXTURE_SIZE, COLOR_CORRECTION_TEXTURE_SIZE, COLOR_CORRECTION_TEXTURE_SIZE, IMAGE_FORMAT_BGRX8888,
		TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_CLAMPS | 
		TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_CLAMPU | TEXTUREFLAGS_NODEBUGOVERRIDE );

	// color correction textures are procedural, provide the download interface
	ITextureRegenerator *pRegen = new CColorCorrectionRegen( m_Handle );
	m_pColorCorrectionTexture->SetTextureRegenerator( pRegen );

	// xbox needs to defer the real download (only once) to when the volume data is pushed in
	if ( !IsX360() )
	{
		m_pColorCorrectionTexture->Download();
	}
}

void ColorCorrectionLookup_t::ReleaseTexture()
{
	if ( IsX360() )
	{
		// there is no release/restore ability on the xbox
		Assert( 0 );
		return;
	}

	m_pColorCorrectionTexture->Release();
}

void ColorCorrectionLookup_t::RestoreTexture()
{
	if ( IsX360() )
	{
		// there is no release/restore ability on the xbox
		Assert( 0 );
		return;
	}

	// Put the texture back onto the board
	m_pColorCorrectionTexture->OnRestore();	// Give render targets a chance to reinitialize themselves if necessary (due to AA changes).
	m_pColorCorrectionTexture->Download();
}


//-----------------------------------------------------------------------------
//  IColorCorrectionSystem interface implementation
//-----------------------------------------------------------------------------
class CColorCorrectionSystem : public IColorCorrectionSystem
{
public:
	virtual void Init();
	virtual void Shutdown();

	virtual void LockLookup();
	virtual void SetLookup( RGBX5551_t inColor, color24 outColor );
	virtual void UnlockLookup();
	virtual color24 GetLookup( RGBX5551_t inColor );

	virtual void LoadLookup( const char *pLookupName );
	virtual void CopyLookup( const color24 *pSrcColorCorrection );
	virtual void ResetLookup();

	virtual ColorCorrectionHandle_t AddLookup( const char *pName );
	virtual bool RemoveLookup( ColorCorrectionHandle_t handle );

	virtual void  SetLookupWeight( ColorCorrectionHandle_t handle, float flWeight );
	virtual float GetLookupWeight( ColorCorrectionHandle_t handle );
	virtual float GetLookupWeight( int i );

	virtual void LockLookup( ColorCorrectionHandle_t handle );
	virtual void SetLookup( ColorCorrectionHandle_t handle, RGBX5551_t inColor, color24 outColor );
	virtual color24 GetLookup( ColorCorrectionHandle_t handle, RGBX5551_t inColor );
	virtual void UnlockLookup( ColorCorrectionHandle_t handle, bool bDownload );

	virtual void LoadLookup( ColorCorrectionHandle_t handle, const char *pLookupName );
	virtual void CopyLookup( ColorCorrectionHandle_t handle, const color24 *pSrcColorCorrection );
	virtual void ResetLookup( ColorCorrectionHandle_t handle );

	virtual void ResetLookupWeights();

	virtual void ReleaseTextures();
	virtual void RestoreTextures();

	virtual color24 ConvertToColor24( RGBX5551_t inColor );

	virtual int GetNumLookups();

	virtual void SetResetable( ColorCorrectionHandle_t handle, bool bResetable );
	virtual void GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo );
	virtual void EnableColorCorrection( bool bEnable );

	virtual void OnProceduralRegenComplete( ColorCorrectionHandle_t handle );

	// returns NULL if not found
	virtual ColorCorrectionHandle_t FindLookup( const char *pName );

protected:
	CUtlVector< ColorCorrectionLookup_t * > m_ColorCorrectionList;
    ColorCorrectionHandle_t m_DefaultColorCorrectionHandle;
	ColorCorrectionHandle_t m_UnnamedColorCorrectionHandle;
	float m_DefaultColorCorrectionWeight;
	bool m_bEnabled;

	void SortLookups();
	ColorCorrectionLookup_t *FindLookup( ColorCorrectionHandle_t handle );
	ColorCorrectionHandle_t GetLookupHandle( const char *pName );
	void SetLookupPtr( ColorCorrectionLookup_t *lookup, RGBX5551_t inColor, color24 outColor );
	void GetNormalizedWeights( float *pDefaultWeight, float *pLookupWeights );
};

CColorCorrectionSystem g_ColorCorrectionSystem;
IColorCorrectionSystem *g_pColorCorrectionSystem = &g_ColorCorrectionSystem;

#ifndef _X360
// Don't allow this on the 360.. it doesn't work in mat_queued_mode
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CColorCorrectionSystem, IColorCorrectionSystem, COLORCORRECTION_INTERFACE_VERSION, g_ColorCorrectionSystem );
#endif


//-----------------------------------------------------------------------------
//  Wrappers for release/restore functionality
//-----------------------------------------------------------------------------
void ReleaseColorCorrection( int nChangeFlags )
{
	if ( IsX360() )
	{
		// there is no release/restore ability on the xbox
		Assert( 0 );
		return;
	}

	if ( nChangeFlags & MATERIAL_RESTORE_VERTEX_FORMAT_CHANGED )
		return;

	g_pColorCorrectionSystem->ReleaseTextures();
}

void RestoreColorCorrection( int nChangeFlags )
{
	if ( IsX360() )
	{
		// there is no release/restore ability on the xbox
		Assert( 0 );
		return;
	}

	if ( nChangeFlags & MATERIAL_RESTORE_VERTEX_FORMAT_CHANGED )
		return;
	g_pColorCorrectionSystem->RestoreTextures();
}


//-----------------------------------------------------------------------------
//  Init function
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::Init()
{
	m_DefaultColorCorrectionHandle = (ColorCorrectionHandle_t)-1;
	m_UnnamedColorCorrectionHandle = GetLookupHandle( "unnamed" );
	m_DefaultColorCorrectionWeight = 0.0f;
	m_bEnabled = true;

	MaterialSystem()->AddReleaseFunc( ReleaseColorCorrection );
	MaterialSystem()->AddRestoreFunc( RestoreColorCorrection );
}

//-----------------------------------------------------------------------------
//  Shutdown function
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::Shutdown()
{
	for ( int i=0;i<m_ColorCorrectionList.Count();i++ )
	{
		delete m_ColorCorrectionList[i];
	}

	MaterialSystem()->RemoveReleaseFunc( ReleaseColorCorrection );
	MaterialSystem()->RemoveRestoreFunc( RestoreColorCorrection );
}


//-----------------------------------------------------------------------------
// Global enable/disable of color correction
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::EnableColorCorrection( bool bEnable )
{
	m_bEnabled = bEnable;
}


//-----------------------------------------------------------------------------
//  Lock the "unnamed" color correction lookup
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::LockLookup()
{
	ColorCorrectionLookup_t *lookup = FindLookup( m_UnnamedColorCorrectionHandle );
	if ( !lookup )
	{
		AddLookup( "unnamed" );
	}

	LockLookup( m_UnnamedColorCorrectionHandle );
}

//-----------------------------------------------------------------------------
//  Set the specified mapping for the "unnamed" color correction lookup
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::SetLookup( RGBX5551_t inColor, color24 outColor )
{
	ColorCorrectionLookup_t *lookup = FindLookup( m_UnnamedColorCorrectionHandle );
	if ( !lookup )
		return;

	SetLookup( m_UnnamedColorCorrectionHandle, inColor, outColor );
}

//-----------------------------------------------------------------------------
//  Unlock the "unnamed" color correction lookup
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::UnlockLookup()
{
	ColorCorrectionLookup_t *lookup = FindLookup( m_UnnamedColorCorrectionHandle );
	if ( !lookup )
		return;

	UnlockLookup( m_UnnamedColorCorrectionHandle, true );
}

//-----------------------------------------------------------------------------
//  Get the specified mapping from the "unnamed" color correction lookup
//-----------------------------------------------------------------------------
color24 CColorCorrectionSystem::GetLookup( RGBX5551_t inColor )
{
	ColorCorrectionLookup_t *lookup = FindLookup( m_UnnamedColorCorrectionHandle );
	if ( !lookup )
		AddLookup( "unnamed" );

	return GetLookup( m_UnnamedColorCorrectionHandle, inColor );
}

//-----------------------------------------------------------------------------
//  Load the specified file into the "unnamed" color correction lookup
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::LoadLookup( const char *pLookupName )
{
	ColorCorrectionLookup_t *lookup = FindLookup( m_UnnamedColorCorrectionHandle );
	if ( !lookup )
		return;

	return LoadLookup( m_UnnamedColorCorrectionHandle, pLookupName );
}

//-----------------------------------------------------------------------------
//  Copy the specified data into the "unnamed" color correction lookup
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::CopyLookup( const color24 *pSrcColorCorrection )
{
	ColorCorrectionLookup_t *lookup = FindLookup( m_UnnamedColorCorrectionHandle );
	if ( !lookup )
		return;

	CopyLookup( m_UnnamedColorCorrectionHandle, pSrcColorCorrection );
}

//-----------------------------------------------------------------------------
//  Reset the "unnamed" color correction lookup
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::ResetLookup()
{
	ColorCorrectionLookup_t *lookup = FindLookup( m_UnnamedColorCorrectionHandle );
	if ( !lookup )
		return;

	return ResetLookup( m_UnnamedColorCorrectionHandle );
}

//-----------------------------------------------------------------------------
//  ColorCorrectionLookup_t sorting function
//-----------------------------------------------------------------------------
typedef ColorCorrectionLookup_t * CCLPtr;
int _cdecl CompareLookups( const CCLPtr *lookup_a, const CCLPtr *lookup_b )
{
	if ( (*lookup_a)->m_flWeight < (*lookup_b)->m_flWeight )
		return 1;
	else if ( (*lookup_a)->m_flWeight > (*lookup_b)->m_flWeight )
		return -1;
	return 0;
}

//-----------------------------------------------------------------------------
//  Sort ColorCorrectionLookup_t vector based on decreasing weight
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::SortLookups()
{
	m_ColorCorrectionList.Sort( CompareLookups );

	for ( int i=0;i<COLOR_CORRECTION_MAX_TEXTURES && i<m_ColorCorrectionList.Count();i++ )
	{
		TextureManager()->SetColorCorrectionTexture( i, m_ColorCorrectionList[i]->m_pColorCorrectionTexture );
	}

	for ( int i=m_ColorCorrectionList.Count();i<COLOR_CORRECTION_MAX_TEXTURES;i++ )
	{
		TextureManager()->SetColorCorrectionTexture( i, NULL );
	}
}

//-----------------------------------------------------------------------------
//  Find the ColorCorrectionLookup_t corresponding to the specified handle
//-----------------------------------------------------------------------------
ColorCorrectionLookup_t *CColorCorrectionSystem::FindLookup( ColorCorrectionHandle_t handle )
{
	for ( int i=0; i<m_ColorCorrectionList.Count(); i++ )
	{
		if ( m_ColorCorrectionList[i]->m_Handle == handle )
		{
			return m_ColorCorrectionList[i];
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
//  Find the handle associated with a specified name
//-----------------------------------------------------------------------------
ColorCorrectionHandle_t CColorCorrectionSystem::GetLookupHandle( const char *pName )
{	
	// case and slash insensitive
	FileNameHandle_t hName = g_pFullFileSystem->FindOrAddFileName( pName );
	COMPILE_TIME_ASSERT( sizeof( FileNameHandle_t ) == sizeof( ColorCorrectionHandle_t ) );

	return (ColorCorrectionHandle_t)hName;
}

//-----------------------------------------------------------------------------
//  Add a color correction lookup with the specified name
//-----------------------------------------------------------------------------
ColorCorrectionHandle_t CColorCorrectionSystem::AddLookup( const char *pName )
{
	ColorCorrectionHandle_t handle = GetLookupHandle( pName );
	if ( handle == m_DefaultColorCorrectionHandle )
		return handle;

	ColorCorrectionLookup_t	*lookup = FindLookup( handle );
	if ( lookup )
	{
		// Already got one by this name
		return handle;
	}

	lookup = new ColorCorrectionLookup_t( handle );
	m_ColorCorrectionList.AddToTail( lookup );

	// xbox needs to defer the real download (only once) to when the volume data is pushed in
	// the PC downloads becuase that was the legacy behavior (silly to me)
	bool bDownload = !IsX360();
	
	LockLookup( handle );
	ResetLookup( handle );
	UnlockLookup( handle, bDownload );

	SetLookupWeight( handle, 1.0f );

	return handle;
}

ColorCorrectionHandle_t CColorCorrectionSystem::FindLookup( const char *pName )
{
	ColorCorrectionHandle_t handle = GetLookupHandle( pName );
	if ( handle == m_DefaultColorCorrectionHandle )
		return handle;

	ColorCorrectionLookup_t	*pLookup = FindLookup( handle );
	if ( pLookup )
	{
		// Already got one by this name
		return handle;
	}

	// not found
	return (ColorCorrectionHandle_t)0;
}

//-----------------------------------------------------------------------------
//  Remove the specified color correction lookup
//-----------------------------------------------------------------------------
bool CColorCorrectionSystem::RemoveLookup( ColorCorrectionHandle_t handle )
{
	if ( handle == m_DefaultColorCorrectionHandle )
		return false;

	for ( int i=0;i<m_ColorCorrectionList.Count();i++ )
	{
		ColorCorrectionLookup_t *lookup = m_ColorCorrectionList[i];
		if ( lookup->m_Handle == handle )
		{
			m_ColorCorrectionList.Remove( i );

			delete lookup;

            return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
//  Sets the weight for the specified color correction lookup
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::SetLookupWeight( ColorCorrectionHandle_t handle, float flWeight )
{
	if ( handle == m_DefaultColorCorrectionHandle )
	{
		m_DefaultColorCorrectionWeight = flWeight;
		return;
	}
    
	ColorCorrectionLookup_t *lookup = FindLookup( handle );
	if ( lookup && flWeight>lookup->m_flWeight )
	{
		lookup->m_flWeight = flWeight;
	}

	SortLookups();
}

//-----------------------------------------------------------------------------
//  Gets the weight for the specified color correction lookup
//-----------------------------------------------------------------------------
float CColorCorrectionSystem::GetLookupWeight( ColorCorrectionHandle_t handle )
{
	if ( handle == m_DefaultColorCorrectionHandle )
	{
		return m_DefaultColorCorrectionWeight;
	}

	ColorCorrectionLookup_t *lookup = FindLookup( handle );
	if ( !lookup )
		return 0.0f;

	return lookup->m_flWeight;
}
    
//-----------------------------------------------------------------------------
//  Gets the weight for the color correction lookup specified by index
//-----------------------------------------------------------------------------
float CColorCorrectionSystem::GetLookupWeight( int i )
{
	if ( i<m_ColorCorrectionList.Count() )
	{
		if ( i==-1 )
			return m_DefaultColorCorrectionWeight;

		ColorCorrectionLookup_t *lookup = m_ColorCorrectionList[ i ];
		return lookup->m_flWeight;
	}
	
	return 0.0f;
}

//-----------------------------------------------------------------------------
//  Lock the specified color correction lookup
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::LockLookup( ColorCorrectionHandle_t handle )
{
	if ( handle == m_DefaultColorCorrectionHandle )
	{
		return;
	}

	ColorCorrectionLookup_t *lookup = FindLookup( handle );
	if ( !lookup )
		return;

	lookup->m_bLocked = true;
}

//-----------------------------------------------------------------------------
//  Set the mapping for the specified color correction lookup
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::SetLookup( ColorCorrectionHandle_t handle, RGBX5551_t inColor, color24 outColor )
{
	if ( handle == m_DefaultColorCorrectionHandle )
		return;

	ColorCorrectionLookup_t *lookup = FindLookup( handle );
	if ( !lookup )
		return;

	SetLookupPtr( lookup, inColor, outColor );
}

//-----------------------------------------------------------------------------
//  Set the mapping for the specified color correction lookup
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::SetLookupPtr( ColorCorrectionLookup_t *lookup, RGBX5551_t inColor, color24 outColor )
{
	Assert( lookup->m_bLocked );

	if ( !lookup->m_pColorCorrection )
	{
		// Uh oh, contract broken, the backing volume was freed, no access should have occured
		Assert( 0 );
		return;
	}

	lookup->m_pColorCorrection[inColor.r + inColor.g*COLOR_CORRECTION_TEXTURE_SIZE + inColor.b*COLOR_CORRECTION_TEXTURE_SIZE*COLOR_CORRECTION_TEXTURE_SIZE] = outColor;
}

//-----------------------------------------------------------------------------
//  Get the mapping for the specified color correction lookup
//-----------------------------------------------------------------------------
color24 CColorCorrectionSystem::GetLookup( ColorCorrectionHandle_t handle, RGBX5551_t inColor )
{
	if ( handle == m_DefaultColorCorrectionHandle )
	{
		color24 col = ConvertToColor24( inColor );
		return col;
	}

	ColorCorrectionLookup_t *lookup = FindLookup( handle );
	if ( !lookup )
	{
		color24 col; col.r = 0; col.g = 0; col.b = 0;
		return col;
	}

	if ( !lookup->m_pColorCorrection )
	{
		// Uh oh, contract broken, the backing volume was freed, no access should have occured
		Assert( 0 );
		color24 col; col.r = 0; col.g = 0; col.b = 0;
		return col;
	}

	return lookup->m_pColorCorrection[inColor.r + inColor.g*COLOR_CORRECTION_TEXTURE_SIZE + inColor.b*COLOR_CORRECTION_TEXTURE_SIZE*COLOR_CORRECTION_TEXTURE_SIZE];
}

//-----------------------------------------------------------------------------
//  Unlock the specified color correction lookup
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::UnlockLookup( ColorCorrectionHandle_t handle, bool bDownload )
{
	if ( handle == m_DefaultColorCorrectionHandle )
	{
		return;
	}

	ColorCorrectionLookup_t *lookup = FindLookup( handle );
	if ( !lookup )
		return;

	lookup->m_bLocked = false;

	if ( bDownload )
	{
		lookup->m_pColorCorrectionTexture->Download();
	}
}

//-----------------------------------------------------------------------------
//  Load the specified file into the color correction lookup
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::LoadLookup( ColorCorrectionHandle_t handle, const char *pLookupName )
{
	if ( handle == m_DefaultColorCorrectionHandle )
	{
		return;
	}

	ColorCorrectionLookup_t *lookup = FindLookup( handle );
	if ( !lookup )
		return;

	Assert( lookup->m_bLocked );
	Assert( pLookupName );

	char szXboxName[MAX_PATH];
	if ( IsX360() || IsPS3() )
	{
		pLookupName = CreatePlatformFilename( pLookupName, szXboxName, sizeof( szXboxName ) );
	}

	CUtlBuffer colorBuff;
	if ( !g_pFullFileSystem->ReadFile( pLookupName, "GAME", colorBuff ) )
	{
		Warning( "CColorCorrectionSystem: Missing '%s'\n", pLookupName );
		return;
	}

	int res = (int)powf( (float)( colorBuff.TellMaxPut()/sizeof( color24 ) ), 1.0f/3.0f );
	if ( res != COLOR_CORRECTION_TEXTURE_SIZE || res*res*res*(int)sizeof( color24 ) != colorBuff.TellMaxPut() )
	{
		Warning( "CColorCorrectionSystem: Raw file '%s' has bad size (%d)\n", pLookupName, colorBuff.TellMaxPut() );
		return;
	}

	color24 *pColors = (color24 *)colorBuff.Base();
	int colorIndex = 0;
	RGBX5551_t inColor;

	inColor.b = 0;
	for ( int b = 0; b < COLOR_CORRECTION_TEXTURE_SIZE; ++b, ++inColor.b )
	{
		inColor.g = 0;
		for ( int g = 0; g < COLOR_CORRECTION_TEXTURE_SIZE; ++g, ++inColor.g )
		{
			inColor.r = 0;
			for ( int r = 0; r < COLOR_CORRECTION_TEXTURE_SIZE; ++r, ++inColor.r )
			{
				color24 vOutColor24 = pColors[colorIndex];
				SetLookupPtr( lookup, inColor, vOutColor24 );
				colorIndex++;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//  Copy the data to the specified color correction lookup
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::CopyLookup( ColorCorrectionHandle_t handle, const color24 *pSrcColorCorrection )
{
	if ( handle == m_DefaultColorCorrectionHandle )
	{
		return;
	}

	ColorCorrectionLookup_t *lookup = FindLookup( handle );
	if ( !lookup )
		return;

	Assert( lookup->m_bLocked );
	Assert( pSrcColorCorrection );

	if ( !lookup->m_pColorCorrection )
	{
		// Uh oh, contract broken, the backing volume was freed, no access should have occured
		Assert( 0 );
		return;
	}

	Q_memcpy( lookup->m_pColorCorrection, pSrcColorCorrection, sizeof(color24)*COLOR_CORRECTION_TEXTURE_SIZE*COLOR_CORRECTION_TEXTURE_SIZE*COLOR_CORRECTION_TEXTURE_SIZE );
}

//-----------------------------------------------------------------------------
//  Reset the mapping for the specified color correction lookup
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::ResetLookup( ColorCorrectionHandle_t handle )
{
	if ( handle == m_DefaultColorCorrectionHandle )
	{
		return;
	}

	ColorCorrectionLookup_t *lookup = FindLookup( handle );
	if ( !lookup )
		return;

	Assert( lookup->m_bLocked );

	RGBX5551_t inColor;

	inColor.b = 0;
	for ( int b = 0; b < COLOR_CORRECTION_TEXTURE_SIZE; ++b, ++inColor.b )
	{
		inColor.g = 0;
		for ( int g = 0; g < COLOR_CORRECTION_TEXTURE_SIZE; ++g, ++inColor.g )
		{
			inColor.r = 0;
			for ( int r = 0; r < COLOR_CORRECTION_TEXTURE_SIZE; ++r, ++inColor.r )
			{
				color24 outColor = ConvertToColor24( inColor );
				SetLookupPtr( lookup, inColor, outColor );
			}
		}
	}
}

//-----------------------------------------------------------------------------
//  Reset the weight to zero for all lookups
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::ResetLookupWeights()
{
    m_DefaultColorCorrectionWeight = 0.0f;

	for ( int i=0;i<m_ColorCorrectionList.Count();i++ )
	{
		ColorCorrectionLookup_t *lookup = m_ColorCorrectionList[i];
		if ( lookup->m_bResetable )
		{
			lookup->m_flWeight = 0.0f;
		}
		else
		{
			lookup->m_flWeight = 1.0f;
		}
	}
}

//-----------------------------------------------------------------------------
//  Convert a color from RGBX5551_t to color24
//-----------------------------------------------------------------------------
color24 CColorCorrectionSystem::ConvertToColor24( RGBX5551_t inColor )
{
	color24 out;
	out.r = (inColor.r << 3) | (inColor.r >> 2);
	out.g = (inColor.g << 3) | (inColor.g >> 2);
	out.b = (inColor.b << 3) | (inColor.b >> 2);
	return out;
}

//-----------------------------------------------------------------------------
//  Call ReleaseTexture for all ColorCorrectionLookup_t
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::ReleaseTextures()
{
	if ( IsX360() )
	{
		// there is no release/restore ability on the xbox
		Assert( 0 );
		return;
	}

	for ( int i=0;i<m_ColorCorrectionList.Count();i++ )
	{
		m_ColorCorrectionList[i]->ReleaseTexture();
	}
}

//-----------------------------------------------------------------------------
//  Call RestoreTexture for all ColorCorrectionLookup_t
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::RestoreTextures()
{
	if ( IsX360() )
	{
		// there is no release/restore ability on the xbox
		Assert( 0 );
		return;
	}

	for ( int i=0;i<m_ColorCorrectionList.Count();i++ )
	{
		m_ColorCorrectionList[i]->RestoreTexture();
	}
}

//-----------------------------------------------------------------------------
//  Normalize the active set of color correction weights
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::GetNormalizedWeights( float *pDefaultWeight, float *pLookupWeights )
{
	float total_weight = 0.0f;
	int nLoopCount = MIN( m_ColorCorrectionList.Count(), COLOR_CORRECTION_MAX_TEXTURES );
	for ( int i=0; i<nLoopCount; i++ )
	{
		total_weight += m_ColorCorrectionList[i]->m_flWeight;
		pLookupWeights[i] = m_ColorCorrectionList[i]->m_flWeight;
	}

	for ( int i = nLoopCount; i < COLOR_CORRECTION_MAX_TEXTURES; ++i )
	{
		pLookupWeights[i] = 0.0f;
	}

	if ( total_weight <= ( 1.0f - 1e-3 ) )
	{
		*pDefaultWeight = 1.0f - total_weight;
	}
	else
	{
		*pDefaultWeight = 0.0f;

		float inv_total_weight = 1.0f / total_weight;
		for ( int i=0; i< nLoopCount; i++ )
		{
			pLookupWeights[i] *= inv_total_weight;
		}
	}
}

//-----------------------------------------------------------------------------
//  Returns the number of active lookup
//-----------------------------------------------------------------------------
int CColorCorrectionSystem::GetNumLookups()
{
	int i;
	for ( i=0;i<m_ColorCorrectionList.Count()&&i<COLOR_CORRECTION_MAX_TEXTURES;i++ )
	{
		 if ( m_ColorCorrectionList[i]->m_flWeight<=0.0f )
			 break;
	}

	return i;
}

//-----------------------------------------------------------------------------
// Enables/disables resetting of this lookup
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::SetResetable( ColorCorrectionHandle_t handle, bool bResetable )
{
	ColorCorrectionLookup_t *lookup = FindLookup( handle );
	if ( lookup )
	{
		lookup->m_bResetable = bResetable;
	}
}

//-----------------------------------------------------------------------------
// Returns info for the shaders to control color correction
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo )
{
	COMPILE_TIME_ASSERT( COLOR_CORRECTION_MAX_TEXTURES == ARRAYSIZE( pInfo->m_pLookupWeights ) );
	pInfo->m_bIsEnabled = m_bEnabled && ( GetNumLookups() > 0 || m_DefaultColorCorrectionWeight != 0.0f );
	pInfo->m_nLookupCount = GetNumLookups();
	GetNormalizedWeights( &pInfo->m_flDefaultWeight, pInfo->m_pLookupWeights );
}

void CColorCorrectionSystem::OnProceduralRegenComplete( ColorCorrectionHandle_t handle )
{
	if ( !IsX360() )
	{
		// xbox only
		return;
	}

	ColorCorrectionLookup_t *pLookup = FindLookup( handle );
	if ( !pLookup )
	{
		// huh? should have been there
		Assert( 0 );
		return;
	}

	// Based on nefarious knowledge, the backing volume can be ditched on the xbox.
	// The xbox only uses file based color correction, and does not support release/restore.
	// For memory savings on L4D, at 98k per instance, l42_hospita01_aparments with ~20
	// correction volumes, saves 2MB. The inept design enforces a unique instance per weight/entity.
	delete [] pLookup->m_pColorCorrection;
	// mark as free, used to check validity of this hack
	pLookup->m_pColorCorrection = NULL;
}
