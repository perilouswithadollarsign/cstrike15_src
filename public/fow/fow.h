//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: this is the only public access point to the Fog of War routines
//
// $NoKeywords: $
//=====================================================================================//

#ifndef FOW_H
#define FOW_H
#if defined( COMPILER_MSVC )
#pragma once
#endif


#include "utlvector.h"
#include "utlmap.h"
#include "utlspheretree.h"


class CFoW_RadiusOccluder;
class CFoW_Viewer;
class CFoW_TriSoupCollection;
class CFoW_LineOccluder;
class CFoW_HorizontalSlice;
class IFileSystem;
class CPhysCollide;
class IPhysicsCollision;


// maximum number of unique visible sides / teams
#define MAX_FOW_TEAMS	2


// fow does maximum amount of safety checks for data coming into it
#define FOW_SAFETY_DANCE	1


// bit pattern for Visibility Grid
#define FOW_VG_MAX_HEIGHT_GROUP		0x07	// only support height groups from 0 to 7
#define FOW_VG_UNUSED				0x08	// unused
#define FOW_VG_DEFAULT_VISIBLE		0x10	// future optimization to mark default viewing radius
#define FOW_VG_IS_VISIBLE			0x20	// cell is currently visible
#define FOW_VG_WAS_VISIBLE			0x40	// cell has been visible at one point in the past
#define FOW_VG_INVALID				0x80	// invalid cell location


// debug flags
#define FOW_DEBUG_SHOW_VIEWERS_TEAM_0		0x00000001
#define FOW_DEBUG_SHOW_OCCLUDERS			0x00000002
#define FOW_DEBUG_SHOW_GRID					0x00000004
#define FOW_DEBUG_SHOW_VIEWERS_TEAM_1		0x00000008

#define FOW_DEBUG_VIEW_TIME			( 1.0f / 15.0f )
//#define FOW_DEBUG_VIEW_TIME			( 0 )

// fow limits
#define FOW_MAX_VIEWERS_TO_CHECK			100
#define FOW_MAX_RADIUS_OCCLUDERS_TO_CHECK	400
#define FOW_MAX_LINE_OCCLUDERS_TO_CHECK		400
#define FOW_OVER_VISIBILITY					1.5f	// keeps around a fully visibile area a bit longer
#define FOW_FADE_DELAY						0.25f;	// helps control flickering on edge cases when viewers are moving

class CFoW
{
public:
			CFoW( );
			~CFoW( );

	// setup
			// Frees all memory
	void	ClearState( );
			// Sets the number of viewer teams
	void	SetNumberOfTeams( int nCount );
			// Sets the world mins/maxs and how big the grid sizes should be
	void	SetSize( Vector &vWorldMins, Vector &vWorldMaxs, int nHorizontalGridSize, int nVerticalGridSize = -1 );
			//
	void	SetCustomVerticalLevels( float *pflHeightLevels, int nCount );
			// Sets the visibility degree fade rate ( in seconds )
	void	SetDegreeFadeRate( float flDegreeFadeRate ) { m_flDegreeFadeRate = flDegreeFadeRate; }

	// fow system info
				// are we initialized?
	bool		IsInitialized( ) { return m_bInitialized; }
				// get the world size of the FoW
	void		GetSize( Vector &vWorldMins, Vector &vWorldMaxs );
				// get the horizontal grid size
	inline int	GetHorizontalGridSize( ) { return m_nHorizontalGridSize; }
				//
	int			GetXGridUnits( ) { return m_nGridXUnits; }
				//
	int			GetYGridUnits( ) { return m_nGridYUnits; }
				// get the lower vertical coord, the grid size, and grid units
	void		GetVerticalGridInfo( int &nBottomZ, int &nGridSize, int &nGridUnits, float **pVerticalLevels );
				// snap the x/y coordinates to the grid
	void		SnapCoordsToGrid( Vector &vIn, Vector &vOut, bool bGoLower );
				// return how visible a cell is ( 0.0 = not currently visible, 1.0 = fully visible )
	float		LookupVisibilityDegree( int nXLoc, int nYLoc, int nTeam );
				// creates or returns a grid to radius table
	int			*FindRadiusTable( float flRadius );
				//
	void		CenterCoordToGrid( Vector &vCoords );

