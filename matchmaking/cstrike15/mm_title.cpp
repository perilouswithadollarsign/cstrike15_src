//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_title.h"
#include "mm_title_richpresence.h"
#include "csgo.spa.h"

#ifdef _PS3
#include <netex/net.h>
#include <netex/libnetctl.h>
#endif

#include "fmtstr.h"
#include "gametypes/igametypes.h"
#include "netmessages_signon.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IGameTypes *g_pGameTypes;


CMatchTitle::CMatchTitle()
{
}

CMatchTitle::~CMatchTitle()
{
}


//
// Init / shutdown
//

InitReturnVal_t CMatchTitle::Init()
{
	if ( IGameEventManager2 *mgr = g_pMatchExtensions->GetIGameEventManager2() )
	{
		mgr->AddListener( this, "server_pre_shutdown", false );
		mgr->AddListener( this, "game_newmap", false );
		mgr->AddListener( this, "finale_start", false );
		mgr->AddListener( this, "round_start", false );
		mgr->AddListener( this, "round_end", false );
		mgr->AddListener( this, "difficulty_changed", false );
		mgr->AddListener( this, "player_connect", false );
		mgr->AddListener( this, "player_disconnect", false );
	}

#ifdef _GAMECONSOLE
	// Initialize Title Update version
	extern ConVar mm_tu_string;
	mm_tu_string.SetValue( "00000000" );
#endif

	g_pGameTypes->Initialize();

	return INIT_OK;
}

void CMatchTitle::Shutdown()
{
	if ( IGameEventManager2 *mgr = g_pMatchExtensions->GetIGameEventManager2() )
	{
		mgr->RemoveListener( this );
	}
}


//
// Implementation
//

uint64 CMatchTitle::GetTitleID()
{
#ifdef _X360
	return TITLEID_COUNTER_STRIKE__GO;
#elif !defined( SWDS ) && !defined( NO_STEAM )
	static uint64 uiAppID = 0ull;
	if ( !uiAppID && steamapicontext && steamapicontext->SteamUtils() )
	{
		uiAppID = steamapicontext->SteamUtils()->GetAppID();
	}
	return uiAppID;
#else
	return 0ull;
#endif
}

uint64 CMatchTitle::GetTitleServiceID()
{
#ifdef _X360
	return 0x45410880ull; // Left 4 Dead 1 Service ID
#else
	return 0ull;
#endif
}

#ifdef _PS3

uint64 CMatchTitle::GetTitleSettingsFlags()
{
	return MATCHTITLE_SETTING_MULTIPLAYER
		| MATCHTITLE_VOICE_INGAME
		| MATCHTITLE_FORCE_PSN_NAMES
		| MATCHTITLE_PLAYERMGR_DISABLED
		| MATCHTITLE_SERVERMGR_DISABLED;
}

#else

uint64 CMatchTitle::GetTitleSettingsFlags()
{
	return MATCHTITLE_SETTING_MULTIPLAYER
		| MATCHTITLE_VOICE_INGAME
		| MATCHTITLE_PLAYERMGR_ALLFRIENDS
#if !defined( CSTRIKE15 )
		| MATCHTITLE_SETTING_NODEDICATED
		| MATCHTITLE_PLAYERMGR_DISABLED
		| MATCHTITLE_SERVERMGR_DISABLED
		| MATCHTITLE_INVITE_ONLY_SINGLE_USER
#else
		| MATCHTITLE_PLAYERMGR_FRIENDREQS
#endif // !CSTRIKE15
	;
}

#endif

#ifdef _PS3
void *g_pMatchTitle_NetMemory;
#endif

