//===== Copyright  Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// Defines the entry point for the application.
//
//==================================================================//

#if defined( _WIN32 )
#if !defined( _X360 )
#include <windows.h>
#include "shlwapi.h" // registry stuff
#include <direct.h>
#endif
#elif defined ( OSX ) 
#include <Carbon/Carbon.h>
#elif defined ( LINUX )
#define O_EXLOCK 0
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <locale.h>
#elif defined ( _PS3 )
// nothing here for now...
#else
#error
#endif
#include "appframework/ilaunchermgr.h"
#include <stdio.h>
#include "tier0/icommandline.h"
#include "engine_launcher_api.h"
#include "ifilesystem.h"
#include "tier1/interface.h"
#include "tier0/dbg.h"
#include "iregistry.h"
#include "appframework/iappsystem.h"
#include "appframework/AppFramework.h"
#include <vgui/vgui.h>
#include <vgui/ISurface.h>
#include "tier0/platform.h"
#include "tier0/memalloc.h"
#include "datacache/iresourceaccesscontrol.h"
#include "filesystem.h"
#include "tier1/utlrbtree.h"
#include "materialsystem/imaterialsystem.h"
#include "istudiorender.h"
#include "vgui/IVGui.h"
#include "IHammer.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "vphysics_interface.h"
#include "filesystem_init.h"
#include "vstdlib/iprocessutils.h"
#include "avi/iavi.h"
#include "avi/ibik.h"
#include "avi/iquicktime.h"
#include "tier1/tier1.h"
#include "tier2/tier2.h"
#include "tier3/tier3.h"
#include "p4lib/ip4.h"
#include "inputsystem/iinputsystem.h"
#include "filesystem/IQueuedLoader.h"
#include "filesystem/IXboxInstaller.h"
#include "reslistgenerator.h"
#include "tier1/fmtstr.h"
#include "steam/steam_api.h"
#include "vscript/ivscript.h"
#include "tier0/miniprofiler.h"
#include "networksystem/inetworksystem.h"
#include "tier1/fmtstr.h"
#include "vjobs_interface.h"
#include "vstdlib/jobthread.h"

#if defined( _PS3 )
#include "sys/ppu_thread.h"
#include "ps3/ps3_win32stubs.h"
#include "ps3/ps3_core.h"
#include "ps3/ps3_helpers.h"
#include "ps3/ps3_console.h"
#include "../public/ps3_pathinfo.h"
#include "../public/tls_ps3.h"
#elif defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#include "xbox/xbox_console.h"
#include "xbox/xbox_launch.h"
#endif

#ifdef LINUX
#include "SDL.h"

#define MB_OK 			0x00000001
#define MB_SYSTEMMODAL	0x00000002
#define MB_ICONERROR	0x00000004
int MessageBox( HWND hWnd, const char *message, const char *header, unsigned uType );
#endif

#ifdef OSX
#define RELAUNCH_FILE "/tmp/hl2_relaunch"
#endif

#if defined( DEVELOPMENT_ONLY ) || defined( ALLOW_TEXT_MODE )
#define ALLOW_MULTI_CLIENTS_PER_MACHINE 1
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined ( CSTRIKE15 )

#define DEFAULT_HL2_GAMEDIR	"csgo"

#else

#define DEFAULT_HL2_GAMEDIR	"hl2"

#endif // CSTRIKE15

// A logging channel used during engine initialization
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_EngineInitialization, "EngineInitialization" );
#if defined( USE_SDL )
extern void* CreateSDLMgr();
#elif defined( OSX )
extern void* CreateCCocoaMgr();
#endif


#define SIXENSE
#ifdef SIXENSE
extern bool DoesFileExistIn( const char *pDirectoryName, const char *pFilename );
#endif

//-----------------------------------------------------------------------------
// Modules...
//-----------------------------------------------------------------------------
static IEngineAPI *g_pEngineAPI;
static IHammer *g_pHammer;

bool g_bTextMode = false;

#ifndef _PS3
static char g_szBasedir[MAX_PATH];
static char g_szGamedir[MAX_PATH];
#else
#endif

// copied from sys.h
struct FileAssociationInfo
{
	char const  *extension;
	char const  *command_to_issue;
};

static FileAssociationInfo g_FileAssociations[] =
{
	{ ".dem", "playdemo" },
	{ ".sav", "load" },
	{ ".bsp", "map" },
};

#ifdef _WIN32
#pragma warning(disable:4073)
#pragma init_seg(lib)
#endif

class CLeakDump
{
public:
	CLeakDump()
	 :	m_bCheckLeaks( false )
	{
	}

	~CLeakDump()
	{
		if ( m_bCheckLeaks )
		{
			g_pMemAlloc->DumpStats();
		}
	}

	bool m_bCheckLeaks;
} g_LeakDump;

class CLauncherLoggingListener : public ILoggingListener
{
public:
	virtual void Log( const LoggingContext_t *pContext, const tchar *pMessage )
	{
#if !defined( _CERT ) && !defined( _PS3 )
#if defined ( WIN32 ) || defined( LINUX )
		if ( pContext->m_Severity == LS_WARNING && pContext->m_ChannelID == LOG_EngineInitialization )
		{
			::MessageBox( NULL, pMessage, "Warning!", MB_OK | MB_SYSTEMMODAL | MB_ICONERROR );
		}
		else if ( pContext->m_Severity == LS_ASSERT && !ShouldUseNewAssertDialog() )
		{
			::MessageBox( NULL, pMessage, "Assert!", MB_OK | MB_SYSTEMMODAL | MB_ICONERROR );
		}
		else if ( pContext->m_Severity == LS_ERROR )
		{
			::MessageBox( NULL, pMessage, "Error!", MB_OK | MB_SYSTEMMODAL | MB_ICONERROR );
		}
#elif defined(OSX)
		CFOptionFlags responseFlags;
		CFStringRef message;
		message = CFStringCreateWithCString(NULL, pMessage, CFStringGetSystemEncoding() ) ;
		
		if ( pContext->m_Severity == LS_WARNING && pContext->m_ChannelID == LOG_EngineInitialization )
		{
			CFUserNotificationDisplayAlert(0, kCFUserNotificationCautionAlertLevel, 0, 0, 0, CFSTR( "Warning" ), message, NULL, NULL, NULL, &responseFlags);
		}
		else if ( pContext->m_Severity == LS_ASSERT && !ShouldUseNewAssertDialog() )
		{
			CFUserNotificationDisplayAlert(0, kCFUserNotificationNoteAlertLevel, 0, 0, 0, CFSTR( "Assert" ), message, NULL, NULL, NULL, &responseFlags);
		}
		else if ( pContext->m_Severity == LS_ERROR )
		{
			CFUserNotificationDisplayAlert(0,  kCFUserNotificationStopAlertLevel, 0, 0, 0, CFSTR( "Error" ), message, NULL, NULL, NULL, &responseFlags);
		}	
		CFRelease(message);
#else
#warning "Popup a dialog here"
#endif
#endif // CERT
	}
};

#if defined( _PS3 )
const char *GetGameDirectory( void )
{
	return g_pPS3PathInfo->GameImagePath();
}
#else
//-----------------------------------------------------------------------------
// Purpose: Return the game directory
// Output : char
//-----------------------------------------------------------------------------
char *GetGameDirectory( void )
{
	return g_szGamedir;
}

void SetGameDirectory( const char *game )
{
	Q_strncpy( g_szGamedir, game, sizeof(g_szGamedir) );
}
#endif

