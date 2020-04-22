//======= Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//=============================================================================//

#include "bitmap/bitmap.h"
#include "dbg.h"
#include "utlbuffer.h"
#include "bitmap/psd.h"
#include "bitmap/tgaloader.h"

// Should be last include
#include "tier0/memdbgon.h"

bool Bitmap_t::IsValid() const
{
	if ( m_nWidth <= 0 || m_nHeight <= 0 || m_pBits == NULL )
	{
		Assert( m_nWidth == 0 );
		Assert( m_nHeight == 0 );
		Assert( m_pBits == NULL );
		return false;
	}
	return true;
}

void Bitmap_t::Clear()
{
	if ( m_pBits && m_bOwnsBuffer )
	{
		free( m_pBits );
	}
	Reset();
}

void Bitmap_t::Init( int xs, int ys, ImageFormat imageFormat, int nStride )
{

	// Check for bogus allocation sizes
	if (xs <= 0 || ys <= 0 )
	{
		Assert( xs == 0 );
		Assert( ys == 0 );
		Clear();
		return;
	}

	int nPixSize = ImageLoader::SizeInBytes( imageFormat );

	// Auto detect stride
	if ( nStride == 0 )
	{
		nStride = nPixSize * xs;
	}

	// Check for NOP
	if (
		m_pBits
		&& m_bOwnsBuffer
		&& m_nWidth == xs
		&& m_nHeight == ys
		&& nStride == m_nStride
		&& nPixSize == m_nPixelSize )
	{
		// We're already got a buffer of the right size.
		// The only thing that might be wrong is the pixel format.
		m_ImageFormat = imageFormat;
		return;
	}

	// Free up anything already allocated
	Clear();

	// Remember dimensions and pixel format
	m_nWidth = xs;
	m_nHeight = ys;
	m_ImageFormat = imageFormat;
	m_nPixelSize = nPixSize;
	m_nStride = nStride;

	// Allocate buffer.  Because this is a PC game,
	// failure is impossible....right?
	m_pBits = (byte *)malloc( ys * m_nStride );

	// Assume ownership
	m_bOwnsBuffer = true;
}

void Bitmap_t::SetBuffer( int nWidth, int nHeight, ImageFormat imageFormat, unsigned char *pBits, bool bAssumeOwnership, int nStride )
{
	Assert( pBits );
	Assert( nWidth > 0 );
	Assert( nHeight > 0 );

	// Free up anything already allocated
	Clear();

	// Remember dimensions and pixel format
	m_nWidth = nWidth;
	m_nHeight = nHeight;
	m_ImageFormat = imageFormat;
	m_nPixelSize = ImageLoader::SizeInBytes( imageFormat );
	if ( nStride == 0 )
	{
		m_nStride = m_nPixelSize * nWidth;
	}
	else
	{
		m_nStride = nStride;
	}

	// Set our buffer pointer
	m_pBits = pBits;

	// Assume ownership of the buffer, if requested
	m_bOwnsBuffer = bAssumeOwnership;

	// We should be good to go
	Assert( IsValid() );
}

Color Bitmap_t::GetColor( int x, int y ) const
{
	Assert( x >= 0 && x < m_nWidth );
	Assert( y >= 0 && y < m_nHeight );
	Assert( m_pBits );

	// Get pointer to pixel data
	byte *ptr = m_pBits + (y*m_nStride) + x* m_nPixelSize;

	// Check supported image formats
	switch ( m_ImageFormat )
	{
		case IMAGE_FORMAT_RGBA8888:
			return Color( ptr[0], ptr[1], ptr[2], ptr[3] );

		case IMAGE_FORMAT_ABGR8888:
			return Color( ptr[3], ptr[2], ptr[1], ptr[0] );

		default:
			Assert( !"Unsupport image format!");
			return Color( 255,0,255,255 );
	}
}

void Bitmap_t::SetColor( int x, int y, Color c )
{
	Assert( x >= 0 && x < m_nWidth );
	Assert( y >= 0 && y < m_nHeight );
	Assert( m_pBits );

	// Get pointer to pixel data
	byte *ptr = m_pBits + (y*m_nStride) + x* m_nPixelSize;

	// Check supported image formats
	switch ( m_ImageFormat )
	{
		case IMAGE_FORMAT_RGBA8888:
			ptr[0] = c.r();
			ptr[1] = c.g();
			ptr[2] = c.b();
			ptr[3] = c.a();
			break;

		case IMAGE_FORMAT_ABGR8888:
			ptr[0] = c.a();
			ptr[1] = c.b();
			ptr[2] = c.g();
			ptr[3] = c.r();
			break;

		default:
			Assert( !"Unsupport image format!");
			break;
	}
}

