//========= Copyright (c) 1996-2007, Valve Corporation, All rights reserved. ============//

#ifndef LIGHT_SHAFTS_HELPER_H
#define LIGHT_SHAFTS_HELPER_H
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
struct LightShaftsVars_t
{
	LightShaftsVars_t() { memset( this, 0xFF, sizeof( LightShaftsVars_t ) ); }

	int m_nShadowDepthTexture;

	int m_nNoiseTexture;
	int m_nNoiseStrength;

	int m_nCookieTexture;
	int m_nCookieFrameNum;

	int m_nWorldToTexture;
	int m_nFlashlightColor;
	int m_nAttenFactors;
	int m_nOriginFarZ;
	int m_nQuatOrientation;
	int m_nShadowFilterSize;
	int m_nShadowAtten;
	int m_nShadowJitterSeed;
	int m_nUberlight;
	int m_nEnableShadows;
	int m_nFlashlightTime;
	int m_nNumPlanes;

	// Uberlight parameters
	int m_nUberNearFar;
	int m_nUberHeightWidth;
	int m_nUberRoundness;

	int m_nTime;

	int m_nVolumetricIntensity;
};

void InitParamsLightShafts( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, LightShaftsVars_t &info );
void InitLightShafts( CBaseVSShader *pShader, IMaterialVar** params, LightShaftsVars_t &info );
void DrawLightShafts( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
					  IShaderShadow* pShaderShadow, LightShaftsVars_t &info, VertexCompressionType_t vertexCompression );

#endif // LIGHT_SHAFTS_HELPER_H
