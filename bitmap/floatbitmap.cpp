//===== Copyright © 1996-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose:
//
//===========================================================================//

#include <tier0/platform.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "bitmap/floatbitmap.h"
#include <tier2/tier2.h>
#include "bitmap/imageformat.h"
#include "bitmap/tgaloader.h"
#include "tier1/strtools.h"
#include "filesystem.h"
#include "vstdlib/jobthread.h"

// for PSD loading
#include "bitmap/bitmap.h"
#include "bitmap/psd.h"
#include "tier2/utlstreambuffer.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
IThreadPool *FloatBitMap_t::sm_pFBMThreadPool = NULL;


//-----------------------------------------------------------------------------
// Sets a thread pool for all float bitmap work to be done on
//-----------------------------------------------------------------------------
void FloatBitMap_t::SetThreadPool( IThreadPool* pPool )
{
	sm_pFBMThreadPool = pPool;
}


//-----------------------------------------------------------------------------
// Utility methods
//-----------------------------------------------------------------------------
#define SQ(x) ((x)*(x))

// linear interpolate between 2 control points (L,R)
inline float LinInterp( float frac, float L, float R )
{
	return ( ( ( R - L ) * frac ) + L );
}

// bilinear interpolate between 4 control points (UL,UR,LL,LR)
inline float BiLinInterp( float Xfrac, float Yfrac, float UL, float UR, float LL, float LR )
{
	float iu = LinInterp( Xfrac, UL, UR );
	float il = LinInterp( Xfrac, LL, LR );
	return( LinInterp( Yfrac, iu, il ) );
}

// trilinear interpolate between 8 control points (ULN,URN,LLN,LRN,ULF,URF,LLF,LRF)
inline float TriLinInterp( float Xfrac, float Yfrac, float Zfrac, float ULN, 
	float URN, float LLN, float LRN, float ULF, float URF, float LLF, float LRF )
{
	float iu = LinInterp( Xfrac, ULN, URN );
	float il = LinInterp( Xfrac, LLN, LRN );
	float jn =( LinInterp( Yfrac, iu, il ) );

	iu = LinInterp( Xfrac, ULF, URF );
	il = LinInterp( Xfrac, LLF, LRF );
	float jf =( LinInterp( Yfrac, iu, il ) );

	return( LinInterp( Zfrac, jn, jf ) );
}

static char GetChar( FileHandle_t & f )
{
	char a;
	g_pFullFileSystem->Read( &a, 1, f );
	return a;
}

static int GetInt( FileHandle_t & f )
{
	char buf[100];
	char * bout = buf;
	for( ; ; )
	{
		char c = GetChar( f );
		if ( ( c <'0' ) || ( c >'9' ) )
			break;
		*( bout++ ) = c;
	}
	*( bout++ ) = 0;
	return atoi( buf );
}


//-----------------------------------------------------------------------------
// Constructors
//-----------------------------------------------------------------------------
FloatBitMap_t::FloatBitMap_t( int nWidth, int nHeight, int nDepth, int nAttributeMask )
{
	Init( nWidth, nHeight, nDepth, nAttributeMask );
}

FloatBitMap_t::FloatBitMap_t( const FloatBitMap_t *pOrig ) 
{
	Init( pOrig->NumCols(), pOrig->NumRows(), pOrig->NumSlices() );
	LoadFromFloatBitmap( pOrig );
}


//-----------------------------------------------------------------------------
// Initialize, shutdown
//-----------------------------------------------------------------------------
void FloatBitMap_t::Shutdown()
{
	Purge();
}


//-----------------------------------------------------------------------------
// Construct a floating point gamma table
//-----------------------------------------------------------------------------
static void ConstructFloatGammaTable( float* pTable, float flSrcGamma, float flDstGamma )
{
	float flOO1023 = 1.0f / 1023.0f;
	for( int i = 0; i < 1024; i++ )
	{
		pTable[i] = pow( (float)i * flOO1023, flSrcGamma / flDstGamma );
	}
}

static void ConstructShortGammaTable( uint16* pTable, float flSrcGamma, float flDstGamma )
{
	float flOO1023 = 1.0f / 1023.0f;
	for( int i = 0; i < 1024; i++ )
	{
		pTable[i] = ( uint16 )( 1023.0f * pow( (float)i * flOO1023, flSrcGamma / flDstGamma ) + 0.5f );
	}
}


//-----------------------------------------------------------------------------
// Gets the gamma table
//-----------------------------------------------------------------------------
static const float *GetFloatGammaTable( float flSrcGamma )
{
	// Compute gamma tables...
	static float s_pGammaToLinear[1024];
	static float s_flLastSrcGamma = -1;

	if ( s_flLastSrcGamma != flSrcGamma )
	{
		ConstructFloatGammaTable( s_pGammaToLinear, flSrcGamma, 1.0f );
		s_flLastSrcGamma = flSrcGamma;
	}

	return s_pGammaToLinear;
}

static const uint16 *GetShortGammaTable( float flDestGamma )
{
	// Compute gamma tables...
	static uint16 s_pLinearToGamma[1024];
	static float s_flLastDestGamma = -1;

	if ( s_flLastDestGamma != flDestGamma )
	{
		ConstructShortGammaTable( s_pLinearToGamma, 1.0f, flDestGamma );
		s_flLastDestGamma = flDestGamma;
	}

	return s_pLinearToGamma;
}


//-----------------------------------------------------------------------------
// Loads from a buffer, assumes dimensions match the bitmap size
//-----------------------------------------------------------------------------
void FloatBitMap_t::LoadFromBuffer( const void *pBuffer, size_t nBufSize, ImageFormat fmt, float flGamma )
{
	if ( !pBuffer || !nBufSize )
		return;

	Assert( ImageLoader::GetMemRequired( NumCols(), NumRows(), NumSlices(), 1, fmt ) == (int)nBufSize );
	if( ImageLoader::GetMemRequired( NumCols(), NumRows(), NumSlices(), 1, fmt ) != (int)nBufSize )
	{
		Warning( "FloatBitMap_t::LoadFromBuffer: Received improper buffer size, skipping!\n" );
		return;
	}

	// Cache off constant values
	float c[FBM_ATTR_COUNT] = { 0.0 };
	int nMask = GetAttributeMask();
	for ( int i = 0; i < FBM_ATTR_COUNT; ++i )
	{
		if ( ( nMask & ( 1 << i ) ) == 0 )
		{
			c[i] = ConstantValue( i );
		}
	}

	switch( fmt )
	{
	case IMAGE_FORMAT_ABGR8888:
		LoadFromBufferRGBA( ( const ABGR8888_t* )pBuffer, nBufSize / sizeof(ABGR8888_t), GetFloatGammaTable( flGamma ) );
		break;

	case IMAGE_FORMAT_RGBA8888:
		LoadFromBufferRGBA( ( const RGBA8888_t* )pBuffer, nBufSize / sizeof(RGBA8888_t), GetFloatGammaTable( flGamma ) );
		break;

	case IMAGE_FORMAT_BGRA8888:
		LoadFromBufferRGBA( ( const BGRA8888_t* )pBuffer, nBufSize / sizeof(BGRA8888_t), GetFloatGammaTable( flGamma ) );
		break;

	case IMAGE_FORMAT_RGB888:
		LoadFromBufferRGB( ( const RGB888_t* )pBuffer, nBufSize / sizeof(RGB888_t), GetFloatGammaTable( flGamma ) );
		break;

	case IMAGE_FORMAT_BGR888:
		LoadFromBufferRGB( ( const BGR888_t* )pBuffer, nBufSize / sizeof(BGR888_t), GetFloatGammaTable( flGamma ) );
		break;

	case IMAGE_FORMAT_BGRX8888:
		LoadFromBufferRGB( ( BGRX8888_t* )pBuffer, nBufSize / sizeof(BGRX8888_t), GetFloatGammaTable( flGamma ) );
		break;

	case IMAGE_FORMAT_UV88:
		LoadFromBufferUV( ( const UV88_t* )pBuffer, nBufSize / sizeof(UV88_t) );
		break;

	case IMAGE_FORMAT_UVWQ8888:
		LoadFromBufferUVWQ( ( const UVWQ8888_t* )pBuffer, nBufSize / sizeof(UVWQ8888_t) );
		break;

	case IMAGE_FORMAT_UVLX8888:
		LoadFromBufferUVLX( ( const UVLX8888_t* )pBuffer, nBufSize / sizeof(UVLX8888_t) );
		break;

	default:
		Warning( "FloatBitMap_t::LoadFromBuffer: Unsupported color format, skipping!\n" );
		Assert( 0 );
		break;
	}

	// Restore constant values
	for ( int i = 0; i < FBM_ATTR_COUNT; ++i )
	{
		if ( ( nMask & ( 1 << i ) ) == 0 )
		{
			FillAttr( i, c[i] );
		}
	}
}


