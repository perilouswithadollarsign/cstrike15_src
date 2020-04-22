//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#include "materialobjects/dmetexture.h"
#include "datamodel/dmelementfactoryhelper.h"

#include "materialsystem/IMaterial.h"
#include "materialsystem/IMaterialSystem.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeTextureFrame, CDmeTextureFrame );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeTextureFrame::OnConstruction()
{
	m_MipLevels.Init( this, "mipLevels" );
}

void CDmeTextureFrame::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// List manipulation
//-----------------------------------------------------------------------------
int CDmeTextureFrame::MipLevelCount() const
{
	return m_MipLevels.Count();
}

CDmeImageArray *CDmeTextureFrame::GetMipLevel( int nIndex ) const
{
	return m_MipLevels[ nIndex ];
}

void CDmeTextureFrame::AddMipLevel( CDmeImageArray *pImages )
{
	m_MipLevels.AddToTail( pImages );
}

CDmeImageArray *CDmeTextureFrame::AddMipLevel( )
{
	CDmeImageArray *pImages = CreateElement< CDmeImageArray >( "mip", GetFileId() );
	AddMipLevel( pImages );
	return pImages;
}


int CDmeTextureFrame::ImageCount() const
{
	int nImageCount = 0;
	int nMipCount = MipLevelCount();
	for ( int m = 0; m < nMipCount; ++m )
	{
		CDmeImageArray *pMipLevel = GetMipLevel( m );
		nImageCount = MAX( nImageCount, pMipLevel->ImageCount() );
	}
	return nImageCount;
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeTexture, CDmeTexture );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeTexture::OnConstruction()
{
//	m_pVTFTexture = CreateVTFTexture();

	m_bClampS.Init( this, "clampS" );
	m_bClampT.Init( this, "clampT" );
	m_bClampU.Init( this, "clampU" );
	m_bNoDebugOverride.Init( this, "noDebugOverride" );
	m_bNoLod.Init( this, "noMipmapLOD" );
	m_bNiceFiltered.Init( this, "niceFiltered" );
	m_bNormalMap.Init( this, "normalMap" );
	m_flBumpScale.Init( this, "bumpScale" );
	m_nCompressType.Init( this, "compressType" );
	m_nFilterType.Init( this, "filterType" );
	m_nMipmapType.Init( this, "mipmapType" );
	m_nTextureType.Init( this, "textureType" );
	m_Frames.Init( this, "frames" );
}

void CDmeTexture::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Gets dimensions
//-----------------------------------------------------------------------------
int CDmeTexture::Width() const
{
	int nFrameCount = m_Frames.Count();
	int nWidth = nFrameCount ? m_Frames[0]->GetMipLevel( 0 )->Width() : 0;

#ifdef _DEBUG
	for ( int f = 1; f < nFrameCount; ++f )
	{
		CDmeTextureFrame *pFrame = m_Frames[f];
		Assert( pFrame->GetMipLevel( 0 )->Width() == nWidth );
	}
#endif
	return nWidth;
}

int CDmeTexture::Height() const
{
	int nFrameCount = m_Frames.Count();
	int nHeight = nFrameCount ? m_Frames[0]->GetMipLevel( 0 )->Height() : 0;

#ifdef _DEBUG
	for ( int f = 1; f < nFrameCount; ++f )
	{
		CDmeTextureFrame *pFrame = m_Frames[f];
		Assert( pFrame->GetMipLevel( 0 )->Height() == nHeight );
	}
#endif
	return nHeight;
}

int CDmeTexture::Depth() const
{
	int nFrameCount = m_Frames.Count();
	int nDepth = nFrameCount ? m_Frames[0]->GetMipLevel( 0 )->Depth() : 0;

#ifdef _DEBUG
	for ( int f = 1; f < nFrameCount; ++f )
	{
		CDmeTextureFrame *pFrame = m_Frames[f];
		Assert( pFrame->GetMipLevel( 0 )->Depth() == nDepth );
	}
#endif
	return nDepth;
}

