//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "BaseVSShader.h"
#include "cpp_shader_constant_register_map.h"

#include "teeth_vs20.inc"
#include "teeth_flashlight_vs20.inc"
#include "teeth_bump_vs20.inc"
#include "teeth_ps20.inc"
#include "teeth_ps20b.inc"
#include "teeth_flashlight_ps20.inc"
#include "teeth_flashlight_ps20b.inc"
#include "teeth_bump_ps20.inc"
#include "teeth_bump_ps20b.inc"

#if !defined( _X360 ) && !defined( _PS3 )
#include "teeth_vs30.inc"
#include "teeth_ps30.inc"
#include "teeth_bump_vs30.inc"
#include "teeth_bump_ps30.inc"
#include "teeth_flashlight_vs30.inc"
#include "teeth_flashlight_ps30.inc"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DEFINE_FALLBACK_SHADER( Teeth, Teeth_DX9 )

BEGIN_VS_SHADER( Teeth_DX9, "Help for Teeth_DX9" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( ILLUMFACTOR, SHADER_PARAM_TYPE_FLOAT, "1", "Amount to darken or brighten the teeth" )
		SHADER_PARAM( FORWARD, SHADER_PARAM_TYPE_VEC3, "[1 0 0]", "Forward direction vector for teeth lighting" )
		SHADER_PARAM( BUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "models/shadertest/shader1_normal", "bump map" )
		SHADER_PARAM( PHONGEXPONENT, SHADER_PARAM_TYPE_FLOAT, "100", "phong exponent" )
 	    SHADER_PARAM( ENTITYORIGIN, SHADER_PARAM_TYPE_VEC3,"0.0","center if the model in world space" )
 	    SHADER_PARAM( WARPPARAM, SHADER_PARAM_TYPE_FLOAT,"0.0","animation param between 0 and 1" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		params[FLASHLIGHTTEXTURE]->SetStringValue( GetFlashlightTextureFilename() );

		SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		LoadTexture( FLASHLIGHTTEXTURE, TEXTUREFLAGS_SRGB );
		LoadTexture( BASETEXTURE, TEXTUREFLAGS_SRGB );

		if( params[BUMPMAP]->IsDefined() )
		{
			LoadTexture( BUMPMAP );
		}
	}

	void DrawUsingVertexShader( IMaterialVar** params, IShaderDynamicAPI *pShaderAPI, IShaderShadow* pShaderShadow, VertexCompressionType_t vertexCompression )
	{
		bool hasBump = params[BUMPMAP]->IsTexture();

		BlendType_t nBlendType = EvaluateBlendRequirements( BASETEXTURE, true );
		bool bFullyOpaque = (nBlendType != BT_BLENDADD) && (nBlendType != BT_BLEND) && !IS_FLAG_SET(MATERIAL_VAR_ALPHATEST); //dest alpha is free for special use
		
		SHADOW_STATE
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );		// Base map

			int flags = VERTEX_POSITION | VERTEX_NORMAL;
			int nTexCoordCount = 1;
			int userDataSize = 0;

			if ( hasBump )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );	// Bump map
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, false );
				pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );	// Normalization sampler for per-pixel lighting
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER2, false );
				userDataSize = 4;										// tangent S
			}

			// This shader supports compressed vertices, so OR in that flag:
			flags |= VERTEX_FORMAT_COMPRESSED;
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );

			if ( hasBump )
			{
#if !defined( _X360 ) && !defined( _PS3 )
				if ( !g_pHardwareConfig->HasFastVertexTextures() )
#endif
				{
					bool bFlattenStaticControlFlow = !g_pHardwareConfig->SupportsStaticControlFlow();

					DECLARE_STATIC_VERTEX_SHADER( teeth_bump_vs20 );
					SET_STATIC_VERTEX_SHADER_COMBO( FLATTEN_STATIC_CONTROL_FLOW, bFlattenStaticControlFlow );
					SET_STATIC_VERTEX_SHADER( teeth_bump_vs20 );

					// ps_2_b version which does phong
					if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
					{
						DECLARE_STATIC_PIXEL_SHADER( teeth_bump_ps20b );
						SET_STATIC_PIXEL_SHADER( teeth_bump_ps20b );
					}
					else
					{
						DECLARE_STATIC_PIXEL_SHADER( teeth_bump_ps20 );
						SET_STATIC_PIXEL_SHADER( teeth_bump_ps20 );
					}
				}
#if !defined( _X360 ) && !defined( _PS3 )
				else
				{
					// The vertex shader uses the vertex id stream
					SET_FLAGS2( MATERIAL_VAR2_USES_VERTEXID );

					DECLARE_STATIC_VERTEX_SHADER( teeth_bump_vs30 );
					SET_STATIC_VERTEX_SHADER( teeth_bump_vs30 );

					DECLARE_STATIC_PIXEL_SHADER( teeth_bump_ps30 );
					SET_STATIC_PIXEL_SHADER( teeth_bump_ps30 );
				}
#endif
			}
			else
			{
#if !defined( _X360 ) && !defined( _PS3 )
				if ( !g_pHardwareConfig->HasFastVertexTextures() )
#endif
				{
					bool bFlattenStaticControlFlow = !g_pHardwareConfig->SupportsStaticControlFlow();

					DECLARE_STATIC_VERTEX_SHADER( teeth_vs20 );
					SET_STATIC_VERTEX_SHADER_COMBO( FLATTEN_STATIC_CONTROL_FLOW, bFlattenStaticControlFlow );
					SET_STATIC_VERTEX_SHADER( teeth_vs20 );

					if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
					{
						DECLARE_STATIC_PIXEL_SHADER( teeth_ps20b );
						SET_STATIC_PIXEL_SHADER( teeth_ps20b );
					}
					else
					{
						DECLARE_STATIC_PIXEL_SHADER( teeth_ps20 );
						SET_STATIC_PIXEL_SHADER( teeth_ps20 );
					}
				}
#if !defined( _X360 ) && !defined( _PS3 )
				else
				{
					// The vertex shader uses the vertex id stream
					SET_FLAGS2( MATERIAL_VAR2_USES_VERTEXID );

					DECLARE_STATIC_VERTEX_SHADER( teeth_vs30 );
					SET_STATIC_VERTEX_SHADER( teeth_vs30 );

					DECLARE_STATIC_PIXEL_SHADER( teeth_ps30 );
					SET_STATIC_PIXEL_SHADER( teeth_ps30 );
				}
#endif
			}

			// On DX9, do sRGB
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, true );
			pShaderShadow->EnableSRGBWrite( true );

			FogToFogColor();

			pShaderShadow->EnableAlphaWrites( bFullyOpaque );

			// Lighting constants
			PI_BeginCommandBuffer();
			PI_SetPixelShaderAmbientLightCube( PSREG_AMBIENT_CUBE );
			PI_SetPixelShaderLocalLighting( PSREG_LIGHT_INFO_ARRAY );

			// For non-bumped case, ambient cube is computed in the vertex shader
			if ( !hasBump )
			{
				PI_SetVertexShaderAmbientLightCube();
			}

			PI_EndCommandBuffer();
		}
		DYNAMIC_STATE
		{
			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, BASETEXTURE, FRAME );
			if ( hasBump )
			{
				BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, BUMPMAP );
			}
			pShaderAPI->BindStandardTexture( SHADER_SAMPLER2, TEXTURE_BINDFLAGS_NONE, TEXTURE_NORMALIZATION_CUBEMAP_SIGNED );

			Vector4D lighting;
			params[FORWARD]->GetVecValue( lighting.Base(), 3 );
			lighting[3] = params[ILLUMFACTOR]->GetFloatValue();
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, lighting.Base() );

			LightState_t lightState = {0, false, false};
			pShaderAPI->GetDX9LightState( &lightState );

			pShaderAPI->SetPixelShaderFogParams( PSREG_FOG_PARAMS );

			float vEyePos_SpecExponent[4];
			pShaderAPI->GetWorldSpaceCameraPosition( vEyePos_SpecExponent );
			vEyePos_SpecExponent[3] = 0.0f;
			pShaderAPI->SetPixelShaderConstant( PSREG_EYEPOS_SPEC_EXPONENT, vEyePos_SpecExponent, 1 );

			if ( hasBump )
			{	
#if !defined( _X360 ) && !defined( _PS3 )
				if ( !g_pHardwareConfig->HasFastVertexTextures() )
#endif
				{
					bool bUseStaticControlFlow = g_pHardwareConfig->SupportsStaticControlFlow();

					DECLARE_DYNAMIC_VERTEX_SHADER( teeth_bump_vs20 );
					SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
					SET_DYNAMIC_VERTEX_SHADER_COMBO( STATIC_LIGHT,  lightState.m_bStaticLight  ? 1 : 0 );
					SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
					SET_DYNAMIC_VERTEX_SHADER_COMBO( NUM_LIGHTS, bUseStaticControlFlow ? 0 : lightState.m_nNumLights );
					SET_DYNAMIC_VERTEX_SHADER( teeth_bump_vs20 );
		
					// ps_2_b version which does Phong
					if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
					{
						Vector4D vSpecExponent;
						vSpecExponent[3] = params[PHONGEXPONENT]->GetFloatValue();

						pShaderAPI->SetPixelShaderConstant( PSREG_EYEPOS_SPEC_EXPONENT, vSpecExponent.Base(), 1 );

						DECLARE_DYNAMIC_PIXEL_SHADER( teeth_bump_ps20b );
						SET_DYNAMIC_PIXEL_SHADER_COMBO( NUM_LIGHTS,  lightState.m_nNumLights );
						SET_DYNAMIC_PIXEL_SHADER_COMBO( AMBIENT_LIGHT, lightState.m_bAmbientLight ? 1 : 0 );
						SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITE_DEPTH_TO_DESTALPHA, bFullyOpaque && pShaderAPI->ShouldWriteDepthToDestAlpha() );
						SET_DYNAMIC_PIXEL_SHADER( teeth_bump_ps20b );
					}
					else
					{
						DECLARE_DYNAMIC_PIXEL_SHADER( teeth_bump_ps20 );
						SET_DYNAMIC_PIXEL_SHADER_COMBO( NUM_LIGHTS, lightState.m_nNumLights );
						SET_DYNAMIC_PIXEL_SHADER_COMBO( AMBIENT_LIGHT, lightState.m_bAmbientLight ? 1 : 0 );
						SET_DYNAMIC_PIXEL_SHADER( teeth_bump_ps20 );
					}
				}
