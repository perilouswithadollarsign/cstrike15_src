//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_title.h"
#include "mm_title_richpresence.h"
#include "swarm.spa.h"
#include "matchext_swarm.h"

#include "vstdlib/random.h"
#include "checksum_crc.h"
#include "fmtstr.h"

#include <locale>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar mm_matchmaking_version( "mm_matchmaking_version", "9" );
ConVar mm_matchmaking_dlcsquery( "mm_matchmaking_dlcsquery", "2" );

class CMatchTitleGameSettingsMgr : public IMatchTitleGameSettingsMgr
{
public:
	// Extends server game details
	virtual void ExtendServerDetails( KeyValues *pDetails, KeyValues *pRequest );

	// Adds the essential part of game details to be broadcast
	virtual void ExtendLobbyDetailsTemplate( KeyValues *pDetails, char const *szReason, KeyValues *pFullSettings );

	// Extends game settings update packet for lobby transition,
	// either due to a migration or due to an endgame condition
	virtual void ExtendGameSettingsForLobbyTransition( KeyValues *pSettings, KeyValues *pSettingsUpdate, bool bEndGame );


	// Rolls up game details for matches grouping
	virtual KeyValues * RollupGameDetails( KeyValues *pDetails, KeyValues *pRollup, KeyValues *pQuery );


	// Defines session search keys for matchmaking
	virtual KeyValues * DefineSessionSearchKeys( KeyValues *pSettings );

	// Defines dedicated server search key
	virtual KeyValues * DefineDedicatedSearchKeys( KeyValues *pSettings );


	// Initializes full game settings from potentially abbreviated game settings
	virtual void InitializeGameSettings( KeyValues *pSettings );

	// Extends game settings update packet before it gets merged with
	// session settings and networked to remote clients
	virtual void ExtendGameSettingsUpdateKeys( KeyValues *pSettings, KeyValues *pUpdateDeleteKeys );

	// Prepares system for session creation
	virtual KeyValues * PrepareForSessionCreate( KeyValues *pSettings );


	// Executes the command on the session settings, this function on host
	// is allowed to modify Members/Game subkeys and has to fill in modified players KeyValues
	// When running on a remote client "ppPlayersUpdated" is NULL and players cannot
	// be modified
	virtual void ExecuteCommand( KeyValues *pCommand, KeyValues *pSessionSystemData, KeyValues *pSettings, KeyValues **ppPlayersUpdated );

	// Prepares the lobby for game or adjust settings of new players who
	// join a game in progress, this function is allowed to modify
	// Members/Game subkeys and has to fill in modified players KeyValues
	virtual void PrepareLobbyForGame( KeyValues *pSettings, KeyValues **ppPlayersUpdated );

	// Prepares the host team lobby for game adjusting the game settings
	// this function is allowed to prepare modification package to update
	// Game subkeys.
	// Returns the update/delete package to be applied to session settings
	// and pushed to dependent two sesssion of the two teams.
	virtual KeyValues * PrepareTeamLinkForGame( KeyValues *pSettingsLocal, KeyValues *pSettingsRemote );
};

CMatchTitleGameSettingsMgr g_MatchTitleGameSettingsMgr;
IMatchTitleGameSettingsMgr *g_pIMatchTitleGameSettingsMgr = &g_MatchTitleGameSettingsMgr;


//
// Mission information block
//

// Transfer mission info keys from mission manifest into key values
static void TransferMissionInformationToInfo( char const *szMissionName, KeyValues *pInfo )
{
	KeyValues *pNewMission = NULL;
	if ( !pInfo )
		return;

	if ( !szMissionName || !*szMissionName )
	{
		pInfo->SetString( "version",		pNewMission->GetString( "version", "1" ) );
#ifndef _X360
		pInfo->SetString( "builtin",		pNewMission->GetString( "builtin", "1" ) );
		pInfo->SetString( "displaytitle",	pNewMission->GetString( "displaytitle", "#L4D360UI_Campaign_Any" ) );
		pInfo->SetString( "author",			pNewMission->GetString( "author", "" ) );
		pInfo->SetString( "website",		pNewMission->GetString( "website", "" ) );
#endif
		return;
	}

	// Determine the new MissionInfo
	if ( KeyValues *pAllMissions = g_pMatchExtSwarm->GetAllMissions() )
	{
		pNewMission = pAllMissions->FindKey( szMissionName );
	}

	pInfo->SetString( "version",		pNewMission->GetString( "version", "" ) );
#ifndef _X360
	pInfo->SetString( "builtin",		pNewMission->GetString( "builtin", "1" ) );
	pInfo->SetString( "displaytitle",	pNewMission->GetString( "displaytitle", "" ) );
	pInfo->SetString( "author",			pNewMission->GetString( "author", "" ) );
	pInfo->SetString( "website",		pNewMission->GetString( "website", "" ) );
#endif
}

// Determine the DLC mask for the current settings
static uint64 DetermineDlcRequiredMask( KeyValues *pAggregateSettings )
{
	KeyValues *pMissionInfo = NULL;
	KeyValues *pChapterInfo = g_pMatchExtSwarm->GetMapInfo( pAggregateSettings, &pMissionInfo );
	return
		pMissionInfo->GetUint64( "dlcmask" ) |
		pChapterInfo->GetUint64( "dlcmask" );
}


//
// Implementation of CMatchTitleGameSettingsMgr
//

