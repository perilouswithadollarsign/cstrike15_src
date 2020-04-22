//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//

#include "BaseVSShader.h"
#include "prototype_helper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DEFINE_FALLBACK_SHADER( Prototype, Prototype_dx9 )
BEGIN_VS_SHADER( Prototype_dx9, "Prototype" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( BUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "", "Normal map" )
		SHADER_PARAM( BUMPFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "Frame number for $bumpmap" )
	END_SHADER_PARAMS

	void SetupVarsPrototype( PrototypeVars_t &info )
	{
		info.m_nBaseTexture = BASETEXTURE;
		info.m_nBaseTextureFrame = FRAME;
 
		info.m_nBumpmap = BUMPMAP;
		info.m_nBumpFrame = BUMPFRAME;
	}

	SHADER_INIT_PARAMS()
	{
		PrototypeVars_t info;
		SetupVarsPrototype( info );
		InitParamsPrototype( this, params, pMaterialName, info );
	}

	SHADER_FALLBACK
	{
		if ( ( g_pHardwareConfig->GetDXSupportLevel() < 90 ) || ( !g_pHardwareConfig->SupportsPixelShaders_2_b() ) )
		{
			return "Wireframe";
		}

		return 0;
	}

	SHADER_INIT
	{
		PrototypeVars_t info;
		SetupVarsPrototype( info );
		InitPrototype( this, params, info );
	}

	SHADER_DRAW
	{
		PrototypeVars_t info;
		SetupVarsPrototype( info );
		DrawPrototype( this, params, pShaderAPI, pShaderShadow, info, vertexCompression );
	}
END_SHADER
