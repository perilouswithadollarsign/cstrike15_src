//========= Copyright (c) Valve Corporation, All rights reserved. ============//

#include "BaseVSShader.h"
#include "weapondecal_dx9_helper.h"
#include "cpp_shader_constant_register_map.h"

// Auto generated inc files
#include "weapondecal_vs20.inc"
#include "weapondecal_vs30.inc"
#include "weapondecal_ps20b.inc"
#include "weapondecal_ps30.inc"
#include "common_hlsl_cpp_consts.h"

#include "shaderlib/commandbuilder.h"

void InitParamsWeaponDecal( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, WeaponDecalVars_t &info )
{
	//static ConVarRef gpu_level( "gpu_level" );
	//int nGPULevel = gpu_level.GetInt();

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nWearProgress, kDefaultWearProgress );

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nWearRemapMin, kDefaultWearRemapMin );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nWearRemapMid, kDefaultWearRemapMid );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nWearRemapMax, kDefaultWearRemapMax );

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nWearWidthMin, kDefaultWearWidthMin );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nWearWidthMax, kDefaultWearWidthMax );
	
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nUnWearStrength, kDefaultUnWearStrength );

	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nPhong, kDefaultPhong );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nPhongExponent, kDefaultPhongExponent );
	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nPhongFresnelRanges, kDefaultPhongFresnelRanges, 3 );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nPhongAlbedoTint, kDefaultPhongAlbedoTint );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nPhongBoost, kDefaultPhongBoost );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nPhongAlbedoBoost, kDefaultPhongAlbedoBoost );
	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nEnvmapTint, kDefaultEnvmapTint, 3 );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nDecalStyle, kDefaultDecalStyle );
	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nColorTint, kDefaultColorTint, 3 );
	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nColorTint2, kDefaultColorTint2, 3 );
	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nColorTint3, kDefaultColorTint3, 3 );
	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nColorTint4, kDefaultColorTint4, 3 );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nAlpha, kDefaultAlpha );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nPatternRotation, kDefaultPatternRotation );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nPatternScale, kDefaultPatternScale );

	SET_PARAM_STRING_IF_NOT_DEFINED( info.m_nBaseTexture, kDefaultBaseTexture );
	SET_PARAM_STRING_IF_NOT_DEFINED( info.m_nAOTexture, kDefaultAOTexture );
	SET_PARAM_STRING_IF_NOT_DEFINED( info.m_nGrungeTexture, kDefaultGrungeTexture );
	SET_PARAM_STRING_IF_NOT_DEFINED( info.m_nWearTexture, kDefaultWearTexture );

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nHighlight, kDefaultHighlight );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nHighlightCycle, kDefaultHighlightCycle );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nPeel, kDefaultPeel );

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nWearBias, kDefaultWearBias );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nAlphaMask, kDefaultAlphaMask );
	//SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nFastWearThreshold, kDefaultFastWearThreshold );

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nGrungeScale, kDefaultGrungeScale );

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nDesatBaseTint, kDefaultDesatBaseTint );

	//SET_FLAGS( MATERIAL_VAR_ALPHATEST );
	SET_FLAGS( MATERIAL_VAR_ALPHA_MODIFIED_BY_PROXY );
	SET_FLAGS( MATERIAL_VAR_TRANSLUCENT );
	//SET_FLAGS( MATERIAL_VAR_DECAL );
	SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
	SET_FLAGS2( MATERIAL_VAR2_LIGHTING_VERTEX_LIT );
	if ( params[info.m_nDecalStyle]->GetIntValue() == 4 )
	{
		SET_FLAGS2( MATERIAL_VAR2_NEEDS_TANGENT_SPACES );
	}
}

