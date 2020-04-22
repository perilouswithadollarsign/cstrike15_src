//========== Copyright (c) Valve Corporation, All rights reserved. ==========//
//
// Purpose: This is where all common code for vertex shaders go.
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef _FOW_PS_FXC_H
#define _FOW_PS_FXC_H


#if ( FOW == 1 )


static const float3 g_FoWToGray		= float3( 0.3, 0.59, 0.11 );

#define FOW_DARKNESS_VALUE	0.5
#define FOW_GRAY_FACTOR		0.75
#define FOW_COLOR_FACTOR	0.25
#define FOW_GRAY_HILIGHTS	2.0

float3 CalcFoW( const sampler FoWSampler, const float2 vFoWCoords, const float3 CurrentColor )
{
	float	vFoWResult = tex2D( FoWSampler, vFoWCoords.xy ).r;

	float3	vGray = pow( dot( CurrentColor, g_FoWToGray ), FOW_GRAY_HILIGHTS );
	float3	vFinalColor = ( vGray * FOW_GRAY_FACTOR	 * FOW_DARKNESS_VALUE ) + ( CurrentColor * FOW_COLOR_FACTOR * FOW_DARKNESS_VALUE );

	return ( vFinalColor * ( 1.0 - vFoWResult ) ) + ( CurrentColor * vFoWResult );
}


#endif


#endif // _FOW_PS_FXC_H
