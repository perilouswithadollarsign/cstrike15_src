//========== Copyright (c) Valve Corporation, All rights reserved. ==========//

// X360 and PS3 cascaded shadow mapping

// 7LS TODO - PS3
#if defined(_X360)

	float CSMSampleShadowBuffer360Simple( sampler DepthSampler, const float3 vProjCoords )
	{
		float fLOD;
		float2 shadowMapCenter = vProjCoords.xy;			// Center of shadow filter
		float objDepth = min( vProjCoords.z, 0.99999 );		// Object depth in shadow space

		// TODO: why doesn't the reverse depth path work with CSM's here since CPU side is set to flip z

	//#if defined( REVERSE_DEPTH_ON_X360 )
	// 	objDepth = 1.0f - objDepth;
	//#endif	

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

	//#if defined( REVERSE_DEPTH_ON_X360 )
	// 	float4 vCompare = (vSampledDepths < objDepth.xxxx);
	//#else
		float4 vCompare = (vSampledDepths > objDepth.xxxx);
	//#endif

		return 1.0f - dot( vCompare, vWeights );
	}

	float CSMSampleShadowBuffer3604x4( sampler DepthSampler, const float3 vProjCoords )
	{
		float2 vShadowMapCenter = vProjCoords.xy + float2( .5f / 1408.0f, .5f / 1408.0f );			// Center of shadow filter
		
		// This shader assumes REVERSE_DEPTH_ON_X360 is always defined.
		// TODO: why doesn't the reverse depth path work on CSM's since CPU code is set to flip z


		//float flObjDepth = 1.0f - min( vProjCoords.z, 0.99999f );		// Object depth in shadow space
		float flObjDepth = min( vProjCoords.z, 0.99999f );		// Object depth in shadow space

		// projective distance from z plane in view coords
		float4 vDist4 = float4( flObjDepth, flObjDepth, flObjDepth, flObjDepth );

		//fraction component of projected coordinates; here FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION represents the shadowmap size
		float2 vTexRes = float2( 1408.0f, 1408.0f );
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
				sgt vInLight, vShadowMapVals, vDist4
			};
			//7LS flipped z sub for above sgt args 
			//sgt vInLight, vDist4, vShadowMapVals
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
				sgt vInLight, vShadowMapVals, vDist4
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
				sgt vInLight, vShadowMapVals, vDist4
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
				sgt vInLight, vShadowMapVals, vDist4
			};
			float4 vShadowMapWeights = float4( vWeights.x, vWeights.y, vWeights.x * vWeights.y, 1 );
			flPercentInLight += dot( vInLight, vShadowMapWeights );
		}

		//sum of weights is 9  since border taps are bilinearly filtered
		return 1.0f - (( 1.0f / 9.0f ) * flPercentInLight);
	}

