//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//
#if defined( PORTAL2_PUZZLEMAKER )

#include "vpuzzlemakerpublishprogress.h"
#include "vgui_controls/ImagePanel.h"
#include "vfooterpanel.h"
#include "vgenericconfirmation.h"
#include <time.h>
#include "vgui/ilocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;


CPuzzleMakerPublishProgress::CPuzzleMakerPublishProgress( Panel *pParent, const char *pPanelName )
						   : BaseClass( pParent, pPanelName ),
							 m_pSpinner( NULL )
{
	GameUI().PreventEngineHideGameUI();

	SetDeleteSelfOnClose( true );
	SetProportional( true );

	//SetDialogTitle( "Publish to workshop" );
	
	SetFooterEnabled( true );
	UpdateFooter();
}


void CPuzzleMakerPublishProgress::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pSpinner = static_cast<ImagePanel*>( FindChildByName( "PublishSpinner" ) );

	UpdateFooter();
}


void CPuzzleMakerPublishProgress::OnThink()
{
	BaseClass::OnThink();

	//Update spinner
	if ( m_pSpinner )
	{
		int nAnimFrame = ( uint64 )( Plat_FloatTime() * 10 );
		m_pSpinner->SetFrame( nAnimFrame );
	}
}


void CPuzzleMakerPublishProgress::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		pFooter->SetButtons( FB_NONE );
	}
}


void CPuzzleMakerPublishProgress::OnKeyCodePressed( vgui::KeyCode code )
{
	int iUserSlot = GetJoystickForCode( code );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( iUserSlot );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		// Suppress this command from chaining to the base
		return;
	}

	BaseClass::OnKeyCodePressed( code );
}


void CPuzzleMakerPublishProgress::CancelButtonPressed()
{
	CancelPublish();
	CloseDialog();
}

void CPuzzleMakerPublishProgress::CancelPublish()
{
	//Cancel publishing here
}


void CPuzzleMakerPublishProgress::CloseDialog()
{
	CBaseModPanel::GetSingleton().PlayUISound( UISOUND_BACK );
	
	GameUI().AllowEngineHideGameUI();
	engine->ExecuteClientCmd("gameui_hide");
	CBaseModPanel::GetSingleton().CloseAllWindows();
}


void CPuzzleMakerPublishProgress::BeginPublish( ERemoteStoragePublishedFileVisibility eVisiblity )
{
	m_eVisiblity = eVisiblity;
	// TODO: Make this be an interactive step the user takes where they pose their chamber
	g_pPuzzleMaker->TakeScreenshotAsync( PuzzleMakerPublishScreenshotCallback, false );
}


void BaseModUI::PuzzleMakerPublishScreenshotCallback( const char *pszScreenshotName )
{
	CPuzzleMakerPublishProgress *pSelf = static_cast<CPuzzleMakerPublishProgress*>( CBaseModPanel::GetSingleton().GetWindow( WT_PUZZLEMAKERPUBLISHPROGRESS ) );
	if ( pSelf )
	{
		pSelf->PublishFile();
	}
}

void CPuzzleMakerPublishProgress::PublishingError( const wchar_t *wszError, bool bCloseAll /*= true*/ )
{
	GenericConfirmation* pConfirmation =
		static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

	GenericConfirmation::Data_t data;
	data.pWindowTitle = "#PORTAL2_PuzzleMaker_PublishingError";
	data.pMessageTextW = wszError;

	data.pCancelButtonText = "#L4D360UI_Back";
	data.bCancelButtonEnabled = true;
	if ( bCloseAll )
	{
		data.pfnCancelCallback = &ConfirmPublishFailure_Callback;
	}
	else
	{
		data.pfnCancelCallback = &ConfirmPublishFailureGoBack_Callback;
	}

	pConfirmation->SetUsageData( data );
}


void CPuzzleMakerPublishProgress::ConfirmPublishFailure_Callback()
{
	GameUI().AllowEngineHideGameUI();
	engine->ExecuteClientCmd("gameui_hide");
	CBaseModPanel::GetSingleton().CloseAllWindows();
}


