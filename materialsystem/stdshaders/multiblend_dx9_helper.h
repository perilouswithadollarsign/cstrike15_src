//====== Copyright © 1996-2006, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef MULTIBLEND_DX9_HELPER_H
#define MULTIBLEND_DX9_HELPER_H
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
struct Multiblend_DX9_Vars_t
{
	Multiblend_DX9_Vars_t() { memset( this, 0xFF, sizeof( *this ) ); }

	int			m_nBaseTextureTransform;
	int			m_nBaseTexture;
	int			m_nSpecTexture;
	int			m_nBaseTexture2;
	int			m_nSpecTexture2;
	int			m_nBaseTexture3;
	int			m_nSpecTexture3;
	int			m_nBaseTexture4;
	int			m_nSpecTexture4;
	int 		m_nRotation;
	int 		m_nRotation2;
	int 		m_nRotation3;
	int 		m_nRotation4;
	int			m_nScale;
	int			m_nScale2;
	int			m_nScale3;
	int			m_nScale4;
};

void InitParamsMultiblend_DX9( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, 
						   Multiblend_DX9_Vars_t &info );
void InitMultiblend_DX9( CBaseVSShader *pShader, IMaterialVar** params, Multiblend_DX9_Vars_t &info );
void DrawMultiblend_DX9( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
				   IShaderShadow* pShaderShadow, Multiblend_DX9_Vars_t &info, VertexCompressionType_t vertexCompression );

#endif // MULTIBLEND_DX9_HELPER_H
