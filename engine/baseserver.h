//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef BASESERVER_H
#define BASESERVER_H
#ifdef _WIN32
#pragma once
#endif

#include <iserver.h>
#include <netadr.h>
#include <bitbuf.h>
#include <utlvector.h>
#include "baseclient.h"
#include "netmessages.h"
#include "net.h"
#include "event_system.h"
#include <utlmap.h>

class CNetworkStringTableContainer;
class PackedEntity;
class ServerClass;
class INetworkStringTable;	
typedef intp SerializedEntityHandle_t;

enum server_state_t
{
	ss_dead = 0,	// Dead
	ss_loading,		// Spawning
	ss_active,		// Running
	ss_paused,		// Running, but paused
};

// MAX_CHALLENGES is made large to prevent a denial
//  of service attack that could cycle all of them
//  out before legitimate users connected
#define MAX_CHALLENGES 16384

// time a challenge is valid for, in seconds
#define CHALLENGE_LIFETIME 60*60.0f

// MAX_DELTA_TICKS defines the maximum delta difference allowed 
// for delta compression, if clients request on older tick as
// delta baseline, send a full update. 
#define MAX_DELTA_TICKS	192		// this is about 3 seconds

typedef struct
{
	ns_address  adr;       // Address where challenge value was sent to.
	int			challenge; // To connect, adr IP address must respond with this #
	float		time;      // # is valid for only a short duration.
} challenge_t;


class CBaseServer  : public IServer
{
friend class CMaster;

public:
	CBaseServer();
	virtual ~CBaseServer();

	bool RestartOnLevelChange() { return m_bRestartOnLevelChange; }

public: // IServer implementation

	virtual int		GetNumClients( void ) const; // returns current number of clients
	virtual int		GetNumProxies( void ) const; // returns number of attached HLTV proxies
	virtual int		GetNumFakeClients() const; // returns number of fake clients/bots
	virtual int		GetMaxClients( void ) const; // returns current client limit
	virtual int		GetUDPPort( void ) const { return NET_GetUDPPort( m_Socket );	}
	virtual IClient	*GetClient( int index ) { return m_Clients[index]; } // returns interface to client 
	virtual int		GetClientCount() const { return m_Clients.Count(); } // for iteration;
	virtual float	GetTime( void ) const;
	virtual int		GetTick( void ) const { return m_nTickCount; }
	virtual float	GetTickInterval( void ) const { return m_flTickInterval; }
	virtual float	GetTimescale( void ) const { return m_flTimescale; }
	virtual const char *GetName( void ) const;
	virtual const char *GetMapName( void ) const { return m_szMapname; }
	virtual const char *GetBaseMapName( void ) const { return m_szBaseMapname; }
	virtual const char *GetMapGroupName( void ) const { return m_szMapGroupName; }
	virtual int		GetSpawnCount( void ) const { return m_nSpawnCount; }
	virtual int		GetNumClasses( void ) const { return serverclasses; }
	virtual int		GetClassBits( void ) const { return serverclassbits; }
	virtual void	GetNetStats( float &avgIn, float &avgOut );
	virtual int		GetNumPlayers();
	virtual	bool	GetPlayerInfo( int nClientIndex, player_info_t *pinfo );
	virtual float	GetCPUUsage( void ) { return m_fCPUPercent; }
		
	virtual bool	IsActive( void ) const { return m_State >= ss_active; }	
	virtual bool	IsLoading( void ) const { return m_State == ss_loading; }
	virtual bool	IsDedicated( void ) const { return m_bIsDedicated; }
	FORCEINLINE bool IsDedicatedForXbox( void ) const { return m_bIsDedicatedForXbox; }
	FORCEINLINE bool IsDedicatedForPS3( void ) const { return m_bIsDedicatedForPS3; }
	virtual bool	IsPaused( void ) const { return m_State == ss_paused; }
	virtual bool	IsMultiplayer( void ) const { return m_nMaxclients > 1; }
	virtual bool	IsPausable( void ) const { return false; }
	virtual bool	IsHLTV( void ) const { return false; }
	virtual bool	IsReplay( void ) const { return false; }

