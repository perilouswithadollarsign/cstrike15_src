//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "cbase.h"
#include "cs_bot.h"
#include "weapon_c4.h"
#include "cs_simple_hostage.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------------------------------
/**
 * Plant the bomb.
 */
void PickupHostageState::OnEnter( CCSBot *me )
{
	me->Crouch();
	me->SetDisposition( CCSBot::SELF_DEFENSE );

}

//--------------------------------------------------------------------------------------------------------------
/**
 * Plant the bomb.
 */
void PickupHostageState::OnUpdate( CCSBot *me )
{
	const float timeout = 7.0f;
	if (gpGlobals->curtime - me->GetStateTimestamp() > timeout)
	{
		// find a new spot
		me->Idle();
		return;
	}

	// look at the entity
	Vector pos = m_entity->EyePosition();
	me->SetLookAt( "Use entity", pos, PRIORITY_HIGH );

	// if we are looking at the entity, "use" it and exit
	if (me->IsLookingAtPosition( pos ))
	{
		me->UseEnvironment();
	}

	CHostage *hostage = assert_cast<CHostage *>( m_entity.Get() );

	if ( hostage && hostage->IsFollowingSomeone() )
	{
		me->Idle();
		return;
	}
}

//--------------------------------------------------------------------------------------------------------------
void PickupHostageState::OnExit( CCSBot *me )
{
	// equip our rifle (in case we were interrupted while holding C4)
	me->EquipBestWeapon();
	me->StandUp();
	me->ResetStuckMonitor();
	me->SetDisposition( CCSBot::ENGAGE_AND_INVESTIGATE );
	me->ClearLookAt();

	CHostage *hostage = assert_cast<CHostage *>( m_entity.Get() );

	if ( hostage && hostage->IsFollowing( me ) )
		me->IncreaseHostageEscortCount();
}
