//======= Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//=============================================================================//

#if defined( _WIN32 ) && !defined( _X360 ) && !defined( DX_TO_GL_ABSTRACTION )
#include <windows.h>
#include "../dx9sdk/include/d3d9types.h"
#include "dx11sdk/d3d11.h"
#endif
#include "bitmap/imageformat.h"
#include "basetypes.h"
#include "tier0/dbg.h"
#ifndef _PS3
#include <malloc.h>
#include <memory.h>
#else
#include <stdlib.h>
#endif
#include "nvtc.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "tier1/utlmemory.h"
#include "tier1/strtools.h"
#include "mathlib/compressed_vector.h"

// Should be last include
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Various important function types for each color format
//-----------------------------------------------------------------------------
static ImageFormatInfo_t g_ImageFormatInfo[] =
{
	{ "UNKNOWN",					0, 0, 0, 0, 0, 0, 0, false, false, false },		// IMAGE_FORMAT_UNKNOWN,
	{ "RGBA8888",					4, 8, 8, 8, 8, 0, 0, false, false, false },		// IMAGE_FORMAT_RGBA8888,
	{ "ABGR8888",					4, 8, 8, 8, 8, 0, 0, false, false, false },		// IMAGE_FORMAT_ABGR8888, 
	{ "RGB888",						3, 8, 8, 8, 0, 0, 0, false, false, false },		// IMAGE_FORMAT_RGB888,
	{ "BGR888",						3, 8, 8, 8, 0, 0, 0, false, false, false },		// IMAGE_FORMAT_BGR888,
	{ "RGB565",						2, 5, 6, 5, 0, 0, 0, false, false, false },		// IMAGE_FORMAT_RGB565, 
	{ "I8",							1, 0, 0, 0, 0, 0, 0, false, false, false },		// IMAGE_FORMAT_I8,
	{ "IA88",						2, 0, 0, 0, 8, 0, 0, false, false, false },		// IMAGE_FORMAT_IA88
	{ "P8",							1, 0, 0, 0, 0, 0, 0, false, false, false },		// IMAGE_FORMAT_P8
	{ "A8",							1, 0, 0, 0, 8, 0, 0, false, false, false },		// IMAGE_FORMAT_A8
	{ "RGB888_BLUESCREEN",			3, 8, 8, 8, 0, 0, 0, false, false, false },		// IMAGE_FORMAT_RGB888_BLUESCREEN
	{ "BGR888_BLUESCREEN",			3, 8, 8, 8, 0, 0, 0, false, false, false },		// IMAGE_FORMAT_BGR888_BLUESCREEN
	{ "ARGB8888",					4, 8, 8, 8, 8, 0, 0, false, false, false },		// IMAGE_FORMAT_ARGB8888
	{ "BGRA8888",					4, 8, 8, 8, 8, 0, 0, false, false, false },		// IMAGE_FORMAT_BGRA8888
	{ "DXT1",						0, 0, 0, 0, 0, 0, 0, true, false, false },		// IMAGE_FORMAT_DXT1
	{ "DXT3",						0, 0, 0, 0, 8, 0, 0, true, false, false },		// IMAGE_FORMAT_DXT3
	{ "DXT5",						0, 0, 0, 0, 8, 0, 0, true, false, false },		// IMAGE_FORMAT_DXT5
	{ "BGRX8888",					4, 8, 8, 8, 0, 0, 0, false, false, false },		// IMAGE_FORMAT_BGRX8888
	{ "BGR565",						2, 5, 6, 5, 0, 0, 0, false, false, false },		// IMAGE_FORMAT_BGR565
	{ "BGRX5551",					2, 5, 5, 5, 0, 0, 0, false, false, false },		// IMAGE_FORMAT_BGRX5551
	{ "BGRA4444",					2, 4, 4, 4, 4, 0, 0, false, false, false },		// IMAGE_FORMAT_BGRA4444
	{ "DXT1_ONEBITALPHA",			0, 0, 0, 0, 0, 0, 0, true, false, false },		// IMAGE_FORMAT_DXT1_ONEBITALPHA
	{ "BGRA5551",					2, 5, 5, 5, 1, 0, 0, false, false, false },		// IMAGE_FORMAT_BGRA5551
	{ "UV88",						2, 8, 8, 0, 0, 0, 0, false, false, false },		// IMAGE_FORMAT_UV88
	{ "UVWQ8888",					4, 8, 8, 8, 8, 0, 0, false, false, false },		// IMAGE_FORMAT_UVWQ8888
	{ "RGBA16161616F",				8, 16, 16, 16, 16, 0, 0, false, true, false },	// IMAGE_FORMAT_RGBA16161616F
	{ "RGBA16161616",				8, 16, 16, 16, 16, 0, 0, false, false, false },	// IMAGE_FORMAT_RGBA16161616
	{ "UVLX8888",					4, 8, 8, 8, 8, 0, 0, false, false, false },		// IMAGE_FORMAT_UVLX8888
	{ "R32F",						4, 32, 0, 0, 0, 0, 0, false, true, false },		// IMAGE_FORMAT_R32F
	{ "RGB323232F",					12, 32, 32, 32, 0, 0, 0, false, true, false },	// IMAGE_FORMAT_RGB323232F
	{ "RGBA32323232F",				16, 32, 32, 32, 32, 0, 0, false, true, false },	// IMAGE_FORMAT_RGBA32323232F
	{ "RG1616F",					4, 16, 16, 0, 0, 0, 0, false, true, false },	// IMAGE_FORMAT_RG1616F
	{ "RG3232F",					8, 32, 32, 0, 0, 0, 0, false, true, false },	// IMAGE_FORMAT_RG3232F
	{ "RGBX8888",					4, 8, 8, 8, 0, 0, 0, false, false, false },		// IMAGE_FORMAT_RGBX8888

	{ "NV_NULL",					4,  8, 8, 8, 8, 0, 0, false, false, false },	// IMAGE_FORMAT_NV_NULL

	// Vendor-dependent compressed formats typically used for normal map compression
	{ "ATI1N",						0, 0, 0, 0, 0, 0, 0, true, false, false },		// IMAGE_FORMAT_ATI1N
	{ "ATI2N",						0, 0, 0, 0, 0, 0, 0, true, false, false },		// IMAGE_FORMAT_ATI2N

	{ "RGBA1010102",				4, 10, 10, 10, 2, 0, 0, false, false, false },	// IMAGE_FORMAT_RGBA1010102
	{ "BGRA1010102",				4, 10, 10, 10, 2, 0, 0, false, false, false },	// IMAGE_FORMAT_BGRA1010102
	{ "R16F",						2, 16, 0, 0, 0, 0, 0, false, false, false },    // IMAGE_FORMAT_R16F

	// Vendor-dependent depth formats used for shadow depth mapping
	{ "D16",						2, 0, 0, 0, 0, 16, 0, false, false, true },		// IMAGE_FORMAT_D16
	{ "D15S1",						2, 0, 0, 0, 0, 15, 1, false, false, true },		// IMAGE_FORMAT_D15S1
	{ "D32",						4, 0, 0, 0, 0, 32, 0, false, false, true },		// IMAGE_FORMAT_D32
	{ "D24S8",						4, 0, 0, 0, 0, 24, 8, false, false, true },		// IMAGE_FORMAT_D24S8
	{ "LINEAR_D24S8",				4, 0, 0, 0, 0, 24, 8, false, false, true },		// IMAGE_FORMAT_LINEAR_D24S8
	{ "D24X8",						4, 0, 0, 0, 0, 24, 0, false, false, true },		// IMAGE_FORMAT_D24X8
	{ "D24X4S4",					4, 0, 0, 0, 0, 24, 4, false, false, true },		// IMAGE_FORMAT_D24X4S4
	{ "D24FS8",						4, 0, 0, 0, 0, 24, 8, false, false, true },		// IMAGE_FORMAT_D24FS8
	{ "D16_SHADOW",					2, 0, 0, 0, 0, 16, 0, false, false, true },		// IMAGE_FORMAT_D16_SHADOW
	{ "D24X8_SHADOW",				4, 0, 0, 0, 0, 24, 0, false, false, true },		// IMAGE_FORMAT_D24X8_SHADOW

	{ "LINEAR_BGRX8888",			4, 8, 8, 8, 8, 0, 0, false, false, false },		// IMAGE_FORMAT_LINEAR_BGRX8888
	{ "LINEAR_RGBA8888",			4, 8, 8, 8, 8, 0, 0, false, false, false },		// IMAGE_FORMAT_LINEAR_RGBA8888
	{ "LINEAR_ABGR8888",			4, 8, 8, 8, 8, 0, 0, false, false, false },		// IMAGE_FORMAT_LINEAR_ABGR8888
	{ "LINEAR_ARGB8888",			4, 8, 8, 8, 8, 0, 0, false, false, false },		// IMAGE_FORMAT_LINEAR_ARGB8888
	{ "LINEAR_BGRA8888",			4, 8, 8, 8, 8, 0, 0, false, false, false },		// IMAGE_FORMAT_LINEAR_BGRA8888
	{ "LINEAR_RGB888",				3, 8, 8, 8, 0, 0, 0, false, false, false },		// IMAGE_FORMAT_LINEAR_RGB888
	{ "LINEAR_BGR888",				3, 8, 8, 8, 0, 0, 0, false, false, false },		// IMAGE_FORMAT_LINEAR_BGR888
	{ "LINEAR_BGRX5551",			2, 5, 5, 5, 0, 0, 0, false, false, false },		// IMAGE_FORMAT_LINEAR_BGRX5551
	{ "LINEAR_I8",					1, 0, 0, 0, 0, 0, 0, false, false, false },		// IMAGE_FORMAT_LINEAR_I8
	{ "LINEAR_RGBA16161616",		8, 16, 16, 16, 16, 0, 0, false, false, false },	// IMAGE_FORMAT_LINEAR_RGBA16161616
	{ "LINEAR_A8",					1, 0, 0, 0, 8, 0, 0, false, false, false },		// IMAGE_FORMAT_LINEAR_A8
	{ "LINEAR_DXT1",				0, 0, 0, 0, 0, 0, 0, true, false, false },		// IMAGE_FORMAT_LINEAR_DXT1
	{ "LINEAR_DXT3",				0, 0, 0, 0, 8, 0, 0, true, false, false },		// IMAGE_FORMAT_LINEAR_DXT3
	{ "LINEAR_DXT5",				0, 0, 0, 0, 8, 0, 0, true, false, false },		// IMAGE_FORMAT_LINEAR_DXT5

	{ "LE_BGRX8888",				4, 8, 8, 8, 8, 0, 0, false, false, false },		// IMAGE_FORMAT_LE_BGRX8888
	{ "LE_BGRA8888",				4, 8, 8, 8, 8, 0, 0, false, false, false },		// IMAGE_FORMAT_LE_BGRA8888

	{ "DXT1_RUNTIME",				0, 0, 0, 0, 0, 0, 0, true, false, false },		// IMAGE_FORMAT_DXT1_RUNTIME
	{ "DXT5_RUNTIME",				0, 0, 0, 0, 8, 0, 0, true, false, false },		// IMAGE_FORMAT_DXT5_RUNTIME

	// Vendor-dependent depth formats used for resolving
	{ "INTZ",						4, 0, 0, 0, 0, 24, 8, false, false, true},		// IMAGE_FORMAT_INTZ
};


