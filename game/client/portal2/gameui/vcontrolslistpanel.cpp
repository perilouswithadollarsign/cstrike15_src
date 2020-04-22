//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "vcontrolslistpanel.h"
#include "GameUI_Interface.h"
#include "EngineInterface.h"

#include <vgui/IInput.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui/Cursor.h>
#include <KeyValues.h>
#include "vgui_controls/Label.h"
#include "vgui_controls/scrollbar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: panel used for inline editing of key bindings
//-----------------------------------------------------------------------------
class CInlineEditPanel : public vgui::Panel
{
public:
	CInlineEditPanel() : vgui::Panel(NULL, "InlineEditPanel")
	{
		m_FillColor = Color( 0, 0, 0, 0 );
		m_DashColor = Color( 0, 0, 0, 0 );

		m_nDashedLineSize = 0;
		m_nDashLength = 0;
		m_nGapLength = 0;
	};

	virtual void Paint()
	{
		int x = 0, y = 0, wide, tall;
		GetSize( wide, tall );

		vgui::surface()->DrawSetColor( m_FillColor );
		vgui::surface()->DrawFilledRect( x, y, x + wide, y + tall );

		vgui::surface()->DrawSetColor( m_DashColor );

		int tx0 = x;
		int tx1 = x + wide;
		int ty0 = y;
		int ty1 = y + tall;

		int nOffset = Plat_FloatTime() * 64.0f;
		DrawDashedLine( tx0, ty0, tx1, ty0+m_nDashedLineSize, m_nDashLength, m_nGapLength, nOffset );	// top
		DrawDashedLine( tx0, ty0, tx0+m_nDashedLineSize, ty1, m_nDashLength, m_nGapLength, -nOffset );	// left
		DrawDashedLine( tx0, ty1-m_nDashedLineSize, tx1, ty1, m_nDashLength, m_nGapLength, -nOffset );	// bottom
		DrawDashedLine( tx1-m_nDashedLineSize, ty0, tx1, ty1, m_nDashLength, m_nGapLength, nOffset );	// right
	}

	virtual void OnKeyCodeTyped(KeyCode code)
	{
		if ( !IsGameConsole() && IsJoystickCode( code ) )
			return;

		// forward up
		if (GetParent())
		{
			GetParent()->OnKeyCodeTyped(code);
		}
	}

	virtual void ApplySchemeSettings(IScheme *pScheme)
	{
		Panel::ApplySchemeSettings(pScheme);
		SetBorder(pScheme->GetBorder("DepressedButtonBorder"));

		m_FillColor = GetSchemeColor( "InlineEditPanel.FillColor", m_FillColor, pScheme );
		m_DashColor = GetSchemeColor( "InlineEditPanel.DashColor", m_DashColor, pScheme );

		const char *pResult = pScheme->GetResourceString( "InlineEditPanel.LineSize" );
		if ( pResult[0] )
		{
			m_nDashedLineSize = vgui::scheme()->GetProportionalScaledValue( atoi( pResult ) );
		}

		pResult = pScheme->GetResourceString( "InlineEditPanel.DashLength" );
		if ( pResult[0] )
		{
			m_nDashLength = vgui::scheme()->GetProportionalScaledValue( atoi( pResult ) );
		}

		pResult = pScheme->GetResourceString( "InlineEditPanel.GapLength" );
		if ( pResult[0] )
		{
			m_nGapLength = vgui::scheme()->GetProportionalScaledValue( atoi( pResult ) );
		}
	}

	void OnMousePressed(vgui::MouseCode code)
	{
		// forward up mouse pressed messages to be handled by the key options
		if (GetParent())
		{
			GetParent()->OnMousePressed(code);
		}
	}

private:
	void DrawDashedLine( int x0, int y0, int x1, int y1, int dashLen, int gapLen, int nOffset )
	{
		nOffset %= ( dashLen + gapLen );

		// work out which way the line goes
		if ((x1 - x0) > (y1 - y0))
		{
			// x direction line
			x0 += nOffset;
			while (1)
			{
				if (x0 + dashLen > x1)
				{
					// draw partial
					surface()->DrawFilledRect(x0, y0, x1, y1);
				}
				else
				{
					surface()->DrawFilledRect(x0, y0, x0 + dashLen, y1);
				}

				x0 += dashLen;

				if (x0 + gapLen > x1)
					break;

				x0 += gapLen;
			}
		}
		else
		{
			// y direction
			y0 += nOffset;
			while (1)
			{
				if (y0 + dashLen > y1)
				{
					// draw partial
					surface()->DrawFilledRect(x0, y0, x1, y1);
				}
				else
				{
					surface()->DrawFilledRect(x0, y0, x1, y0 + dashLen);
				}

				y0 += dashLen;

				if (y0 + gapLen > y1)
					break;

				y0 += gapLen;
			}
		}
	}

private:
	Color	m_FillColor;
	Color	m_DashColor;
	int		m_nDashedLineSize;
	int		m_nDashLength;
	int		m_nGapLength;
};

