//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: a localized viewer for the Fog of War
//
// $NoKeywords: $
//=====================================================================================//

#ifndef FOW_VIEWER_H
#define FOW_VIEWER_H
#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "mathlib/vector.h"

class CFoW;

class CFoW_Viewer
{
public:
	// constructor to init this viewer with the id and temp
	CFoW_Viewer( int nID, unsigned nViewerTeam );
	// destructor to free up the local vis grids
	~CFoW_Viewer( void );

			// update the radius of the viewer.  this will realloc the local vis grids
	void	UpdateSize( CFoW *pFoW, float flRadius );
			// update the location of the viewer
	bool	UpdateLocation( CFoW *pFoW, const Vector &vLocation, Vector *pvOldLocation );
			// update the height group of this viewer
	void	UpdateHeightGroup( uint8 nHeightGroup );

					//
	inline void		Dirty( ) { m_bDirty = true; }
					//
	inline bool		IsDirty( ) { return m_bDirty; }
					// get the team the viewer is on
	inline unsigned	GetTeam( void ) { return m_nViewerTeam; }
					// get the radius of the viewer
	inline float	GetSize( void ) { return m_flRadius; }
					// get the grid-centered location of the viewer
	inline Vector	&GetLocation( void ) { return m_vLocation; }
					// get the real location of the viewer
	inline Vector	&GetRealLocation( void ) { return m_vRealLocation; }
					// get the height group of the viewer
	inline uint8	GetHeightGroup( void ) { return m_nHeightGroup; }
					// get the grid units of the local vis.  ( diameter / horizontal grid size ) rounded up
	inline int		GetGridUnits( void ) { return m_nGridUnits; }
					// get the local visibility grid
	inline byte		*GetVisibility( void ) { return m_pVisibility; }
					// get the visibility radius grid
	inline int		*GetVisibilityRadius( void ) { return m_pVisibilityRadius; }
					// get the grid units of the radius grid ( circumference / horizontal grid size ) rounded up
	inline int		GetRadiusUnits( void ) { return m_nRadiusUnits; }
					// get the upper left coords of this viewer
	void			GetStartPosition( Vector2D &vResults );
					// get the lower right coords of this viewer
	void			GetEndPosition( Vector2D &vResults );

			// calculate the localized visibility against all occluders.
	void	CalcLocalizedVisibility( CFoW *pFoW );
			// clear the localized radius grid to the maximum distance
	void	DefaultViewingRadius( CFoW *pFoW );

				//
	void		DrawDebugInfo( Vector &vLocation, float flViewRadius, unsigned nFlags );
				//
	size_t		GetAllocatedMemory( ) { return m_nAllocatedMemory; }

private:
			// clear the localized visibility grid to just the raw radius
	void	DefaultViewingArea( CFoW *pFoW );
			// turn the radius grid into the localized visibility grid
	void	ResolveRadius( CFoW *pFoW );

	int			m_nID;					// the id of this viewer
	unsigned	m_nViewerTeam;			// the team this viewer belongs to
	float		m_flRadius;				// the radius of this viewer
	Vector		m_vLocation;			// the grid location of this viewer
	Vector		m_vRealLocation;		// the real location of this viewer
	byte		*m_pVisibility;			// the localized visibility grid
	int			m_nGridUnits;			// the grid units ( diameter / horizontal grid size ) rounded up
	int			*m_pVisibilityRadius;	// the localized visibility depth circle
	int			m_nRadiusUnits;			// the grid units of the radius grid ( circumference / horizontal grid size ) rounded up
	int			*m_pVisibilityTable;	// the visibility table to go from grid to radius
	bool		m_bDirty;				// 
	uint8		m_nHeightGroup;			// the height group this viewer belongs to
	size_t		m_nAllocatedMemory;		//
};

#endif // FOW_VIEWER_H
