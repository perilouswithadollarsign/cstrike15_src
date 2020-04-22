//========== Copyright (c) Valve Corporation, All rights reserved. ==========//

// Use samplers 9+

// Use constanst registers 29+

#ifndef USEPATTERN
	#define USEPATTERN 0
#endif

#ifndef GENERATEBASETEXTURE
	#define GENERATEBASETEXTURE 0
#endif

#ifndef GENERATENORMAL
	#define GENERATENORMAL 0
#endif

#ifndef GENERATEMASKS1
	#define GENERATEMASKS1 0
#endif

#ifndef BASEALPHAPHONGMASK
	#define BASEALPHAPHONGMASK 0
#endif

#ifndef BASEALPHAENVMASK
	#define BASEALPHAENVMASK 0
#endif

#ifndef BUMPALPHAENVMASK
	#define BUMPALPHAENVMASK 0
#endif

#ifndef CHEAPFILTERING
	#define CHEAPFILTERING 0
#endif

#if ( GENERATENORMAL && ( ( BASEALPHAPHONGMASK == 1 ) && ( BUMPALPHAENVMASK == 1 ) || ( BASEALPHAPHONGMASK == 0 ) ) )
	#define NORMALALPHAUSED 1
#else
	#define NORMALALPHAUSED 0
#endif
	

sampler MaterialMaskSampler			: register( s9 );
sampler AOSampler					: register( s10 );
sampler3D GrungeSampler				: register( s11 );
sampler3D DetailSampler				: register( s12 );
sampler3D DetailNormalSampler		: register( s13 );

#if ( USEPATTERN > 0 )
	sampler PatternSampler			: register( s14 );
#endif

sampler NoiseSampler				: register( s15 );

const float4 g_vDetailScale							: register( c108 );
const float4 g_vDetailSpecBoost						: register( c32 );
const float4 g_vDamageDetailSpecBoost				: register( c100 );
const float4 g_vDetailEnvBoost						: register( c34 );
const float4 g_vDamageDetailEnvBoost				: register( c35 );
const float4 g_vDetailPhongAlbedoTint				: register( c36 );
const float4 g_vDetailWarpIndex						: register( c37 );
const float4 g_vDetailMetalness						: register( c38 );
const float4 g_vDetailNormalDepth					: register( c39 );
const float4 g_vDamageNormalEdgeDepth				: register( c40 );
const float4 g_vDamageLevels0_1						: register( c41 );
#define		 g_vDamageLevels0						g_vDamageLevels0_1.xy
#define		 g_vDamageLevels1						g_vDamageLevels0_1.zw
const float4 g_vDamageLevels2_3						: register( c42 );
#define		 g_vDamageLevels2						g_vDamageLevels2_3.xy
#define		 g_vDamageLevels3						g_vDamageLevels2_3.zw
const float4 g_vPatternTexcoordTransform[2]			: register( c43 );	// c43 & c44
const int4   g_vPatternColorIndices					: register( c45 );
const float4 g_vWearProgress_PatternParams			: register( c46 );
#define		 g_fWearProgress						g_vWearProgress_PatternParams.x
#define		 g_fPatternDetailInfluence				g_vWearProgress_PatternParams.y
#define		 g_nPatternReplaceIndex					int( g_vWearProgress_PatternParams.z )
#define		 g_fTexelSize							g_vWearProgress_PatternParams.w
const float4 g_paletteColor0_paletteColor3r			: register( c47 );
#define		 g_cPaletteColor0						g_paletteColor0_paletteColor3r.rgb
const float4 g_paletteColor1_paletteColor3g			: register( c48 );
#define		 g_cPaletteColor1						g_paletteColor1_paletteColor3g.rgb
const float4 g_paletteColor2_paletteColor3b			: register( c49 );
#define		 g_cPaletteColor2						g_paletteColor2_paletteColor3b.rgb
#define		 g_cPaletteColor3						float3( g_paletteColor0_paletteColor3r.w, g_paletteColor1_paletteColor3g.w, g_paletteColor2_paletteColor3b.w )
const float4 g_paletteColor4_paletteColor7r			: register( c50 );
#define		 g_cPaletteColor4						g_paletteColor4_paletteColor7r.rgb
const float4 g_paletteColor5_paletteColor7g			: register( c51 );
#define		 g_cPaletteColor5						g_paletteColor5_paletteColor7g.rgb
const float4 g_paletteColor6_paletteColor7b			: register( c52 );
#define		 g_cPaletteColor6						g_paletteColor6_paletteColor7b.rgb
#define		 g_cPaletteColor7						float3( g_paletteColor4_paletteColor7r.w, g_paletteColor5_paletteColor7g.w, g_paletteColor6_paletteColor7b.w )
const float4 g_vGrungeTexcoordTransform[2]			: register( c53 );	// c53 & c54
const float4 g_vWearBleaching						: register( c55 );
const float4 g_vWearDetailPhongBoost				: register( c56 );
const float4 g_vWearDetailEnvBoost					: register( c57 );
const float4 g_vDamageEdgePhongBoost				: register( c58 );
const float4 g_vDamageEdgeEnvBoost					: register( c59 );
const float4 g_vCurvatureWearBoost					: register( c60 );
const float4 g_vCurvatureWearPower					: register( c61 );
const float4 g_vDamageDetailSaturation				: register( c62 );
const float4 g_vDamageBleaching						: register( c63 );
const float4 g_vDetailGrungeFactor					: register( c90 ); //64
const float4 g_vPatternParams						: register( c91 ); //65
#define		 g_fPatternPhongFactor					g_vPatternParams.x
#define		 g_fPatternPaintThickness				g_vPatternParams.y
#define		 g_fFlipFixup							g_vPatternParams.z
//#define	 UNUSED									g_vPatternParams.zw
const float4 g_vDamageBrightness					: register( c92 ); //66
const float4 g_vGrungeMax							: register( c93 ); //67
const float4 g_vGrimeSaturation						: register( c94 ); //70
const float4 g_vGrimeBrightness						: register( c95 ); //72
const float4 g_vGrungeTexcoordRotation[2]			: register( c96 );
const float4 g_vPatternTexcoordRotation[2]			: register( c98 );

