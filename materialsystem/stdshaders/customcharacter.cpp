//=============== Copyright © Valve Corporation, All rights reserved. =================//
//
//=====================================================================================//

#include "BaseVSShader.h"
#include "customcharacter_vs30.inc"
#include "customcharacter_ps30.inc"

#include "../materialsystem_global.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

BEGIN_VS_SHADER( CustomCharacter, "Help for CustomCharacter Shader" )
	BEGIN_SHADER_PARAMS
	
	// Original material samplers
	SHADER_PARAM( BUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "", "" )
	SHADER_PARAM( MASKS1, SHADER_PARAM_TYPE_TEXTURE, "", "" )
	SHADER_PARAM( BASEALPHAPHONGMASK, SHADER_PARAM_TYPE_BOOL, "", "" )
	SHADER_PARAM( BASEALPHAENVMASK, SHADER_PARAM_TYPE_BOOL, "", "" )
	SHADER_PARAM( BUMPALPHAENVMASK, SHADER_PARAM_TYPE_BOOL, "", "" )

	// Composite Samplers
	SHADER_PARAM( MATERIALMASK, SHADER_PARAM_TYPE_TEXTURE, "", "" )
	SHADER_PARAM( AO, SHADER_PARAM_TYPE_TEXTURE, "", "" )
	SHADER_PARAM( DETAIL, SHADER_PARAM_TYPE_TEXTURE, "", "" )
	SHADER_PARAM( DETAILNORMAL, SHADER_PARAM_TYPE_TEXTURE, "", "" )
	SHADER_PARAM( GRUNGE, SHADER_PARAM_TYPE_TEXTURE, "", "" )
	SHADER_PARAM( GRUNGETEXTURETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "", "" )
	SHADER_PARAM( NOISE, SHADER_PARAM_TYPE_TEXTURE, "", "" )

	// Composite Parameters Per-Material
	SHADER_PARAM( DETAILSCALE, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( DETAILPHONGBOOST, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( WEARDETAILPHONGBOOST, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( DAMAGEDETAILPHONGBOOST, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( DETAILENVBOOST, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( WEARDETAILENVBOOST, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( DAMAGEDETAILENVBOOST, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( DETAILPHONGALBEDOTINT, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( DAMAGEDETAILSATURATION, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( DAMAGEDETAILBRIGHTNESSADJUSTMENT, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( DETAILWARPINDEX, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( DETAILMETALNESS, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( DETAILNORMALDEPTH, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( DAMAGENORMALEDGEDEPTH, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( DAMAGEEDGEPHONGBOOST, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( DAMAGEEDGEENVBOOST, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( CURVATUREWEARBOOST, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( CURVATUREWEARPOWER, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( GRIME, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( GRIMESATURATION, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( GRIMEBRIGHTNESSADJUSTMENT, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( DAMAGEGRUNGE, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( DETAILGRUNGE, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( GRUNGEMAX, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( DAMAGELEVELS1, SHADER_PARAM_TYPE_VEC2, "", "" )
	SHADER_PARAM( DAMAGELEVELS2, SHADER_PARAM_TYPE_VEC2, "", "" )
	SHADER_PARAM( DAMAGELEVELS3, SHADER_PARAM_TYPE_VEC2, "", "" )
	SHADER_PARAM( DAMAGELEVELS4, SHADER_PARAM_TYPE_VEC2, "", "" )

	// Composite Pattern Parameters
	SHADER_PARAM( PATTERN, SHADER_PARAM_TYPE_TEXTURE, "", "" )
	SHADER_PARAM( PATTERNTEXTURETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "", "" )
	SHADER_PARAM( PATTERNREPLACEINDEX, SHADER_PARAM_TYPE_INTEGER, "", "" )
	SHADER_PARAM( PATTERNCOLORINDICES, SHADER_PARAM_TYPE_VEC4, "", "" )
	SHADER_PARAM( PATTERNDETAILINFLUENCE, SHADER_PARAM_TYPE_FLOAT, "", "" )
	SHADER_PARAM( PATTERNPHONGFACTOR, SHADER_PARAM_TYPE_FLOAT, "", "" )
	SHADER_PARAM( PATTERNPAINTTHICKNESS, SHADER_PARAM_TYPE_FLOAT, "", "" )
	SHADER_PARAM( FLIPFIXUP, SHADER_PARAM_TYPE_FLOAT, "", "" )

	// Composite Parameters
	SHADER_PARAM( PALETTECOLOR1, SHADER_PARAM_TYPE_COLOR, "", "" )
	SHADER_PARAM( PALETTECOLOR2, SHADER_PARAM_TYPE_COLOR, "", "" )
	SHADER_PARAM( PALETTECOLOR3, SHADER_PARAM_TYPE_COLOR, "", "" )
	SHADER_PARAM( PALETTECOLOR4, SHADER_PARAM_TYPE_COLOR, "", "" )
	SHADER_PARAM( PALETTECOLOR5, SHADER_PARAM_TYPE_COLOR, "", "" )
	SHADER_PARAM( PALETTECOLOR6, SHADER_PARAM_TYPE_COLOR, "", "" )
	SHADER_PARAM( PALETTECOLOR7, SHADER_PARAM_TYPE_COLOR, "", "" )
	SHADER_PARAM( PALETTECOLOR8, SHADER_PARAM_TYPE_COLOR, "", "" )
	SHADER_PARAM( CUSTOMPAINTJOB, SHADER_PARAM_TYPE_BOOL, "", "" )
	SHADER_PARAM( ALLOVERPAINTJOB, SHADER_PARAM_TYPE_BOOL, "", "" )

	SHADER_PARAM( WEARPROGRESS, SHADER_PARAM_TYPE_FLOAT, "", "" )
	SHADER_PARAM( WEAREXPONENT, SHADER_PARAM_TYPE_FLOAT, "", "" )

	SHADER_PARAM( GRUNGETEXTUREROTATION, SHADER_PARAM_TYPE_MATRIX, "", "" )
	SHADER_PARAM( PATTERNTEXTUREROTATION, SHADER_PARAM_TYPE_MATRIX, "", "")
		
END_SHADER_PARAMS

SHADER_INIT_PARAMS()
{	
	if ( !params[BASEALPHAPHONGMASK]->IsDefined() )
	{
		params[BASEALPHAPHONGMASK]->SetIntValue( 0 );
	}

	if ( !params[BASEALPHAENVMASK]->IsDefined() )
	{
		params[BASEALPHAENVMASK]->SetIntValue( 0 );
	}

	if ( !params[BUMPALPHAENVMASK]->IsDefined() )
	{
		params[BUMPALPHAENVMASK]->SetIntValue( 0 );
	}

	if ( !params[DETAILSCALE]->IsDefined() )
	{
		params[DETAILSCALE]->SetFloatValue( 4.0f );
	}

	if ( !params[DETAILPHONGBOOST]->IsDefined() )
	{
		params[DETAILPHONGBOOST]->SetVecValue( 1.0f, 1.0f, 1.0f, 1.0f );
	}

	if ( !params[WEARDETAILPHONGBOOST]->IsDefined() )
	{
		params[WEARDETAILPHONGBOOST]->SetVecValue( 0.0f, 0.0f, 0.0f, 0.0f );
	}

	if ( !params[DAMAGEDETAILPHONGBOOST]->IsDefined() )
	{
		params[DAMAGEDETAILPHONGBOOST]->SetVecValue( 1.0f, 1.0f, 1.0f, 1.0f );
	}

	if ( !params[DETAILENVBOOST]->IsDefined() )
	{
		params[DETAILENVBOOST]->SetVecValue( 1.0f, 1.0f, 1.0f, 1.0f );
	}

	if ( !params[WEARDETAILENVBOOST]->IsDefined() )
	{
		params[WEARDETAILENVBOOST]->SetVecValue( 0.0f, 0.0f, 0.0f, 0.0f );
	}

	if ( !params[DAMAGEDETAILENVBOOST]->IsDefined() )
	{
		params[DAMAGEDETAILENVBOOST]->SetVecValue( 0.0f, 0.0f, 0.0f, 0.0f );
	}

	if ( !params[DETAILPHONGALBEDOTINT]->IsDefined() )
	{
		params[DETAILPHONGALBEDOTINT]->SetVecValue( 0.0f, 0.0f, 0.0f, 0.0f );
	}

	if ( !params[DETAILWARPINDEX]->IsDefined() )
	{
		params[DETAILWARPINDEX]->SetVecValue( 0.0f, 0.0f, 0.0f, 0.0f );
	}

	// Metalness is just a multiply on the albedo, so invert the defined amount to get the multiplier
	if ( !params[DETAILMETALNESS]->IsDefined() )
	{
		params[DETAILMETALNESS]->SetVecValue( 1.0f, 1.0f, 1.0f, 1.0f );
	}
	else
	{
		float vParams[4] = { 0, 0, 0, 0 };
		params[DETAILMETALNESS]->GetVecValue( vParams, 4 );
		for ( int i = 0; i < 4; i++ )
		{
			vParams[i] = clamp( vParams[i], 0.0f, 1.0f );
		}
		params[DETAILMETALNESS]->SetVecValue( vParams, 4 );
	}

	if ( !params[DETAILNORMALDEPTH]->IsDefined() )
	{
		params[DETAILNORMALDEPTH]->SetVecValue( 1.0f, 1.0f, 1.0f, 1.0f );
	}

	if ( !params[DAMAGENORMALEDGEDEPTH]->IsDefined() )
	{
		params[DAMAGENORMALEDGEDEPTH]->SetVecValue( 1.0f, 1.0f, 1.0f, 1.0f );
	}

	if ( !params[DAMAGEEDGEPHONGBOOST]->IsDefined() )
	{
		params[DAMAGEEDGEPHONGBOOST]->SetVecValue( 0.0f, 0.0f, 0.0f, 0.0f );
	}

	if ( !params[DAMAGEEDGEENVBOOST]->IsDefined() )
	{
		params[DAMAGEEDGEENVBOOST]->SetVecValue( 0.0f, 0.0f, 0.0f, 0.0f );
	}

	if ( !params[DAMAGELEVELS1]->IsDefined() )
	{
		params[DAMAGELEVELS1]->SetVecValue( 0.0f, 1.0f );
	}

	if ( !params[DAMAGELEVELS2]->IsDefined() )
	{
		params[DAMAGELEVELS2]->SetVecValue( 0.0f, 1.0f );
	}

	if ( !params[DAMAGELEVELS3]->IsDefined() )
	{
		params[DAMAGELEVELS3]->SetVecValue( 0.0f, 1.0f );
	}

	if ( !params[DAMAGELEVELS4]->IsDefined() )
	{
		params[DAMAGELEVELS4]->SetVecValue( 0.0f, 1.0f );
	}

	if ( !params[CUSTOMPAINTJOB]->IsDefined() )
	{
		params[CUSTOMPAINTJOB]->SetIntValue( 0 );
	}

	if ( !params[PATTERNDETAILINFLUENCE]->IsDefined() )
	{
		params[PATTERNDETAILINFLUENCE]->SetFloatValue( 32.0f );
	}

	if ( !params[ALLOVERPAINTJOB]->IsDefined() )
	{
		params[ALLOVERPAINTJOB]->SetIntValue( 0 );
	}

	if ( !params[CURVATUREWEARBOOST]->IsDefined() )
	{
		params[CURVATUREWEARBOOST]->SetVecValue( 0.0f, 0.0f, 0.0f, 0.0f );
	}

	if ( !params[CURVATUREWEARPOWER]->IsDefined() )
	{
		params[CURVATUREWEARPOWER]->SetVecValue( 1.0f, 1.0f, 1.0f, 1.0f );
	}


	if ( !params[DAMAGEDETAILSATURATION]->IsDefined() )
	{
		params[DAMAGEDETAILSATURATION]->SetVecValue( 0.0f, 0.0f, 0.0f, 0.0f );
	}

	if (!params[DAMAGEDETAILBRIGHTNESSADJUSTMENT]->IsDefined())
	{
		params[DAMAGEDETAILBRIGHTNESSADJUSTMENT]->SetVecValue(0.0f, 0.0f, 0.0f, 0.0f);
	}

	if ( !params[GRIME]->IsDefined() )
	{
		params[GRIME]->SetVecValue( 0.25f, 0.25f, 0.25f, 0.25f );
	}

	if ( !params[GRIMESATURATION]->IsDefined() )
	{
		params[GRIMESATURATION]->SetVecValue( 0.0f, 0.0f, 0.0f, 0.0f );
	}

	if ( !params[GRIMEBRIGHTNESSADJUSTMENT]->IsDefined() )
	{
		params[GRIMEBRIGHTNESSADJUSTMENT]->SetVecValue( 0.0f, 0.0f, 0.0f, 0.0f );
	}

	if ( !params[DAMAGEGRUNGE]->IsDefined() )
	{
		params[DAMAGEGRUNGE]->SetVecValue( 0.25f, 0.25f, 0.25f, 0.25f );
	}

	if ( !params[DETAILGRUNGE]->IsDefined() )
	{
		params[DETAILGRUNGE]->SetVecValue( 0.0f, 0.0f, 0.0f, 0.0f );
	}

	if (!params[GRUNGEMAX]->IsDefined())
	{
		params[GRUNGEMAX]->SetVecValue( 1.0f, 1.0f, 1.0f, 1.0f );
	}

	if (!params[WEARPROGRESS]->IsDefined())
	{
		params[WEARPROGRESS]->SetFloatValue(0.0f);
	}

	if (!params[WEAREXPONENT]->IsDefined())
	{
		params[WEAREXPONENT]->SetFloatValue(1.0f);
	}

	if ( !params[PATTERNCOLORINDICES]->IsDefined() )
	{
		params[PATTERNCOLORINDICES]->SetVecValue( 1, 2, 3, 4 );
	}

	if ( !params[PATTERNREPLACEINDEX]->IsDefined() )
	{
		params[PATTERNREPLACEINDEX]->SetIntValue( 1 );
	}

	if (!params[NOISE]->IsDefined())
	{
		params[NOISE]->SetStringValue("models/weapons/customization/materials/noise");
	}

	if (!params[PATTERNPHONGFACTOR]->IsDefined())
	{
		params[PATTERNPHONGFACTOR]->SetFloatValue(0.0f);
	}

	if (!params[PATTERNPAINTTHICKNESS]->IsDefined())
	{
		params[PATTERNPAINTTHICKNESS]->SetFloatValue(0.0f);
	}

	if (!params[FLIPFIXUP]->IsDefined())
	{
		params[FLIPFIXUP]->SetFloatValue(1.0f);
	}
}

SHADER_FALLBACK
{
	return 0;
}

SHADER_INIT
{
	if ( params[BUMPMAP]->IsDefined() )
	{
		LoadTexture( BUMPMAP );
	}

	if ( params[MASKS1]->IsDefined() )
	{
		LoadTexture( MASKS1 );
	}

	if ( params[MATERIALMASK]->IsDefined() )
	{
		LoadTexture( MATERIALMASK );
	}

	if ( params[AO]->IsDefined() )
	{
		LoadTexture( AO );
	}

	if ( params[GRUNGE]->IsDefined() )
	{
		LoadTexture( GRUNGE );
	}

	if ( params[DETAIL]->IsDefined() )
	{
		LoadTexture( DETAIL );
	}

	if ( params[DETAILNORMAL]->IsDefined() )
	{
		LoadTexture( DETAILNORMAL );
	}

	if ( params[PATTERN]->IsDefined() )
	{
		LoadTexture( PATTERN );
	}

	LoadTexture( NOISE );
}

SHADER_DRAW
{
	bool bHasBumpMap = IsTextureSet( BUMPMAP, params );
	bool bHasMasks1 = IsTextureSet( MASKS1, params );

	// Phong mask uses bump alpha by default, but can use base alpha if specified
	bool bBaseAlphaPhongMask = params[BASEALPHAPHONGMASK]->IsDefined() && ( params[BASEALPHAPHONGMASK]->GetIntValue() > 0 );
	// Envmap uses same mask as spec
	bool bBaseAlphaEnvMask = bBaseAlphaPhongMask;
	bool bBumpAlphaEnvMask = !bBaseAlphaPhongMask;
	// Unless we've specified it should be different.
			bBaseAlphaEnvMask = ( bBaseAlphaEnvMask || ( ( params[BASEALPHAENVMASK]->IsDefined() && ( params[BASEALPHAENVMASK]->GetIntValue() > 0 ) ) ) );
			bBumpAlphaEnvMask = ( params[BUMPALPHAENVMASK]->IsDefined() && ( params[BUMPALPHAENVMASK]->GetIntValue() > 0 ) );
	// Can't have both.
			bBaseAlphaEnvMask = bBaseAlphaEnvMask && !bBumpAlphaEnvMask;

	bool bGenerateNormalMap = bHasBumpMap;
	bool bGenerateMasks1 = !bHasBumpMap && bHasMasks1;
	bool bGenerateBaseTexture = !bGenerateNormalMap && !bGenerateMasks1;
		
	bool bPattern = ( bGenerateBaseTexture || bGenerateNormalMap ) && IsTextureSet( PATTERN, params );
	bool bHasCustomPaint = bGenerateBaseTexture && params[CUSTOMPAINTJOB]->IsDefined() && ( params[CUSTOMPAINTJOB]->GetIntValue() > 0 );
	bool bHasAlloverPaint = bGenerateBaseTexture && params[ALLOVERPAINTJOB]->IsDefined() && ( params[ALLOVERPAINTJOB]->GetIntValue() > 0 );

	bool bHasAO = !bGenerateMasks1 && IsTextureSet( AO, params );
	bool bHasMaterialMask = IsTextureSet( MATERIALMASK, params );
	bool bHasGrunge = !bGenerateMasks1 && IsTextureSet( GRUNGE, params );
	bool bHasDetail = !bGenerateMasks1 && IsTextureSet( DETAIL, params );
	bool bHasDetailNormal = bGenerateNormalMap && IsTextureSet( DETAILNORMAL, params );

	SHADOW_STATE
	{
		SetInitialShadowState();

		unsigned int flags = VERTEX_POSITION;

		int nTexCoordCount = 1;
		pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, 0 );

		// Vertex Shader
		DECLARE_STATIC_VERTEX_SHADER( customcharacter_vs30 );
		SET_STATIC_VERTEX_SHADER( customcharacter_vs30 );

		// Pixel Shader
		DECLARE_STATIC_PIXEL_SHADER( customcharacter_ps30 );
		SET_STATIC_PIXEL_SHADER_COMBO( GENERATEBASETEXTURE, bGenerateBaseTexture );
		SET_STATIC_PIXEL_SHADER_COMBO( GENERATENORMAL, bGenerateNormalMap );
		SET_STATIC_PIXEL_SHADER_COMBO( GENERATEMASKS1, bGenerateMasks1 );
		SET_STATIC_PIXEL_SHADER_COMBO( CHEAPFILTERING, !bGenerateBaseTexture && !bGenerateNormalMap );
		SET_STATIC_PIXEL_SHADER_COMBO( BASEALPHAPHONGMASK, bBaseAlphaPhongMask );
		SET_STATIC_PIXEL_SHADER_COMBO( BASEALPHAENVMASK, bBaseAlphaEnvMask );
		SET_STATIC_PIXEL_SHADER_COMBO( BUMPALPHAENVMASK, bBumpAlphaEnvMask );
		SET_STATIC_PIXEL_SHADER_COMBO( USEPATTERN, bPattern + bHasCustomPaint + ( 2 * bHasAlloverPaint ) );
		SET_STATIC_PIXEL_SHADER( customcharacter_ps30 );

		if ( bHasBumpMap )
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
		}
		if ( bHasMasks1 )
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
		}
		if ( bHasMaterialMask )
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER9, true );
		}
		if ( bHasAO )
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER10, true );
		}
		if ( bHasGrunge )
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER11, true );
		}
		if ( bHasDetail )
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER12, true );
		}
		if ( bHasDetailNormal )
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER13, true );
		}
		pShaderShadow->EnableTexture( SHADER_SAMPLER15, true );

		pShaderShadow->EnableAlphaWrites( true );
		pShaderShadow->EnableSRGBWrite( bGenerateBaseTexture );

		}
		DYNAMIC_STATE
		{
			pShaderAPI->SetDefaultState();

			if ( bHasBumpMap )
			{
				BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, BUMPMAP, -1 );
			}
			if ( bHasMasks1 )
			{
				BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, MASKS1, -1 );
			}
			if ( bHasMaterialMask )
			{
				BindTexture( SHADER_SAMPLER9, TEXTURE_BINDFLAGS_NONE, MATERIALMASK, -1 );
			}
			if ( bHasAO )
			{
				BindTexture( SHADER_SAMPLER10, TEXTURE_BINDFLAGS_NONE, AO, -1 );
			}
			if ( bHasGrunge )
			{
				BindTexture( SHADER_SAMPLER11, TEXTURE_BINDFLAGS_SRGBREAD, GRUNGE, -1 );
			}
			if ( bHasDetail )
			{
				BindTexture( SHADER_SAMPLER12, TEXTURE_BINDFLAGS_NONE, DETAIL, -1 );
			}
			if ( bHasDetailNormal )
			{
				BindTexture( SHADER_SAMPLER13, TEXTURE_BINDFLAGS_NONE, DETAILNORMAL, -1 );
			}
			if ( bPattern )
			{
				BindTexture( SHADER_SAMPLER14, bHasCustomPaint ? TEXTURE_BINDFLAGS_SRGBREAD : TEXTURE_BINDFLAGS_NONE, PATTERN, -1 );
			}
			BindTexture(SHADER_SAMPLER15, TEXTURE_BINDFLAGS_NONE, NOISE, -1);

			// Vertex Shader
			DECLARE_DYNAMIC_VERTEX_SHADER( customcharacter_vs30 );
			SET_DYNAMIC_VERTEX_SHADER( customcharacter_vs30 );

			// Pixel Shader
			DECLARE_DYNAMIC_PIXEL_SHADER( customcharacter_ps30 );
			SET_DYNAMIC_PIXEL_SHADER( customcharacter_ps30 );

			float vParams[4] = { 0, 0, 0, 0 };

			params[DETAILSCALE]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(108, vParams, 1);

			params[DETAILPHONGBOOST]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(32, vParams, 1);

			params[DAMAGEDETAILPHONGBOOST]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(100, vParams, 1);

			params[DETAILENVBOOST]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(34, vParams, 1);

			params[DAMAGEDETAILENVBOOST]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(35, vParams, 1);

			params[DETAILPHONGALBEDOTINT]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(36, vParams, 1);

			params[DETAILWARPINDEX]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(37, vParams, 1);

			params[DETAILMETALNESS]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(38, vParams, 1);

			params[DETAILNORMALDEPTH]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(39, vParams, 1);

			params[DAMAGENORMALEDGEDEPTH]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(40, vParams, 1);

			float vParams2[2] = { 0, 0 };
			params[DAMAGELEVELS1]->GetVecValue(vParams, 2);
			params[DAMAGELEVELS2]->GetVecValue(vParams2, 2);
			vParams[2] = vParams2[0];
			vParams[3] = vParams2[1];
			pShaderAPI->SetPixelShaderConstant(41, vParams, 1);

			params[DAMAGELEVELS3]->GetVecValue(vParams, 2);
			params[DAMAGELEVELS4]->GetVecValue(vParams2, 2);
			vParams[2] = vParams2[0];
			vParams[3] = vParams2[1];
			pShaderAPI->SetPixelShaderConstant(42, vParams, 1);

			SetPixelShaderTextureTransform(43, PATTERNTEXTURETRANSFORM); // 43-44

			params[PATTERNCOLORINDICES]->GetVecValue(vParams, 4);
			vParams[0] = clamp(vParams[0] - 1, 0, 7);
			vParams[1] = clamp(vParams[1] - 1, 0, 7);
			vParams[2] = clamp(vParams[2] - 1, 0, 7);
			vParams[3] = clamp(vParams[3] - 1, 0, 7);
			pShaderAPI->SetPixelShaderConstant(45, vParams, 1);

			vParams[0] = pow(params[WEARPROGRESS]->GetFloatValue(), params[WEAREXPONENT]->GetFloatValue());
			vParams[1] = params[PATTERNDETAILINFLUENCE]->GetFloatValue();
			if (bPattern)
			{
				vParams[2] = clamp(params[PATTERNREPLACEINDEX]->GetIntValue() - 1, 0, 7);
			}
			else
			{
				vParams[2] = -1;
			}
			vParams[3] = 1.0f / 1024.0f; // assuming texture size of 2048.  Should be dynamic?
			pShaderAPI->SetPixelShaderConstant(46, vParams, 1);

			float vParams3[3] = { 0, 0, 0 };
			params[PALETTECOLOR4]->GetVecValue(vParams3, 3);
			vParams3[0] = SrgbGammaToLinear(vParams3[0] / 255.0f);
			vParams3[1] = SrgbGammaToLinear(vParams3[1] / 255.0f);
			vParams3[2] = SrgbGammaToLinear(vParams3[2] / 255.0f);

			params[PALETTECOLOR1]->GetVecValue(vParams, 3);
			vParams[0] = SrgbGammaToLinear(vParams[0] / 255.0f);
			vParams[1] = SrgbGammaToLinear(vParams[1] / 255.0f);
			vParams[2] = SrgbGammaToLinear(vParams[2] / 255.0f);
			vParams[3] = vParams3[0];
			pShaderAPI->SetPixelShaderConstant(47, vParams, 1);

			params[PALETTECOLOR2]->GetVecValue(vParams, 3);
			vParams[0] = SrgbGammaToLinear(vParams[0] / 255.0f);
			vParams[1] = SrgbGammaToLinear(vParams[1] / 255.0f);
			vParams[2] = SrgbGammaToLinear(vParams[2] / 255.0f);
			vParams[3] = vParams3[1];
			pShaderAPI->SetPixelShaderConstant(48, vParams, 1);

			params[PALETTECOLOR3]->GetVecValue(vParams, 3);
			vParams[0] = SrgbGammaToLinear(vParams[0] / 255.0f);
			vParams[1] = SrgbGammaToLinear(vParams[1] / 255.0f);
			vParams[2] = SrgbGammaToLinear(vParams[2] / 255.0f);
			vParams[3] = vParams3[2];
			pShaderAPI->SetPixelShaderConstant(49, vParams, 1);

			params[PALETTECOLOR8]->GetVecValue(vParams3, 3);
			vParams3[0] = SrgbGammaToLinear(vParams3[0] / 255.0f);
			vParams3[1] = SrgbGammaToLinear(vParams3[1] / 255.0f);
			vParams3[2] = SrgbGammaToLinear(vParams3[2] / 255.0f);

			params[PALETTECOLOR5]->GetVecValue(vParams, 3);
			vParams[0] = SrgbGammaToLinear(vParams[0] / 255.0f);
			vParams[1] = SrgbGammaToLinear(vParams[1] / 255.0f);
			vParams[2] = SrgbGammaToLinear(vParams[2] / 255.0f);
			vParams[3] = vParams3[0];
			pShaderAPI->SetPixelShaderConstant(50, vParams, 1);

			params[PALETTECOLOR6]->GetVecValue(vParams, 3);
			vParams[0] = SrgbGammaToLinear(vParams[0] / 255.0f);
			vParams[1] = SrgbGammaToLinear(vParams[1] / 255.0f);
			vParams[2] = SrgbGammaToLinear(vParams[2] / 255.0f);
			vParams[3] = vParams3[1];
			pShaderAPI->SetPixelShaderConstant(51, vParams, 1);

			params[PALETTECOLOR7]->GetVecValue(vParams, 3);
			vParams[0] = SrgbGammaToLinear(vParams[0] / 255.0f);
			vParams[1] = SrgbGammaToLinear(vParams[1] / 255.0f);
			vParams[2] = SrgbGammaToLinear(vParams[2] / 255.0f);
			vParams[3] = vParams3[2];
			pShaderAPI->SetPixelShaderConstant(52, vParams, 1);

			SetPixelShaderTextureTransform(53, GRUNGETEXTURETRANSFORM); // 53-54

			params[GRIME]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(55, vParams, 1);

			params[WEARDETAILPHONGBOOST]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(56, vParams, 1);

			params[WEARDETAILENVBOOST]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(57, vParams, 1);

			params[DAMAGEEDGEPHONGBOOST]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(58, vParams, 1);

			params[DAMAGEEDGEENVBOOST]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(59, vParams, 1);

			params[CURVATUREWEARBOOST]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(60, vParams, 1);

			params[CURVATUREWEARPOWER]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(61, vParams, 1);

			params[DAMAGEDETAILSATURATION]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(62, vParams, 1);

			params[DAMAGEGRUNGE]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(63, vParams, 1);

			params[DETAILGRUNGE]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(90, vParams, 1);

			vParams[0] = params[PATTERNPHONGFACTOR]->GetFloatValue();
			vParams[1] = params[PATTERNPAINTTHICKNESS]->GetFloatValue();
			vParams[2] = params[FLIPFIXUP]->GetFloatValue();
			vParams[3] = 0;
			pShaderAPI->SetPixelShaderConstant(91, vParams, 1);

			params[DAMAGEDETAILBRIGHTNESSADJUSTMENT]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(92, vParams, 1);

			params[GRUNGEMAX]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(93, vParams, 1);

			// c68-89 used by csms

			params[GRIMESATURATION]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(94, vParams, 1);

			params[GRIMEBRIGHTNESSADJUSTMENT]->GetVecValue(vParams, 4);
			pShaderAPI->SetPixelShaderConstant(95, vParams, 1);

			SetPixelShaderTextureTransform( 96, GRUNGETEXTUREROTATION );
			SetPixelShaderTextureTransform( 98, PATTERNTEXTUREROTATION );
		}
		Draw();
	}

END_SHADER