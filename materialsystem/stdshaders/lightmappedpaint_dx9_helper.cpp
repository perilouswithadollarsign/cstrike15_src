//========= Copyright (c) 1996-2007, Valve LLC, All rights reserved. ============
//
// Purpose: Lightmapped paint shader
//
// $Header: $
// $NoKeywords: $
//=============================================================================

#include "lightmappedpaint_dx9_helper.h"
#include "BaseVSShader.h"
#include "shaderlib/commandbuilder.h"
#include "convar.h"
#include "lightmappedgeneric_vs20.inc"
#include "lightmappedpaint_ps20.inc"
#include "lightmappedpaint_ps20b.inc"

#include "tier0/vprof.h"
#include "shaderapifast.h"

#include "tier0/memdbgon.h"

extern ConVar mat_ambient_light_r;
extern ConVar mat_ambient_light_g;
extern ConVar mat_ambient_light_b;


void DrawLightmappedPaint_DX9( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI, IShaderShadow* pShaderShadow, 
								 LightmappedGeneric_DX9_Vars_t &info, CBasePerMaterialContextData **pContextDataPtr )
{
	SHADOW_STATE
	{
		pShader->SetInitialShadowState();
	}
	
	CLightmappedGeneric_DX9_Context *pContextData = reinterpret_cast< CLightmappedGeneric_DX9_Context *>( *pContextDataPtr );

	bool bHDR = g_pHardwareConfig->GetHDRType() != HDR_TYPE_NONE;

	if ( pShaderShadow || ( ! pContextData ) || pContextData->m_bMaterialVarsChanged )
	{
		static ConVarRef gpu_level( "gpu_level" );
		int nGPULevel = gpu_level.GetInt();
		bool bFullyOpaqueWithoutAlphaTest = false; 
		bool bFullyOpaque = false;
		bool bNeedRegenStaticCmds = (! pContextData ) || pShaderShadow;
		bool bThickPaint = ( nGPULevel > 1 );
		#ifdef _GAMECONSOLE
			bThickPaint = TRUE;
		#endif

		if ( ! pContextData )								// make sure allocated
		{
			pContextData = new CLightmappedGeneric_DX9_Context;
			*pContextDataPtr = pContextData;
		}

		bool hasBump = true; //g_pConfig->UseBumpmapping();
		bool hasSSBump = hasBump && (info.m_nSelfShadowedBumpFlag != -1) &&	( params[info.m_nSelfShadowedBumpFlag]->GetIntValue() );
		bool hasSelfIllum = IS_FLAG_SET( MATERIAL_VAR_SELFILLUM );
		
		bool bHasBlendModulateTexture = 
			(info.m_nBlendModulateTexture != -1) &&
			(params[info.m_nBlendModulateTexture]->IsTexture() );


		pContextData->m_bFullyOpaque = bFullyOpaque;
		pContextData->m_bFullyOpaqueWithoutAlphaTest = bFullyOpaqueWithoutAlphaTest;

		bool hasEnvmapMask = params[info.m_nEnvmapMask]->IsTexture();
				
		if ( pShaderShadow || bNeedRegenStaticCmds )
		{
			bool hasVertexColor = IS_FLAG_SET( MATERIAL_VAR_VERTEXCOLOR );
			
			bool hasEnvmap = params[info.m_nPaintEnvmap]->IsTexture();
			int envmap_variant; //0 = no envmap, 1 = regular, 2 = darken in shadow mode
			if( hasEnvmap )
			{
				//only enabled darkened cubemap mode when the scale calls for it. And not supported in ps20 when also using a 2nd bumpmap
				envmap_variant = ((GetFloatParam( info.m_nEnvMapLightScale, params ) > 0.0f) && g_pHardwareConfig->SupportsPixelShaders_2_b()) ? 2 : 1;
			}
			else
			{
				envmap_variant = 1; 
			}

			bool bSeamlessMapping = ( ( info.m_nSeamlessMappingScale != -1 ) && 
									  ( params[info.m_nSeamlessMappingScale]->GetFloatValue() != 0.0 ) );
			
			if ( bNeedRegenStaticCmds )
			{
				pContextData->ResetStaticCmds();
				CCommandBufferBuilder< CFixedCommandStorageBuffer< 5000 > > staticCmdsBuf;

#if 0
				int nLightingPreviewMode = IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER0 ) + 2 * IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER1 );
				if ( ( nLightingPreviewMode == ENABLE_FIXED_LIGHTING_OUTPUTNORMAL_AND_DEPTH ) && IsPC() )
				{
					staticCmdsBuf.SetVertexShaderNearAndFarZ( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6 );	// Needed for SSAO
				}
#endif

				staticCmdsBuf.BindStandardTexture( SHADER_SAMPLER1, bHDR ? TEXTURE_BINDFLAGS_NONE : TEXTURE_BINDFLAGS_SRGBREAD, TEXTURE_LIGHTMAP );

				//noise
				staticCmdsBuf.BindStandardTexture( SHADER_SAMPLER6, TEXTURE_BINDFLAGS_NONE, TEXTURE_SHADOW_NOISE_2D );

				//paint map
				staticCmdsBuf.BindStandardTexture( SHADER_SAMPLER9, TEXTURE_BINDFLAGS_NONE, TEXTURE_PAINT );

				if ( bSeamlessMapping )
				{
					staticCmdsBuf.SetVertexShaderConstant4(
						VERTEX_SHADER_SHADER_SPECIFIC_CONST_0,
						params[info.m_nSeamlessMappingScale]->GetFloatValue(),0,0,0 );
				}
				staticCmdsBuf.StoreEyePosInPixelShaderConstant( 10 );
#ifndef _PS3
				staticCmdsBuf.SetPixelShaderFogParams( 11 );
#endif
				staticCmdsBuf.End();
				// now, copy buf
				pContextData->m_pStaticCmds = new uint8[staticCmdsBuf.Size()];
				memcpy( pContextData->m_pStaticCmds, staticCmdsBuf.Base(), staticCmdsBuf.Size() );
			}
			if ( pShaderShadow )
			{
				pShader->EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );

				// Alpha test: FIXME: shouldn't this be handled in Shader_t::SetInitialShadowState
				//pShaderShadow->EnableAlphaTest( true );
				//pShaderShadow->AlphaFunc( SHADER_ALPHAFUNC_GREATER, 0.0f );
				
				
				unsigned int flags = VERTEX_POSITION;

				flags |= VERTEX_TANGENT_S | VERTEX_TANGENT_T | VERTEX_NORMAL;
							
				if( hasEnvmapMask )
				{
					pShaderShadow->EnableTexture( SHADER_SAMPLER5, true );
				}

				if( hasVertexColor )
				{
					flags |= VERTEX_COLOR;
				}

				// texcoord0 : base texcoord
				// texcoord1 : lightmap texcoord
				// texcoord2 : lightmap texcoord offset
				int numTexCoords;
				
				// if ( ShaderApiFast( pShaderAPI )->InEditorMode() )
// 				if ( pShader->CanUseEditorMaterials() )
// 				{
// 					numTexCoords = 1;
// 				}
// 				else
				{
					numTexCoords = 2;
					if( hasBump )
					{
						numTexCoords = 3;
					}
				}
		
				int nLightingPreviewMode = 0;
#if 0
				int nLightingPreviewMode = IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER0 ) + 2 * IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER1 );
