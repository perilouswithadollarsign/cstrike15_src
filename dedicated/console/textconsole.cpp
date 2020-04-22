//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// TextConsole.cpp: implementation of the TextConsole class.
//
//////////////////////////////////////////////////////////////////////

//#include "port.h"
#pragma warning(disable:4100) //unreferenced formal parameter
#include "textconsole.h"
#pragma warning(default:4100) //unreferenced formal parameter
#include "utlvector.h"
#include "tier1/strtools.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


bool CTextConsole::Init( /*IBaseSystem * system*/ )
{
	//m_System = system;	// NULL or a valid base system interface

	memset( m_szConsoleText, 0, sizeof( m_szConsoleText ) );
	m_nConsoleTextLen	= 0;
	m_nCursorPosition	= 0;

	memset( m_szSavedConsoleText, 0, sizeof( m_szSavedConsoleText ) );
	m_nSavedConsoleTextLen = 0;

	memset( m_aszLineBuffer, 0, sizeof( m_aszLineBuffer ) );
	m_nTotalLines	= 0;
	m_nInputLine	= 0;
	m_nBrowseLine	= 0;

	// these are log messages, not related to console
	Msg( "\n" );
	Msg( "Console initialized.\n" );

	m_ConsoleVisible = true;
  
	return true;
}

void CTextConsole::SetVisible( bool visible )
{
	m_ConsoleVisible = visible;
}

bool CTextConsole::IsVisible()
{
	return m_ConsoleVisible;
}


void CTextConsole::ShutDown( void )
{
}


void CTextConsole::Print( char * pszMsg )
{
	if ( m_nConsoleTextLen )
	{
		int nLen;

		nLen = m_nConsoleTextLen;

		while ( nLen-- )
		{
			PrintRaw( "\b \b" );
		}
	}

	PrintRaw( pszMsg );

	if ( m_nConsoleTextLen )
	{
		PrintRaw( m_szConsoleText, m_nConsoleTextLen );
	}
	
	UpdateStatus();
}


int CTextConsole::ReceiveNewline( void )
{
	int nLen = 0;

	Echo( "\n" );

	if ( m_nConsoleTextLen )
	{
		nLen = m_nConsoleTextLen;

		m_szConsoleText[ m_nConsoleTextLen ] = 0;
		m_nConsoleTextLen = 0;
		m_nCursorPosition = 0;

		// cache line in buffer, but only if it's not a duplicate of the previous line
		if ( ( m_nInputLine == 0 ) || ( strcmp( m_aszLineBuffer[ m_nInputLine - 1 ], m_szConsoleText ) ) )
		{
			strncpy( m_aszLineBuffer[ m_nInputLine ], m_szConsoleText, MAX_CONSOLE_TEXTLEN );
			
			m_nInputLine++;

			if ( m_nInputLine > m_nTotalLines )
				m_nTotalLines = m_nInputLine;

			if ( m_nInputLine >= MAX_BUFFER_LINES )
				m_nInputLine = 0;

		}

		m_nBrowseLine = m_nInputLine;
	}

	return nLen;
}


void CTextConsole::ReceiveBackspace( void )
{
	int nCount;

	if ( m_nCursorPosition == 0 )
	{
		return;
	}

	m_nConsoleTextLen--;
	m_nCursorPosition--;

	Echo( "\b" );

	for ( nCount = m_nCursorPosition; nCount < m_nConsoleTextLen; nCount++ )
	{
		m_szConsoleText[ nCount ] = m_szConsoleText[ nCount + 1 ];
		Echo( m_szConsoleText + nCount, 1 );
	}

	Echo( " " );

	nCount = m_nConsoleTextLen;
	while ( nCount >= m_nCursorPosition )
	{
		Echo( "\b" );
		nCount--;
	}

	m_nBrowseLine = m_nInputLine;
}


void CTextConsole::ReceiveTab( void )
{
	CUtlVector<char *> matches;

	//if ( !m_System )
	//	return;

	m_szConsoleText[ m_nConsoleTextLen ] = 0;

	//m_System->GetCommandMatches( m_szConsoleText, &matches );

	if ( matches.Count() == 0 )
	{
		return;
	}

	if ( matches.Count() == 1 )
	{
		char * pszCmdName;
		char * pszRest;

		pszCmdName	= matches[0];
		pszRest		= pszCmdName + strlen( m_szConsoleText );

		if ( pszRest )
		{
			Echo( pszRest );
			strcat( m_szConsoleText, pszRest );
			m_nConsoleTextLen += strlen( pszRest );

			Echo( " " );
			strcat( m_szConsoleText, " " );
			m_nConsoleTextLen++;
		}
	}
	else
	{
		int		nLongestCmd;
		int		nTotalColumns;
		int		nCurrentColumn;
		char *	pszCurrentCmd;
		int i = 0;

		nLongestCmd = 0;

		pszCurrentCmd = matches[0];
		while ( pszCurrentCmd )
		{
			if ( (int)strlen( pszCurrentCmd) > nLongestCmd )
			{
				nLongestCmd = strlen( pszCurrentCmd);
			}

			i++;
			pszCurrentCmd = (char *)matches[i];
		}

		nTotalColumns	= ( GetWidth() - 1 ) / ( nLongestCmd + 1 );
		nCurrentColumn	= 0;

		Echo( "\n" );

		// Would be nice if these were sorted, but not that big a deal
		pszCurrentCmd = matches[0];
		i = 0;
		while ( pszCurrentCmd )
		{
			char szFormatCmd[ 256 ];

			nCurrentColumn++;

			if ( nCurrentColumn > nTotalColumns )
			{
				Echo( "\n" );
				nCurrentColumn = 1;
			}

			Q_snprintf( szFormatCmd, sizeof(szFormatCmd), "%-*s ", nLongestCmd, pszCurrentCmd );
			Echo( szFormatCmd );

			i++;
			pszCurrentCmd = matches[i];
		}

		Echo( "\n" );
		Echo( m_szConsoleText );
		// TODO: Tack on 'common' chars in all the matches, i.e. if I typed 'con' and all the
		//       matches begin with 'connect_' then print the matches but also complete the
		//       command up to that point at least.
	}

	m_nCursorPosition	= m_nConsoleTextLen;
	m_nBrowseLine		= m_nInputLine;
}



