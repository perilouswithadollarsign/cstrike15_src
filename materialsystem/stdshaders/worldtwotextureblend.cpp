//===== Copyright � 1996-2007, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"

#include "convar.h"

#if !defined( _X360 ) && !defined( _PS3 )
#include "lightmappedgeneric_vs30.inc"
#include "worldtwotextureblend_ps30.inc"
#endif

#include "lightmappedgeneric_vs20.inc"
#include "worldtwotextureblend_ps20.inc"
#include "worldtwotextureblend_ps20b.inc"

#include "shaderapifast.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



#if defined( CSTRIKE15 ) && defined( _X360 )
static ConVar r_shader_srgbread( "r_shader_srgbread", "1", 0, "1 = use shader srgb texture reads, 0 = use HW" );
#else
static ConVar r_shader_srgbread( "r_shader_srgbread", "0", 0, "1 = use shader srgb texture reads, 0 = use HW" );
#endif


// FIXME: Need to make a dx9 version so that "CENTROID" works.
BEGIN_VS_SHADER( WorldTwoTextureBlend, 
			  "Help for WorldTwoTextureBlend" )

BEGIN_SHADER_PARAMS
    SHADER_PARAM_OVERRIDE( BASETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "shadertest/WorldTwoTextureBlend", "iris texture", 0 )
	SHADER_PARAM( ALBEDO, SHADER_PARAM_TYPE_TEXTURE, "shadertest/WorldTwoTextureBlend", "albedo (Base texture with no baked lighting)" )
	SHADER_PARAM( SELFILLUMTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Self-illumination tint" )
	SHADER_PARAM( DETAIL, SHADER_PARAM_TYPE_TEXTURE, "shadertest/WorldTwoTextureBlend_detail", "detail texture" )
	SHADER_PARAM( DETAILFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $detail" )
	SHADER_PARAM( DETAILSCALE, SHADER_PARAM_TYPE_FLOAT, "1.0", "scale of the detail texture" )
	SHADER_PARAM( DETAIL_ALPHA_MASK_BASE_TEXTURE, SHADER_PARAM_TYPE_BOOL, "0", 
				  "If this is 1, then when detail alpha=0, no base texture is blended and when "
				  "detail alpha=1, you get detail*base*lightmap" )
	SHADER_PARAM( BUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "models/shadertest/shader1_normal", "bump map" )
	SHADER_PARAM( BUMPFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $bumpmap" )
	SHADER_PARAM( BUMPTRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$bumpmap texcoord transform" )
	SHADER_PARAM( NODIFFUSEBUMPLIGHTING, SHADER_PARAM_TYPE_INTEGER, "0", "0 == Use diffuse bump lighting, 1 = No diffuse bump lighting" )
	SHADER_PARAM( SEAMLESS_SCALE, SHADER_PARAM_TYPE_FLOAT, "0", "Scale factor for 'seamless' texture mapping. 0 means to use ordinary mapping" )

	SHADER_PARAM( SHADERSRGBREAD360, SHADER_PARAM_TYPE_BOOL, "0", "Simulate srgb read in shader code")
END_SHADER_PARAMS

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT_PARAMS()
	{

		if( !params[DETAIL_ALPHA_MASK_BASE_TEXTURE]->IsDefined() )
		{
			params[DETAIL_ALPHA_MASK_BASE_TEXTURE]->SetIntValue( 0 );
		}
	
		params[FLASHLIGHTTEXTURE]->SetStringValue( GetFlashlightTextureFilename() );

		// Write over $basetexture with $albedo if we are going to be using diffuse normal mapping.
		if( g_pConfig->UseBumpmapping() && params[BUMPMAP]->IsDefined() && params[ALBEDO]->IsDefined() &&
			params[BASETEXTURE]->IsDefined() && 
			!( params[NODIFFUSEBUMPLIGHTING]->IsDefined() && params[NODIFFUSEBUMPLIGHTING]->GetIntValue() ) )
		{
			params[BASETEXTURE]->SetStringValue( params[ALBEDO]->GetStringValue() );
		}
	
		if( !params[NODIFFUSEBUMPLIGHTING]->IsDefined() )
		{
			params[NODIFFUSEBUMPLIGHTING]->SetIntValue( 0 );
		}

		if( !params[SELFILLUMTINT]->IsDefined() )
		{
			params[SELFILLUMTINT]->SetVecValue( 1.0f, 1.0f, 1.0f );
		}

		if( !params[DETAILSCALE]->IsDefined() )
		{
			params[DETAILSCALE]->SetFloatValue( 4.0f );
		}

		if( !params[BUMPFRAME]->IsDefined() )
		{
			params[BUMPFRAME]->SetIntValue( 0 );
		}

		if( !params[DETAILFRAME]->IsDefined() )
		{
			params[DETAILFRAME]->SetIntValue( 0 );
		}

		// No texture means no self-illum or env mask in base alpha
		if ( !params[BASETEXTURE]->IsDefined() )
		{
			CLEAR_FLAGS( MATERIAL_VAR_SELFILLUM );
			CLEAR_FLAGS( MATERIAL_VAR_BASEALPHAENVMAPMASK );
		}

		// If in decal mode, no debug override...
		if (IS_FLAG_SET(MATERIAL_VAR_DECAL))
		{
			SET_FLAGS( MATERIAL_VAR_NO_DEBUG_OVERRIDE );
		}

		SET_FLAGS2( MATERIAL_VAR2_LIGHTING_LIGHTMAP );
		if( g_pConfig->UseBumpmapping() && params[BUMPMAP]->IsDefined() && (params[NODIFFUSEBUMPLIGHTING]->GetIntValue() == 0) )
		{
			SET_FLAGS2( MATERIAL_VAR2_LIGHTING_BUMPED_LIGHTMAP );
		}

		// srgb read 360
		InitIntParam( SHADERSRGBREAD360, params, 0 );
	}

	SHADER_INIT
	{
		if( g_pConfig->UseBumpmapping() && params[BUMPMAP]->IsDefined() )
		{
			LoadBumpMap( BUMPMAP );
		}
		
		if (params[BASETEXTURE]->IsDefined())
		{
			LoadTexture( BASETEXTURE, TEXTUREFLAGS_SRGB );

			if (!params[BASETEXTURE]->GetTextureValue()->IsTranslucent())
			{
				CLEAR_FLAGS( MATERIAL_VAR_SELFILLUM );
				CLEAR_FLAGS( MATERIAL_VAR_BASEALPHAENVMAPMASK );
			}
		}

		if (params[DETAIL]->IsDefined())
		{
			LoadTexture( DETAIL );
		}

		LoadTexture( FLASHLIGHTTEXTURE, TEXTUREFLAGS_SRGB );
		
		// Don't alpha test if the alpha channel is used for other purposes
		if (IS_FLAG_SET(MATERIAL_VAR_SELFILLUM) || IS_FLAG_SET(MATERIAL_VAR_BASEALPHAENVMAPMASK) )
		{
			CLEAR_FLAGS( MATERIAL_VAR_ALPHATEST );
		}
			
		// We always need this because of the flashlight.
		SET_FLAGS2( MATERIAL_VAR2_NEEDS_TANGENT_SPACES );
	}

	void DrawPass( IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
		IShaderShadow* pShaderShadow, bool hasFlashlight, VertexCompressionType_t vertexCompression )
	{
		bool bSinglePassFlashlight = false;
		bool hasBump = params[BUMPMAP]->IsTexture();
		bool hasBaseTexture = params[BASETEXTURE]->IsTexture();
		bool hasDetailTexture = /*!hasBump && */params[DETAIL]->IsTexture();
		bool hasVertexColor = IS_FLAG_SET( MATERIAL_VAR_VERTEXCOLOR ) != 0;
		bool bHasDetailAlpha = params[DETAIL_ALPHA_MASK_BASE_TEXTURE]->GetIntValue() != 0;
		bool bIsAlphaTested = IS_FLAG_SET( MATERIAL_VAR_ALPHATEST ) != 0;

		BlendType_t nBlendType = EvaluateBlendRequirements( BASETEXTURE, true );
		bool bFullyOpaque = (nBlendType != BT_BLENDADD) && (nBlendType != BT_BLEND) && !IS_FLAG_SET(MATERIAL_VAR_ALPHATEST); //dest alpha is free for special use

		bool bSeamlessMapping = params[SEAMLESS_SCALE]->GetFloatValue() != 0.0;

#if defined( CSTRIKE15 )
		bool bShaderSrgbRead = ( IsX360() && r_shader_srgbread.GetBool() );
#else
		bool bShaderSrgbRead = ( IsX360() && IS_PARAM_DEFINED( SHADERSRGBREAD360 ) && params[SHADERSRGBREAD360]->GetIntValue() );
#endif

		SHADOW_STATE
		{
			ShadowFilterMode_t nShadowFilterMode = SHADOWFILTERMODE_DEFAULT;

			// Alpha test: FIXME: shouldn't this be handled in Shader_t::SetInitialShadowState
			pShaderShadow->EnableAlphaTest( bIsAlphaTested );
			if( hasFlashlight )
			{
				if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
				{
					nShadowFilterMode = g_pHardwareConfig->GetShadowFilterMode( false /* bForceLowQuality */, g_pHardwareConfig->HasFastVertexTextures() && !IsPlatformX360() && !IsPlatformPS3() /* bPS30 */ );	// Based upon vendor and device dependent formats
				}

				SetAdditiveBlendingShadowState( BASETEXTURE, true );
				pShaderShadow->EnableDepthWrites( false );

				// Be sure not to write to dest alpha
				pShaderShadow->EnableAlphaWrites( false );
			}
			else
			{
				SetDefaultBlendingShadowState( BASETEXTURE, true );
			}

			unsigned int flags = VERTEX_POSITION;
			if( hasBaseTexture )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, !bShaderSrgbRead );
			}
			//			if( hasLightmap )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE );
			}
			if( hasFlashlight )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
				pShaderShadow->EnableTexture( SHADER_SAMPLER7, true );
				//pShaderShadow->SetShadowDepthFiltering( SHADER_SAMPLER7 );
				flags |= VERTEX_TANGENT_S | VERTEX_TANGENT_T | VERTEX_NORMAL;
			}
			if( hasDetailTexture )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );
			}
			if( hasBump )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );
			}
			if( hasVertexColor )
			{
				flags |= VERTEX_COLOR;
			}

			// Normalizing cube map
			pShaderShadow->EnableTexture( SHADER_SAMPLER6, true );

			// texcoord0 : base texcoord
			// texcoord1 : lightmap texcoord
			// texcoord2 : lightmap texcoord offset
			int numTexCoords = 2;
			if( hasBump )
			{
				numTexCoords = 3;
			}

			pShaderShadow->VertexShaderVertexFormat( flags, numTexCoords, 0, 0 );

			// Pre-cache pixel shaders
			bool hasSelfIllum = IS_FLAG_SET( MATERIAL_VAR_SELFILLUM );

			pShaderShadow->EnableSRGBWrite( true );

			int nLightingPreviewMode = 0;
