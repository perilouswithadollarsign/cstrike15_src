//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#include "cbase.h"

#if defined( PORTAL2_PUZZLEMAKER )

#include "vplaytestdemosdialog.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "EngineInterface.h"
#include "IGameUIFuncs.h"
#include "gameui_util.h"
#include "vgui/ISurface.h"
#include "VGenericConfirmation.h"
#include "vgui/ilocalize.h"
#include "vgui_controls/scrollbar.h"
#include "vgui_controls/imagepanel.h"
#include "gameui/portal2/vdialoglistbutton.h"
#include "vpvplobby.h"
#include "vgui/portal_leaderboard_graph_panel.h"
#include "gameui/portal2/vportalchallengestatspanel.h"
#include "GameUI_Interface.h"
#include "c_user_message_register.h"
#include "c_portal_player.h"
#include "vgui/IInput.h"

#include "portal_mp_gamerules.h"
#include "gamerules.h"

#include "demofile/demoformat.h"	 // demoheader_t

#include "FileSystem.h"

#include "gc_clientsystem.h"
#include "econ_gcmessages.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;
using namespace BaseModUI;

namespace BaseModUI
{

//=============================================================================
CPlaytestDemosDialog::CPlaytestDemosDialog( Panel *pParent, const char *pPanelName ):
	BaseClass( pParent, pPanelName ), 
	m_autodelete_pMapDemoValues( (KeyValues*) NULL )
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	// Subscribe to event notifications
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	m_pMapList = new GenericPanelList( this, "MapList", GenericPanelList::ISM_PERITEM | GenericPanelList::ISM_ALPHA_INVISIBLE );
	m_pMapList->SetPaintBackgroundEnabled( false );

	m_pDemoFileList = new GenericPanelList( this, "StatList", GenericPanelList::ISM_PERITEM | GenericPanelList::ISM_ALPHA_INVISIBLE );
	m_pDemoFileList->SetPaintBackgroundEnabled( false );

	m_pPlayerLabel = new Label( this, "LblPlayerName", "" );
	m_pTimeLabel = new Label( this, "LblTime", "" );

	m_pCloudImage = NULL;
	m_pDwnCloudImage = NULL;
	m_pSpinner = NULL;

	m_bDownloadingDemos = false;
	m_bDownloadingMaps = false;

	m_flNextDemoRequest = 0.0f;

	ResetLogging();

	V_strncpy( m_ResourceName, "Resource/UI/BaseModUI/playtestdemosdialog.res",sizeof( m_ResourceName ) );
	V_strncpy( m_szCurrentDemoFile, "", sizeof(m_szCurrentDemoFile) );
	V_strncpy( m_szCurrentMap, "", sizeof(m_szCurrentMap) );

	m_pMapDemoValues = new KeyValues( "Maps" );
	m_autodelete_pMapDemoValues.Assign( m_pMapDemoValues );

	SetDialogTitle( "#PORTAL2_CommunityPuzzle_WatchPlaytests", NULL, false, 0, 0, 0 );

	SetFooterEnabled( true );
	UpdateFooter();
}

//=============================================================================
CPlaytestDemosDialog::~CPlaytestDemosDialog()
{
	if ( m_pMapList )
	{
		delete m_pMapList;
	}

	if ( m_pDemoFileList )
	{
		delete m_pDemoFileList;
	}

	// Free up any demos we were downloading
	FreeDemoDownloads();

	// Delete the memory
	m_DemoUploads.Purge();

	// Don't hold onto map handles any longer
	// FIXME: Tell the file request manager to forget them as well?
	m_vMapDownloadHandles.Purge();

	// Unsubscribe from event notifications
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
}


//=============================================================================
void CPlaytestDemosDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// use cloud images to depict cloud activity
	m_pCloudImage = dynamic_cast< ImagePanel* >( FindChildByName( "PnlCloudPic" ) );
	if ( m_pCloudImage )
	{
		m_pCloudImage->SetVisible( false );
	}

	m_pDwnCloudImage = dynamic_cast< ImagePanel* >( FindChildByName( "PnlDwnCloudPic" ) );
	if ( m_pDwnCloudImage )
	{
		m_pDwnCloudImage->SetVisible( false );
	}

	m_pSpinner = dynamic_cast< ImagePanel* >( FindChildByName( "WorkingAnim" ) );
	if ( m_pSpinner )
	{
		m_pSpinner->SetVisible( false );
	}


	m_pMapList->SetScrollBarVisible( true );
	m_pMapList->SetScrollArrowsVisible( false );

	m_pDemoFileList->SetScrollBarVisible( true );
	m_pDemoFileList->SetScrollArrowsVisible( false );

	m_pMapList->SelectPanelItem( 0 );

	UpdateFooter();
}

//=============================================================================
void CPlaytestDemosDialog::Activate()
{
	BaseClass::Activate();

	ResetLogging();

	DownloadDemos();

	//EnumerateCloudFiles();

	m_flNextDemoRequest = gpGlobals->curtime + 1.0f;

	UpdateFooter();
}


void CPlaytestDemosDialog::OnEvent( KeyValues *pEvent )
{
	char const *pEventName = pEvent->GetName();


	// see if we've received a demo handle
	if ( !V_stricmp( "OnDemoHandleReceived", pEventName ) )
	{

		// cycle thru the key values
		for ( KeyValues *pKVHandles = pEvent->GetFirstSubKey(); pKVHandles; pKVHandles = pKVHandles->GetNextKey() )
		{
			CreateDownloadRequest( pKVHandles ); 
		}

		// no need to continue requesting updates
		m_flNextDemoRequest = 0.0f;
	}
	else if ( !V_stricmp( "OnNewDemoCountReceived", pEventName ) )
	{
		// cycle thru the key values
		for ( KeyValues *pKVHandles = pEvent->GetFirstSubKey(); pKVHandles; pKVHandles = pKVHandles->GetNextKey() )
		{
			PublishedFileId_t mapID = pKVHandles->GetUint64( "mapID", 0 );
			int nDemoCount = pKVHandles->GetInt( "newDemoCount", 0 );

			DevMsg( "New Demos for map: %llu  Count: %d\n", mapID, nDemoCount );
		}
	}

}


