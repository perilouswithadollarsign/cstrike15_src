#include "fow_2dplane.h"
#include "mathlib/vector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: construct the plane from the given line segment
// Input  : bx - starting x coord
//			by - starting y coord
//			ex - ending x coord
//			ey - ending y coord
//-----------------------------------------------------------------------------
CFOW_2DPlane::CFOW_2DPlane( float bx, float by, float ex, float ey )
{
	Init( bx, by, ex, ey );
}


//-----------------------------------------------------------------------------
// Purpose: construct the plane from the given point and normal
// Input  : x - point on plane
//			y - point on plane
//			vNormal - normal of the plane
//-----------------------------------------------------------------------------
CFOW_2DPlane::CFOW_2DPlane( float x, float y, Vector2D &vNormal )
{
	Init( x, y, vNormal );
}


//-----------------------------------------------------------------------------
// Purpose: construct the plane from the given distance and normal
// Input  : flDistance - distance for the plane
//			vNormal - normal of the plane
//-----------------------------------------------------------------------------
CFOW_2DPlane::CFOW_2DPlane( float flDistance, Vector2D &vNormal )
{
	m_flDistance = flDistance;
	m_vNormal = vNormal;
}


//-----------------------------------------------------------------------------
// Purpose: init routine to generate the plane from the given line segment
// Input  : bx - starting x coord
//			by - starting y coord
//			ex - ending x coord
//			ey - ending y coord
//-----------------------------------------------------------------------------
void CFOW_2DPlane::Init( float bx, float by, float ex, float ey )
{
	float		nx = ( ex - bx );
	float		ny = ( ey - by );
	float		flLen = ( float )sqrt( nx * nx + ny * ny );
	Vector2D	vNormal;

	nx /= flLen;
	ny /= flLen;

	vNormal.x = ny;
	vNormal.y = -nx;
	Init( bx, by, vNormal );
}


//-----------------------------------------------------------------------------
// Purpose: init routine to generate the plane from the given point and normal
// Input  : x - point on plane
//			y - point on plane
//			vNormal - normal of the plane
//-----------------------------------------------------------------------------
void CFOW_2DPlane::Init( float x, float y, Vector2D &vNormal )
{
	m_vNormal = vNormal;

	m_flDistance = ( x * m_vNormal.x + y * m_vNormal.y );
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the point is in front of the plane
// Input  : px - point to check
//			py - point to check
// Output : returns true if the point is in front of the plane
//-----------------------------------------------------------------------------
bool CFOW_2DPlane::PointInFront( float px, float py )
{
	return ( DistanceFrom( px, py ) >= 0.0f );
}


//-----------------------------------------------------------------------------
// Purpose: returns the distance the point is from the plane
// Input  : px - point to check
//			py - point to check
// Output : returns the distance the point is from the plane
//-----------------------------------------------------------------------------
float CFOW_2DPlane::DistanceFrom( float px, float py )
{
	float d = ( px * m_vNormal.x + py * m_vNormal.y );

	return d - m_flDistance;
}


//-----------------------------------------------------------------------------
// Purpose: finds the fraction from the starting point towards the normal along the line formed with ending point
// Input  : bx - starting x coord of the line segment
//			by - starting y coord of the line segment
//			ex - ending x coord of the line segment
//			ey - ending y coord of the line segment
// Output : returns the distance along the line segment the plane from the starting coord
//-----------------------------------------------------------------------------
float CFOW_2DPlane::DistanceFromLineStart( float bx, float by, float ex, float ey )
{
	Vector2D	vPointA( bx, by );
	Vector2D	vPointB( ex, ey );
	Vector2D	vDiff( vPointB - vPointA );
	Vector2D	vNormal = vDiff;
	float		flLen = ( float )sqrt( vDiff.x * vDiff.x + vDiff.y * vDiff.y );

	vNormal /= flLen;

	float t = -( m_vNormal.Dot( vPointA ) - m_flDistance ) / m_vNormal.Dot( vNormal );

	return t;
}


//-----------------------------------------------------------------------------
// Purpose: finds the fraction from the starting point towards the normal along the line formed with ending point
// Input  : bx - starting x coord of the line segment
//			by - starting y coord of the line segment
//			ex - ending x coord of the line segment
//			ey - ending y coord of the line segment
// Output : returns the distance along the line segment the plane from the starting coord
//-----------------------------------------------------------------------------
float CFOW_2DPlane::DistanceFromRay( float bx, float by, float dx, float dy )
{
	Vector2D	vPointA( bx, by );
	Vector2D	vNormal( dx, dy );
	float		flNormalDiff = m_vNormal.Dot( vNormal );

	if ( flNormalDiff == 0.0f )
	{
		return 0.0f;
	}

	float t = -( m_vNormal.Dot( vPointA ) - m_flDistance ) / flNormalDiff;

	return t;
}

#include <tier0/memdbgoff.h>
