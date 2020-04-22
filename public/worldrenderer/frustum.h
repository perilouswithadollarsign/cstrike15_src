//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef FRUSTUM_H
#define FRUSTUM_H

#include "mathlib/camera.h"
#include "mathlib/ssemath.h"
#include "worldrenderer/bvhsupport.h"


class CWorldRenderFrustum : public CFrustum
{

public:
	//
	// Slight modification of
	// http://appsrv.cse.cuhk.edu.hk/~fzhang/pssm_vrcia/shadow_vrcia.pdf
	//
	float CalculateSplitPlaneDistance( int nSplit, int nShadowSplits, float flMaxShadowDistance )
	{
		float fIM = (float)nSplit / (float)nShadowSplits;
		float f = MIN( flMaxShadowDistance, m_camera.m_flZFar );
		float n = m_camera.m_flZNear;
		float fBias = 0.1f;
		float flLog = n + ( n * powf( f / n, fIM ) );
		float flLinear = ( n + ( f - n ) * fIM );
		return ( flLog * 0.9f + flLinear * 0.10f ) + fBias;
	}

	void UpdateLightFrustumFromClippedVolume( float flNear, float flFar )
	{
		//VMatrix mLightProj = OrthoMatrixRH( m_fSplitXSize, m_fSplitZSize, MAX( m_camera.m_flZNear, flNear ), MIN( m_camera.m_flZFar, flFar ) );
		VMatrix mLightProj = OrthoMatrixRH( m_fSplitXSize, m_fSplitZSize, flNear, flFar );
		SetProj( mLightProj );
		CalcViewProj();
	}

	void SetSplitSizes( float flXSize, float flZSize ) { m_fSplitXSize = flXSize; m_fSplitZSize = flZSize; }

	void CalculateLightFrusta( CWorldRenderFrustum **ppOutFrustums, int nShadowSplits, Vector vLightDir, float flLightDistance, float flLightFarPlane, float flMaxShadowDistance )
	{
		float fPreviousSplit = m_camera.m_flZNear;
		Vector vEye = m_camera.m_origin;
		Vector vEyeDir = m_forward;
		vEyeDir.NormalizeInPlace();
		vLightDir.NormalizeInPlace();

		for( int i=0; i<nShadowSplits; i++ )
		{
			float fSplitPlane = CalculateSplitPlaneDistance( i + 1, nShadowSplits, flMaxShadowDistance );

			float fZDist = fSplitPlane - fPreviousSplit;
			float fCenterZ = ( fPreviousSplit + fSplitPlane ) * 0.5f;

			float fXSize = tanf( DEG2RAD(m_camera.m_flFOVX) * 0.5f ) * fSplitPlane * 2.0f;
			fXSize += ( nShadowSplits - i ) * 12.0f;
			fZDist *= 1.10f;

#if ( SNAP_SHADOW_MAP == 1 )
			fXSize = max( fXSize, fZDist );
			fZDist = fXSize;
#endif

			Vector vLightLookAt = vEye + vEyeDir * fCenterZ;
			Vector vLightPosition = vLightLookAt + vLightDir * flLightDistance;

			// Create the light frustum
			VMatrix mLightView;
			VMatrix mLightProj;
#if ( SNAP_SHADOW_MAP == 1 )
			Vector vFrustumUp( 0,1,0 );
#else
			Vector vFrustumUp = vEyeDir;
#endif
			mLightView = ViewMatrixRH( vLightPosition, vLightLookAt, vFrustumUp );

#if ( SNAP_SHADOW_MAP == 1 )
			// Clamp camera movement to full-pixels only
			float fUnitsPerTexel = fXSize / (float)g_ShadowDepthTextureSize;
			mLightView._41 = floorf( mLightView._41 / fUnitsPerTexel ) * fUnitsPerTexel;
			mLightView._42 = floorf( mLightView._42 / fUnitsPerTexel ) * fUnitsPerTexel;
#endif

			CWorldRenderFrustum *pOutFrustum = (ppOutFrustums)[i];

			mLightProj = OrthoMatrixRH( fXSize, fZDist, m_camera.m_flZNear, flLightFarPlane );
			pOutFrustum->SetSplitSizes( fXSize, fZDist );

			pOutFrustum->SetView( mLightView );
			pOutFrustum->SetProj( mLightProj );
			pOutFrustum->CalcViewProj();
			pOutFrustum->SetCameraPosition( vLightPosition );
			Vector vLightForward;
			Vector vLightLeft;
			Vector vLightUp;
			VMatrix mView( pOutFrustum->GetView() );
			MatrixGetRow( mView, 0, &vLightLeft );
			MatrixGetRow( mView, 1, &vLightUp );
			MatrixGetRow( mView, 2, &vLightForward );
			Vector vLightRight = -vLightLeft;
			vLightForward = -vLightForward;

			pOutFrustum->SetNearFarPlanes( m_camera.m_flZNear, flLightFarPlane );
			pOutFrustum->SetForward( -vLightDir );

			Frustum_t lightFrustum;
			VPlane pSixPlanes[6];
			GenerateOrthoFrustum( vec3_origin, vLightForward, vLightRight, vLightUp, 
				-fXSize/2.0f, fXSize/2.0f, -fZDist/2.0f, fZDist/2.0f, 
				m_camera.m_flZNear, flLightFarPlane, 
				pSixPlanes );
			lightFrustum.SetPlanes( pSixPlanes );
			pOutFrustum->SetFrustum( lightFrustum );

			fPreviousSplit = fSplitPlane;
		}
	}

protected:
	float		m_fSplitXSize;
	float		m_fSplitZSize;
};



#endif
