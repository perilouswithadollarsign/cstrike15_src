//========= Copyright � 1996-2010, Valve Corporation, All rights reserved. ============//
//
//
//=============================================================================//

#include "cbase.h"

#if defined( PORTAL2_PUZZLEMAKER )

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
#include "transitionpanel.h"
#include "econ_gcmessages.h"
#include "gc_clientsystem.h"
#include "imageutils.h"
#include "vCommunityMapDialog.h"
#include "portal_mp_gamerules.h"
#include "c_community_coop.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------------------------------
// Purpose: Reset the progress of the DLC2 storyline
//--------------------------------------------------------------------------------------------------------------
void CommandResetVOProgress( void )
{
	filesystem->RemoveFile( "scripts/vo_progress.txt", "MOD" );
}
static ConCommand cm_reset_vo_progress( "cm_reset_vo_progress", CommandResetVOProgress, "Reset the progress of the PeTI storyline." );

#define MIN_WAITSCREEN_DELAY	3.0f // Seconds

ConVar ui_show_community_map_names( "ui_show_community_map_names", "0" );

using namespace vgui;
using namespace BaseModUI;

class CCommunityMaps_WaitForDownloadOperation : public IMatchAsyncOperation
{
public:
	virtual bool IsFinished() { return false; }
	virtual AsyncOperationState_t GetState() { return AOS_RUNNING; }
	virtual uint64 GetResult() { return 0ull; }
	virtual void Abort();
	virtual void Release() { Assert( 0 ); }

public:
	CCommunityMaps_WaitForDownloadOperation() {}
	IMatchAsyncOperation * Prepare();
}
g_CommunityMaps_WaitForDownloadOperation;

IMatchAsyncOperation *CCommunityMaps_WaitForDownloadOperation::Prepare()
{
	return this;
}

void CCommunityMaps_WaitForDownloadOperation::Abort()
{
	CommunityMapDialog *pMapDialog = ( CommunityMapDialog * ) CBaseModPanel::GetSingleton().GetWindow( WT_COMMUNITYMAP );
	if ( !pMapDialog )
		return;
	
	pMapDialog->PostMessage( pMapDialog, new KeyValues( "MapDownloadAborted", "msg", "" ) );
}

//=============================================================================
//
// Custom hybrid bitmap button code
//
//=============================================================================

HybridBitmapButton::HybridBitmapButton( vgui::Panel *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget /*= NULL*/, const char *pCmd /*= NULL*/ ) :
	BaseClass( parent, panelName, text, pActionSignalTarget, pCmd )
{
}

HybridBitmapButton::HybridBitmapButton( vgui::Panel *parent, const char *panelName, const wchar_t *text, vgui::Panel *pActionSignalTarget /*= NULL*/, const char *pCmd /*= NULL*/ ) :
	BaseClass( parent, panelName, text, pActionSignalTarget, pCmd )
{
}

//-----------------------------------------------------------------------------
// Purpose: Allow a background color and border to be specified
//-----------------------------------------------------------------------------
void HybridBitmapButton::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	m_TextColor = Color( 225, 225, 225, 255 );
	m_FocusColor = Color( 255, 255, 255, 255 );
	m_CursorColor = m_TextColor;
	m_DisabledColor = m_TextColor;
	m_FocusDisabledColor = m_TextColor;
	m_ListButtonActiveColor = m_TextColor;
	m_ListButtonInactiveColor = m_TextColor;

	BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: See if the mouse is hovering over us
//-----------------------------------------------------------------------------
bool HybridBitmapButton::GetHighlightBounds( int &x, int &y, int &w, int &h )
{
	GetPos( x, y );
	GetSize( w, h );

	int cX, cY;
	surface()->SurfaceGetCursorPos( cX, cY );
	
	int pX, pY;
	GetParent()->GetPos( pX, pY );

	cX-=pX;
	cY-=pY;

	return ( ( cX > x ) && ( cX < (x + w) ) && ( cY > y ) && ( cY < ( y + h ) ) );
}

//-----------------------------------------------------------------------------
// Purpose: Allow a background color and border to be specified
//-----------------------------------------------------------------------------
void HybridBitmapButton::Paint( void )
{
	int x, y, w, h;
	bool bTrackingCursor = GetHighlightBounds( x, y, w, h );

	if ( bTrackingCursor )
	{
		m_TextColor = Color( 255, 255, 255, 255 );
		m_FocusColor = Color( 255, 255, 255, 255 );

		SetDefaultColor( Color( 255, 255, 255, 255 ), Color( 0, 0, 0, 96 ) );
		SetArmedColor( Color( 255, 255, 255, 255 ), Color( 0, 0, 0, 96 ) );
		SetDepressedColor( Color( 255, 255, 255, 255 ), Color( 0, 0, 0, 128 ) );
		SetButtonBorderEnabled( true );
	}
	else
	{
		m_TextColor = Color( 225, 225, 225, 255 );
		m_FocusColor = Color( 255, 255, 255, 255 );

		SetDefaultColor( Color( 128, 128, 128, 128 ), Color( 0, 0, 0, 32 ) );
		SetArmedColor( Color( 164, 164, 164, 128 ), Color( 0, 0, 0, 32 ) );
		SetDepressedColor( Color( 255, 255, 255, 128 ), Color( 0, 0, 0, 64 ) );
		SetButtonBorderEnabled( false );
	}

	// We'd like to do this regardless...
	PaintBorder();
	PaintBackground();

	BaseClass::Paint();
}

