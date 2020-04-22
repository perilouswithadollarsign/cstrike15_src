//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "cbase.h"
#include "vchallengemodedialog.h"
#include "VFooterPanel.h"
#include "VGenericPanelList.h"
//#include "vgui_controls/ImagePanel.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui/ilocalize.h"
#include "vgui_controls/scrollbar.h"
#include "shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;


namespace BaseModUI
{


CMapListItem::CMapListItem( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName ),
	m_pListCtrlr( ( GenericPanelList * )pParent )
{
	m_pDialog = dynamic_cast< CChallengeModeDialog * >( m_pListCtrlr->GetParent() );

	SetProportional( true );
	SetPaintBackgroundEnabled( true );

	m_nChapterIndex = 0;
	m_nMapIndex = 0;
	m_hTextFont = vgui::INVALID_FONT;

	m_nTextOffsetY = 0;

	m_bSelected = false;
	m_bHasMouseover = false;
	m_bLocked = false;
}

void CMapListItem::SetChapterIndex( int nIndex, bool bMapEntry )
{
	m_nChapterIndex = nIndex;
	if ( nIndex <= 0 || bMapEntry )
		return;

	Label *pLabel = dynamic_cast< Label* >( FindChildByName( "LblChapterName" ) );
	if ( !pLabel )
		return;

	wchar_t chapterString[256];
	wchar_t *pChapterTitle = NULL;

	pChapterTitle = g_pVGuiLocalize->Find( CFmtStr( "#SP_PRESENCE_TEXT_CH%d", m_nChapterIndex ) );
	bool bIsLocked = m_nChapterIndex > m_pDialog->GetNumAllowedChapters();
	if ( bIsLocked )
	{
		// chapter is locked, hide title
		m_bLocked = true;

		V_wcsncpy( chapterString, pChapterTitle, sizeof( chapterString ) );
		wchar_t *pHeaderPrefix = wcsstr( chapterString, L"\n" );
		if ( pHeaderPrefix )
		{
			// truncate the title, want to preserve "Chapter ?"
			*pHeaderPrefix = 0;
			pChapterTitle = chapterString;
		}
	}

		
	if ( !pChapterTitle )
	{
		pChapterTitle = L"";
	}

	wchar_t *pHeaderPrefix = wcsstr( pChapterTitle, L"\n" );
	if ( pHeaderPrefix )
	{
		pChapterTitle = pHeaderPrefix + 1;
	}

	pLabel->SetText( pChapterTitle );
}

	
void CMapListItem::SetMapIndex( int nIndex )
{
	m_nMapIndex = nIndex;
	if ( nIndex <= 0 )
		return;

	Label *pLabel = dynamic_cast< Label* >( FindChildByName( "LblChapterName" ) );
	if( !pLabel )
		return;

	wchar_t mapString[256];

	wchar_t *pMapTitle = g_pVGuiLocalize->Find( CFmtStr( "#SP_MAP_NAME_CH%d_MAP%d", m_nChapterIndex, m_nMapIndex ) );

	bool bIsLocked = m_nChapterIndex > m_pDialog->GetNumAllowedChapters();
	if( bIsLocked )
	{
		m_bLocked = true;

		V_wcsncpy( mapString, L"Locked", sizeof(mapString) );
		pMapTitle = mapString;
	}

	pLabel->SetText( pMapTitle );
}

void CMapListItem::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/BaseModUI/newgame_chapteritem.res" );

	m_hTextFont = pScheme->GetFont( "NewGameChapterName", true );

	m_TextColor = GetSchemeColor( "HybridButton.TextColor", pScheme );
	m_FocusColor = GetSchemeColor( "HybridButton.FocusColor", pScheme );
	m_CursorColor = GetSchemeColor( "HybridButton.CursorColor", pScheme );
	m_LockedColor = GetSchemeColor( "HybridButton.LockedColor", pScheme );
	m_MouseOverCursorColor = GetSchemeColor( "HybridButton.MouseOverCursorColor", pScheme );
	m_LostFocusColor = Color( 120, 120, 120, 255 );

	m_nTextOffsetY = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "NewGameDialog.TextOffsetY" ) ) );
}