ImageFormat CDmeTexture::Format() const
{
	int nFrameCount = m_Frames.Count();
	ImageFormat nFormat = nFrameCount ? m_Frames[0]->GetMipLevel( 0 )->Format() : IMAGE_FORMAT_UNKNOWN;

#ifdef _DEBUG
	for ( int f = 0; f < nFrameCount; ++f )
	{
		CDmeTextureFrame *pFrame = m_Frames[f];
		int nMipCount = pFrame->MipLevelCount();
		for ( int m = 0; m < nMipCount; ++m )
		{
			CDmeImageArray *pMipLevel = pFrame->GetMipLevel( m );
			Assert( pMipLevel->Format() == nFormat );
		}
	}
#endif
	return nFormat;
}


int CDmeTexture::MipLevelCount() const
{
	int nFrameCount = m_Frames.Count();
	int nMipCount = nFrameCount ? m_Frames[0]->MipLevelCount( ) : 0;

#ifdef _DEBUG
	for ( int f = 0; f < nFrameCount; ++f )
	{
		CDmeTextureFrame *pFrame = m_Frames[f];
		Assert( nMipCount == pFrame->MipLevelCount() );
	}
#endif
	return nMipCount;
}

int CDmeTexture::ImageCount() const
{
	int nImageCount = 0;
	int nFrameCount = m_Frames.Count();
	for ( int f = 0; f < nFrameCount; ++f )
	{
		CDmeTextureFrame *pFrame = m_Frames[f];
		nImageCount = MAX( nImageCount, pFrame->ImageCount() );
	}
	return nImageCount;
}


//-----------------------------------------------------------------------------
// Computes texture flags
//-----------------------------------------------------------------------------
int CDmeTexture::CalcTextureFlags( int nDepth ) const
{
	int nFlags = 0;
	if ( m_bClampS )
	{
		nFlags |= TEXTUREFLAGS_CLAMPS;
	}
	if ( m_bClampT )
	{
		nFlags |= TEXTUREFLAGS_CLAMPT;
	}
	if ( m_bClampU )
	{
		nFlags |= TEXTUREFLAGS_CLAMPU;
	}
	if ( m_bNoLod )
	{
		nFlags |= TEXTUREFLAGS_NOLOD;
	}
	if ( m_bNormalMap )
	{
		nFlags |= TEXTUREFLAGS_NORMAL;
	}
	if ( m_bNormalMap )
	{
		nFlags |= TEXTUREFLAGS_NORMAL;
	}
	if ( m_bNoDebugOverride )
	{
		nFlags |= TEXTUREFLAGS_NODEBUGOVERRIDE;
	}

	switch ( m_nCompressType )
	{
	case DMETEXTURE_COMPRESS_DEFAULT:
	case DMETEXTURE_COMPRESS_DXT1:
		break;
	case DMETEXTURE_COMPRESS_DXT5:
		nFlags |= TEXTUREFLAGS_HINT_DXT5;
		break;
	}

	switch ( m_nFilterType )
	{
	case DMETEXTURE_FILTER_DEFAULT:
	case DMETEXTURE_FILTER_BILINEAR:
		break;
	case DMETEXTURE_FILTER_ANISOTROPIC:
		nFlags |= TEXTUREFLAGS_ANISOTROPIC;
		break;
	case DMETEXTURE_FILTER_TRILINEAR:
		nFlags |= TEXTUREFLAGS_TRILINEAR;
		break;
	case DMETEXTURE_FILTER_POINT:
		nFlags |= TEXTUREFLAGS_POINTSAMPLE;
		break;
	}

	switch ( m_nMipmapType )
	{
	case DMETEXTURE_MIPMAP_DEFAULT:
	case DMETEXTURE_MIPMAP_ALL_LEVELS:
		break;
	case DMETEXTURE_MIPMAP_NONE:
		nFlags |= TEXTUREFLAGS_NOMIP;
		break;
	}

	if ( nDepth > 1 )
	{
		// FIXME: Volume textures don't currently support DXT compression
		nFlags &= ~TEXTUREFLAGS_HINT_DXT5;
	}

	return nFlags;
}


