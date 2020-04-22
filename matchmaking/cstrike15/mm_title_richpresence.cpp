//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_title_richpresence.h"
#include "mm_title_contextvalues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static void SetAllUsersContext( DWORD dwContextId, DWORD dwValue, bool bAsync = true )
{
#ifdef _X360
	for ( int k = 0; k < ( int ) XBX_GetNumGameUsers(); ++ k )
	{
		if ( XBX_GetUserIsGuest( k ) )
			continue;
		int iCtrlr = XBX_GetUserId( k );
		
		//if ( dwContextId == X_CONTEXT_PRESENCE ) DevMsg( "Set presence to %d for user %d\n", dwValue, iCtrlr );

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
	SetAllUsersContext( X_CONTEXT_GAME_MODE, CONTEXT_GAME_MODE_CSS_GAME_MODE_MULTIPLAYER, false );

	// matchmaking version
	{
		static int val; // must be valid for the async call
		extern ConVar mm_title_debug_version;
		val = mm_title_debug_version.GetInt();
		SetAllUsersProperty( PROPERTY_CSS_MATCH_VERSION, sizeof( val ), &val );
		DevMsg( "PrepareForSessionCreate: matchmaking version %d\n", val );
	}

	return NULL;
}

void MM_Title_RichPresence_Update( KeyValues *pFullSettings, KeyValues *pUpdatedSettings )
{
#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
	( void ) g_pMatchExtensions->GetIBaseClientDLL()->GetRichPresenceStatusString();
#endif
#ifdef _X360
	if ( !pFullSettings )
	{
		SetAllUsersContext( X_CONTEXT_PRESENCE, CONTEXT_PRESENCE_MAINMENU ); // main menu
		return;
	}

	// Also set players information during initial rich presence update
	if ( !pUpdatedSettings && pFullSettings )
	{
		MM_Title_RichPresence_PlayersChanged( pFullSettings );

		// Open slots
		int numSlots = pFullSettings->GetInt( "members/numSlots", XBX_GetNumGameUsers() );
		{
			static int val; // must be valid for the async call
			val = numSlots;
			SetAllUsersProperty( PROPERTY_CSS_OPEN_SLOTS, sizeof( val ), &val );
		}

		// Team slots
		int numTeamSlots = MAX( pFullSettings->GetInt( "members/numTSlotsFree", 0 ), pFullSettings->GetInt( "members/numCTSlotsFree", 0 ) );
		{
			static int val; // must be valid for the async call
			val = numTeamSlots;
			SetAllUsersProperty( PROPERTY_CSS_MAX_OPEN_TEAM_SLOTS, sizeof( val ), &val );
		}

		// Skill fields
		{
			static int val = 0; // must be valid for the async call
			SetAllUsersProperty( PROPERTY_CSS_AGGREGATE_EXPERIENCE, sizeof( val ), &val );
			SetAllUsersProperty( PROPERTY_CSS_AGGREGATE_SKILL0, sizeof( val ), &val );
			SetAllUsersProperty( PROPERTY_CSS_AGGREGATE_SKILL1, sizeof( val ), &val );
			SetAllUsersProperty( PROPERTY_CSS_AGGREGATE_SKILL2, sizeof( val ), &val );
			SetAllUsersProperty( PROPERTY_CSS_AGGREGATE_SKILL3, sizeof( val ), &val );
			SetAllUsersProperty( PROPERTY_CSS_AGGREGATE_SKILL4, sizeof( val ), &val );
		}

		// Listen/dedicated server resolver
		{
			static int val; // must be valid for the async call
			val = 0;
			extern ConVar mm_title_debug_dccheck;
			if ( mm_title_debug_dccheck.GetInt() )
				val = ( ( mm_title_debug_dccheck.GetInt() > 0 ) ? 0 : 1 );
			SetAllUsersProperty( PROPERTY_CSS_SEARCH_LISTEN_SERVER, sizeof( val ), &val );
		}
	}

	// pUpdatedSettings = NULL when the session is created and all contexts need to be set
	KeyValues *pNewSettings = pUpdatedSettings ? pUpdatedSettings : pFullSettings;
	
// 	if ( KeyValues *kvVal = pNewSettings->FindKey( "game/dlcrequired" ) )
// 	{
// 		static int val[10]; // must be valid for the async call
// 		uint64 uiDlcRequired = kvVal->GetUint64();
// 		extern ConVar mm_matchmaking_dlcsquery;
// 		for ( int k = 1; k <= mm_matchmaking_dlcsquery.GetInt(); ++ k )
// 		{
// 			val[k] = !!( uiDlcRequired & ( 1ull << k ) );
// 			DevMsg( "DLC%d required: %d\n", k, val[k] );
// 			SetAllUsersProperty( PROPERTY_REQUIRED_DLC1 - 1 + k, sizeof( val ), &val );
// 		}
// 	}

	// Actual game type (classic, gungame)
	if ( char const *szGameType = pNewSettings->GetString( "game/type", NULL ) )
	{
		SetAllUsersContext( CONTEXT_CSS_GAME_TYPE, g_GameTypeContexts->ScanValues( szGameType ) );
	}

	// Game state
	if ( char const *szGameState = pNewSettings->GetString( "game/state", NULL ) )
	{
		if ( !V_stricmp( pFullSettings->GetString( "system/network" ), "offline" ) )
			SetAllUsersContext( CONTEXT_GAME_STATE, CONTEXT_GAME_STATE_SINGLE_PLAYER );
		else
			SetAllUsersContext( CONTEXT_GAME_STATE, ( !V_stricmp( "game", szGameState ) ) ? CONTEXT_GAME_STATE_MULTIPLAYER : CONTEXT_GAME_STATE_IN_MENUS );
	}

	// Actual game mode (casual, competitive, pro, etc)
	if ( char const *szValue = pNewSettings->GetString( "game/mode", NULL ) )
	{
		SetAllUsersContext( CONTEXT_CSS_GAME_MODE, g_GameModeContexts->ScanValues( szValue ) );

		static int val; // must be valid for the async call
		val = g_GameModeAsNumberContexts->ScanValues( szValue );
		SetAllUsersProperty( PROPERTY_CSS_GAME_MODE_AS_NUMBER, sizeof( val ), &val );
	}

	// MapGroup being used
	if ( char const *szMapGroupName = pNewSettings->GetString( "game/mapgroupname", NULL ) )
	{
		SetAllUsersContext( CONTEXT_CSS_MAP_GROUP, g_MapGroupContexts->ScanValues( szMapGroupName ) );
	}

	// Privacy
	if ( char const *szPrivacy = pNewSettings->GetString( "system/access", NULL ) )
	{
		SetAllUsersContext( CONTEXT_CSS_PRIVACY, g_PrivacyContexts->ScanValues( szPrivacy ) );
	}

	// Listen server
	if ( char const *szListenServer = pNewSettings->GetString( "server/server", NULL ) )
	{
		static int val; // must be valid for the async call
		val = ( !V_stricmp( "listen", szListenServer ) ? 1 : 0 );
		extern ConVar mm_title_debug_dccheck;
		if ( mm_title_debug_dccheck.GetInt() )
			val = ( ( mm_title_debug_dccheck.GetInt() > 0 ) ? 0 : 1 );
		SetAllUsersProperty( PROPERTY_CSS_SEARCH_LISTEN_SERVER, sizeof( val ), &val );
	}

	//
	// Determine Rich Presence Display
	//
	if ( char const *szGameModeForRichPresence = pFullSettings->GetString( "game/mode", NULL ) )
	{
		// Online/Offline
		if ( char const *szNetwork = pFullSettings->GetString( "system/network", NULL ) )
		{
			DWORD dwLevelPresence = CONTEXT_PRESENCE_MAINMENU;

			if ( V_stricmp( "offline", szNetwork ) == 0 )
			{
				dwLevelPresence = CONTEXT_PRESENCE_SINGLEPLAYER;
			}
			else if ( !V_stricmp( "lobby", pFullSettings->GetString( "game/state" ) ) )
			{
				dwLevelPresence = CONTEXT_PRESENCE_LOBBY;
			}
			else
			{
				// Privacy
				if ( char const *szPrivacy = pFullSettings->GetString( "system/access", NULL ) )
				{
					DWORD dwPrivacy = g_PrivacyContexts->ScanValues( szPrivacy );
					if ( dwPrivacy == CONTEXT_CSS_PRIVACY_PUBLIC )
					{
						// Public match

						// See if there are any free slots
						int numSlots = pFullSettings->GetInt( "members/numSlots", 0 );
						int numPlayers = pFullSettings->GetInt( "members/numPlayers", 0 );
				
						dwLevelPresence = (numSlots > numPlayers) ? CONTEXT_PRESENCE_MULTIPLAYER : CONTEXT_PRESENCE_MULTIPLAYER_NO_SLOTS;
					}
					else
					{
						// Private/invite only match
						dwLevelPresence = CONTEXT_PRESENCE_MULTIPLAYER_PRIVATE;
					}
				}
			}

			SetAllUsersContext( X_CONTEXT_PRESENCE, dwLevelPresence );
		}
				
		// Update the map being used
		DWORD dwMapRichPresence = pFullSettings->GetInt( "game/mapRichPresence", 0xFFFF );
		if ( dwMapRichPresence == 0xFFFF )
		{
			// We didn't have a richpresence context set in GameModes.txt so look it up based on the name
			if ( char const *szMapName = pFullSettings->GetString( "game/map", NULL ) )
			{
				dwMapRichPresence = g_LevelContexts->ScanValues( szMapName );
			}
		}
		if ( dwMapRichPresence != 0xFFFF )
		{
			SetAllUsersContext( CONTEXT_CSS_LEVEL, dwMapRichPresence );
		}
	}
#endif // _X360
}

void MM_Title_RichPresence_PlayersChanged( KeyValues *pFullSettings )
{
//#ifdef _X360
//	// Set the installed DLCs masks
//	static int val[10]; // must be valid for the async call
//	uint64 uiDlcInstalled = g_pMatchFramework->GetMatchSystem()->GetDlcManager()->GetDataInfo()->GetUint64( "@info/installed" );
//	extern ConVar mm_matchmaking_dlcsquery;
//	for ( int k = 1; k <= mm_matchmaking_dlcsquery.GetInt(); ++ k )
//	{
//		val[k] = !!( uiDlcInstalled & ( 1ull << k ) );
//		DevMsg( "DLC%d installed: %d\n", k, val[k] );
//		SetAllUsersProperty( PROPERTY_INSTALLED_DLC1 - 1 + k, sizeof( val[k] ), &val[k] );
//	}
//#endif
}

// Called by the client to notify matchmaking that it should update matchmaking properties based
// on player distribution among the teams.
void MM_Title_RichPresence_UpdateTeamPropertiesCSGO( KeyValues *pCurrentSettings, KeyValues *pTeamProperties )
{
#ifdef _X360
	int numSlots = pCurrentSettings->GetInt( "members/numSlots", 0 );
	int numPlayers = pTeamProperties->GetInt( "members/numPlayers", 0 );
	int numSpectators = pTeamProperties->GetInt( "members/numSpectators", 0 );
	int numExtraSpectatorSlots = pCurrentSettings->GetInt( "members/numExtraSpectatorSlots", 0 );
	int numFreeTSlots = pCurrentSettings->GetInt( "members/numTSlotsFree", 0 );
	int numFreeCTSlots = pCurrentSettings->GetInt( "members/numCTSlotsFree", 0 );

	// Spectator overflow is computed in case we end up in a situation in which there are more
	// spectators than spectator slots.  In that case, the extra spectators are counted against
	// the active player slots.
	int spectatorOverflow = numSpectators - numExtraSpectatorSlots;

	
	static int nPROPERTY_CSS_OPEN_SLOTS;
	nPROPERTY_CSS_OPEN_SLOTS = numSlots - numPlayers;
	if ( spectatorOverflow > 0 )
	{
		nPROPERTY_CSS_OPEN_SLOTS = numSlots - ( numPlayers - numExtraSpectatorSlots + spectatorOverflow );
	}
	SetAllUsersProperty( PROPERTY_CSS_OPEN_SLOTS, sizeof( nPROPERTY_CSS_OPEN_SLOTS ), &nPROPERTY_CSS_OPEN_SLOTS );



	// post the average skill rank so matchmaking will work
	int avgRank = pTeamProperties->GetInt( "members/timeout", DEFAULT_NEW_PLAYER_ELO_RANK );
	static int nPROPERTY_CSS_AGGREGATE_SKILL0;
	nPROPERTY_CSS_AGGREGATE_SKILL0 = avgRank;
	SetAllUsersProperty( PROPERTY_CSS_AGGREGATE_SKILL0, sizeof( nPROPERTY_CSS_AGGREGATE_SKILL0 ), &nPROPERTY_CSS_AGGREGATE_SKILL0 );


	// Post the maximum number of free slots on either team, for team matchmaking only
	static int nPROPERTY_CSS_MAX_OPEN_TEAM_SLOTS;
	nPROPERTY_CSS_MAX_OPEN_TEAM_SLOTS = max( numFreeTSlots, numFreeCTSlots );
	SetAllUsersProperty( PROPERTY_CSS_MAX_OPEN_TEAM_SLOTS, sizeof( nPROPERTY_CSS_MAX_OPEN_TEAM_SLOTS ), &nPROPERTY_CSS_MAX_OPEN_TEAM_SLOTS );

#elif defined( _PS3 )
	// This is hacky: we stuff the rank into system data so that if lobby migrates
	// to a new owner and the metadata wasn't correctly set by the previous owner
	// then the new owner will set it
	if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
	{
		KeyValues *kvSystemData = pMatchSession->GetSessionSystemData();
		if ( !kvSystemData )
			return;

		int avgRank = pTeamProperties->GetInt( "members/timeout", DEFAULT_NEW_PLAYER_ELO_RANK );
		kvSystemData->SetInt( "timeout", avgRank );

		int numOpenSlots = 10 - pTeamProperties->GetInt( "members/numPlayers" );
		if ( numOpenSlots < 0 )
			numOpenSlots = 0;

		kvSystemData->SetInt( "numOpenSlots", numOpenSlots );

		char const *szSessionType = kvSystemData->GetString( "type", NULL );
		
		DevMsg( "Session timeout value=%d (%s)\n", avgRank, szSessionType ? szSessionType : "offline" );
		DevMsg( "Session numOpenSlots=%d (%s)\n", numOpenSlots, szSessionType ? szSessionType : "offline" );
		if ( szSessionType && !Q_stricmp( szSessionType, "client" ) )
			return;	// don't run on clients for now, maybe later when we become owner

		steamapicontext->SteamMatchmaking()->SetLobbyData( kvSystemData->GetUint64( "xuidReserve", 0ull ), "game:timeout", CFmtStr( "%u", avgRank ) );
		steamapicontext->SteamMatchmaking()->SetLobbyData( kvSystemData->GetUint64( "xuidReserve", 0ull ), "game:numOpenSlots", CFmtStr( "%u", numOpenSlots ) );
	}
#endif
}

