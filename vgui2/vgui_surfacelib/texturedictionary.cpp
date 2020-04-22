//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Contains all texture state for the material system surface to use
//
//===========================================================================//

#include "bitmap/imageformat.h"
#include "vgui_surfacelib/texturedictionary.h"
#include "utllinkedlist.h"
#include "checksum_crc.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "tier0/dbg.h"
#include "keyvalues.h"
#include "pixelwriter.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "vtf/vtf.h"
#include "rendersystem/irenderdevice.h"
#include "rendersystem/irendercontext.h"
#include "resourcesystem/stronghandle.h"
#include "utldict.h"
#include "vgui/ISystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define TEXTURE_ID_UNKNOWN	-1

class CMatSystemTexture;

// Case-sensitive string checksum
static CRC32_t Texture_CRCName( const char *string )
{
	CRC32_t crc;
	
	CRC32_Init( &crc );
	CRC32_ProcessBuffer( &crc, (void *)string, strlen( string ) );
	CRC32_Final( &crc );

	return crc;
}

// [smessick] Less than function for Xuids.
#if defined( _X360 )
static bool XuidLessFunc( const XUID &lhs, const XUID &rhs )
{
	return lhs < rhs;
}
#endif // _X360

class CFontTextureRegen;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CMatSystemTexture
{
public:
	CMatSystemTexture( void );
	~CMatSystemTexture( void );

	void SetId( int id ) { m_ID = id; };

	CRC32_t	GetCRC() const;
	void SetCRC( CRC32_t val );

	void SetMaterial( const char *pFileName );
	void SetMaterial( IMaterial *pMaterial );

	// This is used when we want different rendering state sharing the same procedural texture (fonts)
	void ReferenceOtherProcedural( CMatSystemTexture *pTexture, IMaterial *pMaterial );

	// Source1 version
	IMaterial *GetMaterial() { return m_pMaterial; }
	// Source2 version
	HRenderTexture GetTextureHandle(){ return m_Texture2; }
	int Width() const { return m_iWide; }
	int Height() const { return m_iTall; }

	bool IsProcedural( void ) const;
	void SetProcedural( bool proc );

 	bool IsReference() const { return ( m_Flags & TEXTURE_IS_REFERENCE ) ? true : false; }

	// Source1 version
	void SetTextureRGBA( const char* rgba, int wide, int tall, ImageFormat format, ETextureScaling eScaling );
	
	// Source2 version
	void SetTextureRGBA( IRenderDevice *pRenderDevice, const char *rgba, int width, int height, ImageFormat format, ETextureScaling eScaling );
	
	// Source1 version
	void SetSubTextureRGBA( int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall );
	void SetSubTextureRGBAEx( int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat imageFormat );
	
	// Source2 version
	void SetSubTextureRGBA( IRenderDevice *pRenderDevice, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall );
	
	void UpdateSubTextureRGBA( int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat imageFormat );

#if defined( _X360 )

	// Update the local gamerpic texture.
	virtual bool SetLocalGamerpicTexture( DWORD userIndex );

	// Update the given texture with a remote player's gamerpic.
	virtual bool SetRemoteGamerpicTexture( XUID xuid );

#endif // _X360

	float	m_s0, m_t0, m_s1, m_t1;

private:
	void CreateRegen( int nWidth, int nHeight, ImageFormat format );
	void ReleaseRegen( void );
	void CleanUpMaterial();

	ITexture *GetTextureValue( void );

#if defined( _X360 )
	// Create the gamerpic material and texture.
	void CreateGamerpicTexture( void );
#endif // _X360

private:
	enum
	{
		TEXTURE_IS_PROCEDURAL = 0x1,
		TEXTURE_IS_REFERENCE = 0x2
	};

	CRC32_t				m_crcFile;
	IMaterial			*m_pMaterial;
	ITexture			*m_pTexture;  // Source1 texture support
	HRenderTextureStrong m_Texture2;	 // Source2 texture support
	int					m_iWide;
	int					m_iTall;
	int					m_iInputWide;
	int					m_iInputTall;
	int					m_ID;
	int					m_Flags;
	CFontTextureRegen	*m_pRegen;
};


//-----------------------------------------------------------------------------
// A class that manages textures used by the material system surface
//-----------------------------------------------------------------------------
class CTextureDictionary : public ITextureDictionary
{
public:
	CTextureDictionary( void );

	void SetRenderDevice( IRenderDevice *pRenderDevice ); // Source2 support

	// Create, destroy textures
	int	CreateTexture( bool procedural = false );
	void DestroyTexture( int id );
	void DestroyAllTextures();

	// Is this a valid id?
	bool IsValidId( int id ) const;

	// Binds a material to a texture
	virtual void BindTextureToFile( int id, const char *pFileName );
	virtual void BindTextureToMaterial( int id, IMaterial *pMaterial );
	virtual void BindTextureToMaterialReference( int id, int referenceId, IMaterial *pMaterial );

	// Texture info
	IMaterial *GetTextureMaterial( int id ); // Source1 version
	HRenderTexture GetTextureHandle( int textureId ); // Source2 version
	void GetTextureSize(int id, int& iWide, int& iTall );
	void GetTextureTexCoords( int id, float &s0, float &t0, float &s1, float &t1 );

	void SetTextureRGBA( int id, const char* rgba, int wide, int tall );
	void SetTextureRGBAEx( int id, const char* rgba, int wide, int tall, ImageFormat format, ETextureScaling eScaling );
	
	void SetSubTextureRGBA( int id, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall );
	void SetSubTextureRGBAEx( int id, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat imageFormat );
	
	void UpdateSubTextureRGBA( int id, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat imageFormat );

	int	FindTextureIdForTextureFile( char const *pFileName );

