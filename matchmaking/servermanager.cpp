//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef _X360
#include "xbox/xboxstubs.h"
#endif

#include "mm_framework.h"

#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
#include "steam/matchmakingtypes.h"
#endif

#include "proto_oob.h"
#include "fmtstr.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#pragma warning (disable : 4355 )

size_t const mm_filter_max_size = 128; // Maximum size of a filter key, it's key+value+3

static ConVar mm_server_search_update_interval( "mm_server_search_update_interval", "60", FCVAR_DEVELOPMENTONLY, "Interval between servers updates." );

static ConVar mm_server_search_inet_ping_interval( "mm_server_search_inet_ping_interval", "1.0", FCVAR_DEVELOPMENTONLY, "How long to wait between pinging internet server details." );
static ConVar mm_server_search_inet_ping_timeout( "mm_server_search_inet_ping_timeout", "3.0", FCVAR_DEVELOPMENTONLY, "How long to wait for internet server details." );
static ConVar mm_server_search_inet_ping_window( "mm_server_search_inet_ping_window", "10", FCVAR_DEVELOPMENTONLY, "How many servers can be pinged for server details in a batch." );
static ConVar mm_server_search_inet_ping_refresh( "mm_server_search_inet_ping_refresh", "15", FCVAR_DEVELOPMENTONLY, "How often to refresh a listed server." );
static ConVar mm_server_search_server_lifetime( "mm_server_search_server_lifetime", "180", FCVAR_DEVELOPMENTONLY, "How long until a server is no longer returned by the master till we remove it." );

static ConVar mm_server_search_lan_ping_interval( "mm_server_search_lan_ping_interval", "0.4", FCVAR_DEVELOPMENTONLY, "Interval between LAN discovery pings." );
static ConVar mm_server_search_lan_ping_duration( "mm_server_search_lan_ping_duration", "1.0", FCVAR_DEVELOPMENTONLY, "Duration of LAN discovery ping phase." );

static ConVar mm_server_search_lan_ports( "mm_server_search_lan_ports",
										 "27015,27016,27017,27018,27019,27020",
										 FCVAR_RELEASE | FCVAR_ARCHIVE,
										 "Ports to scan during LAN games discovery. Also used to discover and correctly connect to dedicated LAN servers behind NATs." );

//
// Server implementation
//

class CServerPinging : public CServer
{
public:
	CServerPinging() : m_flTimeout( 0 ) {}
public:
#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
	gameserveritem_t m_gsi;
#endif
	float m_flTimeout;
};

CServer::CServer() :
	m_flLastRefresh( Plat_FloatTime() ),
	m_xuid( 0ull ),
	m_pGameDetails( NULL )
{
}

CServer::~CServer()
{
	if ( m_pGameDetails )
		m_pGameDetails->deleteThis();
	m_pGameDetails = NULL;
}

XUID CServer::GetOnlineId()
{
	return m_xuid;
}

KeyValues * CServer::GetGameDetails()
{
	return m_pGameDetails;
}

bool CServer::IsJoinable()
{
	return m_pGameDetails != NULL;
}

void CServer::Join()
{
	char const *szConnectString = m_pGameDetails->GetString( "server/connectstring", NULL );
	if ( !szConnectString || !*szConnectString )
		return;

	g_pMatchExtensions->GetIVEngineClient()->ClientCmd( CFmtStr( "connect %s\n", szConnectString ) );
}

//
// Server manager implementation
//

CServerManager::CServerManager()
{
#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
	m_hRequest = NULL;
#endif
	m_bUpdateEnabled = false;
	m_flNextUpdateTime = 0.0f;
	m_flNextServerUpdateTime = 0.0f;
	m_eState = STATE_IDLE;
}

CServerManager::~CServerManager()
{
	m_Servers.PurgeAndDeleteElements();
	m_ServersPinging.PurgeAndDeleteElements();

#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
	if ( m_hRequest )
		steamapicontext->SteamMatchmakingServers()->ReleaseRequest( m_hRequest );
	m_hRequest = NULL;
#endif
}

static CServerManager g_ServerManager;
CServerManager *g_pServerManager = &g_ServerManager;