//-----------------------------------------------------------------------------
// Gets the executable name
//-----------------------------------------------------------------------------
bool GetExecutableName( char *out, int outSize )
{
#ifdef WIN32
	if ( !::GetModuleFileName( ( HINSTANCE )GetModuleHandle( NULL ), out, outSize ) )
	{
		return false;
	}
	return true;
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Return the executable name
// Output : char
//-----------------------------------------------------------------------------
const char * GetExecutableFilename()
{
#ifdef _PS3
	return "csgo";
#else // !_PS3
	char exepath[MAX_PATH];
	static char filename[MAX_PATH];

#ifdef WIN32
	filename[0] = 0;
	if ( GetExecutableName( exepath, sizeof( exepath ) ) )
	{
		_splitpath
		( 
			exepath, // Input
			NULL,  // drive
			NULL,  // dir
			filename, // filename
			NULL // extension
		);
	}

	Q_strlower( filename );
#else
	filename[0] = 0;
#endif
	return filename;
#endif // _PS3
}

#if !defined(_PS3)
//-----------------------------------------------------------------------------
// Purpose: Return the base directory
// Output : char
//-----------------------------------------------------------------------------
char *GetBaseDirectory( void )
{
	return g_szBasedir;
}
#else
const char *GetBaseDirectory( void )
{
	return g_pPS3PathInfo->GameImagePath();
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Determine the directory where this .exe is running from
//-----------------------------------------------------------------------------
void UTIL_ComputeBaseDir()
{
#ifndef _PS3
	g_szBasedir[0] = 0;

	if ( IsX360() )
	{
		char const *pBaseDir = CommandLine()->ParmValue( "-basedir" );
		if ( pBaseDir )
		{
			strcpy( g_szBasedir, pBaseDir );
		}
	}

	if ( !g_szBasedir[0] && GetExecutableName( g_szBasedir, sizeof( g_szBasedir ) ) )
	{
		char *pBuffer = strrchr( g_szBasedir, '\\' );
		if ( *pBuffer )
		{
			*(pBuffer+1) = '\0';
		}

		int j = strlen( g_szBasedir );
		if (j > 0)
		{
			if ( ( g_szBasedir[j-1] == '\\' ) || 
				 ( g_szBasedir[j-1] == '/' ) )
			{
				g_szBasedir[j-1] = 0;
			}
		}
	}

	if ( IsPC() )
	{
		char const *pOverrideDir = CommandLine()->CheckParm( "-basedir" );
		if ( pOverrideDir )
		{
			strcpy( g_szBasedir, pOverrideDir );
		}
	}

#ifdef WIN32
	Q_strlower( g_szBasedir );
#endif
	Q_FixSlashes( g_szBasedir );

#else
	

#endif
}

#ifdef WIN32
BOOL WINAPI MyHandlerRoutine( DWORD dwCtrlType )
{
#if !defined( _X360 )
	TerminateProcess( GetCurrentProcess(), 2 );
#endif
	return TRUE;
}
#endif

void InitTextMode()
{
#ifdef WIN32
#if !defined( _X360 )
	AllocConsole();

	SetConsoleCtrlHandler( MyHandlerRoutine, TRUE );

	freopen( "CONIN$", "rb", stdin );		// reopen stdin handle as console window input
	freopen( "CONOUT$", "wb", stdout );		// reopen stout handle as console window output
	freopen( "CONOUT$", "wb", stderr );		// reopen stderr handle as console window output
#else
	XBX_Error( "%s %s: Not Supported", __FILE__, __LINE__ );
#endif
#endif
}

void SortResList( char const *pchFileName, char const *pchSearchPath );

#define ALL_RESLIST_FILE	"all.lst"
#define ENGINE_RESLIST_FILE  "engine.lst"

#ifdef _PS3

void TryToLoadSteamOverlayDLL()
{
}

#else // !_PS3

// create file to dump out to
class CLogAllFiles
{
public:
	CLogAllFiles();
	void Init();
	void Shutdown();
	void LogFile( const char *fullPathFileName, const char *options );

private:
	static void LogAllFilesFunc( const char *fullPathFileName, const char *options );
	void LogToAllReslist( char const *line );

	bool		m_bActive;
	char		m_szCurrentDir[_MAX_PATH];

	// persistent across restarts
	CUtlRBTree< CUtlString, int > m_Logged;
	CUtlString	m_sResListDir;
	CUtlString	m_sFullGamePath;
};

static CLogAllFiles g_LogFiles;

static bool AllLogLessFunc( CUtlString const &pLHS, CUtlString const &pRHS )
{
	return CaselessStringLessThan( pLHS.Get(), pRHS.Get() );
}

CLogAllFiles::CLogAllFiles() :
	m_bActive( false ),
	m_Logged( 0, 0, AllLogLessFunc )
{
	MEM_ALLOC_CREDIT();
	m_sResListDir = "reslists";
}

void CLogAllFiles::Init()
{
	if ( IsX360() )
	{
		return;
	}

	// Can't do this in edit mode
	if ( CommandLine()->CheckParm( "-edit" ) )
	{
		return;
	}

	if ( !CommandLine()->CheckParm( "-makereslists" ) )
	{
		return;
	}

	m_bActive = true;

	char const *pszDir = NULL;
	if ( CommandLine()->CheckParm( "-reslistdir", &pszDir ) && pszDir )
	{
		char szDir[ MAX_PATH ];
		Q_strncpy( szDir, pszDir, sizeof( szDir ) );
		Q_StripTrailingSlash( szDir );
#ifdef WIN32
		Q_strlower( szDir );
#endif
		Q_FixSlashes( szDir );
		if ( Q_strlen( szDir ) > 0 )
		{
			m_sResListDir = szDir;
		}
	}

	// game directory has not been established yet, must derive ourselves
	char path[MAX_PATH];
	Q_snprintf( path, sizeof(path), "%s/%s", GetBaseDirectory(), CommandLine()->ParmValue( "-game", "hl2" ) );
	Q_FixSlashes( path );
#ifdef WIN32
	Q_strlower( path );
#endif
	m_sFullGamePath = path;

	// create file to dump out to
	char szDir[ MAX_PATH ];
	V_snprintf( szDir, sizeof( szDir ), "%s\\%s", m_sFullGamePath.String(), m_sResListDir.String() );
	g_pFullFileSystem->CreateDirHierarchy( szDir, "GAME" );

	g_pFullFileSystem->AddLoggingFunc( &LogAllFilesFunc );

	if ( !CommandLine()->FindParm( "-startmap" ) && !CommandLine()->FindParm( "-startstage" ) )
	{
		m_Logged.RemoveAll();
		g_pFullFileSystem->RemoveFile( CFmtStr( "%s\\%s\\%s", m_sFullGamePath.String(), m_sResListDir.String(), ALL_RESLIST_FILE ), "GAME" );
	}

#ifdef WIN32
	::GetCurrentDirectory( sizeof(m_szCurrentDir), m_szCurrentDir );
	Q_strncat( m_szCurrentDir, "\\", sizeof(m_szCurrentDir), 1 );
	_strlwr( m_szCurrentDir );
#else
	getcwd( m_szCurrentDir, sizeof(m_szCurrentDir) );
	Q_strncat( m_szCurrentDir, "/", sizeof(m_szCurrentDir), 1 );
#endif
}

void CLogAllFiles::Shutdown()
{
	if ( !m_bActive )
		return;

	m_bActive = false;

	if ( CommandLine()->CheckParm( "-makereslists" ) )
	{
		g_pFullFileSystem->RemoveLoggingFunc( &LogAllFilesFunc );
	}

	// Now load and sort all.lst
	SortResList( CFmtStr( "%s\\%s\\%s", m_sFullGamePath.String(), m_sResListDir.String(), ALL_RESLIST_FILE ), "GAME" );
	// Now load and sort engine.lst
	SortResList( CFmtStr( "%s\\%s\\%s", m_sFullGamePath.String(), m_sResListDir.String(), ENGINE_RESLIST_FILE ), "GAME" );

	m_Logged.Purge();
}

void CLogAllFiles::LogToAllReslist( char const *line )
{
	// Open for append, write data, close.
	FileHandle_t fh = g_pFullFileSystem->Open( CFmtStr( "%s\\%s\\%s", m_sFullGamePath.String(), m_sResListDir.String(), ALL_RESLIST_FILE ), "at", "GAME" );
	if ( fh != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFullFileSystem->Write("\"", 1, fh);
		g_pFullFileSystem->Write( line, Q_strlen(line), fh );
		g_pFullFileSystem->Write("\"\n", 2, fh);
		g_pFullFileSystem->Close( fh );
	}
}

void CLogAllFiles::LogFile(const char *fullPathFileName, const char *options)
{
	if ( !m_bActive )
	{
		Assert( 0 );
		return;
	}

	// write out to log file
	Assert( fullPathFileName[1] == ':' );

	int idx = m_Logged.Find( fullPathFileName );
	if ( idx != m_Logged.InvalidIndex() )
	{
		return;
	}

	m_Logged.Insert( fullPathFileName );

	// make it relative to our root directory
	const char *relative = Q_stristr( fullPathFileName, GetBaseDirectory() );
	if ( relative )
	{
		relative += ( Q_strlen( GetBaseDirectory() ) + 1 );

		char rel[ MAX_PATH ];
		Q_strncpy( rel, relative, sizeof( rel ) );
#ifdef WIN32
		Q_strlower( rel );
#endif
		Q_FixSlashes( rel );
		
		LogToAllReslist( rel );
	}
}

//-----------------------------------------------------------------------------
// Purpose: callback function from filesystem
//-----------------------------------------------------------------------------
void CLogAllFiles::LogAllFilesFunc(const char *fullPathFileName, const char *options)
{
	g_LogFiles.LogFile( fullPathFileName, options );
}

//-----------------------------------------------------------------------------
// Purpose: This is a bit of a hack because it appears 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
static bool IsWin98OrOlder()
{
	bool retval = false;

#if defined( WIN32 ) && !defined( _X360 )
	OSVERSIONINFOEX osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	
	BOOL bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *) &osvi);
	if( !bOsVersionInfoEx )
	{
		// If OSVERSIONINFOEX doesn't work, try OSVERSIONINFO.
		osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
		if ( !GetVersionEx ( (OSVERSIONINFO *) &osvi) )
		{
			Error( "IsWin98OrOlder:  Unable to get OS version information" );
		}
	}

	switch (osvi.dwPlatformId)
	{
	case VER_PLATFORM_WIN32_NT:
		// NT, XP, Win2K, etc. all OK for SSE
		break;
	case VER_PLATFORM_WIN32_WINDOWS:
		// Win95, 98, Me can't do SSE
		retval = true;
		break;
	case VER_PLATFORM_WIN32s:
		// Can't really run this way I don't think...
		retval = true;
		break;
	default:
		break;
	}
#endif

	return retval;
}


