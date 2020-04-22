//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef RENDER_H
#define RENDER_H

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector.h"

// render.h -- public interface to refresh functions

extern float r_blend; // Global blending factor for the current entity
extern float r_colormod[3]; // Global color modulation for the current entity
extern bool g_bIsBlendingOrModulating;

#ifndef DEDICATED
#include "cl_splitscreen.h" // ISplitScreen
#endif

//-----------------------------------------------------------------------------
// Current view
//-----------------------------------------------------------------------------
inline const Vector &CurrentViewOrigin()
{
	extern Vector g_CurrentViewOrigin;
#ifndef DEDICATED
	extern bool g_bCanAccessCurrentView;
	Assert( g_bCanAccessCurrentView );
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
#endif
	return g_CurrentViewOrigin;
}

inline const Vector &CurrentViewForward()
{
	extern Vector g_CurrentViewForward;
	extern Vector g_CurrentViewOrigin;
#ifndef DEDICATED
	extern bool g_bCanAccessCurrentView;
	Assert( g_bCanAccessCurrentView );
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
#endif
	return g_CurrentViewForward;
}

inline const Vector &CurrentViewRight()
{
	extern Vector g_CurrentViewRight;
#ifndef DEDICATED
	extern bool g_bCanAccessCurrentView;
	Assert( g_bCanAccessCurrentView );
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
#endif
	return g_CurrentViewRight;
}

inline const Vector &CurrentViewUp()
{
	extern Vector g_CurrentViewUp;
#ifndef DEDICATED
	extern bool g_bCanAccessCurrentView;
	Assert( g_bCanAccessCurrentView );
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
#endif
	return g_CurrentViewUp;
}


extern Vector g_MainViewOrigin[ MAX_SPLITSCREEN_CLIENTS ];
extern Vector g_MainViewForward[ MAX_SPLITSCREEN_CLIENTS ];
extern Vector g_MainViewRight[ MAX_SPLITSCREEN_CLIENTS ];
extern Vector g_MainViewUp[ MAX_SPLITSCREEN_CLIENTS ];

//-----------------------------------------------------------------------------
// Main view
//-----------------------------------------------------------------------------
inline const Vector &MainViewOrigin( int nSlot = -1 )
{
#ifdef DEDICATED
	nSlot = 0;
#else
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	}
#endif
	return g_MainViewOrigin[ nSlot ];
}

inline const Vector &MainViewForward( int nSlot = -1 )
{
#ifdef DEDICATED
	nSlot = 0;
#else
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	}
#endif
	return g_MainViewForward[ nSlot ];
}

inline const Vector &MainViewRight( int nSlot = -1 )
{
#ifdef DEDICATED
	nSlot = 0;
#else
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	}
#endif
	return g_MainViewRight[ nSlot ];
}

inline const Vector &MainViewUp( int nSlot = -1 )
{
#ifdef DEDICATED
	nSlot = 0;
#else
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	}
#endif
	return g_MainViewUp[ nSlot ];
}


void R_Init (void);
void R_LevelInit(void);
void R_LevelShutdown(void);

// Loads world geometry. Called when map changes or dx level changes
void R_LoadWorldGeometry( bool bDXChange = false );

#include "view_shared.h"
#include "ivrenderview.h"

class VMatrix;

abstract_class IRender
{
public:
	virtual void	FrameBegin( void ) = 0;
	virtual void	FrameEnd( void ) = 0;
	
	virtual void	ViewSetupVis( bool novis, int numorigins, const Vector origin[] ) = 0;

	virtual void	ViewDrawFade( byte *color, IMaterial* pFadeMaterial, bool mapFullTextureToScreen = true ) = 0;

	virtual void	DrawSceneBegin( void ) = 0;
	virtual void	DrawSceneEnd( void ) = 0;

	virtual IWorldRenderList * CreateWorldList() = 0;
#if defined(_PS3)
	virtual IWorldRenderList * CreateWorldList_PS3( int viewID ) = 0;
	virtual void	BuildWorldLists_PS3_Epilogue( IWorldRenderList *pList, WorldListInfo_t* pInfo, bool bShadowDepth ) = 0;
#else
	virtual void	BuildWorldLists_Epilogue( IWorldRenderList *pList, WorldListInfo_t* pInfo, bool bShadowDepth ) = 0;
#endif
	virtual void	BuildWorldLists( IWorldRenderList *pList, WorldListInfo_t* pInfo, int iForceViewLeaf, const VisOverrideData_t* pVisData, bool bShadowDepth, float *pReflectionWaterHeight ) = 0;
	virtual void	DrawWorldLists( IMatRenderContext *pRenderContext, IWorldRenderList *pList, unsigned long flags, float waterZAdjust ) = 0;

	// UNDONE: these are temporary functions that will end up on the other
	// side of this interface
	// accessors
//	virtual const Vector& UnreflectedViewOrigin() = 0;
	virtual const Vector& ViewOrigin( void ) = 0;
	virtual const QAngle& ViewAngles( void ) = 0;
	virtual CViewSetup const &ViewGetCurrent( void ) = 0;
	virtual const VMatrix& ViewMatrix( void ) = 0;
	virtual const VMatrix& WorldToScreenMatrix( void ) = 0;
	virtual float	GetFramerate( void ) = 0;
	virtual float	GetZNear( void ) = 0;
	virtual float	GetZFar( void ) = 0;

	// Query current fov and view model fov
	virtual float	GetFov( void ) = 0;
	virtual float	GetFovY( void ) = 0;
	virtual float	GetFovViewmodel( void ) = 0;

	// Compute the clip-space coordinates of a point in 3D
	// Clip-space is normalized screen coordinates (-1 to 1 in x and y)
	// Returns true if the point is behind the camera
	virtual bool	ClipTransform( Vector const& point, Vector* pClip ) = 0;

	// Compute the screen-space coordinates of a point in 3D
	// This returns actual pixels
	// Returns true if the point is behind the camera
	virtual bool ScreenTransform( Vector const& point, Vector* pScreen ) = 0;

	virtual void Push3DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes ) = 0;
	virtual void Push3DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes, ITexture* pDepthTexture ) = 0;
	virtual void Push2DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes ) = 0;
	virtual void PopView( IMatRenderContext *pRenderContext, Frustum frustumPlanes ) = 0;
	virtual void SetMainView( const Vector &vecOrigin, const QAngle &angles ) = 0;
	virtual void ViewSetupVisEx( bool novis, int numorigins, const Vector origin[], unsigned int &returnFlags ) = 0;
	virtual void OverrideViewFrustum( Frustum custom ) = 0;
	virtual void UpdateBrushModelLightmap( model_t *model, IClientRenderable *Renderable ) = 0;
	virtual void BeginUpdateLightmaps( void ) = 0;
	virtual void EndUpdateLightmaps() = 0;
	virtual bool InLightmapUpdate( void ) const = 0;
};

void R_PushDlights (void);

// UNDONE Remove this - pass this around to functions/systems that need it.
extern IRender *g_EngineRenderer;
#endif			// RENDER_H
