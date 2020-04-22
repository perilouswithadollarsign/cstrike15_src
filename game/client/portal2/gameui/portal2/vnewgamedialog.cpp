//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "vnewgamedialog.h"
#include "VFooterPanel.h"
#include "VGenericPanelList.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui/ilocalize.h"

#include "cegclientwrapper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

struct coop_commentary_t
{
	const char *m_pMapName;
	const char *m_pTitle;
	const char *m_pPictureName;
};

coop_commentary_t g_CoopCommentaryMaps[] = 
{
	{ "mp_coop_catapult_1", "#PORTAL2_CoopCommentary_Title1",	"vgui/chapters/coopcommentary_chapter1" },
	{ "mp_coop_lobby_2",	"#PORTAL2_CoopCommentary_Title2",	"vgui/chapters/coopcommentary_chapter2" },
	{ "mp_coop_wall_5",		"#PORTAL2_CoopCommentary_Title3",	"vgui/chapters/coopcommentary_chapter3" },
};

namespace BaseModUI
{

class ChapterLabel : public vgui::Label
{
	DECLARE_CLASS_SIMPLE( ChapterLabel, vgui::Label );

public:
	ChapterLabel( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName, "" )
	{
		m_pDialog = dynamic_cast< CNewGameDialog * >( pParent );

		SetProportional( true );
		SetPaintBackgroundEnabled( true );

		SetMouseInputEnabled( false );
		SetKeyBoardInputEnabled( false );

		m_hChapterNumberFont = vgui::INVALID_FONT;
		m_hChapterNameFont = vgui::INVALID_FONT;

		m_nChapterIndex = 0;
	}

	void SetChapterIndex( int nChapterIndex )
	{
		m_nChapterIndex = nChapterIndex;
	}

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		m_hChapterNumberFont = pScheme->GetFont( "NewGameChapter", true );
		m_hChapterNameFont = pScheme->GetFont( "NewGameChapterName", true );
	}

	virtual void PaintBackground()
	{
		if ( !m_nChapterIndex )
			return;

		wchar_t *pChapterTitle = NULL;
		wchar_t chapterNumberString[256];
		chapterNumberString[0] = 0;

		bool bIsLocked = false;
		int nNumCommentaryMaps = 0;
		if ( m_pDialog->IsCommentaryDialog() )
		{
			nNumCommentaryMaps = ARRAYSIZE( g_CoopCommentaryMaps );
			if ( m_nChapterIndex <= nNumCommentaryMaps )
			{
				pChapterTitle = g_pVGuiLocalize->Find( g_CoopCommentaryMaps[m_nChapterIndex-1].m_pTitle );
			}
		}

		if ( m_nChapterIndex > nNumCommentaryMaps )
		{
			// sp chapter labels
			pChapterTitle = g_pVGuiLocalize->Find( CFmtStr( "#SP_PRESENCE_TEXT_CH%d", m_nChapterIndex - nNumCommentaryMaps ) );
			bIsLocked = ( m_nChapterIndex - nNumCommentaryMaps ) > m_pDialog->GetNumAllowedChapters();
		}

		if ( !pChapterTitle )
		{
			pChapterTitle = L"";
		}

		V_wcsncpy( chapterNumberString, pChapterTitle, sizeof( chapterNumberString ) );
		wchar_t *pHeaderPrefix = wcsstr( chapterNumberString, L"\n" );
		if ( pHeaderPrefix )
		{
			*pHeaderPrefix = 0;
			V_wcsncpy( pHeaderPrefix, L" - ", sizeof( chapterNumberString ) - V_wcslen( chapterNumberString ) * sizeof( wchar_t ) );
		}

		if ( !bIsLocked )
		{
			pHeaderPrefix = wcsstr( pChapterTitle, L"\n" );
			if ( pHeaderPrefix )
			{
				pChapterTitle = pHeaderPrefix + 1;
			}
		}
		else
		{
			pChapterTitle =  g_pVGuiLocalize->Find( "#GameUI_Achievement_Locked" );
			if ( !pChapterTitle )
			{
				pChapterTitle = L"...";
			}
		}

		int x = 0;
		x += DrawText( x, 0, chapterNumberString, m_hChapterNumberFont, Color( 0, 0, 0, 255 ) );
		int yTitleOffset = 0;
		if ( IsOSX() )
			yTitleOffset -=  (( surface()->GetFontTall(m_hChapterNameFont) - surface()->GetFontTall(m_hChapterNumberFont) )/2 + 1) ;
		x += DrawText( x, yTitleOffset, pChapterTitle, m_hChapterNameFont, Color( 0, 0, 0, 255 ) );
	}

private:
	int	DrawText( int x, int y, const wchar_t *pString, vgui::HFont hFont, Color color )
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

