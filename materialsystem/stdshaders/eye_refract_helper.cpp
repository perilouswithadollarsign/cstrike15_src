//===== Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ======//

#include "BaseVSShader.h"
#include "mathlib/vmatrix.h"
#include "eye_refract_helper.h"

#include "cpp_shader_constant_register_map.h"

#include "eye_refract_vs20.inc"
#include "eye_refract_ps20.inc"
#include "eye_refract_ps20b.inc"

#if !defined( _X360 ) && !defined( _PS3 )
#include "eye_refract_vs30.inc"
#include "eye_refract_ps30.inc"
#endif

#include "convar.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


static ConVar r_lightwarpidentity( "r_lightwarpidentity","0", FCVAR_CHEAT );
static ConVar mat_displacementmap( "mat_displacementmap", "1", FCVAR_CHEAT );

void InitParams_Eyes_Refract( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, Eye_Refract_Vars_t &info )
{
	params[FLASHLIGHTTEXTURE]->SetStringValue( GetFlashlightTextureFilename() );

	// Set material flags
	SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
	SET_FLAGS2( MATERIAL_VAR2_LIGHTING_VERTEX_LIT );

	// Set material parameter default values
	if ( ( info.m_nDilation >= 0 ) && ( !params[info.m_nDilation]->IsDefined() ) )
	{
		params[info.m_nDilation]->SetFloatValue( kDefaultDilation );
	}

	if ( ( info.m_nGlossiness >= 0 ) && ( !params[info.m_nGlossiness]->IsDefined() ) )
	{
		params[info.m_nGlossiness]->SetFloatValue( kDefaultGlossiness );
	}

	if ( ( info.m_nSphereTexKillCombo >= 0 ) && ( !params[info.m_nSphereTexKillCombo]->IsDefined() ) )
	{
		params[info.m_nSphereTexKillCombo]->SetIntValue( kDefaultSphereTexKillCombo );
	}

	if ( ( info.m_nRaytraceSphere >= 0 ) && ( !params[info.m_nRaytraceSphere]->IsDefined() ) )
	{
		params[info.m_nRaytraceSphere]->SetIntValue( kDefaultRaytraceSphere );
	}

	if ( ( info.m_nAmbientOcclColor >= 0 ) && ( !params[info.m_nAmbientOcclColor]->IsDefined() ) )
	{
		params[info.m_nAmbientOcclColor]->SetVecValue( kDefaultAmbientOcclColor, 4 );
	}

	if ( ( info.m_nEyeballRadius >= 0 ) && ( !params[info.m_nEyeballRadius]->IsDefined() ) )
	{
		params[info.m_nEyeballRadius]->SetFloatValue( kDefaultEyeballRadius );
	}

	if ( ( info.m_nParallaxStrength >= 0 ) && ( !params[info.m_nParallaxStrength]->IsDefined() ) )
	{
		params[info.m_nParallaxStrength]->SetFloatValue( kDefaultParallaxStrength );
	}

	if ( ( info.m_nCorneaBumpStrength >= 0 ) && ( !params[info.m_nCorneaBumpStrength]->IsDefined() ) )
	{
		params[info.m_nCorneaBumpStrength]->SetFloatValue( kDefaultCorneaBumpStrength );
	}

	if ( ( info.m_nAmbientOcclusion >= 0 ) && ( !params[info.m_nAmbientOcclusion]->IsDefined() ) )
	{
		params[info.m_nAmbientOcclusion]->SetFloatValue( 0.0f );
	}
}