#if !defined( _X360 ) && !defined( _PS3 )
				else
				{
					SetHWMorphVertexShaderState( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6, VERTEX_SHADER_SHADER_SPECIFIC_CONST_7, SHADER_VERTEXTEXTURE_SAMPLER0 );

					DECLARE_DYNAMIC_VERTEX_SHADER( teeth_bump_vs30 );
					SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
					SET_DYNAMIC_VERTEX_SHADER_COMBO( STATIC_LIGHT,  lightState.m_bStaticLight  ? 1 : 0 );
					SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
					SET_DYNAMIC_VERTEX_SHADER( teeth_bump_vs30 );

					Vector4D vSpecExponent;
					vSpecExponent[3] = params[PHONGEXPONENT]->GetFloatValue();
					pShaderAPI->SetPixelShaderConstant( PSREG_EYEPOS_SPEC_EXPONENT, vSpecExponent.Base(), 1 );

					DECLARE_DYNAMIC_PIXEL_SHADER( teeth_bump_ps30 );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( NUM_LIGHTS,  lightState.m_nNumLights );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( AMBIENT_LIGHT, lightState.m_bAmbientLight ? 1 : 0 );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITE_DEPTH_TO_DESTALPHA, bFullyOpaque && pShaderAPI->ShouldWriteDepthToDestAlpha() );
					SET_DYNAMIC_PIXEL_SHADER( teeth_bump_ps30 );
				}