void InitWeaponDecal( CBaseVSShader *pShader, IMaterialVar** params, WeaponDecalVars_t &info )
{
	// Load textures
	if ( ( info.m_nBaseTexture != -1 ) && params[info.m_nBaseTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nBaseTexture );
	}
	if ( ( info.m_nAOTexture != -1 ) && params[info.m_nAOTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nAOTexture );
	}
	if ( ( info.m_nExpTexture != -1 ) && params[info.m_nExpTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nExpTexture );
	}
	if ( ( info.m_nNormalMap != -1 ) && params[info.m_nNormalMap]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nNormalMap );
	}
	if ( ( info.m_nEnvmapTexture != -1 ) && params[info.m_nEnvmapTexture]->IsDefined() )
	{
		pShader->LoadCubeMap( info.m_nEnvmapTexture, TEXTUREFLAGS_SRGB );
	}
	if ( ( info.m_nGrungeTexture != -1 ) && params[info.m_nGrungeTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nGrungeTexture );
	}
	if ( ( info.m_nWearTexture != -1 ) && params[info.m_nWearTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nWearTexture );
	}
	if ( ( info.m_nHologramSpectrum != -1 ) && params[info.m_nHologramSpectrum]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nHologramSpectrum );
	}
	if ( ( info.m_nHologramMask != -1 ) && params[info.m_nHologramMask]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nHologramMask );
	}
	if ( ( info.m_nAnisoDirTexture != -1 ) && params[info.m_nAnisoDirTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nAnisoDirTexture );
	}
}

class CWeaponDecal_DX9_Context : public CBasePerMaterialContextData
{
public:
	CCommandBufferBuilder< CFixedCommandStorageBuffer< 800 > > m_SemiStaticCmdsOut;
};