void Init_Eyes_Refract( CBaseVSShader *pShader, IMaterialVar** params, Eye_Refract_Vars_t &info )
{
	pShader->LoadTexture( info.m_nCorneaTexture );								// SHADER_SAMPLER0  (this is a normal, hence not sRGB)
	pShader->LoadTexture( info.m_nIris, TEXTUREFLAGS_SRGB );					// SHADER_SAMPLER1
	pShader->LoadCubeMap( info.m_nEnvmap, TEXTUREFLAGS_SRGB );					// SHADER_SAMPLER2
	pShader->LoadTexture( info.m_nAmbientOcclTexture, TEXTUREFLAGS_SRGB );		// SHADER_SAMPLER3

	if ( IS_PARAM_DEFINED( info.m_nDiffuseWarpTexture ) )
	{
		pShader->LoadTexture( info.m_nDiffuseWarpTexture );						// SHADER_SAMPLER4
	}

	pShader->LoadTexture( FLASHLIGHTTEXTURE, TEXTUREFLAGS_SRGB );				// SHADER_SAMPLER5
}

void Draw_Eyes_Refract_Internal( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
	IShaderShadow* pShaderShadow, bool bDrawFlashlightAdditivePass, Eye_Refract_Vars_t &info, VertexCompressionType_t vertexCompression )
{
	bool bDiffuseWarp = IS_PARAM_DEFINED( info.m_nDiffuseWarpTexture );

	SHADOW_STATE
	{
		SET_FLAGS2( MATERIAL_VAR2_LIGHTING_VERTEX_LIT );

		pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );	// Cornea normal
		pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );	// Iris
		pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );	// Cube reflection
		pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );	// Ambient occlusion

		// Set stream format (note that this shader supports compression)
		unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED;
		int nTexCoordCount = 1;
		int userDataSize = 0;
		pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );

		if ( bDiffuseWarp )
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );	// Light warp
		}

#if !defined( PLATFORM_X360 )
		bool bWorldNormal = ( ENABLE_FIXED_LIGHTING_OUTPUTNORMAL_AND_DEPTH == ( IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER0 ) + 2 * IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER1 )));
#endif

		ShadowFilterMode_t nShadowFilterMode = SHADOWFILTERMODE_DEFAULT;
		if ( bDrawFlashlightAdditivePass == true )
		{
			if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				nShadowFilterMode = g_pHardwareConfig->GetShadowFilterMode( false /* bForceLowQuality */, g_pHardwareConfig->HasFastVertexTextures() && !IsPlatformX360() && !IsPlatformPS3() );	// Based upon vendor and device dependent formats
			}

			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableAlphaWrites( false );
			pShader->EnableAlphaBlending( SHADER_BLEND_ONE, SHADER_BLEND_ONE ); // Additive blending 
			pShaderShadow->EnableTexture( SHADER_SAMPLER5, true );	// Flashlight cookie
		}
		else
		{
			pShaderShadow->EnableAlphaWrites( true );
		}

#if !defined( _X360 ) && !defined( _PS3 )
		if ( !g_pHardwareConfig->HasFastVertexTextures() )