void CServerManager::EnableServersUpdate( bool bEnable )
{
	if ( bEnable &&
		( g_pMatchFramework->GetMatchTitle()->GetTitleSettingsFlags() & MATCHTITLE_SERVERMGR_DISABLED ) )
		bEnable = false;

	m_bUpdateEnabled = bEnable;
	m_flNextUpdateTime = 0.0f;

	// If enabled the search, we'll pick it up next update
	if ( bEnable )
		return;

	// Otherwise search is being disabled
	m_Servers.PurgeAndDeleteElements();
	m_ServersPinging.PurgeAndDeleteElements();

#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
	if ( m_eState == STATE_FETCHING_SERVERS && m_hRequest )
	{
		steamapicontext->SteamMatchmakingServers()->ReleaseRequest( m_hRequest );
		m_hRequest = NULL;
	}
#endif

	// Will clean up all servers and go IDLE
	OnAllDataFetched();
}

int CServerManager::GetNumServers()
{
	return m_Servers.Count();
}

IMatchServer * CServerManager::GetServerByIndex( int iServerIdx )
{
	return m_Servers.IsValidIndex( iServerIdx ) ? m_Servers[ iServerIdx ] : NULL;
}

IMatchServer * CServerManager::GetServerByOnlineId( XUID xuidServerOnline )
{
	return GetServerRecordByOnlineId( m_Servers, xuidServerOnline );
}

CServer * CServerManager::GetServerRecordByOnlineId( CUtlVector< CServer * > &arr, XUID xuidServerOnline )
{
	for ( int k = 0; k < arr.Count(); ++ k )
	{
		CServer *pServer = arr[ k ];
		if ( pServer && pServer->GetOnlineId() == xuidServerOnline )
			return pServer;
	}
	return NULL;
}