//-----------------------------------------------------------------------------
// Purpose: Construction
//-----------------------------------------------------------------------------
VControlsListPanel::VControlsListPanel( vgui::Panel *parent, const char *listName )	: vgui::SectionedListPanel( parent, listName )
{
	m_bCaptureMode	= false;
	m_nClickRow		= 0;
	m_pInlineEditPanel = new CInlineEditPanel();

	m_hFont = INVALID_FONT;
	m_hButtonFont = INVALID_FONT;
	m_hItemFont = INVALID_FONT;
	m_hSymbolFont = vgui::INVALID_FONT;

	m_nScrollArrowInset = 0;
	m_nButtonOffset = 0;

	m_bLeftArrowHighlighted = false;
	m_bRightArrowHighlighted = false;

	m_pSectionedScrollBar = NULL;

	if ( IsGameConsole() )
	{
		m_pLblDownArrow = new Label( this, "LblDownArrow", "#GameUI_Icons_DOWN_ARROW" );
		m_pLblUpArrow = new Label( this, "LblUpArrow", "#GameUI_Icons_UP_ARROW" );
	}
	else
	{
		m_pLblUpArrow = NULL;
		m_pLblDownArrow = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
VControlsListPanel::~VControlsListPanel()
{
	m_pInlineEditPanel->MarkForDeletion();

	delete m_pLblDownArrow;
	delete m_pLblUpArrow;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VControlsListPanel::ApplySchemeSettings(IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetPostChildPaintEnabled( true );

	m_hFont	= pScheme->GetFont("Default", IsProportional() ); 

	m_nScrollArrowInset = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "CustomButtonBindings.ArrowInset" ) ) );
	m_nButtonOffset = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "CustomButtonBindings.ButtonOffset" ) ) );

	if ( IsGameConsole() )
	{
		m_pLblDownArrow->SetFont( pScheme->GetFont( "GameUIButtonsMini", true ) );
		m_pLblUpArrow->SetFont( pScheme->GetFont( "GameUIButtonsMini", true ) );
	}

	m_hButtonFont = pScheme->GetFont( "GameUIButtonsMini", true );

	if ( IsPC() )
	{
		m_hSymbolFont = pScheme->GetFont( "MarlettLarge", true );
	}

	m_SelectedTextColor = GetSchemeColor( "SectionedListPanel.SelectedTextColor", pScheme );

	m_ListButtonActiveColor = GetSchemeColor( "HybridButton.ListButtonActiveColor", pScheme );
	m_ListButtonInactiveColor = GetSchemeColor( "HybridButton.ListButtonInactiveColor", pScheme );

	m_pSectionedScrollBar = dynamic_cast<vgui::ScrollBar *>( FindChildByName("SectionedScrollBar") );
}