// Adds the essential part of game details to be broadcast
void CMatchTitleGameSettingsMgr::ExtendLobbyDetailsTemplate( KeyValues *pDetails, char const *szReason, KeyValues *pFullSettings )
{
	static KeyValues *pkvExt = KeyValues::FromString(
		"settings",
		" game { "
			" mode #empty# "
			" difficulty #empty# "
			" maxrounds #int#3 "
			" campaign #empty# "
			" chapter #int#1 "
			" state #empty# "
			" missioninfo { "
				" version #empty# "
#ifndef _X360
				" builtin #empty# "
				" displaytitle #empty# "
				" author #empty# "
				" website #empty# "
#endif
			" } "
		" } "
		);

	pDetails->MergeFrom( pkvExt, KeyValues::MERGE_KV_UPDATE );

	if ( szReason && !Q_stricmp( szReason, "reserve" ) )
	{
		// For the reservation we have to analyze all DLCs of the session machines
		// and set special keys for the server reservation
		// e.g.: pDetails->SetInt( "GameExtras/bat", -1 );
	}
}

// Extends server game details
void CMatchTitleGameSettingsMgr::ExtendServerDetails( KeyValues *pDetails, KeyValues *pRequest )
{
	// Query server info
	INetSupport::ServerInfo_t si;
	g_pMatchExtensions->GetINetSupport()->GetServerInfo( &si );

	// Determine map name
	char const *szMapName = si.m_szMapName;
	if ( !si.m_bActive )
	{
		if ( IVEngineClient *pIVEngineClient = g_pMatchExtensions->GetIVEngineClient() )
		{
			szMapName = pIVEngineClient->GetLevelNameShort();
		}
	}
	if ( !szMapName )
		szMapName = "";

	// Server is always in game
	pDetails->SetString( "game/state", "game" );

	//
	// Set game mode based on convars
	//
	static ConVarRef mp_gamemode( "mp_gamemode", true );
	if ( mp_gamemode.IsValid() )
	{
		pDetails->SetString( "game/mode", mp_gamemode.GetString() );
	}

	//
	// Determine game campaign and map info
	//
	KeyValues *pMissionInfo = NULL;
	if ( KeyValues *pMapInfo = g_pMatchExtSwarm->GetMapInfoByBspName( pDetails, szMapName, &pMissionInfo ) )
	{
		pDetails->SetString( "game/campaign", pMissionInfo->GetString( "name" ) );
		pDetails->SetInt( "game/chapter", pMapInfo->GetInt( "chapter" ) );

		// Setup the mission info keys
		TransferMissionInformationToInfo(
			pDetails->GetString( "game/campaign" ),
			pDetails->FindKey( "game/missioninfo", true ) );
	}

	//
	// Determine game difficulty
	//
	static ConVarRef ZombieDifficulty( "z_difficulty", true );
	if ( ZombieDifficulty.IsValid() )
	{
		pDetails->SetString( "game/difficulty", ZombieDifficulty.GetString() );
	}

	//
	// Determine max rounds
	//
	static ConVarRef r_mp_roundlimit( "mp_roundlimit", true );
	if ( r_mp_roundlimit.IsValid() )
	{
		pDetails->SetInt( "game/maxrounds", r_mp_roundlimit.GetInt() );
	}

	//
	// Determine required dlc mask
	//
	pDetails->SetUint64( "game/dlcrequired", DetermineDlcRequiredMask( pDetails ) );
}

// Extends game settings update packet for lobby transition,
// either due to a migration or due to an endgame condition
void CMatchTitleGameSettingsMgr::ExtendGameSettingsForLobbyTransition( KeyValues *pSettings, KeyValues *pSettingsUpdate, bool bEndGame )
{
	pSettingsUpdate->SetString( "game/state", "lobby" );

	char const *szGameMode = pSettings->GetString( "game/mode", "coop" );
	szGameMode;

	if ( bEndGame )
	{
		bool bNoRollChapterTo1 = false; // !Q_stricmp( szGameMode, "survival" ) || !Q_stricmp( szGameMode, "scavenge" ) || !Q_stricmp( szGameMode, "teamscavenge" );
		
		if ( !bNoRollChapterTo1 )
		{
			pSettingsUpdate->SetInt( "game/chapter", 1 );
		}
	}
}

// Adds a lowercased string into crc
void AppendToRollup( char const *sz, CRC32_t &u )
{
	if ( !sz )
		return;
	
	char const *p1 = sz;
	char const *p2 = p1;
	while ( *p2 )
	{
		while ( *p2 && !isupper( *p2 ) )
		{
			++ p2;
		}
		
		if ( p2 > p1 )
		{
			CRC32_ProcessBuffer( &u, p1, p2 - p1 );
		}
		
		if ( *p2 )
		{
			char ch = tolower( *p2 );
			++ p2;
			CRC32_ProcessBuffer( &u, &ch, sizeof( ch ) );
		}

		p1 = p2;
	}
}

