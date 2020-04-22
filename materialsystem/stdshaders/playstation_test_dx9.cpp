//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
// PS3 test shader
//
//==================================================================================================

#include "BaseVSShader.h"
#include "playstation_test_vs20.inc"
#include "playstation_test_ps20.inc"
#include "playstation_test_ps20b.inc"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_VS_SHADER_FLAGS( PS3TestShader, "PS3 Test Shader", SHADER_NOT_EDITABLE )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( TEST_TEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Test texture to sample from." )
	END_SHADER_PARAMS

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		if( params[ TEST_TEXTURE ]->IsDefined() )
		{
			LoadTexture( TEST_TEXTURE );
		}
	}

	SHADER_DRAW
	{
		SHADOW_STATE 
		{
			pShaderShadow->VertexShaderVertexFormat( VERTEX_POSITION, 1, 0, 0 );

			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );

			DECLARE_STATIC_VERTEX_SHADER( playstation_test_vs20 );
			SET_STATIC_VERTEX_SHADER( playstation_test_vs20 );
			
			DECLARE_STATIC_PIXEL_SHADER( playstation_test_ps20b );
			SET_STATIC_PIXEL_SHADER( playstation_test_ps20b );

			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->DepthFunc( SHADER_DEPTHFUNC_ALWAYS );
			pShaderShadow->EnableAlphaWrites( false );
			pShaderShadow->EnableCulling( false );
		}

		DYNAMIC_STATE
		{
			// Bind textures
			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, TEST_TEXTURE );

			DECLARE_DYNAMIC_VERTEX_SHADER( playstation_test_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( playstation_test_vs20 );

			DECLARE_DYNAMIC_PIXEL_SHADER( playstation_test_ps20b );
			SET_DYNAMIC_PIXEL_SHADER( playstation_test_ps20b );
		}

		Draw();
	}
END_SHADER
