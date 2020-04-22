//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 2d line occlusion for Fog of War
//
// $NoKeywords: $
//=====================================================================================//

#ifndef FOW_LINEOCCLUDER_H
#define FOW_LINEOCCLUDER_H
#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "mathlib/vector.h"
#include "fow_2dplane.h"

class CFoW;
class CFoW_Viewer;

class CFoW_LineOccluder
{
public:
	// construct a line occluder from the given points.  the normal is supplied, though if it doesn't match up with the points, then the points are swapped.
	CFoW_LineOccluder( float bx, float by, float ex, float ey, Vector2D &vNormal, int nSliceNum );

	CFoW_LineOccluder( Vector2D &vStart, Vector2D &vEnd, CFOW_2DPlane &Plane, int nSliceNum );

					// get the starting point as determined by the normal
	inline Vector2D	&GetStart( void ) { return m_vStart; }
					// get the ending point as determined by the normal
	inline Vector2D	&GetEnd( void ) { return m_vEnd; }
					// get the plane normal
	inline Vector2D	GetPlaneNormal( void ) { return m_Plane.GetNormal(); }
					// get the plane normal
	inline float 	GetPlaneDistance( void ) { return m_Plane.GetDistance(); }
					// get the slice index this occluder belongs to
	inline int		GetSliceNum( void ) { return m_nSliceNum; }

	// determine the occlusion of this line for the viewer
	void	ObstructViewer( CFoW *pFoW, CFoW_Viewer *pViewer );

private:
	Vector2D		m_vStart;		// the starting point as determined by the normal
	Vector2D		m_vEnd;			// the ending point as determined by the normal
	CFOW_2DPlane	m_Plane;		// the plane that is formed
	int				m_nSliceNum;	// the slice index of this occluder
};

#endif // FOW_LINEOCCLUDER_H
