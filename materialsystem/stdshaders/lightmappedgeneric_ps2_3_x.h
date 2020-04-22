//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================

//	SKIP: $NORMALMAPALPHAENVMAPMASK && $BASEALPHAENVMAPMASK
//	SKIP: $NORMALMAPALPHAENVMAPMASK && $ENVMAPMASK
//	SKIP: $BASEALPHAENVMAPMASK && $ENVMAPMASK
//	SKIP: $BASEALPHAENVMAPMASK && $SELFILLUM
//  SKIP: !$FASTPATH && $FASTPATHENVMAPCONTRAST
//  SKIP: !$FASTPATH && $FASTPATHENVMAPTINT
//	SKIP: !$BUMPMAP && $BUMPMAP2
//	SKIP: $BASEALPHAENVMAPMASK && ( $BUMPMAP && !$ENVMAPANISOTROPY )
//  SKIP: $SEAMLESS && ( $DETAIL_BLEND_MODE != 12 )
//  SKIP: $BUMPMASK && ( $SEAMLESS ||  ( $DETAILTEXTURE != 12 ) || $SELFILLUM || $BASETEXTURE2 )
//  SKIP: $ENVMAPANISOTROPY && !$ENVMAP && ( $BUMPMAP != 1 )
//  SKIP: $ENVMAPANISOTROPY && $NORMALMAPALPHAENVMAPMASK
//  SKIP: !$BUMPMAP && $ADDBUMPMAPS
//  SKIP: !$BUMPMAP2 && $ADDBUMPMAPS
//  SKIP: $BUMPMASK && $ADDBUMPMAPS
//  SKIP: $ADDBUMPMAPS && ( $DETAIL_BLEND_MODE != 12 ) [ps20]
//  SKIP: ( $DETAIL == 2 ) && !$BASETEXTURE2

// 360 compiler craps out on some combo in this family.  Content doesn't use blendmode 10 anyway
//  SKIP: $FASTPATH && $PIXELFOGTYPE && $BASETEXTURE2 && $CUBEMAP && ($DETAIL_BLEND_MODE == 10 ) [CONSOLE]

// Turning off 32bit lightmaps on Portal 2 to save shader perf. --Thorsten
//#define USE_32BIT_LIGHTMAPS_ON_360 //uncomment to use 32bit lightmaps, be sure to keep this in sync with the same #define in materialsystem/cmatlightmaps.cpp

#if defined( SHADER_MODEL_PS_2_0 )
//#error ps2.0 support removed for this shader
#endif

// NOTE: This has to be before inclusion of common_lightmappedgeneric_fxc.h to get the vertex format right!
#if ( DETAIL_BLEND_MODE == 12 )
	#define DETAILTEXTURE 0
#else
	#if ( DETAIL2 == 1)
		#define DETAILTEXTURE 2
	#else
		#define DETAILTEXTURE 1
	#endif
#endif

#include "common_ps_fxc.h"
#include "common_flashlight_fxc.h"
#define PIXELSHADER
#include "common_lightmappedgeneric_fxc.h"

#if SEAMLESS
	#define USE_FAST_PATH 1
#else
	#define USE_FAST_PATH FASTPATH
#endif

const float4 g_EnvmapTint : register( c0 );

#if ( USE_FAST_PATH == 1 || LIGHTING_PREVIEW == 1 )

	#if FASTPATHENVMAPCONTRAST == 0
		static const float3 g_EnvmapContrast = { 0.0f, 0.0f, 0.0f };
	#else
		static const float3 g_EnvmapContrast = { 1.0f, 1.0f, 1.0f };
	#endif
	static const float3 g_EnvmapSaturation = { 1.0f, 1.0f, 1.0f };
	static const float g_FresnelReflection = 1.0f;
	static const float g_OneMinusFresnelReflection = 0.0f;
	static const float4 g_SelfIllumTint = { 1.0f, 1.0f, 1.0f, 1.0f };

#else // ( USE_FAST_PATH == 0 )

	const float3 g_EnvmapContrast				: register( c2 );
	const float3 g_EnvmapSaturation				: register( c3 );
	const float4 g_FresnelReflectionReg			: register( c4 );
	#define g_FresnelReflection g_FresnelReflectionReg.a
	#define g_OneMinusFresnelReflection g_FresnelReflectionReg.b
	const float4 g_SelfIllumTint					: register( c7 );

#endif

const float4 g_DetailTint_and_BlendFactor	: register( c8 );
#define g_DetailTint (g_DetailTint_and_BlendFactor.rgb)
#define g_DetailBlendFactor (g_DetailTint_and_BlendFactor.w)

#if ADDBUMPMAPS == 1
#define g_vAddBumpMapScale1 g_DetailTint_and_BlendFactor.r;
#define g_vAddBumpMapScale2 g_DetailTint_and_BlendFactor.g;
#endif

const float4 g_Detail2Tint_and_BlendFactor : register(c9);
#define g_Detail2Tint (g_Detail2Tint_and_BlendFactor.rgb)
#define g_Detail2BlendFactor (g_Detail2Tint_and_BlendFactor.w)

const float3 g_EyePos						: register( c10 );
const float4 g_FogParams						: register( c11 );
const float4 g_TintValuesTimesLightmapScale	: register( c12 );

#define g_flAlpha2 g_TintValuesTimesLightmapScale.w

const float4 g_FlashlightAttenuationFactors	: register( c13 );
const float3 g_FlashlightPos				: register( c14 );
const float4x4 g_FlashlightWorldToTexture	: register( c15 ); // through c18
const float4 g_ShadowTweaks					: register( c19 );

#if !defined( SHADER_MODEL_PS_2_0 ) && ( FLASHLIGHT == 0 )
	#define g_cAmbientColor cFlashlightScreenScale.rgb
	//const float3 g_cAmbientColor				: register( c31 );
#endif

#if ( ( CUBEMAP == 2 ) || ( ENVMAPANISOTROPY ) )
	const float4 g_envMapParams : register( c20 );
#endif

#if ( CUBEMAP == 2 )
	#define g_DiffuseCubemapScale g_envMapParams.y
	#define g_fvDiffuseCubemapMin float3( g_envMapParams.z, g_envMapParams.z, g_envMapParams.z )
	#define g_fvDiffuseCubemapMax float3( g_envMapParams.w, g_envMapParams.w, g_envMapParams.w )
#endif

#if ( ENVMAPANISOTROPY )
	#define g_EnvmapAnisotropyScale g_envMapParams.x
