//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VHybridButton.h"
#include "basemodpanel.h"
#include "VFooterPanel.h"
#include "VFlyoutMenu.h"
#include "EngineInterface.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Tooltip.h"
#include "vgui/IVgui.h"
#include "tier1/KeyValues.h"
#include "vgui/ilocalize.h"
#include "vgui/IInput.h"
#include "VDropDownMenu.h"
#include "VSliderControl.h"
#include "gamemodes.h"

#ifndef _X360
#include <ctype.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace BaseModUI;
using namespace vgui;

ConVar ui_virtualnav_render( "ui_virtualnav_render", "0", FCVAR_DEVELOPMENTONLY );

DECLARE_BUILD_FACTORY_DEFAULT_TEXT( BaseModHybridButton, HybridButton );

void Demo_DisableButton( Button *pButton )
{
	BaseModHybridButton *pHybridButton = dynamic_cast<BaseModHybridButton *>(pButton);

	if (pHybridButton)
	{
		pHybridButton->SetEnabled( false );

		char szTooltip[512];
		wchar_t *wUnicode = g_pVGuiLocalize->Find( "#L4D360UI_MainMenu_DemoVersion" );
		if ( !wUnicode )
			wUnicode = L"";

		g_pVGuiLocalize->ConvertUnicodeToANSI( wUnicode, szTooltip, sizeof( szTooltip ) );
	}
}

void Dlc1_DisableButton( Button *pButton )
{
	BaseModHybridButton *pHybridButton = dynamic_cast<BaseModHybridButton *>(pButton);

	if (pHybridButton)
	{
		pHybridButton->SetEnabled( false );

		char szTooltip[512];
		wchar_t *wUnicode = g_pVGuiLocalize->Find( "#L4D360UI_DLC1_NotInstalled" );

		if ( !wUnicode )
			wUnicode = L"";

		g_pVGuiLocalize->ConvertUnicodeToANSI( wUnicode, szTooltip, sizeof( szTooltip ) );
	}
}

struct HybridEnableStates
{
	BaseModUI::BaseModHybridButton::EnableCondition mCondition;
	char mConditionName[64];
};

HybridEnableStates sHybridStates[] = 
{
	{ BaseModUI::BaseModHybridButton::EC_LIVE_REQUIRED,	"LiveRequired" },
	{ BaseModUI::BaseModHybridButton::EC_NOTFORDEMO,	"Never" }
};

//=============================================================================
// Constructor / Destructor
//=============================================================================

BaseModUI::BaseModHybridButton::BaseModHybridButton( Panel *parent, const char *panelName, const char *text, Panel *pActionSignalTarget, const char *pCmd )
	: BaseClass( parent, panelName, text, pActionSignalTarget, pCmd ),
	m_clrEnabledOverride( 0, 0, 0, 0 )
{
	SetPaintBorderEnabled( false );
	SetPaintBackgroundEnabled( false );
	SetContentAlignment( a_northwest );
	SetClosed();
	SetButtonActivationType( ACTIVATE_ONRELEASED );
	SetConsoleStylePanel( true );

	m_iUsablePlayerIndex = USE_EVERYBODY;
	m_isNavigateTo = false;
	m_bOnlyActiveUser = false;
	m_bIgnoreButtonA = false;

	mEnableCondition = EC_ALWAYS;

	m_nStyle = BUTTON_DEFAULT;

	m_hTextFont = vgui::INVALID_FONT;
	m_hSymbolFont = vgui::INVALID_FONT;

	m_originalTall = 0;
	m_textInsetX = 0;
	m_textInsetY = 0;
	m_nListInsetX = 0;

	m_iSelectedArrow = -1;
	m_iUnselectedArrow = -1;

	m_nWideAtOpen = 0;

	m_bAllCaps = false;
	m_bPostCheckMark = false;
	m_bUseAlternateTiles = false;

	m_nCursorHeight = 0;
	m_nMultiline = 0;

	m_nDialogListCurrentIndex = 0;

	m_nEnabledImageId = -1;
	m_nFocusImageId = -1;

	m_flLastBitmapAnimTime = 0;
	m_nBitmapFrame = 0;
	m_nBitmapFrameCache = 0;
}

BaseModUI::BaseModHybridButton::BaseModHybridButton( Panel *parent, const char *panelName, const wchar_t *text, Panel *pActionSignalTarget, const char *pCmd )
	: BaseClass( parent, panelName, text, pActionSignalTarget, pCmd ),
	m_clrEnabledOverride( 0, 0, 0, 0 )
{
	SetPaintBorderEnabled( false );
	SetPaintBackgroundEnabled( false );
	SetContentAlignment( a_northwest );
	SetClosed();
	SetButtonActivationType( ACTIVATE_ONRELEASED );

	m_iUsablePlayerIndex = USE_EVERYBODY;
	m_isNavigateTo = false;
	m_iUsablePlayerIndex = -1;

	mEnableCondition = EC_ALWAYS;

	m_nStyle = BUTTON_DEFAULT;

	m_hTextFont = vgui::INVALID_FONT;
	m_hSymbolFont = vgui::INVALID_FONT;

	m_originalTall = 0;
	m_textInsetX = 0;
	m_textInsetY = 0;
	m_nListInsetX = 0;

	m_iSelectedArrow = -1;
	m_iUnselectedArrow = -1;

	m_bAllCaps = false;
	m_bPostCheckMark = false;

	m_nCursorHeight = 0;
	m_nMultiline = 0;

	m_nDialogListCurrentIndex = 0;

	m_nEnabledImageId = -1;
	m_nFocusImageId = -1;

	m_flLastBitmapAnimTime = 0;
	m_nBitmapFrame = 0;
	m_nBitmapFrameCache = 0;
}

