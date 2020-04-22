//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "net_support.h"

#ifndef DEDICATED
#include "client.h"
#endif

#include "server.h"
#include "host_cmd.h"
#include "net.h"
#include "proto_oob.h"
#include "steamdatagram/isteamdatagramclient.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNetSupportImpl::CNetSupportImpl()
{
	;
}

CNetSupportImpl::~CNetSupportImpl()
{
	;
}

static CNetSupportImpl g_NetSupport;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CNetSupportImpl, INetSupport,
								   INETSUPPORT_VERSION_STRING, g_NetSupport );

//
// IAppSystem implementation
//

static CreateInterfaceFn s_pfnDelegateFactory;
static void * InternalFactory( const char *pName, int *pReturnCode )
{
	if ( pReturnCode )
	{
		*pReturnCode = IFACE_OK;
	}

	// Try to get interface via delegate
	if ( void *pInterface = s_pfnDelegateFactory ? s_pfnDelegateFactory( pName, pReturnCode ) : NULL )
	{
		return pInterface;
	}

	// Try to get internal interface
	if ( void *pInterface = Sys_GetFactoryThis()( pName, pReturnCode ) )
	{
		return pInterface;
	}

	// Failed
	if ( pReturnCode )
	{
		*pReturnCode = IFACE_FAILED;
	}
	return NULL;	
}

bool CNetSupportImpl::Connect( CreateInterfaceFn factory )
{
	Assert( !s_pfnDelegateFactory );

	s_pfnDelegateFactory = factory;

	CreateInterfaceFn ourFactory = InternalFactory;
	ConnectTier1Libraries( &ourFactory, 1 );
	ConnectTier2Libraries( &ourFactory, 1 );

	s_pfnDelegateFactory = NULL;

	return true;
}

void CNetSupportImpl::Disconnect()
{
	DisconnectTier2Libraries();
	DisconnectTier1Libraries();
}

void * CNetSupportImpl::QueryInterface( const char *pInterfaceName )
{
	if ( !Q_stricmp( pInterfaceName, INETSUPPORT_VERSION_STRING ) )
		return static_cast< INetSupport * >( this );

	return NULL;
}

InitReturnVal_t CNetSupportImpl::Init()
{
	return INIT_OK;
}

void CNetSupportImpl::Shutdown()
{
	return;
}

//
// Net support implementation
//

int CNetSupportImpl::GetEngineBuildNumber()
{
	return GetHostVersion();
}

void CNetSupportImpl::GetServerInfo( ServerInfo_t *pServerInfo )
{
	if ( !pServerInfo )
		return;
	memset( pServerInfo, 0, sizeof( *pServerInfo ) );

	extern netadr_t net_local_adr;
	extern ConVar sv_steamgroup_exclusive;

	pServerInfo->m_netAdr = net_local_adr;
	static bool s_bHideHostIP = !!CommandLine()->FindParm( "-hidehostip" );
	if ( !NET_GetPublicAdr( pServerInfo->m_netAdrOnline ) )
	{
		if ( s_bHideHostIP )
		{
			static netadr_t s_netAdrLocalhost( "127.0.0.1" );
			pServerInfo->m_netAdr.SetIP( s_netAdrLocalhost.GetIPHostByteOrder() );
		}
		pServerInfo->m_netAdrOnline = pServerInfo->m_netAdr;
	}
	else if ( s_bHideHostIP )
	{
		pServerInfo->m_netAdr = pServerInfo->m_netAdrOnline;
	}
	pServerInfo->m_nPort = sv.GetUDPPort();

	pServerInfo->m_netAdr.SetPort( pServerInfo->m_nPort );
	pServerInfo->m_netAdrOnline.SetPort( pServerInfo->m_nPort );

	pServerInfo->m_bActive = sv.IsActive() || sv.IsLoading();
	pServerInfo->m_bDedicated = sv.IsDedicated();
	pServerInfo->m_bLobbyExclusive = sv.IsExclusiveToLobbyConnections();
	pServerInfo->m_bGroupExclusive = sv_steamgroup_exclusive.GetBool() || sv.GetPassword();
	pServerInfo->m_bInMainMenuBkgnd = sv.IsLevelMainMenuBackground();

	pServerInfo->m_szServerName = sv.GetName();
	pServerInfo->m_szMapName = sv.GetMapName();
	pServerInfo->m_szMapGroupName = sv.GetMapGroupName();

	pServerInfo->m_numMaxHumanPlayers = sv.GetMaxHumanPlayers();
	pServerInfo->m_numHumanPlayers = sv.GetNumHumanPlayers();
}

