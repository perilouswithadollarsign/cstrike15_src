//===== Copyright (c) 1996-2007, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
// This is what all vs/ps (dx8+) shaders inherit from.
//===========================================================================//
#if !defined(_STATIC_LINKED) || defined(STDSHADER_DX9_DLL_EXPORT)

#include "cpp_shader_constant_register_map.h"
#include "BaseVSShader.h"
#include "mathlib/vmatrix.h"
#include "mathlib/bumpvects.h"
#include "convar.h"
#include "tier0/icommandline.h"

#ifdef HDR
#include "vertexlit_and_unlit_generic_hdr_ps20.inc"
#include "vertexlit_and_unlit_generic_hdr_ps20b.inc"
#endif

#if !defined( _X360 ) && !defined( _PS3 )
#include "lightmappedgeneric_flashlight_vs30.inc"
#include "flashlight_ps30.inc"
#endif

#include "lightmappedgeneric_flashlight_vs20.inc"
#include "flashlight_ps20.inc"
#include "flashlight_ps20b.inc"
#include "vertexlitgeneric_flashlight_vs20.inc"

#include "shaderapifast.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// NOTE: This is externed in BaseVSShader.h so it needs to be here
#if defined( _PS3 ) || defined( OSX )
extern ConVar r_flashlightbrightness;
#else
ConVar r_flashlightbrightness( "r_flashlightbrightness", "0.25", FCVAR_CHEAT );
#endif

// These functions are to be called from the shaders.

