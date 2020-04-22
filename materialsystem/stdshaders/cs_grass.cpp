//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "BaseVSShader.h"
#include <string.h>
#include "const.h"

#include "cpp_shader_constant_register_map.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#include "cs_grass_vs20.inc"
#include "cs_grass_ps20.inc"
#include "cs_grass_ps20b.inc"
#include "cs_grass_ps30.inc"

ConVar cl_detail_scale( "cl_detail_scale", "2", FCVAR_CHEAT, "" );

BEGIN_VS_SHADER( Grass, "Help for Grass" )
			  
	BEGIN_SHADER_PARAMS
		//SHADER_PARAM( DUMMYPARAM, SHADER_PARAM_TYPE_VEC3, "[0 0 0]", "" )
	END_SHADER_PARAMS

	SHADER_FALLBACK
	{
		return 0;
	}
	SHADER_INIT_PARAMS()
	{
		SET_FLAGS( MATERIAL_VAR_NO_DEBUG_OVERRIDE );
		SET_FLAGS( MATERIAL_VAR_VERTEXCOLOR );
		SET_FLAGS( MATERIAL_VAR_VERTEXALPHA );
	}

	SHADER_INIT
	{
		LoadTexture( BASETEXTURE, TEXTUREFLAGS_SRGB );
	}

