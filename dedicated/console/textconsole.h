//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#if !defined TEXTCONSOLE_H
#define TEXTCONSOLE_H
#pragma once

#ifdef _WIN32
#include <windows.h>
#endif // _WIN32


#define MAX_CONSOLE_TEXTLEN 256
#define MAX_BUFFER_LINES	30

//class IBaseSystem;

class CTextConsole  
{
public:
	virtual ~CTextConsole()
	{
	};

	virtual bool		Init ( /*IBaseSystem * system*/ );
	virtual void		ShutDown( void );
	virtual void		Print( char * pszMsg );

	virtual void		SetTitle( char * pszTitle )			{ };
	virtual void		SetStatusLine( char * pszStatus )	{ };
	virtual void		UpdateStatus()						{ };

	// Must be provided by children
	virtual void		PrintRaw( char * pszMsg, int nChars = 0 )	= 0;
	virtual void		Echo( char * pszMsg, int nChars = 0 )	= 0;
	virtual char *		GetLine( void )								= 0;
	virtual int			GetWidth( void )							= 0;

	virtual void		SetVisible( bool visible );
	virtual bool		IsVisible();


protected:
	char	m_szConsoleText[ MAX_CONSOLE_TEXTLEN ];						// console text buffer
	int		m_nConsoleTextLen;											// console textbuffer length
	int		m_nCursorPosition;											// position in the current input line

	// Saved input data when scrolling back through command history
	char	m_szSavedConsoleText[ MAX_CONSOLE_TEXTLEN ];				// console text buffer
	int		m_nSavedConsoleTextLen;										// console textbuffer length

	char	m_aszLineBuffer[ MAX_BUFFER_LINES ][ MAX_CONSOLE_TEXTLEN ];	// command buffer last MAX_BUFFER_LINES commands
	int		m_nInputLine;												// Current line being entered
	int		m_nBrowseLine;												// current buffer line for up/down arrow
	int		m_nTotalLines;												// # of nonempty lines in the buffer

	bool	m_ConsoleVisible;

//	IBaseSystem * m_System;

	int		ReceiveNewline( void );
	void	ReceiveBackspace( void );
	void	ReceiveTab( void );
	void	ReceiveStandardChar( const char ch );
	void	ReceiveUpArrow( void );
	void	ReceiveDownArrow( void );
	void	ReceiveLeftArrow( void );
	void	ReceiveRightArrow( void );
};

#endif // !defined