#endif
		{
			DECLARE_STATIC_VERTEX_SHADER( eye_refract_vs20 );
			SET_STATIC_VERTEX_SHADER_COMBO( HALFLAMBERT, IS_FLAG_SET( MATERIAL_VAR_HALFLAMBERT ) );
			SET_STATIC_VERTEX_SHADER_COMBO( FLASHLIGHT, bDrawFlashlightAdditivePass ? 1 : 0 );
			SET_STATIC_VERTEX_SHADER_COMBO( LIGHTWARPTEXTURE, bDiffuseWarp ? 1 : 0 );
			SET_STATIC_VERTEX_SHADER_COMBO( WORLD_NORMAL, 0 );

			SET_STATIC_VERTEX_SHADER( eye_refract_vs20 );

			if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				bool bSphereTexKillCombo = IS_PARAM_DEFINED( info.m_nSphereTexKillCombo ) ? ( params[info.m_nSphereTexKillCombo]->GetIntValue() ? true : false ) : ( kDefaultSphereTexKillCombo ? true : false );
				bool bRayTraceSphere = IS_PARAM_DEFINED( info.m_nRaytraceSphere ) ? ( params[info.m_nRaytraceSphere]->GetIntValue() ? true : false ) : ( kDefaultRaytraceSphere ? true : false );

				DECLARE_STATIC_PIXEL_SHADER( eye_refract_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( SPHERETEXKILLCOMBO, bSphereTexKillCombo ? 1 : 0 );
				SET_STATIC_PIXEL_SHADER_COMBO( RAYTRACESPHERE, bRayTraceSphere ? 1 : 0 );
				SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHT, bDrawFlashlightAdditivePass ? 1 : 0 );
				SET_STATIC_PIXEL_SHADER_COMBO( LIGHTWARPTEXTURE, bDiffuseWarp ? 1 : 0 );
				SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode );
				SET_STATIC_PIXEL_SHADER_COMBO( WORLD_NORMAL, 0 );
				SET_STATIC_PIXEL_SHADER( eye_refract_ps20b );

				if ( bDrawFlashlightAdditivePass == true )
				{
					pShaderShadow->EnableTexture( SHADER_SAMPLER6, true );	// Shadow depth map
					//pShaderShadow->SetShadowDepthFiltering( SHADER_SAMPLER6 );
					pShaderShadow->EnableTexture( SHADER_SAMPLER7, true );	// Noise map
				}
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( eye_refract_ps20 );
				SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHT, bDrawFlashlightAdditivePass ? 1 : 0 );
				SET_STATIC_PIXEL_SHADER_COMBO( LIGHTWARPTEXTURE, bDiffuseWarp ? 1 : 0 );
				SET_STATIC_PIXEL_SHADER_COMBO( WORLD_NORMAL, 0 );
				SET_STATIC_PIXEL_SHADER( eye_refract_ps20 );
			}
		}
#if !defined( _X360 ) && !defined( _PS3 )
		else
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER8, true );	// Screen space ambient occlusion

			// The vertex shader uses the vertex id stream
			SET_FLAGS2( MATERIAL_VAR2_USES_VERTEXID );
			SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_TESSELLATION );

			DECLARE_STATIC_VERTEX_SHADER( eye_refract_vs30 );
			SET_STATIC_VERTEX_SHADER_COMBO( HALFLAMBERT, IS_FLAG_SET( MATERIAL_VAR_HALFLAMBERT ) );
			SET_STATIC_VERTEX_SHADER_COMBO( FLASHLIGHT, bDrawFlashlightAdditivePass ? 1 : 0 );
			SET_STATIC_VERTEX_SHADER_COMBO( LIGHTWARPTEXTURE, bDiffuseWarp ? 1 : 0 );
			SET_STATIC_VERTEX_SHADER_COMBO( WORLD_NORMAL, bWorldNormal );
			SET_STATIC_VERTEX_SHADER( eye_refract_vs30 );

			bool bSphereTexKillCombo = IS_PARAM_DEFINED( info.m_nSphereTexKillCombo ) ? ( params[info.m_nSphereTexKillCombo]->GetIntValue() ? true : false ) : ( kDefaultSphereTexKillCombo ? true : false );
			bool bRayTraceSphere = IS_PARAM_DEFINED( info.m_nRaytraceSphere ) ? ( params[info.m_nRaytraceSphere]->GetIntValue() ? true : false ) : ( kDefaultRaytraceSphere ? true : false );

			DECLARE_STATIC_PIXEL_SHADER( eye_refract_ps30 );
			SET_STATIC_PIXEL_SHADER_COMBO( SPHERETEXKILLCOMBO, bSphereTexKillCombo ? 1 : 0 );
			SET_STATIC_PIXEL_SHADER_COMBO( RAYTRACESPHERE, bRayTraceSphere ? 1 : 0 );
			SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHT, bDrawFlashlightAdditivePass ? 1 : 0 );
			SET_STATIC_PIXEL_SHADER_COMBO( LIGHTWARPTEXTURE, bDiffuseWarp ? 1 : 0 );
			SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode );
			SET_STATIC_PIXEL_SHADER_COMBO( WORLD_NORMAL, bWorldNormal );
			SET_STATIC_PIXEL_SHADER( eye_refract_ps30 );

			if ( bDrawFlashlightAdditivePass == true )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER6, true );	// Shadow depth map
				pShaderShadow->EnableTexture( SHADER_SAMPLER7, true );	// Noise map
			}
		}
