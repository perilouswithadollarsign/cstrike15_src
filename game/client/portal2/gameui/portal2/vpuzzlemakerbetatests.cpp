//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#include "cbase.h"

#if defined( PORTAL2_PUZZLEMAKER )

#include "vpuzzlemakerbetatests.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "EngineInterface.h"
#include "IGameUIFuncs.h"
#include "gameui_util.h"
#include "vgui/ISurface.h"
#include "VGenericConfirmation.h"
#include "VGenericPanelList.h"
#include "gameui/portal2/vdialoglistbutton.h"
#include "vpuzzlemakeruilistitem.h"

#include "FileSystem.h"

#if !defined( _X360 ) && !defined( _PS3 )
#include "gc_clientsystem.h"
#include "econ_gcmessages.h"
#endif
#include "imageutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;
using namespace BaseModUI;


CPuzzleMakerBetaTestList::CPuzzleMakerBetaTestList( Panel *pParent, const char *pPanelName, MenuType_t nMenuType ):
BaseClass( pParent, pPanelName ), m_pBetaTestList( NULL )
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	// Subscribe to event notifications
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	m_bDownloadingMaps = false;
	m_bDownloadingThumbnails = false;
	m_bPublishedDataDirty = false;
	m_bDrawSpinner = false;
	m_flNextLoadThumbnailTime = 0.0f;
	m_nThumbnailImageId = -1;
	m_hAsyncControl = NULL;
	m_pThumbnailImage = NULL;
	m_pLblChamberName = NULL;
	m_pImgWorkingAnim = NULL;
	m_pImgPlayerAvatar = NULL;
	m_pImgAvatarBorder = NULL;
	m_pLblPlayerName = NULL;
	m_pAvatar = NULL;
	m_pImgAvatarWorkingAnim = NULL;
	m_currentPlayerID = 0;
	m_flRetryAvatarTime = -1.0f;
	m_MapMapIDToNewDemoCount.SetLessFunc( DefLessFunc( UGCHandle_t ) );

	m_nActiveQueryID = k_UGCHandleInvalid;

	m_menuType = nMenuType;

	if ( m_menuType == MENU_BETA_RESULTS )
	{
		SetDialogTitle( "#PORTAL2_EditorMenu_BetaTestHeader" );
	}
	else if ( m_menuType == MENU_PUBLISHED_RESULTS )
	{
		SetDialogTitle( "#PORTAL2_EditorMenu_PublishedTestHeader" );
	}
	else if ( m_menuType == MENU_BETA_PLAYER_LIST )
	{
		SetDialogTitle( "#PORTAL2_EditorMenu_BetaSessionsHeader" );
	}

	m_pBetaTestList = new GenericPanelList( this, "PnlTestList", GenericPanelList::ISM_PERITEM | GenericPanelList::ISM_ALPHA_INVISIBLE );
	m_pBetaTestList->SetPaintBackgroundEnabled( false );
	m_pBetaTestList->SetScrollBarVisible( true );

	SetFooterEnabled( true );
	UpdateFooter();
}


CPuzzleMakerBetaTestList::~CPuzzleMakerBetaTestList()
{
	if ( m_pBetaTestList )
	{
		delete m_pBetaTestList;
	}

	if ( m_pAvatar )
	{
		BaseModUI::CUIGameData::Get()->AccessAvatarImage( m_currentPlayerID, BaseModUI::CUIGameData::kAvatarImageRelease );
		m_pAvatar = NULL;
	}

	// Unsubscribe from event notifications
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
}


void CPuzzleMakerBetaTestList::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pThumbnailImage = static_cast< ImagePanel *>( FindChildByName( "ImgChamberThumb" ) );
	m_pLblChamberName = static_cast< Label *>( FindChildByName( "LblChamberName" ) );
	m_pImgWorkingAnim = static_cast< ImagePanel *>( FindChildByName( "WorkingAnim" ) );
	m_pImgAvatarWorkingAnim = static_cast< ImagePanel *>( FindChildByName( "AvatarWorkingAnim" ) );
	m_pImgPlayerAvatar = static_cast< ImagePanel *>( FindChildByName( "ImgPlayerAvatar" ) );
	m_pImgAvatarBorder = static_cast< ImagePanel *>( FindChildByName( "ImgAvatarBorder" ) );
	m_pLblPlayerName = static_cast< Label *>( FindChildByName( "LblCurrentPlayerName" ) );

	Label *pColumnTitle = static_cast< Label *>( FindChildByName( "LblColumnTitle" ) );
	// TEMP - stats will be more than one panel - maybe make it into a sub-panel?
	Label *pStatsLabel = static_cast< Label *>( FindChildByName( "LblChamberStatLegend" ) );
	Label *pPlayerInfoLabel = static_cast< Label *>( FindChildByName( "LblCurrentPlayerInfo" ) );
	Label *pPlayerStatsLabel = static_cast< Label *>( FindChildByName( "LblCurrentPlayerStats" ) );
	if ( pColumnTitle && pStatsLabel && pPlayerInfoLabel && pPlayerStatsLabel )
	{
		if ( m_menuType == MENU_BETA_RESULTS )
		{
			pColumnTitle->SetText( "#PORTAL2_EditorMenu_TestCount" );
		}
		else if ( m_menuType == MENU_PUBLISHED_RESULTS )
		{
			pColumnTitle->SetText( "#PORTAL2_EditorMenu_CurrentRating" );
		}
		else 
		{
			SetDialogSubTitle( m_szMapName );
			pColumnTitle->SetVisible( false );
			pStatsLabel->SetVisible( false );
			pPlayerInfoLabel->SetVisible( true );
			pPlayerStatsLabel->SetVisible( true );
		}
	}
	
	UpdateFooter();
}


