//======= Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#ifndef FLOATBITMAP_H
#define FLOATBITMAP_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier1/utlsoacontainer.h"
#include "mathlib/mathlib.h"
#include "bitmap/imageformat.h"

class IThreadPool;


struct PixRGBAF 
{
	float Red;
	float Green;
	float Blue;
	float Alpha;
};

struct PixRGBA8
{
	unsigned char Red;
	unsigned char Green;
	unsigned char Blue;
	unsigned char Alpha;
};

inline PixRGBAF PixRGBA8_to_F( PixRGBA8 const &x )
{
	PixRGBAF f;
	f.Red = x.Red / float( 255.0f );
	f.Green = x.Green / float( 255.0f );
	f.Blue = x.Blue / float( 255.0f );
	f.Alpha = x.Alpha / float( 255.0f );
	return f;
}

inline PixRGBA8 PixRGBAF_to_8( PixRGBAF const &f )
{
	PixRGBA8 x;
	x.Red = ( unsigned char )MAX( 0, MIN( 255.0,255.0*f.Red ) );
	x.Green = ( unsigned char )MAX( 0, MIN( 255.0,255.0*f.Green ) );
	x.Blue = ( unsigned char )MAX( 0, MIN( 255.0,255.0*f.Blue ) );
	x.Alpha = ( unsigned char )MAX( 0, MIN( 255.0,255.0*f.Alpha ) );
	return x;
}

#define SPFLAGS_MAXGRADIENT 1

// bit flag options for ComputeSelfShadowedBumpmapFromHeightInAlphaChannel:
#define SSBUMP_OPTION_NONDIRECTIONAL 1						// generate ambient occlusion only
#define SSBUMP_MOD2X_DETAIL_TEXTURE 2						// scale so that a flat unshadowed
                                                            // value is 0.5, and bake rgb luminance
                                                            // in.

// attributes for csoaa container
enum FBMAttribute_t
{
	FBM_ATTR_RED = 0,
	FBM_ATTR_GREEN = 1,
	FBM_ATTR_BLUE = 2,
	FBM_ATTR_ALPHA = 3,

	FBM_ATTR_COUNT
};

enum FBMAttributeMask_t
{
	FBM_ATTR_RED_MASK = ( 1 << FBM_ATTR_RED ),
	FBM_ATTR_GREEN_MASK = ( 1 << FBM_ATTR_GREEN ),
	FBM_ATTR_BLUE_MASK = ( 1 << FBM_ATTR_BLUE ),
	FBM_ATTR_ALPHA_MASK = ( 1 << FBM_ATTR_ALPHA ),

	FBM_ATTR_RGB_MASK = FBM_ATTR_RED_MASK | FBM_ATTR_GREEN_MASK | FBM_ATTR_BLUE_MASK,
	FBM_ATTR_RGBA_MASK = FBM_ATTR_RED_MASK | FBM_ATTR_GREEN_MASK | FBM_ATTR_BLUE_MASK | FBM_ATTR_ALPHA_MASK,
};


enum DownsampleFlags_t
{
	DOWNSAMPLE_CLAMPS		= 0x1,
	DOWNSAMPLE_CLAMPT		= 0x2,
	DOWNSAMPLE_CLAMPU		= 0x4,
	DOWNSAMPLE_ALPHATEST	= 0x10,
};

struct DownsampleInfo_t
{
	int m_nFlags;	// see DownsampleFlags_t
	float m_flAlphaThreshhold;
	float m_flAlphaHiFreqThreshhold;
};


//-----------------------------------------------------------------------------
// Float bitmap 
//-----------------------------------------------------------------------------
class FloatBitMap_t : public CSOAContainer
{
	typedef CSOAContainer BaseClass;

public:
	FloatBitMap_t();
	FloatBitMap_t( int nWidth, int nHeight, int nDepth = 1, int nAttributeMask = FBM_ATTR_RGBA_MASK );	// make one and allocate space
	FloatBitMap_t( const char *pFilename );					// read one from a file (tga or pfm)
	FloatBitMap_t( const FloatBitMap_t *pOrig );

	// Initialize, shutdown
	void Init( int nWidth, int nHeight, int nDepth = 1, int nAttributeMask = FBM_ATTR_RGBA_MASK );
	void Shutdown();

	// Computes the attribute mask
	int GetAttributeMask() const;

	// Does the bitmap have data for a particular component?
	bool HasAttributeData( FBMAttribute_t a ) const;

	// Compute valid attribute list
	int ComputeValidAttributeList( int pIndex[4] ) const;

