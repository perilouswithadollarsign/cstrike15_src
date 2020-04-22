//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================

#if defined( _X360 )

void GetBaseTextureAndNormal( sampler base, sampler base2, sampler bump, bool bBase2, bool bBump, 
							  float3 coords, float2 bumpcoords,
							  float3 vWeights,
							  out float4 vResultBase, out float4 vResultBase2, out float4 vResultBump )
{
	vResultBase = 0;
	vResultBase2 = 0;
	vResultBump = 0;

	if ( !bBump )
	{
		vResultBump = float4(0, 0, 1, 1);
	}

#if SEAMLESS

	vWeights = max( vWeights - 0.3, 0 );
	vWeights *= 1.0f / dot( vWeights, float3(1,1,1) );

	[branch]
	if (vWeights.x > 0)
	{
		vResultBase  += vWeights.x * tex2D( base,  coords.zy );

		if ( bBase2 )
		{
			vResultBase2 += vWeights.x * tex2D( base2, coords.zy );
		}

		if ( bBump )
		{
			vResultBump  += vWeights.x * tex2D( bump,  coords.zy );
		}
	}

	[branch]
	if (vWeights.y > 0)
	{
		vResultBase  += vWeights.y * tex2D( base,  coords.xz );

		if ( bBase2 )
		{
			vResultBase2 += vWeights.y * tex2D( base2, coords.xz );
		}
		if ( bBump )
		{
			vResultBump  += vWeights.y * tex2D( bump,  coords.xz );
		}
	}

	[branch]
	if (vWeights.z > 0)
	{
		vResultBase  += vWeights.z * tex2D( base,  coords.xy );
		if ( bBase2 )
		{
			vResultBase2 += vWeights.z * tex2D( base2, coords.xy );
		}

		if ( bBump )
		{
			vResultBump  += vWeights.z * tex2D( bump,  coords.xy );
		}
	}

	#if ( SHADER_SRGB_READ == 1 )
		// Do this after the blending to save shader ops

		#if defined( CSTRIKE15 )
			// [mariod] - for CSTRIKE15 we are not using any PWL textures but gamma 2.2 reads of srgb textures
			vResultBase.rgb = GammaToLinear( vResultBase.rgb );
			vResultBase2.rgb = GammaToLinear( vResultBase2.rgb );
		#else
			vResultBase.rgb = X360GammaToLinear( vResultBase.rgb );
			vResultBase2.rgb = X360GammaToLinear( vResultBase2.rgb );
		#endif
	#endif

#else  // not seamless

	vResultBase = tex2Dsrgb( base, coords.xy );

	if ( bBase2 )
	{
		vResultBase2 = tex2Dsrgb( base2, coords.xy );
	}

	if ( bBump )
	{
		vResultBump  = tex2D( bump, bumpcoords.xy );
	}

#endif
}

#else // PC

void GetBaseTextureAndNormal( sampler base, sampler base2, sampler bump, bool bBase2, bool bBump,
							  float3 coords, float3 coords2, float2 bumpcoords, HALF3 vWeights,
							 out HALF4 vResultBase, out HALF4 vResultBase2, out HALF4 vResultBump )
{
	vResultBase = 0;
	vResultBase2 = 0;
	vResultBump = 0;

	if ( !bBump )
	{
		vResultBump = HALF4(0, 0, 1, 1);
	}

#if SEAMLESS

	vResultBase  += vWeights.x * h4tex2D( base, coords.zy );
	if ( bBase2 )
	{
		vResultBase2 += vWeights.x * h4tex2D( base2, coords.zy );
	}
	if ( bBump )
	{
		vResultBump  += vWeights.x * h4tex2D( bump, coords.zy );
	}

	vResultBase  += vWeights.y * h4tex2D( base, coords.xz );
	if ( bBase2 )
	{
		vResultBase2 += vWeights.y * h4tex2D( base2, coords.xz );
	}
	if ( bBump )
	{
		vResultBump  += vWeights.y * h4tex2D( bump, coords.xz );
	}

	vResultBase  += vWeights.z * h4tex2D( base, coords.xy );
	if ( bBase2 )
	{
		vResultBase2 += vWeights.z * h4tex2D( base2, coords.xy );
	}
	if ( bBump )
	{
		vResultBump  += vWeights.z * h4tex2D( bump, coords.xy );
	}

#else  // not seamless

	vResultBase  = h4tex2D( base, coords.xy );
	if ( bBase2 )
	{
		vResultBase2 = h4tex2D( base2, coords2.xy );
	}
	if ( bBump )
	{
		vResultBump  = h4tex2D( bump, bumpcoords.xy );
	}

#endif
}

#endif