BaseModUI::BaseModHybridButton::~BaseModHybridButton()
{
	// purposely not destroying our textures
	// otherwise i/o constantly on these
}

BaseModHybridButton::State BaseModHybridButton::GetCurrentState()
{
	State curState = Enabled;
	if ( IsPC() )
	{
		if ( HasFocus() )
		{
			curState = IsEnabled() ? Focus : FocusDisabled;
		}
	}
	if ( m_isOpen )
	{
		curState = Open;
	}
	else if ( IsArmed() || m_isNavigateTo ) //NavigateTo doesn't instantly give focus to the control
	{										//so this little boolean value is meant to let us know we should have focus for the "focus state"
		if ( IsEnabled() )					//until vgui catches up and registers us as having focus.
		{
			curState = Focus;
		}
		else
		{
			curState = FocusDisabled;
		}

		if ( IsArmed() )
		{
			m_isNavigateTo = false;
		}
	}
	else if ( !IsEnabled() )
	{
		curState = Disabled;
	}

	return curState;
}

void BaseModHybridButton::SetOpen()
{
	if ( m_isOpen )
		return;
	m_isOpen = true;
	if ( IsPC() )
	{
		PostMessageToAllSiblingsOfType< BaseModHybridButton >( new KeyValues( "OnSiblingHybridButtonOpened" ) );
	}
}

void BaseModHybridButton::SetClosed()
{
	if ( !m_isOpen )
		return;

	m_isOpen = false;
}

void BaseModHybridButton::OnSiblingHybridButtonOpened()
{
	if ( !IsPC() )
		return;

	bool bClosed = false;

	FlyoutMenu *pActiveFlyout = FlyoutMenu::GetActiveMenu();

	if ( pActiveFlyout )
	{
		BaseModHybridButton *button = dynamic_cast< BaseModHybridButton* >( pActiveFlyout->GetNavFrom() );
		if ( button && button == this )
		{
			// We need to close the flyout attached to this button
			FlyoutMenu::CloseActiveMenu();
			bClosed = true;
		}
	}

	if ( !bClosed )
	{
		SetClosed();
	}

	m_isNavigateTo = false;
}

void BaseModHybridButton::OnMousePressed( vgui::MouseCode code )
{
	BaseClass::OnMousePressed( code );
	if ( IsPC() )
	{
		if ( code == MOUSE_RIGHT )
		{
			FlyoutMenu::CloseActiveMenu( this );
		}
		else
		{
			if ( ( code == MOUSE_LEFT ) && !IsEnabled() && ( dynamic_cast<FlyoutMenu *>( GetParent() ) == NULL ) )
			{
				// when trying to use an inactive item that isn't part of a flyout. Close any open flyouts.
				FlyoutMenu::CloseActiveMenu( this );
			}

			if ( code == MOUSE_LEFT && m_nStyle == BUTTON_DIALOGLIST )
			{
				ListSelectionChange_t selectionDir = SELECT_NEXT;

				int nButtonX, nButtonY;
				if ( GetDialogListButtonCenter( nButtonX, nButtonY ) )
				{
					int iPosX;
					int iPosY;
					input()->GetCursorPos( iPosX, iPosY );
					ScreenToLocal( iPosX, iPosY );
					if ( iPosX < nButtonX )
					{
						selectionDir = SELECT_PREV;
					}
				}

				ChangeDialogListSelection( selectionDir );
			}

			RequestFocus( 0 );			
		}
	}
}

void BaseModHybridButton::NavigateTo()
{
	if ( !IsEnabled() )
	{
		vgui::Panel *pPanel = NULL;
		Panel::NAV_DIRECTION lastNavDirection = GetLastNavDirection();
		switch ( lastNavDirection )
		{
		case ND_UP:
			pPanel = this;
			do
			{
				pPanel = pPanel->GetNavUp();
				if ( !pPanel || ( pPanel->IsEnabled() && pPanel->IsVisible() ) )
				{
					break;
				}
			} 
			while ( pPanel != this );
			break;

		case ND_DOWN:
		default:
			pPanel = this;
			do
			{
				pPanel = pPanel->GetNavDown();
				if ( !pPanel || ( pPanel->IsEnabled() && pPanel->IsVisible() ) )
				{
					break;
				}
			}
			while ( pPanel != this );
			break;
		}

		if ( pPanel )
		{
			NavigateFrom();
			pPanel->NavigateTo();
		}

		return;
	}

	BaseClass::NavigateTo();

	FlyoutMenu* parentMenu = dynamic_cast< FlyoutMenu* >( GetParent() );
	if( parentMenu )
	{
		parentMenu->NotifyChildFocus( this );
	}

	if (GetVParent())
	{
		KeyValues *msg = new KeyValues("OnHybridButtonNavigatedTo");
		msg->SetInt("button", ToHandle() );

		ivgui()->PostMessage(GetVParent(), msg, GetVPanel());
	}

	m_isNavigateTo = true;
	if ( IsPC() )
	{
		RequestFocus( 0 );
	}
}

void BaseModHybridButton::NavigateFrom()
{
	BaseClass::NavigateFrom();

	m_isNavigateTo = false;
}