#endif
			}
			else
			{
#if !defined( _X360 ) && !defined( _PS3 )
				if ( !g_pHardwareConfig->HasFastVertexTextures() )
#endif
				{
					bool bUseStaticControlFlow = g_pHardwareConfig->SupportsStaticControlFlow();

					DECLARE_DYNAMIC_VERTEX_SHADER( teeth_vs20 );
					SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
					SET_DYNAMIC_VERTEX_SHADER_COMBO( DYNAMIC_LIGHT, lightState.HasDynamicLight() );
					SET_DYNAMIC_VERTEX_SHADER_COMBO( STATIC_LIGHT,  lightState.m_bStaticLight  ? 1 : 0 );
					SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
					SET_DYNAMIC_VERTEX_SHADER_COMBO( NUM_LIGHTS, bUseStaticControlFlow ? 0 : lightState.m_nNumLights );
					SET_DYNAMIC_VERTEX_SHADER( teeth_vs20 );

					if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
					{
						DECLARE_DYNAMIC_PIXEL_SHADER( teeth_ps20b );
						SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITE_DEPTH_TO_DESTALPHA, bFullyOpaque && pShaderAPI->ShouldWriteDepthToDestAlpha() );
						SET_DYNAMIC_PIXEL_SHADER( teeth_ps20b );
					}
					else
					{
						DECLARE_DYNAMIC_PIXEL_SHADER( teeth_ps20 );
						SET_DYNAMIC_PIXEL_SHADER( teeth_ps20 );
					}
				}