void DrawWeaponDecal(  CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
	IShaderShadow* pShaderShadow, WeaponDecalVars_t &info, VertexCompressionType_t vertexCompression,
	CBasePerMaterialContextData **pContextDataPtr )
{
	CWeaponDecal_DX9_Context *pContextData = reinterpret_cast< CWeaponDecal_DX9_Context *> ( *pContextDataPtr );

	bool bPhong				= ( info.m_nPhong			!= -1 ) && ( params[info.m_nPhong]->GetIntValue() == 1 );
	bool bMirrorHorizontal	= ( info.m_nMirrorHorizontal!= -1 ) && ( params[info.m_nMirrorHorizontal]->GetIntValue() == 1 );
	bool bBaseTexture		= ( info.m_nBaseTexture		!= -1 ) && params[info.m_nBaseTexture]->IsTexture();
	bool bAOTexture			= ( info.m_nAOTexture		!= -1 )	&& params[info.m_nAOTexture	]->IsTexture();
	bool bExpTexture		= ( info.m_nExpTexture		!= -1 )	&& params[info.m_nExpTexture]->IsTexture();
	bool bNormalMap			= ( info.m_nNormalMap		!= -1 )	&& params[info.m_nNormalMap]->IsTexture();
	bool bEnvmapTexture		= ( info.m_nEnvmapTexture	!= -1 )	&& params[info.m_nEnvmapTexture]->IsTexture();
	bool bHologramSpectrum	= ( info.m_nHologramSpectrum!= -1 )	&& params[info.m_nHologramSpectrum]->IsTexture();
	bool bHologramMask		= ( info.m_nHologramMask	!= -1 )	&& params[info.m_nHologramMask]->IsTexture();
	bool bGrungeTexture		= ( info.m_nGrungeTexture	!= -1 ) && params[info.m_nGrungeTexture]->IsTexture();
	bool bWearTexture		= ( info.m_nWearTexture		!= -1 ) && params[info.m_nWearTexture]->IsTexture();
	bool bAnisoDirTexture	= ( info.m_nAnisoDirTexture	!= -1 ) && params[info.m_nAnisoDirTexture]->IsTexture();
	bool bThirdPerson		= ( info.m_nThirdPerson		!= -1 ) && ( params[info.m_nThirdPerson]->GetIntValue() == 1 );
	bool bHighlight			= ( info.m_nHighlight		!= -1 ) && ( params[info.m_nHighlight]->GetFloatValue() > 0 );
	bool bPeel				= ( info.m_nPeel			!= -1 ) && ( params[info.m_nPeel]->GetFloatValue() > 0 ) && bHighlight == false;
	int nDecalStyle = params[info.m_nDecalStyle]->GetIntValue();
	bool bAlphaMask			= ( info.m_nAlphaMask	!= -1 ) && ( params[info.m_nAlphaMask]->GetIntValue() == 1 );
	bool bSFM = ( ToolsEnabled() && IsPlatformWindowsPC() && g_pHardwareConfig->SupportsPixelShaders_3_0() ) ? true : false;
	bool bDesatBaseTint		= ( info.m_nDesatBaseTint	!= -1 ) && ( params[info.m_nDesatBaseTint]->GetFloatValue() > 0 );

	//bool bFlashlight = pShader->UsingFlashlight( params );

	if ( ( pShader->IsSnapshotting() && pShaderShadow ) || (! pContextData ) || ( pContextData->m_bMaterialVarsChanged ) )
	{

		if ( pShader->IsSnapshotting() )
		{
			// Set stream format (note that this shader supports compression)
			int userDataSize = 0;
			unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED;

			int nTexCoordCount = 1;
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );

			// Vertex Shader
			if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_STATIC_VERTEX_SHADER( weapondecal_vs30 );
				SET_STATIC_VERTEX_SHADER_COMBO( MIRROR, bMirrorHorizontal );
				SET_STATIC_VERTEX_SHADER_COMBO( NORMALMAP, ( nDecalStyle == 4 ) || ( nDecalStyle == 5 ) );
				SET_STATIC_VERTEX_SHADER( weapondecal_vs30 );
			}
			else
			{
				DECLARE_STATIC_VERTEX_SHADER( weapondecal_vs20 );
				SET_STATIC_VERTEX_SHADER_COMBO( MIRROR, bMirrorHorizontal );
				SET_STATIC_VERTEX_SHADER_COMBO( NORMALMAP, ( nDecalStyle == 4 ) || ( nDecalStyle == 5 ) );
				SET_STATIC_VERTEX_SHADER( weapondecal_vs20 );
			}

			// Pixel Shader
			if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_STATIC_PIXEL_SHADER( weapondecal_ps30 );
				SET_STATIC_PIXEL_SHADER_COMBO( ALPHAMASK, bAlphaMask );
				SET_STATIC_PIXEL_SHADER_COMBO( PHONG, bPhong );
				SET_STATIC_PIXEL_SHADER_COMBO( PHONGEXPONENTTEXTURE, bExpTexture );
				SET_STATIC_PIXEL_SHADER_COMBO( CUBEMAP, bEnvmapTexture );
				SET_STATIC_PIXEL_SHADER_COMBO( DECALSTYLE, nDecalStyle );
				SET_STATIC_PIXEL_SHADER_COMBO( THIRDPERSON, bThirdPerson );		
				SET_STATIC_PIXEL_SHADER_COMBO( CASCADED_SHADOW_MAPPING, g_pHardwareConfig->SupportsCascadedShadowMapping() && !bSFM );
				SET_STATIC_PIXEL_SHADER_COMBO( DESATBASETINT, bDesatBaseTint );
				SET_STATIC_PIXEL_SHADER( weapondecal_ps30 );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( weapondecal_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( PHONG, bPhong );
				SET_STATIC_PIXEL_SHADER_COMBO( PHONGEXPONENTTEXTURE, bExpTexture );
				SET_STATIC_PIXEL_SHADER_COMBO( CUBEMAP, bEnvmapTexture );
				SET_STATIC_PIXEL_SHADER_COMBO( DECALSTYLE, nDecalStyle );
				SET_STATIC_PIXEL_SHADER_COMBO( THIRDPERSON, bThirdPerson );
				SET_STATIC_PIXEL_SHADER_COMBO( DESATBASETINT, bDesatBaseTint );
				SET_STATIC_PIXEL_SHADER( weapondecal_ps20b );
			}

			pShader->EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );

			// Textures
			if ( bBaseTexture )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );		// [sRGB] Base
			}
			if( bAOTexture && !bThirdPerson )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );		// [sRGB] Ambient Occlusion
			}
			if( bExpTexture )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );		// Exponent
			}
			//if( bEnvmapTexture )
			//{
			//	pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );		// Envmap
			//}
			if( bWearTexture && !bThirdPerson )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );		// Scratches
			}
			if( bGrungeTexture && !bThirdPerson )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER5, true );		// [sRGB] Grunge
			}
			if( bHologramSpectrum || bNormalMap )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER6, true );		// Hologram spectrum or normal map
			}
			if( bHologramMask )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER7, true );		// Hologram spectrum
			}
			if ( bAnisoDirTexture )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER8, true );		// Anisotropy Direction map
			}

			//if ( bFlashlight )
			//{
			//	pShader->SetAdditiveBlendingShadowState( info.m_nBaseTexture, true );
			//}

			pShaderShadow->EnableAlphaWrites( false );
			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableSRGBWrite( true );
			pShaderShadow->EnableBlending( true );

			pShader->PI_BeginCommandBuffer();
			pShader->PI_SetPixelShaderAmbientLightCube( PSREG_AMBIENT_CUBE );
			pShader->PI_SetPixelShaderLocalLighting( PSREG_LIGHT_INFO_ARRAY );
			//pShader->PI_SetModulationPixelShaderDynamicState_LinearColorSpace( 1 );
			pShader->PI_EndCommandBuffer();
			
		}
		if ( pShaderAPI && ( (! pContextData ) || ( pContextData->m_bMaterialVarsChanged ) ) )
		{
			if ( !pContextData )								// make sure allocated
			{
				pContextData = new CWeaponDecal_DX9_Context;
				*pContextDataPtr = pContextData;
			}
			pContextData->m_bMaterialVarsChanged = false;
			pContextData->m_SemiStaticCmdsOut.Reset();

			///////////////////////////
			// Semi-static block
			///////////////////////////

			// VS Constants
			
			//pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureScaledTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, info.m_nBaseTextureTransform, info.m_nPatternScale );

			pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureScaledTransformRotate( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, 
				info.m_nBaseTextureTransform, 
				info.m_nPatternScale,
				info.m_nPatternRotation );

			//derive basetexture aspect ratio
			float flAspectRatioHeight = 1.0f;
			if ( bBaseTexture )
			{
				ITexture *pBaseTexture = params[info.m_nBaseTexture]->GetTextureValue();
				if ( pBaseTexture )
				{
					float flWidth = pBaseTexture->GetActualWidth();
					float flHeight = pBaseTexture->GetActualHeight();

					if ( flHeight != 0 )
						flAspectRatioHeight = flWidth / flHeight;
				}
			}

			float fvVsConst2[4] = { flAspectRatioHeight, 0.0f, 0.0f, 0.0f };
			pContextData->m_SemiStaticCmdsOut.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_2, fvVsConst2, 1 );

			if ( bBaseTexture )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nBaseTexture, -1 );
			}
			if( bAOTexture && !bThirdPerson ) 
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER1, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nAOTexture, -1 );
			}
			if( bExpTexture )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER2, TEXTURE_BINDFLAGS_NONE, info.m_nExpTexture, -1 );
			}
			if( bNormalMap && ( nDecalStyle == 4 || nDecalStyle == 5 ) )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER6, TEXTURE_BINDFLAGS_NONE, info.m_nNormalMap, -1 );
			}
			if( bEnvmapTexture )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER3, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nEnvmapTexture, -1 );
			}
			if( bWearTexture && !bThirdPerson )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER4, TEXTURE_BINDFLAGS_NONE, info.m_nWearTexture, -1 );
			}
			if( bGrungeTexture && !bThirdPerson )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER9, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nGrungeTexture, -1 );
			}
			if ( bHologramSpectrum && ( nDecalStyle != 4 ) && ( nDecalStyle != 5 ) )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER6, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nHologramSpectrum, -1 );
			}
			if( bHologramMask )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER7, TEXTURE_BINDFLAGS_NONE, info.m_nHologramMask, -1 );
			}
			if ( bAnisoDirTexture )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER8, TEXTURE_BINDFLAGS_NONE, info.m_nAnisoDirTexture, -1 );
			}
			//pContextData->m_SemiStaticCmdsOut.SetPixelShaderTextureTransform( 10, info.m_nBaseTextureTransform );
			pContextData->m_SemiStaticCmdsOut.BindStandardTexture( SHADER_SAMPLER5, TEXTURE_BINDFLAGS_NONE, TEXTURE_NORMALIZATION_CUBEMAP_SIGNED );

			// PS Constants
			
			float flGrungeScale = params[info.m_nGrungeScale]->GetFloatValue();

			//remap wear midpoint along a simple bezier parabola
			//y = 2(1-x) x MID + x^2
			float flX = params[info.m_nWearProgress]->GetFloatValue();
			float flP = params[info.m_nWearRemapMid]->GetFloatValue();
			float flRemappedWear = 2.0f * ( 1.0f - flX ) * flX * flP + ( flX * flX );
			
			//remap wear to custom min/max bounds
			flRemappedWear *= ( params[info.m_nWearRemapMax]->GetFloatValue() - params[info.m_nWearRemapMin]->GetFloatValue() );
			flRemappedWear += params[info.m_nWearRemapMin]->GetFloatValue();

			//we already shipped wear progress levels, this is an additional param that individual stickers 
			//can drive to bias their wear AGAIN as they move away from 0
			flRemappedWear += flX * flX * params[info.m_nWearBias]->GetFloatValue();

			//lerp wear width along wear progress
			float flLerpedWearWidth = Lerp( params[info.m_nWearProgress]->GetFloatValue(), params[info.m_nWearWidthMin]->GetFloatValue(), params[info.m_nWearWidthMax]->GetFloatValue() );

			float fvConst0[4] = {	params[info.m_nWearProgress]->GetFloatValue(), 
									flLerpedWearWidth,
									flRemappedWear,
									params[info.m_nUnWearStrength]->GetFloatValue() };
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 0, fvConst0, 1 );

			float fvConst1[4] = {	params[info.m_nPhongExponent]->GetFloatValue(),
									params[info.m_nPhongBoost]->GetFloatValue(), 
									params[info.m_nPhongAlbedoBoost]->GetFloatValue(),
									//1.0f - params[info.m_nFastWearThreshold]->GetFloatValue() };
									flGrungeScale };
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 1, fvConst1, 1 );

			float fvConst2[4] = {	params[info.m_nPhongFresnelRanges]->GetVecValue()[0], 
									params[info.m_nPhongFresnelRanges]->GetVecValue()[1], 
									params[info.m_nPhongFresnelRanges]->GetVecValue()[2], 
									params[info.m_nPhongAlbedoTint]->GetFloatValue() };
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 2, fvConst2, 1 );

			float fvConst3[4] = {	params[info.m_nEnvmapTint]->GetVecValue()[0], 
									params[info.m_nEnvmapTint]->GetVecValue()[1], 
									params[info.m_nEnvmapTint]->GetVecValue()[2], 
									SrgbGammaToLinear( params[info.m_nColorTint3]->GetVecValue()[0] / 255.0 ) };
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 3, fvConst3, 1 );

			float fvConst10[4] = {	SrgbGammaToLinear( params[info.m_nColorTint]->GetVecValue()[0] / 255.0 ), 
									SrgbGammaToLinear( params[info.m_nColorTint]->GetVecValue()[1] / 255.0 ), 
									SrgbGammaToLinear( params[info.m_nColorTint]->GetVecValue()[2] / 255.0 ), 
									SrgbGammaToLinear( params[info.m_nColorTint3]->GetVecValue()[1] / 255.0 ) };
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 10, fvConst10, 1 );

			float fvConst11[4] = {	SrgbGammaToLinear( params[info.m_nColorTint2]->GetVecValue()[0] / 255.0 ), 
									SrgbGammaToLinear( params[info.m_nColorTint2]->GetVecValue()[1] / 255.0 ), 
									SrgbGammaToLinear( params[info.m_nColorTint2]->GetVecValue()[2] / 255.0 ), 
									SrgbGammaToLinear( params[info.m_nColorTint3]->GetVecValue()[2] / 255.0 ) };
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 11, fvConst11, 1 );
			
			float fvConst13[4] = {	SrgbGammaToLinear( params[info.m_nColorTint4]->GetVecValue()[0] / 255.0 ), 
									SrgbGammaToLinear( params[info.m_nColorTint4]->GetVecValue()[1] / 255.0 ), 
									SrgbGammaToLinear( params[info.m_nColorTint4]->GetVecValue()[2] / 255.0 ), 
									params[info.m_nDesatBaseTint]->GetFloatValue() };
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 13, fvConst13, 1 );

			if ( bHighlight )
			{
				float fvConst14[4] = {  params[info.m_nHighlight]->GetFloatValue(),
										params[info.m_nHighlightCycle]->GetFloatValue(),
										0.0f, 0.0f };
				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant(14, fvConst14, 1);
			}
			else if ( bPeel )
			{
				float fvConst14[4] = { params[info.m_nPeel]->GetFloatValue(), 
									   params[info.m_nHighlightCycle]->GetFloatValue(), 
									   0.0f, 0.0f };
				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant(14, fvConst14, 1);
			}

			pContextData->m_SemiStaticCmdsOut.End();
		}
	}
	if ( pShaderAPI ) //DYNAMIC_STATE
	{
		if ( IsPC() && pShaderAPI->InFlashlightMode() )
		{
			// Don't draw anything for the flashlight pass
			pShader->Draw( false );
			return;
		}

#ifdef _PS3
		CCommandBufferBuilder< CDynamicCommandStorageBuffer > DynamicCmdsOut;
		ShaderApiFast( pShaderAPI )->ExecuteCommandBuffer( pContextData->m_SemiStaticCmdsOut.Base() );
#else
		CCommandBufferBuilder< CFixedCommandStorageBuffer< 600 > > DynamicCmdsOut;
		DynamicCmdsOut.Call( pContextData->m_SemiStaticCmdsOut.Base() );
#endif

		///////////////////////////
		// dynamic block
		///////////////////////////
		int numBones = ShaderApiFast( pShaderAPI )->GetCurrentNumBones();

		LightState_t lightState = { 0, false, false };
		ShaderApiFast( pShaderAPI )->GetDX9LightState( &lightState );

		// VERTEX SHADER SETUP
		if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
		{
			DECLARE_DYNAMIC_VERTEX_SHADER( weapondecal_vs30 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, ( numBones > 0 ) );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( NUM_LIGHTS, IsPlatformOSX() ? Min( 2, lightState.m_nNumLights ) : lightState.m_nNumLights );
			SET_DYNAMIC_VERTEX_SHADER( weapondecal_vs30 );
		}
		else
		{
			DECLARE_DYNAMIC_VERTEX_SHADER( weapondecal_vs20 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, ( numBones > 0 ) );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( NUM_LIGHTS, IsPlatformOSX() ? Min( 2, lightState.m_nNumLights ) : lightState.m_nNumLights );
			SET_DYNAMIC_VERTEX_SHADER( weapondecal_vs20 );
		}

		// VS constants
		
		
        bool bCSMEnabled;

        bCSMEnabled = pShaderAPI->IsCascadedShadowMapping() && !bSFM;

		if ( bCSMEnabled )
		{
			ITexture *pDepthTextureAtlas = NULL;
			const CascadedShadowMappingState_t &cascadeState = pShaderAPI->GetCascadedShadowMappingState( &pDepthTextureAtlas );

			if ( pDepthTextureAtlas )
			{
				DynamicCmdsOut.BindTexture( pShader, SHADER_SAMPLER15, TEXTURE_BINDFLAGS_SHADOWDEPTH, pDepthTextureAtlas, 0 );
				DynamicCmdsOut.SetPixelShaderConstant( 64, &cascadeState.m_vLightColor.x, CASCADED_SHADOW_MAPPING_CONSTANT_BUFFER_SIZE );
			}
		}

		// PIXEL SHADER SETUP
		if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( weapondecal_ps30 );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( NUM_LIGHTS, IsPlatformOSX() ? Min( 2, lightState.m_nNumLights ) : lightState.m_nNumLights );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( DYN_CSM_ENABLED, ( IsPlatformOSX() ? 0 : ( bCSMEnabled ? 1 : 0 ) ) );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( HIGHLIGHT, bHighlight ? 2 : 0 );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( PEEL, bPeel ? 1 : 0 );
			SET_DYNAMIC_PIXEL_SHADER( weapondecal_ps30 );
		}
		else
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( weapondecal_ps20b );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( NUM_LIGHTS, IsPlatformOSX() ? Min( 2, lightState.m_nNumLights ) : lightState.m_nNumLights );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( DYN_CSM_ENABLED, ( IsPlatformOSX() ? 0 : ( bCSMEnabled ? 1 : 0 ) ) );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( HIGHLIGHT, bHighlight ? 1 : 0 );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( PEEL, bPeel ? 1 : 0 );
			SET_DYNAMIC_PIXEL_SHADER( weapondecal_ps20b );
		}

		float vEyePos[4] = { 0, 0, 0, 0 };
		ShaderApiFast( pShaderAPI )->GetWorldSpaceCameraPosition( vEyePos );
		DynamicCmdsOut.SetPixelShaderConstant( 12, vEyePos, 1 );

		if ( bEnvmapTexture )
		{
			bool bHdr = ( g_pHardwareConfig->GetHDRType() != HDR_TYPE_NONE );
			DynamicCmdsOut.BindEnvCubemapTexture( pShader, SHADER_SAMPLER3, bHdr ? TEXTURE_BINDFLAGS_NONE : TEXTURE_BINDFLAGS_SRGBREAD, info.m_nEnvmapTexture, 0 /*info.m_nEnvmapFrame*/ );
		}		

		DynamicCmdsOut.End();

		// end dynamic block

		pShaderAPI->ExecuteCommandBuffer( DynamicCmdsOut.Base() );

	}
	pShader->Draw();
}