bool BaseModHybridButton::GetDialogListButtonCenter( int &x, int &y )
{
	if ( m_nStyle != BUTTON_DIALOGLIST || !m_DialogListItems.IsValidIndex( m_nDialogListCurrentIndex ) )
		return false;

	wchar_t szUnicode[512];
	wchar_t *pLocalizedString = g_pVGuiLocalize->Find( m_DialogListItems[m_nDialogListCurrentIndex].m_String.Get() );
	if ( !pLocalizedString )
	{
		if ( !m_DialogListItems[m_nDialogListCurrentIndex].m_StringParm1.IsEmpty() )
		{
			wchar_t szTempUnicode[512];
			g_pVGuiLocalize->ConvertANSIToUnicode( m_DialogListItems[m_nDialogListCurrentIndex].m_String.Get(), szTempUnicode, sizeof( szTempUnicode ) );

			wchar_t *pParm1String = g_pVGuiLocalize->Find( m_DialogListItems[m_nDialogListCurrentIndex].m_StringParm1.Get() );
			if ( !pParm1String )
			{
				pParm1String = L"";
			}

			V_snwprintf( szUnicode, ARRAYSIZE( szUnicode ), szTempUnicode, pParm1String );
		}
		else
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( m_DialogListItems[m_nDialogListCurrentIndex].m_String.Get(), szUnicode, sizeof( szUnicode ) );
		}
	}
	else
	{
		Q_wcsncpy( szUnicode, pLocalizedString, sizeof( szUnicode ) );
	}

	int textWide, textTall;
	surface()->GetTextSize( m_hTextFont, szUnicode, textWide, textTall );

	x = ( GetWide() - m_textInsetX - textWide );

	if ( IsPC() && m_hSymbolFont != vgui::INVALID_FONT )
	{
		int nArrowWide, nArrowTall;
		vgui::surface()->GetTextSize( m_hSymbolFont, L"3", nArrowWide, nArrowTall );
		x -= nArrowWide;
	}
	else
	{
		int nArrowSize = vgui::surface()->GetCharacterWidth( m_hTextFont, L'<' );
		x -= 2 * nArrowSize;
	}

	x += textWide/2;
	y = GetTall()/2;

	return true;
}

void BaseModHybridButton::DrawDialogListButton( Color textColor )
{
	if ( !m_DialogListItems.IsValidIndex( m_nDialogListCurrentIndex ) )
		return;

	wchar_t szUnicode[512];
	wchar_t *pLocalizedString = g_pVGuiLocalize->Find( m_DialogListItems[m_nDialogListCurrentIndex].m_String.Get() );
	if ( !pLocalizedString )
	{
		if ( !m_DialogListItems[m_nDialogListCurrentIndex].m_StringParm1.IsEmpty() )
		{
			wchar_t szTempUnicode[512];
			g_pVGuiLocalize->ConvertANSIToUnicode( m_DialogListItems[m_nDialogListCurrentIndex].m_String.Get(), szTempUnicode, sizeof( szTempUnicode ) );

			wchar_t *pParm1String = g_pVGuiLocalize->Find( m_DialogListItems[m_nDialogListCurrentIndex].m_StringParm1.Get() );
			if ( !pParm1String )
			{
				pParm1String = L"";
			}

			V_snwprintf( szUnicode, ARRAYSIZE( szUnicode ), szTempUnicode, pParm1String );
		}
		else
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( m_DialogListItems[m_nDialogListCurrentIndex].m_String.Get(), szUnicode, sizeof( szUnicode ) );
		}
	}
	else
	{
		Q_wcsncpy( szUnicode, pLocalizedString, sizeof( szUnicode ) );
	}

	int len = V_wcslen( szUnicode );

	int textWide, textTall;
	surface()->GetTextSize( m_hTextFont, szUnicode, textWide, textTall );
	
	int x = GetWide() - m_textInsetX - textWide;

	Color leftCursorColor = IsGameConsole() ? textColor : m_ListButtonInactiveColor;
	Color rightCursorColor = IsGameConsole() ? textColor : m_ListButtonInactiveColor;

	if ( GetCurrentState() == Focus && m_DialogListItems.Count() )
	{
		if ( IsPC() )
		{
			if ( m_nStyle == BUTTON_DIALOGLIST )
			{
				// dialog list buttons highlight their L/R depending on mouse position
				int nCursorPosX;
				int nCursorPosY;
				input()->GetCursorPos( nCursorPosX, nCursorPosY );
				ScreenToLocal( nCursorPosX, nCursorPosY );
				if ( nCursorPosY >= 0 && nCursorPosY <= GetTall() )
				{
					int nButtonCenterX, nButtonCenterY;
					if ( GetDialogListButtonCenter( nButtonCenterX, nButtonCenterY ) )
					{
						if ( nCursorPosX < nButtonCenterX )
						{
							leftCursorColor = m_ListButtonActiveColor;
						}
						else
						{
							rightCursorColor = m_ListButtonActiveColor;
						}
					}
				}
			}
			else
			{
				rightCursorColor = m_ListButtonActiveColor;
			}
		}

		if ( IsPC() && m_hSymbolFont != vgui::INVALID_FONT )
		{
			// pc can support a symbol font and use real arrows
			int nArrowWide, nArrowTall;
			vgui::surface()->GetTextSize( m_hSymbolFont, L"3", nArrowWide, nArrowTall );

			x -= nArrowWide;
			int nArrowInsetY = ( GetTall() - nArrowTall ) / 2;

			vgui::surface()->DrawSetTextFont( m_hSymbolFont );

			if ( m_nStyle == BUTTON_DIALOGLIST )
			{
				vgui::surface()->DrawSetTextColor( leftCursorColor );
				vgui::surface()->DrawSetTextPos( x - nArrowWide, nArrowInsetY );
				vgui::surface()->DrawUnicodeString( L"3" );
			}

			vgui::surface()->DrawSetTextColor( rightCursorColor );
			vgui::surface()->DrawSetTextPos( x + textWide, nArrowInsetY );
			vgui::surface()->DrawUnicodeString( L"4" );
		}
		else
		{
			int nArrowSize = vgui::surface()->GetCharacterWidth( m_hTextFont, L'<' );
			x -= 2 * nArrowSize;

			vgui::surface()->DrawSetTextFont( m_hTextFont );

			if ( m_nStyle == BUTTON_DIALOGLIST )
			{
				vgui::surface()->DrawSetTextColor( leftCursorColor );
				vgui::surface()->DrawSetTextPos( x - 2 * nArrowSize, m_textInsetY );
				vgui::surface()->DrawUnicodeString( L"<" );
			}

			vgui::surface()->DrawSetTextColor( rightCursorColor );
			vgui::surface()->DrawSetTextPos( x + textWide + nArrowSize, m_textInsetY );
			vgui::surface()->DrawUnicodeString( L">" );
		}
	}

	vgui::surface()->DrawSetTextFont( m_hTextFont );
	vgui::surface()->DrawSetTextPos( x, m_textInsetY  );
	vgui::surface()->DrawSetTextColor( textColor );

	int availableWidth = textWide + m_textInsetX;
	if ( textWide > availableWidth )
	{
		// length of 3 dots
		int ellipsesLen = 3 * vgui::surface()->GetCharacterWidth( m_hTextFont, L'.' );
		availableWidth -= ellipsesLen;

		// draw as much as possible
		for ( int i = 0; i < len; i++ )
		{
			vgui::surface()->DrawUnicodeChar( szUnicode[i] );
			availableWidth -= vgui::surface()->GetCharacterWidth( m_hTextFont, szUnicode[i] );
			if ( availableWidth <= 0 )
				break;
		}
		
		// finish with ...
		vgui::surface()->DrawPrintText( L"...", 3 );
	}
	else
	{
		vgui::surface()->DrawUnicodeString( szUnicode );
	}
}