//bool LoadVTF( const char *pszFilename )
//{
//
//	// Load the raw file data
//	CUtlBuffer fileData;
//	if ( !filesystem->ReadFile( pszFilename, "game", fileData ) )
//	{
//		Warning( "Failed to load %s\n", pszFilename);
//		return false;
//	}
//
//	return LoadVTFFromBuffer( fileData, pszFilename );
//}
//
//bool LoadVTFFromBuffer( CUtlBuffer fileData, const char *pszDebugName = "buffer" )
//{
//
//	// Parse it into VTF object
//	IVTFTexture *pVTFTexture( CreateVTFTexture() );
//	if ( !pVTFTexture->Unserialize( fileData ) )
//	{
//		DestroyVTFTexture( pVTFTexture );
//		Warning( "Failed to deserialize VTF %s\n", pszDebugName);
//		return false;
//	}
//
//	// We are re-reading our own files, so they should be 8888's
//	if ( pVTFTexture->Format() != IMAGE_FORMAT_RGBA8888 )
//	{
//		DestroyVTFTexture( pVTFTexture );
//		Warning( "%s isn't RGBA8888\n", pszDebugName);
//		return false;
//	}
//
//	// Copy the image data
//	Allocate( pVTFTexture->Width(), pVTFTexture->Height() );
//	for ( int y = 0 ; y < m_nHeight ; ++y )
//	{
//		memcpy( PixPtr(0, y), pVTFTexture->ImageData(0, 0, 0, 0, y), m_nWidth*4 );
//	}
//
//	// Clean up
//	DestroyVTFTexture( pVTFTexture );
//	return true;
//}
//
//bool SaveVTF( CUtlBuffer &outBuffer )
//{
//	// Create the VTF to write into
//	IVTFTexture *pVTFTexture( CreateVTFTexture() );
//	const int nFlags = TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_SRGB;
//	if ( !pVTFTexture->Init( m_nWidth, m_nHeight, 1, IMAGE_FORMAT_RGBA8888, nFlags, 1, 1 ) )
//	{
//		DestroyVTFTexture( pVTFTexture );
//		return false;
//	}
//
//	// write the rgba image to the vtf texture using the pixel writer
//	CPixelWriter pixelWriter;		
//	pixelWriter.SetPixelMemory( pVTFTexture->Format(), pVTFTexture->ImageData(), pVTFTexture->RowSizeInBytes( 0 ) );
//
//	for (int y = 0; y < m_nHeight; ++y)
//	{
//		pixelWriter.Seek( 0, y );
//		for (int x = 0; x < m_nWidth; ++x)
//		{
//			Color c = GetPix( x, y );
//			pixelWriter.WritePixel( c.r(), c.g(), c.b(), c.a() );
//		}
//	}
//
//	// Serialize to the buffer
//	if ( !pVTFTexture->Serialize( outBuffer ) )
//	{
//		DestroyVTFTexture( pVTFTexture );
//		return false;
//	}
//	DestroyVTFTexture( pVTFTexture );
//	return true;
//}

