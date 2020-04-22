//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#include "materialobjects/dmeimage.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "bitmap/imageformat.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeImage, CDmeImage );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeImage::OnConstruction()
{
	m_nWidth.Init( this, "width", FATTRIB_HIDDEN );
	m_nHeight.Init( this, "height", FATTRIB_HIDDEN );
	m_nDepth.Init( this, "depth", FATTRIB_HIDDEN );
	m_nFormat.Init( this, "format", FATTRIB_HIDDEN );
	m_flGamma.Init( this, "gamma", FATTRIB_HIDDEN );
	m_Bits.Init( this, "bits", FATTRIB_HIDDEN | FATTRIB_HAS_CALLBACK );
	m_Mode = DMEIMAGE_STORAGE_NONE;
	m_bInModification = false;
	m_bInFloatBitmapModification = false;
	m_bIgnoreChangedBitsAttribute = false;
	m_hModify = NULL;
}

void CDmeImage::OnDestruction()
{
	Assert( !m_bInModification && !m_bInFloatBitmapModification );
}


//-----------------------------------------------------------------------------
// Used to make sure the attribute is well-behaved
//-----------------------------------------------------------------------------
void CDmeImage::OnAttributeChanged( CDmAttribute *pAttribute )
{
	BaseClass::OnAttributeChanged( pAttribute );
	if ( pAttribute == m_Bits.GetAttribute() )
	{
		if ( !m_bIgnoreChangedBitsAttribute )
		{
			// If the attribute changed (undo), the attribute contains the bits;
			// discard the float bitmap state
			if ( m_Mode == DMEIMAGE_STORAGE_FLOAT_BITMAP )
			{
				SetFloatBitmapStorageMode( false, true );
			}
		}
	}
}

void CDmeImage::OnElementUnserialized()
{
	BaseClass::OnElementUnserialized();

	// After reading, the attribute contains the bits; discard anything else
	if ( m_Mode != DMEIMAGE_STORAGE_ATTRIBUTE )
	{
		SetFloatBitmapStorageMode( false, true );
	}
}

void CDmeImage::OnElementSerialized()
{
	BaseClass::OnElementSerialized();

	// Prior to serialization, make sure the bits attribute is holding the current bits
	if ( m_Mode == DMEIMAGE_STORAGE_FLOAT_BITMAP )
	{
		SetFloatBitmapStorageMode( false );
	}
}


//-----------------------------------------------------------------------------
// Initializes the buffer, doesn't allocate space
//-----------------------------------------------------------------------------
void CDmeImage::Init( int nWidth, int nHeight, int nDepth, ImageFormat fmt, float flGamma )
{
	if ( IsUsingFloatBitmapStorageMode() )
	{
		m_ComputeBits.Shutdown();
	}
	m_Bits.Set( NULL, 0 );

	m_nWidth = nWidth;
	m_nHeight = nHeight;
	m_nDepth = nDepth;
	m_nFormat = fmt;
	m_flGamma = flGamma;
	m_Mode = DMEIMAGE_STORAGE_NONE;
}


//-----------------------------------------------------------------------------
// Image format
//-----------------------------------------------------------------------------
ImageFormat CDmeImage::Format() const
{
	return (ImageFormat)( m_nFormat.Get() );
}

const char *CDmeImage::FormatName() const
{
	return ImageLoader::GetName( Format() );
}


//-----------------------------------------------------------------------------
// returns the size of one row
//-----------------------------------------------------------------------------
int CDmeImage::RowSizeInBytes( ) const
{
	return ImageLoader::GetMemRequired( m_nWidth, 1, 1, Format(), false );
}


//-----------------------------------------------------------------------------
// returns the size of one z slice
//-----------------------------------------------------------------------------
int CDmeImage::ZSliceSizeInBytes( ) const
{
	return ImageLoader::GetMemRequired( m_nWidth, m_nHeight, 1, Format(), false );
}


//-----------------------------------------------------------------------------
// returns the total size of the image
//-----------------------------------------------------------------------------
int CDmeImage::SizeInBytes( ) const
{
	return ImageLoader::GetMemRequired( m_nWidth, m_nHeight, m_nDepth, Format(), false );
}


