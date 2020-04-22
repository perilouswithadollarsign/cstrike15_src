//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: "Steam" based pseudo socket support (PC only)
//
// $NoKeywords: $
//
//=============================================================================//
#include "net_ws_headers.h"
#include "tier0/vprof.h"
#include "sv_ipratelimit.h"

#if IsPlatformWindows()
#else
#include <poll.h>
#endif

#if !defined(_X360) && !defined(NO_STEAM) && !defined(DEDICATED)
#define USE_STEAM_SOCKETS
#endif

ConVar net_threaded_socket_recovery_time( "net_threaded_socket_recovery_time", "60", FCVAR_RELEASE, "Number of seconds over which the threaded socket pump algorithm will fully recover client ratelimit." );
ConVar net_threaded_socket_recovery_rate( "net_threaded_socket_recovery_rate", "6400", FCVAR_RELEASE, "Number of packets per second that threaded socket pump algorithm allows from client." );
ConVar net_threaded_socket_burst_cap( "net_threaded_socket_burst_cap", 
#ifdef DEDICATED
	"256",
#else
	"1024",
#endif
	FCVAR_RELEASE, "Max number of packets per burst beyond which threaded socket pump algorithm will start dropping packets." );

struct net_threaded_buffer_t
{
	int len;
	byte buf[ NET_MAX_MESSAGE ];

	inline byte *Base() { return buf + len; }
	inline int Capacity() { return sizeof( buf ) - len; }
	inline byte *MoveAppend( net_threaded_buffer_t *pOther )
	{
		if ( pOther->len > Capacity() )
			return NULL;

		byte *pBase = Base();
		Q_memcpy( pBase, pOther->buf, pOther->len );
		len += pOther->len;
		pOther->len = 0;
		return pBase;
	}
};
CTSPool<net_threaded_buffer_t> g_NetThreadedBuffers;

netadr_t g_NetAdrRatelimited;
int32 g_numRatelimitedPackets = -100;
class CThreadedSocketQueue
{
public:
	CThreadedSocketQueue() : m_mapSocketThreads( DefLessFunc( int ) ) {}
	~CThreadedSocketQueue()
	{
		m_mapSocketThreads.PurgeAndDeleteElements();
	}

	static bool ShouldUseSocketsThreaded()
	{
		static bool s_bThreaded = !CommandLine()->FindParm( "-nothreadedsockets" );
		return s_bThreaded;
	}
	
	int recvfrom( int s, char * buf, int len, struct sockaddr * from )
	{
		CSocketThread *pThread = GetSocketThread( s );
		return pThread ? pThread->recvfrom( buf, len, from ) : 0;
	}

	void EnableThreadedRecv( int s, int nsSock, bool bEnable )
	{
		CSocketThread *pThread = GetSocketThread( s, nsSock, bEnable );
		( void ) ( pThread );
	}

	void CloseSocket( int s )
	{
		CUtlMap< int, CSocketThread * >::IndexType_t idx = m_mapSocketThreads.Find( s );
		if ( idx != m_mapSocketThreads.InvalidIndex() )
		{
			CSocketThread *pThread = m_mapSocketThreads.Element( idx );
			delete pThread;
			m_mapSocketThreads.RemoveAt( idx );
		}
	}

private:
	class CSocketThread
	{
	public:
		explicit CSocketThread( int s, int nsSock ) : m_s( s ), m_nsSock( nsSock ), m_hThread( NULL ), m_pDataQueueBufferCollect( NULL )
		{
#if IsPlatformWindows()
			m_wsaEvents[0] = ::WSACreateEvent();
			if ( !m_wsaEvents[0] )
				Error( "WSACreateEvent failed\n" );
			m_wsaEvents[1] = ::WSACreateEvent();
			if ( !m_wsaEvents[1] )
				Error( "WSACreateEvent failed\n" );
			int ret = ::WSAEventSelect( m_s, m_wsaEvents[ 1 ], FD_READ );
			if ( ret )
				Error( "WSAEventSelect failed\n" );
#else
			Q_memset( m_sockSignalPipe, -1, sizeof( m_sockSignalPipe ) );
			int ret = socketpair( PF_LOCAL, SOCK_STREAM, 0, m_sockSignalPipe ); // 0=main; 1=pump
			if ( ret )
				Error( "socketpair failed!\n" );
#endif

			// Create the thread and start socket processing
			m_hThread = CreateSimpleThread( CallbackThreadProc, this );
			if ( !m_hThread )
				Error( "socket thread failed!\n" );
		}
		~CSocketThread()
		{
			// Kill the thread!
#if IsPlatformWindows()
			::WSASetEvent( m_wsaEvents[0] );
#else
			write( m_sockSignalPipe[0], "", 1 ); // write one zero byte to wake the thread
#endif
			// wait for it to die
			ThreadJoin( m_hThread );
			ReleaseThreadHandle( m_hThread );
			m_hThread = NULL;

			// Shutdown resources
#if IsPlatformWindows()
			::WSACloseEvent( m_wsaEvents[0] );
			::WSACloseEvent( m_wsaEvents[1] );
#else
			close( m_sockSignalPipe[0] );
			close( m_sockSignalPipe[1] );
#endif
			
			// Purge all packet data blocks
			m_tslstDataQueue.Purge();
			
			// Return all the buffers back into the pool
			do
			{
				if ( m_pDataQueueBufferCollect )
				{
					g_NetThreadedBuffers.PutObject( m_pDataQueueBufferCollect );
					m_pDataQueueBufferCollect = NULL;
				}
			} while ( m_tslstBuffers.PopItem( &m_pDataQueueBufferCollect ) );
		}

