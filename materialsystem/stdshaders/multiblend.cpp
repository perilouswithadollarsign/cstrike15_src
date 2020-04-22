//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: Multiblending shader for Spy in TF2 (and probably many other things to come)
//
// $NoKeywords: $
//=====================================================================================//

#include "BaseVSShader.h"
#include "convar.h"
#include "multiblend_dx9_helper.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


DEFINE_FALLBACK_SHADER( Multiblend, Multiblend_DX90 )

BEGIN_VS_SHADER( Multiblend_DX90, "Help for Multiblend" )

	BEGIN_SHADER_PARAMS
		SHADER_PARAM( SPECTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( BASETEXTURE2, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( SPECTEXTURE2, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( BASETEXTURE3, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( SPECTEXTURE3, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( BASETEXTURE4, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( SPECTEXTURE4, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( ROTATION, SHADER_PARAM_TYPE_FLOAT, "0.0", "" )
		SHADER_PARAM( SCALE, SHADER_PARAM_TYPE_FLOAT, "1.0", "" )
		SHADER_PARAM( ROTATION2, SHADER_PARAM_TYPE_FLOAT, "0.0", "" )
		SHADER_PARAM( SCALE2, SHADER_PARAM_TYPE_FLOAT, "1.0", "" )
		SHADER_PARAM( ROTATION3, SHADER_PARAM_TYPE_FLOAT, "0.0", "" )
		SHADER_PARAM( SCALE3, SHADER_PARAM_TYPE_FLOAT, "1.0", "" )
		SHADER_PARAM( ROTATION4, SHADER_PARAM_TYPE_FLOAT, "0.0", "" )
		SHADER_PARAM( SCALE4, SHADER_PARAM_TYPE_FLOAT, "1.0", "" )
	END_SHADER_PARAMS

	void SetupVars( Multiblend_DX9_Vars_t& info )
	{
		info.m_nBaseTextureTransform = BASETEXTURETRANSFORM;
		info.m_nBaseTexture = BASETEXTURE;
		info.m_nSpecTexture = SPECTEXTURE;
		info.m_nBaseTexture2 = BASETEXTURE2;
		info.m_nSpecTexture2 = SPECTEXTURE2;
		info.m_nBaseTexture3 = BASETEXTURE3;
		info.m_nSpecTexture3 = SPECTEXTURE3;
		info.m_nBaseTexture4 = BASETEXTURE4;
		info.m_nSpecTexture4 = SPECTEXTURE4;
		info.m_nRotation = ROTATION;
		info.m_nRotation2 = ROTATION2;
		info.m_nRotation3 = ROTATION3;
		info.m_nRotation4 = ROTATION4;
		info.m_nScale = SCALE;
		info.m_nScale2 = SCALE2;
		info.m_nScale3 = SCALE3;
		info.m_nScale4 = SCALE4;
	}

	SHADER_INIT_PARAMS()
	{
		Multiblend_DX9_Vars_t info;
		SetupVars( info );
		InitParamsMultiblend_DX9( this, params, pMaterialName, info );
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		Multiblend_DX9_Vars_t info;
		SetupVars( info );
		InitMultiblend_DX9( this, params, info );
	}

	SHADER_DRAW
	{
		Multiblend_DX9_Vars_t info;
		SetupVars( info );
		DrawMultiblend_DX9( this, params, pShaderAPI, pShaderShadow, info, vertexCompression );
	}
END_SHADER

