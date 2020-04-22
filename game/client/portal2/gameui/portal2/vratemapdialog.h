//========= Copyright © Valve Corporation, All rights reserved. ============//
//
//
//==========================================================================//

#ifndef __VRATEMAPDIALOG_H__
#define __VRATEMAPDIALOG_H__

#if defined( PORTAL2_PUZZLEMAKER )

#include "basemodui.h"
#include "filesystem.h"
#include "vcommunitymapdialog.h"

class CVoteImagePanel;
class CGCCommunityMapDefinition;

namespace BaseModUI {

class GenericPanelList;
class CommunityMapInfoLabel;
class RateOptionsItem;

class RateMapDialog : public CBaseModFrame, public IBaseModFrameListener, public IMatchEventsSink
{
	DECLARE_CLASS_SIMPLE( RateMapDialog, CBaseModFrame );

public:
	RateMapDialog( vgui::Panel *pParent, const char *pPanelName );
	~RateMapDialog();

	// IMatchEventSink implementation
	virtual void OnEvent( KeyValues *pEvent );

	MESSAGE_FUNC( MsgReturnToGame, "MsgReturnToGame" );
	MESSAGE_FUNC_CHARPTR( MapDownloadAborted, "MapDownloadAborted", msg );

	void ThumbnailLoaded( const FileAsyncRequest_t &asyncRequest, int numReadBytes, FSAsyncStatus_t err );

	void CommitChanges();

	virtual void OnMousePressed( vgui::MouseCode code );

protected:
	virtual void OnCommand( char const *pCommand );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Activate();
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void PaintBackground();
	virtual void RunFrame();
	virtual void OnClose( void );
	virtual void SetDataSettings( KeyValues *pSettings );

private:
	void	UpdateFollowStatus( bool bStatus );
	void	UpdateFooter();
	void	Reset();
	void	DrawThumbnailImage();
	bool	StartAsyncThumbnailLoad( const char *pScreenshotFilename );
	void	ReturnToMapQueue( void );

	bool	LoadThumbnailFromContainer( const char *pScreenshotFilename );
	void	LoadThumbnailFromContainerSuccess();
	void	ProceedToNextMapInQueue( void );

	void	ViewMapInWorkshop( void );
	void	LaunchNextMap( void );

	bool	GetHighlightBounds( CVoteImagePanel *pCmd, int &x, int &y, int &w, int &h );
	void	UpdateVoteButtons( void );
	void	UpdateMapFields( void );

	void	MoveMapToHistory( void );
	void	StartCommitVote( void );

	GenericPanelList	*m_pRateOptionsList;
	vgui::ImagePanel	*m_pThumbnailImage;
	vgui::ImagePanel	*m_pThumbnailSpinner;

	// Vote action spinners
	vgui::ImagePanel	*m_pVoteUpSpinner;
	vgui::ImagePanel	*m_pVoteDownSpinner;

	vgui::Label			*m_pMapTitleLabel;
	vgui::Label			*m_pMapAuthorLabel;
	vgui::Label			*m_pMapDetailsLabel;
	vgui::ImagePanel	*m_pAuthorAvatarImage;
	CDialogListButton	*m_pFollowAuthorButton;
	vgui::Label			*m_pTotalVotesLabel;
	IconRatingItem		*m_pRatingsItem;

	CVoteImagePanel		*m_pVoteUpImage;
	CVoteImagePanel		*m_pVoteDownImage;

	float				m_flVoteCommitTime;		// Used to place a spinner on the vote button when pressed to show action
	bool				m_bNeedsMoveToFront;
	int					m_nVoteState;			// 0 - no vote, 1 - vote up, -1 - vote down
	int					m_nInitialVoteState;	// ditto
	
	bool				m_bEndOfLevelVersion;	// Whether this dialog was called up at the level transition, or mid-game
	bool				m_bFollowing;
	bool				m_bInitialFollowState;
	int 				m_bFollowStateChanged;

	CSteamID			m_AuthorSteamID;
	uint32				m_unMapID;

	int					m_nThumbnailImageId;
	bool				m_bWaitingForMapDownload;
	PublishedFileId_t	m_unNextMapInQueueID;

	FSAsyncControl_t	m_hAsyncControl;

	float				m_flTransitionStartTime;

	CUtlString			m_ThumbnailFilename;
	
	UGCHandle_t			m_hThumbFileHandle;
	
#if !defined( _GAMECONSOLE )
	CCallResult<RateMapDialog, RemoteStorageUserVoteDetails_t> m_callbackGetUserPublishedItemVoteDetails;
	void Steam_OnGetUserPublishedItemVoteDetails( RemoteStorageUserVoteDetails_t *pResult, bool bError );
#endif // USE_STEAM_BETA_APIS && !_GAMECONSOLE

};

};

#endif // PORTAL2_PUZZLEMAKER

#endif // __VRATEMAPDIALOG_H__
