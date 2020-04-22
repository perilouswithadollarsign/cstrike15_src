//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Shader for creating energy effects
//
//==========================================================================//

#include "BaseVSShader.h"
#include "solidenergy_dx9_helper.h"
#include "cpp_shader_constant_register_map.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DEFINE_FALLBACK_SHADER( SolidEnergy, SolidEnergy_dx9 )

BEGIN_VS_SHADER( SolidEnergy_dx9, "SolidEnergy" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( DETAIL1, SHADER_PARAM_TYPE_TEXTURE, "shader/BaseTexture", "detail map 1" )
		SHADER_PARAM( DETAIL1SCALE, SHADER_PARAM_TYPE_FLOAT, "1.0", "scale detail1 as multiplier of base UVs" )
		SHADER_PARAM( DETAIL1FRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for detail1" )
		SHADER_PARAM( DETAIL1BLENDMODE, SHADER_PARAM_TYPE_INTEGER, "0", "detail 1 blend mode: 0=add, 1=mod2x, 2=mul, 3=alphamul (mul masked by base alpha)" ) 
		SHADER_PARAM( DETAIL1BLENDFACTOR, SHADER_PARAM_TYPE_FLOAT, "1.0", "detail 1 blend factor" )
		SHADER_PARAM( DETAIL1TEXTURETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "detail1 texcoord transform" )

		SHADER_PARAM( DETAIL2, SHADER_PARAM_TYPE_TEXTURE, "shader/BaseTexture", "detail map 2" )
		SHADER_PARAM( DETAIL2SCALE, SHADER_PARAM_TYPE_FLOAT, "1.0", "scale detail1 as multiplier of base UVs" )
		SHADER_PARAM( DETAIL2FRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for detail1" )
		SHADER_PARAM( DETAIL2BLENDMODE, SHADER_PARAM_TYPE_INTEGER, "0", "detail 1 blend mode: 0=add, 1=mod2x, 2=mul, 3=detailmul (mul with detail1)" ) 
		SHADER_PARAM( DETAIL2BLENDFACTOR, SHADER_PARAM_TYPE_FLOAT, "1.0", "detail 1 blend factor" )
		SHADER_PARAM( DETAIL2TEXTURETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "detail1 texcoord transform" )

		SHADER_PARAM( TANGENTTOPACITYRANGES, SHADER_PARAM_TYPE_VEC4, "[1 0.9 0 0.6]", "enables view-based opacity falloff based on tangent t direction, great for cylinders, includes last term for scaling backface opacity")
		SHADER_PARAM( TANGENTSOPACITYRANGES, SHADER_PARAM_TYPE_VEC4, "[1 0.9 0 0.6]", "enables view-based opacity falloff based on tangent s direction, great for cylinders, includes last term for scaling backface opacity")
		SHADER_PARAM( FRESNELOPACITYRANGES, SHADER_PARAM_TYPE_VEC4, "[1 0.9 0 0.6]", "enables fresnel-based opacity falloff, includes last term for scaling backface opacity")
		SHADER_PARAM( NEEDSTANGENTT, SHADER_PARAM_TYPE_BOOL, "0", "don't need to set this explicitly, it gets set when tangenttopacityranges is defined" )
		SHADER_PARAM( NEEDSTANGENTS, SHADER_PARAM_TYPE_BOOL, "0", "don't need to set this explicitly, it gets set when tangentSopacityranges is defined" )
		SHADER_PARAM( NEEDSNORMALS, SHADER_PARAM_TYPE_BOOL, "0", "don't need to set this explicitly, it gets set when fresnelopacityranges is defined" )
		SHADER_PARAM( DEPTHBLEND, SHADER_PARAM_TYPE_BOOL, "0", "enables depth-feathering" )
		SHADER_PARAM( DEPTHBLENDSCALE, SHADER_PARAM_TYPE_FLOAT, "50.0", "Amplify or reduce DEPTHBLEND fading. Lower values make harder edges." )

		SHADER_PARAM( FLOWMAP, SHADER_PARAM_TYPE_TEXTURE, "", "flowmap" )
		SHADER_PARAM( FLOWMAPFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $flowmap" )
		SHADER_PARAM( FLOWMAPSCROLLRATE, SHADER_PARAM_TYPE_VEC2, "[0 0", "2D rate to scroll $flowmap" )
		SHADER_PARAM( FLOW_NOISE_TEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "flow noise texture" )
		SHADER_PARAM( TIME, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( FLOW_WORLDUVSCALE, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( FLOW_NORMALUVSCALE, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( FLOW_TIMEINTERVALINSECONDS, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( FLOW_UVSCROLLDISTANCE, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( FLOW_NOISE_SCALE, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( FLOW_LERPEXP, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( FLOWBOUNDS, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( POWERUP, SHADER_PARAM_TYPE_FLOAT, "", "" )

		SHADER_PARAM( FLOW_COLOR_INTENSITY, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( FLOW_COLOR, SHADER_PARAM_TYPE_VEC3, "", "" )
		SHADER_PARAM( FLOW_VORTEX_COLOR, SHADER_PARAM_TYPE_VEC3, "", "" )
		SHADER_PARAM( FLOW_VORTEX_SIZE, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( FLOW_VORTEX1, SHADER_PARAM_TYPE_BOOL, "", "" )
		SHADER_PARAM( FLOW_VORTEX_POS1, SHADER_PARAM_TYPE_VEC3, "", "" )
		SHADER_PARAM( FLOW_VORTEX2, SHADER_PARAM_TYPE_BOOL, "", "" )
		SHADER_PARAM( FLOW_VORTEX_POS2, SHADER_PARAM_TYPE_VEC3, "", "" )
		SHADER_PARAM( FLOW_CHEAP, SHADER_PARAM_TYPE_BOOL, "", "" )

		SHADER_PARAM( MODELFORMAT, SHADER_PARAM_TYPE_BOOL, "", "" )
		SHADER_PARAM( OUTPUTINTENSITY, SHADER_PARAM_TYPE_FLOAT, "1.0", "" );

	END_SHADER_PARAMS

	void SetupVarsSolidEnergy( SolidEnergyVars_t &info )
	{
		info.m_nBaseTexture = BASETEXTURE;
		info.m_nBaseTextureTransform = BASETEXTURETRANSFORM;
		info.m_nDetail1Texture = DETAIL1;
		info.m_nDetail1Scale = DETAIL1SCALE;
		info.m_nDetail1Frame = DETAIL1FRAME;
		info.m_nDetail1BlendMode = DETAIL1BLENDMODE;
		info.m_nDetail1TextureTransform = DETAIL1TEXTURETRANSFORM;
		info.m_nDetail2Texture = DETAIL2;
		info.m_nDetail2Scale = DETAIL2SCALE;
		info.m_nDetail2Frame = DETAIL2FRAME;
		info.m_nDetail2BlendMode = DETAIL2BLENDMODE;
		info.m_nDetail2TextureTransform = DETAIL2TEXTURETRANSFORM;
		info.m_nTangentTOpacityRanges = TANGENTTOPACITYRANGES;
		info.m_nTangentSOpacityRanges = TANGENTSOPACITYRANGES;
		info.m_nFresnelOpacityRanges = FRESNELOPACITYRANGES;
		info.m_nNeedsTangentT = NEEDSTANGENTT;
		info.m_nNeedsTangentS = NEEDSTANGENTS;
		info.m_nNeedsNormals = NEEDSNORMALS;
		info.m_nDepthBlend = DEPTHBLEND;
		info.m_nDepthBlendScale = DEPTHBLENDSCALE;
		info.m_nFlowMap = FLOWMAP;
		info.m_nFlowMapFrame = FLOWMAPFRAME;
		info.m_nFlowMapScrollRate = FLOWMAPSCROLLRATE;
		info.m_nFlowNoiseTexture = FLOW_NOISE_TEXTURE;
		info.m_nTime = TIME;
		info.m_nFlowWorldUVScale = FLOW_WORLDUVSCALE;
		info.m_nFlowNormalUVScale = FLOW_NORMALUVSCALE;
		info.m_nFlowTimeIntervalInSeconds = FLOW_TIMEINTERVALINSECONDS;
		info.m_nFlowUVScrollDistance = FLOW_UVSCROLLDISTANCE;
		info.m_nFlowNoiseScale = FLOW_NOISE_SCALE;
		info.m_nFlowLerpExp = FLOW_LERPEXP;
		info.m_nFlowBoundsTexture = FLOWBOUNDS;
		info.m_nPowerUp = POWERUP;
		info.m_nFlowColorIntensity = FLOW_COLOR_INTENSITY;
		info.m_nFlowColor = FLOW_COLOR;
		info.m_nFlowVortexColor = FLOW_VORTEX_COLOR;
		info.m_nFlowVortexSize = FLOW_VORTEX_SIZE;
		info.m_nFlowVortex1 = FLOW_VORTEX1;
		info.m_nFlowVortexPos1 = FLOW_VORTEX_POS1;
		info.m_nFlowVortex2 = FLOW_VORTEX2;
		info.m_nFlowVortexPos2 = FLOW_VORTEX_POS2;
		info.m_nFlowCheap = FLOW_CHEAP;
		info.m_nModel = MODELFORMAT;
		info.m_nOutputIntensity = OUTPUTINTENSITY;
	}

	SHADER_INIT_PARAMS()
	{
		SolidEnergyVars_t info;
		SetupVarsSolidEnergy( info );
		InitParamsSolidEnergy( this, params, pMaterialName, info );
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		SolidEnergyVars_t info;
		SetupVarsSolidEnergy( info );
		InitSolidEnergy( this, params, info );
	}

	SHADER_DRAW
	{
		SolidEnergyVars_t info;
		SetupVarsSolidEnergy( info );
		DrawSolidEnergy( this, params, pShaderAPI, pShaderShadow, info, vertexCompression, pContextDataPtr );
	}
END_SHADER