void BaseModHybridButton::PaintButtonEx()
{
	if ( m_nStyle == BUTTON_VIRTUALNAV && !ui_virtualnav_render.GetBool() )
	{
		// this is invisible button simply driving the virtual navigation
		return;
	}

	int wide, tall;
	GetSize( wide, tall );

	// due to vertical resizing, center within the control
	int x = 0;
	int y = 0; //( tall - m_originalTall ) / 2;
//	tall = m_originalTall;

	if ( ( m_nStyle == BUTTON_DROPDOWN || m_nStyle == BUTTON_GAMEMODE ) && GetCurrentState() == Open && m_nWideAtOpen )
	{
		wide = m_nWideAtOpen;
	}

	bool bDrawCursor = false;
	Color textColor;

	State curState = GetCurrentState();
	switch ( curState )
	{
	case Enabled:
		// selectable, just not highlighted
		textColor = m_TextColor;
		if ( m_clrEnabledOverride.GetRawColor() )
		{
			textColor = m_clrEnabledOverride;
		}
		bDrawCursor = false;
		break;

	case Disabled:
		textColor = m_DisabledColor;
		bDrawCursor = false;
		break;

	case FocusDisabled:
		textColor = m_FocusDisabledColor;
		bDrawCursor = false;
		break;

	case Open:
		// flyout menu is attached
		textColor = m_TextColor;
		bDrawCursor = true;
		break;

	case Focus:
		// active item
		textColor = m_FocusColor;
		bDrawCursor = true;
		break;
	}

	if ( m_nStyle == BUTTON_BITMAP )
	{
		surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
		if ( curState == Focus )
		{
			surface()->DrawSetTextureFrame( m_nFocusImageId, m_nBitmapFrame, &m_nBitmapFrameCache );
			surface()->DrawSetTexture( m_nFocusImageId );
		}
		else
		{
			surface()->DrawSetTexture( m_nEnabledImageId );
		}

		vgui::surface()->DrawTexturedRect( 0, 0, tall, tall );

		x += tall;

		bDrawCursor = false;
	}

	wchar_t szUnicode[512];
	GetText( szUnicode, sizeof( szUnicode ) );
	int len = V_wcslen( szUnicode );

	if ( m_bAllCaps )
	{
		for ( int i = len - 1; i >= 0; --i )
		{
			szUnicode[ i ] = towupper( szUnicode[ i ] );
		}
	}

	int textWide, textTall;
	surface()->GetTextSize( m_hTextFont, szUnicode, textWide, textTall );
	textWide = clamp( textWide, 0, wide - m_textInsetX * 2 );
	textTall = clamp( textTall, 0, tall - m_textInsetY * 2 );

	if ( bDrawCursor )
	{
		surface()->DrawSetColor( m_CursorColor );
		int cursorY = ( tall - m_nCursorHeight ) / 2;
		surface()->DrawFilledRect( x, cursorY, x+wide, cursorY + m_nCursorHeight );
	}

	if ( m_nStyle == BUTTON_LEFTINDIALOG && m_bPostCheckMark && m_hSymbolFont != vgui::INVALID_FONT )
	{
		int nCheckWide, nCheckTall;
		vgui::surface()->GetTextSize( m_hSymbolFont, L"a", nCheckWide, nCheckTall );

		int nArrowInsetY = ( tall - nCheckTall ) / 2;

		vgui::surface()->DrawSetTextFont( m_hSymbolFont );
		vgui::surface()->DrawSetTextColor( textColor );
		vgui::surface()->DrawSetTextPos( x + m_textInsetX + textWide + nCheckWide/4, nArrowInsetY );
		vgui::surface()->DrawUnicodeString( L"a" );
	}

	vgui::surface()->DrawSetTextFont( m_hTextFont );
	vgui::surface()->DrawSetTextPos( x + m_textInsetX, y + m_textInsetY  );
	vgui::surface()->DrawSetTextColor( textColor );

	if ( m_nStyle == BUTTON_MIXEDCASE )
	{
		wchar_t *pwszLine = szUnicode;
		int nLine = 0;
		while ( wchar_t *pwszNewLine = wcschr( pwszLine, L'\n' ) )
		{
			*pwszNewLine = 0;
			vgui::surface()->DrawSetTextPos( x + m_textInsetX, y + m_textInsetY + nLine*m_nMultiline );
			vgui::surface()->DrawUnicodeString( pwszLine );
			pwszLine = pwszNewLine + 1;
			++ nLine;
		}
		vgui::surface()->DrawSetTextPos( x + m_textInsetX, y + m_textInsetY + nLine*m_nMultiline );
		vgui::surface()->DrawUnicodeString( pwszLine );
	}
	else
	{
		// assume drawn, unless otherwise shortened with ellipsis
		int availableWidth = GetWide() - x - m_textInsetX;
		if ( textWide > availableWidth )
		{
			// length of 3 dots
			int ellipsesLen = 3 * vgui::surface()->GetCharacterWidth( m_hTextFont, L'.' );
			availableWidth -= ellipsesLen;

			// draw as much as possible
			for ( int i = 0; i < len; i++ )
			{
				vgui::surface()->DrawUnicodeChar( szUnicode[i] );
				availableWidth -= vgui::surface()->GetCharacterWidth( m_hTextFont, szUnicode[i] );
				if ( availableWidth <= 0 )
					break;
			}

			// finish with ...
			vgui::surface()->DrawPrintText( L"...", 3 );
		}
		else
		{
			vgui::surface()->DrawUnicodeString( szUnicode );
		}
	}

	if ( m_nStyle == BUTTON_LEFTINDIALOG || m_nStyle == BUTTON_DIALOGLIST )
	{
		DrawDialogListButton( textColor );
	}
}

