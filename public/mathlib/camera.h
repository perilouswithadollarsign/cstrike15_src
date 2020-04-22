//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef CAMERA_H
#define CAMERA_H

#ifdef _WIN32
#pragma once
#endif

#include <math.h>
#include <float.h>

// For vec_t, put this somewhere else?
#include "tier0/basetypes.h"
#include "mathlib/vector.h"

#include "tier0/dbg.h"
#include "mathlib/vector2d.h"
#include "mathlib/math_pfns.h"
#include "mathlib/vmatrix.h"
#include "mathlib/ssemath.h"
#include "datamap.h"
#include "mathlib/aabb.h"

#include "tier0/memalloc.h"
// declarations for camera and frustum

extern VMatrix g_matViewToCameraMatrix, g_matCameraToViewMatrix;

struct ALIGN16 Camera_t
{
	void Init( const Vector &origin, const QAngle &angles, float flNear, float flFar, float flFOV, float flAspect );
	void InitOrtho( const Vector &origin, const QAngle &angles, float flNear, float flFar, float flWidth, float flHeight );

	void InitViewParameters( const Vector &vOrigin, const QAngle &vAngles );

	void InitOrthoProjection( float flZNear, float flZFar, float flWidth, float flHeight );
	bool IsOrthographic() const;
	void InitPerspectiveProjection( float flZNear, float flZFar, float flFOVX, float flAspect );
	// generates 8 vertices of the frustum
	// vertex order is near plane (UL, UR, LL, LR), far plane (UL, UR, LL, LR)
	void ComputeGeometry( Vector *pVertsOut8 ) const;
	void ComputeGeometry( Vector *pVertsOut8, const Vector &vForward, const Vector &vLeft, const Vector &vUp ) const;

	inline bool operator ==( const Camera_t &other ) const;
	inline bool operator !=( const Camera_t &other ) const;

	Vector m_origin;
	QAngle m_angles;

	// FOV for X/width. 
	// This should be set to -1 to get an ortho projection,
	// in which case it'll use m_flWidth and m_flHeight.
	float m_flFOVX;
	float m_flAspect; // For Perspective

	float m_flZNear;
	float m_flZFar;

	float m_flWidth;	// For ortho.
	float m_flHeight;
} ALIGN16_POST;

inline void Camera_t::Init( const Vector &origin, const QAngle &angles, float flNear, float flFar, float flFOV, float flAspect )
{
	InitViewParameters( origin, angles );
	InitPerspectiveProjection( flNear, flFar, flFOV, flAspect );
	m_flWidth = -1;
	m_flHeight = -1;
}

inline void Camera_t::InitOrtho( const Vector &origin, const QAngle &angles, float flNear, float flFar, float flWidth, float flHeight )
{
	InitViewParameters( origin, angles );
	InitOrthoProjection( flNear, flFar, flWidth, flHeight );
}


inline void Camera_t::InitViewParameters( const Vector &vOrigin, const QAngle &vAngles )
{
	m_origin = vOrigin;
	m_angles = vAngles;
}

inline void Camera_t::InitOrthoProjection( float flZNear, float flZFar, float flWidth, float flHeight )
{
	m_flFOVX = -1;
	m_flZNear = flZNear;
	m_flZFar = flZFar;
	m_flWidth = flWidth;
	m_flHeight = flHeight;
}

inline bool Camera_t::IsOrthographic() const
{
	return m_flFOVX == -1;
}

inline void Camera_t::InitPerspectiveProjection( float flZNear, float flZFar, float flFOVX, float flAspect )
{
	m_flFOVX = flFOVX;
	m_flAspect = flAspect;
	m_flZNear = flZNear;
	m_flZFar = flZFar;
}

inline bool Camera_t::operator ==( const Camera_t &other ) const
{
	return ( m_origin == other.m_origin ) && 
		( m_angles == other.m_angles ) &&
		( m_flFOVX == other.m_flFOVX ) &&
		( m_flAspect == other.m_flAspect ) &&
		( m_flZNear == other.m_flZNear ) &&
		( m_flZFar == other.m_flZFar ) &&
		( m_flWidth == other.m_flWidth ) &&
		( m_flHeight == other.m_flHeight );
}


