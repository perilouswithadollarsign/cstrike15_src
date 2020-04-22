//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// TextConsoleUnix.h: Unix interface for the TextConsole class.
//
//////////////////////////////////////////////////////////////////////

#if !defined TEXTCONSOLE_UNIX_H
#define TEXTCONSOLE_UNIX_H


#ifndef _WIN32


#include <termios.h>
#include <stdio.h>
#include "textconsole.h"


typedef enum
{
    ESCAPE_CLEAR = 0,
    ESCAPE_RECEIVED,
    ESCAPE_BRACKET_RECEIVED
} escape_sequence_t;


class CTextConsoleUnix : public CTextConsole
{
public:
	virtual ~CTextConsoleUnix()
	{
	};

	bool		Init();
	void		ShutDown( void );
	void		PrintRaw( char * pszMsg, int nChars = 0 );
	void		Echo( char * pszMsg, int nChars = 0 );
	char *		GetLine( void );
	int			GetWidth( void );

private:
	int kbhit( void );

	bool m_bConDebug;

	struct termios termStored;
	FILE *tty;
};


#endif // _ndef WIN32


#endif // !defined
