#include "fow.h"
#include "fow_radiusoccluder.h"
#include "fow_viewer.h"
#include "fow_2dplane.h"
#include "engine/IVDebugOverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


extern IVDebugOverlay *debugoverlay;


//-----------------------------------------------------------------------------
// Purpose: constructor to init this occluder with the id
// Input  : nID - the id of this occluder
//-----------------------------------------------------------------------------
CFoW_RadiusOccluder::CFoW_RadiusOccluder( int nID )
{
	m_nID = nID;
	m_flRadius = 0.0f;
	m_vLocation.Zero();
	m_nHeightGroup = 0;
	m_bEnabled = true;
}


//-----------------------------------------------------------------------------
// Purpose: update the radius of this occluder
// Input  : flRadius - the new radius size
//-----------------------------------------------------------------------------
void CFoW_RadiusOccluder::UpdateSize( float flRadius )
{
	m_flRadius = flRadius;
}


//-----------------------------------------------------------------------------
// Purpose: update the location of this occluder
// Input  : vLocation - the new location
//-----------------------------------------------------------------------------
void CFoW_RadiusOccluder::UpdateLocation( Vector &vLocation )
{
	m_vLocation = vLocation;
}


//-----------------------------------------------------------------------------
// Purpose: update the height group of this occluder
// Input  : nHeightGroup - the new height group
//-----------------------------------------------------------------------------
void CFoW_RadiusOccluder::UpdateHeightGroup( uint8 nHeightGroup )
{
	m_nHeightGroup = nHeightGroup;
}


//-----------------------------------------------------------------------------
// Purpose: is the occluder within range of the viewer?
// Input  : pViewer - the viewer to check against
// Output : returns true if the two circles intersect
//-----------------------------------------------------------------------------
bool CFoW_RadiusOccluder::IsInRange( CFoW_Viewer *pViewer )
{
	if ( m_bEnabled == false )
	{
		return false;
	}

	Vector	vDiff = pViewer->GetLocation() - m_vLocation;
	float	flLen = sqrt( ( vDiff.x * vDiff.x ) + ( vDiff.y * vDiff.y ) );

	return ( flLen <= m_flRadius + pViewer->GetSize() );
}