#endif

		// On DX9, get the gamma read and write correct
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, true );			// Iris
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER2, true );			// Cube map reflection
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER3, true );			// Ambient occlusion
		pShaderShadow->EnableSRGBWrite( true );

		if ( bDrawFlashlightAdditivePass == true )
		{
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER5, true );		// Flashlight cookie
		}

		// Fog
		if ( bDrawFlashlightAdditivePass == true )
		{
			pShader->FogToBlack();
		}
		else
		{
			pShader->FogToFogColor();
		}

		// Per-instance state
		pShader->PI_BeginCommandBuffer();
		if ( !bDrawFlashlightAdditivePass )
		{
			pShader->PI_SetPixelShaderLocalLighting( PSREG_LIGHT_INFO_ARRAY );
		}
		pShader->PI_SetVertexShaderAmbientLightCube();
		pShader->PI_SetPixelShaderAmbientLightCubeLuminance( 10 );
		pShader->PI_EndCommandBuffer();
	}
	DYNAMIC_STATE
	{
		VMatrix worldToTexture;
		ITexture *pFlashlightDepthTexture = NULL;
		FlashlightState_t flashlightState;
		bool bFlashlightShadows = false;
		if ( bDrawFlashlightAdditivePass == true )
		{
			flashlightState = pShaderAPI->GetFlashlightStateEx( worldToTexture, &pFlashlightDepthTexture );
			bFlashlightShadows = flashlightState.m_bEnableShadows;
		}

		bool bSinglePassFlashlight = false;

		pShader->BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, info.m_nCorneaTexture );					// Cornea normal
		pShader->BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nIris, info.m_nIrisFrame );
		pShader->BindTexture( SHADER_SAMPLER2, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nEnvmap );
		pShader->BindTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nAmbientOcclTexture );

		if ( bDiffuseWarp )
		{
			if ( r_lightwarpidentity.GetBool() )
			{
				pShaderAPI->BindStandardTexture( SHADER_SAMPLER4, TEXTURE_BINDFLAGS_NONE, TEXTURE_IDENTITY_LIGHTWARP );
			}
			else
			{
				pShader->BindTexture( SHADER_SAMPLER4, TEXTURE_BINDFLAGS_NONE, info.m_nDiffuseWarpTexture );
			}
		}

		// On PC, we sample from ambient occlusion texture
		if ( IsPC() && g_pHardwareConfig->HasFastVertexTextures() )
		{
			ITexture *pAOTexture = pShaderAPI->GetTextureRenderingParameter( TEXTURE_RENDERPARM_AMBIENT_OCCLUSION );

			if ( pAOTexture )
			{
				pShader->BindTexture( SHADER_SAMPLER8, TEXTURE_BINDFLAGS_NONE, pAOTexture );
			}
			else
			{
				pShaderAPI->BindStandardTexture( SHADER_SAMPLER8, TEXTURE_BINDFLAGS_NONE, TEXTURE_WHITE );
			}
		}

		if ( bDrawFlashlightAdditivePass == true )
			pShader->BindTexture( SHADER_SAMPLER5, TEXTURE_BINDFLAGS_SRGBREAD, flashlightState.m_pSpotlightTexture, flashlightState.m_nSpotlightTextureFrame );

		pShader->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, info.m_nEyeOrigin );
		pShader->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_2, info.m_nIrisU );
		pShader->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3, info.m_nIrisV );

		if ( bDrawFlashlightAdditivePass == true )
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_4, flashlightState.m_vecLightOrigin.Base(), 1 );

		LightState_t lightState = { 0, false, false };
		if ( bDrawFlashlightAdditivePass == false )
		{
			pShaderAPI->GetDX9LightState( &lightState );
		}

		int nFixedLightingMode = pShaderAPI->GetIntRenderingParameter( INT_RENDERPARM_ENABLE_FIXED_LIGHTING );

