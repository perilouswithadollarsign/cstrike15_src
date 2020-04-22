//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// net.h -- Half-Life's interface to the networking layer
// For banning IP addresses (or allowing private games)
#ifndef NET_H
#define NET_H
#ifdef _WIN32
#pragma once
#endif

#include "common.h"
#include "bitbuf.h"
#include "netadr.h"
#include "inetchannel.h"
#include "networksystem/inetworksystem.h"

class ISteamDatagramTransportClient;

// Flow control bytes per second limits:
// 16,000 bytes per second = 128kbps
// default rate: 192kbytes per second = 1.5mbps
// 0.75Mbytes per second = 6mbps
#define MIN_RATE		  16000
#define DEFAULT_RATE	 196608
#define MAX_RATE		 786432

#ifdef _GAMECONSOLE
#define SIGNON_TIME_OUT				75.0f  // signon disconnect timeout
#else
#define SIGNON_TIME_OUT				300.0f  // signon disconnect timeout
#endif
#define SIGNON_TIME_OUT_360			75.0f

#define FRAGMENT_BITS		8
#define FRAGMENT_SIZE		(1<<FRAGMENT_BITS)
#define MAX_FILE_SIZE_BITS	26
#define MAX_FILE_SIZE		((1<<MAX_FILE_SIZE_BITS)-1)	// maximum transferable size is	64MB

// 0 == regular, 1 == file stream
#define MAX_STREAMS			2    

#define	FRAG_NORMAL_STREAM	0
#define FRAG_FILE_STREAM	1

#define TCP_CONNECT_TIMEOUT		4.0f
#define	PORT_ANY				-1
#define PORT_TRY_MAX			10							// the number of different ports to try to find an unused one
#define PORT_TRY_MAX_FORKED     150							// the number to try when we are running as a forked child process (for the server farm)


#define TCP_MAX_ACCEPTS			8

#define LOOPBACK_SOCKETS	2

#define STREAM_CMD_NONE		0	// waiting for next blob
#define STREAM_CMD_AUTH		1	// first command, send back challengenr
#define STREAM_CMD_DATA		2	// receiving a data blob
#define STREAM_CMD_FILE		3	// receiving a file blob
#define STREAM_CMD_ACKN		4	// acknowledged a recveived blob

// NETWORKING INFO

class INetChannel;

enum ESocketIndex_t
{
	NS_INVALID = -1,

	NS_CLIENT = 0,	// client socket
	NS_SERVER,	// server socket
#ifdef _X360
	NS_X360_SYSTEMLINK,
	NS_X360_LOBBY,
	NS_X360_TEAMLINK,
#endif
	NS_HLTV,
	NS_HLTV1, // Note: NS_HLTV1 must follow NS_HLTV, NS_HLTV2 must follow NS_HLTV1, etc.
#if defined( REPLAY_ENABLED )
	NS_REPLAY,
#endif
	MAX_SOCKETS
};

extern	netadr_t	net_local_adr;
extern	double		net_time;

class INetChannelHandler;
class IConnectionlessPacketHandler;
class CMsgSteamDatagramGameServerAuthTicket;
class ISteamDatagramTransportClient;

// Start up networking
void		NET_Init( bool bDedicated );


// initialize queued packet sender. must do after fork(), not before
void NET_InitPostFork( void );


// Shut down networking
void		NET_Shutdown (void);
// Read any incoming packets, dispatch to known netchannels and call handler for connectionless packets
void		NET_ProcessSocket( int sock, IConnectionlessPacketHandler * handler );
// Set a port to listen mode
void		NET_ListenSocket( int sock, bool listen );
// Send connectionsless string over the wire
void		NET_OutOfBandPrintf(int sock, const ns_address &adr, PRINTF_FORMAT_STRING const char *format, ...) FMTFUNCTION( 3, 4 );
void		NET_OutOfBandDelayedPrintf(int sock, const ns_address &adr, uint32 unMillisecondsDelay, PRINTF_FORMAT_STRING const char *format, ...) FMTFUNCTION( 4, 5 );
// Send a raw packet, connectionless must be provided (chan can be NULL)
int			NET_SendPacket ( INetChannel *chan, int sock,  const ns_address &to, const  unsigned char *data, int length, bf_write *pVoicePayload = NULL, bool bUseCompression = false, uint32 unMillisecondsDelay = 0u );
// Called periodically to maybe send any queued packets (up to 4 per frame)
void		NET_SendQueuedPackets();
// Start set current network configuration
void		NET_SetMultiplayer(bool multiplayer);
// Set net_time
void		NET_SetTime( double realtime );
// RunFrame must be called each system frame before reading/sending on any socket
void		NET_RunFrame( double realtime );
// Check configuration state
bool		NET_IsMultiplayer( void );
bool		NET_IsDedicated( void );
#ifdef SERVER_XLSP
bool		NET_IsDedicatedForXbox( void );
#else
FORCEINLINE bool NET_IsDedicatedForXbox( void )
{
	return false;
}
#endif

