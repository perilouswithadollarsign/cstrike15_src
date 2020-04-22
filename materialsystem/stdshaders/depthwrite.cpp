//========= Copyright � 1996-2007, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#include "BaseVSShader.h"
#include "convar.h"

#include "depthwrite_ps20.inc"
#include "depthwrite_ps20b.inc"
#include "depthwrite_vs20.inc"

#if !defined( _X360 ) &&! defined( _PS3 )
#include "depthwrite_ps30.inc"
#include "depthwrite_vs30.inc"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

static ConVar mat_displacementmap( "mat_displacementmap", "1", FCVAR_CHEAT );

BEGIN_VS_SHADER_FLAGS( DepthWrite, "Help for Depth Write", SHADER_NOT_EDITABLE )

	BEGIN_SHADER_PARAMS
		SHADER_PARAM( ALPHATESTREFERENCE, SHADER_PARAM_TYPE_FLOAT, "", "Alpha reference value" )
		SHADER_PARAM( DISPLACEMENTMAP, SHADER_PARAM_TYPE_TEXTURE, "shadertest/BaseTexture", "Displacement map" )
		SHADER_PARAM( DISPLACEMENTWRINKLE, SHADER_PARAM_TYPE_BOOL, "0", "Displacement map contains wrinkle displacements")
		SHADER_PARAM( COLOR_DEPTH, SHADER_PARAM_TYPE_BOOL, "0", "Write depth as color" )
		
		// vertexlitgeneric tree sway animation control
		SHADER_PARAM( TREESWAY, SHADER_PARAM_TYPE_INTEGER, "0", "" );
		SHADER_PARAM( TREESWAYHEIGHT, SHADER_PARAM_TYPE_FLOAT, "1000", "" );
		SHADER_PARAM( TREESWAYSTARTHEIGHT, SHADER_PARAM_TYPE_FLOAT, "0.2", "" );
		SHADER_PARAM( TREESWAYRADIUS, SHADER_PARAM_TYPE_FLOAT, "300", "" );
		SHADER_PARAM( TREESWAYSTARTRADIUS, SHADER_PARAM_TYPE_FLOAT, "0.1", "" );
		SHADER_PARAM( TREESWAYSPEED, SHADER_PARAM_TYPE_FLOAT, "1", "" );
		SHADER_PARAM( TREESWAYSPEEDHIGHWINDMULTIPLIER, SHADER_PARAM_TYPE_FLOAT, "2", "" );
		SHADER_PARAM( TREESWAYSTRENGTH, SHADER_PARAM_TYPE_FLOAT, "10", "" );
		SHADER_PARAM( TREESWAYSCRUMBLESPEED, SHADER_PARAM_TYPE_FLOAT, "0.1", "" );
		SHADER_PARAM( TREESWAYSCRUMBLESTRENGTH, SHADER_PARAM_TYPE_FLOAT, "0.1", "" );
		SHADER_PARAM( TREESWAYSCRUMBLEFREQUENCY, SHADER_PARAM_TYPE_FLOAT, "0.1", "" );
		SHADER_PARAM( TREESWAYFALLOFFEXP, SHADER_PARAM_TYPE_FLOAT, "1.5", "" );
		SHADER_PARAM( TREESWAYSCRUMBLEFALLOFFEXP, SHADER_PARAM_TYPE_FLOAT, "1.0", "" );
		SHADER_PARAM( TREESWAYSPEEDLERPSTART, SHADER_PARAM_TYPE_FLOAT, "3", "" );
		SHADER_PARAM( TREESWAYSPEEDLERPEND, SHADER_PARAM_TYPE_FLOAT, "6", "" );
		SHADER_PARAM( TREESWAYSTATIC, SHADER_PARAM_TYPE_BOOL, "0", "" );
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );

#if !defined( CSTRIKE15 )
		if ( IsGameConsole() )
		{
			params[TREESWAY]->SetIntValue( 0 );
		}
#else
		if ( IsPlatformOSX() || IsPS3() )
		{
			params[TREESWAY]->SetIntValue( 0 );
		}
