//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#ifndef __VPLAYTESTDEMOSDIALOG_H__
#define __VPLAYTESTDEMOSDIALOG_H__

#if defined( PORTAL2_PUZZLEMAKER )

#include "basemodui.h"
#include "VGenericPanelList.h"
#include "vcommunitymapdialog.h"
#include "vgui_controls/ImagePanel.h"

using namespace vgui;
using namespace BaseModUI;

class CDialogListButton;
class CPortalLeaderboardGraphPanel;
class CPortalChallengeStatsPanel;

// hold info derived from a demo file
class CDemoInfo 
{
public:
	CDemoInfo () :
		m_mapID( 0 ),
		m_mapFileHandle( k_UGCHandleInvalid ),
		m_originalDemoHandle( k_UGCHandleInvalid ),
		m_demoHandle( k_UGCHandleInvalid ),
		m_bInLocalCloud( false )
	{
	}

	~CDemoInfo()
	{
		/* do NOT free file request, as it is now owned by the basemodpanel system */
	}

	PublishedFileId_t m_mapID;
	UGCHandle_t		m_mapFileHandle;		// UGC handle for the map associated with this demo
	UGCHandle_t		m_demoHandle;			// Demo handle
	UGCHandle_t		m_originalDemoHandle;	// Handle to the original demo (only used when replicating file to user's local cloud)
	bool			m_bInLocalCloud;
	bool			m_bNewDemo;
	char			m_szFilename[MAX_PATH];	// Filename of the demo (for tracking uploads)

#if !defined( NO_STEAM )
	CSteamID m_playerID;		// Steam ID of the owning user of this demo
#endif
};

namespace BaseModUI {

	class CDemoListItem;



//=============================================================================
class CPlaytestDemosDialog : public CBaseModFrame, public IMatchEventsSink
{
	DECLARE_CLASS_SIMPLE( CPlaytestDemosDialog, CBaseModFrame );

public:
	CPlaytestDemosDialog( vgui::Panel *pParent, const char *pPanelName );
	~CPlaytestDemosDialog();
	
	virtual void	OnKeyCodePressed( vgui::KeyCode code );

	// IMatchEventSink implementation
	virtual void OnEvent( KeyValues *pEvent );
	
	void	UpdateFooter();

	virtual void OnClose();

	void SetCurrentMap( const char *pMap, CDemoListItem *pMapItem );
	void SetCurrentDemo( const char *pDemo, CDemoListItem *pMapItem );

protected:
	virtual void	Activate();
	virtual void	ApplySchemeSettings( vgui::IScheme* pScheme );
	
	virtual void	OnCommand( const char *pCommand );
	virtual void	OnThink();
	virtual void	OnMousePressed( vgui::MouseCode code );

	virtual void SetDataSettings( KeyValues *pSettings );

	void ClearDemoInfo();
	void SetDemoFileInfos();
	void UpdateDemoList();
	void EnableMap( const char *pMapName );
	void EnableFinishedMaps( void );
	void SetDemoKV();
	void UploadDemo( CDemoInfo *demoInfo );
	bool UpdateFileUploadRequest( CDemoInfo *pDemoInfo );
	void DownloadDemos();
	void CreateDownloadRequest( KeyValues *pKV );

	bool DemoDownloadsComplete( void );
	bool MapDownloadsComplete( void );
	void UpdateDemoUploads( void );

	void DeleteSelectedDemo();
	void FreeDemoDownloads( void );

	void ClearCloudPlaytests();
	void EnumerateCloudFiles();
	void UpdateTimeLabel( float flTime );

	void ClockSpinner( bool bVisible );

	CDemoListItem *GetDemoPanel( const char *pDemoName );

private:

	GenericPanelList	*m_pMapList;
	GenericPanelList	*m_pDemoFileList;

	Label				*m_pPlayerLabel;
	Label				*m_pTimeLabel;

	vgui::ImagePanel	*m_pCloudImage;
	vgui::ImagePanel	*m_pDwnCloudImage;
	vgui::ImagePanel	*m_pSpinner;

	// remember which map and which sub-demo we are currently selecting
	char				m_szCurrentMap[ MAX_MAP_NAME ];
	char				m_szCurrentDemoFile[ MAX_PATH ];