//-----------------------------------------------------------------------------
// Purpose: Figure out if Steam is running, then load the GameOverlayRenderer.dll
//-----------------------------------------------------------------------------
void TryToLoadSteamOverlayDLL()
{
#if defined( WIN32 ) && !defined( _X360 )
	// First, check if the module is already loaded, perhaps because we were run from Steam directly
	HMODULE hMod = GetModuleHandle( "GameOverlayRenderer.dll" );
	if ( hMod )
	{
		return;
	}

	const char *pchSteamInstallPath = SteamAPI_GetSteamInstallPath();
	if ( pchSteamInstallPath )
	{
		char rgchSteamPath[MAX_PATH];
		V_ComposeFileName( pchSteamInstallPath, "GameOverlayRenderer.dll", rgchSteamPath, Q_ARRAYSIZE(rgchSteamPath) );
		// This could fail, but we can't fix it if it does so just ignore failures
		LoadLibrary( rgchSteamPath );
	}
#endif
}

#endif // _PS3

//-----------------------------------------------------------------------------
// Inner loop: initialize, shutdown main systems, load steam to 
//-----------------------------------------------------------------------------
class CSourceAppSystemGroup : public CSteamAppSystemGroup
{
public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit();
	virtual int Main();
	virtual void PostShutdown();
	virtual void Destroy();

private:
	const char *DetermineDefaultMod();
	const char *DetermineDefaultGame();

	bool m_bEditMode;
};


//-----------------------------------------------------------------------------
// The dirty disk error report function
//-----------------------------------------------------------------------------
void ReportDirtyDiskNoMaterialSystem()
{
#ifdef _X360
	for ( int i = 0; i < 4; ++i )
	{
		if ( XUserGetSigninState( i ) != eXUserSigninState_NotSignedIn )
		{
			XShowDirtyDiscErrorUI( i );
			return;
		}
	}
	XShowDirtyDiscErrorUI( 0 );
#endif
}

IVJobs * g_pVJobs = NULL;


//-----------------------------------------------------------------------------
// Instantiate all main libraries
//-----------------------------------------------------------------------------
bool CSourceAppSystemGroup::Create()
{
	COM_TimestampedLog( "CSourceAppSystemGroup::Create()" );

	double start, elapsed;
	start = Plat_FloatTime();

	IFileSystem *pFileSystem = (IFileSystem*)FindSystem( FILESYSTEM_INTERFACE_VERSION );
	pFileSystem->InstallDirtyDiskReportFunc( ReportDirtyDiskNoMaterialSystem );

#ifdef WIN32
	CoInitialize( NULL );
#endif

	// Are we running in edit mode?
	m_bEditMode = CommandLine()->CheckParm( "-edit" ) ? true : false;

	AppSystemInfo_t appSystems[] = 
	{
#define LAUNCHER_APPSYSTEM( name ) name DLL_EXT_STRING
#ifdef _PS3
		{ LAUNCHER_APPSYSTEM( "vjobs" ),                VJOBS_INTERFACE_VERSION },  // Vjobs must shut down after engine and materialsystem
#endif
		{ LAUNCHER_APPSYSTEM( "engine" ),				CVAR_QUERY_INTERFACE_VERSION },	// NOTE: This one must be first!!
		{ LAUNCHER_APPSYSTEM( "filesystem_stdio" ),		QUEUEDLOADER_INTERFACE_VERSION },
#if defined( _X360 )
		{ LAUNCHER_APPSYSTEM( "filesystem_stdio" ),		XBOXINSTALLER_INTERFACE_VERSION },
#endif
		{ LAUNCHER_APPSYSTEM( "inputsystem" ),			INPUTSYSTEM_INTERFACE_VERSION },
		{ LAUNCHER_APPSYSTEM( "vphysics" ),				VPHYSICS_INTERFACE_VERSION },
		{ LAUNCHER_APPSYSTEM( "materialsystem" ),		MATERIAL_SYSTEM_INTERFACE_VERSION },
		{ LAUNCHER_APPSYSTEM( "datacache" ),			DATACACHE_INTERFACE_VERSION },
		{ LAUNCHER_APPSYSTEM( "datacache" ),			MDLCACHE_INTERFACE_VERSION },
		{ LAUNCHER_APPSYSTEM( "datacache" ),			STUDIO_DATA_CACHE_INTERFACE_VERSION },
		{ LAUNCHER_APPSYSTEM( "studiorender" ),			STUDIO_RENDER_INTERFACE_VERSION },
		{ LAUNCHER_APPSYSTEM( "soundemittersystem" ),	SOUNDEMITTERSYSTEM_INTERFACE_VERSION },
		{ LAUNCHER_APPSYSTEM( "vscript" ),				VSCRIPT_INTERFACE_VERSION },
#ifdef WIN32
		{ LAUNCHER_APPSYSTEM("soundsystem"),			SOUNDSYSTEM_INTERFACE_VERSION },
#endif

#if !defined( _GAMECONSOLE )
    #if defined ( AVI_VIDEO )
 		{ LAUNCHER_APPSYSTEM( "valve_avi" ),			AVI_INTERFACE_VERSION },
    #endif 		
    #if defined ( BINK_VIDEO )
 		{ LAUNCHER_APPSYSTEM( "valve_avi" ),			BIK_INTERFACE_VERSION },
 	#endif
	#if defined( QUICKTIME_VIDEO ) 		
 		{ LAUNCHER_APPSYSTEM( "valve_avi" ),			QUICKTIME_INTERFACE_VERSION },
    #endif		
#elif defined( BINK_ENABLED_FOR_CONSOLE )
		{ LAUNCHER_APPSYSTEM( "engine" ),				BIK_INTERFACE_VERSION },	
#endif
		// NOTE: This has to occur before vgui2.dll so it replaces vgui2's surface implementation
		{ LAUNCHER_APPSYSTEM( "vguimatsurface" ),		VGUI_SURFACE_INTERFACE_VERSION },
		{ LAUNCHER_APPSYSTEM( "vgui2" ),				VGUI_IVGUI_INTERFACE_VERSION },
		{ LAUNCHER_APPSYSTEM( "engine" ),				VENGINE_LAUNCHER_API_VERSION },

		{ "", "" }					// Required to terminate the list
	};

#if defined( USE_SDL )
    AddSystem( (IAppSystem *)CreateSDLMgr(),	SDLMGR_INTERFACE_VERSION );
#elif defined( OSX )
	AddSystem( (IAppSystem *)CreateCCocoaMgr(), COCOAMGR_INTERFACE_VERSION );
#endif

	if ( !AddSystems( appSystems ) ) 
		return false;

	// SF4 TODO
	// Windows - See if we need to launch SF4 instead of SF3
	// When we move entirely to SF4 this can go back in appsystems[] where it used to be
#if defined( INCLUDE_SCALEFORM )
   
	if ( CommandLine()->FindParm( "-sf3" ) )
	{
		AppSystemInfo_t scaleformInfo[] =
		{
			{ LAUNCHER_APPSYSTEM( "scaleformui_3" ),		SCALEFORMUI_INTERFACE_VERSION },
			{ "", "" }
		};

		if ( !AddSystems( scaleformInfo ) ) 
		{
			return false;
		}			
	}
	else
	{
		AppSystemInfo_t scaleformInfo[] =
		{
			{ LAUNCHER_APPSYSTEM( "scaleformui" ),		SCALEFORMUI_INTERFACE_VERSION },
			{ "", "" }
		};

		if ( !AddSystems( scaleformInfo ) ) 
			return false;	
	}
#endif // INCLUDE_SCALEFORM
		
	// Hook in datamodel and p4 control if we're running with -tools
	if ( IsPC() && ( ( CommandLine()->FindParm( "-tools" ) && !CommandLine()->FindParm( "-nop4" ) ) || CommandLine()->FindParm( "-p4" ) ) )
	{
		AppModule_t p4libModule = LoadModule( "p4lib.dll" );
		IP4 *p4 = (IP4*)AddSystem( p4libModule, P4_INTERFACE_VERSION );
		
		// If we are running with -steam then that means the tools are being used by an SDK user. Don't exit in this case!
		if ( !p4 && !CommandLine()->FindParm( "-steam" ) )
		{
			return false;
		}
	}

	if ( IsPC() && IsPlatformWindows() )
	{
		AppModule_t vstdlibModule = LoadModule( LAUNCHER_APPSYSTEM( "vstdlib" ) );
		IProcessUtils *processUtils = ( IProcessUtils* )AddSystem( vstdlibModule, PROCESS_UTILS_INTERFACE_VERSION );
		if ( !processUtils )
			return false;
	}

	if ( CommandLine()->FindParm( "-dev" ) )
	{
		// Used to guarantee precache consistency
		AppModule_t datacacheModule = LoadModule( LAUNCHER_APPSYSTEM( "datacache" ) );
		IResourceAccessControl *pResourceAccess = (IResourceAccessControl*)AddSystem( datacacheModule, RESOURCE_ACCESS_CONTROL_INTERFACE_VERSION );
		if ( !pResourceAccess )
			return false;
	}

	// Connect to iterfaces loaded in AddSystems that we need locally
	IMaterialSystem *pMaterialSystem = (IMaterialSystem*)FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION );
	if ( !pMaterialSystem )
		return false;

	g_pEngineAPI = (IEngineAPI*)FindSystem( VENGINE_LAUNCHER_API_VERSION );

	// Load the hammer DLL if we're in editor mode
	if ( m_bEditMode )
	{
		AppModule_t hammerModule = LoadModule( LAUNCHER_APPSYSTEM( "hammer_dll" ) );
		g_pHammer = (IHammer*)AddSystem( hammerModule, INTERFACEVERSION_HAMMER );
		if ( !g_pHammer )
		{
			return false;
		}
	}

	// Load up the appropriate shader DLL
	// This has to be done before connection.
	char const *pDLLName = "shaderapidx9" DLL_EXT_STRING;
	const char* pArg = NULL;
	if ( CommandLine()->FindParm( "-noshaderapi" ) )
	{
		pDLLName = "shaderapiempty" DLL_EXT_STRING;
	}
	if ( CommandLine()->CheckParm( "-shaderapi", &pArg ))
	{
		pDLLName = pArg;
	}
	pMaterialSystem->SetShaderAPI( pDLLName );

	elapsed = Plat_FloatTime() - start;
	COM_TimestampedLog( "CSourceAppSystemGroup::Create() - Took %.4f secs to load libraries and get factories.", (float)elapsed );

	return true;
}

