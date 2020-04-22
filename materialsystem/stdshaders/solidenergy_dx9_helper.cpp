//========= Copyright (c) Valve Corporation, All rights reserved. ============//

#include "BaseVSShader.h"
#include "solidenergy_dx9_helper.h"
#include "cpp_shader_constant_register_map.h"

// Auto generated inc files
#include "solidenergy_vs20.inc"
#include "solidenergy_ps20b.inc"
#include "common_hlsl_cpp_consts.h"

#if !defined( _GAMECONSOLE )
	#include "solidenergy_vs30.inc"
	#include "solidenergy_ps30.inc"
#endif

#include "shaderlib/commandbuilder.h"

void InitParamsSolidEnergy( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, SolidEnergyVars_t &info )
{
	static ConVarRef gpu_level( "gpu_level" );
	int nGPULevel = gpu_level.GetInt();

	// Set material parameter default values
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nDetail1Scale, kDefaultDetailScale );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nDetail1Frame, kDefaultDetailFrame );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nDetail1BlendMode, kDefaultDetailBlendMode );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nDetail2Scale, kDefaultDetailScale );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nDetail2Frame, kDefaultDetailFrame );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nDetail2BlendMode, kDefaultDetailBlendMode );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nDepthBlendScale, kDefaultDepthBlendScale );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nNeedsTangentT, IS_PARAM_DEFINED( info.m_nTangentTOpacityRanges ) && ( nGPULevel > 1 ) );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nNeedsTangentS, IS_PARAM_DEFINED( info.m_nTangentSOpacityRanges )&& ( nGPULevel > 1 ) );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nNeedsNormals, IS_PARAM_DEFINED( info.m_nFresnelOpacityRanges )&& ( nGPULevel > 1 ) );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nFlowWorldUVScale, kDefaultDetailScale );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nFlowNormalUVScale, kDefaultDetailScale );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nFlowTimeIntervalInSeconds, kDefaultTimescale );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nFlowUVScrollDistance, kDefaultScrollDist );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nFlowNoiseScale, kDefaultNoiseScale );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nPowerUp, kDefaultPowerUpIntensity );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nFlowColorIntensity, kDefaultIntensity );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nFlowVortexSize, kDefaultVortexSize );
	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nFlowColor, kDefaultFieldColor, 3 );
	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nFlowVortexColor, kDefaultVortexColor, 3 );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nFlowCheap, nGPULevel < 2 );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nModel, 0 );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nOutputIntensity, 1.0f );

	if ( nGPULevel < 2 )
	{
		params[info.m_nDetail2Texture]->SetUndefined();
	}
}

void InitSolidEnergy( CBaseVSShader *pShader, IMaterialVar** params, SolidEnergyVars_t &info )
{
	// Load textures
	if ( ( info.m_nBaseTexture != -1 ) && params[info.m_nBaseTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nBaseTexture, TEXTUREFLAGS_SRGB );
	}
	if ( ( info.m_nDetail1Texture != -1 ) && params[info.m_nDetail1Texture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nDetail1Texture, TEXTUREFLAGS_SRGB );
	}
	if ( ( info.m_nDetail2Texture != -1 ) && params[info.m_nDetail2Texture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nDetail2Texture, TEXTUREFLAGS_SRGB );
	}
	if ( ( info.m_nFlowMap != -1 ) && params[info.m_nFlowMap]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nFlowMap );
		if ( ( info.m_nFlowNoiseTexture != -1 ) && params[info.m_nFlowNoiseTexture]->IsDefined() )
		{
			pShader->LoadTexture( info.m_nFlowNoiseTexture );
		}
		if ( ( info.m_nFlowBoundsTexture != -1 ) && params[info.m_nFlowBoundsTexture]->IsDefined() )
		{
			pShader->LoadTexture( info.m_nFlowBoundsTexture );
		}
	}

	if ( ( info.m_nModel != -1 ) && ( params[info.m_nModel]->GetIntValue() != 0 ) )
	{
		SET_FLAGS( MATERIAL_VAR_MODEL );
	}

	SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
}

class CSolidEnergy_DX9_Context : public CBasePerMaterialContextData
{
public:
	CCommandBufferBuilder< CFixedCommandStorageBuffer< 800 > > m_SemiStaticCmdsOut;
};