#endif

#if defined( SHADER_MODEL_PS_3_0 )
const float3 g_TintValuesWithoutLightmapScale	: register( c21 );
#else
const float4 g_vCSMLightColor : register(c21);
#endif

#if ( PHONG )
const float4 g_Phong_Exp_and_BaseTint : register( c22 );
#define g_PhongExp g_Phong_Exp_and_BaseTint.x
#define g_PhongTint g_Phong_Exp_and_BaseTint.y
#define g_PhongExp2 g_Phong_Exp_and_BaseTint.z
#define g_PhongTint2 g_Phong_Exp_and_BaseTint.w

const float4 g_PhongMask_Contrast_and_Brightness : register( c23 );
#define g_PhongMaskContrast g_PhongMask_Contrast_and_Brightness.x
#define g_PhongMaskBrightness g_PhongMask_Contrast_and_Brightness.y
#define g_PhongMaskContrast2 g_PhongMask_Contrast_and_Brightness.z
#define g_PhongMaskBrightness2 g_PhongMask_Contrast_and_Brightness.w

const float4 g_PhongAmount : register( c24 );
const float4 g_PhongAmount2 : register( c25 );
#endif

#if ( ENVMAPMASK ) && defined( SHADER_MODEL_PS_3_0 )
const float4 g_EnvmapMaskTexCoordTransform[2]	: register( c35 );
const float4 g_EnvmapMaskTexCoordTransform2[2]  : register( c37 );
#endif

sampler BaseTextureSampler			: register( s0 );
sampler LightmapSampler				: register( s1 );
samplerCUBE EnvmapSampler			: register( s2 );

#if FANCY_BLENDING
	sampler BlendModulationSampler	: register( s3 );
	#if ( CASCADED_SHADOW_MAPPING ) && defined( SHADER_MODEL_PS_3_0 ) && ( BUMPMAP > 0 )
		const float4 g_vDropShadowParams : register( c26 );
		#define g_fDropShadowScale g_vDropShadowParams.x
		#define g_fDropShadowOpacity g_vDropShadowParams.y
		#define g_fDropShadowHighlightScale g_vDropShadowParams.z
		#define g_fDropShadowDepthExaggeration g_vDropShadowParams.w
	#endif
#endif

#if ( BASETEXTURE2 ) && defined( SHADER_MODEL_PS_3_0 )
	const float4 g_vTintLayer1			: register( c33 );
	const float4 g_vTintLayer2			: register( c34 );
#endif

#if ( FANCY_BLENDING >= 2 ) && defined( SHADER_MODEL_PS_3_0 )
	const float4 g_vBlendParams			: register( c46 );
	#define g_flBlendSoftness g_vBlendParams.x
	#define g_flLayerBorderStrength g_vBlendParams.y
	#define g_flLayerBorderOffset g_vBlendParams.z
	#define g_flLayerBorderSoftness g_vBlendParams.w
	const float4 g_vLayerBorderTint		: register( c47 );
	const float4 g_vEdgeBlendParams		: register( c48 );
	#define g_flLayerNormalEdgePunchInSign g_vEdgeBlendParams.x
	#define g_flLayerNormalEdgeStrength g_vEdgeBlendParams.y
	#define g_flLayerNormalEdgeOffset g_vEdgeBlendParams.z
	#define g_flLayerNormalEdgeSoftness g_vEdgeBlendParams.w
#endif

#if ( DETAILTEXTURE != 0 )
	sampler DetailSampler			: register( s12 );

	#if ( DETAILTEXTURE == 2 )
		sampler DetailSampler2		: register( s9 );
	#endif
#endif

sampler BumpmapSampler				: register( s4 );

#if (BUMPMAP == 1) && defined( _PS3 )
// Causes the Cg compiler to automatically produce _bx2 modifier on the texture load instead of producing a MAD to range expand the vector, saving one instruction.
#pragma texsign BumpmapSampler
#pragma texformat BumpmapSampler RGBA8
#endif

#if BUMPMAP2 == 1
	sampler BumpmapSampler2			: register( s5 );
#endif

#if ( ENVMAPMASK ) && defined( SHADER_MODEL_PS_3_0 )
sampler EnvmapMaskSampler			: register( s6 );
#if ( BASETEXTURE2 )
	sampler EnvmapMaskSampler2		: register( s10 );
#endif
#endif

sampler BaseTextureSampler2			: register( s7 );

#if BUMPMASK == 1
	sampler BumpMaskSampler			: register( s8 );
	#if NORMALMASK_DECODE_MODE == NORM_DECODE_ATI2N_ALPHA
		sampler AlphaMaskSampler	: register( s11 );	// alpha
	#else
		#define AlphaMaskSampler		BumpMaskSampler
	#endif
#endif

#if ( defined( _X360 ) || defined( _PS3 ) ) && FLASHLIGHT
	sampler FlashlightSampler		: register( s13 );
	sampler ShadowDepthSampler		: register( s14 );
	sampler RandRotSampler			: register( s15 );

#if defined(_PS3)
// Needed for optimal shadow filter code generation on PS3.
#pragma texformat ShadowDepthSampler DEPTH_COMPONENT24
#endif

#endif

#ifdef PHONG_DEBUG
#undef PHONG_DEBUG
#endif
#define PHONG_DEBUG 0


//const float g_flTime : register( c24 );

float Luminance( float3 cColor )
{
	// Formula for calculating luminance based on NTSC standard
	return dot( cColor.rgb, float3( 0.2125, 0.7154, 0.0721 ) );
}

//-----------------------------------------------------------------------------------------------------------------------------

#if ( CASCADED_SHADOW_MAPPING ) && !defined( _X360 ) && !defined( _PS3 ) && !defined( SHADER_MODEL_PS_2_B )
const bool g_bCSMEnabled : register(b0);
#undef CASCADE_SIZE
#define CASCADE_SIZE 1
#endif

#if ( CASCADE_SIZE > 0 )
	#undef CASCADE_SIZE
	#define CASCADE_SIZE 3
#endif

#if ( ( CASCADED_SHADOW_MAPPING ) && ( CASCADE_SIZE > 0 ) )
	sampler CSMDepthAtlasSampler : register( s15 );
	
	#if defined(_PS3)
		// Needed for optimal shadow filter code generation on PS3.
		#pragma texformat CSMDepthAtlasSampler DEPTH_COMPONENT24
	#endif

	#if defined( SHADER_MODEL_PS_2_B )
		#define CSM_LIGHTMAPPEDGENERIC
	#endif

	#include "csm_common_fxc.h"
	#include "csm_blending_fxc.h"
