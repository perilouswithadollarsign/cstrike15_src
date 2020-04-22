//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
//=============================================================================//

#ifndef VIEW_SHARED_H
#define VIEW_SHARED_H

#ifdef _WIN32
#pragma once
#endif

#include "convar.h"
#include "mathlib/vector.h"
#include "mathlib/camera.h"
#include "materialsystem/MaterialSystemUtil.h"

class CVolumeCuller;

//-----------------------------------------------------------------------------
// Flags passed in with view setup
//-----------------------------------------------------------------------------
enum ClearFlags_t
{
	VIEW_CLEAR_COLOR = 0x1,
	VIEW_CLEAR_DEPTH = 0x2,
	VIEW_CLEAR_FULL_TARGET = 0x4,
	VIEW_NO_DRAW = 0x8,
	VIEW_CLEAR_OBEY_STENCIL = 0x10, // Draws a quad allowing stencil test to clear through portals
	VIEW_CLEAR_STENCIL = 0x20,
};


enum MotionBlurMode_t
{
	MOTION_BLUR_DISABLE = 1,
	MOTION_BLUR_GAME = 2,			// Game uses real-time inter-frame data
	MOTION_BLUR_SFM = 3				// Use SFM data passed in CViewSetup structure
};

//-----------------------------------------------------------------------------
// Purpose: Renderer setup data.  
//-----------------------------------------------------------------------------
class CViewSetup
{
public:
	CViewSetup()
	{
		m_flAspectRatio = 0.0f;

		// These match mat_dof convars
		m_flNearBlurDepth = 20.0;
		m_flNearFocusDepth = 100.0;
		m_flFarFocusDepth = 250.0;
		m_flFarBlurDepth = 1000.0;
		m_flNearBlurRadius = 10.0;
		m_flFarBlurRadius = 5.0;
		m_nDoFQuality = 0;

		m_bRenderToSubrectOfLargerScreen = false;
		m_bDoBloomAndToneMapping = true;
		m_nMotionBlurMode = MOTION_BLUR_GAME;
		m_bDoDepthOfField = false;
		m_bHDRTarget = false;
		m_bOffCenter = false;
		m_bCacheFullSceneState = false;
		m_bDrawWorldNormal = false;
		m_bCullFrontFaces = false;
		m_bCustomViewMatrix = false;
//		m_bUseExplicitViewVector = false;
		m_bCustomProjMatrix = false;
		m_bCSMView = false;
		m_pCSMVolumeCuller = NULL;
	}

	float ComputeViewMatrices( VMatrix *pWorldToView, VMatrix *pViewToProjection, VMatrix *pWorldToProjection ) const;
	
// shared by 2D & 3D views

	// left side of view window
	int			x;					
	int			m_nUnscaledX;
	// top side of view window
	int			y;					
	int			m_nUnscaledY;
	// width of view window
	int			width;				
	int			m_nUnscaledWidth;
	// height of view window
	int			height;				
	int			m_nUnscaledHeight;

	// X360 csm resolve offsets
#if defined(_X360)
	int			xCsmDstOffset;
	int			yCsmDstOffset;
#endif

// the rest are only used by 3D views

	// Orthographic projection?
	bool		m_bOrtho;
	// View-space rectangle for ortho projection.
	float		m_OrthoLeft;		
	float		m_OrthoTop;
	float		m_OrthoRight;
	float		m_OrthoBottom;

	// Note: This really should be called "m_bCustomCameraMatrix", because m_matCustomViewMatrix is interpreted as world->camera, not world->view.
	bool		m_bCustomViewMatrix;
	matrix3x4_t	m_matCustomViewMatrix;

	bool		m_bCustomProjMatrix;
	VMatrix		m_matCustomProjMatrix;

	const CVolumeCuller *m_pCSMVolumeCuller;
	
	// horizontal FOV in degrees
	float		fov;				
	// horizontal FOV in degrees for in-view model
	float		fovViewmodel;		

	// 3D origin of camera
	Vector		origin;					

