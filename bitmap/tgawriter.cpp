//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include <stdlib.h>
#include <stdio.h>
#include "tier0/dbg.h"
#ifndef _PS3
#include <malloc.h>
#else
#include <stdlib.h>
#endif
#include "filesystem.h"
#include "bitmap/tgawriter.h"
#include "tier1/utlbuffer.h"
#include "bitmap/imageformat.h"
#include "tier2/tier2.h"
#include "tier2/fileutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace TGAWriter
{

#pragma pack(1)
struct TGAHeader_t
{
	unsigned char 	id_length;
	unsigned char   colormap_type;
	unsigned char   image_type;
	unsigned short	colormap_index;
	unsigned short  colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin;
	unsigned short	y_origin;
	unsigned short	width;
	unsigned short	height;
	unsigned char	pixel_size;
	unsigned char	attributes;
};
#pragma pack()

#define fputc myfputc
#define fwrite myfwrite

static void fputLittleShort( unsigned short s, CUtlBuffer &buffer )
{
	buffer.PutChar( s & 0xff );
	buffer.PutChar( s >> 8 );
}

static inline void myfputc( unsigned char c, FileHandle_t fileHandle )
{
	g_pFullFileSystem->Write( &c, 1, fileHandle );
}

static inline void myfwrite( void const *data, int size1, int size2, FileHandle_t fileHandle )
{
	g_pFullFileSystem->Write( data, size1 * size2, fileHandle );
}


//-----------------------------------------------------------------------------
// FIXME: assumes that we don't need to do gamma correction.
//-----------------------------------------------------------------------------
bool WriteToBuffer( unsigned char *pImageData, CUtlBuffer &buffer, int width, int height, 
					ImageFormat srcFormat, ImageFormat dstFormat )
{
	TGAHeader_t header;

	// Fix the dstFormat to match what actually is going to go into the file
	switch( dstFormat )
	{
	case IMAGE_FORMAT_RGB888:
		dstFormat = IMAGE_FORMAT_BGR888;
		break;
#if defined( _X360 )
	case IMAGE_FORMAT_LINEAR_RGB888:
		dstFormat = IMAGE_FORMAT_LINEAR_BGR888;
		break;
#endif
	case IMAGE_FORMAT_RGBA8888:
		dstFormat = IMAGE_FORMAT_BGRA8888;
		break;
	}

	header.id_length = 0; // comment length
	header.colormap_type = 0; // ???

	switch( dstFormat )
	{
	case IMAGE_FORMAT_BGR888:
#if defined( _X360 )
	case IMAGE_FORMAT_LINEAR_BGR888:
#endif
		header.image_type = 2; // 24/32 bit uncompressed TGA
		header.pixel_size = 24;
		break;
	case IMAGE_FORMAT_BGRA8888:
		header.image_type = 2; // 24/32 bit uncompressed TGA
		header.pixel_size = 32;
		break;
	case IMAGE_FORMAT_I8:
		header.image_type = 1; // 8 bit uncompressed TGA
		header.pixel_size = 8;
		break;
	default:
		return false;
		break;
	}

	header.colormap_index = 0;
	header.colormap_length = 0;
	header.colormap_size = 0;
	header.x_origin = 0;
	header.y_origin = 0;
	header.width = ( unsigned short )width;
	header.height = ( unsigned short )height;
	header.attributes = 0x20;	// Makes it so we don't have to vertically flip the image

	buffer.PutChar( header.id_length );
	buffer.PutChar( header.colormap_type );
	buffer.PutChar( header.image_type );
	fputLittleShort( header.colormap_index, buffer );
	fputLittleShort( header.colormap_length, buffer );
	buffer.PutChar( header.colormap_size );
	fputLittleShort( header.x_origin, buffer );
	fputLittleShort( header.y_origin, buffer );
	fputLittleShort( header.width, buffer );
	fputLittleShort( header.height, buffer );
	buffer.PutChar( header.pixel_size );
	buffer.PutChar( header.attributes );

	int nSizeInBytes = width * height * ImageLoader::SizeInBytes( dstFormat );
	buffer.EnsureCapacity( buffer.TellPut() + nSizeInBytes );
	unsigned char *pDst = (unsigned char*)buffer.PeekPut();

	if ( !ImageLoader::ConvertImageFormat( pImageData, srcFormat, pDst, dstFormat, width, height ) )
		return false;

	buffer.SeekPut( CUtlBuffer::SEEK_CURRENT, nSizeInBytes );
	return true;
}

bool WriteDummyFileNoAlloc( const char *fileName, int width, int height, enum ImageFormat dstFormat )
{
	TGAHeader_t tgaHeader;

	Assert( g_pFullFileSystem );
	if( !g_pFullFileSystem )
	{
		return false;
	}
	COutputFile fp( fileName );

	int nBytesPerPixel, nImageType, nPixelSize;

	switch( dstFormat )
	{
	case IMAGE_FORMAT_BGR888:
#if defined( _X360 )
	case IMAGE_FORMAT_LINEAR_BGR888:
#endif
		nBytesPerPixel = 3; // 24/32 bit uncompressed TGA
		nPixelSize = 24;
		nImageType = 2;
		break;
	case IMAGE_FORMAT_BGRA8888:
		nBytesPerPixel = 4; // 24/32 bit uncompressed TGA
		nPixelSize = 32;
		nImageType = 2;
		break;
	case IMAGE_FORMAT_I8:
		nBytesPerPixel = 1; // 8 bit uncompressed TGA
		nPixelSize = 8;
		nImageType = 1;
		break;
	default:
		return false;
		break;
	}

    memset( &tgaHeader, 0, sizeof(tgaHeader) );
    tgaHeader.id_length  = 0;
    tgaHeader.image_type = (unsigned char) nImageType;
    tgaHeader.width      = (unsigned short) width;
    tgaHeader.height     = (unsigned short) height;
    tgaHeader.pixel_size = (unsigned char) nPixelSize;
    tgaHeader.attributes = 0x20;

    // Write the Targa header
	fp.Write( &tgaHeader, sizeof(TGAHeader_t) );

	// Write out width * height black pixels
	unsigned char black[4] = { 0x00, 0x00, 0x00, 0x00 }; //  Valve Orange would be: { 0x1E, 0x9A, 0xFF, 0x00 }
	for (int i = 0; i < width * height; i++)
	{
		fp.Write( black, nBytesPerPixel );
	}

	return true;
}

bool WriteTGAFile( const char *fileName, int width, int height, enum ImageFormat srcFormat, uint8 const *srcData, int nStride )
{
	TGAHeader_t tgaHeader;

	COutputFile fp( fileName );

	int nBytesPerPixel, nImageType, nPixelSize;

	bool bMustConvert = false;
	ImageFormat dstFormat = srcFormat;
 
	switch( srcFormat )
	{
	case IMAGE_FORMAT_BGR888:
#if defined( _X360 )
	case IMAGE_FORMAT_LINEAR_BGR888:
#endif
		nBytesPerPixel = 3; // 24/32 bit uncompressed TGA
		nPixelSize = 24;
		nImageType = 2;
		break;
	case IMAGE_FORMAT_BGRA8888:
		nBytesPerPixel = 4; // 24/32 bit uncompressed TGA
		nPixelSize = 32;
		nImageType = 2;
		break;
	case IMAGE_FORMAT_RGBA8888:
		bMustConvert = true;
		dstFormat = IMAGE_FORMAT_BGRA8888;
		nBytesPerPixel = 4; // 24/32 bit uncompressed TGA
		nPixelSize = 32;
		nImageType = 2;
		break;
	case IMAGE_FORMAT_I8:
		nBytesPerPixel = 1; // 8 bit uncompressed TGA
		nPixelSize = 8;
		nImageType = 1;
		break;
	default:
		return false;
		break;
	}

    memset( &tgaHeader, 0, sizeof(tgaHeader) );
    tgaHeader.id_length  = 0;
    tgaHeader.image_type = (unsigned char) nImageType;
    tgaHeader.width      = (unsigned short) width;
    tgaHeader.height     = (unsigned short) height;
    tgaHeader.pixel_size = (unsigned char) nPixelSize;
    tgaHeader.attributes = 0x20;

    // Write the Targa header
	fp.Write( &tgaHeader, sizeof(TGAHeader_t) );

	// Write out image data
	if ( bMustConvert )
	{
		uint8 *pLineBuf = new uint8[ nBytesPerPixel * width ];
		while( height-- )
		{
			ImageLoader::ConvertImageFormat( srcData, srcFormat, pLineBuf, dstFormat, width, 1 );
			fp.Write( pLineBuf, nBytesPerPixel * width );
			srcData += nStride;
		}
		delete[] pLineBuf;
	}
	else
	{
		while( height-- )
		{
			fp.Write( srcData, nBytesPerPixel * width );
			srcData += nStride;
		}
		
	}
	return true;
}

// This routine writes into a width x height subrect of an existing file at the offset ( nXOrigin, nYOrigin ) where the target image has a stride of nStride
bool WriteRectNoAlloc( unsigned char *pImageData, const char *fileName, int nXOrigin, int nYOrigin, int width, int height, int nStride, enum ImageFormat srcFormat )
{
	Assert( g_pFullFileSystem );
	if( !g_pFullFileSystem )
	{
		return false;
	}
	FileHandle_t fp;
	fp = g_pFullFileSystem->Open( fileName, "r+b" );

	// Read in the targa header
	TGAHeader_t tgaHeader;
	g_pFullFileSystem->Read( &tgaHeader, sizeof(tgaHeader), fp );

	int nBytesPerPixel, nImageType, nPixelSize;

	switch( srcFormat )
	{
	case IMAGE_FORMAT_BGR888:
#if defined( _X360 )
	case IMAGE_FORMAT_LINEAR_BGR888:
#endif
		nBytesPerPixel = 3; // 24/32 bit uncompressed TGA
		nPixelSize = 24;
		nImageType = 2;
		break;
	case IMAGE_FORMAT_BGRA8888:
		nBytesPerPixel = 4; // 24/32 bit uncompressed TGA
		nPixelSize = 32;
		nImageType = 2;
		break;
	case IMAGE_FORMAT_I8:
		nBytesPerPixel = 1; // 8 bit uncompressed TGA
		nPixelSize = 8;
		nImageType = 1;
		break;
	default:
		return false;
		break;
	}

	// Verify src data matches the targa we're going to write into
	if ( nPixelSize != tgaHeader.pixel_size )
	{
		Warning( "TGA doesn't match source data.\n" );
		return false;
	}

	// Seek to the origin of the target subrect from the beginning of the file
	g_pFullFileSystem->Seek( fp, nBytesPerPixel * (tgaHeader.width * nYOrigin + nXOrigin), FILESYSTEM_SEEK_CURRENT );

	unsigned char *pSrc = pImageData;

	// Run through each scanline of the incoming rect
	for (int row=0; row < height; row++ )
	{
		g_pFullFileSystem->Write( pSrc, nBytesPerPixel * width, fp );

		// Advance src pointer to next scanline
		pSrc += nBytesPerPixel * nStride;

		// Seek ahead in the file
		g_pFullFileSystem->Seek( fp, nBytesPerPixel * ( tgaHeader.width - width ), FILESYSTEM_SEEK_CURRENT );
	}

	g_pFullFileSystem->Close( fp );
	return true;
}



// Returns weight for Source pixel based on its position in the guardbanded source tile
static float GetSrcWeight( int nFileY, int nFileX, int nFileHeight, int nFileWidth,
						  int nRow, int nCol, int nHeight, int nWidth, int nGuardBandHeight, int nGuardBandWidth )
{
	float flXWeight = 1.0f;
	float flYWeight = 1.0f;

	if ( nFileX < nGuardBandWidth )							// Left edge of file doesn't feather in X
	{
		flXWeight = 1.0f;
	}
	else if ( nFileX > ( nFileWidth - nGuardBandWidth )	)	// Right edge of file doesn't feather in X
	{
		flXWeight = 1.0f;
	}
	else if ( nCol < 2*nGuardBandWidth )					// Left guardband
	{
		flXWeight = nCol / ( (float) nGuardBandWidth * 2.0f );
	}
	else if ( nCol > nWidth )								// Right guardband
	{
		flXWeight = 1.0f - ( ( nCol - nWidth ) / ( (float) nGuardBandWidth * 2.0f ) );
	}


	if ( nFileY < nGuardBandHeight )						// Top edge of file doesn't feather in Y
	{
		flYWeight = 1.0f;
	}
	else if ( nFileY > ( nFileHeight - nGuardBandHeight ) )	// Bottom edge of file doesn't feather in Y
	{
		flYWeight = 1.0f;
	}
	else if ( nRow < 2*nGuardBandHeight )						// Top guardband
	{
		flYWeight = nRow / ( (float) nGuardBandHeight * 2.0f );
	}
	else if ( nRow > nHeight )								// Bottom guardband
	{
		flYWeight = 1.0f - ( ( nRow - nHeight ) / ( (float) nGuardBandHeight * 2.0f ) );
	}

	float flWeight = flXWeight * flYWeight;
	Assert( ( flWeight >= 0 ) && ( flWeight <= 1.0f ) );
	return flWeight;
}

// This flavor of this routine also writes a subrect into an existing file, but it feathers in a ( nGuardBandWidth, nGuardBandHeight ) border,
bool WriteRectNoAllocFeather( unsigned char *pImageData, const char *fileName, int nXOrigin, int nYOrigin, int width, int height,
							  int nGuardBandWidth, int nGuardBandHeight, int nStride, enum ImageFormat srcFormat )
{
	Assert( g_pFullFileSystem );
	if( !g_pFullFileSystem )
		return false;

	FileHandle_t fp;
	fp = g_pFullFileSystem->Open( fileName, "r+b" );

	// Read in the targa header
	TGAHeader_t tgaHeader;
	g_pFullFileSystem->Read( &tgaHeader, sizeof(tgaHeader), fp );

	int nBytesPerPixel, nImageType, nPixelSize;

	switch( srcFormat )
	{
	case IMAGE_FORMAT_BGR888:
#if defined( _X360 )
	case IMAGE_FORMAT_LINEAR_BGR888:
#endif
		nBytesPerPixel = 3; // 24/32 bit uncompressed TGA
		nPixelSize = 24;
		nImageType = 2;
		break;
	case IMAGE_FORMAT_BGRA8888:
		nBytesPerPixel = 4; // 24/32 bit uncompressed TGA
		nPixelSize = 32;
		nImageType = 2;
		break;
	case IMAGE_FORMAT_I8:
		nBytesPerPixel = 1; // 8 bit uncompressed TGA
		nPixelSize = 8;
		nImageType = 1;
		break;
	default:
		return false;
		break;
	}

	// Verify src data matches the targa we're going to write into
	if ( nPixelSize != tgaHeader.pixel_size )
	{
		Warning( "TGA doesn't match source data.\n" );
		return false;
	}

	// Run through each scanline of the incoming tile, including guardbands
	for ( int row=0; row < ( height + 2*nGuardBandHeight ) ; row++ )
	{
		// Point to start of row
		unsigned char *pSrc = pImageData + ( row * nBytesPerPixel * nStride );
		unsigned char filePixel[4] = { 0, 0, 0, 0 };

		// Run through each column of the incoming tile, including guardbands
		for ( int col=0; col < ( width + 2*nGuardBandWidth ) ; col++ )
		{
			// Pixel location in the file
			int fileX = nXOrigin - nGuardBandWidth + col;
			int fileY = nYOrigin - nGuardBandHeight + row;

			// If we're within the limits of the file's image
			if ( ( fileX >= 0 ) && ( fileY >= 0 ) && ( fileX < tgaHeader.width ) && ( fileY < tgaHeader.height ) )
			{
				// Always just seek from the head of the file to the pixel in question and read it in
				g_pFullFileSystem->Seek( fp, sizeof(tgaHeader) + nBytesPerPixel * ( tgaHeader.width * fileY + fileX ), FILESYSTEM_SEEK_HEAD );
				g_pFullFileSystem->Read( filePixel, 3, fp ); // Just read B, G, R bytes

				// Add weighted src pixel to the data from the file (which should be initialized to be black at start of day)
				float flWeight = GetSrcWeight(  fileY, fileX, tgaHeader.height, tgaHeader.width, row, col, height, width, nGuardBandHeight, nGuardBandWidth );
				filePixel[0] = (int) ( flWeight * (float)pSrc[0] + (float)filePixel[0] );
				filePixel[1] = (int) ( flWeight * (float)pSrc[1] + (float)filePixel[1] );
				filePixel[2] = (int) ( flWeight * (float)pSrc[2] + (float)filePixel[2] );

				// Jump back to start of current pixel and write over it
				g_pFullFileSystem->Seek( fp, -3, FILESYSTEM_SEEK_CURRENT );
				g_pFullFileSystem->Write( filePixel, nBytesPerPixel, fp );
			}

			// Advance src pointer to next column
			pSrc += nBytesPerPixel;
		}
	}

	g_pFullFileSystem->Close( fp );
	return true;
}


} // end namespace TGAWriter

