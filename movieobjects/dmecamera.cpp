//======= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======
//
// A class representing a camera
//
//===============================================================================

#include "movieobjects/dmecamera.h"
#include "tier0/dbg.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "mathlib/vector.h"
#include "movieobjects/dmetransform.h"
#include "materialsystem/imaterialsystem.h"
#include "movieobjects_interfaces.h"
#include "tier2/tier2.h"

// FIXME: REMOVE
#include "istudiorender.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeCamera, CDmeCamera );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeCamera::OnConstruction()
{
	m_flFieldOfView.InitAndSet( this, "fieldOfView", 30.0f );

	// FIXME: This currently matches the client DLL for HL2
	// but we probably need a way of getting this state from the client DLL
	m_zNear.InitAndSet( this, "znear", 3.0f );
	m_zFar.InitAndSet( this, "zfar", 16384.0f * 1.73205080757f );

	m_flFocalDistance.InitAndSet( this, "focalDistance", 72.0f);
	m_flZeroParallaxDistance.InitAndSet( this, "zeroParallaxDistance", 75.0f );
	m_flEyeSeparation.InitAndSet( this, "eyeSeparation", 0.75f );
	m_flAperture.InitAndSet( this, "aperture", 0.2f);
	m_shutterSpeed.InitAndSet( this, "shutterSpeed", DmeTime_t( 0.5f / 24.0f ) );
	m_flToneMapScale.InitAndSet( this, "toneMapScale", 1.0f );
	m_flAOBias.InitAndSet( this, "SSAOBias", 0.0005f );
	m_flAOStrength.InitAndSet( this, "SSAOStrength", 1.0f );
	m_flAORadius.InitAndSet( this, "SSAORadius", 15.0f );
	m_flBloomScale.InitAndSet( this, "bloomScale", 0.28f );
	m_nDoFQuality.InitAndSet( this, "depthOfFieldQuality", 0 );
	m_nMotionBlurQuality.InitAndSet( this, "motionBlurQuality", 0 );
	m_flBloomWidth.InitAndSet( this, "bloomWidth", 9.0f );
	m_bOrtho.InitAndSet( this, "ortho", false );

	// Ortho
	m_nAxis.InitAndSet( this, "axis", 0 );
	m_bWasBehindFrustum.InitAndSet( this, "behindfrustum", false );
	m_flDistance.InitAndSet( this, "distance", 32.0f );
	for ( int i = 0; i < AXIS_COUNT; ++i )
	{
		char sz[ 32 ];
		Q_snprintf( sz, sizeof( sz ), "scale%d", i );
		m_flScale[ i ].InitAndSet( this, sz, 1.0f );
		Q_snprintf( sz, sizeof( sz ), "lookat%d", i );
		m_vecLookAt[ i ].Init( this, sz );
	}

	m_vecAxis.Init();
	m_vecOrigin.Init();
	m_angRotation.Init();
	SetIdentityMatrix( m_Transform );
}

void CDmeCamera::OnDestruction()
{
}
	
//-----------------------------------------------------------------------------
// Loads the material system view matrix based on the transform
//-----------------------------------------------------------------------------
void CDmeCamera::LoadViewMatrix( bool bUseEngineCoordinateSystem )
{
	if ( !g_pMaterialSystem )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	VMatrix view;
	GetViewMatrix( view, bUseEngineCoordinateSystem );
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadMatrix( view );
}

//-----------------------------------------------------------------------------
// Loads the material system projection matrix based on the fov, etc.
//-----------------------------------------------------------------------------
void CDmeCamera::LoadProjectionMatrix( int nDisplayWidth, int nDisplayHeight )
{
	if ( !g_pMaterialSystem )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );

	VMatrix proj;
	GetProjectionMatrix( proj, nDisplayWidth, nDisplayHeight );
	pRenderContext->LoadMatrix( proj );
}

