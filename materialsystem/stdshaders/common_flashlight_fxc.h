//========== Copyright (c) Valve Corporation, All rights reserved. ==========//
//
// Purpose: Common pixel shader code specific to flashlights
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef COMMON_FLASHLIGHT_FXC_H_
#define COMMON_FLASHLIGHT_FXC_H_

#include "common_ps_fxc.h"


// Superellipse soft clipping
//
// Input:
//   - Point Q on the x-y plane
//   - The equations of two superellipses (with major/minor axes given by
//     a,b and A,B for the inner and outer ellipses, respectively)
//   - This is changed a bit from the original RenderMan code to be better vectorized
//
// Return value:
//   - 0 if Q was inside the inner ellipse
//   - 1 if Q was outside the outer ellipse
//   - smoothly varying from 0 to 1 in between
float2 ClipSuperellipse( float2 Q,			// Point on the xy plane
						 float4 aAbB,		// Dimensions of superellipses
						 float2 rounds )	// Same roundness for both ellipses
{
	float2 qr, Qabs = abs(Q);				// Project to +x +y quadrant

	float2 bx_Bx = Qabs.x * aAbB.zw;
	float2 ay_Ay = Qabs.y * aAbB.xy;

	qr.x = pow( pow( bx_Bx.x, rounds.x ) + pow( ay_Ay.x, rounds.x ), rounds.y );  // rounds.x = 2 / roundness
	qr.y = pow( pow( bx_Bx.y, rounds.x ) + pow( ay_Ay.y, rounds.x ), rounds.y );  // rounds.y = -roundness/2

	return qr * aAbB.xy * aAbB.zw;
}

// Volumetric light shaping
//
// Inputs:
//   - the point being shaded, in the local light space
//   - all information about the light shaping, including z smooth depth
//     clipping, superellipse xy shaping, and distance falloff.
// Return value:
//   - attenuation factor based on the falloff and shaping
float uberlight(float3 PL,					// Point in light space

				float3 smoothEdge0,			// edge0 for three smooth steps
				float3 smoothEdge1,			// edge1 for three smooth steps
				float3 smoothOneOverWidth,	// width of three smooth steps

				float2 shear,				// shear in X and Y
				float4 aAbB,				// Superellipse dimensions
				float2 rounds )				// two functions of roundness packed together
{
	float2 qr = ClipSuperellipse( (PL.xy / PL.z) - shear, aAbB, rounds );

	smoothEdge0.x = qr.x;					// Fill in the dynamic parts of the smoothsteps
	smoothEdge1.x = qr.y;					// The other components are pre-computed outside of the shader
	smoothOneOverWidth.x = 1.0f / ( qr.y - qr.x );
	float3 x = float3( 1, PL.z, PL.z );

	float3 atten3 = smoothstep3( smoothEdge0, smoothEdge1, smoothOneOverWidth, x );

	// Modulate the three resulting attenuations (flipping the sense of the attenuation from the superellipse and the far clip)
	return (1.0f - atten3.x) * atten3.y * (1.0f - atten3.z);
}

#if defined( _X360 )
	#define FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION ( 720.0f )
#elif defined( _PS3 )
	#define FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION ( 864.0f )
#else
	#define FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION ( 1024.0f )
#endif

// JasonM - TODO: remove this simpleton version
float DoShadow( sampler DepthSampler, float4 texCoord )
{
	const float g_flShadowBias = 0.0005f;
	float2 uoffset = float2( 0.5f/FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION, 0.0f );
	float2 voffset = float2( 0.0f, 0.5f/FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION );
	float3 projTexCoord = texCoord.xyz / texCoord.w;
	float4 flashlightDepth = float4(	tex2D( DepthSampler, projTexCoord.xy + uoffset + voffset ).x,
										tex2D( DepthSampler, projTexCoord.xy + uoffset - voffset ).x,
										tex2D( DepthSampler, projTexCoord.xy - uoffset + voffset ).x,
										tex2D( DepthSampler, projTexCoord.xy - uoffset - voffset ).x	);

#	if ( defined( REVERSE_DEPTH_ON_X360 ) )
	{
		flashlightDepth = 1.0f - flashlightDepth;
	}
#	endif

	float shadowed = 0.0f;
	float z = texCoord.z/texCoord.w;
	float4 dz = float4(z,z,z,z) - (flashlightDepth + float4( g_flShadowBias, g_flShadowBias, g_flShadowBias, g_flShadowBias));
	float4 shadow = float4(0.25f,0.25f,0.25f,0.25f);

	if( dz.x <= 0.0f )
		shadowed += shadow.x;
	if( dz.y <= 0.0f )
		shadowed += shadow.y;
	if( dz.z <= 0.0f )
		shadowed += shadow.z;
	if( dz.w <= 0.0f )
		shadowed += shadow.w;

	return shadowed;
}


float DoShadowNvidiaRAWZOneTap( sampler DepthSampler, const float4 shadowMapPos )
{
	float ooW = 1.0f / shadowMapPos.w;								// 1 / w
	float3 shadowMapCenter_objDepth = shadowMapPos.xyz * ooW;		// Do both projections at once

	float2 shadowMapCenter = shadowMapCenter_objDepth.xy;			// Center of shadow filter
	float objDepth = shadowMapCenter_objDepth.z;					// Object depth in shadow space

	float fDepth = dot(tex2D(DepthSampler, shadowMapCenter).arg, float3(0.996093809371817670572857294849, 0.0038909914428586627756752238080039, 1.5199185323666651467481343000015e-5));

	return fDepth > objDepth;
}


float DoShadowNvidiaRAWZ( sampler DepthSampler, const float4 shadowMapPos )
{
	float fE = 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;	 // Epsilon

	float ooW = 1.0f / shadowMapPos.w;								// 1 / w
	float3 shadowMapCenter_objDepth = shadowMapPos.xyz * ooW;		// Do both projections at once

	float2 shadowMapCenter = shadowMapCenter_objDepth.xy;			// Center of shadow filter
	float objDepth = shadowMapCenter_objDepth.z;					// Object depth in shadow space

	float4 vDepths;
	vDepths.x = dot(tex2D(DepthSampler, shadowMapCenter + float2(  fE,  fE )).arg, float3(0.996093809371817670572857294849, 0.0038909914428586627756752238080039, 1.5199185323666651467481343000015e-5));
	vDepths.y = dot(tex2D(DepthSampler, shadowMapCenter + float2( -fE,  fE )).arg, float3(0.996093809371817670572857294849, 0.0038909914428586627756752238080039, 1.5199185323666651467481343000015e-5));
	vDepths.z = dot(tex2D(DepthSampler, shadowMapCenter + float2(  fE, -fE )).arg, float3(0.996093809371817670572857294849, 0.0038909914428586627756752238080039, 1.5199185323666651467481343000015e-5));
	vDepths.w = dot(tex2D(DepthSampler, shadowMapCenter + float2( -fE, -fE )).arg, float3(0.996093809371817670572857294849, 0.0038909914428586627756752238080039, 1.5199185323666651467481343000015e-5));

	return dot(vDepths > objDepth.xxxx, float4(0.25, 0.25, 0.25, 0.25));
}

float DoShadowNvidiaCheap( sampler DepthSampler, const float4 shadowMapPos )
{
	float fTexelEpsilon = 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;

	float ooW = 1.0f / shadowMapPos.w;								// 1 / w
	float3 shadowMapCenter_objDepth = shadowMapPos.xyz * ooW;		// Do both projections at once

	float2 shadowMapCenter = shadowMapCenter_objDepth.xy;			// Center of shadow filter
	float objDepth = shadowMapCenter_objDepth.z;					// Object depth in shadow space

	float4 vTaps;
	vTaps.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  fTexelEpsilon,  fTexelEpsilon), objDepth, 1 ) ).x;
	vTaps.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2( -fTexelEpsilon,  fTexelEpsilon), objDepth, 1 ) ).x;
	vTaps.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  fTexelEpsilon, -fTexelEpsilon), objDepth, 1 ) ).x;
	vTaps.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2( -fTexelEpsilon, -fTexelEpsilon), objDepth, 1 ) ).x;

	return dot(vTaps, float4(0.25, 0.25, 0.25, 0.25));
}

float DoShadowNvidiaPCF3x3Box( sampler DepthSampler, const float3 vProjCoords )
{
	float fTexelEpsilon = 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;

	//float ooW = 1.0f / shadowMapPos.w;								// 1 / w
	//float3 shadowMapCenter_objDepth = shadowMapPos.xyz * ooW;		// Do both projections at once
	float3 shadowMapCenter_objDepth = vProjCoords;

	float2 shadowMapCenter = shadowMapCenter_objDepth.xy;			// Center of shadow filter
	float objDepth = shadowMapCenter_objDepth.z;					// Object depth in shadow space

	float4 vOneTaps;
	vOneTaps.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  fTexelEpsilon,  fTexelEpsilon ), objDepth, 1 ) ).x;
	vOneTaps.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2( -fTexelEpsilon,  fTexelEpsilon ), objDepth, 1 ) ).x;
	vOneTaps.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  fTexelEpsilon, -fTexelEpsilon ), objDepth, 1 ) ).x;
	vOneTaps.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2( -fTexelEpsilon, -fTexelEpsilon ), objDepth, 1 ) ).x;
	float flOneTaps = dot( vOneTaps, float4(1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f));

	float4 vTwoTaps;
	vTwoTaps.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  fTexelEpsilon,  0 ), objDepth, 1 ) ).x;
	vTwoTaps.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2( -fTexelEpsilon,  0 ), objDepth, 1 ) ).x;
	vTwoTaps.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  0, -fTexelEpsilon ), objDepth, 1 ) ).x;
	vTwoTaps.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  0, -fTexelEpsilon ), objDepth, 1 ) ).x;
	float flTwoTaps = dot( vTwoTaps, float4(1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f));

	float flCenterTap = tex2Dproj( DepthSampler, float4( shadowMapCenter, objDepth, 1 ) ).x * (1.0f / 9.0f);

	// Sum all 9 Taps
	return flOneTaps + flTwoTaps + flCenterTap;
}

