//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#include "cbase.h"

#if defined( PORTAL2_PUZZLEMAKER )

#include <time.h>
#include "vpuzzlemakermychambers.h"
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
#include "puzzlemaker/puzzlemaker.h"
#include "vgui/Ilocalize.h"
#include "vpuzzlemakersavedialog.h"
#include "vpuzzlemakerexitconfirmation.h"
#include "bitmap/tgaloader.h"
#include "vgui_controls/imagepanel.h"
#include "imageutils.h"
#include "keyvalues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;
using namespace BaseModUI;

extern QString AUTOSAVE_FILE_NAME;

// Used to format our date / time
extern void FormatFileTimeString( time_t nFileTime, wchar_t *pOutputBuffer, int nBufferSizeInBytes );

BaseModUI::CPuzzleMakerUIPanel::CPuzzleMakerUIPanel( vgui::Panel *pParent, const char *pPanelName ):
BaseClass( pParent, pPanelName )
{

}

CPuzzleMakerMyChambers::CPuzzleMakerMyChambers( Panel *pParent, const char *pPanelName )
					  : BaseClass( pParent, pPanelName ),
						m_pChamberList( NULL ),
						m_pChamberSortButton( NULL ),
						m_pSelectedListPanel( NULL ),
						m_nDeleteIndex( 0 ),
						m_bInEditor( false ),
						m_nThumbnailImageId( -1 ),
						m_flTransitionStartTime( 0.f ),
						m_flNextLoadThumbnailTime( 0.f ),
						m_hAsyncControl(NULL),
						m_hPuzzleInfoAsyncControl(NULL),
						m_nAutoSaveIndex( -1 ),
						m_pLblAutoSaveFound( NULL ),
						m_pLblChamberName( NULL ),
						m_pNewChamberStub( NULL ),
						m_sortType( CHAMBER_SORT_STATUS ),
						m_bSetupComplete( false ),
						m_nSelectedListItemIndex( 0 ),
						m_pAutoSaveInfoImage( NULL ),
						m_pCurrentPuzzleFileInfoRequest( NULL ),
						m_bPuzzlesLoadedFromDisk( false ),
						m_pThumbnailImage( NULL ),
						m_pLblStatCreated( NULL ),
						m_pLblStatCreatedData( NULL ),
						m_pLblStatModified( NULL ),
						m_pLblStatModifiedData( NULL ),
						m_pLblStatPublished( NULL ),
						m_pLblStatPublishedData( NULL ),
						m_pLblStatRating( NULL ),
						m_pRatingItem( NULL ),
						m_pPuzzleListSpinner( NULL ),
						m_pThumbnailSpinner( NULL ),
						m_bDeferredFinishCompleted( false )
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	m_pChamberList = new GenericPanelList( this, "ChamberList", GenericPanelList::ISM_PERITEM | GenericPanelList::ISM_ALPHA_INVISIBLE );
	m_pChamberList->SetPaintBackgroundEnabled( false );
	m_pChamberList->SetScrollBarVisible( true );

	// Subscribe to event notifications
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	SetDialogTitle( "#PORTAL2_EditorMenu_MyTestChambers" );

	SetFooterEnabled( true );

	// allows us to get RunFrame() during wait screen occlusion
	AddFrameListener( this );
}


CPuzzleMakerMyChambers::~CPuzzleMakerMyChambers()
{
	if ( m_hAsyncControl )
	{
		g_pFullFileSystem->AsyncFinish( m_hAsyncControl );
		g_pFullFileSystem->AsyncRelease( m_hAsyncControl );
		m_hAsyncControl = NULL;
	}

	if ( m_hPuzzleInfoAsyncControl )
	{
		g_pFullFileSystem->AsyncFinish( m_hPuzzleInfoAsyncControl );
		g_pFullFileSystem->AsyncRelease( m_hPuzzleInfoAsyncControl );
		m_hPuzzleInfoAsyncControl = NULL;
	}

	delete m_pChamberList;
	delete m_pNewChamberStub;
	delete m_pCurrentPuzzleFileInfoRequest;

	m_SavedChambers.FindAndRemove( m_pNewChamberStub );
	m_SavedChambers.PurgeAndDeleteElements();
	
	RemoveFrameListener( this );

	// Unsubscribe from event notifications
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );

	// Clean up our thumbnail
	if ( surface() && m_nThumbnailImageId != -1 )
	{
		// evict prior screenshot
		surface()->DestroyTextureID( m_nThumbnailImageId );
		m_nThumbnailImageId = -1;
	}
}


void CPuzzleMakerMyChambers::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pChamberSortButton = static_cast< CDialogListButton * >( FindChildByName( "ListBtnChamberSort" ) );
	if ( m_pChamberSortButton )
	{
		m_pChamberSortButton->SetCurrentSelectionIndex( 0 );
		m_pChamberSortButton->SetArrowsAlwaysVisible( IsPC() );
		m_pChamberSortButton->SetCanWrap( !IsPC() );
		m_pChamberSortButton->SetDrawAsDualStateButton( false );
		
		// If the puzzles aren't loaded, do let them click on things and mess things up
		if ( m_bPuzzlesLoadedFromDisk == false )
		{
			m_pChamberSortButton->SetEnabled( false );
		}
	}

	m_pThumbnailImage = static_cast<ImagePanel*>( FindChildByName( "ImgChamberThumb" ) );

	// Get pointers to our myriad children
	m_pLblChamberName		= dynamic_cast< vgui::Label * >( FindChildByName( "LblChamberName" ) );
	m_pLblStatCreated		= dynamic_cast< vgui::Label * >( FindChildByName( "LblChamberStatCreated" ) );
	m_pLblStatCreatedData	= dynamic_cast< vgui::Label * >( FindChildByName( "LblChamberStatCreatedData" ) );
	m_pLblStatModified		= dynamic_cast< vgui::Label * >( FindChildByName( "LblChamberStatLastModified" ) );
	m_pLblStatModifiedData	= dynamic_cast< vgui::Label * >( FindChildByName( "LblChamberStatLastModifiedData" ) );
	m_pLblStatPublished		= dynamic_cast< vgui::Label * >( FindChildByName( "LblChamberStatLastPublished" ) );
	m_pLblStatPublishedData	= dynamic_cast< vgui::Label * >( FindChildByName( "LblChamberStatLastPublishedData" ) );	
	m_pLblStatRating		= dynamic_cast< vgui::Label * >( FindChildByName( "LblChamberStatRating" ) );
	m_pRatingItem			= dynamic_cast< IconRatingItem * >( FindChildByName( "RatingsItem" ) );
	m_pPuzzleListSpinner	= dynamic_cast< vgui::ImagePanel* >( FindChildByName( "PuzzleListSpinner" ) );
	m_pThumbnailSpinner		= dynamic_cast< vgui::ImagePanel* >( FindChildByName( "ThumbnailSpinner" ) );

	// Autosave related items
	m_pLblAutoSaveFound = static_cast<vgui::Label*>( FindChildByName( "LblAutoSaveFound" ) );
	m_pAutoSaveInfoImage = static_cast<vgui::ImagePanel*>( FindChildByName( "InfoIcon" ) );

	// Always attempt to select the chamber list first
    if ( m_pChamberList )
	{
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom();
		}
		m_pChamberList->NavigateTo();
	}

	// Start populating the maps from disk
	PopulateChamberListFromDisk();

	if ( m_nAutoSaveIndex != -1 )
	{
		UpdateFooter();
	}

	// If we didn't find anything, launch right away
	if ( !m_bInEditor && m_SavedChambers.Count() == 1 )
	{
		// There are no active chambers currently, so start right into the editor
		LoadChamber( 0 );
	}

	m_bSetupComplete = true;
}

//-----------------------------------------------------------------------------
// Purpose:	Sort the map list by download time (starting with the newest on top)
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::ShowPuzzleInformation( bool bShow )
{
	if ( m_pChamberList )
		m_pChamberList->SetVisible( bShow );

	if ( m_pChamberSortButton )
		m_pChamberSortButton->SetVisible( bShow );

	if ( m_pThumbnailImage )
		m_pThumbnailImage->SetVisible( bShow );

	if ( m_pLblChamberName )
		m_pLblChamberName->SetVisible( bShow );

	if ( m_pLblStatCreated )
		m_pLblStatCreated->SetVisible( bShow );

	if ( m_pLblStatCreatedData )
		m_pLblStatCreatedData->SetVisible( bShow );

	if ( m_pLblStatModified )
		m_pLblStatModified->SetVisible( bShow );

	if ( m_pLblStatModifiedData )
		m_pLblStatModifiedData->SetVisible( bShow );

	if ( m_pLblStatPublished )
		m_pLblStatPublished->SetVisible( bShow );

	if ( m_pLblStatPublishedData )
		m_pLblStatPublishedData->SetVisible( bShow );

	if ( m_pLblStatRating )
		m_pLblStatRating->SetVisible( bShow );

	if ( m_pRatingItem )
		m_pRatingItem->SetVisible( bShow );
}