inline bool Camera_t::operator !=( const Camera_t &other ) const
{
	return !( *this == other );
}


///\name Functions to set up a VMatrix from various input view specifications
//@{
/// This maps the X/Y bounds into [-1,1] and flNear/flFar into [0,1]
inline VMatrix OrthoMatrixRH( float x1, float y1, float x2, float y2, float flNear, float flFar )
{
	float flDelta = flNear - flFar;

	float ix = 2.0f / ( x2 - x1 );
	float iy = 2.0f / ( y2 - y1 );

	VMatrix mRet( 
		ix,		 0,		0,				0,
		0,		 iy,	0,				0,
		0,		 0,		1.0f / flDelta,	flNear / flDelta,
		0,		 0,		0,	1 );

	return mRet;
}

inline VMatrix OrthoMatrixOffCenterRH( float x1, float y1, float x2, float y2, float flNear, float flFar )
{
	float flDelta = flNear - flFar;

	float ix = 2.0f / ( x2 - x1 );
	float iy = 2.0f / ( y2 - y1 );

	VMatrix mRet( 
		ix,		 0,		0,				-(ix * x1) - 1,
		0,		 iy,	0,				-(iy * y1) - 1,
		0,		 0,		1.0f / flDelta,	flNear / flDelta,
		0,		 0,		0,	1 );

	return mRet;
}

/// This maps the X/Y bounds into [-1,1] and flNear/flFar into [0,1]
/// This is left-handed for concatenation onto the viewproj matrix for app-tiling in source2
inline VMatrix OrthoMatrixOffCenterLH( float x1, float y1, float x2, float y2, float flNear, float flFar )
{
	float flDelta = flFar - flNear;

	float ix = 2.0f / ( x2 - x1 );
	float iy = 2.0f / ( y2 - y1 );

	VMatrix mRet( 
		ix,		 0,		0,				0,
		0,		 iy,	0,				0,
		0,		 0,		1.0f / flDelta,	0,
		( x1 + x2 )/( x1 - x2 ), ( y1 + y2 ) / ( y1 - y2 ),	-flNear / flDelta,	1 );

	return mRet;
}

/// This is the hammer wireframe widget version that inverts depth and shifts the xy coordinates
inline VMatrix OrthoMatrixHammerRH( float x1, float y1, float x2, float y2, float flNear, float flFar )
{
	float flDelta = flNear - flFar;

	float ix = 2.0f / ( x2 - x1 );
	float iy = 2.0f / ( y2 - y1 );

	VMatrix mRet( 
		ix,		 0,		0,				-(ix * x1) - 1,
		0,		 iy,	0,				-(iy * y1) - 1,
		0,		 0,		1.0f / flDelta,	-flNear / flDelta,
		0,		 0,		0,				1 );

	return mRet;
}



/// helper to calculate an ortho matrix for a view region centered at 0 of specified width and height
inline VMatrix OrthoMatrixRH( float flWidth, float flHeight, float flNear, float flFar )
{
	return OrthoMatrixRH( -flWidth/2, -flHeight/2, flWidth/2, flHeight/2, flNear, flFar );
}

/// calculate a view matrix given an origin, forward vector, and up vector
VMatrix ViewMatrixRH( Vector &vEye, Vector &vAt, Vector &vUp );

/// calculate a VMatrix from a camera_t
void ComputeViewMatrix( VMatrix *pWorldToView, const Camera_t& camera );
/// calculate a matrix3x4_t corresponding to a camera_t
void ComputeViewMatrix( matrix3x4_t *pWorldToView, const Camera_t& camera );
void ComputeViewMatrix( matrix3x4_t *pWorldToView, matrix3x4_t *pWorldToCamera, const Camera_t &camera );
void ComputeViewMatrix( matrix3x4_t *pWorldToView, matrix3x4_t *pCameraToWorld, 
	Vector const &vecOrigin,
	Vector const &vecForward, Vector const &vecLeft, Vector const &vecUp );

