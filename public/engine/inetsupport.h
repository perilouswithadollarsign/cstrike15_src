//===== Copyright c 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef INETSUPPORT_H
#define INETSUPPORT_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"
#include "keyvalues.h"
#include "bitbuf.h"
#include "inetchannel.h"
#include "inetmsghandler.h"
#include "matchmaking/imatchasync.h"

class ISteamNetworkingUtils;

abstract_class INetSupport : public IAppSystem
{
public:
	enum NetworkConsts_t
	{
		NC_MAX_ROUTABLE_PAYLOAD = 1200,
	};

	enum NetworkSocket_t
	{
		NS_SOCK_CLIENT = 0,	// client socket
		NS_SOCK_SERVER,		// server socket
#ifdef _X360
		NS_SOCK_SYSTEMLINK,		// X360 system link
		NS_SOCK_LOBBY,			// X360 matchmaking lobby
		NS_SOCK_TEAMLINK,		// X360 matchmaking inter-team link
#endif
	};

	enum SteamP2PChannelId_t
	{
		// see top of net_steamsocketmgr.cpp for why we need seperate channels for client & server
		SP2PC_RECV_CLIENT = 0,
		SP2PC_RECV_SERVER,
		SP2PC_LOBBY
	};

	struct ServerInfo_t
	{
		netadr_t m_netAdr;
		netadr_t m_netAdrOnline;
		uint16	m_nPort;

		bool m_bActive;		// sv.IsActive
		bool m_bDedicated;	// sv.IsDedicated
		bool m_bLobbyExclusive;	// Exclusive to lobby connections
		bool m_bGroupExclusive;	// Exclusive to Steam Group
		bool m_bInMainMenuBkgnd; // Server is in a background map

		char const *m_szServerName;
		char const *m_szMapName;
		char const *m_szMapGroupName;

		int m_numMaxHumanPlayers;
		int m_numHumanPlayers;
	};

	struct ClientInfo_t
	{
		int m_nSignonState;	// client signon state
		int m_nSocket;		// client socket
		INetChannel *m_pNetChannel;

		int m_numHumanPlayers;	// Number of human players on the server
	};

public:
	// Get engine build number
	virtual int GetEngineBuildNumber() = 0;

	// Get server info
	virtual void GetServerInfo( ServerInfo_t *pServerInfo ) = 0;

	// Get client info
	virtual void GetClientInfo( ClientInfo_t *pClientInfo ) = 0;

	// Update a local server reservation
	virtual void UpdateServerReservation( uint64 uiReservation ) = 0;

	// Update a client reservation before connecting to a server
	virtual void UpdateClientReservation( uint64 uiReservation, uint64 uiMachineIdHost ) = 0;

	// Submit a server reservation packet
	virtual void ReserveServer(
		const ns_address &netAdrPublic, const ns_address &netAdrPrivate,
		uint64 nServerReservationCookie, KeyValues *pKVGameSettings,
		IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation ) = 0;

	// Check server reservation cookie matches cookie held by client
	virtual bool CheckServerReservation( 
		const ns_address &netAdrPublic, uint64 nServerReservationCookie, uint32 uiReservationStage,
		IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation ) = 0;

	virtual bool ServerPing( const ns_address &netAdrPublic,
		IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation ) = 0;

	// When client event is fired
	virtual void OnMatchEvent( KeyValues *pEvent ) = 0;

	// Process incoming net packets on the socket
	virtual void ProcessSocket( int sock, IConnectionlessPacketHandler * pHandler ) = 0;

	// Send a network packet
	virtual int SendPacket (
		INetChannel *chan, int sock,  const netadr_t &to,
		const void *data, int length,
		bf_write *pVoicePayload = NULL,
		bool bUseCompression = false ) = 0;

	virtual ISteamNetworkingUtils *GetSteamNetworkingUtils() = 0;
};

#define INETSUPPORT_VERSION_STRING "INETSUPPORT_003"


#endif // INETSUPPORT_H