	// heading of camera (pitch, yaw, roll)
	QAngle		angles;				
	// local Z coordinate of near plane of camera
	float		zNear;			
		// local Z coordinate of far plane of camera
	float		zFar;			

	// local Z coordinate of near plane of camera ( when rendering view model )
	float		zNearViewmodel;		
	// local Z coordinate of far plane of camera ( when rendering view model )
	float		zFarViewmodel;		

	// The aspect ratio to use for computing the perspective projection matrix
	// (0.0f means use the viewport)
	float		m_flAspectRatio;

	// Camera settings to control depth of field
	float		m_flNearBlurDepth;
	float		m_flNearFocusDepth;
	float		m_flFarFocusDepth;
	float		m_flFarBlurDepth;
	float		m_flNearBlurRadius;
	float		m_flFarBlurRadius;
	int			m_nDoFQuality;

	// Camera settings to control motion blur
	MotionBlurMode_t	m_nMotionBlurMode;
	float	m_flShutterTime;				// In seconds
	Vector	m_vShutterOpenPosition;			// Start of frame or "shutter open"
	QAngle	m_shutterOpenAngles;			//
	Vector	m_vShutterClosePosition;		// End of frame or "shutter close"
	QAngle	m_shutterCloseAngles;			// 

	// Controls for off-center projection (needed for poster rendering)
	float		m_flOffCenterTop;
	float		m_flOffCenterBottom;
	float		m_flOffCenterLeft;
	float		m_flOffCenterRight;
	bool		m_bOffCenter:1;

	// set to true if this is to draw into a subrect of the larger screen
	// this really is a hack, but no more than the rest of the way this class is used
	bool		m_bRenderToSubrectOfLargerScreen:1;

	// Controls that the SFM needs to tell the engine when to do certain post-processing steps
	bool		m_bDoBloomAndToneMapping:1;
	bool		m_bDoDepthOfField:1;
	bool		m_bHDRTarget:1;
	bool		m_bDrawWorldNormal:1;
	bool		m_bCullFrontFaces:1;

	// Cached mode for certain full-scene per-frame varying state such as sun entity coverage
	bool		m_bCacheFullSceneState:1;

	// True if this is a CSM depth view. The CSM world->view matrix doesn't have an XY translation (that's moved into the CSM ortho view->projection 
	// matrix to address continuity issues), so the usual assumptions made about camera/view space do not necessarily apply.
	bool		m_bCSMView:1;
};

inline float CViewSetup::ComputeViewMatrices( VMatrix *pWorldToView, VMatrix *pViewToProjection, VMatrix *pWorldToProjection ) const
{
	float flAspectRatio = m_flAspectRatio;
	if ( flAspectRatio == 0.0f )
	{
		flAspectRatio = (height != 0) ? ( (float)width / (float)height ) : 1.0f;
	}

	if( !m_bCustomViewMatrix )
	{
		ComputeViewMatrix( pWorldToView, origin, angles );
	}
	else
	{
		ComputeViewMatrix( pWorldToView, m_matCustomViewMatrix );
	}

	if ( m_bCustomProjMatrix )
	{
		*pViewToProjection = m_matCustomProjMatrix;
	}
	else if ( m_bOrtho )
	{
		MatrixBuildOrtho( *pViewToProjection, m_OrthoLeft, m_OrthoTop, 
			m_OrthoRight, m_OrthoBottom, zNear, zFar );
	}
	else if ( m_bOffCenter ) // Off-center projection, useful for AA jitter and tiled output of posters
	{
		MatrixBuildPerspectiveOffCenterX( *pViewToProjection, fov, flAspectRatio, 
			zNear, zFar, m_flOffCenterBottom, m_flOffCenterTop,
			m_flOffCenterLeft, m_flOffCenterRight );
	}
	else
	{
		MatrixBuildPerspectiveX( *pViewToProjection, fov, flAspectRatio, zNear, zFar );
	}

	MatrixMultiply( *pViewToProjection, *pWorldToView, *pWorldToProjection );

	return flAspectRatio;
}

#endif // VIEW_SHARED_H