//void Resize( int nNewSizeX, int nNewSizeY, const Image *pImgSrc = NULL )
//{
//	if ( pImgSrc == NULL )
//	{
//		pImgSrc = this;
//	}
//
//	if ( nNewSizeX == m_nWidth && nNewSizeY == m_nHeight && pImgSrc == this )
//	{
//		return;
//	}
//
//	byte *pNewData = (byte *)malloc( nNewSizeX * nNewSizeY * 4 );
//	ImgUtl_StretchRGBAImage( pImgSrc->m_pBits, pImgSrc->m_nWidth, pImgSrc->m_nHeight, pNewData, nNewSizeX, nNewSizeY );
//	Clear();
//	m_pBits = pNewData;
//	m_nWidth = nNewSizeX;
//	m_nHeight = nNewSizeY;
//}
//
//void Crop( int x0, int y0, int nNewSizeX, int nNewSizeY, const Image *pImgSrc )
//{
//	if ( pImgSrc == NULL )
//	{
//		pImgSrc = this;
//	}
//
//	if ( nNewSizeX == m_nWidth && nNewSizeY == m_nHeight && pImgSrc == this )
//	{
//		return;
//	}
//
//
//	Assert( x0 >= 0 );
//	Assert( y0 >= 0 );
//	Assert( x0 + nNewSizeX <= pImgSrc->m_nWidth );
//	Assert( y0 + nNewSizeY <= pImgSrc->m_nHeight );
//
//	// Allocate new buffer
//	int nRowSize = nNewSizeX * 4;
//	byte *pNewData = (byte *)malloc( nNewSizeY * nRowSize );
//
//	// Copy data, one row at a time
//	for ( int y = 0 ; y < nNewSizeY ; ++y )
//	{
//		memcpy( pNewData + y*nRowSize, pImgSrc->PixPtr(x0, y0+y), nRowSize );
//	}
//
//	// Replace current buffer with the new one
//	Clear();
//	m_pBits = pNewData;
//	m_nWidth = nNewSizeX;
//	m_nHeight = nNewSizeY;
//}

void Bitmap_t::MakeLogicalCopyOf( Bitmap_t &src, bool bTransferBufferOwnership )
{
	// What does it mean to make a logical copy of an
	// invalid bitmap?  I'll tell you what it means: you have a bug.
	Assert( src.IsValid() );

	// Free up anything we already own
	Clear();

	// Copy all of the member variables so we are
	// a logical copy of the source bitmap
	m_nWidth = src.m_nWidth;
	m_nHeight = src.m_nHeight;
	m_nPixelSize = src.m_nPixelSize;
	m_nStride = src.m_nStride;
	m_ImageFormat = src.m_ImageFormat;
	m_pBits = src.m_pBits;
	Assert( !m_bOwnsBuffer );

	// Check for assuming ownership of the buffer
	if ( bTransferBufferOwnership )
	{
		if ( src.m_bOwnsBuffer )
		{
			m_bOwnsBuffer = true;
			src.m_bOwnsBuffer = false;
		}
		else
		{
			// They don't own the buffer?  Then who does?
			// Maybe nobody, and it would safe to assume
			// ownership.  But more than likely, this is a
			// bug.
			Assert( src.m_bOwnsBuffer );

			// And a leak is better than a double-free.
			// Don't assume ownership of the buffer.
		}
	}
}

void Bitmap_t::Crop( int x0, int y0, int nWidth, int nHeight, const Bitmap_t *pImgSource )
{
	// Check for cropping in place, then save off our data to a temp
	Bitmap_t temp;
	if ( pImgSource == this || !pImgSource )
	{
		temp.MakeLogicalCopyOf( *this, m_bOwnsBuffer );
		pImgSource = &temp;
	}

	// No source image?
	if ( !pImgSource->IsValid() )
	{
		Assert( pImgSource->IsValid() );
		return;
	}

	// Sanity check crop rectangle
	Assert( x0 >= 0 );
	Assert( y0 >= 0 );
	Assert( x0 + nWidth <= pImgSource->Width() );
	Assert( y0 + nHeight <= pImgSource->Height() );

	// Allocate buffer
	Init( nWidth, nHeight, pImgSource->Format() );

	// Something wrong?
	if ( !IsValid() )
	{
		Assert( IsValid() );
		return;
	}

	// Copy the data a row at a time
	int nRowSize = m_nWidth * m_nPixelSize;
	for ( int y = 0 ; y < m_nHeight ; ++y )
	{
		memcpy( GetPixel(y,0), pImgSource->GetPixel( x0, y + y0 ), nRowSize );
	}
}

