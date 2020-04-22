//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "BaseVSShader.h"
#include "vertexlitgeneric_dx9_helper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DEFINE_FALLBACK_SHADER( Wireframe, Wireframe_DX9 )

BEGIN_VS_SHADER( Wireframe_DX9,
			  "Help for Wireframe_DX9" )

	BEGIN_SHADER_PARAMS
		SHADER_PARAM( DISPLACEMENTMAP, SHADER_PARAM_TYPE_TEXTURE, "shadertest/BaseTexture", "Displacement map" )
		SHADER_PARAM( DISPLACEMENTWRINKLE, SHADER_PARAM_TYPE_BOOL, "0", "Displacement map contains wrinkle displacements")
	END_SHADER_PARAMS

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT_PARAMS()
	{
		VertexLitGeneric_DX9_Vars_t vars;
		vars.m_nDisplacementMap = DISPLACEMENTMAP;
		vars.m_nDisplacementWrinkleMap = DISPLACEMENTWRINKLE;
		InitParamsVertexLitGeneric_DX9( this, params, pMaterialName, false, vars );

		SET_FLAGS( MATERIAL_VAR_NO_DEBUG_OVERRIDE );
		SET_FLAGS( MATERIAL_VAR_NOFOG );
		SET_FLAGS( MATERIAL_VAR_WIREFRAME );
	}

	SHADER_INIT
	{
		VertexLitGeneric_DX9_Vars_t vars;
		vars.m_nDisplacementMap = DISPLACEMENTMAP;
		vars.m_nDisplacementWrinkleMap = DISPLACEMENTWRINKLE;
		InitVertexLitGeneric_DX9( this, params, false, vars );
	}

	SHADER_DRAW
	{
		VertexLitGeneric_DX9_Vars_t vars;
		vars.m_nDisplacementMap = DISPLACEMENTMAP;
		vars.m_nDisplacementWrinkleMap = DISPLACEMENTWRINKLE;
		DrawVertexLitGeneric_DX9( this, params, pShaderAPI, pShaderShadow, false, vars, vertexCompression, pContextDataPtr );
	}
END_SHADER

