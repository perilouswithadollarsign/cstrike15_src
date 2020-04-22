//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "BaseVSShader.h"

#include "screenspaceeffect_vs20.inc"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

static void GetAdjustedShaderName( char *pOutputBuffer, char const *pShader20Name )
{
	Q_strcpy( pOutputBuffer, pShader20Name );
	if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
	{
		size_t iLength = Q_strlen( pOutputBuffer );
		if( ( iLength > 4 ) && 
			(
				( Q_stricmp( pOutputBuffer + iLength - 5, "_ps20" ) == 0 ) || 
				( Q_stricmp( pOutputBuffer + iLength - 5, "_vs20" ) == 0 )
				)
			)
		{
			strcpy( pOutputBuffer + iLength - 2, "20b" );
		}
	}
}

DEFINE_FALLBACK_SHADER( screenspace_general, screenspace_general_dx9 )
BEGIN_VS_SHADER_FLAGS( screenspace_general_dx9, "Help for screenspace_general", SHADER_NOT_EDITABLE )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( C0_X,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( C0_Y,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( C0_Z,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( C0_W,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( C1_X,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( C1_Y,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( C1_Z,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( C1_W,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( C2_X,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( C2_Y,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( C2_Z,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( C2_W,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( C3_X,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( C3_Y,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( C3_Z,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( C3_W,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( C4_X,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( C4_Y,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( C4_Z,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( C4_W,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( PIXSHADER, SHADER_PARAM_TYPE_STRING, "", "Name of the pixel shader to use" )
		SHADER_PARAM( VERTEXSHADER, SHADER_PARAM_TYPE_STRING, "", "Name of the vertex shader to use" )
		SHADER_PARAM( DISABLE_COLOR_WRITES,SHADER_PARAM_TYPE_INTEGER,"0","")
		SHADER_PARAM( ALPHATESTED,SHADER_PARAM_TYPE_FLOAT,"0","")
		SHADER_PARAM( ALPHA_BLEND_COLOR_OVERLAY, SHADER_PARAM_TYPE_INTEGER, "0", "")
		SHADER_PARAM( ALPHA_BLEND, SHADER_PARAM_TYPE_INTEGER, "0", "")
		SHADER_PARAM( TEXTURE1, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( TEXTURE2, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( TEXTURE3, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( LINEARREAD_BASETEXTURE, SHADER_PARAM_TYPE_INTEGER, "0", "" )
		SHADER_PARAM( LINEARREAD_TEXTURE1, SHADER_PARAM_TYPE_INTEGER, "0", "" )
		SHADER_PARAM( LINEARREAD_TEXTURE2, SHADER_PARAM_TYPE_INTEGER, "0", "" )
		SHADER_PARAM( LINEARREAD_TEXTURE3, SHADER_PARAM_TYPE_INTEGER, "0", "" )
		SHADER_PARAM( LINEARWRITE,SHADER_PARAM_TYPE_INTEGER,"0","")
		SHADER_PARAM( VERTEXTRANSFORM,SHADER_PARAM_TYPE_INTEGER, "0", "verts are in world space" )
		SHADER_PARAM( ALPHABLEND,SHADER_PARAM_TYPE_INTEGER, "0", "whether or not to enable alpha blend" )
		SHADER_PARAM( MULTIPLYCOLOR, SHADER_PARAM_TYPE_INTEGER, "0", "whether or not to multiply src and dest color" )
		SHADER_PARAM( WRITEALPHA,SHADER_PARAM_TYPE_INTEGER, "0", "whether or not to enable alpha write" )
		SHADER_PARAM( WRITEDEPTH,SHADER_PARAM_TYPE_INTEGER, "0", "whether or not to enable depth write" )
		SHADER_PARAM( TCSIZE0, SHADER_PARAM_TYPE_INTEGER, "2", "Number of components in texture coord0" )
		SHADER_PARAM( TCSIZE1, SHADER_PARAM_TYPE_INTEGER, "0", "Number of components in texture coord1" )
		SHADER_PARAM( TCSIZE2, SHADER_PARAM_TYPE_INTEGER, "0", "Number of components in texture coord2" )
		SHADER_PARAM( TCSIZE3, SHADER_PARAM_TYPE_INTEGER, "0", "Number of components in texture coord3" )
		SHADER_PARAM( TCSIZE4, SHADER_PARAM_TYPE_INTEGER, "0", "Number of components in texture coord4" )
		SHADER_PARAM( TCSIZE5, SHADER_PARAM_TYPE_INTEGER, "0", "Number of components in texture coord5" )
		SHADER_PARAM( TCSIZE6, SHADER_PARAM_TYPE_INTEGER, "0", "Number of components in texture coord6" )
		SHADER_PARAM( TCSIZE7, SHADER_PARAM_TYPE_INTEGER, "0", "Number of components in texture coord7" )
		SHADER_PARAM( POINTSAMPLE_BASETEXTURE, SHADER_PARAM_TYPE_INTEGER, "0", "" )
		SHADER_PARAM( POINTSAMPLE_TEXTURE1, SHADER_PARAM_TYPE_INTEGER, "0", "" )
		SHADER_PARAM( POINTSAMPLE_TEXTURE2, SHADER_PARAM_TYPE_INTEGER, "0", "" )
		SHADER_PARAM( POINTSAMPLE_TEXTURE3, SHADER_PARAM_TYPE_INTEGER, "0", "" )
	    SHADER_PARAM( CULL, SHADER_PARAM_TYPE_INTEGER, "0", "Culling control - 0 = nocull, 1 = do cull" )
	    SHADER_PARAM( DEPTHTEST, SHADER_PARAM_TYPE_INTEGER, "0", "Enable Depthtest" )
	    SHADER_PARAM( COPYALPHA, SHADER_PARAM_TYPE_INTEGER, "0", "")
	END_SHADER_PARAMS

    SHADER_INIT_PARAMS()
    {
		if ( ! params[TCSIZE0]->IsDefined() )
			params[TCSIZE0]->SetIntValue( 2 );
	}

    SHADER_INIT
	{

		if ( params[BASETEXTURE]->IsDefined() )
		{
#if defined( PLATFORM_POSIX ) && !defined( _PS3 )
			ImageFormat fmt = params[BASETEXTURE]->GetTextureValue()->GetImageFormat();
			bool bSRGB;
			if ( ( fmt == IMAGE_FORMAT_RGBA16161616F ) || ( fmt == IMAGE_FORMAT_RGBA16161616 ) )
				bSRGB = false;
			else
				bSRGB = !params[LINEARREAD_BASETEXTURE]->IsDefined() || !params[LINEARREAD_BASETEXTURE]->GetIntValue();
			LoadTexture( BASETEXTURE, bSRGB ? TEXTUREFLAGS_SRGB : 0 );
#else
			LoadTexture( BASETEXTURE );
#endif // PLATFORM_POSIX
		}
		if ( params[TEXTURE1]->IsDefined() )
		{
#if defined( PLATFORM_POSIX ) && !defined( _PS3 )
			ImageFormat fmt = params[TEXTURE1]->GetTextureValue()->GetImageFormat();
			bool bSRGB;
			if ( ( fmt == IMAGE_FORMAT_RGBA16161616F ) || ( fmt == IMAGE_FORMAT_RGBA16161616 ) )
				bSRGB = false;
			else
				bSRGB = !params[LINEARREAD_TEXTURE1]->IsDefined() || !params[LINEARREAD_TEXTURE1]->GetIntValue();
			LoadTexture( TEXTURE1, bSRGB ? TEXTUREFLAGS_SRGB : 0 );
#else
			LoadTexture( TEXTURE1 );
#endif // PLATFORM_POSIX
		}
		if ( params[TEXTURE2]->IsDefined() )
		{
#if defined( PLATFORM_POSIX ) && !defined( _PS3 )
			ImageFormat fmt = params[TEXTURE2]->GetTextureValue()->GetImageFormat();
			bool bSRGB;
			if ( ( fmt == IMAGE_FORMAT_RGBA16161616F ) || ( fmt == IMAGE_FORMAT_RGBA16161616 ) )
				bSRGB = false;
			else
				bSRGB = !params[LINEARREAD_TEXTURE2]->IsDefined() || !params[LINEARREAD_TEXTURE2]->GetIntValue();
			LoadTexture( TEXTURE2, bSRGB ? TEXTUREFLAGS_SRGB : 0 );
#else
			LoadTexture( TEXTURE2 );
#endif // PLATFORM_POSIX
		}
		if ( params[TEXTURE3]->IsDefined() )
		{
#if defined( PLATFORM_POSIX ) && !defined( _PS3 )
			ImageFormat fmt = params[TEXTURE3]->GetTextureValue()->GetImageFormat();
			bool bSRGB;
			if ( ( fmt == IMAGE_FORMAT_RGBA16161616F ) || ( fmt == IMAGE_FORMAT_RGBA16161616 ) )
				bSRGB = false;
			else
				bSRGB = !params[LINEARREAD_TEXTURE3]->IsDefined() || !params[LINEARREAD_TEXTURE3]->GetIntValue();
			LoadTexture( TEXTURE3, bSRGB ? TEXTUREFLAGS_SRGB : 0 );
#else
			LoadTexture( TEXTURE3 );
#endif // PLATFORM_POSIX
		}
	}
	
	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_DRAW
	{
		bool bCustomVertexShader = params[VERTEXSHADER]->IsDefined();
		SHADOW_STATE
		{
			pShaderShadow->EnableDepthWrites( params[WRITEDEPTH]->GetIntValue() != 0 );
			if ( params[WRITEDEPTH]->GetIntValue() != 0 )
			{
				pShaderShadow->EnableDepthTest( true );
				pShaderShadow->DepthFunc( SHADER_DEPTHFUNC_ALWAYS );
			}
			pShaderShadow->EnableAlphaWrites( params[WRITEALPHA]->GetIntValue() != 0 );
			pShaderShadow->EnableDepthTest( params[DEPTHTEST]->GetIntValue() != 0 );
			pShaderShadow->EnableCulling( params[CULL]->GetIntValue() != 0 );

			if (params[BASETEXTURE]->IsDefined())
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
				pShaderShadow->EnableSRGBRead(SHADER_SAMPLER0, !params[LINEARREAD_BASETEXTURE]->IsDefined() || !params[LINEARREAD_BASETEXTURE]->GetIntValue() );
			}				
			if (params[TEXTURE1]->IsDefined())
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
				pShaderShadow->EnableSRGBRead(SHADER_SAMPLER1, !params[LINEARREAD_TEXTURE1]->IsDefined() || !params[LINEARREAD_TEXTURE1]->GetIntValue() );
			}				
			if (params[TEXTURE2]->IsDefined())
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
				pShaderShadow->EnableSRGBRead(SHADER_SAMPLER2, !params[LINEARREAD_TEXTURE2]->IsDefined() || !params[LINEARREAD_TEXTURE2]->GetIntValue() );
			}				
			if (params[TEXTURE3]->IsDefined())
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );
				pShaderShadow->EnableSRGBRead(SHADER_SAMPLER3,false);
				pShaderShadow->EnableSRGBRead(SHADER_SAMPLER3, !params[LINEARREAD_TEXTURE3]->IsDefined() || !params[LINEARREAD_TEXTURE3]->GetIntValue() );
			}				
			int fmt = VERTEX_POSITION;

			const bool bHasVertexColor = IS_FLAG_SET( MATERIAL_VAR_VERTEXCOLOR );
			if ( bHasVertexColor )
			{
				fmt |= VERTEX_COLOR;
			}
		


			int nTexCoordSize[8];
			int nNumTexCoords = 0;

			static int s_tcSizeIds[]={ TCSIZE0, TCSIZE1, TCSIZE2, TCSIZE3, TCSIZE4, TCSIZE5, TCSIZE6, TCSIZE7 };

			int nCtr = 0;
			while( nCtr < 8 && ( params[s_tcSizeIds[nCtr]]->GetIntValue() ) )
			{
				nNumTexCoords++;
				nTexCoordSize[nCtr] = params[s_tcSizeIds[nCtr]]->GetIntValue();
				nCtr++;
			}
			pShaderShadow->VertexShaderVertexFormat( fmt, nNumTexCoords, nTexCoordSize, 0 );

			if ( IS_FLAG_SET(MATERIAL_VAR_ADDITIVE) )
			{
				EnableAlphaBlending( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
			}
			else if ( params[MULTIPLYCOLOR]->GetIntValue() )
			{
				EnableAlphaBlending( SHADER_BLEND_ZERO, SHADER_BLEND_SRC_COLOR );
			}
			else if ( params[ALPHABLEND]->GetIntValue() )
			{
				EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
			}
			else
			{
				pShaderShadow->EnableBlending( false );
			}

			// maybe convert from linear to gamma on write.
			bool srgb_write=true;
			if (params[LINEARWRITE]->GetFloatValue())
				srgb_write=false;
			pShaderShadow->EnableSRGBWrite( srgb_write );

			char szShaderNameBuf[256];

			if ( bCustomVertexShader )
			{
				GetAdjustedShaderName( szShaderNameBuf, params[VERTEXSHADER]->GetStringValue() );
				pShaderShadow->SetVertexShader( params[VERTEXSHADER]->GetStringValue(), 0 ); //szShaderNameBuf, 0 );
			}
			else
			{
				// Pre-cache shaders
				DECLARE_STATIC_VERTEX_SHADER( screenspaceeffect_vs20 );
				SET_STATIC_VERTEX_SHADER_COMBO_HAS_DEFAULT( VERTEXCOLOR, bHasVertexColor );
				SET_STATIC_VERTEX_SHADER_COMBO_HAS_DEFAULT( TRANSFORMVERTS, params[VERTEXTRANSFORM]->GetIntValue() );
				SET_STATIC_VERTEX_SHADER( screenspaceeffect_vs20 );
			}
				
			if (params[DISABLE_COLOR_WRITES]->GetIntValue())
			{
				pShaderShadow->EnableColorWrites(false);
			}
//			if (params[ALPHATESTED]->GetFloatValue())
			{
				pShaderShadow->EnableAlphaTest(true);
				pShaderShadow->AlphaFunc(SHADER_ALPHAFUNC_GREATER,0.0);
			}

			GetAdjustedShaderName( szShaderNameBuf, params[PIXSHADER]->GetStringValue() );
			pShaderShadow->SetPixelShader( szShaderNameBuf, 0 );
			if ( params[ ALPHA_BLEND_COLOR_OVERLAY ]->GetIntValue() )
			{
				// Used for adding L4D halos
				EnableAlphaBlending( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
			}
			if ( params[ ALPHA_BLEND ]->GetIntValue() )
			{
				// Used for adding L4D halos
				EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
			}


			if( params[ COPYALPHA ]->GetIntValue() )
			{
				pShaderShadow->EnableBlending( false );
				pShaderShadow->AlphaFunc( SHADER_ALPHAFUNC_ALWAYS, 0.0f );
			}

		}

		DYNAMIC_STATE
		{
			if (params[BASETEXTURE]->IsDefined())
			{
				BindTexture( SHADER_SAMPLER0, SRGBReadMask( !params[LINEARREAD_BASETEXTURE]->IsDefined() || !params[LINEARREAD_BASETEXTURE]->GetIntValue() ), BASETEXTURE );
				if ( params[POINTSAMPLE_BASETEXTURE]->GetIntValue() )
					pShaderAPI->SetTextureFilterMode( SHADER_SAMPLER0, TFILTER_MODE_POINTSAMPLED );
			}
			if (params[TEXTURE1]->IsDefined())
			{
				BindTexture( SHADER_SAMPLER1, SRGBReadMask( !params[LINEARREAD_TEXTURE1]->IsDefined() || !params[LINEARREAD_TEXTURE1]->GetIntValue() ), TEXTURE1, -1 );
				if ( params[POINTSAMPLE_TEXTURE1]->GetIntValue() )
					pShaderAPI->SetTextureFilterMode( SHADER_SAMPLER1, TFILTER_MODE_POINTSAMPLED );
			}
			if (params[TEXTURE2]->IsDefined())
			{
				BindTexture( SHADER_SAMPLER2, SRGBReadMask( !params[LINEARREAD_TEXTURE2]->IsDefined() || !params[LINEARREAD_TEXTURE2]->GetIntValue() ), TEXTURE2, -1 );
				if ( params[POINTSAMPLE_TEXTURE2]->GetIntValue() )
					pShaderAPI->SetTextureFilterMode( SHADER_SAMPLER2, TFILTER_MODE_POINTSAMPLED );
			}
			if (params[TEXTURE3]->IsDefined())
			{
				BindTexture( SHADER_SAMPLER3, SRGBReadMask( !params[LINEARREAD_TEXTURE3]->IsDefined() || !params[LINEARREAD_TEXTURE3]->GetIntValue() ), TEXTURE3, -1 );
				if ( params[POINTSAMPLE_TEXTURE3]->GetIntValue() )
					pShaderAPI->SetTextureFilterMode( SHADER_SAMPLER3, TFILTER_MODE_POINTSAMPLED );
			}
			float c0[]={
				params[C0_X]->GetFloatValue(),
				params[C0_Y]->GetFloatValue(),
				params[C0_Z]->GetFloatValue(),
				params[C0_W]->GetFloatValue(),
				params[C1_X]->GetFloatValue(),
				params[C1_Y]->GetFloatValue(),
				params[C1_Z]->GetFloatValue(),
				params[C1_W]->GetFloatValue(),
				params[C2_X]->GetFloatValue(),
				params[C2_Y]->GetFloatValue(),
				params[C2_Z]->GetFloatValue(),
				params[C2_W]->GetFloatValue(),
				params[C3_X]->GetFloatValue(),
				params[C3_Y]->GetFloatValue(),
				params[C3_Z]->GetFloatValue(),
				params[C3_W]->GetFloatValue(),
				params[C4_X]->GetFloatValue(),
				params[C4_Y]->GetFloatValue(),
				params[C4_Z]->GetFloatValue(),
				params[C4_W]->GetFloatValue()
			};

			pShaderAPI->SetPixelShaderConstant( 0, c0, ARRAYSIZE(c0)/4 );

			float eyePos[4];
			pShaderAPI->GetWorldSpaceCameraPosition( eyePos );
			pShaderAPI->SetPixelShaderConstant( 10, eyePos, 1 );

			pShaderAPI->SetVertexShaderIndex( 0 );
			pShaderAPI->SetPixelShaderIndex( 0 );

			if ( ! bCustomVertexShader )
			{
				DECLARE_DYNAMIC_VERTEX_SHADER( screenspaceeffect_vs20 );
				SET_DYNAMIC_VERTEX_SHADER( screenspaceeffect_vs20 );
			}
		}
		Draw();
	}
END_SHADER
