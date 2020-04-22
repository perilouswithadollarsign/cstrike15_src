//========= Copyright � Valve Corporation, All rights reserved. ============//
//
//
//==========================================================================//

#include "cbase.h"
#include <time.h>
#include "VFooterPanel.h"
#include "VGenericPanelList.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui/ilocalize.h"
#include "FileSystem.h"
#include "VGenericConfirmation.h"
#include "bitmap/tgaloader.h"
#include "steamcloudsync.h"
#ifdef _PS3
#include "sysutil/sysutil_savedata.h"
#endif
#include "vgui_controls/scrollbar.h"
#include <vgui_controls/ImageList.h>
#include "vgui_avatarimage.h"

#include "gc_clientsystem.h"
#include "econ_gcmessages.h"
#include "vratemapdialog.h"
#include "imageutils.h"
#include "rtime.h"

#ifdef PORTAL2_PUZZLEMAKER
#include "c_community_coop.h"
#endif // PORTAL2_PUZZLEMAKER

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( PORTAL2_PUZZLEMAKER )

using namespace vgui;
using namespace BaseModUI;

class CRateMapDialog_WaitForDownloadOperation : public IMatchAsyncOperation
{
public:
	virtual bool IsFinished() { return false; }
	virtual AsyncOperationState_t GetState() { return AOS_RUNNING; }
	virtual uint64 GetResult() { return 0ull; }
	virtual void Abort();
	virtual void Release() { Assert( 0 ); }

public:
	CRateMapDialog_WaitForDownloadOperation() {}
	IMatchAsyncOperation * Prepare();
}
g_RateMapDialog_WaitForDownloadOperation;

extern ConVar cm_community_debug_spew;
extern Color rgbaCommunityDebug;

IMatchAsyncOperation *CRateMapDialog_WaitForDownloadOperation::Prepare()
{
	return this;
}

void CRateMapDialog_WaitForDownloadOperation::Abort()
{
	RateMapDialog *pRateMapDialog = (RateMapDialog *) CBaseModPanel::GetSingleton().GetWindow( WT_RATEMAP );
	if ( !pRateMapDialog )
		return;

	pRateMapDialog->PostMessage( pRateMapDialog, new KeyValues( "MapDownloadAborted", "msg", "" ) );
}

void RateMapDialog::MapDownloadAborted( const char *msg )
{
	// Stop waiting for the download and clear our waitscreen
	m_bWaitingForMapDownload = false;
	m_unNextMapInQueueID = 0;

	// Stop our wait screen
	CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
}

//=======================================================================
// 
// Vote up/down image buttons
//
//=======================================================================

class CVoteImagePanel : public vgui::ImagePanel
{
public:
	DECLARE_CLASS_SIMPLE( CVoteImagePanel, vgui::ImagePanel );

	CVoteImagePanel( vgui::Panel *parent, const char *name, bool bThumbsUp ) : vgui::ImagePanel( parent, name )
	{
		m_nState = 0;
		m_bThumbsUp = bThumbsUp;

		const char *lpszDefaultImage = ( m_bThumbsUp ) ? "thumbs_up_default" : "thumbs_down_default";
		SetImage( lpszDefaultImage );
	}

	void SetState( int nState ) 
	{ 
		m_nState = nState; 
		const char *lpszImage;
		
		switch( nState )
		{
		default:
		case 0:
			lpszImage = ( m_bThumbsUp ) ? "thumbs_up_default" : "thumbs_down_default";
			break;
		case 1:
			lpszImage = ( m_bThumbsUp ) ? "thumbs_up_vote" : "thumbs_down_vote";
			break;
		case 2:
			lpszImage = ( m_bThumbsUp ) ? "thumbs_up_focus" : "thumbs_down_focus";
			break;
		}
		
		SetImage( lpszImage );
	}

	void OnMousePressed( vgui::MouseCode code )
	{
		GetParent()->OnMousePressed( code );
	}


private:
	int		m_nState;
	bool	m_bThumbsUp;
};

static RateMapDialog * GetMyRateMapDialog()
{
	return static_cast< RateMapDialog * >( BASEMODPANEL_SINGLETON.GetWindow( WT_RATEMAP ) );
}