void BaseModHybridButton::Paint()
{
	// bypass ALL of CA's inept broken drawing code
	// not using VGUI controls, to much draw state is misconfigured by CA to salvage
	PaintButtonEx();

	// only do the forced selection for a single frame.
	m_isNavigateTo = false;
}

void BaseModHybridButton::OnThink()
{
	switch( mEnableCondition )
	{
	case EC_LIVE_REQUIRED:
		{
#ifdef _GAMECONSOLE
			SetEnabled( CUIGameData::Get()->SignedInToLive() );
#else
			SetEnabled( true );
#endif
		}	
		break;
	case EC_NOTFORDEMO:
		{
			if ( IsEnabled() )
			{
				Demo_DisableButton( this );
			}
		}
		break;
	}

	if ( m_nStyle == BUTTON_BITMAP && GetCurrentState() == Focus )
	{
		// clock the anim at 10hz
		float time = Plat_FloatTime();
		if ( ( m_flLastBitmapAnimTime + 0.08f ) < time )
		{
			m_flLastBitmapAnimTime = time;
			m_nBitmapFrame++;
		}
	}

	BaseClass::OnThink();
}

void BaseModHybridButton::ApplySettings( KeyValues *pInResourceData )
{
	BaseClass::ApplySettings( pInResourceData );

	vgui::IScheme *pScheme = vgui::scheme()->GetIScheme( GetScheme() );
	if ( !pScheme )
		return;

	m_TextColor = GetSchemeColor( "HybridButton.TextColor", pScheme );
	m_FocusColor = GetSchemeColor( "HybridButton.FocusColor", pScheme );
	m_CursorColor = GetSchemeColor( "HybridButton.CursorColor", pScheme );
	m_DisabledColor = GetSchemeColor( "HybridButton.DisabledColor", pScheme );
	m_FocusDisabledColor = GetSchemeColor( "HybridButton.FocusDisabledColor", pScheme );
	m_ListButtonActiveColor = GetSchemeColor( "HybridButton.ListButtonActiveColor", pScheme );
	m_ListButtonInactiveColor = GetSchemeColor( "HybridButton.ListButtonInactiveColor", pScheme );

	const char *pDefaultFontString = pScheme->GetResourceString( "HybridButton.Font" );
	const char *pDefaultSymbolFontString = pScheme->GetResourceString( "HybridButton.SymbolFont" );

	m_bAllCaps = atoi( pScheme->GetResourceString( "HybridButton.AllCaps" ) ) != 0;

	m_textInsetX = atoi( pScheme->GetResourceString( "HybridButton.TextInsetX" ) );
	m_textInsetX = vgui::scheme()->GetProportionalScaledValue( m_textInsetX );

	m_textInsetY = atoi( pScheme->GetResourceString( "HybridButton.TextInsetY" ) );
	m_textInsetY = vgui::scheme()->GetProportionalScaledValue( m_textInsetY );

	m_nCursorHeight = atoi( pScheme->GetResourceString( "HybridButton.CursorHeight" ) );
	m_nCursorHeight = vgui::scheme()->GetProportionalScaledValue( m_nCursorHeight );

	m_nMultiline = atoi( pScheme->GetResourceString( "HybridButton.MultiLine" ) );
	m_nMultiline = vgui::scheme()->GetProportionalScaledValue( m_nMultiline );

	// if a style is specified attempt to load values in from the SCHEME file
	const char *pStyle = pInResourceData->GetString( "Style", NULL );
	if ( !pStyle )
	{
		pStyle = "DefaultButton";
	}

	m_nStyle = BUTTON_DEFAULT;
	const char *pFormatString = pScheme->GetResourceString( CFmtStr( "%s.Style", pStyle ) );
	if ( pFormatString && pFormatString[0] )
	{
		m_nStyle = (ButtonStyle_t)atoi( pFormatString );
	}

	m_TextColor = GetSchemeColor( CFmtStr( "%s.TextColor", pStyle ), m_TextColor, pScheme );
	m_FocusColor = GetSchemeColor( CFmtStr( "%s.FocusColor", pStyle ), m_FocusColor, pScheme );
	m_CursorColor = GetSchemeColor( CFmtStr( "%s.CursorColor", pStyle ), m_CursorColor, pScheme );
	m_DisabledColor = GetSchemeColor( CFmtStr( "%s.DisabledColor", pStyle ), m_DisabledColor, pScheme );
	m_FocusDisabledColor = GetSchemeColor( CFmtStr( "%s.FocusDisabledColor", pStyle ), m_FocusDisabledColor, pScheme );
	m_ListButtonActiveColor = GetSchemeColor( CFmtStr( "%s.ListButtonActiveColor", pStyle ), m_ListButtonActiveColor, pScheme );
	m_ListButtonInactiveColor = GetSchemeColor( CFmtStr( "%s.ListButtonInactiveColor", pStyle ), m_ListButtonInactiveColor, pScheme );

	// If the parent is an alternate style, change our colors to conform
	CBaseModFrame *pParent = dynamic_cast<CBaseModFrame *>(GetParent());
	if ( m_bUseAlternateTiles || ( pParent && pParent->UsesAlternateTiles() ) )
	{
		// set the alternate colors
		m_TextColor = GetSchemeColor( "HybridButton.TextColorAlt", pScheme );
		m_FocusColor = GetSchemeColor( "HybridButton.FocusColorAlt", pScheme );
		m_CursorColor = GetSchemeColor( "HybridButton.MouseOverCursorColorAlt", pScheme );
	}

	const char *pFontString = pScheme->GetResourceString( CFmtStr( "%s.Font", pStyle ) );
	m_hTextFont = pScheme->GetFont( pFontString[0] ? pFontString : pDefaultFontString, true );
	m_nTextFontHeight = vgui::surface()->GetFontTall( m_hTextFont );
	SetFont( m_hTextFont );

	const char *pSymbolFontString = pScheme->GetResourceString( CFmtStr( "%s.SymbolFont", pStyle ) );
	m_hSymbolFont = pScheme->GetFont( pSymbolFontString[0] ? pSymbolFontString : pDefaultSymbolFontString, true );

	// get the cursor height
	const char *pResult = pScheme->GetResourceString( CFmtStr( "%s.%s", pStyle, "CursorHeight" ) );
	if ( pResult[0] )
	{
		m_nCursorHeight = vgui::scheme()->GetProportionalScaledValue( atoi( pResult ) );
	}

	pResult = pScheme->GetResourceString( CFmtStr( "%s.%s", pStyle, "MultiLine" ) );
	if ( pResult[0] )
	{
		m_nMultiline = vgui::scheme()->GetProportionalScaledValue( atoi( pResult ) );
	}

	pResult = pScheme->GetResourceString( CFmtStr( "%s.%s", pStyle, "AllCaps" ) );
	if ( pResult[0] )
	{
		m_bAllCaps = atoi( pResult ) != 0;
	}

	pResult = pScheme->GetResourceString( CFmtStr( "%s.%s", pStyle, "ListInsetX" ) );
	if ( pResult[0] )
	{
		m_nListInsetX = vgui::scheme()->GetProportionalScaledValue( atoi( pResult ) );
	}

	m_nWideAtOpen = vgui::scheme()->GetProportionalScaledValue( pInResourceData->GetInt( "wideatopen", 0 ) );

	// Get the x text inset
	pResult = pScheme->GetResourceString( CFmtStr( "%s.%s", pStyle, "TextInsetX" ) );
	if ( pResult[0] )
	{
		m_textInsetX = vgui::scheme()->GetProportionalScaledValue( atoi( pResult ) );
	}

	// Get the y text inset
	pResult = pScheme->GetResourceString( CFmtStr( "%s.%s", pStyle, "TextInsetY" ) );
	if ( pResult[0] )
	{
		m_textInsetY = vgui::scheme()->GetProportionalScaledValue( atoi( pResult ) );
	}

	// vgui's standard handling of the tabPosition tag doesn't properly navigate for the X360
	if ( pInResourceData->GetInt( "tabPosition", 0 ) == 1 )
	{
		NavigateTo();
	}

	//0 = press and release
	//1 = press
	//2 = release
	int activationType = pInResourceData->GetInt( "ActivationType", IsPC() ? 1 : 2 );
	clamp( activationType, 0, 2 );
	SetButtonActivationType( static_cast< vgui::Button::ActivationType_t >( activationType ) );

	// it's a pain to specify all the button sizes when we don't need to
	// force the button height to be derived from the font
	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );
	m_originalTall = tall;

	if ( m_nStyle != BUTTON_BITMAP )
	{
		tall = MAX( m_nTextFontHeight, m_nCursorHeight );
		SetSize( wide, tall );
	}

	m_iUsablePlayerIndex = USE_EVERYBODY;
	if ( const char *pszValue = pInResourceData->GetString( "usablePlayerIndex", "" ) )
	{
		if ( !stricmp( "primary", pszValue ) )
		{
			m_iUsablePlayerIndex = USE_PRIMARY;
		}
		else if ( !stricmp( "nobody", pszValue ) )
		{
			m_iUsablePlayerIndex = USE_NOBODY;
		}
		else if ( isdigit( pszValue[0] ) )
		{
			m_iUsablePlayerIndex = atoi( pszValue );
		}
	}

	// handle different conditions to allow the control to be enabled and disabled automatically
	const char *pCondition = pInResourceData->GetString( "EnableCondition" );
	for ( int index = 0; index < ( sizeof( sHybridStates ) / sizeof( HybridEnableStates ) ); ++index )
	{
		if ( Q_stricmp( pCondition, sHybridStates[ index ].mConditionName ) == 0 )
		{
			mEnableCondition = sHybridStates[ index ].mCondition;
			break;
		}
	}

	if ( mEnableCondition == EC_NOTFORDEMO )
	{
		if ( IsEnabled() )
		{
			Demo_DisableButton( this );
		}
	}

	m_bOnlyActiveUser = ( pInResourceData->GetInt( "OnlyActiveUser", 0 ) != 0 );
	m_bIgnoreButtonA = ( pInResourceData->GetInt( "IgnoreButtonA", 0 ) != 0 );

	m_bShowDropDownIndicator = ( pInResourceData->GetInt( "ShowDropDownIndicator", 0 ) != 0 );
	m_bOverrideDropDownIndicator = false;

	m_iSelectedArrowSize = vgui::scheme()->GetProportionalScaledValue( pInResourceData->GetInt( "DropDownIndicatorSize", 8 ) );

	if ( m_nStyle == BUTTON_DIALOGLIST )
	{
		KeyValues *pList = pInResourceData->FindKey( "list", false );
		if ( pList )
		{
			for ( KeyValues *pKey = pList->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
			{
				int iIndex = m_DialogListItems.AddToTail();
				m_DialogListItems[iIndex].m_String = pKey->GetName();
				m_DialogListItems[iIndex].m_CommandString = pKey->GetString();
			}
		}
	}
	else if ( m_nStyle == BUTTON_BITMAP )
	{
		const char *pImageName = pInResourceData->GetString( "bitmap_enabled", "" );
		if ( pImageName[0] )
		{
			m_nEnabledImageId = CBaseModPanel::GetSingleton().GetImageId( pImageName );
		}

		pImageName = pInResourceData->GetString( "bitmap_focus", "" );
		if ( pImageName[0] )
		{
			m_nFocusImageId = CBaseModPanel::GetSingleton().GetImageId( pImageName );
		}
	}
}