		int recvfrom( char * buf, int len, struct sockaddr * from )
		{
			//
			// Called on application main thread
			//
			ReceivedData_t data;
			if ( !m_tslstDataQueue.PopItem( &data ) )
				return 0;

			Q_memcpy( buf, data.buf, MIN( len, data.len ) );
			Q_memcpy( from, &data.from, sizeof( data.from ) );

			//
			// We are about to return this data, free up allocated buffers up to this
			//
			if ( !m_pDataQueueBufferCollect )
			{
				m_tslstBuffers.PopItem( &m_pDataQueueBufferCollect );
			}

			if ( ( data.buf >= m_pDataQueueBufferCollect->buf ) && ( data.buf < m_pDataQueueBufferCollect->buf + sizeof( m_pDataQueueBufferCollect->buf ) ) )
				;// The returned data was still referencing the head of scratch buffers
			else
			{	// The returned data is ahead in the list, previous buffer can go back in the pool
				g_NetThreadedBuffers.PutObject( m_pDataQueueBufferCollect );
				m_pDataQueueBufferCollect = NULL;
			}

			return data.len;
		}

	private:
		struct CPerNetChanRatelimit_t
		{
			double m_dblNetTimeMark;
			int32 m_numPackets;
			int32 m_numRatelimited;
		};
		static uintp CallbackThreadProc( void *pvParam ) { reinterpret_cast<CSocketThread*>(pvParam)->ThreadProc(); return 0; }
		void ThreadProc()
		{
			// Where are we getting new data?
			net_threaded_buffer_t *pThreadBufferSyscall = NULL;
			net_threaded_buffer_t *pThreadBufferCollect = NULL;
			
			struct sockaddr	from;
			int	fromlen = sizeof( from );
			netadr_t adrt;
			adrt.SetType( NA_IP );
			adrt.Clear();

			extern volatile int g_NetChannelsRefreshCounter;
			int pumpNetChannelsRefreshCounter = -1;
			CUtlVector< struct sockaddr > arrNetChans;
			typedef CUtlMap< uint64, CPerNetChanRatelimit_t, int, CDefLess< uint64 > > MapPerClientRatelimit_t;
			MapPerClientRatelimit_t mapPerClientRatelimit;

#if IsPlatformWindows()
			const DWORD cWsaEvents = Q_ARRAYSIZE( m_wsaEvents );
#else
			const int numSyscallPollFDs = 2;
			struct pollfd syscallPollFDs[ numSyscallPollFDs ];
			Q_memset( syscallPollFDs, 0, sizeof( syscallPollFDs ) );
			syscallPollFDs[ 0 ].fd = m_s;
			syscallPollFDs[ 0 ].events = POLLIN;
			syscallPollFDs[ 1 ].fd = m_sockSignalPipe[1];
			syscallPollFDs[ 1 ].events = POLLIN;
#endif

			volatile double &vdblNetTimeForThread = net_time;
			for ( ;; )
			{
				if ( !pThreadBufferSyscall )
				{
					// Need a new buffer
					net_threaded_buffer_t *pbuf = g_NetThreadedBuffers.GetObject();
					pbuf->len = 0;
					m_tslstBuffers.PushItem( pbuf );
					pThreadBufferSyscall = pbuf;
				}

				// Recv socket data
				int ret = ::recvfrom( m_s, ( char* ) pThreadBufferSyscall->buf, sizeof( pThreadBufferSyscall->buf ), 0, &from, (socklen_t*)&fromlen );
				if ( ret <= 0 )
				{
					// Efficiently sleep while we wait for next packet
#if IsPlatformWindows()
					DWORD dwWsaWaitResult = ::WSAWaitForMultipleEvents( cWsaEvents, m_wsaEvents, FALSE, WSA_INFINITE, FALSE );
					if ( dwWsaWaitResult == WSA_WAIT_EVENT_0 )
						return; // Finish the socket pump thread due to event signal from destructor
					WSANETWORKEVENTS wsaNetworkEvents;
					::WSAEnumNetworkEvents( m_s, m_wsaEvents[1], &wsaNetworkEvents ); // Reset socket read event
#else
					ret = ::poll( syscallPollFDs, numSyscallPollFDs, -1 ); // Sleep indefinitely until data available
					if ( syscallPollFDs[ 1 ].revents )
						return;	// Finish the socket reading thread if socketpair pump end of the pipe has incoming signal
#endif
					continue;
				}
				if ( ret > 0 )
				{
					// Is this a connectionless packet?
					if ( 0xFFFFFFFF == * ( uint32 * ) pThreadBufferSyscall->buf )
					{
						adrt.SetFromSockadr( &from );
						if ( !CheckConnectionLessRateLimits( adrt ) )
							ret = 0;
					}
					else
					{
						// Check if we need to refresh our netchans for this socket type
						if ( pumpNetChannelsRefreshCounter != g_NetChannelsRefreshCounter )
						{
							pumpNetChannelsRefreshCounter = g_NetChannelsRefreshCounter;
							arrNetChans.RemoveAll();
							extern void NET_FindAllNetChannelAddresses( int socket, CUtlVector< struct sockaddr > &arrNetChans );
							NET_FindAllNetChannelAddresses( m_nsSock, arrNetChans );

							// Since we just recomputed net channels, use this opportunity to expire obsolete ratelimits for old clients
							FOR_EACH_MAP_FAST( mapPerClientRatelimit, idxPerClientRateLimit )
							{
								CPerNetChanRatelimit_t &pncrt = mapPerClientRatelimit.Element( idxPerClientRateLimit );
								double dblTimeSinceTimeMark = ( vdblNetTimeForThread - pncrt.m_dblNetTimeMark );
								if ( dblTimeSinceTimeMark > net_threaded_socket_recovery_time.GetFloat() )
									mapPerClientRatelimit.RemoveAt( idxPerClientRateLimit );
							}
						}

						// This is a connection-oriented packet, must have a netchan
						bool bNetChanAvailable = false;
						for ( int k = 0; k < arrNetChans.Count(); ++ k )
						{
							struct sockaddr &sockAddressNetChan = arrNetChans[k];
							if (
								!Q_memcmp( &((struct sockaddr_in*)&sockAddressNetChan)->sin_addr.s_addr, &((struct sockaddr_in*)&from)->sin_addr.s_addr, sizeof( ((struct sockaddr_in*)&from)->sin_addr.s_addr ) )
								&& ( ((struct sockaddr_in*)&sockAddressNetChan)->sin_port == ((struct sockaddr_in*)&from)->sin_port )
								)
							{
								bNetChanAvailable = true;

								//
								// Track ratelimit on this netchan
								//
								uint64 uiRatelimitKey = ( uint64( ((struct sockaddr_in*)&from)->sin_addr.s_addr ) << 32 ) | uint32( ((struct sockaddr_in*)&from)->sin_port );
								MapPerClientRatelimit_t::IndexType_t idxPerClientRateLimit = mapPerClientRatelimit.Find( uiRatelimitKey );
								if ( idxPerClientRateLimit == mapPerClientRatelimit.InvalidIndex() )
								{
									CPerNetChanRatelimit_t pncrt;
									Q_memset( &pncrt, 0, sizeof( pncrt ) );
									pncrt.m_dblNetTimeMark = vdblNetTimeForThread;
									pncrt.m_numPackets = 1;
									mapPerClientRatelimit.Insert( uiRatelimitKey, pncrt );
								}
								else
								{
									CPerNetChanRatelimit_t &pncrt = mapPerClientRatelimit.Element( idxPerClientRateLimit );
									double dblTimeSinceTimeMark = ( vdblNetTimeForThread - pncrt.m_dblNetTimeMark );
									if ( ( dblTimeSinceTimeMark > net_threaded_socket_recovery_time.GetFloat() ) ||
										( dblTimeSinceTimeMark < 0 ) )
									{
										pncrt.m_numPackets = 0;
									}
									else if ( dblTimeSinceTimeMark > 0 )
									{
										int32 numPacketsToRecover = dblTimeSinceTimeMark * net_threaded_socket_recovery_rate.GetFloat();
										pncrt.m_numPackets = MAX( pncrt.m_numPackets - numPacketsToRecover, 0 );
									}
									
									if ( pncrt.m_numPackets > net_threaded_socket_burst_cap.GetInt() )
									{
										bNetChanAvailable = false;
										++ pncrt.m_numRatelimited;
										if ( pncrt.m_numRatelimited > g_numRatelimitedPackets + 5 )
										{
											g_NetAdrRatelimited.SetFromSockadr( &from ); // remember last ratelimited address for logging purposes
											g_numRatelimitedPackets = pncrt.m_numRatelimited;
										}
									}
									else
									{
										pncrt.m_dblNetTimeMark = vdblNetTimeForThread;
										++ pncrt.m_numPackets;
										pncrt.m_numRatelimited = 0;
									}
								}

								break;
							}
						}
						if ( !bNetChanAvailable )
							ret = 0;
					}
				}
				if ( ret > 0 )
				{
					ReceivedData_t recvData;
					recvData.buf = NULL;
					recvData.len = ret;
					Q_memcpy( &recvData.from, &from, sizeof( recvData.from ) );

					// Check if we still have more room in pending recv buffer
					pThreadBufferSyscall->len = ret;
					if ( byte *pbMoveAppend = pThreadBufferCollect ? pThreadBufferCollect->MoveAppend( pThreadBufferSyscall ) : NULL )
					{
						recvData.buf = pbMoveAppend;
					}
					else
					{
						pThreadBufferCollect = pThreadBufferSyscall;
						pThreadBufferSyscall = NULL;

						recvData.buf = pThreadBufferCollect->buf;
					}

					m_tslstDataQueue.PushItem( recvData );
				}
			}
		}

