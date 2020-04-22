//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#include "mathlib/camera.h"
#include "tier0/dbg.h"
#include "mathlib/vector.h"
#include "mathlib/vmatrix.h"
#include "tier2/tier2.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define FORWARD_AXIS 0
#define LEFT_AXIS 1
#define UP_AXIS 2

/// matrix to align our Valve coordinate system camera space with a standard viewing coordinate system
/// x-axis goes from left to right in view space (right in camera space)
/// y-axis goes from bottom to top in view space (up in camera space)
/// z-axis goes from far to near in view space (-forward in camera space)
/// The constructor takes sequential matrix rows, which are basis vectors in view space (so transposed from the init)
#ifdef YUP_ACTIVE
#error YUP_ACTIVE is not supported on this branch.
#endif

/// g_ViewAlignMatrix.Init( Vector(0,-1,0), Vector(0,0,1), Vector(-1,0,0), vec3_origin );
static matrix3x4_t g_ViewAlignMatrix( 0, 0, -1, 0,		-1, 0, 0, 0,	0, 1, 0, 0 );
VMatrix g_matViewToCameraMatrix( 0, 0, -1, 0,	-1, 0, 0, 0,   0, 1, 0, 0,  0, 0, 0, 1 );
VMatrix g_matCameraToViewMatrix( 0, -1, 0, 0,   0, 0, 1, 0,  -1, 0, 0, 0,  0, 0, 0, 1 );

//--------------------------------------------------------------------------------------------------
// Extract the direction vectors from the a matrix 
//--------------------------------------------------------------------------------------------------
void ExtractDirectionVectors( Vector *pForward, Vector *pLeft, Vector *pUp, const matrix3x4_t &mMatrix )
{ 
	MatrixGetColumn( mMatrix, FORWARD_AXIS, *pForward );
	MatrixGetColumn( mMatrix, LEFT_AXIS, *pLeft );
	MatrixGetColumn( mMatrix, UP_AXIS, *pUp );
}


// Returns points  in this order:
//  2--3
//	|  |
//	0--1
void CalcFarPlaneCameraRelativePoints( Vector *p4PointsOut, Vector &vForward, Vector &vUp, Vector &vLeft, float flFarPlane, 
									   float flFovX, float flAspect,
									   float flClipSpaceBottomLeftX /*= -1.0f*/, float flClipSpaceBottomLeftY /*= -1.0f*/,
									   float flClipSpaceTopRightX /*= 1.0f*/, float flClipSpaceTopRightY /*= 1.0f*/ )
{
	Vector vFowardShift = flFarPlane * vForward;
	Vector vUpShift;
	Vector vRightShift;
	if ( flFovX == -1 )
	{
		vUpShift = vUp;
		vRightShift = -vLeft;
	}
	else
	{
		float flTanX = tanf( DEG2RAD( flFovX * 0.5f ) );
		float flTanY = flTanX / flAspect;

		vUpShift = flFarPlane * flTanY * vUp;
		vRightShift = flFarPlane * flTanX * -vLeft;
	}

	p4PointsOut[0] = vFowardShift + flClipSpaceBottomLeftX * vRightShift + flClipSpaceBottomLeftY * vUpShift;
	p4PointsOut[1] = vFowardShift + flClipSpaceTopRightX * vRightShift + flClipSpaceBottomLeftY * vUpShift;
	p4PointsOut[2] = vFowardShift + flClipSpaceBottomLeftX * vRightShift + flClipSpaceTopRightY * vUpShift;
	p4PointsOut[3] = vFowardShift + flClipSpaceTopRightX * vRightShift + flClipSpaceTopRightY * vUpShift;
}

//-----------------------------------------------------------------------------
// accessors for generated matrices
//-----------------------------------------------------------------------------
void ComputeViewMatrix( matrix3x4_t *pWorldToView, matrix3x4_t *pCameraToWorld, const Camera_t &camera )
{
	AngleMatrix( camera.m_angles, camera.m_origin, *pCameraToWorld );
	matrix3x4_t tmp;
	ConcatTransforms( *pCameraToWorld, g_ViewAlignMatrix, tmp );
	MatrixInvert( tmp, *pWorldToView );
}

