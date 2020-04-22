//========= Copyright (c) 1996-2007, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "BaseVSShader.h"

#if !defined( _GAMECONSOLE )
#include "engine_post_vs30.inc"
#include "engine_post_ps30.inc"
#endif

#include "engine_post_vs20.inc"
#include "engine_post_ps20.inc"
#include "engine_post_ps20b.inc"

#include "../materialsystem_global.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

ConVar mat_screen_blur_override( "mat_screen_blur_override", "-1.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar mat_depth_blur_focal_distance_override( "mat_depth_blur_focal_distance_override", "-1.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar mat_depth_blur_strength_override( "mat_depth_blur_strength_override", "-1.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar mat_grain_scale_override( "mat_grain_scale_override", "-1.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar mat_local_contrast_scale_override( "mat_local_contrast_scale_override", "0.0", FCVAR_CHEAT );
ConVar mat_local_contrast_midtone_mask_override( "mat_local_contrast_midtone_mask_override", "-1.0", FCVAR_CHEAT );
ConVar mat_local_contrast_vignette_start_override( "mat_local_contrast_vignette_start_override", "-1.0", FCVAR_CHEAT );
ConVar mat_local_contrast_vignette_end_override( "mat_local_contrast_vignette_end_override", "-1.0", FCVAR_CHEAT );
ConVar mat_local_contrast_edge_scale_override( "mat_local_contrast_edge_scale_override", "-1000.0", FCVAR_CHEAT );
ConVar mat_vignette_enable( "mat_vignette_enable", "1", FCVAR_REPLICATED );
ConVar mat_noise_enable( "mat_noise_enable", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );


DEFINE_FALLBACK_SHADER( Engine_Post, Engine_Post_dx9 )
BEGIN_VS_SHADER_FLAGS( Engine_Post_dx9, "Engine post-processing effects (software anti-aliasing, bloom, color-correction", SHADER_NOT_EDITABLE )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( FBTEXTURE,				SHADER_PARAM_TYPE_TEXTURE,	"_rt_FullFrameFB",	"Full framebuffer texture" )
		SHADER_PARAM( AAENABLE,					SHADER_PARAM_TYPE_BOOL,		"0",				"Enable software anti-aliasing" )
		SHADER_PARAM( FXAAINTERNALC,			SHADER_PARAM_TYPE_VEC4,		"[0 0 0 0]",		"Internal fxaa Console values set via material proxy" )
		SHADER_PARAM( FXAAINTERNALQ,			SHADER_PARAM_TYPE_VEC4,		"[0 0 0 0]",		"Internal fxaa Quality values set via material proxy" )
		SHADER_PARAM( AAINTERNAL1,				SHADER_PARAM_TYPE_VEC4,		"[0 0 0 0]",		"Internal anti-aliasing values set via material proxy" )
		SHADER_PARAM( AAINTERNAL2,				SHADER_PARAM_TYPE_VEC4,		"[0 0 0 0]",		"Internal anti-aliasing values set via material proxy" )
		SHADER_PARAM( AAINTERNAL3,				SHADER_PARAM_TYPE_VEC4,		"[0 0 0 0]",		"Internal anti-aliasing values set via material proxy" )
		SHADER_PARAM( BLOOMENABLE,				SHADER_PARAM_TYPE_BOOL,		"1",				"Enable bloom" )
		SHADER_PARAM( BLOOMAMOUNT,				SHADER_PARAM_TYPE_FLOAT,	"1.0",				"Bloom scale factor" )
		SHADER_PARAM( SCREENEFFECTTEXTURE,		SHADER_PARAM_TYPE_TEXTURE,	"",					"used for paint or vomit screen effect" )
		SHADER_PARAM( DEPTHBLURENABLE,			SHADER_PARAM_TYPE_BOOL,		"0",				"Inexpensive depth-of-field substitute" )

		SHADER_PARAM( ALLOWVIGNETTE,			SHADER_PARAM_TYPE_BOOL,		"0",				"Allow vignette" )
		SHADER_PARAM( VIGNETTEENABLE,			SHADER_PARAM_TYPE_BOOL,		"0",				"Enable vignette" )
		SHADER_PARAM( INTERNAL_VIGNETTETEXTURE,	SHADER_PARAM_TYPE_TEXTURE,	"dev/vignette",		"" )

		SHADER_PARAM( ALLOWNOISE,				SHADER_PARAM_TYPE_BOOL,		"0",				"Allow noise" )
		SHADER_PARAM( NOISEENABLE,				SHADER_PARAM_TYPE_BOOL,		"0",				"Enable noise" )
		SHADER_PARAM( NOISESCALE,				SHADER_PARAM_TYPE_FLOAT,	"0",				"Noise scale" )
		SHADER_PARAM( NOISETEXTURE,				SHADER_PARAM_TYPE_TEXTURE,	"",					"Noise texture" )

		SHADER_PARAM( ALLOWLOCALCONTRAST,		SHADER_PARAM_TYPE_BOOL,		"0",				"Enable local contrast enhancement" )
		SHADER_PARAM( LOCALCONTRASTENABLE,		SHADER_PARAM_TYPE_BOOL,		"0",				"Enable local contrast enhancement" )
		SHADER_PARAM( LOCALCONTRASTSCALE,		SHADER_PARAM_TYPE_FLOAT,	"0",				"Local contrast scale" )
		SHADER_PARAM( LOCALCONTRASTMIDTONEMASK,	SHADER_PARAM_TYPE_FLOAT,	"0",				"Local contrast midtone mask" )
		SHADER_PARAM( LOCALCONTRASTVIGNETTESTART,	SHADER_PARAM_TYPE_BOOL,	"0",				"Enable local contrast enhancement" )
		SHADER_PARAM( LOCALCONTRASTVIGNETTEEND,	SHADER_PARAM_TYPE_FLOAT,	"0",				"Local contrast scale" )
		SHADER_PARAM( LOCALCONTRASTEDGESCALE,	SHADER_PARAM_TYPE_FLOAT,	"0",				"Local contrast midtone mask" )

		SHADER_PARAM( BLURREDVIGNETTEENABLE,	SHADER_PARAM_TYPE_BOOL,		"0",				"Enable blurred vignette" )
		SHADER_PARAM( BLURREDVIGNETTESCALE,		SHADER_PARAM_TYPE_FLOAT,	"0",				"blurred vignette strength" )
		SHADER_PARAM( FADETOBLACKSCALE,			SHADER_PARAM_TYPE_FLOAT,	"0",				"fade strength" )

		SHADER_PARAM( DEPTHBLURFOCALDISTANCE,	SHADER_PARAM_TYPE_FLOAT,	"0",				"Distance in dest-alpha space [0,1] of focal plane." )
		SHADER_PARAM( DEPTHBLURSTRENGTH,		SHADER_PARAM_TYPE_FLOAT,	"0",				"Strength of depth-blur effect" )
		SHADER_PARAM( SCREENBLURSTRENGTH,		SHADER_PARAM_TYPE_FLOAT,	"0",				"Full-screen blur factor" )

		SHADER_PARAM( VOMITCOLOR1,				SHADER_PARAM_TYPE_VEC3,		"[0 0 0 0]",		"1st vomit blend color" )
		SHADER_PARAM( VOMITCOLOR2,				SHADER_PARAM_TYPE_VEC3,		"[0 0 0 0]",		"2st vomit blend color" )
		SHADER_PARAM( VOMITREFRACTSCALE,		SHADER_PARAM_TYPE_FLOAT,	"0.15",				"vomit refract strength" )
		SHADER_PARAM( VOMITENABLE,				SHADER_PARAM_TYPE_BOOL,		"0",				"Enable vomit refract" )

		SHADER_PARAM( FADECOLOR,				SHADER_PARAM_TYPE_VEC4,		"[0 0 0 0]",		"viewfade color" )
		SHADER_PARAM( FADE,						SHADER_PARAM_TYPE_INTEGER,	"0",				"fade type. 0 = off, 1 = lerp, 2 = modulate" )

		SHADER_PARAM( TV_GAMMA,					SHADER_PARAM_TYPE_INTEGER,	"0",				"0 default, 1 used for laying off 360 movies" )
		SHADER_PARAM( DESATURATEENABLE,			SHADER_PARAM_TYPE_INTEGER,	"0",				"Desaturate with math, turns off color correction" )
		SHADER_PARAM( DESATURATION,				SHADER_PARAM_TYPE_FLOAT,	"0",				"Desaturation Amount" )

		// Tool color correction setup
		SHADER_PARAM( TOOLMODE,					SHADER_PARAM_TYPE_BOOL,		"1",				"tool mode" )
		SHADER_PARAM( TOOLCOLORCORRECTION,		SHADER_PARAM_TYPE_FLOAT,	"1",				"tool color correction override" )
		SHADER_PARAM( WEIGHT_DEFAULT,			SHADER_PARAM_TYPE_FLOAT,	"1",				"weight default" )
		SHADER_PARAM( WEIGHT0,					SHADER_PARAM_TYPE_FLOAT,	"1",				"weight0" )
		SHADER_PARAM( WEIGHT1,					SHADER_PARAM_TYPE_FLOAT,	"1",				"weight1" )
		SHADER_PARAM( WEIGHT2,					SHADER_PARAM_TYPE_FLOAT,	"1",				"weight2" )
		SHADER_PARAM( WEIGHT3,					SHADER_PARAM_TYPE_FLOAT,	"1",				"weight3" )
		SHADER_PARAM( NUM_LOOKUPS,				SHADER_PARAM_TYPE_FLOAT,	"0",				"num_lookups" )
		SHADER_PARAM( TOOLTIME,					SHADER_PARAM_TYPE_FLOAT,	"0",				"tooltime" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		if ( !params[ INTERNAL_VIGNETTETEXTURE ]->IsDefined() )
		{
			params[ INTERNAL_VIGNETTETEXTURE ]->SetStringValue( "dev/vignette" );
		}
		if ( !params[ AAENABLE ]->IsDefined() )
		{
			params[ AAENABLE ]->SetIntValue( 0 );
		}
		if ( !params[ FXAAINTERNALC ]->IsDefined() )
		{
			params[ FXAAINTERNALC ]->SetVecValue( 0, 0, 0, 0 );
		}
		if ( !params[ FXAAINTERNALQ ]->IsDefined() )
		{
			params[ FXAAINTERNALQ ]->SetVecValue( 0, 0, 0, 0 );
		}
		if ( !params[ AAINTERNAL1 ]->IsDefined() )
		{
			params[ AAINTERNAL1 ]->SetVecValue( 0, 0, 0, 0 );
		}
		if ( !params[ AAINTERNAL2 ]->IsDefined() )
		{
			params[ AAINTERNAL2 ]->SetVecValue( 0, 0, 0, 0 );
		}
		if ( !params[ AAINTERNAL3 ]->IsDefined() )
		{
			params[ AAINTERNAL3 ]->SetVecValue( 0, 0, 0, 0 );
		}
		if ( !params[ BLOOMENABLE ]->IsDefined() )
		{
			params[ BLOOMENABLE ]->SetIntValue( 1 );
		}
		if ( !params[ BLOOMAMOUNT ]->IsDefined() )
		{
			params[ BLOOMAMOUNT ]->SetFloatValue( 1.0f );
		}
		if ( !params[ DEPTHBLURENABLE ]->IsDefined() )
		{
			params[ DEPTHBLURENABLE ]->SetIntValue( 0 );
		}
		if ( !params[ ALLOWNOISE ]->IsDefined() )
		{
			params[ ALLOWNOISE ]->SetIntValue( 1 );
		}
		if ( !params[ NOISESCALE ]->IsDefined() )
		{
			params[ NOISESCALE ]->SetFloatValue( 1.0f );
		}
		if ( !params[ NOISEENABLE ]->IsDefined() )
		{
			params[ NOISEENABLE ]->SetIntValue( 0 );
		}
		if ( !params[ ALLOWVIGNETTE ]->IsDefined() )
		{
			params[ ALLOWVIGNETTE ]->SetIntValue( 1 );
		}
		if ( !params[ VIGNETTEENABLE ]->IsDefined() )
		{
			params[ VIGNETTEENABLE ]->SetIntValue( 0 );
		}
		if ( !params[ ALLOWLOCALCONTRAST ]->IsDefined() )
		{
			params[ ALLOWLOCALCONTRAST ]->SetIntValue( 1 );
		}
		if ( !params[ LOCALCONTRASTSCALE ]->IsDefined() )
		{
			params[ LOCALCONTRASTSCALE ]->SetFloatValue( 1.0f );
		}
		if ( !params[ LOCALCONTRASTMIDTONEMASK ]->IsDefined() )
		{
			params[ LOCALCONTRASTMIDTONEMASK ]->SetFloatValue( 1000.0f );
		}
		if ( !params[ LOCALCONTRASTENABLE ]->IsDefined() )
		{
			params[ LOCALCONTRASTENABLE ]->SetIntValue( 0 );
		}
		if ( !params[ LOCALCONTRASTVIGNETTESTART ]->IsDefined() )
		{
			params[ LOCALCONTRASTVIGNETTESTART ]->SetFloatValue( 0.7f );
		}
		if ( !params[ LOCALCONTRASTVIGNETTEEND ]->IsDefined() )
		{
			params[ LOCALCONTRASTVIGNETTEEND ]->SetFloatValue( 1.0f );
		}
		if ( !params[ LOCALCONTRASTEDGESCALE ]->IsDefined() )
		{
			params[ LOCALCONTRASTEDGESCALE ]->SetFloatValue( 0.0f );
		}
		if ( !params[ BLURREDVIGNETTEENABLE ]->IsDefined() )
		{
			params[ BLURREDVIGNETTEENABLE ]->SetIntValue( 0 );
		}
		if ( !params[ BLURREDVIGNETTESCALE ]->IsDefined() )
		{
			params[ BLURREDVIGNETTESCALE ]->SetFloatValue( 0.0f );
		}
		if ( !params[ FADETOBLACKSCALE ]->IsDefined() )
		{
			params[ FADETOBLACKSCALE ]->SetFloatValue( 0.0f );
		}
		if ( !params[ DEPTHBLURFOCALDISTANCE ]->IsDefined() )
		{
			params[ DEPTHBLURFOCALDISTANCE ]->SetFloatValue( 0.0f );
		}
		if ( !params[ DEPTHBLURSTRENGTH ]->IsDefined() )
		{
			params[ DEPTHBLURSTRENGTH ]->SetFloatValue( 0.0f );
		}
		if ( !params[ SCREENBLURSTRENGTH ]->IsDefined() )
		{
			params[ SCREENBLURSTRENGTH ]->SetFloatValue( 0.0f );
		}
		if ( !params[ TOOLMODE ]->IsDefined() )
		{
			params[ TOOLMODE ]->SetIntValue( 0 );
		}
		if ( !params[ TOOLCOLORCORRECTION ]->IsDefined() )
		{
			params[ TOOLCOLORCORRECTION ]->SetFloatValue( 0.0f );
		}
		if ( !params[ VOMITENABLE ]->IsDefined() )
		{
			params[ VOMITENABLE ]->SetIntValue( 0 );
		}
		if ( !params[ VOMITREFRACTSCALE ]->IsDefined() )
		{
			params[ VOMITREFRACTSCALE ]->SetFloatValue( 0.15f );
		}
		if ( !params[ VOMITCOLOR1 ]->IsDefined() )
		{
			params[ VOMITCOLOR1 ]->SetVecValue( 1.0, 1.0, 0.0 );
		}
		if ( !params[ VOMITCOLOR2 ]->IsDefined() )
		{
			params[ VOMITCOLOR2 ]->SetVecValue( 0.0, 1.0, 0.0 );
		}
		if ( !params[ FADE ]->IsDefined() )
		{
			params[ FADE ]->SetIntValue( 0 );
		}
		if ( !params[ FADECOLOR ]->IsDefined() )
		{
			params[ FADECOLOR ]->SetVecValue( 0.0f, 0.0f, 0.0f, 0.0f );
		}
		if ( !params[ TV_GAMMA ]->IsDefined() )
		{
			params[ TV_GAMMA ]->SetIntValue( 0 );
		}
		if ( !params[ DESATURATEENABLE ]->IsDefined() )
		{
			params[ DESATURATEENABLE ]->SetIntValue( 0 );
		}
		if ( !params[ DESATURATION ]->IsDefined() )
		{
			params[ DESATURATION ]->SetFloatValue( 0.0f );
		}

		SET_FLAGS2( MATERIAL_VAR2_NEEDS_FULL_FRAME_BUFFER_TEXTURE );
	}

	SHADER_FALLBACK
	{
		// This shader should not be *used* unless we're >= DX9  (bloomadd.vmt/screenspace_general_dx8 should be used for DX8)
		return 0;
	}

	SHADER_INIT
	{
		if ( params[BASETEXTURE]->IsDefined() )
		{
			LoadTexture( BASETEXTURE );
		}

		if ( params[FBTEXTURE]->IsDefined() )
		{
			LoadTexture( FBTEXTURE );
		}

		if ( params[SCREENEFFECTTEXTURE]->IsDefined() )
		{
			LoadTexture( SCREENEFFECTTEXTURE );
		}

		if ( params[NOISETEXTURE]->IsDefined() )
		{
			LoadTexture( NOISETEXTURE );
		}

		if ( params[INTERNAL_VIGNETTETEXTURE]->IsDefined() )
		{
			LoadTexture( INTERNAL_VIGNETTETEXTURE );
		}
	}

	SHADER_DRAW
	{
		bool bSFM = ( ToolsEnabled() && IsPlatformWindowsPC() && g_pHardwareConfig->SupportsPixelShaders_3_0() ) ? true : false;
		bSFM;
		bool bToolMode = params[TOOLMODE]->GetIntValue() != 0;
		bool bDepthBlurEnable = params[ DEPTHBLURENABLE ]->GetIntValue() != 0;
		bool bForceSRGBReadsAndWrites = false;

		SHADOW_STATE
		{
			// This shader uses opaque blending, but needs to match the behaviour of bloom_add/screen_spacegeneral,
			// which uses additive blending (and is used when bloom is enabled but col-correction and AA are not).
			//   BUT!
			// Hardware sRGB blending is incorrect (on pre-DX10 cards, sRGB values are added directly).
			//   SO...
			// When doing the bloom addition in the pixel shader, we need to emulate that incorrect
			// behaviour - by turning sRGB read OFF for the FB texture and by turning sRGB write OFF
			// (which is fine, since the AA process works better on an sRGB framebuffer than a linear
			// one; gamma colours more closely match luminance perception. The color-correction process
			// has always taken gamma-space values as input anyway).

			// On OpenGL, we MUST do sRGB reads from the bloom and full framebuffer textures AND sRGB
			// writes on the way out to the framebuffer.  Hence, our colors are linear in the shader.
			// Given this, we use the LINEAR_INPUTS combo to convert to sRGB for the purposes of color
			// correction, since that is how the color correction textures are authored.
			bool bLinearInput = false;
			bool bLinearOutput = false;


			pShaderShadow->EnableBlending( false );

			// The (sRGB) bloom texture is bound to sampler 0
			pShaderShadow->EnableTexture(  SHADER_SAMPLER0, true  );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, bForceSRGBReadsAndWrites );
			pShaderShadow->EnableSRGBWrite( bForceSRGBReadsAndWrites );

			// The (sRGB) full framebuffer texture is bound to sampler 1:
			pShaderShadow->EnableTexture(  SHADER_SAMPLER1, true  );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, bForceSRGBReadsAndWrites );

			// Up to 4 (sRGB) color-correction lookup textures are bound to samplers 2-5:
			int nLookupCount = MIN( (int) params[NUM_LOOKUPS]->GetFloatValue(), 3 );
			for( int i = 0 ; i < nLookupCount; i++ )
			{
				pShaderShadow->EnableTexture(  ( Sampler_t ) ( SHADER_SAMPLER2 + i ), true );
				pShaderShadow->EnableSRGBRead( ( Sampler_t ) ( SHADER_SAMPLER2 + i ), false );
			}

			// Noise
			pShaderShadow->EnableTexture(  SHADER_SAMPLER6, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER6, false );

			// Vignette
			pShaderShadow->EnableTexture(  SHADER_SAMPLER7, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER7, false );

			// Screen effect texture
			pShaderShadow->EnableTexture(  SHADER_SAMPLER8, true  );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER8, false );

			int		format				= VERTEX_POSITION;
			int		numTexCoords		= 1;
			int *	pTexCoordDimensions	= NULL;
			int		userDataSize		= 0;
			pShaderShadow->VertexShaderVertexFormat( format, numTexCoords, pTexCoordDimensions, userDataSize );

#if !defined( _GAMECONSOLE )
			if( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_STATIC_VERTEX_SHADER( engine_post_vs30 );
				SET_STATIC_VERTEX_SHADER( engine_post_vs30 );
			}
			else
#endif
			{
				DECLARE_STATIC_VERTEX_SHADER( engine_post_vs20 );
				SET_STATIC_VERTEX_SHADER( engine_post_vs20 );
			}

#if !defined( _GAMECONSOLE )
			if( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_STATIC_PIXEL_SHADER( engine_post_ps30 );
				SET_STATIC_PIXEL_SHADER_COMBO( TOOL_MODE, bToolMode );
				SET_STATIC_PIXEL_SHADER_COMBO( DEPTH_BLUR_ENABLE, bDepthBlurEnable );
				SET_STATIC_PIXEL_SHADER_COMBO( LINEAR_INPUT,  bLinearInput );
				SET_STATIC_PIXEL_SHADER_COMBO( LINEAR_OUTPUT, bLinearOutput );
				SET_STATIC_PIXEL_SHADER( engine_post_ps30 );
			}
			else 
#endif
			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( engine_post_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( TOOL_MODE, bToolMode );
				SET_STATIC_PIXEL_SHADER_COMBO( DEPTH_BLUR_ENABLE, bDepthBlurEnable );
				SET_STATIC_PIXEL_SHADER_COMBO( LINEAR_INPUT,  bLinearInput );
				SET_STATIC_PIXEL_SHADER_COMBO( LINEAR_OUTPUT, bLinearOutput );
				SET_STATIC_PIXEL_SHADER( engine_post_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( engine_post_ps20 );
				SET_STATIC_PIXEL_SHADER_COMBO( TOOL_MODE, bToolMode );
				SET_STATIC_PIXEL_SHADER_COMBO( DEPTH_BLUR_ENABLE, bDepthBlurEnable );
				SET_STATIC_PIXEL_SHADER( engine_post_ps20 );
			}
		}
		DYNAMIC_STATE
		{
			BindTexture( SHADER_SAMPLER0, bForceSRGBReadsAndWrites ? TEXTURE_BINDFLAGS_SRGBREAD : TEXTURE_BINDFLAGS_NONE, BASETEXTURE, -1 );

			// FIXME: need to set FBTEXTURE to be point-sampled (will speed up this shader significantly on 360)
			//        and assert that it's set to SHADER_TEXWRAPMODE_CLAMP (since the shader will sample offscreen)
			BindTexture( SHADER_SAMPLER1, bForceSRGBReadsAndWrites ? TEXTURE_BINDFLAGS_SRGBREAD : TEXTURE_BINDFLAGS_NONE, FBTEXTURE,   -1 );

			ShaderColorCorrectionInfo_t ccInfo = { false, 0, 1.0f, { 1.0f, 1.0f, 1.0f, 1.0f } };

			float flTime;
			if ( bToolMode )
			{
				flTime = params[TOOLTIME]->GetFloatValue();
			}
			else
			{
				flTime = pShaderAPI->CurrentTime();
			}

			// ps20b has a desaturation control that overrides color correction, only used by the SFM (tools mode on Windows)
			bool bDesaturateEnable = bToolMode && ( params[DESATURATEENABLE]->GetIntValue() != 0 ) && g_pHardwareConfig->SupportsPixelShaders_2_b() && IsPlatformWindows();
			
			if ( params[FADE]->GetIntValue() == 0 )
			{
				// Not fading, so set the constant to cause nothing to change about the pixel color
				float vConst[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				pShaderAPI->SetPixelShaderConstant( 15, vConst );
			}
			else
			{
				pShaderAPI->SetPixelShaderConstant( 15, params[ FADECOLOR ]->GetVecValue(), 1 );
			}
			
			if ( bDesaturateEnable )
			{
				float vPsConst[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				vPsConst[0] = params[DESATURATION]->GetFloatValue();
				pShaderAPI->SetPixelShaderConstant( 16, vPsConst, 1 );
			}
			else // set up color correction
			{
				bool bToolColorCorrection = params[TOOLCOLORCORRECTION]->GetIntValue() != 0;
				if ( bToolColorCorrection )
				{
					ccInfo.m_bIsEnabled = true;

					ccInfo.m_nLookupCount = (int) params[NUM_LOOKUPS]->GetFloatValue();
					ccInfo.m_flDefaultWeight = params[WEIGHT_DEFAULT]->GetFloatValue();
					ccInfo.m_pLookupWeights[0] = params[WEIGHT0]->GetFloatValue();
					ccInfo.m_pLookupWeights[1] = params[WEIGHT1]->GetFloatValue();
					ccInfo.m_pLookupWeights[2] = params[WEIGHT2]->GetFloatValue();
					ccInfo.m_pLookupWeights[3] = params[WEIGHT3]->GetFloatValue();
				}
				else
				{
					pShaderAPI->GetCurrentColorCorrection( &ccInfo );
				}
			}

			int colCorrectNumLookups = MIN( ccInfo.m_nLookupCount, 3 );
			if ( colCorrectNumLookups )
			{
				int i;
				for ( i = 0; i < colCorrectNumLookups; i++ )
				{
					StandardTextureId_t id = (StandardTextureId_t)(TEXTURE_COLOR_CORRECTION_VOLUME_0 + i);

					bool bIsTextureValid = pShaderAPI->IsStandardTextureHandleValid( id );
					if ( !bIsTextureValid )
					{
						// Texture isn't valid - give up now (and set the COL_CORRECT_NUM_LOOKUPS combo appropriately) otherwise GL mode will be unhappy
						break;
					}
				
					pShaderAPI->BindStandardTexture( (Sampler_t)(SHADER_SAMPLER2 + i), TEXTURE_BINDFLAGS_NONE, id );
				}
				colCorrectNumLookups = i;
			}

			// Upload 1-pixel X&Y offsets [ (+dX,0,+dY,-dX) is chosen to work with the allowed ps20 swizzles ]
			// The shader will sample in a cross (up/down/left/right from the current sample), for 5-tap
			// (quality 0) mode and add another 4 samples in a diagonal cross, for 9-tap (quality 1) mode
			ITexture * pTarget	= params[FBTEXTURE]->GetTextureValue();
			int		width		= pTarget->GetActualWidth();
			int		height		= pTarget->GetActualHeight();
			float	dX			= 1.0f / width;
			float	dY			= 1.0f / height;
			float	offsets[4]	= { +dX, 0, +dY, -dX };
			pShaderAPI->SetPixelShaderConstant( 0, &offsets[0], 1 );

			// Upload AA tweakables:
			//   x - strength (this can be used to toggle the AA off, or to weaken it where pathological cases are showing)
			//   y - reduction of 1-pixel-line blurring (blurring of 1-pixel lines causes issues, so it's tunable)
			//   z - edge threshold multiplier (default 1.0, < 1.0 => more edges softened, > 1.0 => fewer edges softened)
			//   w - tap offset multiplier (default 1.0, < 1.0 => sharper image, > 1.0 => blurrier image)
			float	tweakables[4] = {	params[ AAINTERNAL1 ]->GetVecValue()[0],
										params[ AAINTERNAL1 ]->GetVecValue()[1],
										params[ AAINTERNAL3 ]->GetVecValue()[0],
										params[ AAINTERNAL3 ]->GetVecValue()[1] };
			pShaderAPI->SetPixelShaderConstant( 1, &tweakables[0], 1 );

			// Upload AA UV transform (converts bloom texture UVs to framebuffer texture UVs)
			float	uvTrans[4] = {	params[ AAINTERNAL2 ]->GetVecValue()[0],
									params[ AAINTERNAL2 ]->GetVecValue()[1],
									params[ AAINTERNAL2 ]->GetVecValue()[2],
									params[ AAINTERNAL2 ]->GetVecValue()[3] };
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, &uvTrans[0], 1 );


			// Upload FXAA constants
			// ensure indexes match those in engine_post_ps2x.fxc
			// refer to fxaa3_11_fxc.h for parameter documentation 
	
			//
			// FXAA Console
			//

			float	uvTrans_UpperLeftLowerRight[4] = { -0.5f*dX, -0.5f*dY, 0.5f*dX, 0.5f*dY };
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_1, &uvTrans_UpperLeftLowerRight[0], 1 );

			float   N = params[ FXAAINTERNALC ]->GetVecValue()[0];
			float	fxaaConsoleRcpFrameOpt[4] = { -N*dX, -N*dY, N*dX, N*dY };
			pShaderAPI->SetPixelShaderConstant( 17, &fxaaConsoleRcpFrameOpt[0], 1 );

			float   fxaaConsoleRcpFrameOpt2[4] = { -2.0f*dX, -2.0f*dY, 2.0f*dX, 2.0f*dY };
			pShaderAPI->SetPixelShaderConstant( 18, &fxaaConsoleRcpFrameOpt2[0], 1 );

			float   fxaaConsole360RcpFrameOpt2[4] = { 8.0f*dX, 8.0f*dY, -4.0f*dX, -4.0f*dY };
			pShaderAPI->SetPixelShaderConstant( 19, &fxaaConsole360RcpFrameOpt2[0], 1 );

			float   fxaaConsoleEdge[4] = { 0.0f,
										   params[ FXAAINTERNALC ]->GetVecValue()[1],
									       params[ FXAAINTERNALC ]->GetVecValue()[2],
			                               params[ FXAAINTERNALC ]->GetVecValue()[3] };
			pShaderAPI->SetPixelShaderConstant( 20, &fxaaConsoleEdge[0], 1 );

			float   fxaaConsole360ConstDir[4] = { 1.0f, -1.0f, 0.25f, -0.25f };
			pShaderAPI->SetPixelShaderConstant( 21, &fxaaConsole360ConstDir[0], 1 );

			//
			// FXAA Quality
			//

			float	fxaaQualityRcpFrame[4] = { dX, dY, 0.5f*dX, 0.5f*dY };
			pShaderAPI->SetPixelShaderConstant( 22, &fxaaQualityRcpFrame[0], 1 );

			float   fxaaQualitySubpixEdge[4] = { params[ FXAAINTERNALQ ]->GetVecValue()[0],
											     params[ FXAAINTERNALQ ]->GetVecValue()[1],
												 params[ FXAAINTERNALQ ]->GetVecValue()[2],
												 params[ FXAAINTERNALQ ]->GetVecValue()[3] };
			pShaderAPI->SetPixelShaderConstant( 23, &fxaaQualitySubpixEdge[0], 1 );


			// Upload color-correction weights:
			pShaderAPI->SetPixelShaderConstant( 3, &ccInfo.m_flDefaultWeight );
			pShaderAPI->SetPixelShaderConstant( 4, ccInfo.m_pLookupWeights );

			int aaEnabled = false;
			if ( IsGameConsole() || ( g_pHardwareConfig->SupportsPixelShaders_3_0() && !bSFM ) )
			{
				aaEnabled = ( params[ AAINTERNAL1 ]->GetVecValue()[0] != 0.0f ) ? 1 : 0;
			}
						 
			int bloomEnabled		= ( params[ BLOOMENABLE ]->GetIntValue() == 0 ) ? 0 : 1;
			int colCorrectEnabled	= ccInfo.m_bIsEnabled;

			float flBloomFactor = bloomEnabled ? 1.0f : 0.0f;
			flBloomFactor *= params[BLOOMAMOUNT]->GetFloatValue();
			float bloomConstant[4] = 
			{ 
				flBloomFactor, 
				params[ SCREENBLURSTRENGTH ]->GetFloatValue(), 
				params[ DEPTHBLURFOCALDISTANCE ]->GetFloatValue(), 
				params[ DEPTHBLURSTRENGTH ]->GetFloatValue()
			};

			if ( mat_screen_blur_override.GetFloat() >= 0.0f )
			{
				bloomConstant[1] = mat_screen_blur_override.GetFloat();
			}
			if ( mat_depth_blur_focal_distance_override.GetFloat() >= 0.0f )
			{
				bloomConstant[2] = mat_depth_blur_focal_distance_override.GetFloat();
			}
#ifdef _X360
			bloomConstant[3] = 0.0f; // Depth blur is currently broken on X360 because we're not writing out the depth scale properly
#else // !_X360
			if ( mat_depth_blur_strength_override.GetFloat() >= 0.0f )
			{
				bloomConstant[3] = mat_depth_blur_strength_override.GetFloat();
			}
#endif // _X360
			
			pShaderAPI->SetPixelShaderConstant( 5, bloomConstant );

			// Vignette
			bool bVignetteEnable = false; 
			if ( mat_vignette_enable.GetInt() )
			{
				bVignetteEnable = ( params[ VIGNETTEENABLE ]->GetIntValue() != 0 ) && ( params[ ALLOWVIGNETTE ]->GetIntValue() != 0 );
			}
			if ( bVignetteEnable )
			{
				BindTexture( SHADER_SAMPLER7, TEXTURE_BINDFLAGS_NONE, INTERNAL_VIGNETTETEXTURE );
			}

			// Noise
			bool bNoiseEnable = false;
			if ( mat_noise_enable.GetInt() )
			{
				bNoiseEnable = false; //( params[ NOISEENABLE ]->GetIntValue() != 0 ) && ( params[ ALLOWNOISE ]->GetIntValue() != 0 );
			}

			int nFbTextureHeight = params[FBTEXTURE]->GetTextureValue()->GetActualHeight();
			if ( nFbTextureHeight < 720 )
			{
				// Disable noise at low resolutions
				bNoiseEnable = false;
			}

			// Be even more draconian about disabling noise at low resolutions on Mac
			if ( ( nFbTextureHeight < 1024 ) )
			{
				bNoiseEnable = false;
			}

			if ( bNoiseEnable )
			{
				BindTexture( SHADER_SAMPLER6, TEXTURE_BINDFLAGS_NONE, NOISETEXTURE );

				// Noise scale
				float vPsConst6[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				vPsConst6[0] = params[ NOISESCALE ]->GetFloatValue();
				if ( mat_grain_scale_override.GetFloat() != -1.0f )
				{
					vPsConst6[0] = mat_grain_scale_override.GetFloat();
				}

				if ( IsX360() )
				{
					vPsConst6[0] *= 0.15f;
				}

				if ( vPsConst6[0] <= 0.0f )
				{
					bNoiseEnable = false;
				}

				pShaderAPI->SetPixelShaderConstant( 6, vPsConst6 );

				// Time % 1000 for scrolling
				float vPsConst[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				vPsConst[0] = flTime;
				vPsConst[0] -= (float)( (int)( vPsConst[0] / 1000.0f ) ) * 1000.0f;
				pShaderAPI->SetPixelShaderConstant( 7, vPsConst, 1 );
			}

			// Local Contrast
			bool bLocalContrastEnable = ( params[ LOCALCONTRASTENABLE ]->GetIntValue() != 0 ) && ( params[ ALLOWLOCALCONTRAST ]->GetIntValue() != 0 );
			bool bBlurredVignetteEnable = ( bLocalContrastEnable ) && ( params[ BLURREDVIGNETTEENABLE ]->GetIntValue() != 0 );

			if ( bLocalContrastEnable )
			{
				// Contrast scale
				float vPsConst[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				vPsConst[0] = params[ LOCALCONTRASTSCALE ]->GetFloatValue();
				if ( mat_local_contrast_scale_override.GetFloat() != 0.0f )
				{
					vPsConst[0] = mat_local_contrast_scale_override.GetFloat();
				}
				vPsConst[1] = params[ LOCALCONTRASTMIDTONEMASK ]->GetFloatValue();
				if ( mat_local_contrast_midtone_mask_override.GetFloat() >= 0.0f )
				{
					vPsConst[1] = mat_local_contrast_midtone_mask_override.GetFloat();
				}
				vPsConst[2] = params[ BLURREDVIGNETTESCALE ]->GetFloatValue();
				pShaderAPI->SetPixelShaderConstant( 8, vPsConst, 1 );

				vPsConst[0] = params[ LOCALCONTRASTVIGNETTESTART ]->GetFloatValue();
				if ( mat_local_contrast_vignette_start_override.GetFloat() >= 0.0f )
				{
					vPsConst[0] = mat_local_contrast_vignette_start_override.GetFloat();
				}
				vPsConst[1] = params[ LOCALCONTRASTVIGNETTEEND ]->GetFloatValue();
				if ( mat_local_contrast_vignette_end_override.GetFloat() >= 0.0f )
				{
					vPsConst[1] = mat_local_contrast_vignette_end_override.GetFloat();
				}
				vPsConst[2] = params[ LOCALCONTRASTEDGESCALE ]->GetFloatValue();
				if ( mat_local_contrast_edge_scale_override.GetFloat() >= -1.0f )
				{
					vPsConst[2] = mat_local_contrast_edge_scale_override.GetFloat();
				}
				pShaderAPI->SetPixelShaderConstant( 9, vPsConst, 1 );
			}

			// fade to black
			float vPsConst[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			vPsConst[0] = params[ FADETOBLACKSCALE ]->GetFloatValue();
			pShaderAPI->SetPixelShaderConstant( 10, vPsConst, 1 );

			bool bFadeToBlackEnable = vPsConst[0] > 0.0f;

			bool bVomitEnable = false; //( params[ VOMITENABLE ]->GetIntValue() != 0 );
			if ( bVomitEnable )
			{
				BindTexture( SHADER_SAMPLER8, TEXTURE_BINDFLAGS_NONE, SCREENEFFECTTEXTURE );

				params[ VOMITCOLOR1 ]->GetVecValue( vPsConst, 3 );
				vPsConst[3] = params[ VOMITREFRACTSCALE ]->GetFloatValue();
				pShaderAPI->SetPixelShaderConstant( 11, vPsConst, 1 );
				params[ VOMITCOLOR2 ]->GetVecValue( vPsConst, 3 );
				vPsConst[3] = 0.0f;
				pShaderAPI->SetPixelShaderConstant( 12, vPsConst, 1 );

				// Get viewport and render target dimensions and set shader constant to do a 2D mad
				int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
				pShaderAPI->GetCurrentViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );

				int nRtWidth, nRtHeight;
				pShaderAPI->GetCurrentRenderTargetDimensions( nRtWidth, nRtHeight );

				float vViewportMad[4];

				// screen->viewport transform
				vViewportMad[0] = ( float )nRtWidth / ( float )nViewportWidth;
				vViewportMad[1] = ( float )nRtHeight / ( float )nViewportHeight;
				vViewportMad[2] = -( float )nViewportX / ( float )nViewportWidth;
				vViewportMad[3] = -( float )nViewportY / ( float )nViewportHeight;
				pShaderAPI->SetPixelShaderConstant( 13, vViewportMad, 1 );

				// viewport->screen transform
				vViewportMad[0] = ( float )nViewportWidth / ( float )nRtWidth;
				vViewportMad[1] = ( float )nViewportHeight / ( float )nRtHeight;
				vViewportMad[2] = ( float )nViewportX / ( float )nRtWidth;
				vViewportMad[3] = ( float )nViewportY / ( float )nRtHeight;
				pShaderAPI->SetPixelShaderConstant( 14, vViewportMad, 1 );
			}


			if ( !colCorrectEnabled )
			{
				colCorrectNumLookups = 0;
			}

			bool bConvertFromLinear = bToolMode && ( g_pHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT );

			// JasonM - double check this if the SFM needs to use the engine post FX clip in main
			bool bConvertToLinear = bToolMode && bConvertFromLinear && ( g_pHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT );
			
			int nFadeType = clamp( params[FADE]->GetIntValue(), 0, 2 ); 

#if !defined( _GAMECONSOLE )
			if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( engine_post_ps30 );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( AA_ENABLE,						aaEnabled );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( COL_CORRECT_NUM_LOOKUPS,		colCorrectNumLookups );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( CONVERT_FROM_LINEAR,			bConvertFromLinear );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( CONVERT_TO_LINEAR,				bConvertToLinear );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( NOISE_ENABLE,					bNoiseEnable ); // Always zero in Portal 2
				SET_DYNAMIC_PIXEL_SHADER_COMBO( VIGNETTE_ENABLE,				bVignetteEnable ); // Always zero in Portal 2
				SET_DYNAMIC_PIXEL_SHADER_COMBO( LOCAL_CONTRAST_ENABLE,			bLocalContrastEnable ); // Always zero in Portal 2
				SET_DYNAMIC_PIXEL_SHADER_COMBO( BLURRED_VIGNETTE_ENABLE,		bBlurredVignetteEnable ); // Always zero in Portal 2
				SET_DYNAMIC_PIXEL_SHADER_COMBO( VOMIT_ENABLE,					bVomitEnable );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( FADE_TO_BLACK,					bFadeToBlackEnable );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( FADE_TYPE,						nFadeType );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( TV_GAMMA,						params[TV_GAMMA]->GetIntValue() && bToolMode ? 1 : 0 );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( DESATURATEENABLE,				bDesaturateEnable );
				SET_DYNAMIC_PIXEL_SHADER( engine_post_ps30 );
			}
			else 
#endif
			if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( engine_post_ps20b );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( AA_ENABLE,						aaEnabled );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( COL_CORRECT_NUM_LOOKUPS,		colCorrectNumLookups );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( CONVERT_FROM_LINEAR,			bConvertFromLinear );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( CONVERT_TO_LINEAR,				bConvertToLinear );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( NOISE_ENABLE,					bNoiseEnable ); // Always zero in Portal 2
				SET_DYNAMIC_PIXEL_SHADER_COMBO( VIGNETTE_ENABLE,				bVignetteEnable ); // Always zero in Portal 2
				SET_DYNAMIC_PIXEL_SHADER_COMBO( LOCAL_CONTRAST_ENABLE,			bLocalContrastEnable ); // Always zero in Portal 2
				SET_DYNAMIC_PIXEL_SHADER_COMBO( BLURRED_VIGNETTE_ENABLE,		bBlurredVignetteEnable ); // Always zero in Portal 2
				SET_DYNAMIC_PIXEL_SHADER_COMBO( VOMIT_ENABLE,					bVomitEnable );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( FADE_TO_BLACK,					bFadeToBlackEnable );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( FADE_TYPE,						nFadeType );
#if !defined( _X360 ) && !defined( _PS3 )
				SET_DYNAMIC_PIXEL_SHADER_COMBO( TV_GAMMA,						params[TV_GAMMA]->GetIntValue() && bToolMode ? 1 : 0 );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( DESATURATEENABLE,				bDesaturateEnable );
#endif
				SET_DYNAMIC_PIXEL_SHADER( engine_post_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( engine_post_ps20 );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( AA_ENABLE,						aaEnabled );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( COL_CORRECT_NUM_LOOKUPS,		colCorrectNumLookups );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( VOMIT_ENABLE,					bVomitEnable );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( FADE_TO_BLACK,					bFadeToBlackEnable );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( FADE_TYPE,						nFadeType );
				SET_DYNAMIC_PIXEL_SHADER( engine_post_ps20 );
			}

#if !defined( _GAMECONSOLE )
			if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_DYNAMIC_VERTEX_SHADER( engine_post_vs30 );
				SET_DYNAMIC_VERTEX_SHADER( engine_post_vs30 );
			}
			else
#endif
			{
				DECLARE_DYNAMIC_VERTEX_SHADER( engine_post_vs20 );
				SET_DYNAMIC_VERTEX_SHADER( engine_post_vs20 );
			}
		}
		Draw();
	}
END_SHADER
