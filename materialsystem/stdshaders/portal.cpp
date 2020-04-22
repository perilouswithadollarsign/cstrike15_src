//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "BaseVSShader.h"
#include "portal_vs20.inc"
#include "portal_ps20.inc"
#include "portal_ps20b.inc"
#include "convar.h"
#include "cpp_shader_constant_register_map.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


DEFINE_FALLBACK_SHADER( Portal, Portal_DX90 )

BEGIN_VS_SHADER( Portal_DX90, 
				"Help for Portal shader" )

				BEGIN_SHADER_PARAMS
				SHADER_PARAM_OVERRIDE( COLOR, SHADER_PARAM_TYPE_COLOR, "{255 255 255}", "unused", SHADER_PARAM_NOT_EDITABLE )
				SHADER_PARAM_OVERRIDE( ALPHA, SHADER_PARAM_TYPE_FLOAT, "1.0", "unused", SHADER_PARAM_NOT_EDITABLE )
				SHADER_PARAM( STATICAMOUNT, SHADER_PARAM_TYPE_FLOAT, "0.0", "Amount of the static blend texture to blend into the base texture" )
				SHADER_PARAM( STATICBLENDTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "When adding static, this is the texture that gets blended in" )
				SHADER_PARAM( STATICBLENDTEXTUREFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "" )
				SHADER_PARAM( ALPHAMASKTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "An alpha mask for odd shaped portals" )
				SHADER_PARAM( ALPHAMASKTEXTUREFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "" )
				SHADER_PARAM( RENDERFIXZ, SHADER_PARAM_TYPE_INTEGER, "0", "Special depth handling, intended for rendering bug workarounds for extremely close polygons" )
				SHADER_PARAM( USEALTERNATEVIEWMATRIX, SHADER_PARAM_TYPE_INTEGER, "1", "Use the alternate view matrix instead of the current view matrix" )
				SHADER_PARAM( ALTERNATEVIEWMATRIX, SHADER_PARAM_TYPE_MATRIX, "0", "The alternate view matrix to use when $usealternateviewmatrix is enabled" )
				END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS( MATERIAL_VAR_TRANSLUCENT );
		if( !params[BASETEXTURE]->IsDefined() )
		{
			SET_FLAGS2( MATERIAL_VAR2_NEEDS_POWER_OF_TWO_FRAME_BUFFER_TEXTURE );
		}
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		if ( params[BASETEXTURE]->IsDefined() )
		{
			if ( IsGameConsole() )
			{
				// prevent unused rt access
				IMaterialVar* pNameVar = params[BASETEXTURE];
				const char *pStringValue = pNameVar->GetStringValue();
				if ( !V_stricmp( pStringValue, "_rt_portal1" ) || !V_stricmp( pStringValue, "_rt_portal2" ) )
				{
					pNameVar->SetStringValue( "white" );
				}
			}
			LoadTexture( BASETEXTURE, TEXTUREFLAGS_SRGB );
		}

		if ( params[STATICBLENDTEXTURE]->IsDefined() )
			LoadTexture( STATICBLENDTEXTURE );	
		if ( params[ALPHAMASKTEXTURE]->IsDefined() )
			LoadTexture( ALPHAMASKTEXTURE );

		if ( !params[STATICAMOUNT]->IsDefined() )
			params[STATICAMOUNT]->SetFloatValue( 0.0f );

		if ( !params[STATICAMOUNT]->IsDefined() )
			params[STATICAMOUNT]->SetFloatValue( 0.0f );

		if ( !params[STATICBLENDTEXTURE]->IsDefined() )
			params[STATICBLENDTEXTURE]->SetIntValue( 0 );
		if ( !params[STATICBLENDTEXTUREFRAME]->IsDefined() )
			params[STATICBLENDTEXTUREFRAME]->SetIntValue( 0 );

		if ( !params[ALPHAMASKTEXTURE]->IsDefined() )
			params[ALPHAMASKTEXTURE]->SetIntValue( 0 );
		if ( !params[ALPHAMASKTEXTUREFRAME]->IsDefined() )
			params[ALPHAMASKTEXTUREFRAME]->SetIntValue( 0 );

		if ( !params[RENDERFIXZ]->IsDefined() )
			params[RENDERFIXZ]->SetIntValue( 0 );

		if ( !params[USEALTERNATEVIEWMATRIX]->IsDefined() )
			params[USEALTERNATEVIEWMATRIX]->SetIntValue( 0 );

		if ( !params[ALTERNATEVIEWMATRIX]->IsDefined() )
		{
			VMatrix matIdentity;
			matIdentity.Identity();
			params[ALTERNATEVIEWMATRIX]->SetMatrixValue( matIdentity );
		}
	}

	SHADER_DRAW
	{
		bool bStaticBlendTexture = params[STATICBLENDTEXTURE]->IsTexture();
		bool bAlphaMaskTexture = ( params[ALPHAMASKTEXTURE]->IsTexture()? 1 : 0 );
		
		float fStaticAmount = params[STATICAMOUNT]->GetFloatValue();
		
		SHADOW_STATE
		{
			SetInitialShadowState();
			FogToFogColor();

			if( params[RENDERFIXZ]->GetIntValue() == 0 )
			{
				//pShaderShadow->EnablePolyOffset( SHADER_POLYOFFSET_DECAL ); //a portal is effectively a decal on top of a wall
				pShaderShadow->DepthFunc( SHADER_DEPTHFUNC_NEAREROREQUAL );
			}
			else
			{
				pShaderShadow->EnablePolyOffset( SHADER_POLYOFFSET_DISABLE );
				pShaderShadow->DepthFunc( SHADER_DEPTHFUNC_ALWAYS );
				pShaderShadow->EnableDepthTest( false );
				pShaderShadow->EnableDepthWrites( false );
			}

			pShaderShadow->EnableAlphaTest( true );

			if( bAlphaMaskTexture )
			{
				pShaderShadow->EnableBlending( true );
				pShaderShadow->BlendFunc( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
			}
			else
			{
				pShaderShadow->EnableBlending( false );
			}

			pShaderShadow->EnableSRGBWrite( true );

			int fmt = VERTEX_POSITION | VERTEX_NORMAL;
			int userDataSize = 0;
			int	iTexCoords = 2;
			if( IS_FLAG_SET( MATERIAL_VAR_MODEL ) )
			{
				userDataSize = 4;				
			}
			else
			{
				fmt |= VERTEX_TANGENT_S | VERTEX_TANGENT_T;
			}
			pShaderShadow->VertexShaderVertexFormat( fmt, iTexCoords, NULL, userDataSize );

			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, true );

			if( bStaticBlendTexture || bAlphaMaskTexture )
				pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );

			if( bStaticBlendTexture && bAlphaMaskTexture )
				pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
			
			// Sampler for nvidia's stereo hackery
			pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER3, false );

			DECLARE_STATIC_VERTEX_SHADER( portal_vs20 );
			SET_STATIC_VERTEX_SHADER_COMBO( HASALPHAMASK, bAlphaMaskTexture );
			SET_STATIC_VERTEX_SHADER_COMBO( HASSTATICTEXTURE, bStaticBlendTexture );
			SET_STATIC_VERTEX_SHADER_COMBO( USEALTERNATEVIEW, (params[USEALTERNATEVIEWMATRIX]->GetIntValue() != 0) );
			SET_STATIC_VERTEX_SHADER( portal_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( portal_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( HASALPHAMASK, bAlphaMaskTexture );
				SET_STATIC_PIXEL_SHADER_COMBO( HASSTATICTEXTURE, bStaticBlendTexture );
				SET_STATIC_PIXEL_SHADER( portal_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( portal_ps20 );
				SET_STATIC_PIXEL_SHADER_COMBO( HASALPHAMASK, bAlphaMaskTexture );
				SET_STATIC_PIXEL_SHADER_COMBO( HASSTATICTEXTURE, bStaticBlendTexture );
				SET_STATIC_PIXEL_SHADER( portal_ps20 );
			}

		}
		DYNAMIC_STATE
		{
			pShaderAPI->SetDefaultState();

			//x is static, y is inverse static
			float pc0[4] = { fStaticAmount, 1.0f - fStaticAmount, 0.0f, 0.0f };
			pShaderAPI->SetPixelShaderConstant( 0, pc0 );

			pShaderAPI->SetPixelShaderFogParams( PSREG_FOG_PARAMS );

			float vEyePos_SpecExponent[4];
			pShaderAPI->GetWorldSpaceCameraPosition( vEyePos_SpecExponent );
			vEyePos_SpecExponent[3] = 0.0f;
			pShaderAPI->SetPixelShaderConstant( PSREG_EYEPOS_SPEC_EXPONENT, vEyePos_SpecExponent, 1 );

			if ( params[BASETEXTURE]->IsTexture() )
				BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, BASETEXTURE, FRAME );
			else
				pShaderAPI->BindStandardTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0 );

			bool bHasStatic = (fStaticAmount > 0.0f);
			bool bUsingStaticTexture = (bStaticBlendTexture && bHasStatic);

			if ( bAlphaMaskTexture )
			{
				BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, ALPHAMASKTEXTURE, ALPHAMASKTEXTUREFRAME );
				if ( bUsingStaticTexture )
					BindTexture( SHADER_SAMPLER2, TEXTURE_BINDFLAGS_NONE, STATICBLENDTEXTURE, STATICBLENDTEXTUREFRAME );
			}
			else
			{
				if ( bUsingStaticTexture )
					BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, STATICBLENDTEXTURE, STATICBLENDTEXTUREFRAME );
			}

			if ( params[USEALTERNATEVIEWMATRIX]->GetIntValue() != 0 )
			{
				const VMatrix &matCustomView = params[ALTERNATEVIEWMATRIX]->GetMatrixValue();
				
				VMatrix matProj;
				pShaderAPI->GetMatrix( MATERIAL_PROJECTION, matProj.Base() );
				MatrixTranspose( matProj, matProj );

				VMatrix matFinal;
				MatrixMultiply( matProj, matCustomView, matFinal );
#ifdef _PS3
				// PS3's Cg likes things in row-major rather than column-major
				MatrixTranspose( matFinal, matFinal );
#endif // _PS3
				pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, matFinal.Base(), 4 );
			}

			// Get viewport and render target dimensions and set shader constant to do a 2D mad
			{
				int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
				pShaderAPI->GetCurrentViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );

				int nRtWidth, nRtHeight;
				pShaderAPI->GetCurrentRenderTargetDimensions( nRtWidth, nRtHeight );

				float vViewportMad[4];
				vViewportMad[0] = ( float )nViewportWidth / ( float )nRtWidth;
				vViewportMad[1] = ( float )nViewportHeight / ( float )nRtHeight;
				vViewportMad[2] = ( float )nViewportX / ( float )nRtWidth;
				vViewportMad[3] = ( float )nViewportY / ( float )nRtHeight;

				pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_4, vViewportMad, 1 );
			}

			int nPortalRecursionDepth = ShaderApiFast( pShaderAPI )->GetIntRenderingParameter( INT_RENDERPARM_PORTAL_RECURSION_DEPTH );
			float vPackedConst4[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			if( g_pHardwareConfig->UseFastClipping() )
			{
				vPackedConst4[0] = ( nPortalRecursionDepth > 0 ) ? 1.0f : 0.0f;
			}
			else
			{
				vPackedConst4[0] = ( nPortalRecursionDepth > 1 )? 1.0f : 0.0f;
			}
			ShaderApiFast( pShaderAPI )->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_5, vPackedConst4, 1 );

			bool bNvidiaStereoActiveThisFrame = pShaderAPI->IsStereoActiveThisFrame();
			if ( bNvidiaStereoActiveThisFrame )
			{
				pShaderAPI->BindStandardTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, TEXTURE_STEREO_PARAM_MAP );
			}

			DECLARE_DYNAMIC_VERTEX_SHADER( portal_vs20 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( ADDSTATIC, bHasStatic );
			SET_DYNAMIC_VERTEX_SHADER( portal_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( portal_ps20b );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( ADDSTATIC, bHasStatic );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( D_NVIDIA_STEREO, bNvidiaStereoActiveThisFrame );
				SET_DYNAMIC_PIXEL_SHADER( portal_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( portal_ps20 );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( ADDSTATIC, bHasStatic );
				SET_DYNAMIC_PIXEL_SHADER( portal_ps20 );
			}
		}

		Draw();		
	}

END_SHADER