void ComputeViewMatrix( matrix3x4_t *pWorldToView, matrix3x4_t *pCameraToWorld, 
						Vector const &vecOrigin,
						Vector const &vecForward, Vector const &vecLeft, Vector const &vecUp )
{
	MatrixSetColumn( vecForward, FORWARD_AXIS, *pCameraToWorld );
	MatrixSetColumn( vecLeft, LEFT_AXIS, *pCameraToWorld );
	MatrixSetColumn( vecUp, UP_AXIS, *pCameraToWorld );
	MatrixSetColumn( vecOrigin, ORIGIN, *pCameraToWorld );
	matrix3x4_t tmp;
	ConcatTransforms( *pCameraToWorld, g_ViewAlignMatrix, tmp );
	MatrixInvert( tmp, *pWorldToView );
}


void ComputeViewMatrix( matrix3x4_t *pWorldToView, const Camera_t &camera )
{
	matrix3x4_t cameraToWorld;
	ComputeViewMatrix( pWorldToView, &cameraToWorld, camera );
}

void ComputeViewMatrix( VMatrix *pWorldToView, const Camera_t &camera )
{
	matrix3x4_t transform, invTransform;
	AngleMatrix( camera.m_angles, camera.m_origin, transform );

	VMatrix matRotate( transform );

#ifndef YUP_ACTIVE
	VMatrix matRotateZ;
	MatrixBuildRotationAboutAxis( matRotateZ, Vector(0,0,1), -90 );
	MatrixMultiply( matRotate, matRotateZ, matRotate );

	VMatrix matRotateX;
	MatrixBuildRotationAboutAxis( matRotateX, Vector(1,0,0), 90 );
	MatrixMultiply( matRotate, matRotateX, matRotate );
	transform = matRotate.As3x4();
#else
	VMatrix matRotateUp;
	MatrixBuildRotationAboutAxis( matRotateUp, Vector(0,1,0), -180 );
	transform = matRotate.As3x4();
#endif

	MatrixInvert( transform, invTransform );
		
	pWorldToView->Init( invTransform );
}

void ComputeViewMatrix( VMatrix *pViewMatrix, const Vector &origin, const QAngle &angles )
{
	static VMatrix baseRotation;
	static bool bDidInit;

	if ( !bDidInit )
	{
		MatrixBuildRotationAboutAxis( baseRotation, Vector( 1, 0, 0 ), -90 );
		MatrixRotate( baseRotation, Vector( 0, 0, 1 ), 90 );
		bDidInit = true;
	}

	*pViewMatrix = baseRotation;
	MatrixRotate( *pViewMatrix, Vector( 1, 0, 0 ), -angles[2] );
	MatrixRotate( *pViewMatrix, Vector( 0, 1, 0 ), -angles[0] );
	MatrixRotate( *pViewMatrix, Vector( 0, 0, 1 ), -angles[1] );

	MatrixTranslate( *pViewMatrix, -origin );
}

void ComputeViewMatrix( VMatrix *pViewMatrix, const matrix3x4_t &matGameCustom )
{
	//translate game coordinates to rendering coordinates. Basically does the same as baseRotation in the other version of ComputeViewMatrix()
	pViewMatrix->m[0][0] = -matGameCustom.m_flMatVal[1][0];
	pViewMatrix->m[0][1] = -matGameCustom.m_flMatVal[1][1];
	pViewMatrix->m[0][2] = -matGameCustom.m_flMatVal[1][2];
	pViewMatrix->m[0][3] = -matGameCustom.m_flMatVal[1][3];

	pViewMatrix->m[1][0] = matGameCustom.m_flMatVal[2][0];
	pViewMatrix->m[1][1] = matGameCustom.m_flMatVal[2][1];
	pViewMatrix->m[1][2] = matGameCustom.m_flMatVal[2][2];
	pViewMatrix->m[1][3] = matGameCustom.m_flMatVal[2][3];

	pViewMatrix->m[2][0] = -matGameCustom.m_flMatVal[0][0];
	pViewMatrix->m[2][1] = -matGameCustom.m_flMatVal[0][1];
	pViewMatrix->m[2][2] = -matGameCustom.m_flMatVal[0][2];
	pViewMatrix->m[2][3] = -matGameCustom.m_flMatVal[0][3];

	//standard 4th row
	pViewMatrix->m[3][0] = pViewMatrix->m[3][1] = pViewMatrix->m[3][2] = 0.0f;
	pViewMatrix->m[3][3] = 1.0f;
}