	// debug info
				// 
	void		SetDebugVisibility( bool bVisible );
				// 
	void		EnableDebugFlags( unsigned nFlags );
				// 
	void		DisableDebugFlags( unsigned nFlags );
				//
	void		DrawDebugInfo( Vector &vLocation, float flViewRadius );
				//
	void		PrintStats( );

	// viewers
				// adds a new viewer to the system
	int			AddViewer( unsigned nViewerTeam );
				// removes a viewer from the system
	void		RemoveViewer( int nID );
				// updates the viewer's location
	void		UpdateViewerLocation( int nID, const Vector &vLocation );
				// updates the viewer's seeing radius
	void		UpdateViewerSize( int nID, float flRadius );
				// updates the viewer's seeing radius
	void		UpdateViewerHeightGroup( int nID, uint8 nHeightGroup );

	// radius occluders
								// adds a new radius occluder to the system
	int							AddOccluder( bool nPermanent );
								// removes an occluder from the system
	void						RemoveOccluder( int nID );
	void						EnableOccluder( int nID, bool bEnable );
								// returns the total number of radius occluders
	inline int					GetNumOccluders() { return m_Occluders.Count(); }
								// get access to the radius occluder object
	inline CFoW_RadiusOccluder	*GetOccluder( int nIndex ) { return m_Occluders[ nIndex ]; }
								// update an occluder's location
	void						UpdateOccluderLocation( int nID, Vector &vLocation );
								// update an occluder's size
	void						UpdateOccluderSize( int nID, float flRadius );
								// updates the occluder's height group
	void						UpdateOccluderHeightGroup( int nID, uint8 nHeightGroup );
								// internal function called by viewers to radius occlude nearby objects
	void						ObstructOccludersNearViewer( int nViewerID );

	// world occlusion
	void	SetWorldCollision( CPhysCollide *pCollideable, IPhysicsCollision *pPhysCollision );

	// tri soup ( line ) occluders
							// adds a tri soup collection to the system
	int						AddTriSoup( );
							// removes a tri soup collection from the system
	void					RemoveTriSoup( int nID );
							// clears all entries from the collection ( useful for hammer editing only )
	void					ClearTriSoup( int nID );
							// adds a tri to the collection.  this is immediately split up into the horizontal slices.  very slow!
	void					AddTri( int nID, Vector &vPointA, Vector &vPointB, Vector &vPointC );
							//
	int						GetNumTriSoups( ) { return m_TriSoupCollection.Count(); }
							// get access to a tri soup collection object
	CFoW_TriSoupCollection	*GetTriSoup( int nID );
							// add a line occulder from a horizontal slice
	void					AddTriSoupOccluder( CFoW_LineOccluder *pOccluder, int nSliceNum );

	// horizontal slices ( from tri soups )
								// get the slice index given the vertical position
	int							GetHorizontalSlice( float flZPos );
								//
	float						GetSliceZPosition( int nIndex ) { return m_pVerticalLevels[ nIndex ]; } 
								// get access to the slice object
	inline CFoW_HorizontalSlice	*GetSlice( int nIndex ) { return m_pHorizontalSlices[ nIndex ]; }

	// visibility
			// solve the visibility for all teams and all viewers - slow!
	void	SolveVisibility( float flFrameTime );
			// Purpose: returns the visibility info of a location to a team
	uint8	GetLocationInfo( unsigned nViewerTeam, const Vector &vLocation );
			// Purpose: returns the visibility degree of a location to a team
	float	GetLocationVisibilityDegree( unsigned nViewerTeam, const Vector &vLocation, float flRadius = 0.0f );
			// given the coords and an offset to move BACK, finds the grid location
	void	GetGridUnits( const Vector &vCoords, float flXOffset, float flYOffset, bool bGoLower, int &nGridX, int &nGridY );