void CMatchTitle::PrepareNetStartupParams( void *pNetStartupParams )
{
#ifdef _X360
	XNetStartupParams &xnsp = *( XNetStartupParams * ) pNetStartupParams;

	xnsp.cfgQosDataLimitDiv4 = 64; // 256 bytes

	// Increase outstanding QoS responses significantly
	// See: https://forums.xboxlive.com/AnswerPage.aspx?qid=493b207b-66b9-42bc-b23d-ddc306e09749&tgt=1
	xnsp.cfgQosSrvMaxSimultaneousResponses = 255;

	xnsp.cfgSockDefaultRecvBufsizeInK = 64; // Increase receive size for UDP to 64k
	xnsp.cfgSockDefaultSendBufsizeInK = 64; // Keep send size at 64k too

	int numGamePlayersMax = GetTotalNumPlayersSupported();

	int numConnections = 4 * ( numGamePlayersMax - 1 );
	//   - the max number of connections to members of your game party
	//   - the max number of connections to members of your social party
	//   - the max number of connections to a pending game party (if you are joining a new one ).
	//   - matchmakings client info structure also creates a connection per client for the lobby.

	//   1 - the main game session
	int numTotalConnections = 1 + numConnections;

	//   29 - total Connections (XNADDR/XNKID pairs) ,using 5 sessions (XNKID/XNKEY pairs).

	xnsp.cfgKeyRegMax = 16; //adding some extra room because of lazy dealocation of these pairs.
	xnsp.cfgSecRegMax = MAX( 64, numTotalConnections ); //adding some extra room because of lazy dealocation of these pairs.
	
	xnsp.cfgSockMaxDgramSockets = xnsp.cfgSecRegMax;
	xnsp.cfgSockMaxStreamSockets = xnsp.cfgSecRegMax;
#endif

#if defined( _PS3 ) && defined( NO_STEAM )
	MEM_ALLOC_CREDIT_( "NO_STEAM: CMatchTitle::PrepareNetStartupParams" );
	sys_net_initialize_parameter_t &snip = *( sys_net_initialize_parameter_t * ) pNetStartupParams;

	snip.memory_size = 512 * 1024;
	snip.memory = malloc( snip.memory_size ); // alternatively this can be a global array

	g_pMatchTitle_NetMemory = snip.memory;	// bookmark the memory address for later inspection if necessary
#endif
}

int CMatchTitle::GetTotalNumPlayersSupported()
{
#ifdef _GAMECONSOLE
	//  [jason] Rounding this up to 16, since this was the value used on cstrike15 xbla.  Among other things, this
	//		controls how many remote talker voice channels are reserved for each client in XHV
	return 16;
#else
	return 64; // On PC this is not limited, return max number dedicated servers can ever run with
#endif
}

// Get a guest player name
char const * CMatchTitle::GetGuestPlayerName( int iUserIndex )
{
	if ( vgui::ILocalize *pLocalize = g_pMatchExtensions->GetILocalize() )
	{
		if ( wchar_t* wStringTableEntry = pLocalize->Find( "#SFUI_LocalPlayer" ) )
		{
			static char szName[ MAX_PLAYER_NAME_LENGTH ] = {0};
			pLocalize->ConvertUnicodeToANSI( wStringTableEntry, szName, ARRAYSIZE( szName ) );
			return szName;
		}
	}
	
	return "";
}

// Sets up all necessary client-side convars and user info before
// connecting to server
void CMatchTitle::PrepareClientForConnect( KeyValues *pSettings )
{
#ifndef SWDS
	int numPlayers = 1;
#ifdef _GAMECONSOLE
	numPlayers = XBX_GetNumGameUsers();
#endif

	//
	// Now we set the convars
	//

	for ( int k = 0; k < numPlayers; ++ k )
	{
		int iController = k;
#ifdef _GAMECONSOLE
		iController = XBX_GetUserId( k );
#endif
		IPlayerLocal *pPlayerLocal = g_pPlayerManager->GetLocalPlayer( iController );
		if ( !pPlayerLocal )
			continue;

		// Set "name"
		static SplitScreenConVarRef s_cl_name( "name" );
		char const *szName = pPlayerLocal->GetName();
		s_cl_name.SetValue( k, szName );

		// Set "networkid_force"
		if ( IsX360() )
		{
			static SplitScreenConVarRef s_networkid_force( "networkid_force" );
			uint64 xid = pPlayerLocal->GetXUID();
			s_networkid_force.SetValue( k, CFmtStr( "%08X:%08X:", uint32( xid >> 32 ), uint32( xid ) ) );
		}
	}
#endif
}

bool CMatchTitle::StartServerMap( KeyValues *pSettings )
{
	int numPlayers = 1;
#ifdef _GAMECONSOLE
	numPlayers = XBX_GetNumGameUsers();
#endif

	char const *szMap = pSettings->GetString( "game/map", NULL );
	if ( !szMap )
		return false;

	// Check that we have the server interface and that the map is valid
	if ( !g_pMatchExtensions->GetIVEngineServer() )
		return false;
	if ( !g_pMatchExtensions->GetIVEngineServer()->IsMapValid( szMap ) )
		return false;

	//
	// Prepare game dll reservation package
	//
	KeyValues *pGameDllReserve = g_pMatchFramework->GetMatchNetworkMsgController()->PackageGameDetailsForReservation( pSettings );
	KeyValues::AutoDelete autodelete( pGameDllReserve );

	pGameDllReserve->SetString( "map/mapcommand", ( numPlayers <= 1 ) ? "map" : "ss_map" );

	if ( !Q_stricmp( "commentary", pSettings->GetString( "options/play", "" ) ) )
		pGameDllReserve->SetString( "map/mapcommand", "map_commentary" );

	// Run map based off the faked reservation packet
	g_pMatchExtensions->GetIVEngineClient()->StartLoadingScreenForKeyValues( pGameDllReserve );

	return true;
}