void BroadcastFollowState( bool bState )
{
	KeyValues *pKV = new KeyValues( "OnFollowStateReceived" );
	pKV->SetBool( "FollowState", bState );
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( pKV );
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
RateMapDialog::RateMapDialog( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName, false, true )
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );

	// Subscribe to event notifications
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	// allows us to get RunFrame() during wait screen occlusion
	AddFrameListener( this );


	m_pRateOptionsList = NULL;
	m_pThumbnailImage = NULL;
	m_pThumbnailSpinner = NULL;

	m_pVoteUpSpinner = NULL;
	m_pVoteDownSpinner = NULL;

	m_pMapTitleLabel = NULL;
	m_pMapAuthorLabel = NULL;
	m_pMapDetailsLabel = NULL;
	m_pAuthorAvatarImage = NULL;
	m_pFollowAuthorButton = NULL;
	m_pTotalVotesLabel = NULL;
	m_pRatingsItem = NULL;

	m_pVoteUpImage = NULL;
	m_pVoteDownImage = NULL;

	const char *pDialogTitle = "#PORTAL2_RateTestChamber";
	SetDialogTitle( pDialogTitle );

	SetFooterEnabled( true );
	
	m_bWaitingForMapDownload = false;
	m_unMapID = 0;
	m_nThumbnailImageId = -1;
	m_hAsyncControl = NULL;
	m_unNextMapInQueueID = 0;
	m_hThumbFileHandle = k_UGCHandleInvalid;
	m_bEndOfLevelVersion = false;
	m_flVoteCommitTime = 0;
	
	m_nVoteState = m_nInitialVoteState = 0;
	m_bInitialFollowState = m_bFollowing = false;
	m_flTransitionStartTime = 0;

	m_pVoteUpImage = new CVoteImagePanel( this, "BtnVoteUp", true );
	m_pVoteDownImage = new CVoteImagePanel( this, "BtnVoteDown", false );	

	GameUI().PreventEngineHideGameUI();
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
RateMapDialog::~RateMapDialog()
{
	// Unsubscribe from event notifications
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );

	RemoveFrameListener( this );

	if ( m_pAuthorAvatarImage )
	{
		uint64 nSteamID = m_AuthorSteamID.ConvertToUint64();
		BaseModUI::CUIGameData::Get()->AccessAvatarImage( nSteamID, BaseModUI::CUIGameData::kAvatarImageRelease );
		m_pAuthorAvatarImage = NULL;
	}

	if ( m_hAsyncControl )
	{
		g_pFullFileSystem->AsyncFinish( m_hAsyncControl );
		g_pFullFileSystem->AsyncRelease( m_hAsyncControl );
		m_hAsyncControl = NULL;
	}

	if ( surface() && m_nThumbnailImageId != -1 )
	{
		// evict prior screenshot
		surface()->DestroyTextureID( m_nThumbnailImageId );
		m_nThumbnailImageId = -1;
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Send all of our voting / follow updates as we exit
//-----------------------------------------------------------------------------
void RateMapDialog::OnClose( void )
{
	BaseClass::OnClose();

	// Tell our parent to nuke the game ui
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnCloseMapRating" ) );
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void RateMapDialog::OnEvent( KeyValues *pEvent )
{
	char const *pEventName = pEvent->GetName();

	// See if the community map list has changed underneath us
	if ( !V_stricmp( "OnFollowStateReceived", pEventName ) )
	{
		if ( m_pFollowAuthorButton )
		{
			m_pFollowAuthorButton->SetEnabled( true );
			if ( pEvent->GetBool( "FollowState" ) )
			{
				UpdateFollowStatus( true );
				m_bInitialFollowState = true;
			}
			else
			{
				UpdateFollowStatus( false );
				m_bInitialFollowState = false; 
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Construct the day's label
//-----------------------------------------------------------------------------
void RateMapDialog::Steam_OnGetUserPublishedItemVoteDetails( RemoteStorageUserVoteDetails_t *pResult, bool bError )
{
	if ( bError || pResult->m_eResult != k_EResultOK )
	{
		Assert(0);
		if( cm_community_debug_spew.GetBool() ) ConColorMsg( rgbaCommunityDebug,  "[RateMapDialog] Failed to get user voting data for %llu\n (Error: %d)", pResult->m_nPublishedFileId, pResult->m_eResult ); 
		Log_Warning( LOG_WORKSHOP, "[RateMapDialog] Failed to get user voting data for %llu\n (Error: %d)", pResult->m_nPublishedFileId, pResult->m_eResult );
		return;
	}
	
	// Translate the workshop enum into our local understanding of the vote state
	if ( pResult->m_eVote == k_EWorkshopVoteUnvoted )
	{
		// No vote
		m_nVoteState = 0;
	}
	else if ( pResult->m_eVote == k_EWorkshopVoteFor )
	{
		// For
		m_nVoteState = 1;
	}
	else
	{
		// Against
		m_nVoteState = -1;
	}

	UpdateVoteButtons();
}

//-----------------------------------------------------------------------------
// Construct the day's label
//-----------------------------------------------------------------------------
void RateMapDialog::UpdateMapFields( void )
{
	// Pull out our current map
	PublishedFileId_t nMapID = BASEMODPANEL_SINGLETON.GetCurrentCommunityMapID();
	if( cm_community_debug_spew.GetBool() ) ConColorMsg( rgbaCommunityDebug,  "[RateMapDialog] UpdateMapFields: mapID %llu\n", nMapID ); 
	if ( nMapID == 0 )
	{
		// No map definition, this is invalid!
		m_pVoteDownImage->SetEnabled( false );
		m_pVoteUpImage->SetEnabled( false );
	}
	else 
	{
		// Get the map's information
		const PublishedFileInfo_t *pFileInfo = WorkshopManager().GetPublishedFileInfoByID( nMapID );
		if ( pFileInfo == NULL )
		{
			if( cm_community_debug_spew.GetBool() ) ConColorMsg( rgbaCommunityDebug,  "[RateMapDialog] UpdateMapFields: No map definition!\n" ); 
			// No map definition, this is invalid!
			Assert( pFileInfo != NULL );
			m_pVoteDownImage->SetEnabled( false );
			m_pVoteUpImage->SetEnabled( false );
			UpdateFooter();
			return;
		}

		if( cm_community_debug_spew.GetBool() ) ConColorMsg( rgbaCommunityDebug,  "[RateMapDialog] UpdateMapFields: Map definition OK!\n" ); 

		m_unMapID = pFileInfo->m_nPublishedFileId;

		char szMapName[128];
		V_strncpy( szMapName, pFileInfo->m_rgchTitle, sizeof(szMapName) );

		if ( m_pMapTitleLabel )
		{
			m_pMapTitleLabel->SetText( szMapName );
		}

		// Get the author's persona name
		CSteamID steamID( pFileInfo->m_ulSteamIDOwner );
		m_AuthorSteamID = steamID;

		// Label the author
		if ( m_pMapAuthorLabel )
		{
			wchar_t finalString[MAX_PATH] = L"";
			wchar_t convertedString[MAX_PATH] = L"";

			const wchar_t * authorFormat = g_pVGuiLocalize->Find( "#PORTAL2_CommunityPuzzle_Author" );
			g_pVGuiLocalize->ConvertANSIToUnicode( steamapicontext->SteamFriends()->GetFriendPersonaName( m_AuthorSteamID ), convertedString, sizeof( convertedString ) );
			if ( authorFormat )
			{
				g_pVGuiLocalize->ConstructString( finalString, sizeof( finalString ), authorFormat, 1, convertedString );
			}

			// Set the final text
			m_pMapAuthorLabel->SetText( finalString );
		}

		// Check the vote state
		m_nVoteState = 0; // FIXME: We need to be able to receive this state from the APIs

		if ( m_pVoteUpImage )
		{
			m_pVoteUpImage->SetEnabled( true );
		}

		if ( m_pVoteDownImage )
		{
			m_pVoteDownImage->SetEnabled( true );
		}

		if ( m_pVoteUpSpinner )
		{
			m_pVoteUpSpinner->SetVisible( false );
		}

		if ( m_pVoteDownSpinner )
		{
			m_pVoteDownSpinner->SetVisible( false );
		}

		// Get our avatar from Steam
		uint64 nSteamID = m_AuthorSteamID.ConvertToUint64();
		if ( m_pAuthorAvatarImage )
		{
			vgui::IImage *pImage = BaseModUI::CUIGameData::Get()->AccessAvatarImage( nSteamID, BaseModUI::CUIGameData::kAvatarImageRequest );
			m_pAuthorAvatarImage->SetImage( pImage );
			m_pAuthorAvatarImage->SetVisible( true );

		}

		// Otherwise, kick off our download
		m_hThumbFileHandle = pFileInfo->m_hPreviewFile;
		
		// This function will automatically return if we've already asked for this file
		if ( BASEMODPANEL_SINGLETON.CreateThumbnailFileRequest( (*pFileInfo) ) == false )
		{
			Assert( 0 );
			m_hThumbFileHandle = k_UGCHandleInvalid;
		}

		// Update vote totals and current rating
		if ( m_pRatingsItem )
		{
			uint32 upVotes = 0, downVotes = 0;
			float flRating = 0.0f;
			if ( pFileInfo->GetVoteData( &flRating, &upVotes, &downVotes ) )
			{
				int nTotalVotes = ( upVotes + downVotes );

				if ( nTotalVotes < MINIMUM_VOTE_THRESHOLD )
				{
					m_pRatingsItem->SetEnabled( false );
					m_pRatingsItem->SetRating( 0.0f );
				}
				else
				{
					m_pRatingsItem->SetEnabled( true );
					m_pRatingsItem->SetRating( flRating );
				}

				m_pRatingsItem->SetVisible( true );

				// Setup our total votes label's text
				wchar_t wszNumVotes[128];
				V_snwprintf( wszNumVotes, ARRAYSIZE(wszNumVotes), L"%d", nTotalVotes );

				wchar_t wszTotalVotes[128];
				g_pVGuiLocalize->ConstructString( wszTotalVotes, ARRAYSIZE(wszTotalVotes), g_pVGuiLocalize->Find( "#Portal2UI_NumRatings" ), 1, wszNumVotes );

				m_pTotalVotesLabel->SetText( wszTotalVotes );
				m_pTotalVotesLabel->SetVisible( true );
			}
			else
			{
				m_pRatingsItem->SetEnabled( false );
				m_pTotalVotesLabel->SetEnabled( false );
			}			
		}

		// FIXME: Handle users looking at their own content		

#if 0 // Disable this GC call for now
		// Query the GC for more information about this author
		GCSDK::CGCMsg<MsgGCCommunityMapIsFollowingAuthor_t> msgFollowing( k_EMsgGCCommunityMapIsFollowingAuthor );
		msgFollowing.Body().m_ulAuthorID = m_AuthorSteamID.GetAccountID();
		GCClientSystem()->BSendMessage( msgFollowing );
#endif // 0 

		// Query the GC for more information how this user voted
#if !defined( _GAMECONSOLE )

		SteamAPICall_t hSteamAPICall = steamapicontext->SteamRemoteStorage()->GetUserPublishedItemVoteDetails( pFileInfo->m_nPublishedFileId );
		m_callbackGetUserPublishedItemVoteDetails.Set( hSteamAPICall, this, &RateMapDialog::Steam_OnGetUserPublishedItemVoteDetails );

#endif // !_GAMECONSOLE
	}

	// In case our state has changed due to no "next" map
	UpdateFooter();
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void RateMapDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pThumbnailImage =		dynamic_cast< ImagePanel* >( FindChildByName( "MapImage" ) );
	m_pThumbnailSpinner =	dynamic_cast< vgui::ImagePanel * >( FindChildByName( "ThumbnailSpinner" ) );
	m_pAuthorAvatarImage =	dynamic_cast< vgui::ImagePanel * >( FindChildByName( "AuthorAvatarImage" ) );
	m_pMapTitleLabel =		dynamic_cast< vgui::Label * >( FindChildByName( "MapTitleLabel" ) );
	m_pMapAuthorLabel =		dynamic_cast< vgui::Label * >( FindChildByName( "MapAuthorLabel" ) );
	m_pMapDetailsLabel =	dynamic_cast< vgui::Label * >( FindChildByName( "MapDetailsLabel" ) );
	m_pFollowAuthorButton = dynamic_cast< CDialogListButton * >( FindChildByName( "BtnFollowAuthor" ) );
	m_pRatingsItem =		dynamic_cast< IconRatingItem * >( FindChildByName( "RatingsItem" ) );
	m_pTotalVotesLabel =	dynamic_cast< vgui::Label * >( FindChildByName( "TotalVotesLabel" ) );
	m_pVoteUpSpinner =		dynamic_cast< vgui::ImagePanel * >( FindChildByName( "ThumbsUpSpinner" ) );
	m_pVoteDownSpinner =	dynamic_cast< vgui::ImagePanel * >( FindChildByName( "ThumbsDownSpinner" ) );

	// Turn this off to start with until the GC tells us the real state of this
	if ( m_pFollowAuthorButton )
	{
		m_pFollowAuthorButton->SetEnabled( false );
		m_pFollowAuthorButton->SetCurrentSelectionIndex( 0 );
		m_pFollowAuthorButton->SetArrowsAlwaysVisible( IsPC() );
		m_pFollowAuthorButton->SetCanWrap( true );
		m_pFollowAuthorButton->SetDrawAsDualStateButton( false );
	}

	UpdateMapFields();
	Reset();
}

void RateMapDialog::SetDataSettings( KeyValues *pSettings )
{
	m_bEndOfLevelVersion = pSettings->GetBool( "options/allowskiptonextlevel", false );

	BaseClass::SetDataSettings( pSettings );
}

//-----------------------------------------------------------------------------
// Purpose:	Update our 
//-----------------------------------------------------------------------------
void RateMapDialog::UpdateFollowStatus( bool bStatus )
{
	// Take our current state
	m_bFollowing = bStatus;

	// Update our button text to reflect the change
	if ( m_pFollowAuthorButton != NULL )
	{
		if ( m_bFollowing )
		{			
			m_pFollowAuthorButton->SetCurrentSelectionIndex( 0 );
		}
		else
		{
			m_pFollowAuthorButton->SetCurrentSelectionIndex( 1 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void RateMapDialog::OnCommand( char const *szCommand )
{
	if ( !V_stricmp( szCommand, "Back" ) )
	{
		// Act as though 360 back button was pressed
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, BASEMODPANEL_SINGLETON.GetLastActiveUserId() ) );
		return;
	}

	if ( !V_stricmp( szCommand, "NextLevel" ) )
	{
		// Act as though 360 Y button was pressed
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_X, BASEMODPANEL_SINGLETON.GetLastActiveUserId() ) );
		return;
	}

	if ( !V_stricmp( szCommand, "Follow" ) )
	{
		UpdateFollowStatus( true );
	
	}

	if ( !V_stricmp( szCommand, "StopFollowing" ) )
	{
		UpdateFollowStatus( false );
	}

	BaseClass::OnCommand( szCommand );
}

//-----------------------------------------------------------------------------
// Purpose:	Launch the author's workshop page (scoped to this game) in the Steam overlay
//-----------------------------------------------------------------------------
void RateMapDialog::ViewMapInWorkshop( void )
{
	PublishedFileId_t nMapID = BASEMODPANEL_SINGLETON.GetCurrentCommunityMapID();
	OverlayResult_t result = BASEMODPANEL_SINGLETON.ViewCommunityMapInWorkshop( nMapID );

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
			confirmation->SetUsageData(data);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void RateMapDialog::Activate()
{
	BaseClass::Activate();

	m_flTransitionStartTime = 0;

	UpdateFooter();
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void RateMapDialog::Reset()
{
	// Do something!
}

//-----------------------------------------------------------------------------
// Purpose: Launch the map, once complete
//-----------------------------------------------------------------------------
void RateMapDialog::LaunchNextMap( void )
{
	const PublishedFileInfo_t *pMapInfo = WorkshopManager().GetPublishedFileInfoByID( m_unNextMapInQueueID );
	Assert( pMapInfo );
	if ( pMapInfo == NULL )
		return;

	// Age this map from the queue to our history
	MoveMapToHistory();

	// Save this for later reference
	BASEMODPANEL_SINGLETON.SetCurrentCommunityMapID( m_unNextMapInQueueID );

	const char *lpszFilename = WorkshopManager().GetUGCFilename( pMapInfo->m_hFile );
	const char *lpszDirectory = WorkshopManager().GetUGCFileDirectory( pMapInfo->m_hFile );

	char szFilenameNoExtension[MAX_PATH];
	Q_FileBase( lpszFilename, szFilenameNoExtension, sizeof(szFilenameNoExtension) );

	char szMapName[MAX_PATH];
	V_SafeComposeFilename( lpszDirectory, szFilenameNoExtension, szMapName, sizeof(szMapName) );

	// Move past the "maps" folder, it's implied by the following call to load
	const char *lpszUnbasedDirectory = V_strnchr( szMapName, CORRECT_PATH_SEPARATOR, sizeof(szMapName) );
	lpszUnbasedDirectory++; // Move past the actual path separator character

	// Increment the number of maps we've played so far
	int nNumMapsPlayedThisSession = BASEMODPANEL_SINGLETON.GetNumCommunityMapsPlayedThisSession();
	BASEMODPANEL_SINGLETON.SetNumCommunityMapsPlayedThisSession( nNumMapsPlayedThisSession+1 );

	KeyValues *pSettings = new KeyValues( "FadeOutStartGame" );
	KeyValues::AutoDelete autodelete_pSettings( pSettings );
	pSettings->SetString( "map", lpszUnbasedDirectory );
	pSettings->SetString( "reason", "newgame" );
	BASEMODPANEL_SINGLETON.OpenWindow( WT_FADEOUTSTARTGAME, this, true, pSettings );

	m_unNextMapInQueueID = 0;
}


//-----------------------------------------------------------------------------
// Purpose:	Move on to the next map in the queue
//-----------------------------------------------------------------------------
void RateMapDialog::ProceedToNextMapInQueue( void )
{
	switch ( BASEMODPANEL_SINGLETON.GetCommunityMapQueueMode() )
	{
	case QUEUEMODE_USER_QUEUE:
	case QUEUEMODE_QUICK_PLAY:
		{
			// Get the current map we're playing as a starting point
			PublishedFileId_t nMapID = BASEMODPANEL_SINGLETON.GetCurrentCommunityMapID();
			if ( nMapID == 0 )
			{
				// FIXME: This means that we launched this menu without being in a valid community map. Huh?
				Assert( nMapID != 0 );
				Warning( "RateMapDialog::ProceedToNextMapInQueue - nMapID zero!\n" );
				return;
			}

			// Get the next map in our queue
			const PublishedFileInfo_t *pFileInfo = BASEMODPANEL_SINGLETON.GetNextCommunityMapInQueueBasedOnQueueMode();
			if ( pFileInfo == NULL )
			{
				// FIXME: This means the next map in our queue is unknown or invalid.
				Assert( pFileInfo != NULL );
				Warning( "RateMapDialog::ProceedToNextMapInQueue - pFileInfo NULL!\n" );
				return;
			}

			// It's valid info, so hold onto it in case we can't launch immediately
			m_unNextMapInQueueID = pFileInfo->m_nPublishedFileId;

			// Now see if we have a file for the map already
			if ( WorkshopManager().UGCFileRequestExists( pFileInfo->m_hFile ) )
			{
				// See if it's done downloading or in error
				UGCFileRequestStatus_t status = WorkshopManager().GetUGCFileRequestStatus( pFileInfo->m_hFile );
				if ( status == UGCFILEREQUEST_ERROR )
				{
					Assert( status != UGCFILEREQUEST_ERROR );
					BASEMODPANEL_SINGLETON.OpenMessageDialog( "#PORTAL2_WorkshopError_DownloadError_Title", "#PORTAL2_WorkshopError_DownloadError" );
					return;
				}

				if ( status == UGCFILEREQUEST_FINISHED )
				{
					// Done downloading, just launch it
					LaunchNextMap();
					return;
				}
			}
			else
			{
				// This means that we have a map in our queue that has no associated file request.
				BASEMODPANEL_SINGLETON.CreateMapFileRequest( *pFileInfo );		
			}

			// Move this query to the top of the list
			WorkshopManager().PromoteUGCFileRequestToTop( pFileInfo->m_hFile );

			// Throw up a "waiting for file download" wait screen
			KeyValues *pSettings = new KeyValues( "WaitScreen" );
			KeyValues::AutoDelete autodelete_pSettings( pSettings );
			pSettings->SetPtr( "options/asyncoperation", g_RateMapDialog_WaitForDownloadOperation.Prepare() );
			pSettings->SetUint64( "options/filehandle", pFileInfo->m_hFile );

			// We're still downloading, so stall while it works
			m_bWaitingForMapDownload = CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_CommunityPuzzle_WaitForFileDownload", 3.0f, pSettings );

			break;
		}
	case QUEUEMODE_USER_COOP_QUEUE:
	case QUEUEMODE_COOP_QUICK_PLAY:
		{
			// let community coop manager handle the transition
			// clear client ui_state
			if ( IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession() )
			{
				KeyValues *kvProgress = new KeyValues( "Portal2::ClientCommunityVotingState" );
				kvProgress->SetString( "run", "all" );
				kvProgress->SetUint64( "clxuid", g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetUserId( 0 ) )->GetXUID() );
				kvProgress->SetString( "voting_state", "has_voted" );
				pIMatchSession->Command( KeyValues::AutoDeleteInline( kvProgress ) );

				BASEMODPANEL_SINGLETON.CloseAllWindows();

				// Open a waitscreen while we wait for the puzzles to get enumerated
				BaseModUI::CUIGameData::Get()->OpenWaitScreen( "#Portal2UI_Matchmaking_Hosting", 0.0f, NULL );
			}

			m_bWaitingForMapDownload = false;
			break;
		}
	default:
		Assert(0);
	}
}


//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void RateMapDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	int iUserSlot = GetJoystickForCode( code );
	BASEMODPANEL_SINGLETON.SetLastActiveUserId( iUserSlot );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_A:	
		// If we're in the end of level version, A means continue to the next map in the queue
		if ( m_bEndOfLevelVersion )
		{
			ProceedToNextMapInQueue();
		}
		else
		{
			NavigateBack();
		}
		break;

	case KEY_XBUTTON_Y:
		ViewMapInWorkshop();
		break;
		
	case KEY_XBUTTON_X:
		// Skip to the next level
		ProceedToNextMapInQueue();
		break;

	case KEY_XBUTTON_B:
		if ( m_bEndOfLevelVersion )
		{
			// This means "Return to Queue" if we're at the end of a map
			ReturnToMapQueue();
			return;
		}

		break;
	}

	BaseClass::OnKeyCodePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose:	Close this session and return to our map queue
//-----------------------------------------------------------------------------
void RateMapDialog::ReturnToMapQueue( void )
{
	// Age this map from the queue to our history
	MoveMapToHistory();

	// All done!
	if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
	{
		// Closing an active session results in disconnecting from the game.
		g_pMatchFramework->CloseSession();
	}
	else
	{
		// On PC people can be playing via console bypassing matchmaking
		// and required session settings, so to leave game duplicate
		// session closure with an extra "disconnect" command.
		engine->ExecuteClientCmd( "disconnect" );
	}

	GameUI().ActivateGameUI();
	GameUI().AllowEngineHideGameUI();

	g_pMatchFramework->CloseSession();
	BASEMODPANEL_SINGLETON.CloseAllWindows();
	BASEMODPANEL_SINGLETON.MoveToCommunityMapQueue();
	BASEMODPANEL_SINGLETON.OpenFrontScreen();
}

// Amount of time to lock out the buttons from being pressed and display a spinner
const float g_VoteCommitDelay = 0.5f;

//-----------------------------------------------------------------------------
// Purpose:	Lock out the vote buttons and show a spinner for awhile
//-----------------------------------------------------------------------------
void RateMapDialog::StartCommitVote( void )
{
	// Lock out the buttons for an amount of time
	m_flVoteCommitTime = gpGlobals->realtime + g_VoteCommitDelay;
	
	// Pick the right spinner to show
	if ( m_pVoteUpSpinner )
	{
		m_pVoteUpSpinner->SetVisible( ( m_nVoteState == 1 ) );
	}

	if ( m_pVoteDownSpinner )
	{
		m_pVoteDownSpinner->SetVisible( ( m_nVoteState == -1 ) );
	}
	
	// Put the images into a default state
	if ( m_pVoteUpImage )
	{
		m_pVoteUpImage->SetState( 0 );
		m_pVoteUpImage->SetEnabled( false );
		m_pVoteUpImage->SetAlpha( 64 );
	}

	if ( m_pVoteDownImage )
	{
		m_pVoteDownImage->SetState( 0 );
		m_pVoteDownImage->SetEnabled( false );
		m_pVoteDownImage->SetAlpha( 64 );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void RateMapDialog::CommitChanges( void )
{
	// Commit the vote if it changed and isn't a "no vote"
	if ( m_nInitialVoteState != m_nVoteState && m_nVoteState != 0 )
	{
		StartCommitVote();

		bool bVoteUp = ( m_nVoteState == 1 );
		WorkshopManager().UpdatePublishedItemVote( m_unMapID, bVoteUp );
	}

	// Commit our follow action if it's changed
	if ( m_bInitialFollowState != m_bFollowing )
	{
		if ( m_bFollowing )
		{
			// Send the command to the GC
			GCSDK::CGCMsg<MsgGCCommunityMapFollowAuthor_t> msg( k_EMsgGCCommunityMapFollowAuthor );
			msg.Body().m_ulAuthorID = m_AuthorSteamID.GetAccountID();
			GCClientSystem()->BSendMessage( msg );
		}
		else
		{
			// Send the command to the GC
			GCSDK::CGCMsg<MsgGCCommunityMapStopFollowingAuthor_t> msg( k_EMsgGCCommunityMapStopFollowingAuthor );
			msg.Body().m_ulAuthorID = m_AuthorSteamID.GetAccountID();
			GCClientSystem()->BSendMessage( msg );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void RateMapDialog::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BASEMODPANEL_SINGLETON.GetFooterPanel();
	if ( pFooter )
	{
		int visibleButtons = FB_BBUTTON|FB_YBUTTON;

		bool bNextMapAvailable = false;
		PublishedFileId_t nMapID = BASEMODPANEL_SINGLETON.GetCurrentCommunityMapID();
		if ( BASEMODPANEL_SINGLETON.IsCommunityCoop() )
		{
			bNextMapAvailable = g_CommunityCoopManager.HasNextMap();
		}
		else if ( nMapID != 0 )
		{
			bNextMapAvailable = ( BASEMODPANEL_SINGLETON.GetNextCommunityMapInQueueBasedOnQueueMode() != NULL );
		}

		if ( m_bEndOfLevelVersion )
		{
			if ( bNextMapAvailable )
			{
				visibleButtons |= FB_ABUTTON;
				pFooter->SetButtonText( FB_ABUTTON, "#Portal2UI_NextPuzzle" );
			}

			pFooter->SetButtonText( FB_BBUTTON, "#PORTAL2_CommunityPuzzle_ReturnToQueue" );
		}
		else
		{
			visibleButtons |= FB_ABUTTON;
			pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_Done" );
			pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Cancel" );		
		}
		
		pFooter->SetButtonText( FB_YBUTTON, "#PORTAL2_CommunityPuzzle_ViewInWorkshop" );

		// Turn on the appropriate buttons for our state
		pFooter->SetButtons( visibleButtons );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void RateMapDialog::ThumbnailLoaded( const FileAsyncRequest_t &asyncRequest, int nNumReadBytes, FSAsyncStatus_t err )
{
	int nSaveGameImageId = -1;

#if !defined( NO_STEAM )
	if ( err == FSASYNC_OK )
	{
		int nWidth, nHeight;
		CUtlBuffer srcBuf;
		srcBuf.SetExternalBuffer( asyncRequest.pData, nNumReadBytes, 0, CUtlBuffer::READ_ONLY );

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
			nSaveGameImageId = m_nThumbnailImageId;

			// Free our resulting image
			dstBuf.Purge();

			// transition into the image
			m_flTransitionStartTime = Plat_FloatTime() + 0.3f;
			m_nThumbnailImageId = nSaveGameImageId;
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void MapThumbnailLoaded( const FileAsyncRequest_t &asyncRequest, int numReadBytes, FSAsyncStatus_t err )
{
	RateMapDialog *pDialog = static_cast< RateMapDialog* >( BASEMODPANEL_SINGLETON.GetWindow( WT_RATEMAP ) );
	if ( pDialog )
	{
		pDialog->ThumbnailLoaded( asyncRequest, numReadBytes, err );
	}
}	

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
bool RateMapDialog::StartAsyncThumbnailLoad( const char *pThumbnailFilename )
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

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void RateMapDialog::RunFrame()
{
	BaseClass::RunFrame();

	// Update the wait screen status if we're waiting to launch a map
	if ( m_bWaitingForMapDownload )
	{
		const PublishedFileInfo_t *pFileInfo = WorkshopManager().GetPublishedFileInfoByID( m_unNextMapInQueueID );
		if ( pFileInfo != NULL )
		{				
			if ( WorkshopManager().GetUGCFileRequestStatus( pFileInfo->m_hFile ) == UGCFILEREQUEST_FINISHED )
			{
				// Close down the wait panel in this case
				CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
				LaunchNextMap();
				return;
			}
		}
	}

	if ( m_nThumbnailImageId == -1 && m_hThumbFileHandle != k_UGCHandleInvalid ) 
	{
		if ( WorkshopManager().GetUGCFileRequestStatus( m_hThumbFileHandle ) == UGCFILEREQUEST_FINISHED )
		{
			// Start our load of this file
			char szLocalFilename[MAX_PATH];
			WorkshopManager().GetUGCFullPath( m_hThumbFileHandle, szLocalFilename, sizeof( szLocalFilename ) );

			StartAsyncThumbnailLoad( szLocalFilename );
		}
	}
	
	// Check if we should re-enable out buttons
	if ( m_flVoteCommitTime && m_flVoteCommitTime < gpGlobals->realtime )
	{
		m_flVoteCommitTime = 0;
		
		if ( m_pVoteUpImage )
		{
			m_pVoteUpImage->SetState( m_nVoteState );
			m_pVoteUpImage->SetEnabled( true );
			m_pVoteUpImage->SetAlpha( 255 );
		}
		
		if ( m_pVoteDownImage )
		{
			m_pVoteDownImage->SetState( m_nVoteState );
			m_pVoteDownImage->SetEnabled( true );
			m_pVoteDownImage->SetAlpha( 255 );
		}

		if ( m_pVoteDownSpinner )
		{
			m_pVoteDownSpinner->SetVisible( false );
		}
		
		if ( m_pVoteUpSpinner )
		{
			m_pVoteUpSpinner->SetVisible( false );
		}
	}

	const int nAnimFrame = ( uint64 )( Plat_FloatTime() * 10 );

	// Clock spinner
	if ( m_pThumbnailSpinner )
	{
		m_pThumbnailSpinner->SetFrame( nAnimFrame );
		bool bWorking = ( m_nThumbnailImageId == -1 ) || ( m_flTransitionStartTime > Plat_FloatTime() );
		m_pThumbnailSpinner->SetVisible( bWorking );
	}

	if ( m_pVoteUpSpinner )
	{
		m_pVoteUpSpinner->SetFrame( nAnimFrame );
	}

	if ( m_pVoteDownSpinner )
	{
		m_pVoteDownSpinner->SetFrame( nAnimFrame );
	}

	/*
	if ( m_bWaitingForMapDownload )
	{
		if ( m_hNextMapRequest.Update() == UGCFILEREQUEST_FINISHED )
		{
			// Close down the wait panel in this case
			// CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
			LaunchNextMapInQueue();
			m_bWaitingForMapDownload = false;
			return;
		}
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void RateMapDialog::MsgReturnToGame()
{
	NavigateBack();
}

bool RateMapDialog::GetHighlightBounds( CVoteImagePanel *pCmd, int &x, int &y, int &w, int &h )
{
	if ( !pCmd )
		return false;

	pCmd->GetPos( x, y);
	pCmd->GetSize( w, h );

	int cX, cY;
	surface()->SurfaceGetCursorPos( cX, cY );
	this->ScreenToLocal( cX, cY );

	return ( ( cX > x ) && ( cX < (x + w) ) && ( cY > y ) && ( cY < ( y + h ) ) );
}

	//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void RateMapDialog::UpdateVoteButtons( void )
{
	if ( m_pVoteUpImage && m_pVoteUpImage->IsEnabled() )
	{
		int x, y, w, h;
		bool bTrackingCursor = GetHighlightBounds( m_pVoteUpImage, x,y,w,h );

		int nState = ( bTrackingCursor ) ? 1 : 0;
		if ( m_nVoteState == 1 )
			nState = 2;

		m_pVoteUpImage->SetState( nState );
	}
	
	if ( m_pVoteDownImage && m_pVoteDownImage->IsEnabled() )
	{
		int x, y, w, h;
		bool bTrackingCursor = GetHighlightBounds( m_pVoteDownImage, x,y,w,h );
		
		int nState = ( bTrackingCursor ) ? 1 : 0;
		if ( m_nVoteState == -1 )
			nState = 2;

		m_pVoteDownImage->SetState( nState );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void RateMapDialog::PaintBackground()
{
	BaseClass::PaintBackground();

	if ( m_bNeedsMoveToFront )
	{
		vgui::ipanel()->MoveToFront( GetVPanel() );
		m_bNeedsMoveToFront = false;
	}

	DrawThumbnailImage();

	UpdateVoteButtons();
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void RateMapDialog::OnMousePressed( MouseCode code )
{
	bool bVoteCast = false;

	if ( code == MOUSE_LEFT )
	{
		if ( m_pVoteUpImage && m_pVoteUpImage->IsEnabled() )
		{
			int x, y, w, h;
			if ( GetHighlightBounds( m_pVoteUpImage, x,y,w,h ) )
			{
				if( m_nVoteState != 1 )
				{
					// Cast the vote here
					m_nVoteState = 1;
					bVoteCast = true;
				}				
			}
		}
		
		if ( m_pVoteDownImage && m_pVoteDownImage->IsEnabled() )
		{
			int x, y, w, h;
			if ( GetHighlightBounds( m_pVoteDownImage, x,y,w,h ) )
			{
				if ( m_nVoteState != -1 )
				{
					// Cast the vote here
					m_nVoteState = -1;
					bVoteCast = true;
				}
			}
		}
	}
	
	// If we've added a valid vote, then commit it
	if ( bVoteCast )
	{
		// TODO: Animate the vote being placed and delay everything while it happens
		CommitChanges();
	}

	BaseClass::OnMousePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void RateMapDialog::DrawThumbnailImage()
{
	// Don't draw if we're not loaded
	if ( m_nThumbnailImageId == -1 )
		return;

	float flLerp = 1.0;
	if ( m_flTransitionStartTime > 0 )
	{
		m_pThumbnailImage->SetVisible( true );
		flLerp = RemapValClamped( Plat_FloatTime(), m_flTransitionStartTime, m_flTransitionStartTime + 0.3f, 0.0f, 1.0f );
		if ( flLerp >= 1.0f )
		{
			// finished transition
			m_flTransitionStartTime = 0;
		}
	}
	else if ( m_pThumbnailImage->IsVisible() == false )
	{
		return;
	}

	if ( m_pThumbnailImage == NULL || m_pThumbnailImage->IsVisible() == false )
		return;

	int x, y, wide, tall;
	m_pThumbnailImage->GetBounds( x, y, wide, tall );

	// Draw a black border around the image
	surface()->DrawSetColor( Color( 0, 0, 0, flLerp * 255.0f ) );
	surface()->DrawOutlinedRect( x-1, y-1, x+wide+1, y+tall+1 );

	// Draw the thumbnail image
	surface()->DrawSetColor( Color( 255, 255, 255, flLerp * 255.0f ) );
	surface()->DrawSetTexture( m_nThumbnailImageId );
	surface()->DrawTexturedRect( x, y, x+wide, y+tall );
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
bool RateMapDialog::LoadThumbnailFromContainer( const char *pThumbnailFilename )
{
	m_ThumbnailFilename = pThumbnailFilename;
	LoadThumbnailFromContainerSuccess();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:	Read the thumbnail off our storage device, async
//-----------------------------------------------------------------------------
void RateMapDialog::LoadThumbnailFromContainerSuccess()
{
	FileAsyncRequest_t request;
	request.pszFilename = m_ThumbnailFilename.Get();
	request.pfnCallback = ::MapThumbnailLoaded;
	request.pContext = NULL;
	request.flags = FSASYNC_FLAGS_FREEDATAPTR;

	// schedule the async operation
	g_pFullFileSystem->AsyncRead( request, &m_hAsyncControl );	
}

//-----------------------------------------------------------------------------
// Purpose: Take our current map, move it out of our queue and into our history list
//-----------------------------------------------------------------------------
void RateMapDialog::MoveMapToHistory( void )
{
	PublishedFileId_t nMapID = BASEMODPANEL_SINGLETON.GetCurrentCommunityMapID();
	Assert( nMapID != 0 );
	if ( nMapID == 0 )
		return;

	// Remove this map from our queue
	// BASEMODPANEL_SINGLETON.UnsubscribeFromMap( nMapID );

	// Add a history event for it
	CRTime::UpdateRealTime();
	RTime32 currentTime = CRTime::RTime32TimeCur();
	BASEMODPANEL_SINGLETON.MarkCommunityMapCompletionTime( nMapID, currentTime );
}

//-----------------------------------------------------------------------------
// Purpose: Receive messages to do with community maps being subscribed to
//-----------------------------------------------------------------------------
class CGCIsFollowingAuthorResponse : public GCSDK::CGCClientJob
{
public:
	CGCIsFollowingAuthorResponse( GCSDK::CGCClient *pClient ) : GCSDK::CGCClientJob( pClient ) {}

	virtual bool BYieldingRunGCJob( GCSDK::IMsgNetPacket *pNetPacket )
	{		
		GCSDK::CGCMsg<MsgGCCommunityMapIsFollowingAuthorResponse_t> msg( pNetPacket );
		BroadcastFollowState( msg.Body().m_bFollowing );
		return true;
	}
};

GC_REG_JOB( GCSDK::CGCClient, CGCIsFollowingAuthorResponse, "CGCIsFollowingAuthorResponse", k_EMsgGCCommunityMapIsFollowingAuthorResponse, GCSDK::k_EServerTypeGCClient );

#endif // PORTAL2_PUZZLEMAKER