//=============================================================================
void CPlaytestDemosDialog::OnKeyCodePressed( KeyCode code )
{
	int iSlot = GetJoystickForCode( code );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( iSlot );

	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		break;

	case KEY_ENTER:
	case KEY_XBUTTON_A:
		{
			// if it is new, tell GC it is no longer new
			CDemoListItem *pItem = static_cast< CDemoListItem *>( m_pDemoFileList->GetSelectedPanelItem() );
			if ( pItem && pItem->IsNewDemo() )
			{
				GCSDK::CProtoBufMsg<CMsgPlaytestMarkDemoNotNew>	msgDemo( k_EMsgGCPlaytestMarkDemoNotNew );
				msgDemo.Body().set_ugc_handle( pItem->GetUGCHandle() );
				GCClientSystem()->BSendMessage( msgDemo );
			}
			// play the current demo file
			char szCommand[ MAX_PATH ];
			V_snprintf( szCommand, sizeof(szCommand), "playdemo %s", m_szCurrentDemoFile );
			engine->ClientCmd_Unrestricted( szCommand );
		}
		break;

	case KEY_XBUTTON_X:
		ClearCloudPlaytests();
		break;

	case KEY_XBUTTON_Y:
		// delete selected demo
		//ClearCloudPlaytests();
		DeleteSelectedDemo();
		break;

	case KEY_UP:
	case KEY_XBUTTON_UP:
	case KEY_XSTICK1_UP:
	case KEY_XSTICK2_UP:
		break;

	case KEY_DOWN:
	case KEY_XBUTTON_DOWN:
	case KEY_XSTICK1_DOWN:
	case KEY_XSTICK2_DOWN:
		break;

	case KEY_LEFT:
	case KEY_XBUTTON_LEFT:
	case KEY_XSTICK1_LEFT:
	case KEY_XSTICK2_LEFT:
		break;

	case KEY_RIGHT:
	case KEY_XBUTTON_RIGHT:
	case KEY_XSTICK1_RIGHT:
	case KEY_XSTICK2_RIGHT:
		break;
	}
	BaseClass::OnKeyCodePressed(code);
}


//=============================================================================
void CPlaytestDemosDialog::OnCommand(const char *command)
{
	if ( !V_stricmp( "Cancel", command ) || !V_stricmp( "Back", command ) )
	{
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		return;
	}
	
	UpdateFooter();

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: Handle downloading our demos from the UGC
//	Return: Whether or not the downloads are done
//-----------------------------------------------------------------------------
bool CPlaytestDemosDialog::DemoDownloadsComplete( void )
{
	// If we're not marked as doing work, don't bother
	if ( m_bDownloadingDemos == false )
		return true;

	// Handle logging
	if ( !bProcessesLogged[ DOWNLOAD_DEMOS ] )
	{
		Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] UpdateDemoDownloads:: Downloading demo files\n" );
		bProcessesLogged[ DOWNLOAD_DEMOS ] = true;
	}
	
	// Work through each demo and wait for it to finish up
	FOR_EACH_VEC( m_vDemoDownloads, itr )
	{
		// See if any of our downloads are still active
		UGCFileRequestStatus_t status = WorkshopManager().GetUGCFileRequestStatus( m_vDemoDownloads[itr]->m_demoHandle );
		if ( status != UGCFILEREQUEST_FINISHED && status != UGCFILEREQUEST_ERROR )
			return false;
	}

	// Demos are done downloading
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handle downloading our maps from the UGC
//	Return: Whether or not the downloads are done
//-----------------------------------------------------------------------------
bool CPlaytestDemosDialog::MapDownloadsComplete( void )
{
	// If we think we were done last frame, we are
	if ( m_bDownloadingMaps == false )
		return true;

	// Handle logging
	if ( !bProcessesLogged[ DOWNLOAD_MAPS ] )
	{
		Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] OnThink:: Downloading maps\n" );
		bProcessesLogged[ DOWNLOAD_MAPS ] = true;
	}
	// update each map download request
	FOR_EACH_VEC( m_vMapDownloadHandles, itr )
	{
		UGCFileRequestStatus_t status = WorkshopManager().GetUGCFileRequestStatus( m_vMapDownloadHandles[itr] );
		if ( status != UGCFILEREQUEST_FINISHED && status != UGCFILEREQUEST_ERROR )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handle downloading our maps from the UGC
//	Return: Whether or not the downloads are done
//-----------------------------------------------------------------------------
void CPlaytestDemosDialog::UpdateDemoUploads( void )
{
	// Make sure there are uploads to update
	if ( m_DemoUploads.Count() == 0 )
		return;	

	// Handle logging
	if ( !bProcessesLogged[ UPLOAD_DEMOS ] )
	{
		Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] OnThink:: Processing demo uploads.\n" );
		bProcessesLogged[ UPLOAD_DEMOS ] = true;
	}

	// Turn on the cloud image while we work
	if ( m_pCloudImage )
	{
		m_pCloudImage->SetVisible( true );
	}

	// update the current upload request we're working on
	bool bDone = UpdateFileUploadRequest( m_DemoUploads.Tail() );
	if ( bDone )
	{
		// Remove the upload request and move to the next
		CDemoInfo *pCompletedUpload = m_DemoUploads.RemoveAtTail();
		
		// Remove this from the file request manager
		// WorkshopManager().DeleteUGCFileRequest( pCompletedUpload->m_demoHandle );
		
		delete pCompletedUpload;
	}

	// NOTE: We'll start on the next make on the run through
}

void CPlaytestDemosDialog::FreeDemoDownloads( void )
{
	// Kill all our data
	m_vDemoDownloads.PurgeAndDeleteElements();
}

//=============================================================================
void CPlaytestDemosDialog::OnThink()
{

	// if we need to do another request to download demos and our delay between requests is up,
	// initiate another demo download request
	if ( m_flNextDemoRequest > 0.0f && m_flNextDemoRequest <= gpGlobals->curtime )
	{
		DownloadDemos();
	}

	// First, wait for our demo downloads to complete
	if ( m_bDownloadingDemos )
	{
		// Start with these objects on (we'll turn them off when we're done)
		if ( m_pDwnCloudImage )
		{
			m_pDwnCloudImage->SetVisible( true );
		}

		ClockSpinner( true );

		// If we've just completed downloading everything this frame, then clean up our work
		if ( DemoDownloadsComplete() )
		{
			Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] OnThink:: Done downloading all demo files\n" );

			FOR_EACH_VEC( m_vDemoDownloads, itr )
			{
				// if not in local cloud
				if ( !m_vDemoDownloads[ itr ]->m_bInLocalCloud )
				{
					// upload to local cloud
					UploadDemo( m_vDemoDownloads[ itr ] );
				}
			}


			// turn off the cloud image
			if ( m_pDwnCloudImage )
			{
				m_pDwnCloudImage->SetVisible( false );
			}

			// turn off the spinner
			ClockSpinner( false );

			// set the list of demos
			SetDemoFileInfos();
			UpdateDemoList();
			UpdateFooter();
			
			// we're done with the download requests - get rid of them
			FreeDemoDownloads();
			InvalidateLayout( true );

			m_bDownloadingDemos = false;
		}
	}
	
	// If we're not downloading demos, then see if we're downloading maps
	if ( m_bDownloadingDemos == false && m_bDownloadingMaps )
	{
		// If we finished this frame, update our visuals
		if ( MapDownloadsComplete() )
		{
			Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] OnThink:: Done downloading maps\n" );
			EnableFinishedMaps();
			m_bDownloadingMaps = false;
		}
	}

	// Update our demo uploads if we're not doing anything else
	if ( m_bDownloadingDemos == false && m_bDownloadingMaps == false )
	{
		if ( m_DemoUploads.Count() != 0 )
		{
			UpdateDemoUploads();
		}
		else
		{
			// No demos uploading, hide the cloud image
			if ( m_pCloudImage )
			{
				m_pCloudImage->SetVisible( false );
			}
		}
	}

	BaseClass::OnThink();
}


//=============================================================================
void CPlaytestDemosDialog::OnClose()
{
	BaseClass::OnClose();
}


