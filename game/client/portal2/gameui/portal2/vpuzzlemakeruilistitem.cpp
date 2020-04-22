//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#include "cbase.h"
#include "vpuzzlemakeruilistitem.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "EngineInterface.h"
#include "IGameUIFuncs.h"
#include "gameui_util.h"
#include "vgui/ISurface.h"
#include "VGenericConfirmation.h"
#include "VGenericPanelList.h"
#include "gameui/portal2/vdialoglistbutton.h"
#include "vpuzzlemakermychambers.h" // CPuzzleMakerUIPanel

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( PORTAL2_PUZZLEMAKER )

using namespace vgui;
using namespace BaseModUI;


BaseModUI::CPuzzleMakerUIListItem::CPuzzleMakerUIListItem( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName ),
	m_pListCtrlr( ( GenericPanelList *)pParent )
{
	m_pParentPanel = static_cast< CPuzzleMakerUIPanel*>( m_pListCtrlr->GetParent() );

	SetProportional( true );
	SetPaintBackgroundEnabled( true );

	//m_hTextFont = vgui::INVALID_FONT;

	m_pLblName = NULL;
	m_szPrimaryLabelText[0] = '\0';
	m_bSelected = false;
	m_bHasMouseover = false;
}


void BaseModUI::CPuzzleMakerUIListItem::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/BaseModUI/puzzlemaker_listitem.res" );

	m_TextColor = GetSchemeColor( "HybridButton.TextColorAlt", pScheme );
	m_FocusColor = GetSchemeColor( "HybridButton.FocusColorAlt", pScheme );
	m_CursorColor = GetSchemeColor( "HybridButton.CursorColorAlt", pScheme );
	m_LockedColor = Color( 64, 64, 64, 128 );//GetSchemeColor( "HybridButton.LockedColor", pScheme );
	m_MouseOverCursorColor = GetSchemeColor( "HybridButton.MouseOverCursorColorALt", pScheme );
	m_LostFocusColor = m_CursorColor;
	m_LostFocusColor.SetColor( m_LostFocusColor.r(), m_LostFocusColor.g(), m_LostFocusColor.b(), 50); //Color( 120, 120, 120, 255 );
	m_BaseColor = Color( 255, 255, 255, 0 );

	m_pLblName = dynamic_cast< vgui::Label * >( FindChildByName( "LblItemName" ) );

	/*if ( m_pLblName )
	{
		m_hTextFont = m_pLblName->GetFont();
	}*/
}


void BaseModUI::CPuzzleMakerUIListItem::PaintBackground()
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

	// set the colors for the labels
	if (m_pLblName )
	{
		if ( HasFocus() || IsSelected() || HasMouseover() )
		{
			m_pLblName->SetFgColor( m_FocusColor );
		}
		else
		{
			m_pLblName->SetFgColor( m_TextColor );
		}
	}

	DrawListItemLabel( m_pLblName );

}


void BaseModUI::CPuzzleMakerUIListItem::OnCursorEntered()
{
	SetHasMouseover( true );

	if ( IsPC() )
		return;

	if ( GetParent() )
		GetParent()->NavigateToChild( this );
	else
		NavigateTo();
}


void BaseModUI::CPuzzleMakerUIListItem::NavigateTo()
{
	m_pListCtrlr->SelectPanelItemByPanel( this );

	SetHasMouseover( true );
	RequestFocus();
	int nNumPanels = m_pListCtrlr->GetPanelItemCount();
	for ( int i = 0; i < nNumPanels; ++i )
	{
		CChamberListItem *pPanel = static_cast< CChamberListItem *>( m_pListCtrlr->GetPanelItem( i ) );
		if ( pPanel )
		{
			pPanel->SetSelected( false );
		}
	}
	SetSelected( true );

	BaseClass::NavigateTo();
}


void BaseModUI::CPuzzleMakerUIListItem::NavigateFrom()
{
	SetHasMouseover( false );

	BaseClass::NavigateFrom();
}


