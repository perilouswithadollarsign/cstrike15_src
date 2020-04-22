//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "cbase.h"
#include "vdialoglistbutton.h"
#include "basemodpanel.h"
#include "VFooterPanel.h"
//#include "VFlyoutMenu.h"
#include "EngineInterface.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Tooltip.h"
#include "vgui/IVgui.h"
#include "tier1/KeyValues.h"
#include "vgui/ilocalize.h"
#include "vgui/IInput.h"
#include "vgui_controls/imagepanel.h"
//#include "VDropDownMenu.h"
#include "VSliderControl.h"
#include "gamemodes.h"

#ifndef _X360
#include <ctype.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace BaseModUI;
using namespace vgui;

DECLARE_BUILD_FACTORY_DEFAULT_TEXT( CDialogListButton, DialogListButton );


//=============================================================================
// Constructor / Destructor
//=============================================================================

CDialogListButton::CDialogListButton( Panel *parent, const char *panelName, const char *text, Panel *pActionSignalTarget, const char *pCmd)
	: BaseClass( parent, panelName, text, pActionSignalTarget, pCmd ),
	m_clrEnabledOverride( 0, 0, 0, 0 )
{
	SetPaintBorderEnabled( false );
	SetPaintBackgroundEnabled( false );

	m_isNavigateTo = false;

	m_hTextFont = vgui::INVALID_FONT;
	m_hSymbolFont = vgui::INVALID_FONT;

	m_originalTall = 0;
	m_textInsetX = 0;
	m_textInsetY = 0;
	m_nListInsetX = 0;
	m_arrowInsetX = 0;
	m_bArrowsAlwaysVisible = false;

	m_iSelectedArrow = -1;
	m_iUnselectedArrow = -1;

	m_nWideAtOpen = 0;

	m_bAllCaps = false;
	m_bPostCheckMark = false;
	m_bCanWrap = true;
	m_bDrawAsDualStateButton = false;
	m_nDualStateCenter = 0;

	m_nCursorHeight = 0;
	m_nMultiline = 0;

	m_nDialogListCurrentIndex = 0;

	m_nEnabledImageId = -1;
	m_nFocusImageId = -1;

	m_flLastBitmapAnimTime = 0;
	m_nBitmapFrame = 0;
	m_nBitmapFrameCache = 0;
}

CDialogListButton::CDialogListButton( Panel *parent, const char *panelName, const wchar_t *text, Panel *pActionSignalTarget, const char *pCmd)
	: BaseClass( parent, panelName, text, pActionSignalTarget, pCmd ),
	m_clrEnabledOverride( 0, 0, 0, 0 )
{
	SetPaintBorderEnabled( false );
	SetPaintBackgroundEnabled( false );
	
	m_isNavigateTo = false;

	m_hTextFont = vgui::INVALID_FONT;
	m_hSymbolFont = vgui::INVALID_FONT;

	m_originalTall = 0;
	m_textInsetX = 0;
	m_textInsetY = 0;
	m_nListInsetX = 0;
	m_arrowInsetX = 0;
	m_bArrowsAlwaysVisible = false;

	m_iSelectedArrow = -1;
	m_iUnselectedArrow = -1;

	m_bAllCaps = false;
	m_bPostCheckMark = false;
	m_bCanWrap = true;
	m_bDrawAsDualStateButton = false;
	m_nDualStateCenter = 0;

	m_nCursorHeight = 0;
	m_nMultiline = 0;

	m_nDialogListCurrentIndex = 0;

	m_nEnabledImageId = -1;
	m_nFocusImageId = -1;

	m_flLastBitmapAnimTime = 0;
	m_nBitmapFrame = 0;
	m_nBitmapFrameCache = 0;
}

CDialogListButton::~CDialogListButton()
{
	// purposely not destroying our textures
	// otherwise i/o constantly on these
}