#define SHADER_USE_VERTEX_COLOR		1
#define SHADER_USE_CONSTANT_COLOR	2

	void SetCSGrassCommonShadowState( unsigned int shaderFlags, IMaterialVar **params )
	{
		IShaderShadow *pShaderShadow = s_pShaderShadow;
		pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, true );

		bool bCSMEnabled;
		int	nCSMQualityComboValue = 0;
		bool bSFM = ( ToolsEnabled() && IsPlatformWindowsPC() && g_pHardwareConfig->SupportsPixelShaders_3_0() ) ? true : false;

        bCSMEnabled = g_pHardwareConfig->SupportsCascadedShadowMapping() && !bSFM;
        if ( bCSMEnabled )
        {
            nCSMQualityComboValue = g_pHardwareConfig->GetCSMShaderMode( materials->GetCurrentConfigForVideoCard().GetCSMQualityMode() );
        }

		unsigned int flags = VERTEX_POSITION;

		//if( shaderFlags & SHADER_USE_VERTEX_COLOR )
		{
			flags |= VERTEX_COLOR;
		}

		int numTexCoords = 1;
		static int s_TexCoordDims[] = { 4 };
		pShaderShadow->VertexShaderVertexFormat( flags, numTexCoords, s_TexCoordDims, 0 );

		pShaderShadow->EnableAlphaTest( true );
		//pShaderShadow->EnableAlphaWrites( false );
		pShaderShadow->EnableDepthWrites( true );
		pShaderShadow->EnableCulling( false ); // grass quad winding order can go either way
		pShaderShadow->EnableSRGBWrite( true );

		DefaultFog();

		DECLARE_STATIC_VERTEX_SHADER( cs_grass_vs20 );
		SET_STATIC_VERTEX_SHADER( cs_grass_vs20 );

		if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
		{
			DECLARE_STATIC_PIXEL_SHADER( cs_grass_ps30 );
			SET_STATIC_PIXEL_SHADER_COMBO( CASCADED_SHADOW_MAPPING, bCSMEnabled );
			SET_STATIC_PIXEL_SHADER_COMBO( CSM_MODE, nCSMQualityComboValue);
			SET_STATIC_PIXEL_SHADER( cs_grass_ps30 );
		}
		else if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
		{
			DECLARE_STATIC_PIXEL_SHADER( cs_grass_ps20b );
			SET_STATIC_PIXEL_SHADER( cs_grass_ps20b );
		}
		else
		{
			DECLARE_STATIC_PIXEL_SHADER( cs_grass_ps20 );
			SET_STATIC_PIXEL_SHADER( cs_grass_ps20 );
		}

	}

	void SetCSGrassCommonDynamicState( unsigned int shaderFlags )
	{
		IShaderDynamicAPI *pShaderAPI = s_pShaderAPI;

		BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, BASETEXTURE, FRAME );

		//LoadModelViewMatrixIntoVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0 );
		//LoadProjectionMatrixIntoVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3 );

		float flTime = pShaderAPI->CurrentTime();//sin( pShaderAPI->CurrentTime() ) + sin( pShaderAPI->CurrentTime() * 2 );

		float flConst[4]={ flTime, cl_detail_scale.GetFloat(), 0, 0 };
		pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, flConst, 4 );
		
		static ConVarRef sv_dangerzone_enabled( "sv_dangerzone_enabled" );
		static ConVarRef sv_dangerzone_radius( "sv_dangerzone_radius" );
		static ConVarRef sv_dangerzone_x( "sv_dangerzone_x" );
		static ConVarRef sv_dangerzone_y( "sv_dangerzone_y" );
		static ConVarRef sv_dangerzone_z( "sv_dangerzone_z" );

		bool bCullGrassOusideDangerRadius = false;
		if ( sv_dangerzone_enabled.GetBool() )
		{
			bCullGrassOusideDangerRadius = true;

			flConst[0] = sv_dangerzone_x.GetFloat();
			flConst[1] = sv_dangerzone_y.GetFloat();
			flConst[2] = sv_dangerzone_z.GetFloat();
			flConst[3] = sv_dangerzone_radius.GetFloat();

			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3, flConst, 4 );
		}

		DECLARE_DYNAMIC_VERTEX_SHADER( cs_grass_vs20 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( DZONE, bCullGrassOusideDangerRadius );
		SET_DYNAMIC_VERTEX_SHADER( cs_grass_vs20 );



		bool bCSMEnabled;
		bool bSFM = ( ToolsEnabled() && IsPlatformWindowsPC() && g_pHardwareConfig->SupportsPixelShaders_3_0() ) ? true : false;

        bCSMEnabled = g_pHardwareConfig->SupportsCascadedShadowMapping() && pShaderAPI->IsCascadedShadowMapping() && !bSFM;

		if ( bCSMEnabled )
		{
			ITexture *pDepthTextureAtlas = NULL;
			const CascadedShadowMappingState_t &cascadeState = pShaderAPI->GetCascadedShadowMappingState( &pDepthTextureAtlas );

			if ( pDepthTextureAtlas )
			{
				BindTexture( SHADER_SAMPLER15, TEXTURE_BINDFLAGS_SHADOWDEPTH, pDepthTextureAtlas, 0 );
				pShaderAPI->SetPixelShaderConstant( 64, &cascadeState.m_vLightColor.x, CASCADED_SHADOW_MAPPING_CONSTANT_BUFFER_SIZE );
			}
		}

		if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( cs_grass_ps30 );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( DYN_CSM_ENABLED, bCSMEnabled ? 1 : 0 );
			SET_DYNAMIC_PIXEL_SHADER( cs_grass_ps30 );
		}
		else if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( cs_grass_ps20b );
			SET_DYNAMIC_PIXEL_SHADER( cs_grass_ps20b );
		}
		else
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( cs_grass_ps20 );
			SET_DYNAMIC_PIXEL_SHADER( cs_grass_ps20 );
		}

		pShaderAPI->SetPixelShaderFogParams( PSREG_FOG_PARAMS );

		float vEyePos_SpecExponent[4];
		pShaderAPI->GetWorldSpaceCameraPosition( vEyePos_SpecExponent );

		vEyePos_SpecExponent[3] = 0.0f;
		pShaderAPI->SetPixelShaderConstant( PSREG_EYEPOS_SPEC_EXPONENT, vEyePos_SpecExponent, 1 );
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			SetCSGrassCommonShadowState( 0, params );
		}
		DYNAMIC_STATE
		{
			SetCSGrassCommonDynamicState( 0 );
		}
		
		Draw();
	}
END_SHADER
