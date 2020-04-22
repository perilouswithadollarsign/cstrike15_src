//========= Copyright © 1996-2010, Valve Corporation, All rights reserved. ============//
//
//
//=============================================================================//

#ifndef __VSAVELOADGAMEDIALOG_H__
#define __VSAVELOADGAMEDIALOG_H__

#include "basemodui.h"
#include "filesystem.h"

#ifdef _PS3
enum PS3AsyncOperation_e
{
	PS3_ASYNC_OP_NONE = 0,
	PS3_ASYNC_OP_READ_SAVE,
	PS3_ASYNC_OP_WRITE_SAVE,
	PS3_ASYNC_OP_READ_SCREENSHOT,
	PS3_ASYNC_OP_DELETE_SAVE,
};

class CPS3AsyncStatus : public CPS3SaveRestoreAsyncStatus
{
public:
	CPS3AsyncStatus()
	{
		Reset();	
	}

	void StartOperation( PS3AsyncOperation_e nOperation )
	{
		switch ( nOperation )
		{
		case PS3_ASYNC_OP_READ_SAVE:
			m_nCurrentOperationTag = kSAVE_TAG_READ_SAVE;
			break;
		case PS3_ASYNC_OP_WRITE_SAVE:
			m_nCurrentOperationTag = kSAVE_TAG_WRITE_SAVE;
			break;
		case PS3_ASYNC_OP_READ_SCREENSHOT:
			m_nCurrentOperationTag = kSAVE_TAG_READ_SCREENSHOT;
			break;
		case PS3_ASYNC_OP_DELETE_SAVE:
			m_nCurrentOperationTag = kSAVE_TAG_DELETE_SAVE;
			break;
		default:
			m_nCurrentOperationTag = kSAVE_TAG_UNKNOWN;
			break;
		}

		m_AsyncOperation = nOperation;
		m_bAsyncOperationComplete = false;
		m_bAsyncOperationActive = true;
	}

	bool IsOperationActive()
	{
		return m_bAsyncOperationActive;
	}

	bool IsOperationComplete()
	{
		// status processing may still be in progress 
		return m_bAsyncOperationComplete;
	}

	bool IsOperationPending()
	{
		return m_bPendingOperationActive;
	}

	void Reset()
	{
		m_AsyncOperation = PS3_ASYNC_OP_NONE;
		m_bAsyncOperationActive = false;
		m_bAsyncOperationComplete = true;

		m_PendingOperation = PS3_ASYNC_OP_NONE;
		m_bPendingOperationActive = false;
	}

	PS3AsyncOperation_e	m_AsyncOperation;
	bool				m_bAsyncOperationActive;
	bool				m_bAsyncOperationComplete;

	PS3AsyncOperation_e	m_PendingOperation;
	bool				m_bPendingOperationActive;
};
#endif

namespace BaseModUI {

class GenericPanelList;
class SaveGameInfoLabel;
class SaveGameItem;

class SaveLoadGameDialog : public CBaseModFrame, public IBaseModFrameListener, public IMatchEventsSink
{
	DECLARE_CLASS_SIMPLE( SaveLoadGameDialog, CBaseModFrame );

public:
	SaveLoadGameDialog( vgui::Panel *pParent, const char *pPanelName, bool bIsSaveDialog );
	~SaveLoadGameDialog();

	// IMatchEventSink implementation
	virtual void OnEvent( KeyValues *pEvent );

	MESSAGE_FUNC_CHARPTR( OnItemSelected, "OnItemSelected", pPanelName );
	MESSAGE_FUNC( DeviceChangeCompleted, "DeviceChangeCompleted" );
	MESSAGE_FUNC( MsgSaveFailure, "MsgSaveFailure" );
	MESSAGE_FUNC( MsgReturnToGame, "MsgReturnToGame" );
	MESSAGE_FUNC( MsgDeleteCompleted, "MsgDeleteCompleted" );