void CPuzzleMakerBetaTestList::Activate()
{
	BaseClass::Activate();

	// make sure the panel items are all clear
	//m_pBetaTestList->RemoveAllPanelItems();

	// TEMP
	unsigned int nNumCommunityMaps = CBaseModPanel::GetSingleton().GetNumCommunityMapsInQueue();
	if ( nNumCommunityMaps )
	{
		for ( unsigned int i = 0; i < nNumCommunityMaps; i++ )
		{
			const PublishedFileInfo_t *pResult = CBaseModPanel::GetSingleton().GetCommunityMap( i );
			if ( pResult == NULL )
			{
				// FIXME: Handle this error case!
				continue;
			}

			// Display the demo handle and map id
			//DevMsg("MapID: %llu  MapFileID: %llu\n", pResult->m_nPublishedFileId, pResult->m_hFile );
		}
	}


	if ( m_menuType == MENU_BETA_RESULTS )
	{

#if !defined( NO_STEAM )
		CSteamID authorID;
		if ( steamapicontext && steamapicontext->SteamUser() )
		{
			authorID = steamapicontext->SteamUser()->GetSteamID();
		}

		// request maps and new counts from the GC
		GCSDK::CProtoBufMsg<CMsgPlaytestRetrieveDemoHandles> msgRetrieveNewCount( k_EMsgGCPlaytestRetrieveNewDemoCount );
		msgRetrieveNewCount.Body().set_author_id( authorID.GetAccountID() );
		GCClientSystem()->BSendMessage( msgRetrieveNewCount );

		// get a list of user published files ( maps )
		if ( ISteamRemoteStorage *pRemoteStorage = GetISteamRemoteStorage() )
		{
			/*SteamAPICall_t hSteamAPICall = pRemoteStorage->EnumerateUserPublishedFiles( 0 );
			m_callbackEnumeratePublishedPuzzles.Set( hSteamAPICall, this, &CPuzzleMakerBetaTestList::Steam_OnEnumeratePublishedPuzzles );

			SteamAPICall_t hSteamAPICall2 = pRemoteStorage->EnumerateUserSubscribedFiles( 0 );
			m_callbackEnumerateSubscribedPuzzles.Set( hSteamAPICall2, this, &CPuzzleMakerBetaTestList::Steam_OnEnumerateSubscribedPuzzles );*/
		}
#endif
		
	}
	else if ( m_menuType == MENU_PUBLISHED_RESULTS )
	{
		CPublishedListItem *pItem = m_pBetaTestList->AddPanelItem< CPublishedListItem >( "puzzlemaker_publisheditem" );
		if ( pItem )
		{
			pItem->SetPrimaryText( "Test Chamber Name" );
			pItem->SetChamberRating( 3 );
		}

		pItem = m_pBetaTestList->AddPanelItem< CPublishedListItem >( "puzzlemaker_publisheditem" );
		if ( pItem )
		{
			pItem->SetPrimaryText( "Test Chamber Name 2" );
			pItem->SetChamberRating( 5 );
		}

		pItem = m_pBetaTestList->AddPanelItem< CPublishedListItem >( "puzzlemaker_publisheditem" );
		if ( pItem )
		{
			pItem->SetPrimaryText( "Test Chamber Name 3" );
			pItem->SetChamberRating( 0 );
		}
	}
	else if ( m_menuType == MENU_BETA_PLAYER_LIST )
	{

		// request players and demo counts from the GC
		GCSDK::CProtoBufMsg<CMsgPlaytestRetrieveDemoPlayersForMap> msgRetrievePlayers( k_EMsgGCPlaytestRetrieveDemoPlayersForMap );
		msgRetrievePlayers.Body().set_map_id( m_nMapID );
		GCClientSystem()->BSendMessage( msgRetrievePlayers );

	}


	m_pBetaTestList->SelectPanelItem( 0 );

	UpdateFooter();
}


void CPuzzleMakerBetaTestList::OnKeyCodePressed( KeyCode code )
{
	int joystick = GetJoystickForCode( code );
	int userId = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	if ( joystick != userId || joystick < 0 )
	{	
		return;
	}

	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		BaseClass::OnKeyCodePressed( code );
		break;

	case KEY_XBUTTON_A:
		{
			CBetaMapListItem *pMapListItem = static_cast< CBetaMapListItem *>( m_pBetaTestList->GetSelectedPanelItem() );
			KeyValues *pBetaMapValues = new KeyValues( "BetaMapInfo" );
			pBetaMapValues->SetInt( "ThumbnailID", m_nThumbnailImageId );
			pBetaMapValues->SetUint64( "MapID", pMapListItem->GetMapID() );
			pBetaMapValues->SetString( "MapName", pMapListItem->GetPrimaryText() );
			CBaseModPanel::GetSingleton().OpenWindow( WT_BETATESTPLAYERLIST, this, true, pBetaMapValues );
		}
		break;

	default:
		BaseClass::OnKeyCodePressed(code);
		break;
	}
}


