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

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


static float ScaleValue( float f, float overbright )
{
	// map a value between 0..255 to the scale factor
	int ival = ( int )f;
	return ival * ( overbright / 255.0 );
}

static float IScaleValue( float f, float overbright )
{
	f *= ( 1.0 / overbright );
	int ival = ( int )MIN( 255, ceil( f * 255.0 ));
	return ival;
}

void MaybeSetScaleVaue( FloatBitMap_t const & orig, FloatBitMap_t & newbm, int x, int y,
					   float newscale, float overbright )
{
	// clamp the given scale value to the legal range for that pixel and regnerate the rgb
	// components.
	float maxc = MAX( MAX( orig.Pixel( x, y, 0, 0 ), orig.Pixel( x, y, 0, 1 )), orig.Pixel( x, y, 0, 2 ));
	if ( maxc == 0.0 )
	{
		// pixel is black. any scale value is fine.
		newbm.Pixel( x, y, 0, 3 ) = newscale;
		for( int c = 0;c < 3;c++ )
			newbm.Pixel( x, y, 0, c ) = 0;
	}
	else
	{
//		float desired_floatscale=maxc;
		float scale_we_will_get = ScaleValue( newscale, overbright );
//		if (scale_we_will_get >= desired_floatscale )
		{
			newbm.Pixel( x, y, 0, 3 ) = newscale;
			for( int c = 0;c < 3;c++ )
				newbm.Pixel( x, y, 0, c ) = orig.Pixel( x, y, 0, c ) / ( scale_we_will_get );
		}
	}
}


void FloatBitMap_t::Uncompress( float overbright )
{
	for( int y = 0;y < NumRows();y++ )
		for( int x = 0;x < NumCols();x++ )
		{
			int iactual_alpha_value = ( int )( 255.0 * Pixel( x, y, 0, 3 ) );
			float actual_alpha_value = iactual_alpha_value * ( 1.0 / 255.0 );
			for( int c = 0;c < 3;c++ )
			{
				int iactual_color_value = ( int )( 255.0 * Pixel( x, y, 0, c ) );
				float actual_color_value = iactual_color_value * ( 1.0 / 255.0 );
				Pixel( x, y, 0, c ) = actual_alpha_value * actual_color_value * overbright;
			}
		}
}

#define GAUSSIAN_WIDTH 5
#define SQ(x) ((x)*(x))

void FloatBitMap_t::CompressTo8Bits( float overbright )
{
	FloatBitMap_t TmpFBM( NumCols(), NumRows() );
	// first, saturate to max overbright
	for( int y = 0;y < NumRows();y++ )
		for( int x = 0;x < NumCols();x++ )
			for( int c = 0;c < 3;c++ )
				Pixel( x, y, 0, c ) = MIN( overbright, Pixel( x, y, 0, c ));
	// first pass - choose nominal scale values to convert to rgb,scale
	for( int y = 0;y < NumRows();y++ )
		for( int x = 0;x < NumCols();x++ )
		{
			// determine maximum component
			float maxc = MAX( MAX( Pixel( x, y, 0, 0 ), Pixel( x, y, 0, 1 )), Pixel( x, y, 0, 2 ));
			if ( maxc == 0 )
			{
				for( int c = 0;c < 4;c++ )
					TmpFBM.Pixel( x, y, 0, c ) = 0;
			}
			else
			{
				float desired_floatscale = maxc;
				float closest_iscale = IScaleValue( desired_floatscale, overbright );
				float scale_value_we_got = ScaleValue( closest_iscale, overbright );
				TmpFBM.Pixel( x, y, 0, 3 ) = closest_iscale;
				for( int c = 0;c < 3;c++ )
					TmpFBM.Pixel( x, y, 0, c ) = Pixel( x, y, 0, c ) / scale_value_we_got;
			}
		}
	// now, refine scale values
#ifdef FILTER_TO_REDUCE_LERP_ARTIFACTS
// I haven't been able to come up with a filter which eleiminates objectionable artifacts on all
// source textures. So, I've gone to doing the lerping in the shader.
	int pass = 0;
	while( pass < 1 )
	{
		FloatBitMap_t temp_filtered( & TmpFBM );
		for( int y = 0;y < NumRows();y++ )
		{
			for( int x = 0;x < NumCols();x++ )
			{
				float sum_scales = 0.0;
				float sum_weights = 0.0;
				for( int yofs =- GAUSSIAN_WIDTH;yofs <= GAUSSIAN_WIDTH;yofs++ )
					for( int xofs =- GAUSSIAN_WIDTH;xofs <= GAUSSIAN_WIDTH;xofs++ )
					{
						float r = 0.456 * GAUSSIAN_WIDTH;
						r = 0.26 * GAUSSIAN_WIDTH;
						float x1 = xofs / r;
						float y1 = yofs / r;
						float a = ( SQ( x1 ) + SQ( y1 )) / ( 2.0 * SQ( r ));
						float w = exp( - a );
						sum_scales += w * TmpFBM.PixelClamped( x + xofs, y + yofs, 3 );
						sum_weights += w;
					}
				int new_trial_scale = sum_scales * ( 1.0 / sum_weights );
				MaybeSetScaleVaue( * this, temp_filtered, x, y, new_trial_scale, overbright );
			}
		}
		pass++;
		memcpy( TmpFBM.RGBAData, temp_filtered.RGBAData, NumCols() * NumRows() * 4 * sizeof( float ));
	}
#endif

	CopyAttrFrom( TmpFBM, FBM_ATTR_RED );
	CopyAttrFrom( TmpFBM, FBM_ATTR_GREEN );
	CopyAttrFrom( TmpFBM, FBM_ATTR_BLUE );
	CopyAttrFrom( TmpFBM, FBM_ATTR_ALPHA );

	// now, map scale to real value
	for( int y = 0; y < NumRows(); y++ )
		for( int x = 0; x < NumCols(); x++ )
			Pixel( x, y, 0, 3 ) *= ( 1.0 / 255.0 );
}
