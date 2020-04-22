//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "BaseVSShader.h"
#include "refract_dx9_helper.h"
#include "convar.h"
#include "refract_vs20.inc"
#include "refract_ps20.inc"
#include "refract_ps20b.inc"
#include "cpp_shader_constant_register_map.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


#define MAXBLUR 1

// number of pixels to shrink the viewport by when handling mirroring of texcoords about the viewport edges.  This deals with not stepping out of viewport due to blurring, etc.
#define REFRACT_VIEWPORT_SHRINK_PIXELS ( 2 )

// FIXME: doesn't support fresnel!
void InitParamsRefract_DX9( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, Refract_DX9_Vars_t &info )
{
	SET_FLAGS2( MATERIAL_VAR2_NEEDS_TANGENT_SPACES );
	SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
	if( !params[info.m_nEnvmapTint]->IsDefined() )
	{
		params[info.m_nEnvmapTint]->SetVecValue( 1.0f, 1.0f, 1.0f );
	}
	if( !params[info.m_nEnvmapContrast]->IsDefined() )
	{
		params[info.m_nEnvmapContrast]->SetFloatValue( 0.0f );
	}
	if( !params[info.m_nEnvmapSaturation]->IsDefined() )
	{
		params[info.m_nEnvmapSaturation]->SetFloatValue( 1.0f );
	}
	if( !params[info.m_nEnvmapFrame]->IsDefined() )
	{
		params[info.m_nEnvmapFrame]->SetIntValue( 0 );
	}
	if( !params[info.m_nFresnelReflection]->IsDefined() )
	{
		params[info.m_nFresnelReflection]->SetFloatValue( 1.0f );
	}
	if( !params[info.m_nMasked]->IsDefined() )
	{
		params[info.m_nMasked]->SetIntValue( 0 );
	}
	if( !params[info.m_nBlurAmount]->IsDefined() )
	{
		params[info.m_nBlurAmount]->SetIntValue( 0 );
	}
	if( !params[info.m_nFadeOutOnSilhouette]->IsDefined() )
	{
		params[info.m_nFadeOutOnSilhouette]->SetIntValue( 0 );
	}
	if( !params[info.m_nNoViewportFixup]->IsDefined() )
	{
		params[info.m_nNoViewportFixup]->SetIntValue( 0 );
	}
	if( !params[info.m_nMirrorAboutViewportEdges]->IsDefined() )
	{
		params[info.m_nMirrorAboutViewportEdges]->SetIntValue( 0 );
	}
	if ( !params[info.m_nMagnifyEnable]->IsDefined() )
	{
		params[info.m_nMagnifyEnable]->SetIntValue( 0 );
	}
	if ( !params[info.m_nMagnifyCenter]->IsDefined() )
	{
		params[info.m_nMagnifyCenter]->SetVecValue( 0, 0, 0, 0 );
	}
	if ( !params[info.m_nMagnifyScale]->IsDefined() )
	{
		params[info.m_nMagnifyScale]->SetIntValue( 0 );
	}
	if ( !params[info.m_nLocalRefract]->IsDefined() )
	{
		params[info.m_nLocalRefract]->SetIntValue( 0 );
	}
	if ( !params[info.m_nLocalRefractDepth]->IsDefined() )
	{
		params[info.m_nLocalRefractDepth]->SetFloatValue( 0.05f );
	}

	// Local refract doesn't need a copy of the frame buffer and doesn't require the translucent flag
	if ( params[info.m_nLocalRefract]->GetIntValue() == 0 )
	{
		SET_FLAGS( MATERIAL_VAR_TRANSLUCENT );
		SET_FLAGS2( MATERIAL_VAR2_NEEDS_POWER_OF_TWO_FRAME_BUFFER_TEXTURE );
	}
}

