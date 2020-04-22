//========= Copyright (c) Valve Corporation, All rights reserved. ============//

#include "BaseVSShader.h"
#include "customweapon_dx9_helper.h"
#include "cpp_shader_constant_register_map.h"

// Auto generated inc files
#include "customweapon_vs20.inc"
#include "customweapon_vs30.inc"
#include "customweapon_ps20b.inc"
#include "customweapon_ps30.inc"
#include "common_hlsl_cpp_consts.h"

#include "shaderlib/commandbuilder.h"

void InitParamsCustomWeapon( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, CustomWeaponVars_t &info )
{
	static ConVarRef gpu_level( "gpu_level" );
	int nGPULevel = gpu_level.GetInt();

	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nCamoColor0, kDefaultCamoColor0, 3 );
	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nCamoColor1, kDefaultCamoColor1, 3 );
	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nCamoColor2, kDefaultCamoColor2, 3 );
	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nCamoColor3, kDefaultCamoColor3, 3 );

	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nBaseDiffuseOverride, kDefaultBaseDiffuseOverride );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nPhongExponent, kDefaultPhongExponent );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nPhongIntensity, kDefaultPhongIntensity );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nWearProgress, kDefaultWearProgress );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nPaintStyle, kDefaultPaintStyle );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nExponentMode, kDefaultExponentMode );

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nPhongAlbedoFactor, kDefaultPhongAlbedoFactor );

	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nCheap, nGPULevel < 2 );

	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nPreview, 0 );
	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nPreviewPhongFresnelRanges, kDefaultPreviewPhongFresnelRanges, 3 );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nPreviewPhongAlbedoTint, 0 );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nPhongAlbedoBoost, -1 );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nPreviewPhongBoost, kDefaultPreviewPhongBoost );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nPreviewPaintPhongAlbedoBoost, kDefaultPreviewPhongBoost );

	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nPreviewIgnoreWeaponScale, 0 );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nPreviewWeaponObjScale, kDefaultWeaponScale );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nPreviewWeaponUVScale, kDefaultWeaponScale );

	if ( ( g_pHardwareConfig->SupportsPixelShaders_3_0() ) && ( info.m_nPreview != -1 ) && ( params[info.m_nPreview]->GetIntValue() == 1 ) )
	{
		SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
		SET_FLAGS2( MATERIAL_VAR2_LIGHTING_VERTEX_LIT );
	}
}

void InitCustomWeapon( CBaseVSShader *pShader, IMaterialVar** params, CustomWeaponVars_t &info )
{
	// Load textures
	if ( ( info.m_nBaseTexture != -1 ) && params[info.m_nBaseTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nBaseTexture );
	}
	if ( ( info.m_nSurfaceTexture != -1 ) && params[info.m_nSurfaceTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nSurfaceTexture );
	}
	if ( ( info.m_nAOTexture != -1 ) && params[info.m_nAOTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nAOTexture );
	}
	if ( ( info.m_nMasksTexture != -1 ) && params[info.m_nMasksTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nMasksTexture );
	}
	if ( ( info.m_nPosTexture != -1 ) && params[info.m_nPosTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nPosTexture );
	}
	if ( ( info.m_nGrungeTexture != -1 ) && params[info.m_nGrungeTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nGrungeTexture );
	}
	if ( ( info.m_nWearTexture != -1 ) && params[info.m_nWearTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nWearTexture );
	}
	if ( ( info.m_nPaintTexture != -1 ) && params[info.m_nPaintTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nPaintTexture );
	}
	if ( ( info.m_nExpTexture != -1 ) && params[info.m_nExpTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nExpTexture );
	}
}

class CCustomWeapon_DX9_Context : public CBasePerMaterialContextData
{
public:
	CCommandBufferBuilder< CFixedCommandStorageBuffer< 800 > > m_SemiStaticCmdsOut;
};