// Rolls up game details for matches grouping
KeyValues * CMatchTitleGameSettingsMgr::RollupGameDetails( KeyValues *pDetails, KeyValues *pRollup, KeyValues *pQuery )
{
	if ( !pDetails && !pRollup )
		return NULL;

	MEM_ALLOC_CREDIT();
	if ( !pDetails )
	{
		// Check if the rollup is for a not installed addon with website
		if ( pRollup->GetInt( "game/missioninfo/builtin", 0 ) )
			return NULL;	// builtin, not interested

		char const *szWebsite = pRollup->GetString( "game/missioninfo/website", NULL );
		if ( !szWebsite || !*szWebsite )
			return NULL;	// no website, not interested

		int const nInstalledVersion = g_pMatchExtSwarm->GetAllMissions()->GetInt(
			CFmtStr( "%s/version", pRollup->GetString( "game/campaign" ) ), -1 );
		if ( pRollup->GetInt( "game/missioninfo/version", 0 ) <= nInstalledVersion )
			return NULL;	// addon installed with same or newer version, not interested

		// Zero out games information
		if ( KeyValues *pAgg = pRollup->FindKey( "rollup" ) )
		{
			pRollup->RemoveSubKey( pAgg );
			pAgg->deleteThis();
		}

		return pRollup;
	}

	if ( !pRollup )
	{
		// Determine the game mode
		char const *szGameMode = pDetails->GetString( "game/mode" );
		if ( !szGameMode || !*szGameMode )
			return NULL;

		// For chapter-based rollups
		bool bChapterBased = !Q_stricmp( "survival", szGameMode ) || !Q_stricmp( "scavenge", szGameMode );
		bool bDifficulty = !Q_stricmp( "coop", szGameMode ) || !Q_stricmp( "realism", szGameMode );

		// Determine campaign name
		char const *szCampaign = pDetails->GetString( "game/campaign" );
		if ( !szCampaign || !*szCampaign )
			return NULL;

		// Prepare the rollup
		pRollup = new KeyValues( "rollup" );
		CRC32_t uRollupKey = 0;
		CRC32_Init( &uRollupKey );

		// Game/mode
		pRollup->SetString( "game/mode", szGameMode );
		AppendToRollup( szGameMode, uRollupKey );

		// Game/campaign
		pRollup->SetString( "game/campaign", szCampaign );
		AppendToRollup( szCampaign, uRollupKey );

		// Game/chapter
		if ( bChapterBased )
		{
			int iChapter = pDetails->GetInt( "game/chapter", -1 );
			if ( iChapter > 0 )
			{
				pRollup->SetInt( "game/chapter", iChapter );
				CRC32_ProcessBuffer( &uRollupKey, &iChapter, sizeof( iChapter ) );
			}
		}

		// Game/difficulty
		if ( bDifficulty )
		{
			char const *szDifficulty = pDetails->GetString( "game/difficulty", NULL );
			if ( szDifficulty && *szDifficulty )
			{
				pRollup->SetString( "game/difficulty", szDifficulty );
				AppendToRollup( szDifficulty, uRollupKey );
			}
		}

		// Game/state, only if queried for specific game state
		if ( char const *szGameState = pQuery->GetString( "game/state", NULL ) )
		{
			char const *szState = pDetails->GetString( "game/state", NULL );
			if ( szState && *szState )
			{
				pRollup->SetString( "game/state", szState );
				// AppendToRollup( szState, uRollupKey ); <- state doesn't affect the rollup
			}
		}

		// Let the aggregation system know the key
		pRollup->SetUint64( "rollupkey", uRollupKey );
		return pRollup;
	}

	// We need to rollup the formula
	if ( pDetails && pRollup )
	{
		// Add the rollup mission info
		int iVersion = pRollup->GetInt( "game/missioninfo/version", -1 ), iVersionD = pDetails->GetInt( "game/missioninfo/version" );
		if ( iVersionD > iVersion )
		{
			pRollup->FindKey( "game/missioninfo", true )->MergeFrom( pDetails->FindKey( "game/missioninfo" ), KeyValues::MERGE_KV_UPDATE );
		}

		// Add the rollup formula
		char const *szGameState = pDetails->GetString( "game/state", "game" );
		if ( Q_stricmp( szGameState, "lobby" ) && Q_stricmp( szGameState, "game" ) )
			szGameState = "game"; // force all other states like "finale" to be game
		CFmtStr strRollupKey( "rollup/%s", szGameState );
		pRollup->SetInt( strRollupKey, 1 + pRollup->GetInt( strRollupKey ) );

		// Rolled up
		return pRollup;
	}

	return NULL;
}

// Defines dedicated server search key
KeyValues * CMatchTitleGameSettingsMgr::DefineDedicatedSearchKeys( KeyValues *pSettings )
{
	if ( IsPC() )
	{
		MEM_ALLOC_CREDIT();
		char const *szGameMode = pSettings->GetString( "game/mode", "coop" );
		if ( !szGameMode || !*szGameMode )
			szGameMode = "empty";

		char const *szMissionTag = NULL;
		KeyValues *pMissionInfo = NULL;
		if ( g_pMatchExtSwarm->GetMapInfo( pSettings, &pMissionInfo ) )
		{
			if ( !pMissionInfo->GetInt( "builtin" ) )
			{
				szMissionTag = pMissionInfo->GetString( "cfgtag" );
			}
		}
		
		static ConVarRef sv_search_key( "sv_search_key" );

		KeyValues *pKeys = new KeyValues( "SearchKeys" );
		pKeys->SetString( "gamedata", CFmtStr( szMissionTag ? "%s,key:%s%d,%s" : "%s,key:%s%d",
			szGameMode,
			sv_search_key.GetString(),
			g_pMatchExtensions->GetINetSupport()->GetEngineBuildNumber(),
			szMissionTag ) );
		return pKeys;
	}
	else
	{
		return NULL;
	}
}