#endif
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		if ( params[DISPLACEMENTMAP]->IsDefined() )
		{
			LoadTexture( DISPLACEMENTMAP );
		}
	}

	SHADER_DRAW
	{
		bool bAlphaClip = IS_FLAG_SET( MATERIAL_VAR_ALPHATEST );
		int nTreeSwayMode = GetIntParam( TREESWAY, params, 0 );
		nTreeSwayMode = clamp( nTreeSwayMode, 0, 2 );
		bool bHasDisplacement = params[DISPLACEMENTMAP]->IsTexture();
#if !defined( PLATFORM_X360 ) && !defined( _PS3 )
		bool bHasDisplacementWrinkles = params[DISPLACEMENTWRINKLE]->GetIntValue() != 0;
#endif
		int nColorDepth = GetIntParam( COLOR_DEPTH, params, 0 );

		SHADOW_STATE
		{
			// Set stream format (note that this shader supports compression)
			unsigned int flags = VERTEX_POSITION | VERTEX_FORMAT_COMPRESSED;
			int nTexCoordCount = 1;
			int userDataSize = 0;
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );

			// Bias primitives when rendering into shadow map so we get slope-scaled depth bias
			// rather than having to apply a constant bias in the filtering shader later
			if ( nColorDepth == 0 )
			{
				pShaderShadow->EnablePolyOffset( SHADER_POLYOFFSET_SHADOW_BIAS );
			}

			// Turn off writes to color buffer since we always sample shadows from the DEPTH texture later
			// This gives us double-speed fill when rendering INTO the shadow map
			pShaderShadow->EnableColorWrites( nColorDepth == 1 );
			pShaderShadow->EnableAlphaWrites( false );
		
			// Turn off srgb writes to color depth buffer
			pShaderShadow->EnableSRGBWrite( false );

			// Don't backface cull unless alpha clipping, since this can cause artifacts when the
			// geometry is clipped by the flashlight near plane
			// If a material was already marked nocull, don't cull it
			pShaderShadow->EnableCulling( IS_FLAG_SET(MATERIAL_VAR_ALPHATEST) && !IS_FLAG_SET(MATERIAL_VAR_NOCULL) );



			if ( bHasDisplacement && IsPC() && g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				pShaderShadow->EnableVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER2, true );
			}



#if !defined( _X360 ) && !defined( _PS3 )
			if ( !g_pHardwareConfig->SupportsPixelShaders_3_0() )
#endif
			{
				DECLARE_STATIC_VERTEX_SHADER( depthwrite_vs20 );
				SET_STATIC_VERTEX_SHADER_COMBO( ONLY_PROJECT_POSITION, !bAlphaClip && IsX360() && !nColorDepth ); //360 needs to know if it *shouldn't* output texture coordinates to avoid shader patches
				SET_STATIC_VERTEX_SHADER_COMBO( COLOR_DEPTH, nColorDepth );
				SET_STATIC_VERTEX_SHADER_COMBO( TREESWAY, nTreeSwayMode );
				SET_STATIC_VERTEX_SHADER( depthwrite_vs20 );

				if ( bAlphaClip || g_pHardwareConfig->PlatformRequiresNonNullPixelShaders() || nColorDepth )
				{
					pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
					pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, true );

					if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
					{
						DECLARE_STATIC_PIXEL_SHADER( depthwrite_ps20b );
						SET_STATIC_PIXEL_SHADER_COMBO( COLOR_DEPTH, nColorDepth );
						SET_STATIC_PIXEL_SHADER( depthwrite_ps20b );
					}
					else
					{
						DECLARE_STATIC_PIXEL_SHADER( depthwrite_ps20 );
						SET_STATIC_PIXEL_SHADER_COMBO( COLOR_DEPTH, nColorDepth );
						SET_STATIC_PIXEL_SHADER( depthwrite_ps20 );
					}
				}
			}