namespace ImageLoader
{

//-----------------------------------------------------------------------------
// Returns info about each image format
//-----------------------------------------------------------------------------
const ImageFormatInfo_t& ImageFormatInfo( ImageFormat fmt )
{
	COMPILE_TIME_ASSERT( ( NUM_IMAGE_FORMATS + 1 ) == ARRAYSIZE( g_ImageFormatInfo ) );
	Assert( unsigned( fmt + 1 ) <= ( NUM_IMAGE_FORMATS ) );
	return g_ImageFormatInfo[ fmt + 1 ];
}

int GetMemRequired( int width, int height, int depth, int nMipmapCount, ImageFormat imageFormat, int *pAdjustedHeight )
{
	depth = MAX( 1, depth );

	int nRet = 0;
	if ( nMipmapCount == 1 )
	{
		// Block compressed formats
		const ImageFormatInfo_t &fmt = ImageFormatInfo( imageFormat );
		if ( fmt.m_bIsCompressed )
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
			width >>= 2;
			height >>= 2;

			int numBlocks = width * height * depth;
			switch ( imageFormat )
			{
			case IMAGE_FORMAT_DXT1:
			case IMAGE_FORMAT_DXT1_RUNTIME:
			case IMAGE_FORMAT_LINEAR_DXT1:
			case IMAGE_FORMAT_ATI1N:
				nRet = numBlocks * 8;
				break;

			case IMAGE_FORMAT_DXT3:
			case IMAGE_FORMAT_DXT5:
			case IMAGE_FORMAT_DXT5_RUNTIME:
			case IMAGE_FORMAT_LINEAR_DXT3:
			case IMAGE_FORMAT_LINEAR_DXT5:
			case IMAGE_FORMAT_ATI2N:
				nRet = numBlocks * 16;
				break;
			}
		}
		else
		{
			nRet = width * height * depth * fmt.m_nNumBytes;
		}
		if ( pAdjustedHeight )
		{
			*pAdjustedHeight = height;
		}
		return nRet;
	}

