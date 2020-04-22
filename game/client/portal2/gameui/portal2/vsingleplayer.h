//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VSINGLEPLAYER_H__
#define __VSINGLEPLAYER_H__

#include "basemodui.h"

namespace BaseModUI {

class CSinglePlayer : public CBaseModFrame, public IBaseModFrameListener, public IMatchEventsSink
{
	DECLARE_CLASS_SIMPLE( CSinglePlayer, CBaseModFrame );

public:
	CSinglePlayer( vgui::Panel *pParent, const char *pPanelName );
	~CSinglePlayer();

	// IMatchEventSink implementation
	virtual void OnEvent( KeyValues *pEvent );

	static void ConfirmCommentary_Callback();
	static void ConfirmChallengeMode_Callback();
	void OpenCommentaryDialog();
	void OpenChallengeDialog();

	MESSAGE_FUNC( MsgPS3AsyncOperationComplete, "MsgPS3AsyncOperationComplete" );
	MESSAGE_FUNC( MsgPS3AsyncOperationFailure, "MsgPS3AsyncOperationFailure" );

protected:
	virtual void OnCommand( char const *szCommand );
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void Activate();
	virtual void RunFrame();
	virtual void SetDataSettings( KeyValues *pSettings );

private:
	void	UpdateFooter();
	void	SetupConditions();

	void	LoadSaveGameFromContainerSuccess();
	void	LoadSaveGameFromContainer( const char *pMapName, const char *pFilename );

	void	CheckForAnySaves();

	bool	m_bFullySetup;
	bool	m_bHasStorageDevice;
	bool	m_bHasAnySaveGame;

	bool	m_bWaitingToLoadFromContainer;
	bool	m_bLoadingFromContainer;

	CUtlString	m_LoadFilename;
	CUtlString	m_MapName;

#ifdef _PS3
	// This variable has to stay alive regardless of
	// the life scope of the window since save operations
	// will write to it later
	static CPS3SaveRestoreAsyncStatus	m_PS3SaveRestoreAsyncStatus;
#endif
};

};

#endif