void ComputeProjectionMatrix( VMatrix *pCameraToProjection, const Camera_t &camera, int width, int height )
{
	float flApsectRatio = (float)width / (float)height;
	ComputeProjectionMatrix( pCameraToProjection, camera.m_flZNear, camera.m_flZFar, camera.m_flFOVX, flApsectRatio );
}

void ComputeProjectionMatrix( VMatrix *pCameraToProjection, float flZNear, float flZFar, float flFOVX, float flAspectRatio )
{
	float halfWidth = tan( flFOVX * M_PI / 360.0 );
	float halfHeight = halfWidth / flAspectRatio;
	memset( pCameraToProjection, 0, sizeof( VMatrix ) );
	pCameraToProjection->m[0][0]  = 1.0f / halfWidth;
	pCameraToProjection->m[1][1]  = 1.0f / halfHeight;
	pCameraToProjection->m[2][2] = flZFar / ( flZNear - flZFar );
	pCameraToProjection->m[3][2] = -1.0f;
	pCameraToProjection->m[2][3] = flZNear * flZFar / ( flZNear - flZFar );
}

void ComputeProjectionMatrix( VMatrix *pCameraToProjection, float flZNear, float flZFar, float flFOVX, float flAspectRatio, 
							  float flClipSpaceBottomLeftX, float flClipSpaceBottomLeftY,
							  float flClipSpaceTopRightX, float flClipSpaceTopRightY )
{
	Vector pNearPoints[ 4 ];
	Vector vForward( 0, 0, 1 );
	Vector vUp( 0, 1, 0 );
	Vector vLeft( -1, 0, 0 );
	CalcFarPlaneCameraRelativePoints( pNearPoints, vForward, vUp, vLeft, flZNear, 
									  flFOVX, flAspectRatio,
									  flClipSpaceBottomLeftX, flClipSpaceBottomLeftY,
									  flClipSpaceTopRightX, flClipSpaceTopRightY );

	float l = pNearPoints[ 0 ].x;
	float r = pNearPoints[ 1 ].x;
	float b = pNearPoints[ 0 ].y;
	float t = pNearPoints[ 2 ].y;
	float zn = flZNear;
	float zf = flZFar;

	float flWidth = r - l;
	float flHeight = t - b;
	float flReverseDepth = zn - zf;

	memset( pCameraToProjection, 0, sizeof( VMatrix ) );

	pCameraToProjection->m[0][0] = ( 2.0f * zn ) / flWidth;
	pCameraToProjection->m[1][1] = ( 2.0f * zn ) / flHeight;
	pCameraToProjection->m[2][2] = zf / flReverseDepth;
	pCameraToProjection->m[0][2] = ( l + r ) / flWidth;
	pCameraToProjection->m[1][2] = ( t + b ) / flHeight;
	pCameraToProjection->m[2][3] = ( zn * zf ) / flReverseDepth;
	pCameraToProjection->m[3][2] = -1.0f;
	/*
	// this is the matrix we're going for here
	2*zn/(r-l)   0            0                0
	0            2*zn/(t-b)   0                0
	(l+r)/(r-l)  (t+b)/(t-b)  zf/(zn-zf)      -1
	0            0            zn*zf/(zn-zf)    0
	*/
}

//-----------------------------------------------------------------------------
// Computes the screen space position given a screen size
//-----------------------------------------------------------------------------
void ComputeScreenSpacePosition( Vector2D *pScreenPosition, const Vector &vecWorldPosition, 
								 const Camera_t &camera, int width, int height )
{
	VMatrix view, proj, viewproj;
	ComputeViewMatrix( &view, camera );
	ComputeProjectionMatrix( &proj, camera, width, height );
	MatrixMultiply( proj, view, viewproj );

	Vector vecScreenPos;
	Vector3DMultiplyPositionProjective( viewproj, vecWorldPosition, vecScreenPos );

	pScreenPosition->x = ( vecScreenPos.x + 1.0f ) * width / 2.0f;
	pScreenPosition->y = ( -vecScreenPos.y + 1.0f ) * height / 2.0f;
}

