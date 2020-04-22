//========== Copyright (c) Valve Corporation, All rights reserved. ==========//

// PC cascaded shadow mapping

// This defines must be kept in sync with the CSM_DEFAULT_DEPTH_TEXTURE_RESOLUTION, etc. macros in c_env_cascade_light.cpp - otherwise you'll get subtle filtering artifacts.
#define CSM_DEPTH_TEXTURE_RESOLUTION_VERY_LOW 		( 640*2 )
#define CSM_DEPTH_TEXTURE_RESOLUTION_LOW 			( 768*2 )
#define CSM_DEPTH_TEXTURE_RESOLUTION_MEDIUM_OR_HIGH ( 1024*2 )

// Bilinear Percentage Closer Filtering with ATI Fetch4
#if 1
// This works on real ATI X1000-series hardware that uses a DX9-style FETCH4 swizzle.
float CSMSampleShadowBuffer1TapATIBilinear( float2 vPositionLs, float flComparisonDepth )
{
	float flSunShadowingShadowTextureWidth = CSM_DEPTH_TEXTURE_RESOLUTION_VERY_LOW;
	float flSunShadowingShadowTextureHeight = CSM_DEPTH_TEXTURE_RESOLUTION_VERY_LOW;
	float flSunShadowingInvShadowTextureWidth = 1.0f / CSM_DEPTH_TEXTURE_RESOLUTION_VERY_LOW;
	float flSunShadowingInvShadowTextureHeight = 1.0f / CSM_DEPTH_TEXTURE_RESOLUTION_VERY_LOW;
	
	float2 vFracPositionLs = frac( vPositionLs * float2( flSunShadowingShadowTextureWidth, flSunShadowingShadowTextureHeight ) );
	//float2 vSamplePositionLs = vPositionLs - vFracPositionLs * float2( flSunShadowingInvShadowTextureWidth, flSunShadowingInvShadowTextureHeight );
	//vSamplePositionLs += .00125f/CSM_DEPTH_TEXTURE_RESOLUTION_VERY_LOW;
	float2 vSamplePositionLs = vPositionLs;
	
	float4 vCmpSamples = tex2D( CSMDepthAtlasSampler, vSamplePositionLs.xy ).argb;
		
	vCmpSamples = vCmpSamples > flComparisonDepth;
	
	float4 vFactors = float4( ( 1.0f - vFracPositionLs.x ) * ( 1.0f - vFracPositionLs.y ), vFracPositionLs.x   * ( 1.0f - vFracPositionLs.y ),
							  ( 1.0f - vFracPositionLs.x ) *          vFracPositionLs.y,   vFracPositionLs.x   *          vFracPositionLs.y );
		 		
	return dot( vCmpSamples, vFactors );
}
#else
// This works properly on recent ATI hardware that uses DX 10.1+ style GATHER4 swizzles. Argh.
float CSMSampleShadowBuffer1TapATIBilinear( float2 vPositionLs, float flComparisonDepth )
{
	float flSunShadowingShadowTextureWidth = CSM_DEPTH_TEXTURE_RESOLUTION_VERY_LOW;
	float flSunShadowingShadowTextureHeight = CSM_DEPTH_TEXTURE_RESOLUTION_VERY_LOW;
	float flSunShadowingInvShadowTextureWidth = 1.0f / CSM_DEPTH_TEXTURE_RESOLUTION_VERY_LOW;
	float flSunShadowingInvShadowTextureHeight = 1.0f / CSM_DEPTH_TEXTURE_RESOLUTION_VERY_LOW;
	
	float2 vFracPositionLs = frac( vPositionLs * float2( flSunShadowingShadowTextureWidth, flSunShadowingShadowTextureHeight ) );
	float2 vSamplePositionLs = vPositionLs - vFracPositionLs * float2( flSunShadowingInvShadowTextureWidth, flSunShadowingInvShadowTextureHeight );
	vSamplePositionLs += .00125f/CSM_DEPTH_TEXTURE_RESOLUTION_VERY_LOW;
		
	float4 vCmpSamples = tex2D( CSMDepthAtlasSampler, vSamplePositionLs.xy ).abrg;
		
	vCmpSamples = vCmpSamples > flComparisonDepth;
	
	float4 vFactors = float4( ( 1.0f - vFracPositionLs.x ) * ( 1.0f - vFracPositionLs.y ), vFracPositionLs.x   * ( 1.0f - vFracPositionLs.y ),
							  ( 1.0f - vFracPositionLs.x ) *          vFracPositionLs.y,   vFracPositionLs.x   *          vFracPositionLs.y );
		 		
	return dot( vCmpSamples, vFactors );
}
#endif