	MESSAGE_FUNC( MsgPS3AsyncSystemReady, "MsgPS3AsyncSystemReady" );
	MESSAGE_FUNC( MsgPS3AsyncOperationComplete, "MsgPS3AsyncOperationComplete" );
	MESSAGE_FUNC( MsgPS3AsyncOperationFailure, "MsgPS3AsyncOperationFailure" );

	const SaveGameInfo_t *GetSaveGameInfo( int nSaveGameIndex );

	bool IsSaveGameDialog() { return m_bIsSaveGameDialog; }
	bool HasStorageDevice() { return m_bHasStorageDevice; }

	void RequestDeleteSaveGame( int nSaveGameIndex );
	void ConfirmDeleteSaveGame();

	void RequestOverwriteSaveGame( int nSaveGameIndex );
	void ConfirmOverwriteSaveGame();

	void ScreenshotLoaded( const FileAsyncRequest_t &asyncRequest, int numReadBytes, FSAsyncStatus_t err );
	void LoadSaveGameFromContainer( const char *pMapName, const char *pFilename );

	bool IsInputEnabled() { return m_bInputEnabled; }
	void EnableInput( bool bEnable ) { m_bInputEnabled = bEnable; }

	void HandleYbutton();

protected:
	virtual void OnCommand( char const *pCommand );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Activate();
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void PaintBackground();
	virtual void RunFrame();

private:
	void	UpdateFooter();
	void	SetSaveGameImage( int nChapter );
	void	SortSaveGames();
	void	PopulateSaveGameList();
	void	Reset();
	void	DrawSaveGameImage();
	bool	StartAsyncScreenshotLoad( const char *pScreenshotFilename );

	void	DeleteAndCommit();
	void	DeleteSuccess();

	void	WriteSaveGameToContainer();
	void	WriteSaveGameToContainerSuccess();

	void	LoadSaveGameFromContainerSuccess();

	bool	LoadScreenshotFromContainer( const char *pScreenshotFilename );
	void	LoadScreenshotFromContainerSuccess();

#ifdef _PS3
	void	StartPS3Operation( PS3AsyncOperation_e nOperation );
#endif

	static void ConfirmDeleteSaveGame_Callback();
	static void ConfirmOverwriteSaveGame_Callback();
	static void ConfirmSaveFailure_Callback();

	bool				m_bInputEnabled;

	bool				m_bIsSaveGameDialog;
	bool				m_bHasStorageDevice;
	bool				m_bSteamCloudResetRequested;

	bool				m_bSaveInProgress;
	bool				m_bSaveStarted;

	GenericPanelList	*m_pSaveGameList;
	vgui::ImagePanel	*m_pSaveGameImage;
	SaveGameInfoLabel	*m_pSaveGameInfoLabel;
	vgui::ImagePanel	*m_pWorkingAnim;
	vgui::Label			*m_pAutoSaveLabel;
	vgui::ImagePanel	*m_pCloudSaveLabel;

	CUtlVector< SaveGameInfo_t > m_SaveGameInfos;

	int					m_nSaveGameToDelete;
	int					m_nSaveGameToOverwrite;

	int					m_nSaveGameScreenshotId;
	int					m_nNewSaveGameImageId;
	int					m_nNoSaveGameImageId;
	int					m_nVignetteImageId;

	int					m_nSaveGameImageId;

	CUtlVector< int >	m_ChapterImages;

	FSAsyncControl_t	m_hAsyncControl;

	float				m_flTransitionStartTime;
	float				m_flNextLoadScreenshotTime;

	CUtlString			m_DeleteFilename;
	CUtlString			m_SaveFilename;
	CUtlString			m_SaveComment;
	CUtlString			m_ScreenshotFilename;
	CUtlString			m_LoadFilename;
	CUtlString			m_MapName;
	CUtlString			m_CurrentlySelectedItemInternalIDName;

#ifdef _PS3
	// This variable has to stay alive regardless of
	// the life scope of the window since save operations
	// will write to it later
	static CPS3AsyncStatus		m_PS3AsyncStatus;
#endif
};

char *RenamePS3SaveGameFile( const char *pOriginalFilename, char *pNewFilename, int nNewFilenameSize );

};

#endif