#endif

//-----------------------------------------------------------------------------------------------------------------------------

#if defined( _X360 )
	// The compiler runs out of temp registers in certain combos, increase the maximum for now
	#if ( BASETEXTURE2 && (BUMPMAP == 2) && CUBEMAP && NORMALMAPALPHAENVMAPMASK && DIFFUSEBUMPMAP && FLASHLIGHT && SHADER_SRGB_READ )
		[maxtempreg(44)]
	#elif ( SHADER_SRGB_READ == 1 )
		[maxtempreg(41)]
	#else
		[maxtempreg(36)]
	#endif
#endif

#if LIGHTING_PREVIEW == 2
LPREVIEW_PS_OUT main( PS_INPUT i )
#else
float4_color_return_type main( PS_INPUT i ) : COLOR
#endif
{
	bool bBaseTexture2 = BASETEXTURE2 ? true : false;
	bool bDetailTexture = ( DETAILTEXTURE != 0 ) ? true : false;
	bool bDetailTexture2 = ( DETAILTEXTURE == 2 ) ? true : false;
	bool bBumpmap = BUMPMAP ? true : false;
	bool bDiffuseBumpmap = DIFFUSEBUMPMAP ? true : false;
	bool bEnvmapMask = ENVMAPMASK ? true : false;
	bool bBaseAlphaEnvmapMask = BASEALPHAENVMAPMASK ? true : false;
	bool bSelfIllum = SELFILLUM ? true : false;
	bool bNormalMapAlphaEnvmapMask = NORMALMAPALPHAENVMAPMASK ? true : false;

	HALF4 baseColor = 0.0h;
	HALF4 baseColor2 = 0.0h;
	HALF4 vNormal = HALF4( 0, 0, 1, 1 );
	float3 baseTexCoords = float3( 0, 0, 0 );
	float3 baseTexCoords2 = float3( 0, 0, 0 );
	float3 worldPos = i.worldPos_projPosZ.xyz;
	HALF3x3 tangenttranspose = HALF3x3( HALF3(i.tangentSpaceTranspose0_vertexBlendX.xyz), HALF3(i.tangentSpaceTranspose1_bumpTexCoord2u.xyz), HALF3(i.tangentSpaceTranspose2_bumpTexCoord2v.xyz) );

	float3 worldVertToEyeVector = g_EyePos - worldPos;
	#if SEAMLESS
		baseTexCoords  = i.SeamlessTexCoord.xyz;
		baseTexCoords2 = i.SeamlessTexCoord.xyz;
	#else
		baseTexCoords.xy  = i.BASETEXCOORD;
		baseTexCoords2.xy = i.BASETEXCOORD2;
	#endif

	float3 coords  = baseTexCoords;
	float3 coords2 = baseTexCoords2;


	float2 detailTexCoord = 0.0f;
	float2 detailTexCoord2 = 0.0f;
	float2 bumpmapTexCoord = 0.0f;
	float2 bumpmapTexCoord2 = 0.0f;

	#if ( DETAILTEXTURE != 0 )
		detailTexCoord = i.DETAILCOORD;

		#if ( DETAILTEXTURE == 2 ) && defined( SHADER_MODEL_PS_3_0 )
			detailTexCoord2 = i.DETAILCOORD2;
		#endif
	#endif

	#if BUMPMAP
		bumpmapTexCoord = i.BUMPCOORD;
		bumpmapTexCoord2 = float2( i.BUMPCOORD2U, i.BUMPCOORD2V );
	#endif

	GetBaseTextureAndNormal( BaseTextureSampler, BaseTextureSampler2, BumpmapSampler,
							 bBaseTexture2, bBumpmap || bNormalMapAlphaEnvmapMask, 
							 coords, coords2, bumpmapTexCoord, 
							 (HALF3)i.vertexColor.rgb, baseColor, baseColor2, vNormal );
	#if ( ENVMAPANISOTROPY )
		HALF anisotropyFactor = g_EnvmapAnisotropyScale;
	#endif
	#if BUMPMAP == 1	// not ssbump
		vNormal.xyz = vNormal.xyz * 2.0h - 1.0h;					// make signed if we're not ssbump
		HALF3 vThisReallyIsANormal = vNormal.xyz;
		#if ( ENVMAPANISOTROPY )
			anisotropyFactor *= (HALF)vNormal.a;
		#endif
	#endif

	HALF4 lightmapColor1 = HALF4( 1.0, 1.0, 1.0, 1.0 );
	HALF4 lightmapColor2 = HALF4( 1.0, 1.0, 1.0, 1.0 );
	HALF4 lightmapColor3 = HALF4( 1.0, 1.0, 1.0, 1.0 );

	#if LIGHTING_PREVIEW == 0
		if ( bBumpmap && bDiffuseBumpmap )
		{
			float2 bumpCoord1;
			float2 bumpCoord2;
			float2 bumpCoord3;
			ComputeBumpedLightmapCoordinates( i.lightmapTexCoord1And2, i.lightmapTexCoord3_bumpTexCoord.xy,
				bumpCoord1, bumpCoord2, bumpCoord3 );

			lightmapColor1 = LightMapSample( LightmapSampler, bumpCoord1 );
			lightmapColor2 = LightMapSample( LightmapSampler, bumpCoord2 );
			lightmapColor3 = LightMapSample( LightmapSampler, bumpCoord3 );
		}
		else
		{
			float2 bumpCoord1 = ComputeLightmapCoordinates( i.lightmapTexCoord1And2, i.lightmapTexCoord3_bumpTexCoord.xy );
			lightmapColor1 = LightMapSample( LightmapSampler, bumpCoord1 );
		}
	#endif

	HALF4 detailColor = HALF4( 1.0f, 1.0f, 1.0f, 1.0f );
	HALF4 detailColor2 = HALF4( 1.0f, 1.0f, 1.0f, 1.0f );

	#if ( DETAILTEXTURE != 0 )
		#if SHADER_MODEL_PS_2_0 || ADDBUMPMAPS == 1
			detailColor = h4tex2D( DetailSampler, detailTexCoord );			
		#else
			detailColor = HALF4( g_DetailTint, 1.0h ) * h4tex2D( DetailSampler, detailTexCoord );
		#endif

		#if ( DETAILTEXTURE == 2 )
			detailColor2 = HALF4( g_Detail2Tint, 1.0h ) * h4tex2D( DetailSampler2, detailTexCoord2 );
		#endif
	#endif

	HALF blendedAlpha = baseColor.a;

	HALF blendfactor = i.tangentSpaceTranspose0_vertexBlendX.w;

#if ( BASETEXTURE2 ) && defined( SHADER_MODEL_PS_3_0 )
	baseColor.rgb *= g_vTintLayer1.rgb;
	baseColor2.rgb *= g_vTintLayer2.rgb;
#endif

#if ( PHONG )
	// save off basecolor for phong mask generation before it potentially gets overwritten with a blend of basecolor and basecolor2
	HALF4 baseColor1 = baseColor;
#endif

	HALF4 vBlendModulateTexel = HALF4( 0.0f, 0.0f, 0.0f, 0.0f );
	float flBlendModulateFactor = 0.0f;
	
	if ( bBaseTexture2 )
	{
#if (SELFILLUM == 0) && (PIXELFOGTYPE != PIXEL_FOG_TYPE_HEIGHT) && (FANCY_BLENDING == 1) && (SEAMLESS == 0)
		vBlendModulateTexel = h4tex2D( BlendModulationSampler, i.BLENDMODULATECOORD );
		HALF minb=max(0, vBlendModulateTexel.g - vBlendModulateTexel.r );
		HALF maxb=min(1, vBlendModulateTexel.g + vBlendModulateTexel.r );
		blendfactor=smoothstep(minb,maxb,blendfactor);
		#if ( CASCADED_SHADOW_MAPPING == 1 ) && ( BUMPMAP > 0 ) && defined( SHADER_MODEL_PS_3_0 )// drop shadows on blend textures
			if ( g_fDropShadowOpacity > 0.0f )
			{
				float3 vWorldLightDir = normalize( g_vCSMLightDir );
				float3x3 worldToTangentSpace = transpose( tangenttranspose );
				HALF2 vShadowOffset = float2( 0, 0 );
				vShadowOffset.x = dot( vWorldLightDir, worldToTangentSpace[0] );
				vShadowOffset.y = dot( vWorldLightDir, worldToTangentSpace[1] );
				HALF NdotL = dot( vWorldLightDir, tangenttranspose[2] );
				HALF HNdotL = NdotL * 0.5f + 0.5f;
				HALF fShadowOffset = ( vBlendModulateTexel.g - pow( vBlendModulateTexel.g, g_fDropShadowDepthExaggeration ) * g_fDropShadowDepthExaggeration ) * g_fDropShadowScale;
				HALF fHighlightOffset = -vBlendModulateTexel.g * g_fDropShadowHighlightScale;
				fShadowOffset = lerp( fShadowOffset, fHighlightOffset, blendfactor );
				vShadowOffset = vShadowOffset * fShadowOffset * NdotL;
				HALF4 vShadowSample = h4tex2D( BlendModulationSampler, i.BLENDMODULATECOORD + vShadowOffset );
				minb=max(0.0039, vShadowSample.g - vShadowSample.r );
				maxb=min(1, vShadowSample.g + vShadowSample.r );
				HALF dropshadow=smoothstep( maxb, minb, i.tangentSpaceTranspose0_vertexBlendX.w );
				baseColor.rgb *= lerp( 1.0f, max( dropshadow, 1.0f - g_fDropShadowOpacity ), smoothstep( 0.1f, 0.5f, HNdotL )  );
				HALF highlight=smoothstep(maxb,minb,i.tangentSpaceTranspose0_vertexBlendX.w) * g_fDropShadowOpacity;
				baseColor2.rgb += baseColor2.rgb * highlight * HNdotL;
			}
		#endif
#elif (FANCY_BLENDING >= 2) && (SELFILLUM == 0) && (PIXELFOGTYPE != PIXEL_FOG_TYPE_HEIGHT) && (SEAMLESS == 0) && defined( SHADER_MODEL_PS_3_0 )
		vBlendModulateTexel = h4tex2D( BlendModulationSampler, i.BLENDMODULATECOORD );
		flBlendModulateFactor = vBlendModulateTexel.g;
		#if ( FANCY_BLENDING == 3 )
		{
			flBlendModulateFactor = vBlendModulateTexel.a;
		}
		#endif

		HALF minb = max( 0, flBlendModulateFactor - g_flBlendSoftness );
		HALF maxb = min( 1, flBlendModulateFactor + g_flBlendSoftness );
		
		float flBlendfactor = smoothstep( minb, maxb, blendfactor );

		HALF minborder = max( 0, flBlendModulateFactor - g_flLayerBorderSoftness );
		HALF maxborder = min( 1, flBlendModulateFactor + g_flLayerBorderSoftness );
		float flBorderWeight = smoothstep( minborder, maxborder, saturate( blendfactor - g_flLayerBorderOffset ) );
		float flBorderStrength = ( 1.0 - abs( flBorderWeight * 2.0 - 1.0 ) ) * g_flLayerBorderStrength;
		baseColor.rgb *= lerp( float3( 1.0, 1.0, 1.0 ), g_vLayerBorderTint.rgb, flBorderStrength );

		blendfactor = flBlendfactor;
#endif
		baseColor.rgb = lerp( baseColor.rgb, baseColor2.rgb, blendfactor );
		blendedAlpha = lerp( baseColor.a, baseColor2.a, blendfactor );
	}

	HALF3 specularFactor = 1.0h;
	HALF4 vNormalMask = HALF4(0, 0, 1, 1);

	if ( bBumpmap )
	{
		#if ( BUMPMAP2 == 1 )
		{
			float2 b2TexCoord = bumpmapTexCoord2;

			HALF4 vNormal2;
			#if ( BUMPMAP == 2 )
			{
				vNormal2 = h4tex2D( BumpmapSampler2, b2TexCoord );
			}
			#else
			{
				HALF4 normalTexel = h4tex2D( BumpmapSampler2, b2TexCoord );
				vNormal2 = HALF4( normalTexel.xyz * 2.0h - 1.0h, normalTexel.a );
			}
			#endif

			#if ( BUMPMASK == 1 )
				HALF3 vNormal1 = DecompressNormal( BumpmapSampler, i.BUMPCOORD, NORMALMASK_DECODE_MODE, AlphaMapSampler );

				vNormal.xyz = normalize( vNormal1.xyz + vNormal2.xyz );

				// Third normal map...same coords as base
				normalTexel = h4tex2D( BumpMaskSampler, i.BASETEXCOORD );
				vNormalMask = HALF4( normalTexel.xyz * 2.0h - 1.0h, normalTexel.a );

				vNormal.xyz = lerp( vNormalMask.xyz, vNormal.xyz, vNormalMask.a );		// Mask out normals from vNormal
				specularFactor = vNormalMask.a;
			#else // BUMPMASK == 0

				#if ADDBUMPMAPS == 1
					vNormal.xy *= g_vAddBumpMapScale1;
					vNormal2.xy *= g_vAddBumpMapScale2;
					vNormal.xyz = normalize( vNormal.xyz + vNormal2.xyz );
				#elif (FANCY_BLENDING == 3) && (SELFILLUM == 0) && (PIXELFOGTYPE != PIXEL_FOG_TYPE_HEIGHT) && (SEAMLESS == 0) && defined( SHADER_MODEL_PS_3_0 )
					float3 vEdgeNormal = float3( vBlendModulateTexel.xy * 2 - 1, 0.0 );
					vEdgeNormal.xy *= g_flLayerNormalEdgePunchInSign;

					HALF minedge = max( 0, flBlendModulateFactor - g_flLayerNormalEdgeSoftness );
					HALF maxedge = min( 1, flBlendModulateFactor + g_flLayerNormalEdgeSoftness );
					float flEdgeWeight = smoothstep( minedge, maxedge, saturate( i.tangentSpaceTranspose0_vertexBlendX.w - g_flLayerNormalEdgeOffset ) );
					float flEdgeBlendStrength = ( 1.0 - abs( flEdgeWeight * 2.0 - 1.0 ) ) * g_flLayerNormalEdgeStrength;
					flEdgeBlendStrength *= blendfactor;
					flEdgeBlendStrength = saturate( flEdgeBlendStrength );

					vNormal2.xyz = lerp( vNormal2.xyz, vEdgeNormal.xyz, flEdgeBlendStrength );
					vNormal2.xyz = normalize( vNormal2.xyz );
					vNormal.xyz = lerp( vNormal.xyz, vNormal2.xyz, blendfactor);

				#else
					vNormal.xyz = lerp( vNormal.xyz, vNormal2.xyz, blendfactor);
				#endif

			#endif
			
			if ( bNormalMapAlphaEnvmapMask )
			{
				specularFactor *= (HALF)vNormal.a;
				// Mappers don't like that the 2nd normal alpha contributes to the envmap mask.
				//specularFactor *= lerp( vNormal.a, vNormal2.a, blendfactor );
			}
		}
		#else // BUMPMAP2 == 1
		{
			if ( bNormalMapAlphaEnvmapMask )
			{
				specularFactor *= (HALF)vNormal.a;
			}
		}
		#endif // BUMPMAP2 == 1
	}
	else if ( bNormalMapAlphaEnvmapMask )
	{
		specularFactor *= (HALF)vNormal.a;
	}

#if ENVMAPMASK && defined( SHADER_MODEL_PS_3_0 )
	{
		// note - dropped support for sm2/2b
		float2 envmapMaskTexCoord = float2( dot( i.ENVMAPMASKCOORD, g_EnvmapMaskTexCoordTransform[0].xy ) + g_EnvmapMaskTexCoordTransform[0].w,
											dot( i.ENVMAPMASKCOORD, g_EnvmapMaskTexCoordTransform[1].xy ) + g_EnvmapMaskTexCoordTransform[1].w );
		float3 envmapMask = h3tex2D( EnvmapMaskSampler, envmapMaskTexCoord ).xyz;

		#if BASETEXTURE2
		{
			float2 envmapMaskTexCoord2 = float2( dot( i.ENVMAPMASKCOORD, g_EnvmapMaskTexCoordTransform2[0].xy ) + g_EnvmapMaskTexCoordTransform2[0].w,
												 dot( i.ENVMAPMASKCOORD, g_EnvmapMaskTexCoordTransform2[1].xy ) + g_EnvmapMaskTexCoordTransform2[1].w );
			float3 envmapMask2 = h3tex2D( EnvmapMaskSampler2, envmapMaskTexCoord2 ).xyz;

			envmapMask.rgb = lerp( envmapMask.rgb, envmapMask2.rgb, blendfactor );
		}
		#endif

		specularFactor *= envmapMask;
	}
#endif

	if ( bBaseAlphaEnvmapMask )
	{
		specularFactor *= 1.0h - blendedAlpha; // Reversing alpha blows!
	}
	
	HALF4 albedo = HALF4( 1.0f, 1.0f, 1.0f, 1.0f );
	HALF alpha = 1.0h;
	albedo *= baseColor;
	if (
		#if ( DETAIL_BLEND_MODE == TCOMBINE_MASK_BASE_BY_DETAIL_ALPHA )
		( !bDetailTexture ) &&	// In this mode we must latch alpha post detail lerp blend with base texture (see "alpha *= albedo.a" below)
		#endif
		( !bBaseAlphaEnvmapMask && !bSelfIllum )
		)
	{
		alpha *= baseColor.a;
	}

	float detailBlendFactor = 0.0f;
	if ( bDetailTexture )
	{
		if ( bDetailTexture2 )
		{
			// combine detail maps and blend factors
			detailColor = lerp( detailColor, detailColor2, blendfactor );
			detailBlendFactor = lerp( g_DetailBlendFactor, g_Detail2BlendFactor, blendfactor );
		}
		else
		{
			detailBlendFactor = g_DetailBlendFactor;
		}

		albedo = TextureCombine( albedo, detailColor, DETAIL_BLEND_MODE, detailBlendFactor );
		#if ( DETAIL_BLEND_MODE == TCOMBINE_MASK_BASE_BY_DETAIL_ALPHA )
		alpha *= albedo.a; // In this mode we latch alpha post detail lerp now, #if above ensures that we don't pre-multiply by baseColor.a earlier
		#endif
		#if ( ( DETAIL_BLEND_MODE == TCOMBINE_MOD2X_SELECT_TWO_PATTERNS ) && !BASETEXTURE2 && !SELFILLUM )
		{
			// don't do this in the SELFILLUM case since we don't have enough instructions in ps20
			specularFactor *= 2.0h * lerp( detailColor.g, detailColor.b, baseColor.a );
		}
		#endif
	}

	// The vertex color contains the modulation color + vertex color combined
	#if ( SEAMLESS == 0 )
		albedo.rgb *= i.vertexColor.rgb;
	#endif

	// MAINTOL4DMERGEFIXME
	//alpha *= i.vertexColor.a * g_flAlpha2; // not sure about this one
	alpha *= i.vertexColor.a; // not sure about this one

	float flShadowScalar = 0.0;
	float flShadow = 1.0;

	// Save this off for single-pass flashlight, since we'll still need the SSBump vector, not a real normal
	HALF3 vSSBumpVector = vNormal.xyz;

	HALF3 diffuseLighting;
	if ( bBumpmap && bDiffuseBumpmap )
	{
		// ssbump
		#if ( BUMPMAP == 2 )
			#if ( DETAIL_BLEND_MODE == TCOMBINE_SSBUMP_BUMP )
				vNormal.xyz *= lerp( HALF3( 1, 1, 1 ), 2 * detailColor.xyz, alpha );
				vSSBumpVector = vNormal.xyz;
				alpha = 1;
			#endif
			diffuseLighting = vNormal.x * lightmapColor1.rgb +
							  vNormal.y * lightmapColor2.rgb +
							  vNormal.z * lightmapColor3.rgb;

			#if ( ( CSM_BLENDING == 1 ) && ( CASCADED_SHADOW_MAPPING ) && ( CASCADE_SIZE > 0 ) )
				diffuseLighting = BlendBumpDiffuseLightmapWithCSM( diffuseLighting, lightmapColor1.a, lightmapColor2.a, lightmapColor3.a, vNormal.xyz, worldPos, flShadow, flShadowScalar );
			#endif

			// SSBump textures are created assuming the shader decodes lighting for each basis vector by taking dot( N, basis )*lightmap.
			// But the lightmaps are created assuming that the 3 coeffs sum to 1.0 and are more like barycentric coords than visibility
			// along the basis vector...so the lightmap math is really just a weighted average of the 3 directional light maps.  So a flat
			// normal should have 3 weights each = 0.333.  But since ssbump textures are created assuming the other math, a flat normal
			// converted into an ssbump texture generates 3 weights each = 0.578, so instead of all 3 weights summing to 1.0, they sum
			// to 1.733.  To adjust for this, I'm scaling these coefficients by 1 / 1.733 = 0.578. NOTE: I'm not scaling vNormal directly
			// since it is used elsewhere for flashlight computations and shouldn't be scaled for that code.
			diffuseLighting *= 0.57735025882720947h;

			diffuseLighting *= (HALF3)g_TintValuesTimesLightmapScale.rgb;
			// now, calculate vNormal for reflection purposes. if vNormal isn't needed, hopefully
			// the compiler will eliminate these calculations
			vNormal.xyz = normalize( bumpBasis[0]*vNormal.x + bumpBasis[1]*vNormal.y + bumpBasis[2]*vNormal.z);
		#else

			HALF3 dp;
			dp.x = saturate( dot( vNormal.xyz, bumpBasis[0] ) );
			dp.y = saturate( dot( vNormal.xyz, bumpBasis[1] ) );
			dp.z = saturate( dot( vNormal.xyz, bumpBasis[2] ) );
			dp *= dp;

			#if ( DETAIL_BLEND_MODE == TCOMBINE_SSBUMP_BUMP )
				dp *= 2*detailColor.rgb;
			#endif

			diffuseLighting = dp.x * lightmapColor1.rgb +
							  dp.y * lightmapColor2.rgb +
							  dp.z * lightmapColor3.rgb;
			HALF sum = dot( dp, HALF3( 1.0f, 1.0f, 1.0f ) );

			#if ( ( CSM_BLENDING == 1 ) && ( CASCADED_SHADOW_MAPPING ) && ( CASCADE_SIZE > 0 ) )
				diffuseLighting = BlendBumpDiffuseLightmapWithCSM( diffuseLighting.rgb, lightmapColor1.a, lightmapColor2.a, lightmapColor3.a, dp, worldPos, flShadow, flShadowScalar );
			#endif

			diffuseLighting *= (HALF3)g_TintValuesTimesLightmapScale.rgb / sum;

		#endif
	}
	else
	{
		diffuseLighting = lightmapColor1.rgb;

		#if ( ( CSM_BLENDING == 1 ) && ( CASCADED_SHADOW_MAPPING ) && ( CASCADE_SIZE > 0 ) )
			diffuseLighting = BlendDiffuseLightmapWithCSM( diffuseLighting, lightmapColor1.a, worldPos, flShadow, flShadowScalar );
		#endif

		diffuseLighting.rgb *= g_TintValuesTimesLightmapScale.rgb;
	}

	// OLD CSM BLENDING - see above for fixed/improved. This version supports older vrad baked lightmap alpha
	// also a catch path for ssbump
	#if ( ( ( CSM_BLENDING == 0 ) ) && ( CASCADED_SHADOW_MAPPING ) && ( CASCADE_SIZE > 0 ) )
	{
#if !defined( _X360 ) && !defined( _PS3 ) && !defined( SHADER_MODEL_PS_2_B )
		if ( g_bCSMEnabled )
		{
#endif						
		// Can't enable dynamic jumps around the Fetch4 shader, because it can't use tex2dlod()
#if ( CSM_MODE != CSM_MODE_ATI_FETCH4 ) && !defined( SHADER_MODEL_PS_2_B )
			[branch]
#endif		
			if ( lightmapColor1.a > 0.0f )
			{
				float flSunPercent;
				if ( bBumpmap && bDiffuseBumpmap )
				{
					flSunPercent = lightmapColor1.a / ( Luminance( lightmapColor1.rgb + lightmapColor2.rgb + lightmapColor3.rgb ) * 0.3333 );
				}
				else
				{
					flSunPercent = lightmapColor1.a / Luminance( lightmapColor1.rgb );
				}

				flShadow = CSMComputeShadowing( worldPos );
					
				flShadowScalar = 1.0 - ( flSunPercent * ( 1.0 - flShadow ) );
				
				/* Debug - blink full shadows
				if ( step( frac( g_flTime * 0.5 ), 0.5 ) )
				{
					flShadowScalar = 1.0 - lightmapColor1.a;
				}
				//*/

				// Apply csm shadows
				diffuseLighting.rgb *= flShadowScalar;

				// Desaturate shadow color since we only have a grayscale dim factor
				diffuseLighting.rgb = lerp( diffuseLighting.bgr, diffuseLighting.rgb, flShadowScalar * 0.5 + 0.5 );

	// debug visualization			
	//			diffuseLighting.rgb = lerp( float3(1.0f-flShadowScalar,1.0f-flShadowScalar,1.0f-flShadowScalar), CSMVisualizeSplit( worldPos ), .3f );
	//			return float4(diffuseLighting.rgb, 1.0f);
			}			
#if !defined( _X360 ) && !defined( _PS3 ) && !defined( SHADER_MODEL_PS_2_B )
		}
#endif

	}
	#endif

	HALF3 worldSpaceNormal = mul( vNormal.xyz, tangenttranspose );
	#if !defined( SHADER_MODEL_PS_2_0 ) && ( FLASHLIGHT == 0 )
		diffuseLighting += (HALF3)g_cAmbientColor;
	#endif

	HALF3 diffuseComponent = albedo.rgb * diffuseLighting;

	#if ( defined( _X360 ) || defined( _PS3 ) ) && FLASHLIGHT
		// ssbump doesn't pass a normal to the flashlight...it computes shadowing a different way
		#if ( BUMPMAP == 2 )
			bool bHasNormal = false;

			float3 worldPosToLightVector = g_FlashlightPos - worldPos;

			HALF3 tangentPosToLightVector;
			tangentPosToLightVector.x = dot( worldPosToLightVector, tangenttranspose[0] );
			tangentPosToLightVector.y = dot( worldPosToLightVector, tangenttranspose[1] );
			tangentPosToLightVector.z = dot( worldPosToLightVector, tangenttranspose[2] );

			tangentPosToLightVector = normalize( tangentPosToLightVector );
			HALF nDotL = saturate( vSSBumpVector.x*dot( tangentPosToLightVector, bumpBasis[0]) +
									vSSBumpVector.y*dot( tangentPosToLightVector, bumpBasis[1]) +
									vSSBumpVector.z*dot( tangentPosToLightVector, bumpBasis[2]) );
		#else
			bool bHasNormal = true;
			HALF nDotL = 1.0h;
		#endif

		bool bShadows = FLASHLIGHTSHADOWS ? true : false;
	
		HALF3 flashlightColor = DoFlashlight( g_FlashlightPos, worldPos, i.flashlightSpacePos,
			worldSpaceNormal, g_FlashlightAttenuationFactors.xyz, 
			g_FlashlightAttenuationFactors.w, FlashlightSampler, ShadowDepthSampler,
			RandRotSampler, 0, bShadows, i.vProjPos.xy / i.vProjPos.w, false, g_ShadowTweaks, bHasNormal );

		diffuseComponent = albedo.xyz * ( diffuseLighting + ( flashlightColor * nDotL * (HALF3)g_TintValuesWithoutLightmapScale.rgb ) );
	#endif

	if ( bSelfIllum )
	{
		HALF3 selfIllumComponent = (HALF3)g_SelfIllumTint.xyz * albedo.xyz;
		diffuseComponent = lerp( diffuseComponent, selfIllumComponent, baseColor.a );
	}

	HALF3 specularLighting = HALF3( 0.0f, 0.0f, 0.0f );
	#if ( CUBEMAP )
	{
		float3 reflectVect = CalcReflectionVectorUnnormalized( worldSpaceNormal, worldVertToEyeVector );

		// Calc Fresnel factor
		HALF3 eyeVect = normalize(worldVertToEyeVector);
		HALF fresnel = 1.0h - dot( worldSpaceNormal, eyeVect );

		#if ( ENVMAPANISOTROPY ) // For anisotropic reflections on macroscopically rough sufaces like asphalt
			// Orthogonalize the view vector to the  surface normal, and use it as the anisotropy direction
			reflectVect = normalize( reflectVect );
			float3 rvec = cross( -eyeVect.xyz, worldSpaceNormal.xyz );
			float3 tang = cross( rvec, worldSpaceNormal.xyz );
				   rvec = cross( tang, reflectVect );
			float3 reflectVectAniso = normalize( cross( rvec, worldSpaceNormal.xyz ) );
			// Anisotropy amount is influenced by the view angle to the surface.  The more oblique the angle the more anisotropic the surface appears.
			anisotropyFactor *= dot( reflectVectAniso, -eyeVect );
			anisotropyFactor *= anisotropyFactor;
			reflectVect = normalize( lerp( reflectVect, reflectVectAniso, anisotropyFactor ) );
		#endif

		fresnel = max( 0, fresnel ); // precision issues on RSX cause this value to occasionally go negative, which results in a NaN presumably because of the exp(log(n)) operation
		fresnel = pow( fresnel, 4.0h ); //changing this to 4th power to save 2 cycles - visually it's very similar

		fresnel = fresnel * (HALF)g_OneMinusFresnelReflection + (HALF)g_FresnelReflection;

		specularLighting = (HALF)ENV_MAP_SCALE * h3texCUBE( EnvmapSampler, reflectVect ).rgb;

		#if (CUBEMAP == 2) //cubemap darkened by lightmap mode
			float3 cubemapLight = saturate( ( diffuseLighting - g_fvDiffuseCubemapMin ) * g_fvDiffuseCubemapMax );
			specularLighting = lerp( specularLighting, specularLighting * cubemapLight, (HALF)g_DiffuseCubemapScale ); //reduce the cubemap contribution when the pixel is in shadow
		#endif

		specularLighting *= specularFactor;
		specularLighting *= (HALF3)g_EnvmapTint.rgb;

		#if FANCY_BLENDING == 0
			HALF3 specularLightingSquared = specularLighting * specularLighting;
			specularLighting = lerp( specularLighting, specularLightingSquared, (HALF)g_EnvmapContrast );
			HALF3 greyScale = dot( specularLighting, HALF3( 0.299f, 0.587f, 0.114f ) );
			specularLighting = lerp( greyScale, specularLighting, (HALF)g_EnvmapSaturation );
		#endif
		specularLighting *= fresnel;
	}
	#endif

	if ( bDetailTexture )
	{
		diffuseComponent = TextureCombinePostLighting( diffuseComponent, detailColor, DETAIL_BLEND_MODE, detailBlendFactor );
	}

	// PHONG 
#if ( PHONG ) && ( CASCADED_SHADOW_MAPPING )
	[branch]
	if ( flShadowScalar > 0.0f )
	{
		float3 phongLighting = float3( 0.0f, 0.0f, 0.0f );
	
		float3 eyeVect = normalize( worldVertToEyeVector );
		float3 vLightDir = normalize( g_vCSMLightDir );

		float3 vHalfAngle = normalize( eyeVect.xyz + vLightDir.xyz);
		// need normalized worldspacenormal here else NDotH raised to phongExp blows up
		worldSpaceNormal = normalize(worldSpaceNormal);

		float NDotH = saturate( dot( worldSpaceNormal.xyz, vHalfAngle.xyz ) );

		float2 phongMask = float2( 0.0f, 0.0f );
		float2 phongTerm = float2( 0.0f, 0.0f ); 

		// greyscale phong mask
		phongMask.x = dot( baseColor1.rgb, float3(0.299f, 0.587f, 0.114f) );
		// phong lighting
		phongTerm.x = pow( NDotH, g_PhongExp ); // Raise to specular exponent

		if ( bBaseTexture2 )
		{
			phongMask.y = dot( baseColor2.rgb, float3(0.299f, 0.587f, 0.114f) );
			phongTerm.y = pow( NDotH, g_PhongExp2 ); 
		}

		// phong mask contrast, brightness
		phongMask.xy = saturate( ( (phongMask.xy - 0.5f) * g_PhongMask_Contrast_and_Brightness.xz ) + 0.5f + g_PhongMask_Contrast_and_Brightness.yw );

		// * mask
		phongTerm.xy *= phongMask.xy;

		// phong lighting material1
		phongLighting = phongTerm.xxx * g_PhongAmount.rgb * lerp( float3(1.0f, 1.0f, 1.0f), baseColor1.rgb, g_PhongTint ); // * amount * tint

		// material2
		if ( bBaseTexture2 )
		{
			float3 phongLighting2 = phongTerm.yyy * g_PhongAmount2.rgb * lerp( float3(1.0f, 1.0f, 1.0f), baseColor2.rgb, g_PhongTint2 ); // term * mask * amount * tint

			// blend
			phongLighting = lerp( phongLighting, phongLighting2, blendfactor );
		}
 
		#if ( CSM_BLENDING == 1 )
			// mask with N.L * ao * baked shadow * dynamic shadow
			phongLighting *= flShadow * flShadowScalar * specularFactor;
		#else
			phongLighting *= pow( saturate( dot( worldSpaceNormal, vLightDir ) ), 0.5f ); // Mask with N.L raised to a power
			phongLighting *= flShadow * diffuseLighting * specularFactor;	// modulate with csm shadow, diffuse lighting, spec(env map) mask if present
		#endif


		specularLighting += phongLighting;

#if ( PHONG_DEBUG == 1 )
		// debug phong mask
		diffuseComponent = 0.0f;
		specularLighting = lerp( phongMask.xxx, 0.0f, blendfactor );
		specularLighting += lerp( 0.0f, phongMask.yyy, blendfactor );
#endif
	}

	// use .a channel of phongAmount as a modulator for the diffuse component (useful for debugging, or approximating energy conservation
	if ( bBaseTexture2 )
	{
		diffuseComponent *= lerp( g_PhongAmount.a, g_PhongAmount2.a, blendfactor );
	}
	else
	{
		diffuseComponent *= g_PhongAmount.a;
	}

	#endif

	HALF3 result = diffuseComponent + specularLighting;

	#if ( LIGHTING_PREVIEW == 3 )
	{
		return float4( worldSpaceNormal, i.worldPos_projPosZ.w );
	}
	#endif

	#if ( LIGHTING_PREVIEW == 1 )
	{
		float dotprod = 0.2 + abs( dot( normalize(worldSpaceNormal), normalize(worldVertToEyeVector) ) );
		return FinalOutput( float4( dotprod*albedo.xyz*(g_TintValuesTimesLightmapScale.rgb/g_TintValuesTimesLightmapScale.w), alpha ), 0, PIXEL_FOG_TYPE_NONE, TONEMAP_SCALE_NONE );
	}
	#endif

	#if ( LIGHTING_PREVIEW == 2 )
	{
		LPREVIEW_PS_OUT ret;
		ret.color = float4( albedo.xyz,alpha );
		ret.normal = float4( worldSpaceNormal, i.worldPos_projPosZ.w );
		ret.position = float4( worldPos, alpha );
		ret.flags = float4( 1, 1, 1, alpha );
		return FinalOutput( ret, 0, PIXEL_FOG_TYPE_NONE, TONEMAP_SCALE_NONE );	
	}
	#endif

	#if ( LIGHTING_PREVIEW == 0 )
	{
		bool bWriteDepthToAlpha = false;

		// ps_2_b and beyond
		#if !(defined(SHADER_MODEL_PS_1_1) || defined(SHADER_MODEL_PS_1_4) || defined(SHADER_MODEL_PS_2_0))
			bWriteDepthToAlpha = ( WRITE_DEPTH_TO_DESTALPHA != 0 ) && ( WRITEWATERFOGTODESTALPHA == 0 );
		#endif

		HALF flVertexFogFactor = 0.0h;
		// FIXME: Reintroduce support for vertex fog
		//#if !HARDWAREFOGBLEND && !DOPIXELFOG
		//{
		//	#if ( SEAMLESS )
		//	{
		//		flVertexFogFactor = i.SeamlessTexCoord_fogFactorW.w;
		//	}
		//	#else
		//	{
		//		flVertexFogFactor = i.baseTexCoord_fogFactorZ.z;
		//	}
		//	#endif
		//}
		//#endif

		HALF fogFactor = CalcPixelFogFactor( PIXELFOGTYPE, g_FogParams, g_EyePos.xyz, worldPos, i.worldPos_projPosZ.w );
		//HALF fogFactor = CalcPixelFogFactorSupportsVertexFog( PIXELFOGTYPE, g_FogParams, g_EyePos.xyz, worldPos, i.worldPos_projPosZ.w, flVertexFogFactor );
		#if WRITEWATERFOGTODESTALPHA && (PIXELFOGTYPE == PIXEL_FOG_TYPE_HEIGHT)
			alpha = fogFactor;
		#endif

		float4_color_return_type vOutput = FinalOutputHalf( HALF4( result.rgb, alpha ), fogFactor, PIXELFOGTYPE, TONEMAP_SCALE_LINEAR, bWriteDepthToAlpha, i.worldPos_projPosZ.w );

		#if ( defined( _X360 ) )
		{
			vOutput.xyz += ScreenSpaceOrderedDither( i.vScreenPos );
		}
		#endif

		return vOutput;
	}
	#endif
}