float CSMSampleShadowBuffer1Tap( float2 vPositionLs, float flComparisonDepth )
{
	// Non-gameconsole
	return tex2Dlod( CSMDepthAtlasSampler, float4( vPositionLs.x, vPositionLs.y, flComparisonDepth, 0.0f ) ).x;
}

float CSMSampleShadowBuffer9Taps( float2 shadowMapCenter, float objDepth )
{
	float fTexelEpsilon = 1.0f / CSM_DEPTH_TEXTURE_RESOLUTION_MEDIUM_OR_HIGH;
	
	float4 vSampleBase = float4( shadowMapCenter, objDepth, 0.0f );

	float4 vOneTaps;
	vOneTaps.x = tex2Dlod( CSMDepthAtlasSampler, vSampleBase + float4(  fTexelEpsilon,  fTexelEpsilon, 0, 0 ) ).x;
	vOneTaps.y = tex2Dlod( CSMDepthAtlasSampler, vSampleBase + float4( -fTexelEpsilon,  fTexelEpsilon, 0, 0 ) ).x;
	vOneTaps.z = tex2Dlod( CSMDepthAtlasSampler, vSampleBase + float4(  fTexelEpsilon, -fTexelEpsilon, 0, 0 ) ).x;
	vOneTaps.w = tex2Dlod( CSMDepthAtlasSampler, vSampleBase + float4( -fTexelEpsilon, -fTexelEpsilon, 0, 0 ) ).x;
	float flOneTaps = dot( vOneTaps, float4(1.0f / 16.0f, 1.0f / 16.0f, 1.0f / 16.0f, 1.0f / 16.0f));

	float4 vTwoTaps;
	vTwoTaps.x = tex2Dlod( CSMDepthAtlasSampler, vSampleBase + float4(  fTexelEpsilon,  0, 0, 0 ) ).x;
	vTwoTaps.y = tex2Dlod( CSMDepthAtlasSampler, vSampleBase + float4( -fTexelEpsilon,  0, 0, 0 ) ).x;
	vTwoTaps.z = tex2Dlod( CSMDepthAtlasSampler, vSampleBase + float4(  0, -fTexelEpsilon, 0, 0 ) ).x;
	vTwoTaps.w = tex2Dlod( CSMDepthAtlasSampler, vSampleBase + float4(  0,  fTexelEpsilon, 0, 0 ) ).x;
	float flTwoTaps = dot( vTwoTaps, float4(2.0f / 16.0f, 2.0f / 16.0f, 2.0f / 16.0f, 2.0f / 16.0f));

	float flCenterTap = tex2Dlod( CSMDepthAtlasSampler, vSampleBase ).x * float(4.0f / 16.0f);

	// Sum all 9 Taps
	return flOneTaps + flTwoTaps + flCenterTap;
}