#if !defined( _X360 ) && !defined( _PS3 )
				else
				{
					SetHWMorphVertexShaderState( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6, VERTEX_SHADER_SHADER_SPECIFIC_CONST_7, SHADER_VERTEXTEXTURE_SAMPLER0 );

					DECLARE_DYNAMIC_VERTEX_SHADER( teeth_vs30 );
					SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
					SET_DYNAMIC_VERTEX_SHADER_COMBO( DYNAMIC_LIGHT, lightState.HasDynamicLight() );
					SET_DYNAMIC_VERTEX_SHADER_COMBO( STATIC_LIGHT,  lightState.m_bStaticLight  ? 1 : 0 );
					SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
					SET_DYNAMIC_VERTEX_SHADER( teeth_vs30 );

					DECLARE_DYNAMIC_PIXEL_SHADER( teeth_ps30 );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITE_DEPTH_TO_DESTALPHA, bFullyOpaque && pShaderAPI->ShouldWriteDepthToDestAlpha() );
					SET_DYNAMIC_PIXEL_SHADER( teeth_ps30 );
				}
#endif
			}
		}
		Draw();
	}

	void DrawFlashlight( IMaterialVar** params, IShaderDynamicAPI *pShaderAPI, IShaderShadow* pShaderShadow, VertexCompressionType_t vertexCompression )
	{
		SHADOW_STATE
		{
			// Be sure not to write to dest alpha
			pShaderShadow->EnableAlphaWrites( false );

			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );		// Base map
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );		// Flashlight spot
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, true );

			// Additive blend the teeth, lit by the flashlight
			s_pShaderShadow->EnableAlphaTest( false );
			s_pShaderShadow->BlendFunc( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
			s_pShaderShadow->EnableBlending( true );

			// Set stream format (note that this shader supports compression)
			int flags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED;
			int nTexCoordCount = 1;
			int userDataSize = 0;
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );

			ShadowFilterMode_t nShadowFilterMode = SHADOWFILTERMODE_DEFAULT;
			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );		// shadow depth map
				//pShaderShadow->SetShadowDepthFiltering( SHADER_SAMPLER2 );
				pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );		// shadow noise

				nShadowFilterMode = g_pHardwareConfig->GetShadowFilterMode( false /*bForceLowQuality */, g_pHardwareConfig->HasFastVertexTextures() && !IsPlatformX360() && !IsPlatformPS3() /* bPS30 */ );	// Based upon vendor and device dependent formats
			}