//-----------------------------------------------------------------------------
// Purpose: obstruct the viewer by updating the local viewer grid
// Input  : pFoW - the main FoW object
//			pViewer - the viewer to obstruct
//-----------------------------------------------------------------------------
void CFoW_RadiusOccluder::ObstructViewerGrid( CFoW *pFoW, CFoW_Viewer *pViewer )
{
	if ( m_bEnabled == false )
	{
		return;
	}

	Vector	vViewerLoc = pViewer->GetLocation();
	float	flViewerRadius = pViewer->GetSize();
	Vector	vDelta = ( vViewerLoc - m_vLocation );

	vDelta.z = 0.0f;
	float flLength = vDelta.Length();

	if ( flLength > ( flViewerRadius + m_flRadius ) )
	{
		return;
	}

//	if ( length <= m_flRadius || length > ViewerRadius )
	if ( flLength <= m_flRadius )
	{
		return;
	}

	float flAngle = ( float )atan2( vDelta.y, vDelta.x );
	float flTangentLen = sqrt( flLength * flLength - m_flRadius * m_flRadius );
	float flTangentAngle = ( float )asin( m_flRadius / flLength );

	// compute the two tangent angles
	float flPos = flAngle + flTangentAngle;
	float flNeg = flAngle - flTangentAngle;

	float x[ 6 ], y[ 6 ];

	// compute the two tangent points
	x[ 0 ] = -( float )cos( flPos ) * flTangentLen + vViewerLoc.x;
	y[ 0 ] = -( float )sin( flPos ) * flTangentLen + vViewerLoc.y;
	x[ 5 ] = -( float )cos( flNeg ) * flTangentLen + vViewerLoc.x;
	y[ 5 ] = -( float )sin( flNeg ) * flTangentLen + vViewerLoc.y;

	// extend the tangent points to the viewer's edge
	x[ 1 ] = -( float )cos( flPos ) * flViewerRadius + vViewerLoc.x;
	y[ 1 ] = -( float )sin( flPos ) * flViewerRadius + vViewerLoc.y;
	x[ 4 ] = -( float )cos( flNeg ) * flViewerRadius + vViewerLoc.x;
	y[ 4 ] = -( float )sin( flNeg ) * flViewerRadius + vViewerLoc.y;

	// compute the forward direction of the viewer's intersection through this blocker
	float fx = -vDelta.x / flLength;
	float fy = -vDelta.y / flLength;

	// compute half the length between the viewer's tangent edges
	float dx2 = x[ 4 ] - x[ 1 ];
	float dy2 = y[ 4 ] - y[ 1 ];
	float flHalflen = ( dx2 * dx2 + dy2 * dy2 ) / 4;

	// compute the side of the triangle that forms from viewer's radius to half way across the viewer's tangent edges
	float flLen2 = ( float )sqrt( flViewerRadius * flViewerRadius - flHalflen );
	flLen2 = flViewerRadius - flLen2;

	// compute the box extents to encompass the circle's bounds
	x[ 2 ] = x[ 1 ] + ( fx * flLen2 );
	y[ 2 ] = y[ 1 ] + ( fy * flLen2 );
	x[ 3 ] = x[ 4 ] + ( fx * flLen2 );
	y[ 3 ] = y[ 4 ] + ( fy * flLen2 );

	CFOW_2DPlane Planes[ 6 ];

	for (int i = 0; i < 6; i++)
	{
		Planes[ i ].Init( x[ i ], y[ i ], x[ ( i + 1 ) % 6 ], y[ ( i + 1 ) % 6 ] );
	}

	float flMinX = x[ 0 ], flMinY = y[ 0 ], flMaxX = x[ 0 ], flMaxY = y[ 0 ];

	for ( int i = 1; i < 6; i++ )
	{
		if ( x[ i ] < flMinX )
		{
			flMinX = x[ i ];
		}
		if ( x[ i ] > flMaxX )
		{
			flMaxX = x[ i ];
		}
		if ( y[ i ] < flMinY )
		{
			flMinY = y[ i ];
		}
		if ( y[i] > flMaxY )
		{
			flMaxY = y[ i ];
		}
	}

	float		px, py, flStart_py, ex, ey;
	int			nGridX, nGridY, nStartGridY;
	Vector2D	vViewerStart, vViewerEnd;
	int			nGridSize = pFoW->GetHorizontalGridSize();

	pViewer->GetStartPosition( vViewerStart );
	pViewer->GetEndPosition( vViewerEnd );

	if ( flMinX > vViewerStart.x )
	{
		nGridX = ( int )( flMinX - vViewerStart.x ) / nGridSize;
		px = vViewerStart.x + ( int )( nGridSize * nGridX );
	}
	else
	{
		px = vViewerStart.x;
		nGridX = 0;
	}
	if ( flMaxX < vViewerEnd.x )
	{
		ex = flMaxX;
	}
	else
	{
		ex = vViewerEnd.x;
	}

	if ( flMinY > vViewerStart.y )
	{
		nStartGridY = ( int )( flMinY - vViewerStart.y ) / nGridSize;
		flStart_py = vViewerStart.y + ( int )( nGridSize * nStartGridY );
	}
	else
	{
		nStartGridY = 0;
		flStart_py = vViewerStart.y;
	}
	if ( flMaxY < vViewerEnd.y )
	{
		ey = flMaxY;
	}
	else
	{
		ey = vViewerEnd.y;
	}

	byte	*pLocalVisibility = pViewer->GetVisibility();
	int		nLocalGridUnits = pViewer->GetGridUnits();

	// offset to center of grid
	px += nGridSize / 2;
	flStart_py += nGridSize / 2;

	for ( ; px < ex; px += nGridSize, nGridX++)
	{
		for ( nGridY = nStartGridY, py = flStart_py; py < ey; py += nGridSize, nGridY++ )
		{
			byte	*pPos = pLocalVisibility + ( nGridX * nLocalGridUnits ) + nGridY;

			if ( ( ( *pPos ) & FOW_VG_IS_VISIBLE ) == 0 )
			{
				continue;
			}

			int i;
			for ( i = 0; i < 6; i++ )
			{
#if 0
				// we don't need to check the bounding planes - these would be used to construct a stencil buffer though
				if ( i == 1 || i == 2 || i == 3 )
				{
					continue;
				}
#endif
				if ( !Planes[ i ].PointInFront( px, py ) )
				{
					break;
				}
			}

			if ( i == 6 )
			{
				( *pPos ) &= ~FOW_VG_IS_VISIBLE;
			}
		}
	}
}