#define g_lum float3( 0.299, 0.587, 0.114 )
#define g_unitVec float3( 0.577f, 0.577f, 0.577f )
// todo: convert to param

#define g_sampleSliceZ float4( 0.0f, 0.375f, 0.625f, 1.0f )

float3 colorize( float3 cVal, float3 cColor, float fBrightness, float fSaturation )
{
	if ( fSaturation > 0 )
	{
		float fLocalSaturation = fSaturation;
		float3 cRetVal = normalize(cColor * dot(cColor, g_lum));

		fLocalSaturation *= pow(abs(dot(cRetVal - g_unitVec, g_unitVec)), 0.2f);
		fLocalSaturation = saturate(fLocalSaturation);

		float3 cDiff = normalize(cRetVal - float3(0.577f, 0.577f, 0.577f));
		cRetVal = saturate(cDiff * 2.0f + 1.0f);

		float3 temp = cRetVal * g_lum;
		float fSaturatedBrightness = fBrightness / (temp.r + temp.g + temp.b);

		cRetVal = lerp(cVal, cRetVal * fSaturatedBrightness, fLocalSaturation);

		return cRetVal;
	}
	else
	{
		return lerp( cVal, dot( cVal, g_lum ), -fSaturation );
	}
}

float fourWayLerp( float4 fvVal, float3 fvMask )
{
   float fVal = lerp( fvVal.x, fvVal.y, fvMask.r );
   fVal = lerp( fVal, fvVal.z, fvMask.g );
   fVal = lerp( fVal, fvVal.w, fvMask.b );
   return fVal;
}

float2 fourWayLerp( float2 fvVal[4], float3 fvMask )
{
   float2 fvRetVal = lerp( fvVal[0], fvVal[1], fvMask.r );
   fvRetVal = lerp( fvRetVal, fvVal[2], fvMask.g );
   fvRetVal = lerp( fvRetVal, fvVal[3], fvMask.b );
   return fvRetVal;
}

float3 fourWayLerp( float3 fvVal[4], float3 fvMask )
{
   float3 fvRetVal = lerp( fvVal[0], fvVal[1], fvMask.r );
   fvRetVal = lerp( fvRetVal, fvVal[2], fvMask.g );
   fvRetVal = lerp( fvRetVal, fvVal[3], fvMask.b );
   return fvRetVal;
}

float4 fourWayLerp( float4 fvVal[4], float3 fvMask )
{
   float4 fvRetVal = lerp( fvVal[0], fvVal[1], fvMask.r );
   fvRetVal = lerp( fvRetVal, fvVal[2], fvMask.g );
   fvRetVal = lerp( fvRetVal, fvVal[3], fvMask.b );
   return fvRetVal;
}

