//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_title.h"
#include "mm_title_richpresence.h"

#include "vstdlib/random.h"
#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


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
// Implementation of CMatchTitleGameSettingsMgr
//

// Extends server game details
void CMatchTitleGameSettingsMgr::ExtendServerDetails( KeyValues *pDetails, KeyValues *pRequest )
{
	// Query server info
	INetSupport::ServerInfo_t si;
	g_pMatchExtensions->GetINetSupport()->GetServerInfo( &si );

	// Server is always in game
	pDetails->SetString( "game/state", "game" );

	//
	// Determine map info
	//
	{
		pDetails->SetString( "game/bspname", CFmtStr( "%s", si.m_szMapName ) );
	}
}

// Adds the essential part of game details to be broadcast
void CMatchTitleGameSettingsMgr::ExtendLobbyDetailsTemplate( KeyValues *pDetails, char const *szReason, KeyValues *pFullSettings )
{
	static KeyValues *pkvExt = KeyValues::FromString(
		"settings",
		" game { "
			" bspname #empty# "
		" } "
		);

	pDetails->MergeFrom( pkvExt, KeyValues::MERGE_KV_UPDATE );
}

// Extends game settings update packet for lobby transition,
// either due to a migration or due to an endgame condition
void CMatchTitleGameSettingsMgr::ExtendGameSettingsForLobbyTransition( KeyValues *pSettings, KeyValues *pSettingsUpdate, bool bEndGame )
{
	pSettingsUpdate->SetString( "game/state", "lobby" );
}

// Rolls up game details for matches grouping
KeyValues * CMatchTitleGameSettingsMgr::RollupGameDetails( KeyValues *pDetails, KeyValues *pRollup, KeyValues *pQuery )
{
	return NULL;
}

// Defines dedicated server search key
KeyValues * CMatchTitleGameSettingsMgr::DefineDedicatedSearchKeys( KeyValues *pSettings )
{
	if ( IsPC() )
	{		
		static ConVarRef sv_search_key( "sv_search_key" );

		KeyValues *pKeys = new KeyValues( "SearchKeys" );
		pKeys->SetString( "gametype", CFmtStr( "%s,sv_search_key_%s%d",
			"empty",
			sv_search_key.GetString(),
			g_pMatchExtensions->GetINetSupport()->GetEngineBuildNumber() ) );
		return pKeys;
	}
	else
	{
		return NULL;
	}
}

// Defines session search keys for matchmaking
KeyValues * CMatchTitleGameSettingsMgr::DefineSessionSearchKeys( KeyValues *pSettings )
{
	MEM_ALLOC_CREDIT();
	KeyValues *pResult = new KeyValues( "SessionSearch" );

	pResult->SetInt( "numPlayers", pSettings->GetInt( "members/numPlayers", XBX_GetNumGameUsers() ) );

/*
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
			if ( KeyValues *pAllMissions = g_pMatchExtL4D->GetAllMissions() )
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
*/
	{
		if ( char const *szValue = pSettings->GetString( "game/bspname", NULL ) )
		{
			pResult->SetString( "Filter=/game:bspname", szValue );
		}
	}

	return pResult;
}

// Initializes full game settings from potentially abbreviated game settings
void CMatchTitleGameSettingsMgr::InitializeGameSettings( KeyValues *pSettings )
{
	char const *szNetwork = pSettings->GetString( "system/network", "LIVE" );

	if ( KeyValues *kv = pSettings->FindKey( "game", true ) )
	{
		kv->SetString( "state", "lobby" );
	}
		

	// Offline games don't need slots and player setup
	if ( !Q_stricmp( "offline", szNetwork ) )
		return;

	//
	// Set the number of slots
	//
	int numSlots = 2;
	pSettings->SetInt( "members/numSlots", numSlots );
}

// Extends game settings update packet before it gets merged with
// session settings and networked to remote clients
void CMatchTitleGameSettingsMgr::ExtendGameSettingsUpdateKeys( KeyValues *pSettings, KeyValues *pUpdateDeleteKeys )
{
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
	return NULL;
}

// Executes the command on the session settings, this function on host
// is allowed to modify Members/Game subkeys and has to fill in modified players KeyValues
// When running on a remote client "ppPlayersUpdated" is NULL and players cannot
// be modified
void CMatchTitleGameSettingsMgr::ExecuteCommand( KeyValues *pCommand, KeyValues *pSessionSystemData, KeyValues *pSettings, KeyValues **ppPlayersUpdated )
{
	//char const *szCommand = pCommand->GetName();
}


