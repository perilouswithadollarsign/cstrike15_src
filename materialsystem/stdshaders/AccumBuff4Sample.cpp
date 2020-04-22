//========= Copyright (c) 1996-2005, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "BaseVSShader.h"
#include "common_hlsl_cpp_consts.h"
#include "screenspaceeffect_vs20.inc"
#include "accumbuff4sample_ps20.inc"
#include "accumbuff4sample_ps20b.inc"
#include "convar.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


BEGIN_VS_SHADER_FLAGS( accumbuff4sample, "Help for AccumBuff4Sample", SHADER_NOT_EDITABLE )
	BEGIN_SHADER_PARAMS

		// Four textures to sample
		SHADER_PARAM( TEXTURE0, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( TEXTURE1, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( TEXTURE2, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( TEXTURE3, SHADER_PARAM_TYPE_TEXTURE, "", "" )

		// Corresponding weights for the four input textures
		SHADER_PARAM( WEIGHTS, SHADER_PARAM_TYPE_VEC4, "", "Weight for Samples" )

	END_SHADER_PARAMS

	SHADER_INIT
	{
		LoadTexture( TEXTURE0 );
		LoadTexture( TEXTURE1 );
		LoadTexture( TEXTURE2 );
		LoadTexture( TEXTURE3 );
	}
	
	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableDepthTest( false );
			pShaderShadow->EnableAlphaWrites( true );
			pShaderShadow->EnableBlending( false );
			pShaderShadow->EnableCulling( false );

			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );
			int fmt = VERTEX_POSITION;
			pShaderShadow->VertexShaderVertexFormat( fmt, 1, 0, 0 );

			DECLARE_STATIC_VERTEX_SHADER( screenspaceeffect_vs20 );
			SET_STATIC_VERTEX_SHADER( screenspaceeffect_vs20 );
			
			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( accumbuff4sample_ps20b );
				SET_STATIC_PIXEL_SHADER( accumbuff4sample_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( accumbuff4sample_ps20 );
				SET_STATIC_PIXEL_SHADER( accumbuff4sample_ps20 );
			}
		}

		DYNAMIC_STATE
		{
			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, TEXTURE0, -1 );
			BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, TEXTURE1, -1 );
			BindTexture( SHADER_SAMPLER2, TEXTURE_BINDFLAGS_NONE, TEXTURE2, -1 );
			BindTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, TEXTURE3, -1 );

			SetPixelShaderConstant( 0, WEIGHTS );

			DECLARE_DYNAMIC_VERTEX_SHADER( screenspaceeffect_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( screenspaceeffect_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( accumbuff4sample_ps20b );
				SET_DYNAMIC_PIXEL_SHADER( accumbuff4sample_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( accumbuff4sample_ps20 );
				SET_DYNAMIC_PIXEL_SHADER( accumbuff4sample_ps20 );
			}
		}
		Draw();
	}
END_SHADER