//-----------------------------------------------------------------------------
// Sets up studiorender camera state
//-----------------------------------------------------------------------------
void CDmeCamera::LoadStudioRenderCameraState()
{
	// FIXME: Remove this! This should automatically happen in DrawModel
	// in studiorender.
	if ( !g_pStudioRender )
		return;

	matrix3x4_t transform;
	GetTransform()->GetTransform( transform );

	Vector vecOrigin, vecRight, vecUp, vecForward;
	MatrixGetColumn( transform, 0, vecRight );
	MatrixGetColumn( transform, 1, vecUp );
	MatrixGetColumn( transform, 2, vecForward );
	MatrixGetColumn( transform, 3, vecOrigin );
	g_pStudioRender->SetViewState( vecOrigin, vecRight, vecUp, vecForward );
}

//-----------------------------------------------------------------------------
// Returns the x FOV (the full angle)
//-----------------------------------------------------------------------------
float CDmeCamera::GetFOVx() const
{
	return m_flFieldOfView;
}

//-----------------------------------------------------------------------------
// Returns the near Z in inches
//-----------------------------------------------------------------------------
float CDmeCamera::GetNearZ() const
{
	return m_zNear;
}

//-----------------------------------------------------------------------------
// Sets the near Z in inches
//-----------------------------------------------------------------------------
void CDmeCamera::SetNearZ( float zNear )
{
	m_zNear = zNear;
}

//-----------------------------------------------------------------------------
// Returns the far Z in inches
//-----------------------------------------------------------------------------
float CDmeCamera::GetFarZ() const
{
	return m_zFar;
}

//-----------------------------------------------------------------------------
// Sets the far Z in inches
//-----------------------------------------------------------------------------
void CDmeCamera::SetFarZ( float zFar )
{
	m_zFar = zFar;
}

void CDmeCamera::SetFOVx( float fov )
{
	m_flFieldOfView = fov;
}

//-----------------------------------------------------------------------------
// Returns the focal distance in inches
//-----------------------------------------------------------------------------
float CDmeCamera::GetFocalDistance() const
{
	return m_flFocalDistance;
}

//-----------------------------------------------------------------------------
// Sets the focal distance in inches
//-----------------------------------------------------------------------------
void CDmeCamera::SetFocalDistance( const float &flFocalDistance )
{
	m_flFocalDistance = flFocalDistance;
}

//-----------------------------------------------------------------------------
// Returns the zero-parallax distance in inches
//-----------------------------------------------------------------------------
float CDmeCamera::GetZeroParallaxDistance() const
{
	return m_flZeroParallaxDistance;
}

//-----------------------------------------------------------------------------
// Sets the zero-parallax distance in inches
//-----------------------------------------------------------------------------
void CDmeCamera::SetZeroParallaxDistance( const float &flZeroParallaxDistance )
{
	m_flZeroParallaxDistance = flZeroParallaxDistance;
}

//-----------------------------------------------------------------------------
// Returns the eye separation distance in inches
//-----------------------------------------------------------------------------
float CDmeCamera::GetEyeSeparation() const
{
	return m_flEyeSeparation;
}

//-----------------------------------------------------------------------------
// Sets the eye separation distance in inches
//-----------------------------------------------------------------------------
void CDmeCamera::SetEyeSeparation( const float &flEyeSeparation )
{
	m_flEyeSeparation = flEyeSeparation;
}

//-----------------------------------------------------------------------------
// Returns the camera aperture in inches
//-----------------------------------------------------------------------------
float CDmeCamera::GetAperture() const
{
	return m_flAperture;
}

//-----------------------------------------------------------------------------
// Returns the camera aperture in inches
//-----------------------------------------------------------------------------
DmeTime_t CDmeCamera::GetShutterSpeed() const
{
	return m_shutterSpeed;
}

//-----------------------------------------------------------------------------
// Returns the tone map scale
//-----------------------------------------------------------------------------
float CDmeCamera::GetToneMapScale() const
{
	return m_flToneMapScale;
}

//-----------------------------------------------------------------------------
// Returns the camera's Ambient occlusion bias
//-----------------------------------------------------------------------------
float CDmeCamera::GetAOBias() const
{
	return m_flAOBias;
}

//-----------------------------------------------------------------------------
// Returns the camera's Ambient occlusion strength
//-----------------------------------------------------------------------------
float CDmeCamera::GetAOStrength() const
{
	return m_flAOStrength;
}

