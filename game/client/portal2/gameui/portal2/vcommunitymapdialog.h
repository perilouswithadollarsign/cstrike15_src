//========= Copyright © 1996-2010, Valve Corporation, All rights reserved. ============//
//
//
//=============================================================================//

#ifndef __VCOMMUNITYMAPDIALOG_H__
#define __VCOMMUNITYMAPDIALOG_H__

#if defined( PORTAL2_PUZZLEMAKER )

#include "basemodui.h"
#include "filesystem.h"
#include "VGenericPanelList.h"
#include "gameui/portal2/vdialoglistbutton.h"
#include "vhybridbutton.h"

#define MINIMUM_VOTE_THRESHOLD	1	// Need at least four votes to bother showing the ratings

namespace BaseModUI {

//=============================================================================
// 
// Custom hybrid bitmap button code
//
//=============================================================================

// We only really want to draw a background color and alter our highlight behavior

class HybridBitmapButton : public BaseModHybridButton
{
	DECLARE_CLASS_SIMPLE( HybridBitmapButton, BaseModHybridButton );

public:
	
	HybridBitmapButton( Panel *parent, const char *panelName, const char *text, Panel *pActionSignalTarget = NULL, const char *pCmd = NULL );
	HybridBitmapButton( Panel *parent, const char *panelName, const wchar_t *text, Panel *pActionSignalTarget = NULL, const char *pCmd = NULL );

	virtual void Paint( void );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

private:
	bool GetHighlightBounds( int &x, int &y, int &w, int &h );
};

//=============================================================================
// 
// Iconic rating image
//
//=============================================================================

#define	RATING_SCALE	5	// "Stars"

class IconRatingItem : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( IconRatingItem, vgui::EditablePanel );

public:
	IconRatingItem( vgui::Panel *pParent, const char *pPanelName );
	~IconRatingItem( void );
	
	void SetRating( float flRating );
	void SetEnabled( bool bEnabled );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout( void );
	virtual void ApplySettings( KeyValues *inResourceData );

private:
	vgui::ImagePanel	*m_pPointIcons[RATING_SCALE];
	float				m_flRating;	// 1...RATING_SCALE
	char				m_szOnIconName[128];
	char				m_szOffIconName[128];
	char				m_szHalfIconName[128];
};

class CommunityMapDialog;

//=============================================================================
// 
// Community map item info label
//
//-----------------------------------------------------------------------------
class CommunityMapInfoLabel : public vgui::Label
{
	DECLARE_CLASS_SIMPLE( CommunityMapInfoLabel, vgui::Label );

public:

	CommunityMapInfoLabel( vgui::Panel *pParent, const char *pPanelName );

	void	SetCommunityMapInfo( PublishedFileId_t nMapID, uint64 unCreatorID );
	uint64	GetOwnerID( void ) const { return m_ulOwnerID; }
	void	Update( void );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PaintBackground();

private:
	int DrawText( int x, int y, const wchar_t *pString, vgui::HFont hFont, Color color );

	CommunityMapDialog *m_pDialog;

	vgui::HFont	m_hAuthorSteamNameFont;

	PublishedFileId_t	m_nCommunityMapIndex;
	uint64				m_ulOwnerID;

	wchar_t				m_AuthorSteamNameString[256];
};


//=============================================================================
// 
// Community map item
//
//-----------------------------------------------------------------------------

class CommunityMapItem : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CommunityMapItem, vgui::EditablePanel );

public:
	CommunityMapItem( vgui::Panel *pParent, const char *pPanelName );

	void SetCommunityMapInfo( PublishedFileId_t nIndex, uint32 timeSubscribed, uint32 timeLastPlayed );
	void SetOwnerID( uint64 nOwnerID );

	bool IsSelected() { return m_bSelected; }
	void SetSelected( bool bSelected ) { m_bSelected = bSelected; }

	bool HasMouseover() { return m_bHasMouseover; }

	void SetHasMouseover( bool bHasMouseover )
	{
		if ( bHasMouseover )
		{
			for ( int i = 0; i < m_pListCtrlr->GetPanelItemCount(); i++ )
			{
				CommunityMapItem *pItem = dynamic_cast< CommunityMapItem* >( m_pListCtrlr->GetPanelItem( i ) );
				if ( pItem && pItem != this )
				{
					pItem->SetHasMouseover( false );
				}
			}
		}
		m_bHasMouseover = bHasMouseover; 
	}

	void	SetTitle( const wchar_t *lpszTitle );

	uint32	GetTimeSubscribed( void ) { return m_timeSubscribed; }
	uint32	GetTimeLastPlayed( void ) { return m_timeLastPlayed; }
	uint64	GetOwnerSteamID( void ) { return m_nOwnerID; }
	PublishedFileId_t	GetCommunityMapIndex( void ) { return m_nCommunityMapIndex; }

	virtual void	OnKeyCodePressed( vgui::KeyCode code );
	bool			OnKeyCodePressed_Queue( vgui::KeyCode code );
	bool			OnKeyCodePressed_History( vgui::KeyCode code );

	void SetDisabled( bool bState ) { m_bDisabled = bState; }

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PaintBackground();
	virtual void OnCursorEntered();
	virtual void OnCursorExited();
	virtual void NavigateTo();
	virtual void NavigateFrom();
	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnMouseDoublePressed( vgui::MouseCode code );
	virtual void PerformLayout();
	virtual void OnMessage( const KeyValues *params, vgui::VPANEL ifromPanel );