	virtual void	BroadcastMessage( INetMessage &msg, bool onlyActive = false, bool reliable = false );
	virtual void	BroadcastMessage( INetMessage &msg, IRecipientFilter &filter );
	virtual void	BroadcastPrintf ( PRINTF_FORMAT_STRING const char *fmt, ...) FMTFUNCTION( 2, 3 );

	virtual const char * GetPassword() const;

	virtual void	SetMaxClients( int number );
	virtual void	SetPaused(bool paused);
	virtual void	SetTimescale( float flTimescale );
	virtual void	SetPassword(const char *password);

	virtual void	DisconnectClient(IClient *client, const char *reason );
	
	virtual void 	WriteDeltaEntities( CBaseClient *client, CClientFrame *to, CClientFrame *from, CSVCMsg_PacketEntities_t &msg );
	virtual void	WriteTempEntities( CBaseClient *client, CFrameSnapshot *to, CFrameSnapshot *from, CSVCMsg_TempEntities_t &msg, int ev_max );
	
public: // IConnectionlessPacketHandler implementation

	virtual bool	ProcessConnectionlessPacket( netpacket_t * packet );

	virtual void	Init( bool isDedicated );
	virtual void	Clear( void );
	virtual void	Shutdown( void );
	virtual CBaseClient *CreateFakeClient(const char *name);
	virtual void 	RemoveClientFromGame( CBaseClient *client ) {};
	virtual void	SendClientMessages ( bool bSendSnapshots );
	virtual void	FillServerInfo(CSVCMsg_ServerInfo &serverinfo);
	virtual void	UserInfoChanged( int nClientIndex );

	virtual bool	GetClassBaseline( ServerClass *pClass, SerializedEntityHandle_t *pHandle);
	void	RunFrame( void );
	void	ProcessVoice( void );
	void	InactivateClients( void );
	void	ReconnectClients( void );
	void	CheckTimeouts (void);
	void	UpdateUserSettings(void);
	void	SendPendingServerInfo(void);
	void	OnSteamServerLogonSuccess( uint32 externalIP );

	INetworkStringTable *GetInstanceBaselineTable( void );
	INetworkStringTable *GetLightStyleTable( void );
	INetworkStringTable *GetUserInfoTable( void );

	virtual void	RejectConnection(const ns_address &adr, PRINTF_FORMAT_STRING const char *fmt, ... ) FMTFUNCTION( 3, 4 );

	float	GetFinalTickTime( void ) const;

	virtual bool CheckIPRestrictions( const ns_address &adr, int nAuthProtocol );

	void	SetMasterServerRulesDirty();
	void	SendQueryPortToClient( netadr_t &adr );

	void	RecalculateTags( void );
	void	AddTag( const char *pszTag, const char *pszSubTagValue = NULL );
	void	RemoveTag( const char *pszTag, bool bSubTag = false );

	CBaseClient *CreateSplitClient( const CMsg_CVars& vecUserInfo, CBaseClient *pAttachedTo );
	CBaseClient *GetBaseUserForSplitClient( CBaseClient *pSplitUser );

	void QueueSplitScreenDisconnect( CBaseClient *pSplitHost, CBaseClient *pSplitUser );
	void ProcessSplitScreenDisconnects();

	void		UpdateGameType();
	void		UpdateGameData();
	
	char const  *GetGameType() const;
	char const *GetGameData() const;
	int GetGameDataVersion() const;

	bool IsPlayingSoloAgainstBots() const;
	bool ShouldHideServer() const;
	bool ShouldHideFromMasterServer() const;
	int  GetMaxHumanPlayers() const;
	int  GetNumHumanPlayers() const;

	void GetMasterServerPlayerCounts( int &nHumans, int &nMaxHumanSlots, int &nBots );

	void OnPasswordChanged();

	void ShowTags() const;
	uint64 GetMatchId()const { return m_nMatchId; }
protected:

	virtual IClient *ConnectClient ( const ns_address &adr, int protocol, int challenge, int authProtocol, 
					    const char *name, const char *password, const char *hashedCDkey, int cdKeyLen,
						CUtlVector< CCLCMsg_SplitPlayerConnect_t* > & splitScreenClients, bool isClientLowViolence, CrossPlayPlatform_t clientPlatform,
						const byte *pbEncryptionKey, int nEncryptionKeyIndex );
	
