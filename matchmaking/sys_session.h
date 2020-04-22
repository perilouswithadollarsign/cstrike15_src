//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef SYS_SESSION_H
#define SYS_SESSION_H
#ifdef _WIN32
#pragma once
#endif

class CSysSessionBase;
class CSysSessionHost;
class CSysSessionClient;

#include "x360_lobbyapi.h"
#include "x360_netmgr.h"

class CSysSessionBase
#ifdef _X360
	: public IX360NetworkEvents
#endif
{
	friend class CSysSessionHost;
	friend class CSysSessionClient;

protected:
	CSysSessionBase( KeyValues *pSettings );
	virtual ~CSysSessionBase();

public:
	virtual bool Update();
	virtual void Destroy();
	virtual void Command( KeyValues *pCommand );

	uint64 GetReservationCookie();
	uint64 GetNonceCookie();
	uint64 GetSessionID();
	virtual XUID GetHostXuid( XUID xuidValidResult = 0ull ) = 0;

	void SetSessionActiveGameplayState( bool bActive, char const *szSecureServerAddress );
	void UpdateTeamProperties( KeyValues *pTeamProperties );
	void UpdateServerInfo( KeyValues *pServerKey );

public:
	void ReplyLanSearch( KeyValues *msg );

public:
	virtual void DebugPrint();
	void SendMessage( KeyValues *msg );

protected:
	virtual void ReceiveMessage( KeyValues *msg, bool bValidatedLobbyMember, XUID xuidSrc );
	virtual void OnPlayerLeave( XUID xuid );
	virtual bool IsServiceSession();

	bool FindAndRemovePlayerFromMembers( XUID xuid );
	void UpdateSessionProperties( KeyValues *kv, bool bHost );

	virtual void OnSessionEvent( KeyValues *notify );
	void SendEventsNotification( KeyValues *notify );
	bool SendQueuedEventsNotifications();

	void PrintValue( KeyValues *val, char *chBuffer, int numBytesBuffer );

#ifdef _X360

	CX360LobbyObject m_lobby;
	CX360NetworkMgr *m_pNetworkMgr;
	IX360LobbyAsyncOperation *m_pAsyncOperation;

	CX360LobbyMigrateHandle_t m_hLobbyMigrateCall;
	CX360LobbyMigrateOperation_t m_MigrateCallState;

	INetSupport::NetworkSocket_t GetX360NetSocket();

	virtual void OnAsyncOperationFinished() = 0;
	void ReleaseAsyncOperation();

	// IX360NetworkEvents
	virtual void OnX360NetPacket( KeyValues *msg );
	virtual void OnX360NetDisconnected( XUID xuidRemote );

	// Members management code
	void OnX360AllSessionMembersJoinLeave( KeyValues *kv );

	// Check whether host migration should be allowed
	// on the session
	virtual bool ShouldAllowX360HostMigration();

	virtual bool UpdateMigrationCall();

#elif !defined( NO_STEAM )
public:
	STEAM_CALLBACK_MANUAL( CSysSessionBase, Steam_OnLobbyChatMsg, LobbyChatMsg_t, m_CallbackOnLobbyChatMsg );
	STEAM_CALLBACK_MANUAL( CSysSessionBase, Steam_OnLobbyChatUpdate, LobbyChatUpdate_t, m_CallbackOnLobbyChatUpdate );
	STEAM_CALLBACK( CSysSessionBase, Steam_OnServersConnected, SteamServersConnected_t, m_CallbackOnServersConnected );
	STEAM_CALLBACK( CSysSessionBase, Steam_OnServersDisconnected, SteamServersDisconnected_t, m_CallbackOnServersDisconnected );
	STEAM_CALLBACK( CSysSessionBase, Steam_OnP2PSessionRequest, P2PSessionRequest_t, m_CallbackOnP2PSessionRequest );

protected:
	void UnpackAndReceiveMessage( const void *pvBuffer, int numBytes, bool bValidatedLobbyMember, XUID xuidSrc );
	void SetupSteamRankingConfiguration();
	bool IsSteamRankingConfigured() const;

protected:
	CSteamLobbyObject m_lobby;
	bool m_bVoiceUsingSessionP2P;

	char const * LobbyEnterErrorAsString( LobbyEnter_t *pLobbyEnter );
	void LobbySetDataFromKeyValues( char const *szPath, KeyValues *key, bool bRecurse = true );

#else
	uint64 m_lobby;
#endif

protected:
	// Voice engine
	virtual void Voice_ProcessTalkers( KeyValues *pMachine, bool bAdd );
	virtual void Voice_CaptureAndTransmitLocalVoiceData();
	virtual void Voice_Playback( KeyValues *msg, XUID xuidSrc );
	virtual void Voice_UpdateLocalHeadsetsStatus();
	float m_Voice_flLastHeadsetStatusCheck;

public:
	virtual void Voice_UpdateMutelist();

protected:
	KeyValues *m_pSettings;
	XUID m_xuidMachineId;

public:

	enum Result
	{
		RESULT_UNDEFINED,
		RESULT_FAIL,
		RESULT_SUCCESS
	};

	Result m_result;
	Result GetResult() { return m_result; }
};

