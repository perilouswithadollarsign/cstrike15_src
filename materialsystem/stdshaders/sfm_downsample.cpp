//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"
#include "common_hlsl_cpp_consts.h"

#include "downsample_nohdr_ps20.inc"
#include "downsample_nohdr_ps20b.inc"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


BEGIN_VS_SHADER_FLAGS( sfm_downsample_shader, "Help for Downsample", SHADER_NOT_EDITABLE )
	BEGIN_SHADER_PARAMS
	END_SHADER_PARAMS

	SHADER_INIT
	{
		LoadTexture( BASETEXTURE );
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
			pShaderShadow->EnableAlphaWrites( true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, false );
			pShaderShadow->EnableSRGBWrite( false );

			pShaderShadow->VertexShaderVertexFormat( VERTEX_POSITION, 1, 0, 0 );

			pShaderShadow->SetVertexShader( "downsample_vs20", 0 );

			DECLARE_STATIC_PIXEL_SHADER( downsample_nohdr_ps20b );
			SET_STATIC_PIXEL_SHADER_COMBO( BLOOMTYPE, 0 );
			SET_STATIC_PIXEL_SHADER_COMBO( PS3REGCOUNT48, 0 );
			SET_STATIC_PIXEL_SHADER_COMBO( SRGB_INPUT_ADAPTER, 0 ); // Mac sRGB insanity			
			SET_STATIC_PIXEL_SHADER( downsample_nohdr_ps20b );
		}

		DYNAMIC_STATE
		{
			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, BASETEXTURE, -1 );

			int width, height;
			pShaderAPI->GetCurrentRenderTargetDimensions( width, height );

			float v[4][4];
			float dX = 1.0f / (float) width;
			float dY = 1.0f / (float) height;

			v[0][0] = 0.5 * dX;
			v[0][1] = 0.5 * dY;
			v[1][0] = 2.5 * dX;
			v[1][1] = 0.5 * dY;
			v[2][0] = 0.5 * dX;
			v[2][1] = 2.5 * dY;
			v[3][0] = 2.5 * dX;
			v[3][1] = 2.5 * dY;
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, &v[0][0], 4 );

			pShaderAPI->SetVertexShaderIndex( 0 );

			float flPixelShaderParams[4] = { 1.0f, 1.0f, 1.0f, 2.2f };
			pShaderAPI->SetPixelShaderConstant( 0, flPixelShaderParams, 1 );

			float vPsConst1[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			pShaderAPI->SetPixelShaderConstant( 1, vPsConst1, 1 );

			DECLARE_DYNAMIC_PIXEL_SHADER( downsample_nohdr_ps20b );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( FLOAT_BACK_BUFFER, 1 );
			SET_DYNAMIC_PIXEL_SHADER( downsample_nohdr_ps20b );
		}
		Draw();
	}
END_SHADER