void CServerManager::OnEvent( KeyValues *pEvent )
{
	if ( IsX360() )
		return;

	char const *szName = pEvent->GetName();

	if ( !Q_stricmp( szName, "OnNetLanConnectionlessPacket" ) )
	{
		char const * arrServerProbeKeys[] = { "LanSearchServerPing", "ConnectServerDetailsRequest", "InetSearchServerDetails" };

		for ( int k = 0; k < ARRAYSIZE( arrServerProbeKeys ); ++ k )
		{
			KeyValues *pProbeKey = pEvent->FindKey( arrServerProbeKeys[k] );
			if ( !pProbeKey )
				continue;

			KeyValues *pDetails = g_pMatchFramework->GetMatchNetworkMsgController()->GetActiveServerGameDetails( pEvent );
			KeyValues::AutoDelete autodelete_pDetails( pDetails );
			if ( !pDetails )
				return;

			if ( !pDetails->FindKey( "server" ) )
				return;

			pDetails->FindKey( arrServerProbeKeys[k], true )->MergeFrom( pProbeKey, KeyValues::MERGE_KV_UPDATE );

			g_pConnectionlessLanMgr->SendPacket( pDetails, pEvent->GetString( "from" ), INetSupport::NS_SOCK_SERVER );
			return;
		}

		if ( KeyValues *pGameDetailsServer = pEvent->FindKey( "GameDetailsServer" ) )
		{
			// Incoming data:
			//
			//	System
			//	Game
			//	Server
			//	Members
			//  LanSearchServerPing
			//		timestamp = holds the time packet was sent for ping

			// Server ping
			int nPing = 0;
			if ( float flTimeSent = pGameDetailsServer->GetFloat( "LanSearchServerPing/timestamp" ) )
			{
				float flSeconds = Plat_FloatTime() - flTimeSent;
				nPing = flSeconds * 1000;
				if ( nPing < 0 )
					nPing = 0;
				if ( nPing >= 1000 )
					nPing = 999;
			}
			else if ( uint64 xuidInetPing = pGameDetailsServer->GetUint64( "InetSearchServerDetails/pingxuid" ) )
			{
				CServerPinging *pServerPinging = ( CServerPinging * ) GetServerRecordByOnlineId( m_ServersPinging, xuidInetPing );
				if ( !pServerPinging )
					return;
				
				if ( float flTimeSent = pGameDetailsServer->GetFloat( "InetSearchServerDetails/timestamp" ) )
				{
					float flSeconds = Plat_FloatTime() - flTimeSent;
					nPing = flSeconds * 1000;
					if ( nPing < 0 )
						nPing = 0;
					if ( nPing >= 1000 )
						nPing = 999;
				}
#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
				else
				{
					nPing = pServerPinging->m_gsi.m_nPing;
				}
#endif
				// Remove the server from outstanding pings list
				m_ServersPinging.FindAndFastRemove( pServerPinging );
				delete pServerPinging;
			}
			else if ( char const *szDetailsAdr = pGameDetailsServer->GetString( "ConnectServerDetailsRequest/server" ) )
			{
				g_pMatchExtensions->GetINetSupport()->OnMatchEvent( pEvent );
				return;
			}
			else
				return;

			// Server address
			char const *szAddr = pGameDetailsServer->GetString( "server/adronline", NULL );
			if ( !szAddr || !*szAddr )
				return;

			// Determine server network address
			netadr_t netAddress;
			netAddress.SetType( NA_IP );
			netAddress.SetPort( PORT_SERVER );
			netAddress.SetFromString( szAddr );

			// Check if this is not our local server
			INetSupport::ServerInfo_t si;
			g_pMatchExtensions->GetINetSupport()->GetServerInfo( &si );
			if ( si.m_bActive && ( si.m_netAdr.CompareAdr( netAddress ) || si.m_netAdrOnline.CompareAdr( netAddress ) ) )
				return;

			// Coalesce it into its XUID online id
			XUID xuidOnline = uint64( netAddress.GetIPNetworkByteOrder() ) | ( uint64( netAddress.GetPort() ) << 32ull );

			// Prepare the settings
			KeyValues *pSettings = pGameDetailsServer->MakeCopy();
			pSettings->SetName( "settings" );
			if ( KeyValues *kvLanSearch = pSettings->FindKey( "LanSearch" ) )
			{
				pSettings->RemoveSubKey( kvLanSearch );
				kvLanSearch->deleteThis();
			}
			pSettings->SetInt( "server/ping", nPing );
			
			if ( char const *szPacketFrom = pEvent->GetString( "from", NULL ) )
				pSettings->SetString( "server/connectstring", szPacketFrom );

			//
			// Find the server or create a new one
			//
			IMatchServer *pExistingServer = GetServerByOnlineId( xuidOnline );
			CServer *pServer = ( CServer * ) pExistingServer;
			if ( !pServer )
			{
				pServer = new CServer();
				pServer->m_xuid = xuidOnline;
#if !defined( NO_STEAM ) && !defined( SWDS )
				servernetadr_t serverNetAddr;
				serverNetAddr.Init( netAddress.GetIPHostByteOrder(), netAddress.GetPort(), netAddress.GetPort() );
				pServer->m_netAdr = serverNetAddr;
#endif
				m_Servers.AddToTail( pServer );
			}

			if ( pServer->m_pGameDetails )
			{
				// Average out the ping value
				int nKnownPing = pServer->m_pGameDetails->GetInt( "server/ping", 0 );
				if ( nKnownPing < nPing )
				{
					// we got a high ping, try to display the ping as low as possible
					nPing = ( nKnownPing * 9 + nPing * 1 ) / 10;
					pSettings->SetInt( "server/ping", nPing );
				}

				pServer->m_pGameDetails->deleteThis();
			}

			pServer->m_pGameDetails = pSettings;
			pServer->m_flLastRefresh = Plat_FloatTime();

			// Signal that we have updated a server
			KeyValues *kvEvent = new KeyValues( "OnMatchServerMgrUpdate", "update", "server" );
			kvEvent->SetUint64( "xuid", xuidOnline );
			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( kvEvent );
		}
	}
	else if ( !Q_stricmp( "Client::ResendGameDetailsRequest", szName ) )
	{
		KeyValues *kv = new KeyValues( "ConnectServerDetailsRequest" );
		KeyValues::AutoDelete autodelete( kv );
		kv->SetString( "server", pEvent->GetString( "to", "" ) );

		g_pConnectionlessLanMgr->SendPacket( kv, pEvent->GetString( "to", "" ) );
	}
}

