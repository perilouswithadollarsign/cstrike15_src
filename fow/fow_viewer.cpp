#include "fow_viewer.h"
#include "fow_radiusoccluder.h"
#include "fow_lineoccluder.h"
#include "fow_horizontalslice.h"
#include "fow.h"
#include "engine/IVDebugOverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


extern IVDebugOverlay *debugoverlay;


//-----------------------------------------------------------------------------
// Purpose: constructor to init this viewer with the id and temp
// Input  : nID - the id of this viewer
//			nViewerTeam - the team this viewer is on
//-----------------------------------------------------------------------------
CFoW_Viewer::CFoW_Viewer( int nID, unsigned nViewerTeam )
{
	m_nID = nID;
	m_nViewerTeam = nViewerTeam;
	m_vLocation.Init();
	m_flRadius = 0.0f;
	m_pVisibility = NULL;

	m_pVisibilityRadius = NULL;
	m_nRadiusUnits = 0;
	m_nHeightGroup = 0;

	m_nAllocatedMemory = 0;

	m_bDirty = true;
}


//-----------------------------------------------------------------------------
// Purpose: destructor to free up the local vis grids
//-----------------------------------------------------------------------------
CFoW_Viewer::~CFoW_Viewer( void )
{
	if ( m_pVisibility )
	{
		free( m_pVisibility );
		m_pVisibility = NULL;
	}

	if ( m_pVisibilityRadius )
	{
		free( m_pVisibilityRadius );
		m_pVisibilityRadius = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: update the radius of the viewer.  this will realloc the local vis grids
// Input  : pFoW - the main FoW object
//			flRadius - the new radius
//-----------------------------------------------------------------------------
void CFoW_Viewer::UpdateSize( CFoW *pFoW, float flRadius )
{
	m_flRadius = flRadius;

	if ( m_pVisibility )
	{
		free( m_pVisibility );
	}

	if ( m_pVisibilityRadius )
	{
		free( m_pVisibilityRadius );
	}

	m_nAllocatedMemory = 0;

	int nGridSize = pFoW->GetHorizontalGridSize();

	m_nGridUnits = ( ( m_flRadius * 2 ) + nGridSize - 1 ) / nGridSize;
	m_nGridUnits |= 1;	// always make it odd, so that we have a true center
	m_pVisibility = ( byte * )malloc( sizeof( m_pVisibility[ 0 ] ) * m_nGridUnits * m_nGridUnits );
	m_nAllocatedMemory += sizeof( m_pVisibility[ 0 ] ) * m_nGridUnits * m_nGridUnits;

	m_nRadiusUnits = ( ( 2 * M_PI * m_flRadius ) + nGridSize - 1 ) / nGridSize;
//	m_nRadiusUnits = 360;
	m_pVisibilityRadius = ( int * )malloc( sizeof( m_pVisibilityRadius[ 0 ] ) * m_nRadiusUnits );
	m_nAllocatedMemory += sizeof( m_pVisibilityRadius[ 0 ] ) * m_nRadiusUnits;

	m_pVisibilityTable = pFoW->FindRadiusTable( flRadius );

	m_bDirty = true;
}


//-----------------------------------------------------------------------------
// Purpose: update the location of the viewer
// Input  : vLocation - the new location
//-----------------------------------------------------------------------------
bool CFoW_Viewer::UpdateLocation( CFoW *pFoW, const Vector &vLocation, Vector *pvOldLocation )
{
	Vector vNewLocation = vLocation;

	m_vRealLocation = vLocation;

	pFoW->CenterCoordToGrid( vNewLocation );

	if ( vNewLocation.x != m_vLocation.x || vNewLocation.y != m_vLocation.y )
	{	// we've moved to a new grid center
		m_bDirty = true;
		if ( pvOldLocation != NULL )
		{
			*pvOldLocation = m_vLocation;
		}
		m_vLocation = vNewLocation;

		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: update the height group of this viewer
// Input  : nHeightGroup - the new height group
//-----------------------------------------------------------------------------
void CFoW_Viewer::UpdateHeightGroup( uint8 nHeightGroup )
{
	if ( m_nHeightGroup != nHeightGroup )
	{
		m_bDirty = true;
	}

	m_nHeightGroup = nHeightGroup;
}


//-----------------------------------------------------------------------------
// Purpose: get the upper left coords of this viewer
// Output : vResults - the upper left location of this viewer
//-----------------------------------------------------------------------------
void CFoW_Viewer::GetStartPosition( Vector2D &vResults )
{
	vResults.x = m_vLocation.x - m_flRadius;
	vResults.y = m_vLocation.y - m_flRadius;
}


//-----------------------------------------------------------------------------
// Purpose: get the lower right coords of this viewer
// Output : vResults - the lower right of this viewer
//-----------------------------------------------------------------------------
void CFoW_Viewer::GetEndPosition( Vector2D &vResults )
{
	vResults.x = m_vLocation.x + m_flRadius;
	vResults.y = m_vLocation.y + m_flRadius;
}


//-----------------------------------------------------------------------------
// Purpose: calculate the localized visibility against all occluders.
// Input  : pFoW - the main FoW object
//-----------------------------------------------------------------------------
void CFoW_Viewer::CalcLocalizedVisibility( CFoW *pFoW )
{
	if ( m_bDirty == false )
	{
		return;
	}

//	DefaultViewingArea( pFoW );
	DefaultViewingRadius( pFoW );

	pFoW->ObstructOccludersNearViewer( m_nID );

#if 0
	int		NumOccluders = FoW->GetNumOccluders();

	for ( int i = 0; i < NumOccluders; i++ )
	{
		CFoW_RadiusOccluder *Occluder = FoW->GetOccluder( i );

		if ( !Occluder )
		{
			continue;
		}

		if ( !Occluder->IsInRange( this  ) )
		{
			continue;
		}

		Occluder->ObstructViewer( FoW, this );
	}
#endif

	int nSliceIndex = pFoW->GetHorizontalSlice( m_vLocation.z );
	if ( nSliceIndex != -1 )
	{
		pFoW->GetSlice( nSliceIndex )->ObstructViewer( pFoW, this );
	}

	ResolveRadius( pFoW );

	m_bDirty = false;
}

//-----------------------------------------------------------------------------
// Purpose: clear the localized visibility grid to just the raw radius
// Input  : pFoW - the main FoW object
//-----------------------------------------------------------------------------
// use this FOW_VG_DEFAULT_VISIBLE
void CFoW_Viewer::DefaultViewingArea( CFoW *pFoW )
{
	memset( m_pVisibility, 0, sizeof( m_pVisibility[ 0 ] ) * m_nGridUnits * m_nGridUnits );

	int		nGridSize = pFoW->GetHorizontalGridSize();
	int		nOffset = ( m_nGridUnits / 2 ) * nGridSize;
	byte	*pVisibility = m_pVisibility;
	int		nRadius2 = m_flRadius * m_flRadius;

	for ( int x = 0, xPos = -nOffset; x < m_nGridUnits; x++, xPos += nGridSize )
	{
		for ( int y = 0, yPos = -nOffset; y < m_nGridUnits; y++, yPos += nGridSize, pVisibility++ )
		{
			if ( ( ( xPos * xPos ) + ( yPos * yPos ) ) <= nRadius2 )
			{
				*pVisibility = FOW_VG_IS_VISIBLE | FOW_VG_DEFAULT_VISIBLE;
			}
			else
			{
				*pVisibility = 0;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: clear the localized radius grid to the maximum distance
// Input  : pFoW - the main FoW object
//-----------------------------------------------------------------------------
void CFoW_Viewer::DefaultViewingRadius( CFoW *pFoW )
{
	int		nRadius2 = m_flRadius * m_flRadius;

	for ( int i = 0; i < m_nRadiusUnits; i++ )
	{
		m_pVisibilityRadius[ i ] = nRadius2;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
//-----------------------------------------------------------------------------
void CFoW_Viewer::DrawDebugInfo( Vector &vLocation, float flViewRadius, unsigned nFlags )
{
	if ( ( nFlags & ( FOW_DEBUG_SHOW_VIEWERS_TEAM_0 | FOW_DEBUG_SHOW_VIEWERS_TEAM_1 ) ) == 0 )
	{
		return;
	}

	Vector vDiff = vLocation - m_vLocation;
	
	if ( vDiff.Length2D() > flViewRadius + m_flRadius )
	{
		return;
	}

	if ( ( nFlags & FOW_DEBUG_SHOW_VIEWERS_TEAM_0 ) != 0 )
	{
		debugoverlay->AddSphereOverlay( m_vLocation, m_flRadius, 10, 10, 0, 255, 0, 127, FOW_DEBUG_VIEW_TIME );
		debugoverlay->AddBoxOverlay( m_vLocation, Vector( -16.0f, -16.0f, -16.0f ), Vector( 16.0f, 16.0f, 16.0f ), QAngle( 0, 0, 0 ), 0, 255, 0, 127, FOW_DEBUG_VIEW_TIME );
	}

	if ( ( nFlags & FOW_DEBUG_SHOW_VIEWERS_TEAM_1 ) != 0 )
	{
		debugoverlay->AddSphereOverlay( m_vLocation, m_flRadius, 10, 10, 0, 0, 255, 127, FOW_DEBUG_VIEW_TIME );
		debugoverlay->AddBoxOverlay( m_vLocation, Vector( -16.0f, -16.0f, -16.0f ), Vector( 16.0f, 16.0f, 16.0f ), QAngle( 0, 0, 0 ), 0, 0, 255, 127, FOW_DEBUG_VIEW_TIME );
	}
}


//-----------------------------------------------------------------------------
// Purpose: turn the radius grid into the localized visibility grid
// Input  : pFoW - the main FoW object
//-----------------------------------------------------------------------------
void CFoW_Viewer::ResolveRadius( CFoW *pFoW )
{
#if 0
	int		nGridSize = pFoW->GetHorizontalGridSize();
	int		nHalfGridSize = nGridSize / 2;
	byte	*pVisibility = m_pVisibility;

//	int		Radius2 = m_flRadius * m_flRadius;

	for ( int x = 0, xPos = -m_flRadius + nHalfGridSize; x < m_nGridUnits; x++, xPos += nGridSize )
	{
		for ( int y = 0, yPos = -m_flRadius + nHalfGridSize; y < m_nGridUnits; y++, yPos += nGridSize, pVisibility++ )
		{
			float flDist = sqrt( ( float )( ( xPos * xPos ) + ( yPos * yPos ) ) );
			if ( flDist > m_flRadius )
			{
				*pVisibility = 0;
				continue;
			}

			float nx = xPos / flDist;
			float ny = yPos / flDist;

			float flAngle = ( 0 * nx ) + ( 1 * ny );
			float flRealAngle = RAD2DEG( acos( flAngle ) );

			flAngle = -flAngle + 1;
			if ( nx < 0.0f )
			{
				flAngle = 4 - flAngle;
				flRealAngle = 360 - flRealAngle;
			}
			flAngle /= 4.0f;
			flAngle *= m_nRadiusUnits;
			flRealAngle = ( flRealAngle / 360.0f ) * m_nRadiusUnits;

			if ( flDist <= m_pVisibilityRadius[ ( int )flRealAngle ] )
			{
				*pVisibility = FOW_VG_IS_VISIBLE;
			}
			else
			{
				*pVisibility = 0;
			}
		}
	}
#else
	int		nGridSize = pFoW->GetHorizontalGridSize();
	int		nOffset = ( m_nGridUnits / 2 ) * nGridSize;
	byte	*pVisibility = m_pVisibility;
	int		*pVisibilityTable = m_pVisibilityTable;

	for ( int x = 0, xPos = -nOffset; x < m_nGridUnits; x++, xPos += nGridSize )
	{
		for ( int y = 0, yPos = -nOffset; y < m_nGridUnits; y++, yPos += nGridSize, pVisibility++, pVisibilityTable++ )
		{
			if ( ( *pVisibilityTable ) == -1 )
			{
				*pVisibility = 0;
				continue;
			}

			float flDist = ( ( xPos * xPos ) + ( yPos * yPos ) );
			if ( flDist <= m_pVisibilityRadius[ *pVisibilityTable ] )
			{
				*pVisibility = FOW_VG_IS_VISIBLE;
			}
			else
			{
				*pVisibility = 0;
			}
		}
	}
#endif
}

#include <tier0/memdbgoff.h>
