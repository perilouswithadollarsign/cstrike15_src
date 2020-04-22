//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), April 2005

#include "cbase.h"
#include "cs_bot.h"
#include "BasePropDoor.h"
#include "doors.h"
#include "props.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-------------------------------------------------------------------------------------------------
/**
 * Face the door and open it.
 * NOTE: This state assumes we are standing in range of the door to be opened, with no obstructions.
 */
void OpenDoorState::OnEnter( CCSBot *me )
{
	m_isDone = false;
	m_timeout.Start( 0.5f );
}


//-------------------------------------------------------------------------------------------------
void OpenDoorState::SetDoor( CBaseEntity *door )
{
	CBaseDoor *funcDoor = dynamic_cast< CBaseDoor * >(door);
	if ( funcDoor )
	{
		m_funcDoor = funcDoor;
		return;
	}

	CBasePropDoor *propDoor = dynamic_cast< CBasePropDoor * >(door);
	if ( propDoor )
	{
		m_propDoor = propDoor;
		return;
	}
}


//-------------------------------------------------------------------------------------------------
void OpenDoorState::OnUpdate( CCSBot *me )
{
	me->ResetStuckMonitor();

	// look at the door
	Vector pos;
	bool isDoorMoving = false;
	CBaseEntity *door = NULL;

	if ( m_funcDoor.Get() )
	{
		door = m_funcDoor;
		isDoorMoving = m_funcDoor->m_toggle_state == TS_GOING_UP || m_funcDoor->m_toggle_state == TS_GOING_DOWN;
	}
	else if ( m_propDoor.Get() )
	{
		door = m_propDoor;
		isDoorMoving = m_propDoor->IsDoorOpening() || m_propDoor->IsDoorClosing();

		CPropDoorRotatingBreakable *pPropDoor = dynamic_cast< CPropDoorRotatingBreakable* >( door );
		if ( pPropDoor && pPropDoor->IsDoorLocked() && pPropDoor->IsBreakable() == false )
		{
			m_isDone = true;
			return;
		}
	}

	// wait for door to swing open before leaving state
	if ( isDoorMoving || !door )
	{
		m_isDone = true;
		return;
	}

	me->SetLookAt( "Open door", door->WorldSpaceCenter(), PRIORITY_UNINTERRUPTABLE );

	// if we are looking at the door, "use" it and exit
	if ( me->IsLookingAtPosition( door->WorldSpaceCenter() ) )
	{
		if ( m_timeout.IsElapsed() )
		{
			// possibly stuck - blow the damn door away!
			me->PrimaryAttack();

			if ( door )
			{
				AssertMsg( door->GetHealth() > 2, "Bot is stuck on a door and is going to destroy it to get free!\n" );

				CTakeDamageInfo damageInfo( me, me, 2.0f, DMG_GENERIC );
				door->TakeDamage( damageInfo );
			}

		}

		// we are looking at it - use it directly to avoid complications
		door->Use( me, me, USE_ON, 0.0f );
	}
}


//-------------------------------------------------------------------------------------------------
void OpenDoorState::OnExit( CCSBot *me )
{
	me->ClearLookAt();
	me->ResetStuckMonitor();
}



