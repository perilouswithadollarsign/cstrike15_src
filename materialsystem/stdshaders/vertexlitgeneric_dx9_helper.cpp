//====== Copyright (c) 1996-2008, Valve Corporation, All rights reserved. =====//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#include "BaseVSShader.h"
#include "vertexlitgeneric_dx9_helper.h"
#include "phong_dx9_helper.h"

#include "vertexlit_and_unlit_generic_vs20.inc"
#include "vertexlit_and_unlit_generic_bump_vs20.inc"

#include "vertexlit_and_unlit_generic_ps20.inc"
#include "vertexlit_and_unlit_generic_ps20b.inc"
#include "vertexlit_and_unlit_generic_bump_ps20.inc"
#include "vertexlit_and_unlit_generic_bump_ps20b.inc"

#if !defined( _GAMECONSOLE )
	#include "vertexlit_and_unlit_generic_vs30.inc"
	#include "vertexlit_and_unlit_generic_ps30.inc"
	#include "vertexlit_and_unlit_generic_bump_vs30.inc"
	#include "vertexlit_and_unlit_generic_bump_ps30.inc"
#endif

#include "shaderapifast.h"
#include "shaderlib/commandbuilder.h"
#include "convar.h"

#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar r_lightwarpidentity( "r_lightwarpidentity","0", FCVAR_CHEAT );
static ConVar mat_phong( "mat_phong", "1" );
static ConVar mat_displacementmap( "mat_displacementmap", "1", FCVAR_CHEAT );

extern ConVar lm_test;
static ConVar mat_force_vertexfog( "mat_force_vertexfog", "0", FCVAR_DEVELOPMENTONLY );

#if defined( CSTRIKE15 ) && defined( _X360 )
static ConVar r_shader_srgbread( "r_shader_srgbread", "1", 0, "1 = use shader srgb texture reads, 0 = use HW" );
#else
static ConVar r_shader_srgbread( "r_shader_srgbread", "0", 0, "1 = use shader srgb texture reads, 0 = use HW" );
#endif


// r_staticlight_streams
//	defines format of lighting for static props.
//	"1"	= lighting comes as 4 bytes per vertex and is lighting with regard to vertex normal
//	"3" = lighting comes as 12 bytes per vertex and is lighting with regard to hl2-basis
ConVar r_staticlight_streams( "r_staticlight_streams", "1", FCVAR_HIDDEN );

// r_staticlight_streams_indirect_only
//	defines format of lighting for static props using vertexlitgeneric_bump shader
//	"0"	= lighting stream colors have sunlight baked in
//	"1" = lighting stream colors store indirect only
//  Prequisite: 3 lighting streams enabled
ConVar r_staticlight_streams_indirect_only( "r_staticlight_streams_indirect_only", "-", FCVAR_HIDDEN );

static void r_staticlight_mode_changed( IConVar *var, const char *pOldValue, float flOldValue )
{
	g_pMaterialSystem->ReloadMaterials( NULL );
}
ConVar r_staticlight_mode( "r_staticlight_mode", "0", FCVAR_DEVELOPMENTONLY, "0 - support three color streams, 1 - use avg of three streams, 2 - single color stream", r_staticlight_mode_changed );


static inline bool WantsPhongShaderInternal( IMaterialVar** params, const VertexLitGeneric_DX9_Vars_t &info )
{
	if ( info.m_nPhong == -1)								// Don't use without Phong flag
		return false;

	if ( params[info.m_nPhong]->GetIntValue() == 0 )		// Don't use without Phong flag set to 1
		return false;

	if ( ( info.m_nDiffuseWarpTexture != -1 ) && params[info.m_nDiffuseWarpTexture]->IsTexture() ) // If there's Phong flag and diffuse warp do Phong
		return true;

	if ( ( info.m_nBaseMapAlphaPhongMask != -1 ) && params[info.m_nBaseMapAlphaPhongMask]->GetIntValue() != 1 )
	{
		if ( info.m_nBumpmap == -1 )						// Don't use without a bump map
			return false;

		if ( !params[info.m_nBumpmap]->IsDefined() )		// Don't use if the texture isn't specified
			return false;
	}

	return true;
}

static inline bool WantsPhongShader( IMaterialVar** params, const VertexLitGeneric_DX9_Vars_t &info )
{
	if ( !mat_phong.GetBool() )
	{
		// If mat_phong is disabled (because the GPU level is too low), check to see if the material uses Phong and wants to force Phong on anyway.
		if ( ( info.m_nForcePhong != -1 ) && ( params[ info.m_nForcePhong ]->GetIntValue() ) )
		{
			if ( WantsPhongShaderInternal( params, info ) )
			{
				return true;
			}
		}
		return false;
	}

	return WantsPhongShaderInternal( params, info );
}


//-----------------------------------------------------------------------------
// Initialize shader parameters
//-----------------------------------------------------------------------------
void InitParamsVertexLitGeneric_DX9( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, bool bVertexLitGeneric, VertexLitGeneric_DX9_Vars_t &info )
{	
	InitIntParam( info.m_nBlendWithSmokeGrenade, params, 0 );

	if ( info.m_nBlendWithSmokeGrenadePosEntity != -1 && !params[info.m_nBlendWithSmokeGrenadePosEntity]->IsDefined() )
		params[info.m_nBlendWithSmokeGrenadePosEntity]->SetVecValue( 0, 0, 0 );

	if ( info.m_nBlendWithSmokeGrenadePosSmoke != -1 && !params[info.m_nBlendWithSmokeGrenadePosSmoke]->IsDefined() )
		params[info.m_nBlendWithSmokeGrenadePosSmoke]->SetVecValue( 0, 0, 0 );

#if defined ( CSTRIKE15 )
	InitIntParam( info.m_nShaderSrgbRead360, params, 1 );
#else
	InitIntParam( info.m_nShaderSrgbRead360, params, 0 );
#endif

	InitIntParam( info.m_nPhong, params, 0 );

	InitIntParam( info.m_nAllowFenceRenderStateHack, params, 0 );
	InitFloatParam( info.m_nAlphaTestReference, params, 0.0f );
	InitIntParam( info.m_nVertexAlphaTest, params, 0 );

	InitIntParam( info.m_nFlashlightNoLambert, params, 0 );
	InitIntParam( info.m_nLowQualityFlashlightShadows, params, 0 );
	InitIntParam( info.m_nDisableCSMLookup, params, 0 );
	
	if ( info.m_nDetailTint != -1 && !params[info.m_nDetailTint]->IsDefined() )
	{
		params[info.m_nDetailTint]->SetVecValue( 1.0f, 1.0f, 1.0f );
	}

	if ( info.m_nEnvmapTint != -1 && !params[info.m_nEnvmapTint]->IsDefined() )
	{
		params[info.m_nEnvmapTint]->SetVecValue( 1.0f, 1.0f, 1.0f );
	}

	InitIntParam( info.m_nEnvmapFrame, params, 0 );
	InitIntParam( info.m_nBumpFrame, params, 0 );
	InitFloatParam( info.m_nDetailTextureBlendFactor, params, 1.0 );
	InitIntParam( info.m_nReceiveFlashlight, params, 0 );

	InitFloatParam( info.m_nDetailScale, params, 4.0f );

	if ( (info.m_nSelfIllumTint != -1) && (!params[info.m_nSelfIllumTint]->IsDefined()) )
	{
		params[info.m_nSelfIllumTint]->SetVecValue( 1.0f, 1.0f, 1.0f );
	}

	// Set the selfillummask flag2
	if ( ( info.m_nSelfIllumMask != -1 ) && ( params[info.m_nSelfIllumMask]->IsDefined() ) )
	{
		SET_FLAGS2( MATERIAL_VAR2_SELFILLUMMASK );
	}

	InitFloatParam( info.m_nSelfIllumMaskScale, params, 1.0f );

	// Override vertex fog via the global setting if it isn't enabled/disabled in the material file.
	if ( !IS_FLAG_DEFINED( MATERIAL_VAR_VERTEXFOG ) && mat_force_vertexfog.GetBool() )
	{
		SET_FLAGS( MATERIAL_VAR_VERTEXFOG );
	}

	if ( WantsPhongShader( params, info ) )
	{
		if ( !g_pHardwareConfig->SupportsPixelShaders_2_b() )
		{
			params[info.m_nPhong]->SetIntValue( 0 );
		}
		else
		{
			InitParamsPhong_DX9( pShader, params, pMaterialName, info );
			return;
		}
	}
	
	// FLASHLIGHTFIXME: Do ShaderAPI::BindFlashlightTexture
	if ( info.m_nFlashlightTexture != -1 )
	{
		params[FLASHLIGHTTEXTURE]->SetStringValue( GetFlashlightTextureFilename() );
	}
	
	// Write over $basetexture with $info.m_nBumpmap if we are going to be using diffuse normal mapping.
	if ( info.m_nAlbedo != -1 && g_pConfig->UseBumpmapping() && info.m_nBumpmap != -1 && params[info.m_nBumpmap]->IsDefined() && params[info.m_nAlbedo]->IsDefined() &&
		params[info.m_nBaseTexture]->IsDefined() )
	{
		params[info.m_nBaseTexture]->SetStringValue( params[info.m_nAlbedo]->GetStringValue() );
	}

	// This shader can be used with hw skinning
	SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );

	if ( bVertexLitGeneric )
	{
		SET_FLAGS2( MATERIAL_VAR2_LIGHTING_VERTEX_LIT );
	}
	else
	{
		CLEAR_FLAGS( MATERIAL_VAR_SELFILLUM );
	}

	InitIntParam( info.m_nEnvmapMaskFrame, params, 0 );
	InitFloatParam( info.m_nEnvmapContrast, params, 0.0 );
	InitFloatParam( info.m_nEnvmapSaturation, params, 1.0f );
	if ( info.m_nEnvMapLightScale != -1 )
		InitFloatParam( info.m_nEnvMapLightScale, params, 0.0f );
	if ( info.m_nEnvMapLightScaleMinMax != -1 )
		InitVecParam( info.m_nEnvMapLightScaleMinMax, params, 0.0f, 1.0f );
	InitFloatParam( info.m_nSeamlessScale, params, 0.0 );

	// handle line art parms
	InitFloatParam( info.m_nEdgeSoftnessStart, params, 0.5 );
	InitFloatParam( info.m_nEdgeSoftnessEnd, params, 0.5 );
	InitFloatParam( info.m_nGlowAlpha, params, 1.0 );
	InitFloatParam( info.m_nOutlineAlpha, params, 1.0 );

	// No texture means no self-illum or env mask in base alpha
	if ( info.m_nBaseTexture != -1 && !params[info.m_nBaseTexture]->IsDefined() )
	{
		CLEAR_FLAGS( MATERIAL_VAR_SELFILLUM );
		CLEAR_FLAGS( MATERIAL_VAR_BASEALPHAENVMAPMASK );
	}

	// If in decal mode, no debug override...
	if (IS_FLAG_SET(MATERIAL_VAR_DECAL))
	{
		SET_FLAGS( MATERIAL_VAR_NO_DEBUG_OVERRIDE );
	}

	if( ( (info.m_nBumpmap != -1) && g_pConfig->UseBumpmapping() && params[info.m_nBumpmap]->IsDefined() ) 
		// we don't need a tangent space if we have envmap without bumpmap
		//		|| ( info.m_nEnvmap != -1 && params[info.m_nEnvmap]->IsDefined() ) 
		)
	{
		SET_FLAGS2( MATERIAL_VAR2_NEEDS_TANGENT_SPACES );
	}
	else if ( bVertexLitGeneric && (info.m_nDiffuseWarpTexture != -1) && params[info.m_nDiffuseWarpTexture]->IsDefined() ) // diffuse warp goes down bump path...
	{
		SET_FLAGS2( MATERIAL_VAR2_NEEDS_TANGENT_SPACES );
	}
	else // no tangent space needed
	{
		CLEAR_FLAGS( MATERIAL_VAR_NORMALMAPALPHAENVMAPMASK );
	}

	bool hasNormalMapAlphaEnvmapMask = IS_FLAG_SET( MATERIAL_VAR_NORMALMAPALPHAENVMAPMASK );
	if ( hasNormalMapAlphaEnvmapMask )
	{
		params[info.m_nEnvmapMask]->SetUndefined();
		CLEAR_FLAGS( MATERIAL_VAR_BASEALPHAENVMAPMASK );
	}

	if ( IS_FLAG_SET( MATERIAL_VAR_BASEALPHAENVMAPMASK ) && info.m_nBumpmap != -1 && 
		params[info.m_nBumpmap]->IsDefined() && !hasNormalMapAlphaEnvmapMask )
	{
		DevWarning( "material %s has a normal map and $basealphaenvmapmask.  Must use $normalmapalphaenvmapmask to get specular.\n\n", pMaterialName );
		params[info.m_nEnvmap]->SetUndefined();
	}
	
	if ( info.m_nEnvmapMask != -1 && params[info.m_nEnvmapMask]->IsDefined() && info.m_nBumpmap != -1 && params[info.m_nBumpmap]->IsDefined() )
	{
		params[info.m_nEnvmapMask]->SetUndefined();
		if ( !hasNormalMapAlphaEnvmapMask )
		{
			DevWarning( "material %s has a normal map and an envmapmask.  Must use $normalmapalphaenvmapmask.\n\n", pMaterialName );
			params[info.m_nEnvmap]->SetUndefined();
		}
	}

	// If mat_specular 0, then get rid of envmap
	if ( !g_pConfig->UseSpecular() && info.m_nEnvmap != -1 && params[info.m_nEnvmap]->IsDefined() && params[info.m_nBaseTexture]->IsDefined() )
	{
		params[info.m_nEnvmap]->SetUndefined();
	}

	InitFloatParam( info.m_nHDRColorScale, params, 1.0f );

	InitIntParam( info.m_nLinearWrite, params, 0 );
	InitIntParam( info.m_nGammaColorRead, params, 0 );

	InitIntParam( info.m_nAllowDiffuseModulation, params, 1 );

	if ( ( info.m_nEnvMapFresnelMinMaxExp != -1 ) && !params[info.m_nEnvMapFresnelMinMaxExp]->IsDefined() )
	{
		params[info.m_nEnvMapFresnelMinMaxExp]->SetVecValue( 0.0f, 1.0f, 2.0f, 0.0f );
	}
	if ( ( info.m_nBaseAlphaEnvMapMaskMinMaxExp != -1 ) && !params[info.m_nBaseAlphaEnvMapMaskMinMaxExp]->IsDefined() )
	{
		// Default to min: 1 max: 0 exp: 1 so that we default to the legacy behavior for basealphaenvmapmask, which is 1-baseColor.a
		// These default values translate to a scale of -1, bias of 1 and exponent 1 in the shader.
		params[info.m_nBaseAlphaEnvMapMaskMinMaxExp]->SetVecValue( 1.0f, 0.0f, 1.0f, 0.0f );
	}

	InitIntParam( info.m_nTreeSway, params, 0 );
	InitFloatParam( info.m_nTreeSwayHeight, params, 1000.0f );
	InitFloatParam( info.m_nTreeSwayStartHeight, params, 0.1f );
	InitFloatParam( info.m_nTreeSwayRadius, params, 300.0f );
	InitFloatParam( info.m_nTreeSwayStartRadius, params, 0.2f );
	InitFloatParam( info.m_nTreeSwaySpeed, params, 1.0f );
	InitFloatParam( info.m_nTreeSwaySpeedHighWindMultiplier, params, 2.0f );
	InitFloatParam( info.m_nTreeSwayStrength, params, 10.0f );
	InitFloatParam( info.m_nTreeSwayScrumbleSpeed, params, 5.0f );
	InitFloatParam( info.m_nTreeSwayScrumbleStrength, params, 10.0f );
	InitFloatParam( info.m_nTreeSwayScrumbleFrequency, params, 12.0f );
	InitFloatParam( info.m_nTreeSwayFalloffExp, params, 1.5f );
	InitFloatParam( info.m_nTreeSwayScrumbleFalloffExp, params, 1.0f );
	InitFloatParam( info.m_nTreeSwaySpeedLerpStart, params, 3.0f );
	InitFloatParam( info.m_nTreeSwaySpeedLerpEnd, params, 6.0f );
	
	InitIntParam( info.m_nTreeSwayStatic, params, 0 );

	InitIntParam( info.m_nModelDecalIgnoreZ, params, 0 );
}


//-----------------------------------------------------------------------------
// Initialize shader
//-----------------------------------------------------------------------------