// 25 taps is crazy expensive, just here for comparison purposes.
float CSMSampleShadowBuffer25Taps( float2 shadowMapCenter, float objDepth )
{
	float flTexelEpsilon    = 1.0f / CSM_DEPTH_TEXTURE_RESOLUTION_MEDIUM_OR_HIGH;
	float flTwoTexelEpsilon = 2.0f * flTexelEpsilon;

	float4 c0 = float4( 1.0f / 331.0f, 7.0f / 331.0f, 4.0f / 331.0f, 20.0f / 331.0f );
	float4 c1 = float4( 33.0f / 331.0f, 55.0f / 331.0f, -flTexelEpsilon, 0.0f );
	float4 c2 = float4( flTwoTexelEpsilon, -flTwoTexelEpsilon, 0.0f, flTexelEpsilon );
	float4 c3 = float4( flTexelEpsilon, -flTexelEpsilon, flTwoTexelEpsilon, -flTwoTexelEpsilon );

	float4 vOneTaps;
	vOneTaps.x = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c2.xx, objDepth, 0 ) ).x;	//  2  2
	vOneTaps.y = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c2.yx, objDepth, 0 ) ).x;	// -2  2
	vOneTaps.z = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c2.xy, objDepth, 0 ) ).x;	//  2 -2
	vOneTaps.w = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c2.yy, objDepth, 0 ) ).x;	// -2 -2
	float flSum = dot( vOneTaps, c0.xxxx );

	float4 vSevenTaps;
	vSevenTaps.x = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c2.xz, objDepth, 0 ) ).x;	//  2 0
	vSevenTaps.y = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c2.yz, objDepth, 0 ) ).x;	// -2 0
	vSevenTaps.z = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c2.zx, objDepth, 0 ) ).x;	// 0 2
	vSevenTaps.w = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c2.zy, objDepth, 0 ) ).x;	// 0 -2
	flSum += dot( vSevenTaps, c0.yyyy );

	float4 vFourTapsA, vFourTapsB;
	vFourTapsA.x = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c2.xw, objDepth, 0 ) ).x;	// 2 1
	vFourTapsA.y = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c2.wx, objDepth, 0 ) ).x;	// 1 2
	vFourTapsA.z = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c3.yz, objDepth, 0 ) ).x;	// -1 2
	vFourTapsA.w = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c3.wx, objDepth, 0 ) ).x;	// -2 1
	vFourTapsB.x = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c3.wy, objDepth, 0 ) ).x;	// -2 -1
	vFourTapsB.y = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c3.yw, objDepth, 0 ) ).x;	// -1 -2
	vFourTapsB.z = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c3.xw, objDepth, 0 ) ).x;	// 1 -2
	vFourTapsB.w = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c3.zy, objDepth, 0 ) ).x;	// 2 -1
	flSum += dot( vFourTapsA, c0.zzzz );
	flSum += dot( vFourTapsB, c0.zzzz );

	float4 v20Taps;
	v20Taps.x = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c3.xx, objDepth, 0 ) ).x;	// 1 1
	v20Taps.y = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c3.yx, objDepth, 0 ) ).x;	// -1 1
	v20Taps.z = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c3.xy, objDepth, 0 ) ).x;	// 1 -1
	v20Taps.w = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c3.yy, objDepth, 0 ) ).x;	// -1 -1
	flSum += dot( v20Taps, c0.wwww );

	float4 v33Taps;
	v33Taps.x = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c2.wz, objDepth, 0 ) ).x;	// 1 0
	v33Taps.y = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c1.zw, objDepth, 0 ) ).x;	// -1 0
	v33Taps.z = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c1.wz, objDepth, 0 ) ).x;	// 0 -1
	v33Taps.w = tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter + c2.zw, objDepth, 0 ) ).x;	// 0 1
	flSum += dot( v33Taps, c1.xxxx );

	flSum += tex2Dlod( CSMDepthAtlasSampler, float4( shadowMapCenter, objDepth, 0 ) ).x * c1.y;
	
	return flSum;
}

float CSMSampleShadowBuffer( float2 vPositionLs, float flComparisonDepth )
{
#if (CSM_MODE == CSM_MODE_ATI_FETCH4 )
	return CSMSampleShadowBuffer1TapATIBilinear( vPositionLs, flComparisonDepth );
#elif ( CSM_MODE == CSM_MODE_VERY_LOW_OR_LOW )
	return CSMSampleShadowBuffer1Tap( vPositionLs, flComparisonDepth );
#else	
	return CSMSampleShadowBuffer9Taps( vPositionLs, flComparisonDepth );
#endif	
}

int CSMRangeTestExpanded( float2 vCoords )
{
	// Returns true if the coordinates are within [.02,.98] - purposely a little sloppy to prevent the shadow filter kernel from leaking outside the cascade's portion of the atlas.
	vCoords = vCoords * ( 1.0f / .96f ) - float2( .02f / .96f, .02f / .96f );
	return ( dot( saturate( vCoords.xy ) - vCoords.xy, float2( 1, 1 ) ) == 0.0f );
}

int CSMRangeTestNonExpanded( float2 vCoords )
{
	return ( dot( saturate( vCoords.xy ) - vCoords.xy, float2( 1, 1 ) ) == 0.0f );
}