#if 0
			int nLightingPreviewMode = IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER0 ) + 2 * IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER1 );
#endif

#if !defined( _X360 ) && !defined( _PS3 )
			if ( g_pHardwareConfig->HasFastVertexTextures() )
			{
				DECLARE_STATIC_VERTEX_SHADER( lightmappedgeneric_vs30 );
				SET_STATIC_VERTEX_SHADER_COMBO( ENVMAP_MASK,  false );
				SET_STATIC_VERTEX_SHADER_COMBO( BUMPMASK,  false );
				SET_STATIC_VERTEX_SHADER_COMBO( TANGENTSPACE,  hasFlashlight );
				SET_STATIC_VERTEX_SHADER_COMBO( BUMPMAP,  hasBump );
				SET_STATIC_VERTEX_SHADER_COMBO( VERTEXCOLOR,  hasVertexColor );
				SET_STATIC_VERTEX_SHADER_COMBO( VERTEXALPHATEXBLENDFACTOR, false );
				SET_STATIC_VERTEX_SHADER_COMBO( SEAMLESS, bSeamlessMapping ); //( bumpmap_variant == 2 )?1:0);
				SET_STATIC_VERTEX_SHADER_COMBO( DETAILTEXTURE,  hasDetailTexture );
				SET_STATIC_VERTEX_SHADER_COMBO( SELFILLUM,  hasSelfIllum );
				SET_STATIC_VERTEX_SHADER_COMBO( FANCY_BLENDING,  false );
				SET_STATIC_VERTEX_SHADER_COMBO( LIGHTING_PREVIEW, nLightingPreviewMode != 0 );
				SET_STATIC_VERTEX_SHADER_COMBO( PAINT, 0 );
				SET_STATIC_VERTEX_SHADER_COMBO( ADDBUMPMAPS, 0 );
				SET_STATIC_VERTEX_SHADER( lightmappedgeneric_vs30 );
			}
			else
