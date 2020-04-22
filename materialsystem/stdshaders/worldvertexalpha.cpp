//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"

#include "worldvertexalpha_ps20.inc"
#include "worldvertexalpha_ps20b.inc"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


BEGIN_VS_SHADER( WorldVertexAlpha, "Help for WorldVertexAlpha" )

	BEGIN_SHADER_PARAMS
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS2( MATERIAL_VAR2_LIGHTING_LIGHTMAP );
		SET_FLAGS2( MATERIAL_VAR2_BLEND_WITH_LIGHTMAP_ALPHA );
	}
	SHADER_INIT
	{
		// Load the base texture here!
		LoadTexture( BASETEXTURE );
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_DRAW
	{
		if( g_pHardwareConfig->SupportsVertexAndPixelShaders() && !UsingEditor( params ) )
		{
			// DX 9 version with HDR support

			// Pass 1
			SHADOW_STATE
			{
				SetInitialShadowState();

				pShaderShadow->EnableAlphaWrites( true );

				// Base time lightmap (Need two texture stages)
				pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
				pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
				pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );

				int fmt = VERTEX_POSITION;
				pShaderShadow->VertexShaderVertexFormat( fmt, 2, 0, 0 );

				pShaderShadow->EnableBlending( true );

				// Looks backwards, but this is done so that lightmap alpha = 1 when only
				// using 1 texture (needed for translucent displacements).
				pShaderShadow->BlendFunc( SHADER_BLEND_ONE_MINUS_SRC_ALPHA, SHADER_BLEND_SRC_ALPHA );
				pShaderShadow->EnableBlendingSeparateAlpha( true );
				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ZERO, SHADER_BLEND_SRC_ALPHA );

				worldvertexalpha_Static_Index vshIndex;
				pShaderShadow->SetVertexShader( "WorldVertexAlpha", vshIndex.GetIndex() );

				if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
				{
					DECLARE_STATIC_PIXEL_SHADER( worldvertexalpha_ps20b );
					SET_STATIC_PIXEL_SHADER_COMBO( PASS, 0 );
					SET_STATIC_PIXEL_SHADER( worldvertexalpha_ps20b );
				}
				else
				{
					DECLARE_STATIC_PIXEL_SHADER( worldvertexalpha_ps20 );
					SET_STATIC_PIXEL_SHADER_COMBO( PASS, 0 );
					SET_STATIC_PIXEL_SHADER( worldvertexalpha_ps20 );
				}


				FogToFogColor();
			}

			DYNAMIC_STATE
			{
				// Bind the base texture (Stage0) and lightmap (Stage1)
				BindTexture( SHADER_SAMPLER0, false, BASETEXTURE );
				pShaderAPI->BindStandardTexture( SHADER_SAMPLER1, false, TEXTURE_LIGHTMAP );

				worldvertexalpha_Dynamic_Index vshIndex;
				vshIndex.SetDOWATERFOG( pShaderAPI->GetSceneFogMode() == MATERIAL_FOG_LINEAR_BELOW_FOG_Z );
				pShaderAPI->SetVertexShaderIndex( vshIndex.GetIndex() );

				if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
				{
					DECLARE_DYNAMIC_PIXEL_SHADER( worldvertexalpha_ps20b );
					SET_DYNAMIC_PIXEL_SHADER( worldvertexalpha_ps20b );
				}
				else
				{
					DECLARE_DYNAMIC_PIXEL_SHADER( worldvertexalpha_ps20 );
					SET_DYNAMIC_PIXEL_SHADER( worldvertexalpha_ps20 );
				}
			}
			Draw();

			// Pass 2
			SHADOW_STATE
			{
				SetInitialShadowState();

				pShaderShadow->EnableAlphaWrites( true );
				pShaderShadow->EnableColorWrites( false );

				// Base time lightmap (Need two texture stages)
				pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
				pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
				pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );

				int fmt = VERTEX_POSITION;
				pShaderShadow->VertexShaderVertexFormat( fmt, 2, 0, 0 );

				pShaderShadow->EnableBlending( true );

				// Looks backwards, but this is done so that lightmap alpha = 1 when only
				// using 1 texture (needed for translucent displacements).
				pShaderShadow->BlendFunc( SHADER_BLEND_ONE_MINUS_SRC_ALPHA, SHADER_BLEND_SRC_ALPHA );
				pShaderShadow->EnableBlendingSeparateAlpha( true );
				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ONE, SHADER_BLEND_ONE );

				worldvertexalpha_Static_Index vshIndex;
				pShaderShadow->SetVertexShader( "WorldVertexAlpha", vshIndex.GetIndex() );

				if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
				{
					DECLARE_STATIC_PIXEL_SHADER( worldvertexalpha_ps20b );
					SET_STATIC_PIXEL_SHADER_COMBO( PASS, 1 );
					SET_STATIC_PIXEL_SHADER( worldvertexalpha_ps20b );
				}
				else
				{
					DECLARE_STATIC_PIXEL_SHADER( worldvertexalpha_ps20 );
					SET_STATIC_PIXEL_SHADER_COMBO( PASS, 1 );
					SET_STATIC_PIXEL_SHADER( worldvertexalpha_ps20 );
				}

				FogToFogColor();
			}

			DYNAMIC_STATE
			{
				// Bind the base texture (Stage0) and lightmap (Stage1)
				BindTexture( SHADER_SAMPLER0, BASETEXTURE );
				pShaderAPI->BindStandardTexture( SHADER_SAMPLER1, false, TEXTURE_LIGHTMAP );

				worldvertexalpha_Dynamic_Index vshIndex;
				vshIndex.SetDOWATERFOG( pShaderAPI->GetSceneFogMode() == MATERIAL_FOG_LINEAR_BELOW_FOG_Z );
				pShaderAPI->SetVertexShaderIndex( vshIndex.GetIndex() );

				if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
				{
					DECLARE_DYNAMIC_PIXEL_SHADER( worldvertexalpha_ps20b );
					SET_DYNAMIC_PIXEL_SHADER( worldvertexalpha_ps20b );
				}
				else
				{
					DECLARE_DYNAMIC_PIXEL_SHADER( worldvertexalpha_ps20 );
					SET_DYNAMIC_PIXEL_SHADER( worldvertexalpha_ps20 );
				}
			}
			Draw();
		}
	}
END_SHADER