void ComputeViewMatrix( VMatrix *pViewMatrix, const Vector &origin, const QAngle &angles );
void ComputeViewMatrix( VMatrix *pViewMatrix, const matrix3x4_t &matGameCustom );

void ComputeProjectionMatrix( VMatrix *pCameraToProjection, const Camera_t& camera, int width, int height );
void ComputeProjectionMatrix( VMatrix *pCameraToProjection, float flZNear, float flZFar, float flFOVX, float flAspectRatio );
void ComputeProjectionMatrix( VMatrix *pCameraToProjection, float flZNear, float flZFar, float flFOVX, float flAspectRatio, 
	float flClipSpaceBottomLeftX, float flClipSpaceBottomLeftY,
	float flClipSpaceTopRightX, float flClipSpaceTopRightY );

//@}


void CalcFarPlaneCameraRelativePoints( Vector *p4PointsOut, Vector &vForward, Vector &vUp, Vector &vLeft, float flFarPlane, 
	float flFovX, float flFovY,
	float flClipSpaceBottomLeftX = -1.0f, float flClipSpaceBottomLeftY = -1.0f,
	float flClipSpaceTopRightX = 1.0f, float flClipSpaceTopRightY = 1.0f );

/// transform a point from 3d to 2d, given screen width + height
void ComputeScreenSpacePosition( Vector2D *pScreenPosition, const Vector &vecWorldPosition, 
	const Camera_t &camera, int width, int height );



// Functions to build frustum information given params
void MatricesFromCamera( VMatrix &mWorldToView, VMatrix &mProjection, const Camera_t &camera,
	float flClipSpaceBottomLeftX = -1.0f, float flClipSpaceBottomLeftY = -1.0f,
	float flClipSpaceTopRightX = 1.0f, float flClipSpaceTopRightY = 1.0f );
void FrustumFromViewProj( Frustum_t *pFrustum, const VMatrix &mViewProj, const Vector &origin, bool bD3DClippingRange = true );
void FrustumFromMatrices( Frustum_t *pFrustum, const VMatrix &mWorldToView, const VMatrix &mProjection, const Vector &origin, bool bD3DClippingRange = true );
// TODO: desired api.
//void MatrixFromFrustum( VMatrix *pViewProj, const Frustum_t &frustum );
VMatrix ViewProjFromVectors( const Vector &origin, float flNear, float flFar, float flFOV, float flAspect,
	Vector const &vecForward, Vector const &vecLeft, Vector const &vecUp );

enum EBoxOverlapFlags
{
	BOXCHECK_FLAGS_OVERLAPS_NEAR = 1,
	BOXCHECK_FLAGS_OVERLAPS_FAR = 2,
};




/// Class holding a camera, frustum planes, and transformation matrices, and methods to calculate
/// them and keep them in sync.
class CFrustum
{
public:
	CFrustum();
	~CFrustum(){}

	//--------------------------------------------------------------------------------------------------
	// Camera fxns
	//--------------------------------------------------------------------------------------------------
	void InitCamera( const Camera_t &Other )
	{
		m_camera = Other;
		m_bDirty = true;
	}

	// For an off-center projection:
	// flClipSpaceXXXX coordinates are in clip space, where ( -1,-1 ) is the bottom left corner of the screen
	// and ( 1,1 ) is the top right corner of the screen.
	void InitCamera( const Vector &origin, const QAngle &angles, float flNear, float flFar, float flFOV, float flAspect,
		float flClipSpaceBottomLeftX = -1.0f,	float flClipSpaceBottomLeftY = -1.0f, float flClipSpaceTopRightX = 1.0f, float flClipSpaceTopRightY = 1.0f )
	{
		m_camera.Init( origin, angles, flNear, flFar, flFOV, flAspect );

		m_flClipSpaceBottomLeftX = flClipSpaceBottomLeftX;
		m_flClipSpaceBottomLeftY = flClipSpaceBottomLeftY;
		m_flClipSpaceTopRightX = flClipSpaceTopRightX;
		m_flClipSpaceTopRightY = flClipSpaceTopRightY;
		m_bDirty = true;
	}

