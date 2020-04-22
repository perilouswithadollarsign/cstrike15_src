//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include <halton.h>

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


HaltonSequenceGenerator_t::HaltonSequenceGenerator_t(int b)
{
	base=b;
	fbase=(float) b;
	seed=1;

}

float HaltonSequenceGenerator_t::GetElement(int elem)
{
	int tmpseed=seed;
	float ret=0.0;
	float base_inv=1.0/fbase;
	while(tmpseed)
	{
		int dig=tmpseed % base;
		ret+=((float) dig)*base_inv;
		base_inv/=fbase;
		tmpseed/=base;
	}
	return ret;
}


int InsideOut( int nTotal, int nCounter )
{
    int b = 0;
	for ( int m = nTotal, k = 1; k < nTotal; k <<= 1 )
	{
		if ( nCounter << 1 >= m )
		{
			b += k;
			nCounter -= ( m + 1 ) >> 1;
			m >>= 1;
		}
		else 
		{
			m = ( m + 1 ) >> 1;
		}
	}
	Assert( ( b >= 0 ) && ( b < nTotal ) );
	return b;
}