// 1 2 1
// 2 4 2
// 1 2 1
#ifdef _PS3
// Tweaked for good code gen with the SCE Cg compiler.
half DoShadowNvidiaPCF3x3Gaussian( sampler DepthSampler, const float3 shadowMapPos )
{
	float fTexelEpsilon = 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;

	float3 shadowMapCenter_objDepth = shadowMapPos.xyz;

	float3 shadowMapCenter = shadowMapCenter_objDepth.xyz;			// Center of shadow filter
		
	float4 vUV0 = shadowMapCenter.xyzx + float4( fTexelEpsilon,  fTexelEpsilon, 0.0f, -fTexelEpsilon );		
	float4 vUV1 = shadowMapCenter.xyzx + float4( fTexelEpsilon, -fTexelEpsilon, 0.0f, -fTexelEpsilon );		
	
	half4 vOneTaps;
	vOneTaps.x = h4tex2D( DepthSampler, vUV0.xyz ).x;
	vOneTaps.y = h4tex2D( DepthSampler, vUV0.wyz ).y;
	vOneTaps.z = h4tex2D( DepthSampler, vUV1.xyz ).z;
	vOneTaps.w = h4tex2D( DepthSampler, vUV1.wyz ).w;
	half flSum = dot( vOneTaps, half4(1.0f, 1.0f, 1.0f, 1.0f));
	
	float4 vUV2 = shadowMapCenter.xyzx + float4( fTexelEpsilon,  0.0f, 0.0f, -fTexelEpsilon );		
	float4 vUV3 = shadowMapCenter.xyzy + float4( 0.0f, -fTexelEpsilon, 0.0f,  fTexelEpsilon );		

	half4 vTwoTaps;
	vTwoTaps.x = h4tex2D( DepthSampler, vUV2.xyz ).x;
	vTwoTaps.y = h4tex2D( DepthSampler, vUV2.wyz ).y;
	vTwoTaps.z = h4tex2D( DepthSampler, vUV3.xyz ).z;
	vTwoTaps.w = h4tex2D( DepthSampler, vUV3.xwz ).w;
	flSum += dot( vTwoTaps, half4(2.0f, 2.0f, 2.0f, 2.0f));

	flSum += tex2D( DepthSampler, shadowMapCenter ).x * half(4.0f);

	// Sum all 9 Taps
	return flSum * (1.0h / 16.0h);
}
#else
float DoShadowNvidiaPCF3x3Gaussian( sampler DepthSampler, const float3 shadowMapPos )
{
	float fTexelEpsilon = 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;

	float3 shadowMapCenter_objDepth = shadowMapPos.xyz;

	float2 shadowMapCenter = shadowMapCenter_objDepth.xy;			// Center of shadow filter
	float objDepth = shadowMapCenter_objDepth.z;					// Object depth in shadow space

	float4 vOneTaps;
	vOneTaps.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  fTexelEpsilon,  fTexelEpsilon ), objDepth, 1 ) ).x;
	vOneTaps.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2( -fTexelEpsilon,  fTexelEpsilon ), objDepth, 1 ) ).x;
	vOneTaps.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  fTexelEpsilon, -fTexelEpsilon ), objDepth, 1 ) ).x;
	vOneTaps.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2( -fTexelEpsilon, -fTexelEpsilon ), objDepth, 1 ) ).x;
	float flOneTaps = dot( vOneTaps, float4(1.0f / 16.0f, 1.0f / 16.0f, 1.0f / 16.0f, 1.0f / 16.0f));

	float4 vTwoTaps;
	vTwoTaps.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  fTexelEpsilon,  0 ), objDepth, 1 ) ).x;
	vTwoTaps.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2( -fTexelEpsilon,  0 ), objDepth, 1 ) ).x;
	vTwoTaps.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  0, -fTexelEpsilon ), objDepth, 1 ) ).x;
	vTwoTaps.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  0,  fTexelEpsilon ), objDepth, 1 ) ).x;
	float flTwoTaps = dot( vTwoTaps, float4(2.0f / 16.0f, 2.0f / 16.0f, 2.0f / 16.0f, 2.0f / 16.0f));

	float flCenterTap = tex2Dproj( DepthSampler, float4( shadowMapCenter, objDepth, 1 ) ).x * float(4.0f / 16.0f);

	// Sum all 9 Taps
	return flOneTaps + flTwoTaps + flCenterTap;
}
#endif

//
//	1	4	7	4	1
//	4	20	33	20	4
//	7	33	55	33	7
//	4	20	33	20	4
//	1	4	7	4	1
//
#ifdef _PS3
// Tweaked for good code gen with the SCE Cg compiler.
float DoShadowNvidiaPCF5x5Gaussian( sampler DepthSampler, const float3 vProjCoords )
{
	float flTexelEpsilon    = 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;
	float flTwoTexelEpsilon = 2.0f * flTexelEpsilon;

	//float ooW = 1.0f / shadowMapPos.w;								// 1 / w
	float3 shadowMapCenter_objDepth = vProjCoords;//shadowMapPos.xyz * ooW;		// Do both projections at once

	float2 shadowMapCenter = shadowMapCenter_objDepth.xy;			// Center of shadow filter
	float objDepth = shadowMapCenter_objDepth.z;					// Object depth in shadow space

	half4 c0 = half4( 1.0f / 331.0f, 7.0f / 331.0f, 4.0f / 331.0f, 20.0f / 331.0f );
	half4 c1 = half4( 33.0f / 331.0f, 55.0f / 331.0f, -flTexelEpsilon, 0.0f );
	float4 c2 = float4( flTwoTexelEpsilon, -flTwoTexelEpsilon, 0.0f, flTexelEpsilon );
	float4 c3 = float4( flTexelEpsilon, -flTexelEpsilon, flTwoTexelEpsilon, -flTwoTexelEpsilon );

	half4 vOneTaps;
	vOneTaps.x = tex2D( DepthSampler, float3( shadowMapCenter + c2.xx, objDepth ) ).x;	//  2  2
	vOneTaps.y = tex2D( DepthSampler, float3( shadowMapCenter + c2.yx, objDepth ) ).y;	// -2  2
	vOneTaps.z = tex2D( DepthSampler, float3( shadowMapCenter + c2.xy, objDepth ) ).z;	//  2 -2
	vOneTaps.w = tex2D( DepthSampler, float3( shadowMapCenter + c2.yy, objDepth ) ).w;	// -2 -2
	half flSum = dot( vOneTaps, c0.xxxx );

	half4 vSevenTaps;
	vSevenTaps.x = tex2D( DepthSampler, float3( shadowMapCenter + c2.xz, objDepth ) ).x;	//  2 0
	vSevenTaps.y = tex2D( DepthSampler, float3( shadowMapCenter + c2.yz, objDepth ) ).y;	// -2 0
	vSevenTaps.z = tex2D( DepthSampler, float3( shadowMapCenter + c2.zx, objDepth ) ).z;	// 0 2
	vSevenTaps.w = tex2D( DepthSampler, float3( shadowMapCenter + c2.zy, objDepth ) ).w;	// 0 -2
	flSum += dot( vSevenTaps, c0.yyyy );

	half4 vFourTapsA, vFourTapsB;
	vFourTapsA.x = tex2D( DepthSampler, float3( shadowMapCenter + c2.xw, objDepth ) ).x;	// 2 1
	vFourTapsA.y = tex2D( DepthSampler, float3( shadowMapCenter + c2.wx, objDepth ) ).y;	// 1 2
	vFourTapsA.z = tex2D( DepthSampler, float3( shadowMapCenter + c3.yz, objDepth ) ).z;	// -1 2
	vFourTapsA.w = tex2D( DepthSampler, float3( shadowMapCenter + c3.wx, objDepth ) ).w;	// -2 1

	vFourTapsB.x = tex2D( DepthSampler, float3( shadowMapCenter + c3.wy, objDepth ) ).x;	// -2 -1
	vFourTapsB.y = tex2D( DepthSampler, float3( shadowMapCenter + c3.yw, objDepth ) ).y;	// -1 -2
	vFourTapsB.z = tex2D( DepthSampler, float3( shadowMapCenter + c3.xw, objDepth ) ).z;	// 1 -2
	vFourTapsB.w = tex2D( DepthSampler, float3( shadowMapCenter + c3.zy, objDepth ) ).w;	// 2 -1
	flSum += dot( vFourTapsA, c0.zzzz );
	flSum += dot( vFourTapsB, c0.zzzz );

	half4 v20Taps;
	v20Taps.x = tex2D( DepthSampler, float3( shadowMapCenter + c3.xx, objDepth ) ).x;	// 1 1
	v20Taps.y = tex2D( DepthSampler, float3( shadowMapCenter + c3.yx, objDepth ) ).y;	// -1 1
	v20Taps.z = tex2D( DepthSampler, float3( shadowMapCenter + c3.xy, objDepth ) ).z;	// 1 -1
	v20Taps.w = tex2D( DepthSampler, float3( shadowMapCenter + c3.yy, objDepth ) ).w;	// -1 -1
	flSum += dot( v20Taps, c0.wwww );

	half4 v33Taps;
	v33Taps.x = tex2D( DepthSampler, float3( shadowMapCenter + c2.wz, objDepth ) ).x;	// 1 0
	v33Taps.y = tex2D( DepthSampler, float3( shadowMapCenter + c1.zw, objDepth ) ).y;	// -1 0
	v33Taps.z = tex2D( DepthSampler, float3( shadowMapCenter + c1.wz, objDepth ) ).z;	// 0 -1
	v33Taps.w = tex2D( DepthSampler, float3( shadowMapCenter + c2.zw, objDepth ) ).w;	// 0 1
	flSum += dot( v33Taps, c1.xxxx );

	flSum += tex2D( DepthSampler, float3( shadowMapCenter, objDepth ) ).x * c1.y;

	return flSum;
}
#else
float DoShadowNvidiaPCF5x5GaussianPC( sampler DepthSampler, const float3 vProjCoords )
{
	float flTexelEpsilon    = 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;
	float flTwoTexelEpsilon = 2.0f * flTexelEpsilon;

	//float ooW = 1.0f / shadowMapPos.w;								// 1 / w
	float3 shadowMapCenter_objDepth = vProjCoords;//shadowMapPos.xyz * ooW;		// Do both projections at once

	float2 shadowMapCenter = shadowMapCenter_objDepth.xy;			// Center of shadow filter
	float objDepth = shadowMapCenter_objDepth.z;					// Object depth in shadow space

	float4 c0 = float4( 1.0f / 331.0f, 7.0f / 331.0f, 4.0f / 331.0f, 20.0f / 331.0f );
	float4 c1 = float4( 33.0f / 331.0f, 55.0f / 331.0f, -flTexelEpsilon, 0.0f );
	float4 c2 = float4( flTwoTexelEpsilon, -flTwoTexelEpsilon, 0.0f, flTexelEpsilon );
	float4 c3 = float4( flTexelEpsilon, -flTexelEpsilon, flTwoTexelEpsilon, -flTwoTexelEpsilon );

	float4 vOneTaps;
	vOneTaps.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.xx, objDepth, 1 ) ).x;	//  2  2
	vOneTaps.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.yx, objDepth, 1 ) ).x;	// -2  2
	vOneTaps.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.xy, objDepth, 1 ) ).x;	//  2 -2
	vOneTaps.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.yy, objDepth, 1 ) ).x;	// -2 -2
	float flSum = dot( vOneTaps, c0.xxxx );

	float4 vSevenTaps;
	vSevenTaps.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.xz, objDepth, 1 ) ).x;	//  2 0
	vSevenTaps.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.yz, objDepth, 1 ) ).x;	// -2 0
	vSevenTaps.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.zx, objDepth, 1 ) ).x;	// 0 2
	vSevenTaps.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.zy, objDepth, 1 ) ).x;	// 0 -2
	flSum += dot( vSevenTaps, c0.yyyy );

	float4 vFourTapsA, vFourTapsB;
	vFourTapsA.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.xw, objDepth, 1 ) ).x;	// 2 1
	vFourTapsA.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.wx, objDepth, 1 ) ).x;	// 1 2
	vFourTapsA.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.yz, objDepth, 1 ) ).x;	// -1 2
	vFourTapsA.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.wx, objDepth, 1 ) ).x;	// -2 1
	vFourTapsB.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.wy, objDepth, 1 ) ).x;	// -2 -1
	vFourTapsB.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.yw, objDepth, 1 ) ).x;	// -1 -2
	vFourTapsB.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.xw, objDepth, 1 ) ).x;	// 1 -2
	vFourTapsB.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.zy, objDepth, 1 ) ).x;	// 2 -1
	flSum += dot( vFourTapsA, c0.zzzz );
	flSum += dot( vFourTapsB, c0.zzzz );

	float4 v20Taps;
	v20Taps.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.xx, objDepth, 1 ) ).x;	// 1 1
	v20Taps.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.yx, objDepth, 1 ) ).x;	// -1 1
	v20Taps.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.xy, objDepth, 1 ) ).x;	// 1 -1
	v20Taps.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.yy, objDepth, 1 ) ).x;	// -1 -1
	flSum += dot( v20Taps, c0.wwww );

	float4 v33Taps;
	v33Taps.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.wz, objDepth, 1 ) ).x;	// 1 0
	v33Taps.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + c1.zw, objDepth, 1 ) ).x;	// -1 0
	v33Taps.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + c1.wz, objDepth, 1 ) ).x;	// 0 -1
	v33Taps.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.zw, objDepth, 1 ) ).x;	// 0 1
	flSum += dot( v33Taps, c1.xxxx );

	flSum += tex2Dproj( DepthSampler, float4( shadowMapCenter, objDepth, 1 ) ).x * c1.y;
	
	flSum = pow( flSum, 1.4f );

	return flSum;
}
#endif