	virtual bool GetRedirectAddressForConnectClient( const ns_address &adr, CUtlVector< CCLCMsg_SplitPlayerConnect_t* > & splitScreenClients, ns_address *pNetAdrRedirect ) { return false; }
	
	CBaseClient *GetFreeClient( const ns_address &adr );

	virtual CBaseClient *CreateNewClient( int slot ) { return NULL; }; // must be derived

	
	virtual bool	FinishCertificateCheck( const ns_address &adr, int nAuthProtocol, const char *szRawCertificate ) { return true; };
	
	virtual int		GetChallengeNr ( const ns_address &adr );
	virtual int		GetChallengeType ( const ns_address &adr );

	virtual bool	CheckProtocol( const ns_address &adr, int nProtocol );
	virtual bool	CheckChallengeNr( const ns_address &adr, int nChallengeValue );
	virtual bool	CheckChallengeType( CBaseClient *client, int nNewUserID, const ns_address &adr, int nAuthProtocol, const char *pchLogonCookie, int cbCookie );
	virtual bool	CheckPassword( const ns_address &adr, const char *password, const char *name );

	virtual void	ReplyChallenge( const ns_address &adr, bf_read &msg );
	virtual void	ReplyServerChallenge( const ns_address &adr);
	virtual void	ReplyReservationRequest( const ns_address &adr, bf_read &msg );
	virtual void	ReplyReservationCheckRequest( const ns_address &adr, bf_read &msg );

	virtual void	CalculateCPUUsage();

	// Keep the master server data updated.
	virtual bool	ShouldUpdateMasterServer();
	
	void			CheckMasterServerRequestRestart();
	void			UpdateMasterServer();
	void			UpdateMasterServerRules();
	virtual void	UpdateMasterServerPlayers() {}
	void			ForwardPacketsFromMasterServerUpdater();

	void SetRestartOnLevelChange(bool state)  { m_bRestartOnLevelChange = state; }

	bool RequireValidChallenge( const ns_address &adr );
	bool ValidChallenge( const ns_address &adr, int challengeNr );
	bool ValidInfoChallenge( const ns_address &adr, const char *nugget );

	void ClearBaselineHandles( void );

	// Data
public:

	server_state_t	m_State;		// some actions are only valid during load
	int				m_Socket;		// network socket 
	int				m_nTickCount;	// current server tick
	char			m_szMapname[MAX_PATH];		// map name and path without extension
	char			m_szBaseMapname[MAX_MAP_NAME];		// map name without path or extension
	char			m_szMapGroupName[64];	// map group name
	char			m_szSkyname[64];		// skybox name
	char			m_Password[32];		// server password

	CRC32_t			worldmapCRC;      // For detecting that client has a hacked local copy of map, the client will be dropped if this occurs.
	CRC32_t			clientDllCRC; // The dll that this server is expecting clients to be using.
	CRC32_t			stringTableCRC;
	CNetworkStringTableContainer *m_StringTables;	// newtork string table container

	INetworkStringTable *m_pInstanceBaselineTable; 
	INetworkStringTable *m_pLightStyleTable;
	INetworkStringTable *m_pUserInfoTable;
	INetworkStringTable *m_pServerStartupTable;
	INetworkStringTable *m_pDownloadableFileTable;

	CUtlMap< int, SerializedEntityHandle_t > m_BaselineHandles;

	// This will get set to NET_MAX_PAYLOAD if the server is MP.
	bf_write			m_Signon;
	CUtlMemory<byte>	m_SignonBuffer;

	int			serverclasses;		// number of unique server classes
	int			serverclassbits;	// log2 of serverclasses

	bool	IsReserved() const { return m_nReservationCookie != 0; }
	void	Unreserve();	// clear reservation if there is one
	void	UpdateReservedState();

	uint64	GetReservationCookie() const;
	void	SetReservationCookie( uint64 uiCookie, PRINTF_FORMAT_STRING char const *pchReasonFormat, ... ) FMTFUNCTION( 3, 4 );
	bool ReserveServerForQueuedGame( char const *szReservationPayload );
	