		struct ReceivedData_t
		{
			byte *buf;
			int len;
			struct sockaddr	from;
		};

		
	//
	// Thread object data members
	//
	private:
		int const m_s;									// [const] accessed by pump thread in ::recvfrom
		int const m_nsSock;								// [const] accessed by pump thread doing netchans lookup
		ThreadHandle_t m_hThread;						// Thread handle, accessed by main thread
		CTSQueue< ReceivedData_t > m_tslstDataQueue;	// FIFO - actual data packets pumped from socket, thread-safe access on both threads
		CTSQueue< net_threaded_buffer_t * > m_tslstBuffers;	// FIFO - buffers storing data pumped from socket, multiple packets can be stored in one memory chunk, thread-safe access on both threads
		net_threaded_buffer_t *m_pDataQueueBufferCollect; // Main thread tracking when collect buffer can be returned to global pool
#if IsPlatformWindows()
		WSAEVENT m_wsaEvents[2];
#else
		int m_sockSignalPipe[2];
#endif
	};
	CSocketThread * GetSocketThread( int s, int nsSock = -1, bool bRequired = false )
	{
		CUtlMap< int, CSocketThread * >::IndexType_t idx = m_mapSocketThreads.Find( s );
		if ( idx != m_mapSocketThreads.InvalidIndex() )
			return m_mapSocketThreads.Element( idx );
		if ( bRequired )
		{
			CSocketThread *pNew = new CSocketThread( s, nsSock );
			m_mapSocketThreads.Insert( s, pNew );
			return pNew;
		}
		return NULL;
	}
	CUtlMap< int, CSocketThread * > m_mapSocketThreads;
};
CThreadedSocketQueue g_ThreadedSocketQueue;

void On_NET_ProcessSocket_Start( int hUDP, int sock )
{
	if ( g_numRatelimitedPackets > 0 )
	{	// Spew about ratelimit on the main thread
		ConMsg( "Net channel ratelimit exceeded for %s: %d packets rejected.\n", g_NetAdrRatelimited.ToString(), g_numRatelimitedPackets );
		g_NetAdrRatelimited.Clear();
		g_numRatelimitedPackets = -100;
	}
}
void On_NET_ProcessSocket_End( int hUDP, int sock )
{
	if ( CThreadedSocketQueue::ShouldUseSocketsThreaded() )
		g_ThreadedSocketQueue.EnableThreadedRecv( hUDP, sock, true );
}


#if defined( USE_STEAM_SOCKETS )

#include "cl_steamauth.h"
#include "tier1/tokenset.h"
#include "utlmap.h"

// matchmaking
#include "matchmaking/imatchframework.h"
#include "matchmaking/iplayer.h"
#include "matchmaking/imatchtitle.h"
#include "matchmaking/mm_helpers.h"

