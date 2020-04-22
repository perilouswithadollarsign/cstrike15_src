//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef BASECLIENTSTATE_H
#define BASECLIENTSTATE_H
#ifdef _WIN32
#pragma once
#endif

#include "inetmsghandler.h"
#include "protocol.h"
#include "client_class.h"
#include "cdll_int.h"
#include "tier1/netadr.h"
#include "common.h"
#include "clockdriftmgr.h"
#include "convar.h"
#include "cl_bounded_cvars.h"
#include "tier1/utlstring.h"
#include "netmessages.h"
#include "utlmap.h"

#include "matchmaking/imatchasync.h"
#include "netmessages.pb.h"

 // Only send this many requests before timing out.
#define CL_CONNECTION_RETRIES		4 

// Mininum time gap (in seconds) before a subsequent connection request is sent.
#define CL_MIN_RESEND_TIME			1.5f   
// Max time.  The cvar cl_resend is bounded by these.
#define CL_MAX_RESEND_TIME			20.0f   

// In release, send commands at least this many times per second
#define MIN_CMD_RATE				10.0f
#define MAX_CMD_RATE				128.0f

typedef intp SerializedEntityHandle_t;

extern ConVar cl_name;

abstract_class CBaseClientState;

// This represents a server's 
class C_ServerClassInfo
{
public:
				C_ServerClassInfo();
				~C_ServerClassInfo();

public:

	ClientClass	*m_pClientClass;
	char		*m_ClassName;
	char		*m_DatatableName;

	// This is an index into the network string table (GetBaseLocalClient().GetInstanceBaselineTable()).
	int			m_InstanceBaselineIndex; // INVALID_STRING_INDEX if not initialized yet.
};

#define EndGameAssertMsg( assertion, msg ) \
	if ( !(assertion) )\
		Host_EndGame msg


class CNetworkStringTableContainer;
class PackedEntity;
class INetworkStringTable;
class CEntityReadInfo;	

struct DeferredConnection_t
{
	DeferredConnection_t()
	{
		m_bActive = false;
		m_nChallenge = 0;
		m_nAuthprotocol = 0;
		m_unGSSteamID = 0ull;;
		m_unLobbyID = 0ull;
		m_bGSSecure = false;
		m_bRequiresPassword = false;
		m_bDCFriendsReqd = false;
		m_bOfficialValveServer = false;
		m_nEncryptionKey = 0;
		m_nEncryptedSize = 0;
		memset( m_chLobbyType, 0, sizeof( m_chLobbyType ) );
	}

	bool	m_bActive;
	bool	m_bGSSecure;
	bool	m_bRequiresPassword;
	bool	m_bDCFriendsReqd;
	bool	m_bOfficialValveServer;
	char	m_chLobbyType[16];

	int		m_nChallenge;
	int		m_nAuthprotocol;
	uint64	m_unGSSteamID;
	uint64	m_unLobbyID;
	ns_address m_adrServerAddress;
	int		m_nEncryptionKey;
	int		m_nEncryptedSize;
};

// 0 == public, 1 == private, 2 == double NAT'd private (for  direct connection)
struct Remote_t
{
	bool Resolve();

	CUtlString	m_szAlias; // debugging: "public", "private", "direct"
	CUtlString	m_szRetryAddress;
	// Address for actual packets (may  differ from m_szRetryAddress)
	ns_address	m_adrRemote;
};

class CAddressList
{
public:

	CAddressList() {}

	void RemoveAll();
	bool IsRemoteInList( char const *pchAdrCheck ) const;
	bool IsAddressInList( const ns_address &adr ) const;

	// Only adds if not in list already
	void AddRemote( char const *pchAddress, char const *pchAlias );

	void Describe( CUtlString &str );

	int	Count() const;
	Remote_t &Get( int index );
	const Remote_t &Get( int index ) const;

private:

	CUtlVector< Remote_t > m_List;
};

// Classes to help with sending messages to server, with retries and timeouts

// Start by sending the message immediately. Wait for timeout before
// sending message again. 
// If maxAttempts exceeded with no response set state to AOS_FAILED and call callback.
// If a response is received set state to AOS_SUCCEEDED and call callback. 
// Result is only valid if state == AOS_SUCCEEDED
class CServerMsg : public IMatchAsyncOperation
{
public:

	// Methods of IMatchAsyncOperation
	bool IsFinished() { return m_eState > AOS_ABORTING; }
	AsyncOperationState_t GetState() { return m_eState; }
	uint64 GetResult() { return m_result; }
	void Abort() { m_eState = AOS_ABORTED; }

	// Timeout in seconds
	explicit CServerMsg( CBaseClientState *pParent, IMatchAsyncOperationCallback *pCallback, 
		const ns_address& serverAdr, int socket, uint32 maxAttempts, double timeout );

	// Per frame update, check for timeout etc if state == AOS_RUNNING
	void Update( void );

	// Call this function to check if it is ok to process a msg
	bool IsValidResponse( const ns_address &from, uint32 token );

	// Called by derived class when response received. Sets state to AOS_SUCCEEDED
	// and calls callback
	void ResponseReceived( uint64 result );

	// Methods derived class must implement. Token is a random number that can be 
	// used to match up a message-response pair. A new token is generated for each
	// SendMsg call; the last token generated can be queried using GetLastToken()
	virtual void SendMsg( const ns_address& serverAdr, int socket, uint32 token ) = 0;

	// See comment for SendMsg()
	uint32 GetLastToken() { return m_lastToken; }

	const ns_address& GetServerAddr() { return m_serverAdr; }

	// base client state parent
	CBaseClientState *m_pParent;

	// Data members
	AsyncOperationState_t m_eState;

	IMatchAsyncOperationCallback *m_pCallback;
	ns_address m_serverAdr;
	int m_socket;

	double m_lastMsgSendTime;
	double m_timeOut;

	uint32 m_maxAttempts;
	uint32 m_numAttempts;

	uint64 m_result;

	uint32 m_lastToken;	// See comment for SendMsg()
};

class CServerMsg_CheckReservation : public CServerMsg
{
public:

	explicit CServerMsg_CheckReservation( CBaseClientState *pParent, IMatchAsyncOperationCallback *pCallback, 
		const ns_address &serverAdr, int socket, uint64 reservationCookie, uint32 uiReservationStage );

	void Release();
	void SendMsg( const ns_address& serverAdr, int socket, uint32 token );
	void ResponseReceived( const ns_address& from, bf_read &msg, int32 hostVersion, uint32 token );

	uint64 m_reservationCookie;
	uint32 m_uiReservationStage;
};

class CServerMsg_Ping : public CServerMsg
{
public:

	explicit CServerMsg_Ping( CBaseClientState *pParent, IMatchAsyncOperationCallback *pCallback, 
		const ns_address &serverAdr, int socket );

	void Release();
	void SendMsg( const ns_address& serverAdrserverAdr, int socket, uint32 token );
	void ResponseReceived( const ns_address& from, bf_read &msg, int32 hostVersion, uint32 token );

	double m_timeLastMsgSent;
};