//=============================================================================
void CPlaytestDemosDialog::SetDataSettings( KeyValues *pSettings )
{
	BaseClass::SetDataSettings( pSettings );
}


//=============================================================================
void CPlaytestDemosDialog::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BASEMODPANEL_SINGLETON.GetFooterPanel();
	if ( pFooter )
	{
		int visibleButtons = FB_BBUTTON /*| FB_XBUTTON*/;
		//pFooter->SetButtonText( FB_XBUTTON, "TEMP Clear Cloud Playtests" );

		// if there is a valid demo file selected
		if ( V_stricmp("", m_szCurrentDemoFile) != 0 )
		{
			visibleButtons |= FB_ABUTTON | FB_YBUTTON;
			pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_SignIn_SignInPlay" );
			pFooter->SetButtonText( FB_YBUTTON, "#PORTAL2_CommunityPuzzle_DeletePlaytest" );
		}

		pFooter->SetButtons( visibleButtons );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
	}
}


void CPlaytestDemosDialog::OnMousePressed( vgui::MouseCode code )
{
	BaseClass::OnMousePressed( code );
}

void CPlaytestDemosDialog::ClockSpinner( bool bVisible )
{
	if ( m_pSpinner )
	{
		int nAnimFrame = ( uint64 )( Plat_FloatTime() * 10 );
		m_pSpinner->SetFrame( nAnimFrame );
		m_pSpinner->SetVisible( bVisible );
	}
}


void CPlaytestDemosDialog::ClearDemoInfo()
{
	m_pMapDemoValues->Clear();
}


void CPlaytestDemosDialog::SetDemoFileInfos()
{
	// make sure there are no existing demo file info lying around
	ClearDemoInfo();

	// loop thru the downloaded demos
	FOR_EACH_VEC( m_vDemoDownloads, itr )
	{
		// Get the status of this download
		UGCHandle_t hDemoHandle = m_vDemoDownloads[itr]->m_demoHandle;
		UGCFileRequestStatus_t status = WorkshopManager().GetUGCFileRequestStatus( hDemoHandle );

		// if the demo errored out, skip it
		if ( status == UGCFILEREQUEST_ERROR )
		{
			Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] SetDemoFileInfos:: Skipping a demo with Error status.\n" );
			continue;
		}
		// get the full file name with path, and the base file name
		char szFullPath[ MAX_PATH ];
		WorkshopManager().GetUGCFullPath( hDemoHandle, szFullPath, ARRAYSIZE(szFullPath) );

		// Get the filename minus the extension
		char szFilename[MAX_PATH];
		V_FileBase( szFullPath, szFilename, ARRAYSIZE(szFilename) );

		// get the header data from the demo
		KeyValues *pDemoFileData = new KeyValues( szFilename );
		engine->GetDemoHeaderInfo( szFullPath, pDemoFileData );

		// add a subkey for the UGCHandle of the demo
		pDemoFileData->SetUint64("handle", hDemoHandle );
		
		// add a subkey for if the demo is marked new
		pDemoFileData->SetBool( "newDemo", m_vDemoDownloads[itr]->m_bNewDemo );

		//KeyValuesDumpAsDevMsg( pDemoFileData );

		// find map in key values (create it if it doesn't exist)
		KeyValues *pMapData = m_pMapDemoValues->FindKey( pDemoFileData->GetString( "map" ), true );
		
		// add the demo file subkey to that map
		pMapData->AddSubKey( pDemoFileData );
	}
}


void CPlaytestDemosDialog::SetCurrentMap( const char *pMap, CDemoListItem *pMapItem )
{
	// even if the map name is the same, we may have new
	// demos, so still do the update

	// make sure we have good pointers
	if ( pMap == NULL || pMapItem == NULL )
	{
		return;
	}

	// set the new map name
	V_strncpy( m_szCurrentMap, pMap, sizeof( m_szCurrentMap ) );
	
	// clear the old demo file list
	m_pDemoFileList->RemoveAllPanelItems();

	// clear the old selected demo file
	m_szCurrentDemoFile[0] = '\0';

	// if the map is disabled
	if ( pMapItem->IsDisabled() )
	{
		// leave it blank, update the footer to not have the play button
		UpdateFooter();
		return;
	}

	// set the demo file items for that map
	KeyValues *pMapDemos = m_pMapDemoValues->FindKey( m_szCurrentMap );
	for ( KeyValues *pDemoKV = pMapDemos->GetFirstSubKey(); pDemoKV != NULL; pDemoKV = pDemoKV->GetNextKey() )
	{
		// add the item
		CDemoListItem *pItem = m_pDemoFileList->AddPanelItem< CDemoListItem >( "newgame_chapteritem" );
		if ( pItem )
		{
			// tell the item its demo handle
			pItem->SetUGCHandle( pDemoKV->GetUint64( "handle", k_UGCHandleInvalid ) );
			// if this is a new demo
			bool bNewDemo = pDemoKV->GetBool( "newDemo", true );
			pItem->SetAsNew( bNewDemo );


			pItem->SetLabelText( pDemoKV->GetName(), false );
		}
	}

	// if there are any demo files
	if ( m_pDemoFileList->GetPanelItemCount() )
	{
		// select the first in the list
		m_pDemoFileList->SelectPanelItem( 0 );
	}
}


void CPlaytestDemosDialog::SetCurrentDemo( const char *pDemo, CDemoListItem *pMapItem )
{
	// if the same as the current demo file do nothing
	if ( V_stricmp( m_szCurrentDemoFile, pDemo ) == 0 )
		return;

	// set the new demo file name
	V_snprintf( m_szCurrentDemoFile, sizeof( m_szCurrentDemoFile ), "playtests/%s", pDemo );

	// do nothing if disabled
	if ( !pMapItem->IsDisabled() )
	{
		// find the kv
		KeyValues *pDemoData = m_pMapDemoValues->FindKey( m_szCurrentMap )->FindKey( pDemo );
		if ( pDemoData )
		{
			// set all the label texts
			m_pPlayerLabel->SetText( pDemoData->GetString( "player" ) );
		
			UpdateTimeLabel( pDemoData->GetFloat( "time" ) );
		}
	}

	UpdateFooter();
}


void CPlaytestDemosDialog::UpdateTimeLabel( float flTime )
{
	int nSeconds = flTime;
	int nMiliseconds = ( flTime - nSeconds ) * 100;
	int nMinutes = nSeconds / 60;
	nSeconds = nSeconds % 60;

	// update the label
	char szTime[32];
	V_snprintf( szTime, sizeof(szTime), "%02d:%02d.%02d", nMinutes, nSeconds, nMiliseconds );
	m_pTimeLabel->SetText( szTime );
}