float DoShadowATICheap( sampler DepthSampler, const float4 shadowMapPos )
{
    float2 shadowMapCenter = shadowMapPos.xy/shadowMapPos.w;
	float objDepth = shadowMapPos.z / shadowMapPos.w;
	float fSampleDepth = tex2D( DepthSampler, shadowMapCenter ).x;

	objDepth = min( objDepth, 0.99999 ); //HACKHACK: On 360, surfaces at or past the far flashlight plane have an abrupt cutoff. This is temp until a smooth falloff is implemented

	return fSampleDepth > objDepth;
}


// Smooth filter using ATI Fetch 4
float DoShadowATIFetch4( sampler DepthSampler, const float3 vProjCoords )
{
	// This should only ever get run on a ps_3_0 part
	#if ( !defined( SHADER_MODEL_PS_3_0 ) )
	{
		return 1.0f;
	}
	#endif

	float4  shadowMapVals[ 4 ];
	float4	shadowMapWeights[ 4 ];
	// Important: This shader was originally in DoTA. To get this shader working in Portal2, I had to eliminate the -.5f offsets and weird swizzle.
	// I'm not positive, but the min/mag filter settings must differ between the two titles, which might account for the difference?
	float4  quadOffsets[ 4 ] = 
	{
		{ -1.0f, -1.0f, 0, 0 },
		{  1.0f, -1.0f, 0, 0 },
		{ -1.0f,  1.0f, 0, 0 },
		{  1.0f,  1.0f, 0, 0 },		
	};

	float3 shadowMapCenter_objDepth = vProjCoords;
	float2 shadowMapCenter = shadowMapCenter_objDepth.xy;			// Center of shadow filter
	float objDepth = shadowMapCenter_objDepth.z;					// Object depth in shadow space
	float4 vFullTexelOffset = float4( 1.0 / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION, 1.0 / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION, 0.0, 0.0 );

	float2 vTexRes = float2( FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION, FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION );

	// Fetch 4 2x2 quads
	shadowMapVals[ 0 ] = tex2D( DepthSampler, shadowMapCenter + ( vFullTexelOffset.xy * quadOffsets[ 0 ].xy ) );
	shadowMapVals[ 1 ] = tex2D( DepthSampler, shadowMapCenter + ( vFullTexelOffset.xy * quadOffsets[ 1 ].xy ) );
	shadowMapVals[ 2 ] = tex2D( DepthSampler, shadowMapCenter + ( vFullTexelOffset.xy * quadOffsets[ 2 ].xy ) );
	shadowMapVals[ 3 ] = tex2D( DepthSampler, shadowMapCenter + ( vFullTexelOffset.xy * quadOffsets[ 3 ].xy ) );

	// Fraction component of projected coordinates
	float2 pFrac = frac( shadowMapCenter * vTexRes );

	shadowMapWeights[ 0 ] = float4(	1,			1 - pFrac.x,	1,			1 - pFrac.x );
	shadowMapWeights[ 1 ] = float4( pFrac.x,	1,				pFrac.x,	1 );
	shadowMapWeights[ 2 ] = float4( 1,			1 - pFrac.x,	1,			1 - pFrac.x);
	shadowMapWeights[ 3 ] = float4( pFrac.x,	1,				pFrac.x,	1 );

	shadowMapWeights[ 0 ] *= float4(  1 - pFrac.y,  1,       1,       1 - pFrac.y  );
	shadowMapWeights[ 1 ] *= float4(  1 - pFrac.y,  1,       1,       1 - pFrac.y  );
	shadowMapWeights[ 2 ] *= float4(  1,			pFrac.y, pFrac.y, 1			   );
	shadowMapWeights[ 3 ] *= float4(  1,			pFrac.y, pFrac.y, 1            );

	// Projective distance from z plane in view coords
	float flDist = objDepth - 0.005;
	float4 dist = float4( flDist, flDist, flDist, flDist );

	float4 inLight = ( dist < shadowMapVals[ 0 ] );
	float percentInLight = dot( inLight, shadowMapWeights[ 0 ] );

	inLight = ( dist < shadowMapVals[ 1 ] );
	percentInLight += dot( inLight, shadowMapWeights[ 1 ] );

	inLight = ( dist < shadowMapVals[ 2 ] );
	percentInLight += dot( inLight, shadowMapWeights[ 2 ] );

	inLight = ( dist < shadowMapVals[ 3 ] );
	percentInLight += dot( inLight, shadowMapWeights[ 3 ] );

	// Sum of weights is 9 since border taps are bilinearly filtered
	return ( 1.0f / 9.0f ) * percentInLight;
}

// Bilinear Percentage Closer Filtering, fetching depths and manually doing the four compares and the bilerp
float DoShadowATIBilinear( sampler DepthSampler, const float3 vProjCoords )
{
	float2 vPositionLs = vProjCoords.xy;
	float flComparisonDepth = vProjCoords.z;
	
	// Emulate bilinear PCF - shader originally from source2 (src/shaders/include/sun_shadowing.fxc).
	float flSunShadowingShadowTextureWidth = FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;
	float flSunShadowingShadowTextureHeight = FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;
	float flSunShadowingInvShadowTextureWidth = 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;
	float flSunShadowingInvShadowTextureHeight = 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;
	
	float2 vFracPositionLs = frac( vPositionLs * float2( flSunShadowingShadowTextureWidth, flSunShadowingShadowTextureHeight ) );
	float2 vSamplePositionLs = vPositionLs - vFracPositionLs * float2( flSunShadowingInvShadowTextureWidth, flSunShadowingInvShadowTextureHeight );
	
	float4 vCmpSamples;		
	vCmpSamples.x = tex2D( DepthSampler, vSamplePositionLs + float2( 0.0f * flSunShadowingInvShadowTextureWidth, 0.0f * flSunShadowingInvShadowTextureHeight ) ).x;
	vCmpSamples.y = tex2D( DepthSampler, vSamplePositionLs + float2( 1.0f * flSunShadowingInvShadowTextureWidth, 0.0f * flSunShadowingInvShadowTextureHeight ) ).x;
	vCmpSamples.z = tex2D( DepthSampler, vSamplePositionLs + float2( 0.0f * flSunShadowingInvShadowTextureWidth, 1.0f * flSunShadowingInvShadowTextureHeight ) ).x;
	vCmpSamples.w = tex2D( DepthSampler, vSamplePositionLs + float2( 1.0f * flSunShadowingInvShadowTextureWidth, 1.0f * flSunShadowingInvShadowTextureHeight ) ).x;

	vCmpSamples = vCmpSamples > flComparisonDepth;
	
	float4 vFactors = float4( ( 1.0f - vFracPositionLs.x ) * ( 1.0f - vFracPositionLs.y ), vFracPositionLs.x   * ( 1.0f - vFracPositionLs.y ),
							  ( 1.0f - vFracPositionLs.x ) *          vFracPositionLs.y,   vFracPositionLs.x   *          vFracPositionLs.y );
		 		
	return dot( vCmpSamples, vFactors );
}