#endif
			{
				DECLARE_STATIC_VERTEX_SHADER( lightmappedgeneric_vs20 );
				SET_STATIC_VERTEX_SHADER_COMBO( ENVMAP_MASK,  false );
				SET_STATIC_VERTEX_SHADER_COMBO( BUMPMASK,  false );
				SET_STATIC_VERTEX_SHADER_COMBO( TANGENTSPACE,  hasFlashlight );
				SET_STATIC_VERTEX_SHADER_COMBO( BUMPMAP,  hasBump );
				SET_STATIC_VERTEX_SHADER_COMBO( VERTEXCOLOR,  hasVertexColor );
				SET_STATIC_VERTEX_SHADER_COMBO( VERTEXALPHATEXBLENDFACTOR, false );
				SET_STATIC_VERTEX_SHADER_COMBO( SEAMLESS, bSeamlessMapping ); //( bumpmap_variant == 2 )?1:0);
				SET_STATIC_VERTEX_SHADER_COMBO( DETAILTEXTURE,  hasDetailTexture );
				SET_STATIC_VERTEX_SHADER_COMBO( SELFILLUM,  hasSelfIllum );
				SET_STATIC_VERTEX_SHADER_COMBO( FANCY_BLENDING,  false );
				SET_STATIC_VERTEX_SHADER_COMBO( LIGHTING_PREVIEW, nLightingPreviewMode != 0 );
				SET_STATIC_VERTEX_SHADER_COMBO( PAINT, 0 );
				SET_STATIC_VERTEX_SHADER_COMBO( ADDBUMPMAPS, 0 );
	#if defined( _X360 ) || defined( _PS3 )
				SET_STATIC_VERTEX_SHADER_COMBO( FLASHLIGHT, hasFlashlight );
	#endif
				SET_STATIC_VERTEX_SHADER( lightmappedgeneric_vs20 );
			}