void CPlaytestDemosDialog::CreateDownloadRequest( KeyValues *pKV )
{
	// get the info from the keyvalues
	UGCHandle_t demoHandle = pKV->GetUint64( "ugcHandle" );
	PublishedFileId_t mapID = pKV->GetUint64( "mapID" );
	UGCHandle_t mapFileHandle = pKV->GetUint64( "mapFileID" );
	bool bInLocalCloud = pKV->GetBool( "inLocalCloud", false );
	bool bNewDemo = pKV->GetBool( "newDemo", false );

	Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] CreateDownloadRequest:: "
		"UGCHandle: %llu  MapID: %llu  MapFileID: %llu  InLocalCloud: %d  NewDemo: %d\n",
		pKV->GetUint64( "ugcHandle" ), pKV->GetUint64( "mapID" ), pKV->GetUint64( "mapFileID" ), pKV->GetBool( "inLocalCloud", false ), pKV->GetBool( "newDemo", false ) );

	// Add a request for this map
	bool bDemoRequestAdded = WorkshopManager().CreateFileDownloadRequest( demoHandle, "playtests", NULL );
	if ( bDemoRequestAdded )
	{
		Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] CreateDownloadRequest:: Adding UGCFileRequest: %llu\n",  demoHandle );

		// Set all the demo download info and add it to the list to download
		CDemoInfo *pDemoDownload = new CDemoInfo();
		pDemoDownload->m_bInLocalCloud = bInLocalCloud;
		pDemoDownload->m_bNewDemo = bNewDemo;
		pDemoDownload->m_mapID = mapID;
		pDemoDownload->m_mapFileHandle = mapFileHandle;
		pDemoDownload->m_demoHandle = demoHandle;
#if !defined( NO_STEAM )
		CSteamID playerID( pKV->GetUint64( "playerID" ) ) ;
		pDemoDownload->m_playerID = playerID;
#endif

		// Add it to the list of known downloading maps
		m_vDemoDownloads.AddToTail( pDemoDownload );
		m_bDownloadingDemos = true;
	}
	else
	{
		// This means that the request for a file was denied!
		Assert( bDemoRequestAdded );
		Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] CreateDownloadRequest:: Unable to request demo: %llu\n", demoHandle );
	}


	// Make a request for this map (making sure we don't already have it)
	if ( WorkshopManager().UGCFileRequestExists( mapFileHandle ) == false )
	{
		// make sure we're not already downloading this map (in case of multiple replays)
		Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] CreateDownloadRequest:: Adding download of mapFileID: %llu\n", mapFileHandle );
		
		char szTargetDirectory[MAX_PATH];
		V_snprintf( szTargetDirectory, sizeof(szTargetDirectory), "%s%c%llu", COMMUNITY_MAP_PATH, CORRECT_PATH_SEPARATOR, mapFileHandle );
		
		bool bMapRequestAdded = WorkshopManager().CreateFileDownloadRequest( mapFileHandle, szTargetDirectory, NULL );
		if ( bMapRequestAdded )
		{
			// Hold on to it so we can query for its state
			m_vMapDownloadHandles.AddToTail( mapFileHandle );
			m_bDownloadingMaps = true;
		}
		else
		{
			Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] CreateDownloadRequest:: File request for MapFileID: %llu failed!\n", mapFileHandle );
		}
	}
	else
	{
		Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] CreateDownloadRequest:: Already downloading mapFileID: %llu\n", mapFileHandle );
		
		// Hold on to it so we can query for its state
		// FIXME: Double-check that this needs to be here in the "already found case"
		m_vMapDownloadHandles.AddToTail( mapFileHandle );
		m_bDownloadingMaps = true;
	}
}


void CPlaytestDemosDialog::DownloadDemos()
{
	// make sure the current selected demo is clear
	V_strncpy( m_szCurrentDemoFile, "", sizeof( m_szCurrentDemoFile ) );

	// make sure the download requests are clear	
	FreeDemoDownloads();
	m_vMapDownloadHandles.Purge();

#if !defined( NO_STEAM )
	CSteamID authorID;
	if ( steamapicontext && steamapicontext->SteamUser() )
	{
		authorID = steamapicontext->SteamUser()->GetSteamID();
	}
	// ask the GC for demoHandles for our steamID
	GCSDK::CProtoBufMsg<CMsgPlaytestRetrieveDemoHandles> msgDemo( k_EMsgGCPlaytestRetrieveDemoHandles );
	msgDemo.Body().set_author_id( authorID.GetAccountID() );
	GCClientSystem()->BSendMessage( msgDemo );

	Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] DownloadDemos:: Sent GC PlaytestRetrieveDemoHandles message for AuthorID: %u\n",
		authorID.GetAccountID() );

	// TEMP!!
	/*GCSDK::CProtoBufMsg<CMsgPlaytestRetrieveDemoHandles> msgRetrieveNewCount( k_EMsgGCPlaytestRetrieveNewDemoCount );
	msgRetrieveNewCount.Body().set_author_id( authorID.ConvertToUint64() );
	GCClientSystem()->BSendMessage( msgRetrieveNewCount );*/

	m_flNextDemoRequest = gpGlobals->curtime + 1.0f;
#endif

}


void CPlaytestDemosDialog::UpdateDemoList()
{
	if ( !m_pMapList || !m_pDemoFileList )
		return;

	m_pMapList->RemoveAllPanelItems();
	m_pDemoFileList->RemoveAllPanelItems();
	
	// clear our selected demo
	m_szCurrentDemoFile[0] = '\0';

	m_pMapList->GetScrollBar()->SetValue( 0 );
	m_pDemoFileList->GetScrollBar()->SetValue( 0 );

	// loop thru the map names
	for ( KeyValues *pNextMap = m_pMapDemoValues->GetFirstSubKey(); pNextMap != NULL; pNextMap = pNextMap->GetNextKey() )
	{
		// make sure it has subkeys
		KeyValues *pNextDemo = pNextMap->GetFirstSubKey();
		if ( pNextDemo == NULL )
		{
			// don't add an item for an empty map
			continue;
		}
		
		const char* pName = pNextMap->GetName();
		CDemoListItem *pItem = m_pMapList->AddPanelItem< CDemoListItem >( "newgame_chapteritem" );
		if ( pItem )
		{
			pItem->SetLabelText( pName );
			pItem->SetDisabled( true ); // start out disabled
		}
	}

	int nMapCount = m_pMapList->GetPanelItemCount();

	// if first time entry
	if ( m_szCurrentMap[0] == '\0' && nMapCount > 0 )
	{
		m_pMapList->SelectPanelItem( 0 );
	}
	else
	{
		bool bMapFound = false;
		for( int i = 0; i < nMapCount; ++i )
		{
			// check if this panel is the previously selected map
			CDemoListItem *pMapPanel = static_cast< CDemoListItem *>( m_pMapList->GetPanelItem( i ) );
			if ( V_stricmp( m_szCurrentMap, pMapPanel->GetMapName() ) != 0)
			{
				continue;
			}

			// this is the previously selected map, re-select it
			m_pMapList->SelectPanelItem( i );
			bMapFound = true;
			break;
		}

		if ( !bMapFound )
		{
			// our previously selected map no longer exists
			m_szCurrentMap[0] = '\0';
			// if there are other maps, select the top one
			if ( nMapCount )
			{
				m_pMapList->SelectPanelItem( 0 );
			}
		}
	}

	// make sure we re-enable any maps that are properly downloaded
	EnableFinishedMaps();

}

