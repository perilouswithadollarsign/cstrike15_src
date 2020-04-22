//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//

#include "BaseVSShader.h"
#include "proto_ice_helper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DEFINE_FALLBACK_SHADER( ProtoIce, ProtoIce_dx9 )
BEGIN_VS_SHADER( ProtoIce_dx9, "ProtoIce" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( BUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "", "Normal map" )
		SHADER_PARAM( BUMPFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "Frame number for $bumpmap" )

		SHADER_PARAM( SSBUMP, SHADER_PARAM_TYPE_INTEGER, "0", "whether or not to use alternate bumpmap format with height" )

	END_SHADER_PARAMS

	void SetupVarsProtoIce( ProtoIceVars_t &info )
	{
		info.m_nBaseTexture = BASETEXTURE;
		info.m_nBaseTextureFrame = FRAME;
 
		info.m_nBumpmap = BUMPMAP;
		info.m_nBumpFrame = BUMPFRAME;

		info.m_nSsBump = SSBUMP;
	}

	SHADER_INIT_PARAMS()
	{
		ProtoIceVars_t info;
		SetupVarsProtoIce( info );
		InitParamsProtoIce( this, params, pMaterialName, info );
	}

	SHADER_FALLBACK
	{
		if ( ( g_pHardwareConfig->GetDXSupportLevel() < 90 ) || ( !g_pHardwareConfig->SupportsPixelShaders_2_b() ) )
		{
			return "Wireframe";
		}

		if ( g_pConfig && g_pConfig->bEditMode )
		{
			return "VertexLitGeneric";
		}

		return 0;
	}

	SHADER_INIT
	{
		ProtoIceVars_t info;
		SetupVarsProtoIce( info );
		InitProtoIce( this, params, info );
	}

	SHADER_DRAW
	{
		ProtoIceVars_t info;
		SetupVarsProtoIce( info );
		DrawProtoIce( this, params, pShaderAPI, pShaderShadow, info, vertexCompression );
	}
END_SHADER
