//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"
#include "multiblend_dx9_helper.h"
#include "..\shaderapidx9\locald3dtypes.h"												   
#include "convar.h"
#include "cpp_shader_constant_register_map.h"
#include "multiblend_vs20.inc"
#include "multiblend_vs30.inc"
#include "multiblend_ps20.inc"
#include "multiblend_ps20b.inc"
#include "multiblend_ps30.inc"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


// FIXME: doesn't support fresnel!
void InitParamsMultiblend_DX9( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, Multiblend_DX9_Vars_t &info )
{
	SET_FLAGS2( MATERIAL_VAR2_LIGHTING_LIGHTMAP );
}

void InitMultiblend_DX9( CBaseVSShader *pShader, IMaterialVar** params, Multiblend_DX9_Vars_t &info )
{
	if ( params[ info.m_nBaseTexture ]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nBaseTexture );
	}

	if ( info.m_nSpecTexture != -1 && params[ info.m_nSpecTexture ]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nSpecTexture );
	}

	if ( info.m_nBaseTexture2 != -1 && params[ info.m_nBaseTexture2 ]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nBaseTexture2 );
	}

	if ( info.m_nSpecTexture2 != -1 && params[ info.m_nSpecTexture2 ]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nSpecTexture2 );
	}

	if ( info.m_nBaseTexture3 != -1 && params[ info.m_nBaseTexture3 ]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nBaseTexture3 );
	}

	if ( info.m_nSpecTexture3 != -1 && params[ info.m_nSpecTexture3 ]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nSpecTexture3 );
	}

	if ( info.m_nBaseTexture4 != -1 && params[ info.m_nBaseTexture4 ]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nBaseTexture4 );
	}

	if ( info.m_nSpecTexture4 != -1 && params[ info.m_nSpecTexture4 ]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nSpecTexture4 );
	}
}

void DrawMultiblend_DX9( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
				    IShaderShadow* pShaderShadow, Multiblend_DX9_Vars_t &info, VertexCompressionType_t vertexCompression )
{
	bool bIsModel = IS_FLAG_SET( MATERIAL_VAR_MODEL );
	int nLightingPreviewMode = IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER0 ) + 2 * IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER1 );
	bool bHasSpec1 = ( info.m_nSpecTexture != -1 && params[ info.m_nSpecTexture ]->IsDefined() );
	bool bHasSpec2 = ( info.m_nSpecTexture2 != -1 && params[ info.m_nSpecTexture2 ]->IsDefined() );
	bool bHasSpec3 = ( info.m_nSpecTexture3 != -1 && params[ info.m_nSpecTexture3 ]->IsDefined() );
	bool bHasSpec4 = ( info.m_nSpecTexture4 != -1 && params[ info.m_nSpecTexture4 ]->IsDefined() );
	bool bUsingEditor = pShader->CanUseEditorMaterials(); // pShader->UsingEditor( params );
	bool bSRGBLightMaps = ( g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE );

	SHADOW_STATE
	{
		pShader->SetInitialShadowState( );

		pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
		pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
		pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );
		pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );
		pShaderShadow->EnableTexture( SHADER_SAMPLER5, true );
		pShaderShadow->EnableTexture( SHADER_SAMPLER6, true );
		pShaderShadow->EnableTexture( SHADER_SAMPLER7, true );
		pShaderShadow->EnableTexture( SHADER_SAMPLER8, true );
		pShaderShadow->EnableTexture( SHADER_SAMPLER9, true );

		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, true );	// Always SRGB read on base map 1
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER2, true );	// Always SRGB read on base map 2
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER3, true );	// Always SRGB read on base map 3
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER4, true );	// Always SRGB read on base map 4
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER6, true );	// Always SRGB read on spec map 1
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER7, true );	// Always SRGB read on spec map 1
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER8, true );	// Always SRGB read on spec map 1
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER9, true );	// Always SRGB read on spec map 1

		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER5, bSRGBLightMaps );

		pShaderShadow->EnableSRGBWrite( true );
		pShaderShadow->EnableAlphaWrites( true ); // writing water fog alpha always.

		unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL;
		int nTexCoordCount = 8;
		static int s_TexCoordSize[]={	2,			// 
										2,			// 
										0,			// 
										4,			// alpha blend
										4,			// vertex / blend color 0
										4,			// vertex / blend color 1
										4,			// vertex / blend color 2
										4			// vertex / blend color 3
									};

		pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, s_TexCoordSize, 0 );

#ifndef _X360
		if ( !g_pHardwareConfig->HasFastVertexTextures() )