// Helper function to set filter for BuiltIn criteria on PC
static void DefineSessionSearchKeys_BuiltIn( KeyValues *pSettings, KeyValues *pSearchKeys )
{
	MEM_ALLOC_CREDIT();
	char const *szValue = pSettings->GetString( "game/missioninfo/builtin", NULL );

	// Check if we need to force "official" setting
	char const *szCampaign = pSettings->GetString( "game/campaign" );
	if ( !*szCampaign && IsPC() )
	{
		// Any campaign - candidate for "official" restriction
		char const *szAction = pSettings->GetString( "options/action" );
		if ( !Q_stricmp( szAction, "quickmatch" ) ||
			 !Q_stricmp( szAction, "custommatch" ) )
		{
			szValue = "official";	// matchmaking on "ANY" = "official"
		}

		// Team matchmaking also means "official"
		if ( StringHasPrefix( pSettings->GetString( "game/mode" ), "team" ) )
		{
			szValue = "official";
		}
	}

	if ( szValue )
	{
		// Different values mean different search criteria:
		if ( !Q_stricmp( szValue, "addon" ) )
			pSearchKeys->SetInt( "Filter=/game:missioninfo:builtin", 0 );
		else if ( !Q_stricmp( szValue, "official" ) )
			pSearchKeys->SetInt( "Filter=/game:missioninfo:builtin", 1 );
		else if ( !Q_stricmp( szValue, "installedaddon" ) )
			pSearchKeys->SetInt( "Filter=/game:missioninfo:builtin", 0 );
		else if ( !Q_stricmp( szValue, "notinstalledaddon" ) )
		{
			pSearchKeys->SetInt( "Filter=/game:missioninfo:builtin", 0 );
			KeyValues *pAllMissions = g_pMatchExtSwarm->GetAllMissions();
			for ( KeyValues *pMission = pAllMissions ? pAllMissions->GetFirstTrueSubKey() : NULL; pMission; pMission = pMission->GetNextTrueSubKey() )
			{
				if ( !pMission->GetBool( "builtin" ) )
					pSearchKeys->FindKey( "Filter<>", true )->AddSubKey(
					new KeyValues( "game:campaign", NULL, pMission->GetString( "name" ) ) );
			}
			pSearchKeys->SetString( "Filter<>/game:missioninfo:website", "" );
		}
	}
}

