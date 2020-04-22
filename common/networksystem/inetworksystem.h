//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef INETWORKSYSTEM_H
#define INETWORKSYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "appframework/iappsystem.h"


// This is the packet payload without any header bytes (which are attached for actual sending)
#define	NET_MAX_PAYLOAD			( 262144 - 4)	// largest message we can send in bytes
#define NET_MAX_PAYLOAD_BITS	18		// 2^NET_MAX_PAYLOAD_BITS > NET_MAX_PAYLOAD
// This is just the client_t->netchan.datagram buffer size (shouldn't ever need to be huge)
#define NET_MAX_DATAGRAM_PAYLOAD 4000	// = maximum unreliable playload size

// UDP has 28 byte headers
#define UDP_HEADER_SIZE				(20+8)	// IP = 20, UDP = 8

#define MAX_ROUTABLE_PAYLOAD		1200	// x360 requires <= 1260, but now that listen servers can support "steam" mediated sockets, steam enforces 1200 byte limit

#if (MAX_ROUTABLE_PAYLOAD & 3) != 0
#error Bit buffers must be a multiple of 4 bytes
#endif

#define MIN_ROUTABLE_PAYLOAD		16		// minimum playload size

#define NETMSG_TYPE_BITS	8	// must be 2^NETMSG_TYPE_BITS > SVC_LASTMSG

// This is the payload plus any header info (excluding UDP header)

#define HEADER_BYTES	9	// 2*4 bytes seqnr, 1 byte flags

// Pad this to next higher 16 byte boundary
// This is the largest packet that can come in/out over the wire, before processing the header
//  bytes will be stripped by the networking channel layer
#define	NET_MAX_MESSAGE	PAD_NUMBER( ( NET_MAX_PAYLOAD + HEADER_BYTES ), 16 )

#define NET_HEADER_FLAG_SPLITPACKET				-2
#define NET_HEADER_FLAG_COMPRESSEDPACKET		-3

//-----------------------------------------------------------------------------
// Forward declarations: 
//-----------------------------------------------------------------------------
class INetworkMessageHandler;
class INetworkMessage;
class INetChannel;
class INetworkMessageFactory;
class bf_read;
class bf_write;
typedef struct netadr_s netadr_t;
class CNetPacket;


//-----------------------------------------------------------------------------
// Default ports
//-----------------------------------------------------------------------------
enum
{
	NETWORKSYSTEM_DEFAULT_SERVER_PORT = 27001,
	NETWORKSYSTEM_DEFAULT_CLIENT_PORT = 27002
};


//-----------------------------------------------------------------------------
// This interface encompasses a one-way communication path between two
//-----------------------------------------------------------------------------
typedef int ConnectionHandle_t;

enum ConnectionStatus_t
{
	CONNECTION_STATE_DISCONNECTED = 0,
	CONNECTION_STATE_CONNECTING,
	CONNECTION_STATE_CONNECTION_FAILED,
	CONNECTION_STATE_CONNECTED,
};


//-----------------------------------------------------------------------------
// This interface encompasses a one-way communication path between two machines
//-----------------------------------------------------------------------------
// 
// abstract_class INetChannel
// {
// public:
// //	virtual INetworkMessageHandler *GetMsgHandler( void ) const = 0;
// 	virtual const netadr_t	&GetRemoteAddress( void ) const = 0;
// 
// 	// send a net message
// 	// NOTE: There are special connect/disconnect messages?
// 	virtual bool AddNetMsg( INetworkMessage *msg, bool bForceReliable = false ) = 0; 
// //	virtual bool RegisterMessage( INetworkMessage *msg ) = 0;
// 
// 	virtual ConnectionStatus_t GetConnectionState( ) = 0;
// 
// /*
// 	virtual ConnectTo( const netadr_t& to ) = 0;
// 	virtual Disconnect() = 0;
// 
// 	virtual const netadr_t& GetLocalAddress() = 0;
// 
// 	virtual const netadr_t& GetRemoteAddress() = 0;
// */
// };


//-----------------------------------------------------------------------------
// Network event types + structures
//-----------------------------------------------------------------------------
enum NetworkEventType_t
{
	NETWORK_EVENT_CONNECTED = 0,
	NETWORK_EVENT_DISCONNECTED,
	NETWORK_EVENT_MESSAGE_RECEIVED,
};

struct NetworkEvent_t
{
	NetworkEventType_t m_nType;
};

struct NetworkConnectionEvent_t : public NetworkEvent_t
{
	INetChannel *m_pChannel;
};

struct NetworkDisconnectionEvent_t : public NetworkEvent_t
{
	INetChannel *m_pChannel;
};

struct NetworkMessageReceivedEvent_t : public NetworkEvent_t
{
	INetChannel *m_pChannel;
	INetworkMessage *m_pNetworkMessage;
};


//-----------------------------------------------------------------------------
// Main interface for low-level networking (packet sending). This is a low-level interface
//-----------------------------------------------------------------------------
abstract_class INetworkSystem : public IAppSystem
{
public:
	// Installs network message factories to be used with all connections
	virtual bool RegisterMessage( INetworkMessage *msg ) = 0;

	// Start, shutdown a server
	virtual bool StartServer( unsigned short nServerListenPort = NETWORKSYSTEM_DEFAULT_SERVER_PORT ) = 0;
	virtual void ShutdownServer( ) = 0;

	// Process server-side network messages
	virtual void ServerReceiveMessages() = 0;
	virtual void ServerSendMessages() = 0;

	// Start, shutdown a client
	virtual bool StartClient( unsigned short nClientListenPort = NETWORKSYSTEM_DEFAULT_CLIENT_PORT ) = 0;
	virtual void ShutdownClient( ) = 0;

	// Process client-side network messages
	virtual void ClientSendMessages() = 0;
	virtual void ClientReceiveMessages() = 0;

	// Connect, disconnect a client to a server
	virtual INetChannel* ConnectClientToServer( const char *pServer, int nServerListenPort = NETWORKSYSTEM_DEFAULT_SERVER_PORT ) = 0;
	virtual void DisconnectClientFromServer( INetChannel* pChan ) = 0;

	// Event queue
	virtual NetworkEvent_t *FirstNetworkEvent( ) = 0;
	virtual NetworkEvent_t *NextNetworkEvent( ) = 0;

	// Returns the local host name
	virtual const char* GetLocalHostName( void ) const = 0;
	virtual const char* GetLocalAddress( void ) const = 0;

	/*
	// NOTE: Server methods
	// NOTE: There's only 1 client INetChannel ever
	// There can be 0-N server INetChannels.
	virtual INetChannel* CreateConnection( bool bIsClientConnection ) = 0;

	// Add methods for setting unreliable payloads
	*/
};


#endif // INETWORKSYSTEM_H