//=============================================================================
CDialogListButton::State CDialogListButton::GetCurrentState()
{
	State curState = Enabled;
	if ( IsPC() )
	{
		if ( HasFocus() )
		{
			curState = IsEnabled() ? Focus : FocusDisabled;
		}
	}
	
	if ( IsArmed() ) 
	{										
		if ( IsEnabled() )					
		{
			curState = Focus;
		}
		else
		{
			curState = FocusDisabled;
		}
	}
	else if ( !IsEnabled() )
	{
		curState = Disabled;
	}

	return curState;
}


//=============================================================================
void CDialogListButton::OnMousePressed( vgui::MouseCode code )
{
	BaseClass::OnMousePressed( code );
	if ( IsPC() )
	{

			if ( code == MOUSE_LEFT )
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

//=============================================================================
void CDialogListButton::NavigateTo()
{
	BaseClass::NavigateTo();
	m_isNavigateTo = true;
}

//=============================================================================
void CDialogListButton::NavigateFrom()
{
	BaseClass::NavigateFrom();

	m_isNavigateTo = false;
}

//=============================================================================
bool CDialogListButton::GetDialogListButtonCenter( int &x, int &y )
{
	if( m_bDrawAsDualStateButton )
	{
		x = m_nDualStateCenter;
	}
	else
	{
		x = GetWide() / 2;
	}
	y = GetTall() / 2;

	return true;
}


void CDialogListButton::GetStringAtIndex( int nIndex, wchar_t *pszDest, int nArraySize)
{
	wchar_t *pLocalizedString = g_pVGuiLocalize->Find( m_DialogListItems[nIndex].m_String.Get() );
	if ( !pLocalizedString )
	{
		if ( !m_DialogListItems[nIndex].m_StringParm1.IsEmpty() )
		{
			wchar_t szTempUnicode[512];
			g_pVGuiLocalize->ConvertANSIToUnicode( m_DialogListItems[nIndex].m_String.Get(), szTempUnicode, sizeof( szTempUnicode ) );

			wchar_t *pParm1String = g_pVGuiLocalize->Find( m_DialogListItems[nIndex].m_StringParm1.Get() );
			if ( !pParm1String )
			{
				pParm1String = L"";
			}

			V_snwprintf( pszDest, nArraySize, szTempUnicode, pParm1String );
		}
		else
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( m_DialogListItems[nIndex].m_String.Get(), pszDest, nArraySize );
		}
	}
	else
	{
		Q_wcsncpy( pszDest, pLocalizedString, nArraySize );
	}
}


void DrawStringInButton( wchar_t *pszString, int x, int y, int nAvailableWidth, int nStringLength, int nTextWidth, Color color, vgui::HFont hTextFont )
{
	vgui::surface()->DrawSetTextFont( hTextFont );
	vgui::surface()->DrawSetTextPos( x, y  );
	vgui::surface()->DrawSetTextColor( color );

	if ( nTextWidth > nAvailableWidth )
	{
		// length of 3 dots
		int ellipsesLen = 3 * vgui::surface()->GetCharacterWidth( hTextFont, L'.' );
		nAvailableWidth -= ellipsesLen;

		// draw as much as possible
		for ( int i = 0; i < nStringLength; i++ )
		{
			vgui::surface()->DrawUnicodeChar( pszString[i] );
			nAvailableWidth -= vgui::surface()->GetCharacterWidth( hTextFont, pszString[i] );
			if ( nAvailableWidth <= 0 )
				break;
		}

		// finish with ...
		vgui::surface()->DrawPrintText( L"...", 3 );
	}
	else
	{
		vgui::surface()->DrawUnicodeString( pszString );
	}
}


//=============================================================================
void CDialogListButton::DrawDialogListButton( Color textColor )
{
	if ( !m_DialogListItems.IsValidIndex( m_nDialogListCurrentIndex ) )
		return;

	// set arrow colors
	Color leftCursorColor;
	Color rightCursorColor;

	if ( HasFocus() )
	{
		leftCursorColor = IsGameConsole() ? textColor : m_ListButtonInactiveColor;
		rightCursorColor = IsGameConsole() ? textColor : m_ListButtonInactiveColor;
	}
	else
	{
		leftCursorColor = IsGameConsole() ? textColor : m_ListButtonNonFocusColor;
		rightCursorColor = IsGameConsole() ? textColor : m_ListButtonNonFocusColor;
	}

	// set arrow colors based on cursor position
	if ( GetCurrentState() == Focus && m_DialogListItems.Count() )
	{
		if ( IsPC() )
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
						rightCursorColor = m_ListButtonInactiveColor;
					}
					else
					{
						rightCursorColor = m_ListButtonActiveColor;
						leftCursorColor = m_ListButtonInactiveColor;
					}
				}
			}
		}
	}

	int x = m_arrowInsetX;
	int nArrowWidth = 0;

	if ( ( GetCurrentState() == Focus && m_DialogListItems.Count()) || m_bArrowsAlwaysVisible )
	{
		bool bDrawLeftArrow = true;
		bool bDrawRightArrow = true;

		//Don't draw left arrow if on the first item and can't wrap
		if( !m_bCanWrap && m_nDialogListCurrentIndex == 0 )
		{
			bDrawLeftArrow = false;
		}

		//Don't draw right arrow if on the last item and can't wrap
		if( !m_bCanWrap && m_nDialogListCurrentIndex == ( m_DialogListItems.Count() - 1 ) )
		{
			bDrawRightArrow = false;
		}

		if ( IsPC() && m_hSymbolFont != vgui::INVALID_FONT )
		{
			// pc can support a symbol font and use real arrows
			int nArrowWide, nArrowTall;
			vgui::surface()->GetTextSize( m_hSymbolFont, L"3", nArrowWide, nArrowTall );

			nArrowWidth = nArrowWide;
			int nArrowInsetY = ( GetTall() - nArrowTall ) / 2;

			vgui::surface()->DrawSetTextFont( m_hSymbolFont );
			//Left arrow
			if( bDrawLeftArrow )
			{
				vgui::surface()->DrawSetTextColor( leftCursorColor );
				vgui::surface()->DrawSetTextPos( x, nArrowInsetY );
				vgui::surface()->DrawUnicodeString( L"3" );
			}

			//Right arrow
			if( bDrawRightArrow )
			{
				x = GetWide() - m_arrowInsetX - nArrowWide; 
				vgui::surface()->DrawSetTextColor( rightCursorColor );
				vgui::surface()->DrawSetTextPos( x, nArrowInsetY );
				vgui::surface()->DrawUnicodeString( L"4" );
			}
		}
		else
		{
			int nArrowSize = vgui::surface()->GetCharacterWidth( m_hTextFont, L'<' );
			nArrowWidth = nArrowSize;
			vgui::surface()->DrawSetTextFont( m_hTextFont );
			//Left arrow
			if( bDrawLeftArrow )
			{
				vgui::surface()->DrawSetTextColor( leftCursorColor );
				vgui::surface()->DrawSetTextPos( x, m_textInsetY );
				vgui::surface()->DrawUnicodeString( L"<" );
			}
			x = GetWide() - m_arrowInsetX - nArrowSize;
			//Right arrow
			if( bDrawRightArrow )
			{
				vgui::surface()->DrawSetTextColor( rightCursorColor );
				vgui::surface()->DrawSetTextPos( x, m_textInsetY );
				vgui::surface()->DrawUnicodeString( L">" );
			}
		}
	}

	if( m_bDrawAsDualStateButton )
	{
		//Dual state buttons can only have 2 items
		if( GetListItemCount() != 2 )
		{
			return;
		}

		//Get both the strings
		wchar_t szLeftString[512];
		wchar_t szRightString[512];
		GetStringAtIndex( 0, szLeftString, ARRAYSIZE(szLeftString) );
		GetStringAtIndex( 1, szRightString, ARRAYSIZE(szRightString) );

		//Get max length of text in the button
		int nButtonWidth = GetWide();
		int x1 = m_arrowInsetX + nArrowWidth + m_textInsetX;
		int x2 = nButtonWidth - m_arrowInsetX - nArrowWidth;
		int nUsableWidth = x2 - x1;

		//Get size of the '/'
		int slashWide, slashTall;
		surface()->GetTextSize( m_hTextFont, L"/", slashWide, slashTall );

		//Get the usable width of each of the strings
		int nLeftStringMaxWidth = ( nUsableWidth/2 ) - m_textInsetX;
		int nRightStringMaxWidth = ( nUsableWidth/2 ) - ( slashWide + m_textInsetX );

		Color leftColor, rightColor;
		vgui::HFont *pLeftFont, *pRightFont;
 		if( m_nDialogListCurrentIndex == 0 )
		{
			leftColor = textColor;
			rightColor = m_DisabledColor;
			pLeftFont = &m_hTextFont;
			pRightFont = &m_hTextFontSmall;
		}
		else
		{
			leftColor = m_DisabledColor;
			rightColor = textColor;
			pLeftFont = &m_hTextFontSmall;
			pRightFont = &m_hTextFont;
		}

		//Draw left string
		int leftX = m_arrowInsetX + nArrowWidth + m_textInsetX;
		int leftWidth, leftHeight;
		surface()->GetTextSize( *pLeftFont, szLeftString, leftWidth, leftHeight );
		int nTempWidth = leftWidth;
		int nTempHeight = leftHeight;
		surface()->GetTextSize( m_hTextFont, szLeftString, leftWidth, leftHeight );
		int leftLength = V_wcslen( szLeftString );
		DrawStringInButton( szLeftString, leftX - ( nTempWidth - leftWidth ), m_textInsetY - ( nTempHeight - leftHeight ), nLeftStringMaxWidth, leftLength, leftWidth, leftColor, *pLeftFont );

		//Draw '/'
		int slashX = leftX + leftWidth +( m_textInsetX/2 );
		int slashLength = V_wcslen( L"/" );
		m_nDualStateCenter = slashX + ( slashWide/2 );
		DrawStringInButton( L"/", slashX, m_textInsetY, slashWide, slashLength, slashWide, m_DisabledColor, m_hTextFont );

		//Draw right string
		int rightX = slashX + slashWide + ( m_textInsetX/2 );
		int rightWidth, rightHeight;
		surface()->GetTextSize( *pRightFont, szLeftString, rightWidth, rightHeight );
		nTempHeight = rightHeight;
		surface()->GetTextSize( m_hTextFont, szRightString, rightWidth, rightHeight );
		int rightLength = V_wcslen( szRightString );
		DrawStringInButton( szRightString, rightX, m_textInsetY - ( nTempHeight - rightHeight ), nRightStringMaxWidth, rightLength, rightWidth, rightColor, *pRightFont );
	}
	else
	{
		wchar_t szUnicode[512];
		GetStringAtIndex( m_nDialogListCurrentIndex, szUnicode, ARRAYSIZE(szUnicode) );

		wchar_t *pHeaderPrefix = wcsstr( szUnicode, L"\n" );
		if ( pHeaderPrefix )
		{
			pHeaderPrefix++;
			Q_wcsncpy( szUnicode, pHeaderPrefix, sizeof( szUnicode ) );
		}

		int len = V_wcslen( szUnicode );
		// get the size of the text
		int textWide, textTall;
		surface()->GetTextSize( m_hTextFont, szUnicode, textWide, textTall );

		x = m_arrowInsetX + nArrowWidth + m_textInsetX;
		int availableWidth = GetWide() - ( m_arrowInsetX * 2 ) - ( m_textInsetX * 2 ) - ( nArrowWidth * 2 ); //textWide + m_textInsetX;

		DrawStringInButton( szUnicode, x, m_textInsetY, availableWidth, len, textWide, textColor, m_hTextFont );
	}
}