	void InitOrthoCamera( const Vector &origin, const QAngle &angles, float flNear, float flFar, float flWidth, float flHeight,
		float flClipSpaceBottomLeftX = -1.0f, float flClipSpaceBottomLeftY = -1.0f, float flClipSpaceTopRightX = 1.0f, float flClipSpaceTopRightY = 1.0f )
	{
		m_camera.InitOrtho( origin, angles, flNear, flFar, flWidth, flHeight );
		m_camera.m_flAspect = flWidth / flHeight;

		m_flClipSpaceBottomLeftX = flClipSpaceBottomLeftX;
		m_flClipSpaceBottomLeftY = flClipSpaceBottomLeftY;
		m_flClipSpaceTopRightX = flClipSpaceTopRightX;
		m_flClipSpaceTopRightY = flClipSpaceTopRightY;
		m_bDirty = true;
	}

	bool IsOrthographic() const { return m_camera.IsOrthographic(); }

	void SetCameraPosition( const Vector &origin );
	const Vector &GetCameraPosition() const { return m_camera.m_origin; }

	void SetCameraAngles( const QAngle &angles );
	const QAngle &GetCameraAngles() const { return m_camera.m_angles; }

	// Sets the distance from the camera to the near/far clipping plane in world units.
	void SetCameraNearFarPlanes( float flNear, float flFar );
	void GetCameraNearFarPlanes( float &flNear, float &flFar ) const { flNear = m_camera.m_flZNear; flFar = m_camera.m_flZFar; }

	// Sets the distance from the camera to the near clipping plane in world units.
	void SetCameraNearPlane( float flNear );
	float GetCameraNearPlane() const { return m_camera.m_flZNear; }

	// Sets the distance from the camera to the far clipping plane in world units.
	void SetCameraFarPlane( float flFar );
	float GetCameraFarPlane() const { return m_camera.m_flZFar; }

	// Set the field of view (in degrees)
	void SetCameraFOV( float flFOV );
	float GetCameraFOV() const { return m_camera.m_flFOVX; }

	void SetCameraWidthHeight( float flWidth, float flHeight );
	void GetCameraWidthHeight( float &width, float &height ) const { width = m_camera.m_flWidth; height = m_camera.m_flHeight; }

	void SetCameraWidth( float flWidth );
	float GetCameraWidth() const { return m_camera.m_flWidth; }

	void SetCameraHeight( float flHeight );
	float GetCameraHeight() const { return m_camera.m_flHeight; }

	void SetCameraAspect( float flAspect );
	float GetCameraAspect() const { return m_camera.m_flAspect; }

	/// Returns mask of BOXCHECK_FLAGS_xxx indicating the status of the box with respect to this
	/// frustum's near and far clip planes.
	int CheckBoxAgainstNearAndFarPlanes( const VectorAligned &minBounds, const VectorAligned &maxBounds ) const;

	/// given an AABB, return the values of the near and far plane which will enclose the box
	void GetNearAndFarPlanesAroundBox( float *pNear, float *pFar, AABB_t const &inBox, Vector &vOriginShift ) const;

	/// Compute the approximate size of a sphere. Rough calculation suitable for lod selection,
	/// etc.  Result is in terms of approximate % coverage of the viewport, not taking clipping
	/// into account.
	float ComputeScreenSize( Vector vecOrigin, float flRadius ) const;

	/// Return the Sin of the FOV
	FORCEINLINE float SinFOV( void ) const { return sin( DEG2RAD( GetCameraFOV() ) ); }

	const Camera_t &GetCameraStruct() const { return m_camera; }

	//--------------------------------------------------------------------------------------------------
	// Frustum fxns
	//--------------------------------------------------------------------------------------------------

	void SetFrustumStruct( const Frustum_t &frustumStruct ) { m_frustumStruct = frustumStruct; }

	// Camera oriented directions
	const Vector &CameraForward() const { return m_forward; }
	const Vector &CameraLeft() const { return m_left; }
	const Vector &CameraUp() const { return m_up; }