void CPlaytestDemosDialog::UploadDemo( CDemoInfo *demoInfo )
{
	// Have to make a new demoInfo so that we aren't reusing the file request
	CDemoInfo *pDemoUpload = new CDemoInfo( );
	pDemoUpload->m_mapID = demoInfo->m_mapID;
	pDemoUpload->m_mapFileHandle = demoInfo->m_mapFileHandle;
	pDemoUpload->m_originalDemoHandle = pDemoUpload->m_demoHandle;
	pDemoUpload->m_demoHandle = k_UGCHandleInvalid;

#if !defined( NO_STEAM )
	pDemoUpload->m_playerID = demoInfo->m_playerID;
#endif
	
	char szFullPath[MAX_PATH];
	WorkshopManager().GetUGCFullPath( demoInfo->m_demoHandle, szFullPath, ARRAYSIZE(szFullPath) );

	const char *lpszDirectory = WorkshopManager().GetUGCFileDirectory( demoInfo->m_demoHandle );
	const char *lpszFilename = WorkshopManager().GetUGCFilename( demoInfo->m_demoHandle );

	// Make a new file request
	WorkshopManager().CreateFileUploadRequest( szFullPath, lpszDirectory, lpszFilename );

	// Take the full filename as it would show up on disk
	V_strncpy( pDemoUpload->m_szFilename, szFullPath, ARRAYSIZE( pDemoUpload->m_szFilename ) );
	
	Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] UploadDemo:: New demo upload request:\n"
		"File: %s  MapID: %llu  MapFileID: %llu  PlayerID: %u\n",
		pDemoUpload->m_szFilename, demoInfo->m_mapID, demoInfo->m_mapFileHandle, demoInfo->m_playerID.GetAccountID() );

	// add to upload requests
	m_DemoUploads.Insert( pDemoUpload );
}


void CPlaytestDemosDialog::ClearCloudPlaytests()
{
	ISteamRemoteStorage *pRemoteStorage = GetISteamRemoteStorage();
	if ( pRemoteStorage )
	{
		// get a count of stored files
		int nNumFiles = pRemoteStorage->GetFileCount();

		// keep a vector of the file names found on the cloud
		CUtlVector< const char *> vpFileNames;
		// get all the file names from the cloud
		for ( int i = 0; i < nNumFiles; ++i )
		{
			int32 nFileSize;
			const char *pCloudFileName; 
			pCloudFileName = pRemoteStorage->GetFileNameAndSize(i, &nFileSize );
			// get the path
			char szFileName[ MAX_MAP_NAME ];
			V_strncpy( szFileName, pCloudFileName, sizeof( szFileName ) );
			V_StripFilename( szFileName );
			if ( !V_stricmp( szFileName, "playtests" ) )
			{
				vpFileNames.AddToTail( pCloudFileName );
			}
		}

		// deletion is separate to not screw up iteration of the cloud file names
		FOR_EACH_VEC( vpFileNames, itr )
		{
			DevMsg("Clearing Filename %s\n", vpFileNames[itr] );
			pRemoteStorage->FileDelete( vpFileNames[itr] );
		}
	}
	
}

void CPlaytestDemosDialog::DeleteSelectedDemo()
{
	// find the handle of the currently selected map

	// loop thru the demo file maps
	for ( KeyValues *pNextMap = m_pMapDemoValues->GetFirstSubKey(); pNextMap != NULL; pNextMap = pNextMap->GetNextKey() )
	{
		// get the current map name
		const char* pName = pNextMap->GetName();
		if( V_stricmp( pName, m_szCurrentMap ) != 0 )
		{
			// not our map, move to the next
			continue;
		}

		// get the current demo
		for ( KeyValues *pNextDemo = pNextMap->GetFirstSubKey(); pNextDemo != NULL; pNextDemo = pNextDemo->GetNextKey() )
		{
			//see if we have the right demo file
			const char* pDemoName = pNextDemo->GetName();
			char szCurrentDemo[ MAX_PATH ];
			V_FileBase( m_szCurrentDemoFile, szCurrentDemo, sizeof(szCurrentDemo) );
			if ( V_stricmp( pDemoName, szCurrentDemo ) != 0 )
			{
				// not our demo, move to the next
				continue;
			}

			// get the UGCHandle for the demo
			UGCHandle_t demoHandle = pNextDemo->GetUint64( "handle", k_UGCHandleInvalid );

			// remove the handle from the GC
#if !defined( NO_STEAM )
			/*CSteamID authorID;
			if ( steamapicontext && steamapicontext->SteamUser() )
			{
				authorID = steamapicontext->SteamUser()->GetSteamID();
			}*/
			// remove the handle from the GC
			GCSDK::CProtoBufMsg<CMsgPlaytestRemoveDemo> msgDemo( k_EMsgGCPlaytestRemoveDemo );
			msgDemo.Body().set_ugc_handle( demoHandle );
			GCClientSystem()->BSendMessage( msgDemo );

			Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] DeleteSelectedDemo:: Sent GC PlaytestRemoveDemo message:\n"
				"UGCHandle: %llu\n", demoHandle );

			// remove the file from the Cloud
			char szFileNameWithExtension[ MAX_PATH ];
			V_snprintf( szFileNameWithExtension, sizeof(szFileNameWithExtension), "%s.dem", m_szCurrentDemoFile );

			ISteamRemoteStorage *pRemoteStorage = GetISteamRemoteStorage();
			if ( pRemoteStorage )
			{
				if ( pRemoteStorage->FileExists( szFileNameWithExtension ) )
				{
					if ( !pRemoteStorage->FileDelete( szFileNameWithExtension ) )
					{
						Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] DeleteSelectedDemo:: Failed to remove file from cloud: %s\n", szFileNameWithExtension );
						AssertMsg( 0, "Failed to remove file from cloud." );
					}

					Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] DeleteSelectedDemo:: Deleted Cloud File: %s\n", szFileNameWithExtension );
				}
				else
				{
					Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] DeleteSelectedDemo:: Cloud file does not exist: %s\n", szFileNameWithExtension );
				}
			}

			// remove the local file - doesn't seem to be happening with just filedelete (might need fileload first)
			g_pFullFileSystem->RemoveFile( szFileNameWithExtension );
			Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] DeleteSelectedDemo:: Removed local file: %s\n", szFileNameWithExtension );
#endif

			// remove the demo from the key values
			pNextMap->RemoveSubKey( pNextDemo );

			// we're done looping
			break;
		}
		
	}

	UpdateDemoList();
	UpdateFooter();
}


void CPlaytestDemosDialog::EnumerateCloudFiles()
{
	ISteamRemoteStorage *pRemoteStorage = GetISteamRemoteStorage();
	if ( !pRemoteStorage )
	{
		Warning( "CPlaytestDemosDialog::EnumerateCloudFiles - Cannot access Steam cloud!\n" );
		return;
	}

	// TEMP - enumerate cloud files
	DevMsg( "Remote Storage Files:\n" );
	int nFileCount = pRemoteStorage->GetFileCount();
	for ( int i = 0; i < nFileCount; ++i )
	{
		int nFileSize;
		const char *pFileName = pRemoteStorage->GetFileNameAndSize( i, &nFileSize );
		bool bExists = pRemoteStorage->FileExists( pFileName );
		bool bPersisted = pRemoteStorage->FilePersisted( pFileName );
		DevMsg( "File %d of %d: %s\n", i+1, nFileCount, pFileName );
		DevMsg( "File Exists: %s, ", bExists ? "True" : "False" );
		DevMsg( "File Persisted: %s\n\n", bPersisted ? "True" : "False" );
	}

	//// TEMP - enumerate published files (??)
	//SteamAPICall_t hSteamAPICall = pRemoteStorage->EnumerateUserPublishedFiles( 0 );
	//m_callbackEnumerateFiles.Set( hSteamAPICall, this, &CPlaytestDemosDialog::Steam_OnEnumerateFiles );
}


