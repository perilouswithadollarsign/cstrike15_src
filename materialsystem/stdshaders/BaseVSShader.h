//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
// This is what all vs/ps (dx8+) shaders inherit from.
//===========================================================================//

#ifndef BASEVSSHADER_H
#define BASEVSSHADER_H

#ifdef _WIN32		   
#pragma once
#endif

#include "cpp_shader_constant_register_map.h"
#include "shaderlib/cshader.h"
#include "shaderlib/BaseShader.h"
#include "shaderapifast.h"
#include "convar.h"
#include <renderparm.h>


// Texture combining modes for combining base and detail/basetexture2
// Matches what's in common_ps_fxc.h
#define DETAIL_BLEND_MODE_RGB_EQUALS_BASE_x_DETAILx2				0	// Original mode (Mod2x)
#define DETAIL_BLEND_MODE_RGB_ADDITIVE								1	// Base.rgb+detail.rgb*fblend
#define DETAIL_BLEND_MODE_DETAIL_OVER_BASE							2
#define DETAIL_BLEND_MODE_FADE										3	// Straight fade between base and detail.
#define DETAIL_BLEND_MODE_BASE_OVER_DETAIL							4	// Use base alpha for blend over detail
#define DETAIL_BLEND_MODE_RGB_ADDITIVE_SELFILLUM					5	// Add detail color post lighting
#define DETAIL_BLEND_MODE_RGB_ADDITIVE_SELFILLUM_THRESHOLD_FADE		6
#define DETAIL_BLEND_MODE_MOD2X_SELECT_TWO_PATTERNS					7	// Use alpha channel of base to select between mod2x channels in r+a of detail
#define DETAIL_BLEND_MODE_MULTIPLY									8
#define DETAIL_BLEND_MODE_MASK_BASE_BY_DETAIL_ALPHA					9	// Use alpha channel of detail to mask base
#define DETAIL_BLEND_MODE_SSBUMP_BUMP								10	// Use detail to modulate lighting as an ssbump
#define DETAIL_BLEND_MODE_SSBUMP_NOBUMP								11	// Detail is an ssbump but use it as an albedo. shader does the magic here - no user needs to specify mode 11
#define DETAIL_BLEND_MODE_NONE										12	// There is no detail texture

// Texture combining modes for combining base and decal texture
#define DECAL_BLEND_MODE_DECAL_ALPHA								0	// Original mode ( = decalRGB*decalA + baseRGB*(1-decalA))
#define DECAL_BLEND_MODE_RGB_MOD1X									1	// baseRGB * decalRGB
#define DECAL_BLEND_MODE_NONE										2	// There is no decal texture

// We force aniso on certain textures for the consoles only
#if defined( _GAMECONSOLE )
	#define ANISOTROPIC_OVERRIDE TEXTUREFLAGS_ANISOTROPIC
#else
	#define ANISOTROPIC_OVERRIDE 0
#endif

//-----------------------------------------------------------------------------
// Helper macro for vertex shaders
//-----------------------------------------------------------------------------
#define BEGIN_VS_SHADER_FLAGS(_name, _help, _flags)	__BEGIN_SHADER_INTERNAL( CBaseVSShader, _name, _help, _flags )
#define BEGIN_VS_SHADER(_name,_help)	__BEGIN_SHADER_INTERNAL( CBaseVSShader, _name, _help, 0 )


// useful parameter initialization macro
#define INIT_FLOAT_PARM( parm, value )					\
		if ( !params[(parm)]->IsDefined() )				\
		{												\
			params[(parm)]->SetFloatValue( (value) );	\
		}