#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
void CServerManager::ServerResponded( HServerListRequest hReq, int iServer )
{
	gameserveritem_t *gsi = steamapicontext->SteamMatchmakingServers()->GetServerDetails( hReq, iServer );
	if ( !gsi )
		return;

	// Every time a server responds during groups query, bump this groups last request timeout
	m_lanSearchData.m_flLastBroadcastTime = Plat_FloatTime();

	// Determine server network address
	netadr_t netAddress;
	netAddress.SetType( NA_IP );
	netAddress.SetPort( PORT_SERVER );
	netAddress.SetFromString( gsi->m_NetAdr.GetConnectionAddressString() );

	// Coalesce it into its XUID online id
	XUID xuidOnline = uint64( netAddress.GetIPNetworkByteOrder() ) | ( uint64( netAddress.GetPort() ) << 32ull );

	// Check if we have a pinging record for this server
	CServerPinging *pServer = ( CServerPinging * ) GetServerRecordByOnlineId( m_ServersPinging, xuidOnline );
	if ( pServer )
	{
		// We already have a pinging record for this server, just update its info
		pServer->m_gsi = *gsi;
		pServer->m_netAdr = gsi->m_NetAdr;
		return;
	}

	// Create a new pinging record
	pServer = new CServerPinging();
	pServer->m_xuid = xuidOnline;
	pServer->m_gsi = *gsi;
	pServer->m_netAdr = gsi->m_NetAdr;
	m_ServersPinging.AddToTail( pServer );
}

void CServerManager::RefreshComplete( HServerListRequest hReq, EMatchMakingServerResponse response )
{
	if ( m_eState == STATE_FETCHING_SERVERS )
		m_eState = STATE_GROUP_FETCHED;

	steamapicontext->SteamMatchmakingServers()->ReleaseRequest( m_hRequest );
	m_hRequest = NULL;
}
#endif

void CServerManager::Update()
{
#if !( !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS ) )
	return;
#endif

	float now = Plat_FloatTime();

	switch ( m_eState )
	{
	case STATE_IDLE:
		if ( m_bUpdateEnabled && !IsLocalClientConnectedToServer() )
		{
			if ( now > m_flNextUpdateTime )
			{
				MarkOldServersAndBeginSearch();
			}
			else if ( now > m_flNextServerUpdateTime )
			{
				float nextUpdatePeriod = mm_server_search_inet_ping_refresh.GetFloat();

				// any servers not in the pinging list but haven't been refreshed in a while should be refreshed
				for ( int i = 0; i < m_Servers.Count(); ++i )
				{
					float timePassed = now - m_Servers[i]->m_flLastRefresh;
					if ( timePassed > mm_server_search_inet_ping_refresh.GetFloat() )
					{
						CServerPinging *pServerPinging = ( CServerPinging * ) GetServerRecordByOnlineId( m_ServersPinging, m_Servers[i]->GetOnlineId() );
						if ( !pServerPinging )
						{
							pServerPinging = new CServerPinging();
							pServerPinging->m_xuid = m_Servers[i]->GetOnlineId();
#if !defined( NO_STEAM ) && !defined( SWDS )
							pServerPinging->m_netAdr = m_Servers[i]->m_netAdr;
#endif
							m_ServersPinging.AddToTail( pServerPinging );
						}
					}
					else
					{
						nextUpdatePeriod = MIN( nextUpdatePeriod, mm_server_search_inet_ping_refresh.GetFloat() - timePassed );
					}
				}

				m_flNextServerUpdateTime = now + nextUpdatePeriod;

				m_eState = STATE_REQUESTING_DETAILS;
				break;
			}
		}
		break;

	case STATE_LAN_SEARCH:
		UpdateLanSearch();
		break;

	case STATE_GROUP_SEARCH:
		if ( StartFetchingGroupServersData() )
			break;
		// else -> // fall through

	case STATE_GROUP_FETCHED:
		OnGroupFetched();
		break;

#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
	case STATE_FETCHING_SERVERS:
		if ( Plat_FloatTime() > m_lanSearchData.m_flLastBroadcastTime + mm_server_search_inet_ping_timeout.GetFloat() )
		{
			steamapicontext->SteamMatchmakingServers()->ReleaseRequest( m_hRequest );
			m_hRequest = NULL;
			
			m_eState = STATE_GROUP_FETCHED;
		}
		break;
#endif

	case STATE_REQUESTING_DETAILS:
		UpdateRequestingDetails();
		break;
	}
}