//-----------------------------------------------------------------------------
// Pixel and vertex shader constants....
//-----------------------------------------------------------------------------
void CBaseVSShader::SetPixelShaderConstant( int pixelReg, int constantVar, int constantVar2 )
{
	Assert( !IsSnapshotting() );
	if ((!s_ppParams) || (constantVar == -1) || (constantVar2 == -1))
		return;

	IMaterialVar* pPixelVar = s_ppParams[constantVar];
	Assert( pPixelVar );
	IMaterialVar* pPixelVar2 = s_ppParams[constantVar2];
	Assert( pPixelVar2 );

	float val[4];
	if (pPixelVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
	{
		pPixelVar->GetVecValue( val, 3 );
	}
	else
	{
		val[0] = val[1] = val[2] = pPixelVar->GetFloatValue();
	}

	val[3] = pPixelVar2->GetFloatValue();
	s_pShaderAPI->SetPixelShaderConstant( pixelReg, val );	
}

void CBaseVSShader::SetPixelShaderConstantGammaToLinear( int pixelReg, int constantVar, int constantVar2 )
{
	Assert( !IsSnapshotting() );
	if ((!s_ppParams) || (constantVar == -1) || (constantVar2 == -1))
		return;

	IMaterialVar* pPixelVar = s_ppParams[constantVar];
	Assert( pPixelVar );
	IMaterialVar* pPixelVar2 = s_ppParams[constantVar2];
	Assert( pPixelVar2 );

	float val[4];
	if (pPixelVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
	{
		pPixelVar->GetVecValue( val, 3 );
	}
	else
	{
		val[0] = val[1] = val[2] = pPixelVar->GetFloatValue();
	}

	val[3] = pPixelVar2->GetFloatValue();
	val[0] = val[0] > 1.0f ? val[0] : GammaToLinear( val[0] );
	val[1] = val[1] > 1.0f ? val[1] : GammaToLinear( val[1] );
	val[2] = val[2] > 1.0f ? val[2] : GammaToLinear( val[2] );

	s_pShaderAPI->SetPixelShaderConstant( pixelReg, val );	
}

void CBaseVSShader::SetPixelShaderConstant_W( int pixelReg, int constantVar, float fWValue )
{
	Assert( !IsSnapshotting() );
	if ((!s_ppParams) || (constantVar == -1))
		return;

	IMaterialVar* pPixelVar = s_ppParams[constantVar];
	Assert( pPixelVar );

	float val[4];
	if (pPixelVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
		pPixelVar->GetVecValue( val, 4 );
	else
		val[0] = val[1] = val[2] = val[3] = pPixelVar->GetFloatValue();
	val[3]=fWValue;
	s_pShaderAPI->SetPixelShaderConstant( pixelReg, val );	
}

void CBaseVSShader::SetPixelShaderConstant( int pixelReg, int constantVar )
{
	Assert( !IsSnapshotting() );
	if ((!s_ppParams) || (constantVar == -1))
		return;

	IMaterialVar* pPixelVar = s_ppParams[constantVar];
	Assert( pPixelVar );

	float val[4];
	if (pPixelVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
		pPixelVar->GetVecValue( val, 4 );
	else
		val[0] = val[1] = val[2] = val[3] = pPixelVar->GetFloatValue();
	s_pShaderAPI->SetPixelShaderConstant( pixelReg, val );	
}

void CBaseVSShader::SetPixelShaderConstantGammaToLinear( int pixelReg, int constantVar )
{
	Assert( !IsSnapshotting() );
	if ((!s_ppParams) || (constantVar == -1))
		return;

	IMaterialVar* pPixelVar = s_ppParams[constantVar];
	Assert( pPixelVar );

	float val[4];
	if (pPixelVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
		pPixelVar->GetVecValue( val, 4 );
	else
		val[0] = val[1] = val[2] = val[3] = pPixelVar->GetFloatValue();

	val[0] = val[0] > 1.0f ? val[0] : GammaToLinear( val[0] );
	val[1] = val[1] > 1.0f ? val[1] : GammaToLinear( val[1] );
	val[2] = val[2] > 1.0f ? val[2] : GammaToLinear( val[2] );

	s_pShaderAPI->SetPixelShaderConstant( pixelReg, val );	
}

void CBaseVSShader::SetVertexShaderConstantGammaToLinear( int var, float const* pVec, int numConst, bool bForce )
{
	int i;
	for( i = 0; i < numConst; i++ )
	{
		float vec[4];
		vec[0] = pVec[i*4+0] > 1.0f ? pVec[i*4+0] : GammaToLinear( pVec[i*4+0] );
		vec[1] = pVec[i*4+1] > 1.0f ? pVec[i*4+1] : GammaToLinear( pVec[i*4+1] );
		vec[2] = pVec[i*4+2] > 1.0f ? pVec[i*4+2] : GammaToLinear( pVec[i*4+2] );
		vec[3] = pVec[i*4+3];

		s_pShaderAPI->SetVertexShaderConstant( var + i, vec, 1, bForce );
	}
}

void CBaseVSShader::SetPixelShaderConstantGammaToLinear( int var, float const* pVec, int numConst, bool bForce )
{
	int i;
	for( i = 0; i < numConst; i++ )
	{
		float vec[4];
		vec[0] = pVec[i*4+0] > 1.0f ? pVec[i*4+0] : GammaToLinear( pVec[i*4+0] );
		vec[1] = pVec[i*4+1] > 1.0f ? pVec[i*4+1] : GammaToLinear( pVec[i*4+1] );
		vec[2] = pVec[i*4+2] > 1.0f ? pVec[i*4+2] : GammaToLinear( pVec[i*4+2] );

		vec[3] = pVec[i*4+3];

		s_pShaderAPI->SetPixelShaderConstant( var + i, vec, 1, bForce );
	}
}

// GR - special version with fix for const/lerp issue
void CBaseVSShader::SetPixelShaderConstantFudge( int pixelReg, int constantVar )
{
	Assert( !IsSnapshotting() );
	if ((!s_ppParams) || (constantVar == -1))
		return;

	IMaterialVar* pPixelVar = s_ppParams[constantVar];
	Assert( pPixelVar );

	float val[4];
	if (pPixelVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
	{
		pPixelVar->GetVecValue( val, 4 );
		val[0] = val[0] * 0.992f + 0.0078f;
		val[1] = val[1] * 0.992f + 0.0078f;
		val[2] = val[2] * 0.992f + 0.0078f;
		val[3] = val[3] * 0.992f + 0.0078f;
	}
	else
		val[0] = val[1] = val[2] = val[3] = pPixelVar->GetFloatValue() * 0.992f + 0.0078f;
	s_pShaderAPI->SetPixelShaderConstant( pixelReg, val );	
}

void CBaseVSShader::SetVertexShaderConstant( int vertexReg, int constantVar )
{
	Assert( !IsSnapshotting() );
	if ((!s_ppParams) || (constantVar == -1))
		return;

	IMaterialVar* pVertexVar = s_ppParams[constantVar];
	Assert( pVertexVar );

	float val[4];
	if (pVertexVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
		pVertexVar->GetVecValue( val, 4 );
	else
		val[0] = val[1] = val[2] = val[3] = pVertexVar->GetFloatValue();
	s_pShaderAPI->SetVertexShaderConstant( vertexReg, val );	
}


//-----------------------------------------------------------------------------
// Sets vertex shader texture transforms
//-----------------------------------------------------------------------------
void CBaseVSShader::SetVertexShaderTextureTranslation( int vertexReg, int translationVar )
{
	float offset[2] = {0, 0};

	IMaterialVar* pTranslationVar = s_ppParams[translationVar];
	if (pTranslationVar)
	{
		if (pTranslationVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
			pTranslationVar->GetVecValue( offset, 2 );
		else
			offset[0] = offset[1] = pTranslationVar->GetFloatValue();
	}

	Vector4D translation[2];
	translation[0].Init( 1.0f, 0.0f, 0.0f, offset[0] );
	translation[1].Init( 0.0f, 1.0f, 0.0f, offset[1] );
	s_pShaderAPI->SetVertexShaderConstant( vertexReg, translation[0].Base(), 2 ); 
}

void CBaseVSShader::SetVertexShaderTextureScale( int vertexReg, int scaleVar )
{
	float scale[2] = {1, 1};

	IMaterialVar* pScaleVar = s_ppParams[scaleVar];
	if (pScaleVar)
	{
		if (pScaleVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
			pScaleVar->GetVecValue( scale, 2 );
		else if (pScaleVar->IsDefined())
			scale[0] = scale[1] = pScaleVar->GetFloatValue();
	}

	Vector4D scaleMatrix[2];
	scaleMatrix[0].Init( scale[0], 0.0f, 0.0f, 0.0f );
	scaleMatrix[1].Init( 0.0f, scale[1], 0.0f, 0.0f );
	s_pShaderAPI->SetVertexShaderConstant( vertexReg, scaleMatrix[0].Base(), 2 ); 
}

void CBaseVSShader::SetVertexShaderTextureTransform( int vertexReg, int transformVar )
{
	Vector4D transformation[2];
	IMaterialVar* pTransformationVar = s_ppParams[transformVar];
	if (pTransformationVar && (pTransformationVar->GetType() == MATERIAL_VAR_TYPE_MATRIX))
	{
		const VMatrix &mat = pTransformationVar->GetMatrixValue();
		transformation[0].Init( mat[0][0], mat[0][1], mat[0][2], mat[0][3] );
		transformation[1].Init( mat[1][0], mat[1][1], mat[1][2], mat[1][3] );
	}
	else
	{
		transformation[0].Init( 1.0f, 0.0f, 0.0f, 0.0f );
		transformation[1].Init( 0.0f, 1.0f, 0.0f, 0.0f );
	}
	s_pShaderAPI->SetVertexShaderConstant( vertexReg, transformation[0].Base(), 2 ); 
}

void CBaseVSShader::SetVertexShaderTextureScaledTransform( int vertexReg, int transformVar, int scaleVar )
{
	Vector4D transformation[2];
	IMaterialVar* pTransformationVar = s_ppParams[transformVar];
	if (pTransformationVar && (pTransformationVar->GetType() == MATERIAL_VAR_TYPE_MATRIX))
	{
		const VMatrix &mat = pTransformationVar->GetMatrixValue();
		transformation[0].Init( mat[0][0], mat[0][1], mat[0][2], mat[0][3] );
		transformation[1].Init( mat[1][0], mat[1][1], mat[1][2], mat[1][3] );
	}
	else
	{
		transformation[0].Init( 1.0f, 0.0f, 0.0f, 0.0f );
		transformation[1].Init( 0.0f, 1.0f, 0.0f, 0.0f );
	}

	Vector2D scale( 1, 1 );
	IMaterialVar* pScaleVar = s_ppParams[scaleVar];
	if (pScaleVar)
	{
		if (pScaleVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
			pScaleVar->GetVecValue( scale.Base(), 2 );
		else if (pScaleVar->IsDefined())
			scale[0] = scale[1] = pScaleVar->GetFloatValue();
	}

	// Apply the scaling
	transformation[0][0] *= scale[0];
	transformation[0][1] *= scale[1];
	transformation[1][0] *= scale[0];
	transformation[1][1] *= scale[1];
	transformation[0][3] *= scale[0];
	transformation[1][3] *= scale[1];
	s_pShaderAPI->SetVertexShaderConstant( vertexReg, transformation[0].Base(), 2 ); 
}


//-----------------------------------------------------------------------------
// Sets pixel shader texture transforms
//-----------------------------------------------------------------------------
void CBaseVSShader::SetPixelShaderTextureTranslation( int pixelReg, int translationVar )
{
	float offset[2] = {0, 0};

	IMaterialVar* pTranslationVar = s_ppParams[translationVar];
	if (pTranslationVar)
	{
		if (pTranslationVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
			pTranslationVar->GetVecValue( offset, 2 );
		else
			offset[0] = offset[1] = pTranslationVar->GetFloatValue();
	}

	Vector4D translation[2];
	translation[0].Init( 1.0f, 0.0f, 0.0f, offset[0] );
	translation[1].Init( 0.0f, 1.0f, 0.0f, offset[1] );
	s_pShaderAPI->SetPixelShaderConstant( pixelReg, translation[0].Base(), 2 ); 
}

void CBaseVSShader::SetPixelShaderTextureScale( int pixelReg, int scaleVar )
{
	float scale[2] = {1, 1};

	IMaterialVar* pScaleVar = s_ppParams[scaleVar];
	if (pScaleVar)
	{
		if (pScaleVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
			pScaleVar->GetVecValue( scale, 2 );
		else if (pScaleVar->IsDefined())
			scale[0] = scale[1] = pScaleVar->GetFloatValue();
	}

	Vector4D scaleMatrix[2];
	scaleMatrix[0].Init( scale[0], 0.0f, 0.0f, 0.0f );
	scaleMatrix[1].Init( 0.0f, scale[1], 0.0f, 0.0f );
	s_pShaderAPI->SetPixelShaderConstant( pixelReg, scaleMatrix[0].Base(), 2 ); 
}

void CBaseVSShader::SetPixelShaderTextureTransform( int pixelReg, int transformVar )
{
	Vector4D transformation[2];
	IMaterialVar* pTransformationVar = s_ppParams[transformVar];
	if (pTransformationVar && (pTransformationVar->GetType() == MATERIAL_VAR_TYPE_MATRIX))
	{
		const VMatrix &mat = pTransformationVar->GetMatrixValue();
		transformation[0].Init( mat[0][0], mat[0][1], mat[0][2], mat[0][3] );
		transformation[1].Init( mat[1][0], mat[1][1], mat[1][2], mat[1][3] );
	}
	else
	{
		transformation[0].Init( 1.0f, 0.0f, 0.0f, 0.0f );
		transformation[1].Init( 0.0f, 1.0f, 0.0f, 0.0f );
	}
	s_pShaderAPI->SetPixelShaderConstant( pixelReg, transformation[0].Base(), 2 ); 
}

void CBaseVSShader::SetPixelShaderTextureScaledTransform( int pixelReg, int transformVar, int scaleVar )
{
	Vector4D transformation[2];
	IMaterialVar* pTransformationVar = s_ppParams[transformVar];
	if (pTransformationVar && (pTransformationVar->GetType() == MATERIAL_VAR_TYPE_MATRIX))
	{
		const VMatrix &mat = pTransformationVar->GetMatrixValue();
		transformation[0].Init( mat[0][0], mat[0][1], mat[0][2], mat[0][3] );
		transformation[1].Init( mat[1][0], mat[1][1], mat[1][2], mat[1][3] );
	}
	else
	{
		transformation[0].Init( 1.0f, 0.0f, 0.0f, 0.0f );
		transformation[1].Init( 0.0f, 1.0f, 0.0f, 0.0f );
	}

	Vector2D scale( 1, 1 );
	IMaterialVar* pScaleVar = s_ppParams[scaleVar];
	if (pScaleVar)
	{
		if (pScaleVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
			pScaleVar->GetVecValue( scale.Base(), 2 );
		else if (pScaleVar->IsDefined())
			scale[0] = scale[1] = pScaleVar->GetFloatValue();
	}

	// Apply the scaling
	transformation[0][0] *= scale[0];
	transformation[0][1] *= scale[1];
	transformation[1][0] *= scale[0];
	transformation[1][1] *= scale[1];
	transformation[0][3] *= scale[0];
	transformation[1][3] *= scale[1];
	s_pShaderAPI->SetPixelShaderConstant( pixelReg, transformation[0].Base(), 2 ); 
}


//-----------------------------------------------------------------------------
// Moves a matrix into vertex shader constants 
//-----------------------------------------------------------------------------
void CBaseVSShader::SetVertexShaderMatrix3x4( int vertexReg, int matrixVar )
{
	IMaterialVar* pTranslationVar = s_ppParams[matrixVar];
	if (pTranslationVar)
	{
		s_pShaderAPI->SetVertexShaderConstant( vertexReg, &pTranslationVar->GetMatrixValue( )[0][0], 3 ); 
	}
	else
	{
		VMatrix matrix;
		MatrixSetIdentity( matrix );
		s_pShaderAPI->SetVertexShaderConstant( vertexReg, &matrix[0][0], 3 ); 
	}
}

void CBaseVSShader::SetVertexShaderMatrix4x4( int vertexReg, int matrixVar )
{
	IMaterialVar* pTranslationVar = s_ppParams[matrixVar];
	if (pTranslationVar)
	{
#ifdef _PS3
		VMatrix transpose = pTranslationVar->GetMatrixValue().Transpose();
		s_pShaderAPI->SetVertexShaderConstant( vertexReg, &transpose[0][0], 4 ); 
#else // _PS3
		s_pShaderAPI->SetVertexShaderConstant( vertexReg, &pTranslationVar->GetMatrixValue( )[0][0], 4 ); 
#endif // !_PS3
	}
	else
	{
		VMatrix matrix;
		MatrixSetIdentity( matrix );
		s_pShaderAPI->SetVertexShaderConstant( vertexReg, &matrix[0][0], 4 ); 
	}
}


//-----------------------------------------------------------------------------
// Loads the view matrix into vertex shader constants
//-----------------------------------------------------------------------------
void CBaseVSShader::LoadViewMatrixIntoVertexShaderConstant( int vertexReg )
{
	VMatrix mat, transpose;
	s_pShaderAPI->GetMatrix( MATERIAL_VIEW, mat.m[0] );

#ifdef _PS3
	transpose = mat;
#else // _PS3
	MatrixTranspose( mat, transpose );
#endif // !_PS3
	s_pShaderAPI->SetVertexShaderConstant( vertexReg, transpose.m[0], 3 );
}


//-----------------------------------------------------------------------------
// Loads the projection matrix into pixel shader constants
//-----------------------------------------------------------------------------
void CBaseVSShader::LoadProjectionMatrixIntoVertexShaderConstant( int vertexReg )
{
	VMatrix mat, transpose;
	s_pShaderAPI->GetActualProjectionMatrix( mat.m[0] );

#ifdef _PS3
	transpose = mat;
#else // _PS3
	MatrixTranspose( mat, transpose );
#endif // !_PS3
	s_pShaderAPI->SetVertexShaderConstant( vertexReg, transpose.m[0], 4 );
}


//-----------------------------------------------------------------------------
// Loads the model * view matrix into pixel shader constants
//-----------------------------------------------------------------------------
void CBaseVSShader::LoadModelViewMatrixIntoVertexShaderConstant( int vertexReg )
{
	VMatrix view, model, modelView, transpose;
	s_pShaderAPI->GetMatrix( MATERIAL_MODEL, model.m[0] );

	MatrixTranspose( model, model );

	s_pShaderAPI->GetMatrix( MATERIAL_VIEW, view.m[0] );

	MatrixTranspose( view, view );

	MatrixMultiply( view, model, modelView );

	s_pShaderAPI->SetVertexShaderConstant( vertexReg, modelView.m[0], 3 );
}


//-----------------------------------------------------------------------------
// Loads bump lightmap coordinates into the pixel shader
//-----------------------------------------------------------------------------
void CBaseVSShader::LoadBumpLightmapCoordinateAxes_PixelShader( int pixelReg )
{
	Vector4D basis[3];
	for (int i = 0; i < 3; ++i)
	{
		memcpy( &basis[i], &g_localBumpBasis[i], 3 * sizeof(float) );
		basis[i][3] = 0.0f;
	}
	s_pShaderAPI->SetPixelShaderConstant( pixelReg, (float*)basis, 3 );
}


//-----------------------------------------------------------------------------
// Loads bump lightmap coordinates into the pixel shader
//-----------------------------------------------------------------------------
void CBaseVSShader::LoadBumpLightmapCoordinateAxes_VertexShader( int vertexReg )
{
	Vector4D basis[3];

	// transpose
	int i;
	for (i = 0; i < 3; ++i)
	{
		basis[i][0] = g_localBumpBasis[0][i];
		basis[i][1] = g_localBumpBasis[1][i];
		basis[i][2] = g_localBumpBasis[2][i];
		basis[i][3] = 0.0f;
	}
	s_pShaderAPI->SetVertexShaderConstant( vertexReg, (float*)basis, 3 );
	for (i = 0; i < 3; ++i)
	{
		memcpy( &basis[i], &g_localBumpBasis[i], 3 * sizeof(float) );
		basis[i][3] = 0.0f;
	}
	s_pShaderAPI->SetVertexShaderConstant( vertexReg + 3, (float*)basis, 3 );
}


//-----------------------------------------------------------------------------
// Helper methods for pixel shader overbrighting
//-----------------------------------------------------------------------------
void CBaseVSShader::EnablePixelShaderOverbright( int reg, bool bEnable, bool bDivideByTwo )
{
	// can't have other overbright values with pixel shaders as it stands.
	float v[4];
	if( bEnable )
	{
		v[0] = v[1] = v[2] = v[3] = bDivideByTwo ? OVERBRIGHT / 2.0f : OVERBRIGHT;
	}
	else
	{
		v[0] = v[1] = v[2] = v[3] = bDivideByTwo ? 1.0f / 2.0f : 1.0f;
	}
	s_pShaderAPI->SetPixelShaderConstant( reg, v, 1 );
}

//-----------------------------------------------------------------------------
// Converts a color + alpha into a vector4
//-----------------------------------------------------------------------------
void CBaseVSShader::ColorVarsToVector( int colorVar, int alphaVar, Vector4D &color )
{
	color.Init( 1.0, 1.0, 1.0, 1.0 ); 
	if ( colorVar != -1 )
	{
		IMaterialVar* pColorVar = s_ppParams[colorVar];
		if ( pColorVar->GetType() == MATERIAL_VAR_TYPE_VECTOR )
		{
			pColorVar->GetVecValue( color.Base(), 3 );
		}
		else
		{
			color[0] = color[1] = color[2] = pColorVar->GetFloatValue();
		}
	}
	if ( alphaVar != -1 )
	{
		float flAlpha = s_ppParams[alphaVar]->GetFloatValue();
		color[3] = clamp( flAlpha, 0.0f, 1.0f );
	}
}

#ifdef _DEBUG
ConVar mat_envmaptintoverride( "mat_envmaptintoverride", "-1" );
ConVar mat_envmaptintscale( "mat_envmaptintscale", "-1" );
#endif

//-----------------------------------------------------------------------------
// Helpers for dealing with envmap tint
//-----------------------------------------------------------------------------
// set alphaVar to -1 to ignore it.
void CBaseVSShader::SetEnvMapTintPixelShaderDynamicState( int pixelReg, int tintVar, int alphaVar, bool bConvertFromGammaToLinear )
{
	float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	if( g_pConfig->bShowSpecular && g_pConfig->nFullbright != 2 )
	{
		IMaterialVar* pAlphaVar = NULL;
		if( alphaVar >= 0 )
		{
			pAlphaVar = s_ppParams[alphaVar];
		}
		if( pAlphaVar )
		{
			color[3] = pAlphaVar->GetFloatValue();
		}

		IMaterialVar* pTintVar = s_ppParams[tintVar];
#ifdef _DEBUG
		pTintVar->GetVecValue( color, 3 );

		float envmapTintOverride = mat_envmaptintoverride.GetFloat();
		float envmapTintScaleOverride = mat_envmaptintscale.GetFloat();

		if( envmapTintOverride != -1.0f )
		{
			color[0] = color[1] = color[2] = envmapTintOverride;
		}
		if( envmapTintScaleOverride != -1.0f )
		{
			color[0] *= envmapTintScaleOverride;
			color[1] *= envmapTintScaleOverride;
			color[2] *= envmapTintScaleOverride;
		}

		if( bConvertFromGammaToLinear )
		{
			color[0] = color[0] > 1.0f ? color[0] : GammaToLinear( color[0] );
			color[1] = color[1] > 1.0f ? color[1] : GammaToLinear( color[1] );
			color[2] = color[2] > 1.0f ? color[2] : GammaToLinear( color[2] );
		}
#else
		if( bConvertFromGammaToLinear )
		{
			pTintVar->GetLinearVecValue( color, 3 );
		}
		else
		{
			pTintVar->GetVecValue( color, 3 );
		}
#endif
	}
	else
	{
		color[0] = color[1] = color[2] = color[3] = 0.0f;
	}
	s_pShaderAPI->SetPixelShaderConstant( pixelReg, color, 1 );
}


//-----------------------------------------------------------------------------
// Sets up hw morphing state for the vertex shader
//-----------------------------------------------------------------------------
void CBaseVSShader::SetHWMorphVertexShaderState( int nDimConst, int nSubrectConst, VertexTextureSampler_t morphSampler )
{
#if !defined( _X360 ) && !defined( _PS3 )
	if ( !s_pShaderAPI->IsHWMorphingEnabled() )
		return;

	int nMorphWidth, nMorphHeight;
	s_pShaderAPI->GetStandardTextureDimensions( &nMorphWidth, &nMorphHeight, TEXTURE_MORPH_ACCUMULATOR );

	int nDim = s_pShaderAPI->GetIntRenderingParameter( INT_RENDERPARM_MORPH_ACCUMULATOR_4TUPLE_COUNT );
	float pMorphAccumSize[4] = { nMorphWidth, nMorphHeight, nDim, 0.0f };
	s_pShaderAPI->SetVertexShaderConstant( nDimConst, pMorphAccumSize );

	int nXOffset = s_pShaderAPI->GetIntRenderingParameter( INT_RENDERPARM_MORPH_ACCUMULATOR_X_OFFSET );
	int nYOffset = s_pShaderAPI->GetIntRenderingParameter( INT_RENDERPARM_MORPH_ACCUMULATOR_Y_OFFSET );
	int nWidth = s_pShaderAPI->GetIntRenderingParameter( INT_RENDERPARM_MORPH_ACCUMULATOR_SUBRECT_WIDTH );
	int nHeight = s_pShaderAPI->GetIntRenderingParameter( INT_RENDERPARM_MORPH_ACCUMULATOR_SUBRECT_HEIGHT );
	float pMorphAccumSubrect[4] = { nXOffset, nYOffset, nWidth, nHeight };
	s_pShaderAPI->SetVertexShaderConstant( nSubrectConst, pMorphAccumSubrect );

	s_pShaderAPI->BindStandardVertexTexture( morphSampler, TEXTURE_MORPH_ACCUMULATOR );
#endif
}


//-----------------------------------------------------------------------------
// GR - translucency query
//-----------------------------------------------------------------------------
BlendType_t CBaseVSShader::EvaluateBlendRequirements( int textureVar, bool isBaseTexture,
													  int detailTextureVar )
{
	// Either we've got a constant modulation
	bool isTranslucent = IsAlphaModulating();

	// Or we've got a vertex alpha
	isTranslucent = isTranslucent || (CurrentMaterialVarFlags() & MATERIAL_VAR_VERTEXALPHA);

	// Or we've got a texture alpha (for blending or alpha test)
	isTranslucent = isTranslucent || ( TextureIsTranslucent( textureVar, isBaseTexture ) &&
		                               !(CurrentMaterialVarFlags() & MATERIAL_VAR_ALPHATEST ) );

	if ( ( detailTextureVar != -1 ) && ( ! isTranslucent ) )
	{
		isTranslucent = TextureIsTranslucent( detailTextureVar, isBaseTexture );
	}

	if ( CurrentMaterialVarFlags() & MATERIAL_VAR_ADDITIVE )
	{	
		return isTranslucent ? BT_BLENDADD : BT_ADD;	// Additive
	}
	else
	{
		return isTranslucent ? BT_BLEND : BT_NONE;		// Normal blending
	}
}

void CBaseVSShader::SetFlashlightVertexShaderConstants( bool bBump, int bumpTransformVar, bool bDetail, int detailScaleVar, bool bSetTextureTransforms )
{
	Assert( !IsSnapshotting() );

	VMatrix worldToTexture;
	const FlashlightState_t &flashlightState = s_pShaderAPI->GetFlashlightState( worldToTexture );

	// Set the flashlight origin
	float pos[4];
	pos[0] = flashlightState.m_vecLightOrigin[0];
	pos[1] = flashlightState.m_vecLightOrigin[1];
	pos[2] = flashlightState.m_vecLightOrigin[2];
	pos[3] = 1.0f / ( ( 0.6f * flashlightState.m_FarZ ) - flashlightState.m_FarZ );		// DX8 needs this

	s_pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, pos, 1 );

	s_pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_1, worldToTexture.Base(), 4 );

	// Set the flashlight attenuation factors
	float atten[4];
	atten[0] = flashlightState.m_fConstantAtten;
	atten[1] = flashlightState.m_fLinearAtten;
	atten[2] = flashlightState.m_fQuadraticAtten;
	atten[3] = flashlightState.m_FarZAtten;
	s_pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_5, atten, 1 );

	if ( bDetail )
	{
		SetVertexShaderTextureScaledTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_8, BASETEXTURETRANSFORM, detailScaleVar );
	}

	if( bSetTextureTransforms )
	{
		SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6, BASETEXTURETRANSFORM );
		if( !bDetail && bBump && bumpTransformVar != -1 )
		{
			SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_8, bumpTransformVar ); // aliased on top of detail transform
		}
	}
}

void CBaseVSShader::DrawFlashlight_dx90( IMaterialVar** params, IShaderDynamicAPI *pShaderAPI, 
										IShaderShadow* pShaderShadow, DrawFlashlight_dx90_Vars_t &vars )
{
	bool bSFM = ( ToolsEnabled() && IsPlatformWindowsPC() && g_pHardwareConfig->SupportsPixelShaders_3_0() ) ? true : false;

	// FLASHLIGHTFIXME: hack . . need to fix the vertex shader so that it can deal with and without bumps for vertexlitgeneric
	if( !vars.m_bLightmappedGeneric )
	{
		vars.m_bBump = false;
	}
	bool bBump2 = vars.m_bWorldVertexTransition && vars.m_bBump && vars.m_nBumpmapVar2 != -1 && params[vars.m_nBumpmapVar2]->IsTexture();
	bool bSeamless = vars.m_fSeamlessScale != 0.0;
	bool bDetail = vars.m_bLightmappedGeneric && (vars.m_nDetailVar != -1) && params[vars.m_nDetailVar]->IsDefined() && (vars.m_nDetailScale != -1) && g_pConfig->UseDetailTexturing();

	int nDetailBlendMode = DETAIL_BLEND_MODE_RGB_EQUALS_BASE_x_DETAILx2;
	if ( bDetail )
	{
		nDetailBlendMode = GetIntParam( vars.m_nDetailTextureCombineMode, params );
		ITexture *pDetailTexture = params[vars.m_nDetailVar]->GetTextureValue();
		if ( pDetailTexture->GetFlags() & TEXTUREFLAGS_SSBUMP )
		{
			if ( vars.m_bBump )
				nDetailBlendMode = DETAIL_BLEND_MODE_SSBUMP_BUMP;	// ssbump
			else
				nDetailBlendMode = DETAIL_BLEND_MODE_SSBUMP_NOBUMP;	// ssbump_nobump
		}
	}
	
	if( pShaderShadow )
	{
		SetInitialShadowState();
		pShaderShadow->EnableDepthWrites( false );
		pShaderShadow->EnableAlphaWrites( false );

		// Alpha blend
		SetAdditiveBlendingShadowState( BASETEXTURE, true );

		// Alpha test
		pShaderShadow->EnableAlphaTest( IS_FLAG_SET( MATERIAL_VAR_ALPHATEST ) );
		if ( vars.m_nAlphaTestReference != -1 && params[vars.m_nAlphaTestReference]->GetFloatValue() > 0.0f )
		{
			pShaderShadow->AlphaFunc( SHADER_ALPHAFUNC_GEQUAL, params[vars.m_nAlphaTestReference]->GetFloatValue() );
		}

		// Spot sampler
		pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, true );

		// Base sampler
		pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, true );

		// Normalizing cubemap sampler
		pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );

		// Normalizing cubemap sampler2 or normal map sampler
		pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );

		// RandomRotation sampler
		pShaderShadow->EnableTexture( SHADER_SAMPLER5, true );

		// Flashlight depth sampler
		pShaderShadow->EnableTexture( SHADER_SAMPLER7, true );
		//pShaderShadow->SetShadowDepthFiltering( SHADER_SAMPLER7 );

		if( vars.m_bWorldVertexTransition )
		{
			// $basetexture2
			pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER4, true );
		}
		if( bBump2 )
		{
			// Normalmap2 sampler
			pShaderShadow->EnableTexture( SHADER_SAMPLER6, true );
		}
		if( bDetail )
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER8, true );				// detail sampler
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER8, IsSRGBDetailTexture( nDetailBlendMode ) );
		}

		pShaderShadow->EnableSRGBWrite( true );

		if( vars.m_bLightmappedGeneric )
		{
#if !defined( _X360 ) && !defined( _PS3 )
			if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_STATIC_VERTEX_SHADER( lightmappedgeneric_flashlight_vs30 );
				SET_STATIC_VERTEX_SHADER_COMBO( WORLDVERTEXTRANSITION, vars.m_bWorldVertexTransition );
				SET_STATIC_VERTEX_SHADER_COMBO( NORMALMAP, vars.m_bBump );
				SET_STATIC_VERTEX_SHADER_COMBO( SEAMLESS, bSeamless );
				SET_STATIC_VERTEX_SHADER_COMBO( DETAIL, bDetail );
				SET_STATIC_VERTEX_SHADER( lightmappedgeneric_flashlight_vs30 );
			}
			else
#endif
			{
				DECLARE_STATIC_VERTEX_SHADER( lightmappedgeneric_flashlight_vs20 );
				SET_STATIC_VERTEX_SHADER_COMBO( WORLDVERTEXTRANSITION, vars.m_bWorldVertexTransition );
				SET_STATIC_VERTEX_SHADER_COMBO( NORMALMAP, vars.m_bBump );
				SET_STATIC_VERTEX_SHADER_COMBO( SEAMLESS, bSeamless );
				SET_STATIC_VERTEX_SHADER_COMBO( DETAIL, bDetail );
				SET_STATIC_VERTEX_SHADER( lightmappedgeneric_flashlight_vs20 );
			}

			unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL;
			if( vars.m_bBump )
			{
				flags |= VERTEX_TANGENT_S | VERTEX_TANGENT_T;
			}
			int numTexCoords = 1;
			if( vars.m_bWorldVertexTransition )
			{
				flags |= VERTEX_COLOR;
				numTexCoords = 2; // need lightmap texcoords to get alpha.
			}
			pShaderShadow->VertexShaderVertexFormat( flags, numTexCoords, 0, 0 );
		}
		else
		{

			// Need a 3.0 vs here?

			DECLARE_STATIC_VERTEX_SHADER( vertexlitgeneric_flashlight_vs20 );
			SET_STATIC_VERTEX_SHADER_COMBO( TEETH, vars.m_bTeeth );
			SET_STATIC_VERTEX_SHADER( vertexlitgeneric_flashlight_vs20 );

			unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL;
			int numTexCoords = 1;
			pShaderShadow->VertexShaderVertexFormat( flags, numTexCoords, 0, vars.m_bBump ? 4 : 0 );
		}

		int nBumpMapVariant = 0;
		if ( vars.m_bBump )
		{
			nBumpMapVariant = ( vars.m_bSSBump ) ? 2 : 1;
		}

