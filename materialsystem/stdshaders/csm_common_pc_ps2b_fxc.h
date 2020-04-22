//========== Copyright (c) Valve Corporation, All rights reserved. ==========//

// ps2b cascaded shadow mapping
// can be used for OSX in absence of GL3.x


// 1 2 1
// 2 4 2
// 1 2 1
float CSMSampleShadowBuffer( sampler DepthSampler, const float3 shadowMapPos )
{
	float fTexelEpsilon = g_flInvCascadeResolution; 

	float3 shadowMapCenter_objDepth = shadowMapPos.xyz;

	float3 shadowMapCenter = shadowMapCenter_objDepth.xyz;			// Center of shadow filter
	float objDepth = shadowMapCenter_objDepth.z;					// Object depth in shadow space

 	float4 vUV0 = shadowMapCenter.xyzx + float4( fTexelEpsilon,  fTexelEpsilon, 0.0f, -fTexelEpsilon );		
 	float4 vUV1 = shadowMapCenter.xyzx + float4( fTexelEpsilon, -fTexelEpsilon, 0.0f, -fTexelEpsilon );		

	float4 vOneTaps;
	vOneTaps.x = tex2Dproj( DepthSampler, float4( vUV0.xyz, 1 ) ).x;
	vOneTaps.y = tex2Dproj( DepthSampler, float4( vUV0.wyz, 1 ) ).x;
	vOneTaps.z = tex2Dproj( DepthSampler, float4( vUV1.xyz, 1 ) ).x;
	vOneTaps.w = tex2Dproj( DepthSampler, float4( vUV1.wyz, 1 ) ).x;
	float flSum = dot( vOneTaps, 1.0f );

 	float4 vUV2 = shadowMapCenter.xyzx + float4( fTexelEpsilon,  0.0f, 0.0f, -fTexelEpsilon );		
 	float4 vUV3 = shadowMapCenter.xyzy + float4( 0.0f, -fTexelEpsilon, 0.0f,  fTexelEpsilon );		

	float4 vTwoTaps;
	vTwoTaps.x = tex2Dproj( DepthSampler, float4( vUV2.xyz, 1 ) ).x;
	vTwoTaps.y = tex2Dproj( DepthSampler, float4( vUV2.wyz, 1 ) ).x;
	vTwoTaps.z = tex2Dproj( DepthSampler, float4( vUV3.xyz, 1 ) ).x;
	vTwoTaps.w = tex2Dproj( DepthSampler, float4( vUV3.xwz, 1 ) ).x;
	flSum += dot( vTwoTaps, 2.0f );

	flSum += tex2Dproj( DepthSampler, float4( shadowMapCenter, 1 ) ).x * 4.0f;

	// Sum all 9 Taps
	return flSum * ( 1.0f / 16.0f );
}


float CSMSampleShadowBuffer1Tap( float2 vPositionLs, float flComparisonDepth )
{
	#if ( CSM_VIEWMODELQUALITY == 0 )
		return tex2Dproj( CSMDepthAtlasSampler, float4( vPositionLs.x, vPositionLs.y, flComparisonDepth.x, 1.0f) ).x;
	#else
		return CSMSampleShadowBuffer( CSMDepthAtlasSampler, float3( vPositionLs.x, vPositionLs.y, flComparisonDepth ) );
	#endif
}

