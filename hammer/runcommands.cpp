//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ====
//
// Purpose: Handles running the OS commands for map compilation.
//
//=============================================================================

#include "stdafx.h"
#include <afxtempl.h>
#include "GameConfig.h"
#include "RunCommands.h"
#include "Options.h"
#include <process.h>
#include <io.h>
#include <direct.h>
#include "GlobalFunctions.h"
#include "hammer.h"
#include "mapdoc.h"
#include "gridnav.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

static bool s_bRunsCommands = false;

bool IsRunningCommands() { return s_bRunsCommands; }

static char *pszDocPath, *pszDocName, *pszDocExt;


// --------------------------------------------------------------------------------------------------------------- //
// This class queues the list of commands to execute, then sends them to hammer_run_map_launcher.exe
// --------------------------------------------------------------------------------------------------------------- //

class CCommandExecuter
{
public:
	CCommandExecuter( bool bWaitForKeypress );
	~CCommandExecuter();

	void AddCommandVA( const char *pStr, ... );
	void AddCommandWithArgList( char **ppParms );

	void EncodeCommand( const char *pCommand, CString &out );

	void Launch();

private:
	// Each command is the full command line with each argument in quotes.
	CUtlVector<CString*> m_Commands;
	bool m_bWaitForKeypress;
};

CCommandExecuter::CCommandExecuter( bool bWaitForKeypress )
{
	m_bWaitForKeypress = bWaitForKeypress;
}

CCommandExecuter::~CCommandExecuter()
{
	m_Commands.PurgeAndDeleteElements();
}

void CCommandExecuter::AddCommandVA( const char *pStr, ... )
{
	char fullStr[8192];

	va_list marker;
	va_start( marker, pStr );
	V_vsnprintf( fullStr, sizeof( fullStr ), pStr, marker );
	va_end( marker );

	CString *pFullStr = new CString;
	*pFullStr = fullStr;
	m_Commands.AddToTail( pFullStr );
}

void CCommandExecuter::AddCommandWithArgList( char **ppParms )
{
	CString *pFullStr = new CString;
	CString &str = *pFullStr;

	while ( *ppParms )
	{
		str += *ppParms;
		str += " ";
		++ppParms;
	}

	m_Commands.AddToTail( pFullStr );
}

void CCommandExecuter::EncodeCommand( const char *pCommand, CString &out )
{
	// 'a' - 'p'
	int len = V_strlen( pCommand );
	char *pTempStr = new char[ len*2 + 1 ];
	for ( int i=0; i < len; i++ )
	{
		pTempStr[i*2+0] = 'a' + (((unsigned char)pCommand[i] >> 0) & 0xF);
		pTempStr[i*2+1] = 'a' + (((unsigned char)pCommand[i] >> 4) & 0xF);
	}
	pTempStr[len*2] = 0;

	out = pTempStr;
	delete [] pTempStr;
}

void CCommandExecuter::Launch()
{
	char szFilename[MAX_PATH];
	GetModuleFileName( NULL, szFilename, sizeof( szFilename ) );
	V_StripLastDir( szFilename, sizeof( szFilename ) );
	V_AppendSlash( szFilename, sizeof( szFilename ) );
	if ( APP()->IsFoundryMode() )
	{
		V_strncat( szFilename, "bin", sizeof( szFilename ) );
		V_AppendSlash( szFilename, sizeof( szFilename ) );
	}

	// Make a master string of all the args.
	CString fullCommand = szFilename;
	fullCommand += "hammer_run_map_launcher.exe ";

	if ( m_bWaitForKeypress )
		fullCommand += "-WaitForKeypress ";

	for ( int i=0; i < m_Commands.Count(); i++ )
	{
		// We encode the commands here into a string without spaces and quotes so hammer_run_map_launcher can pick it up
		// exactly as we have it here.
		CString encodedCommand;
		EncodeCommand( *m_Commands[i], encodedCommand );
		fullCommand += encodedCommand;
		fullCommand += " ";
	}

	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	memset( &si, 0, sizeof( si ) );
	si.cb = sizeof( si );
	
	CreateProcess( 
		NULL,
		(char*)(const char*)fullCommand,
		NULL,
		NULL,
		FALSE,
		CREATE_NEW_CONSOLE,
		NULL,
		NULL,
		&si,
		&pi );	
}



