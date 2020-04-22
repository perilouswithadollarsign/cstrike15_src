//========= Copyright � 1996-2006, Valve Corporation, All rights reserved. ============//
#include "BaseVSShader.h"
#include "mathlib/vmatrix.h"
#include "portal_refract_helper.h"
#include "convar.h"

// Auto generated inc files
#include "portal_refract_vs20.inc"
#include "portal_refract_ps20.inc"
#include "portal_refract_ps20b.inc"

#include "shaderapifast.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


void InitParamsPortalRefract( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, PortalRefractVars_t &info )
{
	// Set material flags
	SET_FLAGS( MATERIAL_VAR_MODEL );
	SET_FLAGS2( MATERIAL_VAR2_NEEDS_TANGENT_SPACES );

	SET_FLAGS( MATERIAL_VAR_TRANSLUCENT );

	// Set material parameter default values
	if ( ( info.m_nStage != -1 ) && ( !params[info.m_nStage]->IsDefined() ) )
	{
		params[info.m_nStage]->SetIntValue( 0 );
	}

	if ( ( info.m_nStage != -1 ) && ( params[info.m_nStage]->GetIntValue() == 0 ) )
	{
		SET_FLAGS2( MATERIAL_VAR2_NEEDS_POWER_OF_TWO_FRAME_BUFFER_TEXTURE );
	}

	if ( ( info.m_nPortalOpenAmount != -1 ) && ( !params[info.m_nPortalOpenAmount]->IsDefined() ) )
	{
		params[info.m_nPortalOpenAmount]->SetFloatValue( kDefaultPortalOpenAmount );
	}

	if ( ( info.m_nPortalStatic != -1 ) && ( !params[info.m_nPortalStatic]->IsDefined() ) )
	{
		params[info.m_nPortalStatic]->SetFloatValue( kDefaultPortalStatic );
	}

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nTime, 0.0f );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nUseOnStaticProp, 0 );
}

void InitPortalRefract( CBaseVSShader *pShader, IMaterialVar** params, PortalRefractVars_t &info )
{
	int nStage = IS_PARAM_DEFINED( info.m_nStage ) ? params[info.m_nStage]->GetIntValue() : 0;

	// Load textures
	if ( nStage == 2 ) // Only load textures for third stage
	{
		if ( (info.m_nPortalMaskTexture != -1) && params[info.m_nPortalMaskTexture]->IsDefined() )
		{
			pShader->LoadTexture( info.m_nPortalMaskTexture );
		}

		if ( (info.m_nPortalColorTexture != -1) && params[info.m_nPortalColorTexture]->IsDefined() )
		{
			pShader->LoadTexture( info.m_nPortalColorTexture, TEXTUREFLAGS_SRGB );
		}
	}
}

