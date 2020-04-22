//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: A shader that builds the shadow using render-to-texture
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#include "BaseVSShader.h"
#include "convar.h"
#include "mathlib/vmatrix.h"

#include "unlitgeneric_vs20.inc"
#include "shadowbuildtexture_ps20.inc"
#include "shadowbuildtexture_ps20b.inc"

#if !defined( _X360 ) && !defined( _PS3 )
	#include "shadowbuildtexture_ps30.inc"
	#include "unlitgeneric_vs30.inc"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar mat_displacementmap( "mat_displacementmap", "1", FCVAR_CHEAT );

DEFINE_FALLBACK_SHADER( ShadowBuild, ShadowBuild_DX9 )

BEGIN_VS_SHADER_FLAGS( ShadowBuild_DX9, "Help for ShadowBuild", SHADER_NOT_EDITABLE )

	BEGIN_SHADER_PARAMS
		SHADER_PARAM( TRANSLUCENT_MATERIAL, SHADER_PARAM_TYPE_MATERIAL, "", "Points to a material to grab translucency from" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
		SET_FLAGS( MATERIAL_VAR_NO_DEBUG_OVERRIDE );
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		if (params[BASETEXTURE]->IsDefined())
		{
			LoadTexture( BASETEXTURE, TEXTUREFLAGS_SRGB );
		}
	}

	SHADER_DRAW
	{
		bool bHDR = g_pHardwareConfig->GetHDRType() != HDR_TYPE_NONE;
		
		// Snack important parameters from the original material
		// FIXME: What about alpha modulation? Need a solution for that
		ITexture *pTexture = NULL;
		IMaterialVar **ppTranslucentParams = NULL;
		if (params[TRANSLUCENT_MATERIAL]->IsDefined())
		{
			IMaterial *pMaterial = params[TRANSLUCENT_MATERIAL]->GetMaterialValue();
			if (pMaterial)
			{
				ppTranslucentParams = pMaterial->GetShaderParams();
				if ( ppTranslucentParams[BASETEXTURE]->IsTexture() )
				{
					pTexture = ppTranslucentParams[BASETEXTURE]->GetTextureValue();
				}
			}
		}
		
		SHADOW_STATE
		{
			// Add the alphas into the frame buffer
			EnableAlphaBlending( SHADER_BLEND_ONE, SHADER_BLEND_ONE );

			// Base texture.  We just use this for alpha, but enable SRGB read to make everything consistent.
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, pTexture || !bHDR );

			pShaderShadow->EnableSRGBWrite( true );

			pShaderShadow->EnableAlphaWrites( true );
			pShaderShadow->EnableDepthWrites( false );
	//		pShaderShadow->DepthFunc( SHADER_DEPTHFUNC_ALWAYS );
		pShaderShadow->EnableDepthTest( false );

#if defined( _PS3 )
			pShaderShadow->EnableDepthTest( false );
#else
#endif
			// Specify vertex format (note that this shader supports compression)
			unsigned int flags = VERTEX_POSITION | VERTEX_FORMAT_COMPRESSED;
			unsigned int nTexCoordCount = 1;
			unsigned int userDataSize = 0;
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );

#if !defined( _X360 ) && !defined( _PS3 )
			if ( !g_pHardwareConfig->HasFastVertexTextures() )
#endif
			{
				DECLARE_STATIC_VERTEX_SHADER( unlitgeneric_vs20 );
				SET_STATIC_VERTEX_SHADER_COMBO( VERTEXCOLOR, 0  );
				SET_STATIC_VERTEX_SHADER( unlitgeneric_vs20 );

				if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
				{
					DECLARE_STATIC_PIXEL_SHADER( shadowbuildtexture_ps20b );
					SET_STATIC_PIXEL_SHADER( shadowbuildtexture_ps20b );
				}
				else
				{
					DECLARE_STATIC_PIXEL_SHADER( shadowbuildtexture_ps20 );
					SET_STATIC_PIXEL_SHADER( shadowbuildtexture_ps20 );
				}
			}
