//===== Copyright © Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "cbase.h"
#include "cs_nav_mesh.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

const char* g_pszPostActivateSetupThink = "PostActivateSetupThink";

class CPointHidingSpot : public CServerOnlyPointEntity
{
	DECLARE_CLASS( CPointHidingSpot, CServerOnlyPointEntity )
public:
	CPointHidingSpot();
	DECLARE_DATADESC();

	virtual void Activate() OVERRIDE;
	virtual void UpdateOnRemove() OVERRIDE;
	void PostActivateSetupThink();
	void DetachFromHidingSpot();
protected:
	CNavArea *m_pNavArea;		// nav our hiding spot is associated with
	HidingSpot *m_pSpot;
};

BEGIN_DATADESC( CPointHidingSpot )
END_DATADESC()

LINK_ENTITY_TO_CLASS( point_hiding_spot, CPointHidingSpot );



CPointHidingSpot::CPointHidingSpot() : 
m_pNavArea( NULL ),
m_pSpot( NULL )
{
}

void CPointHidingSpot::Activate()
{
	BaseClass::Activate();
		
	// Do setup after all activates have run and we're in game simulation. Nav gets loaded/setup during server
	// activate so we can't guarantee this ent will be before or after nav load.
	SetContextThink( &CPointHidingSpot::PostActivateSetupThink, gpGlobals->curtime + 0.1f, g_pszPostActivateSetupThink );
}

void CPointHidingSpot::DetachFromHidingSpot()
{
	m_pSpot = NULL;
}

void CPointHidingSpot::UpdateOnRemove()
{
	if ( m_pNavArea && m_pSpot )
	{
		m_pNavArea->RemoveHidingSpot( m_pSpot );
		bool bSuccess = TheHidingSpots.FindAndRemove( m_pSpot );
		Assert( bSuccess );
		NOTE_UNUSED( bSuccess );
		delete m_pSpot;
	}
	BaseClass::UpdateOnRemove();
}

void CPointHidingSpot::PostActivateSetupThink()
{
	if ( !m_pNavArea )
	{
		m_pNavArea = TheNavMesh->GetNearestNavArea( this, 0, MAX_COORD_FLOAT );
		if ( !m_pNavArea )
		{
			Warning( "Warning: point_hiding_spot (%s) couldn't find a nearby nav. Removing.", GetDebugName() );
			UTIL_Remove( this );
			return;
		}
		Vector vecPointOnGround;
		m_pNavArea->GetClosestPointOnArea( GetAbsOrigin(), &vecPointOnGround );
		m_pSpot = TheNavMesh->CreateHidingSpot();
		m_pSpot->SetSaved( false );
		m_pSpot->SetPosition( vecPointOnGround );
		m_pSpot->SetFlags( HidingSpot::IN_COVER | HidingSpot::GOOD_SNIPER_SPOT | HidingSpot::IDEAL_SNIPER_SPOT );
		m_pSpot->SetArea( m_pNavArea );
		m_pNavArea->AddHidingSpot( m_pSpot );

		SetContextThink( NULL, TICK_NEVER_THINK, g_pszPostActivateSetupThink );
	}
}