//=============================================================================
void CDialogListButton::PaintButtonEx()
{
	int wide, tall;
	GetSize( wide, tall );

	int x = 0;

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

	case Focus:
		// active item
		textColor = m_FocusColor;
		bDrawCursor = true;
		break;
	}

	if ( bDrawCursor )
	{
		surface()->DrawSetColor( m_CursorColor );
		int cursorY = ( tall - m_nCursorHeight ) / 2;
		surface()->DrawFilledRect( x, cursorY, x+wide, cursorY + m_nCursorHeight );
	}

	DrawDialogListButton( textColor );
}

//=============================================================================
void CDialogListButton::Paint()
{
	PaintButtonEx();

	// only do the forced selection for a single frame.
	m_isNavigateTo = false;
}

//=============================================================================
void CDialogListButton::ApplySettings( KeyValues *pInResourceData )
{
	BaseClass::ApplySettings( pInResourceData );

	vgui::IScheme *pScheme = vgui::scheme()->GetIScheme( GetScheme() );
	if ( !pScheme )
		return;

	m_TextColor = GetSchemeColor( "HybridButton.TextColor", pScheme );
	m_FocusColor = GetSchemeColor( "HybridButton.FocusColor", pScheme );
	m_CursorColor = GetSchemeColor( "HybridButton.CursorColor", pScheme );
	m_BaseColor = Color( 255, 255, 255, 0 );
	m_DisabledColor = GetSchemeColor( "HybridButton.DisabledColor", pScheme );
	m_FocusDisabledColor = GetSchemeColor( "HybridButton.FocusDisabledColor", pScheme );
	m_ListButtonActiveColor = GetSchemeColor( "HybridButton.ListButtonActiveColor", pScheme );
	m_ListButtonInactiveColor = GetSchemeColor( "HybridButton.ListButtonInactiveColor", pScheme );
	m_ListButtonNonFocusColor = Color ( 0, 0, 0, 255 );

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

	m_hTextFont = pScheme->GetFont( "NewGameChapterName", true );
	m_nTextFontHeight = vgui::surface()->GetFontTall( m_hTextFont );
	SetFont( m_hTextFont );

	m_hTextFontSmall = pScheme->GetFont( "InstructorTitle_ss", true );

	const char *pStyle = pInResourceData->GetString( "Style", NULL ); //"DialogListButton";
	if ( !pStyle )
	{
		pStyle = "DefaultButton";
	}

	// if using the puzzlemaker alternate color scheme
	if ( !V_stricmp( pStyle, "AltButton" ) )
	{
		// set the alternate colors
		m_TextColor = GetSchemeColor( "HybridButton.TextColorAlt", pScheme );
		m_FocusColor = GetSchemeColor( "HybridButton.FocusColorAlt", pScheme );
		m_CursorColor = GetSchemeColor( "HybridButton.MouseOverCursorColorAlt", pScheme );
		m_ListButtonActiveColor = GetSchemeColor( "HybridButton.ListButtonActiveColorAlt", pScheme );
		m_ListButtonInactiveColor = GetSchemeColor( "HybridButton.ListButtonInactiveColorAlt", pScheme );
		//m_ListButtonInactiveColor = //GetSchemeColor( "HybridButton.FocusColorAlt", pScheme );
		m_ListButtonNonFocusColor = GetSchemeColor( "HybridButton.TextColorAlt", pScheme ); // Color ( 0, 0, 0, 255 );
		// reset to default button style for everything else
		pStyle = "DefaultButton";
	}

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
	m_arrowInsetX = vgui::scheme()->GetProportionalScaledValue( pInResourceData->GetInt( "arrowInsetX", 0 ) );

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

	tall = MAX( m_nTextFontHeight, m_nCursorHeight );
	SetSize( wide, tall );
	m_nDualStateCenter = wide/2;

	m_iSelectedArrowSize = vgui::scheme()->GetProportionalScaledValue( pInResourceData->GetInt( "DropDownIndicatorSize", 8 ) );

	KeyValues *pList;

	if ( m_nListNumber )
	{
		char szList[6];
		V_snprintf( szList, sizeof( szList ), "list%i", m_nListNumber );
		pList = pInResourceData->FindKey( szList, false );
	}
	else
	{
		pList = pInResourceData->FindKey( "list", false );
	}

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

//=============================================================================
void CDialogListButton::ApplySchemeSettings( vgui::IScheme *pScheme )
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

	// use find or create pattern, avoid pointless redundant i/o
	pImageName = "vgui/icon_arrow";
	m_iUnselectedArrow = vgui::surface()->DrawGetTextureId( pImageName );
	if ( m_iUnselectedArrow == -1 )
	{
		m_iUnselectedArrow = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_iUnselectedArrow, pImageName, true, false );	
	}

	SetDepressedBorder( NULL );
}


