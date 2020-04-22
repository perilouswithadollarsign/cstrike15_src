//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"
#include "sfm_screenspace_vs30.inc"
#include "sfm_shape_ps30.inc"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


BEGIN_VS_SHADER_FLAGS( sfm_shape_shader, "Help for SFM shape", SHADER_NOT_EDITABLE )
BEGIN_SHADER_PARAMS
END_SHADER_PARAMS

SHADER_INIT
{
	if( params[BASETEXTURE]->IsDefined() )
	{
		LoadTexture( BASETEXTURE );
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

		DECLARE_STATIC_PIXEL_SHADER( sfm_shape_ps30 );
		SET_STATIC_PIXEL_SHADER( sfm_shape_ps30 );
	}

	DYNAMIC_STATE
	{
		BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, BASETEXTURE, -1 );

		DECLARE_DYNAMIC_VERTEX_SHADER( sfm_screenspace_vs30 );
		SET_DYNAMIC_VERTEX_SHADER( sfm_screenspace_vs30 );

		DECLARE_DYNAMIC_PIXEL_SHADER( sfm_shape_ps30 );
		SET_DYNAMIC_PIXEL_SHADER( sfm_shape_ps30 );

		// Set constant to enable translation of VPOS to render target coordinates in ps_3_0
		pShaderAPI->SetScreenSizeForVPOS();
	}
	Draw();
}
END_SHADER
