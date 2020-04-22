//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// CTextConsoleUnix.cpp: Unix implementation of the TextConsole class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _WIN32


#include "TextConsoleUnix.h"

// un-needed include...
//#include "resource.h"
#include "tier0/icommandline.h"
//#include "../ifilesystem.h"
#include "filesystem.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <signal.h>



#define CONSOLE_LOG_FILE "console.log"

bool CTextConsoleUnix::Init( )
{
	static struct termios termNew;
	sigset_t block_ttou;

	sigemptyset (&block_ttou);
	sigaddset (&block_ttou, SIGTTOU);
	sigprocmask (SIG_BLOCK, &block_ttou, NULL);

	tty = stdout;
	
	// this code is for echo-ing key presses to the connected tty
	// (which is != STDOUT) 
	if ( isatty(STDIN_FILENO) )
	{
		tty = fopen( ctermid( NULL ), "w+" );	
		if ( !tty )
		{	
			printf("Unable to open tty(%s) for output\n", ctermid( NULL ) );
			tty = stdout;
		}
		else
		{
			setbuf( tty, NULL ); // turn buffering off
		}
	}
	else
	{
		tty = fopen( "/dev/null", "w+" );
		if ( !tty )
		{
			tty = stdout;
		}
	}

	tcgetattr( STDIN_FILENO, &termStored );

	memcpy( &termNew, &termStored, sizeof( struct termios ) );

	/* Disable canonical mode, and set buffer size to 1 byte */
	termNew.c_lflag &= ( ~ICANON );
	termNew.c_cc[ VMIN ] = 1;
	termNew.c_cc[ VTIME ] = 0;

	/* disable echo */
	termNew.c_lflag &= ( ~ECHO );

	tcsetattr( STDIN_FILENO, TCSANOW, &termNew );
	sigprocmask (SIG_UNBLOCK, &block_ttou, NULL);

	m_bConDebug = CommandLine()->FindParm( "-condebug" ) != 0;
	if ( m_bConDebug && CommandLine()->FindParm( "-conclearlog" ) )
	{
		g_pFullFileSystem->RemoveFile( CONSOLE_LOG_FILE, "GAME" );
	}

	return CTextConsole::Init();
}


void CTextConsoleUnix::ShutDown( void )
{
	sigset_t block_ttou;

 	sigemptyset (&block_ttou);
 	sigaddset (&block_ttou, SIGTTOU);
	sigprocmask (SIG_BLOCK, &block_ttou, NULL);
	tcsetattr( STDIN_FILENO, TCSANOW, &termStored );
	sigprocmask (SIG_UNBLOCK, &block_ttou, NULL);

	CTextConsole::ShutDown();
}


// return 0 if the kb isn't hit
int CTextConsoleUnix::kbhit( void )
{
  fd_set			rfds;
  struct timeval	tv;

  /* Watch stdin (fd 0) to see when it has input. */
  FD_ZERO( &rfds );
  FD_SET( STDIN_FILENO, &rfds );

  /* Return immediately. */
  tv.tv_sec	= 0;
  tv.tv_usec	= 0;

  /* Must be in raw or cbreak mode for this to work correctly. */
  return select( STDIN_FILENO + 1, &rfds, NULL, NULL, &tv )!=-1 && FD_ISSET( STDIN_FILENO, &rfds );
}


