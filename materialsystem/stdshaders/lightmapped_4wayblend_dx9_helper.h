//========= Copyright (c) 1996-2014, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef LIGHTMAPPED_4WAYBLEND_DX9_HELPER_H
#define LIGHTMAPPED_4WAYBLEND_DX9_HELPER_H

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
struct Lightmapped_4WayBlend_DX9_Vars_t
{
	Lightmapped_4WayBlend_DX9_Vars_t() { memset( this, 0xFF, sizeof(Lightmapped_4WayBlend_DX9_Vars_t) ); }

	int m_nBaseTexture;
	int m_nBaseTextureFrame;
	int m_nBaseTextureTransform;
	int m_nSelfIllumTint;

	int m_nDetail;
	int m_nDetailFrame;
	int m_nDetailScale;
	int m_nDetailTextureCombineMode;
	int m_nDetailTextureBlendFactor;
	int m_nDetailTextureBlendFactor2;
	int m_nDetailTextureBlendFactor3;
	int m_nDetailTextureBlendFactor4;
	int m_nDetailTint;

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
	int m_nTexture1LumStart;
	int m_nTexture1LumEnd;
	int m_nBumpmap2;
	int m_nBumpFrame2;
	int m_nBumpTransform2;
	int m_nBaseTexture2;
	int m_nBaseTexture2Frame;
	int m_nTexture2uvScale;
	int m_nTexture2BlendStart;
	int m_nTexture2BlendEnd;
	int m_nTexture2LumStart;
	int m_nTexture2LumEnd;
	int m_nTexture2BumpBlendFactor;
	int m_nBaseTexture3;
	int m_nTexture3BlendMode;
	int m_nBaseTexture3Frame;
	int m_nTexture3uvScale;
	int m_nTexture3BlendStart;
	int m_nTexture3BlendEnd;
	int m_nTexture3LumStart;
	int m_nTexture3LumEnd;
	int m_nTexture3BumpBlendFactor;
	int m_nBaseTexture4;
	int m_nTexture4BlendMode;
	int m_nBaseTexture4Frame;
	int m_nTexture4uvScale;
	int m_nTexture4BlendStart;
	int m_nTexture4BlendEnd;
	int m_nTexture4LumStart;
	int m_nTexture4LumEnd;
	int m_nTexture4BumpBlendFactor;
	int m_nLumBlendFactor2;
	int m_nLumBlendFactor3;
	int m_nLumBlendFactor4;
	int m_nFlashlightTexture;
	int m_nFlashlightTextureFrame;
	int m_nSelfShadowedBumpFlag;
	int m_nForceBumpEnable;
	int m_nSeamlessMappingScale;
	int m_nAlphaTestReference;
	int m_nEnvmapAnisotropy;
	int m_nEnvmapAnisotropyScale;

	int m_nShaderSrgbRead360;

	int m_nEnvMapLightScale;
	int m_nEnvMapLightScaleMinMax;

	float m_uvScaleDefault;
};

class CLightmapped_4WayBlend_DX9_Context : public CBasePerMaterialContextData
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

	CLightmapped_4WayBlend_DX9_Context( void )
	{
		m_pStaticCmds = NULL;
	}

	~CLightmapped_4WayBlend_DX9_Context( void )
	{
		ResetStaticCmds();
	}
};

void InitParamsLightmapped_4WayBlend_DX9( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, Lightmapped_4WayBlend_DX9_Vars_t &info );
void InitLightmapped_4WayBlend_DX9( CBaseVSShader *pShader, IMaterialVar** params, Lightmapped_4WayBlend_DX9_Vars_t &info );
void DrawLightmapped_4WayBlend_DX9( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI, IShaderShadow* pShaderShadow, 
									Lightmapped_4WayBlend_DX9_Vars_t &info, CBasePerMaterialContextData **pContextDataPtr	 );

void DrawLightmapped_4WayBlend_DX9_FastPath( int *dynVSIdx, int *dynPSIdx, CBaseVSShader *pShader, IMaterialVar** params, 
											 IShaderDynamicAPI *pShaderAPI, Lightmapped_4WayBlend_DX9_Vars_t &info, CBasePerMaterialContextData **pContextDataPtr, BOOL bCSMEnabled );

#endif // MULTIBLEND_DX9_HELPER_H
