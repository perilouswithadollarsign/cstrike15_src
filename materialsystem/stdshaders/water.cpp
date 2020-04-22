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

ConVar r_buildingmapforworld( "r_buildingmapforworld", "0" );

#include "WaterCheap_vs20.inc"
#include "WaterCheap_ps20.inc"
#include "WaterCheap_ps20b.inc"
#include "Water_vs20.inc"
#include "water_ps20.inc"
#include "water_ps20b.inc"
#include "shaderlib/commandbuilder.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

DEFINE_FALLBACK_SHADER( Water, Water_DX9_HDR )

BEGIN_VS_SHADER( Water_DX90, 
			  "Help for Water" )

	BEGIN_SHADER_PARAMS
		SHADER_PARAM( REFRACTTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "_rt_WaterRefraction", "" )
		SHADER_PARAM( SCENEDEPTH, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( REFLECTTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "_rt_WaterReflection", "" )
		SHADER_PARAM( REFRACTAMOUNT, SHADER_PARAM_TYPE_FLOAT, "0", "" )
		SHADER_PARAM( REFRACTTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "refraction tint" )
		SHADER_PARAM( REFLECTAMOUNT, SHADER_PARAM_TYPE_FLOAT, "0.8", "" )
		SHADER_PARAM( REFLECTTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "reflection tint" )
		SHADER_PARAM( NORMALMAP, SHADER_PARAM_TYPE_TEXTURE, "dev/water_normal", "normal map" )
		SHADER_PARAM( BUMPFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $bumpmap" )
		SHADER_PARAM( BUMPTRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$bumpmap texcoord transform" )
		SHADER_PARAM( TIME, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( WATERDEPTH, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( CHEAPWATERSTARTDISTANCE, SHADER_PARAM_TYPE_FLOAT, "", "This is the distance from the eye in inches that the shader should start transitioning to a cheaper water shader." )
		SHADER_PARAM( CHEAPWATERENDDISTANCE, SHADER_PARAM_TYPE_FLOAT, "", "This is the distance from the eye in inches that the shader should finish transitioning to a cheaper water shader." )
		SHADER_PARAM( ENVMAP, SHADER_PARAM_TYPE_TEXTURE, "env_cubemap", "envmap" )
		SHADER_PARAM( ENVMAPFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "" )
		SHADER_PARAM( FOGCOLOR, SHADER_PARAM_TYPE_COLOR, "", "" )
		SHADER_PARAM( FORCECHEAP, SHADER_PARAM_TYPE_BOOL, "", "" )
		SHADER_PARAM( REFLECTENTITIES, SHADER_PARAM_TYPE_BOOL, "", "" )
		SHADER_PARAM( FOGSTART, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( FOGEND, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( ABOVEWATER, SHADER_PARAM_TYPE_BOOL, "", "" )
		SHADER_PARAM( WATERBLENDFACTOR, SHADER_PARAM_TYPE_FLOAT, "1.0", "" )
		SHADER_PARAM( NOFRESNEL, SHADER_PARAM_TYPE_BOOL, "0", "" )
		SHADER_PARAM( NOLOWENDLIGHTMAP, SHADER_PARAM_TYPE_BOOL, "0", "" )
		SHADER_PARAM( SCROLL1, SHADER_PARAM_TYPE_COLOR, "", "" )
		SHADER_PARAM( SCROLL2, SHADER_PARAM_TYPE_COLOR, "", "" )

		SHADER_PARAM( FLASHLIGHTTINT, SHADER_PARAM_TYPE_FLOAT, "0", "" )
		SHADER_PARAM( LIGHTMAPWATERFOG, SHADER_PARAM_TYPE_BOOL, "0", "" )
		SHADER_PARAM( FORCEFRESNEL, SHADER_PARAM_TYPE_FLOAT, "0", "" )

		SHADER_PARAM( FORCEENVMAP, SHADER_PARAM_TYPE_BOOL, "0", "" )

		SHADER_PARAM( DEPTH_FEATHER, SHADER_PARAM_TYPE_INTEGER, "0", "" )

		// New flow params
		SHADER_PARAM( FLOWMAP, SHADER_PARAM_TYPE_TEXTURE, "", "flowmap" )
		SHADER_PARAM( FLOWMAPFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $flowmap" )
		SHADER_PARAM( FLOWMAPSCROLLRATE, SHADER_PARAM_TYPE_VEC2, "[0 0", "2D rate to scroll $flowmap" )
		SHADER_PARAM( FLOW_NOISE_TEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "flow noise texture" )

		SHADER_PARAM( FLOW_WORLDUVSCALE, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( FLOW_NORMALUVSCALE, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( FLOW_TIMEINTERVALINSECONDS, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( FLOW_UVSCROLLDISTANCE, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( FLOW_BUMPSTRENGTH, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( FLOW_NOISE_SCALE, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( FLOW_DEBUG, SHADER_PARAM_TYPE_BOOL, "0", "" )

		SHADER_PARAM( COLOR_FLOW_UVSCALE, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( COLOR_FLOW_TIMEINTERVALINSECONDS, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( COLOR_FLOW_UVSCROLLDISTANCE, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( COLOR_FLOW_LERPEXP, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( COLOR_FLOW_DISPLACEBYNORMALSTRENGTH, SHADER_PARAM_TYPE_FLOAT, "", "" )

		SHADER_PARAM( SIMPLEOVERLAY, SHADER_PARAM_TYPE_TEXTURE, "", "simpleoverlay" )

	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		if( !params[ABOVEWATER]->IsDefined() )
		{
			Warning( "***need to set $abovewater for material %s\n", pMaterialName );
			params[ABOVEWATER]->SetIntValue( 1 );
		}
		SET_FLAGS2( MATERIAL_VAR2_NEEDS_TANGENT_SPACES );
		if( !params[CHEAPWATERSTARTDISTANCE]->IsDefined() )
		{
			params[CHEAPWATERSTARTDISTANCE]->SetFloatValue( 500.0f );
		}
		if( !params[CHEAPWATERENDDISTANCE]->IsDefined() )
		{
			params[CHEAPWATERENDDISTANCE]->SetFloatValue( 1000.0f );
		}
		if( !params[SCROLL1]->IsDefined() )
		{
			params[SCROLL1]->SetVecValue( 0.0f, 0.0f, 0.0f );
		}
		if( !params[SCROLL2]->IsDefined() )
		{
			params[SCROLL2]->SetVecValue( 0.0f, 0.0f, 0.0f );
		}
		if( !params[FOGCOLOR]->IsDefined() )
		{
			params[FOGCOLOR]->SetVecValue( 1.0f, 0.0f, 0.0f );
			Warning( "material %s needs to have a $fogcolor.\n", pMaterialName );
		}
		if( !params[REFLECTENTITIES]->IsDefined() )
		{
			params[REFLECTENTITIES]->SetIntValue( 0 );
		}
		if( !params[WATERBLENDFACTOR]->IsDefined() )
		{
			params[WATERBLENDFACTOR]->SetFloatValue( 1.0f );
		}

		if ( IsPS3() && !params[SCENEDEPTH]->IsDefined() )
		{
			params[SCENEDEPTH]->SetStringValue( "^PS3^DEPTHBUFFER" );
		}

		// If there's no envmap or reflection texture, make sure to set the reflection tint to 0 so we don't reflect garbage
		// (The better change would be to add static combos to support no environment map but this is a lower impact change at this point)
		if ( !params[ ENVMAP ]->IsDefined() && !params[ REFLECTTEXTURE ]->IsDefined() )
		{
			params[ REFLECTTINT ]->SetVecValue( 0.0f, 0.0f, 0.0f, 0.0f );
		}

		InitFloatParam( FLOW_WORLDUVSCALE, params, 1.0f );
		InitFloatParam( FLOW_NORMALUVSCALE, params, 1.0f );
		InitFloatParam( FLOW_TIMEINTERVALINSECONDS, params, 0.4f );
		InitFloatParam( FLOW_UVSCROLLDISTANCE, params, 0.2f );
		InitFloatParam( FLOW_BUMPSTRENGTH, params, 1.0f );
		InitFloatParam( FLOW_NOISE_SCALE, params, 0.0002f );

		InitFloatParam( COLOR_FLOW_UVSCALE, params, 1.0f );
		InitFloatParam( COLOR_FLOW_TIMEINTERVALINSECONDS, params, 0.4f );
		InitFloatParam( COLOR_FLOW_UVSCROLLDISTANCE, params, 0.2f );
		InitFloatParam( COLOR_FLOW_LERPEXP, params, 1.0f );
		InitFloatParam( COLOR_FLOW_DISPLACEBYNORMALSTRENGTH, params, 0.0025f );

		InitIntParam( FORCEENVMAP, params, 0 );

		InitIntParam( FORCECHEAP, params, 0 );
		InitFloatParam( FLASHLIGHTTINT, params, 1.0f );
		InitIntParam( LIGHTMAPWATERFOG, params, 0 );
		InitFloatParam( FORCEFRESNEL, params, -1.0f );

		// Fallbacks for water need lightmaps usually
		if ( params[BASETEXTURE]->IsDefined() || ( params[LIGHTMAPWATERFOG]->GetIntValue() != 0 ) )
		{
			SET_FLAGS2( MATERIAL_VAR2_LIGHTING_LIGHTMAP );
		}

		SET_FLAGS2( MATERIAL_VAR2_LIGHTING_LIGHTMAP );
		// Don't need bumped lightmaps unless we have a basetexture.  We only use them otherwise for lighting the water fog, which only needs one sample.
		if( params[BASETEXTURE]->IsDefined() && g_pConfig->UseBumpmapping() && params[NORMALMAP]->IsDefined() )
		{
			SET_FLAGS2( MATERIAL_VAR2_LIGHTING_BUMPED_LIGHTMAP );
		}

		if ( !params[DEPTH_FEATHER]->IsDefined() )
		{
			params[DEPTH_FEATHER]->SetIntValue( 0 );
		}
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		Assert( params[WATERDEPTH]->IsDefined() );

		if( params[REFRACTTEXTURE]->IsDefined() )
		{
			LoadTexture( REFRACTTEXTURE, TEXTUREFLAGS_SRGB );
		}
		if( params[SCENEDEPTH]->IsDefined() )
		{
			LoadTexture( SCENEDEPTH, 0 );
		}
		if( params[REFLECTTEXTURE]->IsDefined() )
		{
			LoadTexture( REFLECTTEXTURE, TEXTUREFLAGS_SRGB );
		}
		if ( params[ENVMAP]->IsDefined() )
		{
			LoadCubeMap( ENVMAP, TEXTUREFLAGS_SRGB );
		}
		if ( params[NORMALMAP]->IsDefined() )
		{
			LoadBumpMap( NORMALMAP );
		}
		if( params[BASETEXTURE]->IsDefined() )
		{
			LoadTexture( BASETEXTURE, TEXTUREFLAGS_SRGB );
		}
		if ( params[FLOWMAP]->IsDefined() )
		{
			LoadTexture( FLOWMAP );
		}
		if ( params[FLOW_NOISE_TEXTURE]->IsDefined() )
		{
			LoadTexture( FLOW_NOISE_TEXTURE );
		}
		if ( params[SIMPLEOVERLAY]->IsDefined() )
		{
			LoadTexture( SIMPLEOVERLAY, TEXTUREFLAGS_SRGB );
		}
	}

	inline void GetVecParam( int constantVar, float *val )
	{
		if( constantVar == -1 )
			return;

		IMaterialVar* pVar = s_ppParams[constantVar];
		Assert( pVar );

		if (pVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
			pVar->GetVecValue( val, 4 );
		else
			val[0] = val[1] = val[2] = val[3] = pVar->GetFloatValue();
	}

	inline void DrawReflectionRefraction( IMaterialVar **params, IShaderShadow* pShaderShadow,
		IShaderDynamicAPI* pShaderAPI, bool bReflection, bool bRefraction ) 
	{
		Vector4D Scroll1;
		params[SCROLL1]->GetVecValue( Scroll1.Base(), 4 );

		bool bHasFlowmap = params[FLOWMAP]->IsTexture();
		bool bHasBaseTexture = params[BASETEXTURE]->IsTexture();
		bool bHasMultiTexture = fabs( Scroll1.x ) > 0.0f;
		bool hasFlashlight = !bHasMultiTexture && UsingFlashlight( params );
		bool bLightmapWaterFog = ( params[LIGHTMAPWATERFOG]->GetIntValue() != 0 );
		bool bHasSimpleOverlay = params[SIMPLEOVERLAY]->IsTexture();
		bool bForceFresnel = ( params[FORCEFRESNEL]->GetFloatValue() != -1.0f );

		if ( bHasFlowmap )
		{
			bHasMultiTexture = false;
		}

		if ( bHasBaseTexture || bHasMultiTexture )
		{
			//hasFlashlight = false;
			//bLightmapWaterFog = false;
		}

		// LIGHTMAP - needed either with basetexture or lightmapwaterfog.  Not sure where the bReflection restriction comes in.
		bool bUsingLightmap = bLightmapWaterFog || ( bReflection && bHasBaseTexture );

		SHADOW_STATE
		{
			SetInitialShadowState( );
			if ( bRefraction )
			{
				// refract sampler
				pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, !IsX360() );
			}

			if ( bReflection )
			{
				// reflect sampler
				pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, !IsX360() );
			}  
			else
			{
				// envmap sampler
				pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, false );
			}

			if ( bHasBaseTexture )
			{
				// BASETEXTURE
				pShaderShadow->EnableTexture( SHADER_SAMPLER10, true );
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER10, true );
			}

			// normal map
			pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );

			if ( bUsingLightmap )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER3, false );
			}

			// flowmap
			if ( bHasFlowmap )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER4, false );

				pShaderShadow->EnableTexture( SHADER_SAMPLER5, true );
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER5, false );
			}

			if( hasFlashlight  )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER6, true );

				pShaderShadow->EnableTexture( SHADER_SAMPLER7, true );
				//pShaderShadow->SetShadowDepthFiltering( SHADER_SAMPLER7 );

				pShaderShadow->EnableTexture( SHADER_SAMPLER8, true );
			}
			
			if ( IsGameConsole() )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER9, true );
			}

			if ( bHasSimpleOverlay )
			{
				// SIMPLEOVERLAY
				pShaderShadow->EnableTexture( SHADER_SAMPLER11, true );
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER11, true );
			}

			// pseudo-translucent water only gets used on platforms which disable refract (as a cheaper substitute for refractive water)
			if( IS_FLAG_SET( MATERIAL_VAR_PSEUDO_TRANSLUCENT ) )
			{
				s_pShaderShadow->EnableBlendingForceOpaque( true );
				s_pShaderShadow->BlendFunc( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				s_pShaderShadow->EnableDepthWrites( true );
			}

			int fmt = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_TANGENT_S | VERTEX_TANGENT_T;

			// texcoord0 : base texcoord
			// texcoord1 : lightmap texcoord
			// texcoord2 : lightmap texcoord offset
			int numTexCoords = 1;
			// You need lightmap data if you are using lightmapwaterfog or you have a basetexture.
			if ( bLightmapWaterFog || bHasBaseTexture )
			{
				numTexCoords = 3;
			}
			pShaderShadow->VertexShaderVertexFormat( fmt, numTexCoords, 0, 0 );
			
			DECLARE_STATIC_VERTEX_SHADER( water_vs20 );
			SET_STATIC_VERTEX_SHADER_COMBO( MULTITEXTURE, bHasMultiTexture );
			SET_STATIC_VERTEX_SHADER_COMBO( BASETEXTURE, bHasBaseTexture );
			SET_STATIC_VERTEX_SHADER_COMBO( FLASHLIGHT, hasFlashlight );
			SET_STATIC_VERTEX_SHADER_COMBO( LIGHTMAPWATERFOG, bLightmapWaterFog );
			SET_STATIC_VERTEX_SHADER_COMBO( FLOWMAP, bHasFlowmap );
			SET_STATIC_VERTEX_SHADER( water_vs20 );

			// "REFLECT" "0..1"
			// "REFRACT" "0..1"
			
			if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( water_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( REFLECT,  bReflection );
				SET_STATIC_PIXEL_SHADER_COMBO( REFRACT,  bRefraction );
				SET_STATIC_PIXEL_SHADER_COMBO( ABOVEWATER,  params[ABOVEWATER]->GetIntValue() );
				SET_STATIC_PIXEL_SHADER_COMBO( MULTITEXTURE, bHasMultiTexture );
				SET_STATIC_PIXEL_SHADER_COMBO( BASETEXTURE, bHasBaseTexture );
				SET_STATIC_PIXEL_SHADER_COMBO( FLOWMAP, bHasFlowmap );
				SET_STATIC_PIXEL_SHADER_COMBO( FLOW_DEBUG, clamp( params[ FLOW_DEBUG ]->GetIntValue(), 0, 2 ) );
				SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHT, hasFlashlight );
				SET_STATIC_PIXEL_SHADER_COMBO( LIGHTMAPWATERFOG, bLightmapWaterFog );
				SET_STATIC_PIXEL_SHADER_COMBO( FORCEFRESNEL, bForceFresnel );
				SET_STATIC_PIXEL_SHADER_COMBO( SIMPLEOVERLAY, bHasSimpleOverlay );
				SET_STATIC_PIXEL_SHADER( water_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( water_ps20 );
				SET_STATIC_PIXEL_SHADER_COMBO( REFLECT,  bReflection );
				SET_STATIC_PIXEL_SHADER_COMBO( REFRACT,  bRefraction );
				SET_STATIC_PIXEL_SHADER_COMBO( ABOVEWATER,  params[ABOVEWATER]->GetIntValue() );
				SET_STATIC_PIXEL_SHADER_COMBO( MULTITEXTURE, bHasMultiTexture );
				SET_STATIC_PIXEL_SHADER_COMBO( BASETEXTURE, bHasBaseTexture );
//				SET_STATIC_PIXEL_SHADER_COMBO( FLOWMAP, bHasFlowmap );
				SET_STATIC_PIXEL_SHADER_COMBO( FLOW_DEBUG, clamp( params[ FLOW_DEBUG ]->GetIntValue(), 0, 2 ) );
				SET_STATIC_PIXEL_SHADER_COMBO( FORCEFRESNEL, bForceFresnel );
				SET_STATIC_PIXEL_SHADER_COMBO( SIMPLEOVERLAY, bHasSimpleOverlay );
				SET_STATIC_PIXEL_SHADER( water_ps20 );
			}

			FogToFogColor();

			// we are writing linear values from this shader.
			pShaderShadow->EnableSRGBWrite( true );

			pShaderShadow->EnableAlphaWrites( true );
		}
		DYNAMIC_STATE
		{
			pShaderAPI->SetDefaultState();
			if ( bRefraction )
			{
				// HDRFIXME: add comment about binding.. Specify the number of MRTs in the enable
				BindTexture( SHADER_SAMPLER0, SRGBReadMask( !IsX360() ), REFRACTTEXTURE, -1 );
			}

			if ( bReflection )
			{
				BindTexture( SHADER_SAMPLER1, SRGBReadMask( !IsX360() ), REFLECTTEXTURE, -1 );
			}
			else if ( params[ ENVMAP ]->IsDefined() )
			{
				BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, ENVMAP );
			}

			BindTexture( SHADER_SAMPLER2, TEXTURE_BINDFLAGS_NONE, NORMALMAP, BUMPFRAME );

			if ( bUsingLightmap )
			{
				pShaderAPI->BindStandardTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, TEXTURE_LIGHTMAP );
			}

			if( bHasBaseTexture )
			{
				BindTexture( SHADER_SAMPLER10, IsX360() ? TEXTURE_BINDFLAGS_NONE : TEXTURE_BINDFLAGS_SRGBREAD, BASETEXTURE, FRAME );
			}

			if ( bHasFlowmap )
			{
				BindTexture( SHADER_SAMPLER4, TEXTURE_BINDFLAGS_NONE, FLOWMAP, FLOWMAPFRAME );
				BindTexture( SHADER_SAMPLER5, TEXTURE_BINDFLAGS_NONE, FLOW_NOISE_TEXTURE );

				float vFlowConst1[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				vFlowConst1[0] = 1.0f / params[ FLOW_WORLDUVSCALE ]->GetFloatValue();
				vFlowConst1[1] = 1.0f / params[ FLOW_NORMALUVSCALE ]->GetFloatValue();
				vFlowConst1[2] = params[ FLOW_BUMPSTRENGTH ]->GetFloatValue();
				vFlowConst1[3] = params[ COLOR_FLOW_DISPLACEBYNORMALSTRENGTH ]->GetFloatValue();
				pShaderAPI->SetPixelShaderConstant( 13, vFlowConst1, 1 );

				float vFlowConst2[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				vFlowConst2[0] = params[ FLOW_TIMEINTERVALINSECONDS ]->GetFloatValue();
				vFlowConst2[1] = params[ FLOW_UVSCROLLDISTANCE ]->GetFloatValue();
				vFlowConst2[2] = params[ FLOW_NOISE_SCALE ]->GetFloatValue();
				pShaderAPI->SetPixelShaderConstant( 14, vFlowConst2, 1 );

				float vColorFlowConst1[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				vColorFlowConst1[0] = 1.0f / params[ COLOR_FLOW_UVSCALE ]->GetFloatValue();
				vColorFlowConst1[1] = params[ COLOR_FLOW_TIMEINTERVALINSECONDS ]->GetFloatValue();
				vColorFlowConst1[2] = params[ COLOR_FLOW_UVSCROLLDISTANCE ]->GetFloatValue();
				vColorFlowConst1[3] = params[ COLOR_FLOW_LERPEXP ]->GetFloatValue();
				pShaderAPI->SetPixelShaderConstant( 26, vColorFlowConst1, 1 );
			}
			
			if( bHasSimpleOverlay )
			{
				BindTexture( SHADER_SAMPLER11, TEXTURE_BINDFLAGS_SRGBREAD, SIMPLEOVERLAY );
			}

			if ( IsGameConsole() )
			{
				if ( IsPS3() )
				{
					BindTexture( SHADER_SAMPLER9, TEXTURE_BINDFLAGS_NONE, SCENEDEPTH, -1 );
				}
				else if ( IsX360() )
				{
					pShaderAPI->BindStandardTexture( SHADER_SAMPLER9, TEXTURE_BINDFLAGS_NONE, TEXTURE_FRAME_BUFFER_FULL_DEPTH );
				}
				else
				{
					Error( "water.cpp: Unsupported console platform.\n" );
				}
				
				VMatrix viewMatrix, projMatrix, worldToProjMatrix, projToWorldMatrix;
				pShaderAPI->GetMatrix( MATERIAL_VIEW, viewMatrix.m[0] );
				pShaderAPI->GetActualProjectionMatrix( projMatrix.m[0] );
								
				// The view and proj matrices are transposed vs. what you would normally expect, argh.
				//viewMatrix = viewMatrix.Transpose();
				//projMatrix = projMatrix.Transpose();
				//MatrixMultiply( projMatrix, viewMatrix, worldToProjMatrix );
				//MatrixInverseGeneral( worldToProjMatrix, projToWorldMatrix );

				// One less transpose.
				MatrixMultiply( viewMatrix, projMatrix, worldToProjMatrix );
				MatrixInverseGeneral( worldToProjMatrix, projToWorldMatrix );
				projToWorldMatrix = projToWorldMatrix.Transpose();
								
				// Send down rows 2 (Z) and 3 (W), because that's all the water shader needs to recover worldspace Z.
				pShaderAPI->SetPixelShaderConstant( 33, &projToWorldMatrix.m[2][0], 1 );
				pShaderAPI->SetPixelShaderConstant( 34, &projToWorldMatrix.m[3][0], 1 );
				
				int nDepthFeather = params[DEPTH_FEATHER]->GetIntValue();
				const float flDepthFeatherDistanceFactor = .16f;
				Vector4D vEdgeFeatheringParams( flDepthFeatherDistanceFactor, nDepthFeather ? 0.0f : 1.0f, 0.0f, 0.0f );
				pShaderAPI->SetPixelShaderConstant( 35, vEdgeFeatheringParams.Base(), 1 );
			}
			
			// Time
			float vTimeConst[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			float flTime = pShaderAPI->CurrentTime();
			vTimeConst[0] = flTime;
			//vTimeConst[0] -= ( float )( ( int )( vTimeConst[0] / 1000.0f ) ) * 1000.0f;
			pShaderAPI->SetPixelShaderConstant( 8, vTimeConst, 1 );

			// These constants are used to rotate the world space water normals around the up axis to align the
			// normal with the camera and then give us a 2D offset vector to use for reflection and refraction uv's
			VMatrix mView;
			pShaderAPI->GetMatrix( MATERIAL_VIEW, mView.m[0] );
			mView = mView.Transpose3x3();

			Vector4D vCameraRight( mView.m[0][0], mView.m[0][1], mView.m[0][2], 0.0f );
			vCameraRight.z = 0.0f; // Project onto the plane of water
			vCameraRight.AsVector3D().NormalizeInPlace();

			Vector4D vCameraForward;
			CrossProduct( Vector( 0.0f, 0.0f, 1.0f ), vCameraRight.AsVector3D(), vCameraForward.AsVector3D() ); // I assume the water surface normal is pointing along z!

			pShaderAPI->SetPixelShaderConstant( 22, vCameraRight.Base() );
			pShaderAPI->SetPixelShaderConstant( 23, vCameraForward.Base() );

			SetPixelShaderConstant( 25, FORCEFRESNEL );

			// Refraction tint
			if( bRefraction )
			{
				SetPixelShaderConstantGammaToLinear( 1, REFRACTTINT );
			}

			// Reflection tint
			if ( g_pHardwareConfig->GetHDRType() == HDR_TYPE_INTEGER )
			{
				// Need to multiply by 4 in linear space since we premultiplied into
				// the render target by .25 to get overbright data in the reflection render target.
				float gammaReflectTint[3];
				params[REFLECTTINT]->GetVecValue( gammaReflectTint, 3 );
				float linearReflectTint[4];
				linearReflectTint[0] = GammaToLinear( gammaReflectTint[0] ) * 4.0f;
				linearReflectTint[1] = GammaToLinear( gammaReflectTint[1] ) * 4.0f;
				linearReflectTint[2] = GammaToLinear( gammaReflectTint[2] ) * 4.0f;
				linearReflectTint[3] = params[WATERBLENDFACTOR]->GetFloatValue();
				pShaderAPI->SetPixelShaderConstant( 4, linearReflectTint, 1 );
			}
			else
			{
				SetPixelShaderConstantGammaToLinear( 4, REFLECTTINT, WATERBLENDFACTOR );
			}

			SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_1, BUMPTRANSFORM );
			
			float curtime=pShaderAPI->CurrentTime();
			float vc0[4];
			float v0[4];
			params[SCROLL1]->GetVecValue(v0,4);
			vc0[0]=curtime*v0[0];
			vc0[1]=curtime*v0[1];
			params[SCROLL2]->GetVecValue(v0,4);
			vc0[2]=curtime*v0[0];
			vc0[3]=curtime*v0[1];
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3, vc0, 1 );

			float c0[4] = { 1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f, 0.0f };
			pShaderAPI->SetPixelShaderConstant( 0, c0, 1 );
			
			float c2[4] = { 0.5f, 0.5f, 0.5f, 0.5f };
			pShaderAPI->SetPixelShaderConstant( 2, c2, 1 );
			
			// fresnel constants
			float c3[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
			pShaderAPI->SetPixelShaderConstant( 3, c3, 1 );

			float c5[4] = { params[REFLECTAMOUNT]->GetFloatValue(), params[REFLECTAMOUNT]->GetFloatValue(), 
				params[REFRACTAMOUNT]->GetFloatValue(), params[REFRACTAMOUNT]->GetFloatValue() };
			pShaderAPI->SetPixelShaderConstant( 5, c5, 1 );

#if 0
			SetPixelShaderConstantGammaToLinear( 6, FOGCOLOR );
#else
			// Need to use the srgb curve since that we do in UpdatePixelFogColorConstant so that we match the older version of water where we render to an offscreen buffer and fog on the way in.
			float fogColorConstant[4];

			params[FOGCOLOR]->GetVecValue( fogColorConstant, 3 );
			fogColorConstant[3] = 0.0f;

			fogColorConstant[0] = SrgbGammaToLinear( fogColorConstant[0] );
			fogColorConstant[1] = SrgbGammaToLinear( fogColorConstant[1] );
			fogColorConstant[2] = SrgbGammaToLinear( fogColorConstant[2] );
			pShaderAPI->SetPixelShaderConstant( 6, fogColorConstant, 1 );
#endif

			float c7[4] = 
			{ 
				params[FOGSTART]->GetFloatValue(), 
				params[FOGEND]->GetFloatValue() - params[FOGSTART]->GetFloatValue(), 
				1.0f, 
				0.0f 
			};
			if (g_pHardwareConfig->GetHDRType() == HDR_TYPE_INTEGER )
			{
				// water overbright factor
				c7[2] = 4.0;
			}
			pShaderAPI->SetPixelShaderConstant( 7, c7, 1 );

			pShaderAPI->SetPixelShaderFogParams( PSREG_FOG_PARAMS );

			float vEyePos_SpecExponent[4];
			pShaderAPI->GetWorldSpaceCameraPosition( vEyePos_SpecExponent );
			vEyePos_SpecExponent[3] = 0.0f;
			pShaderAPI->SetPixelShaderConstant( PSREG_EYEPOS_SPEC_EXPONENT, vEyePos_SpecExponent, 1 );

			if( bHasFlowmap )
			{
				SetPixelShaderConstant( 9, FLOWMAPSCROLLRATE );
			}

			DECLARE_DYNAMIC_VERTEX_SHADER( water_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( water_vs20 );

#ifdef _PS3
			CCommandBufferBuilder< CDynamicCommandStorageBuffer > DynamicCmdsOut;
#else
			CCommandBufferBuilder< CFixedCommandStorageBuffer< 1000 > > DynamicCmdsOut;
#endif

			bool bFlashlightShadows = false;
			bool bUberlight = false;
			if( hasFlashlight )
			{
#ifdef _PS3
				CCommandBufferBuilder< CFixedCommandStorageBuffer< 256 > > flashlightECB;
#endif

				pShaderAPI->GetFlashlightShaderInfo( &bFlashlightShadows, &bUberlight );
#ifdef _PS3
				{
					flashlightECB.SetVertexShaderFlashlightState( VERTEX_SHADER_SHADER_SPECIFIC_CONST_4 );
				}
#endif
				if( IsX360())
				{
					DynamicCmdsOut.SetVertexShaderFlashlightState( VERTEX_SHADER_SHADER_SPECIFIC_CONST_4 );
				}

				CBCmdSetPixelShaderFlashlightState_t state;
				state.m_LightSampler = SHADER_SAMPLER6; // FIXME . . don't want this here.
				state.m_DepthSampler = SHADER_SAMPLER7;
				state.m_ShadowNoiseSampler = SHADER_SAMPLER8;
				state.m_nColorConstant = PSREG_FLASHLIGHT_COLOR;
				state.m_nAttenConstant = 15;
				state.m_nOriginConstant = 16;
				state.m_nDepthTweakConstant = 21;
				state.m_nScreenScaleConstant = PSREG_FLASHLIGHT_SCREEN_SCALE;
				state.m_nWorldToTextureConstant = -1;
				state.m_bFlashlightNoLambert = false;
				state.m_bSinglePassFlashlight = true;

#ifdef _PS3
				{
					flashlightECB.SetPixelShaderFlashlightState( state );
					flashlightECB.End();

					ShaderApiFast( pShaderAPI )->ExecuteCommandBufferPPU( flashlightECB.Base() );
				}
#else
				{
					DynamicCmdsOut.SetPixelShaderFlashlightState( state );
				}
#endif
				DynamicCmdsOut.SetPixelShaderConstant( 10, FLASHLIGHTTINT );
			}

			// Get viewport and render target dimensions and set shader constant to do a 2D mad
			int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
			pShaderAPI->GetCurrentViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );

			int nRtWidth, nRtHeight;
			pShaderAPI->GetCurrentRenderTargetDimensions( nRtWidth, nRtHeight );

			float vViewportMad[4];

			// viewport->screen transform
			vViewportMad[0] = ( float )nViewportWidth / ( float )nRtWidth;
			vViewportMad[1] = ( float )nViewportHeight / ( float )nRtHeight;
			vViewportMad[2] = ( float )nViewportX / ( float )nRtWidth;
			vViewportMad[3] = ( float )nViewportY / ( float )nRtHeight;
			DynamicCmdsOut.SetPixelShaderConstant( 24, vViewportMad, 1 );

			if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( water_ps20b );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHTSHADOWS, bFlashlightShadows );
				SET_DYNAMIC_PIXEL_SHADER( water_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( water_ps20 );
				SET_DYNAMIC_PIXEL_SHADER( water_ps20 );
			}

			DynamicCmdsOut.End();
			pShaderAPI->ExecuteCommandBuffer( DynamicCmdsOut.Base() );
		}
		Draw();
	}

	inline void DrawCheapWater( IMaterialVar **params, IShaderShadow* pShaderShadow, 
		                        IShaderDynamicAPI* pShaderAPI, bool bBlend, bool bRefraction )
	{
		bool bHasFlowmap = params[FLOWMAP]->IsTexture();
		SHADOW_STATE
		{
			SetInitialShadowState( );

			// In edit mode, use nocull
			if ( UsingEditor( params ) )
			{
				s_pShaderShadow->EnableCulling( false );
			}

			if( bBlend )
			{
				EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
			}
			// envmap
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			// normal map
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
			if( bRefraction && bBlend )
			{
				// refraction map (used for alpha)
				pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
			}

			// Noise texture
			if ( bHasFlowmap )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );
				pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );
			}

			// Normalizing cube map
			pShaderShadow->EnableTexture( SHADER_SAMPLER6, true );
			int fmt = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_TANGENT_S | VERTEX_TANGENT_T;
			pShaderShadow->VertexShaderVertexFormat( fmt, 1, 0, 0 );

			DECLARE_STATIC_VERTEX_SHADER( watercheap_vs20 );
			SET_STATIC_VERTEX_SHADER_COMBO( BLEND,  bBlend && bRefraction );
			SET_STATIC_VERTEX_SHADER( watercheap_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( watercheap_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( FRESNEL,  params[NOFRESNEL]->GetIntValue() == 0 );
				SET_STATIC_PIXEL_SHADER_COMBO( BLEND,  bBlend );
				SET_STATIC_PIXEL_SHADER_COMBO( REFRACTALPHA,  bRefraction );
				SET_STATIC_PIXEL_SHADER_COMBO( HDRTYPE,  g_pHardwareConfig->GetHDRType() );
				Vector4D Scroll1;
				params[SCROLL1]->GetVecValue( Scroll1.Base(), 4 );
				SET_STATIC_PIXEL_SHADER_COMBO( MULTITEXTURE,fabs(Scroll1.x) > 0.0);
				SET_STATIC_PIXEL_SHADER_COMBO( FLOWMAP, bHasFlowmap );
				SET_STATIC_PIXEL_SHADER_COMBO( FLOW_DEBUG, clamp( params[ FLOW_DEBUG ]->GetIntValue(), 0, 2 ) );
				SET_STATIC_PIXEL_SHADER( watercheap_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( watercheap_ps20 );
				SET_STATIC_PIXEL_SHADER_COMBO( FRESNEL,  params[NOFRESNEL]->GetIntValue() == 0 );
				SET_STATIC_PIXEL_SHADER_COMBO( BLEND,  bBlend );
				SET_STATIC_PIXEL_SHADER_COMBO( REFRACTALPHA,  bRefraction );
				SET_STATIC_PIXEL_SHADER_COMBO( HDRTYPE,  g_pHardwareConfig->GetHDRType() );
				Vector4D Scroll1;
				params[SCROLL1]->GetVecValue( Scroll1.Base(), 4 );
				SET_STATIC_PIXEL_SHADER_COMBO( MULTITEXTURE,fabs(Scroll1.x) > 0.0);
				SET_STATIC_PIXEL_SHADER_COMBO( FLOWMAP, bHasFlowmap );
				SET_STATIC_PIXEL_SHADER_COMBO( FLOW_DEBUG, clamp( params[ FLOW_DEBUG ]->GetIntValue(), 0, 2 ) );
				SET_STATIC_PIXEL_SHADER( watercheap_ps20 );
			}

			// HDRFIXME: test cheap water!
			if( g_pHardwareConfig->GetHDRType() != HDR_TYPE_NONE )
			{
				// we are writing linear values from this shader.
				pShaderShadow->EnableSRGBWrite( true );
			}

			FogToFogColor();
		}
		DYNAMIC_STATE
		{
			pShaderAPI->SetDefaultState();

			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, ENVMAP, ENVMAPFRAME );
			BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, NORMALMAP, BUMPFRAME );
			if( bRefraction && bBlend )
			{
				BindTexture( SHADER_SAMPLER2, TEXTURE_BINDFLAGS_NONE, REFRACTTEXTURE, -1 );
			}

			if ( bHasFlowmap )
			{
				BindTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, FLOWMAP );
				BindTexture( SHADER_SAMPLER4, TEXTURE_BINDFLAGS_NONE, FLOW_NOISE_TEXTURE );

				float vFlowConst1[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				vFlowConst1[0] = 1.0f / params[ FLOW_WORLDUVSCALE ]->GetFloatValue();
				vFlowConst1[1] = 1.0f / params[ FLOW_NORMALUVSCALE ]->GetFloatValue();
				vFlowConst1[2] = params[ FLOW_BUMPSTRENGTH ]->GetFloatValue();
				pShaderAPI->SetPixelShaderConstant( 13, vFlowConst1, 1 );

				float vFlowConst2[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				vFlowConst2[0] = params[ FLOW_TIMEINTERVALINSECONDS ]->GetFloatValue();
				vFlowConst2[1] = params[ FLOW_UVSCROLLDISTANCE ]->GetFloatValue();
				vFlowConst2[2] = params[ FLOW_NOISE_SCALE ]->GetFloatValue();
				pShaderAPI->SetPixelShaderConstant( 14, vFlowConst2, 1 );

				// Time % 1000
				float vTimeConst[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				float flTime = pShaderAPI->CurrentTime();
				vTimeConst[0] = flTime;
				//vTimeConst[0] -= ( float )( ( int )( vTimeConst[0] / 1000.0f ) ) * 1000.0f;
				pShaderAPI->SetPixelShaderConstant( 10, vTimeConst, 1 );
			}

			pShaderAPI->BindStandardTexture( SHADER_SAMPLER6, TEXTURE_BINDFLAGS_NONE, TEXTURE_NORMALIZATION_CUBEMAP_SIGNED );

			SetPixelShaderConstant( 0, FOGCOLOR );

			float cheapWaterStartDistance = params[CHEAPWATERSTARTDISTANCE]->GetFloatValue();
			float cheapWaterEndDistance = params[CHEAPWATERENDDISTANCE]->GetFloatValue();
			float cheapWaterParams[4] = 
			{
				cheapWaterStartDistance * VSHADER_VECT_SCALE,
				cheapWaterEndDistance * VSHADER_VECT_SCALE,
				PSHADER_VECT_SCALE / ( cheapWaterEndDistance - cheapWaterStartDistance ),
				cheapWaterStartDistance / ( cheapWaterEndDistance - cheapWaterStartDistance ),
			};
			pShaderAPI->SetPixelShaderConstant( 1, cheapWaterParams );

			if( g_pConfig->bShowSpecular )
			{
				SetPixelShaderConstant( 2, REFLECTTINT, WATERBLENDFACTOR );
			}
			else
			{
				float zero[4] = { 0.0f, 0.0f, 0.0f, params[WATERBLENDFACTOR]->GetFloatValue() };
				pShaderAPI->SetPixelShaderConstant( 2, zero );
			}
		
			pShaderAPI->SetPixelShaderFogParams( PSREG_FOG_PARAMS );

			float vEyePos_SpecExponent[4];
			pShaderAPI->GetWorldSpaceCameraPosition( vEyePos_SpecExponent );
			vEyePos_SpecExponent[3] = 0.0f;
			pShaderAPI->SetPixelShaderConstant( PSREG_EYEPOS_SPEC_EXPONENT, vEyePos_SpecExponent, 1 );

			if( params[SCROLL1]->IsDefined())
			{
				float curtime=pShaderAPI->CurrentTime();
				float vc0[4];
				float v0[4];
				params[SCROLL1]->GetVecValue(v0,4);
				vc0[0]=curtime*v0[0];
				vc0[1]=curtime*v0[1];
				params[SCROLL2]->GetVecValue(v0,4);
				vc0[2]=curtime*v0[0];
				vc0[3]=curtime*v0[1];
				pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3, vc0, 1 );
			}

			DECLARE_DYNAMIC_VERTEX_SHADER( watercheap_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( watercheap_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( watercheap_ps20b );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( HDRENABLED,  IsHDREnabled() );
				SET_DYNAMIC_PIXEL_SHADER( watercheap_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( watercheap_ps20 );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( HDRENABLED,  IsHDREnabled() );
				SET_DYNAMIC_PIXEL_SHADER( watercheap_ps20 );
			}
		}
		Draw();
	}

	SHADER_DRAW
	{
		bool bRefraction = params[REFRACTTEXTURE]->IsTexture();
		bool bReflection = params[REFLECTTEXTURE]->IsTexture();
		bool bEnvMap = params[ENVMAP]->IsTexture() && ( params[FORCEENVMAP]->GetIntValue() == 1 );
		bool bForceCheap = ( params[FORCECHEAP]->GetIntValue() != 0 );

		if ( ( bReflection || bRefraction || bEnvMap ) && !UsingEditor( params ) && !bForceCheap )
		{
			#ifdef _GAMECONSOLE 
			{
				if ( IS_FLAG_SET( MATERIAL_VAR_PSEUDO_TRANSLUCENT ) )
				{
					// Do not render pseudo translucent water during the auto Z pass on Xbox 360
					if ( pShaderAPI )
					{
						pShaderAPI->EnablePredication( false, true );
					}
				}
			}
			#endif // _GAMECONSOLE 

			DrawReflectionRefraction( params, pShaderShadow, pShaderAPI, bReflection, bRefraction );

			#ifdef _GAMECONSOLE 
			{
				if ( IS_FLAG_SET( MATERIAL_VAR_PSEUDO_TRANSLUCENT ) )
				{
					if ( pShaderAPI )
					{
						pShaderAPI->DisablePredication();
					}
				}
			}
			#endif // _GAMECONSOLE 
		}
		else
		{
			bool bBlend = false;
			DrawCheapWater( params, pShaderShadow, pShaderAPI, bBlend, bRefraction );
		}
	}
END_SHADER

//-----------------------------------------------------------------------------
// This allows us to use a block labelled 'Water_DX9_HDR' in the water materials
//-----------------------------------------------------------------------------
BEGIN_INHERITED_SHADER( Water_DX9_HDR, Water_DX90,
			  "Help for Water_DX9_HDR" )

	SHADER_FALLBACK
	{
		if( g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE )
		{
			return "WATER_DX90";
		}
		return 0;
	}
END_INHERITED_SHADER