void InitVertexLitGeneric_DX9( CBaseVSShader *pShader, IMaterialVar** params, bool bVertexLitGeneric, VertexLitGeneric_DX9_Vars_t &info )
{
	// both detailed and bumped = needs Phong shader (for now)
	bool bNeedsPhongBecauseOfDetail = false;
	
	//bool bHasBump = ( info.m_nBumpmap != -1 ) && params[info.m_nBumpmap]->IsTexture();
	//if ( bHasBump )
	//{
	//	if (  ( info.m_nDetail != -1 ) && params[info.m_nDetail]->IsDefined() )
	//		bNeedsPhongBecauseOfDetail = true;
	//}

	if ( bNeedsPhongBecauseOfDetail || 
		 ( info.m_nPhong != -1 && 
		   params[info.m_nPhong]->GetIntValue() ) && 
		 g_pHardwareConfig->SupportsPixelShaders_2_b() )
	{
		bool bPhongEnabled = mat_phong.GetBool();
		
		// If mat_phong is disabled (because the GPU level is too low), check to see if the material uses Phong and wants to force Phong on anyway.
		if ( !bPhongEnabled )
		{
			if ( ( info.m_nForcePhong != -1 ) && ( params[info.m_nForcePhong]->GetIntValue() ) )
			{
				bPhongEnabled = true;
			}
		}

		if ( bPhongEnabled )
		{
			InitPhong_DX9( pShader, params, info );
			return;
		}
	}

	if ( info.m_nFlashlightTexture != -1 )
	{
		pShader->LoadTexture( info.m_nFlashlightTexture, TEXTUREFLAGS_SRGB );
	}
	
	bool bIsBaseTextureTranslucent = false;
	if ( info.m_nBaseTexture != -1 && params[info.m_nBaseTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nBaseTexture, ( ( ( info.m_nGammaColorRead != -1 ) && ( params[info.m_nGammaColorRead]->GetIntValue() == 1 ) ) ? 0 : TEXTUREFLAGS_SRGB ) | ANISOTROPIC_OVERRIDE );

		if ( params[info.m_nBaseTexture]->GetTextureValue()->IsTranslucent() )
		{
			bIsBaseTextureTranslucent = true;
		}
	}

	bool bHasSelfIllumMask = IS_FLAG_SET( MATERIAL_VAR_SELFILLUM ) && (info.m_nSelfIllumMask != -1) && params[info.m_nSelfIllumMask]->IsDefined();

	// No alpha channel in any of the textures? No self illum or envmapmask
	if ( !bIsBaseTextureTranslucent )
	{
		bool bHasSelfIllumFresnel = IS_FLAG_SET( MATERIAL_VAR_SELFILLUM ) && ( info.m_nSelfIllumFresnel != -1 ) && ( params[info.m_nSelfIllumFresnel]->GetIntValue() != 0 );

		// Can still be self illum with no base alpha if using one of these alternate modes
		if ( !bHasSelfIllumFresnel && !bHasSelfIllumMask )
		{
			CLEAR_FLAGS( MATERIAL_VAR_SELFILLUM );
		}

		CLEAR_FLAGS( MATERIAL_VAR_BASEALPHAENVMAPMASK );
	}

	if ( info.m_nDetail != -1 && params[info.m_nDetail]->IsDefined() )
	{
		int nDetailBlendMode = ( info.m_nDetailTextureCombineMode == -1 ) ? 0 : params[info.m_nDetailTextureCombineMode]->GetIntValue();
		pShader->LoadTexture( info.m_nDetail, IsSRGBDetailTexture( nDetailBlendMode ) ? TEXTUREFLAGS_SRGB : 0 );
	}

	if ( g_pConfig->UseBumpmapping() )
	{
		if ( (info.m_nBumpmap != -1) && params[info.m_nBumpmap]->IsDefined() )
		{
			pShader->LoadBumpMap( info.m_nBumpmap, ANISOTROPIC_OVERRIDE );
			SET_FLAGS2( MATERIAL_VAR2_DIFFUSE_BUMPMAPPED_MODEL );
		}
		else if ( (info.m_nDiffuseWarpTexture != -1) && params[info.m_nDiffuseWarpTexture]->IsDefined() )
		{
			SET_FLAGS2( MATERIAL_VAR2_DIFFUSE_BUMPMAPPED_MODEL );
		}
	}

	// Don't alpha test if the alpha channel is used for other purposes
	if ( ( IS_FLAG_SET( MATERIAL_VAR_SELFILLUM) && !IS_FLAG2_SET( MATERIAL_VAR2_SELFILLUMMASK ) ) || IS_FLAG_SET( MATERIAL_VAR_BASEALPHAENVMAPMASK ) )
	{
		CLEAR_FLAGS( MATERIAL_VAR_ALPHATEST );
	}
	
	if ( info.m_nEnvmap != -1 && params[info.m_nEnvmap]->IsDefined() )
	{
		pShader->LoadCubeMap( info.m_nEnvmap, ( g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE ? TEXTUREFLAGS_SRGB : 0 ) | ANISOTROPIC_OVERRIDE );
	}

	if ( info.m_nEnvmapMask != -1 && params[info.m_nEnvmapMask]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nEnvmapMask );
	}

	if ( (info.m_nDiffuseWarpTexture != -1) && params[info.m_nDiffuseWarpTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nDiffuseWarpTexture );
	}

	if ( bHasSelfIllumMask )
	{
		pShader->LoadTexture( info.m_nSelfIllumMask );
	}

	if ( ( info.m_nDisplacementMap != -1 ) && params[info.m_nDisplacementMap]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nDisplacementMap );
	}

	if ( ( info.m_nDecalTexture != -1 ) && params[info.m_nDecalTexture]->IsDefined() )
	{
		int nDecalBlendMode = (info.m_nDecalTextureCombineMode == -1) ? 0 : params[info.m_nDecalTextureCombineMode]->GetIntValue();
		pShader->LoadTexture( info.m_nDecalTexture, IsSRGBDecalTexture( nDecalBlendMode ) ? TEXTUREFLAGS_SRGB : 0 );
	}

	if ( (info.m_nTintMaskTexture != -1) && params[info.m_nTintMaskTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nTintMaskTexture, TEXTUREFLAGS_SRGB );
	}
}

// FIXME: 
// From revision #18 of this file in staging:
//     "Quick fix to keep Phong shader from being run if the mat_bumpmap convar is set to zero."
// This change caused shader problems in HLMV (use ep2/Hunter model as an example) and I was told to fix it. -Jeep
//
//extern ConVar mat_bumpmap( "mat_bumpmap", "1" );

class CVertexLitGeneric_DX9_Context : public CBasePerMaterialContextData
{
public:
	CCommandBufferBuilder< CFixedCommandStorageBuffer< 800 > > m_SemiStaticCmdsOut;
#ifdef _PS3
	CCommandBufferBuilder< CFixedCommandStorageBuffer< 256 > > m_flashlightECB;
#endif
};


//-----------------------------------------------------------------------------
// Draws the shader
//-----------------------------------------------------------------------------
static void DrawVertexLitGeneric_DX9_Internal( CBaseVSShader *pShader, IMaterialVar** params,
											   IShaderDynamicAPI * pShaderAPI,
											   IShaderShadow* pShaderShadow,
											   bool bVertexLitGeneric, bool bHasFlashlight, bool bSinglePassFlashlight, 
											   VertexLitGeneric_DX9_Vars_t &info,
											   VertexCompressionType_t vertexCompression,
											   CBasePerMaterialContextData **pContextDataPtr ) 

{
	CVertexLitGeneric_DX9_Context *pContextData = reinterpret_cast< CVertexLitGeneric_DX9_Context *> ( *pContextDataPtr );

	bool bAvgStaticLightStreams = r_staticlight_mode.GetInt() == 1;
	bool bUseStaticControlFlow = g_pHardwareConfig->SupportsStaticControlFlow() && ( !bAvgStaticLightStreams );
	bool bSupportsSM3 = g_pHardwareConfig->SupportsPixelShaders_3_0() && ( !bAvgStaticLightStreams );

	bool bSFM = ( ToolsEnabled() && IsPlatformWindowsPC() && bSupportsSM3 ) ? true : false;

	bool bHasBump = IsTextureSet( info.m_nBumpmap, params );
#if !defined( _GAMECONSOLE )
	bool bIsDecal = IS_FLAG_SET( MATERIAL_VAR_DECAL );
#endif

	float fSinglePassFlashlight = bSinglePassFlashlight ? 1.0f : 0.0f;

	bool hasDiffuseLighting = bVertexLitGeneric;

#if defined( CSTRIKE15 )
	bool bShaderSrgbRead = ( IsX360() && r_shader_srgbread.GetBool() );
#else
	bool bShaderSrgbRead = ( IsX360() && IS_PARAM_DEFINED( info.m_nShaderSrgbRead360 ) && ( params[info.m_nShaderSrgbRead360]->GetIntValue() ) );
#endif

	bool bIsAlphaTested = IS_FLAG_SET( MATERIAL_VAR_ALPHATEST ) != 0;
	bool bHasDiffuseWarp = (!bHasFlashlight || bSinglePassFlashlight) && hasDiffuseLighting && (info.m_nDiffuseWarpTexture != -1) && params[info.m_nDiffuseWarpTexture]->IsTexture();

	bool bHasDecalTexture = IsTextureSet( info.m_nDecalTexture, params );
	bool bHasTintMaskTexture = IsTextureSet( info.m_nTintMaskTexture, params );
		
	//bool bNoCull = IS_FLAG_SET( MATERIAL_VAR_NOCULL );
	bool bFlashlightNoLambert = false;
	if ( ( info.m_nFlashlightNoLambert != -1 ) && params[info.m_nFlashlightNoLambert]->GetIntValue() )
	{
		bFlashlightNoLambert = true;
	}

	bool bLowQualityFlashlightShadows = false;
	if ( IsGameConsole() )
	{
		// Low quality shadows, useful on dense folliage.
		if ( ( info.m_nLowQualityFlashlightShadows != -1 ) && params[info.m_nLowQualityFlashlightShadows]->GetIntValue() )
		{
			bLowQualityFlashlightShadows = true;
		}
	}

	bool bDisableCSMLookup = false;
	
	if ( ( ( info.m_nDisableCSMLookup != -1 ) && params[info.m_nDisableCSMLookup]->GetIntValue() ) || g_pConfig->nFullbright == 1 )
	{
		// disable CSM lookup/filtering, useful on dense foliage
		bDisableCSMLookup = true;
	}
	
	bool bAmbientOnly = IsBoolSet( info.m_nAmbientOnly, params );
	// Hack

	int nDetailBlendMode= GetIntParam( info.m_nDetailTextureCombineMode, params );
	int nDecalBlendMode = GetIntParam( info.m_nDecalTextureCombineMode, params );

	if ( ( nDetailBlendMode == DETAIL_BLEND_MODE_RGB_ADDITIVE_SELFILLUM_THRESHOLD_FADE ) && ( !g_pHardwareConfig->SupportsPixelShaders_2_b() ) )
	{
		nDetailBlendMode = DETAIL_BLEND_MODE_RGB_ADDITIVE_SELFILLUM;	// skip fancy threshold blending if ps2.0
	}

	BlendType_t nBlendType;

	int nDetailTranslucencyTexture = -1;

	float fBlendFactor = GetFloatParam( info.m_nDetailTextureBlendFactor, params, 1.0 );
	bool bHasDetailTexture = IsTextureSet( info.m_nDetail, params );
	if ( bHasDetailTexture && ( fBlendFactor > 0.0 ) )
	{
		if ( ( nDetailBlendMode == DETAIL_BLEND_MODE_FADE ) ||
			 ( nDetailBlendMode == DETAIL_BLEND_MODE_MULTIPLY ) ||
			 ( nDetailBlendMode == DETAIL_BLEND_MODE_MASK_BASE_BY_DETAIL_ALPHA ) )
		{
			nDetailTranslucencyTexture = info.m_nDetail;
		}
	}

	bool bHasDisplacement = (info.m_nDisplacementMap != -1) && params[info.m_nDisplacementMap]->IsTexture() && bSFM;

	bool bHasBaseTexture = IsTextureSet( info.m_nBaseTexture, params );
	if ( bHasBaseTexture )
	{
		nBlendType = pShader->EvaluateBlendRequirements( info.m_nBaseTexture, true, nDetailTranslucencyTexture );
	}
	else
	{
		nBlendType = pShader->EvaluateBlendRequirements( info.m_nEnvmapMask, false );
	}
	bool bFullyOpaque = (nBlendType != BT_BLENDADD) && (nBlendType != BT_BLEND) && !bIsAlphaTested && (!bHasFlashlight || bSinglePassFlashlight); //dest alpha is free for special use

	bool bHasEnvmap = (!bHasFlashlight || bSinglePassFlashlight) && info.m_nEnvmap != -1 && params[info.m_nEnvmap]->IsTexture();

	if ( IsPC() && bHasFlashlight && bHasEnvmap && bSinglePassFlashlight )
	{
		Warning( "VertexLitGeneric_Dx9: Unsupported combo! Can't use envmap + flashlight + singlepass flashlight!\n" );
	}
	bool bSRGBWrite = true;
	if ( ( info.m_nLinearWrite != -1 ) && ( params[info.m_nLinearWrite]->GetIntValue() == 1 ) )
	{
		bSRGBWrite = false;
	}

	bool bHasVertexColor = bVertexLitGeneric ? false : IS_FLAG_SET( MATERIAL_VAR_VERTEXCOLOR );
	bool bHasVertexAlpha = bVertexLitGeneric ? false : IS_FLAG_SET( MATERIAL_VAR_VERTEXALPHA );

	bool bTreeSway = ( GetIntParam( info.m_nTreeSway, params, 0 ) != 0 );
	int nTreeSwayMode = GetIntParam( info.m_nTreeSway, params, 0 );
	nTreeSwayMode = clamp( nTreeSwayMode, 0, 2 );

#if !defined( _GAMECONSOLE )
	bool bMorphing = ( !pShaderAPI || pShaderAPI->IsHWMorphingEnabled() ) && !bTreeSway && bSFM && g_pHardwareConfig->HasFastVertexTextures();
#endif

	if ( pShader->IsSnapshotting() || (! pContextData ) || ( pContextData->m_bMaterialVarsChanged ) )
	{
		bool bSeamlessBase = IsBoolSet( info.m_nSeamlessBase, params ) && !bTreeSway;
		bool bSeamlessDetail = IsBoolSet( info.m_nSeamlessDetail, params ) && !bTreeSway;
		bool bDistanceAlpha = IsBoolSet( info.m_nDistanceAlpha, params );
		bool bHasSelfIllum = (!bHasFlashlight || bSinglePassFlashlight) && IS_FLAG_SET( MATERIAL_VAR_SELFILLUM );

		bool bHasSelfIllumMask = bHasSelfIllum && IsTextureSet( info.m_nSelfIllumMask, params );
		bool bHasSelfIllumInEnvMapMask =
			( info.m_nSelfIllumEnvMapMask_Alpha != -1 ) &&
			( params[info.m_nSelfIllumEnvMapMask_Alpha]->GetFloatValue() != 0.0 ) ;
		bool bDesaturateWithBaseAlpha = !bHasSelfIllum && !bHasSelfIllumMask && GetFloatParam( info.m_nDesaturateWithBaseAlpha, params ) > 0.0f;

		if  ( pShader->IsSnapshotting() )
		{
			// Per-instance state
#if 0
			int nLightingPreviewMode = IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER0 ) + 2 * IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER1 );
#else
			int nLightingPreviewMode = IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER0 );