HALF4 LightMapSample( sampler LightmapSampler, float2 vTexCoord )
{
	#if ( !defined( _X360 ) || !defined( USE_32BIT_LIGHTMAPS_ON_360 ) )
	{
		HALF4 sample = h4tex2D( LightmapSampler, vTexCoord );
		return sample;
	}
	#else
	{
		#if 0 //1 for cheap sampling, 0 for accurate scaling from the individual samples
		{
			float4 sample = tex2D( LightmapSampler, vTexCoord );

			return HALF4( sample.rgb * sample.a, 1.0 );
		}
		#else
		{
			float4 Weights;
			float4 samples_0; //no arrays allowed in inline assembly
			float4 samples_1;
			float4 samples_2;
			float4 samples_3;
			
			asm {
				tfetch2D samples_0, vTexCoord.xy, LightmapSampler, OffsetX = -0.5, OffsetY = -0.5, MinFilter=point, MagFilter=point, MipFilter=keep, UseComputedLOD=false
				tfetch2D samples_1, vTexCoord.xy, LightmapSampler, OffsetX =  0.5, OffsetY = -0.5, MinFilter=point, MagFilter=point, MipFilter=keep, UseComputedLOD=false
				tfetch2D samples_2, vTexCoord.xy, LightmapSampler, OffsetX = -0.5, OffsetY =  0.5, MinFilter=point, MagFilter=point, MipFilter=keep, UseComputedLOD=false
				tfetch2D samples_3, vTexCoord.xy, LightmapSampler, OffsetX =  0.5, OffsetY =  0.5, MinFilter=point, MagFilter=point, MipFilter=keep, UseComputedLOD=false

				getWeights2D Weights, vTexCoord.xy, LightmapSampler
			};

			Weights = float4( (1-Weights.x)*(1-Weights.y), Weights.x*(1-Weights.y), (1-Weights.x)*Weights.y, Weights.x*Weights.y );

			float3 result;
			result.rgb  = samples_0.rgb * (samples_0.a * Weights.x);
			result.rgb += samples_1.rgb * (samples_1.a * Weights.y);
			result.rgb += samples_2.rgb * (samples_2.a * Weights.z);
			result.rgb += samples_3.rgb * (samples_3.a * Weights.w);
		
			return float4( result, 1.0 );
		}
		#endif
	}
	#endif
}

#ifdef PIXELSHADER
	#define VS_OUTPUT PS_INPUT
#endif

struct VS_OUTPUT
{
#ifndef PIXELSHADER
	float4 projPos : POSITION;
#if !defined( _X360 ) && !defined( SHADER_MODEL_VS_3_0 )
	float  fog : FOG;
#endif
#endif

#if SEAMLESS
	float3 SeamlessTexCoord							: TEXCOORD0;
#else
	float4 baseTexCoord_blendmodulateTexCoord		: TEXCOORD0;
#endif

	float4 detailTexCoord_EnvmapMaskTexCoord		: TEXCOORD1;
	float4 lightmapTexCoord1And2					: TEXCOORD2_centroid;
	float4 lightmapTexCoord3_bumpTexCoord			: TEXCOORD3_centroid;
	float4 worldPos_projPosZ						: TEXCOORD4;

	float4 tangentSpaceTranspose0_vertexBlendX		: TEXCOORD5;
	float4 tangentSpaceTranspose1_bumpTexCoord2u	: TEXCOORD6;
	float4 tangentSpaceTranspose2_bumpTexCoord2v	: TEXCOORD7;

#if defined ( SHADER_MODEL_VS_3_0 ) || defined ( SHADER_MODEL_PS_3_0 )
	float4 baseTexCoord2_detailTexCoord2			: TEXCOORD8;
#endif

	float4 vertexColor								: COLOR0;

	// Extra iterators on 360, used in flashlight combo
#if ( defined( _X360 ) || defined( _PS3 ) ) && FLASHLIGHT
	float4 flashlightSpacePos : TEXCOORD8;
	float4 vProjPos : TEXCOORD9;
#endif

#if defined( PIXELSHADER ) && defined( _X360 )
	float2 vScreenPos : VPOS;
#endif
};

// base
#if SEAMLESS
// don't use BASETEXCOORD in the SEAMLESS case
#else
#define BASETEXCOORD baseTexCoord_blendmodulateTexCoord.xy

#if defined ( SHADER_MODEL_VS_3_0 ) || defined ( SHADER_MODEL_PS_3_0 )
#define BASETEXCOORD2 baseTexCoord2_detailTexCoord2.xy
#else
#define BASETEXCOORD2 baseTexCoord_blendmodulateTexCoord.xy
#endif
#endif

// detail
#define DETAILCOORD detailTexCoord_EnvmapMaskTexCoord.xy
#if defined ( SHADER_MODEL_VS_3_0 ) || defined ( SHADER_MODEL_PS_3_0 )
#define DETAILCOORD2 baseTexCoord2_detailTexCoord2.zw
#endif

// bump
#define BUMPCOORD lightmapTexCoord3_bumpTexCoord.zw
#define BUMPCOORD2U tangentSpaceTranspose1_bumpTexCoord2u.w
#define BUMPCOORD2V tangentSpaceTranspose2_bumpTexCoord2v.w

#define ENVMAPMASKCOORD detailTexCoord_EnvmapMaskTexCoord.zw

#if !SEAMLESS
#define BLENDMODULATECOORD baseTexCoord_blendmodulateTexCoord.zw
#endif