#if !defined( _X360 ) && !defined( _PS3 )
			if ( g_pHardwareConfig->HasFastVertexTextures() )
			{
				DECLARE_STATIC_PIXEL_SHADER( worldtwotextureblend_ps30 );
				SET_STATIC_PIXEL_SHADER_COMBO( DETAILTEXTURE,  hasDetailTexture );
				SET_STATIC_PIXEL_SHADER_COMBO( BUMPMAP,  hasBump );
				SET_STATIC_PIXEL_SHADER_COMBO( VERTEXCOLOR,  hasVertexColor );
				SET_STATIC_PIXEL_SHADER_COMBO( SELFILLUM,  hasSelfIllum );
				SET_STATIC_PIXEL_SHADER_COMBO( DETAIL_ALPHA_MASK_BASE_TEXTURE,  bHasDetailAlpha );
				SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHT,  hasFlashlight );
				SET_STATIC_PIXEL_SHADER_COMBO( SEAMLESS,  bSeamlessMapping );
				SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode );
				SET_STATIC_PIXEL_SHADER_COMBO( SHADER_SRGB_READ, bShaderSrgbRead );
				SET_STATIC_PIXEL_SHADER( worldtwotextureblend_ps30 );
			}
			else
#endif
			if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( worldtwotextureblend_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( DETAILTEXTURE,  hasDetailTexture );
				SET_STATIC_PIXEL_SHADER_COMBO( BUMPMAP,  hasBump );
				SET_STATIC_PIXEL_SHADER_COMBO( VERTEXCOLOR,  hasVertexColor );
				SET_STATIC_PIXEL_SHADER_COMBO( SELFILLUM,  hasSelfIllum );
				SET_STATIC_PIXEL_SHADER_COMBO( DETAIL_ALPHA_MASK_BASE_TEXTURE,  bHasDetailAlpha );
				SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHT,  hasFlashlight );
				SET_STATIC_PIXEL_SHADER_COMBO( SEAMLESS,  bSeamlessMapping );
				SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode );
				SET_STATIC_PIXEL_SHADER_COMBO( SHADER_SRGB_READ, bShaderSrgbRead );
				SET_STATIC_PIXEL_SHADER( worldtwotextureblend_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( worldtwotextureblend_ps20 );
				SET_STATIC_PIXEL_SHADER_COMBO( DETAILTEXTURE,  hasDetailTexture );
				SET_STATIC_PIXEL_SHADER_COMBO( BUMPMAP,  hasBump );
				SET_STATIC_PIXEL_SHADER_COMBO( VERTEXCOLOR,  hasVertexColor );
				SET_STATIC_PIXEL_SHADER_COMBO( SELFILLUM,  hasSelfIllum );
				SET_STATIC_PIXEL_SHADER_COMBO( DETAIL_ALPHA_MASK_BASE_TEXTURE,  bHasDetailAlpha );
				SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHT,  hasFlashlight );
				SET_STATIC_PIXEL_SHADER_COMBO( SEAMLESS,  bSeamlessMapping );
				SET_STATIC_PIXEL_SHADER_COMBO( SHADER_SRGB_READ, bShaderSrgbRead );
				SET_STATIC_PIXEL_SHADER( worldtwotextureblend_ps20 );
			}

			// HACK HACK HACK - enable alpha writes all the time so that we have them for
			// underwater stuff. 
			// But only do it if we're not using the alpha already for translucency
			pShaderShadow->EnableAlphaWrites( bFullyOpaque );

			if( hasFlashlight )
			{
				FogToBlack();
			}
			else
			{
				DefaultFog();
			}

			PI_BeginCommandBuffer();
			PI_SetModulationVertexShaderDynamicState( );
			PI_EndCommandBuffer();
		}
		DYNAMIC_STATE
		{
			if( hasBaseTexture )
			{
				BindTexture( SHADER_SAMPLER0, SRGBReadMask( !bShaderSrgbRead ), BASETEXTURE, FRAME );
			}
			else
			{
				ShaderApiFast( pShaderAPI )->BindStandardTexture( SHADER_SAMPLER0, SRGBReadMask( !bShaderSrgbRead ), TEXTURE_WHITE );
			}

			//			if( hasLightmap )
			{
				ShaderApiFast( pShaderAPI )->BindStandardTexture( SHADER_SAMPLER1, ( g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE ) ? TEXTURE_BINDFLAGS_SRGBREAD : TEXTURE_BINDFLAGS_NONE, TEXTURE_LIGHTMAP );
			}

			bool bFlashlightShadows = false;
			bool bUberlight = false;
			if( hasFlashlight )
			{
				VMatrix worldToTexture;
				ITexture *pFlashlightDepthTexture;
				FlashlightState_t state = ShaderApiFast( pShaderAPI )->GetFlashlightStateEx( worldToTexture, &pFlashlightDepthTexture );
				bFlashlightShadows = state.m_bEnableShadows;
				bUberlight = state.m_bUberlight;

				SetFlashLightColorFromState( state, pShaderAPI, bSinglePassFlashlight );

				BindTexture( SHADER_SAMPLER2, TEXTURE_BINDFLAGS_NONE, state.m_pSpotlightTexture, state.m_nSpotlightTextureFrame );

				if( pFlashlightDepthTexture && g_pConfig->ShadowDepthTexture() )
				{
					BindTexture( SHADER_SAMPLER7, TEXTURE_BINDFLAGS_SHADOWDEPTH, pFlashlightDepthTexture );
				}
			}
			if( hasDetailTexture )
			{
				BindTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, DETAIL, DETAILFRAME );
			}
			if( hasBump )
			{
				if( !g_pConfig->m_bFastNoBump )
				{
					BindTexture( SHADER_SAMPLER4, TEXTURE_BINDFLAGS_NONE, BUMPMAP, BUMPFRAME );
				}
				else
				{
					ShaderApiFast( pShaderAPI )->BindStandardTexture( SHADER_SAMPLER4, TEXTURE_BINDFLAGS_NONE, TEXTURE_NORMALMAP_FLAT );
				}
			}
			ShaderApiFast( pShaderAPI )->BindStandardTexture( SHADER_SAMPLER6, TEXTURE_BINDFLAGS_NONE, TEXTURE_NORMALIZATION_CUBEMAP_SIGNED );

			// If we don't have a texture transform, we don't have
			// to set vertex shader constants or run vertex shader instructions
			// for the texture transform.
			bool bHasTextureTransform = 
				!( params[BASETEXTURETRANSFORM]->MatrixIsIdentity() &&
				params[BUMPTRANSFORM]->MatrixIsIdentity() );

			bool bVertexShaderFastPath = !bHasTextureTransform;
			if( params[DETAIL]->IsTexture() )
			{
				bVertexShaderFastPath = false;
			}
			if( ShaderApiFast( pShaderAPI )->GetIntRenderingParameter(INT_RENDERPARM_ENABLE_FIXED_LIGHTING) != 0 )
			{
				bVertexShaderFastPath = false;
			}

			if( !bVertexShaderFastPath )
			{
				if ( !bSeamlessMapping )
				{
					SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, BASETEXTURETRANSFORM );
				}
				if( hasBump && !bHasDetailAlpha )
				{
					SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_2, BUMPTRANSFORM );
					Assert( !hasDetailTexture );
				}
			}

			MaterialFogMode_t fogType = ShaderApiFast( pShaderAPI )->GetSceneFogMode();

			if ( IsPC() )
			{
				bool bWorldNormal = ShaderApiFast( pShaderAPI )->GetIntRenderingParameter( INT_RENDERPARM_ENABLE_FIXED_LIGHTING ) == ENABLE_FIXED_LIGHTING_OUTPUTNORMAL_AND_DEPTH;
				if ( bWorldNormal )
				{
					float vEyeDir[4];
					ShaderApiFast( pShaderAPI )->GetWorldSpaceCameraDirection( vEyeDir );

					float flFarZ = ShaderApiFast( pShaderAPI )->GetFarZ();
					vEyeDir[0] /= flFarZ;	// Divide by farZ for SSAO algorithm
					vEyeDir[1] /= flFarZ;
					vEyeDir[2] /= flFarZ;
					ShaderApiFast( pShaderAPI )->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_12, vEyeDir );
				}
			}

