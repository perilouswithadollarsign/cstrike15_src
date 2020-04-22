//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: Parallax Occlusion Mapping test shader
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#include "BaseVSShader.h"
#include "convar.h"
#include "parallaxtest_vs30.inc"
#include "parallaxtest_ps30.inc"

ConVar mat_parallaxmapsamplesmin( "mat_parallaxmapsamplesmin", "12" );
ConVar mat_parallaxmapsamplesmax( "mat_parallaxmapsamplesmax", "50" );

BEGIN_VS_SHADER( ParallaxTest,
				 "Help for ParallaxTest" )

	BEGIN_SHADER_PARAMS
		SHADER_PARAM( BUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "models/shadertest/shader1_normal", "bump map" )
		SHADER_PARAM( BUMPFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $bumpmap" )
	END_SHADER_PARAMS

	SHADER_FALLBACK
	{
		return 0;
	}

	// Set up anything that is necessary to make decisions in SHADER_FALLBACK.
	SHADER_INIT_PARAMS()
	{
	}

	SHADER_INIT
	{
		LoadTexture( BASETEXTURE );
		LoadTexture( BUMPMAP );
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_TANGENT_S | VERTEX_TANGENT_T;
			int numTexCoords = 1;
			pShaderShadow->VertexShaderVertexFormat( flags, numTexCoords, 0, 0 );

			// base
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			// normal + height
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );

			DECLARE_STATIC_VERTEX_SHADER( parallaxtest_vs30 );
			SET_STATIC_VERTEX_SHADER( parallaxtest_vs30 );

			DECLARE_STATIC_PIXEL_SHADER( parallaxtest_ps30 );
			SET_STATIC_PIXEL_SHADER( parallaxtest_ps30 );
		}
		DYNAMIC_STATE
		{
			// base texture
			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, BASETEXTURE, FRAME );
			
			// normal + height
			BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, BUMPMAP, BUMPFRAME );
			
			DECLARE_DYNAMIC_VERTEX_SHADER( parallaxtest_vs30 );
			SET_DYNAMIC_VERTEX_SHADER( parallaxtest_vs30 );

			DECLARE_DYNAMIC_PIXEL_SHADER( parallaxtest_ps30 );
			SET_DYNAMIC_PIXEL_SHADER( parallaxtest_ps30 );

			Vector4D c0( mat_parallaxmapsamplesmin.GetFloat(), mat_parallaxmapsamplesmax.GetFloat(), 0.0f, 0.0f );
			pShaderAPI->SetPixelShaderConstant( 0, c0.Base(), 1 );
		}
		Draw();
	}
END_SHADER
 