#if !defined( _X360 ) && !defined( _PS3 )
			if ( !g_pHardwareConfig->HasFastVertexTextures() )
#endif
			{
				DECLARE_STATIC_VERTEX_SHADER( teeth_flashlight_vs20 );
				SET_STATIC_VERTEX_SHADER( teeth_flashlight_vs20 );

				if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
				{
					DECLARE_STATIC_PIXEL_SHADER( teeth_flashlight_ps20b );
					SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode );
					SET_STATIC_PIXEL_SHADER( teeth_flashlight_ps20b );
				}
				else
				{
					DECLARE_STATIC_PIXEL_SHADER( teeth_flashlight_ps20 );
					SET_STATIC_PIXEL_SHADER( teeth_flashlight_ps20 );
				}
			}
#if !defined( _X360 ) && !defined( _PS3 )
			else
			{
				// The vertex shader uses the vertex id stream
				SET_FLAGS2( MATERIAL_VAR2_USES_VERTEXID );

				DECLARE_STATIC_VERTEX_SHADER( teeth_flashlight_vs30 );
				SET_STATIC_VERTEX_SHADER( teeth_flashlight_vs30 );

				DECLARE_STATIC_PIXEL_SHADER( teeth_flashlight_ps30 );
				SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode );
				SET_STATIC_PIXEL_SHADER( teeth_flashlight_ps30 );
			}
#endif
			// On DX9, do sRGB
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, true );
			pShaderShadow->EnableSRGBWrite( true );

			FogToFogColor();
		}
		DYNAMIC_STATE
		{
			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, BASETEXTURE, FRAME );

			// State for spotlight projection, attenuation etc
			SetFlashlightVertexShaderConstants( false, -1, false, -1, true );

			VMatrix worldToTexture;
			ITexture *pFlashlightDepthTexture;
			FlashlightState_t flashlightState = pShaderAPI->GetFlashlightStateEx( worldToTexture, &pFlashlightDepthTexture );
			SetFlashLightColorFromState( flashlightState, pShaderAPI, false, PSREG_FLASHLIGHT_COLOR );

			bool bFlashlightShadows = g_pHardwareConfig->SupportsPixelShaders_2_b() ? ( flashlightState.m_bEnableShadows && ( pFlashlightDepthTexture != NULL ) ) : false;
			if( pFlashlightDepthTexture && g_pConfig->ShadowDepthTexture() && flashlightState.m_bEnableShadows )
			{
				BindTexture( SHADER_SAMPLER2, TEXTURE_BINDFLAGS_SHADOWDEPTH, pFlashlightDepthTexture, 0 );
				pShaderAPI->BindStandardTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, TEXTURE_SHADOW_NOISE_2D );
			}

			Vector4D lighting;
			params[FORWARD]->GetVecValue( lighting.Base(), 3 );
			lighting[3] = params[ILLUMFACTOR]->GetFloatValue();
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_8, lighting.Base() );

			float atten[4], pos[4], tweaks[4];
			SetFlashLightColorFromState( flashlightState, pShaderAPI, false, PSREG_FLASHLIGHT_COLOR );

			BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_SRGBREAD, flashlightState.m_pSpotlightTexture, flashlightState.m_nSpotlightTextureFrame );

			atten[0] = flashlightState.m_fConstantAtten;		// Set the flashlight attenuation factors
			atten[1] = flashlightState.m_fLinearAtten;
			atten[2] = flashlightState.m_fQuadraticAtten;
			atten[3] = flashlightState.m_FarZAtten;
			pShaderAPI->SetPixelShaderConstant( PSREG_FLASHLIGHT_ATTENUATION, atten, 1 );

			pos[0] = flashlightState.m_vecLightOrigin[0];		// Set the flashlight origin
			pos[1] = flashlightState.m_vecLightOrigin[1];
			pos[2] = flashlightState.m_vecLightOrigin[2];
			pos[3] = flashlightState.m_FarZ;
			pShaderAPI->SetPixelShaderConstant( PSREG_FLASHLIGHT_POSITION_RIM_BOOST, pos, 1 );	// steps on rim boost

			pShaderAPI->SetPixelShaderConstant( PSREG_FLASHLIGHT_TO_WORLD_TEXTURE, worldToTexture.Base(), 4 );

			// Tweaks associated with a given flashlight
			tweaks[0] = ShadowFilterFromState( flashlightState );
			tweaks[1] = ShadowAttenFromState( flashlightState );
			HashShadow2DJitter( flashlightState.m_flShadowJitterSeed, &tweaks[2], &tweaks[3] );
			pShaderAPI->SetPixelShaderConstant( PSREG_ENVMAP_TINT__SHADOW_TWEAKS, tweaks, 1 );

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

			float vFlashlightPos[4];
			pShaderAPI->GetWorldSpaceCameraPosition( vFlashlightPos );
			pShaderAPI->SetPixelShaderConstant( PSREG_FLASHLIGHT_POSITION_RIM_BOOST, vFlashlightPos, 1 );

			if ( IsX360() )
			{
				pShaderAPI->SetBooleanPixelShaderConstant( 0, &flashlightState.m_nShadowQuality, 1 );
			}

