//====== Copyright © 1996-2006, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef PROJECTED_DX9_HELPER_H
#define PROJECTED_DX9_HELPER_H
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
struct Projected_DX9_Vars_t
{
	Projected_DX9_Vars_t() { memset( this, 0xFF, sizeof( *this ) ); }

	int m_nBaseTextureTransform;
	int m_nBaseTexture;
};

void InitParamsProjected_DX9( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, 
						   Projected_DX9_Vars_t &info );
void InitProjected_DX9( CBaseVSShader *pShader, IMaterialVar** params, Projected_DX9_Vars_t &info );
void DrawProjected_DX9( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
				   IShaderShadow* pShaderShadow, Projected_DX9_Vars_t &info, VertexCompressionType_t vertexCompression );

#endif // PROJECTED_DX9_HELPER_H
