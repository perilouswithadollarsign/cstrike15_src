//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Bomb target area
//
//=============================================================================//

#include "cbase.h"
#include "cs_player.h"
#include "weapon_csbase.h"
#include "func_bomb_target.h"
#include "cs_player_resource.h"
#include "cs_gamerules.h"

LINK_ENTITY_TO_CLASS( func_bomb_target, CBombTarget );

BEGIN_DATADESC( CBombTarget )
	DEFINE_FUNCTION( BombTargetTouch ),
	DEFINE_FUNCTION( BombTargetUse ),			//needed?

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "BombExplode", OnBombExplode ),
	DEFINE_INPUTFUNC( FIELD_VOID, "BombPlanted", OnBombPlanted ),
	DEFINE_INPUTFUNC( FIELD_VOID, "BombDefused", OnBombDefused ),

	// Outputs
	DEFINE_OUTPUT( m_OnBombExplode,	"BombExplode" ),
	DEFINE_OUTPUT( m_OnBombPlanted,	"BombPlanted" ),
	DEFINE_OUTPUT( m_OnBombDefused,	"BombDefused" ),
	DEFINE_KEYFIELD( m_bIsHeistBombTarget, FIELD_BOOLEAN, "heistbomb" ),
	DEFINE_KEYFIELD( m_szMountTarget, FIELD_STRING, "bomb_mount_target" ),

END_DATADESC()

CBombTarget::CBombTarget( void )
{
	m_bIsHeistBombTarget = false;
	m_bBombPlantedHere = false;
	m_szMountTarget = NULL_STRING;
	m_hInstructorHint = NULL;
}

void CBombTarget::Spawn()
{
	InitTrigger();
	SetTouch( &CBombTarget::BombTargetTouch );
	SetUse( &CBombTarget::BombTargetUse );

	//VisibilityMonitor_AddEntity( this, 1200.0f, NULL, NULL );
}

void CBombTarget::ReInitOnRoundStart( void )
{
	if ( m_hInstructorHint.Get() )
	{
		CPointEntity *pEnt = static_cast< CPointEntity* >( m_hInstructorHint.Get() );
		UTIL_Remove( pEnt );
		m_hInstructorHint = NULL;
	}

	m_bBombPlantedHere = false;

	//CCSPlayerResource *pCSPR = CSPlayerResource();
	Vector bombA = Vector( 0, 0, 0 );
	Vector bombB = Vector( 0, 0, 0 );
	Vector vecTrigger = CollisionProp()->WorldSpaceCenter();
	if ( CSPlayerResource() )
	{
		bombA = CSPlayerResource()->GetBombsiteAPosition();
		bombB = CSPlayerResource()->GetBombsiteBPosition();

		CPointEntity *pEnt;
		if ( vecTrigger.DistTo( bombA ) > vecTrigger.DistTo( bombB ) )
		{
			pEnt = static_cast< CPointEntity* >( CreateEntityByName( "info_bomb_target_hint_B" ) );
		}
		else
		{
			pEnt = static_cast< CPointEntity* >( CreateEntityByName( "info_bomb_target_hint_A" ) );
		}

		if ( pEnt )
		{
			pEnt->Spawn();
			trace_t tr;

			UTIL_TraceLine( vecTrigger, vecTrigger + Vector ( 0, 0, -128 ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
			Vector vecHint = tr.endpos +  Vector ( 0, 0, 16 );

			pEnt->SetAbsOrigin( vecHint );
			pEnt->SetOwnerEntity( this );
			pEnt->SetParent( this );

			m_hInstructorHint = pEnt;
		}
	}
}

void CBombTarget::BombTargetTouch( CBaseEntity* pOther )
{
	CCSPlayer *p = dynamic_cast< CCSPlayer* >( pOther );
	if ( p )
	{
		p->m_bInBombZoneTrigger = true;

		if ( p->HasC4() && m_bBombPlantedHere == false && (CSGameRules()->m_bBombPlanted == false || CSGameRules()->IsPlayingCoopMission()) )
		{
			p->m_bInBombZone = true;
			p->m_iBombSiteIndex = entindex();

			//bool bC4Active = p->GetActiveCSWeapon() && p->GetActiveCSWeapon()->GetCSWeaponID() == WEAPON_C4;
			if ( !(p->m_iDisplayHistoryBits & DHF_IN_TARGET_ZONE) )
			{
				p->m_iDisplayHistoryBits |= DHF_IN_TARGET_ZONE;
			}
		}
	}
}

void CBombTarget::BombTargetUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	//SUB_UseTargets( NULL, USE_TOGGLE, 0 );
	DevMsg( 2, "BombTargetUse does nothing\n" );
}

// Relay to our outputs
void CBombTarget::OnBombExplode( inputdata_t &inputdata )
{
	m_OnBombExplode.FireOutput(this, this);
}

// Relay to our outputs
void CBombTarget::OnBombPlanted( inputdata_t &inputdata )
{
	m_OnBombPlanted.FireOutput(this, this);

	m_bBombPlantedHere = true;
}
// Relay to our outputs
void CBombTarget::OnBombDefused( inputdata_t &inputdata )
{
	m_OnBombDefused.FireOutput(this, this);
}


//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for displaying a hint for the CS bomb targets
//-----------------------------------------------------------------------------
void CInfoInstructorHintBombTargetA::Spawn( void )
{
	VisibilityMonitor_AddEntity( this, 5000.0f, NULL, NULL );
}

LINK_ENTITY_TO_CLASS( info_bomb_target_hint_A, CInfoInstructorHintBombTargetA );

BEGIN_DATADESC( CInfoInstructorHintBombTargetA )

END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for displaying a hint for the CS bomb targets
//-----------------------------------------------------------------------------
void CInfoInstructorHintBombTargetB::Spawn( void )
{
	VisibilityMonitor_AddEntity( this, 5000.0f, NULL, NULL );
}

LINK_ENTITY_TO_CLASS( info_bomb_target_hint_B, CInfoInstructorHintBombTargetB );

BEGIN_DATADESC( CInfoInstructorHintBombTargetB )

END_DATADESC()