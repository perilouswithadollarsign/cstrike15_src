//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

#include "cs_app_lifetime_gamestats.h"
#include "steamworks_gamestats.h"
#include "steam/isteamutils.h"
#include "csgo_serialnumbersequence.h"

#include "gametypes.h"
#include "matchmaking/imatchframework.h"

#include "cs_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IGameTypes *g_pGameTypes;

ConVar cl_debug_client_gamestats( "cl_debug_client_gamestats", "1" );

static CS_App_Lifetime_Gamestats g_CSAppLifetimeGamestats;
CS_App_Lifetime_Gamestats* CSAppLifetimeGameStats() { return &g_CSAppLifetimeGamestats; }

uint16 CS_App_Lifetime_Gamestats::m_unEventCount = 0;

const char* g_szValidUIEvents [] =
{
	"ServerBrowserLAN",
	"ServerBrowserFriends",
	"ServerBrowserJoinFriend",
	"ServerBrowserUnknownJoinType",
	"ServerBrowserFavorites",
	"ServerBrowserHistory",
	"ServerBrowserSpectator",
	"ServerBrowserInternet",
	"ServerBrowserUnknownPageType",
	"ServerBrowserJoinByIP",
	"HLTVConnectRelay",
	"CommunityQuickPlay",
	"ConnectStringOnCommandline",
	"PurchaseEventRequired",
	"PurchaseEventMinutesPlayed",
	"PurchaseEventNoMinutesPlayed",
	"PurchaseEventStash",
	"InventoryEquipMainMenu",
	"InventoryEquipConnectedActive",
	"InventoryEquipConnectedInactive",
	"CrateUnlockMainMenu",
	"CrateUnlockConnected",
	"WeaponInspectMainMenuDefault",
	"WeaponInspectMainMenuPainted",
	"WeaponInspectConnectedDefault",
	"WeaponInspectConnectedPainted",
	"WorkshopWeaponPreview",
	"MatchDownloadAttempt",
	"WatchLiveMatch",
	"WatchDownloadedMatch",
	"WatchGOTVTheater",
	"WatchTwitchStream",
	"WeaponInspectExternalItem",
	"ViewedOffers",
	"BorrowedMusicKit",
	"ViewOnMarket",
	"InsufficientLevel",
	"PartyBrowserRefreshEmpty",
	"PartyBrowserRefreshResults",
	"PartyBrowserJoin",
	"FriendJoin",
	"SoloSearch",
	"WatchEmbeddedStreamExternally",
	"PartyBrowserJoinNearby",
	"SwitchedToHRTFEnabled",
	"SwitchedToHRTFDisabled",
	"PartyBrowserJoinInvited",
};

ConVar steamworks_sessionid_lifetime_client( "steamworks_sessionid_lifetime_client", "0", FCVAR_HIDDEN, "The full client session ID for the new steamworks gamestats." );


CS_App_Lifetime_Gamestats::CS_App_Lifetime_Gamestats() : 
	BaseClass( "CS_App_Lifetime_Gamestats", "steamworks_sessionid_lifetime_client" ),
	m_GameJoinRequested( this, &CS_App_Lifetime_Gamestats::OnGameJoinRequested )
{
}


bool CS_App_Lifetime_Gamestats::Init()
{
	if ( !BaseClass::Init() )
		return false;

	StartSession();

	return true;
}

void CS_App_Lifetime_Gamestats::Shutdown()
{
	WriteStats();
	EndSession();
}

void CS_App_Lifetime_Gamestats::WriteStats( void )
{
	// Refresh the interface in case steam has unloaded
	m_SteamWorksInterface = GetInterface();

	if ( !m_SteamWorksInterface )
	{
		DevMsg( "WARNING: Attempted to send a steamworks gamestats row when the steamworks interface was not available!" );
	}
	else
	{
		WriteUIEvents();
		WriteGameJoins();
	}
}

EResult CS_App_Lifetime_Gamestats::WriteUIEvents()
{
	FOR_EACH_VEC( m_vecUIEvents, i )
	{
		uint64 iTableID = 0;
		m_SteamWorksInterface->AddNewRow( &iTableID, m_SessionID, "CSGOUIEvent" );
		if ( !iTableID )
			return k_EResultFail;

		UIEvent_t *pData = &m_vecUIEvents[i];

		WriteStringToTable( pData->strEventID.Access(), iTableID, "EventID" );
		WriteIntToTable( pData->unCount, iTableID, "EventCount" );
		WriteIntToTable( pData->unTime, iTableID, "TimeSubmitted" );

		if ( cl_debug_client_gamestats.GetBool() )
		{
			Msg( "Steamworks gamestats: %s adding UI data %s id=%d time=%d\n", Name(), pData->strEventID.Get(), pData->unCount, pData->unTime );
		}

		EResult res = m_SteamWorksInterface->CommitRow( iTableID );
		if ( res != k_EResultOK )
		{
			char pzMessage[MAX_PATH] = {0};
			V_snprintf( pzMessage, ARRAYSIZE(pzMessage), "Failed To Submit table %s", "CSUIEvent" );
			Assert( pzMessage );
			return res;
		}
	}
	m_vecUIEvents.RemoveAll();
	return k_EResultOK;
}

