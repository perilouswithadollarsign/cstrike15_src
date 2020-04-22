#include "fow.h"
#include "fow_lineoccluder.h"
#include "fow_viewer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: construct a line accluder from the given points.  the normal is supplied, though if it doesn't match up with the points, then the points are swapped.
// Input  : bx - starting x coord
//			by - starting y coord
//			ex - ending x coord
//			ey - ending y coord
//			vNormal - the normal coming from a pre-existing plane from which the line was formed
//			nSlinceNum - the slice this occluder belongs to
//-----------------------------------------------------------------------------
CFoW_LineOccluder::CFoW_LineOccluder( float bx, float by, float ex, float ey, Vector2D &vNormal, int nSliceNum )
{
	m_vStart.Init( bx, by );
	m_vEnd.Init( ex, ey );
	m_Plane.Init( bx, by, ex, ey );

	if ( fabs( m_Plane.GetNormal().x - vNormal.x ) < 0.1f && fabs( m_Plane.GetNormal().y - vNormal.y ) < 0.1f )
	{
		m_vStart.Init( ex, ey );
		m_vEnd.Init( bx, by );
		m_Plane.Init( ex, ey, bx, by );
	}

	m_nSliceNum = nSliceNum;
}


CFoW_LineOccluder::CFoW_LineOccluder( Vector2D &vStart, Vector2D &vEnd, CFOW_2DPlane &Plane, int nSliceNum )
{
	m_vStart = vStart;
	m_vEnd = vEnd;
	m_Plane = Plane;
	m_nSliceNum = nSliceNum;
}

// #define SLOW_PATH	1