private:
	int DrawText( int x, int y, int nLabelTall, const wchar_t *pString, vgui::HFont hFont, Color color );
	void FormatFileTimeString( time_t nFileTime, wchar_t *pOutputBuffer, int nBufferSizeInBytes );
	void ViewMapInWorkshop( void );

	CommunityMapDialog	*m_pDialog;
	GenericPanelList	*m_pListCtrlr;
	PublishedFileId_t	m_nCommunityMapIndex;
	uint64				m_nOwnerID;

	vgui::HFont			m_hTitleFont;

	int					m_nTextOffsetY;

	Color				m_TextColor;
	Color				m_FocusColor;
	Color				m_CursorColor;
	Color				m_MouseOverCursorColor;
	Color				m_DisabledColor;

	bool				m_bSelected;
	bool				m_bHasMouseover;
	bool				m_bDisabled;

	wchar_t				m_TitleString[128];

	uint32				m_timeSubscribed;	// When this user subscribed to the file (used for sorting the queue)
	uint32				m_timeLastPlayed;	// When the user last played this map (used for sorting the history list)
};

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
class CCommunityMapList : public BaseModUI::GenericPanelList
{
	DECLARE_CLASS_SIMPLE( CCommunityMapList, GenericPanelList );
public:
	CCommunityMapList( vgui::Panel *parent, const char *panelName, int selectionModeMask ) : 
	  BaseClass( parent, panelName, selectionModeMask ) 
	  {
		  // ...
	  }

	  CommunityMapItem *GetPanelByMapIndex( PublishedFileId_t nMapIndex )
	  {
		  // 
		  for ( int i = 0; i < GetPanelItemCount(); i++ )
		  {
			  CommunityMapItem *pItem = (CommunityMapItem *) GetPanelItem( i );
			  if ( pItem && pItem->GetCommunityMapIndex() == nMapIndex )
				  return pItem;
		  }

		  return NULL;
	  }
};

class CommunityMapDialog : public CBaseModFrame, public IBaseModFrameListener, public IMatchEventsSink
{
	DECLARE_CLASS_SIMPLE( CommunityMapDialog, CBaseModFrame );

public:
	CommunityMapDialog( vgui::Panel *pParent, const char *pPanelName );
	~CommunityMapDialog();

	// IMatchEventSink implementation
	virtual void OnEvent( KeyValues *pEvent );

	MESSAGE_FUNC_CHARPTR( OnItemSelected, "OnItemSelected", pPanelName );
	MESSAGE_FUNC_CHARPTR( OnItemRemoved, "OnItemRemoved", pPanelName );
	MESSAGE_FUNC_CHARPTR( MapDownloadAborted, "MapDownloadAborted", msg );
	MESSAGE_FUNC( MsgSaveFailure, "MsgSaveFailure" );
	MESSAGE_FUNC( MsgDeleteCompleted, "MsgDeleteCompleted" );

	MESSAGE_FUNC( MsgPS3AsyncSystemReady, "MsgPS3AsyncSystemReady" );
	MESSAGE_FUNC( MsgPS3AsyncOperationComplete, "MsgPS3AsyncOperationComplete" );
	MESSAGE_FUNC( MsgPS3AsyncOperationFailure, "MsgPS3AsyncOperationFailure" );

	// const SaveGameInfo_t *GetSaveGameInfo( int nSaveGameIndex );

	bool HasStorageDevice() { return m_bHasStorageDevice; }

	void RequestDeleteCommunityMap( PublishedFileId_t nSaveGameIndex );
	void ConfirmDeleteCommunityMap( void );

	void RequestOverwriteSaveGame( int nSaveGameIndex );
	void ConfirmOverwriteSaveGame( void );

	void ScreenshotLoaded( const FileAsyncRequest_t &asyncRequest, int numReadBytes, FSAsyncStatus_t err );
	void LoadSaveGameFromContainer( const char *pMapName, const char *pFilename );

	bool IsInputEnabled() { return m_bInputEnabled; }
	void EnableInput( bool bEnable ) { m_bInputEnabled = bEnable; }

	void HandleYbutton( void );

	void BeginLaunchCommunityMap( PublishedFileId_t nCommunityMapID, bool bRecordDemo = false  );
	void BeginLaunchCoopCommunityMap( PublishedFileId_t nCommunityMapID );

	bool IsShowingMapQueue( void ) const { return ( m_pListTypeButton == NULL || m_pListTypeButton->GetCurrentSelectionIndex() == 0 ); }

	static void FixFooter();

protected:
	virtual void	OnCommand( char const *pCommand );
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	Activate( void );
	virtual void	OnKeyCodePressed( vgui::KeyCode code );
	virtual void	PaintBackground( void );
	virtual void	RunFrame( void );

private:

	void	HideMapInfo( void );

