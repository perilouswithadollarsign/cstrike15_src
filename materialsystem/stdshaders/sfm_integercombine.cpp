//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"
#include "sfm_combine_vs20.inc"
#include "sfm_integercombine_ps20.inc"
#include "sfm_integercombine_ps20b.inc"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


BEGIN_VS_SHADER_FLAGS( sfm_integercombine_shader, "Help for SFM integer HDR combine pass", SHADER_NOT_EDITABLE )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( ORIGINALTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "" )	// Original full resolution texture
		SHADER_PARAM( BLURREDTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "" )	// Blurred quarter-resolution texture
		SHADER_PARAM( BLOOMAMOUNT, SHADER_PARAM_TYPE_VEC4, "", "" )			// How much bloom gets added in
		SHADER_PARAM( ISFLOAT, SHADER_PARAM_TYPE_INTEGER, "", "" )			// Float or integer?
	END_SHADER_PARAMS

	SHADER_INIT
	{
		LoadTexture( ORIGINALTEXTURE );
		LoadTexture( BLURREDTEXTURE );
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

			int fmt = VERTEX_POSITION;
			pShaderShadow->VertexShaderVertexFormat( fmt, 1, 0, 0 );

			DECLARE_STATIC_VERTEX_SHADER( sfm_combine_vs20 );
			SET_STATIC_VERTEX_SHADER( sfm_combine_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( sfm_integercombine_ps20b );
				SET_STATIC_PIXEL_SHADER( sfm_integercombine_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( sfm_integercombine_ps20 );
				SET_STATIC_PIXEL_SHADER( sfm_integercombine_ps20 );
			}
		}

		DYNAMIC_STATE
		{
			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, ORIGINALTEXTURE, -1 );
			BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE,  BLURREDTEXTURE, -1 );

			SetPixelShaderConstant( 0, BLOOMAMOUNT );


			bool bIsFloat = params[ISFLOAT]->GetIntValue() != 0;

			DECLARE_DYNAMIC_VERTEX_SHADER( sfm_combine_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( sfm_combine_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( sfm_integercombine_ps20b );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( ISFLOAT, bIsFloat );
				SET_DYNAMIC_PIXEL_SHADER( sfm_integercombine_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( sfm_integercombine_ps20 );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( ISFLOAT, bIsFloat );
				SET_DYNAMIC_PIXEL_SHADER( sfm_integercombine_ps20 );
			}
		}
		Draw();
	}
END_SHADER