#elif defined(_PS3)

	// keep this in sync with c_env_cascade_light.cpp
	#define CASCADE_RESOLUTION 768

	// 1 2 1
	// 2 4 2
	// 1 2 1
	// Tweaked for good code gen with the SCE Cg compiler.
	half CSMSampleShadowBufferPS33x3( sampler DepthSampler, const float3 shadowMapPos )
	{
		float fTexelEpsilon = 1.0f / CASCADE_RESOLUTION;

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

	#error Unsupported

#endif // #elif defined(_PS3)

float CSMSampleShadowBuffer1Tap( float2 vPositionLs, float flComparisonDepth )
{
#if defined(_X360)
	#if (CSM_VIEWMODELQUALITY == 0)
		return CSMSampleShadowBuffer360Simple( CSMDepthAtlasSampler, float3( vPositionLs.x, vPositionLs.y, flComparisonDepth ) );
	#else
		return CSMSampleShadowBuffer3604x4( CSMDepthAtlasSampler, float3( vPositionLs.x, vPositionLs.y, flComparisonDepth ) );
	#endif
#elif defined(_PS3)
	#if (CSM_VIEWMODELQUALITY == 0)
		return tex2Dproj( CSMDepthAtlasSampler, float4( vPositionLs.x, vPositionLs.y, flComparisonDepth.x, 1.0f ) );
	#else
		return CSMSampleShadowBufferPS33x3( CSMDepthAtlasSampler, float3( vPositionLs.x, vPositionLs.y, flComparisonDepth ) );
	#endif
#else
	#error Unsupported
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

float CSMComputeSplitLerpFactor( float2 vPositionToSampleLs )
{
	float2 vSplitLerpFactorTemp = float2( 1.0f, 1.0f ) - saturate( ( abs( vPositionToSampleLs.xy - float2( .5f, .5f ) ) - float2( g_flSunShadowingSplitLerpFactorBase, g_flSunShadowingSplitLerpFactorBase ) ) * float2( g_flSunShadowingSplitLerpFactorInvRange, g_flSunShadowingSplitLerpFactorInvRange ) );
	return vSplitLerpFactorTemp.x * vSplitLerpFactorTemp.y;
}

float4 CSMTransformLightToTexture( float4 pos, float4x4 mat )
{
#if defined(_PS3)
	return mul( mat, pos );
#else
	return mul( pos, mat );
#endif
}

#if ( CASCADE_SIZE == 0 )
	float CSMComputeShadowing( float3 vPositionWs ) 
	{
		return 1.0f;
	}
#elif ( CSM_MODE >= 1 )

	#error Invalid CSM_MODE
	
#else
	// CSM shader quality level 0 (the only supported level on gameconsole)
	float CSMComputeShadowing( float3 vPositionWs ) 
	{
		float flShadowScalar = 1.0f;

		float4 vPosition4Ws = float4( vPositionWs.xyz, 1.0f );

		float3 vPositionToSampleLs = float3( 0.0f, 0.0f, CSMTransformLightToTexture( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[0] ).z );

#if ( CSM_VIEWMODELQUALITY == 0 )

		// only consider cascade 1 and 2 for console perf

#if defined(_PS3)
		float4 cascadeAtlasUVOffsets_1 = g_vCascadeAtlasUVOffsets[1];
		float4 cascadeAtlasUVOffsets_2 = g_vCascadeAtlasUVOffsets[2];

		float4 cascadeAtlasUVOffset = cascadeAtlasUVOffsets_1;
#else
		int nCascadeIndex = 1;
#endif

		vPositionToSampleLs.xy = CSMTransformLightToTexture( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[1] ).xy;

		if ( !CSMRangeTestExpanded( vPositionToSampleLs.xy ) )
		{
			vPositionToSampleLs.xy = CSMTransformLightToTexture( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[2] ).xy;
#if defined(_PS3)
			cascadeAtlasUVOffset = cascadeAtlasUVOffsets_2;
#else
			nCascadeIndex = 2;
#endif
		}

#if defined(_PS3)
		vPositionToSampleLs.xy = saturate( vPositionToSampleLs.xy ) * cascadeAtlasUVOffset.zw + cascadeAtlasUVOffset.xy;
#else
		vPositionToSampleLs.xy = saturate( vPositionToSampleLs.xy ) * g_vCascadeAtlasUVOffsets[nCascadeIndex].zw + g_vCascadeAtlasUVOffsets[nCascadeIndex].xy;
#endif
		float3 vCamDelta = vPositionWs - g_vCamPosition.xyz;
		float flZLerpFactor = saturate( dot( vCamDelta, vCamDelta ) * g_flSunShadowingZLerpFactorRange + g_flSunShadowingZLerpFactorBase );

		flShadowScalar = CSMSampleShadowBuffer( vPositionToSampleLs.xy, vPositionToSampleLs.z );
		flShadowScalar = lerp( flShadowScalar, 1.0f, flZLerpFactor );
#else 
    	// Viewmodel shadowing
		// only use cascade 0 for viewmodel rendering

		vPositionToSampleLs.xy = CSMTransformLightToTexture( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[0] ).xy;

		vPositionToSampleLs.xy = saturate( vPositionToSampleLs.xy ) * g_vCascadeAtlasUVOffsets[0].zw + g_vCascadeAtlasUVOffsets[0].xy;

		flShadowScalar = CSMSampleShadowBuffer( vPositionToSampleLs.xy, vPositionToSampleLs.z );
#endif // CSM_VIEWMODELQUALITY == 0

		return flShadowScalar;
	}

#endif // #if ( CSM_MODE == 0 )
