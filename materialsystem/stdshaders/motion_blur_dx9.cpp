//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"
#include "motion_blur_vs20.inc"
#include "motion_blur_ps20.inc"
#include "motion_blur_ps20b.inc"
#include "convar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar mat_motion_blur_percent_of_screen_max( "mat_motion_blur_percent_of_screen_max", "4.0" );

DEFINE_FALLBACK_SHADER( MotionBlur, MotionBlur_dx9 )
BEGIN_VS_SHADER_FLAGS( MotionBlur_dx9, "Motion Blur", SHADER_NOT_EDITABLE )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( MOTIONBLURINTERNAL, SHADER_PARAM_TYPE_VEC4, "[0 0 0 0]", "Internal motion blur value set by proxy" )
		SHADER_PARAM( MOTIONBLURVIEWPORTINTERNAL, SHADER_PARAM_TYPE_VEC4, "[0 0 0 0]", "Internal motion blur value set by proxy" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		if ( params[BASETEXTURE]->IsDefined() )
		{
			LoadTexture( BASETEXTURE, IsOSX() && g_pHardwareConfig->CanDoSRGBReadFromRTs() ? TEXTUREFLAGS_SRGB : 0 );
		}
	}

	SHADER_DRAW
	{
		bool bForceSRGBReadsAndWrites = IsOSXOpenGL() && g_pHardwareConfig->CanDoSRGBReadFromRTs();
		SHADOW_STATE
		{
			pShaderShadow->VertexShaderVertexFormat( VERTEX_POSITION, 1, 0, 0 );

			// On OSX OpenGL, we must do sRGB reads and writes since these render targets are tagged as such
			bool bForceSRGBReadsAndWrites = IsOSX() && g_pHardwareConfig->CanDoSRGBReadFromRTs();
			
			// NOTE: sRGB is disabled because of the NV8800 brokenness
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, bForceSRGBReadsAndWrites );
			pShaderShadow->EnableSRGBWrite( bForceSRGBReadsAndWrites );

			DECLARE_STATIC_VERTEX_SHADER( motion_blur_vs20 );
			SET_STATIC_VERTEX_SHADER( motion_blur_vs20 );

			if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( motion_blur_ps20b );
				SET_STATIC_PIXEL_SHADER( motion_blur_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( motion_blur_ps20 );
				SET_STATIC_PIXEL_SHADER( motion_blur_ps20 );
			}

			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableAlphaWrites( false );
		}

		DYNAMIC_STATE
		{
			DECLARE_DYNAMIC_VERTEX_SHADER( motion_blur_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( motion_blur_vs20 );

			// Bind textures
			BindTexture( SHADER_SAMPLER0, bForceSRGBReadsAndWrites ? TEXTURE_BINDFLAGS_SRGBREAD : TEXTURE_BINDFLAGS_NONE, BASETEXTURE );

			// Get texture dimensions
			ITexture *src_texture = params[BASETEXTURE]->GetTextureValue();
			//int flTextureWidth = src_texture->GetActualWidth();
			int flTextureHeight = src_texture->GetActualHeight();

			// Percent of screen clamp
			float vConst[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			vConst[0] = mat_motion_blur_percent_of_screen_max.GetFloat() / 100.0f;
			const float *pBlurViewportInternal = params[MOTIONBLURVIEWPORTINTERNAL]->GetVecValue();
			if ( ( pBlurViewportInternal[0] > 0.0f ) || ( pBlurViewportInternal[1] > 0.0f ) || ( pBlurViewportInternal[2] < 1.0f ) || ( pBlurViewportInternal[3] < 1.0f ) )
			{
				vConst[0] *= 0.25f; // Reduce the max if we're not rendering full screen (hack to dumb down motion blur in split screen)
			}
			pShaderAPI->SetPixelShaderConstant( 0, vConst, 1 );

			// Set values from material proxy
			pShaderAPI->SetPixelShaderConstant( 1, params[MOTIONBLURINTERNAL]->GetVecValue(), 1 );
			pShaderAPI->SetPixelShaderConstant( 2, params[MOTIONBLURVIEWPORTINTERNAL]->GetVecValue(), 1 );

			float flApproximateBlurLength = fabs( params[MOTIONBLURINTERNAL]->GetVecValue()[0] ) + fabs( params[MOTIONBLURINTERNAL]->GetVecValue()[1] ) +
											fabs( params[MOTIONBLURINTERNAL]->GetVecValue()[2] ) + fabs( params[MOTIONBLURINTERNAL]->GetVecValue()[3] );

			// Quality based on screen resolution height
			int nNumBlurSamples = 6;
			if ( flTextureHeight >= 1080 ) // 1080p and higher
				nNumBlurSamples = 14;
			else if ( flTextureHeight >= 720 ) // 720p to 1080p
				nNumBlurSamples = 10;
			else // Lower resolution than 720p
				nNumBlurSamples = 6;

			// If we are blurring less than half the allowed blur max, use fewer samples
			float flPercentMaxBlur = clamp( 2.0f * flApproximateBlurLength / MIN( mat_motion_blur_percent_of_screen_max.GetFloat() / 100.0f, 4.0f ), 0.0f, 1.0f );
			nNumBlurSamples = ( int )( ( float )nNumBlurSamples * flPercentMaxBlur );
			if ( nNumBlurSamples < 1 )
				nNumBlurSamples = 1;

			if ( flApproximateBlurLength == 0.0f )
			{
				// No motion blur this frame, so force 0 blur samples. This will cause the shader to do a single unblurred fetch.
				nNumBlurSamples = 0;
			}

			nNumBlurSamples = clamp( nNumBlurSamples, 0, 14 );

			if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( motion_blur_ps20b );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( D_NUM_BLUR_SAMPLES, nNumBlurSamples );
				SET_DYNAMIC_PIXEL_SHADER( motion_blur_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( motion_blur_ps20 );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( D_NUM_BLUR_SAMPLES, nNumBlurSamples );
				SET_DYNAMIC_PIXEL_SHADER( motion_blur_ps20 );
			}
		}

		Draw();
	}
END_SHADER
