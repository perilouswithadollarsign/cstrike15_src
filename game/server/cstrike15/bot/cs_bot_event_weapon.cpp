//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "cbase.h"
#include "cs_gamerules.h"
#include "keyvalues.h"

#include "cs_bot.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnWeaponFire( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	// for knife fighting - if our victim is attacking or reloading, rush him
	/// @todo Propagate events into active state
	if (GetBotEnemy() == player && IsUsingKnife())
	{
		ForceRun( 5.0f );
	}

	const float ShortRange = 1000.0f;
	const float NormalRange = 2000.0f;

	float range;

	/// @todo Check weapon type (knives are pretty quiet)
	/// @todo Use actual volume, account for silencers, etc.

	// [mlowrance] use the weapon as posted in the event message
	int iWeaponID = -1;
	const char *weaponName = event->GetString( "weapon" );
	if ( weaponName )
	{
		iWeaponID = AliasToWeaponID( weaponName );
	}
	
	if ( iWeaponID == -1 )
		return;

	switch( iWeaponID )
	{
		// silent "firing"
		case WEAPON_HEGRENADE:
		case WEAPON_SMOKEGRENADE:
		case WEAPON_FLASHBANG:
		case WEAPON_INCGRENADE:
		case WEAPON_MOLOTOV:
		case WEAPON_DECOY:
		case WEAPON_TAGRENADE:
		case WEAPON_C4:
			return;

		// quiet
		case WEAPON_KNIFE:
		case WEAPON_KNIFE_GG:
			range = ShortRange;
			break;

		// loud
		case WEAPON_AWP:
			range = 99999.0f;
			break;

		// normal
		default:
			if ( event->GetBool( "silenced" ) )
			{
				range = ShortRange;
			}
			else
			{
				range = NormalRange;
			}
			break;
	}

	OnAudibleEvent( event, player, range, PRIORITY_HIGH, true ); // weapon_fire
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnWeaponFireOnEmpty( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	// for knife fighting - if our victim is attacking or reloading, rush him
	/// @todo Propagate events into active state
	if (GetBotEnemy() == player && IsUsingKnife())
	{
		ForceRun( 5.0f );
	}

	OnAudibleEvent( event, player, 1100.0f, PRIORITY_LOW, false ); // weapon_fire_on_empty
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnWeaponReload( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	// for knife fighting - if our victim is attacking or reloading, rush him
	/// @todo Propagate events into active state
	if (GetBotEnemy() == player && IsUsingKnife())
	{
		ForceRun( 5.0f );
	}

	OnAudibleEvent( event, player, 1100.0f, PRIORITY_LOW, false ); // weapon_reload
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnWeaponZoom( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 1100.0f, PRIORITY_LOW, false ); // weapon_zoom
}



