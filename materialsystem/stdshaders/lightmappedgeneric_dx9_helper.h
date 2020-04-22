//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef LIGHTMAPPEDGENERIC_DX9_HELPER_H
#define LIGHTMAPPEDGENERIC_DX9_HELPER_H

#include <string.h>
#include "BaseVSShader.h"
#include "shaderlib/commandbuilder.h"


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
struct LightmappedGeneric_DX9_Vars_t
{
	LightmappedGeneric_DX9_Vars_t() { memset( this, 0xFF, sizeof(LightmappedGeneric_DX9_Vars_t) ); }

	int m_nBaseTexture;
	int m_nBaseTextureFrame;
	int m_nBaseTextureTransform;
	int m_nSelfIllumTint;

	int m_nDetail;
	int m_nDetailFrame;
	int m_nDetailScale;
	int m_nDetailTextureCombineMode;
	int m_nDetailTextureBlendFactor;
	int m_nDetailTint;

	int m_nDetail2;
	int m_nDetailFrame2;
	int m_nDetailScale2;
	int m_nDetailTextureBlendFactor2;
	int m_nDetailTint2;

	int m_nEnvmap;
	int m_nEnvmapFrame;
	int m_nEnvmapMask;
	int m_nEnvmapMaskFrame;
	int m_nEnvmapMaskTransform;
	int m_nEnvmapTint;
	int m_nBumpmap;
	int m_nBumpFrame;
	int m_nBumpTransform;
	int m_nEnvmapContrast;
	int m_nEnvmapSaturation;
	int m_nFresnelReflection;
	int m_nNoDiffuseBumpLighting;
	int m_nBumpmap2;
	int m_nBumpFrame2;
	int m_nBumpTransform2;
	int m_nBumpMask;
	int m_nBaseTexture2;
	int m_nBaseTextureFrame2;
	int m_nBaseTextureTransform2;
	int m_nFlashlightTexture;
	int m_nFlashlightTextureFrame;
	int m_nBlendModulateTexture;
	int m_nSelfShadowedBumpFlag;
	int m_nForceBumpEnable;
	int m_nSeamlessMappingScale;
	int m_nAlphaTestReference;
	int m_nEnvmapAnisotropy;
	int m_nEnvmapAnisotropyScale;
	int m_nNoEnvmapMip;
	int m_nAddBumpMaps;
	int m_nBumpDetailScale1;
	int m_nBumpDetailScale2;

	int m_nShaderSrgbRead360;

	int m_nEnvMapLightScale;
	int m_nEnvMapLightScaleMinMax;

	int m_nPaintSplatNormal;
	int m_nPaintSplatBubbleLayout;
	int m_nPaintSplatBubble;
	int m_nPaintEnvmap;

	int m_nPhong;
	int m_nPhongExp;
	int m_nPhongExp2;
	int m_nPhongBaseTint;
	int m_nPhongBaseTint2;
	int m_nPhongMaskContrastBrightness;
	int m_nPhongMaskContrastBrightness2;
	int m_nPhongAmount;
	int m_nPhongAmount2;

	int m_nDropShadowOpacity;
	int m_nDropShadowScale;
	int m_nDropShadowHighlightScale;
	int m_nDropShadowDepthExaggeration;

	int m_nLayerTint1;
	int m_nLayerTint2;

	int m_nBlendModulateTransform;

	int m_nEnvmapMask2;
	int m_nEnvmapMaskFrame2;
	int m_nEnvmapMaskTransform2;

	int m_nAltLayerBlending;
	int m_nBlendSoftness;
	int m_nLayerBorderStrength;
	int m_nLayerBorderOffset;
	int m_nLayerBorderSoftness;
	int m_nLayerBorderTint;

	int m_nLayerEdgePunchIn;
	int m_nLayerEdgeStrength;
	int m_nLayerEdgeOffset;
	int m_nLayerEdgeSoftness;
	int m_nLayerEdgeNormal;
};

class CLightmappedGeneric_DX9_Context : public CBasePerMaterialContextData
{
public:
	uint8 *m_pStaticCmds;
	CCommandBufferBuilder< CFixedCommandStorageBuffer< 900 > > m_SemiStaticCmdsOut;
#ifdef _PS3
	CCommandBufferBuilder< CFixedCommandStorageBuffer< 256 > > m_flashlightECB;
#endif

	bool m_bVertexShaderFastPath;
	bool m_bPixelShaderFastPath;
	bool m_bFullyOpaque;
	bool m_bFullyOpaqueWithoutAlphaTest;

	void ResetStaticCmds( void )
	{
		if ( m_pStaticCmds )
		{
			delete[] m_pStaticCmds;
			m_pStaticCmds = NULL;
		}
	}

	CLightmappedGeneric_DX9_Context( void )
	{
		m_pStaticCmds = NULL;
	}

	~CLightmappedGeneric_DX9_Context( void )
	{
		ResetStaticCmds();
	}
};

void InitParamsLightmappedGeneric_DX9( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, LightmappedGeneric_DX9_Vars_t &info );
void InitLightmappedGeneric_DX9( CBaseVSShader *pShader, IMaterialVar** params, LightmappedGeneric_DX9_Vars_t &info );
void DrawLightmappedGeneric_DX9( CBaseVSShader *pShader, IMaterialVar** params, 
								 IShaderDynamicAPI *pShaderAPI, IShaderShadow* pShaderShadow, 
								 LightmappedGeneric_DX9_Vars_t &info, CBasePerMaterialContextData **pContextDataPtr	 );

void DrawLightmappedGeneric_DX9_FastPath( int *dynVSIdx, int *dynPSIdx, CBaseVSShader *pShader, IMaterialVar** params, 
										 IShaderDynamicAPI *pShaderAPI, 
										 LightmappedGeneric_DX9_Vars_t &info, CBasePerMaterialContextData **pContextDataPtr, BOOL bCSMEnabled );

#endif // LIGHTMAPPEDGENERIC_DX9_HELPER_H
