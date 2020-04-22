//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "BaseVSShader.h"
#include "mathlib/vmatrix.h"
#include "common_hlsl_cpp_consts.h" // hack hack hack!
#include "convar.h"

#include "character_ssao_vs20.inc"
#include "character_ssao_vs30.inc"
#include "character_ssao_ps20.inc"
#include "character_ssao_ps20b.inc"
#include "character_ssao_ps30.inc"
#include "shaderlib/commandbuilder.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

// this shader processes a screen-space normal + depth prepass into screen-space AO for later reference

#define NOISE_SAMPLE 0

BEGIN_VS_SHADER( CharacterSSAO, "Help for CharacterSSAO" )

	BEGIN_SHADER_PARAMS
		
		#ifdef NOISE_SAMPLE
			SHADER_PARAM( NOISEMAP, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		#endif

	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS2( MATERIAL_VAR2_LIGHTING_UNLIT );
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		if( params[BASETEXTURE]->IsDefined() )
		{
			LoadTexture( BASETEXTURE, 0 );
		}
		#ifdef NOISE_SAMPLE
			if( params[NOISEMAP]->IsDefined() )
			{
				LoadTexture( NOISEMAP, 0 );
			}
		#endif
	}

	inline void DrawCharacterSSAO( IMaterialVar **params, IShaderShadow* pShaderShadow, IShaderDynamicAPI* pShaderAPI, int vertexCompression )
	{
		SHADOW_STATE
		{
			SetInitialShadowState( );

			if( params[BASETEXTURE]->IsDefined() )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			}

			#ifdef NOISE_SAMPLE
				if( params[NOISEMAP]->IsDefined() )
				{
					pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
				}
			#endif

			unsigned int flags = VERTEX_POSITION;
			int nTexCoordCount = 1;
			int userDataSize = 0;
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );
			
			DECLARE_STATIC_VERTEX_SHADER( character_ssao_vs20 );
			SET_STATIC_VERTEX_SHADER( character_ssao_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_STATIC_PIXEL_SHADER( character_ssao_ps30 );
				SET_STATIC_PIXEL_SHADER_COMBO( QUALITY_MODE, 2 );
				SET_STATIC_PIXEL_SHADER( character_ssao_ps30 );
			}
			else if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( character_ssao_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( QUALITY_MODE, 1 );
				SET_STATIC_PIXEL_SHADER( character_ssao_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( character_ssao_ps20 );
				SET_STATIC_PIXEL_SHADER_COMBO( QUALITY_MODE, 0 );
				SET_STATIC_PIXEL_SHADER( character_ssao_ps20 );
			}
			
			pShaderShadow->EnableAlphaWrites( false );
			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableSRGBWrite( false );

			#ifdef NOISE_SAMPLE
				EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				pShaderShadow->EnableBlending( true );
			#else
				pShaderShadow->EnableBlending( false );
			#endif

		}
		DYNAMIC_STATE
		{
			pShaderAPI->SetDefaultState();

			if( params[BASETEXTURE]->IsDefined() )
			{
				BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, BASETEXTURE, -1 );
			}

			#ifdef NOISE_SAMPLE
				if( params[NOISEMAP]->IsDefined() )
				{
					BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, NOISEMAP, -1 );
				}
			#endif

			SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, BASETEXTURETRANSFORM );
			
			#ifdef NOISE_SAMPLE
				float c0[4] = {	fmod( pShaderAPI->CurrentTime(), 1.0f ) * 14.54321f, fmod( pShaderAPI->CurrentTime(), 1.0f ) * 16.12345f, 0, 0 };
				pShaderAPI->SetPixelShaderConstant( 0, c0, 1 );
			#endif

			DECLARE_DYNAMIC_VERTEX_SHADER( character_ssao_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( character_ssao_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( character_ssao_ps20b );
				SET_DYNAMIC_PIXEL_SHADER( character_ssao_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( character_ssao_ps20 );
				SET_DYNAMIC_PIXEL_SHADER( character_ssao_ps20 );
			}
		}
		Draw();
	}

	SHADER_DRAW
	{
		DrawCharacterSSAO( params, pShaderShadow, pShaderAPI, vertexCompression );
	}
END_SHADER


