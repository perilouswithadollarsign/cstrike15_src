//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef MM_SESSION_OFFLINE_CUSTOM_H
#define MM_SESSION_OFFLINE_CUSTOM_H
#ifdef _WIN32
#pragma once
#endif


//
// CMatchSessionOfflineCustom
//
// Implementation of an offline session
// that allows customization before the actual
// game commences (like playing commentary mode
// or playing single-player)
//

class CMatchSessionOfflineCustom : public IMatchSessionInternal
{
	// Methods of IMatchSession
public:
	// Get an internal pointer to session system-specific data
	virtual KeyValues * GetSessionSystemData() { return NULL; }
	
	// Get an internal pointer to session settings
	virtual KeyValues * GetSessionSettings();

	// Update session settings, only changing keys and values need
	// to be passed and they will be updated
	virtual void UpdateSessionSettings( KeyValues *pSettings );

	virtual void UpdateTeamProperties( KeyValues *pTeamProperties );

	virtual uint64 GetSessionID();

	// Issue a session command
	virtual void Command( KeyValues *pCommand );

	// Run a frame update
	virtual void Update();

	// Destroy the session object
	virtual void Destroy();

	// Debug print a session object
	virtual void DebugPrint();

	// Check if another session is joinable
	virtual bool IsAnotherSessionJoinable( char const *pszAnotherSessionInfo ) { return true; }

	// Process event
	virtual void OnEvent( KeyValues *pEvent );

public:
	explicit CMatchSessionOfflineCustom( KeyValues *pSettings );
	~CMatchSessionOfflineCustom();

protected:
	void InitializeGameSettings();

	//
	// Overrides
	//
protected:
	void OnGamePrepareLobbyForGame();

protected:
	KeyValues *m_pSettings;
	KeyValues::AutoDelete m_autodelete_pSettings;

	enum State_t
	{
		STATE_INIT,
		STATE_CONFIG,
		STATE_RUNNING
	};
	State_t m_eState;
	bool m_bExpectingServerReload;
};

#endif
