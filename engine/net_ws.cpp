//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// net_ws.c
// Windows IP Support layer.

#include "tier0/vprof.h"
#include "net_ws_headers.h"
#include "net_ws_queued_packet_sender.h"
#include "tier1/lzss.h"
#include "tier1/tokenset.h"
#include "matchmaking/imatchframework.h"
#include "tier2/tier2.h"
#include "ienginetoolinternal.h"
#include "server.h"
#include "mathlib/IceKey.H"
#include "steamdatagram/isteamdatagramclient.h"
#include "steamdatagram/isteamdatagramserver.h"
#include "steamdatagram/isteamnetworkingutils.h"
#include "engine/inetsupport.h"

#if !defined( _X360 ) && !defined( NO_STEAM )
#include "sv_steamauth.h"
#endif

#ifndef DEDICATED
#include "cl_steamauth.h"
#endif

#ifdef _PS3
#include <cell/sysmodule.h>
#endif


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define NET_COMPRESSION_STACKBUF_SIZE 4096 

static ConVar net_showsplits( "net_showsplits", "0", FCVAR_RELEASE, "Show info about packet splits" );

static ConVar net_splitrate( "net_splitrate", "1", FCVAR_RELEASE, "Number of fragments for a splitpacket that can be sent per frame" );

static ConVar ipname        ( "ip", "localhost", FCVAR_RELEASE, "Overrides IP for multihomed hosts" );
static ConVar ipname_tv     ( "ip_tv", "", FCVAR_RELEASE, "Overrides IP used to bind TV port for multihomed hosts" );
static ConVar ipname_tv1    ( "ip_tv1", "", FCVAR_RELEASE, "Overrides IP used to bind TV1 port for multihomed hosts" );
static ConVar ipname_relay	( "ip_relay", "", FCVAR_RELEASE, "Overrides IP used to redirect TV relay connections for NAT hosts" );
static ConVar ipname_steam  ( "ip_steam", "", FCVAR_RELEASE, "Overrides IP used to bind Steam port for multihomed hosts" );
static ConVar hostport      ( "hostport", NETSTRING( PORT_SERVER ) , FCVAR_RELEASE, "Host game server port" );
static ConVar hostip		( "hostip", "", FCVAR_RELEASE, "Host game server ip" );
static ConVar net_public_adr( "net_public_adr", "", FCVAR_RELEASE, "For servers behind NAT/DHCP meant to be exposed to the public internet, this is the public facing ip address string: (\"x.x.x.x\" )" );

static ConVar clientport    ( "clientport", NETSTRING( PORT_CLIENT ), FCVAR_RELEASE, "Host game client port" );
static ConVar hltvport		( "tv_port", NETSTRING( PORT_HLTV ), FCVAR_RELEASE, "Host GOTV[0] port" );
static ConVar hltvport1		( "tv_port1", NETSTRING( PORT_HLTV1 ), FCVAR_RELEASE, "Host GOTV[1] port" );
#if defined( REPLAY_ENABLED )
static ConVar replayport	( "replay_port", va("%i",PORT_REPLAY), 0, "Host Replay port" );
#endif

static ConVar fakelag		( "net_fakelag", "0", FCVAR_CHEAT, "Lag all incoming network data (including loopback) by this many milliseconds." );
static ConVar fakeloss		( "net_fakeloss", "0", FCVAR_CHEAT, "Simulate packet loss as a percentage (negative means drop 1/n packets)" ); 
static ConVar droppackets	( "net_droppackets", "0", FCVAR_CHEAT, "Drops next n packets on client" ); 
static ConVar fakejitter	( "net_fakejitter", "0", FCVAR_CHEAT, "Jitter fakelag packet time" );

static ConVar net_compressvoice( "net_compressvoice", "0", 0, "Attempt to compress out of band voice payloads (360 only)." );
ConVar net_usesocketsforloopback( "net_usesocketsforloopback", "0",
#ifdef _DEBUG
	FCVAR_RELEASE
#else
	0
#endif
	, "Use network sockets layer even for listen server local player's packets (multiplayer only)." );

static ConVar voice_verbose( "voice_verbose", "0", FCVAR_DEVELOPMENTONLY, "Turns on debug output with detailed spew about voice data processing." );

static ConVar voice_xsend_debug( "voice_xsend_debug", "0" );

#ifdef _DEBUG
static ConVar fakenoise		( "net_fakenoise", "0", FCVAR_CHEAT, "Simulate corrupt network packets (changes n bits per packet randomly)" ); 
static ConVar fakeshuffle	( "net_fakeshuffle", "0", FCVAR_CHEAT, "Shuffles order of every nth packet (needs net_fakelag)" ); 
static ConVar recvpackets	( "net_recvpackets", "-1", FCVAR_CHEAT, "Receive exactly next n packets if >= 0" ); 
static ConVar	net_savelargesplits( "net_savelargesplits", "-1", 0, "If not -1, then if a split has this many or more split parts, save the entire packet to disc for analysis." );
#endif

#ifdef _X360
static void NET_LogServerCallback( IConVar *var, const char *pOldString, float flOldValue );
static ConVar net_logserver( "net_logserver", "0", 0,  "Dump server stats to a file", NET_LogServerCallback );
static ConVar net_loginterval( "net_loginterval", "1", 0, "Time in seconds between server logs" );
#endif

static ConVar sv_steamdatagramtransport_port( "sv_steamdatagramtransport_port", "", FCVAR_RELEASE, "If non zero, listen for proxied traffic on the specified port" );

//-----------------------------------------------------------------------------
// Toggle Xbox 360 network security to allow cross-platform testing
//-----------------------------------------------------------------------------
#if !defined( _X360 )
#define X360SecureNetwork() false
#define IPPROTO_VDP	IPPROTO_UDP
#elif defined( _CERT )
#define X360SecureNetwork() true
#else
bool X360SecureNetwork( void )
{
	if ( CommandLine()->FindParm( "-xnet_bypass_security" ) )
	{
		return false;
	}
	return true;
}
#endif

extern ConVar net_showudp;
extern ConVar net_showudp_oob;
extern ConVar net_showudp_remoteonly;
extern ConVar net_showtcp;
extern ConVar net_blocksize;
extern int host_framecount;

extern bool ShouldChecksumPackets();
extern unsigned short BufferToShortChecksum( const void *pvData, size_t nLength );
extern void NET_InitParanoidMode();
void NET_ClearQueuedPacketsForChannel( INetChannel *chan );

#define DEF_LOOPBACK_SIZE 2048

typedef struct
{
	int			nPort;		// UDP/TCP use same port number
	bool		bListening;	// true if TCP port is listening
	int			hUDP;		// handle to UDP socket from socket()
	int			hTCP;		// handle to TCP socket from socket()
} netsocket_t;

typedef struct 
{
	int				newsock;	// handle of new socket
	int				netsock;	// handle of listen socket
	float			time;
	netadr_t		addr;
} pendingsocket_t;


#include "tier0/memdbgoff.h"

struct loopback_t
{
	char		*data;		// loopback buffer
	int			datalen;	// current data length
	char		defbuffer[ DEF_LOOPBACK_SIZE ];

	DECLARE_FIXEDSIZE_ALLOCATOR( loopback_t );
};

#include "tier0/memdbgon.h"

DEFINE_FIXEDSIZE_ALLOCATOR( loopback_t, 2, CUtlMemoryPool::GROW_SLOW );

// Split long packets.  Anything over 1460 is failing on some routers
typedef struct
{
	int		currentSequence;
	int		splitCount;
	int		totalSize;
	int		nExpectedSplitSize;
	char	buffer[ NET_MAX_MESSAGE ];	// This has to be big enough to hold the largest message
} LONGPACKET;

// Use this to pick apart the network stream, must be packed
#pragma pack(1)
typedef struct
{
	int		netID;
	int		sequenceNumber;
	int		packetID : 16;
	int		nSplitSize : 16;
} SPLITPACKET;
#pragma pack()

#define MIN_USER_MAXROUTABLE_SIZE	576  // ( X.25 Networks )
#define MAX_USER_MAXROUTABLE_SIZE	MAX_ROUTABLE_PAYLOAD


#define MAX_SPLIT_SIZE	(MAX_USER_MAXROUTABLE_SIZE - sizeof( SPLITPACKET ))
#define MIN_SPLIT_SIZE	(MIN_USER_MAXROUTABLE_SIZE - sizeof( SPLITPACKET ))

static ConVar sv_maxroutable
	( 
	"sv_maxroutable", 
	"1200", 
	0, 
	"Server upper bound on net_maxroutable that a client can use.", 
	true, MIN_USER_MAXROUTABLE_SIZE, 
	true, MAX_USER_MAXROUTABLE_SIZE 
	);

ConVar net_maxroutable
	( 
	"net_maxroutable", 
	"1200", 
	FCVAR_ARCHIVE | FCVAR_USERINFO, 
	"Requested max packet size before packets are 'split'.", 
	true, MIN_USER_MAXROUTABLE_SIZE, 
	true, MAX_USER_MAXROUTABLE_SIZE 
	);

netadr_t	net_local_adr;
double		net_time = 0.0f;	// current time, updated each frame

static	CUtlVector<netsocket_t> net_sockets;	// the sockets
static	CUtlVector<netpacket_t>	net_packets;

static	bool net_multiplayer = false;	// if true, configured for Multiplayer
static	bool net_noip = false;	// Disable IP support, can't switch to MP mode
static	bool net_nodns = false;	// Disable DNS request to avoid long timeouts
bool net_notcp = true;	// Disable TCP support
static	bool net_nohltv = false; // disable HLTV support
static	bool net_addhltv1 = false; // by default, HLTV1 (sup)port is disabled. Must be enabled explicitly with -addhltv1 cmdline parameter
#if defined( REPLAY_ENABLED )
static	bool net_noreplay = false;	// disable Replay support
#endif
static	bool net_dedicated = false;	// true is dedicated system
static	bool net_dedicatedForXbox = false; // true is dedicated system serving xbox
static	bool net_dedicatedForXboxInsecure = false; // true if dedicated system serving insecure xbox
static	int  net_error = 0;			// global error code updated with NET_GetLastError()

static int g_nFakeSocketHandle = 0;	// for when we are only using Steam. Need a fake socket handle.

volatile int g_NetChannelsRefreshCounter = 0;
static CUtlVectorMT< CUtlVector< CNetChan* > >			s_NetChannels;
static CUtlVectorMT< CUtlVector< pendingsocket_t > >	s_PendingSockets;

CTSQueue<loopback_t *> s_LoopBacks[LOOPBACK_SOCKETS];
static netpacket_t*	s_pLagData[MAX_SOCKETS];  // List of lag structures, if fakelag is set.

ISteamDatagramTransportGameserver *g_pSteamDatagramGameserver = nullptr;
ISteamDatagramTransportClient *g_pSteamDatagramClient = nullptr;
ns_address g_addrSteamDatagramProxiedGameServer;

static void CloseSteamDatagramClientConnection()
{
	if ( g_pSteamDatagramClient )
	{
		g_pSteamDatagramClient->Close();
		g_pSteamDatagramClient = nullptr;
	}
	g_addrSteamDatagramProxiedGameServer.Clear();
}

unsigned short NET_HostToNetShort( unsigned short us_in )
{
	return htons( us_in );
}

unsigned short NET_NetToHostShort( unsigned short us_in )
{
	return ntohs( us_in );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *s - 
//			*sadr - 
// Output : bool	NET_StringToSockaddr
//-----------------------------------------------------------------------------
bool NET_StringToSockaddr( const char *s, struct sockaddr *sadr )
{
	char	*colon;
	char	copy[128];
	Q_memset (sadr, 0, sizeof(*sadr));
	((struct sockaddr_in *)sadr)->sin_family = AF_INET;
	((struct sockaddr_in *)sadr)->sin_port = 0;

	Q_strncpy (copy, s, sizeof( copy ) );
	// strip off a trailing :port if present
	for (colon = copy ; *colon ; colon++)
	{
		if (*colon == ':')
		{
			*colon = 0;
			((struct sockaddr_in *)sadr)->sin_port = NET_HostToNetShort((short)atoi(colon+1));	
		}
	}
	
	if (copy[0] >= '0' && copy[0] <= '9' && Q_strstr( copy, "." ) )
	{
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
	}
	else
	{
		if ( net_nodns )
			return false;	// DNS names disabled

		struct hostent	*h;
		if ( (h = gethostbyname(copy)) == NULL )
			return false;
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
	}
	
	return true;
}

int NET_GetLastError( void )
{
#if defined( _WIN32 )
	net_error = WSAGetLastError();
#else
	net_error = errno;
#endif
	return net_error;
}

/*
==================
NET_ClearLaggedList

==================
*/
void NET_ClearLaggedList(netpacket_t **pList)
{
	netpacket_t * p = (*pList);

	while ( p )
	{
		netpacket_t * n = p->pNext;

		if ( p->data )
		{
			delete[] p->data;
			p->data = NULL;
		}
		delete p;
		p = n;
	}

	(*pList) = NULL;
}

void NET_ClearLagData( int sock )
{
	if ( sock < MAX_SOCKETS && s_pLagData[sock] )
	{
		NET_ClearLaggedList( &s_pLagData[sock] );
	}
}

/*
=============
NET_StringToAdr

localhost
idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
=============
*/
bool NET_StringToAdr ( const char *s, netadr_t *a)
{
	return a->SetFromString( s, !net_nodns );
}

void NET_FindAllNetChannelAddresses( int socket, CUtlVector< struct sockaddr > &arrNetChans )
{
	AUTO_LOCK_FM( s_NetChannels );

	int numChannels = s_NetChannels.Count();

	for ( int i = 0; i < numChannels; i++ )
	{
		CNetChan * chan = s_NetChannels[ i ];

		// sockets must match
		if ( socket != chan->GetSocket() )
			continue;

		// only ip based ones
		if ( chan->GetRemoteAddress().m_AddrType != NSAT_NETADR )
		{
			continue;
		}

		// and the IP:Port address 
		struct sockaddr sockAddress;
		chan->GetRemoteAddress().m_adr.ToSockadr( &sockAddress );
		arrNetChans.AddToTail( sockAddress );
	}
}

CNetChan *NET_FindNetChannel(int socket, const ns_address &adr )
{
	AUTO_LOCK_FM( s_NetChannels );

	int numChannels = s_NetChannels.Count();

	for ( int i = 0; i < numChannels; i++ )
	{
		CNetChan * chan = s_NetChannels[i];

		// sockets must match
		if ( socket != chan->GetSocket() )
			continue;

		// and the address 
		if ( adr == chan->GetRemoteAddress() )
		{
			return chan;	// found it
		}
	}

	return NULL;	// no channel found
}

void NET_CloseSocket( int hSocket, int sock = -1)
{
	if ( !hSocket )
		return;

	// close socket handle
	if ( !OnlyUseSteamSockets() )
	{
		// bugbug - shouldn't this clear net_sockets[sock].hUDP?
		int ret = closesocket( hSocket );
		if ( ret == -1 )
		{
			NET_GetLastError();
			ConMsg ("WARNING! NET_CloseSocket: %s\n", NET_ErrorString(net_error));
		}	

		// if hSocket mapped to hTCP, clear hTCP
		if ( sock >= 0 )
		{
			if ( net_sockets[sock].hTCP == hSocket )
			{
				net_sockets[sock].hTCP = 0;
				net_sockets[sock].bListening = false;
			}
		}

		// If closing client socket, make sure we don't keep trying
		// to talk to server
		if ( sock == NS_CLIENT )
		{
			CloseSteamDatagramClientConnection();
		}
	}

	g_pSteamSocketMgr->CloseSocket( hSocket, sock );
}

/*
====================
NET_IPSocket
====================
*/
int NET_OpenSocket ( const char *net_interface, int& port, int protocol )
{
	if ( OnlyUseSteamSockets() )
	{
		Msg ("WARNING: NET_OpenSocket: Not implemented - Should be using Steam\n");
		return 0;
	}
	
	struct sockaddr_in	address;
	unsigned int		opt;
	int					newsocket = -1;

	if ( protocol == IPPROTO_TCP )
	{
		newsocket = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );
	}
	else // as UDP or VDP
	{
		newsocket = socket( PF_INET, SOCK_DGRAM, protocol );
	}

	if ( newsocket == -1 )
	{
		NET_GetLastError(); 
		if ( net_error != WSAEAFNOSUPPORT )
			Msg ("WARNING: NET_OpenSocket: socket failed: %s", NET_ErrorString(net_error));

		return 0;
	}

	
	opt =  1; // make it non-blocking
	int ret = ioctlsocket( newsocket, FIONBIO, (unsigned long*)&opt );
	if ( ret == -1 )
	{
		NET_GetLastError();
		Msg ("WARNING: NET_OpenSocket: ioctl FIONBIO: %s\n", NET_ErrorString(net_error) );
	}
	
	if ( protocol == IPPROTO_TCP )
	{
		if ( !IsX360() ) // SO_KEEPALIVE unsupported on the 360
		{
			opt = 1; // set TCP options: keep TCP connection alive
			ret = setsockopt( newsocket, SOL_SOCKET, SO_KEEPALIVE, (char *)&opt, sizeof(opt) );
			if (ret == -1)
			{
				NET_GetLastError();		
				Msg ("WARNING: NET_OpenSocket: setsockopt SO_KEEPALIVE: %s\n", NET_ErrorString(net_error));
				return 0;
			}
		}

		linger optlinger;	// set TCP options: Does not block close waiting for unsent data to be sent
		optlinger.l_linger = 0;
		optlinger.l_onoff = 0;
		ret = setsockopt( newsocket, SOL_SOCKET, SO_LINGER, (char *)&optlinger, sizeof(optlinger) );
		if (ret == -1)
		{
			NET_GetLastError();		
			Msg ("WARNING: NET_OpenSocket: setsockopt SO_LINGER: %s\n", NET_ErrorString(net_error));
			return 0;
		}

		opt = 1; // set TCP options: Disables the Nagle algorithm for send coalescing.
		ret = setsockopt( newsocket, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, sizeof(opt) );
		if (ret == -1)
		{
			NET_GetLastError();		
			Msg ("WARNING: NET_OpenSocket: setsockopt TCP_NODELAY: %s\n", NET_ErrorString(net_error));
			return 0;
		}
	}

	// Set the send and receive buffer sizes for TCP sockets, and UDP sockets on Windows. Windows UDP
	// sockets default to 8 KB buffers which makes dataloss very common. Linux (Ubuntu 12.04 anyway)
	// UDP sockets default to 208 KB so there is no need to change the setting.
	// Use net_usesocketsforloopback 1 to use sockets for listen servers for testing.
	if ( protocol == IPPROTO_TCP || IsPlatformWindowsPC() )
	{
		const int UDP_BUFFER_SIZE = 128 * 1024; // Better than 8 KB.
		const int bufferSize = ( protocol == IPPROTO_TCP ) ? NET_MAX_MESSAGE : UDP_BUFFER_SIZE;
		opt = bufferSize;
		ret = setsockopt( newsocket, SOL_SOCKET, SO_SNDBUF, (char *)&opt, sizeof(opt) );
		if (ret == -1)
		{
			NET_GetLastError();		
			Msg ("WARNING: NET_OpenSocket: setsockopt SO_SNDBUF: %s\n", NET_ErrorString(net_error));
			return 0;
		}

		opt = bufferSize;
		ret = setsockopt( newsocket, SOL_SOCKET, SO_RCVBUF, (char *)&opt, sizeof(opt) );
		if (ret == -1)
		{
			NET_GetLastError();		
			Msg ("WARNING: NET_OpenSocket: setsockopt SO_RCVBUF: %s\n", NET_ErrorString(net_error));
			return 0;
		}
	}

	if ( protocol == IPPROTO_TCP )
	{
		return newsocket;	// don't bind TCP sockets by default
	}

	// rest is UDP only
	
	// VDP protocol (Xbox 360 secure network) doesn't support SO_BROADCAST
	if ( !IsX360() || protocol != IPPROTO_VDP )
 	{
		opt = 1; // set UDP options: make it broadcast capable
		ret = setsockopt( newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(opt) );
		if (ret == -1)
		{
			NET_GetLastError();		
			Msg ("WARNING: NET_OpenSocket: setsockopt SO_BROADCAST: %s\n", NET_ErrorString(net_error));
			return 0;
		}
	}
	
	if ( CommandLine()->FindParm( "-reuse" ) )
	{
		opt = 1; // make it reusable
		ret = setsockopt( newsocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt) );
		if (ret == -1)
		{
			NET_GetLastError();
			Msg ("WARNING: NET_OpenSocket: setsockopt SO_REUSEADDR: %s\n", NET_ErrorString(net_error));
			return 0;
		}
	}

	if (!net_interface || !net_interface[0] || !Q_strcmp(net_interface, "localhost"))
	{
		address.sin_addr.s_addr = INADDR_ANY;
	}
	else
	{
		NET_StringToSockaddr (net_interface, (struct sockaddr *)&address);
	}

	address.sin_family = AF_INET;

	int port_offset;	// try binding socket to port, try next 10 is port is already used

	int nNumTries = PORT_TRY_MAX;
	if ( IsChildProcess() )
		nNumTries = PORT_TRY_MAX_FORKED;

	// Add support for "+net_port_try" argument to override on command line
	int nCommandLineOverrideNumTries = nNumTries;
	nCommandLineOverrideNumTries = CommandLine()->ParmValue( "+net_port_try", nCommandLineOverrideNumTries );
	nCommandLineOverrideNumTries = CommandLine()->ParmValue( "-net_port_try", nCommandLineOverrideNumTries );
	if ( ( nCommandLineOverrideNumTries > 0 ) && ( nCommandLineOverrideNumTries < nNumTries ) )
		nNumTries = nCommandLineOverrideNumTries;

	for ( port_offset = 0; port_offset < nNumTries; port_offset++ )
	{
		if ( port == PORT_ANY )
		{
			address.sin_port = 0;	// = INADDR_ANY
		}
		else
		{
			address.sin_port = NET_HostToNetShort((short)( port + port_offset ));
		}

		ret = bind( newsocket, (struct sockaddr *)&address, sizeof(address) );
		if ( ret != -1 )
		{
			if ( port != PORT_ANY && port_offset != 0 )
			{
				port += port_offset;	// update port
				ConDMsg( "Socket bound to non-default port %i because original port was already in use.\n", port );
			}
			break;
		}

		NET_GetLastError();

		if ( port == PORT_ANY || net_error != WSAEADDRINUSE )
		{
			Msg ("WARNING: NET_OpenSocket: bind: %s\n", NET_ErrorString(net_error));
			NET_CloseSocket(newsocket,-1);
			return 0;
		}

		// Try next port
	}

	if ( port_offset == nNumTries )
	{
		Msg( "WARNING: UDP_OpenSocket: unable to bind socket\n" );
		NET_CloseSocket( newsocket,-1 );
		return 0;
	}
	
	return newsocket;
}

