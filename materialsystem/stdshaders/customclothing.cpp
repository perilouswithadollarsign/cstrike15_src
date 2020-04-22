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

#include "customclothing_vs20.inc"
#include "customclothing_vs30.inc"
#include "customclothing_ps20b.inc"
#include "customclothing_ps30.inc"

#include "writez_vs20.inc"
#include "white_ps20.inc"
#include "white_ps20b.inc"

#include "shaderlib/commandbuilder.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

static const float kDefaultColor[3] = { 0.5f, 0.5f, 0.5f };

#define MODE_3D_PREVIEW 0
#define MODE_2D_COMPOSITE 1
#define MODE_3D_POSTCOMPOSITE 2

BEGIN_VS_SHADER( CustomClothing, "Help for CustomClothing" )

	BEGIN_SHADER_PARAMS

		//always required
		SHADER_PARAM( BASETEXTURE,		SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( BUMPMAP,			SHADER_PARAM_TYPE_TEXTURE, "", "" )

		//only for 3d post-composite
		SHADER_PARAM( AOSCREENBUFFER,			SHADER_PARAM_TYPE_TEXTURE, "", "" )

		//composite inputs, only for 3d preview and 2d composite
		SHADER_PARAM( AOMAP,			SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( MASKMAP,			SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( OFFSETMAP,		SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( PATTERN1,			SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( PATTERN2,			SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( GRIME,			SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( LOGOMAP,			SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( LOGOX,			SHADER_PARAM_TYPE_FLOAT, 0, "" )
		SHADER_PARAM( LOGOY,			SHADER_PARAM_TYPE_FLOAT, 0, "" )
		SHADER_PARAM( LOGOSCALE,		SHADER_PARAM_TYPE_FLOAT, 0, "" )
		SHADER_PARAM( LOGOROTATE,		SHADER_PARAM_TYPE_FLOAT, 0, "" )
		SHADER_PARAM( LOGO2X,			SHADER_PARAM_TYPE_FLOAT, 0, "" )
		SHADER_PARAM( LOGO2Y,			SHADER_PARAM_TYPE_FLOAT, 0, "" )
		SHADER_PARAM( LOGO2SCALE,		SHADER_PARAM_TYPE_FLOAT, 0, "" )
		SHADER_PARAM( LOGO2ROTATE,		SHADER_PARAM_TYPE_FLOAT, 0, "" )
		SHADER_PARAM( LOGO2ENABLED,		SHADER_PARAM_TYPE_BOOL, 0, "" )
		SHADER_PARAM( LOGOMASKCRISP,	SHADER_PARAM_TYPE_FLOAT, 0, "" )
		SHADER_PARAM( LOGOWEAR,			SHADER_PARAM_TYPE_FLOAT, 0, "" )
		SHADER_PARAM( PATTERN1COLOR1,	SHADER_PARAM_TYPE_VEC3, "", "" )
		SHADER_PARAM( PATTERN1COLOR2,	SHADER_PARAM_TYPE_VEC3, "", "" )
		SHADER_PARAM( PATTERN1COLOR3,	SHADER_PARAM_TYPE_VEC3, "", "" )
		SHADER_PARAM( PATTERN1COLOR4,	SHADER_PARAM_TYPE_VEC3, "", "" )
		SHADER_PARAM( PATTERN1SCALE,	SHADER_PARAM_TYPE_FLOAT, 0, "" )
		SHADER_PARAM( PATTERN2COLOR1,	SHADER_PARAM_TYPE_VEC3, "", "" )
		SHADER_PARAM( PATTERN2COLOR2,	SHADER_PARAM_TYPE_VEC3, "", "" )
		SHADER_PARAM( PATTERN2COLOR3,	SHADER_PARAM_TYPE_VEC3, "", "" )
		SHADER_PARAM( PATTERN2COLOR4,	SHADER_PARAM_TYPE_VEC3, "", "" )
		SHADER_PARAM( PATTERN2SCALE,	SHADER_PARAM_TYPE_FLOAT, 0, "" )		
		SHADER_PARAM( SWAPPATTERNMASKS,	SHADER_PARAM_TYPE_BOOL, 0, "" )
		SHADER_PARAM( CAVITYCONTRAST,	SHADER_PARAM_TYPE_FLOAT, 0, "" )
		SHADER_PARAM( OFFSETAMOUNT,		SHADER_PARAM_TYPE_FLOAT, 0, "" )
		
		SHADER_PARAM( ENTCENTER,		SHADER_PARAM_TYPE_VEC3, "", "" )

		//sets the behavior of the shader for different purposes
		SHADER_PARAM( COMPOSITEMODE,	SHADER_PARAM_TYPE_INTEGER, MODE_3D_PREVIEW, "" )

		//override parameter for dynamic combo
		SHADER_PARAM( AOPASS,			SHADER_PARAM_TYPE_BOOL, 0, "" )

	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		
		SET_PARAM_INT_IF_NOT_DEFINED( COMPOSITEMODE, MODE_3D_PREVIEW )

		SET_PARAM_INT_IF_NOT_DEFINED( AOPASS, 0 )
		
		SET_PARAM_STRING_IF_NOT_DEFINED( BASETEXTURE, "Dev/flat_normal" );
		SET_PARAM_STRING_IF_NOT_DEFINED( BUMPMAP, "Dev/flat_normal" );

		SET_PARAM_STRING_IF_NOT_DEFINED( AOSCREENBUFFER, "_rt_character_ssao" );
		
		if ( params[ COMPOSITEMODE ]->GetIntValue() == MODE_2D_COMPOSITE )
		{
			//we never want lighting to affect a 2D image-space composite
			SET_FLAGS2( MATERIAL_VAR2_LIGHTING_UNLIT );
		}
		else
		{
			SET_FLAGS2( MATERIAL_VAR2_LIGHTING_VERTEX_LIT );
			SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
		}

		if ( params[ COMPOSITEMODE ]->GetIntValue() == MODE_3D_PREVIEW && !g_pHardwareConfig->SupportsPixelShaders_3_0() )
		{
			DevWarning( "CustomClothing shader cannot run in preview quality mode on non-sm3 hardware.\n" );
			params[ COMPOSITEMODE ]->SetIntValue( MODE_3D_POSTCOMPOSITE );
		}

		if ( params[ COMPOSITEMODE ]->GetIntValue() == MODE_3D_PREVIEW || params[ COMPOSITEMODE ]->GetIntValue() == MODE_2D_COMPOSITE )
		{
			SET_PARAM_STRING_IF_NOT_DEFINED( GRIME, "models/player/custom_player/shared/clothing_grime" );

			// throw some warnings if we can't find necessary composite input maps

			if ( !params[ AOMAP ]->IsDefined() )
			{
				IMaterial *pOwnerMaterial = params[AOMAP]->GetOwningMaterial();
				char szMaterialName[MAX_PATH] = "unknown CustomClothing material";
				if ( pOwnerMaterial )	
					V_strcpy_safe( szMaterialName, pOwnerMaterial->GetName() );
				DevWarning( "Warning: $aomap is undefined in %s\n", szMaterialName );
			}

			if ( !params[ MASKMAP ]->IsDefined() )
			{
				IMaterial *pOwnerMaterial = params[MASKMAP]->GetOwningMaterial();
				char szMaterialName[MAX_PATH] = "unknown CustomClothing material";
				if ( pOwnerMaterial )	
					V_strcpy_safe( szMaterialName, pOwnerMaterial->GetName() );
				DevWarning( "Warning: $maskmap is undefined in %s\n", szMaterialName );
			}

			SET_PARAM_INT_IF_NOT_DEFINED( SWAPPATTERNMASKS, 0 );

			//if pattern2 is defined but pattern1 isn't, secretly move pattern2 into pattern1 and flag the pattern masks to swap
			if ( !params[ PATTERN1 ]->IsDefined() && params[ PATTERN2 ]->IsDefined() )
			{
				params[ PATTERN1 ]->SetStringValue( params[ PATTERN2 ]->GetStringValue() );
				params[ PATTERN1COLOR1 ]->SetVecValue( params[ PATTERN2COLOR1 ]->GetVecValue(), 4 );
				params[ PATTERN1COLOR2 ]->SetVecValue( params[ PATTERN2COLOR2 ]->GetVecValue(), 4 );
				params[ PATTERN1COLOR3 ]->SetVecValue( params[ PATTERN2COLOR3 ]->GetVecValue(), 4 );
				params[ PATTERN1COLOR4 ]->SetVecValue( params[ PATTERN2COLOR4 ]->GetVecValue(), 4 );
				params[ PATTERN1SCALE ]->SetFloatValue( params[ PATTERN2SCALE ]->GetFloatValue() );
				params[ PATTERN2 ]->SetUndefined();
				params[ SWAPPATTERNMASKS ]->SetIntValue( params[ SWAPPATTERNMASKS ]->GetIntValue() == 0 ? 1 : 0 );
			}

			//TODO: detect if pattern1 and pattern2 are the same and bind them to the same sampler

			SET_PARAM_FLOAT_IF_NOT_DEFINED( LOGOX, 0.5f );
			SET_PARAM_FLOAT_IF_NOT_DEFINED( LOGOY, 0.5f );
			SET_PARAM_FLOAT_IF_NOT_DEFINED( LOGOSCALE, 1 );
			SET_PARAM_FLOAT_IF_NOT_DEFINED( LOGOROTATE, 0 );

			SET_PARAM_FLOAT_IF_NOT_DEFINED( LOGO2X, 0.5f );
			SET_PARAM_FLOAT_IF_NOT_DEFINED( LOGO2Y, 0.5f );
			SET_PARAM_FLOAT_IF_NOT_DEFINED( LOGO2SCALE, 1 );
			SET_PARAM_FLOAT_IF_NOT_DEFINED( LOGO2ROTATE, 0 );

			SET_PARAM_FLOAT_IF_NOT_DEFINED( LOGOMASKCRISP, 0.5f );
			SET_PARAM_FLOAT_IF_NOT_DEFINED( LOGOWEAR, 0.5f );

			SET_PARAM_VEC_IF_NOT_DEFINED( PATTERN1COLOR1, kDefaultColor, 3 );
			SET_PARAM_VEC_IF_NOT_DEFINED( PATTERN1COLOR2, kDefaultColor, 3 );
			SET_PARAM_VEC_IF_NOT_DEFINED( PATTERN1COLOR3, kDefaultColor, 3 );
			SET_PARAM_VEC_IF_NOT_DEFINED( PATTERN1COLOR4, kDefaultColor, 3 );
			SET_PARAM_FLOAT_IF_NOT_DEFINED( PATTERN1SCALE, 1 );

			SET_PARAM_VEC_IF_NOT_DEFINED( PATTERN2COLOR1, kDefaultColor, 3 );
			SET_PARAM_VEC_IF_NOT_DEFINED( PATTERN2COLOR2, kDefaultColor, 3 );
			SET_PARAM_VEC_IF_NOT_DEFINED( PATTERN2COLOR3, kDefaultColor, 3 );
			SET_PARAM_VEC_IF_NOT_DEFINED( PATTERN2COLOR4, kDefaultColor, 3 );
			SET_PARAM_FLOAT_IF_NOT_DEFINED( PATTERN2SCALE, 1 );

			SET_PARAM_FLOAT_IF_NOT_DEFINED( CAVITYCONTRAST, 1 );
			SET_PARAM_FLOAT_IF_NOT_DEFINED( OFFSETAMOUNT, 0.2f );
		}

	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		if ( params[AOPASS]->GetIntValue() != 1 )
		{
			if ( params[BASETEXTURE]->IsDefined() )	{ LoadTexture( BASETEXTURE ); }
		}

		if ( params[BUMPMAP]->IsDefined() )		{ LoadBumpMap( BUMPMAP ); }

		if ( params[ COMPOSITEMODE ]->GetIntValue() == MODE_3D_POSTCOMPOSITE )
		{
			if ( params[AOSCREENBUFFER]->IsDefined() )	{ LoadTexture( AOSCREENBUFFER ); }
		}

		if ( params[ COMPOSITEMODE ]->GetIntValue() == MODE_3D_PREVIEW || params[ COMPOSITEMODE ]->GetIntValue() == MODE_2D_COMPOSITE )
		{
			if ( params[AOMAP]->IsDefined() )		{ LoadTexture( AOMAP );	}
			if ( params[MASKMAP]->IsDefined() )		{ LoadTexture( MASKMAP ); }
			if ( params[PATTERN1]->IsDefined() )	{ LoadTexture( PATTERN1 ); }
			if ( params[PATTERN2]->IsDefined() )	{ LoadTexture( PATTERN2 ); }
			if ( params[GRIME]->IsDefined() )		{ LoadTexture( GRIME ); }
			if ( params[OFFSETMAP]->IsDefined() )	{ LoadTexture( OFFSETMAP ); }
			if ( params[LOGOMAP]->IsDefined() )		{ LoadTexture( LOGOMAP ); }
		}
	}

	inline void DrawCustomClothing( IMaterialVar **params, IShaderShadow* pShaderShadow, IShaderDynamicAPI* pShaderAPI, int vertexCompression )
	{
		SHADOW_STATE
		{
			SetInitialShadowState( );

			int iCompositeMode = params[ COMPOSITEMODE ]->GetIntValue();

			if ( params[AOPASS]->GetIntValue() != 1 )
			{
				if ( params[BASETEXTURE]->IsDefined() )			{ pShaderShadow->EnableTexture( SHADER_SAMPLER0, true ); }
			}
			
			//pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, true );
			if ( params[BUMPMAP]->IsDefined() )				{ pShaderShadow->EnableTexture( SHADER_SAMPLER1, true ); }

			if ( iCompositeMode == MODE_3D_POSTCOMPOSITE )
			{
				if ( params[AOSCREENBUFFER]->IsDefined() )	{ pShaderShadow->EnableTexture( SHADER_SAMPLER10, true ); }
			}

			bool bNeedCompositeInputs = iCompositeMode == MODE_2D_COMPOSITE || iCompositeMode == MODE_3D_PREVIEW;

			if ( bNeedCompositeInputs )
			{
				if ( params[AOMAP]->IsDefined() )			{ pShaderShadow->EnableTexture( SHADER_SAMPLER2, true ); }
				if ( params[MASKMAP]->IsDefined() )			{ pShaderShadow->EnableTexture( SHADER_SAMPLER3, true ); }
				if ( params[PATTERN1]->IsDefined() )		{ pShaderShadow->EnableTexture( SHADER_SAMPLER4, true ); }
				if ( params[PATTERN2]->IsDefined() )		{ pShaderShadow->EnableTexture( SHADER_SAMPLER6, true ); }
				if ( params[GRIME]->IsDefined() )			{ pShaderShadow->EnableTexture( SHADER_SAMPLER7, true ); }
				if ( params[OFFSETMAP]->IsDefined() )		{ pShaderShadow->EnableTexture( SHADER_SAMPLER8, true ); }
				if ( params[LOGOMAP]->IsDefined() )			{ pShaderShadow->EnableTexture( SHADER_SAMPLER9, true ); }
			}

			if ( iCompositeMode == MODE_2D_COMPOSITE )
			{
				EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ZERO ); //assume we're compositing onto a dirty surface
			}
			else
			{
				EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );

				pShaderShadow->AlphaFunc( SHADER_ALPHAFUNC_ALWAYS, 0.0f );
				/*
				BlendFunc( SHADER_BLEND_ZERO, SHADER_BLEND_ZERO );
				BlendOp( SHADER_BLEND_OP_ADD );
				EnableBlendingSeparateAlpha( false );
				BlendFuncSeparateAlpha( SHADER_BLEND_ZERO, SHADER_BLEND_ZERO );
				BlendOpSeparateAlpha( SHADER_BLEND_OP_ADD );
				AlphaFunc( SHADER_ALPHAFUNC_GEQUAL, 0.7f );
				*/

				// Normalizing cube map
				pShaderShadow->EnableTexture( SHADER_SAMPLER5, true );
			}

			unsigned int flags = VERTEX_POSITION| VERTEX_FORMAT_COMPRESSED;
			if ( iCompositeMode != MODE_2D_COMPOSITE )
				flags |= VERTEX_NORMAL;

			int nTexCoordCount = 1;
			int userDataSize = 0;
			if (IS_FLAG_SET( MATERIAL_VAR_VERTEXCOLOR ))
			{
				flags |= VERTEX_COLOR;
			}
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );
			
			if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_STATIC_VERTEX_SHADER( customclothing_vs30 );
				SET_STATIC_VERTEX_SHADER_COMBO( COMPOSITEMODE, iCompositeMode );
				SET_STATIC_VERTEX_SHADER( customclothing_vs30 );
			}
			else
			{
				DECLARE_STATIC_VERTEX_SHADER( customclothing_vs20 );
				SET_STATIC_VERTEX_SHADER_COMBO( COMPOSITEMODE, iCompositeMode );
				SET_STATIC_VERTEX_SHADER( customclothing_vs20 );
			}

			bool bUsePattern1 =			bNeedCompositeInputs && params[ PATTERN1 ]->IsDefined() && params[ PATTERN1 ]->IsTexture();
			bool bUsePattern2 =			bNeedCompositeInputs && params[ PATTERN2 ]->IsDefined() && params[ PATTERN2 ]->IsTexture();
			bool bUsePatternOffset =	bNeedCompositeInputs && (bUsePattern1 || bUsePattern2) && params[ OFFSETMAP ]->IsDefined() && params[ OFFSETMAP ]->IsTexture();
			bool bUseLogo1 =			bNeedCompositeInputs && params[ LOGOMAP ]->IsDefined() && params[ LOGOMAP ]->IsTexture();
			bool bUseLogo2 =			bNeedCompositeInputs && bUseLogo1 && params[ LOGO2ENABLED ]->IsDefined() && (params[ LOGO2ENABLED ]->GetIntValue() > 0);
			bool bSwapPatternMasks =	bNeedCompositeInputs && bUsePattern2 && (params[ SWAPPATTERNMASKS ]->GetIntValue() > 0);

			bool bCSMEnabled = g_pHardwareConfig->SupportsCascadedShadowMapping();
			int	nCSMQualityComboValue = g_pHardwareConfig->GetCSMShaderMode( materials->GetCurrentConfigForVideoCard().GetCSMQualityMode() );

			if ( iCompositeMode == MODE_2D_COMPOSITE || !bCSMEnabled || params[ AOPASS ]->GetIntValue() > 0 )
			{
				bCSMEnabled = false;
				nCSMQualityComboValue = 0;
			}

			if ( params[ AOPASS ]->GetIntValue() > 0 )
			{
				bUsePattern1 =		false;
				bUsePattern2 =		false;
				bUsePatternOffset =	false;
				bUseLogo1 =			false;
				bUseLogo2 =			false;
				bSwapPatternMasks =	false;
			}

			if( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_STATIC_PIXEL_SHADER( customclothing_ps30 );
				SET_STATIC_PIXEL_SHADER_COMBO( CASCADED_SHADOW_MAPPING, bCSMEnabled );
				SET_STATIC_PIXEL_SHADER_COMBO( COMPOSITEMODE, iCompositeMode );
				SET_STATIC_PIXEL_SHADER_COMBO( CSM_MODE, nCSMQualityComboValue );
				SET_STATIC_PIXEL_SHADER_COMBO( USE_PATTERN1, bUsePattern1 );
				SET_STATIC_PIXEL_SHADER_COMBO( USE_PATTERN2, bUsePattern2 );
				SET_STATIC_PIXEL_SHADER_COMBO( USE_PATTERN_OFFSET, bUsePatternOffset );
				SET_STATIC_PIXEL_SHADER_COMBO( USE_LOGO1, bUseLogo1 );
				SET_STATIC_PIXEL_SHADER_COMBO( USE_LOGO2, bUseLogo2 );
				SET_STATIC_PIXEL_SHADER_COMBO( SWAP_PATTERN_MASKS, bSwapPatternMasks );
				SET_STATIC_PIXEL_SHADER( customclothing_ps30 );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( customclothing_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( COMPOSITEMODE, iCompositeMode );
				SET_STATIC_PIXEL_SHADER_COMBO( USE_PATTERN1, bUsePattern1 );
				SET_STATIC_PIXEL_SHADER_COMBO( USE_PATTERN2, bUsePattern2 );
				SET_STATIC_PIXEL_SHADER_COMBO( USE_PATTERN_OFFSET, bUsePatternOffset );
				SET_STATIC_PIXEL_SHADER_COMBO( USE_LOGO1, bUseLogo1 );
				SET_STATIC_PIXEL_SHADER_COMBO( USE_LOGO2, bUseLogo2 );
				SET_STATIC_PIXEL_SHADER_COMBO( SWAP_PATTERN_MASKS, bSwapPatternMasks );
				SET_STATIC_PIXEL_SHADER( customclothing_ps20b );
			}
			
			pShaderShadow->EnableAlphaWrites( true );
			pShaderShadow->EnableDepthWrites( true );
			pShaderShadow->EnableSRGBWrite( true );
			pShaderShadow->EnableBlending( false ); // important for csm shadows
			pShaderShadow->EnableAlphaTest( true ); // for ghillie suits and other leafy things

			if ( iCompositeMode != MODE_2D_COMPOSITE )
			{
				PI_BeginCommandBuffer();
				PI_SetPixelShaderAmbientLightCube( PSREG_AMBIENT_CUBE );
				PI_SetPixelShaderLocalLighting( PSREG_LIGHT_INFO_ARRAY );
				//pShader->PI_SetModulationPixelShaderDynamicState_LinearColorSpace( 1 );
				PI_EndCommandBuffer();
			}

		}
		DYNAMIC_STATE
		{
			pShaderAPI->SetDefaultState();

			SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, BASETEXTURETRANSFORM );

			int iCompositeMode = params[ COMPOSITEMODE ]->GetIntValue();

			if ( params[AOPASS]->GetIntValue() != 1 )
			{
				if ( params[BASETEXTURE]->IsDefined() )			{ BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, BASETEXTURE, -1 ); }
			}
			
			if ( params[BUMPMAP]->IsDefined() )				{ BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, BUMPMAP, -1 ); }

			if ( iCompositeMode == MODE_3D_POSTCOMPOSITE )
			{
				//TEXTURE_BINDFLAGS_SHADOWDEPTH
				if ( params[AOSCREENBUFFER]->IsDefined() )	{ BindTexture( SHADER_SAMPLER10, TEXTURE_BINDFLAGS_NONE, AOSCREENBUFFER, -1 ); }
			}

			if ( iCompositeMode == MODE_2D_COMPOSITE || iCompositeMode == MODE_3D_PREVIEW )
			{
				if ( params[AOMAP]->IsDefined() )			{ BindTexture( SHADER_SAMPLER2, TEXTURE_BINDFLAGS_NONE, AOMAP, -1 ); }
				if ( params[MASKMAP]->IsDefined() )			{ BindTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, MASKMAP, -1 ); }
				if ( params[PATTERN1]->IsDefined() )		{ BindTexture( SHADER_SAMPLER4, TEXTURE_BINDFLAGS_NONE, PATTERN1, -1 ); }
				if ( params[PATTERN2]->IsDefined() )		{ BindTexture( SHADER_SAMPLER6, TEXTURE_BINDFLAGS_NONE, PATTERN2, -1 ); }
				if ( params[GRIME]->IsDefined() )			{ BindTexture( SHADER_SAMPLER7, TEXTURE_BINDFLAGS_NONE, GRIME, -1 ); }
				if ( params[OFFSETMAP]->IsDefined() )		{ BindTexture( SHADER_SAMPLER8, TEXTURE_BINDFLAGS_NONE, OFFSETMAP, -1 ); }
				if ( params[LOGOMAP]->IsDefined() )			{ BindTexture( SHADER_SAMPLER9, TEXTURE_BINDFLAGS_SRGBREAD, LOGOMAP, -1 ); }
			

				float c0[4] = {		SrgbGammaToLinear( params[PATTERN1COLOR1]->GetVecValue()[0] / 255.0 ),
									SrgbGammaToLinear( params[PATTERN1COLOR1]->GetVecValue()[1] / 255.0 ),
									SrgbGammaToLinear( params[PATTERN1COLOR1]->GetVecValue()[2] / 255.0 ),
									SrgbGammaToLinear( params[PATTERN1COLOR4]->GetVecValue()[0] / 255.0 ) };

				float c1[4] = {		SrgbGammaToLinear( params[PATTERN1COLOR2]->GetVecValue()[0] / 255.0 ),
									SrgbGammaToLinear( params[PATTERN1COLOR2]->GetVecValue()[1] / 255.0 ),
									SrgbGammaToLinear( params[PATTERN1COLOR2]->GetVecValue()[2] / 255.0 ),
									SrgbGammaToLinear( params[PATTERN1COLOR4]->GetVecValue()[1] / 255.0 ) };

				float c2[4] = {		SrgbGammaToLinear( params[PATTERN1COLOR3]->GetVecValue()[0] / 255.0 ),
									SrgbGammaToLinear( params[PATTERN1COLOR3]->GetVecValue()[1] / 255.0 ),
									SrgbGammaToLinear( params[PATTERN1COLOR3]->GetVecValue()[2] / 255.0 ),
									SrgbGammaToLinear( params[PATTERN1COLOR4]->GetVecValue()[2] / 255.0 ) };
				
				pShaderAPI->SetPixelShaderConstant( 0, c0, 1 );
				pShaderAPI->SetPixelShaderConstant( 1, c1, 1 );
				pShaderAPI->SetPixelShaderConstant( 2, c2, 1 );

				float c10[4] = {	SrgbGammaToLinear( params[PATTERN2COLOR1]->GetVecValue()[0] / 255.0 ),
									SrgbGammaToLinear( params[PATTERN2COLOR1]->GetVecValue()[1] / 255.0 ),
									SrgbGammaToLinear( params[PATTERN2COLOR1]->GetVecValue()[2] / 255.0 ),
									SrgbGammaToLinear( params[PATTERN2COLOR4]->GetVecValue()[0] / 255.0 ) };

				float c11[4] = {	SrgbGammaToLinear( params[PATTERN2COLOR2]->GetVecValue()[0] / 255.0 ),
									SrgbGammaToLinear( params[PATTERN2COLOR2]->GetVecValue()[1] / 255.0 ),
									SrgbGammaToLinear( params[PATTERN2COLOR2]->GetVecValue()[2] / 255.0 ),
									SrgbGammaToLinear( params[PATTERN2COLOR4]->GetVecValue()[1] / 255.0 ) };

				float c12[4] = {	SrgbGammaToLinear( params[PATTERN2COLOR3]->GetVecValue()[0] / 255.0 ),
									SrgbGammaToLinear( params[PATTERN2COLOR3]->GetVecValue()[1] / 255.0 ),
									SrgbGammaToLinear( params[PATTERN2COLOR3]->GetVecValue()[2] / 255.0 ),
									SrgbGammaToLinear( params[PATTERN2COLOR4]->GetVecValue()[2] / 255.0 ) };

				pShaderAPI->SetPixelShaderConstant( 10, c10, 1 );
				pShaderAPI->SetPixelShaderConstant( 11, c11, 1 );
				pShaderAPI->SetPixelShaderConstant( 12, c12, 1 );

				float c13[4] = {	params[PATTERN1SCALE]->GetFloatValue(),
									params[PATTERN2SCALE]->GetFloatValue(),
									params[CAVITYCONTRAST]->GetFloatValue(),
									params[OFFSETAMOUNT]->GetFloatValue() };
				pShaderAPI->SetPixelShaderConstant( 13, c13, 1 );

				float c14[4] = {	params[LOGOX]->GetFloatValue(),
									params[LOGOY]->GetFloatValue(),
									params[LOGOSCALE]->GetFloatValue(),
									params[LOGOROTATE]->GetFloatValue() };
				pShaderAPI->SetPixelShaderConstant( 14, c14, 1 );

				float c15[4] = {	params[LOGO2X]->GetFloatValue(),
									params[LOGO2Y]->GetFloatValue(),
									params[LOGO2SCALE]->GetFloatValue(),
									params[LOGO2ROTATE]->GetFloatValue() };
				pShaderAPI->SetPixelShaderConstant( 15, c15, 1 );

				float c16[4] = {	0.5f - params[LOGOMASKCRISP]->GetFloatValue(),
									0.5f + params[LOGOMASKCRISP]->GetFloatValue(),
									params[LOGOWEAR]->GetFloatValue(),
									0 };
				pShaderAPI->SetPixelShaderConstant( 16, c16, 1 );

			}

			bool bAOPrePass = params[ AOPASS ]->GetIntValue() > 0;

			if ( iCompositeMode != MODE_2D_COMPOSITE )
			{
				pShaderAPI->BindStandardTexture( SHADER_SAMPLER5, TEXTURE_BINDFLAGS_NONE, TEXTURE_NORMALIZATION_CUBEMAP_SIGNED );

				float c18[4];
				pShaderAPI->GetWorldSpaceCameraPosition( c18 );

				if ( bAOPrePass )
				{
					Vector vecWorldSpaceCamPos = Vector( c18[0], c18[1], c18[2] );
					Vector vecEntityCenter = Vector( params[ ENTCENTER ]->GetVecValue()[0], params[ ENTCENTER ]->GetVecValue()[1], params[ ENTCENTER ]->GetVecValue()[2] );
					c18[3] = (vecWorldSpaceCamPos - vecEntityCenter).Length();
				}
				else
				{
					c18[3] = 0;
				}
				
				pShaderAPI->SetPixelShaderConstant( 18, c18, 1 );

			}

			if ( iCompositeMode == MODE_3D_POSTCOMPOSITE )
			{
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
			}


			int nNumLights = 0;
			if ( !bAOPrePass && iCompositeMode != MODE_2D_COMPOSITE )
			{
				LightState_t lightState = {0, false, false};
				pShaderAPI->GetDX9LightState( &lightState );
				nNumLights = lightState.m_nNumLights;
			}

			if( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_DYNAMIC_VERTEX_SHADER( customclothing_vs30 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( NUM_LIGHTS, nNumLights );
				SET_DYNAMIC_VERTEX_SHADER( customclothing_vs30 );
			}
			else
			{
				DECLARE_DYNAMIC_VERTEX_SHADER( customclothing_vs20 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( NUM_LIGHTS, nNumLights );
				SET_DYNAMIC_VERTEX_SHADER( customclothing_vs20 );
			}

			bool bCSMEnabled = pShaderAPI->IsCascadedShadowMapping() && (iCompositeMode != MODE_2D_COMPOSITE);
			if ( bCSMEnabled && !bAOPrePass )
			{
				ITexture *pDepthTextureAtlas = NULL;
				const CascadedShadowMappingState_t &cascadeState = pShaderAPI->GetCascadedShadowMappingState( &pDepthTextureAtlas );

				if ( pDepthTextureAtlas )
				{
					BindTexture( SHADER_SAMPLER15, TEXTURE_BINDFLAGS_SHADOWDEPTH, pDepthTextureAtlas, 0 );

					//float vCSMTexParams[4] = { cascadeState.m_TexParams3.m_flDistLerpFactorBase, cascadeState.m_TexParams3.m_flDistLerpFactorInvRange, cascadeState.m_TexParams.m_flInvShadowTextureWidth, 0.0f };
					//pShaderAPI->SetPixelShaderConstant( 17, &cascadeState.m_vCamPosition.x, 1 );
					//pShaderAPI->SetPixelShaderConstant( 18, vCSMTexParams, 1 );

					pShaderAPI->SetPixelShaderConstant( 64, &cascadeState.m_vLightColor.x, CASCADED_SHADOW_MAPPING_CONSTANT_BUFFER_SIZE );
				}
			}

			if( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( customclothing_ps30 );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( NUM_LIGHTS, nNumLights );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( DYN_CSM_ENABLED, bCSMEnabled && !bAOPrePass );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( AO_MODE, bAOPrePass );
				SET_DYNAMIC_PIXEL_SHADER( customclothing_ps30 );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( customclothing_ps20b );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( NUM_LIGHTS, nNumLights );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( DYN_CSM_ENABLED, 0 );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( AO_MODE, bAOPrePass );
				SET_DYNAMIC_PIXEL_SHADER( customclothing_ps20b );
			}

		}
		Draw();
	}

	SHADER_DRAW
	{
		DrawCustomClothing( params, pShaderShadow, pShaderAPI, vertexCompression );
	}

	void ExecuteFastPath( int *dynVSIdx, int *dynPSIdx,  IMaterialVar** params, IShaderDynamicAPI * pShaderAPI, 
		VertexCompressionType_t vertexCompression, CBasePerMaterialContextData **pContextDataPtr, BOOL bCSMEnabled )
	{

		//only intended for csm shadow depth pass

		DECLARE_DYNAMIC_VERTEX_SHADER( writez_vs20 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
		SET_DYNAMIC_VERTEX_SHADER( writez_vs20 );

		*dynVSIdx = _vshIndex.GetIndex();

		// No pixel shader on Direct3D, doubles fill rate
		if ( IsOSXOpenGL() )
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( white_ps20 );
			SET_DYNAMIC_PIXEL_SHADER( white_ps20 );
			*dynPSIdx = _pshIndex.GetIndex();
		}
		else
		{
			*dynPSIdx = 0;
		}
	}

	bool IsTranslucent( IMaterialVar **params ) const
	{
		return false;
	}

END_SHADER


