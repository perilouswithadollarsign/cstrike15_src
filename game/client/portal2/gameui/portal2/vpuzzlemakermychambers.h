//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#ifndef __VPUZZLEMAKERMYCHAMBERS_H__
#define __VPUZZLEMAKERMYCHAMBERS_H__

#if defined( PORTAL2_PUZZLEMAKER )

#include "basemodui.h"
#include "vgui_controls/panellistpanel.h"
#include "vpuzzlemakeruilistitem.h"
#include "puzzlemaker/puzzlemaker.h"
#include "vcommunitymapdialog.h"


struct PuzzleFileInfo_t
{
	CUtlString m_strBaseFileName;	// "testchamber" + (timestamp)
	PuzzleFilesInfo_t m_publishInfo;
};


using namespace vgui;
using namespace BaseModUI;

class CDialogListButton;

enum MyChambersSortType_t 
{
	CHAMBER_SORT_STATUS,
	CHAMBER_SORT_MODIFIED,
};

namespace BaseModUI {

	class GenericPanelList;
	class CChamberListItem;

	// Adds an UpdateFooter virtual function to all panels used in the Puzzle Maker UI
	class CPuzzleMakerUIPanel : public CBaseModFrame
	{
		DECLARE_CLASS_SIMPLE( CPuzzleMakerUIPanel, CBaseModFrame );

		CPuzzleMakerUIPanel( vgui::Panel *pParent, const char *pPanelName );
		virtual ~CPuzzleMakerUIPanel() { }

	protected:
		virtual void UpdateFooter() { } 

		friend class CPuzzleMakerUIListItem;
	};

	//=============================================================================
	class CPuzzleMakerMyChambers : public CPuzzleMakerUIPanel, public IBaseModFrameListener, public IMatchEventsSink
	{
		DECLARE_CLASS_SIMPLE( CPuzzleMakerMyChambers, CPuzzleMakerUIPanel );

	public:
		CPuzzleMakerMyChambers( vgui::Panel *pParent, const char *pPanelName );
		~CPuzzleMakerMyChambers();

		virtual void OnEvent( KeyValues *pEvent );

		void LoadChamber( int nIndex );
		void DeleteSelectedPuzzle();
		void UnpublishSelectedPuzzle();
		void OnPublishedFileDeleted( bool bError, PublishedFileId_t nID );
		void UseInEditorFooter();

		void ScreenshotLoaded( const FileAsyncRequest_t &asyncRequest, int nNumReadBytes, FSAsyncStatus_t err );
		void ReadPuzzleInfoFromBuffer( const FileAsyncRequest_t &asyncRequest, int nNumReadBytes, FSAsyncStatus_t err );

		virtual void PaintBackground();
		MESSAGE_FUNC_CHARPTR( OnItemSelected, "OnItemSelected", pPanelName );
		MESSAGE_FUNC( OnGameUIHidden, "GameUIHidden" );	// called when the GameUI is hidden
		MESSAGE_FUNC( OnRefreshList, "RefreshList" );	// Repopulate our list because something has changed

		static void FixFooter();

	protected:
		virtual void	Activate();
		virtual void	ApplySchemeSettings( vgui::IScheme* pScheme );
		virtual void	OnKeyCodePressed( vgui::KeyCode code );
		virtual void	OnCommand( const char *pCommand );
		virtual void	OnThink();
		virtual void	RunFrame();
		virtual void	SetDataSettings( KeyValues *pSettings );

	private:
		void SetupChamberInformation( int nIndex );
		void SetStatusLabelForListItem( CChamberListItem *pItem, PuzzleFileInfo_t *pPuzzleInfo );
		void SetSortType( MyChambersSortType_t sortType );
		bool StartAsyncScreenshotLoad( const char *pThumbnailFilename );
		virtual void UpdateFooter();
		void SetSelectedChamber( int nIndex );
		void ViewMapInWorkshop( void );

		void PopulateChamberListFromDisk( bool bForceUpdate = false );
		void SortAndListChambers();
		void GetPuzzleInfoFromFile( const char *pszFileName, int puzzleInfoIndex );
		void NewPanelSelected( CChamberListItem *pSelectedPanel );
		void ShowPendingFileActionsSpinner( bool bState );
		void FinishPuzzleInfoLoad( void );
		void ShowPuzzleInformation( bool bShow );
		void UpdateFileInfoRequests( void );
		void UpdateSpinners( void );
		void ShowRenameDialog();
		void ShowDeleteDialog();
		bool ResolveOrphanedWorkshopFile( PuzzleFilesInfo_t *pPuzzleInfo );

		void DeletePuzzleAtIndex( int nIndex );

		void PromptToLoadAutoSave();

		char m_szSteamID[MAX_PATH];

