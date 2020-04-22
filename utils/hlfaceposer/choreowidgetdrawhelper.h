//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CHOREOWIDGETDRAWHELPER_H
#define CHOREOWIDGETDRAWHELPER_H
#ifdef _WIN32
#pragma once
#endif

#include <mxtk/mx.h>
#include "choreowidget.h"
#include "utlvector.h"
#include "color.h"

//-----------------------------------------------------------------------------
// Purpose: Helper class that automagically sets up and destroys a memory device-
//  context for flicker-free refershes
//-----------------------------------------------------------------------------
class CChoreoWidgetDrawHelper
{
public:
	// Construction/destruction
				CChoreoWidgetDrawHelper( mxWindow *widget);
				CChoreoWidgetDrawHelper( mxWindow *widget, const Color& bgColor );
				CChoreoWidgetDrawHelper( mxWindow *widget, int x, int y, int w, int h, const Color& bgColor );
				CChoreoWidgetDrawHelper( mxWindow *widget, RECT& bounds );
				CChoreoWidgetDrawHelper( mxWindow *widget, RECT& bounds, const Color& bgColor );

				CChoreoWidgetDrawHelper( mxWindow *widget, RECT& bounds, bool noPageFlip );

	virtual		~CChoreoWidgetDrawHelper( void );

	// Allow caller to draw onto the memory dc, too
	HDC			GrabDC( void );

	// Compute text size
	static int	CalcTextWidth( const char *font, int pointsize, int weight, const char *fmt, ... );
	static int	CalcTextWidth( HFONT font, const char *fmt, ... );

	static int	CalcTextWidthW( const char *font, int pointsize, int weight, const wchar_t *fmt, ... );
	static int	CalcTextWidthW( HFONT font, const wchar_t *fmt, ... );

	void		DrawColoredTextW( const char *font, int pointsize, int weight, const Color& clr, RECT& rcText, const wchar_t *fmt, ... );
	void		DrawColoredTextW( HFONT font, const Color& clr, RECT& rcText, const wchar_t *fmt, ... );
	void		DrawColoredTextCharsetW( const char *font, int pointsize, int weight, DWORD charset, const Color& clr, RECT& rcText, const wchar_t *fmt, ... );

	void		CalcTextRect( const char *font, int pointsize, int weight, int maxwidth, RECT& rcText, const char *fmt, ... );

	// Draw text
	void		DrawColoredText( const char *font, int pointsize, int weight, const Color& clr, RECT& rcText, const char *fmt, ... );
	void		DrawColoredText( HFONT font, const Color& clr, RECT& rcText, const char *fmt, ... );
	void		DrawColoredTextCharset( const char *font, int pointsize, int weight, DWORD charset, const Color& clr, RECT& rcText, const char *fmt, ... );
	void		DrawColoredTextMultiline( const char *font, int pointsize, int weight, const Color& clr, RECT& rcText, const char *fmt, ... );
	// Draw a line
	void		DrawColoredLine( const Color& clr, int style, int width, int x1, int y1, int x2, int y2 );
	void		DrawColoredPolyLine( const Color& clr, int style, int width, CUtlVector< POINT >& points );

	// Draw a blending ramp
	POINTL		DrawColoredRamp( const Color& clr, int style, int width, int x1, int y1, int x2, int y2, float rate, float sustain );
	// Draw a filled rect
	void		DrawFilledRect( const Color& clr, int x1, int y1, int x2, int y2 );
	// Draw an outlined rect
	void		DrawOutlinedRect( const Color& clr, int style, int width, int x1, int y1, int x2, int y2 );
	void		DrawOutlinedRect( const Color& clr, int style, int width, RECT& rc );

	void		DrawFilledRect( HBRUSH br, RECT& rc );
	void		DrawFilledRect( const Color& clr, RECT& rc );

	void		DrawGradientFilledRect( RECT& rc, const Color& clr1, const Color& clr2, bool vertical );

	void		DrawLine( int x1, int y1, int x2, int y2, const Color& clr, int thickness );

	// Draw a triangle
	void		DrawTriangleMarker( RECT& rc, const Color& fill, bool inverted = false );

	void		DrawCircle( const Color& clr, int x, int y, int radius, bool filled = true );

	// Get width/height of draw area
	int			GetWidth( void );
	int			GetHeight( void );

	// Get client rect for drawing
	void		GetClientRect( RECT& rc );

	void		StartClipping( RECT& clipRect );
	void		StopClipping( void );

	// Remap rect if we're using a clipped viewport
	void		OffsetSubRect( RECT& rc );

private:
	// Internal initializer
	void		Init( mxWindow *widget, int x, int y, int w, int h, const Color& bgColor, bool noPageFlip );

	void		ClipToRects( void );

	// The window we are drawing on
	HWND		m_hWnd;
	// The final DC
	HDC			m_dcReal;
	// The working DC
	HDC			m_dcMemory;
	// Client area and offsets
	RECT		m_rcClient;
	int			m_x, m_y;
	int			m_w, m_h;
	// Bitmap for drawing in the memory DC
	HBITMAP		m_bmMemory;
	HBITMAP		m_bmOld;
	// Remember the original default color
	Color	m_clrOld;

	CUtlVector < RECT > m_ClipRects;
	HRGN		m_ClipRegion;

	bool		m_bNoPageFlip;
};

#endif // CHOREOWIDGETDRAWHELPER_H