#if !defined( _X360 ) && !defined( _PS3 )
		if ( !g_pHardwareConfig->HasFastVertexTextures() )
#endif
		{
			DECLARE_DYNAMIC_VERTEX_SHADER( eye_refract_vs20 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( DYNAMIC_LIGHT, lightState.HasDynamicLight() );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( NUM_LIGHTS, lightState.m_nNumLights );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( TESSELLATION, 0 );
			SET_DYNAMIC_VERTEX_SHADER( eye_refract_vs20 );
		}
#if !defined( _X360 ) && !defined( _PS3 )
		else
		{
			pShader->SetHWMorphVertexShaderState( VERTEX_SHADER_SHADER_SPECIFIC_CONST_10, VERTEX_SHADER_SHADER_SPECIFIC_CONST_11, SHADER_VERTEXTEXTURE_SAMPLER0 );

			if ( nFixedLightingMode == ENABLE_FIXED_LIGHTING_OUTPUTNORMAL_AND_DEPTH )
			{
				float vEyeDir[4];
				pShaderAPI->GetWorldSpaceCameraDirection( vEyeDir );

				float flFarZ = pShaderAPI->GetFarZ();
				vEyeDir[0] /= flFarZ;	// Divide by farZ for SSAO algorithm
				vEyeDir[1] /= flFarZ;
				vEyeDir[2] /= flFarZ;
				pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_5, vEyeDir );
			}

			TessellationMode_t nTessellationMode = pShaderAPI->GetTessellationMode();
			if ( nTessellationMode != TESSELLATION_MODE_DISABLED )
			{
				pShaderAPI->BindStandardVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER1, TEXTURE_SUBDIVISION_PATCHES );

				bool bHasDisplacement = false; // TODO
				float vSubDDimensions[4] = { 1.0f/pShaderAPI->GetSubDHeight(), bHasDisplacement && mat_displacementmap.GetBool() ? 1.0f : 0.0f, 0.0f, 0.0f };
				pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_1, vSubDDimensions );
			}

			DECLARE_DYNAMIC_VERTEX_SHADER( eye_refract_vs30 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( DYNAMIC_LIGHT, lightState.HasDynamicLight() );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( NUM_LIGHTS, lightState.m_nNumLights );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( TESSELLATION, nTessellationMode );
			SET_DYNAMIC_VERTEX_SHADER( eye_refract_vs30 );
		}
