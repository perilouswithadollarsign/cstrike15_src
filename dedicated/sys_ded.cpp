//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include <stdio.h>
#include <stdlib.h>
#include "isys.h"
#include "conproc.h"
#include "dedicated.h"
#include "engine_hlds_api.h"
#include "checksum_md5.h"
#include "mathlib/mathlib.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "idedicatedexports.h"
#include "vgui/vguihelpers.h"
#include "appframework/AppFramework.h"
#include "filesystem_init.h"
#include "tier2/tier2.h"
#include "dedicated.h"
#include "vstdlib/cvar.h"
#ifdef LINUX
#include <mcheck.h>
#endif

#ifdef _WIN32
#include <windows.h> 
#include <direct.h>
#include "keyvalues.h"
// filesystem_steam.cpp implements this useful function - mount all the caches for a given app ID.
extern void MountDependencies( int iAppId, CUtlVector<unsigned int> &depList );
#else
#define _chdir chdir
#include <unistd.h>
#endif

void* FileSystemFactory( const char *pName, int *pReturnCode );
bool InitInstance( );
void ProcessConsoleInput( void );
const char *UTIL_GetExecutableDir( );
bool NET_Init( void );
void NET_Shutdown( void );
const char *UTIL_GetBaseDir( void );
bool g_bVGui = false;

#if defined( CSTRIKE15 )
const char *g_gameName = "csgo";
#else
const char *g_gameName = "hl2";
#endif

#if defined ( _WIN32 )
#include "console/TextConsoleWin32.h"
CTextConsoleWin32 console;
#else
#include "console/TextConsoleUnix.h"
CTextConsoleUnix console;
#endif

extern char *gpszCvars;

IDedicatedServerAPI *engine = NULL;

int g_nSubProcessId = 0;

#ifdef POSIX
extern char g_szEXEName[ 256 ];
#endif

class CDedicatedServerLoggingListener : public ILoggingListener
{
public:
	virtual void Log( const LoggingContext_t *pContext, const tchar *pMessage )
	{
		if ( sys )
		{
			if ( g_nSubProcessId )
			{
				sys->Printf( " #%0x2d:%s", g_nSubProcessId, pMessage );
			}
			else
			{
				sys->Printf( "#%s", pMessage );
			}
		}
#ifdef _WIN32
		Plat_DebugString( pMessage );
#endif

		if ( pContext->m_Severity == LS_ERROR )
		{
			// In Windows vgui mode, make a message box or they won't ever see the error.
#ifdef _WIN32
			if ( g_bVGui )
			{
				MessageBox( NULL, pMessage, "Error", MB_OK | MB_TASKMODAL );
			}
			TerminateProcess( GetCurrentProcess(), 1 );
#elif POSIX
			fflush(stdout);
			_exit(1);
#else
#error "Implement me"
#endif
		}
	}
};


#if defined(POSIX) && !defined(_PS3)
#define MAX_LINUX_CMDLINE 2048
static char linuxCmdline[ MAX_LINUX_CMDLINE +7 ]; // room for -steam

void BuildCmdLine( int argc, char **argv )
{
	int len;
	int i;
	
	for (len = 0, i = 0; i < argc; i++)
	{
		len += strlen(argv[i]);
	}
	
	if ( len > MAX_LINUX_CMDLINE )
	{
		printf( "command line too long, %i max\n", MAX_LINUX_CMDLINE );
		exit(-1);
		return;
	}
	
	linuxCmdline[0] = '\0';
	for ( i = 0; i < argc; i++ )
	{
		if ( i > 0 )
		{
			strcat( linuxCmdline, " " );
		}
		strcat( linuxCmdline, argv[ i ] );
	}
	strcat( linuxCmdline, " -steam" );
}

char *GetCommandLine()
{
	return linuxCmdline;
}
#endif

static CNonFatalLoggingResponsePolicy s_NonFatalLoggingResponsePolicy;
static CDedicatedServerLoggingListener s_DedicatedServerLoggingListener;