	// Methods to initialize bitmap data
	bool LoadFromPFM( const char *pFilename );				// load from floating point pixmap (.pfm) file
	bool LoadFromPSD( const char *pFilename );				// load from psd file
	void InitializeWithRandomPixelsFromAnotherFloatBM( const FloatBitMap_t &other );
	void Clear( float r, float g, float b, float alpha );	// set all pixels to speicifed values (0..1 nominal)
	void LoadFromBuffer( const void *pBuffer, size_t nBufSize, ImageFormat fmt, float flGamma );	// Assumes dimensions match the bitmap size
	void LoadFromFloatBitmap( const FloatBitMap_t *pOrig );

	// Methods to write bitmap data to files
	bool WriteTGAFile( const char *pFilename ) const;
	bool WritePFM( const char *pFilename );					// save to floating point pixmap (.pfm) file
	void WriteToBuffer( void *pBuffer, size_t nBufSize, ImageFormat fmt, float flGamma ) const;	// Assumes dimensions match the bitmap size

	// Methods to read + write constant values
	const float &ConstantValue( int comp ) const;

	// Methods to read + write individual pixels
	float &Pixel( int x, int y, int z, int comp ) const;
	float &PixelWrapped( int x, int y, int z, int comp ) const;
	float &PixelClamped( int x, int y, int z, int comp ) const;
	float &Alpha( int x, int y, int z ) const;

	// look up a pixel value with bilinear interpolation
	float InterpolatedPixel( float x, float y, int comp ) const;
	float InterpolatedPixel( float x, float y, float z, int comp ) const;

	PixRGBAF PixelRGBAF( int x, int y, int z ) const;
	void WritePixelRGBAF(int x, int y, int z, PixRGBAF value) const;
	void WritePixel(int x, int y, int z, int comp, float value);

	// Method to slam a particular channel to always be the same value 
	void SetChannel( int comp, float flValue );

	// paste, performing boundary matching. Alpha channel can be used to make
	// brush shape irregular
	void SmartPaste( const FloatBitMap_t &brush, int xofs, int yofs, uint32 flags );

	// force to be tileable using poisson formula
	void MakeTileable( void );

	void ReSize( int NewXSize, int NewYSize );

	// Makes the image be a sub-range of the current image
	void Crop( int x, int y, int z, int nWidth, int nHeight, int nDepth );

	// find the bounds of the area that has non-zero alpha.
	void GetAlphaBounds( int &minx, int &miny, int &maxx, int &maxy );

	// Solve the poisson equation for an image. The alpha channel of the image controls which
	// pixels are "modifiable", and can be used to set boundary conditions. Alpha=0 means the pixel
	// is locked.  deltas are in the order [(x,y)-(x,y-1),(x,y)-(x-1,y),(x,y)-(x+1,y),(x,y)-(x,y+1)
	void Poisson( FloatBitMap_t * deltas[4],
		int n_iters,
		uint32 flags                                  // SPF_xxx
		);

	void QuarterSize( FloatBitMap_t *pBitmap );				// get a new one downsampled
	void QuarterSizeBlocky( FloatBitMap_t *pBitmap );		// get a new one downsampled
	void QuarterSizeWithGaussian( FloatBitMap_t *pBitmap );	// downsample 2x using a gaussian

	// Downsample using nice filter (NOTE: Dest bitmap needs to have been initialized w/ final size)
	void DownsampleNiceFiltered( const DownsampleInfo_t& info, FloatBitMap_t *pBitmap );

	void RaiseToPower( float pow );
	void ScaleGradients( void );
	void Logize( void );                                        // pix=log(1+pix)
	void UnLogize( void );                                      // pix=exp(pix)-1

	// compress to 8 bits converts the hdr texture to an 8 bit texture, encoding a scale factor
	// in the alpha channel. upon return, the original pixel can be (approximately) recovered
	// by the formula rgb*alpha*overbright.
	// this function performs special numerical optimization on the texture to minimize the error
	// when using bilinear filtering to read the texture.
	void CompressTo8Bits( float overbright );
	// decompress a bitmap converted by CompressTo8Bits
	void Uncompress( float overbright );


	Vector AverageColor( void );								// average rgb value of all pixels
	float BrightestColor( void );								// highest vector magnitude

	void ScaleRGB( float scale_factor );						// for all pixels, r,g,b*=scale_factor

	// given a bitmap with height stored in the alpha channel, generate vector positions and normals
	void ComputeVertexPositionsAndNormals( float flHeightScale, Vector ** ppPosOut, Vector ** ppNormalOut ) const;

