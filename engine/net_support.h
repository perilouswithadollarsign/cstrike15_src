//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef NET_SUPPORT_H
#define NET_SUPPORT_H
#ifdef _WIN32
#pragma once
#endif


#include "tier0/dbg.h"
#include "tier0/icommandline.h"

#include "tier1/strtools.h"
#include "tier1/checksum_crc.h"
#include "tier1/keyvalues.h"
#include "tier1/utlbuffer.h"

#include "mathlib/mathlib.h"

#include "appframework/IAppSystemGroup.h"
#include "matchmaking/imatchframework.h"
#include "engine/inetsupport.h"

#include "tier2/tier2.h"


class CNetSupportImpl : public CTier2AppSystem< INetSupport >
{
	typedef CTier2AppSystem< INetSupport > BaseClass;

	// Methods of IAppSystem
public:
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// Methods of INetSupport
public:
	// Get engine build number
	virtual int GetEngineBuildNumber();

	// Get server info
	virtual void GetServerInfo( ServerInfo_t *pServerInfo );

	// Get client info
	virtual void GetClientInfo( ClientInfo_t *pClientInfo );

	// Update a local server reservation
	virtual void UpdateServerReservation( uint64 uiReservation );

	// Update a client reservation
	virtual void UpdateClientReservation( uint64 uiReservation, uint64 uiMachineIdHost );

	// Submit a server reservation packet
	virtual void ReserveServer(
		const ns_address &netAdrPublic, const ns_address &netAdrPrivate,
		uint64 nServerReservationCookie, KeyValues *pKVGameSettings,
		IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation ) OVERRIDE;

	// Check server reservation cookie matches cookie held by client
	virtual bool CheckServerReservation( 
		const ns_address &netAdrPublic, uint64 nServerReservationCookie, uint32 uiReservationStage,
		IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation ) OVERRIDE;

	virtual bool ServerPing( const ns_address &netAdrPublic,
		IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation ) OVERRIDE;

	// When client event is fired
	virtual void OnMatchEvent( KeyValues *pEvent );

	// Process incoming net packets on the socket
	virtual void ProcessSocket( int sock, IConnectionlessPacketHandler * pHandler );

	// Send a network packet
	virtual int SendPacket (
		INetChannel *chan, int sock,  const netadr_t &to,
		const void *data, int length,
		bf_write *pVoicePayload = NULL,
		bool bUseCompression = false );

    virtual ISteamNetworkingUtils *GetSteamNetworkingUtils() OVERRIDE;

public:
	CNetSupportImpl();
	~CNetSupportImpl();
};

#endif // NET_SUPPORT_H
