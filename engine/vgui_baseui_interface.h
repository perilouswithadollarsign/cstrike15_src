//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Defines the interface that the GameUI dll exports
//
// $NoKeywords: $
//===========================================================================//

#ifndef VGUI_BASEUI_INTERFACE_H
#define VGUI_BASEUI_INTERFACE_H

#ifdef _WIN32
#pragma once
#endif

#include "ienginevgui.h"
#include "inputsystem/ButtonCode.h"

#if !defined( _X360 )
#include "xbox/xboxstubs.h"
#endif

//-----------------------------------------------------------------------------
// Foward declarations
//-----------------------------------------------------------------------------
class IMatSystemSurface;
class Color;
class IGameUI;
class KeyValues;
struct InputEvent_t;
FORWARD_DECLARE_HANDLE( InputContextHandle_t );

IGameUI* GetGameUI( void );


//-----------------------------------------------------------------------------
// Global singleton interfaces related to VGUI 
//-----------------------------------------------------------------------------

// enumeration of level loading progress bar spots
enum LevelLoadingProgress_e
{
	PROGRESS_INVALID = -2,
	PROGRESS_DEFAULT = -1,

	PROGRESS_NONE,
	PROGRESS_CHANGELEVEL,
	PROGRESS_SPAWNSERVER,
	PROGRESS_LOADWORLDMODEL,
	PROGRESS_CRCMAP,
	PROGRESS_CRCCLIENTDLL,
	PROGRESS_CREATENETWORKSTRINGTABLES,
	PROGRESS_PRECACHEWORLD,
	PROGRESS_CLEARWORLD,
	PROGRESS_LEVELINIT,
	PROGRESS_PRECACHE,
	PROGRESS_ACTIVATESERVER,
	PROGRESS_BEGINCONNECT,
	PROGRESS_SIGNONCHALLENGE,
	PROGRESS_SIGNONCONNECT,
	PROGRESS_SIGNONCONNECTED,
	PROGRESS_PROCESSSERVERINFO,
	PROGRESS_PROCESSSTRINGTABLE,
	PROGRESS_SIGNONNEW,
	PROGRESS_SENDCLIENTINFO,
	PROGRESS_SENDSIGNONDATA,
	PROGRESS_SIGNONSPAWN,
	PROGRESS_CREATEENTITIES,
	PROGRESS_FULLYCONNECTED,
	PROGRESS_PRECACHELIGHTING,
	PROGRESS_READYTOPLAY,
	PROGRESS_HIGHESTITEM,	// must be last item in list
};



//-----------------------------------------------------------------------------
// Purpose: Centerpoint for handling all user interface in the engine
//-----------------------------------------------------------------------------
abstract_class IEngineVGuiInternal : public IEngineVGui
{
public:

	virtual void Init() = 0;
	virtual void Connect() = 0;
	virtual void Shutdown() = 0;
	virtual bool SetVGUIDirectories() = 0;
	virtual bool IsInitialized() const = 0;
	virtual CreateInterfaceFn GetGameUIFactory() = 0;
	virtual bool Key_Event( const InputEvent_t &event ) = 0;
	virtual void BackwardCompatibility_Paint() = 0;
	virtual void UpdateButtonState( const InputEvent_t &event ) = 0;
	virtual void PostInit() = 0;

    virtual void Paint( PaintMode_t mode ) = 0;

	// handlers for game UI (main menu)
	virtual void ActivateGameUI() = 0;
	virtual bool HideGameUI() = 0;
	virtual bool IsGameUIVisible() = 0;

	// console
	virtual void ShowConsole() = 0;
	virtual void HideConsole() = 0;
	virtual bool IsConsoleVisible() = 0;
	virtual void ClearConsole() = 0;

	virtual void HideDebugSystem() = 0;

	// level loading
	virtual void OnLevelLoadingStarted( const char *levelName, bool bLocalServer ) = 0;
	virtual void OnLevelLoadingFinished() = 0;
	virtual void NotifyOfServerConnect(const char *game, int IP, int connectionPort, int queryPort) = 0;
	virtual void NotifyOfServerDisconnect() = 0;
	virtual void EnabledProgressBarForNextLoad() = 0;
	virtual void UpdateProgressBar(LevelLoadingProgress_e progress, bool showDialog = true ) = 0;
	virtual void UpdateCustomProgressBar( float progress, const wchar_t *desc ) = 0;
	virtual void StartCustomProgress() = 0;
	virtual void FinishCustomProgress() = 0;
	virtual void UpdateSecondaryProgressBarWithFile( float progress, const char *pDesc, int nBytesTotal ) = 0;
	virtual void UpdateSecondaryProgressBar( float progress, const wchar_t *desc ) = 0;
	virtual void ShowErrorMessage() = 0;
	virtual void HideLoadingPlaque() = 0;
	virtual void StartLoadingScreenForCommand( const char* command ) = 0;
	virtual void StartLoadingScreenForKeyValues( KeyValues* keyValues ) = 0;

	// Should pause?
	virtual bool ShouldPause() = 0;
	virtual void SetGameDLLPanelsVisible( bool show ) = 0;
	// Allows the level loading progress to show map-specific info
	virtual void SetProgressLevelName( const char *levelName ) = 0;

	virtual void Simulate() = 0;

	virtual void SetNotAllowedToHideGameUI( bool bNotAllowedToHide ) = 0;
	virtual void SetNotAllowedToShowGameUI( bool bNotAllowedToShow ) = 0;

	virtual void NeedConnectionProblemWaitScreen() = 0;
	virtual void ShowPasswordUI( char const *pchCurrentPW ) = 0;
	virtual void OnToolModeChanged( bool bGameMode ) = 0;

	virtual InputContextHandle_t GetGameUIInputContext() = 0;

	virtual bool IsPlayingFullScreenVideo() = 0;
};

// Purpose: singleton accessor
#ifndef DEDICATED
extern IEngineVGuiInternal *EngineVGui();
#endif


#endif // VGUI_BASEUI_INTERFACE_H