void BaseModHybridButton::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetReleasedSound( CBaseModPanel::GetSingleton().GetUISoundName( UISOUND_ACCEPT ) );

	const char *pImageName;

	// use find or create pattern, avoid pointless redundant i/o
	pImageName = "vgui/icon_arrow_down";
	m_iSelectedArrow = vgui::surface()->DrawGetTextureId( pImageName );
	if ( m_iSelectedArrow == -1 )
	{
		m_iSelectedArrow = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_iSelectedArrow, pImageName, true, false );	
	}

	// use find or create pattern, avoid pointles redundant i/o
	pImageName = "vgui/icon_arrow";
	m_iUnselectedArrow = vgui::surface()->DrawGetTextureId( pImageName );
	if ( m_iUnselectedArrow == -1 )
	{
		m_iUnselectedArrow = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_iUnselectedArrow, pImageName, true, false );	
	}
}

void BaseModHybridButton::OnKeyCodePressed( vgui::KeyCode code )
{
	int iJoystick = GetJoystickForCode( code );

	if ( m_bOnlyActiveUser )
	{
		// Only allow input from the active userid
		int userId = CBaseModPanel::GetSingleton().GetLastActiveUserId();

		if( iJoystick != userId || iJoystick < 0 )
		{	
			return;
		}
	}

	BaseModUI::CBaseModPanel::GetSingleton().SetLastActiveUserId( iJoystick );

	int iController = XBX_GetUserId( iJoystick );
	bool bIsPrimaryUser = ( iController >= 0 && XBX_GetPrimaryUserId() == DWORD( iController ) );

	KeyCode localCode = GetBaseButtonCode( code );

	if ( ( localCode == KEY_XBUTTON_A ) )
	{
		if ( m_bIgnoreButtonA )
		{
			// Don't swallow the a key... our parent wants it
			CallParentFunction( new KeyValues( "KeyCodePressed", "code", code ) );
			return;
		}

		bool bEnabled = true;
		if ( !IsEnabled() )
		{
			bEnabled = false;
		}

		switch( m_iUsablePlayerIndex )
		{
		case USE_EVERYBODY:
			break;

		case USE_PRIMARY:
			if ( !bIsPrimaryUser )
				bEnabled = false;
			break;

		case USE_SLOT0:
		case USE_SLOT1:
		case USE_SLOT2:
		case USE_SLOT3:
			if ( iJoystick != m_iUsablePlayerIndex )
				bEnabled = false;
			break;

		default:
			bEnabled = false;
			break;
		}

		if ( !bEnabled )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_INVALID );
			return;
		}
	}

	if ( m_nStyle == BUTTON_DIALOGLIST )
	{
		switch ( localCode )
		{
		case KEY_XBUTTON_A:
			ChangeDialogListSelection( SELECT_NEXT );
			break;

		case KEY_XSTICK1_LEFT:
		case KEY_XSTICK2_LEFT:
		case KEY_XBUTTON_LEFT:
		case KEY_XBUTTON_LEFT_SHOULDER:
		case KEY_LEFT:
			ChangeDialogListSelection( SELECT_PREV );
			break;

		case KEY_XSTICK1_RIGHT:
		case KEY_XSTICK2_RIGHT:
		case KEY_XBUTTON_RIGHT:
		case KEY_XBUTTON_RIGHT_SHOULDER:
		case KEY_RIGHT:
			ChangeDialogListSelection( SELECT_NEXT );
			break;
		}
	}

	BaseClass::OnKeyCodePressed( code );
}