#endif
				pShaderShadow->VertexShaderVertexFormat( flags, numTexCoords, 0, 0 );

				// Pre-cache pixel shaders

				int bumpmap_variant=(hasSSBump) ? 2 : hasBump;

				DECLARE_STATIC_VERTEX_SHADER( lightmappedgeneric_vs20 );
				SET_STATIC_VERTEX_SHADER_COMBO( ENVMAP_MASK,  hasEnvmapMask );
				SET_STATIC_VERTEX_SHADER_COMBO( TANGENTSPACE, 1 ); //need tangent transpose matrix for lighting
				SET_STATIC_VERTEX_SHADER_COMBO( BUMPMAP,  hasBump );
				SET_STATIC_VERTEX_SHADER_COMBO( VERTEXCOLOR, IS_FLAG_SET( MATERIAL_VAR_VERTEXCOLOR ) );
				SET_STATIC_VERTEX_SHADER_COMBO( VERTEXALPHATEXBLENDFACTOR, 0 );
				SET_STATIC_VERTEX_SHADER_COMBO( BUMPMASK, 0 );
				SET_STATIC_VERTEX_SHADER_COMBO( LIGHTING_PREVIEW, nLightingPreviewMode );
				SET_STATIC_VERTEX_SHADER_COMBO( SEAMLESS, bSeamlessMapping );
				SET_STATIC_VERTEX_SHADER_COMBO( DETAILTEXTURE, 0 );
				SET_STATIC_VERTEX_SHADER_COMBO( FANCY_BLENDING, bHasBlendModulateTexture );
				SET_STATIC_VERTEX_SHADER_COMBO( SELFILLUM,  hasSelfIllum );
				SET_STATIC_VERTEX_SHADER_COMBO( PAINT, 1 );
				SET_STATIC_VERTEX_SHADER_COMBO( ADDBUMPMAPS, 0 );
