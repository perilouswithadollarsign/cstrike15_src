//========= Copyright (c) 1996-2006, Valve Corporation, All rights reserved. ============//

#ifndef PROTOTYPE_HELPER_H
#define PROTOTYPE_HELPER_H
#ifdef _WIN32
#pragma once
#endif

#include <string.h>

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CBaseVSShader;
class IMaterialVar;
class IShaderDynamicAPI;
class IShaderShadow;

//-----------------------------------------------------------------------------
// Init params/ init/ draw methods
//-----------------------------------------------------------------------------
struct PrototypeVars_t
{
	PrototypeVars_t() { memset( this, 0xFF, sizeof(PrototypeVars_t) ); }

	int m_nBaseTexture;
	int m_nBaseTextureFrame;

	int m_nBumpmap;
	int m_nBumpFrame;
};

void InitParamsPrototype( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, PrototypeVars_t &info );
void InitPrototype( CBaseVSShader *pShader, IMaterialVar** params, PrototypeVars_t &info );
void DrawPrototype( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
					IShaderShadow* pShaderShadow, PrototypeVars_t &info, VertexCompressionType_t vertexCompression );

#endif // PROTOTYPE_HELPER_H