class CSysSessionHost : public CSysSessionBase
{
public:
	explicit CSysSessionHost( KeyValues *pSettings );
	explicit CSysSessionHost( CSysSessionClient *pClient, KeyValues *pSettings );
	virtual ~CSysSessionHost();

public:
	virtual bool Update();
	virtual void Destroy();

	virtual XUID GetHostXuid( XUID xuidValidResult = 0ull );

	void KickPlayer( KeyValues *pCommand );

#ifdef _X360
	void GetHostSessionInfo( char chBuffer[ XSESSION_INFO_STRING_LENGTH ] );
	uint64 GetHostSessionId();
#endif

	void UpdateMembersInfo();
	void OnUpdateSessionSettings( KeyValues *kv );
	void OnPlayerUpdated( KeyValues *pPlayer );
	void OnMachineUpdated( KeyValues *pMachine );

	void Migrate( KeyValues *pCommand );

	void ReserveTeamSession( XUID key, int numPlayers );
	void UnreserveTeamSession( );

	void SetCryptKey( uint64 ullCrypt ) { m_ullCrypt = ullCrypt; }
	uint64 GetCryptKey() const { return m_ullCrypt; }

public:
	virtual void DebugPrint();

protected:
	virtual void ReceiveMessage( KeyValues *msg, bool bValidatedLobbyMember, XUID xuidSrc ) OVERRIDE;
	virtual void OnPlayerLeave( XUID xuid );

protected:

	bool Process_RequestJoinData( XUID xuidClient, KeyValues *pSettings );
	void Process_TeamReservation( XUID key, int teamSize );
	void Process_VoiceStatus( KeyValues *msg, XUID xuidSrc );
	void Process_VoiceMutelist( KeyValues *msg );

	void UpdateStateInit();
	
	void InitSessionProperties();
	void UpdateSessionProperties( KeyValues *kv );	
	
	uint64 m_ullCrypt;
	XUID m_teamResKey;
	int  m_numRemainingTeamPlayers;
	float m_flTeamResStartTime;

#ifdef _X360

	virtual void OnAsyncOperationFinished();

	// IX360NetworkEvents
	virtual void OnX360NetDisconnected( XUID xuidRemote );
	virtual bool OnX360NetConnectionlessPacket( netpacket_t *pkt, KeyValues *msg );

	void DestroyAfterMigrationFinished();

#elif !defined( NO_STEAM )

	CCallResult< CSysSessionHost, LobbyCreated_t > m_CallbackOnLobbyCreated;
	void Steam_OnLobbyCreated( LobbyCreated_t *p, bool bError );

	STEAM_CALLBACK_MANUAL( CSysSessionHost, Steam_OnLobbyEntered, LobbyEnter_t, m_CallbackOnLobbyEntered );

	bool GetLobbyType( KeyValues *kv, ELobbyType *peType, bool *pbJoinable );
	double m_dblDormantMembersCheckTime;
	int m_numDormantMembersDetected;

#endif

	enum State_t
	{
		STATE_INIT,
		STATE_CREATING,
		STATE_IDLE,
		STATE_FAIL,
		STATE_MIGRATE,
#ifdef _X360
		STATE_ALLOWING_MIGRATE,
		STATE_DELETE,
#endif
		STATE_UNDEFINED
	};

	State_t m_eState;
	float m_flTimeOperationStarted;
	float m_flInitializeTimestamp;

	CUtlMap< XUID, double, int, CDefLess< XUID > > m_mapKickedPlayers;
};