bool RunServerIteration( bool bSupressStdIOBecauseWeAreAForkedChild )
{
	bool bDone = false;
		
#if defined ( _WIN32 )
	MSG msg;

	while( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
	{
		//if (!GetMessage( &msg, NULL, 0, 0))
		if ( msg.message == WM_QUIT )
		{
			bDone = true;
			break;
		}

		TranslateMessage( &msg );
		DispatchMessage( &msg );
	}

	if  ( IsPC() )
	{
		// NOTE: Under some implementations of Win9x, 
		// dispatching messages can cause the FPU control word to change
		SetupFPUControlWord();
	}

	if ( bDone /*|| gbAppHasBeenTerminated*/ )
		return bDone;
#endif // _WIN32

	if ( g_bVGui )
	{
#ifdef _WIN32
		RunVGUIFrame();
#endif
	}
	else
	{
		if (! bSupressStdIOBecauseWeAreAForkedChild )
		{
			// Calling ProcessConsoleInput can cost about a tenth of a millisecond.
			// We used to call it up to 1,000 times a second. Even calling it once
			// a frame is wasteful since the console hardly needs that level of
			// responsiveness, and calling it too frequently is a waste of CPU time
			// and power.
			static int s_nProcessCount;
			// Don't set this too high since the users keystrokes are not reflected
			// until this ProcessConsoleInput is called.
			const int nConsoleInputFrames = 5;
			++s_nProcessCount;
			if ( s_nProcessCount > nConsoleInputFrames )
			{
				s_nProcessCount = 0;
				ProcessConsoleInput();
			}
		}
	}

	if ( !engine->RunFrame() )
	{
		bDone = true;
	}

	sys->UpdateStatus( 0  /* don't force */ );

	return bDone;
}

//-----------------------------------------------------------------------------
//
//  Server loop
//
//-----------------------------------------------------------------------------
void RunServer( bool bSupressStdIOBecauseWeAreAForkedChild )
{


#ifdef _WIN32
	if(gpszCvars)
	{
		engine->AddConsoleText(gpszCvars);
	}
#endif

	// run 2 engine frames first to get the engine to load its resources
	if (g_bVGui)
	{
#ifdef _WIN32
		RunVGUIFrame();
#endif
	}
	if ( !engine->RunFrame() )
	{
		return;
	}
	if (g_bVGui)
	{
#ifdef _WIN32
		RunVGUIFrame();
#endif
	}

	if ( !engine->RunFrame() )
	{
		return;
	}

	if (g_bVGui)
	{
#ifdef _WIN32
		VGUIFinishedConfig();
		RunVGUIFrame();
#endif
	}
	
	bool bDone = false;
	while ( ! bDone )
	{
		bDone = RunServerIteration( bSupressStdIOBecauseWeAreAForkedChild );
	}
}

//-----------------------------------------------------------------------------
//
// initialize the console or wait for vgui to start the server
//
//-----------------------------------------------------------------------------
bool ConsoleStartup( CreateInterfaceFn dedicatedFactory )
{
#ifdef _WIN32
	if ( g_bVGui )
	{
		StartVGUI( dedicatedFactory );
		RunVGUIFrame();
		// Run the config screen
		while (VGUIIsInConfig()	&& VGUIIsRunning())
		{
			RunVGUIFrame();
		}

		if ( VGUIIsStopping() )
		{
			return false;
		}
	}
	else
#endif // _WIN32
	{
		if ( !console.Init() )
		{
			return false;	 
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
// Instantiate all main libraries
//-----------------------------------------------------------------------------
bool CDedicatedAppSystemGroup::Create( )
{
	// Hook the debug output stuff (override the spew func in the appframework)
	LoggingSystem_PushLoggingState();
	LoggingSystem_SetLoggingResponsePolicy( &s_NonFatalLoggingResponsePolicy );
	LoggingSystem_RegisterLoggingListener( &s_DedicatedServerLoggingListener );

	// Added the dedicated exports module for the engine to grab
	AppModule_t dedicatedModule = LoadModule( Sys_GetFactoryThis() );
	IAppSystem *pSystem = AddSystem( dedicatedModule, VENGINE_DEDICATEDEXPORTS_API_VERSION );
	if ( !pSystem )
		return false;

	return sys->LoadModules( this );
}

bool CDedicatedAppSystemGroup::PreInit( )
{
	// A little hack needed because dedicated links directly to filesystem .cpp files
	g_pFullFileSystem = NULL;

	if ( !BaseClass::PreInit() )
		return false;

	CFSSteamSetupInfo steamInfo;
	steamInfo.m_pDirectoryName = NULL;
	steamInfo.m_bOnlyUseDirectoryName = false;
	steamInfo.m_bToolsMode = false;
	steamInfo.m_bSetSteamDLLPath = false;
	steamInfo.m_bSteam = g_pFullFileSystem->IsSteam();
	steamInfo.m_bNoGameInfo = steamInfo.m_bSteam;
	if ( FileSystem_SetupSteamEnvironment( steamInfo ) != FS_OK )
		return false;

	CFSMountContentInfo fsInfo;
	fsInfo.m_pFileSystem = g_pFullFileSystem;
	fsInfo.m_bToolsMode = false;
	fsInfo.m_pDirectoryName = steamInfo.m_GameInfoPath;

	if ( FileSystem_MountContent( fsInfo ) != FS_OK )
		return false;

	if ( !NET_Init() )
		return false;

	// Needs to be done prior to init material system config
	CFSSearchPathsInit initInfo;

	initInfo.m_pFileSystem = g_pFullFileSystem;
	initInfo.m_pDirectoryName = CommandLine()->ParmValue( "-game" );

	// Load gameinfo.txt and setup all the search paths, just like the tools do.
	FileSystem_LoadSearchPaths( initInfo );

#ifdef _WIN32
	if ( CommandLine()->CheckParm( "-console" ) )
	{
		g_bVGui = false;
	}
	else
	{
		g_bVGui = true;
	}
#else
	// no VGUI under linux
	g_bVGui = false; 
#endif

	if ( !g_bVGui )
	{
		if ( !sys->CreateConsoleWindow() )
			return false;
	}

	return true;
}

int CDedicatedAppSystemGroup::Main( )
{
	if ( !ConsoleStartup( GetFactory() ) )
		return -1;

#ifdef _WIN32
	if ( g_bVGui )
	{
		RunVGUIFrame();
	}
	else
	{
		// mount the caches
		if (CommandLine()->CheckParm("-steam"))
		{
			// Add a search path for the base dir
			char fullLocationPath[MAX_PATH];
			if ( _getcwd( fullLocationPath, MAX_PATH ) )
			{
				g_pFullFileSystem->AddSearchPath( fullLocationPath, "MAIN" );
			}

			// Find the gameinfo.txt for our mod and mount it's caches
			char gameInfoFilename[MAX_PATH];
			Q_snprintf( gameInfoFilename, sizeof(gameInfoFilename) - 1, "%s\\gameinfo.txt", CommandLine()->ParmValue( "-game", g_gameName ) );
			KeyValues *gameData = new KeyValues( "GameInfo" );
			if ( gameData->LoadFromFile( g_pFullFileSystem, gameInfoFilename ) )
			{
				KeyValues *pFileSystem = gameData->FindKey( "FileSystem" );
				int iAppId = pFileSystem->GetInt( "SteamAppId" );
				if ( iAppId )
				{
					CUtlVector<unsigned int> depList;
					MountDependencies( iAppId, depList );
				}
			}
			gameData->deleteThis();

			// remove our base search path
			g_pFullFileSystem->RemoveSearchPaths( "MAIN" );
		}
	}
#endif

	// Set up mod information
	ModInfo_t info;
	info.m_pInstance = GetAppInstance();
	info.m_pBaseDirectory = UTIL_GetBaseDir();
	info.m_pInitialMod = CommandLine()->ParmValue( "-game", g_gameName );
	info.m_pInitialGame = CommandLine()->ParmValue( "-defaultgamedir", g_gameName );
	info.m_pParentAppSystemGroup = this;
	info.m_bTextMode = CommandLine()->CheckParm( "-textmode" ) ? true : false;

	if ( engine->ModInit( info ) )
	{
		engine->ModShutdown();
	} // if engine->ModInit

	return 0;
}

void CDedicatedAppSystemGroup::PostShutdown()
{
#ifdef _WIN32
	if ( g_bVGui )
	{
		StopVGUI();
	}
#endif
	sys->DestroyConsoleWindow();
	console.ShutDown();
	NET_Shutdown();
	BaseClass::PostShutdown();
}

void CDedicatedAppSystemGroup::Destroy() 
{
	LoggingSystem_PopLoggingState();
}


//-----------------------------------------------------------------------------
// Gets the executable name
//-----------------------------------------------------------------------------
bool GetExecutableName( char *out, int nMaxLen )
{
#ifdef _WIN32
	if ( !::GetModuleFileName( ( HINSTANCE )GetModuleHandle( NULL ), out, nMaxLen ) )
	{
		return false;
	}
	return true;
#elif POSIX
	Q_strncpy( out, g_szEXEName, nMaxLen );
	return true;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------
void UTIL_ComputeBaseDir( char *pBaseDir, int nMaxLen )
{
	int j;
	char *pBuffer = NULL;

	pBaseDir[ 0 ] = 0;

	if ( GetExecutableName( pBaseDir, nMaxLen ) )
	{
		pBuffer = strrchr( pBaseDir, CORRECT_PATH_SEPARATOR );
		if ( pBuffer && *pBuffer )
		{
			*(pBuffer+1) = '\0';
		}

		j = strlen( pBaseDir );
		if (j > 0)
		{
			if ( ( pBaseDir[ j-1 ] == '\\' ) || 
				 ( pBaseDir[ j-1 ] == '/' ) )
			{
				pBaseDir[ j-1 ] = 0;
			}
		}
	}

	char const *pOverrideDir = CommandLine()->CheckParm( "-basedir" );
	if ( pOverrideDir )
	{
		strcpy( pBaseDir, pOverrideDir );
	}

	Q_strlower( pBaseDir );
	Q_FixSlashes( pBaseDir );
}


//-----------------------------------------------------------------------------
// This class is a helper class used for steam-based applications.
// It loads up the file system in preparation for using it to load other
// required modules from steam.
//
// I couldn't use the one in appframework because the dedicated server
// inlines all the filesystem code.
//-----------------------------------------------------------------------------
class CDedicatedSteamApplication : public CSteamApplication
{
public:
	CDedicatedSteamApplication( CSteamAppSystemGroup *pAppSystemGroup );
	virtual bool Create( );
};


//-----------------------------------------------------------------------------
// This class is a helper class used for steam-based applications.
// It loads up the file system in preparation for using it to load other
// required modules from steam.
//
// I couldn't use the one in appframework because the dedicated server
// inlines all the filesystem code.
//-----------------------------------------------------------------------------
CDedicatedSteamApplication::CDedicatedSteamApplication( CSteamAppSystemGroup *pAppSystemGroup ) : CSteamApplication( pAppSystemGroup )
{
}


//-----------------------------------------------------------------------------
// Implementation of IAppSystemGroup
//-----------------------------------------------------------------------------
bool CDedicatedSteamApplication::Create( )
{
	// Add in the cvar factory
	AppModule_t cvarModule = LoadModule( VStdLib_GetICVarFactory() );
	AddSystem( cvarModule, CVAR_INTERFACE_VERSION );

	AppModule_t fileSystemModule = LoadModule( FileSystemFactory );
	m_pFileSystem = (IFileSystem*)AddSystem( fileSystemModule, FILESYSTEM_INTERFACE_VERSION );

	if ( !m_pFileSystem )
	{
		Warning( "Unable to load the file system!\n" );
		return false;
	}

	return true;
}

static bool s_GameInfoSuggestFN( CFSSteamSetupInfo const *pFsSteamSetupInfo, char *pchPathBuffer, int nBufferLength, bool *pbBubbleDirectories )
{
	V_strncpy( pchPathBuffer, "left4dead", nBufferLength );
	return true;
}




//-----------------------------------------------------------------------------
//
// Main entry point for dedicated server, shared between win32 and linux
//
//-----------------------------------------------------------------------------

int main(int argc, char **argv)
{
#if !defined( POSIX ) && !defined( _WIN64 )
	_asm
	{
		fninit
	}
#endif

	SetupFPUControlWord();

#ifdef POSIX
	strcpy(g_szEXEName, *argv);
	// Store off command line for argument searching
	BuildCmdLine(argc, argv);
#endif

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

	// Store off command line for argument searching
	CommandLine()->CreateCmdLine( GetCommandLine() );
#ifndef _WIN32
	Plat_SetCommandLine( CommandLine()->GetCmdLine() );
#endif

#ifdef LINUX
	if ( CommandLine()->CheckParm( "-mtrace" ) )
	{
		mtrace();
	}
#ifndef DEDICATED
	if ( CommandLine()->CheckParm( "-logmem" ) )
	{
		EnableMemoryLogging( true );
	}
#endif
#endif

	// Figure out the directory the executable is running from
	// and make that be the current working directory
	char pBasedir[ MAX_PATH ];
	UTIL_ComputeBaseDir( pBasedir, MAX_PATH );
	_chdir( pBasedir );

	// Rehook the command line.
	CommandLine()->CreateCmdLine( GetCommandLine() );

	if ( !InitInstance() )
		return -1;

	SetSuggestGameInfoDirFn( s_GameInfoSuggestFN );
	CDedicatedAppSystemGroup dedicatedSystems;
	CDedicatedSteamApplication steamApplication( &dedicatedSystems );
	int nRet = steamApplication.Run( );

#ifdef LINUX
#ifndef DEDICATED

	EnableMemoryLogging( false );

	if ( CommandLine()->CheckParm( "-mtrace" ) )
	{
		muntrace();
	}
#endif
#endif
	return nRet;

}