	// View oriented directions i.e. view align matrix has been applied
	void ViewForward( Vector& vViewForward ) const;
	void ViewLeft( Vector& vViewLeft ) const;
	void ViewUp(  Vector& vViewUp ) const;

	void SetView( VMatrix &mWorldToView ) { m_worldToView = mWorldToView.As3x4(); }
	const matrix3x4_t &GetView() const { return m_worldToView; }

	void SetProj( VMatrix &mProj ) { m_projection = mProj; }
	const VMatrix &GetProj() const { return m_projection; }
	const VMatrix &GetInvProj() const { return m_invProjection; }

	// The viewProj and invViewProj matrices are NOT transposed.
	void SetViewProj( VMatrix &viewProj ) { m_viewProj = viewProj; }
	const VMatrix &GetViewProj() const { return m_viewProj; }
	VMatrix GetViewProjTranspose() const { return m_viewProj.Transpose(); }

	void SetInvViewProj( VMatrix &invViewProj ) { m_invViewProj = invViewProj; }
	const VMatrix &GetInvViewProj() const { return m_invViewProj; }
	VMatrix GetInvViewProjTranspose() const { return m_invViewProj.Transpose(); }

	bool BoundingVolumeIntersectsFrustum( AABB_t const &box ) const;
	bool BoundingVolumeIntersectsFrustum( Vector const &mins, Vector const &maxes ) const;
	bool BoundingVolumeIntersectsFrustum( AABB_t const &box, Vector &vOriginShift ) const;

	/// Update the matrix and clip planes for this frustum to reflect the state of the embedded Camera_t
	void UpdateFrustumFromCamera();

	/// build a full frustum from rotation vectors plus camera vars
	void BuildFrustumFromVectors( const Vector &origin, float flNear, float flFar, float flFOV, float flAspect,
		Vector const &vecForward, Vector const &vecLeft, Vector const &vecUp );

	void BuildShadowFrustum( VMatrix &newWorldToView, VMatrix &newProj );

	void BuildFrustumFromParameters(
		const Vector &origin, const QAngle &angles, 
		float flNear, float flFar, float flFOV, float flAspect,
		const VMatrix &worldToView, const VMatrix &viewToProj );

	/// calculate the projection of the 4 view-frustum corner rays onto a plane
	void CalcFarPlaneCameraRelativePoints( Vector *p4PointsOut, float flFarPlane, 
		float flClipSpaceBottomLeftX = -1.0f, float flClipSpaceBottomLeftY = -1.0f,
		float flClipSpaceTopRightX = 1.0f, float flClipSpaceTopRightY = 1.0f ) const;


	/// concatenate the projection and view matrices, and also update the inverse view and
	/// projection matrices 
	void CalcViewProj( );

	/// Transform a point from world space into camera space.
	Vector4D TransformPointToHomogenousViewCoordinates( Vector const &pnt ) const;

	void SetClipSpaceBounds( float flClipSpaceBottomLeftX, float flClipSpaceBottomLeftY, float flClipSpaceTopRightX, float flClipSpaceTopRightY );

	void GetClipSpaceBounds( float &flClipSpaceBottomLeftX, float &flClipSpaceBottomLeftY, float &flClipSpaceTopRightX, float &flClipSpaceTopRightY ) const
	{
		flClipSpaceBottomLeftX = m_flClipSpaceBottomLeftX;
		flClipSpaceBottomLeftY = m_flClipSpaceBottomLeftY;
		flClipSpaceTopRightX = m_flClipSpaceTopRightX;
		flClipSpaceTopRightY = m_flClipSpaceTopRightY;
	}

	// NOTE: Not tested with ortho projections
	void ComputeBounds( Vector *pMins, Vector *pMaxs ) const;
	void ComputeGeometry( Vector *pVertsOut8 ) const { m_camera.ComputeGeometry( pVertsOut8, m_forward, m_left, m_up ); }

	const Frustum_t &GetFrustumStruct() const { return m_frustumStruct; }

