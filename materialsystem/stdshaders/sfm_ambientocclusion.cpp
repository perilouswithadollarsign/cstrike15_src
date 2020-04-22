//===== Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"
#include "sfm_ambientocclusion_vs30.inc"
#include "sfm_ambientocclusion_ps30.inc"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

static bool s_bInited = false;

#define NUM_SSAO_SAMPLES 9

static Vector4D s_vSphereSamples[ NUM_SSAO_SAMPLES ];

float RPercent()
{
	return float( rand() - (VALVE_RAND_MAX/2) ) / (float)(VALVE_RAND_MAX/2);
}

float RPercentABS()
{
	return float( rand() ) / (float)(VALVE_RAND_MAX);
}


BEGIN_VS_SHADER_FLAGS( sfm_ambientocclusion_shader, "Help for SFM ambient occlusion pass", SHADER_NOT_EDITABLE )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( FRONTNDTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( JITTERSEED, SHADER_PARAM_TYPE_VEC4, "", "" )
		SHADER_PARAM( EYEPOSZNEAR, SHADER_PARAM_TYPE_VEC4, "", "" )
		SHADER_PARAM( EYEDIR, SHADER_PARAM_TYPE_VEC4, "", "" ) // Eye direction over zFar
		SHADER_PARAM( FARZ, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( BIAS, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( STRENGTH, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( RADIUS, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( VIEWPROJ, SHADER_PARAM_TYPE_MATRIX, "", "" )
		SHADER_PARAM( AOMODE, SHADER_PARAM_TYPE_INTEGER, "", "" )
	END_SHADER_PARAMS

	SHADER_INIT
	{
		LoadTexture( FRONTNDTEXTURE );
//		LoadTexture( BACKNDTEXTURE );
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
			pShaderShadow->EnableDepthTest( false );
			pShaderShadow->EnableAlphaWrites( false );
			pShaderShadow->EnableBlending( false );
			pShaderShadow->EnableCulling( false );

			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );	// Front ND buffer
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );	// Noise map

			int fmt = VERTEX_POSITION;
			int nTexCoordDimensions[3] = { 2, 3, 3 };
			// Two texture coordinates (first for 2D screen space, second for 3D world space far plane)
			pShaderShadow->VertexShaderVertexFormat( fmt, 3, nTexCoordDimensions, 0 );

			DECLARE_STATIC_VERTEX_SHADER( sfm_ambientocclusion_vs30 );
			SET_STATIC_VERTEX_SHADER( sfm_ambientocclusion_vs30 );

			DECLARE_STATIC_PIXEL_SHADER( sfm_ambientocclusion_ps30 );
			SET_STATIC_PIXEL_SHADER( sfm_ambientocclusion_ps30 );
		}

		DYNAMIC_STATE
		{
			// TODO: we could easily just roll these into the shader itself
			if ( !s_bInited )
			{
				int nSqrtNumSamples = (int)sqrtf( (float)NUM_SSAO_SAMPLES );
				int nNumSamples = nSqrtNumSamples * nSqrtNumSamples;

				Vector *pvDirections = new Vector[nNumSamples];
				Assert( pvDirections );

				int i = 0;
				float oneoverN = 1.0f / (float)nSqrtNumSamples;

				// Fill an N*N*2 array with uniformly distributed
				// samples across the sphere using jittered stratification
				for ( int a=0; a < nSqrtNumSamples; a++ ) 
				{
					for ( int b=0; b < nSqrtNumSamples; b++ ) 
					{
						// Generate unbiased distribution of spherical coords
						float x = ( a + fabs( RPercent() ) ) * oneoverN; // do not reuse results
						float y = ( b + fabs( RPercent() ) ) * oneoverN; // each sample must be random
						float theta = 2.0f * acosf( sqrtf(1.0f - x) );
						float phi = 2.0f * M_PI * y;

						// Convert spherical coords to unit vector
						Vector vec( sinf(theta)*cosf(phi), sinf(theta)*sinf(phi), cosf(theta) );
						pvDirections[i++] = vec;
					}
				}

				for ( int s=0; s<NUM_SSAO_SAMPLES; s++ )
				{
					Vector vSample = pvDirections[s] * RPercentABS();
					s_vSphereSamples[s].Init( vSample.x, vSample.y, vSample.z, 1 );
				}

				s_bInited = true;
			}

			float flFar = params[FARZ]->GetFloatValue();
			float flBias = params[BIAS]->GetFloatValue();//0.005f;
			float flStrenth = params[STRENGTH]->GetFloatValue();//2.0f;
			float flSampleRadius = params[RADIUS]->GetFloatValue();//16.0f;
			float vSampleRadiusNBias[4] = { flSampleRadius, flFar / ( flSampleRadius * flStrenth ), flFar, flBias * flFar };
			pShaderAPI->SetPixelShaderConstant( 10, vSampleRadiusNBias, 1 );

			pShaderAPI->SetPixelShaderConstant( 11, (float *) &s_vSphereSamples[0], NUM_SSAO_SAMPLES );

			// Set c5...c8 to contain ViewProj matrix
			const VMatrix &mViewProj = params[VIEWPROJ]->GetMatrixValue();
			Vector4D vMatrixRows[4];
			vMatrixRows[0].Init( mViewProj[0][0], mViewProj[1][0], mViewProj[2][0], mViewProj[3][0] );
			vMatrixRows[1].Init( mViewProj[0][1], mViewProj[1][1], mViewProj[2][1], mViewProj[3][1] );
			vMatrixRows[2].Init( mViewProj[0][2], mViewProj[1][2], mViewProj[2][2], mViewProj[3][2] );
			vMatrixRows[3].Init( mViewProj[0][3], mViewProj[1][3], mViewProj[2][3], mViewProj[3][3] );
			pShaderAPI->SetPixelShaderConstant( 5, vMatrixRows[0].Base(), 4 );

			int nScreenWidth, nScreenHeight;
			pShaderAPI->GetCurrentRenderTargetDimensions( nScreenWidth, nScreenHeight );
			float vScreenSize[4] = { 1.0f / (float) nScreenWidth, 1.0f / (float) nScreenHeight, 0.0f, 0.0f };
			pShaderAPI->SetPixelShaderConstant( 2, vScreenSize, 1 );

			float vEyePosZNear[4];
			params[EYEPOSZNEAR]->GetVecValue( vEyePosZNear, 4 );
			pShaderAPI->SetPixelShaderConstant( 3, vEyePosZNear, 1 );

			float vEyeDirection[4];
			params[EYEDIR]->GetVecValue( vEyeDirection, 4 );			// This is eye direction over zFar
			pShaderAPI->SetPixelShaderConstant( 4, vEyeDirection, 1 );

			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, FRONTNDTEXTURE, -1 );
			pShaderAPI->BindStandardTexture( SHADER_SAMPLER1, TEXTURE_BINDFLAGS_NONE, TEXTURE_SSAO_NOISE_2D );

			int nTexWidth, nTexHeight;
			pShaderAPI->GetStandardTextureDimensions( &nTexWidth, &nTexHeight, TEXTURE_SSAO_NOISE_2D );

			float vRandSampleScale[4] = { 1.0f / (float)nTexWidth, 1.0f / (float)nTexHeight, 0.5f + ( 0.5f / (float)nScreenWidth ), 0.5f + ( 0.5f / (float)nScreenHeight ) };
			pShaderAPI->SetPixelShaderConstant( 9, vRandSampleScale, 1 );

			float vNoiseOffset[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			HashShadow2DJitter( params[JITTERSEED]->GetFloatValue(), vNoiseOffset, vNoiseOffset+1 );
			pShaderAPI->SetPixelShaderConstant( 1, vNoiseOffset, 1 );

			DECLARE_DYNAMIC_VERTEX_SHADER( sfm_ambientocclusion_vs30 );
			SET_DYNAMIC_VERTEX_SHADER( sfm_ambientocclusion_vs30 );

			DECLARE_DYNAMIC_PIXEL_SHADER( sfm_ambientocclusion_ps30 );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( AO_MODE, params[AOMODE]->GetIntValue() );
			SET_DYNAMIC_PIXEL_SHADER( sfm_ambientocclusion_ps30 );
		}
		Draw();
	}
END_SHADER