void CPuzzleMakerBetaTestList::OnCommand(const char *command)
{
	if ( !V_stricmp( "Cancel", command ) || !V_stricmp( "Back", command ) )
	{
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}


bool CPuzzleMakerBetaTestList::UpdateDownloads( CUtlVector<const UGCFileRequest_t *> &vDownloads )
{
	// FIXME: JDW
#if 0
	bool bDone = true;
	FOR_EACH_VEC(vDownloads, itr)
	{
		UGCFileRequestStatus_t currStatus = vDownloads[itr]->fileRequest.GetStatus();
		if ( currStatus != UGCFILEREQUEST_FINISHED && currStatus != UGCFILEREQUEST_ERROR )
		{
			bDone = false;
		}
	}

	return bDone;
#endif	
	return false;
}


void CPuzzleMakerBetaTestList::CreateMapPanel( PublishedFileInfo_t &publishedFileInfo )
{
	// get the count of new demos
	int nMapIndex = m_MapMapIDToNewDemoCount.Find( publishedFileInfo.m_nPublishedFileId );
	if ( nMapIndex == m_MapMapIDToNewDemoCount.InvalidIndex() )
	{
		return;
	}

	int nNewDemoCount = m_MapMapIDToNewDemoCount[nMapIndex];

	// make sure there isn't already an existing map item for this map
	int nPanelCount = m_pBetaTestList->GetPanelItemCount();
	for ( int i = 0; i < nPanelCount; ++i )
	{
		CBetaMapListItem *pMapItem = static_cast< CBetaMapListItem *>( m_pBetaTestList->GetPanelItem( i ) );
		if ( pMapItem )
		{
			if ( pMapItem->GetMapID() == publishedFileInfo.m_nPublishedFileId )
			{
				// already a map item for this map
				return;
			}
		}
	}

	// create the panel item
	CBetaMapListItem *pItem = m_pBetaTestList->AddPanelItem< CBetaMapListItem >( "puzzlemaker_chamberitem" );
	if ( pItem )
	{
		pItem->SetPrimaryText( publishedFileInfo.m_rgchTitle );
		pItem->AddNewDemos( nNewDemoCount );
		// only used for thumbnails
		pItem->SetMapID( publishedFileInfo.m_nPublishedFileId );
		pItem->SetMapFileID( publishedFileInfo.m_hFile );
		pItem->SetThumbnailID( publishedFileInfo.m_hPreviewFile );
	}

}




const UGCFileRequest_t *CPuzzleMakerBetaTestList::GetThumbnailFileRequestForMap( UGCHandle_t mapThumbID )
{
	// FIXME: JDW
#if 0
	// find the map
	FOR_EACH_VEC( m_vMapThumbnailDownloads, itr )
	{
		if ( m_vMapThumbnailDownloads[itr]->fileHandle == mapThumbID )
		{
			return m_vMapThumbnailDownloads[itr];
		}
	}
#endif

	return NULL;
}


void CPuzzleMakerBetaTestList::OnThink()
{
	if ( m_menuType == MENU_BETA_RESULTS )
	{

	// handle processing the queries for map info
#if !defined( NO_STEAM )
		if ( m_vMapsToQuery.Count() && m_nActiveQueryID == k_UGCHandleInvalid )
		{
			// Iterate over the maps to query until we find a valid one 
			while ( 1 )
			{
				PublishedFileId_t mapID = m_vMapsToQuery.RemoveAtHead();
				ISteamRemoteStorage *pRemoteStorage = GetISteamRemoteStorage();
				SteamAPICall_t hSteamAPICall;
				if ( !pRemoteStorage || ( ( hSteamAPICall = pRemoteStorage->GetPublishedFileDetails( mapID ) ) == k_uAPICallInvalid ) )
				{
					// TODO: Handle the error case
					Msg( "Failed to query for details on UGC (%llu)\n", mapID );
					continue;
				}

				// Query for the details on this map and mark it as the active query
				m_callbackGetPublishedFileDetails.Set( hSteamAPICall, this, &CPuzzleMakerBetaTestList::Steam_OnGetPublishedFileDetails );
				m_nActiveQueryID = mapID;
				break;
			}
		}
#endif

		// add map panels if necessary
		if ( m_bPublishedDataDirty )
		{
			// mark no longer dirty
			m_bPublishedDataDirty = false;
			// create map panels as necessary
			FOR_EACH_VEC( m_vPublishedFileData, itr )
			{
				CreateMapPanel( m_vPublishedFileData[itr] );
			}

			// if there were no previous panel items, or none were selected
			if ( m_pBetaTestList->GetSelectedPanelItem() == NULL )
			{
				// select the first item in the list
				m_pBetaTestList->SelectPanelItem( 0 );
			}
		
		}

		// handle downloading the map thumbnails
		if ( m_bDownloadingThumbnails )
		{
			// update each thumbnail download request
			bool bDone = UpdateDownloads( m_vMapThumbnailDownloads );
			if ( bDone )
			{
				m_bDownloadingThumbnails = false;
			}
		}

		// FIXME: JDW
#if 0
		// handle thumbnails finishing their loads
		if ( m_flNextLoadThumbnailTime > 0.0f && Plat_FloatTime() >= m_flNextLoadThumbnailTime )
		{
			// get selected item
			CBetaMapListItem *pListItem = static_cast< CBetaMapListItem *>( m_pBetaTestList->GetSelectedPanelItem() );
			if ( pListItem )
			{
				const UGCFileRequest_t *pThumbInfo = GetThumbnailFileRequestForMap( pListItem->GetThumbnailID() );
				if ( pThumbInfo && pThumbInfo->fileRequest.GetStatus() == UGCFILEREQUEST_FINISHED )
				{
					// don't allow another screenshot request
					m_flNextLoadThumbnailTime = 0;
					m_nThumbnailImageId = -1;
				

					// build the name of our new thumbnail
					char szLocalFilename[MAX_PATH];
					V_snprintf( szLocalFilename, sizeof(szLocalFilename), "%s/%s", pThumbInfo->szTargetDirectory, pThumbInfo->szTargetFilename );

					// Start our load of this file
					StartAsyncScreenshotLoad( szLocalFilename );
				}
			}
		}
#endif
	} // end MENU_BETA_RESULTS
	else if ( m_menuType == MENU_BETA_PLAYER_LIST )
	{
		// see if we have to retry loading our avatar
		if ( m_flRetryAvatarTime > 0.0f )
		{
			if ( m_flRetryAvatarTime < gpGlobals->curtime )
			{
#if !defined( NO_STEAM )
				if ( m_pAvatar == NULL && m_pImgPlayerAvatar != NULL )
				{
					m_flRetryAvatarTime = -1.0f;
					m_pAvatar = BaseModUI::CUIGameData::Get()->AccessAvatarImage( m_currentPlayerID, BaseModUI::CUIGameData::kAvatarImageRequest, CGameUiAvatarImage::LARGE );
					m_pImgPlayerAvatar->SetImage( m_pAvatar );
					m_bDrawSpinner = false;
				}
#endif
			}
		}
	}

	// update the loading spinner
	ClockSpinner( m_bDrawSpinner );

	BaseClass::OnThink();
}



void CPuzzleMakerBetaTestList::PaintBackground( void )
{
	BaseClass::PaintBackground();

	DrawThumbnailImage();
}


void CPuzzleMakerBetaTestList::DrawThumbnailImage()
{
	if ( m_nThumbnailImageId < 0 )
	{
		 return;
	}

	int x, y, wide, tall;
	m_pThumbnailImage->GetBounds( x, y, wide, tall );

	// Draw a black border around the image
	surface()->DrawSetColor( Color( 0, 0, 0, 255 ) );
	surface()->DrawOutlinedRect( x-1, y-1, x+wide+1, y+tall+1 );

	// Draw the thumbnail image
	surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
	surface()->DrawSetTexture( m_nThumbnailImageId );
	surface()->DrawTexturedRect( x, y, x+wide, y+tall );
}


void CPuzzleMakerBetaTestList::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		int visibleButtons = FB_BBUTTON;
		if ( m_menuType == MENU_BETA_RESULTS )
		{
			visibleButtons |= FB_ABUTTON;
			pFooter->SetButtonText( FB_ABUTTON, "#PORTAL2_EditorMenu_DetailsButton" );
		}

		pFooter->SetButtons( visibleButtons );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
	}
}