//=============================================================================
void CDialogListButton::OnKeyCodePressed( vgui::KeyCode code )
{
	BaseClass::OnKeyCodePressed( code );

	KeyCode localCode = GetBaseButtonCode( code );

	if ( ( localCode == KEY_XBUTTON_A ) )
	{
		bool bEnabled = true;
		if ( !IsEnabled() )
		{
			bEnabled = false;
		}

		if ( !bEnabled )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_INVALID );
			return;
		}
	}

	switch ( localCode )
	{
	/*case KEY_XBUTTON_A:
		ChangeDialogListSelection( SELECT_NEXT );
		break;*/

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

	case KEY_UP:
	case KEY_DOWN:
	case KEY_XBUTTON_UP:
	case KEY_XBUTTON_DOWN:
	case KEY_XSTICK1_UP:
	case KEY_XSTICK1_DOWN:
	case KEY_XSTICK2_UP:
	case KEY_XSTICK2_DOWN:
	case KEY_XBUTTON_A:
	case KEY_ENTER:
		GetParent()->OnKeyCodePressed( code );
		break;
	}

}


//=============================================================================
void CDialogListButton::OnKeyCodeReleased( KeyCode keycode )
{
	bool bOldArmedState = IsArmed();

	BaseClass::OnKeyCodeReleased( keycode );

	if ( bOldArmedState && !IsArmed() )
		SetArmed( true );
}