// for INetSupport defines
#include "engine/inetsupport.h"

#include "server.h"


ConVar net_steamcnx_debug( "net_steamcnx_debug", "0", 0, "Show debug spew for steam based connections, 2 shows all network traffic for steam sockets." );
static ConVar net_steamcnx_enabled( "net_steamcnx_enabled", "1", FCVAR_RELEASE, "Use steam connections on listen server as a fallback, 2 forces use of steam connections instead of raw UDP." );
static ConVar net_steamcnx_allowrelay( "net_steamcnx_allowrelay", "1", FCVAR_RELEASE | FCVAR_ARCHIVE, "Allow steam connections to attempt to use relay servers as fallback (best if specified on command line:  +net_steamcnx_allowrelay 1)" );

#define STEAM_CNX_COLOR Color( 255, 255, 100, 255 )

extern ConVar cl_timeout;

static const tokenset_t< ESocketIndex_t > s_SocketIndexMap[] =
{						 
	{ "NS_CLIENT",		NS_CLIENT		},                          
	{ "NS_SERVER",		NS_SERVER		},                          
#ifdef _X360
	{ "NS_X360_SYSTEMLINK",	NS_X360_SYSTEMLINK	},
	{ "NS_X360_LOBBY",		NS_X360_LOBBY		},
	{ "NS_X360_TEAMLINK",	NS_X360_TEAMLINK	},
#endif
	{ "NS_HLTV",		NS_HLTV			},
	{ "NS_HLTV1",		NS_HLTV1		},
	{ NULL, ( ESocketIndex_t )-1 }
};

static const tokenset_t< EP2PSessionError > s_EP2PSessionErrorIndexMap[] =
{
	{ "None", k_EP2PSessionErrorNone },
	{ "Not running app", k_EP2PSessionErrorNotRunningApp },					// target is not running the same game
	{ "No rights to app", k_EP2PSessionErrorNoRightsToApp },				// local user doesn't own the app that is running
	{ "User not logged in", k_EP2PSessionErrorDestinationNotLoggedIn },		// target user isn't connected to Steam
	{ "Timeout", k_EP2PSessionErrorTimeout }
};



// Why are there two Steam P2P channels instead of one client/server channel?
//
// We use a client receive channel and a server receive channel to simulate sockets. When a user is running a listen server, ::recvfrom will be called
// simultaneously by both the server & client objects. If we were only using one channel, we would need to parse each packet received on that channel,
// determine if really intended for the callers socket, and potentially store if for another socket.

// code in this file only handles two types of sockets
static inline bool IsSteamSocketType( ESocketIndex_t eSocketType )
{
	return (eSocketType == NS_CLIENT || eSocketType == NS_SERVER);	
}

// assumes you have already called IsSteamSocketType
static inline int GetChannelForSocketType( ESocketIndex_t eSocketType )
{
	return (eSocketType == NS_CLIENT) ? INetSupport::SP2PC_RECV_CLIENT : INetSupport::SP2PC_RECV_SERVER;
}

// each virtual socket we have open to another user
class CSteamSocket
{
public:
	explicit CSteamSocket( const CSteamID &steamIdRemote, ESocketIndex_t eSocketType, const netadr_t &addr );

	const CSteamID &GetSteamID() const { return m_steamID; }
	ESocketIndex_t GetSocketType() const { return m_eSocketType; }
	const netadr_t &GetNetAddress() const { return m_addr; }

	inline ESocketIndex_t GetRemoteSocketType() const
	{
		return ( m_eSocketType == NS_CLIENT ) ? NS_SERVER : NS_CLIENT;
	}

	inline int GetRemoteChannel() const
	{
		return GetChannelForSocketType( GetRemoteSocketType() );
	}

	inline int GetLocalChannel() const
	{
		return GetChannelForSocketType( m_eSocketType );
	}

private:
	CSteamID		m_steamID;			// SteamID of other user
	ESocketIndex_t	m_eSocketType;		// The socket type this connection was created on
	netadr_t		m_addr;				// The fake net address we have returned for this user
};

CSteamSocket::CSteamSocket( const CSteamID &steamIdRemote, ESocketIndex_t eSocketType, const netadr_t &addr ) :
	m_steamID( steamIdRemote ), 
	m_eSocketType( eSocketType ),
	m_addr( addr )
{
}


class CSteamSocketMgr : public ISteamSocketMgr
{
public:
	CSteamSocketMgr();
	~CSteamSocketMgr();

	virtual void Init() OVERRIDE;
	virtual void Shutdown() OVERRIDE;

	virtual ISteamSocketMgr::ESteamCnxType GetCnxType();

	virtual void OpenSocket( int s, int nModule, int nSetPort, int nDefaultPort, const char *pName, int nProtocol, bool bTryAny ) OVERRIDE;
	virtual void CloseSocket( int s, int nModule ) OVERRIDE;

	virtual int sendto( int s, const char * buf, int len, int flags, const ns_address &to ) OVERRIDE;
	virtual int recvfrom( int s, char * buf, int len, int flags, ns_address *from ) OVERRIDE;

	virtual uint64 GetSteamIDForRemote( const ns_address &remote ) OVERRIDE;

	// client connection state
	STEAM_CALLBACK( CSteamSocketMgr, OnP2PSessionRequest, P2PSessionRequest_t, m_callbackP2PSessionRequest );
	STEAM_CALLBACK( CSteamSocketMgr, OnP2PSessionConnectFail, P2PSessionConnectFail_t, m_callbackP2PSessionConnectFail );

	CSteamSocket *InitiateConnection( ESocketIndex_t eSocketType, const CSteamID &steamID, const byte *data, size_t len );
	void DestroyConnection( ESocketIndex_t eSocketType, const CSteamID &steamID );

	void PrintStatus();

private:
	CSteamSocket *CreateConnection( ESocketIndex_t eSocketType, const CSteamID &steamID );
	void DestroyConnection( CSteamSocket *pSocket );	
	