void CMapListItem::PaintBackground()
{
	//bool bHasFocus = HasFocus() || IsSelected();

	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );

	// if we're highlighted, background
	if ( HasFocus() )
	{
		surface()->DrawSetColor( m_CursorColor );
		surface()->DrawFilledRect( 0, 0, wide, tall );
	}
	else if ( IsSelected() )
	{
		surface()->DrawSetColor( m_LostFocusColor );
		surface()->DrawFilledRect( 0, 0, wide, tall );
	}
	else if ( HasMouseover() )
	{
		surface()->DrawSetColor( m_MouseOverCursorColor );
		surface()->DrawFilledRect( 0, 0, wide, tall );
	}

	DrawListItemLabel( dynamic_cast< vgui::Label * >( FindChildByName( "LblChapterName" ) ) );
}

void CMapListItem::OnCursorEntered()
{ 
	SetHasMouseover( true );

	if ( IsPC() )
		return;

	if ( GetParent() )
		GetParent()->NavigateToChild( this );
	else
		NavigateTo();
}

void CMapListItem::NavigateTo( void )
{
	m_pListCtrlr->SelectPanelItemByPanel( this );
#if !defined( _GAMECONSOLE )
	SetHasMouseover( true );
	RequestFocus();
#endif
	BaseClass::NavigateTo();
}

void CMapListItem::NavigateFrom( void )
{
	SetHasMouseover( false );
	// get the parent
	CChallengeModeDialog *pPanel = static_cast< CChallengeModeDialog * >( CBaseModPanel::GetSingleton().GetWindow( WT_CHALLENGEMODE ) );
	if ( pPanel )
	{
		if ( pPanel->GetCurrentChapter() == m_nChapterIndex && pPanel->GetCurrentMap() == m_nMapIndex )
			SetSelected( true );
	}

	BaseClass::NavigateFrom();
#ifdef _GAMECONSOLE
	OnClose();
#endif
}

void CMapListItem::OnKeyCodePressed( vgui::KeyCode code )
{
	int iUserSlot = GetJoystickForCode( code );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( iUserSlot );
	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_A:
	case KEY_ENTER:
		ActivateSelectedItem();
		return;
	/*case KEY_RIGHT:
	case KEY_XSTICK1_RIGHT:
		if ( m_nMapIndex != 0)
			break;*/
		// move to the second panel
	}

	BaseClass::OnKeyCodePressed( code );
}

void CMapListItem::OnMousePressed( vgui::MouseCode code )
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

void CMapListItem::OnMouseDoublePressed( vgui::MouseCode code )
{
	if ( code == MOUSE_LEFT )
	{
		CMapListItem* pListItem = static_cast< CMapListItem* >( m_pListCtrlr->GetSelectedPanelItem() );
		if ( pListItem )
		{
			OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		}

		return;
	}

	BaseClass::OnMouseDoublePressed( code );
}

void CMapListItem::PerformLayout()
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


void CMapListItem::DrawListItemLabel( vgui::Label *pLabel )
{
	if ( !pLabel )
		return;

	bool bHasFocus = HasFocus() || IsSelected();

	Color textColor = m_bLocked ? m_LockedColor : m_TextColor;
	if ( bHasFocus && !m_bLocked )
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

bool CMapListItem::ActivateSelectedItem()
{
	CMapListItem* pListItem = static_cast< CMapListItem* >( m_pListCtrlr->GetSelectedPanelItem() );
	if ( !pListItem || pListItem->IsLocked() )
		return false;

	int nChapterIndex = pListItem->GetChapterIndex();
	int nMapIndex = pListItem->GetMapIndex();
	if ( nChapterIndex > 0 && nMapIndex > 0)
	{
		const char *pMapName = NULL;

		pMapName = CBaseModPanel::GetSingleton().GetMapName( nChapterIndex, nMapIndex );

		if ( pMapName && pMapName[0] )
		{
			// check if we're accessing leaderboards
			if ( m_pDialog->InLeaderboards() )
			{
				char szCommand[64] = { 0 };
				V_strncpy( szCommand, "open_challenge_leaderboard 0 ", sizeof( szCommand ) );
				V_strncat( szCommand, pMapName, sizeof( szCommand ) );
				engine->ExecuteClientCmd( szCommand );
			}
			else // not leaderboards, start the challenge map
			{
				KeyValues *pSettings = new KeyValues( "FadeOutStartGame" );
				KeyValues::AutoDelete autodelete_pSettings( pSettings );
				pSettings->SetString( "map", pMapName );
				pSettings->SetString( "reason", "challenge" );
				CBaseModPanel::GetSingleton().OpenWindow( WT_FADEOUTSTARTGAME, m_pDialog, true, pSettings );
			}
		}
		return true;
	}
	return false;
}

void CMapListItem::SetHasMouseover( bool bHasMouseover )
{
	if ( bHasMouseover )
	{
		for ( int i = 0; i < m_pListCtrlr->GetPanelItemCount(); i++ )
		{
			CMapListItem *pItem = dynamic_cast< CMapListItem* >( m_pListCtrlr->GetPanelItem( i ) );
			if ( pItem && pItem != this )
			{
				pItem->SetHasMouseover( false );
			}
		}
	}
	m_bHasMouseover = bHasMouseover;
}

CChallengeModeDialog::CChallengeModeDialog( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName, false, true )
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );

	m_nAllowedChapters = 0;

	m_bDrawAsLocked = false;

	m_bLeaderboards = false;

	m_pChapterList = new GenericPanelList( this, "ChapterList", GenericPanelList::ISM_PERITEM | GenericPanelList::ISM_ALPHA_INVISIBLE );
	m_pChapterList->SetPaintBackgroundEnabled( false );

	m_pMapList = new GenericPanelList( this, "MapList", GenericPanelList::ISM_PERITEM | GenericPanelList::ISM_ALPHA_INVISIBLE );
	m_pMapList->SetPaintBackgroundEnabled( false );

	m_nChapterIndex = -1;
	m_nMapIndex = -1;
	m_bMapListActive = false;

	const char *pDialogTitle = "#PORTAL2_ChallengeMode";
	SetDialogTitle( pDialogTitle );

	SetFooterEnabled( true );

}