void CPuzzleMakerBetaTestList::SetDataSettings( KeyValues *pSettings )
{
	//m_menuType = static_cast< MenuType_t >( pSettings->GetInt( "menuType", MENU_BETA_RESULTS ) );

	if ( m_menuType == MENU_BETA_PLAYER_LIST )
	{
		m_nThumbnailImageId = pSettings->GetInt( "ThumbnailID", -1 );
		m_nMapID = pSettings->GetUint64( "MapID", k_UGCHandleInvalid );
		V_strncpy( m_szMapName, pSettings->GetString( "MapName", "" ), sizeof( m_szMapName ) );

	}

	BaseClass::SetDataSettings( pSettings );
}


void CPuzzleMakerBetaTestList::OnEvent( KeyValues *pEvent )
{
	char const *pEventName = pEvent->GetName();

	if ( !V_stricmp( "OnNewDemoCountReceived", pEventName ) && m_menuType == MENU_BETA_RESULTS )
	{
		// cycle thru the key values
		for ( KeyValues *pKVHandles = pEvent->GetFirstSubKey(); pKVHandles; pKVHandles = pKVHandles->GetNextKey() )
		{
			PublishedFileId_t mapID = pKVHandles->GetUint64( "mapID", k_UGCHandleInvalid );
			int nDemoCount = pKVHandles->GetInt( "newDemoCount", 0 );

			// add the map to the list
			m_MapMapIDToNewDemoCount.Insert( mapID, nDemoCount );

			// get published file data on the maps - so we can get the thumbnail
			m_vMapsToQuery.Insert( mapID );
		}
	}
	else if ( !V_stricmp( "OnPlayersForMapReceived", pEventName ) && m_menuType == MENU_BETA_PLAYER_LIST )
	{
#if !defined( NO_STEAM )
		// cycle thru the key values
		if ( !steamapicontext || !steamapicontext->SteamUtils() )
		{
			Assert( 0 );
			Warning( "CPuzzleMakerBetaTestList::OnEvent(%s): Could not access Steam.\n", pEventName );
		}

		for ( KeyValues *pKVPlayers = pEvent->GetFirstSubKey(); pKVPlayers; pKVPlayers = pKVPlayers->GetNextKey() )
		{
			CSteamID playerID( pKVPlayers->GetInt( "playerID" ), steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual ); //pKVPlayers->GetInt( "playerID" );
			int nNewDemoCount = pKVPlayers->GetInt( "newDemoCount" );

			// create the panel item
			CBetaPlayerListItem *pItem = m_pBetaTestList->AddPanelItem< CBetaPlayerListItem >( "puzzlemaker_betaplayeritem" );
			if ( pItem && steamapicontext && steamapicontext->SteamFriends() )
			{
				pItem->SetPrimaryText( steamapicontext->SteamFriends()->GetFriendPersonaName( playerID ) );
				// TEMP!! - choosing up or down vote at random
				pItem->SetupLabels( playerID, !!nNewDemoCount, !!RandomInt(0,1) );
			}
			
		}

		// if we have any panelse
		if ( m_pBetaTestList->GetPanelItemCount() )
		{
			m_pBetaTestList->SelectPanelItem( 0 );
		}
		
#endif
	}

}