#if defined( _X360 ) || defined( _PS3 )
				SET_STATIC_VERTEX_SHADER_COMBO( FLASHLIGHT, 0);
#endif
				SET_STATIC_VERTEX_SHADER( lightmappedgeneric_vs20 );

#define TCOMBINE_NONE 12									// there is no detail texture

				if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
				{
					DECLARE_STATIC_PIXEL_SHADER( lightmappedpaint_ps20b );
					SET_STATIC_PIXEL_SHADER_COMBO( BUMPMAP,  bumpmap_variant );
					SET_STATIC_PIXEL_SHADER_COMBO( CUBEMAP,  envmap_variant );
					SET_STATIC_PIXEL_SHADER_COMBO( SEAMLESS, bSeamlessMapping );
					SET_STATIC_PIXEL_SHADER_COMBO( THICKPAINT, bThickPaint );
					SET_STATIC_PIXEL_SHADER( lightmappedpaint_ps20b );
					
				}
				else
				{
					DECLARE_STATIC_PIXEL_SHADER( lightmappedpaint_ps20 );
					SET_STATIC_PIXEL_SHADER_COMBO( BUMPMAP,  bumpmap_variant );
					SET_STATIC_PIXEL_SHADER_COMBO( CUBEMAP,  envmap_variant );
					SET_STATIC_PIXEL_SHADER_COMBO( SEAMLESS, bSeamlessMapping );
					SET_STATIC_PIXEL_SHADER_COMBO( THICKPAINT, bThickPaint );
					SET_STATIC_PIXEL_SHADER( lightmappedpaint_ps20 );
				}
				// HACK HACK HACK - enable alpha writes all the time so that we have them for
				// underwater stuff and writing depth to dest alpha
				// But only do it if we're not using the alpha already for translucency
				pShaderShadow->EnableAlphaWrites( bFullyOpaque );

				pShaderShadow->EnableSRGBWrite( true );

				pShader->DefaultFog();

				float flLScale = pShaderShadow->GetLightMapScaleFactor();
				pShader->PI_BeginCommandBuffer();
				pShader->PI_SetModulationPixelShaderDynamicState( 21 );

				pShader->PI_SetModulationPixelShaderDynamicState_LinearScale_ScaleInW( 12, flLScale );
				pShader->PI_SetModulationVertexShaderDynamicState_LinearScale( flLScale );
				pShader->PI_EndCommandBuffer();
			} // end shadow state
		} // end shadow || regen display list

		if ( pShaderAPI && ( pContextData->m_bMaterialVarsChanged ) )
		{
			// need to regenerate the semistatic cmds
			pContextData->m_SemiStaticCmdsOut.Reset();
			pContextData->m_bMaterialVarsChanged = false;

			// If we don't have a texture transform, we don't have
			// to set vertex shader constants or run vertex shader instructions
			// for the texture transform.
			bool bHasTextureTransform = 
				!( params[info.m_nBaseTextureTransform]->MatrixIsIdentity() &&
				   params[info.m_nBumpTransform]->MatrixIsIdentity() &&
				   params[info.m_nBumpTransform2]->MatrixIsIdentity() &&
				   params[info.m_nEnvmapMaskTransform]->MatrixIsIdentity() );
						
			pContextData->m_bVertexShaderFastPath = !bHasTextureTransform;

			if( params[info.m_nDetail]->IsTexture() )
			{
				pContextData->m_bVertexShaderFastPath = false;
			}
			int nTransformToLoad = -1;
			if ( ( hasBump || hasSSBump ) && !hasSelfIllum && !bHasBlendModulateTexture )
			{
				nTransformToLoad = info.m_nBumpTransform;
			}
			pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureTransform( 
				VERTEX_SHADER_SHADER_SPECIFIC_CONST_10, nTransformToLoad );

			if ( ! pContextData->m_bVertexShaderFastPath )
			{
				bool bSeamlessMapping = ( ( info.m_nSeamlessMappingScale != -1 ) && 
										  ( params[info.m_nSeamlessMappingScale]->GetFloatValue() != 0.0 ) );
				bool hasEnvmapMask = params[info.m_nEnvmapMask]->IsTexture();
				if (!bSeamlessMapping )
					pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, info.m_nBaseTextureTransform );
				// If we have a detail texture, then the bump texcoords are the same as the base texcoords.
				if( hasBump )
				{
					pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_2, info.m_nBumpTransform );
				}
				if( hasEnvmapMask )
				{
					pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_4, info.m_nEnvmapMaskTransform );
				}
			}
			pContextData->m_SemiStaticCmdsOut.SetEnvMapTintPixelShaderDynamicState( 0, info.m_nEnvmapTint );
			
			float envmapTintVal[4];
			float selfIllumTintVal[4];
			params[info.m_nEnvmapTint]->GetVecValue( envmapTintVal, 3 );
			params[info.m_nSelfIllumTint]->GetVecValue( selfIllumTintVal, 3 );
			float envmapContrast = params[info.m_nEnvmapContrast]->GetFloatValue();
			float envmapSaturation = params[info.m_nEnvmapSaturation]->GetFloatValue();
			float fresnelReflection = params[info.m_nFresnelReflection]->GetFloatValue();
			bool hasEnvmap = params[info.m_nPaintEnvmap]->IsTexture();
			int envmap_variant; //0 = no envmap, 1 = regular, 2 = darken in shadow mode
			if( hasEnvmap )
			{
				//only enabled darkened cubemap mode when the scale calls for it. And not supported in ps20 when also using a 2nd bumpmap
				envmap_variant = ((GetFloatParam( info.m_nEnvMapLightScale, params ) > 0.0f) && g_pHardwareConfig->SupportsPixelShaders_2_b()) ? 2 : 1;
			}
			else
			{
				envmap_variant = 0; 
			}

			pContextData->m_bPixelShaderFastPath = true;
			bool bUsingContrastOrSaturation = hasEnvmap && ( ( (envmapContrast != 0.0f) && (envmapContrast != 1.0f) ) || (envmapSaturation != 1.0f) );
			bool bUsingFresnel = hasEnvmap && (fresnelReflection != 1.0f);
			bool bUsingSelfIllumTint = IS_FLAG_SET(MATERIAL_VAR_SELFILLUM) && (selfIllumTintVal[0] != 1.0f || selfIllumTintVal[1] != 1.0f || selfIllumTintVal[2] != 1.0f); 
			if ( bUsingContrastOrSaturation || bUsingFresnel || bUsingSelfIllumTint || !g_pConfig->bShowSpecular )
			{
				pContextData->m_bPixelShaderFastPath = false;
			}
			if( !pContextData->m_bPixelShaderFastPath )
			{
				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstants( 2, 3 );
				pContextData->m_SemiStaticCmdsOut.OutputConstantData( params[info.m_nEnvmapContrast]->GetVecValue() );
				pContextData->m_SemiStaticCmdsOut.OutputConstantData( params[info.m_nEnvmapSaturation]->GetVecValue() );
				float flFresnel = params[info.m_nFresnelReflection]->GetFloatValue();
				// [ 0, 0, 1-R(0), R(0) ]
				pContextData->m_SemiStaticCmdsOut.OutputConstantData4( 0., 0., 1.0 - flFresnel, flFresnel );
				
				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 7, params[info.m_nSelfIllumTint]->GetVecValue() );
			}

			// parallax and cubemap light scale mapping parms (c20)
			if ( envmap_variant == 2 )
			{
				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant4( 20, 0, GetFloatParam( info.m_nEnvMapLightScale, params), 0, 0 );
			}

			// texture binds
			
			// handle mat_fullbright 2
			bool bLightingOnly = g_pConfig->nFullbright == 2 && !IS_FLAG_SET( MATERIAL_VAR_NO_DEBUG_OVERRIDE );
			if( bLightingOnly )
			{
				// disable color modulation
				float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderConstant( VERTEX_SHADER_MODULATION_COLOR, color );

				// turn off environment mapping
				envmapTintVal[0] = 0.0f;
				envmapTintVal[1] = 0.0f;
				envmapTintVal[2] = 0.0f;
			}

			if( params[info.m_nPaintSplatNormal]->IsTexture() )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER7, TEXTURE_BINDFLAGS_NONE, info.m_nPaintSplatNormal );
			}
			else
			{
				pContextData->m_SemiStaticCmdsOut.BindStandardTexture( SHADER_SAMPLER7, TEXTURE_BINDFLAGS_NONE, TEXTURE_NORMALMAP_FLAT );
			}

			if( params[info.m_nPaintSplatBubbleLayout]->IsTexture() )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER4, TEXTURE_BINDFLAGS_NONE, info.m_nPaintSplatBubbleLayout );
			}
			else
			{
				pContextData->m_SemiStaticCmdsOut.BindStandardTexture( SHADER_SAMPLER4, TEXTURE_BINDFLAGS_NONE, TEXTURE_BLACK );
			}

			if( params[info.m_nPaintSplatBubble]->IsTexture() )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER5, TEXTURE_BINDFLAGS_NONE, info.m_nPaintSplatBubble );
			}
			else
			{
				pContextData->m_SemiStaticCmdsOut.BindStandardTexture( SHADER_SAMPLER5, TEXTURE_BINDFLAGS_NONE, TEXTURE_NORMALMAP_FLAT );
			}

			if ( bHasBlendModulateTexture )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, info.m_nBlendModulateTexture, -1 );
			}

			pContextData->m_SemiStaticCmdsOut.End();
		}
	}
	DYNAMIC_STATE
	{
		ShaderApiFast( pShaderAPI )->SetDefaultState();
#ifdef _PS3
		CCommandBufferBuilder< CDynamicCommandStorageBuffer > DynamicCmdsOut;
		ShaderApiFast( pShaderAPI )->ExecuteCommandBuffer( pContextData->m_pStaticCmds );
		ShaderApiFast( pShaderAPI )->ExecuteCommandBuffer( pContextData->m_SemiStaticCmdsOut.Base() );
#else
		CCommandBufferBuilder< CFixedCommandStorageBuffer< 1000 > > DynamicCmdsOut;
		DynamicCmdsOut.Call( pContextData->m_pStaticCmds );
		DynamicCmdsOut.Call( pContextData->m_SemiStaticCmdsOut.Base() );
#endif

		bool hasEnvmap = params[info.m_nPaintEnvmap]->IsTexture();

		if ( hasEnvmap )
		{
			DynamicCmdsOut.BindEnvCubemapTexture( pShader, SHADER_SAMPLER2, bHDR ? TEXTURE_BINDFLAGS_NONE : TEXTURE_BINDFLAGS_SRGBREAD, info.m_nPaintEnvmap, info.m_nEnvmapFrame );
		}

		bool bVertexShaderFastPath = pContextData->m_bVertexShaderFastPath;

		int nFixedLightingMode = ShaderApiFast( pShaderAPI )->GetIntRenderingParameter( INT_RENDERPARM_ENABLE_FIXED_LIGHTING );
		if( nFixedLightingMode != ENABLE_FIXED_LIGHTING_NONE )
		{
			bVertexShaderFastPath = false;
		}

		float vEyePos[4];
		ShaderApiFast( pShaderAPI )->GetWorldSpaceCameraPosition( vEyePos );
		DynamicCmdsOut.SetVertexShaderConstant4( 12, vEyePos[0], vEyePos[1], vEyePos[2], 0.0f );

		// These constants are used to rotate the world space water normals around the up axis to align the
		// normal with the camera and then give us a 2D offset vector to use for reflection and refraction uv's
		VMatrix mView;
		ShaderApiFast( pShaderAPI )->GetMatrix( MATERIAL_VIEW, mView.m[0] );
		mView = mView.Transpose3x3();

		Vector4D vCameraRight( mView.m[0][0], mView.m[0][1], mView.m[0][2], 0.0f );
		vCameraRight.z = 0.0f; // Project onto the plane of water
		vCameraRight.AsVector3D().NormalizeInPlace();

		Vector4D vCameraForward;
		CrossProduct( Vector( 0.0f, 0.0f, 1.0f ), vCameraRight.AsVector3D(), vCameraForward.AsVector3D() ); // I assume the water surface normal is pointing along z!

		ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( 22, vCameraRight.Base() );
		ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( 23, vCameraForward.Base() );

		MaterialFogMode_t fogType = ShaderApiFast( pShaderAPI )->GetSceneFogMode();
		DECLARE_DYNAMIC_VERTEX_SHADER( lightmappedgeneric_vs20 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( FASTPATH,  bVertexShaderFastPath );
		SET_DYNAMIC_VERTEX_SHADER_CMD( DynamicCmdsOut, lightmappedgeneric_vs20 );

		// This block of logic is up here and is so verbose because the compiler on the Mac was previously
		// optimizing much of this away and allowing out of range values into the logic which ultimately
		// computes the dynamic index.  Please leave this here and don't try to weave it into the dynamic
		// combo setting macros below.
		bool bPixelShaderFastPath = false;
		if ( pContextData->m_bPixelShaderFastPath )
			bPixelShaderFastPath = true;
		bool bFastPath = false;
		if ( bPixelShaderFastPath )
			bFastPath = true;


		if ( nFixedLightingMode != ENABLE_FIXED_LIGHTING_NONE )
		{
			bPixelShaderFastPath = false;
		}
		bool bWriteDepthToAlpha;
		bool bWriteWaterFogToAlpha;
		if(  pContextData->m_bFullyOpaque ) 
		{
			bWriteDepthToAlpha = ShaderApiFast( pShaderAPI )->ShouldWriteDepthToDestAlpha();
			bWriteWaterFogToAlpha = (fogType == MATERIAL_FOG_LINEAR_BELOW_FOG_Z);
			AssertMsg( !(bWriteDepthToAlpha && bWriteWaterFogToAlpha), "Can't write two values to alpha at the same time." );
		}
		else
		{
			//can't write a special value to dest alpha if we're actually using as-intended alpha
			bWriteDepthToAlpha = false;
			bWriteWaterFogToAlpha = false;
		}


		// only do ambient light when not using flashlight
		float vAmbientColor[4] = { mat_ambient_light_r.GetFloat(), mat_ambient_light_g.GetFloat(), mat_ambient_light_b.GetFloat(), 0.0f };
		if ( g_pConfig->nFullbright == 1 )
		{
			vAmbientColor[0] = vAmbientColor[1] = vAmbientColor[2] = 0.0f;
		}
		DynamicCmdsOut.SetPixelShaderConstant( 31, vAmbientColor, 1 );

		float envmapContrast = params[info.m_nEnvmapContrast]->GetFloatValue();
		if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( lightmappedpaint_ps20b );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( FASTPATH,  bFastPath  );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( FASTPATHENVMAPCONTRAST,  bPixelShaderFastPath && envmapContrast == 1.0f );
			
			// Don't write fog to alpha if we're using translucency
			SET_DYNAMIC_PIXEL_SHADER_CMD( DynamicCmdsOut, lightmappedpaint_ps20b );
		}
		else
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( lightmappedpaint_ps20 );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( FASTPATH,  bPixelShaderFastPath );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( FASTPATHENVMAPCONTRAST,  bPixelShaderFastPath && envmapContrast == 1.0f );
			
			// Don't write fog to alpha if we're using translucency
			SET_DYNAMIC_PIXEL_SHADER_CMD( DynamicCmdsOut, lightmappedpaint_ps20 );
		}

		DynamicCmdsOut.End();
#ifdef _PS3
		ShaderApiFast( pShaderAPI )->SetPixelShaderFogParams( 11 );
#endif
		ShaderApiFast( pShaderAPI )->ExecuteCommandBuffer( DynamicCmdsOut.Base() );
	}
	pShader->Draw();
}
