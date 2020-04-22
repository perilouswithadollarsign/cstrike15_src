//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//


#include "game.h"
#include "gamemanager.h"
#include "matchsystem.h"
#include "playermanager.h"
#include "matchmaking.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


CMatchGame::CMatchGame()
{
}

CMatchGame::~CMatchGame()
{
}

void CMatchGame::Reset()
{
	RemoveAllPlayers();
}

XNKID CMatchGame::GetXNKID()
{
	return (XNKID) g_pMatchmaking->GetSessionXNKID();
}

int CMatchGame::GetNumPlayers()
{
	return m_PlayerList.Count();
}

IPlayer * CMatchGame::GetPlayer( int index )
{
	if ( m_PlayerList.IsValidIndex( index ) )
		return m_PlayerList[index];
	else
		return NULL;
}

void CMatchGame::AddPlayer( XUID xuid, int pExperience[ MATCH_MAX_DIFFICULTIES ] /*= NULL*/, int pSkills[ MATCH_MAX_DIFFICULTIES ][ MATCH_MAX_SKILL_FIELDS ] /*= NULL*/ )
{
	if ( MM_IsDebug() )
	{
		Msg( "[L4DMM] CMatchGame::AddPlayer( %llx )\n", xuid );
	}

	IPlayer * player = g_pPlayerManager->FindPlayer( xuid );
	if ( !player )
	{
		Warning( "[L4DMM] CMatchGame::AddPlayer - Cannot find player by %llx!\n", xuid );
		return;
	}

	// Find a player first
	for ( int k = 0; k < m_PlayerList.Count(); ++ k )
	{
		if( m_PlayerList[k]->GetXUID() == xuid )
		{
			Warning( "[L4DMM] CMatchGame::AddPlayer - player %llx already added!\n", xuid );
			return;
		}
	}

	m_PlayerList.AddToTail( player );
}

void CMatchGame::RemovePlayer( XUID xuid )
{
	if ( MM_IsDebug() )
	{
		Msg( "[L4DMM] CMatchGame::RemovePlayer( %llx )\n", xuid );
	}

	int numRemoved = 0;
	for ( int k = 0; k < m_PlayerList.Count(); ++ k )
	{
		if( m_PlayerList[k]->GetXUID() == xuid )
		{
			m_PlayerList.Remove( k );
			++ numRemoved;
		}
	}

	if ( !numRemoved )
	{
		Warning( "[L4DMM] CMatchGame::RemovePlayer - Cannot find player by %llx!\n", xuid );
	}
}

void CMatchGame::RemoveAllPlayers( )
{
	if ( MM_IsDebug() )
	{
		Msg( "[L4DMM] CMatchGame::RemoveAllPlayers\n" );
	}

	m_PlayerList.RemoveAll();
}

int CMatchGame::GetAggregateExperience( )
{
	int difficulty = 1; // CMatchSystem::GetMatchSystem()->GetManager()->GameGetDifficulty();

	double exp = 0;
	int numExp = 0;

	int numPlayers = m_PlayerList.Count();
	
	for( int index = 0; index < numPlayers; ++index )
	{
		IPlayer * player = GetPlayer( index );
		if( player )
		{
			// TODO: exp += player->GetExperience( difficulty );
			++ numExp;
		}
	}

	if( numExp )
		exp /= numExp;

	if ( MM_IsDebug() >= 2 )
	{
		Msg( "[L4DMM] Game aggregate experience on difficulty%d = %f out of %d exp players.\n", difficulty, exp, numExp );
	}
	
	return exp;
}

int CMatchGame::GetAggregateSkill( int iSkillType )
{
	int difficulty = 1; // CMatchSystem::GetMatchSystem()->GetManager()->GameGetDifficulty();

	double skill = 0;
	int numSkills = 0;

	int numPlayers = m_PlayerList.Count();

	for( int index = 0; index < numPlayers; ++index )
	{
		IPlayer * player = GetPlayer( index );
		if( player )
		{
			// TODO: skill += player->GetSkill( iSkillType, difficulty );
			++ numSkills;
		}
	}
	
	if( numSkills )
		skill /= numSkills;

	if ( MM_IsDebug() >= 2 )
	{
		Msg( "[L4DMM] Game aggregate skill%d on difficulty%d = %f out of %d skill players.\n", iSkillType, difficulty, skill, numSkills );
	}
	
	return skill;
}