	virtual void BindTextureToMaterial2Reference( int id, int referenceId, IMaterial2 *pMaterial ) { Assert(0); }
	virtual void BindTextureToMaterial2( int id, IMaterial2 *pMaterial ) { Assert(0); }
	virtual IMaterial2 *GetTextureMaterial2( int id ) { Assert(0); return NULL; }

#if defined( _X360 )

	//
	// Local gamerpic
	//

	// Get the texture id for the local gamerpic.
	virtual int GetLocalGamerpicTextureID( void ) const { return m_LocalGamerpicTextureID; }

	// Update the local gamerpic texture. Use the given texture if a gamerpic cannot be loaded.
	virtual bool SetLocalGamerpicTexture( DWORD userIndex, const char *pDefaultGamerpicFileName );

	//
	// Remote gamerpic
	//

	// Get the texture id for a remote gamerpic with the given xuid.
	virtual int GetRemoteGamerpicTextureID( XUID xuid );

	// Update the remote gamerpic texture for the given xuid. Use the given texture if a gamerpic cannot be loaded.
	virtual bool SetRemoteGamerpicTextureID( XUID xuid, const char *pDefaultGamerpicFileName );

#endif // _X360

public:
	CMatSystemTexture	*GetTexture( int id );

private:

#if defined( _X360 )

	// Create a new remote gamerpic texture entry in the map.
	bool CreateRemoteGamerpicTexture( XUID xuid );

	// Create a default remote gamerpic texture entry in the map.
	bool CreateDefaultRemoteGamerpicTexture( XUID xuid, const char *pDefaultGamerpicFileName );

#endif // _X360

	CUtlLinkedList< CMatSystemTexture, unsigned short >	m_Textures;	// Source1 textures
	CUtlDict< int, int> m_TextureIDs;

#if defined( _X360 )

	int m_LocalGamerpicTextureID; // texture index for the local gamerpic

	struct RemoteGamerpicData
	{
		int m_TextureID; // index of the remote gamerpic texture
		double m_TimeStamp; // last access time
	};

	enum
	{
		MAX_REMOTE_GAMERPIC_TEXTURES = 32
	};

	CUtlMap< XUID, RemoteGamerpicData > m_RemoteGamerpicTextureIDMap; // map of xuids to texture ids

#endif // _X360
};

static CTextureDictionary s_TextureDictionary;


//-----------------------------------------------------------------------------
// A texture regenerator that holds onto the bits at all times
//-----------------------------------------------------------------------------
class CFontTextureRegen : public ITextureRegenerator
{
public:
	CFontTextureRegen( int nWidth, int nHeight, ImageFormat format )
	{
		m_nFormat = format;
		m_nWidth  = nWidth;
		m_nHeight = nHeight;

		if ( IsPC() )
		{
			int size = ImageLoader::GetMemRequired( m_nWidth, m_nHeight, 1, m_nFormat, false );
			m_pTextureBits = new unsigned char[size];
			memset( m_pTextureBits, 0, size );
		}
		else
		{
			// will be allocated as needed
			m_pTextureBits = NULL;
		}
	}

	~CFontTextureRegen( void )
	{
		DeleteTextureBits();
	}

	void UpdateBackingBits( Rect_t &subRect, const unsigned char *pBits, Rect_t &uploadRect, ImageFormat format )
	{
		int size = ImageLoader::GetMemRequired( m_nWidth, m_nHeight, 1, m_nFormat, false );
		if ( IsPC() )
		{
			if ( !m_pTextureBits )
				return;
		}
		else
		{
			Assert( !m_pTextureBits );
			m_pTextureBits = new unsigned char[size];
			memset( m_pTextureBits, 0, size );
		}

		// Copy subrect into backing bits storage
		// source data is expected to be in same format as backing bits
		int y;
		if ( ImageLoader::SizeInBytes( m_nFormat ) == 4 )
		{
			bool bIsInputFullRect = ( subRect.width != uploadRect.width || subRect.height != uploadRect.height );
			Assert( (subRect.x >= 0) && (subRect.y >= 0) );
			Assert( (subRect.x + subRect.width <= m_nWidth) && (subRect.y + subRect.height <= m_nHeight) );
			for ( y=0; y < subRect.height; ++y )
			{
				int idx = ( (subRect.y + y) * m_nWidth + subRect.x ) << 2;
				unsigned int *pDst = (unsigned int*)(&m_pTextureBits[ idx ]);
				int offset = bIsInputFullRect ? (subRect.y+y)*uploadRect.width + subRect.x : y*uploadRect.width;
				const unsigned int *pSrc = (const unsigned int *)(&pBits[ offset << 2 ]);
				ImageLoader::ConvertImageFormat( (const unsigned char *)pSrc, format,(unsigned char *)pDst, m_nFormat, subRect.width, 1 );
			}
		}
		else
		{
			// cannot subrect copy when format is not RGBA
			if ( subRect.width != m_nWidth || subRect.height != m_nHeight )
			{
				Assert( 0 );
				return;
			}
			Q_memcpy( m_pTextureBits, pBits, size );
		}
	}

	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect )
	{
		if ( !m_pTextureBits )
			return;

		Assert( (pVTFTexture->Width() == m_nWidth) && (pVTFTexture->Height() == m_nHeight) );

		int nFormatBytes = ImageLoader::SizeInBytes( m_nFormat );
		if ( nFormatBytes == 4 )
		{
			if ( m_nFormat == pVTFTexture->Format() )
			{
				int ymax = pSubRect->y + pSubRect->height;
				for( int y = pSubRect->y; y < ymax; ++y )
				{
					// copy each row across for the update
					char *pchData = (char *)pVTFTexture->ImageData( 0, 0, 0, 0, y ) + pSubRect->x *nFormatBytes;
					int size = ImageLoader::GetMemRequired( pSubRect->width, 1, 1, m_nFormat, false );
					V_memcpy( pchData, m_pTextureBits + (y * m_nWidth + pSubRect->x) * nFormatBytes, size );
				}
			}
			else
			{
				// formats don't match so do a pixel by pixel swizel
				CPixelWriter pixelWriter;
				pixelWriter.SetPixelMemory( 
					pVTFTexture->Format(), 
					pVTFTexture->ImageData( 0, 0, 0 ), 
					pVTFTexture->RowSizeInBytes( 0 ) );

				// Now upload the part we've been asked for
				int xmax = pSubRect->x + pSubRect->width;
				int ymax = pSubRect->y + pSubRect->height;
				int x, y;

				for( y = pSubRect->y; y < ymax; ++y )
				{
 					pixelWriter.Seek( pSubRect->x, y );
					unsigned char *rgba = &m_pTextureBits[ (y * m_nWidth + pSubRect->x) * nFormatBytes ];

					for ( x=pSubRect->x; x < xmax; ++x )
					{
						pixelWriter.WritePixel( rgba[0], rgba[1], rgba[2], rgba[3] );
						rgba += nFormatBytes;
					}
				}
			}
		}
		else
		{
			// cannot subrect copy when format is not RGBA
			if ( pSubRect->width != m_nWidth || pSubRect->height != m_nHeight )
			{
				Assert( 0 );
				return;
			}
			int size = ImageLoader::GetMemRequired( m_nWidth, m_nHeight, 1, m_nFormat, false );
			Q_memcpy( pVTFTexture->ImageData( 0, 0, 0 ), m_pTextureBits, size );
		}
	}

	virtual void Release()
	{
		// Called by the material system when this needs to go away
		DeleteTextureBits();
	}

	void DeleteTextureBits()
	{
		if ( m_pTextureBits )
		{
			delete [] m_pTextureBits;
			m_pTextureBits = NULL;
		}
	}

