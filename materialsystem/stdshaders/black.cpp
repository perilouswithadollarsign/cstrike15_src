//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
//=====================================================================================//

#include "BaseVSShader.h"
#include "black_vs20.inc"
#include "black_ps20.inc"
#include "black_ps20b.inc"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_VS_SHADER( Black, "Help for Black" )
	BEGIN_SHADER_PARAMS
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
		SET_FLAGS( MATERIAL_VAR_VERTEXFOG );
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			// Set stream format (note that this shader supports compression)
			int flags = VERTEX_POSITION | VERTEX_FORMAT_COMPRESSED;
			 // NOTE: Have to say that we want 1 texcoord here even though we don't use it or you'll get this Warning in another part of the code: 
			//		"ERROR: shader asking for a too-narrow vertex format - you will see errors if running with debug D3D DLLs!\n\tPadding the vertex format with extra texcoords"
			int nTexCoordCount = 1;
			int userDataSize = 0;
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );

			DECLARE_STATIC_VERTEX_SHADER( black_vs20 );
			SET_STATIC_VERTEX_SHADER( black_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( black_ps20b );
				SET_STATIC_PIXEL_SHADER( black_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( black_ps20 );
				SET_STATIC_PIXEL_SHADER( black_ps20 );
			}

			pShaderShadow->EnableSRGBWrite( true );

			FogToFogColor();
		}
		DYNAMIC_STATE
		{
			DECLARE_DYNAMIC_VERTEX_SHADER( black_vs20 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING,  s_pShaderAPI->GetCurrentNumBones() > 0 );
			SET_DYNAMIC_VERTEX_SHADER( black_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( black_ps20b );
				SET_DYNAMIC_PIXEL_SHADER( black_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( black_ps20 );
				SET_DYNAMIC_PIXEL_SHADER( black_ps20 );
			}
			unsigned char gammaFogColor[3];
			pShaderAPI->GetSceneFogColor( gammaFogColor );

			Vector4D fogColorPostTonemapLinearSpace;
			float flToneMappingScaleLinear = pShaderAPI->GetToneMappingScaleLinear().x;
			fogColorPostTonemapLinearSpace.x = flToneMappingScaleLinear * SrgbGammaToLinear( ( 1.0f / 255.0f ) * gammaFogColor[0] );
			fogColorPostTonemapLinearSpace.y = flToneMappingScaleLinear * SrgbGammaToLinear( ( 1.0f / 255.0f ) * gammaFogColor[1] );
			fogColorPostTonemapLinearSpace.z = flToneMappingScaleLinear * SrgbGammaToLinear( ( 1.0f / 255.0f ) * gammaFogColor[2] );
			fogColorPostTonemapLinearSpace.w = 1.0f;
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, fogColorPostTonemapLinearSpace.Base() );
		}
		Draw();
	}

END_SHADER


