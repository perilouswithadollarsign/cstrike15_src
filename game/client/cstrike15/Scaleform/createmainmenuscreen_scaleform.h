//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: [jason] Creates the Main Menu Screen in Scaleform.
//
// $NoKeywords: $
//=============================================================================//
#if defined( INCLUDE_SCALEFORM )

#ifndef CREATEMAINMENUSCREEN_SCALEFORM_H
#define CREATEMAINMENUSCREEN_SCALEFORM_H
#ifdef _WIN32
#pragma once
#endif

#include "matchmaking/imatchframework.h"
#include "scaleformui/scaleformui.h"
#include "messagebox_scaleform.h"

class CCreateMainMenuScreenScaleform : public ScaleformFlashInterface, public IMessageBoxEventCallback, public IShaderDeviceDependentObject, public IMatchEventsSink
{
protected:
	static CCreateMainMenuScreenScaleform* m_pInstance;

	CCreateMainMenuScreenScaleform( );

public:
	static void LoadDialog( void );
	static void UnloadDialog( void );
	static void RestorePanel( void );
	static void ShowPanel( bool bShow, bool immediate = false );
	static bool IsActive() { return m_pInstance != NULL; }
	static void UpdateDialog( void );
	static CCreateMainMenuScreenScaleform* GetInstance() { return m_pInstance; }

	virtual bool OnMessageBoxEvent( MessageBoxFlags_t buttonPressed );

	virtual void OnEvent( KeyValues *pEvent );

	// Called to trigger commands on the BasePanel, like opening other dialogs, configuring options, etc.
	//  See the CBaseModPanel::RunMenuCommand function for specific available commands.
	void BasePanelRunCommand( SCALEFORM_CALLBACK_ARGS_DECL );

	// Determines whether we can run any multiplayer options on the menu
	void IsMultiplayerPrivilegeEnabled( SCALEFORM_CALLBACK_ARGS_DECL );

	void LaunchTraining( SCALEFORM_CALLBACK_ARGS_DECL );

	void ViewMapInWorkshop( SCALEFORM_CALLBACK_ARGS_DECL );

	void GetPreviousLevel( SCALEFORM_CALLBACK_ARGS_DECL );

	// IShaderDeviceDependentObject methods
	virtual void DeviceLost( void );
	virtual void DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd );
	virtual void ScreenSizeChanged( int width, int height ) { }

	void Tick(); // per-frame triggered from basepanel::RunFrame

	void PerformKeyRebindings( void );

protected:
	virtual void FlashReady( void );
	virtual void PostUnloadFlash( void );
	virtual void FlashLoaded( void );

	void Show( void );
	void Hide( void );
	void HideImmediate( void );

	void InnerRestorePanel( void );

	void DoLaunchTraining( void );

protected:
	CMessageBoxScaleform* m_pConfirmDialog;
	bool	m_bVisible;
	bool	m_bHideOnLoad;
	bool	m_bTrainingRequested;
	uint32	m_uiClientHelloRequestedTimestampMS;
	int		m_iPreviousPlayerLevel;
};

#endif // CREATEMAINMENUSCREEN_SCALEFORM_H
#endif // include scaleform
