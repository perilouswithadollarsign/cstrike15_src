//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "BaseVSShader.h"

#include "writez_vs20.inc"
#include "black_ps20.inc"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Enables alpha blending workaround for an (apparently) weird color/alpha write disable bug on NVidia GL drivers.
// For NVidia, the actual bug is in glBlitFramebuffer() (it doesn't ignore the currently set colormask), which we fix in glmgr.cpp Blit2(), so I'm disabling this more expensive workaround.
ConVar gl_nvidia_occlusion_workaround( "gl_nvidia_occlusion_workaround", "0" );
ConVar gl_amd_occlusion_workaround( "gl_amd_occlusion_workaround", "1" );

DEFINE_FALLBACK_SHADER( Occlusion, Occlusion_DX9 )

BEGIN_VS_SHADER_FLAGS( Occlusion_DX9, "Help for Occlusion", SHADER_NOT_EDITABLE )

	BEGIN_SHADER_PARAMS
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			if ( IsOpenGL() && ( IsPlatformLinux() || IsPlatformWindowsPC() ) && gl_nvidia_occlusion_workaround.GetBool() )
			{
				// Another workaround is to disable color writes but enable alpha writes (with no blending), which this trashes alpha which may be used in some branches.
				// Without either workaround colorwrite enable somehow doesn't get re-enabled in subsequent passes (even though I can clearly see the engine issuing GL calls to set the mask back to 255,255,255,255)
				pShaderShadow->EnableColorWrites( true );
				pShaderShadow->EnableAlphaWrites( true );
				pShaderShadow->EnableBlending( true );
				pShaderShadow->BlendFunc( SHADER_BLEND_ZERO, SHADER_BLEND_ONE );
			}
			else
			{
				pShaderShadow->EnableColorWrites( false );
				pShaderShadow->EnableAlphaWrites( false );
			}
			pShaderShadow->EnableDepthWrites( false );

			DECLARE_STATIC_VERTEX_SHADER( writez_vs20 );
			SET_STATIC_VERTEX_SHADER( writez_vs20 );

			// No pixel shader on Direct3D, doubles fill rate
			if ( g_pHardwareConfig->PlatformRequiresNonNullPixelShaders() )
			{
				DECLARE_STATIC_PIXEL_SHADER( black_ps20 );
				SET_STATIC_PIXEL_SHADER( black_ps20 );

				// Workaround for weird AMD bug - if sRGB write isn't enabled here then sRGB write enable in subsequent world rendering passes will randomly not take effect (even though we're enabling it) in the driver.
				if ( ( IsPlatformLinux() || IsPlatformWindowsPC() ) && gl_amd_occlusion_workaround.GetBool() )
				{
					pShaderShadow->EnableSRGBWrite( true );
				}
			}

			// Set stream format (note that this shader supports compression)
			unsigned int flags = VERTEX_POSITION | VERTEX_FORMAT_COMPRESSED;
			int nTexCoordCount = 1;
			int userDataSize = 0;
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );
		}
		DYNAMIC_STATE
		{
			DECLARE_DYNAMIC_VERTEX_SHADER( writez_vs20 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
			SET_DYNAMIC_VERTEX_SHADER( writez_vs20 );

			// No pixel shader on Direct3D, doubles fill rate
			if ( g_pHardwareConfig->PlatformRequiresNonNullPixelShaders() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( black_ps20 );
				SET_DYNAMIC_PIXEL_SHADER( black_ps20 );
			}
		}
		Draw();
	}
END_SHADER