float CSMComputeSplitLerpFactor( float2 vPositionToSampleLs )
{
	float2 vSplitLerpFactorTemp = float2( 1.0f, 1.0f ) - saturate( ( abs( vPositionToSampleLs.xy - float2( .5f, .5f ) ) - float2( g_flSunShadowingSplitLerpFactorBase, g_flSunShadowingSplitLerpFactorBase ) ) * float2( g_flSunShadowingSplitLerpFactorInvRange, g_flSunShadowingSplitLerpFactorInvRange ) );
	return vSplitLerpFactorTemp.x * vSplitLerpFactorTemp.y;
}

float4 CSMTransformLightToTexture( float4 pos, float4x4 mat )
{
	return mul( pos, mat );
}

#if ( CASCADE_SIZE == 0 )
	float CSMComputeShadowing( float3 vPositionWs ) 
	{
		return 1.0f;
	}
#elif ( CSM_MODE == CSM_MODE_HIGH )
	// Each cascade is 1024x1024, sample from up to 2 cascades, 9 tap filtering for each sample, smoothly lerp between each, 3 total cascades
	float CSMComputeShadowing( float3 vPositionWs ) 
	{
		float flShadowScalar = 1.0f;
			
		float4 vPosition4Ws = float4( vPositionWs.xyz, 1.0f );

		float3 vPositionToSampleLs = float3( 0.0f, 0.0f, 0.0f );
       	int nCascadeIndex = 0;
		
		vPositionToSampleLs.xy = mul( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[0] ).xy;

		// Non-expanded texcoord range tests because the 2D lerp will haved faded to the next cascade long before the filter kernels leaks outside the cascade's atlas region
		[flatten]
		if ( !CSMRangeTestNonExpanded( vPositionToSampleLs.xy ) )
		{
			nCascadeIndex = 1;
			vPositionToSampleLs.xy = mul( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[1] ).xy;

			[flatten]			
			if ( !CSMRangeTestNonExpanded( vPositionToSampleLs.xy ) )
			{
				nCascadeIndex = 2;
				vPositionToSampleLs.xy = mul( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[2] ).xy;
			}
		}
		
		vPositionToSampleLs.z = mul( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[0] ).z;

		float flSplitLerpFactor = CSMComputeSplitLerpFactor( vPositionToSampleLs.xy );

		vPositionToSampleLs.xy = saturate( vPositionToSampleLs.xy ) * g_vCascadeAtlasUVOffsets[nCascadeIndex].zw + g_vCascadeAtlasUVOffsets[nCascadeIndex].xy;
		flShadowScalar = CSMSampleShadowBuffer( vPositionToSampleLs.xy, vPositionToSampleLs.z );

		[branch]
		if ( flSplitLerpFactor < 1.0f )
		{
			float flShadowScalar1 = 1.0f;

			[flatten]
			if ( nCascadeIndex < 2 )
			{
				float2 vPosition1Ls = mul( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[nCascadeIndex + 1] ).xy;

				vPosition1Ls.xy = saturate( vPosition1Ls.xy ) * g_vCascadeAtlasUVOffsets[nCascadeIndex + 1].zw + g_vCascadeAtlasUVOffsets[nCascadeIndex + 1].xy;
				flShadowScalar1 = CSMSampleShadowBuffer( vPosition1Ls.xy, vPositionToSampleLs.z );				
			}
					
			flShadowScalar =  lerp( flShadowScalar1, flShadowScalar, saturate( flSplitLerpFactor ) );
		}
				
		float3 vCamDelta = vPositionWs - g_vCamPosition.xyz;
		float flZLerpFactor = saturate( dot( vCamDelta, vCamDelta ) * g_flSunShadowingZLerpFactorRange + g_flSunShadowingZLerpFactorBase );
		flShadowScalar = lerp( flShadowScalar, 1.0f, flZLerpFactor );

		return flShadowScalar;
	}
