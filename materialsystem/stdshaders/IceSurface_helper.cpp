//========= Copyright © 1996-2007, Valve Corporation, All rights reserved. ============//

#include "BaseVSShader.h"
#include "IceSurface_helper.h"
#include "cpp_shader_constant_register_map.h"
/*
#include "mathlib/VMatrix.h"
#include "convar.h"
*/

// Auto generated inc files
#include "icesurface_vs30.inc"
#include "icesurface_ps30.inc"


void InitParamsIceSurface( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, IceSurfaceVars_t &info )
{
	// Set material parameter default values
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nBackSurface, kDefaultBackSurface );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nUVScale, kDefaultUVScale );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nBumpStrength, kDefaultBumpStrength );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nFresnelBumpStrength, kDefaultFresnelBumpStrength );

	SET_PARAM_INT_IF_NOT_DEFINED(   info.m_nInteriorEnable, kDefaultInteriorEnable );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nInteriorFogStrength, kDefaultInteriorFogStrength );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nInteriorFogLimit, kDefaultInteriorFogLimit );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nInteriorFogNormalBoost, kDefaultInteriorFogNormalBoost );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nInteriorBackgroundBoost, kDefaultInteriorBackgroundBoost );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nInteriorAmbientScale, kDefaultInteriorAmbientScale );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nInteriorBackLightScale, kDefaultInteriorBackLightScale );	
	SET_PARAM_VEC_IF_NOT_DEFINED(   info.m_nInteriorColor, kDefaultInteriorColor, 3 );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nInteriorRefractStrength, kDefaultInteriorRefractStrength );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nInteriorRefractBlur, kDefaultInteriorRefractBlur );

	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nFresnelParams, kDefaultFresnelParams, 3 );
	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nBaseColorTint, kDefaultBaseColorTint, 3 );
	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nEnvMapTint, kDefaultEnvMapTint, 3 );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nDiffuseScale, kDefaultDiffuseScale );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nSpecExp, kDefaultSpecExp );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nSpecScale, kDefaultSpecScale );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nSpecExp2, kDefaultSpecExp );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nSpecScale2, kDefaultSpecScale );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nRimLightExp, kDefaultRimLightExp );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nRimLightScale, kDefaultRimLightScale );
	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nUVProjOffset, kDefaultUVProjOffset, 3 );
	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nBBMin, kDefaultBB, 3 );
	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nBBMax, kDefaultBB, 3 );

	// FLASHLIGHTFIXME: Do ShaderAPI::BindFlashlightTexture
	Assert( info.m_nFlashlightTexture >= 0 );
	if ( g_pHardwareConfig->SupportsBorderColor() )
	{
		params[info.m_nFlashlightTexture]->SetStringValue( "effects/flashlight_border" );
	}
	else
	{
		params[info.m_nFlashlightTexture]->SetStringValue( "effects/flashlight001" );
	}
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nFlashlightTextureFrame, 0 );

	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nBumpFrame, kDefaultBumpFrame )

	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nContactShadows, kDefaultContactShadows );

	// Set material flags
	SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
	SET_FLAGS2( MATERIAL_VAR2_LIGHTING_VERTEX_LIT );

	if ( params[info.m_nInteriorEnable]->IsDefined() && params[info.m_nInteriorEnable]->GetIntValue() != 0 )
	{
		SET_FLAGS2( MATERIAL_VAR2_NEEDS_FULL_FRAME_BUFFER_TEXTURE );
	}
}

void InitIceSurface( CBaseVSShader *pShader, IMaterialVar** params, IceSurfaceVars_t &info )
{
	// Load textures
	if ( (info.m_nBaseTexture != -1) && params[info.m_nBaseTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nBaseTexture );
	}

	if ( (info.m_nNormalMap != -1) && params[info.m_nNormalMap]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nNormalMap );
	}

	if ( (info.m_nSpecMap != -1) && params[info.m_nSpecMap]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nSpecMap );
	}

	if ( (info.m_nLightWarpTexture != -1) && params[info.m_nLightWarpTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nLightWarpTexture );
	}

	if ( (info.m_nFresnelWarpTexture != -1) && params[info.m_nFresnelWarpTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nFresnelWarpTexture );
	}

	if ( (info.m_nOpacityTexture != -1) && params[info.m_nOpacityTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nOpacityTexture );
	}

	if ( (info.m_nEnvMap != -1) && params[info.m_nEnvMap]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nEnvMap );
	}

	if ( (info.m_nFlashlightTexture != -1) && params[info.m_nFlashlightTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nFlashlightTexture, TEXTUREFLAGS_SRGB );
	}
}

