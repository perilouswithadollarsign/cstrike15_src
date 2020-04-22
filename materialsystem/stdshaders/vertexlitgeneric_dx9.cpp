//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//=====================================================================================//

#include "BaseVSShader.h"
#include "vertexlitgeneric_dx9_helper.h"
#include "emissive_scroll_blended_pass_helper.h"
#include "cloak_blended_pass_helper.h"
#include "flesh_interior_blended_pass_helper.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


BEGIN_VS_SHADER( VertexLitGeneric, "Help for VertexLitGeneric" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( ALBEDO, SHADER_PARAM_TYPE_TEXTURE, "shadertest/BaseTexture", "albedo (Base texture with no baked lighting)" )
		SHADER_PARAM( COMPRESS, SHADER_PARAM_TYPE_TEXTURE, "shadertest/BaseTexture", "compression wrinklemap" )
		SHADER_PARAM( STRETCH, SHADER_PARAM_TYPE_TEXTURE, "shadertest/BaseTexture", "expansion wrinklemap" )
		SHADER_PARAM( SELFILLUMTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Self-illumination tint" )
		SHADER_PARAM( DETAIL, SHADER_PARAM_TYPE_TEXTURE, "shadertest/detail", "detail texture" )
		SHADER_PARAM( DETAILFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $detail" )
		SHADER_PARAM( DETAILSCALE, SHADER_PARAM_TYPE_FLOAT, "4", "scale of the detail texture" )
		SHADER_PARAM( ENVMAP, SHADER_PARAM_TYPE_TEXTURE, "shadertest/shadertest_env", "envmap" )
		SHADER_PARAM( ENVMAPFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "envmap frame number" )
		SHADER_PARAM( ENVMAPMASK, SHADER_PARAM_TYPE_TEXTURE, "shadertest/shadertest_envmask", "envmap mask" )
		SHADER_PARAM( ENVMAPMASKFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "" )
		SHADER_PARAM( ENVMAPMASKTRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$envmapmask texcoord transform" )
		SHADER_PARAM( ENVMAPTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "envmap tint" )
		SHADER_PARAM( BUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "models/shadertest/shader1_normal", "bump map" )
		SHADER_PARAM( BUMPCOMPRESS, SHADER_PARAM_TYPE_TEXTURE, "models/shadertest/shader3_normal", "compression bump map" )
		SHADER_PARAM( BUMPSTRETCH, SHADER_PARAM_TYPE_TEXTURE, "models/shadertest/shader1_normal", "expansion bump map" )
		SHADER_PARAM( BUMPFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $bumpmap" )
		SHADER_PARAM( BUMPTRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$bumpmap texcoord transform" )
		SHADER_PARAM( ENVMAPCONTRAST, SHADER_PARAM_TYPE_FLOAT, "0.0", "contrast 0 == normal 1 == color*color" )
		SHADER_PARAM( ENVMAPSATURATION, SHADER_PARAM_TYPE_FLOAT, "1.0", "saturation 0 == greyscale 1 == normal" )
 	    SHADER_PARAM( SELFILLUM_ENVMAPMASK_ALPHA, SHADER_PARAM_TYPE_FLOAT,"0.0","defines that self illum value comes from env map mask alpha" )
		SHADER_PARAM( SELFILLUMFRESNEL, SHADER_PARAM_TYPE_BOOL, "0", "Self illum fresnel" )
		SHADER_PARAM( SELFILLUMFRESNELMINMAXEXP, SHADER_PARAM_TYPE_VEC4, "0", "Self illum fresnel min, max, exp" )
		SHADER_PARAM( SELFILLUMMASKSCALE, SHADER_PARAM_TYPE_FLOAT, "0", "Scale self illum effect strength" )
		SHADER_PARAM( SELFILLUMFRESNELENABLEDTHISFRAME, SHADER_PARAM_TYPE_BOOL, "0", "Self illum fresnel" )

		SHADER_PARAM( ALPHATESTREFERENCE, SHADER_PARAM_TYPE_FLOAT, "0.0", "" )	
		SHADER_PARAM( ALLOWFENCERENDERSTATEHACK, SHADER_PARAM_TYPE_BOOL, "0", "" )
		SHADER_PARAM( FLASHLIGHTNOLAMBERT, SHADER_PARAM_TYPE_BOOL, "0", "Flashlight pass sets N.L=1.0" )
		SHADER_PARAM( LOWQUALITYFLASHLIGHTSHADOWS, SHADER_PARAM_TYPE_BOOL, "0", "Force low quality flashlight shadows (faster)" )
		SHADER_PARAM( DISABLECSMLOOKUP, SHADER_PARAM_TYPE_BOOL, "0", "Force no CSM lookup/filter (faster)" )

		// Debugging term for visualizing ambient data on its own
		SHADER_PARAM( AMBIENTONLY, SHADER_PARAM_TYPE_INTEGER, "0", "Control drawing of non-ambient light ()" )

		SHADER_PARAM( PHONGEXPONENT, SHADER_PARAM_TYPE_FLOAT, "5.0", "Phong exponent for local specular lights" )
		SHADER_PARAM( PHONGTINT, SHADER_PARAM_TYPE_VEC3, "5.0", "Phong tint for local specular lights" )
		SHADER_PARAM( PHONGALBEDOTINT, SHADER_PARAM_TYPE_BOOL, "1.0", "Apply tint by albedo (controlled by spec exponent texture" )
		SHADER_PARAM( LIGHTWARPTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "shadertest/BaseTexture", "1D ramp texture for tinting scalar diffuse term" )
		SHADER_PARAM( PHONGWARPTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "shadertest/BaseTexture", "warp the specular term" )
		SHADER_PARAM( PHONGFRESNELRANGES, SHADER_PARAM_TYPE_VEC3, "[0  0.5  1]", "Parameters for remapping fresnel output" )
		SHADER_PARAM( PHONGBOOST, SHADER_PARAM_TYPE_FLOAT, "1.0", "Phong overbrightening factor (specular mask channel should be authored to account for this)" )
		SHADER_PARAM( PHONGALBEDOBOOST, SHADER_PARAM_TYPE_FLOAT, "1.0", "Phong albedo overbrightening factor (specular mask channel should be authored to account for this)" )
		SHADER_PARAM( PHONGEXPONENTTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "shadertest/BaseTexture", "Phong Exponent map" )
		SHADER_PARAM( PHONG, SHADER_PARAM_TYPE_BOOL, "0", "enables phong lighting" )
		SHADER_PARAM( FORCEPHONG, SHADER_PARAM_TYPE_BOOL, "0", "forces Phong lighting, even at low GPU levels (Phong must already be enabled)" )
		SHADER_PARAM( BASEMAPALPHAPHONGMASK, SHADER_PARAM_TYPE_INTEGER, "0", "indicates that there is no normal map and that the phong mask is in base alpha" )
		SHADER_PARAM( INVERTPHONGMASK, SHADER_PARAM_TYPE_INTEGER, "0", "invert the phong mask (0=full phong, 1=no phong)" )
		SHADER_PARAM( ENVMAPFRESNEL, SHADER_PARAM_TYPE_FLOAT, "0", "Degree to which Fresnel should be applied to env map" )
		SHADER_PARAM( SELFILLUMMASK, SHADER_PARAM_TYPE_TEXTURE, "shadertest/BaseTexture", "If we bind a texture here, it overrides base alpha (if any) for self illum" )
		SHADER_PARAM( BASEMAPLUMINANCEPHONGMASK, SHADER_PARAM_TYPE_INTEGER, "0", "indicates that the base luminance should be used to mask phong" )

	    // detail texturing
	    SHADER_PARAM( DETAILBLENDMODE, SHADER_PARAM_TYPE_INTEGER, "0", "mode for combining detail texture with base. 0=normal, 1= additive, 2=alpha blend detail over base, 3=crossfade" )
		SHADER_PARAM( DETAILBLENDFACTOR, SHADER_PARAM_TYPE_FLOAT, "1", "blend amount for detail texture." )
		SHADER_PARAM( DETAILTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "detail texture tint" )
		SHADER_PARAM( DETAILTEXTURETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$detail texcoord transform" )

		// Rim lighting terms
		SHADER_PARAM( RIMLIGHT, SHADER_PARAM_TYPE_BOOL, "0", "enables rim lighting" )
		SHADER_PARAM( RIMLIGHTEXPONENT, SHADER_PARAM_TYPE_FLOAT, "4.0", "Exponent for rim lights" )
		SHADER_PARAM( RIMLIGHTBOOST, SHADER_PARAM_TYPE_FLOAT, "1.0", "Boost for rim lights" )
		SHADER_PARAM( RIMMASK, SHADER_PARAM_TYPE_BOOL, "0", "Indicates whether or not to use alpha channel of exponent texture to mask the rim term" )

	    // Seamless mapping scale
		SHADER_PARAM( SEAMLESS_BASE, SHADER_PARAM_TYPE_BOOL, "0", "whether to apply seamless mapping to the base texture. requires a smooth model." )
		SHADER_PARAM( SEAMLESS_DETAIL, SHADER_PARAM_TYPE_BOOL, "0", "where to apply seamless mapping to the detail texture." )
	    SHADER_PARAM( SEAMLESS_SCALE, SHADER_PARAM_TYPE_FLOAT, "1.0", "the scale for the seamless mapping. # of repetions of texture per inch." )

		// Emissive Scroll Pass
		SHADER_PARAM( EMISSIVEBLENDENABLED, SHADER_PARAM_TYPE_BOOL, "0", "Enable emissive blend pass" )
		SHADER_PARAM( EMISSIVEBLENDBASETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "self-illumination map" )
		SHADER_PARAM( EMISSIVEBLENDSCROLLVECTOR, SHADER_PARAM_TYPE_VEC2, "[0.11 0.124]", "Emissive scroll vec" )
		SHADER_PARAM( EMISSIVEBLENDSTRENGTH, SHADER_PARAM_TYPE_FLOAT, "1.0", "Emissive blend strength" )
		SHADER_PARAM( EMISSIVEBLENDTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "self-illumination map" )
		SHADER_PARAM( EMISSIVEBLENDTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Self-illumination tint" )
		SHADER_PARAM( EMISSIVEBLENDFLOWTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "flow map" )
		SHADER_PARAM( TIME, SHADER_PARAM_TYPE_FLOAT, "0.0", "Needs CurrentTime Proxy" )

		// Cloak Pass
		SHADER_PARAM( CLOAKPASSENABLED, SHADER_PARAM_TYPE_BOOL, "0", "Enables cloak render in a second pass" )
		SHADER_PARAM( CLOAKFACTOR, SHADER_PARAM_TYPE_FLOAT, "0.0", "" )
		SHADER_PARAM( CLOAKCOLORTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Cloak color tint" )
		SHADER_PARAM( REFRACTAMOUNT, SHADER_PARAM_TYPE_FLOAT, "2", "" )

		// Flesh Interior Pass
		SHADER_PARAM( FLESHINTERIORENABLED, SHADER_PARAM_TYPE_BOOL, "0", "Enable Flesh interior blend pass" )
		SHADER_PARAM( FLESHINTERIORTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Flesh color texture" )
		SHADER_PARAM( FLESHINTERIORNOISETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Flesh noise texture" )
		SHADER_PARAM( FLESHBORDERTEXTURE1D, SHADER_PARAM_TYPE_TEXTURE, "", "Flesh border 1D texture" )
		SHADER_PARAM( FLESHNORMALTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Flesh normal texture" )
		SHADER_PARAM( FLESHSUBSURFACETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Flesh subsurface texture" )
		SHADER_PARAM( FLESHCUBETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Flesh cubemap texture" )
		SHADER_PARAM( FLESHBORDERNOISESCALE, SHADER_PARAM_TYPE_FLOAT, "1.5", "Flesh Noise UV scalar for border" )
		SHADER_PARAM( FLESHDEBUGFORCEFLESHON, SHADER_PARAM_TYPE_BOOL, "0", "Flesh Debug full flesh" )
		SHADER_PARAM( FLESHEFFECTCENTERRADIUS1, SHADER_PARAM_TYPE_VEC4, "[0 0 0 0.001]", "Flesh effect center and radius" )
		SHADER_PARAM( FLESHEFFECTCENTERRADIUS2, SHADER_PARAM_TYPE_VEC4, "[0 0 0 0.001]", "Flesh effect center and radius" )
		SHADER_PARAM( FLESHEFFECTCENTERRADIUS3, SHADER_PARAM_TYPE_VEC4, "[0 0 0 0.001]", "Flesh effect center and radius" )
		SHADER_PARAM( FLESHEFFECTCENTERRADIUS4, SHADER_PARAM_TYPE_VEC4, "[0 0 0 0.001]", "Flesh effect center and radius" )
		SHADER_PARAM( FLESHSUBSURFACETINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Subsurface Color" )
		SHADER_PARAM( FLESHBORDERWIDTH, SHADER_PARAM_TYPE_FLOAT, "0.3", "Flesh border" )
		SHADER_PARAM( FLESHBORDERSOFTNESS, SHADER_PARAM_TYPE_FLOAT, "0.42", "Flesh border softness (> 0.0 && <= 0.5)" )
		SHADER_PARAM( FLESHBORDERTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Flesh border Color" )
		SHADER_PARAM( FLESHGLOBALOPACITY, SHADER_PARAM_TYPE_FLOAT, "1.0", "Flesh global opacity" )
		SHADER_PARAM( FLESHGLOSSBRIGHTNESS, SHADER_PARAM_TYPE_FLOAT, "0.66", "Flesh gloss brightness" )
		SHADER_PARAM( FLESHSCROLLSPEED, SHADER_PARAM_TYPE_FLOAT, "1.0", "Flesh scroll speed" )

		SHADER_PARAM( SEPARATEDETAILUVS, SHADER_PARAM_TYPE_BOOL, "0", "Use texcoord1 for detail texture" )
		SHADER_PARAM( LINEARWRITE, SHADER_PARAM_TYPE_INTEGER, "0", "Disables SRGB conversion of shader results." )

#if defined( CSTRIKE15 )
		SHADER_PARAM( SHADERSRGBREAD360, SHADER_PARAM_TYPE_BOOL, "1", "Simulate srgb read in shader code")
#else
		SHADER_PARAM( SHADERSRGBREAD360, SHADER_PARAM_TYPE_BOOL, "0", "Simulate srgb read in shader code")
#endif

		SHADER_PARAM( AMBIENTOCCLUSION, SHADER_PARAM_TYPE_FLOAT, "0.0", "Amount of screen space ambient occlusion to use (0..1 range)")

		SHADER_PARAM( DISPLACEMENTMAP, SHADER_PARAM_TYPE_TEXTURE, "shadertest/BaseTexture", "Displacement map" )
		SHADER_PARAM( DISPLACEMENTWRINKLE, SHADER_PARAM_TYPE_BOOL, "0", "Displacement map contains wrinkle displacements")

		SHADER_PARAM( BLENDTINTBYBASEALPHA, SHADER_PARAM_TYPE_BOOL, "0", "Use the base alpha to blend in the $color modulation")
		SHADER_PARAM( NOTINT, SHADER_PARAM_TYPE_BOOL, "0", "Disable tinting" )

		SHADER_PARAM( DESATURATEWITHBASEALPHA, SHADER_PARAM_TYPE_FLOAT, "0.0", "Use the base alpha to desaturate the base texture.  Set to non-zero to enable, value gets multiplied into the alpha channel before desaturating.")

		SHADER_PARAM( ALLOWDIFFUSEMODULATION, SHADER_PARAM_TYPE_BOOL, "1", "Allow per-instance color modulation")

		// vertexlitgeneric envmap fresnel control
		SHADER_PARAM( ENVMAPFRESNELMINMAXEXP, SHADER_PARAM_TYPE_VEC3, "[0.0 1.0 2.0]", "Min/max fresnel range and exponent for vertexlitgeneric" )
		SHADER_PARAM( BASEALPHAENVMAPMASKMINMAXEXP, SHADER_PARAM_TYPE_VEC3, "[1.0 0.0 1.0]", "" )

		// This is to allow phong materials to disable half lambert. Half lambert has always been forced on in phong,
		// so the only safe way to allow artists to disable half lambert is to create this param that disables the
		// default behavior of forcing half lambert on.
		SHADER_PARAM( PHONGDISABLEHALFLAMBERT, SHADER_PARAM_TYPE_BOOL, "0", "Disable half lambert for phong")

		// vertexlitgeneric tree sway animation control
		SHADER_PARAM( TREESWAY, SHADER_PARAM_TYPE_INTEGER, "0", "" );
		SHADER_PARAM( TREESWAYHEIGHT, SHADER_PARAM_TYPE_FLOAT, "1000", "" );
		SHADER_PARAM( TREESWAYSTARTHEIGHT, SHADER_PARAM_TYPE_FLOAT, "0.2", "" );
		SHADER_PARAM( TREESWAYRADIUS, SHADER_PARAM_TYPE_FLOAT, "300", "" );
		SHADER_PARAM( TREESWAYSTARTRADIUS, SHADER_PARAM_TYPE_FLOAT, "0.1", "" );
		SHADER_PARAM( TREESWAYSPEED, SHADER_PARAM_TYPE_FLOAT, "1", "" );
		SHADER_PARAM( TREESWAYSPEEDHIGHWINDMULTIPLIER, SHADER_PARAM_TYPE_FLOAT, "2", "" );
		SHADER_PARAM( TREESWAYSTRENGTH, SHADER_PARAM_TYPE_FLOAT, "10", "" );
		SHADER_PARAM( TREESWAYSCRUMBLESPEED, SHADER_PARAM_TYPE_FLOAT, "0.1", "" );
		SHADER_PARAM( TREESWAYSCRUMBLESTRENGTH, SHADER_PARAM_TYPE_FLOAT, "0.1", "" );
		SHADER_PARAM( TREESWAYSCRUMBLEFREQUENCY, SHADER_PARAM_TYPE_FLOAT, "0.1", "" );
		SHADER_PARAM( TREESWAYFALLOFFEXP, SHADER_PARAM_TYPE_FLOAT, "1.5", "" );
		SHADER_PARAM( TREESWAYSCRUMBLEFALLOFFEXP, SHADER_PARAM_TYPE_FLOAT, "1.0", "" );
		SHADER_PARAM( TREESWAYSPEEDLERPSTART, SHADER_PARAM_TYPE_FLOAT, "3", "" );
		SHADER_PARAM( TREESWAYSPEEDLERPEND, SHADER_PARAM_TYPE_FLOAT, "6", "" );
		SHADER_PARAM( TREESWAYSTATIC, SHADER_PARAM_TYPE_BOOL, "0", "" );

		SHADER_PARAM( DECALTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Decal texture" )
		SHADER_PARAM( DECALBLENDMODE, SHADER_PARAM_TYPE_INTEGER, "0", "mode for combining decal texture with base. 0=normal(decal*srca + base*(1-srca), 1= mod, 2=mod2x, 3=additive" )

		SHADER_PARAM( ENVMAPLIGHTSCALE, SHADER_PARAM_TYPE_FLOAT, "0.0", "How much the lightmap effects environment map reflection, 0.0 is off, 1.0 will allow complete blackness of the environment map if the lightmap is black" )
		SHADER_PARAM( ENVMAPLIGHTSCALEMINMAX, SHADER_PARAM_TYPE_VEC2, "[0.0 1.0]", "Thresholds for the lightmap envmap effect.  Setting the min higher increases the minimum light amount at which the envmap gets nerfed to nothing." )

		SHADER_PARAM( BLENDWITHSMOKEGRENADE, SHADER_PARAM_TYPE_BOOL, "0", "" );
		SHADER_PARAM( BLENDWITHSMOKEGRENADEPOSENTITY, SHADER_PARAM_TYPE_VEC3, "[0 0 0]", "" );
		SHADER_PARAM( BLENDWITHSMOKEGRENADEPOSSMOKE, SHADER_PARAM_TYPE_VEC3, "[0 0 0]", "" );

		SHADER_PARAM( MODELDECALIGNOREZ, SHADER_PARAM_TYPE_BOOL, "0", "" );

		SHADER_PARAM( TINTMASKTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Separate tint mask texture (as opposed to using basetexture alpha)" )
		SHADER_PARAM( ENVMAPMASKINTINTMASKTEXTURE, SHADER_PARAM_TYPE_BOOL, "0", "Envmap mask is stored in ting mask texture (instead of normal map alpha)" )

	END_SHADER_PARAMS

	void SetupVars( VertexLitGeneric_DX9_Vars_t& info )
	{
		info.m_nBaseTexture = BASETEXTURE;
		info.m_nWrinkle = COMPRESS;
		info.m_nStretch = STRETCH;
		info.m_nBaseTextureFrame = FRAME;
		info.m_nBaseTextureTransform = BASETEXTURETRANSFORM;
		info.m_nAlbedo = ALBEDO;
		info.m_nSelfIllumTint = SELFILLUMTINT;
		info.m_nDetail = DETAIL;
		info.m_nDetailFrame = DETAILFRAME;
		info.m_nDetailScale = DETAILSCALE;
		info.m_nEnvmap = ENVMAP;
		info.m_nEnvmapFrame = ENVMAPFRAME;
		info.m_nEnvmapMask = ENVMAPMASK;
		info.m_nEnvmapMaskFrame = ENVMAPMASKFRAME;
		info.m_nEnvmapMaskTransform = ENVMAPMASKTRANSFORM;
		info.m_nEnvmapTint = ENVMAPTINT;
		info.m_nBumpmap = BUMPMAP;
		info.m_nNormalWrinkle = BUMPCOMPRESS;
		info.m_nNormalStretch = BUMPSTRETCH;
		info.m_nBumpFrame = BUMPFRAME;
		info.m_nBumpTransform = BUMPTRANSFORM;
		info.m_nEnvmapContrast = ENVMAPCONTRAST;
		info.m_nEnvmapSaturation = ENVMAPSATURATION;
		info.m_nAlphaTestReference = ALPHATESTREFERENCE;
		info.m_nAllowFenceRenderStateHack = ALLOWFENCERENDERSTATEHACK;
		info.m_nFlashlightNoLambert = FLASHLIGHTNOLAMBERT;
		info.m_nLowQualityFlashlightShadows = LOWQUALITYFLASHLIGHTSHADOWS;
		info.m_nDisableCSMLookup = DISABLECSMLOOKUP;

		info.m_nFlashlightTexture = FLASHLIGHTTEXTURE;
		info.m_nFlashlightTextureFrame = FLASHLIGHTTEXTUREFRAME;
		info.m_nSelfIllumEnvMapMask_Alpha = SELFILLUM_ENVMAPMASK_ALPHA;
		info.m_nSelfIllumFresnel = SELFILLUMFRESNEL;
		info.m_nSelfIllumFresnelMinMaxExp = SELFILLUMFRESNELMINMAXEXP;
		info.m_nSelfIllumMaskScale = SELFILLUMMASKSCALE;
		info.m_nSelfIllumFresnelEnabledThisFrame = SELFILLUMFRESNELENABLEDTHISFRAME;

		info.m_nAmbientOnly = AMBIENTONLY;
		info.m_nPhongExponent = PHONGEXPONENT;
		info.m_nPhongExponentTexture = PHONGEXPONENTTEXTURE;
		info.m_nPhongTint = PHONGTINT;
		info.m_nPhongAlbedoTint = PHONGALBEDOTINT;
		info.m_nDiffuseWarpTexture = LIGHTWARPTEXTURE;
		info.m_nPhongWarpTexture = PHONGWARPTEXTURE;
		info.m_nPhongBoost = PHONGBOOST;
		info.m_nPhongAlbedoBoost = PHONGALBEDOBOOST;
		info.m_nPhongFresnelRanges = PHONGFRESNELRANGES;
		info.m_nPhong = PHONG;
		info.m_nForcePhong = FORCEPHONG;
		info.m_nBaseMapAlphaPhongMask = BASEMAPALPHAPHONGMASK;
		info.m_nEnvmapFresnel = ENVMAPFRESNEL;
		info.m_nDetailTextureCombineMode = DETAILBLENDMODE;
		info.m_nDetailTextureBlendFactor = DETAILBLENDFACTOR;
		info.m_nDetailTextureTransform = DETAILTEXTURETRANSFORM;

		info.m_nBaseMapLuminancePhongMask = BASEMAPLUMINANCEPHONGMASK;

		// Rim lighting parameters
		info.m_nRimLight = RIMLIGHT;
		info.m_nRimLightPower = RIMLIGHTEXPONENT;
		info.m_nRimLightBoost = RIMLIGHTBOOST;
		info.m_nRimMask = RIMMASK;

		// seamless
		info.m_nSeamlessScale = SEAMLESS_SCALE;
		info.m_nSeamlessDetail = SEAMLESS_DETAIL;
		info.m_nSeamlessBase = SEAMLESS_BASE;

		info.m_nSeparateDetailUVs = SEPARATEDETAILUVS;

		info.m_nLinearWrite = LINEARWRITE;
		info.m_nDetailTint = DETAILTINT;
		info.m_nInvertPhongMask = INVERTPHONGMASK;

		info.m_nSelfIllumMask = SELFILLUMMASK;

		info.m_nShaderSrgbRead360 = SHADERSRGBREAD360;

		info.m_nAmbientOcclusion = AMBIENTOCCLUSION;

		info.m_nBlendTintByBaseAlpha = BLENDTINTBYBASEALPHA;
		info.m_nNoTint = NOTINT;

		info.m_nDesaturateWithBaseAlpha = DESATURATEWITHBASEALPHA;

		info.m_nAllowDiffuseModulation = ALLOWDIFFUSEMODULATION;

		info.m_nEnvMapFresnelMinMaxExp = ENVMAPFRESNELMINMAXEXP;
		info.m_nBaseAlphaEnvMapMaskMinMaxExp = BASEALPHAENVMAPMASKMINMAXEXP;
		info.m_nDisplacementMap = DISPLACEMENTMAP;

		info.m_nDisplacementWrinkleMap = DISPLACEMENTWRINKLE;

		info.m_nPhongDisableHalfLambert = PHONGDISABLEHALFLAMBERT;

		info.m_nTreeSway = TREESWAY;
		info.m_nTreeSwayHeight = TREESWAYHEIGHT;
		info.m_nTreeSwayStartHeight = TREESWAYSTARTHEIGHT;
		info.m_nTreeSwayRadius = TREESWAYRADIUS;
		info.m_nTreeSwayStartRadius = TREESWAYSTARTRADIUS;
		info.m_nTreeSwaySpeed = TREESWAYSPEED;
		info.m_nTreeSwaySpeedHighWindMultiplier = TREESWAYSPEEDHIGHWINDMULTIPLIER;
		info.m_nTreeSwayStrength = TREESWAYSTRENGTH;
		info.m_nTreeSwayScrumbleSpeed = TREESWAYSCRUMBLESPEED;
		info.m_nTreeSwayScrumbleStrength = TREESWAYSCRUMBLESTRENGTH;
		info.m_nTreeSwayScrumbleFrequency = TREESWAYSCRUMBLEFREQUENCY;
		info.m_nTreeSwayFalloffExp = TREESWAYFALLOFFEXP;
		info.m_nTreeSwayScrumbleFalloffExp = TREESWAYSCRUMBLEFALLOFFEXP;		
		info.m_nTreeSwaySpeedLerpStart = TREESWAYSPEEDLERPSTART;
		info.m_nTreeSwaySpeedLerpEnd = TREESWAYSPEEDLERPEND;
		info.m_nTreeSwayStatic = TREESWAYSTATIC;

		info.m_nDecalTexture = DECALTEXTURE;
		info.m_nDecalTextureCombineMode = DECALBLENDMODE;
		
		info.m_nEnvMapLightScale = ENVMAPLIGHTSCALE;
		info.m_nEnvMapLightScaleMinMax = ENVMAPLIGHTSCALEMINMAX;

		info.m_nBlendWithSmokeGrenade = BLENDWITHSMOKEGRENADE;
		info.m_nBlendWithSmokeGrenadePosEntity = BLENDWITHSMOKEGRENADEPOSENTITY;
		info.m_nBlendWithSmokeGrenadePosSmoke = BLENDWITHSMOKEGRENADEPOSSMOKE;

		info.m_nModelDecalIgnoreZ = MODELDECALIGNOREZ;

		info.m_nTintMaskTexture = TINTMASKTEXTURE;
		info.m_nEnvMapMaskInTintMaskTexture = ENVMAPMASKINTINTMASKTEXTURE;
	}

	// Cloak Pass
	void SetupVarsCloakBlendedPass( CloakBlendedPassVars_t &info )
	{
		info.m_nCloakFactor = CLOAKFACTOR;
		info.m_nCloakColorTint = CLOAKCOLORTINT;
		info.m_nRefractAmount = REFRACTAMOUNT;

		// Delete these lines if not bump mapping!
		info.m_nBumpmap = BUMPMAP;
		info.m_nBumpFrame = BUMPFRAME;
		info.m_nBumpTransform = BUMPTRANSFORM;
	}

	bool NeedsPowerOfTwoFrameBufferTexture( IMaterialVar **params, bool bCheckSpecificToThisFrame ) const 
	{ 
		if ( params[CLOAKPASSENABLED]->GetIntValue() ) // If material supports cloaking
		{
			if ( bCheckSpecificToThisFrame == false ) // For setting model flag at load time
				return true;
			else if ( ( params[CLOAKFACTOR]->GetFloatValue() > 0.0f ) && ( params[CLOAKFACTOR]->GetFloatValue() < 1.0f ) ) // Per-frame check
				return true;
			// else, not cloaking this frame, so check flag2 in case the base material still needs it
		}

		// Check flag2 if not drawing cloak pass
		return IS_FLAG2_SET( MATERIAL_VAR2_NEEDS_POWER_OF_TWO_FRAME_BUFFER_TEXTURE ); 
	}

	bool IsTranslucent( IMaterialVar **params ) const
	{
		if ( params[CLOAKPASSENABLED]->GetIntValue() ) // If material supports cloaking
		{
			if ( ( params[CLOAKFACTOR]->GetFloatValue() > 0.0f ) && ( params[CLOAKFACTOR]->GetFloatValue() < 1.0f ) ) // Per-frame check
				return true;
			// else, not cloaking this frame, so check flag in case the base material still needs it
		}

		// Check flag if not drawing cloak pass
		return IS_FLAG_SET( MATERIAL_VAR_TRANSLUCENT ); 
	}

	// Emissive Scroll Pass
	void SetupVarsEmissiveScrollBlendedPass( EmissiveScrollBlendedPassVars_t &info )
	{
		info.m_nBlendStrength = EMISSIVEBLENDSTRENGTH;
		info.m_nBaseTexture = EMISSIVEBLENDBASETEXTURE;
		info.m_nFlowTexture = EMISSIVEBLENDFLOWTEXTURE;
		info.m_nEmissiveTexture = EMISSIVEBLENDTEXTURE;
		info.m_nEmissiveTint = EMISSIVEBLENDTINT;
		info.m_nEmissiveScrollVector = EMISSIVEBLENDSCROLLVECTOR;
		info.m_nTime = TIME;
	}

	// Flesh Interior Pass
	void SetupVarsFleshInteriorBlendedPass( FleshInteriorBlendedPassVars_t &info )
	{
		info.m_nFleshTexture = FLESHINTERIORTEXTURE;
		info.m_nFleshNoiseTexture = FLESHINTERIORNOISETEXTURE;
		info.m_nFleshBorderTexture1D = FLESHBORDERTEXTURE1D;
		info.m_nFleshNormalTexture = FLESHNORMALTEXTURE;
		info.m_nFleshSubsurfaceTexture = FLESHSUBSURFACETEXTURE;
		info.m_nFleshCubeTexture = FLESHCUBETEXTURE;

		info.m_nflBorderNoiseScale = FLESHBORDERNOISESCALE;
		info.m_nflDebugForceFleshOn = FLESHDEBUGFORCEFLESHON;
		info.m_nvEffectCenterRadius1 = FLESHEFFECTCENTERRADIUS1;
		info.m_nvEffectCenterRadius2 = FLESHEFFECTCENTERRADIUS2;
		info.m_nvEffectCenterRadius3 = FLESHEFFECTCENTERRADIUS3;
		info.m_nvEffectCenterRadius4 = FLESHEFFECTCENTERRADIUS4;

		info.m_ncSubsurfaceTint = FLESHSUBSURFACETINT;
		info.m_nflBorderWidth = FLESHBORDERWIDTH;
		info.m_nflBorderSoftness = FLESHBORDERSOFTNESS;
		info.m_ncBorderTint = FLESHBORDERTINT;
		info.m_nflGlobalOpacity = FLESHGLOBALOPACITY;
		info.m_nflGlossBrightness = FLESHGLOSSBRIGHTNESS;
		info.m_nflScrollSpeed = FLESHSCROLLSPEED;

		info.m_nTime = TIME;
	}

	SHADER_INIT_PARAMS()
	{
		bool bDisableTreeSway = false; 
		
		// Disable tree swaying on consoles, or on "cheap" shadow filter modes (which are too pixelated).
		if ( IsGameConsole() || IsPlatformOSX() )
#if defined( CSTRIKE15 ) 
			if( IsPlatformOSX() || IsPS3() )
			{
				bDisableTreeSway = true;
			}
#else
			bDisableTreeSway = true;
#endif
		else if ( g_pHardwareConfig->GetShadowFilterMode( false /* bForceLowQuality */, g_pHardwareConfig->SupportsPixelShaders_3_0() /* bPS30 */ ) >= SHADOWFILTERMODE_FIRST_CHEAP_MODE )
		{
			bDisableTreeSway = true;
		}

		
		if ( bDisableTreeSway )
		{
			params[TREESWAY]->SetIntValue( 0 );
		}

		VertexLitGeneric_DX9_Vars_t vars;
		SetupVars( vars );
		InitParamsVertexLitGeneric_DX9( this, params, pMaterialName, true, vars );

		// Cloak Pass
		if ( !params[CLOAKPASSENABLED]->IsDefined() )
		{
			params[CLOAKPASSENABLED]->SetIntValue( 0 );
		}
		else if ( params[CLOAKPASSENABLED]->GetIntValue() )
		{
			CloakBlendedPassVars_t info;
			SetupVarsCloakBlendedPass( info );
			InitParamsCloakBlendedPass( this, params, pMaterialName, info );
		}

		// Emissive Scroll Pass
		if ( !params[EMISSIVEBLENDENABLED]->IsDefined() )
		{
			params[EMISSIVEBLENDENABLED]->SetIntValue( 0 );
		}
		else if ( params[EMISSIVEBLENDENABLED]->GetIntValue() )
		{
			EmissiveScrollBlendedPassVars_t info;
			SetupVarsEmissiveScrollBlendedPass( info );
			InitParamsEmissiveScrollBlendedPass( this, params, pMaterialName, info );
		}

		// Flesh Interior Pass
		if ( !params[FLESHINTERIORENABLED]->IsDefined() )
		{
			params[FLESHINTERIORENABLED]->SetIntValue( 0 );
		}
		else if ( params[FLESHINTERIORENABLED]->GetIntValue() )
		{
			FleshInteriorBlendedPassVars_t info;
			SetupVarsFleshInteriorBlendedPass( info );
			InitParamsFleshInteriorBlendedPass( this, params, pMaterialName, info );
		}
		
		if ( !params[PHONGBOOST]->IsDefined() )
		{
			params[PHONGBOOST]->SetFloatValue( 1.0f );
		}

		if ( !params[PHONGFRESNELRANGES]->IsDefined() )
		{
			params[PHONGFRESNELRANGES]->SetVecValue( 1.0f, 1.0f, 1.0f );
		}

		if ( !params[PHONGALBEDOBOOST]->IsDefined() )
		{
			if ( params[PHONGBOOST]->IsDefined() )
			{
				params[PHONGALBEDOBOOST]->SetFloatValue( params[PHONGBOOST]->GetFloatValue() );
			}
			else
			{
				params[PHONGALBEDOBOOST]->SetFloatValue( 1.0f );
			}
		}
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		VertexLitGeneric_DX9_Vars_t vars;
		SetupVars( vars );
		InitVertexLitGeneric_DX9( this, params, true, vars );

		// Cloak Pass
		if ( params[CLOAKPASSENABLED]->GetIntValue() )
		{
			CloakBlendedPassVars_t info;
			SetupVarsCloakBlendedPass( info );
			InitCloakBlendedPass( this, params, info );
		}

		// Emissive Scroll Pass
		if ( params[EMISSIVEBLENDENABLED]->GetIntValue() )
		{
			EmissiveScrollBlendedPassVars_t info;
			SetupVarsEmissiveScrollBlendedPass( info );
			InitEmissiveScrollBlendedPass( this, params, info );
		}

		// Flesh Interior Pass
		if ( params[FLESHINTERIORENABLED]->GetIntValue() )
		{
			FleshInteriorBlendedPassVars_t info;
			SetupVarsFleshInteriorBlendedPass( info );
			InitFleshInteriorBlendedPass( this, params, info );
		}
	}

	SHADER_DRAW
	{
		// Skip the standard rendering if cloak pass is fully opaque
		bool bDrawStandardPass = true;
		if ( params[CLOAKPASSENABLED]->GetIntValue() && ( pShaderShadow == NULL ) ) // && not snapshotting
		{
			CloakBlendedPassVars_t info;
			SetupVarsCloakBlendedPass( info );
			if ( CloakBlendedPassIsFullyOpaque( params, info ) )
			{
				bDrawStandardPass = false;
			}
		}

		// Standard rendering pass
		if ( bDrawStandardPass )
		{
			VertexLitGeneric_DX9_Vars_t vars;
			SetupVars( vars );
			DrawVertexLitGeneric_DX9( this, params, pShaderAPI, pShaderShadow, true, vars, vertexCompression, pContextDataPtr );
		}
		else
		{
			// Skip this pass!
			Draw( false );
		}

		// Cloak Pass
		if ( params[CLOAKPASSENABLED]->GetIntValue() )
		{
			// If ( snapshotting ) or ( we need to draw this frame )
 			if ( ( pShaderShadow != NULL ) || ( ( params[CLOAKFACTOR]->GetFloatValue() > 0.0f ) && ( params[CLOAKFACTOR]->GetFloatValue() < 1.0f ) ) )
			{
				CloakBlendedPassVars_t info;
				SetupVarsCloakBlendedPass( info );
				DrawCloakBlendedPass( this, params, pShaderAPI, pShaderShadow, info, vertexCompression );
			}
			else // We're not snapshotting and we don't need to draw this frame
			{
				// Skip this pass!
				Draw( false );
			}
		}

		// Emissive Scroll Pass
		if ( params[EMISSIVEBLENDENABLED]->GetIntValue() )
		{
			// If ( snapshotting ) or ( we need to draw this frame )
			if ( ( pShaderShadow != NULL ) || ( params[EMISSIVEBLENDSTRENGTH]->GetFloatValue() > 0.0f ) )
			{
				EmissiveScrollBlendedPassVars_t info;
				SetupVarsEmissiveScrollBlendedPass( info );
				DrawEmissiveScrollBlendedPass( this, params, pShaderAPI, pShaderShadow, info, vertexCompression );
			}
			else // We're not snapshotting and we don't need to draw this frame
			{
				// Skip this pass!
				Draw( false );
			}
		}

		// Flesh Interior Pass
		if ( params[FLESHINTERIORENABLED]->GetIntValue() )
		{
			// If ( snapshotting ) or ( we need to draw this frame )
			if ( ( pShaderShadow != NULL ) || ( true ) )
			{
				FleshInteriorBlendedPassVars_t info;
				SetupVarsFleshInteriorBlendedPass( info );
				DrawFleshInteriorBlendedPass( this, params, pShaderAPI, pShaderShadow, info, vertexCompression );
			}
			else // We're not snapshotting and we don't need to draw this frame
			{
				// Skip this pass!
				Draw( false );
			}
		}
	}


	void ExecuteFastPath( int *dynVSIdx, int *dynPSIdx,  IMaterialVar** params, IShaderDynamicAPI * pShaderAPI, 
		VertexCompressionType_t vertexCompression, CBasePerMaterialContextData **pContextDataPtr, BOOL bCSMEnabled )
	{
		*dynVSIdx = -1;
		*dynPSIdx = -1;

		VertexLitGeneric_DX9_Vars_t vars;
		SetupVars( vars );

		if ( params[CLOAKPASSENABLED]->GetIntValue() || params[EMISSIVEBLENDENABLED]->GetIntValue() || 
			 params[FLESHINTERIORENABLED]->GetIntValue() )
		{
			return;
		}
				
		// Only draw standard pass for now
		// GS_TODO: Other passes
		DrawVertexLitGeneric_DX9_ExecuteFastPath( dynVSIdx, dynPSIdx, this, params, pShaderAPI, vars, vertexCompression, 
			pContextDataPtr, bCSMEnabled );

	}

END_SHADER
