//========= Copyright © 1996-2014, Valve Corporation, All rights reserved. ============//
//
// Purpose: Multiblend shader
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#include "BaseVSShader.h"
#include "convar.h"
#include "lightmapped_4wayblend_dx9_helper.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


static Lightmapped_4WayBlend_DX9_Vars_t s_info;


BEGIN_VS_SHADER( Lightmapped_4WayBlend,
				 "Help for Lightmapped_4WayBlend" )

	BEGIN_SHADER_PARAMS
		SHADER_PARAM( SELFILLUMTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Self-illumination tint" )
		SHADER_PARAM( DETAIL, SHADER_PARAM_TYPE_TEXTURE, "shadertest/detail", "detail texture" )
		SHADER_PARAM( DETAILFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $detail" )
		SHADER_PARAM( DETAILSCALE, SHADER_PARAM_TYPE_FLOAT, "4", "scale of the detail texture" )

		// detail (multi-) texturing
		SHADER_PARAM( DETAILBLENDMODE, SHADER_PARAM_TYPE_INTEGER, "0", "mode for combining detail texture with base. 0=normal, 1= additive, 2=alpha blend detail over base, 3=crossfade" )
		SHADER_PARAM( DETAILBLENDFACTOR, SHADER_PARAM_TYPE_FLOAT, "1", "blend amount for detail texture." )
		SHADER_PARAM( DETAILBLENDFACTOR2, SHADER_PARAM_TYPE_FLOAT, "1", "blend amount for detail texture on texture 2." )
		SHADER_PARAM( DETAILBLENDFACTOR3, SHADER_PARAM_TYPE_FLOAT, "1", "blend amount for detail texture on texture 3." )
		SHADER_PARAM( DETAILBLENDFACTOR4, SHADER_PARAM_TYPE_FLOAT, "1", "blend amount for detail texture on texture 4." )
		SHADER_PARAM( DETAILTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "detail texture tint" )

		SHADER_PARAM( ENVMAP, SHADER_PARAM_TYPE_TEXTURE, "shadertest/shadertest_env", "envmap" )
		SHADER_PARAM( ENVMAPFRAME, SHADER_PARAM_TYPE_INTEGER, "", "" )
		SHADER_PARAM( ENVMAPMASK, SHADER_PARAM_TYPE_TEXTURE, "shadertest/shadertest_envmask", "envmap mask" )
		SHADER_PARAM( ENVMAPMASKFRAME, SHADER_PARAM_TYPE_INTEGER, "", "" )
		SHADER_PARAM( ENVMAPMASKTRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$envmapmask texcoord transform" )
		SHADER_PARAM( ENVMAPTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "envmap tint" )
		SHADER_PARAM( BUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "models/shadertest/shader1_normal", "bump map" )
		SHADER_PARAM( BUMPFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $bumpmap" )
		SHADER_PARAM( BUMPTRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$bumpmap texcoord transform" )
		SHADER_PARAM( ENVMAPCONTRAST, SHADER_PARAM_TYPE_FLOAT, "0.0", "contrast 0 == normal 1 == color*color" )
		SHADER_PARAM( ENVMAPSATURATION, SHADER_PARAM_TYPE_FLOAT, "1.0", "saturation 0 == greyscale 1 == normal" )
		SHADER_PARAM( FRESNELREFLECTION, SHADER_PARAM_TYPE_FLOAT, "1.0", "1.0 == mirror, 0.0 == water" )
		SHADER_PARAM( NODIFFUSEBUMPLIGHTING, SHADER_PARAM_TYPE_INTEGER, "0", "0 == Use diffuse bump lighting, 1 = No diffuse bump lighting" )
		SHADER_PARAM( TEXTURE1_LUMSTART, SHADER_PARAM_TYPE_FLOAT, "0.0", "texture 1 start component for lum smoothstep" )
		SHADER_PARAM( TEXTURE1_LUMEND, SHADER_PARAM_TYPE_FLOAT, "1.0", "texture 1 end component for lum smoothstep" )
		SHADER_PARAM( BUMPMAP2, SHADER_PARAM_TYPE_TEXTURE, "models/shadertest/shader3_normal", "bump map" )
		SHADER_PARAM( BUMPFRAME2, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $bumpmap" )
		SHADER_PARAM( BUMPTRANSFORM2, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$bumpmap texcoord transform" )
		SHADER_PARAM( BASETEXTURE2, SHADER_PARAM_TYPE_TEXTURE, "shadertest/lightmappedtexture", "Blended texture" )
		SHADER_PARAM( FRAME2, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $basetexture2" )
		SHADER_PARAM( TEXTURE2_UVSCALE, SHADER_PARAM_TYPE_FLOAT, "1.0", "uniform scale texture 2 uv coords" )
		SHADER_PARAM( TEXTURE2_BLENDSTART, SHADER_PARAM_TYPE_FLOAT, "0.0", "texture 2 start component for blend smoothstep" )
		SHADER_PARAM( TEXTURE2_BLENDEND, SHADER_PARAM_TYPE_FLOAT, "1.0", "texture 2 end component for blend smoothstep" )
		SHADER_PARAM( TEXTURE2_LUMSTART, SHADER_PARAM_TYPE_FLOAT, "0.0", "texture 2 start component for lum smoothstep" )
		SHADER_PARAM( TEXTURE2_LUMEND, SHADER_PARAM_TYPE_FLOAT, "1.0", "texture 2 end component for lum smoothstep" )
		SHADER_PARAM( TEXTURE2_BUMPBLENDFACTOR, SHADER_PARAM_TYPE_FLOAT, "1.0", "texture 2 how much the bump blends in (ignored if bumpmap2 exists)" )
		SHADER_PARAM( BASETEXTURE3, SHADER_PARAM_TYPE_TEXTURE, "shadertest/lightmappedtexture", "Blended texture 3" )
		SHADER_PARAM( TEXTURE3_BLENDMODE, SHADER_PARAM_TYPE_INTEGER, "0", "blendmode for texture 3. 0 = blend, 1 = multiply 2x" )
		SHADER_PARAM( FRAME3, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $basetexture3" )
		SHADER_PARAM( TEXTURE3_UVSCALE, SHADER_PARAM_TYPE_FLOAT, "1.0", "uniform scale texture 3 uv coords" )
		SHADER_PARAM( TEXTURE3_BLENDSTART, SHADER_PARAM_TYPE_FLOAT, "0.0", "texture 3 start component for blend smoothstep" )
		SHADER_PARAM( TEXTURE3_BLENDEND, SHADER_PARAM_TYPE_FLOAT, "1.0", "texture 3 end component for blend smoothstep" )
		SHADER_PARAM( TEXTURE3_LUMSTART, SHADER_PARAM_TYPE_FLOAT, "0.0", "texture 3 start component for lum smoothstep" )
		SHADER_PARAM( TEXTURE3_LUMEND, SHADER_PARAM_TYPE_FLOAT, "1.0", "texture 3 end component for lum smoothstep" )
		SHADER_PARAM( TEXTURE3_BUMPBLENDFACTOR, SHADER_PARAM_TYPE_FLOAT, "1.0", "texture 3 how much the bump blends in" )
		SHADER_PARAM( BASETEXTURE4, SHADER_PARAM_TYPE_TEXTURE, "shadertest/lightmappedtexture", "Blended texture 4" )
		SHADER_PARAM( TEXTURE4_BLENDMODE, SHADER_PARAM_TYPE_INTEGER, "0", "blendmode for texture 4. 0 = blend, 1 = multiply 2x" )
		SHADER_PARAM( FRAME4, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $basetexture4" )
		SHADER_PARAM( TEXTURE4_UVSCALE, SHADER_PARAM_TYPE_FLOAT, "1.0", "uniform scale texture 4 uv coords" )
		SHADER_PARAM( TEXTURE4_BLENDSTART, SHADER_PARAM_TYPE_FLOAT, "0.0", "texture 4 start component for blend smoothstep" )
		SHADER_PARAM( TEXTURE4_BLENDEND, SHADER_PARAM_TYPE_FLOAT, "1.0", "texture 4 end component for blend smoothstep" )
		SHADER_PARAM( TEXTURE4_LUMSTART, SHADER_PARAM_TYPE_FLOAT, "0.0", "texture 4 start component for lum smoothstep" )
		SHADER_PARAM( TEXTURE4_LUMEND, SHADER_PARAM_TYPE_FLOAT, "1.0", "texture 4 end component for lum smoothstep" )
		SHADER_PARAM( TEXTURE4_BUMPBLENDFACTOR, SHADER_PARAM_TYPE_FLOAT, "1.0", "texture 4 how much the bump blends in" )
		SHADER_PARAM( SSBUMP, SHADER_PARAM_TYPE_INTEGER, "0", "whether or not to use alternate bumpmap format with height" )
		SHADER_PARAM( BUMP_FORCE_ON, SHADER_PARAM_TYPE_BOOL, "0", "force bump-mapping on, even for low-end machines" )
		SHADER_PARAM( SEAMLESS_SCALE, SHADER_PARAM_TYPE_FLOAT, "0", "Scale factor for 'seamless' texture mapping. 0 means to use ordinary mapping" )
		SHADER_PARAM( ALPHATESTREFERENCE, SHADER_PARAM_TYPE_FLOAT, "0.0", "" )	
		SHADER_PARAM( LUMBLENDFACTOR2, SHADER_PARAM_TYPE_FLOAT, "1.0", "blend factor between the luminance of layer 2 and layer one" )
		SHADER_PARAM( LUMBLENDFACTOR3, SHADER_PARAM_TYPE_FLOAT, "1.0", "blend factor between the luminance of layer 3 and layers below it" )
		SHADER_PARAM( LUMBLENDFACTOR4, SHADER_PARAM_TYPE_FLOAT, "1.0", "blend factor between the luminance of layer 4 and layers below it" )
		SHADER_PARAM( ENVMAPANISOTROPY, SHADER_PARAM_TYPE_BOOL, "0", "Enable anisotropic cubemap lookups for macroscopically rough/microscopically smooth surfaces, like wet asphalt" )
		SHADER_PARAM( ENVMAPANISOTROPYSCALE, SHADER_PARAM_TYPE_FLOAT, "1.0", "Scale anisotropy amount for cubemap lookups" )

#if defined( CSTRIKE15 )
		SHADER_PARAM( SHADERSRGBREAD360, SHADER_PARAM_TYPE_BOOL, "1", "Simulate srgb read in shader code")
#else
		SHADER_PARAM( SHADERSRGBREAD360, SHADER_PARAM_TYPE_BOOL, "0", "Simulate srgb read in shader code")
#endif

		SHADER_PARAM( ENVMAPLIGHTSCALE, SHADER_PARAM_TYPE_FLOAT, "0.0", "How much the lightmap effects environment map reflection, 0.0 is off, 1.0 will allow complete blackness of the environment map if the lightmap is black" )
		SHADER_PARAM( ENVMAPLIGHTSCALEMINMAX, SHADER_PARAM_TYPE_VEC2, "[0.0 1.0]", "Thresholds for the lightmap envmap effect.  Setting the min higher increases the minimum light amount at which the envmap gets nerfed to nothing." )

END_SHADER_PARAMS

	void SetupVars( Lightmapped_4WayBlend_DX9_Vars_t& info )
	{
		info.m_nBaseTexture = BASETEXTURE;
		info.m_nBaseTextureFrame = FRAME;
		info.m_nBaseTextureTransform = BASETEXTURETRANSFORM;
		info.m_nSelfIllumTint = SELFILLUMTINT;

		info.m_nDetail = DETAIL;
		info.m_nDetailFrame = DETAILFRAME;
		info.m_nDetailScale = DETAILSCALE;
		info.m_nDetailTextureCombineMode = DETAILBLENDMODE;
		info.m_nDetailTextureBlendFactor = DETAILBLENDFACTOR;
		info.m_nDetailTextureBlendFactor2 = DETAILBLENDFACTOR2;
		info.m_nDetailTextureBlendFactor3 = DETAILBLENDFACTOR3;
		info.m_nDetailTextureBlendFactor4 = DETAILBLENDFACTOR4;
		info.m_nDetailTint = DETAILTINT;

		info.m_nEnvmap = ENVMAP;
		info.m_nEnvmapFrame = ENVMAPFRAME;
		info.m_nEnvmapMask = ENVMAPMASK;
		info.m_nEnvmapMaskFrame = ENVMAPMASKFRAME;
		info.m_nEnvmapMaskTransform = ENVMAPMASKTRANSFORM;
		info.m_nEnvmapTint = ENVMAPTINT;
		info.m_nBumpmap = BUMPMAP;
		info.m_nBumpFrame = BUMPFRAME;
		info.m_nBumpTransform = BUMPTRANSFORM;
		info.m_nEnvmapContrast = ENVMAPCONTRAST;
		info.m_nEnvmapSaturation = ENVMAPSATURATION;
		info.m_nFresnelReflection = FRESNELREFLECTION;
		info.m_nNoDiffuseBumpLighting = NODIFFUSEBUMPLIGHTING;
		info.m_nTexture1LumStart = TEXTURE1_LUMSTART;
		info.m_nTexture1LumEnd = TEXTURE1_LUMEND;
		info.m_nBumpmap2 = BUMPMAP2;
		info.m_nBumpFrame2 = BUMPFRAME2;
		info.m_nBumpTransform2 = BUMPTRANSFORM2;
		info.m_nBaseTexture2 = BASETEXTURE2;
		info.m_nBaseTexture2Frame = FRAME2;
		info.m_nTexture2uvScale = TEXTURE2_UVSCALE;
		info.m_nTexture2BlendStart = TEXTURE2_BLENDSTART;
		info.m_nTexture2BlendEnd = TEXTURE2_BLENDEND;
		info.m_nTexture2LumStart = TEXTURE2_LUMSTART;
		info.m_nTexture2LumEnd = TEXTURE2_LUMEND;
		info.m_nTexture2BumpBlendFactor = TEXTURE2_BUMPBLENDFACTOR;
		info.m_nBaseTexture3 = BASETEXTURE3;
		info.m_nTexture3BlendMode = TEXTURE3_BLENDMODE;
		info.m_nBaseTexture3Frame = FRAME3;
		info.m_nTexture3uvScale = TEXTURE3_UVSCALE;
		info.m_nTexture3BlendStart = TEXTURE3_BLENDSTART;
		info.m_nTexture3BlendEnd = TEXTURE3_BLENDEND;
		info.m_nTexture3LumStart = TEXTURE3_LUMSTART;
		info.m_nTexture3LumEnd = TEXTURE3_LUMEND;
		info.m_nTexture3BumpBlendFactor = TEXTURE3_BUMPBLENDFACTOR;
		info.m_nBaseTexture4 = BASETEXTURE4;
		info.m_nTexture4BlendMode = TEXTURE4_BLENDMODE;
		info.m_nBaseTexture4Frame = FRAME4;
		info.m_nTexture4uvScale = TEXTURE4_UVSCALE;
		info.m_nTexture4BlendStart = TEXTURE4_BLENDSTART;
		info.m_nTexture4BlendEnd = TEXTURE4_BLENDEND;
		info.m_nTexture4LumStart = TEXTURE4_LUMSTART;
		info.m_nTexture4LumEnd = TEXTURE4_LUMEND;
		info.m_nTexture4BumpBlendFactor = TEXTURE4_BUMPBLENDFACTOR;
		info.m_nLumBlendFactor2 = LUMBLENDFACTOR2;
		info.m_nLumBlendFactor3 = LUMBLENDFACTOR3;
		info.m_nLumBlendFactor4 = LUMBLENDFACTOR4;
		info.m_nFlashlightTexture = FLASHLIGHTTEXTURE;
		info.m_nFlashlightTextureFrame = FLASHLIGHTTEXTUREFRAME;
		info.m_nSelfShadowedBumpFlag = SSBUMP;
		info.m_nForceBumpEnable = BUMP_FORCE_ON;
		info.m_nSeamlessMappingScale = SEAMLESS_SCALE;
		info.m_nAlphaTestReference = ALPHATESTREFERENCE;
		info.m_nEnvmapAnisotropy = ENVMAPANISOTROPY;
		info.m_nEnvmapAnisotropyScale = ENVMAPANISOTROPYSCALE;

		info.m_nShaderSrgbRead360 = SHADERSRGBREAD360;

		info.m_nEnvMapLightScale = ENVMAPLIGHTSCALE;
		info.m_nEnvMapLightScaleMinMax = ENVMAPLIGHTSCALEMINMAX;
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	// Set up anything that is necessary to make decisions in SHADER_FALLBACK.
	SHADER_INIT_PARAMS()
	{
		SetupVars( s_info );
		InitParamsLightmapped_4WayBlend_DX9( this, params, pMaterialName, s_info );
	}

	SHADER_INIT
	{
		SetupVars( s_info );
		InitLightmapped_4WayBlend_DX9( this, params, s_info );
	}

	SHADER_DRAW
	{
		DrawLightmapped_4WayBlend_DX9( this, params, pShaderAPI, pShaderShadow, s_info, pContextDataPtr );
	}

	void ExecuteFastPath( int *dynVSIdx, int *dynPSIdx,  IMaterialVar** params, IShaderDynamicAPI * pShaderAPI, 
		VertexCompressionType_t vertexCompression, CBasePerMaterialContextData **pContextDataPtr, BOOL bCSMEnabled )
	{
		*dynVSIdx = -1;
		*dynPSIdx = -1;

		DrawLightmapped_4WayBlend_DX9_FastPath( dynVSIdx, dynPSIdx, this, params, pShaderAPI, s_info, pContextDataPtr, bCSMEnabled );
	}
END_SHADER
