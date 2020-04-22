//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IGAMEUI_H
#define IGAMEUI_H
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"
#include "vgui/IPanel.h"

#if !defined( _X360 )
#include "xbox/xboxstubs.h"
#endif

class CCommand;

// reasons why the user can't connect to a game server
enum ESteamLoginFailure
{
	STEAMLOGINFAILURE_NONE,
	STEAMLOGINFAILURE_BADTICKET,
	STEAMLOGINFAILURE_NOSTEAMLOGIN,
	STEAMLOGINFAILURE_VACBANNED,
	STEAMLOGINFAILURE_LOGGED_IN_ELSEWHERE
};

enum ESystemNotify
{
	SYSTEMNOTIFY_STORAGEDEVICES_CHANGED,
	SYSTEMNOTIFY_USER_SIGNEDIN,
	SYSTEMNOTIFY_USER_SIGNEDOUT,
	SYSTEMNOTIFY_XLIVE_LOGON_ESTABLISHED,      // we are logged into live service
	SYSTEMNOTIFY_XLIVE_LOGON_CLOSED,		   // no longer logged into live - either from natural (signed out) or unnatural (e.g. severed net connection) causes	
	SYSTEMNOTIFY_XUIOPENING,
	SYSTEMNOTIFY_XUICLOSED,
	SYSTEMNOTIFY_INVITE_SHUTDOWN,				// Cross-game invite is causing us to shutdown
	SYSTEMNOTIFY_MUTECHANGED,					// Player changed mute settings
	SYSTEMNOTIFY_INPUTDEVICESCHANGED,			// Input device has changed (used for controller disconnection)
	SYSTEMNOTIFY_PROFILE_UNAVAILABLE,			// Profile failed to read or write
};

// these are used to show the modal message box on different slots
enum ECommandMsgBoxSlot
{
	CMB_SLOT_FULL_SCREEN = -1,
	CMB_SLOT_PLAYER_0,
	CMB_SLOT_PLAYER_1,
};

//-----------------------------------------------------------------------------
// Purpose: contains all the functions that the GameUI dll exports
//-----------------------------------------------------------------------------
abstract_class IGameUI 
{
public:
	// initialization/shutdown
	virtual void Initialize( CreateInterfaceFn appFactory ) = 0;
	virtual void PostInit() = 0;

	// connect to other interfaces at the same level (gameui.dll/server.dll/client.dll)
	virtual void Connect( CreateInterfaceFn gameFactory ) = 0;

	virtual void Start() = 0;
	virtual void Shutdown() = 0;
	virtual void RunFrame() = 0;

	// notifications
	virtual void OnGameUIActivated() = 0;
	virtual void OnGameUIHidden() = 0;
	
	// OLD: Use OnConnectToServer2
	virtual void OLD_OnConnectToServer(const char *game, int IP, int port) = 0; 
	
	virtual void OnDisconnectFromServer_OLD( uint8 eSteamLoginFailure, const char *username ) = 0;
	virtual void OnLevelLoadingStarted( const char *levelName, bool bShowProgressDialog ) = 0;
	virtual void OnLevelLoadingFinished(bool bError, const char *failureReason, const char *extendedReason) = 0;
	virtual void StartLoadingScreenForCommand( const char* command ) = 0;
	virtual void StartLoadingScreenForKeyValues( KeyValues* keyValues ) = 0;

	// level loading progress, returns true if the screen needs updating
	virtual bool UpdateProgressBar(float progress, const char *statusText, bool showDialog = true ) = 0;
	// Shows progress desc, returns previous setting... (used with custom progress bars )
	virtual bool SetShowProgressText( bool show ) = 0;
	virtual bool UpdateSecondaryProgressBar(float progress, const wchar_t *desc ) = 0;

	// !!!!!!!!!members added after "GameUI011" initial release!!!!!!!!!!!!!!!!!!!
	// Allows the level loading progress to show map-specific info
	virtual void SetProgressLevelName( const char *levelName ) = 0;
	
	// Xbox 360
	virtual void ShowMessageDialog( const uint nType, vgui::Panel *pOwner ) = 0;
	virtual void ShowMessageDialog( const char* messageID, const char* titleID ) = 0;

	virtual void CreateCommandMsgBox( const char* pszTitle, const char* pszMessage, bool showOk = true, bool showCancel = false, const char* okCommand = NULL, const char* cancelCommand = NULL, const char* closedCommand = NULL, const char* pszLegend = NULL ) = 0;
	virtual void CreateCommandMsgBoxInSlot( ECommandMsgBoxSlot slot, const char* pszTitle, const char* pszMessage, bool showOk = true, bool showCancel = false, const char* okCommand = NULL, const char* cancelCommand = NULL, const char* closedCommand = NULL, const char* pszLegend = NULL ) = 0;

	// inserts specified panel as background for level load dialog
	virtual void SetLoadingBackgroundDialog( vgui::VPANEL panel ) = 0;

	virtual void OnConnectToServer2(const char *game, int IP, int connectionPort, int queryPort) = 0;

	virtual void SetProgressOnStart() = 0;
	virtual void OnDisconnectFromServer( uint8 eSteamLoginFailure ) = 0;
/*
	virtual void OnConfirmQuit( void ) = 0;

	virtual bool IsMainMenuVisible( void ) = 0;

	// Client DLL is providing us with a panel that it wants to replace the main menu with
	virtual void SetMainMenuOverride( vgui::VPANEL panel ) = 0;
	
	// Client DLL is telling us that a main menu command was issued, probably from its custom main menu panel
	virtual void SendMainMenuCommand( const char *pszCommand ) = 0;
*/	
	virtual void NeedConnectionProblemWaitScreen() = 0;
	virtual void ShowPasswordUI( char const *pchCurrentPW ) = 0;

#if defined( _X360 ) && defined( _DEMO )
	virtual void OnDemoTimeout( void ) = 0;
#endif

	virtual bool LoadingProgressWantsIsolatedRender( bool bContextValid ) = 0;

	virtual bool IsPlayingFullScreenVideo() = 0;

	virtual bool IsTransitionEffectEnabled() = 0;

	virtual bool IsInLevel() = 0;

	virtual void RestoreTopLevelMenu() = 0;
};

#define GAMEUI_INTERFACE_VERSION "GameUI011"

#endif // IGAMEUI_H