	bool GetTypeForSocket( int s, ESocketIndex_t *peType );
	netadr_t GenerateRemoteAddress();

	CSteamSocket *FindSocketForAddress( const ns_address &adr );
	CSteamSocket *FindSocketForUser( ESocketIndex_t eSocketType, const CSteamID &steamID );

	bool IsValid() const
	{
		return m_bInitialized && Steam3Client().SteamNetworking();
	}

	// For Remote clients
	CUtlVector< CSteamSocket * > m_vecRemoteSockets;
	CUtlMap< netadr_t, CSteamSocket * > m_mapAdrToSteamSocket;
	CUtlMap< int, ESocketIndex_t > m_mapSocketToESocketType;

	int			m_nNextRemoteAddress;
	bool		m_bInitialized;
};

CSteamSocketMgr::CSteamSocketMgr() : 
	m_bInitialized( false ),
	m_nNextRemoteAddress( 1 ),
	m_mapAdrToSteamSocket( 0, 0, DefLessFunc( netadr_t ) ),
	m_mapSocketToESocketType( 0, 0, DefLessFunc( int ) ),
	m_callbackP2PSessionRequest( this, &CSteamSocketMgr::OnP2PSessionRequest ),
	m_callbackP2PSessionConnectFail( this, &CSteamSocketMgr::OnP2PSessionConnectFail )
{
}

CSteamSocketMgr::~CSteamSocketMgr()
{
}

void CSteamSocketMgr::Init()
{
	m_bInitialized = true;
	if ( Steam3Client().SteamNetworking() )
		Steam3Client().SteamNetworking()->AllowP2PPacketRelay( net_steamcnx_allowrelay.GetBool() );
}

void CSteamSocketMgr::Shutdown()
{
	if ( !IsValid() )
		return;

	// Destroy remote sockets
	FOR_EACH_VEC_BACK( m_vecRemoteSockets, i )
	{
		CSteamSocket *pSocket = m_vecRemoteSockets[i];
		// this will delete pSocket
		DestroyConnection( pSocket );
	}
	m_vecRemoteSockets.RemoveAll();

	Assert( m_mapAdrToSteamSocket.Count() == 0 );
	m_mapAdrToSteamSocket.RemoveAll();
	m_mapSocketToESocketType.RemoveAll();

	m_bInitialized = false;
}

netadr_t CSteamSocketMgr::GenerateRemoteAddress()
{
	netadr_t ret( m_nNextRemoteAddress++, STEAM_CNX_PORT );
	return ret;
}

void CSteamSocketMgr::OpenSocket( int s, int nModule, int nSetPort, int nDefaultPort, const char *pName, int nProtocol, bool bTryAny )
{
	if ( !IsValid() )
		return;

	ESocketIndex_t eSocketType = ESocketIndex_t( nModule );

	if ( !IsSteamSocketType( eSocketType ) )
		return;	

	// make sure we dont have a socket for this type
	FOR_EACH_MAP_FAST( m_mapSocketToESocketType, i )
	{
		if ( m_mapSocketToESocketType[i] == eSocketType )
		{
			AssertMsg1( false, "Already have a socket for this type: %s", s_SocketIndexMap->GetNameByToken( eSocketType ) );
			return;
		}
	}

	// save socket
	m_mapSocketToESocketType.InsertOrReplace( s, eSocketType );

	if ( net_steamcnx_debug.GetBool() )
	{
		ConColorMsg( STEAM_CNX_COLOR, "Opened Steam Socket %s ( socket %d )\n", s_SocketIndexMap->GetNameByToken( eSocketType ), s );
	}
}

void CSteamSocketMgr::CloseSocket( int s, int nModule )
{
	if ( g_ThreadedSocketQueue.ShouldUseSocketsThreaded() )
		g_ThreadedSocketQueue.CloseSocket( s );

	if ( !IsValid() )
		return;

	ESocketIndex_t eSocketType = ESocketIndex_t( nModule );
	if ( !IsSteamSocketType( eSocketType ) )
		return;

	if ( net_steamcnx_debug.GetBool() )
	{
		ConColorMsg( STEAM_CNX_COLOR, "Closed Steam Socket %s\n", s_SocketIndexMap->GetNameByToken( eSocketType ) );
	}

	FOR_EACH_VEC_BACK( m_vecRemoteSockets, i )
	{
		CSteamSocket *pSocket = m_vecRemoteSockets[i];
		if ( pSocket->GetSocketType() == eSocketType )
			DestroyConnection( pSocket );
	}
	
	FOR_EACH_MAP( m_mapSocketToESocketType, i )
	{
		if ( m_mapSocketToESocketType[ i ] == eSocketType )
		{
			m_mapSocketToESocketType.RemoveAt( i );
			break;
		}
	}
}

CSteamSocket *CSteamSocketMgr::InitiateConnection( ESocketIndex_t eSocketTypeFrom, const CSteamID &steamID, const byte *data, size_t len )
{
	CSteamSocket *pSocket = CreateConnection( eSocketTypeFrom, steamID );
	if ( !pSocket )
		return NULL;

	// don't have to wait for a connection to be established.. just send the packet
	if ( !Steam3Client().SteamNetworking()->SendP2PPacket( pSocket->GetSteamID(), data, len, k_EP2PSendReliable, pSocket->GetRemoteChannel() ) )
	{
		DestroyConnection( eSocketTypeFrom, steamID );
		return NULL;
	}

	return pSocket;
}

CSteamSocket *CSteamSocketMgr::CreateConnection( ESocketIndex_t eSocketType, const CSteamID &steamID )
{
	if ( !IsValid() )
		return NULL;

	// if we already have a socket for this user, return that
	CSteamSocket *pSocket = FindSocketForUser( eSocketType, steamID );
	if ( pSocket )
		return pSocket;

	netadr_t adrRemote = GenerateRemoteAddress();
	ConColorMsg( STEAM_CNX_COLOR, "Generated %s for %llx\n", adrRemote.ToString(), steamID.ConvertToUint64() );

	// create
	pSocket = new CSteamSocket( steamID, eSocketType, adrRemote );
	m_mapAdrToSteamSocket.Insert( adrRemote, pSocket );
	m_vecRemoteSockets.AddToTail( pSocket );

	if ( net_steamcnx_debug.GetBool() )
	{
		ConColorMsg( STEAM_CNX_COLOR, "Created %s connection to %llx\n", s_SocketIndexMap->GetNameByToken( eSocketType ), steamID.ConvertToUint64() );
	}

	return pSocket;
}