#endif

		// Special constant for DX9 eyes: { Dilation, Glossiness, x, x };
		float vPSConst[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		vPSConst[0] = IS_PARAM_DEFINED( info.m_nDilation ) ? params[info.m_nDilation]->GetFloatValue() : kDefaultDilation;
		vPSConst[1] = IS_PARAM_DEFINED( info.m_nGlossiness ) ? params[info.m_nGlossiness]->GetFloatValue() : kDefaultGlossiness;
		vPSConst[2] = 0.0f; // NOT USED
		vPSConst[3] = IS_PARAM_DEFINED( info.m_nCorneaBumpStrength ) ? params[info.m_nCorneaBumpStrength]->GetFloatValue() : kDefaultCorneaBumpStrength;
		pShaderAPI->SetPixelShaderConstant( 0, vPSConst, 1 );

		pShaderAPI->SetPixelShaderConstant( 1, IS_PARAM_DEFINED( info.m_nEyeOrigin ) ? params[info.m_nEyeOrigin]->GetVecValue() : kDefaultEyeOrigin, 1 );
		pShaderAPI->SetPixelShaderConstant( 2, IS_PARAM_DEFINED( info.m_nIrisU ) ? params[info.m_nIrisU]->GetVecValue() : kDefaultIrisU, 1 );
		pShaderAPI->SetPixelShaderConstant( 3, IS_PARAM_DEFINED( info.m_nIrisV ) ? params[info.m_nIrisV]->GetVecValue() : kDefaultIrisV, 1 );

		float vEyePos[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		pShaderAPI->GetWorldSpaceCameraPosition( vEyePos );
		pShaderAPI->SetPixelShaderConstant( 4, vEyePos, 1 );

		float vAmbientOcclusion[4] = { 0.33f, 0.33f, 0.33f, 0.0f };
		if ( IS_PARAM_DEFINED( info.m_nAmbientOcclColor ) )
		{
			params[info.m_nAmbientOcclColor]->GetVecValue( vAmbientOcclusion, 3 );
		}
		vAmbientOcclusion[3] = IS_PARAM_DEFINED( info.m_nAmbientOcclusion ) ? params[info.m_nAmbientOcclusion]->GetFloatValue() : 0.0f;

		float vPackedConst6[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		//vPackedConst6[0] Unused
		vPackedConst6[1] = IS_PARAM_DEFINED( info.m_nEyeballRadius ) ? params[info.m_nEyeballRadius]->GetFloatValue() : kDefaultEyeballRadius;
		//vPackedConst6[2] = IS_PARAM_DEFINED( info.m_nRaytraceSphere ) ? params[info.m_nRaytraceSphere]->GetFloatValue() : kDefaultRaytraceSphere;
		vPackedConst6[3] = IS_PARAM_DEFINED( info.m_nParallaxStrength ) ? params[info.m_nParallaxStrength]->GetFloatValue() : kDefaultParallaxStrength;
		pShaderAPI->SetPixelShaderConstant( 6, vPackedConst6, 1 );

		if ( bDrawFlashlightAdditivePass == true )
		{
			SetFlashLightColorFromState( flashlightState, pShaderAPI, bSinglePassFlashlight );

			if ( pFlashlightDepthTexture && g_pConfig->ShadowDepthTexture() && flashlightState.m_bEnableShadows )
			{
				pShader->BindTexture( SHADER_SAMPLER6, TEXTURE_BINDFLAGS_SHADOWDEPTH, pFlashlightDepthTexture, 0 );
				pShaderAPI->BindStandardTexture( SHADER_SAMPLER7, TEXTURE_BINDFLAGS_NONE, TEXTURE_SHADOW_NOISE_2D );
			}
		}

		if ( nFixedLightingMode == ENABLE_FIXED_LIGHTING_OUTPUTNORMAL_AND_DEPTH )
		{
			float vEyeDir[4];
			pShaderAPI->GetWorldSpaceCameraDirection( vEyeDir );

			float flFarZ = pShaderAPI->GetFarZ();
			vEyeDir[0] /= flFarZ;	// Divide by farZ for SSAO algorithm
			vEyeDir[1] /= flFarZ;
			vEyeDir[2] /= flFarZ;
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_5, vEyeDir );
		}

		// Flashlight tax
#if !defined( _X360 ) && !defined( _PS3 )
		if ( !g_pHardwareConfig->HasFastVertexTextures() )
#endif
		{
			if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( eye_refract_ps20b );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( NUM_LIGHTS, lightState.m_nNumLights );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHTSHADOWS, bFlashlightShadows );
				SET_DYNAMIC_PIXEL_SHADER( eye_refract_ps20b );
			}
			else // ps.2.0
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( eye_refract_ps20 );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( NUM_LIGHTS, lightState.m_nNumLights );
				SET_DYNAMIC_PIXEL_SHADER( eye_refract_ps20 );
			}
		}