class CSysSessionClient : public CSysSessionBase
{
public:
	explicit CSysSessionClient( KeyValues *pSettings );
	explicit CSysSessionClient( CSysSessionHost *pHost, KeyValues *pSettings );
	virtual ~CSysSessionClient();

public:
	virtual bool Update();
	virtual void Destroy();

	virtual XUID GetHostXuid( XUID xuidValidResult = 0ull );

#ifdef _X360
	char const * GetHostNetworkAddress( XSESSION_INFO &xsi );
#endif

	void Migrate( KeyValues *pCommand );

public:
	virtual void DebugPrint();

protected:
	virtual void ReceiveMessage( KeyValues *msg, bool bValidatedLobbyMember, XUID xuidSrc ) OVERRIDE;
	virtual void OnPlayerLeave( XUID xuid );

protected:
	void Send_RequestJoinData();

	void Process_ReplyJoinData_Our( KeyValues *msg );
	void Process_ReplyJoinData_Other( KeyValues *msg );
	void Process_OnPlayerUpdated( KeyValues *msg );
	void Process_OnMachineUpdated( KeyValues *msg );
	void Process_Kicked( KeyValues *msg );

protected:
	void UpdateStateInit();
		
	void InitSessionProperties( KeyValues *pSettings );
	void UpdateSessionProperties( KeyValues *kv );

#ifdef _X360

	virtual void OnAsyncOperationFinished();
	virtual void XP2P_Interconnect();

	// IX360NetworkEvents
	virtual void OnX360NetDisconnected( XUID xuidRemote );
	virtual bool OnX360NetConnectionlessPacket( netpacket_t *pkt, KeyValues *msg );

	XNADDR m_xnaddrLocal;

#elif !defined( NO_STEAM )

	STEAM_CALLBACK_MANUAL( CSysSessionClient, Steam_OnLobbyEntered, LobbyEnter_t, m_CallbackOnLobbyEntered );

#endif

	struct RequestJoinDataInfo_t
	{
		XUID m_xuidLeader;	// XUID of the leader who must reply
		float m_fTimeSent;	// Time when request was sent
	} m_RequestJoinDataInfo;

	enum State_t
	{
		STATE_INIT,
#if !defined ( NO_STEAM )
        STATE_JOIN_LOBBY,
#endif
		STATE_CREATING,
		STATE_REQUESTING_JOIN_DATA,
		STATE_IDLE,
		STATE_FAIL,
		STATE_MIGRATE,
#ifdef _X360
		STATE_DELETE,
#endif
		STATE_UNDEFINED
	};

	State_t m_eState;

	float m_flInitializeTimestamp;
};

class CSysSessionConTeamHost : public CSysSessionBase
{
public:
	explicit CSysSessionConTeamHost( KeyValues *pSettings );
	virtual ~CSysSessionConTeamHost();

	virtual bool Update();
	virtual void Destroy();

	// Once reservation is successful call this function to know which side
	// each player should be assigned to
	// In keyvalues, the convention is that team CT = 1, team T = 2
	bool GetPlayerSidesAssignment( int *numPlayers, uint64 playerIDs[10], int side[10] );
	
protected:
	virtual void ReceiveMessage( KeyValues *msg, bool bValidatedLobbyMember, XUID xuidSrc ) OVERRIDE;
	void SendReservationRequest();
	void Succeeded();
	void Failed();
	XUID GetHostXuid( XUID xuidValidResult );

	enum State_t
	{
		STATE_INIT,
		STATE_WAITING_LOBBY_JOIN,
		STATE_SEND_RESERVATION_REQUEST,
		STATE_WAITING_RESERVATION_REQUEST,
		STATE_DONE,
		STATE_DELETE,
	};

	State_t m_eState;	

	float m_lastRequestSendTime;
	
public:

#ifdef _X360

	XSESSION_INFO m_sessionInfo;

	virtual bool OnX360NetConnectionlessPacket( netpacket_t *pkt, KeyValues *msg );
	virtual void OnAsyncOperationFinished();
	IN_ADDR m_inaddr;

#elif !defined( NO_STEAM )

	STEAM_CALLBACK_MANUAL( CSysSessionConTeamHost, Steam_OnLobbyEntered, LobbyEnter_t, m_CallbackOnLobbyEntered );

#endif
};


#ifdef _X360
void SysSession360_UpdatePending();
#endif

#endif