#if !defined( _X360 ) && !defined( _PS3 )
			else
			{
				SET_FLAGS2( MATERIAL_VAR2_USES_VERTEXID );
				SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_TESSELLATION );

				DECLARE_STATIC_VERTEX_SHADER( depthwrite_vs30 );
				SET_STATIC_VERTEX_SHADER_COMBO( ONLY_PROJECT_POSITION, 0 ); //360 only combo, and this is a PC path
				SET_STATIC_VERTEX_SHADER_COMBO( TREESWAY, nTreeSwayMode );
				SET_STATIC_VERTEX_SHADER_COMBO( COLOR_DEPTH, nColorDepth );
				SET_STATIC_VERTEX_SHADER( depthwrite_vs30 );

				pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, true );

				DECLARE_STATIC_PIXEL_SHADER( depthwrite_ps30 );
				SET_STATIC_PIXEL_SHADER_COMBO( COLOR_DEPTH, nColorDepth );
				SET_STATIC_PIXEL_SHADER( depthwrite_ps30 );
			}
#endif
		}
		DYNAMIC_STATE
		{

#if !defined( _X360 ) && !defined( _PS3 )
			if ( !g_pHardwareConfig->SupportsPixelShaders_3_0() )
#endif
			{
				DECLARE_DYNAMIC_VERTEX_SHADER( depthwrite_vs20 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, ( int )vertexCompression );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( TESSELLATION, 0 );
				SET_DYNAMIC_VERTEX_SHADER( depthwrite_vs20 );

				if ( bAlphaClip )
				{
					BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, BASETEXTURE, FRAME );

					float vAlphaThreshold[4] = {0.7f, 0.7f, 0.7f, 0.7f};
					if ( ALPHATESTREFERENCE != -1 && ( params[ALPHATESTREFERENCE]->GetFloatValue() > 0.0f ) )
					{
						vAlphaThreshold[0] = vAlphaThreshold[1] = vAlphaThreshold[2] = vAlphaThreshold[3] = params[ALPHATESTREFERENCE]->GetFloatValue();
					}

					pShaderAPI->SetPixelShaderConstant( 0, vAlphaThreshold, 1 );
				}

				if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
				{
					DECLARE_DYNAMIC_PIXEL_SHADER( depthwrite_ps20b );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( ALPHACLIP, bAlphaClip );
					SET_DYNAMIC_PIXEL_SHADER( depthwrite_ps20b );
				}
				else
				{
					DECLARE_DYNAMIC_PIXEL_SHADER( depthwrite_ps20 );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( ALPHACLIP, bAlphaClip );
					SET_DYNAMIC_PIXEL_SHADER( depthwrite_ps20 );
				}
			}
#if !defined( _X360 ) && !defined( _PS3 )
			else // 3.0 shader case (PC only)
			{
				TessellationMode_t nTessellationMode = pShaderAPI->GetTessellationMode();
				if ( nTessellationMode != TESSELLATION_MODE_DISABLED )
				{
					pShaderAPI->BindStandardVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER1, TEXTURE_SUBDIVISION_PATCHES );

					float vSubDControls[4] = { 1.0f/pShaderAPI->GetSubDHeight(),
						bHasDisplacement && mat_displacementmap.GetBool() ? 1.0f : 0.0f,
						bHasDisplacementWrinkles && mat_displacementmap.GetBool() ? 1.0f : 0.0f, 0.0f };

					pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_8, vSubDControls );

					if( bHasDisplacement )
					{
						BindVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER2, DISPLACEMENTMAP );
					}
					else
					{
						pShaderAPI->BindStandardVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER2, TEXTURE_BLACK );
					}

					// Currently, tessellation is mutually exclusive with any kind of GPU-side skinning, morphing or vertex compression
					Assert( !pShaderAPI->IsHWMorphingEnabled() );
					Assert( pShaderAPI->GetCurrentNumBones() == 0 );
					Assert( vertexCompression == 0);
				}

				if ( g_pHardwareConfig->HasFastVertexTextures() )
				{
					SetHWMorphVertexShaderState( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6, VERTEX_SHADER_SHADER_SPECIFIC_CONST_7, SHADER_VERTEXTEXTURE_SAMPLER0 );
				}

				DECLARE_DYNAMIC_VERTEX_SHADER( depthwrite_vs30 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, ( pShaderAPI->GetCurrentNumBones() > 0 ) && ( nTessellationMode == TESSELLATION_MODE_DISABLED ) );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression && ( nTessellationMode == TESSELLATION_MODE_DISABLED ) );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( TESSELLATION, nTessellationMode );
				SET_DYNAMIC_VERTEX_SHADER( depthwrite_vs30 );

				if ( bAlphaClip )
				{
					BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, BASETEXTURE, FRAME );

					float vAlphaThreshold[4] = {0.7f, 0.7f, 0.7f, 0.7f};
					if ( ALPHATESTREFERENCE != -1 && ( params[ALPHATESTREFERENCE]->GetFloatValue() > 0.0f ) )
					{
						vAlphaThreshold[0] = vAlphaThreshold[1] = vAlphaThreshold[2] = vAlphaThreshold[3] = params[ALPHATESTREFERENCE]->GetFloatValue();
					}

					pShaderAPI->SetPixelShaderConstant( 0, vAlphaThreshold, 1 );
				}

				DECLARE_DYNAMIC_PIXEL_SHADER( depthwrite_ps30 );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( ALPHACLIP, bAlphaClip );
				SET_DYNAMIC_PIXEL_SHADER( depthwrite_ps30 );
			}
