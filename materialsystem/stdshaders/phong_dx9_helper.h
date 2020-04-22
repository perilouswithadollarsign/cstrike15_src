//========= Copyright (c) 1996-2008, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef PHONG_DX9_HELPER_H
#define PHONG_DX9_HELPER_H

#include <string.h>

#include "vertexlitgeneric_dx9_helper.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CBaseVSShader;
class IMaterialVar;
class IShaderDynamicAPI;
class IShaderShadow;

void InitParamsPhong_DX9( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, VertexLitGeneric_DX9_Vars_t &info );
void InitPhong_DX9( CBaseVSShader *pShader, IMaterialVar** params, VertexLitGeneric_DX9_Vars_t &info );
void DrawPhong_DX9( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI, IShaderShadow* pShaderShadow,
				  VertexLitGeneric_DX9_Vars_t &info, VertexCompressionType_t vertexCompression, CBasePerMaterialContextData **pContextDataPtr );
void DrawPhong_DX9_ExecuteFastPath( int *vsDynIndex, int *psDynIndex,
								   CBaseVSShader *pShader,	IMaterialVar** params, IShaderDynamicAPI * pShaderAPI,
								   VertexLitGeneric_DX9_Vars_t &info, 
								   VertexCompressionType_t vertexCompression,
								   CBasePerMaterialContextData **pContextDataPtr, BOOL bCSMEnabled );

inline void ClampDetailBlendModeAndWarn( int &nDetailBlendMode, int nMin, int nMax )
{
	if ( nDetailBlendMode < nMin || nDetailBlendMode > nMax )
	{
		Warning( "========================================================================\n" );
		Warning( "========================================================================\n" );
		Warning( "========================================================================\n" );
		Warning( "Material uses an out of range $detailblendmode of %d. Should be in [%d,%d].\nGive a programmer a repro case, or look at your modified vmt files\n", nDetailBlendMode, nMin, nMax );
		Warning( "========================================================================\n" );
		Warning( "========================================================================\n" );
		Warning( "========================================================================\n" );
		nDetailBlendMode = 0;
	}
}

inline void ClampDecalBlendModeAndWarn( int &nDecalBlendMode, int nMin, int nMax )
{
	if ( nDecalBlendMode < nMin || nDecalBlendMode > nMax )
	{
		Warning( "========================================================================\n" );
		Warning( "========================================================================\n" );
		Warning( "========================================================================\n" );
		Warning( "Material uses an out of range $decalblendmode of %d. Should be in [%d,%d].\nGive a programmer a repro case, or look at your modified vmt files\n", nDecalBlendMode, nMin, nMax );
		Warning( "========================================================================\n" );
		Warning( "========================================================================\n" );
		Warning( "========================================================================\n" );
		nDecalBlendMode = 0;
	}
}

#endif // PHONG_DX9_HELPER_H
