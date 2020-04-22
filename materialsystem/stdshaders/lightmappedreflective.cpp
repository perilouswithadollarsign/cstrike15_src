//============ Copyright (c) Valve Corporation, All rights reserved. ============

#include "BaseVSShader.h"
#include "mathlib/vmatrix.h"
#include "common_hlsl_cpp_consts.h" // hack hack hack!

#include "lightmappedreflective_vs20.inc"
#include "lightmappedreflective_ps20.inc"
#include "lightmappedreflective_ps20b.inc"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

DEFINE_FALLBACK_SHADER( LightmappedReflective, LightmappedReflective_DX90 )

BEGIN_VS_SHADER( LightmappedReflective_DX90, "Help for Lightmapped Reflective" )

	BEGIN_SHADER_PARAMS
		SHADER_PARAM( REFRACTTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "_rt_WaterRefraction", "" )
		SHADER_PARAM( REFLECTTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "_rt_WaterReflection", "" )
		SHADER_PARAM( REFRACTAMOUNT, SHADER_PARAM_TYPE_FLOAT, "0", "" )
		SHADER_PARAM( REFRACTTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "refraction tint" )
		SHADER_PARAM( REFLECTAMOUNT, SHADER_PARAM_TYPE_FLOAT, "0.8", "" )
		SHADER_PARAM( REFLECTTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "reflection tint" )
		SHADER_PARAM( NORMALMAP, SHADER_PARAM_TYPE_TEXTURE, "dev/water_normal", "normal map" )
		SHADER_PARAM( BUMPFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $bumpmap" )
		SHADER_PARAM( BUMPTRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$bumpmap texcoord transform" )
		SHADER_PARAM( ENVMAPMASK, SHADER_PARAM_TYPE_TEXTURE, "shadertest/shadertest_envmask", "envmap mask" )
		SHADER_PARAM( ENVMAPMASKFRAME, SHADER_PARAM_TYPE_INTEGER, "", "" )
		SHADER_PARAM( REFLECTANCE, SHADER_PARAM_TYPE_FLOAT, "0.25", "" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		if ( !params[REFLECTANCE]->IsDefined() )
		{
			params[REFLECTANCE]->SetFloatValue( 0.25f );
		}

		SET_FLAGS2( MATERIAL_VAR2_NEEDS_TANGENT_SPACES );
		if ( params[BASETEXTURE]->IsDefined() )
		{
			SET_FLAGS2( MATERIAL_VAR2_LIGHTING_LIGHTMAP );
			if ( g_pConfig->UseBumpmapping() && params[NORMALMAP]->IsDefined() )
			{
				SET_FLAGS2( MATERIAL_VAR2_LIGHTING_BUMPED_LIGHTMAP );
			}
		}
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		if ( params[REFRACTTEXTURE]->IsDefined() )
		{
			LoadTexture( REFRACTTEXTURE, g_pHardwareConfig->GetHDRType() == HDR_TYPE_INTEGER || IsOSX() ? TEXTUREFLAGS_SRGB : 0 );
		}
		if ( params[REFLECTTEXTURE]->IsDefined() )
		{
			LoadTexture( REFLECTTEXTURE, g_pHardwareConfig->GetHDRType() == HDR_TYPE_INTEGER || IsOSX() ? TEXTUREFLAGS_SRGB : 0 );
		}
		if ( params[NORMALMAP]->IsDefined() )
		{
			LoadBumpMap( NORMALMAP );
		}
		if( params[BASETEXTURE]->IsDefined() )
		{
			LoadTexture( BASETEXTURE, TEXTUREFLAGS_SRGB );
			
			if( params[ENVMAPMASK]->IsDefined() )
			{
				LoadTexture( ENVMAPMASK );
			}
		}
		else
		{
			params[ENVMAPMASK]->SetUndefined();
		}
	}

	inline void DrawReflectionRefraction( IMaterialVar **params, IShaderShadow *pShaderShadow,
		IShaderDynamicAPI *pShaderAPI, bool bReflection, bool bRefraction )
	{
		BlendType_t nBlendType = EvaluateBlendRequirements( BASETEXTURE, true );
		bool bFullyOpaque = ( nBlendType != BT_BLENDADD ) && ( nBlendType != BT_BLEND ) && !IS_FLAG_SET( MATERIAL_VAR_ALPHATEST ); //dest alpha is free for special use

		SHADOW_STATE
		{
			SetInitialShadowState( );
			if( bRefraction )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );	// Refract
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, g_pHardwareConfig->GetHDRType() == HDR_TYPE_INTEGER || IsOSX() );
				
				pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );	// Base
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, true );
			}
			
			if( bReflection )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );	// Reflect
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER2, g_pHardwareConfig->GetHDRType() == HDR_TYPE_INTEGER || IsOSX() );
				
				pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );	// Lightmap
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER3, g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE );
			}
			
			if( params[BASETEXTURE]->IsTexture() )
			{
				// BASETEXTURE
				pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, true );
				
				// LIGHTMAP
				pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER3, g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE );

				if ( params[ENVMAPMASK]->IsTexture() )
				{
					pShaderShadow->EnableTexture( SHADER_SAMPLER6, true );
				}
			}

			// normal map
			pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );

			int fmt = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_TANGENT_S | VERTEX_TANGENT_T;

			// texcoord0 : base texcoord
			// texcoord1 : lightmap texcoord
			// texcoord2 : lightmap texcoord offset
			int numTexCoords = 1;
			if ( params[BASETEXTURE]->IsTexture() )
			{
				numTexCoords = 3;
			}
			pShaderShadow->VertexShaderVertexFormat( fmt, numTexCoords, 0, 0 );

			if ( IS_FLAG_SET( MATERIAL_VAR_TRANSLUCENT ) )
			{
				EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
			}

			DECLARE_STATIC_VERTEX_SHADER( lightmappedreflective_vs20 );
			SET_STATIC_VERTEX_SHADER_COMBO( BASETEXTURE, params[BASETEXTURE]->IsTexture() );
			SET_STATIC_VERTEX_SHADER( lightmappedreflective_vs20 );

			// "REFLECT" "0..1"
			// "REFRACT" "0..1"

			if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( lightmappedreflective_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( REFLECT, bReflection );
				SET_STATIC_PIXEL_SHADER_COMBO( REFRACT, bRefraction );
				SET_STATIC_PIXEL_SHADER_COMBO( BASETEXTURE, params[BASETEXTURE]->IsTexture() );
				SET_STATIC_PIXEL_SHADER_COMBO( ENVMAPMASK, params[ENVMAPMASK]->IsTexture() && params[BASETEXTURE]->IsTexture() );
				SET_STATIC_PIXEL_SHADER( lightmappedreflective_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( lightmappedreflective_ps20 );
				SET_STATIC_PIXEL_SHADER_COMBO( REFLECT, bReflection );
				SET_STATIC_PIXEL_SHADER_COMBO( REFRACT, bRefraction );
				SET_STATIC_PIXEL_SHADER_COMBO( BASETEXTURE, params[BASETEXTURE]->IsTexture() );
				SET_STATIC_PIXEL_SHADER_COMBO( ENVMAPMASK, params[ENVMAPMASK]->IsTexture() && params[BASETEXTURE]->IsTexture() );
				SET_STATIC_PIXEL_SHADER( lightmappedreflective_ps20 );
			}

			FogToFogColor();

			// we are writing linear values from this shader.
			pShaderShadow->EnableSRGBWrite( true );

			pShaderShadow->EnableAlphaWrites( bFullyOpaque );
		}
		DYNAMIC_STATE
		{
			TextureBindFlags_t nBindFlags = ( g_pHardwareConfig->GetHDRType() == HDR_TYPE_INTEGER || IsOSXOpenGL() ) ? TEXTURE_BINDFLAGS_SRGBREAD : TEXTURE_BINDFLAGS_NONE;


			if ( bRefraction )
			{
				// HDRFIXME: add comment about binding.. Specify the number of MRTs in the enable
				BindTexture( SHADER_SAMPLER0, nBindFlags, REFRACTTEXTURE, -1 );
			}

			if ( bReflection )
			{
				BindTexture( SHADER_SAMPLER2, nBindFlags, REFLECTTEXTURE, -1 );
			}

			BindTexture( SHADER_SAMPLER4, TEXTURE_BINDFLAGS_NONE, NORMALMAP, BUMPFRAME );

			if ( params[BASETEXTURE]->IsTexture() )
			{
				BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_SRGBREAD, BASETEXTURE, FRAME );
				pShaderAPI->BindStandardTexture( SHADER_SAMPLER3, ( g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE ) ? TEXTURE_BINDFLAGS_SRGBREAD : TEXTURE_BINDFLAGS_NONE, TEXTURE_LIGHTMAP );
				SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3, BASETEXTURETRANSFORM );

				if ( params[ENVMAPMASK]->IsTexture() )
				{
					BindTexture( SHADER_SAMPLER6, TEXTURE_BINDFLAGS_NONE, ENVMAPMASK, ENVMAPMASKFRAME );
				}
			}

			// Refraction tint
			if ( bRefraction )
			{
				SetPixelShaderConstantGammaToLinear( 1, REFRACTTINT );
			}

			// Reflection tint
			if ( bReflection )
			{
				SetPixelShaderConstantGammaToLinear( 4, REFLECTTINT );
			}

			SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_1, BUMPTRANSFORM );

			float c0[4] = { 1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f, 0.0f };
			pShaderAPI->SetPixelShaderConstant( 0, c0, 1 );

			float c2[4] = { 0.5f, 0.5f, 0.5f, 0.5f };
			pShaderAPI->SetPixelShaderConstant( 2, c2, 1 );

			// fresnel constants
			float c3[4] = { clamp( params[REFLECTANCE]->GetFloatValue(), 0.0f, 1.0f ), 0.0f, 0.0f, 0.0f };
			pShaderAPI->SetPixelShaderConstant( 3, c3, 1 );

			float c5[4] = { params[REFLECTAMOUNT]->GetFloatValue(), params[REFLECTAMOUNT]->GetFloatValue(),
				params[REFRACTAMOUNT]->GetFloatValue(), params[REFRACTAMOUNT]->GetFloatValue() };
			pShaderAPI->SetPixelShaderConstant( 5, c5, 1 );

			pShaderAPI->SetPixelShaderFogParams( PSREG_FOG_PARAMS );

			float vEyePos_SpecExponent[4];
			pShaderAPI->GetWorldSpaceCameraPosition( vEyePos_SpecExponent );
			vEyePos_SpecExponent[3] = 0.0f;
			pShaderAPI->SetPixelShaderConstant( PSREG_EYEPOS_SPEC_EXPONENT, vEyePos_SpecExponent, 1 );

			DECLARE_DYNAMIC_VERTEX_SHADER( lightmappedreflective_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( lightmappedreflective_vs20 );

			if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( lightmappedreflective_ps20b );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITE_DEPTH_TO_DESTALPHA, bFullyOpaque && pShaderAPI->ShouldWriteDepthToDestAlpha() );
				SET_DYNAMIC_PIXEL_SHADER( lightmappedreflective_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( lightmappedreflective_ps20 );
				SET_DYNAMIC_PIXEL_SHADER( lightmappedreflective_ps20 );
			}
		}
		Draw();
	}

	SHADER_DRAW
	{
		bool bRefraction = params[REFRACTTEXTURE]->IsTexture();
		bool bReflection = params[REFLECTTEXTURE]->IsTexture();
		DrawReflectionRefraction( params, pShaderShadow, pShaderAPI, bReflection, bRefraction );
	}

END_SHADER
