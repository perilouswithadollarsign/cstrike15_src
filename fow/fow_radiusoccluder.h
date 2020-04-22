//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: radius occlusion for Fog of War
//
// $NoKeywords: $
//=====================================================================================//

#ifndef FOW_RADIUSOCCLUDER_H
#define FOW_RADIUSOCCLUDER_H
#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "mathlib/vector.h"

class CFoW;
class CFoW_Viewer;

class CFoW_RadiusOccluder
{
public:
	// constructor to init this occluder with the id
	CFoW_RadiusOccluder( int nID );

			// update the radius of this occluder
	void	UpdateSize( float flRadius );
			// update the location of this occluder
	void	UpdateLocation( Vector &vLocation );
			// update the height group of this occluder
	void	UpdateHeightGroup( uint8 nHeightGroup );

					// get the radius of this occluder
	inline float	GetSize( void ) { return m_flRadius; }
					// get the location of this occluder
	inline Vector	&GetLocation( void ) { return m_vLocation; }
					// 
	inline uint8	GetHeightGroup( void ) { return m_nHeightGroup; }

					//
	inline bool		IsEnabled( ) { return m_bEnabled; }
					//
	inline void		SetEnable( bool bEnable ) { m_bEnabled = bEnable; }

			// is the occluder within range of the viewer?
	bool	IsInRange( CFoW_Viewer *pViewer );

			// obstruct the viewer by updating the local viewer grid
	void	ObstructViewerGrid( CFoW *pFoW, CFoW_Viewer *pViewer );
			// obstruct the viewer by updating the depth circle
	void	ObstructViewerRadius( CFoW *pFoW, CFoW_Viewer *pViewer );

				//
	void		DrawDebugInfo( Vector &vLocation, float flViewRadius, unsigned nFlags );

private:
	int			m_nID;					// the id of this occluder
	float		m_flRadius;				// the radius of this occluder
	Vector		m_vLocation;			// the location of this occluder
	bool		m_bEnabled;
	uint8		m_nHeightGroup;			// the height group this occluder belongs to
};

#endif // FOW_RADIUSOCCLUDER_H