void Bitmap_t::SetPixelData( const Bitmap_t &src, int nSrcX1, int nSrcY1, int nCopySizeX, int nCopySizeY, int nDestX1, int nDestY1 )
{
	// Safety
	if ( !src.IsValid() )
	{
		Assert( src.IsValid() );
		return;
	}
	if ( !IsValid() )
	{
		Assert( IsValid() );
		return;
	}

	// You need to specify a valid source rectangle, we cannot clip that for you
	if ( nSrcX1 < 0 || nSrcY1 < 0 || nSrcX1 + nCopySizeX > src.Width() || nSrcY1 + nCopySizeY > src.Height() )
	{
		Assert( nSrcX1 >= 0 );
		Assert( nSrcY1 >= 0 );
		Assert( nSrcX1 + nCopySizeX <= src.Width() );
		Assert( nSrcY1 + nCopySizeY <= src.Height() );
		return;
	}

	// But we can clip the rectangle if it extends outside the destination image in a perfectly
	// reasonable way
	if ( nDestX1 < 0 )
	{
		nCopySizeX += nDestX1;
		nDestX1 = 0;
	}
	if ( nDestX1 + nCopySizeX > Width() )
	{
		nCopySizeX = Width() - nDestX1;
	}
	if ( nDestY1 < 0 )
	{
		nCopySizeY += nDestY1;
		nDestY1 = 0;
	}
	if ( nDestY1 + nCopySizeY > Height() )
	{
		nCopySizeY = Height() - nDestY1;
	}
	if ( nCopySizeX <= 0 || nCopySizeY <= 0 )
	{
		return;
	}

	// Copy the pixel data
	for ( int y = 0 ; y < nCopySizeY ; ++y )
	{
		// Wow, this could be a lot faster in the common case
		// that the pixe formats are the same.  But...this code
		// is simple and works, and is NOT the root of all evil.
		for ( int x = 0 ; x < nCopySizeX ; ++x )
		{
			Color c = src.GetColor( nSrcX1 + x, nSrcY1 + y );
			SetColor( nDestX1 + x, nDestY1 + y, c );
		}
	}
}

void Bitmap_t::SetPixelData( const Bitmap_t &src, int nDestX1, int nDestY1 )
{
	SetPixelData( src, 0, 0, src.Width(), src.Height(), nDestX1, nDestY1 );
}

//-----------------------------------------------------------------------------
// Returns true if it's a PFM file
//-----------------------------------------------------------------------------
bool IsPFMFile( CUtlBuffer &buf )
{
	int nGet = buf.TellGet();
	char c0 = buf.GetChar();
	char c1 = buf.GetChar();
	char c2 = buf.GetChar();
	buf.SeekGet( CUtlBuffer::SEEK_HEAD, nGet );

	return ( c0 == 'P' && ( c1 == 'F' || c1 == 'f' ) && c2 == 0xA );
}


//-----------------------------------------------------------------------------
// This reads an integer from a binary CUtlBuffer.
//-----------------------------------------------------------------------------
static int ReadIntFromUtlBuffer( CUtlBuffer &buf )
{
	int val = 0;
	int c;
	while( buf.IsValid() )
	{
		c = buf.GetChar();
		if( c >= '0' && c <= '9' )
		{
			val = val * 10 + ( c - '0' );
		}
		else
		{
			buf.SeekGet( CUtlBuffer::SEEK_CURRENT, -1 );
			break;
		}
	}
	return val;
}

static inline bool IsWhitespace( char c )
{
	return c == ' ' || c == '\t' || c == 10;
}

static void EatWhiteSpace( CUtlBuffer &buf )
{
	while( buf.IsValid() )
	{
		int	c = buf.GetChar();
		if( !IsWhitespace( c ) )
		{
			buf.SeekGet( CUtlBuffer::SEEK_CURRENT, -1 );
			return;
		}
	}
	return;
}


//-----------------------------------------------------------------------------
// Reads PFM info + advances to the texture bits
//-----------------------------------------------------------------------------
bool PFMGetInfo_AndAdvanceToTextureBits( CUtlBuffer &pfmBuffer, int &nWidth, int &nHeight, ImageFormat &imageFormat )
{
	pfmBuffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	if( pfmBuffer.GetChar() != 'P' )
	{
		Assert( 0 );
		return false;
	}
	char c = pfmBuffer.GetChar();
	if ( c == 'F' )
	{
		imageFormat = IMAGE_FORMAT_RGB323232F;
	}
	else if ( c == 'f' )
	{
		imageFormat = IMAGE_FORMAT_R32F;
	}
	else
	{
		Assert( 0 );
		return false;
	}

	if( pfmBuffer.GetChar() != 0xa )
	{
		Assert( 0 );
		return false;
	}
	nWidth = ReadIntFromUtlBuffer( pfmBuffer );
	EatWhiteSpace( pfmBuffer );
	nHeight = ReadIntFromUtlBuffer( pfmBuffer );

	// eat crap until the next newline
	while( pfmBuffer.IsValid() && pfmBuffer.GetChar() != 0xa )
	{
	}

	int nScale = ReadIntFromUtlBuffer( pfmBuffer );

	// eat crap until the next newline
	while( pfmBuffer.IsValid() && pfmBuffer.GetChar() != 0xa )
	{
	}

	if ( nScale > 0 )
	{
		pfmBuffer.SetBigEndian( true );
	}

	// Here, the buffer should be at the start of the texture data
	return true;
}