// CBaseClientState
abstract_class CBaseClientState :
	public INetChannelHandler,
	public IConnectionlessPacketHandler
{
	
public:
	CBaseClientState();
	virtual ~CBaseClientState();

public: // IConnectionlessPacketHandler interface:
		
	virtual bool ProcessConnectionlessPacket(struct netpacket_s *packet);

public: // INetMsgHandler interface:
		
	virtual void ConnectionStart(INetChannel *chan) OVERRIDE;
	virtual void ConnectionStop( ) OVERRIDE;
	virtual void ConnectionClosing( const char *reason );
	virtual void ConnectionCrashed(const char *reason);

	virtual void PacketStart(int incoming_sequence, int outgoing_acknowledged) {};
	virtual void PacketEnd( void ) {};

	virtual void FileReceived( const char *fileName, unsigned int transferID, bool isReplayDemoFile );
	virtual void FileRequested(const char *fileName, unsigned int transferID, bool isReplayDemoFile );
	virtual void FileDenied(const char *fileName, unsigned int transferID, bool isReplayDemoFile );
	virtual void FileSent(const char *fileName, unsigned int transferID, bool isReplayDemoFile );

	virtual bool ChangeSplitscreenUser( int nSplitScreenUserSlot ); // interleaved networking used by SS system is changing the SS player slot that the subsequent messages pertain to

public: // IServerMessageHandlers
	
	virtual bool NETMsg_Tick( const CNETMsg_Tick& msg );
	static bool NETMsg_Tick_Delegate( CBaseClientState *pThis, const CNETMsg_Tick& msg ) { return pThis->NETMsg_Tick( msg ); }
	virtual bool NETMsg_StringCmd( const CNETMsg_StringCmd& msg );
	bool NETMsg_SignonState( const CNETMsg_SignonState& msg );
	virtual bool NETMsg_PlayerAvatarData( const CNETMsg_PlayerAvatarData& msg );
	virtual bool NETMsg_SetConVar( const CNETMsg_SetConVar& msg );

	bool SVCMsg_CmdKeyValues( const CSVCMsg_CmdKeyValues& msg);
	virtual bool SVCMsg_EncryptedData( const CSVCMsg_EncryptedData& msg );
	bool SVCMsg_SendTable( const CSVCMsg_SendTable& msg );
	bool SVCMsg_Print( const CSVCMsg_Print& msg );
	virtual bool SVCMsg_ServerInfo( const CSVCMsg_ServerInfo& msg );
	virtual bool SVCMsg_ClassInfo( const CSVCMsg_ClassInfo& msg );
	virtual bool SVCMsg_SetPause( const CSVCMsg_SetPause& msg );
	virtual bool SVCMsg_SetView( const CSVCMsg_SetView& msg );
	virtual bool SVCMsg_CreateStringTable( const CSVCMsg_CreateStringTable& msg );
	virtual bool SVCMsg_UpdateStringTable( const CSVCMsg_UpdateStringTable& msg );
	virtual bool SVCMsg_VoiceInit( const CSVCMsg_VoiceInit& msg ) = 0;
	virtual bool SVCMsg_VoiceData( const CSVCMsg_VoiceData& msg ) = 0;
	virtual bool SVCMsg_FixAngle( const CSVCMsg_FixAngle& msg ) = 0;
	virtual bool SVCMsg_Prefetch( const CSVCMsg_Prefetch& msg ) = 0;
	virtual bool SVCMsg_CrosshairAngle( const CSVCMsg_CrosshairAngle& msg ) = 0;
	virtual bool SVCMsg_BSPDecal( const CSVCMsg_BSPDecal& msg ) = 0;
	virtual bool SVCMsg_SplitScreen( const CSVCMsg_SplitScreen& msg );
	virtual bool SVCMsg_GetCvarValue( const CSVCMsg_GetCvarValue& msg );
	virtual bool SVCMsg_Menu( const CSVCMsg_Menu& msg );
	virtual bool SVCMsg_UserMessage( const CSVCMsg_UserMessage& msg ) = 0;
	virtual bool SVCMsg_PaintmapData( const CSVCMsg_PaintmapData& msg ) = 0;
	virtual bool SVCMsg_GameEvent( const CSVCMsg_GameEvent& msg ) = 0;
	virtual bool SVCMsg_GameEventList( const CSVCMsg_GameEventList &msg );
	virtual bool SVCMsg_TempEntities( const CSVCMsg_TempEntities& msg ) = 0;
	virtual bool SVCMsg_PacketEntities( const CSVCMsg_PacketEntities& msg );
	virtual bool SVCMsg_Sounds( const CSVCMsg_Sounds& msg ) = 0;
	virtual bool SVCMsg_EntityMsg( const CSVCMsg_EntityMsg& msg ) = 0;
	
	CNetMessageBinder m_NETMsgTick;
	CNetMessageBinder m_NETMsgStringCmd;
	CNetMessageBinder m_NETMsgSignonState;
	CNetMessageBinder m_NETMsgSetConVar;
	CNetMessageBinder m_NETMsgPlayerAvatarData;
	
	CNetMessageBinder m_SVCMsgServerInfo;
	CNetMessageBinder m_SVCMsgCmdKeyValues;
	CNetMessageBinder m_SVCMsg_EncryptedData;
	CNetMessageBinder m_SVCMsgClassInfo;
	CNetMessageBinder m_SVCMsgSendTable;
	CNetMessageBinder m_SVCMsgPrint;
	CNetMessageBinder m_SVCMsgSetPause;
	CNetMessageBinder m_SVCMsgSetView;
	CNetMessageBinder m_SVCMsgCreateStringTable;
	CNetMessageBinder m_SVCMsgUpdateStringTable;
	CNetMessageBinder m_SVCMsgVoiceInit;
	CNetMessageBinder m_SVCMsgVoiceData;
	CNetMessageBinder m_SVCMsgFixAngle;
	CNetMessageBinder m_SVCMsgPrefetch;
	CNetMessageBinder m_SVCMsgCrosshairAngle;
	CNetMessageBinder m_SVCMsgBSPDecal;
	CNetMessageBinder m_SVCMsgSplitScreen;
	CNetMessageBinder m_SVCMsgGetCvarValue;
	CNetMessageBinder m_SVCMsgMenu;
	CNetMessageBinder m_SVCMsgUserMessage;
	CNetMessageBinder m_SVCMsgPaintmapData;
	CNetMessageBinder m_SVCMsgGameEvent;
	CNetMessageBinder m_SVCMsgGameEventList;
	CNetMessageBinder m_SVCMsgTempEntities;
	CNetMessageBinder m_SVCMsgPacketEntities;
	CNetMessageBinder m_SVCMsgSounds;
	CNetMessageBinder m_SVCMsgEntityMsg;

public: // IMatchEventsSink
	virtual void OnEvent( KeyValues *pEvent );

public: 
	inline	bool IsActive( void ) const { return m_nSignonState == SIGNONSTATE_FULL; };
	inline	bool IsConnected( void ) const { return m_nSignonState >= SIGNONSTATE_CONNECTED; };
	inline	bool IsConnecting( void ) const { return m_nSignonState >= SIGNONSTATE_NONE; }
	virtual	void Clear( void );
	virtual void FullConnect( const ns_address &adr, int nEncryptionKey ); // a connection was established
	virtual void Connect( const char *pchPublicAddress, char const *pchPrivateAddress, const char* szJoinType ); // start a connection challenge
	virtual void ConnectSplitScreen( const char *pchPublicAddress, char const *pchPrivateAddress, int numPlayers, const char* szJoinType ); // start a connection challenge
	virtual bool SetSignonState ( int state, int count, const CNETMsg_SignonState *msg );
	virtual void Disconnect( bool bShowMainMenu = true );
	virtual void SendConnectPacket ( const ns_address &netAdrRemote, int challengeNr, int authProtocol, uint64 unGSSteamID, bool bGSSecure );
	virtual const char *GetCDKeyHash() { return "123"; }
	virtual void RunFrame ( void );
	virtual void CheckForResend ( bool bForceResendNow = false );
	virtual void CheckForReservationResend( void );
	virtual void ResendGameDetailsRequest( const ns_address &adr );
	virtual void InstallStringTableCallback( char const *tableName ) { }
	virtual bool HookClientStringTable( char const *tableName ) { return false; }
	virtual bool LinkClasses( void );
	virtual int  GetConnectionRetryNumber() const { return m_nRetryMax; }
	virtual const char *GetClientName() { return cl_name.GetString(); }
	virtual void ReserveServer( const ns_address &netAdrPublic, const ns_address &netAdrPrivate, uint64 nServerReservationCookie,
		KeyValues *pKVGameSettings, IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation );

	bool CheckServerReservation( const ns_address &netAdrPublic, uint64 nServerReservationCookie, uint32 uiReservationStage, IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation );
	bool ServerPing( const ns_address &netAdrPublic, IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation );

	struct ReservationResponseReply_t
	{
		ReservationResponseReply_t() { m_uiResponse = 0; m_bValveDS = false; m_numGameSlots = 0; }
		ns_address m_adrFrom;
		uint8 m_uiResponse;
		bool m_bValveDS;
		uint32 m_numGameSlots;
	};
	virtual void HandleReservationResponse( const ReservationResponseReply_t &reply );
	virtual void HandleReserveServerChallengeResponse( int nChallengeNr );

	virtual void SetServerReservationCookie( uint64 nReservationCookie ) { m_nServerReservationCookie = nReservationCookie; }

	static ClientClass* FindClientClass(const char *pClassName);

	CClockDriftMgr& GetClockDriftMgr();
	
	int GetClientTickCount() const;	// Get the client tick count.
	void SetClientTickCount( int tick );

	int GetServerTickCount() const;
	void SetServerTickCount( int tick );

	void SetClientAndServerTickCount( int tick );

	INetworkStringTable *GetStringTable( const char * name ) const;
	
	PackedEntity *GetEntityBaseline( int iBaseline, int nEntityIndex );
	void SetEntityBaseline(int iBaseline, ClientClass *pClientClass, int index, SerializedEntityHandle_t handle);
	void CopyEntityBaseline( int iFrom, int iTo );
	void FreeEntityBaselines();
	bool GetClassBaseline( int iClass, SerializedEntityHandle_t *pHandle );
	void UpdateInstanceBaseline( int nStringNumber );
	ClientClass *GetClientClass( int i );

	void ForceFullUpdate( char const *pchReason );
	void SendStringCmd(const char * command);
	
	virtual void ReadPacketEntities( CEntityReadInfo &u ) = 0;

	int				GetViewEntity();
	void			HandleDeferredConnection();
	void			SetConnectionPassword( char const *pchCurrentPW );
	void			ResetConnectionRetries();

	virtual bool IsClientStateTv() const { return false; }
protected:

	bool InternalProcessStringCmd( const CNETMsg_StringCmd& msg );

private:
	bool PrepareSteamConnectResponse( uint64 unGSSteamID, bool bGSSecure, const ns_address &adr, bf_write &msg );
	void SendReserveServerMsg();	
	void SendReserveServerChallenge();
	void BuildReserveServerPayload( bf_write &msg, int nChallengeNr );

	void SendReservationCheckMsg( const ns_address &netAdrPublic, uint64 nServerReservationCookie );

	int FindSplitPlayerSlot( int nPlayerIndex );

	void ConnectInternal( const char *pchPublicAddress, char const *pchPrivateAddress, int numPlayers, const char* szJoinType );

	void RememberIPAddressForLobby( uint64 unLobbyID, const ns_address &adrRemote );
	bool IsRemoteInList( char const *pchAdrCheck ) const;
	bool ShouldUseDirectConnectAddress( const CAddressList &list ) const;

public:
	// Connection to server.			
	int				m_Socket;		// network socket 
	INetChannel		*m_NetChannel;		// Our sequenced channel to the remote server.
	bool			m_bSplitScreenUser;
	unsigned int	m_nChallengeNr;	// connection challenge number
	double			m_flConnectTime;	// If gap of connect_time to net_time > 3000, then resend connect packet
	int				m_nRetryNumber;	// number of retry connection attempts
	int				m_nRetryMax;	// max # of retry attempts allowed

	// Address for actual packets (may  differ from m_szRetryAddress)
	CAddressList	m_Remote;
	
	struct DirectConnectLobby_t
	{
		DirectConnectLobby_t() : m_flEndTime( -1 ), m_unLobbyID( 0ull ) {}

		float		m_flEndTime;
		ns_address	m_adrRemote;
		uint64		m_unLobbyID;
	};

	DirectConnectLobby_t	m_DirectConnectLobby;

	uint64			m_ListenServerSteamID;

	int				m_nSignonState;    // see SIGNONSTATE_* definitions
	double			m_flNextCmdTime; // When can we send the next command packet?
	int				m_nServerCount;	// server identification for prespawns, must match the svs.spawncount which
									// is incremented on server spawning.  This supercedes svs.spawn_issued, in that
									// we can now spend a fair amount of time sitting connected to the server
									// but downloading models, sounds, etc.  So much time that it is possible that the
									// server might change levels again and, if so, we need to know that.
	int			m_nCurrentSequence;	// this is the sequence number of the current incoming packet	

	uint64		m_ulGameServerSteamID; // Steam ID of the game server we are trying to connect to, or are connected to.  Zero if unknown

	CClockDriftMgr m_ClockDriftMgr;

	int			m_nDeltaTick;		//	last valid received snapshot (server) tick
	bool		m_bPaused;			// send over by server
	int			m_nViewEntity;		// player point of view override

	int			m_nPlayerSlot;		// own player entity index-1. skips world. Add 1 to get cl_entitites index;
	int			m_nSplitScreenSlot;

	char		m_szLevelName[ MAX_PATH ];	// for display on solo scoreboard
	char		m_szLevelNameShort[ 40 ]; // removes maps/ and .bsp extension
	char		m_szMapGroupName[ 40 ]; //the name of the map group we are currently playing in
	char		m_szLastLevelNameShort[ 40 ]; // stores the previous value of m_szLevelNameShort when that gets cleared
	PublishedFileId_t m_unUGCMapFileID; // If a community map, this is the published file id

	int			m_nMaxClients;		// max clients on server
	int			m_nNumPlayersToConnect;	// number of clients to connect to server.

	class CAsyncOperation_ReserveServer : public IMatchAsyncOperation
	{
	public:
		explicit CAsyncOperation_ReserveServer( CBaseClientState *pParent ) : m_eState( AOS_RUNNING ), m_pParent( pParent ) 
		{
			m_numGameSlotsForReservation = 0;
		}

		virtual bool IsFinished() { return m_eState > AOS_ABORTING; }
		virtual AsyncOperationState_t GetState() { return m_eState; }
		virtual uint64 GetResult();
		virtual uint64 GetResultExtraInfo() { return m_numGameSlotsForReservation; }
		virtual void Abort() {}
		virtual void Release()
		{
			if ( m_pParent && m_pParent->m_pServerReservationOperation == this )
			{
				m_pParent->m_pServerReservationOperation = NULL;
				m_pParent->m_pServerReservationCallback = NULL;
			}
			delete this;
		}

	public:
		AsyncOperationState_t m_eState;
		ns_address m_adr;
		CBaseClientState *m_pParent;
		uint32 m_numGameSlotsForReservation;
	};
	
	CAsyncOperation_ReserveServer *m_pServerReservationOperation;			// server reservation operation
	IMatchAsyncOperationCallback *m_pServerReservationCallback;		// callback for pending reservation request

	CUtlVector< CServerMsg_CheckReservation * > m_arrSvReservationCheck;
	CUtlVector< CServerMsg_Ping * > m_arrSvPing;

	uint64 m_nServerReservationCookie;								// cookie to set during reservation and provide upon connection
	KeyValues *m_pKVGameSettings;									// game settings to request on reserved server
	double m_flReservationMsgSendTime;								// time we last sent reservation msg
	int m_nReservationMsgRetryNumber;								// # of times we've retried sending reservation msg
	CAddressList m_netadrReserveServer;									// netadr of server we're trying to reserve
	bool	m_bEnteredPassword;
	bool	m_bWaitingForPassword;

#if ENGINE_CONNECT_VIA_MMS
	bool	m_bWaitingForServerGameDetails;
#endif

	DeferredConnection_t			m_DeferredConnection;
	CUtlMap< int32, byte*, int32, CDefLess< int32 > > m_mapGeneratedEncryptionKeys;

	PackedEntity	*m_pEntityBaselines[2][MAX_EDICTS];	// storing entity baselines
		
	// This stuff manages the receiving of data tables and instantiating of client versions
	// of server-side classes.
	C_ServerClassInfo	*m_pServerClasses;
	int					m_nServerClasses;
	int					m_nServerClassBits;
	char				m_szEncryptionKey[STEAM_KEYSIZE];
	unsigned int		m_iEncryptionKeySize;

	CNetworkStringTableContainer *m_StringTableContainer;

	CUtlMap< int, SerializedEntityHandle_t > m_BaselineHandles;

	typedef CUtlMap< uint32, CNETMsg_PlayerAvatarData_t *, int, CDefLess< uint32 > > PlayerAvatarDataMap_t;
	PlayerAvatarDataMap_t m_mapPlayerAvatarData;
	CNETMsg_PlayerAvatarData_t * AllocOwnPlayerAvatarData() const;
	
	bool m_bRestrictServerCommands;	// If true, then the server is only allowed to execute commands marked with FCVAR_SERVER_CAN_EXECUTE on the client.
	bool m_bRestrictClientCommands;	// If true, then IVEngineClient::ClientCmd is only allowed to execute commands marked with FCVAR_CLIENTCMD_CAN_EXECUTE on the client.

	// tracks valid reception of server info
	bool m_bServerInfoProcessed;
	int m_nServerProtocolVersion;
	int m_nServerInfoMsgProtocol;
	bool m_bServerConnectionRedirect;
};


inline CClockDriftMgr& CBaseClientState::GetClockDriftMgr()
{
	return m_ClockDriftMgr;
}


inline void CBaseClientState::SetClientTickCount( int tick )
{
	m_ClockDriftMgr.m_nClientTick = tick;
}

inline int CBaseClientState::GetClientTickCount() const
{
	return m_ClockDriftMgr.m_nClientTick;
}

inline int CBaseClientState::GetServerTickCount() const
{
	return m_ClockDriftMgr.m_nServerTick;
}

inline void CBaseClientState::SetServerTickCount( int tick )
{
	m_ClockDriftMgr.m_nServerTick = tick;
}

inline void CBaseClientState::SetClientAndServerTickCount( int tick )
{
	m_ClockDriftMgr.m_nServerTick = m_ClockDriftMgr.m_nClientTick = tick;
}

#endif // BASECLIENTSTATE_H