#if !defined( _X360 ) && !defined( _PS3 )
			if ( !g_pHardwareConfig->HasFastVertexTextures() )
#endif
			{
				DECLARE_DYNAMIC_VERTEX_SHADER( teeth_flashlight_vs20 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
				SET_DYNAMIC_VERTEX_SHADER( teeth_flashlight_vs20 );

				if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
				{
					DECLARE_DYNAMIC_PIXEL_SHADER( teeth_flashlight_ps20b );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHTSHADOWS, bFlashlightShadows );
					SET_DYNAMIC_PIXEL_SHADER( teeth_flashlight_ps20b );
				}
				else
				{
					DECLARE_DYNAMIC_PIXEL_SHADER( teeth_flashlight_ps20 );
					SET_DYNAMIC_PIXEL_SHADER( teeth_flashlight_ps20 );
				}
			}
#if !defined( _X360 ) && !defined( _PS3 )
			else
			{
				SetHWMorphVertexShaderState( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6, VERTEX_SHADER_SHADER_SPECIFIC_CONST_7, SHADER_VERTEXTEXTURE_SAMPLER0 );

				DECLARE_DYNAMIC_VERTEX_SHADER( teeth_flashlight_vs30 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
				SET_DYNAMIC_VERTEX_SHADER( teeth_flashlight_vs30 );

				DECLARE_DYNAMIC_PIXEL_SHADER( teeth_flashlight_ps30 );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHTSHADOWS, bFlashlightShadows );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( UBERLIGHT, flashlightState.m_bUberlight );
				SET_DYNAMIC_PIXEL_SHADER( teeth_flashlight_ps30 );

				SetupUberlightFromState( pShaderAPI, flashlightState );
			}
#endif
		}
		Draw();
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			SET_FLAGS2( MATERIAL_VAR2_LIGHTING_VERTEX_LIT );
		}
		bool hasFlashlight = UsingFlashlight( params );
		if ( !hasFlashlight || ( IsX360() || IsPS3() ) )
		{
			DrawUsingVertexShader( params, pShaderAPI, pShaderShadow, vertexCompression );
			SHADOW_STATE
			{
				SetInitialShadowState();
			}
		}
		if( hasFlashlight )
		{
			DrawFlashlight( params, pShaderAPI, pShaderShadow, vertexCompression );
		}
	}
END_SHADER