// Poisson disc, randomly rotated at different UVs
float DoShadowPoisson16Sample( sampler DepthSampler, sampler RandomRotationSampler, const float3 vProjCoords, const float2 vScreenPos, const float4 vShadowTweaks, bool bNvidiaHardwarePCF, bool bFetch4 )
{
	float2 vPoissonOffset[8] = { float2(  0.3475f,  0.0042f ), float2(  0.8806f,  0.3430f ), float2( -0.0041f, -0.6197f ), float2(  0.0472f,  0.4964f ),
								 float2( -0.3730f,  0.0874f ), float2( -0.9217f, -0.3177f ), float2( -0.6289f,  0.7388f ), float2(  0.5744f, -0.7741f ) };

	float flScaleOverMapSize = vShadowTweaks.x * 2;		// Tweak parameters to shader
	float2 vNoiseOffset = vShadowTweaks.zw;
	float4 vLightDepths = 0, accum = 0.0f;
	float2 rotOffset = 0;

	float2 shadowMapCenter = vProjCoords.xy;			// Center of shadow filter
	float objDepth = min( vProjCoords.z, 0.99999 );		// Object depth in shadow space

	// 2D Rotation Matrix setup
	float3 RMatTop = 0, RMatBottom = 0;
#if defined(SHADER_MODEL_PS_2_0) || defined(SHADER_MODEL_PS_2_B) || defined(SHADER_MODEL_PS_3_0)
	RMatTop.xy = tex2D( RandomRotationSampler, cFlashlightScreenScale.xy * (vScreenPos * 0.5 + 0.5) + vNoiseOffset).xy * 2.0 - 1.0;
	RMatBottom.xy = float2(-1.0, 1.0) * RMatTop.yx;	// 2x2 rotation matrix in 4-tuple
#endif

	RMatTop *= flScaleOverMapSize;				// Scale up kernel while accounting for texture resolution
	RMatBottom *= flScaleOverMapSize;

	RMatTop.z = shadowMapCenter.x;				// To be added in d2adds generated below
	RMatBottom.z = shadowMapCenter.y;
	
	float fResult = 0.0f;

	if ( bNvidiaHardwarePCF )
	{
		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[0].xy ) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[0].xy ) + RMatBottom.z;
		vLightDepths.x += tex2Dproj( DepthSampler, float4(rotOffset, objDepth, 1) ).x;

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[1].xy ) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[1].xy ) + RMatBottom.z;
		vLightDepths.y += tex2Dproj( DepthSampler, float4(rotOffset, objDepth, 1) ).x;

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[2].xy ) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[2].xy ) + RMatBottom.z;
		vLightDepths.z += tex2Dproj( DepthSampler, float4(rotOffset, objDepth, 1) ).x;

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[3].xy ) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[3].xy ) + RMatBottom.z;
		vLightDepths.w += tex2Dproj( DepthSampler, float4(rotOffset, objDepth, 1) ).x;

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[4].xy ) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[4].xy ) + RMatBottom.z;
		vLightDepths.x += tex2Dproj( DepthSampler, float4(rotOffset, objDepth, 1) ).x;

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[5].xy ) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[5].xy ) + RMatBottom.z;
		vLightDepths.y += tex2Dproj( DepthSampler, float4(rotOffset, objDepth, 1) ).x;

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[6].xy ) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[6].xy ) + RMatBottom.z;
		vLightDepths.z += tex2Dproj( DepthSampler, float4(rotOffset, objDepth, 1) ).x;

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[7].xy ) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[7].xy ) + RMatBottom.z;
		vLightDepths.w += tex2Dproj( DepthSampler, float4(rotOffset, objDepth, 1) ).x;

		// This should actually be float4( 0.125, 0.125, 0.125, 0.125) but we've tuned so many shots in the SFM
		// relying on this bug that it doesn't seem right to fix until we have done something like move
		// this code out to a staging branch for a shipping game.
		// This is certainly one source of difference between ATI and nVidia in SFM layoffs
		return dot( vLightDepths, float4( 0.25, 0.25, 0.25, 0.25) );
	}
	else if ( bFetch4 )
	{
		for( int i=0; i<8; i++ )
		{
			rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[i].xy ) + RMatTop.z;
			rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[i].xy ) + RMatBottom.z;
			vLightDepths = tex2D( DepthSampler, rotOffset.xy );
			accum += (vLightDepths > objDepth.xxxx);
		}

		return dot( accum, float4( 1.0f/32.0f, 1.0f/32.0f, 1.0f/32.0f, 1.0f/32.0f) );
	}
	else	// ATI vanilla hardware shadow mapping
	{
		for( int i=0; i<2; i++ )
		{
			rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[4*i+0].xy ) + RMatTop.z;
			rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[4*i+0].xy ) + RMatBottom.z;
			vLightDepths.x = tex2D( DepthSampler, rotOffset.xy ).x;

			rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[4*i+1].xy ) + RMatTop.z;
			rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[4*i+1].xy ) + RMatBottom.z;
			vLightDepths.y = tex2D( DepthSampler, rotOffset.xy ).x;

			rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[4*i+2].xy ) + RMatTop.z;
			rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[4*i+2].xy ) + RMatBottom.z;
			vLightDepths.z = tex2D( DepthSampler, rotOffset.xy ).x;

			rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[4*i+3].xy ) + RMatTop.z;
			rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[4*i+3].xy ) + RMatBottom.z;
			vLightDepths.w = tex2D( DepthSampler, rotOffset.xy ).x;

			accum += (vLightDepths > objDepth.xxxx);
		}

		return dot( accum, float4( 0.125, 0.125, 0.125, 0.125 ) );
	}
}

#if defined( _X360 )

// Poisson disc, randomly rotated at different UVs
float DoShadow360Simple( sampler DepthSampler, const float3 vProjCoords )
{
	float fLOD;
	float2 shadowMapCenter = vProjCoords.xy;			// Center of shadow filter
	float objDepth = min( vProjCoords.z, 0.99999 );		// Object depth in shadow space

#if defined( REVERSE_DEPTH_ON_X360 )
	objDepth = 1.0f - objDepth;
#endif	

	float4 vSampledDepths, vWeights;

	asm 
	{
		tfetch2D vSampledDepths.x___, shadowMapCenter, DepthSampler, OffsetX = -0.5, OffsetY = -0.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
		tfetch2D vSampledDepths._x__, shadowMapCenter, DepthSampler, OffsetX =  0.5, OffsetY = -0.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
		tfetch2D vSampledDepths.__x_, shadowMapCenter, DepthSampler, OffsetX = -0.5, OffsetY =  0.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
		tfetch2D vSampledDepths.___x, shadowMapCenter, DepthSampler, OffsetX =  0.5, OffsetY =  0.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
	};

	asm
	{
		getWeights2D vWeights, shadowMapCenter.xy, DepthSampler, MagFilter=linear, MinFilter=linear, UseComputedLOD=false, UseRegisterLOD=false
	};

	vWeights = float4( (1-vWeights.x)*(1-vWeights.y), vWeights.x*(1-vWeights.y), (1-vWeights.x)*vWeights.y, vWeights.x*vWeights.y );

#if defined( REVERSE_DEPTH_ON_X360 )
	float4 vCompare = (vSampledDepths < objDepth.xxxx);
#else
	float4 vCompare = (vSampledDepths > objDepth.xxxx);
#endif

	return dot( vCompare, vWeights );
}