void BaseModHybridButton::OnKeyCodeReleased( KeyCode keycode )
{
	//at some point vgui_controls/button.cpp got a 360 only change to never set the armed state to false ever. Too late to fix the logic behind that, just roll with it on PC
	//fixes bug where menu items de-highlight after letting go of the arrow key used to navigate to it
	bool bOldArmedState = IsArmed();

	BaseClass::OnKeyCodeReleased( keycode );

	if ( bOldArmedState && !IsArmed() )
		SetArmed( true );
}

void BaseModHybridButton::SetCurrentSelection( const char *pText )
{
	if ( m_nStyle == BUTTON_LEFTINDIALOG )
	{
		m_nDialogListCurrentIndex = 0;
		m_DialogListItems.Purge();

		if ( pText && pText[0] )
		{
			// optional RHS value
			int i = m_DialogListItems.AddToTail();
			m_DialogListItems[i].m_String = pText;
		}
	}
	else if ( m_nStyle == BUTTON_DIALOGLIST )
	{
		for ( int i = 0; i < m_DialogListItems.Count(); i++ )
		{
			if ( m_DialogListItems[i].m_bEnabled && !V_stricmp( m_DialogListItems[i].m_String.Get(), pText ) )
			{
				m_nDialogListCurrentIndex = i;
				break;
			}
		}
	}
}

