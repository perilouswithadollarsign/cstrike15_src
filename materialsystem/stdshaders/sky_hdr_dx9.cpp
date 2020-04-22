//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#include "BaseVSShader.h"
#include "sky_vs20.inc"
#include "sky_ps20.inc"
#include "sky_ps20b.inc"
#include "sky_hdr_compressed_ps20.inc"
#include "sky_hdr_compressed_ps20b.inc"
#include "sky_hdr_compressed_rgbs_ps20.inc"
#include "sky_hdr_compressed_rgbs_ps20b.inc"

#include "convar.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


static ConVar mat_use_compressed_hdr_textures( "mat_use_compressed_hdr_textures", "1" );

DEFINE_FALLBACK_SHADER( Sky, Sky_HDR_DX9 )

BEGIN_VS_SHADER( Sky_HDR_DX9, "Help for Sky_HDR_DX9 shader" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( HDRBASETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "base texture when running with HDR enabled" )
		SHADER_PARAM( HDRCOMPRESSEDTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "base texture (compressed) for hdr compression method A" )
		SHADER_PARAM( HDRCOMPRESSEDTEXTURE0, SHADER_PARAM_TYPE_TEXTURE, "", "compressed base texture0 for hdr compression method B" )
		SHADER_PARAM( HDRCOMPRESSEDTEXTURE1, SHADER_PARAM_TYPE_TEXTURE, "", "compressed base texture1 for hdr compression method B" )
		SHADER_PARAM( HDRCOMPRESSEDTEXTURE2, SHADER_PARAM_TYPE_TEXTURE, "", "compressed base texture2 for hdr compression method B" )
		SHADER_PARAM_OVERRIDE( COLOR, SHADER_PARAM_TYPE_VEC3, "[ 1 1 1]", "color multiplier", SHADER_PARAM_NOT_EDITABLE )
	END_SHADER_PARAMS

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS( MATERIAL_VAR_NOFOG );
		SET_FLAGS( MATERIAL_VAR_IGNOREZ );
	}
	SHADER_INIT
	{
		// First figure out if sampler zero wants to be sRGB
		int nSamplerZeroFlags = 0;
		if ( (params[HDRCOMPRESSEDTEXTURE]->IsDefined()) &&	mat_use_compressed_hdr_textures.GetBool() )
		{
			nSamplerZeroFlags = 0;
		}
		else
		{
			if (params[HDRCOMPRESSEDTEXTURE0]->IsDefined())
			{
				nSamplerZeroFlags = 0;
			}
			else
			{
				ITexture *txtr=params[HDRBASETEXTURE]->GetTextureValue();
				ImageFormat fmt=txtr->GetImageFormat();
				if ( ( fmt == IMAGE_FORMAT_RGBA16161616F ) || ( fmt == IMAGE_FORMAT_RGBA16161616 ) )
					nSamplerZeroFlags = 0;
				else
					nSamplerZeroFlags = TEXTUREFLAGS_SRGB;
			}
		}

		// Next, figure out which texture will be on sampler zero
		int nSampler0 = HDRCOMPRESSEDTEXTURE;
		if ( params[HDRCOMPRESSEDTEXTURE]->IsDefined() && mat_use_compressed_hdr_textures.GetBool() )
		{
			nSampler0 = HDRCOMPRESSEDTEXTURE;
		}
		else
		{
			if ( params[HDRCOMPRESSEDTEXTURE0]->IsDefined() )
			{
				nSampler0 = HDRCOMPRESSEDTEXTURE0;
			}
			else
			{
				nSampler0 = HDRBASETEXTURE;
			}
		}

		// Load the appropriate textures, making sure that the texture set on sampler 0 is sRGB if necessary
		if ( params[HDRCOMPRESSEDTEXTURE]->IsDefined() && (mat_use_compressed_hdr_textures.GetBool() ) )
		{
			LoadTexture( HDRCOMPRESSEDTEXTURE, HDRCOMPRESSEDTEXTURE == nSampler0 ? nSamplerZeroFlags : 0 );
		}
		else
		{
			if (params[HDRCOMPRESSEDTEXTURE0]->IsDefined())
			{
				LoadTexture( HDRCOMPRESSEDTEXTURE0, HDRCOMPRESSEDTEXTURE0 == nSampler0 ? nSamplerZeroFlags : 0 );
				if ( params[HDRCOMPRESSEDTEXTURE1]->IsDefined() )
				{
					LoadTexture( HDRCOMPRESSEDTEXTURE1, HDRCOMPRESSEDTEXTURE1 == nSampler0 ? nSamplerZeroFlags : 0 );
				}
				if ( params[HDRCOMPRESSEDTEXTURE2]->IsDefined())
				{
					LoadTexture( HDRCOMPRESSEDTEXTURE2, HDRCOMPRESSEDTEXTURE2 == nSampler0 ? nSamplerZeroFlags : 0 );
				}
			}
			else
			{
				if ( params[HDRBASETEXTURE]->IsDefined() )
				{
					LoadTexture( HDRBASETEXTURE, HDRBASETEXTURE == nSampler0 ? nSamplerZeroFlags : 0 );
				}
			}
		}
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			SetInitialShadowState();

//			pShaderShadow->EnableAlphaWrites( true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->VertexShaderVertexFormat( VERTEX_POSITION, 1, NULL, 0 );

			DECLARE_STATIC_VERTEX_SHADER( sky_vs20 );
			SET_STATIC_VERTEX_SHADER( sky_vs20 );

			if ( (params[HDRCOMPRESSEDTEXTURE]->IsDefined()) &&
				 mat_use_compressed_hdr_textures.GetBool() )
			{
				pShaderShadow->EnableSRGBRead(SHADER_SAMPLER0,false);
				if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
				{
					DECLARE_STATIC_PIXEL_SHADER( sky_hdr_compressed_rgbs_ps20b );
					SET_STATIC_PIXEL_SHADER( sky_hdr_compressed_rgbs_ps20b );
				}
				else
				{
					DECLARE_STATIC_PIXEL_SHADER( sky_hdr_compressed_rgbs_ps20 );
					SET_STATIC_PIXEL_SHADER( sky_hdr_compressed_rgbs_ps20 );
				}
			}
			else
			{
				if (params[HDRCOMPRESSEDTEXTURE0]->IsDefined())
				{
					pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
					pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
					
					pShaderShadow->EnableSRGBRead(SHADER_SAMPLER0,false);
					pShaderShadow->EnableSRGBRead(SHADER_SAMPLER1,false);
					pShaderShadow->EnableSRGBRead(SHADER_SAMPLER2,false);
					if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
					{
						DECLARE_STATIC_PIXEL_SHADER( sky_hdr_compressed_ps20b );
						SET_STATIC_PIXEL_SHADER( sky_hdr_compressed_ps20b );
					}
					else
					{
						DECLARE_STATIC_PIXEL_SHADER( sky_hdr_compressed_ps20 );
						SET_STATIC_PIXEL_SHADER( sky_hdr_compressed_ps20 );
					}
				}
				else
				{
					ITexture *txtr=params[HDRBASETEXTURE]->GetTextureValue();
					ImageFormat fmt=txtr->GetImageFormat();
					if ((fmt==IMAGE_FORMAT_RGBA16161616F) || (fmt==IMAGE_FORMAT_RGBA16161616))
						pShaderShadow->EnableSRGBRead(SHADER_SAMPLER0,false);
					else
						pShaderShadow->EnableSRGBRead(SHADER_SAMPLER0,true);
					
					if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
					{
						DECLARE_STATIC_PIXEL_SHADER( sky_ps20b );
						SET_STATIC_PIXEL_SHADER( sky_ps20b );
					}
					else
					{
						DECLARE_STATIC_PIXEL_SHADER( sky_ps20 );
						SET_STATIC_PIXEL_SHADER( sky_ps20 );
					}
				}
			}
			// we are writing linear values from this shader.
			pShaderShadow->EnableSRGBWrite( true );

			pShaderShadow->EnableAlphaWrites( true );

#if defined(_PS3)
			// only way to change order in which skybox can be rendered (or to appropriately defer skybox rendering on PS3 in the absence of z pre-pass) if the skybox material is set to ignorez 1.
			// kind of obviates the need for ignorez flags/ignorez in the material. Note that not all skybox materials use this shader.
			// TODO - agree on a more common way of handling z with skyboxes (material vs shader).
 			pShaderShadow->EnableDepthWrites( false );
 			pShaderShadow->EnableDepthTest( true );
#endif
		}

		DYNAMIC_STATE
		{
			DECLARE_DYNAMIC_VERTEX_SHADER( sky_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( sky_vs20 );

			// Texture coord transform
			SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_1, BASETEXTURETRANSFORM );

			float c0[4]={1,1,1,1};
			if (params[COLOR]->IsDefined())
			{
				memcpy(c0,params[COLOR]->GetVecValue(),3*sizeof(float));
			}
			if ( 
				params[HDRCOMPRESSEDTEXTURE]->IsDefined() &&
				mat_use_compressed_hdr_textures.GetBool()
				)
			{
				// set up data needs for pixel shader interpolation
				ITexture *txtr=params[HDRCOMPRESSEDTEXTURE]->GetTextureValue();
				float w=txtr->GetActualWidth();
				float h=txtr->GetActualHeight();
				float FUDGE=0.01/MAX(w,h);					// per ATI
				float c1[4]={0.5/w-FUDGE, 0.5/h-FUDGE, w, h };
				pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, c1);

				BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, HDRCOMPRESSEDTEXTURE, FRAME );
				c0[0]*=8.0;
				c0[1]*=8.0;
				c0[2]*=8.0;
				if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
				{
					DECLARE_DYNAMIC_PIXEL_SHADER( sky_hdr_compressed_rgbs_ps20b );
					SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITE_DEPTH_TO_DESTALPHA, pShaderAPI->ShouldWriteDepthToDestAlpha() );
					SET_DYNAMIC_PIXEL_SHADER( sky_hdr_compressed_rgbs_ps20b );
				}
				else
				{
					DECLARE_DYNAMIC_PIXEL_SHADER( sky_hdr_compressed_rgbs_ps20 );
					SET_DYNAMIC_PIXEL_SHADER( sky_hdr_compressed_rgbs_ps20 );
				}
			}
			else
			{
				float c1[4]={0,0,0,0};
				pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, c1);

				if (params[HDRCOMPRESSEDTEXTURE0]->IsDefined() )
				{
					BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, HDRCOMPRESSEDTEXTURE0, FRAME );
					BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, HDRCOMPRESSEDTEXTURE1, FRAME );
					BindTexture( SHADER_SAMPLER2, TEXTURE_BINDFLAGS_NONE, HDRCOMPRESSEDTEXTURE2, FRAME );
					if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
					{
						DECLARE_DYNAMIC_PIXEL_SHADER( sky_hdr_compressed_ps20b );
						SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITE_DEPTH_TO_DESTALPHA, pShaderAPI->ShouldWriteDepthToDestAlpha() );
						SET_DYNAMIC_PIXEL_SHADER( sky_hdr_compressed_ps20b );
					}
					else
					{
						DECLARE_DYNAMIC_PIXEL_SHADER( sky_hdr_compressed_ps20 );
						SET_DYNAMIC_PIXEL_SHADER( sky_hdr_compressed_ps20 );
					}

				}
				else
				{
					BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, HDRBASETEXTURE, FRAME );
					ITexture *txtr=params[HDRBASETEXTURE]->GetTextureValue();
					ImageFormat fmt=txtr->GetImageFormat();
					if (
						(fmt==IMAGE_FORMAT_RGBA16161616) ||
						( (fmt==IMAGE_FORMAT_RGBA16161616F) && 
						  (g_pHardwareConfig->GetHDRType()==HDR_TYPE_INTEGER))
						)
					{
						c0[0]*=16.0;
						c0[1]*=16.0;
						c0[2]*=16.0;
					}
					if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
					{
						DECLARE_DYNAMIC_PIXEL_SHADER( sky_ps20b );
						SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITE_DEPTH_TO_DESTALPHA, pShaderAPI->ShouldWriteDepthToDestAlpha() );
						SET_DYNAMIC_PIXEL_SHADER( sky_ps20b );
					}
					else
					{
						DECLARE_DYNAMIC_PIXEL_SHADER( sky_ps20 );
						SET_DYNAMIC_PIXEL_SHADER( sky_ps20 );
					}
				}
			}
			pShaderAPI->SetPixelShaderConstant( 0, c0, 1 );
		}
		Draw( );
	}
END_SHADER