float DoShadowXbox4x4Samples( sampler DepthSampler, const float3 vProjCoords, float NdotL )
{
	float2 vShadowMapCenter = vProjCoords.xy + float2( .5f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION, .5f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION );			// Center of shadow filter
	// This shader assumes REVERSE_DEPTH_ON_X360 is always defined.
	float flObjDepth = 1.0f - min( vProjCoords.z, 0.99999f );		// Object depth in shadow space

	// projective distance from z plane in view coords
	float4 vDist4 = float4( flObjDepth, flObjDepth, flObjDepth, flObjDepth );

	//fraction component of projected coordinates; here FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION represents the shadowmap size
	float2 vTexRes = float2( FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION, FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION );
	float2 vFrac = frac( vShadowMapCenter * vTexRes );
	float4 vWeights = float4( vFrac.x, vFrac.y, 1.0f - vFrac.x, 1.0f - vFrac.y );
	
	float flPercentInLight;

	[isolate]
	{
		float4 vShadowMapVals, vInLight;
		asm 
		{
			tfetch2D vShadowMapVals.x___, vShadowMapCenter, DepthSampler, OffsetX = -1.0, OffsetY = -2.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			tfetch2D vShadowMapVals._x__, vShadowMapCenter, DepthSampler, OffsetX = -2.0, OffsetY = -1.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			tfetch2D vShadowMapVals.__x_, vShadowMapCenter, DepthSampler, OffsetX = -1.0, OffsetY = -1.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			tfetch2D vShadowMapVals.___x, vShadowMapCenter, DepthSampler, OffsetX = -2.0, OffsetY = -2.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			sgt vInLight, vDist4, vShadowMapVals
		};
		float4 vShadowMapWeights = float4( vWeights.w, vWeights.z, 1, vWeights.z * vWeights.w );
		flPercentInLight = dot( vInLight, vShadowMapWeights );
	}
	
	[isolate]
	{
		float4 vShadowMapVals, vInLight;
		asm 
		{
			tfetch2D vShadowMapVals.x___, vShadowMapCenter, DepthSampler, OffsetX =  1.0, OffsetY = -2.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			tfetch2D vShadowMapVals._x__, vShadowMapCenter, DepthSampler, OffsetX =  0.0, OffsetY = -1.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			tfetch2D vShadowMapVals.__x_, vShadowMapCenter, DepthSampler, OffsetX =  1.0, OffsetY = -1.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			tfetch2D vShadowMapVals.___x, vShadowMapCenter, DepthSampler, OffsetX =  0.0, OffsetY = -2.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			sgt vInLight, vDist4, vShadowMapVals
		};
		float4 vShadowMapWeights = float4( vWeights.x * vWeights.w, 1, vWeights.x, vWeights.w );
		flPercentInLight += dot( vInLight, vShadowMapWeights );
	}

	[isolate]
	{
		float4 vShadowMapVals, vInLight;
		asm 
		{
			tfetch2D vShadowMapVals.x___, vShadowMapCenter, DepthSampler, OffsetX = -1.0, OffsetY =  0.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			tfetch2D vShadowMapVals._x__, vShadowMapCenter, DepthSampler, OffsetX = -2.0, OffsetY =  1.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			tfetch2D vShadowMapVals.__x_, vShadowMapCenter, DepthSampler, OffsetX = -1.0, OffsetY =  1.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			tfetch2D vShadowMapVals.___x, vShadowMapCenter, DepthSampler, OffsetX = -2.0, OffsetY =  0.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			sgt vInLight, vDist4, vShadowMapVals
		};
		float4 vShadowMapWeights = float4( 1, vWeights.z * vWeights.y, vWeights.y, vWeights.z );
		flPercentInLight += dot( vInLight, vShadowMapWeights );
	}

	[isolate]
	{
		float4 vShadowMapVals, vInLight;
		asm 
		{
			tfetch2D vShadowMapVals.x___, vShadowMapCenter, DepthSampler, OffsetX =  1.0, OffsetY =  0.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			tfetch2D vShadowMapVals._x__, vShadowMapCenter, DepthSampler, OffsetX =  0.0, OffsetY =  1.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			tfetch2D vShadowMapVals.__x_, vShadowMapCenter, DepthSampler, OffsetX =  1.0, OffsetY =  1.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			tfetch2D vShadowMapVals.___x, vShadowMapCenter, DepthSampler, OffsetX =  0.0, OffsetY =  0.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			sgt vInLight, vDist4, vShadowMapVals
		};
		float4 vShadowMapWeights = float4( vWeights.x, vWeights.y, vWeights.x * vWeights.y, 1 );
		flPercentInLight += dot( vInLight, vShadowMapWeights );
	}

	//sum of weights is 9  since border taps are bilinearly filtered
	return ( 1.0f / 9.0f ) * flPercentInLight;
}

// Return value: x = dZ/dX, y=dZ/dY (dX/dY in normalized shadow texture space)
float2 ComputeReceiverPlaneDepthBiasGradients( float3 vProjCoords, float NdotL )
{
	// See http://developer.amd.com/media/gpu_assets/Isidoro-ShadowMapping.pdf	
	float3 vDUVDistDX = ddx( vProjCoords );
	float3 vDUVDistDY = ddy( vProjCoords );

	float flDet = ( ( vDUVDistDX.x * vDUVDistDY.y ) - ( vDUVDistDX.y * vDUVDistDY.x ) );

	float flInvDet = ( flDet != 0.0f ) ? ( 1.0f / flDet ) : 0.0f;
	vDUVDistDY *= flInvDet;

	float2 vDDistDUV;
	vDDistDUV.x = vDUVDistDY.y * vDUVDistDX.z - vDUVDistDX.y * vDUVDistDY.z;
	vDDistDUV.y = vDUVDistDX.x * vDUVDistDY.z - vDUVDistDY.x * vDUVDistDX.z;
			
	// Stable work around for when abs(flDet) gets extremely small - fade out receiver plane bias as NdotL approaches 0.
	NdotL = saturate( NdotL);
	vDDistDUV = lerp(float2(0.0f, 0.0f), vDDistDUV, NdotL * NdotL );
	return vDDistDUV;
}

float DoShadow360BilinearX( sampler DepthSampler, float3 vProjCoords, float2 vCenterOfs, float2 vDDistDuv, float4 vSampledDepths, float4 vWeights )
{
	vCenterOfs *= ( 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION );
	float flReceiverPlaneBias = vCenterOfs.x * vDDistDuv.x + vCenterOfs.y * vDDistDuv.y;
	float flDepthComparisonValue = vProjCoords.z + flReceiverPlaneBias;
    	    
	float4 vCompare = vSampledDepths < float4( flDepthComparisonValue, flDepthComparisonValue, flDepthComparisonValue, flDepthComparisonValue );
	return dot( vCompare, vWeights );
}

float DoShadowXbox4x4SamplesX( sampler DepthSampler, float3 vProjCoords, float NdotL )
{
	[branch]
	if ( NdotL <= 0.0f)
		return 0.0f;

	vProjCoords.z = 1.0f - min( vProjCoords.z, 0.99999f );		// Object depth in shadow space

	float2 vDDistDUV = ComputeReceiverPlaneDepthBiasGradients( vProjCoords, NdotL );
	
	float4 vWeights;
	asm
	{
		getWeights2D vWeights, vProjCoords.xy, DepthSampler, MagFilter=linear, MinFilter=linear, UseComputedLOD=false, UseRegisterLOD=false
	};	
	vWeights = float4( (1-vWeights.x)*(1-vWeights.y), vWeights.x*(1-vWeights.y), (1-vWeights.x)*vWeights.y, vWeights.x*vWeights.y );
	
	float4 vr00, vr10, vr01, vr11;
	asm 
	{
		// r00=(-1, -1)
		tfetch2D vr00.x___, vProjCoords, DepthSampler, OffsetX = -1.5, OffsetY = -1.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
		tfetch2D vr00._x__, vProjCoords, DepthSampler, OffsetX = -0.5, OffsetY = -1.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
		tfetch2D vr00.__x_, vProjCoords, DepthSampler, OffsetX = -1.5, OffsetY = -0.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
		tfetch2D vr00.___x, vProjCoords, DepthSampler, OffsetX = -0.5, OffsetY = -0.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
	};
	asm
	{		
		// r10=(+1, -1)
		tfetch2D vr10.x___, vProjCoords, DepthSampler, OffsetX =  0.5, OffsetY = -1.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
		tfetch2D vr10._x__, vProjCoords, DepthSampler, OffsetX =  1.5, OffsetY = -1.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
		tfetch2D vr10.__x_, vProjCoords, DepthSampler, OffsetX =  0.5, OffsetY = -0.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
		tfetch2D vr10.___x, vProjCoords, DepthSampler, OffsetX =  1.5, OffsetY = -0.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
   	};
    asm
	{
		// r01=(-1, +1)
		tfetch2D vr01.x___, vProjCoords, DepthSampler, OffsetX = -1.5, OffsetY =  0.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
		tfetch2D vr01._x__, vProjCoords, DepthSampler, OffsetX = -0.5, OffsetY =  0.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
		tfetch2D vr01.__x_, vProjCoords, DepthSampler, OffsetX = -1.5, OffsetY =  1.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
		tfetch2D vr01.___x, vProjCoords, DepthSampler, OffsetX = -0.5, OffsetY =  1.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
	};
	asm
	{
		// r11=(+1, +1)		
		tfetch2D vr11.x___, vProjCoords, DepthSampler, OffsetX =  0.5, OffsetY =  0.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
		tfetch2D vr11._x__, vProjCoords, DepthSampler, OffsetX =  1.5, OffsetY =  0.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
		tfetch2D vr11.__x_, vProjCoords, DepthSampler, OffsetX =  0.5, OffsetY =  1.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
		tfetch2D vr11.___x, vProjCoords, DepthSampler, OffsetX =  1.5, OffsetY =  1.5, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
	};
	
	float4 vOneTaps;
	vOneTaps.x = DoShadow360BilinearX( DepthSampler, vProjCoords, float2(  1.0f,  1.0f ), vDDistDUV, vr11, vWeights ); 
	vOneTaps.y = DoShadow360BilinearX( DepthSampler, vProjCoords, float2( -1.0f,  1.0f ), vDDistDUV, vr01, vWeights );
	vOneTaps.z = DoShadow360BilinearX( DepthSampler, vProjCoords, float2(  1.0f, -1.0f ), vDDistDUV, vr10, vWeights );
	vOneTaps.w = DoShadow360BilinearX( DepthSampler, vProjCoords, float2( -1.0f, -1.0f ), vDDistDUV, vr00, vWeights );
	float flOneTaps = dot( vOneTaps, float4(1.0f / 16.0f, 1.0f / 16.0f, 1.0f / 16.0f, 1.0f / 16.0f));

	float4 vTwoTaps;
	vTwoTaps.x = DoShadow360BilinearX( DepthSampler, vProjCoords, float2(  1.0f,  0 ), vDDistDUV, float4( vr10.z, vr10.w, vr11.x, vr11.y ), vWeights );
	vTwoTaps.y = DoShadow360BilinearX( DepthSampler, vProjCoords, float2( -1.0f,  0 ), vDDistDUV, float4( vr00.z, vr00.w, vr01.x, vr01.y ), vWeights );
	vTwoTaps.z = DoShadow360BilinearX( DepthSampler, vProjCoords, float2(  0, -1.0f ), vDDistDUV, float4( vr00.y, vr10.x, vr00.w, vr10.z ), vWeights );
	vTwoTaps.w = DoShadow360BilinearX( DepthSampler, vProjCoords, float2(  0,  1.0f ), vDDistDUV, float4( vr01.y, vr11.x, vr01.w, vr11.z), vWeights );
	float flTwoTaps = dot( vTwoTaps, float4(2.0f / 16.0f, 2.0f / 16.0f, 2.0f / 16.0f, 2.0f / 16.0f));

	float flCenterTap = DoShadow360BilinearX( DepthSampler, vProjCoords, float2( 0.0f, 0.0f ), vDDistDUV, float4( vr00.w, vr10.z, vr01.y, vr11.x ), vWeights ) * float(4.0f / 16.0f);
	
	// Sum all 9 Taps
	float flShadowFactor = saturate( flOneTaps + flTwoTaps + flCenterTap );
	
	// Complete hack here, but it looks good (falloff is more circular/less blocky).
	flShadowFactor = pow( flShadowFactor, 1.45f );
	return flShadowFactor;
}

