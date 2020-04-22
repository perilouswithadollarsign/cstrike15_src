//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#ifndef __VPUZZLEMAKERBETATESTLIST_H__
#define __VPUZZLEMAKERBETATESTLIST_H__

#if defined( PORTAL2_PUZZLEMAKER )

#include "basemodui.h"
#include "vgui_controls/panellistpanel.h"
#include "vpuzzlemakeruilistitem.h"
#include "vpuzzlemakermychambers.h"
#include "vgui_controls/imagepanel.h"

using namespace vgui;
using namespace BaseModUI;

class CDialogListButton;



namespace BaseModUI {

	class GenericPanelList;
	class CChamberListItem;


	//=============================================================================
	class CPuzzleMakerBetaTestList : public CPuzzleMakerUIPanel, public IMatchEventsSink
	{
		DECLARE_CLASS_SIMPLE( CPuzzleMakerBetaTestList, CPuzzleMakerUIPanel );

	public:

		typedef enum
		{
			MENU_INVALID = 0,
			MENU_BETA_RESULTS,
			MENU_BETA_PLAYER_LIST,
			MENU_PUBLISHED_RESULTS,
			MENU_COUNT
		} MenuType_t;

		CPuzzleMakerBetaTestList( vgui::Panel *pParent, const char *pPanelName, MenuType_t nMenuType );
		~CPuzzleMakerBetaTestList();

		// IMatchEventSink implementation
		virtual void OnEvent( KeyValues *pEvent );

		MESSAGE_FUNC_CHARPTR( OnItemSelected, "OnItemSelected", pPanelName );
		void ScreenshotLoaded( const FileAsyncRequest_t &asyncRequest, int nNumReadBytes, FSAsyncStatus_t err );

	protected:
		virtual void	Activate();
		virtual void	ApplySchemeSettings( vgui::IScheme* pScheme );
		virtual void	OnKeyCodePressed( vgui::KeyCode code );
		virtual void	OnCommand( const char *pCommand );
		virtual void	OnThink();
		virtual void	PaintBackground( void );

		virtual void SetDataSettings( KeyValues *pSettings );

		bool MapInDownloadList( UGCHandle_t mapID );
		bool UpdateDownloads( CUtlVector<const UGCFileRequest_t *> &vDownloads );

		void CreateMapPanel( PublishedFileInfo_t &publishedFileInfo );

		void SetMapThumbnailImage( UGCHandle_t mapID );
		const UGCFileRequest_t *GetThumbnailFileRequestForMap( UGCHandle_t mapThumbID );
		bool StartAsyncScreenshotLoad( const char *pThumbnailFilename );
		bool LoadThumbnailFromContainer( const char *pThumbnailFilename );

#if !defined( NO_STEAM )
		void SetPlayerInfo( CSteamID playerID );
#endif
		

	private:
		// private helper functions
		virtual void	UpdateFooter();
		void			DrawThumbnailImage( void );
		void			ClockSpinner( bool bVisible );
		void			SetPlayerName( const char *pPlayerName );

		// vgui elements
		GenericPanelList	*m_pBetaTestList;
		vgui::ImagePanel	*m_pThumbnailImage;
		Label				*m_pLblChamberName;
		Label				*m_pLblPlayerName;
		vgui::ImagePanel	*m_pImgWorkingAnim;
		vgui::ImagePanel	*m_pImgPlayerAvatar;
		vgui::ImagePanel	*m_pImgAvatarBorder;
		vgui::ImagePanel	*m_pImgAvatarWorkingAnim;
		vgui::IImage		*m_pAvatar;

		// player avatar elements
		// current selected player's SteamID
		uint64			m_currentPlayerID;
		float			m_flRetryAvatarTime;

		// differentiate menu types this panel is used for
		MenuType_t			m_menuType;

		// data passed to the panel for the list of players who have demos for the current map
		char				m_szMapName[ MAX_MAP_NAME ];
		PublishedFileId_t	m_nMapID;

		// map thumbnail image data
		float				m_flNextLoadThumbnailTime;
		int					m_nThumbnailImageId;
		FSAsyncControl_t	m_hAsyncControl;
		bool				m_bDrawSpinner;
		CUtlVector<const UGCFileRequest_t *> m_vMapThumbnailDownloads;	// actual download requests for map thumbnails
		bool				m_bDownloadingThumbnails;

		bool				m_bPublishedDataDirty;
		
		// handle map data and their new demo counts
		CUtlMap< UGCHandle_t, int > m_MapMapIDToNewDemoCount;
		CUtlVector<UGCFileRequest_t *> m_vMapDownloads;	// actual download requests for maps
		bool				m_bDownloadingMaps;

		// get published file data for the maps
		CUtlQueue< UGCHandle_t > m_vMapsToQuery;
		UGCHandle_t m_nActiveQueryID;
		CUtlVector< PublishedFileInfo_t > m_vPublishedFileData;

		

#if !defined( NO_STEAM )
		// Per-item callback for enumerating user published files (maps)
		CCallResult<CPuzzleMakerBetaTestList, RemoteStorageEnumerateUserPublishedFilesResult_t> m_callbackEnumeratePublishedPuzzles;
		void Steam_OnEnumeratePublishedPuzzles( RemoteStorageEnumerateUserPublishedFilesResult_t *pResult, bool bError );

		// TEMP 
		CCallResult<CPuzzleMakerBetaTestList, RemoteStorageEnumerateUserSubscribedFilesResult_t> m_callbackEnumerateSubscribedPuzzles;
		void Steam_OnEnumerateSubscribedPuzzles( RemoteStorageEnumerateUserSubscribedFilesResult_t *pResult, bool bError );

		// Per-item callback for retrieving details on maps
		CCallResult<CPuzzleMakerBetaTestList, RemoteStorageGetPublishedFileDetailsResult_t> m_callbackGetPublishedFileDetails;
		void Steam_OnGetPublishedFileDetails( RemoteStorageGetPublishedFileDetailsResult_t *pResult, bool bError );
#endif
		friend class CChamberListItem;
	};


	//=============================================================================
	class CPublishedListItem : public CPuzzleMakerUIListItem
	{
		DECLARE_CLASS_SIMPLE( CPublishedListItem, CPuzzleMakerUIListItem );
	public:
		CPublishedListItem( vgui::Panel *pParent, const char *pPanelName );

		void SetChamberRating( int nRating );

	protected:
		virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
		virtual void PaintBackground();

	private:

		vgui::ImagePanel				*m_pImgChamberRating;
		int						m_nRating;
	};

	//=============================================================================
	class CBetaPlayerListItem : public CPuzzleMakerUIListItem
	{
		DECLARE_CLASS_SIMPLE( CBetaPlayerListItem, CPuzzleMakerUIListItem );
	public:
		CBetaPlayerListItem( vgui::Panel *pParent, const char *pPanelName );

		//void SetupLabels( bool bNewTest, bool bUpvoted );
#if !defined( NO_STEAM )
		void SetupLabels( CSteamID &playerID, bool bNewTest, bool bUpvoted );
		CSteamID GetPlayerID( void ) const { return m_playerID; }
#endif

	protected:
		virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
		virtual void PaintBackground();

		void SetImageHighlighted( bool bHighlighted );

	private:

		vgui::ImagePanel				*m_pImgThumbRating;
		Label							*m_pLblStatus;
		bool							m_bUpvoted;
		bool							m_bNewTests;

#if !defined( NO_STEAM )
		CSteamID						m_playerID;
#endif
	};

};

#endif // PORTAL2_PUZZLEMAKER

#endif // __VPUZZLEMAKERBETATESTLIST_H__
