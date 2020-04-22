//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_title_richpresence.h"
#include "swarm.spa.h"

#include "matchext_swarm.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//
// Mapping of context values
//

ContextValue_t g_pcv_CONTEXT_GAME_MODE[] = {
	{ "versus",			CONTEXT_GAME_MODE_VERSUS },
	{ "teamversus",		CONTEXT_GAME_MODE_TEAMVERSUS },
	{ "scavenge",		CONTEXT_GAME_MODE_SCAVENGE },
	{ "teamscavenge",	CONTEXT_GAME_MODE_TEAMSCAVENGE },
	{ "survival",		CONTEXT_GAME_MODE_SURVIVAL },
	{ "realism",		CONTEXT_GAME_MODE_REALISM },
	{ NULL,				CONTEXT_GAME_MODE_COOP },
};

ContextValue_t g_pcv_CONTEXT_STATE[] = {
	{ "game",			CONTEXT_STATE_GAME },
	{ "finale",			CONTEXT_STATE_FINALE },
	{ NULL,				CONTEXT_STATE_LOBBY },
};

ContextValue_t g_pcv_CONTEXT_DIFFICULTY[] = {
	{ "easy",			CONTEXT_DIFFICULTY_EASY },
	{ "hard",			CONTEXT_DIFFICULTY_HARD },
	{ "impossible",		CONTEXT_DIFFICULTY_IMPOSSIBLE },
	{ NULL,				CONTEXT_DIFFICULTY_NORMAL },
};

ContextValue_t g_pcv_CONTEXT_ACCESS[] = {
	{ "public",		CONTEXT_ACCESS_PUBLIC },
	{ "friends",	CONTEXT_ACCESS_FRIENDS },
	{ NULL,			CONTEXT_ACCESS_PRIVATE },
};


//
// User context and property setting
//

static void SetAllUsersContext( DWORD dwContextId, DWORD dwValue, bool bAsync = true )
{
#ifdef _X360
	for ( int k = 0; k < ( int ) XBX_GetNumGameUsers(); ++ k )
	{
		if ( XBX_GetUserIsGuest( k ) )
			continue;
		int iCtrlr = XBX_GetUserId( k );
		if ( bAsync )
			XUserSetContextEx( iCtrlr, dwContextId, dwValue, MMX360_NewOverlappedDormant() );
		else
			XUserSetContext( iCtrlr, dwContextId, dwValue );
	}
#endif
}

static void SetAllUsersProperty( DWORD dwPropertyId, DWORD cbValue, void const *pvValue )
{
#ifdef _X360
	for ( int k = 0; k < ( int ) XBX_GetNumGameUsers(); ++ k )
	{
		if ( XBX_GetUserIsGuest( k ) )
			continue;
		int iCtrlr = XBX_GetUserId( k );
		XUserSetPropertyEx( iCtrlr, dwPropertyId, cbValue, pvValue, MMX360_NewOverlappedDormant() );
	}
#endif
}

KeyValues * MM_Title_RichPresence_PrepareForSessionCreate( KeyValues *pSettings )
{
	if ( char const *szValue = pSettings->GetString( "game/mode", NULL ) )
	{
		SetAllUsersContext( X_CONTEXT_GAME_MODE, g_pcv_CONTEXT_GAME_MODE->ScanValues( szValue ), false );
	}

	// matchmaking version
	{
		static int val; // must be valid for the async call
		extern ConVar mm_matchmaking_version;
		val = mm_matchmaking_version.GetInt();
		SetAllUsersProperty( PROPERTY_MMVERSION, sizeof( val ), &val );
		DevMsg( "PrepareForSessionCreate: matchmaking version %d\n", val );
	}

	return NULL;
}