void DrawSolidEnergy(  CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
					IShaderShadow* pShaderShadow, SolidEnergyVars_t &info, VertexCompressionType_t vertexCompression,
					CBasePerMaterialContextData **pContextDataPtr )
{
	CSolidEnergy_DX9_Context *pContextData = reinterpret_cast< CSolidEnergy_DX9_Context *> ( *pContextDataPtr );

	bool bAlphaBlend = IS_FLAG_SET( MATERIAL_VAR_TRANSLUCENT );
	bool bDetail1 = ( info.m_nDetail1Texture != -1 ) && params[info.m_nDetail1Texture]->IsTexture();
	bool bDetail2 = bDetail1 && ( info.m_nDetail2Texture != -1 ) && params[info.m_nDetail2Texture]->IsTexture();
	bool bDepthBlend = bAlphaBlend && ( info.m_nDepthBlend != -1 ) && ( params[info.m_nDepthBlend]->GetIntValue() != 0 );
	
	bool bHasFlowmap = !bDetail1 && ( info.m_nFlowMap != -1 ) && params[info.m_nFlowMap]->IsTexture();
	bool bHasCheapFlow = !( IsGameConsole() ) && bHasFlowmap && ( params[info.m_nFlowCheap]->GetIntValue() != 0 );

	if ( pShader->IsSnapshotting() || (! pContextData ) || ( pContextData->m_bMaterialVarsChanged ) )
	{
		bool bAdditiveBlend = IS_FLAG_SET( MATERIAL_VAR_ADDITIVE );
		bool bHasVertexColor = IS_FLAG_SET( MATERIAL_VAR_VERTEXCOLOR );
		bool bHasVertexAlpha = IS_FLAG_SET( MATERIAL_VAR_VERTEXALPHA );
		bool bModel = IS_FLAG_SET( MATERIAL_VAR_MODEL );

		bool bTangentT = ( info.m_nNeedsTangentT != -1 ) && params[info.m_nNeedsTangentT]->GetIntValue();
		bool bTangentS = ( info.m_nNeedsTangentS != -1 ) && params[info.m_nNeedsTangentS]->GetIntValue();
		if ( bTangentS && bTangentT ) // If both on, T wins
		{
			bTangentS = false;
		}

		bool bFresnel = !bTangentS && !bTangentT && ( info.m_nNeedsNormals != -1 ) && params[info.m_nNeedsNormals]->GetIntValue();
		int nDetail1BlendMode = ( info.m_nDetail1BlendMode != -1 ) ? params[info.m_nDetail1BlendMode]->GetIntValue() : kDefaultDetailBlendMode;
		nDetail1BlendMode = bDetail1 ? clamp( nDetail1BlendMode, 0, kMaxDetailBlendMode ) : 0;
		int nDetail2BlendMode = ( info.m_nDetail2BlendMode != -1 ) ? params[info.m_nDetail2BlendMode]->GetIntValue() : kDefaultDetailBlendMode;
		nDetail2BlendMode = bDetail2 ? clamp( nDetail2BlendMode, 0, kMaxDetailBlendMode ) : 0;

		if ( pShader->IsSnapshotting() )
		{
			// Set stream format (note that this shader supports compression)
			int userDataSize = 0;
			unsigned int flags = VERTEX_POSITION | VERTEX_FORMAT_COMPRESSED;
			if ( !bModel && ( bTangentS || bTangentT || bHasFlowmap ) )
			{
				flags |= VERTEX_TANGENT_S;
				flags |= VERTEX_TANGENT_T;
			}
			if ( bModel && ( bTangentS || bTangentT || bHasFlowmap ) )
			{
				flags |= VERTEX_USERDATA_SIZE( 4 );
				userDataSize = 4;
			}
			if ( bModel || bFresnel || bTangentS || bTangentT || bHasFlowmap )
			{
				flags |= VERTEX_NORMAL;
			}
			if ( bHasVertexColor || bHasVertexAlpha )
			{
				flags |= VERTEX_COLOR;
			}

			int nTexCoordCount = 1;
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );

			// Vertex Shader
#if !defined( _GAMECONSOLE )
			if ( g_pHardwareConfig->GetDXSupportLevel() < 95 )
#endif
			{
				DECLARE_STATIC_VERTEX_SHADER( solidenergy_vs20 );
				SET_STATIC_VERTEX_SHADER_COMBO( VERTEXCOLOR,  bHasVertexColor || bHasVertexAlpha );
				SET_STATIC_VERTEX_SHADER_COMBO( DETAIL1, bDetail1 );
				SET_STATIC_VERTEX_SHADER_COMBO( DETAIL2, bDetail2 );
				SET_STATIC_VERTEX_SHADER_COMBO( TANGENTTOPACITY, bTangentT );
				SET_STATIC_VERTEX_SHADER_COMBO( TANGENTSOPACITY, bTangentS );
				SET_STATIC_VERTEX_SHADER_COMBO( FRESNELOPACITY, bFresnel );
				SET_STATIC_VERTEX_SHADER_COMBO( FLOWMAP, bHasFlowmap );
				SET_STATIC_VERTEX_SHADER_COMBO( MODELFORMAT, bModel );
				SET_STATIC_VERTEX_SHADER( solidenergy_vs20 );
			}
#if !defined( _GAMECONSOLE )
			else
			{
				DECLARE_STATIC_VERTEX_SHADER( solidenergy_vs30 );
				SET_STATIC_VERTEX_SHADER_COMBO( VERTEXCOLOR,  bHasVertexColor || bHasVertexAlpha );
				SET_STATIC_VERTEX_SHADER_COMBO( DETAIL1, bDetail1 );
				SET_STATIC_VERTEX_SHADER_COMBO( DETAIL2, bDetail2 );
				SET_STATIC_VERTEX_SHADER_COMBO( TANGENTTOPACITY, bTangentT );
				SET_STATIC_VERTEX_SHADER_COMBO( TANGENTSOPACITY, bTangentS );
				SET_STATIC_VERTEX_SHADER_COMBO( FRESNELOPACITY, bFresnel );
				SET_STATIC_VERTEX_SHADER_COMBO( FLOWMAP, bHasFlowmap );
				SET_STATIC_VERTEX_SHADER_COMBO( MODELFORMAT, bModel );
				SET_STATIC_VERTEX_SHADER( solidenergy_vs30 );
			}
#endif

			// Pixel Shader
#if !defined( _GAMECONSOLE )
			if ( g_pHardwareConfig->GetDXSupportLevel() < 95 )
#endif
			{
				DECLARE_STATIC_PIXEL_SHADER( solidenergy_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( ADDITIVE, bAdditiveBlend );
				SET_STATIC_PIXEL_SHADER_COMBO( DETAIL1, bDetail1 );
				SET_STATIC_PIXEL_SHADER_COMBO( DETAIL2, bDetail2 );
				SET_STATIC_PIXEL_SHADER_COMBO( DETAIL1BLENDMODE, nDetail1BlendMode );
				SET_STATIC_PIXEL_SHADER_COMBO( DETAIL2BLENDMODE, nDetail2BlendMode );
				SET_STATIC_PIXEL_SHADER_COMBO( VERTEXCOLOR, ( bHasVertexColor || bHasVertexAlpha ) );
				SET_STATIC_PIXEL_SHADER_COMBO( TANGENTTOPACITY, bTangentT );
				SET_STATIC_PIXEL_SHADER_COMBO( TANGENTSOPACITY, bTangentS );
				SET_STATIC_PIXEL_SHADER_COMBO( FRESNELOPACITY, bFresnel );
				SET_STATIC_PIXEL_SHADER_COMBO( DEPTHBLEND, bDepthBlend );
				SET_STATIC_PIXEL_SHADER_COMBO( FLOWMAP, bHasFlowmap );
				SET_STATIC_PIXEL_SHADER_COMBO( FLOW_CHEAP, bHasCheapFlow );
				SET_STATIC_PIXEL_SHADER( solidenergy_ps20b );
			}
#if !defined( _GAMECONSOLE )
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( solidenergy_ps30 );
				SET_STATIC_PIXEL_SHADER_COMBO( ADDITIVE, bAdditiveBlend );
				SET_STATIC_PIXEL_SHADER_COMBO( DETAIL1, bDetail1 );
				SET_STATIC_PIXEL_SHADER_COMBO( DETAIL2, bDetail2 );
				SET_STATIC_PIXEL_SHADER_COMBO( DETAIL1BLENDMODE, nDetail1BlendMode );
				SET_STATIC_PIXEL_SHADER_COMBO( DETAIL2BLENDMODE, nDetail2BlendMode );
				SET_STATIC_PIXEL_SHADER_COMBO( VERTEXCOLOR, ( bHasVertexColor || bHasVertexAlpha ) );
				SET_STATIC_PIXEL_SHADER_COMBO( TANGENTTOPACITY, bTangentT );
				SET_STATIC_PIXEL_SHADER_COMBO( TANGENTSOPACITY, bTangentS );
				SET_STATIC_PIXEL_SHADER_COMBO( FRESNELOPACITY, bFresnel );
				SET_STATIC_PIXEL_SHADER_COMBO( DEPTHBLEND, bDepthBlend );
				SET_STATIC_PIXEL_SHADER_COMBO( FLOWMAP, bHasFlowmap );
				SET_STATIC_PIXEL_SHADER_COMBO( FLOW_CHEAP, bHasCheapFlow );
				SET_STATIC_PIXEL_SHADER( solidenergy_ps30 );
			}
#endif

			// Textures
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );		// [sRGB] Base
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, true );

			if( bDetail1 )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );	// [sRGB] Detail11
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, true );
			}

			if( bDetail2 )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );	// [sRGB] Detail12
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER4, true );
			}

			if ( bDepthBlend )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER3, true ); // Depth (consoles only)
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER3, false );
			}

			if ( bHasFlowmap )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER5, true ); // Flow map
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER5, false );

				pShaderShadow->EnableTexture( SHADER_SAMPLER6, true ); // Flow map noise
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER6, false );

				pShaderShadow->EnableTexture( SHADER_SAMPLER7, true ); // Flow map bounds
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER7, false );
			}

			if ( bAlphaBlend )
			{
				if ( bAdditiveBlend )
				{
					pShader->EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE );
				}
				else
				{
					pShader->EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				}
				pShaderShadow->EnableAlphaWrites( false );
				pShaderShadow->EnableDepthWrites( !bDepthBlend && !bAdditiveBlend );
			}
			else
			{
				pShader->DisableAlphaBlending();
				pShaderShadow->EnableAlphaWrites( true );
				pShaderShadow->EnableDepthWrites( true );
			}

			pShaderShadow->EnableSRGBWrite( true );
		}
		if ( pShaderAPI && ( (! pContextData ) || ( pContextData->m_bMaterialVarsChanged ) ) )
		{
			if ( !pContextData )								// make sure allocated
			{
				pContextData = new CSolidEnergy_DX9_Context;
				*pContextDataPtr = pContextData;
			}
			pContextData->m_bMaterialVarsChanged = false;
			pContextData->m_SemiStaticCmdsOut.Reset();

			///////////////////////////
			// Semi-static block
			///////////////////////////

			float flConsts[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

			// VS consts

			if ( info.m_nBaseTextureTransform != -1 )
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, info.m_nBaseTextureTransform ); // 0-1

			if ( IS_PARAM_DEFINED( info.m_nDetail1TextureTransform ) )
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureScaledTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_2, info.m_nDetail1TextureTransform, info.m_nDetail1Scale ); // 2-3
			else
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureScaledTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_2, info.m_nBaseTextureTransform, info.m_nDetail1Scale );

			if ( IS_PARAM_DEFINED( info.m_nDetail2TextureTransform ) )
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureScaledTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6, info.m_nDetail2TextureTransform, info.m_nDetail2Scale ); // 6-7
			else
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureScaledTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6, info.m_nBaseTextureTransform, info.m_nDetail2Scale );

			// PS Constants
			pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, BASETEXTURE, -1 );

			if( bHasFlowmap )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER5, TEXTURE_BINDFLAGS_NONE, info.m_nFlowMap, info.m_nFlowMapFrame );
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER6, TEXTURE_BINDFLAGS_NONE, info.m_nFlowNoiseTexture, -1 );
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER7, TEXTURE_BINDFLAGS_NONE, info.m_nFlowBoundsTexture, -1 );
			}

			if( IS_PARAM_DEFINED( info.m_nTangentTOpacityRanges ) )
				params[info.m_nTangentTOpacityRanges]->GetVecValue( flConsts, 4 );
			else
				memcpy( flConsts, kDefaultFalloffRanges, sizeof( kDefaultFalloffRanges ) );
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 0, flConsts, 1 );

			if( IS_PARAM_DEFINED( info.m_nTangentSOpacityRanges ) )
				params[info.m_nTangentSOpacityRanges]->GetVecValue( flConsts, 4 );
			else
				memcpy( flConsts, kDefaultFalloffRanges, sizeof( kDefaultFalloffRanges ) );
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 1, flConsts, 1 );

			if( IS_PARAM_DEFINED( info.m_nFresnelOpacityRanges ) )
				params[info.m_nFresnelOpacityRanges]->GetVecValue( flConsts, 4 );
			else
				memcpy( flConsts, kDefaultFalloffRanges, sizeof( kDefaultFalloffRanges ) );
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 2, flConsts, 1 );

