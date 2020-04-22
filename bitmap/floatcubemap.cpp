
#include <tier0/platform.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "bitmap/floatbitmap.h"
#include <filesystem.h>
#include <mathlib/vector.h>
#include "mathlib/spherical_geometry.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


static Vector face_xvector[6]={								// direction of x pixels on face
	Vector( - 1, 0, 0 ),											// back
	Vector( 1, 0, 0 ),											// down
	Vector( 1, 0, 0 ),											// front
	Vector( 0, 1, 0 ),											// left
	Vector( 0, - 1, 0 ),											// right
	Vector( 1, 0, 0 )											// up
};

static Vector face_yvector[6]={								// direction of y pixels on face
	Vector( 0, 0, - 1 ),											// back
	Vector( 0, 1, 0 ),											// down
	Vector( 0, 0, - 1 ),											// front
	Vector( 0, 0, - 1 ),											// left
	Vector( 0, 0, - 1 ),											// right
	Vector( 0, - 1, 0 )											// up
};


static Vector face_zvector[6]={
	Vector( 1, 1, 1 ),											// back
	Vector( - 1, - 1, - 1 ),										// down
	Vector( - 1, - 1, 1 ),										// front
	Vector( 1, - 1, 1 ),											// left
	Vector( - 1, 1, 1 ),											// right
	Vector( - 1, 1, 1 )											// up
};


static char const *namepts[6]={"%sbk.pfm","%sdn.pfm","%sft.pfm","%slf.pfm","%srt.pfm","%sup.pfm"};

FloatCubeMap_t::FloatCubeMap_t( char const *basename )
{
	for( int f = 0;f < 6;f++ )
	{
		char fnamebuf[512];
		sprintf( fnamebuf, namepts[f], basename );
		face_maps[f].LoadFromPFM( fnamebuf );
	}
}

void FloatCubeMap_t::WritePFMs( char const *basename )
{
	for( int f = 0;f < 6;f++ )
	{
		char fnamebuf[512];
		sprintf( fnamebuf, namepts[f], basename );
		face_maps[f].WritePFM( fnamebuf );
	}
}

Vector FloatCubeMap_t::PixelDirection( int face, int x, int y )
{
	FloatBitMap_t const & bm = face_maps[face];
	float xc = x * 1.0 / ( bm.NumCols() - 1 );
	float yc = y * 1.0 / ( bm.NumRows() - 1 );
	Vector dir = 2 * xc * face_xvector[face]+
			2 * yc * face_yvector[face]+ face_zvector[face];
	VectorNormalize( dir );
	return dir;
}

Vector FloatCubeMap_t::FaceNormal( int face )
{
	float xc = 0.5;
	float yc = 0.5;
	Vector dir = 2 * xc * face_xvector[face]+
			2 * yc * face_yvector[face]+ face_zvector[face];
	VectorNormalize( dir );
	return dir;
}

void FloatCubeMap_t::Resample( FloatCubeMap_t & out, float flPhongExponent )
{
	// terribly slow brute force algorithm just so I can try it out
	for( int dface = 0;dface < 6;dface++ )
	{
		for( int dy = 0;dy < out.face_maps[dface].NumRows();dy++ )
			for( int dx = 0;dx < out.face_maps[dface].NumCols();dx++ )
			{
				float sum_weights = 0;
				float sum_rgb[3]={0, 0, 0};
				for( int sface = 0;sface < 6;sface++ )
				{
					// easy 15% optimization - check if faces point away from each other
					if ( DotProduct( FaceNormal( sface ), FaceNormal( sface ) ) >- 0.9 )
					{
						Vector ddir = out.PixelDirection( dface, dx, dy );
						for( int sy = 0;sy < face_maps[sface].NumRows();sy++ )
							for( int sx = 0;sx < face_maps[sface].NumCols();sx++ )
							{
								float dp = DotProduct( ddir, PixelDirection( sface, sx, sy ) );
								if ( dp > 0.0 )
								{
									dp = pow( dp, flPhongExponent );
									sum_weights += dp;
									for( int c = 0;c < 3;c++ )
										sum_rgb[c] += dp * face_maps[sface].Pixel( sx, sy, 0, c );
								}
							}
					}
				}
				for( int c = 0;c < 3;c++ )
					out.face_maps[dface].Pixel( dx, dy, 0, c ) = sum_rgb[c] * ( 1.0 / sum_weights );
			}
	}
}




void FloatCubeMap_t::CalculateSphericalHarmonicApproximation( int nOrder, Vector *flCoeffs )
{
	for( int nL = 0; nL <= nOrder; nL++ )
	{
		for( int nM = - nL ; nM <= nL; nM++ )
		{
			Vector vecSum( 0, 0, 0 );
			float flSumWeights = 0.;
			for( int nFace = 0; nFace < 6; nFace++ )
				for( int nY = 0; nY < face_maps[nFace].NumRows(); nY++ )
					for( int nX = 0; nX < face_maps[nFace].NumCols(); nX++ )
					{
						// determine direction and area of sample. !!speed!! this could be incremental
						Vector dir00 = PixelDirection( nFace, nX, nY );
						Vector dir01 = PixelDirection( nFace, nX, nY + 1 );
						Vector dir10 = PixelDirection( nFace, nX + 1 , nY );
						Vector dir11 = PixelDirection( nFace, nX + 1, nY + 1 );
						float flArea = UnitSphereTriangleArea( dir00, dir10, dir11 ) +
							UnitSphereTriangleArea( dir00, dir01, dir11 );
						float flHarmonic = SphericalHarmonic( nL, nM, dir00 );
						flSumWeights += flArea;
						for( int c = 0; c < 3; c++ )
							vecSum[c] += flHarmonic * face_maps[nFace].Pixel( nX, nY, 0, c ) * flArea;
					}
			vecSum *= ( ( 4 * M_PI ) / flSumWeights );
			*( flCoeffs++ ) = vecSum;
		}
	}
}

void FloatCubeMap_t::GenerateFromSphericalHarmonics( int nOrder, Vector const *flCoeffs )
{
	for( int nFace = 0; nFace < 6; nFace++ )
		face_maps[nFace].Clear( 0, 0, 0, 1 );
	for( int nL = 0; nL <= nOrder; nL++ )
	{
		for( int nM = - nL ; nM <= nL; nM++ )
		{
			for( int nFace = 0; nFace < 6; nFace++ )
				for( int nY = 0; nY < face_maps[nFace].NumRows(); nY++ )
					for( int nX = 0; nX < face_maps[nFace].NumCols(); nX++ )
					{
						// determine direction and area of sample. !!speed!! this could be incremental
						Vector dir00 = PixelDirection( nFace, nX, nY );
						float flHarmonic = SphericalHarmonic( nL, nM, dir00 );
						for( int c = 0; c < 3; c++ )
							face_maps[nFace].Pixel( nX, nY, 0, c ) += ( *flCoeffs )[c] * flHarmonic;
					}
			flCoeffs++;
		}
	}
}

