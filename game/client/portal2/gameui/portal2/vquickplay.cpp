//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//============================================================================//

#include "cbase.h"

#include "vquickplay.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "vgenericwaitscreen.h"
#include "VGenericConfirmation.h"
#include "c_community_coop.h"
#include "vgui/ISurface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define MIN_WAITSCREEN_WAIT				1.0f
#define MAX_ENUMERATION_WAIT_TIME		90.0f

using namespace vgui;
using namespace BaseModUI;

extern ConVar cm_community_debug_spew;
extern Color rgbaCommunityDebug;

// ============================================================================
//
//  Handles waiting for the quickplay enumeration to complete
//
// ============================================================================

class CQuickPlay_WaitForEnumerationOperation : public IMatchAsyncOperation
{
public:
	virtual bool IsFinished() { return false; }
	virtual AsyncOperationState_t GetState() { return AOS_RUNNING; }
	virtual uint64 GetResult() { return 0ull; }
	virtual void Release() { Assert( 0 ); }
	virtual void Abort()
	{
		CQuickPlay *pQuickPlayDialog = (CQuickPlay *) BASEMODPANEL_SINGLETON.GetWindow( WT_QUICKPLAY );
		if ( !pQuickPlayDialog )
			return;

		pQuickPlayDialog->PostMessage( pQuickPlayDialog, new KeyValues( "FindQuickPlayMapsAborted", "msg", "" ) );
	}

public:
	CQuickPlay_WaitForEnumerationOperation() {}
	IMatchAsyncOperation * Prepare() { return this; }
}
g_QuickPlay_WaitForEnumerationOperation;

// ============================================================================
//
//  Checks state of enumeration to ensure that results have come back
//
// ============================================================================
class CQuickPlay_QueryQuickPlayMapsCallback : public IWaitscreenCallbackInterface
{
public:
	
	CQuickPlay_QueryQuickPlayMapsCallback( void ) : m_bFinished( false ) { }

	virtual void OnThink()
	{
		if ( m_bFinished ) 
			return;

		if( BASEMODPANEL_SINGLETON.QuickPlayEntriesReady() )
		{
			if ( BASEMODPANEL_SINGLETON.GetNextQuickPlayMapInQueue() )
			{
				// Handle the error case
				CQuickPlay *pQuickPlayDialog = (CQuickPlay *) BASEMODPANEL_SINGLETON.GetWindow( WT_QUICKPLAY );
				if ( !pQuickPlayDialog )
					return;

				pQuickPlayDialog->PostMessage( pQuickPlayDialog, new KeyValues( "FindQuickPlayMapsComplete", "msg", "" ) );
				m_bFinished = true;
			}
			else if ( BASEMODPANEL_SINGLETON.GetNumQuickPlayEntries() == 0 )
			{
				m_bFinished = true;

				// An error occurred we should message to the user
				CQuickPlay *pQuickPlayDialog = (CQuickPlay *) BASEMODPANEL_SINGLETON.GetWindow( WT_QUICKPLAY );
				if ( !pQuickPlayDialog )
				{
					g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "error", "error", "nopuzzle" ) );
				}
				else
				{
					pQuickPlayDialog->PostMessage( pQuickPlayDialog, new KeyValues( "FindQuickPlayMapsFailed", "msg", "" ) );
				}
				
			}
		}
		else if ( BASEMODPANEL_SINGLETON.QuickPlayEntriesError() )
		{
			// An error occurred we should message to the user
			CQuickPlay *pQuickPlayDialog = (CQuickPlay *) BASEMODPANEL_SINGLETON.GetWindow( WT_QUICKPLAY );
			if ( !pQuickPlayDialog )
				return;

			pQuickPlayDialog->PostMessage( pQuickPlayDialog, new KeyValues( "FindQuickPlayMapsError", "msg", "" ) );
			m_bFinished = true;
		}
	}

	IWaitscreenCallbackInterface *Prepare( void ) 
	{ 
		m_bFinished = false; 
		return this; 
	}

private:
	bool	m_bFinished;
}
g_QuickPlay_QueryQuickPlayMapsCallback;

//=============================================================================
//
//  Enumeration types
//
//=============================================================================

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
EnumerationTypeItem::EnumerationTypeItem( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName )
{
	m_pListCtrlr = dynamic_cast< GenericPanelList * >( pParent );

	SetProportional( true );
	SetPaintBackgroundEnabled( true );

	m_TitleString[0] = '\0';

	m_hTitleFont = vgui::INVALID_FONT;

	m_nTextOffsetY = 0;

	m_bSelected = false;
	m_bHasMouseover = false;
	m_bDisabled = false;
	m_EnumerationType = k_EWorkshopEnumerationTypeRankedByVote;
}