//-----------------------------------------------------------------------------
// Purpose: Start capture prompt display
//-----------------------------------------------------------------------------
void VControlsListPanel::StartCaptureMode( HCursor hCursor )
{
	m_bCaptureMode = true;
	EnterEditMode(m_nClickRow, 1, m_pInlineEditPanel);

	m_pInlineEditPanel->SetZPos( GetZPos() - 1 );

	input()->SetMouseFocus(m_pInlineEditPanel->GetVPanel());
	input()->SetMouseCapture(m_pInlineEditPanel->GetVPanel());

	engine->StartKeyTrapMode();

	if (hCursor)
	{
		m_pInlineEditPanel->SetCursor(hCursor);

		// save off the cursor position so we can restore it
		vgui::input()->GetCursorPos( m_iMouseX, m_iMouseY );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Finish capture prompt display
//-----------------------------------------------------------------------------
void VControlsListPanel::EndCaptureMode( HCursor hCursor )
{
	m_bCaptureMode = false;
	input()->SetMouseCapture(NULL);
	LeaveEditMode();
	RequestFocus();
	input()->SetMouseFocus(GetVPanel());
	if (hCursor)
	{
		m_pInlineEditPanel->SetCursor(hCursor);
		surface()->SetCursor(hCursor);	
		if ( hCursor != dc_none )
		{
			vgui::input()->SetCursorPos ( m_iMouseX, m_iMouseY );	
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set active row column
//-----------------------------------------------------------------------------
void VControlsListPanel::SetItemOfInterest(int itemID)
{
	m_nClickRow	= itemID;
}

//-----------------------------------------------------------------------------
// Purpose: Retrieve row, column of interest
//-----------------------------------------------------------------------------
int VControlsListPanel::GetItemOfInterest()
{
	return m_nClickRow;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're currently waiting to capture a key
//-----------------------------------------------------------------------------
bool VControlsListPanel::IsCapturing( void )
{
	return m_bCaptureMode;
}

//-----------------------------------------------------------------------------
// Purpose: Forwards mouse pressed message up to keyboard page when in capture
//-----------------------------------------------------------------------------
void VControlsListPanel::OnMousePressed(vgui::MouseCode code)
{
	if (IsCapturing())
	{
		// forward up mouse pressed messages to be handled by the key options
		if (GetParent())
		{
			GetParent()->OnMousePressed(code);
		}
	}
	else
	{
		BaseClass::OnMousePressed(code);
	}
}


//-----------------------------------------------------------------------------
// Purpose: input handler
//-----------------------------------------------------------------------------
void VControlsListPanel::OnMouseDoublePressed( vgui::MouseCode code )
{
	if (IsItemIDValid(GetSelectedItem()))
	{
		// enter capture mode
		OnKeyCodePressed(KEY_ENTER);
	}
	else
	{
		BaseClass::OnMouseDoublePressed(code);
	}
}

void VControlsListPanel::OnKeyCodePressed( vgui::KeyCode code )
{
	vgui::KeyCode basecode = GetBaseButtonCode( code );

	switch( basecode )
	{
		case KEY_XBUTTON_UP:
			{
				int nItemId = GetSelectedItem();
				if ( nItemId != -1 && nItemId <= 1 )
				{
					// forces the scroll to its minumum
					// otherwise the header stays scrolled off the top
					BaseClass::OnKeyCodeTyped( KEY_PAGEUP );
				}
				else
				{
					MoveSelectionUp();
				}
			}
			return;

		case KEY_XBUTTON_DOWN:
			MoveSelectionDown();
			return;
	}

	BaseClass::OnKeyCodePressed( code );
}

void VControlsListPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	if ( IsGameConsole() )
	{
		int wide, tall;
		m_pLblDownArrow->GetContentSize( wide, tall );
		m_pLblDownArrow->SetSize( wide, tall );

		m_pLblUpArrow->GetContentSize( wide, tall );
		m_pLblUpArrow->SetSize( wide, tall );

		int xPos = GetWide() - wide - m_nScrollArrowInset;
		int yPos = 0;
		m_pLblUpArrow->SetPos( xPos, yPos );
		m_pLblUpArrow->SetAlpha( 100 );

		yPos = GetTall() - m_pLblDownArrow->GetTall();
		m_pLblDownArrow->SetPos( xPos, yPos );	
		m_pLblDownArrow->SetAlpha( 100 );
		
		int x, y;
		if ( GetItemBounds( GetItemIDFromRow( 0 ), x, y, wide, tall ) )
		{
			if ( y < 0 )
			{
				m_pLblUpArrow->SetAlpha( 255 );
			}
		}

		if ( GetItemBounds( GetItemIDFromRow( GetItemCount() - 1 ), x, y, wide, tall ) )
		{
			if ( y + tall > GetTall() )
			{
				m_pLblDownArrow->SetAlpha( 255 );
			}
		}
	}
}

void VControlsListPanel::ResetToTop()
{
	SetSelectedItem( 0 );
	ScrollToItem( 0 );
	BaseClass::OnKeyCodeTyped( KEY_PAGEUP );
}

void VControlsListPanel::SetInternalItemFont( HFont hFont )
{
	m_hItemFont = hFont;
}

void VControlsListPanel::PostChildPaint()
{
	for ( int i = 0; i < GetItemCount(); i++ )
	{
		int nItemID = GetItemIDFromRow( i );
		int nSelectedItemID = GetSelectedItem();

		KeyValues *pItemData = GetItemData( nItemID );
		if ( pItemData )
		{
			const wchar_t *pButtonString = pItemData->GetWString( "DrawAsButton" );
			if ( pButtonString && pButtonString[0] )
			{
				int buttonLen = V_wcslen( pButtonString );

				int buttonWide = 0, buttonTall = 0;
				vgui::surface()->GetTextSize( m_hButtonFont, pButtonString, buttonWide, buttonTall );
				
				int x, y, wide, tall;
				if ( GetCellBounds( nItemID, 0, x, y, wide, tall ) )
				{
					if ( nSelectedItemID != -1 && nSelectedItemID == nItemID )
					{
						vgui::surface()->DrawSetTextColor( 255, 255, 255, 255 );
					}
					else
					{
						vgui::surface()->DrawSetTextColor( 200, 200, 200, 255 );
					}

					vgui::surface()->DrawSetTextFont( m_hButtonFont );
					vgui::surface()->DrawSetTextPos( x + ( m_nButtonOffset - buttonWide ) / 2, y + ( tall - buttonTall ) / 2 + 1 );
					vgui::surface()->DrawPrintText( pButtonString, buttonLen );
				}

				if ( GetCellBounds( nItemID, 1, x, y, wide, tall ) )
				{
					if ( nSelectedItemID != -1 && nSelectedItemID == nItemID )
					{
						// take over drawing of selected state in order to punch-in L/R arrows
						// PC will draw arrows using symbol font which consoles don't have
						const wchar_t *pActionString = pItemData->GetWString( "ActionText" );
						if ( pActionString )
						{
							if ( IsPC() )
							{
								// dialog list buttons highlight their L/R depending on mouse position
								int nCursorPosX;
								int nCursorPosY;
								input()->GetCursorPos( nCursorPosX, nCursorPosY );
								ScreenToLocal( nCursorPosX, nCursorPosY );

								int nStringWide = 0, nStringTall = 0;
								vgui::surface()->GetTextSize( m_hItemFont, pActionString, nStringWide, nStringTall );

								// left arrow
								int nLeftArrowWide = 0, nLeftArrowTall = 0;
								const wchar_t *pLeftArrowString = L"3";
								vgui::surface()->GetTextSize( m_hSymbolFont, pLeftArrowString, nLeftArrowWide, nLeftArrowTall );
								m_bLeftArrowHighlighted = nCursorPosY > y && nCursorPosY < y + tall && nCursorPosX < x + nLeftArrowWide;
								vgui::surface()->DrawSetTextFont( m_hSymbolFont );
								vgui::surface()->DrawSetTextColor( m_bLeftArrowHighlighted ? m_ListButtonActiveColor : m_ListButtonInactiveColor );
								vgui::surface()->DrawSetTextPos( x, y + ( tall - nLeftArrowTall ) / 2 );
								vgui::surface()->DrawPrintText( pLeftArrowString, V_wcslen( pLeftArrowString ) );

								// text
								vgui::surface()->DrawSetTextFont( m_hItemFont );
								vgui::surface()->DrawSetTextColor( m_SelectedTextColor );
								vgui::surface()->DrawSetTextPos( x + nLeftArrowWide, y + ( tall - nStringTall ) / 2 );
								vgui::surface()->DrawPrintText( pActionString, V_wcslen( pActionString ) );

								// right arrow
								int nRightArrowWide = 0, nRightArrowTall = 0;
								const wchar_t *pRightArrowString = L"4";
								vgui::surface()->GetTextSize( m_hSymbolFont, pRightArrowString, nRightArrowWide, nRightArrowTall );
								m_bRightArrowHighlighted = nCursorPosY > y && nCursorPosY < y + tall && nCursorPosX >= x + nLeftArrowWide;
								vgui::surface()->DrawSetTextFont( m_hSymbolFont );
								vgui::surface()->DrawSetTextColor( m_bRightArrowHighlighted ? m_ListButtonActiveColor : m_ListButtonInactiveColor);
								vgui::surface()->DrawSetTextPos( x + nLeftArrowWide + nStringWide, y + ( tall - nRightArrowTall ) / 2 );
								vgui::surface()->DrawPrintText( pRightArrowString, V_wcslen( pRightArrowString ) );
							}
							else
							{
								int nStringWide = 0, nStringTall = 0;
		
								vgui::surface()->DrawSetTextColor( m_SelectedTextColor );
								vgui::surface()->DrawSetTextFont( m_hItemFont );

								const wchar_t *pLeftArrowString = L"< ";
								vgui::surface()->GetTextSize( m_hItemFont, pLeftArrowString, nStringWide, nStringTall );
								vgui::surface()->DrawSetTextPos( x, y + ( tall - nStringTall ) / 2 );
								vgui::surface()->DrawPrintText( pLeftArrowString, V_wcslen( pLeftArrowString ) );
								x += nStringWide;

								vgui::surface()->GetTextSize( m_hItemFont, pActionString, nStringWide, nStringTall );
								vgui::surface()->DrawSetTextPos( x, y + ( tall - nStringTall ) / 2 );
								vgui::surface()->DrawPrintText( pActionString, V_wcslen( pActionString ) );
								x += nStringWide;

								const wchar_t *pRightArrowString = L" >";
								vgui::surface()->GetTextSize( m_hItemFont, pRightArrowString, nStringWide, nStringTall );
								vgui::surface()->DrawSetTextPos( x, y + ( tall - nStringTall ) / 2 );
								vgui::surface()->DrawPrintText( pRightArrowString, V_wcslen( pRightArrowString ) );
								x += nStringWide;
							}
						}
					}
				}
			}
		}
	}
}
