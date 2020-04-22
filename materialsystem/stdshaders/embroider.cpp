//=========== Copyright © Valve Corporation, All rights reserved. ===========//
//
// Purpose: Create embroidery effect textures from arbitrary images
//
//===========================================================================//

#include "BaseVSShader.h"
#include "embroider_vs30.inc"
#include "embroider_ps30.inc"

#include "../materialsystem_global.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_VS_SHADER( Embroider, "Help for Embroider" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( BASETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Original image" )
		SHADER_PARAM( STITCHTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Stitching pattern with normal and alpha" )
		SHADER_PARAM( BACKINGTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Backing pattern with normal and ao in alpha" )
		SHADER_PARAM( SAMPLEOFFSETTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Backing pattern offset for choosing stitch color" )
		SHADER_PARAM( SPHERENORMAL, SHADER_PARAM_TYPE_TEXTURE, "", "Spherical normal to aid in influencing anisotropic specular highlights in end-use render" )
		SHADER_PARAM( NCOLORS, SHADER_PARAM_TYPE_INTEGER, "10", "Number of colors for posterization" )
		SHADER_PARAM( COLORGAMMA, SHADER_PARAM_TYPE_FLOAT, "0.8", "Gamma for posterization" )
		SHADER_PARAM( TEXTUREMODE, SHADER_PARAM_TYPE_INTEGER, "1", "Multiple textures need to be generated for end-use render. Mode determines which one we're working on." )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		if ( !params[STITCHTEXTURE]->IsDefined() )
		{
			params[STITCHTEXTURE]->SetStringValue( "models/player/customization/source/stitch_normal" );
		}
		if ( !params[SPHERENORMAL]->IsDefined() )
		{
			params[SPHERENORMAL]->SetStringValue( "models/player/customization/source/spherical_normal" );
		}
		if ( !params[BACKINGTEXTURE]->IsDefined() )
		{
			params[BACKINGTEXTURE]->SetStringValue( "models/player/customization/source/backing_normal" );
		}
		if ( !params[SAMPLEOFFSETTEXTURE]->IsDefined() )
		{
			params[SAMPLEOFFSETTEXTURE]->SetStringValue( "models/player/customization/source/stitch_sample_offset" );
		}
		if ( !params[NCOLORS]->IsDefined() )
		{
			params[NCOLORS]->SetIntValue( 10 );
		}
		if ( !params[COLORGAMMA]->IsDefined() )
		{
			params[COLORGAMMA]->SetFloatValue( 0.8 );
		}
		if ( !params[TEXTUREMODE]->IsDefined() )
		{
			params[TEXTUREMODE]->SetIntValue( 0 );
		}

	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		LoadTexture( BASETEXTURE, TEXTUREFLAGS_BORDER );
		LoadTexture( STITCHTEXTURE );
		LoadTexture( BACKINGTEXTURE );
		LoadTexture( SPHERENORMAL );
		LoadTexture( SAMPLEOFFSETTEXTURE );
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			// Set stream format (note that this shader supports compression)
			int flags = VERTEX_POSITION | VERTEX_FORMAT_COMPRESSED;
			 // NOTE: Have to say that we want 1 texcoord here even though we don't use it or you'll get this Warning in another part of the code: 
			//		"ERROR: shader asking for a too-narrow vertex format - you will see errors if running with debug D3D DLLs!\n\tPadding the vertex format with extra texcoords"
			int nTexCoordCount = 1;
			int userDataSize = 0;
			int mode = params[TEXTUREMODE]->GetIntValue();
			int palettize = params[NCOLORS]->GetIntValue() > 0;
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );

			// Vertex Shader
			if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_STATIC_VERTEX_SHADER( embroider_vs30 );
				SET_STATIC_VERTEX_SHADER( embroider_vs30 );
			}

			// Pixel Shader
			if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_STATIC_PIXEL_SHADER( embroider_ps30 );
				SET_STATIC_PIXEL_SHADER_COMBO( MODE, mode );
				SET_STATIC_PIXEL_SHADER_COMBO( PALETTIZE, palettize );
				SET_STATIC_PIXEL_SHADER( embroider_ps30 );
			}

			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );
			
			pShaderShadow->EnableSRGBWrite( false ); // diffuse writes srgb
			pShaderShadow->EnableAlphaWrites( ( mode != 2 ) ); // aniso and diffuse write alpha
		}
		DYNAMIC_STATE
		{
			
			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, BASETEXTURE, -1 ); // NOT srgb! We are posterizing the color and assuming linear gives us better spread to the colors at the low end
			BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, STITCHTEXTURE, -1 );
			BindTexture( SHADER_SAMPLER2, TEXTURE_BINDFLAGS_NONE, BACKINGTEXTURE, -1 );
			BindTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_NONE, SPHERENORMAL, -1 );
			BindTexture( SHADER_SAMPLER4, TEXTURE_BINDFLAGS_NONE, SAMPLEOFFSETTEXTURE, -1 );

			SetPixelShaderConstant( 0, NCOLORS );
			SetPixelShaderConstant( 1, COLORGAMMA );

			// Vertex Shader
			if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_DYNAMIC_VERTEX_SHADER( embroider_vs30 );
				SET_DYNAMIC_VERTEX_SHADER( embroider_vs30 );
			}

			// Pixel Shader

			if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( embroider_ps30 );
				SET_DYNAMIC_PIXEL_SHADER( embroider_ps30 );
			}
		}
		Draw();
	}

END_SHADER


