//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 2d plane routines for Fog of War
//
// $NoKeywords: $
//=====================================================================================//

#ifndef FOW_2DPLANE_H
#define FOW_2DPLANE_H
#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "mathlib/vector.h"

class CFOW_2DPlane
{
public:
	CFOW_2DPlane( void ) { }
	// construct the plane from the given line segment
	CFOW_2DPlane( float bx, float by, float ex, float ey );
	// construct the plane from the given point and normal
	CFOW_2DPlane( float x, float y, Vector2D &vNormal );

	CFOW_2DPlane( float flDistance, Vector2D &vNormal );

	// init routine to generate the plane from the given line segment
	void Init( float bx, float by, float ex, float ey );
	// init routine to generate the plane from the given point and normal
	void Init( float x, float y, Vector2D &vNormal );

	// returns the normal of the plane
	inline Vector2D	&GetNormal( void ) { return m_vNormal; }

	// 
	inline float GetDistance( ) { return m_flDistance; }


	// returns true if the point is in front of the plane
	bool	PointInFront( float px, float py );
	// returns the distance the point is from the plane
	float	DistanceFrom( float px, float py );
	// finds the fraction from the starting point towards the normal along the line formed with ending point
	float	DistanceFromLineStart( float bx, float by, float ex, float ey );
	// finds the fraction from the starting point towards the normal along the line formed with ending point
	float	DistanceFromRay( float bx, float by, float dx, float dy );

private:
	Vector2D	m_vNormal;		// the normal of the plane
	float		m_flDistance;	// the plane distance
};

#endif // FOW_2DPLANE_H
