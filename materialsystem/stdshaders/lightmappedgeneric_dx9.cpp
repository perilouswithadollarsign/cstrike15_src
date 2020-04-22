//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Lightmap only shader
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#include "BaseVSShader.h"
#include "convar.h"
#include "lightmappedgeneric_dx9_helper.h"
#include "lightmappedpaint_dx9_helper.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


static LightmappedGeneric_DX9_Vars_t s_info;


BEGIN_VS_SHADER( LightmappedGeneric,
				 "Help for LightmappedGeneric" )

				 BEGIN_SHADER_PARAMS
				 SHADER_PARAM( SELFILLUMTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Self-illumination tint" )
				 SHADER_PARAM( DETAIL, SHADER_PARAM_TYPE_TEXTURE, "shadertest/detail", "detail texture" )
				 SHADER_PARAM( DETAILFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $detail" )
				 SHADER_PARAM( DETAILSCALE, SHADER_PARAM_TYPE_FLOAT, "4", "scale of the detail texture" )
				 SHADER_PARAM( DETAIL2, SHADER_PARAM_TYPE_TEXTURE, "shadertest/detail", "detail texture 2" )
				 SHADER_PARAM( DETAILFRAME2, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $detail2" )
				 SHADER_PARAM( DETAILSCALE2, SHADER_PARAM_TYPE_FLOAT, "4", "scale of the detail texture 2" )

				 // detail (multi-) texturing
				 SHADER_PARAM( DETAILBLENDMODE, SHADER_PARAM_TYPE_INTEGER, "0", "mode for combining detail texture with base. 0=normal, 1= additive, 2=alpha blend detail over base, 3=crossfade" )
				 SHADER_PARAM( DETAILBLENDFACTOR, SHADER_PARAM_TYPE_FLOAT, "1", "blend amount for detail texture." )
				 SHADER_PARAM( DETAILTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "detail texture tint" )
				 SHADER_PARAM( DETAILBLENDFACTOR2, SHADER_PARAM_TYPE_FLOAT, "1", "blend amount for detail texture 2." )
				 SHADER_PARAM( DETAILTINT2, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "detail texture 2 tint" )

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
				 SHADER_PARAM( BUMPMAP2, SHADER_PARAM_TYPE_TEXTURE, "models/shadertest/shader3_normal", "bump map" )
				 SHADER_PARAM( BUMPFRAME2, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $bumpmap" )
				 SHADER_PARAM( BUMPTRANSFORM2, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$bumpmap texcoord transform" )
				 SHADER_PARAM( BUMPMASK, SHADER_PARAM_TYPE_TEXTURE, "models/shadertest/shader3_normal", "bump map" )
				 SHADER_PARAM( BASETEXTURE2, SHADER_PARAM_TYPE_TEXTURE, "shadertest/lightmappedtexture", "Blended texture" )
				 SHADER_PARAM( FRAME2, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $basetexture2" )
				 SHADER_PARAM( BLENDMODULATETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "texture to use r/g channels for blend range for" )
				 SHADER_PARAM( SSBUMP, SHADER_PARAM_TYPE_INTEGER, "0", "whether or not to use alternate bumpmap format with height" )
				 SHADER_PARAM( BUMP_FORCE_ON, SHADER_PARAM_TYPE_BOOL, "0", "force bump-mapping on, even for low-end machines" )
				 SHADER_PARAM( SEAMLESS_SCALE, SHADER_PARAM_TYPE_FLOAT, "0", "Scale factor for 'seamless' texture mapping. 0 means to use ordinary mapping" )
				 SHADER_PARAM( ALPHATESTREFERENCE, SHADER_PARAM_TYPE_FLOAT, "0.0", "" )
				 SHADER_PARAM( ENVMAPANISOTROPY, SHADER_PARAM_TYPE_BOOL, "0", "Enable anisotropic cubemap lookups for macroscopically rough/microscopically smooth surfaces, like wet asphalt" )
				 SHADER_PARAM( ENVMAPANISOTROPYSCALE, SHADER_PARAM_TYPE_FLOAT, "1.0", "Scale anisotropy amount for cubemap lookups" )
				 SHADER_PARAM( NOENVMAPMIP, SHADER_PARAM_TYPE_BOOL, "0", "Force envmap lookup to always use the top-level mip map" )
				 SHADER_PARAM( ADDBUMPMAPS, SHADER_PARAM_TYPE_BOOL, "0", "Add bump map 1 and 2 together, renormalize" )
				 SHADER_PARAM( BUMPDETAILSCALE1, SHADER_PARAM_TYPE_FLOAT, "1.0", "" )
				 SHADER_PARAM( BUMPDETAILSCALE2, SHADER_PARAM_TYPE_FLOAT, "1.0", "" )

#if defined( CSTRIKE15 )
				 SHADER_PARAM( SHADERSRGBREAD360, SHADER_PARAM_TYPE_BOOL, "1", "Simulate srgb read in shader code" )
#else
				 SHADER_PARAM( SHADERSRGBREAD360, SHADER_PARAM_TYPE_BOOL, "0", "Simulate srgb read in shader code")
#endif

				 SHADER_PARAM( ENVMAPLIGHTSCALE, SHADER_PARAM_TYPE_FLOAT, "0.0", "How much the lightmap effects environment map reflection, 0.0 is off, 1.0 will allow complete blackness of the environment map if the lightmap is black" )
				 SHADER_PARAM( ENVMAPLIGHTSCALEMINMAX, SHADER_PARAM_TYPE_VEC2, "[0.0 1.0]", "Thresholds for the lightmap envmap effect.  Setting the min higher increases the minimum light amount at which the envmap gets nerfed to nothing." )

				 SHADER_PARAM( PHONG, SHADER_PARAM_TYPE_BOOL, "1", "Phong" )
				 SHADER_PARAM( PHONGEXPONENT, SHADER_PARAM_TYPE_FLOAT, "5.0", "Phong exponent for Key (CSM Casting) Light - on $basetexture" )
				 SHADER_PARAM( PHONGEXPONENT2, SHADER_PARAM_TYPE_FLOAT, "5.0", "Phong exponent for Key (CSM Casting) Light - on $basetexture2" )
				 SHADER_PARAM( PHONGBASETINT, SHADER_PARAM_TYPE_FLOAT, "0.0", "Amount to tint Phong * $basetexture" )
				 SHADER_PARAM( PHONGBASETINT2, SHADER_PARAM_TYPE_FLOAT, "0.0", "Amount to tint Phong * $basetexture2" )
				 SHADER_PARAM( PHONGMASKCONTRASTBRIGHTNESS, SHADER_PARAM_TYPE_VEC2, "[1.0 0.0]", "Contrast and Brightness for Phong Mask generation using $basetexture" )
				 SHADER_PARAM( PHONGMASKCONTRASTBRIGHTNESS2, SHADER_PARAM_TYPE_VEC2, "[1.0 0.0]", "Contrast and Brightness for Phong Mask generation using $basetexture2" )
				 SHADER_PARAM( PHONGAMOUNT, SHADER_PARAM_TYPE_VEC4, "[1.0 1.0 1.0 1.0]", "Phong tint and amount on $basetexture > 1 to scale phong, .a is diffuseComponent multiplier, 0=> remove diffuse, >1 => overbright diffuse" )
				 SHADER_PARAM( PHONGAMOUNT2, SHADER_PARAM_TYPE_VEC4, "[1.0 1.0 1.0 1.0]", "Phong tint and amount on $basetexture2 > 1 to scale phong, .a is diffuseComponent multiplier, 0=> remove diffuse, >1 => overbright diffuse" )

				 SHADER_PARAM( BASETEXTURETRANSFORM2, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$basetexture2 texcoord transform" )

				 SHADER_PARAM( LAYERTINT1, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "color tint for layer 1" )
				 SHADER_PARAM( LAYERTINT2, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "color tint for layer 2" )

				 SHADER_PARAM( BLENDMODULATETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$blendmodulatetexture texcoord transform" )

				 SHADER_PARAM( ENVMAPMASK2, SHADER_PARAM_TYPE_TEXTURE, "shadertest/shadertest_envmask", "envmap mask 2" )
				 SHADER_PARAM( ENVMAPMASKFRAME2, SHADER_PARAM_TYPE_INTEGER, "", "" )
				 SHADER_PARAM( ENVMAPMASKTRANSFORM2, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$envmapmask2 texcoord transform" )

				 SHADER_PARAM( NEWLAYERBLENDING, SHADER_PARAM_TYPE_BOOL, "0", "Enable new layer blend math" )
				 SHADER_PARAM( BLENDSOFTNESS, SHADER_PARAM_TYPE_FLOAT, "0.5", "Layer blend softness" )
				 SHADER_PARAM( LAYERBORDERSTRENGTH, SHADER_PARAM_TYPE_FLOAT, "0.5", "Layer border strength" )
				 SHADER_PARAM( LAYERBORDEROFFSET, SHADER_PARAM_TYPE_FLOAT, "0.0", "Layer border offset" )
				 SHADER_PARAM( LAYERBORDERSOFTNESS, SHADER_PARAM_TYPE_FLOAT, "0.5", "Layer border softness" )
				 SHADER_PARAM( LAYERBORDERTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "color tint for layer border" )

				 SHADER_PARAM( LAYEREDGENORMAL, SHADER_PARAM_TYPE_BOOL, "0", "Enable normals on layer edge" )
				 SHADER_PARAM( LAYEREDGEPUNCHIN, SHADER_PARAM_TYPE_BOOL, "0", "Invert normals on layer edge" )
				 SHADER_PARAM( LAYEREDGESTRENGTH, SHADER_PARAM_TYPE_FLOAT, "0.5", "Layer edge normal strength" )
				 SHADER_PARAM( LAYEREDGESOFTNESS, SHADER_PARAM_TYPE_FLOAT, "0.5", "Layer edge normal blend softness" )
				 SHADER_PARAM( LAYEREDGEOFFSET, SHADER_PARAM_TYPE_FLOAT, "0.0", "Layer edge normal blend offset" )
END_SHADER_PARAMS

	void SetupVars( LightmappedGeneric_DX9_Vars_t& info )
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
		info.m_nDetailTint = DETAILTINT;

		info.m_nDetail2 = DETAIL2;
		info.m_nDetailFrame2 = DETAILFRAME2;
		info.m_nDetailScale2 = DETAILSCALE2;
		info.m_nDetailTextureBlendFactor2 = DETAILBLENDFACTOR2;
		info.m_nDetailTint2 = DETAILTINT2;

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
		info.m_nBumpmap2 = BUMPMAP2;
		info.m_nBumpFrame2 = BUMPFRAME2;
		info.m_nBumpTransform2 = BUMPTRANSFORM2;
		info.m_nBumpMask = BUMPMASK;
		info.m_nBaseTexture2 = BASETEXTURE2;
		info.m_nBaseTextureFrame2 = FRAME2;
		info.m_nBaseTextureTransform2 = BASETEXTURETRANSFORM2;
		info.m_nFlashlightTexture = FLASHLIGHTTEXTURE;
		info.m_nFlashlightTextureFrame = FLASHLIGHTTEXTUREFRAME;
		info.m_nBlendModulateTexture = BLENDMODULATETEXTURE;
		info.m_nSelfShadowedBumpFlag = SSBUMP;
		info.m_nForceBumpEnable = BUMP_FORCE_ON;
		info.m_nSeamlessMappingScale = SEAMLESS_SCALE;
		info.m_nAlphaTestReference = ALPHATESTREFERENCE;
		info.m_nEnvmapAnisotropy = ENVMAPANISOTROPY;
		info.m_nEnvmapAnisotropyScale = ENVMAPANISOTROPYSCALE;
		info.m_nNoEnvmapMip = NOENVMAPMIP;
		info.m_nAddBumpMaps = ADDBUMPMAPS;
		info.m_nBumpDetailScale1 = BUMPDETAILSCALE1;
		info.m_nBumpDetailScale2 = BUMPDETAILSCALE2;

		info.m_nShaderSrgbRead360 = SHADERSRGBREAD360;

		info.m_nEnvMapLightScale = ENVMAPLIGHTSCALE;
		info.m_nEnvMapLightScaleMinMax = ENVMAPLIGHTSCALEMINMAX;

		info.m_nPhong = PHONG;
		info.m_nPhongExp = PHONGEXPONENT;
		info.m_nPhongExp2 = PHONGEXPONENT2;
		info.m_nPhongBaseTint = PHONGBASETINT;
		info.m_nPhongBaseTint2 = PHONGBASETINT2;
		info.m_nPhongMaskContrastBrightness = PHONGMASKCONTRASTBRIGHTNESS;
		info.m_nPhongMaskContrastBrightness2 = PHONGMASKCONTRASTBRIGHTNESS2;
		info.m_nPhongAmount = PHONGAMOUNT;
		info.m_nPhongAmount2 = PHONGAMOUNT2;

		info.m_nLayerTint1 = LAYERTINT1;
		info.m_nLayerTint2 = LAYERTINT2;

		info.m_nBlendModulateTransform = BLENDMODULATETRANSFORM;

		info.m_nEnvmapMask2 = ENVMAPMASK2;
		info.m_nEnvmapMaskFrame2 = ENVMAPMASKFRAME2;
		info.m_nEnvmapMaskTransform2 = ENVMAPMASKTRANSFORM2;

		info.m_nAltLayerBlending = NEWLAYERBLENDING;
		info.m_nBlendSoftness = BLENDSOFTNESS;
		info.m_nLayerBorderStrength = LAYERBORDERSTRENGTH;
		info.m_nLayerBorderOffset = LAYERBORDEROFFSET;
		info.m_nLayerBorderSoftness = LAYERBORDERSOFTNESS;
		info.m_nLayerBorderTint = LAYERBORDERTINT;

		info.m_nLayerEdgePunchIn = LAYEREDGEPUNCHIN;
		info.m_nLayerEdgeStrength = LAYEREDGESTRENGTH;
		info.m_nLayerEdgeOffset = LAYEREDGEOFFSET;
		info.m_nLayerEdgeSoftness = LAYEREDGESOFTNESS;
		info.m_nLayerEdgeNormal = LAYEREDGENORMAL;
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	// Set up anything that is necessary to make decisions in SHADER_FALLBACK.
	SHADER_INIT_PARAMS()
	{
		SetupVars( s_info );
		InitParamsLightmappedGeneric_DX9( this, params, pMaterialName, s_info );
	}

	SHADER_INIT
	{
		SetupVars( s_info );
		InitLightmappedGeneric_DX9( this, params, s_info );
	}

	SHADER_DRAW
	{
		DrawLightmappedGeneric_DX9( this, params, pShaderAPI, pShaderShadow, s_info, pContextDataPtr );
	}

	void ExecuteFastPath( int *dynVSIdx, int *dynPSIdx,  IMaterialVar** params, IShaderDynamicAPI * pShaderAPI, 
		VertexCompressionType_t vertexCompression, CBasePerMaterialContextData **pContextDataPtr, BOOL bCSMEnabled )
	{
		*dynVSIdx = -1;
		*dynPSIdx = -1;

		DrawLightmappedGeneric_DX9_FastPath( dynVSIdx, dynPSIdx, this, params, pShaderAPI, s_info, pContextDataPtr, bCSMEnabled );
	}
END_SHADER
