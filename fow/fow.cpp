#include "fow.h"
#include "fow_radiusoccluder.h"
#include "fow_viewer.h"
#include "fow_trisoup.h"
#include "fow_horizontalslice.h"
#include "keyvalues.h"
#include "utlbuffer.h"
#include "filesystem.h"
#include "vstdlib/jobthread.h"
#include "vphysics_interface.h"
#include "gametrace.h"
#include "vprof.h"
#include "engine/IVDebugOverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


extern IVDebugOverlay *debugoverlay;


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
CFoW::CFoW( ) : 
	m_Occluders( 100, 100 ),
	m_Viewers( 100, 100 ),
	m_TriSoupCollection( 50, 50 ),
	m_RadiusTables( 0, 0, DefLessFunc( float ) )
{
	m_nNumberOfTeams = 0;
	for ( int i = 0; i < MAX_FOW_TEAMS; i++ )
	{
		m_pVisibilityGridFlags[ i ] = NULL;
		m_pVisibilityGridDegree[ i ] = NULL;
		m_pVisibilityFadeTimer[ i ] = NULL;
	}

	m_pHorizontalSlices = NULL;
	m_pVerticalLevels = NULL;

	m_flDegreeFadeRate = 2.0;

	m_nGridZUnits = 0;

	m_bInitialized = false;
	m_bDebugVisible = false;
	m_nDebugFlags = 0;

	m_nHorizontalGridAllocationSize = m_nVerticalGridAllocationSize = 0;
	m_nRadiusTableSize = 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
CFoW::~CFoW( )
{
	ClearState();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFoW::ClearState( )
{
	for ( int i = 0; i < MAX_FOW_TEAMS; i++ )
	{
		if ( m_pVisibilityGridFlags[ i ] )
		{
			free( m_pVisibilityGridFlags[ i ] );
			m_pVisibilityGridFlags[ i ] = NULL;
			free( m_pVisibilityGridDegree[ i ] );
			m_pVisibilityGridDegree[ i ] = NULL;
			free( m_pVisibilityFadeTimer[ i ] );
			m_pVisibilityFadeTimer[ i ] = NULL;
		}
	}

	if ( m_pHorizontalSlices )
	{
		for ( int i = 0; i < m_nGridZUnits; i++ )
		{
			delete m_pHorizontalSlices[ i ];
		}
		delete m_pHorizontalSlices;

		m_pHorizontalSlices = NULL;
	}

	m_nGridZUnits = 0;

	if ( m_pVerticalLevels )
	{
		delete m_pVerticalLevels;
		m_pVerticalLevels = NULL;
	}

	m_Occluders.PurgeAndDeleteElements();
	m_Viewers.PurgeAndDeleteElements();
	m_TriSoupCollection.PurgeAndDeleteElements();

	for( unsigned int i = 0; i < m_RadiusTables.Count(); i++ )
	{
		if ( m_RadiusTables.IsValidIndex( i ) == false )
		{
			continue;
		}
		free( m_RadiusTables.Element( i ) );
	}
	m_RadiusTables.RemoveAll();

	m_ViewerTree.Purge();
	m_OccluderTree.Purge();

	m_nHorizontalGridAllocationSize = m_nVerticalGridAllocationSize = 0;
	m_nRadiusTableSize = 0;

	m_bInitialized = false;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the number of viewer teams
// Input  : nCount - the maximum number of teams
//-----------------------------------------------------------------------------
void CFoW::SetNumberOfTeams( int nCount )
{
	Assert( nCount > 0 && nCount <= MAX_FOW_TEAMS );

	m_nNumberOfTeams = nCount;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the world mins/maxs and how big the grid sizes should be
// Input  : vWorldMins - the world minimums
//			vWorldMaxs - the world maximums
//			nHorizontalGridSize - the horizontal size the world should be chopped up by ( world xy size / this value ) rounded up
//			nVerticalGridSize - the vertical size the world should be chopped up by ( world z size / this value ) rounded up
//-----------------------------------------------------------------------------
void CFoW::SetSize( Vector &vWorldMins, Vector &vWorldMaxs, int nHorizontalGridSize, int nVerticalGridSize )
{
	Assert( m_nNumberOfTeams > 0 );

	m_nHorizontalGridAllocationSize = m_nVerticalGridAllocationSize = 0;

	m_vWorldMins = vWorldMins;
	m_vWorldMaxs = vWorldMaxs;
	m_nHorizontalGridSize = nHorizontalGridSize;
	m_nVerticalGridSize = nVerticalGridSize;

	m_nGridXUnits = ( ( m_vWorldMaxs.x - m_vWorldMins.x ) + m_nHorizontalGridSize - 1 ) / m_nHorizontalGridSize;
	m_nGridYUnits = ( ( m_vWorldMaxs.y - m_vWorldMins.y ) + m_nHorizontalGridSize - 1 ) / m_nHorizontalGridSize;
	m_vWorldMaxs.x = m_vWorldMins.x + ( m_nGridXUnits * m_nHorizontalGridSize );
	m_vWorldMaxs.y = m_vWorldMins.y + ( m_nGridYUnits * m_nHorizontalGridSize );

	m_nTotalHorizontalUnits = m_nGridXUnits * m_nGridYUnits;
	int nDegreeAllocationSize = sizeof( *m_pVisibilityGridDegree[ 0 ] ) * m_nTotalHorizontalUnits;
	int nFlagAllocationSize = sizeof( *m_pVisibilityGridFlags[ 0 ] ) * m_nTotalHorizontalUnits;

	for ( int i = 0; i < m_nNumberOfTeams; i++ )
	{
		m_pVisibilityGridFlags[ i ] = ( byte * )malloc( nFlagAllocationSize );
		memset( m_pVisibilityGridFlags[ i ], 0, nFlagAllocationSize );
		m_nHorizontalGridAllocationSize += nFlagAllocationSize;

		m_pVisibilityGridDegree[ i ] = ( float * )malloc( nDegreeAllocationSize );
		m_nHorizontalGridAllocationSize += nDegreeAllocationSize;
		m_pVisibilityFadeTimer[ i ] = ( float * )malloc( nDegreeAllocationSize );
		m_nHorizontalGridAllocationSize += nDegreeAllocationSize;
//		memset( m_pVisibilityGridDegree[ i ], 0, nDegreeAllocationSize );
		for( int j = 0; j < m_nTotalHorizontalUnits; j++ )
		{
			m_pVisibilityGridDegree[ i ][ j ] = 0.0f;
			m_pVisibilityFadeTimer[ i ][ j ] = 0.0f;
		}
	}

	if ( m_nVerticalGridSize > 0 )
	{
		m_nGridZUnits = ( ( m_vWorldMaxs.z - m_vWorldMins.z ) + m_nVerticalGridSize - 1 ) / m_nVerticalGridSize;
		m_pVerticalLevels = ( float * )malloc( sizeof( m_pVerticalLevels[ 0 ] ) * m_nGridZUnits );
		m_pHorizontalSlices = ( CFoW_HorizontalSlice ** )malloc( sizeof( m_pHorizontalSlices[ 0 ] ) * m_nGridZUnits );
		for ( int i = 0; i < m_nGridZUnits; i++ )
		{
			m_pHorizontalSlices[ i ] = new CFoW_HorizontalSlice();
			m_nVerticalGridAllocationSize += sizeof( CFoW_HorizontalSlice );
			m_pVerticalLevels[ i ] = m_vWorldMins.z + ( ( i + 0.75f ) * m_nVerticalGridSize );
		}
		m_nVerticalGridAllocationSize += sizeof( m_pVerticalLevels[ 0 ] ) * m_nGridZUnits;
		m_nVerticalGridAllocationSize += sizeof( m_pHorizontalSlices[ 0 ] ) * m_nGridZUnits;
	}

	m_bInitialized = true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
//-----------------------------------------------------------------------------
void CFoW::SetCustomVerticalLevels( float *pflHeightLevels, int nCount )
{
	m_nGridZUnits = nCount;
	m_nVerticalGridSize = -1;

	m_pVerticalLevels = ( float * )malloc( sizeof( m_pVerticalLevels[ 0 ] ) * m_nGridZUnits );
	m_pHorizontalSlices = ( CFoW_HorizontalSlice ** )malloc( sizeof( m_pHorizontalSlices[ 0 ] ) * m_nGridZUnits );
	for ( int i = 0; i < m_nGridZUnits; i++ )
	{
		m_pHorizontalSlices[ i ] = new CFoW_HorizontalSlice();
		m_nVerticalGridAllocationSize += sizeof( CFoW_HorizontalSlice );
		m_pVerticalLevels[ i ] = pflHeightLevels[ i ];
	}

	m_nVerticalGridAllocationSize += sizeof( m_pVerticalLevels[ 0 ] ) * m_nGridZUnits;
	m_nVerticalGridAllocationSize += sizeof( m_pHorizontalSlices[ 0 ] ) * m_nGridZUnits;
}


//-----------------------------------------------------------------------------
// Purpose: get the world size of the FoW
// Output : vWorldMins - the world minimums
//			vWorldMaxs - the world maximums
//-----------------------------------------------------------------------------
void CFoW::GetSize( Vector &vWorldMins, Vector &vWorldMaxs )
{
	vWorldMins = m_vWorldMins;
	vWorldMaxs = m_vWorldMaxs;
}


//-----------------------------------------------------------------------------
// Purpose: get the lower vertical coord, the grid size, and grid units
// Output : nBottomZ - the world minimum z value
//			nGridSize - the size the world is chopped up by
//			nGridUnits - the number of units the world has been chopped into
//-----------------------------------------------------------------------------
void CFoW::GetVerticalGridInfo( int &nBottomZ, int &nGridSize, int &nGridUnits, float **pVerticalLevels )
{
	nBottomZ = m_vWorldMins.z;
	nGridSize = m_nVerticalGridSize;
	nGridUnits = m_nGridZUnits;
	*pVerticalLevels = m_pVerticalLevels;
}


//-----------------------------------------------------------------------------
// Purpose: snap the x/y coordinates to the grid
// Input  : vIn - the input coordinates
//			bGoLower - should we snap to the left/bottom or right/top of the grid
// Output : vOut - the output coordinates snapped to the grid.  z is unsnapped.
//-----------------------------------------------------------------------------
void CFoW::SnapCoordsToGrid( Vector &vIn, Vector &vOut, bool bGoLower )
{
	if ( bGoLower )
	{
		vOut.x = m_vWorldMins.x + ( floor( ( vIn.x - m_vWorldMins.x ) / m_nHorizontalGridSize ) * m_nHorizontalGridSize );
		vOut.y = m_vWorldMins.y + ( floor( ( vIn.y - m_vWorldMins.y ) / m_nHorizontalGridSize ) * m_nHorizontalGridSize );
		vOut.z = vIn.z;
	}
	else
	{
		vOut.x = m_vWorldMins.x + ( ceil( ( vIn.x - m_vWorldMins.x ) / m_nHorizontalGridSize ) * m_nHorizontalGridSize );
		vOut.y = m_vWorldMins.y + ( ceil( ( vIn.y - m_vWorldMins.y ) / m_nHorizontalGridSize ) * m_nHorizontalGridSize );
		vOut.z = vIn.z;
	}

	vOut.x = clamp( vOut.x, m_vWorldMins.x, m_vWorldMaxs.x );
	vOut.y = clamp( vOut.y, m_vWorldMins.y, m_vWorldMaxs.y );
}


//-----------------------------------------------------------------------------
// Purpose: return how visible a cell is ( 0.0 = not currently visible, 1.0 = fully visible )
// Input  : nXLoc - the world x location
//			nYLoc - the world y location
//			nTeam - which team to look up
// Output : returns the visibility degree
//-----------------------------------------------------------------------------
float CFoW::LookupVisibilityDegree( int nXLoc, int nYLoc, int nTeam )
{
	int x = ( nXLoc - m_vWorldMins.x ) / m_nHorizontalGridSize;
	int y = ( nYLoc - m_vWorldMins.y ) / m_nHorizontalGridSize;

	x = clamp( x, 0, m_nGridXUnits - 1 );
	y = clamp( y, 0, m_nGridYUnits - 1 );

	float flValue = m_pVisibilityGridDegree[ nTeam ][ ( x * m_nGridYUnits ) + y ];

	return ( flValue > 1.0f ? 1.0f : flValue );
}


//-----------------------------------------------------------------------------
// Purpose: creates or returns a grid to radius table
// Input  : flRadius - the size of the radius
// Output : returns the visibility table to go from grid to radius
//-----------------------------------------------------------------------------
int *CFoW::FindRadiusTable( float flRadius )
{
	int	nIndex = m_RadiusTables.Find( flRadius );

	if ( m_RadiusTables.IsValidIndex( nIndex ) )
	{
		return m_RadiusTables[ nIndex ];
	}

	int nGridUnits = ( ( flRadius * 2 ) + m_nHorizontalGridSize - 1 ) / m_nHorizontalGridSize;
	nGridUnits |= 1;	// always make it odd, so that we have a true center
	int nSize = sizeof( int ) * nGridUnits * nGridUnits;
	int	*pVisibilityData = ( int * )malloc( nSize );
	memset( pVisibilityData, -1, nSize );
	m_nRadiusTableSize += nSize;

	int		nOffset = ( nGridUnits / 2 ) * m_nHorizontalGridSize;
	int		nRadiusUnits = ( ( 2 * M_PI * flRadius ) + m_nHorizontalGridSize - 1 ) / m_nHorizontalGridSize;
	int		*pVisibility = pVisibilityData;

	for ( int x = 0, xPos = -nOffset; x < nGridUnits; x++, xPos += m_nHorizontalGridSize )
	{
		for ( int y = 0, yPos = -nOffset; y < nGridUnits; y++, yPos += m_nHorizontalGridSize, pVisibility++ )
		{
			float flDist = sqrt( ( float )( ( xPos * xPos ) + ( yPos * yPos ) ) );
			if ( flDist > flRadius )
			{
				*pVisibility = -1;
				continue;
			}

			float nx = xPos / flDist;
			float ny = yPos / flDist;

			float flAngle = ( 0.0f * nx ) + ( 1.0f * ny );
			float flRealAngle = RAD2DEG( acos( flAngle ) );

			if ( nx < 0.0f )
			{
				flRealAngle = 360 - flRealAngle;
			}

			flRealAngle = ( flRealAngle / 360.0f ) * nRadiusUnits;

			*pVisibility = ( int )flRealAngle;
		}
	}

	m_RadiusTables.Insert( flRadius, pVisibilityData );

	return pVisibilityData;
}

//-----------------------------------------------------------------------------
// Purpose: adds a new viewer to the system
// Input  : nViewerTeam - the team the viewer is on
// Output : returns the id of the new viewer
//-----------------------------------------------------------------------------
int CFoW::AddViewer( unsigned nViewerTeam )
{
	int		nSlotID = -1;

	// optimize this!
	for ( int i = 0; i < m_Viewers.Count(); i++ )
	{
		if ( m_Viewers[ i ] == NULL )
		{
			nSlotID = i;
			break;
		}
	}

	if ( nSlotID == -1 )
	{
		nSlotID = m_Viewers.Count();
		m_Viewers.AddToTail( NULL );
	}

	CFoW_Viewer	*pViewer = new CFoW_Viewer( nSlotID, nViewerTeam );
	m_Viewers[ nSlotID ] = pViewer;

	InsertViewerIntoTree( nSlotID );

	return nSlotID;
}


//-----------------------------------------------------------------------------
// Purpose: removes a viewer from the system
// Input  : nID - the id of the viewer
//-----------------------------------------------------------------------------
void CFoW::RemoveViewer( int nID )
{
	if ( m_Viewers[ nID ] != NULL )
	{
		RemoveViewerFromTree( nID );

		delete m_Viewers[ nID ];
		m_Viewers[ nID ] = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: updates the viewer's location
// Input  : nID - the id of the viewer
//			vLocation - the new location of the viewer
//-----------------------------------------------------------------------------
void CFoW::UpdateViewerLocation( int nID, const Vector &vLocation )
{
	Vector	vOldLocation;
	Assert( m_Viewers[ nID ] );

#ifdef FOW_SAFETY_DANCE
	if ( m_Viewers[ nID ] == NULL )
	{
		Warning( "CFoW: UpdateViewerLocation( %d, ( %g, %g %g ) ) has missing viewer\n", nID, vLocation.x, vLocation.y, vLocation.z );
		return;
	}
#endif

	if ( m_Viewers[ nID ]->UpdateLocation( this, vLocation, &vOldLocation ) == true )
	{
		RemoveViewerFromTree( nID, &vOldLocation );
		InsertViewerIntoTree( nID );
	}
}


//-----------------------------------------------------------------------------
// Purpose: updates the viewer's seeing radius
// Input  : nID - the id of the viewer
//			flRadius - the new radius of the viewer
//-----------------------------------------------------------------------------
void CFoW::UpdateViewerSize( int nID, float flRadius )
{
	Assert( m_Viewers[ nID ] );

#ifdef FOW_SAFETY_DANCE
	if ( m_Viewers[ nID ] == NULL )
	{
		Warning( "CFoW: UpdateViewerSize( %d, %g ) has missing viewer\n", nID, flRadius );
		return;
	}
#endif

	RemoveViewerFromTree( nID );
	m_Viewers[ nID ]->UpdateSize( this, flRadius );
	InsertViewerIntoTree( nID );
}


//-----------------------------------------------------------------------------
// Purpose: updates the viewer's seeing radius
// Input  : nID - the id of the viewer
//			nHeightGroup - the new height group of the viewer
//-----------------------------------------------------------------------------
void CFoW::UpdateViewerHeightGroup( int nID, uint8 nHeightGroup )
{
	Assert( m_Viewers[ nID ] );

#ifdef FOW_SAFETY_DANCE
	if ( m_Viewers[ nID ] == NULL )
	{
		Warning( "CFoW: UpdateViewerHeightGroup( %d, %d ) has missing viewer\n", nID, ( int )nHeightGroup );
		return;
	}
#endif

	m_Viewers[ nID ]->UpdateHeightGroup( nHeightGroup );
}


//-----------------------------------------------------------------------------
// Purpose: adds a new radius occluder to the system
// Input  : nPermanent - unused
// Output : returns the id of the new occluder
//-----------------------------------------------------------------------------
int CFoW::AddOccluder( bool nPermanent )
{
	int		nSlotID = -1;

	// optimize this!
	for ( int i = 0; i < m_Occluders.Count(); i++ )
	{
		if ( m_Occluders[ i ] == NULL )
		{
			nSlotID = i;
			break;
		}
	}

	if ( nSlotID == -1 )
	{
		nSlotID = m_Occluders.Count();
		m_Occluders.AddToTail( NULL );
	}

	CFoW_RadiusOccluder	*pOccluder = new CFoW_RadiusOccluder( nSlotID );
	m_Occluders[ nSlotID ] = pOccluder;

	InsertOccluderIntoTree( nSlotID );

	return nSlotID;
}


//-----------------------------------------------------------------------------
// Purpose: removes an occluder from the system
// Input  : nID - the id of the occluder
//-----------------------------------------------------------------------------
void CFoW::RemoveOccluder( int nID )
{
	if ( m_Occluders[ nID ] != NULL )
	{
		RemoveOccluderFromTree( nID );
		DirtyViewers( m_Occluders[ nID ]->GetLocation(), m_Occluders[ nID ]->GetSize() );

		delete m_Occluders[ nID ];
		m_Occluders[ nID ] = NULL;

//		RepopulateOccluders();		// CUtlSphereTree has no delete function for now
	}
}


void CFoW::EnableOccluder( int nID, bool bEnable )
{
#ifdef FOW_SAFETY_DANCE
	if ( m_Occluders[ nID ] == NULL )
	{
		Warning( "CFoW: EnableOccluder( %d, %s ) has missing occluder\n", nID, bEnable ? "true" : "false" );
		return;
	}
#endif

	m_Occluders[ nID ]->SetEnable( bEnable );
	DirtyViewers( m_Occluders[ nID ]->GetLocation(), m_Occluders[ nID ]->GetSize() );
}


//-----------------------------------------------------------------------------
// Purpose: update an occluder's location
// Input  : nID - the id of the occluder
//			vLocation - the new location of the occluder
//-----------------------------------------------------------------------------
void CFoW::UpdateOccluderLocation( int nID, Vector &vLocation )
{
	Assert( m_Occluders[ nID ] );

#ifdef FOW_SAFETY_DANCE
	if ( m_Occluders[ nID ] == NULL )
	{
		Warning( "CFoW: UpdateOccluderLocation( %d, ( %g, %g, %g ) ) has missing occluder\n", nID, vLocation.x, vLocation.y, vLocation.z );
		return;
	}
#endif

	RemoveOccluderFromTree( nID );
	DirtyViewers( m_Occluders[ nID ]->GetLocation(), m_Occluders[ nID ]->GetSize() );

	m_Occluders[ nID ]->UpdateLocation( vLocation );

//	RepopulateOccluders();		// CUtlSphereTree has no delete function for now
	InsertOccluderIntoTree( nID );
	DirtyViewers( m_Occluders[ nID ]->GetLocation(), m_Occluders[ nID ]->GetSize() );
}


//-----------------------------------------------------------------------------
// Purpose: update an occluder's size
// Input  : nID - the id of the occluder
//			flRadius - the new radius of the occluder
//-----------------------------------------------------------------------------
void CFoW::UpdateOccluderSize( int nID, float flRadius )
{
	Assert( m_Occluders[ nID ] );

#ifdef FOW_SAFETY_DANCE
	if ( m_Occluders[ nID ] == NULL )
	{
		Warning( "CFoW: UpdateOccluderSize( %d, %g ) has missing occluder\n", nID, flRadius );
		return;
	}
#endif

//	RepopulateOccluders();		// CUtlSphereTree has no delete function for now
	RemoveOccluderFromTree( nID );
	DirtyViewers( m_Occluders[ nID ]->GetLocation(), ( m_Occluders[ nID ]->GetSize() > flRadius ? m_Occluders[ nID ]->GetSize() : flRadius ) );

	m_Occluders[ nID ]->UpdateSize( flRadius );

	InsertOccluderIntoTree( nID );
}


//-----------------------------------------------------------------------------
// Purpose: updates the occluder's height group
// Input  : nID - the id of the occluder
//			nHeightGroup - the new height group of the occluder
//-----------------------------------------------------------------------------
void CFoW::UpdateOccluderHeightGroup( int nID, uint8 nHeightGroup )
{
	Assert( m_Occluders[ nID ] );

#ifdef FOW_SAFETY_DANCE
	if ( m_Occluders[ nID ] == NULL )
	{
		Warning( "CFoW: UpdateOccluderHeightGroup( %d, %d ) has missing occluder\n", nID, ( int )nHeightGroup );
		return;
	}
#endif

	//	RepopulateOccluders();		// CUtlSphereTree has no delete function for now
	DirtyViewers( m_Occluders[ nID ]->GetLocation(), m_Occluders[ nID ]->GetSize() );

	m_Occluders[ nID ]->UpdateHeightGroup( nHeightGroup );
}


//-----------------------------------------------------------------------------
// Purpose: internal function called by viewers to radius occlude nearby objects
// Input  : nViewerID - the id of the viewer to obstruct
//-----------------------------------------------------------------------------
void CFoW::ObstructOccludersNearViewer( int nViewerID )
{
	Assert( m_Viewers[ nViewerID ] );

	CFoW_Viewer				*pViewer = m_Viewers[ nViewerID ];

	Sphere_t				TestSphere( pViewer->GetLocation().x, pViewer->GetLocation().y, pViewer->GetLocation().z, pViewer->GetSize() );
	CFoW_RadiusOccluder		*FixedPointerArray[ FOW_MAX_RADIUS_OCCLUDERS_TO_CHECK ];
	CUtlVector< void * >	FoundOccluders( ( void ** )FixedPointerArray, FOW_MAX_RADIUS_OCCLUDERS_TO_CHECK );

	int RealCount = m_OccluderTree.IntersectWithSphere( TestSphere, true, FoundOccluders, FOW_MAX_RADIUS_OCCLUDERS_TO_CHECK );
	if ( RealCount > FOW_MAX_RADIUS_OCCLUDERS_TO_CHECK )
	{	
		// we overflowed, what should we do?
		Assert( 0 );
	}

	//	Msg( "Slice Counts: %d / %d\n", FoundOccluders.Count(), RealCount );

	for ( int i = 0; i < FoundOccluders.Count(); i++ )
	{
		FixedPointerArray[ i ]->ObstructViewerRadius( this, pViewer );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
//-----------------------------------------------------------------------------
void CFoW::SetWorldCollision( CPhysCollide *pCollideable, IPhysicsCollision	*pPhysCollision )
{
	for( int x = 0; x < m_nGridXUnits; x++ )
	{
		float	flXPos = ( x * m_nHorizontalGridSize ) + m_vWorldMins.x + ( m_nHorizontalGridSize / 2.0f );
		for( int y = 0; y < m_nGridYUnits; y++ )
		{
			float	flYPos = ( y * m_nHorizontalGridSize ) + m_vWorldMins.y + ( m_nHorizontalGridSize / 2.0f );
			Vector	vStart( flXPos, flYPos, 99999.0f ), vEnd( flXPos, flYPos, -99999.0f );

			Vector	vResultOrigin;
			QAngle	vResultAngles;
			trace_t TraceResult;

			pPhysCollision->TraceBox( vStart, vEnd, vec3_origin, vec3_origin, pCollideable, vResultOrigin, vResultAngles, &TraceResult );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: adds a tri soup collection to the system
// Output : returns the id of the new tri soup collection
//-----------------------------------------------------------------------------
int CFoW::AddTriSoup( )
{
	int		nSlotID = -1;

	// optimize this!
	for ( int i = 0; i < m_TriSoupCollection.Count(); i++ )
	{
		if ( m_TriSoupCollection[ i ] == NULL )
		{
			nSlotID = i;
			break;
		}
	}

	if ( nSlotID == -1 )
	{
		nSlotID = m_TriSoupCollection.Count();
		m_TriSoupCollection.AddToTail( NULL );
	}

	CFoW_TriSoupCollection *pTriSoup = new CFoW_TriSoupCollection( nSlotID );
	m_TriSoupCollection[ nSlotID ] = pTriSoup;

	return nSlotID;
}


//-----------------------------------------------------------------------------
// Purpose: removes a tri soup collection from the system 
// Input  : nID - the id of the tri soup
//-----------------------------------------------------------------------------
void CFoW::RemoveTriSoup( int nID )
{
	if ( m_TriSoupCollection[ nID ] != NULL )
	{
		delete m_TriSoupCollection[ nID ];
		m_TriSoupCollection[ nID ] = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: clears all entries from the collection ( useful for hammer editing only )
// Input  : nID - the id of the tri soup
//-----------------------------------------------------------------------------
void CFoW::ClearTriSoup( int nID )
{
	Assert( m_TriSoupCollection[ nID ] );

	m_TriSoupCollection[ nID ]->Clear();

	for ( int i = 0; i < m_nGridZUnits; i++ )
	{
		m_pHorizontalSlices[ i ]->Clear();
	}

	for ( int i = 0; i < m_TriSoupCollection.Count(); i++ )
	{
		if ( m_TriSoupCollection[ i ] )
		{
			m_TriSoupCollection[ i ]->RepopulateOccluders( this );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: adds a tri to the collection.  this is immediately split up into the horizontal slices.  very slow!
// Input  : nID - the id of the tri soup
//			vPointA - a point on the tri
//			vPointB - a point on the tri
//			vPointC - a point on the tri
//-----------------------------------------------------------------------------
void CFoW::AddTri( int nID, Vector &vPointA, Vector &vPointB, Vector &vPointC )
{
	Assert( m_TriSoupCollection[ nID ] );

	m_TriSoupCollection[ nID ]->AddTri( this, vPointA, vPointB, vPointC );
}


//-----------------------------------------------------------------------------
// Purpose: get access to a tri soup collection object
// Input  : nID - the id of the tri soup
// Output : returns the tri soup collection object
//-----------------------------------------------------------------------------
CFoW_TriSoupCollection *CFoW::GetTriSoup( int nID )
{
	Assert( m_TriSoupCollection[ nID ] );

	return m_TriSoupCollection[ nID ];
}


//-----------------------------------------------------------------------------
// Purpose: add a line occulder from a horizontal slice
// Input  : pOccluder - the line occluder to add
//			nSliceNum - which slice to add the occluder on
//-----------------------------------------------------------------------------
void CFoW::AddTriSoupOccluder( CFoW_LineOccluder *pOccluder, int nSliceNum )
{
	m_pHorizontalSlices[ nSliceNum ]->AddHorizontalOccluder( pOccluder );
}


//-----------------------------------------------------------------------------
// Purpose: get the slice index given the vertical position
// Input  : flZPos - the world z position to find the slice for
// Output : returns the slice index or -1 if the position is out of range
//-----------------------------------------------------------------------------
int CFoW::GetHorizontalSlice( float flZPos )
{
	if ( m_nVerticalGridSize == 0 || m_pHorizontalSlices == NULL )
	{
		return -1;
	}

#if 0
	int nIndex = ( int )( ( flZPos - m_vWorldMins.z ) / m_nVerticalGridSize );
	if ( nIndex < 0 )
	{	// we are getting a z position outside of our world size - potentially bad
		return 0;
	}
	else if ( nIndex >= m_nGridZUnits )
	{	// we are getting a z position outside of our world size - potentially bad
		return m_nGridZUnits - 1;
	}
#endif

	for ( int nSlice = 0; nSlice < m_nGridZUnits; nSlice++ )
	{
		if ( flZPos < m_pVerticalLevels[ nSlice ] )
		{
			return nSlice;
		}
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void PrepVisibilityThreaded( CFoW *pFoW )
{
	pFoW->PrepVisibility();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CalcLocalizedVisibilityThreaded( CFoW *pFoW, CFoW_Viewer *pViewer )
{
	pViewer->CalcLocalizedVisibility( pFoW );
}


// #define TIME_ME	1

//-----------------------------------------------------------------------------
// Purpose: solve the visibility for all teams and all viewers - slow!
// Input  : flFrameTime - the time since the last visibility solve.  The amount 
//				of change in the visibility degree is dependent upon this value.
//-----------------------------------------------------------------------------
void CFoW::SolveVisibility( float flFrameTime )
{
	VPROF_BUDGET( "CFoW::SolveVisibility", VPROF_BUDGETGROUP_OTHER_UNACCOUNTED );

#if 1
#ifdef TIME_ME
	uint32	nThreadedTime = Plat_MSTime();
#endif // #ifdef TIME_ME
	int	nRealCount = 1;

	for ( int i = 0; i < m_Viewers.Count(); i++ )
	{
		if ( m_Viewers[ i ] == NULL )
		{
			continue;
		}
		
#ifdef FOW_SAFETY_DANCE
		if ( m_Viewers[ i ]->GetSize() <= 1.0f )
		{
			Warning( "CFoW: Viewer %d has invalid radius!\n", i );
			continue;
		}
#endif

		if ( m_Viewers[ i ] != NULL && m_Viewers[ i ]->IsDirty() == true )
		{
			nRealCount++;
		}
	}

	CJob			**pJobs = ( CJob ** )stackalloc( sizeof( CJob * ) * nRealCount );
	CThreadEvent	**pHandles = ( CThreadEvent ** )stackalloc( sizeof( CThreadEvent * ) * nRealCount );

	nRealCount = 0;

	pJobs[ nRealCount ] = new CFunctorJob( CreateFunctor( ::PrepVisibilityThreaded, this ) );
	pJobs[ nRealCount ]->SetFlags( JF_QUEUE );
	g_pThreadPool->AddJob( pJobs[ nRealCount ] );
	pHandles[ nRealCount ] = pJobs[ nRealCount ]->AccessEvent();
	nRealCount++;

//	PrepVisibilityThreaded( this );

	for ( int i = 0; i < m_Viewers.Count(); i++ )
	{
		if ( m_Viewers[ i ] == NULL )
			continue;

		if ( m_Viewers[ i ]->GetSize() <= 1.0f )
		{
			continue;
		}

		if ( m_Viewers[ i ] != NULL && m_Viewers[ i ]->IsDirty() == true )
		{
			pJobs[ nRealCount ] = new CFunctorJob( CreateFunctor( ::CalcLocalizedVisibilityThreaded, this, m_Viewers[ i ] ) );
			pJobs[ nRealCount ]->SetFlags( JF_QUEUE );
			g_pThreadPool->AddJob( pJobs[ nRealCount ] );
			pHandles[ nRealCount ] = pJobs[ nRealCount ]->AccessEvent();
			nRealCount++;
//			CalcLocalizedVisibilityThreaded( this, m_Viewers[ i ] );
		}
	}

	g_pThreadPool->YieldWait( pHandles, nRealCount, true, TT_INFINITE );

#ifdef TIME_ME
	uint32	nMergeTime = Plat_MSTime();
#endif // #ifdef TIME_ME

	for ( int i = 0; i < m_Viewers.Count(); i++ )
	{
		if ( m_Viewers[ i ] )
		{
			MergeViewerVisibility( i );
		}
	}

#ifdef TIME_ME
	uint32 nUpdateTime = Plat_MSTime();
#endif // #ifdef TIME_ME

	UpdateVisibleAmounts( flFrameTime );
	
#ifdef TIME_ME
	uint32 nFinishTime = Plat_MSTime();
	Msg( "Thread: %d, Merge: %d, Update: %d, Total %d\n", nMergeTime - nThreadedTime, nUpdateTime - nMergeTime, nFinishTime - nUpdateTime, nFinishTime - nThreadedTime );
#endif // #ifdef TIME_ME

#else
	uint32 Time1 = Plat_MSTime();
	PrepVisibility();
	uint32 Time2 = Plat_MSTime();

	double LocalTime = 0.0;
	double MergeTime = 0.0;

	for ( int i = 0; i < m_Viewers.Count(); i++ )
	{
		if ( m_Viewers[ i ] )
		{
			double t1 = Plat_FloatTime();
			m_Viewers[ i ]->CalcLocalizedVisibility( this );
			double t2 = Plat_FloatTime();
			MergeViewerVisibility( i );
			double t3 = Plat_FloatTime();

			LocalTime += ( t2 - t1 );
			MergeTime += ( t3 - t2 );
		}
	}

	uint32 Time3 = Plat_MSTime();
	UpdateVisibleAmounts( flFrameTime );
	uint32 Time4 = Plat_MSTime();

	Msg( "Prep: %d, Local %lg, Merge: %lg, Update: %d, Total: %d\n", Time2-Time1, LocalTime * 1000, MergeTime * 1000, Time4-Time3, Time4 - Time1 );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: returns the visibility info of a location to a team
// Input  : nViewerTeam - the team that is doing the viewing of this location
//			vLocation - the world location to get the results
// Output : returns bits associated with the visilbity of the grid location
//-----------------------------------------------------------------------------
uint8 CFoW::GetLocationInfo( unsigned nViewerTeam, const Vector &vLocation )
{
	int		nIndex = GetGridIndex( vLocation, 0.0f, 0.0f, true );
#ifdef FOW_SAFETY_DANCE
	if ( nIndex < 0 || nIndex >= m_nTotalHorizontalUnits )
	{
		Warning( "CFoW: GetLocationInfo() called with invalid view location of %g, %g, %g\n", vLocation.x, vLocation.y, vLocation.z );

		return FOW_VG_INVALID;
	}
#endif

	byte	*pDestPos = m_pVisibilityGridFlags[ nViewerTeam ] + nIndex;

	return ( *pDestPos );
}


//-----------------------------------------------------------------------------
// Purpose: returns the visibility info of a location to a team
// Input  : nViewerTeam - the team that is doing the viewing of this location
//			vLocation - the world location to get the results
// Output : returns bits associated with the visilbity of the grid location
//-----------------------------------------------------------------------------
float CFoW::GetLocationVisibilityDegree( unsigned nViewerTeam, const Vector &vLocation, float flRadius )
{
	if ( flRadius <= 1.0f )
	{
		int	nGridX, nGridY;

		GetGridUnits( vLocation, 0.0f, 0.0f, true, nGridX, nGridY );

		int		nIndex = ( ( nGridX * m_nGridYUnits ) + nGridY );

#ifdef FOW_SAFETY_DANCE
		if ( nIndex < 0 || nIndex >= m_nTotalHorizontalUnits )
		{
			Warning( "CFoW: GetLocationVisibilityDegree() called with invalid view location of %g, %g, %g\n", vLocation.x, vLocation.y, vLocation.z );
			return 0.0f;
		}
#endif

		float	*pDestPos = m_pVisibilityGridDegree[ nViewerTeam ] + nIndex;
		return ( ( *pDestPos ) > 1.0f ? 1.0f : ( *pDestPos ) );
	}

	float	flBestDegree = 0.0f;

	int		nMinGridX, nMinGridY;
	int		nMaxGridX, nMaxGridY;
	Vector	vDelta( flRadius + 16.0f, flRadius + 16.0f, 0 );

	Vector	vMin = vLocation - vDelta;
	Vector	vMax = vLocation + vDelta;

	GetGridUnits( vMin, 0.0f, 0.0f, true, nMinGridX, nMinGridY );
	GetGridUnits( vMax, 0.0f, 0.0f, true, nMaxGridX, nMaxGridY );

	int nCount = 0;
	float flTotal = 0.0f;

	for( int nXOffset = nMinGridX; nXOffset <= nMaxGridX; nXOffset++ )
	{
		if ( nXOffset < 0 || nXOffset >= m_nGridXUnits )
		{
			continue;
		}
		for( int nYOffset = nMinGridY; nYOffset <= nMaxGridY; nYOffset++ )
		{
			if ( nYOffset < 0 || nYOffset >= m_nGridYUnits )
			{
				continue;
			}

			int nLocation = ( ( nXOffset * m_nGridYUnits ) + nYOffset );

			float	*pDestPos = m_pVisibilityGridDegree[ nViewerTeam ] + nLocation;

			flTotal += ( ( *pDestPos ) > 1.0f ? 1.0f : ( *pDestPos ) ); 
			nCount++;

#if 0
			if ( ( *pDestPos ) > flBestDegree )
			{
				flBestDegree = ( *pDestPos );

				if ( flBestDegree == 1.0f )
				{
					return 1.0f;
				}
			}
#endif
		}
	}

	if ( nCount > 0 )
	{
		flBestDegree = flTotal / nCount;
	}
	else
	{
#ifdef FOW_SAFETY_DANCE
		Warning( "CFoW: GetLocationVisibilityDegree() called with invalid view location of %g, %g, %g\n", vLocation.x, vLocation.y, vLocation.z );
#endif // #ifdef FOW_SAFETY_DANCE
	}

	return flBestDegree;
}


//-----------------------------------------------------------------------------
// Purpose: adds an viewer to the sphere tree
// Input:	nIndex - the index of the viewer
//-----------------------------------------------------------------------------
void CFoW::InsertViewerIntoTree( int nIndex )
{
	if ( m_Viewers[ nIndex ] != NULL )
	{
		Sphere_t	Bounds;

		Bounds.AsVector3D() = m_Viewers[ nIndex ]->GetLocation();
		Bounds.w = m_Viewers[ nIndex ]->GetSize();

		m_ViewerTree.Insert( (void *)m_Viewers[ nIndex ], &Bounds );
	}
}


//-----------------------------------------------------------------------------
// Purpose: removes an viewer from the sphere tree
// Input:	nIndex - the index of the viewer
//-----------------------------------------------------------------------------
void CFoW::RemoveViewerFromTree( int nIndex, Vector *pvOldLocation )
{
	if ( m_Viewers[ nIndex ] != NULL )
	{
		Sphere_t	Bounds;

		if ( pvOldLocation != NULL )
		{
			Bounds.AsVector3D() = *pvOldLocation;
		}
		else
		{
			Bounds.AsVector3D() = m_Viewers[ nIndex ]->GetLocation();
		}
		Bounds.w = m_Viewers[ nIndex ]->GetSize();

		m_ViewerTree.Remove( (void *)m_Viewers[ nIndex ], &Bounds );
	}
}


void CFoW::DirtyViewers( Vector &vLocation, float flRadius )
{
//	CFoW_RadiusOccluder		*pOcculder = m_Occluders[ nOccluderID ];

	Sphere_t				TestSphere( vLocation.x, vLocation.y, vLocation.z, flRadius );
	CFoW_Viewer				*FixedPointerArray[ FOW_MAX_VIEWERS_TO_CHECK ];
	CUtlVector< void * >	FoundViewers( ( void ** )FixedPointerArray, FOW_MAX_VIEWERS_TO_CHECK );

	int RealCount = m_ViewerTree.IntersectWithSphere( TestSphere, true, FoundViewers, FOW_MAX_VIEWERS_TO_CHECK );
	if ( RealCount > FOW_MAX_VIEWERS_TO_CHECK )
	{	
		// we overflowed, what should we do?
		Assert( 0 );
	}

	for ( int i = 0; i < FoundViewers.Count(); i++ )
	{
		FixedPointerArray[ i ]->Dirty();
	}
}


//-----------------------------------------------------------------------------
// Purpose: adds all occluders back into the visibility tree
//-----------------------------------------------------------------------------
void CFoW::RepopulateOccluders( )
{
	m_OccluderTree.RemoveAll();

	for ( int i = 0; i < m_Occluders.Count(); i++ )
	{
		if ( m_Occluders[ i ] != NULL )
		{
			Sphere_t	Bounds;

			Bounds.AsVector3D() = m_Occluders[ i ]->GetLocation();
			Bounds.w = m_Occluders[ i ]->GetSize();

			m_OccluderTree.Insert( (void *)m_Occluders[ i ], &Bounds );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: adds an occluder to the sphere tree
// Input:	nIndex - the index of the occluder
//-----------------------------------------------------------------------------
void CFoW::InsertOccluderIntoTree( int nIndex )
{
	if ( m_Occluders[ nIndex ] != NULL )
	{
		Sphere_t	Bounds;

		Bounds.AsVector3D() = m_Occluders[ nIndex ]->GetLocation();
		Bounds.w = m_Occluders[ nIndex ]->GetSize();

		m_OccluderTree.Insert( (void *)m_Occluders[ nIndex ], &Bounds );
	}
}


//-----------------------------------------------------------------------------
// Purpose: removes an occluder from the sphere tree
// Input:	nIndex - the index of the occluder
//-----------------------------------------------------------------------------
void CFoW::RemoveOccluderFromTree( int nIndex )
{
	if ( m_Occluders[ nIndex ] != NULL )
	{
		Sphere_t	Bounds;

		Bounds.AsVector3D() = m_Occluders[ nIndex ]->GetLocation();
		Bounds.w = m_Occluders[ nIndex ]->GetSize();

		m_OccluderTree.Remove( (void *)m_Occluders[ nIndex ], &Bounds );
	}
}


//-----------------------------------------------------------------------------
// Purpose: defaults the viewing grids
//-----------------------------------------------------------------------------
void CFoW::PrepVisibility( )
{
	int nSize = m_nGridXUnits * m_nGridYUnits;

	for ( int i = 0; i < m_nNumberOfTeams; i++ )
	{
		byte	*pFlagPtr = m_pVisibilityGridFlags[ i ];
		byte	*pFlagEndPtr = pFlagPtr + nSize;

		for ( ; pFlagPtr < pFlagEndPtr; pFlagPtr++ )
		{
			if ( ( ( *pFlagPtr ) & ( FOW_VG_IS_VISIBLE ) ) == FOW_VG_IS_VISIBLE )
			{
				( *pFlagPtr ) &= ~FOW_VG_IS_VISIBLE;

				if ( ( ( *pFlagPtr ) & ( FOW_VG_WAS_VISIBLE ) ) == 0 )
				{
					( *pFlagPtr ) |= FOW_VG_WAS_VISIBLE;
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: updates the viewer grids
// Input  : flFrameTime - the time since the last visibility solve.  The amount 
//				of change in the visibility degree is dependent upon this value.
//-----------------------------------------------------------------------------
void CFoW::UpdateVisibleAmounts( float flFrameTime )
{
	int nSize = m_nGridXUnits * m_nGridYUnits;

	flFrameTime /= m_flDegreeFadeRate;

	for ( int i = 0; i < m_nNumberOfTeams; i++ )
	{
		byte	*pFlagPtr = m_pVisibilityGridFlags[ i ];
		byte	*pFlagEndPtr = pFlagPtr + nSize;
		float	*pDegreePtr = m_pVisibilityGridDegree[ i ];
		float	*pFadeTimePtr = m_pVisibilityFadeTimer[ i ];

		for ( ; pFlagPtr < pFlagEndPtr; pFlagPtr++, pDegreePtr++, pFadeTimePtr++ )
		{
			if ( ( ( *pFlagPtr ) & FOW_VG_IS_VISIBLE ) != 0 )
			{
				if ( ( *pDegreePtr ) < FOW_OVER_VISIBILITY )
				{
					( *pDegreePtr ) += flFrameTime;
					if ( ( *pDegreePtr ) > FOW_OVER_VISIBILITY )
					{
						( *pDegreePtr ) = FOW_OVER_VISIBILITY;
					}
				}
				( *pFadeTimePtr ) = FOW_FADE_DELAY;
			}
			else
			{
				if ( ( *pFadeTimePtr ) > 0.0f )
				{
					// worry about going negative and using up the remainder?
					( *pFadeTimePtr ) -= flFrameTime;
				}
				else if ( ( *pDegreePtr ) > 0.0f )
				{
					( *pDegreePtr ) -= flFrameTime;
					if ( ( *pDegreePtr ) < 0.0f )
					{
						( *pDegreePtr ) = 0.0f;
					}
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: given the coords and an offset to move BACK, finds the grid index
// Input  : vCoords - the world coordinates ( z is ignored )
//			flXOffset - the x offset to SUBTRACT from the world coordinates
//			flXOffset - the y offset to SUBTRACT from the world coordinates
//			bGoLower - should we snap to the left/bottom or right/top of the grid
// Output : returns the index into the grids.  returns -1 if it is invalid.
//-----------------------------------------------------------------------------
int CFoW::GetGridIndex( const Vector &vCoords, float flXOffset, float flYOffset, bool bGoLower )
{
	int	nGridX, nGridY;

	GetGridUnits( vCoords, flXOffset, flYOffset, bGoLower, nGridX, nGridY );

#if 0
	if ( nGridX < 0 || nGridX >= m_nGridXUnits ||
		 nGridY < 0 || nGridY >= m_nGridYUnits )
	{
		return -1;
	}
#endif

	return ( nGridX * m_nGridYUnits ) + nGridY;
}


//-----------------------------------------------------------------------------
// Purpose: given the coords and an offset to move BACK, finds the grid location
// Input  : vCoords - the world coordinates ( z is ignored )
//			flXOffset - the x offset to SUBTRACT from the world coordinates
//			flXOffset - the y offset to SUBTRACT from the world coordinates
//			bGoLower - should we snap to the left/bottom or right/top of the grid
// Output : nGridX - the x grid location
//			nGridY - the y grid location
//-----------------------------------------------------------------------------
void CFoW::GetGridUnits( const Vector &vCoords, float flXOffset, float flYOffset, bool bGoLower, int &nGridX, int &nGridY )
{
	if ( bGoLower )
	{
		nGridX = floor( ( vCoords.x - flXOffset - m_vWorldMins.x ) / m_nHorizontalGridSize );
		nGridY = floor( ( vCoords.y - flYOffset - m_vWorldMins.y ) / m_nHorizontalGridSize );
	}
	else
	{
		nGridX = ceil( ( vCoords.x - flXOffset - m_vWorldMins.x ) / m_nHorizontalGridSize );
		nGridY = ceil( ( vCoords.y - flYOffset - m_vWorldMins.y ) / m_nHorizontalGridSize );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CFoW::CenterCoordToGrid( Vector &vCoords )
{
	int nGridX, nGridY;

	GetGridUnits( vCoords, 0.0f, 0.0f, true, nGridX, nGridY );

	vCoords.x = m_vWorldMins.x + ( nGridX * m_nHorizontalGridSize ) + ( m_nHorizontalGridSize / 2.0f );
	vCoords.y = m_vWorldMins.y + ( nGridY * m_nHorizontalGridSize ) + ( m_nHorizontalGridSize / 2.0f );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CFoW::SetDebugVisibility( bool bVisible )
{
	m_bDebugVisible = bVisible;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CFoW::EnableDebugFlags( unsigned nFlags )
{
	m_nDebugFlags |= nFlags;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CFoW::DisableDebugFlags( unsigned nFlags )
{
	m_nDebugFlags &= ~nFlags;
}


//-----------------------------------------------------------------------------
// Purpose: merge a local viewer's visibility to the global grid
// Input  : nID - the id of the viewer to merge in
//-----------------------------------------------------------------------------
void CFoW::MergeViewerVisibility( int nID )
{
	int nStartX, nStartY, nEndX, nEndY, nWidth, nHeight;

	CFoW_Viewer *pViewer = m_Viewers[ nID ];
	if ( !pViewer )
	{
		return;
	}

	nWidth = nHeight = pViewer->GetGridUnits();
	nStartX = 0;
	nStartY = 0;

	GetGridUnits( pViewer->GetLocation(), pViewer->GetSize(), pViewer->GetSize(), true, nStartX, nStartY );
	nEndX = nStartX + nWidth;
	nEndY = nStartY + nHeight;

	byte	*pSrcPos = pViewer->GetVisibility();

	int		nIndex = GetGridIndex( pViewer->GetLocation(), pViewer->GetSize(), pViewer->GetSize(), true );

	byte	*pDestPos = m_pVisibilityGridFlags[ pViewer->GetTeam() ] + nIndex;
	int		nYDestStride = m_nGridYUnits - nHeight;
	int		nYSrcStride = 0;

	if ( nStartX < 0 )
	{
		pDestPos += ( -nStartX ) * m_nGridYUnits;
		pSrcPos += ( -nStartX ) * nHeight;
		nStartX = 0;
	}
	if ( nEndX > m_nGridXUnits )
	{
		nEndX -= ( nEndX - m_nGridXUnits );
	}

	if ( nStartY < 0 )
	{
		pDestPos += ( -nStartY );
		pSrcPos += ( -nStartY );
		nYDestStride += ( -nStartY );
		nYSrcStride += ( -nStartY );
		nStartY = 0;
	}
	if ( nEndY > m_nGridYUnits )
	{
		nYDestStride += ( nEndY - m_nGridYUnits );
		nYSrcStride += ( nEndY - m_nGridYUnits );
		nEndY -= ( nEndY - m_nGridYUnits );
	}

	uint8	nViewerHeightGroup = pViewer->GetHeightGroup();
	int		nCount = 0;

	for ( int x = nStartX; x < nEndX; x++, pDestPos += nYDestStride, pSrcPos += nYSrcStride)
	{
		for ( int y = nStartY; y < nEndY; y++, pSrcPos++, pDestPos++ )
		{
			nCount++;
			*pDestPos |= *pSrcPos;

			if ( ( ( *pSrcPos ) & FOW_VG_IS_VISIBLE ) != 0 && ( ( *pDestPos ) & FOW_VG_MAX_HEIGHT_GROUP ) < nViewerHeightGroup )
			{
				( *pDestPos ) = ( ( *pDestPos ) & ( ~FOW_VG_MAX_HEIGHT_GROUP ) ) | nViewerHeightGroup;
			}
			// handle height group in bits
		}
	}

#ifdef FOW_SAFETY_DANCE
	if ( nCount == 0 )
	{	// either radius that is too small ( or invalid ) or the location is off the grid
		Warning( "CFoW: MergeViewerVisibility() Viewer %d has no contribution at location of %g, %g, %g\n", nID, pViewer->GetLocation().x, pViewer->GetLocation().y, pViewer->GetLocation().z );
	}
#endif 
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CFoW::DrawDebugInfo( Vector &vLocation, float flViewRadius )
{
	if ( m_bDebugVisible == false )
	{
		return;
	}

//	debugoverlay->AddBoxOverlay( Vector( 0.0f, 0.0f, 0.0f ), Vector( -512.0f, -512.0f, -512.0f ), Vector( 512.0f, 512.0f, 512.0f ), QAngle( 0, 0, 0 ), 0, 255, 0, 16, 0 );
//	debugoverlay->AddSphereOverlay( Vector( 0.0f, 0.0f, 0.0f ), 512.0f, 10, 10, 255, 0, 0, 127, 0 );

	if ( ( m_nDebugFlags & ( FOW_DEBUG_SHOW_GRID ) ) != 0 )
	{
		const float flGridOffset = 4.0f;

		for( int x = 0; x < m_nGridXUnits; x++ )
		{
			float flRealStartX = ( x * m_nHorizontalGridSize ) + m_vWorldMins.x + flGridOffset;
			float flRealEndX = flRealStartX + m_nHorizontalGridSize - flGridOffset - flGridOffset;
			float flCenterX = ( flRealStartX + flRealEndX ) / 2.0f;

			for( int y = 0; y < m_nGridYUnits; y++ )
			{
				float flRealStartY = ( y * m_nHorizontalGridSize ) + m_vWorldMins.y + flGridOffset;
				float flRealEndY = flRealStartY + m_nHorizontalGridSize - flGridOffset - flGridOffset;
				float flCenterY = ( flRealStartY + flRealEndY ) / 2.0f;
			
				Vector vDiff = Vector( flCenterX, flCenterY, 0.0f ) - vLocation;
				if ( vDiff.Length2D() > flViewRadius + m_nHorizontalGridSize )
				{
					continue;
				}

				int r, g, b;

				int nLocation = ( ( x * m_nGridYUnits ) + y );
				float	*pDestPos = m_pVisibilityGridDegree[ 0 ] + nLocation;
				float	flValue = ( ( *pDestPos ) > 1.0f ? 1.0f : ( *pDestPos ) );

				g = 255 * flValue;
				r = 255 - g;
				b = 0;

				debugoverlay->AddLineOverlay( Vector( flRealStartX, flRealStartY, 0.0f ), Vector( flRealEndX, flRealStartY, 0.0f ), r, g, b, true, FOW_DEBUG_VIEW_TIME );
				debugoverlay->AddLineOverlay( Vector( flRealEndX, flRealStartY, 0.0f ), Vector( flRealEndX, flRealEndY, 0.0f ), r, g, b, true, FOW_DEBUG_VIEW_TIME );
				debugoverlay->AddLineOverlay( Vector( flRealEndX, flRealEndY, 0.0f ), Vector( flRealStartX, flRealEndY, 0.0f ), r, g, b, true, FOW_DEBUG_VIEW_TIME );
				debugoverlay->AddLineOverlay( Vector( flRealStartX, flRealEndY, 0.0f ), Vector( flRealStartX, flRealStartY, 0.0f ), r, g, b, true, FOW_DEBUG_VIEW_TIME );
			}
		}
	}

	for ( int i = 0; i < m_Viewers.Count(); i++ )
	{
		if ( m_Viewers[ i ] == NULL )
		{
			continue;
		}

		m_Viewers[ i ]->DrawDebugInfo( vLocation, flViewRadius, m_nDebugFlags );
	}

	for ( int i = 0; i < m_Occluders.Count(); i++ )
	{
		if ( m_Occluders[ i ] == NULL )
		{
			continue;
		}

		m_Occluders[ i ]->DrawDebugInfo( vLocation, flViewRadius, m_nDebugFlags );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CFoW::PrintStats( )
{
	int		nNumViewers = 0;
	size_t	nViewerSize = 0;
	int		nNumOccluders = 0;
	size_t	nOccluderSize = 0;
	int		nNumTriSoupCollections = 0;
	size_t	nTriSoupCollectionSize = 0;
	int		nNumLineOccluders = 0;
	size_t	nLineOccluderSize = 0;
	size_t	nTotal = 0;

	for ( int i = 0; i < m_Viewers.Count(); i++ )
	{
		if ( m_Viewers[ i ] == NULL )
		{
			continue;
		}

		nNumViewers++;
		nViewerSize += sizeof( CFoW_Viewer );
		nViewerSize += m_Viewers[ i ]->GetAllocatedMemory();
	}


	for ( int i = 0; i < m_Occluders.Count(); i++ )
	{
		if ( m_Occluders[ i ] == NULL )
		{
			continue;
		}

		nNumOccluders++;
		nOccluderSize += sizeof( CFoW_RadiusOccluder );
	}

	for( int i = 0; i < m_TriSoupCollection.Count(); i++ )
	{
		nNumTriSoupCollections++;
		nTriSoupCollectionSize += sizeof( CFoW_TriSoupCollection );

		for( int j = 0; j < m_TriSoupCollection[ i ]->GetNumOccluders(); j++ )
		{
//			CFoW_LineOccluder	*pOccluder = m_TriSoupCollection[ i ]->GetOccluder( j );

			nNumLineOccluders++;
			nLineOccluderSize += sizeof( CFoW_LineOccluder );
		}
	}

	Msg( "FoW Stats\n" );
	Msg( "   Num Active Viewers: %d\n", nNumViewers );
	Msg( "   Num Active Radius Occluders: %d\n", nNumOccluders );
	Msg( "   Num Tri Soup Collections: %d\n", nNumTriSoupCollections );
	Msg( "   Num Active Line Occluders: %d\n", nNumLineOccluders );
	Msg( "FoW Memory\n" );
	Msg( "   Horizontal Grid Allocation Size: %d\n", m_nHorizontalGridAllocationSize );
	Msg( "   Veritcal Grid Allocation Size: %d\n", m_nVerticalGridAllocationSize );
	Msg( "   Radius Table Size: %d\n", m_nRadiusTableSize );
	Msg( "   Viewers Size: %d\n", nViewerSize );
	Msg( "   Radius Occluders Size: %d\n", nOccluderSize );
	Msg( "   Tri Soup Collection Size: %d\n", nTriSoupCollectionSize );
	Msg( "   Line Occluders Size: %d\n", nLineOccluderSize );

	nTotal = m_nHorizontalGridAllocationSize + m_nVerticalGridAllocationSize + m_nRadiusTableSize + nViewerSize + nOccluderSize + nTriSoupCollectionSize + nLineOccluderSize;
	Msg( "   --------------------------------------\n");
	Msg( "   Approximate Total Size: %d\n", nTotal );

}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CFoW::GenerateVMF( IFileSystem *pFileSystem, const char *pszFileName )
{
	KeyValues	*kv = new KeyValues( NULL );
	char		temp[ 128 ];
	int			nCount = 1;

	KeyValues *pWorldKV = new KeyValues( "world" );
	pWorldKV->SetInt( "id", nCount );
	nCount++;
	pWorldKV->SetInt( "mapversion", 22 );
	pWorldKV->SetString( "classname", "worldspawn" );
	pWorldKV->SetInt( "fow", 1 );
	sprintf( temp, "%g %g %g", m_vWorldMins.x, m_vWorldMins.y, m_vWorldMins.z );
	pWorldKV->SetString( "m_vWorldMins", temp );
	sprintf( temp, "%g %g %g", m_vWorldMaxs.x, m_vWorldMaxs.y, m_vWorldMaxs.z );
	pWorldKV->SetString( "m_vWorldMaxs", temp );
	pWorldKV->SetInt( "m_nHorizontalGridSize", m_nHorizontalGridSize );
	pWorldKV->SetInt( "m_nVerticalGridSize", m_nVerticalGridSize );
	pWorldKV->SetInt( "m_nGridXUnits", m_nGridXUnits );
	pWorldKV->SetInt( "m_nGridYUnits", m_nGridYUnits );
	pWorldKV->SetInt( "m_nGridZUnits", m_nGridZUnits );
	for ( int i = 0; i < m_nGridZUnits; i++ )
	{
		sprintf( temp, "m_pVerticalLevels_%d", i );
		pWorldKV->SetFloat( temp, m_pVerticalLevels[ i ] );
	}

	kv->AddSubKey( pWorldKV );

	for( int i = 0; i < m_Viewers.Count(); i++ )
	{
		CFoW_Viewer	*pViewer = m_Viewers[ i ];

		if ( pViewer )
		{
			KeyValues *pViewerKV = new KeyValues( "entity" );

			pViewerKV->SetInt( "id", nCount );
			nCount++;

			pViewerKV->SetString( "classname", "env_viewer" );
			pViewerKV->SetFloat( "radius", pViewer->GetSize() );
			sprintf( temp, "%g %g %g", pViewer->GetLocation().x, pViewer->GetLocation().y, pViewer->GetLocation().z );
			pViewerKV->SetString( "origin", temp );
			pViewerKV->SetInt( "height_group", ( int )pViewer->GetHeightGroup() );
			pViewerKV->SetInt( "team", ( int )pViewer->GetTeam() );

			kv->AddSubKey( pViewerKV );
		}
	}

	for( int i = 0; i < m_Occluders.Count(); i++ )
	{
		CFoW_RadiusOccluder	*pOccluder = m_Occluders[ i ];

		if ( pOccluder )
		{
			KeyValues *pOccluderKV = new KeyValues( "entity" );

			pOccluderKV->SetInt( "id", nCount );
			nCount++;

			pOccluderKV->SetString( "classname", "env_occluder" );
			pOccluderKV->SetFloat( "radius", pOccluder->GetSize() );
			sprintf( temp, "%g %g %g", pOccluder->GetLocation().x, pOccluder->GetLocation().y, pOccluder->GetLocation().z );
			pOccluderKV->SetString( "origin", temp );
			pOccluderKV->SetInt( "height_group", ( int )pOccluder->GetHeightGroup() );

			kv->AddSubKey( pOccluderKV );
		}
	}

	for( int i = 0; i < m_TriSoupCollection.Count(); i++ )
	{
		for( int j = 0; j < m_TriSoupCollection[ i ]->GetNumOccluders(); j++ )
		{
			CFoW_LineOccluder	*pOccluder = m_TriSoupCollection[ i ]->GetOccluder( j );

			if ( pOccluder )
			{
				KeyValues *pOccluderKV = new KeyValues( "entity" );

				pOccluderKV->SetInt( "id", nCount );
				nCount++;

				pOccluderKV->SetString( "classname", "env_line_occluder" );
				sprintf( temp, "%g %g", pOccluder->GetStart().x, pOccluder->GetStart().y );
				pOccluderKV->SetString( "start", temp );
				sprintf( temp, "%g %g", pOccluder->GetEnd().x, pOccluder->GetEnd().y );
				pOccluderKV->SetString( "end", temp );
				sprintf( temp, "%g %g %g", pOccluder->GetPlaneNormal().x, pOccluder->GetPlaneNormal().y, pOccluder->GetPlaneDistance() );
				pOccluderKV->SetString( "plane", temp );
				pOccluderKV->SetInt( "slice_num", pOccluder->GetSliceNum() );

				kv->AddSubKey( pOccluderKV );
			}
		}
	}

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	for ( KeyValues *pWriteKV = kv->GetFirstSubKey(); pWriteKV != NULL; pWriteKV = pWriteKV->GetNextKey() )
	{
		pWriteKV->RecursiveSaveToFile( buf, 0 );
	}

	pFileSystem->WriteFile( pszFileName, NULL, buf );
}

#include <tier0/memdbgoff.h>


/*

RJ Optimization Ideas:

1.  void CFoW_RadiusOccluder::ObstructViewerGrid( CFoW *FoW, CFoW_Viewer *Viewer )
	Don't need the extending sides for point in front of plane checking ( see commented out section )

2.  void CFoW_Viewer::DefaultViewingArea( CFoW *FoW )
	Do this only initially, set the flag FOW_VG_DEFAULT_VISIBLE, then use the flag from that point on.

3.  for line blocker, calc start and end angles and sweep between ( rather than doing 360 degree sweep )

4.  only recalc if an item has moved to a new grid location

5.  360 entry tables for cos / acos lookups

6.  multithread main calc, or sub thread it further?  the float grid update can be easily slit up

7.  obvious radius square calcs to avoid sqrt()


DONE

	tree to only check radius of things near by

*/