	// Mipmap version
	int memSize = 0;

	// Not sensical for mip chains
	if ( pAdjustedHeight )
	{
		*pAdjustedHeight = 0;
	}
	while ( true )
	{
		memSize += GetMemRequired( width, height, depth, imageFormat, false );
		if ( width == 1 && height == 1 && depth == 1 )
			break;

		width >>= 1;
		height >>= 1;
		depth >>= 1;
		if ( width < 1 )
		{
			width = 1;
		}
		if ( height < 1 )
		{
			height = 1;
		}
		if ( depth < 1 )
		{
			depth = 1;
		}
		if ( nMipmapCount )
		{
			if ( --nMipmapCount == 0 )
				break;
		}
	}

	return memSize;
}

int GetMemRequired( int width, int height, int depth, ImageFormat imageFormat, bool mipmap, int *pAdjustedHeight )
{
	return GetMemRequired( width, height, depth, mipmap ? 0 : 1, imageFormat, pAdjustedHeight );
}

int GetMipMapLevelByteOffset( int width, int height, ImageFormat imageFormat, int skipMipLevels, int nDepth )
{
	int offset = 0;

	while( skipMipLevels > 0 )
	{
		offset += GetMemRequired( width, height, nDepth, 1, imageFormat );
		if( width == 1 && height == 1 && nDepth == 1 )
		{
			break;
		}
		width = MAX( 1, width >> 1 );
		height = MAX( 1, height >> 1 );
		nDepth = MAX( 1, nDepth >> 1 );
		skipMipLevels--;
	}
	return offset;
}