	// debug
			// Generates a vmf file based upon the current viewers and occluders
	void	GenerateVMF( IFileSystem *pFileSystem, const char *pszFileName );

private:
			// adds an occluder to the sphere tree
	void	InsertViewerIntoTree( int nIndex );
			// removes an occluder from the sphere tree
	void	RemoveViewerFromTree( int nIndex, Vector *pvOldLocation = NULL );
			//
	void	DirtyViewers( Vector &vLocation, float flRadius );
			// adds all occluders back into the visibility tree
	void	RepopulateOccluders( );
			// adds an occluder to the sphere tree
	void	InsertOccluderIntoTree( int nIndex );
			// removes an occluder from the sphere tree
	void	RemoveOccluderFromTree( int nIndex );
			// defaults the viewing grids
	void	PrepVisibility( );
			// updates the viewer grids
	void	UpdateVisibleAmounts( float flFrameTime );
			// given the coords and an offset to move BACK, finds the grid index
	int		GetGridIndex( const Vector &vCoords, float flXOffset, float flYOffset, bool bGoLower );
			// merge a local viewer's visibility to the global grid
	void	MergeViewerVisibility( int nID );

private:
	bool	m_bInitialized;				// fow system is ready
	int		m_nNumberOfTeams;			// number of teams
	Vector	m_vWorldMins;				// world mins
	Vector	m_vWorldMaxs;				// world maxs
	float	m_flDegreeFadeRate;			// fade in rate for visibility degree
	int		m_nHorizontalGridSize;		// the horizontal ( x y ) grid size
	int		m_nVerticalGridSize;		// the vertical ( z ) grid size
	int		m_nGridXUnits;				// number of X grid pieces ( world X size / horizontal grid size ) rounded up
	int		m_nGridYUnits;				// number of Y grid pieces ( world Y size / horizontal grid size ) rounded up
	int		m_nTotalHorizontalUnits;	// X * Y grid pieces
	int		m_nGridZUnits;				// number of Z grid pieces ( world Z size / vertical grid size ) rounded up

	float32					*m_pVisibilityGridDegree[ MAX_FOW_TEAMS ];	// the degree of visibility ( 0.0 = not visible, 1.0 = visible )
	float32					*m_pVisibilityFadeTimer[ MAX_FOW_TEAMS ];	// the delay before visibility starts to fade
	uint8					*m_pVisibilityGridFlags[ MAX_FOW_TEAMS ];	// flags to indicate visibility status
	CFoW_HorizontalSlice	**m_pHorizontalSlices;						// horizontal line occluder slices from tri soups
	float32					*m_pVerticalLevels;

	CUtlVector< CFoW_Viewer	* >				m_Viewers;					// the list of all viewers
	CUtlVector< CFoW_RadiusOccluder * >		m_Occluders;				// the list of all radius occluders
	CUtlSphereTree							m_ViewerTree;				// sphere tree for quick finding of nearby viewers
	CUtlSphereTree							m_OccluderTree;				// sphere tree for quick finding of nearby radius occluders
	CUtlVector< CFoW_TriSoupCollection * >	m_TriSoupCollection;		// the list of all tri soups
	CUtlMap< float, int * >					m_RadiusTables;				// the cached visibility tables to go from grid to radius

	// debug info
	bool					m_bDebugVisible;
	unsigned				m_nDebugFlags;
	size_t					m_nHorizontalGridAllocationSize;
	size_t					m_nVerticalGridAllocationSize;
	size_t					m_nRadiusTableSize;

	friend void PrepVisibilityThreaded( CFoW *pFoW );
};


#endif // FOW_H
