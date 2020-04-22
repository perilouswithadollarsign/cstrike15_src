//======= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======
//
// A class representing a camera
//
//===============================================================================
#ifndef DMECAMERA_H
#define DMECAMERA_H
#ifdef _WIN32
#pragma once
#endif

#include "movieobjects/dmedag.h"

enum EOrthoAxes
{
	AXIS_X = 0,
	AXIS_Y,
	AXIS_Z,
	AXIS_X_NEG,
	AXIS_Y_NEG,
	AXIS_Z_NEG,

	AXIS_COUNT, // == 6
};

//-----------------------------------------------------------------------------
// A class representing a camera
//-----------------------------------------------------------------------------
class CDmeCamera : public CDmeDag
{
	DEFINE_ELEMENT( CDmeCamera, CDmeDag );

public:
	// Sets up render state in the material system for rendering
	// This includes the view matrix and the projection matrix
	void SetupRenderState( int nDisplayWidth, int nDisplayHeight, bool bUseEngineCoordinateSystem = false );

	// accessors for generated matrices
	void GetViewMatrix( VMatrix &view, bool bUseEngineCoordinateSystem = false );
	void GetProjectionMatrix( VMatrix &proj, int width, int height );
	void GetViewProjectionInverse( VMatrix &viewprojinv, int width, int height );
	void ComputeScreenSpacePosition( const Vector &vecWorldPosition, int width, int height, Vector2D *pScreenPosition );

	// Returns the x FOV (the full angle)
	float GetFOVx() const;
	void SetFOVx( float fov );

	// Near and far Z
	float GetNearZ() const;
	void SetNearZ( float zNear );
	float GetFarZ() const;
	void SetFarZ( float zFar );

	// Returns the focal distance in inches
	float GetFocalDistance() const;

	// Sets the focal distance in inches
	void SetFocalDistance( const float &fFocalDistance );

	// Zero-parallax distance for stereo rendering
	float GetZeroParallaxDistance() const;
	void SetZeroParallaxDistance( const float &fZeroParallaxDistance );

	// Eye separation for stereo rendering
	float GetEyeSeparation() const;
	void SetEyeSeparation( const float &flEyeSeparation );

	// Returns the aperture size in inches
	float GetAperture() const;

	// Returns the shutter speed in seconds
	DmeTime_t GetShutterSpeed() const;

	// Returns the tone map scale
	float GetToneMapScale() const;

	// Returns the camera's Ambient occlusion bias
	float GetAOBias() const;

	// Returns the camera's Ambient occlusion sgrength
	float GetAOStrength() const;

	// Returns the camera's Ambient occlusion radius
	float GetAORadius() const;

	// Returns the tone map scale
	float GetBloomScale() const;

	// Returns the tone map scale
	float GetBloomWidth() const;

	// Returns the view direction
	void GetViewDirection( Vector *pDirection );

	// Returns Depth of Field quality level
	int GetDepthOfFieldQuality() const;

	// Returns the Motion Blur quality level
	int GetMotionBlurQuality() const;

	// Ortho stuff
	void			OrthoUpdate();
	const matrix3x4_t &GetOrthoTransform() const;
	const Vector	  &GetOrthoAbsOrigin() const;
	const QAngle	  &GetOrthoAbsAngles() const;

	void FromCamera( CDmeCamera *pCamera );
	void ToCamera( CDmeCamera *pCamera );

private:
	// Loads the material system view matrix based on the transform
	void LoadViewMatrix( bool bUseEngineCoordinateSystem );

	// Loads the material system projection matrix based on the fov, etc.
	void LoadProjectionMatrix( int nDisplayWidth, int nDisplayHeight );

	// Sets the studiorender state
	void LoadStudioRenderCameraState();

	CDmaVar< float > m_flFieldOfView;
	CDmaVar< float > m_zNear;
	CDmaVar< float > m_zFar;

	CDmaVar< float > m_flFocalDistance;
	CDmaVar< float > m_flZeroParallaxDistance;
	CDmaVar< float > m_flEyeSeparation;
	CDmaVar< float > m_flAperture;
	CDmaTime         m_shutterSpeed;
	CDmaVar< float > m_flToneMapScale;
	CDmaVar< float > m_flAOBias;
	CDmaVar< float > m_flAOStrength;
	CDmaVar< float > m_flAORadius;
	CDmaVar< float > m_flBloomScale;
	CDmaVar< float > m_flBloomWidth;

	CDmaVar< int >	m_nDoFQuality;
	CDmaVar< int >	m_nMotionBlurQuality;

public:
	CDmaVar< bool >		m_bOrtho;

	// Ortho vars
	CDmaVar< Vector >	m_vecLookAt[ AXIS_COUNT ];
	CDmaVar< float	>	m_flScale[ AXIS_COUNT ];
	CDmaVar< float	>	m_flDistance;
	CDmaVar< int >		m_nAxis;
	CDmaVar< bool > 	m_bWasBehindFrustum;

private:  // private ortho vars
	Vector			m_vecAxis;
	Vector			m_vecOrigin;
	QAngle			m_angRotation;
	matrix3x4_t		m_Transform;

};


#endif // DMECAMERA_H