//-----------------------------------------------------------------------------
// Purpose:	Set the visible title for this entry
//-----------------------------------------------------------------------------
void EnumerationTypeItem::SetTitle( const wchar_t *lpszTitle )
{
	V_wcsncpy( m_TitleString, lpszTitle, sizeof( m_TitleString ) );
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void EnumerationTypeItem::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/BaseModUI/QuickPlayItem.res" );

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
void EnumerationTypeItem::PaintBackground()
{
	bool bHasFocus = HasFocus() || IsSelected();

	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );

	// if we're highlighted, background
	if ( HasMouseover() )
	{
		vgui::surface()->DrawSetColor( m_MouseOverCursorColor );
		vgui::surface()->DrawFilledRect( 0, 0, wide, tall );
	}
	else if ( bHasFocus )
	{
		vgui::surface()->DrawSetColor( m_CursorColor );
		vgui::surface()->DrawFilledRect( 0, 0, wide, tall );
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
int	EnumerationTypeItem::DrawText( int x, int y, int nLabelTall, const wchar_t *pString, vgui::HFont hFont, Color color )
{
	int len = V_wcslen( pString );

	int textWide, textTall;
	vgui::surface()->GetTextSize( hFont, pString, textWide, textTall );

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
void EnumerationTypeItem::OnCursorEntered()
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
void EnumerationTypeItem::OnCursorExited() 
{
	SetHasMouseover( false ); 
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void EnumerationTypeItem::NavigateTo()
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
void EnumerationTypeItem::NavigateFrom()
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
void EnumerationTypeItem::OnKeyCodePressed( vgui::KeyCode code )
{
	// if ( !m_pDialog->IsInputEnabled() )
	//	return;

	int iUserSlot = GetJoystickForCode( code );
	BASEMODPANEL_SINGLETON.SetLastActiveUserId( iUserSlot );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_A:
	case KEY_ENTER:
		{
			EnumerationTypeItem* pListItem = static_cast< EnumerationTypeItem* >( m_pListCtrlr->GetSelectedPanelItem() );
			if ( pListItem )
			{
				CQuickPlay *pQuickPlayDialog = (CQuickPlay *) BASEMODPANEL_SINGLETON.GetWindow( WT_QUICKPLAY );
				if ( !pQuickPlayDialog )
					return;

				PostMessage( pQuickPlayDialog, new KeyValues( "LaunchRequested", "type", m_EnumerationType ) );
			}
		}
		return;
	}

	BaseClass::OnKeyCodePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void EnumerationTypeItem::OnMousePressed( vgui::MouseCode code )
{
	if ( code == MOUSE_LEFT )
	{
		if ( GetParent() )
			GetParent()->NavigateToChild( this );
		else
			NavigateTo();

		CQuickPlay *pQuickPlayDialog = (CQuickPlay *) BASEMODPANEL_SINGLETON.GetWindow( WT_QUICKPLAY );
		if ( !pQuickPlayDialog )
			
			return;
		PostMessage( pQuickPlayDialog, new KeyValues( "EnumerationTypeSelected", "type", m_EnumerationType ) );
		return;
	}

	BaseClass::OnMousePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void EnumerationTypeItem::OnMouseDoublePressed( vgui::MouseCode code )
{
	if ( code == MOUSE_LEFT )
	{
		EnumerationTypeItem* pListItem = static_cast< EnumerationTypeItem* >( m_pListCtrlr->GetSelectedPanelItem() );
		if ( pListItem )
		{
			OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, BASEMODPANEL_SINGLETON.GetLastActiveUserId() ) );
		}
	}

	BaseClass::OnMouseDoublePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void EnumerationTypeItem::PerformLayout()
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
void EnumerationTypeItem::OnMessage( const KeyValues *params, vgui::VPANEL ifromPanel )
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

// Map of enumeration types to their respective titles
static char *g_pszQuickPlayTitle[NUM_QUICKPLAY_ENUMERATION_TYPES] = 
{	
	"PORTAL2_QuickPlay_HighestRated",
	"PORTAL2_QuickPlay_MostRecent",
	"PORTAL2_QuickPlay_MostPopular",
	"PORTAL2_QuickPlay_Friends_Favorites",
	"PORTAL2_QuickPlay_Friends_Rated",
	"PORTAL2_QuickPlay_Friends_Creations",
	"PORTAL2_QuickPlay_Followers_Recent"
};

// Map of enumeration types to their respective descriptions
static char *g_pszQuickPlayDescriptions[NUM_QUICKPLAY_ENUMERATION_TYPES] = 
{	
	"#PORTAL2_QuickPlayDesc_HighestRated",
	"#PORTAL2_QuickPlayDesc_MostRecent",
	"#PORTAL2_QuickPlayDesc_MostPopular",
	"#PORTAL2_QuickPlayDesc_Friends_Favorites",
	"#PORTAL2_QuickPlayDesc_Friends_Rated",
	"#PORTAL2_QuickPlayDesc_Friends_Creations",
	"#PORTAL2_QuickPlayDesc_Followers_Recent"
};

// Map of enumeration types to their respective descriptions
static char *g_pszQuickPlayIcons[NUM_QUICKPLAY_ENUMERATION_TYPES] = 
{	
	"quick_toprated",
	"quick_recent",
	"quick_popular",
	"quick_friendsfavorites",
	"quick_friendstoprated",
	"quick_friendscreations",
	"quick_followers"
};

// Precache our glow effects
PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheQuickPlayIcons )
	PRECACHE( MATERIAL, "vgui/quick_toprated" )
	PRECACHE( MATERIAL, "vgui/quick_recent" )
	PRECACHE( MATERIAL, "vgui/quick_popular" )
	PRECACHE( MATERIAL, "vgui/quick_friendsfavorites" )
	PRECACHE( MATERIAL, "vgui/quick_friendstoprated" )
	PRECACHE( MATERIAL, "vgui/quick_friendscreations" )
	PRECACHE( MATERIAL, "vgui/quick_followers" )
PRECACHE_REGISTER_END()

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CQuickPlay::CQuickPlay( Panel *parent, const char *panelName ) :
	BaseClass( parent, panelName ),
	m_eState( IDLE ),
	m_flStartupTime( 0.0f ),
	m_pQuickPlayIcon( NULL )
{
	// allows us to get RunFrame() during wait screen occlusion
	AddFrameListener( this );

	SetDeleteSelfOnClose( true );
	SetProportional( true );

	SetDialogTitle( "#GameUI_QuickPlay" );

	SetFooterEnabled( true );
	UpdateFooter();

#if !defined( _GAMECONSOLE )
	m_pEnumerationTypesList = new GenericPanelList( this, "EnumerationTypeList", GenericPanelList::ISM_PERITEM | GenericPanelList::ISM_ALPHA_INVISIBLE );
	m_pEnumerationTypesList->SetPaintBackgroundEnabled( false );	
#endif // !_GAMECONSOLE
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CQuickPlay::~CQuickPlay()
{
	RemoveFrameListener( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuickPlay::Activate( void )
{
	BaseClass::Activate();
	UpdateFooter();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuickPlay::OnKeyCodePressed(KeyCode code)
{
	int joystick = GetJoystickForCode( code );
	int userId = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	if ( joystick != userId || joystick < 0 )
	{	
		return;
	}

	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_A:
		StartEnumeration();
		BaseClass::OnKeyCodePressed(code);
		break;

	case KEY_XBUTTON_B:
		BASEMODPANEL_SINGLETON.SetCommunityMapQueueMode( ( BASEMODPANEL_SINGLETON.GetCommunityMapQueueMode() == QUEUEMODE_QUICK_PLAY ) ? QUEUEMODE_USER_QUEUE : QUEUEMODE_USER_COOP_QUEUE );
		BaseClass::OnKeyCodePressed(code);
		break;

	default:
		BaseClass::OnKeyCodePressed(code);
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CQuickPlay::StartEnumeration( void )
{
	if ( BASEMODPANEL_SINGLETON.GetCommunityMapQueueMode() == QUEUEMODE_COOP_QUICK_PLAY )
	{
		// Quickplay does not enumerate files at this point.  C_CommunityCoopManager will handle the enumeration and downloading of files from here out.
		CUIGameData::Get()->InitiateOnlineCoopPlay( NULL, "playonline", "coop_community", "" );
	}
	else
	{
		// Singleplayer enumerates files now!
		BeginQuickPlayEnumeration();
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CQuickPlay::BeginQuickPlayEnumeration( void )
{
	// Throw up a "waiting for file download" wait screen
	KeyValues *pSettings = new KeyValues( "WaitScreen" );
	KeyValues::AutoDelete autodelete_pSettings( pSettings );
	pSettings->SetPtr( "options/asyncoperation", g_QuickPlay_WaitForEnumerationOperation.Prepare() );
	pSettings->SetPtr( "options/waitscreencallback", g_QuickPlay_QueryQuickPlayMapsCallback.Prepare() );

	CUIGameData::Get()->CloseWaitScreen( NULL, NULL );

	// We're still downloading, so stall while it works
	CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_EditorMenu_WaitForQuickPlayResults", MIN_WAITSCREEN_WAIT, pSettings );
	m_eState = WAITING_FOR_ENUMERATION;

	m_flStartupTime = gpGlobals->curtime;

#if !defined( _GAMECONSOLE )
	// If we've got our history, go ahead and query for quickplay maps now
	if ( BASEMODPANEL_SINGLETON.QueueHistoryReady() )
	{
		if( cm_community_debug_spew.GetBool() ) ConColorMsg( rgbaCommunityDebug, "Querying for quickplay maps - BeginQuickPlayEnumeration\n" );
		BASEMODPANEL_SINGLETON.QueryForQuickPlayMaps();
		m_eState = QUERIED_FOR_QUICK_PLAY_MAPS;
	}
#endif // _GAMECONSOLE
}

#if !defined(_GAMECONSOLE )

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CQuickPlay::SetQuickPlaySelection( EWorkshopEnumerationType eType )
{
	// Make sure we're within a reasonable
	Assert( eType < ARRAYSIZE( g_pszQuickPlayDescriptions ) );
	if ( eType >= ARRAYSIZE( g_pszQuickPlayDescriptions ) )
		return;

	BASEMODPANEL_SINGLETON.SetCurrentQuickPlayEnumerationType( eType );

	// NOTE: This must match our enum indexing!
	m_pEnumerationTypesList->SelectPanelItem( eType );
	
	m_pQuickPlayDescription->SetText( g_pszQuickPlayDescriptions[eType]);

	// Set the icon type
	if ( m_pQuickPlayIcon )
	{
		m_pQuickPlayIcon->SetImage( g_pszQuickPlayIcons[eType] );
	}
}

#endif // !_GAMECONSOLE

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuickPlay::OnCommand( const char *command )
{
#if !defined(_GAMECONSOLE )

	if ( !V_stricmp( "QuickPlayTestChambers", command ) )
	{
		// Do nothing
	}
	else
#endif // !_GAMECONSOLE
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuickPlay::UpdateFooter( void )
{
	CBaseModFooterPanel *footer = BASEMODPANEL_SINGLETON.GetFooterPanel();
	if ( footer == NULL )
		return;
	
	// Setup our basic buttons
	footer->SetButtons( FB_ABUTTON|FB_BBUTTON );
	footer->SetButtonText( FB_ABUTTON, "#L4D360UI_SignIn_SignInPlay" );
	footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuickPlay::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pQuickPlayDescription = dynamic_cast< vgui::Label * >( FindChildByName( "LblQuickPlayDescription" ) );

#if !defined(_GAMECONSOLE )

	// Add all of our enumeration types to our list
	for ( int i=0; i < ARRAYSIZE( m_pQuickPlayTypeButtons ); i++ )
	{
		// Add a panel to our list
		EnumerationTypeItem *pItem = m_pEnumerationTypesList->AddPanelItem< EnumerationTypeItem >( "EnumerationTypeItem" );
		
		const wchar_t *pDesc = g_pVGuiLocalize->Find( g_pszQuickPlayTitle[i] );
		pItem->SetTitle( pDesc );
		int nTall = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 24 );
		pItem->SetTall( nTall );
		pItem->SetEnumerationType( (EWorkshopEnumerationType) i );
	}

	m_pQuickPlayIcon = dynamic_cast< ImagePanel * >( FindChildByName( "FilterIcon" ) );

	SetQuickPlaySelection( k_EWorkshopEnumerationTypeRankedByVote );

#endif // !_GAMECONSOLE 

	UpdateFooter();
}

//-----------------------------------------------------------------------------
// Purpose: Reset everything to known state after an error or user-initiated cancellation of the enumeration
//-----------------------------------------------------------------------------
void CQuickPlay::Reset( void )
{
	// Stop our wait screen
	GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );
	if( waitScreen )
	{
		waitScreen->Close();
	}

	m_eState = IDLE;
	m_flStartupTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuickPlay::RunFrame( void )
{
	// First, make sure we haven't taken too long to get here
	if ( m_flStartupTime && ( gpGlobals->curtime - m_flStartupTime ) > MAX_ENUMERATION_WAIT_TIME )
	{
		Reset();

		// Tell the user there was a problem with their request
		GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( BASEMODPANEL_SINGLETON.OpenWindow( WT_GENERICCONFIRMATION, this, true ) );
		GenericConfirmation::Data_t data;
		data.pWindowTitle = "#PORTAL2_WorkshopError_DownloadError_Title";
		data.pMessageText = "#PORTAL2_QuickPlay_Timeout";
		data.bOkButtonEnabled = true;
		data.pfnOkCallback = NULL;

		confirmation->SetUsageData( data );
	
		return;
	}

	// If we're waiting to query for quickplay maps, check our conditions
	if ( m_eState == WAITING_FOR_ENUMERATION )
	{
		// Query for quick play maps, pop a waiting dialog
		if ( BASEMODPANEL_SINGLETON.QueueHistoryReady() )
		{
#if !defined( _GAMECONSOLE )
			BASEMODPANEL_SINGLETON.QueryForQuickPlayMaps();
			m_eState = QUERIED_FOR_QUICK_PLAY_MAPS;
			if( cm_community_debug_spew.GetBool() ) ConColorMsg( rgbaCommunityDebug, "Querying for quickplay maps - RunFrame\n" );
#endif // _GAMECONSOLE
		}
	}
	else if ( m_eState == ATTEMPTING_TO_LAUNCH_MAP )
	{
		switch ( BASEMODPANEL_SINGLETON.GetCommunityMapQueueMode() )
		{
		case QUEUEMODE_QUICK_PLAY:
			{
				AttemptLaunchQuickPlayMap();
			}
			break;
		case QUEUEMODE_COOP_QUICK_PLAY:
			{
				Assert(0);
			}
			break;
		default:
			Assert(0);
		}	
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuickPlay::FindQuickPlayMapsComplete( const char *lpszMsg )
{
	if( cm_community_debug_spew.GetBool() ) ConColorMsg( rgbaCommunityDebug, "Attempting to launch map!\n" );
	m_eState = ATTEMPTING_TO_LAUNCH_MAP;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuickPlay::FindQuickPlayMapsFailed( const char *lpszMsg )
{
	// Reset to a known state
	Reset();

	// Tell the user there was a problem with their request
	GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( BASEMODPANEL_SINGLETON.OpenWindow( WT_GENERICCONFIRMATION, this, true ) );
	GenericConfirmation::Data_t data;
	data.pWindowTitle = "#PORTAL2_QuickPlay_Title";
	data.pMessageText = "#PORTAL2_QuickPlay_NoPuzzles";
	data.bOkButtonEnabled = true;
	data.pfnOkCallback = NULL;
	
	confirmation->SetUsageData( data );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuickPlay::FindQuickPlayMapsError( const char *lpszMsg )
{
	// Reset to a known state
	Reset();

	// Tell the user there was a problem with their request
	GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( BASEMODPANEL_SINGLETON.OpenWindow( WT_GENERICCONFIRMATION, this, true ) );
	GenericConfirmation::Data_t data;
	data.pWindowTitle = "#PORTAL2_QuickPlay_Title";
	data.pMessageText = "#PORTAL2_QuickPlay_Error";
	data.bOkButtonEnabled = true;
	data.pfnOkCallback = NULL;

	confirmation->SetUsageData( data );
}

//-----------------------------------------------------------------------------
// Purpose: The type selection has changed
//-----------------------------------------------------------------------------
void CQuickPlay::EnumerationTypeSelected( int type )
{
	SetQuickPlaySelection( (EWorkshopEnumerationType) type );
}

//-----------------------------------------------------------------------------
// Purpose: The type selection has changed
//-----------------------------------------------------------------------------
void CQuickPlay::LaunchRequested( int type )
{
	SetQuickPlaySelection( (EWorkshopEnumerationType) type );
	StartEnumeration();
}

//-----------------------------------------------------------------------------
// Purpose: User stopped looking for quick play maps
//-----------------------------------------------------------------------------
void CQuickPlay::FindQuickPlayMapsAborted( const char *msg )
{
	// Reset to a known state
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: Launch the map, once complete
//-----------------------------------------------------------------------------
void CQuickPlay::AttemptLaunchQuickPlayMap( void )
{
	// Now, find and launch the map in question
	const PublishedFileInfo_t *pInfo = BASEMODPANEL_SINGLETON.GetNextQuickPlayMapInQueue();
	if ( pInfo == NULL )
	{
		// Assert( pInfo );
		// Log_Warning( LOG_WORKSHOP, "[CPuzzleMakerMenu] Failed to load first map in quick play queue!\n" );
		return;
	}

	// See if it's ready to go
	UGCFileRequestStatus_t status = WorkshopManager().GetUGCFileRequestStatus( pInfo->m_hFile );	

	// Switch our wait panel type
	CUIGameData::Get()->UpdateWaitPanel( pInfo->m_hFile );
	CUIGameData::Get()->UpdateWaitPanel( "#PORTAL2_WaitScreen_DownloadingPuzzle", 1.0f );

	switch( status )
	{
	case UGCFILEREQUEST_ERROR:
		{
			// There was a bug, skip this file
			BASEMODPANEL_SINGLETON.RemoveQuickPlayMapFromQueue( pInfo->m_nPublishedFileId );
			// Reset the clock to give the next map a fair chance
			m_flStartupTime = gpGlobals->curtime;

			// There's no more maps!
			if( BASEMODPANEL_SINGLETON.GetNumQuickPlayEntries() == 0 )
			{
				FindQuickPlayMapsFailed( "" );
			}
		}
		break;
	
	case UGCFILEREQUEST_READY:
		{
			// Move this up in priority to clear other requests
			WorkshopManager().PromoteUGCFileRequestToTop( pInfo->m_hFile );
		}
		break;

	case UGCFILEREQUEST_FINISHED:
		{
			// Close down the wait panel in this case
			if ( m_flStartupTime && ( gpGlobals->curtime - m_flStartupTime ) >= MIN_WAITSCREEN_WAIT )
			{
				LaunchQuickPlayMap( pInfo->m_nPublishedFileId );
			}
		}
		break;

	default:
		break;
	}

	// Keep waiting...
}

//-----------------------------------------------------------------------------
// Purpose: Launch the map, once complete
//-----------------------------------------------------------------------------
void CQuickPlay::LaunchQuickPlayMap( PublishedFileId_t unFileID )
{
	// Get our map information
	const PublishedFileInfo_t *pMapInfo = WorkshopManager().GetPublishedFileInfoByID( unFileID );
	Assert( pMapInfo );
	if ( pMapInfo == NULL )
		return;

	m_flStartupTime = 0.0f;

	// Save this for later reference
	BASEMODPANEL_SINGLETON.SetCurrentCommunityMapID( unFileID );
	BASEMODPANEL_SINGLETON.SetNumCommunityMapsPlayedThisSession( 1 );

	// Don't draw us behind this window when it launches!
	BASEMODPANEL_SINGLETON.CloseAllWindows();

	const char *lpszFilename = WorkshopManager().GetUGCFilename( pMapInfo->m_hFile );
	const char *lpszDirectory = WorkshopManager().GetUGCFileDirectory( pMapInfo->m_hFile );

	char szFilenameNoExtension[MAX_PATH];
	Q_FileBase( lpszFilename, szFilenameNoExtension, sizeof(szFilenameNoExtension) );

	char szMapName[MAX_PATH];
	V_SafeComposeFilename( lpszDirectory, szFilenameNoExtension, szMapName, sizeof(szMapName) );

	// Move past the "maps" folder, it's implied by the following call to load
	const char *lpszUnbasedDirectory = V_strnchr( szMapName, CORRECT_PATH_SEPARATOR, sizeof(szMapName) );
	lpszUnbasedDirectory++; // Move past the actual path separator character

	if( cm_community_debug_spew.GetBool() ) Warning( "-----Loading quickplay map %llu-----\n", unFileID );

	KeyValues *pSettings = new KeyValues( "FadeOutStartGame" );
	KeyValues::AutoDelete autodelete_pSettings( pSettings );
	pSettings->SetString( "map", lpszUnbasedDirectory );
	pSettings->SetString( "reason", "newgame" );
	BASEMODPANEL_SINGLETON.OpenWindow( WT_FADEOUTSTARTGAME, this, true, pSettings );

	m_eState = IDLE;
}