void BaseModHybridButton::ModifySelectionString( const char *pCommand, const char *pNewText )
{
	for ( int i = 0; i < m_DialogListItems.Count(); i++ )
	{
		if ( !V_stricmp( m_DialogListItems[i].m_CommandString.Get(), pCommand ) )
		{
			m_DialogListItems[i].m_String = pNewText;
			break;
		}
	}
}

void BaseModHybridButton::ModifySelectionStringParms( const char *pCommand, const char *pParm1 )
{
	for ( int i = 0; i < m_DialogListItems.Count(); i++ )
	{
		if ( !V_stricmp( m_DialogListItems[i].m_CommandString.Get(), pCommand ) )
		{
			m_DialogListItems[i].m_StringParm1 = pParm1;
			break;
		}
	}
}

bool BaseModHybridButton::GetListSelectionString( const char *pCommand, char *pOutBuff, int nOutBuffSize )
{
	for ( int i = 0; i < m_DialogListItems.Count(); i++ )
	{
		if ( !V_stricmp( m_DialogListItems[i].m_CommandString.Get(), pCommand ) )
		{
			V_strncpy( pOutBuff, m_DialogListItems[i].m_String.Get(), nOutBuffSize );
			return true;
		}
	}

	return false;
}

void BaseModHybridButton::ChangeDialogListSelection( ListSelectionChange_t eNext )
{
	int nIncrementDir = ( eNext == SELECT_PREV ) ? -1 : 1; 

	int nNumAttempts = 0;
	int nNewIndex = m_nDialogListCurrentIndex;
	do
	{
		nNewIndex = ( nNewIndex + m_DialogListItems.Count() + nIncrementDir ) % m_DialogListItems.Count();
		if ( m_DialogListItems[nNewIndex].m_bEnabled )
			break;
		nNumAttempts++;
	}
	while ( nNumAttempts < m_DialogListItems.Count() );

	if ( m_nDialogListCurrentIndex != nNewIndex )
	{
		m_nDialogListCurrentIndex = nNewIndex;
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_CLICK );

		PostActionSignal( new KeyValues( "Command", "command", m_DialogListItems[m_nDialogListCurrentIndex].m_CommandString.Get() ) );
	}
	else
	{
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_INVALID );
	}
}

void BaseModHybridButton::EnableListItem( const char *pText, bool bEnable )
{
	if ( m_nStyle == BUTTON_DIALOGLIST )
	{
		for ( int i = 0; i < m_DialogListItems.Count(); i++ )
		{
			if ( !V_stricmp( m_DialogListItems[i].m_String.Get(), pText ) )
			{
				m_DialogListItems[i].m_bEnabled = bEnable;
				break;
			}
		}
	}
}

void BaseModHybridButton::OnCursorEntered()
{
	BaseClass::OnCursorEntered();
	if ( IsPC() )
	{
		if ( !m_isOpen )
		{
			if ( IsEnabled() && !HasFocus() )
			{
				CBaseModPanel::GetSingleton().PlayUISound( UISOUND_FOCUS );
			}

			if ( GetParent() )
			{
				GetParent()->NavigateToChild( this );
			}
			else
			{
				NavigateTo();
			}
		}
	}
}

void BaseModHybridButton::OnCursorExited()
{
	// This is a hack for now, we shouldn't close if the cursor goes to the flyout of this item...
	// Maybe have VFloutMenu check the m_navFrom and it's one of these, keep the SetClosedState...
	BaseClass::OnCursorExited();
	if ( IsPC() )
	{
//		SetClosed();
	}
}

// Message targets that the button has been pressed
void BaseModHybridButton::FireActionSignal( void )
{
	BaseClass::FireActionSignal();

	if ( IsPC() )
	{
		PostMessageToAllSiblingsOfType< BaseModHybridButton >( new KeyValues( "OnSiblingHybridButtonOpened" ) );
	}
}


Panel* BaseModHybridButton::NavigateUp()
{
	Panel *target = BaseClass::NavigateUp();
	if ( IsPC() && !target && 
		(dynamic_cast< DropDownMenu * >( GetParent() ) || dynamic_cast< SliderControl * >( GetParent() )) )
	{
		target = GetParent()->NavigateUp();
	}

	return target;
}

Panel* BaseModHybridButton::NavigateDown()
{
	Panel *target = BaseClass::NavigateDown();
	if ( IsPC() && !target && 
		(dynamic_cast< DropDownMenu * >( GetParent() ) || dynamic_cast< SliderControl * >( GetParent() )) )
	{
		target = GetParent()->NavigateDown();
	}
	return target;
}

Panel* BaseModHybridButton::NavigateLeft()
{
	Panel *target = BaseClass::NavigateLeft();
	if ( IsPC() && !target && 
		dynamic_cast< DropDownMenu * >( GetParent() ) )
	{
		target = GetParent()->NavigateLeft();
	}
	return target;
}

Panel* BaseModHybridButton::NavigateRight()
{
	Panel *target = BaseClass::NavigateRight();
	if ( IsPC() && !target && 
		dynamic_cast< DropDownMenu * >( GetParent() ) )
	{
		target = GetParent()->NavigateRight();
	}
	return target;
}