// Defines session search keys for matchmaking
KeyValues * CMatchTitleGameSettingsMgr::DefineSessionSearchKeys( KeyValues *pSettings )
{
	MEM_ALLOC_CREDIT();
	KeyValues *pResult = new KeyValues( "SessionSearch" );

	pResult->SetInt( "numPlayers", pSettings->GetInt( "members/numPlayers", XBX_GetNumGameUsers() ) );
	
	char const *szGameMode = pSettings->GetString( "game/mode", "" );

	if ( IsX360() )
	{

		if ( char const *szValue = pSettings->GetString( "game/mode", NULL ) )
		{
			static ContextValue_t values[] = {
				{ "versus",			SESSION_MATCH_QUERY_PUBLIC_STATE_C___SORT___CHAPTER },
				{ "teamversus",		SESSION_MATCH_QUERY_TEAM_STATE_C_CHAPTER },
				{ "scavenge",		SESSION_MATCH_QUERY_PUBLIC_STATE_C_CHAPTER___SORT___ROUNDS },
				{ "teamscavenge",	SESSION_MATCH_QUERY_TEAM_STATE_C_CHAPTER_ROUNDS },
				{ "survival",		SESSION_MATCH_QUERY_PUBLIC_STATE_C_CHAPTER },
				{ "coop",			SESSION_MATCH_QUERY_PUBLIC_STATE_C_DIFF___SORT___CHAPTER },
				{ "realism",		SESSION_MATCH_QUERY_PUBLIC_STATE_C_DIFF___SORT___CHAPTER },
				{ NULL,				0xFFFF },
			};

			pResult->SetInt( "rule", values->ScanValues( szValue ) );
		}

		// Set the matchmaking version
		pResult->SetInt( CFmtStr( "Properties/%d", PROPERTY_MMVERSION ), mm_matchmaking_version.GetInt() );

#ifdef _X360
		// Set the installed DLCs masks
		uint64 uiDlcsMask = MatchSession_GetDlcInstalledMask();
		for ( int k = 1; k <= mm_matchmaking_dlcsquery.GetInt(); ++ k )
		{
			pResult->SetInt( CFmtStr( "Properties/%d", PROPERTY_MMVERSION + k ), !!( uiDlcsMask & ( 1ull << k ) ) );
		}
		pResult->SetInt( "dlc1", PROPERTY_MMVERSION + 1 );
		pResult->SetInt( "dlcN", PROPERTY_MMVERSION + mm_matchmaking_dlcsquery.GetInt() );
#endif

		// X_CONTEXT_GAME_TYPE 
		pResult->SetInt( CFmtStr( "Contexts/%d", X_CONTEXT_GAME_TYPE ), X_CONTEXT_GAME_TYPE_STANDARD );

		// X_CONTEXT_GAME_MODE 
		if ( char const *szValue = pSettings->GetString( "game/mode", NULL ) )
		{
			pResult->SetInt( CFmtStr( "Contexts/%d", X_CONTEXT_GAME_MODE ), g_pcv_CONTEXT_GAME_MODE->ScanValues( szValue ) );
		}

		if ( char const *szValue = pSettings->GetString( "game/state", NULL ) )
		{
			pResult->SetInt( CFmtStr( "Contexts/%d", CONTEXT_STATE ), g_pcv_CONTEXT_STATE->ScanValues( szValue ) );
		}

		if ( char const *szValue = pSettings->GetString( "game/difficulty", NULL ) )
		{
			if ( !Q_stricmp( "coop", szGameMode ) || !Q_stricmp( "realism", szGameMode ) )
			{
				pResult->SetInt( CFmtStr( "Contexts/%d", CONTEXT_DIFFICULTY ), g_pcv_CONTEXT_DIFFICULTY->ScanValues( szValue ) );
			}
		}

		if ( int val = pSettings->GetInt( "game/maxrounds" ) )
		{
			if ( !Q_stricmp( "scavenge", szGameMode ) || !Q_stricmp( "teamscavenge", szGameMode ) )
			{
				pResult->SetInt( CFmtStr( "Properties/%d", PROPERTY_MAXROUNDS ), val );
			}
		}

		char const *szCampaign = pSettings->GetString( "game/campaign" );
		if ( *szCampaign )
		{
			DWORD dwContext = CONTEXT_CAMPAIGN_UNKNOWN;
			if ( KeyValues *pAllMissions = g_pMatchExtSwarm->GetAllMissions() )
			{
				if ( KeyValues *pMission = pAllMissions->FindKey( szCampaign ) )
				{
					dwContext = pMission->GetInt( "x360ctx", dwContext );
				}
			}
			if ( dwContext != CONTEXT_CAMPAIGN_UNKNOWN )
			{
				pResult->SetInt( CFmtStr( "Contexts/%d", CONTEXT_CAMPAIGN ), dwContext );
			}
		}

		if ( int val = pSettings->GetInt( "game/chapter" ) )
		{
			pResult->SetInt( CFmtStr( "Properties/%d", PROPERTY_CHAPTER ), val );
		}

	}
	else
	{

		char const *szGameMode = pSettings->GetString( "game/mode" );

		if ( char const *szValue = pSettings->GetString( "game/state", NULL ) )
		{
			pResult->SetString( "Filter=/game:state", szValue );
		}

		if ( char const *szValue = pSettings->GetString( "game/mode", NULL ) )
		{
			pResult->SetString( "Filter=/game:mode", szValue );
		}

		if ( char const *szValue = pSettings->GetString( "game/difficulty", NULL ) )
		{
			pResult->SetString( "Filter=/game:difficulty", szValue );
		}

		if ( int val = pSettings->GetInt( "game/maxrounds" ) )
		{
			char const *szFilterType = NULL;
			if ( !Q_stricmp( "scavenge", szGameMode ) )
				szFilterType = "Near";
			else if ( !Q_stricmp( "teamscavenge", szGameMode ) )
				szFilterType = "Filter=";
			if ( szFilterType )
				pResult->SetInt( CFmtStr( "%s/game:maxrounds", szFilterType ), val );
		}

		char const *szCampaign = pSettings->GetString( "game/campaign" );
		if ( *szCampaign )
		{
			pResult->SetString( "Filter=/game:campaign", szCampaign );
		}

		if ( int val = pSettings->GetInt( "game/chapter" ) )
		{
			char const *szFilterType = "Near";
			if ( !Q_stricmp( "survival", szGameMode ) ||
				 !Q_stricmp( "scavenge", szGameMode ) ||
				 StringHasPrefix( szGameMode, "team" ) )
				szFilterType = "Filter=";

			pResult->SetInt( CFmtStr( "%s/game:chapter", szFilterType ), val );
		}

		// For all game modes (except team-on-team) prefer games with more players (e.g. prefer 7/8 game over 2/8 game)
		if ( !StringHasPrefix( szGameMode, "team" ) )
		{
			pResult->SetInt( "Near/members:numPlayers", 99 );	// passing in a random big number to cause descending sort order
		}

		// BuiltIn search keys
		DefineSessionSearchKeys_BuiltIn( pSettings, pResult );

		// For team-on-team game modes require dedicated/local servers preference to match
		if ( StringHasPrefix( szGameMode, "team" ) )
		{
			char const *szLocal = pSettings->GetString( "options/server" );
			char const *szFilter = Q_stricmp( szLocal, "listen" ) ? "Filter<>" : "Filter=";
			pResult->SetString( CFmtStr( "%s/options:server", szFilter ), "listen" );
		}

	}

	//
	// In case the search is quickmatch or custom match for
	// a game in progress, add a second search pass for games
	// in lobby. This way if the search doesn't find any games
	// in progress then it will at least find a lobby.
	//
	char const *szAction = pSettings->GetString( "options/action" );
	if ( ( !Q_stricmp( szAction, "quickmatch" ) ||
		   !Q_stricmp( szAction, "custommatch" ) ) &&
		   !Q_stricmp( "game", pSettings->GetString( "game/state", "" ) ) )
	{
		KeyValues *pNextSearchPass = pResult->MakeCopy();
		pNextSearchPass->SetName( "nextpass" );
		pResult->AddSubKey( pNextSearchPass );

		// When matchmaking for a game in progress allow lobbies to also be considered if no games in progress found
		if ( IsX360() )
			pNextSearchPass->SetInt( CFmtStr( "Contexts/%d", CONTEXT_STATE ), CONTEXT_STATE_LOBBY );
		else
			pNextSearchPass->SetString( "Filter=/game:state", "lobby" );
	}

	//
	// For team based game modes there are chances that a team link session
	// is not specifying a campaign/chapter, so we need to perform fallback
	// searches to ANY
	//
	if ( StringHasPrefix( szGameMode, "team" ) )
	{
		KeyValues *pLastPass = pResult;

		// If we specify a chapter, then try linking on ANY chapter
		if ( pSettings->GetInt( "game/chapter" ) )
		{
			KeyValues *pNextSearchPass = pLastPass->MakeCopy();
			pNextSearchPass->SetName( "nextpass" );
			pLastPass->AddSubKey( pNextSearchPass );

			pLastPass = pNextSearchPass;

			// Search for chapter = 0
			if ( IsX360() )
				pNextSearchPass->SetInt( CFmtStr( "Properties/%d", PROPERTY_CHAPTER ), 0 );
			else
				pNextSearchPass->SetInt( "Filter=/game:chapter", 0 );
		}

		// If we specify a campaign and that's a built-in campaign, then try linking on ANY campaign
		if ( *pSettings->GetString( "game/campaign" ) && ( IsX360() || pSettings->GetInt( "game/missioninfo/builtin" ) ) )
		{
			KeyValues *pNextSearchPass = pLastPass->MakeCopy();
			pNextSearchPass->SetName( "nextpass" );
			pLastPass->AddSubKey( pNextSearchPass );

			pLastPass = pNextSearchPass;

			// Search for campaign = ANY
			if ( IsX360() )
				pNextSearchPass->SetInt( CFmtStr( "Contexts/%d", CONTEXT_CAMPAIGN ), CONTEXT_CAMPAIGN_ANY );
			else
				pNextSearchPass->SetString( "Filter=/game:campaign", "" );
		}
	}


	return pResult;
}

