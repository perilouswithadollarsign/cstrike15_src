//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"
#include "cpp_shader_constant_register_map.h"

#include "decalmodulate_vs20.inc"
#include "decalmodulate_ps20.inc"
#include "decalmodulate_ps20b.inc"

#if !defined( _X360 ) && !defined( _PS3 )
#include "decalmodulate_vs30.inc"
#include "decalmodulate_ps30.inc"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DEFINE_FALLBACK_SHADER( DecalModulate, DecalModulate_DX9 )

BEGIN_VS_SHADER( DecalModulate_dx9, 
			  "Help for DecalModulate_dx9" )
			  
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( FOGEXPONENT, SHADER_PARAM_TYPE_FLOAT, "0.4", "exponent to tweak fog fade" )
		SHADER_PARAM( FOGSCALE, SHADER_PARAM_TYPE_FLOAT, "1.0", "scale to tweak fog fade" )
        SHADER_PARAM( FOGFADESTART, SHADER_PARAM_TYPE_FLOAT, "0", "fog amount at which to start fading decal" )
		SHADER_PARAM( FOGFADEEND, SHADER_PARAM_TYPE_FLOAT, "0", "fog amount at which to end fading decal" )
	END_SHADER_PARAMS
	
	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT_PARAMS()
	{
		if( !params[ FOGEXPONENT ]->IsDefined() )
		{
			params[ FOGEXPONENT ]->SetFloatValue( 0.4f );
		}

		if( !params[ FOGSCALE ]->IsDefined() )
		{
			params[ FOGSCALE ]->SetFloatValue( 1.0f );
		}

		if( !params[ FOGFADESTART]->IsDefined() )
		{
			params[ FOGFADESTART ]->SetFloatValue( 0 );
		}
		if( !params[ FOGFADEEND ]->IsDefined() )
		{
			params[ FOGFADEEND ]->SetFloatValue( 0.0f );
		}

		SET_FLAGS( MATERIAL_VAR_NO_DEBUG_OVERRIDE );

#if !defined( _X360 ) && !defined( _PS3 )
		if ( g_pHardwareConfig->HasFastVertexTextures() )
		{
			// The vertex shader uses the vertex id stream
			SET_FLAGS2( MATERIAL_VAR2_USES_VERTEXID );
			SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
		}
#endif
	}

	SHADER_INIT
	{
		LoadTexture( BASETEXTURE );
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			pShaderShadow->EnableAlphaTest( true );
			pShaderShadow->AlphaFunc( SHADER_ALPHAFUNC_GREATER, 0.0f );
			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnablePolyOffset( SHADER_POLYOFFSET_DECAL );
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );

			// Be sure not to write to dest alpha
			pShaderShadow->EnableAlphaWrites( false );

			// SRGB conversions hose the blend on some hardware, so keep we everything in gamma space
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, false );
			pShaderShadow->EnableSRGBWrite( false );

			pShaderShadow->EnableBlending( true );
			pShaderShadow->BlendFunc( SHADER_BLEND_DST_COLOR, SHADER_BLEND_SRC_COLOR );
			pShaderShadow->DisableFogGammaCorrection( true ); //fog should stay exactly middle grey
			FogToGrey();

			int nLightingPreviewMode = IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER0 ) + 2 * IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER1 );

			bool bHasVertexAlpha = IS_FLAG_SET( MATERIAL_VAR_VERTEXCOLOR ) && IS_FLAG_SET( MATERIAL_VAR_VERTEXALPHA );
			
			bool bHasFogFade = ( params[FOGFADEEND]->GetFloatValue() > 0.0f );

#if !defined( _X360 ) && !defined( _PS3 )
			if ( !g_pHardwareConfig->HasFastVertexTextures() )
#endif
			{
				DECLARE_STATIC_VERTEX_SHADER( decalmodulate_vs20 );
				SET_STATIC_VERTEX_SHADER_COMBO( VERTEXCOLOR,  bHasVertexAlpha );
				SET_STATIC_VERTEX_SHADER_COMBO( LIGHTING_PREVIEW, nLightingPreviewMode != 0 );
				SET_STATIC_VERTEX_SHADER( decalmodulate_vs20 );

				if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
				{
					DECLARE_STATIC_PIXEL_SHADER( decalmodulate_ps20b );
					SET_STATIC_PIXEL_SHADER_COMBO( VERTEXALPHA,  bHasVertexAlpha );
					SET_STATIC_PIXEL_SHADER_COMBO( FOGFADE, bHasFogFade );
					SET_STATIC_PIXEL_SHADER( decalmodulate_ps20b );
				}
				else
				{
					DECLARE_STATIC_PIXEL_SHADER( decalmodulate_ps20 );
					SET_STATIC_PIXEL_SHADER_COMBO( VERTEXALPHA,  bHasVertexAlpha );
					SET_STATIC_PIXEL_SHADER_COMBO( FOGFADE, bHasFogFade );
					SET_STATIC_PIXEL_SHADER( decalmodulate_ps20 );
				}
			}