VMatrix ViewMatrixRH( Vector &vEye, Vector &vAt, Vector &vUp )
{
	Vector xAxis, yAxis;
	Vector zAxis = vEye - vAt;
	xAxis = CrossProduct( vUp, zAxis );
	yAxis = CrossProduct( zAxis, xAxis );
	xAxis.NormalizeInPlace();
	yAxis.NormalizeInPlace();
	zAxis.NormalizeInPlace();
	float flDotX = -DotProduct( xAxis, vEye );
	float flDotY = -DotProduct( yAxis, vEye );
	float flDotZ = -DotProduct( zAxis, vEye );

	// YUP_ACTIVE: This is ok
	VMatrix mRet( 
		xAxis.x, yAxis.x, zAxis.x, 0,
		xAxis.y, yAxis.y, zAxis.y, 0,
		xAxis.z, yAxis.z, zAxis.z, 0,
		flDotX, flDotY, flDotZ, 1 );
	return mRet.Transpose();
}


// Given populated camera params, generate view and proj matrices.
void MatricesFromCamera( VMatrix &mWorldToView, VMatrix &mProjection, const Camera_t &camera,
							   float flClipSpaceBottomLeftX, float flClipSpaceBottomLeftY,
							   float flClipSpaceTopRightX, float flClipSpaceTopRightY )
{
	matrix3x4_t cameraToWorld;	
	ComputeViewMatrix( &mWorldToView.As3x4(), &cameraToWorld, camera );

	if ( camera.IsOrthographic() )
	{
		mProjection = OrthoMatrixRH( camera.m_flWidth, camera.m_flHeight, camera.m_flZNear, camera.m_flZFar );
	}
	else
	{
		ComputeProjectionMatrix( &mProjection, camera.m_flZNear, camera.m_flZFar, camera.m_flFOVX, camera.m_flAspect, 
			flClipSpaceBottomLeftX, flClipSpaceBottomLeftY, flClipSpaceTopRightX, flClipSpaceTopRightY );
	}
}

// Generate frustum planes from viewproj matrix
void FrustumFromViewProj( Frustum_t *pFrustum, const VMatrix &mViewProj, const Vector &origin, bool bD3DClippingRange )
{
	VPlane planes[FRUSTUM_NUMPLANES];
	ExtractClipPlanesFromNonTransposedMatrix( mViewProj, planes, bD3DClippingRange );

	// Subtract the origin.
	for ( int i = 0; i < FRUSTUM_NUMPLANES; ++i )
	{
		planes[i].m_Dist = planes[i].m_Dist + DotProduct( planes[i].m_Normal, -origin );
	}

	pFrustum->SetPlanes( planes );	
}

// Generate frustum planes given view and proj matrices
void FrustumFromMatrices( Frustum_t *pFrustum, const VMatrix &mWorldToView, const VMatrix &mProjection, const Vector &origin, bool bD3DClippingRange )
{
	VMatrix viewProj;
	viewProj = ( mProjection * mWorldToView ); 

	FrustumFromViewProj( pFrustum, viewProj, origin, bD3DClippingRange );
}

VMatrix ViewProjFromVectors( const Vector &origin, float flNear, float flFar, float flFOV, float flAspect,
						 Vector const &vecForward, Vector const &vecLeft, Vector const &vecUp )
{
	
	matrix3x4_t	mCameraToWorld;
	matrix3x4_t	mWorldToView;
	ComputeViewMatrix( &mWorldToView, &mCameraToWorld, origin, vecForward, vecLeft, vecUp );
	VMatrix mProjection;
	ComputeProjectionMatrix( &mProjection, flNear, flFar, flFOV, flAspect );
	VMatrix mViewProj;
	mViewProj = (mProjection * VMatrix(mWorldToView));
	return mViewProj;
}



int CFrustum::CheckBoxAgainstNearAndFarPlanes( const VectorAligned &minBounds, const VectorAligned &maxBounds ) const
{
	// !!speed!! not super fast. change to use simd, inlining
	float flNear = 0;
	float flFar = 0;
	AABB_t aabb;
	aabb.m_vMinBounds = minBounds;
	aabb.m_vMaxBounds = maxBounds;
	Vector vZero( 0, 0, 0 );
	GetNearAndFarPlanesAroundBox( &flNear, &flFar, aabb, vZero );
	int nRet = 0;
	if ( flNear <= m_camera.m_flZNear )
	{
		nRet |= BOXCHECK_FLAGS_OVERLAPS_NEAR;
	}
	if ( flFar >= m_camera.m_flZFar )
	{
		nRet |= BOXCHECK_FLAGS_OVERLAPS_FAR;
	}
	return nRet;

}