#if !defined( _X360 ) && !defined( _PS3 )
			else
			{
				SET_FLAGS2( MATERIAL_VAR2_USES_VERTEXID );
				SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_TESSELLATION );

				DECLARE_STATIC_VERTEX_SHADER( unlitgeneric_vs30 );
				SET_STATIC_VERTEX_SHADER_COMBO( VERTEXCOLOR, 0  );
				SET_STATIC_VERTEX_SHADER( unlitgeneric_vs30 );

				DECLARE_STATIC_PIXEL_SHADER( shadowbuildtexture_ps30 );
				SET_STATIC_PIXEL_SHADER( shadowbuildtexture_ps30 );
			}
#endif
			PI_BeginCommandBuffer();
			PI_SetModulationVertexShaderDynamicState();
			PI_EndCommandBuffer();
		}
		DYNAMIC_STATE
		{
			if ( pTexture )
			{
				BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, pTexture, ppTranslucentParams[FRAME]->GetIntValue() );

				Vector4D transformation[2];
				const VMatrix &mat = ppTranslucentParams[BASETEXTURETRANSFORM]->GetMatrixValue();
				transformation[0].Init( mat[0][0], mat[0][1], mat[0][2], mat[0][3] );
				transformation[1].Init( mat[1][0], mat[1][1], mat[1][2], mat[1][3] );
				pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, transformation[0].Base(), 2 ); 
			}
			else
			{
				pShaderAPI->BindStandardTexture( SHADER_SAMPLER0, bHDR ? TEXTURE_BINDFLAGS_NONE : TEXTURE_BINDFLAGS_SRGBREAD, TEXTURE_LIGHTMAP_FULLBRIGHT );
			}

#if !defined( _X360 ) && !defined( _PS3 )
			TessellationMode_t nTessellationMode = TESSELLATION_MODE_DISABLED;
			if ( !g_pHardwareConfig->HasFastVertexTextures() )
#endif
			{
				// Compute the vertex shader index.
				DECLARE_DYNAMIC_VERTEX_SHADER( unlitgeneric_vs20 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( TESSELLATION, 0 );
				SET_DYNAMIC_VERTEX_SHADER( unlitgeneric_vs20 );

				if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
				{
					DECLARE_DYNAMIC_PIXEL_SHADER( shadowbuildtexture_ps20b );
					SET_DYNAMIC_PIXEL_SHADER( shadowbuildtexture_ps20b );
				}
				else
				{
					DECLARE_DYNAMIC_PIXEL_SHADER( shadowbuildtexture_ps20 );
					SET_DYNAMIC_PIXEL_SHADER( shadowbuildtexture_ps20 );
				}
			}
#if !defined( _X360 ) && !defined( _PS3 )
			else
			{
				nTessellationMode = pShaderAPI->GetTessellationMode();
				if ( nTessellationMode != TESSELLATION_MODE_DISABLED )
				{
					pShaderAPI->BindStandardVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER1, TEXTURE_SUBDIVISION_PATCHES );

					bool bHasDisplacement = false; // TODO
					float vSubDDimensions[4] = { 1.0f/pShaderAPI->GetSubDHeight(), bHasDisplacement && mat_displacementmap.GetBool() ? 1.0f : 0.0f, 0.0f, 0.0f };
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

				// Compute the vertex shader index.
				DECLARE_DYNAMIC_VERTEX_SHADER( unlitgeneric_vs30 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( TESSELLATION, nTessellationMode );
				SET_DYNAMIC_VERTEX_SHADER( unlitgeneric_vs30 );

				DECLARE_DYNAMIC_PIXEL_SHADER( shadowbuildtexture_ps30 );
				SET_DYNAMIC_PIXEL_SHADER( shadowbuildtexture_ps30 );
			}
#endif

			
		}
		Draw( );
	}
END_SHADER
