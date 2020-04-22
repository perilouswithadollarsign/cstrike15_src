//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Shader for compositing weapon textures
//
//==========================================================================//

#include "BaseVSShader.h"
#include "customweapon_dx9_helper.h"
#include "cpp_shader_constant_register_map.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DEFINE_FALLBACK_SHADER( CustomWeapon, CustomWeapon_dx9 )

	BEGIN_VS_SHADER( CustomWeapon_dx9, "CustomWeapon" )
	BEGIN_SHADER_PARAMS

	SHADER_PARAM( SURFACETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "shader/SurfaceTexture", "RGB: Object-space normal, A: Cavity" )
	SHADER_PARAM( AOTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "shader/AOTexture", "RGB: Ambient Occlusion" )
	SHADER_PARAM( MASKSTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "shader/MasksTexture", "R: Anodized, G: Camo2, B: Camo3, A: Black is paintable, White reveals original basetexture" )
	SHADER_PARAM( POSTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "shader/PosTexture", "RGB: High-precision object-space position" )
	SHADER_PARAM( GRUNGETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "shader/PosTexture", "RGB: Tiling grunge A: spec, black dulls the paint finish" )
	SHADER_PARAM( WEARTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "shader/PosTexture", "RGB: Greyscale wear speed factor, white wears soonest" )
	SHADER_PARAM( PAINTTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "shader/PosTexture", "RGB: High-precision object-space position" )
	SHADER_PARAM( EXPTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "shader/PosTexture", "RGB: Exponent texture" )

	SHADER_PARAM( CAMOCOLOR0, SHADER_PARAM_TYPE_VEC3, "[ 0 0 0 ]", "Customization color 0" )
	SHADER_PARAM( CAMOCOLOR1, SHADER_PARAM_TYPE_VEC3, "[ 0 0 0 ]", "Customization color 1" )
	SHADER_PARAM( CAMOCOLOR2, SHADER_PARAM_TYPE_VEC3, "[ 0 0 0 ]", "Customization color 2" )
	SHADER_PARAM( CAMOCOLOR3, SHADER_PARAM_TYPE_VEC3, "[ 0 0 0 ]", "Customization color 3" )

	SHADER_PARAM( BASEDIFFUSEOVERRIDE, SHADER_PARAM_TYPE_INTEGER, "0", "Set if this customization style needs a different (non-default) underlying diffuse map for this weapon" )
	SHADER_PARAM( PHONGEXPONENT, SHADER_PARAM_TYPE_INTEGER, "4", "Phong exponent value" )
	SHADER_PARAM( WEARPROGRESS, SHADER_PARAM_TYPE_FLOAT, "0.45", "Wear/worn progress amount" )
	SHADER_PARAM( PHONGALBEDOBOOST, SHADER_PARAM_TYPE_FLOAT, "1", "Phong albedo boost value for anodized metallic effect" )
	SHADER_PARAM( PHONGINTENSITY, SHADER_PARAM_TYPE_INTEGER, "10", "Phong intensity value" )
	SHADER_PARAM( PAINTSTYLE, SHADER_PARAM_TYPE_INTEGER, "0", "Indicates a specific customization style: 0 = none, 1 = solid, 2 = hydrographic, 3 = spray, 4 = anodized, 5 = anodized multicolored, 6 = anodized airbrushed, 7 = custom texture, 8 = patina/antiqued" )
	SHADER_PARAM( EXPONENTMODE, SHADER_PARAM_TYPE_INTEGER, "0", "Set to 1 to an exponent texture for the given style rather than a base diffuse" )

	SHADER_PARAM( PATTERNTEXTURETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$PAINTTEXTURE texcoord transform" )
	SHADER_PARAM( WEARTEXTURETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$WEARTEXTURE texcoord transform" )
	SHADER_PARAM( GRUNGETEXTURETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$GRUNGETEXTURE texcoord transform" )

	SHADER_PARAM( CHEAPMODE, SHADER_PARAM_TYPE_BOOL, "", "Cheap mode for low-spec hardware. Disables certain nice-but-not-fundamentally-vital features such as super-sampling the HDR precision" )

	SHADER_PARAM( PHONGALBEDOFACTOR, SHADER_PARAM_TYPE_FLOAT, "1.0", "When using the anodizing mode, some specular values already affected by phongalbedo in the source need to be reduced to compensate for increase in phongalbedoboost")

	SHADER_PARAM( PREVIEW, SHADER_PARAM_TYPE_BOOL, "0", "Enable to preview the shader, realtime, on a model" )
	SHADER_PARAM( PHONGFRESNELRANGES, SHADER_PARAM_TYPE_VEC3, "[ .25 .5 1 ]", "Phong fresnel ranges for preview render" )
	SHADER_PARAM( PHONGALBEDOTINT, SHADER_PARAM_TYPE_BOOL, "0", "Enable phong albedo tint for preview render" )
	SHADER_PARAM( PHONGBOOST, SHADER_PARAM_TYPE_FLOAT, "1.0", "Phong boost for preview render" )
	SHADER_PARAM( PAINTPHONGALBEDOBOOST, SHADER_PARAM_TYPE_FLOAT, "1.0", "Phong albedo boost for preview render" )

	SHADER_PARAM( PREVIEWWEAPONOBJSCALE, SHADER_PARAM_TYPE_FLOAT, "1.0", "Weapon-specific scale factor for patterns applied via object space projections" )
	SHADER_PARAM( PREVIEWWEAPONUVSCALE, SHADER_PARAM_TYPE_FLOAT, "1.0", "Weapon-specific scale factor for patterns applied via UVs" )
	SHADER_PARAM( PREVIEWIGNOREWEAPONSCALE, SHADER_PARAM_TYPE_BOOL, "0", "Paint-specific toggle for ignoring weapon-specific scales" )

END_SHADER_PARAMS

	void SetupVarsCustomWeapon( CustomWeaponVars_t &info )
{
	info.m_nBaseTexture = BASETEXTURE;
	info.m_nBaseTextureTransform = BASETEXTURETRANSFORM;

	info.m_nSurfaceTexture = SURFACETEXTURE;
	info.m_nAOTexture = AOTEXTURE;
	info.m_nMasksTexture = MASKSTEXTURE;
	info.m_nPosTexture = POSTEXTURE;
	info.m_nGrungeTexture = GRUNGETEXTURE;
	info.m_nWearTexture = WEARTEXTURE;
	info.m_nPaintTexture = PAINTTEXTURE;
	info.m_nExpTexture = EXPTEXTURE;

	info.m_nCamoColor0 = CAMOCOLOR0;
	info.m_nCamoColor1 = CAMOCOLOR1;
	info.m_nCamoColor2 = CAMOCOLOR2;
	info.m_nCamoColor3 = CAMOCOLOR3;

	info.m_nBaseDiffuseOverride = BASEDIFFUSEOVERRIDE;
	info.m_nPhongExponent = PHONGEXPONENT;
	info.m_nPhongIntensity = PHONGINTENSITY;
	info.m_nWearProgress = WEARPROGRESS;
	info.m_nPaintStyle = PAINTSTYLE;
	info.m_nExponentMode = EXPONENTMODE;

	info.m_nPatternTextureTransform = PATTERNTEXTURETRANSFORM;
	info.m_nWearTextureTransform = WEARTEXTURETRANSFORM;
	info.m_nGrungeTextureTransform = GRUNGETEXTURETRANSFORM;

	info.m_nCheap = CHEAPMODE;

	info.m_nPhongAlbedoFactor = PHONGALBEDOFACTOR;

	info.m_nPreview = PREVIEW;
	info.m_nPreviewPhongFresnelRanges = PHONGFRESNELRANGES;
	info.m_nPreviewPhongAlbedoTint = PHONGALBEDOTINT;
	info.m_nPreviewPhongBoost = PHONGBOOST;
	info.m_nPhongAlbedoBoost = PHONGALBEDOBOOST;
	info.m_nPreviewPaintPhongAlbedoBoost = PAINTPHONGALBEDOBOOST;

	info.m_nPreviewWeaponObjScale = PREVIEWWEAPONOBJSCALE;
	info.m_nPreviewWeaponUVScale = PREVIEWWEAPONUVSCALE;
	info.m_nPreviewIgnoreWeaponScale = PREVIEWIGNOREWEAPONSCALE;
}

SHADER_INIT_PARAMS()
{
	CustomWeaponVars_t info;
	SetupVarsCustomWeapon( info );
	InitParamsCustomWeapon( this, params, pMaterialName, info );
}

SHADER_FALLBACK
{
	return 0;
}

SHADER_INIT
{
	CustomWeaponVars_t info;
	SetupVarsCustomWeapon( info );
	InitCustomWeapon( this, params, info );
}

SHADER_DRAW
{
	CustomWeaponVars_t info;
	SetupVarsCustomWeapon( info );
	DrawCustomWeapon( this, params, pShaderAPI, pShaderShadow, info, vertexCompression, pContextDataPtr );
}
END_SHADER