int NET_ConnectSocket( int sock, const netadr_t &addr )
{
	Assert( (sock >= 0) && (sock < net_sockets.Count()) );

	netsocket_t *netsock = &net_sockets[sock];

	if ( netsock->hTCP )
	{
		NET_CloseSocket( netsock->hTCP, sock );
	}

	if ( net_notcp )
		return 0;

	sockaddr saddr;

	addr.ToSockadr( &saddr );

	int anyport = PORT_ANY;

	netsock->hTCP = NET_OpenSocket( ipname.GetString(), anyport, true );

	if ( !netsock->hTCP )
	{
		Msg( "Warning! NET_ConnectSocket failed opening socket %i, port %i.\n", sock, net_sockets[sock].nPort );
		return false;
	}

	int ret;
	ret = connect( netsock->hTCP, &saddr, sizeof(saddr) );
	if ( ret == -1 )
	{
		NET_GetLastError();

		if ( net_error != WSAEWOULDBLOCK )
		{
			Msg ("NET_ConnectSocket: %s\n", NET_ErrorString( net_error ) );
			return 0;
		}
	}

	return net_sockets[sock].hTCP;
}

int NET_SendStream( int nSock, const char * buf, int len, int flags )
{
	if ( OnlyUseSteamSockets() )
	{
		Msg( "Warning! NET_SendStream called when only using Steam sockets\n" );
		return 0;
	}

	//int ret = send( nSock, buf, len, flags );
	int ret = send( nSock, buf, len, flags );
	if ( ret == -1 )
	{
		NET_GetLastError();

		if ( net_error == WSAEWOULDBLOCK )
		{
			return 0; // ignore EWOULDBLOCK
		}

		Msg ("NET_SendStream: %s\n", NET_ErrorString( net_error ) );
	}

	return ret;
}

int NET_ReceiveStream( int nSock, char * buf, int len, int flags )
{
	if ( OnlyUseSteamSockets() )
	{
		Msg( "Warning! NET_ReceiveStream called when only using Steam sockets\n" );
		return 0;
	}

	int ret = recv( nSock, buf, len, flags );
	if ( ret == -1 )
	{
		NET_GetLastError();

		if ( net_error == WSAEWOULDBLOCK || 
			 net_error == WSAENOTCONN )
		{
			return 0; // ignore EWOULDBLOCK
		}

		Msg ("NET_ReceiveStream: %s\n", NET_ErrorString( net_error ) );
	}

	return ret;
}

INetChannel *NET_CreateNetChannel( int socket, const ns_address *adr, const char * name, INetChannelHandler * handler, const byte *pbEncryptionKey, bool bForceNewChannel )
{
	CNetChan *chan = NULL;

	if ( !bForceNewChannel && adr != NULL )
	{
		// try to find real network channel if already existing
		if ( ( chan = NET_FindNetChannel( socket, *adr ) ) != NULL )
		{
			// channel already known, clear any old stuff before Setup wipes all
			chan->Clear();
		}
	}

	if ( !chan )
	{
		// create new channel
		chan = new CNetChan();

		AUTO_LOCK_FM( s_NetChannels );
		s_NetChannels.AddToTail( chan );
	}

	NET_ClearLagData( socket );

	// just reset and return
	ns_address adrToUse;
	if ( adr )
		adrToUse = *adr;
	else
		adrToUse.Clear();
	chan->Setup( socket, adrToUse, name, handler, pbEncryptionKey );

	++ g_NetChannelsRefreshCounter;

	return chan;
}

void NET_RemoveNetChannel(INetChannel *netchan, bool bDeleteNetChan)
{
	if ( !netchan )
	{
		return;
	}

	AUTO_LOCK_FM( s_NetChannels );
	if ( s_NetChannels.Find( static_cast<CNetChan*>(netchan) ) == s_NetChannels.InvalidIndex() )
	{
		DevMsg(1, "NET_CloseNetChannel: unknown channel.\n");
		return;
	}

	s_NetChannels.FindAndRemove( static_cast<CNetChan*>(netchan) );

	NET_ClearQueuedPacketsForChannel( netchan );
	
	if ( bDeleteNetChan )
		delete netchan;

	++ g_NetChannelsRefreshCounter;
}


/*
=============================================================================

LOOPBACK BUFFERS FOR LOCAL PLAYER

=============================================================================
*/


void NET_SendLoopPacket (int sock, int length, const unsigned char *data )
{
	// Never loop on anything other than client/server
	if ( sock != NS_CLIENT && sock != NS_SERVER )
		return;

	loopback_t	*loop;

	if ( length > NET_MAX_PAYLOAD )
	{
		DevMsg( "NET_SendLoopPacket:  packet too big (%i).\n", length );
		return;
	}

	loop = new loopback_t;

	if ( length <= DEF_LOOPBACK_SIZE )
	{
		loop->data = loop->defbuffer;
	}
	else
	{
		loop->data  = new char[ length ];
	}

	Q_memcpy (loop->data, data, length);
	loop->datalen = length;

	if ( sock == NS_SERVER )
	{
		s_LoopBacks[NS_CLIENT].PushItem( loop );
	}
	else if ( sock == NS_CLIENT )
	{
		s_LoopBacks[NS_SERVER].PushItem( loop );
	}
	else
	{
		DevMsg( "NET_SendLoopPacket:  invalid socket (%i).\n", sock );
		return;
	}
}

//=============================================================================

int NET_CountLaggedList( netpacket_t *pList )
{
	int c = 0;
	netpacket_t *p = pList;
	
	while ( p )
	{
		c++;
		p = p->pNext;
	}

	return c;
}

/*
===================
NET_AddToLagged

===================
*/
void NET_AddToLagged( netpacket_t **pList, netpacket_t *pPacket )
{
	if ( pPacket->pNext )
	{
		Msg("NET_AddToLagged::Packet already linked\n");
		return;
	}

	// first copy packet

	netpacket_t *newPacket = new netpacket_t;

	(*newPacket) = (*pPacket);  // copy packet infos
	newPacket->data = new unsigned char[ pPacket->size ];	// create new data buffer
	Q_memcpy( newPacket->data, pPacket->data, pPacket->size ); // copy packet data
	newPacket->pNext = NULL;

	// if list is empty, this is our first element
	if ( (*pList) == NULL )
	{
		(*pList) = newPacket;	// put packet in top of list
	}
	else
	{
		netpacket_t *last = (*pList);

		while ( last->pNext )
		{
			// got to end of list
			last = last->pNext;
		}

		// add at end
		last->pNext = newPacket;
	}
}

// Actual lag to use in msec
static float s_FakeLag = 0.0;

float NET_GetFakeLag()
{
	return s_FakeLag;
}

// How quickly we converge to a new value for fakelag
#define FAKELAG_CONVERGE	200  // ms per second

/*
==============================
NET_AdjustLag

==============================
*/
void NET_AdjustLag( void )
{
	// Already converged?
	if ( fakelag.GetFloat() == s_FakeLag )
		return;

	static double s_LastTime = 0;
	
	// Bound time step
	
	float dt = clamp( net_time - s_LastTime, 0.0f, 0.2f );
	
	s_LastTime = net_time;

	// Figure out how far we have to go
	float diff = fakelag.GetFloat() - s_FakeLag;

	// How much can we converge this frame
	float converge = FAKELAG_CONVERGE * dt;

	// Last step, go the whole way
	if ( converge > fabs( diff ) )
	{
		converge = fabs( diff );
	}

	// Converge toward fakelag.GetFloat()
	if ( diff < 0.0 )
	{
		// Converge toward fakelag.GetFloat()
		s_FakeLag -= converge;
	}
	else
	{
		s_FakeLag += converge;
	}
}


bool NET_LagPacket (bool newdata, netpacket_t * packet)
{
	static int losscount[MAX_SOCKETS];

	if ( packet->source >= MAX_SOCKETS )
		return newdata; // fake lag not supported for extra sockets

	if ( (droppackets.GetInt() > 0)  && newdata && (packet->source == NS_CLIENT) )
	{
		droppackets.SetValue( droppackets.GetInt() - 1 );
		return false;
	}

	if ( fakeloss.GetFloat() && newdata )
	{
		losscount[packet->source]++;

		if ( fakeloss.GetFloat() > 0.0f )
		{
			// Act like we didn't hear anything if we are going to lose the packet.
			// Depends on random # generator.
			if (RandomInt(0,100) <= (int)fakeloss.GetFloat())
				return false;
		}
		else
		{
			int ninterval;

			ninterval = (int)(fabs( fakeloss.GetFloat() ) );
			ninterval = MAX( 2, ninterval );

			if ( !( losscount[packet->source] % ninterval ) )
			{
				return false;
			}
		}
	}

	if (s_FakeLag <= 0.0)
	{
		// Never leave any old msgs around
		for ( int i=0; i<MAX_SOCKETS; i++ )
		{
			NET_ClearLagData( i );
		}
		return newdata;
	}

	// if new packet arrived in fakelag list
	if ( newdata )
	{
		NET_AddToLagged( &s_pLagData[packet->source], packet );
	}

	// Now check the correct list and feed any message that is old enough.
	netpacket_t *p = s_pLagData[packet->source]; // current packet

	if ( !p )
		return false;	// no packet in lag list

	float target = s_FakeLag;
	if ( fakejitter.GetFloat() > 0.0f )
	{
		float maxjitter = MIN( fakejitter.GetFloat(), target * 0.5f );
		target += RandomFloat( -maxjitter, maxjitter );
	}

	if ( (p->received + (target/1000.0f)) > net_time )
		return false;	// not time yet for this packet

#ifdef _DEBUG
	if ( fakeshuffle.GetInt() && p->pNext )
	{
		if ( !RandomInt( 0, fakeshuffle.GetInt() ) )
		{
			// swap p and p->next
			netpacket_t * t = p->pNext;
			p->pNext = t->pNext;
			t->pNext = p;
			p = t; 
		}
	}
#endif
	
	// remove packet p from list (is head)
	s_pLagData[packet->source] = p->pNext;
		
	// copy & adjust content
	packet->source	= p->source;	
	packet->from	= p->from;		
	packet->pNext	= NULL;			// no next
	packet->received = net_time;	// new time
	packet->size	= p->size;		
	packet->wiresize = p->wiresize;
	packet->stream	= p->stream;
			
	Q_memcpy( packet->data, p->data, p->size );

	// free lag packet
					
	delete[] p->data;
	delete p;
			
	return true;
}

// Calculate MAX_SPLITPACKET_SPLITS according to the smallest split size
#define MAX_SPLITPACKET_SPLITS ( NET_MAX_MESSAGE / MIN_SPLIT_SIZE )
#define SPLIT_PACKET_STALE_TIME		15.0f

class CSplitPacketEntry
{
public:
	CSplitPacketEntry()
	{

		int i;
		for ( i = 0; i < MAX_SPLITPACKET_SPLITS; i++ )
		{
			splitflags[ i ] = -1;
		}

		memset( &netsplit, 0, sizeof( netsplit ) );
		lastactivetime = 0.0f;
	}

public:
	ns_address		from;
	int				splitflags[ MAX_SPLITPACKET_SPLITS ];
	LONGPACKET		netsplit;
	// host_time the last time any entry was received for this entry
	float			lastactivetime;
};