		CUtlVector<PuzzleFileInfo_t *> m_SavedChambers;

		PuzzleFileInfo_t	*m_pNewChamberStub;
		int m_nAutoSaveIndex;

		void DrawThumbnailImage();
		char m_szScreenshotName[MAX_PATH];
		vgui::ImagePanel *m_pThumbnailImage;
		int	m_nThumbnailImageId;
		float m_flTransitionStartTime;
		float m_flNextLoadThumbnailTime;
		
		GenericPanelList		*m_pChamberList;
		CDialogListButton		*m_pChamberSortButton;
		CChamberListItem		*m_pSelectedListPanel;
		int						m_nSelectedListItemIndex;
		int						m_nDeleteIndex;
		bool					m_bInEditor;
		bool					m_bSetupComplete;
		bool					m_bDeferredFinishCompleted;
		FSAsyncControl_t		m_hAsyncControl;
		MyChambersSortType_t	m_sortType;
		FSAsyncControl_t		m_hPuzzleInfoAsyncControl;
		
		vgui::Label				*m_pLblAutoSaveFound;
		vgui::Label				*m_pLblChamberName;
		vgui::Label				*m_pLblStatCreated;
		vgui::Label				*m_pLblStatCreatedData;
		vgui::Label				*m_pLblStatModified;
		vgui::Label				*m_pLblStatModifiedData;
		vgui::Label				*m_pLblStatPublished;	
		vgui::Label				*m_pLblStatPublishedData;
		vgui::Label				*m_pLblStatRating;
		vgui::ImagePanel		*m_pAutoSaveInfoImage;
		vgui::ImagePanel		*m_pPuzzleListSpinner;
		vgui::ImagePanel		*m_pThumbnailSpinner;
		IconRatingItem			*m_pRatingItem;
		friend class CChamberListItem;

		struct AsyncPuzzleInfoRequest_t
		{
			int	nPuzzleInfoIndex;
			char szFilename[MAX_PATH];
		};

		// Collection of known files on disk that we need to retrieve more information for
		CUtlQueue<AsyncPuzzleInfoRequest_t *>	m_PuzzleInfoFileRequests;
		AsyncPuzzleInfoRequest_t				*m_pCurrentPuzzleFileInfoRequest;
		bool									m_bPuzzlesLoadedFromDisk;
	};


	//=============================================================================
	class CChamberListItem : public CPuzzleMakerUIListItem
	{
		DECLARE_CLASS_SIMPLE( CChamberListItem, CPuzzleMakerUIListItem );
	public:
		CChamberListItem( vgui::Panel *pParent, const char *pPanelName );

		void SetChamberStatus( wchar_t *pChamberStatus );
		void SetChamberInfoIndex( int nIndex ) { m_nChamberInfoIndex = nIndex; }
		int GetChamberInfoIndex( void ) { return m_nChamberInfoIndex; }

	protected:
		virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
		virtual void PaintBackground();

		virtual void OnMousePressed( vgui::MouseCode code );
		virtual void OnMouseDoublePressed( vgui::MouseCode code );
		virtual void OnKeyCodePressed( vgui::KeyCode code );

	private:

		Label *m_pLblChamberStatus;
		int m_nNewDemoCount;
		int m_nChamberInfoIndex;

		GenericPanelList *m_pListCtrlr;
		CPuzzleMakerMyChambers *m_pDialog;
		bool m_bHasMouseover;
	};

	//=============================================================================
	class CBetaMapListItem : public CPuzzleMakerUIListItem
	{
		DECLARE_CLASS_SIMPLE( CBetaMapListItem, CPuzzleMakerUIListItem );
	public:
		CBetaMapListItem( vgui::Panel *pParent, const char *pPanelName );

		void AddNewDemos( int nNewDemoCount );
		void SetMapID( PublishedFileId_t mapID ) { m_nMapID = mapID; }
		void SetMapFileID( UGCHandle_t mapFileID ) { m_nMapFileID = mapFileID; }
		void SetThumbnailID( UGCHandle_t thumbID ) { m_nMapThumbnailFileID = thumbID; }

		PublishedFileId_t GetMapID( void ) const { return m_nMapID; }
		UGCHandle_t GetMapFileID( void ) const { return m_nMapFileID; }
		UGCHandle_t GetThumbnailID( void ) const { return m_nMapThumbnailFileID; }

	protected:
		virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
		virtual void PaintBackground();

	private:

		Label				*m_pLblNewDemoCount;
		int					m_nNewDemoCount;
		UGCHandle_t			m_nMapFileID;
		UGCHandle_t			m_nMapThumbnailFileID;
		PublishedFileId_t	m_nMapID;
	};

};

#endif // PORTAL2_PUZZLEMAKER

#endif // __VPUZZLEMAKERMYCHAMBERS_H__