bool CSourceAppSystemGroup::PreInit()
{
	CreateInterfaceFn factory = GetFactory();
	ConnectTier1Libraries( &factory, 1 );
	ConVar_Register( );
	ConnectTier2Libraries( &factory, 1 );
	ConnectTier3Libraries( &factory, 1 );

	if ( !g_pFullFileSystem || !g_pMaterialSystem )
		return false;

#ifdef _PS3
	g_pVJobs = ( IVJobs* )factory( VJOBS_INTERFACE_VERSION, NULL );  // this is done only once; g_pVJobs doesn't change even after multiple reloads of VJobs.prx
	FileSystem_AddSearchPath_Platform( g_pFullFileSystem, GetGameDirectory() );
#else // _PS3
	CFSSteamSetupInfo steamInfo;
	steamInfo.m_bToolsMode = false;
	steamInfo.m_bSetSteamDLLPath = false;
	steamInfo.m_bSteam = g_pFullFileSystem->IsSteam();
	steamInfo.m_bOnlyUseDirectoryName = true;
	steamInfo.m_pDirectoryName = DetermineDefaultMod();
	if ( !steamInfo.m_pDirectoryName )
	{
		steamInfo.m_pDirectoryName = DetermineDefaultGame();
		if ( !steamInfo.m_pDirectoryName )
		{
			Error( "FileSystem_LoadFileSystemModule: no -defaultgamedir or -game specified." );
		}
	}
	if ( FileSystem_SetupSteamEnvironment( steamInfo ) != FS_OK )
		return false;

	CFSMountContentInfo fsInfo;
	fsInfo.m_pFileSystem = g_pFullFileSystem;
	fsInfo.m_bToolsMode = m_bEditMode;
	fsInfo.m_pDirectoryName = steamInfo.m_GameInfoPath;
	if ( FileSystem_MountContent( fsInfo ) != FS_OK )
		return false;

#if defined( SUPPORT_VPK )
	Msg( "start timing %f\n", Plat_FloatTime() );
	char const *pVPKName = CommandLine()->ParmValue( "-vpk" );
	if ( pVPKName )
	{
		fsInfo.m_pFileSystem->AddVPKFile( pVPKName );
	}
#endif

	if ( IsPC() || !IsX360() )
	{
		fsInfo.m_pFileSystem->AddSearchPath( "platform", "PLATFORM" );
	}
	else
	{
		// 360 needs absolute paths
		FileSystem_AddSearchPath_Platform( g_pFullFileSystem, steamInfo.m_GameInfoPath );
	}

	if ( IsPC() )
	{
		// This will get called multiple times due to being here, but only the first one will do anything
		reslistgenerator->Init( GetBaseDirectory(), CommandLine()->ParmValue( "-game", "hl2" ) );

		// This MUST get called each time, but will actually fix up the command line as needed
		reslistgenerator->TickAndFixupCommandLine();
	}

	// FIXME: Logfiles is mod-specific, needs to move into the engine.
	g_LogFiles.Init();

	// Required to run through the editor
	if ( m_bEditMode )
	{
		g_pMaterialSystem->EnableEditorMaterials();	
	}

#endif // !_PS3

	StartupInfo_t info;
	info.m_pInstance = GetAppInstance();
	info.m_pBaseDirectory = GetBaseDirectory();
	info.m_pInitialMod = DetermineDefaultMod();
	info.m_pInitialGame = DetermineDefaultGame();
	info.m_pParentAppSystemGroup = this;
	info.m_bTextMode = g_bTextMode;

	return g_pEngineAPI->SetStartupInfo( info );
}

int CSourceAppSystemGroup::Main()
{
	return g_pEngineAPI->Run();
}

void CSourceAppSystemGroup::PostShutdown()
{
#ifndef _PS3
	// FIXME: Logfiles is mod-specific, needs to move into the engine.
	g_LogFiles.Shutdown();
#endif // _PS3

	reslistgenerator->Shutdown();

	DisconnectTier3Libraries();
	DisconnectTier2Libraries();
	ConVar_Unregister( );
	DisconnectTier1Libraries();
}