EResult CS_App_Lifetime_Gamestats::WriteGameJoins()
{
	FOR_EACH_VEC( m_vecGameJoins, i )
	{
		uint64 iTableID = 0;
		m_SteamWorksInterface->AddNewRow( &iTableID, m_SessionID, "CSGOGameJoin" );
		if ( !iTableID )
			return k_EResultFail;

		uint64 ullGameSessionID = m_vecGameJoins[i].ullSessionID;
		uint16 unGameType = (uint16)m_vecGameJoins[i].unGameType;
		uint16 unGameMode = (uint16)m_vecGameJoins[i].unGameMode;
		bool bValveOfficial = m_vecGameJoins[i].bIsValveOfficial;

		WriteInt64ToTable( ullGameSessionID, iTableID, "GameSessionID" );
		WriteIntToTable( unGameType, iTableID, "GameTypeID" );
		WriteIntToTable( unGameMode, iTableID, "GameModeID" );
		WriteIntToTable( bValveOfficial, iTableID, "IsValveOfficial" );
		
		if ( cl_debug_client_gamestats.GetBool() )
		{
			Msg( "Steamworks gamestats: %s adding game session %llu", Name(), ullGameSessionID ); 
		}

		EResult res = m_SteamWorksInterface->CommitRow( iTableID );
		if ( res != k_EResultOK )
		{
			char pzMessage[MAX_PATH] = {0};
			V_snprintf( pzMessage, ARRAYSIZE(pzMessage), "Failed To Submit table %s", "CSGOGameJoin" );
			Assert( pzMessage );
			return res;
		}
	}
	m_vecGameJoins.RemoveAll();
	return k_EResultOK;
}

static bool BHelperCheckSafeUserCmdString( char const *ipconnect )
{
	bool bConnectValid = true;
	while ( *ipconnect )
	{
		if ( ( ( *ipconnect >= '0' ) && ( *ipconnect <= '9' ) ) ||
			( ( *ipconnect >= 'a' ) && ( *ipconnect <= 'z' ) ) ||
			( ( *ipconnect >= 'A' ) && ( *ipconnect <= 'Z' ) ) ||
			( *ipconnect == '_' ) || ( *ipconnect == '-' ) || ( *ipconnect == '.' ) ||
			( *ipconnect == ':' ) || ( *ipconnect == '?' ) || ( *ipconnect == '%' ) ||
			( *ipconnect == '/' ) || ( *ipconnect == '=' ) || ( *ipconnect == ' ' ) ||
			( *ipconnect == '[' ) || ( *ipconnect == ']' ) || ( *ipconnect == '@' ) ||
			( *ipconnect == '"' ) || ( *ipconnect == '\'' ) || ( *ipconnect == '#' ) ||
			( *ipconnect == '(' ) || ( *ipconnect == ')' ) ||  ( *ipconnect == '!' ) ||
			( *ipconnect == '\\' ) || ( *ipconnect == '$' )
			)
			++ipconnect;
		else
		{
			bConnectValid = false;
			break;
		}
	}
	return bConnectValid;
}

void CS_App_Lifetime_Gamestats::OnGameJoinRequested( GameRichPresenceJoinRequested_t *pCallback )
{
	/* Removed for partner depot */
}

void CS_App_Lifetime_Gamestats::RecordUIEvent( const char* szEventName )
{
	if ( m_unEventCount == (uint16)-1 )
		return; 

	bool bFound = false;
	for ( int i = 0; i < ARRAYSIZE( g_szValidUIEvents ); ++i )
	{
		if ( V_strcmp( g_szValidUIEvents[i], szEventName) == 0 )	
		{
			bFound = true;
			break;
		}
	}

	// Only record whitelisted events
	if ( !bFound )
	{
		return;
	}

	Assert( V_strlen( szEventName ) < UI_EVENT_NAME_MAX );

	uint16 idx = m_vecUIEvents.AddToTail();
	m_vecUIEvents[idx].unCount = ++m_unEventCount;
	CRTime now;
	m_vecUIEvents[idx].unTime = now.GetRTime32();
	m_vecUIEvents[idx].strEventID.Set( szEventName );
}

void CS_App_Lifetime_Gamestats::AddSessionIDsToTable( int iTableID ) 
{
	// Our client side session.
	WriteInt64ToTable( m_SessionID, iTableID, "SessionID" );
}

void CS_App_Lifetime_Gamestats::RecordGameJoin( uint64 ullSessionID )
{
	GameJoins_t gameJoinInfo;
	gameJoinInfo.ullSessionID = ullSessionID;
	gameJoinInfo.unGameMode	 = g_pGameTypes->GetCurrentGameMode();
	gameJoinInfo.unGameType	 = g_pGameTypes->GetCurrentGameType();
	gameJoinInfo.bIsValveOfficial = CSGameRules() && CSGameRules()->IsValveDS();

	if ( m_vecGameJoins.Find( gameJoinInfo ) == m_vecGameJoins.InvalidIndex() )
	{
		m_vecGameJoins.AddToTail( gameJoinInfo );
	}
	else
	{
		Assert( 0 ); // Attempting double add-- would cause PK violation. 
	}
}
