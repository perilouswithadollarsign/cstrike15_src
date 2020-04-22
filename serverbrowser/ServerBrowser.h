//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SERVERBROWSER_H
#define SERVERBROWSER_H
#ifdef _WIN32
#pragma once
#endif

class CServerBrowserDialog;

//-----------------------------------------------------------------------------
// Purpose: Handles the UI and pinging of a half-life game server list
//-----------------------------------------------------------------------------
class CServerBrowser : public IServerBrowser, public IVGuiModule
{
public:
	CServerBrowser();
	~CServerBrowser();

	// IVGui module implementation
	virtual bool Initialize(CreateInterfaceFn *factorylist, int numFactories);
	virtual bool PostInitialize(CreateInterfaceFn *modules, int factoryCount);
	virtual vgui::VPANEL GetPanel();
	virtual bool Activate();
	virtual bool IsValid();
	virtual void Shutdown();
	virtual void Deactivate();
	virtual void Reactivate();
	virtual void SetParent(vgui::VPANEL parent);

	// IServerBrowser implementation
	// joins a specified game - game info dialog will only be opened if the server is fully or passworded
	virtual bool JoinGame( uint32 unGameIP, uint16 usGamePort );
	virtual bool JoinGame( uint64 ulSteamIDFriend );

	// opens a game info dialog to watch the specified server; associated with the friend 'userName'
	virtual bool OpenGameInfoDialog( uint64 ulSteamIDFriend );

	// forces the game info dialog closed
	virtual void CloseGameInfoDialog( uint64 ulSteamIDFriend );

	// closes all the game info dialogs
	virtual void CloseAllGameInfoDialogs();

	// methods
	virtual void CreateDialog();
	virtual void Open();

	// true if the user can't play a game
	bool IsVACBannedFromGame( int nAppID );


private:
	vgui::DHANDLE<CServerBrowserDialog> m_hInternetDlg;
};

// singleton accessor
CServerBrowser &ServerBrowser();

class CSteamAPIContext;
extern CSteamAPIContext *steamapicontext;


#endif // SERVERBROWSER_H