CChallengeModeDialog::~CChallengeModeDialog()
{
	delete m_pChapterList;
	delete m_pMapList;
}

void CChallengeModeDialog::OnCommand( char const *szCommand )
{
	if ( !Q_strcmp( szCommand, "Back" ) )
	{
		// Act as though 360 back button was pressed
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		return;
	}

	BaseClass::OnCommand( szCommand );
}

void CChallengeModeDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// set the correct title
	if ( m_bLeaderboards )
	{
		const char *pDialogTitle = "#L4D360UI_Leaderboard_Title";
		SetDialogTitle( pDialogTitle );
	}
	else
	{
		const char *pDialogTitle = "#PORTAL2_ChallengeMode";
		SetDialogTitle( pDialogTitle );
	}
	

	// determine allowed number of unlocked chapters
	m_nAllowedChapters = BaseModUI::CBaseModPanel::GetSingleton().GetChapterProgress() - 1;
	if ( m_nAllowedChapters < 0 )
	{
		m_nAllowedChapters = 0;
	}
	int nNumChapters = BaseModUI::CBaseModPanel::GetSingleton().GetNumChapters() - 1;

	

	// the list will be all coop commentary maps and the sp maps 
	int nNumListItems = nNumChapters;

	
	for ( int i = 0; i < nNumListItems; i++ )
	{
		CMapListItem *pItem = m_pChapterList->AddPanelItem< CMapListItem >( "newgame_chapteritem" );
		if ( pItem )
		{
			pItem->SetChapterIndex( i + 1 );
		}
	}

	m_pChapterList->SetScrollBarVisible( !IsGameConsole() );
	m_pMapList->SetScrollBarVisible( !IsGameConsole() );

	if ( m_ActiveControl )
	{
		m_ActiveControl->NavigateFrom();
	}
	m_pChapterList->NavigateTo();

	m_pChapterList->SelectPanelItem( 0, GenericPanelList::SD_DOWN, true, false );

	UpdateFooter();
}

void CChallengeModeDialog::Activate()
{
	BaseClass::Activate();

	m_pChapterList->NavigateTo();

	UpdateFooter();
}

void CChallengeModeDialog::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		int visibleButtons = FB_BBUTTON;

		CMapListItem *pListItem = static_cast< CMapListItem* >( m_pChapterList->GetSelectedPanelItem() );
		if ( pListItem && !pListItem->IsLocked() )
		{
			visibleButtons |= FB_ABUTTON;
		}

		pFooter->SetButtons( visibleButtons );
		pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
	}
}