void CPlaytestDemosDialog::EnableMap( const char *pMapName )
{
	// take the path and extension off the map name
	char szFileName[ MAX_MAP_NAME ];
	V_FileBase(pMapName, szFileName, sizeof( szFileName ) );
	// loop thru map items
	int nMapCount = m_pMapList->GetPanelItemCount();
	for ( int i = 0; i < nMapCount; ++i )
	{
		CDemoListItem *pMapItem = static_cast< CDemoListItem* >( m_pMapList->GetPanelItem( i ) );
		if ( V_stricmp( pMapItem->GetMapName(), szFileName ) != 0 )
		{
			continue;
		}

		pMapItem->SetDisabled( false );
		if ( !V_stricmp( m_szCurrentMap, pMapItem->GetMapName() ) )
		{
			SetCurrentMap( m_szCurrentMap, pMapItem );
		}
		InvalidateLayout();
		break;
	}
}


void CPlaytestDemosDialog::EnableFinishedMaps( void )
{
	FOR_EACH_VEC( m_vMapDownloadHandles, itr )
	{
		UGCFileRequestStatus_t status = WorkshopManager().GetUGCFileRequestStatus( m_vMapDownloadHandles[itr] );
		if ( status == UGCFILEREQUEST_FINISHED )
		{
			const char *lpszFilename = WorkshopManager().GetUGCFilename( m_vMapDownloadHandles[itr] );
			EnableMap( lpszFilename );
		}
	}
}


bool CPlaytestDemosDialog::UpdateFileUploadRequest( CDemoInfo *pDemoInfo )
{
	AssertMsg( pDemoInfo != NULL, "UpdateFileUploadRequest: Upload request is NULL!" );

	UGCFileRequestStatus_t status = WorkshopManager().GetUGCFileRequestStatusByFilename( pDemoInfo->m_szFilename );
	Assert( status != UGCFILEREQUEST_INVALID );

	if ( status == UGCFILEREQUEST_FINISHED )
	{
		// Now we have a real handle, so hold onto it
		pDemoInfo->m_demoHandle = WorkshopManager().GetUGCFileHandleByFilename( pDemoInfo->m_szFilename );

		Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] UpdateFileUploadRequest:: File %s has a status of Finished.\n", pDemoInfo->m_szFilename );

		// tell the GC about the demo file
#if !defined( NO_STEAM )
		GCSDK::CProtoBufMsg<CMsgPlaytestReportDemo>	msgDemo( k_EMsgGCPlaytestReportDemo );

		CSteamID authorID;
		if ( steamapicontext && steamapicontext->SteamUser() )
		{
			authorID = steamapicontext->SteamUser()->GetSteamID();
		}
		msgDemo.Body().set_author_id( authorID.GetAccountID() );
		msgDemo.Body().set_player_id( pDemoInfo->m_playerID.GetAccountID() );
		msgDemo.Body().set_ugc_handle( pDemoInfo->m_demoHandle );
		msgDemo.Body().set_map_id( pDemoInfo->m_mapID );
		msgDemo.Body().set_map_file_id( pDemoInfo->m_mapFileHandle );
		GCClientSystem()->BSendMessage( msgDemo );

		Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] UpdateFileUploadRequest:: GC PlaytestReportDemo message sent:\n"
			"AuthorID: %u  PlayerID: %u  UGCHandle: %llu  MapID: %llu  MapFileID: %llu\n",
			authorID.GetAccountID(), pDemoInfo->m_playerID.GetAccountID(), pDemoInfo->m_demoHandle, pDemoInfo->m_mapID, pDemoInfo->m_mapFileHandle );
#if 1
		// Now that it has succeeded, we mark this as done
		GCSDK::CProtoBufMsg<CMsgPlaytestMarkDemoForDelete>	msgMarkForDelete( k_EMsgGCPlaytestMarkDemoForDelete );
		msgMarkForDelete.Body().set_ugc_handle( pDemoInfo->m_originalDemoHandle );
		msgMarkForDelete.Body().set_cloud_owner_id( pDemoInfo->m_playerID.GetAccountID() );
		GCClientSystem()->BSendMessage( msgMarkForDelete );

		Log_Msg( LOG_WORKSHOP, "[CPlaytestDemosDialog] OnThink:: Sent GC PlaytestMarkDemoForDelete message:\n"
			"UGCHandle: %llu  CloudOwnerID: %u\n",  pDemoInfo->m_originalDemoHandle, pDemoInfo->m_playerID.GetAccountID() );
#endif

#endif

		/*char szHandle[32];
		V_snprintf( szHandle, sizeof(szHandle), "%llu", pDemoInfo->m_fileRequest.GetCloudHandle() );
		Assert( V_strlen( szHandle ) <= 16  );
		DevMsg( "DemoUGCHandle: %llu\n", pDemoInfo->m_fileRequest.GetCloudHandle() );*/

		// update the KeyValues for the uploaded demo - specifically its UGCHandle
		for ( KeyValues *pNextMap = m_pMapDemoValues->GetFirstSubKey(); pNextMap != NULL; pNextMap = pNextMap->GetNextKey() )
		{
			// we don't have the map name, so look thru all the dem files for each map
			for ( KeyValues *pNextDemo = pNextMap->GetFirstSubKey(); pNextDemo != NULL; pNextDemo = pNextDemo->GetNextKey() )
			{
				const char* pFileName = pNextDemo->GetName();
				char szCurrentDemoName[ MAX_PATH ];
				const char *lpszTargetFilename = WorkshopManager().GetUGCFilename( pDemoInfo->m_demoHandle );
				V_StripExtension( lpszTargetFilename, szCurrentDemoName, sizeof(szCurrentDemoName) );
				if ( !V_stricmp( pFileName, szCurrentDemoName ) )
				{
					// TEST
					//KeyValuesDumpAsDevMsg( pNextDemo );
					pNextDemo->SetUint64( "handle", pDemoInfo->m_demoHandle );
					// update the panel item's data, if it exists
					CDemoListItem *pDemoItem = GetDemoPanel( pFileName );
					if ( pDemoItem )
					{
						pDemoItem->SetUGCHandle( pDemoInfo->m_demoHandle );
					}

					return true;
				}
			}
		}


		return true;
	}
	else if ( status == UGCFILEREQUEST_ERROR )
	{
		Warning( "An error occured while attempting to upload a file to the UGC server!\n" );
		return true;
	}
	
	return false;
}