void CNetSupportImpl::GetClientInfo( ClientInfo_t *pClientInfo )
{
#ifndef DEDICATED
	if ( !pClientInfo )
		return;
	memset( pClientInfo, 0, sizeof( *pClientInfo ) );

#ifndef DEDICATED
	CClientState &rcs = GetBaseLocalClient();
	
	pClientInfo->m_nSignonState = rcs.m_nSignonState;
	pClientInfo->m_nSocket = rcs.m_Socket;
	pClientInfo->m_pNetChannel = rcs.m_NetChannel;
#endif

	// Compute number of human players on the server
	pClientInfo->m_numHumanPlayers = 0;
	if ( rcs.IsConnected() && rcs.m_pUserInfoTable )
	{
		for ( int i = 0; i < rcs.m_nMaxClients; i++ )
		{
			extern IVEngineClient *engineClient;
			player_info_t pi;

			if ( engineClient->GetPlayerInfo( i + 1, &pi ) &&
				!pi.fakeplayer &&
				!pi.ishltv )
			{
				++ pClientInfo->m_numHumanPlayers; // This is an actual human player
			}
		}
	}
#endif
}

void CNetSupportImpl::UpdateClientReservation( uint64 uiReservation, uint64 uiMachineIdHost )
{
#ifndef DEDICATED
	GetBaseLocalClient().SetServerReservationCookie( uiReservation );
	GetBaseLocalClient().m_ListenServerSteamID = uiMachineIdHost;
#endif
}

void CNetSupportImpl::UpdateServerReservation( uint64 uiReservation )
{
	sv.SetReservationCookie( uiReservation, "netsupport" );
}



void CNetSupportImpl::ReserveServer(
	const ns_address &netAdrPublic, const ns_address &netAdrPrivate, uint64 nServerReservationCookie,
	KeyValues *pKVGameSettings,
	IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation )
{
#ifndef DEDICATED
	GetBaseLocalClient().ReserveServer( netAdrPublic, netAdrPrivate,
		nServerReservationCookie, pKVGameSettings,
		pCallback, ppAsyncOperation );
#else
	Assert( 0 );
#endif
}

bool CNetSupportImpl::CheckServerReservation(
	const ns_address &netAdrPublic, uint64 nServerReservationCookie, uint32 uiReservationStage,
	IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation )
{
#ifndef DEDICATED
	return GetBaseLocalClient().CheckServerReservation( netAdrPublic, 
		nServerReservationCookie, uiReservationStage, pCallback, ppAsyncOperation );
#else
	Assert( 0 );
	return false;
#endif
}

bool CNetSupportImpl::ServerPing( const ns_address &netAdrPublic, 
	IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation )
{
#ifndef DEDICATED
	return GetBaseLocalClient().ServerPing( netAdrPublic, pCallback, ppAsyncOperation );
#else
	Assert( 0 );
	return false;
#endif
}

// When client event is fired
void CNetSupportImpl::OnMatchEvent( KeyValues *pEvent )
{
#ifndef DEDICATED
	GetBaseLocalClient().OnEvent( pEvent );
#endif
}

void CNetSupportImpl::ProcessSocket( int sock, IConnectionlessPacketHandler * pHandler )
{
	NET_ProcessSocket( sock, pHandler );
}

// Send a network packet
int CNetSupportImpl::SendPacket (
	INetChannel *chan, int sock,  const netadr_t &to,
	const void *data, int length,
	bf_write *pVoicePayload /*= NULL*/,
	bool bUseCompression /*= false*/ )
{
	netadr_t inetAddr( to );
	if ( !inetAddr.GetPort() && inetAddr.GetType() == NA_BROADCAST )
	{
		inetAddr.SetPort(
#ifdef _X360
			NET_GetUDPPort( NS_X360_SYSTEMLINK )
#else
			PORT_SERVER
#endif
			);
	}

	return NET_SendPacket( chan, sock, inetAddr, ( const unsigned char * ) data, length, pVoicePayload, bUseCompression );
}

ISteamNetworkingUtils *CNetSupportImpl::GetSteamNetworkingUtils()
{
	#ifdef DEDICATED
		Assert( false ); // why are we asking?
		return nullptr;
	#else
		return ::SteamNetworkingUtils();
	#endif
}