#ifdef _DEBUG
ConVar mm_test_slots( "mm_test_slots", "0", FCVAR_DEVELOPMENTONLY, "Force the game to support a different number of max slots.\n" );
#endif

// Initializes full game settings from potentially abbreviated game settings
void CMatchTitleGameSettingsMgr::InitializeGameSettings( KeyValues *pSettings )
{
	MEM_ALLOC_CREDIT();
	char const *szNetwork = pSettings->GetString( "system/network", "LIVE" );
	char const *szPlayOptions = pSettings->GetString( "options/play", "" );

	if ( KeyValues *kv = pSettings->FindKey( "game", true ) )
	{
		kv->SetString( "state", "lobby" );

		KeyValuesAddDefaultString( kv, "mode", "coop" );
		char const *szGameMode = kv->GetString( "mode" );

		// Allowing ANY campaign/chapter
		bool bAllowAnyChapter = false;
		if ( StringHasPrefix( szGameMode, "team" ) )
			bAllowAnyChapter = true;

		// Build a list of random campaigns (weighted by chapters for single-chapter game modes)
		bool bSingleChapterGameMode = !Q_stricmp( "survival", szGameMode ) || !Q_stricmp( "scavenge", szGameMode );
		CFmtStr sModeChapters( "modes/%s/chapters", szGameMode );
		CUtlVector< KeyValues * > arrBuiltinMissions;
		for ( KeyValues *pMission = g_pMatchExtSwarm->GetAllMissions()->GetFirstTrueSubKey(); pMission; pMission = pMission->GetNextTrueSubKey() )
		{
			if ( !pMission->GetInt( "builtin" ) )
				continue;

			if ( !Q_stricmp( "credits", pMission->GetString( "name" ) ) )
				continue;

			int numChapters = pMission->GetInt( sModeChapters );
			if ( !numChapters )
				continue;

			// Weigh missions proportionally to the number of chapters
			// if the game mode is a single chapter
			arrBuiltinMissions.AddToTail( pMission );
			if ( bSingleChapterGameMode )
			{
				for ( int k = 1; k < numChapters; ++ k )
					arrBuiltinMissions.AddToTail( pMission );
			}
		}
		
		// Random campaign generation if player was searching for "Any" campaign
		if ( !bAllowAnyChapter && !*kv->GetString( "campaign" ) && arrBuiltinMissions.Count() )
		{
			int iRandomMission = RandomInt( 0, arrBuiltinMissions.Count() - 1 );
			kv->SetString( "campaign", arrBuiltinMissions[ iRandomMission ]->GetString( "name" ) );
		}
		
		// In survival/scavenge mode we also randomly generate the chapter if "Any" was searched
		if ( !bAllowAnyChapter && !kv->GetInt( "chapter" ) )
		{
			if ( bSingleChapterGameMode )
			{
				int nChapters = g_pMatchExtSwarm->GetAllMissions()->GetInt( CFmtStr( "%s/modes/%s/chapters", kv->GetString( "campaign" ), szGameMode ), 1 );
				int nRandomChapter = RandomInt( 1, nChapters );
				kv->SetInt( "chapter", nRandomChapter );
			}
			else
			{
				kv->SetInt( "chapter", 1 );
			}
		}
		Assert( bAllowAnyChapter || g_pMatchExtSwarm->GetMapInfo( pSettings ) );

		KeyValuesAddDefaultString( kv, "difficulty", "normal" );
		KeyValuesAddDefaultValue( kv, "maxrounds", 3, SetInt );

		if ( !Q_stricmp( "offline", szNetwork ) && arrBuiltinMissions.Count() )
		{
			kv->SetString( "campaign", arrBuiltinMissions[0]->GetString( "name" ) );	
			kv->SetInt( "chapter", 1 );
		}

		if ( !Q_stricmp( "commentary", szPlayOptions ) )
		{
			kv->SetString( "campaign", "L4D2C5" );	// Commentary is on C5
			kv->SetInt( "chapter", 1 );
			kv->SetString( "difficulty", "easy" );
		}

		// Credits are played on a dedicated map
		if ( !Q_stricmp( "credits", szPlayOptions ) )
		{
			kv->SetString( "campaign", "credits" );
			kv->SetInt( "chapter", 1 );
			kv->SetString( "difficulty", "easy" );
		}
	}

	// Setup the mission info keys
	TransferMissionInformationToInfo(
		pSettings->GetString( "game/campaign" ),
		pSettings->FindKey( "game/missioninfo", true ) );

	pSettings->SetUint64( "game/dlcrequired", DetermineDlcRequiredMask( pSettings ) );

	// Offline games don't need slots and player setup
	if ( !Q_stricmp( "offline", szNetwork ) )
		return;

	//
	// Set the number of slots
	//
	int numSlots = 4;
	const char *pszGameMode = pSettings->GetString( "game/mode", "" );
	if ( !Q_stricmp( "versus", pszGameMode ) || !Q_stricmp( "scavenge", pszGameMode ) )
	{
		numSlots = 8;
	}

#ifdef _DEBUG
	if ( int nForceDebugSlots = mm_test_slots.GetInt() )
		numSlots = nForceDebugSlots;
#endif

	pSettings->SetInt( "members/numSlots", numSlots );
}

