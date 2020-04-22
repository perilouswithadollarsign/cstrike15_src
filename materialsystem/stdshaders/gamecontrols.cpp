//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"
#include "gamecontrols_vs20.inc"
#include "gamecontrols_ps20.inc"


BEGIN_VS_SHADER_FLAGS( GAMECONTROLS, "Help for Game Controls", SHADER_NOT_EDITABLE )
	BEGIN_SHADER_PARAMS
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS( MATERIAL_VAR_TRANSLUCENT );
		SET_FLAGS( MATERIAL_VAR_NOCULL );
		SET_FLAGS( MATERIAL_VAR_IGNOREZ );
	}

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
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );

			int nTexCoordDimensions = 3;
			pShaderShadow->VertexShaderVertexFormat( VERTEX_POSITION | VERTEX_COLOR, 1, &nTexCoordDimensions, 0 );
			EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );

			DECLARE_STATIC_VERTEX_SHADER( gamecontrols_vs20 );
			SET_STATIC_VERTEX_SHADER( gamecontrols_vs20 );

			DECLARE_STATIC_PIXEL_SHADER( gamecontrols_ps20 );
			SET_STATIC_PIXEL_SHADER( gamecontrols_ps20 );
		}
		DYNAMIC_STATE
		{
			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, BASETEXTURE );

			DECLARE_DYNAMIC_VERTEX_SHADER( gamecontrols_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( gamecontrols_vs20 );

			DECLARE_DYNAMIC_PIXEL_SHADER( gamecontrols_ps20 );
			SET_DYNAMIC_PIXEL_SHADER( gamecontrols_ps20 );
		}
		Draw();
	}
END_SHADER
