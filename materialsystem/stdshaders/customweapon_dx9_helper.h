//========= Copyright © Valve Corporation, All rights reserved. ============//

#ifndef CUSTOMWEAPON_HELPER_H
#define CUSTOMWEAPON_HELPER_H
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
struct CustomWeaponVars_t
{
	CustomWeaponVars_t()
	{
		memset( this, 0xFF, sizeof( CustomWeaponVars_t ) );
	}

	int m_nBaseTexture;
	int m_nBaseTextureTransform;
	int m_nSurfaceTexture;
	int m_nAOTexture;
	int m_nMasksTexture;
	int m_nPosTexture;
	int m_nGrungeTexture;
	int m_nWearTexture;
	int m_nPaintTexture;
	int m_nExpTexture;

	int m_nCamoColor0;
	int m_nCamoColor1;
	int m_nCamoColor2;
	int m_nCamoColor3;
	int m_nBaseDiffuseOverride;
	int m_nPhongExponent;
	int m_nPhongIntensity;
	int m_nWearProgress;
	int m_nPhongAlbedoBoost;
	int m_nPaintStyle;
	int m_nExponentMode;

	int m_nPatternTextureTransform;
	int m_nWearTextureTransform;
	int m_nGrungeTextureTransform;

	int m_nCheap;
	int m_nPhongAlbedoFactor;

	int m_nPreview;
	int m_nPreviewPhongFresnelRanges;
	int m_nPreviewPhongAlbedoTint;
	int m_nPreviewPhongBoost;
	int m_nPreviewPaintPhongAlbedoBoost;
	int m_nPreviewEnvMap;

	int m_nPreviewWeaponObjScale;
	int m_nPreviewWeaponUVScale;
	int m_nPreviewIgnoreWeaponScale;
};

// default shader param values
static const float kDefaultCamoColor0[3] = { 0.0f, 0.0f, 0.0f };
static const float kDefaultCamoColor1[3] = { 0.0f, 0.0f, 0.0f };
static const float kDefaultCamoColor2[3] = { 0.0f, 0.0f, 0.0f };
static const float kDefaultCamoColor3[3] = { 0.0f, 0.0f, 0.0f };
static const float kDefaultPreviewPhongFresnelRanges[3] = { 1.0f, 1.0f, 1.0f };

static const int   kDefaultBaseDiffuseOverride = 0;
static const int   kDefaultPhongExponent = 4;
static const int   kDefaultPhongIntensity = 10;
static const float   kDefaultWearProgress = 0.45f;
static const float   kDefaultPhongAlbedoBoost = 1.0f;
static const int   kDefaultPaintStyle = 0;
static const int   kDefaultExponentMode = 0;
static const float   kDefaultPatternScale = 0.5f;
static const float   kDefaultPatternOffset = 0.34f;
static const float   kDefaultWearOffset = 0.16f;
static const float   kDefaultPhongAlbedoFactor = 1.0f;
static const float   kDefaultPreviewPhongBoost = -1.0f;
static const float   kDefaultWeaponScale = 1.0f;

void InitParamsCustomWeapon( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, CustomWeaponVars_t &info );

void InitCustomWeapon( CBaseVSShader *pShader, IMaterialVar** params, CustomWeaponVars_t &info );

void DrawCustomWeapon( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
	IShaderShadow* pShaderShadow, CustomWeaponVars_t &info, VertexCompressionType_t vertexCompression,
	CBasePerMaterialContextData **pContextDataPtr );

#endif // CUSTOMWEAPON_HELPER_H