void CFrustum::GetNearAndFarPlanesAroundBox( float *pNear, float *pFar, AABB_t const &inBox, Vector &vOriginShift ) const
{
	AABB_t box = inBox;
	box.m_vMinBounds -= vOriginShift;
	box.m_vMaxBounds -= vOriginShift;

	Vector vCorners[8];
	vCorners[0] = box.m_vMinBounds;
	vCorners[1] = Vector( box.m_vMinBounds.x, box.m_vMinBounds.y, box.m_vMaxBounds.z );
	vCorners[2] = Vector( box.m_vMinBounds.x, box.m_vMaxBounds.y, box.m_vMinBounds.z );
	vCorners[3] = Vector( box.m_vMinBounds.x, box.m_vMaxBounds.y, box.m_vMaxBounds.z );

	vCorners[4] = Vector( box.m_vMaxBounds.x, box.m_vMinBounds.y, box.m_vMinBounds.z );
	vCorners[5] = Vector( box.m_vMaxBounds.x, box.m_vMinBounds.y, box.m_vMaxBounds.z );
	vCorners[6] = Vector( box.m_vMaxBounds.x, box.m_vMaxBounds.y, box.m_vMinBounds.z );
	vCorners[7] = box.m_vMaxBounds;

	float flNear = FLT_MAX;//m_camera.m_flZNear;
	float flFar = -FLT_MAX;//m_camera.m_flZFar;
	for ( int i=0; i<8; ++i )
	{
		Vector vDelta = vCorners[i] - m_camera.m_origin;
		float flDist = DotProduct( m_forward, vDelta );
		flNear = MIN( flNear, flDist );
		flFar = MAX( flFar, flDist );
	}

	*pNear = flNear;
	*pFar = flFar;
}


void CFrustum::UpdateFrustumFromCamera()
{
	if ( !m_bDirty )
		return;

	ComputeViewMatrix( &m_worldToView, &m_cameraToWorld, m_camera );

	if ( m_camera.IsOrthographic() )
	{
		MatrixBuildOrtho( m_projection,	
				m_camera.m_flWidth  * m_flClipSpaceBottomLeftX * 0.5f,
				m_camera.m_flHeight * m_flClipSpaceTopRightY * 0.5f,
				m_camera.m_flWidth  * m_flClipSpaceTopRightX   * 0.5f,
				m_camera.m_flHeight * m_flClipSpaceBottomLeftY   * 0.5f, 
				m_camera.m_flZNear, m_camera.m_flZFar );
	}
	else
	{
		// Determine the extents
		ComputeProjectionMatrix( &m_projection, m_camera.m_flZNear, m_camera.m_flZFar, m_camera.m_flFOVX, m_camera.m_flAspect, 
								 m_flClipSpaceBottomLeftX, m_flClipSpaceBottomLeftY, m_flClipSpaceTopRightX, m_flClipSpaceTopRightY );	
	}

	CalcViewProj();
	ExtractDirectionVectors( &m_forward, &m_left, &m_up, m_cameraToWorld );

	FrustumFromViewProj( &m_frustumStruct, m_viewProj, m_camera.m_origin, true );

	m_bDirty = false;
}


void CFrustum::BuildFrustumFromVectors( const Vector &origin, float flNear, float flFar, float flFOV, float flAspect,
									   Vector const &vecForward, Vector const &vecLeft, Vector const &vecUp )
{
	InitCamera( origin, QAngle( 0, 0, 0 ), flNear, flFar, flFOV, flAspect );
	ComputeViewMatrix( &m_worldToView, &m_cameraToWorld, origin, vecForward, vecLeft, vecUp );
	ComputeProjectionMatrix( &m_projection, flNear, flFar, flFOV, flAspect );
	m_viewProj = (m_projection * VMatrix(m_worldToView));
	ExtractDirectionVectors( &m_forward, &m_left, &m_up, m_cameraToWorld );

	m_frustumStruct.CreatePerspectiveFrustum( vec3_origin, m_forward, -m_left, m_up, 
		flNear, flFar, flFOV, flAspect );

	MatrixInverseGeneral( m_viewProj, m_invViewProj );
	MatrixInverseGeneral( m_projection, m_invProjection );

	VMatrix worldToView( m_worldToView );

	VMatrix viewToWorld;
	worldToView.InverseGeneral( viewToWorld );
	m_cameraToWorld = viewToWorld.As3x4();

	viewToWorld.GetTranslation( m_camera.m_origin );

}