//-----------------------------------------------------------------------------
// Purpose:	Populate our list once we're ready to go
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::Activate()
{
	BaseClass::Activate();

	// FIXME: This was acting as a catch-all for refreshing the puzzle list when returning from a sub-menu
	//		  We need to convert those cases to use the "RefreshList" message from the sub-menus

	if ( m_bSetupComplete && m_bPuzzlesLoadedFromDisk )
	{
		// Start populating the maps from disk
		PopulateChamberListFromDisk();

		// If we didn't find anything, launch right away
		if ( !m_bInEditor && m_SavedChambers.Count() == 1 )
		{
			// There are no active chambers currently, so start right into the editor
			LoadChamber( 0 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Sort the map list by download time (starting with the newest on top)
//-----------------------------------------------------------------------------
int MyChambersListSortFunc_Status( PuzzleFileInfo_t * const *a, PuzzleFileInfo_t * const *b )
{
	const uint64 &aFileID = (*a)->m_publishInfo.m_uFileID;
	const uint64 &bFileID = (*b)->m_publishInfo.m_uFileID;

	// NOTE: We want published maps to move "up" in our sort
	if ( aFileID == 0 )
		return 1;
	
	if ( aFileID > bFileID )
		return -1;

	return ( aFileID < bFileID ) ? 1 : 0;
}

//-----------------------------------------------------------------------------
// Purpose:	Sort the map list by download time (starting with the newest on top)
//-----------------------------------------------------------------------------
int MyChambersListSortFunc_LastModified( PuzzleFileInfo_t * const *a, PuzzleFileInfo_t * const *b )
{
	const unsigned &aTimestamp = (*a)->m_publishInfo.m_uTimeStamp_Modified;
	const unsigned &bTimestamp = (*b)->m_publishInfo.m_uTimeStamp_Modified;

	// NOTE: We want newer maps to move "up" in our sort
	if ( aTimestamp > bTimestamp )
		return -1;

	return ( aTimestamp < bTimestamp ) ? 1 : 0;
}

//-----------------------------------------------------------------------------
// Purpose:	Set the selected chamber and all the book keeping that goes with it
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::SetSelectedChamber( int nIndex )
{
	// Select the first item
	m_nSelectedListItemIndex = nIndex;
	m_pChamberList->SelectPanelItem( m_nSelectedListItemIndex );
	m_pSelectedListPanel = static_cast< CChamberListItem * >(m_pChamberList->GetSelectedPanelItem());
}

//-----------------------------------------------------------------------------
// Purpose:	Set the status label (right-justified text) for the item
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::SetStatusLabelForListItem( CChamberListItem *pItem, PuzzleFileInfo_t *pPuzzleInfo )
{
	PublishedFileId_t unFileID = pPuzzleInfo->m_publishInfo.m_uFileID;
	
	switch( m_sortType )
	{
	case CHAMBER_SORT_STATUS:
		{
			if ( unFileID != 0 )
			{
				wchar_t statusText[256];
				statusText[0] = 0;

				const PublishedFileInfo_t *pInfo = WorkshopManager().GetPublishedFileInfoByID( unFileID );
				if ( pInfo != NULL )
				{
					// See if the version on disk is newer than our last update to Workshop
					int unTimeDiff = abs( (int) ( pPuzzleInfo->m_publishInfo.m_uTimeStamp_Modified - pInfo->m_rtimeUpdated ) );
					if ( unTimeDiff > 30 )
					{
						// Add an asterisk to denote that we're not up-to-date on disk
						V_wcsncpy( statusText, L"*", ARRAYSIZE(statusText) );
					}
				
					V_wcsncat( statusText, g_pVGuiLocalize->Find( "#PORTAL2_EditorMenu_Status_Published" ), ARRAYSIZE( statusText ) );
					pItem->SetChamberStatus( statusText );
				}
				else
				{
					// TODO: In this case our published data is bogus and we need to clear it out

					// Set the text to be "local" instead
					wchar_t *statusText = g_pVGuiLocalize->Find( "#PORTAL2_EditorMenu_Status_Local" );
					pItem->SetChamberStatus( statusText );
				}
			}
			else
			{
				wchar_t *statusText = g_pVGuiLocalize->Find( "#PORTAL2_EditorMenu_Status_Local" );
				pItem->SetChamberStatus( statusText );
			}
		}
		break;
	
	case CHAMBER_SORT_MODIFIED:
		{
			wchar_t szDateText[128];
			FormatFileTimeString( pPuzzleInfo->m_publishInfo.m_uTimeStamp_Modified, szDateText, ARRAYSIZE(szDateText) );
			pItem->SetChamberStatus( szDateText );
		}
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Sort the chambers based on our current sort selection and add them into our list
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::SortAndListChambers( void )
{
	// We can't sort if we're not done loading
	if ( m_bPuzzlesLoadedFromDisk == false )
	{
		Assert( m_bPuzzlesLoadedFromDisk );
		return;
	}

	// Nuke all pre-existing panel items
	m_pChamberList->RemoveAllPanelItems();
    m_ActiveControl = NULL;

	// Take the new chamber stub item out before we sort
	m_SavedChambers.FindAndRemove( m_pNewChamberStub );

	// Sort by the proper type
	if ( m_sortType == CHAMBER_SORT_STATUS )
	{
		m_SavedChambers.Sort( MyChambersListSortFunc_Status );
	}
	else if ( m_sortType == CHAMBER_SORT_MODIFIED )
	{
		m_SavedChambers.Sort( MyChambersListSortFunc_LastModified );
	}

	// Replace our new chamber stub unless we're in the editor
	if ( m_bInEditor == false )
	{
		m_SavedChambers.AddToHead( m_pNewChamberStub );
	}

	//If an autosave was found
	if ( m_nAutoSaveIndex != -1 )
	{
		for ( int i = 0; i < m_SavedChambers.Count(); ++i )
		{
			if ( V_strstr( m_SavedChambers[i]->m_publishInfo.m_strPuzzleFileName, "autosave.p2c" ) )
			{
				m_nAutoSaveIndex = i;
				break;
			}
		}
	}

	// Now add all the real puzzles underneath the top item
	for ( int i = 0; i < m_SavedChambers.Count(); ++i )
	{
		if ( i == m_nAutoSaveIndex )
			continue;
			
		CChamberListItem *pItem = m_pChamberList->AddPanelItem<CChamberListItem>( "puzzlemaker_chamberitem" );
		pItem->SetChamberInfoIndex( i );
		pItem->SetPrimaryText( m_SavedChambers[i]->m_publishInfo.m_strPuzzleTitle );
		
		if ( m_bInEditor || ( !m_bInEditor && i != 0 ) )
		{
			// Setup the status based on how we're sorting
			SetStatusLabelForListItem( pItem, m_SavedChambers[i] );
		}
		else
		{
			// The new chamber stub has no status
			pItem->SetChamberStatus( L"" );
		}
	}

	UpdateFooter();
}


//-----------------------------------------------------------------------------
// Purpose:	Find all of our puzzles on disk and collect their information for display
//	 FIXME: We should only hit the disk on required refreshes, so this code should be refactored
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::PopulateChamberListFromDisk( bool bForceUpdate /*= false*/ )
{
	// Unless forced to, prefer to not hit the disk again
	if ( m_bPuzzlesLoadedFromDisk && !bForceUpdate )
		return;

	// Reset our global state
	m_bDeferredFinishCompleted = false;
	m_bPuzzlesLoadedFromDisk = false;
	m_nSelectedListItemIndex = 0;

	// Hide all of our information while we're loading puzzles
	ShowPuzzleInformation( false );

	//Clear old files
	m_SavedChambers.FindAndRemove( m_pNewChamberStub );
	m_SavedChambers.PurgeAndDeleteElements();
	m_nAutoSaveIndex = -1;

	//Get the save directory
	const char *pSaveDir = engine->GetSaveDirName();
	char szSaveDir[MAX_PATH];
	V_strncpy( szSaveDir, pSaveDir, V_strlen( pSaveDir ) );
	V_FixSlashes( szSaveDir );

	//Isolate the steam ID
	char *pLastSeparator = V_strrchr( szSaveDir, CORRECT_PATH_SEPARATOR );
	if ( !pLastSeparator )
	{
		return;
	}
	//Increment by 1 to get just the ID
	pLastSeparator += 1;

	//Store the steam ID
	V_strncpy( m_szSteamID, pLastSeparator, V_strlen( pLastSeparator ) + 1 );

	char szPuzzlePath[MAX_PATH];
	char szMapPath[MAX_PATH];

	//Set the puzzle and map path
	V_snprintf( szPuzzlePath, sizeof( szPuzzlePath ), "puzzles%c%s%c", CORRECT_PATH_SEPARATOR, m_szSteamID, CORRECT_PATH_SEPARATOR );
	V_snprintf( szMapPath, sizeof( szMapPath ), "maps%cpuzzlemaker%c%s%c", CORRECT_PATH_SEPARATOR, CORRECT_PATH_SEPARATOR, m_szSteamID, CORRECT_PATH_SEPARATOR );
	//V_StrSubst( pSaveDir, "save", "puzzles", m_szPuzzlePath, sizeof( m_szPuzzlePath ), false );
	//V_StrSubst( pSaveDir, "save", "maps", m_szMapPath, sizeof( m_szMapPath ), false );

	//Search characters for the puzzles
	char szPuzzleSearchPath[MAX_PATH];
	V_snprintf( szPuzzleSearchPath, sizeof( szPuzzleSearchPath ), "%s*.p2c", szPuzzlePath );

	V_FixSlashes( szPuzzlePath );
	V_FixSlashes( szMapPath );
	V_FixSlashes( szPuzzleSearchPath );

	//Create the directories if they don't exist
	g_pFullFileSystem->CreateDirHierarchy( szPuzzlePath );
	g_pFullFileSystem->CreateDirHierarchy( szMapPath );

	if ( !m_bInEditor )
	{
		// If we don't have a stub already filled out, create it
		if ( m_pNewChamberStub == NULL )
		{
			m_pNewChamberStub = new PuzzleFileInfo_t;
			const wchar_t *pwszNewChamber = g_pVGuiLocalize->Find( "#PORTAL2_CommunityPuzzle_NewChamber" );
			char szNewChamber[MAX_PATH];
			V_wcstostr( pwszNewChamber, V_wcslen( pwszNewChamber ) + 1, szNewChamber, sizeof( szNewChamber ) );
			m_pNewChamberStub->m_publishInfo.m_strPuzzleTitle = szNewChamber;
			m_pNewChamberStub->m_publishInfo.m_strScreenshotFileName = "puzzles/newchamber.jpg";
			m_pNewChamberStub->m_publishInfo.m_uFileID = 0;
		}
		
		// Add this element into the list as a stub
		m_SavedChambers.AddToTail( m_pNewChamberStub );
	}

	//Iterate over the puzzle files
	FileFindHandle_t hFind = -1;
	const char *pszFileName = g_pFullFileSystem->FindFirst( szPuzzleSearchPath, &hFind );
	while ( pszFileName )
	{
		char szFullFileName[MAX_PATH];
		V_snprintf( szFullFileName, sizeof( szFullFileName ), "%s%s", szPuzzlePath, pszFileName );
		 
		//Don't add the autosave file if in the editor
		if ( m_bInEditor && V_strstr( szFullFileName, "autosave.p2c" ) )
		{
			//Find the next file
			pszFileName = g_pFullFileSystem->FindNext( hFind );
		}
		else if ( V_strstr( szFullFileName, "error.p2c" ) || V_strstr( szFullFileName, ".p2c_" ) )
		{
			//Find the next file
			pszFileName = g_pFullFileSystem->FindNext( hFind );
		}
		else
		{
			PuzzleFileInfo_t *pNewPuzzle = new PuzzleFileInfo_t;
			int nIndex = m_SavedChambers.AddToTail( pNewPuzzle );

			// Store steam ID
			pNewPuzzle->m_publishInfo.m_strSteamIDPath = m_szSteamID + CORRECT_PATH_SEPARATOR;

			//The file name of the puzzle
			pNewPuzzle->m_publishInfo.m_strPuzzleFileName = szFullFileName;

			//Info for the chamber ( title, description )
			GetPuzzleInfoFromFile( szFullFileName, nIndex );

			//Look for possible map
			char szMapName[MAX_PATH];
			char szFileBaseName[MAX_PATH];
			V_FileBase( pszFileName, szFileBaseName, sizeof( szFileBaseName ) );
			V_snprintf( szMapName, sizeof( szMapName ), "%s%s.bsp", szMapPath, szFileBaseName );
			if ( g_pFullFileSystem->FileExists( szMapName, "MOD" ) )
			{
				pNewPuzzle->m_publishInfo.m_strMapFileName = szMapName;
				pNewPuzzle->m_strBaseFileName = szFileBaseName;
			}

			//Look for possible screenshot
			char szScreenshotFilename[MAX_PATH];
			V_snprintf( szScreenshotFilename, sizeof( szScreenshotFilename ), "%s%s.jpg", szPuzzlePath, szFileBaseName );
			if ( g_pFullFileSystem->FileExists( szScreenshotFilename, "MOD" ) )
			{
				pNewPuzzle->m_publishInfo.m_strScreenshotFileName = szScreenshotFilename;
			}

			//Don't display the autosave file
			if ( V_strstr( szFullFileName, "autosave.p2c" ) )
			{
				m_nAutoSaveIndex = nIndex;
			}

			//Find the next file
			pszFileName = g_pFullFileSystem->FindNext( hFind );
		}
	}
	//End find
	g_pFullFileSystem->FindClose( hFind );

	// If we didn't find anything, mark that we're complete immediately
	if ( m_SavedChambers.Count() == 1 && m_PuzzleInfoFileRequests.Count() == 0 )
	{
		m_bPuzzlesLoadedFromDisk = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Get the puzzle info for a file from disk
//	 FIXME: We should plumb this into the puzzle editor so it can handle its own data - this doesn't account for versioning!
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::GetPuzzleInfoFromFile( const char *pszFileName, int puzzleInfoIndex )
{
	AsyncPuzzleInfoRequest_t *pRequest = new AsyncPuzzleInfoRequest_t;
	pRequest->nPuzzleInfoIndex = puzzleInfoIndex;
	V_strncpy( pRequest->szFilename, pszFileName, ARRAYSIZE(pRequest->szFilename) );
	
	// Add it to the list of puzzles that we're trying to grab info for
	m_PuzzleInfoFileRequests.Insert( pRequest );
}

//-----------------------------------------------------------------------------
// Purpose:	After all puzzle information is loaded off disk, layout the dialog with that data
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::FinishPuzzleInfoLoad( void )
{
	m_bDeferredFinishCompleted = true;
	m_bPuzzlesLoadedFromDisk = true;
	ShowPendingFileActionsSpinner( false );
	if ( m_pChamberSortButton )
	{
		m_pChamberSortButton->SetEnabled( true );
	}

	// We have chambers, so sort them as desired and populate our list view
	SortAndListChambers();

	// Display information about the puzzles now
	ShowPuzzleInformation( true );

	// Handle autosaves and selections
	if ( m_nAutoSaveIndex != -1 )
	{
		UpdateFooter();
		PromptToLoadAutoSave();
		SetSelectedChamber( m_nAutoSaveIndex );
		SetupChamberInformation( m_nAutoSaveIndex );
		m_nSelectedListItemIndex = m_nAutoSaveIndex;
	}
	else
	{
		SetSelectedChamber( m_nSelectedListItemIndex );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Get the puzzle info for a file from disk
//	 FIXME: We should plumb this into the puzzle editor so it can handle its own data - this doesn't account for versioning!
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::ReadPuzzleInfoFromBuffer( const FileAsyncRequest_t &asyncRequest, int nNumReadBytes, FSAsyncStatus_t err )
{
	Assert( m_pCurrentPuzzleFileInfoRequest != NULL );

	KeyValues *pPuzzleKeys = new KeyValues( "Puzzle" );
	KeyValues::AutoDelete autoDelete_puzzleKeys( pPuzzleKeys );

	pPuzzleKeys->UsesEscapeSequences( true );

	// The puzzle info comes to us without null termination, but 
	// KeyValues::LoadFromBuffer expects the buffer passed in to be null terminated.
	// Put the buffer from asyncRequest into a CUtlBuffer that will auto-place the null
	// termination for us.
	CUtlBuffer srcBuf;
	srcBuf.SetBufferType( true, true );
	srcBuf.Put( asyncRequest.pData, nNumReadBytes );

	// Load and parse the keyvalues
	pPuzzleKeys->LoadFromBuffer( NULL, (const char *) srcBuf.Base() );

	// Clean up
	srcBuf.Purge();

	PuzzleFilesInfo_t &puzzleInfo = m_SavedChambers[m_pCurrentPuzzleFileInfoRequest->nPuzzleInfoIndex]->m_publishInfo;
	puzzleInfo.m_strPuzzleTitle = pPuzzleKeys->GetString( "Title" );

	// If this is the autosave, then tack on "(Autosave) " onto the front of it
	if( V_strstr( m_pCurrentPuzzleFileInfoRequest->szFilename, "autosave.p2c" ) )
	{
		puzzleInfo.m_strPuzzleTitle = CUtlString("(Autosave) ") + puzzleInfo.m_strPuzzleTitle;
	}

	puzzleInfo.m_strDescription = pPuzzleKeys->GetString( "Description" );
	puzzleInfo.m_uFileID = pPuzzleKeys->GetUint64( "FileID" );

	// Use the current time if the one on disk is invalid
	time_t curTime = time( NULL );
	puzzleInfo.m_uTimeStamp_Created = pPuzzleKeys->GetUint64( "Timestamp_Created", curTime );
	puzzleInfo.m_uTimeStamp_Modified = pPuzzleKeys->GetUint64( "Timestamp_Modified", curTime );

	puzzleInfo.m_bIsCoop = pPuzzleKeys->GetBool( "Coop", false );

	// Free up the data here, we're done
	delete m_pCurrentPuzzleFileInfoRequest;
	m_pCurrentPuzzleFileInfoRequest = NULL;

	// Try to clean up cases where a file has become out of sync with the Workshop
	if ( ResolveOrphanedWorkshopFile( &puzzleInfo ) )
	{
		// There was a change, we want to save this back to disk!
		pPuzzleKeys->SetUint64( "FileID", puzzleInfo.m_uFileID );
		pPuzzleKeys->SaveToFile( g_pFullFileSystem, asyncRequest.pszFilename );
	}

	// If we're done now, show our files
	if ( m_PuzzleInfoFileRequests.Count() == 0 )
	{
		FinishPuzzleInfoLoad();
	}
}


void CPuzzleMakerMyChambers::FixFooter()
{
	CPuzzleMakerMyChambers *pSelf = static_cast<CPuzzleMakerMyChambers*>( CBaseModPanel::GetSingleton().GetWindow( WT_EDITORCHAMBERLIST ) );
	if( pSelf )
	{
		pSelf->UpdateFooter();
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Open the overlay to the Workshop page for this map
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::ViewMapInWorkshop( void )
{
	CChamberListItem* pListItem = static_cast<CChamberListItem *>( m_pChamberList->GetSelectedPanelItem() );
	if ( pListItem )
	{
		Assert( m_SavedChambers.IsValidIndex(m_nSelectedListItemIndex) );
		PublishedFileId_t nFileID = m_SavedChambers[m_nSelectedListItemIndex]->m_publishInfo.m_uFileID;
		Assert( nFileID != 0 );
		if ( nFileID == 0 )
			return;

#if !defined(NO_STEAM)
		OverlayResult_t result = BASEMODPANEL_SINGLETON.ViewCommunityMapInWorkshop( nFileID );

		if( result != RESULT_OK )
		{
			if( result == RESULT_FAIL_OVERLAY_DISABLED )
			{
				GenericConfirmation* confirmation = 
					static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );
				GenericConfirmation::Data_t data;
				data.pWindowTitle = "#L4D360UI_SteamOverlay_Title";
				data.pMessageText = "#L4D360UI_SteamOverlay_Text";
				data.bOkButtonEnabled = true;
				data.pfnOkCallback = CPuzzleMakerMyChambers::FixFooter;
				confirmation->SetUsageData(data);
			}
		}
#endif // !NO_STEAM
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Handle our key presses
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::OnKeyCodePressed( KeyCode code )
{
	int joystick = GetJoystickForCode( code );
	int userId = BASEMODPANEL_SINGLETON.GetLastActiveUserId();
	if ( joystick != userId || joystick < 0 )
	{	
		return;
	}

	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_A:
		{
			if ( m_nAutoSaveIndex != -1 )
			{
				LoadChamber( m_nAutoSaveIndex );
			}
			else
			{
				LoadChamber( m_nSelectedListItemIndex );
			}
		}
		break;
	case KEY_XBUTTON_B:
		{
			// All the way out if we're in the editor
			if( m_bInEditor )
			{
				GameUI().AllowEngineHideGameUI();
				engine->ExecuteClientCmd("gameui_hide");
				BASEMODPANEL_SINGLETON.CloseAllWindows();
			}

		}
		break;
	case KEY_XBUTTON_X: //Rename
		{
			if ( m_nAutoSaveIndex == -1 )
			{
				ShowRenameDialog();
			}
		}
		break;
	case KEY_XBUTTON_Y: //Delete
		{
			if ( m_nAutoSaveIndex != -1 )
			{
				DeletePuzzleAtIndex( m_nAutoSaveIndex );
				
				// Nuke the thumbnail image until a new one is loaded and prepped
				if ( m_pThumbnailImage )
				{
					m_pThumbnailImage->SetVisible( false );
				}

				m_pLblAutoSaveFound->SetVisible( false );
				m_pAutoSaveInfoImage->SetVisible( false );

				// Hide the footer to prevent double-clicking and causing
				// the delete dialog to pop up.
				CBaseModFooterPanel *pFooter = BASEMODPANEL_SINGLETON.GetFooterPanel();
				if ( pFooter )
				{
					pFooter->SetVisible( false );
				}
			}
			else
			{
				ShowDeleteDialog();
			}
		}
		break;
	case KEY_XBUTTON_LEFT_SHOULDER:
		{
			ViewMapInWorkshop();
		}
		break;
	}

	BaseClass::OnKeyCodePressed(code);
}

//-----------------------------------------------------------------------------
// Purpose:	Set the sort order the user desires and resort the list
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::SetSortType( MyChambersSortType_t sortType )
{
	// If nothing has changed, do no work
	if ( m_sortType == sortType )
		return;

	// Take the new sort type
	m_sortType = sortType;

	//Sort the list
	SortAndListChambers();
}

//-----------------------------------------------------------------------------
// Purpose:	Handle commands from our child elements
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::OnCommand(const char *command)
{
	if ( !V_stricmp( "Cancel", command ) || !V_stricmp( "Back", command ) )
	{
		// Don't allow this if we're in the middle of loading
		if ( m_bPuzzlesLoadedFromDisk == false )
			return;

		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, BASEMODPANEL_SINGLETON.GetLastActiveUserId() ) );
	}
	else if ( !V_stricmp( "EditorMenu_ByDate", command ) )
	{
		SetSortType( CHAMBER_SORT_MODIFIED );
	}
	else if ( !V_stricmp( "EditorMenu_ByStatus", command ) )
	{
		SetSortType( CHAMBER_SORT_STATUS );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Handle matchmaking events
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	// Deal with deletion event
	if ( !V_stricmp( "CommunityMap_Deleted", szEvent ) )
	{
		uint64 mapID = pEvent->GetUint64( "mapID", 0 );
		OnPublishedFileDeleted( false, mapID );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Update all our displayed information for the selected map
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::SetupChamberInformation( int nIndex )
{
	// Stop any current transition
	m_flTransitionStartTime = 0;

	// we need to not spam the i/o as users scroll through save games
	// once the input activity settles, THEN we start loading the screenshot
	m_flNextLoadThumbnailTime = Plat_FloatTime() + 0.3f;

	// Make sure to turn this back on
	if ( m_pThumbnailImage )
	{
		m_pThumbnailImage->SetVisible( true );
	}

	// Set our name
	m_pLblChamberName->SetText( m_SavedChambers[nIndex]->m_publishInfo.m_strPuzzleTitle );

	// Disable everything if we're the "New test chamber" item
	if ( !m_bInEditor && nIndex == 0 )
	{
		m_pLblStatCreated->SetEnabled( false );
		m_pLblStatModified->SetEnabled( false );
		m_pLblStatCreatedData->SetEnabled( false );
		m_pLblStatCreatedData->SetText( L"" );
		m_pLblStatModifiedData->SetEnabled( false );
		m_pLblStatModifiedData->SetText( L"" );
		m_pLblStatPublished->SetEnabled( false );
		m_pLblStatPublishedData->SetEnabled( false );
		m_pLblStatRating->SetEnabled( false );
		m_pRatingItem->SetEnabled( false );
		m_pRatingItem->SetRating( 0.0f );
		m_pLblStatPublishedData->SetText( L"" );
		
		return;
	}
		
	// Enable these if the selection is valid
	m_pLblStatCreated->SetEnabled( true );
	m_pLblStatModified->SetEnabled( true );

	// Setup our file timestamps
	wchar_t	szData[256];
	FormatFileTimeString( m_SavedChambers[nIndex]->m_publishInfo.m_uTimeStamp_Created, szData, ARRAYSIZE(szData) );
	m_pLblStatCreatedData->SetEnabled( true );
	m_pLblStatCreatedData->SetText( szData );
	
	FormatFileTimeString( m_SavedChambers[nIndex]->m_publishInfo.m_uTimeStamp_Modified, szData, ARRAYSIZE(szData) );
	m_pLblStatModifiedData->SetEnabled( true );
	m_pLblStatModifiedData->SetText( szData );

	// Add more details if we're a published map
	const PublishedFileInfo_t *pInfo = WorkshopManager().GetPublishedFileInfoByID( m_SavedChambers[nIndex]->m_publishInfo.m_uFileID );
	if ( pInfo != NULL )
	{
		FormatFileTimeString( pInfo->m_rtimeUpdated, szData, ARRAYSIZE(szData) );
		m_pLblStatPublished->SetEnabled( true );
		m_pLblStatPublishedData->SetEnabled( true );
		m_pLblStatPublishedData->SetText( szData );

		m_pLblStatRating->SetEnabled( true );
		m_pRatingItem->SetEnabled( true );
		float flVoteScore;
		if ( pInfo->GetVoteData( &flVoteScore, NULL, NULL ) )
		{
			m_pRatingItem->SetEnabled( true );
			m_pRatingItem->SetRating( flVoteScore );
		}
		else
		{
			m_pRatingItem->SetEnabled( false );
			m_pRatingItem->SetRating( 0.0f );
		}
	}
	else
	{
		// If we're not a published map, hide this information
		m_pLblStatPublished->SetEnabled( false );
		m_pLblStatPublishedData->SetEnabled( false );
		m_pLblStatRating->SetEnabled( false );
		m_pRatingItem->SetEnabled( false );
		m_pRatingItem->SetRating( 0.0f );
		m_pLblStatPublishedData->SetText( L"" );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Ask the user what they want to do with their autosave
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::PromptToLoadAutoSave( void )
{
	if ( m_pChamberList )
		m_pChamberList->SetVisible( false );

	if ( m_pChamberSortButton )
		m_pChamberSortButton->SetVisible( false );

	if ( m_pLblAutoSaveFound )
		m_pLblAutoSaveFound->SetVisible( true );

	if ( m_pAutoSaveInfoImage )
		m_pAutoSaveInfoImage->SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose:	Update our dialog when a new list item is selected
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::NewPanelSelected( CChamberListItem *pSelectedPanel )
{
	Assert( pSelectedPanel );

	m_pSelectedListPanel = pSelectedPanel;

	//Selected panel changed
	m_nSelectedListItemIndex = pSelectedPanel->GetChamberInfoIndex();
	
	// Change all the labels for this selected map
	SetupChamberInformation( m_nSelectedListItemIndex );

	// Update the options for the selected item
	UpdateFooter();
}

//-----------------------------------------------------------------------------
// Purpose:	An item in our list has been selected
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::OnItemSelected( const char *pPanelName )
{
	if ( m_bSetupComplete == false || m_bPuzzlesLoadedFromDisk == false )
		return;

	CChamberListItem *pNewSelectedPanel = static_cast<CChamberListItem *>(m_pChamberList->GetSelectedPanelItem());
	NewPanelSelected( pNewSelectedPanel );

	// Set active state
	for ( int i = 0; i < m_pChamberList->GetPanelItemCount(); i++ )
	{
		CChamberListItem *pItem = static_cast< CChamberListItem* >( m_pChamberList->GetPanelItem( i ) );
		if ( pItem )
		{
			pItem->SetSelected( pItem == pNewSelectedPanel );
		}
	}

	m_ActiveControl = pNewSelectedPanel;
}

//-----------------------------------------------------------------------------
// Purpose:	Update our dialog when a new list item is selected
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::OnThink()
{
	BaseClass::OnThink();
}

//-----------------------------------------------------------------------------
// Purpose:	Deal with per-frame activities
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::ShowPendingFileActionsSpinner( bool bState )
{
	if ( m_pPuzzleListSpinner )
	{
		m_pPuzzleListSpinner->SetVisible( bState );
	}

	// Hide or display any other relevant visual items here
}

//-----------------------------------------------------------------------------
// Purpose:	Callback for completion of screenshot loading from disk
//-----------------------------------------------------------------------------
void PuzzleFileInfoLoaded( const FileAsyncRequest_t &asyncRequest, int numReadBytes, FSAsyncStatus_t err )
{
	CPuzzleMakerMyChambers *pDialog = static_cast< CPuzzleMakerMyChambers* >( BASEMODPANEL_SINGLETON.GetWindow( WT_EDITORCHAMBERLIST ) );
	if ( pDialog )
	{
		pDialog->ReadPuzzleInfoFromBuffer( asyncRequest, numReadBytes, err );
	}
}	

//-----------------------------------------------------------------------------
// Purpose:	Push our async file requests forward
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::UpdateFileInfoRequests( void )
{
	// If there are puzzles to collect info off disk for, then we need to do so
	if ( m_PuzzleInfoFileRequests.Count() == 0 )
		return;

	// Pop the spinner up
	ShowPendingFileActionsSpinner( true );

	// Make sure we're not in the middle of another operation
	if ( m_hPuzzleInfoAsyncControl )
	{
		FSAsyncStatus_t status = g_pFullFileSystem->AsyncStatus( m_hPuzzleInfoAsyncControl );
		switch ( status )
		{
		case FSASYNC_STATUS_PENDING:
		case FSASYNC_STATUS_INPROGRESS:
		case FSASYNC_STATUS_UNSERVICED:
			{
				// i/o in progress, caller must retry
				return ;
			}
		}

		// Finished
		g_pFullFileSystem->AsyncFinish( m_hPuzzleInfoAsyncControl );
		g_pFullFileSystem->AsyncRelease( m_hPuzzleInfoAsyncControl );
		m_hPuzzleInfoAsyncControl = NULL;
	}

	// If nothing is currently pending, then start off a new request
	if ( m_pCurrentPuzzleFileInfoRequest == NULL )
	{
		m_pCurrentPuzzleFileInfoRequest = m_PuzzleInfoFileRequests.RemoveAtHead();

		FileAsyncRequest_t request;
		request.pszFilename = m_pCurrentPuzzleFileInfoRequest->szFilename;
		request.pfnCallback = ::PuzzleFileInfoLoaded;
		request.pContext = NULL;
		request.flags = FSASYNC_FLAGS_FREEDATAPTR;

		// schedule the async operation
		g_pFullFileSystem->AsyncRead( request, &m_hPuzzleInfoAsyncControl );	
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Deal with updating spinner animation and whether they should be shown or not
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::UpdateSpinners( void )
{
	// Frame for all spinner this tick
	const int nAnimFrame = ( uint64 )( Plat_FloatTime() * 10 );

	// Update the spinner's spinning
	if ( m_pPuzzleListSpinner )
	{
		m_pPuzzleListSpinner->SetFrame( nAnimFrame );
	}

	// Hande whether the thumbnail spinner should be visible
	if ( m_pThumbnailSpinner )
	{
		m_pThumbnailSpinner->SetFrame( nAnimFrame );

		const bool bThumbnailLoading = ( m_bPuzzlesLoadedFromDisk == true ) && ( m_flNextLoadThumbnailTime || m_nThumbnailImageId == -1 );
		m_pThumbnailSpinner->SetVisible( bThumbnailLoading );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Deal with per-frame activities
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::RunFrame()
{
	BaseClass::RunFrame();

	// This catches the case where the population of the list occurs immediately and before the ApplySchemeSettings function is called.
	// If we do this too soon, we don't have valid pointers to the members inside the dialog we need to update, so we have to wait until
	// we've signaled that function has been called. -- jdw
	if ( m_bSetupComplete && m_bPuzzlesLoadedFromDisk && m_bDeferredFinishCompleted == false )
	{
		// If we don't have anything in the list then we only have a new chamber to load
		if ( m_SavedChambers.Count() == 1 && m_PuzzleInfoFileRequests.Count() == 0 )
		{
			FinishPuzzleInfoLoad();
		}
		
		m_bDeferredFinishCompleted = true;
	}

	// Push our async file info requests forward
	UpdateFileInfoRequests();
	
	// Animate our spinners
	UpdateSpinners();

	// Handle thumbnails finishing their loads
	if ( !m_flTransitionStartTime && m_flNextLoadThumbnailTime && Plat_FloatTime() >= m_flNextLoadThumbnailTime )
	{
		// Start our load of this file
		StartAsyncScreenshotLoad( m_SavedChambers[m_nSelectedListItemIndex]->m_publishInfo.m_strScreenshotFileName );	
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Callback for completion of screenshot loading from disk
//-----------------------------------------------------------------------------
void MyChambersScreenshotLoaded( const FileAsyncRequest_t &asyncRequest, int numReadBytes, FSAsyncStatus_t err )
{
	CPuzzleMakerMyChambers *pDialog = static_cast< CPuzzleMakerMyChambers* >( BASEMODPANEL_SINGLETON.GetWindow( WT_EDITORCHAMBERLIST ) );
	if ( pDialog )
	{
		pDialog->ScreenshotLoaded( asyncRequest, numReadBytes, err );
	}
}	

//-----------------------------------------------------------------------------
// Purpose:	Get a screenshot off of disk
//-----------------------------------------------------------------------------
bool CPuzzleMakerMyChambers::StartAsyncScreenshotLoad( const char *pThumbnailFilename )
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

	FileAsyncRequest_t request;
	request.pszFilename = pThumbnailFilename;
	request.pfnCallback = ::MyChambersScreenshotLoaded;
	request.pContext = NULL;
	request.flags = FSASYNC_FLAGS_FREEDATAPTR;

	// schedule the async operation
	g_pFullFileSystem->AsyncRead( request, &m_hAsyncControl );	

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:	Screenshot is loaded, display it
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::ScreenshotLoaded( const FileAsyncRequest_t &asyncRequest, int nNumReadBytes, FSAsyncStatus_t err )
{
	int nSaveGameImageId = -1;

	if ( err == FSASYNC_OK )
	{
		int nWidth, nHeight;
		CUtlBuffer srcBuf;
		srcBuf.SetExternalBuffer( asyncRequest.pData, nNumReadBytes, 0, CUtlBuffer::READ_ONLY );
		CUtlBuffer destBuffer;

		// Read the preview JPEG to RGB
		if ( ImgUtl_ReadJPEGAsRGBA( srcBuf, destBuffer, nWidth, nHeight ) == CE_SUCCESS )
		{
			// success
			if ( m_nThumbnailImageId == -1 )
			{
				// Create a procedural texture id
				m_nThumbnailImageId = vgui::surface()->CreateNewTextureID( true );
			}

			// Write this to the texture so we can draw it
			surface()->DrawSetTextureRGBALinear( m_nThumbnailImageId, (unsigned char*)destBuffer.Base(), nWidth, nHeight );
			
			// Done with this memory now
			destBuffer.Purge();
			
			// transition into the image
			nSaveGameImageId = m_nThumbnailImageId;
			m_flTransitionStartTime = Plat_FloatTime() + 0.1f;
			m_flNextLoadThumbnailTime = 0;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Setup our footer element based on what's selected
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BASEMODPANEL_SINGLETON.GetFooterPanel();
	if ( pFooter )
	{
		pFooter->SetVisible( true );

		int nButtons = (FB_ABUTTON|FB_BBUTTON);

		if ( m_bInEditor )
		{
			pFooter->SetButtonText( FB_ABUTTON, "#PORTAL2_PuzzleMaker_OpenPuzzle" );
			pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Cancel" );
		}
		else
		{
			if ( m_nAutoSaveIndex != -1 )
			{
				nButtons |= FB_YBUTTON;
				pFooter->SetButtonText( FB_ABUTTON, "#PORTAL2_ButtonAction_Load" );
				// pFooter->SetButtonText( FB_XBUTTON, "    " ); // FIXME: We need a better solution for this
				pFooter->SetButtonText( FB_YBUTTON, "#PORTAL2_ButtonAction_Discard" );
			}
			else if ( m_nSelectedListItemIndex == 0 ) //Create new
			{
				pFooter->SetButtonText( FB_ABUTTON, "#PORTAL2_PuzzleMaker_CreatePuzzle" );
			}
			else
			{
				// Start with our core options that apply in all cases
				nButtons |= (FB_XBUTTON|FB_YBUTTON);
				pFooter->SetButtonText( FB_ABUTTON, "#PORTAL2_PuzzleMaker_EditPuzzle" );
				pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );				
				pFooter->SetButtonText( FB_XBUTTON, "#PORTAL2_PuzzleMaker_RenamePuzzle" );
				
				// Attempt to get information about this published file
				Assert( m_SavedChambers.IsValidIndex(m_nSelectedListItemIndex) );
				PublishedFileId_t nFileID = m_SavedChambers[m_nSelectedListItemIndex]->m_publishInfo.m_uFileID;
				const PublishedFileInfo_t *pInfo = WorkshopManager().GetPublishedFileInfoByID( nFileID );
				if ( pInfo != NULL )
				{
					// This is a published file, change up our options
					pFooter->SetButtonText( FB_YBUTTON, "#PORTAL2_PuzzleMaker_UnpublishPuzzle" );

					// Allow us to view the map in the Workshop
					nButtons |= FB_LSHOULDER;
					pFooter->SetButtonText( FB_LSHOULDER, "#PORTAL2_CommunityPuzzle_ViewInWorkshop" );
				}
				else
				{
					pFooter->SetButtonText( FB_YBUTTON, "#PORTAL2_PuzzleMaker_DeletePuzzle" );
				}			
			}

		}

		pFooter->SetButtons( nButtons );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Load the chamber and launch into the editor
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::LoadChamber( int nIndex )
{
	const PuzzleFileInfo_t *pPuzzleInfo = m_SavedChambers[nIndex];

	char szMapName[MAX_PATH];

	char szMapPath[MAX_PATH];
	V_snprintf( szMapPath, sizeof( szMapPath ), "puzzlemaker%c", CORRECT_PATH_SEPARATOR );
	bool bLaunchAsCoop = false;

	//0 means new chamber when we're not in-editor
	if ( nIndex == 0 && !m_bInEditor )
	{
		//Start a new chamber
		V_snprintf( szMapName, sizeof( szMapName ), "%snewchamber", szMapPath );
		g_pPuzzleMaker->NewPuzzle( true );
	}
	else //Selected an existing chamber
	{
		if ( pPuzzleInfo->m_publishInfo.m_strMapFileName.IsEmpty() )
		{
			//Start a new chamber
			V_snprintf( szMapName, sizeof( szMapName ), "%snewchamber", szMapPath );
		}
		else
		{
			// Map file exists!  Use it!
			V_snprintf( szMapName, sizeof( szMapName ), "%s%s%c%s", szMapPath, m_szSteamID, CORRECT_PATH_SEPARATOR, pPuzzleInfo->m_strBaseFileName.Get() );
			bLaunchAsCoop = pPuzzleInfo->m_publishInfo.m_bIsCoop;
		}
		g_pPuzzleMaker->LoadPuzzle( pPuzzleInfo->m_publishInfo.m_strPuzzleFileName );
	}
	
	g_pPuzzleMaker->SetActive( true );

	// Use the session switching concommand.  This uses the FadeOutStartGame panel.
	const char* pszSessionType = bLaunchAsCoop ? "mp" : "sp";
	char szCommand[MAX_PATH];
	V_snprintf( szCommand, sizeof( szCommand ), "puzzlemaker_show 1; puzzlemaker_switch_session %s %s", szMapName, pszSessionType );
	engine->ClientCmd_Unrestricted( szCommand );
}


void CPuzzleMakerMyChambers::ShowRenameDialog()
{
	//Load the selected puzzle in the editor
	const PuzzleFileInfo_t *puzzleInfo = m_SavedChambers[m_nSelectedListItemIndex];
	g_pPuzzleMaker->LoadPuzzle( puzzleInfo->m_publishInfo.m_strPuzzleFileName );

	CPuzzleMakerSaveDialog *pSaveDialog = static_cast<CPuzzleMakerSaveDialog*>(BASEMODPANEL_SINGLETON.OpenWindow( WT_PUZZLEMAKERSAVEDIALOG, this, true ));
	pSaveDialog->SetReason( PUZZLEMAKER_SAVE_RENAME );
	pSaveDialog->SetScreenshotName( puzzleInfo->m_publishInfo.m_strScreenshotFileName );
}


//-----------------------------------------------------------------------------
// Purpose:	Pass this back up the chain
//-----------------------------------------------------------------------------
void DeletePuzzle( void )
{
	CPuzzleMakerMyChambers *pSelf = static_cast<CPuzzleMakerMyChambers*>( BASEMODPANEL_SINGLETON.GetWindow( WT_EDITORCHAMBERLIST ) );
	if ( pSelf )
	{
		pSelf->DeleteSelectedPuzzle();
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Pass this back up the chain
//-----------------------------------------------------------------------------
void UnpublishPuzzle( void )
{
	CPuzzleMakerMyChambers *pSelf = static_cast<CPuzzleMakerMyChambers*>( BASEMODPANEL_SINGLETON.GetWindow( WT_EDITORCHAMBERLIST ) );
	if ( pSelf )
	{
		pSelf->UnpublishSelectedPuzzle();
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Pop up a warning dialog, then continue if the user consents
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::ShowDeleteDialog( void )
{
	//Cache off the current selected index as the panel will repopulate and reset the index before the callback is called
	m_nDeleteIndex = m_nSelectedListItemIndex; 

	GenericConfirmation *pConfirmation = 
		static_cast< GenericConfirmation* >( BASEMODPANEL_SINGLETON.OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

	GenericConfirmation::Data_t data;

	Assert( m_SavedChambers.IsValidIndex(m_nSelectedListItemIndex) );
	PublishedFileId_t nFileID = m_SavedChambers[m_nSelectedListItemIndex]->m_publishInfo.m_uFileID;
	const PublishedFileInfo_t *pInfo = WorkshopManager().GetPublishedFileInfoByID( nFileID );
	if ( pInfo != NULL )
	{
		// "Unpublish" command
		data.pWindowTitle = "#PORTAL2_PuzzleMaker_UnpublishPuzzleConfirm";
		data.pMessageText = "#PORTAL2_PuzzleMaker_UnpublishPuzzleConfirmMsg";
		data.pOkButtonText = "#PORTAL2_PuzzleMaker_UnpublishPuzzle";
		data.pfnOkCallback = &UnpublishPuzzle;
	}
	else
	{
		// "Delete" command
		data.pWindowTitle = "#PORTAL2_PuzzleMaker_DeletePuzzleConfirm";
		data.pMessageText = "#PORTAL2_PuzzleMaker_DeletePuzzleConfirmMsg";
		data.pOkButtonText = "#PORTAL2_PuzzleMaker_DeletePuzzle";
		data.pfnOkCallback = &DeletePuzzle;
	}

	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = true;
	data.pCancelButtonText = "#L4D360UI_Cancel";

	pConfirmation->SetUsageData( data );
}

//-----------------------------------------------------------------------------
// Purpose:	Published file deletion has completed
//-----------------------------------------------------------------------------
void OnPublishedFileDeleted( bool bError, PublishedFileId_t nID )
{
	CPuzzleMakerMyChambers *pSelf = static_cast<CPuzzleMakerMyChambers*>( BASEMODPANEL_SINGLETON.GetWindow( WT_EDITORCHAMBERLIST ) );
	if ( pSelf )
	{
		pSelf->OnPublishedFileDeleted( bError, nID );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Published file deletion has completed
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::OnPublishedFileDeleted( bool bError, PublishedFileId_t nID )
{
    // Done, close the wait screen
    CUIGameData::Get()->CloseWaitScreen( NULL, NULL );

	// Nuke the 
	Assert( bError == false );
	Assert( m_SavedChambers.IsValidIndex(m_nDeleteIndex) );
	PuzzleFileInfo_t *pFileInfo = m_SavedChambers[m_nDeleteIndex];
	g_pPuzzleMaker->LoadPuzzle( pFileInfo->m_publishInfo.m_strPuzzleFileName );
	
	// ICK
	PuzzleFilesInfo_t puzzleInfo = g_pPuzzleMaker->GetPuzzleInfo();
	puzzleInfo.m_uFileID = 0;
	g_pPuzzleMaker->SetPuzzleInfo( puzzleInfo );
	g_pPuzzleMaker->SavePuzzle( false );

	// Update our status
	SortAndListChambers();

    // FIXME: We lost what we were after, so now we just select the top of the list
    SetSelectedChamber( 0 );

    m_nDeleteIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose:	Unpublish the currently selected file
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::UnpublishSelectedPuzzle( void )
{
	if ( m_nDeleteIndex != 0 && m_nDeleteIndex < m_SavedChambers.Count() )
	{
		// Ask the base panel to do the work
		const PuzzleFileInfo_t *puzzleInfo = m_SavedChambers[m_nDeleteIndex];		
		if ( WorkshopManager().DeletePublishedFile( puzzleInfo->m_publishInfo.m_uFileID ) )
		{
			// Pop a wait screen
			CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_CommunityPuzzle_WaitForDeletion", 1.0f );
		}
		else
		{
			// Something went wrong!
			Assert( 0 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Delete the currently selected file
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::DeleteSelectedPuzzle()
{
	DeletePuzzleAtIndex( m_nDeleteIndex );

	m_nDeleteIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose:	Delete a puzzle at a specified index
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::DeletePuzzleAtIndex( int nIndex )
{
	if ( nIndex > 0 && nIndex < m_SavedChambers.Count() )
	{
		PuzzleFileInfo_t *puzzleInfo = m_SavedChambers[nIndex];
		g_pFullFileSystem->RemoveFile( puzzleInfo->m_publishInfo.m_strPuzzleFileName );
		g_pFullFileSystem->RemoveFile( puzzleInfo->m_publishInfo.m_strMapFileName );
		g_pFullFileSystem->RemoveFile( puzzleInfo->m_publishInfo.m_strScreenshotFileName );

		//Repopulate and sort the list
		PopulateChamberListFromDisk( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Change the footer type if we're currently in the editor
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::UseInEditorFooter()
{
	m_bInEditor = true;
	UpdateFooter();
	SetDialogTitle( "#PORTAL2_PuzzleMaker_OpenPuzzle" );
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::OnGameUIHidden()
{
	Close();
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::PaintBackground( void )
{
	BaseClass::PaintBackground();

	DrawThumbnailImage();
}

//-----------------------------------------------------------------------------
// Purpose:	Handle fading in / out our thumbnail image
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::DrawThumbnailImage( void )
{
	// Don't draw anything else in this frame
	if ( !m_pThumbnailImage || m_nThumbnailImageId == -1 || m_pThumbnailImage->IsVisible() == false )
		return;

	// If a load is coming up, don't display
	if ( m_flNextLoadThumbnailTime )
		return;

	float flLerp = 1.0;
	if ( m_flTransitionStartTime )
	{
		flLerp = RemapValClamped( Plat_FloatTime(), m_flTransitionStartTime, m_flTransitionStartTime + 0.3f, 0.0f, 1.0f );
		if ( flLerp >= 1.0f )
		{
			// finished transition
			m_flTransitionStartTime = 0;
		}
	}

	//Draw the screenshot
	if ( m_nThumbnailImageId != -1 )
	{
		int x, y, wide, tall;
		m_pThumbnailImage->GetBounds( x, y, wide, tall );

		int nTextureWide, nTextureTall;
		surface()->DrawGetTextureSize( m_nThumbnailImageId, nTextureWide, nTextureTall );
		float flScale = MIN( (float)wide/nTextureWide, (float)tall/nTextureTall );
		int nFinalWidth = nTextureWide * flScale;
		int nFinalHeight = nTextureTall * flScale;
		int xPos = x + ( wide/2 ) - ( nFinalWidth/2 );
		int yPos = y + ( tall/2 ) - ( nFinalHeight/2 );
		surface()->DrawSetColor( Color( 255, 255, 255, flLerp * 255.0f  ) );
		surface()->DrawSetTexture( m_nThumbnailImageId );
		surface()->DrawTexturedRect( xPos, yPos, xPos + nFinalWidth, yPos + nFinalHeight );

		// Draw a black border around the image
		surface()->DrawSetColor( Color( 0, 0, 0, flLerp * 255.0f ) );
		surface()->DrawOutlinedRect( xPos - 1, yPos -1, xPos + nFinalWidth + 1, yPos + nFinalHeight + 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Setup our type here
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::SetDataSettings( KeyValues *pSettings )
{
	BaseClass::SetDataSettings( pSettings );
	if ( pSettings == NULL )
		return;

	// Determine if we're in the editor or not
	bool bInEditor = pSettings->GetBool( "ineditor", false );
	if ( bInEditor )
	{
		UseInEditorFooter();
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Force us to refresh out main list
//-----------------------------------------------------------------------------
void CPuzzleMakerMyChambers::OnRefreshList( void )
{
	PopulateChamberListFromDisk( true );
}

// Whether or not we should attempt to fix orphaned files on the disk
ConVar cm_fix_orphaned_files( "cm_fix_orphaned_files", false, FCVAR_NONE );

//-----------------------------------------------------------------------------
// Purpose:	Find files on the disk that are also represented in the Workshop but of which no hard connection is present
//	Return: Whether or not this file required some sort of fix-up for its Workshop ID
//-----------------------------------------------------------------------------
bool CPuzzleMakerMyChambers::ResolveOrphanedWorkshopFile( PuzzleFilesInfo_t *pPuzzleInfo )
{
	if ( pPuzzleInfo == NULL )
		return false;

	if ( cm_fix_orphaned_files.GetBool() == false )
		return false;

	// If Steam hasn't reported back to us, we can't do any valid work here
	if ( BASEMODPANEL_SINGLETON.HasReceivedUserPublishedMapsBaseline() == false )
		return false;

	// If we have a file ID, make sure it matches something in our known published maps
	if ( pPuzzleInfo->m_uFileID )
	{
		const PublishedFileInfo_t *pInfo = BASEMODPANEL_SINGLETON.GetUserPublishedMapByFileID( pPuzzleInfo->m_uFileID );
		if ( pInfo == NULL )
		{
			// This map thinks it's a published map from the user, but we have no record from Steam about it
			pPuzzleInfo->m_uFileID = 0;
			return true;
		}
	}
	else
	{
		// In this case, there is no file ID and we need to reconcile this file against what we know from Steam
		for ( unsigned int i = 0; i < BASEMODPANEL_SINGLETON.GetNumUserPublishedMaps(); i++ )
		{
			// Get the file information for this index
			const PublishedFileInfo_t *pInfo = BASEMODPANEL_SINGLETON.GetUserPublishedMap( i );
			if ( pInfo == NULL )
				continue;

			if ( WorkshopManager().UGCFileRequestExists( pInfo->m_hFile ) ) 
			{
				const char *pFilename = WorkshopManager().GetUGCFilename( pInfo->m_hFile );
				const char *pLocalFilename = V_UnqualifiedFileName( pPuzzleInfo->m_strMapFileName );
				if ( pFilename && pLocalFilename && V_stricmp( pLocalFilename, pFilename ) == 0 )
				{
					// A match was found, take it
					pPuzzleInfo->m_uFileID = pInfo->m_nPublishedFileId;
					return true;
				}
			}
		}

	}

	// No change occurred, the file is in a reasonable state
	return false;
}

// ============================================================================
//
//  Custom panel list item
//
// ============================================================================

CChamberListItem::CChamberListItem( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName )
{
	m_pListCtrlr = dynamic_cast<GenericPanelList *>( pParent );
	m_pDialog = dynamic_cast<CPuzzleMakerMyChambers*>( pParent->GetParent() );
	m_pLblChamberStatus = NULL;
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CChamberListItem::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/BaseModUI/puzzlemaker_chamberitem.res" );

	//const char *pDefaultFontString = pScheme->GetResourceString( "HybridButton.Font" );
	//const char *pStyle = "DialogListButton";
	//const char *pFontString = pScheme->GetResourceString( CFmtStr( "%s.Font", pStyle ) );
	//m_hTextFont = pScheme->GetFont( pFontString[0] ? pFontString : pDefaultFontString, true );

	m_TextColor = GetSchemeColor( "HybridButton.TextColorAlt", pScheme );
	m_FocusColor = GetSchemeColor( "HybridButton.FocusColorAlt", pScheme );
	m_CursorColor = GetSchemeColor( "HybridButton.CursorColorAlt", pScheme );
	m_LockedColor = Color( 64, 64, 64, 128 ); //GetSchemeColor( "HybridButton.LockedColor", pScheme );
	m_MouseOverCursorColor = GetSchemeColor( "HybridButton.MouseOverCursorColorAlt", pScheme );
	m_LostFocusColor = m_CursorColor;
	m_LostFocusColor.SetColor( m_LostFocusColor.r(), m_LostFocusColor.g(), m_LostFocusColor.b(), 50); //Color( 120, 120, 120, 255 );
	m_BaseColor = Color( 255, 255, 255, 0 );

	m_pLblName = dynamic_cast< vgui::Label * >( FindChildByName( "LblItemName" ) );
	m_pLblChamberStatus = dynamic_cast< vgui::Label * >( FindChildByName( "LblChamberStatus" ) );
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CChamberListItem::OnKeyCodePressed( vgui::KeyCode code )
{
	int iUserSlot = GetJoystickForCode( code );
	BASEMODPANEL_SINGLETON.SetLastActiveUserId( iUserSlot );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_A:
	case KEY_ENTER:
		{
			m_pDialog->LoadChamber( m_nChamberInfoIndex );
		}
		return;
	}

	BaseClass::OnKeyCodePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CChamberListItem::OnMousePressed( vgui::MouseCode code )
{
	if ( code == MOUSE_LEFT )
	{
		if ( GetParent() )
			GetParent()->NavigateToChild( this );
		else
			NavigateTo();
		return;
	}

	BaseClass::OnMousePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CChamberListItem::OnMouseDoublePressed( vgui::MouseCode code )
{
	if ( code == MOUSE_LEFT )
	{
		CChamberListItem* pListItem = static_cast< CChamberListItem* >( m_pListCtrlr->GetSelectedPanelItem() );
		if ( pListItem )
		{
			OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, BASEMODPANEL_SINGLETON.GetLastActiveUserId() ) );
		}
	}

	BaseClass::OnMouseDoublePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose:	Draw our state (selected, hovered, etc)
//-----------------------------------------------------------------------------
void CChamberListItem::PaintBackground()
{
	bool bHasFocus = HasFocus() || IsSelected();

	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );

	// if we're highlighted, background
	if ( HasMouseover() )
	{
		surface()->DrawSetColor( m_MouseOverCursorColor );
		surface()->DrawFilledRect( 0, 0, wide, tall );
	}
	else if ( bHasFocus )
	{
		surface()->DrawSetColor( m_CursorColor );
		surface()->DrawFilledRect( 0, 0, wide, tall );
	}

	// set the colors for the labels
	if ( m_pLblName && m_pLblChamberStatus )
	{
		if ( HasFocus() || IsSelected() || HasMouseover() )
		{
			m_pLblName->SetFgColor( m_FocusColor );
			m_pLblChamberStatus->SetFgColor( m_FocusColor );
		}
		else
		{
			m_pLblName->SetFgColor( m_TextColor );
			m_pLblChamberStatus->SetFgColor( m_TextColor );
		}
	}

	// DrawListItemLabel( m_pLblChamberStatus );
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CChamberListItem::SetChamberStatus( wchar_t *pChamberStatus )
{
	if ( m_pLblChamberStatus == NULL )
		return;

	m_pLblChamberStatus->SetText( pChamberStatus );
}

void cc_puzzlemaker_open_chamber( const CCommand &args )
{
	// It's a fullscreen hud element, so we don't need to activate it for other slots
	if ( GET_ACTIVE_SPLITSCREEN_SLOT() != 0 )
		return;

	engine->ExecuteClientCmd("gameui_activate");
	BaseModUI::CBaseModFrame *pInGameMenu = BASEMODPANEL_SINGLETON.OpenWindow( BaseModUI::WT_INGAMEMAINMENU, NULL, true );
	GameUI().PreventEngineHideGameUI();


	if ( g_pPuzzleMaker->HasUnsavedChanges() )
	{
		CPuzzleMakerExitConfirmation* pExitConfirm = static_cast<CPuzzleMakerExitConfirmation*>(BASEMODPANEL_SINGLETON.OpenWindow( BaseModUI::WT_PUZZLEMAKEREXITCONRFIRMATION, pInGameMenu, true ));
		pExitConfirm->SetReason( EXIT_REASON_OPEN );
	}
	else
	{
		// Open our chambers dialog, making sure to note that we're inside the editor (handles autosaves differently)
		KeyValues *pSettings = new KeyValues( "settings" );
		KeyValues::AutoDelete autoDelete_settings( pSettings );
		pSettings->SetBool( "ineditor", true );

		BASEMODPANEL_SINGLETON.OpenWindow( BaseModUI::WT_EDITORCHAMBERLIST, pInGameMenu, true, pSettings );
	}
}
static ConCommand puzzlemaker_open_chamber("puzzlemaker_open_chamber", cc_puzzlemaker_open_chamber );

#define EPOCH_1_DAY		(86400)
#define EPOCH_2_DAYS	(EPOCH_1_DAY*2)

//-----------------------------------------------------------------------------
// Purpose:	Determines if the dates are within two calendar days of one another, and if so, returns the number of days (0 or 1)
//-----------------------------------------------------------------------------
inline bool DatesWithinOneCalendarDay( time_t aTime, time_t bTime, int &nNumDays ) 
{
	// Must be within a two 24-hour day period
	nNumDays = -1;
	if ( abs( aTime - bTime ) >= EPOCH_2_DAYS )
		return false;

	// Setup our local time information
	tm nearTimeInfo = *localtime( (aTime>bTime) ? &aTime : &bTime );
	tm farTimeInfo = *localtime( (aTime>bTime) ? &bTime : &aTime );

	// Get the day of the year for both times
	int farYDay = farTimeInfo.tm_yday;
	int nearYDay = nearTimeInfo.tm_yday;

	// See if we're straddling the New Year
	if ( nearTimeInfo.tm_year != farTimeInfo.tm_year )
	{
		// If we are, rebase the near time to be in the year of the far year (accounting for leap years)
		nearYDay += ( ( farTimeInfo.tm_year % 4 == 0 && farTimeInfo.tm_year % 100 != 0) || farTimeInfo.tm_year % 400 == 0) ? (365+1) : (364+1); // NOTE: This is zero-based
	}

	// Setup our return values;
	nNumDays = (nearYDay - farYDay);
	return (nNumDays<=1);
}

//-----------------------------------------------------------------------------
// Purpose:	Output a string constructed from a timestamp
//-----------------------------------------------------------------------------
void FormatFileTimeString( time_t nFileTime, wchar_t *pOutputBuffer, int nBufferSizeInBytes )
{
	static const char *s_weekdays[] = 
	{
		"Sunday",
		"Monday",
		"Tuesday",
		"Wednesday",
		"Thursday",
		"Friday",
		"Saturday",
	};

	static const char *s_months[] = 
	{
		"January",
		"February",
		"March",
		"April",
		"May",
		"June",
		"July",
		"August",
		"September",
		"October",
		"November",
		"December",
	};
	
	int nNumDaysBetweenTimes = 0;

	// Get it for our file time
	tm fileTimeInfo = *localtime( &nFileTime );

	// Format like: Jan 10 11:35 PM
	const wchar_t *pDateTimeFormatString = pDateTimeFormatString = L"$M $D $t";

	const wchar_t *pWeekdayString = g_pVGuiLocalize->Find( CFmtStr( "#PORTAL2_%s_Short", s_weekdays[fileTimeInfo.tm_wday] ) );
	if ( !pWeekdayString )
	{
		pWeekdayString = L"";
	}

	const wchar_t *pMonthString = g_pVGuiLocalize->Find( CFmtStr( "#PORTAL2_%s", s_months[fileTimeInfo.tm_mon] ) );
	if ( !pMonthString )
	{
		pMonthString = L"";
	}

	const wchar_t *pTodayString = g_pVGuiLocalize->Find( "#PORTAL2_Today" );
	if ( !pTodayString )
	{
		pTodayString = L"";
	}

	const wchar_t *pYesterdayString = g_pVGuiLocalize->Find( "#PORTAL2_Yesterday" );
	if ( !pYesterdayString )
	{
		pYesterdayString = L"";
	}

	const wchar_t *pCurrent = pDateTimeFormatString;
	wchar_t *pOutputString = pOutputBuffer;
	int nNumCharsRemaining = nBufferSizeInBytes/2;

	while ( *pCurrent && nNumCharsRemaining > 1 )
	{
		if ( *pCurrent == L'$' )
		{
			switch ( pCurrent[1] )
			{
			case L'S':
				{
					// We must be within two days to be here
					const wchar_t *pDayString = L"";

					// Now determine the time span
					if ( nNumDaysBetweenTimes == 0 )
					{
						pDayString = pTodayString;
					}
					else if ( nNumDaysBetweenTimes == 1 )
					{
						pDayString = pYesterdayString;
					}
					else
					{
						// Too far ago, skip this entry!
						pCurrent += 2;
						continue;
					}

					V_wcsncpy( pOutputString, pDayString, nNumCharsRemaining );
					int len = V_wcslen( pDayString );
					pOutputString += len;
					nNumCharsRemaining -= len;
					pCurrent += 2;
				}
				continue;

			case L'W':
				{
					V_wcsncpy( pOutputString, pWeekdayString, nNumCharsRemaining );
					int len = V_wcslen( pWeekdayString );
					pOutputString += len;
					nNumCharsRemaining -= len;
					pCurrent += 2;
				}
				continue;

			case L'M':
				{
					V_wcsncpy( pOutputString, pMonthString, nNumCharsRemaining );
					int len = V_wcslen( pMonthString );
					pOutputString += len;
					nNumCharsRemaining -= len;
					pCurrent += 2;
				}
				continue;

			case L'D':
				{
					wchar_t dayString[64];
					V_snwprintf( dayString, ARRAYSIZE( dayString ), L"%d", fileTimeInfo.tm_mday );
					V_wcsncpy( pOutputString, dayString, nNumCharsRemaining );
					int len = V_wcslen( dayString );
					pOutputString += len;
					nNumCharsRemaining -= len;
					pCurrent += 2;
				}
				continue;

			case L'T':
				{
					// 24 hour time format
					wchar_t timeString[64];
					V_snwprintf( timeString, ARRAYSIZE( timeString ), L"%2.2d:%2.2d", fileTimeInfo.tm_hour, fileTimeInfo.tm_min );
					V_wcsncpy( pOutputString, timeString, nNumCharsRemaining );
					int len = V_wcslen( timeString );
					pOutputString += len;
					nNumCharsRemaining -= len;
					pCurrent += 2;
				}
				continue;

			case L't':
				{
					// 12 hour time format
					wchar_t timeString[64];
					const wchar_t *pAMPMString = L"AM";
					int nHour = fileTimeInfo.tm_hour;
					if ( nHour == 0 )
					{
						nHour = 12;
					}
					else if ( nHour == 12 )
					{
						pAMPMString = L"PM";
					}
					else if ( nHour >= 13 )
					{
						nHour -= 12;
						pAMPMString = L"PM";
					}
					V_snwprintf( timeString, ARRAYSIZE( timeString ), L"%d:%2.2d " PRI_WS_FOR_WS, nHour, fileTimeInfo.tm_min, pAMPMString );
					V_wcsncpy( pOutputString, timeString, nNumCharsRemaining );
					int len = V_wcslen( timeString );
					pOutputString += len;
					nNumCharsRemaining -= len;
					pCurrent += 2;
				}
				continue;
			}
		}

		*pOutputString = *pCurrent;
		pOutputString++;
		pCurrent++;
		nNumCharsRemaining--;
	}
}

#endif // PORTAL2_PUZZLEMAKER