void CSteamSocketMgr::DestroyConnection( CSteamSocket *pSocket )
{
	if ( !IsValid() || !pSocket )
		return;	

	// remove from address map & vector
	m_mapAdrToSteamSocket.Remove( pSocket->GetNetAddress() );
	m_vecRemoteSockets.FindAndFastRemove( pSocket );

	// we can close both channels with this user, as we can only talk to his server or client. If their client is talking to our server,
	// our client shouldn't be talking to their server
	Steam3Client().SteamNetworking()->CloseP2PChannelWithUser( pSocket->GetSteamID(), pSocket->GetLocalChannel() );
	Steam3Client().SteamNetworking()->CloseP2PChannelWithUser( pSocket->GetSteamID(), pSocket->GetRemoteChannel() );

	// log
	if ( net_steamcnx_debug.GetBool() )
	{
		ConColorMsg( STEAM_CNX_COLOR, "Destroyed %s connection to %llx\n", s_SocketIndexMap->GetNameByToken( pSocket->GetSocketType() ), pSocket->GetSteamID().ConvertToUint64() );
	}
	
	// done with socket
	delete pSocket;
}

void CSteamSocketMgr::DestroyConnection( ESocketIndex_t eSocketType, const CSteamID &steamID )
{
	if ( !IsValid() )
		return;
	
	CSteamSocket *pSocket = FindSocketForUser( eSocketType, steamID );
	DestroyConnection( pSocket );	
}

CSteamSocket *CSteamSocketMgr::FindSocketForAddress( const ns_address &adr )
{

	// !FIXME! Eventually we probably should actually use SteamID P2P address type for this,
	// and not assign virtual addresses.

	if ( !adr.IsType<netadr_t>() )
	{
		Assert( false );
		return nullptr;
	}

	int idx = m_mapAdrToSteamSocket.Find( adr.AsType<netadr_t>() );
	if ( idx == m_mapAdrToSteamSocket.InvalidIndex() )
		return NULL;
	return m_mapAdrToSteamSocket[ idx ];
}

CSteamSocket *CSteamSocketMgr::FindSocketForUser( ESocketIndex_t eSocketType, const CSteamID &steamID )
{
	FOR_EACH_VEC( m_vecRemoteSockets, i )
	{
		CSteamSocket *pSocket = m_vecRemoteSockets[i];
		if ( pSocket->GetSteamID() == steamID && pSocket->GetSocketType() == eSocketType )
			return pSocket;
	}

	return NULL;
}

void CSteamSocketMgr::OnP2PSessionRequest( P2PSessionRequest_t *pParam )
{

#ifndef DEDICATED

	// on listen servers, don't accept connections from others if they aren't in our matchmaking session
	if ( !g_pMatchFramework || !g_pMatchFramework->GetMatchSession() )
		return;

	if ( GetBaseLocalClient().IsConnected() && !sv.IsActive() )
		return;
	
	if ( !SessionMembersFindPlayer( g_pMatchFramework->GetMatchSession()->GetSessionSettings(), pParam->m_steamIDRemote.ConvertToUint64() ) )
		return;

#endif

	// accept all connections
	Steam3Client().SteamNetworking()->AcceptP2PSessionWithUser( pParam->m_steamIDRemote );

	if ( net_steamcnx_debug.GetBool() )
	{
		ConColorMsg( STEAM_CNX_COLOR, "Accepted P2P connection with %llx\n", pParam->m_steamIDRemote.ConvertToUint64() );
	}
}

void CSteamSocketMgr::OnP2PSessionConnectFail( P2PSessionConnectFail_t *pParam )
{
	// log disconnect
	if ( net_steamcnx_debug.GetBool() )
	{
		const char *pchP2PError = s_EP2PSessionErrorIndexMap->GetNameByToken( (EP2PSessionError)pParam->m_eP2PSessionError );
		ConColorMsg( STEAM_CNX_COLOR, "Received connection fail for user %llx %s\n", pParam->m_steamIDRemote.ConvertToUint64(), pchP2PError );
	}

	// close all connections to this user
	FOR_EACH_VEC_BACK( m_vecRemoteSockets, i )
	{
		CSteamSocket *pSocket = m_vecRemoteSockets[i];
		if ( pSocket->GetSteamID() != pParam->m_steamIDRemote )
			continue;

		DestroyConnection( pSocket );
	}	
}

int CSteamSocketMgr::sendto( int s, const char * buf, int len, int flags, const ns_address &to )
{
	if ( !to.IsType<netadr_t>() )
	{
		Warning( "WARNING: sendto: don't know how to send to non-IP address '%s'\n", ns_address_render( to ).String() );
		Assert( false );
		return -1;	// return socket_error
	}

	if ( IsValid() )
	{
		CSteamSocket *pSteamSocket = FindSocketForAddress( to );
		if ( pSteamSocket )
		{
			Steam3Client().SteamNetworking()->SendP2PPacket( pSteamSocket->GetSteamID(), buf, len, k_EP2PSendUnreliable, pSteamSocket->GetRemoteChannel() );
			if ( net_steamcnx_debug.GetInt() >= 3 )
			{
				P2PSessionState_t p2pSessionState;
				Q_memset( &p2pSessionState, 0, sizeof( p2pSessionState ) );
				bool bSuccess = Steam3Client().SteamNetworking()->GetP2PSessionState( pSteamSocket->GetSteamID(), &p2pSessionState );

				ESocketIndex_t eType = NS_INVALID;
				GetTypeForSocket( s, &eType );

				ConColorMsg( STEAM_CNX_COLOR, "  Send to %llx %u bytes on %s (status %s - %s)\n", 
					pSteamSocket->GetSteamID().ConvertToUint64(), 
					len,
					s_SocketIndexMap->GetNameByToken( eType ),
					bSuccess ? "true" : "false",
					p2pSessionState.m_bConnectionActive ? "connected" :"not connected" );
			}

			return len;
		}

		else if ( to.AsType<netadr_t>().GetPort() == STEAM_CNX_PORT )
		{
			if ( net_steamcnx_debug.GetInt() >= 1 )
			{
				ConColorMsg( STEAM_CNX_COLOR, "  Attempted to send %u bytes on unknown steam socket address %s\n", len, ns_address_render( to ).String() );
			}

			return len;
		}
	}

	if ( OnlyUseSteamSockets() )
	{
		Warning( "WARNING: sendto: CSteamSocketMgr isn't initialized and we aren't falling back to our own sockets\n");
		return -1;	// return socket_error
	}

	// Plain old socket send
	sockaddr sadr;
	to.AsType<netadr_t>().ToSockadr( &sadr );
	return ::sendto( s, buf, len, flags, &sadr, sizeof(sadr) );
}