	//--------------------------------------------------------------------------------------------------
	void ViewToWorld( const Vector2D &vViewMinusOneToOne, Vector *pOutWorld );
	void BuildRay( const Vector2D &vViewMinusOneToOne, Vector *pOutRayStart, Vector *pOutRayDirection );

protected:

	Camera_t m_camera; // NOTE: SIMD-aligned

	Frustum_t m_frustumStruct; // NOTE: SIMD-aligned

	// For off-center projection
	float m_flClipSpaceBottomLeftX;
	float m_flClipSpaceBottomLeftY;
	float m_flClipSpaceTopRightX;
	float m_flClipSpaceTopRightY;
		
	Vector m_forward;
	Vector m_left;
	Vector m_up;

	// Camera/view matrices. (The space order is: world->camera->view->projection->screenspace.)
	matrix3x4_t	m_cameraToWorld;	// camera->world (NOT view->world, and not the inverse of m_worldToView)
	matrix3x4_t	m_worldToView;		// world->view
	
	// Projection matrices.
	VMatrix m_projection;			// view->proj
	VMatrix m_invProjection;		// proj->view

	// Combined world->projection matrices.
	VMatrix m_viewProj;				// world->proj
	VMatrix m_invViewProj;			// proj->world

	bool m_bDirty;	
};

inline CFrustum::CFrustum()
{
	InitCamera( Vector( 0, 0, 0 ), QAngle( 0, 0, 0 ), 10, 100, 90, 1.0f );
	V_memset( &m_frustumStruct, 0, sizeof(Frustum_t) );

	m_forward.Init( 0, 0, 0 );
	m_left.Init( 0, 0, 0 );
	m_up.Init( 0, 0, 0 );

	V_memset( &m_cameraToWorld, 0, sizeof(matrix3x4_t) );
	V_memset( &m_worldToView, 0, sizeof(matrix3x4_t) );
	V_memset( &m_projection, 0, sizeof(VMatrix) );
	V_memset( &m_invProjection, 0, sizeof(VMatrix) );
	V_memset( &m_viewProj, 0, sizeof(VMatrix) );
	V_memset( &m_invViewProj, 0, sizeof(VMatrix) );

	m_bDirty = true;
}

inline void CFrustum::SetCameraPosition( const Vector &origin ) 
{ 
	if ( m_camera.m_origin == origin )
		return;

	m_camera.m_origin = origin;
	Assert( origin.IsValid() && origin.IsReasonable() );

	m_bDirty = true; 
}

inline void CFrustum::SetCameraAngles( const QAngle &angles ) 
{ 
	if ( m_camera.m_angles == angles )
		return;

	m_camera.m_angles = angles;
	m_bDirty = true; 
}


inline void CFrustum::SetCameraNearFarPlanes( float flNear, float flFar ) 
{ 
	if ( ( m_camera.m_flZNear == flNear ) && ( m_camera.m_flZFar == flFar ) )
		return;

	m_camera.m_flZNear = flNear; 
	m_camera.m_flZFar = flFar;
	m_bDirty = true; 
}

inline void CFrustum::SetCameraNearPlane( float flNear ) 
{ 
	if ( m_camera.m_flZNear == flNear ) 
		return;

	m_camera.m_flZNear = flNear; 
	m_bDirty = true; 
}

inline void CFrustum::SetCameraFarPlane( float flFar ) 
{ 
	if ( m_camera.m_flZFar == flFar ) 
		return;

	m_camera.m_flZFar = flFar;
	m_bDirty = true; 
}

/// Set the field of view (in degrees)
inline void CFrustum::SetCameraFOV( float flFOV ) 
{ 
	if ( m_camera.m_flFOVX == flFOV ) 
		return;

	m_camera.m_flFOVX = flFOV;
	m_bDirty = true; 
}

inline void CFrustum::SetCameraWidthHeight( float flWidth, float flHeight ) 
{ 
	if ( ( m_camera.m_flWidth == flWidth ) && ( m_camera.m_flHeight == flHeight ) )
		return;

	m_camera.m_flWidth = flWidth; 
	m_camera.m_flHeight = flHeight; 
	m_camera.m_flAspect = m_camera.m_flWidth / m_camera.m_flHeight; 
	m_bDirty = true; 
}