void DrawIceSurface(  CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
				IShaderShadow* pShaderShadow, IceSurfaceVars_t &info, VertexCompressionType_t vertexCompression )
{
	bool bHasFlashlight = pShader->UsingFlashlight( params );
	bool bBackSurface = (info.m_nBackSurface != -1) && ( params[info.m_nBackSurface]->GetIntValue() > 0 );
	bool bLightWarp = (info.m_nLightWarpTexture != -1) && params[info.m_nLightWarpTexture]->IsDefined();
	bool bFresnelWarp = (info.m_nFresnelWarpTexture != -1) && params[info.m_nFresnelWarpTexture]->IsDefined();
	bool bOpacityTexture = (info.m_nOpacityTexture != -1) && params[info.m_nOpacityTexture]->IsDefined();
	bool bInteriorLayer = (info.m_nInteriorEnable != -1) && ( params[info.m_nInteriorEnable]->GetIntValue() > 0 );
	bool bContactShadows = (info.m_nContactShadows != -1) && ( params[info.m_nContactShadows]->GetIntValue() > 0 );
	bool bSpecMap = (info.m_nSpecMap != -1) && params[info.m_nSpecMap]->IsDefined();
  	bool bEnvMap = (info.m_nEnvMap != -1) && params[info.m_nEnvMap]->IsDefined();

	SHADOW_STATE
	{
		// Set stream format (note that this shader supports compression)
		unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL;
		int nTexCoordCount = 1;
		int userDataSize = 0;
		int texCoordDims[4] = { 4, 4, 4, 4 };
		pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, texCoordDims, userDataSize );

		ShadowFilterMode_t nShadowFilterMode = SHADOWFILTERMODE_DEFAULT;
		if ( bHasFlashlight )
		{
			nShadowFilterMode = g_pHardwareConfig->GetShadowFilterMode( false /* bForceLowQuality */, true /* bPS30 */ );	// Based upon vendor and device dependent formats
		}

		// Vertex Shader
		DECLARE_STATIC_VERTEX_SHADER( icesurface_vs30 );
		SET_STATIC_VERTEX_SHADER( icesurface_vs30 );
	
		// Pixel Shader
		if( /* g_pHardwareConfig->SupportsPixelShaders_3_0() */ true )
		{
			DECLARE_STATIC_PIXEL_SHADER( icesurface_ps30 );
			SET_STATIC_PIXEL_SHADER_COMBO( BACK_SURFACE, bBackSurface );
			SET_STATIC_PIXEL_SHADER_COMBO( LIGHT_WARP, bLightWarp );
			SET_STATIC_PIXEL_SHADER_COMBO( FRESNEL_WARP, bFresnelWarp );
			SET_STATIC_PIXEL_SHADER_COMBO( OPACITY_TEXTURE, bOpacityTexture );
			SET_STATIC_PIXEL_SHADER_COMBO( INTERIOR_LAYER, bInteriorLayer );
			SET_STATIC_PIXEL_SHADER_COMBO( HIGH_PRECISION_DEPTH, (g_pHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT) ? true : false );
			SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode );
			SET_STATIC_PIXEL_SHADER_COMBO( CONTACT_SHADOW, bContactShadows );	// only do contact shadows on outer shell (which has interior layer enabled)
			SET_STATIC_PIXEL_SHADER( icesurface_ps30 );
		}
		else
		{
			Assert( !"No ps_3_0" );
		}

		// Textures
		pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );		//[sRGB] Base
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, true );
		pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );		//		 Bump
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, false );
		pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );		//[sRGB] Backbuffer
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER2, true );
		pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );		//       Spec mask
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER3, false );
		pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );		//[sRGB] Light warp
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER4, true );
		pShaderShadow->EnableTexture( SHADER_SAMPLER5, true );		//		 Fresnel warp	// TODO: Could be in alpha of lightwarp
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER5, false );
		pShaderShadow->EnableTexture( SHADER_SAMPLER6, true );		//		 Opacity
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER6, false );
		pShaderShadow->EnableTexture( SHADER_SAMPLER7, true );		//[sRGB] Envmap
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER7, true );

		if( bHasFlashlight )
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER8, true );	//		 Shadow depth map
			//pShaderShadow->SetShadowDepthFiltering( SHADER_SAMPLER8 );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER8, false );
			pShaderShadow->EnableTexture( SHADER_SAMPLER9, true );	//		 Noise map
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER9, false );
			pShaderShadow->EnableTexture( SHADER_SAMPLER10, true );	//[sRGB] Flashlight cookie
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER10, true );
		}

		pShaderShadow->EnableSRGBWrite( true );
		pShaderShadow->EnableAlphaWrites( true );

		// Per-instance state
		pShader->PI_BeginCommandBuffer();
   		pShader->PI_SetVertexShaderAmbientLightCube();
		pShader->PI_SetPixelShaderAmbientLightCube( PSREG_AMBIENT_CUBE );
		pShader->PI_SetPixelShaderLocalLighting( PSREG_LIGHT_INFO_ARRAY );
		pShader->PI_EndCommandBuffer();
	}

	DYNAMIC_STATE
	{
		///////////////////////
		// VERTEX SHADER SETUP
		///////////////////////

		// Set Vertex Shader Combos
		DECLARE_DYNAMIC_VERTEX_SHADER( icesurface_vs30 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
		SET_DYNAMIC_VERTEX_SHADER( icesurface_vs30 );

		LightState_t lightState = { 0, false, false };
		pShaderAPI->GetDX9LightState( &lightState );

		// VS constants
		float flConsts[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		flConsts[0] = IS_PARAM_DEFINED( info.m_nUVScale ) ? params[info.m_nUVScale]->GetFloatValue() : kDefaultUVScale;
		pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, flConsts );

		if ( IS_PARAM_DEFINED( info.m_nUVProjOffset ) )
			params[info.m_nUVProjOffset]->GetVecValue( flConsts, 3 );
		else
			memcpy( flConsts, kDefaultUVProjOffset, sizeof( kDefaultUVProjOffset ) );
		pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_1, flConsts );

		if ( IS_PARAM_DEFINED( info.m_nBBMin ) )
			params[info.m_nBBMin]->GetVecValue( flConsts, 3 );
		else
			memcpy( flConsts, kDefaultBB, sizeof( kDefaultBB ) );
		pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_2, flConsts );

		if ( IS_PARAM_DEFINED( info.m_nBBMax ) )
			params[info.m_nBBMax]->GetVecValue( flConsts, 3 );
		else
			memcpy( flConsts, kDefaultBB, sizeof( kDefaultBB ) );
		pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3, flConsts );

		//////////////////////
		// PIXEL SHADER SETUP
		//////////////////////

		// Bind textures
		pShader->BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, BASETEXTURE );
		pShader->BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, info.m_nNormalMap, info.m_nBumpFrame );
		pShaderAPI->BindStandardTexture( SHADER_SAMPLER2, TEXTURE_BINDFLAGS_SRGBREAD, TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0 ); // Refraction Map
		
		if ( bSpecMap )
		{
			pShader->BindTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, info.m_nSpecMap );
		}
		else
		{
			pShaderAPI->BindStandardTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, TEXTURE_WHITE );
		}

		if ( bLightWarp )
		{
			pShader->BindTexture( SHADER_SAMPLER4, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nLightWarpTexture );
		}

		if ( bFresnelWarp )
		{
			pShader->BindTexture( SHADER_SAMPLER5, TEXTURE_BINDFLAGS_NONE, info.m_nFresnelWarpTexture );
		}

		if ( bOpacityTexture )
		{
			pShader->BindTexture( SHADER_SAMPLER6, TEXTURE_BINDFLAGS_NONE, info.m_nOpacityTexture );
		}

		if ( bEnvMap )
		{
			pShader->BindTexture( SHADER_SAMPLER7, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nEnvMap );
		}
		else
		{
			pShaderAPI->BindStandardTexture( SHADER_SAMPLER7, TEXTURE_BINDFLAGS_NONE, TEXTURE_BLACK );
		}

		// flashlightfixme: put this in common code.
		bool bFlashlightShadows = false;
		if( bHasFlashlight )
		{
			Assert( info.m_nFlashlightTexture >= 0 && info.m_nFlashlightTextureFrame >= 0 );
			pShader->BindTexture( SHADER_SAMPLER10, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nFlashlightTexture, info.m_nFlashlightTextureFrame );
			VMatrix worldToTexture;
			ITexture *pFlashlightDepthTexture;
			FlashlightState_t state = pShaderAPI->GetFlashlightStateEx( worldToTexture, &pFlashlightDepthTexture );
			bFlashlightShadows = state.m_bEnableShadows && ( pFlashlightDepthTexture != NULL );

			SetFlashLightColorFromState( state, pShaderAPI, PSREG_FLASHLIGHT_COLOR );

			if( pFlashlightDepthTexture && g_pConfig->ShadowDepthTexture() && state.m_bEnableShadows )
			{
				pShader->BindTexture( SHADER_SAMPLER8, TEXTURE_BINDFLAGS_SHADOWDEPTH, pFlashlightDepthTexture );
				pShaderAPI->BindStandardTexture( SHADER_SAMPLER9, TEXTURE_BINDFLAGS_NONE, TEXTURE_SHADOW_NOISE_2D );
			}

			float atten[4], pos[4], tweaks[4];

			atten[0] = state.m_fConstantAtten;		// Set the flashlight attenuation factors
			atten[1] = state.m_fLinearAtten;
			atten[2] = state.m_fQuadraticAtten;
			atten[3] = state.m_FarZAtten;
			pShaderAPI->SetPixelShaderConstant( PSREG_FLASHLIGHT_ATTENUATION, atten, 1 );

			pos[0] = state.m_vecLightOrigin[0];		// Set the flashlight origin
			pos[1] = state.m_vecLightOrigin[1];
			pos[2] = state.m_vecLightOrigin[2];
			pos[3] = state.m_FarZ;
			pShaderAPI->SetPixelShaderConstant( PSREG_FLASHLIGHT_POSITION_RIM_BOOST, pos, 1 );	// steps on rim boost

			pShaderAPI->SetPixelShaderConstant( PSREG_FLASHLIGHT_TO_WORLD_TEXTURE, worldToTexture.Base(), 4 );

			// Tweaks associated with a given flashlight
			tweaks[0] = ShadowFilterFromState( state );
			tweaks[1] = ShadowAttenFromState( state );
			pShader->HashShadow2DJitter( state.m_flShadowJitterSeed, &tweaks[2], &tweaks[3] );
			pShaderAPI->SetPixelShaderConstant( PSREG_ENVMAP_TINT__SHADOW_TWEAKS, tweaks, 1 );

			// Dimensions of screen, used for screen-space noise map sampling
			float vScreenScale[4] = {1280.0f / 32.0f, 720.0f / 32.0f, 0, 0};
			int nWidth, nHeight;
			pShaderAPI->GetBackBufferDimensions( nWidth, nHeight );

			int nTexWidth, nTexHeight;
			pShaderAPI->GetStandardTextureDimensions( &nTexWidth, &nTexHeight, TEXTURE_SHADOW_NOISE_2D );

			vScreenScale[0] = (float) nWidth  / nTexWidth;
			vScreenScale[1] = (float) nHeight / nTexHeight;

			pShaderAPI->SetPixelShaderConstant( PSREG_FLASHLIGHT_SCREEN_SCALE, vScreenScale, 1 );

			if ( IsX360() )
			{
				pShaderAPI->SetBooleanPixelShaderConstant( 0, &state.m_nShadowQuality, 1 );
			}
		}

		flConsts[0] = IS_PARAM_DEFINED( info.m_nBumpStrength ) ? params[info.m_nBumpStrength]->GetFloatValue() : kDefaultBumpStrength;
		flConsts[1] = (g_pHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT) ? 8192.0f : 192.0f;	// destalpha dest scale factor. TODO: put this in its own const and call shaderAPI method to set
		flConsts[2] = IS_PARAM_DEFINED( info.m_nInteriorFogStrength ) ? params[info.m_nInteriorFogStrength]->GetFloatValue() : kDefaultInteriorFogStrength;
		flConsts[3] = IS_PARAM_DEFINED( info.m_nInteriorRefractStrength ) ? params[info.m_nInteriorRefractStrength]->GetFloatValue() : kDefaultInteriorRefractStrength;
		pShaderAPI->SetPixelShaderConstant( 0, flConsts, 1 );

		Assert( IS_PARAM_DEFINED( info.m_nFresnelParams ) );
		if ( IS_PARAM_DEFINED( info.m_nFresnelParams ) )
			params[info.m_nFresnelParams]->GetVecValue( flConsts, 3 );
		else
			memcpy( flConsts, kDefaultFresnelParams, sizeof( kDefaultFresnelParams ) );
		flConsts[3] = params[info.m_nInteriorBackgroundBoost]->GetFloatValue();
		pShaderAPI->SetPixelShaderConstant( 1, flConsts, 1 );

		flConsts[0] = IS_PARAM_DEFINED( info.m_nRimLightExp ) ? params[info.m_nRimLightExp]->GetFloatValue() : kDefaultRimLightExp;
		flConsts[1] = IS_PARAM_DEFINED( info.m_nRimLightScale ) ? params[info.m_nRimLightScale]->GetFloatValue() : kDefaultRimLightScale;
		flConsts[2] = IS_PARAM_DEFINED( info.m_nSpecScale ) ? params[info.m_nSpecScale]->GetFloatValue() : kDefaultSpecScale;
		flConsts[3] = IS_PARAM_DEFINED( info.m_nSpecExp2 ) ? params[info.m_nSpecExp2]->GetFloatValue() : kDefaultSpecExp;
		pShaderAPI->SetPixelShaderConstant( 3, flConsts, 1 );

		flConsts[0] = IS_PARAM_DEFINED( info.m_nSpecScale2 ) ? params[info.m_nSpecScale2]->GetFloatValue() : kDefaultSpecScale;
		flConsts[1] = IS_PARAM_DEFINED( info.m_nFresnelBumpStrength ) ? params[info.m_nFresnelBumpStrength]->GetFloatValue() : kDefaultFresnelBumpStrength;
		flConsts[2] = IS_PARAM_DEFINED( info.m_nDiffuseScale ) ? params[info.m_nDiffuseScale]->GetFloatValue() : kDefaultDiffuseScale;
		flConsts[3] = IS_PARAM_DEFINED( info.m_nInteriorAmbientScale ) ? params[info.m_nInteriorAmbientScale]->GetFloatValue() : kDefaultInteriorAmbientScale;
		pShaderAPI->SetPixelShaderConstant( 10, flConsts, 1 );

		pShaderAPI->GetWorldSpaceCameraPosition( flConsts );
		flConsts[3] = IS_PARAM_DEFINED( info.m_nSpecExp ) ? params[info.m_nSpecExp]->GetFloatValue() : kDefaultSpecExp;
		pShaderAPI->SetPixelShaderConstant( PSREG_EYEPOS_SPEC_EXPONENT, flConsts, 1 );

		// Depth alpha [ TODO: support fog ]
		bool bWriteDepthToAlpha = pShaderAPI->ShouldWriteDepthToDestAlpha();
		flConsts[0] = bWriteDepthToAlpha ? 1.0f : 0.0f;
		pShaderAPI->SetPixelShaderConstant( PSREG_FOG_PARAMS, flConsts, 1 );

		if ( IS_PARAM_DEFINED( info.m_nBaseColorTint ) )
			params[info.m_nBaseColorTint]->GetVecValue( flConsts, 3 );
		else
			memcpy( flConsts, kDefaultBaseColorTint, sizeof( kDefaultBaseColorTint ) );
		flConsts[3] = IS_PARAM_DEFINED( info.m_nInteriorBackLightScale ) ? params[info.m_nInteriorBackLightScale]->GetFloatValue() : kDefaultInteriorBackLightScale;
		pShaderAPI->SetPixelShaderConstant( 19, flConsts, 1 );

		if ( IS_PARAM_DEFINED( info.m_nInteriorColor ) )
			params[info.m_nInteriorColor]->GetVecValue( flConsts, 3 );
		else
			memcpy( flConsts, kDefaultInteriorColor, sizeof( kDefaultInteriorColor ) );
		flConsts[3] = params[info.m_nInteriorRefractBlur]->GetFloatValue();
		pShaderAPI->SetPixelShaderConstant( 32, flConsts, 1 );

		float mView[16];
		pShaderAPI->GetMatrix( MATERIAL_VIEW, mView );
		pShaderAPI->SetPixelShaderConstant( 33, mView, 3 );

		flConsts[0] = IS_PARAM_DEFINED( info.m_nInteriorFogLimit ) ? params[info.m_nInteriorFogLimit]->GetFloatValue() : kDefaultInteriorFogLimit;
		flConsts[0] = 1.0f - flConsts[0];
		flConsts[1] = params[info.m_nInteriorFogNormalBoost]->GetFloatValue();
		pShaderAPI->SetPixelShaderConstant( 36, flConsts, 1 );

		if ( IS_PARAM_DEFINED( info.m_nEnvMapTint ) )
			params[info.m_nEnvMapTint]->GetVecValue( flConsts, 3 );
		else
			memcpy( flConsts, kDefaultEnvMapTint, sizeof( kDefaultEnvMapTint ) );
		pShaderAPI->SetPixelShaderConstant( 37, flConsts, 1 );

		// Set Pixel Shader Combos
		if( /*g_pHardwareConfig->SupportsPixelShaders_2_b()*/ true )
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( icesurface_ps30 );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( NUM_LIGHTS, lightState.m_nNumLights );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHT, bHasFlashlight );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHTSHADOWS, bFlashlightShadows );
			SET_DYNAMIC_PIXEL_SHADER( icesurface_ps30 );
		}
		else
		{
			Assert( !"No ps_3_0" );
		}
	}
	
    pShader->Draw();
}