#endif

			if ( nTreeSwayMode != 0 )
			{
				float flParams[4];

				flParams[0] = pShaderAPI->CurrentTime();

				Vector windDir = IsBoolSet( TREESWAYSTATIC, params ) ? Vector(0.5f,0.5f,0) : pShaderAPI->GetVectorRenderingParameter( VECTOR_RENDERPARM_WIND_DIRECTION );

				flParams[1] = windDir.x;
				flParams[2] = windDir.y;
				flParams[3] = 0.0f;
				pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_2, flParams );

				flParams[0] = GetFloatParam( TREESWAYSCRUMBLEFALLOFFEXP, params, 1.0f );
				flParams[1] = GetFloatParam( TREESWAYFALLOFFEXP, params, 1.0f );
				flParams[2] = GetFloatParam( TREESWAYSCRUMBLESPEED, params, 3.0f );
				flParams[3] = GetFloatParam( TREESWAYSPEEDHIGHWINDMULTIPLIER, params, 2.0f );
				pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3, flParams );

				flParams[0] = GetFloatParam( TREESWAYHEIGHT, params, 1000.0f );
				flParams[1] = GetFloatParam( TREESWAYSTARTHEIGHT, params, 0.1f );
				flParams[2] = GetFloatParam( TREESWAYRADIUS, params, 300.0f );
				flParams[3] = GetFloatParam( TREESWAYSTARTRADIUS, params, 0.2f );
				pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_4, flParams );

				flParams[0] = GetFloatParam( TREESWAYSPEED, params, 1.0f );
				flParams[1] = GetFloatParam( TREESWAYSTRENGTH, params, 10.0f );
				flParams[2] = GetFloatParam( TREESWAYSCRUMBLEFREQUENCY, params, 12.0f );
				flParams[3] = GetFloatParam( TREESWAYSCRUMBLESTRENGTH, params, 10.0f );
				pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_5, flParams );

				flParams[0] = GetFloatParam( TREESWAYSPEEDLERPSTART, params, 3.0f );
				flParams[1] = GetFloatParam( TREESWAYSPEEDLERPEND, params, 6.0f );
				flParams[2] = 0.0f;
				flParams[3] = 0.0f;
				pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_9, flParams );

			}

		}	// DYNAMIC_STATE

		Draw( );
	}

	void ExecuteFastPath( int *dynVSIdx, int *dynPSIdx,  IMaterialVar** params, IShaderDynamicAPI * pShaderAPI, 
						  VertexCompressionType_t vertexCompression, CBasePerMaterialContextData **pContextDataPtr, BOOL bCSMEnabled )
	{
		*dynVSIdx = -1;
		*dynPSIdx = -1;

		int numBones = ShaderApiFast( pShaderAPI )->GetCurrentNumBones();

		int m_nCOMPRESSED_VERTS = vertexCompression? 1: 0;
		int m_nSKINNING = (numBones > 0)? 1: 0;
		int m_nMORPHING = 0;
		int m_nTESSELLATION = 0;
		*dynVSIdx = ( 1 * m_nCOMPRESSED_VERTS ) + ( 2 * m_nSKINNING ) + ( 4 * m_nMORPHING ) + ( 8 * m_nTESSELLATION ) + 0;

		int mtlFlags = params[FLAGS]->GetIntValue();
		int m_nALPHACLIP = (mtlFlags & MATERIAL_VAR_ALPHATEST)? 1: 0;
		*dynPSIdx = ( 1 * m_nALPHACLIP ) + 0;

		if ( m_nALPHACLIP )
		{
			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, BASETEXTURE, FRAME );

			float vAlphaThreshold[4] = {0.7f, 0.7f, 0.7f, 0.7f};
			if ( ALPHATESTREFERENCE != -1 && ( params[ALPHATESTREFERENCE]->GetFloatValue() > 0.0f ) )
			{
				vAlphaThreshold[0] = vAlphaThreshold[1] = vAlphaThreshold[2] = vAlphaThreshold[3] = 
					params[ALPHATESTREFERENCE]->GetFloatValue();
			}

			pShaderAPI->SetPixelShaderConstant( 0, vAlphaThreshold, 1 );
		}

		int nTreeSwayMode = GetIntParam( TREESWAY, params, 0 );

		if ( nTreeSwayMode != 0 )
		{
			float flParams[4];

			flParams[0] = pShaderAPI->CurrentTime();
			Vector windDir = IsBoolSet( TREESWAYSTATIC, params ) ? Vector(0.5f,0.5f,0) : pShaderAPI->GetVectorRenderingParameter( VECTOR_RENDERPARM_WIND_DIRECTION );

			flParams[1] = windDir.x;
			flParams[2] = windDir.y;
			flParams[3] = 0.0f;
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_2, flParams );

			flParams[0] = GetFloatParam( TREESWAYSCRUMBLEFALLOFFEXP, params, 1.0f );
			flParams[1] = GetFloatParam( TREESWAYFALLOFFEXP, params, 1.0f );
			flParams[2] = GetFloatParam( TREESWAYSCRUMBLESPEED, params, 3.0f );
			flParams[3] = GetFloatParam( TREESWAYSPEEDHIGHWINDMULTIPLIER, params, 2.0f );
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3, flParams );

			flParams[0] = GetFloatParam( TREESWAYHEIGHT, params, 1000.0f );
			flParams[1] = GetFloatParam( TREESWAYSTARTHEIGHT, params, 0.1f );
			flParams[2] = GetFloatParam( TREESWAYRADIUS, params, 300.0f );
			flParams[3] = GetFloatParam( TREESWAYSTARTRADIUS, params, 0.2f );
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_4, flParams );

			flParams[0] = GetFloatParam( TREESWAYSPEED, params, 1.0f );
			flParams[1] = GetFloatParam( TREESWAYSTRENGTH, params, 10.0f );
			flParams[2] = GetFloatParam( TREESWAYSCRUMBLEFREQUENCY, params, 12.0f );
			flParams[3] = GetFloatParam( TREESWAYSCRUMBLESTRENGTH, params, 10.0f );
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_5, flParams );

			flParams[0] = GetFloatParam( TREESWAYSPEEDLERPSTART, params, 3.0f );
			flParams[1] = GetFloatParam( TREESWAYSPEEDLERPEND, params, 6.0f );
			flParams[2] = 0.0f;
			flParams[3] = 0.0f;
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_9, flParams );
		}

		// WVP
		pShaderAPI->SetVertexShaderViewProj();
		pShaderAPI->UpdateVertexShaderMatrix(0);
	}

END_SHADER