DECLARE_BUILD_FACTORY_DEFAULT_TEXT( HybridBitmapButton, "" );

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
IconRatingItem::IconRatingItem( vgui::Panel *pParent, const char *pPanelName ) 	: 
	BaseClass( pParent, pPanelName ),
	m_flRating( 0.0f )
{
	for ( int i=0; i < RATING_SCALE; i++ )
	{
		m_pPointIcons[i] = NULL;
	}
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
IconRatingItem::~IconRatingItem( void )
{
	for ( int i=0; i < RATING_SCALE; i++ )
	{
		delete m_pPointIcons[i];
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the rating for the item
//-----------------------------------------------------------------------------
void IconRatingItem::SetRating( float flRating )
{
	m_flRating = (flRating*RATING_SCALE);

	int nWhole = (int) ( m_flRating + 0.5f );
	
	// Now, update the internal "stars" to reflect this
	for ( int i=1; i < RATING_SCALE+1; i++ )
	{
		if ( m_flRating >= i )
		{
			m_pPointIcons[i-1]->SetImage( m_szOnIconName );
		}
		else 
		{
			if ( nWhole < i )
			{
				m_pPointIcons[i-1]->SetImage( m_szOffIconName );
			}
			else
			{
				m_pPointIcons[i-1]->SetImage( m_szHalfIconName );
			}
		}
	}

	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void IconRatingItem::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	// Get our icon names out
	Q_strncpy( m_szOnIconName, inResourceData->GetString( "on_image", "" ), ARRAYSIZE(m_szOnIconName) );
	Q_strncpy( m_szOffIconName, inResourceData->GetString( "off_image", "" ), ARRAYSIZE(m_szOffIconName) );
	Q_strncpy( m_szHalfIconName, inResourceData->GetString( "half_image", "" ), ARRAYSIZE(m_szHalfIconName) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void IconRatingItem::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// Setup all of our icons
	for ( int i=0; i < RATING_SCALE; i++ )
	{
		if ( m_pPointIcons[i] == NULL )
		{
			m_pPointIcons[i] = new vgui::ImagePanel( this, "RatingTick" );
		}
		
		m_pPointIcons[i]->SetImage( m_szOffIconName );
		m_pPointIcons[i]->SetShouldScaleImage( true );
		
		// FIXME: We'll need to make some adjustments on the art side
		m_pPointIcons[i]->SetTall( vgui::scheme()->GetProportionalScaledValue( 10 ) );
		m_pPointIcons[i]->SetWide( vgui::scheme()->GetProportionalScaledValue( 10 ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void IconRatingItem::SetEnabled( bool bEnabled )
{
	int nAlpha = ( bEnabled ) ? 255 : 64;
	for ( int i=0; i < RATING_SCALE; i++ )
	{
		if ( m_pPointIcons[i] != NULL )
		{
			m_pPointIcons[i]->SetAlpha( nAlpha );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void IconRatingItem::PerformLayout( void )
{
	BaseClass::PerformLayout();

	// Lay out all the icons

	int nHeight = GetTall();
	int nTickHeight = m_pPointIcons[0]->GetTall();
	int nTallOffset = ( nHeight - nTickHeight ) / 2;

	int nWidth = GetWide();
	int nTickWidth = m_pPointIcons[0]->GetWide();
	int nTotalTickWidth = ( nTickWidth * RATING_SCALE );

	int nPadding = ( nWidth - nTotalTickWidth ) / (RATING_SCALE-1);

	// Now lay them all out
	for ( int i=0; i < RATING_SCALE; i++ )
	{
		if ( m_pPointIcons[i] )
		{
			m_pPointIcons[i]->SetPos( (nTickWidth*i)+(nPadding*i), nTallOffset );
		}
	}
}

DECLARE_BUILD_FACTORY( IconRatingItem );

namespace BaseModUI
{

//=============================================================================
//
//  Community map dialog
//
//=============================================================================

static CommunityMapDialog * GetMyCommunityMapDialog()
{
	return static_cast< CommunityMapDialog * >( BASEMODPANEL_SINGLETON.GetWindow( WT_COMMUNITYMAP ) );
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CommunityMapInfoLabel::CommunityMapInfoLabel( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName, "" )
{
	m_pDialog = dynamic_cast< CommunityMapDialog * >( pParent );

	SetProportional( true );
	SetPaintBackgroundEnabled( true );

	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );

	m_hAuthorSteamNameFont = vgui::INVALID_FONT;

	m_nCommunityMapIndex = 0;

	m_AuthorSteamNameString[0] = '\0';
}

//-----------------------------------------------------------------------------
// Purpose: Setup our label info
//-----------------------------------------------------------------------------
void CommunityMapInfoLabel::SetCommunityMapInfo( PublishedFileId_t nIndex, uint64 unCreatorID )
{
	m_nCommunityMapIndex = nIndex;
	m_ulOwnerID = unCreatorID;

	Update();
}

//-----------------------------------------------------------------------------
// Purpose: Cause our text to update based on our new persona data
//-----------------------------------------------------------------------------
void CommunityMapInfoLabel::Update( void )
{
	CSteamID steamID( m_ulOwnerID, steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual );
	wchar_t convertedString[MAX_PATH] = L"";

	const wchar_t * authorFormat = g_pVGuiLocalize->Find( "#PORTAL2_CommunityPuzzle_Author" );
	g_pVGuiLocalize->ConvertANSIToUnicode( steamapicontext->SteamFriends()->GetFriendPersonaName( steamID ), convertedString, sizeof( convertedString ) );
	if ( authorFormat )
	{
		g_pVGuiLocalize->ConstructString( m_AuthorSteamNameString, ARRAYSIZE( m_AuthorSteamNameString ), authorFormat, 1, convertedString );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapInfoLabel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_hAuthorSteamNameFont = pScheme->GetFont( "NewGameChapterName", true );
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapInfoLabel::PaintBackground()
{
	// Draw the other text
	DrawText( 0, 0, m_AuthorSteamNameString, m_hAuthorSteamNameFont, Color( 255, 255, 255, 255 ) );
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
int CommunityMapInfoLabel::DrawText( int x, int y, const wchar_t *pString, vgui::HFont hFont, Color color )
{
	int len = V_wcslen( pString );

	int textWide, textTall;
	surface()->GetTextSize( hFont, pString, textWide, textTall );

	vgui::surface()->DrawSetTextFont( hFont );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawSetTextColor( color );
	vgui::surface()->DrawPrintText( pString, len );

	return textWide;
}

//=============================================================================
//
//  Per=map item which displays in a sorted list
//
//=============================================================================

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CommunityMapItem::CommunityMapItem( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName )
{
	m_pListCtrlr = dynamic_cast< GenericPanelList * >( pParent );
	m_pDialog = dynamic_cast< CommunityMapDialog * >( m_pListCtrlr->GetParent() );

	SetProportional( true );
	SetPaintBackgroundEnabled( true );

	m_timeSubscribed = 0;
	m_timeLastPlayed = 0;
	m_nCommunityMapIndex = 0;

	m_TitleString[0] = '\0';

	m_hTitleFont = vgui::INVALID_FONT;

	m_nTextOffsetY = 0;

	m_bSelected = false;
	m_bHasMouseover = false;
	m_bDisabled = true;
}

//-----------------------------------------------------------------------------
// Purpose:	Set the info for this entry
//-----------------------------------------------------------------------------
void CommunityMapItem::SetCommunityMapInfo( PublishedFileId_t nIndex, uint32 timeSubscribed, uint32 timeLastPlayed )
{
	m_nCommunityMapIndex = nIndex;
	m_timeSubscribed = timeSubscribed;
	m_timeLastPlayed = timeLastPlayed;
}

//-----------------------------------------------------------------------------
// Purpose:	Set the visible title for this entry
//-----------------------------------------------------------------------------
void CommunityMapItem::SetTitle( const wchar_t *lpszTitle )
{
	V_wcsncpy( m_TitleString, lpszTitle, sizeof( m_TitleString ) );
}

//-----------------------------------------------------------------------------
// Purpose:	Set the owner's id
//-----------------------------------------------------------------------------
void CommunityMapItem::SetOwnerID( uint64 nOwnerID )
{
	m_nOwnerID = nOwnerID;
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapItem::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/BaseModUI/CommunityMapItem.res" );

	m_hTitleFont = pScheme->GetFont( "NewGameChapterName", true );

	m_TextColor = GetSchemeColor( "HybridButton.TextColorAlt", pScheme );
	m_FocusColor = GetSchemeColor( "HybridButton.FocusColorAlt", pScheme );
	m_CursorColor = GetSchemeColor( "HybridButton.CursorColorAlt", pScheme );
	m_MouseOverCursorColor = GetSchemeColor( "HybridButton.MouseOverCursorColorAlt", pScheme );
	m_DisabledColor = Color( 255, 255, 255, 48 );

	m_nTextOffsetY = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "SaveGameDialog.TextOffsetY" ) ) );
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapItem::PaintBackground()
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

	Label *pLabel = dynamic_cast< Label* >( FindChildByName( "LblMapName" ) );
	if ( pLabel )
	{
		int x, y, labelWide, labelTall;
		pLabel->GetBounds( x, y, labelWide, labelTall );

		Color textColor = m_TextColor;
		if ( m_bDisabled )
		{
			if ( bHasFocus || HasMouseover() )
			{
				textColor = m_FocusColor;
			}
			else
			{
				textColor = m_DisabledColor;
			}
		}
		else if ( bHasFocus || HasMouseover() )
		{
			textColor = m_FocusColor;
		}

		DrawText( x, y, labelTall, m_TitleString, m_hTitleFont, textColor );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
int	CommunityMapItem::DrawText( int x, int y, int nLabelTall, const wchar_t *pString, vgui::HFont hFont, Color color )
{
	int len = V_wcslen( pString );

	int textWide, textTall;
	surface()->GetTextSize( hFont, pString, textWide, textTall );

	// vertical center
	y += ( nLabelTall - textTall ) / 2 + m_nTextOffsetY;

	vgui::surface()->DrawSetTextFont( hFont );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawSetTextColor( color );
	vgui::surface()->DrawPrintText( pString, len );

	return textWide;
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapItem::OnCursorEntered()
{ 
	SetHasMouseover( true ); 

	if ( IsPC() )
		return;

	if ( IsGameConsole() )
		return;

	if ( GetParent() )
		GetParent()->NavigateToChild( this );
	else
		NavigateTo();
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapItem::OnCursorExited() 
{
	SetHasMouseover( false ); 
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapItem::NavigateTo()
{
	m_pListCtrlr->SelectPanelItemByPanel( this );
#if !defined( _GAMECONSOLE )
	SetHasMouseover( true );
	RequestFocus();
#endif
	BaseClass::NavigateTo();
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapItem::NavigateFrom()
{
	SetHasMouseover( false );
	BaseClass::NavigateFrom();
#ifdef _GAMECONSOLE
	OnClose();
#endif
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapItem::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( !m_pDialog->IsInputEnabled() )
		return;

	int iUserSlot = GetJoystickForCode( code );
	BASEMODPANEL_SINGLETON.SetLastActiveUserId( iUserSlot );

	bool bCodeHandled = false;
	if ( m_pDialog->IsShowingMapQueue() )
	{
		bCodeHandled = OnKeyCodePressed_Queue( code );
	}
	else
	{
		bCodeHandled = OnKeyCodePressed_History( code );
	}

	// Pass the message along if we didn't find anything to do with it
	if ( bCodeHandled == false )
	{
		BaseClass::OnKeyCodePressed( code );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Open the overlay to the Workshop page for this map
//-----------------------------------------------------------------------------
void CommunityMapItem::ViewMapInWorkshop( void )
{
	CommunityMapItem* pListItem = static_cast< CommunityMapItem* >( m_pListCtrlr->GetSelectedPanelItem() );
	if ( pListItem )
	{
#if !defined(NO_STEAM)
		OverlayResult_t result = BASEMODPANEL_SINGLETON.ViewCommunityMapInWorkshop( pListItem->GetCommunityMapIndex() );

		if( result != RESULT_OK )
		{
			if( result == RESULT_FAIL_OVERLAY_DISABLED )
			{
				GenericConfirmation* confirmation = 
					static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, m_pDialog, true ) );
				GenericConfirmation::Data_t data;
				data.pWindowTitle = "#L4D360UI_SteamOverlay_Title";
				data.pMessageText = "#L4D360UI_SteamOverlay_Text";
				data.bOkButtonEnabled = true;
				data.pfnOkCallback = CommunityMapDialog::FixFooter;
				confirmation->SetUsageData(data);
			}
		}
#endif // !NO_STEAM
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
bool CommunityMapItem::OnKeyCodePressed_Queue( vgui::KeyCode code )
{
	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_A:
	case KEY_ENTER:
		{
			CommunityMapItem* pListItem = static_cast< CommunityMapItem* >( m_pListCtrlr->GetSelectedPanelItem() );
			if ( pListItem )
			{
				if ( BASEMODPANEL_SINGLETON.GetCommunityMapQueueMode() == QUEUEMODE_USER_QUEUE )
				{
					// Start our download
					bool bRecordDemo = false; // ( code == KEY_XBUTTON_LEFT_SHOULDER ) ? true : false;
					m_pDialog->BeginLaunchCommunityMap( pListItem->GetCommunityMapIndex(), bRecordDemo );
				}
				else
				{
					// Start coop download
					m_pDialog->BeginLaunchCoopCommunityMap( pListItem->GetCommunityMapIndex() );
				}
			}
		}
		return true;

	case KEY_XBUTTON_LEFT_SHOULDER:
		{
			ViewMapInWorkshop();
			return true;
		}
		break;

	case KEY_XBUTTON_X:
	case KEY_DELETE:
		{
			CBaseModFooterPanel *pFooter = BASEMODPANEL_SINGLETON.GetFooterPanel();
			if ( pFooter && ( pFooter->GetButtons() & FB_XBUTTON ) )
			{
				CommunityMapItem* pListItem = static_cast< CommunityMapItem* >( m_pListCtrlr->GetSelectedPanelItem() );
				if ( pListItem )
				{
					m_pDialog->RequestDeleteCommunityMap( pListItem->GetCommunityMapIndex() );
				}
			}
		}
		return true;

	case KEY_XBUTTON_Y:
		m_pDialog->HandleYbutton();
		return true;
	}

	// Not handled
	return false;
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
bool CommunityMapItem::OnKeyCodePressed_History( vgui::KeyCode code )
{
	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_A:
	case KEY_ENTER:
		{
			BASEMODPANEL_SINGLETON.SubscribeToMap( GetCommunityMapIndex() );

			// Take it out of our list
			unsigned short nPanelIndex;
			m_pListCtrlr->GetPanelItemIndex( this, nPanelIndex );

			m_pListCtrlr->RemovePanelItem( nPanelIndex );
		}
		return true;
	
	case KEY_XBUTTON_LEFT_SHOULDER:
		{
			ViewMapInWorkshop();
			return true;
		}
		break;
	}
	
	// Not handled
	return false;
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapItem::OnMousePressed( vgui::MouseCode code )
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
void CommunityMapItem::OnMouseDoublePressed( vgui::MouseCode code )
{
	if ( m_pDialog->IsShowingMapQueue() )
	{
		if ( code == MOUSE_LEFT )
		{
			CommunityMapItem* pListItem = static_cast< CommunityMapItem* >( m_pListCtrlr->GetSelectedPanelItem() );
			if ( pListItem )
			{
				OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, BASEMODPANEL_SINGLETON.GetLastActiveUserId() ) );
			}
		}
	}

	BaseClass::OnMouseDoublePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapItem::PerformLayout()
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

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapItem::OnMessage( const KeyValues *params, vgui::VPANEL ifromPanel )
{
	BaseClass::OnMessage( params, ifromPanel );

	if ( !V_strcmp( params->GetName(), "PanelSelected" ) ) 
	{
		SetSelected( true );
	}
	if ( !V_strcmp( params->GetName(), "PanelUnSelected" ) ) 
	{
		SetSelected( false );
	}

}

#ifdef _PS3
CPS3AsyncStatus CommunityMapDialog::m_PS3AsyncStatus;
#endif


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CommunityMapDialog::CommunityMapDialog( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName, false, true )
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );

	// Subscribe to event notifications
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	// allows us to get RunFrame() during wait screen occlusion
	AddFrameListener( this );

	m_bInputEnabled = true;

	m_bSaveStarted = false;
	m_bWaitingForMapDownload = false;
	m_flWaitScreenDelay = 0.0f;
	m_bSaveInProgress = false;
	m_bSteamCloudResetRequested = false;
	m_bSetupComplete = false;

	m_pThumbnailImage = NULL;
	m_pThumbnailSpinner = NULL;
	m_pDownloadingSpinner = NULL;
	m_pQueueSpinner = NULL;
	m_pAvatarSpinner = NULL;
	m_pVoteInfoSpinner = NULL;

	m_pAuthorAvatarImage = NULL;

	m_hAsyncControl = NULL;

	m_nMapThumbnailId = -1;

	m_nNoSaveGameImageId = -1;
	m_nNewSaveGameImageId = -1;
	m_nThumbnailImageId = -1;

	m_flTransitionStartTime = 0;
	m_flNextLoadThumbnailTime = 0;

	m_flAvatarRetryTime = -1;
	m_nCommunityMapToLaunch = 0;

#if defined( _X360 )
	int iUserSlot = BASEMODPANEL_SINGLETON.GetLastActiveUserId();
	int iController = XBX_GetUserId( iUserSlot );
	DWORD nStorageDevice = XBX_GetStorageDeviceId( iController );
	m_bHasStorageDevice = ( XBX_DescribeStorageDevice( nStorageDevice ) != 0 );
#else
	// all other platforms have static storage devices
	m_bHasStorageDevice = true;
#endif
	
#ifdef _PS3
	m_PS3AsyncStatus.Reset();
#endif

	m_pCommunityMapList = new CCommunityMapList( this, "CommunityMapList", GenericPanelList::ISM_PERITEM | GenericPanelList::ISM_ALPHA_INVISIBLE );
	m_pCommunityMapList->SetPaintBackgroundEnabled( false );	

	m_pCommunityMapInfoLabel = new CommunityMapInfoLabel( this, "CommunityMapInfo" );
	m_pRatingItem = NULL;

	// make sure the queue type is correct, so the vcommunitymapdialog displays the correct queue
	switch ( BASEMODPANEL_SINGLETON.GetCommunityMapQueueMode() )
	{
	case QUEUEMODE_QUICK_PLAY:
		BASEMODPANEL_SINGLETON.SetCommunityMapQueueMode( QUEUEMODE_USER_QUEUE );
		break;
	case QUEUEMODE_COOP_QUICK_PLAY:
		BASEMODPANEL_SINGLETON.SetCommunityMapQueueMode( QUEUEMODE_USER_COOP_QUEUE );
		break;
	case QUEUEMODE_INVALID:
		Assert(0);
	}

	bool bCoop = BASEMODPANEL_SINGLETON.GetCommunityMapQueueMode() == QUEUEMODE_USER_COOP_QUEUE;
	const char *pDialogTitle = bCoop ? "PORTAL2_CommunityPuzzle_CoopBrowserTitle" : "PORTAL2_CommunityPuzzle_SPBrowserTitle";
	SetDialogTitle( pDialogTitle );

	SetFooterEnabled( true );
	
	m_ulCurrentAvatarPlayerID = 0;

	m_bQueueHistoryRequested = false;

	m_CallbackPersonaStateChanged.Register( this, &CommunityMapDialog::Steam_OnPersonaUpdated );

	Log_Msg( LOG_WORKSHOP, "[VCommunityMapDialog] Opened\n" );
}

//-----------------------------------------------------------------------------
// se
//-----------------------------------------------------------------------------
CommunityMapDialog::~CommunityMapDialog()
{
	if ( m_hAsyncControl )
	{
		g_pFullFileSystem->AsyncFinish( m_hAsyncControl, true );
		g_pFullFileSystem->AsyncRelease( m_hAsyncControl );
		m_hAsyncControl = NULL;
	}

	if ( surface() && m_nMapThumbnailId != -1 )
	{
		// evict prior screenshot
		surface()->DestroyTextureID( m_nMapThumbnailId );
		m_nMapThumbnailId = -1;
	}

	delete m_pCommunityMapList;
	delete m_pCommunityMapInfoLabel;

#ifdef _PS3
	m_PS3AsyncStatus.Reset();
#endif

	// Unsubscribe from event notifications
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );

	RemoveFrameListener( this );

	// Kill the author avatar
	if ( m_pAuthorAvatarImage )
	{
		if ( m_ulCurrentAvatarPlayerID != 0 )
		{
			BaseModUI::CUIGameData::Get()->AccessAvatarImage( m_ulCurrentAvatarPlayerID, BaseModUI::CUIGameData::kAvatarImageRelease );
			m_ulCurrentAvatarPlayerID = 0;
		}
		
		m_pAuthorAvatarImage = NULL;		
	}

	if ( m_hAsyncControl )
	{
		g_pFullFileSystem->AsyncFinish( m_hAsyncControl );
		g_pFullFileSystem->AsyncRelease( m_hAsyncControl );
		m_hAsyncControl = NULL;
	}

	Log_Msg( LOG_WORKSHOP, "[VCommunityMapDialog] Closed\n" );
}

//-----------------------------------------------------------------------------
// Purpose:	Request our queue history from the GC so we can populate our list
//-----------------------------------------------------------------------------
void CommunityMapDialog::RequestQueueHistory( void )
{
	// BASEMODPANEL_SINGLETON.RequestQueueHistory();
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pThumbnailImage = dynamic_cast< ImagePanel* >( FindChildByName( "CommunityMapImage" ) );
	m_pThumbnailSpinner = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "ThumbnailSpinner" ) );
	m_pDownloadingSpinner = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "DownloadingSpinner" ) );
	m_pQueueSpinner = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "QueueSpinner" ) );
	m_pVoteInfoSpinner = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "VoteSpinner" ) );
	m_pAvatarSpinner = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "AvatarSpinner" ) );
	m_pAuthorAvatarImage = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "AuthorAvatarImage" ) );
	m_pNoMapsLabel1 = dynamic_cast< vgui::Label * >( FindChildByName( "NoMapsLabel1" ) );
	m_pListTypeButton =  dynamic_cast< CDialogListButton * >( FindChildByName( "ListBtnQueueType" ) );
	m_nNoSaveGameImageId = BASEMODPANEL_SINGLETON.GetImageId( "vgui/no_save_game" );
	m_nNewSaveGameImageId = BASEMODPANEL_SINGLETON.GetImageId( "vgui/new_save_game" );
	m_pRatingItem = dynamic_cast< IconRatingItem * >( FindChildByName( "RatingsItem" ) );
	m_pTotalVotesLabel = dynamic_cast< vgui::Label * >( FindChildByName( "TotalVotesLabel" ) );
	m_pNoBaselineLabel1 = dynamic_cast< vgui::Label * >( FindChildByName( "NoBaselineLabel1" ) );
	m_pNoBaselineLabel2 = dynamic_cast< vgui::Label * >( FindChildByName( "NoBaselineLabel2" ) );
	m_pQuickPlayButton = dynamic_cast< vgui::Button * >( FindChildByName( "BtnQuickPlay" ) );

	// Window dressing for author badge
	m_pAuthorBadge = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "ImgEmployeeBadge" ) );
	m_pAuthorBadgeOverlay = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "ImgBadgeOverlay" ) );
	// m_pAuthorBadgeAward = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "ImgBadgeUpgrade" ) );
	m_pAuthorBadgeLogo = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "ImgBadgeLogo" ) );

	if ( m_pCommunityMapList )
	{
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom();
		}
		m_pCommunityMapList->NavigateTo();
		m_pCommunityMapList->SetScrollBarVisible( false );
	}

	if ( m_pListTypeButton )
	{
		m_pListTypeButton->SetCurrentSelectionIndex( 0 );
		m_pListTypeButton->SetArrowsAlwaysVisible( IsPC() );
		m_pListTypeButton->SetCanWrap( true );
		m_pListTypeButton->SetDrawAsDualStateButton( false );
		m_pListTypeButton->SetVisible( false );
	}

	if ( m_bQueueHistoryRequested == false )
	{
		RequestQueueHistory();
		m_bQueueHistoryRequested = true;
	}

	m_bSetupComplete = true;

	Reset();
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::OnCommand( char const *szCommand )
{
	if ( !V_stricmp( szCommand, "Back" ) )
	{
		// Act as though 360 back button was pressed
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, BASEMODPANEL_SINGLETON.GetLastActiveUserId() ) );
		return;
	}
	
	if ( !V_stricmp( szCommand, "BrowseWorkshop" ) )
	{
		// Act as though 360 Y button was pressed
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_Y, BASEMODPANEL_SINGLETON.GetLastActiveUserId() ) );
		return;
	}
	else if ( !V_stricmp( szCommand, "CommunityMaps_Queue" ) )
	{
		// Show the map queue
		PopulateCommunityMapList();
	}
	else if ( !V_stricmp( szCommand, "CommunityMaps_History" ) )
	{
		// Show the map history
		PopulateQueueHistoryList();
	}
	else if ( !V_stricmp( szCommand, "QuickPlay" ) )
	{
		// Show the quick play options
		BASEMODPANEL_SINGLETON.SetCommunityMapQueueMode( BASEMODPANEL_SINGLETON.GetCommunityMapQueueMode() == QUEUEMODE_USER_COOP_QUEUE ? QUEUEMODE_COOP_QUICK_PLAY : QUEUEMODE_QUICK_PLAY );
		CBaseModPanel::GetSingleton().OpenWindow( WT_QUICKPLAY, this, true );		
	}

	BaseClass::OnCommand( szCommand );
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::Activate()
{
	BaseClass::Activate();

	g_CommunityCoopManager.ClearPartnerHistory();
	
	if ( m_bSetupComplete )
	{
		UpdateFooter();
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::Reset()
{
	if ( IsShowingMapQueue() )
	{
		PopulateCommunityMapList();
	}
	else 
	{
		PopulateQueueHistoryList();
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( !IsGameConsole() )
	{
		// handle button presses by the footer
		CommunityMapItem* pListItem = static_cast< CommunityMapItem* >( m_pCommunityMapList->GetSelectedPanelItem() );
		if ( pListItem )
		{
			if ( code == KEY_XBUTTON_A || code == KEY_XBUTTON_X || code == KEY_DELETE ||
				 code == KEY_ENTER || code == KEY_XBUTTON_LEFT_SHOULDER )
			{
				pListItem->OnKeyCodePressed( code );
				return;
			}
		}
	}

	int iUserSlot = GetJoystickForCode( code );
	BASEMODPANEL_SINGLETON.SetLastActiveUserId( iUserSlot );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_Y:
		HandleYbutton();
		return;
	}

	BaseClass::OnKeyCodePressed( code );
}

void CommunityMapDialog::FixFooter()
{
	CommunityMapDialog *pSelf = static_cast<CommunityMapDialog*>( CBaseModPanel::GetSingleton().GetWindow( WT_COMMUNITYMAP ) );
	if( pSelf )
	{
		pSelf->UpdateFooter();
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::HandleYbutton()
{
#if !defined(NO_STEAM)
	OverlayResult_t result = BASEMODPANEL_SINGLETON.ViewAllCommunityMapsInWorkshop();

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
			data.pfnOkCallback = CommunityMapDialog::FixFooter;
			confirmation->SetUsageData(data);
		}
	}
#endif // !NO_STEAM
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::UpdateFooter( void )
{
	// Don't mess with the footer if we're not the active window
	if( BASEMODPANEL_SINGLETON.GetActiveWindowType() != WT_COMMUNITYMAP )
	{
		return;
	}

	CBaseModFooterPanel *pFooter = BASEMODPANEL_SINGLETON.GetFooterPanel();
	if ( pFooter )
	{
		if ( IsShowingMapQueue() )
		{
			int visibleButtons = (FB_ABUTTON|FB_BBUTTON|FB_XBUTTON|FB_YBUTTON|FB_LSHOULDER);

			// Must have maps in our queue to do any of these commands
			if ( m_pCommunityMapList && m_pCommunityMapList->GetPanelItemCount() == 0 )
			{
				visibleButtons &= ~(FB_ABUTTON|FB_XBUTTON|FB_LSHOULDER);
			}

			pFooter->SetButtons( visibleButtons, FF_ABXYDL_ORDER );

			if ( IsGameConsole() )
			{
				pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
			}
			else
			{
				// pFooter->SetButtonText( FB_ABUTTON, IsSaveGameDialog() ? "#PORTAL2_ButtonAction_Save" : "#PORTAL2_ButtonAction_Load" );
			}
			pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_SignIn_SignInPlay" );
			pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
			pFooter->SetButtonText( FB_XBUTTON, "#PORTAL2_CommunityPuzzle_RemovePuzzleFromQueue" );
			pFooter->SetButtonText( FB_YBUTTON, "#PORTAL2_CommunityPuzzle_FindPuzzles" );			
			pFooter->SetButtonText( FB_LSHOULDER, "#PORTAL2_CommunityPuzzle_ViewInWorkshop" );
		}
		else
		{
			int visibleButtons = FB_BBUTTON;

			// Must have maps in our queue history to do any of these commands
			if ( m_pCommunityMapList && m_pCommunityMapList->GetPanelItemCount() > 0 )
			{
				visibleButtons |= (FB_LSHOULDER|FB_ABUTTON);
			}

			pFooter->SetButtons( visibleButtons, FF_ABXYDL_ORDER );

			pFooter->SetButtonText( FB_ABUTTON, "#PORTAL2_CommunityPuzzle_AddToQueue" );
			pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
			pFooter->SetButtonText( FB_LSHOULDER, "#PORTAL2_CommunityPuzzle_ViewInWorkshop" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::OnItemSelected( const char *pPanelName )
{
	CommunityMapItem* pListItem = static_cast< CommunityMapItem* >( m_pCommunityMapList->GetSelectedPanelItem() );
	
	// Setup our information for the map
	SetSelectedMap( pListItem->GetCommunityMapIndex() );
}


//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::OnItemRemoved( const char *pPanelName )
{
	// check if there are no more items in the panel list
	if ( m_pCommunityMapList->GetPanelItemCount() == 0 )
	{
		// clear the removed panel as being active
		m_ActiveControl = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::SetSelectedMap( PublishedFileId_t nMapIndex )
{
	// stop any current transition
	m_flTransitionStartTime = 0;
	m_flNextLoadThumbnailTime = 0;

#if !defined( NO_STEAM )
	
	// Get the information about this map
	const PublishedFileInfo_t *pFileInfo = WorkshopManager().GetPublishedFileInfoByID( (PublishedFileId_t) nMapIndex );
	Assert( pFileInfo != NULL );
	if ( pFileInfo == NULL )
		return; // FIXME: Display a broken thumbnail image!

	m_pCommunityMapInfoLabel->SetCommunityMapInfo( nMapIndex, pFileInfo->m_ulSteamIDOwner );
	// Get our avatar from Steam
	UpdateAvatarImage( pFileInfo->m_ulSteamIDOwner );

	// Updating our rating widget
	if ( m_pRatingItem )
	{
		uint32 upVotes = 0, downVotes = 0;
		float flRating = 0.0f;
		if ( pFileInfo->GetVoteData( &flRating, &upVotes, &downVotes ) )
		{
			int nTotalVotes = ( upVotes + downVotes );
			
			if ( nTotalVotes < MINIMUM_VOTE_THRESHOLD )
			{
				m_pRatingItem->SetEnabled( false );
				m_pRatingItem->SetRating( 0.0f );
			}
			else
			{
				m_pRatingItem->SetEnabled( true );
				m_pRatingItem->SetRating( flRating );
			}

			m_pRatingItem->SetVisible( true );
			
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
			m_pRatingItem->SetVisible( false );
			m_pTotalVotesLabel->SetVisible( false );
		}
	}

	// Promote this request to the top of the list so it's not stalled behind other requests
	WorkshopManager().PromoteUGCFileRequestToTop( pFileInfo->m_hPreviewFile );

	// Start the spinner and stop drawing an image until we have one
	m_nThumbnailImageId = -1;

	// we need to not spam the i/o as users scroll through save games
	// once the input activity settles, THEN we start loading the screenshot
	m_flNextLoadThumbnailTime = Plat_FloatTime() + 0.3f;

#endif // !NO_STEAM

	// Try to select the first item if none are here
	CommunityMapItem* pListItem = static_cast< CommunityMapItem* >( m_pCommunityMapList->GetSelectedPanelItem() );
	if ( pListItem == NULL )
	{
		// This will cause us to be called again and correct the issue
		m_pCommunityMapList->SelectPanelItem( 0 );
		return;
	}

	// Set active state
	for ( int i = 0; i < m_pCommunityMapList->GetPanelItemCount(); i++ )
	{
		CommunityMapItem *pItem = static_cast< CommunityMapItem* >( m_pCommunityMapList->GetPanelItem( i ) );
		if ( pItem )
		{
			pItem->SetSelected( pItem == pListItem );
		}
	}

	m_ActiveControl = pListItem;
	UpdateFooter();
}
//-----------------------------------------------------------------------------
// Purpose:	Make sure we've got something selected
//-----------------------------------------------------------------------------
void CommunityMapDialog::EnsureSelection( void )
{
	// If we haven't selected anything yet, select the first item (we don't like to be in the "nothing selected" state)
	if ( m_pCommunityMapList && m_pCommunityMapList->GetSelectedPanelItem() == NULL )
	{
		m_pCommunityMapList->SelectPanelItem( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::ScreenshotLoaded( const FileAsyncRequest_t &asyncRequest, int nNumReadBytes, FSAsyncStatus_t err )
{
	CommunityMapItem* pListItem = static_cast< CommunityMapItem* >( m_pCommunityMapList->GetSelectedPanelItem() );
	if ( !pListItem )
		return;

	int nSaveGameImageId = -1;

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
			if ( m_nMapThumbnailId == -1 )
			{
				// Create a procedural texture id
				m_nMapThumbnailId = vgui::surface()->CreateNewTextureID( true );
			}

			// Write this to the texture so we can draw it
			surface()->DrawSetTextureRGBALinear( m_nMapThumbnailId, (const unsigned char *) dstBuf.Base(), nWidth, nHeight );
			nSaveGameImageId = m_nMapThumbnailId;

			// Free our resulting image
			dstBuf.Purge();
		}
	}

	// transition into the image
	m_flTransitionStartTime = Plat_FloatTime() + 0.1f;
	m_nThumbnailImageId = nSaveGameImageId;
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CMScreenshotLoaded( const FileAsyncRequest_t &asyncRequest, int numReadBytes, FSAsyncStatus_t err )
{
	CommunityMapDialog *pDialog = static_cast< CommunityMapDialog* >( BASEMODPANEL_SINGLETON.GetWindow( WT_COMMUNITYMAP ) );
	if ( pDialog )
	{
		pDialog->ScreenshotLoaded( asyncRequest, numReadBytes, err );
	}
}	

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
bool CommunityMapDialog::StartAsyncScreenshotLoad( const char *pThumbnailFilename )
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
	if ( m_nMapThumbnailId != -1 )
	{
		// evict prior screenshot
		surface()->DestroyTextureID( m_nMapThumbnailId );
		m_nMapThumbnailId = -1;
	}

	return LoadThumbnailFromContainer( pThumbnailFilename );
}

//-----------------------------------------------------------------------------
// Purpose:	Sort the map list by download time (starting with the newest on top)
//-----------------------------------------------------------------------------
static int __cdecl CommunityMapListSortFunc( vgui::Panel* const *a, vgui::Panel* const *b)
{
	CommunityMapItem *pfA = static_cast< CommunityMapItem * >(*a);
	CommunityMapItem *pfB = static_cast< CommunityMapItem * >(*b);

	// Tie-break on our index if the times are equal
	if ( pfA->GetTimeSubscribed() == pfB->GetTimeSubscribed() )
		return ( pfA->GetCommunityMapIndex() - pfB->GetCommunityMapIndex() );

	return ( pfA->GetTimeSubscribed() - pfB->GetTimeSubscribed() );
}

//-----------------------------------------------------------------------------
// Purpose:	Sort the map list by download time (starting with the newest on top)
//-----------------------------------------------------------------------------
static int __cdecl QueueHistoryListSortFunc( vgui::Panel* const *a, vgui::Panel* const *b)
{
	CommunityMapItem *pfA = static_cast< CommunityMapItem * >(*a);
	CommunityMapItem *pfB = static_cast< CommunityMapItem * >(*b);

	// Tie-break on our index if the times are equal
	if ( pfA->GetTimeLastPlayed() == pfB->GetTimeLastPlayed() )
		return ( pfA->GetCommunityMapIndex() - pfB->GetCommunityMapIndex() );

	return ( pfB->GetTimeLastPlayed() - pfA->GetTimeLastPlayed() );
}

//-----------------------------------------------------------------------------
// Purpose:	Sort the map list by download time (starting with the newest on top)
//-----------------------------------------------------------------------------
void CommunityMapDialog::SortMapList( void )
{
	if ( IsShowingMapQueue() )
	{
		m_pCommunityMapList->SortPanelItems( CommunityMapListSortFunc );
	}
	else
	{
		m_pCommunityMapList->SortPanelItems( QueueHistoryListSortFunc );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::PopulateCommunityMapList()
{
	m_pCommunityMapList->RemoveAllPanelItems();

	if ( IsPC() )
	{
		m_pCommunityMapList->SetScrollBarVisible( false );
	}

	// Handle all of our community maps we know about
	unsigned int nNumCommunityMaps = BASEMODPANEL_SINGLETON.GetNumCommunityMapsInQueue();
	if ( nNumCommunityMaps )
	{
		for ( unsigned int i = 0; i < nNumCommunityMaps; i++ )
		{
			const PublishedFileInfo_t *pResult = BASEMODPANEL_SINGLETON.GetCommunityMap( i );
			if ( pResult == NULL )
			{
				// FIXME: Handle this error case!
				continue;
			}

			AddMapToList( pResult );
		}
	}

	// Determine if we need a scrollbar
	UpdateScrollbarState();
	UpdateFooter();

	// Start with our first item in the list
	m_pCommunityMapList->SelectPanelItem( 0, GenericPanelList::SD_DOWN, true, false, true );

	bool bReceivedBaseline = BASEMODPANEL_SINGLETON.HasReceivedQueueBaseline();

	// Setup our "no maps" label appropriately
	m_pNoMapsLabel1->SetVisible( bReceivedBaseline && ( nNumCommunityMaps == 0 ) );
	m_pListTypeButton->SetVisible( bReceivedBaseline && ( nNumCommunityMaps != 0 ) );
	m_pQuickPlayButton->SetVisible( bReceivedBaseline );
}

//-----------------------------------------------------------------------------
// Purpose:	Show the community map list
//-----------------------------------------------------------------------------
void CommunityMapDialog::PopulateQueueHistoryList( void )
{
	m_pCommunityMapList->RemoveAllPanelItems();

	if ( IsPC() )
	{
		m_pCommunityMapList->SetScrollBarVisible( false );
	}

	// Start with this off
	HideMapInfo();

	// Handle all of our community maps we know about
	unsigned int nNumCommunityMaps = BASEMODPANEL_SINGLETON.GetNumQueueHistoryEntries();
	if ( nNumCommunityMaps )
	{
		for ( unsigned int i = 0; i < nNumCommunityMaps; i++ )
		{
			const PublishedFileInfo_t *pResult = BASEMODPANEL_SINGLETON.GetQueueHistoryEntry( i );
			if ( pResult == NULL )
			{
				// FIXME: Handle this error case!
				continue;
			}

			// Don't show something in our queue history if it's also in our active queue
			if ( BASEMODPANEL_SINGLETON.GetCommunityMapByFileID( pResult->m_nPublishedFileId ) != NULL )
				continue;
			
			// Add this to our list
			AddMapToList( pResult );
		}
	}

	// Determine if we need a scrollbar
	UpdateScrollbarState();
	UpdateFooter();

	// Start with our first item in the list
	m_pCommunityMapList->SelectPanelItem( 0, GenericPanelList::SD_DOWN, true, false, true );

	// Never show this in our history view
	// TODO: Make this talk about what the history is?
	m_pNoMapsLabel1->SetVisible( false );
}

//-----------------------------------------------------------------------------
// Purpose:	Update the information about an item in our list
//-----------------------------------------------------------------------------
bool CommunityMapDialog::UpdateMapInList( CommunityMapItem *pItem, const PublishedFileInfo_t *pFileInfo )
{
	if ( pItem == NULL || pFileInfo == NULL )
		return false;

	pItem->SetCommunityMapInfo( pFileInfo->m_nPublishedFileId, pFileInfo->m_rtimeSubscribed, pFileInfo->m_rtimeLastPlayed );

	wchar_t wTitle[k_cchPublishedDocumentTitleMax];
	g_pVGuiLocalize->ConvertANSIToUnicode( pFileInfo->m_rgchTitle, wTitle, sizeof( wTitle ) );
	pItem->SetTitle( wTitle );
	
	if ( IsShowingMapQueue() )
	{
		// See if we've ever played this before
		const PublishedFileInfo_t *pHistoryInfo = BASEMODPANEL_SINGLETON.GetQueueHistoryEntryByFileID( pFileInfo->m_nPublishedFileId );
		bool bPlayed = ( pHistoryInfo != NULL );
		if ( bPlayed )
		{
			// See if we've played it since last subscribing to the file
			bPlayed = ( pHistoryInfo->m_rtimeLastPlayed > pFileInfo->m_rtimeSubscribed );
		}
		// bool bPlayed = (  != NULL );
		pItem->SetDisabled( bPlayed );
	}
	else
	{
		pItem->SetDisabled( false );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:	Add a map to our list
//	Return: Whether or not the map was actually added (some maps are rejected if they're for the wrong visible list)
//-----------------------------------------------------------------------------
bool CommunityMapDialog::AddMapToList( const PublishedFileInfo_t *pFileInfo )
{
	bool bFileInQueue = ( BASEMODPANEL_SINGLETON.GetCommunityMapByFileID( pFileInfo->m_nPublishedFileId ) != NULL );

	// If we're showing the map queue, this must be in it (i.e. queued map)
	if ( IsShowingMapQueue() && bFileInQueue == false )
		return false;

	// If we're not showing the map queue, this must also not be a member of that (i.e. history entry)
	if ( IsShowingMapQueue() == false && bFileInQueue )
		return false;

	// if map is valid for current queue mode
	if ( !BASEMODPANEL_SINGLETON.IsValidMapForCurrentQueueMode( pFileInfo ) )
		return false;

	// Add a panel to our list
	CommunityMapItem *pItem = m_pCommunityMapList->AddPanelItem< CommunityMapItem >( "CommunityMapItem" );
	return UpdateMapInList( pItem, pFileInfo );
}

//-----------------------------------------------------------------------------
// Purpose:	Remove a map from our list
//-----------------------------------------------------------------------------
void CommunityMapDialog::RemoveMapFromList( PublishedFileId_t mapID )
{
	// Only remove the map from the current list if it's for the correct list we're showing
	if ( IsShowingMapQueue() == false && BASEMODPANEL_SINGLETON.GetCommunityMapByFileID( mapID ) )
		return;

	// Remove this item from the user's list
	CommunityMapItem *pItem = m_pCommunityMapList->GetPanelByMapIndex( mapID );
	if ( pItem != NULL )
	{
		unsigned short nPanelIndex;
		if ( m_pCommunityMapList->GetPanelItemIndex( pItem, nPanelIndex ) )
		{
			m_pCommunityMapList->RemovePanelItem( nPanelIndex );
		}
	}

	// Setup our scrollbars properly
	UpdateScrollbarState();
	UpdateFooter();
}

//-----------------------------------------------------------------------------
// Purpose:	Update a map that already exists in our list, or add it if it doesn't currently exist
//-----------------------------------------------------------------------------
void CommunityMapDialog::UpdateOrAddMapToList( PublishedFileId_t mapID )
{
	// Verify we're in a good state to allow this
	Assert( m_pCommunityMapList != NULL );
	if ( m_pCommunityMapList == NULL )
		return;

	bool bSuccess = false;

	// See if we have this map in our list and nuke and re-add it if we do
	const PublishedFileInfo_t *pMapInfo = WorkshopManager().GetPublishedFileInfoByID( mapID );
	if ( pMapInfo )
	{
		// Kill this panel if it exists already and re-add it
		CommunityMapItem *pItem = m_pCommunityMapList->GetPanelByMapIndex( mapID );
		if ( pItem != NULL )
		{
			bSuccess = UpdateMapInList( pItem, pMapInfo );
		}
		else 
		{
			bSuccess = AddMapToList( pMapInfo );
		}
	}

	// Add it in like it's a new entry
	if ( bSuccess )
	{
		UpdateScrollbarState();
		UpdateFooter();
		EnsureSelection();
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::OnEvent( KeyValues *pEvent )
{
	char const *pEventName = pEvent->GetName();

	// See if the community map list has changed underneath us
	if ( !V_stricmp( "CommunityMapListener_Added", pEventName ) )
	{
		// NOTE: We'll pick this up in the published file update callback, for now, just note it for logging purposes
		PublishedFileId_t mapID = pEvent->GetUint64( "mapID", 0 );
		Log_Msg( LOG_WORKSHOP, "[VCommunityMapDialog] Map Added: %llu\n", mapID );		
	}	
	else if ( !V_stricmp( "CommunityMapListener_Removed", pEventName ) )
	{
		// Delete the map from our list
		PublishedFileId_t mapID = pEvent->GetUint64( "mapID", 0 );
		Log_Msg( LOG_WORKSHOP, "[VCommunityMapDialog] Map Removed: %llu\n", mapID );
		RemoveMapFromList( mapID );
	}
	else if ( !V_stricmp( "PublishedFileInfoListener_UpdateComplete", pEventName ) )
	{
		// A file's information has been updated, so act on that if we care to
		PublishedFileId_t mapID = pEvent->GetUint64( "fileID", 0 );
		Log_Msg( LOG_WORKSHOP, "[VCommunityMapDialog] Published File Info Updated: %llu\n", mapID );
		UpdateOrAddMapToList( mapID );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Take a map ID and return our MAP file request for it
//-----------------------------------------------------------------------------
UGCHandle_t CommunityMapDialog::GetMapFileRequestForMapID( PublishedFileId_t nMapID )
{
	// Find the map in question
	const PublishedFileInfo_t *pFileInfo = WorkshopManager().GetPublishedFileInfoByID( nMapID );
	if ( pFileInfo == NULL )
		return k_UGCHandleInvalid;

	return pFileInfo->m_hFile;
}

//-----------------------------------------------------------------------------
// Purpose:	Take a map ID and return our THUMBNAIL file request for it
//-----------------------------------------------------------------------------
UGCHandle_t CommunityMapDialog::GetThumbnailFileRequestForMapID( PublishedFileId_t nMapID )
{
	// Find the map in question
	const PublishedFileInfo_t *pFileInfo = WorkshopManager().GetPublishedFileInfoByID( nMapID );
	if ( pFileInfo == NULL )
		return k_UGCHandleInvalid;

	return pFileInfo->m_hPreviewFile;
}

//-----------------------------------------------------------------------------
// Purpose:	Turn off all information about the map
//-----------------------------------------------------------------------------
void CommunityMapDialog::HideMapInfo( void )
{
	m_pVoteInfoSpinner->SetVisible( false );
	m_pRatingItem->SetVisible( false );
	m_pTotalVotesLabel->SetVisible( false );
	m_pCommunityMapInfoLabel->SetVisible( false );
	m_pAvatarSpinner->SetVisible( false );

	ShowAuthorBadge( false );
}

#define BASELINE_TIMEOUT_THRESHOLD 30.0f	// Seconds

//-----------------------------------------------------------------------------
// Purpose:	Update all the spinners present on the page
//-----------------------------------------------------------------------------
void CommunityMapDialog::UpdateSpinners( void )
{
	// Frame for all spinner this tick
	int nAnimFrame = ( uint64 )( Plat_FloatTime() * 10 );

	// Update the queue status spinner
	if ( m_pQueueSpinner )
	{		
		m_pQueueSpinner->SetFrame( nAnimFrame );
		bool bPendingBaseline = ( IsShowingMapQueue() ) ? !BASEMODPANEL_SINGLETON.HasReceivedQueueBaseline() : !BASEMODPANEL_SINGLETON.HasReceivedQueueHistoryBaseline();
		if ( bPendingBaseline )
		{
			// If we're waiting for the baseline from Steam, make sure that we're not too far outside our timeout range
			float flRequestTime = ( IsShowingMapQueue() ) ? BASEMODPANEL_SINGLETON.GetQueueBaselineRequestTime() : BASEMODPANEL_SINGLETON.GetQueueHistoryBaselineRequestTime();
			bool bBaselineTimeoutExhausted = ( ( gpGlobals->curtime - flRequestTime ) > BASELINE_TIMEOUT_THRESHOLD );

			// Turn on our "queue update pending" indicators as appropriate
			m_pNoBaselineLabel1->SetVisible( bBaselineTimeoutExhausted );
			m_pNoBaselineLabel2->SetVisible( bBaselineTimeoutExhausted );
			m_pQueueSpinner->SetVisible( bBaselineTimeoutExhausted == false );
			
			// Hide all remaining spinners in this case
			m_pThumbnailSpinner->SetVisible( false );
			m_pVoteInfoSpinner->SetVisible( false );
			
			// Hide all the information for the map until something is valid and selected
			HideMapInfo();

			return; // Stop any other updates, we're authoritative!
		}
		else
		{
			// Remove the "queue update pending" markers
			m_pNoBaselineLabel1->SetVisible( false );
			m_pNoBaselineLabel2->SetVisible( false );
			m_pQueueSpinner->SetVisible( false );
		}
	}

	// Update the author avatar image
	if ( m_pAvatarSpinner )
	{
		m_pAvatarSpinner->SetFrame( nAnimFrame );		
	}

	// Update the thumbnail spinner
	if ( m_pThumbnailSpinner )
	{
		m_pThumbnailSpinner->SetFrame( nAnimFrame );
		bool bThumbnailInvalid = ( m_nThumbnailImageId == -1 );
		m_pThumbnailSpinner->SetVisible( bThumbnailInvalid );
	}

	// Update the queue status spinner
	if ( m_pVoteInfoSpinner )
	{	
		CommunityMapItem* pSelectedItem = static_cast< CommunityMapItem* >( m_pCommunityMapList->GetSelectedPanelItem() );
		if ( pSelectedItem == NULL )
		{
			// No currently selected item, so we can't query
			m_pVoteInfoSpinner->SetVisible( false );
		}
		else
		{
			// Get the status of what we're selecting
			bool bHasVoteDataForSelectedItem = false;
			PublishedFileId_t nFileID = pSelectedItem->GetCommunityMapIndex();
			const PublishedFileInfo_t *pInfo = WorkshopManager().GetPublishedFileInfoByID( nFileID );
			if ( pInfo != NULL )
			{
				bHasVoteDataForSelectedItem = pInfo->HasVoteData();
			}

			// Update
			m_pVoteInfoSpinner->SetVisible( bHasVoteDataForSelectedItem == false );
			m_pVoteInfoSpinner->SetFrame( nAnimFrame );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::RunFrame()
{
	BaseClass::RunFrame();

	// Update the wait screen status if we're waiting to launch a map
	if ( m_bWaitingForMapDownload && ( gpGlobals->curtime - m_flWaitScreenDelay ) > MIN_WAITSCREEN_DELAY )
	{
		if ( m_nCommunityMapToLaunch != 0 )
		{
			UGCHandle_t nPendingFileDownloadHandle = GetMapFileRequestForMapID( m_nCommunityMapToLaunch );
			if ( WorkshopManager().GetUGCFileRequestStatus( nPendingFileDownloadHandle ) == UGCFILEREQUEST_FINISHED )
			{
				// Close down the wait panel in this case
				CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
				LaunchCommunityMap( m_nCommunityMapToLaunch );
				m_nCommunityMapToLaunch = 0;
				return;
			}
		}
	}

	// Update all the state spinners on the dialog
	UpdateSpinners();
	
	// Download status spinner
	if ( m_pDownloadingSpinner && m_pDownloadingSpinner->IsVisible() )
	{
		int nAnimFrame = ( uint64 )( Plat_FloatTime() * 10 );
		m_pDownloadingSpinner->SetFrame( nAnimFrame );
		
		// See if we need to draw our download spinner
		if ( m_nThumbnailImageId >= 0 )
		{
			CommunityMapItem* pSelectedItem = static_cast< CommunityMapItem* >( m_pCommunityMapList->GetSelectedPanelItem() );
			if ( pSelectedItem )
			{
				UGCHandle_t hThumbHandle = GetThumbnailFileRequestForMapID( pSelectedItem->GetCommunityMapIndex() );
				UGCFileRequestStatus_t status = WorkshopManager().GetUGCFileRequestStatus( hThumbHandle );
				if ( status == UGCFILEREQUEST_FINISHED )
				{
					m_pDownloadingSpinner->SetVisible( false );
				}
			}
		}
		else
		{
			// We're waiting for a thumbnail image, so don't show the download spinner on top of it
			m_pDownloadingSpinner->SetVisible( false );
		}
	}

	// Handle updating our avatar
	if ( m_flAvatarRetryTime > 0 && gpGlobals->curtime < m_flAvatarRetryTime )
	{
		UpdateAvatarImage( m_ulCurrentAvatarPlayerID );
	}

	// Handle thumbnails finishing their loads
	if ( !m_flTransitionStartTime && m_flNextLoadThumbnailTime && Plat_FloatTime() >= m_flNextLoadThumbnailTime )
	{
		CommunityMapItem* pSelectedItem = static_cast< CommunityMapItem* >( m_pCommunityMapList->GetSelectedPanelItem() );
		if ( pSelectedItem )
		{
			UGCHandle_t hThumbHandle = GetThumbnailFileRequestForMapID( pSelectedItem->GetCommunityMapIndex() );
			UGCFileRequestStatus_t status = WorkshopManager().GetUGCFileRequestStatus( hThumbHandle );
			if ( status == UGCFILEREQUEST_FINISHED )
			{
				// Do not allow another screenshot request
				m_flNextLoadThumbnailTime = 0;
				m_nThumbnailImageId = -1;
				m_flTransitionStartTime = Plat_FloatTime();

				const char *lpszFilename = WorkshopManager().GetUGCFilename( hThumbHandle );
				const char *lpszDirectory = WorkshopManager().GetUGCFileDirectory( hThumbHandle );

				// Make sure we got valid data back
				Assert( lpszFilename != NULL );
				if ( lpszFilename == NULL )
					return;

				// Build the name of our new thumbnail
				char szLocalFullPath[MAX_PATH];
				V_SafeComposeFilename( lpszDirectory, lpszFilename, szLocalFullPath, ARRAYSIZE(szLocalFullPath) );
				
				// Start our load of this file
				StartAsyncScreenshotLoad( szLocalFullPath );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::MsgSaveFailure()
{
	/*
	GenericConfirmation *pConfirmation = static_cast< GenericConfirmation * >( BASEMODPANEL_SINGLETON.OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

	GenericConfirmation::Data_t data;
	data.pWindowTitle = "#PORTAL2_MsgBx_AnySaveFailure";
	data.pMessageText = "#PORTAL2_MsgBx_AnySaveFailureTxt";
	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = false;
	data.pfnOkCallback = &ConfirmSaveFailure_Callback;

	pConfirmation->SetUsageData( data );
	*/
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::MsgDeleteCompleted()
{
/*
#if defined( _X360 )
	// ensure the delete gets comitted in case the MC is yanked
	engine->FinishContainerWrites( XBX_GetPrimaryUserId() );
#endif

	DeleteSuccess();
*/
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::RequestDeleteCommunityMap( PublishedFileId_t nCommunityMapIndex )
{
	// Store this for later
	m_nCommunityMapToDelete = nCommunityMapIndex;
	DeleteAndCommit();

	/*
    GenericConfirmation *pConfirmation = static_cast< GenericConfirmation * >( BASEMODPANEL_SINGLETON.OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

	GenericConfirmation::Data_t data;
	data.pWindowTitle = "#PORTAL2_CommunityPuzzle_ConfirmPuzzleRemovalTitle";
	data.pMessageText = "#PORTAL2_CommunityPuzzle_ConfirmPuzzleRemoval";
	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = true;
	data.pfnOkCallback = &ConfirmDeleteCommunityMap_Callback;
	data.pOkButtonText = "#PORTAL2_CommunityPuzzle_RemovePuzzleFromQueue";

    pConfirmation->SetUsageData( data );
    */
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::ConfirmDeleteCommunityMap()
{
	DeleteAndCommit();
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::ConfirmDeleteCommunityMap_Callback()
{
	if ( CommunityMapDialog *pSelf = GetMyCommunityMapDialog() )
	{
		pSelf->ConfirmDeleteCommunityMap();
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::RequestOverwriteSaveGame( int nSaveGameIndex )
{
	/*
	if ( !ui_allow_delete_autosave.GetBool() )
	{
		const SaveGameInfo_t *pSaveGameInfo = GetSaveGameInfo( nSaveGameIndex );
		if ( pSaveGameInfo && ( pSaveGameInfo->m_bIsAutoSave || pSaveGameInfo->m_bIsCloudSave || pSaveGameInfo->m_bIsInCloud ) )
		{
			// not allowing user to delete an autosave
			return;
		}
	}
	*/

	/*
	m_nSaveGameToOverwrite = nSaveGameIndex;

	GenericConfirmation *pConfirmation;

	if ( nSaveGameIndex == INVALID_SAVE_GAME_INDEX )
	{
		// request to create a new save
		int numCloudSaves = 0;
		for ( int k = 0; k < m_SaveGameInfos.Count(); ++ k )
		{
			if ( m_SaveGameInfos[k].m_bIsCloudSave || m_SaveGameInfos[k].m_bIsInCloud )
				++ numCloudSaves;
		}
		if ( m_SaveGameInfos.Count() - numCloudSaves >= MAX_SAVE_SLOTS )
		{
			// not enough save slots
			pConfirmation = static_cast< GenericConfirmation * >( BASEMODPANEL_SINGLETON.OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

			GenericConfirmation::Data_t data;
			data.pWindowTitle = "#PORTAL2_MsgBx_NewSaveSlot";
			data.pMessageText = "#PORTAL2_MsgBx_NewSaveSlotMaxTxt";
			data.bOkButtonEnabled = true;

			pConfirmation->SetUsageData( data );
		}
		else
		{
			// there is no confirm
			ConfirmOverwriteSaveGame();
		}

		return;
	}

	pConfirmation = static_cast< GenericConfirmation * >( BASEMODPANEL_SINGLETON.OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

	GenericConfirmation::Data_t data;
	data.pWindowTitle = "#PORTAL2_MsgBx_ConfirmOverwriteSave";
	data.pMessageText = "#PORTAL2_MsgBx_ConfirmOverwriteSaveTxt";
	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = true;
	data.pfnOkCallback = &ConfirmOverwriteSaveGame_Callback;
	data.pOkButtonText = "#PORTAL2_ButtonAction_Save";

	pConfirmation->SetUsageData( data );
	*/
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::ConfirmOverwriteSaveGame()
{
	/*
	CUtlString savename;

	uint64 uiNumSaveGames = 0;
	for ( int iCountSaves = 0; iCountSaves < m_SaveGameInfos.Count(); ++ iCountSaves )
	{
		if ( !m_SaveGameInfos[iCountSaves].m_bIsAutoSave )
		{
			++ uiNumSaveGames;
		}
	}
	CUIGameData::Get()->GameStats_ReportAction(
		m_SaveGameInfos.IsValidIndex( m_nSaveGameToOverwrite ) ? "saveover" : "savenew",
		engine->GetLevelNameShort(), uiNumSaveGames );

	if ( m_SaveGameInfos.IsValidIndex( m_nSaveGameToOverwrite ) )
	{
		// saves have multiple .x.y extensions, need the basename
		savename = m_SaveGameInfos[m_nSaveGameToOverwrite].m_Filename.Get();		
		char *pExtension = V_stristr( savename.Get(), PLATFORM_EXT ".sav" );
		if ( pExtension )
		{
			*pExtension = '\0';
		}
	}
	else
	{
		// form a fairly unique filename, allows us to not have to check for prior existence collision
		time_t currentTime;
		time( &currentTime );
		savename = CFmtStr( "%u", currentTime );
	}

	// initiate the save
	m_bSaveStarted = CUIGameData::Get()->OpenWaitScreen( ( IsGameConsole() ? "#PORTAL2_WaitScreen_SavingGame" : "#PORTAL2_Hud_SavingGame" ), 0.0f, NULL );
	if ( m_bSaveStarted )
	{
		// no commands allowed until save fails
		EnableInput( false );

		char fullSaveFilename[MAX_PATH];
		char comment[MAX_PATH];

		m_bSaveInProgress = engine->SaveGame( 
			savename.Get(), 
			IsX360(), 
			fullSaveFilename, 
			sizeof( fullSaveFilename ),
			comment,
			sizeof( comment ) );

		// use the full savename to determin the full screenshot name
		char screenshotFilename[MAX_PATH];
		V_strncpy( screenshotFilename, fullSaveFilename, sizeof( screenshotFilename ) );
		char *pExtension = V_stristr( screenshotFilename, ".sav" );
		if ( pExtension )
		{
			// must do this special extension handling due to name.PLATFORM.sav style extensions
			// double extensions aren't handled by strtools properly
			*pExtension = '\0';
		}
		V_strncat( screenshotFilename, ".tga", sizeof( screenshotFilename ) );

		m_SaveFilename = fullSaveFilename;
		m_ThumbnailFilename = screenshotFilename;
		m_SaveComment = comment;
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::ConfirmOverwriteSaveGame_Callback()
{
	/*
	CommunityMapDialog *pSelf = static_cast< CommunityMapDialog * >( BASEMODPANEL_SINGLETON.GetWindow( WT_SAVEGAME ) );
	if ( !pSelf )
		return;

	pSelf->ConfirmOverwriteSaveGame();
	*/
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::PaintBackground()
{
	BaseClass::PaintBackground();

	DrawThumbnailImage();
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::DrawThumbnailImage()
{
	if ( !m_pThumbnailImage || m_nThumbnailImageId < 0 )
	{
		/*
		if ( m_pVoteUpLabel && m_pVoteUpLabel->IsVisible() )
		{
			m_pVoteUpLabel->SetVisible( false );
		}
		
		if ( m_pVoteDownLabel && m_pVoteDownLabel->IsVisible() )
		{
			m_pVoteDownLabel->SetVisible( false );
		}
		*/
		
		// Don't draw anything else in this frame
		return;
	}

	int x, y, wide, tall;
	m_pThumbnailImage->GetBounds( x, y, wide, tall );

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

	// Draw a black border around the image
	surface()->DrawSetColor( Color( 0, 0, 0, flLerp * 255.0f ) );
	surface()->DrawOutlinedRect( x-1, y-1, x+wide+1, y+tall+1 );

	// Draw the thumbnail image
	surface()->DrawSetColor( Color( 255, 255, 255, flLerp * 255.0f ) );
	surface()->DrawSetTexture( m_nThumbnailImageId );
	surface()->DrawTexturedRect( x, y, x+wide, y+tall );

	// Fade up anything coming with us
	if ( flLerp > 0.0f && flLerp < 1.0f )
	{
		/*
		m_pVoteUpLabel->SetVisible( true );
		m_pVoteUpLabel->SetFgColor( Color( 255, 255, 255, flLerp * 255.0f ) );
		
		m_pVoteDownLabel->SetVisible( true );
		m_pVoteDownLabel->SetFgColor( Color( 255, 255, 255, flLerp * 255.0f ) );
		*/
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::MsgPS3AsyncSystemReady()
{
/*
#ifdef _PS3
	DevMsg( "MsgPS3AsyncSystemReady(): PendingOperation: %d\n", m_PS3AsyncStatus.m_PendingOperation );

	PS3AsyncOperation_e nPendingOperation = m_PS3AsyncStatus.m_PendingOperation;
	if ( nPendingOperation == PS3_ASYNC_OP_NONE )
	{
		// nothing to do, possibly canceled
		return;
	}

	if ( ps3saveuiapi->IsSaveUtilBusy() || !m_PS3AsyncStatus.JobDone() )
	{
		// async system busy, defer operation and try again next frame
		m_PS3AsyncStatus.m_bPendingOperationActive = true;
		return;
	}

	m_PS3AsyncStatus.m_PendingOperation = PS3_ASYNC_OP_NONE;

	switch ( nPendingOperation )
	{
	case PS3_ASYNC_OP_READ_SAVE:
		m_PS3AsyncStatus.StartOperation( PS3_ASYNC_OP_READ_SAVE );
		ps3saveuiapi->Load( &m_PS3AsyncStatus, V_GetFileName( m_LoadFilename.Get() ), m_LoadFilename.Get() );
		break;

	case PS3_ASYNC_OP_WRITE_SAVE:
		{
			m_PS3AsyncStatus.StartOperation( PS3_ASYNC_OP_WRITE_SAVE );

			// pre-qualify existence of screenshot, avoid ps3 save error about non-existence
			const char *pScreenshotFilename = NULL;
			if ( !m_ThumbnailFilename.IsEmpty() && g_pFullFileSystem->FileExists( m_ThumbnailFilename.Get() ) )
			{
				pScreenshotFilename = m_ThumbnailFilename.Get();
			}
			ps3saveuiapi->Write( &m_PS3AsyncStatus, m_SaveFilename.Get(), pScreenshotFilename, m_SaveComment.Get(), IPS3SaveRestoreToUI::kSECURE );
		}
		break;

	case PS3_ASYNC_OP_DELETE_SAVE:
		m_PS3AsyncStatus.StartOperation( PS3_ASYNC_OP_DELETE_SAVE );
		ps3saveuiapi->Delete( &m_PS3AsyncStatus, V_GetFileName( m_DeleteFilename.Get() ), V_GetFileName( m_ThumbnailFilename.Get() ) );
		break;

	case PS3_ASYNC_OP_READ_SCREENSHOT:
		m_PS3AsyncStatus.StartOperation( PS3_ASYNC_OP_READ_SCREENSHOT );
		ps3saveuiapi->Load( &m_PS3AsyncStatus, V_GetFileName( m_ThumbnailFilename.Get() ), m_ThumbnailFilename.Get() );
		break;
	}
#endif
	*/
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::MsgPS3AsyncOperationFailure()
{
	/*
#ifdef _PS3
	DevMsg( "MsgPS3AsyncOperationFailure(): Operation:%d, SonyRetVal:%d\n", m_PS3AsyncStatus.m_AsyncOperation, m_PS3AsyncStatus.GetSonyReturnValue() );

	m_PS3AsyncStatus.m_bAsyncOperationComplete = true;

	// failure, open a confirmation
	GenericConfirmation::Data_t data;
	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = false;
	data.pfnOkCallback = &ConfirmSaveFailure_Callback;

	int nError = m_PS3AsyncStatus.GetSonyReturnValue();

	switch ( m_PS3AsyncStatus.m_AsyncOperation )
	{
	case PS3_ASYNC_OP_DELETE_SAVE:
		data.pWindowTitle = "#PORTAL2_MsgBx_DeleteFailure";
		data.pMessageText = "#PORTAL2_MsgBx_DeleteFailureTxt";
		break;

	case PS3_ASYNC_OP_WRITE_SAVE:
		data.pWindowTitle = "#PORTAL2_MsgBx_SaveFailure";
		if ( nError == CELL_SAVEDATA_ERROR_NOSPACE || nError == CELL_SAVEDATA_ERROR_SIZEOVER )
		{
			data.pMessageText = "#PORTAL2_MsgBx_SaveMoreSpacePleaseDelete";
		}
		else
		{
			data.pMessageText = "#PORTAL2_MsgBx_SaveFailureTxt";
		}
		break;

	case PS3_ASYNC_OP_READ_SAVE:
		data.pWindowTitle = "#PORTAL2_MsgBx_LoadFailure";
		data.pMessageText = "#PORTAL2_MsgBx_LoadFailureTxt";
		break;

	case PS3_ASYNC_OP_READ_SCREENSHOT:
		// silent failure - this will do the right thing and handle the error as a missing screenshot
		LoadThumbnailFromContainerSuccess();
		return;
	}
	
	GenericConfirmation *pConfirmation = static_cast< GenericConfirmation * >( BASEMODPANEL_SINGLETON.OpenWindow( WT_GENERICCONFIRMATION, this, false ) );
	pConfirmation->SetUsageData( data );
#endif
	*/
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::MsgPS3AsyncOperationComplete()
{
/*
#ifdef _PS3
	DevMsg( "MsgPS3AsyncOperationComplete(): Operation:%d, SonyRetVal:%d\n", m_PS3AsyncStatus.m_AsyncOperation, m_PS3AsyncStatus.GetSonyReturnValue() );

	int nError = m_PS3AsyncStatus.GetSonyReturnValue();

	if ( m_PS3AsyncStatus.m_AsyncOperation == PS3_ASYNC_OP_WRITE_SAVE )
	{
		if ( nError != CELL_SAVEDATA_RET_OK )
		{
			// in case of failure delete the local save
			g_pFullFileSystem->RemoveFile( m_SaveFilename.Get() );
		}
		else
		{
			// in case of success rename the local save file
			// this avoids any growth problems with large saves filling the local cache
			// we always re-get the save from the container before loading
			char ps3Filename[MAX_PATH];
			RenamePS3SaveGameFile( m_SaveFilename.Get(), ps3Filename, sizeof( ps3Filename ) );
		}
	}

	if ( nError != CELL_SAVEDATA_RET_OK )
	{
		if ( m_PS3AsyncStatus.m_AsyncOperation == PS3_ASYNC_OP_READ_SCREENSHOT )
		{
			MsgPS3AsyncOperationFailure();
		}
		else
		{
			// failure, transition to confirmation
			if ( !CUIGameData::Get()->CloseWaitScreen( this, "MsgPS3AsyncOperationFailure" ) )
			{
				PostMessage( this, new KeyValues( "MsgPS3AsyncOperationFailure" ) );
			}
		}
		return;
	}

	m_PS3AsyncStatus.m_bAsyncOperationComplete = true;

	switch ( m_PS3AsyncStatus.m_AsyncOperation )
	{
	case PS3_ASYNC_OP_DELETE_SAVE:
		CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
		DeleteSuccess();
		break;

	case PS3_ASYNC_OP_WRITE_SAVE:
		CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
		WriteSaveGameToContainerSuccess();
		break;

	case PS3_ASYNC_OP_READ_SAVE:
		CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
		LoadSaveGameFromContainerSuccess();
		break;

	case PS3_ASYNC_OP_READ_SCREENSHOT:
		LoadThumbnailFromContainerSuccess();
		break;
	}
#endif
	*/
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::DeleteSuccess()
{
	// repopulate
	// Reset();
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::DeleteAndCommit( void )
{
#if !defined( NO_STEAM )
	
	// FIXME: Don't nuke the files just yet, we need a more complete solution for that

	// Clear map from disk
	UGCHandle_t hMapHandle = GetMapFileRequestForMapID( m_nCommunityMapToDelete );
	WorkshopManager().DeleteUGCFileRequest( hMapHandle, true );

	// Clear the thumbnail
	UGCHandle_t hThumbHandle = GetThumbnailFileRequestForMapID( m_nCommunityMapToDelete );
	WorkshopManager().DeleteUGCFileRequest( hThumbHandle, true );

	// NOTE: We expect to get a return message from the base panel which will cause us to actually remove the map from our list!

	// Unsubscribe from this file now
	BASEMODPANEL_SINGLETON.UnsubscribeFromMap( m_nCommunityMapToDelete );
#endif // !NO_STEAM

}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::WriteSaveGameToContainerSuccess()
{
	/*
	CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_MsgBx_SaveCompletedTxt", 1.0f, NULL );
	if ( !CUIGameData::Get()->CloseWaitScreen( this, "MsgReturnToGame" ) )
	{
		PostMessage( this, new KeyValues( "MsgReturnToGame" ) );
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::WriteSaveGameToContainer()
{
/*
#ifdef _PS3
	CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_WaitScreen_SavingGame", 0.0f, NULL );
	StartPS3Operation( PS3_ASYNC_OP_WRITE_SAVE );
#else
	WriteSaveGameToContainerSuccess();
#endif
*/
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::LoadSaveGameFromContainerSuccess()
{
	/*
	const char *pFilenameToLoad = m_LoadFilename.Get();

#ifdef _PS3
	char ps3Filename[MAX_PATH];
	pFilenameToLoad = RenamePS3SaveGameFile( pFilenameToLoad, ps3Filename, sizeof( ps3Filename ) );
#endif

	if ( BASEMODPANEL_SINGLETON.GetActiveWindowType() == GetWindowType() )
	{
		KeyValues *pSettings = new KeyValues( "FadeOutStartGame" );
		KeyValues::AutoDelete autodelete_pSettings( pSettings );
		pSettings->SetString( "map", m_MapName.Get() );
		pSettings->SetString( "loadfilename", pFilenameToLoad );
		pSettings->SetString( "reason", "load" );
		BASEMODPANEL_SINGLETON.OpenWindow( WT_FADEOUTSTARTGAME, this, true, pSettings );
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::LoadSaveGameFromContainer( const char *pMapName, const char *pFilename )
{
	/*
	m_MapName = pMapName;
	m_LoadFilename = pFilename;

#ifdef _PS3
	if ( CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_WaitScreen_LoadingGame", 0.0f, NULL ) )
	{
		// establish the full filename at the expected target
		char fullFilename[MAX_PATH];
		V_ComposeFileName( engine->GetSaveDirName(), (char *)V_GetFileName( pFilename ), fullFilename, sizeof( fullFilename ) );
		m_LoadFilename = fullFilename;

		StartPS3Operation( PS3_ASYNC_OP_READ_SAVE );
	}
#else
	LoadSaveGameFromContainerSuccess();
#endif
	*/
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
bool CommunityMapDialog::LoadThumbnailFromContainer( const char *pThumbnailFilename )
{
/*
#ifdef _PS3
	// no need to load from container if screenshot already cached
	bool bIsScreenshotCached = g_pFullFileSystem->FileExists( pScreenshotFilename, "MOD" );
	if ( !bIsScreenshotCached && 
		( ps3saveuiapi->IsSaveUtilBusy() || m_PS3AsyncStatus.IsOperationPending() || !m_PS3AsyncStatus.IsOperationComplete() ) )
	{
		// busy, screenshots don't queue
		// caller wired for retry
		return false;
	}
#endif
*/
	m_ThumbnailFilename = pThumbnailFilename;

/*
#ifdef _PS3
	if ( !bIsScreenshotCached )
	{
		StartPS3Operation( PS3_ASYNC_OP_READ_SCREENSHOT );
	}
	else
	{
		// already have it locally
		LoadScreenshotFromContainerSuccess();
	}
#else
*/
	LoadThumbnailFromContainerSuccess();
// #endif

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:	Read the thumbnail off our storage device, async
//-----------------------------------------------------------------------------
void CommunityMapDialog::LoadThumbnailFromContainerSuccess()
{
	FileAsyncRequest_t request;
	request.pszFilename = m_ThumbnailFilename.Get();
	request.pfnCallback = ::CMScreenshotLoaded;
	request.pContext = NULL;
	request.flags = FSASYNC_FLAGS_FREEDATAPTR;

	// schedule the async operation
	g_pFullFileSystem->AsyncRead( request, &m_hAsyncControl );	
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
#ifdef _PS3
void CommunityMapDialog::StartPS3Operation( PS3AsyncOperation_e nOperation )
{
	/*
	m_PS3AsyncStatus.m_PendingOperation = nOperation;
	m_PS3AsyncStatus.m_bPendingOperationActive = true;
	*/
}
#endif

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CommunityMapDialog::ConfirmSaveFailure_Callback()
{
	/*
	CommunityMapDialog *pSelf = static_cast< CommunityMapDialog * >( BASEMODPANEL_SINGLETON.GetWindow( WT_SAVEGAME ) );
	if ( !pSelf )
		return;

	pSelf->EnableInput( true );
	*/
}

}; // namespace BaseModUI

//-----------------------------------------------------------------------------
// Purpose: Launch the map, once complete
//-----------------------------------------------------------------------------
void CommunityMapDialog::LaunchCommunityMap( PublishedFileId_t nMapID )
{
	UGCHandle_t hMapHandle = GetMapFileRequestForMapID( nMapID );
	Assert( hMapHandle != k_UGCHandleInvalid );
	if ( hMapHandle == k_UGCHandleInvalid )
		return;

	const char *lpszFilename = WorkshopManager().GetUGCFilename( hMapHandle );
	const char *lpszDirectory = WorkshopManager().GetUGCFileDirectory( hMapHandle );
	
	Assert( lpszFilename != NULL );
	if ( lpszFilename == NULL )
		return;
	
	char szFilenameNoExtension[MAX_PATH];
	Q_FileBase( lpszFilename, szFilenameNoExtension, sizeof(szFilenameNoExtension) );

	char szMapName[MAX_PATH];
	V_SafeComposeFilename( lpszDirectory, szFilenameNoExtension, szMapName, sizeof(szMapName) );
	
	// Move past the "maps" folder, it's implied by the following call to load
	const char *lpszUnbasedDirectory = V_strnchr( szMapName, CORRECT_PATH_SEPARATOR, sizeof(szMapName) );
	lpszUnbasedDirectory++; // Move past the actual path separator character

	// Save this for later reference
	BASEMODPANEL_SINGLETON.SetCurrentCommunityMapID( nMapID );
	
	// Don't draw us behind this window when it launches!
	BASEMODPANEL_SINGLETON.CloseAllWindows();

	KeyValues *pSettings = new KeyValues( "FadeOutStartGame" );
	KeyValues::AutoDelete autodelete_pSettings( pSettings );
	pSettings->SetString( "map", lpszUnbasedDirectory );
	pSettings->SetString( "reason", "newgame" );
	BASEMODPANEL_SINGLETON.OpenWindow( WT_FADEOUTSTARTGAME, this, true, pSettings );

	m_nCommunityMapToLaunch = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CommunityMapDialog::BeginLaunchCommunityMap( PublishedFileId_t nMapID, bool bRecordDemo /*= false*/ )
{
	// TODO: If we're not done downloading at this point, we need to stall!
	UGCHandle_t hMapHandle = GetMapFileRequestForMapID( nMapID );
	if ( hMapHandle == k_UGCHandleInvalid )
		return;

	// Hold this so we remember what we're doing
	m_nCommunityMapToLaunch = nMapID;

	UGCFileRequestStatus_t status = WorkshopManager().GetUGCFileRequestStatus( hMapHandle );
	if ( status != UGCFILEREQUEST_FINISHED )
	{
		// If there's an error, tell the user
		if ( status == UGCFILEREQUEST_ERROR )
		{
			BASEMODPANEL_SINGLETON.OpenMessageDialog( "Error", "An error occurred while downloading this map!" );
			return;
		}

		// If the download hasn't started yet, then we need to kick it off
		if ( status == UGCFILEREQUEST_READY || status == UGCFILEREQUEST_INVALID )
		{
			const PublishedFileInfo_t *fileInfo = WorkshopManager().GetPublishedFileInfoByID( nMapID );
			BASEMODPANEL_SINGLETON.CreateMapFileRequest( *fileInfo );
		}

		// Move this query to the top of the list
		WorkshopManager().PromoteUGCFileRequestToTop( hMapHandle );

		// Throw up a "waiting for file download" wait screen
		KeyValues *pSettings = new KeyValues( "WaitScreen" );
		KeyValues::AutoDelete autodelete_pSettings( pSettings );
		pSettings->SetPtr( "options/asyncoperation", g_CommunityMaps_WaitForDownloadOperation.Prepare() );
		pSettings->SetUint64( "options/filehandle", hMapHandle );

		// We're still downloading, so stall while it works
		m_bWaitingForMapDownload = CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_CommunityPuzzle_WaitForFileDownload", 1.0f, pSettings );

		m_flWaitScreenDelay = gpGlobals->curtime;
		return;
	}

	// We're ready, go!
	LaunchCommunityMap( m_nCommunityMapToLaunch );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CommunityMapDialog::BeginLaunchCoopCommunityMap( PublishedFileId_t nCommunityMapID )
{
	BASEMODPANEL_SINGLETON.SetCurrentCommunityMapID( nCommunityMapID );
	CUIGameData::Get()->InitiateOnlineCoopPlay( NULL, "playonline", "coop_community", g_CommunityCoopManager.GetCommunityMapName( nCommunityMapID ) );
}


//-----------------------------------------------------------------------------
// Purpose: Update the user's avatar image, based on a Steam ID
//-----------------------------------------------------------------------------
void CommunityMapDialog::UpdateScrollbarState( void )
{
	// Setup our scrollbar visibility on PC
	if ( m_pCommunityMapList && m_pCommunityMapList->GetPanelItemCount() )
	{
		// Always valid to hide these
		if ( m_pNoMapsLabel1 )
		{
			m_pNoMapsLabel1->SetVisible( false );
		}
		
		if ( m_pListTypeButton )
		{
			m_pListTypeButton->SetVisible( true );
		}
		
		if ( m_pQuickPlayButton ) 
		{
			m_pQuickPlayButton->SetVisible( true );
		}
		
		// auto-hide scroll bar when not necessary
		if ( IsPC() )
		{
			CommunityMapItem *pItem = static_cast< CommunityMapItem* >( m_pCommunityMapList->GetPanelItem( 0 ) );
			if ( pItem )
			{
				int nPanelItemTall = pItem->GetTall();
				if ( nPanelItemTall )
				{
					int nNumVisibleItems = m_pCommunityMapList->GetTall()/nPanelItemTall;
					bool bScrollBarNecessary = ( m_pCommunityMapList->GetPanelItemCount() > nNumVisibleItems );
					m_pCommunityMapList->SetScrollBarVisible( bScrollBarNecessary ); 
				}
			}
		}

		// Sort the list by the subscription event timestamp
		SortMapList();
	}
	else
	{	
		// Only show a call to action if this is the map queue (not history)
		bool bPendingBaseline = ( IsShowingMapQueue() ) ? !BASEMODPANEL_SINGLETON.HasReceivedQueueBaseline() : !BASEMODPANEL_SINGLETON.HasReceivedQueueHistoryBaseline();
		if ( bPendingBaseline == false && IsShowingMapQueue() )
		{
			// Tell the user they don't have any maps
			if ( m_pNoMapsLabel1 )
			{
				m_pNoMapsLabel1->SetVisible( true );
			}

			if ( m_pListTypeButton )
			{
				m_pListTypeButton->SetVisible( false );
			}

			if ( m_pQuickPlayButton )
			{
				m_pQuickPlayButton->SetVisible( false );
			}
			
			// Hide all the map information
			HideMapInfo();
		}

		m_nThumbnailImageId = -2;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Show or hide the author badge decorations
//-----------------------------------------------------------------------------
void CommunityMapDialog::ShowAuthorBadge( bool bVisible )
{
	m_pAuthorAvatarImage->SetVisible( bVisible );
	m_pAuthorBadge->SetVisible( bVisible );
	m_pAuthorBadgeOverlay->SetVisible( bVisible );
	// m_pAuthorBadgeAward->SetVisible( bVisible );
	m_pAuthorBadgeLogo->SetVisible( bVisible );
}

//-----------------------------------------------------------------------------
// Purpose: Update the user's avatar image, based on a Steam ID
//-----------------------------------------------------------------------------
void CommunityMapDialog::UpdateAvatarImage( uint64 ulSteamID )
{
	Assert( m_pAuthorAvatarImage );
	if ( m_pAuthorAvatarImage == NULL )
		return;

	uint64 oldAvatar = m_ulCurrentAvatarPlayerID;
	m_ulCurrentAvatarPlayerID = ulSteamID;

	vgui::IImage *pImage = BaseModUI::CUIGameData::Get()->AccessAvatarImage( ulSteamID, BaseModUI::CUIGameData::kAvatarImageRequest );
	if ( pImage != NULL )
	{
		m_pAuthorAvatarImage->SetImage( pImage );
		ShowAuthorBadge( true );
		m_pAvatarSpinner->SetVisible( false );
		m_flAvatarRetryTime = -1;
	}
	else
	{
		m_flAvatarRetryTime = gpGlobals->curtime + 1.0f;
		m_pAvatarSpinner->SetVisible( true );
	}

	// Release the image if it's no longer valid for us
	if ( oldAvatar != 0 && oldAvatar != m_ulCurrentAvatarPlayerID )
	{
		BaseModUI::CUIGameData::Get()->AccessAvatarImage( oldAvatar, BaseModUI::CUIGameData::kAvatarImageRelease );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Callback for a persona changing
//-----------------------------------------------------------------------------
void CommunityMapDialog::MapDownloadAborted( const char *msg )
{
	// Stop waiting for the download
	m_bWaitingForMapDownload = false;
	m_nCommunityMapToLaunch = 0;
	m_flWaitScreenDelay = 0;

	// Stop our wait screen
	CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Callback for a persona changing
//-----------------------------------------------------------------------------

#if !defined( NO_STEAM )
void CommunityMapDialog::Steam_OnPersonaUpdated( PersonaStateChange_t *pPersonaStateChanged )
{
	// If we're showing this user's information, update it
	if ( m_pCommunityMapInfoLabel->GetOwnerID() == pPersonaStateChanged->m_ulSteamID )
	{
		m_pCommunityMapInfoLabel->Update();
		UpdateAvatarImage( pPersonaStateChanged->m_ulSteamID );
	}
}
#endif // !NO_STEAM

#endif // PORTAL2_PUZZLEMAKER