void CServerManager::MarkOldServersAndBeginSearch()
{
	DevMsg( 2, "Server manager refreshing...\n" );

	// Signal that we are starting a search
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
		"OnMatchServerMgrUpdate", "update", "searchstarted" ) );

	m_ServersPinging.PurgeAndDeleteElements();

	m_groupSearchData.Reset();
	m_lanSearchData = SLanSearchData_t();
	m_eState = STATE_LAN_SEARCH;

	// If broadcasts are disallowed, then go straight to group search state
	extern ConVar net_allow_multicast;
	if ( !net_allow_multicast.GetBool() )
		m_eState = STATE_GROUP_SEARCH;
}

void CServerManager::UpdateLanSearch()
{
	if ( m_lanSearchData.m_flStartTime && m_lanSearchData.m_flLastBroadcastTime )
	{
		if ( Plat_FloatTime() > m_lanSearchData.m_flStartTime + mm_server_search_lan_ping_duration.GetFloat() )
		{
			m_eState = STATE_GROUP_SEARCH;
			return;
		}

		if ( Plat_FloatTime() < m_lanSearchData.m_flLastBroadcastTime + mm_server_search_lan_ping_interval.GetFloat() )
		{
			// waiting out interval between pings
			return;
		}
	}
	else
	{
		// Initialize the start time of the lan broadcast
		m_lanSearchData.m_flStartTime = Plat_FloatTime();
	}

	//
	// Send the packet
	//
	m_lanSearchData.m_flLastBroadcastTime = Plat_FloatTime();

	KeyValues *kv = new KeyValues( "LanSearchServerPing" );
	KeyValues::AutoDelete autodelete( kv );
	kv->SetFloat( "timestamp", Plat_FloatTime() );

	if ( mm_server_search_lan_ports.GetString()[0] )
	{
		// Build the list of ports to scan
		CSplitString arrPorts( mm_server_search_lan_ports.GetString(), "," );

		for ( int i = 0; i < arrPorts.Count(); i++ )
		{
			// Port number
			int nPort = Q_atoi( arrPorts[i] );
			if ( nPort <= 0 )
				continue;

			g_pConnectionlessLanMgr->SendPacket( kv, CFmtStr( "*:%d", nPort ) );
		}
	}
}

void CServerManager::RemoveOldServers()
{
	float now = Plat_FloatTime();

	for ( int k = 0; k < m_Servers.Count(); ++ k )
	{
		CServer *pServer = m_Servers[ k ];
		if ( pServer && now - pServer->m_flLastRefresh < mm_server_search_server_lifetime.GetFloat() )
			continue;

		m_Servers.Remove( k -- );
	}
}

bool CServerManager::StartFetchingGroupServersData()
{
#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
	ISteamUser *pUser = steamapicontext->SteamUser();
	ISteamFriends *pFriends = steamapicontext->SteamFriends();
	if ( !pUser || !pFriends )
		return false;

	int iGroupCount = pFriends->GetClanCount();
	if ( !iGroupCount )
		return false;

	m_groupSearchData.m_UserGroupAccountIDs.SetCount( iGroupCount );
	for ( int k = 0; k < iGroupCount; ++ k )
	{
		m_groupSearchData.m_UserGroupAccountIDs[ k ] = pFriends->GetClanByIndex( k ).GetAccountID();
	}

	return FetchGroupServers();
#else
	return false;
#endif
}