typedef CUtlVector< CSplitPacketEntry > vecSplitPacketEntries_t;
static CUtlVector<vecSplitPacketEntries_t> net_splitpackets;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void NET_DiscardStaleSplitpackets( const int sock )
{
	if ( !net_splitpackets.IsValidIndex( sock ) )
		return;
	
	vecSplitPacketEntries_t &splitPacketEntries = net_splitpackets[sock];
	int i;
	for ( i = splitPacketEntries.Count() - 1; i >= 0; i-- )
	{
		CSplitPacketEntry *entry = &splitPacketEntries[ i ];
		Assert( entry );

		if ( net_time < ( entry->lastactivetime + SPLIT_PACKET_STALE_TIME ) )
			continue;

		splitPacketEntries.Remove( i );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *from - 
// Output : CSplitPacketEntry
//-----------------------------------------------------------------------------
CSplitPacketEntry *NET_FindOrCreateSplitPacketEntry( const int sock, const ns_address &from )
{
	vecSplitPacketEntries_t &splitPacketEntries = net_splitpackets[sock];
	int i, count = splitPacketEntries.Count();
	CSplitPacketEntry *entry = NULL;
	for ( i = 0; i < count; i++ )
	{
		entry = &splitPacketEntries[ i ];
		Assert( entry );

		if ( from == entry->from )
			break;
	}

	if ( i >= count )
	{
		CSplitPacketEntry newentry;
		newentry.from = from;

		splitPacketEntries.AddToTail( newentry );

		entry = &splitPacketEntries[ splitPacketEntries.Count() - 1 ];
	}

	Assert( entry );
	return entry;
}

static const tokenset_t< ESocketIndex_t > s_SocketDescMap[] =
{						 
	{ "cl",		NS_CLIENT		},                          
	{ "sv",		NS_SERVER		},                          
#ifdef _X360
	{ "Xsl",	NS_X360_SYSTEMLINK	},
	{ "Xlb",	NS_X360_LOBBY		},
	{ "Xtl",	NS_X360_TEAMLINK	},
#endif
	{ "htv",	NS_HLTV			},
	{ "htv1",	NS_HLTV1			},
#if defined( REPLAY_ENABLED )
	{ "rply",	NS_REPLAY		},
#endif
	{ NULL,		(ESocketIndex_t)-1 }
};

static char const *DescribeSocket( int sock )
{
	return s_SocketDescMap->GetNameByToken( ( ESocketIndex_t ) sock, "??" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pData - 
//			size - 
//			*outSize - 
// Output : bool
//-----------------------------------------------------------------------------
bool NET_GetLong( const int sock, netpacket_t *packet )
{
	int				packetNumber, packetCount, sequenceNumber, offset;
	short			packetID;
	SPLITPACKET		*pHeader;
	
	if ( packet->size < sizeof(SPLITPACKET) ) 
	{
		Msg( "Invalid split packet length %i\n", packet->size );
		return false;
	}

	CSplitPacketEntry *entry = NET_FindOrCreateSplitPacketEntry( sock, packet->from );
	Assert( entry );
	if ( !entry )
		return false;

	entry->lastactivetime = net_time;
	Assert( packet->from.CompareAdr( entry->from ) );

	pHeader = ( SPLITPACKET * )packet->data;
	// pHeader is network endian correct
	sequenceNumber	= LittleLong( pHeader->sequenceNumber );
	packetID		= LittleShort( (short)pHeader->packetID );
	// High byte is packet number
	packetNumber	= ( packetID >> 8 );	
	// Low byte is number of total packets
	packetCount		= ( packetID & 0xff );	

	int nSplitSizeMinusHeader = (int)LittleShort( (short)pHeader->nSplitSize );
	if ( nSplitSizeMinusHeader < MIN_SPLIT_SIZE ||
		 nSplitSizeMinusHeader > MAX_SPLIT_SIZE )
	{
		Msg( "NET_GetLong:  Split packet from %s with invalid split size (number %i/ count %i) where size %i is out of valid range [%d - %d ]\n", 
			ns_address_render( packet->from ).String(), 
			packetNumber, 
			packetCount, 
			nSplitSizeMinusHeader,
			MIN_SPLIT_SIZE,
			MAX_SPLIT_SIZE );
		return false;
	}

	if ( packetNumber >= MAX_SPLITPACKET_SPLITS ||
		 packetCount > MAX_SPLITPACKET_SPLITS )
	{
		Msg( "NET_GetLong:  Split packet from %s with too many split parts (number %i/ count %i) where %i is max count allowed\n", 
			ns_address_render( packet->from ).String(), 
			packetNumber, 
			packetCount, 
			MAX_SPLITPACKET_SPLITS );
		return false;
	}

	// First packet in split series?
	if ( entry->netsplit.currentSequence == -1 || 
		sequenceNumber != entry->netsplit.currentSequence )
	{
		entry->netsplit.currentSequence	= sequenceNumber;
		entry->netsplit.splitCount		= packetCount;
		entry->netsplit.nExpectedSplitSize = nSplitSizeMinusHeader;
	}

	if ( entry->netsplit.nExpectedSplitSize != nSplitSizeMinusHeader )
	{
		Msg( "NET_GetLong:  Split packet from %s with inconsistent split size (number %i/ count %i) where size %i not equal to initial size of %i\n", 
			ns_address_render( packet->from ).String(), 
			packetNumber, 
			packetCount, 
			nSplitSizeMinusHeader,
			entry->netsplit.nExpectedSplitSize
			);
		return false;
	}

	int size = packet->size - sizeof(SPLITPACKET);

	if ( entry->splitflags[ packetNumber ] != sequenceNumber )
	{
		// Last packet in sequence? set size
		if ( packetNumber == (packetCount-1) )
		{
			entry->netsplit.totalSize = (packetCount-1) * nSplitSizeMinusHeader + size;
		}

		entry->netsplit.splitCount--;		// Count packet
		entry->splitflags[ packetNumber ] = sequenceNumber;

		if ( net_showsplits.GetInt() && net_showsplits.GetInt() != 3 )
		{
			Msg( "<-- [%s] Split packet %4i/%4i seq %5i size %4i mtu %4i from %s\n", 
				DescribeSocket( sock ),
				packetNumber + 1, 
				packetCount, 
				sequenceNumber,
				size, 
				nSplitSizeMinusHeader + sizeof( SPLITPACKET ), 
				ns_address_render( packet->from ).String() );
		}
	}
	else
	{
		Msg( "NET_GetLong:  Ignoring duplicated split packet %i of %i ( %i bytes ) from %s\n", packetNumber + 1, packetCount, size, ns_address_render( packet->from ).String() );
	}


	// Copy the incoming data to the appropriate place in the buffer
	offset = (packetNumber * nSplitSizeMinusHeader);
	memcpy( entry->netsplit.buffer + offset, packet->data + sizeof(SPLITPACKET), size );
	
	// Have we received all of the pieces to the packet?
	if ( entry->netsplit.splitCount <= 0 )
	{
		entry->netsplit.currentSequence = -1;	// Clear packet
		if ( entry->netsplit.totalSize > sizeof(entry->netsplit.buffer) )
		{
			Msg("Split packet too large! %d bytes from %s\n", entry->netsplit.totalSize, ns_address_render( packet->from ).String() );
			return false;
		}

		Q_memcpy( packet->data, entry->netsplit.buffer, entry->netsplit.totalSize );
		packet->size = entry->netsplit.totalSize;
		packet->wiresize = entry->netsplit.totalSize;
		return true;
	}

	return false;
}


bool NET_GetLoopPacket ( netpacket_t * packet )
{
	Assert ( packet );

	loopback_t	*loop = NULL;

	if ( packet->source > NS_SERVER )
		return false;
		
	if ( !s_LoopBacks[packet->source].PopItem( &loop ) )
	{
		return false;
	}

	if (loop->datalen == 0)
	{
		// no packet in loopback buffer
		delete loop;
		return ( NET_LagPacket( false, packet ) );
	}

	// copy data from loopback buffer to packet 
	packet->from.SetAddrType( NSAT_NETADR );
	packet->from.m_adr.SetType( NA_LOOPBACK );
	packet->size = loop->datalen;
	packet->wiresize = loop->datalen;
	Q_memcpy ( packet->data, loop->data, packet->size );
	
	loop->datalen = 0; // buffer is avalibale again

	if ( loop->data != loop->defbuffer )
	{
		delete loop->data;
		loop->data = loop->defbuffer;
	}

	delete loop;

	// allow lag system to modify packet
	return ( NET_LagPacket( true, packet ) );	
}

static int NET_ReceiveRawPacket( int sock, void *buf, int len, ns_address *from )
{
	int net_socket = net_sockets[ sock ].hUDP;

	int ret = g_pSteamSocketMgr->recvfrom(net_socket, (char*)buf, len, 0, from );
	if ( ret > 0 )
		return ret;

	// Still nothing?  Check proxied clients
	if ( g_pSteamDatagramGameserver )
	{
		CSteamID remoteSteamID;
		uint64 usecTimeRecv;
		if ( sock == NS_SERVER )
		{
			ret = g_pSteamDatagramGameserver->RecvDatagram( buf, len, &remoteSteamID, &usecTimeRecv, STEAM_P2P_GAME_SERVER );
			if ( ret > 0 )
			{
	            from->SetFromSteamID( remoteSteamID, STEAM_P2P_GAME_CLIENT );
				from->m_AddrType = NSAT_PROXIED_CLIENT;
				return ret;
			}
		}
		else if ( sock == NS_HLTV )
		{
			ret = g_pSteamDatagramGameserver->RecvDatagram( buf, len, &remoteSteamID, &usecTimeRecv, STEAM_P2P_HLTV );
			if ( ret > 0 )
			{
	            from->SetFromSteamID( remoteSteamID, STEAM_P2P_GAME_CLIENT );
				from->m_AddrType = NSAT_PROXIED_CLIENT;
				return ret;
			}
		}
		else if ( sock == NS_HLTV1 )
		{
			ret = g_pSteamDatagramGameserver->RecvDatagram( buf, len, &remoteSteamID, &usecTimeRecv, STEAM_P2P_HLTV1 );
			if ( ret > 0 )
			{
	            from->SetFromSteamID( remoteSteamID, STEAM_P2P_GAME_CLIENT );
				from->m_AddrType = NSAT_PROXIED_CLIENT;
				return ret;
			}
		}
	}

	// Still nothing?  Check proxied server
	#ifndef DEDICATED
		if ( sock == NS_CLIENT && ret <= 0 && g_pSteamDatagramClient && g_addrSteamDatagramProxiedGameServer.IsValid() )
		{
			//CSteamID remoteSteamID;
			uint64 usecTimeRecv;
			int ret = g_pSteamDatagramClient->RecvDatagram( buf, len, &usecTimeRecv, STEAM_P2P_GAME_CLIENT );
			if ( ret > 0 )
			{
				*from = g_addrSteamDatagramProxiedGameServer;
				//pReceiveData->usTime = usecTimeRecv;
				return ret;
			}
		}
	#endif

	// nothing
	return 0;
}

static bool NET_ReceiveDatagram_Helper( const int sock, netpacket_t * packet, bool &bNoMorePacketsInSocketPipe )
{
	Assert ( packet );
	Assert ( net_multiplayer );

#if defined( _DEBUG ) && !defined( _PS3 )
	if ( recvpackets.GetInt() >= 0 )
	{
		unsigned long bytes;

		int net_socket = net_sockets[packet->source].hUDP;
		ioctlsocket( net_socket, FIONREAD, &bytes );

		if ( bytes <= 0 )
		{
			bNoMorePacketsInSocketPipe = true;
			return false;
		}

		if ( recvpackets.GetInt() == 0 )
		{
			bNoMorePacketsInSocketPipe = true;
			return false;
		}

		recvpackets.SetValue( recvpackets.GetInt() - 1 );
	}
#endif

	int ret = NET_ReceiveRawPacket( sock, packet->data, NET_MAX_MESSAGE, &packet->from );
	bNoMorePacketsInSocketPipe = ( ret <= 0 );
	if ( ret > 0 )
	{
		packet->wiresize = ret;

		MEM_ALLOC_CREDIT();
		CUtlMemoryFixedGrowable< byte, NET_COMPRESSION_STACKBUF_SIZE > bufVoice( NET_COMPRESSION_STACKBUF_SIZE );

		unsigned int nVoiceBits = 0u;

		if ( IsX360() || net_dedicatedForXbox )
		{
			// X360TBD: Check for voice data and forward it to XAudio
			// For now, just pull off the 2-byte VDP header and shift the data
			unsigned short nDataBytes = ( *( unsigned short * )packet->data );

			// 0xFFFF check is necessary because our LAN is broadcasting Source Engine Query requests
			// which uses the out of band header, 0xFFFFFFFF, so it's not an XBox VDP packet.
			if ( nDataBytes != 0xFFFF )
			{
				Assert( nDataBytes > 0 && nDataBytes <= ret );

				int nVoiceBytes = ret - nDataBytes - 2;
				if ( nVoiceBytes > 0 )
				{
					if ( voice_verbose.GetBool() )
					{
						Msg( "* NET_ReceiveDatagram: receiving voice from %s (%d bytes)\n", ns_address_render( packet->from ).String(), nVoiceBytes );
					}

					byte *pVoice = (byte *)packet->data + 2 + nDataBytes;

					nVoiceBits = (unsigned int)LittleShort( *( unsigned short *)pVoice );
					unsigned int nExpectedVoiceBytes = Bits2Bytes( nVoiceBits );
					pVoice += sizeof( unsigned short );
					
					CLZSS lzss;
					if ( lzss.IsCompressed( pVoice ) )
					{
						unsigned int unDecompressedVoice = lzss.GetActualSize( pVoice );
						if ( unDecompressedVoice != nExpectedVoiceBytes )
						{
							return false;
						}

						bufVoice.EnsureCapacity( unDecompressedVoice );

						// Decompress it
						unsigned int unCheck = lzss.SafeUncompress( pVoice, bufVoice.Base(), unDecompressedVoice );
						if ( unCheck != unDecompressedVoice )
						{
							return false;
						}

						nVoiceBytes = unDecompressedVoice;
					}
					else
					{
						bufVoice.EnsureCapacity( nVoiceBytes );
						Q_memcpy( bufVoice.Base(), pVoice, nVoiceBytes );
					}
				}

				Q_memmove( packet->data, &packet->data[2], nDataBytes );

				ret = nDataBytes;
			}
		}

		packet->size = ret;
		

		if ( ret < NET_MAX_MESSAGE )
		{
			// Check for split message
			if ( LittleLong( *(int *)packet->data ) == NET_HEADER_FLAG_SPLITPACKET )	
			{
				if ( !NET_GetLong( sock, packet ) )
					return false;
			}

			// Now check if the data on the wire is encrypted?
			CUtlMemoryFixedGrowable< byte, NET_COMPRESSION_STACKBUF_SIZE > memDecryptedAll( NET_COMPRESSION_STACKBUF_SIZE );
			if ( LittleLong( *(int *)packet->data ) != CONNECTIONLESS_HEADER )
			{
				// If the channel has encryption then decrypt the packet
				CNetChan * chan = NET_FindNetChannel( sock, packet->from );
				if ( !chan )
					return false;	// this is not an error during connect/disconnect, but non-connectionless packets must have a channel to process anyways

				if ( const unsigned char *pubEncryptionKey = chan->GetChannelEncryptionKey() )
				{
					// Decrypt the packet
					IceKey iceKey( 2 );
					iceKey.set( pubEncryptionKey );

					if ( ( packet->size % iceKey.blockSize() ) == 0 )
					{
						// Decrypt the message
						memDecryptedAll.EnsureCapacity( packet->size );
						unsigned char *pchCryptoBuffer = ( unsigned char * ) stackalloc( iceKey.blockSize() );
						for ( int k = 0; k < ( int ) packet->size; k += iceKey.blockSize() )
						{
							iceKey.decrypt( ( const unsigned char * ) ( packet->data + k ), pchCryptoBuffer );
							Q_memcpy( memDecryptedAll.Base() + k, pchCryptoBuffer, iceKey.blockSize() );
						}

						// Check how much random fudge we have
						int numRandomFudgeBytes = *memDecryptedAll.Base();
						if ( ( numRandomFudgeBytes > 0 ) && ( int( numRandomFudgeBytes + 1 + sizeof( int32 ) ) < packet->size ) )
						{
							// Fetch the size of the encrypted message
							int32 numBytesWrittenWire = 0;
							Q_memcpy( &numBytesWrittenWire, memDecryptedAll.Base() + 1 + numRandomFudgeBytes, sizeof( int32 ) );
							int32 const numBytesWritten = BigLong( numBytesWrittenWire );	// byteswap from the wire

							// Make sure the total size of the message matches the expectations
							if ( int( numRandomFudgeBytes + 1 + sizeof( int32 ) +numBytesWritten ) == packet->size )
							{
								// Fix the packet to point at decrypted data!
								packet->size = numBytesWritten;
								Q_memcpy( packet->data, memDecryptedAll.Base() + 1 + numRandomFudgeBytes + sizeof( int32 ), packet->size );
							}
						}
					}
				}
			}
			
			// Next check for compressed message
			if ( LittleLong( *(int *)packet->data) == NET_HEADER_FLAG_COMPRESSEDPACKET )
			{
				byte *pCompressedData = packet->data + sizeof( unsigned int );

				CLZSS lzss;
				// Decompress
				int actualSize = lzss.GetActualSize( pCompressedData );
				if ( actualSize <= 0 || actualSize > NET_MAX_PAYLOAD )
					return false;

				MEM_ALLOC_CREDIT();
				CUtlMemoryFixedGrowable< byte, NET_COMPRESSION_STACKBUF_SIZE > memDecompressed( NET_COMPRESSION_STACKBUF_SIZE );
				memDecompressed.EnsureCapacity( actualSize );

				unsigned int uDecompressedSize = lzss.SafeUncompress( pCompressedData, memDecompressed.Base(), actualSize );
				if ( uDecompressedSize == 0 || ((unsigned int)actualSize) != uDecompressedSize )
				{
					return false;
				}

				// packet->wiresize is already set
				Q_memcpy( packet->data, memDecompressed.Base(), uDecompressedSize );

				packet->size = uDecompressedSize;
			}

			if ( nVoiceBits > 0 )
			{
				// 9th byte is flag byte
				byte flagByte = *( (byte *)packet->data + sizeof( unsigned int ) + sizeof( unsigned int ) );
				unsigned int unPacketBits = packet->size << 3;
				int nPadBits = DECODE_PAD_BITS( flagByte );
				unPacketBits -= nPadBits;

				//check the CRC value in the original data packet
				if( ShouldChecksumPackets() )  
				{
					//we still want to honor the old checksum so we need to do it here instead of the usual location in CNetChan::ProcessPacketHeader
					//If the layout of the header ever changes this code will need to be updated.
					int checkSumByteOffset = sizeof( unsigned int ) + sizeof( unsigned int ) + sizeof( byte );
					packet->message.Seek( checkSumByteOffset << 3 );
					int oldChecksum = packet->message.ReadUBitLong( 16 );
					packet->message.Seek(0);

					int rawDataByteOffset = sizeof( unsigned int ) + sizeof( unsigned int ) + sizeof( byte ) + sizeof( unsigned short );
					void *pvData = packet->data + rawDataByteOffset;
					int nCheckSumBytes = packet->size - rawDataByteOffset;
					if ( nCheckSumBytes <= 0 || nCheckSumBytes > NET_MAX_PAYLOAD )
					{
						ConMsg ( "corrupted packet detected (checksumbytes %d)\n", nCheckSumBytes );
						return false;
					}

					unsigned short usDataCheckSum = BufferToShortChecksum( pvData, nCheckSumBytes );
					if ( usDataCheckSum != oldChecksum )
					{
						ConMsg ( "corrupted packet detected\n" );
						return false;
					}
				}

				// create the combined gamedata + voicedata packet
				bf_write fixup;
				fixup.SetDebugName( "X360 Fixup" );
				fixup.StartWriting( packet->data, NET_MAX_MESSAGE, unPacketBits );
				fixup.WriteBits( bufVoice.Base(), nVoiceBits );

				// Make sure we have enough bits to read a final net_NOP opcode before compressing 
				int nRemainingBits = fixup.GetNumBitsWritten() % 8;
				if ( nRemainingBits > 0 &&  nRemainingBits <= (8-NETMSG_TYPE_BITS) )
				{
					CNETMsg_NOP_t nop;
					nop.WriteToBuffer( fixup );
				}

				packet->size = fixup.GetNumBytesWritten();

				//recompute the new CRC value in the header.
				if( ShouldChecksumPackets() )
				{
					//CNetChan::ProcessPacketHeader will still be looking for the checksum so we need to generate one that will keep it happy.
					int checkSumByteOffset = sizeof( unsigned int ) + sizeof( unsigned int ) + sizeof( byte );
					fixup.SeekToBit( checkSumByteOffset << 3 );//seek to bit position of checksum

					int rawDataByteOffset = sizeof( unsigned int ) + sizeof( unsigned int ) + sizeof( byte ) + sizeof( unsigned short );
					void *pvData = packet->data + rawDataByteOffset;
					int nCheckSumBytes = packet->size - rawDataByteOffset;
					unsigned short newChecksum = BufferToShortChecksum( pvData, nCheckSumBytes );

					fixup.WriteUBitLong( newChecksum, 16 );
				}
			}

			return NET_LagPacket( true, packet );
		}
		else
		{
			ConDMsg ( "NET_ReceiveDatagram:  Oversize packet from %s\n", ns_address_render( packet->from ).String() );
		}
	}
	else if ( ret == -1  )									// error?
	{
		NET_GetLastError();

		switch ( net_error )
		{
		case WSAEWOULDBLOCK:
		case WSAECONNRESET:
		case WSAECONNREFUSED:
			break;
		case WSAEMSGSIZE:
			ConDMsg ("NET_ReceivePacket: %s\n", NET_ErrorString(net_error));
			break;
		default:
			// Let's continue even after errors
			ConDMsg ("NET_ReceivePacket: %s\n", NET_ErrorString(net_error));
			break;
		}
	}

	return false;
}

#define NET_WS_PACKET_PROFILE 0
#if NET_WS_PACKET_PROFILE
static uint64 g_nSockUDPTotalGood = 0;
static uint64 g_nSockUDPTotalBad = 0;
static uint64 g_nSockUDPTotalProcess = 0;
#define NET_WS_PACKET_STAT( sock, var ) if ( sv.IsActive() ) { if ( sock == NS_SERVER ) ++ var; } else { if ( sock == NS_CLIENT ) ++ var; }
CON_COMMAND( net_show_packet_stats, "Displays UDP packet statistics and resets the counters\n" )
{
	Msg( "UDP processed: %llu pumps, %llu good pkts, %llu bad pkts.\n", g_nSockUDPTotalProcess, g_nSockUDPTotalGood, g_nSockUDPTotalBad );
	Msg( "UDP rate: %.6f good pkt/pmp, %.6f bad pkt/pmp.\n", double( g_nSockUDPTotalGood )/double( MAX( g_nSockUDPTotalProcess, 1 ) ), double( g_nSockUDPTotalBad )/double( MAX( g_nSockUDPTotalProcess, 1 ) ) );
	g_nSockUDPTotalGood = 0;
	g_nSockUDPTotalBad = 0;
	g_nSockUDPTotalProcess = 0;
}
#else
#define NET_WS_PACKET_STAT( sock, var )
#endif
bool NET_ReceiveDatagram ( const int sock, netpacket_t * packet )
{
	for ( ;; )
	{
		bool bNoMorePacketsInSocketPipe = true;
		bool bFoundGoodPacket = NET_ReceiveDatagram_Helper( sock, packet, bNoMorePacketsInSocketPipe );
		if ( bFoundGoodPacket )
		{
			NET_WS_PACKET_STAT( sock, g_nSockUDPTotalGood );
			return true;
		}
		if ( bNoMorePacketsInSocketPipe )
		{
			return false;
		}
		else
		{
			NET_WS_PACKET_STAT( sock, g_nSockUDPTotalBad );
			// continue, this was a bad code that in old networking code would cause a packet processing hitch
		}
	}
}

netpacket_t *NET_GetPacket (int sock, byte *scratch )
{
	if ( !net_packets.IsValidIndex( sock ) )
		return NULL;
	
	// Each socket has its own netpacket to allow multithreading
	netpacket_t &inpacket = net_packets[sock];

	NET_AdjustLag();
	NET_DiscardStaleSplitpackets( sock );

	// setup new packet
	inpacket.from.Clear();
	inpacket.received = net_time;
	inpacket.source = sock;	
	inpacket.data = scratch;
	inpacket.size = 0;
	inpacket.wiresize = 0;
	inpacket.pNext = NULL;
	inpacket.message.SetDebugName("inpacket.message");

	// Check loopback first
	if ( !NET_GetLoopPacket( &inpacket ) )
	{
#ifdef PORTAL2
		extern IVEngineClient *engineClient;
		// PORTAL2-specific hack for console perf - don't waste time reading from the actual socket (expensive Steam code)
		if ( !NET_IsMultiplayer() || engineClient->IsSplitScreenActive() 
			|| ( !IsGameConsole() && sv.IsActive() && !sv. IsMultiplayer() ) )
#else // PORTAL2
		if ( !NET_IsMultiplayer() )
#endif // !PORTAL2
		{
			return NULL;
		}

		// then check UDP data 
		if ( !NET_ReceiveDatagram( sock, &inpacket ) )
		{
			// at last check if the lag system has a packet for us
			if ( !NET_LagPacket (false, &inpacket) )
			{
				return NULL;	// we don't have any new packet
			}
		}
	}
	
	Assert ( inpacket.size ); 

#ifdef _DEBUG
	if ( fakenoise.GetInt() > 0 )
	{
		COM_AddNoise( inpacket.data, inpacket.size, fakenoise.GetInt() );
	}
#endif
	
	// prepare bitbuffer for reading packet with new size
	inpacket.message.StartReading( inpacket.data, inpacket.size );

	return &inpacket;
}

void NET_ProcessPending( void )
{
	AUTO_LOCK_FM( s_PendingSockets );
	for ( int i=0; i<s_PendingSockets.Count();i++ )
	{
		pendingsocket_t * psock = &s_PendingSockets[i];

		ALIGN4 char	headerBuf[5] ALIGN4_POST;

		if ( (net_time - psock->time) > TCP_CONNECT_TIMEOUT )
		{
			NET_CloseSocket( psock->newsock );
			s_PendingSockets.Remove( i );
			continue;
		}

		int ret = NET_ReceiveStream( psock->newsock, headerBuf, sizeof(headerBuf), 0 );

		if ( ret == 0 )
		{
			continue;	// nothing received
		}
		else if ( ret == -1 )
		{
			NET_CloseSocket( psock->newsock );
			s_PendingSockets.Remove( i );
			continue;	// connection closed somehow
		}
		
		bf_read		header( headerBuf, sizeof(headerBuf) );

		int cmd = header.ReadByte();
		unsigned long challengeNr = header.ReadLong();
		bool bOK = false;	

		if ( cmd == STREAM_CMD_ACKN )
		{
			AUTO_LOCK_FM( s_NetChannels );
			for ( int j = 0; j < s_NetChannels.Count(); j++ )
			{
				CNetChan * chan = s_NetChannels[j];

				if ( chan->GetSocket() != psock->netsock )
					continue;

				if ( challengeNr == chan->GetChallengeNr() && !chan->m_StreamSocket )
				{
					if ( psock->addr.CompareAdr( chan->remote_address.AsType<netadr_t>(), true ) )
					{
						chan->m_StreamSocket = psock->newsock;
						chan->m_StreamActive = true;
						
						chan->ResetStreaming();

						bOK = true;

						if ( net_showtcp.GetInt() )
						{
							Msg ("TCP <- %s: connection accepted\n", psock->addr.ToString() );
						}
						
						break;
					}
					else
					{
						Msg ("TCP <- %s: IP address mismatch.\n", psock->addr.ToString() );
					}
				}
			}
		}

		if ( !bOK )
		{
			Msg ("TCP <- %s: invalid connection request.\n", psock->addr.ToString() );
			NET_CloseSocket( psock->newsock );
		}

		s_PendingSockets.Remove( i );
	}
}

void NET_ProcessListen(int sock)
{
	netsocket_t * netsock = &net_sockets[sock];
		
	if ( !netsock->bListening || OnlyUseSteamSockets() )
		return;

	sockaddr sa;
	int nLengthAddr = sizeof(sa);
		
	int newSocket = accept( netsock->hTCP, &sa, (socklen_t*)&nLengthAddr );
	if ( newSocket == -1 )
	{
		NET_GetLastError();

		if ( net_error != WSAEWOULDBLOCK )
		{
			ConDMsg ("NET_ThreadListen: %s\n", NET_ErrorString(net_error));
		}
		return;
	}

	// new connection TCP request, put in pending queue

	pendingsocket_t psock;

	psock.newsock = newSocket;
	psock.netsock = sock;
	psock.addr.SetFromSockadr( &sa );
	psock.time = net_time;

	AUTO_LOCK_FM( s_PendingSockets );
	s_PendingSockets.AddToTail( psock );

	// tell client to send challenge number to identify

	char authcmd = STREAM_CMD_AUTH;

	NET_SendStream( newSocket, &authcmd, 1 , 0 );	

	if ( net_showtcp.GetInt() )
	{
		Msg ("TCP <- %s: connection request.\n", psock.addr.ToString() );
	}
}

void NET_ProcessSocket( int sock, IConnectionlessPacketHandler *handler )
{
	class CAutoNetProcessSocketStartEnd
	{
	public:
		CAutoNetProcessSocketStartEnd( int sock ) : m_sock( sock )
		{
			extern void On_NET_ProcessSocket_Start( int hUDP, int sock );
			On_NET_ProcessSocket_Start( net_sockets[m_sock].hUDP, m_sock );
			NET_WS_PACKET_STAT( sock, g_nSockUDPTotalProcess );
		}
		~CAutoNetProcessSocketStartEnd()
		{
			extern void On_NET_ProcessSocket_End( int hUDP, int sock );
			On_NET_ProcessSocket_End( net_sockets[m_sock].hUDP, m_sock );
		}
	private:
		int m_sock;
	}
	autoThreadSockController( sock );

	netpacket_t * packet;
	
	//Assert ( (sock >= 0) && (sock<net_sockets.Count()) );

	// Scope for the auto_lock
	{
		AUTO_LOCK_FM( s_NetChannels );

		// get streaming data from channel sockets
		int numChannels = s_NetChannels.Count();

		for ( int i = (numChannels-1); i >= 0 ; i-- )
		{
			CNetChan *netchan = s_NetChannels[i];

			// sockets must match
			if ( sock != netchan->GetSocket() )
				continue;

			if ( !netchan->ProcessStream() )
			{
				netchan->GetMsgHandler()->ConnectionCrashed("TCP connection failed.");
			}
		}
	}

	// now get datagrams from sockets
	net_scratchbuffer_t scratch;
	while ( ( packet = NET_GetPacket ( sock, scratch.GetBuffer() ) ) != NULL )
	{
		if ( Filter_ShouldDiscard ( packet->from ) )	// filtering is done by network layer
		{
			Filter_SendBan( packet->from );	// tell them we aren't listening...
			continue;
		} 

		// check for connectionless packet (0xffffffff) first
		if ( LittleLong( *(unsigned int *)packet->data ) == CONNECTIONLESS_HEADER )
		{
			packet->message.ReadLong();	// read the -1

			if ( net_showudp.GetInt() && net_showudp_oob.GetInt() )
			{
				Msg("UDP <- %s: sz=%d OOB '0x%02X' wire=%d\n", ns_address_render( packet->from ).String(), packet->size, packet->data[4], packet->wiresize );
//				for ( int k = 0; k < packet->size; ++ k )
//					Msg( " %02X", packet->data[k] );
//				Msg( "\n" );
//				for ( int k = 0; k < packet->size; ++ k )
//					Msg( "  %c", (packet->data[k] >= 32 && packet->data[k] < 127) ? packet->data[k] : '*' );
//				Msg( "\n" );
			}

			handler->ProcessConnectionlessPacket( packet );
			continue;
		}

		// check for packets from connected clients
		
		CNetChan * netchan = NET_FindNetChannel( sock, packet->from );

		if ( netchan )
		{
			netchan->ProcessPacket( packet, true );
		}
		/* else	// Not an error that may happen during connect or disconnect
		{
			Msg ("Sequenced packet without connection from %s\n" , ns_address_render( packet->from ).String() );
		}*/
	}
}

void NET_LogBadPacket(netpacket_t * packet)
{
	FileHandle_t fp;
	int i = 0;
	char filename[ MAX_OSPATH ];
	bool done = false;

	while ( i < 1000 && !done )
	{
		Q_snprintf( filename, sizeof( filename ), "badpacket%03i.dat", i );
		fp = g_pFileSystem->Open( filename, "rb" );
		if ( !fp )
		{
			fp = g_pFileSystem->Open( filename, "wb" );
			g_pFileSystem->Write( packet->data, packet->size, fp );
			done = true;
		}
		if ( fp )
		{
			g_pFileSystem->Close( fp );
		}
		i++;
	}

	if ( i < 1000 )
	{
		Msg( "Error buffer for %s written to %s\n", ns_address_render( packet->from ).String(), filename );
	}
	else
	{
		Msg( "Couldn't write error buffer, delete error###.dat files to make space\n" );
	}
}

static int NET_SendRawPacket( SOCKET s, const void *buf, int len, const ns_address &to )
{
	switch ( to.m_AddrType )
	{
		case NSAT_NETADR:
			return g_pSteamSocketMgr->sendto( s, (const char *)buf, len, 0, to );

		//case NSAT_P2P:
		//{
		//	Assert( socket.m_pSteamNetworking );
		//	if ( !socket.m_pSteamNetworking )
		//		return -1;
		//	if ( socket.m_pSteamNetworking->SendP2PPacket( to.m_steamID.GetSteamID(), buf, len, k_EP2PSendUnreliable, to.m_steamID.GetSteamChannel() ) )
		//		return length;
		//}
		//break;

		case NSAT_PROXIED_GAMESERVER:
		{
			if ( !g_pSteamDatagramClient )
			{
				Assert( false );
				Warning( "Tried to send packet to proxied gameserver, but no ISteamDatagramTransportClient\n" );
				return -1;
			}
			if ( to != g_addrSteamDatagramProxiedGameServer )
			{
				Assert( false );
				Warning( "Tried to send packet to proxied gameserver %s, but client is currently pointed at gameserver %s\n", ns_address_render( to ).String(), ns_address_render( g_addrSteamDatagramProxiedGameServer ).String() );
				return -1;
			}

			EResult result = g_pSteamDatagramClient->SendDatagram( buf, len, to.m_steamID.GetSteamChannel() );
			if ( result == k_EResultOK || result == k_EResultNoConnection )
				return len;
		}
		break;

		case NSAT_PROXIED_CLIENT:
		{
			if ( !g_pSteamDatagramGameserver )
			{
				Assert( false );
				Warning( "Tried to send packet to proxied client, but no ISteamDatagramTransportGameserver\n" );
				return -1;
			}

			EResult result = g_pSteamDatagramGameserver->SendDatagram( buf, len, to.m_steamID.GetSteamID(), to.m_steamID.GetSteamChannel() );
			if ( result == k_EResultOK )
				return len;

		}
		break;
	}

	Warning( "Attempt to send to unknown address type %d\n", to.m_AddrType );
	Assert( false );
	return -1;
}

int NET_SendToImpl( SOCKET s, const char * buf, int len, const ns_address &to, int iGameDataLength )
{
	int nSend = 0;
	if ( IsX360() || net_dedicatedForXbox )
	{
		// 360 uses VDP protocol to piggyback voice data across the network.
		// [cbGameData][GameData][VoiceData] 
		// cbGameData is a two-byte prefix that contains the number of game data bytes in native order.
		// XLSP servers (the only cross-platform communication possible with a secure network)
		// swaps the header at the SG, decrypts the GameData and then forwards the packet to the title server.
		Assert( len < (unsigned short)-1 );
		const unsigned short nDataBytes = iGameDataLength == -1 ? len : iGameDataLength;

		if ( voice_xsend_debug.GetBool() && iGameDataLength >= 0 && iGameDataLength != len )
		{
			DevMsg( "XVoice: VDP packet to %d with unencrypted %d bytes out of %d bytes\n", s, len - iGameDataLength, len );
		}
		
		const int nVDPHeaderBytes = 2;

		if ( !to.IsType<netadr_t>() )
		{
			Warning( "NET_SendToImpl - cannot send to non-IP address %s\n", ns_address_render( to ).String() );
			return -1;
		}

#if defined( _WIN32 )

		sockaddr sadrto;
		to.AsType<netadr_t>().ToSockadr( &sadrto );

 		WSABUF buffers[2];
		buffers[0].len = nVDPHeaderBytes;
		buffers[0].buf = (char*)&nDataBytes;
		buffers[1].len = len;
		buffers[1].buf = const_cast<char*>( buf );

		if ( nDataBytes < len && voice_verbose.GetBool() )
		{
			Msg( "* NET_SendToImpl: sending voice to %s (%d bytes)\n", ns_address_render( to ).String(), len - nDataBytes );
		}

		WSASendTo( s, buffers, 2, (DWORD*)&nSend, 0, &sadrto, sizeof(sadrto), NULL, NULL );
#else
		//!!perf!! use linux sendmsg for gather similar to WSASendTo http://linux.die.net/man/3/sendmsg
		uint8 *pData = ( uint8 * ) stackalloc( nVDPHeaderBytes + len );
		memcpy( pData, &nDataBytes, nVDPHeaderBytes );
		memcpy( pData + nVDPHeaderBytes, buf, len );
		nSend = NET_SendRawPacket( s, ( const char * ) pData, nVDPHeaderBytes + len, to );
#endif
	}
	else
	{
		nSend = NET_SendRawPacket( s, buf, len, to );
	}

	return nSend;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sock - 
//			s - 
//			buf - 
//			len - 
//			flags - 
//			to - 
//			tolen - 
// Output : int
//-----------------------------------------------------------------------------
bool CL_IsHL2Demo();
bool CL_IsPortalDemo();
static int NET_SendTo( bool verbose, SOCKET s, const char * buf, int len, const ns_address &to, int iGameDataLength )
{	
	int nSend = 0;
	
	
	// If it's 0.0.0.0:0, then it's a fake player + sv_stressbots and we've plumbed everything all 
	// the way through here, where we finally bail out.
	if ( to.IsNull() )
	{		
		return len;
	}

	// Don't send anything out in VCR mode.. it just annoys other people testing in multiplayer.
#ifndef DEDICATED
	if ( ( CL_IsHL2Demo() || CL_IsPortalDemo() ) && !net_dedicated )
	{
		Error( "NET_SendTo: Error" );
	}
#endif // _WIN32

	nSend = NET_SendToImpl
	( 
		s, 
		buf,
		len,
		to, 
		iGameDataLength 
	);

#if defined( _DEBUG )
	if ( verbose && 
		( nSend > 0 ) && 
		( len > MAX_ROUTABLE_PAYLOAD ) )
	{
		ConDMsg( "NET_SendTo:  Packet length (%i) > (%i) bytes\n", len, MAX_ROUTABLE_PAYLOAD );
	}
#endif
	return nSend;
}

#if defined( _DEBUG )

#include "filesystem.h"
#include "filesystem_engine.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *NET_GetDebugFilename( char const *prefix )
{
	static char filename[ MAX_OSPATH ];

	int i;

	for ( i = 0; i < 10000; i++ )
	{
		Q_snprintf( filename, sizeof( filename ), "debug/%s%04i.dat", prefix, i );
		if ( g_pFileSystem->FileExists( filename ) )
			continue;

		return filename;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//			*buf - 
//			len - 
//-----------------------------------------------------------------------------
void NET_StorePacket( char const *filename, byte const *buf, int len )
{
	FileHandle_t fh;

	g_pFileSystem->CreateDirHierarchy( "debug/", "DEFAULT_WRITE_PATH" );
	fh = g_pFileSystem->Open( filename, "wb" );
	if ( FILESYSTEM_INVALID_HANDLE != fh )
	{
		g_pFileSystem->Write( buf, len, fh );
		g_pFileSystem->Close( fh );
	}
}

#endif // _DEBUG

struct SendQueueItem_t
{
	SendQueueItem_t() :
		m_pChannel( NULL ),
		m_Socket( (SOCKET)-1 )
	{
	}

	CNetChan	*m_pChannel;
	SOCKET		m_Socket;
	CUtlBuffer	m_Buffer;
	ns_address	m_To;
};

struct SendQueue_t
{
	SendQueue_t() : 
		m_nHostFrame( 0 )
	{
	}
	int									m_nHostFrame;
	CUtlLinkedList< SendQueueItem_t >	m_SendQueue;
};

static SendQueue_t g_SendQueue;

static int NET_QueuePacketForSend( CNetChan *chan, bool verbose, SOCKET s, const char *buf, int len, const ns_address &to, uint32 msecDelay )
{
	// If net_queued_packet_thread was -1 at startup, then we don't even have a thread.
	if ( net_queued_packet_thread.GetInt() && g_pQueuedPackedSender->IsRunning() )
	{
		g_pQueuedPackedSender->QueuePacket( chan, s, buf, len, to, msecDelay );
	}
	else
	{
		Assert( chan );
		// Set up data structure
		SendQueueItem_t *sq = &g_SendQueue.m_SendQueue[ g_SendQueue.m_SendQueue.AddToTail() ];
		sq->m_Socket = s;
		sq->m_pChannel = chan;
		sq->m_Buffer.Put( (const void *)buf, len );
		sq->m_To = to;
		sq->m_pChannel->IncrementQueuedPackets();
	}
	
	return len;
}

void NET_SendQueuedPacket( SendQueueItem_t *sq )
{
	// Msg( "Send queued packet %d\n", sq->m_Buffer.TellPut() );
	NET_SendTo
	( 
		false, 
		sq->m_Socket, 
		( const char FAR * )sq->m_Buffer.Base(), 
		sq->m_Buffer.TellPut(), 
		sq->m_To,
		-1
	);

	sq->m_pChannel->DecrementQueuedPackets();
}

void NET_ClearQueuedPacketsForChannel( INetChannel *channel )
{
	CUtlLinkedList< SendQueueItem_t >& list = g_SendQueue.m_SendQueue;

	for ( unsigned short i = list.Head(); i != list.InvalidIndex();  )
	{
		unsigned short n = list.Next( i );
		SendQueueItem_t &e = list[ i ];
		if ( e.m_pChannel == channel )
		{
			list.Remove( i );
		}
		i = n;
	}
}

void NET_SendQueuedPackets()
{
	// Only do this once per frame
	if ( host_framecount == g_SendQueue.m_nHostFrame )
		return;
	g_SendQueue.m_nHostFrame = host_framecount;

	CUtlLinkedList< SendQueueItem_t >& list = g_SendQueue.m_SendQueue;

	int nRemaining = net_splitrate.GetInt();
	while ( nRemaining )
	{
		if ( list.IsValidIndex( list.Head() ) )
		{
			SendQueueItem_t *sq = &list[ list.Head() ];
			NET_SendQueuedPacket( sq );
			list.Remove( list.Head() );
			--nRemaining;
		}
		else
		{
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sock - 
//			s - 
//			buf - 
//			len - 
//			flags - 
//			to - 
//			tolen - 
// Output : int
//-----------------------------------------------------------------------------
static volatile int32 s_SplitPacketSequenceNumber[ MAX_SOCKETS ] = {1};

static int NET_SendLong( INetChannel *chan, int sock, SOCKET s, const char * buf, int len, const ns_address &to, int nMaxRoutableSize )
{
	CNetChan *netchan = dynamic_cast< CNetChan * >( chan );

	short nSplitSizeMinusHeader = nMaxRoutableSize - sizeof( SPLITPACKET );

	int nSequenceNumber = -1;
	if ( netchan )
	{
		nSequenceNumber = netchan->IncrementSplitPacketSequence();
	}
	else
	{
		nSequenceNumber = ThreadInterlockedIncrement( &s_SplitPacketSequenceNumber[ sock ] );
	}

	const char *sendbuf = buf;
	int sendlen = len;

	char			packet[ MAX_ROUTABLE_PAYLOAD ];
	SPLITPACKET		*pPacket = (SPLITPACKET *)packet;

	// Make pPacket data network endian correct
	pPacket->netID = LittleLong( NET_HEADER_FLAG_SPLITPACKET );
	pPacket->sequenceNumber = LittleLong( nSequenceNumber );
	pPacket->nSplitSize = LittleShort( nSplitSizeMinusHeader );
	
	int nPacketCount = (sendlen + nSplitSizeMinusHeader - 1) / nSplitSizeMinusHeader;

#if defined( _DEBUG )
	if ( net_savelargesplits.GetInt() != -1 && nPacketCount >= net_savelargesplits.GetInt() )
	{
		char const *filename = NET_GetDebugFilename( "splitpacket" );
		if ( filename )
		{
			Msg( "Saving split packet of %i bytes and %i packets to file %s\n",
				sendlen, nPacketCount, filename );

			NET_StorePacket( filename, (byte const *)sendbuf, sendlen );
		}
		else
		{
			Msg( "Too many files in debug directory, clear out old data!\n" );
		}
	}
#endif

	int nBytesLeft = sendlen;
	int nPacketNumber = 0;
	int nTotalBytesSent = 0;
	int nFragmentsSent = 0;

	while ( nBytesLeft > 0 )
	{
		int size = MIN( nSplitSizeMinusHeader, nBytesLeft );

		pPacket->packetID = LittleShort( (short)(( nPacketNumber << 8 ) + nPacketCount) );
		
		Q_memcpy( packet + sizeof(SPLITPACKET), sendbuf + (nPacketNumber * nSplitSizeMinusHeader), size );
		
		int ret = 0;

		// Setting net_queued_packet_thread to NET_QUEUED_PACKET_THREAD_DEBUG_VALUE goes into a mode where all packets are queued.. can be used to stress-test it.
		// Linux threads aren't prioritized well enough for this to work well (i.e. the queued packet thread doesn't get enough
		// attention to flush itself well). The behavior the queue fixes is that if you send too many DP packets
		// without giving up your timeslice, it'll just discard the 7th and later packets until you Sleep() (issue might be on client recipient side, need to
		// snif packets to double check)

		if ( netchan && (nFragmentsSent >= net_splitrate.GetInt() || net_queued_packet_thread.GetInt() == NET_QUEUED_PACKET_THREAD_DEBUG_VALUE) )
		{
			// Don't let this rate get too high or user's won't be able to receive all of the parts
			// since they'll be too close together.
			// This should use the same rate as is used elsewhere so that we throttle all sends to the
			// same delays. Note that setting the socket's send/receive buffers to a larger size helps
			// to minimize the issues. However unthrottled UDP is still a bad thing.

			float flMaxSplitpacketDataRateBytesPerSecond = (float)netchan->GetDataRate();

			// Calculate the delay (measured from now) for when this packet should be sent.
			uint32 delay = (int)( 1000.0f * ( (float)( nPacketNumber * ( nMaxRoutableSize + UDP_HEADER_SIZE ) ) / flMaxSplitpacketDataRateBytesPerSecond ) + 0.5f );

			ret = NET_QueuePacketForSend( netchan, false, s, packet, size + sizeof(SPLITPACKET), to, delay );
		}
		else
		{
			// Also, we send the first packet no matter what
			// w/o a netchan, if there are too many splits, its possible the packet can't be delivered.  However, this would only apply to out of band stuff like
			//  server query packets, which should never require splitting anyway.
			ret = NET_SendTo( false, s, packet, size + sizeof(SPLITPACKET), to, -1 );
		}

		// First split send
		++nFragmentsSent;

		if ( ret < 0 )
		{
			return ret;
		}

		if ( ret >= size )
		{
			nTotalBytesSent += size;
		}

		nBytesLeft -= size;
		++nPacketNumber;

		// Always bitch about split packets in debug
		if ( net_showsplits.GetInt() && net_showsplits.GetInt() != 2 )
		{
			Msg( "--> [%s] Split packet %4i/%4i seq %5i size %4i mtu %4i to %s [ total %4i ]\n",
				DescribeSocket( sock ),
				nPacketNumber, 
				nPacketCount, 
				nSequenceNumber,
				size,
				nMaxRoutableSize,
				ns_address_render( to ).String(),
				sendlen );
		}
	}
	
	return nTotalBytesSent;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sock - 
//			length - 
//			*data - 
//			to - 
// Output : void NET_SendPacket
//-----------------------------------------------------------------------------

int NET_SendPacket ( INetChannel *chan, int sock,  const ns_address &to, const unsigned char *data, int length, bf_write *pVoicePayload /* = NULL */, bool bUseCompression /*=false*/, uint32 unMillisecondsDelay /*=0u*/ )
{
	int		ret;

	if ( (*(unsigned int*)data == CONNECTIONLESS_HEADER) && bUseCompression )
	{
		Warning( "[NET] Cannot send compressed connectionless packet to %s '0x%02X'\n", ns_address_render( to ).String(), data[4] );
		Assert( 0 );
		return 0;
	}
	if ( ( *( unsigned int* ) data == CONNECTIONLESS_HEADER ) && ( length > MAX_ROUTABLE_PAYLOAD ) )
	{
		Warning( "[NET] Cannot send connectionless packet to %s '0x%02X' exceeding MTU (%u)\n", ns_address_render( to ).String(), data[ 4 ], length );
		Assert( 0 );
		return 0;
	}

	if ( net_showudp.GetInt() && (*(unsigned int*)data == CONNECTIONLESS_HEADER) && net_showudp_oob.GetInt() )
	{
		Assert( !bUseCompression );
		if ( !net_showudp_remoteonly.GetBool() || !( to.IsLocalhost() || to.IsLoopback() ) )
		{
			Msg("UDP -> %s: sz=%d OOB '0x%02X'\n", ns_address_render( to ).String(), length, data[4] );
		}
	}

	if ( !NET_IsMultiplayer() || to.IsLoopback() || ( to.IsLocalhost() && !net_usesocketsforloopback.GetBool() ) )
	{
		Assert( !pVoicePayload );

		NET_SendLoopPacket(sock, length, data );
		return length;
	}

	int net_socket = 0;
	if ( to.IsType<netadr_t>() )
	{
		net_socket = net_sockets[sock].hUDP;
		if (!net_socket)
			return length;
	}

	if ( (droppackets.GetInt() < 0)  && sock == NS_CLIENT )
	{
		droppackets.SetValue( droppackets.GetInt() + 1 );
		return length;
	}

	if ( fakeloss.GetFloat() > 0.0f )
	{
		// simulate sending this packet
		if (RandomInt(0,100) <= (int)fakeloss.GetFloat())
			return length;
	}

	MEM_ALLOC_CREDIT();
	CUtlMemoryFixedGrowable< byte, NET_COMPRESSION_STACKBUF_SIZE > memCompressed( NET_COMPRESSION_STACKBUF_SIZE );
	CUtlMemoryFixedGrowable< byte, NET_COMPRESSION_STACKBUF_SIZE > memCompressedVoice( NET_COMPRESSION_STACKBUF_SIZE );
	CUtlMemoryFixedGrowable< byte, NET_COMPRESSION_STACKBUF_SIZE > memEncryptedAll( NET_COMPRESSION_STACKBUF_SIZE );

	int iGameDataLength = pVoicePayload ? length : -1;

	bool bWroteVoice = false;
	unsigned int nVoiceBytes = 0;

	if ( pVoicePayload )
	{
		memCompressedVoice.EnsureCapacity( pVoicePayload->GetNumBytesWritten() + sizeof( unsigned short ) );

		byte *pVoice = (byte *)memCompressedVoice.Base();

		unsigned short usVoiceBits = pVoicePayload->GetNumBitsWritten();
		*( unsigned short * )pVoice = LittleShort( usVoiceBits );
		pVoice += sizeof( unsigned short );
		
		unsigned int nCompressedLength = pVoicePayload->GetNumBytesWritten();
		byte *pOutput = NULL;
		if ( net_compressvoice.GetBool() )
		{
			CLZSS lzss;
			pOutput = lzss.CompressNoAlloc( pVoicePayload->GetData(), pVoicePayload->GetNumBytesWritten(), (byte *)pVoice, &nCompressedLength );
		}
		if ( !pOutput )
		{
			Q_memcpy( pVoice, pVoicePayload->GetData(), pVoicePayload->GetNumBytesWritten() );
		}

		nVoiceBytes = nCompressedLength + sizeof( unsigned short );
	}

	if ( voice_xsend_debug.GetBool() && nVoiceBytes )
	{
		DevMsg( "XVoice: voice data payload for %p: %d bytes\n", chan, nVoiceBytes );
	}

	if ( bUseCompression )
	{
		CLZSS lzss;
		unsigned int nCompressedLength = length;
	
		memCompressed.EnsureCapacity( length + nVoiceBytes + sizeof( unsigned int ) );

		*(int *)memCompressed.Base() = LittleLong( NET_HEADER_FLAG_COMPRESSEDPACKET );

		byte *pOutput = lzss.CompressNoAlloc( (byte *)data, length, memCompressed.Base() + sizeof( unsigned int ), &nCompressedLength );
		if ( pOutput )
		{
			data	= memCompressed.Base();
			length	= nCompressedLength + sizeof( unsigned int );

			if ( pVoicePayload && pVoicePayload->GetNumBitsWritten() > 0 )
			{
				byte *pVoice = (byte *)memCompressed.Base() + length;
				Q_memcpy( pVoice, memCompressedVoice.Base(), nVoiceBytes );
			}
			
			iGameDataLength = length;

			length += nVoiceBytes;

			bWroteVoice = true;
		}
	}
	
	if ( !bWroteVoice && pVoicePayload && pVoicePayload->GetNumBitsWritten() > 0 )
	{
		memCompressed.EnsureCapacity( length + nVoiceBytes );

		byte *pVoice = (byte *)memCompressed.Base();
		Q_memcpy( pVoice, (const void *)data, length );
		pVoice += length;
		Q_memcpy( pVoice, memCompressedVoice.Base(), nVoiceBytes );
		data	= memCompressed.Base();

		length  += nVoiceBytes;
	}

	// If the network channel has encryption key then we should encrypt
	if ( const unsigned char *pubEncryptionKey = chan ? chan->GetChannelEncryptionKey() : NULL )
	{
		IceKey iceKey( 2 );
		iceKey.set( pubEncryptionKey );

		// Generate some random fudge, ICE operates on 64-bit blocks, so make sure our total size is a multiple of 8 bytes
		int numRandomFudgeBytes = RandomInt( 16, 72 );
		int numTotalEncryptedBytes = 1 + numRandomFudgeBytes + sizeof( int32 ) + length;
		numRandomFudgeBytes += iceKey.blockSize() - ( numTotalEncryptedBytes % iceKey.blockSize() );
		numTotalEncryptedBytes = 1 + numRandomFudgeBytes + sizeof( int32 ) + length;

		char *pchRandomFudgeBytes = ( char * ) stackalloc( numRandomFudgeBytes );
		for ( int k = 0; k < numRandomFudgeBytes; ++k )
			pchRandomFudgeBytes[ k ] = RandomInt( 16, 250 );

		// Prepare the encrypted memory
		memEncryptedAll.EnsureCapacity( numTotalEncryptedBytes );
		* memEncryptedAll.Base() = numRandomFudgeBytes;
		Q_memcpy( memEncryptedAll.Base() + 1, pchRandomFudgeBytes, numRandomFudgeBytes );

		int32 const numBytesWrittenWire = BigLong( length );	// byteswap for the wire
		Q_memcpy( memEncryptedAll.Base() + 1 + numRandomFudgeBytes, &numBytesWrittenWire, sizeof( numBytesWrittenWire ) );
		Q_memcpy( memEncryptedAll.Base() + 1 + numRandomFudgeBytes + sizeof( int32 ), data, length );

		// Encrypt the message
		unsigned char *pchCryptoBuffer = ( unsigned char * ) stackalloc( iceKey.blockSize() );
		for ( int k = 0; k < numTotalEncryptedBytes; k += iceKey.blockSize() )
		{
			iceKey.encrypt( ( const unsigned char * ) ( memEncryptedAll.Base() + k ), pchCryptoBuffer );
			Q_memcpy( memEncryptedAll.Base() + k, pchCryptoBuffer, iceKey.blockSize() );
		}

		// Set the pointers to network out the encrypted data
		data = memEncryptedAll.Base();
		length = numTotalEncryptedBytes;
	}

	// Do we need to break this packet up?
	int nMaxRoutable = MAX_ROUTABLE_PAYLOAD;
	if ( chan )
	{
		nMaxRoutable = clamp( chan->GetMaxRoutablePayloadSize(), MIN_USER_MAXROUTABLE_SIZE, MIN( sv_maxroutable.GetInt(), MAX_USER_MAXROUTABLE_SIZE ) );
	}

	if ( ( unMillisecondsDelay != 0u ) &&
		( length > nMaxRoutable ) )
	{
		Warning( "Can't delay send a packet larger than maxroutable size %d/%d\n", length, nMaxRoutable );
		unMillisecondsDelay = 0u;
	}

	if ( unMillisecondsDelay != 0u )
	{
		ret = NET_QueuePacketForSend( static_cast< CNetChan * >( chan ), false, net_socket, (const char *)data, length, to, unMillisecondsDelay );
	}
	else if ( length <= nMaxRoutable && 
		!(net_queued_packet_thread.GetInt() == NET_QUEUED_PACKET_THREAD_DEBUG_VALUE && chan ) )	
	{
		// simple case, small packet, just send it
		ret = NET_SendTo( true, net_socket, (const char *)data, length, to, iGameDataLength );
	}
	else
	{
		// split packet into smaller pieces
		ret = NET_SendLong( chan, sock, net_socket, (const char *)data, length, to, nMaxRoutable );
	}
	
	if (ret == -1)
	{
		NET_GetLastError();
		
		// wouldblock is silent
		if ( net_error == WSAEWOULDBLOCK )
			return 0;

		if ( net_error == WSAECONNRESET )
			return 0;

		// some PPP links dont allow broadcasts
		if ( ( net_error == WSAEADDRNOTAVAIL) && ( to.IsBroadcast() ) )
			return 0;

		ConDMsg ("NET_SendPacket Warning: %s : %s\n", NET_ErrorString(net_error), ns_address_render( to ).String() );
		ret = length;
	}
	

	return ret;
}

void NET_OutOfBandPrintf(int sock, const ns_address &adr, const char *format, ...)
{
	va_list		argptr;
	char		string[MAX_ROUTABLE_PAYLOAD];
	
	*(unsigned int*)string = CONNECTIONLESS_HEADER;

	va_start (argptr, format);
	Q_vsnprintf (string+4, sizeof( string ) - 4, format,argptr);
	va_end (argptr);

	int length = Q_strlen(string+4) + 5;

	NET_SendPacket ( NULL, sock, adr, (byte *)string, length );
}

void NET_OutOfBandDelayedPrintf(int sock, const ns_address &adr, uint32 unMillisecondsDelay, const char *format, ...)
{
	va_list		argptr;
	char		string[MAX_ROUTABLE_PAYLOAD];

	*(unsigned int*)string = CONNECTIONLESS_HEADER;

	va_start (argptr, format);
	Q_vsnprintf (string+4, sizeof( string ) - 4, format,argptr);
	va_end (argptr);

	int length = Q_strlen(string+4) + 5;

	NET_SendPacket ( NULL, sock, adr, (byte *)string, length, 0, false, unMillisecondsDelay );
}

/*
====================
NET_CloseAllSockets
====================
*/
void NET_CloseAllSockets (void)
{
	// shut down any existing and open sockets
	for (int i=0 ; i<net_sockets.Count() ; i++)
	{
		if ( net_sockets[i].nPort )
		{
			NET_CloseSocket( net_sockets[i].hUDP );
			NET_CloseSocket( net_sockets[i].hTCP );

			net_sockets[i].nPort = 0;
			net_sockets[i].bListening = false;
			net_sockets[i].hUDP = 0;
			net_sockets[i].hTCP = 0;
		}
	}

	// shut down all pending sockets
	AUTO_LOCK_FM( s_PendingSockets );
	for(int j=0; j<s_PendingSockets.Count();j++ )
	{
		NET_CloseSocket( s_PendingSockets[j].newsock );
	}

	s_PendingSockets.RemoveAll();

	// Close steam sockets as well
	g_pSteamSocketMgr->Shutdown();
	g_pSteamSocketMgr->Init();

	// Shutdown steam datagram server, if we were listening
	if ( g_pSteamDatagramGameserver )
	{
		g_pSteamDatagramGameserver->Destroy();
		g_pSteamDatagramGameserver = NULL;
	}

	// Shutdown steam datagram client, if we have one
	CloseSteamDatagramClientConnection();
}

/*
====================
NET_FlushAllSockets
====================
*/
void NET_FlushAllSockets( void )
{
	// drain any packets that my still lurk in our incoming queue
	char data[2048];
	ns_address from;
	
	for (int i=0 ; i<net_sockets.Count() ; i++)
	{
		if ( net_sockets[i].hUDP )
		{
			int bytes = 1;

			// loop until no packets are pending anymore
			while ( bytes > 0  )
			{
				bytes = NET_ReceiveRawPacket( net_sockets[i].hUDP, data, sizeof(data), &from );
			}
		}
	}
}

#if defined( IS_WINDOWS_PC )
#include <Iphlpapi.h>

// Simple helper class to enumerate and cache of IP addresses of local network adapters
class CBindAddressHelper
{
public:
	CBindAddressHelper() : m_bInitialized( false )
	{
	}

	void GetBindAddresses( CUtlVector< CUtlString >& list )
	{
		if ( !m_bInitialized )
		{
			m_bInitialized = true;
			BuildBindAddresses( m_CachedAddresses );
		}

		for ( int i = 0; i < m_CachedAddresses.Count(); ++i )
		{
			list.AddToTail( m_CachedAddresses[ i ] );
		}
	}

private:

	void BuildBindAddresses( CUtlVector< CUtlString >& list )
	{
		IP_ADAPTER_INFO info_temp;
		ULONG len = 0;
		if ( GetAdaptersInfo( &info_temp, &len ) != ERROR_BUFFER_OVERFLOW )
			return;
		IP_ADAPTER_INFO *infos = new IP_ADAPTER_INFO[ len ];
		if ( !infos )
		{
			Sys_Error( "BuildBindAddresses:  Out of memory allocating %d bytes\n", sizeof( IP_ADAPTER_INFO ) * len );
			return;
		}

		if ( GetAdaptersInfo( infos, &len ) == NO_ERROR )
		{
			for ( IP_ADAPTER_INFO *info = infos; info != NULL; info = info->Next ) 
			{
				if ( info->Type == MIB_IF_TYPE_LOOPBACK )
					continue;
				if ( !Q_strcmp( info->IpAddressList.IpAddress.String, "0.0.0.0" ) )
					continue;

				DevMsg( "NET_GetBindAddresses found %s: '%s'\n", info->IpAddressList.IpAddress.String, info->Description );
				list.AddToTail( CUtlString( info->IpAddressList.IpAddress.String ) );
			}
		}
		delete[] infos;
	}

	bool						m_bInitialized;
	CUtlVector< CUtlString >	m_CachedAddresses;
};
static CBindAddressHelper g_BindAddressHelper;
#endif

#ifndef DEDICATED

// Initialize steam client datagram lib if we haven't already
static bool CheckInitSteamDatagramClientLib()
{
	static bool bInittedNetwork = false;
	if ( bInittedNetwork )
		return true;

	if ( !Steam3Client().SteamHTTP() )
	{
		Warning( "Cannot init steam datagram client, no Steam HTTP interface\n" );
		return false;
	}

	// Locate the first PLATFORM path
	char szAbsPlatform[MAX_FILEPATH] = "";
	const char *pszConfigDir = "config";
	g_pFullFileSystem->GetSearchPath( "PLATFORM", false, szAbsPlatform, sizeof(szAbsPlatform) );

	char *semi = strchr( szAbsPlatform, ';' );
	if ( semi )
		*semi = '\0';

	// Set partner.  Running in china?
	ESteamDatagramPartner ePartner = k_ESteamDatagramPartner_Steam;
	if ( CommandLine()->HasParm( "-perfectworld" ) )
		ePartner = k_ESteamDatagramPartner_China;
	int iPartnerMark = -1; // CSGO doesn't prune the config based on partner!

	char szAbsConfigDir[ MAX_FILEPATH];
	V_ComposeFileName( szAbsPlatform, pszConfigDir, szAbsConfigDir, sizeof(szAbsConfigDir) );
	SteamDatagramClient_Init( szAbsConfigDir, ePartner, iPartnerMark );
	bInittedNetwork = true;

	return true;
}

void NET_PrintSteamdatagramClientStatus()
{
	if ( !g_pSteamDatagramClient )
	{
		Msg( "No steam datagram client connection active\n" );
		return;
	}
	ISteamDatagramTransportClient::ConnectionStatus status;
	g_pSteamDatagramClient->GetConnectionStatus( status );
	int sz = status.Print( NULL, 0 );
	CUtlMemory<char> buf;
	buf.EnsureCapacity( sz );
	char *p = buf.Base();
	status.Print( p, sz );
	for (;;)
	{
		char *newline = strchr( p, '\n' );
		if ( newline )
			*newline = '\0';
		Msg( "%s\n", p );
		if ( !newline )
			break;
		p = newline+1;
	}
}
CON_COMMAND( steamdatagram_client_status, "Print steam datagram client status" )
{
	NET_PrintSteamdatagramClientStatus();
}

bool NET_InitSteamDatagramProxiedGameserverConnection( const ns_address &adr )
{
	Assert( adr.GetAddressType() == NSAT_PROXIED_GAMESERVER );

	// Most common case - talking to the same server as before
	if ( g_pSteamDatagramClient )
	{
		if ( g_addrSteamDatagramProxiedGameServer.m_steamID.GetSteamID() == adr.m_steamID.GetSteamID() )
		{
			g_addrSteamDatagramProxiedGameServer.m_steamID.SetSteamChannel( adr.m_steamID.GetSteamChannel() );
			return true;
		}

		// We have a client, but it was to talk to a different server.  Clear our ticket!
		g_pSteamDatagramClient->Close();
		g_addrSteamDatagramProxiedGameServer.Clear();
	}

	// Get a client to talk to this server
	g_pSteamDatagramClient = SteamDatagramClient_Connect( adr.m_steamID.GetSteamID() );
	if ( !g_pSteamDatagramClient )
		return false;

	// OK, remember who we're talking to
	g_addrSteamDatagramProxiedGameServer = adr;
	return true;
}

#endif

static void OpenSocketInternal( int nModule, int nSetPort, int nDefaultPort, const char *pName, int nProtocol, bool bTryAny )
{
	CUtlVector< CUtlString > vecBindableAddresses;
	if ( ( NS_HLTV == nModule )&& ipname_tv.GetString()[0] )
	{
		vecBindableAddresses.AddToTail( CUtlString( ipname_tv.GetString() ) );
	}
	else if ( ( NS_HLTV1 == nModule ) && ipname_tv1.GetString()[ 0 ] )
	{
		vecBindableAddresses.AddToTail( CUtlString( ipname_tv1.GetString() ) );
	}
	else
	{
		vecBindableAddresses.AddToTail( CUtlString( ipname.GetString() ) );
	}
#if defined( IS_WINDOWS_PC )
	g_BindAddressHelper.GetBindAddresses( vecBindableAddresses );
#endif

	int port = nSetPort ? nSetPort : nDefaultPort;
	int *handle = NULL;
	if( nProtocol == IPPROTO_TCP )
	{
		handle = &net_sockets[nModule].hTCP;
	}
	else if ( nProtocol == IPPROTO_UDP || nProtocol == IPPROTO_VDP )
	{
		handle = &net_sockets[nModule].hUDP;
	}
	else
	{
		Sys_Error( "Unrecognized protocol type %d", nProtocol );
		return;
	}

	if ( !net_sockets[nModule].nPort )
	{
		int nSavePort = port;
		for ( int i = 0; i < vecBindableAddresses.Count(); ++i )
		{
			port = nSavePort;

			char const *pchIPAddressToBind = vecBindableAddresses[ i ].String();

			if ( i > 0 )
			{
				Msg( "Trying to open socket on %s\n", pchIPAddressToBind );
			}

			// If we are only using Steam sockets, get a fake handle
			*handle = OnlyUseSteamSockets() ? ++g_nFakeSocketHandle : NET_OpenSocket (pchIPAddressToBind, port, nProtocol );

			if ( !OnlyUseSteamSockets() && !*handle && bTryAny )
			{
				port = PORT_ANY;	// try again with PORT_ANY
				*handle = NET_OpenSocket (pchIPAddressToBind, port, nProtocol );
			}

			// Stop on first success
			if ( *handle )
				break;
		}

		if ( !*handle )
		{
			Warning( "Couldn't allocate any %s IP port, tried %d addresses %s", pName, vecBindableAddresses.Count(),
				( vecBindableAddresses.Count() ? vecBindableAddresses.Head().Get() : "(none)" ) );
			Plat_ExitProcess( 0 );	// cause a silent exit without writing a core dump
			return;
		}

		net_sockets[nModule].nPort = port;
	}
	else
	{
		Msg("WARNING: NET_OpenSockets: %s port %i already open.\n", pName, net_sockets[nModule].nPort );
	}

	if ( handle )
	{
		g_pSteamSocketMgr->OpenSocket( *handle, nModule, nSetPort, nDefaultPort, pName, nProtocol, bTryAny );
	}
	#ifndef DEDICATED
		if ( nModule == NS_CLIENT )
			CheckInitSteamDatagramClientLib();
	#endif
}

/*
====================
NET_OpenSockets
====================
*/
void NET_OpenSockets (void)
{	
	// Xbox 360 uses VDP protocol to combine encrypted game data with clear voice data
	const int nProtocol = IsX360() ? IPPROTO_VDP : IPPROTO_UDP;

	OpenSocketInternal( NS_SERVER, hostport.GetInt(), PORT_SERVER, "server", nProtocol, false );
	OpenSocketInternal( NS_CLIENT, clientport.GetInt(), PORT_SERVER, "client", nProtocol, true );

#ifdef _X360
	int nX360Port = PORT_X360_RESERVED_FIRST;
	OpenSocketInternal( NS_X360_SYSTEMLINK,	0, nX360Port ++,	"x360systemlink",	IPPROTO_UDP,	false );
	OpenSocketInternal( NS_X360_LOBBY,		0, nX360Port ++,	"x360lobby",		nProtocol,		false );
	OpenSocketInternal( NS_X360_TEAMLINK,	0, nX360Port ++,	"x360teamlink",		nProtocol,		false );
	Assert( nX360Port <= PORT_X360_RESERVED_LAST );
#endif

	if ( !net_nohltv )
	{
		OpenSocketInternal( NS_HLTV, hltvport.GetInt(), PORT_HLTV, "hltv", nProtocol, false );
		if ( net_addhltv1 )
		{
			OpenSocketInternal( NS_HLTV1, hltvport1.GetInt(), PORT_HLTV1, "hltv1", nProtocol, false );
		}
	}
#if defined( REPLAY_ENABLED )
	if ( !net_noreplay )
	{
		OpenSocketInternal( NS_REPLAY, replayport.GetInt(), PORT_REPLAY, "replay", nProtocol, false );
	}
#endif
}

bool NET_IsSocketOpen( int nSockIdx )
{
	if ( !net_sockets.IsValidIndex( nSockIdx ) )
		return false;

	if ( !net_sockets[nSockIdx].hUDP )
		return false;

	return true;
}

int NET_AddExtraSocket( int port )
{
	int newSocket = net_sockets.AddToTail();

	Q_memset( &net_sockets[newSocket], 0, sizeof(netsocket_t) );

	OpenSocketInternal( newSocket, port, PORT_ANY, "extra", IPPROTO_UDP, true );

	net_packets.EnsureCount( newSocket+1 );
	net_splitpackets.EnsureCount( newSocket+1 );

	return newSocket;
}

void NET_RemoveAllExtraSockets()
{
	for (int i=MAX_SOCKETS ; i<net_sockets.Count() ; i++)
	{
		if ( net_sockets[i].nPort )
		{
			NET_CloseSocket( net_sockets[i].hUDP );
			NET_CloseSocket( net_sockets[i].hTCP );
		}
	}
	net_sockets.RemoveMultiple( MAX_SOCKETS, net_sockets.Count()-MAX_SOCKETS );

	Assert( net_sockets.Count() == MAX_SOCKETS );
}

unsigned short NET_GetUDPPort(int socket)
{
	return net_sockets.IsValidIndex( socket ) ?
		net_sockets[socket].nPort : 0;
}

static void NET_ConPrintByteStream( uint8 const *pb, int nSize )
{
	for ( int k = 0; k < nSize; ++ k, ++ pb )
		Msg( " %02X", *pb );
}

/*
================
NET_GetLocalAddress

Returns the servers' ip address as a string.
================
*/
void NET_GetLocalAddress (void)
{
	net_local_adr.Clear();

	if ( net_noip )
	{
		Msg("TCP/UDP Disabled.\n");
	}
	else
	{
#ifdef _X360
		int err = 0;
		XNADDR xnaddrLocal;
		ZeroMemory( &xnaddrLocal, sizeof( xnaddrLocal ) );
		while( XNET_GET_XNADDR_PENDING == ( err = XNetGetTitleXnAddr( &xnaddrLocal ) ) )
			continue;

		static struct XnAddrType_t
		{
			int m_code;
			char const *m_szValue;
		}
		arrXnAddrTypes[] = {
			{ XNET_GET_XNADDR_NONE,				"NONE" },
			{ XNET_GET_XNADDR_ETHERNET,         "ETHERNET" },
			{ XNET_GET_XNADDR_STATIC,           "STATIC" },
			{ XNET_GET_XNADDR_DHCP,             "DHCP" },
			{ XNET_GET_XNADDR_PPPOE,            "PPPoE" },
			{ XNET_GET_XNADDR_GATEWAY,          "GATEWAY" },
			{ XNET_GET_XNADDR_DNS,              "DNS" },
			{ XNET_GET_XNADDR_ONLINE,			"ONLINE" },
			{ XNET_GET_XNADDR_TROUBLESHOOT,		"TROUBLESHOOT" },
			{ 0, NULL }
		};

		Msg( "Local XNetwork address type 0x%08X", err );
		for ( XnAddrType_t const *pxat = arrXnAddrTypes; pxat->m_code; ++ pxat )
		{
			if ( ( err & pxat->m_code ) == pxat->m_code )
				Msg( " %s", pxat->m_szValue );
		}
		Msg( "\n" );

		net_local_adr.SetFromString( "127.0.0.1" );
		Msg( "Local IP address: %d.%d.%d.%d\n",
			xnaddrLocal.ina.S_un.S_un_b.s_b1,
			xnaddrLocal.ina.S_un.S_un_b.s_b2,
			xnaddrLocal.ina.S_un.S_un_b.s_b3,
			xnaddrLocal.ina.S_un.S_un_b.s_b4 );

#elif defined( _PS3 )
		CellNetCtlInfo cnci;
		memset( &cnci, 0, sizeof( cnci ) );

		// Print CELL network information for debug output
		Msg( "=========== CELL network information ===========\n" );
		for ( int iCellInfo = CELL_NET_CTL_INFO_DEVICE; iCellInfo <= CELL_NET_CTL_INFO_UPNP_CONFIG; ++ iCellInfo )
		{
			int ret = cellNetCtlGetInfo( iCellInfo, &cnci );
			if ( CELL_OK != ret )
			{
				Warning( "NET: failed to obtain CELL NET INFO #%d, error code %d.\n", iCellInfo, ret );
			}
			else switch ( iCellInfo )
			{
				case CELL_NET_CTL_INFO_DEVICE:
					Msg( " Device:            %u\n", cnci.device );
					break;
				case CELL_NET_CTL_INFO_ETHER_ADDR:
					Msg( " Ethernet Address:  [" );
						NET_ConPrintByteStream( cnci.ether_addr.data, sizeof( cnci.ether_addr.data ) );
						NET_ConPrintByteStream( cnci.ether_addr.padding, sizeof( cnci.ether_addr.padding ) );
						Msg( " ]\n" );
						break;
				case CELL_NET_CTL_INFO_MTU:
					Msg( " MTU:               %u\n", cnci.mtu );
					break;
				case CELL_NET_CTL_INFO_LINK:
					Msg( " Link:              %u\n", cnci.link );
					break;
				case CELL_NET_CTL_INFO_LINK_TYPE:
					Msg( " Link type:         %u\n", cnci.link_type );
					break;
				case CELL_NET_CTL_INFO_BSSID:
					Msg( " BSSID Address:     [" );
						NET_ConPrintByteStream( cnci.bssid.data, sizeof( cnci.bssid.data ) );
						NET_ConPrintByteStream( cnci.bssid.padding, sizeof( cnci.bssid.padding ) );
						Msg( " ]\n" );
						break;
				case CELL_NET_CTL_INFO_SSID:
					Msg( " SSID Address:      [" );
						NET_ConPrintByteStream( cnci.ssid.data, sizeof( cnci.ssid.data ) );
						NET_ConPrintByteStream( &cnci.ssid.term, sizeof( cnci.ssid.term ) );
						NET_ConPrintByteStream( cnci.ssid.padding, sizeof( cnci.ssid.padding ) );
						Msg( " ]\n" );
						break;
				case CELL_NET_CTL_INFO_WLAN_SECURITY:
					Msg( " WLAN security:     %u\n", cnci.wlan_security );
					break;
				case CELL_NET_CTL_INFO_8021X_TYPE:
					Msg( " WAuth 8021x type:  %u\n", cnci.auth_8021x_type );
					break;
				case CELL_NET_CTL_INFO_8021X_AUTH_NAME:
					Msg( " WAuth 8021x name:  %s\n", cnci.auth_8021x_auth_name );
					break;
				case CELL_NET_CTL_INFO_RSSI:
					Msg( " WRSSI:             %u\n", cnci.rssi );
					break;
				case CELL_NET_CTL_INFO_CHANNEL:
					Msg( " WChannel:          %u\n", cnci.channel );
					break;
				case CELL_NET_CTL_INFO_IP_CONFIG:
					Msg( " Ipconfig:          %u\n", cnci.ip_config );
					break;
				case CELL_NET_CTL_INFO_DHCP_HOSTNAME:
					Msg( " DHCP hostname:     %s\n", cnci.dhcp_hostname );
					break;
				case CELL_NET_CTL_INFO_PPPOE_AUTH_NAME:
					Msg( " PPPOE auth name:   %s\n", cnci.pppoe_auth_name );
					break;
				case CELL_NET_CTL_INFO_IP_ADDRESS:
					Msg( " IP address:        %s\n", cnci.ip_address );
					break;
				case CELL_NET_CTL_INFO_NETMASK:
					Msg( " Net mask:          %s\n", cnci.netmask );
					break;
				case CELL_NET_CTL_INFO_DEFAULT_ROUTE:
					Msg( " Default route:     %s\n", cnci.default_route );
					break;
				case CELL_NET_CTL_INFO_PRIMARY_DNS:
					Msg( " Primary DNS:       %s\n", cnci.primary_dns );
					break;
				case CELL_NET_CTL_INFO_SECONDARY_DNS:
					Msg( " Secondary DNS:     %s\n", cnci.secondary_dns );
					break;
				case CELL_NET_CTL_INFO_HTTP_PROXY_CONFIG:
					Msg( " HTTP proxy config: %u\n", cnci.http_proxy_config );
					break;
				case CELL_NET_CTL_INFO_HTTP_PROXY_SERVER:
					Msg( " HTTP proxy server: %s\n", cnci.http_proxy_server );
					break;
				case CELL_NET_CTL_INFO_HTTP_PROXY_PORT:
					Msg( " HTTP proxy port: %d\n", cnci.http_proxy_port );
					break;
				case CELL_NET_CTL_INFO_UPNP_CONFIG:
					Msg( " UPNP config:       %u\n", cnci.upnp_config );
					break;
				default:
					Msg( " UNKNOWNNETDATA[%d]:     [", iCellInfo );
						NET_ConPrintByteStream( reinterpret_cast< const uint8* >( &cnci ), sizeof( cnci ) );
						Msg( " ]\n" );
					break;
			}
		}
		Msg( "================================================\n" );
		// -- end CELL network debug information

		net_local_adr.SetFromString( "127.0.0.1" );
		if ( CELL_OK == cellNetCtlGetInfo( CELL_NET_CTL_INFO_IP_ADDRESS, &cnci ) )
			net_local_adr.SetFromString( cnci.ip_address );
#else
		char	buff[512];

		// If we have changed the ip var from the command line, use that instead.
		if ( Q_strcmp(ipname.GetString(), "localhost") )
		{
			Q_strncpy(buff, ipname.GetString(), sizeof( buff ) );	// use IP set with ipname
		}
		else
		{
#if defined( LINUX )
		//        Run the systems ifconfig call to scan for an eth0 address so we don't show only the machine's loopback address
		//
		// note:  This block simply grabs and prints out the IP address to the TTY stream
		//        the rest of the port/network information is printed in NET_Config()

		FILE * fp = popen("ifconfig", "r");
        	if (fp) 
		{
            		char *curLine=NULL; 
			size_t n;
			bool lastWasEth0 = false;
                	while ((getline(&curLine, &n, fp) > 0) && curLine) 	
			{
				// loop through each line returned from ifconfig
				if( strstr(curLine, "Link encap:") )
				{
					if(strstr(curLine, "eth0") )
						lastWasEth0 = true; // this is part of the entry we want, the next IP after this is the right one
					else
						lastWasEth0 = false;
				} 

                 	  	if (lastWasEth0 && (curLine = strstr(curLine, "inet addr:") )) 
				{
                        		curLine+=10; // skip past the eth0 lable and blank space to the address we want
					char* curChar = strchr(curLine, ' ');
                        		if ( curChar ) 
					{
						*curChar ='\0';
                        			Msg("Network: IP %s ", curLine);
                        		}
                   		}
                	}
	        	pclose(fp);
        	}
		else
		{
			Msg( "Network: <failed to find IP> " );
		}
#endif // LINUX

			gethostname( buff, sizeof(buff) );	// get own IP address
			buff[sizeof(buff)-1] = 0;			// Ensure that it doesn't overrun the buffer
		}

		NET_StringToAdr (buff, &net_local_adr);
#endif

		int ipaddr = ( net_local_adr.ip[0] << 24 ) + 
			( net_local_adr.ip[1] << 16 ) + 
			( net_local_adr.ip[2] << 8 ) + 
			net_local_adr.ip[3];

		hostip.SetValue( ipaddr );
	}
}


/*
====================
NET_IsConfigured

Is winsock ip initialized?
====================
*/
bool NET_IsMultiplayer( void )
{
	return net_multiplayer;
}

bool NET_IsDedicated( void )
{
	return net_dedicated;
}

#ifdef SERVER_XLSP
bool NET_IsDedicatedForXbox( void )
{
	return net_dedicated && net_dedicatedForXbox;
}
#endif

#ifdef _X360
#include "iengine.h"
static FileHandle_t g_fh;
void NET_LogServerStatus( void )
{
	if ( !g_fh )
		return;

	static float fNextTime = 0.f;
	float fCurrentTime = eng->GetCurTime();

	if ( fCurrentTime >= fNextTime )
	{
		fNextTime = fCurrentTime + net_loginterval.GetFloat();
	}
	else
	{
		return;
	}

	AUTO_LOCK_FM( s_NetChannels );
	int numChannels = s_NetChannels.Count();

	if ( numChannels == 0 )
	{
		ConMsg( "No active net channels.\n" );
		return;
	}

	enum
	{
		NET_LATENCY,
		NET_LOSS,
		NET_PACKETS_IN,
		NET_PACKETS_OUT,
		NET_CHOKE_IN,
		NET_CHOKE_OUT,
		NET_FLOW_IN,
		NET_FLOW_OUT,
		NET_TOTAL_IN,
		NET_TOTAL_OUT,
		NET_LAST,
	};
	float fStats[NET_LAST] = {0.f};

	for ( int i = 0; i < numChannels; ++i )
	{
		INetChannel *chan = s_NetChannels[i];
		fStats[NET_LATENCY] += chan->GetAvgLatency(FLOW_OUTGOING);
		fStats[NET_LOSS] += chan->GetAvgLoss(FLOW_INCOMING);
		fStats[NET_PACKETS_IN] += chan->GetAvgPackets(FLOW_INCOMING);
		fStats[NET_PACKETS_OUT] += chan->GetAvgPackets(FLOW_OUTGOING);
		fStats[NET_CHOKE_IN] += chan->GetAvgChoke(FLOW_INCOMING);
		fStats[NET_CHOKE_OUT] += chan->GetAvgChoke(FLOW_OUTGOING);
		fStats[NET_FLOW_IN] += chan->GetAvgData(FLOW_INCOMING);
		fStats[NET_FLOW_OUT] += chan->GetAvgData(FLOW_OUTGOING);
		fStats[NET_TOTAL_IN] += chan->GetTotalData(FLOW_INCOMING);
		fStats[NET_TOTAL_OUT] += chan->GetTotalData(FLOW_OUTGOING);
	}

	for ( int i = 0; i < NET_LAST; ++i )
	{
		fStats[i] /= numChannels;
	}

	const unsigned int size = 128;
	char msg[size];
	Q_snprintf( msg, size, "%.0f,%d,%.0f,%.0f,%.0f,%.1f,%.1f,%.1f,%.1f,%.1f\n", 
				fCurrentTime,
				numChannels,
				fStats[NET_LATENCY],
				fStats[NET_LOSS],
				fStats[NET_PACKETS_IN], 
				fStats[NET_PACKETS_OUT],
				fStats[NET_FLOW_IN]/1024.0f, 
				fStats[NET_FLOW_OUT]/1024.0f,
				fStats[NET_CHOKE_IN],
				fStats[NET_CHOKE_OUT]
			 );

	g_pFileSystem->Write( msg, Q_strlen( msg ), g_fh );
}

void NET_LogServerCallback( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );

	if ( var.GetBool() )
	{
		if ( g_fh )
		{
			g_pFileSystem->Close( g_fh );
			g_fh = 0;
		}

		g_fh = g_pFileSystem->Open( "dump.csv", "wt" );
		if ( !g_fh )
		{
			Msg( "Failed to open log file\n" );
			pConVar->SetValue( 0 );
			return;
		}

		char msg[128];
		Q_snprintf( msg, 128, "Time,Channels,Latency,Loss,Packets In,Packets Out,Flow In(kB/s),Flow Out(kB/s),Choke In,Choke Out\n" );
		g_pFileSystem->Write( msg, Q_strlen( msg ), g_fh );
	}
	else
	{
		if ( g_fh )
		{
			g_pFileSystem->Close( g_fh );
			g_fh = 0;
		}
	}
}
#endif

/*
====================
NET_SetTime

Updates net_time
====================
*/
void NET_SetTime( double realtime )
{
	static double s_last_realtime = 0;

	double frametime = realtime - s_last_realtime;
	s_last_realtime = realtime;

	if ( frametime > 1.0f )
	{
		// if we have very long frame times because of loading stuff
		// don't apply that to net time to avoid unwanted timeouts
		frametime = 1.0f;
	}
	else if ( frametime < 0.0f )
	{
		frametime = 0.0f;
	}

#if !defined( DEDICATED )
	// adjust network time so fakelag works with host_timescale
	net_time += frametime * g_pEngineToolInternal->GetTimescale();
#else
    	net_time += frametime * sv.GetTimescale();
#endif
}

/*
====================
NET_RunFrame

RunFrame must be called each system frame before reading/sending on any socket
====================
*/
void NET_RunFrame( double realtime )
{
	NET_SetTime( realtime );

	RCONServer().RunFrame();

#ifdef ENABLE_RPT
	RPTServer().RunFrame();
#endif

	Con_RunFrame();
#ifndef DEDICATED
	RCONClient().RunFrame();
	#ifdef ENABLE_RPT
	RPTClient().RunFrame();
	#endif
#endif
#ifdef _X360
	if ( net_logserver.GetInt() )
	{
		NET_LogServerStatus();
	}
#endif

	if ( g_pMatchFramework )
	{
		g_pMatchFramework->RunFrame();
	}

	if ( !NET_IsMultiplayer() || net_notcp )
		return;

	// process TCP sockets:
	for ( int i=0; i< net_sockets.Count(); i++ )
	{
		if ( net_sockets[i].hTCP && net_sockets[i].bListening )
		{
			NET_ProcessListen( i );
		}
	}

	NET_ProcessPending();
}

void NET_ClearLoopbackBuffers()
{
	for (int i = 0; i < LOOPBACK_SOCKETS; i++)
	{
		loopback_t *loop = NULL;

		while ( s_LoopBacks[i].PopItem( &loop ) )
		{
			if ( loop->data && loop->data != loop->defbuffer )
			{
				delete [] loop->data;
			}
			delete loop;
		}
	}
}

void NET_ConfigLoopbackBuffers( bool bAlloc )
{
	NET_ClearLoopbackBuffers();
}

/*
====================
NET_Config

A single player game will only use the loopback code
====================
*/

void NET_Config ( void )
{
	// free anything
	NET_CloseAllSockets();	// close all UDP/TCP sockets

	net_time = 0.0f;

	// now reconfigure

	if ( net_multiplayer )
	{	
		// don't allocate loopback buffers
		NET_ConfigLoopbackBuffers( false );

		// get localhost IP address
		NET_GetLocalAddress();

		// reopen sockets if in MP mode
		NET_OpenSockets();

		// setup the rcon server sockets
		if ( net_dedicated || CommandLine()->FindParm( "-usercon" ) )
		{
			netadr_t rconAddr = net_local_adr;
			rconAddr.SetPort( net_sockets[NS_SERVER].nPort );
			RCONServer().SetAddress( rconAddr.ToString() );
			RCONServer().CreateSocket();
		}
	}
	else
	{
		// allocate loopback buffers
		NET_ConfigLoopbackBuffers( true );
	}
	

#if !defined( LINUX )
  	// note:  linux prints out the network IP before calling this function
	DevMsg( "Network: IP %s ", net_local_adr.ToString(true));
#endif
	DevMsg( "mode %s, dedicated %s, ports %i SV / %i CL\n", 
		net_multiplayer?"MP":"SP", net_dedicated?"Yes":"No", 
		net_sockets[NS_SERVER].nPort, net_sockets[NS_CLIENT].nPort );
}

/*
====================
NET_SetDedicated

A single player game will only use the loopback code
====================
*/

void NET_SetDedicated ()
{
	if ( net_noip )
	{
		Msg( "Warning! Dedicated not possible with -noip parameter.\n");
		return;		
	}

	net_dedicated = true;
	net_dedicatedForXbox = ( CommandLine()->FindParm( "-xlsp" ) != 0 );
	net_dedicatedForXboxInsecure = ( CommandLine()->FindParm( "-xlsp_insecure" ) != 0 );
}

void NET_ListenSocket( int sock, bool bListen )
{
	Assert( (sock >= 0) && (sock < net_sockets.Count()) );

	netsocket_t * netsock = &net_sockets[sock];

	if ( netsock->hTCP )
	{
		NET_CloseSocket( netsock->hTCP, sock );
	}

	if ( !NET_IsMultiplayer() || net_notcp )
		return;

	if ( bListen )
	{
		const char * net_interface = ipname.GetString();

		netsock->hTCP = NET_OpenSocket( net_interface, netsock->nPort, true );

		if ( !netsock->hTCP )
		{
			Msg( "Warning! NET_ListenSocket failed opening socket %i, port %i.\n", sock, net_sockets[sock].nPort );
			return;
		}

		struct sockaddr_in	address;

		if (!net_interface || !net_interface[0] || !Q_strcmp(net_interface, "localhost"))
		{
			address.sin_addr.s_addr = INADDR_ANY;
		}
		else
		{
			NET_StringToSockaddr (net_interface, (struct sockaddr *)&address);
		}

		address.sin_family = AF_INET;
		address.sin_port = NET_HostToNetShort((short)( netsock->nPort ));

		int ret;
		ret = bind( netsock->hTCP, (struct sockaddr *)&address, sizeof(address) );
		if ( ret == -1 )
		{
			NET_GetLastError();
			Msg ("WARNING: NET_ListenSocket bind failed on socket %i, port %i.\n", netsock->hTCP, netsock->nPort );
			return;
		}

		ret = listen( netsock->hTCP, TCP_MAX_ACCEPTS );
		if ( ret == -1 )
		{
			NET_GetLastError();
			Msg ("WARNING: NET_ListenSocket listen failed on socket %i, port %i.\n", netsock->hTCP, netsock->nPort );
			return;
		}

		netsock->bListening = true;
	}
}

void NET_SetMultiplayer(bool multiplayer)
{
	if ( net_noip && multiplayer )
	{
		Msg( "Warning! Multiplayer mode not available with -noip parameter.\n");
		return;		
	}

	if ( net_dedicated && !multiplayer )
	{
		Msg( "Warning! Singleplayer mode not available on dedicated server.\n");
		return;		
	}

	// reconfigure if changed
	if ( net_multiplayer != multiplayer )
	{
		net_multiplayer = multiplayer;
		NET_Config();
	}

	// clear loopback buffer in single player mode
	if ( !multiplayer )
	{
		NET_ClearLoopbackBuffers();
	}
}

void NET_InitPostFork( void )
{
	if ( CommandLine()->FindParm( "-NoQueuedPacketThread" ) )
		Warning( "Found -NoQueuedPacketThread, so no queued packet thread will be created.\n" );
	else
		g_pQueuedPackedSender->Setup();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bIsDedicated - 
//-----------------------------------------------------------------------------
void NET_Init( bool bIsDedicated )
{
	// In dedicated server mode network must be initialized post-fork
	
	// single entry guard
	{
		static bool sbNetworkingIntialized = false;

		if ( sbNetworkingIntialized )
		{
			return;
		}

		sbNetworkingIntialized = true;
	}

	if (CommandLine()->FindParm("-nodns"))
	{
		net_nodns = true;
	}

	if (CommandLine()->FindParm("-usetcp"))
	{
		net_notcp = false;
	}

	if ( IsGameConsole() || CommandLine()->FindParm( "-nohltv" ) )
	{
		net_nohltv = true;
	}

	if ( CommandLine()->FindParm( "-addhltv1" ) )
	{
		net_addhltv1 = true;
	}

#if defined( REPLAY_ENABLED )
	if ( CommandLine()->FindParm("-noreplay"))
	{
		net_noreplay = true;
	}
#endif

	if (CommandLine()->FindParm("-noip"))
	{
		net_noip = true;
	}
	else
	{
#if defined( _X360 )
		XOnlineCleanup();

		XNetStartupParams xnsp;
		memset( &xnsp, 0, sizeof( xnsp ) );
		xnsp.cfgSizeOfStruct = sizeof( XNetStartupParams );
		if ( X360SecureNetwork() )
		{
			Msg( "Xbox 360 Network: Secure.\n" );
		}
		else
		{
			// Allow cross-platform communication
			xnsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
			Warning( "Xbox 360 Network: Security Bypassed.\n" );
		}

		// Prepare for the number of connections required by the title
		g_pMatchFramework->GetMatchTitle()->PrepareNetStartupParams( &xnsp );

		INT err = XNetStartup( &xnsp );
		if ( err )
		{
			Warning( "Error! XNetStartup() failed, error %d.\n", err);
		}
		else
		{
			Msg( "\n"
				 "Xbox 360 secure network initialized:\n"
				 "     flags:            0x%08X\n"
				 "     reg XNKID/XNKEY:  %d\n"
				 "     reg XNADDR/XNKID: %d\n"
				 "     max UDP sockets:  %d\n"
				 "     max TCP sockets:  %d\n"
				 "     buffer size recv: %d K\n"
				 "     buffer size send: %d K\n"
				 "     QOS reply size:   %d b\n"
				 "     QOS timeout:      %d sec\n"
				 "     QOS retries:      %d\n"
				 "     QOS responses:    %d\n"
				 "     QOS pair wait:    %d sec\n"
				 "\n",
				 xnsp.cfgFlags,
				 xnsp.cfgSockMaxDgramSockets,
				 xnsp.cfgSockMaxStreamSockets,
				 xnsp.cfgSockDefaultRecvBufsizeInK,
				 xnsp.cfgSockDefaultSendBufsizeInK,
				 xnsp.cfgKeyRegMax,
				 xnsp.cfgSecRegMax,
				 xnsp.cfgQosDataLimitDiv4 * 4,
				 xnsp.cfgQosProbeTimeoutInSeconds,
				 xnsp.cfgQosProbeRetries,
				 xnsp.cfgQosSrvMaxSimultaneousResponses,
				 xnsp.cfgQosPairWaitTimeInSeconds
				);

			// initialize winsock 2.2
			WSAData wsaData = {0};
			err = WSAStartup( MAKEWORD(2,2), &wsaData );
			if ( err != 0 )
			{
				Warning( "Error! Failed to WSAStartup! err = %d.\n", err );
				net_noip = true;
			}
			else
			{
				Msg( "Socket layer initialized:\n"
					 "       wsa ver used: %d.%d\n"
					 "       wsa ver max:  %d.%d\n"
					 "       description:  %s\n"
					 "       sys status:   %s\n"
					 "\n",
					 LOBYTE( wsaData.wVersion ), HIBYTE( wsaData.wVersion ),
					 LOBYTE( wsaData.wHighVersion ), HIBYTE( wsaData.wHighVersion ),
					 wsaData.szDescription,
					 wsaData.szSystemStatus
					);

				err = XOnlineStartup();
				if ( err != ERROR_SUCCESS )
				{
					Warning( "Error! XOnlineStartup() failed, error %d.\n", err );
				}
				else
				{
					Msg( "XOnline services started.\n\n" );
				}
			}
		}
#elif defined( _WIN32 )
		// initialize winsock 2.0
		WSAData wsaData;
		if ( WSAStartup( MAKEWORD(2,0), &wsaData ) != 0 )
		{
			ConMsg( "Error! Failed to load network socket library.\n");
			net_noip = true;
		}
#elif defined( _PS3 )
		#if !defined( NO_STEAM )
		// Steam initializes networking
		if ( cellSysmoduleIsLoaded( CELL_SYSMODULE_NET ) != CELL_SYSMODULE_LOADED )
			net_noip = true;
		#else
		int err = cellSysmoduleLoadModule( CELL_SYSMODULE_NET );
		if ( err < 0 )
		{
			ConMsg( "Error! cellSysmoduleLoadModule error %d loading NET!\n", err );
			net_noip = true;
		}
		else
		{
			Msg( "cellSysmoduleLoadModule loaded NET.\n" );

			sys_net_initialize_parameter_t netParams;
			memset( &netParams, 0, sizeof( netParams ) );

			// Prepare for the number of connections required by the title
			g_pMatchFramework->GetMatchTitle()->PrepareNetStartupParams( &netParams );

			err = sys_net_initialize_network_ex( &netParams );
			if ( err < 0 )
			{
				ConMsg( "Error! sys_net_initialize_network_ex error %d ( %d kBytes of memory allocated )!\n", err, netParams.memory_size / 1024 );
				net_noip = true;

				cellSysmoduleUnloadModule( CELL_SYSMODULE_NET );
			}
			else
			{
				Msg( "sys_net_initialize_network_ex succeeded ( %d kBytes of memory allocated )!\n", netParams.memory_size / 1024 );

				int err = cellNetCtlInit();
				
				// GSidhu - in case of NO_STEAM we try and init this lib twice
				if ( (err < 0) && (err != CELL_NET_CTL_ERROR_NOT_TERMINATED) )
				{
					ConMsg( "Error! cellNetCtlInit error %d!\n", err );
					net_noip = true;

//					sys_net_finalize_network();
//					cellSysmoduleUnloadModule( CELL_SYSMODULE_NET );
				}
				else
				{
					Msg( "cellNetCtlInit succeeded.\n\n" );
				}
			}
		}
		#endif // NO_STEAM
#endif
	}

	Assert( NET_MAX_PAYLOAD < (1<<NET_MAX_PAYLOAD_BITS) );
	Assert( MAX_FILE_SIZE < (1<<MAX_FILE_SIZE_BITS) );

	net_time = 0.0f;

	//
	// Process ports configuration
	//
	{
		// Host port
		int nHostPort = hostport.GetInt();
		nHostPort = CommandLine()->ParmValue( "-port", nHostPort );
		nHostPort = CommandLine()->ParmValue( "+port", nHostPort );
		nHostPort = CommandLine()->ParmValue( "+hostport", nHostPort );
		hostport.SetValue( nHostPort );

		// Client port
		int nClientPort = clientport.GetInt();
		nClientPort = CommandLine()->ParmValue( "+clientport", nClientPort );
		clientport.SetValue( nClientPort );

		// HLTV ports
		{
			int nHltvPort = hltvport.GetInt();
			nHltvPort = CommandLine()->ParmValue( "+tv_port", nHltvPort );
			hltvport.SetValue( nHltvPort );
		}

		{
			int nHltvPort1 = hltvport1.GetInt();
			nHltvPort1 = CommandLine()->ParmValue( "+tv_port1", nHltvPort1 );
			hltvport1.SetValue( nHltvPort1 );
		}
	}

	// clear static stuff
	net_sockets.EnsureCount( MAX_SOCKETS );
	net_packets.EnsureCount( MAX_SOCKETS );
	net_splitpackets.EnsureCount( MAX_SOCKETS );

	for ( int i = 0; i < MAX_SOCKETS; ++i )
	{
		s_pLagData[i] = NULL;
		Q_memset( &net_sockets[i], 0, sizeof(netsocket_t) );
	}

	if ( const char *ip = CommandLine()->ParmValue( "-ip" ) ) // if they had a command line option for IP
	{
		ipname.SetValue( ip );  // update the cvar right now, this will get overwritten by "stuffcmds" later
	}
	if ( const char *ip_tv = CommandLine()->ParmValue( "-ip_tv" ) ) // if they had a command line option for IP for GOTV
	{
		ipname_tv.SetValue( ip_tv );  // update the cvar right now, this will get overwritten by "stuffcmds" later
	}
	if ( const char *ip_tv1 = CommandLine()->ParmValue( "-ip_tv1" ) ) // if they had a command line option for IP for GOTV
	{
		ipname_tv1.SetValue( ip_tv1 );  // update the cvar right now, this will get overwritten by "stuffcmds" later
	}
	if ( const char *ip_relay = CommandLine()->ParmValue( "-ip_relay" ) ) // if they had a command line option for IP relay for GOTV
	{
		ipname_relay.SetValue( ip_relay );  // update the cvar right now, this will get overwritten by "stuffcmds" later
	}
	if ( const char *ip_steam = CommandLine()->ParmValue( "-ip_steam" ) ) // if they had a command line option for IP for Steam
	{
		ipname_steam.SetValue( ip_steam );  // update the cvar right now, this will get overwritten by "stuffcmds" later
	}

	if ( bIsDedicated )
	{
		// set dedicated MP mode
		NET_SetDedicated();
	}
	else
	{
		// set SP mode
		NET_ConfigLoopbackBuffers( true );
	}

	NET_InitParanoidMode();

	NET_SetMultiplayer( !!( g_pMatchFramework->GetMatchTitle()->GetTitleSettingsFlags() & MATCHTITLE_SETTING_MULTIPLAYER ) );

	// Go ahead and create steam datagram client, and start measuring pings to data centers
	#ifndef DEDICATED
	if ( CheckInitSteamDatagramClientLib() )
	{
		if ( ::SteamNetworkingUtils() )
			::SteamNetworkingUtils()->CheckPingDataUpToDate( 0.0f );
	}
	#endif
}

/*
====================
NET_Shutdown

====================
*/
void NET_Shutdown (void)
{
	int nError = 0;

	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		NET_ClearLaggedList( &s_pLagData[i] );
	}

	g_pQueuedPackedSender->Shutdown();

	net_multiplayer = false;
	net_dedicated = false;

	NET_CloseAllSockets();
	NET_ConfigLoopbackBuffers( false );
#ifndef DEDICATED
	SteamDatagramClient_Kill();
#endif

#if defined(_WIN32)
	if ( !net_noip )
	{
#if defined(_X360)
		nError = XOnlineCleanup();
		if ( nError != ERROR_SUCCESS )
		{
			Msg( "Warning! Failed to complete XOnlineCleanup = 0x%x.\n", nError );
		}
#endif	// _X360

		nError = WSACleanup();
		if ( nError )
		{
			Msg("Failed to complete WSACleanup = 0x%x.\n", nError );
		}
	}
#elif defined( _PS3 )
	#if !defined( NO_STEAM )
	// Steam manages networking
	#else
	if ( !net_noip )
	{
		cellNetCtlTerm();
		sys_net_finalize_network();
		cellSysmoduleUnloadModule( CELL_SYSMODULE_NET );
	}
	#endif
#endif	// _WIN32

	Assert( s_NetChannels.Count() == 0 );
	Assert( s_PendingSockets.Count() == 0);
}

void NET_PrintChannelStatus( INetChannel * chan )
{
	Msg( "NetChannel '%s':\n", chan->GetName() );
	Msg( "- remote IP: %s\n", chan->GetAddress() );
	Msg( "- online: %s\n", COM_FormatSeconds( chan->GetTimeConnected() ) );
	Msg( "- reliable: %s\n", chan->HasPendingReliableData()?"pending data":"available" );
	Msg( "- latency: %.1f, loss %.2f\n", chan->GetAvgLatency(FLOW_OUTGOING), chan->GetAvgLoss(FLOW_INCOMING) );
	Msg( "- packets: in %.1f/s, out %.1f/s\n", chan->GetAvgPackets(FLOW_INCOMING), chan->GetAvgPackets(FLOW_OUTGOING) );
	Msg( "- choke: in %.2f, out %.2f\n", chan->GetAvgChoke(FLOW_INCOMING), chan->GetAvgChoke(FLOW_OUTGOING) );
	Msg( "- flow: in %.1f, out %.1f kB/s\n", chan->GetAvgData(FLOW_INCOMING)/1024.0f, chan->GetAvgData(FLOW_OUTGOING)/1024.0f );
	Msg( "- total: in %.1f, out %.1f MB\n\n", (float)chan->GetTotalData(FLOW_INCOMING)/(1024*1024), (float)chan->GetTotalData(FLOW_OUTGOING)/(1024*1024) );
}

CON_COMMAND( net_channels, "Shows net channel info" )
{
	int numChannels = s_NetChannels.Count();

	if ( numChannels == 0 )
	{
		ConMsg( "No active net channels.\n" );
		return;
	}

	AUTO_LOCK_FM( s_NetChannels );
	for ( int i = 0; i < numChannels; i++ )
	{
		NET_PrintChannelStatus( s_NetChannels[i] );
	}
}

CON_COMMAND( net_start, "Inits multiplayer network sockets" )
{
	net_multiplayer = true;
	NET_Config();
}

CON_COMMAND( net_status, "Shows current network status" )
{
	AUTO_LOCK_FM( s_NetChannels );
	int numChannels = s_NetChannels.Count();

	ConMsg("Net status for host %s:\n", 
		net_local_adr.ToString(true) );

	ConMsg("- Config: %s, %s, %i connections\n",
		net_multiplayer?"Multiplayer":"Singleplayer",
		net_dedicated?"dedicated":"listen",
		numChannels	);
		

	ConMsg("- Ports: " );
	for ( int k = 0; k < MAX_SOCKETS; ++ k )
	{
		ConMsg( "%s%d %u, ", DescribeSocket( k ), k, NET_GetUDPPort( k ) );
	}
	ConMsg( "%d total.\n", MAX_SOCKETS );

	if ( numChannels <= 0 )
	{
		return;
	}

	// gather statistics:

	float avgLatencyOut = 0;
	float avgLatencyIn = 0;
	float avgPacketsOut = 0;
	float avgPacketsIn = 0;
	float avgLossOut = 0;
	float avgLossIn = 0;
	float avgDataOut = 0;
	float avgDataIn = 0;

	for ( int i = 0; i < numChannels; i++ )
	{
		CNetChan *chan = s_NetChannels[i];

		avgLatencyOut += chan->GetAvgLatency(FLOW_OUTGOING);
		avgLatencyIn += chan->GetAvgLatency(FLOW_INCOMING);

		avgLossIn += chan->GetAvgLoss(FLOW_INCOMING);
		avgLossOut += chan->GetAvgLoss(FLOW_OUTGOING);

		avgPacketsIn += chan->GetAvgPackets(FLOW_INCOMING);
		avgPacketsOut += chan->GetAvgPackets(FLOW_OUTGOING);
		
		avgDataIn += chan->GetAvgData(FLOW_INCOMING);
		avgDataOut += chan->GetAvgData(FLOW_OUTGOING);
	}

	ConMsg( "- Latency: avg out %.2fs, in %.2fs\n",  avgLatencyOut/numChannels, avgLatencyIn/numChannels );
 	ConMsg( "- Loss:    avg out %.1f, in %.1f\n", avgLossOut/numChannels, avgLossIn/numChannels );
	ConMsg( "- Packets: net total out  %.1f/s, in %.1f/s\n", avgPacketsOut, avgPacketsIn );
	ConMsg( "           per client out %.1f/s, in %.1f/s\n", avgPacketsOut/numChannels, avgPacketsIn/numChannels );
	ConMsg( "- Data:    net total out  %.1f, in %.1f kB/s\n", avgDataOut/1024.0f, avgDataIn/1024.0f );
	ConMsg( "           per client out %.1f, in %.1f kB/s\n", (avgDataOut/numChannels)/1024.0f, (avgDataIn/numChannels)/1024.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Generic buffer compression from source into dest
// Input  : *dest - 
//			*destLen - 
//			*source - 
//			sourceLen - 
// Output : int
//-----------------------------------------------------------------------------
bool NET_BufferToBufferCompress( char *dest, unsigned int *destLen, char *source, unsigned int sourceLen )
{
	Assert( dest );
	Assert( destLen );
	Assert( source );

	Q_memcpy( dest, source, sourceLen );
	CLZSS s;
	unsigned int uCompressedLen = 0;
	byte *pbOut = s.Compress( (byte *)source, sourceLen, &uCompressedLen );
	if ( pbOut && uCompressedLen > 0 && uCompressedLen <= *destLen )
	{
		Q_memcpy( dest, pbOut, uCompressedLen );
		*destLen = uCompressedLen;
		free( pbOut );
	}
	else
	{
		if ( pbOut )
		{
			free( pbOut );
		}
		Q_memcpy( dest, source, sourceLen );
		*destLen = sourceLen;
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Generic buffer decompression from source into dest
// Input  : *dest - 
//			*destLen - 
//			*source - 
//			sourceLen - 
// Output : int
//-----------------------------------------------------------------------------
bool NET_BufferToBufferDecompress( char *dest, unsigned int *destLen, char *source, unsigned int sourceLen )
{
	CLZSS s;
	if ( s.IsCompressed( (byte *)source ) )
	{
		unsigned int uDecompressedLen = s.GetActualSize( (byte *)source );
		if ( uDecompressedLen > *destLen )
		{
			Warning( "NET_BufferToBufferDecompress with improperly sized dest buffer (%u in, %u needed)\n", *destLen, uDecompressedLen );
			return false;
		}
		else
		{
			*destLen = s.SafeUncompress( (byte *)source, (byte *)dest, *destLen );
		}
	}
	else
	{
		if ( sourceLen > *destLen )
		{
			Warning( "NET_BufferToBufferDecompress with improperly sized dest buffer (%u in, %u needed)\n", *destLen, sourceLen );
			return false;
		}

		Q_memcpy( dest, source, sourceLen );
		*destLen = sourceLen;
	}

	return true;
}

void NET_SleepUntilMessages( int nMilliseconds )
{
	fd_set fdset;
	FD_ZERO(&fdset);

	if ( !net_sockets[NS_SERVER].hUDP )
	{
		Sys_Sleep( nMilliseconds );
		return;
	}

	unsigned int nSocket = (unsigned int)net_sockets[NS_SERVER].hUDP;
	FD_SET( nSocket, &fdset );
	struct timeval tv = { 0 };
	tv.tv_usec = nMilliseconds * 1000;
	select( nSocket + 1, &fdset, NULL, NULL, &tv );
}

bool NET_GetPublicAdr( netadr_t &adr )
{
	bool bRet = false;
	unsigned short port = NET_GetUDPPort( NS_SERVER );
	if ( net_public_adr.GetString()[ 0 ] )
	{
		bRet = true;
		adr.SetType( NA_IP );
		adr.SetFromString( net_public_adr.GetString() );
		if ( adr.GetPort() == 0 )
			adr.SetPort( port );
	}
#if !defined( _X360 ) && !defined( NO_STEAM )
	else if ( NET_IsDedicated() &&
		Steam3Server().SteamGameServer()->GetPublicIP() != 0u )
	{
		bRet = true;
		adr.SetType( NA_IP );
		adr.SetIPAndPort( Steam3Server().SteamGameServer()->GetPublicIP(), port );
	}
#endif
	return bRet;
}

void NET_SteamDatagramServerListen()
{
	// Receiving on steam datagram transport?
	// We only open one interface object (corresponding to one UDP port).
	// The other "sockets" are different channels on this interface
	if ( sv_steamdatagramtransport_port.GetInt() == 0 )
		return;
	if ( g_pSteamDatagramGameserver )
		return;

	SteamDatagramErrMsg errMsg;
	EResult result;
	g_pSteamDatagramGameserver = SteamDatagram_GameserverListen( GetSteamUniverse(), sv_steamdatagramtransport_port.GetInt(), &result, errMsg );
	if ( g_pSteamDatagramGameserver )
	{
		Msg( "Listening for Steam datagram transport on port %d\n", sv_steamdatagramtransport_port.GetInt() );
	}
	else
	{
		Warning( "SteamDatagram_GameserverListen failed with error code %d.  %s\n", result, errMsg );

		// Clear the convar so we don't advertise that we are listening!
		sv_steamdatagramtransport_port.SetValue( 0 );
	}
}

void NET_TerminateConnection( int sock, const ns_address &peer )
{
#if defined( USE_STEAM_SOCKETS )
	if ( peer.IsType<netadr_t)() )
	{
		uint64 steamIDRemote = g_pSteamSocketMgr->GetSteamIDForRemote( peer.AsType<netadr_t>() );
		NET_TerminateSteamConnection( steamIDRemote );
	}
#endif
#ifndef DEDICATED
	if ( peer == g_addrSteamDatagramProxiedGameServer )
		CloseSteamDatagramClientConnection();
#endif		
}

//////////////////////////////////////////////////////////////////////////
//
// Cryptography support code
//
//////////////////////////////////////////////////////////////////////////

#ifdef Verify
#undef Verify
#endif

#define bswap_16 __bswap_16
#define bswap_64 __bswap_64

#include "cryptlib.h"
#include "rsa.h"
#include "osrng.h"

using namespace CryptoPP;
typedef AutoSeededX917RNG<AES> CAutoSeededRNG;

// list of auto-seeded RNG pointers
// these are very expensive to construct, so it makes sense to cache them
CTSList<CAutoSeededRNG>&
GlobalRNGList()
{
	static CTSList<CAutoSeededRNG> g_tslistPAutoSeededRNG;
	return g_tslistPAutoSeededRNG;
}

// to avoid deconstructor order issuses we allow to manually free the list
void FreeListRNG()
{
	GlobalRNGList().Purge();
}

//-----------------------------------------------------------------------------
// Purpose: thread-safe access to a pool of cryptoPP random number generators
//-----------------------------------------------------------------------------
class CPoolAllocatedRNG
{
public:
	CPoolAllocatedRNG()
	{
		m_pRNGNode = GlobalRNGList().Pop();
		if ( !m_pRNGNode )
		{
			m_pRNGNode = new CTSList<CAutoSeededRNG>::Node_t;
		}
	}

	~CPoolAllocatedRNG()
	{
		GlobalRNGList().Push( m_pRNGNode );
	}

	CAutoSeededRNG &GetRNG()
	{
		return m_pRNGNode->elem;
	}

private:
	CTSList<CAutoSeededRNG>::Node_t *m_pRNGNode;
};


//////////////////////////////////////////////////////////////////////////
//
// Encrypted networking
//
//////////////////////////////////////////////////////////////////////////

bool NET_CryptVerifyServerCertificateAndAllocateSessionKey( bool bOfficial, const ns_address &from,
	const byte *pchKeyPub, int numKeyPub,
	const byte *pchKeySgn, int numKeySgn,
	byte **pbAllocatedKey, int *pnAllocatedCryptoBlockSize )
{
	static const byte CsgoMasterPublicKey[] = { 0 }; // Removed for partner depot

	// For now, only IPv4 addresses allowed.  Shouldn't be too hard to figure out how to
	// generate blocks to sign for other types of addresses
	uint32 unCertIP = 0;
	switch ( from.GetAddressType() )
	{
		case NSAT_NETADR:
			unCertIP = from.AsType<netadr_t>().GetIPHostByteOrder();
			break;
		case NSAT_PROXIED_GAMESERVER:
		{
			unCertIP = SteamNetworkingUtils()->GetIPForServerSteamIDFromTicket( from.m_steamID.GetSteamID() );
			if ( unCertIP == 0 )
			{
				Warning( "NET_CryptVerifyServerCertificateAndAllocateSessionKey - cannot check signature for proxied server '%s', because we don't have an SDR ticket to that server.\n", ns_address_render( from ).String() );
				Assert(false);
				return false;
			}
			break;
		}
	}
	if ( unCertIP == 0 )
	{
		Warning( "NET_CryptVerifyServerCertificateAndAllocateSessionKey - cannot check signature for '%s', cannot determine IP to use\n", ns_address_render( from ).String() );
		Assert(false);
		return false;
	}

	//
	// Verify certificate
	//
	bool bCertificateValidated = false;
	for ( int numAddrBits = 32; ( numAddrBits >= 16 ) && !bCertificateValidated; -- numAddrBits )
	{
		CUtlBuffer bufSignature;
		try           // handle any exceptions crypto++ may throw
		{
			StringSource stringSourcePublicKey( CsgoMasterPublicKey, Q_ARRAYSIZE( CsgoMasterPublicKey ), true );
			RSASSA_PKCS1v15_SHA_Verifier pub( stringSourcePublicKey );
			CUtlBuffer bufDataFile;
			bufDataFile.EnsureCapacity( numKeyPub + 20 );
			bufDataFile.Put( pchKeyPub, numKeyPub );

			netadr_t adrMasked( unCertIP & ( (~0) << (32-numAddrBits) ), 0 );
			char chBuffer[20] = {};
			V_sprintf_safe( chBuffer, "%s/%u", adrMasked.ToString( true ), numAddrBits );
			bufDataFile.Put( chBuffer, V_strlen( chBuffer ) );

			bCertificateValidated = pub.VerifyMessage( ( byte* ) bufDataFile.Base(), bufDataFile.TellPut(), pchKeySgn, numKeySgn );
#ifdef _DEBUG
			DevMsg( "NET_CryptVerifyServerCertificateAndAllocateSessionKey: VerifyMessage for %s %s\n",
				chBuffer, bCertificateValidated ? "succeeded" : "failed" );
#endif
		}
		catch ( Exception e )
		{
#ifdef _DEBUG
			Warning( "NET_CryptVerifyServerCertificateAndAllocateSessionKey: VerifyMessage threw exception %s (%d)\n",
				e.what(), e.GetErrorType() );
#endif
		}
		catch ( ... )
		{
#ifdef _DEBUG
			Warning( "NET_CryptVerifyServerCertificateAndAllocateSessionKey: VerifyMessage threw unknown exception\n" );
#endif
		}
	}
	if ( !bCertificateValidated )
		return false;

	//
	// Allocate client key
	//
	float flTime = Plat_FloatTime();
	RandomSeed( * reinterpret_cast< int * >( &flTime ) );
	byte ubClientKey[NET_CRYPT_KEY_LENGTH];
	for ( int j = 0; j < NET_CRYPT_KEY_LENGTH; ++j )
	{
		ubClientKey[ j ] = ( byte ) ( unsigned int ) RandomInt( 0, 255 );
	}

	//
	// Encrypt client key using the public key
	//
	try           // handle any exceptions crypto++ may throw
	{
		StringSource stringSourcePublicKey( pchKeyPub, numKeyPub, true );
		RSAES_OAEP_SHA_Encryptor rsaEncryptor( stringSourcePublicKey );

		// calculate how many blocks of encryption will we need to do
		AssertFatal( rsaEncryptor.FixedMaxPlaintextLength() <= UINT32_MAX );
		uint32 cBlocks = 1 + ( ( NET_CRYPT_KEY_LENGTH - 1 ) / ( uint32 ) rsaEncryptor.FixedMaxPlaintextLength() );
		// calculate how big the output will be
		AssertFatal( rsaEncryptor.FixedCiphertextLength() <= UINT32_MAX / cBlocks );
		uint32 cubCipherText = cBlocks * ( uint32 ) rsaEncryptor.FixedCiphertextLength();

		// ensure there is sufficient room in output buffer for result
		byte *pbResult = new byte[ NET_CRYPT_KEY_LENGTH + cubCipherText ];
		Q_memcpy( pbResult, ubClientKey, NET_CRYPT_KEY_LENGTH );
		
		*pbAllocatedKey = pbResult;
		*pnAllocatedCryptoBlockSize = cubCipherText;

		// Encryption pass
		uint32 cubPlaintextData = NET_CRYPT_KEY_LENGTH;
		const byte *pubPlaintextData = ubClientKey;
		byte *pubEncryptedData = pbResult + NET_CRYPT_KEY_LENGTH;

		// encrypt the message, using as many blocks as required
		CPoolAllocatedRNG rng;
		for ( uint32 nBlock = 0; nBlock < cBlocks; nBlock++ )
		{
			// encrypt either all remaining plaintext, or maximum allowed plaintext per RSA encryption operation
			uint32 cubToEncrypt = MIN( cubPlaintextData, ( uint32 ) rsaEncryptor.FixedMaxPlaintextLength() );
			// encrypt the plaintext
			rsaEncryptor.Encrypt( rng.GetRNG(), pubPlaintextData, cubToEncrypt, pubEncryptedData );
			// adjust input and output pointers and remaining plaintext byte count
			pubPlaintextData += cubToEncrypt;
			cubPlaintextData -= cubToEncrypt;
			pubEncryptedData += rsaEncryptor.FixedCiphertextLength();
		}
		Assert( 0 == cubPlaintextData );         // should have no remaining plaintext to encrypt

#ifdef _DEBUG
		DevMsg( "NET_CryptVerifyServerCertificateAndAllocateSessionKey: Encrypted %u bytes key as %u bytes ciphertext\n",
			NET_CRYPT_KEY_LENGTH, cubCipherText );
#endif
		return true;
	}
	catch ( Exception e )
	{
#ifdef _DEBUG
		Warning( "NET_CryptVerifyServerCertificateAndAllocateSessionKey: Encrypt threw exception %s (%d)\n",
			e.what(), e.GetErrorType() );
#endif
		return false;
	}
	catch ( ... )
	{
#ifdef _DEBUG
		Warning( "NET_CryptVerifyServerCertificateAndAllocateSessionKey: Encrypt threw unknown exception\n" );
#endif
		return false;
	}
}

bool NET_CryptVerifyClientSessionKey( bool bOfficial,
	const byte *pchKeyPri, int numKeyPri,
	const byte *pbEncryptedKey, int numEncryptedBytes,
	byte *pbPlainKey, int numPlainKeyBytes )
{
	try           // handle any exceptions crypto++ may throw
	{
		StringSource stringSourcePrivateKey( pchKeyPri, numKeyPri, true );
		RSAES_OAEP_SHA_Decryptor rsaDecryptor( stringSourcePrivateKey );

		// calculate how many blocks of decryption will we need to do
		AssertFatal( rsaDecryptor.FixedCiphertextLength() <= UINT32_MAX );
		uint32 cubFixedCiphertextLength = ( uint32 ) rsaDecryptor.FixedCiphertextLength();
		
		// Ensure encrypted data is valid and has length that is exact multiple of 128 bytes
		uint32 cubEncryptedData = numEncryptedBytes;
		if ( 0 != ( cubEncryptedData % cubFixedCiphertextLength ) )
		{
#ifdef _DEBUG
			Warning( "NET_CryptVerifyClientSessionKey: invalid ciphertext length %d, needs to be a multiple of %d\n",
				cubEncryptedData, cubFixedCiphertextLength );
#endif
			return false;
		}
		uint32 cBlocks = cubEncryptedData / cubFixedCiphertextLength;

		// calculate how big the maximum output will be
		size_t cubMaxPlaintext = rsaDecryptor.MaxPlaintextLength( rsaDecryptor.FixedCiphertextLength() );
		AssertFatal( cubMaxPlaintext <= UINT32_MAX / cBlocks );
		uint32 cubPlaintextDataMax = cBlocks * ( uint32 ) cubMaxPlaintext;
		Assert( cubPlaintextDataMax > 0 );
		// ensure there is sufficient room in output buffer for result
		if ( int( cubPlaintextDataMax ) >= numPlainKeyBytes )
		{
#ifdef _DEBUG
			Warning( "NET_CryptVerifyClientSessionKey: insufficient output buffer for decryption, needed %d got %d\n",
				cubPlaintextDataMax, numPlainKeyBytes );
#endif
			return false;
		}

		// decrypt the data, using as many blocks as required
		CPoolAllocatedRNG rng;
		uint32 cubPlaintextData = 0;
		for ( uint32 nBlock = 0; nBlock < cBlocks; nBlock++ )
		{
			// decrypt one block (always of fixed size)
			int cubToDecrypt = cubFixedCiphertextLength;
			DecodingResult decodingResult = rsaDecryptor.Decrypt( rng.GetRNG(), pbEncryptedKey, cubToDecrypt, pbPlainKey );
			if ( !decodingResult.isValidCoding )
			{
#ifdef _DEBUG
				Warning( "NET_CryptVerifyClientSessionKey: failed to decrypt\n" );
#endif
				return false;
			}
			// adjust input and output pointers and remaining encrypted byte count
			pbEncryptedKey += cubToDecrypt;
			cubEncryptedData -= cubToDecrypt;
			pbPlainKey += decodingResult.messageLength;
			AssertFatal( decodingResult.messageLength <= UINT32_MAX );
			cubPlaintextData += ( uint32 ) decodingResult.messageLength;
		}
		Assert( 0 == cubEncryptedData );  // should have no remaining encrypted data to decrypt
		
		if ( cubPlaintextData != NET_CRYPT_KEY_LENGTH )
		{
#ifdef _DEBUG
			Warning( "NET_CryptVerifyClientSessionKey: decrypted %u bytes when expecting %u bytes\n", cubPlaintextData, NET_CRYPT_KEY_LENGTH );
#endif
			return false;
		}

		return true;
	}
	catch ( Exception e )
	{
#ifdef _DEBUG
		Warning( "NET_CryptVerifyClientSessionKey: Decrypt threw exception %s (%d)\n",
			e.what(), e.GetErrorType() );
#endif
		return false;
	}
	catch ( ... )
	{
#ifdef _DEBUG
		Warning( "NET_CryptVerifyClientSessionKey: Decrypt threw unknown exception\n" );
#endif
		return false;
	}
}

bool NET_CryptGetNetworkCertificate( ENetworkCertificate_t eType, const byte **pbData, int *pnumBytes )
{
	static char const *s_szCertificateFile = CommandLine()->ParmValue( "-certificate", ( char const * ) NULL );
	if ( !s_szCertificateFile )
		return false;

	static bool s_bCertificateFilesLoaded = false;
	static CUtlBuffer bufCertificate;
	static int s_nCertificateOffset[ k_ENetworkCertificate_Max ] = {};
	static int s_nCertificateLength[ k_ENetworkCertificate_Max ] = {};
	if ( !s_bCertificateFilesLoaded )
	{
		s_bCertificateFilesLoaded = true;

		if ( !g_pFullFileSystem->ReadFile( s_szCertificateFile, NULL, bufCertificate ) ||
			!bufCertificate.Base() || ( bufCertificate.Size() < ( k_ENetworkCertificate_Max + 1 ) * 3 * sizeof( int ) ) )
		{
			Warning( "NET_CryptGetNetworkCertificate failed to load certificate '%s'\n", s_szCertificateFile );
			Plat_ExitProcess( 0 );	// we must exit process, but we will skip writing the core dump
			return false;
		}

		if ( V_memcmp( bufCertificate.Base(), "CSv1", 4 ) )
		{
			Warning( "NET_CryptGetNetworkCertificate certificate version mismatch '%s'\n", s_szCertificateFile );
			Plat_ExitProcess( 0 );	// we must exit process, but we will skip writing the core dump
			return false;
		}
		
		bufCertificate.GetInt(); // CSv1
		int nTOC = bufCertificate.GetInt();
		if ( nTOC != k_ENetworkCertificate_Max )
		{
			Warning( "NET_CryptGetNetworkCertificate certificate TOC length mismatch '%s'\n", s_szCertificateFile );
			Plat_ExitProcess( 0 );	// we must exit process, but we will skip writing the core dump
			return false;
		}

		for ( int j = 0; j < k_ENetworkCertificate_Max; ++ j )
		{
			int nFileID = bufCertificate.GetInt();
			if ( nFileID != j )
			{
				Warning( "NET_CryptGetNetworkCertificate certificate TOC entry %d invalid in '%s'\n", j, s_szCertificateFile );
				Plat_ExitProcess( 0 );	// we must exit process, but we will skip writing the core dump
				return false;
			}

			s_nCertificateOffset[j] = bufCertificate.GetInt();
			s_nCertificateLength[j] = bufCertificate.GetInt();
		}
	}

	*pbData = ( ( const byte * ) bufCertificate.Base() ) + s_nCertificateOffset[eType];
	*pnumBytes = s_nCertificateLength[eType];

	return true;
}

#ifdef _DEBUG
CON_COMMAND( net_encrypt_key_generate, "Generate a public/private keypair" )
{
	if ( args.ArgC() <= 2 )
	{
		Warning( "Usage: net_encrypt_key_generate <numbits> <filename>\n" );
		return;
	}

	uint32 cKeyBits = Q_atoi( args.Arg( 1 ) );

	bool bSuccess = false;
	std::string strPrivateKey;
	std::string strPublicKey;

	try           // handle any exceptions crypto++ may throw
	{
		// generate private key
		StringSink stringSinkPrivateKey( strPrivateKey );
		CPoolAllocatedRNG rng;
		RSAES_OAEP_SHA_Decryptor priv( rng.GetRNG(), cKeyBits );
		priv.DEREncode( stringSinkPrivateKey );

		// generate public key
		StringSink stringSinkPublicKey( strPublicKey );
		RSAES_OAEP_SHA_Encryptor pub( priv );
		pub.DEREncode( stringSinkPublicKey );
		bSuccess = true;
	}
	catch ( Exception e )
	{
		Warning( "net_encrypt_key_generate: crypto++ threw exception %s (%d)\n",
			e.what(), e.GetErrorType() );
	}
	catch ( ... )
	{
		Warning( "net_encrypt_key_generate: crypto++ threw unknown exception\n" );
	}

	if ( bSuccess )
	{
		char chFile[256];
		
		CUtlBuffer bufPrivate( strPrivateKey.c_str(), strPrivateKey.length(), CUtlBuffer::READ_ONLY );
		V_sprintf_safe( chFile, "%s.private", args.Arg( 2 ) );
		if ( !g_pFullFileSystem->WriteFile( chFile, NULL, bufPrivate ) )
		{
			Warning( "net_encrypt_key_generate: failed to write %u bits keypair file '%s'\n", cKeyBits, chFile );
			return;
		}

		CUtlBuffer bufPublic( strPublicKey.c_str(), strPublicKey.length(), CUtlBuffer::READ_ONLY );
		V_sprintf_safe( chFile, "%s.public", args.Arg( 2 ) );
		if ( !g_pFullFileSystem->WriteFile( chFile, NULL, bufPublic ) )
		{
			Warning( "net_encrypt_key_generate: failed to write %u bits keypair file '%s'\n", cKeyBits, chFile );
			return;
		}

		Msg( "net_encrypt_key_generate: wrote %u bits keypair files '%s.private/public'\n", cKeyBits, args.Arg( 2 ) );
	}
	else
	{
		Warning( "net_encrypt_key_generate: failed to generate %u bits keypair files '%s.private/public'\n", cKeyBits, args.Arg( 2 ) );
	}
}

CON_COMMAND( net_encrypt_key_signature, "Compute key signature for the payloads" )
{
	if ( args.ArgC() <= 4 )
	{
		Warning( "Usage: net_encrypt_key_signature <file.privatekey> <file.payload> <string.payload> <output.file>\n" );
		return;
	}

	bool bRet = false;
	CUtlBuffer bufSignature;
	try           // handle any exceptions crypto++ may throw
	{
		CUtlBuffer bufPrivateKey;
		if ( !g_pFullFileSystem->ReadFile( args.Arg( 1 ), NULL, bufPrivateKey ) )
		{
			Warning( "net_encrypt_key_signature: failed to read private key file '%s'\n", args.Arg( 1 ) );
			return;
		}

		CUtlBuffer bufDataFile;
		if ( !g_pFullFileSystem->ReadFile( args.Arg( 2 ), NULL, bufDataFile ) )
		{
			Warning( "net_encrypt_key_signature: failed to read data file '%s'\n", args.Arg( 2 ) );
			return;
		}

		char const *szStringPayload = args.Arg( 3 );
		int nStringPayloadLength = Q_strlen( szStringPayload );
		if ( nStringPayloadLength > 0 )
			bufDataFile.Put( szStringPayload, nStringPayloadLength );

		StringSource stringSourcePrivateKey( ( byte * ) bufPrivateKey.Base(), bufPrivateKey.TellPut(), true );
		RSASSA_PKCS1v15_SHA_Signer rsaSigner( stringSourcePrivateKey );
		CPoolAllocatedRNG rng;

		bufSignature.EnsureCapacity( rsaSigner.MaxSignatureLength() );
		{
			size_t len = rsaSigner.SignMessage( rng.GetRNG(), ( byte * ) bufDataFile.Base(), bufDataFile.TellPut(), ( byte * ) bufSignature.Base() );
			bufSignature.SeekPut( CUtlBuffer::SEEK_HEAD, ( int32 ) ( uint32 ) len );
			bRet = true;
			Msg( "net_encrypt_key_signature: generated %u bytes signature for payload data +%u=%u bytes\n", bufSignature.TellPut(), nStringPayloadLength, bufDataFile.TellPut() );
		}
	}
	catch ( Exception e )
	{
		Warning( "net_encrypt_key_signature: SignMessage threw exception %s (%d)\n",
			e.what(), e.GetErrorType() );
	}
	catch ( ... )
	{
		Warning( "net_encrypt_key_signature: SignMessage threw unknown exception\n" );
	}

	if ( bRet )
	{
		if ( !g_pFullFileSystem->WriteFile( args.Arg( 4 ), NULL, bufSignature ) )
		{
			Warning( "net_encrypt_key_signature: failed to write file '%s'\n", args.Arg( 4 ) );
			return;
		}

		Msg( "net_encrypt_key_generate: wrote %u bytes signature file '%s'\n", bufSignature.TellPut(), args.Arg( 4 ) );
	}
	else
	{
		Warning( "net_encrypt_key_signature: failed\n" );
	}
}

CON_COMMAND( net_encrypt_key_compress, "Compress all key signatures into a single file" )
{
	if ( args.ArgC() <= 1 )
	{
		Warning( "Usage: net_encrypt_key_compress <file>\n" );
		return;
	}

	CUtlBuffer bufComposite;

	char const * arrFiles[] = { "public", "private", "signature" }; // ENetworkCertificate_t order
	CUtlBuffer bufData[ Q_ARRAYSIZE( arrFiles ) ];
	for ( int j = 0; j < Q_ARRAYSIZE( arrFiles ); ++ j )
	{
		char chFile[ 1024 ] = {};
		V_sprintf_safe( chFile, "%s.%s", args.Arg( 1 ), arrFiles[j] );
		if ( !g_pFullFileSystem->ReadFile( chFile, NULL, bufData[j] ) )
		{
			Warning( "net_encrypt_key_compress: failed to read data file '%s'\n", chFile );
			return;
		}
	}

	bufComposite.Put( "CSv1", 4 );
	bufComposite.PutInt( Q_ARRAYSIZE( arrFiles ) );
	int nDataOffsetBase = bufComposite.TellPut() + Q_ARRAYSIZE( arrFiles )*3*4;
	int nDataOffset = nDataOffsetBase;
	for ( int j = 0; j < Q_ARRAYSIZE( arrFiles ); ++ j )
	{
		bufComposite.PutInt( j );						// file ID
		bufComposite.PutInt( nDataOffset );				// offset
		bufComposite.PutInt( bufData[j].TellPut() );	// length
		nDataOffset += bufData[j].TellPut();
	}
	if ( bufComposite.TellPut() != nDataOffsetBase )
	{
		Warning( "net_encrypt_key_compress: failed to align composite TOC for '%s'\n", args.Arg( 1 ) );
		return;
	}
	for ( int j = 0; j < Q_ARRAYSIZE( arrFiles ); ++ j )
	{
		bufComposite.Put( bufData[j].Base(), bufData[j].TellPut() );
	}

	if ( !g_pFullFileSystem->WriteFile( args.Arg( 1 ), NULL, bufComposite ) )
	{
		Warning( "net_encrypt_key_compress: failed to write file '%s'\n", args.Arg( 1 ) );
		return;
	}

	for ( int j = 0; j < Q_ARRAYSIZE( arrFiles ); ++ j )
	{
		char chFile[ 1024 ] = {};
		V_sprintf_safe( chFile, "%s.%s", args.Arg( 1 ), arrFiles[j] );
		g_pFullFileSystem->RemoveFile( chFile );
	}

	Msg( "net_encrypt_key_compress: compressed file '%s' (%u bytes)\n", args.Arg( 1 ), bufComposite.TellPut() );
}

CON_COMMAND( net_encrypt_key_make_clusters, "Generate certificates and their signatures using master key" )
{
	if ( args.ArgC() <= 2 )
	{
		Warning( "Usage: net_encrypt_key_make_clusters <numbits> <file.privatekey>\n" );
		return;
	}

	char const * arrAddressMasks[] = {
//		"atl-1", "162.254.199.0/25"  ,
// 		"dxb-1", "185.25.183.0/25"  ,
// 		"eat-1", "192.69.96.0/23"	,
// 		"eat-2", "192.69.96.0/23"	,
//		"eat-3", "192.69.96.0/23"	,
// 		"eat-4", "192.69.96.0/23"	,
// 		"gru-4", "205.185.194.0/24"	,
//		"iad-1", "208.78.164.90/23"	,
// 		"iad-2", "208.78.164.0/23"	,
// 		"iad-3", "208.78.164.0/23"	,
// 		"iad-4", "208.78.166.0/24"	,
//		"lax-1", "162.254.194.0/24"	,
// 		"lux-1", "146.66.152.0/23"	,
// 		"lux-2", "146.66.152.0/23"	,
// 		"lux-3", "146.66.152.0/23"	,
// 		"lux-4", "146.66.158.0/23"	,
// 		"lux-5", "146.66.158.0/23"	,
// 		"lux-6", "146.66.158.0/23"	,
//		"lux-7", "155.133.240.0/23"	,
//		"lux-8", "155.133.240.0/23"	,
// 		"sgp-1", "103.28.54.0/23"	,
// 		"sgp-2", "103.28.54.0/23"	,
// 		"sgp-3", "103.28.54.0/23"	,
// 		"sto-1", "146.66.156.0/23"	,
// 		"sto-2", "146.66.156.0/23"	,
// 		"sto-3", "146.66.156.0/23"	,
// 		"sto-4", "185.25.180.0/23"	,
// 		"sto-5", "185.25.180.0/23"	,
// 		"sto-6", "185.25.180.0/23"	,
//		"sto-7", "155.133.242.0/23"	,
//		"sto-8", "155.133.242.0/23"	,
// 		"syd-1", "103.10.125.0/24",
// 		"vie-1", "146.66.155.0/24"	,
// 		"vie-2", "185.25.182.0/24"	,
// 		"jhb-1", "197.80.200.48/29"	,
 		"jhb-1", "155.133.238.0/24"	,
 		"jhb-2", "155.133.238.0/24"	,
//		"cpt-1", "197.84.209.20/30",
// 		"bom-1", "45.113.191.128/27",
//		"bom-1", "45.113.137.128/27",
//		"tyo-1", "45.121.186.0/23",
//		"tyo-2", "45.121.186.0/23",
//		"hkg-1", "155.133.244.0/24",
//		"mad-1", "155.133.246.0/23",
// 		"blv-1", "172.16.0.0/16",
//		"scl-1", "155.133.249.0/24",
//		"lim-1", "143.137.146.0/24",
//		"ord-1", "155.133.226.0/24",
//		"vie-3", "155.133.228.0/23",
//		"syd-2", "155.133.227.0/24",
//		"gru-5", "155.133.224.0/23",
//		"jhb-2", "155.133.238.0/24",
//		"bom-2", "155.133.233.0/24",
//		"maa-1", "155.133.232.0/24",
//		"waw-1", "155.133.230.0/23",
//		"lim-2", "190.216.121.0/24",
//		"atl-2", "155.133.234.0/24",
//		"ord-1", "208.78.167.0/24",
//		"ord-2", "208.78.167.0/24",
//		"tsn-1", "125.39.181.0/24",
//		"tsn-2", "60.28.165.128/25",
//		"can-2", "125.88.174.0/24",
//		"sha-3", "121.46.225.0/24",
//		"eleague-major-atlanta-2017", "172.27.10.20/31",
	};

	for ( int j = 0; j < Q_ARRAYSIZE( arrAddressMasks )/2; ++j )
	{
		char const *szName = arrAddressMasks[ 2*j ];
		char const *szAddr = arrAddressMasks[ 2*j+1 ];
		char const *szSlash = strchr( szAddr, '/' );
		if ( !szSlash ) continue;

		char chBuffer[ 1024 ] = {};
		V_sprintf_safe( chBuffer, "net_encrypt_key_generate %s %s.certificate;\n", args.Arg( 1 ), szName );
		Cbuf_AddText( CBUF_FIRST_PLAYER, chBuffer );

		V_sprintf_safe( chBuffer, "net_encrypt_key_signature \"%s\" %s.certificate.public \"%s\" %s.certificate.signature;",
			args.Arg( 2 ), szName, szAddr, szName );
		Cbuf_AddText( CBUF_FIRST_PLAYER, chBuffer );

		V_sprintf_safe( chBuffer, "net_encrypt_key_compress %s.certificate;\n", szName );
		Cbuf_AddText( CBUF_FIRST_PLAYER, chBuffer );

		Cbuf_Execute();
	}
}
#endif


