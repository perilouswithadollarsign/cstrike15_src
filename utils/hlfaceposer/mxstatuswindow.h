//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef MXSTATUSWINDOW_H
#define MXSTATUSWINDOW_H
#ifdef _WIN32
#pragma once
#endif

#include "faceposertoolwindow.h"
#include "color.h"

class mxScrollbar;

#define IDC_STATUS_SCROLL 1000

class mxStatusWindow : public mxWindow, public IFacePoserToolWindow
{
public:
	mxStatusWindow (mxWindow *parent, int x, int y, int w, int h, const char *label = 0 );
	~mxStatusWindow();

	void StatusPrint(const Color& clr, bool overwrite, const char *text );

	virtual void	DrawActiveTool();

	virtual void redraw();
	virtual bool PaintBackground( void );

	virtual int handleEvent( mxEvent *event );

	virtual void Think( float dt );

private:

	void PositionSliders( int sboffset );

	enum
	{
		MAX_TEXT_LINES = 1024,
		TEXT_LINE_MASK = MAX_TEXT_LINES - 1,
	};

	struct TextLine
	{
		char		m_szText[ 512 ];
		Color		rgb;
		float		curtime;
	};

	TextLine		m_rgTextLines[ MAX_TEXT_LINES ];
	int				m_nCurrentLine;

	mxScrollbar		*m_pScrollbar;
};

extern mxStatusWindow *g_pStatusWindow;

#endif // MXSTATUSWINDOW_H
