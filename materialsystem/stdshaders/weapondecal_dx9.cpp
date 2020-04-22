//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Shader for decals on weapon models
//
//==========================================================================//

#include "BaseVSShader.h"
#include "weapondecal_dx9_helper.h"
#include "cpp_shader_constant_register_map.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DEFINE_FALLBACK_SHADER( WeaponDecal, WeaponDecal_dx9 )

	BEGIN_VS_SHADER( WeaponDecal_dx9, "WeaponDecal" )
	BEGIN_SHADER_PARAMS
	
	SHADER_PARAM( THIRDPERSON, SHADER_PARAM_TYPE_BOOL, "0", "Third-person mode" )
	SHADER_PARAM( AOTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "shader/AOTexture", "R: cavity, G: AO, B: mig shading" )
	SHADER_PARAM( EXPTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "shader/ExpTexture", "RGB: Exponent texture" )
	SHADER_PARAM( NORMALMAP, SHADER_PARAM_TYPE_TEXTURE, "dev/flatnormal", "Normal Map" )
	SHADER_PARAM( HOLOSPECTRUM, SHADER_PARAM_TYPE_TEXTURE, "", "RGB: Hologram spectrum texture" )
	SHADER_PARAM( HOLOMASK, SHADER_PARAM_TYPE_TEXTURE, "", "RGB: Hologram mask texture: R " )
	SHADER_PARAM( GRUNGETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "shader/PosTexture", "RGB: Tiling grunge A: spec, black dulls the paint finish" )
	SHADER_PARAM( WEARTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "shader/PosTexture", "RGB: Greyscale wear speed factor, white wears soonest" )
	SHADER_PARAM( ANISODIRTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "black", "RG give direction vector, A masks specularity" )
	SHADER_PARAM( MIRRORHORIZONTAL, SHADER_PARAM_TYPE_BOOL, "0", "Flip decal horizontally" )
	SHADER_PARAM( WEARREMAPMIN, SHADER_PARAM_TYPE_FLOAT, "0.0", "" )
	SHADER_PARAM( WEARREMAPMID, SHADER_PARAM_TYPE_FLOAT, "0.5", "" )
	SHADER_PARAM( WEARREMAPMAX, SHADER_PARAM_TYPE_FLOAT, "1.0", "" )
	SHADER_PARAM( WEARWIDTHMIN, SHADER_PARAM_TYPE_FLOAT, "0.3", "" )
	SHADER_PARAM( WEARWIDTHMAX, SHADER_PARAM_TYPE_FLOAT, "0.06", "" )
	SHADER_PARAM( ALPHAMASK, SHADER_PARAM_TYPE_INTEGER, "0", "" )
	//SHADER_PARAM( FASTWEARTHRESHOLD, SHADER_PARAM_TYPE_FLOAT, "0.0", "" )
	SHADER_PARAM( HIGHLIGHT, SHADER_PARAM_TYPE_FLOAT, "0.0", "Highlight the edges of the sticker")
	SHADER_PARAM( HIGHLIGHTCYCLE, SHADER_PARAM_TYPE_FLOAT, "0.0", "Scroll the highlight effect")
	SHADER_PARAM( PEEL, SHADER_PARAM_TYPE_FLOAT, "0.0", "An effect that 'peels' the sticker in local uv space")
	SHADER_PARAM( ENVMAP, SHADER_PARAM_TYPE_TEXTURE, "env_cubemap", "Environment cubemap" )
	SHADER_PARAM( ENVMAPTINT, SHADER_PARAM_TYPE_VEC3, "[ 1 1 1 ]", "Envmap tint" )
	SHADER_PARAM( ALPHA, SHADER_PARAM_TYPE_INTEGER, "1", "Alpha" )
	SHADER_PARAM( PHONG, SHADER_PARAM_TYPE_INTEGER, "0", "Enable phong shading" )
	SHADER_PARAM( PHONGEXPONENT, SHADER_PARAM_TYPE_INTEGER, "4", "Phong exponent value" )
	SHADER_PARAM( PHONGFRESNELRANGES, SHADER_PARAM_TYPE_VEC3, "[ 1 1 1 ]", "Phong fresnel ranges" )
	SHADER_PARAM( PHONGALBEDOTINT, SHADER_PARAM_TYPE_BOOL, "0", "Phong albedo tint" )
	SHADER_PARAM( PHONGBOOST, SHADER_PARAM_TYPE_FLOAT, "1", "Phong boost" )
	SHADER_PARAM( PHONGALBEDOBOOST, SHADER_PARAM_TYPE_FLOAT, "1", "Phong albedo boost" )
	SHADER_PARAM( DECALSTYLE, SHADER_PARAM_TYPE_INTEGER, "0", "Decal style: 0 = Plastic/PVC sticker" )
	//SHADER_PARAM( PATTERNTEXTURETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "logo texcoord transform" )
	SHADER_PARAM( PATTERNROTATION, SHADER_PARAM_TYPE_FLOAT, "0.0", "Pattern rotation" )
	SHADER_PARAM( PATTERNSCALE, SHADER_PARAM_TYPE_FLOAT, "1.0", "Pattern scale" )
	SHADER_PARAM( COLORTINT4, SHADER_PARAM_TYPE_VEC3, "[ 1 1 1 ]", "Diffuse tint" )
	SHADER_PARAM( COLORTINT3, SHADER_PARAM_TYPE_VEC3, "[ 1 1 1 ]", "Diffuse tint" )
	SHADER_PARAM( COLORTINT2, SHADER_PARAM_TYPE_VEC3, "[ 1 1 1 ]", "Diffuse tint" )
	SHADER_PARAM( COLORTINT, SHADER_PARAM_TYPE_VEC3, "[ 1 1 1 ]", "Diffuse tint" )
	SHADER_PARAM( WEARBIAS, SHADER_PARAM_TYPE_FLOAT, "0.5", "" )
	SHADER_PARAM( UNWEARSTRENGTH, SHADER_PARAM_TYPE_FLOAT, "0.1", "" )
	SHADER_PARAM( WEARPROGRESS, SHADER_PARAM_TYPE_FLOAT, "0.5", "Wear/worn progress amount" )
	SHADER_PARAM( GRUNGESCALE, SHADER_PARAM_TYPE_FLOAT, "2", "" )
	SHADER_PARAM( DESATBASETINT, SHADER_PARAM_TYPE_FLOAT, "0", "Desaturate basemap and affect tint" )
	END_SHADER_PARAMS

	void SetupVarsWeaponDecal( WeaponDecalVars_t &info )
{
	info.m_nBaseTexture = BASETEXTURE;
	info.m_nBaseTextureTransform = BASETEXTURETRANSFORM;
	info.m_nAOTexture = AOTEXTURE;
	info.m_nNormalMap = NORMALMAP;
	info.m_nWearRemapMin = WEARREMAPMIN;
	info.m_nWearRemapMid = WEARREMAPMID;
	info.m_nWearRemapMax = WEARREMAPMAX;
	info.m_nWearProgress = WEARPROGRESS;
	info.m_nWearWidthMin = WEARWIDTHMIN;
	info.m_nWearWidthMax = WEARWIDTHMAX;
	info.m_nUnWearStrength = UNWEARSTRENGTH;
	info.m_nExpTexture = EXPTEXTURE;
	info.m_nHologramSpectrum = HOLOSPECTRUM;
	info.m_nHologramMask = HOLOMASK;
	info.m_nGrungeTexture = GRUNGETEXTURE;
	info.m_nWearTexture = WEARTEXTURE;
	info.m_nAnisoDirTexture = ANISODIRTEXTURE;
	info.m_nEnvmapTexture = ENVMAP;
	info.m_nEnvmapTint = ENVMAPTINT;
	info.m_nColorTint = COLORTINT;
	info.m_nColorTint2 = COLORTINT2;
	info.m_nColorTint3 = COLORTINT3;
	info.m_nColorTint4 = COLORTINT4;
	info.m_nPhong = PHONG;
	info.m_nPhongExponent = PHONGEXPONENT;
	info.m_nPhongFresnelRanges = PHONGFRESNELRANGES;
	info.m_nPhongAlbedoTint = PHONGALBEDOTINT;
	info.m_nPhongBoost = PHONGBOOST;
	info.m_nPhongAlbedoBoost = PHONGALBEDOBOOST;
	info.m_nDecalStyle = DECALSTYLE;
	info.m_nAlpha = ALPHA;
	//info.m_nPatternTextureTransform = PATTERNTEXTURETRANSFORM;
	info.m_nPatternRotation = PATTERNROTATION;
	info.m_nPatternScale = PATTERNSCALE;
	info.m_nMirrorHorizontal = MIRRORHORIZONTAL;
	info.m_nThirdPerson = THIRDPERSON;
	info.m_nHighlight = HIGHLIGHT;
	info.m_nHighlightCycle = HIGHLIGHTCYCLE;
	info.m_nPeel = PEEL;
	info.m_nWearBias = WEARBIAS;
	info.m_nAlphaMask = ALPHAMASK;
	//info.m_nFastWearThreshold = FASTWEARTHRESHOLD;
	info.m_nGrungeScale = GRUNGESCALE;
	info.m_nDesatBaseTint = DESATBASETINT;
}

SHADER_INIT_PARAMS()
{
	WeaponDecalVars_t info;
	SetupVarsWeaponDecal( info );
	InitParamsWeaponDecal( this, params, pMaterialName, info );
}

SHADER_FALLBACK
{
	return 0;
}

SHADER_INIT
{
	WeaponDecalVars_t info;
	SetupVarsWeaponDecal( info );
	InitWeaponDecal( this, params, info );
}

SHADER_DRAW
{
	WeaponDecalVars_t info;
	SetupVarsWeaponDecal( info );
	DrawWeaponDecal( this, params, pShaderAPI, pShaderShadow, info, vertexCompression, pContextDataPtr );
}
END_SHADER