#endif
		{
			DECLARE_STATIC_VERTEX_SHADER( multiblend_vs20 );
			SET_STATIC_VERTEX_SHADER_COMBO( SPECULAR, !bUsingEditor );
			SET_STATIC_VERTEX_SHADER_COMBO( MODEL,  bIsModel );
			SET_STATIC_VERTEX_SHADER( multiblend_vs20 );

			// Bind ps_2_b shader so we can get Phong terms
			if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( multiblend_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( LIGHTING_PREVIEW, nLightingPreviewMode );
				SET_STATIC_PIXEL_SHADER( multiblend_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( multiblend_ps20 );
				SET_STATIC_PIXEL_SHADER_COMBO( LIGHTING_PREVIEW, nLightingPreviewMode );
				SET_STATIC_PIXEL_SHADER( multiblend_ps20 );
			}
		}
#ifndef _X360
		else
		{
			// The vertex shader uses the vertex id stream
			SET_FLAGS2( MATERIAL_VAR2_USES_VERTEXID );

			DECLARE_STATIC_VERTEX_SHADER( multiblend_vs30 );
			SET_STATIC_VERTEX_SHADER_COMBO( SPECULAR, !bUsingEditor );
			SET_STATIC_VERTEX_SHADER_COMBO( MODEL,  bIsModel );
			SET_STATIC_VERTEX_SHADER( multiblend_vs30 );

			// Bind ps_2_b shader so we can get Phong terms
			DECLARE_STATIC_PIXEL_SHADER( multiblend_ps30 );
			SET_STATIC_PIXEL_SHADER_COMBO( LIGHTING_PREVIEW, nLightingPreviewMode );
			SET_STATIC_PIXEL_SHADER( multiblend_ps30 );
		}
#endif

		pShader->DefaultFog();

		float flLScale = pShaderShadow->GetLightMapScaleFactor();

		// Lighting constants
		pShader->PI_BeginCommandBuffer();
		pShader->PI_SetPixelShaderAmbientLightCube( PSREG_AMBIENT_CUBE );
//		pShader->PI_SetPixelShaderLocalLighting( PSREG_LIGHT_INFO_ARRAY );
		pShader->PI_SetModulationPixelShaderDynamicState_LinearScale_ScaleInW( PSREG_CONSTANT_02, flLScale );
		pShader->PI_EndCommandBuffer();
	}
	DYNAMIC_STATE
	{
		pShaderAPI->SetDefaultState();

		// Bind textures
		pShader->BindTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nBaseTexture );							// Base Map 1
		pShader->BindTexture( SHADER_SAMPLER2, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nBaseTexture2 );							// Base Map 2
		pShader->BindTexture( SHADER_SAMPLER3, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nBaseTexture3 );							// Base Map 3
		pShader->BindTexture( SHADER_SAMPLER4, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nBaseTexture4 );							// Base Map 4
		if ( bHasSpec1 == true )
		{
			pShader->BindTexture( SHADER_SAMPLER6, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nSpecTexture );						// Spec Map 1
		}
		else
		{
			pShaderAPI->BindStandardTexture( SHADER_SAMPLER6, TEXTURE_BINDFLAGS_SRGBREAD, TEXTURE_BLACK );
		}
		if ( bHasSpec2 == true )
		{
			pShader->BindTexture( SHADER_SAMPLER7, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nSpecTexture2 );						// Spec Map 2
		}
		else
		{
			pShaderAPI->BindStandardTexture( SHADER_SAMPLER7, TEXTURE_BINDFLAGS_SRGBREAD, TEXTURE_BLACK );
		}
		if ( bHasSpec3 == true )
		{
			pShader->BindTexture( SHADER_SAMPLER8, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nSpecTexture3 );						// Spec Map 3
		}
		else
		{
			pShaderAPI->BindStandardTexture( SHADER_SAMPLER8, TEXTURE_BINDFLAGS_SRGBREAD, TEXTURE_BLACK );
		}
		if ( bHasSpec4 == true )
		{
			pShader->BindTexture( SHADER_SAMPLER9, TEXTURE_BINDFLAGS_SRGBREAD, info.m_nSpecTexture4 );						// Spec Map 4
		}
		else
		{
			pShaderAPI->BindStandardTexture( SHADER_SAMPLER9, TEXTURE_BINDFLAGS_SRGBREAD, TEXTURE_BLACK );
		}



		pShaderAPI->BindStandardTexture( SHADER_SAMPLER5, ( bSRGBLightMaps ) ? TEXTURE_BINDFLAGS_SRGBREAD : TEXTURE_BINDFLAGS_NONE, TEXTURE_LIGHTMAP );

		Vector4D	vRotations( DEG2RAD( params[ info.m_nRotation ]->GetFloatValue() ), DEG2RAD( params[ info.m_nRotation2 ]->GetFloatValue() ), 
								DEG2RAD( params[ info.m_nRotation3 ]->GetFloatValue() ), DEG2RAD( params[ info.m_nRotation4 ]->GetFloatValue() ) );
		pShaderAPI->SetVertexShaderConstant( 27, vRotations.Base() );

		Vector4D	vScales( params[ info.m_nScale ]->GetFloatValue() > 0.0f ? params[ info.m_nScale ]->GetFloatValue() : 1.0f, 
							 params[ info.m_nScale2 ]->GetFloatValue() > 0.0f ? params[ info.m_nScale2 ]->GetFloatValue() : 1.0f, 
							 params[ info.m_nScale3 ]->GetFloatValue() > 0.0f ? params[ info.m_nScale3 ]->GetFloatValue() : 1.0f, 
							 params[ info.m_nScale4 ]->GetFloatValue() > 0.0f ? params[ info.m_nScale4 ]->GetFloatValue() : 1.0f );
		pShaderAPI->SetVertexShaderConstant( 28, vScales.Base() );

		Vector4D vLightDir;
		vLightDir.AsVector3D() = pShaderAPI->GetVectorRenderingParameter( VECTOR_RENDERPARM_GLOBAL_LIGHT_DIRECTION );
		vLightDir.w = pShaderAPI->GetFloatRenderingParameter( FLOAT_RENDERPARM_SPECULAR_POWER );
		pShaderAPI->SetVertexShaderConstant( 29, vLightDir.Base() );


		LightState_t lightState = {0, false, false};
		pShaderAPI->GetDX9LightState( &lightState );

