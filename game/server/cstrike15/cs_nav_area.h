//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// nav_area.h
// Navigation areas
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

#ifndef _CS_NAV_AREA_H_
#define _CS_NAV_AREA_H_

#include "nav_area.h"

class CCSHidingSpot : public HidingSpot
{
public:
	virtual ~CCSHidingSpot();
	void SetOwningEntity( class CPointHidingSpot *pHidingSpotEnt );
protected:
	CPointHidingSpot *m_pOwningEntity;
};


//-------------------------------------------------------------------------------------------------------------------
/**
 * A CNavArea is a rectangular region defining a walkable area in the environment
 */
class CCSNavArea : public CNavArea
{
public:
	DECLARE_CLASS( CCSNavArea, CNavArea );

	CCSNavArea( void );
	~CCSNavArea();

	virtual void OnServerActivate( void );						// (EXTEND) invoked when map is initially loaded
	virtual void OnRoundRestart( void );						// (EXTEND) invoked for each area when the round restarts

	virtual void Draw( void ) const;							// draw area for debugging & editing

	virtual void Save( CUtlBuffer &fileBuffer, unsigned int version ) const;	// (EXTEND)
	virtual NavErrorType Load( CUtlBuffer &fileBuffer, unsigned int version, unsigned int subVersion );		// (EXTEND)
	virtual NavErrorType PostLoad( void );								// (EXTEND) invoked after all areas have been loaded - for pointer binding, etc

	virtual void CustomAnalysis( bool isIncremental = false );		// for game-specific analysis

	virtual float GetDangerDecayRate( void ) const;				// return danger decay rate per second
	virtual float GetEarliestOccupyTime( int teamID ) const OVERRIDE;			// returns the minimum time for someone of the given team to reach this spot from their spawn

	// Use nav blockers in coop mode. There is a bug with these functions causing bots to lose
	// their path at the start of rounds that is undiagnosed at the time of this comment. Coop needs nav blockers
	// and doesn't (seem) to have any issues with blocked nav so let's leave it on for them
	virtual void UpdateBlocked( bool force = false, int teamID = TEAM_ANY ) OVERRIDE;
	// Updates the (un)blocked status of the nav area (throttled)
	virtual bool IsBlocked( int teamID, bool ignoreNavBlockers = false ) const OVERRIDE;	


	//- approach areas ----------------------------------------------------------------------------------
	struct ApproachInfo
	{
		NavConnect here;										///< the approach area
		NavConnect prev;										///< the area just before the approach area on the path
        NavConnect next;										///< the area just after the approach area on the path
		uint16 prevToHereHow;                                   // NavTraverseType
		uint16 hereToNextHow;
	};
	const ApproachInfo *GetApproachInfo( int i ) const	{ return &m_approach[i]; }
	int GetApproachInfoCount( void ) const				{ return m_approachCount; }
	void ComputeApproachAreas( void );							///< determine the set of "approach areas" - for map learning

	//- player counting --------------------------------------------------------------------------------
	void ClearPlayerCount( void );								///< set the player count to zero

protected:
	NavErrorType LoadLegacy( CUtlBuffer &fileBuffer, unsigned int version, unsigned int subVersion );


private:
	//- approach areas ----------------------------------------------------------------------------------
	enum { MAX_APPROACH_AREAS = 16 };
	ApproachInfo m_approach[ MAX_APPROACH_AREAS ];
	unsigned char m_approachCount;

	unsigned char m_paddingToAlignTo128[ int( 0 - sizeof( CNavArea ) - ( sizeof( ApproachInfo ) * MAX_APPROACH_AREAS ) - sizeof( unsigned char ) ) & 127 ];
};

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
//
// Inlines
//

inline void CCSNavArea::ClearPlayerCount( void )
{
#ifndef PLATFORM_64BITS
	COMPILE_TIME_ASSERT( sizeof( CCSNavArea ) == 768 ); // legacy 32-bit struct was 768 bytes, preserving for compatibility
#endif
	for( int i=0; i<MAX_NAV_TEAMS; ++i )
	{
		m_playerCount[ i ] = 0;
	}
}

#endif // _CS_NAV_AREA_H_
