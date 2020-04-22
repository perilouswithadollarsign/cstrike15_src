//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================

void GetBaseTextureAndNormal( sampler base, sampler base2, sampler base3, sampler base4, sampler bump, bool bBump,
							  float3 coords, float2 bumpcoords, float4 uvScales23, float2 uvScales4, HALF3 vWeights,
							 out HALF4 vResultBase, out HALF4 vResultBase2, out HALF4 vResultBase3, out HALF4 vResultBase4, out HALF4 vResultBump )
{
	vResultBase = 0;
	vResultBase2 = 0;
	vResultBase3 = 0;
	vResultBase4 = 0;
	vResultBump = 0;

	if ( !bBump )
	{
		vResultBump = HALF4(0, 0, 1, 1);
	}

#if SEAMLESS

	vResultBase  += vWeights.x * h4tex2D( base, coords.zy );
	vResultBase2 += vWeights.x * h4tex2D( base2, coords.zy * uvScales23.xy );
	vResultBase3 += vWeights.x * h4tex2D( base3, coords.zy * uvScales23.zw );
	vResultBase4 += vWeights.x * h4tex2D( base4, coords.zy * uvScales4.xy  );
	if ( bBump )
	{
		vResultBump  += vWeights.x * h4tex2D( bump, coords.zy );
	}

	vResultBase  += vWeights.y * h4tex2D( base, coords.xz );
	vResultBase2 += vWeights.y * h4tex2D( base2, coords.xz * uvScales23.xy );
	vResultBase3 += vWeights.y * h4tex2D( base3, coords.xz * uvScales23.zw );
	vResultBase4 += vWeights.y * h4tex2D( base4, coords.xz * uvScales4.xy  );
	if ( bBump )
	{
		vResultBump  += vWeights.y * h4tex2D( bump, coords.xz );
	}

	vResultBase  += vWeights.z * h4tex2D( base, coords.xy );
	vResultBase2 += vWeights.z * h4tex2D( base2, coords.xy * uvScales23.xy );
	vResultBase3 += vWeights.z * h4tex2D( base3, coords.xy * uvScales23.zw );
	vResultBase4 += vWeights.z * h4tex2D( base4, coords.xy * uvScales4.xy  );
	if ( bBump )
	{
		vResultBump  += vWeights.z * h4tex2D( bump, coords.xy );
	}

#else  // not seamless

	vResultBase  = h4tex2D( base, coords.xy );
	vResultBase2 = h4tex2D( base2, coords.xy * uvScales23.xy );
	vResultBase3 = h4tex2D( base3, coords.xy * uvScales23.zw  );
	vResultBase4 = h4tex2D( base4, coords.xy * uvScales4.xy  );
	if ( bBump )
	{
		vResultBump  = h4tex2D( bump, bumpcoords.xy );
	}

#endif
}


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
		float4 projPos					: POSITION;	
		#if !defined( _X360 ) && !defined( SHADER_MODEL_VS_3_0 )
			float  fog						: FOG;
		#endif
	#endif

	#if SEAMLESS
		#if HARDWAREFOGBLEND || DOPIXELFOG
			float3 SeamlessTexCoord_fogFactorW         : TEXCOORD0;            // zy xz
		#else
			float4 SeamlessTexCoord_fogFactorW         : TEXCOORD0;            // zy xz
		#endif
	#else
		#if HARDWAREFOGBLEND || DOPIXELFOG
			float2 baseTexCoord_fogFactorZ				: TEXCOORD0;
		#else
			float3 baseTexCoord_fogFactorZ				: TEXCOORD0;
		#endif
	#endif

	// Detail textures and bumpmaps are mutually exclusive so that we have enough texcoords.
	float4 detailOrBumpAndEnvmapMaskTexCoord : TEXCOORD1;   // envmap mask
	float4 lightmapTexCoord1And2	: TEXCOORD2_centroid;
	float4 lightmapTexCoord3		: TEXCOORD3_centroid;
	float4 worldPos_projPosZ		: TEXCOORD4;
	float3x3 tangentSpaceTranspose	: TEXCOORD5;
	// tangentSpaceTranspose		: TEXCOORD6
	// tangentSpaceTranspose		: TEXCOORD7
	float4 vertexColor				: COLOR0;
	float4 vertexBlend				: COLOR1;

	// Extra iterators on 360, used in flashlight combo
	#if ( defined( _X360 ) || defined( _PS3 ) ) && FLASHLIGHT
		float4 flashlightSpacePos		: TEXCOORD8;
		float4 vProjPos					: TEXCOORD9;
	#endif
	
	#if defined( PIXELSHADER ) && defined( _X360 )
		 float2 vScreenPos : VPOS;
	#endif
};

#define DETAILCOORDS detailOrBumpAndEnvmapMaskTexCoord.xy
#define DETAILORBUMPCOORDS DETAILCOORDS
#define ENVMAPMASKCOORDS detailOrBumpAndEnvmapMaskTexCoord.wz

#if DETAILTEXTURE && BUMPMAP && !SELFILLUM
	#define BUMPCOORDS lightmapTexCoord3.wz
#elif DETAILTEXTURE
	#define BUMPCOORDS baseTexCoord_fogFactorZ.xy
#else
	// This is the SEAMLESS case too since we skip SEAMLESS && DETAILTEXTURE
	#define BUMPCOORDS detailOrBumpAndEnvmapMaskTexCoord.xy
#endif

#if SEAMLESS
	// don't use BASETEXCOORD in the SEAMLESS case
#else
	#define BASETEXCOORD baseTexCoord_fogFactorZ.xy
#endif