// Extends game settings update packet before it gets merged with
// session settings and networked to remote clients
void CMatchTitleGameSettingsMgr::ExtendGameSettingsUpdateKeys( KeyValues *pSettings, KeyValues *pUpdateDeleteKeys )
{
	MEM_ALLOC_CREDIT();
	// Check if the campaign key is deleted
	if ( pUpdateDeleteKeys->FindKey( "delete/game/campaign" ) )
	{
		pUpdateDeleteKeys->SetString( "delete/game/missioninfo", "delete" );
	}

	// Check if the campaign key is modified
	if ( char const *szNewMission = pUpdateDeleteKeys->GetString( "update/game/campaign", NULL ) )
	{
		TransferMissionInformationToInfo( szNewMission,
			pUpdateDeleteKeys->FindKey( "update/game/missioninfo", true ) );
	}

	// Check if the campaign or chapter key is modified
	if ( pUpdateDeleteKeys->GetString( "update/game/campaign" ) ||
		 pUpdateDeleteKeys->GetString( "update/game/chapter" ) )
	{
		KeyValues *pAggregateSettings = pSettings->MakeCopy();
		KeyValues::AutoDelete autodelete( pAggregateSettings );
		pAggregateSettings->MergeFrom( pUpdateDeleteKeys );
		uint64 uiDlcMaskRequired = DetermineDlcRequiredMask( pAggregateSettings );
		if ( uiDlcMaskRequired != pAggregateSettings->GetUint64( "game/dlcrequired" ) )
			pUpdateDeleteKeys->SetUint64( "update/game/dlcrequired", uiDlcMaskRequired );
	}
}

// Prepares system for session creation
KeyValues * CMatchTitleGameSettingsMgr::PrepareForSessionCreate( KeyValues *pSettings )
{
	return MM_Title_RichPresence_PrepareForSessionCreate( pSettings );
}

// Prepares the lobby for game or adjust settings of new players who
// join a game in progress, this function is allowed to modify
// Members/Game subkeys and has to write modified players XUIDs
void CMatchTitleGameSettingsMgr::PrepareLobbyForGame( KeyValues *pSettings, KeyValues **ppPlayersUpdated )
{
	// set player avatar/teams, etc
}