#if !defined( _X360 ) && !defined( _PS3 )
		if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
		{
			ShadowFilterMode_t nShadowFilterMode = g_pHardwareConfig->GetShadowFilterMode( false /* bForceLowQuality */, true /* bPS30 */ );

			DECLARE_STATIC_PIXEL_SHADER( flashlight_ps30 );
			SET_STATIC_PIXEL_SHADER_COMBO( SFM, bSFM );
			SET_STATIC_PIXEL_SHADER_COMBO( NORMALMAP, nBumpMapVariant );
			SET_STATIC_PIXEL_SHADER_COMBO( NORMALMAP2, bBump2 );
			SET_STATIC_PIXEL_SHADER_COMBO( WORLDVERTEXTRANSITION, vars.m_bWorldVertexTransition );
			SET_STATIC_PIXEL_SHADER_COMBO( SEAMLESS, bSeamless );
			SET_STATIC_PIXEL_SHADER_COMBO( DETAILTEXTURE, bDetail );
			SET_STATIC_PIXEL_SHADER_COMBO( DETAIL_BLEND_MODE, nDetailBlendMode );
			SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode );
			SET_STATIC_PIXEL_SHADER( flashlight_ps30 );
		}
		else
#endif
		if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
		{
			ShadowFilterMode_t nShadowFilterMode = g_pHardwareConfig->GetShadowFilterMode( false /* bForceLowQuality */, false /* bPS30 */ );

			DECLARE_STATIC_PIXEL_SHADER( flashlight_ps20b );
			SET_STATIC_PIXEL_SHADER_COMBO( SFM, bSFM );
			SET_STATIC_PIXEL_SHADER_COMBO( NORMALMAP, nBumpMapVariant );
			SET_STATIC_PIXEL_SHADER_COMBO( NORMALMAP2, bBump2 );
			SET_STATIC_PIXEL_SHADER_COMBO( WORLDVERTEXTRANSITION, vars.m_bWorldVertexTransition );
			SET_STATIC_PIXEL_SHADER_COMBO( SEAMLESS, bSeamless );
			SET_STATIC_PIXEL_SHADER_COMBO( DETAILTEXTURE, bDetail );
			SET_STATIC_PIXEL_SHADER_COMBO( DETAIL_BLEND_MODE, nDetailBlendMode );
			SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode );
			SET_STATIC_PIXEL_SHADER( flashlight_ps20b );
		}
		else
		{
			DECLARE_STATIC_PIXEL_SHADER( flashlight_ps20 );
			SET_STATIC_PIXEL_SHADER_COMBO( SFM, bSFM );
			SET_STATIC_PIXEL_SHADER_COMBO( NORMALMAP, nBumpMapVariant );
			SET_STATIC_PIXEL_SHADER_COMBO( NORMALMAP2, bBump2 );
			SET_STATIC_PIXEL_SHADER_COMBO( WORLDVERTEXTRANSITION, vars.m_bWorldVertexTransition );
			SET_STATIC_PIXEL_SHADER_COMBO( SEAMLESS, bSeamless );
			SET_STATIC_PIXEL_SHADER_COMBO( DETAILTEXTURE, bDetail );
			SET_STATIC_PIXEL_SHADER_COMBO( DETAIL_BLEND_MODE, nDetailBlendMode );
			SET_STATIC_PIXEL_SHADER( flashlight_ps20 );
		}
		FogToBlack();

		PI_BeginCommandBuffer();
		PI_SetModulationPixelShaderDynamicState( PSREG_DIFFUSE_MODULATION );
		PI_EndCommandBuffer();
	}
	else
	{
		VMatrix worldToTexture;
		ITexture *pFlashlightDepthTexture;
		FlashlightState_t flashlightState = ShaderApiFast( pShaderAPI )->GetFlashlightStateEx( worldToTexture, &pFlashlightDepthTexture );

		SetFlashLightColorFromState( flashlightState, pShaderAPI, false );

		BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, flashlightState.m_pSpotlightTexture, flashlightState.m_nSpotlightTextureFrame );

		ShaderApiFast( pShaderAPI )->BindStandardTexture( SHADER_SAMPLER5, TEXTURE_BINDFLAGS_NONE, TEXTURE_SHADOW_NOISE_2D );
		if( pFlashlightDepthTexture && g_pConfig->ShadowDepthTexture() && flashlightState.m_bEnableShadows )
		{
			BindTexture( SHADER_SAMPLER7, TEXTURE_BINDFLAGS_SHADOWDEPTH, pFlashlightDepthTexture, 0 );

			// Tweaks associated with a given flashlight
			float tweaks[4];
			tweaks[0] = ShadowFilterFromState( flashlightState );
			tweaks[1] = ShadowAttenFromState( flashlightState );
			HashShadow2DJitter( flashlightState.m_flShadowJitterSeed, &tweaks[2], &tweaks[3] );
			ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( PSREG_ENVMAP_TINT__SHADOW_TWEAKS, tweaks, 1 );

			// Dimensions of screen, used for screen-space noise map sampling
			float vScreenScale[4] = {1280.0f / 32.0f, 720.0f / 32.0f, 0, 0};
			int nWidth, nHeight;
			ShaderApiFast( pShaderAPI )->GetBackBufferDimensions( nWidth, nHeight );

			int nTexWidth, nTexHeight;
			ShaderApiFast( pShaderAPI )->GetStandardTextureDimensions( &nTexWidth, &nTexHeight, TEXTURE_SHADOW_NOISE_2D );

			vScreenScale[0] = (float) nWidth  / nTexWidth;
			vScreenScale[1] = (float) nHeight / nTexHeight;
			vScreenScale[2] = 1.0f / flashlightState.m_flShadowMapResolution;
			vScreenScale[3] = 2.0f / flashlightState.m_flShadowMapResolution;

			ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( PSREG_FLASHLIGHT_SCREEN_SCALE, vScreenScale, 1 );
		}
		else
		{
			ShaderApiFast( pShaderAPI )->BindStandardTexture( SHADER_SAMPLER7, TEXTURE_BINDFLAGS_NONE, TEXTURE_WHITE );
		}

		if( params[BASETEXTURE]->IsTexture() && g_pConfig->nFullbright != 2 )
		{
			BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_SRGBREAD, BASETEXTURE, FRAME );
		}
		else
		{
			ShaderApiFast( pShaderAPI )->BindStandardTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_SRGBREAD, TEXTURE_GREY );
		}
		if( vars.m_bWorldVertexTransition )
		{
			Assert( vars.m_nBaseTexture2Var >= 0 && vars.m_nBaseTexture2FrameVar >= 0 );
			BindTexture( SHADER_SAMPLER4, TEXTURE_BINDFLAGS_SRGBREAD, vars.m_nBaseTexture2Var, vars.m_nBaseTexture2FrameVar );
		}
		ShaderApiFast( pShaderAPI )->BindStandardTexture( SHADER_SAMPLER2, TEXTURE_BINDFLAGS_NONE, TEXTURE_NORMALIZATION_CUBEMAP );
		if( vars.m_bBump )
		{
			BindTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, vars.m_nBumpmapVar, vars.m_nBumpmapFrame );
		}
		else
		{
			ShaderApiFast( pShaderAPI )->BindStandardTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, TEXTURE_NORMALIZATION_CUBEMAP );
		}

		if( bDetail )
		{
			BindTexture( SHADER_SAMPLER8, IsSRGBDetailTexture( nDetailBlendMode ) ? TEXTURE_BINDFLAGS_SRGBREAD : TEXTURE_BINDFLAGS_NONE, vars.m_nDetailVar );
		}

		if( vars.m_bWorldVertexTransition )
		{
			if( bBump2 )
			{
				BindTexture( SHADER_SAMPLER6, TEXTURE_BINDFLAGS_NONE, vars.m_nBumpmapVar2, vars.m_nBumpmapFrame2 );
			}
		}

		if( vars.m_bLightmappedGeneric )
		{
#if !defined( _X360 ) && !defined( _PS3 )
			if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_DYNAMIC_VERTEX_SHADER( lightmappedgeneric_flashlight_vs30 );
				SET_DYNAMIC_VERTEX_SHADER( lightmappedgeneric_flashlight_vs30 );
			}
			else