// Writes a error file with bad packet content
void		NET_LogBadPacket(netpacket_t * packet);

// bForceNew (used for bots) tells it not to share INetChannels (bots will crash when disconnecting if they
// share an INetChannel).
INetChannel	*NET_CreateNetChannel( int socketnumber, const ns_address *adr, const char * name, INetChannelHandler * handler, const byte *pbEncryptionKey, bool bForceNew );
void		NET_RemoveNetChannel(INetChannel *netchan, bool bDeleteNetChan);
void		NET_PrintChannelStatus( INetChannel * chan );

void		NET_WriteStringCmd( const char * cmd, bf_write *buf );

// Address conversion
bool		NET_StringToAdr ( const char *s, netadr_t *a);
// Convert from host to network byte ordering
unsigned short NET_HostToNetShort( unsigned short us_in );
// and vice versa
unsigned short NET_NetToHostShort( unsigned short us_in );

// Find out what port is mapped to a local socket
unsigned short NET_GetUDPPort(int socket);

// add/remove extra sockets for testing
int NET_AddExtraSocket( int port );
void NET_RemoveAllExtraSockets();

const char *NET_ErrorString (int code); // translate a socket error into a friendly string

// Returns true if compression succeeded, false otherwise
bool NET_BufferToBufferCompress( char *dest, unsigned int *destLen, char *source, unsigned int sourceLen );
bool NET_BufferToBufferDecompress( char *dest, unsigned int *destLen, char *source, unsigned int sourceLen );

netadr_t NET_InitiateSteamConnection(int sock, uint64 uSteamID, PRINTF_FORMAT_STRING const char *format, ...) FMTFUNCTION( 3, 4 );
void NET_TerminateConnection(int sock, const ns_address &peer );
void NET_TerminateSteamConnection(int sock, uint64 uSteamID );

void NET_SleepUntilMessages( int nMilliseconds );

// If net_public_adr convar is set then returns that, otherwise, checks with steam if we are a dedicated server (eventually will work for the client) and returns that
// Returns false if not able to deduce address
bool NET_GetPublicAdr( netadr_t &adr );

/// Start listening for Steam datagram, if the convar tells us to
void NET_SteamDatagramServerListen();

/// Called when we receive a ticket to play on a particular gameserver
#ifndef DEDICATED

/// Make sure we are setup to talk to this gameserver
bool NET_InitSteamDatagramProxiedGameserverConnection( const ns_address &adr );
#endif

//============================================================================
//
// Encrypted network channel communication support
//

#define NET_CRYPT_KEY_LENGTH 16
bool NET_CryptVerifyServerCertificateAndAllocateSessionKey( bool bOfficial, const ns_address &from,
	const byte *pchKeyPub, int numKeyPub,
	const byte *pchKeySgn, int numKeySgn,
	byte **pbAllocatedKey, int *pnAllocatedCryptoBlockSize );
bool NET_CryptVerifyClientSessionKey( bool bOfficial,
	const byte *pchKeyPri, int numKeyPri,
	const byte *pbEncryptedKey, int numEncryptedBytes,
	byte *pbPlainKey, int numPlainKeyBytes );

enum ENetworkCertificate_t
{
	k_ENetworkCertificate_PublicKey,
	k_ENetworkCertificate_PrivateKey,
	k_ENetworkCertificate_Signature,
	k_ENetworkCertificate_Max
};
bool NET_CryptGetNetworkCertificate( ENetworkCertificate_t eType, const byte **pbData, int *pnumBytes );

//============================================================================

// Message data
typedef struct
{
	// Size of message sent/received
	int		size;
	// Time that message was sent/received
	float	time;
} flowstats_t;

struct sockaddr;

class ISteamSocketMgr
{
public:
	enum ESteamCnxType
	{
		ESCT_NEVER = 0,
		ESCT_ASBACKUP,
		ESCT_ALWAYS,

		ESCT_MAXTYPE,
	};

	enum
	{
		STEAM_CNX_PORT = 1,
	};

	virtual void Init() = 0;
	virtual void Shutdown() = 0;

	virtual ESteamCnxType GetCnxType() = 0;

	virtual void OpenSocket( int s, int nModule, int nSetPort, int nDefaultPort, const char *pName, int nProtocol, bool bTryAny ) = 0;
	virtual void CloseSocket( int s, int nModule ) = 0;

	virtual int sendto( int s, const char * buf, int len, int flags, const ns_address &to ) = 0;
	virtual int recvfrom( int s, char * buf, int len, int flags, ns_address * from ) = 0;

	virtual uint64 GetSteamIDForRemote( const ns_address &remote ) = 0;
};

extern ISteamSocketMgr *g_pSteamSocketMgr;

// Some hackery to avoid using va() in constructor since we cache off the pointer to the string in the ConVar!!!
#define NET_STRINGIZE( x ) #x
#define NET_MAKESTRING( macro, val )	macro(val)
#define NETSTRING( val ) NET_MAKESTRING( NET_STRINGIZE, val )

#endif // !NET_H
