//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "BaseVSShader.h"
#include "common_hlsl_cpp_consts.h"
#include "convar.h"

#include "Downsample_nohdr_ps20.inc"
#include "Downsample_nohdr_ps20b.inc"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar r_bloomtintr( "r_bloomtintr", "0.3" );
static ConVar r_bloomtintg( "r_bloomtintg", "0.59" );
static ConVar r_bloomtintb( "r_bloomtintb", "0.11" );
static ConVar r_bloomtintexponent( "r_bloomtintexponent", "2.2" );

BEGIN_VS_SHADER_FLAGS( Downsample_nohdr, "Help for Downsample_nohdr", SHADER_NOT_EDITABLE )

	BEGIN_SHADER_PARAMS
		SHADER_PARAM( BLOOMTYPE, SHADER_PARAM_TYPE_INTEGER, "0", "" )
		SHADER_PARAM( BLOOMEXP, SHADER_PARAM_TYPE_FLOAT, "2.5", "" )
		SHADER_PARAM( BLOOMSATURATION, SHADER_PARAM_TYPE_FLOAT, "1.0", "" )
		SHADER_PARAM( BLOOMTINTENABLE, SHADER_PARAM_TYPE_INTEGER, "1", "" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_PARAM_FLOAT_IF_NOT_DEFINED( BLOOMEXP, 2.5f );
		SET_PARAM_FLOAT_IF_NOT_DEFINED( BLOOMSATURATION, 1.0f );
		SET_PARAM_INT_IF_NOT_DEFINED( BLOOMTINTENABLE, 1 );

		if ( !params[BLOOMTYPE]->IsDefined() )
		{
			params[BLOOMTYPE]->SetIntValue( 0 );
		}
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		LoadTexture( BASETEXTURE );
	}

	SHADER_DRAW
	{
		// Render targets are pegged as sRGB on OSX, so just force these reads and writes
		bool bForceSRGBReadAndWrite = false;

		SHADOW_STATE
		{
			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableAlphaWrites( true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );

			// Render targets are pegged as sRGB on OSX, so just force these reads and writes
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, bForceSRGBReadAndWrite );
			pShaderShadow->EnableSRGBWrite( bForceSRGBReadAndWrite );

			pShaderShadow->VertexShaderVertexFormat( VERTEX_POSITION, 1, 0, 0 );

			pShaderShadow->SetVertexShader( "Downsample_vs20", 0 );
			
			if( g_pHardwareConfig->SupportsPixelShaders_2_b() || g_pHardwareConfig->ShouldAlwaysUseShaderModel2bShaders() )
			{
				DECLARE_STATIC_PIXEL_SHADER( downsample_nohdr_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( BLOOMTYPE, params[BLOOMTYPE]->GetIntValue() );
				SET_STATIC_PIXEL_SHADER_COMBO( SRGB_INPUT_ADAPTER, bForceSRGBReadAndWrite );
				SET_STATIC_PIXEL_SHADER_COMBO( PS3REGCOUNT48, 0 );
				SET_STATIC_PIXEL_SHADER( downsample_nohdr_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( downsample_nohdr_ps20 );
				SET_STATIC_PIXEL_SHADER_COMBO( BLOOMTYPE, params[BLOOMTYPE]->GetIntValue() );
#ifdef _GAMECONSOLE
				SET_STATIC_PIXEL_SHADER_COMBO( SRGB_INPUT_ADAPTER, bForceSRGBReadAndWrite );
#endif
				SET_STATIC_PIXEL_SHADER_COMBO( PS3REGCOUNT48, 0 );
				SET_STATIC_PIXEL_SHADER( downsample_nohdr_ps20 );
			}
		}

		DYNAMIC_STATE
		{
			BindTexture( SHADER_SAMPLER0, bForceSRGBReadAndWrite ? TEXTURE_BINDFLAGS_SRGBREAD : TEXTURE_BINDFLAGS_NONE, BASETEXTURE, -1 );

			int width, height;
			pShaderAPI->GetBackBufferDimensions( width, height );

			float v[4][4];
			float dX = 1.0f/width;
			float dY = 1.0f/height;

			v[0][0] = .5*dX;
			v[0][1] = .5*dY;
			v[1][0] = 2.5*dX;
			v[1][1] = .5*dY;
			v[2][0] = .5*dX;
			v[2][1] = 2.5*dY;
			v[3][0] = 2.5*dX;
			v[3][1] = 2.5*dY;
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, &v[0][0], 4 );

			pShaderAPI->SetVertexShaderIndex( 0 );

			float flPixelShaderParams[4] = { r_bloomtintr.GetFloat(),
											 r_bloomtintg.GetFloat(),
											 r_bloomtintb.GetFloat(),
											 r_bloomtintexponent.GetFloat() };
			if ( params[ BLOOMTINTENABLE ]->GetIntValue() == 0 )
			{
				flPixelShaderParams[0] = 1.0f;
				flPixelShaderParams[1] = 1.0f;
				flPixelShaderParams[2] = 1.0f;
				flPixelShaderParams[3] = 1.0f;
			}
			pShaderAPI->SetPixelShaderConstant( 0, flPixelShaderParams, 1 );

			float vPsConst1[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			vPsConst1[0] = params[BLOOMEXP]->GetFloatValue();
			vPsConst1[1] = params[BLOOMSATURATION]->GetFloatValue();
			pShaderAPI->SetPixelShaderConstant( 1, vPsConst1, 1 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() || g_pHardwareConfig->ShouldAlwaysUseShaderModel2bShaders() )
			{
				int floatBackBuffer = ( ( g_pHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT ) && !IsX360() ) ? 1 : 0;

				DECLARE_DYNAMIC_PIXEL_SHADER( downsample_nohdr_ps20b );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( FLOAT_BACK_BUFFER, floatBackBuffer );
				SET_DYNAMIC_PIXEL_SHADER( downsample_nohdr_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( downsample_nohdr_ps20 );
				SET_DYNAMIC_PIXEL_SHADER( downsample_nohdr_ps20 );
			}
		}
		Draw();
	}
END_SHADER
