//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "vextrasdialog.h"
#include "VFooterPanel.h"
#include "VGenericPanelList.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui/ilocalize.h"
#include "filesystem.h"
#include "gameui_util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

namespace BaseModUI
{

class CInfoLabel : public vgui::Label
{
	DECLARE_CLASS_SIMPLE( CInfoLabel, vgui::Label );

public:
	CInfoLabel( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName, "" )
	{
		m_pDialog = dynamic_cast< CExtrasDialog * >( pParent );

		SetProportional( true );
		SetPaintBackgroundEnabled( true );

		SetMouseInputEnabled( false );
		SetKeyBoardInputEnabled( false );

		m_hFont = vgui::INVALID_FONT;
		
		m_nInfoIndex = -1;
	}

	void SetInfoIndex( int nInfoIndex )
	{
		m_nInfoIndex = nInfoIndex;
	}

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		m_hFont = pScheme->GetFont( IsGameConsole() ? "GamerTagStatus" : "NewGameChapter", true );
	}

	virtual void PaintBackground()
	{
		ExtraInfo_t *pExtraInfo = m_pDialog->GetExtraInfo( m_nInfoIndex );
		if ( !pExtraInfo )
			return;

		wchar_t szUnicode[512];
		wchar_t *pSubtitle = g_pVGuiLocalize->Find( pExtraInfo->m_SubtitleString.Get() );
		if ( !pSubtitle )
		{
			pSubtitle = szUnicode;
			g_pVGuiLocalize->ConvertANSIToUnicode(  pExtraInfo->m_SubtitleString.Get(), szUnicode, sizeof( szUnicode ) );
		}

		DrawText( 0, 0, pSubtitle, m_hFont, Color( 0, 0, 0, 255 ) );
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

	CExtrasDialog	*m_pDialog;
	vgui::HFont		m_hFont;
	int				m_nInfoIndex;
};

class CInfoListItem : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CInfoListItem, vgui::EditablePanel );

public:
	CInfoListItem( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName ),
		m_pListCtrlr( ( GenericPanelList * )pParent )
	{
		m_pDialog = dynamic_cast< CExtrasDialog * >( m_pListCtrlr->GetParent() );

		SetProportional( true );
		SetPaintBackgroundEnabled( true );

		m_nInfoIndex = -1;
		m_hTextFont = vgui::INVALID_FONT;

		m_nTextOffsetY = 0;

		m_bSelected = false;
		m_bHasMouseover = false;
	}

	void SetInfoIndex( int nIndex )
	{
		m_nInfoIndex = nIndex;
		ExtraInfo_t *pExtraInfo = m_pDialog->GetExtraInfo( m_nInfoIndex );
		if ( !pExtraInfo )
			return;

		Label *pLabel = dynamic_cast< Label* >( FindChildByName( "LblInfoName" ) );
		if ( !pLabel )
			return;

		wchar_t szUnicode[512];
		wchar_t *pTitle = g_pVGuiLocalize->Find( pExtraInfo->m_TitleString.Get() );
		if ( !pTitle )
		{
			pTitle = szUnicode;
			g_pVGuiLocalize->ConvertANSIToUnicode(  pExtraInfo->m_TitleString.Get(), szUnicode, sizeof( szUnicode ) );
		}

		pLabel->SetText( pTitle );
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
				CInfoListItem *pItem = dynamic_cast< CInfoListItem* >( m_pListCtrlr->GetPanelItem( i ) );
				if ( pItem && pItem != this )
				{
					pItem->SetHasMouseover( false );
				}
			}
		}
		m_bHasMouseover = bHasMouseover; 
	}

	int GetInfoIndex()
	{
		return m_nInfoIndex;
	}

	void OnKeyCodePressed( vgui::KeyCode code )
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
		BaseClass::OnKeyCodePressed( code );
	}

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		LoadControlSettings( "Resource/UI/BaseModUI/extras_infoitem.res" );

		m_hTextFont = pScheme->GetFont( "NewGameChapterName", true );

		m_TextColor = GetSchemeColor( "HybridButton.TextColor", pScheme );
		m_FocusColor = GetSchemeColor( "HybridButton.FocusColor", pScheme );
		m_CursorColor = GetSchemeColor( "HybridButton.CursorColor", pScheme );
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

		DrawListItemLabel( dynamic_cast< vgui::Label * >( FindChildByName( "LblInfoName" ) ) );
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

	void OnMousePressed( vgui::MouseCode code )
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

	void OnMouseDoublePressed( vgui::MouseCode code )
	{
		if ( code == MOUSE_LEFT )
		{
			CInfoListItem* pListItem = static_cast< CInfoListItem* >( m_pListCtrlr->GetSelectedPanelItem() );
			if ( pListItem )
			{
				OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
			}
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

		Color textColor = m_TextColor;
		if ( bHasFocus )
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
		CInfoListItem* pListItem = static_cast< CInfoListItem* >( m_pListCtrlr->GetSelectedPanelItem() );
		if ( !pListItem )
			return false;

		ExtraInfo_t *pExtraInfo = m_pDialog->GetExtraInfo( pListItem->GetInfoIndex() );
		if ( !pExtraInfo )
			return false;

		if ( !pExtraInfo->m_MapName.IsEmpty() )
		{
			KeyValues *pSettings = new KeyValues( "FadeOutStartGame" );
			KeyValues::AutoDelete autodelete_pSettings( pSettings );
			pSettings->SetString( "map", pExtraInfo->m_MapName.Get() );
			pSettings->SetString( "reason", "bonusmap" );
			CBaseModPanel::GetSingleton().OpenWindow( WT_FADEOUTSTARTGAME, m_pDialog, true, pSettings );
			return true;
		}
		
		if ( !pExtraInfo->m_VideoName.IsEmpty() )
		{
			KeyValues *pSettings = new KeyValues( "MoviePlayer" );
			KeyValues::AutoDelete autodelete_pSettings( pSettings );
			pSettings->SetString( "video", pExtraInfo->m_VideoName.Get() );
			pSettings->SetBool( "letterbox", true );
			CBaseModPanel::GetSingleton().OpenWindow( WT_MOVIEPLAYER, m_pDialog, false, pSettings );
			return true;
		}
		
		if ( !pExtraInfo->m_URLName.IsEmpty() )
		{
#if !defined( NO_STEAM )
			if ( steamapicontext && steamapicontext->SteamFriends() &&
				steamapicontext->SteamUtils() && steamapicontext->SteamUtils()->IsOverlayEnabled() )
			{
				steamapicontext->SteamFriends()->ActivateGameOverlayToWebPage( pExtraInfo->m_URLName.Get() );
			}
			else
#endif
			{
				CUIGameData::Get()->DisplayOkOnlyMsgBox( NULL, "#L4D360UI_SteamOverlay_Title", "#L4D360UI_SteamOverlay_Text" );
			}
			return true;
		}

		if ( !pExtraInfo->m_Command.IsEmpty() )
		{
			engine->ExecuteClientCmd( pExtraInfo->m_Command.Get() );
			return true;
		}

		return false;
	}

	CExtrasDialog		*m_pDialog;

	GenericPanelList	*m_pListCtrlr;
	vgui::HFont			m_hTextFont;
	int					m_nInfoIndex;

	Color				m_TextColor;
	Color				m_FocusColor;
	Color				m_DisabledColor;
	Color				m_CursorColor;
	Color				m_MouseOverCursorColor;

	bool				m_bSelected;
	bool				m_bHasMouseover;

	int					m_nTextOffsetY;
};