#if !defined( _X360 ) && !defined( _PS3 )
		else
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( eye_refract_ps30 );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( NUM_LIGHTS, nFixedLightingMode ? 0 : lightState.m_nNumLights );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHTSHADOWS, nFixedLightingMode ? false : bFlashlightShadows );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( UBERLIGHT, flashlightState.m_bUberlight );
			SET_DYNAMIC_PIXEL_SHADER( eye_refract_ps30 );

			// Set constant to enable translation of VPOS to render target coordinates in ps_3_0
			pShaderAPI->SetScreenSizeForVPOS();

			SetupUberlightFromState( pShaderAPI, flashlightState );
		}
#endif

		pShaderAPI->SetPixelShaderFogParams( PSREG_FOG_PARAMS );

		if ( bDrawFlashlightAdditivePass == true )
		{
			float atten[4], pos[4], tweaks[4];
			atten[0] = flashlightState.m_fConstantAtten;		// Set the flashlight attenuation factors
			atten[1] = flashlightState.m_fLinearAtten;
			atten[2] = flashlightState.m_fQuadraticAtten;
			atten[3] = flashlightState.m_FarZAtten;
			pShaderAPI->SetPixelShaderConstant( 7, atten, 1 );

			pos[0] = flashlightState.m_vecLightOrigin[0];		// Set the flashlight origin
			pos[1] = flashlightState.m_vecLightOrigin[1];
			pos[2] = flashlightState.m_vecLightOrigin[2];
			pShaderAPI->SetPixelShaderConstant( 8, pos, 1 );

			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6, worldToTexture.Base(), 4 );

			// Tweaks associated with a given flashlight
			tweaks[0] = ShadowFilterFromState( flashlightState );
			tweaks[1] = ShadowAttenFromState( flashlightState );
			pShader->HashShadow2DJitter( flashlightState.m_flShadowJitterSeed, &tweaks[2], &tweaks[3] );
			pShaderAPI->SetPixelShaderConstant( 9, tweaks, 1 );

			// Dimensions of screen, used for screen-space noise map sampling
			float vScreenScale[4] = {1280.0f / 32.0f, 720.0f / 32.0f, 0, 0};
			int nWidth, nHeight;
			pShaderAPI->GetBackBufferDimensions( nWidth, nHeight );

			int nTexWidth, nTexHeight;
			pShaderAPI->GetStandardTextureDimensions( &nTexWidth, &nTexHeight, TEXTURE_SHADOW_NOISE_2D );

			vScreenScale[0] = (float) nWidth  / nTexWidth;
			vScreenScale[1] = (float) nHeight / nTexHeight;
			vScreenScale[2] = 1.0f / flashlightState.m_flShadowMapResolution;
			vScreenScale[3] = 2.0f / flashlightState.m_flShadowMapResolution;
			pShaderAPI->SetPixelShaderConstant( PSREG_FLASHLIGHT_SCREEN_SCALE, vScreenScale, 1 );

			vAmbientOcclusion[3] *= flashlightState.m_flAmbientOcclusion;
		}

		vAmbientOcclusion[3] = MIN( MAX( vAmbientOcclusion[3], 0.0f ), 1.0f );
		pShaderAPI->SetPixelShaderConstant( 5, vAmbientOcclusion, 1 );
	}
	pShader->Draw();
}


void Draw_Eyes_Refract( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
	IShaderShadow* pShaderShadow, Eye_Refract_Vars_t &info, VertexCompressionType_t vertexCompression )
{
	bool bHasFlashlight = pShader->UsingFlashlight( params );
	if ( bHasFlashlight && ( IsX360() || IsPS3() ) )
	{
		Draw_Eyes_Refract_Internal( pShader, params, pShaderAPI, pShaderShadow, false, info, vertexCompression );
		if ( pShaderShadow )
		{
			pShader->SetInitialShadowState( );
		}
	}
	Draw_Eyes_Refract_Internal( pShader, params, pShaderAPI, pShaderShadow, bHasFlashlight, info, vertexCompression );
}
