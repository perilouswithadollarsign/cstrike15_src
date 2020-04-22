//===== Copyright © 1996-2007, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"
#include "sfm_ao_blur_vs30.inc"
#include "sfm_ao_blur_ps30.inc"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


BEGIN_VS_SHADER_FLAGS( sfm_ao_blur_shader, "Help for SFM ambient occlusion blur pass", SHADER_NOT_EDITABLE )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( FRONTNDTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( AOTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( JITTERSEED, SHADER_PARAM_TYPE_VEC4, "", "" )
	END_SHADER_PARAMS

	SHADER_INIT
	{
		LoadTexture( FRONTNDTEXTURE );
		LoadTexture( AOTEXTURE );
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
			pShaderShadow->EnableAlphaWrites( false );
			pShaderShadow->EnableBlending( false );
			pShaderShadow->EnableCulling( false );

			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );	// Noise map

			int fmt = VERTEX_POSITION;
			pShaderShadow->VertexShaderVertexFormat( fmt, 1, NULL, 0 );

			DECLARE_STATIC_VERTEX_SHADER( sfm_ao_blur_vs30 );
			SET_STATIC_VERTEX_SHADER( sfm_ao_blur_vs30 );

			DECLARE_STATIC_PIXEL_SHADER( sfm_ao_blur_ps30 );
			SET_STATIC_PIXEL_SHADER( sfm_ao_blur_ps30 );
		}

		DYNAMIC_STATE
		{
			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, FRONTNDTEXTURE, -1 );
			BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, AOTEXTURE, -1 );

			// Dimensions of screen, used for screen-space noise map sampling
			float vScreenScale[4] = {1280.0f / 32.0f, 720.0f / 32.0f, 0, 0};
			int nWidth, nHeight;
			pShaderAPI->GetBackBufferDimensions( nWidth, nHeight );
			vScreenScale[0] = (float) nWidth  / 32.0f;
			vScreenScale[1] = (float) nHeight / 32.0f;
			pShaderAPI->SetPixelShaderConstant( 0, vScreenScale, 1 );

			float vNoiseOffset[4];
			HashShadow2DJitter( params[JITTERSEED]->GetFloatValue(), vNoiseOffset, vNoiseOffset+1 );
			pShaderAPI->SetPixelShaderConstant( 1, vNoiseOffset, 1 );

			DECLARE_DYNAMIC_VERTEX_SHADER( sfm_ao_blur_vs30 );
			SET_DYNAMIC_VERTEX_SHADER( sfm_ao_blur_vs30 );

			DECLARE_DYNAMIC_PIXEL_SHADER( sfm_ao_blur_ps30 );
			SET_DYNAMIC_PIXEL_SHADER( sfm_ao_blur_ps30 );

		}
		Draw();
	}
END_SHADER