private:
	unsigned char	*m_pTextureBits;
	short			m_nWidth;
	short			m_nHeight;
	ImageFormat		m_nFormat;
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMatSystemTexture::CMatSystemTexture( void )
{
	m_Texture2 = RENDER_TEXTURE_HANDLE_INVALID;
	m_pMaterial = NULL;
	m_pTexture = NULL;
	m_crcFile = (CRC32_t)0;
	m_iWide = m_iTall = 0;
	m_s0 = m_t0 = 0;
	m_s1 = m_t1 = 1;

	m_Flags = 0;
	m_pRegen = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMatSystemTexture::~CMatSystemTexture( void )
{
	CleanUpMaterial();
}

bool CMatSystemTexture::IsProcedural( void ) const
{
	return (m_Flags & TEXTURE_IS_PROCEDURAL) != 0;
}

void CMatSystemTexture::SetProcedural( bool proc )
{
	if (proc)
	{
		m_Flags |= TEXTURE_IS_PROCEDURAL;
	}
	else
	{
		m_Flags &= ~TEXTURE_IS_PROCEDURAL;
	}
}

void CMatSystemTexture::CleanUpMaterial()
{
	if ( m_pMaterial )
	{
		// causes the underlying texture (if unreferenced) to be deleted as well
		m_pMaterial->DecrementReferenceCount();
		m_pMaterial->DeleteIfUnreferenced();
		m_pMaterial = NULL;
	}

	if ( m_pTexture )
	{
		m_pTexture->SetTextureRegenerator( NULL );
		m_pTexture->DecrementReferenceCount();
		m_pTexture->DeleteIfUnreferenced();
		m_pTexture = NULL;
	}

	ReleaseRegen();
}

void CMatSystemTexture::CreateRegen( int nWidth, int nHeight, ImageFormat format )
{
	Assert( IsProcedural() );

	if ( !m_pRegen )
	{
		m_pRegen = new CFontTextureRegen( nWidth, nHeight, format );
	}
}

void CMatSystemTexture::ReleaseRegen( void )
{
	if (m_pRegen)
	{
		if (!IsReference())
		{
			delete m_pRegen;
		}

		m_pRegen = NULL;
	}
}

// Source1 version

// [smessick] Moved this outside of SetTextureRGBA.
static int s_nTextureId = 0;

void CMatSystemTexture::SetTextureRGBA( const char *rgba, int wide, int tall, ImageFormat format, ETextureScaling eScaling )
{
	Assert( IsProcedural() );
	if ( !IsProcedural() )
		return;

	if ( !m_pMaterial )
	{
		int width = wide;
		int height = tall;

		// Create a procedural material to fit this texture into
		char pTextureName[64];
		Q_snprintf( pTextureName, sizeof( pTextureName ), "__vgui_texture_%d", s_nTextureId );
		++s_nTextureId;

		int nFlags = TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | 
			TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_SRGB |
			TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_SINGLECOPY;

		if ( eScaling == k_ETextureScalingPointSample )
			nFlags |= TEXTUREFLAGS_POINTSAMPLE;

		Assert( g_pMaterialSystem );
		ITexture *pTexture = g_pMaterialSystem->CreateProceduralTexture( 
			pTextureName, 
			TEXTURE_GROUP_VGUI, 
			width,
			height,
			format,
			nFlags  );

		KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
		pVMTKeyValues->SetInt( "$vertexcolor", 1 );
		pVMTKeyValues->SetInt( "$vertexalpha", 1 );
		pVMTKeyValues->SetInt( "$ignorez", 1 );
		pVMTKeyValues->SetInt( "$no_fullbright", 1 );
		pVMTKeyValues->SetInt( "$translucent", 1 );
		pVMTKeyValues->SetString( "$basetexture", pTextureName );

		IMaterial *pMaterial = g_pMaterialSystem->CreateMaterial( pTextureName, pVMTKeyValues );
		pMaterial->Refresh();

		// Has to happen after the refresh
		pTexture->DecrementReferenceCount();

		SetMaterial( pMaterial );
		
		m_iInputTall = tall;
		m_iInputWide = wide;

		// Undo the extra +1 refCount
		pMaterial->DecrementReferenceCount();
	}

	Assert( wide <= m_iWide );
	Assert( tall <= m_iTall );

	// Just replace the whole thing
	SetSubTextureRGBAEx( 0, 0, (const unsigned char *)rgba, wide, tall, format );
}

//-----------------------------------------------------------------------------
// Source2 version
//-----------------------------------------------------------------------------
void CMatSystemTexture::SetTextureRGBA( IRenderDevice *pRenderDevice, const char *rgba, int width, int height, ImageFormat format, ETextureScaling eScaling )
{
	if ( m_Texture2 == RENDER_TEXTURE_HANDLE_INVALID )
	{
		static intp s_UniqueID = 0x4500000;
		TextureHeader_t spec;
		memset( &spec, 0, sizeof(TextureHeader_t) );
		spec.m_nWidth = width;
		spec.m_nHeight = height;
		spec.m_nNumMipLevels = 1; 
		spec.m_nDepth = 1;
		spec.m_nImageFormat = IMAGE_FORMAT_RGBA8888;
		char pResourceName[16];
		Q_snprintf( pResourceName, sizeof(pResourceName), "%d", s_UniqueID );
		m_Texture2 = pRenderDevice->FindOrCreateTexture( "matsystemtexture", pResourceName, &spec );
		s_UniqueID++;
	}

	// This is normally set in SetMaterial(), since source2 has no materials just going to use the texture's size for these bounds.
	m_iWide = width;
	m_iTall = height;

	// Just replace the whole thing
	SetSubTextureRGBA( pRenderDevice, 0, 0, (const unsigned char *)rgba, width, height );
}

#if defined( _X360 )

//-----------------------------------------------------------------------------
// Create the gamerpic material and texture.
//-----------------------------------------------------------------------------
void CMatSystemTexture::CreateGamerpicTexture( void )
{
	Assert( m_pMaterial == NULL );

	// create a procedural material to fit this texture into
	char pTextureName[64];
	Q_snprintf( pTextureName, sizeof( pTextureName ), "__vgui_texture_%d", s_nTextureId );
	++s_nTextureId;

	ITexture *pTexture = g_pMaterialSystem->CreateGamerpicTexture( 
		pTextureName, 
		TEXTURE_GROUP_VGUI, 
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | 
		TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD | 
		TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_POINTSAMPLE );
	Assert( pTexture != NULL );

	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetInt( "$vertexcolor", 1 );
	pVMTKeyValues->SetInt( "$vertexalpha", 1 );
	pVMTKeyValues->SetInt( "$ignorez", 1 );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$translucent", 1 );
	pVMTKeyValues->SetString( "$basetexture", pTextureName );

	IMaterial *pMaterial = g_pMaterialSystem->CreateMaterial( pTextureName, pVMTKeyValues );
	pMaterial->Refresh();

	// Has to happen after the refresh
	pTexture->DecrementReferenceCount();

	SetMaterial( pMaterial );

	// undo the extra +1 refCount
	pMaterial->DecrementReferenceCount();
}

//-----------------------------------------------------------------------------
// Update the given texture with the player gamerpic for the local player at the given index.
//-----------------------------------------------------------------------------
bool CMatSystemTexture::SetLocalGamerpicTexture( DWORD userIndex )
{
	Assert( IsProcedural() );
	if ( !IsProcedural() )
	{
		return false;
	}

	// Create the material and texture.
	if ( !m_pMaterial )
	{
		CreateGamerpicTexture();
	}

	// Update the gamerpic texture.
	if ( m_pMaterial != NULL )
	{
		return g_pMaterialSystem->UpdateLocalGamerpicTexture( m_pTexture, userIndex );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Update the given texture with a remote player's gamerpic.
//-----------------------------------------------------------------------------
bool CMatSystemTexture::SetRemoteGamerpicTexture( XUID xuid )
{
	Assert( IsProcedural() );
	if ( !IsProcedural() )
	{
		return false;
	}

	// Create the material and texture.
	if ( !m_pMaterial )
	{
		CreateGamerpicTexture();
	}

	// Update the gamerpic texture.
	if ( m_pMaterial != NULL )
	{
		return g_pMaterialSystem->UpdateRemoteGamerpicTexture( m_pTexture, xuid );
	}

	return false;
}

#endif // _X360

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ITexture *CMatSystemTexture::GetTextureValue( void )
{
	Assert( IsProcedural() );
	if ( !m_pMaterial )
		return NULL;

	return m_pTexture;
}

void CMatSystemTexture::SetSubTextureRGBA( int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall )
{
	SetSubTextureRGBAEx( drawX, drawY, rgba, subTextureWide, subTextureTall, IMAGE_FORMAT_RGBA8888 );
}

void CMatSystemTexture::SetSubTextureRGBAEx( int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat format )
{
	ITexture *pTexture = GetTextureValue();
	if ( !pTexture )
		return;

	Assert( IsProcedural() );
	if ( !IsProcedural() )
		return;

	Assert( drawX < m_iWide );
	Assert( drawY < m_iTall );
	Assert( drawX + subTextureWide <= m_iWide );
	Assert( drawY + subTextureTall <= m_iTall );

	Assert( m_pRegen );

	Assert( rgba );

	Rect_t subRect;
	subRect.x = drawX;
	subRect.y = drawY;
	subRect.width = MIN(subTextureWide,m_iWide-drawX);
	subRect.height = MIN(subTextureTall,m_iTall-drawY);

	Rect_t textureSize;
	textureSize.x = 0;
	textureSize.y = 0;
	textureSize.width = subTextureWide;
	textureSize.height = subTextureTall;

	m_pRegen->UpdateBackingBits( subRect, rgba, textureSize, format );
	pTexture->Download( &subRect );

	if ( IsGameConsole() )
	{	
		// xboxissue - no need to persist "backing bits", saves memory
		// the texture (commonly font page) "backing bits" are allocated during UpdateBackingBits() which get blitted
		// into by procedural regeneration in preparation for download() which then subrect blits
		// out of and into target texture (d3d upload)
		// the "backing bits" are then no longer required
		m_pRegen->DeleteTextureBits();
	}
}

void CMatSystemTexture::UpdateSubTextureRGBA( int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat imageFormat )
{
	ITexture *pTexture = GetTextureValue();
	if ( !pTexture )
		return;

	Assert( IsProcedural() );
	if ( !IsProcedural() )
		return;

	Assert( drawX < m_iWide );
	Assert( drawY < m_iTall );
	Assert( drawX + subTextureWide <= m_iWide );
	Assert( drawY + subTextureTall <= m_iTall );

	Assert( m_pRegen );

	Assert( rgba );

	Rect_t subRect;
	subRect.x = drawX;
	subRect.y = drawY;
	subRect.width = subTextureWide;
	subRect.height = subTextureTall;

	Rect_t textureSize;
	textureSize.x = 0;
	textureSize.y = 0;
	textureSize.width = m_iInputWide;
	textureSize.height = m_iInputTall;

	m_pRegen->UpdateBackingBits( subRect, rgba, textureSize, imageFormat );
	pTexture->Download( &subRect );

	if ( IsGameConsole() )
	{	
		// xboxissue - no need to persist "backing bits", saves memory
		// the texture (commonly font page) "backing bits" are allocated during UpdateBackingBits() which get blitted
		// into by procedural regeneration in preparation for download() which then subrect blits
		// out of and into target texture (d3d upload)
		// the "backing bits" are then no longer required
		m_pRegen->DeleteTextureBits();
	}
}

//-----------------------------------------------------------------------------
// Source2 version
//-----------------------------------------------------------------------------
void CMatSystemTexture::SetSubTextureRGBA( IRenderDevice *pRenderDevice, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall )
{
	Assert( m_Texture2 != RENDER_TEXTURE_HANDLE_INVALID );

	CRenderContextPtr pRenderContext( pRenderDevice, RENDER_TARGET_BINDING_BACK_BUFFER );	
	Rect_t myRect = { drawX, drawY, subTextureWide, subTextureTall };
	pRenderContext->SetTextureData( m_Texture2, NULL, rgba, subTextureWide * subTextureTall * 4, false, 0, &myRect );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatSystemTexture::SetCRC( CRC32_t val )
{
	m_crcFile = val;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CRC32_t CMatSystemTexture::GetCRC() const
{ 
	return m_crcFile; 
}

//-----------------------------------------------------------------------------
// Source1 version
//-----------------------------------------------------------------------------
void CMatSystemTexture::SetMaterial( IMaterial *pMaterial )
{
	if ( !g_pMaterialSystem )
		return;
	
	if ( pMaterial )
	{
		// Increment the reference count of the new material
		// Do it before cleaning up the old material just in
		// case the old material refers to the new one, we
		// wouldn't want to destroy the new material.
		pMaterial->IncrementReferenceCount();
	}
	
	// Decrement references to old texture
	CleanUpMaterial();

	m_pMaterial = pMaterial;

	if (!m_pMaterial)
	{
		m_iWide = m_iTall = 0;
		m_s0 = m_t0 = 0.0f;
		m_s1 = m_t1 = 1.0f;
		return;
	}

	// Compute texture size
	m_iWide = m_pMaterial->GetMappingWidth();
	m_iTall = m_pMaterial->GetMappingHeight();

	// Compute texture coordinates
	float flPixelCenterX = 0.0f;
	float flPixelCenterY = 0.0f;

	// TOGL Linux/Win now automatically accounts for the half pixel offset between D3D9 vs. GL, but OSX's version of togl doesn't
	if ( !IsOSX() && !IsX360() )
	{
		// only do texel fudges on D3D
		if ( m_iWide > 0.0f && m_iTall > 0.0f)
		{
			flPixelCenterX = 0.5f / m_iWide;
			flPixelCenterY = 0.5f / m_iTall;
		}
	}

	m_s0 = flPixelCenterX;
	m_t0 = flPixelCenterY;

	// FIXME: Old code used +, it should be - yes?!??!
	m_s1 = 1.0 - flPixelCenterX;
	m_t1 = 1.0 - flPixelCenterY;

	if ( IsProcedural() )
	{
		bool bFound;
		IMaterialVar *pNameVar = m_pMaterial->FindVar( "$baseTexture", &bFound );
		if ( bFound && pNameVar->IsDefined() )
		{
			m_pTexture = pNameVar->GetTextureValue();
			if ( m_pTexture )
			{
				m_pTexture->IncrementReferenceCount();

				// Upload new data
				CreateRegen( m_iWide, m_iTall, m_pTexture->GetImageFormat() );
				m_pTexture->SetTextureRegenerator( m_pRegen );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// This is used when we want different rendering state sharing the same procedural texture (fonts)
//-----------------------------------------------------------------------------
void CMatSystemTexture::ReferenceOtherProcedural( CMatSystemTexture *pTexture, IMaterial *pMaterial )
{
	// Decrement references to old texture
	CleanUpMaterial();

	Assert( pTexture->IsProcedural() );

	m_Flags |= TEXTURE_IS_REFERENCE;

	m_pMaterial = pMaterial;

	if (!m_pMaterial)
	{
		m_iWide = m_iTall = 0;
		m_s0 = m_t0 = 0.0f;
		m_s1 = m_t1 = 1.0f;
		return;
	}

	m_iWide = pTexture->m_iWide;
	m_iTall = pTexture->m_iTall;
	m_s0 = pTexture->m_s0;
	m_t0 = pTexture->m_t0;
	m_s1 = pTexture->m_s1;
	m_t1 = pTexture->m_t1;

	Assert( (pMaterial->GetMappingWidth() == m_iWide) && (pMaterial->GetMappingHeight() == m_iTall) );

	// Increment its reference count
	m_pMaterial->IncrementReferenceCount();

	bool bFound;
	IMaterialVar *tv = m_pMaterial->FindVar( "$baseTexture", &bFound );
	if ( bFound )
	{
		m_pTexture = tv->GetTextureValue();
		if ( m_pTexture )
		{
			m_pTexture->IncrementReferenceCount();
			Assert( m_pTexture == pTexture->m_pTexture );

			// Reference, but do *not* create a new one!!!
			m_pRegen = pTexture->m_pRegen;
		}
	}
}

void CMatSystemTexture::SetMaterial( const char *pFileName )
{
	// Get a pointer to the new material
	Assert( g_pMaterialSystem );
	IMaterial *pMaterial = g_pMaterialSystem->FindMaterial( pFileName, TEXTURE_GROUP_VGUI );

#if defined( DEVELOPMENT_ONLY ) || defined( ALLOW_TEXT_MODE )
	static bool s_bTextMode = CommandLine()->HasParm( "-textmode" );
#else
	const bool s_bTextMode = false;
#endif

	if ( IsErrorMaterial( pMaterial ) && !s_bTextMode )
	{
		if (IsOSX())
		{
			printf( "\n ##### Missing Vgui material %s\n", pFileName );
		}
		Msg( "--- Missing Vgui material %s\n", pFileName );
	}

	SetMaterial( pMaterial );
}

//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
ITextureDictionary *TextureDictionary()
{
	return &s_TextureDictionary;
}

//-----------------------------------------------------------------------------
//   Constructor.
//-----------------------------------------------------------------------------
CTextureDictionary::CTextureDictionary( void )
{
	// First entry is bogus texture
	m_Textures.AddToTail();

#if defined( _X360 )

	// Set the local gamerpic index to be invalid.
	m_LocalGamerpicTextureID = INVALID_TEXTURE_ID;

	// Set the less than function for the remote gamerpic map.
	m_RemoteGamerpicTextureIDMap.SetLessFunc( XuidLessFunc );

#endif // _X360
}


//-----------------------------------------------------------------------------
// Create, destroy textures
//-----------------------------------------------------------------------------
int	CTextureDictionary::CreateTexture( bool procedural /*=false*/ )
{
	int idx = m_Textures.AddToTail();
	CMatSystemTexture &texture = m_Textures[idx];
	texture.SetProcedural( procedural );
	texture.SetId( idx );

	return idx;
}

void CTextureDictionary::DestroyTexture( int id )
{
	Assert( id != m_Textures.InvalidIndex() && m_Textures.IsInList( id ) );
	if ( id != INVALID_TEXTURE_ID )
	{
		if ( m_Textures.IsInList( id ) )
		{
			m_Textures.Remove( (unsigned short)id );
		}
	}
}

void CTextureDictionary::DestroyAllTextures()
{
	m_Textures.RemoveAll();

#if defined( _X360 )

	// Clean the map as well. All ids were destroyed in m_Textures.RemoveAll().
	m_RemoteGamerpicTextureIDMap.RemoveAll();

	// This texture id was destroyed in m_Textures.RemoveAll().
	m_LocalGamerpicTextureID = INVALID_TEXTURE_ID;

#endif // _X360

	// First entry is bogus texture
	m_Textures.AddToTail();	
	CMatSystemTexture &texture = m_Textures[0];
	texture.SetId( 0 );
}


//-----------------------------------------------------------------------------
//	Fill in texture contents
//-----------------------------------------------------------------------------
void CTextureDictionary::SetTextureRGBA( int id, const char* rgba, int wide, int tall )
{
	SetTextureRGBAEx( id, rgba, wide, tall, IMAGE_FORMAT_RGBA8888, k_ETextureScalingPointSample );
}

void CTextureDictionary::SetTextureRGBAEx( int id, const char* rgba, int wide, int tall, ImageFormat format, ETextureScaling eScaling )
{
	if (!IsValidId(id))
	{
		Msg( "SetTextureRGBA: Invalid texture id %i\n", id );
		return;
	}

	CMatSystemTexture &texture = m_Textures[id];
	if ( g_pMaterialSystem )
	{
		// Source1 support
		texture.SetTextureRGBA( rgba, wide, tall, format, eScaling );		
	}
	else
	{
		// Source2 support
		texture.SetTextureRGBA( g_pRenderDevice, rgba, wide, tall, format, eScaling );
	}
}

//-----------------------------------------------------------------------------
//	Fill in texture contents
//-----------------------------------------------------------------------------
void CTextureDictionary::SetSubTextureRGBA( int id, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall )
{
	SetSubTextureRGBAEx( id, drawX, drawY, rgba, subTextureWide, subTextureTall, IMAGE_FORMAT_RGBA8888 );
}


void CTextureDictionary::SetSubTextureRGBAEx( int id, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat format )
{
	if (!IsValidId(id))
	{
		Msg( "SetSubTextureRGBA: Invalid texture id %i\n", id );
		return;
	}

	CMatSystemTexture &texture = m_Textures[id];

	if ( g_pMaterialSystem )
	{
		// Source1 support
		texture.SetSubTextureRGBAEx( drawX, drawY, rgba, subTextureWide, subTextureTall, format );
	}
	else
	{
		// Source2 support
		texture.SetSubTextureRGBA( g_pRenderDevice, drawX, drawY, rgba, subTextureWide, subTextureTall );
	}	
}

void CTextureDictionary::UpdateSubTextureRGBA( int id, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat imageFormat )
{
	if (!IsValidId(id))
	{
		Msg( "UpdateSubTextureRGBA: Invalid texture id %i\n", id );
		return;
	}

	CMatSystemTexture &texture = m_Textures[id];
	texture.UpdateSubTextureRGBA( drawX, drawY, rgba, subTextureWide, subTextureTall, imageFormat  );
}

#if defined( _X360 )

//-----------------------------------------------------------------------------
// Update the local gamerpic texture. Use the given texture if a gamerpic cannot be loaded.
//-----------------------------------------------------------------------------
bool CTextureDictionary::SetLocalGamerpicTexture( DWORD userIndex, const char *pDefaultGamerpicFileName )
{
	Assert( pDefaultGamerpicFileName != NULL );

	if ( !IsValidId( m_LocalGamerpicTextureID ) )
	{
		// Create the local gamerpic texture id.
		m_LocalGamerpicTextureID = CreateTexture( true );
	}
	else if ( !m_Textures[m_LocalGamerpicTextureID].IsProcedural() )
	{
		// The existing texture must be procedural to use a gamerpic. If the existing one
		// isn't, then destroy it. This can happen if the texture is using the default image.
		DestroyTexture( m_LocalGamerpicTextureID );
		m_LocalGamerpicTextureID = CreateTexture( true );
	}

	// Fill the texture with the local gamerpic.
	CMatSystemTexture &texture = m_Textures[m_LocalGamerpicTextureID];
	if ( !texture.SetLocalGamerpicTexture( userIndex ) )
	{
		// Remove the bad texture.
		DestroyTexture( m_LocalGamerpicTextureID );

		// Try to use the default texture instead.
		m_LocalGamerpicTextureID = CreateTexture( false ); // not procedural
		BindTextureToFile( m_LocalGamerpicTextureID, pDefaultGamerpicFileName );
	}

	return IsValidId( m_LocalGamerpicTextureID );
}

//-----------------------------------------------------------------------------
// Get the texture id for a remote gamerpic with the given xuid.
//-----------------------------------------------------------------------------
int CTextureDictionary::GetRemoteGamerpicTextureID( XUID xuid )
{
	int index = m_RemoteGamerpicTextureIDMap.Find( xuid );
	if ( index != m_RemoteGamerpicTextureIDMap.InvalidIndex() )
	{
		RemoteGamerpicData &gamerpicData = m_RemoteGamerpicTextureIDMap.Element( index );
		gamerpicData.m_TimeStamp = g_pVGuiSystem->GetCurrentTime(); // save the time stamp
		return gamerpicData.m_TextureID;
	}

	return INVALID_TEXTURE_ID;
}

//-----------------------------------------------------------------------------
// Create a new remote gamerpic texture entry in the map
//-----------------------------------------------------------------------------
bool CTextureDictionary::CreateRemoteGamerpicTexture( XUID xuid )
{
	// Check for a bad xuid.
	if ( xuid == INVALID_XUID )
	{
		Msg( "[CTextureDictionary] invalid xuid %d!\n", xuid );
		return false;
	}

	// Create the new texture.
	int id = CreateTexture( true );
	if ( !IsValidId( id ) )
	{
		return false;
	}

	// Update the gamerpic texture.
	CMatSystemTexture &texture = m_Textures[id];
	if ( !texture.SetRemoteGamerpicTexture( xuid ) )
	{
		DestroyTexture( id );
		return false;
	}

	// Add a new entry in the map.
	RemoteGamerpicData gamerpicData;
	gamerpicData.m_TextureID = id;
	gamerpicData.m_TimeStamp = g_pVGuiSystem->GetCurrentTime(); // set the time stamp
	m_RemoteGamerpicTextureIDMap.Insert( xuid, gamerpicData );

	return true;
}

//-----------------------------------------------------------------------------
// Create a remote gamerpic texture using the given default image
//-----------------------------------------------------------------------------
bool CTextureDictionary::CreateDefaultRemoteGamerpicTexture( XUID xuid, const char *pDefaultGamerpicFileName )
{
	// Check for a bad xuid.
	if ( xuid == INVALID_XUID )
	{
		Msg( "[CTextureDictionary] invalid xuid %d!\n", xuid );
		return false;
	}

	// Create the new texture.
	int id = CreateTexture( false ); // not procedural
	if ( !IsValidId( id ) )
	{
		return false;
	}

	// Load the texture from file.
	BindTextureToFile( id, pDefaultGamerpicFileName );

	// Add a new entry in the map.
	RemoteGamerpicData gamerpicData;
	gamerpicData.m_TextureID = id;
	gamerpicData.m_TimeStamp = g_pVGuiSystem->GetCurrentTime(); // set the time stamp
	m_RemoteGamerpicTextureIDMap.Insert( xuid, gamerpicData );

	return true;
}

//-----------------------------------------------------------------------------
// Update the remote gamerpic texture for the given xuid. Use the given texture if a gamerpic cannot be loaded.
//-----------------------------------------------------------------------------
bool CTextureDictionary::SetRemoteGamerpicTextureID( XUID xuid, const char *pDefaultGamerpicFileName )
{
	int id = GetRemoteGamerpicTextureID( xuid );
	if ( IsValidId( id ) )
	{
		// Update the existing gamerpic texture for this xuid.
		CMatSystemTexture &texture = m_Textures[id];
		if ( !texture.SetRemoteGamerpicTexture( xuid ) )
		{
			// Remove from the map and the texture array.
			m_RemoteGamerpicTextureIDMap.Remove( xuid );
			DestroyTexture( id );

			// Use the default texture.
			return CreateDefaultRemoteGamerpicTexture( xuid, pDefaultGamerpicFileName );
		}
	}
	else
	{
		if ( m_RemoteGamerpicTextureIDMap.Count() >= MAX_REMOTE_GAMERPIC_TEXTURES )
		{
			//
			// Find the oldest entry in the map.
			//

			int gamerpicCount = m_RemoteGamerpicTextureIDMap.Count();
			COMPILE_TIME_ASSERT( MAX_REMOTE_GAMERPIC_TEXTURES > 1 );

			RemoteGamerpicData &gamerpicData = m_RemoteGamerpicTextureIDMap.Element( 0 );
			int gamerpicIndex = 0;
			for ( int i = 1; i < gamerpicCount; ++i )
			{
				RemoteGamerpicData &nextGamerpicData = m_RemoteGamerpicTextureIDMap.Element( i );
				if ( nextGamerpicData.m_TimeStamp < gamerpicData.m_TimeStamp )
				{
					gamerpicData = nextGamerpicData;
					gamerpicIndex = i;
				}
			}

			// Remove the oldest entry.
			id = gamerpicData.m_TextureID;
			m_RemoteGamerpicTextureIDMap.RemoveAt( gamerpicIndex );
			DestroyTexture( id );
		}

		// Create a new entry.
		if ( !CreateRemoteGamerpicTexture( xuid ) )
		{
			return CreateDefaultRemoteGamerpicTexture( xuid, pDefaultGamerpicFileName );
		}
	}

	return true;
}

#endif // _X360

//-----------------------------------------------------------------------------
// Returns true if the id is valid
//-----------------------------------------------------------------------------
bool CTextureDictionary::IsValidId( int id ) const
{
	Assert( id != 0 );
	if ( id == 0 )
		return false;

	if( m_Textures.IsValidIndex( id ) )
	{
		if ( !m_Textures.IsInList( id ) )
		{
			return false;
		}
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Binds a file to a particular texture
//-----------------------------------------------------------------------------
void CTextureDictionary::BindTextureToFile( int id, const char *pFileName )
{
	if (!IsValidId(id))
	{
		Msg( "BindTextureToFile: Invalid texture id for file %s\n", pFileName );
		return;
	}

	CMatSystemTexture &texture = m_Textures[id];

	// Reload from file if the material was never loaded, or if the filename has changed at all
	CRC32_t fileNameCRC = Texture_CRCName( pFileName );
	if ( !texture.GetMaterial() || fileNameCRC != texture.GetCRC() )
	{
		// New texture name
		texture.SetCRC( fileNameCRC );
		texture.SetMaterial( pFileName );
	}
}


//-----------------------------------------------------------------------------
// Binds a material to a texture
//-----------------------------------------------------------------------------
void CTextureDictionary::BindTextureToMaterial( int id, IMaterial *pMaterial )
{
	if (!IsValidId(id))
	{
		Msg( "BindTextureToFile: Invalid texture id %d\n", id );
		return;
	}

	CMatSystemTexture &texture = m_Textures[id];
	texture.SetMaterial( pMaterial );
}


//-----------------------------------------------------------------------------
// Binds a material to a texture reference
//-----------------------------------------------------------------------------
void CTextureDictionary::BindTextureToMaterialReference( int id, int referenceId, IMaterial *pMaterial )
{
	if (!IsValidId(id) || !IsValidId(referenceId))
	{
		Msg( "BindTextureToFile: Invalid texture ids %d %d\n", id, referenceId );
		return;
	}

	CMatSystemTexture &texture = m_Textures[id];
	CMatSystemTexture &textureSource = m_Textures[referenceId];
	texture.ReferenceOtherProcedural( &textureSource, pMaterial );
}


//-----------------------------------------------------------------------------
// Returns the material associated with an id
//-----------------------------------------------------------------------------
IMaterial *CTextureDictionary::GetTextureMaterial( int id )
{
	if (!IsValidId(id))
		return NULL;

	return m_Textures[id].GetMaterial();
}

//-----------------------------------------------------------------------------
// Returns the texture handle associated with an id
//-----------------------------------------------------------------------------
HRenderTexture CTextureDictionary::GetTextureHandle( int id )
{
	if (!IsValidId(id))
		return NULL;

	return m_Textures[id].GetTextureHandle();
}

//-----------------------------------------------------------------------------
// Returns the material size associated with an id
//-----------------------------------------------------------------------------
void CTextureDictionary::GetTextureSize(int id, int& iWide, int& iTall )
{
	if (!IsValidId(id))
	{
		iWide = iTall = 0;
		return;
	}

	iWide = m_Textures[id].Width();
	iTall = m_Textures[id].Height();
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void CTextureDictionary::GetTextureTexCoords( int id, float &s0, float &t0, float &s1, float &t1 )
{
	if (!IsValidId(id))
	{
		s0 = t0 = 0.0f;
		s1 = t1 = 1.0f;
		return;
	}

	s0 = m_Textures[id].m_s0;
	t0 = m_Textures[id].m_t0;
	s1 = m_Textures[id].m_s1;
	t1 = m_Textures[id].m_t1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : id - 
// Output : CMatSystemTexture
//-----------------------------------------------------------------------------
CMatSystemTexture *CTextureDictionary::GetTexture( int id )
{
	if (!IsValidId(id))
		return NULL;

	return &m_Textures[ id ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFileName - 
//-----------------------------------------------------------------------------
int	CTextureDictionary::FindTextureIdForTextureFile( char const *pFileName )
{
	for ( int i = m_Textures.Head(); i != m_Textures.InvalidIndex(); i = m_Textures.Next( i ) )
	{
		CMatSystemTexture *tex = &m_Textures[i];
		if ( !tex )
			continue;

		IMaterial *mat = tex->GetMaterial();
		if ( !mat )
			continue;

		if ( !stricmp( mat->GetName(), pFileName ) )
			return i;
	}

	return -1;
}