//-----------------------------------------------------------------------------
// Converts the image into its requested storage mode
//-----------------------------------------------------------------------------
void CDmeImage::SetFloatBitmapStorageMode( bool bFloatBitmap, bool bDiscardContents )
{
	Assert( !m_bInModification && !m_bInFloatBitmapModification );

	StorageMode_t nMode = ( bFloatBitmap ) ? DMEIMAGE_STORAGE_FLOAT_BITMAP : DMEIMAGE_STORAGE_ATTRIBUTE;
	if ( nMode == m_Mode )
		return;

	// Do this to avoid re-entrancy problems in BeginModification
	StorageMode_t nOldMode = m_Mode;
	m_Mode = nMode;

	switch( m_Mode )
	{
	case DMEIMAGE_STORAGE_NONE:
		break;

	case DMEIMAGE_STORAGE_ATTRIBUTE:
		if ( !bDiscardContents && ( nOldMode == DMEIMAGE_STORAGE_FLOAT_BITMAP ) )
		{
			m_nWidth = m_ComputeBits.NumCols();
			m_nHeight = m_ComputeBits.NumRows();
			m_nDepth = m_ComputeBits.NumSlices();

			CUtlBinaryBlock &buf = BeginModification();
			buf.SetLength( SizeInBytes() );
			m_ComputeBits.WriteToBuffer( buf.Get(), buf.Length(), Format(), Gamma() );
			EndModification();
		}
		else
		{
			// Question: If we're in float bitmap mode, but we discard contents,
			// should we copy back the dimensions?
			CUtlBinaryBlock &buf = BeginModification();
			buf.SetLength( SizeInBytes() );
			EndModification();
		}
		m_ComputeBits.Shutdown();
		break;

	case DMEIMAGE_STORAGE_FLOAT_BITMAP:
		const ImageFormatInfo_t &info = ImageLoader::ImageFormatInfo( Format() );
		int nMask = 0;
		if ( info.m_nNumRedBits > 0 )	nMask |= FBM_ATTR_RED_MASK;
		if ( info.m_nNumGreenBits > 0 )	nMask |= FBM_ATTR_GREEN_MASK;
		if ( info.m_nNumBlueBits > 0 )	nMask |= FBM_ATTR_BLUE_MASK;
		if ( info.m_nNumAlphaBits > 0 )	nMask |= FBM_ATTR_ALPHA_MASK;

		m_ComputeBits.Init( m_nWidth, m_nHeight, m_nDepth, nMask );
		if ( !bDiscardContents && ( nOldMode == DMEIMAGE_STORAGE_ATTRIBUTE ) )
		{
			m_ComputeBits.LoadFromBuffer( m_Bits.Get(), m_Bits.Length(), Format(), Gamma() );
		}
		break;
	}
}


//-----------------------------------------------------------------------------
// Copies the bits in whatever form they are currently in
//-----------------------------------------------------------------------------
void CDmeImage::CopyFrom( CDmeImage *pSrcImage, ImageFormat fmt )
{
	if ( fmt == IMAGE_FORMAT_UNKNOWN )
	{
		fmt = pSrcImage->Format();
	}

	Init( pSrcImage->Width(), pSrcImage->Height(), pSrcImage->Depth(), fmt, pSrcImage->Gamma() );

	if ( !pSrcImage->HasImageData() )
		return;

	if ( pSrcImage->IsUsingFloatBitmapStorageMode() )
	{
		SetFloatBitmapStorageMode( true, true );
		m_ComputeBits.LoadFromFloatBitmap( pSrcImage->FloatBitmap() );
	}
	else
	{
		if ( pSrcImage->Format() == Format() )
		{
			SetImageBits( pSrcImage->ImageBits(), pSrcImage->SizeInBytes() );
		}
		else
		{
			int nMemory = ImageLoader::GetMemRequired( Width(), Height(), Depth(), 1, Format() );
			CUtlBinaryBlock &bits = BeginModification();
			bits.SetLength( nMemory );
			ImageLoader::ConvertImageFormat( (const uint8*)pSrcImage->ImageBits(), pSrcImage->Format(), 
				(uint8*)bits.Get(), Format(), Width(), Height() * Depth() );
			EndModification();
		}
	}
}


