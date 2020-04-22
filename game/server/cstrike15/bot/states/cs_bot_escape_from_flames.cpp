//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "cbase.h"
#include "cs_bot.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------------------------------
/**
 * Escape from flames we're standing in!
 */
void EscapeFromFlamesState::OnEnter( CCSBot *me )
{
	me->StandUp();
	me->Run();
	me->StopWaiting();
	me->DestroyPath();
	me->EquipKnife();

	m_safeArea = NULL;
	m_searchTimer.Invalidate();
}


//--------------------------------------------------------------------------------------------------------------
class CNonDamagingScan : public ISearchSurroundingAreasFunctor
{
public:
	CNonDamagingScan( void )
	{
		m_safeArea = NULL;
		m_safeAreaTravelRange = FLT_MAX;
	}

	virtual ~CNonDamagingScan() { }

	virtual bool operator() ( CNavArea *area, CNavArea *priorArea, float travelDistanceSoFar )
	{
		const float maxSearchRange = 2000.0f;
		if ( travelDistanceSoFar < maxSearchRange )
		{
			if ( !area->IsDamaging() && travelDistanceSoFar < m_safeAreaTravelRange )
			{
				m_safeArea = area;
			}
		}

		return true;
	}

	CNavArea *m_safeArea;
	float m_safeAreaTravelRange;
};


//--------------------------------------------------------------------------------------------------------------
CNavArea *EscapeFromFlamesState::FindNearestNonDamagingArea( CCSBot *me ) const
{
	CNavArea *myArea = me->GetLastKnownArea();

	if ( !myArea )
	{
		return NULL;
	}

	CNonDamagingScan scan;
	SearchSurroundingAreas( myArea, scan );

	return scan.m_safeArea;
}


//--------------------------------------------------------------------------------------------------------------
void EscapeFromFlamesState::OnUpdate( CCSBot *me )
{
	// default behavior if we're safe
	if ( me->GetTimeSinceBurnedByFlames() > 1.5f )
	{
		me->Idle();
		return;
	}

	if ( m_searchTimer.IsElapsed() )
	{
		// because flames grow and change, periodically re-search
		m_searchTimer.Start( RandomFloat( 0.5f, 1.0f ) );

		m_safeArea = FindNearestNonDamagingArea( me );
	}

	// look around
	me->UpdateLookAround();

	me->EquipBestWeapon();
	me->FireWeaponAtEnemy();

	if ( me->UpdatePathMovement() != CCSBot::PROGRESSING )
	{
		if ( m_safeArea )
		{
			me->ComputePath( m_safeArea->GetCenter(), FASTEST_ROUTE );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void EscapeFromFlamesState::OnExit( CCSBot *me )
{
	me->EquipBestWeapon();
}