	CNewGameDialog *m_pDialog;

	vgui::HFont	m_hChapterNumberFont;
	vgui::HFont	m_hChapterNameFont;

	int			m_nChapterIndex;
};

class ChapterListItem : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( ChapterListItem, vgui::EditablePanel );

public:
	ChapterListItem( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName ),
		m_pListCtrlr( ( GenericPanelList * )pParent )
	{
		m_pDialog = dynamic_cast< CNewGameDialog * >( m_pListCtrlr->GetParent() );

		SetProportional( true );
		SetPaintBackgroundEnabled( true );

		m_nChapterIndex = 0;
		m_hTextFont = vgui::INVALID_FONT;

		m_nTextOffsetY = 0;

		m_bSelected = false;
		m_bHasMouseover = false;
		m_bLocked = false;
	}

	void SetChapterIndex( int nIndex )
	{
		m_nChapterIndex = nIndex;
		if ( nIndex <= 0 )
			return;

		Label *pLabel = dynamic_cast< Label* >( FindChildByName( "LblChapterName" ) );
		if ( !pLabel )
			return;

		wchar_t chapterString[256];
		wchar_t *pChapterTitle = NULL;

		int nNumCommentaryMaps = 0;
		if ( m_pDialog->IsCommentaryDialog() )
		{
			nNumCommentaryMaps = ARRAYSIZE( g_CoopCommentaryMaps );
			if ( m_nChapterIndex <= nNumCommentaryMaps )
			{
				pChapterTitle = g_pVGuiLocalize->Find( g_CoopCommentaryMaps[m_nChapterIndex-1].m_pTitle );
			}
		}

		if ( m_nChapterIndex > nNumCommentaryMaps )
		{
			pChapterTitle = g_pVGuiLocalize->Find( CFmtStr( "#SP_PRESENCE_TEXT_CH%d", m_nChapterIndex - nNumCommentaryMaps ) );
			bool bIsLocked = ( m_nChapterIndex - nNumCommentaryMaps ) > m_pDialog->GetNumAllowedChapters();
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

	bool IsSelected( void ) { return m_bSelected; }
	void SetSelected( bool bSelected ) { m_bSelected = bSelected; }

	bool HasMouseover( void ) { return m_bHasMouseover; }

	void SetHasMouseover( bool bHasMouseover )
	{
		if ( bHasMouseover )
		{
			for ( int i = 0; i < m_pListCtrlr->GetPanelItemCount(); i++ )
			{
				ChapterListItem *pItem = dynamic_cast< ChapterListItem* >( m_pListCtrlr->GetPanelItem( i ) );
				if ( pItem && pItem != this )
				{
					pItem->SetHasMouseover( false );
				}
			}
		}
		m_bHasMouseover = bHasMouseover; 
	}

	bool IsLocked( void ) { return m_bLocked; }

	int GetChapterIndex()
	{
		return m_nChapterIndex;
	}

	CEG_NOINLINE void OnKeyCodePressed( vgui::KeyCode code )
	{
		int iUserSlot = GetJoystickForCode( code );
		CBaseModPanel::GetSingleton().SetLastActiveUserId( iUserSlot );
		switch( GetBaseButtonCode( code ) )
		{
		case KEY_XBUTTON_A: 
		case KEY_ENTER: 
			ActivateSelectedItem();
			return;
		}

		CEG_PROTECT_MEMBER_FUNCTION( ChapterListItem_OnKeyCodePressed );

		BaseClass::OnKeyCodePressed( code );
	}

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		LoadControlSettings( "Resource/UI/BaseModUI/newgame_chapteritem.res" );

		m_hTextFont = pScheme->GetFont( "NewGameChapterName", true );

		m_TextColor = GetSchemeColor( "HybridButton.TextColor", pScheme );
		m_FocusColor = GetSchemeColor( "HybridButton.FocusColor", pScheme );
		m_CursorColor = GetSchemeColor( "HybridButton.CursorColor", pScheme );
		m_LockedColor = GetSchemeColor( "HybridButton.LockedColor", pScheme );
		m_MouseOverCursorColor = GetSchemeColor( "HybridButton.MouseOverCursorColor", pScheme );

		m_nTextOffsetY = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "NewGameDialog.TextOffsetY" ) ) );
	}

	virtual void PaintBackground()
	{
		bool bHasFocus = HasFocus() || IsSelected();
	
		int x, y, wide, tall;
		GetBounds( x, y, wide, tall );

		if ( bHasFocus )
		{
			surface()->DrawSetColor( m_CursorColor );
			surface()->DrawFilledRect( 0, 0, wide, tall );
		}
		else if ( HasMouseover() )
		{
			surface()->DrawSetColor( m_MouseOverCursorColor );
			surface()->DrawFilledRect( 0, 0, wide, tall );
		}

		DrawListItemLabel( dynamic_cast< vgui::Label * >( FindChildByName( "LblChapterName" ) ) );
	}

	virtual void OnCursorEntered()
	{
		SetHasMouseover( true ); 

		if ( IsPC() )
			return;

		if ( GetParent() )
			GetParent()->NavigateToChild( this );
		else
			NavigateTo();
	}

	virtual void OnCursorExited()
	{
		SetHasMouseover( false ); 
	}

	virtual void NavigateTo( void )
	{
		m_pListCtrlr->SelectPanelItemByPanel( this );
#if !defined( _GAMECONSOLE )
		SetHasMouseover( true );
		RequestFocus();
#endif
		BaseClass::NavigateTo();
	}

	virtual void NavigateFrom( void )
	{
		SetHasMouseover( false );
		BaseClass::NavigateFrom();
#ifdef _GAMECONSOLE
		OnClose();
#endif
	}

	CEG_NOINLINE void OnMousePressed( vgui::MouseCode code )
	{
		CEG_PROTECT_MEMBER_FUNCTION( ChapterListItem_OnMousePressed );

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

	CEG_NOINLINE void OnMouseDoublePressed( vgui::MouseCode code )
	{
		if ( code == MOUSE_LEFT )
		{
			ChapterListItem* pListItem = static_cast< ChapterListItem* >( m_pListCtrlr->GetSelectedPanelItem() );
			if ( pListItem )
			{
				OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
			}

			CEG_PROTECT_MEMBER_FUNCTION( ChapterListItem_OnMouseDoublePressed );

			return;
		}

		BaseClass::OnMouseDoublePressed( code );
	}

	void PerformLayout()
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

private:
	void DrawListItemLabel( vgui::Label *pLabel )
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

	bool ActivateSelectedItem()
	{
		ChapterListItem* pListItem = static_cast< ChapterListItem* >( m_pListCtrlr->GetSelectedPanelItem() );
		if ( !pListItem || pListItem->IsLocked() )
			return false;

		int nChapterIndex = pListItem->GetChapterIndex();
		if ( nChapterIndex > 0 )
		{
			const char *pMapName = NULL;

			int nNumCommentaryMaps = 0;
			if ( m_pDialog->IsCommentaryDialog() )
			{
				nNumCommentaryMaps = ARRAYSIZE( g_CoopCommentaryMaps );
				if ( nChapterIndex <= nNumCommentaryMaps )
				{
					pMapName = g_CoopCommentaryMaps[nChapterIndex - 1].m_pMapName;
				}
			}

			if ( nChapterIndex > nNumCommentaryMaps )
			{
				pMapName = CBaseModPanel::GetSingleton().ChapterToMapName( nChapterIndex - nNumCommentaryMaps );
			}

			if ( pMapName && pMapName[0] )
			{
				bool bSavesAllowed;
				bSavesAllowed = !m_pDialog->IsCommentaryDialog();
#if defined( _GAMECONSOLE )
				if ( XBX_GetPrimaryUserIsGuest() )
				{
					bSavesAllowed = false;
				}
#endif
				if ( IsGameConsole() && bSavesAllowed )
				{
					KeyValues *pSettings = new KeyValues( "NewGame" );
					KeyValues::AutoDelete autodelete_pSettings( pSettings );
					pSettings->SetString( "map", pMapName );
					pSettings->SetString( "reason", m_pDialog->IsCommentaryDialog() ? "commentary" : "newgame" );
					CBaseModPanel::GetSingleton().OpenWindow( WT_AUTOSAVENOTICE, m_pDialog, true, pSettings );
				}
				else
				{
					KeyValues *pSettings = new KeyValues( "FadeOutStartGame" );
					KeyValues::AutoDelete autodelete_pSettings( pSettings );
					pSettings->SetString( "map", pMapName );
					pSettings->SetString( "reason", m_pDialog->IsCommentaryDialog() ? "commentary" : "newgame" );
					CBaseModPanel::GetSingleton().OpenWindow( WT_FADEOUTSTARTGAME, m_pDialog, true, pSettings );
				}
				return true;
			}
		}

		return false;
	}

	CNewGameDialog		*m_pDialog;

	GenericPanelList	*m_pListCtrlr;
	vgui::HFont			m_hTextFont;
	int					m_nChapterIndex;

	Color				m_TextColor;
	Color				m_FocusColor;
	Color				m_DisabledColor;
	Color				m_CursorColor;
	Color				m_LockedColor;
	Color				m_MouseOverCursorColor;

	bool				m_bSelected;
	bool				m_bHasMouseover;
	bool				m_bLocked;

	int					m_nTextOffsetY;
};

