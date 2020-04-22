//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "client_pch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Determine length of text string
// Input  : *font - 
//			*fmt - 
//			... - 
// Output : 
//-----------------------------------------------------------------------------
int DrawTextLen( vgui::HFont font, const wchar_t *text )
{
	int len = wcslen( text );

	int x = 0;

	vgui::surface()->DrawSetTextFont( font );

	for ( int i = 0 ; i < len; i++ )
	{
		int a, b, c;
		vgui::surface()->GetCharABCwide( font, text[i], a, b, c );
		// Ignore a
		if ( i != 0 )
			x += a;
		x += b;
		if ( i != len - 1 )
			x += c;
	}

	return x;
}


//-----------------------------------------------------------------------------
// Purpose: Draws colored text to a vgui panel
// Input  : *font - font to use
//			x - position of text
//			y - 
//			r - color of text
//			g - 
//			b - 
//			a - alpha ( 0 = opaque, 255 = transparent )
//			*fmt - va_* text string
//			... - 
// Output : int - horizontal # of pixels drawn
//-----------------------------------------------------------------------------

void DrawColoredText( vgui::HFont font, int x, int y, int r, int g, int b, int a, const wchar_t *text )
{
	int len = wcslen( text );
	
	if ( len <= 0 )
		return;

	MatSysQueueMark( g_pMaterialSystem, "DrawColoredText\n" );
	vgui::surface()->DrawSetTextFont( font );

	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawSetTextColor( r, g, b, a );

	vgui::surface()->DrawPrintText( text, len );

	MatSysQueueMark( g_pMaterialSystem, "END DrawColoredText\n" );
}

void DrawColoredText( vgui::HFont font, int x, int y, Color clr, const wchar_t *text )
{
	int r, g, b, a;
	clr.GetColor( r, g, b, a );
	::DrawColoredText( font, x, y, r, g, b, a, text);
}

void DrawCenteredColoredText( vgui::HFont font, int left, int top, int right, int bottom, Color clr, const wchar_t *text )
{
	int textHeight = vgui::surface()->GetFontTall( font );
	int textWidth = DrawTextLen( font, text );
	DrawColoredText( font, (right + left) / 2 - textWidth / 2, (bottom + top) / 2 - textHeight / 2, clr, text );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBasePanel::CBasePanel( vgui::Panel *parent, char const *panelName )
: vgui::Panel( parent, panelName )
{
	vgui::ivgui()->AddTickSignal( GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBasePanel::~CBasePanel( void )
{
}

void CBasePanel::OnTick()
{
	SetVisible( ShouldDraw() );
}

