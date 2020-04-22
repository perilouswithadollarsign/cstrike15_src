//========= Copyright © 1996-2007, Valve Corporation, All rights reserved. ============//

#ifndef BLOB_HELPER_H
#define BLOB_HELPER_H
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
struct BlobVars_t
{
	BlobVars_t()
	{
		memset( this, 0xFF, sizeof( BlobVars_t ) );
	}

	int m_nBackSurface;
	int m_nBumpStrength;
	int m_nFresnelBumpStrength;
	int m_nNormalMap;
	int m_nBaseTexture;
	int m_nLightWarpTexture;
	int m_nFresnelWarpTexture;
	int m_nOpacityTexture;
	int m_nSpecMap;
	int m_nUVScale;

	int m_nInteriorEnable;
	int m_nInteriorFogStrength;
	int m_nInteriorFogLimit;
	int m_nInteriorFogNormalBoost;
	int m_nInteriorBackgroundBoost;
	int m_nInteriorAmbientScale;
	int m_nInteriorBackLightScale;
	int m_nInteriorColor;
	int m_nInteriorRefractStrength;
	int m_nInteriorRefractBlur;

	int m_nFresnelParams;
	int m_nDiffuseScale;
	int m_nSpecExp;
	int m_nSpecScale;
	int m_nSpecExp2;
	int m_nSpecScale2;
	int m_nRimLightExp;
	int m_nRimLightScale;
	int m_nBaseColorTint;
	int m_nSelfIllumFresnelEnable;
	int m_nSelfIllumFresnelParams;
	int m_nSelfIllumTint;
	int m_nUVProjOffset;
	int m_nBBMin;
	int m_nBBMax;
	int m_nArmature;
	int m_nFlashlightTexture;
	int m_nFlashlightTextureFrame;
	int m_nArmColorTint;
	int m_nArmWiden;
	int m_nArmWidthExp;
	int m_nArmWidthScale;
	int m_nArmWidthBias;
	int m_nAnimateArmPulses;
	int m_nVolumeTex;
	int m_nBumpFrame;
	int m_nGlowScale;
	int m_nPulse;
	int m_nContactShadows;
};

// default shader param values
static const int   kDefaultBackSurface = 0;
static const float kDefaultBumpStrength = 1.0f;
static const float kDefaultFresnelBumpStrength = 1.0f;
static const float kDefaultUVScale = 0.02f;

static const int   kDefaultInteriorEnable = 1;
static const float kDefaultInteriorFogStrength = 0.06f;
static const float kDefaultInteriorFogLimit = 0.8f;
static const float kDefaultInteriorFogNormalBoost = 0.0f;
static const float kDefaultInteriorBackgroundBoost = 0.0f;
static const float kDefaultInteriorAmbientScale = 0.3f;
static const float kDefaultInteriorBackLightScale = 0.3f;
static const float kDefaultInteriorColor[3] = { 0.5f, 0.5f, 0.5f };
static const float kDefaultInteriorRefractStrength = 0.015f;
static const float kDefaultInteriorRefractBlur = 0.2f;

static const float kDefaultFresnelParams[3] = { 0.0f, 0.5f, 2.0f };
static const float kDefaultBaseColorTint[3] = { 1.0f, 1.0f, 1.0f };
static const float kDefaultDiffuseScale = 1.0f;
static const float kDefaultSpecExp = 1.0f;
static const float kDefaultSpecScale = 1.0f;
static const float kDefaultRimLightExp = 10.0f;
static const float kDefaultRimLightScale = 1.0f;
static const int   kDefaultSelfIllumFresnelEnable = 0;
static const float kDefaultSelfIllumFresnelParams[3] = { 0.0f, 1.0f, 1.0f };
static const float kDefaultSelfIllumTint[3] = { 1.0f, 1.0f, 1.0f };
static const float kDefaultUVProjOffset[3] = { 0.0f, 0.0f, 0.0f };
static const float kDefaultBB[3] = { 0.0f, 0.0f, 0.0f };

static const int   kDefaultArmature = 0;
static const float kDefaultArmColorTint[3] = { 1.0f, 1.0f, 1.0f };
static const int   kDefaultArmWiden = 0;
static const float kDefaultArmWidthExp = 1.0f;
static const float kDefaultArmWidthScale = 1.0f;
static const float kDefaultArmWidthBias = 0.0f;
static const int   kDefaultAnimateArmPulses = 1;

static const int   kDefaultVolumeTex = 0;
static const int   kDefaultBumpFrame = 0;
static const float kDefaultGlowScale = 1.0f;
static const int   kDefaultPulse = 0;
static const int   kDefaultContactShadows = 0;

void InitParamsBlob( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, BlobVars_t &info );
void InitBlob(	CBaseVSShader *pShader, IMaterialVar** params, BlobVars_t &info );
void DrawBlob(	CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
				IShaderShadow* pShaderShadow, BlobVars_t &info, VertexCompressionType_t vertexCompression );
void DrawArmature(	CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
					IShaderShadow* pShaderShadow, BlobVars_t &info, VertexCompressionType_t vertexCompression, int nPassIdx );

#endif // BLOB_HELPER_H
