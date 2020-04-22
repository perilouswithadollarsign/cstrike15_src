//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef MM_SESSION_ONLINE_HOST_H
#define MM_SESSION_ONLINE_HOST_H
#ifdef _WIN32
#pragma once
#endif

class CMatchSessionOnlineTeamSearch;
class CMatchSessionOnlineSearch;

//
// CMatchSessionOnlineHost
//
// Implementation of an online session of a host machine
//

class CMatchSessionOnlineHost : public IMatchSessionInternal
{
	// Methods of IMatchSession
public:
	// Get an internal pointer to session system-specific data
	virtual KeyValues * GetSessionSystemData();
	
	// Get an internal pointer to session settings
	virtual KeyValues * GetSessionSettings();

	// Update session settings, only changing keys and values need
	// to be passed and they will be updated
	virtual void UpdateSessionSettings( KeyValues *pSettings );

	virtual void UpdateTeamProperties( KeyValues *pTeamProperties );

	// Issue a session command
	virtual void Command( KeyValues *pCommand );

	virtual uint64 GetSessionID();

	// Run a frame update
	virtual void Update();

	// Destroy the session object
	virtual void Destroy();

	// Debug print a session object
	virtual void DebugPrint();

	// Check if another session is joinable
	virtual bool IsAnotherSessionJoinable( char const *pszAnotherSessionInfo );

	// Process event
	virtual void OnEvent( KeyValues *pEvent );

public:
	explicit CMatchSessionOnlineHost( KeyValues *pSettings );
	explicit CMatchSessionOnlineHost( CSysSessionClient *pSysSession, KeyValues *pExtendedSettings );
	~CMatchSessionOnlineHost();

protected:
	void InitializeGameSettings();
	void MigrateGameSettings();

	void OnRunCommand( KeyValues *pCommand );
	void OnRunCommand_Start();
	void OnRunCommand_StartDsSearchFinished();
	void OnRunCommand_StartListenServerStarted( uint32 externalIP );
	void OnRunCommand_Cancel_DsSearch();
	void OnRunCommand_Match();
	void OnRunCommand_QueueConnect( KeyValues *pCommand );
	void OnRunCommand_Cancel_Match();
	void ConnectGameServer( CDsSearcher::DsResult_t *pDsResult );
	void StartListenServerMap();
	void OnEndGameToLobby();
	void SetSessionActiveGameplayState( bool bActive, char const *szSecureServerAddress );
	void InviteTeam();

	//
	// Overrides
	//
protected:
	virtual void OnGamePrepareLobbyForGame();
	virtual void OnGamePlayerMachinesConnected( int numMachines );

protected:
	KeyValues *m_pSettings;
	KeyValues::AutoDelete m_autodelete_pSettings;

	KeyValues *m_pSysData;
	KeyValues::AutoDelete m_autodelete_pSysData;

	enum State_t
	{
		STATE_INIT,
		STATE_CREATING,
		STATE_LOBBY,
		STATE_MATCHING,		// host is running matchmaking for opponents
		STATE_MATCHINGRESTART,	// team communication failed and matching needs to restart
		STATE_STARTING,		// host is running a dedicated search or other things before actual game server kicks in
		STATE_LOADING,		// host is starting a server map
		STATE_GAME,
		STATE_ENDING,
		STATE_MIGRATE
	};
	State_t m_eState;

	CSysSessionHost *m_pSysSession;
	CDsSearcher *m_pDsSearcher;
	CMatchSessionOnlineTeamSearch *m_pTeamSearcher;
	CMatchSessionOnlineSearch *m_pMatchSearcher;
};

#endif