//-----------------------------------------------------------------------------
// Color converts the image into the destination format.
//-----------------------------------------------------------------------------
void CDmeImage::ConvertFormat( ImageFormat fmt )
{
	if ( fmt == m_nFormat )
		return;

	if ( ImageLoader::IsCompressed( Format() ) )
	{
		// Cannot convert from a compressed format
		Assert( 0 );
		return;
	}

	if ( ImageLoader::IsCompressed( fmt ) )
	{
		// Not supported for compressed textures
		Assert( 0 );
		return;
	}

	// NOTE: Not super fast, needs to copy the data here even if we use
	// the faster ImageLoader::ConvertImageFormat calls. And those
	// don't support volume textures. So, screw it. Doing an easier implementation.
	// This will convert it to 32323232F, and on return, it will convert
	// to the desired format.
	if ( m_Mode == DMEIMAGE_STORAGE_ATTRIBUTE )
	{
		SetFloatBitmapStorageMode( true );
	}

	m_nFormat = fmt;
}


//-----------------------------------------------------------------------------
// Sets the color of every pixel
//-----------------------------------------------------------------------------
void CDmeImage::Clear( float r, float g, float b, float a )
{
	SetFloatBitmapStorageMode( true, true );
	m_ComputeBits.Clear( r, g, b, a );
}


//-----------------------------------------------------------------------------
// Compresses an image into this image
//-----------------------------------------------------------------------------
void CDmeImage::CompressImage( CDmeImage *pSrcImage, ImageFormat fmt )
{
	if ( ImageLoader::IsCompressed( pSrcImage->Format() ) )
	{
		// Cannot convert from a compressed format
		Assert( 0 );
		return;
	}

	// Must be a compressed format
	if ( !ImageLoader::IsCompressed( fmt ) )
	{
		Assert( 0 );
		return;
	}

	Init( pSrcImage->Width(), pSrcImage->Height(), pSrcImage->Depth(), fmt, pSrcImage->Gamma() );

	// We can only convert from a few well-known formats
	pSrcImage->ConvertFormat( IMAGE_FORMAT_RGBA8888 );

	int nSrcFaceStride = pSrcImage->ZSliceSizeInBytes();
	int nDstFaceStride = ZSliceSizeInBytes();

	CUtlBinaryBlock &dstBlock = BeginModification();
	dstBlock.SetLength( SizeInBytes() );

	const uint8* pSrcData = reinterpret_cast< const uint8* >( pSrcImage->ImageBits() );
	uint8* pDstData = reinterpret_cast< uint8* >( dstBlock.Get() );
	for ( int z = 0; z < Depth(); ++z, pSrcData += nSrcFaceStride, pDstData += nDstFaceStride )
	{
		ImageLoader::ConvertImageFormat( pSrcData, pSrcImage->Format(), 
			pDstData, fmt, pSrcImage->Width(), pSrcImage->Height() );
	}
	EndModification();
}



//-----------------------------------------------------------------------------
// Reinterprets the image as a new color format; no work is done
//-----------------------------------------------------------------------------
void CDmeImage::ReinterpetFormat( ImageFormat fmt )
{
	if ( ImageLoader::SizeInBytes( fmt ) != ImageLoader::SizeInBytes( Format() ) )
	{
		// Src + dst format must be the same size for this to work!
		Assert( 0 );
		return;
	}

	m_nFormat = fmt;
}


//-----------------------------------------------------------------------------
// Copies bits into the image bits buffer
//-----------------------------------------------------------------------------
void CDmeImage::SetImageBits( const void *pBits, int nSize )
{
	SetFloatBitmapStorageMode( false, true );
	m_Bits.Set( pBits, nSize );
}


//-----------------------------------------------------------------------------
// Used for bit modification
//-----------------------------------------------------------------------------
CUtlBinaryBlock &CDmeImage::BeginModification( )
{
	SetFloatBitmapStorageMode( false );
	Assert( !m_bInModification && !m_bInFloatBitmapModification );
	m_bInModification = true;
	return m_Bits.GetAttribute()->BeginModifyValueInPlace< CUtlBinaryBlock >( &m_hModify );
}

void CDmeImage::EndModification( )
{
	Assert( m_bInModification && !m_bInFloatBitmapModification );
	m_bInModification = false;
	m_bIgnoreChangedBitsAttribute = true;
	m_Bits.GetAttribute()->EndModifyValueInPlace< CUtlBinaryBlock >( m_hModify );
	m_bIgnoreChangedBitsAttribute = false;
	m_hModify = NULL;
}


