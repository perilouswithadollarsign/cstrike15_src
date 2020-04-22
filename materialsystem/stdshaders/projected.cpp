//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: Projecteding shader for Spy in TF2 (and probably many other things to come)
//
// $NoKeywords: $
//=====================================================================================//

#include "BaseVSShader.h"
#include "convar.h"
#include "projected_dx9_helper.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


DEFINE_FALLBACK_SHADER( Projected, Projected_DX90 )

BEGIN_VS_SHADER( Projected_DX90, "Help for Projected" )

	BEGIN_SHADER_PARAMS
		SHADER_PARAM( SPECTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "" )
	END_SHADER_PARAMS

	void SetupVars( Projected_DX9_Vars_t& info )
	{
		info.m_nBaseTextureTransform = BASETEXTURETRANSFORM;
		info.m_nBaseTexture = BASETEXTURE;
	}

	SHADER_INIT_PARAMS()
	{
		Projected_DX9_Vars_t info;
		SetupVars( info );
		InitParamsProjected_DX9( this, params, pMaterialName, info );
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		Projected_DX9_Vars_t info;
		SetupVars( info );
		InitProjected_DX9( this, params, info );
	}

	SHADER_DRAW
	{
		Projected_DX9_Vars_t info;
		SetupVars( info );
		DrawProjected_DX9( this, params, pShaderAPI, pShaderShadow, info, vertexCompression );
	}
END_SHADER