	// generate a normal map with height stored in alpha.  uses hl2 tangent basis to support baked
	// self shadowing.  the bump scale maps the height of a pixel relative to the edges of the
	// pixel. This function may take a while - many millions of rays may be traced.  applications
	// using this method need to link w/ raytrace.lib
	FloatBitMap_t * ComputeSelfShadowedBumpmapFromHeightInAlphaChannel(
		float bump_scale, int nrays_to_trace_per_pixel = 100,
		uint32 nOptionFlags = 0								// SSBUMP_OPTION_XXX
		) const;


	// generate a conventional normal map from a source with height stored in alpha.
	FloatBitMap_t *ComputeBumpmapFromHeightInAlphaChannel( float bump_scale ) const ;

	// bilateral (edge preserving) smoothing filter. edge_threshold_value defines the difference in
	// values over which filtering will not occur. Each channel is filtered independently. large
	// radii will run slow, since the bilateral filter is neither separable, nor is it a
	// convolution that can be done via fft.
	void TileableBilateralFilter( int radius_in_pixels, float edge_threshold_value );

	// Sets a thread pool for all float bitmap work to be done on
	static void SetThreadPool( IThreadPool* pPool );

protected:
	void QuarterSize2D( FloatBitMap_t *pDest, int nStart, int nCount );
	void QuarterSize3D( FloatBitMap_t *pDest, int nStart, int nCount );
	void QuarterSizeBlocky2D( FloatBitMap_t *pDest, int nStart, int nCount );
	void QuarterSizeBlocky3D( FloatBitMap_t *pDest, int nStart, int nCount );

	template< class T > void LoadFromBufferRGBFloat( const T *pBuffer, int nPixelCount );
	template< class T > void LoadFromBufferRGB( const T *pBuffer, int nPixelCount, const float *pGammaTable );
	template< class T > void LoadFromBufferRGBAFloat( const T *pBuffer, int nPixelCount );
	template< class T > void LoadFromBufferRGBA( const T *pBuffer, int nPixelCount, const float *pGammaTable );
	template< class T > void LoadFromBufferUV( const T *pBuffer, int nPixelCount );
	template< class T > void LoadFromBufferUVWQ( const T *pBuffer, int nPixelCount );
	template< class T > void LoadFromBufferUVLX( const T *pBuffer, int nPixelCount );
	template< class T > void WriteToBufferRGB( T *pBuffer, int nPixelCount, const uint16 *pInvGammaTable ) const;
	template< class T > void WriteToBufferRGBFloat( T *pBuffer, int nPixelCount ) const;
	template< class T > void WriteToBufferRGBA( T *pBuffer, int nPixelCount, const uint16 *pInvGammaTable ) const;
	template< class T > void WriteToBufferRGBAFloat( T *pBuffer, int nPixelCount ) const;
	template< class T > void WriteToBufferUV( T *pBuffer, int nPixelCount ) const;
	template< class T > void WriteToBufferUVWQ( T *pBuffer, int nPixelCount ) const;
	template< class T > void WriteToBufferUVLX( T *pBuffer, int nPixelCount ) const;

	static int CoordWrap( int nC, int nLimit );