CExtrasDialog::CExtrasDialog( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName, false, true )
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );

	m_pInfoImage = NULL;

	m_nVignetteImageId = -1;
	m_nInfoImageId = -1;

	m_pInfoList = new GenericPanelList( this, "InfoList", GenericPanelList::ISM_PERITEM | GenericPanelList::ISM_ALPHA_INVISIBLE );
	m_pInfoList->SetPaintBackgroundEnabled( false );	

	m_pInfoLabel = new CInfoLabel( this, "InfoText" );

	SetDialogTitle( "#L4D360UI_MainMenu_Extras" );

	SetFooterEnabled( true );

	PopulateFromScript();
}

CExtrasDialog::~CExtrasDialog()
{
	delete m_pInfoList;
	delete m_pInfoLabel;
}

void CExtrasDialog::PopulateFromScript()
{
	// read script info
	KeyValues* pKV = new KeyValues( "extras" );
	if ( !pKV->LoadFromFile( g_pFullFileSystem, "scripts/extras.txt", "GAME" ) )
	{
		pKV->deleteThis();
		return;
	}

	CGameUIConVarRef developer( "developer" );

	for ( KeyValues *pKey = pKV->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey() )
	{
		if ( pKey->GetBool( "developer", false ) && !developer.GetBool() )
		{
			// ignore developer entries when no in developer
			continue;
		}

		int nIndex = m_ExtraInfos.AddToTail();
		m_ExtraInfos[nIndex].m_TitleString = pKey->GetString( "title" );
		m_ExtraInfos[nIndex].m_SubtitleString = pKey->GetString( "subtitle" );
		m_ExtraInfos[nIndex].m_MapName = pKey->GetString( "map" );
		m_ExtraInfos[nIndex].m_VideoName = pKey->GetString( "video" );
		m_ExtraInfos[nIndex].m_URLName = pKey->GetString( "url" );
		m_ExtraInfos[nIndex].m_Command = pKey->GetString( "command" );

		const char *pImageName = pKey->GetString( "pic" );
		if ( pImageName && pImageName[0] )
		{
			m_ExtraInfos[nIndex].m_nImageId = CBaseModPanel::GetSingleton().GetImageId( pImageName );
		}
	}

	pKV->deleteThis();
}