bool CSteamSocketMgr::GetTypeForSocket( int s, ESocketIndex_t *peType )
{
	int i = m_mapSocketToESocketType.Find( s );
	if ( i == m_mapSocketToESocketType.InvalidIndex() )
		return false;

	*peType = m_mapSocketToESocketType[i];
	return true;
}

int CSteamSocketMgr::recvfrom( int s, char * buf, int len, int flags, ns_address *from )
{
	// Check non-steam socket first
	if ( !OnlyUseSteamSockets() )
	{
		sockaddr sadrfrom;
		socklen_t fromlen = sizeof(sadrfrom);
		int iret = ( g_ThreadedSocketQueue.ShouldUseSocketsThreaded() )
			? g_ThreadedSocketQueue.recvfrom( s, buf, len, &sadrfrom )
			: ::recvfrom( s, buf, len, flags, &sadrfrom, &fromlen );	

		if ( iret > 0 )
		{
			from->SetFromSockadr( &sadrfrom );
			return iret;
		}
	}
	

	if ( !IsValid() )
		return 0;

	// need to get data by socket type
	ESocketIndex_t eSocketType = NS_INVALID;
	if ( !GetTypeForSocket( s, &eSocketType ) )
		return 0;

	//
	// IPC-Steam performance optimization: don't do any IPC to P2P sockets API calls
	// if the session settings indicate that there can be no P2P communication
	//
	switch ( eSocketType )
	{
	case NS_CLIENT:
		// We can only be receiving P2P communication if we are a client of another
		// listen server, make sure that the session has the right data
		if ( sv.IsDedicated() )
			return 0;
		if ( sv.IsActive() )
			return 0;
		// Otherwise check how many players are connected to our session, if nobody is connected
		// then do no P2P communication with nobody
		if ( !g_pMatchFramework || !g_pMatchFramework->GetMatchSession() ||
			( g_pMatchFramework->GetMatchSession()->GetSessionSettings()->GetInt( "members/numMachines", 0 ) < 2 ) ||
			Q_stricmp( g_pMatchFramework->GetMatchSession()->GetSessionSettings()->GetString( "server/server" ), "listen" ) )
			return 0;
		break;
	case NS_SERVER:
		// Dedicated servers don't do P2P communication
		if ( sv.IsDedicated() )
			return 0;
		// If we are not running a listen server then there shouldn't be any P2P communication
		if ( !sv.IsActive() )
			return 0;
		// Otherwise check how many players are connected to our session, if nobody is connected
		// then do no P2P communication with nobody
		if ( !g_pMatchFramework || !g_pMatchFramework->GetMatchSession() ||
			( g_pMatchFramework->GetMatchSession()->GetSessionSettings()->GetInt( "members/numMachines", 0 ) < 2 ) )
			return 0;
		break;
	default:
		// There can be no P2P communication on any other socket type
		return 0;
	}

	uint32 cubMsg = 0;
	CSteamID steamIDRemote;

	// if no data to read, will return false
	if ( !Steam3Client().SteamNetworking()->ReadP2PPacket( buf, len, &cubMsg, &steamIDRemote, GetChannelForSocketType( eSocketType ) ) || cubMsg == 0 )
		return 0;

	// We have the SteamID for a user who sent us a packet on the channel, but we dont know on what channel the sender is waiting for a response.
	// We could add the response channel to each packet, but because clients only communicate with servers, and vice versa, we can assume
	// that the sender is our opposite. This conversion is done in CSteamSocket::GetTargetSocketType()

	// could be a new connection from this user.. add if necessary
	CSteamSocket *pSocket = FindSocketForUser( eSocketType, steamIDRemote );
	if ( !pSocket )
	{
		pSocket = CreateConnection( eSocketType, steamIDRemote );
		Assert( pSocket );
	}

	// got data.. update params
	*from = ns_address( pSocket->GetNetAddress() );

	if ( net_steamcnx_debug.GetInt() >= 3 )
	{
		ConColorMsg( STEAM_CNX_COLOR, "  Received from %llx %u bytes on %s\n", steamIDRemote.ConvertToUint64(), cubMsg, s_SocketIndexMap->GetNameByToken( eSocketType ) );
	}

	return cubMsg;
}

ISteamSocketMgr::ESteamCnxType CSteamSocketMgr::GetCnxType()
{
	return (ISteamSocketMgr::ESteamCnxType)clamp( net_steamcnx_enabled.GetInt(), (int)ESCT_NEVER, (int)ESCT_MAXTYPE - 1 );
}

uint64 CSteamSocketMgr::GetSteamIDForRemote( const ns_address &remote )
{
	const CSteamSocket *pSocket = FindSocketForAddress( remote );
	if ( pSocket )
	{
		return pSocket->GetSteamID().ConvertToUint64();
	}
	return 0ull;
}