#endif
			pShader->PI_BeginCommandBuffer();
			if ( bVertexLitGeneric )
			{
				if ( bHasBump || bHasDiffuseWarp )
				{
					pShader->PI_SetPixelShaderAmbientLightCube( 5 );
					pShader->PI_SetPixelShaderLocalLighting( 13 );
				}
				pShader->PI_SetVertexShaderAmbientLightCube();
			}
			// material can choose to support per-instance modulation via $allowdiffusemodulation
			bool bAllowDiffuseModulation = ( info.m_nAllowDiffuseModulation == -1 ) ? true : ( params[info.m_nAllowDiffuseModulation]->GetIntValue() != 0 );

			if ( bAllowDiffuseModulation )
			{
				if ( ( info.m_nHDRColorScale != -1 ) && pShader->IsHDREnabled() )
				{
					if ( bSRGBWrite )
						pShader->PI_SetModulationPixelShaderDynamicState_LinearColorSpace_LinearScale( 1, params[info.m_nHDRColorScale]->GetFloatValue() );
					else
						pShader->PI_SetModulationPixelShaderDynamicState_LinearScale( 1, params[info.m_nHDRColorScale]->GetFloatValue() );
				}
				else
				{
					if ( bSRGBWrite )
						pShader->PI_SetModulationPixelShaderDynamicState_LinearColorSpace( 1 );
					else
						pShader->PI_SetModulationPixelShaderDynamicState( 1 );
				}
			}
			else
			{
				pShader->PI_SetModulationPixelShaderDynamicState_Identity( 1 );
			}
			pShader->PI_EndCommandBuffer();

			bool hasBaseAlphaEnvmapMask = IS_FLAG_SET( MATERIAL_VAR_BASEALPHAENVMAPMASK );
			bool hasNormalMapAlphaEnvmapMask = IS_FLAG_SET( MATERIAL_VAR_NORMALMAPALPHAENVMAPMASK );
			
			if ( info.m_nVertexAlphaTest != -1 && params[info.m_nVertexAlphaTest]->GetIntValue() > 0 )
			{
				bHasVertexAlpha = true;
			}

			// look at color and alphamod stuff.
			// Unlit generic never uses the flashlight
			bool bHasSelfIllumFresnel = ( !bHasDetailTexture ) && ( bHasSelfIllum ) && ( info.m_nSelfIllumFresnel != -1 ) && ( params[info.m_nSelfIllumFresnel]->GetIntValue() != 0 );
			if ( bHasSelfIllumFresnel )
			{
				CLEAR_FLAGS( MATERIAL_VAR_NORMALMAPALPHAENVMAPMASK );
				hasNormalMapAlphaEnvmapMask = false;
			}

			bool bHasEnvmap = (!bHasFlashlight || bSinglePassFlashlight) && ( info.m_nEnvmap != -1 ) && params[info.m_nEnvmap]->IsTexture();
			bool bHasEnvmapMask = (!bHasFlashlight || bSinglePassFlashlight) && ( info.m_nEnvmapMask != -1 && params[info.m_nEnvmapMask]->IsTexture() );
			bool bHasNormal = bVertexLitGeneric || bHasEnvmap || bHasFlashlight || bSeamlessBase || bSeamlessDetail;
			if ( IsPC() )
			{
				// On PC, LIGHTING_PREVIEW requires normals (they won't use much memory - unlitgeneric isn't used on many models)
				bHasNormal = true;
			}

			bool bHasEnvMapFresnel = bHasEnvmap && IsBoolSet( info.m_nEnvmapFresnel, params );

			//bool bHalfLambert = IS_FLAG_SET( MATERIAL_VAR_HALFLAMBERT );

			// Disabling half-lambert for CSGO (not compatible with CSM's, causes bad shadow aliasing).
			bool bHalfLambert = false;

			bool bSetBlendingShadowStateCalled = false;

			ShadowFilterMode_t nShadowFilterMode = SHADOWFILTERMODE_DEFAULT;
			if ( bHasFlashlight )
			{
				if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
				{
					nShadowFilterMode = g_pHardwareConfig->GetShadowFilterMode( bLowQualityFlashlightShadows /* bForceLowQuality */, bSupportsSM3 && !IsGameConsole() /* bPS30 */ );	// Based upon vendor and device dependent formats
				}

				if ( bSinglePassFlashlight )
				{
					pShader->SetBlendingShadowState( nBlendType );
					bSetBlendingShadowStateCalled = true;
				}
				else
				{
					//doing the flashlight as a second additive pass
					if (params[info.m_nBaseTexture]->IsTexture())
					{
						pShader->SetAdditiveBlendingShadowState( info.m_nBaseTexture, true );
					}
					else
					{
						pShader->SetAdditiveBlendingShadowState( info.m_nEnvmapMask, false );
					}

					if ( bIsAlphaTested )
					{
						// disable alpha test and use the zfunc zequals since alpha isn't guaranteed to 
						// be the same on both the regular pass and the flashlight pass.
						pShaderShadow->EnableAlphaTest( false );
						pShaderShadow->DepthFunc( SHADER_DEPTHFUNC_EQUAL );
					}

					// Be sure not to write to dest alpha
					pShaderShadow->EnableAlphaWrites( false );

					pShaderShadow->EnableBlending( true );
					pShaderShadow->EnableDepthWrites( false );
				}
			}
			else
			{
				pShader->SetBlendingShadowState( nBlendType );
				bSetBlendingShadowStateCalled = true;
			}

			// Fence render state override
			if ( bSetBlendingShadowStateCalled )
			{
				if ( ( info.m_nAllowFenceRenderStateHack != -1 ) && params[info.m_nAllowFenceRenderStateHack]->GetIntValue() )
				{
					pShader->EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
					pShaderShadow->EnableDepthWrites( true );
				}
			}

			bool bModelDecalIgnoreZ = IsBoolSet( info.m_nModelDecalIgnoreZ, params );
			if ( bModelDecalIgnoreZ && !pShader->UsingEditor( params ) /* <-- don't ignorez in hammer, hammer may render entities in wrong order making this mesh 'invisible' behind something that renders subsequently */ )
			{
				pShaderShadow->EnableDepthWrites( false );
			}

			// Alpha test
			pShaderShadow->EnableAlphaTest( bIsAlphaTested );
			if ( info.m_nAlphaTestReference != -1 && params[info.m_nAlphaTestReference]->GetFloatValue() > 0.0f )
			{
				pShaderShadow->AlphaFunc( SHADER_ALPHAFUNC_GEQUAL, params[info.m_nAlphaTestReference]->GetFloatValue() );
			}

			unsigned int flags = VERTEX_POSITION;
			if ( bHasNormal )
			{
				flags |= VERTEX_NORMAL;
			}

			int userDataSize = 0;
			bool bSRGBInputAdapter = false;

			// basetexture
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );

			if ( bHasBaseTexture )
			{
				if ( ( info.m_nGammaColorRead != -1 ) && ( params[info.m_nGammaColorRead]->GetIntValue() == 1 ) )
					pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, false );
				else
					pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, !bShaderSrgbRead );
			}


			bool bSampler0SrgbRead = false;
			if ( bHasBaseTexture )
			{
				if ( ( info.m_nGammaColorRead != -1 ) && ( params[info.m_nGammaColorRead]->GetIntValue() == 1 ) )
					bSampler0SrgbRead = false;
				else
					bSampler0SrgbRead = !bShaderSrgbRead;
			}
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, bSampler0SrgbRead );

			if ( bHasEnvmap )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
				if( g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE )
				{
					pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, true );
				}
			}
			if ( bHasFlashlight )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER8, true );	// Depth texture
				//pShaderShadow->SetShadowDepthFiltering( SHADER_SAMPLER8 );
				pShaderShadow->EnableTexture( SHADER_SAMPLER6, true );	// Noise map

				pShaderShadow->EnableTexture( SHADER_SAMPLER7, true );	// Flashlight cookie
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER7, true );
			}
			if ( bHasDetailTexture )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER2, IsSRGBDetailTexture( nDetailBlendMode ) );
			}
			if ( bHasBump || bHasDiffuseWarp )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );
				userDataSize = 4; // tangent S
				// Normalizing cube map
				pShaderShadow->EnableTexture( SHADER_SAMPLER5, true );
			}
			if ( bHasEnvmapMask )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );
			}

			if ( ( bHasVertexColor || bHasVertexAlpha ) || ( bIsDecal && !IS_FLAG_SET( MATERIAL_VAR_WIREFRAME ) ) )
			{
				flags |= VERTEX_COLOR;
			}
			else if ( !bHasDiffuseWarp )
			{
				flags |= VERTEX_COLOR_STREAM_1;
			}

			if( bHasDiffuseWarp && (!bHasFlashlight || bSinglePassFlashlight) && !bHasSelfIllumFresnel )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER9, true );	// Diffuse warp texture
			}

			if( bHasSelfIllum )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER11, true );	// self illum mask
			}
			if ( bHasDisplacement && IsPC() && g_pHardwareConfig->HasFastVertexTextures() )
			{
				pShaderShadow->EnableVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER2, true );
			}

			if ( bHasDecalTexture )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER12, true ); // decal overlay
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER12, IsSRGBDecalTexture( nDecalBlendMode ) );
			}

			if ( bHasTintMaskTexture )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER13, true ); 
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER13, true );
			}

			pShaderShadow->EnableSRGBWrite( bSRGBWrite );
		
			// texcoord0 : base texcoord
			int pTexCoordDim[3] = { 2, 2, 3 };
			int nTexCoordCount = 1;
		
			if ( IsBoolSet( info.m_nSeparateDetailUVs, params ) )
			{
				++nTexCoordCount;
			}
			else
			{
				pTexCoordDim[1] = 0;
			}

#ifndef _GAMECONSOLE
			// Special morphed decal information 
			if ( bIsDecal && bMorphing )
			{
				nTexCoordCount = 3;
			}
#endif

			// This shader supports compressed vertices, so OR in that flag:
			flags |= VERTEX_FORMAT_COMPRESSED;

			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, pTexCoordDim, userDataSize );

			if ( bHasBump || bHasDiffuseWarp )
			{
				#ifndef _GAMECONSOLE
				if ( !bSupportsSM3 )
				#endif
				{
					DECLARE_STATIC_VERTEX_SHADER( vertexlit_and_unlit_generic_bump_vs20 );
					SET_STATIC_VERTEX_SHADER_COMBO( SFM, bSFM );
					SET_STATIC_VERTEX_SHADER_COMBO( HALFLAMBERT,  bHalfLambert);
					SET_STATIC_VERTEX_SHADER_COMBO( USE_WITH_2B,  g_pHardwareConfig->SupportsPixelShaders_2_b() );
					#ifdef _GAMECONSOLE
						SET_STATIC_VERTEX_SHADER_COMBO( FLASHLIGHT,  bHasFlashlight );
					#endif
					SET_STATIC_VERTEX_SHADER_COMBO( WORLD_NORMAL, 0 );
					SET_STATIC_VERTEX_SHADER_COMBO( FLATTEN_STATIC_CONTROL_FLOW, !bUseStaticControlFlow );
					bool bCSMEnabled_ps2b = false;
					SET_STATIC_VERTEX_SHADER_COMBO( CASCADED_SHADOW_MAPPING, bCSMEnabled_ps2b );
					SET_STATIC_VERTEX_SHADER( vertexlit_and_unlit_generic_bump_vs20 );
				
					if ( g_pHardwareConfig->SupportsPixelShaders_2_b() || g_pHardwareConfig->ShouldAlwaysUseShaderModel2bShaders() ) // Always send OpenGL this way
					{
						DECLARE_STATIC_PIXEL_SHADER( vertexlit_and_unlit_generic_bump_ps20b );
						SET_STATIC_PIXEL_SHADER_COMBO( SFM, bSFM );
						SET_STATIC_PIXEL_SHADER_COMBO( CUBEMAP,  bHasEnvmap );
						SET_STATIC_PIXEL_SHADER_COMBO( DIFFUSELIGHTING,  hasDiffuseLighting );
						SET_STATIC_PIXEL_SHADER_COMBO( LIGHTWARPTEXTURE, bHasDiffuseWarp && !bHasSelfIllumFresnel && !bHasFlashlight );
						SET_STATIC_PIXEL_SHADER_COMBO( SELFILLUM,  bHasSelfIllum );
						SET_STATIC_PIXEL_SHADER_COMBO( SELFILLUMFRESNEL, bHasSelfIllumFresnel );
						SET_STATIC_PIXEL_SHADER_COMBO( NORMALMAPALPHAENVMAPMASK,  hasNormalMapAlphaEnvmapMask );
						SET_STATIC_PIXEL_SHADER_COMBO( HALFLAMBERT,  bHalfLambert);
						SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHT,  bHasFlashlight );
						SET_STATIC_PIXEL_SHADER_COMBO( DETAILTEXTURE,  bHasDetailTexture );
						ClampDetailBlendModeAndWarn( nDetailBlendMode, 0, 7 );
						SET_STATIC_PIXEL_SHADER_COMBO( DETAIL_BLEND_MODE, nDetailBlendMode );
						ClampDecalBlendModeAndWarn( nDecalBlendMode, 0, 1 );
						SET_STATIC_PIXEL_SHADER_COMBO( DECAL_BLEND_MODE, bHasDecalTexture ? nDecalBlendMode : 2 );
						SET_STATIC_PIXEL_SHADER_COMBO( TINTMASKTEXTURE, bHasTintMaskTexture );
						SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode );
						SET_STATIC_PIXEL_SHADER_COMBO( SHADER_SRGB_READ, bShaderSrgbRead );
						SET_STATIC_PIXEL_SHADER_COMBO( WORLD_NORMAL, 0 );
						SET_STATIC_PIXEL_SHADER_COMBO( CASCADED_SHADOW_MAPPING, g_pHardwareConfig->SupportsCascadedShadowMapping() && !bSFM && !bHasFlashlight && !bDisableCSMLookup );
						SET_STATIC_PIXEL_SHADER_COMBO( CSM_MODE, 0 );
						SET_STATIC_PIXEL_SHADER( vertexlit_and_unlit_generic_bump_ps20b );
					}
					else // ps_2_0
					{
						DECLARE_STATIC_PIXEL_SHADER( vertexlit_and_unlit_generic_bump_ps20 );
						SET_STATIC_PIXEL_SHADER_COMBO( SFM, bSFM );
						SET_STATIC_PIXEL_SHADER_COMBO( CUBEMAP,  bHasEnvmap );
						SET_STATIC_PIXEL_SHADER_COMBO( DIFFUSELIGHTING,  hasDiffuseLighting );
						SET_STATIC_PIXEL_SHADER_COMBO( LIGHTWARPTEXTURE, bHasDiffuseWarp && !bHasSelfIllumFresnel );
						SET_STATIC_PIXEL_SHADER_COMBO( SELFILLUM,  bHasSelfIllum );
						SET_STATIC_PIXEL_SHADER_COMBO( SELFILLUMFRESNEL, bHasSelfIllumFresnel );
						SET_STATIC_PIXEL_SHADER_COMBO( NORMALMAPALPHAENVMAPMASK,  hasNormalMapAlphaEnvmapMask );
						SET_STATIC_PIXEL_SHADER_COMBO( HALFLAMBERT,  bHalfLambert);
						SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHT,  bHasFlashlight );
						SET_STATIC_PIXEL_SHADER_COMBO( DETAILTEXTURE,  bHasDetailTexture );
						ClampDetailBlendModeAndWarn( nDetailBlendMode, 0, 7 );
						SET_STATIC_PIXEL_SHADER_COMBO( DETAIL_BLEND_MODE, nDetailBlendMode );
						// Decal blend mode 2 is mod2x. It's implemented not as a combo but rather remapped to the
						// Multiply blend mode (1), we also plumb in a 2.0 scalar into a shader constant below.
						ClampDecalBlendModeAndWarn( nDecalBlendMode, 0, 2 );
						SET_STATIC_PIXEL_SHADER_COMBO( DECAL_BLEND_MODE, bHasDecalTexture ? Min( nDecalBlendMode, 1 ) : 2 );
						SET_STATIC_PIXEL_SHADER_COMBO( TINTMASKTEXTURE, 0 );
						SET_STATIC_PIXEL_SHADER_COMBO( SHADER_SRGB_READ, bShaderSrgbRead );
						SET_STATIC_PIXEL_SHADER_COMBO( WORLD_NORMAL, 0 );
						SET_STATIC_PIXEL_SHADER_COMBO( CASCADED_SHADOW_MAPPING, 0 );
						SET_STATIC_PIXEL_SHADER_COMBO( CSM_MODE, 0 );
						SET_STATIC_PIXEL_SHADER( vertexlit_and_unlit_generic_bump_ps20 );
					}
				}
				#if !defined( _GAMECONSOLE ) && !defined( _PS3 )
				else
				{
					// The vertex shader uses the vertex id stream
					if ( bSFM )
					{
						SET_FLAGS2( MATERIAL_VAR2_USES_VERTEXID );
						SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_TESSELLATION );
					}

					int nCSMQualityComboValue = g_pHardwareConfig->GetCSMShaderMode( materials->GetCurrentConfigForVideoCard().GetCSMQualityMode() );

					DECLARE_STATIC_VERTEX_SHADER( vertexlit_and_unlit_generic_bump_vs30 );
					SET_STATIC_VERTEX_SHADER_COMBO( SFM, bSFM );
					SET_STATIC_VERTEX_SHADER_COMBO( HALFLAMBERT, bHalfLambert);
					SET_STATIC_VERTEX_SHADER_COMBO( USE_WITH_2B, true );
					SET_STATIC_VERTEX_SHADER_COMBO( DECAL, bIsDecal );
					SET_STATIC_VERTEX_SHADER_COMBO( WORLD_NORMAL, nLightingPreviewMode == 3 );
					SET_STATIC_VERTEX_SHADER_COMBO( CASCADED_SHADOW_MAPPING, 0 );
					SET_STATIC_VERTEX_SHADER( vertexlit_and_unlit_generic_bump_vs30 );

					DECLARE_STATIC_PIXEL_SHADER( vertexlit_and_unlit_generic_bump_ps30 );
					SET_STATIC_PIXEL_SHADER_COMBO( SFM, bSFM );
					SET_STATIC_PIXEL_SHADER_COMBO( CUBEMAP, bHasEnvmap );
					SET_STATIC_PIXEL_SHADER_COMBO( DIFFUSELIGHTING, hasDiffuseLighting );
					SET_STATIC_PIXEL_SHADER_COMBO( LIGHTWARPTEXTURE, bHasDiffuseWarp && !bHasSelfIllumFresnel );
					SET_STATIC_PIXEL_SHADER_COMBO( SELFILLUM,  bHasSelfIllum && !bHasFlashlight ); // Careful here if we do single-pass flashlight on PC
					SET_STATIC_PIXEL_SHADER_COMBO( SELFILLUMFRESNEL, bHasSelfIllumFresnel );
					SET_STATIC_PIXEL_SHADER_COMBO( NORMALMAPALPHAENVMAPMASK, hasNormalMapAlphaEnvmapMask );
					SET_STATIC_PIXEL_SHADER_COMBO( HALFLAMBERT, bHalfLambert );
					SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHT, bHasFlashlight );
					SET_STATIC_PIXEL_SHADER_COMBO( DETAILTEXTURE, bHasDetailTexture );
					ClampDetailBlendModeAndWarn( nDetailBlendMode, 0, 7 );
					SET_STATIC_PIXEL_SHADER_COMBO( DETAIL_BLEND_MODE, nDetailBlendMode );
					ClampDecalBlendModeAndWarn( nDecalBlendMode, 0, 2 );
						// Decal blend mode 2 is mod2x. It's implemented not as a combo but rather remapped to the
						// Multiply blend mode (1), we also plumb in a 2.0 scalar into a shader constant below.
					SET_STATIC_PIXEL_SHADER_COMBO( DECAL_BLEND_MODE, bHasDecalTexture ? Min( nDecalBlendMode, 1 ) : 2 );
					SET_STATIC_PIXEL_SHADER_COMBO( TINTMASKTEXTURE, bHasTintMaskTexture );
					SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode );
					SET_STATIC_PIXEL_SHADER_COMBO( SHADER_SRGB_READ, bShaderSrgbRead );
					SET_STATIC_PIXEL_SHADER_COMBO( WORLD_NORMAL, nLightingPreviewMode == 3 );
					SET_STATIC_PIXEL_SHADER_COMBO( CASCADED_SHADOW_MAPPING, g_pHardwareConfig->SupportsCascadedShadowMapping() && !bSFM && !bHasFlashlight && !bDisableCSMLookup );
					SET_STATIC_PIXEL_SHADER_COMBO( CSM_MODE, ( g_pHardwareConfig->SupportsCascadedShadowMapping() && !bSFM && !bHasFlashlight && !bDisableCSMLookup ) ? nCSMQualityComboValue : 0 );
					SET_STATIC_PIXEL_SHADER( vertexlit_and_unlit_generic_bump_ps30 );
				}
				#endif
			}
			else // !(bHasBump || bHasDiffuseWarp)
			{
#if 0
				int nLightingPreviewMode = IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER0 ) + 2 * IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER1 );