float CSMSampleShadowBuffer( float2 vPositionLs, float flComparisonDepth )
{
	return CSMSampleShadowBuffer1Tap( vPositionLs, flComparisonDepth );
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


float4 CSMTransformLightToTexture( float4 pos, float4x4 mat )
{
#if defined(_PS3)
	return mul( mat, pos );
#else
	return mul( pos, mat );
#endif
}

float CSMTransformLightToTexture_Element( float4 pos, float4 matRow )
{
	return mul( pos, matRow );
}

#if ( CASCADE_SIZE == 0 )
	float CSMComputeShadowing( float3 vPositionWs ) 
	{
		return 1.0f;
	}
#elif ( CSM_MODE >= 1 )

	#error Invalid CSM_MODE
	
#else

// CSM shader quality level 0 (the only supported level on gameconsole or ps_2_b)

#if defined( CSM_LIGHTMAPPEDGENERIC )

float CSMComputeShadowing( float3 vPositionWs ) 
{
	float flShadowScalar = 1.0f;

	float4 vPosition4Ws = float4( vPositionWs.xyz, 1.0f );

	//		float3 vPositionToSampleLs = float3( 0.0f, 0.0f, CSMTransformLightToTexture( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[0] ).z );
	float3 vPositionToSampleLs = float3( 0.0f, 0.0f, CSMTransformLightToTexture_Element( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices0z ) );


	// only consider cascade 1 and 2 for console/ps_2_b perf
	float2 cascadeAtlasUVScale	= float2(0.5, 0.5);
	float2 cascadeAtlasUVOffset	= float2(0.5, 0.0); // offset cascade 1

	vPositionToSampleLs.x = CSMTransformLightToTexture_Element( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices1x );
	vPositionToSampleLs.y = CSMTransformLightToTexture_Element( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices1y );

	if ( !CSMRangeTestExpanded( vPositionToSampleLs.xy ) )
	{
	//			vPositionToSampleLs.xy = CSMTransformLightToTexture( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[2] ).xy;
		vPositionToSampleLs.x = CSMTransformLightToTexture_Element( vPosition4Ws.xyzw,  g_matWorldToShadowTexMatrices2x );
		vPositionToSampleLs.y = CSMTransformLightToTexture_Element( vPosition4Ws.xyzw,  g_matWorldToShadowTexMatrices2y );

		//			cascadeAtlasUVOffset = cascadeAtlasUVOffsets_2;
		cascadeAtlasUVOffset = float2(0.0, 0.5);
	}

	vPositionToSampleLs.xy = saturate( vPositionToSampleLs.xy ) * cascadeAtlasUVScale + cascadeAtlasUVOffset;

	float3 vCamDelta = vPositionWs - g_vCamPosition.xyz;
	float flZLerpFactor = saturate( dot( vCamDelta, vCamDelta ) * g_flSunShadowingZLerpFactorRange + g_flSunShadowingZLerpFactorBase );

	flShadowScalar = CSMSampleShadowBuffer( vPositionToSampleLs.xy, vPositionToSampleLs.z );
	flShadowScalar = lerp( flShadowScalar, 1.0f, flZLerpFactor );


	return flShadowScalar;
}

#elif defined( CSM_VERTEXLIT_AND_UNLIT_GENERIC ) || defined( CSM_VERTEXLIT_AND_UNLIT_GENERIC_BUMP ) || defined( CSM_PHONG ) || defined( CSM_CHARACTER )

float CSMComputeShadowing( float3 vPositionWs, float2 lightToTextureXform0or1, float2 lightToTextureXform2, float lightToTextureXform0z ) 
{
	float flShadowScalar = 1.0f;

	float4 vPosition4Ws = float4( vPositionWs.xyz, 1.0f );

	float3 vPositionToSampleLs = float3( 0.0f, 0.0f, lightToTextureXform0z );

#if ( CSM_VIEWMODELQUALITY == 0 )

	// only consider cascade 1 and 2 for console/ps_2_b perf
	float2 cascadeAtlasUVScale	= float2(0.5, 0.5);
	float2 cascadeAtlasUVOffset	= float2(0.5, 0.0); // offset cascade 1

	vPositionToSampleLs.xy = lightToTextureXform0or1.xy;

	if ( !CSMRangeTestExpanded( vPositionToSampleLs.xy ) )
	{
		vPositionToSampleLs.xy = lightToTextureXform2.xy;
		cascadeAtlasUVOffset = float2(0.0, 0.5);
	}

	vPositionToSampleLs.xy = saturate( vPositionToSampleLs.xy ) * cascadeAtlasUVScale + cascadeAtlasUVOffset;

	float3 vCamDelta = vPositionWs - g_vCamPosition.xyz;
	float flZLerpFactor = saturate( dot( vCamDelta, vCamDelta ) * g_flSunShadowingZLerpFactorRange + g_flSunShadowingZLerpFactorBase );

	flShadowScalar = CSMSampleShadowBuffer( vPositionToSampleLs.xy, vPositionToSampleLs.z );
	flShadowScalar = lerp( flShadowScalar, 1.0f, flZLerpFactor );
#else 
	// Viewmodel shadowing
	// only use cascade 0 for viewmodel rendering
	vPositionToSampleLs.xy = lightToTextureXform0or1.xy;
	vPositionToSampleLs.xy = saturate( vPositionToSampleLs.xy ) * float2(0.5, 0.5) + float2(0.5, 0.5); // cascade 3 for viewmodel

	flShadowScalar = CSMSampleShadowBuffer( vPositionToSampleLs.xy, vPositionToSampleLs.z );
#endif // CSM_VIEWMODELQUALITY == 0

	return flShadowScalar;
}

#else

#error This shader does not support CSM

#endif


#endif // #if ( CSM_MODE == 0 )