//=============================================================================
void CDialogListButton::SetCurrentSelection( const char *pText )
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


//=============================================================================
void CDialogListButton::SetCurrentSelectionIndex( int nIndex )
{
	if ( nIndex < 0 || nIndex >= m_DialogListItems.Count() )
		return;

	if ( m_DialogListItems[nIndex].m_bEnabled )
	{
		m_nDialogListCurrentIndex = nIndex;
	}
}


//=============================================================================
void CDialogListButton::SetNextSelection()
{
	ChangeDialogListSelection( SELECT_NEXT );
}


//=============================================================================
void CDialogListButton::SetPreviousSelection()
{
	ChangeDialogListSelection( SELECT_PREV );
}


//=============================================================================
void CDialogListButton::ModifySelectionString( const char *pCommand, const char *pNewText )
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


//=============================================================================
void CDialogListButton::ModifySelectionStringParms( const char *pCommand, const char *pParm1 )
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


//=============================================================================
bool CDialogListButton::GetListSelectionString( const char *pCommand, char *pOutBuff, int nOutBuffSize )
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


//=============================================================================
void CDialogListButton::ChangeDialogListSelection( ListSelectionChange_t eNext )
{
	int nIncrementDir = ( eNext == SELECT_PREV ) ? -1 : 1; 

	int nNumAttempts = 0;
	int nNewIndex = m_nDialogListCurrentIndex;

	bool bUpdateSelection = true;

	//If this button can't wrap and is being asked to wrap
	if( !m_bCanWrap &&
		( ( m_nDialogListCurrentIndex == 0 && eNext == SELECT_PREV ) ||
		( m_nDialogListCurrentIndex == ( m_DialogListItems.Count() - 1 ) && eNext == SELECT_NEXT ) ) )
	{
		bUpdateSelection = false;
	}

	if( bUpdateSelection )
	{
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
}


//=============================================================================
void CDialogListButton::EnableListItem( const char *pText, bool bEnable )
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

void CDialogListButton::ForceCurrentSelectionCommand()
{
	PostActionSignal( new KeyValues( "Command", "command", m_DialogListItems[m_nDialogListCurrentIndex].m_CommandString.Get() ) );
}

void CDialogListButton::SetListItemText( int nIndex, const char *pText )
{
	if( nIndex >= 0 && nIndex < m_DialogListItems.Count() )
	{
		m_DialogListItems[ nIndex ].m_String.Set( pText );
	}
}