#endif
			{
				DECLARE_DYNAMIC_VERTEX_SHADER( lightmappedgeneric_flashlight_vs20 );
				SET_DYNAMIC_VERTEX_SHADER( lightmappedgeneric_flashlight_vs20 );
			}

			if ( bSeamless )
			{
				float const0[4]={ vars.m_fSeamlessScale,0,0,0};
				ShaderApiFast( pShaderAPI )->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6, const0 );
			}

			if ( bDetail )
			{
				float vDetailConstants[4] = {1,1,1,1};

				if ( vars.m_nDetailTint != -1 )
				{
					params[vars.m_nDetailTint]->GetVecValue( vDetailConstants, 3 );
				}

				if ( vars.m_nDetailTextureBlendFactor != -1 )
				{
					vDetailConstants[3] = params[vars.m_nDetailTextureBlendFactor]->GetFloatValue();
				}

				ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( 0, vDetailConstants, 1 );
			}
		}
		else
		{
			DECLARE_DYNAMIC_VERTEX_SHADER( vertexlitgeneric_flashlight_vs20 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, ShaderApiFast( pShaderAPI )->GetCurrentNumBones() > 0 );
			SET_DYNAMIC_VERTEX_SHADER( vertexlitgeneric_flashlight_vs20 );

			if( vars.m_bTeeth )
			{
				Assert( vars.m_nTeethForwardVar >= 0 );
				Assert( vars.m_nTeethIllumFactorVar >= 0 );
				Vector4D lighting;
				params[vars.m_nTeethForwardVar]->GetVecValue( lighting.Base(), 3 );
				lighting[3] = params[vars.m_nTeethIllumFactorVar]->GetFloatValue();
				ShaderApiFast( pShaderAPI )->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, lighting.Base() );
			}
		}

		ShaderApiFast( pShaderAPI )->SetPixelShaderFogParams( PSREG_FOG_PARAMS );

		float vEyePos_SpecExponent[4];
		ShaderApiFast( pShaderAPI )->GetWorldSpaceCameraPosition( vEyePos_SpecExponent );
		vEyePos_SpecExponent[3] = 0.0f;
		ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( PSREG_EYEPOS_SPEC_EXPONENT, vEyePos_SpecExponent, 1 );