//-----------------------------------------------------------------------------
// This version is for mipmaps which are stored smallest level to largest level in memory
//-----------------------------------------------------------------------------
int GetMipMapLevelByteOffsetReverse( int nWidth, int nHeight, int nDepth, int nTotalMipCount, ImageFormat imageFormat, int nMipLevel )
{
	if ( nTotalMipCount == 1 )
		return 0;

	int nSkipSize = 0;
	for ( int i = 0; i < nTotalMipCount; ++i )
	{
		int nMipSize = GetMemRequired( nWidth, nHeight, nDepth, 1, imageFormat );
		if ( i > nMipLevel )
		{
			nSkipSize += nMipSize;
		}
		if( nWidth == 1 && nHeight == 1 && nDepth == 1 )
			break;
		nWidth = MAX( 1, nWidth >> 1 );
		nHeight = MAX( 1, nHeight >> 1 );
		nDepth = MAX( 1, nDepth >> 1 );
	}
	return nSkipSize;
}



void GetMipMapLevelDimensions( int *width, int *height, int skipMipLevels )
{
	while( skipMipLevels > 0 )
	{
		if( *width == 1 && *height == 1 )
		{
			break;
		}
		*width >>= 1;
		*height >>= 1;
		if( *width < 1 )
		{
			*width = 1;
		}
		if( *height < 1 )
		{
			*height = 1;
		}
		skipMipLevels--;
	}
}