#ifndef _PS3
			pContextData->m_SemiStaticCmdsOut.SetDepthFeatheringShaderConstants( 4, params[info.m_nDepthBlendScale]->GetFloatValue() );
#endif

			float flOutputIntensity = params[ info.m_nOutputIntensity ]->GetFloatValue();

			if ( bHasFlowmap )
			{
				float vFlowConst1[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				vFlowConst1[0] = params[ info.m_nFlowWorldUVScale ]->GetFloatValue();
				vFlowConst1[1] = 0.0f; // Empty
				vFlowConst1[2] = 0.0f; // Empty
				vFlowConst1[3] = flOutputIntensity;
				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 6, vFlowConst1, 1 );

				float vFlowConst2[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				vFlowConst2[0] = params[ info.m_nFlowTimeIntervalInSeconds ]->GetFloatValue();
				vFlowConst2[1] = params[ info.m_nFlowUVScrollDistance ]->GetFloatValue();
				vFlowConst2[2] = 0.0f;
				vFlowConst2[3] = params[ info.m_nFlowLerpExp ]->GetFloatValue();
				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 7, vFlowConst2, 1 );

				float vFlowConst3[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				params[info.m_nFlowColor]->GetVecValue( vFlowConst3, 3 );
				vFlowConst3[3] = 0.0f; // Empty
				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 8, vFlowConst3, 1 );

				float vFlowConst4[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				params[info.m_nFlowVortexColor]->GetVecValue( vFlowConst4, 3 );
				vFlowConst4[3] = params[ info.m_nFlowVortexSize ]->GetFloatValue();
				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 9, vFlowConst4, 1 );
			}
			else
			{
				float vFlowConst1[4] = { 0.0f, 0.0f, 0.0f, flOutputIntensity };
				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 6, vFlowConst1, 1 );
			}

			pContextData->m_SemiStaticCmdsOut.End();
		}
	}
	if ( pShaderAPI ) //DYNAMIC_STATE
	{
		if ( IsPC() && pShaderAPI->InFlashlightMode() )
		{
			// Don't draw anything for the flashlight pass
			pShader->Draw( false );
			return;
		}

#ifdef _PS3
		CCommandBufferBuilder< CDynamicCommandStorageBuffer > DynamicCmdsOut;
		ShaderApiFast( pShaderAPI )->ExecuteCommandBuffer( pContextData->m_SemiStaticCmdsOut.Base() );
#else
		CCommandBufferBuilder< CFixedCommandStorageBuffer< 400 > > DynamicCmdsOut;
		DynamicCmdsOut.Call( pContextData->m_SemiStaticCmdsOut.Base() );
#endif



		///////////////////////////
		// dynamic block
		///////////////////////////

		bool bWriteDepthToAlpha = pShaderAPI->ShouldWriteDepthToDestAlpha() && !bAlphaBlend;
		float flPowerUp = params[ info.m_nPowerUp ]->GetFloatValue();
		float flIntensity = params[info.m_nFlowColorIntensity]->GetFloatValue();

		bool bActive = ( flIntensity > 0.0f );
		if ( ( bHasFlowmap ) && ( flPowerUp <= 0.0f ) )
		{
			bActive = false;
		}

		bool bPowerup = bActive && bHasFlowmap && ( flPowerUp > 0.0f && flPowerUp < 1.0f );
		bool bVortex1 = bActive && bHasFlowmap && ( info.m_nFlowVortex1 != -1 ) && ( params[info.m_nFlowVortex1]->GetIntValue() != 0 );
		bool bVortex2 = bActive && bHasFlowmap && ( info.m_nFlowVortex2 != -1 ) && ( params[info.m_nFlowVortex2]->GetIntValue() != 0 );

		// VERTEX SHADER SETUP
#if !defined( _GAMECONSOLE )
		if ( g_pHardwareConfig->GetDXSupportLevel() < 95 )
#endif
		{
			DECLARE_DYNAMIC_VERTEX_SHADER( solidenergy_vs20 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING,  pShaderAPI->GetCurrentNumBones() > 0 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( VORTEX1, bVortex1 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( VORTEX2, bVortex2 );
			SET_DYNAMIC_VERTEX_SHADER( solidenergy_vs20 );
		}
#if !defined( _GAMECONSOLE )
		else
		{
			DECLARE_DYNAMIC_VERTEX_SHADER( solidenergy_vs30 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING,  pShaderAPI->GetCurrentNumBones() > 0 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( VORTEX1, bVortex1 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( VORTEX2, bVortex2 );
			SET_DYNAMIC_VERTEX_SHADER( solidenergy_vs30 );
		}
#endif

		// VS constants
		float flConsts[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		// Get viewport and render target dimensions and set shader constant to do a 2D mad
		int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
		pShaderAPI->GetCurrentViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );

		int nRtWidth, nRtHeight;
		pShaderAPI->GetCurrentRenderTargetDimensions( nRtWidth, nRtHeight );

		pShaderAPI->GetWorldSpaceCameraPosition( flConsts );
		flConsts[3] = 0.0f;
		DynamicCmdsOut.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_4, flConsts, 1 );

		// Compute viewport mad that takes projection space coords (post divide by W) into normalized screenspace, taking into account the currently set viewport.
		flConsts[0] =  .5f * ( ( float )nViewportWidth / ( float )nRtWidth );
		flConsts[1] = -.5f * ( ( float )nViewportHeight / ( float )nRtHeight );
		flConsts[2] =  flConsts[0] + ( ( float )nViewportX / ( float )nRtWidth );
		flConsts[3] = -flConsts[1] + ( ( float )nViewportY / ( float )nRtHeight );
		DynamicCmdsOut.SetPixelShaderConstant( DEPTH_FEATHER_VIEWPORT_MAD, flConsts, 1 );

		if( bHasFlowmap )
		{
			float vFlowConst1[4] =  { 0.0f, 0.0f, 0.0f, 0.0f };
			params[info.m_nFlowVortexPos1]->GetVecValue( vFlowConst1, 3 );
			vFlowConst1[3] = params[ info.m_nFlowNoiseScale ]->GetFloatValue();
			DynamicCmdsOut.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_9, vFlowConst1, 1 );

			float vFlowConst2[4] =  { 0.0f, 0.0f, 0.0f, 0.0f };
			params[info.m_nFlowVortexPos2]->GetVecValue( vFlowConst2, 3 );
			vFlowConst2[3] = params[ info.m_nFlowNormalUVScale ]->GetFloatValue();
			DynamicCmdsOut.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_10, vFlowConst2, 1 );
		}

		// PIXEL SHADER SETUP

#if !defined( _GAMECONSOLE )
		if ( g_pHardwareConfig->GetDXSupportLevel() < 95 )
#endif
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( solidenergy_ps20b );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( ACTIVE, bActive );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( POWERUP, bPowerup );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( VORTEX1, bVortex1 );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( VORTEX2, bVortex2 );
			SET_DYNAMIC_PIXEL_SHADER( solidenergy_ps20b );
		}
#if !defined( _GAMECONSOLE )
		else
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( solidenergy_ps30 );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( ACTIVE, bActive );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( POWERUP, bPowerup );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( VORTEX1, bVortex1 );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( VORTEX2, bVortex2 );
			SET_DYNAMIC_PIXEL_SHADER( solidenergy_ps30 );
		}