void CPuzzleMakerPublishProgress::ConfirmPublishFailureGoBack_Callback()
{
	BaseModUI::CPuzzleMakerPublishProgress* pConfirmation =
		static_cast<BaseModUI::CPuzzleMakerPublishProgress*>( BaseModUI::CBaseModPanel::GetSingleton().GetWindow( BaseModUI::WT_PUZZLEMAKERPUBLISHPROGRESS ) );

	if ( pConfirmation )
	{
		if ( !pConfirmation->NavigateBack() )
		{
			pConfirmation->Close();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Share the file with Steam Cloud and return the handle for later usage
//-----------------------------------------------------------------------------
bool CPuzzleMakerPublishProgress::PublishFile( )
{
	const PuzzleFilesInfo_t& puzzleInfo = g_pPuzzleMaker->GetPuzzleInfo();

	if ( PrepFile( puzzleInfo.m_strMapFileName ) == false )
		return false;

	// Write that preview to Steam
	if ( PrepFile( puzzleInfo.m_strScreenshotFileName ) == false )
		return false;

	// Convert the file into the name it'll appear as in the cloud
	char szFinalBSPFilename[MAX_PATH];
	GetCloudFilename( puzzleInfo.m_strMapFileName, szFinalBSPFilename, sizeof(szFinalBSPFilename) );

	char szFinalPreviewFilename[MAX_PATH];
	GetCloudFilename( puzzleInfo.m_strScreenshotFileName, szFinalPreviewFilename, sizeof(szFinalPreviewFilename) );

	CUtlVector< const char * >	vecTags;	// Tags for this BSP file
	g_pPuzzleMaker->GetTagsForCurrentPuzzle( vecTags );

	Msg( "[PORATL2 PUZZLEMAKER]\tTags for puzzle\n");
	for( int i=0; i<vecTags.Count(); ++i )
	{
		Msg( "                     \t%s\n", vecTags[i] );
	}	

	SteamParamStringArray_t strArray;
	strArray.m_ppStrings = vecTags.Base();
	strArray.m_nNumStrings = vecTags.Count();

	const PublishedFileInfo_t* publishedFileInfo = BASEMODPANEL_SINGLETON.GetUserPublishedMapByFileID( puzzleInfo.m_uFileID );

	// TODO: At this point we need to ask Steam if this file is real. If not, we need to publish it

	Msg( "[PORATL2 PUZZLEMAKER]\tPublishing: Querying Steam with FileID %d\n", puzzleInfo.m_uFileID );

	ISteamRemoteStorage *pRemoteStorage = GetISteamRemoteStorage();
	if( !pRemoteStorage )
	{
		PublishingError( g_pVGuiLocalize->Find( "#PORTAL2_PuzzleMaker_PublishError_NoSteam" ) );
	}


	if( publishedFileInfo )
	{
		Msg( "[PORATL2 PUZZLEMAKER]\tPublishing: Published file found.  Updating...\n" );

		PublishedFileUpdateHandle_t updateHandle = pRemoteStorage->CreatePublishedFileUpdateRequest( puzzleInfo.m_uFileID );

		// Update title
		pRemoteStorage->UpdatePublishedFileTitle( updateHandle, puzzleInfo.m_strPuzzleTitle );
		// Update description
		pRemoteStorage->UpdatePublishedFileDescription( updateHandle, puzzleInfo.m_strDescription );
		// Update the preview image
		pRemoteStorage->UpdatePublishedFilePreviewFile( updateHandle, szFinalPreviewFilename );
		// Update bsp
		pRemoteStorage->UpdatePublishedFileFile( updateHandle, szFinalBSPFilename );

		// Since the file changed, update the tags as well
		// This needs to ultimately account for "objective" tags and "subjective" tags and not nuke 
		// the latter when changing the former
		pRemoteStorage->UpdatePublishedFileTags( updateHandle, &strArray );
		
		// Update visiblity
		pRemoteStorage->UpdatePublishedFileVisibility( updateHandle, m_eVisiblity );

		// Call for update
		SteamAPICall_t hSteamAPICall = pRemoteStorage->CommitPublishedFileUpdate( updateHandle );
		if ( hSteamAPICall == k_uAPICallInvalid )
		{
			// FIXME: We could be more descriptive here...
			PublishingError( g_pVGuiLocalize->Find( "#PORTAL2_PuzzleMaker_PublishError_SteamError" ) );
			return false;
		}
		else
		{
			m_callbackUpdateFile.Set( hSteamAPICall, this, &CPuzzleMakerPublishProgress::Steam_OnUpdateFile );	
			return true;
		}
	}
	else
	{
		Msg( "[PORATL2 PUZZLEMAKER]\tPublishing: Published file not found.  Publishing...\n" );
		Msg( "\t\tBSP: %s\n", szFinalBSPFilename );
		Msg( "\t\tThumbnail: %s\n", szFinalPreviewFilename );
		Msg( "\t\tAppID: %d\n", steamapicontext->SteamUtils()->GetAppID() );
		Msg( "\t\tTitle: %s\n", puzzleInfo.m_strPuzzleTitle.Get() );
		Msg( "\t\tDescription: %s\n", puzzleInfo.m_strDescription.Get() );

		// Publish the file
		SteamAPICall_t hSteamAPICall = k_uAPICallInvalid;
		if ( pRemoteStorage )
		{
			hSteamAPICall = pRemoteStorage->PublishWorkshopFile( 
							szFinalBSPFilename, 
							szFinalPreviewFilename, 
							steamapicontext->SteamUtils()->GetAppID(),
							puzzleInfo.m_strPuzzleTitle,
							puzzleInfo.m_strDescription,
							m_eVisiblity,
							&strArray,
							k_EWorkshopFileTypeCommunity );
		}

		// Validate the call
		if ( hSteamAPICall != k_uAPICallInvalid )
		{
			// Set the callback
			m_callbackPublishFile.Set( hSteamAPICall, this, &CPuzzleMakerPublishProgress::Steam_OnPublishFile );	

			// We're done!
			return true;
		}
	}

	// Something didn't work out right
	PublishingError( g_pVGuiLocalize->Find( "#PORTAL2_PuzzleMaker_PublishError_SteamError" ) );
	return false;
}



//-----------------------------------------------------------------------------
// Purpose: Write the files to the cloud in preparation of publishing them out
//-----------------------------------------------------------------------------
bool CPuzzleMakerPublishProgress::PrepFile( const char *lpszFilename )
{
	Msg( "[PORATL2 PUZZLEMAKER]\tWriting file to cloud: %s\n", lpszFilename );

	// Now, publish
	int32 totalBytes, availableBytes;
	ISteamRemoteStorage *pRemoteStorage = GetISteamRemoteStorage();
	if ( !pRemoteStorage || !pRemoteStorage->GetQuota( &totalBytes, &availableBytes ) )
	{
		wchar_t wszFileName[512];
		g_pVGuiLocalize->ConvertANSIToUnicode( lpszFilename, wszFileName, ARRAYSIZE( wszFileName ) );

		wchar_t wszErrorString[512];
		g_pVGuiLocalize->ConstructString( wszErrorString, ARRAYSIZE( wszErrorString ), g_pVGuiLocalize->Find( "#PORTAL2_PuzzleMaker_PublishError_NoSteamCloud" ), 1, wszFileName );

		PublishingError( wszErrorString );

		return false;
	}

#if VERBOSE_OUTPUT
	Msg("UGC: %dk of %dk remaining in Steam Cloud\n", (availableBytes/1024), (totalBytes/1024) );
#endif // VERBOSE_OUTPUT

	// Find the result on disk, if it made it
	if ( g_pFullFileSystem->FileExists( lpszFilename ) )
	{
		// Verify we're not going to exceed the size quota in the cloud
		if ( g_pFullFileSystem->Size( lpszFilename ) >= (uint32) availableBytes )
		{
			wchar_t wszFileName[512];
			g_pVGuiLocalize->ConvertANSIToUnicode( lpszFilename, wszFileName, ARRAYSIZE( wszFileName ) );

			wchar_t wszCloudBytes[128];
			V_snwprintf( wszCloudBytes, ARRAYSIZE( wszCloudBytes ), L"%d", availableBytes );

			wchar_t wszErrorString[512];
			g_pVGuiLocalize->ConstructString( wszErrorString, ARRAYSIZE( wszErrorString ), g_pVGuiLocalize->Find( "#PORTAL2_PuzzleMaker_PublishError_ExhaustCloudSpace" ), 2, wszFileName, wszCloudBytes );

			PublishingError( wszErrorString );

			return false;
		}

		// Read the actual file into memory for the write
		// FIXME: This could be async, but for this tool it's easier to just make the user wait
		CUtlBuffer fileBuffer;
		g_pFullFileSystem->ReadFile( lpszFilename, NULL, fileBuffer );

		// Convert the file into the name it'll appear as in the cloud
		char szFinalFilename[MAX_PATH];
		GetCloudFilename( lpszFilename, szFinalFilename, sizeof(szFinalFilename) );

		if ( pRemoteStorage->FileWrite( szFinalFilename, fileBuffer.Base(), fileBuffer.TellPut() ) == false )
		{
			wchar_t wszFileName[512];
			g_pVGuiLocalize->ConvertANSIToUnicode( szFinalFilename, wszFileName, ARRAYSIZE( wszFileName ) );

			wchar_t wszErrorString[512];
			g_pVGuiLocalize->ConstructString( wszErrorString, ARRAYSIZE( wszErrorString ), g_pVGuiLocalize->Find( "#PORTAL2_PuzzleMaker_PublishError_NoCloudSpace" ), 1, wszFileName );

			PublishingError( wszErrorString );

			return false;
		}

		// Done with the file data
		fileBuffer.Purge();
		return true;
	}

	wchar_t wszFileName[512];
	g_pVGuiLocalize->ConvertANSIToUnicode( lpszFilename, wszFileName, ARRAYSIZE( wszFileName ) );

	wchar_t wszErrorString[512];
	g_pVGuiLocalize->ConstructString( wszErrorString, ARRAYSIZE( wszErrorString ), g_pVGuiLocalize->Find( "#PORTAL2_PuzzleMaker_PublishError_FileNotOnDisk" ), 1, wszFileName );

	PublishingError( wszErrorString );
	return false;
}


#define CUSTOM_MAP_SUBDIR	"mymaps"

//-----------------------------------------------------------------------------
// Purpose: Prep a filename for how it'll show up in the cloud
//-----------------------------------------------------------------------------
void CPuzzleMakerPublishProgress::GetCloudFilename( const char *lpszIn, char *lpszOut, int nOutSize )
{
	const char *lpszBaseFilename = V_GetFileName( lpszIn );	
	V_ComposeFileName( CUSTOM_MAP_SUBDIR, lpszBaseFilename, lpszOut, nOutSize );
}


//-----------------------------------------------------------------------------
// Purpose: Either show the overlay with the map, or a dialog telling the
// user that their publish was successful
//-----------------------------------------------------------------------------
void CPuzzleMakerPublishProgress::SuccessDialog( const PublishedFileId_t& id )
{
	// Show the map in our overlay
	OverlayResult_t result = BASEMODPANEL_SINGLETON.ViewCommunityMapInWorkshop( id );

	if( result != RESULT_OK )
	{
		GenericConfirmation* pConfirmation =
		static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

		GenericConfirmation::Data_t data;
		data.pWindowTitle = "#PORTAL2_PuzzleMaker_PublishingSuccess";
		data.pMessageText = "#PORTAL2_PuzzleMaker_PublishingSuccessMessage";

		data.pCancelButtonText = "#L4D360UI_Ok";
		data.bCancelButtonEnabled = true;
		data.pfnCancelCallback = &ConfirmPublishFailure_Callback;

		pConfirmation->SetUsageData( data );
	}
	else
	{
		CloseDialog();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Callback when our update call has completed
//-----------------------------------------------------------------------------
void CPuzzleMakerPublishProgress::Steam_OnUpdateFile( RemoteStorageUpdatePublishedFileResult_t *pResult, bool bError )
{
	if ( CheckPublishingError( pResult->m_eResult, bError ) )
	{
		return;
	}

	if ( steamapicontext && steamapicontext->SteamUser() && steamapicontext->SteamFriends() )
	{
		SuccessDialog( pResult->m_nPublishedFileId );

		// Update puzzle file with GC file handle
		PuzzleFilesInfo_t puzzleInfo = g_pPuzzleMaker->GetPuzzleInfo();
		puzzleInfo.m_uFileID = pResult->m_nPublishedFileId;
		g_pPuzzleMaker->SetPuzzleInfo( puzzleInfo );
		g_pPuzzleMaker->SavePuzzle( false );	// Save, so the file handle gets written to disk

		Msg( "[PORATL2 PUZZLEMAKER]\tPublishing: Update succeeded" );

		// Add file ID to published maps
		BASEMODPANEL_SINGLETON.AddUserPublishedMap( puzzleInfo.m_uFileID );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Callback when our publish call has completed
//-----------------------------------------------------------------------------
void CPuzzleMakerPublishProgress::Steam_OnPublishFile( RemoteStoragePublishFileResult_t *pResult, bool bError )
{
	if ( CheckPublishingError( pResult->m_eResult, bError ) )
	{
		return;
	}

	if ( steamapicontext && steamapicontext->SteamUser() && steamapicontext->SteamFriends() )
	{
		SuccessDialog( pResult->m_nPublishedFileId );

		// Update puzzle file with GC file handle
		PuzzleFilesInfo_t puzzleInfo = g_pPuzzleMaker->GetPuzzleInfo();
		puzzleInfo.m_uFileID = pResult->m_nPublishedFileId;
		
		time_t curTime = time( NULL );
		puzzleInfo.m_uTimeStamp_Modified = curTime;
		g_pPuzzleMaker->SetPuzzleInfo( puzzleInfo );
		g_pPuzzleMaker->SavePuzzle( false );	// Save, so the file handle gets written to disk

		Msg( "[PORATL2 PUZZLEMAKER]\tPublishing: Publish succeeded.  New FileID %d", puzzleInfo.m_uFileID );

		// Add file ID to published maps
		BASEMODPANEL_SINGLETON.AddUserPublishedMap( puzzleInfo.m_uFileID );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Checks if there is a publishing error and generates the localized error string
//-----------------------------------------------------------------------------
bool CPuzzleMakerPublishProgress::CheckPublishingError( EResult eResult, bool bError )
{
	if ( bError || eResult != k_EResultOK )
	{
		switch ( eResult )
		{
		case k_EResultDuplicateName:
			{
				PublishingError( g_pVGuiLocalize->Find( "#PORTAL2_PuzzleMaker_PublishError_DuplicateName" ), false );
			}
			break;
		case k_EResultBusy:
			{
				PublishingError( g_pVGuiLocalize->Find( "#PORTAL2_PuzzleMaker_PublishError_TooBusy" ), false );
			}
			break;
		case k_EResultTimeout:
		case k_EResultConnectFailed:
		case k_EResultNotLoggedOn:
			{
				PublishingError( g_pVGuiLocalize->Find( "#PORTAL2_PuzzleMaker_PublishError_ConnectionError" ), false );
			}
			break;
		case k_EResultFileNotFound:
			{
				PublishingError( g_pVGuiLocalize->Find( "#PORTAL2_PuzzleMaker_PublishError_FileNotFound" ), false );
			}
			break;
		default:
			{
				wchar_t wszErrorCode[128];
				V_snwprintf( wszErrorCode, ARRAYSIZE( wszErrorCode ), L"%d", eResult );

				wchar_t wszErrorString[MAX_PATH];
				g_pVGuiLocalize->ConstructString( wszErrorString, ARRAYSIZE( wszErrorString ), g_pVGuiLocalize->Find( "#PORTAL2_PuzzleMaker_PublishError_ErrorNumber" ), 1, wszErrorCode );

				PublishingError( wszErrorString );
			}
			break;
		}

		return true;
	}

	return false;
}

#endif //PORTAL2_PUZZLEMAKER