CEG_NOINLINE CNewGameDialog::CNewGameDialog( vgui::Panel *pParent, const char *pPanelName, bool bIsCommentaryDialog ) : BaseClass( pParent, pPanelName, false, true )
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );

	m_bIsCommentaryDialog = bIsCommentaryDialog;

	m_pChapterImage = NULL;

	m_nAllowedChapters = 0;

	m_nVignetteImageId = -1;
	m_nChapterImageId = -1;
	m_bDrawAsLocked = false;

	m_pChapterList = new GenericPanelList( this, "ChapterList", GenericPanelList::ISM_PERITEM | GenericPanelList::ISM_ALPHA_INVISIBLE );
	m_pChapterList->SetPaintBackgroundEnabled( false );	

	CEG_PROTECT_MEMBER_FUNCTION( CNewGameDialog_CNewGameDialog );

	m_pChapterLabel = new ChapterLabel( this, "ChapterText" );

	const char *pDialogTitle = bIsCommentaryDialog ? "#L4D360UI_GameSettings_Commentary" : "#PORTAL2_NewGame";
	SetDialogTitle( pDialogTitle );

	SetFooterEnabled( true );
}

CNewGameDialog::~CNewGameDialog()
{
	delete m_pChapterList;
	delete m_pChapterLabel;
}