void FloatBitMap_t::LoadFromFloatBitmap( const FloatBitMap_t *pOrig )
{
	Assert( ( NumCols() == pOrig->NumCols() ) && ( NumRows() == pOrig->NumRows() ) && ( NumSlices() == pOrig->NumSlices() ) );
	if ( ( NumCols() != pOrig->NumCols() ) || ( NumRows() != pOrig->NumRows() ) || ( NumSlices() != pOrig->NumSlices() ) )
	{
		Warning( "FloatBitMap_t::LoadFromFloatBitmap: Received improper bitmap size, skipping!\n" );
		return;
	}

	float flDefaultVal[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	for ( int i = 0; i < 4; ++i )
	{
		if ( !HasAttributeData( (FBMAttribute_t)i ) )
		{
			FillAttr( i, flDefaultVal[i] );
		}
		else
		{
			CopyAttrFrom( *pOrig, i );
		}
	}
}


//-----------------------------------------------------------------------------
// Writes into a buffer, assumes dimensions match the bitmap size
//-----------------------------------------------------------------------------
void FloatBitMap_t::WriteToBuffer( void *pBuffer, size_t nBufSize, ImageFormat fmt, float flGamma ) const
{
	if ( !pBuffer || !nBufSize )
		return;

	Assert( ImageLoader::GetMemRequired( NumCols(), NumRows(), NumSlices(), 1, fmt ) == (int)nBufSize );
	if( ImageLoader::GetMemRequired( NumCols(), NumRows(), NumSlices(), 1, fmt ) != (int)nBufSize )
	{
		Warning( "FloatBitMap_t::WriteToBuffer: Received improper buffer size, skipping!\n" );
		return;
	}

	switch( fmt )
	{
	case IMAGE_FORMAT_ABGR8888:
		WriteToBufferRGBA( ( ABGR8888_t* )pBuffer, nBufSize / sizeof(ABGR8888_t), GetShortGammaTable( flGamma ) );
		break;

	case IMAGE_FORMAT_RGBA8888:
		WriteToBufferRGBA( ( RGBA8888_t* )pBuffer, nBufSize / sizeof(RGBA8888_t), GetShortGammaTable( flGamma ) );
		break;

	case IMAGE_FORMAT_BGRA8888:
		WriteToBufferRGBA( ( BGRA8888_t* )pBuffer, nBufSize / sizeof(BGRA8888_t), GetShortGammaTable( flGamma ) );
		break;

	case IMAGE_FORMAT_RGB888:
		WriteToBufferRGB( ( RGB888_t* )pBuffer, nBufSize / sizeof(RGB888_t), GetShortGammaTable( flGamma ) );
		break;

	case IMAGE_FORMAT_BGR888:
		WriteToBufferRGB( ( BGR888_t* )pBuffer, nBufSize / sizeof(BGR888_t), GetShortGammaTable( flGamma ) );
		break;

	case IMAGE_FORMAT_BGRX8888:
		WriteToBufferRGB( ( BGRX8888_t* )pBuffer, nBufSize / sizeof(BGRX8888_t), GetShortGammaTable( flGamma ) );
		break;

	case IMAGE_FORMAT_UV88:
		WriteToBufferUV( ( UV88_t* )pBuffer, nBufSize / sizeof(UV88_t) );
		break;

	case IMAGE_FORMAT_UVWQ8888:
		WriteToBufferUVWQ( ( UVWQ8888_t* )pBuffer, nBufSize / sizeof(UVWQ8888_t) );
		break;

	case IMAGE_FORMAT_UVLX8888:
		WriteToBufferUVLX( ( UVLX8888_t* )pBuffer, nBufSize / sizeof(UVLX8888_t) );
		break;

	default:
		Warning( "FloatBitMap_t::WriteToBuffer: Unsupported color format, skipping!\n" );
		Assert( 0 );
		break;
	}
}

//-----------------------------------------------------------------------------
// Loads from a PSD file
//-----------------------------------------------------------------------------
bool FloatBitMap_t::LoadFromPSD( const char *pFilename )
{
	Bitmap_t bitmap;
	CUtlStreamBuffer buf( pFilename, "GAME", CUtlBuffer::READ_ONLY );
	if ( IsPSDFile( buf ) )
	{
		if ( !PSDReadFileRGBA8888( buf, bitmap ) )
			return false;

		if ( bitmap.Format() != IMAGE_FORMAT_RGBA8888 )
			return false;
	}
	Init( bitmap.Width(), bitmap.Height() );
	for ( int x = 0; x < bitmap.Width(); x++ )
	{
		for ( int y = 0; y < bitmap.Height(); y++ )
		{
			RGBA8888_t* pPixel = (RGBA8888_t*)bitmap.GetPixel( x, y );
 			Pixel( x, y, 0, 0 ) = pPixel->r / 255.0f;
 			Pixel( x, y, 0, 1 ) = pPixel->g / 255.0f;
 			Pixel( x, y, 0, 2 ) = pPixel->b / 255.0f;
			Pixel( x, y, 0, 3 ) = pPixel->a / 255.0f;
		}
	}
	return true;
}	

//-----------------------------------------------------------------------------
// Loads from a PFM file
//-----------------------------------------------------------------------------
#define PFM_MAX_XSIZE 2048
bool FloatBitMap_t::LoadFromPFM( const char* fname )
{
	bool bSuccess = false;
	FileHandle_t f = g_pFullFileSystem->Open(fname, "rb");
	if ( !f )
		return false;

	if( ( GetChar( f ) == 'P' ) && ( GetChar( f ) == 'F' ) && ( GetChar( f ) == '\n' ) )
	{
		int nWidth = GetInt( f );
		int nHeight = GetInt( f );

		// eat crap until the next newline
		while( GetChar( f ) != '\n' )
		{
		}

		//			printf("file %s w=%d h=%d\n",fname,Width,Height);
		Init( nWidth, nHeight );

		for( int y = nHeight - 1; y >= 0; y-- )
		{
			float linebuffer[PFM_MAX_XSIZE * 3];
			g_pFullFileSystem->Read( linebuffer, 3 * nWidth * sizeof( float ), f );
			for( int x = 0; x < nWidth; x++ )
			{
				for( int c = 0; c < 3; c++ )
				{
					Pixel( x, y, 0, c ) = linebuffer[x * 3 + c];
				}
			}
		}
		bSuccess = true;
	}
	g_pFullFileSystem->Close( f );	// close file after reading
	return bSuccess;
}

bool FloatBitMap_t::WritePFM( const char* fname )
{
	FileHandle_t f = g_pFullFileSystem->Open(fname, "wb");

	if ( f )
	{
		g_pFullFileSystem->FPrintf( f, "PF\n%d %d\n-1.000000\n", NumCols(), NumRows());
		for( int y = NumRows() - 1; y >= 0; y-- )
		{
			float linebuffer[PFM_MAX_XSIZE * 3];
			for( int x = 0; x < NumCols(); x++ )
			{
				for( int c = 0; c < 3; c++ )
				{
					linebuffer[x * 3 + c]= Pixel( x, y, 0, c );
				}
			}
			g_pFullFileSystem->Write( linebuffer, 3 * NumCols() * sizeof( float ), f );
		}
		g_pFullFileSystem->Close( f );

		return true;
	}

	return false;
}


float FloatBitMap_t::InterpolatedPixel( float x, float y, int comp ) const
{
	int Top = ( int )floor( y );
	float Yfrac = y - Top;
	int Bot = MIN( NumRows() - 1, Top + 1 );
	int Left = ( int )floor( x );
	float Xfrac = x - Left;
	int Right = MIN( NumCols() - 1, Left + 1 );
	return
		BiLinInterp( Xfrac, Yfrac,
		Pixel( Left, Top, 0, comp ),
		Pixel( Right, Top, 0, comp ),
		Pixel( Left, Bot, 0, comp ),
		Pixel( Right, Bot, 0, comp ) );

}

float FloatBitMap_t::InterpolatedPixel( float x, float y, float z, int comp ) const
{
	int Top = ( int )floor( y );
	float Yfrac = y - Top;
	int Bot = MIN( NumRows() - 1, Top + 1 );
	int Left = ( int )floor( x );
	float Xfrac = x - Left;
	int Right = MIN( NumCols() - 1, Left + 1 );
	int Near = ( int )floor( z );
	float Zfrac = z - Near;
	int Far = MIN( NumSlices() - 1, Near + 1 );
	return
		TriLinInterp( Xfrac, Yfrac, Zfrac,
		Pixel( Left, Top, Near, comp ),
		Pixel( Right, Top, Near, comp ),
		Pixel( Left, Bot, Near, comp ),
		Pixel( Right, Bot, Near, comp ),
		Pixel( Left, Top, Far, comp ),
		Pixel( Right, Top, Far, comp ),
		Pixel( Left, Bot, Far, comp ),
		Pixel( Right, Bot, Far, comp ) );
}


//-----------------------------------------------------------------------------
// Method to slam a particular channel to always be the same value 
//-----------------------------------------------------------------------------
void FloatBitMap_t::SetChannel( int comp, float flValue )
{
	for ( int z = 0; z < NumSlices(); z++ )
	{
		for ( int y = 0; y < NumRows(); y++ )
		{
			for ( int x = 0; x < NumCols(); x++ )
			{
				Pixel( x, y, z, comp ) = flValue;
			}
		}
	}
}


//-----------------------------------------------------------------
// resize (with bilinear filter) truecolor bitmap in place

void FloatBitMap_t::ReSize( int NewWidth, int NewHeight )
{
	float XRatio = ( float )NumCols() / ( float )NewWidth;
	float YRatio = ( float )NumRows() / ( float )NewHeight;
	float SourceX, SourceY, Xfrac, Yfrac;
	int Top, Bot, Left, Right;

	FloatBitMap_t newrgba( NewWidth, NewHeight );

	SourceY = 0;
	for( int y = 0; y < NewHeight; y++ )
	{
		Yfrac = SourceY - floor( SourceY );
		Top = ( int )SourceY;
		Bot = ( int )(SourceY + 1 );
		if ( Bot >= NumRows() )
			Bot = NumRows() - 1;
		SourceX = 0;
		for( int x = 0; x < NewWidth; x++ )
		{
			Xfrac = SourceX - floor( SourceX );
			Left = ( int )SourceX;
			Right = ( int )( SourceX + 1 );
			if ( Right >= NumCols() )
				Right = NumCols() - 1;
			for( int c = 0; c < 4; c++ )
			{
				newrgba.Pixel( x, y, 0, c ) = BiLinInterp( Xfrac, Yfrac,
														Pixel( Left, Top, 0, c ),
														Pixel( Right, Top, 0, c ),
														Pixel( Left, Bot, 0, c ),
														Pixel( Right, Bot, 0, c ) );
			}
			SourceX += XRatio;
		}
		SourceY += YRatio;
	}
	MoveDataFrom( newrgba );
}

//-----------------------------------------------------------------------------
// Makes the image be a sub-range of the current image
//-----------------------------------------------------------------------------
void FloatBitMap_t::Crop( int x1, int y1, int z1, int nWidth, int nHeight, int nDepth )
{
	int x2 = x1 + nWidth;
	int y2 = y1 + nHeight;
	int z2 = z1 + nDepth;

	if ( ( x1 >= NumCols() || y1 >= NumRows() || z1 >= NumSlices() ) || ( x2 <= 0 || y2 <= 0 || z2 <= 0 ) )
	{
		Init( nWidth, nHeight, nDepth );
		return;
	}

	x1 = clamp( x1, 0, NumCols() );
	y1 = clamp( y1, 0, NumRows() );
	z1 = clamp( z1, 0, NumSlices() );
	x2 = clamp( x2, 0, NumCols() );
	y2 = clamp( y2, 0, NumRows() );
	z2 = clamp( z2, 0, NumSlices() );
	nWidth = x2 - x1; nHeight = y2 - y1; nDepth = z2 - z1;

	// Check for crops that don't require data movement
	if ( NumSlices() <= 1 )
	{
		if ( nWidth == NumCols() )
		{
			m_nRows = nHeight;
			return;
		}
	}
	else
	{
		if ( nWidth == NumCols() && nHeight == NumRows() )
		{
			m_nSlices = nDepth;
			return;
		}
	}

	// Generic data movement; works because data is stored in ascending
	// order in memory in the same way as we're looping over the data
	int ci[4];
	int nAttr = ComputeValidAttributeList( ci );
	for ( int z = 0; z < nDepth; ++z )
	{
		for ( int y = 0; y < nHeight; ++y )
		{
			for ( int x = 0; x < nWidth; ++x )
			{
				for ( int c = 0; c < nAttr; ++c )
				{
					Pixel( x, y, z, ci[c] ) = Pixel( x + x1, y + y1, z + z1, ci[c] );
				}
			}
		}
	}
}



struct TGAHeader_t
{
	unsigned char 	id_length, colormap_type, image_type;
	unsigned char	colormap_index0, colormap_index1, colormap_length0, colormap_length1;
	unsigned char	colormap_size;
	unsigned char	x_origin0, x_origin1, y_origin0, y_origin1, width0, width1, height0, height1;
	unsigned char	pixel_size, attributes;
};

bool FloatBitMap_t::WriteTGAFile( const char* filename ) const
{
	FileHandle_t f = g_pFullFileSystem->Open(filename, "wb");
	if ( f )
	{
		TGAHeader_t myheader;
		memset( & myheader, 0, sizeof( myheader ) );
		myheader.image_type = 2;
		myheader.pixel_size = 32;
		myheader.width0 = NumCols() & 0xff;
		myheader.width1 = ( NumCols() >> 8 );
		myheader.height0 = NumRows() & 0xff;
		myheader.height1 = ( NumRows() >> 8 );
		myheader.attributes = 0x20;
		g_pFullFileSystem->Write( & myheader, sizeof( myheader ), f );
		// now, write the pixels
		for( int y = 0; y < NumRows(); y++ )
		{
			for( int x = 0; x < NumCols(); x++ )
			{
				PixRGBAF fpix = PixelRGBAF( x, y, 0 );
				PixRGBA8 pix8 = PixRGBAF_to_8( fpix );

				g_pFullFileSystem->Write( & pix8.Blue, 1, f );
				g_pFullFileSystem->Write( & pix8.Green, 1, f );
				g_pFullFileSystem->Write( & pix8.Red, 1, f );
				g_pFullFileSystem->Write( & pix8.Alpha, 1, f );
			}
		}
		g_pFullFileSystem->Close( f );	// close file after reading

		return true;
	}
	return false;
}

FloatBitMap_t::FloatBitMap_t( const char* tgafilename )
{
	// load from a tga or pfm or psd
	if ( Q_stristr( tgafilename, ".psd") )
	{
		LoadFromPSD( tgafilename );
		return;
	}
	if ( Q_stristr( tgafilename, ".pfm") )
	{
		LoadFromPFM( tgafilename );
		return;
	}

	int width1, height1;
	ImageFormat imageFormat1;
	float gamma1;

	if( !TGALoader::GetInfo( tgafilename, &width1, &height1, &imageFormat1, &gamma1 ) )
	{
		Warning( "FloatBitMap_t: Error opening %s\n", tgafilename );
		Init( 1, 1 );
		Pixel( 0, 0, 0, 0 ) = Pixel( 0, 0, 0, 3 ) = 1.0f;
		Pixel( 0, 0, 0, 1 ) = Pixel( 0, 0, 0, 2 ) = 0.0f;
		return;
	}

	Init( width1, height1 );

	uint8 * pImage1Tmp =
		new uint8 [ImageLoader::GetMemRequired( width1, height1, 1, imageFormat1, false )];

	if( !TGALoader::Load( pImage1Tmp, tgafilename, width1, height1, imageFormat1, 2.2f, false ) )
	{
		Warning( "FloatBitMap_t: Error loading %s\n", tgafilename );
		Init( 1, 1 );
		Pixel( 0, 0, 0, 0 ) = Pixel( 0, 0, 0, 3 ) = 1.0f;
		Pixel( 0, 0, 0, 1 ) = Pixel( 0, 0, 0, 2 ) = 0.0f;
		delete[] pImage1Tmp;
		return;
	}
	uint8 * pImage1 =
		new uint8 [ImageLoader::GetMemRequired( width1, height1, 1, IMAGE_FORMAT_ABGR8888, false )];

	ImageLoader::ConvertImageFormat( pImage1Tmp, imageFormat1, pImage1, IMAGE_FORMAT_ABGR8888, width1, height1, 0, 0 );

	for( int y = 0; y < height1; y++ )
	{
		for( int x = 0; x < width1; x++ )
		{
			for( int c = 0; c < 4; c++ )
			{
				Pixel( x, y, 0, 3 - c ) = pImage1[c + 4 * ( x + ( y * width1 ) )]/ 255.0;
			}
		}
	}

	delete[] pImage1;
	delete[] pImage1Tmp;
}


//-----------------------------------------------------------------------------
// Downsample using bilinear filtering
//-----------------------------------------------------------------------------
void FloatBitMap_t::QuarterSize2D( FloatBitMap_t *pDest, int nStart, int nCount )
{
	for( int y = nStart; y < nStart + nCount; y++ )
	{
		for( int x = 0; x < pDest->NumCols(); x++ )
		{
			for( int c = 0; c < 4; c++ )
			{
				pDest->Pixel( x, y, 0, c ) = 
					( ( Pixel( x * 2, y * 2, 0, c ) +		Pixel( x * 2 + 1, y * 2, 0, c ) +
						Pixel( x * 2, y * 2 + 1, 0, c ) +	Pixel( x * 2 + 1, y * 2 + 1, 0, c ) ) / 4.0f );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Downsample using trilinear filtering
//-----------------------------------------------------------------------------
void FloatBitMap_t::QuarterSize3D( FloatBitMap_t *pDest, int nStart, int nCount )
{
	for( int z = nStart; z < nStart + nCount; z++ )
	{
		for( int y = 0; y < pDest->NumRows(); y++ )
		{
			for( int x = 0; x < pDest->NumCols(); x++ )
			{
				for( int c = 0; c < 4; c++ )
				{
					pDest->Pixel( x, y, z, c ) = 
						( ( Pixel( x * 2, y * 2,	z * 2,		c ) +	Pixel( x * 2 + 1, y * 2,	z * 2,		c ) +
							Pixel( x * 2, y * 2 + 1,z * 2,		c ) +	Pixel( x * 2 + 1, y * 2 + 1,z * 2,		c ) +
							Pixel( x * 2, y * 2,	z * 2 + 1,	c ) +	Pixel( x * 2 + 1, y * 2,	z * 2 + 1,	c ) +
							Pixel( x * 2, y * 2 + 1,z * 2 + 1,	c ) +	Pixel( x * 2 + 1, y * 2 + 1,z * 2 + 1,	c ) ) / 8.0f );
				}
			}
		}
	}
}

void FloatBitMap_t::QuarterSize( FloatBitMap_t *pBitmap )
{
	// generate a new bitmap half on each axis
	bool bIs2D = ( NumSlices() == 1 );

	int sx = NumCols() / 2;
	int sy = NumRows() / 2;
	int sz = NumSlices() / 2;
	sx = MAX( sx, 1 );
	sy = MAX( sy, 1 );
	sz = MAX( sz, 1 );

	pBitmap->Init( sx, sy, sz );

	if ( bIs2D )
	{
		ParallelLoopProcessChunks( sm_pFBMThreadPool, pBitmap, 0, sy, 16, this, &FloatBitMap_t::QuarterSize2D );
	}
	else
	{
		ParallelLoopProcessChunks( sm_pFBMThreadPool, pBitmap, 0, sz, 8, this, &FloatBitMap_t::QuarterSize3D );
	}
}


//-----------------------------------------------------------------------------
// Downsample using bilinear filtering
//-----------------------------------------------------------------------------
void FloatBitMap_t::QuarterSizeBlocky2D( FloatBitMap_t *pDest, int nStart, int nCount )
{
	for( int y = nStart; y < nStart + nCount; y++ )
	{
		for( int x = 0; x < pDest->NumCols(); x++ )
		{
			for( int c = 0; c < 4; c++ )
			{
				pDest->Pixel( x, y, 0, c ) = Pixel( x * 2, y * 2, 0, c );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Downsample using trilinear filtering
//-----------------------------------------------------------------------------
void FloatBitMap_t::QuarterSizeBlocky3D( FloatBitMap_t *pDest, int nStart, int nCount )
{
	for( int z = nStart; z < nStart + nCount; z++ )
	{
		for( int y = 0; y < pDest->NumRows(); y++ )
		{
			for( int x = 0; x < pDest->NumCols(); x++ )
			{
				for( int c = 0; c < 4; c++ )
				{
					pDest->Pixel( x, y, z, c ) = Pixel( x * 2, y * 2, z * 2, c );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Downsample using point sampling
//-----------------------------------------------------------------------------
void FloatBitMap_t::QuarterSizeBlocky( FloatBitMap_t *pBitmap )
{
	// generate a new bitmap half on each axis
	int sx = NumCols() / 2;
	int sy = NumRows() / 2;
	int sz = NumSlices() / 2;
	sx = MAX( sx, 1 );
	sy = MAX( sy, 1 );
	sz = MAX( sz, 1 );

	pBitmap->Init( sx, sy, sz );

	if ( NumSlices() == 1 )
	{
		ParallelLoopProcessChunks( sm_pFBMThreadPool, pBitmap, 0, sy, 128, this, &FloatBitMap_t::QuarterSizeBlocky2D );
	}
	else
	{
		ParallelLoopProcessChunks( sm_pFBMThreadPool, pBitmap, 0, sz, 32, this, &FloatBitMap_t::QuarterSizeBlocky3D );
	}
}


enum KernelType_t
{
	KERNEL_DEFAULT = 0,
	KERNEL_ALPHATEST,
};

struct FloatBitmapResampleInfo_t
{
	FloatBitMap_t *m_pSrcBitmap;
	FloatBitMap_t *m_pDestBitmap;
	const KernelInfo_t *m_pKernel;
	float *m_pAlphaResult;
	int m_nFlags;
	int m_nSrcWidth;
	int m_nSrcHeight;
	int m_nSrcDepth;
	float m_flAlphaThreshhold;
	float m_flAlphaHiFreqThreshhold;
	int m_nWRatio;
	int m_nHRatio;
	int m_nDRatio;
};

typedef void (*ApplyKernelFunc_t)( FloatBitmapResampleInfo_t *pInfo, int nStart, int nCount );

//-----------------------------------------------------------------------------
// Apply Kernel to an image
//-----------------------------------------------------------------------------
template< int type, bool bIsPowerOfTwo >
class CKernelWrapper
{
public:
	static inline int ActualX( int x, FloatBitmapResampleInfo_t *pInfo )
	{
		if ( pInfo->m_nFlags & DOWNSAMPLE_CLAMPS )
			return clamp( x, 0, pInfo->m_nSrcWidth - 1 );

		// This works since pInfo->m_nSrcWidth is a power of two.
		// Even for negative #s!
		if ( bIsPowerOfTwo )
			return x & (pInfo->m_nSrcWidth - 1);
		return x % pInfo->m_nSrcWidth;
	}

	static inline int ActualY( int y, FloatBitmapResampleInfo_t *pInfo )
	{
		if ( pInfo->m_nFlags & DOWNSAMPLE_CLAMPT )
			return clamp( y, 0, pInfo->m_nSrcHeight - 1 );

		// This works since pInfo->m_nSrcHeight is a power of two.
		// Even for negative #s!
		if ( bIsPowerOfTwo )
			return y & (pInfo->m_nSrcHeight - 1);
		return y % pInfo->m_nSrcHeight;
	}

	static inline int ActualZ( int z, FloatBitmapResampleInfo_t *pInfo )
	{
		if ( pInfo->m_nFlags & DOWNSAMPLE_CLAMPU )
			return clamp( z, 0, pInfo->m_nSrcDepth - 1 );

		// This works since pInfo->m_nSrcDepth is a power of two.
		// Even for negative #s!
		if ( bIsPowerOfTwo )
			return z & (pInfo->m_nSrcDepth - 1);
		return z % pInfo->m_nSrcDepth;
	}

	static void ComputeWeightedAverageColor( FloatBitmapResampleInfo_t *pInfo, 
		int startX, int startY, int startZ, float *total )
	{
		total[0] = total[1] = total[2] = total[3] = 0.0f;
		for ( int j = 0, srcZ = startZ; j < pInfo->m_pKernel->m_nDepth; ++j, ++srcZ )
		{
			int sz = ActualZ( srcZ, pInfo );

			for ( int k = 0, srcY = startY; k < pInfo->m_pKernel->m_nHeight; ++k, ++srcY )
			{
				int sy = ActualY( srcY, pInfo );

				int kernelIdx = pInfo->m_pKernel->m_nWidth * ( k + j * pInfo->m_pKernel->m_nHeight );
				for ( int l = 0, srcX = startX; l < pInfo->m_pKernel->m_nWidth; ++l, ++srcX, ++kernelIdx )
				{
					int sx = ActualX( srcX, pInfo );					

					float flKernelFactor = pInfo->m_pKernel->m_pKernel[kernelIdx];
					if ( flKernelFactor == 0.0f )
						continue;

					total[FBM_ATTR_RED] += flKernelFactor * pInfo->m_pSrcBitmap->Pixel( sx, sy, sz, FBM_ATTR_RED );
					total[FBM_ATTR_GREEN] += flKernelFactor * pInfo->m_pSrcBitmap->Pixel( sx, sy, sz, FBM_ATTR_GREEN );
					total[FBM_ATTR_BLUE] += flKernelFactor * pInfo->m_pSrcBitmap->Pixel( sx, sy, sz, FBM_ATTR_BLUE );
					if ( type != KERNEL_ALPHATEST )
					{
						total[FBM_ATTR_ALPHA] += flKernelFactor * pInfo->m_pSrcBitmap->Pixel( sx, sy, sz, FBM_ATTR_ALPHA );
					}
					else
					{
						if ( pInfo->m_pSrcBitmap->Pixel( sx, sy, sz, FBM_ATTR_ALPHA ) > ( 192.0f / 255.0f ) )
						{
							total[FBM_ATTR_ALPHA] += flKernelFactor;
						}
					}
				}
			}
		}	
	}

	static void AddAlphaToAlphaResult( FloatBitmapResampleInfo_t *pInfo, 
		int startX, int startY, int startZ, float flAlpha, float *pAlphaResult )
	{
		for ( int j = 0, srcZ = startZ; j < pInfo->m_pKernel->m_nDepth; ++j, ++srcZ )
		{
			int sz = ActualZ( srcZ, pInfo );
			sz *= pInfo->m_nSrcWidth * pInfo->m_nSrcHeight;

			for ( int k = 0, srcY = startY; k < pInfo->m_pKernel->m_nHeight; ++k, ++srcY )
			{
				int sy = ActualY( srcY, pInfo );
				sy *= pInfo->m_nSrcWidth;

				int kernelIdx = k * pInfo->m_pKernel->m_nWidth + j * pInfo->m_pKernel->m_nWidth * pInfo->m_pKernel->m_nHeight;
				for ( int l = 0, srcX = startX; l < pInfo->m_pKernel->m_nWidth; ++l, ++srcX, ++kernelIdx )
				{
					int sx = ActualX( srcX, pInfo );					
					int srcPixel = sz + sy + sx;

					float flKernelFactor = pInfo->m_pKernel->m_pInvKernel[kernelIdx];
					if ( flKernelFactor == 0.0f )
						continue;
					pAlphaResult[srcPixel] += flKernelFactor * flAlpha;
				}
			}
		}
	}

	static void AdjustAlphaChannel( FloatBitmapResampleInfo_t *pInfo, float *pAlphaResult )
	{
		// Find the delta between the alpha + source image
		int i, k;
		int nDstPixel = 0;
		for ( k = 0; k < pInfo->m_nSrcDepth; ++k )
		{
			for ( i = 0; i < pInfo->m_nSrcHeight; ++i )
			{
				for ( int j = 0; j < pInfo->m_nSrcWidth; ++j, ++nDstPixel )
				{
					pAlphaResult[nDstPixel] = fabs( pAlphaResult[nDstPixel] - pInfo->m_pSrcBitmap->Pixel( j, i, k, FBM_ATTR_ALPHA ) );
				}
			}
		}

		// Apply the kernel to the image
		int nInitialZ = ( pInfo->m_nDRatio > 1 ) ? (pInfo->m_nDRatio >> 1) - ((pInfo->m_nDRatio * pInfo->m_pKernel->m_nDiameter) >> 1) : 0;
		int nInitialY = ( pInfo->m_nHRatio > 1 ) ? (pInfo->m_nHRatio >> 1) - ((pInfo->m_nHRatio * pInfo->m_pKernel->m_nDiameter) >> 1) : 0;
		int nInitialX = ( pInfo->m_nWRatio > 1 ) ? (pInfo->m_nWRatio >> 1) - ((pInfo->m_nWRatio * pInfo->m_pKernel->m_nDiameter) >> 1) : 0;

		float flInvFactor = 1.0f;
		if ( pInfo->m_nDRatio != 0 )
			flInvFactor *= pInfo->m_nDRatio;
		if ( pInfo->m_nHRatio != 0 )
			flInvFactor *= pInfo->m_nHRatio;
		if ( pInfo->m_nWRatio != 0 )
			flInvFactor *= pInfo->m_nWRatio;
		flInvFactor = 1.0f / flInvFactor;

		for ( int h = 0; h < pInfo->m_pDestBitmap->NumSlices(); ++h )
		{
			int startZ = pInfo->m_nDRatio * h + nInitialZ;
			for ( i = 0; i < pInfo->m_pDestBitmap->NumRows(); ++i )
			{
				int startY = pInfo->m_nHRatio * i + nInitialY;
				for ( int j = 0; j < pInfo->m_pDestBitmap->NumCols(); ++j )
				{
					if ( pInfo->m_pDestBitmap->Pixel( j, i, h, FBM_ATTR_ALPHA ) == 1.0f )
						continue;

					int startX = pInfo->m_nWRatio * j + nInitialX;
					float flAlphaDelta = 0.0f;

					for ( int m = 0, srcZ = startZ; m < pInfo->m_nDRatio; ++m, ++srcZ )
					{
						int sz = ActualZ( srcZ, pInfo );
						sz *= pInfo->m_nSrcWidth * pInfo->m_nSrcHeight;

						for ( int k = 0, srcY = startY; k < pInfo->m_nHRatio; ++k, ++srcY )
						{
							int sy = ActualY( srcY, pInfo );
							sy *= pInfo->m_nSrcWidth;

							for ( int l = 0, srcX = startX; l < pInfo->m_nWRatio; ++l, ++srcX )
							{
								// HACK: This temp variable fixes an internal compiler error in vs2005
								int temp = srcX;
								int sx = ActualX( temp, pInfo );

								int srcPixel = sz + sy + sx;
								flAlphaDelta += pAlphaResult[srcPixel];
							}
						}
					}

					flAlphaDelta *= flInvFactor;
					if ( flAlphaDelta > pInfo->m_flAlphaHiFreqThreshhold )
					{
						pInfo->m_pDestBitmap->Pixel( j, i, h, FBM_ATTR_ALPHA ) = 1.0f;
					}
				}
			}
		}
	}

	static void ApplyKernel( FloatBitmapResampleInfo_t *pInfo, int nStart, int nCount )
	{
		// Apply the kernel to the image
		int nInitialZ = ( pInfo->m_nDRatio > 1 ) ? (pInfo->m_nDRatio >> 1) - ((pInfo->m_nDRatio * pInfo->m_pKernel->m_nDiameter) >> 1) : 0;
		int nInitialY = ( pInfo->m_nHRatio > 1 ) ? (pInfo->m_nHRatio >> 1) - ((pInfo->m_nHRatio * pInfo->m_pKernel->m_nDiameter) >> 1) : 0;
		int nInitialX = ( pInfo->m_nWRatio > 1 ) ? (pInfo->m_nWRatio >> 1) - ((pInfo->m_nWRatio * pInfo->m_pKernel->m_nDiameter) >> 1) : 0;

		int dw = pInfo->m_pDestBitmap->NumCols();
		int dh = pInfo->m_pDestBitmap->NumRows();
		int dd = pInfo->m_pDestBitmap->NumSlices();

		int sk, ek, si, ei;
		if ( dd == 1 )
		{
			sk = 0; ek = dd; si = nStart; ei = nStart + nCount;
		}
		else
		{
			sk = nStart; ek = nStart + nCount; si = 0; ei = dh;
		}

		for ( int k = sk; k < ek; ++k )
		{
			int startZ = pInfo->m_nDRatio * k + nInitialZ;
			for ( int i = si; i < ei; ++i )
			{
				int startY = pInfo->m_nHRatio * i + nInitialY;
				for ( int j = 0; j < dw; ++j )
				{
					int startX = pInfo->m_nWRatio * j + nInitialX;

					float total[4];
					ComputeWeightedAverageColor( pInfo, startX, startY, startZ, total );

					// NOTE: Can't use a table here, we lose too many bits
					if( type != KERNEL_ALPHATEST )
					{
						for ( int ch = 0; ch < 4; ++ ch )
						{
							pInfo->m_pDestBitmap->Pixel( j, i, k, ch ) = clamp( total[ch], 0.0f, 1.0f );
						}
					}
					else
					{
						// If there's more than 40% coverage, then keep the pixel (renormalize the color based on coverage)
						float flAlpha = ( total[3] >= pInfo->m_flAlphaThreshhold ) ? 1.0f : 0.0f;

						for ( int ch = 0; ch < 3; ++ ch )
						{
							pInfo->m_pDestBitmap->Pixel( j, i, k, ch ) = clamp( total[ch], 0.0f, 1.0f );
						}
						pInfo->m_pDestBitmap->Pixel( j, i, k, FBM_ATTR_ALPHA ) = clamp( flAlpha, 0.0f, 1.0f );

						AddAlphaToAlphaResult( pInfo, startX, startY, startZ, flAlpha, pInfo->m_pAlphaResult );
					}
				}
			}
		}

		if ( type == KERNEL_ALPHATEST )
		{
			AdjustAlphaChannel( pInfo, pInfo->m_pAlphaResult );
		}
	}
};

typedef CKernelWrapper< KERNEL_DEFAULT, false >		ApplyKernelDefault_t;
typedef CKernelWrapper< KERNEL_ALPHATEST, false >	ApplyKernelAlphatest_t;
typedef CKernelWrapper< KERNEL_DEFAULT, true >		ApplyKernelDefaultPow2_t;
typedef CKernelWrapper< KERNEL_ALPHATEST, true >	ApplyKernelAlphatestPow2_t;

static ApplyKernelFunc_t g_KernelFunc[] =
{
	ApplyKernelDefault_t::ApplyKernel,
	ApplyKernelAlphatest_t::ApplyKernel,
};

static ApplyKernelFunc_t g_KernelFuncPow2[] =
{
	ApplyKernelDefaultPow2_t::ApplyKernel,
	ApplyKernelAlphatestPow2_t::ApplyKernel,
};

//-----------------------------------------------------------------------------
// Downsample using specified kernel (NOTE: Dest bitmap needs to have been initialized w/ final size)
//-----------------------------------------------------------------------------
void FloatBitMap_t::DownsampleNiceFiltered( const DownsampleInfo_t& downsampleInfo, FloatBitMap_t *pDestBitmap )
{
	FloatBitmapResampleInfo_t info;

	info.m_nFlags = downsampleInfo.m_nFlags;
	info.m_pSrcBitmap = this;
	info.m_pDestBitmap = pDestBitmap;
	info.m_nWRatio = NumCols() / pDestBitmap->NumCols();
	info.m_nHRatio = NumRows() / pDestBitmap->NumRows();
	info.m_nDRatio = NumSlices() / pDestBitmap->NumSlices();
	info.m_nSrcWidth = NumCols();
	info.m_nSrcHeight = NumRows();
	info.m_nSrcDepth = NumSlices();
	info.m_flAlphaThreshhold = ( downsampleInfo.m_flAlphaThreshhold >= 0.0f ) ? downsampleInfo.m_flAlphaThreshhold : 0.4f;
	info.m_flAlphaHiFreqThreshhold = ( downsampleInfo.m_flAlphaHiFreqThreshhold >= 0.0f ) ? downsampleInfo.m_flAlphaHiFreqThreshhold : 0.4f;
	info.m_pAlphaResult = NULL;

	KernelInfo_t kernel;
	ImageLoader::ComputeNiceFilterKernel( info.m_nWRatio, info.m_nHRatio, info.m_nDRatio, &kernel );
	info.m_pKernel = &kernel;

	bool bIsPowerOfTwo = IsPowerOfTwo( info.m_nSrcWidth ) && IsPowerOfTwo( info.m_nSrcHeight ) && IsPowerOfTwo( info.m_nSrcDepth ); 

	KernelType_t type;
	if ( downsampleInfo.m_nFlags & DOWNSAMPLE_ALPHATEST )
	{
		int nSize = info.m_nSrcHeight * info.m_nSrcWidth * info.m_nSrcDepth * sizeof(float);
		info.m_pAlphaResult = (float*)malloc( nSize );
		memset( info.m_pAlphaResult, 0, nSize );
		type = KERNEL_ALPHATEST;
	}
	else
	{
		type = KERNEL_DEFAULT;
	}

	int nCount, nChunkSize;
	if ( info.m_nDRatio == 1 )
	{
		nCount = pDestBitmap->NumRows();
		nChunkSize = 16;
	}
	else
	{
		nCount = pDestBitmap->NumSlices();
		nChunkSize = 8;
	}

	if ( bIsPowerOfTwo )
	{
		ParallelLoopProcessChunks( sm_pFBMThreadPool, &info, 0, nCount, nChunkSize, g_KernelFuncPow2[type] );
	}
	else
	{
		ParallelLoopProcessChunks( sm_pFBMThreadPool, &info, 0, nCount, nChunkSize, g_KernelFunc[type] );
	}

	if ( info.m_pAlphaResult )
	{
		free( info.m_pAlphaResult );
	}
	ImageLoader::CleanupNiceFilterKernel( &kernel );
}


Vector FloatBitMap_t::AverageColor( void )
{
	Vector ret( 0, 0, 0 );
	for( int y = 0; y < NumRows(); y++ )
		for( int x = 0; x < NumCols(); x++ )
			for( int c = 0; c < 3; c++ )
				ret[c]+= Pixel( x, y, 0, c );
	ret *= 1.0 / ( NumCols() * NumRows() );
	return ret;
}

float FloatBitMap_t::BrightestColor( void )
{
	float ret = 0.0;
	for( int y = 0; y < NumRows(); y++ )
		for( int x = 0; x < NumCols(); x++ )
		{
			Vector v( Pixel( x, y, 0, 0 ), Pixel( x, y, 0, 1 ), Pixel( x, y, 0, 2 ) );
			ret = MAX( ret, v.Length() );
		}
		return ret;
}

template < class T > static inline void SWAP( T & a, T & b )
{
	T temp = a;
	a = b;
	b = temp;
}

void FloatBitMap_t::RaiseToPower( float power )
{
	for( int y = 0; y < NumRows(); y++ )
		for( int x = 0; x < NumCols(); x++ )
			for( int c = 0; c < 3; c++ )
				Pixel( x, y, 0, c ) = pow( ( float )MAX( 0.0, Pixel( x, y, 0, c ) ), ( float )power );

}

void FloatBitMap_t::Logize( void )
{
	for( int y = 0; y < NumRows(); y++ )
		for( int x = 0; x < NumCols(); x++ )
			for( int c = 0; c < 3; c++ )
				Pixel( x, y, 0, c ) = log( 1.0 + Pixel( x, y, 0, c ) );

}

void FloatBitMap_t::UnLogize( void )
{
	for( int y = 0; y < NumRows(); y++ )
		for( int x = 0; x < NumCols(); x++ )
			for( int c = 0; c < 3; c++ )
				Pixel( x, y, 0, c ) = exp( Pixel( x, y, 0, c ) ) - 1;
}


void FloatBitMap_t::Clear( float r, float g, float b, float a )
{
	for ( int z = 0; z < NumSlices(); ++z )
	{
		for( int y = 0; y < NumRows(); y++ )
		{
			for( int x = 0; x < NumCols(); x++ )
			{
				Pixel( x, y, z, 0 ) = r;
				Pixel( x, y, z, 1 ) = g;
				Pixel( x, y, z, 2 ) = b;
				Pixel( x, y, z, 3 ) = a;
			}
		}
	}
}

void FloatBitMap_t::ScaleRGB( float scale_factor )
{
	for( int y = 0; y < NumRows(); y++ )
		for( int x = 0; x < NumCols(); x++ )
			for( int c = 0; c < 3; c++ )
				Pixel( x, y, 0, c ) *= scale_factor;
}

static int dx[4]={0, - 1, 1, 0};
static int dy[4]={- 1, 0, 0, 1};

#define NDELTAS 4

void FloatBitMap_t::SmartPaste( const FloatBitMap_t & b, int xofs, int yofs, uint32 Flags )
{
	// now, need to make Difference map
	FloatBitMap_t DiffMap0( this );
	FloatBitMap_t DiffMap1( this );
	FloatBitMap_t DiffMap2( this );
	FloatBitMap_t DiffMap3( this );
	FloatBitMap_t * deltas[4]={& DiffMap0, & DiffMap1, & DiffMap2, & DiffMap3};
	for( int x = 0; x < NumCols(); x++ )
		for( int y = 0; y < NumRows(); y++ )
			for( int c = 0; c < 3; c++ )
			{
				for( int i = 0; i < NDELTAS; i++ )
				{
					int x1 = x + dx[i];
					int y1 = y + dy[i];
					x1 = MAX( 0, x1 );
					x1 = MIN( NumCols() - 1, x1 );
					y1 = MAX( 0, y1 );
					y1 = MIN( NumRows() - 1, y1 );
					float dx1 = Pixel( x, y, 0, c ) - Pixel( x1, y1, 0, c );
					deltas[i]-> Pixel( x, y, 0, c ) = dx1;
				}
			}
			for( int x = 1; x < b.NumCols() - 1; x++ )
				for( int y = 1; y < b.NumRows() - 1; y++ )
					for( int c = 0; c < 3; c++ )
					{
						for( int i = 0; i < NDELTAS; i++ )
						{
							float diff = b.Pixel( x, y, 0, c ) - b.Pixel( x + dx[i], y + dy[i], 0, c );
							deltas[i]-> Pixel( x + xofs, y + yofs, 0, c ) = diff;
							if ( Flags & SPFLAGS_MAXGRADIENT )
							{
								float dx1 = Pixel( x + xofs, y + yofs, 0, c ) - Pixel( x + dx[i]+ xofs, y + dy[i]+ yofs, 0, c );
								if ( fabs( dx1 ) > fabs( diff ) )
									deltas[i]-> Pixel( x + xofs, y + yofs, 0, c ) = dx1;
							}
						}
					}

					// now, calculate modifiability
					for( int x = 0; x < NumCols(); x++ )
						for( int y = 0; y < NumRows(); y++ )
						{
							float modify = 0;
							if (
								( x > xofs + 1 ) && ( x <= xofs + b.NumCols() - 2 ) &&
								( y > yofs + 1 ) && ( y <= yofs + b.NumRows() - 2 ) )
								modify = 1;
							Alpha( x, y, 0 ) = modify;
						}

						//   // now, force a fex pixels in center to be constant
						//   int midx=xofs+b.Width/2;
						//   int midy=yofs+b.Height/2;
						//   for(x=midx-10;x<midx+10;x++)
						//     for(int y=midy-10;y<midy+10;y++)
						//     {
						//       Alpha(x,y)=0;
						//       for(int c=0;c<3;c++)
						//         Pixel(x,y,c)=b.Pixel(x-xofs,y-yofs,c);
						//     }
						Poisson( deltas, 6000, Flags );
}

void FloatBitMap_t::ScaleGradients( void )
{
	// now, need to make Difference map
	FloatBitMap_t DiffMap0( this );
	FloatBitMap_t DiffMap1( this );
	FloatBitMap_t DiffMap2( this );
	FloatBitMap_t DiffMap3( this );
	FloatBitMap_t * deltas[4]={& DiffMap0, & DiffMap1, & DiffMap2, & DiffMap3};
	double gsum = 0.0;
	for( int x = 0; x < NumCols(); x++ )
		for( int y = 0; y < NumRows(); y++ )
			for( int c = 0; c < 3; c++ )
			{
				for( int i = 0; i < NDELTAS; i++ )
				{
					int x1 = x + dx[i];
					int y1 = y + dy[i];
					x1 = MAX( 0, x1 );
					x1 = MIN( NumCols() - 1, x1 );
					y1 = MAX( 0, y1 );
					y1 = MIN( NumRows() - 1, y1 );
					float dx1 = Pixel( x, y, 0, c ) - Pixel( x1, y1, 0, c );
					deltas[i]-> Pixel( x, y, 0, c ) = dx1;
					gsum += fabs( dx1 );
				}
			}
			// now, reduce gradient changes
			//  float gavg=gsum/(NumCols()*NumRows());
			for( int x = 0; x < NumCols(); x++ )
				for( int y = 0; y < NumRows(); y++ )
					for( int c = 0; c < 3; c++ )
					{
						for( int i = 0; i < NDELTAS; i++ )
						{
							float norml = 1.1 * deltas[i]-> Pixel( x, y, 0, c );
							//           if (norml<0.0)
							//             norml=-pow(-norml,1.2);
							//           else
							//             norml=pow(norml,1.2);
							deltas[i]-> Pixel( x, y, 0, c ) = norml;
						}
					}

					// now, calculate modifiability
					for( int x = 0; x < NumCols(); x++ )
						for( int y = 0; y < NumRows(); y++ )
						{
							float modify = 0;
							if (
								( x > 0 ) && ( x < NumCols() - 1 ) &&
								( y ) && ( y < NumRows() - 1 ) )
							{
								modify = 1;
								Alpha( x, y, 0 ) = modify;
							}
						}

						Poisson( deltas, 2200, 0 );
}



static inline float FLerp( float f1, float f2, float t )
{
	return f1 + ( f2 - f1 ) * t;
}

void FloatBitMap_t::MakeTileable( void )
{
	FloatBitMap_t rslta( this );
	// now, need to make Difference map
	FloatBitMap_t DiffMapX( this );
	FloatBitMap_t DiffMapY( this );
	// set each pixel=avg-pixel
	FloatBitMap_t * cursrc =& rslta;
	for( int x = 1; x < NumCols() - 1; x++ )
		for( int y = 1; y < NumRows() - 1; y++ )
			for( int c = 0; c < 3; c++ )
			{
				DiffMapX.Pixel( x, y, 0, c ) = Pixel( x, y, 0, c ) - Pixel( x + 1, y, 0, c );
				DiffMapY.Pixel( x, y, 0, c ) = Pixel( x, y, 0, c ) - Pixel( x, y + 1, 0, c );
			}
			// initialize edge conditions
			for( int x = 0; x < NumCols(); x++ )
			{
				for( int c = 0; c < 3; c++ )
				{
					float a = 0.5 * ( Pixel( x, NumRows() - 1, 0, c ) += Pixel( x, 0, 0, c ) );
					rslta.Pixel( x, NumRows() - 1, 0, c ) = a;
					rslta.Pixel( x, 0, 0, c ) = a;
				}
			}
			for( int y = 0; y < NumRows(); y++ )
			{
				for( int c = 0; c < 3; c++ )
				{
					float a = 0.5 * ( Pixel( NumCols() - 1, y, 0, c ) + Pixel( 0, y, 0, c ) );
					rslta.Pixel( NumCols() - 1, y, 0, c ) = a;
					rslta.Pixel( 0, y, 0, c ) = a;
				}
			}
			FloatBitMap_t rsltb( & rslta );
			FloatBitMap_t * curdst =& rsltb;

			// now, ready to iterate
			for( int pass = 0; pass < 10; pass++ )
			{
				float error = 0.0;
				for( int x = 1; x < NumCols() - 1; x++ )
					for( int y = 1; y < NumRows() - 1; y++ )
						for( int c = 0; c < 3; c++ )
						{
							float desiredx = DiffMapX.Pixel( x, y, 0, c ) + cursrc->Pixel( x + 1, y, 0, c );
							float desiredy = DiffMapY.Pixel( x, y, 0, c ) + cursrc->Pixel( x, y + 1, 0, c );
							float desired = 0.5 * ( desiredy + desiredx );
							curdst->Pixel( x, y, 0, c ) = FLerp( cursrc->Pixel( x, y, 0, c ), desired, 0.5 );
							error += SQ( desired - cursrc->Pixel( x, y, 0, c ) );
						}
						SWAP( cursrc, curdst );
			}
			// paste result
			for( int x = 0; x < NumCols(); x++ )
				for( int y = 0; y < NumRows(); y++ )
					for( int c = 0; c < 3; c++ )
						Pixel( x, y, 0, c ) = curdst->Pixel( x, y, 0, c );
}


void FloatBitMap_t::GetAlphaBounds( int & minx, int & miny, int & maxx, int & maxy )
{
	for( minx = 0; minx < NumCols(); minx++ )
	{
		int y;
		for( y = 0; y < NumRows(); y++ )
			if ( Alpha( minx, y, 0 ) )
				break;
		if ( y != NumRows() )
			break;
	}
	for( maxx = NumCols() - 1; maxx >= 0; maxx-- )
	{
		int y;
		for( y = 0; y < NumRows(); y++ )
			if ( Alpha( maxx, y, 0 ) )
				break;
		if ( y != NumRows() )
			break;
	}
	for( miny = 0; minx < NumRows(); miny++ )
	{
		int x;
		for( x = minx; x <= maxx; x++ )
			if ( Alpha( x, miny, 0 ) )
				break;
		if ( x < maxx )
			break;
	}
	for( maxy = NumRows() - 1; maxy >= 0; maxy-- )
	{
		int x;
		for( x = minx; x <= maxx; x++ )
			if ( Alpha( x, maxy, 0 ) )
				break;
		if ( x < maxx )
			break;
	}
}

void FloatBitMap_t::Poisson( FloatBitMap_t * deltas[4],
							int n_iters,
							uint32 flags                                  // SPF_xxx
							)
{
	int minx, miny, maxx, maxy;
	GetAlphaBounds( minx, miny, maxx, maxy );
	minx = MAX( 1, minx );
	miny = MAX( 1, miny );
	maxx = MIN( NumCols() - 2, maxx );
	maxy = MIN( NumRows() - 2, maxy );
	if ( ( ( maxx - minx ) > 25 ) && ( maxy - miny ) > 25 )
	{
		// perform at low resolution
		FloatBitMap_t * lowdeltas[NDELTAS];
		for( int i = 0; i < NDELTAS; i++ )
		{
			lowdeltas[i] = new FloatBitMap_t;
			deltas[i]->QuarterSize( lowdeltas[i] );
		}
		FloatBitMap_t * tmp = new FloatBitMap_t;
		QuarterSize( tmp );
		tmp->Poisson( lowdeltas, n_iters * 4, flags );
		// now, propagate results from tmp to us
		for( int x = 0; x < tmp->NumCols(); x++ )
			for( int y = 0; y < tmp->NumRows(); y++ )
				for( int xi = 0; xi < 2; xi++ )
					for( int yi = 0; yi < 2; yi++ )
						if ( Alpha( x * 2 + xi, y * 2 + yi, 0 ) )
						{
							for( int c = 0; c < 3; c++ )
								Pixel( x * 2 + xi, y * 2 + yi, 0, c ) =
								FLerp( Pixel( x * 2 + xi, y * 2 + yi, 0, c ), tmp->Pixel( x, y, 0, c ), Alpha( x * 2 + xi, y * 2 + yi, 0 ) );
						}
						char fname[80];
						sprintf(fname,"sub%dx%d.tga",tmp->NumCols(),tmp->NumRows());
						tmp->WriteTGAFile( fname );
						sprintf(fname,"submrg%dx%d.tga",tmp->NumCols(),tmp->NumRows());
						WriteTGAFile( fname );
						delete tmp;
						for( int i = 0; i < NDELTAS; i++ )
							delete lowdeltas[i];
	}
	FloatBitMap_t work1( this );
	FloatBitMap_t work2( this );
	FloatBitMap_t * curdst =& work1;
	FloatBitMap_t * cursrc =& work2;
	// now, ready to iterate
	while( n_iters-- )
	{
		float error = 0.0;
		for( int x = minx; x <= maxx; x++ )
		{
			for( int y = miny; y <= maxy; y++ )
			{
				if ( Alpha( x, y, 0 ) )
				{
					for( int c = 0; c < 3; c++ )
					{
						float desired = 0.0;
						for( int i = 0; i < NDELTAS; i++ )
							desired += deltas[i]-> Pixel( x, y, 0, c ) + cursrc->Pixel( x + dx[i], y + dy[i], 0, c );
						desired *= ( 1.0 / NDELTAS );
						//            desired=FLerp(Pixel(x,y,c),desired,Alpha(x,y));
						curdst->Pixel( x, y, 0, c ) = FLerp( cursrc->Pixel( x, y, 0, c ), desired, 0.5 );
						error += SQ( desired - cursrc->Pixel( x, y, 0, c ) );
					}
				}
				SWAP( cursrc, curdst );
			}
		}
	}
	// paste result
	for( int x = 0; x < NumCols(); x++ )
	{
		for( int y = 0; y < NumRows(); y++ )
		{
			for( int c = 0; c < 3; c++ )
			{
				Pixel( x, y, 0, c ) = curdst->Pixel( x, y, 0, c );
			}
		}
	}
}