void InitRefract_DX9( CBaseVSShader *pShader, IMaterialVar** params, Refract_DX9_Vars_t &info )
{
	if (params[info.m_nBaseTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nBaseTexture, TEXTUREFLAGS_SRGB | ANISOTROPIC_OVERRIDE );
	}
	if (params[info.m_nNormalMap]->IsDefined() )
	{
		pShader->LoadBumpMap( info.m_nNormalMap, ANISOTROPIC_OVERRIDE );
	}
	if (params[info.m_nNormalMap2]->IsDefined() )
	{
		pShader->LoadBumpMap( info.m_nNormalMap2, ANISOTROPIC_OVERRIDE );
	}
	if( params[info.m_nEnvmap]->IsDefined() )
	{
		pShader->LoadCubeMap( info.m_nEnvmap, TEXTUREFLAGS_SRGB | ANISOTROPIC_OVERRIDE );
	}
	if( params[info.m_nRefractTintTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nRefractTintTexture, TEXTUREFLAGS_SRGB  );
	}
}

void DrawRefract_DX9( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
					 IShaderShadow* pShaderShadow, Refract_DX9_Vars_t &info, VertexCompressionType_t vertexCompression )
{
	bool bIsModel = IS_FLAG_SET( MATERIAL_VAR_MODEL );
	bool bHasEnvmap = params[info.m_nEnvmap]->IsTexture();
	bool bRefractTintTexture = params[info.m_nRefractTintTexture]->IsTexture();
	bool bFadeOutOnSilhouette = params[info.m_nFadeOutOnSilhouette]->GetIntValue() != 0;
	int blurAmount = params[info.m_nBlurAmount]->GetIntValue();
	bool bMasked = (params[info.m_nMasked]->GetIntValue() != 0);
	bool bSecondaryNormal = ( ( info.m_nNormalMap2 != -1 ) && ( params[info.m_nNormalMap2]->IsTexture() ) );
	bool bColorModulate = ( ( info.m_nVertexColorModulate != -1 ) && ( params[info.m_nVertexColorModulate]->GetIntValue() ) );
	bool bWriteZ = params[info.m_nNoWriteZ]->GetIntValue() == 0;
	bool bMirrorAboutViewportEdges = IsX360() && ( info.m_nMirrorAboutViewportEdges != -1 ) && ( params[info.m_nMirrorAboutViewportEdges]->GetIntValue() != 0 );
	bool bUseMagnification = params[info.m_nMagnifyEnable]->GetIntValue() != 0;
	
	if( blurAmount < 0 )
	{
		blurAmount = 0;
	}
	else if( blurAmount > MAXBLUR )
	{
		blurAmount = MAXBLUR;
	}

	BlendType_t nBlendType = pShader->EvaluateBlendRequirements( BASETEXTURE, true );
	bool bFullyOpaque = (nBlendType != BT_BLENDADD) && (nBlendType != BT_BLEND) && !IS_FLAG_SET(MATERIAL_VAR_ALPHATEST); //dest alpha is free for special use
	bFullyOpaque &= !bMasked;

	bool bTranslucentNormal = pShader->TextureIsTranslucent( info.m_nNormalMap, false );
	bFullyOpaque &= (! bTranslucentNormal );

	SHADOW_STATE
	{
		pShader->SetInitialShadowState( );

		pShaderShadow->EnableDepthWrites( bWriteZ );

		// Alpha test: FIXME: shouldn't this be handled in Shader_t::SetInitialShadowState
		pShaderShadow->EnableAlphaTest( IS_FLAG_SET(MATERIAL_VAR_ALPHATEST) );

		// If envmap is not specified, the alpha channel is the translucency
		// (If envmap *is* specified, alpha channel is the reflection amount)
		if ( params[info.m_nNormalMap]->IsTexture() && !bHasEnvmap )
		{
			pShader->SetDefaultBlendingShadowState( info.m_nNormalMap, false );
		}

		// source render target that contains the image that we are warping.
		pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER2, !IsX360() );

		// normal map
		pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );

		if ( bSecondaryNormal )
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
		}

		if( bHasEnvmap )
		{
			// envmap
			pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER4, true );
		}
		if( bRefractTintTexture )
		{
			// refract tint texture
			pShaderShadow->EnableTexture( SHADER_SAMPLER5, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER5, true );
		}

		// Sampler for nvidia's stereo hackery
		pShaderShadow->EnableTexture( SHADER_SAMPLER6, true );
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER6, false );

		pShaderShadow->EnableSRGBWrite( true );

		unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL;
		int userDataSize = 0;
		int nTexCoordCount = 1;
		if( bIsModel )
		{
			userDataSize = 4;
		}
		else
		{
			flags |= VERTEX_TANGENT_S | VERTEX_TANGENT_T;
		}

		if ( bColorModulate )
		{
			flags |= VERTEX_COLOR;
		}
		
		// This shader supports compressed vertices, so OR in that flag:
		flags |= VERTEX_FORMAT_COMPRESSED;

		pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );
		
		DECLARE_STATIC_VERTEX_SHADER( refract_vs20 );
		SET_STATIC_VERTEX_SHADER_COMBO( MODEL,  bIsModel );
		SET_STATIC_VERTEX_SHADER_COMBO( COLORMODULATE, bColorModulate );
		SET_STATIC_VERTEX_SHADER( refract_vs20 );

		// We have to do this in the shader on R500 or Leopard
		bool bShaderSRGBConvert = IsOSX() && ( g_pHardwareConfig->FakeSRGBWrite() || !g_pHardwareConfig->CanDoSRGBReadFromRTs() );

		if ( g_pHardwareConfig->SupportsPixelShaders_2_b() || g_pHardwareConfig->ShouldAlwaysUseShaderModel2bShaders() ) // always send OpenGL down the ps2b path
		{
			DECLARE_STATIC_PIXEL_SHADER( refract_ps20b );
			SET_STATIC_PIXEL_SHADER_COMBO( BLUR,  blurAmount );
			SET_STATIC_PIXEL_SHADER_COMBO( FADEOUTONSILHOUETTE,  bFadeOutOnSilhouette );
			SET_STATIC_PIXEL_SHADER_COMBO( CUBEMAP,  bHasEnvmap );
			SET_STATIC_PIXEL_SHADER_COMBO( REFRACTTINTTEXTURE,  bRefractTintTexture );
			SET_STATIC_PIXEL_SHADER_COMBO( MASKED, bMasked );
			SET_STATIC_PIXEL_SHADER_COMBO( COLORMODULATE, bColorModulate );
			SET_STATIC_PIXEL_SHADER_COMBO( SECONDARY_NORMAL, bSecondaryNormal );
			SET_STATIC_PIXEL_SHADER_COMBO( MIRRORABOUTVIEWPORTEDGES, bMirrorAboutViewportEdges );
			SET_STATIC_PIXEL_SHADER_COMBO( MAGNIFY, bUseMagnification );
			SET_STATIC_PIXEL_SHADER_COMBO( SHADER_SRGB_READ, bShaderSRGBConvert );
			SET_STATIC_PIXEL_SHADER_COMBO( LOCALREFRACT, ( params[info.m_nLocalRefract]->GetIntValue() != 0 ) );
			SET_STATIC_PIXEL_SHADER( refract_ps20b );
		}
		else
		{
			DECLARE_STATIC_PIXEL_SHADER( refract_ps20 );
			SET_STATIC_PIXEL_SHADER_COMBO( BLUR,  blurAmount );
			SET_STATIC_PIXEL_SHADER_COMBO( FADEOUTONSILHOUETTE,  bFadeOutOnSilhouette );
			SET_STATIC_PIXEL_SHADER_COMBO( CUBEMAP,  bHasEnvmap );
			SET_STATIC_PIXEL_SHADER_COMBO( REFRACTTINTTEXTURE,  bRefractTintTexture );
			SET_STATIC_PIXEL_SHADER_COMBO( MASKED, bMasked );
			SET_STATIC_PIXEL_SHADER_COMBO( COLORMODULATE, bColorModulate );
			SET_STATIC_PIXEL_SHADER_COMBO( SECONDARY_NORMAL, bSecondaryNormal );
			SET_STATIC_PIXEL_SHADER_COMBO( MIRRORABOUTVIEWPORTEDGES, bMirrorAboutViewportEdges );
			SET_STATIC_PIXEL_SHADER_COMBO( MAGNIFY, bUseMagnification );
			SET_STATIC_PIXEL_SHADER_COMBO( LOCALREFRACT, ( params[info.m_nLocalRefract]->GetIntValue() != 0 ) );
			SET_STATIC_PIXEL_SHADER( refract_ps20 );
		}
		pShader->DefaultFog();
		if( bMasked )
		{
			pShader->EnableAlphaBlending( SHADER_BLEND_ONE_MINUS_SRC_ALPHA, SHADER_BLEND_SRC_ALPHA );
		}

		pShaderShadow->EnableAlphaWrites( bFullyOpaque );
	}
	DYNAMIC_STATE
	{
		pShaderAPI->SetDefaultState();

		if ( params[info.m_nBaseTexture]->IsTexture() )
		{
			pShader->BindTexture( SHADER_SAMPLER2, IsX360() ? TEXTURE_BINDFLAGS_NONE : TEXTURE_BINDFLAGS_SRGBREAD, info.m_nBaseTexture, info.m_nFrame );

			// Set refract texture dimensions
			ITexture *pTarget = params[info.m_nBaseTexture]->GetTextureValue();
			int nWidth = pTarget->GetActualWidth();
			int nHeight = pTarget->GetActualHeight();
			float c7[4] = { nHeight / nWidth, 1.0f, params[info.m_nLocalRefractDepth]->GetFloatValue(), 0.0f };
			pShaderAPI->SetPixelShaderConstant( 7, c7, 1 );
		}
		else
		{
			pShaderAPI->BindStandardTexture( SHADER_SAMPLER2, IsX360() ? TEXTURE_BINDFLAGS_NONE : TEXTURE_BINDFLAGS_SRGBREAD, TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0 );

			// Set refract texture dimensions
			int nWidth = 0;
			int nHeight = 0;
			pShaderAPI->GetStandardTextureDimensions( &nWidth, &nHeight, TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0 );
			float c7[4] = { nHeight / nWidth, 1.0f, params[info.m_nLocalRefractDepth]->GetFloatValue(), 0.0f };
			pShaderAPI->SetPixelShaderConstant( 7, c7, 1 );
		}

		pShader->BindTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, info.m_nNormalMap, info.m_nBumpFrame );

		if ( bSecondaryNormal )
		{
			pShader->BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, info.m_nNormalMap2, info.m_nBumpFrame2 );
		}

		if( bHasEnvmap )
		{
			pShader->BindTexture( SHADER_SAMPLER4, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nEnvmap, info.m_nEnvmapFrame );
		}

		if( bRefractTintTexture )
		{
			pShader->BindTexture( SHADER_SAMPLER5, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nRefractTintTexture, info.m_nRefractTintTextureFrame );
		}

		bool bNvidiaStereoActiveThisFrame = pShaderAPI->IsStereoActiveThisFrame();
		if ( bNvidiaStereoActiveThisFrame )
		{
			pShaderAPI->BindStandardTexture( SHADER_SAMPLER6, TEXTURE_BINDFLAGS_NONE, TEXTURE_STEREO_PARAM_MAP );
		}

		DECLARE_DYNAMIC_VERTEX_SHADER( refract_vs20 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING,  pShaderAPI->GetCurrentNumBones() > 0 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
		SET_DYNAMIC_VERTEX_SHADER( refract_vs20 );

		if ( g_pHardwareConfig->SupportsPixelShaders_2_b() || g_pHardwareConfig->ShouldAlwaysUseShaderModel2bShaders() ) // always send OpenGL down the ps2b path
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( refract_ps20b );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITE_DEPTH_TO_DESTALPHA, bWriteZ && bFullyOpaque && pShaderAPI->ShouldWriteDepthToDestAlpha() );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( D_NVIDIA_STEREO, bNvidiaStereoActiveThisFrame );
			SET_DYNAMIC_PIXEL_SHADER( refract_ps20b );
		}
		else
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( refract_ps20 );
			SET_DYNAMIC_PIXEL_SHADER( refract_ps20 );
		}

		pShader->SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_1, info.m_nBumpTransform );	// 1 & 2
		pShader->SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3, info.m_nBumpTransform2 );	// 3 & 4

		pShaderAPI->SetPixelShaderFogParams( PSREG_FOG_PARAMS );

		float vEyePos_SpecExponent[4];
		pShaderAPI->GetWorldSpaceCameraPosition( vEyePos_SpecExponent );
		vEyePos_SpecExponent[3] = 0.0f;
		pShaderAPI->SetPixelShaderConstant( PSREG_EYEPOS_SPEC_EXPONENT, vEyePos_SpecExponent, 1 );

		pShader->SetPixelShaderConstantGammaToLinear( 0, info.m_nEnvmapTint );
		pShader->SetPixelShaderConstantGammaToLinear( 1, info.m_nRefractTint );
		pShader->SetPixelShaderConstant( 2, info.m_nEnvmapContrast );
		pShader->SetPixelShaderConstant( 3, info.m_nEnvmapSaturation );
		float c5[4] = { params[info.m_nRefractAmount]->GetFloatValue(), 
			params[info.m_nRefractAmount]->GetFloatValue(), 0.0f, 0.0f };

		// Time % 1000
		c5[3] = pShaderAPI->CurrentTime();
		c5[3] -= (float)( (int)( c5[3] / 1000.0f ) ) * 1000.0f;
		pShaderAPI->SetPixelShaderConstant( 5, c5, 1 );

		float c6[4];
		params[info.m_nMagnifyCenter]->GetVecValue( c6, 2 );
		c6[2] = params[info.m_nMagnifyScale]->GetFloatValue();
		if ( c6[2] != 0 )
		{
			c6[2] = 1.0f / c6[2]; // Shader uses the inverse scale value
		}
		pShaderAPI->SetPixelShaderConstant( 6, c6, 1 );

		float cVs3[4] = { c5[3], 0.0f, 0.0f, 0.0f };
		pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_5, cVs3, 1 );

		// Get viewport and render target dimensions and set shader constant to do a 2D mad and also deal with mirror on viewport edges.
		int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
		pShaderAPI->GetCurrentViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );

		int nRtWidth, nRtHeight;
		pShaderAPI->GetCurrentRenderTargetDimensions( nRtWidth, nRtHeight );

		float vViewportMad[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
		if ( params[ info.m_nNoViewportFixup ]->GetIntValue() == 0 )
		{
			vViewportMad[0] = ( float )nViewportWidth / ( float )nRtWidth;
			vViewportMad[1] = ( float )nViewportHeight / ( float )nRtHeight;
			vViewportMad[2] = ( float )nViewportX / ( float )nRtWidth;
			vViewportMad[3] = ( float )nViewportY / ( float )nRtHeight;
		}
		pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6, vViewportMad, 1 );

		if ( bMirrorAboutViewportEdges )
		{
			// Need the extents that we are allowed to sample from the refract texture to clamp by for splitscreen, etc.
			float vNormalizedViewportMinXYMaxWZ[4];

			vNormalizedViewportMinXYMaxWZ[0] = ( float )( nViewportX + REFRACT_VIEWPORT_SHRINK_PIXELS ) / ( float )nRtWidth;
			vNormalizedViewportMinXYMaxWZ[1] = ( float )( nViewportY + REFRACT_VIEWPORT_SHRINK_PIXELS ) / ( float )nRtHeight;
			vNormalizedViewportMinXYMaxWZ[3] = ( float )( nViewportX + nViewportWidth - REFRACT_VIEWPORT_SHRINK_PIXELS - 1 ) / ( float )nRtWidth;
			vNormalizedViewportMinXYMaxWZ[2] = ( float )( nViewportY + nViewportHeight - REFRACT_VIEWPORT_SHRINK_PIXELS - 1 ) / ( float )nRtHeight;

			pShaderAPI->SetPixelShaderConstant( 4, vNormalizedViewportMinXYMaxWZ, 1 );
		}
	}
	pShader->Draw();
}