/// Given only the world->view and an ortho view->proj matrices, this helper method computes
/// the implied frustum values needed for orthographic shadow buffer rendering (but
/// should work with perspective projections too).  This is slow and general, but
/// it should guarantee a frustum in a consistent/sane state given any world->view and
/// view->proj matrices.
void CFrustum::BuildShadowFrustum( VMatrix &newWorldToView, VMatrix &newProj )
{
	SetView( newWorldToView );
	SetProj( newProj );
	CalcViewProj();

	VMatrix &viewToProj = m_projection;
	Assert( ( viewToProj.m[3][0] == 0.0f ) && ( viewToProj.m[3][1] == 0.0f )  && ( viewToProj.m[3][2] == 0.0f ) && ( viewToProj[3][3] == 1.0f ) );

	VMatrix worldToView( m_worldToView );

	VMatrix worldToCamera;
	MatrixMultiply( g_matViewToCameraMatrix, worldToView, worldToCamera );
	VMatrix cameraToWorld;
	MatrixInverseGeneral( worldToCamera, cameraToWorld );
	m_cameraToWorld = cameraToWorld.As3x4();

	// Compute camera location in world space.

	VMatrix viewToWorld;
	MatrixInverseGeneral( worldToView, viewToWorld );
	viewToWorld.GetTranslation( m_camera.m_origin );

	cameraToWorld.GetTranslation( m_camera.m_origin );

	MatrixToAngles( cameraToWorld, m_camera.m_angles );

	// forward/left/up - world relative coordinates, assuming an FPS camera sitting on an XY plane, Z is up
	MatrixGetRow( worldToCamera, FORWARD_AXIS, &m_forward );
	MatrixGetRow( worldToCamera, LEFT_AXIS, &m_left );
	MatrixGetRow( worldToCamera, UP_AXIS, &m_up );

	// Compute near/far planes, assuming D3D-style clipping range of [0,1]
	VMatrix projToView;
	viewToProj.InverseGeneral( projToView );

	Vector vNearPoint, vFarPoint; 
	projToView.V3Mul( Vector( 0.0f, 0.0f, 0.0f ), vNearPoint );
	projToView.V3Mul( Vector( 0.0f, 0.0f, 1.0f ), vFarPoint );
	m_camera.m_flZNear = fabs( vNearPoint.z );
	m_camera.m_flZFar = fabs( vFarPoint.z );
	m_camera.m_flAspect = 1.0f;
	m_camera.m_flFOVX = -1.0f;

	Vector vCornerPoints[2];
	
	// Y's are negated here because MatrixBuildOrtho() flips top/bottom!
	projToView.V3Mul( Vector( -1.0f,  1.0f, 0.0f ), vCornerPoints[0] );	// left/bottom
	projToView.V3Mul( Vector(  1.0f, -1.0f, 0.0f ), vCornerPoints[1] );	// right/top
	
	m_flClipSpaceBottomLeftX = vCornerPoints[0].x; 
	m_flClipSpaceBottomLeftY = vCornerPoints[0].y;
	m_flClipSpaceTopRightX = vCornerPoints[1].x;
	m_flClipSpaceTopRightY = vCornerPoints[1].y;
	
	m_camera.m_flWidth = 2.0f;
	m_camera.m_flHeight = 2.0f;

	// Now compute the frustum planes used for culling purposes. These planes are computed assuming the camera is already at the origin.
	VMatrix worldToCamLocalWorld;
	// This is confusing - vShadowCamPos is not negated here, because we need to compensate for the fact that the 
	// frustum culling code makes the cam pos the origin before culling by subtracting the camera's origin - so undo it.
	// Calc a viewproj matrix that has no g_ViewAlignMatrix matrix in it.
	MatrixBuildTranslation( worldToCamLocalWorld, m_camera.m_origin.x, m_camera.m_origin.y, m_camera.m_origin.z );
	VMatrix worldToCamLocalWorldToView( worldToView * worldToCamLocalWorld );
	VMatrix shadowCamLocalWorldToViewProj( viewToProj * worldToCamLocalWorldToView );
		
	VPlane pSixPlanes[FRUSTUM_NUMPLANES];
	
#if 0
	Vector actualOriginProjSpace( 0.0f, 0.0f, .5f );
	Vector actualOriginWorldSpace;
	VMatrix projToWorld;
	m_viewProj.InverseGeneral( projToWorld );
	projToWorld.V3Mul( actualOriginProjSpace, actualOriginWorldSpace );
	
	ExtractClipPlanesFromNonTransposedMatrix( m_viewProj, pSixPlanes, true );
	// Testing
	float flDots[6];
	for (uint i = 0; i < 6; i++)
	{
		flDots[i] = pSixPlanes[i].DistTo( actualOriginWorldSpace );
	}
#endif

	ExtractClipPlanesFromNonTransposedMatrix( shadowCamLocalWorldToViewProj, pSixPlanes, true );

	m_frustumStruct.SetPlanes( pSixPlanes );

	MatrixInverseGeneral( m_viewProj, m_invViewProj );
	MatrixInverseGeneral( m_projection, m_invProjection );

	m_bDirty = false;

	// This should be a no-op (ignoring FP precision) if all the above stuff was done right.
	//m_bDirty = true;
	//UpdateFrustumFromCamera();
}