void CSourceAppSystemGroup::Destroy() 
{
	g_pEngineAPI = NULL;
	g_pMaterialSystem = NULL;
	g_pHammer = NULL;
	g_pVJobs = NULL;

#ifdef WIN32
	CoUninitialize();
#endif
}


//-----------------------------------------------------------------------------
// Determines the initial mod to use at load time.
// We eventually (hopefully) will be able to switch mods at runtime
// because the engine/hammer integration really wants this feature.
//-----------------------------------------------------------------------------
const char *CSourceAppSystemGroup::DetermineDefaultMod()
{
	if ( !m_bEditMode )
	{   		 
		return CommandLine()->ParmValue( "-game", DEFAULT_HL2_GAMEDIR );
	}
	return g_pHammer->GetDefaultMod();
}

const char *CSourceAppSystemGroup::DetermineDefaultGame()
{
	if ( !m_bEditMode )
	{
		return CommandLine()->ParmValue( "-defaultgamedir", DEFAULT_HL2_GAMEDIR );
	}
	return g_pHammer->GetDefaultGame();
}

//-----------------------------------------------------------------------------
// MessageBox for OSX
//-----------------------------------------------------------------------------
#if defined(OSX)
#include "CoreFoundation/CoreFoundation.h"

int MessageBox( HWND hWnd, const char *message, const char *header, unsigned uType )
{
    //convert the strings from char* to CFStringRef
    CFStringRef header_ref      = CFStringCreateWithCString( NULL, header,     strlen(header)    );
    CFStringRef message_ref  = CFStringCreateWithCString( NULL, message,  strlen(message) );

    CFOptionFlags result;  //result code from the message box
  
    //launch the message box
    CFUserNotificationDisplayAlert( 0, // no timeout
                                    kCFUserNotificationNoteAlertLevel, //change it depending message_type flags ( MB_ICONASTERISK.... etc.)
									NULL, //icon url, use default, you can change it depending message_type flags
									NULL, //not used
									NULL, //localization of strings
									header_ref, //header text 
									message_ref, //message text
									NULL, //default "ok" text in button
									NULL,
									NULL, //other button title, null--> no other button
									&result //response flags
									);

    //Clean up the strings
    CFRelease( header_ref );
    CFRelease( message_ref );

    //Convert the result
    if( result == kCFUserNotificationDefaultResponse )
        return 0;
    else
        return 1;

}

#elif defined( LINUX )

int MessageBox( HWND hWnd, const char *message, const char *header, unsigned uType )
{
	SDL_ShowSimpleMessageBox( 0, header, message, GetAssertDialogParent() );
	return 0;
}

#endif

//-----------------------------------------------------------------------------
// Allow only one windowed source app to run at a time
//-----------------------------------------------------------------------------
#ifdef WIN32
HANDLE g_hMutex = NULL;
#elif defined( POSIX )
int g_lockfd = -1;
char g_lockFilename[MAX_PATH];
#endif

bool GrabSourceMutex()
{
#ifdef WIN32
	if ( IsPC() )
	{
		// don't allow more than one instance to run
		g_hMutex = ::CreateMutex(NULL, FALSE, TEXT("hl2_singleton_mutex"));

		unsigned int waitResult = ::WaitForSingleObject(g_hMutex, 0);

		// Here, we have the mutex
		if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_ABANDONED)
			return true;

		// couldn't get the mutex, we must be running another instance
		::CloseHandle(g_hMutex);

		// If there is a VPROJECT defined, we assume you are a developer and know the risks
		// of running multiple copies of the engine
		if ( getenv( "VPROJECT" ) && CommandLine()->FindParm( "-allowmultiple" ) )
		{
			return true;
		}

		return false;
	}
#elif defined( POSIX )
	// Under OSX use flock in /tmp/source_engine_<game>.lock, create the file if it doesn't exist
	const char *pchGameParam = CommandLine()->ParmValue( "-game", DEFAULT_HL2_GAMEDIR );
	CRC32_t gameCRC;
	CRC32_Init(&gameCRC);
	CRC32_ProcessBuffer( &gameCRC, (void *)pchGameParam, Q_strlen( pchGameParam ) );
	CRC32_Final( &gameCRC );
	
#ifdef LINUX
	/*
	 * Linux
	 */

	// Check TMPDIR environment variable for temp directory.
	char *tmpdir = getenv( "TMPDIR" );

	// If it's NULL, or it doesn't exist, or it isn't a directory, fallback to /tmp.
	struct stat buf;
	if( !tmpdir || stat( tmpdir, &buf ) || !S_ISDIR ( buf.st_mode ) )
		tmpdir = "/tmp";

	V_snprintf( g_lockFilename, sizeof(g_lockFilename), "%s/source_engine_%lu.lock", tmpdir, gameCRC );

	g_lockfd = open( g_lockFilename, O_WRONLY | O_CREAT, 0666 );
	if ( g_lockfd == -1 )
	{
		printf( "open(%s) failed\n", g_lockFilename );
		return false;
	}

	// In case we have a umask setting creation to something other than 0666,
	// force it to 0666 so we don't lock other users out of the game if
	// the game dies etc.
	fchmod(g_lockfd, 0666);

	struct flock fl;
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;

	if ( fcntl ( g_lockfd, F_SETLK, &fl ) == -1 )
	{
		printf( "fcntl(%d) for %s failed\n", g_lockfd, g_lockFilename );
		return false;
	}

	return true;
#else
	/*
	 * OSX
	 */

	V_snprintf( g_lockFilename, sizeof(g_lockFilename), "/tmp/source_engine_%lu.lock", gameCRC );
	g_lockfd = open( g_lockFilename, O_CREAT | O_WRONLY | O_EXLOCK | O_NONBLOCK | O_TRUNC, 0777 );
	if (g_lockfd >= 0)
	{
		// make sure we give full perms to the file, we only one instance per machine
		fchmod( g_lockfd, 0777 );

		// we leave the file open, under unix rules when we die we'll automatically close and remove the locks
		return true;
	}

	// We were unable to open the file, it should be because we are unable to retain a lock
	if ( errno != EWOULDBLOCK)
	{
		fprintf( stderr, "unexpected error %d trying to exclusively lock %s\n", errno, g_lockFilename );

		// Let them launch because we don't know what's going on and wouldn't want
		// to stop them launching.
		return true;
	}

	return false;
#endif	// LINUX

#endif	// POSIX
	return true;
}

void ReleaseSourceMutex()
{
#ifdef WIN32
	if ( IsPC() && g_hMutex )
	{
		::ReleaseMutex( g_hMutex );
		::CloseHandle( g_hMutex );
		g_hMutex = NULL;
	}
#elif defined( POSIX )
	if ( g_lockfd != -1 )
	{
		close( g_lockfd );
		g_lockfd = -1;
		unlink( g_lockFilename );
	}
#endif
}

