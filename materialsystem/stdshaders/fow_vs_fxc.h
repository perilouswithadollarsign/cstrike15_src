//========== Copyright (c) Valve Corporation, All rights reserved. ==========//
//
// Purpose: This is where all common code for vertex shaders go.
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef _FOW_VS_FXC_H
#define _FOW_VS_FXC_H


#if ( FOW == 1 )


float2 CalcFoWCoord( const float4 vFoWWorldSize, const float2 vWorldPos )
{
	return ( vWorldPos - vFoWWorldSize.xy ) / vFoWWorldSize.zw;
}


#endif


#endif // _FOW_VS_FXC_H
