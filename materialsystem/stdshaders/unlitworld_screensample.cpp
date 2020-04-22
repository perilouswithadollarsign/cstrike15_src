//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "BaseVSShader.h"
#include "mathlib/vmatrix.h"
#include "common_hlsl_cpp_consts.h" // hack hack hack!
#include "convar.h"

#include "unlitworld_screensample_vs20.inc"
#include "unlitworld_screensample_ps20.inc"
#include "unlitworld_screensample_ps20b.inc"

#include "shaderlib/commandbuilder.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


BEGIN_VS_SHADER( UnLitWorld_ScreenSample, "Help for UnLitWorld_ScreenSample" )

	BEGIN_SHADER_PARAMS

		//always required
		SHADER_PARAM( BASETEXTURE,		SHADER_PARAM_TYPE_TEXTURE, "", "" )

	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_PARAM_STRING_IF_NOT_DEFINED( BASETEXTURE, "Dev/flat_normal" );
		
		SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		if ( params[BASETEXTURE]->IsDefined() )	{ LoadTexture( BASETEXTURE ); }
	}

	inline void DrawUnLitWorld_ScreenSample( IMaterialVar **params, IShaderShadow* pShaderShadow, IShaderDynamicAPI* pShaderAPI, int vertexCompression )
	{
		SHADOW_STATE
		{
			SetInitialShadowState( );

			if ( params[BASETEXTURE]->IsDefined() )			{ pShaderShadow->EnableTexture( SHADER_SAMPLER0, true ); }


			if ( IS_FLAG_SET(MATERIAL_VAR_ADDITIVE) )
			{
				EnableAlphaBlending( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
			}
			else
			{
				EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
			}

			//pShaderShadow->EnableBlending( true );
			//pShaderShadow->BlendFunc( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );

			//pShaderShadow->AlphaFunc( SHADER_ALPHAFUNC_ALWAYS, 0.0f );
			/*
			BlendFunc( SHADER_BLEND_ZERO, SHADER_BLEND_ZERO );
			BlendOp( SHADER_BLEND_OP_ADD );
			EnableBlendingSeparateAlpha( false );
			BlendFuncSeparateAlpha( SHADER_BLEND_ZERO, SHADER_BLEND_ZERO );
			BlendOpSeparateAlpha( SHADER_BLEND_OP_ADD );
			AlphaFunc( SHADER_ALPHAFUNC_GEQUAL, 0.7f );
			*/

			unsigned int flags = VERTEX_POSITION| VERTEX_FORMAT_COMPRESSED;
			flags |= VERTEX_NORMAL;

			int nTexCoordCount = 1;
			int userDataSize = 0;
			if (IS_FLAG_SET( MATERIAL_VAR_VERTEXCOLOR ))
			{
				flags |= VERTEX_COLOR;
			}
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );
			
			DECLARE_STATIC_VERTEX_SHADER( unlitworld_screensample_vs20 );
			SET_STATIC_VERTEX_SHADER( unlitworld_screensample_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( unlitworld_screensample_ps20b );
				SET_STATIC_PIXEL_SHADER( unlitworld_screensample_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( unlitworld_screensample_ps20 );
				SET_STATIC_PIXEL_SHADER( unlitworld_screensample_ps20 );
			}
			
			//pShaderShadow->EnableAlphaWrites( true );
			//pShaderShadow->EnableDepthWrites( true );
			pShaderShadow->EnableSRGBWrite( true );
			pShaderShadow->EnableBlending( true );
			//pShaderShadow->EnableAlphaTest( true );

			PI_BeginCommandBuffer();
			PI_SetModulationPixelShaderDynamicState_LinearColorSpace( 1 );
			PI_EndCommandBuffer();


		}
		DYNAMIC_STATE
		{
			pShaderAPI->SetDefaultState();

			SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, BASETEXTURETRANSFORM );

			if ( params[BASETEXTURE]->IsDefined() )			{ BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, BASETEXTURE, -1 ); }
			
			

			// Get viewport and render target dimensions and set shader constant to do a 2D mad
			int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
			pShaderAPI->GetCurrentViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );

			int nRtWidth, nRtHeight;
			pShaderAPI->GetCurrentRenderTargetDimensions( nRtWidth, nRtHeight );

			// Compute viewport mad that takes projection space coords (post divide by W) into normalized screenspace, taking into account the currently set viewport.
			float vViewportMad[4];
			vViewportMad[0] =  .5f * ( ( float )nViewportWidth / ( float )nRtWidth );
			vViewportMad[1] = -.5f * ( ( float )nViewportHeight / ( float )nRtHeight );
			vViewportMad[2] =  vViewportMad[0] + ( ( float )nViewportX / ( float )nRtWidth );
			vViewportMad[3] = -vViewportMad[1] + ( ( float )nViewportY / ( float )nRtHeight );
			pShaderAPI->SetPixelShaderConstant( 17, vViewportMad, 1 );

			float c18[4];
			pShaderAPI->GetWorldSpaceCameraPosition( c18 );
			c18[3] = 0;				
			pShaderAPI->SetPixelShaderConstant( 18, c18, 1 );

			float c0[4] = {		1.0f / (float)MAX(nViewportWidth, 1.0f) ,	// one X pixel in normalized 0..1 screenspace
								1.0f / (float)MAX(nViewportHeight, 1.0f) ,	// one Y pixel in normalized 0..1 screenspace
								0,
								0 };
				
			pShaderAPI->SetPixelShaderConstant( 0, c0, 1 );

			DECLARE_DYNAMIC_VERTEX_SHADER( unlitworld_screensample_vs20 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER( unlitworld_screensample_vs20 );
						
			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( unlitworld_screensample_ps20b );
				SET_DYNAMIC_PIXEL_SHADER( unlitworld_screensample_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( unlitworld_screensample_ps20 );
				SET_DYNAMIC_PIXEL_SHADER( unlitworld_screensample_ps20 );
			}

		}
		Draw();
	}

	SHADER_DRAW
	{
		DrawUnLitWorld_ScreenSample( params, pShaderShadow, pShaderAPI, vertexCompression );
	}

	//void ExecuteFastPath( int *dynVSIdx, int *dynPSIdx,  IMaterialVar** params, IShaderDynamicAPI * pShaderAPI, 
	//	VertexCompressionType_t vertexCompression, CBasePerMaterialContextData **pContextDataPtr, BOOL bCSMEnabled )
	//{
	//	DrawUnLitWorld_ScreenSample( params, pShaderShadow, pShaderAPI, vertexCompression );
	//}

	bool IsTranslucent( IMaterialVar **params ) const
	{
		return false;
	}

END_SHADER


