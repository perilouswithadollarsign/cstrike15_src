//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// TextConsoleWin32.h: Win32 interface for the TextConsole class.
//
//////////////////////////////////////////////////////////////////////

#if !defined TEXTCONSOLE_WIN32_H
#define TEXTCONSOLE_WIN32_H
#pragma once


#ifdef _WIN32


#include <windows.h>
#include "TextConsole.h"

class CTextConsoleWin32 : public CTextConsole
{
public:
	CTextConsoleWin32();
	virtual ~CTextConsoleWin32()
	{
	};

	bool		Init( /*IBaseSystem * system*/ );
	void		ShutDown( void );
	void		PrintRaw( char * pszMsz, int nChars = 0 );
	void		Echo( char * pszMsz, int nChars = 0 );
	char *		GetLine( void );
	int			GetWidth( void );
	void		SetTitle( char * pszTitle );
	void		SetStatusLine( char * pszStatus );
	void		UpdateStatus( void );
	void		SetColor( WORD );
	void		SetVisible( bool visible );

private:
	HANDLE	hinput;		// standard input handle
	HANDLE	houtput;	// standard output handle
	WORD	Attrib;		// attrib colours for status bar
	
	char	statusline[81];			// first line in console is status line
};


#endif // _WIN32


#endif // !defined
