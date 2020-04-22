//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"

#include "debugmrttexture_ps20.inc"
#include "debugmrttexture_ps20b.inc"
#include "debugmrttexture_vs20.inc"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


BEGIN_VS_SHADER_FLAGS( DebugMRTTexture, "Help for DebugMRTTexture", SHADER_NOT_EDITABLE )

	BEGIN_SHADER_PARAMS
		SHADER_PARAM( MRTINDEX, SHADER_PARAM_TYPE_INTEGER, "", "" )
	END_SHADER_PARAMS

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT_PARAMS()
	{
	}

	SHADER_INIT
	{
		LoadTexture( BASETEXTURE );
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );

			DECLARE_STATIC_VERTEX_SHADER( debugmrttexture_vs20 );
			SET_STATIC_VERTEX_SHADER( debugmrttexture_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( debugmrttexture_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( MRTINDEX,  params[MRTINDEX]->GetIntValue() );
				SET_STATIC_PIXEL_SHADER( debugmrttexture_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( debugmrttexture_ps20 );
				SET_STATIC_PIXEL_SHADER_COMBO( MRTINDEX,  params[MRTINDEX]->GetIntValue() );
				SET_STATIC_PIXEL_SHADER( debugmrttexture_ps20 );
			}

			int numTexCoords = 2;
			pShaderShadow->VertexShaderVertexFormat( VERTEX_POSITION, numTexCoords, 0, 0 );
		}
		DYNAMIC_STATE
		{
			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, BASETEXTURE, FRAME );

			DECLARE_DYNAMIC_VERTEX_SHADER( debugmrttexture_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( debugmrttexture_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( debugmrttexture_ps20b );
				SET_DYNAMIC_PIXEL_SHADER( debugmrttexture_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( debugmrttexture_ps20 );
				SET_DYNAMIC_PIXEL_SHADER( debugmrttexture_ps20 );
			}
		}
		Draw();
	}
END_SHADER

