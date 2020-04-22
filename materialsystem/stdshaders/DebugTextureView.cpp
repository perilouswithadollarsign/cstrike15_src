//========= Copyright (c) 1996-2007, Valve Corporation, All rights reserved. ============//

#include "BaseVSShader.h"
#include "shaderlib/cshader.h"

#include "debugtextureview_vs20.inc"
#include "debugtextureview_ps20.inc"
#include "debugtextureview_ps20b.inc"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


DEFINE_FALLBACK_SHADER( DebugTextureView, DebugTextureView_dx9 )
BEGIN_VS_SHADER( DebugTextureView_dx9, "Help for DebugTextureView" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( SHOWALPHA, SHADER_PARAM_TYPE_BOOL, "0", "" )
	END_SHADER_PARAMS

	SHADER_INIT
	{
		if ( params[BASETEXTURE]->IsDefined() )
		{
			LoadTexture( BASETEXTURE );
		}
	}
	
	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableAlphaTest( true );

			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );

			// Set stream format (note that this shader supports compression)
			unsigned int flags = VERTEX_POSITION | VERTEX_FORMAT_COMPRESSED;
			int nTexCoordCount = 1;
			int userDataSize = 0;
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );

			DECLARE_STATIC_VERTEX_SHADER( debugtextureview_vs20 );
			SET_STATIC_VERTEX_SHADER( debugtextureview_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( debugtextureview_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( SHOWALPHA, params[SHOWALPHA]->GetIntValue() != 0 );
				SET_STATIC_PIXEL_SHADER( debugtextureview_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( debugtextureview_ps20 );
				SET_STATIC_PIXEL_SHADER_COMBO( SHOWALPHA, params[SHOWALPHA]->GetIntValue() != 0 );
				SET_STATIC_PIXEL_SHADER( debugtextureview_ps20 );
			}
		}

		DYNAMIC_STATE
		{
			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, BASETEXTURE, FRAME );
			//pShaderAPI->BindStandardTexture( SHADER_SAMPLER1, TEXTURE_LIGHTMAP );

			ITexture *pTexture = params[BASETEXTURE]->GetTextureValue();

			float cPsConst0[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			if ( ( pTexture->GetImageFormat() == IMAGE_FORMAT_RGBA16161616F ) ||
				 ( pTexture->GetImageFormat() == IMAGE_FORMAT_RGBA16161616 ) ||
				 ( pTexture->GetImageFormat() == IMAGE_FORMAT_RGB323232F ) ||
				 ( pTexture->GetImageFormat() == IMAGE_FORMAT_RGBA32323232F ) )
			{
				if ( pTexture->IsCubeMap() )
					cPsConst0[0] = 1.0f;
				else
					cPsConst0[1] = 1.0f;
			}

			if ( pTexture->IsVolumeTexture() )
			{
				int nSlices = pTexture->GetMappingDepth();
				int nDesiredSlice = int( Plat_FloatTime() ) % nSlices;

				cPsConst0[2] = float( nDesiredSlice * 2 + 1 ) / ( nSlices * 2 );
			}
			
			pShaderAPI->SetPixelShaderConstant( 0, cPsConst0 );

			DECLARE_DYNAMIC_VERTEX_SHADER( debugtextureview_vs20 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER( debugtextureview_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( debugtextureview_ps20b );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( ISCUBEMAP, pTexture->IsCubeMap() );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( ISVOLUME, pTexture->IsVolumeTexture() );
				SET_DYNAMIC_PIXEL_SHADER( debugtextureview_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( debugtextureview_ps20 );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( ISCUBEMAP, pTexture->IsCubeMap() );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( ISVOLUME, pTexture->IsVolumeTexture() );
				SET_DYNAMIC_PIXEL_SHADER( debugtextureview_ps20 );
			}
		}
		Draw();
	}
END_SHADER