	static IThreadPool *sm_pFBMThreadPool;
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline FloatBitMap_t::FloatBitMap_t(void)
{
}

inline void FloatBitMap_t::Init( int nXSize, int nYSize, int nZSize, int nAttributeMask )
{
	PurgeData();
	SetAttributeType( FBM_ATTR_RED,		ATTRDATATYPE_FLOAT, ( nAttributeMask & FBM_ATTR_RED_MASK ) != 0 );
	SetAttributeType( FBM_ATTR_GREEN,	ATTRDATATYPE_FLOAT, ( nAttributeMask & FBM_ATTR_GREEN_MASK ) != 0 );
	SetAttributeType( FBM_ATTR_BLUE,	ATTRDATATYPE_FLOAT, ( nAttributeMask & FBM_ATTR_BLUE_MASK ) != 0 );
	SetAttributeType( FBM_ATTR_ALPHA,	ATTRDATATYPE_FLOAT, ( nAttributeMask & FBM_ATTR_ALPHA_MASK ) != 0 );
	AllocateData( nXSize, nYSize, nZSize );

	// Default alpha to white. All others default to 0.0 by default
	if ( ( nAttributeMask & FBM_ATTR_ALPHA_MASK ) == 0 )
	{
		FillAttr( FBM_ATTR_ALPHA, 1.0f ); 
	}
}


//-----------------------------------------------------------------------------
// Computes the attribute mask
//-----------------------------------------------------------------------------
inline int FloatBitMap_t::GetAttributeMask() const
{
	int nMask = 0;
	if ( HasAllocatedMemory( FBM_ATTR_RED ) )
		nMask |= FBM_ATTR_RED_MASK;
	if ( HasAllocatedMemory( FBM_ATTR_GREEN ) )
		nMask |= FBM_ATTR_GREEN_MASK;
	if ( HasAllocatedMemory( FBM_ATTR_BLUE ) )
		nMask |= FBM_ATTR_BLUE_MASK;
	if ( HasAllocatedMemory( FBM_ATTR_ALPHA ) )
		nMask |= FBM_ATTR_ALPHA_MASK;
	return nMask;
}


//-----------------------------------------------------------------------------
// Compute valid attribute list
//-----------------------------------------------------------------------------
inline int FloatBitMap_t::ComputeValidAttributeList( int pIndex[4] ) const
{
	int nCount = 0;
	if ( HasAllocatedMemory( FBM_ATTR_RED ) )
		pIndex[ nCount++ ] = FBM_ATTR_RED;
	if ( HasAllocatedMemory( FBM_ATTR_GREEN ) )
		pIndex[ nCount++ ] = FBM_ATTR_GREEN;
	if ( HasAllocatedMemory( FBM_ATTR_BLUE ) )
		pIndex[ nCount++ ] = FBM_ATTR_BLUE;
	if ( HasAllocatedMemory( FBM_ATTR_ALPHA ) )
		pIndex[ nCount++ ] = FBM_ATTR_ALPHA;
	return nCount;
}


//-----------------------------------------------------------------------------
// Does the bitmap have data for a particular component?
//-----------------------------------------------------------------------------
inline bool FloatBitMap_t::HasAttributeData( FBMAttribute_t a ) const
{
	return HasAllocatedMemory( a );
}


inline int FloatBitMap_t::CoordWrap( int nC, int nLimit )
{
	if ( nC >= nLimit )
	{
		nC %= nLimit;
	}
	else if ( nC < 0 )
	{
		nC += nLimit * ( ( -nC + nLimit-1 ) / nLimit );
	}
	return nC;
}

inline float &FloatBitMap_t::Pixel(int x, int y, int z, int comp) const
{
	Assert( (x >= 0 ) && (x < NumCols() ) );
	Assert( (y >= 0) &&  (y < NumRows() ) );
	Assert( (z >= 0) &&  (z < NumSlices() ) );
	float *pData = ElementPointer<float>( comp, x, y, z );
	return *pData;
}

inline const float &FloatBitMap_t::ConstantValue( int comp ) const
{
	Assert( !HasAllocatedMemory( comp ) );
	return Pixel( 0, 0, 0, comp );
}

inline float &FloatBitMap_t::PixelWrapped(int x, int y, int z, int comp) const
{
	// like Pixel except wraps around to other side
	x = CoordWrap( x, NumCols() );
	y = CoordWrap( y, NumRows() );
	z = CoordWrap( z, NumSlices() );
	return Pixel( x, y, z, comp );
}

inline float &FloatBitMap_t::PixelClamped(int x, int y, int z, int comp) const
{
	// like Pixel except wraps around to other side
	x = clamp( x, 0, NumCols() - 1 );
	y = clamp( y, 0, NumRows() - 1 );
	z = clamp( z, 0, NumSlices() - 1 );
	return Pixel( x, y, z, comp );
}


inline float &FloatBitMap_t::Alpha( int x, int y, int z ) const
{
	Assert( ( x >= 0 ) && ( x < NumCols() ) );
	Assert( ( y >= 0 ) && ( y < NumRows() ) );
	Assert( ( z >= 0 ) && ( z < NumSlices() ) );
	return Pixel( x, y, z, FBM_ATTR_ALPHA );
}

inline PixRGBAF FloatBitMap_t::PixelRGBAF( int x, int y, int z ) const
{
	Assert( ( x >= 0 ) && ( x < NumCols() ) );
	Assert( ( y >= 0 ) && ( y < NumRows() ) );
	Assert( ( z >= 0 ) && ( z < NumSlices() ) );

	PixRGBAF RetPix;
	RetPix.Red = Pixel( x, y, z, FBM_ATTR_RED );
	RetPix.Green = Pixel( x, y, z, FBM_ATTR_GREEN );
	RetPix.Blue = Pixel( x, y, z, FBM_ATTR_BLUE );
	RetPix.Alpha = Pixel( x, y, z, FBM_ATTR_ALPHA );
	return RetPix;
}


inline void FloatBitMap_t::WritePixelRGBAF( int x, int y, int z, PixRGBAF value ) const
{
	Pixel( x, y, z, FBM_ATTR_RED ) = value.Red;
	Pixel( x, y, z, FBM_ATTR_GREEN ) = value.Green;
	Pixel( x, y, z, FBM_ATTR_BLUE ) = value.Blue;
	Pixel( x, y, z, FBM_ATTR_ALPHA ) = value.Alpha;
}


inline void FloatBitMap_t::WritePixel(int x, int y, int z, int comp, float value)
{
	Pixel( x, y, z, comp ) = value;
}


//-----------------------------------------------------------------------------
// Loads an array of image data into a buffer
//-----------------------------------------------------------------------------
template< class T > void FloatBitMap_t::LoadFromBufferRGBFloat( const T *pBuffer, int nPixelCount )
{
	Assert( nPixelCount == NumCols() * NumRows() * NumSlices() );
	for( int z = 0; z < NumSlices(); ++z )
	{
		for( int y = 0; y < NumRows(); ++y )
		{
			for( int x = 0; x < NumCols(); ++x )
			{
				Pixel( x, y, z, FBM_ATTR_RED ) = pBuffer[x].r;
				Pixel( x, y, z, FBM_ATTR_GREEN ) = pBuffer[x].g;
				Pixel( x, y, z, FBM_ATTR_BLUE ) = pBuffer[x].b;
				Pixel( x, y, z, FBM_ATTR_ALPHA ) = 1.0f;
			}
			pBuffer += NumCols();
		}
	}
}

template< class T > void FloatBitMap_t::LoadFromBufferRGB( const T *pBuffer, int nPixelCount, const float *pGammaTable )
{
	Assert( nPixelCount == NumCols() * NumRows() * NumSlices() );
	for( int z = 0; z < NumSlices(); ++z )
	{
		for( int y = 0; y < NumRows(); ++y )
		{
			for( int x = 0; x < NumCols(); ++x )
			{
				Pixel( x, y, z, FBM_ATTR_RED ) = pGammaTable[ pBuffer[x].RTo10Bit( ) ];
				Pixel( x, y, z, FBM_ATTR_GREEN ) = pGammaTable[ pBuffer[x].GTo10Bit( ) ];
				Pixel( x, y, z, FBM_ATTR_BLUE ) = pGammaTable[ pBuffer[x].BTo10Bit( ) ];
				Pixel( x, y, z, FBM_ATTR_ALPHA ) = 1.0f;
			}
			pBuffer += NumCols();
		}
	}
}

template< class T > void FloatBitMap_t::LoadFromBufferRGBAFloat( const T *pBuffer, int nPixelCount )
{
	Assert( nPixelCount == NumCols() * NumRows() * NumSlices() );
	for( int z = 0; z < NumSlices(); ++z )
	{
		for( int y = 0; y < NumRows(); ++y )
		{
			for( int x = 0; x < NumCols(); ++x )
			{
				Pixel( x, y, z, FBM_ATTR_RED ) = pBuffer[x].r;
				Pixel( x, y, z, FBM_ATTR_GREEN ) = pBuffer[x].g;
				Pixel( x, y, z, FBM_ATTR_BLUE ) = pBuffer[x].b;
				Pixel( x, y, z, FBM_ATTR_ALPHA ) = pBuffer[x].a;
			}
			pBuffer += NumCols();
		}
	}
}

template< class T > void FloatBitMap_t::LoadFromBufferRGBA( const T *pBuffer, int nPixelCount, const float *pGammaTable )
{
	float flOO1023 = 1.0f / 1023.0f;
	Assert( nPixelCount == NumCols() * NumRows() * NumSlices() );
	for( int z = 0; z < NumSlices(); ++z )
	{
		for( int y = 0; y < NumRows(); ++y )
		{
			for( int x = 0; x < NumCols(); ++x )
			{
				Pixel( x, y, z, FBM_ATTR_RED ) = pGammaTable[ pBuffer[x].RTo10Bit( ) ];
				Pixel( x, y, z, FBM_ATTR_GREEN ) = pGammaTable[ pBuffer[x].GTo10Bit( ) ];
				Pixel( x, y, z, FBM_ATTR_BLUE ) = pGammaTable[ pBuffer[x].BTo10Bit( ) ];
				Pixel( x, y, z, FBM_ATTR_ALPHA ) = pBuffer[x].ATo10Bit( ) * flOO1023;
			}
			pBuffer += NumCols();
		}
	}
}


//-----------------------------------------------------------------------------
// Loads from UV buffers
//-----------------------------------------------------------------------------
template< class T > void FloatBitMap_t::LoadFromBufferUV( const T *pBuffer, int nPixelCount )
{
	float fl2O255 = 2.0f / 255.0f;
	for( int z = 0; z < NumSlices(); ++z )
	{
		for( int y = 0; y < NumRows(); ++y )
		{
			for( int x = 0; x < NumCols(); ++x )
			{
				Pixel( x, y, z, FBM_ATTR_RED ) = ( pBuffer[x].u + 128 ) * fl2O255 - 1.0f;
				Pixel( x, y, z, FBM_ATTR_GREEN ) = ( pBuffer[x].v + 128 ) * fl2O255 - 1.0f;
				Pixel( x, y, z, FBM_ATTR_BLUE ) = 0.0f;
				Pixel( x, y, z, FBM_ATTR_ALPHA ) = 1.0f;
			}
			pBuffer += NumCols();
		}
	}
}

template< class T > void FloatBitMap_t::LoadFromBufferUVWQ( const T *pBuffer, int nPixelCount )
{
	float fl2O255 = 2.0f / 255.0f;
	for( int z = 0; z < NumSlices(); ++z )
	{
		for( int y = 0; y < NumRows(); ++y )
		{
			for( int x = 0; x < NumCols(); ++x )
			{
				Pixel( x, y, z, FBM_ATTR_RED ) = ( pBuffer[x].u + 128 ) * fl2O255 - 1.0f;
				Pixel( x, y, z, FBM_ATTR_GREEN ) = ( pBuffer[x].v + 128 ) * fl2O255 - 1.0f;
				Pixel( x, y, z, FBM_ATTR_BLUE ) = ( pBuffer[x].w + 128 ) * fl2O255 - 1.0f;
				Pixel( x, y, z, FBM_ATTR_ALPHA ) = ( pBuffer[x].q + 128 ) * fl2O255 - 1.0f;
			}
			pBuffer += NumCols();
		}
	}
}

template< class T > void FloatBitMap_t::LoadFromBufferUVLX( const T *pBuffer, int nPixelCount )
{
	float flOO255 = 1.0f / 255.0f;
	for( int z = 0; z < NumSlices(); ++z )
	{
		for( int y = 0; y < NumRows(); ++y )
		{
			for( int x = 0; x < NumCols(); ++x )
			{
				Pixel( x, y, z, FBM_ATTR_RED ) = ( pBuffer[x].u + 128 ) * 2.0f * flOO255 - 1.0f;
				Pixel( x, y, z, FBM_ATTR_GREEN ) = ( pBuffer[x].v + 128 ) * 2.0f * flOO255 - 1.0f;
				Pixel( x, y, z, FBM_ATTR_BLUE ) = pBuffer[x].l * flOO255;
				Pixel( x, y, z, FBM_ATTR_ALPHA ) = 1.0f;
			}
			pBuffer += NumCols();
		}
	}
}


//-----------------------------------------------------------------------------
// Writes an array of image data into a buffer
//-----------------------------------------------------------------------------
template< class T > void FloatBitMap_t::WriteToBufferRGBFloat( T *pBuffer, int nPixelCount ) const
{
	Assert( nPixelCount == NumCols() * NumRows() * NumSlices() );
	for( int z = 0; z < NumSlices(); ++z )
	{
		for( int y = 0; y < NumRows(); ++y )
		{
			for( int x = 0; x < NumCols(); ++x )
			{
				pBuffer[x].r = Pixel( x, y, z, FBM_ATTR_RED );
				pBuffer[x].g = Pixel( x, y, z, FBM_ATTR_GREEN );
				pBuffer[x].b = Pixel( x, y, z, FBM_ATTR_BLUE );
			}
			pBuffer += NumCols();
		}
	}
}

template< class T > void FloatBitMap_t::WriteToBufferRGB( T *pBuffer, int nPixelCount, const uint16 *pInvGammaTable ) const
{
	int c;
	Assert( nPixelCount == NumCols() * NumRows() * NumSlices() );
	for( int z = 0; z < NumSlices(); ++z )
	{
		for( int y = 0; y < NumRows(); ++y )
		{
			for( int x = 0; x < NumCols(); ++x )
			{
				c = ( int )( 1023.0f * Pixel( x, y, z, FBM_ATTR_RED ) + 0.5f ); 
				pBuffer[x].RFrom10Bit( pInvGammaTable[ clamp( c, 0, 1023 ) ] );
				c = ( int )( 1023.0f * Pixel( x, y, z, FBM_ATTR_GREEN ) + 0.5f ); 
				pBuffer[x].GFrom10Bit( pInvGammaTable[ clamp( c, 0, 1023 ) ] );
				c = ( int )( 1023.0f * Pixel( x, y, z, FBM_ATTR_BLUE ) + 0.5f ); 
				pBuffer[x].BFrom10Bit( pInvGammaTable[ clamp( c, 0, 1023 ) ] );
			}
			pBuffer += NumCols();
		}
	}
}

template< class T > void FloatBitMap_t::WriteToBufferRGBAFloat( T *pBuffer, int nPixelCount ) const
{
	Assert( nPixelCount == NumCols() * NumRows() * NumSlices() );
	for( int z = 0; z < NumSlices(); ++z )
	{
		for( int y = 0; y < NumRows(); ++y )
		{
			for( int x = 0; x < NumCols(); ++x )
			{
				pBuffer[x].r = Pixel( x, y, z, FBM_ATTR_RED );
				pBuffer[x].g = Pixel( x, y, z, FBM_ATTR_GREEN );
				pBuffer[x].b = Pixel( x, y, z, FBM_ATTR_BLUE );
				pBuffer[x].a = Pixel( x, y, z, FBM_ATTR_ALPHA );
			}
			pBuffer += NumCols();
		}
	}
}

template< class T > void FloatBitMap_t::WriteToBufferRGBA( T *pBuffer, int nPixelCount, const uint16 *pInvGammaTable ) const
{
	int c;
	Assert( nPixelCount == NumCols() * NumRows() * NumSlices() );
	for( int z = 0; z < NumSlices(); ++z )
	{
		for( int y = 0; y < NumRows(); ++y )
		{
			for( int x = 0; x < NumCols(); ++x )
			{
				c = ( int )( 1023.0f * Pixel( x, y, z, FBM_ATTR_RED ) + 0.5f ); 
				pBuffer[x].RFrom10Bit( pInvGammaTable[ clamp( c, 0, 1023 ) ] );
				c = ( int )( 1023.0f * Pixel( x, y, z, FBM_ATTR_GREEN ) + 0.5f ); 
				pBuffer[x].GFrom10Bit( pInvGammaTable[ clamp( c, 0, 1023 ) ] );
				c = ( int )( 1023.0f * Pixel( x, y, z, FBM_ATTR_BLUE ) + 0.5f ); 
				pBuffer[x].BFrom10Bit( pInvGammaTable[ clamp( c, 0, 1023 ) ] );
				c = ( int )( 1023.0f * Pixel( x, y, z, FBM_ATTR_ALPHA ) + 0.5f ); 
				pBuffer[x].AFrom10Bit( clamp( c, 0, 1023 ) );
			}
			pBuffer += NumCols();
		}
	}
}


//-----------------------------------------------------------------------------
// Writes to UV buffers
//-----------------------------------------------------------------------------
template< class T > void FloatBitMap_t::WriteToBufferUV( T *pBuffer, int nPixelCount ) const
{
	int c;
	float fl255O2 = 255.0f / 2.0f;
	for( int z = 0; z < NumSlices(); ++z )
	{
		for( int y = 0; y < NumRows(); ++y )
		{
			for( int x = 0; x < NumCols(); ++x )
			{
				c = ( int )( fl255O2 * ( Pixel( x, y, z, FBM_ATTR_RED ) + 1.0f ) );
				pBuffer[x].u = clamp( c, 0, 255 ) - 128;
				c = ( int )( fl255O2 * ( Pixel( x, y, z, FBM_ATTR_GREEN ) + 1.0f ) );
				pBuffer[x].v = clamp( c, 0, 255 ) - 128;
			}
			pBuffer += NumCols();
		}
	}
}

template< class T > void FloatBitMap_t::WriteToBufferUVWQ( T *pBuffer, int nPixelCount ) const 
{
	int c;
	float fl255O2 = 255.0f / 2.0f;
	for( int z = 0; z < NumSlices(); ++z )
	{
		for( int y = 0; y < NumRows(); ++y )
		{
			for( int x = 0; x < NumCols(); ++x )
			{
				c = ( int )( fl255O2 * ( Pixel( x, y, z, FBM_ATTR_RED ) + 1.0f ) );
				pBuffer[x].u = clamp( c, 0, 255 ) - 128;
				c = ( int )( fl255O2 * ( Pixel( x, y, z, FBM_ATTR_GREEN ) + 1.0f ) );
				pBuffer[x].v = clamp( c, 0, 255 ) - 128;
				c = ( int )( fl255O2 * ( Pixel( x, y, z, FBM_ATTR_BLUE ) + 1.0f ) );
				pBuffer[x].w = clamp( c, 0, 255 ) - 128;
				c = ( int )( fl255O2 * ( Pixel( x, y, z, FBM_ATTR_ALPHA ) + 1.0f ) );
				pBuffer[x].q = clamp( c, 0, 255 ) - 128;
			}
			pBuffer += NumCols();
		}
	}
}

template< class T > void FloatBitMap_t::WriteToBufferUVLX( T *pBuffer, int nPixelCount ) const
{
	int c;
	float fl255O2 = 255.0f / 2.0f;
	for( int z = 0; z < NumSlices(); ++z )
	{
		for( int y = 0; y < NumRows(); ++y )
		{
			for( int x = 0; x < NumCols(); ++x )
			{
				c = ( int )( fl255O2 * ( Pixel( x, y, z, FBM_ATTR_RED ) + 1.0f ) );
				pBuffer[x].u = clamp( c, 0, 255 ) - 128;
				c = ( int )( fl255O2 * ( Pixel( x, y, z, FBM_ATTR_GREEN ) + 1.0f ) );
				pBuffer[x].v = clamp( c, 0, 255 ) - 128;
				c = ( int )( 255.0f * Pixel( x, y, z, FBM_ATTR_BLUE ) );
				pBuffer[x].l = clamp( c, 0, 255 );
				pBuffer[x].x = 255;
			}
			pBuffer += NumCols();
		}
	}
}



// a FloatCubeMap_t holds the floating point bitmaps for 6 faces of a cube map
class FloatCubeMap_t
{
public:
	FloatBitMap_t face_maps[6];