#if !defined( _X360 ) && !defined( _PS3 )
			if (g_pHardwareConfig->HasFastVertexTextures() )
			{
				DECLARE_DYNAMIC_VERTEX_SHADER( lightmappedgeneric_vs30 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( FASTPATH,  bVertexShaderFastPath );
				SET_DYNAMIC_VERTEX_SHADER( lightmappedgeneric_vs30 );
			}
			else
#endif
			{
				DECLARE_DYNAMIC_VERTEX_SHADER( lightmappedgeneric_vs20 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( FASTPATH,  bVertexShaderFastPath );
				SET_DYNAMIC_VERTEX_SHADER( lightmappedgeneric_vs20 );
			}

			bool bWriteDepthToAlpha;
			bool bWriteWaterFogToAlpha;
			if( bFullyOpaque ) 
			{
				bWriteDepthToAlpha = ShaderApiFast( pShaderAPI )->ShouldWriteDepthToDestAlpha();
				bWriteWaterFogToAlpha = (fogType == MATERIAL_FOG_LINEAR_BELOW_FOG_Z);
				AssertMsg( !(bWriteDepthToAlpha && bWriteWaterFogToAlpha), "Can't write two values to alpha at the same time." );
			}
			else
			{
				//can't write a special value to dest alpha if we're actually using as-intended alpha
				bWriteDepthToAlpha = false;
				bWriteWaterFogToAlpha = false;
			}

#if !defined( _X360 ) && !defined( _PS3 )
			if ( g_pHardwareConfig->HasFastVertexTextures() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( worldtwotextureblend_ps30 );

				// Don't write fog to alpha if we're using translucency
				SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITE_DEPTH_TO_DESTALPHA, bWriteDepthToAlpha );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHTSHADOWS, bFlashlightShadows );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( UBERLIGHT, bUberlight );
				SET_DYNAMIC_PIXEL_SHADER( worldtwotextureblend_ps30 );
			}
			else