#if !defined( _X360 ) && !defined( _PS3 )
		if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( flashlight_ps30 );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHTSHADOWS, flashlightState.m_bEnableShadows && ( pFlashlightDepthTexture != NULL ) );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( UBERLIGHT, flashlightState.m_bUberlight && bSFM );
			SET_DYNAMIC_PIXEL_SHADER( flashlight_ps30 );

			SetupUberlightFromState( pShaderAPI, flashlightState );
		}
		else
#endif
		if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( flashlight_ps20b );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( FLASHLIGHTSHADOWS, flashlightState.m_bEnableShadows && ( pFlashlightDepthTexture != NULL ) );
			SET_DYNAMIC_PIXEL_SHADER( flashlight_ps20b );
		}
		else
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( flashlight_ps20 );
			SET_DYNAMIC_PIXEL_SHADER( flashlight_ps20 );
		}

		float atten[4];										// Set the flashlight attenuation factors
		atten[0] = flashlightState.m_fConstantAtten;
		atten[1] = flashlightState.m_fLinearAtten;
		atten[2] = flashlightState.m_fQuadraticAtten;
		atten[3] = flashlightState.m_FarZAtten;
		s_pShaderAPI->SetPixelShaderConstant( PSREG_FLASHLIGHT_ATTENUATION, atten, 1 );

		float pos[4];										// Set the flashlight origin
		pos[0] = flashlightState.m_vecLightOrigin[0];
		pos[1] = flashlightState.m_vecLightOrigin[1];
		pos[2] = flashlightState.m_vecLightOrigin[2];
		pos[3] = flashlightState.m_FarZ;
		ShaderApiFast( pShaderAPI )->SetPixelShaderConstant( PSREG_FLASHLIGHT_POSITION_RIM_BOOST, pos, 1 );	// rim boost not really used here

		SetFlashlightVertexShaderConstants( vars.m_bBump, vars.m_nBumpTransform, bDetail, vars.m_nDetailScale,  bSeamless ? false : true );
	}
	Draw();
}