//-----------------------------------------------------------------------------
// Computes the desired texture format based on flags
//-----------------------------------------------------------------------------
ImageFormat CDmeTexture::ComputeDesiredImageFormat( ImageFormat srcFormat, int nWidth, int nHeight, int nDepth, int nFlags )
{
	// HDRFIXME: Need to figure out what format to use here.
	if ( srcFormat == IMAGE_FORMAT_RGB323232F )
		return IMAGE_FORMAT_RGBA16161616F;

	/*
	if( bDUDVTarget)
	{
		if ( bCopyAlphaToLuminance && ( nFlags & ( TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA ) ) )
			return IMAGE_FORMAT_UVLX8888;
		return IMAGE_FORMAT_UV88;
	}
	*/

	// can't compress textures that are smaller than 4x4
	if ( (nFlags & TEXTUREFLAGS_PROCEDURAL) ||
		( nWidth < 4 ) || ( nHeight < 4 ) || ( nDepth > 1 ) )
	{
		if ( nFlags & ( TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA ) )
			return IMAGE_FORMAT_BGRA8888;
		return IMAGE_FORMAT_BGR888;
	}
	
	if( nFlags & TEXTUREFLAGS_HINT_DXT5 )
		return IMAGE_FORMAT_DXT5;

	// compressed with alpha blending
	if ( nFlags & TEXTUREFLAGS_EIGHTBITALPHA )
		return IMAGE_FORMAT_DXT5;
	
	if ( nFlags & TEXTUREFLAGS_ONEBITALPHA )
		return IMAGE_FORMAT_DXT5; // IMAGE_FORMAT_DXT1_ONEBITALPHA

	return IMAGE_FORMAT_DXT1;
}


//-----------------------------------------------------------------------------
// Adds a frame
//-----------------------------------------------------------------------------
CDmeTextureFrame *CDmeTexture::AddFrame()
{
	CDmeTextureFrame *pImageArray = CreateElement< CDmeTextureFrame >( "frame", GetFileId() );
	m_Frames.AddToTail( pImageArray );
	return pImageArray;
}


void CDmeTexture::RemoveAllFrames()
{
	m_Frames.RemoveAll();
}


//-----------------------------------------------------------------------------
// Returns all images associated with a particular frame + face (mip count amount)
//-----------------------------------------------------------------------------
void CDmeTexture::GetImages( int nFrame, int nImageIndex, CDmeImage **ppImages, int nSize )
{
	memset( ppImages, 0, nSize * sizeof(CDmeImage*) );

	int nFrameCount = FrameCount();
	if ( nFrame >= nFrameCount )
		return;

	CDmeTextureFrame *pFrame = GetFrame( nFrame );
	if ( !pFrame )
		return;

	int nMipCount = MIN( pFrame->MipLevelCount(), nSize );
	for ( int m = 0; m < nMipCount; ++m )
	{
		CDmeImageArray *pImageArray = pFrame->GetMipLevel( m );
		if ( !pImageArray )
			continue;

		int nImageCount = pImageArray->ImageCount();
		if ( nImageIndex >= nImageCount )
			continue;

		ppImages[m] = pImageArray->GetImage( nImageIndex );
	}
}

void CDmeTexture::CompressTexture( CDmeTexture *pSrcTexture, ImageFormat fmt )
{
	class CCompressionFunctor
	{
	public:
		CCompressionFunctor( ImageFormat fmt ) { m_Format = fmt; }
		void operator()( CDmeImage *pDstImage, CDmeImage *pSrcImage )
		{
			pDstImage->CompressImage( pSrcImage, m_Format );
		}

		ImageFormat m_Format;
	};

	CCompressionFunctor compress( fmt );
	ProcessTexture( pSrcTexture, compress );
}


//-----------------------------------------------------------------------------
// resolve
//-----------------------------------------------------------------------------
void CDmeTexture::Resolve()
{
}
