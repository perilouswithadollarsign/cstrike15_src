//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "cbase.h"

#include "bot.h"
#include "bot_manager.h"
#include "nav_area.h"
#include "bot_util.h"
#include "basegrenade_shared.h"

#include "cs_bot.h"

#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


float g_BotUpkeepInterval = 0.0f;
float g_BotUpdateInterval = 0.0f;


//--------------------------------------------------------------------------------------------------------------
CBotManager::CBotManager()
{
	InitBotTrig();
}


//--------------------------------------------------------------------------------------------------------------
CBotManager::~CBotManager()
{
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked when the round is restarting
 */
void CBotManager::RestartRound( void )
{
	DestroyAllGrenades();
	ClearDebugMessages();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked at the start of each frame
 */
void CBotManager::StartFrame( void )
{
	VPROF_BUDGET( "CBotManager::StartFrame", VPROF_BUDGETGROUP_NPCS );

	ValidateActiveGrenades();

	// debug smoke grenade visualization
	if (cv_bot_debug.GetInt() == 5)
	{
		Vector edge, lastEdge;

		FOR_EACH_LL( m_activeGrenadeList, it )
		{
			ActiveGrenade *ag = m_activeGrenadeList[ it ];

			const Vector &pos = ag->GetDetonationPosition();

			UTIL_DrawBeamPoints( pos, pos + Vector( 0, 0, 50 ), 1, 255, 100, 0 );

			lastEdge = Vector( ag->GetRadius() + pos.x, pos.y, pos.z );
			float angle;
			for( angle=0.0f; angle <= 180.0f; angle += 22.5f )
			{
				edge.x = ag->GetRadius() * BotCOS( angle ) + pos.x;
				edge.y = pos.y;
				edge.z = ag->GetRadius() * BotSIN( angle ) + pos.z;

				UTIL_DrawBeamPoints( edge, lastEdge, 1, 255, 50, 0 );

				lastEdge = edge;
			}

			lastEdge = Vector( pos.x, ag->GetRadius() + pos.y, pos.z );
			for( angle=0.0f; angle <= 180.0f; angle += 22.5f )
			{
				edge.x = pos.x;
				edge.y = ag->GetRadius() * BotCOS( angle ) + pos.y;
				edge.z = ag->GetRadius() * BotSIN( angle ) + pos.z;

				UTIL_DrawBeamPoints( edge, lastEdge, 1, 255, 50, 0 );

				lastEdge = edge;
			}
		}
	}

	// set frame duration
	g_BotUpkeepInterval = m_frameTimer.GetElapsedTime();
	m_frameTimer.Start();

	g_BotUpdateInterval = (g_BotUpdateSkipCount+1) * g_BotUpkeepInterval;

	//
	// Process each active bot
	//
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (!player)
			continue;

		// Hack for now so the temp bot code works. The temp bots are very useful for debugging
		// because they can be setup to mimic the player's usercmds.
		if (player->IsBot() && IsEntityValid( player ) )
		{
			// EVIL: Messes up vtables
			//CBot< CBasePlayer > *bot = static_cast< CBot< CBasePlayer > * >( player );
			CCSBot *bot = dynamic_cast< CCSBot * >( player );

			if ( bot )
			{
				{
					SNPROF("Upkeep");
					bot->Upkeep();
				}

				if (((gpGlobals->tickcount + bot->entindex()) % g_BotUpdateSkipCount) == 0)
				{
					{
						SNPROF("ResetCommand");
						bot->ResetCommand();
					}
					
					{
						SNPROF("Bot Update");
						bot->Update();
					}
				}

				{
					SNPROF("UpdatePlayer");
					bot->UpdatePlayer();
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Add an active grenade to the bot's awareness
 */
void CBotManager::AddGrenade( CBaseGrenade *grenade )
{
	ActiveGrenade *ag = new ActiveGrenade( grenade );
	m_activeGrenadeList.AddToTail( ag );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * The grenade entity in the world is going away
 */
void CBotManager::RemoveGrenade( CBaseGrenade *grenade )
{
	FOR_EACH_LL( m_activeGrenadeList, it )
	{
		ActiveGrenade *ag = m_activeGrenadeList[ it ];

		if (ag->IsEntity( grenade ))
		{
			ag->OnEntityGone();
			m_activeGrenadeList.Remove( it );
			delete ag;
			return;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * The grenade entity has changed its radius
 */
void CBotManager::SetGrenadeRadius( CBaseGrenade *grenade, float radius )
{
	FOR_EACH_LL( m_activeGrenadeList, it )
	{
		ActiveGrenade *ag = m_activeGrenadeList[ it ];

		if (ag->IsEntity( grenade ))
		{
			ag->SetRadius( radius );
			return;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Destroy any invalid active grenades
 */
void CBotManager::ValidateActiveGrenades( void )
{
	int it = m_activeGrenadeList.Head();

	while( it != m_activeGrenadeList.InvalidIndex() )
	{
		ActiveGrenade *ag = m_activeGrenadeList[ it ];

		int current = it;
		it = m_activeGrenadeList.Next( it );

		// lazy validation
		if (!ag->IsValid())
		{
			m_activeGrenadeList.Remove( current );
			delete ag;
			continue;
		}
		else
		{
			ag->Update();
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void CBotManager::DestroyAllGrenades( void )
{
	int it = m_activeGrenadeList.Head();

	while ( it != m_activeGrenadeList.InvalidIndex() )
	{
		ActiveGrenade *ag = m_activeGrenadeList[it];
		int current = it;
		it = m_activeGrenadeList.Next( it );
		m_activeGrenadeList.Remove( current );
		delete ag;
	}

	m_activeGrenadeList.PurgeAndDeleteElements();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if position is inside a smoke cloud
 */
bool CBotManager::IsInsideSmokeCloud( const Vector *pos, float radius )
{
	int it = m_activeGrenadeList.Head();

	while( it != m_activeGrenadeList.InvalidIndex() )
	{
		ActiveGrenade *ag = m_activeGrenadeList[ it ];

		int current = it;
		it = m_activeGrenadeList.Next( it );

		// lazy validation
		if (!ag->IsValid())
		{
			m_activeGrenadeList.Remove( current );
			delete ag;
			continue;
		}

		if (ag->IsSmoke())
		{
			const Vector &smokeOrigin = ag->GetDetonationPosition();

			if ((smokeOrigin - *pos).IsLengthLessThan( ag->GetRadius() + radius ))
				return true;			
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if line intersects smoke volume
 * Determine the length of the line of sight covered by each smoke cloud, 
 * and sum them (overlap is additive for obstruction).
 * If the overlap exceeds the threshold, the bot can't see through.
 */
bool CBotManager::IsLineBlockedBySmoke( const Vector &from, const Vector &to, float grenadeBloat )
{
	VPROF_BUDGET( "CBotManager::IsLineBlockedBySmoke", VPROF_BUDGETGROUP_NPCS );

	float totalSmokedLength = 0.0f;	// distance along line of sight covered by smoke

	// compute unit vector and length of line of sight segment
	//Vector sightDir = to - from;
	//float sightLength = sightDir.NormalizeInPlace();

	FOR_EACH_LL( m_activeGrenadeList, it )
	{
		ActiveGrenade *ag = m_activeGrenadeList[ it ];
		const float smokeRadiusSq = ag->GetRadius() * ag->GetRadius() * grenadeBloat * grenadeBloat;

		if ( ag->IsSmoke() && CSGameRules() )
		{
			float flLengthAdd = CSGameRules()->CheckTotalSmokedLength( smokeRadiusSq, ag->GetDetonationPosition(), from, to );
			// get the totalSmokedLength and check to see if the line starts or stops in smoke.  If it does this will return -1 and we should just bail early
			if ( flLengthAdd == -1 )
				return true;

			totalSmokedLength += flLengthAdd;
		}
	}

	// define how much smoke a bot can see thru
	const float maxSmokedLength = 0.7f * SmokeGrenadeRadius;

	// return true if the total length of smoke-covered line-of-sight is too much
	return (totalSmokedLength > maxSmokedLength);
}


//--------------------------------------------------------------------------------------------------------------
void CBotManager::ClearDebugMessages( void )
{
	m_debugMessageCount = 0;
	m_currentDebugMessage = -1;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Add a new debug message to the message history
 */
void CBotManager::AddDebugMessage( const char *msg )
{
	if (++m_currentDebugMessage >= MAX_DBG_MSGS)
	{
		m_currentDebugMessage = 0;
	}

	if (m_debugMessageCount < MAX_DBG_MSGS)
	{
		++m_debugMessageCount;
	}

	Q_strncpy( m_debugMessage[ m_currentDebugMessage ].m_string, msg, MAX_DBG_MSG_SIZE );
	m_debugMessage[ m_currentDebugMessage ].m_age.Start();
}