static KeyValues * GetCurrentMatchSessionSettings()
{
	IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
	return pIMatchSession ? pIMatchSession->GetSessionSettings() : NULL;
}

//
// <Vitaliy> July-2012
// CS:GO hack: training map has been implemented storing progress in convars instead of
// properly storing it in game profile data, force storing the convars into profile
// when the training session finishes here
// Please, do not use this approach in future code.
//
bool g_bPlayingTrainingMap = false;

void CMatchTitle::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();
	
	if ( !Q_stricmp( "OnPlayerRemoved", szEvent ) ||
		 !Q_stricmp( "OnPlayerUpdated", szEvent ) )
	{
		MM_Title_RichPresence_Update( GetCurrentMatchSessionSettings(), NULL );
	}
	else if ( !Q_stricmp( "OnPlayerMachinesConnected", szEvent ) ||
		!Q_stricmp( "OnPlayerMachinesDisconnected", szEvent ) )
	{
		// Player counts changed on host, update aggregate fields
		IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
		if ( !pMatchSession )
			return;
		KeyValues *kvPackage = new KeyValues( "Update" );
		if ( KeyValues *kvUpdate = kvPackage->FindKey( "update", true ) )
		{
			extern void UpdateAggregateMembersSettings( KeyValues *pFullGameSettings, KeyValues *pUpdate );
			UpdateAggregateMembersSettings( pMatchSession->GetSessionSettings(), kvUpdate );
		}
		pMatchSession->UpdateSessionSettings( KeyValues::AutoDeleteInline( kvPackage ) );
	}
	else if ( !Q_stricmp( "OnMatchSessionUpdate", szEvent ) )
	{
		if ( !Q_stricmp( pEvent->GetString( "state" ), "updated" ) )
		{
			if ( KeyValues *kvUpdate = pEvent->FindKey( "update" ) )
			{
				MM_Title_RichPresence_Update( GetCurrentMatchSessionSettings(), kvUpdate );
			}
		}
		else if ( !Q_stricmp( pEvent->GetString( "state" ), "created" ) ||
				  !Q_stricmp( pEvent->GetString( "state" ), "ready" ) )
		{
			MM_Title_RichPresence_Update( GetCurrentMatchSessionSettings(), NULL );
			if ( IMatchSession *pSession = g_pMatchFramework->GetMatchSession() )
			{
				if ( !Q_stricmp( "training", pSession->GetSessionSettings()->GetString( "game/type" ) ) )
					g_bPlayingTrainingMap = true;
			}
		}
		else if ( !Q_stricmp( pEvent->GetString( "state" ), "closed" ) )
		{
			if ( g_bPlayingTrainingMap )
			{
				g_bPlayingTrainingMap = false;
				g_pMatchExtensions->GetIVEngineClient()->ClientCmd_Unrestricted( CFmtStr( "host_writeconfig_ss %d", XBX_GetPrimaryUserId() ) );
			}
			MM_Title_RichPresence_Update( NULL, NULL );
		}
	}
	else if ( !Q_stricmp( szEvent, "Client::CmdKeyValues" ) )
	{
		KeyValues *pCmd = pEvent->GetFirstTrueSubKey();
		if ( !pCmd )
			return;
		char const *szCmd = pCmd->GetName();
		if ( !Q_stricmp( "ExtendedServerInfo", szCmd ) )
		{
			KeyValuesDumpAsDevMsg( pCmd, 2, 1 );
			g_pGameTypes->SetAndParseExtendedServerInfo( pCmd );
		}
	}
	else if ( !Q_stricmp( "OnEngineClientSignonStateChange", szEvent ) )
	{
		int iOldState = pEvent->GetInt( "old", 0 );
		int iNewState = pEvent->GetInt( "new", 0 );

		if (
			( iOldState >= SIGNONSTATE_CONNECTED &&	// disconnect
			iNewState < SIGNONSTATE_CONNECTED ) ||
			( iOldState < SIGNONSTATE_FULL &&	// full connect
			iNewState >= SIGNONSTATE_FULL )
			)
		{
			MM_Title_RichPresence_Update( NULL, NULL );
		}
	}
	else if ( !Q_stricmp( "OnEngineDisconnectReason", szEvent ) )
	{
		MM_Title_RichPresence_Update( NULL, NULL );
	}
	else if ( !Q_stricmp( "OnEngineEndGame", szEvent ) )
	{
		MM_Title_RichPresence_Update( NULL, NULL );
	}
}