//-----------------------------------------------------------------------------
// Purpose: determine the occlusion of this line for the viewer
// Input  : pFoW - the main FoW object
//			pViewer - the viewer to obstruct
//-----------------------------------------------------------------------------
void CFoW_LineOccluder::ObstructViewer( CFoW *pFoW, CFoW_Viewer *pViewer )
{
	int		nUnits = pViewer->GetRadiusUnits();
	int		*pVisibility = pViewer->GetVisibilityRadius();

	Vector	vCenterLocation = pViewer->GetRealLocation();	// we don't want to use the grid centered location, as the z is not centered
	float distance = m_Plane.DistanceFrom( vCenterLocation.x, vCenterLocation.y );
	if ( distance < 0.0f )
	{
		return;
	}

#ifdef SLOW_PATH
	float			flDegreeAmount = 360.0 / pViewer->GetRadiusUnits();
	CFOW_2DPlane	Edge1, Edge2;

	Edge1.Init( vCenterLocation.x, vCenterLocation.y, m_vStart.x, m_vStart.y );
	Edge2.Init( m_vEnd.x, m_vEnd.y, vCenterLocation.x, vCenterLocation.y );

	float	flCurrentDegree = 0.0f;
	for ( int i = 0; i < nUnits; i++, flCurrentDegree += flDegreeAmount )
	{
		Vector	Location = pViewer->GetLocation();

		Location.x += cos( DEG2RAD( flCurrentDegree ) ) * pViewer->GetSize();
		Location.y += sin( DEG2RAD( flCurrentDegree ) ) * pViewer->GetSize();

		float flDistance = m_Plane.DistanceFromLineStart( vCenterLocation.x, vCenterLocation.y, Location.x, Location.y );
//		flDistance *= pViewer->GetSize();
		if ( flDistance >= 0.0f )
		{
//			distance += Viewer->GetSize();
			flDistance *= flDistance;
			if ( flDistance >= 0.0f && flDistance < pVisibility[ i ] )
			{
				if ( Edge1.PointInFront( Location.x, Location.y ) && Edge2.PointInFront( Location.x, Location.y ) )
				{
					pVisibility[ i ] = flDistance;
				}
			}
		}
	}
#else

	const Vector2D	vStraight( 0.0f, 1.0f );

	Vector2D	P1( m_vStart.x - vCenterLocation.x, m_vStart.y - vCenterLocation.y );
	P1.NormalizeInPlace();
	Vector2D	P2( m_vEnd.x - vCenterLocation.x, m_vEnd.y - vCenterLocation.y );
	P2.NormalizeInPlace();
#if 0

	float	flCurrentDegree = 0.0f;
	for ( int i = 0; i < nUnits; i++, flCurrentDegree += flDegreeAmount )
	{
		Vector	Location = pViewer->GetLocation();

		Location.x += cos( DEG2RAD( flCurrentDegree ) ) * pViewer->GetSize();
		Location.y += sin( DEG2RAD( flCurrentDegree ) ) * pViewer->GetSize();

		float flDistance = m_Plane.DistanceFromLineStart( vCenterLocation.x, vCenterLocation.y, Location.x, Location.y );
		//		flDistance *= pViewer->GetSize();
		if ( flDistance >= 0.0f )
		{
			//			distance += Viewer->GetSize();
			flDistance *= flDistance;
			if ( flDistance >= 0.0f && flDistance < pVisibility[ i ] )
			{
				if ( Edge1.PointInFront( Location.x, Location.y ) && Edge2.PointInFront( Location.x, Location.y ) )
				{
					pVisibility[ i ] = flDistance;
				}
			}
		}
	}
#endif

	if ( fabs( P1.Dot( P2 ) ) > 0.99995f )
	{
		return;
	}

	float flDot1 = vStraight.Dot( P1 );
	float flPos = acos( flDot1 );
	if ( P1.x < 0.0f )
	{
		flPos = 2.0f * M_PI_F - flPos;
	}
	float flDot2 = vStraight.Dot( P2 );
	float flNeg = acos( flDot2 );
	if ( P2.x < 0.0f )
	{
		flNeg = 2.0f * M_PI_F - flNeg;
	}

	if ( fabs( flPos - flNeg ) > M_PI_F )
	{
		if ( flPos < flNeg )
		{
			flPos += M_PI_F * 2.0f;
		}
		else
		{
			flNeg += M_PI_F * 2.0f;
		}
	}
//	float flAng1 = RAD2DEG( flPos );
//	float flAng2 = RAD2DEG( flNeg );

	float	flCurrentDegree, flFinishDegree;
	int		nStartIndex;
	if ( flPos < flNeg )
	{
		flCurrentDegree = flPos;
		flFinishDegree = flNeg;
	}
	else
	{
		flCurrentDegree = flNeg;
		flFinishDegree = flPos;
	}

/*	if ( ( flFinishDegree - flCurrentDegree ) > M_PI_F )
	{
		float	flTemp = flCurrentDegree;
		flCurrentDegree = flFinishDegree;
		flFinishDegree = flTemp + ( 2.0f * M_PI_F );
	}
*/
	nUnits = pViewer->GetRadiusUnits();

	float	flDegreeAmount = 2.0f * M_PI_F / nUnits;
	nStartIndex = ( int )( flCurrentDegree / flDegreeAmount ) % nUnits;
	if ( nStartIndex < 0 )
	{
		nStartIndex += nUnits;
	}

//	Vector	vViewerLoc = pViewer->GetLocation();
//	float	flViewerRadius = pViewer->GetSize();
//	float	flMaxDistance = ( m_vStart - m_vEnd ).LengthSqr() + ( 60.0f * 60.0f );

#if 1
	for ( int i = nStartIndex; flCurrentDegree < flFinishDegree; i++, flCurrentDegree += flDegreeAmount )
	{
		Vector2D	vDelta;

		if ( i >= nUnits )
		{
			i = 0;
		}

		vDelta.x = TableSin( flCurrentDegree );
		vDelta.y = TableCos( flCurrentDegree );

#if 0
		float		flDistance = m_Plane.DistanceFromRay( vCenterLocation.x, vCenterLocation.y, vDelta.x, vDelta.y );
		Vector2D	vFinal = vCenterLocation.AsVector2D() + ( vDelta * flDistance );
		float		flDist1 = ( vFinal - m_vStart ).LengthSqr();
		float		flDist2 = ( vFinal - m_vEnd ).LengthSqr();
		if ( flDistance >= 0.0f && ( flDist1 + flDist2 ) < flMaxDistance )
#else
//		vDelta = ( vDelta * pViewer->GetSize() ) + vCenterLocation.AsVector2D();
//		float flDistance = m_Plane.DistanceFromLineStart( vCenterLocation.x, vCenterLocation.y, vDelta.x, vDelta.y );
		float		flDistance = m_Plane.DistanceFromRay( vCenterLocation.x, vCenterLocation.y, vDelta.x, vDelta.y );
		if ( flDistance >= 0.0f )
#endif

		{
			flDistance *= flDistance;
			if ( flDistance < pVisibility[ i ] )
			{
				pVisibility[ i ] = flDistance;
			}
		}
	}

#else
	int nStart = ( 0.0f / 4.0f ) * nUnits;
	int nEnd = ( 1.0f / 4.0f ) * nUnits;
	for( ; nStart < nEnd; nStart++ )
	{
		pVisibility[ nStart ] /= 5.0f;
	}
#endif
#endif
}

#include <tier0/memdbgoff.h>