void DrawPortalRefract( CBaseVSShader *pShader, IMaterialVar** params, 
						   IShaderDynamicAPI *pShaderAPI, IShaderShadow* pShaderShadow, PortalRefractVars_t &info, VertexCompressionType_t vertexCompression )
{
	int nStage = IS_PARAM_DEFINED( info.m_nStage ) ? params[info.m_nStage]->GetIntValue() : 0;
	int nTinted = ((nStage == 2) && !IS_PARAM_DEFINED( info.m_nPortalColorTexture )) ? 1 : 0; //only in stage 2 with no color texture defined
	// On Leopard / 10.5.8, we can't do an sRGB read from a render target, so we must fake it in shader code
	bool bShaderSRGBRead = false;
	bool bUseOnStaticProp = IS_PARAM_DEFINED( info.m_nUseOnStaticProp ) ? ( params[info.m_nUseOnStaticProp]->GetIntValue() != 0 ): 0;
	
	if ( bUseOnStaticProp )
	{
		nStage = 2;
	}

	SHADOW_STATE
	{
		// Set stream format
		if ( !bUseOnStaticProp )
		{
			pShaderShadow->VertexShaderVertexFormat( VERTEX_POSITION | VERTEX_NORMAL, 2, NULL, 4 );
		}
		else
		{
			pShaderShadow->VertexShaderVertexFormat( VERTEX_POSITION | VERTEX_FORMAT_COMPRESSED, 1, NULL, 0 );
		}

		// Vertex Shader
		DECLARE_STATIC_VERTEX_SHADER( portal_refract_vs20 );
		SET_STATIC_VERTEX_SHADER_COMBO( STAGE, nStage );
		SET_STATIC_VERTEX_SHADER_COMBO( USEONSTATICPROP, bUseOnStaticProp );
		SET_STATIC_VERTEX_SHADER( portal_refract_vs20 );

		// On Leopard / 10.5.8, we can't do an sRGB read from a render target, so we must fake it in shader code
		bool bShaderSRGBRead = false;

		// Pixel Shader
		if( g_pHardwareConfig->SupportsPixelShaders_2_b() || g_pHardwareConfig->ShouldAlwaysUseShaderModel2bShaders() ) // Always send OpenGL / Posix down this path
		{
			DECLARE_STATIC_PIXEL_SHADER( portal_refract_ps20b );
			SET_STATIC_PIXEL_SHADER_COMBO( STAGE, nStage );
			SET_STATIC_PIXEL_SHADER_COMBO( TINTED, nTinted );
			SET_STATIC_PIXEL_SHADER_COMBO( SHADER_SRGB_READ, bShaderSRGBRead ? 1 : 0 );
			SET_STATIC_PIXEL_SHADER( portal_refract_ps20b );
		}
		else
		{
			DECLARE_STATIC_PIXEL_SHADER( portal_refract_ps20 );
			SET_STATIC_PIXEL_SHADER_COMBO( STAGE, nStage );
			SET_STATIC_PIXEL_SHADER_COMBO( TINTED, nTinted );
			SET_STATIC_PIXEL_SHADER( portal_refract_ps20 );
		}

		// Textures
		pShaderShadow->EnableTexture( SHADER_SAMPLER0, true ); // Refraction texture
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, !bShaderSRGBRead && !IsX360() );
		pShaderShadow->EnableTexture( SHADER_SAMPLER1, true ); // Noise
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, false ); // Not sRGB
		pShaderShadow->EnableTexture( SHADER_SAMPLER2, true ); // Color
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER2, true ); // sRGB
		pShaderShadow->EnableSRGBWrite( true ); //Stage 1 doesn't actually need srgb conversion, but it skips the software srgb solution anyway.

		// Sampler for nvidia's stereo hackery
		pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER3, false );

		// Enable alpha testing for all stages
		pShaderShadow->EnableAlphaTest( true );
		pShaderShadow->AlphaFunc( SHADER_ALPHAFUNC_GREATER, 0.5f );

		// Enable alpha blending for stage 2
		if ( nStage == 2 )
		{
			pShader->EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
			pShaderShadow->AlphaFunc( SHADER_ALPHAFUNC_GREATER, 1.0f/255.0f );
		}

		// Disable z-writes for all passes
		if ( nStage != 1 )
		{
			pShaderShadow->EnableDepthWrites( false );
		}
		else
		{
			pShaderShadow->EnableDepthWrites( true );
		}

		// Disable alpha-writes for all passes
		pShaderShadow->EnableAlphaWrites( false );

		pShaderShadow->EnablePolyOffset( SHADER_POLYOFFSET_DECAL );

		if( nStage == 2 )
		{
			pShader->DefaultFog();
		}
	}
	DYNAMIC_STATE
	{
		// Set Vertex Shader Combos
		DECLARE_DYNAMIC_VERTEX_SHADER( portal_refract_vs20 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
		SET_DYNAMIC_VERTEX_SHADER( portal_refract_vs20 );

		// Set Vertex Shader Constants 
		if ( IS_PARAM_DEFINED( info.m_nTextureTransform ) )
		{
			pShader->SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_1, info.m_nTextureTransform );
		}

		// Time % 1000
		float vPackedVsConst1[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		float flTime = IS_PARAM_DEFINED( info.m_nTime ) && params[info.m_nTime]->GetFloatValue() > 0.0f ? params[info.m_nTime]->GetFloatValue() : ShaderApiFast( pShaderAPI )->CurrentTime();
		vPackedVsConst1[0] = flTime;
		vPackedVsConst1[0] -= (float)( floor( vPackedVsConst1[0] / 1000.0f ) ) * 1000.0f;
		ShaderApiFast( pShaderAPI )->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, vPackedVsConst1, 1 );

		bool bNvidiaStereoActiveThisFrame = pShaderAPI->IsStereoActiveThisFrame();
		if ( bNvidiaStereoActiveThisFrame )
		{
			pShaderAPI->BindStandardTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, TEXTURE_STEREO_PARAM_MAP );
		}

		// Set Pixel Shader Combos
		if( g_pHardwareConfig->SupportsPixelShaders_2_b() || g_pHardwareConfig->ShouldAlwaysUseShaderModel2bShaders() ) // Always send OpenGL / Posix down this path
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( portal_refract_ps20b );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( D_NVIDIA_STEREO, bNvidiaStereoActiveThisFrame );
			SET_DYNAMIC_PIXEL_SHADER( portal_refract_ps20b );
		}
		else
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( portal_refract_ps20 );
			SET_DYNAMIC_PIXEL_SHADER( portal_refract_ps20 );
		}

		// Bind textures
		if ( nStage == 0 ) // Only bind frame buffer texture for first stage
		{
			ShaderApiFast( pShaderAPI )->BindStandardTexture( SHADER_SAMPLER0, ( !bShaderSRGBRead && !IsX360() ) ? TEXTURE_BINDFLAGS_SRGBREAD : TEXTURE_BINDFLAGS_NONE, TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0 ); // Refraction Map
		}
		else if ( nStage == 2 ) // Only load textures for third stage
		{
			pShader->BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, info.m_nPortalMaskTexture );
			if ( nTinted == 0 )
			{
				pShader->BindTexture( SHADER_SAMPLER2, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nPortalColorTexture );
			}
		}

		// Set Pixel Shader Constants 
		ShaderApiFast( pShaderAPI )->SetPixelShaderFogParams( 6 );

		// Set c0-c3 to contain four rows of ViewProj matrix
		VMatrix mView, mProj;
		ShaderApiFast( pShaderAPI )->GetMatrix( MATERIAL_VIEW, mView.m[0] );
		ShaderApiFast( pShaderAPI )->GetMatrix( MATERIAL_PROJECTION, mProj.m[0] );
		VMatrix mViewProj = mView * mProj;
		mViewProj = mViewProj.Transpose();
		ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( 0, mViewProj.m[0], 4 );

		// Portal open amount
		float vPackedConst4[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		vPackedConst4[0] = ( IS_PARAM_DEFINED( info.m_nPortalOpenAmount ) ? params[info.m_nPortalOpenAmount]->GetFloatValue() : kDefaultPortalOpenAmount );
		vPackedConst4[1] = 1.0f - ( IS_PARAM_DEFINED( info.m_nPortalStatic ) ? params[info.m_nPortalStatic]->GetFloatValue() : kDefaultPortalStatic );
		vPackedConst4[2] = ( IS_PARAM_DEFINED( info.m_nPortalColorScale ) ? params[info.m_nPortalColorScale]->GetFloatValue() : kDefaultPortalColorScale );
		
		//vPackedConst4[0] = 0.6f;
		//DevMsg( "Refract Time: %f\n", vPackedConst4[0] );

		ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( 4, vPackedConst4, 1 );
		ShaderApiFast( pShaderAPI )->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3, vPackedConst4, 1 );

		// Camera position
		float vEyePos[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		ShaderApiFast( pShaderAPI )->GetWorldSpaceCameraPosition( vEyePos );
		ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( 5, vEyePos, 1 );

		// Tinting
		if ( nTinted )
		{
			ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( 7, IS_PARAM_DEFINED( info.m_nPortalColorGradientDark ) ? params[info.m_nPortalColorGradientDark]->GetVecValue() : kDefaultPortalColorGradientDark, 1 );
			ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( 8, IS_PARAM_DEFINED( info.m_nPortalColorGradientLight ) ? params[info.m_nPortalColorGradientLight]->GetVecValue() : kDefaultPortalColorGradientLight, 1 );
		}

		// Get viewport and render target dimensions and set shader constant to do a 2D mad
		int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
		pShaderAPI->GetCurrentViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );

		int nRtWidth, nRtHeight;
		pShaderAPI->GetCurrentRenderTargetDimensions( nRtWidth, nRtHeight );

		float vViewportMad[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
		vViewportMad[0] = ( float )nViewportWidth / ( float )nRtWidth;
		vViewportMad[1] = ( float )nViewportHeight / ( float )nRtHeight;
		vViewportMad[2] = ( float )nViewportX / ( float )nRtWidth;
		vViewportMad[3] = ( float )nViewportY / ( float )nRtHeight;
		ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( 9, vViewportMad, 1 );

		int nPortalRecursionDepth = ShaderApiFast( pShaderAPI )->GetIntRenderingParameter( INT_RENDERPARM_PORTAL_RECURSION_DEPTH );

		if( g_pHardwareConfig->UseFastClipping() )
		{
			vPackedConst4[0] = ( nPortalRecursionDepth > 0 ) ? 1.0f : 0.0f;
		}
		else
		{
			vPackedConst4[0] = ( nPortalRecursionDepth > 1 )? 1.0f : 0.0f;
		}
		vPackedConst4[1] = 0.0f;
		vPackedConst4[2] = 0.0f;
		vPackedConst4[3] = 0.0f;
		ShaderApiFast( pShaderAPI )->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_4, vPackedConst4, 1 );
	}
	pShader->Draw();
}
