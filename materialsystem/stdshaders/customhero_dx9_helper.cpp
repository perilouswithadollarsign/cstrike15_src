//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//

#include "BaseVSShader.h"
#include "customhero_dx9_helper.h"
#include "cpp_shader_constant_register_map.h"
/*
#include "mathlib/VMatrix.h"
#include "convar.h"
*/

// Auto generated inc files
#include "customhero_vs20.inc"
#include "customhero_ps20.inc"
#include "customhero_ps20b.inc"
#include "customhero_vs30.inc"
#include "customhero_ps30.inc"

#include "shaderlib/commandbuilder.h"

void InitParamsCustomHero( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, CustomHeroVars_t &info )
{
	// Set material parameter default values
	SET_PARAM_STRING_IF_NOT_DEFINED( info.m_nFresnelWarp, kDefaultFresnelWarp );

	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nFlashlightTextureFrame, 0 );

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nDetail1Scale, kDefaultDetailScale );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nDetail1Frame, kDefaultDetailFrame );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nDetail1BlendMode, kDefaultDetailBlendMode );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nDetail1BlendFactor, kDefaultDetail1BlendFactor );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nDetail1BlendToFull, kDefaultBlendToFull );

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nDetail2Scale, kDefaultDetailScale );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nDetail2Frame, kDefaultDetailFrame );
	SET_PARAM_INT_IF_NOT_DEFINED( info.m_nDetail2BlendMode, kDefaultDetailBlendMode );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nDetail2BlendFactor, kDefaultDetail2BlendFactor );

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nDiffuseWarpBlendToFull, kDefaultBlendToFull );

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nColorWarpBlendFactor, kDefaultColorWarpBlendFactor );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nFresnelColorWarpBlendToFull, kDefaultBlendToFull );

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nSpecularExponent, kDefaultExponent );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nSpecularExponentBlendToFull, kDefaultBlendToFull );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nSpecularBlendToFull, kDefaultBlendToFull );
	SET_PARAM_VEC_IF_NOT_DEFINED( info.m_nSpecularColor, kDefaultSpecularColor, 3 );
	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nSpecularScale, kDefaultIntensity );

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nEnvMapBlendToFull, kDefaultBlendToFull );

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nEnvMapBlendToFull, kDefaultBlendToFull );

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nMetallnessBlendToFull, kDefaultBlendToFull );

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nAmbientScale, kDefaultAmbientScale );

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nRimLightScale, kDefaultIntensity );

	SET_PARAM_FLOAT_IF_NOT_DEFINED( info.m_nSelfIllumBlendToFull, kDefaultBlendToFull );
	SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
	SET_FLAGS2( MATERIAL_VAR2_LIGHTING_VERTEX_LIT );
}

void InitCustomHero( CBaseVSShader *pShader, IMaterialVar** params, CustomHeroVars_t &info )
{
	// Load textures
	if ( ( info.m_nBaseTexture != -1 ) && params[info.m_nBaseTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nBaseTexture );
	}
	if ( ( info.m_nNormalMap != -1 ) && params[info.m_nNormalMap]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nNormalMap );
	}
	if ( ( info.m_nMaskMap1 != -1 ) && params[info.m_nMaskMap1]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nMaskMap1 );
	}
	if ( ( info.m_nMaskMap2 != -1 ) && params[info.m_nMaskMap2]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nMaskMap2 );
	}
	if ( ( info.m_nDiffuseWarp != -1 ) && params[info.m_nDiffuseWarp]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nDiffuseWarp );
	}
	if ( ( info.m_nFresnelColorWarp != -1 ) && params[info.m_nFresnelColorWarp]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nFresnelColorWarp );
	}
	if ( ( info.m_nColorWarp != -1 ) && params[info.m_nColorWarp]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nColorWarp );
	}
	if ( ( info.m_nDetail1 != -1 ) && params[info.m_nDetail1]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nDetail1 );
	}
	if ( ( info.m_nDetail2 != -1 ) && params[info.m_nDetail2]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nDetail2 );
	}
	if ( ( info.m_nEnvMap != -1 ) && params[info.m_nEnvMap]->IsDefined() )
	{
		pShader->LoadCubeMap( info.m_nEnvMap );
	}
	if ( ( info.m_nFlashlightTexture != -1 ) && params[info.m_nFlashlightTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nFlashlightTexture );
	}
	if ( ( info.m_nFresnelWarp != -1 ) && params[info.m_nFresnelWarp]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nFresnelWarp );
	}
}

