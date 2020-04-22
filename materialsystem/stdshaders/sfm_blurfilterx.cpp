//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"
#include "sfm_screenspace_vs30.inc"
#include "sfm_blur_ps30.inc"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


BEGIN_VS_SHADER_FLAGS( sfm_blurfilterx_shader, "Help for SFM blur", SHADER_NOT_EDITABLE )
BEGIN_SHADER_PARAMS
	SHADER_PARAM( BLOOMWIDTH, SHADER_PARAM_TYPE_FLOAT, "", "" )
END_SHADER_PARAMS

SHADER_INIT
{
	if( params[BASETEXTURE]->IsDefined() )
	{
		LoadTexture( BASETEXTURE );
	}

	if ( !params[BLOOMWIDTH]->IsDefined() )
	{
		params[BLOOMWIDTH]->SetFloatValue( 4.0 );
	}
}

SHADER_FALLBACK
{
	// Requires 30 shaders
	if ( g_pHardwareConfig->GetDXSupportLevel() < 95 )
	{
		Assert( 0 );
		return "Wireframe";
	}
	return 0;
}

SHADER_DRAW
{
	SHADOW_STATE
	{
		pShaderShadow->EnableDepthWrites( false );
		pShaderShadow->EnableDepthTest( false );
		pShaderShadow->EnableAlphaWrites( true );
		pShaderShadow->EnableBlending( false );
		pShaderShadow->EnableCulling( false );

		pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
		int fmt = VERTEX_POSITION;
		pShaderShadow->VertexShaderVertexFormat( fmt, 1, 0, 0 );		// TODO: remove tex coords

		DECLARE_STATIC_VERTEX_SHADER( sfm_screenspace_vs30 );
		SET_STATIC_VERTEX_SHADER( sfm_screenspace_vs30 );

		DECLARE_STATIC_PIXEL_SHADER( sfm_blur_ps30 );
		SET_STATIC_PIXEL_SHADER( sfm_blur_ps30 );
	}

	DYNAMIC_STATE
	{
		BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, BASETEXTURE, -1 );

		DECLARE_DYNAMIC_VERTEX_SHADER( sfm_screenspace_vs30 );
		SET_DYNAMIC_VERTEX_SHADER( sfm_screenspace_vs30 );

		DECLARE_DYNAMIC_PIXEL_SHADER( sfm_blur_ps30 );
		SET_DYNAMIC_PIXEL_SHADER( sfm_blur_ps30 );

		// Set constant to enable translation of VPOS to render target coordinates in ps_3_0
		pShaderAPI->SetScreenSizeForVPOS();

		float vDir[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
		pShaderAPI->SetPixelShaderConstant( 0, vDir, 1 );

#define NUM_TAPS 18

		float vCoeff[4 * NUM_TAPS];		// Array of NUM_TAPS float4 constants
		float *pDst = vCoeff;			// pointer for walking array of weights
		float fAccum = 0.0f;			// Accumulator for normalizing weights
		float s = params[BLOOMWIDTH]->GetFloatValue() ;	// s is typically 1/3 or 1/5 the number of samples

		// Center Tap
		*pDst = ( 1.0f / ( sqrt( 2.0f * 3.14159f ) * s ) ) * exp( -( 0/(2*s*s) ) );
		fAccum += *pDst;
		pDst += 4;

		// Offcenter taps
		for ( int i = 1; i < NUM_TAPS; i++ )
		{
			*pDst = ( 1.0f / ( sqrt( 2.0f * 3.14159f ) * s ) ) * exp( -( ((float)i*(float)i) / (2*s*s) ) );
			fAccum += *pDst * 2;
			pDst += 4;
		}

		// Normalize weights (center tap)
		pDst = vCoeff;
		*pDst /= fAccum;
		pDst += 4;

		// Normalize weights (offcenter taps)
		for ( int i = 1; i < NUM_TAPS; i++ )
		{
			*pDst /= fAccum;
			pDst += 4;
		}

		pShaderAPI->SetPixelShaderConstant( 33, vCoeff, NUM_TAPS );
	}
	Draw();
}
END_SHADER
