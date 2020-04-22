//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//

#include "BaseVSShader.h"
#include "lightshafts_helper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DEFINE_FALLBACK_SHADER( LightShafts, LightShafts_dx9 )
BEGIN_VS_SHADER( LightShafts_dx9, "LightShafts" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( SHADOWDEPTHTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Shadow Depth Texture" )
		SHADER_PARAM( NOISETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Noise texture for non-uniform density" )
		SHADER_PARAM( COOKIETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Cookie Texture" )
		SHADER_PARAM( COOKIEFRAMENUM, SHADER_PARAM_TYPE_INTEGER, "", "Cookie Frame number for animated cookies" )

		SHADER_PARAM( WORLDTOTEXTURE, SHADER_PARAM_TYPE_MATRIX, "1", "World to Texture Matrix" )
		SHADER_PARAM( FLASHLIGHTCOLOR, SHADER_PARAM_TYPE_VEC4, "1", "Flashlight color" )
		SHADER_PARAM( ATTENFACTORS, SHADER_PARAM_TYPE_VEC4, "1", "Packed attenuation factors" )
		SHADER_PARAM( ORIGINFARZ, SHADER_PARAM_TYPE_VEC4, "1", "Light origin packed with farz" )
		SHADER_PARAM( QUATORIENTATION, SHADER_PARAM_TYPE_VEC4, "1", "Light orientation (quaternion)" )
		SHADER_PARAM( SHADOWFILTERSIZE, SHADER_PARAM_TYPE_FLOAT, "3", "Shadow filter size" )
		SHADER_PARAM( SHADOWATTEN, SHADER_PARAM_TYPE_FLOAT, "1", "Shadow Attenuation" )
		SHADER_PARAM( SHADOWJITTERSEED, SHADER_PARAM_TYPE_FLOAT, "1", "Shadow jitter seed" )
		SHADER_PARAM( UBERLIGHT, SHADER_PARAM_TYPE_INTEGER, "1", "Is this an uberlight?" )
		SHADER_PARAM( ENABLESHADOWS, SHADER_PARAM_TYPE_INTEGER, "1", "Are shadows enabled?" )
		SHADER_PARAM( NOISESTRENGTH, SHADER_PARAM_TYPE_FLOAT, "1", "Strength of noise in volumetrics" )

		// Uberlight parameters
		SHADER_PARAM( UBERNEARFAR, SHADER_PARAM_TYPE_VEC4, "1", "Packed uberlight near and far parameters" )
		SHADER_PARAM( UBERHEIGHTWIDTH, SHADER_PARAM_TYPE_VEC4, "1", "Packed uberlight height and width parameters" )
		SHADER_PARAM( UBERROUNDNESS, SHADER_PARAM_TYPE_FLOAT, "1", "Uberlight roundness" )

		SHADER_PARAM( FLASHLIGHTTIME, SHADER_PARAM_TYPE_FLOAT, "0", "Typically driven by SFM, similar to jitter seed" )
		SHADER_PARAM( NUMPLANES, SHADER_PARAM_TYPE_FLOAT, "0", "Need to know this to normalize intensity" )

		SHADER_PARAM( VOLUMETRICINTENSITY, SHADER_PARAM_TYPE_FLOAT, "1", "Intensity of volumetrics" )

	END_SHADER_PARAMS

	void SetupVarsLightShafts( LightShaftsVars_t &info )
	{
		info.m_nCookieTexture = COOKIETEXTURE;
		info.m_nCookieFrameNum = COOKIEFRAMENUM;
		info.m_nShadowDepthTexture = SHADOWDEPTHTEXTURE;
		info.m_nNoiseTexture = NOISETEXTURE;
		info.m_nNoiseStrength = NOISESTRENGTH;

		info.m_nWorldToTexture = WORLDTOTEXTURE;
		info.m_nFlashlightColor = FLASHLIGHTCOLOR;
		info.m_nAttenFactors = ATTENFACTORS;
		info.m_nOriginFarZ = ORIGINFARZ;
		info.m_nQuatOrientation = QUATORIENTATION;
		info.m_nShadowFilterSize = SHADOWFILTERSIZE;
		info.m_nShadowAtten = SHADOWATTEN;
		info.m_nShadowJitterSeed = SHADOWJITTERSEED;
		info.m_nUberlight = UBERLIGHT;
		info.m_nEnableShadows = ENABLESHADOWS;

		info.m_nFlashlightTime = FLASHLIGHTTIME;
		info.m_nNumPlanes = NUMPLANES;

		// Uberlight parameters
		info.m_nUberNearFar = UBERNEARFAR;
		info.m_nUberHeightWidth = UBERHEIGHTWIDTH;
		info.m_nUberRoundness = UBERROUNDNESS;

		info.m_nVolumetricIntensity = VOLUMETRICINTENSITY;
	}

	SHADER_INIT_PARAMS()
	{
		LightShaftsVars_t info;
		SetupVarsLightShafts( info );
		InitParamsLightShafts( this, params, pMaterialName, info );
	}

	SHADER_FALLBACK
	{
		if ( !g_pHardwareConfig->HasFastVertexTextures() )
		{
			// Fallback to unlit generic
			return "Wireframe";
		}

		return 0;
	}

	SHADER_INIT
	{
		LightShaftsVars_t info;
		SetupVarsLightShafts( info );
		InitLightShafts( this, params, info );
	}

	SHADER_DRAW
	{
		LightShaftsVars_t info;
		SetupVarsLightShafts( info );
		DrawLightShafts( this, params, pShaderAPI, pShaderShadow, info, vertexCompression );
	}
END_SHADER