void customizeCharacter( const float2 vTexcoord, inout float4 vBaseTextureSample, inout float4 vNormalSample, inout float4 vMasks1Params )
{
//GENERATEBASETEXTURE
//GENERATENORMAL
//GENERATEMASKS1

	#if ( ( GENERATEBASETEXTURE == 0 ) && ( GENERATENORMAL == 0 ) && ( GENERATEMASKS1 == 0 ) )
		return;
	#endif

	float4 vMaterialMaskSample = tex2D( MaterialMaskSampler, vTexcoord );

	#if ( ( GENERATEBASETEXTURE == 1 ) || ( GENERATENORMAL == 1 ) )
		#if ( CHEAPFILTERING == 0 )
			float4 fvSampleOffset[9] = { float4( 0.0f, 0.0f, 0.319727891, 0.0f ),
										 float4( -1.0f, -1.0f, 0.051020408, 0.075f ),
										 float4( -1.0f, 0.0f, 0.119047619, 0.175f ),
										 float4( -1.0f, 1.0f, 0.051020408, 0.075f ),
										 float4( 0.0f, -1.0f, 0.119047619, 0.175f ),
										 float4( 0.0f, 1.0f, 0.119047619, 0.175f ),
										 float4( 1.0f, -1.0f, 0.051020408, 0.075f ),
										 float4( 1.0f, 0.0f, 0.119047619, 0.175f ),
										 float4( 1.0f, 1.0f, 0.051020408, 0.075f ) };
		#endif

		float4 vAOSample = tex2D( AOSampler, vTexcoord );
		float fAO = vAOSample.g;
		float fWear = vAOSample.b;
		float fCurvatureWearPower = fourWayLerp( g_vCurvatureWearPower, vMaterialMaskSample.rgb );
		float fCurvature = pow( vAOSample.r, fCurvatureWearPower );
		float fDurability = vAOSample.a;

		// Grunge

		float2 fvGrungeTexcoord = float2( dot( vTexcoord, g_vGrungeTexcoordTransform[0].xy ) + g_vGrungeTexcoordTransform[0].w,
										  dot( vTexcoord, g_vGrungeTexcoordTransform[1].xy ) + g_vGrungeTexcoordTransform[1].w );
   
		float4 vGrungeSamples[4] = { tex3D( GrungeSampler, float3( fvGrungeTexcoord, g_sampleSliceZ.x ) ),
									 tex3D( GrungeSampler, float3( fvGrungeTexcoord, g_sampleSliceZ.y ) ),
									 tex3D( GrungeSampler, float3( fvGrungeTexcoord, g_sampleSliceZ.z ) ), 
									 tex3D( GrungeSampler, float3( fvGrungeTexcoord, g_sampleSliceZ.w ) ) };
                                 
		float4 cGrunge = fourWayLerp( vGrungeSamples, vMaterialMaskSample.rgb );

		// Damage
		float2 fvDamageLevelArray[4] = { g_vDamageLevels0, g_vDamageLevels1, g_vDamageLevels2, g_vDamageLevels3 };
		float2 fvDamageLevels = fourWayLerp( fvDamageLevelArray, vMaterialMaskSample.rgb );
   
		float fWearInfluence = fCurvature * fDurability;

		// Detail
	
		float4 vDetailSamples[4] = { tex3D( DetailSampler, float3( vTexcoord * g_vDetailScale.x, g_sampleSliceZ.x ) ),
									 tex3D( DetailSampler, float3( vTexcoord * g_vDetailScale.y, g_sampleSliceZ.y ) ),
									 tex3D( DetailSampler, float3( vTexcoord * g_vDetailScale.z, g_sampleSliceZ.z ) ), 
									 tex3D( DetailSampler, float3( vTexcoord * g_vDetailScale.w, g_sampleSliceZ.w ) ) };

		float4 fvFinalDetails = fourWayLerp( vDetailSamples, vMaterialMaskSample.rgb );

		float fCurvatureWearBoost = fourWayLerp( g_vCurvatureWearBoost, vMaterialMaskSample.rgb );

		float fDamageAmount = fWearInfluence * cGrunge.a;
		fDamageAmount = saturate( fDamageAmount + fCurvatureWearBoost * fCurvature );
		float fWearAmount = fDamageAmount;
		fDamageAmount *= g_fWearProgress;
		fWearAmount *= g_fWearProgress * 4.0f;
		fDamageAmount = smoothstep( fvDamageLevels.x, fvDamageLevels.y, fDamageAmount );
		// wear is a halo around the damage
		fWearAmount = smoothstep( fvDamageLevels.x, fvDamageLevels.y, fWearAmount );
		fWearAmount = saturate( fWearAmount ) * ( 1.0f - fDamageAmount ) * fvFinalDetails.b;
		float fDamageAndWearAmount = saturate( fDamageAmount + fWearAmount );

		float fDetail = lerp( fvFinalDetails.g, fvFinalDetails.r, fDamageAndWearAmount );
	#endif

	#if ( ( GENERATEBASETEXTURE == 1 ) || ( GENERATENORMAL == 1 ) )

		float4 cPattern = float4( 0.0f, 0.0f, 0.0f, 1.0f );
		float4 cSubColor = float4( 0.0f, 0.0f, 0.0f, 1.0f );
		float3 cvPaletteColorsOrig[8] = { g_cPaletteColor0, g_cPaletteColor1, g_cPaletteColor2, g_cPaletteColor3, g_cPaletteColor4, g_cPaletteColor5, g_cPaletteColor6, g_cPaletteColor7 };

		#if ( USEPATTERN > 0 )
			float2 vPatternTexcoord = float2( dot( vTexcoord, g_vPatternTexcoordTransform[0].xy ) + g_vPatternTexcoordTransform[0].w,
											  dot( vTexcoord, g_vPatternTexcoordTransform[1].xy ) + g_vPatternTexcoordTransform[1].w );
			vPatternTexcoord += ( fDetail * 2.0f - 1.0f ) * g_fTexelSize * g_fPatternDetailInfluence;
			vPatternTexcoord.x *= ( vTexcoord.x < 0 ) ? -1.0 : 1.0;
			float4 vPatternSample = tex2D( PatternSampler, vPatternTexcoord );

			#if ( ( USEPATTERN == 1 ) || ( USEPATTERN == 3 ) )
				float3 cvPatternPalette[4] = { cvPaletteColorsOrig[ g_vPatternColorIndices[ 0 ] ], 
											   cvPaletteColorsOrig[ g_vPatternColorIndices[ 1 ] ], 
											   cvPaletteColorsOrig[ g_vPatternColorIndices[ 2 ] ], 
											   cvPaletteColorsOrig[ g_vPatternColorIndices[ 3 ] ] };

				cPattern.rgb = fourWayLerp( cvPatternPalette, vPatternSample.rgb );
			#else
				cPattern.rgb = vPatternSample.rgb;
			#endif
			cPattern.a = lerp( 1.0f, ( vPatternSample.a - 0.5f ) * 2.0f * g_fPatternPhongFactor + 1.0f, g_fPatternPhongFactor );
		#endif

		float4 cvPaletteColors[8];
	#endif
	
	#if ( GENERATEBASETEXTURE == 1 )

		cvPaletteColors[0] = float4( g_cPaletteColor0, 1.0f );
		cvPaletteColors[1] = float4( g_cPaletteColor1, 1.0f );
		cvPaletteColors[2] = float4( g_cPaletteColor2, 1.0f );
		cvPaletteColors[3] = float4( g_cPaletteColor3, 1.0f );
		cvPaletteColors[4] = float4( g_cPaletteColor4, 1.0f );
		cvPaletteColors[5] = float4( g_cPaletteColor5, 1.0f );
		cvPaletteColors[6] = float4( g_cPaletteColor6, 1.0f );
		cvPaletteColors[7] = float4( g_cPaletteColor7, 1.0f );
		#if ( USEPATTERN > 2 )
			cvPaletteColors[0] = lerp( cPattern, float4( g_cPaletteColor0, cPattern.a ), fDamageAmount );
			cvPaletteColors[1] = lerp( cPattern, float4( g_cPaletteColor1, cPattern.a ), fDamageAmount );
			cvPaletteColors[2] = lerp( cPattern, float4( g_cPaletteColor2, cPattern.a ), fDamageAmount );
			cvPaletteColors[3] = lerp( cPattern, float4( g_cPaletteColor3, cPattern.a ), fDamageAmount );
			cvPaletteColors[4] = lerp( cPattern, float4( g_cPaletteColor4, cPattern.a ), fDamageAmount );
			cvPaletteColors[5] = lerp( cPattern, float4( g_cPaletteColor5, cPattern.a ), fDamageAmount );
			cvPaletteColors[6] = lerp( cPattern, float4( g_cPaletteColor6, cPattern.a ), fDamageAmount );
			cvPaletteColors[7] = lerp( cPattern, float4( g_cPaletteColor7, cPattern.a ), fDamageAmount );
		#else
			cvPaletteColors[0] = lerp( cPattern, float4( g_cPaletteColor0, 1.0f ), saturate( fDamageAmount + ( g_nPatternReplaceIndex != 0 ) ) );
			cvPaletteColors[1] = lerp( cPattern, float4( g_cPaletteColor1, 1.0f ), saturate( fDamageAmount + ( g_nPatternReplaceIndex != 1 ) ) );
			cvPaletteColors[2] = lerp( cPattern, float4( g_cPaletteColor2, 1.0f ), saturate( fDamageAmount + ( g_nPatternReplaceIndex != 2 ) ) );
			cvPaletteColors[3] = lerp( cPattern, float4( g_cPaletteColor3, 1.0f ), saturate( fDamageAmount + ( g_nPatternReplaceIndex != 3 ) ) );
			cvPaletteColors[4] = lerp( cPattern, float4( g_cPaletteColor4, 1.0f ), saturate( fDamageAmount + ( g_nPatternReplaceIndex != 4 ) ) );
			cvPaletteColors[5] = lerp( cPattern, float4( g_cPaletteColor5, 1.0f ), saturate( fDamageAmount + ( g_nPatternReplaceIndex != 5 ) ) );
			cvPaletteColors[6] = lerp( cPattern, float4( g_cPaletteColor6, 1.0f ), saturate( fDamageAmount + ( g_nPatternReplaceIndex != 6 ) ) );
			cvPaletteColors[7] = lerp( cPattern, float4( g_cPaletteColor7, 1.0f ), saturate( fDamageAmount + ( g_nPatternReplaceIndex != 7 ) ) );
		#endif

	#endif
	#if ( ( GENERATEBASETEXTURE == 0 ) && ( GENERATENORMAL == 1 ) )
			cvPaletteColors[0] = float4( 1.0f, 1.0f, 1.0f, 1.0f );
			cvPaletteColors[1] = float4( 1.0f, 1.0f, 1.0f, 1.0f );
			cvPaletteColors[2] = float4(1.0f, 1.0f, 1.0f, 1.0f);
			cvPaletteColors[3] = float4(1.0f, 1.0f, 1.0f, 1.0f);
			cvPaletteColors[4] = float4(1.0f, 1.0f, 1.0f, 1.0f);
			cvPaletteColors[5] = float4(1.0f, 1.0f, 1.0f, 1.0f);
			cvPaletteColors[6] = float4(1.0f, 1.0f, 1.0f, 1.0f);
			cvPaletteColors[7] = float4(1.0f, 1.0f, 1.0f, 1.0f);

			#if ( USEPATTERN > 2 )
				cvPaletteColors[0].a = cPattern.a;
				cvPaletteColors[1].a = cPattern.a;
				cvPaletteColors[2].a = cPattern.a;
				cvPaletteColors[3].a = cPattern.a;
				cvPaletteColors[4].a = cPattern.a;
				cvPaletteColors[5].a = cPattern.a;
				cvPaletteColors[6].a = cPattern.a;
				cvPaletteColors[7].a = cPattern.a;
			#else
				cvPaletteColors[0].a = lerp( cPattern.a, 1.0f, saturate( fDamageAmount + ( g_nPatternReplaceIndex != 0 ) ) );
				cvPaletteColors[1].a = lerp( cPattern.a, 1.0f, saturate( fDamageAmount + ( g_nPatternReplaceIndex != 1 ) ) );
				cvPaletteColors[2].a = lerp( cPattern.a, 1.0f, saturate( fDamageAmount + ( g_nPatternReplaceIndex != 2 ) ) );
				cvPaletteColors[3].a = lerp( cPattern.a, 1.0f, saturate( fDamageAmount + ( g_nPatternReplaceIndex != 3 ) ) );
				cvPaletteColors[4].a = lerp( cPattern.a, 1.0f, saturate( fDamageAmount + ( g_nPatternReplaceIndex != 4 ) ) );
				cvPaletteColors[5].a = lerp( cPattern.a, 1.0f, saturate( fDamageAmount + ( g_nPatternReplaceIndex != 5 ) ) );
				cvPaletteColors[6].a = lerp( cPattern.a, 1.0f, saturate( fDamageAmount + ( g_nPatternReplaceIndex != 6 ) ) );
				cvPaletteColors[7].a = lerp( cPattern.a, 1.0f, saturate( fDamageAmount + ( g_nPatternReplaceIndex != 7 ) ) );
			#endif
	#endif
	#if ( ( GENERATEBASETEXTURE == 1 ) || ( GENERATENORMAL == 1 ) )	
		//switching to float4[2] to save on temp registers
		//float fvSubColorMask[7] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
		float4 fvSubColorMask[2] = { float4( 0, 0, 0, 0 ), float4( 0, 0, 0, 0 ) };
		#if ( CHEAPFILTERING == 0 )

			for ( int i = 0; i < 9; i++ )
			{
				float fSubColorMask = tex2D( MaterialMaskSampler, vTexcoord + fvSampleOffset[i].xy * g_fTexelSize ).a;
				for ( int j = 0; j < 2; j++ )
					for ( int k = 0; k < 4; k++ )
					{
						if ( ( j * 4 + k ) == ( floor( fSubColorMask * 8 ) - 1 ) )
							fvSubColorMask[j][k] += fvSampleOffset[i].z;
					}
			}

			// fill in under the blurred mask
			for ( i = 0; i < 2; i++ )
				for ( int j = 0; j < 4; j++ )
					for ( int k = 0; k <= i; k++ )
					{
						int maxl = ( i == k ) ? j : 4;
						for ( int l = 0; l < maxl; l++ )
							fvSubColorMask[k][l] = saturate( fvSubColorMask[k][l] + ( fvSubColorMask[k][l] > 0 ) * ( fvSubColorMask[i][j] > 0 ) );

					} 
		#else
			float fSubColorMask = tex2D( MaterialMaskSampler, vTexcoord ).a;

			for ( int i = 0; i < 2; i++ )
				for ( int j = 0; j < 4; j++ )
				{
					fvSubColorMask[i][j] = ( ( i * 4 + j ) == ( floor( fSubColorMask * 8 ) - 1 ) );
				}
		#endif
	#endif
	#if ( GENERATEBASETEXTURE == 1)

		cSubColor = lerp( cvPaletteColors[0], cvPaletteColors[1], fvSubColorMask[0][0] );
		cSubColor = lerp( cSubColor, cvPaletteColors[2], fvSubColorMask[0][1] );
		cSubColor = lerp( cSubColor, cvPaletteColors[3], fvSubColorMask[0][2] );
		cSubColor = lerp( cSubColor, cvPaletteColors[4], fvSubColorMask[0][3] );
		cSubColor = lerp( cSubColor, cvPaletteColors[5], fvSubColorMask[1][0] );
		cSubColor = lerp( cSubColor, cvPaletteColors[6], fvSubColorMask[1][1] );
		cSubColor = lerp( cSubColor, cvPaletteColors[7], fvSubColorMask[1][2] );
	#endif
	#if ( ( GENERATEBASETEXTURE == 1 ) || ( GENERATENORMAL == 1 ) )
		float fPatternMask = ( 1.0f - fDamageAmount );

		#if ( USEPATTERN == 1 )
			if ( g_nPatternReplaceIndex == 0 )
			{
				fPatternMask *= 1.0f - fvSubColorMask[0][0];
				fPatternMask *= 1.0f - fvSubColorMask[0][1];
				fPatternMask *= 1.0f - fvSubColorMask[0][2];
				fPatternMask *= 1.0f - fvSubColorMask[0][3];
				fPatternMask *= 1.0f - fvSubColorMask[1][0];
				fPatternMask *= 1.0f - fvSubColorMask[1][1];
				fPatternMask *= 1.0f - fvSubColorMask[1][2];
			}
			else
			{
				fPatternMask *= fvSubColorMask[ g_nPatternReplaceIndex > 4 ][ g_nPatternReplaceIndex > 4 ? g_nPatternReplaceIndex - 5 : g_nPatternReplaceIndex - 1 ];
			}
		#endif

		float fPaintThickness = 0.0f;

		#if ( ( USEPATTERN == 1 ) || ( USEPATTERN == 3 ) )
			fPaintThickness = ( vPatternSample.r + vPatternSample.g + vPatternSample.b ) * g_fPatternPaintThickness;

			if ( g_fPatternPaintThickness > 0 )
			{
				float4 vPatternShadowSample = tex2Dbias( PatternSampler, float4( vPatternTexcoord, 0.0f, 0.5f + 0.1f * g_fPatternPaintThickness ) );
				float fPaintShadowPower = g_fPatternPaintThickness;
				float fPaintShadow = lerp( pow( 1.0f - vPatternShadowSample.r, fPaintShadowPower ), 1.0f, vPatternSample.r );
				fPaintShadow *= lerp( pow( 1.0f - vPatternShadowSample.g, fPaintShadowPower ), 1.0f, vPatternSample.g );
				fPaintShadow *= lerp( pow( 1.0f - vPatternShadowSample.b, fPaintShadowPower ), 1.0f, vPatternSample.b );
				fAO *= lerp( 1.0f, fPaintShadow, fPatternMask );
			}

			fPatternMask *= saturate( fPaintThickness );
			fvFinalDetails.rgb = lerp( fvFinalDetails.rgb, float3( 0.5f, 0.5f, 0.0f ), saturate( fPaintThickness ) * fPatternMask );
			fDetail = lerp( fDetail, 1.0f, saturate( fPaintThickness ) * fPatternMask );
		#endif

	#endif
	#if ( GENERATEBASETEXTURE == 1 )

		float3 cFinalColor = cSubColor.rgb;
		
		float fDamageDetailSaturation = fourWayLerp( g_vDamageDetailSaturation, vMaterialMaskSample.rgb );

		float fSubColorLum = dot( g_lum, cSubColor.rgb );

		// Apply Curvature

		float fDetailCurvatureClean = ( 1.0f - abs( ddx( fvFinalDetails.g * fCurvature ) ) ) * ( 1.0f - abs( ddy( fvFinalDetails.g * fCurvature ) ) );
		fDetailCurvatureClean *= saturate( abs( ddx( fvFinalDetails.g * fCurvature ) ) + abs( ddy ( fvFinalDetails.g * fCurvature ) ) );

		float fDetailCurvatureDamaged = ( 1.0f - abs( ddx( fvFinalDetails.r * fCurvature ) ) ) * ( 1.0f - abs( ddy( fvFinalDetails.r * fCurvature ) ) );
		fDetailCurvatureDamaged *= saturate( abs( ddx( fvFinalDetails.r * fCurvature ) ) + abs( ddy ( fvFinalDetails.r * fCurvature ) ) );

		float fDetailCurvature = lerp( fDetailCurvatureClean, fDetailCurvatureDamaged, fDamageAmount );

		fDetailCurvature *= 1.0f - saturate( abs( ddx( vMaterialMaskSample.r + vMaterialMaskSample.g + vMaterialMaskSample.b ) ) );
		fDetailCurvature *= 1.0f - saturate( abs( ddy( vMaterialMaskSample.r + vMaterialMaskSample.g + vMaterialMaskSample.b ) ) );

		// use a little curvature to exaggerate form
		cFinalColor = lerp( cFinalColor, cFinalColor * fCurvature * 2.0f, 0.5f );
		// lighten sharp edges a little to highlight them
		cFinalColor = lerp( cFinalColor, cFinalColor + cFinalColor * fDetailCurvature, 0.5f );
		
		// Bleaching
		float fWearBleaching = fourWayLerp( g_vWearBleaching, vMaterialMaskSample.rgb );
		float fDamageBleaching = fourWayLerp( g_vDamageBleaching, vMaterialMaskSample.rgb );

		// Grime
		float fGrimeBrightnessAdjustment = fourWayLerp( g_vGrimeBrightness, vMaterialMaskSample.rgb );
		float fGrimeBrightness = dot( cGrunge.rgb, g_lum );
		float fGrimeDetailSaturation = fourWayLerp( g_vGrimeSaturation, vMaterialMaskSample.rgb );
		float3 cGrime = colorize( cGrunge.rgb, cGrunge.rgb, fGrimeBrightness, fGrimeDetailSaturation );
		cGrime *= ( 1.0f + fGrimeBrightnessAdjustment );

		float fBleaching = lerp( g_fWearProgress * fWear * fWearBleaching, fDamageBleaching, fDamageAndWearAmount );

		// Add some grunge even to pristine materials
		float fDetailGrungeFactor = fourWayLerp( g_vDetailGrungeFactor, vMaterialMaskSample.rgb );
		fDetailGrungeFactor = fDetailGrungeFactor * ( 1.0f + g_fWearProgress * fDetailGrungeFactor );
		// Desaturate grunge somewhat at low opacities
		float3 fColorVariation = lerp( cFinalColor, dot( cFinalColor, g_lum ), saturate( 1.0f - fDetailGrungeFactor ) );
		float3 cColorVariation = lerp( cFinalColor, cFinalColor * cGrunge.rgb * 4.0f, fDetailGrungeFactor * ( 1.0f - fDamageAndWearAmount ) );

		cFinalColor = lerp( cColorVariation, cGrime * fDetail, fBleaching );

		float fBrightnessAdjustment = fourWayLerp( g_vDamageBrightness, vMaterialMaskSample.rgb );
		float fBrightness = dot( cFinalColor, g_lum );
		float3 cFinalSaturation = colorize( cFinalColor, cSubColor.rgb, fBrightness, fDamageDetailSaturation );
		cFinalColor = lerp( cFinalColor, cFinalSaturation * ( 1.0f + fBrightnessAdjustment ), fDamageAndWearAmount );		

		// Grunge
		float fGrungeAmount = smoothstep( 0.45f, 0.75f, ( 1.0f - vAOSample.r * vAOSample.g * vAOSample.g ) * g_fWearProgress ) * ( 1.0f - fSubColorLum * 0.5f );
		float fGrungeMax = fourWayLerp( g_vGrungeMax, vMaterialMaskSample.rgb );
		fGrungeAmount *= fGrungeMax;
		cFinalColor = lerp( cFinalColor, cGrunge.rgb * cFinalColor, fGrungeAmount );

		// AO
		cFinalColor = lerp( cFinalColor * cFinalColor * fAO, cFinalColor, fAO );
		cFinalColor = lerp( cFinalColor * cFinalColor * fDetail * 2.0f, cFinalColor, fDetail );

		fBrightness = dot( cFinalColor, g_lum );
		float3 cMottleColor1 = float3( 0.6f, 1.0f, 0.0f );
		float3 cMottleColor2 = float3( 0.2f, 0.6f, 1.0f );
		if ( ( cFinalColor.r > cFinalColor.g ) && ( cFinalColor.r > cFinalColor.b ) )
		{
			cMottleColor1 += cFinalColor.brg;
			cMottleColor2 += cFinalColor.bgr;
		}
		else if ( ( cFinalColor.g > cFinalColor.r ) && ( cFinalColor.g > cFinalColor.b ) )
		{
			cMottleColor1 += cFinalColor.brg;
			cMottleColor2 += cFinalColor.gbr;
		}
		
		float fSaturation = pow( ( 1.0f - fSubColorLum ), 3.5f ) * 0.3f;
		fSaturation += length( cFinalColor - fBrightness.xxx ) * 0.15f + 0.05f;
		float3 fvMottle = tex2D( NoiseSampler, vTexcoord * 2.0f ).rgb;
		float fAOMottle = pow( vAOSample.g, 8.0f );
		fvMottle.r = saturate( fAOMottle * fvMottle.r );
		fvMottle.b = saturate( fvMottle.b * ( 0.5f + fAOMottle * 0.5f ) );

		cFinalColor = lerp( lerp( colorize( cFinalColor, cMottleColor2, fBrightness, fSaturation ), 
								  colorize( cFinalColor, cMottleColor1, fBrightness, fSaturation ), fvMottle.r ),
							cFinalColor, fvMottle.b );
   
		cFinalColor = saturate( cFinalColor );

		vBaseTextureSample.rgb = cFinalColor;

	#endif

	#if ( ( GENERATENORMAL == 1 ) || ( ( GENERATEBASETEXTURE == 1 ) && ( ( BASEALPHAPHONGMASK == 1) || ( BASEALPHAENVMASK == 1 ) ) ) )
		float fDamageNormalEdgeDepth = fourWayLerp( g_vDamageNormalEdgeDepth, vMaterialMaskSample.rgb );

		float2 fvDamageEdgeNormal = float2( 0.0f, 0.0f );
		float2 fPaintModifier = 1.0f;

		#if ( ( USEPATTERN == 1 ) || ( USEPATTERN == 3 ) )
			fPaintModifier = 1.0f + fPatternMask * g_fPatternPaintThickness;
		#endif

		float fDamageEdgeMask = 0.0f;

		for ( int o = 0; o < 2; o++ )
		{
			for ( int n = 0; n < 2; n++ )
			{
				float4 fvGrungeSamples = 0;
				float2 fvSampleOffset = float2( o * 2 - 1, n * 2 - 1 );

				float fGrungeSample = 0;

				float2 vSampleTexcoord = fvGrungeTexcoord + fvSampleOffset.xy * g_fTexelSize * fDamageNormalEdgeDepth * fPaintModifier;
				fvGrungeSamples.x = tex3D( GrungeSampler, float3( vSampleTexcoord, g_sampleSliceZ.x ) ).a;
				fvGrungeSamples.y = tex3D( GrungeSampler, float3( vSampleTexcoord, g_sampleSliceZ.y ) ).a;
				fvGrungeSamples.z = tex3D( GrungeSampler, float3( vSampleTexcoord, g_sampleSliceZ.z ) ).a;
				fvGrungeSamples.w = tex3D( GrungeSampler, float3( vSampleTexcoord, g_sampleSliceZ.w ) ).a;

				fGrungeSample = fourWayLerp( fvGrungeSamples, vMaterialMaskSample.rgb );

				fGrungeSample *= fWearInfluence;	
				fGrungeSample = saturate( fGrungeSample + fCurvatureWearBoost * fCurvature );
				fGrungeSample *= g_fWearProgress;
				fGrungeSample = smoothstep( fvDamageLevels.x, fvDamageLevels.y, fGrungeSample );

				fvDamageEdgeNormal.xy -= ( fDamageAmount - fGrungeSample ) * fvSampleOffset;
				fDamageEdgeMask += abs( fDamageAmount - fGrungeSample );
			}
		}

		fDamageEdgeMask = fDamageEdgeMask * 0.25f;

	#endif

	#if ( GENERATENORMAL == 1 )

		fvDamageEdgeNormal = float2( dot( fvDamageEdgeNormal, g_vGrungeTexcoordRotation[0].xy ),
									 dot( fvDamageEdgeNormal, g_vGrungeTexcoordRotation[1].xy ) );

		float3 fvFinalNormal = vNormalSample * 2.0f - 1.0f;

		// Detail normals

		float4 vNormalSamples[4] = { tex3D( DetailNormalSampler, float3( vTexcoord.xy * g_vDetailScale.x, g_sampleSliceZ.x ) ),
									 tex3D( DetailNormalSampler, float3( vTexcoord.xy * g_vDetailScale.y, g_sampleSliceZ.y ) ),
									 tex3D( DetailNormalSampler, float3( vTexcoord.xy * g_vDetailScale.z, g_sampleSliceZ.z ) ), 
									 tex3D( DetailNormalSampler, float3( vTexcoord.xy * g_vDetailScale.w, g_sampleSliceZ.w ) ) };

		float4 fvDetailNormal = fourWayLerp( vNormalSamples, vMaterialMaskSample.rgb );

		float2 fvDetailNormalDamage = fvDetailNormal.zw * 2.0f - 1.0f;
		float2 fvDetailNormalClean = fvDetailNormal.xy * 2.0f - 1.0f;

		float fDetailNormalDepth = fourWayLerp( g_vDetailNormalDepth, vMaterialMaskSample.rgb );

		fvDetailNormal.xy = lerp( fvDetailNormalClean, fvDetailNormalDamage, fDamageAmount );
		fvDetailNormal.xy *= fDetailNormalDepth;

		float2 fvPaintEdgeNormal = float2( 0.0f, 0.0f );

		#if ( ( USEPATTERN == 1 ) || ( USEPATTERN == 3 ) )

			for ( int m = 0; m < 2; m++ )
			{
				for ( int n = 0; n < 2; n++ )
				{
					float2 fvSampleOffset = float2( m * 2 - 1, n * 2 - 1 );
					
					float3 fvPaintSample = tex2D( PatternSampler, vPatternTexcoord + fvSampleOffset.xy * g_fTexelSize * g_fPatternPaintThickness * 2.0f ).rgb;
					float fPaintSample = ( fvPaintSample.r + fvPaintSample.g + fvPaintSample.b );// * ( 1.0f - fDamageAmount );
					fvPaintEdgeNormal += ( fPaintThickness - fPaintSample ) * fvSampleOffset;
				}
			}

			
			fvPaintEdgeNormal = float2( dot( fvPaintEdgeNormal, g_vPatternTexcoordRotation[0].xy ),
										dot( fvPaintEdgeNormal, g_vPatternTexcoordRotation[1].xy ) );

			fvPaintEdgeNormal.x *= g_fFlipFixup;
			if ( vTexcoord.x < 0 )
			{
				fvPaintEdgeNormal.x *= -1;
			}
			
			fvDetailNormal.xy = lerp( fvDetailNormal.xy, float2( 0, 0 ), min( pow( saturate( fPatternMask * g_fPatternPaintThickness ), 0.5f ), 0.9f ) );
			fvPaintEdgeNormal *= fPatternMask * g_fPatternPaintThickness;
			fDamageNormalEdgeDepth += fPatternMask * g_fPatternPaintThickness;

		#endif

		// Detail normal edges
   
		fvDamageEdgeNormal.xy *= fDamageNormalEdgeDepth /** ( 1.0f - fDamageAmount * fDamageAmount )*/;

		float2 fvEdgeNormals = lerp( fvPaintEdgeNormal.xy, fvDamageEdgeNormal.xy, fDamageAmount );
		vNormalSample.xyz = fvFinalNormal.xyz;
		vNormalSample.xy += fvDetailNormal;
		vNormalSample.xy += fvEdgeNormals;
		vNormalSample.xyz = normalize( vNormalSample.xyz ) * 0.5f + 0.5f;

	#endif

	#if ( ( ( GENERATEBASETEXTURE == 1 ) && ( ( BASEALPHAPHONGMASK == 1 ) || ( BASEALPHAENVMASK == 1 ) ) ) || ( ( GENERATENORMAL == 1 ) && ( ( BASEALPHAPHONGMASK == 0 ) || ( BUMPALPHAENVMASK == 1 ) ) ) )
		float fDetailGrungePhongFactor = fourWayLerp( g_vDetailGrungeFactor, vMaterialMaskSample.rgb );
		fDetailGrungePhongFactor = saturate( g_fWearProgress + fDetailGrungePhongFactor );
		fDetailGrungePhongFactor = lerp( 1.0f, dot( cGrunge.rgb, g_lum ), fDetailGrungePhongFactor );
	#endif

	#if ( ( ( GENERATEBASETEXTURE == 1 ) && ( BASEALPHAPHONGMASK == 1 ) ) || ( ( GENERATENORMAL == 1 ) && ( BASEALPHAPHONGMASK == 0 ) ) )
		float fWearDetailPhongBoost = fourWayLerp( g_vWearDetailPhongBoost, vMaterialMaskSample.rgb );
		float fDamageEdgePhongBoost = fourWayLerp( g_vDamageEdgePhongBoost, vMaterialMaskSample.rgb );
		

		float fDetailSpec = fourWayLerp( g_vDetailSpecBoost, vMaterialMaskSample.rgb ) * fvFinalDetails.a;
		float fDamageDetailSpec = fourWayLerp( g_vDamageDetailSpecBoost, vMaterialMaskSample.rgb ) * fvFinalDetails.r;   
		fDetailSpec = lerp( fDetailSpec, fDamageDetailSpec, fDamageAmount );

		fDetailSpec *= fDetailGrungePhongFactor;

		fDetailSpec = lerp( fDetailSpec, fDetailSpec * fWearDetailPhongBoost, fWearAmount );
		
		fDetailSpec = lerp( fDetailSpec, fDetailSpec * fDamageEdgePhongBoost, fDamageEdgeMask );

		fDetailSpec *= cSubColor.a;

		fDetailSpec = saturate( fDetailSpec );

		#if ( ( GENERATEBASETEXTURE == 1) && ( BASEALPHAPHONGMASK == 1 ) )
			vBaseTextureSample.a = fAO * fAO * fDetailSpec;
		#else
			vNormalSample.a = fAO * fAO * fDetailSpec;
		#endif
	#endif
	#if ( ( ( GENERATEBASETEXTURE == 1 ) && ( BASEALPHAENVMASK == 1) && ( BASEALPHAPHONGMASK == 0 ) ) || ( ( GENERATENORMAL == 1 ) && ( BASEALPHAPHONGMASK == 1 ) && ( BUMPALPHAENVMASK == 1 ) ) )
		float fWearDetailEnvBoost = fourWayLerp( g_vWearDetailEnvBoost, vMaterialMaskSample.rgb );
		float fDamageEdgeEnvBoost = fourWayLerp( g_vDamageEdgeEnvBoost, vMaterialMaskSample.rgb );
		fWearDetailEnvBoost = abs( fWearDetailEnvBoost ) * ( fWearDetailEnvBoost < 0 ? 1.0f - fWear * fvFinalDetails.b : fWear * fvFinalDetails.b );

		float fDetailEnv = fourWayLerp( g_vDetailEnvBoost, vMaterialMaskSample.rgb ) * fvFinalDetails.a;
		float fDamageDetailEnv = fourWayLerp( g_vDamageDetailEnvBoost, vMaterialMaskSample.rgb ) * fvFinalDetails.r;   
		fDetailEnv = lerp( fDetailEnv, fDamageDetailEnv, fDamageAmount );

		fDetailEnv *= fDetailGrungePhongFactor;

		fDetailEnv = lerp( fDetailEnv, fDetailEnv * fWearDetailEnvBoost, fWearAmount );
		
		fDetailEnv = lerp( fDetailEnv, fDetailEnv * fDamageEdgeEnvBoost, fDamageEdgeMask );

		fDetailEnv *= cSubColor.a;

		fDetailEnv = saturate( fDetailEnv );

		#if ( ( GENERATEBASETEXTURE == 1) && ( BASEALPHAENVMASK == 1) )
			vBaseTextureSample.a = fAO * fAO * fDetailEnv;
		#else
			vNormalSample.a = fAO * fAO * fDetailEnv;
		#endif
	#endif

	#if ( GENERATEMASKS1 == 1 )
		//Phong Albedo Tint
   
		float fPhongAlbedoTint = fourWayLerp( g_vDetailPhongAlbedoTint, vMaterialMaskSample.rgb );
   
		// Material ID
   
		float fWarpIndex = fourWayLerp( g_vDetailWarpIndex, vMaterialMaskSample.rgb );
   
		// Metalness
   
		float fMetalness = fourWayLerp( g_vDetailMetalness, vMaterialMaskSample.rgb );
   
		// Masks 1
   
		vMasks1Params.gba = float3( fPhongAlbedoTint, fMetalness, fWarpIndex );
	#endif
}