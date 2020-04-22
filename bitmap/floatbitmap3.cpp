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
#include "vstdlib/vstdlib.h"
#include "vstdlib/random.h"
#include "tier1/strtools.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


void FloatBitMap_t::InitializeWithRandomPixelsFromAnotherFloatBM( FloatBitMap_t const & other )
{
	for( int z = 0; z < NumSlices(); z++ )
	{
		for( int y = 0;y < NumRows();y++ )
		{
			for( int x = 0;x < NumCols();x++ )
			{
				int x1 = RandomInt( 0, other.NumCols() - 1 );
				int y1 = RandomInt( 0, other.NumRows() - 1 );
				int z1 = RandomInt( 0, other.NumSlices() - 1 );
				for( int c = 0; c < 4; c++ )
				{
					Pixel( x, y, z, c ) = other.Pixel( x1, y1, z1, c );
				}
			}
		}
	}
}


void FloatBitMap_t::QuarterSizeWithGaussian( FloatBitMap_t *pBitmap )
{
	// generate a new bitmap half on each axis, using a separable gaussian.
	static float kernel[]={.05, .25, .4, .25, .05};
	pBitmap->Init( NumCols() / 2, NumRows() / 2 );

	for( int y = 0;y < NumRows() / 2;y++ )
		for( int x = 0;x < NumCols() / 2;x++ )
		{
			for( int c = 0;c < 4;c++ )
			{
				float sum = 0;
				float sumweights = 0;							// for versatility in handling the
															// offscreen case
				for( int xofs =- 2;xofs <= 2;xofs++ )
				{
					int orig_x = MAX( 0, MIN( NumCols() - 1, x * 2 + xofs ));
					for( int yofs =- 2;yofs <= 2;yofs++ )
					{
						int orig_y = MAX( 0, MIN( NumRows() - 1, y * 2 + yofs ));
						float coeff = kernel[xofs + 2]* kernel[yofs + 2];
						sum += Pixel( orig_x, orig_y, 0, c ) * coeff;
						sumweights += coeff;
					}
				}
				pBitmap->Pixel( x, y, 0, c ) = sum / sumweights;
			}
		}
}

FloatImagePyramid_t::FloatImagePyramid_t( FloatBitMap_t const & src, ImagePyramidMode_t mode )
{
	memset( m_pLevels, 0, sizeof( m_pLevels ));
	m_nLevels = 1;
	m_pLevels[0]= new FloatBitMap_t( & src );
	ReconstructLowerResolutionLevels( 0 );
}

void FloatImagePyramid_t::ReconstructLowerResolutionLevels( int start_level )
{
	while( ( m_pLevels[start_level]-> NumCols() > 1 ) && ( m_pLevels[start_level]-> NumRows() > 1 ) )
	{
		if ( m_pLevels[start_level + 1] )
			delete m_pLevels[start_level + 1];
		m_pLevels[start_level + 1] = new FloatBitMap_t;
		m_pLevels[start_level]->QuarterSizeWithGaussian( m_pLevels[start_level + 1] );
		start_level++;
	}
	m_nLevels = start_level + 1;
}

float & FloatImagePyramid_t::Pixel( int x, int y, int component, int level ) const
{
	Assert( level < m_nLevels );
	x <<= level;
	y <<= level;
	return m_pLevels[level]-> Pixel( x, y, 0, component );
}

void FloatImagePyramid_t::WriteTGAs( char const * basename ) const
{
	for( int l = 0;l < m_nLevels;l++ )
	{
		char bname_out[1024];
		Q_snprintf(bname_out,sizeof(bname_out),"%s_%02d.tga",basename,l);
		m_pLevels[l]-> WriteTGAFile( bname_out );
	}

}


FloatImagePyramid_t::~FloatImagePyramid_t( void )
{
	for( int l = 0;l < m_nLevels;l++ )
		if ( m_pLevels[l] )
			delete m_pLevels[l];
}
