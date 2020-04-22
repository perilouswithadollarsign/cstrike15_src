//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), Leon Hartwig, 2003

#include "cbase.h"
#include "cs_shareddefs.h"

#include "basegrenade_shared.h"

#include "bot.h"
#include "bot_util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

/// @todo Remove this nasty hack - CreateFakeClient() calls CBot::Spawn, which needs the profile and team
const BotProfile *g_botInitProfile = NULL;
int g_botInitTeam = 0;

//
// NOTE: Because CBot had to be templatized, the code was moved into bot.h
//


//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------

ActiveGrenade::ActiveGrenade( CBaseGrenade *grenadeEntity )
{
	m_entity = grenadeEntity;
	m_detonationPosition = grenadeEntity->GetAbsOrigin();
	m_dieTimestamp = 0.0f;

	m_isSmoke = false;
	m_isFlashbang = false;
	m_isMolotov = false;
	m_isDecoy = false;

	switch ( grenadeEntity->GetGrenadeType() )
	{
	case GRENADE_TYPE_EXPLOSIVE:	m_radius = HEGrenadeRadius;									break;
	case GRENADE_TYPE_FLASH:		m_radius = FlashbangGrenadeRadius; m_isFlashbang = true;	break;
	case GRENADE_TYPE_FIRE:			m_radius = MolotovGrenadeRadius; m_isMolotov = true;		break;
	case GRENADE_TYPE_DECOY:		m_radius = DecoyGrenadeRadius; m_isDecoy = true;			break;
	case GRENADE_TYPE_SMOKE:		m_radius = SmokeGrenadeRadius; m_isSmoke = true;			break;
	case GRENADE_TYPE_SENSOR:		m_radius = MolotovGrenadeRadius; m_isSensor = true;			break;
	default:
		AssertMsg( 0, "Invalid grenade type!\n" );
		m_radius = HEGrenadeRadius;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Called when the grenade in the world goes away
 */
void ActiveGrenade::OnEntityGone( void )
{
	if (m_isSmoke)
	{
		// smoke lingers after grenade is gone
		const float smokeLingerTime = 4.0f;
		m_dieTimestamp = gpGlobals->curtime + smokeLingerTime;
	}

	m_entity = NULL;
}

//--------------------------------------------------------------------------------------------------------------
void ActiveGrenade::Update( void )
{
	if (m_entity != NULL)
	{
		m_detonationPosition = m_entity->GetAbsOrigin();
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if this grenade is valid
 */
bool ActiveGrenade::IsValid( void ) const
{
	if ( m_isSmoke )
	{
		if ( m_entity == NULL && gpGlobals->curtime > m_dieTimestamp )
		{
			return false;
		}
	}
	else
	{
		if ( m_entity == NULL )
		{
			return false;
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
const Vector &ActiveGrenade::GetPosition( void ) const
{
	// smoke grenades can vanish before the smoke itself does - refer to the detonation position
	if (m_entity == NULL)
		return GetDetonationPosition();

	return m_entity->GetAbsOrigin();
}