void FixGameVars(char *pszSrc, char *pszDst, BOOL bUseQuotes)
{
	// run through the parms list and substitute $variable strings for
	//  the real thing
	char *pSrc = pszSrc, *pDst = pszDst;
	BOOL bInQuote = FALSE;
	while(pSrc[0])
	{	
		if(pSrc[0] == '$')	// found a parm
		{
			if(pSrc[1] == '$')	// nope, it's a single symbol
			{
				*pDst++ = '$';
				++pSrc;
			}
			else
			{
				// figure out which parm it is .. 
				++pSrc;
				
				if (!bInQuote && bUseQuotes)
				{
					// not in quote, and subbing a variable.. start quote
					*pDst++ = '\"';
					bInQuote = TRUE;
				}

				if(!strnicmp(pSrc, "file", 4))
				{
					pSrc += 4;
					strcpy(pDst, pszDocName);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "ext", 3))
				{
					pSrc += 3;
					strcpy(pDst, pszDocExt);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "path", 4))
				{
					pSrc += 4;
					strcpy(pDst, pszDocPath);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "exedir", 6))
				{
					pSrc += 6;
					strcpy(pDst, g_pGameConfig->m_szGameExeDir);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "bspdir", 6))
				{
					pSrc += 6;
					strcpy(pDst, g_pGameConfig->szBSPDir);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "bsp_exe", 7))
				{
					pSrc += 7;
					strcpy(pDst, g_pGameConfig->szBSP);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "vis_exe", 7))
				{
					pSrc += 7;
					strcpy(pDst, g_pGameConfig->szVIS);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "light_exe", 9))
				{
					pSrc += 9;
					strcpy(pDst, g_pGameConfig->szLIGHT);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "game_exe", 8))
				{
					pSrc += 8;
					strcpy(pDst, g_pGameConfig->szExecutable);
					pDst += strlen(pDst);
				}
				else if (!strnicmp(pSrc, "gamedir", 7))
				{
					pSrc += 7;
					strcpy(pDst, g_pGameConfig->m_szModDir);
					pDst += strlen(pDst);
				}
			}
		}
		else
		{
			if(*pSrc == ' ' && bInQuote)
			{
				bInQuote = FALSE;
				*pDst++ = '\"';	// close quotes
			}

			// just copy the char into the destination buffer
			*pDst++ = *pSrc++;
		}
	}

	if(bInQuote)
	{
		bInQuote = FALSE;
		*pDst++ = '\"';	// close quotes
	}

	pDst[0] = 0;
}

static void RemoveQuotes(char *pBuf)
{
	if(pBuf[0] == '\"')
		strcpy(pBuf, pBuf+1);
	if(pBuf[strlen(pBuf)-1] == '\"')
		pBuf[strlen(pBuf)-1] = 0;
}

LPCTSTR GetErrorString()
{
	static char szBuf[200];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, 
		szBuf, 200, NULL);
	char *p = strchr(szBuf, '\r');	// get rid of \r\n
	if(p) p[0] = 0;
	return szBuf;
}


