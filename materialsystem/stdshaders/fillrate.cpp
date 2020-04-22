//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"

#include "fillrate_vs20.inc"
#include "fillrate_ps20.inc"
#include "fillrate_ps20b.inc"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_VS_SHADER_FLAGS( Fillrate, "Help for Fillrate", SHADER_NOT_EDITABLE )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( PASSCOUNT, SHADER_PARAM_TYPE_INTEGER, "1", "Number of passes for this material" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
	}

	SHADER_INIT
	{
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	
	SHADER_DRAW
	{
		SHADOW_STATE
		{
			pShaderShadow->EnableDepthTest( false );
			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableBlending( true );
			pShaderShadow->BlendFunc( SHADER_BLEND_ONE, SHADER_BLEND_ONE );

			// Set stream format (note that this shader supports compression)
			unsigned int flags = VERTEX_POSITION | VERTEX_FORMAT_COMPRESSED;
			int nTexCoordCount = 1;
			int userDataSize = 0;
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );

			DECLARE_STATIC_VERTEX_SHADER( fillrate_vs20 );
			SET_STATIC_VERTEX_SHADER( fillrate_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( fillrate_ps20b );
				SET_STATIC_PIXEL_SHADER( fillrate_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( fillrate_ps20 );
				SET_STATIC_PIXEL_SHADER( fillrate_ps20 );
			}
		}
		DYNAMIC_STATE
		{
			int numPasses =  params[PASSCOUNT]->GetIntValue();
			float color[4];
			if (g_pConfig->bMeasureFillRate)
			{
				// have to multiply by 2/255 since pixel shader constant are 1.7.
				// Will divide the 2 out in the pixel shader.
				color[0] = numPasses * ( 2.0f / 255.0f );
			}
			else
			{
				color[0] = ( 16 * numPasses ) * ( 2.0f / 255.0f );
			}
			color[1] = 0.0f;
			color[2] = 0.0f;
			color[3] = 0.0f;
			pShaderAPI->SetPixelShaderConstant( 0, color, 1 );

			DECLARE_DYNAMIC_VERTEX_SHADER( fillrate_vs20 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER( fillrate_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( fillrate_ps20b );
				SET_DYNAMIC_PIXEL_SHADER( fillrate_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( fillrate_ps20 );
				SET_DYNAMIC_PIXEL_SHADER( fillrate_ps20 );
			}
		}
		Draw();

		SHADOW_STATE
		{
			pShaderShadow->EnableDepthTest( false );
			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableBlending( true );
			pShaderShadow->BlendFunc( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
			pShaderShadow->VertexShaderVertexFormat( VERTEX_POSITION, 1, 0, 0 );
			pShaderShadow->PolyMode( SHADER_POLYMODEFACE_FRONT_AND_BACK, SHADER_POLYMODE_LINE );

			DECLARE_STATIC_VERTEX_SHADER( fillrate_vs20 );
			SET_STATIC_VERTEX_SHADER( fillrate_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( fillrate_ps20b );
				SET_STATIC_PIXEL_SHADER( fillrate_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( fillrate_ps20 );
				SET_STATIC_PIXEL_SHADER( fillrate_ps20 );
			}
		}
		DYNAMIC_STATE
		{
			float color[4] = { 0.0f, 0.05f, 0.05f, 0.0f };
			pShaderAPI->SetPixelShaderConstant( 0, color, 1 );

			DECLARE_DYNAMIC_VERTEX_SHADER( fillrate_vs20 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER( fillrate_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( fillrate_ps20b );
				SET_DYNAMIC_PIXEL_SHADER( fillrate_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( fillrate_ps20 );
				SET_DYNAMIC_PIXEL_SHADER( fillrate_ps20 );
			}
		}
		Draw();
	}
END_SHADER