// useful pixel shader declaration macro for ps20/20b c++ code
#define SET_STATIC_PS2X_PIXEL_SHADER_NO_COMBOS( basename )		\
		if( g_pHardwareConfig->SupportsPixelShaders_2_b() )		\
		{														\
			DECLARE_STATIC_PIXEL_SHADER( basename##_ps20b );	\
			SET_STATIC_PIXEL_SHADER( basename##_ps20b );		\
		}														\
		else													\
		{														\
			DECLARE_STATIC_PIXEL_SHADER( basename##_ps20 );		\
			SET_STATIC_PIXEL_SHADER( basename##_ps20 );			\
		}

#define SET_DYNAMIC_PS2X_PIXEL_SHADER_NO_COMBOS( basename )		\
		if( g_pHardwareConfig->SupportsPixelShaders_2_b() )		\
		{														\
			DECLARE_DYNAMIC_PIXEL_SHADER( basename##_ps20b );	\
			SET_DYNAMIC_PIXEL_SHADER( basename##_ps20b );		\
		}														\
		else													\
		{														\
			DECLARE_DYNAMIC_PIXEL_SHADER( basename##_ps20 );		\
			SET_DYNAMIC_PIXEL_SHADER( basename##_ps20 );			\
		}


//-----------------------------------------------------------------------------
// Base class for shaders, contains helper methods.
//-----------------------------------------------------------------------------
class CBaseVSShader : public CBaseShader
{
public:
	// Loads bump lightmap coordinates into the pixel shader
	void LoadBumpLightmapCoordinateAxes_PixelShader( int pixelReg );

	// Loads bump lightmap coordinates into the vertex shader
	void LoadBumpLightmapCoordinateAxes_VertexShader( int vertexReg );

	// Pixel and vertex shader constants....
	void SetPixelShaderConstant( int pixelReg, int constantVar );

	// Pixel and vertex shader constants....
	void SetPixelShaderConstantGammaToLinear( int pixelReg, int constantVar );

	// This version will put constantVar into x,y,z, and constantVar2 into the w
	void SetPixelShaderConstant( int pixelReg, int constantVar, int constantVar2 );
	void SetPixelShaderConstantGammaToLinear( int pixelReg, int constantVar, int constantVar2 );

	// Helpers for setting constants that need to be converted to linear space (from gamma space).
	void SetVertexShaderConstantGammaToLinear( int var, float const* pVec, int numConst = 1, bool bForce = false );
	void SetPixelShaderConstantGammaToLinear( int var, float const* pVec, int numConst = 1, bool bForce = false );

	void SetVertexShaderConstant( int vertexReg, int constantVar );

	// set rgb components of constant from a color parm and give an explicit w value
	void SetPixelShaderConstant_W( int pixelReg, int constantVar, float fWValue );

	// GR - fix for const/lerp issues
	void SetPixelShaderConstantFudge( int pixelReg, int constantVar );

	// Sets vertex shader texture transforms
	void SetVertexShaderTextureTranslation( int vertexReg, int translationVar );
	void SetVertexShaderTextureScale( int vertexReg, int scaleVar );
 	void SetVertexShaderTextureTransform( int vertexReg, int transformVar );
	void SetVertexShaderTextureScaledTransform( int vertexReg, 
											int transformVar, int scaleVar );

	// Set pixel shader texture transforms
	void SetPixelShaderTextureTranslation( int pixelReg, int translationVar );
	void SetPixelShaderTextureScale( int pixelReg, int scaleVar );
 	void SetPixelShaderTextureTransform( int pixelReg, int transformVar );
	void SetPixelShaderTextureScaledTransform( int pixelReg, 
											int transformVar, int scaleVar );

	// Moves a matrix into vertex shader constants 
	void SetVertexShaderMatrix3x4( int vertexReg, int matrixVar );
	void SetVertexShaderMatrix4x4( int vertexReg, int matrixVar );

	// Loads the view matrix into vertex shader constants
	void LoadViewMatrixIntoVertexShaderConstant( int vertexReg );

	// Loads the projection matrix into vertex shader constants
	void LoadProjectionMatrixIntoVertexShaderConstant( int vertexReg );

	// Loads the model->view matrix into vertex shader constants
	void LoadModelViewMatrixIntoVertexShaderConstant( int vertexReg );

	// Helpers for dealing with envmaptint
	void SetEnvMapTintPixelShaderDynamicState( int pixelReg, int tintVar, int alphaVar, bool bConvertFromGammaToLinear = false );
	
	// Helper methods for pixel shader overbrighting
	void EnablePixelShaderOverbright( int reg, bool bEnable, bool bDivideByTwo );

	// Sets up hw morphing state for the vertex shader
	void SetHWMorphVertexShaderState( int nDimConst, int nSubrectConst, VertexTextureSampler_t morphSampler );

	BlendType_t EvaluateBlendRequirements( int textureVar, bool isBaseTexture, int detailTextureVar = -1 );

	// Helper for setting up flashlight constants
	void SetFlashlightVertexShaderConstants( bool bBump, int bumpTransformVar, bool bDetail, int detailScaleVar, bool bSetTextureTransforms );

	struct DrawFlashlight_dx90_Vars_t
	{
		DrawFlashlight_dx90_Vars_t() 
		{ 
			// set all ints to -1
			memset( this, 0xFF, sizeof(DrawFlashlight_dx90_Vars_t) ); 
			// set all bools to a default value.
			m_bBump = false;
			m_bLightmappedGeneric = false;
			m_bWorldVertexTransition = false;
			m_bTeeth = false;
			m_bSSBump = false;
			m_fSeamlessScale = 0.0;
		}
		bool m_bBump;
		bool m_bLightmappedGeneric;
		bool m_bWorldVertexTransition;
		bool m_bTeeth;
		int m_nBumpmapVar;
		int m_nBumpmapFrame;
		int m_nBumpTransform;
		int m_nFlashlightTextureVar;
		int m_nFlashlightTextureFrameVar;
		int m_nBaseTexture2Var;
		int m_nBaseTexture2FrameVar;
		int m_nBumpmapVar2;
		int m_nBumpmapFrame2;
		int m_nBumpTransform2;
		int m_nDetailVar;
		int m_nDetailScale;
		int m_nDetailTextureCombineMode;
		int m_nDetailTextureBlendFactor;
		int m_nDetailTint;
		int m_nDetailVar2;
		int m_nDetailScale2;
		int m_nDetailTextureBlendFactor2;
		int m_nDetailTint2;
		int m_nTeethForwardVar;
		int m_nTeethIllumFactorVar;
		int m_nAlphaTestReference;
		bool m_bSSBump;
		float m_fSeamlessScale;								// 0.0 = not seamless
		int m_nLayerTint1;
		int m_nLayerTint2;
	};
	void DrawFlashlight_dx90( IMaterialVar** params, 
		IShaderDynamicAPI *pShaderAPI, IShaderShadow* pShaderShadow, DrawFlashlight_dx90_Vars_t &vars );

	void HashShadow2DJitter( const float fJitterSeed, float *fU, float* fV );

	//Alpha tested materials can end up leaving garbage in the dest alpha buffer if they write depth. 
	//This pass fills in the areas that passed the alpha test with depth in dest alpha 
	//by writing only equal depth pixels and only if we should be writing depth to dest alpha
	void DrawEqualDepthToDestAlpha( void );
	
private:
	// Converts a color + alpha into a vector4
	void ColorVarsToVector( int colorVar, int alphaVar, Vector4D &color );
};

FORCEINLINE bool IsSRGBDetailTexture( int nMode )
{
	return	( nMode == DETAIL_BLEND_MODE_DETAIL_OVER_BASE ) ||
			( nMode == DETAIL_BLEND_MODE_FADE ) ||
			( nMode == DETAIL_BLEND_MODE_BASE_OVER_DETAIL );
}

FORCEINLINE bool IsSRGBDecalTexture( int nMode )
{
	return	(nMode == DECAL_BLEND_MODE_DECAL_ALPHA);
}

FORCEINLINE char * GetFlashlightTextureFilename()
{
	//if ( !IsX360() && ( g_pHardwareConfig->SupportsBorderColor() ) )
	//{
	//	return "effects/flashlight001_border";
	//}
	//else
	{
		return "effects/flashlight001";
	}
}

extern ConVar r_flashlightbrightness;

FORCEINLINE void SetFlashLightColorFromState( FlashlightState_t const &state, IShaderDynamicAPI *pShaderAPI, bool bSinglePassFlashlight, int nPSRegister=28, bool bFlashlightNoLambert=false )
{
	// Old code
	//float flToneMapScale = ( ShaderApiFast( pShaderAPI )->GetToneMappingScaleLinear() ).x;
	//float flFlashlightScale = 1.0f / flToneMapScale;

	// Fix to old code to keep flashlight from ever getting brighter than 1.0
	//float flToneMapScale = ( ShaderApiFast( pShaderAPI )->GetToneMappingScaleLinear() ).x;
	//if ( flToneMapScale < 1.0f )
	//	flToneMapScale = 1.0f;
	//float flFlashlightScale = 1.0f / flToneMapScale;

	float flFlashlightScale = r_flashlightbrightness.GetFloat();

	if ( !g_pHardwareConfig->GetHDREnabled() )
	{
		// Non-HDR path requires 2.0 flashlight
		flFlashlightScale = 2.0f;
	}

	// DX10 hardware and single pass flashlight require a hack scalar since the flashlight is added in linear space
	if ( ( g_pHardwareConfig->UsesSRGBCorrectBlending() ) || ( bSinglePassFlashlight ) )
	{
		flFlashlightScale *= 2.5f; // Magic number that works well on the 360 and NVIDIA 8800
	}

	flFlashlightScale *= state.m_fBrightnessScale;

	// Generate pixel shader constant
	float const *pFlashlightColor = state.m_Color;
	float vPsConst[4] = { flFlashlightScale * pFlashlightColor[0], flFlashlightScale * pFlashlightColor[1], flFlashlightScale * pFlashlightColor[2], pFlashlightColor[3] };
	vPsConst[3] = bFlashlightNoLambert ? 2.0f : 0.0f; // This will be added to N.L before saturate to force a 1.0 N.L term

	// Red flashlight for testing
	//vPsConst[0] = 0.5f; vPsConst[1] = 0.0f; vPsConst[2] = 0.0f;

	ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( nPSRegister, ( float * )vPsConst );
}

FORCEINLINE float ShadowAttenFromState( FlashlightState_t const &state )
{
	// DX10 requires some hackery due to sRGB/blend ordering change from DX9, which makes the shadows too light
	if ( g_pHardwareConfig->UsesSRGBCorrectBlending() )
		return state.m_flShadowAtten * 0.1f; // magic number

	return state.m_flShadowAtten;
}

FORCEINLINE float ShadowFilterFromState( FlashlightState_t const &state )
{
	// We developed shadow maps at 1024, so we expect the penumbra size to have been tuned relative to that
	return state.m_flShadowFilterSize / 1024.0f;
}


FORCEINLINE void SetupUberlightFromState( IShaderDynamicAPI *pShaderAPI, FlashlightState_t const &state )
{
	// Bail if we can't do ps30 or we don't even want an uberlight
	if ( !g_pHardwareConfig->HasFastVertexTextures() || !state.m_bUberlight || !pShaderAPI )
		return;

	UberlightState_t u = state.m_uberlightState;

	// Set uberlight shader parameters as function of user controls from UberlightState_t
	Vector4D vSmoothEdge0		= Vector4D( 0.0f,			u.m_fCutOn - u.m_fNearEdge,	u.m_fCutOff,				0.0f );
	Vector4D vSmoothEdge1		= Vector4D( 0.0f,			u.m_fCutOn,					u.m_fCutOff + u.m_fFarEdge,	0.0f );
	Vector4D vSmoothOneOverW	= Vector4D( 0.0f,			1.0f / u.m_fNearEdge,		1.0f / u.m_fFarEdge,		0.0f );
	Vector4D vShearRound		= Vector4D( u.m_fShearx,	u.m_fSheary,				2.0f / u.m_fRoundness,	   -u.m_fRoundness / 2.0f );
	Vector4D vaAbB				= Vector4D( u.m_fWidth,		u.m_fWidth + u.m_fWedge,	u.m_fHeight,				u.m_fHeight + u.m_fHedge );

	ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( PSREG_UBERLIGHT_SMOOTH_EDGE_0, vSmoothEdge0.Base(), 1 );
	ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( PSREG_UBERLIGHT_SMOOTH_EDGE_1, vSmoothEdge1.Base(), 1 );
	ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( PSREG_UBERLIGHT_SMOOTH_EDGE_OOW, vSmoothOneOverW.Base(), 1 );
	ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( PSREG_UBERLIGHT_SHEAR_ROUND, vShearRound.Base(), 1 );
	ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( PSREG_UBERLIGHT_AABB, vaAbB.Base(), 1 );

	QAngle angles;
	QuaternionAngles( state.m_quatOrientation, angles );

	// World to Light's View matrix
	matrix3x4_t viewMatrix, viewMatrixInverse;
	AngleMatrix( angles, state.m_vecLightOrigin, viewMatrixInverse );
	MatrixInvert( viewMatrixInverse, viewMatrix );
	ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( PSREG_UBERLIGHT_WORLD_TO_LIGHT, viewMatrix.Base(), 4 );
}


// convenient material variable access functions for helpers to use.
FORCEINLINE bool IsTextureSet( int nVar, IMaterialVar **params )
{
	return ( nVar != -1 ) && ( params[nVar]->IsTexture() );
}

FORCEINLINE bool IsBoolSet( int nVar, IMaterialVar **params )
{
	return ( nVar != -1 ) && ( params[nVar]->GetIntValue() );
}

FORCEINLINE int GetIntParam( int nVar, IMaterialVar **params, int nDefaultValue = 0 )
{
	return ( nVar != -1 ) ? ( params[nVar]->GetIntValue() ) : nDefaultValue;
}

FORCEINLINE float GetFloatParam( int nVar, IMaterialVar **params, float flDefaultValue = 0.0 )
{
	return ( nVar != -1 ) ? ( params[nVar]->GetFloatValue() ) : flDefaultValue;
}

FORCEINLINE void InitFloatParam( int nIndex, IMaterialVar **params, float flValue )
{
	if ( (nIndex != -1) && !params[nIndex]->IsDefined() )
	{
		params[nIndex]->SetFloatValue( flValue );
	}
}

FORCEINLINE void InitIntParam( int nIndex, IMaterialVar **params, int nValue )
{
	if ( (nIndex != -1) && !params[nIndex]->IsDefined() )
	{
		params[nIndex]->SetIntValue( nValue );
	}
}

FORCEINLINE void InitVecParam( int nIndex, IMaterialVar **params, float x, float y )
{
	if ( (nIndex != -1) && !params[nIndex]->IsDefined() )
	{
		params[nIndex]->SetVecValue( x, y );
	}
}

FORCEINLINE void InitVecParam( int nIndex, IMaterialVar **params, float x, float y, float z )
{
	if ( (nIndex != -1) && !params[nIndex]->IsDefined() )
	{
		params[nIndex]->SetVecValue( x, y, z );
	}
}

FORCEINLINE void InitVecParam( int nIndex, IMaterialVar **params, float x, float y, float z, float w )
{
	if ( (nIndex != -1) && !params[nIndex]->IsDefined() )
	{
		params[nIndex]->SetVecValue( x, y, z, w );
	}
}

// Did we launch with -tools
bool ToolsEnabled();

class ConVar;

#ifdef _DEBUG
extern ConVar mat_envmaptintoverride;
extern ConVar mat_envmaptintscale;
#endif


#endif // BASEVSSHADER_H