//-----------------------------------------------------------------------------
// Returns the camera's Ambient occlusion radius
//-----------------------------------------------------------------------------
float CDmeCamera::GetAORadius() const
{
	return m_flAORadius;
}

//-----------------------------------------------------------------------------
// Returns the bloom scale
//-----------------------------------------------------------------------------
float CDmeCamera::GetBloomScale() const
{
	return m_flBloomScale;
}

//-----------------------------------------------------------------------------
// Returns the bloom width
//-----------------------------------------------------------------------------
float CDmeCamera::GetBloomWidth() const
{
	return m_flBloomWidth;
}


//-----------------------------------------------------------------------------
// Returns the number of Depth of Field samples
//-----------------------------------------------------------------------------
int CDmeCamera::GetDepthOfFieldQuality() const
{
	return m_nDoFQuality;
}

//-----------------------------------------------------------------------------
// Returns the number of Motion Blur samples
//-----------------------------------------------------------------------------
int CDmeCamera::GetMotionBlurQuality() const
{
	return m_nMotionBlurQuality;
}

//-----------------------------------------------------------------------------
// Returns the view direction
//-----------------------------------------------------------------------------
void CDmeCamera::GetViewDirection( Vector *pDirection )
{
	matrix3x4_t transform;
	GetTransform()->GetTransform( transform );
	MatrixGetColumn( transform, 2, *pDirection );

	// We look down the -z axis
	*pDirection *= -1.0f;
}

//-----------------------------------------------------------------------------
// Sets up render state in the material system for rendering
//-----------------------------------------------------------------------------
void CDmeCamera::SetupRenderState( int nDisplayWidth, int nDisplayHeight, bool bUseEngineCoordinateSystem /* = false */ )
{
	LoadViewMatrix( bUseEngineCoordinateSystem );
	LoadProjectionMatrix( nDisplayWidth, nDisplayHeight );
	LoadStudioRenderCameraState( );
}

//-----------------------------------------------------------------------------
// accessors for generated matrices
//-----------------------------------------------------------------------------
void CDmeCamera::GetViewMatrix( VMatrix &view, bool bUseEngineCoordinateSystem /* = false */ )
{
	matrix3x4_t transform, invTransform;
	CDmeTransform *pTransform = GetTransform();
	pTransform->GetTransform( transform );

	if ( bUseEngineCoordinateSystem )
	{
		VMatrix matRotate( transform );
		VMatrix matRotateZ;
		MatrixBuildRotationAboutAxis( matRotateZ, Vector(0,0,1), -90 );
		MatrixMultiply( matRotate, matRotateZ, matRotate );

		VMatrix matRotateX;
		MatrixBuildRotationAboutAxis( matRotateX, Vector(1,0,0), 90 );
		MatrixMultiply( matRotate, matRotateX, matRotate );
		transform = matRotate.As3x4();
	}

	MatrixInvert( transform, invTransform );
	view.Init( invTransform );
}

void CDmeCamera::GetProjectionMatrix( VMatrix &proj, int width, int height )
{
	float flFOV = m_flFieldOfView.Get();
	float flZNear = m_zNear.Get();
	float flZFar = m_zFar.Get();
	float flApsectRatio = (float)width / (float)height;

//	MatrixBuildPerspective( proj, flFOV, flFOV * flApsectRatio, flZNear, flZFar );

#if 1
	float halfWidth = tan( flFOV * M_PI / 360.0 );
	float halfHeight = halfWidth / flApsectRatio;
#else
	float halfHeight = tan( flFOV * M_PI / 360.0 );
	float halfWidth = flApsectRatio * halfHeight;
#endif
	memset( proj.Base(), 0, sizeof( proj ) );
	proj[0][0]  = 1.0f / halfWidth;
	proj[1][1]  = 1.0f / halfHeight;
	proj[2][2] = flZFar / ( flZNear - flZFar );
	proj[3][2] = -1.0f;
	proj[2][3] = flZNear * flZFar / ( flZNear - flZFar );
}