#if !defined( _X360 ) && !defined( _PS3 )
			else
			{
				DECLARE_STATIC_VERTEX_SHADER( decalmodulate_vs30 );
				SET_STATIC_VERTEX_SHADER_COMBO( VERTEXCOLOR,  bHasVertexAlpha );
				SET_STATIC_VERTEX_SHADER_COMBO( LIGHTING_PREVIEW, nLightingPreviewMode != 0 );
				SET_STATIC_VERTEX_SHADER( decalmodulate_vs30 );

				DECLARE_STATIC_PIXEL_SHADER( decalmodulate_ps30 );
				SET_STATIC_PIXEL_SHADER_COMBO( VERTEXALPHA,  bHasVertexAlpha );
				SET_STATIC_PIXEL_SHADER_COMBO( FOGFADE, bHasFogFade );
				SET_STATIC_PIXEL_SHADER( decalmodulate_ps30 );
			}
#endif

			// Set stream format (note that this shader supports compression)
			unsigned int flags = VERTEX_POSITION | VERTEX_FORMAT_COMPRESSED;

			if ( bHasVertexAlpha )
			{
				flags |= VERTEX_COLOR;
			}

#if !defined( _X360 ) && !defined( _PS3 )
			// The VS30 shader offsets decals along the normal (for morphed geom)
			flags |= g_pHardwareConfig->HasFastVertexTextures() ? VERTEX_NORMAL : 0;
#endif
			int pTexCoordDim[3] = { 2, 0, 3 };
			int nTexCoordCount = 1;
			int userDataSize = 0;

#if !defined( _X360 ) && !defined( _PS3 )
			if ( g_pHardwareConfig->HasFastVertexTextures() )
			{
				nTexCoordCount = 3;
			}
#endif

			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, pTexCoordDim, userDataSize );
		}
		DYNAMIC_STATE
		{
			if ( pShaderAPI->InFlashlightMode() && !( IsX360() || IsPS3() ) )
			{
				// Don't draw anything for the flashlight pass
				Draw( false );
				return;
			}

			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, BASETEXTURE, FRAME );

			// Set an identity base texture transformation
			Vector4D transformation[2];
			transformation[0].Init( 1.0f, 0.0f, 0.0f, 0.0f );
			transformation[1].Init( 0.0f, 1.0f, 0.0f, 0.0f );
		 	pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, transformation[0].Base(), 2 ); 

			pShaderAPI->SetPixelShaderFogParams( PSREG_FOG_PARAMS );					

			float vEyePos_SpecExponent[4];
			pShaderAPI->GetWorldSpaceCameraPosition( vEyePos_SpecExponent );
			vEyePos_SpecExponent[3] = 0.0f;
			pShaderAPI->SetPixelShaderConstant( PSREG_EYEPOS_SPEC_EXPONENT, vEyePos_SpecExponent, 1 );

			// fog tweaks
			float fConsts[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			fConsts[0] = params[ FOGEXPONENT ]->GetFloatValue();
			fConsts[1] = params[ FOGSCALE ]->GetFloatValue();
			fConsts[2] = params[ FOGFADESTART ]->GetFloatValue();
			fConsts[3] = params[ FOGFADEEND ]->GetFloatValue();
			pShaderAPI->SetPixelShaderConstant( 0, fConsts );

#if !defined( _X360 ) && !defined( _PS3 )
			if ( !g_pHardwareConfig->HasFastVertexTextures() )
#endif
			{
				DECLARE_DYNAMIC_VERTEX_SHADER( decalmodulate_vs20 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, 0 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
//				SET_DYNAMIC_VERTEX_SHADER_COMBO( TESSELLATION, 0 );             // JasonM TODO: set this appropriately when we care about decals on subds				
				SET_DYNAMIC_VERTEX_SHADER( decalmodulate_vs20 );

				if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
				{
					DECLARE_DYNAMIC_PIXEL_SHADER( decalmodulate_ps20b );
					SET_DYNAMIC_PIXEL_SHADER( decalmodulate_ps20b );
				}
				else
				{
					DECLARE_DYNAMIC_PIXEL_SHADER( decalmodulate_ps20 );
					SET_DYNAMIC_PIXEL_SHADER( decalmodulate_ps20 );
				}
			}
#if !defined( _X360 ) && !defined( _PS3 )
			else
			{
				SetHWMorphVertexShaderState( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6, VERTEX_SHADER_SHADER_SPECIFIC_CONST_7, SHADER_VERTEXTEXTURE_SAMPLER0 );

				DECLARE_DYNAMIC_VERTEX_SHADER( decalmodulate_vs30 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
//				SET_DYNAMIC_VERTEX_SHADER_COMBO( TESSELLATION, 0 );             // JasonM TODO: set this appropriately when we care about decals on subds				
				SET_DYNAMIC_VERTEX_SHADER( decalmodulate_vs30 );

				DECLARE_DYNAMIC_PIXEL_SHADER( decalmodulate_ps30 );
				SET_DYNAMIC_PIXEL_SHADER( decalmodulate_ps30 );

				bool bUnusedTexCoords[3] = { false, false, !pShaderAPI->IsHWMorphingEnabled() };
				pShaderAPI->MarkUnusedVertexFields( 0, 3, bUnusedTexCoords );
			}
#endif
		}
		Draw( );
	}
END_SHADER