	FloatCubeMap_t(int xfsize, int yfsize)
	{
		// make an empty one with face dimensions xfsize x yfsize
		for(int f=0;f<6;f++)
		{
			face_maps[f].Init(xfsize,yfsize);
		}
	}

	// load basenamebk,pfm, basenamedn.pfm, basenameft.pfm, ...
	FloatCubeMap_t(const char *basename);

	// save basenamebk,pfm, basenamedn.pfm, basenameft.pfm, ...
	void WritePFMs(const char *basename);

	Vector AverageColor(void)
	{
		Vector ret(0,0,0);
		int nfaces = 0;
		for( int f = 0;f < 6;f ++ )
			if ( face_maps[f].NumElements() )
			{
				nfaces++;
				ret += face_maps[f].AverageColor();
			}
		if ( nfaces )
			ret *= ( 1.0 / nfaces );
		return ret;
	}

	float BrightestColor( void )
	{
		float ret = 0.0;
		for( int f = 0;f < 6;f ++ )
			if ( face_maps[f].NumElements() )
			{
				ret = MAX( ret, face_maps[f].BrightestColor() );
			}
		return ret;
	}


	// generate the N-order spherical harmonic coeffcients to approxiamte this cubemap.
	// for order N, this will fill in 1 + 2 * N + N^2 vectors ( 0 = 0, 1 = 4, 2 = 9 .. )
	// vectors are used to hold the r,g,b coeffs
	void CalculateSphericalHarmonicApproximation( int nOrder, Vector *flCoeffs );