float DoShadowXbox3x3Samples( sampler DepthSampler, const float3 vProjCoords )
{
	float2 vShadowMapCenter = vProjCoords.xy;			// Center of shadow filter
	// This shader assumes REVERSE_DEPTH_ON_X360 is always defined.
	float flObjDepth = 1.0f - min( vProjCoords.z, 0.99999f );		// Object depth in shadow space

	// projective distance from z plane in view coords
	float4 vDist4 = float4( flObjDepth, flObjDepth, flObjDepth, flObjDepth );

	//fraction component of projected coordinates; here FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION represents the shadowmap size
	float2 vTexRes = float2( FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION, FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION );
	float2 vFrac = frac( vShadowMapCenter * vTexRes );
	float4 vWeights = float4( vFrac.x, vFrac.y, 1.0f - vFrac.x, 1.0f - vFrac.y );
	
	float flPercentInLight = 1.0f;

	[isolate]
	{
		float4 vShadowMapVals, vInLight;
		asm 
		{
			tfetch2D vShadowMapVals.x___, vShadowMapCenter, DepthSampler, OffsetX = -1.0, OffsetY = -1.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			tfetch2D vShadowMapVals._x__, vShadowMapCenter, DepthSampler, OffsetX =  0.0, OffsetY = -1.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			tfetch2D vShadowMapVals.__x_, vShadowMapCenter, DepthSampler, OffsetX =  1.0, OffsetY = -1.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			sgt vInLight, vDist4, vShadowMapVals
		};
		float3 vShadowMapWeights = float3( vWeights.z * vWeights.w, vWeights.w, vWeights.x * vWeights.w );
		flPercentInLight = dot( vInLight, vShadowMapWeights );
	}

	[isolate]
	{
		float4 vShadowMapVals, vInLight;
		asm 
		{
			tfetch2D vShadowMapVals.x___, vShadowMapCenter, DepthSampler, OffsetX = -1.0, OffsetY =  0.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			tfetch2D vShadowMapVals._x__, vShadowMapCenter, DepthSampler, OffsetX =  0.0, OffsetY =  0.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			tfetch2D vShadowMapVals.__x_, vShadowMapCenter, DepthSampler, OffsetX =  1.0, OffsetY =  0.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			sgt vInLight, vDist4, vShadowMapVals
		};
		float3 vShadowMapWeights = float3( vWeights.z, 1.0f, vWeights.x );
		flPercentInLight += dot( vInLight, vShadowMapWeights );
	}

	[isolate]
	{
		float4 vShadowMapVals, vInLight;
		asm 
		{
			tfetch2D vShadowMapVals.x___, vShadowMapCenter, DepthSampler, OffsetX = -1.0, OffsetY =  1.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			tfetch2D vShadowMapVals._x__, vShadowMapCenter, DepthSampler, OffsetX =  0.0, OffsetY =  1.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			tfetch2D vShadowMapVals.__x_, vShadowMapCenter, DepthSampler, OffsetX =  1.0, OffsetY =  1.0, UseComputedLOD=false, UseRegisterLOD=false, MagFilter = point, MinFilter = point
			sgt vInLight, vDist4, vShadowMapVals
		};
		float3 vShadowMapWeights = float3( vWeights.z * vWeights.y, vWeights.y, vWeights.x * vWeights.y );
		flPercentInLight += dot( vInLight, vShadowMapWeights );
	}

	return ( 1.0f / 4.0f ) * flPercentInLight;
}

float Do360PCFFetch( sampler DepthSampler, float2 tc, float objDepth )
{
	float fLOD;
	float4 vSampledDepths, vWeights;

	asm {
			getCompTexLOD2D fLOD.x, tc.xy, DepthSampler, AnisoFilter=max16to1
			setTexLOD fLOD.x

			tfetch2D vSampledDepths.x___, tc, DepthSampler, OffsetX = -0.5, OffsetY = -0.5, UseComputedLOD=false, UseRegisterLOD=true, MagFilter = point, MinFilter = point
			tfetch2D vSampledDepths._x__, tc, DepthSampler, OffsetX =  0.5, OffsetY = -0.5, UseComputedLOD=false, UseRegisterLOD=true, MagFilter = point, MinFilter = point
			tfetch2D vSampledDepths.__x_, tc, DepthSampler, OffsetX = -0.5, OffsetY =  0.5, UseComputedLOD=false, UseRegisterLOD=true, MagFilter = point, MinFilter = point
			tfetch2D vSampledDepths.___x, tc, DepthSampler, OffsetX =  0.5, OffsetY =  0.5, UseComputedLOD=false, UseRegisterLOD=true, MagFilter = point, MinFilter = point

			getWeights2D vWeights, tc.xy, DepthSampler, MagFilter=linear, MinFilter=linear, UseComputedLOD=false, UseRegisterLOD=true
	};

	vWeights = float4( (1-vWeights.x)*(1-vWeights.y), vWeights.x*(1-vWeights.y), (1-vWeights.x)*vWeights.y, vWeights.x*vWeights.y );

#if defined( REVERSE_DEPTH_ON_X360 )
	float4 vCompare = (vSampledDepths < objDepth.xxxx);
#else
	float4 vCompare = (vSampledDepths > objDepth.xxxx);
#endif

	return dot( vCompare, vWeights );
}



float Do360NearestFetch( sampler DepthSampler, float2 tc, float objDepth )
{
	float fLOD;
	float4 vSampledDepth;

	asm {
		getCompTexLOD2D fLOD.x, tc.xy, DepthSampler, AnisoFilter=max16to1
		setTexLOD fLOD.x

		tfetch2D vSampledDepth.x___, tc, DepthSampler, UseComputedLOD=false, UseRegisterLOD=true, MagFilter = point, MinFilter = point
	};

#if defined( REVERSE_DEPTH_ON_X360 )
	return (vSampledDepth.x < objDepth.x);
#else
	return (vSampledDepth.x > objDepth.x);
#endif

}


float AmountShadowed_8Tap_360( sampler DepthSampler, float2 tc, float objDepth )
{
	float fLOD;
	float4 vSampledDepthsA, vSampledDepthsB;

	// Optimal 8 rooks pattern to get an idea about whether we're at a penumbra or not
	// From [Kallio07] "Scanline Edge-Flag Algorithm for Antialiasing" 
	//
	//        +---+---+---+---+---+---+---+---+
	//        |   |   |   |   |   | o |   |   |
	//        +---+---+---+---+---+---+---+---+
	//        | o |   |   |   |   |   |   |   |
	//        +---+---+---+---+---+---+---+---+
	//        |   |   |   | o |   |   |   |   |
	//        +---+---+---+---+---+---+---+---+
	//        |   |   |   |   |   |   | o |   |
	//        +---+---+---+---+---+---+---+---+
	//        |   | o |   |   |   |   |   |   |
	//        +---+---+---+---+---+---+---+---+
	//        |   |   |   |   | o |   |   |   |
	//        +---+---+---+---+---+---+---+---+
	//        |   |   |   |   |   |   |   | o |
	//        +---+---+---+---+---+---+---+---+
	//        |   |   | o |   |   |   |   |   |
	//        +---+---+---+---+---+---+---+---+
	//
	asm {
			getCompTexLOD2D fLOD.x, tc.xy, DepthSampler, AnisoFilter=max16to1
			setTexLOD fLOD.x

			tfetch2D vSampledDepthsA.x___, tc, DepthSampler, OffsetX = -2.0, OffsetY = -1.5, UseComputedLOD=false, UseRegisterLOD=true, MagFilter = point, MinFilter = point
			tfetch2D vSampledDepthsA._x__, tc, DepthSampler, OffsetX = -1.5, OffsetY =  0.5, UseComputedLOD=false, UseRegisterLOD=true, MagFilter = point, MinFilter = point
			tfetch2D vSampledDepthsA.__x_, tc, DepthSampler, OffsetX = -1.0, OffsetY =  2.0, UseComputedLOD=false, UseRegisterLOD=true, MagFilter = point, MinFilter = point
			tfetch2D vSampledDepthsA.___x, tc, DepthSampler, OffsetX = -0.5, OffsetY = -1.0, UseComputedLOD=false, UseRegisterLOD=true, MagFilter = point, MinFilter = point

			tfetch2D vSampledDepthsB.x___, tc, DepthSampler, OffsetX =  0.5, OffsetY =  1.0, UseComputedLOD=false, UseRegisterLOD=true, MagFilter = point, MinFilter = point
			tfetch2D vSampledDepthsB._x__, tc, DepthSampler, OffsetX =  1.0, OffsetY = -2.0, UseComputedLOD=false, UseRegisterLOD=true, MagFilter = point, MinFilter = point
			tfetch2D vSampledDepthsB.__x_, tc, DepthSampler, OffsetX =  1.5, OffsetY = -0.5, UseComputedLOD=false, UseRegisterLOD=true, MagFilter = point, MinFilter = point
			tfetch2D vSampledDepthsB.___x, tc, DepthSampler, OffsetX =  2.0, OffsetY =  1.5, UseComputedLOD=false, UseRegisterLOD=true, MagFilter = point, MinFilter = point
	};

#if defined( REVERSE_DEPTH_ON_X360 )
	float4 vCompareA = (vSampledDepthsA < objDepth.xxxx);
	float4 vCompareB = (vSampledDepthsB < objDepth.xxxx);
#else
	float4 vCompareA = (vSampledDepthsA > objDepth.xxxx);
	float4 vCompareB = (vSampledDepthsB > objDepth.xxxx);
#endif

	return dot( vCompareA, float4(0.125,0.125,0.125,0.125) ) + dot( vCompareB, float4(0.125,0.125,0.125,0.125) );
}