void DrawCustomWeapon(  CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
	IShaderShadow* pShaderShadow, CustomWeaponVars_t &info, VertexCompressionType_t vertexCompression,
	CBasePerMaterialContextData **pContextDataPtr )
{
	CCustomWeapon_DX9_Context *pContextData = reinterpret_cast< CCustomWeapon_DX9_Context *> ( *pContextDataPtr );

	bool bSurfaceTexture = ( info.m_nSurfaceTexture != -1 ) && params[info.m_nSurfaceTexture]->IsTexture();
	bool bAOTexture = ( info.m_nAOTexture != -1 ) && params[info.m_nAOTexture]->IsTexture();
	bool bMasksTexture = ( info.m_nMasksTexture != -1 ) && params[info.m_nMasksTexture]->IsTexture();
	bool bPosTexture = ( info.m_nPosTexture != -1 ) && params[info.m_nPosTexture]->IsTexture();
	bool bGrungeTexture = ( info.m_nGrungeTexture != -1 ) && params[info.m_nGrungeTexture]->IsTexture();
	bool bWearTexture = ( info.m_nWearTexture != -1 ) && params[info.m_nWearTexture]->IsTexture();
	bool bPaintTexture = ( info.m_nPaintTexture != -1 ) && params[info.m_nPaintTexture]->IsTexture();
	bool bExpTexture = ( info.m_nExpTexture != -1 ) && params[info.m_nExpTexture]->IsTexture();
	bool bBaseTexture = ( info.m_nBaseTexture != -1 ) && params[info.m_nBaseTexture]->IsTexture();
	bool bPreview = ( g_pHardwareConfig->SupportsPixelShaders_3_0() ) && ( info.m_nPreview != -1 ) && ( params[info.m_nPreview]->GetIntValue() == 1 );
	bool bPreviewPhongAlbedoTint = bPreview && ( info.m_nPreviewPhongAlbedoTint != -1 ) && ( params[info.m_nPreviewPhongAlbedoTint]->GetIntValue() == 1 );
	bExpTexture = bExpTexture || bPreview;

	float fPhongAlbedoFactor;
	if ( bPreview )
	{
		float fOrigPhongAlbedoBoost, fOrigPhongBoost, fPaintPhongAlbedoBoost;
		fOrigPhongAlbedoBoost = params[info.m_nPhongAlbedoBoost]->GetFloatValue();
		fOrigPhongBoost = params[info.m_nPreviewPhongBoost]->GetFloatValue();
		fPaintPhongAlbedoBoost = params[info.m_nPreviewPaintPhongAlbedoBoost]->GetFloatValue();

		fPhongAlbedoFactor = ( fOrigPhongAlbedoBoost < 0 ) ? ( ( fOrigPhongBoost < 0 ) ? 1.0f : fOrigPhongBoost ) : fOrigPhongAlbedoBoost;
		fPhongAlbedoFactor /= ( fPaintPhongAlbedoBoost <= 0 ) ? 1.0f : fPaintPhongAlbedoBoost;
	}
	else
	{
		fPhongAlbedoFactor = params[info.m_nPhongAlbedoFactor]->GetFloatValue();
	}

	int nPaintStyle = 0;
	if ( info.m_nPaintStyle != -1 )
		nPaintStyle = params[info.m_nPaintStyle]->GetIntValue();

	bool bExpMode = ( info.m_nExponentMode != -1 )  && ( params[info.m_nExponentMode]->GetIntValue() == 1);

	bool bCheapMode = ( params[info.m_nCheap]->GetIntValue() != 0 ) && ( ( nPaintStyle == 3 ) || ( nPaintStyle == 6 ) ) && ( !bExpMode );

	if ( ( pShader->IsSnapshotting() && pShaderShadow ) || (! pContextData ) || ( pContextData->m_bMaterialVarsChanged ) )
	{

		if ( pShader->IsSnapshotting() )
		{
			// Set stream format (note that this shader supports compression)
			int userDataSize = 0;
			unsigned int flags = VERTEX_POSITION | VERTEX_FORMAT_COMPRESSED;

			int nTexCoordCount = 1;
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );

			// Vertex Shader
			if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_STATIC_VERTEX_SHADER( customweapon_vs30 );
				SET_STATIC_VERTEX_SHADER_COMBO( PREVIEW, bPreview );
				SET_STATIC_VERTEX_SHADER( customweapon_vs30 );
			}
			else
			{
				DECLARE_STATIC_VERTEX_SHADER( customweapon_vs20 );
				SET_STATIC_VERTEX_SHADER_COMBO( PREVIEW, 0 ); // can't use the preview shader in ps20b
				SET_STATIC_VERTEX_SHADER( customweapon_vs20 );
			}

			// Pixel Shader
			if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_STATIC_PIXEL_SHADER( customweapon_ps30 );
				SET_STATIC_PIXEL_SHADER_COMBO( PAINTSTYLE, nPaintStyle );
				SET_STATIC_PIXEL_SHADER_COMBO( EXPONENTMODE, bExpMode );
				SET_STATIC_PIXEL_SHADER_COMBO( CHEAPMODE, bCheapMode );
				SET_STATIC_PIXEL_SHADER_COMBO( PREVIEW, bPreview );
				SET_STATIC_PIXEL_SHADER_COMBO( PREVIEWPHONGALBEDOTINT, bPreview && bPreviewPhongAlbedoTint );
				SET_STATIC_PIXEL_SHADER_COMBO( PHONGALBEDOFACTORMODE, ( fPhongAlbedoFactor < 1.0f ) );
				SET_STATIC_PIXEL_SHADER( customweapon_ps30 );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( customweapon_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( PAINTSTYLE, nPaintStyle );
				SET_STATIC_PIXEL_SHADER_COMBO( EXPONENTMODE, bExpMode );
				SET_STATIC_PIXEL_SHADER_COMBO( CHEAPMODE, bCheapMode );
				SET_STATIC_PIXEL_SHADER_COMBO( PHONGALBEDOFACTORMODE, ( fPhongAlbedoFactor < 1.0f ) );
				SET_STATIC_PIXEL_SHADER( customweapon_ps20b );
			}

			// Textures
			if( bAOTexture )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );		// [sRGB] Ambient Occlusion
			}
			if( bWearTexture )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );		// Scratches
			}
			if( bExpTexture )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );		// Exponent
			}
			if ( bBaseTexture )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );		// [sRGB] Base
			}
			if( bMasksTexture )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );		// Masks
			}
			if( bGrungeTexture )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER5, true );		// [sRGB] Grunge
			}
			if( bSurfaceTexture )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER6, true );		// Obj-space normal and cavity
			}
			if( bPosTexture )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER7, true );		// High-precision Position
			}
			if( bPaintTexture )
			{	
				pShaderShadow->EnableTexture( SHADER_SAMPLER8, true );		// Paint
			}	

			pShaderShadow->EnableAlphaWrites( true );
			bool bSRGBWrite = ( bPreview || !bExpMode );
			pShaderShadow->EnableSRGBWrite( bSRGBWrite );

			if ( bPreview )
			{
				pShader->PI_BeginCommandBuffer();
				pShader->PI_SetPixelShaderAmbientLightCube( PSREG_AMBIENT_CUBE );
				pShader->PI_SetPixelShaderLocalLighting( PSREG_LIGHT_INFO_ARRAY );
				pShader->PI_EndCommandBuffer();
			}
			
		}
		if ( pShaderAPI && ( (! pContextData ) || ( pContextData->m_bMaterialVarsChanged ) ) )
		{
			if ( !pContextData )								// make sure allocated
			{
				pContextData = new CCustomWeapon_DX9_Context;
				*pContextDataPtr = pContextData;
			}
			pContextData->m_bMaterialVarsChanged = false;
			pContextData->m_SemiStaticCmdsOut.Reset();

			///////////////////////////
			// Semi-static block
			///////////////////////////

			// VS Constants

			if ( IS_PARAM_DEFINED( info.m_nPatternTextureTransform ) )
			{
				if ( params[info.m_nPreviewIgnoreWeaponScale]->GetIntValue() == 1 )
					pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, info.m_nPatternTextureTransform );
				else
					pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureScaledTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, info.m_nPatternTextureTransform, info.m_nPreviewWeaponUVScale );
			}
			else
			{
				if ( params[info.m_nPreviewIgnoreWeaponScale]->GetIntValue() == 1 )
					pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, info.m_nPatternTextureTransform );
				else
					pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureScaledTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, info.m_nBaseTextureTransform, info.m_nPreviewWeaponUVScale );
			}

			if ( IS_PARAM_DEFINED( info.m_nWearTextureTransform ) )
			{
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_2, info.m_nWearTextureTransform );
			}
			else
			{
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_2, info.m_nBaseTextureTransform );
			}

			if ( IS_PARAM_DEFINED( info.m_nGrungeTextureTransform ) )
			{
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_4, info.m_nGrungeTextureTransform );
			}
			else
			{
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_4, info.m_nBaseTextureTransform );
			}

			// PS Constants

			// AO
			if( bAOTexture ) 
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nAOTexture, -1 );
			}

			// Scratches
			if( bWearTexture )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, info.m_nWearTexture, -1 );
			}

			// Exponent
			if( bExpTexture )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER2, TEXTURE_BINDFLAGS_NONE, info.m_nExpTexture, -1 );
			}

			// Base
			if ( bBaseTexture )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER3, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nBaseTexture, -1 );
			}

			// Masks
			if( bMasksTexture )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER4, TEXTURE_BINDFLAGS_NONE, info.m_nMasksTexture, -1 );
			}

			//Grunge
			if( bGrungeTexture )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER5, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nGrungeTexture, -1 );
			}

			// Object Space Normals
			if( bSurfaceTexture )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER6, TEXTURE_BINDFLAGS_NONE, info.m_nSurfaceTexture, -1 );
			}

			// Object Space Position
			if( bPosTexture )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER7, TEXTURE_BINDFLAGS_NONE, info.m_nPosTexture, -1 );
			}

			// Paint pattern
			if( bPaintTexture )
			{
				if ( ( nPaintStyle == 7 ) || ( nPaintStyle == 8 ) || ( nPaintStyle == 9 ) )
					pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER8, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nPaintTexture, -1 );
				else
					pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER8, TEXTURE_BINDFLAGS_NONE, info.m_nPaintTexture, -1 );
			}

			if ( bPreview )
			{
				pContextData->m_SemiStaticCmdsOut.BindStandardTexture( SHADER_SAMPLER9, TEXTURE_BINDFLAGS_NONE, TEXTURE_NORMALIZATION_CUBEMAP_SIGNED );
			}

			float fvCamoConst[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

			float fvCamo3[3] = { 0.0f, 0.0f, 0.0f };
			params[info.m_nCamoColor3]->GetVecValue( fvCamo3, 3 );
			if ( bPreview )
			{
				fvCamo3[0] /= 255.0;
				fvCamo3[1] /= 255.0;
				fvCamo3[2] /= 255.0;
			}
			fvCamo3[0] = SrgbGammaToLinear( fvCamo3[0] );
			fvCamo3[1] = SrgbGammaToLinear( fvCamo3[1] );
			fvCamo3[2] = SrgbGammaToLinear( fvCamo3[2] );

			params[info.m_nCamoColor0]->GetVecValue( fvCamoConst, 3 );
			if ( bPreview )
			{
				fvCamoConst[0] /= 255.0;
				fvCamoConst[1] /= 255.0;
				fvCamoConst[2] /= 255.0;
			}
			fvCamoConst[0] = SrgbGammaToLinear( fvCamoConst[0] );
			fvCamoConst[1] = SrgbGammaToLinear( fvCamoConst[1] );
			fvCamoConst[2] = SrgbGammaToLinear( fvCamoConst[2] );
			fvCamoConst[3] = fvCamo3[0];
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 0, fvCamoConst, 1 );

			params[info.m_nCamoColor1]->GetVecValue( fvCamoConst, 3 );
			if ( bPreview )
			{
				fvCamoConst[0] /= 255.0;
				fvCamoConst[1] /= 255.0;
				fvCamoConst[2] /= 255.0;
			}
			fvCamoConst[0] = SrgbGammaToLinear( fvCamoConst[0] );
			fvCamoConst[1] = SrgbGammaToLinear( fvCamoConst[1] );
			fvCamoConst[2] = SrgbGammaToLinear( fvCamoConst[2] );
			fvCamoConst[3] = fvCamo3[1];
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 1, fvCamoConst, 1 );

			params[info.m_nCamoColor2]->GetVecValue( fvCamoConst, 3 );
			if ( bPreview )
			{
				fvCamoConst[0] /= 255.0;
				fvCamoConst[1] /= 255.0;
				fvCamoConst[2] /= 255.0;
			}
			fvCamoConst[0] = SrgbGammaToLinear( fvCamoConst[0] );
			fvCamoConst[1] = SrgbGammaToLinear( fvCamoConst[1] );
			fvCamoConst[2] = SrgbGammaToLinear( fvCamoConst[2] );
			fvCamoConst[3] = fvCamo3[2];
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 2, fvCamoConst, 1 );

			float fvPhongFactors[4] = { kDefaultPhongAlbedoFactor, kDefaultPhongExponent, kDefaultPhongIntensity, 0 };
			fvPhongFactors[0] = ( fPhongAlbedoFactor > 1 ) ? 1.0/fPhongAlbedoFactor : fPhongAlbedoFactor;
			fvPhongFactors[1] = params[info.m_nPhongExponent]->GetFloatValue();	
			if ( bPreview )
				fvPhongFactors[1] /= 255.0;
			fvPhongFactors[2] = params[info.m_nPhongIntensity]->GetFloatValue();
			if ( bPreview )
			{
				float fPhongBoost = params[info.m_nPreviewPhongBoost]->GetFloatValue();
				if ( ( nPaintStyle == 1 ) || ( nPaintStyle == 2 ) || ( nPaintStyle == 3 ) || ( nPaintStyle == 7 ) || ( nPaintStyle == 8 ) )
				{
					fvPhongFactors[2] = ( fPhongBoost < 0 ) ? fvPhongFactors[2] : fvPhongFactors[2] / fPhongBoost;
				}
			}
			fvPhongFactors[3] = params[info.m_nWearProgress]->GetFloatValue();
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 3, fvPhongFactors, 1 );

			if ( ( nPaintStyle == 3 ) || ( nPaintStyle == 6 ) )
			{
				if ( IS_PARAM_DEFINED( info.m_nPatternTextureTransform ) )
				{
					pContextData->m_SemiStaticCmdsOut.SetPixelShaderTextureTransform( 10, info.m_nPatternTextureTransform );
				}
				else
				{
					pContextData->m_SemiStaticCmdsOut.SetPixelShaderTextureTransform( 10, info.m_nBaseTextureTransform );
				}
			}

			if ( bPreview )
			{
				float fvPhongConst[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
				params[info.m_nPreviewPhongFresnelRanges]->GetVecValue( fvPhongConst, 3 );
				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 13, fvPhongConst, 1 );

				float fOrigPhongAlbedoBoost, fOrigPhongBoost, fPaintPhongAlbedoBoost;
				fOrigPhongAlbedoBoost = params[info.m_nPhongAlbedoBoost]->GetFloatValue();
				fOrigPhongBoost = params[info.m_nPreviewPhongBoost]->GetFloatValue();
				fPaintPhongAlbedoBoost = params[info.m_nPreviewPaintPhongAlbedoBoost]->GetFloatValue();

				float fPhongAlbedoBoost = ( fOrigPhongAlbedoBoost < 0 ) ? ( ( fOrigPhongBoost < 0 ) ? 1.0f : fOrigPhongBoost ) : fOrigPhongAlbedoBoost;
				fPhongAlbedoBoost = ( fPhongAlbedoBoost > fPaintPhongAlbedoBoost ) ? fPhongAlbedoBoost : fPaintPhongAlbedoBoost;

				float fvPreviewPhongBoosts[4] = { 1.0f, 1.0f, 1.0f, 1.0f };;
				fvPreviewPhongBoosts[0] = fPhongAlbedoBoost;
				fvPreviewPhongBoosts[1] = ( fOrigPhongBoost > 0 ) ? fOrigPhongBoost : 1.0f;
				float flWeaponObjScale = ( params[info.m_nPreviewIgnoreWeaponScale]->GetIntValue() == 1 ) ? 1.0f : params[info.m_nPreviewWeaponObjScale]->GetFloatValue();
				fvPreviewPhongBoosts[2] =  flWeaponObjScale;

				pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 14, fvPreviewPhongBoosts, 1 );
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
		int numBones = ShaderApiFast( pShaderAPI )->GetCurrentNumBones();

		LightState_t lightState = { 0, false, false };
		ShaderApiFast( pShaderAPI )->GetDX9LightState( &lightState );

		// VERTEX SHADER SETUP
		if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
		{
			DECLARE_DYNAMIC_VERTEX_SHADER( customweapon_vs30 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, ( numBones > 0 ) );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( NUM_LIGHTS, bPreview ? lightState.m_nNumLights : 0 );
			SET_DYNAMIC_VERTEX_SHADER( customweapon_vs30 );
		}
		else
		{
			DECLARE_DYNAMIC_VERTEX_SHADER( customweapon_vs20 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, ( numBones > 0 ) );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( NUM_LIGHTS, bPreview ? lightState.m_nNumLights : 0 );
			SET_DYNAMIC_VERTEX_SHADER( customweapon_vs20 );
		}

		// VS constants
		

		// PIXEL SHADER SETUP

		if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( customweapon_ps30 );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( NUM_LIGHTS, bPreview ? lightState.m_nNumLights : 0 );
			SET_DYNAMIC_PIXEL_SHADER( customweapon_ps30 );
		}
		else
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( customweapon_ps20b );
			SET_DYNAMIC_PIXEL_SHADER( customweapon_ps20b );
		}

		if ( bPreview )
		{
			float vEyePos[4] = { 0, 0, 0, 0 };
			ShaderApiFast( pShaderAPI )->GetWorldSpaceCameraPosition( vEyePos );
			DynamicCmdsOut.SetPixelShaderConstant( 12, vEyePos, 1 );
		}

		DynamicCmdsOut.End();

		// end dynamic block

		pShaderAPI->ExecuteCommandBuffer( DynamicCmdsOut.Base() );

	}
	pShader->Draw();
}