inline void CFrustum::SetCameraWidth( float flWidth ) 
{ 
	if ( m_camera.m_flWidth == flWidth ) 
		return;

	m_camera.m_flWidth = flWidth; 
	m_camera.m_flAspect = m_camera.m_flWidth / m_camera.m_flHeight; 
	m_bDirty = true; 
}

inline void CFrustum::SetCameraHeight( float flHeight ) 
{ 
	if ( m_camera.m_flHeight == flHeight ) 
		return;

	m_camera.m_flHeight = flHeight; 
	m_camera.m_flAspect = m_camera.m_flWidth / m_camera.m_flHeight;
	m_bDirty = true; 
}

inline void CFrustum::SetCameraAspect( float flAspect ) 
{ 
	if ( m_camera.m_flAspect == flAspect ) 
		return;

	m_camera.m_flAspect = flAspect;
	m_bDirty = true; 
}


inline bool CFrustum::BoundingVolumeIntersectsFrustum( AABB_t const &box ) const
{
	Vector vMins = box.m_vMinBounds - m_camera.m_origin;
	Vector vMaxs = box.m_vMaxBounds - m_camera.m_origin;
	return m_frustumStruct.Intersects( vMins, vMaxs );
}

inline bool CFrustum::BoundingVolumeIntersectsFrustum( Vector const &mins, Vector const &maxes ) const
{
	Vector vMins = mins - m_camera.m_origin;
	Vector vMaxs = maxes - m_camera.m_origin;
	return m_frustumStruct.Intersects( vMins, vMaxs );
}

inline bool CFrustum::BoundingVolumeIntersectsFrustum( AABB_t const &box, Vector &vOriginShift ) const
{
	Vector vMins = box.m_vMinBounds - m_camera.m_origin - vOriginShift;
	Vector vMaxs = box.m_vMaxBounds - m_camera.m_origin - vOriginShift;
	return m_frustumStruct.Intersects( vMins, vMaxs );
}

inline Vector4D CFrustum::TransformPointToHomogenousViewCoordinates( Vector const &pnt ) const
{
	Vector4D v4Rslt;
	GetViewProj().V4Mul( Vector4D( pnt.x, pnt.y, pnt.z, 1.0 ), v4Rslt );
	return v4Rslt;
}

inline void CFrustum::ViewForward( Vector& vViewForward ) const 
{ 
	Vector vFrustumDir;
	MatrixGetRow( VMatrix( m_worldToView ), Z_AXIS, &vFrustumDir );
	VectorNormalize( vFrustumDir );
	vFrustumDir = -vFrustumDir;
	vViewForward = vFrustumDir;
}

inline void CFrustum::ViewLeft( Vector& vViewLeft ) const 
{ 
	Vector vFrustumDir;
	MatrixGetRow( VMatrix( m_worldToView ), X_AXIS, &vFrustumDir ); 
	VectorNormalize( vFrustumDir );
	vViewLeft = -vFrustumDir;
}

inline void CFrustum::ViewUp(  Vector& vViewUp ) const 
{ 
	Vector vFrustumDir;
	MatrixGetRow( VMatrix( m_worldToView ), Y_AXIS, &vFrustumDir );
	VectorNormalize( vFrustumDir );
	vViewUp = vFrustumDir;
}


inline void CFrustum::SetClipSpaceBounds( float flClipSpaceBottomLeftX, float flClipSpaceBottomLeftY, float flClipSpaceTopRightX, float flClipSpaceTopRightY )
{
	m_flClipSpaceBottomLeftX = flClipSpaceBottomLeftX;
	m_flClipSpaceBottomLeftY = flClipSpaceBottomLeftY;
	m_flClipSpaceTopRightX = flClipSpaceTopRightX;
	m_flClipSpaceTopRightY = flClipSpaceTopRightY;
	m_bDirty = true;
}

#endif // CAMERA_H