CEG_NOINLINE void CNewGameDialog::OnCommand( char const *szCommand )
{
	CEG_PROTECT_VIRTUAL_FUNCTION( CNewGameDialog_OnCommand );

	if ( !Q_strcmp( szCommand, "Back" ) )
	{
		// Act as though 360 back button was pressed
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		return;
	}

	BaseClass::OnCommand( szCommand );
}

CEG_NOINLINE void CNewGameDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	m_pChapterImage = dynamic_cast< ImagePanel* >( FindChildByName( "ChapterImage" ) );

	m_nVignetteImageId = CBaseModPanel::GetSingleton().GetImageId( "vgui/chapters/vignette" );

	// determine allowed number of unlocked chapters
	m_nAllowedChapters = BaseModUI::CBaseModPanel::GetSingleton().GetChapterProgress();
	if ( !m_nAllowedChapters )
	{
		m_nAllowedChapters = 1;
	}
	int nNumChapters = BaseModUI::CBaseModPanel::GetSingleton().GetNumChapters();

	// the coop maps are only available in commentary mode
	int nNumCoopCommentaryItems = m_bIsCommentaryDialog ? ARRAYSIZE( g_CoopCommentaryMaps ) : 0;

	// the list will be all coop commentary maps and the sp maps 
	int nNumListItems = nNumCoopCommentaryItems + nNumChapters;

	// reserve a dead entry, chapters start at [1..]
	m_ChapterImages.AddToTail( -1 );

	// add the coop items
	for ( int i = 0; i < nNumCoopCommentaryItems; i++ )
	{
		int nImageId = CBaseModPanel::GetSingleton().GetImageId( g_CoopCommentaryMaps[i].m_pPictureName );
		m_ChapterImages.AddToTail( nImageId );
	}

	CEG_PROTECT_VIRTUAL_FUNCTION( CNewGameDialog_ApplySchemeSettings );

	// add the sp chapters
	int nNoSaveGameImageId = CBaseModPanel::GetSingleton().GetImageId( "vgui/no_save_game" );
	for ( int i = 0; i < nNumChapters; i++ )
	{
		// by default, chapter is still locked
		int nImageId = nNoSaveGameImageId;
		if ( i + 1 <= m_nAllowedChapters )
		{	
			// get the unlocked chapter images
			nImageId = CBaseModPanel::GetSingleton().GetImageId( CFmtStr( "vgui/chapters/chapter%d", i + 1 ) );
		}
		m_ChapterImages.AddToTail( nImageId );
	}
	
	for ( int i = 0; i < nNumListItems; i++ )
	{
		ChapterListItem *pItem = m_pChapterList->AddPanelItem< ChapterListItem >( "newgame_chapteritem" );
		if ( pItem )
		{
			pItem->SetChapterIndex( i + 1 );
		}
	}

	// the chapter image will get set based on the initial selection
	SetChapterImage( 0 );

	m_pChapterList->SetScrollBarVisible( !IsGameConsole() );

	if ( m_ActiveControl )
	{
		m_ActiveControl->NavigateFrom();
	}
	m_pChapterList->NavigateTo();

	m_pChapterList->SelectPanelItem( 0, GenericPanelList::SD_DOWN, true, false );

	UpdateFooter();
}