#ifndef _X360
		if ( !g_pHardwareConfig->HasFastVertexTextures() )
#endif
		{
			DECLARE_DYNAMIC_VERTEX_SHADER( multiblend_vs20 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING,      pShaderAPI->GetCurrentNumBones() > 0 );
			SET_DYNAMIC_VERTEX_SHADER( multiblend_vs20 );

			// Bind ps_2_b shader so we can get Phong, rim and a cloudier refraction
			if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( multiblend_ps20b );
				SET_DYNAMIC_PIXEL_SHADER( multiblend_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( multiblend_ps20 );
				SET_DYNAMIC_PIXEL_SHADER( multiblend_ps20 );
			}
		}
#ifndef _X360
		else
		{
			DECLARE_DYNAMIC_VERTEX_SHADER( multiblend_vs30 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING,      pShaderAPI->GetCurrentNumBones() > 0 );
			SET_DYNAMIC_VERTEX_SHADER( multiblend_vs30 );

			DECLARE_DYNAMIC_PIXEL_SHADER( multiblend_ps30 );
			SET_DYNAMIC_PIXEL_SHADER( multiblend_ps30 );
		}
#endif

		pShader->SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6, info.m_nBaseTextureTransform );

		pShaderAPI->SetPixelShaderFogParams( PSREG_FOG_PARAMS );

		// Pack phong exponent in with the eye position
		float vEyePos_SpecExponent[4];
		float vSpecularTint[4] = {1, 1, 1, 1};
		pShaderAPI->GetWorldSpaceCameraPosition( vEyePos_SpecExponent );

//		if ( (info.m_nPhongExponent != -1) && params[info.m_nPhongExponent]->IsDefined() )
//			vEyePos_SpecExponent[3] = params[info.m_nPhongExponent]->GetFloatValue();		// This overrides the channel in the map
//		else
			vEyePos_SpecExponent[3] = 0;													// Use the alpha channel of the normal map for the exponent

		// If it's all zeros, there was no constant tint in the vmt
		if ( (vSpecularTint[0] == 0.0f) && (vSpecularTint[1] == 0.0f) && (vSpecularTint[2] == 0.0f) )
		{
			vSpecularTint[0] = 1.0f;
			vSpecularTint[1] = 1.0f;
			vSpecularTint[2] = 1.0f;
		}

		pShaderAPI->SetPixelShaderConstant( PSREG_EYEPOS_SPEC_EXPONENT, vEyePos_SpecExponent, 1 );

		// Set c0 and c1 to contain first two rows of ViewProj matrix
		VMatrix matView, matProj, matViewProj;
		pShaderAPI->GetMatrix( MATERIAL_VIEW, matView.m[0] );
		pShaderAPI->GetMatrix( MATERIAL_PROJECTION, matProj.m[0] );
		matViewProj = matView * matProj;
		pShaderAPI->SetPixelShaderConstant( 0, matViewProj.m[0], 2 );

		pShaderAPI->SetPixelShaderFogParams( PSREG_FOG_PARAMS );
	}
	pShader->Draw();
}