#endif

		if ( bDetail1 )
		{
			DynamicCmdsOut.BindTexture( pShader, SHADER_SAMPLER1, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nDetail1Texture, info.m_nDetail1Frame );
		}

		if ( bDetail2 )
		{
			DynamicCmdsOut.BindTexture( pShader, SHADER_SAMPLER4, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nDetail2Texture, info.m_nDetail2Frame );
		}

		if ( bDepthBlend )
		{
			DynamicCmdsOut.BindStandardTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, TEXTURE_FRAME_BUFFER_FULL_DEPTH );
		}

		flConsts[0] = bWriteDepthToAlpha ? 1.0f : 0.0f;
		flConsts[1] = pShaderAPI->CurrentTime();
		flConsts[2] = params[ info.m_nPowerUp ]->GetFloatValue();
		flConsts[3] =  params[info.m_nFlowColorIntensity]->GetFloatValue();
		DynamicCmdsOut.SetPixelShaderConstant( 3, flConsts, 1 );

		DynamicCmdsOut.End();

		// end dynamic block
#ifdef _PS3
		pShaderAPI->SetDepthFeatheringShaderConstants( 4, params[info.m_nDepthBlendScale]->GetFloatValue() );
#endif
		pShaderAPI->ExecuteCommandBuffer( DynamicCmdsOut.Base() );

		//no dynamic combos
	}
	pShader->Draw();
}