#elif ( ( CSM_MODE == CSM_MODE_VERY_LOW_OR_LOW ) || ( CSM_MODE == CSM_MODE_ATI_FETCH4 ) )
	// VERY_LOW = Each cascade is 640x640, sample from 1 cascade only, 2 total cascades
	// LOW = Each cascade is 768x768, sample from 1 cascade only, 2 total cascades
	float CSMComputeShadowing( float3 vPositionWs ) 
	{
		float4 vPosition4Ws = float4( vPositionWs.xyz, 1.0f );
	
		float3 vPositionToSampleLs = float3( 0.0f, 0.0f, CSMTransformLightToTexture( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[0] ).z );
		
		float2 vCascadeUVOffset = g_vCascadeAtlasUVOffsets[1].xy;//float2( .5f, 0.0f );
		vPositionToSampleLs.xy = CSMTransformLightToTexture( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[1] ).xy;

		[flatten]
		if ( !CSMRangeTestExpanded( vPositionToSampleLs.xy ) )
		{
			vCascadeUVOffset = g_vCascadeAtlasUVOffsets[2].xy;
			vPositionToSampleLs.xy = CSMTransformLightToTexture( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[2] ).xy;
		}
		
		float flShadowScalar = CSMSampleShadowBuffer( saturate( vPositionToSampleLs.xy ) * .5f + vCascadeUVOffset, vPositionToSampleLs.z );

		float3 vCamDelta = vPositionWs - g_vCamPosition.xyz;
		float flZLerpFactor = saturate( dot( vCamDelta, vCamDelta ) * g_flSunShadowingZLerpFactorRange + g_flSunShadowingZLerpFactorBase );
		flShadowScalar = lerp( flShadowScalar, 1.0f, flZLerpFactor );

		return flShadowScalar;
	}
#elif ( CSM_MODE == CSM_MODE_MEDIUM )
	// MEDIUM = Each cascade is 1024x1024, sample from 1 cascade only, 9 tap filtering, 3 cascades on vertexlit/phong, 2 cascades on lightmappedgeneric, 3 total cascades
	float CSMComputeShadowing( float3 vPositionWs ) 
	{
		float flShadowScalar = 1.0f;

		float4 vPosition4Ws = float4( vPositionWs.xyz, 1.0f );
	
		float3 vPositionToSampleLs = float3( 0.0f, 0.0f, CSMTransformLightToTexture( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[0] ).z );
		float2 vCascadeAtlasUVOffset = g_vCascadeAtlasUVOffsets[0].xy;
		float flLerpFactorDisable = 1.0f;

#if !defined( CSM_LIGHTMAPPEDGENERIC )
		vPositionToSampleLs.xy = CSMTransformLightToTexture( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[0] ).xy;

		[flatten]
		if ( !CSMRangeTestExpanded( vPositionToSampleLs.xy ) )
#endif
		{
			vCascadeAtlasUVOffset = g_vCascadeAtlasUVOffsets[1].xy;
			vPositionToSampleLs.xy = CSMTransformLightToTexture( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[1] ).xy;

			[flatten]
			if ( !CSMRangeTestExpanded( vPositionToSampleLs.xy ) )
			{
				flLerpFactorDisable = 0.0f;
				vCascadeAtlasUVOffset = g_vCascadeAtlasUVOffsets[2].xy;
				vPositionToSampleLs.xy = CSMTransformLightToTexture( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[2] ).xy;
			}
		}
		
		flShadowScalar = CSMSampleShadowBuffer( saturate( vPositionToSampleLs.xy ) * .5f + vCascadeAtlasUVOffset, vPositionToSampleLs.z );

		float2 vSplitLerpFactorTemp = float2( 1.0f, 1.0f ) - saturate( ( abs( vPositionToSampleLs.xy - float2( .5f, .5f ) ) - float2( g_flSunShadowingSplitLerpFactorBase, g_flSunShadowingSplitLerpFactorBase ) ) * float2( g_flSunShadowingSplitLerpFactorInvRange, g_flSunShadowingSplitLerpFactorInvRange ) );
		float flSplitLerpFactor = vSplitLerpFactorTemp.x * vSplitLerpFactorTemp.y;
		flShadowScalar = lerp( 1.0f, flShadowScalar, saturate( flSplitLerpFactor + flLerpFactorDisable ) );

		float3 vCamDelta = vPositionWs - g_vCamPosition.xyz;
		float flZLerpFactor = saturate( dot( vCamDelta, vCamDelta ) * g_flSunShadowingZLerpFactorRange + g_flSunShadowingZLerpFactorBase );
		flShadowScalar = lerp( flShadowScalar, 1.0f, flZLerpFactor );

		return flShadowScalar;
	}
#elif ( CSM_MODE == CSM_MODE_ATI_FETCH4	)
	
	#error Invalid CSM_MODE
	
#endif