bool RunCommands(CCommandArray& Commands, LPCTSTR pszOrigDocName, bool bWaitForKeypress)
{
	s_bRunsCommands = true;

	char szCurDir[MAX_PATH];
	_getcwd(szCurDir, MAX_PATH);

	// cut up document name into file and extension components.
	//  create two sets of buffers - one set with the long filename
	//  and one set with the 8.3 format.

	char szDocLongPath[MAX_PATH] = {0}, szDocLongName[MAX_PATH] = {0}, 
		szDocLongExt[MAX_PATH] = {0};
	char szDocShortPath[MAX_PATH] = {0}, szDocShortName[MAX_PATH] = {0}, 
		szDocShortExt[MAX_PATH] = {0};

	GetFullPathName(pszOrigDocName, MAX_PATH, szDocLongPath, NULL);
	GetShortPathName(pszOrigDocName, szDocShortPath, MAX_PATH);

	// split them up
	char *p = strrchr(szDocLongPath, '.');
	if(p && strrchr(szDocLongPath, '\\') < p && strrchr(szDocLongPath, '/') < p)
	{
		// got the extension
		strcpy(szDocLongExt, p+1);
		p[0] = 0;
	}

	p = strrchr(szDocLongPath, '\\');
	if(!p)
		p = strrchr(szDocLongPath, '/');
	if(p)
	{
		// got the filepart
		strcpy(szDocLongName, p+1);
		p[0] = 0;
	}

	// split the short part up
	p = strrchr(szDocShortPath, '.');
	if(p && strrchr(szDocShortPath, '\\') < p && strrchr(szDocShortPath, '/') < p)
	{
		// got the extension
		strcpy(szDocShortExt, p+1);
		p[0] = 0;
	}

	p = strrchr(szDocShortPath, '\\');
	if(!p)
		p = strrchr(szDocShortPath, '/');
	if(p)
	{
		// got the filepart
		strcpy(szDocShortName, p+1);
		p[0] = 0;
	}

	CCommandExecuter commandExecuter( bWaitForKeypress );

	int iSize = Commands.GetSize(), i = 0;
	char *ppParms[32];
	while(iSize--)
	{
		CCOMMAND &cmd = Commands[i++];

		// anything there?
		if((!cmd.szRun[0] && !cmd.iSpecialCmd) || !cmd.bEnable)
			continue;

		// set name pointers for long filenames
		pszDocExt = szDocLongExt;
		pszDocName = szDocLongName;
		pszDocPath = szDocLongPath;
		
		char szNewParms[MAX_PATH*5], szNewRun[MAX_PATH*5];

		FixGameVars(cmd.szRun, szNewRun, TRUE);
		FixGameVars(cmd.szParms, szNewParms, TRUE);

		// create a parameter list (not always required)
		char *p = szNewParms;
		ppParms[0] = szNewRun;
		int iArg = 1;
		BOOL bDone = FALSE;
		while(p[0])
		{
			ppParms[iArg++] = p;
			while(p[0])
			{
				if(p[0] == ' ')
				{
					// found a space-separator
					p[0] = 0;

					p++;

					// skip remaining white space
					while (*p == ' ')
						p++;

					break;
				}

				// found the beginning of a quoted parameters
				if(p[0] == '\"')
				{
					while(1)
					{
						p++;
						if(p[0] == '\"')
						{
							// found the end
							if(p[1] == 0)
								bDone = TRUE;
							p[1] = 0;	// kick its ass
							p += 2;

							// skip remaining white space
							while (*p == ' ')
								p++;

							break;
						}
					}
					break;
				}

				// else advance p
				++p;
			}

			if(!p[0] || bDone)
				break;	// done.
		}

		ppParms[iArg] = NULL;

		if(cmd.iSpecialCmd)
		{
			if(cmd.iSpecialCmd == CCCopyFile && iArg == 3)
			{
				RemoveQuotes(ppParms[1]);
				RemoveQuotes(ppParms[2]);
				
				// don't copy if we're already there
				if (stricmp(ppParms[1], ppParms[2]) != 0 )
				{
					commandExecuter.AddCommandVA( "copy \"%s\" \"%s\"", ppParms[1], ppParms[2] );
				}
			}
			else if(cmd.iSpecialCmd == CCDelFile && iArg == 2)
			{
				RemoveQuotes(ppParms[1]);
				commandExecuter.AddCommandVA( "del \"%s\"", ppParms[1] );
			}
			else if(cmd.iSpecialCmd == CCRenameFile && iArg == 3)
			{
				RemoveQuotes(ppParms[1]);
				RemoveQuotes(ppParms[2]);
				commandExecuter.AddCommandVA( "ren \"%s\" \"%s\"", ppParms[1], ppParms[2] );
			}
			else if(cmd.iSpecialCmd == CCChangeDir && iArg == 2)
			{
				RemoveQuotes(ppParms[1]);
				commandExecuter.AddCommandVA( "cd \"%s\"", ppParms[1] );
			}
			else if(cmd.iSpecialCmd == CCGenerateGridNav && iArg == 2 )
			{
				CMapDoc* pMapDoc = CMapDoc::GetActiveMapDoc();
				if ( pMapDoc )
				{
					CGridNav* pGridNav = pMapDoc->GetGridNav();
					if ( pGridNav && pGridNav->IsEnabled() )
					{
						RemoveQuotes(ppParms[1]);
						pGridNav->GenerateGridNavFile( ppParms[1] );
					}
				}
			}
		}
		else
		{
			// Change to the game exe folder before spawning the engine.
			// This is necessary for Steam to find the correct Steam DLL (it
			// uses the current working directory to search).
			char szDir[MAX_PATH];
			V_strncpy( szDir, szNewRun, sizeof(szDir) );
			RemoveQuotes( szDir );
			V_StripFilename( szDir );
			commandExecuter.AddCommandVA( "cd \"%s\"", szDir );

			commandExecuter.AddCommandWithArgList( ppParms );
		}
	}

	commandExecuter.Launch();

	s_bRunsCommands = false;

	return TRUE;
}