#else
				int nLightingPreviewMode = IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER0 );
#endif
bool bDistanceAlphaFromDetail = false;
				bool bSoftMask = false;
				bool bGlow = false;
				bool bOutline = false;

				bHasSelfIllumInEnvMapMask = bHasSelfIllumInEnvMapMask && bHasEnvmapMask;

				if ( bDistanceAlpha )
				{
					bDistanceAlphaFromDetail = IsBoolSet( info.m_nDistanceAlphaFromDetail, params );
					bSoftMask = IsBoolSet( info.m_nSoftEdges, params );
					bGlow = IsBoolSet( info.m_nGlow, params );
					bOutline = IsBoolSet( info.m_nOutline, params );
				}

				#if !defined( _GAMECONSOLE ) && !defined( _PS3 )
				if ( !bSupportsSM3 )
				#endif
				{
					DECLARE_STATIC_VERTEX_SHADER( vertexlit_and_unlit_generic_vs20 );
					SET_STATIC_VERTEX_SHADER_COMBO( SFM, bSFM );
					SET_STATIC_VERTEX_SHADER_COMBO( VERTEXCOLOR,  bHasVertexColor || bHasVertexAlpha );
					SET_STATIC_VERTEX_SHADER_COMBO( CUBEMAP,  bHasEnvmap );
					SET_STATIC_VERTEX_SHADER_COMBO( HALFLAMBERT,  bHalfLambert );
					SET_STATIC_VERTEX_SHADER_COMBO( FLASHLIGHT,  bHasFlashlight );
					SET_STATIC_VERTEX_SHADER_COMBO( SEAMLESS_BASE, bSeamlessBase );
					SET_STATIC_VERTEX_SHADER_COMBO( SEAMLESS_DETAIL, bSeamlessDetail );
					SET_STATIC_VERTEX_SHADER_COMBO( SEPARATE_DETAIL_UVS, IsBoolSet( info.m_nSeparateDetailUVs, params ) );
					SET_STATIC_VERTEX_SHADER_COMBO( LIGHTING_PREVIEW, nLightingPreviewMode );
					SET_STATIC_VERTEX_SHADER_COMBO( TREESWAY, bTreeSway ? nTreeSwayMode : 0 );
					SET_STATIC_VERTEX_SHADER_COMBO( FLATTEN_STATIC_CONTROL_FLOW, !bUseStaticControlFlow );
					SET_STATIC_VERTEX_SHADER_COMBO( DECAL, bIsDecal );
					bool bCSMEnabled_ps2b = false;
					bool bCSMBlending = g_pHardwareConfig->GetCSMAccurateBlending();
					SET_STATIC_VERTEX_SHADER_COMBO( CASCADED_SHADOW_MAPPING, bCSMEnabled_ps2b );
					SET_STATIC_VERTEX_SHADER_COMBO( CSM_BLENDING, bCSMBlending );
					SET_STATIC_VERTEX_SHADER( vertexlit_and_unlit_generic_vs20 );
				
					if ( g_pHardwareConfig->SupportsPixelShaders_2_b() || g_pHardwareConfig->ShouldAlwaysUseShaderModel2bShaders() ) // Always send OpenGL this way
					{
						DECLARE_STATIC_PIXEL_SHADER( vertexlit_and_unlit_generic_ps20b );
						SET_STATIC_PIXEL_SHADER_COMBO( SFM, bSFM );
						SET_STATIC_PIXEL_SHADER_COMBO( SELFILLUM_ENVMAPMASK_ALPHA, bHasSelfIllumInEnvMapMask ); 
						SET_STATIC_PIXEL_SHADER_COMBO( DETAILTEXTURE,  bHasDetailTexture );
						SET_STATIC_PIXEL_SHADER_COMBO( CUBEMAP,  bHasEnvmap );
						SET_STATIC_PIXEL_SHADER_COMBO( DIFFUSELIGHTING,  hasDiffuseLighting );
						SET_STATIC_PIXEL_SHADER_COMBO( ENVMAPMASK,  bHasEnvmapMask && ( bHasEnvmap || bHasSelfIllumInEnvMapMask ) );
						SET_STATIC_PIXEL_SHADER_COMBO( BASEALPHAENVMAPMASK,  hasBaseAlphaEnvmapMask && bHasEnvmap );
						SET_STATIC_PIXEL_SHADER_COMBO( ENVMAPFRESNEL,  bHasEnvMapFresnel && bHasEnvmap );
						SET_STATIC_PIXEL_SHADER_COMBO( SELFILLUM,  bHasSelfIllum );
						SET_STATIC_PIXEL_SHADER_COMBO( VERTEXCOLOR,  bHasVertexColor );
						SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHT,  bHasFlashlight );
						ClampDetailBlendModeAndWarn( nDetailBlendMode, 0, 9 );
						SET_STATIC_PIXEL_SHADER_COMBO( DETAIL_BLEND_MODE, nDetailBlendMode );
						ClampDecalBlendModeAndWarn( nDecalBlendMode, 0, 1 );
						SET_STATIC_PIXEL_SHADER_COMBO( DECAL_BLEND_MODE, bHasDecalTexture ? nDecalBlendMode : 2 );
						SET_STATIC_PIXEL_SHADER_COMBO( TINTMASKTEXTURE, bHasTintMaskTexture );
						SET_STATIC_PIXEL_SHADER_COMBO( SEAMLESS_BASE, bSeamlessBase );
						SET_STATIC_PIXEL_SHADER_COMBO( SEAMLESS_DETAIL, bSeamlessDetail );
						SET_STATIC_PIXEL_SHADER_COMBO( DISTANCEALPHA, bDistanceAlpha );
						SET_STATIC_PIXEL_SHADER_COMBO( DISTANCEALPHAFROMDETAIL, bDistanceAlphaFromDetail );
						SET_STATIC_PIXEL_SHADER_COMBO( SOFT_MASK, bSoftMask );
						SET_STATIC_PIXEL_SHADER_COMBO( OUTLINE, bOutline );
						SET_STATIC_PIXEL_SHADER_COMBO( OUTER_GLOW, bGlow );
						SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode );
						SET_STATIC_PIXEL_SHADER_COMBO( SHADER_SRGB_READ, bShaderSrgbRead );
						SET_STATIC_PIXEL_SHADER_COMBO( DESATURATEWITHBASEALPHA, bDesaturateWithBaseAlpha );
						SET_STATIC_PIXEL_SHADER_COMBO( LIGHTING_PREVIEW, 0 /*nLightingPreviewMode*/ );
						SET_STATIC_PIXEL_SHADER_COMBO( SRGB_INPUT_ADAPTER, bSRGBInputAdapter ? 1 : 0 );
						SET_STATIC_PIXEL_SHADER_COMBO( CASCADED_SHADOW_MAPPING, g_pHardwareConfig->SupportsCascadedShadowMapping() && !bSFM && !bHasFlashlight && !bDisableCSMLookup );
						SET_STATIC_PIXEL_SHADER_COMBO( CSM_MODE, 0 );
						SET_STATIC_PIXEL_SHADER_COMBO( CSM_BLENDING, bCSMBlending );
						SET_STATIC_PIXEL_SHADER( vertexlit_and_unlit_generic_ps20b );
					}
					else // ps_2_0
					{
						DECLARE_STATIC_PIXEL_SHADER( vertexlit_and_unlit_generic_ps20 );
						SET_STATIC_PIXEL_SHADER_COMBO( SFM, bSFM );
						SET_STATIC_PIXEL_SHADER_COMBO( SELFILLUM_ENVMAPMASK_ALPHA, bHasSelfIllumInEnvMapMask ); 
						SET_STATIC_PIXEL_SHADER_COMBO( DETAILTEXTURE,  bHasDetailTexture );
						SET_STATIC_PIXEL_SHADER_COMBO( CUBEMAP,  bHasEnvmap );
						SET_STATIC_PIXEL_SHADER_COMBO( DIFFUSELIGHTING,  hasDiffuseLighting );
						SET_STATIC_PIXEL_SHADER_COMBO( ENVMAPMASK,  bHasEnvmapMask && ( bHasEnvmap || bHasSelfIllumInEnvMapMask ) );
						SET_STATIC_PIXEL_SHADER_COMBO( BASEALPHAENVMAPMASK,  hasBaseAlphaEnvmapMask && bHasEnvmap );
						SET_STATIC_PIXEL_SHADER_COMBO( ENVMAPFRESNEL,  bHasEnvMapFresnel && bHasEnvmap );
						SET_STATIC_PIXEL_SHADER_COMBO( SELFILLUM,  bHasSelfIllum );
						SET_STATIC_PIXEL_SHADER_COMBO( VERTEXCOLOR,  bHasVertexColor );
						SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHT,  bHasFlashlight );
						ClampDetailBlendModeAndWarn( nDetailBlendMode, 0, 9 );
						SET_STATIC_PIXEL_SHADER_COMBO( DETAIL_BLEND_MODE, nDetailBlendMode );
						ClampDecalBlendModeAndWarn( nDecalBlendMode, 0, 1 );
						SET_STATIC_PIXEL_SHADER_COMBO( DECAL_BLEND_MODE, bHasDecalTexture ? nDecalBlendMode : 2 );
						SET_STATIC_PIXEL_SHADER_COMBO( TINTMASKTEXTURE, 0 );
						SET_STATIC_PIXEL_SHADER_COMBO( SEAMLESS_BASE, bSeamlessBase );
						SET_STATIC_PIXEL_SHADER_COMBO( SEAMLESS_DETAIL, bSeamlessDetail );
						SET_STATIC_PIXEL_SHADER_COMBO( DISTANCEALPHA, bDistanceAlpha );
						SET_STATIC_PIXEL_SHADER_COMBO( DISTANCEALPHAFROMDETAIL, bDistanceAlphaFromDetail );
						SET_STATIC_PIXEL_SHADER_COMBO( SOFT_MASK, bSoftMask );
						SET_STATIC_PIXEL_SHADER_COMBO( OUTLINE, bOutline );
						SET_STATIC_PIXEL_SHADER_COMBO( OUTER_GLOW, bGlow );
						SET_STATIC_PIXEL_SHADER_COMBO( SHADER_SRGB_READ, bShaderSrgbRead );
						SET_STATIC_PIXEL_SHADER_COMBO( DESATURATEWITHBASEALPHA, bDesaturateWithBaseAlpha );
						SET_STATIC_PIXEL_SHADER_COMBO( LIGHTING_PREVIEW, 0 /*nLightingPreviewMode*/ );
						SET_STATIC_PIXEL_SHADER_COMBO( CASCADED_SHADOW_MAPPING, 0 );
						SET_STATIC_PIXEL_SHADER_COMBO( CSM_MODE, 0 );
						SET_STATIC_PIXEL_SHADER_COMBO( CSM_BLENDING, bCSMBlending );
						SET_STATIC_PIXEL_SHADER( vertexlit_and_unlit_generic_ps20 );
					}
				}
				#if !defined( _GAMECONSOLE ) && !defined( _PS3 )
				else
				{
					// The vertex shader uses the vertex id stream
					if ( bSFM )
					{
						SET_FLAGS2( MATERIAL_VAR2_USES_VERTEXID );
						SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_TESSELLATION );
					}
					
					int nCSMQualityComboValue = g_pHardwareConfig->GetCSMShaderMode( materials->GetCurrentConfigForVideoCard().GetCSMQualityMode() );
					bool bCSMBlending = g_pHardwareConfig->GetCSMAccurateBlending();
										
					DECLARE_STATIC_VERTEX_SHADER( vertexlit_and_unlit_generic_vs30 );
					SET_STATIC_VERTEX_SHADER_COMBO( SFM, bSFM );
					SET_STATIC_VERTEX_SHADER_COMBO( VERTEXCOLOR, bHasVertexColor || bHasVertexAlpha );
					SET_STATIC_VERTEX_SHADER_COMBO( CUBEMAP, bHasEnvmap );
					SET_STATIC_VERTEX_SHADER_COMBO( HALFLAMBERT, bHalfLambert );
					SET_STATIC_VERTEX_SHADER_COMBO( FLASHLIGHT, bHasFlashlight );
					SET_STATIC_VERTEX_SHADER_COMBO( SEAMLESS_BASE, bSeamlessBase );
					SET_STATIC_VERTEX_SHADER_COMBO( SEAMLESS_DETAIL, bSeamlessDetail );
					SET_STATIC_VERTEX_SHADER_COMBO( SEPARATE_DETAIL_UVS, IsBoolSet( info.m_nSeparateDetailUVs, params ) );
					SET_STATIC_VERTEX_SHADER_COMBO( DECAL, bIsDecal );
					SET_STATIC_VERTEX_SHADER_COMBO( LIGHTING_PREVIEW, nLightingPreviewMode );
					SET_STATIC_VERTEX_SHADER_COMBO( TREESWAY, bTreeSway ? nTreeSwayMode : 0 );	
					SET_STATIC_VERTEX_SHADER_COMBO( CASCADED_SHADOW_MAPPING, 0 );
					SET_STATIC_VERTEX_SHADER_COMBO( CSM_BLENDING, bCSMBlending );
					SET_STATIC_VERTEX_SHADER( vertexlit_and_unlit_generic_vs30 );

					DECLARE_STATIC_PIXEL_SHADER( vertexlit_and_unlit_generic_ps30 );
					SET_STATIC_PIXEL_SHADER_COMBO( SFM, bSFM );
					SET_STATIC_PIXEL_SHADER_COMBO( SELFILLUM_ENVMAPMASK_ALPHA, bHasSelfIllumInEnvMapMask && !bHasFlashlight ); 
					SET_STATIC_PIXEL_SHADER_COMBO( DETAILTEXTURE, bHasDetailTexture );
					SET_STATIC_PIXEL_SHADER_COMBO( CUBEMAP, bHasEnvmap );
					SET_STATIC_PIXEL_SHADER_COMBO( DIFFUSELIGHTING, hasDiffuseLighting );
					SET_STATIC_PIXEL_SHADER_COMBO( ENVMAPMASK, bHasEnvmapMask && !bHasFlashlight && ( bHasEnvmap || bHasSelfIllumInEnvMapMask  ) ); // Careful here if we do single-pass flashlight on PC someday
					SET_STATIC_PIXEL_SHADER_COMBO( BASEALPHAENVMAPMASK,  hasBaseAlphaEnvmapMask && bHasEnvmap );
					SET_STATIC_PIXEL_SHADER_COMBO( ENVMAPFRESNEL,  bHasEnvMapFresnel && bHasEnvmap );
					SET_STATIC_PIXEL_SHADER_COMBO( SELFILLUM, bHasSelfIllum );
					SET_STATIC_PIXEL_SHADER_COMBO( VERTEXCOLOR, bHasVertexColor );
					SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHT, bHasFlashlight );
					ClampDetailBlendModeAndWarn( nDetailBlendMode, 0, 9 );
					SET_STATIC_PIXEL_SHADER_COMBO( DETAIL_BLEND_MODE, nDetailBlendMode );
					ClampDecalBlendModeAndWarn( nDecalBlendMode, 0, 1 );
					SET_STATIC_PIXEL_SHADER_COMBO( DECAL_BLEND_MODE, bHasDecalTexture ? nDecalBlendMode : 2 );
					SET_STATIC_PIXEL_SHADER_COMBO( TINTMASKTEXTURE, bHasTintMaskTexture );
					SET_STATIC_PIXEL_SHADER_COMBO( SEAMLESS_BASE, bSeamlessBase );
					SET_STATIC_PIXEL_SHADER_COMBO( SEAMLESS_DETAIL, bSeamlessDetail );
					SET_STATIC_PIXEL_SHADER_COMBO( DISTANCEALPHA, bDistanceAlpha );
					SET_STATIC_PIXEL_SHADER_COMBO( DISTANCEALPHAFROMDETAIL, bDistanceAlphaFromDetail );
					SET_STATIC_PIXEL_SHADER_COMBO( SOFT_MASK, bSoftMask );
					SET_STATIC_PIXEL_SHADER_COMBO( OUTLINE, bOutline );
					SET_STATIC_PIXEL_SHADER_COMBO( OUTER_GLOW, bGlow );
					SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode );
					SET_STATIC_PIXEL_SHADER_COMBO( SHADER_SRGB_READ, bShaderSrgbRead );
					SET_STATIC_PIXEL_SHADER_COMBO( DESATURATEWITHBASEALPHA, bDesaturateWithBaseAlpha );
					SET_STATIC_PIXEL_SHADER_COMBO( LIGHTING_PREVIEW, 0 /*nLightingPreviewMode*/ );
					SET_STATIC_PIXEL_SHADER_COMBO( CASCADED_SHADOW_MAPPING, g_pHardwareConfig->SupportsCascadedShadowMapping() && !bSFM && !bHasFlashlight && !bDisableCSMLookup );
					SET_STATIC_PIXEL_SHADER_COMBO( CSM_MODE, ( g_pHardwareConfig->SupportsCascadedShadowMapping() && !bSFM && !bHasFlashlight && !bDisableCSMLookup ) ? nCSMQualityComboValue : 0 );
					SET_STATIC_PIXEL_SHADER_COMBO( CSM_BLENDING, bCSMBlending );
					SET_STATIC_PIXEL_SHADER( vertexlit_and_unlit_generic_ps30 );
				}
				#endif
			}

			if ( bHasFlashlight && !bSinglePassFlashlight )
			{
				pShader->FogToBlack();
			}
			else
			{
				pShader->DefaultFog();
			}

			// HACK HACK HACK - enable alpha writes all the time so that we have them for
			// underwater stuff
			pShaderShadow->EnableAlphaWrites( bFullyOpaque );

			if ( IS_FLAG_SET( MATERIAL_VAR_MULTIPLY ) )
			{
				pShader->EnableAlphaBlending( SHADER_BLEND_ZERO, SHADER_BLEND_SRC_COLOR );
			}

		}

		if ( pShaderAPI && ( (! pContextData ) || ( pContextData->m_bMaterialVarsChanged ) ) )
		{
			if ( !pContextData )								// make sure allocated
			{
				pContextData = new CVertexLitGeneric_DX9_Context;
				*pContextDataPtr = pContextData;
			}
			pContextData->m_bMaterialVarsChanged = false;
			pContextData->m_SemiStaticCmdsOut.Reset();
#ifndef _PS3
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderFogParams( 21 );
#endif

#ifdef _PS3	
		    pContextData->m_flashlightECB.Reset();
#endif
			bool bSampler0SrgbRead = false;
			if ( bHasBaseTexture )
			{
				if ( ( info.m_nGammaColorRead != -1 ) && ( params[info.m_nGammaColorRead]->GetIntValue() == 1 ) )
					bSampler0SrgbRead = false;
				else
					bSampler0SrgbRead = !bShaderSrgbRead;
			}

			if ( bHasBaseTexture )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER0, bSampler0SrgbRead ? TEXTURE_BINDFLAGS_SRGBREAD : TEXTURE_BINDFLAGS_NONE, info.m_nBaseTexture, info.m_nBaseTextureFrame );
			}
			else
			{
				if( bHasEnvmap )
				{
					// if we only have an envmap (no basetexture), then we want the albedo to be black.
					pContextData->m_SemiStaticCmdsOut.BindStandardTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, TEXTURE_BLACK );
				}
				else
				{
					pContextData->m_SemiStaticCmdsOut.BindStandardTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, TEXTURE_WHITE );
				}
			}
			if ( bHasDetailTexture )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER2, IsSRGBDetailTexture( nDetailBlendMode ) ? TEXTURE_BINDFLAGS_SRGBREAD : TEXTURE_BINDFLAGS_NONE, info.m_nDetail, info.m_nDetailFrame );
			}
			if ( bHasSelfIllum )
			{
				if ( bHasSelfIllumMask )												// Separate texture for self illum?
				{
					pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER11, TEXTURE_BINDFLAGS_NONE, info.m_nSelfIllumMask, -1 );	// Bind it
				}
				else																	// else
				{
					pContextData->m_SemiStaticCmdsOut.BindStandardTexture( SHADER_SAMPLER11, TEXTURE_BINDFLAGS_NONE, TEXTURE_BLACK );	// Bind dummy
				}
			}
			if ( bHasDecalTexture )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER12, IsSRGBDecalTexture( nDetailBlendMode ) ? TEXTURE_BINDFLAGS_SRGBREAD : TEXTURE_BINDFLAGS_NONE, info.m_nDecalTexture );
			}
			if ( bHasTintMaskTexture )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER13, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nTintMaskTexture );
			}
			if ( bSeamlessDetail || bSeamlessBase )
			{
				float flSeamlessData[4] = { params[info.m_nSeamlessScale]->GetFloatValue(), 0, 0, 0 };
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3, flSeamlessData );
			}
			
			if ( bTreeSway )
			{
				float flParams[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

				flParams[0] = GetFloatParam( info.m_nTreeSwaySpeedHighWindMultiplier, params, 2.0f );
				flParams[1] = GetFloatParam( info.m_nTreeSwayScrumbleFalloffExp, params, 1.0f );
				flParams[2] = GetFloatParam( info.m_nTreeSwayFalloffExp, params, 1.0f );
				flParams[3] = GetFloatParam( info.m_nTreeSwayScrumbleSpeed, params, 3.0f );
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3, flParams );
			}

			if ( info.m_nBaseTextureTransform != -1 )
			{
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, info.m_nBaseTextureTransform );
			}
	
			if ( bHasBump && ( info.m_nBumpTransform != -1 ) )
			{
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_2, info.m_nBumpTransform );
			}

			int nLightingPreviewMode = ShaderApiFast( pShaderAPI )->GetIntRenderingParameter( INT_RENDERPARM_ENABLE_FIXED_LIGHTING );
			nLightingPreviewMode = clamp( nLightingPreviewMode, 0, 1 );

			if ( ( nLightingPreviewMode == ENABLE_FIXED_LIGHTING_OUTPUTNORMAL_AND_DEPTH ) && IsPC() )
			{
				float vEyeDir[4];
				ShaderApiFast( pShaderAPI )->GetWorldSpaceCameraDirection( vEyeDir );

				float flFarZ = ShaderApiFast( pShaderAPI )->GetFarZ();
				vEyeDir[0] /= flFarZ;	// Divide by farZ for SSAO algorithm
				vEyeDir[1] /= flFarZ;
				vEyeDir[2] /= flFarZ;
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderConstant( 26, vEyeDir );	// Needed for SSAO
			}
		
			if ( bHasDetailTexture )
			{
				if ( !bTreeSway )
				{
					if ( IS_PARAM_DEFINED( info.m_nDetailTextureTransform ) )
					{
						pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureScaledTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_4, info.m_nDetailTextureTransform, info.m_nDetailScale );
					}
					else
					{
						pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureScaledTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_4, info.m_nBaseTextureTransform, info.m_nDetailScale );
					}
				}

				//Assert( !bHasBump );
				if ( info.m_nDetailTint  != -1 )
					pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstantGammaToLinear( 10, info.m_nDetailTint );
				else
				{
					pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant4( 10, 1, 1, 1, 1 );
				}
			}
			
			if ( bTreeSway )
			{
				float32 flParams[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				flParams[0] = GetFloatParam( info.m_nTreeSwaySpeedLerpStart, params, 3.0f );
				flParams[1] = GetFloatParam( info.m_nTreeSwaySpeedLerpEnd, params, 6.0f );
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_4, flParams );
			}
			
			if ( bDistanceAlpha )
			{
				float flSoftStart = GetFloatParam( info.m_nEdgeSoftnessStart, params );
				float flSoftEnd = GetFloatParam( info.m_nEdgeSoftnessEnd, params );
				// set all line art shader parms
				bool bScaleEdges = IsBoolSet( info.m_nScaleEdgeSoftnessBasedOnScreenRes, params );
				bool bScaleOutline = IsBoolSet( info.m_nScaleOutlineSoftnessBasedOnScreenRes, params );

				float flResScale = 1.0;

				float flOutlineStart0 = GetFloatParam( info.m_nOutlineStart0, params );
				float flOutlineStart1 = GetFloatParam( info.m_nOutlineStart1, params );
				float flOutlineEnd0 = GetFloatParam( info.m_nOutlineEnd0, params );
				float flOutlineEnd1 = GetFloatParam( info.m_nOutlineEnd1, params );

				if ( bScaleEdges || bScaleOutline )
				{
					int nWidth, nHeight;
					ShaderApiFast( pShaderAPI )->GetBackBufferDimensions( nWidth, nHeight );
					flResScale=MAX( 0.5, MAX( 1024.0/nWidth, 768/nHeight ) );
				
					if ( bScaleEdges )
					{
						float flMid = 0.5 * ( flSoftStart + flSoftEnd );
						flSoftStart = clamp( flMid + flResScale * ( flSoftStart - flMid ), 0.05, 0.99 );
						flSoftEnd = clamp( flMid + flResScale * ( flSoftEnd - flMid ), 0.05, 0.99 );
					}
				

					if ( bScaleOutline )
					{
						// shrink the soft part of the outline, enlarging hard part
						float flMidS = 0.5 * ( flOutlineStart1 + flOutlineStart0 );
						flOutlineStart1 = clamp( flMidS + flResScale * ( flOutlineStart1 - flMidS ), 0.05, 0.99 );
						float flMidE = 0.5 * ( flOutlineEnd1 + flOutlineEnd0 );
						flOutlineEnd1 = clamp( flMidE + flResScale * ( flOutlineEnd1 - flMidE ), 0.05, 0.99 );
					}
					
				}

				float flConsts[]={
					// c5 - glow values
					GetFloatParam( info.m_nGlowX, params ),
					GetFloatParam( info.m_nGlowY, params ),
					GetFloatParam( info.m_nGlowStart, params ),
					GetFloatParam( info.m_nGlowEnd, params ),
					// c6 - glow color
					0,0,0,										// will be filled in
					GetFloatParam( info.m_nGlowAlpha, params ),
					// c7 - mask range parms and basealphaenvmapmask scale and bias
					flSoftStart,
					flSoftEnd,
					0,0,	// filled in below
					// c8 - outline color
					0,0,0,
					GetFloatParam( info.m_nOutlineAlpha, params ),
					// c9 - outline parms
					flOutlineStart0,
					flOutlineStart1,
					flOutlineEnd0,
					flOutlineEnd1,
				};

				if ( info.m_nGlowColor != -1 )
				{
					params[info.m_nGlowColor]->GetVecValue( flConsts+4, 3 );
				}
				if ( info.m_nOutlineColor != -1 )
				{
					params[info.m_nOutlineColor]->GetVecValue( flConsts+12, 3 );
				}
				if ( info.m_nBaseAlphaEnvMapMaskMinMaxExp != -1 )
				{
					flConsts[10] = params[ info.m_nBaseAlphaEnvMapMaskMinMaxExp ]->GetVecValue()[0];
					flConsts[11] = params[ info.m_nBaseAlphaEnvMapMaskMinMaxExp ]->GetVecValue()[1] - flConsts[10];
				}
				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 5, flConsts, 5 );

			}
			else if ( info.m_nBaseAlphaEnvMapMaskMinMaxExp != -1 )
			{
				float flConsts[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				
				flConsts[2] = params[ info.m_nBaseAlphaEnvMapMaskMinMaxExp ]->GetVecValue()[0];
				flConsts[3] = params[ info.m_nBaseAlphaEnvMapMaskMinMaxExp ]->GetVecValue()[1] - flConsts[2];
				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 7, flConsts, 1 );
			}

			if ( !g_pConfig->m_bFastNoBump )
			{
				if ( bHasBump )
				{
					pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, info.m_nBumpmap, info.m_nBumpFrame );
				}
				else if ( bHasDiffuseWarp )
				{
					pContextData->m_SemiStaticCmdsOut.BindStandardTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, TEXTURE_NORMALMAP_FLAT );
				}
			}
			else
			{
				if ( bHasBump )
				{
					pContextData->m_SemiStaticCmdsOut.BindStandardTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, TEXTURE_NORMALMAP_FLAT );
				}
			}
			// Setting w to 1 means use separate selfillummask
			float vEnvMapSaturation_SelfIllumMask[4] = {1.0f, 1.0f, 1.0f, 0.0f};
			if ( info.m_nEnvmapSaturation != -1 )
				params[info.m_nEnvmapSaturation]->GetVecValue( vEnvMapSaturation_SelfIllumMask, 3 );
			
			vEnvMapSaturation_SelfIllumMask[3] = bHasSelfIllumMask ? 1.0f : 0.0f;
			if ( bHasBump )
			{
				// Bump version of the shader doesn't use self illum mask. Use w to store decal texture multiplier instead to support mod2x decal texture blending.
				vEnvMapSaturation_SelfIllumMask[ 3 ] = ( nDecalBlendMode == 2 ) ? 2.0f : 1.0f;
			}
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 3, vEnvMapSaturation_SelfIllumMask, 1 );
			if ( bHasEnvmap )
			{
				pContextData->m_SemiStaticCmdsOut.SetEnvMapTintPixelShaderDynamicStateGammaToLinear( 0, info.m_nEnvmapTint, fSinglePassFlashlight );
			}
			else
			{
				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant4( 0, 0.0f, 0.0f, 0.0f, fSinglePassFlashlight );
			}
			bool bHasEnvmapMask = (!bHasFlashlight || bSinglePassFlashlight ) && ( info.m_nEnvmapMask != -1 ) && params[info.m_nEnvmapMask]->IsTexture();
			if ( bHasEnvmapMask )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER4, TEXTURE_BINDFLAGS_NONE, info.m_nEnvmapMask, info.m_nEnvmapMaskFrame );
			}

			bool bHasEnvMapFresnel = bHasEnvmap && IsBoolSet( info.m_nEnvmapFresnel, params );
			if ( bHasEnvMapFresnel )
			{
				float flConsts[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				params[ info.m_nEnvMapFresnelMinMaxExp ]->GetVecValue( flConsts, 3 );
				flConsts[1] -= flConsts[0];	// convert max fresnel into scale factor

				if ( info.m_nBaseAlphaEnvMapMaskMinMaxExp != -1 )
				{
					flConsts[3] = params[ info.m_nBaseAlphaEnvMapMaskMinMaxExp ]->GetVecValue()[2];		// basealphaenvmapmask exponent in w
				}

				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 13, flConsts, 1 );
			}
			else if ( info.m_nBaseAlphaEnvMapMaskMinMaxExp != -1 )
			{
				// still need to set exponent for basealphaenvmapmask
				float flConsts[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				flConsts[3] = params[ info.m_nBaseAlphaEnvMapMaskMinMaxExp ]->GetVecValue()[2];		// basealphaenvmapmask exponent in w
				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 13, flConsts, 1 );
			}

			bool bHasSelfIllumFresnel = ( !bHasDetailTexture ) && ( bHasSelfIllum ) && ( info.m_nSelfIllumFresnel != -1 ) && ( params[info.m_nSelfIllumFresnel]->GetIntValue() != 0 );

			if ( bHasSelfIllumFresnel && (!bHasFlashlight || bSinglePassFlashlight) )
			{
				float vConstScaleBiasExp[4] = { 1.0f, 0.0f, 1.0f, 0.0f };
				float flMin = IS_PARAM_DEFINED( info.m_nSelfIllumFresnelMinMaxExp ) ? params[info.m_nSelfIllumFresnelMinMaxExp]->GetVecValue()[0] : 0.0f;
				float flMax = IS_PARAM_DEFINED( info.m_nSelfIllumFresnelMinMaxExp ) ? params[info.m_nSelfIllumFresnelMinMaxExp]->GetVecValue()[1] : 1.0f;
				float flExp = IS_PARAM_DEFINED( info.m_nSelfIllumFresnelMinMaxExp ) ? params[info.m_nSelfIllumFresnelMinMaxExp]->GetVecValue()[2] : 1.0f;

				vConstScaleBiasExp[1] = ( flMax != 0.0f ) ? ( flMin / flMax ) : 0.0f; // Bias
				vConstScaleBiasExp[0] = 1.0f - vConstScaleBiasExp[1]; // Scale
				vConstScaleBiasExp[2] = flExp; // Exp
				vConstScaleBiasExp[3] = flMax; // Brightness

				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 11, vConstScaleBiasExp );
			}
			else
			{
				float vSelfIllumScale[4];
				vSelfIllumScale[0] = IS_PARAM_DEFINED( info.m_nSelfIllumMaskScale ) ? params[info.m_nSelfIllumMaskScale]->GetFloatValue() : 1.0f;
				vSelfIllumScale[ 1 ] = bIsDecal ? 1.0f : 0.0f;
				vSelfIllumScale[2] = vSelfIllumScale[3] = 0.0f;

				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 11, vSelfIllumScale );
			}

			// store eye pos in shader constant 20
			float flEyeW = pShader->TextureIsTranslucent( BASETEXTURE, true ) ? 1.0f : 0.0f;
			pContextData->m_SemiStaticCmdsOut.StoreEyePosInPixelShaderConstant( 20, flEyeW );

			if( bHasDiffuseWarp && (!bHasFlashlight || bSinglePassFlashlight) && !bHasSelfIllumFresnel )
			{
				if ( r_lightwarpidentity.GetBool() )
				{
					pContextData->m_SemiStaticCmdsOut.BindStandardTexture( SHADER_SAMPLER9, TEXTURE_BINDFLAGS_NONE, TEXTURE_IDENTITY_LIGHTWARP );
				}
				else
				{
					pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER9, TEXTURE_BINDFLAGS_NONE, info.m_nDiffuseWarpTexture, -1  );
				}
			}

			if ( bHasFlashlight )
			{
#ifdef _PS3
				{
					pContextData->m_flashlightECB.SetVertexShaderFlashlightState( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6 );
				}
#endif
				if( IsX360() || !bHasBump )
				{
					pContextData->m_SemiStaticCmdsOut.SetVertexShaderFlashlightState( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6 );
				}


				CBCmdSetPixelShaderFlashlightState_t state;
				state.m_LightSampler = SHADER_SAMPLER7;
				state.m_DepthSampler = SHADER_SAMPLER8;
				state.m_ShadowNoiseSampler = SHADER_SAMPLER6;
				state.m_nColorConstant = 28;
				state.m_nAttenConstant = 22;
				state.m_nOriginConstant = 23;
				state.m_nDepthTweakConstant = 2;
				state.m_nScreenScaleConstant = 31;
				state.m_nWorldToTextureConstant = ( IsX360() || IsPS3() ) ? -1 : 24;
				state.m_bFlashlightNoLambert = bFlashlightNoLambert;
				state.m_bSinglePassFlashlight = bSinglePassFlashlight;

#ifdef _PS3
				{
					pContextData->m_flashlightECB.SetPixelShaderFlashlightState( state );
					pContextData->m_flashlightECB.End();
				}
#else

				{
					pContextData->m_SemiStaticCmdsOut.SetPixelShaderFlashlightState( state );
				}
#endif

				if ( !( IsX360() || IsPS3() ) && ( g_pHardwareConfig->GetDXSupportLevel() > 92 ) )
				{
					pContextData->m_SemiStaticCmdsOut.SetPixelShaderUberLightState( 
						PSREG_UBERLIGHT_SMOOTH_EDGE_0,		PSREG_UBERLIGHT_SMOOTH_EDGE_1,
						PSREG_UBERLIGHT_SMOOTH_EDGE_OOW,	PSREG_UBERLIGHT_SHEAR_ROUND, 
						PSREG_UBERLIGHT_AABB,				PSREG_UBERLIGHT_WORLD_TO_LIGHT );
				}
			}

			if ( bHasEnvmap && ( !bHasFlashlight || IsGameConsole() ) )
			{
				// This was shader reg 2, but this was conflicting with the flashlight state and we somehow never noticed it.
				// fvEnvmapParams are contrast, lightscale, lightscale min, lightscale max
				// If contrast is less than 0 it's invalid, but the shader is expecting something, so just set 1.0 (specular squared, the darkest).
				float envMapParams[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
				if ( info.m_nEnvmapContrast > 0 )
				{
					// The shader is expecting something, so just set 1.0 (specular squared, the darkest).
					envMapParams[0] = GetFloatParam( info.m_nEnvmapContrast, params );
				}
				
				/* The following (envmapparmas[1,2,3] are no longer used (c19.yzw) in the bump shader so we're hijacking them for the CSM light color
				   They're the only free constants for OSX/ps2b in that PS */

				if ( info.m_nEnvMapLightScale != -1 )
				{
					envMapParams[1] = GetFloatParam( info.m_nEnvMapLightScale, params );
				}
				if ( info.m_nEnvMapLightScaleMinMax != -1 )
				{
					float lightScaleMinMax[2] = { 0.0, 0.0 };
					params[info.m_nEnvMapLightScaleMinMax]->GetVecValue( lightScaleMinMax, 2 );
					envMapParams[2] = lightScaleMinMax[0];
					envMapParams[3] = lightScaleMinMax[1] + lightScaleMinMax[0];

				}

				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 19, envMapParams );
			}

			// mat_fullbright 2 handling
			bool bLightingOnly = bVertexLitGeneric && g_pConfig->nFullbright == 2 && !IS_FLAG_SET( MATERIAL_VAR_NO_DEBUG_OVERRIDE );
			if( bLightingOnly )
			{
				if ( bHasBaseTexture )
				{
					if( ( bHasSelfIllum && !bHasSelfIllumInEnvMapMask )  )
					{
						pContextData->m_SemiStaticCmdsOut.BindStandardTexture( SHADER_SAMPLER0, bSampler0SrgbRead ? TEXTURE_BINDFLAGS_SRGBREAD : TEXTURE_BINDFLAGS_NONE, TEXTURE_GREY_ALPHA_ZERO );
					}
					else
					{
						pContextData->m_SemiStaticCmdsOut.BindStandardTexture( SHADER_SAMPLER0, bSampler0SrgbRead ? TEXTURE_BINDFLAGS_SRGBREAD : TEXTURE_BINDFLAGS_NONE, TEXTURE_GREY );
					}
				}
				if ( bHasDetailTexture )
				{
					pContextData->m_SemiStaticCmdsOut.BindStandardTexture( SHADER_SAMPLER2, IsSRGBDetailTexture( nDetailBlendMode ) ? TEXTURE_BINDFLAGS_SRGBREAD : TEXTURE_BINDFLAGS_NONE, TEXTURE_GREY );
				}
				if ( bHasDecalTexture )
				{
					pContextData->m_SemiStaticCmdsOut.BindStandardTexture( SHADER_SAMPLER12, IsSRGBDecalTexture( nDecalBlendMode ) ? TEXTURE_BINDFLAGS_SRGBREAD : TEXTURE_BINDFLAGS_NONE, TEXTURE_GREY );
				}
				if ( bHasTintMaskTexture )
				{
					pContextData->m_SemiStaticCmdsOut.BindStandardTexture( SHADER_SAMPLER13, TEXTURE_BINDFLAGS_SRGBREAD, TEXTURE_GREY );
				}
			}
			
			if ( bHasBump || bHasDiffuseWarp )
			{
				pContextData->m_SemiStaticCmdsOut.BindStandardTexture( SHADER_SAMPLER5, TEXTURE_BINDFLAGS_NONE, TEXTURE_NORMALIZATION_CUBEMAP_SIGNED );
			}

			if ( bTreeSway )
			{
				float flParams[4];
				flParams[0] = GetFloatParam( info.m_nTreeSwayHeight, params, 1000.0f );
				flParams[1] = GetFloatParam( info.m_nTreeSwayStartHeight, params, 0.1f );
				flParams[2] = GetFloatParam( info.m_nTreeSwayRadius, params, 300.0f );
				flParams[3] = GetFloatParam( info.m_nTreeSwayStartRadius, params, 0.2f );
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_10, flParams );

				flParams[0] = GetFloatParam( info.m_nTreeSwaySpeed, params, 1.0f );
				flParams[1] = GetFloatParam( info.m_nTreeSwayStrength, params, 10.0f );
				flParams[2] = GetFloatParam( info.m_nTreeSwayScrumbleFrequency, params, 12.0f );
				flParams[3] = GetFloatParam( info.m_nTreeSwayScrumbleStrength, params, 10.0f );
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_11, flParams );
			}

			if ( bVertexLitGeneric )
			{
				if ( bDesaturateWithBaseAlpha )
				{
					pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant_W( 4, info.m_nDesaturateWithBaseAlpha, fBlendFactor );
				}
				else
				{
					pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant_W( 4, info.m_nSelfIllumTint, fBlendFactor );
				}
			}
			else
			{
				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant4( 4, 1.0f, 1.0f, 1.0f, fBlendFactor );
			}
			pContextData->m_SemiStaticCmdsOut.End();
		}
	}
	if ( pShaderAPI )
	{

#ifdef _PS3
		CCommandBufferBuilder< CDynamicCommandStorageBuffer > DynamicCmdsOut;
		ShaderApiFast( pShaderAPI )->ExecuteCommandBuffer( pContextData->m_SemiStaticCmdsOut.Base() );
		if (bHasFlashlight) ShaderApiFast( pShaderAPI )->ExecuteCommandBufferPPU( pContextData->m_flashlightECB.Base() );

#else
		CCommandBufferBuilder< CFixedCommandStorageBuffer< 1000 > > DynamicCmdsOut;
		DynamicCmdsOut.Call( pContextData->m_SemiStaticCmdsOut.Base() );
#endif

		if ( bHasEnvmap )
		{
			bool bHdr = ( g_pHardwareConfig->GetHDRType() != HDR_TYPE_NONE );
			DynamicCmdsOut.BindEnvCubemapTexture( pShader, SHADER_SAMPLER1, bHdr ? TEXTURE_BINDFLAGS_NONE : TEXTURE_BINDFLAGS_SRGBREAD, info.m_nEnvmap, info.m_nEnvmapFrame );
		}

		bool bFlashlightShadows = false;
		bool bUberlight = false;
		if ( bHasFlashlight )
		{
			ShaderApiFast( pShaderAPI )->GetFlashlightShaderInfo( &bFlashlightShadows, &bUberlight );
			if ( g_pHardwareConfig->GetDXSupportLevel() <= 92 )
			{
				bUberlight = false;
			}
		}

		// Set up light combo state
		LightState_t lightState = {0, false, false};
		if ( bVertexLitGeneric && (!bHasFlashlight || bSinglePassFlashlight) )
		{
			ShaderApiFast( pShaderAPI )->GetDX9LightState( &lightState );
		}

		MaterialFogMode_t fogType = ShaderApiFast( pShaderAPI )->GetSceneFogMode();
		int numBones = ShaderApiFast( pShaderAPI )->GetCurrentNumBones();

		bool bWriteDepthToAlpha;
		bool bWriteWaterFogToAlpha;
		if( bFullyOpaque ) 
		{
			bWriteDepthToAlpha = ShaderApiFast( pShaderAPI )->ShouldWriteDepthToDestAlpha();
			bWriteWaterFogToAlpha = (fogType == MATERIAL_FOG_LINEAR_BELOW_FOG_Z);
			AssertMsg( !(bWriteDepthToAlpha && bWriteWaterFogToAlpha), "Can't write two values to alpha at the same time." );
		}
		else
		{
			//can't write a special value to dest alpha if we're actually using as-intended alpha
			bWriteDepthToAlpha = false;
			bWriteWaterFogToAlpha = false;
		}

#if !defined( _GAMECONSOLE ) && !defined( _PS3 )
		bool bWorldNormal = ShaderApiFast( pShaderAPI )->GetIntRenderingParameter( INT_RENDERPARM_ENABLE_FIXED_LIGHTING ) == ENABLE_FIXED_LIGHTING_OUTPUTNORMAL_AND_DEPTH;

		TessellationMode_t nTessellationMode = ( bSFM && g_pHardwareConfig->HasFastVertexTextures() ) ? ShaderApiFast( pShaderAPI )->GetTessellationMode() : TESSELLATION_MODE_DISABLED;
		if ( nTessellationMode != TESSELLATION_MODE_DISABLED )
		{
			ShaderApiFast( pShaderAPI )->BindStandardVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER1, TEXTURE_SUBDIVISION_PATCHES );

			float vSubDDimensions[4] = { 1.0f/ShaderApiFast( pShaderAPI )->GetSubDHeight(), bHasDisplacement && mat_displacementmap.GetBool() ? 1.0f : 0.0f, 0.0f, 0.0f };
			ShaderApiFast( pShaderAPI )->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3, vSubDDimensions );

			if( bHasDisplacement )
			{
				pShader->BindVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER2, info.m_nDisplacementMap );
			}
			else
			{
				ShaderApiFast( pShaderAPI )->BindStandardVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER2, TEXTURE_BLACK );
			}

			// Currently, tessellation is mutually exclusive with any kind of GPU-side skinning, morphing or vertex compression
			Assert( !ShaderApiFast( pShaderAPI )->IsHWMorphingEnabled() );
			Assert( numBones == 0 );
			Assert( vertexCompression == 0);
			
			// Also mutually exclusive with these in the non-bump case:
			//$STATICLIGHT3 || $VERTEXCOLOR || $SEAMLESS_BASE || $SEAMLESS_DETAIL || $SEPARATE_DETAIL_UVS
		}