// Take 0..1 seed and map to (u, v) coordinate to be used in shadow filter jittering...
void CBaseVSShader::HashShadow2DJitter( const float fJitterSeed, float *fU, float* fV )
{
	int nTexWidth, nTexHeight;
	s_pShaderAPI->GetStandardTextureDimensions( &nTexWidth, &nTexHeight, TEXTURE_SHADOW_NOISE_2D );

	int nSeed = fmod (fJitterSeed, 1.0f) * nTexWidth * nTexHeight;

	int nRow = nSeed / nTexHeight;
	int nCol = nSeed % nTexWidth;

	// Div and mod to get an individual texel in the fTexRes x fTexRes grid
	*fU = nRow / (float) nTexHeight;	// Row
	*fV = nCol / (float) nTexWidth;		// Column
}

#endif // !_STATIC_LINKED || STDSHADER_DX8_DLL_EXPORT



void CBaseVSShader::DrawEqualDepthToDestAlpha( void )
{
#ifdef STDSHADER_DX9_DLL_EXPORT
	if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
	{
		bool bMakeActualDrawCall = false;
		if( s_pShaderShadow )
		{
			s_pShaderShadow->EnableColorWrites( false );
			s_pShaderShadow->EnableAlphaWrites( true );
			s_pShaderShadow->EnableDepthWrites( false );
			s_pShaderShadow->EnableAlphaTest( false );
			s_pShaderShadow->EnableBlending( false );

			s_pShaderShadow->DepthFunc( SHADER_DEPTHFUNC_EQUAL );

			s_pShaderShadow->SetVertexShader( "depthtodestalpha_vs20", 0 );
			s_pShaderShadow->SetPixelShader( "depthtodestalpha_ps20b", 0 );
		}
		if( s_pShaderAPI )
		{
			s_pShaderAPI->SetVertexShaderIndex( 0 );
			s_pShaderAPI->SetPixelShaderIndex( 0 );

			bMakeActualDrawCall = s_pShaderAPI->ShouldWriteDepthToDestAlpha();
		}
		Draw( bMakeActualDrawCall );
	}
#else
	Assert( 0 ); //probably just needs a shader update to the latest
#endif
}


#if !defined( _PS3 ) && !defined( OSX )
//-----------------------------------------------------------------------------
bool ToolsEnabled()
{
	static bool bToolsMode = ( CommandLine()->CheckParm( "-tools" ) != NULL );
	return bToolsMode;
}
#endif
