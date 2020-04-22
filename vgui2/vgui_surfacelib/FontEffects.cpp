//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Font effects that operate on linear rgba data
//
//=====================================================================================//

#include "tier0/platform.h"
#include <tier0/dbg.h>
#include <math.h>
#include "FontEffects.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Adds center line to font
//-----------------------------------------------------------------------------
void ApplyRotaryEffectToTexture( int rgbaWide, int rgbaTall, unsigned char *rgba, bool bRotary )
{
	if ( !bRotary )
		return;

	int y = rgbaTall * 0.5;

	unsigned char *line = &rgba[(y * rgbaWide) * 4];

	// Draw a line down middle
	for (int x = 0; x < rgbaWide; x++, line+=4)
	{
		line[0] = 127;
		line[1] = 127;
		line[2] = 127;
		line[3] = 255;
	}
}

//-----------------------------------------------------------------------------
// Purpose: adds scanlines to the texture
//-----------------------------------------------------------------------------
void ApplyScanlineEffectToTexture( int rgbaWide, int rgbaTall, unsigned char *rgba, int iScanLines )
{
	if ( iScanLines < 2 )
		return;

	float scale;
	scale = 0.7f;

	// darken all the areas except the scanlines
	for (int y = 0; y < rgbaTall; y++)
	{
		// skip the scan lines
		if (y % iScanLines == 0)
			continue;

		unsigned char *pBits = &rgba[(y * rgbaWide) * 4];

		// darken the other lines
		for (int x = 0; x < rgbaWide; x++, pBits += 4)
		{
			pBits[0] *= scale;
			pBits[1] *= scale;
			pBits[2] *= scale;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: adds a dropshadow the the font texture
//-----------------------------------------------------------------------------
void ApplyDropShadowToTexture( int rgbaWide, int rgbaTall, unsigned char *rgba, int iDropShadowOffset )
{
	if ( !iDropShadowOffset )
		return;

	// walk the original image from the bottom up
	// shifting it down and right, and turning it black (the dropshadow)
	for (int y = rgbaTall - 1; y >= iDropShadowOffset; y--)
	{
		for (int x = rgbaWide - 1; x >= iDropShadowOffset; x--)
		{
			unsigned char *dest = &rgba[(x +  (y * rgbaWide)) * 4];
			if (dest[3] == 0)
			{
				// there is nothing in this spot, copy in the dropshadow
				unsigned char *src = &rgba[(x - iDropShadowOffset + ((y - iDropShadowOffset) * rgbaWide)) * 4];
				dest[0] = 0;
				dest[1] = 0;
				dest[2] = 0;
				dest[3] = src[3];
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: adds an outline to the font texture
//-----------------------------------------------------------------------------
void ApplyOutlineToTexture( int rgbaWide, int rgbaTall, unsigned char *rgba, int iOutlineSize )
{
	if ( !iOutlineSize )
		return;

	int x, y;
	for( y = 0; y < rgbaTall; y++ )
	{
		for( x = 0; x < rgbaWide; x++ )
		{
			unsigned char *src = &rgba[(x + (y * rgbaWide)) * 4];
			if( src[3] == 0 )
			{
				// We have a valid font texel.  Make all the alpha == 0 neighbors black.
				int shadowX, shadowY;
				for( shadowX = -(int)iOutlineSize; shadowX <= (int)iOutlineSize; shadowX++ )
				{
					for( shadowY = -(int)iOutlineSize; shadowY <= (int)iOutlineSize; shadowY++ )
					{
						if( shadowX == 0 && shadowY == 0 )
						{
							continue;
						}
						int testX, testY;
						testX = shadowX + x;
						testY = shadowY + y;
						if( testX < 0 || testX >= rgbaWide ||
							testY < 0 || testY >= rgbaTall )
						{
							continue;
						}
						unsigned char *test = &rgba[(testX + (testY * rgbaWide)) * 4];
						if( test[0] != 0 && test[1] != 0 && test[2] != 0 && test[3] != 0 )
						{
							src[0] = 0;
							src[1] = 0;
							src[2] = 0;
							src[3] = 255;
						}
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gets the blur value for a single pixel
//-----------------------------------------------------------------------------
FORCEINLINE void GetBlurValueForPixel(unsigned char *src, int blur, float *gaussianDistribution, int srcX, int srcY, int rgbaWide, int rgbaTall, unsigned char *dest)
{	
	float accum = 0.0f;

	// scan the positive x direction
	int maxX = MIN(srcX + blur, rgbaWide - 1);
	int minX = MAX(srcX - blur, 0);
	for (int x = minX; x <= maxX; x++)
	{
		int maxY = MIN(srcY + blur, rgbaTall - 1);
		int minY = MAX(srcY - blur, 0);
		for (int y = minY; y <= maxY; y++)
		{
			unsigned char *srcPos = src + ((x + (y * rgbaWide)) * 4);

			// muliply by the value matrix
			float weight = gaussianDistribution[x - srcX + blur];
			float weight2 = gaussianDistribution[y - srcY + blur];
			accum += (srcPos[0] * (weight * weight2));
		}
	}

	dest[0] = dest[1] = dest[2] = 255; //leave ALL pixels white or we get black backgrounds mixed in
	dest[3] = MIN( (int)accum, 255); //blur occurs entirely in the alpha
}

//-----------------------------------------------------------------------------
// Purpose: blurs the texture
//-----------------------------------------------------------------------------
void ApplyGaussianBlurToTexture( int rgbaWide, int rgbaTall, unsigned char *rgba, int iBlur )
{
	float	 *pGaussianDistribution;

	if ( !iBlur  )
		return;

	// generate the gaussian field
	pGaussianDistribution = (float*) stackalloc( (iBlur*2+1) * sizeof(float) );
	double sigma = 0.683 * iBlur;
	for (int x = 0; x <= (iBlur * 2); x++)
	{
		int val = x - iBlur;
		pGaussianDistribution[x] = (float)( 1.0f / sqrt(2 * 3.14 * sigma * sigma)) * pow(2.7, -1 * (val * val) / (2 * sigma * sigma));
	}

	// alloc a new buffer
	unsigned char *src = (unsigned char *) stackalloc( rgbaWide * rgbaTall * 4);

	// copy in
	memcpy(src, rgba, rgbaWide * rgbaTall * 4);

	// incrementing destination pointer
	unsigned char *dest = rgba;
	for (int y = 0; y < rgbaTall; y++)
	{
		for (int x = 0; x < rgbaWide; x++)
		{
			// scan the source pixel
			GetBlurValueForPixel(src, iBlur, pGaussianDistribution, x, y, rgbaWide, rgbaTall, dest);

			// move to the next
			dest += 4;
		}
	}
}