	float				m_flNextDemoRequest;

	// break out of the demo file info into key values
	KeyValues *m_pMapDemoValues;
	KeyValues::AutoDelete m_autodelete_pMapDemoValues;

	// upclouding demo data
	CUtlQueue< CDemoInfo* > m_DemoUploads;

	// cloud data - demos
	CUtlVector< CDemoInfo* > m_vDemoDownloads;
	bool	m_bDownloadingDemos;

	// cloud data - maps
	CUtlVector<UGCHandle_t> m_vMapDownloadHandles;  // keep the handles of the files we want to download
	CUtlQueue<PublishedFileId_t> m_nMapIDsToQuery;	// map id's for the .bsp's we need
	bool m_bDownloadingMaps;


	//// TEMP - enumerate published files
	CCallResult<CPlaytestDemosDialog, RemoteStorageEnumerateUserPublishedFilesResult_t> m_callbackEnumerateFiles;
	void Steam_OnEnumerateFiles( RemoteStorageEnumerateUserPublishedFilesResult_t *pResult, bool bError );


	/// TEMP - Logging
	enum UGCProcessTypes_t
	{
		DOWNLOAD_DEMOS = 0,
		DOWNLOAD_MAPS,
		UPLOAD_DEMOS,
		PROCESSTYPE_COUNT
	};

	bool bProcessesLogged[ PROCESSTYPE_COUNT ];
	void ResetLogging( void );
	
	friend class CDemoListItem;

	//bool	m_bUploading;
};


//=============================================================================
class CDemoListItem : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CDemoListItem, vgui::EditablePanel );
public:
	CDemoListItem( vgui::Panel *pParent, const char *pPanelName );

	void SetLabelText( const char *pMapName, bool bInMapColumn = true );

	void SetAsNew( bool bNew ){ m_bMarkedAsNew = bNew; }
	bool IsNewDemo( void ) { return m_bMarkedAsNew; }

	void SetUGCHandle( UGCHandle_t demoHandle ) { m_demoHandle = demoHandle; }
	UGCHandle_t GetUGCHandle( void ) const { return m_demoHandle; }

	bool IsSelected( void ) { return m_bSelected; }
	void SetSelected( bool bSelected ) { m_bSelected = bSelected; }

	void SetDisabled( bool bState ) { m_bDisabled = bState; }
	bool IsDisabled( void ) const { return m_bDisabled; }

	bool HasMouseover( void ) { return m_bHasMouseover; }
	void SetHasMouseover( bool bHasMouseover );
	void OnKeyCodePressed( vgui::KeyCode code );

	const char *GetMapName( void ) const { return m_szLabelText; }

	virtual void NavigateTo();
	virtual void NavigateFrom();

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PaintBackground();
	virtual void OnCursorEntered();
	virtual void OnCursorExited() { SetHasMouseover( false ); }

	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnMouseDoublePressed( vgui::MouseCode code );
	void PerformLayout();

protected:
	void DrawListItemLabel( vgui::Label *pLabel );

	CPlaytestDemosDialog	*m_pPlaytestDialog;

	GenericPanelList	*m_pListCtrlr;
	vgui::HFont			m_hTextFont;
	vgui::HFont			m_hFriendsListFont;
	vgui::HFont			m_hFriendsListSmallFont;
	vgui::HFont			m_hFriendsListVerySmallFont;

	char				m_szLabelText[ MAX_PATH ];

	Color				m_BaseColor;
	Color				m_TextColor;
	Color				m_FocusColor;
	Color				m_DisabledColor;
	Color				m_CursorColor;
	Color				m_LockedColor;
	Color				m_MouseOverCursorColor;

	Color				m_LostFocusColor;

	bool				m_bSelected;
	bool				m_bHasMouseover;
	bool				m_bDisabled;

	bool				m_bInMapColumn;
	bool				m_bMarkedAsNew;
	UGCHandle_t			m_demoHandle;

	int					m_nTextOffsetY;
};


};

#endif // PORTAL2_PUZZLEMAKER

#endif // __VPLAYTESTDEMOSDIALOG_H__