float AmountShadowed_4Tap_360( sampler DepthSampler, float2 tc, float objDepth )
{
	float fLOD;
	float4 vSampledDepths;

	// Rotated grid pattern to get an idea about whether we're at a penumbra or not
	asm {
		getCompTexLOD2D fLOD.x, tc.xy, DepthSampler, AnisoFilter=max16to1
			setTexLOD fLOD.x

			tfetch2D vSampledDepths.x___, tc, DepthSampler, OffsetX = -1.0, OffsetY =  0.5, UseComputedLOD=false, UseRegisterLOD=true, MagFilter = point, MinFilter = point
			tfetch2D vSampledDepths._x__, tc, DepthSampler, OffsetX = -0.5, OffsetY = -1.0, UseComputedLOD=false, UseRegisterLOD=true, MagFilter = point, MinFilter = point
			tfetch2D vSampledDepths.__x_, tc, DepthSampler, OffsetX =  0.5, OffsetY =  1.0, UseComputedLOD=false, UseRegisterLOD=true, MagFilter = point, MinFilter = point
			tfetch2D vSampledDepths.___x, tc, DepthSampler, OffsetX =  1.0, OffsetY = -0.5, UseComputedLOD=false, UseRegisterLOD=true, MagFilter = point, MinFilter = point
	};

#if defined( REVERSE_DEPTH_ON_X360 )
	float4 vCompare = (vSampledDepths < objDepth.xxxx);
#else
	float4 vCompare = (vSampledDepths > objDepth.xxxx);
#endif

	return dot( vCompare, float4(0.25,0.25,0.25,0.25) );
}

// Poisson disc, randomly rotated at different UVs
float DoShadowPoisson360( sampler DepthSampler, sampler RandomRotationSampler, const float3 vProjCoords, const float2 vScreenPos, const float4 vShadowTweaks )
{
	float2 vPoissonOffset[8] = { float2(  0.3475f,  0.0042f ), float2(  0.8806f,  0.3430f ),
								 float2( -0.0041f, -0.6197f ), float2(  0.0472f,  0.4964f ),
								 float2( -0.3730f,  0.0874f ), float2( -0.9217f, -0.3177f ),
								 float2( -0.6289f,  0.7388f ), float2(  0.5744f, -0.7741f ) };

	float2 shadowMapCenter = vProjCoords.xy;		// Center of shadow filter
	float objDepth = min( vProjCoords.z, 0.99999 );	// Object depth in shadow space

#if defined( REVERSE_DEPTH_ON_X360 )
	objDepth = 1.0f - objDepth;
#endif

	float fAmountShadowed = AmountShadowed_4Tap_360( DepthSampler, shadowMapCenter, objDepth );

	if ( fAmountShadowed >= 1.0f )			// Fully in light
	{
		return 1.0f;
	}
	else	// Do the expensive filtering since we're at least partially shadowed
	{
		float flScaleOverMapSize = 1.7f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;		// Tweak parameters to shader

		// 2D Rotation Matrix setup
		float3 RMatTop = 0, RMatBottom = 0;
#if defined(SHADER_MODEL_PS_2_0) || defined(SHADER_MODEL_PS_2_B) || defined(SHADER_MODEL_PS_3_0)
		RMatTop.xy = tex2D( RandomRotationSampler, cFlashlightScreenScale.xy * (vScreenPos * 0.5 + 0.5)) * 2.0 - 1.0;
		RMatBottom.xy = float2(-1.0, 1.0) * RMatTop.yx;	// 2x2 rotation matrix in 4-tuple
#endif

		RMatTop *= flScaleOverMapSize;					// Scale up kernel while accounting for texture resolution
		RMatBottom *= flScaleOverMapSize;
		RMatTop.z = shadowMapCenter.x;					// To be added in d2adds generated below
		RMatBottom.z = shadowMapCenter.y;
		float2 rotOffset = float2(0,0);
		float4 vAccum = 0;

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[0].xy) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[0].xy) + RMatBottom.z;
		vAccum.x  = Do360NearestFetch( DepthSampler, rotOffset, objDepth );

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[1].xy) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[1].xy) + RMatBottom.z;
		vAccum.y  = Do360NearestFetch( DepthSampler, rotOffset, objDepth );

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[2].xy) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[2].xy) + RMatBottom.z;
		vAccum.z  = Do360NearestFetch( DepthSampler, rotOffset, objDepth );

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[3].xy) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[3].xy) + RMatBottom.z;
		vAccum.w  = Do360NearestFetch( DepthSampler, rotOffset, objDepth );

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[4].xy) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[4].xy) + RMatBottom.z;
		vAccum.x += Do360NearestFetch( DepthSampler, rotOffset, objDepth );

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[5].xy) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[5].xy) + RMatBottom.z;
		vAccum.y += Do360NearestFetch( DepthSampler, rotOffset, objDepth );

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[6].xy) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[6].xy) + RMatBottom.z;
		vAccum.z += Do360NearestFetch( DepthSampler, rotOffset, objDepth );

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[7].xy) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[7].xy) + RMatBottom.z;
		vAccum.w += Do360NearestFetch( DepthSampler, rotOffset, objDepth );

		return dot( vAccum, float4( 0.25, 0.25, 0.25, 0.25) );
	}
}

#endif // _X360

float AmountShadowed_1Tap_NVidiaPCF( sampler DepthSampler, float3 vProjPos )
{
	float2 shadowMapCenter = vProjPos.xy;
	float objDepth = vProjPos.z;

	return tex2Dproj( DepthSampler, float4( shadowMapCenter, objDepth, 1 ) ).x;
}

float AmountShadowed_4Tap_NVidiaPCF( sampler DepthSampler, float3 vProjPos )
{
	float fTexelEpsilon = 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;

	float2 shadowMapCenter = vProjPos.xy;
	float objDepth = vProjPos.z;

	float4 s;
	s.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2( -1.0f,   .5f ) * fTexelEpsilon, objDepth, 1 ) ).x;
	s.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  -.5f, -1.0f ) * fTexelEpsilon, objDepth, 1 ) ).x;
	s.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(   .5f, -1.0f ) * fTexelEpsilon, objDepth, 1 ) ).x;
	s.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  1.0f,  -.5f ) * fTexelEpsilon, objDepth, 1 ) ).x;
		
	return dot( s, float4( .25f, .25f, .25f, .25f ) );
}

float AmountShadowed_5Tap_NVidiaPCF( sampler DepthSampler, float3 vProjPos )
{
	float2 vShadowMapCenter = vProjPos.xy;
	float4 vTexelEpsilon = float4( -1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION, 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION, 0.0f, 1.0f );
		
	HALF4 r;
	r.x = tex2Dproj( DepthSampler, float4( vShadowMapCenter + vTexelEpsilon.xz, vProjPos.z, 1.0f ) ).x;
	r.y = tex2Dproj( DepthSampler, float4( vShadowMapCenter + vTexelEpsilon.yz, vProjPos.z, 1.0f ) ).y;
	r.z = tex2Dproj( DepthSampler, float4( vShadowMapCenter + vTexelEpsilon.zx, vProjPos.z, 1.0f ) ).z;
	r.w = tex2Dproj( DepthSampler, float4( vShadowMapCenter + vTexelEpsilon.zy, vProjPos.z, 1.0f ) ).w;
	HALF flSum = dot( r, HALF4( .175f, .175f, .175f, .175f ) );
	
	flSum += (HALF)( tex2Dproj( DepthSampler, float4( vShadowMapCenter, vProjPos.z, 1.0f ) ).x * (HALF)( .3f ) );

	return flSum;
}



HALF DoFlashlightShadow( sampler DepthSampler, sampler RandomRotationSampler, float3 vProjCoords, float2 vScreenPos, int nShadowLevel, float4 vShadowTweaks, float nDotL = 1.0f )
{
	HALF flShadow = 1.0h;

#if defined( _PS3 )

#if 0
	// Original code, which samples an A8R8G8B8 texture and recovers depth.
	float4 flUnpackedDepth = tex2Dproj( DepthSampler, float4( vProjCoords, 1 ) );
	float flDepth = ( flUnpackedDepth.r * 256.0f * 255.0f + flUnpackedDepth.g * 255.0f + flUnpackedDepth.b ) / ( 255.0f * 256.0f );
	flShadow = ( vProjCoords.z < flDepth ) ? 1.0f : 0.0f;

	return flShadow;
#else
	if ( nShadowLevel == GAMECONSOLE_SINGLE_TAP_PCF )
	{
		return h4tex2D( DepthSampler, float3( vProjCoords.x, vProjCoords.y, vProjCoords.z ) ).x;
	}
	else
	{
		//return AmountShadowed_4Tap_NVidiaPCF( DepthSampler, vProjCoords );
		//return AmountShadowed_5Tap_NVidiaPCF( DepthSampler, vProjCoords );
		//return DoShadowNvidiaPCF5x5Gaussian( DepthSampler, vProjCoords );
		return DoShadowNvidiaPCF3x3Gaussian( DepthSampler, vProjCoords );
	}
#endif

#elif !defined( _X360 ) // PC
	if ( nShadowLevel == NVIDIA_PCF )
	{
#if defined( SHADER_MODEL_PS_2_0 ) || defined( SHADER_MODEL_PS_2_B )
		flShadow = AmountShadowed_4Tap_NVidiaPCF( DepthSampler, vProjCoords );	// This is pretty much just high-end Macs
#else
		flShadow = DoShadowNvidiaPCF5x5GaussianPC( DepthSampler, vProjCoords ); // NVIDIA ps_3 parts and ATI DX10 parts
#endif
	}
	else if( nShadowLevel == ATI_NO_PCF_FETCH4 )
	{
		flShadow = DoShadowATIFetch4( DepthSampler, vProjCoords );				// ATI DX9 ps_3_0 parts 
	}
	else if ( nShadowLevel == NVIDIA_PCF_CHEAP )
	{
		flShadow = AmountShadowed_1Tap_NVidiaPCF( DepthSampler, vProjCoords );	// Low-end NVIDIA parts and low-end Macs
	}
	else if( nShadowLevel == ATI_NOPCF )
	{
		flShadow = DoShadowATIBilinear( DepthSampler, vProjCoords );			// ATI ps_2_b parts
	}

	return flShadow;
#else // 360
	if ( nShadowLevel == GAMECONSOLE_SINGLE_TAP_PCF )
	{
		flShadow = DoShadow360Simple( DepthSampler, vProjCoords );
	}
	else
	{
		flShadow = DoShadowXbox4x4SamplesX( DepthSampler, vProjCoords, nDotL );
	}
	return flShadow;
#endif // PS3 / PC / 360
}