// Prepares the host team lobby for game adjusting the game settings
// this function is allowed to prepare modification package to update
// Game subkeys.
// Returns the update/delete package to be applied to session settings
// and pushed to dependent two sesssion of the two teams.
KeyValues * CMatchTitleGameSettingsMgr::PrepareTeamLinkForGame( KeyValues *pSettingsLocal, KeyValues *pSettingsRemote )
{
	MEM_ALLOC_CREDIT();
	KeyValues *pUpdate = NULL;

	// Figure out game mode (game modes are assumed to match)
	char const *szGameMode = pSettingsLocal->GetString( "game/mode", "coop" );
	Assert( !Q_stricmp( szGameMode, pSettingsRemote->GetString( "game/mode", "coop" ) ) );

	// Check if either campaign is ANY
	char const *szCampaignLocal = pSettingsLocal->GetString( "game/campaign", "" );
	char const *szCampaignRemote = pSettingsRemote->GetString( "game/campaign", "" );
	// Campaigns should either designate ANY or should match
	Assert( !*szCampaignLocal || !*szCampaignRemote || !Q_stricmp( szCampaignLocal, szCampaignRemote ) );
	if ( !*szCampaignLocal || !*szCampaignRemote )
	{
		if ( !pUpdate )
			pUpdate = new KeyValues( "PrepareTeamLinkForGame" );

		// Assign a random campaign if none set
#ifdef _DEMO
		const char *rnd = "L4D2C5";
#else
		CFmtStr rnd( "L4D2C%d", RandomInt( 1, 5 ) );
#endif
		char const *szCampaign = rnd;
		
		// If either session wants a specific campaign, honor them
		if ( *szCampaignLocal )
			szCampaign = szCampaignLocal;
		if ( *szCampaignRemote )
			szCampaign = szCampaignRemote;

		// Set the update key
		pUpdate->SetString( "update/game/campaign", szCampaign );
		szCampaignLocal = pUpdate->GetString( "update/game/campaign" );
	}

	// Check if the chapter is ANY
	int nChapterLocal = pSettingsLocal->GetInt( "game/chapter", 0 );
	int nChapterRemote = pSettingsRemote->GetInt( "game/chapter", 0 );
	// Chapters either designate ANY or should match
	Assert( !nChapterLocal || !nChapterRemote || nChapterRemote == nChapterLocal );
	if ( !nChapterLocal || !nChapterRemote )
	{
		if ( !pUpdate )
			pUpdate = new KeyValues( "PrepareTeamLinkForGame" );
		
		// Generate a random chapter if both are ANY
		int nChapter = 1;
		if ( !Q_stricmp( "teamscavenge", szGameMode ) )
		{
			int nChapters = g_pMatchExtSwarm->GetAllMissions()->GetInt( CFmtStr( "%s/modes/%s/chapters", szCampaignLocal, szGameMode ), 1 );
			nChapter = RandomInt( 1, nChapters );
		}

		// If any session wants a specific chapter, honor them
		if ( nChapterLocal )
			nChapter = nChapterLocal;
		if ( nChapterRemote )
			nChapter = nChapterRemote;

		// Set the update key
		pUpdate->SetInt( "update/game/chapter", nChapter );
		nChapterLocal = nChapter;
	}

	return pUpdate;
}

static void OnRunCommand_Avatar( KeyValues *pCommand, KeyValues *pSettings, KeyValues **ppPlayersUpdated )
{
/*
	MEM_ALLOC_CREDIT();
	XUID xuid = pCommand->GetUint64( "xuid" );
	char const *szAvatar = pCommand->GetString( "avatar", "" );

	KeyValues *pMembers = pSettings->FindKey( "members" );
	if ( !pMembers )
		return;

	// Find the avatar that is going to be updated
	KeyValues *pPlayer = SessionMembersFindPlayer( pSettings, xuid );
	if ( !pPlayer )
		return;

	// Check if the avatar is the same
	if ( !Q_stricmp( szAvatar, pPlayer->GetString( "game/avatar", "" ) ) )
		return;

	KeyValues *kvGame = pPlayer->FindKey( "game", true );
	if ( !kvGame )
		return;

	// If desired avatar is blank, then no validation required
	if ( !*szAvatar )
	{
		kvGame->SetString( "avatar", "" );
	}
	else
	{
		// Count how many times the avatar is currently in use
		int numCurrentlyUsed = 0;

		int numMachines = pMembers->GetInt( "numMachines" );
		for ( int k = 0; k < numMachines; ++ k )
		{
			KeyValues *pMachine = pMembers->FindKey( CFmtStr( "machine%d", k ) );
			if ( !pMachine )
				continue;

			int numPlayers = pMachine->GetInt( "numPlayers" );
			for ( int j = 0; j < numPlayers; ++ j )
			{
				char const *szUsedAvatar = pMachine->GetString( CFmtStr( "player%d/game/avatar", j ), "" );
				if ( !Q_stricmp( szUsedAvatar, szAvatar ) )
					++ numCurrentlyUsed;
			}
		}

		// Validate that the avatar is not taken yet
		if ( !Q_stricmp( szAvatar, "Infected" ) )
		{
			if ( numCurrentlyUsed >= 4 )
				return;
		}
		else
		{
			if ( numCurrentlyUsed >= 1 )
				return;
		}

		kvGame->SetString( "avatar", szAvatar );
	}

	// Notify the sessions of a player update
	* ( ppPlayersUpdated ++ ) = pPlayer;
*/
}

// Executes the command on the session settings, this function on host
// is allowed to modify Members/Game subkeys and has to fill in modified players KeyValues
// When running on a remote client "ppPlayersUpdated" is NULL and players cannot
// be modified
void CMatchTitleGameSettingsMgr::ExecuteCommand( KeyValues *pCommand, KeyValues *pSessionSystemData, KeyValues *pSettings, KeyValues **ppPlayersUpdated )
{
	char const *szCommand = pCommand->GetName();

	if ( !Q_stricmp( "Game::Avatar", szCommand ) )
	{
		if ( !Q_stricmp( "host", pSessionSystemData->GetString( "type", "host" ) ) &&
			 // !Q_stricmp( "lobby", pSessionSystemData->GetString( "state", "lobby" ) ) && - avatars also update when players change team ingame
			 ppPlayersUpdated )
		{
			char const *szAvatar = pCommand->GetString( "avatar" );
			if ( !*szAvatar )
			{
				// Requesting random is only allowed in unlocked lobby
				if ( Q_stricmp( "lobby", pSessionSystemData->GetString( "state", "lobby" ) ) )
					return;
				if ( *pSettings->GetString( "system/lock" ) )
					return;
			}
			OnRunCommand_Avatar( pCommand, pSettings, ppPlayersUpdated );
			return;
		}
	}
}