// #define SLOW_PATH	1


//-----------------------------------------------------------------------------
// Purpose: obstruct the viewer by updating the depth circle
// Input  : pFoW - the main FoW object
//			pViewer - the viewer to obstruct
//-----------------------------------------------------------------------------
void CFoW_RadiusOccluder::ObstructViewerRadius( CFoW *pFoW, CFoW_Viewer *pViewer )
{
	if ( m_bEnabled == false )
	{
		return;
	}

	if ( m_flRadius <= 1.0f )
	{
		Warning( "FoW: Occluder %d has invalid radius\n", m_nID );
		return;
	}

	int nViewerHeightGroup = pViewer->GetHeightGroup();
	if ( nViewerHeightGroup >= 1 && m_nHeightGroup >= 1 && m_nHeightGroup < nViewerHeightGroup )
	{	// both the viewer and the occluder have height groups and this occluder is under the viewer, then don't obstruct
		return;
	}

	Vector	vViewerLoc = pViewer->GetLocation();
	float	flViewerRadius = pViewer->GetSize();
	Vector	vDelta = ( vViewerLoc - m_vLocation );

	vDelta.z = 0.0f;
	float flLength = vDelta.Length();

	if ( flLength > ( flViewerRadius + m_flRadius ) )
	{
		return;
	}

	//	if ( length <= m_flRadius || length > ViewerRadius )
	if ( flLength <= m_flRadius )
	{
		return;
	}

	float flAngle = ( float )atan2( vDelta.x, vDelta.y ) + DEG2RAD( 180.0f );
	float flTangentLen = sqrt( flLength * flLength - m_flRadius * m_flRadius );
	float flTangentAngle = ( float )asin( m_flRadius / flLength );

	// compute the two tangent angles
	float flPos = flAngle + flTangentAngle;
	float flNeg = flAngle - flTangentAngle;

	float x[ 6 ], y[ 6 ];

	// compute the two tangent points
#ifdef SLOW_PATH
	x[ 0 ] = ( float )sin( flPos ) * flTangentLen + vViewerLoc.x;
	y[ 0 ] = ( float )cos( flPos ) * flTangentLen + vViewerLoc.y;
	x[ 5 ] = ( float )sin( flNeg ) * flTangentLen + vViewerLoc.x;
	y[ 5 ] = ( float )cos( flNeg ) * flTangentLen + vViewerLoc.y;
#else
	x[ 0 ] = ( float )TableSin( flPos ) * flTangentLen + vViewerLoc.x;
	y[ 0 ] = ( float )TableCos( flPos ) * flTangentLen + vViewerLoc.y;
	x[ 5 ] = ( float )TableSin( flNeg ) * flTangentLen + vViewerLoc.x;
	y[ 5 ] = ( float )TableCos( flNeg ) * flTangentLen + vViewerLoc.y;
#endif

//	Msg( "%g, %g\n", RAD2DEG( flPos ), RAD2DEG( flNeg ) );

	CFOW_2DPlane	Plane;

	Plane.Init( x[ 5 ], y[ 5 ], x[ 0 ], y[ 0 ] );

	int		nUnits = pViewer->GetRadiusUnits();
	int		*pVisibility = pViewer->GetVisibilityRadius();

	Vector	vCenterLocation = vViewerLoc;
	float	flDistance = Plane.DistanceFrom( vCenterLocation.x, vCenterLocation.y );
	if ( flDistance < 0.0f )
	{
		return;
	}

#ifdef SLOW_PATH
	CFOW_2DPlane	Edge1, Edge2;

	Edge1.Init( vCenterLocation.x, vCenterLocation.y, x[ 0 ], y[ 0 ] );
	Edge2.Init( x[ 5 ], y[ 5 ], vCenterLocation.x, vCenterLocation.y );

	float	flDegreeAmount = 360.0f / nUnits;

	float	flCurrentDegree = 0.0f;
	for ( int i = 0; i < nUnits; i++, flCurrentDegree += flDegreeAmount )
	{
		Vector	vLocation = vViewerLoc;
		Vector	vDelta;

		vDelta.x = sin( DEG2RAD( flCurrentDegree ) );
		vDelta.y = cos( DEG2RAD( flCurrentDegree ) );

		vLocation += vDelta * flViewerRadius;

		float flDistance = Plane.DistanceFromRay( vCenterLocation.x, vCenterLocation.y, vDelta.x, vDelta.y );
		if ( flDistance >= 0.0f )
		{
			flDistance *= flDistance;
			if ( flDistance >= 0.0f && flDistance < pVisibility[ i ] )
			{
				if ( Edge1.PointInFront( vLocation.x, vLocation.y ) && Edge2.PointInFront( vLocation.x, vLocation.y ) )
				{
					pVisibility[ i ] = flDistance;
				}
			}
		}
	}
#else
	
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

	float	flDegreeAmount = 2.0f * M_PI_F / nUnits;
	nStartIndex = ( int )( flCurrentDegree / flDegreeAmount ) % nUnits;
	if ( nStartIndex < 0 )
	{
		nStartIndex += nUnits;
	}

	float	flHorizontalGridSize = pFoW->GetHorizontalGridSize();

	for ( int i = nStartIndex; flCurrentDegree < flFinishDegree; i++, flCurrentDegree += flDegreeAmount )
	{
		Vector	vDelta;

		if ( i >= nUnits )
		{
			i = 0;
		}

		vDelta.x = TableSin( flCurrentDegree );
		vDelta.y = TableCos( flCurrentDegree );

		float flDistance = Plane.DistanceFromRay( vCenterLocation.x, vCenterLocation.y, vDelta.x, vDelta.y );
		if ( flDistance >= 0.0f )
		{
			flDistance += flHorizontalGridSize * 1.1f;	// back off a bit

			flDistance *= flDistance;
			if ( flDistance < pVisibility[ i ] )
			{
				pVisibility[ i ] = flDistance;
			}
		}
	}

#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
//-----------------------------------------------------------------------------
void CFoW_RadiusOccluder::DrawDebugInfo( Vector &vLocation, float flViewRadius, unsigned nFlags )
{
	if ( ( nFlags & FOW_DEBUG_SHOW_OCCLUDERS ) == 0 )
	{
		return;
	}

	Vector vDiff = vLocation - m_vLocation;

	if ( vDiff.Length2D() > flViewRadius + m_flRadius )
	{
		return;
	}

	if ( ( nFlags & FOW_DEBUG_SHOW_OCCLUDERS ) != 0 )
	{
		debugoverlay->AddSphereOverlay( m_vLocation, m_flRadius, 10, 10, 255, 0, 0, 127, FOW_DEBUG_VIEW_TIME );
		debugoverlay->AddBoxOverlay( m_vLocation, Vector( -16.0f, -16.0f, -16.0f ), Vector( 16.0f, 16.0f, 16.0f ), QAngle( 0, 0, 0 ), 255, 0, 0, 127, FOW_DEBUG_VIEW_TIME );
	}
}


#include <tier0/memdbgoff.h>