#endif
			if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( worldtwotextureblend_ps20b );

				// Don't write fog to alpha if we're using translucency
				SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITE_DEPTH_TO_DESTALPHA, bWriteDepthToAlpha );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHTSHADOWS, bFlashlightShadows );
				SET_DYNAMIC_PIXEL_SHADER( worldtwotextureblend_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( worldtwotextureblend_ps20 );

				// Don't write fog to alpha if we're using translucency
				SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITEWATERFOGTODESTALPHA, (fogType == MATERIAL_FOG_LINEAR_BELOW_FOG_Z) && 
												(nBlendType != BT_BLENDADD) && (nBlendType != BT_BLEND) && !bIsAlphaTested );
				SET_DYNAMIC_PIXEL_SHADER( worldtwotextureblend_ps20 );
			}


			// always set the transform for detail textures since I'm assuming that you'll
			// always have a detailscale.
			if( hasDetailTexture )
			{
				SetVertexShaderTextureScaledTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_2, BASETEXTURETRANSFORM, DETAILSCALE );
				Assert( !( hasBump && !bHasDetailAlpha ) );
			}

			SetPixelShaderConstantGammaToLinear( 7, SELFILLUMTINT );

			float eyePos[4];
			ShaderApiFast( pShaderAPI )->GetWorldSpaceCameraPosition( eyePos );
			ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( 10, eyePos, 1 );
			ShaderApiFast( pShaderAPI )->SetPixelShaderFogParams( 11 );

			if ( bSeamlessMapping )
			{
				float map_scale[4]={ params[SEAMLESS_SCALE]->GetFloatValue(),0,0,0};
				ShaderApiFast( pShaderAPI )->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, map_scale );
			}


			if( hasFlashlight )
			{
				VMatrix worldToTexture;
				const FlashlightState_t &flashlightState = ShaderApiFast( pShaderAPI )->GetFlashlightState( worldToTexture );

				// Set the flashlight attenuation factors
				float atten[4];
				atten[0] = flashlightState.m_fConstantAtten;
				atten[1] = flashlightState.m_fLinearAtten;
				atten[2] = flashlightState.m_fQuadraticAtten;
				atten[3] = flashlightState.m_FarZAtten;
				ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( 20, atten, 1 );

				// Set the flashlight origin
				float pos[4];
				pos[0] = flashlightState.m_vecLightOrigin[0];
				pos[1] = flashlightState.m_vecLightOrigin[1];
				pos[2] = flashlightState.m_vecLightOrigin[2];
				pos[3] = flashlightState.m_FarZ; // didn't have this in main. . probably need this?
				ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( 15, pos, 1 );

				ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( 16, worldToTexture.Base(), 4 );

				if ( IsPC() && g_pHardwareConfig->HasFastVertexTextures() )
				{
					SetupUberlightFromState( pShaderAPI, flashlightState );
				}
			}
		}
		Draw();
	}

	SHADER_DRAW
	{
		bool bHasFlashlight = UsingFlashlight( params );
		if ( bHasFlashlight && ( IsX360() || IsPS3() ) )
		{
			DrawPass( params, pShaderAPI, pShaderShadow, false, vertexCompression );
			SHADOW_STATE
			{
				SetInitialShadowState( );
			}
		}
		DrawPass( params, pShaderAPI, pShaderShadow, bHasFlashlight, vertexCompression );
	}

END_SHADER