void CDmeCamera::GetViewProjectionInverse( VMatrix &viewprojinv, int width, int height )
{
	VMatrix view, proj;
	GetViewMatrix( view );
	GetProjectionMatrix( proj, width, height );

	VMatrix viewproj;
	MatrixMultiply( proj, view, viewproj );
	bool success = MatrixInverseGeneral( viewproj, viewprojinv );
	if ( !success )
	{
		Assert( 0 );
		MatrixInverseTR( viewproj, viewprojinv );
	}
}

//-----------------------------------------------------------------------------
// Computes the screen space position given a screen size
//-----------------------------------------------------------------------------
void CDmeCamera::ComputeScreenSpacePosition( const Vector &vecWorldPosition, int width, int height, Vector2D *pScreenPosition )
{
	VMatrix view, proj, viewproj;
	GetViewMatrix( view );
	GetProjectionMatrix( proj, width, height );
	MatrixMultiply( proj, view, viewproj );

	Vector vecScreenPos;
	Vector3DMultiplyPositionProjective( viewproj, vecWorldPosition, vecScreenPos );

	pScreenPosition->x = ( vecScreenPos.x + 1.0f ) * width / 2.0f;
	pScreenPosition->y = ( -vecScreenPos.y + 1.0f ) * height / 2.0f;
}

const matrix3x4_t &CDmeCamera::GetOrthoTransform() const
{
	return m_Transform;
}

const Vector &CDmeCamera::GetOrthoAbsOrigin() const
{
	return m_vecOrigin;
}

const QAngle &CDmeCamera::GetOrthoAbsAngles() const
{
	return m_angRotation;
}

void CDmeCamera::OrthoUpdate()
{
	m_vecAxis.Init( 0, 0, 0 );
	switch ( m_nAxis )
	{
	default:
		Assert( 0 );
	case AXIS_X:
		m_vecAxis[ 1 ] = 1;
		break;
	case AXIS_Y: 
		m_vecAxis[ 2 ] = -1;
		break;
	case AXIS_Z:
		m_vecAxis[ 0 ] = 1;
		break;
	case AXIS_X_NEG:
		m_vecAxis[ 1 ] = -1;
		break;
	case AXIS_Y_NEG: 
		m_vecAxis[ 2 ] = 1;
		break;
	case AXIS_Z_NEG:
		m_vecAxis[ 0 ] = -1;
		break;
	}

	m_vecOrigin = m_vecLookAt[ m_nAxis ].Get() - m_flDistance * m_vecAxis;
	Assert( m_vecOrigin.IsValid() );

	VectorAngles( m_vecAxis,m_angRotation );
	AngleMatrix( m_angRotation, m_vecOrigin, m_Transform );
}

void CDmeCamera::FromCamera( CDmeCamera *pCamera )
{
	m_bOrtho = pCamera->m_bOrtho;
	matrix3x4_t mat;
	pCamera->GetAbsTransform( mat );
	MatrixCopy( mat, m_Transform );
	m_flFieldOfView = pCamera->GetFOVx();

	for ( int i = 0; i < AXIS_COUNT; ++i )
	{
		m_vecLookAt[ i ] =pCamera->m_vecLookAt[ i ];
		m_flScale[ i ] = pCamera->m_flScale[ i ];
	}
	m_flDistance = pCamera->m_flDistance;
	m_nAxis = pCamera->m_nAxis;
	m_bWasBehindFrustum = pCamera->m_bWasBehindFrustum;

	if ( m_bOrtho )
	{
		OrthoUpdate();
	}
}

void CDmeCamera::ToCamera( CDmeCamera *pCamera )
{
	pCamera->m_bOrtho = m_bOrtho;
	pCamera->SetFOVx( m_flFieldOfView );
	pCamera->SetAbsTransform( m_Transform );

	for ( int i = 0; i < AXIS_COUNT; ++i )
	{
		pCamera->m_vecLookAt[ i ] = m_vecLookAt[ i ];
		pCamera->m_flScale[ i ] = m_flScale[ i ];
	}
	pCamera->m_flDistance = m_flDistance;
	pCamera->m_nAxis = m_nAxis;
	pCamera->m_bWasBehindFrustum = m_bWasBehindFrustum;

	if ( m_bOrtho )
	{
		pCamera->OrthoUpdate();
	}
}