void GetMipMapLevelDimensions( int &nWidth, int &nHeight, int &nDepth, int nMipLevel )
{
	for( ; nMipLevel > 0; --nMipLevel )
	{
		if( nWidth <= 1 && nHeight <= 1 && nDepth <= 1 )
			break;
		nWidth >>= 1;
		nHeight >>= 1;
		nDepth >>= 1;
	}

	nWidth = MAX( nWidth, 1 );
	nHeight = MAX( nHeight, 1 );
	nDepth = MAX( nDepth, 1 );
}


int GetNumMipMapLevels( int width, int height, int depth )
{
	if ( depth <= 0 )
	{
		depth = 1;
	}

	if( width < 1 || height < 1 || depth < 1 )
		return 0;

	int numMipLevels = 1;
	while( 1 )
	{
		if( width == 1 && height == 1 && depth == 1 )
			break;

		width >>= 1;
		height >>= 1;
		depth >>= 1;
		if( width < 1 )
		{
			width = 1;
		}
		if( height < 1 )
		{
			height = 1;
		}
		if( depth < 1 )
		{
			depth = 1;
		}
		numMipLevels++;
	}
	return numMipLevels;
}

// Turn off warning about FOURCC formats below...
#pragma warning (disable:4063)

#ifdef POSIX
#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
	((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |   \
	((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))
#endif //defined(MAKEFOURCC)
#endif	
//-----------------------------------------------------------------------------
// convert back and forth from D3D format to ImageFormat, regardless of
// whether it's supported or not
//-----------------------------------------------------------------------------
ImageFormat D3DFormatToImageFormat( D3DFORMAT format )
{
#if defined( PLATFORM_X360 )
	if ( IS_D3DFORMAT_SRGB( format ) )
	{
		// sanitize the format from possible sRGB state for comparison purposes
		format = MAKE_NON_SRGB_FMT( format );
	}
#endif

	switch ( format )
	{
#if !defined( PLATFORM_X360 )
	case D3DFMT_R8G8B8:
		return IMAGE_FORMAT_BGR888;
#endif
#ifndef POSIX
	case D3DFMT_A8B8G8R8:
		return IMAGE_FORMAT_RGBA8888;
	case D3DFMT_X8B8G8R8:
		return IMAGE_FORMAT_RGBX8888;
#endif // !POSIX
	case D3DFMT_A8R8G8B8:
		return IMAGE_FORMAT_BGRA8888;
	case D3DFMT_X8R8G8B8:
		return IMAGE_FORMAT_BGRX8888;
	case D3DFMT_R5G6B5:
		return IMAGE_FORMAT_BGR565;
	case D3DFMT_X1R5G5B5:
		return IMAGE_FORMAT_BGRX5551;
	case D3DFMT_A1R5G5B5:
		return IMAGE_FORMAT_BGRA5551;
	case D3DFMT_A4R4G4B4:
		return IMAGE_FORMAT_BGRA4444;
#if !defined( PLATFORM_X360 ) && !defined( POSIX )
	case D3DFMT_P8:
		return IMAGE_FORMAT_P8;
#endif
	case D3DFMT_L8:
		return IMAGE_FORMAT_I8;
	case D3DFMT_A8L8:
		return IMAGE_FORMAT_IA88;
	case D3DFMT_A8:
		return IMAGE_FORMAT_A8;
	case D3DFMT_DXT1:
		return IMAGE_FORMAT_DXT1;
	case D3DFMT_DXT3:
		return IMAGE_FORMAT_DXT3;
	case D3DFMT_DXT5:
		return IMAGE_FORMAT_DXT5;
	case D3DFMT_V8U8:
		return IMAGE_FORMAT_UV88;
	case D3DFMT_Q8W8V8U8:
		return IMAGE_FORMAT_UVWQ8888;
	case D3DFMT_X8L8V8U8:
		return IMAGE_FORMAT_UVLX8888;
	case D3DFMT_A16B16G16R16F:
		return IMAGE_FORMAT_RGBA16161616F;
	case D3DFMT_A16B16G16R16:
		return IMAGE_FORMAT_RGBA16161616;
	case D3DFMT_R32F:
		return IMAGE_FORMAT_R32F;
	case D3DFMT_A32B32G32R32F:
		return IMAGE_FORMAT_RGBA32323232F;
	case (D3DFORMAT)(MAKEFOURCC('N','U','L','L')):
		return IMAGE_FORMAT_NULL;
	case D3DFMT_D16:
		return IMAGE_FORMAT_D16;
#ifndef POSIX
	case D3DFMT_G16R16F:
		return IMAGE_FORMAT_RG1616F;
	case D3DFMT_G32R32F:
		return IMAGE_FORMAT_RG3232F;
#endif // !POSIX
	case D3DFMT_D24S8:
		return IMAGE_FORMAT_D24S8;
	case (D3DFORMAT)(MAKEFOURCC('A','T','I','1')):
		return IMAGE_FORMAT_ATI1N;
	case (D3DFORMAT)(MAKEFOURCC('A','T','I','2')):
		return IMAGE_FORMAT_ATI2N; 
#ifndef POSIX
	case D3DFMT_A2B10G10R10:
		return IMAGE_FORMAT_RGBA1010102;
	case D3DFMT_A2R10G10B10:
		return IMAGE_FORMAT_BGRA1010102;
	case D3DFMT_R16F:
		return IMAGE_FORMAT_R16F;

	case D3DFMT_D32:
		return IMAGE_FORMAT_D32;

#endif // !POSIX
	case D3DFMT_D24X8:
		return IMAGE_FORMAT_D24X8;

#ifndef PLATFORM_X360
	case D3DFMT_D15S1:
		return IMAGE_FORMAT_D15S1;
	case D3DFMT_D24X4S4:
		return IMAGE_FORMAT_D24X4S4;
#endif

	case D3DFMT_UNKNOWN:
		return IMAGE_FORMAT_UNKNOWN;
#ifdef PLATFORM_X360
	case D3DFMT_LIN_A8R8G8B8:
		return IMAGE_FORMAT_LINEAR_BGRA8888;
	case D3DFMT_LIN_A8B8G8R8:
		return IMAGE_FORMAT_LINEAR_RGBA8888;
	case D3DFMT_LIN_X8R8G8B8:
		return IMAGE_FORMAT_LINEAR_BGRX8888;
	case D3DFMT_LIN_X1R5G5B5:
		return IMAGE_FORMAT_LINEAR_BGRX5551;
	case D3DFMT_LIN_L8:
		return IMAGE_FORMAT_LINEAR_I8;
	case D3DFMT_LIN_A16B16G16R16:
		return IMAGE_FORMAT_LINEAR_RGBA16161616;
	case D3DFMT_LE_X8R8G8B8:
		return IMAGE_FORMAT_LE_BGRX8888;
	case D3DFMT_LE_A8R8G8B8:
		return IMAGE_FORMAT_LE_BGRA8888;
	case D3DFMT_LIN_D24S8:
		return IMAGE_FORMAT_LINEAR_D24S8;
	case D3DFMT_LIN_A8:
		return IMAGE_FORMAT_LINEAR_A8;
	case D3DFMT_LIN_DXT1:
		return IMAGE_FORMAT_LINEAR_DXT1;
	case D3DFMT_LIN_DXT3:
		return IMAGE_FORMAT_LINEAR_DXT3;
	case D3DFMT_LIN_DXT5:
		return IMAGE_FORMAT_LINEAR_DXT5;
#endif

#if !defined( _PS3 )
	case D3DFMT_D24FS8:
		return IMAGE_FORMAT_D24FS8;
#endif // !_PS3
	}
	return IMAGE_FORMAT_UNKNOWN;
}

#ifdef _PS3

// Stub out some formats that don't have direct analgoues on PS3 or that we haven't yet mapped
#define D3DFMT_A8B8G8R8			D3DFMT_UNKNOWN
#define D3DFMT_P8				D3DFMT_UNKNOWN
#define D3DFMT_G16R16F			D3DFMT_UNKNOWN
#define D3DFMT_G32R32F			D3DFMT_UNKNOWN
#define D3DFMT_X8B8G8R8			D3DFMT_UNKNOWN
#define D3DFMT_A2B10G10R10		D3DFMT_UNKNOWN
#define D3DFMT_A2R10G10B10		D3DFMT_UNKNOWN
#define D3DFMT_R16F				D3DFMT_UNKNOWN
#define D3DFMT_D32				D3DFMT_UNKNOWN
#define D3DFMT_D24FS8			D3DFMT_UNKNOWN

#endif // _PS3

// A format exists in here only if there is a direct mapping
static D3DFORMAT s_pD3DFormats[] = 
{
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_UNKNOWN, 
	D3DFMT_A8B8G8R8,	// IMAGE_FORMAT_RGBA8888, 
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_ABGR8888, 
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_RGB888, 
#if !defined( PLATFORM_X360 )
	D3DFMT_R8G8B8,		// IMAGE_FORMAT_BGR888
#else
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_BGR888
#endif
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_RGB565, 
	D3DFMT_L8,			// IMAGE_FORMAT_I8,
	D3DFMT_A8L8,		// IMAGE_FORMAT_IA88,
#ifndef PLATFORM_X360
	D3DFMT_P8,			// IMAGE_FORMAT_P8,
#else
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_P8,
#endif
	D3DFMT_A8,			// IMAGE_FORMAT_A8,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_RGB888_BLUESCREEN,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_BGR888_BLUESCREEN,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_ARGB8888,
	D3DFMT_A8R8G8B8,	// IMAGE_FORMAT_BGRA8888,
	D3DFMT_DXT1,		// IMAGE_FORMAT_DXT1,
	D3DFMT_DXT3,		// IMAGE_FORMAT_DXT3,
	D3DFMT_DXT5,		// IMAGE_FORMAT_DXT5,
	D3DFMT_X8R8G8B8,	// IMAGE_FORMAT_BGRX8888,
	D3DFMT_R5G6B5,		// IMAGE_FORMAT_BGR565,
	D3DFMT_X1R5G5B5,	// IMAGE_FORMAT_BGRX5551,
	D3DFMT_A4R4G4B4,	// IMAGE_FORMAT_BGRA4444,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_DXT1_ONEBITALPHA,
	D3DFMT_A1R5G5B5,	// IMAGE_FORMAT_BGRA5551,
	D3DFMT_V8U8,		// IMAGE_FORMAT_UV88,
	D3DFMT_Q8W8V8U8,	// IMAGE_FORMAT_UVWQ8888,
	D3DFMT_A16B16G16R16F,	// IMAGE_FORMAT_RGBA16161616F,
	D3DFMT_A16B16G16R16,	// IMAGE_FORMAT_RGBA16161616,
	D3DFMT_X8L8V8U8,	// IMAGE_FORMAT_UVLX8888,
	D3DFMT_R32F,		// IMAGE_FORMAT_R32F,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_RGB323232F,
	D3DFMT_A32B32G32R32F,	// IMAGE_FORMAT_RGBA32323232F,
	D3DFMT_G16R16F,		// IMAGE_FORMAT_RG1616F,
	D3DFMT_G32R32F,		// IMAGE_FORMAT_RG3232F,
	D3DFMT_X8B8G8R8,	// IMAGE_FORMAT_RGBX8888,
	(D3DFORMAT)(MAKEFOURCC('N','U','L','L')),		// IMAGE_FORMAT_NULL,
	(D3DFORMAT)(MAKEFOURCC('A','T','I','2')),		// IMAGE_FORMAT_ATI2N,	
	(D3DFORMAT)(MAKEFOURCC('A','T','I','1')),		// IMAGE_FORMAT_ATI1N,
	D3DFMT_A2B10G10R10,	// IMAGE_FORMAT_RGBA1010102,
	D3DFMT_A2R10G10B10,	// IMAGE_FORMAT_BGRA1010102,
	D3DFMT_R16F,        // IMAGE_FORMAT_R16F,
	D3DFMT_D16,			// IMAGE_FORMAT_D16,

#ifndef PLATFORM_X360
	D3DFMT_D15S1,		// IMAGE_FORMAT_D15S1,
	D3DFMT_D32,			// IMAGE_FORMAT_D32,
	D3DFMT_D24S8,		// IMAGE_FORMAT_D24S8,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_LINEAR_D24S8,
	D3DFMT_D24X8,		// IMAGE_FORMAT_D24X8,
	D3DFMT_D24X4S4,		// IMAGE_FORMAT_D24X4S4,
	D3DFMT_D24FS8,		// IMAGE_FORMAT_D24FS8,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_D16_SHADOW,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_D24S8_SHADOW,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_LINEAR_BGRX8888,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_LINEAR_RGBA8888,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_LINEAR_ABGR8888,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_LINEAR_ARGB8888,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_LINEAR_BGRA8888,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_LINEAR_RGB888,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_LINEAR_BGR888,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_LINEAR_BGRX5551,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_LINEAR_I8,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_LINEAR_RGBA16161616,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_LINEAR_A8,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_LINEAR_DXT1,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_LINEAR_DXT3,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_LINEAR_DXT5,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_LE_BGRX8888,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_LE_BGRA8888,
	D3DFMT_DXT1,		// IMAGE_FORMAT_DXT5_RUNTIME,
	D3DFMT_DXT5,		// IMAGE_FORMAT_DXT5_RUNTIME,
	(D3DFORMAT)(MAKEFOURCC('I','N','T','Z')),		// IMAGE_FORMAT_INTZ,	
#else
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_D15S1,
	D3DFMT_D32,			// IMAGE_FORMAT_D32,
	D3DFMT_D24S8,		// IMAGE_FORMAT_D24S8,
	D3DFMT_LIN_D24S8,	// IMAGE_FORMAT_LINEAR_D24S8,
	D3DFMT_D24X8,		// IMAGE_FORMAT_D24X8,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_D24X4S4,
	D3DFMT_D24FS8,		// IMAGE_FORMAT_D24FS8,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_D16_SHADOW,
	D3DFMT_UNKNOWN,		// IMAGE_FORMAT_D24S8_SHADOW,
	D3DFMT_LIN_X8R8G8B8,	// IMAGE_FORMAT_LINEAR_BGRX8888,
	D3DFMT_LIN_A8B8G8R8,	// IMAGE_FORMAT_LINEAR_RGBA8888,
	D3DFMT_UNKNOWN,			// IMAGE_FORMAT_LINEAR_ABGR8888,
	D3DFMT_UNKNOWN,			// IMAGE_FORMAT_LINEAR_ARGB8888,
	D3DFMT_LIN_A8R8G8B8,	// IMAGE_FORMAT_LINEAR_BGRA8888,
	D3DFMT_UNKNOWN,			// IMAGE_FORMAT_LINEAR_RGB888,
	D3DFMT_UNKNOWN,			// IMAGE_FORMAT_LINEAR_BGR888,
	D3DFMT_LIN_X1R5G5B5,	// IMAGE_FORMAT_LINEAR_BGRX5551,
	D3DFMT_LIN_L8,			// IMAGE_FORMAT_LINEAR_I8,
	D3DFMT_LIN_A16B16G16R16,	// IMAGE_FORMAT_LINEAR_RGBA16161616,
	D3DFMT_LIN_A8,			// IMAGE_FORMAT_LINEAR_A8
	D3DFMT_LIN_DXT1,		// IMAGE_FORMAT_LINEAR_DXT1,
	D3DFMT_LIN_DXT3,		// IMAGE_FORMAT_LINEAR_DXT3,
	D3DFMT_LIN_DXT5,		// IMAGE_FORMAT_LINEAR_DXT5,
	D3DFMT_LE_X8R8G8B8,		// IMAGE_FORMAT_LE_BGRX8888,
	D3DFMT_LE_A8R8G8B8,		// IMAGE_FORMAT_LE_BGRA8888,
	D3DFMT_DXT1,		// IMAGE_FORMAT_DXT5_RUNTIME,
	D3DFMT_DXT5,		// IMAGE_FORMAT_DXT5_RUNTIME,
#endif
};

D3DFORMAT ImageFormatToD3DFormat( ImageFormat format )
{
	COMPILE_TIME_ASSERT( ARRAYSIZE( s_pD3DFormats ) == NUM_IMAGE_FORMATS + 1 );
	return s_pD3DFormats[ format + 1 ];
}

#pragma warning (default:4063)

} // ImageLoader namespace ends