//-----------------------------------------------------------------------------
// Used for float bitmap modification
//-----------------------------------------------------------------------------
FloatBitMap_t &CDmeImage::BeginFloatBitmapModification( )
{
	SetFloatBitmapStorageMode( true );
	Assert( !m_bInModification && !m_bInFloatBitmapModification );
	m_bInFloatBitmapModification = true;
	return m_ComputeBits;
}

void CDmeImage::EndFloatBitmapModification( )
{
	Assert( m_bInFloatBitmapModification && !m_bInModification );
	m_bInFloatBitmapModification = false;
}


//-----------------------------------------------------------------------------
// Image data
//-----------------------------------------------------------------------------
const FloatBitMap_t *CDmeImage::FloatBitmap()
{
	SetFloatBitmapStorageMode( true );
	return &m_ComputeBits;
}


//-----------------------------------------------------------------------------
// Creates an image 1/4 size of the source using a box filter
//-----------------------------------------------------------------------------
void CDmeImage::QuarterSize( CDmeImage *pSrcImage )
{
	SetFloatBitmapStorageMode( true, true );
	pSrcImage->SetFloatBitmapStorageMode( true );
	pSrcImage->m_ComputeBits.QuarterSize( &m_ComputeBits );
	m_nFormat = pSrcImage->Format();
	m_flGamma = pSrcImage->Gamma();
}


//-----------------------------------------------------------------------------
// Downsample using nice filter (NOTE: Dest bitmap needs to have been initialized w/ final size)
//-----------------------------------------------------------------------------
void CDmeImage::DownsampleNiceFiltered( const DownsampleInfo_t& info, CDmeImage *pSrcImage )
{
	SetFloatBitmapStorageMode( true, true );
	pSrcImage->SetFloatBitmapStorageMode( true );
	pSrcImage->m_ComputeBits.DownsampleNiceFiltered( info, &m_ComputeBits );
	m_nFormat = pSrcImage->Format();
	m_flGamma = pSrcImage->Gamma();
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeImageArray, CDmeImageArray );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeImageArray::OnConstruction()
{
	m_Images.Init( this, "images" );
}

void CDmeImageArray::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Image format
//-----------------------------------------------------------------------------
int CDmeImageArray::ImageCount() const
{
	return m_Images.Count();
}

CDmeImage *CDmeImageArray::GetImage( int nIndex ) const
{
	return m_Images[nIndex];
}

bool CDmeImageArray::IsConsistent( int nWidth, int nHeight, int nDepth, ImageFormat fmt ) const
{
	if ( ImageCount() == 0 )
		return true;

	CDmeImage *pFirstImage = m_Images[0];
	return ( nWidth == pFirstImage->Width() && nHeight == pFirstImage->Height() &&
		fmt == pFirstImage->Format() && nDepth == pFirstImage->Depth() );
}

void CDmeImageArray::AddImage( CDmeImage *pImage )
{
	if ( !IsConsistent( pImage->Width(), pImage->Height(), pImage->Depth(), pImage->Format() ) )
	{
		Warning( "Attempted to add different size/format images to the image array!\n" );
		return;
	}

	m_Images.AddToTail( pImage );
}

CDmeImage *CDmeImageArray::AddImage( )
{
	CDmeImage *pImage = CreateElement< CDmeImage >( "image", GetFileId() );
	if ( ImageCount() > 0 )
	{
		CDmeImage *pFirstImage = m_Images[0];
		pImage->Init( pFirstImage->Width(), pFirstImage->Height(),
			pFirstImage->Depth(), pFirstImage->Format(), pFirstImage->Gamma() );
	}

	AddImage( pImage );
	return pImage;
}


// Gets dimensions
int CDmeImageArray::Width() const
{
	return ( ImageCount() > 0 ) ? m_Images[0]->Width() : -1;
}

int CDmeImageArray::Height() const
{
	return ( ImageCount() > 0 ) ? m_Images[0]->Height() : -1;
}

int CDmeImageArray::Depth() const
{
	return ( ImageCount() > 0 ) ? m_Images[0]->Depth() : -1;
}

ImageFormat CDmeImageArray::Format() const
{
	return ( ImageCount() > 0 ) ? m_Images[0]->Format() : IMAGE_FORMAT_UNKNOWN;
}