CEG_NOINLINE void CNewGameDialog::Activate()
{
	BaseClass::Activate();

	m_pChapterList->NavigateTo();

	CEG_PROTECT_VIRTUAL_FUNCTION( CNewGameDialog_Activate );

	UpdateFooter();
}

void CNewGameDialog::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		int visibleButtons = FB_BBUTTON;

		ChapterListItem *pListItem;
		pListItem = static_cast< ChapterListItem* >( m_pChapterList->GetSelectedPanelItem() );
		if ( pListItem && !pListItem->IsLocked() )
		{
			visibleButtons |= FB_ABUTTON;
		}

		pFooter->SetButtons( visibleButtons );
		pFooter->SetButtonText( FB_ABUTTON, IsGameConsole() ? "#L4D360UI_Select" : "#PORTAL2_ButtonAction_Play" );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
	}
}

void CNewGameDialog::OnItemSelected( const char *pPanelName )
{
	if ( !m_bLayoutLoaded )
		return;

	ChapterListItem* pListItem = static_cast< ChapterListItem* >( m_pChapterList->GetSelectedPanelItem() );
	SetChapterImage( pListItem ? pListItem->GetChapterIndex() : 0, pListItem->IsLocked() );

#if !defined( _GAMECONSOLE )
	// Set active state
	for ( int i = 0; i < m_pChapterList->GetPanelItemCount(); i++ )
	{
		ChapterListItem *pItem = dynamic_cast< ChapterListItem* >( m_pChapterList->GetPanelItem( i ) );
		if ( pItem )
		{
			pItem->SetSelected( pItem == pListItem );
		}
	}
#endif

	m_ActiveControl = pListItem;

	UpdateFooter();
}

void CNewGameDialog::SetChapterImage( int nChapterIndex, bool bIsLocked )
{
	if ( m_pChapterLabel )
	{
		m_pChapterLabel->SetVisible( nChapterIndex != 0 );
	}

	if ( nChapterIndex )
	{
		m_pChapterLabel->SetChapterIndex( nChapterIndex );
	}

	m_nChapterImageId = m_ChapterImages[nChapterIndex];
	m_bDrawAsLocked = bIsLocked;
}

void CNewGameDialog::PaintBackground()
{
	BaseClass::PaintBackground();

	DrawChapterImage();
}

void CNewGameDialog::DrawChapterImage()
{
	if ( !m_pChapterImage || m_nChapterImageId == -1 )
		return;

	int x, y, wide, tall;
	m_pChapterImage->GetBounds( x, y, wide, tall );

	surface()->DrawSetColor( Color( 255, 255, 255, m_bDrawAsLocked ? IsX360() ? 240 : 200 : 255 ) );
	surface()->DrawSetTexture( m_nChapterImageId );
	surface()->DrawTexturedRect( x, y, x+wide, y+tall );
	surface()->DrawSetTexture( m_nVignetteImageId );
	surface()->DrawTexturedRect( x, y, x+wide, y+tall );
}

void CNewGameDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( !IsGameConsole() )
	{
		// handle button presses by the footer
		ChapterListItem* pListItem = static_cast< ChapterListItem* >( m_pChapterList->GetSelectedPanelItem() );
		if ( pListItem )
		{
			if ( code == KEY_XBUTTON_A )
			{
				pListItem->OnKeyCodePressed( code );
				return;
			}
		}
	}

	BaseClass::OnKeyCodePressed( code );
}

}; // namespace BaseModUI