void CFrustum::CalcFarPlaneCameraRelativePoints( Vector *p4PointsOut, float flFarPlane, 
												 float flClipSpaceBottomLeftX, float flClipSpaceBottomLeftY,
												 float flClipSpaceTopRightX, float flClipSpaceTopRightY ) const
{
	Vector vForward = CameraForward();
	Vector vUp = CameraUp();
	Vector vLeft = CameraLeft();

	float flFovX = GetCameraFOV();
	::CalcFarPlaneCameraRelativePoints( p4PointsOut, vForward, vUp, vLeft, flFarPlane, 
										flFovX, GetCameraAspect(),
										flClipSpaceBottomLeftX, flClipSpaceBottomLeftY,
										flClipSpaceTopRightX, flClipSpaceTopRightY );
}


// generates 8 vertices of the frustum
void Camera_t::ComputeGeometry( Vector *pVertsOut8, const Vector &vForward, const Vector &vLeft, const Vector &vUp ) const
{
	Vector vNearLeft, vFarLeft;
	Vector vNearUp, vFarUp;
	Vector vNear = m_origin + m_flZNear * vForward;
	Vector vFar = m_origin + m_flZFar * vForward;

	if ( IsOrthographic() )
	{
		vNearLeft = vLeft * m_flWidth;
		vNearUp = vUp * m_flHeight;
		vFarLeft = vNearLeft;
		vFarUp = vNearUp;
	}
	else
	{
		float flTanX = tan( DEG2RAD(m_flFOVX) * 0.5f );
		float flooAspect = 1.0f / m_flAspect;
		float flWidth = m_flZNear * flTanX;
		float flHeight = flWidth * flooAspect;

		vNearLeft = vLeft * flWidth;
		vNearUp = vUp * flHeight;

		float flFarWidth = m_flZFar * flTanX;
		float flFarHeight = flFarWidth * flooAspect;
		vFarLeft = vLeft * flFarWidth;
		vFarUp = vUp * flFarHeight;
	}

	pVertsOut8[0] = vNear + vNearLeft - vNearUp;
	pVertsOut8[1] = vNear - vNearLeft - vNearUp;
	pVertsOut8[2] = vNear + vNearLeft + vNearUp;
	pVertsOut8[3] = vNear - vNearLeft + vNearUp;

	pVertsOut8[4] = vFar + vFarLeft - vFarUp;
	pVertsOut8[5] = vFar - vFarLeft - vFarUp;
	pVertsOut8[6] = vFar + vFarLeft + vFarUp;
	pVertsOut8[7] = vFar - vFarLeft + vFarUp;
}

void Camera_t::ComputeGeometry( Vector *pVertsOut8 ) const
{
	Vector vForward, vLeft, vUp;
	AngleVectorsFLU( m_angles, &vForward, &vLeft, &vUp );
	ComputeGeometry( pVertsOut8, vForward, vLeft, vUp );
}

// generates 8 vertices of the frustum as bounds
void CFrustum::ComputeBounds( Vector *pMins, Vector *pMaxs ) const
{
	ClearBounds( *pMins, *pMaxs );
	Vector vPts[8];
	m_camera.ComputeGeometry( vPts, m_forward, m_left, m_up );

	for ( int i = 0; i < 8; i++ )
	{
		AddPointToBounds( vPts[i], *pMins, *pMaxs );
	}
}

