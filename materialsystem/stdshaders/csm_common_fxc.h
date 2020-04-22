//========== Copyright (c) Valve Corporation, All rights reserved. ==========//

#define CSM_MODE_VERY_LOW_OR_LOW 	(0)
#define CSM_MODE_MEDIUM 			(1)
#define CSM_MODE_HIGH 				(2)
#define CSM_MODE_ATI_FETCH4 		(3)


#if defined( SHADER_MODEL_PS_2_B ) && !defined( _GAMECONSOLE ) && !defined( _CONSOLE )

// Register and interpolator usage, per shader

#if defined( CSM_LIGHTMAPPEDGENERIC )

// use 7 registers which we have, just. No interpolators free at all
const float4 g_matWorldToShadowTexMatrices0z	: register( c22 );
const float4 g_matWorldToShadowTexMatrices1x	: register( c23 );
const float4 g_matWorldToShadowTexMatrices1y	: register( c24 );
const float4 g_matWorldToShadowTexMatrices2x	: register( c25 );
const float4 g_matWorldToShadowTexMatrices2y	: register( c26 );
const float4 g_vCamPosition						: register( c27 );
const float4 g_vCSMTexParams					: register( c6 );

#if ( PAINTREFRACT != 0 )
	#error PAINTREFRACT assumed to be 0 as register slots being used for csm's
#endif

#elif defined( CSM_VERTEXLIT_AND_UNLIT_GENERIC )

const float4 g_vCamPosition						: register( c8 );
const float4 g_vCSMTexParams					: register( c9 );

#if ( OUTLINE != 0 )
	#error OUTLINE assumed to be 0 as register slots being used for csm's
#endif

#if ( SEAMLESS_BASE != 0 ) || ( SEAMLESS_DETAIL != 0 )
	#error SEAMLESS_BASE or SEAMLESS_DETAIL assumed to be 0 as register slots being used for csm's
#endif

// the following interpolators are used
// baseTexCoord_csmXform1.zw : TEXCOORD0 - holds CSMTransformLightToTexture_1.xy
// detailTexCoord_csmXform2.zw : TEXCOORD1 - holds CSMTransformLightToTexture_2.xy
// worldSpaceNormal_csmXform0.w : TEXCOORD4 - holds CSMTransformLightToTexture_0.z

#elif defined( CSM_VERTEXLIT_AND_UNLIT_GENERIC_BUMP )

const float4 g_vCamPosition						: register( c17 );
const float4 g_vCSMTexParams					: register( c18 );

// the following interpolators are used
// baseTexCoord2_light0e01_or_csmXform0or1.zw : TEXCOORD0 - holds CSMTransformLightToTexture_0or1.xy
// lightAtten_csmXform2.zw : TEXCOORD2 - holds CSMTransformLightToTexture_2.xy
// vWorldNormal_light2e1_or_csmXform0.w : TEXCOORD3 - holds CSMTransformLightToTexture_0.z

#elif defined( CSM_PHONG )

const float4 g_vCamPosition						: register( c24 );
const float4 g_vCSMTexParams					: register( c25 );

// the following interpolators are used
// flTeamIdFade_csmXform0z_csmXform0or1xy.zw : TEXCOORD7 - holds CSMTransformLightToTexture_0or1.xy
// lightAtten_csmXform2.zw : TEXCOORD1 - holds CSMTransformLightToTexture_2.xy
// flTeamIdFade_csmXform0z_csmXform0or1xy.y : TEXCOORD3 - holds CSMTransformLightToTexture_0.z

#elif defined( CSM_CHARACTER )

const float4 g_vCamPosition						: register( c26 );
const float4 g_vCSMTexParams					: register( c27 );

// the following interpolators are used
// csmXform0z_csmXform0or1xy.yz : TEXCOORD7 - holds CSMTransformLightToTexture_0or1.xy
// vTexCoord0_csmXform2.zw : TEXCOORD0 - holds CSMTransformLightToTexture_2.xy
// csmXform0z_csmXform0or1xy.x : TEXCOORD7 - holds CSMTransformLightToTexture_0.z

#else

#error No support for this shader and csm's when using ps_2_b on pc

#endif

// note this is a different param mapping from non ps_2_b csmtexparams
#define g_flSunShadowingZLerpFactorBase			g_vCSMTexParams.x
#define g_flSunShadowingZLerpFactorRange		g_vCSMTexParams.y
#define g_flInvCascadeResolution				g_vCSMTexParams.z

#else

