//========= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "BaseVSShader.h"
#include "worldimposter_vs20.inc"
#include "worldimposter_ps20.inc"
#include "worldimposter_ps20b.inc"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_VS_SHADER( worldimposter, "Help for worldimposter" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( BASETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( ALBEDO, SHADER_PARAM_TYPE_TEXTURE, "", "" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
		SET_FLAGS( MATERIAL_VAR_VERTEXFOG );
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		LoadTexture( BASETEXTURE, IsX360() ? 0 : TEXTUREFLAGS_SRGB );
		LoadTexture( ALBEDO, TEXTUREFLAGS_SRGB );
	}

	SHADER_DRAW
	{
		bool bHasFlashlight = UsingFlashlight( params );
		SHADOW_STATE
		{
			// Set stream format (note that this shader supports compression)
			int flags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED;
			 // NOTE: Have to say that we want 1 texcoord here even though we don't use it or you'll get this Warning in another part of the code: 
			//		"ERROR: shader asking for a too-narrow vertex format - you will see errors if running with debug D3D DLLs!\n\tPadding the vertex format with extra texcoords"
			int nTexCoordCount = 1;
			int userDataSize = 0;
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );

			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );

			DECLARE_STATIC_VERTEX_SHADER( worldimposter_vs20 );
			SET_STATIC_VERTEX_SHADER( worldimposter_vs20 );

			ShadowFilterMode_t nShadowFilterMode = SHADOWFILTERMODE_DEFAULT;
			if ( bHasFlashlight )
			{
				nShadowFilterMode = g_pHardwareConfig->GetShadowFilterMode( true /* bForceLowQuality */ , false /* bPS30 */ );	// Based upon vendor and device dependent formats
			}

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( worldimposter_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode );
				SET_STATIC_PIXEL_SHADER( worldimposter_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( worldimposter_ps20 );
				SET_STATIC_PIXEL_SHADER( worldimposter_ps20 );
			}

			pShaderShadow->EnableSRGBWrite( true );

			if( bHasFlashlight )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );	//		 Shadow depth map
				pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );	//		 Noise map
				pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );	//[sRGB] Flashlight cookie
			}

			FogToFogColor();
		}
		DYNAMIC_STATE
		{
			bool bFlashlightShadows = false;
			if( bHasFlashlight )
			{
				VMatrix worldToTexture;
				ITexture *pFlashlightDepthTexture;
				FlashlightState_t flashlightState = pShaderAPI->GetFlashlightStateEx( worldToTexture, &pFlashlightDepthTexture );
				bFlashlightShadows = flashlightState.m_bEnableShadows;
				BindTexture( SHADER_SAMPLER4, TEXTURE_BINDFLAGS_SRGBREAD, flashlightState.m_pSpotlightTexture, flashlightState.m_nSpotlightTextureFrame );

//				SetFlashLightColorFromState( state, pShaderAPI, PSREG_FLASHLIGHT_COLOR );

				if( pFlashlightDepthTexture && g_pConfig->ShadowDepthTexture() && bFlashlightShadows )
				{
					BindTexture( SHADER_SAMPLER2, TEXTURE_BINDFLAGS_SHADOWDEPTH, pFlashlightDepthTexture );
					ShaderApiFast( pShaderAPI )->BindStandardTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, TEXTURE_SHADOW_NOISE_2D );
				}

				float atten[4], pos[4], tweaks[4];

				atten[0] = flashlightState.m_fConstantAtten;		// Set the flashlight attenuation factors
				atten[1] = flashlightState.m_fLinearAtten;
				atten[2] = flashlightState.m_fQuadraticAtten;
				atten[3] = flashlightState.m_FarZAtten;
				pShaderAPI->SetPixelShaderConstant( 0, atten, 1 );

				pos[0] = flashlightState.m_vecLightOrigin[0];		// Set the flashlight origin
				pos[1] = flashlightState.m_vecLightOrigin[1];
				pos[2] = flashlightState.m_vecLightOrigin[2];
				pos[3] = flashlightState.m_FarZ;
				pShaderAPI->SetPixelShaderConstant( 1, pos, 1 );	// steps on rim boost

				pShaderAPI->SetPixelShaderConstant( 2, worldToTexture.Base(), 4 );

				// Tweaks associated with a given flashlight
				tweaks[0] = ShadowFilterFromState( flashlightState );
				tweaks[1] = ShadowAttenFromState( flashlightState );
				HashShadow2DJitter( flashlightState.m_flShadowJitterSeed, &tweaks[2], &tweaks[3] );
				pShaderAPI->SetPixelShaderConstant( 6, tweaks, 1 );

				// Dimensions of screen, used for screen-space noise map sampling
				float vScreenScale[4] = {1280.0f / 32.0f, 720.0f / 32.0f, 0, 0};
				int nWidth, nHeight;
				pShaderAPI->GetBackBufferDimensions( nWidth, nHeight );

				int nTexWidth, nTexHeight;
				pShaderAPI->GetStandardTextureDimensions( &nTexWidth, &nTexHeight, TEXTURE_SHADOW_NOISE_2D );

				vScreenScale[0] = (float) nWidth  / nTexWidth;
				vScreenScale[1] = (float) nHeight / nTexHeight;

//				pShaderAPI->SetPixelShaderConstant( PSREG_FLASHLIGHT_SCREEN_SCALE, vScreenScale, 1 );

				if ( IsX360() )
				{
					pShaderAPI->SetBooleanPixelShaderConstant( 0, &flashlightState.m_nShadowQuality, 1 );
				}
			}

			DECLARE_DYNAMIC_VERTEX_SHADER( worldimposter_vs20 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING,  s_pShaderAPI->GetCurrentNumBones() > 0 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, ( int )vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( FLASHLIGHT, bHasFlashlight );
			SET_DYNAMIC_VERTEX_SHADER( worldimposter_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( worldimposter_ps20b );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHT, bHasFlashlight );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHTSHADOWS, bFlashlightShadows );
				SET_DYNAMIC_PIXEL_SHADER( worldimposter_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( worldimposter_ps20 );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHT, bHasFlashlight );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHTSHADOWS, bFlashlightShadows );
				SET_DYNAMIC_PIXEL_SHADER( worldimposter_ps20 );
			}
			BindTexture( SHADER_SAMPLER0, IsX360() ? TEXTURE_BINDFLAGS_NONE : TEXTURE_BINDFLAGS_SRGBREAD, BASETEXTURE, -1 );
			BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_SRGBREAD, ALBEDO, -1 );
		}
		Draw();
	}

END_SHADER


