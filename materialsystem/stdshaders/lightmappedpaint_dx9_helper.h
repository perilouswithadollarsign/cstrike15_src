//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef LIGHTMAPPEDPAINT_DX9_HELPER_H
#define LIGHTMAPPEDPAINT_DX9_HELPER_H

#include "lightmappedgeneric_dx9_helper.h"

void DrawLightmappedPaint_DX9( CBaseVSShader *pShader, IMaterialVar** params, 
								 IShaderDynamicAPI *pShaderAPI, IShaderShadow* pShaderShadow, 
								 LightmappedGeneric_DX9_Vars_t &info, CBasePerMaterialContextData **pContextDataPtr );


void DrawLightmappedPaint_DX9_FastPath( CBaseVSShader *pShader, IMaterialVar** params, 
							  IShaderDynamicAPI *pShaderAPI, LightmappedGeneric_DX9_Vars_t &info, CBasePerMaterialContextData **pContextDataPtr );

#endif // LIGHTMAPPEDPAINT_DX9_HELPER_H
