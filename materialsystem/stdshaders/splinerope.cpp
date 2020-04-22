//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//

#include "BaseVSShader.h"
#include "convar.h"

#include "splinerope_ps20.inc"
#include "splinerope_ps20b.inc"
#include "splinerope_vs20.inc"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar rope_min_pixel_diameter( "rope_min_pixel_diameter", "2.0", FCVAR_CHEAT );

#if defined( CSTRIKE15 ) && defined( _X360 )
static ConVar r_shader_srgbread( "r_shader_srgbread", "1", 0, "1 = use shader srgb texture reads, 0 = use HW" );
#else
static ConVar r_shader_srgbread( "r_shader_srgbread", "0", 0, "1 = use shader srgb texture reads, 0 = use HW" );
#endif

BEGIN_VS_SHADER( SplineRope, "Help for SplineRope" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( SHADERSRGBREAD360, SHADER_PARAM_TYPE_BOOL, "0", "Simulate srgb read in shader code")
		SHADER_PARAM( SHADOWDEPTH, SHADER_PARAM_TYPE_INTEGER, "0", "writing to a shadow depth buffer" )
		SHADER_PARAM( BUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "cable/cablenormalmap", "normal map" )
		SHADER_PARAM( TILING, SHADER_PARAM_TYPE_FLOAT, "1", "cable texture tiling multiplier" )
		SHADER_PARAM( ALPHATESTREFERENCE, SHADER_PARAM_TYPE_FLOAT, "1", "")
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		// srgb read 360
		InitIntParam( SHADERSRGBREAD360, params, 0 );
		InitIntParam( SHADOWDEPTH, params, 0 );
		if ( !params[BUMPMAP]->IsDefined() )
		{
			params[BUMPMAP]->SetStringValue( "cable/cablenormalmap" );
		}

		if ( !params[TILING]->IsDefined() )
		{
			params[TILING]->SetFloatValue( 1.0f );
		}

		if ( !params[ALPHATESTREFERENCE]->IsDefined() )
		{
			params[ALPHATESTREFERENCE]->SetFloatValue( 1.0f );
		}

		SET_FLAGS2( MATERIAL_VAR2_IS_SPRITECARD );  // What's this for?
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		SET_FLAGS2( MATERIAL_VAR2_LIGHTING_VERTEX_LIT );
		LoadTexture( BASETEXTURE, TEXTUREFLAGS_SRGB );
		LoadBumpMap( BUMPMAP );
	}

	SHADER_DRAW
	{
#if defined( CSTRIKE15 )
		bool bShaderSrgbRead = IsX360() && r_shader_srgbread.GetBool();
#else
		bool bShaderSrgbRead = ( IsX360() && params[SHADERSRGBREAD360]->GetIntValue() );
#endif
		bool bShadowDepth = ( params[SHADOWDEPTH]->GetIntValue() != 0 );

		bool bUseAlphaTestRef = ( bShadowDepth == false && ( params[ALPHATESTREFERENCE]->GetFloatValue() < 1 ) );
		float flAlphaTestRef = params[ALPHATESTREFERENCE]->GetFloatValue();

		float flTiling = params[TILING]->GetFloatValue();

		SHADOW_STATE
		{
			// draw back-facing because of yaw spin
			pShaderShadow->EnableCulling( false );

			if ( bShadowDepth )
			{
				// don't write color and alpha since we only interested in depth for shadow maps.
				pShaderShadow->EnableColorWrites( false );
				pShaderShadow->EnableAlphaWrites( false );

				// polyoffset for shadow maps.
				pShaderShadow->EnablePolyOffset( SHADER_POLYOFFSET_SHADOW_BIAS );
			}
			else
			{
				// We need to write to dest alpha for depth feathering.
				pShaderShadow->EnableAlphaWrites( true );

				// base texture
				pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, !bShaderSrgbRead );

				// normal map
				pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );

				pShaderShadow->EnableSRGBWrite( true );

				FogToFogColor();
			}

			static int s_TexCoordSize[]={	4,			// (worldspace xyz) (radius (diameter?) of spline at this point) for first control point
											4,			// (worldspace xyz) (radius of spline at this point) for second control point
											4,			// (worldspace xyz) (radius of spline at this point) for third control point
											4,			// (worldspace xyz) (radius of spline at this point) for fourth control point
			};

			unsigned int flags = VERTEX_POSITION | VERTEX_COLOR;

			int numTexCoords = 4;
			pShaderShadow->VertexShaderVertexFormat( flags, numTexCoords, s_TexCoordSize, 0 );

			DECLARE_STATIC_VERTEX_SHADER( splinerope_vs20 );
			SET_STATIC_VERTEX_SHADER( splinerope_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( splinerope_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( SHADER_SRGB_READ, bShaderSrgbRead );
				SET_STATIC_PIXEL_SHADER_COMBO( SHADOWDEPTH, bShadowDepth );
				SET_STATIC_PIXEL_SHADER_COMBO( ALPHATESTREF, bUseAlphaTestRef );
				SET_STATIC_PIXEL_SHADER( splinerope_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( splinerope_ps20 );
				SET_STATIC_PIXEL_SHADER_COMBO( SHADER_SRGB_READ, bShaderSrgbRead );
				SET_STATIC_PIXEL_SHADER_COMBO( SHADOWDEPTH, bShadowDepth );
				SET_STATIC_PIXEL_SHADER_COMBO( ALPHATESTREF, bUseAlphaTestRef );
				SET_STATIC_PIXEL_SHADER( splinerope_ps20 );
			}
		}
		DYNAMIC_STATE
		{
			// We need these only when screen-orienting, which we are always in this shader.
			LoadModelViewMatrixIntoVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0 );
			LoadProjectionMatrixIntoVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3 );

			// Get viewport and render target dimensions and set shader constant to do a 2D mad
			int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
			pShaderAPI->GetCurrentViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );

			float c7[4]={ 0.0f, 0.0f, 0.0f, 0.0f };
			if ( !g_pHardwareConfig->IsAAEnabled() )
			{
				float flMinPixelDiameter = rope_min_pixel_diameter.GetFloat() / ( float )nViewportWidth;
				c7[0]= c7[1] = c7[2] = c7[3] = flMinPixelDiameter;
			}
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_7, c7, 1 );

			float c8[4]={ flTiling, 0.0f, 0.0f, 0.0f };
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_8, c8, 1 );

			// bind base texture
			BindTexture( SHADER_SAMPLER0, SRGBReadMask( !bShaderSrgbRead ), BASETEXTURE, FRAME );

			// normal map
			BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, BUMPMAP );

			if ( !bShadowDepth )
			{
				pShaderAPI->SetPixelShaderFogParams( 0 );

				float vEyePos[4];
				pShaderAPI->GetWorldSpaceCameraPosition( vEyePos );
				vEyePos[3] = flAlphaTestRef;
				pShaderAPI->SetPixelShaderConstant( 1, vEyePos, 1 );
			}

			DECLARE_DYNAMIC_VERTEX_SHADER( splinerope_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( splinerope_vs20 );

			if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( splinerope_ps20b );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITE_DEPTH_TO_DESTALPHA, pShaderAPI->ShouldWriteDepthToDestAlpha() );
				SET_DYNAMIC_PIXEL_SHADER( splinerope_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( splinerope_ps20 );
				SET_DYNAMIC_PIXEL_SHADER_COMBO( WRITE_DEPTH_TO_DESTALPHA, pShaderAPI->ShouldWriteDepthToDestAlpha() );
				SET_DYNAMIC_PIXEL_SHADER( splinerope_ps20 );
			}
		}
		Draw( );
	}

	void ExecuteFastPath( int *dynVSIdx, int *dynPSIdx,  IMaterialVar** params, IShaderDynamicAPI * pShaderAPI, 
		VertexCompressionType_t vertexCompression, CBasePerMaterialContextData **pContextDataPtr,BOOL bCSMEnabled )
	{
		// This is only to be used for shadow map gen!!!

		s_pShaderAPI = pShaderAPI;

		LoadModelViewMatrixIntoVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0 );
		LoadProjectionMatrixIntoVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3 );

		// Get viewport and render target dimensions and set shader constant to do a 2D mad
		int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
		pShaderAPI->GetCurrentViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );

		float c7[4]={ 0.0f, 0.0f, 0.0f, 0.0f };
		if ( !g_pHardwareConfig->IsAAEnabled() )
		{
			float flMinPixelDiameter = rope_min_pixel_diameter.GetFloat() / ( float )nViewportWidth;
			c7[0]= c7[1] = c7[2] = c7[3] = flMinPixelDiameter;
		}
		pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_7, c7, 1 );

		pShaderAPI->SetVertexShaderViewProj();
		pShaderAPI->SetVertexShaderCameraPos();

		*dynVSIdx = 0;
		*dynPSIdx = 0;
	}

END_SHADER