ExtraInfo_t *CExtrasDialog::GetExtraInfo( int nInfoIndex )
{
	if ( m_ExtraInfos.IsValidIndex( nInfoIndex ) )
	{
		return &m_ExtraInfos[nInfoIndex];
	}

	return NULL;
}

void CExtrasDialog::OnCommand( char const *szCommand )
{
	if ( !Q_strcmp( szCommand, "Back" ) )
	{
		// Act as though 360 back button was pressed
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		return;
	}

	BaseClass::OnCommand( szCommand );
}

void CExtrasDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	m_pInfoImage = dynamic_cast< ImagePanel* >( FindChildByName( "InfoImage" ) );

	m_nVignetteImageId = CBaseModPanel::GetSingleton().GetImageId( "vgui/chapters/vignette" );
	
	m_pInfoList->SetScrollBarVisible( false );

	for ( int i = 0; i < m_ExtraInfos.Count(); i++ )
	{
		CInfoListItem *pItem = m_pInfoList->AddPanelItem< CInfoListItem >( "extras_infoitem" );
		if ( pItem )
		{
			pItem->SetInfoIndex( i );
		}
	}

	if ( m_pInfoList->GetPanelItemCount() )
	{
		// auto-hide scroll bar when not necessary
		if ( IsPC() )
		{
			CInfoListItem *pItem = dynamic_cast< CInfoListItem* >( m_pInfoList->GetPanelItem( 0 ) );
			if ( pItem )
			{
				int nPanelItemTall = pItem->GetTall();
				if ( nPanelItemTall )
				{
					int nNumVisibleItems = m_pInfoList->GetTall()/nPanelItemTall;
					if ( m_pInfoList->GetPanelItemCount() > nNumVisibleItems )
					{
						m_pInfoList->SetScrollBarVisible( true );
					}
				}
			}
		}
	}

	// the image will get set based on the initial selection
	SetInfoImage( -1 );

	if ( m_ActiveControl )
	{
		m_ActiveControl->NavigateFrom();
	}
	m_pInfoList->NavigateTo();

	m_pInfoList->SelectPanelItem( 0, GenericPanelList::SD_DOWN, true, false );

	UpdateFooter();
}

void CExtrasDialog::Activate()
{
	BaseClass::Activate();

	m_pInfoList->NavigateTo();

	UpdateFooter();
}

void CExtrasDialog::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		int visibleButtons = FB_BBUTTON;
		if ( m_ExtraInfos.Count() )
		{
			visibleButtons |= FB_ABUTTON;
		}

		pFooter->SetButtons( visibleButtons );
		pFooter->SetButtonText( FB_ABUTTON, "#PORTAL2_ButtonAction_Play" );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
	}
}

void CExtrasDialog::OnItemSelected( const char *pPanelName )
{
	if ( !m_bLayoutLoaded )
		return;

	CInfoListItem* pListItem = static_cast< CInfoListItem* >( m_pInfoList->GetSelectedPanelItem() );
	SetInfoImage( pListItem ? pListItem->GetInfoIndex() : -1 );

#if !defined( _GAMECONSOLE )
	// Set active state
	for ( int i = 0; i < m_pInfoList->GetPanelItemCount(); i++ )
	{
		CInfoListItem *pItem = dynamic_cast< CInfoListItem* >( m_pInfoList->GetPanelItem( i ) );
		if ( pItem )
		{
			pItem->SetSelected( pItem == pListItem );
		}
	}
#endif

	m_ActiveControl = pListItem;

	UpdateFooter();
}

void CExtrasDialog::SetInfoImage( int nInfoIndex )
{
	if ( m_pInfoLabel )
	{
		m_pInfoLabel->SetVisible( nInfoIndex >= 0 );		
		m_pInfoLabel->SetInfoIndex( nInfoIndex );
	}

	if ( nInfoIndex >= 0 )
	{
		m_nInfoImageId = m_ExtraInfos[nInfoIndex].m_nImageId;
	}
	else
	{
		m_nInfoImageId = -1;
	}
}

void CExtrasDialog::PaintBackground()
{
	BaseClass::PaintBackground();

	DrawInfoImage();
}

void CExtrasDialog::DrawInfoImage()
{
	if ( !m_pInfoImage || m_nInfoImageId == -1 )
		return;

	int x, y, wide, tall;
	m_pInfoImage->GetBounds( x, y, wide, tall );

	surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
	surface()->DrawSetTexture( m_nInfoImageId );
	surface()->DrawTexturedRect( x, y, x+wide, y+tall );
	surface()->DrawSetTexture( m_nVignetteImageId );
	surface()->DrawTexturedRect( x, y, x+wide, y+tall );
}

void CExtrasDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( !IsGameConsole() )
	{
		// handle button presses by the footer
		CInfoListItem* pListItem = static_cast< CInfoListItem* >( m_pInfoList->GetSelectedPanelItem() );
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