void CSteamSocketMgr::PrintStatus()
{
	ConColorMsg( STEAM_CNX_COLOR, "SteamSocketMgr Status\n" );

	if ( !IsValid() )
	{
		ConColorMsg( STEAM_CNX_COLOR, " Invalid (no Steam3Client API?)\n" );
		return;
	}

	// print socket info
	ConColorMsg( STEAM_CNX_COLOR, " %d connections\n", m_vecRemoteSockets.Count() );
	FOR_EACH_VEC( m_vecRemoteSockets, i )
	{	
		CSteamSocket *pSocket = m_vecRemoteSockets[i];

		P2PSessionState_t p2pSessionState;
		if ( !Steam3Client().SteamNetworking()->GetP2PSessionState( pSocket->GetSteamID(), &p2pSessionState ) )
		{
			ConColorMsg( STEAM_CNX_COLOR, " %d %llx, failed to get session state\n", i, pSocket->GetSteamID().ConvertToUint64() );
			continue;
		}

		ConColorMsg( STEAM_CNX_COLOR, " %d %llx, type(%s), psuedoAddr(%s), connected(%s), connecting(%s), relay(%s), bytesQueued(%d), packetsQueued(%d), lasterror(%s)\n", 
			i,
			pSocket->GetSteamID().ConvertToUint64(),
			s_SocketIndexMap->GetNameByToken( pSocket->GetSocketType() ),
			pSocket->GetNetAddress().ToString(),
			p2pSessionState.m_bConnectionActive ? "yes" : "no",
			p2pSessionState.m_bConnecting ? "yes" : "no",
			p2pSessionState.m_bUsingRelay ? "yes" : "no",
			p2pSessionState.m_nBytesQueuedForSend,
			p2pSessionState.m_nPacketsQueuedForSend,
			s_EP2PSessionErrorIndexMap->GetNameByToken( (EP2PSessionError)p2pSessionState.m_eP2PSessionError ) );
	}
}

#else

// For LINUX it's basically all stubbed
#ifdef _PS3
ASSERT_INVARIANT( sizeof( int ) == sizeof( socklen_t ) );
#endif

class CSteamSocketMgr : public ISteamSocketMgr
{
public:
	virtual void Init() {}
	virtual void Shutdown() {}

	ISteamSocketMgr::ESteamCnxType GetCnxType() { return ESCT_NEVER; }

	virtual void OpenSocket( int s, int nModule, int nSetPort, int nDefaultPort, const char *pName, int nProtocol, bool bTryAny ) {}
	virtual void CloseSocket( int s, int nModule )
	{
		if ( g_ThreadedSocketQueue.ShouldUseSocketsThreaded() )
			g_ThreadedSocketQueue.CloseSocket( s );
	}

	virtual int sendto( int s, const char * buf, int len, int flags, const ns_address &to ) OVERRIDE
	{
		if ( to.IsType<netadr_t>() )
		{
			sockaddr sadr;
			to.AsType<netadr_t>().ToSockadr( &sadr );
			return ::sendto( s, buf, len, flags, &sadr, sizeof(sadr) );
		}
		AssertMsg1( false, "Tried to send to non-IP address '%s'", ns_address_render( to ).String() );
		return -1;
	}

	virtual int recvfrom( int s, char * buf, int len, int flags, ns_address *from ) OVERRIDE
	{
		sockaddr sadrfrom;
		socklen_t fromlen = sizeof(sadrfrom);
		int iret = ( g_ThreadedSocketQueue.ShouldUseSocketsThreaded() )
			? g_ThreadedSocketQueue.recvfrom( s, buf, len, &sadrfrom )
			: ::recvfrom( s, buf, len, flags, &sadrfrom, &fromlen );	

		if ( iret > 0 )
			from->SetFromSockadr( &sadrfrom );
		return iret;
	}

	virtual uint64 GetSteamIDForRemote( const ns_address &remote )
	{
		return 0ull;
	}

	void PrintStatus()
	{
	}
};

#endif

CSteamSocketMgr g_SteamSocketMgr;
ISteamSocketMgr *g_pSteamSocketMgr = &g_SteamSocketMgr;

netadr_t NET_InitiateSteamConnection(int sock, uint64 uSteamID, const char *format, ...)
{
	netadr_t adr;
#if defined( USE_STEAM_SOCKETS )
	if ( uSteamID == 0ull )
	{
		Warning( "NET_InitiateSteamConnection called with uSteamID == 0\n" );
		return adr;
	}

	va_list		argptr;
	char		string[ MAX_ROUTABLE_PAYLOAD ];

	va_start( argptr, format );
	Q_vsnprintf( string, sizeof( string ), format, argptr );
	va_end( argptr );

	int length = Q_strlen( string );

	CUtlBuffer sendBuf;
	sendBuf.PutUnsignedInt( (unsigned int)-1 );
	sendBuf.Put( string, length );

	CSteamID steamID;
	steamID.SetFromUint64( uSteamID );

	if ( net_steamcnx_debug.GetBool() )
	{
		ConColorMsg( STEAM_CNX_COLOR, "Initiate %llx\n", uSteamID );
	}

	CSteamSocket *pSocket = g_SteamSocketMgr.InitiateConnection( (ESocketIndex_t)sock, steamID, (const byte *)sendBuf.Base(), sendBuf.TellPut() );
	if ( !pSocket )
	{
		Warning( "NET_InitiateSteamConnection failed to create a socket\n" );
		return adr;
	}

	adr = pSocket->GetNetAddress();
#endif
	return adr;
}

void NET_TerminateSteamConnection( int sock, uint64 uSteamID )
{
#if defined( USE_STEAM_SOCKETS )
	if ( uSteamID == 0ull )
		return;

	if ( net_steamcnx_debug.GetBool() )
	{
		ConColorMsg( STEAM_CNX_COLOR, "Terminate %llx\n", uSteamID );
	}

	g_SteamSocketMgr.DestroyConnection( (ESocketIndex_t)sock, uSteamID );
#endif
}

CON_COMMAND( net_steamcnx_status, "Print status of steam connection sockets." )
{
	g_SteamSocketMgr.PrintStatus();
}