void BaseModUI::CPuzzleMakerUIListItem::OnKeyCodePressed( vgui::KeyCode code )
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
		break;
	}

	m_pParentPanel->UpdateFooter();

	BaseClass::OnKeyCodePressed( code );
}


void BaseModUI::CPuzzleMakerUIListItem::OnMousePressed( vgui::MouseCode code )
{
	if ( code == MOUSE_LEFT )
	{
		if ( GetParent() )
			GetParent()->NavigateToChild( this );
		else
			NavigateTo();

		Assert( m_pParentPanel );
		if( m_pParentPanel )
		{
			m_pParentPanel->UpdateFooter();
		}
		return;
	}

	BaseClass::OnMousePressed( code );
	m_pParentPanel->UpdateFooter();
}


void BaseModUI::CPuzzleMakerUIListItem::OnMouseDoublePressed( vgui::MouseCode code )
{
	if ( code == MOUSE_LEFT )
	{
		CChamberListItem* pListItem = static_cast< CChamberListItem* >( m_pListCtrlr->GetSelectedPanelItem() );
		if ( pListItem )
		{
			OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		}

		return;
	}

	BaseClass::OnMouseDoublePressed( code );
}


void BaseModUI::CPuzzleMakerUIListItem::PerformLayout()
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


void BaseModUI::CPuzzleMakerUIListItem::SetHasMouseover( bool bHasMouseover )
{
	if ( bHasMouseover )
	{
		for ( int i = 0; i < m_pListCtrlr->GetPanelItemCount(); i++ )
		{
			CPuzzleMakerUIListItem *pItem = dynamic_cast< CPuzzleMakerUIListItem* >( m_pListCtrlr->GetPanelItem( i ) );
			if ( pItem && pItem != this )
			{
				pItem->SetHasMouseover( false );
			}
		}
	}
	m_bHasMouseover = bHasMouseover;
}


void BaseModUI::CPuzzleMakerUIListItem::SetPrimaryText( const char *pText )
{
	// store the string for easy retrieval
	V_strncpy( m_szPrimaryLabelText, pText, sizeof( m_szPrimaryLabelText ) );
	// set the actual label text
	if ( m_pLblName )
	{
		m_pLblName->SetText( pText );
	}
}


void BaseModUI::CPuzzleMakerUIListItem::DrawListItemLabel( Label *pLabel )
{
	if ( !pLabel )
		return;

	bool bHasFocus = HasFocus() || IsSelected();

	// set whether locked or normal text color
	Color textColor = bHasFocus ? m_FocusColor : m_TextColor;

	int panelWide, panelTall;
	GetSize( panelWide, panelTall );

	int x, y, labelWide, labelTall;
	pLabel->GetBounds( x, y, labelWide, labelTall );

	wchar_t szUnicode[512];
	pLabel->GetText( szUnicode, sizeof( szUnicode ) );
	int len = V_wcslen( szUnicode );

	HFont textFont = pLabel->GetFont();

	int textWide, textTall;
	surface()->GetTextSize( textFont, szUnicode, textWide, textTall );

	// vertical center
	y += ( labelTall - textTall ) / 2;

	

	vgui::surface()->DrawSetTextFont( textFont );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawSetTextColor( textColor );
	vgui::surface()->DrawPrintText( szUnicode, len );
}


//void BaseModUI::CPuzzleMakerUIListItem::SetChamberName( const char *pChamberName )
//{
//	if ( !m_pLblChamberName )
//	{
//		return;
//	}
//
//	m_pLblChamberName->SetText( pChamberName );
//}
//
//
//void BaseModUI::CPuzzleMakerUIListItem::SetChamberStatus( const char *pChamberStatus )
//{
//	if ( !m_pLblChamberStatus )
//	{
//		return;
//	}
//
//	m_pLblChamberStatus->SetText( pChamberStatus );
//}

#endif // PORTAL2_PUZZLEMAKER