void CPuzzleMakerBetaTestList::ClockSpinner( bool bVisible )
{
	

	ImagePanel *pSpinner = m_menuType == MENU_BETA_PLAYER_LIST ? m_pImgAvatarWorkingAnim : m_pImgWorkingAnim;

	if ( pSpinner )
	{
		int nAnimFrame = ( uint64 )( Plat_FloatTime() * 10 );
		pSpinner->SetFrame( nAnimFrame );
		pSpinner->SetVisible( bVisible );
	}
}


void CPuzzleMakerBetaTestList::OnItemSelected( const char *pPanelName )
{
	if ( !m_bLayoutLoaded )
		return;

	if ( m_menuType == MENU_BETA_RESULTS )
	{
		// get selected item
		CBetaMapListItem *pListItem = static_cast< CBetaMapListItem *>( m_pBetaTestList->GetSelectedPanelItem() );

		// set up thumbnail image
		SetMapThumbnailImage( pListItem->GetMapID() );

		// draw the loading spinner while we wait
		m_bDrawSpinner = true;

		// set the map name label
		if ( m_pLblChamberName )
		{
			m_pLblChamberName->SetText( pListItem->GetPrimaryText() );
		}
	}
	else if ( m_menuType == MENU_BETA_PLAYER_LIST )
	{
		// get selected item
		CBetaPlayerListItem *pPlayerItem = static_cast< CBetaPlayerListItem *>( m_pBetaTestList->GetSelectedPanelItem() );

		// set up player avatar
#if !defined( NO_STEAM )
		SetPlayerInfo( pPlayerItem->GetPlayerID() );
#endif
		// set up player name
		SetPlayerName( pPlayerItem->GetPrimaryText() );
	}
}


void CPuzzleMakerBetaTestList::SetPlayerInfo( CSteamID playerID )
{
	// get the avatar and display it
	if ( !m_pImgPlayerAvatar || !m_pImgAvatarBorder )
	{
		return;
	}
#if !defined( NO_STEAM )
	if ( m_pAvatar )
	{
		BaseModUI::CUIGameData::Get()->AccessAvatarImage( m_currentPlayerID, BaseModUI::CUIGameData::kAvatarImageRelease );
		m_pAvatar = NULL;
	}

	m_currentPlayerID = playerID.ConvertToUint64();

	m_pAvatar = BaseModUI::CUIGameData::Get()->AccessAvatarImage( m_currentPlayerID, BaseModUI::CUIGameData::kAvatarImageRequest, CGameUiAvatarImage::LARGE );
	
	if ( m_pAvatar == NULL )
	{
		m_flRetryAvatarTime = gpGlobals->curtime + 1.0;
		m_bDrawSpinner = true;
		ClockSpinner( true );
	}
	
	m_pImgPlayerAvatar->SetImage( m_pAvatar );
	m_pImgPlayerAvatar->SetVisible( true );
	m_pImgAvatarBorder->SetVisible( true );

#endif
}

void CPuzzleMakerBetaTestList::SetPlayerName( const char *pPlayerName )
{
	if ( !m_pLblPlayerName )
	{
		return;
	}

	// get the localized version of "Tested by "
	char szTestedByText[16];
	char szFullPlayerNameText[128];
	wchar_t *pTestedByText = g_pVGuiLocalize->Find( "#PORTAL2_EditorMenu_TestedByLabel" );
	V_wcstostr( pTestedByText, V_wcslen( pTestedByText ) + 1, szTestedByText, sizeof( szTestedByText ) );
	V_snprintf( szFullPlayerNameText, sizeof( szFullPlayerNameText ), "%s %s", szTestedByText, pPlayerName );

	m_pLblPlayerName->SetText( szFullPlayerNameText );
	m_pLblPlayerName->SetVisible( true );
}


void CPuzzleMakerBetaTestList::SetMapThumbnailImage( UGCHandle_t mapID )
{
	// stop any current transition
	//m_flNextLoadThumbnailTime = 0.0f;
#if !defined( NO_STEAM )
	m_nThumbnailImageId = -1;
	m_flNextLoadThumbnailTime = Plat_FloatTime() + 0.3f;
#endif
}


bool CPuzzleMakerBetaTestList::MapInDownloadList( UGCHandle_t mapID )
{
	// FIXME: JDW
#if 0
	FOR_EACH_VEC( m_vMapDownloads, itr )
	{
		if ( m_vMapDownloads[itr]->fileHandle == mapID )
		{
			return true;
		}
	}
#endif

	return false;
}


