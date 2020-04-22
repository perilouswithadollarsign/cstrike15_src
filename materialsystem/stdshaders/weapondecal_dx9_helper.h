//========= Copyright © Valve Corporation, All rights reserved. ============//

#ifndef WEAPONDECAL_HELPER_H
#define WEAPONDECAL_HELPER_H
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
struct WeaponDecalVars_t
{
	WeaponDecalVars_t()
	{
		memset( this, 0xFF, sizeof( WeaponDecalVars_t ) );
	}

	int m_nBaseTexture;
	int m_nBaseTextureTransform;
	int m_nAOTexture;
	int m_nNormalMap;
	int m_nWearProgress;
	int m_nWearRemapMin;
	int m_nWearRemapMid;
	int m_nWearRemapMax;
	int m_nWearWidthMin;
	int m_nWearWidthMax;
	int m_nUnWearStrength;
	int m_nExpTexture;
	int m_nHologramSpectrum;
	int m_nHologramMask;
	int m_nGrungeTexture;
	int m_nWearTexture;
	int m_nAnisoDirTexture;
	int m_nPhong;
	int m_nPhongExponent;
	int m_nPhongFresnelRanges;
	int m_nPhongAlbedoTint;
	int m_nPhongBoost;
	int m_nPhongAlbedoBoost;
	int m_nEnvmapTexture;
	int m_nEnvmapTint;
	int m_nDecalStyle;
	int m_nColorTint;
	int m_nColorTint2;
	int m_nColorTint3;
	int m_nColorTint4;
	int m_nAlpha;
	//int m_nPatternTextureTransform;
	int m_nPatternRotation;
	int m_nPatternScale;
	int m_nMirrorHorizontal;
	int m_nThirdPerson;
	int m_nHighlight;
	int m_nHighlightCycle;
	int m_nPeel;
	int m_nWearBias;
	int m_nAlphaMask;
	//int m_nFastWearThreshold;
	int m_nGrungeScale;
	int m_nDesatBaseTint;
};

// default shader param values
static const float   kDefaultWearProgress			= 0.0f;
static const float   kDefaultWearRemapMin			= 0.8f;
static const float   kDefaultWearRemapMid			= 0.75f;
static const float   kDefaultWearRemapMax			= 1.0f;
static const float   kDefaultWearWidthMin			= 0.06f;
static const float   kDefaultWearWidthMax			= 0.12f;
static const float   kDefaultUnWearStrength			= 0.2f;
static const int	 kDefaultPhong					= 0;
static const int	 kDefaultPhongExponent			= 4;
static const float   kDefaultPhongFresnelRanges[3]	= { 1.0f, 1.0f, 1.0f };
static const float   kDefaultPhongAlbedoTint		= 0.0f;
static const float   kDefaultPhongBoost				= 1.0f;
static const float   kDefaultPhongAlbedoBoost		= 1.0f;
static const float   kDefaultEnvmapTint[3]			= { 1.0f, 1.0f, 1.0f };
static const int	 kDefaultDecalStyle				= 1;
static const float   kDefaultColorTint[3]			= { 255.0f, 255.0f, 255.0f };
static const float   kDefaultColorTint2[3]			= { 0.0f, 0.0f, 0.0f };
static const float   kDefaultColorTint3[3]			= { 0.0f, 0.0f, 0.0f };
static const float   kDefaultColorTint4[3]			= { 0.0f, 0.0f, 0.0f };
static const int	 kDefaultAlpha					= 1;
static const float	 kDefaultPatternRotation		= 0.0f;
static const float	 kDefaultPatternScale			= 1.0f;
static const int	 kDefaultMirrorHorizontal		= 0;
static const int	 kDefaultThirdPerson			= 0;
static const char	*kDefaultBaseTexture			= "models/weapons/customization/stickers/default/sticker_default";
static const char	*kDefaultAOTexture				= "models/weapons/customization/stickers/default/ao_default";
static const char	*kDefaultGrungeTexture			= "models/weapons/customization/shared/sticker_paper";
static const char	*kDefaultWearTexture			= "models/weapons/customization/shared/paint_wear";
static const float	 kDefaultHighlight				= 0.0f;
static const float	 kDefaultHighlightCycle			= 0.0f;
static const float	 kDefaultPeel					= 0.0f;
static const float   kDefaultWearBias				= 0.0f;
static const int	 kDefaultAlphaMask				= 0;
//static const float   kDefaultFastWearThreshold		= 0.0f;
static const float	 kDefaultGrungeScale			= 2.0f;
static const float	 kDefaultDesatBaseTint			= 0.0f;

void InitParamsWeaponDecal( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, WeaponDecalVars_t &info );

void InitWeaponDecal( CBaseVSShader *pShader, IMaterialVar** params, WeaponDecalVars_t &info );

void DrawWeaponDecal( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
	IShaderShadow* pShaderShadow, WeaponDecalVars_t &info, VertexCompressionType_t vertexCompression,
	CBasePerMaterialContextData **pContextDataPtr );

#endif // WEAPONDECAL_HELPER_H