void CPlaytestDemosDialog::Steam_OnEnumerateFiles( RemoteStorageEnumerateUserPublishedFilesResult_t *pResult, bool bError )
{
	if ( bError || pResult->m_eResult != k_EResultOK )
	{
		Warning( "Unable to enumerate published files!\n" );
	}

	int nTotalResults = pResult->m_nTotalResultCount;

	if ( nTotalResults == 0 )
	{
		DevMsg( "No Published Files Found.\n" );
		return;
	}

	DevMsg( "Published Files:\n" );

	for ( int i = 0; i < nTotalResults; ++i )
	{
		PublishedFileId_t fileID = pResult->m_rgPublishedFileId[ i ];
		DevMsg( "%d of %d: %d\n", i, nTotalResults, fileID );
	}
}


CDemoListItem * CPlaytestDemosDialog::GetDemoPanel( const char *pDemoName )
{
	int nDemoCount = m_pDemoFileList->GetPanelItemCount();

	for ( int i = 0; i < nDemoCount; ++i )
	{
		// get the panel
		CDemoListItem *pDemoPanel = static_cast< CDemoListItem *>( m_pDemoFileList->GetPanelItem( i ) );
		// if this is the demo we want
		if ( !V_stricmp( pDemoName, pDemoPanel->GetMapName() ) )
		{
			return pDemoPanel;
		}
	}

	return NULL;
}


void CPlaytestDemosDialog::ResetLogging( void )
{
	for ( int i = 0; i < PROCESSTYPE_COUNT; ++i )
	{
		bProcessesLogged[ i ] = false;
	}
}


//=============================================================================
CDemoListItem::CDemoListItem( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName ),
	m_pListCtrlr( ( GenericPanelList * )pParent )
{
	m_pPlaytestDialog = dynamic_cast< CPlaytestDemosDialog * >( m_pListCtrlr->GetParent() );

	SetProportional( true );
	SetPaintBackgroundEnabled( true );

	m_hTextFont = vgui::INVALID_FONT;

	m_nTextOffsetY = 0;

	m_bSelected = false;
	m_bDisabled = false;
	m_bHasMouseover = false;
	m_bInMapColumn = true;
	m_bMarkedAsNew = false;
	m_demoHandle = k_UGCHandleInvalid;
}


//=============================================================================	
void CDemoListItem::SetLabelText( const char *pMapName, bool bInMapColumn /*= true */ )
{

	Label *pLabel = dynamic_cast< Label* >( FindChildByName( "LblChapterName" ) );
	if( !pLabel )
		return;

	// store the actual map/demo name
	V_strncpy( m_szLabelText, pMapName, sizeof(m_szLabelText) );

	// if we're in the map column
	if ( bInMapColumn )
	{
		// just set the text straight up
		pLabel->SetText( m_szLabelText );
	}
	else // we're in the demo column
	{
		// remove the demo name portion
		const char *pDemoName = pMapName + V_strlen( m_pPlaytestDialog->m_szCurrentMap ) + 1;
		char szDemoName[ MAX_PATH ];
		// if it is new, add a * to the front of the name - TEMP
		if ( m_bMarkedAsNew )
		{
			V_snprintf( szDemoName, sizeof( szDemoName ), "*%s", pDemoName );
			pDemoName = szDemoName;
		}
		// set the altered text as the label
		pLabel->SetText( pDemoName );
	}

	m_bInMapColumn = bInMapColumn;
}


//=============================================================================
void CDemoListItem::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/BaseModUI/newgame_chapteritem.res" );

	const char *pDefaultFontString = pScheme->GetResourceString( "HybridButton.Font" );
	const char *pStyle = "DialogListButton";
	const char *pFontString = pScheme->GetResourceString( CFmtStr( "%s.Font", pStyle ) );
	m_hTextFont = pScheme->GetFont( pFontString[0] ? pFontString : pDefaultFontString, true );

	m_TextColor = GetSchemeColor( "HybridButton.TextColorAlt", pScheme );
	m_FocusColor = GetSchemeColor( "HybridButton.FocusColorAlt", pScheme );
	m_CursorColor = GetSchemeColor( "HybridButton.CursorColorAlt", pScheme );
	m_LockedColor = Color( 64, 64, 64, 128 );//GetSchemeColor( "HybridButton.LockedColor", pScheme );
	m_MouseOverCursorColor = GetSchemeColor( "HybridButton.MouseOverCursorColorALt", pScheme );
	m_LostFocusColor = m_CursorColor;
	m_LostFocusColor.SetColor( m_LostFocusColor.r(), m_LostFocusColor.g(), m_LostFocusColor.b(), 50); //Color( 120, 120, 120, 255 );
	m_BaseColor = Color( 255, 255, 255, 0 );

	m_nTextOffsetY = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "NewGameDialog.TextOffsetY" ) ) );
}


//=============================================================================
void CDemoListItem::PaintBackground()
{
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

	Label *pLabel = dynamic_cast< Label* >( FindChildByName("LblChapterName") );
	if ( pLabel )
	{
		DrawListItemLabel( pLabel );
	}
}


//=============================================================================
void CDemoListItem::OnCursorEntered()
{ 
	SetHasMouseover( true );

	if ( IsPC() )
		return;

	if ( GetParent() )
		GetParent()->NavigateToChild( this );
	else
		NavigateTo();
}


//=============================================================================
void CDemoListItem::NavigateTo( void )
{
	m_pListCtrlr->SelectPanelItemByPanel( this );

	SetHasMouseover( true );
	RequestFocus();
	int nNumPanels = m_pListCtrlr->GetPanelItemCount();
	for ( int i = 0; i < nNumPanels; ++i )
	{
		CDemoListItem *pPanel = static_cast< CDemoListItem *>( m_pListCtrlr->GetPanelItem( i ) );
		if ( pPanel )
		{
			pPanel->SetSelected( false );
		}
	}
	SetSelected( true );

	if ( m_bInMapColumn )
	{
		m_pPlaytestDialog->SetCurrentMap( m_szLabelText, this );
	}
	else
	{
		m_pPlaytestDialog->SetCurrentDemo( m_szLabelText, this );
	}

	BaseClass::NavigateTo();
}


//=============================================================================
void CDemoListItem::NavigateFrom( void )
{
	SetHasMouseover( false );

	BaseClass::NavigateFrom();

	if ( IsGameConsole() )
	{
		OnClose();
	}
}


//=============================================================================
void CDemoListItem::OnKeyCodePressed( vgui::KeyCode code )
{
	int iUserSlot = GetJoystickForCode( code );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( iUserSlot );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_UP:
	case KEY_XBUTTON_UP:
	case KEY_XSTICK1_UP:
	case KEY_XSTICK2_UP:
		break;

	case KEY_DOWN:
	case KEY_XBUTTON_DOWN:
	case KEY_XSTICK1_DOWN:
	case KEY_XSTICK2_DOWN:
		break;

	case KEY_RIGHT:
	case KEY_XBUTTON_RIGHT:
	case KEY_XSTICK1_RIGHT:
	case KEY_XSTICK2_RIGHT:
		break;

	case KEY_ENTER:
	case KEY_XBUTTON_A:
		m_pPlaytestDialog->OnKeyCodePressed( code );
		return;
	}

	BaseClass::OnKeyCodePressed( code );
}