	bool	IsExclusiveToLobbyConnections() const;
	bool	IsSinglePlayerGame() const; // won't allow external connections, no heartbeat to master server, etc.
	int		GetNumGameSlots() const { return m_numGameSlots; }

	enum EReservationStatus_t
	{
		kEReservationStatusRejected = 0,		// Reservation is reject, client should go away
		kEReservationStatusSuccess = 1,			// Reservation is fully completed, client can connect
		kEReservationStatusPending = 2,			// Reservation is pending, client should wait and might get success packet later
	};
	void	SendReservationStatus( EReservationStatus_t kEReservationStatus );
	void	ClearReservationStatus();

	void	FlagForSteamIDReuseAfterShutdown();

private:

	// Gets the next user ID mod SHRT_MAX and unique (not used by any active clients).
	int			GetNextUserID();
	int			m_nUserid;			// increases by one with every new client


	void		ClearTagStrings();
	void		AddTagString( CUtlString &dest, char const *pchString );

protected:
	CBaseClient *GetFreeClientInternal( const ns_address &adr );

	int			m_nMaxclients;         // Current max #
	int			m_nSpawnCount;			// Number of servers spawned since start,
									// used to check late spawns (e.g., when d/l'ing lots of
									// data)
	float		m_flTickInterval;		// time for 1 tick in seconds
	float		m_flTimescale;			// the game time scale (multiplied in conjunction with host_timescale)


	CUtlVector<CBaseClient*>	m_Clients;		// array of up to [maxclients] client slots.
	
	bool		m_bIsDedicated;
	bool		m_bIsDedicatedForXbox;
	bool		m_bIsDedicatedForPS3;

	CUtlVector<challenge_t> m_ServerQueryChallenges; // prevent spoofed IP's from server queries/connecting

	float		m_fCPUPercent;
	float		m_fStartTime;
	float		m_fLastCPUCheckTime;

	// This is only used for Steam's master server updater to refer to this server uniquely.
	bool		m_bRestartOnLevelChange;
	double		m_flFlagForSteamIDReuseAfterShutdownTime;
	
	bool		m_bMasterServerRulesDirty;
	double		m_flLastMasterServerUpdateTime;
	struct SplitDisconnect_t
	{
		CBaseClient *m_pUser;
		CBaseClient *m_pSplit;
	};

	CUtlVector< SplitDisconnect_t >	m_QueuedForDisconnect;

	struct QueueMatchPlayer_t
	{
		uint32 m_uiAccountID;
		ns_address m_adr;
		uint32 m_uiToken;
		uint32 m_uiReservationStage;
	};
	CUtlVector< QueueMatchPlayer_t > m_arrReservationPlayers;	// Reservation players in queue mode
	uint64		m_nReservationCookie;			// if this server has been reserved, cookie that connecting clients must present
	uint64		*m_pnReservationCookieSession;	// cookie that represents a server session
	float		m_flReservationExpiryTime;		// time at which reservation expires
	float		m_flTimeLastClientLeft;			// time when last client left server
	int			m_numGameSlots;					// number of game slots allocated
	CUtlString	m_GameType;
	CUtlVector<char> m_GameData;
	int			m_GameDataVersion;

	float		m_flTimeReservationGraceStarted;	// time when client attempted to connect and was granted a reservation grace period
	netadr_t	m_adrReservationGraceStarted;		// netadr of the client for whom reservation grace has been given
	bool CanAcceptChallengesFrom( const ns_address &adrFrom ) const;

	struct ReservationStatus_t
	{
		ReservationStatus_t() : m_bActive( false ), m_bSuccess( false )
		{
		}

		bool		m_bActive;
		bool		m_bSuccess;
		ns_address	m_Remote;
	};

	ReservationStatus_t m_ReservationStatus;
	uint64 m_nMatchId;
};

extern CThreadFastMutex g_svInstanceBaselineMutex;

const ConVar &GetIndexedConVar( const ConVar &cv, int nIndex );



#endif // BASESERVER_H