//-----------------------------------------------------------------------------
// Loads a PFM file into a Bitmap_t as RGBA32323232F data
// PFM source data doesn't have alpha, so we put 1.0f into alpha
//-----------------------------------------------------------------------------
bool PFMReadFileRGBA32323232F( CUtlBuffer &fileBuffer, Bitmap_t &bitmap, float pfmScale )
{
	int nWidth = 0, nHeight = 0;
	ImageFormat imageFormat = IMAGE_FORMAT_UNKNOWN;
	PFMGetInfo_AndAdvanceToTextureBits( fileBuffer, nWidth, nHeight, imageFormat );		// Read the header (advances us to the texture bits)
	Assert( imageFormat == IMAGE_FORMAT_RGB323232F );

	bitmap.Init( nWidth, nHeight, IMAGE_FORMAT_RGBA32323232F );	// Init the bitmap

	// NOTE: PFMs are displayed *UPSIDE-DOWN* in Photoshop! (as of CS2... HDRShop gets it right)
	//       Reading rows in reverse order here is the correct thing to do:
	for( int y = ( nHeight - 1 ); y >= 0; y-- )
	{
		Assert( fileBuffer.IsValid() );
		if ( !fileBuffer.IsValid() )
			return false;

		float *pOut = ( (float *)bitmap.GetBits() ) + y*nWidth*4;	// Point to output row

		for ( int x = 0; x < nWidth; x++ )						// For every RGB input color
		{
			fileBuffer.Get( pOut, sizeof( float ) );			// Get red
			*pOut *= pfmScale;									// Scale into output
			pOut++;
			fileBuffer.Get( pOut, sizeof( float ) );			// Get green
			*pOut *= pfmScale;									// Scale into output
			pOut++;
			fileBuffer.Get( pOut, sizeof( float ) );			// Get blue
			*pOut *= pfmScale;									// Scale into output
			pOut++;
			*pOut = 1.0f;										// 1.0f into alpha
			pOut++;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Loads a PFM file into a Bitmap_t as RGB323232F data
//-----------------------------------------------------------------------------
bool PFMReadFileRGB323232F( CUtlBuffer &fileBuffer, Bitmap_t &bitmap, float pfmScale )
{
	// Read the header (advances us to the texture bits)
	int nWidth = 0, nHeight = 0;
	ImageFormat imageFormat = IMAGE_FORMAT_UNKNOWN;
	PFMGetInfo_AndAdvanceToTextureBits( fileBuffer, nWidth, nHeight, imageFormat );
	Assert( imageFormat == IMAGE_FORMAT_RGB323232F );

	// Init the bitmap
	bitmap.Init( nWidth, nHeight, IMAGE_FORMAT_RGB323232F );

	// Read the texels
	for( int y = ( nHeight - 1 ); y >= 0; y-- )
	{
		Assert( fileBuffer.IsValid() );
		if ( !fileBuffer.IsValid() )
			return false;

		// NOTE: PFMs are displayed *UPSIDE-DOWN* in Photoshop! (as of CS2... HDRShop gets it right)
		//       Reading rows in reverse order here is the correct thing to do:
		float *pRow = ( (float *)bitmap.GetBits() ) + y*nWidth*3;

		fileBuffer.Get( pRow, nWidth*3*sizeof( float ) );
		for ( int x = 0; x < nWidth*3; x++ )
		{
			pRow[ x ] *= pfmScale;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Loads the x channel of a PFM file into a Bitmap_t as R32F data
//-----------------------------------------------------------------------------
bool PFMReadFileR32F( CUtlBuffer &fileBuffer, Bitmap_t &bitmap, float pfmScale )
{
	// Read the header (advances us to the texture bits)
	int nWidth, nHeight;
	ImageFormat fileImageFormat; // Format of data in file
	PFMGetInfo_AndAdvanceToTextureBits( fileBuffer, nWidth, nHeight, fileImageFormat );
	Assert( fileImageFormat == IMAGE_FORMAT_RGB323232F );

	// Init the bitmap
	bitmap.Init( nWidth, nHeight, IMAGE_FORMAT_R32F );

	float flMin = FLOAT32_MAX;
	float flMax = FLOAT32_MIN;

	// Read the texels, snarfing out just the x component
	for( int y = ( nHeight - 1 ); y >= 0; y-- )
	{
		Assert( fileBuffer.IsValid() );
		if ( !fileBuffer.IsValid() )
			return false;

		// NOTE: PFMs are displayed *UPSIDE-DOWN* in Photoshop! (as of CS2... HDRShop gets it right)
		//       Reading rows in reverse order here is the correct thing to do:
		float *pRow = ( (float *)bitmap.GetBits() ) + y*nWidth;

		for ( int x = 0; x < nWidth; x++ )
		{
			fileBuffer.Get( pRow+x, sizeof( float ) );		// Grab x component and scale
			pRow[x] *= pfmScale;

			flMin = MIN( pRow[x], flMin );
			flMax = MAX( pRow[x], flMax );

			float flDummy[2];
			fileBuffer.Get( &flDummy, 2*sizeof( float ) );	// Jump past y and z components in file
		}
	}

	printf( "Displacement Range: (%g, %g)\n", flMin, flMax );

	return true;
}


//-----------------------------------------------------------------------------
// Loads a PFM file into a Bitmap_t
//-----------------------------------------------------------------------------
bool PFMReadFile( CUtlBuffer &buf, Bitmap_t *pBitmap )
{
	// Read the header (advances us to the texture bits)
	int nWidth = 0, nHeight = 0;
	ImageFormat fmt = IMAGE_FORMAT_UNKNOWN;
	PFMGetInfo_AndAdvanceToTextureBits( buf, nWidth, nHeight, fmt );

	// Init the bitmap
	pBitmap->Init( nWidth, nHeight, fmt );

	int nRowBytes = ImageLoader::GetMemRequired( nWidth, 1, 1, 1, fmt );

	// Read the texels
	for( int y = ( nHeight - 1 ); y >= 0; y-- )
	{
		Assert( buf.IsValid() );
		if ( !buf.IsValid() )
			return false;

		// NOTE: PFMs are displayed *UPSIDE-DOWN* in Photoshop! (as of CS2... HDRShop gets it right)
		//       Reading rows in reverse order here is the correct thing to do:
		void *pDest = pBitmap->GetBits() + y * nRowBytes;
		buf.Get( pDest, nRowBytes );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Loads a bitmap from an arbitrary file: could be a TGA, PSD, or PFM.
// LoadBitmap autodetects which type
//-----------------------------------------------------------------------------
BitmapFileType_t LoadBitmapFile( CUtlBuffer &buf, Bitmap_t *pBitmap )
{
	if ( IsPSDFile( buf ) )
	{
		// FIXME: Make it actually load the true format
		if ( !PSDReadFileRGBA8888( buf, *pBitmap ) )
			return BITMAP_FILE_TYPE_UNKNOWN;
		return BITMAP_FILE_TYPE_PSD;
	}

	if ( IsPFMFile( buf ) )
	{
		if ( !PFMReadFile( buf, pBitmap ) )
			return BITMAP_FILE_TYPE_UNKNOWN;
		return BITMAP_FILE_TYPE_PFM;
	}

	// It's not really possible to detect TGAness.. there's no identifier
	// Assume TGA file here
	int nWidth, nHeight;
	ImageFormat fmt;
	float flGamma;
	int nGet = buf.TellGet();
	bool bOk = TGALoader::GetInfo( buf, &nWidth, &nHeight, &fmt, &flGamma );
	buf.SeekGet( CUtlBuffer::SEEK_HEAD, nGet );
	if ( !bOk )
		return BITMAP_FILE_TYPE_UNKNOWN;

	// FIXME: TGALoader is incredibly inefficient when trying to just get
	// the bits as-is as it loads into RGBA8888 and then color converts out
	pBitmap->Init( nWidth, nHeight, fmt );
	if ( !TGALoader::Load( pBitmap->GetBits(), buf, nWidth, nHeight, fmt, flGamma, false ) )
		return BITMAP_FILE_TYPE_UNKNOWN;

	return BITMAP_FILE_TYPE_TGA;
}

