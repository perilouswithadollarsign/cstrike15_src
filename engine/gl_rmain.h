//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef GL_RMAIN_H
#define GL_RMAIN_H

#ifdef _WIN32
#pragma once
#endif


#include "mathlib/vector.h"
#include "mathlib/mathlib.h"

extern Frustum_t g_Frustum;

// Cull to the full screen frustum.
inline bool R_CullBox( const Vector& mins, const Vector& maxs )
{
	return g_Frustum.CullBox( mins, maxs );
}

// Draw a rectangle in screenspace. The screen window is (-1,-1) to (1,1).
void R_DrawScreenRect( float left, float top, float right, float bottom );

void R_DrawPortals();
float GetScreenAspect( int viewportWidth, int viewportHeight );
void R_CheckForLightingConfigChanges();
void R_CheckForPaintmapChanges();

// NOTE: Screen coordinates go from 0->w, 0->h
void ComputeWorldToScreenMatrix( VMatrix *pWorldToScreen, const VMatrix &worldToProjection, const CViewSetup &viewSetup );

#endif // GL_RMAIN_H
