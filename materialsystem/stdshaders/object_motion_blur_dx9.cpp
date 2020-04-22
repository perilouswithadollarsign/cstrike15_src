//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
// Object space motion blur shader c++ backing file
//
//==================================================================================================

#include "BaseVSShader.h"
#include "object_motion_blur_vs20.inc"
#include "object_motion_blur_ps20.inc"
#include "object_motion_blur_ps20b.inc"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_VS_SHADER_FLAGS( ObjectMotionBlur, "Object Motion Blur", SHADER_NOT_EDITABLE )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( FB_TEXTURE, SHADER_PARAM_TYPE_TEXTURE, "_rt_FullFrameFB", "Full-screen framebuffer to sample from." )
		SHADER_PARAM( VELOCITY_TEXTURE, SHADER_PARAM_TYPE_TEXTURE, "_rt_SmallHDR0", "Full-screen velocity buffer to sample from." )
	END_SHADER_PARAMS

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		if( params[ FB_TEXTURE ]->IsDefined() )
		{
			LoadTexture( FB_TEXTURE );
		}
		if( params[ VELOCITY_TEXTURE ]->IsDefined() )
		{
			LoadTexture( VELOCITY_TEXTURE );
		}
	}

	SHADER_DRAW
	{
		SHADOW_STATE 
		{
			pShaderShadow->VertexShaderVertexFormat( VERTEX_POSITION, 1, 0, 0 );

			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, false );

			DECLARE_STATIC_VERTEX_SHADER( object_motion_blur_vs20 );
			SET_STATIC_VERTEX_SHADER( object_motion_blur_vs20 );

			Assert( g_pHardwareConfig->SupportsPixelShaders_2_b() );
			
			DECLARE_STATIC_PIXEL_SHADER( object_motion_blur_ps20b );
			SET_STATIC_PIXEL_SHADER( object_motion_blur_ps20b );

			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableAlphaWrites( false );
			pShaderShadow->EnableSRGBWrite( true );
		}

		DYNAMIC_STATE
		{
			// Bind textures
			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, FB_TEXTURE );
			BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, VELOCITY_TEXTURE );

			DECLARE_DYNAMIC_VERTEX_SHADER( object_motion_blur_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( object_motion_blur_vs20 );

			Assert( g_pHardwareConfig->SupportsPixelShaders_2_b() );
			DECLARE_DYNAMIC_PIXEL_SHADER( object_motion_blur_ps20b );
			SET_DYNAMIC_PIXEL_SHADER( object_motion_blur_ps20b );
		}

		Draw();
	}
END_SHADER