//=============================================================================
void CDemoListItem::OnMousePressed( vgui::MouseCode code )
{
	if ( code == MOUSE_LEFT )
	{
		if ( GetParent() )
			GetParent()->NavigateToChild( this );
		else
			NavigateTo();

		Assert( m_pPlaytestDialog );
		if( m_pPlaytestDialog )
		{
			m_pPlaytestDialog->UpdateFooter();
		}
		return;
	}
	BaseClass::OnMousePressed( code );
	m_pPlaytestDialog->UpdateFooter();
}


//=============================================================================
void CDemoListItem::OnMouseDoublePressed( vgui::MouseCode code )
{
	if ( code == MOUSE_LEFT )
	{
		CDemoListItem* pListItem = static_cast< CDemoListItem* >( m_pListCtrlr->GetSelectedPanelItem() );
		if ( pListItem )
		{
			OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		}

		return;
	}

	BaseClass::OnMouseDoublePressed( code );
}


//=============================================================================
void CDemoListItem::PerformLayout()
{
	BaseClass::PerformLayout();

	// set all our children (image panel and labels) to not accept mouse input so they
	// don't eat any mouse input and it all goes to us
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		Panel *panel = GetChild( i );
		Assert( panel );
		panel->SetMouseInputEnabled( false );
	}
}


//=============================================================================
void CDemoListItem::DrawListItemLabel( vgui::Label *pLabel )
{
	if ( !pLabel )
		return;

	// set text color
	Color textColor = m_TextColor;
	if ( m_bDisabled )
	{
		textColor = m_LockedColor;
	}
	else if ( m_bSelected || HasMouseover() )
	{
		textColor = m_FocusColor;
	}

	int panelWide, panelTall;
	GetSize( panelWide, panelTall );

	int x, y, labelWide, labelTall;
	pLabel->GetBounds( x, y, labelWide, labelTall );

	wchar_t szUnicode[512];
	pLabel->GetText( szUnicode, sizeof( szUnicode ) );
	int len = V_wcslen( szUnicode );

	int textWide, textTall;
	surface()->GetTextSize( m_hTextFont, szUnicode, textWide, textTall );

	// vertical center
	y += ( labelTall - textTall ) / 2 + m_nTextOffsetY;

	vgui::surface()->DrawSetTextFont( m_hTextFont );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawSetTextColor( textColor );
	vgui::surface()->DrawPrintText( szUnicode, len );
}


//=============================================================================
void CDemoListItem::SetHasMouseover( bool bHasMouseover )
{
	if ( bHasMouseover )
	{
		for ( int i = 0; i < m_pListCtrlr->GetPanelItemCount(); i++ )
		{
			CDemoListItem *pItem = dynamic_cast< CDemoListItem* >( m_pListCtrlr->GetPanelItem( i ) );
			if ( pItem && pItem != this )
			{
				pItem->SetHasMouseover( false );
			}
		}
	}
	m_bHasMouseover = bHasMouseover;
}


//-----------------------------------------------------------------------------
// Purpose: Receive messages to do with retrieving demo file handles
//-----------------------------------------------------------------------------
class CGCPlaytestRetrieveDemoHandleResponse : public GCSDK::CGCClientJob
{
public:
	CGCPlaytestRetrieveDemoHandleResponse( GCSDK::CGCClient *pClient ) : GCSDK::CGCClientJob( pClient ) {}

	virtual bool BYieldingRunGCJob( GCSDK::IMsgNetPacket *pNetPacket )
	{		
		GCSDK::CProtoBufMsg<CMsgPlaytestRetrieveDemoHandlesResponse> msg( pNetPacket );
		KeyValues *pKV = new KeyValues( "OnDemoHandleReceived" );
		// loop thru repeated UGCHandles in message
		int nHandleCount = msg.Body().ugc_handle_size();
		Log_Msg( LOG_WORKSHOP, "[CGCPlaytestRetrieveDemoHandleResponse]:: %d Handles retrieved from GC.\n", nHandleCount );
		for ( int i = 0; i < nHandleCount; ++i )
		{
			KeyValues *pHandleKV = pKV->CreateNewKey();
			pHandleKV->SetUint64( "ugcHandle", msg.Body().ugc_handle(i) );
			pHandleKV->SetUint64( "mapID", msg.Body().map_id( i ) );
			pHandleKV->SetUint64( "mapFileID", msg.Body().map_file_id( i ) );
			pHandleKV->SetBool( "inLocalCloud", msg.Body().personal_cloud( i ) );
			pHandleKV->SetUint64( "playerID", msg.Body().player_id( i ) );
			pHandleKV->SetBool( "newDemo", msg.Body().new_demo( i ) );
			Log_Msg( LOG_WORKSHOP, "[CGCPlaytestRetrieveDemoHandleResponse] Record %d:\n"
				"PlayerID: %u  UGCHandle: %llu  MapID: %llu  MapFileID: %llu  InLocalCloud: %d  NewDemo: %d\n",
				i, msg.Body().player_id(i), msg.Body().ugc_handle(i), msg.Body().map_id( i ), msg.Body().map_file_id( i ), msg.Body().personal_cloud( i ), msg.Body().new_demo( i ) );
		}
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( pKV );
		//BroadcastUGCHandle( msg.Body().ugc_handle() );
		return true;
	}
};

GC_REG_JOB( GCSDK::CGCClient, CGCPlaytestRetrieveDemoHandleResponse, "CGCPlaytestRetrieveDemoHandleResponse", k_EMsgGCPlaytestRetrieveDemoHandlesResponse, GCSDK::k_EServerTypeGCClient );


//-----------------------------------------------------------------------------
// Purpose: Receive messages to do with retrieving new demo count
//-----------------------------------------------------------------------------
class CGCJobPlaytestRetrieveNewDemoCountResponse : public GCSDK::CGCClientJob
{
public:
	CGCJobPlaytestRetrieveNewDemoCountResponse( GCSDK::CGCClient *pClient ) : GCSDK::CGCClientJob( pClient ) {}

	virtual bool BYieldingRunGCJob( GCSDK::IMsgNetPacket *pNetPacket )
	{		
		GCSDK::CProtoBufMsg<CMsgPlaytestRetrieveNewDemoCountResponse> msg( pNetPacket );
		KeyValues *pKV = new KeyValues( "OnNewDemoCountReceived" );
		// loop thru repeated UGCHandles in message
		int nHandleCount = msg.Body().map_id_size();
		for ( int i = 0; i < nHandleCount; ++i )
		{
			KeyValues *pHandleKV = pKV->CreateNewKey();
			pHandleKV->SetUint64( "mapID", msg.Body().map_id( i ) );
			pHandleKV->SetInt( "newDemoCount", msg.Body().count( i ) );
		}
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( pKV );
		return true;
	}
};

GC_REG_JOB( GCSDK::CGCClient, CGCJobPlaytestRetrieveNewDemoCountResponse, "CGCJobPlaytestRetrieveNewDemoCountResponse", k_EMsgGCPlaytestRetrieveNewDemoCountResponse, GCSDK::k_EServerTypeGCClient );



} // namespace BaseModUI

#endif // PORTAL2_PUZZLEMAKER