// Remove all but the last -game parameter.
// This is for mods based off something other than Half-Life 2 (like HL2MP mods).
// The Steam UI does 'steam -applaunch 320 -game c:\steam\steamapps\sourcemods\modname', but applaunch inserts
// its own -game parameter, which would supercede the one we really want if we didn't intercede here.
void RemoveSpuriousGameParameters()
{
	// Find the last -game parameter.
	int nGameArgs = 0;
	char lastGameArg[MAX_PATH];
	for ( int i=0; i < CommandLine()->ParmCount()-1; i++ )
	{
		if ( Q_stricmp( CommandLine()->GetParm( i ), "-game" ) == 0 )
		{
			Q_snprintf( lastGameArg, sizeof( lastGameArg ), "\"%s\"", CommandLine()->GetParm( i+1 ) );
			++nGameArgs;
			++i;
		}
	}

	// We only care if > 1 was specified.
	if ( nGameArgs > 1 )
	{
		CommandLine()->RemoveParm( "-game" );
		CommandLine()->AppendParm( "-game", lastGameArg );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *param - 
// Output : static char const
//-----------------------------------------------------------------------------
static char const *Cmd_TranslateFileAssociation(char const *param )
{
	static char sz[ 512 ];
	char *retval = NULL;

	char temp[ 512 ];
	Q_strncpy( temp, param, sizeof( temp ) );
	Q_FixSlashes( temp );
#ifdef WIN32
	Q_strlower( temp );
#endif
	const char *extension = V_GetFileExtension(temp);
	// must have an extension to map
	if (!extension)
		return retval;
	extension--; // back up so we have the . in the extension

	int c = ARRAYSIZE( g_FileAssociations );
	for ( int i = 0; i < c; i++ )
	{
		FileAssociationInfo& info = g_FileAssociations[ i ];

		if ( ! Q_strcmp( extension, info.extension ) && 
			! CommandLine()->FindParm(CFmtStr( "+%s", info.command_to_issue ) ) )
		{
			// Translate if haven't already got one of these commands			
			Q_strncpy( sz, temp, sizeof( sz ) );
			Q_FileBase( sz, temp, sizeof( sz ) );

			Q_snprintf( sz, sizeof( sz ), "%s %s", info.command_to_issue, temp );
			retval = sz;
			break;
		}		
	}

	// return null if no translation, otherwise return commands
	return retval;
}

//-----------------------------------------------------------------------------
// Purpose: Converts all the convar args into a convar command
// Input  : none 
// Output : const char * series of convars
//-----------------------------------------------------------------------------
static const char *BuildCommand()
{
	static CUtlBuffer build( 0, 0, CUtlBuffer::TEXT_BUFFER );
	build.Clear();

	// arg[0] is the executable name
	for ( int i=1; i < CommandLine()->ParmCount(); i++ )
	{
		const char *szParm = CommandLine()->GetParm(i);
		if (!szParm) continue;

		if (szParm[0] == '-') 
		{
			// skip -XXX options and eat their args
			const char *szValue = CommandLine()->ParmValue(szParm);
			if ( szValue ) i++;
			continue;
		}
		if (szParm[0] == '+')
		{
			// convert +XXX options and stuff them into the build buffer
			const char *szValue = CommandLine()->ParmValue(szParm);
			if (szValue)
			{
				build.PutString(CFmtStr("%s %s;", szParm+1, szValue));
				i++;
			}
			else
			{
				build.PutString(szParm+1);
				build.PutChar(';');
			}
		}
		else 
		{
			// singleton values, convert to command
			char const *translated = Cmd_TranslateFileAssociation( CommandLine()->GetParm( i ) );
			if (translated)
			{
				build.PutString(translated);
				build.PutChar(';');
			}
		}
	}

	build.PutChar( '\0' );

	return (const char *)build.Base();
}

DLL_IMPORT CLinkedMiniProfiler *g_pPhysicsMiniProfilers;
DLL_IMPORT CLinkedMiniProfiler *g_pOtherMiniProfilers;

CLauncherLoggingListener g_LauncherLoggingListener;


// #define LOADING_MEMORY_WATCHDOG 100

// This block enables a thread that dumps memory stats at regular 
// intervals from the moment the thread pool is initialized until the
// game halts. LOADING_MEMORY_WATCHDOG, if defined, specifies the 
// interval in milliseconds
#if LOADING_MEMORY_WATCHDOG 
namespace
{
	/// a thread that's meant to run intermittently at regular intervals
	class CThreadWatchdog : public CThread
	{
	public:
		CThreadWatchdog( unsigned nIntervalInMilliseconds, const char *pszName, int nAffinity );

		// You will need to call Start() and Stop() on this class externally.



		// for debugging purposes -- record how long it took the payload to execute. 
		// (circular buffer of four samples)
		CCycleCount m_nPayloadTimers[4];
		unsigned int m_nPayloadTimersNextIdx; //< next timer to write into.

	protected:
		virtual bool Init();

		// the "run" function for this thread, gets called once every nIntervalInMilliseconds.
		// return true to keep running, false to stop.
		virtual bool Payload() = 0;

		unsigned m_nIntervalMsec; /// the watchdog will run once every this many msec
		int m_nAffinityMask;

	private:
		// from CThread. !!DO NOT!! override this in inheritors -- your work should be done in Payload().
		virtual int Run();
	};

	CThreadWatchdog::CThreadWatchdog( unsigned nIntervalInMilliseconds, const char *pszName, int nAffinity ) : 
	m_nIntervalMsec(nIntervalInMilliseconds),
		m_nAffinityMask(nAffinity),
		m_nPayloadTimersNextIdx(0)
	{
		SetName( pszName ); // the thread name must be set before calling Start(), or subsequent thread management calls will crash. I don't know why. That's just what happens.
	}

	bool CThreadWatchdog::Init()
	{
		if (!CThread::Init()) 
			return false;

		ThreadSetAffinity( GetThreadHandle(), m_nAffinityMask );
		return true;

	}

	int CThreadWatchdog::Run()
	{
		bool bContinue;
		do 
		{
			CFastTimer payloadtime;
			payloadtime.Start();

			bContinue = Payload();

			payloadtime.End();
			m_nPayloadTimers[ m_nPayloadTimersNextIdx++ & 3 ] = payloadtime.GetDuration();

			Sleep( m_nIntervalMsec );
		} while ( bContinue );
		return 0;
	}

	class CLoadMemoryWatchdog : public CThreadWatchdog
	{
	public:
		CLoadMemoryWatchdog( unsigned nIntervalInMilliseconds, const char *pszFilename ); // filename is assumed to be into a static COMDAT (don't make it in a temp buffer)

	private:
		bool Payload();

		const char *m_pszFilename;
	};

	CLoadMemoryWatchdog::CLoadMemoryWatchdog( unsigned nIntervalInMilliseconds, const char *pszFilename ) : 
	CThreadWatchdog( nIntervalInMilliseconds, "LoadMemWatchdog", IsX360() ? XBOX_CORE_0_HWTHREAD_1 : 0 ),
		m_pszFilename(pszFilename)
	{	
	}

	bool CLoadMemoryWatchdog::Payload()
	{
		g_pMemAlloc->DumpStatsFileBase( m_pszFilename );
		return true;
	}

	CLoadMemoryWatchdog g_MemWatchdog( LOADING_MEMORY_WATCHDOG, "loadingmemory" );
}
#endif

// this class automatically handles the initialization and uninitialization
// of the vpbdm library. It's RAII because we absolutely positively have
// to guarantee that the library cleans up after itself on shutdown, 
// even if we early out of LauncherMain.
#ifdef _PS3
class ValvePS3ConsoleInitializerRAII
{
public:
	ValvePS3ConsoleInitializerRAII( bool bDvdDev, bool bSpewDllInfo, bool bWaitForConsole ) 
	{
#pragma message("TODO: bdvddev / spewdllinfo / wait for console")
		ValvePS3ConsoleInit( );	
		if ( g_pValvePS3Console )
		{
		g_pValvePS3Console->InitConsoleMonitor( bWaitForConsole );
	}
	}

	~ValvePS3ConsoleInitializerRAII()
	{
		ValvePS3ConsoleShutdown();
	}
};
#endif

//-----------------------------------------------------------------------------
// Purpose: The real entry point for the application
// Input  : hInstance - 
//			hPrevInstance - 
//			lpCmdLine - 
//			nCmdShow - 
// Output : int APIENTRY
//-----------------------------------------------------------------------------
#ifdef WIN32
extern "C" __declspec(dllexport) int LauncherMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
#elif defined( _PS3 )
int LauncherMain( int argc, char **argv )
#else
extern "C" DLL_EXPORT int LauncherMain( int argc, char **argv )
#endif
{
#ifdef WIN32
	SetAppInstance( hInstance );
#endif

#ifdef LINUX
	//  fix to stop us from crashing in printf/sscanf functions that don't expect
	//  localization to mess with your "." and "," float seperators. Mac OSX also sets LANG
	//  to en_US.UTF-8 before starting up (in info.plist I believe).
	// We need to double check that localization for libcef is handled correctly
	//  when we slam things to en_US.UTF-8.
	// Also check if C.UTF-8 exists and use it? This file: /usr/lib/locale/C.UTF-8.
	// It looks like it's only installed on Debian distros right now though.
        const char en_US[] = "en_US.UTF-8";

        setenv( "LC_ALL", en_US, 1 );
        setlocale( LC_ALL, en_US );

        const char *CurrentLocale = setlocale( LC_ALL, NULL );
        if ( Q_stricmp( CurrentLocale, en_US ) )
        {
                Warning( "WARNING: setlocale('%s') failed, using locale:'%s'. International characters may not work.\n", en_US, CurrentLocale );
        }
#endif // LINUX

	// Hook the debug output stuff.
	LoggingSystem_RegisterLoggingListener( &g_LauncherLoggingListener );

#ifndef _PS3
	// Quickly check the hardware key, essentially a warning shot.  
	if ( !Plat_VerifyHardwareKeyPrompt() )
	{
		return -1;
	}
#endif // !_PS3

#ifdef WIN32
	CommandLine()->CreateCmdLine( IsPC() ? GetCommandLine() : lpCmdLine );
#else
	CommandLine()->CreateCmdLine( argc, argv );
#endif

#if defined (PLATFORM_OSX) || defined (WIN32)
	// No -dxlevel or +mat_hdr_level allowed in CSGO
	CommandLine()->RemoveParm( "-dxlevel" );
	CommandLine()->RemoveParm( "+mat_hdr_level" );
	CommandLine()->RemoveParm( "+mat_dxlevel" );
#endif

#ifndef _PS3
	// Figure out the directory the executable is running from
	UTIL_ComputeBaseDir();
#endif // _PS3

#if defined (CSTRIKE15)

	// GS - If we didn't specify a game name then default to CSGO
	// This is required for running from a HDD Boot Game package
	if ( CommandLine()->CheckParm( "-game") == NULL )
	{
		CommandLine()->AppendParm( "-game", "csgo" );
	}

#if defined _PS3

	if ( g_pPS3PathInfo->BootType() == CELL_GAME_GAMETYPE_HDD )
	{
		CommandLine()->AppendParm( "+sv_search_key", "testlab" );
		CommandLine()->AppendParm( "-steamBeta", "");
	}

#endif

#endif

	bool bDvdDev, bSpewDllInfo, bWaitForConsole;
	bDvdDev         = CommandLine()->CheckParm( "-dvddev"    ) != NULL;
	bSpewDllInfo    = CommandLine()->CheckParm( "-dllinfo"   ) != NULL;
	bWaitForConsole = CommandLine()->CheckParm( "-vxconsole" ) != NULL;

#if defined( _X360 )
	XboxConsoleInit();
	// sync block until vxconsole responds
	XBX_InitConsoleMonitor( bWaitForConsole || bSpewDllInfo || bDvdDev );
	if ( bDvdDev )
	{
		// just launched, signal vxconsole to sync the dvddev cache before any files get accessed
		XBX_rSyncDvdDevCache();
	}
#elif defined( _PS3 )
	ValvePS3ConsoleInitializerRAII VXBDM( bDvdDev, bSpewDllInfo, bWaitForConsole );	
#endif

#if LOADING_MEMORY_WATCHDOG 
	g_MemWatchdog.Start();
#endif

#if defined( _X360 )
	if ( bWaitForConsole )
	{
		COM_TimestampedLog( "LauncherMain: Application Start - %s", CommandLine()->GetCmdLine() );
	}
	if ( bSpewDllInfo )
	{	
		XBX_DumpDllInfo( GetBaseDirectory() );
		Error( "Stopped!\n" );
	}

	int storageIDs[4];
	XboxLaunch()->GetStorageID( storageIDs );
	for ( int k = 0; k < 4; ++ k )
	{
		DWORD storageID = storageIDs[k];
		if ( XBX_DescribeStorageDevice( storageID ) )
		{
			// Validate the storage device
			XDEVICE_DATA deviceData;
			DWORD ret = XContentGetDeviceData( storageID, &deviceData );
			if ( ret != ERROR_SUCCESS )
			{
				// Device was removed
				storageID = XBX_INVALID_STORAGE_ID;
				XBX_QueueEvent( XEV_LISTENER_NOTIFICATION, WM_SYS_STORAGEDEVICESCHANGED, 0, 0 );
			}
		}
		XBX_SetStorageDeviceId( k, storageID );
	}

	int userID = XboxLaunch()->GetUserID();
	userID = XBX_INVALID_USER_ID;			// TODO: currently game cannot recover from a restart with users signed-in
	if ( userID == XBX_INVALID_USER_ID )
	{
		// didn't come from restart, start up with guest settings
		XUSER_SIGNIN_INFO info;
		for ( int i = 0; i < 4; ++i )
		{
			if ( ERROR_NO_SUCH_USER != XUserGetSigninInfo( i, 0, &info ) )
			{
				userID = i;
				break;
			}
		}

		XBX_SetNumGameUsers( 0 );
		XBX_SetPrimaryUserId( XBX_INVALID_USER_ID );
		XBX_ResetUserIdSlots();
		XBX_SetPrimaryUserIsGuest( 1 );
	}
	else
	{
		int numGameUsers;
		char slot2ctrlr[4];
		char slot2guest[4];
		XboxLaunch()->GetSlotUsers( numGameUsers, slot2ctrlr, slot2guest );

		XBX_SetNumGameUsers( numGameUsers );
		XBX_SetPrimaryUserId( userID );
		if ( numGameUsers == 1 && slot2guest[0] == 1 )
			XBX_SetPrimaryUserIsGuest( 1 );
		else
			XBX_SetPrimaryUserIsGuest( 0 );

		for ( int k = 0; k < 4; ++ k )
		{
			XBX_SetUserId( k, slot2ctrlr[k] );
			XBX_SetUserIsGuest( k, slot2guest[k] );
		}
	}

#ifdef PLATFORM_OSX
	{
		struct stat st;
		if ( stat( RELAUNCH_FILE, &st ) == 0 ) 
		{
			unlink( RELAUNCH_FILE );
		}
	}
#endif

	DevMsg( "[X360 LAUNCH] Started with the following payload:\n" );
	DevMsg( "              Num Game Users   = %d\n", XBX_GetNumGameUsers() );
	for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
	{
		DevMsg( "              Storage Device %d = 0x%08X\n", XBX_GetStorageDeviceId( k ) );
	}
#endif

	// check for a named executable
	const char *exeFilename = GetExecutableFilename();
	if ( !IsGameConsole() && exeFilename[0] && Q_strcmp( exeFilename, "hl2" ) && !CommandLine()->FindParm( "-game" ) )
	{
		CommandLine()->RemoveParm( "-game" );
		CommandLine()->AppendParm( "-game", exeFilename );
	}

	// Uncomment the following code to allow multiplayer on the Xbox 360 for trade shows.
#if 0
#if defined( CSTRIKE15 ) && defined( _X360 ) && !defined( _CERT )
	if ( CommandLine()->FindParm( "-xnet_bypass_security" ) == 0 )
	{
		CommandLine()->AppendParm( "-xnet_bypass_security", "" );
		Warning( "adding -xnet_bypass_security to command line. Remove this for shipping!\n" );
	}

	if ( CommandLine()->FindParm( "-demo_pressbuild_play_addr" ) == 0 )
	{
		CommandLine()->AppendParm( "-demo_pressbuild_play_addr", "192.168.1.100:27015" );
		Warning( "adding -demo_pressbuild_play_addr to command line. Remove this for shipping!\n" );
	}
#endif
#endif

#ifdef SIXENSE
	// If the game arg is currently portal2
	char const *game_param_val = NULL;
	CommandLine()->CheckParm( "-game", &game_param_val );

	if( game_param_val && !Q_strcmp( game_param_val, "portal2" ) )
	{
		// and if there is a portal2_sixense dir that contains a valid gameinfo.txt, then override the game parameter to use that instead
		if( !CommandLine()->CheckParm( "-nosixense" ) && DoesFileExistIn( ".", "portal2_sixense/gameinfo.txt" ) ) 
		{
			CommandLine()->RemoveParm( "-game" );
			CommandLine()->AppendParm( "-game", "portal2_sixense" );
		}
	}
#endif

#ifdef _PS3
	if ( CommandLine()->CheckParm( "-dvddev" ) &&
		!CommandLine()->CheckParm( "-basedir" ) )
	{
		CommandLine()->AppendParm( "-basedir", g_pPS3PathInfo->GameImagePath() );
	}
#endif
	
#ifndef _CERT
	if ( CommandLine()->CheckParm( "-tslist" ) )
	{
		//TestThreads(1);
		int nTests = 10000;
		DevMsg("Running TSList tests\n");
		RunTSListTests( nTests );
		DevMsg("Running TSQueue tests\n");
		RunTSQueueTests( nTests );
		DevMsg("Running Thread Pool tests\n");
		RunThreadPoolTests();
	}
#endif

	// This call is to emulate steam's injection of the GameOverlay DLL into our process if we
	// are running from the command line directly, this allows the same experience the user gets
	// to be present when running from perforce, the call has no effect on X360
	TryToLoadSteamOverlayDLL();

	// See the function for why we do this.
	RemoveSpuriousGameParameters();

#ifdef WIN32
	if ( IsPC() )
	{
		// initialize winsock
		WSAData wsaData;
		int	nError = ::WSAStartup( MAKEWORD(2,0), &wsaData );
		if ( nError )
		{
			Msg( "Warning! Failed to start Winsock via WSAStartup = 0x%x.\n", nError);
		}
	}
#endif

 	// Run in text mode? (No graphics or sound).
 	if ( CommandLine()->CheckParm( "-textmode" ) )
 	{
#if defined( DEVELOPMENT_ONLY ) || defined( ALLOW_TEXT_MODE )
 		g_bTextMode = true;
 		InitTextMode();
#endif
 	}
#ifdef WIN32

#if defined( DEBUG ) && defined( ALLOW_MULTI_CLIENTS_PER_MACHINE )
	else if ( true )
	{
		Warning("Skipping multiple clients from one machine check! Don't ship this way!\n");
	}
#endif

	else if ( !IsX360() )
	{
		int retval = -1;
		// Can only run one windowed source app at a time
		if ( !GrabSourceMutex() )
		{
			// We're going to hijack the existing session and load a new savegame into it. This will mainly occur when users click on links in Bugzilla that will automatically copy saves and load them
			// directly from the web browser. The -hijack command prevents the launcher from objecting that there is already an instance of the game.
			if (CommandLine()->CheckParm( "-hijack" ))
			{
				HWND hwndEngine = FindWindow( "Valve001", NULL );

				// Can't find the engine
				if ( hwndEngine == NULL )
				{
					::MessageBox( NULL, "The modified entity keyvalues could not be sent to the Source Engine because the engine does not appear to be running.", "Source Engine Not Running", MB_OK | MB_ICONEXCLAMATION );
				}
				else
				{			
					const char *szCommand = BuildCommand();

					//
					// Fill out the data structure to send to the engine.
					//
					COPYDATASTRUCT copyData;
					copyData.cbData = strlen( szCommand ) + 1;
					copyData.dwData = 0;
					copyData.lpData = ( void * )szCommand;

					if ( !::SendMessage( hwndEngine, WM_COPYDATA, 0, (LPARAM)&copyData ) )
					{
						::MessageBox( NULL, "The Source Engine was found running, but did not accept the request to load a savegame. It may be an old version of the engine that does not support this functionality.", "Source Engine Declined Request", MB_OK | MB_ICONEXCLAMATION );
					}
					else
					{
						retval = 0;
					}

					free((void *)szCommand);
				}
			}
			else
			{
				::MessageBox(NULL, "Only one instance of the game can be running at one time.", "Source - Warning", MB_ICONINFORMATION | MB_OK);
			}

			return retval;
		}
	}
#elif defined( POSIX )
	else
	{
		if ( !GrabSourceMutex() )
		{
			::MessageBox(NULL, "Only one instance of the game can be running at one time.", "Source - Warning", 0 );
			return -1;
		}
	}
#endif

	if ( !IsX360() )
	{
#ifdef WIN32
		// Make low priority?
		if ( CommandLine()->CheckParm( "-low" ) )
		{
			SetPriorityClass( GetCurrentProcess(), IDLE_PRIORITY_CLASS );
		}
		else if ( CommandLine()->CheckParm( "-high" ) )
		{
			SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS );
		}
#endif

		// If game is not run from Steam then add -insecure in order to avoid client timeout message
		if ( NULL == CommandLine()->CheckParm( "-steam" ) )
		{
			CommandLine()->AppendParm( "-insecure", NULL );
		}
	}

#ifndef _PS3
	// Figure out the directory the executable is running from
	// and make that be the current working directory
	// on the PS3, however, there is no concept of current directories
	_chdir( GetBaseDirectory() );
#endif

	// When building cubemaps, we don't need sound and can't afford to have async I/O - cubemap writes to the BSP can collide with async bsp reads
	if ( CommandLine()->CheckParm( "-buildcubemaps") )
	{
		CommandLine()->AppendParm( "-nosound", NULL );
		CommandLine()->AppendParm( "-noasync", NULL );
	}

	g_LeakDump.m_bCheckLeaks = CommandLine()->CheckParm( "-leakcheck" ) ? true : false;

	bool bRestart = true;
	while ( bRestart )
	{
		bRestart = false;

		CSourceAppSystemGroup sourceSystems;
		CSteamApplication steamApplication( &sourceSystems );
#if defined( OSX ) && !defined( USE_SDL )
		extern int ValveCocoaMain( CAppSystemGroup *pApp );
		int nRetval = ValveCocoaMain( &steamApplication ); 
#else
		int nRetval = steamApplication.Run();
#endif		
#if ENABLE_HARDWARE_PROFILER 
		// Hack fix, causes memory leak, but prevents crash due to bad coding not doing proper teardown
		// need to ensure these list anchors don't anchor stale pointers
		g_pPhysicsMiniProfilers = NULL;
		g_pOtherMiniProfilers = NULL;
#endif

		if ( steamApplication.GetCurrentStage() == CSourceAppSystemGroup::INITIALIZATION )
		{
			bRestart = (nRetval == INIT_RESTART);
		}
		else if ( nRetval == RUN_RESTART )
		{
			bRestart = true;
		}

		bool bReslistCycle = false;
		if ( !bRestart )
		{
			bReslistCycle = reslistgenerator->ShouldContinue();
			bRestart = bReslistCycle;
		}
		
		if ( !bReslistCycle )
		{
			// Remove any overrides in case settings changed
			CommandLine()->RemoveParm( "-w" );
			CommandLine()->RemoveParm( "-h" );
			CommandLine()->RemoveParm( "-width" );
			CommandLine()->RemoveParm( "-height" );
			CommandLine()->RemoveParm( "-sw" );
			CommandLine()->RemoveParm( "-startwindowed" );
			CommandLine()->RemoveParm( "-windowed" );
			CommandLine()->RemoveParm( "-window" );
			CommandLine()->RemoveParm( "-full" );
			CommandLine()->RemoveParm( "-fullscreen" );
			CommandLine()->RemoveParm( "-autoconfig" );
			CommandLine()->RemoveParm( "+mat_hdr_level" );
		}
	}

#ifdef WIN32
	if ( IsPC() )
	{
		// shutdown winsock
		int nError = ::WSACleanup();
		if ( nError )
		{
			Msg( "Warning! Failed to complete WSACleanup = 0x%x.\n", nError );
		}
	}
#endif

	// Allow other source apps to run
	ReleaseSourceMutex();

#if	defined( WIN32 )  && !defined( _X360 )
	// Now that the mutex has been released, check HKEY_CURRENT_USER\Software\Valve\Source\Relaunch URL. If there is a URL here, exec it.
	// This supports the capability of immediately re-launching the the game via Steam in a different audio language 
	HKEY hKey; 
	if ( RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Valve\\Source", NULL, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS )
	{
		char szValue[MAX_PATH];
		DWORD dwValueLen = MAX_PATH;

		if ( RegQueryValueEx( hKey, "Relaunch URL", NULL, NULL, (unsigned char*)szValue, &dwValueLen ) == ERROR_SUCCESS )
		{
			ShellExecute (0, "open", szValue, 0, 0, SW_SHOW);
			RegDeleteValue( hKey, "Relaunch URL" );
		}

		RegCloseKey(hKey);
	}
#elif defined( OSX )
	struct stat st;
	if ( stat( RELAUNCH_FILE, &st ) == 0 ) 
	{
		FILE *fp = fopen( RELAUNCH_FILE, "r" );
		if ( fp )
		{
			char szCmd[256];
			int nChars = fread( szCmd, 1, sizeof(szCmd), fp );
			if ( nChars > 0 )
			{
				char szOpenLine[ MAX_PATH ];
				Q_snprintf( szOpenLine, sizeof(szOpenLine), "open \"%s\"", szCmd );
				system( szOpenLine );
			}
			fclose( fp );
			unlink( RELAUNCH_FILE );
		}
	}
#endif

	return 0;
}
