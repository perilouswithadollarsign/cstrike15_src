//====== Copyright Valve Corporation, All rights reserved. ====================
//
// Client interface to Steam datagram transport library.
//
//=============================================================================

#ifndef ISTEAMDATAGRAMCLIENT_H
#define ISTEAMDATAGRAMCLIENT_H
#ifdef _WIN32
#pragma once
#endif

#include "steamdatagram/steamdatagram_shared.h"
#include "steam/steamuniverse.h"

class CMsgSteamDatagramGameServerAuthTicket;
class ISteamNetworkingUtils;

/// Status of a particular network resource
enum ESteamDatagramAvailability
{
	k_ESteamDatagramAvailability_CannotTry = -3,		// A dependent resource is missing, so this service is unavailable.  (E.g. we cannot talk to routers because Internet is down or we don't have the network config.)
	k_ESteamDatagramAvailability_Failed = -2,			// We have tried for enough time that we would expect to have been successful by now.  We have never been successful
	k_ESteamDatagramAvailability_Previously = -1,		// We tried and were successful at one time, but now it looks like we have a problem
	k_ESteamDatagramAvailability_NeverTried = 0,		// We don't know because we haven't ever checked.
	k_ESteamDatagramAvailability_Attempting = 1,		// We're trying now, but are not yet successful.  This is not an error, but it's not success, either.
	k_ESteamDatagramAvailability_Current = 2,			// Resource is online.
};

//-----------------------------------------------------------------------------
/// Interface to send datagrams to a Steam service that uses the
/// Steam datagram transport infrastructure.

class ISteamDatagramTransportClient
{
public:

	/// Send a datagram to the service
	virtual EResult SendDatagram( const void *pData, uint32 nSizeBytes, int nChannel ) = 0;

	/// Receive the next available datagram from the service.  Your buffer MUST
	/// be at least k_nMaxSteamDatagramTransportPayload.  Returns size of the datagram
	/// returned into your buffer if a message is available.  Returns 0 if nothing
	/// available, or < 0 if there's an error.
	virtual int RecvDatagram( void *pBuffer, uint32 nBufferSize, uint64 *pusecTimeRecv, int nChannel ) = 0;

	/// Close the connection to the gameserver
	virtual void Close() = 0;

	/// Describe state of current connection
	struct ConnectionStatus
	{
		/// Do we have a valid network configuration?  We cannot do anything without this.
		ESteamDatagramAvailability m_eAvailNetworkConfig;

//		/// Does it look like we have a connection to the Internet at all?
//		EAvailability m_eAvailInternet;

		/// Successful communication with a box on the routing network.
		/// This will be marked as failed if there is a general internet
		/// connection.
		ESteamDatagramAvailability m_eAvailAnyRouterCommunication;

		/// End-to-end communication with the gameserver.  This will be marked
		/// as failed if there is a client problem.
		ESteamDatagramAvailability m_eAvailGameserver;

		/// Stats for end-to-end link to the gameserver
		SteamDatagramLinkStats m_statsEndToEnd;

		/// Index of data center containing the gameserver we are trying to talk to (if any)
		int m_idxGameServerDataCenter;

		/// Currently selected front router, if any
		int m_idxPrimaryRouterCluster;
		char m_szPrimaryRouterName[64];
		uint32 m_unPrimaryRouterIP;
		uint16 m_unPrimaryRouterPort;

		/// Stats for "front" link to current router
		SteamDatagramLinkStats m_statsPrimaryRouter;

		/// Back ping time as reported by primary.
		/// (The front ping is in m_statsPrimaryRouter,
		/// and usually the front ping plus the back ping should
		/// approximately equal the end-to-end ping)
		int m_nPrimaryRouterBackPing;

		/// Currently selected back router, if any
		int m_idxBackupRouterCluster;
		char m_szBackupRouterName[64];
		uint32 m_unBackupRouterIP;
		uint16 m_unBackupRouterPort;

		/// Ping times to backup router, if any
		int m_nBackupRouterFrontPing, m_nBackupRouterBackPing;

		/// Print into a buffer.  Returns the number of characters copied,
		/// or needed if you pass NULL.  (Includes the '\0' terminator in
		/// both cases.)
		int Print( char *pszBuf, int cbBuf ) const;
	};

	/// Status query
	virtual void GetConnectionStatus( ConnectionStatus &result ) = 0;
};

/// !KLUDGE! Glue code that will go away when we move everything into
/// the ISteamNetwork interfaces
extern void SteamDatagramClient_Init( const char *pszCacheDirectory, ESteamDatagramPartner ePartner, int iPartnerMask );

/// Get ISteamNetworkingUtils object.  This will eventually go in Steam_api.h with all the rest of its kin
extern ISteamNetworkingUtils *SteamNetworkingUtils();

/// Start talking to a gameserver.
ISteamDatagramTransportClient *SteamDatagramClient_Connect( CSteamID steamID );

/// Shutdown all clients and close all sockets
void SteamDatagramClient_Kill();

#endif // ISTEAMDATAGRAMCLIENT_H