class CCustomHero_DX9_Context : public CBasePerMaterialContextData
{
public:
	CCommandBufferBuilder< CFixedCommandStorageBuffer< 800 > > m_SemiStaticCmdsOut;
};

void DrawCustomHero(  CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
					  IShaderShadow* pShaderShadow, CustomHeroVars_t &info, VertexCompressionType_t vertexCompression,
					  CBasePerMaterialContextData **pContextDataPtr )
{
	CCustomHero_DX9_Context *pContextData = reinterpret_cast< CCustomHero_DX9_Context *> ( *pContextDataPtr );

	bool bHasFlashlight = pShader->UsingFlashlight( params );
	bool bAlphaBlend = IS_FLAG_SET( MATERIAL_VAR_TRANSLUCENT );
	bool bDetail1 = ( info.m_nDetail1 != -1 )&& params[info.m_nDetail1]->IsTexture() && ( info.m_nDetail1BlendMode != -1 )&& ( params[info.m_nDetail1BlendMode]->GetIntValue() > 0 );

	if ( pShader->IsSnapshotting() || (! pContextData ) || ( pContextData->m_bMaterialVarsChanged ) )
	{
		bool bMaskMap1 = ( info.m_nMaskMap1 != -1 ) && params[info.m_nMaskMap1]->IsTexture();
		bool bMaskMap2 = ( info.m_nMaskMap2 != -1 ) && params[info.m_nMaskMap2]->IsTexture();
		bool bDiffuseWarp = ( info.m_nDiffuseWarp != -1 )&& params[info.m_nDiffuseWarp]->IsTexture();
		bool bColorWarp = ( info.m_nColorWarp != -1 )&& params[info.m_nColorWarp]->IsTexture();
		bool bFresnelColorWarp = ( info.m_nFresnelColorWarp != -1 )&& params[info.m_nFresnelColorWarp]->IsTexture();
		bool bEnvMap = ( info.m_nEnvMap != -1 )&& params[info.m_nEnvMap]->IsTexture();
		int nDetail1BlendMode = ( info.m_nDetail1BlendMode != -1 )? params[info.m_nDetail1BlendMode]->GetIntValue() : kDefaultDetailBlendMode;
		nDetail1BlendMode = bDetail1 ? clamp( nDetail1BlendMode, 0, kMaxDetailBlendMode ): 0;

		if ( pShader->IsSnapshotting() )
		{
			// Set stream format (note that this shader supports compression)
			unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED;
			int nTexCoordCount = 1;
			int userDataSize = 4;
			int texCoordDims[4] = { 2, 2, 2, 2 };
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, texCoordDims, userDataSize );

			int nShadowFilterMode = 0;
			if ( bHasFlashlight )
			{
				nShadowFilterMode = g_pHardwareConfig->GetShadowFilterMode(); // Based upon vendor and device dependent formats
			}

			// Vertex Shader
			if ( g_pHardwareConfig->GetDXSupportLevel() >= 95 )
			{
				//looks the same but we'll be enabling vs30-only effects in the shader
				// Is it necessary to do this?
				DECLARE_STATIC_VERTEX_SHADER( customhero_vs30 );
				SET_STATIC_VERTEX_SHADER_COMBO( DETAIL1, bDetail1 );
				SET_STATIC_VERTEX_SHADER( customhero_vs30 );
			}
			else
			{
				DECLARE_STATIC_VERTEX_SHADER( customhero_vs20 );
				SET_STATIC_VERTEX_SHADER_COMBO( DETAIL1, bDetail1 );
				SET_STATIC_VERTEX_SHADER( customhero_vs20 );
			}
		
			// Pixel Shader
			if ( g_pHardwareConfig->GetDXSupportLevel() >= 95 )
			{
				DECLARE_STATIC_PIXEL_SHADER( customhero_ps30 );
				SET_STATIC_PIXEL_SHADER_COMBO( ALPHABLEND, bAlphaBlend );
				//TODO: enable flashlight
				//SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode );
				SET_STATIC_PIXEL_SHADER_COMBO( MASKMAP1, bMaskMap1 );
				SET_STATIC_PIXEL_SHADER_COMBO( MASKMAP2, bMaskMap2 );
				SET_STATIC_PIXEL_SHADER_COMBO( DETAIL1, bDetail1 ); // no combo for detail2 because it should always be available for status effects
				SET_STATIC_PIXEL_SHADER_COMBO( DETAIL1BLENDMODE, nDetail1BlendMode );
				SET_STATIC_PIXEL_SHADER_COMBO( DIFFUSEWARP, bDiffuseWarp );
				SET_STATIC_PIXEL_SHADER_COMBO( COLORWARP, bColorWarp );
				SET_STATIC_PIXEL_SHADER_COMBO( FRESNELCOLORWARP, bFresnelColorWarp );
				SET_STATIC_PIXEL_SHADER_COMBO( ENVMAP, bEnvMap );
				SET_STATIC_PIXEL_SHADER( customhero_ps30 );
			}
			else if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( customhero_ps20b );
				// no combo for detail2 because it should always be available for status effects
				SET_STATIC_PIXEL_SHADER_COMBO( ALPHABLEND, bAlphaBlend );
				SET_STATIC_PIXEL_SHADER_COMBO( MASKMAP1, bMaskMap1 );
				SET_STATIC_PIXEL_SHADER_COMBO( MASKMAP2, bMaskMap2 );
				SET_STATIC_PIXEL_SHADER_COMBO( DETAIL1, bDetail1 );
				SET_STATIC_PIXEL_SHADER_COMBO( DETAIL1BLENDMODE, nDetail1BlendMode );
				SET_STATIC_PIXEL_SHADER_COMBO( DIFFUSEWARP, bDiffuseWarp );
				SET_STATIC_PIXEL_SHADER_COMBO( COLORWARP, bColorWarp );
				SET_STATIC_PIXEL_SHADER_COMBO( FRESNELCOLORWARP, bFresnelColorWarp );
				SET_STATIC_PIXEL_SHADER_COMBO( ENVMAP, bEnvMap );
				SET_STATIC_PIXEL_SHADER( customhero_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( customhero_ps20 );
				// no combo for detail2 because it should always be available for status effects
				SET_STATIC_PIXEL_SHADER_COMBO( ALPHABLEND, bAlphaBlend );
				SET_STATIC_PIXEL_SHADER_COMBO( MASKMAP1, bMaskMap1 );
				SET_STATIC_PIXEL_SHADER_COMBO( MASKMAP2, bMaskMap2 );
				SET_STATIC_PIXEL_SHADER_COMBO( DETAIL1, bDetail1 );
				SET_STATIC_PIXEL_SHADER_COMBO( DETAIL1BLENDMODE, nDetail1BlendMode );
				SET_STATIC_PIXEL_SHADER_COMBO( DIFFUSEWARP, bDiffuseWarp );
				SET_STATIC_PIXEL_SHADER_COMBO( COLORWARP, bColorWarp );
				SET_STATIC_PIXEL_SHADER_COMBO( ENVMAP, bEnvMap );
				SET_STATIC_PIXEL_SHADER( customhero_ps20 );
			}

			// Textures
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );		// [sRGB] Base
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );		// Normal
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, false );
			pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );		// [sRGB] Detail2 for status effects
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER2, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER14, true );		// Fresnel warp
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER14, false );

			if( bMaskMap1 )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );	// Mask set 1
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER3, false );
			}

			if( bMaskMap2 )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );	// Mask set 2
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER4, false );
			}

			if( bDetail1 )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER5, true );	// [sRGB] Detail1
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER5, true );
			}

			if( bDiffuseWarp )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER6, true );	// Diffuse Warp
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER6, false );
			}

			if( bFresnelColorWarp )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER8, true );	// [sRGB] Fresnel Color Warp
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER8, true );
			}

			if( bColorWarp )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER9, true );	// [sRGB] Base Color Warp
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER9, true );
			}

			if( bEnvMap )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER10, true );	// [sRGB] Env map
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER10, true );
			}


			if ( bHasFlashlight )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER11, true );	//		 Shadow depth map
				pShaderShadow->SetShadowDepthFiltering( SHADER_SAMPLER11 );
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER11, false );
				pShaderShadow->EnableTexture( SHADER_SAMPLER12, true );	//		 Noise map
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER12, false );
				pShaderShadow->EnableTexture( SHADER_SAMPLER13, true );	//[sRGB] Flashlight cookie
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER13, true );

				// Flashlight passes - additive blending
				pShader->EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE );
				pShaderShadow->EnableAlphaWrites( false );
				pShaderShadow->EnableDepthWrites( false );
			}
			if ( bAlphaBlend )
			{
				// Base pass - alpha blending (regular translucency)
				pShader->EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				pShaderShadow->EnableAlphaWrites( false );	
				pShaderShadow->EnableDepthWrites( true );
			}
			else
			{
				// Base pass - opaque blending
				pShader->DisableAlphaBlending();
				pShaderShadow->EnableAlphaWrites( true );
				pShaderShadow->EnableDepthWrites( true );
			}

			pShaderShadow->EnableSRGBWrite( true );

			// Per-instance state
			pShader->PI_BeginCommandBuffer();
			pShader->PI_SetVertexShaderAmbientLightCube();
			pShader->PI_SetPixelShaderAmbientLightCube( 4 ); // c4-c9
			pShader->PI_SetPixelShaderLocalLighting( PSREG_LIGHT_INFO_ARRAY );  // c20-c25
			pShader->PI_EndCommandBuffer();
		}
		if ( pShaderAPI && ( (! pContextData ) || ( pContextData->m_bMaterialVarsChanged ) ) )
		{
			if ( !pContextData )								// make sure allocated
			{
				pContextData = new CCustomHero_DX9_Context;
				*pContextDataPtr = pContextData;
			}
			pContextData->m_bMaterialVarsChanged = false;
			pContextData->m_SemiStaticCmdsOut.Reset();
			///////////////////////////
			// Semi-static block
			///////////////////////////

			float flConsts[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

			// VS consts
			flConsts[0] = IS_PARAM_DEFINED( info.m_nDetail1Scale ) ? params[info.m_nDetail1Scale]->GetFloatValue() : kDefaultDetailScale;
			flConsts[1] = IS_PARAM_DEFINED( info.m_nDetail2Scale ) ? params[info.m_nDetail2Scale]->GetFloatValue() : kDefaultDetailScale;
			flConsts[2] = IS_PARAM_DEFINED( info.m_nAmbientScale ) ? params[info.m_nAmbientScale]->GetFloatValue() : kDefaultAmbientScale;
			flConsts[3] = 0; // Empty
			pContextData->m_SemiStaticCmdsOut.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, flConsts, 1 );

			if ( info.m_nBaseTextureTransform != -1 )
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_1, info.m_nBaseTextureTransform ); // 1-2

			if ( IS_PARAM_DEFINED( info.m_nBaseTextureTransform ) )
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureScaledTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3, info.m_nDetail1TextureTransform, info.m_nDetail1Scale ); // 3-4
			else
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureScaledTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3, info.m_nBaseTextureTransform, info.m_nDetail1Scale );
			if ( IS_PARAM_DEFINED( info.m_nBaseTextureTransform ) )
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureScaledTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_5, info.m_nDetail2TextureTransform, info.m_nDetail2Scale ); // 5-6
			else
				pContextData->m_SemiStaticCmdsOut.SetVertexShaderTextureScaledTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_5, info.m_nBaseTextureTransform, info.m_nDetail2Scale );
			
			// 8 free
			
			// PS Constants
			pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER0, BASETEXTURE, -1 );
			pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER1, false, info.m_nNormalMap, -1 );
			pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER14, info.m_nFresnelWarp, -1);

			if( bMaskMap1 )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER3, info.m_nMaskMap1, -1 );
			}

			if( bMaskMap2 )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER4, info.m_nMaskMap2, -1 );
			}

			if( bDiffuseWarp )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER6, info.m_nDiffuseWarp, -1 );
			}

			if( bFresnelColorWarp )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER8, info.m_nFresnelColorWarp, -1 );
			}

			if( bColorWarp )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER9, info.m_nColorWarp, -1 );
			}

			if( bEnvMap )
			{
				pContextData->m_SemiStaticCmdsOut.BindTexture( pShader, SHADER_SAMPLER10, info.m_nEnvMap, -1 );
			}

			float flDetail1BlendToFull = IS_PARAM_DEFINED( info.m_nDetail1BlendToFull ) ? clamp( params[info.m_nDetail1BlendToFull]->GetFloatValue(), 0.0f, 1.0f ): kDefaultBlendToFull;
			float flDiffuseWarpBlendToFull = IS_PARAM_DEFINED( info.m_nDiffuseWarpBlendToFull ) ? clamp( params[info.m_nDiffuseWarpBlendToFull]->GetFloatValue(), 0.0f, 1.0f ): kDefaultBlendToFull;
			float flSpecularBlendToFull = IS_PARAM_DEFINED( info.m_nSpecularBlendToFull ) ? clamp( params[info.m_nSpecularBlendToFull]->GetFloatValue(), 0.0f, 1.0f ): kDefaultBlendToFull;
			float flSpecularExponentBlendToFull = IS_PARAM_DEFINED( info.m_nSpecularExponentBlendToFull ) ? clamp( params[info.m_nSpecularExponentBlendToFull]->GetFloatValue(), 0.0f, 1.0f ): kDefaultBlendToFull;
			float flReflectionsTintByBaseBlendToNone = IS_PARAM_DEFINED( info.m_nReflectionsTintByBaseBlendToNone ) ? clamp( params[info.m_nReflectionsTintByBaseBlendToNone]->GetFloatValue(), 0.0f, 1.0f ): kDefaultBlendToFull;
			float flEnvMapBlendToFull = IS_PARAM_DEFINED( info.m_nEnvMapBlendToFull ) ? clamp( params[info.m_nEnvMapBlendToFull]->GetFloatValue(), 0.0f, 1.0 ): kDefaultBlendToFull;
			float flMetallnessBlendToFull = IS_PARAM_DEFINED( info.m_nMetallnessBlendToFull ) ? clamp( params[info.m_nMetallnessBlendToFull]->GetFloatValue(), 0.0f, 1.0f ): kDefaultBlendToFull;
			float flSelfIllumBlendToFull = IS_PARAM_DEFINED( info.m_nSelfIllumBlendToFull ) ? clamp( params[info.m_nSelfIllumBlendToFull]->GetFloatValue(), 0.0f, 1.0f ): kDefaultBlendToFull;

 			if( IS_PARAM_DEFINED( info.m_nSpecularColor ) )
				params[info.m_nSpecularColor]->GetVecValue( flConsts, 3 );
			else
				memcpy( flConsts, kDefaultSpecularColor, sizeof( kDefaultSpecularColor ) );
			flConsts[3] = IS_PARAM_DEFINED( info.m_nSpecularScale ) ? params[info.m_nSpecularScale]->GetFloatValue() : kDefaultIntensity;
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 0, flConsts, 1 );

			flConsts[0] = IS_PARAM_DEFINED( info.m_nSpecularExponent ) ? params[info.m_nSpecularExponent]->GetFloatValue(): kDefaultExponent;
			flConsts[1] = IS_PARAM_DEFINED( info.m_nRimLightScale ) ? params[info.m_nRimLightScale]->GetFloatValue() : kDefaultIntensity; 
			flConsts[2] = 0; //free
			flConsts[3] = 0; //free
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 1, flConsts, 1 );

			flConsts[0] = IS_PARAM_DEFINED( info.m_nColorWarpBlendFactor ) ? params[info.m_nColorWarpBlendFactor]->GetFloatValue(): kDefaultColorWarpBlendFactor;
			flConsts[1] = IS_PARAM_DEFINED( info.m_nFresnelColorWarpBlendToFull ) ? params[info.m_nFresnelColorWarpBlendToFull]->GetFloatValue(): kDefaultIntensity;
			flConsts[2] = IS_PARAM_DEFINED( info.m_nDetail1BlendFactor ) ? params[info.m_nDetail1BlendFactor]->GetFloatValue() : kDefaultDetail1BlendFactor;
			flConsts[3] = IS_PARAM_DEFINED( info.m_nDetail2BlendFactor ) ? params[info.m_nDetail2BlendFactor]->GetFloatValue() : kDefaultDetail2BlendFactor;
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 3, flConsts, 1 );

			// 10 is free

			// 19 is free

			flConsts[0] = flDetail1BlendToFull;
			flConsts[1] = flDiffuseWarpBlendToFull;
			flConsts[2] = flMetallnessBlendToFull;
			flConsts[3] = flSelfIllumBlendToFull;
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 26, flConsts, 1 );

			flConsts[0] = flSpecularBlendToFull;
			flConsts[1] = flEnvMapBlendToFull;
			flConsts[2] = flReflectionsTintByBaseBlendToNone;
			flConsts[3] = flSpecularExponentBlendToFull;
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderConstant( 27, flConsts, 1 );

			// 29 is free

			// 30 is free

			pContextData->m_SemiStaticCmdsOut.End();
 		}
	}

	if ( pShaderAPI ) //DYNAMIC_STATE
	{
		CCommandBufferBuilder< CFixedCommandStorageBuffer< 400 > > DynamicCmdsOut;
		DynamicCmdsOut.Call( pContextData->m_SemiStaticCmdsOut.Base() );

		///////////////////////////
		// dynamic block
		///////////////////////////

		bool bWriteDepthToAlpha = pShaderAPI->ShouldWriteDepthToDestAlpha() && !bAlphaBlend;
		bool bWriteWaterFogToAlpha = MATERIAL_FOG_LINEAR_BELOW_FOG_Z  && !bAlphaBlend;
		
		int nDetail2BlendMode = IS_PARAM_DEFINED( info.m_nDetail2BlendMode ) ? ( params[info.m_nDetail2BlendMode]->GetIntValue() ): kDefaultDetailBlendMode;
		nDetail2BlendMode = clamp( nDetail2BlendMode, 0, kMaxDetailBlendMode );

		LightState_t lightState = { 0, false, false };
		pShaderAPI->GetDX9LightState( &lightState );

		///////////////////////
		// VERTEX SHADER SETUP
		///////////////////////

		// Set Vertex Shader Combos
		if ( g_pHardwareConfig->GetDXSupportLevel() >= 95 )
		{
			//looks the same but we'll be enabling vs30-only effects in the shader
			// Is it necessary to do this?
			DECLARE_DYNAMIC_VERTEX_SHADER( customhero_vs30 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 ); 
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER( customhero_vs30 );
		}
		else
		{
			DECLARE_DYNAMIC_VERTEX_SHADER( customhero_vs20 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER( customhero_vs20 );
		}

		// VS constants
		float flConsts[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		// TODO: Move out of VS into PS for ps2.0b
		pShaderAPI->GetWorldSpaceCameraPosition( flConsts );
		flConsts[3] = 0.0f;
		DynamicCmdsOut.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_7, flConsts, 1 );

		//////////////////////
		// PIXEL SHADER SETUP
		//////////////////////

		DynamicCmdsOut.BindTexture( pShader, SHADER_SAMPLER2, info.m_nDetail2, info.m_nDetail2Frame );

		if ( bDetail1 )
		{
			DynamicCmdsOut.BindTexture( pShader, SHADER_SAMPLER5, info.m_nDetail1, info.m_nDetail1Frame );
		}

		flConsts[0] = 0.0f; //Empty
		flConsts[1] = 0.0f; //Empty
		flConsts[2] = 0.0f; //Empty
		flConsts[3] = bWriteDepthToAlpha ? 1.0f : 0.0f;
		DynamicCmdsOut.SetPixelShaderConstant( 11, flConsts, 1 );

		// TODO: Disable for PS2.0?  Vertex fog only?
		DynamicCmdsOut.SetPixelShaderFogParams( 12 );

		// TODO: Remove?  Can't use in PS2.0
		bool bFlashlightShadows = false;
		if ( bHasFlashlight && g_pHardwareConfig->SupportsPixelShaders_2_b() )
		{
			Assert( info.m_nFlashlightTexture >= 0 && info.m_nFlashlightTextureFrame >= 0 );
			VMatrix worldToTexture;
			ITexture *pFlashlightDepthTexture;
			FlashlightState_t state = pShaderAPI->GetFlashlightStateEx( worldToTexture, &pFlashlightDepthTexture );
			DynamicCmdsOut.BindTexture( pShader, SHADER_SAMPLER13, state.m_pSpotlightTexture, state.m_nSpotlightTextureFrame );
			bFlashlightShadows = state.m_bEnableShadows;

			SetFlashLightColorFromState( state, pShaderAPI, 28 );

			if( pFlashlightDepthTexture && g_pConfig->ShadowDepthTexture() && state.m_bEnableShadows )
			{
				DynamicCmdsOut.BindTexture( pShader, SHADER_SAMPLER11, pFlashlightDepthTexture, -1 );
				DynamicCmdsOut.BindStandardTexture( SHADER_SAMPLER12, TEXTURE_SHADOW_NOISE_2D );
			}

			float atten[4], pos[4], tweaks[4];

			atten[0] = state.m_fConstantAtten;		// Set the flashlight attenuation factors
			atten[1] = state.m_fLinearAtten;
			atten[2] = state.m_fQuadraticAtten;
			atten[3] = state.m_FarZAtten;
			DynamicCmdsOut.SetPixelShaderConstant( 13, atten, 1 );

			pos[0] = state.m_vecLightOrigin[0];		// Set the flashlight origin
			pos[1] = state.m_vecLightOrigin[1];
			pos[2] = state.m_vecLightOrigin[2];
			pos[3] = state.m_FarZ;

			DynamicCmdsOut.SetPixelShaderConstant( 14, pos, 1 );	// steps on rim boost
			DynamicCmdsOut.SetPixelShaderConstant( 15, worldToTexture.Base(), 4 ); // c15-c18

			// Tweaks associated with a given flashlight
			tweaks[0] = ShadowFilterFromState( state );
			tweaks[1] = ShadowAttenFromState( state );
			pShader->HashShadow2DJitter( state.m_flShadowJitterSeed, &tweaks[2], &tweaks[3] );
			DynamicCmdsOut.SetPixelShaderConstant( 2, tweaks, 1 );

			// Dimensions of screen, used for screen-space noise map sampling
			float vScreenScale[4] = {1280.0f / 32.0f, 720.0f / 32.0f, 0, 0};
			int nWidth, nHeight;
			pShaderAPI->GetBackBufferDimensions( nWidth, nHeight );

			int nTexWidth, nTexHeight;
			pShaderAPI->GetStandardTextureDimensions( &nTexWidth, &nTexHeight, TEXTURE_SHADOW_NOISE_2D );

			vScreenScale[0] = (float) nWidth  / nTexWidth;
			vScreenScale[1] = (float) nHeight / nTexHeight;

			DynamicCmdsOut.SetPixelShaderConstant( 31, vScreenScale, 1 );
		}
		DynamicCmdsOut.End();

		// end dynamic block
		pShaderAPI->ExecuteCommandBuffer( DynamicCmdsOut.Base() );
	
		// Set Pixel Shader Combos
		if ( g_pHardwareConfig->GetDXSupportLevel() >= 95 )
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( customhero_ps30 );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( NUM_LIGHTS, lightState.m_nNumLights );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( DETAIL2BLENDMODE, nDetail2BlendMode );
			//TODO: enable flashlight on ps30
			//SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHT, bHasFlashlight );
			//SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHTSHADOWS, bFlashlightShadows );
			SET_DYNAMIC_PIXEL_SHADER( customhero_ps30 );
		}
		else if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( customhero_ps20b );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( DETAIL2BLENDMODE, nDetail2BlendMode );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha );
			SET_DYNAMIC_PIXEL_SHADER( customhero_ps20b );
		}
		else
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( customhero_ps20 );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( DETAIL2BLENDMODE, nDetail2BlendMode );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha );
			SET_DYNAMIC_PIXEL_SHADER( customhero_ps20 );
		}
	}
	pShader->Draw();
}