//
//
//

int CMatchTitle::GetEventDebugID( void )
{
	return EVENT_DEBUG_ID_INIT;
}

void CMatchTitle::FireGameEvent( IGameEvent *pIGameEvent )
{
	// Parse the game event
	char const *szGameEvent = pIGameEvent->GetName();
	if ( !szGameEvent || !*szGameEvent )
		return;

	if ( !Q_stricmp( "round_start", szGameEvent ) ||
		!Q_stricmp( "round_end", szGameEvent ) ||
		!Q_stricmp( "game_newmap", szGameEvent ) ||
		!Q_stricmp( "player_connect", szGameEvent ) ||
		!Q_stricmp( "player_disconnect", szGameEvent ) )
	{	// Update rich presence
		MM_Title_RichPresence_Update( NULL, NULL );
	}

	// Check if the current match session is on an active game server
	IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
	if ( !pMatchSession )
		return;
	KeyValues *pSessionSettings = pMatchSession->GetSessionSettings();
	char const *szGameServer = pSessionSettings->GetString( "server/server", "" );
	char const *szSystemLock = pSessionSettings->GetString( "system/lock", "" );
	if ( ( !szGameServer || !*szGameServer ) &&
		( !szSystemLock || !*szSystemLock ) )
		return;

	// Also don't run on the client when there's a host
	char const *szSessionType = pMatchSession->GetSessionSystemData()->GetString( "type", NULL );
	if ( szSessionType && !Q_stricmp( szSessionType, "client" ) )
		return;

	if ( !Q_stricmp( "round_start", szGameEvent ) )
	{
		KeyValues *kvUpdate = KeyValues::FromString(
			"update",
			" update { "
				" game { "
					" state game "
				" } "
			" } "
			);
		KeyValues::AutoDelete autodelete( kvUpdate );

		pMatchSession->UpdateSessionSettings( kvUpdate );
	}
	else if ( !Q_stricmp( "round_end", szGameEvent ) )
	{
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnProfilesWriteOpportunity", "reason", "checkpoint"
			) );
	}
	else if ( !Q_stricmp( "finale_start", szGameEvent ) )
	{
		pMatchSession->UpdateSessionSettings( KeyValues::AutoDeleteInline( KeyValues::FromString(
			"update",
			" update { "
				" game { "
					" state finale "
				" } "
			" } "
			) ) );
	}
	else if ( !Q_stricmp( "game_newmap", szGameEvent ) )
	{
		const char *szMapName = pIGameEvent->GetString( "mapname", "" );

		KeyValues *kvUpdate = KeyValues::FromString(
			"update",
			" update { "
				" game { "
					" state game "
				" } "
			" } "
			);
		KeyValues::AutoDelete autodelete( kvUpdate );

		Assert( szMapName && *szMapName );
		if ( szMapName && *szMapName )
		{
			kvUpdate->SetString( "update/game/map", szMapName );

			char const *szWorkshopMap = g_pMatchExtensions->GetIVEngineClient()->GetLevelNameShort();
			if ( StringHasPrefix( szWorkshopMap, "workshop" ) )
			{
				size_t nLenMapName = Q_strlen( szMapName );
				size_t nShortMapName = Q_strlen( szWorkshopMap );
				if ( ( nShortMapName >= nLenMapName ) &&
					!Q_stricmp( szWorkshopMap + nShortMapName - nLenMapName, szMapName ) )
					// Use the name of the workshop map
					kvUpdate->SetString( "update/game/map", szWorkshopMap );
			}
		}

		pMatchSession->UpdateSessionSettings( kvUpdate );

		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnProfilesWriteOpportunity", "reason", "checkpoint"
			) );
	}
	else if ( !Q_stricmp( "server_pre_shutdown", szGameEvent ) )
	{
		char const *szReason = pIGameEvent->GetString( "reason", "quit" );
		if ( !Q_stricmp( szReason, "quit" ) )
		{
			DevMsg( "Received server_pre_shutdown notification - server is shutting down...\n" );

			// Transform the server shutdown event into game end event
			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
				"OnEngineDisconnectReason", "reason", "Server shutting down"
				) );
		}
	}
}