bool CPuzzleMakerBetaTestList::StartAsyncScreenshotLoad( const char *pThumbnailFilename )
{
	if ( m_hAsyncControl )
	{
		FSAsyncStatus_t status = g_pFullFileSystem->AsyncStatus( m_hAsyncControl );
		switch ( status )
		{
		case FSASYNC_STATUS_PENDING:
		case FSASYNC_STATUS_INPROGRESS:
		case FSASYNC_STATUS_UNSERVICED:
			{
				// i/o in progress, caller must retry
				return false;
			}
		}

		// Finished
		g_pFullFileSystem->AsyncFinish( m_hAsyncControl );
		g_pFullFileSystem->AsyncRelease( m_hAsyncControl );
		m_hAsyncControl = NULL;
	}

	// must do this at this point on the main thread to ensure eviction
	if ( m_nThumbnailImageId != -1 )
	{
		// evict prior screenshot
		surface()->DestroyTextureID( m_nThumbnailImageId );
		m_nThumbnailImageId = -1;
	}

	return LoadThumbnailFromContainer( pThumbnailFilename );
}


void CPuzzleMakerBetaTestList::ScreenshotLoaded( const FileAsyncRequest_t &asyncRequest, int nNumReadBytes, FSAsyncStatus_t err )
{
	/*CBetaMapListItem *pMapItem = static_cast< CBetaMapListItem *>( m_pBetaTestList->GetSelectedPanelItem() );
	if ( !pMapItem )
		return;*/


	if ( err == FSASYNC_OK )
	{
		int nWidth, nHeight;
		CUtlBuffer srcBuf( asyncRequest.pData, nNumReadBytes, CUtlBuffer::READ_ONLY );
		CUtlBuffer dstBuf;

		// Read the preview JPEG to RGB
		if ( ImgUtl_ReadJPEGAsRGBA( srcBuf, dstBuf, nWidth, nHeight ) == CE_SUCCESS )
		{
			// success
			if ( m_nThumbnailImageId == -1 )
			{
				// Create a procedural texture id
				m_nThumbnailImageId = vgui::surface()->CreateNewTextureID( true );
			}

			// Write this to the texture so we can draw it
			surface()->DrawSetTextureRGBALinear( m_nThumbnailImageId, (const unsigned char *) dstBuf.Base(), nWidth, nHeight );

			// Free our resulting image
			dstBuf.Purge();

			// stop drawing the load spinner
			m_bDrawSpinner = false;
		}
	}

	//
}


void BetaMapThumbLoaded( const FileAsyncRequest_t &asyncRequest, int numReadBytes, FSAsyncStatus_t err )
{
	CPuzzleMakerBetaTestList *pDialog = static_cast< CPuzzleMakerBetaTestList *>( CBaseModPanel::GetSingleton().GetWindow( WT_BETATESTLIST ) );
	if ( pDialog )
	{
		pDialog->ScreenshotLoaded( asyncRequest, numReadBytes, err );
	}
}


bool CPuzzleMakerBetaTestList::LoadThumbnailFromContainer( const char *pThumbnailFilename )
{
	//m_ThumbnailFilename = pThumbnailFilename;

	FileAsyncRequest_t request;
	request.pszFilename = pThumbnailFilename;
	request.pfnCallback = ::BetaMapThumbLoaded;
	request.pContext = NULL;
	request.flags = FSASYNC_FLAGS_FREEDATAPTR;

	// schedule the async operation
	g_pFullFileSystem->AsyncRead( request, &m_hAsyncControl );

	return true;
}


#if !defined( NO_STEAM )


void CPuzzleMakerBetaTestList::Steam_OnEnumerateSubscribedPuzzles( RemoteStorageEnumerateUserSubscribedFilesResult_t *pResult, bool bError )
{
	// make sure we succeeded
	if ( bError || pResult->m_eResult != k_EResultOK )
	{
		Warning( "Unable to enumerate user's published puzzles!\n" );
		return;
	}

	// let's see what we get out
	const int nNumMaps = pResult->m_nResultsReturned;
	DevMsg( "EnumerateSubscribedPuzzles: Number of maps: %d\n", nNumMaps );

	for ( int i = 0; i < nNumMaps; ++i )
	{
		DevMsg( "EnumerateSubscribedPuzzles: FileID: %llu\n", pResult->m_rgPublishedFileId[i] );
	}
}


void CPuzzleMakerBetaTestList::Steam_OnEnumeratePublishedPuzzles( RemoteStorageEnumerateUserPublishedFilesResult_t *pResult, bool bError )
{
	// make sure we succeeded
	if ( bError || pResult->m_eResult != k_EResultOK )
	{
		Warning( "Unable to enumerate user's published puzzles!\n" );
		return;
	}

	 // let's see what we get out
	const int nNumMaps = pResult->m_nResultsReturned;
	DevMsg( "EnumeratePublishedPuzzles: Number of maps: %d\n", nNumMaps );

	for ( int i = 0; i < nNumMaps; ++i )
	{
		DevMsg( "EnumeratePublishedPuzzles: FileID: %llu\n", pResult->m_rgPublishedFileId[i] );
	}
}