bool CServerManager::FetchGroupServers()
{
#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )

	static const char gamedataFilterType[] = "gamedataor";

	char gamedataFitler[ mm_filter_max_size ];

	*gamedataFitler = 0;

	size_t roomLeft = mm_filter_max_size - strlen( gamedataFilterType ) - 3;

	// Add as many groups as will fit
	while ( m_groupSearchData.m_idxSearchGroupId < m_groupSearchData.m_UserGroupAccountIDs.Count() )
	{
		const char *tag = CFmtStr( *gamedataFitler ? ",grp:%ui" : "grp:%ui", m_groupSearchData.m_UserGroupAccountIDs[ m_groupSearchData.m_idxSearchGroupId ] );
		if ( roomLeft < strlen( tag ) )
		{
			break;
		}

		Q_strncat( gamedataFitler, tag, mm_filter_max_size );
		roomLeft -= strlen( tag );

		++m_groupSearchData.m_idxSearchGroupId;
	}

	MatchMakingKeyValuePair_t filters[ 2 ] = {
		// filter by game
		MatchMakingKeyValuePair_t( "gamedir", COM_GetModDirectory() ),
		// look for group servers
		MatchMakingKeyValuePair_t( gamedataFilterType, gamedataFitler )
	};

	// request the server list.  We will get called back at ServerResponded, ServerFailedToRespond, and RefreshComplete
	m_eState = STATE_FETCHING_SERVERS;
	m_lanSearchData.m_flLastBroadcastTime = Plat_FloatTime();

	MatchMakingKeyValuePair_t *pFilter = filters;
	DevMsg( 2, "Requesting group server list for groups %s...\n", gamedataFitler );
	
	if ( m_hRequest )
		steamapicontext->SteamMatchmakingServers()->ReleaseRequest( m_hRequest );
	m_hRequest = steamapicontext->SteamMatchmakingServers()->RequestInternetServerList(
		( AppId_t ) g_pMatchFramework->GetMatchTitle()->GetTitleID(), &pFilter, ARRAYSIZE( filters ), this );

	return true;

#else
	return false;
#endif
}

void CServerManager::OnGroupFetched()
{
	if ( m_bUpdateEnabled && 
		 m_groupSearchData.m_UserGroupAccountIDs.IsValidIndex( m_groupSearchData.m_idxSearchGroupId ) &&
		 FetchGroupServers() )
		 return;

	OnAllGroupsFetched();
}

void CServerManager::OnAllGroupsFetched()
{
	m_flNextUpdateTime = Plat_FloatTime() + mm_server_search_update_interval.GetInt();

	if ( !m_ServersPinging.Count() )
	{
		OnAllDataFetched();
		return;
	}

	m_eState = STATE_REQUESTING_DETAILS;
	m_lanSearchData.m_flStartTime = Plat_FloatTime();

	RequestPingingDetails();
}

void CServerManager::RequestPingingDetails()
{
	// Ping every server that we deferred to ping
	KeyValues *kv = new KeyValues( "InetSearchServerDetails" );
	KeyValues::AutoDelete autodelete( kv );
	
	for ( int k = 0; k < m_ServersPinging.Count() && k < mm_server_search_inet_ping_window.GetInt(); ++ k )
	{
		CServerPinging *pServerPinging = ( CServerPinging * ) m_ServersPinging[k];
		if ( pServerPinging->m_flTimeout && Plat_FloatTime() > pServerPinging->m_flTimeout )
		{
			m_ServersPinging.FastRemove( k -- );	// server timed out
			delete pServerPinging;
			continue;
		}

		kv->SetFloat( "timestamp", Plat_FloatTime() );
		kv->SetUint64( "pingxuid", pServerPinging->m_xuid );
#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
		g_pConnectionlessLanMgr->SendPacket( kv, pServerPinging->m_netAdr.GetConnectionAddressString() );
#else
		DevWarning( "Cannot request internet pinging server details.\n" );
#endif
		if ( !pServerPinging->m_flTimeout )
			pServerPinging->m_flTimeout = Plat_FloatTime() + mm_server_search_inet_ping_timeout.GetFloat();
	}

	m_lanSearchData.m_flLastBroadcastTime = Plat_FloatTime();

	DevMsg( 2, "Server manager waiting for game details from %d servers...\n", m_ServersPinging.Count() );
}

void CServerManager::UpdateRequestingDetails()
{
	if ( !m_ServersPinging.Count() )
	{
		// We have no more servers to ping
		m_ServersPinging.PurgeAndDeleteElements();
		OnAllDataFetched();
		return;
	}

	if ( m_lanSearchData.m_flLastBroadcastTime + mm_server_search_inet_ping_interval.GetFloat() < Plat_FloatTime() )
	{
		RequestPingingDetails();
	}
}

void CServerManager::OnAllDataFetched()
{
	DevMsg( 2, "Server manager refresh completed.\n" );

	m_eState = STATE_IDLE;

	if ( !m_bUpdateEnabled )
	{
		m_Servers.PurgeAndDeleteElements();
		m_ServersPinging.PurgeAndDeleteElements();
	}
	else
	{
		RemoveOldServers();
	}

	if ( g_pMatchFramework )
	{
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnMatchServerMgrUpdate", "update", "searchfinished" ) );
	}
}

