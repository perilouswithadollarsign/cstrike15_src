//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared code for cs teams
//
//=============================================================================//

#include "cbase.h"
#include "cs_team_shared.h"
#include "gametypes.h"
#include "cs_gamerules.h"

#if defined ( CLIENT_DLL )
#include "c_cs_team.h"
#define CCSTeam C_CSTeam
#define CBasePlayer C_BasePlayer
#else
#include "cs_team.h"
#endif

bool IsAssassinationQuest( uint32 questID )
{
	CEconQuestDefinition *pQuest = GetItemSchema()->GetQuestDefinition( questID );
	if ( pQuest && V_stristr( pQuest->GetQuestExpression(), "act_kill_target" ) )
		return true;
	
	return false;
}

// Checks basic conditions for a quest (mapgroup, mode, etc) to see if a quest is possible to complete
bool Helper_CheckQuestMapAndMode( const CEconQuestDefinition *pQuest )
{
	const char *szMapName = NULL;
	const char *szMapGroupName = NULL;
#if defined ( CLIENT_DLL )
	szMapName = engine->GetLevelNameShort();
	szMapGroupName = engine->GetMapGroupName();
#else
	szMapName = V_UnqualifiedFileName( STRING( gpGlobals->mapname ) );
	szMapGroupName = STRING( gpGlobals->mapGroupName );
#endif
	// Wrong map
	if ( !StringIsEmpty( pQuest->GetMap() ) && V_strcmp( szMapName, pQuest->GetMap() ) )
		return false;

	// Unless the map group is named after our map (so queued for a single map) also confirm we're using the right map group
	if ( V_strcmp( szMapGroupName, CFmtStr( "mg_%s", szMapName ) ) )
	{
		if ( !StringIsEmpty( pQuest->GetMapGroup() ) && V_strcmp( szMapGroupName, pQuest->GetMapGroup() ) )
		{
			return false;
		}
	}

	const char *szCurrentModeAsString = g_pGameTypes->GetGameModeFromInt( g_pGameTypes->GetCurrentGameType(), g_pGameTypes->GetCurrentGameMode() );
	// Mode doesn't match
	if ( V_strcmp( pQuest->GetGameMode(), szCurrentModeAsString ) )
		return false;

	return true;
}


bool IsAssassinationQuestActive( const CEconQuestDefinition *pQuest )
{
	if ( CSGameRules() && CSGameRules()->IsWarmupPeriod() )
		return false;

	// We need to have an active quest with the 'act_kill_target' requirement
	if ( !pQuest || !V_stristr( pQuest->GetQuestExpression(), "act_kill_target" ) )
		return false;

	// Validate target team
	if ( pQuest->GetTargetTeam() != TEAM_TERRORIST && pQuest->GetTargetTeam() != TEAM_CT )
		return false;

	if ( !Helper_CheckQuestMapAndMode( pQuest ) )
		return false;

	return true;
}