#endif

		int nLightingPreviewMode = ShaderApiFast( pShaderAPI )->GetIntRenderingParameter( INT_RENDERPARM_ENABLE_FIXED_LIGHTING );
		nLightingPreviewMode = clamp( nLightingPreviewMode, 0, 1 );

		if ( ( nLightingPreviewMode == ENABLE_FIXED_LIGHTING_OUTPUTNORMAL_AND_DEPTH ) && IsPC() )
		{
			float vEyeDir[4];
			ShaderApiFast( pShaderAPI )->GetWorldSpaceCameraDirection( vEyeDir );

			float flFarZ = ShaderApiFast( pShaderAPI )->GetFarZ();
			vEyeDir[0] /= flFarZ;	// Divide by farZ for SSAO algorithm
			vEyeDir[1] /= flFarZ;
			vEyeDir[2] /= flFarZ;
			ShaderApiFast( pShaderAPI )->SetVertexShaderConstant( 26, vEyeDir );
		}
		else if ( !bHasBump )
		{
			ITexture *pDepthTextureAtlas = NULL;
			const CascadedShadowMappingState_t &cascadeState = pShaderAPI->GetCascadedShadowMappingState( &pDepthTextureAtlas );

			float vConst[ 4 ] = { lightState.m_bStaticLight ? 1.0f : 0.0f, cascadeState.m_vLightColor.x, cascadeState.m_vLightColor.y, cascadeState.m_vLightColor.z };
			ShaderApiFast( pShaderAPI )->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_2, vConst );
		}

		bool bBlendWithSmokeGrenade = IsBoolSet( info.m_nBlendWithSmokeGrenade, params );
		if ( bBlendWithSmokeGrenade )
		{

			//float vSmokeGrenadePosEntity[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			//params[ info.m_nBlendWithSmokeGrenadePosEntity ]->GetVecValue( vSmokeGrenadePosEntity, 3 );

			float vSmokeGrenadePosSmoke[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			params[ info.m_nBlendWithSmokeGrenadePosSmoke ]->GetVecValue( vSmokeGrenadePosSmoke, 3 );

			if ( vSmokeGrenadePosSmoke[0] == 0 && vSmokeGrenadePosSmoke[1] == 0 && vSmokeGrenadePosSmoke[2] == 0 )
			{
				bBlendWithSmokeGrenade = false;
			}
			else
			{
				DynamicCmdsOut.SetPixelShaderConstant( 6, vSmokeGrenadePosSmoke, 1 );
			}

			//float vSmokeDistanceRelativeToEntity[4] = {
			//	vSmokeGrenadePosEntity[0] - vSmokeGrenadePosSmoke[0],
			//	vSmokeGrenadePosEntity[1] - vSmokeGrenadePosSmoke[1],
			//	vSmokeGrenadePosEntity[2] - vSmokeGrenadePosSmoke[2],
			//	0 };

			//DynamicCmdsOut.SetPixelShaderConstant( 13, vSmokeDistanceRelativeToEntity, 1 ); //bashing g_FresnelConstants?

		}

		bool bStaticLight3Streams = (r_staticlight_streams.GetInt() == 3) && (!(bHasVertexColor || bHasVertexAlpha));

		if ( bHasBump || bHasDiffuseWarp )
		{
			bool bCSMEnabled = pShaderAPI->IsCascadedShadowMapping() && !bHasFlashlight && !bSFM && !bDisableCSMLookup;
			if ( ( !( IsGameConsole() ) ) &&
				 ( !bSupportsSM3 || ToolsEnabled() ) )
			{
				bCSMEnabled = false;
			}

			bool bStaticLight3 = lightState.m_bStaticLight && bStaticLight3Streams;
			int32 nStaticLightVsCombo = bStaticLight3 ? ( !r_staticlight_streams_indirect_only.GetBool() ? 2 : 1 ) : 0;

			#if !defined( _GAMECONSOLE ) && !defined( _PS3 )
			if ( !bSupportsSM3 )
			#endif
			{
				DECLARE_DYNAMIC_VERTEX_SHADER( vertexlit_and_unlit_generic_bump_vs20 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING,  numBones > 0 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( TESSELLATION, 0 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( NUM_LIGHTS, bUseStaticControlFlow ? 0 : ( lightState.m_nNumLights ) );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( STATICLIGHT3, nStaticLightVsCombo );
				SET_DYNAMIC_VERTEX_SHADER_CMD( DynamicCmdsOut, vertexlit_and_unlit_generic_bump_vs20 );

				// Bind ps_2_b shader so we can get shadow mapping...
				if ( g_pHardwareConfig->SupportsPixelShaders_2_b() || g_pHardwareConfig->ShouldAlwaysUseShaderModel2bShaders() ) // Always send OpenGL this way
				{
					if ( bCSMEnabled )
					{
 						ITexture *pDepthTextureAtlas = NULL;
 						const CascadedShadowMappingState_t &cascadeState = pShaderAPI->GetCascadedShadowMappingState( &pDepthTextureAtlas );
 
						if ( pDepthTextureAtlas )
						{
 							//DynamicCmdsOut.BindTexture( pShader, SHADER_SAMPLER15, TEXTURE_BINDFLAGS_SHADOWDEPTH, pDepthTextureAtlas, 0 );
                            DynamicCmdsOut.SetPixelShaderConstant( 64, &cascadeState.m_vLightColor.x, CASCADED_SHADOW_MAPPING_CONSTANT_BUFFER_SIZE );
						}
						else
						{
							bCSMEnabled = 0;
						}
					}

					DECLARE_DYNAMIC_PIXEL_SHADER( vertexlit_and_unlit_generic_bump_ps20b );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( NUM_LIGHTS, ( IsPS3() ) ? MIN( 2, lightState.m_nNumLights ) : lightState.m_nNumLights );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( AMBIENT_LIGHT, lightState.m_bAmbientLight && !bStaticLight3 );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHTSHADOWS, bFlashlightShadows );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( CASCADE_SIZE, ( IsGameConsole() ) ? ( bCSMEnabled ? 1 : 0 ) : 0 );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( STATICLIGHT3, bStaticLight3 );
					SET_DYNAMIC_PIXEL_SHADER_CMD( DynamicCmdsOut, vertexlit_and_unlit_generic_bump_ps20b );
				}
				else
				{
					DECLARE_DYNAMIC_PIXEL_SHADER( vertexlit_and_unlit_generic_bump_ps20 );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( NUM_LIGHTS, lightState.m_nNumLights );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( AMBIENT_LIGHT, lightState.m_bAmbientLight && !bStaticLight3 );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( CASCADE_SIZE, 0 );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( STATICLIGHT3, bStaticLight3 );
					SET_DYNAMIC_PIXEL_SHADER_CMD( DynamicCmdsOut, vertexlit_and_unlit_generic_bump_ps20 );
				}
			}
			#if !defined( _GAMECONSOLE ) && !defined( _PS3 )
			else
			{
				if ( bMorphing )
				{
					pShader->SetHWMorphVertexShaderState( VERTEX_SHADER_SHADER_SPECIFIC_CONST_10, VERTEX_SHADER_SHADER_SPECIFIC_CONST_11, SHADER_VERTEXTEXTURE_SAMPLER0 );
				}

				if ( bCSMEnabled )
				{
					ITexture *pDepthTextureAtlas = NULL;
					const CascadedShadowMappingState_t &cascadeState = pShaderAPI->GetCascadedShadowMappingState( &pDepthTextureAtlas );

					DynamicCmdsOut.BindTexture( pShader, SHADER_SAMPLER15, TEXTURE_BINDFLAGS_SHADOWDEPTH, pDepthTextureAtlas, 0 );

					DynamicCmdsOut.SetPixelShaderConstant( 64, &cascadeState.m_vLightColor.x, CASCADED_SHADOW_MAPPING_CONSTANT_BUFFER_SIZE );
				}
				
				BOOL bCSMEnabledBool = bCSMEnabled;						
				pShaderAPI->SetBooleanPixelShaderConstant( 0, &bCSMEnabledBool, 1 );

				DECLARE_DYNAMIC_VERTEX_SHADER( vertexlit_and_unlit_generic_bump_vs30 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, ( numBones > 0 ) && ( nTessellationMode == TESSELLATION_MODE_DISABLED ) );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression  && ( nTessellationMode == TESSELLATION_MODE_DISABLED ) );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( TESSELLATION, nTessellationMode );
				//SET_DYNAMIC_VERTEX_SHADER_COMBO( MORPHING, bMorphing );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( STATICLIGHT3, nStaticLightVsCombo );
				SET_DYNAMIC_VERTEX_SHADER( vertexlit_and_unlit_generic_bump_vs30 );

				DECLARE_DYNAMIC_PIXEL_SHADER( vertexlit_and_unlit_generic_bump_ps30 );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( NUM_LIGHTS, bWorldNormal ? 0 : lightState.m_nNumLights );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( AMBIENT_LIGHT, bWorldNormal ? false : ( lightState.m_bAmbientLight && !bStaticLight3 ) );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHTSHADOWS, bWorldNormal ? 0 : bFlashlightShadows );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( UBERLIGHT, bUberlight && bSFM );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( CASCADE_SIZE, 0 );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( STATICLIGHT3, bStaticLight3 );
				SET_DYNAMIC_PIXEL_SHADER_CMD( DynamicCmdsOut, vertexlit_and_unlit_generic_bump_ps30 );

				bool bUnusedTexCoords[3] = { false, false, !ShaderApiFast( pShaderAPI )->IsHWMorphingEnabled() || !bIsDecal };
				ShaderApiFast( pShaderAPI )->MarkUnusedVertexFields( 0, 3, bUnusedTexCoords );
			}
			#endif
		}
		else // !( bHasBump || bHasDiffuseWarp )
		{
			if ( bAmbientOnly )	// Override selected light combo to be ambient only
			{
				lightState.m_bAmbientLight = true;
				lightState.m_bStaticLight = false;
				lightState.m_nNumLights = 0;
			}

			bool bCSMEnabled = pShaderAPI->IsCascadedShadowMapping() && !bHasFlashlight && !bSFM && !bDisableCSMLookup;
			if ( ( !( IsGameConsole() ) ) &&
				 ( !bSupportsSM3 || ToolsEnabled() ) )
			{
				bCSMEnabled = false;
			}

			#if !defined( _GAMECONSOLE ) && !defined( _PS3 )
			if ( !bSupportsSM3 )
			#endif
			{
				int staticLight3VSCombo = (lightState.m_bStaticLight && bStaticLight3Streams) ? ( ( lightState.m_bStaticLightIndirectOnly )? 2 : 1) : 0;

				DECLARE_DYNAMIC_VERTEX_SHADER( vertexlit_and_unlit_generic_vs20 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( DYNAMIC_LIGHT, lightState.HasDynamicLight() );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( STATICLIGHT3, staticLight3VSCombo );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING,  numBones > 0 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( TESSELLATION, 0 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( NUM_LIGHTS, bUseStaticControlFlow ? 0 : lightState.m_nNumLights );
				SET_DYNAMIC_VERTEX_SHADER_CMD( DynamicCmdsOut, vertexlit_and_unlit_generic_vs20 );

				// Bind ps_2_b shader so we can get shadow mapping
				if ( g_pHardwareConfig->SupportsPixelShaders_2_b() || g_pHardwareConfig->ShouldAlwaysUseShaderModel2bShaders() ) // Always send OpenGL this way
				{
					if ( bCSMEnabled )
					{
 						ITexture *pDepthTextureAtlas = NULL;
 						const CascadedShadowMappingState_t &cascadeState = pShaderAPI->GetCascadedShadowMappingState( &pDepthTextureAtlas );
 
						if (pDepthTextureAtlas)
						{
							//DynamicCmdsOut.BindTexture( pShader, SHADER_SAMPLER15, TEXTURE_BINDFLAGS_SHADOWDEPTH, pDepthTextureAtlas, 0 );
                            DynamicCmdsOut.SetPixelShaderConstant( 64, &cascadeState.m_vLightColor.x, CASCADED_SHADOW_MAPPING_CONSTANT_BUFFER_SIZE );
						}
						else
						{
							bCSMEnabled = 0;
						}
					}

					DECLARE_DYNAMIC_PIXEL_SHADER( vertexlit_and_unlit_generic_ps20b );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( SMOKEGRENADEBLEND, bBlendWithSmokeGrenade );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHTSHADOWS, bFlashlightShadows );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( CASCADE_SIZE, ( IsGameConsole() ) ? ( bCSMEnabled ? 1 : 0 ) : 0 );
					SET_DYNAMIC_PIXEL_SHADER_CMD( DynamicCmdsOut, vertexlit_and_unlit_generic_ps20b );
				}
				else
				{
					DECLARE_DYNAMIC_PIXEL_SHADER( vertexlit_and_unlit_generic_ps20 );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( SMOKEGRENADEBLEND, bBlendWithSmokeGrenade );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( CASCADE_SIZE, 0 );
					SET_DYNAMIC_PIXEL_SHADER_CMD( DynamicCmdsOut, vertexlit_and_unlit_generic_ps20 );
				}
			}
			#if !defined( _GAMECONSOLE ) && !defined( _PS3 )
			else
			{
				if ( bMorphing )
				{
					pShader->SetHWMorphVertexShaderState( VERTEX_SHADER_SHADER_SPECIFIC_CONST_10, VERTEX_SHADER_SHADER_SPECIFIC_CONST_11, SHADER_VERTEXTEXTURE_SAMPLER0 );
				}

				if ( bWorldNormal && IsPC() )
				{
					float vEyeDir[4];
					ShaderApiFast( pShaderAPI )->GetWorldSpaceCameraDirection( vEyeDir );

					float flFarZ = ShaderApiFast( pShaderAPI )->GetFarZ();
					vEyeDir[0] /= flFarZ;	// Divide by farZ for SSAO algorithm
					vEyeDir[1] /= flFarZ;
					vEyeDir[2] /= flFarZ;
					DynamicCmdsOut.SetVertexShaderConstant( 26, vEyeDir );
				}

				if ( bCSMEnabled )
				{
					ITexture *pDepthTextureAtlas = NULL;
					const CascadedShadowMappingState_t &cascadeState = pShaderAPI->GetCascadedShadowMappingState( &pDepthTextureAtlas );

					DynamicCmdsOut.BindTexture( pShader, SHADER_SAMPLER15, TEXTURE_BINDFLAGS_SHADOWDEPTH, pDepthTextureAtlas, 0 );

					DynamicCmdsOut.SetPixelShaderConstant( 64, &cascadeState.m_vLightColor.x, CASCADED_SHADOW_MAPPING_CONSTANT_BUFFER_SIZE );
				}

				if ( !( IsGameConsole() ) )
				{
					BOOL bCSMEnabledBool = bCSMEnabled;						
					pShaderAPI->SetBooleanPixelShaderConstant( 0, &bCSMEnabledBool, 1 );
				}

				int staticLight3VSCombo = ( lightState.m_bStaticLight && bStaticLight3Streams ) ? ( ( lightState.m_bStaticLightIndirectOnly ) ? 2 : 1 ) : 0;

				DECLARE_DYNAMIC_VERTEX_SHADER( vertexlit_and_unlit_generic_vs30 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( DYNAMIC_LIGHT, lightState.HasDynamicLight() );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( STATICLIGHT3, staticLight3VSCombo );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, ( numBones > 0 ) && ( nTessellationMode == TESSELLATION_MODE_DISABLED ) );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression  && ( nTessellationMode == TESSELLATION_MODE_DISABLED ) );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( TESSELLATION, nTessellationMode );
				//SET_DYNAMIC_VERTEX_SHADER_COMBO( MORPHING, bMorphing );
				SET_DYNAMIC_VERTEX_SHADER_CMD( DynamicCmdsOut, vertexlit_and_unlit_generic_vs30 );

				DECLARE_DYNAMIC_PIXEL_SHADER( vertexlit_and_unlit_generic_ps30 );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( SMOKEGRENADEBLEND, bBlendWithSmokeGrenade );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHTSHADOWS, bWorldNormal ? 0 : bFlashlightShadows );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( UBERLIGHT, bUberlight && bSFM );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( CASCADE_SIZE, IsGameConsole() ? ( bCSMEnabled ? 1 : 0 ) : 0 );
				SET_DYNAMIC_PIXEL_SHADER_CMD( DynamicCmdsOut, vertexlit_and_unlit_generic_ps30 );

				bool bUnusedTexCoords[3] = { false, false, !bMorphing || !bIsDecal };
				ShaderApiFast( pShaderAPI )->MarkUnusedVertexFields( 0, 3, bUnusedTexCoords );
			}
			#endif
		}

		if ( bHasBump || bHasDiffuseWarp )
		{
			ITexture *pDepthTextureAtlas = NULL;
			const CascadedShadowMappingState_t &cascadeState = pShaderAPI->GetCascadedShadowMappingState( &pDepthTextureAtlas );

			DynamicCmdsOut.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_12, &cascadeState.m_vLightColor.x );
		}

		if ( !bHasBump || bTreeSway )
		{
			float fTempConst[4];
			fTempConst[0] = fSinglePassFlashlight;
			fTempConst[1] = ShaderApiFast( pShaderAPI )->CurrentTime();
			Vector windDir = IsBoolSet( info.m_nTreeSwayStatic, params ) ? Vector(0.5f,0.5f,0) : pShaderAPI->GetVectorRenderingParameter( VECTOR_RENDERPARM_WIND_DIRECTION );

			fTempConst[2] = windDir.x;
			fTempConst[3] = windDir.y;
			DynamicCmdsOut.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_12, fTempConst );
		}

		float fWriteDepthToAlpha = bWriteDepthToAlpha && IsPC() ? 1.0f : 0.0f;
		float fWriteWaterFogToDestAlpha = bWriteWaterFogToAlpha ? 1.0f : 0.0f;
		float fVertexAlpha = bHasVertexAlpha ? 1.0f : 0.0f;
		float fBlendTintByBaseAlpha = IsBoolSet( info.m_nBlendTintByBaseAlpha, params ) ? 1.0f : 0.0f;
		fBlendTintByBaseAlpha = bHasTintMaskTexture ? 1.0f : fBlendTintByBaseAlpha;	// This is needed to make the shader math work out in the tint texture case

		// Controls for lerp-style paths through shader code (used by bump and non-bump)
		float vShaderControls[4] = { IsBoolSet( info.m_nNoTint, params ) ? -1.0f : ( 1.0f - fBlendTintByBaseAlpha ), fWriteDepthToAlpha, fWriteWaterFogToDestAlpha, fVertexAlpha };
		
		if ( bHasBump )
		{
			// Bump shader doesn't use vertex alpha
			vShaderControls[ 3 ] = GetIntParam( info.m_nEnvMapMaskInTintMaskTexture, params, 0 ) ? 1.0f : 0.0f;
		}
		else
		{
			// Nonbump shader doesn't use water fog dest alpha thing.
			vShaderControls[ 2 ] = GetIntParam( info.m_nEnvMapMaskInTintMaskTexture, params, 0 ) ? 1.0f : 0.0f;
		}
		DynamicCmdsOut.SetPixelShaderConstant( 12, vShaderControls, 1 );

		/* Time - used for debugging
		float vTimeConst[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		float flTime = pShaderAPI->CurrentTime();
		vTimeConst[0] = flTime;
		DynamicCmdsOut.SetPixelShaderConstant( 25, vTimeConst, 1 );
		//*/

		DynamicCmdsOut.End();
