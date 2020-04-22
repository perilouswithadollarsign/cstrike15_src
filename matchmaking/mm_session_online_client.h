//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef MM_SESSION_ONLINE_CLIENT_H
#define MM_SESSION_ONLINE_CLIENT_H
#ifdef _WIN32
#pragma once
#endif


//
// CMatchSessionOnlineClient
//
// Implementation of an online session of a client machine
//

class CMatchSessionOnlineClient : public IMatchSessionInternal
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

	void Init();

public:
	explicit CMatchSessionOnlineClient( KeyValues *pSettings );
	explicit CMatchSessionOnlineClient( CSysSessionClient *pSysSession, KeyValues *pSettings );
	explicit CMatchSessionOnlineClient( CSysSessionHost *pSysSession, KeyValues *pExtendedSettings );
	~CMatchSessionOnlineClient();

public:
	void OnClientFullyConnectedToSession();
	void OnEndGameToLobby();

protected:
	void InitializeGameSettings();

	void OnRunCommand( KeyValues *pCommand );
	void OnRunCommand_QueueConnect( KeyValues *pCommand );
	void ConnectGameServer();

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
		STATE_GAME,
		STATE_ENDING,
		STATE_MIGRATE
	};
	State_t m_eState;

	CSysSessionClient *m_pSysSession;
};

#endif
