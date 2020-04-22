//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "cbase.h"
#include "cs_bot.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


/**
 * Face the entity and "use" it
 * NOTE: This state assumes we are standing in range of the entity to be used, with no obstructions.
 */
void UseEntityState::OnEnter( CCSBot *me )
{
}

void UseEntityState::OnUpdate( CCSBot *me )
{
	// in the case when an entity we were supposed to +use is deleted we shouldn't try to
	// dereference a NULL pointer and should just go to a higher level objective
	if ( !m_entity.Get() )
	{
		me->Idle();
		return;
	}

	// in the very rare situation where two or more bots "used" a hostage at the same time,
	// one bot will fail and needs to time out of this state
	const float useTimeout = 5.0f;
	if (me->GetStateTimestamp() - gpGlobals->curtime > useTimeout)
	{
		me->Idle();
		return;
	}

	// look at the entity
	Vector pos = m_entity->EyePosition();
	me->SetLookAt( "Use entity", pos, PRIORITY_HIGH );

	// if we are looking at the entity, "use" it and exit
	if (me->IsLookingAtPosition( pos ))
	{
		if (TheCSBots()->GetScenario() == CCSBotManager::SCENARIO_RESCUE_HOSTAGES && 
			me->GetTeamNumber() == TEAM_CT &&
			me->GetTask() == CCSBot::COLLECT_HOSTAGES)
		{
			// we are collecting a hostage, assume we were successful - the update check will correct us if we weren't
			me->IncreaseHostageEscortCount();
		}

		me->UseEnvironment();
		me->Idle();
	}
}

void UseEntityState::OnExit( CCSBot *me )
{
	me->ClearLookAt();
	me->ResetStuckMonitor();
}