	bool	FileInSync( const char *lpszFilename, CSteamID authorID, uint32 timeUpdated );
	void	UpdateAvatarImage( uint64 ulSteamID );
	void	UpdateOrAddMapToList( PublishedFileId_t mapID );
	void	LaunchCommunityMap( PublishedFileId_t nMapID );
	bool	UpdateMapInList( CommunityMapItem *pItem, const PublishedFileInfo_t *pFileInfo );

	void	SortMapList( void );
	void	UpdateFooter( void );
	void	SetSelectedMap( PublishedFileId_t nMapIndex );
	void	PopulateCommunityMapList( void );
	void	PopulateQueueHistoryList( void );
	void	Reset( void );
	void	DrawThumbnailImage( void );
	bool	StartAsyncScreenshotLoad( const char *pScreenshotFilename );

	void	DeleteAndCommit( void );
	void	DeleteSuccess( void );

	void	WriteSaveGameToContainer( void );
	void	WriteSaveGameToContainerSuccess( void );

	void	LoadSaveGameFromContainerSuccess( void );

	bool	LoadThumbnailFromContainer( const char *pScreenshotFilename );
	void	LoadThumbnailFromContainerSuccess( void );

#ifdef _PS3
	void	StartPS3Operation( PS3AsyncOperation_e nOperation );
#endif

	void	UpdateSpinners( void );
	void	UpdateScrollbarState( void );

	bool	AddMapToList( const PublishedFileInfo_t *fileInfo );
	void	RemoveMapFromList( PublishedFileId_t mapID );

	void	RequestQueueHistory( void );
	bool	m_bQueueHistoryRequested;

	static void ConfirmDeleteCommunityMap_Callback();
	static void ConfirmOverwriteSaveGame_Callback();
	static void ConfirmSaveFailure_Callback();
	
	void	ShowAuthorBadge( bool bVisible );
	void	EnsureSelection( void );

	UGCHandle_t		GetMapFileRequestForMapID( PublishedFileId_t nMapID );
	UGCHandle_t		GetThumbnailFileRequestForMapID( PublishedFileId_t nMapID );

	bool				m_bInputEnabled;

	bool				m_bIsSaveGameDialog;
	bool				m_bHasStorageDevice;
	bool				m_bSteamCloudResetRequested;

	bool				m_bSaveInProgress;
	bool				m_bSaveStarted;
	bool				m_bSetupComplete;
	
	bool				m_bWaitingForMapDownload;
	float				m_flWaitScreenDelay;		// Enforced delay to ensure that the wait screen can properly layout before we change

	IconRatingItem		*m_pRatingItem;
	CCommunityMapList	*m_pCommunityMapList;
	vgui::ImagePanel	*m_pThumbnailImage;
	CommunityMapInfoLabel	*m_pCommunityMapInfoLabel;
	vgui::ImagePanel	*m_pThumbnailSpinner;
	vgui::ImagePanel	*m_pDownloadingSpinner;
	vgui::ImagePanel	*m_pVoteInfoSpinner;
	vgui::ImagePanel	*m_pQueueSpinner;
	vgui::ImagePanel	*m_pAvatarSpinner;

	vgui::Label			*m_pTotalVotesLabel;

	vgui::Label			*m_pNoMapsLabel1;
	vgui::Label			*m_pNoBaselineLabel1;
	vgui::Label			*m_pNoBaselineLabel2;
	vgui::ImagePanel	*m_pAuthorAvatarImage;
	vgui::Button		*m_pQuickPlayButton;
	CDialogListButton	*m_pListTypeButton;

	// Window dressing for author badge
	vgui::ImagePanel	*m_pAuthorBadge;
	vgui::ImagePanel	*m_pAuthorBadgeOverlay;
	// vgui::ImagePanel	*m_pAuthorBadgeAward;
	vgui::ImagePanel	*m_pAuthorBadgeLogo;

	PublishedFileId_t	m_nCommunityMapToDelete;
	PublishedFileId_t	m_nCommunityMapToLaunch;

	int					m_nMapThumbnailId;
	int					m_nNewSaveGameImageId;
	int					m_nNoSaveGameImageId;

	int					m_nThumbnailImageId;

	FSAsyncControl_t	m_hAsyncControl;

	float				m_flTransitionStartTime;
	float				m_flNextLoadThumbnailTime;

	uint64				m_ulCurrentAvatarPlayerID;
	float				m_flAvatarRetryTime;		// If greater than -1, the time we should retry to update out avatar

	CUtlString			m_ThumbnailFilename;
	CUtlString			m_CurrentlySelectedItemInternalIDName;
	
	// New enumeration APIs
#if !defined( NO_STEAM )
	// For knowing when a persona state has changed
	void OnPersonaUpdated( PersonaStateChange_t *pPersonaStateChanged );
	STEAM_CALLBACK_MANUAL( CommunityMapDialog, Steam_OnPersonaUpdated, PersonaStateChange_t, m_CallbackPersonaStateChanged );
#endif

};

};

#endif // PORTAL2_PUZZLEMAKER

#endif // __VCOMMUNITYMAPDIALOG_H__