void MM_Title_RichPresence_Update( KeyValues *pFullSettings, KeyValues *pUpdatedSettings )
{
	if ( !pFullSettings )
	{
		SetAllUsersContext( X_CONTEXT_PRESENCE, 1 ); // main menu
		return;
	}

	// Also set players information during initial rich presence update
	if ( !pUpdatedSettings && pFullSettings )
	{
		MM_Title_RichPresence_PlayersChanged( pFullSettings );
	}

	// pUpdatedSettings = NULL when the session is created and all contexts need to be set
	KeyValues *pNewSettings = pUpdatedSettings ? pUpdatedSettings : pFullSettings;

	// Current mission/map (can be NULL!)
	char const *szCampaign = pFullSettings->GetString( "game/campaign", "" );
	KeyValues *pInfoMission = szCampaign[0] ? g_pMatchExtSwarm->GetAllMissions()->FindKey( szCampaign ) : NULL;
	KeyValues *pInfoChapter = g_pMatchExtSwarm->GetMapInfo( pFullSettings );

	if ( char const *szValue = pNewSettings->GetString( "system/access", NULL ) )
	{
		SetAllUsersContext( CONTEXT_ACCESS, g_pcv_CONTEXT_ACCESS->ScanValues( szValue ) );
	}

	if ( char const *szValue = pNewSettings->GetString( "game/state", NULL ) )
	{
		SetAllUsersContext( CONTEXT_STATE, g_pcv_CONTEXT_STATE->ScanValues( szValue ) );
	}

	if ( char const *szValue = pNewSettings->GetString( "game/mode", NULL ) )
	{
		SetAllUsersContext( X_CONTEXT_GAME_MODE, g_pcv_CONTEXT_GAME_MODE->ScanValues( szValue ) );
	}

	if ( char const *szValue = pNewSettings->GetString( "game/difficulty", NULL ) )
	{
		SetAllUsersContext( CONTEXT_DIFFICULTY, g_pcv_CONTEXT_DIFFICULTY->ScanValues( szValue ) );
	}

	if ( KeyValues *kvVal = pNewSettings->FindKey( "game/maxrounds" ) )
	{
		static int val; // must be valid for the async call
		val = kvVal->GetInt();
		SetAllUsersProperty( PROPERTY_MAXROUNDS, sizeof( val ), &val );
	}

	bool bSetLevelDescription = false;

	if ( char const *szValue = pNewSettings->GetString( "game/campaign", NULL ) )
	{
		int iCampaign = pInfoMission->GetInt( "x360ctx", CONTEXT_CAMPAIGN_ANY );

		SetAllUsersContext( CONTEXT_CAMPAIGN, iCampaign );
		bSetLevelDescription = true;
	}

	if ( KeyValues *kvVal = pNewSettings->FindKey( "game/chapter" ) )
	{
		static int val; // must be valid for the async call
		val = kvVal->GetInt();
		SetAllUsersProperty( PROPERTY_CHAPTER, sizeof( val ), &val );
		bSetLevelDescription = true;
	}

	if ( bSetLevelDescription )
	{
		int iContext = CONTEXT_LEVELDESCRIPTION_ANY;
		if ( pInfoChapter )
			iContext = pInfoChapter->GetInt( "x360ctx", iContext );
		else if ( pInfoMission )
			iContext = pInfoMission->GetInt( "x360ctx", iContext );	// mission contexts correspond to chapters too
		SetAllUsersContext( CONTEXT_LEVELDESCRIPTION, iContext );
	}

	if ( KeyValues *kvVal = pNewSettings->FindKey( "game/dlcrequired" ) )
	{
		static int val[10]; // must be valid for the async call
		uint64 uiDlcRequired = kvVal->GetUint64();
		extern ConVar mm_matchmaking_dlcsquery;
		for ( int k = 1; k <= mm_matchmaking_dlcsquery.GetInt(); ++ k )
		{
			val[k] = !!( uiDlcRequired & ( 1ull << k ) );
			DevMsg( "DLC%d required: %d\n", k, val[k] );
			SetAllUsersProperty( PROPERTY_MMVERSION + k, sizeof( val ), &val );
		}
	}

	//
	// Determine Rich Presence Display
	//
	if ( char const *szGameModeForRichPresence = pFullSettings->GetString( "game/mode", NULL ) )
	{
		unsigned int dwLevelPresence = CONTEXT_PRESENCE_MAINMENU;

		static ContextValue_t values[] = {
			{ "versus",			CONTEXT_PRESENCE_MODE_GAP_MISSION_LOBBY },
			{ "teamversus",		CONTEXT_PRESENCE_MODE_GAP_MISSION_LOBBY },
			
			{ "survival",		CONTEXT_PRESENCE_MODE_GAP_LEVEL_LOBBY },
			{ "scavenge",		CONTEXT_PRESENCE_MODE_GAP_LEVEL_LOBBY },
			{ "teamscavenge",	CONTEXT_PRESENCE_MODE_GAP_LEVEL_LOBBY },
			
			{ "coop",			CONTEXT_PRESENCE_MODE_DIFF_MISSION_LOBBY },
			{ "realism",		CONTEXT_PRESENCE_MODE_DIFF_MISSION_LOBBY },
			
			{ NULL,				CONTEXT_PRESENCE_MAINMENU },
		};

		dwLevelPresence = values->ScanValues( szGameModeForRichPresence );

		// Hook for offline game
		if ( !Q_stricmp( "offline", pFullSettings->GetString( "system/network" ) ) )
			dwLevelPresence = CONTEXT_PRESENCE_LOCAL_DIFF_MISSION_SETTINGS;

		// Special hook for developers commentary
		if ( !Q_stricmp( "commentary", pFullSettings->GetString( "options/play" ) ) )
			dwLevelPresence = CONTEXT_PRESENCE_DEVELOPER_COMM_SETTINGS;

		// If the game is active, then use the +1 presence
		if ( Q_stricmp( "lobby", pFullSettings->GetString( "game/state", "lobby" ) ) )
			++ dwLevelPresence;

		dwLevelPresence = pInfoMission->GetInt( "x360presence", dwLevelPresence );	// let the mission override
		dwLevelPresence = pInfoChapter->GetInt( "x360presence", dwLevelPresence );	// let the chapter override

		SetAllUsersContext( X_CONTEXT_PRESENCE, dwLevelPresence );
	}
}

void MM_Title_RichPresence_PlayersChanged( KeyValues *pFullSettings )
{
	if ( int numPlayers = pFullSettings->GetInt( "members/numPlayers" ) )
	{
		static int val; // must be valid for the async call
		val = numPlayers;
		SetAllUsersProperty( PROPERTY_NUMPLAYERS, sizeof( val ), &val );
	}
}