static inline void InvertVMatrix( const VMatrix &src, VMatrix &dst )
{
	src.InverseGeneral( dst );
}

void CFrustum::CalcViewProj() 
{ 
	m_viewProj = ( m_projection * VMatrix( m_worldToView ) ); 
	InvertVMatrix( m_viewProj, m_invViewProj );
	InvertVMatrix( m_projection, m_invProjection );
}

float CFrustum::ComputeScreenSize( Vector vecOrigin, float flRadius ) const
{
	vecOrigin -= GetCameraPosition();
	float flDist = vecOrigin.Length();
	if ( flDist < flRadius )
	{
		return 1.0;											// eye inside sphere
	}
	float flSin = sin( DEG2RAD( MIN( GetCameraFOV(), 90.0 ) ) );
	return MIN( 1.0, flSin * ( flRadius / flDist ) );
	
}

void CFrustum::ViewToWorld( const Vector2D &vViewMinusOneToOne, Vector *pOutWorld )
{		
	Vector vView3D;
	vView3D.x = vViewMinusOneToOne.x;
	vView3D.y = vViewMinusOneToOne.y;
	vView3D.z = 0;

	const VMatrix &invViewProjMatrix = GetInvViewProj();
	Vector3DMultiplyPositionProjective( invViewProjMatrix, vView3D, *pOutWorld );	
}

void CFrustum::BuildRay( const Vector2D &vViewMinusOneToOne, Vector *pOutRayStart, Vector *pOutRayDirection )
{
	Vector vClickPoint;
	ViewToWorld( vViewMinusOneToOne, &vClickPoint );

	if ( !IsOrthographic() )
	{
		Camera_t camera = GetCameraStruct();

		Vector vRay = vClickPoint - camera.m_origin;
		VectorNormalize( vRay );

		*pOutRayStart = camera.m_origin;
		*pOutRayDirection = vRay;
	}
	else
	{
		*pOutRayStart = vClickPoint;
		ViewForward( *pOutRayDirection );
	}
}

void CFrustum::BuildFrustumFromParameters(
	const Vector &origin, const QAngle &angles, 
	float flNear, float flFar, float flFOV, float flAspect,
	const VMatrix &worldToView, const VMatrix &viewToProj )
{
	InitCamera( origin, angles, flNear, flFar, flFOV, flAspect );

	m_worldToView = worldToView.As3x4();

	VMatrix worldToCamera;
	MatrixMultiply( g_matViewToCameraMatrix, worldToView, worldToCamera );
	VMatrix cameraToWorld;
	MatrixInverseGeneral( worldToCamera, cameraToWorld );
	m_cameraToWorld = cameraToWorld.As3x4();
	
	m_projection = viewToProj;
	
	CalcViewProj();
						
	// forward/left/up - world relative coordinates, assuming an FPS camera sitting on an XY plane, Z is up
	MatrixGetRow( worldToCamera, FORWARD_AXIS, &m_forward );
	MatrixGetRow( worldToCamera, LEFT_AXIS, &m_left );
	MatrixGetRow( worldToCamera, UP_AXIS, &m_up );

	// Now compute the frustum planes used for culling purposes. These planes are computed assuming the camera is already at the origin.
	VMatrix worldToCamLocalWorld;
	// This is confusing - vShadowCamPos is not negated here, because we need to compensate for the fact that the 
	// frustum culling code makes the cam pos the origin before culling by subtracting the camera's origin - so undo it.
	MatrixBuildTranslation( worldToCamLocalWorld, m_camera.m_origin.x, m_camera.m_origin.y, m_camera.m_origin.z );
		
	VMatrix worldToCamLocalWorldToView( worldToView * worldToCamLocalWorld );
	VMatrix shadowCamLocalWorldToViewProj( viewToProj * worldToCamLocalWorldToView );
	VPlane pSixPlanes[FRUSTUM_NUMPLANES];
	ExtractClipPlanesFromNonTransposedMatrix( shadowCamLocalWorldToViewProj, pSixPlanes, true );

	m_frustumStruct.SetPlanes( pSixPlanes );
	
	m_bDirty = false;

	// This should be a no-op (ignoring FP precision) if all the above stuff was done right.
	//m_bDirty = true;
	//UpdateFrustumFromCamera();
}