float3 SpecularLight( const float3 vWorldNormal, const float3 vLightDir, const float fSpecularExponent,
					  const float3 vEyeDir, const bool bDoSpecularWarp, in sampler specularWarpSampler, float fFresnel )
{
	float3 result = float3(0.0f, 0.0f, 0.0f);

	float3 vReflect = reflect( -vEyeDir, vWorldNormal );			// Reflect view through normal
	float3 vSpecular = saturate(dot( vReflect, vLightDir ));		// L.R	(use half-angle instead?)
	vSpecular = pow( vSpecular.x, fSpecularExponent );				// Raise to specular power

	// Optionally warp as function of scalar specular and fresnel
	if ( bDoSpecularWarp )
		vSpecular *= tex2D( specularWarpSampler, float2(vSpecular.x, fFresnel) ).rgb; // Sample at { (L.R)^k, fresnel }

	return vSpecular;
}

float RemapNormalizedValClamped( float val, float A, float B)
{
	return saturate( (val - A) / (B - A) );
}

void DoSpecularFlashlight( float3 flashlightPos, float3 worldPos, float4 flashlightSpacePosition, float3 worldNormal,  
					float3 attenuationFactors, float farZ, sampler FlashlightSampler, sampler FlashlightDepthSampler, sampler RandomRotationSampler,
					int nShadowLevel, bool bDoShadows, const float2 vScreenPos, const float fSpecularExponent, const float3 vEyeDir,
					const bool bDoDiffuseWarp, sampler DiffuseWarpSampler, const bool bDoSpecularWarp, sampler specularWarpSampler, float fFresnel, float4 vShadowTweaks,

					// Outputs of this shader...separate shadowed diffuse and specular from the flashlight
					out float3 diffuseLighting, out float3 specularLighting )
{
	float3 vProjCoords = flashlightSpacePosition.xyz / flashlightSpacePosition.w;
	float3 flashlightColor = float3(1,1,1);

	flashlightColor = tex2D( FlashlightSampler, vProjCoords.xy ).rgb;

#if defined(SHADER_MODEL_PS_2_B) || defined(SHADER_MODEL_PS_3_0)
	flashlightColor *= flashlightSpacePosition.www > float3(0,0,0);	// Catch back projection (ps2b and up)
#endif

#if defined(SHADER_MODEL_PS_2_0) || defined(SHADER_MODEL_PS_2_B) || defined(SHADER_MODEL_PS_3_0)
	flashlightColor *= cFlashlightColor.xyz;						// Flashlight color
#endif

	float3 delta = flashlightPos - worldPos;
	float3 L = normalize( delta );
	float distSquared = dot( delta, delta );
	float dist = sqrt( distSquared );

	float endFalloffFactor = RemapNormalizedValClamped( dist, farZ, 0.6f * farZ );

	// Attenuation for light and to fade out shadow over distance
	float fAtten = saturate( dot( attenuationFactors, float3( 1.0f, 1.0f/dist, 1.0f/distSquared ) ) );
	
	float NdotL = dot( L.xyz, worldNormal.xyz );

	// Shadowing and coloring terms
#if (defined(SHADER_MODEL_PS_2_B) || defined(SHADER_MODEL_PS_3_0))
	if ( bDoShadows )
	{
		float flShadow = DoFlashlightShadow( FlashlightDepthSampler, RandomRotationSampler, vProjCoords, vScreenPos, nShadowLevel, vShadowTweaks, NdotL );
		float flAttenuated = lerp( flShadow, 1.0f, vShadowTweaks.y );	// Blend between fully attenuated and not attenuated
		flShadow = saturate( lerp( flAttenuated, flShadow, fAtten ) );	// Blend between shadow and above, according to light attenuation
		flashlightColor *= flShadow;									// Shadow term
	}
#endif

	diffuseLighting = fAtten;
	
	// JasonM - experimenting with light-warping the flashlight
	if ( false )//bDoDiffuseWarp )
	{
		float warpCoord = saturate(NdotL * 0.5f + 0.5f);								// 0..1
		diffuseLighting *= tex2D( DiffuseWarpSampler, float2( warpCoord, 0.0f) ).rgb;	// Look up warped light
	}
	else // common path
	{
#if defined(SHADER_MODEL_PS_2_0) || defined(SHADER_MODEL_PS_2_B) || defined(SHADER_MODEL_PS_3_0)
		NdotL += flFlashlightNoLambertValue;
#endif
		diffuseLighting *= saturate( NdotL ); // Lambertian term
	}

	diffuseLighting *= flashlightColor;
	diffuseLighting *= endFalloffFactor;

	// Specular term (masked by diffuse)
	specularLighting = diffuseLighting * SpecularLight ( worldNormal, L, fSpecularExponent, vEyeDir, bDoSpecularWarp, specularWarpSampler, fFresnel );
}

// Diffuse only version
HALF3 DoFlashlight( float3 flashlightPos, float3 worldPos, float4 flashlightSpacePosition, float3 worldNormal, 
					float3 attenuationFactors, float farZ, sampler FlashlightSampler, sampler FlashlightDepthSampler,
					sampler RandomRotationSampler, int nShadowLevel, bool bDoShadows,
					const float2 vScreenPos, bool bClip, float4 vShadowTweaks = float4( 3.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION, 0.0005f, 0.0f, 0.0f), bool bHasNormal = true )
{
	float3 vProjCoords = flashlightSpacePosition.xyz / flashlightSpacePosition.w;
	// rg - Always fetching the flashlight texture on X360 so the GPU computes the LOD factors used to select the mipmap level properly. 
	// Otherwise, if we fetch after the [branch] we'll sometimes get edge artifacts because the GPU fetches from a lower mipmap level when it shouldn't.
	HALF3 flashlightColor = h3tex2D( FlashlightSampler, vProjCoords.xy ).rgb;
    	
	#if ( defined( _X360 ) )
	{
		float3 ltz = vProjCoords.xyz < float3( 0.0f, 0.0f, 0.0f );
		ltz.z = 0.0f; // don't clip the near plane per pixel since we don't do that on the PC.
		float3 gto = vProjCoords.xyz > float3( 1.0f, 1.0f, 1.0f );
		
		[branch]
		if ( dot(ltz + gto, float3(1,1,1)) > 0 )
		{
			if ( bClip )
			{
				clip(-1);
			}
			return float3(0,0,0);
		}
	}
	#endif
			
	#if	( defined(SHADER_MODEL_PS_2_B) || defined(SHADER_MODEL_PS_3_0) )
		flashlightColor *= (HALF)(flashlightSpacePosition.www > float3(0,0,0));	// Catch back projection (ps2b and up)
	#endif

	#if defined(SHADER_MODEL_PS_2_0) || defined(SHADER_MODEL_PS_2_B) || defined(SHADER_MODEL_PS_3_0)
	{
		flashlightColor *= (HALF3)cFlashlightColor.xyz;						// Flashlight color
	}
	#endif

	float3 delta = flashlightPos - worldPos;
	HALF3 L = normalize( delta );
	float distSquared = dot( delta, delta );
	float dist = sqrt( distSquared );

	HALF endFalloffFactor = RemapNormalizedValClamped( dist, farZ, 0.6f * farZ );

	// Attenuation for light and to fade out shadow over distance
	HALF fAtten = saturate( dot( attenuationFactors, float3( 1.0f, 1.0f/dist, 1.0f/distSquared ) ) );
	
	HALF flLDotWorldNormal;
	if ( bHasNormal )
	{
		flLDotWorldNormal = dot( L.xyz, (HALF3)worldNormal.xyz );
	}
	else
	{
		flLDotWorldNormal = 1.0h;
	}

	// Shadowing and coloring terms
#if (defined(SHADER_MODEL_PS_2_B) || defined(SHADER_MODEL_PS_3_0))
	if ( bDoShadows )
	{
		HALF flShadow = DoFlashlightShadow( FlashlightDepthSampler, RandomRotationSampler, vProjCoords, vScreenPos, nShadowLevel, vShadowTweaks, flLDotWorldNormal );
		HALF flAttenuated = lerp( saturate( flShadow ), 1.0h, (HALF)vShadowTweaks.y );	// Blend between fully attenuated and not attenuated
		flShadow = saturate( lerp( flAttenuated, flShadow, (HALF)fAtten ) );	// Blend between shadow and above, according to light attenuation
		flashlightColor *= flShadow;									// Shadow term
	}
#endif

	HALF3 diffuseLighting = fAtten;
    	
#if defined(SHADER_MODEL_PS_2_0) || defined(SHADER_MODEL_PS_2_B) || defined(SHADER_MODEL_PS_3_0)
	diffuseLighting *= saturate( flLDotWorldNormal + (HALF)flFlashlightNoLambertValue ); // Lambertian term
#else
	diffuseLighting *= saturate( flLDotWorldNormal ); // Lambertian (not Half-Lambert) term
#endif

	diffuseLighting *= flashlightColor;
	diffuseLighting *= endFalloffFactor;

	return diffuseLighting;
}

#endif //#ifndef COMMON_FLASHLIGHT_FXC_H_