	// inverse of above
	void GenerateFromSphericalHarmonics( int nOrder, Vector const *flCoeffs );

	// resample a cubemap to one of possibly a lower resolution, using a given phong exponent.
	// dot-product weighting will be used for the filtering operation.
	void Resample(FloatCubeMap_t &dest, float flPhongExponent);

	// returns the normalized direciton vector through a given pixel of a given face
	Vector PixelDirection(int face, int x, int y);

	// returns the direction vector throught the center of a cubemap face
	Vector FaceNormal( int nFaceNumber );
};




// Image Pyramid class.
#define MAX_IMAGE_PYRAMID_LEVELS 16							// up to 64kx64k

enum ImagePyramidMode_t 
{
	PYRAMID_MODE_GAUSSIAN,
};

class FloatImagePyramid_t
{
public:
	int m_nLevels;
	FloatBitMap_t *m_pLevels[MAX_IMAGE_PYRAMID_LEVELS];		// level 0 is highest res

	FloatImagePyramid_t(void)
	{
		m_nLevels = 0;
		memset( m_pLevels, 0, sizeof( m_pLevels ));
	}

	// build one. clones data from src for level 0.
	FloatImagePyramid_t(const FloatBitMap_t &src, ImagePyramidMode_t mode);

	// read or write a Pixel from a given level. All coordinates are specified in the same domain as the base level.
	float &Pixel(int x, int y, int component, int level) const;

	FloatBitMap_t *Level(int lvl) const
	{
		Assert( lvl < m_nLevels );
		return m_pLevels[lvl];
	}
	// rebuild all levels above the specified level
	void ReconstructLowerResolutionLevels(int starting_level);

	~FloatImagePyramid_t(void);

	void WriteTGAs(const char *basename) const;				// outputs name_00.tga, name_01.tga,...
};

#endif
