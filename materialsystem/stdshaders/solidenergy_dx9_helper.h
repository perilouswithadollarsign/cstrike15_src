//========= Copyright © Valve Corporation, All rights reserved. ============//

#ifndef SOLIDENERGY_HELPER_H
#define SOLIDENERGY_HELPER_H
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
// Struct to hold shader param indices
//-----------------------------------------------------------------------------
struct SolidEnergyVars_t
{
	SolidEnergyVars_t()
	{
		memset( this, 0xFF, sizeof( SolidEnergyVars_t ) );
	}

	int m_nBaseTexture;
	int m_nBaseTextureTransform;
	int m_nDetail1Texture;
	int m_nDetail1Scale;
	int m_nDetail1Frame;
	int m_nDetail1BlendMode;
	int m_nDetail1TextureTransform;
	int m_nDetail2Texture;
	int m_nDetail2Scale;
	int m_nDetail2Frame;
	int m_nDetail2BlendMode;
	int m_nDetail2TextureTransform;
	int m_nTangentTOpacityRanges;
	int m_nTangentSOpacityRanges;
	int m_nFresnelOpacityRanges;
	int m_nNeedsTangentT;
	int m_nNeedsTangentS;
	int m_nNeedsNormals;
	int m_nDepthBlend;
	int m_nDepthBlendScale;
	int m_nFlowMap;
	int m_nFlowMapFrame;
	int m_nFlowMapScrollRate;
	int m_nFlowNoiseTexture;
	int m_nTime;
	int m_nFlowWorldUVScale;
	int m_nFlowNormalUVScale;
	int m_nFlowTimeIntervalInSeconds;
	int m_nFlowUVScrollDistance;
	int m_nFlowNoiseScale;
	int m_nFlowLerpExp;
	int m_nFlowBoundsTexture;
	int m_nPowerUp;
	int m_nFlowColorIntensity;
	int m_nFlowColor;
	int m_nFlowVortexColor;
	int m_nFlowVortexSize;
	int m_nFlowVortex1;
	int m_nFlowVortexPos1;
	int m_nFlowVortex2;
	int m_nFlowVortexPos2;
	int m_nFlowCheap;
	int m_nModel;
	int m_nOutputIntensity;
};

// default shader param values
static const float kDefaultDetailScale = 1.0f;
static const int   kDefaultDetailFrame = 0;
static const int   kDefaultDetailBlendMode = 0;
static const int   kMaxDetailBlendMode = 1;
static const float kDefaultFalloffRanges[4] = { 1.0f, 0.9f, 0.0f, 0.7f };
static const float kDefaultDepthBlendScale = 50.0;
static const float kDefaultTimescale = 0.4f;
static const float kDefaultScrollDist = 0.2f;
static const float kDefaultNoiseScale = 0.0002f;
static const float kDefaultPowerUpIntensity = 1.0f;
static const float kDefaultVortexSize = 30.0f;
static const float kDefaultFieldColor[3] = { 0.1f, 0.2f, 0.4f };
static const float kDefaultVortexColor[3] = { 1.2f, 0.4f, 0.0f };
static const float kDefaultIntensity = 1.0f;

void InitParamsSolidEnergy( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, SolidEnergyVars_t &info );
void InitSolidEnergy( CBaseVSShader *pShader, IMaterialVar** params, SolidEnergyVars_t &info );
void DrawSolidEnergy( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
					IShaderShadow* pShaderShadow, SolidEnergyVars_t &info, VertexCompressionType_t vertexCompression,
					CBasePerMaterialContextData **pContextDataPtr );

#endif // SOLIDENERGY_HELPER_H