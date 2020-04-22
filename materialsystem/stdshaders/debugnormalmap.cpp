//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"
#include "convar.h"

#include "unlitgeneric_vs20.inc"
#include "unlitgeneric_ps20.inc"
#include "unlitgeneric_ps20b.inc"

#if !defined( _X360 ) && !defined( _PS3 )
	#include "unlitgeneric_ps30.inc"
	#include "unlitgeneric_vs30.inc"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar mat_displacementmap( "mat_displacementmap", "1", FCVAR_CHEAT );

BEGIN_VS_SHADER_FLAGS( DebugNormalMap, "Help for DebugNormalMap", SHADER_NOT_EDITABLE )
			  
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( BUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "shadertest/WorldDiffuseBumpMap_bump", "bump map" )
		SHADER_PARAM( BUMPFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $bumpmap" )
		SHADER_PARAM( BUMPTRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$bumpmap texcoord transform" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
	}

	SHADER_INIT
	{
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );

			// Set stream format (note that this shader supports compression)
			unsigned int flags = VERTEX_POSITION | VERTEX_FORMAT_COMPRESSED;
			int nTexCoordCount = 1;
			int userDataSize = 0;
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );

#if !defined( _X360 ) && !defined( _PS3 )
			if ( !g_pHardwareConfig->HasFastVertexTextures() )
#endif
			{
				DECLARE_STATIC_VERTEX_SHADER( unlitgeneric_vs20 );
				SET_STATIC_VERTEX_SHADER_COMBO( VERTEXCOLOR, 0 );
				SET_STATIC_VERTEX_SHADER( unlitgeneric_vs20 );

				if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
				{
					DECLARE_STATIC_PIXEL_SHADER( unlitgeneric_ps20b );
					SET_STATIC_PIXEL_SHADER( unlitgeneric_ps20b );
				}
				else
				{
					DECLARE_STATIC_PIXEL_SHADER( unlitgeneric_ps20 );
					SET_STATIC_PIXEL_SHADER( unlitgeneric_ps20 );
				}
			}
#if !defined( _X360 ) && !defined( _PS3 )
			else
			{
				DECLARE_STATIC_VERTEX_SHADER( unlitgeneric_vs30 );
				SET_STATIC_VERTEX_SHADER_COMBO( VERTEXCOLOR, 0 );
				SET_STATIC_VERTEX_SHADER( unlitgeneric_vs30 );

				DECLARE_STATIC_PIXEL_SHADER( unlitgeneric_ps30 );
				SET_STATIC_PIXEL_SHADER( unlitgeneric_ps30 );
			}
#endif

		}
		DYNAMIC_STATE
		{
			if ( params[BUMPMAP]->IsTexture() )
			{
				BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, BUMPMAP, BUMPFRAME );
			}
			else
			{
				pShaderAPI->BindStandardTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, TEXTURE_NORMALMAP_FLAT );
			}
			SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, BUMPTRANSFORM );

#if !defined( _X360 ) && !defined( _PS3 )
			if ( !g_pHardwareConfig->HasFastVertexTextures() )
#endif
			{
				DECLARE_DYNAMIC_VERTEX_SHADER( unlitgeneric_vs20 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( TESSELLATION, 0 );
				SET_DYNAMIC_VERTEX_SHADER( unlitgeneric_vs20 );

				if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
				{
					DECLARE_DYNAMIC_PIXEL_SHADER( unlitgeneric_ps20b );
					SET_DYNAMIC_PIXEL_SHADER( unlitgeneric_ps20b );
				}
				else
				{
					DECLARE_DYNAMIC_PIXEL_SHADER( unlitgeneric_ps20 );
					SET_DYNAMIC_PIXEL_SHADER( unlitgeneric_ps20 );
				}
			}
#if !defined( _X360 ) && !defined( _PS3 )
			else
			{
				TessellationMode_t nTessellationMode = pShaderAPI->GetTessellationMode();
				if ( nTessellationMode != TESSELLATION_MODE_DISABLED )
				{
					pShaderAPI->BindStandardVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER1, TEXTURE_SUBDIVISION_PATCHES );

					bool bHasDisplacement = false; // TODO
					float vSubDDimensions[4] = { 1.0f/pShaderAPI->GetSubDHeight(), bHasDisplacement && mat_displacementmap.GetBool()? 1.0f : 0.0f, 0.0f, 0.0f };
					pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_8, vSubDDimensions );

// JasonM - revisit this later...requires plumbing in a separate vertex texture param type??
//					bool bHasDisplacement = (info.m_nDisplacementMap != -1) && params[info.m_nDisplacementMap]->IsTexture();
//					if( bHasDisplacement )
//					{
//						pShader->BindVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER2, info.m_nDisplacementMap );
//					}
//					else
//					{
//						pShaderAPI->BindStandardVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER2, VERTEX_TEXTURE_BLACK );
//					}
				}
				DECLARE_DYNAMIC_VERTEX_SHADER( unlitgeneric_vs30 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( TESSELLATION, nTessellationMode );
				SET_DYNAMIC_VERTEX_SHADER( unlitgeneric_vs30 );

				DECLARE_DYNAMIC_PIXEL_SHADER( unlitgeneric_ps30 );
				SET_DYNAMIC_PIXEL_SHADER( unlitgeneric_ps30 );
			}
#endif
			
		}
		Draw();
	}
END_SHADER