void CPuzzleMakerBetaTestList::Steam_OnGetPublishedFileDetails( RemoteStorageGetPublishedFileDetailsResult_t *pResult, bool bError )
{
	// Error or success, we're done with this file query
	m_nActiveQueryID = k_UGCHandleInvalid;

	if ( bError || pResult->m_eResult != k_EResultOK )
	{
		Warning( "Unable to retrieve details for file: %llu!\n", pResult->m_nPublishedFileId );
		return;
	}

	// set up a new published file info
	int nIndex = m_vPublishedFileData.AddToTail();

	if ( nIndex != m_vPublishedFileData.InvalidIndex() )
	{
		m_vPublishedFileData[nIndex].m_nPublishedFileId = pResult->m_nPublishedFileId;
		m_vPublishedFileData[nIndex].m_eVisibility = pResult->m_eVisibility;
		m_vPublishedFileData[nIndex].m_hFile = pResult->m_hFile;
		m_vPublishedFileData[nIndex].m_hPreviewFile = pResult->m_hPreviewFile;
		m_vPublishedFileData[nIndex].m_rtimeCreated = pResult->m_rtimeCreated;
		m_vPublishedFileData[nIndex].m_rtimeUpdated = pResult->m_rtimeUpdated;
		m_vPublishedFileData[nIndex].m_ulSteamIDOwner = pResult->m_ulSteamIDOwner;
		memcpy( m_vPublishedFileData[nIndex].m_rgchTitle, pResult->m_rgchTitle, ARRAYSIZE( pResult->m_rgchTitle ) );
	}

	// let the panel know we have new published map data
	m_bPublishedDataDirty = true;

	//
	// Build the file request for this map
	//
	// handle map downloads
	/*if ( !MapInDownloadList( pResult->m_nPublishedFileId ) )
	{
		UGCFileRequest_t *pMapDownloadRequest = new UGCFileRequest_t;
		pMapDownloadRequest->fileHandle = pResult->m_nPublishedFileId;
		char szDirectory[64];
		V_snprintf( szDirectory, sizeof(szDirectory), "%s/%llu", COMMUNITY_MAP_PATH, pResult->m_nPublishedFileId );
		V_strncpy( pMapDownloadRequest->szTargetDirectory, szDirectory, sizeof(pMapDownloadRequest->szTargetDirectory) );
		const UGCFileRequest_t *pRequest = CBaseModPanel::GetSingleton().AddUGCFileRequest( pMapDownloadRequest );
		if ( pRequest == NULL )
		{
			pRequest = CBaseModPanel::GetSingleton().GetUGCFileRequest( pMapDownloadRequest->fileHandle );
			delete pMapDownloadRequest;
			pMapDownloadRequest = pRequest;
		}
		m_vMapDownloads.AddToTail( pMapDownloadRequest );
		m_bDownloadingMaps = true;
	}*/

	//
	// Build the file request for this map's thumbnail
	//

	/*
	UGCFileRequest_t *pThumbFileInfo = new UGCFileRequest_t;
	pThumbFileInfo->fileHandle = pResult->m_hPreviewFile;
	V_snprintf( pThumbFileInfo->szTargetDirectory, ARRAYSIZE(pThumbFileInfo->szTargetDirectory), "%s/%llu", COMMUNITY_MAP_PATH, pResult->m_hFile );
	V_strncpy( pThumbFileInfo->szTargetFilename,  CFmtStr( "%s%d.jpg", COMMUNITY_MAP_THUMBNAIL_PREFIX, pResult->m_nPublishedFileId ), ARRAYSIZE(pThumbFileInfo->szTargetFilename) );

	pThumbFileInfo->nTargetID = pResult->m_nPublishedFileId;
	pThumbFileInfo->unLastUpdateTime = 0;
	pThumbFileInfo->bForceUpdate = false;
	*/

	// FIXME: JDW
	// const UGCFileRequest_t *pThumbRequest = CBaseModPanel::GetSingleton().CreateThumbnailFileRequest( (*pResult) );

	/*
	const UGCFileRequest_t *pRequest = CBaseModPanel::GetSingleton().AddUGCFileRequest( pThumbRequest );
	if ( pRequest == NULL )
	{
		Assert( 0 );
		// FIXME: JDW This isn't legal!
		pRequest = CBaseModPanel::GetSingleton().GetUGCFileRequest( pThumbFileInfo->fileHandle );
		delete pThumbFileInfo;
		pThumbFileInfo = pRequest;
	}
	*/

	// m_vMapThumbnailDownloads.AddToTail( pThumbRequest );
	// m_bDownloadingThumbnails = true;

}
#endif


CPublishedListItem::CPublishedListItem( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName )
{
	m_pImgChamberRating = NULL;
	m_nRating = 0;
}


void CPublishedListItem::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/BaseModUI/puzzlemaker_publisheditem.res" );

	m_pImgChamberRating = static_cast< ImagePanel * >( FindChildByName( "ImgChamberRating" ) );
}


void CPublishedListItem::PaintBackground()
{
	// For some reason using the base PaintBackground wants to draw the text twice
	// so don't call the base function and just let the panel draw the text
	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );

	// if we're highlighted, background
	if ( HasFocus() )
	{
		surface()->DrawSetColor( m_CursorColor );
	}
	else if ( IsSelected() )
	{
		surface()->DrawSetColor( m_LostFocusColor );
	}
	else if ( HasMouseover() )
	{
		surface()->DrawSetColor( m_MouseOverCursorColor );
	}
	else
	{
		surface()->DrawSetColor( m_BaseColor );
	}
	surface()->DrawFilledRect( 0, 0, wide, tall );

	// set the colors for the labels
	if (m_pLblName )
	{
		if ( HasFocus() || IsSelected() || HasMouseover() )
		{
			m_pLblName->SetFgColor( m_FocusColor );
			//m_pLblChamberStatus->SetFgColor( m_FocusColor );
		}
		else
		{
			m_pLblName->SetFgColor( m_TextColor );
			//m_pLblChamberStatus->SetFgColor( m_TextColor );
		}
	}

	//DrawListItemLabel( m_pLblChamberStatus );
}