char * CTextConsoleUnix::GetLine( void )
{
	if ( !kbhit() ) // early return for 99.999% case :)
		return NULL;

	escape_sequence_t es;

	es = ESCAPE_CLEAR;
	sigset_t block_ttou;

 	sigemptyset (&block_ttou);
 	sigaddset (&block_ttou, SIGTTOU);
 	sigaddset (&block_ttou, SIGTTIN);
	sigprocmask (SIG_BLOCK, &block_ttou, NULL);


	while ( 1 )
	{
		char	ch;
		int		nLen;

		if ( !kbhit() )
			break;
		ch = 0;
        	int numRead = read( STDIN_FILENO, &ch, 1 );
		if ( !numRead )
			break;
		
		switch (ch)
		{
		case '\n':	// Enter
			es = ESCAPE_CLEAR;

			nLen = ReceiveNewline();
			if ( nLen )
			{
				sigprocmask (SIG_UNBLOCK, &block_ttou, NULL);
				return m_szConsoleText;	
			}
			break;

		case 127:		// Backspace
		case '\b':		// Backspace
			es = ESCAPE_CLEAR;
			ReceiveBackspace();
			break;

		case '\t':	// TAB
			es = ESCAPE_CLEAR;
			ReceiveTab();
			break;

		case 27:	// Escape character
			es = ESCAPE_RECEIVED;
			break;

		case '[':	// 2nd part of escape sequence
		case 'O':
		case 'o':
			switch( es )
			{
			case ESCAPE_CLEAR:
			case ESCAPE_BRACKET_RECEIVED:
				es = ESCAPE_CLEAR;
				ReceiveStandardChar( ch );
				break;

			case ESCAPE_RECEIVED:
				es = ESCAPE_BRACKET_RECEIVED;
				break;
			}
			break;

		case 'A':
			if ( es == ESCAPE_BRACKET_RECEIVED )
			{
				es = ESCAPE_CLEAR;
				ReceiveUpArrow();
			}
			else
			{
				es = ESCAPE_CLEAR;
				ReceiveStandardChar( ch );
			}
			break;

		case 'B':
			if ( es == ESCAPE_BRACKET_RECEIVED )
			{
				es = ESCAPE_CLEAR;
				ReceiveDownArrow();
			}
			else
			{
				es = ESCAPE_CLEAR;
				ReceiveStandardChar( ch );
			}
			break;

		case 'C':
			if ( es == ESCAPE_BRACKET_RECEIVED )
			{
				es = ESCAPE_CLEAR;
				ReceiveRightArrow();
			}
			else
			{
				es = ESCAPE_CLEAR;
				ReceiveStandardChar( ch );
			}
			break;

		case 'D':
			if ( es == ESCAPE_BRACKET_RECEIVED )
			{
				es = ESCAPE_CLEAR;
				ReceiveLeftArrow();
			}
			else
			{
				es = ESCAPE_CLEAR;
				ReceiveStandardChar( ch );
			}
			break;

		default:
			if ( es != ESCAPE_BRACKET_RECEIVED )	// Just eat this char if it's an unsupported escape
			{
				if ( ( ch >= ' ') && ( ch <= '~' ) )	// dont' accept nonprintable chars
				{
					es = ESCAPE_CLEAR;
					ReceiveStandardChar( ch );
				}
			}
			break;
        }

		fflush( stdout );
	}

	sigprocmask (SIG_UNBLOCK, &block_ttou, NULL);
	return NULL;
}


void CTextConsoleUnix::PrintRaw( char * pszMsg, int nChars )
{
	if ( m_bConDebug )
	{
		FileHandle_t fh = g_pFullFileSystem->Open( CONSOLE_LOG_FILE, "a" );
		if ( fh != FILESYSTEM_INVALID_HANDLE )
		{
			if ( nChars == 0 )
			{
				g_pFullFileSystem->Write( pszMsg, strlen( pszMsg ), fh );
			}
			else
			{
				g_pFullFileSystem->Write( pszMsg, nChars, fh );
			}
			g_pFullFileSystem->Close( fh );
		}
	}

	if ( nChars == 0 )
	{
		printf( "%s", pszMsg );
	}
	else
	{
		int nCount;

		for ( nCount = 0; nCount < nChars; nCount++ )
		{
			printf( "%c", pszMsg[ nCount ] );
		}
	}
}


void CTextConsoleUnix::Echo( char * pszMsg, int nChars )
{
	if ( nChars == 0 )
	{
		fprintf( tty, "%s", pszMsg );
	}
	else
	{
		int nCount;

		for ( nCount = 0; nCount < nChars; nCount++ )
		{
			fprintf( tty, "%c", pszMsg[ nCount ] );
		}
	}
}

int CTextConsoleUnix::GetWidth( void )
{
	struct winsize	ws;
	int				nWidth;

	nWidth = 0;

	if ( ioctl( STDOUT_FILENO, TIOCGWINSZ, &ws ) == 0 )
	{
		nWidth = (int)ws.ws_col;
 	} 
 
 /*	if ( nWidth <= 0 )
	{
		char * ss;

		ss = getenv( "COLUMNS" );

		if ( ss )
		{
			nWidth = atoi( ss );
		}
    }
*/

	if ( nWidth <= 1 )
	{
		nWidth = 80;
	}

	return nWidth;    
} 


#endif // ndef _WIN32