const float4 g_vCSMLightColor 					: register( c64 );
const float4 g_vCSMLightDir 					: register( c65 );
const float4 g_vCSMTexParams					: register( c66 );
const float4 g_vCSMTexParams2					: register( c67 );
const float4 g_vCSMTexParams3					: register( c68 );
const float4x4 g_matWorldToShadowTexMatrices[4] : register( c69 );
const float4 g_vCascadeAtlasUVOffsets[4] 		: register( c85 );
const float4 g_vCamPosition						: register( c89 );

#define g_flSunShadowingInvShadowTextureWidth		g_vCSMTexParams.x
#define g_flSunShadowingInvShadowTextureHeight		g_vCSMTexParams.y
#define g_flSunShadowingHalfInvShadowTextureWidth	g_vCSMTexParams.z
#define g_flSunShadowingHalfInvShadowTextureHeight	g_vCSMTexParams.w
#define g_flSunShadowingShadowTextureWidth			g_vCSMTexParams2.x
#define g_flSunShadowingShadowTextureHeight			g_vCSMTexParams2.y
#define g_flSunShadowingSplitLerpFactorBase			g_vCSMTexParams2.z
#define g_flSunShadowingSplitLerpFactorInvRange		g_vCSMTexParams2.w
#define g_flSunShadowingZLerpFactorBase				g_vCSMTexParams3.x
#define g_flSunShadowingZLerpFactorRange			g_vCSMTexParams3.y

#endif



float3 CSMVisualizePosition( float3 vPositionWs )
{
	float3 cColor = float3( 0, 0, 1 );
	
	int ix = vPositionWs.x / 8.0f;
	int iy = vPositionWs.y / 8.0f;
	
	if ( frac( iy / 2.0f ) ) 
	{
		if ( frac( ix / 2.0f ) ) 
		{
			cColor = float3( 1.0f, 0.0f, 0.0f );
		}
		else
		{
			cColor = float3( 0.0f, 1.0f, 0.0f );
		}
	}
	else
	{
		if ( frac( ix / 2.0f ) ) 
		{
			cColor = float3( 0.0f, 1.0f, 0.0f );
		}
		else
		{
			cColor = float3( 1.0f, 0.0f, 0.0f );
		}
	}		
                		    						 					
	return cColor;
}		

float3 CSMVisualizeSplit( float3 vPositionWs )
{
#if defined( SHADER_MODEL_PS_2_B )
	
	return float3( 1, 0, 0 );

#else
	float4 vPosition4Ws = float4( vPositionWs.xyz, 1.0f );
	
	float3 vPositionToSampleLs = float3( 0.0f, 0.0f, 0.0f );

	int nCascadeIndex = 0;
	vPositionToSampleLs.xy = mul( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[0] ).xy;
	
	#if ( CASCADE_SIZE > 1 )
	if ( dot( saturate( vPositionToSampleLs.xy ) - vPositionToSampleLs.xy, float2( 1, 1 ) ) != 0.0f )
	{
		nCascadeIndex = 1;
		vPositionToSampleLs.xy = mul( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[1] ).xy;

		#if ( CASCADE_SIZE > 2 )
		if ( dot( saturate( vPositionToSampleLs.xy ) - vPositionToSampleLs.xy, float2( 1, 1 ) ) != 0.0f )
		{
			nCascadeIndex = 2;
			vPositionToSampleLs.xy = mul( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[2] ).xy;
			
			#if ( CASCADE_SIZE > 3 )
			if ( dot( saturate( vPositionToSampleLs.xy ) - vPositionToSampleLs.xy, float2( 1, 1 ) ) != 0.0f )
			{
				nCascadeIndex = 3;
				vPositionToSampleLs.xy = mul( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[3] ).xy;
			}
			#endif
		}
		#endif
	}
	#endif
	
	float3 cColor = float3( 1, 0, 1 );
								
	if ( dot( saturate( vPositionToSampleLs.xy ) - vPositionToSampleLs.xy, float2( 1, 1 ) ) == 0.0f )
	{
		if ( nCascadeIndex == 0 ) 
			cColor = float3( 0, 1, 0 );
		else if ( nCascadeIndex == 1 ) 
			cColor = float3( 0, 0, 1 );
		else if ( nCascadeIndex == 2 ) 
			cColor = float3( 0, 1, 1 );
		else 
			cColor = float3( 1, 0, 0 );
	}
                		    						 					
	return cColor;
#endif
}		

#if defined( _GAMECONSOLE ) || defined(_CONSOLE)
	#include "csm_common_gameconsole_fxc.h"
#elif defined( SHADER_MODEL_PS_2_B )
	#include "csm_common_pc_ps2b_fxc.h"
#else
	#include "csm_common_pc_fxc.h"
#endif