void CPublishedListItem::SetChamberRating( int nRating )
{
	if ( !m_pImgChamberRating )
	{
		return;
	}

	// set the name of the image
	char szImgName[8];

	V_snprintf( szImgName, sizeof( szImgName ), "Rating%d", nRating );
	m_pImgChamberRating->SetImage( szImgName );
}


CBetaPlayerListItem::CBetaPlayerListItem( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName )
{
	m_pImgThumbRating = NULL;
	m_pLblStatus = NULL;
	m_bNewTests = false;
	m_bUpvoted = false;
}


void CBetaPlayerListItem::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/BaseModUI/puzzlemaker_betaplayeritem.res" );

	m_pImgThumbRating = static_cast< ImagePanel * >( FindChildByName( "ImgThumbRating" ) );
	m_pLblStatus = static_cast< Label * >( FindChildByName( "LblStatus" ) );
}


void CBetaPlayerListItem::PaintBackground()
{
	// For some reason using the base PaintBackground wants to draw the text twice
	// so don't call the base function and just let the panel draw the text
	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );

	// if we're highlighted, background
	if ( HasFocus() )
	{
		surface()->DrawSetColor( m_CursorColor );
	}
	else if ( IsSelected() )
	{
		surface()->DrawSetColor( m_LostFocusColor );
	}
	else if ( HasMouseover() )
	{
		surface()->DrawSetColor( m_MouseOverCursorColor );
	}
	else
	{
		surface()->DrawSetColor( m_BaseColor );
	}
	surface()->DrawFilledRect( 0, 0, wide, tall );

	// set the colors for the labels
	if ( m_pLblName && m_pLblStatus && m_pImgThumbRating )
	{
		if ( HasFocus() || IsSelected() || HasMouseover() )
		{
			m_pLblName->SetFgColor( m_FocusColor );
			m_pLblStatus->SetFgColor( m_FocusColor );
			SetImageHighlighted( true );
		}
		else
		{
			m_pLblName->SetFgColor( m_TextColor );
			m_pLblStatus->SetFgColor( m_TextColor );
			SetImageHighlighted( false );
		}
	}

	//DrawListItemLabel( m_pLblChamberStatus );
}



#if !defined( NO_STEAM )
void BaseModUI::CBetaPlayerListItem::SetupLabels( CSteamID &playerID, bool bNewTest, bool bUpvoted )
{
	if ( !m_pImgThumbRating || !m_pLblStatus )
	{
		return;
	}

	m_bNewTests = bNewTest;
	m_pLblStatus->SetVisible( bNewTest );

	m_bUpvoted = bUpvoted;

	m_playerID = playerID;

}
#endif


void CBetaPlayerListItem::SetImageHighlighted( bool bHighlighted )
{
	if ( !m_pImgThumbRating )
		return;

	if ( bHighlighted )
	{
		if ( m_bUpvoted )
		{
			m_pImgThumbRating->SetImage( "thumbs_up_nobg_focus" );
		}
		else
		{
			m_pImgThumbRating->SetImage( "thumbs_down_nobg_focus" );
		}
	}
	else
	{
		if ( m_bUpvoted )
		{
			m_pImgThumbRating->SetImage( "thumbs_up_nobg_default" );
		}
		else
		{
			m_pImgThumbRating->SetImage( "thumbs_down_nobg_default" );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Receive messages to do with retrieving new demo count
//-----------------------------------------------------------------------------
class CGCJobPlaytestRetrieveDemoPlayersForMapResponse : public GCSDK::CGCClientJob
{
public:
	CGCJobPlaytestRetrieveDemoPlayersForMapResponse( GCSDK::CGCClient *pClient ) : GCSDK::CGCClientJob( pClient ) {}

	virtual bool BYieldingRunGCJob( GCSDK::IMsgNetPacket *pNetPacket )
	{		
		GCSDK::CProtoBufMsg<CMsgPlaytestRetrieveDemoPlayersForMapResponse> msg( pNetPacket );
		KeyValues *pKV = new KeyValues( "OnPlayersForMapReceived" );
		// loop thru repeated UGCHandles in message
		int nPlayerCount = msg.Body().player_id_size();
		DevMsg("PlayersForMapResponseReceived: %d players.\n", nPlayerCount );
		for ( int i = 0; i < nPlayerCount; ++i )
		{
			KeyValues *pHandleKV = pKV->CreateNewKey();
			DevMsg( " Player %d: %d\n", i, msg.Body().player_id( i ) );
			pHandleKV->SetInt( "playerID", msg.Body().player_id( i ) );
			DevMsg( " New Demo Count: %d\n", msg.Body().new_count( i ) );
			pHandleKV->SetInt( "newDemoCount", msg.Body().new_count( i ) );
		}
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( pKV );
		return true;
	}
};

GC_REG_JOB( GCSDK::CGCClient, CGCJobPlaytestRetrieveDemoPlayersForMapResponse, "CGCJobPlaytestRetrieveDemoPlayersForMapResponse", k_EMsgGCPlaytestRetrieveDemoPlayersForMapResponse, GCSDK::k_EServerTypeGCClient );


#endif // PORTAL2_PUZZLEMAKER