void CChallengeModeDialog::OnItemSelected( const char *pPanelName )
{
	if ( !m_bLayoutLoaded )
		return;
	

	CMapListItem* pListItem = static_cast< CMapListItem* >( m_pChapterList->GetSelectedPanelItem() );

	// if we've changed chapters
	if ( pListItem && ( pListItem->GetChapterIndex() - 1 ) != m_nChapterIndex )
	{
		m_bMapListActive = false;

		m_nChapterIndex = pListItem->GetChapterIndex() - 1;
		// also reset map index
		m_nMapIndex = -1;

		// fill in the list of maps
		int nChapterNumber = pListItem->GetChapterIndex();
		int nMapsInChapter = BaseModUI::CBaseModPanel::GetSingleton().GetNumMapsInChapter( nChapterNumber );

		// clear the panel of previous items
		m_pMapList->RemoveAllPanelItems();
		// reset the scrollbar to the top
		m_pMapList->GetScrollBar()->SetValue( m_nMapIndex );

		for ( int i = 0; i < nMapsInChapter; i++ )
		{
			CMapListItem *pItem = m_pMapList->AddPanelItem< CMapListItem >( "newgame_chapteritem" );
			if ( pItem )
			{
				pItem->SetChapterIndex( nChapterNumber, true ); // ??
				pItem->SetMapIndex( i + 1 );
			}
		}


#if !defined( _GAMECONSOLE )
		// Set active state
		for ( int i = 0; i < m_pChapterList->GetPanelItemCount(); i++ )
		{
			CMapListItem *pItem = dynamic_cast< CMapListItem* >( m_pChapterList->GetPanelItem( i ) );
			if ( pItem )
			{
				pItem->SetSelected( pItem == pListItem );
			}
		}
#endif
	}

	// check if this is a change in the map list
	pListItem = static_cast< CMapListItem* >( m_pMapList->GetSelectedPanelItem() );
	if ( pListItem && ( pListItem->GetMapIndex() - 1 != m_nMapIndex ) )
	{
		m_bMapListActive = true;

		// update map index
		m_nMapIndex = pListItem->GetMapIndex() - 1;

#if !defined( _GAMECONSOLE )
		// Set active state
		for ( int i = 0; i < m_pMapList->GetPanelItemCount(); i++ )
		{
			CMapListItem *pItem = dynamic_cast< CMapListItem* >( m_pMapList->GetPanelItem( i ) );
			if ( pItem )
			{
				pItem->SetSelected( pItem == pListItem );
			}
		}
#endif
	}
	

	UpdateFooter();
}


void CChallengeModeDialog::PaintBackground()
{
	BaseClass::PaintBackground();

	//DrawChapterImage();
}

void CChallengeModeDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( !IsGameConsole() )
	{
		// handle button presses by the footer
		if ( !m_bMapListActive )
		{
			CMapListItem* pListItem = static_cast< CMapListItem* >( m_pChapterList->GetSelectedPanelItem() );
			if ( pListItem )
			{
				if ( code == KEY_XBUTTON_A )
				{
					//pListItem->OnKeyCodePressed( code );
					NavigateRight();
					return;
				}
			}
		}
		else
		{
			CMapListItem* pListItem = static_cast< CMapListItem* >( m_pMapList->GetSelectedPanelItem() );
			if ( pListItem )
			{
				if ( code == KEY_XBUTTON_A )
				{
					pListItem->OnKeyCodePressed( code );
					return;
				}
			}
		}
	}

	BaseClass::OnKeyCodePressed( code );
}

vgui::Panel* CChallengeModeDialog::NavigateRight()
{
	// if the map list isn't already active & the chapter is unlocked
	if ( !m_bMapListActive && ( m_nChapterIndex + 1 <= m_nAllowedChapters ) )
	{
		// select the correct map
		m_bMapListActive = true;

		// select the correct map
		m_nMapIndex = (m_nMapIndex < 0) ? 0 : m_nMapIndex;

		// select the correct item in the map list
		m_pMapList->SelectPanelItem( m_nMapIndex, GenericPanelList::SD_DOWN, true, false );

		// change the focus to the map list
		m_pChapterList->NavigateFrom();
		m_pMapList->NavigateTo();
	}

	return m_pMapList;
}

vgui::Panel* CChallengeModeDialog::NavigateLeft()
{
	if ( !m_bMapListActive )
		return m_pChapterList;

	m_bMapListActive = false;

	// select the correct item in the chapter list
	m_pChapterList->SelectPanelItem( m_nChapterIndex, GenericPanelList::SD_DOWN, true, false );

	// change the focus to the chapter list
	m_pMapList->NavigateFrom();
	m_pChapterList->NavigateTo();

	return m_pChapterList;
}

void CChallengeModeDialog::SetDataSettings( KeyValues *pSettings )
{
	m_bLeaderboards = pSettings->GetBool( "leaderboards", false );

	BaseClass::SetDataSettings( pSettings );
}

}; // namespace BaseModUI
