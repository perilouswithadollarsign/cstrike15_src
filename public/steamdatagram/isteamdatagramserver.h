//====== Copyright Valve Corporation, All rights reserved. ====================
//
// Service interface to Steam datagram transport library.
//
//=============================================================================

#ifndef ISTEAMDATAGRAMSERVER_H
#define ISTEAMDATAGRAMSERVER_H
#ifdef _WIN32
#pragma once
#endif

#include "steamdatagram/steamdatagram_shared.h"

//-----------------------------------------------------------------------------
/// Interface a gameserver uses to communicate to clients through a proxy

class ISteamDatagramTransportGameserver
{
public:

	/// Check for any incoming packets from any clients.  Your buffer MUST
	/// be at least k_nMaxSteamDatagramTransportPayload.  Returns size of the datagram
	/// returned into your buffer if a message is available.  Returns 0 if nothing
	/// available, or < 0 if there's an error.
	virtual int RecvDatagram( void *pBuffer, uint32 nBufferSize, CSteamID *pSteamIDOutFromClient, uint64 *pusecTimeRecv, int nChannel ) = 0;

	/// Send a datagram to the client.  You must have sent or received data from this
	/// client relatively recently.  (Within the last 60 seconds or so.)  If no communication
	/// happens between a server and a client, the client will eventually be forgotten.
	///
	/// If you try to send a packet to a client that has been forgotten, k_EResultNotLoggedOn
	/// will be returned.
	virtual EResult SendDatagram( const void *pData, uint32 nSizeBytes, CSteamID steamIDToClient, int nChannel ) = 0;

	/// Close down the socket and destroy the interface object.
	virtual void Destroy() = 0;

	/// Close the socket (used when forking).  Does not do shutdown
	virtual void CloseSockets() = 0;

	/// !FIXME! Notification mechanism?
	/// Connection quality metrics?
};

/// Start listening on the specified port.  The port parameter is currently not optional,
/// you must choose port.
ISteamDatagramTransportGameserver *SteamDatagram_GameserverListen(
	EUniverse eUniverse,
	uint16 unBindPort,
	EResult *pOutResult,
	SteamDatagramErrMsg &errMsg
);

#endif // ISTEAMDATAGRAMSERVER_H