#ifdef _PS3
		ShaderApiFast( pShaderAPI )->SetPixelShaderFogParams( 21 );
#endif
		ShaderApiFast( pShaderAPI )->ExecuteCommandBuffer( DynamicCmdsOut.Base() );
	}
	pShader->Draw();
}

void DrawVertexLitGeneric_DX9_Internal_ExecuteFastPath( int *vsDynIndex, int *psDynIndex,
													    CBaseVSShader *pShader,	IMaterialVar** params, IShaderDynamicAPI * pShaderAPI,
														VertexLitGeneric_DX9_Vars_t &info, 
														VertexCompressionType_t vertexCompression,
													    CBasePerMaterialContextData **pContextDataPtr, BOOL bCSMEnabled, BOOL bVertexLitGeneric )
{
	CVertexLitGeneric_DX9_Context *pContextData = reinterpret_cast< CVertexLitGeneric_DX9_Context *> ( *pContextDataPtr );

	CCommandBufferBuilder< CFixedCommandStorageBuffer< 1000 > > DynamicCmdsOut;
	DynamicCmdsOut.Call( pContextData->m_SemiStaticCmdsOut.Base() );
	
	bool bHasEnvmap = (info.m_nEnvmap) != -1 && (params[info.m_nEnvmap]->IsTexture());
	if ( bHasEnvmap )
	{
		DynamicCmdsOut.BindEnvCubemapTexture( pShader, SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, info.m_nEnvmap, info.m_nEnvmapFrame );
	}

	DynamicCmdsOut.End();
	ShaderApiFast( pShaderAPI )->ExecuteCommandBuffer( DynamicCmdsOut.Base() );

	// Set up light combo state
	LightState_t lightState = {0, false, false};
	ShaderApiFast( pShaderAPI )->GetDX9LightState( &lightState );
	
	bool bHasBump = IsTextureSet( info.m_nBumpmap, params );
	bool bHasDiffuseWarp = (info.m_nDiffuseWarpTexture != -1) && params[info.m_nDiffuseWarpTexture]->IsTexture();

	if ( !bHasBump )
	{
		ITexture *pDepthTextureAtlas = NULL;
		const CascadedShadowMappingState_t &cascadeState = pShaderAPI->GetCascadedShadowMappingState( &pDepthTextureAtlas );

		float vConst[ 4 ] = { lightState.m_bStaticLight ? 1.0f : 0.0f, cascadeState.m_vLightColor.x, cascadeState.m_vLightColor.y, cascadeState.m_vLightColor.z };
		ShaderApiFast( pShaderAPI )->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_2, vConst );
	}

	int numBones = ShaderApiFast( pShaderAPI )->GetCurrentNumBones();

	int nDetailTranslucencyTexture = -1;

	int nDetailBlendMode= GetIntParam( info.m_nDetailTextureCombineMode, params );
	
	float fBlendFactor = GetFloatParam( info.m_nDetailTextureBlendFactor, params, 1.0 );
	bool bHasDetailTexture = IsTextureSet( info.m_nDetail, params );
	if ( bHasDetailTexture && ( fBlendFactor > 0.0 ) )
	{
		if ( ( nDetailBlendMode == DETAIL_BLEND_MODE_FADE ) ||
			( nDetailBlendMode == DETAIL_BLEND_MODE_MULTIPLY ) ||
			( nDetailBlendMode == DETAIL_BLEND_MODE_MASK_BASE_BY_DETAIL_ALPHA ) )
		{
			nDetailTranslucencyTexture = info.m_nDetail;
		}
	}

	BlendType_t nBlendType;

	bool bHasBaseTexture = IsTextureSet( info.m_nBaseTexture, params );
	if ( bHasBaseTexture )
	{
		nBlendType = pShader->EvaluateBlendRequirements( info.m_nBaseTexture, true, nDetailTranslucencyTexture );
	}
	else
	{
		nBlendType = pShader->EvaluateBlendRequirements( info.m_nEnvmapMask, false );
	}
	
	bool bIsAlphaTested = IS_FLAG_SET( MATERIAL_VAR_ALPHATEST ) != 0;
	bool bFullyOpaque = (nBlendType != BT_BLENDADD) && (nBlendType != BT_BLEND) && !bIsAlphaTested;

	MaterialFogMode_t fogType = ShaderApiFast( pShaderAPI )->GetSceneFogMode();

	bool bWriteDepthToAlpha;
	bool bWriteWaterFogToAlpha;
	if( bFullyOpaque ) 
	{
		bWriteDepthToAlpha = ShaderApiFast( pShaderAPI )->ShouldWriteDepthToDestAlpha();
		bWriteWaterFogToAlpha = (fogType == MATERIAL_FOG_LINEAR_BELOW_FOG_Z);
	}
	else
	{
		//can't write a special value to dest alpha if we're actually using as-intended alpha
		bWriteDepthToAlpha = false;
		bWriteWaterFogToAlpha = false;
	}
	
	pShaderAPI->SetBooleanPixelShaderConstant( 0, &bCSMEnabled, 1 );
	if ( bCSMEnabled )
	{
		ITexture *pDepthTextureAtlas = NULL;
		const CascadedShadowMappingState_t &cascadeState = pShaderAPI->GetCascadedShadowMappingState( &pDepthTextureAtlas );
		pShader->BindTexture( SHADER_SAMPLER15, TEXTURE_BINDFLAGS_SHADOWDEPTH, pDepthTextureAtlas, 0 );
		pShaderAPI->SetPixelShaderConstant( 64, &cascadeState.m_vLightColor.x, CASCADED_SHADOW_MAPPING_CONSTANT_BUFFER_SIZE );
	}

	bool bBlendWithSmokeGrenade = IsBoolSet( info.m_nBlendWithSmokeGrenade, params );
	if ( bBlendWithSmokeGrenade )
	{
		float vSmokeGrenadePosSmoke[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		params[ info.m_nBlendWithSmokeGrenadePosSmoke ]->GetVecValue( vSmokeGrenadePosSmoke, 3 );

		if ( vSmokeGrenadePosSmoke[0] == 0 && vSmokeGrenadePosSmoke[1] == 0 && vSmokeGrenadePosSmoke[2] == 0 )
		{
			bBlendWithSmokeGrenade = false;
		}
		else
		{
			DynamicCmdsOut.SetPixelShaderConstant( 6, vSmokeGrenadePosSmoke, 1 );
		}
	}

	bool bHasVertexColor = bVertexLitGeneric ? false : IS_FLAG_SET( MATERIAL_VAR_VERTEXCOLOR );
	bool bHasVertexAlpha = bVertexLitGeneric ? false : IS_FLAG_SET( MATERIAL_VAR_VERTEXALPHA );

	bool bStaticLight3Streams = (r_staticlight_streams.GetInt() == 3) && (!(bHasVertexColor || bHasVertexAlpha));
	bool bStaticLight3 = lightState.m_bStaticLight && bStaticLight3Streams;
	int32 nStaticLightVsCombo = bStaticLight3 ? ( !r_staticlight_streams_indirect_only.GetBool() ? 2 : 1 ) : 0;

	if ( bHasBump || bHasDiffuseWarp )
	{
		DECLARE_DYNAMIC_VERTEX_SHADER( vertexlit_and_unlit_generic_bump_vs30 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, numBones > 0);
		SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression);
		SET_DYNAMIC_VERTEX_SHADER_COMBO( TESSELLATION, TESSELLATION_MODE_DISABLED );
		//SET_DYNAMIC_VERTEX_SHADER_COMBO( MORPHING, false );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( STATICLIGHT3, nStaticLightVsCombo );
		SET_DYNAMIC_VERTEX_SHADER( vertexlit_and_unlit_generic_bump_vs30 );

		DECLARE_DYNAMIC_PIXEL_SHADER( vertexlit_and_unlit_generic_bump_ps30 );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( NUM_LIGHTS, lightState.m_nNumLights );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( AMBIENT_LIGHT, lightState.m_bAmbientLight && !bStaticLight3 );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHTSHADOWS, false );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( UBERLIGHT, false );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( CASCADE_SIZE, 0 );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( STATICLIGHT3, bStaticLight3 );
		SET_DYNAMIC_PIXEL_SHADER( vertexlit_and_unlit_generic_bump_ps30 );

		*vsDynIndex = _vshIndex.GetIndex();
		*psDynIndex = _pshIndex.GetIndex();
	}
	else
	{
		DECLARE_DYNAMIC_VERTEX_SHADER( vertexlit_and_unlit_generic_vs30 );

		int staticLight3VSCombo = ( lightState.m_bStaticLight && bStaticLight3Streams ) ? ( ( lightState.m_bStaticLightIndirectOnly ) ? 2 : 1 ) : 0;

		//[msmith] Using dynamic lights on a statically lit prop causes a visual pop in the lighting.  For this reason, we ignore dynamic lights on staticly lit props.
		SET_DYNAMIC_VERTEX_SHADER_COMBO( DYNAMIC_LIGHT, lightState.HasDynamicLight() );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, numBones > 0);
		SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression);
		SET_DYNAMIC_VERTEX_SHADER_COMBO( TESSELLATION, TESSELLATION_MODE_DISABLED );
		//SET_DYNAMIC_VERTEX_SHADER_COMBO( MORPHING, false );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( STATICLIGHT3, staticLight3VSCombo );
		SET_DYNAMIC_VERTEX_SHADER( vertexlit_and_unlit_generic_vs30 );

		DECLARE_DYNAMIC_PIXEL_SHADER( vertexlit_and_unlit_generic_ps30 );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( SMOKEGRENADEBLEND, bBlendWithSmokeGrenade );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHTSHADOWS, false );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( UBERLIGHT, false );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( CASCADE_SIZE, 0 );
		SET_DYNAMIC_PIXEL_SHADER( vertexlit_and_unlit_generic_ps30 );

		*vsDynIndex = _vshIndex.GetIndex();
		*psDynIndex = _pshIndex.GetIndex();
	}

	bool bUnusedTexCoords[3] = { false, false, true };
	ShaderApiFast( pShaderAPI )->MarkUnusedVertexFields( 0, 3, bUnusedTexCoords );

	bool bTreeSway = ( GetIntParam( info.m_nTreeSway, params, 0 ) != 0 );
	
	if ( bHasBump || bHasDiffuseWarp )
	{
		ITexture *pDepthTextureAtlas = NULL;
		const CascadedShadowMappingState_t &cascadeState = pShaderAPI->GetCascadedShadowMappingState( &pDepthTextureAtlas );

		ShaderApiFast( pShaderAPI )->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_12, &cascadeState.m_vLightColor.x );
	}

	if ( !bHasBump || bTreeSway )
	{
		float fTempConst[4];
		fTempConst[0] = 0.0f;
		fTempConst[1] = ShaderApiFast( pShaderAPI )->CurrentTime();
		Vector windDir = IsBoolSet( info.m_nTreeSwayStatic, params ) ? Vector(0.5f,0.5f,0) : pShaderAPI->GetVectorRenderingParameter( VECTOR_RENDERPARM_WIND_DIRECTION );

		fTempConst[2] = windDir.x;
		fTempConst[3] = windDir.y;
		ShaderApiFast( pShaderAPI )->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_12, fTempConst );
	}

	float fWriteDepthToAlpha = bWriteDepthToAlpha? 1.0f : 0.0f;
	float fWriteWaterFogToDestAlpha = bWriteWaterFogToAlpha? 1.0f : 0.0f;
	float fVertexAlpha = bHasVertexAlpha ? 1.0f : 0.0f;
	float fBlendTintByBaseAlpha = IsBoolSet( info.m_nBlendTintByBaseAlpha, params ) ? 1.0f : 0.0f;
	bool bHasTintMaskTexture = IsTextureSet( info.m_nTintMaskTexture, params );
	fBlendTintByBaseAlpha = bHasTintMaskTexture ? 1.0f : fBlendTintByBaseAlpha;	// This is needed to make the shader math work out in the tint texture case

	// Controls for lerp-style paths through shader code (used by bump and non-bump)
	float vShaderControls[4] = { IsBoolSet( info.m_nNoTint, params ) ? -1.0f : ( 1.0f - fBlendTintByBaseAlpha ), fWriteDepthToAlpha, fWriteWaterFogToDestAlpha, fVertexAlpha };
	if ( bHasBump )
	{
		// Bump shader doesn't use vertex alpha
		vShaderControls[ 3 ] = GetIntParam( info.m_nEnvMapMaskInTintMaskTexture, params, 0 ) ? 1.0f : 0.0f;
	}
	else
	{
		// Nonbump shader doesn't use water fog dest alpha thing.
		vShaderControls[ 2 ] = GetIntParam( info.m_nEnvMapMaskInTintMaskTexture, params, 0 ) ? 1.0f : 0.0f;
	}
	DynamicCmdsOut.SetPixelShaderConstant( 12, vShaderControls, 1 );

	ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( 12, vShaderControls, 1 );
}