void CTextConsole::ReceiveStandardChar( const char ch )
{
	int nCount;

	// If the line buffer is maxed out, ignore this char
	if ( m_nConsoleTextLen >= ( sizeof( m_szConsoleText ) - 2 ) )
	{
		return;
	}

	nCount = m_nConsoleTextLen;
	while ( nCount > m_nCursorPosition )
	{
		m_szConsoleText[ nCount ] = m_szConsoleText[ nCount - 1 ];
		nCount--;
	}

	m_szConsoleText[ m_nCursorPosition ] = ch;

	Echo( m_szConsoleText + m_nCursorPosition, m_nConsoleTextLen - m_nCursorPosition + 1 );

	m_nConsoleTextLen++;
	m_nCursorPosition++;

	nCount = m_nConsoleTextLen;
	while ( nCount > m_nCursorPosition )
	{
		Echo( "\b" );
		nCount--;
	}

	m_nBrowseLine = m_nInputLine;
}


void CTextConsole::ReceiveUpArrow( void )
{
	int nLastCommandInHistory;

	nLastCommandInHistory = m_nInputLine + 1;
	if ( nLastCommandInHistory > m_nTotalLines )
	{
		nLastCommandInHistory = 0;
	}

	if ( m_nBrowseLine == nLastCommandInHistory )
	{
		return;
	}

	if ( m_nBrowseLine == m_nInputLine )
	{
		if ( m_nConsoleTextLen > 0 )
		{
			// Save off current text
			strncpy( m_szSavedConsoleText, m_szConsoleText, m_nConsoleTextLen );
			// No terminator, it's a raw buffer we always know the length of
		}

		m_nSavedConsoleTextLen = m_nConsoleTextLen;
	}

	m_nBrowseLine--;
	if ( m_nBrowseLine < 0 )
	{
		m_nBrowseLine = m_nTotalLines - 1;
	}

	while ( m_nConsoleTextLen-- )	// delete old line
	{
		Echo( "\b \b" );
	}

	// copy buffered line
	Echo( m_aszLineBuffer[ m_nBrowseLine ] );

	strncpy( m_szConsoleText, m_aszLineBuffer[ m_nBrowseLine ], MAX_CONSOLE_TEXTLEN );
	m_nConsoleTextLen = strlen( m_aszLineBuffer[ m_nBrowseLine ] );

	m_nCursorPosition = m_nConsoleTextLen;
}


void CTextConsole::ReceiveDownArrow( void )
{
	if ( m_nBrowseLine == m_nInputLine )
	{
		return;
	}

	m_nBrowseLine++;
	if ( m_nBrowseLine > m_nTotalLines )
	{
		m_nBrowseLine = 0;
	}

	while ( m_nConsoleTextLen-- )	// delete old line
	{
		Echo( "\b \b" );
	}

	if ( m_nBrowseLine == m_nInputLine )
	{
		if ( m_nSavedConsoleTextLen > 0 )
		{
			// Restore current text
			strncpy( m_szConsoleText, m_szSavedConsoleText, m_nSavedConsoleTextLen );
			// No terminator, it's a raw buffer we always know the length of

			Echo( m_szConsoleText, m_nSavedConsoleTextLen );
		}

		m_nConsoleTextLen = m_nSavedConsoleTextLen;
	}
	else
	{
		// copy buffered line
		Echo( m_aszLineBuffer[ m_nBrowseLine ] );

		strncpy( m_szConsoleText, m_aszLineBuffer[ m_nBrowseLine ], MAX_CONSOLE_TEXTLEN );

		m_nConsoleTextLen = strlen( m_aszLineBuffer[ m_nBrowseLine ] );
	}

	m_nCursorPosition = m_nConsoleTextLen;
}


void CTextConsole::ReceiveLeftArrow( void )
{
	if ( m_nCursorPosition == 0 )
	{
		return;
	}

	Echo( "\b" );
	m_nCursorPosition--;
}


void CTextConsole::ReceiveRightArrow( void )
{
	if ( m_nCursorPosition == m_nConsoleTextLen )
	{
		return;
	}

	Echo( m_szConsoleText + m_nCursorPosition, 1 );
	m_nCursorPosition++;
}