void DrawVertexLitGeneric_DX9( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI * pShaderAPI,
	IShaderShadow* pShaderShadow, bool bVertexLitGeneric, VertexLitGeneric_DX9_Vars_t &info, VertexCompressionType_t vertexCompression,
								CBasePerMaterialContextData **pContextDataPtr )
{
	if ( WantsPhongShader( params, info ) && g_pHardwareConfig->SupportsPixelShaders_2_b() /*&& mat_bumpmap.GetBool()*/ )
	{
		DrawPhong_DX9( pShader, params, pShaderAPI, pShaderShadow, info, vertexCompression, pContextDataPtr );
		return;		
	}
	
	bool bReceiveFlashlight = bVertexLitGeneric || ( GetIntParam( info.m_nReceiveFlashlight, params ) != 0 );
	bool bHasFlashlight = bReceiveFlashlight && pShader->UsingFlashlight( params );

	//since single pass flashlights have a different snapshot than multipass. We need to get snapshots of both and only actually draw the enabled mode
	if( IsX360() || IsPS3() || !bHasFlashlight || (GetIntParam( info.m_nSinglePassFlashlight, params ) == 0) )
	{
		//360/PS3 only support single pass flashlight, so bHasFlashlight == bSinglePassFlashlight. And single pass flashlights are the same as multipass when there's no flashlight.
		DrawVertexLitGeneric_DX9_Internal( pShader, params, pShaderAPI,
			pShaderShadow, bVertexLitGeneric, bHasFlashlight, IsX360() || IsPS3(), info, vertexCompression, pContextDataPtr );
	}
	else //single pass flashlight enabled material. Support both multipass and single pass flashlight
	{
		if( pShaderShadow )
		{
			//snapshotting, grab a snapshot of both modes
			DrawVertexLitGeneric_DX9_Internal( pShader, params, pShaderAPI,
				pShaderShadow, bVertexLitGeneric, bHasFlashlight, false, info, vertexCompression, pContextDataPtr );
			DrawVertexLitGeneric_DX9_Internal( pShader, params, pShaderAPI,
				pShaderShadow, bVertexLitGeneric, bHasFlashlight, true, info, vertexCompression, pContextDataPtr );
		}
		else
		{
			Assert( pShaderAPI );
			if( ShaderApiFast( pShaderAPI )->SinglePassFlashlightModeEnabled() )
			{
				//use only the second (singlepass flashlights) snapshot
				pShader->Draw( false );
				DrawVertexLitGeneric_DX9_Internal( pShader, params, pShaderAPI,
					pShaderShadow, bVertexLitGeneric, bHasFlashlight, true, info, vertexCompression, pContextDataPtr );
			}
			else
			{
				//use only the first (multipass flashlights) snapshot
				DrawVertexLitGeneric_DX9_Internal( pShader, params, pShaderAPI,
					pShaderShadow, bVertexLitGeneric, bHasFlashlight, false, info, vertexCompression, pContextDataPtr );
				pShader->Draw( false );
			}
		}
	}
}

void DrawVertexLitGeneric_DX9_ExecuteFastPath( int *vsDynIndex, int *psDynIndex,
											   CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI * pShaderAPI, 
											  VertexLitGeneric_DX9_Vars_t &info, 
											  VertexCompressionType_t vertexCompression, CBasePerMaterialContextData **pContextDataPtr, 
											  BOOL bCSMEnabled, BOOL bVertexLitGeneric /*= true*/ )
{
	if ( WantsPhongShader( params, info ) )
	{
		DrawPhong_DX9_ExecuteFastPath( vsDynIndex, psDynIndex, pShader, params, pShaderAPI, info, vertexCompression, pContextDataPtr, bCSMEnabled );
		return;
	}

	DrawVertexLitGeneric_DX9_Internal_ExecuteFastPath( vsDynIndex, psDynIndex, pShader, params, pShaderAPI,
			info, vertexCompression, pContextDataPtr, bCSMEnabled, bVertexLitGeneric );
}
