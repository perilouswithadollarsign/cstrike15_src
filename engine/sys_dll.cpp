//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//


#if defined(_WIN32) && !defined(_X360)
#include "winlite.h"
#endif
#ifdef OSX
#include <Carbon/Carbon.h>
#include <sys/sysctl.h>
#endif
#if defined(LINUX)
#include <unistd.h>
#include <fcntl.h>
#include "SDL.h"
#endif

#include "quakedef.h"
#include "igame.h"
#include "errno.h"
#include "host.h"
#include "profiling.h"
#include "server.h"
#include "vengineserver_impl.h"
#include "filesystem_engine.h"
#include "sys.h"
#include "sys_dll.h"
#include "ivideomode.h"
#include "host_cmd.h"
#include "crtmemdebug.h"
#include "sv_log.h"
#include "sv_main.h"
#include "traceinit.h"
#include "dt_test.h"
#include "keys.h"
#include "gl_matsysiface.h"
#include "tier0/icommandline.h"
#include "tier0/stacktools.h"
#include "cmd.h"
#include <ihltvdirector.h>
#include <ireplaydirector.h>
#include "MapReslistGenerator.h"
#include "DevShotGenerator.h"
#include "cdll_engine_int.h"
#include "dt_send.h"
#include "idedicatedexports.h"
#include "cvar.h"
#include "cl_steamauth.h"
#include "status.h"
#include "tier0/logging.h"
#include "tier2/tier2_logging.h"

#include "vgui_baseui_interface.h"
#include "tier0/systeminformation.h"
#ifdef _WIN32
#if !defined( _X360 )
#include <io.h>
#endif
#endif
#include "toolframework/itoolframework.h"
#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#elif defined( _PS3 )
#include "ps3/ps3_console.h"
#include "sys/tty.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ONE_HUNDRED_TWENTY_EIGHT_MB	(128 * 1024 * 1024)

ConVar mem_min_heapsize( "mem_min_heapsize", "48", 0, "Minimum amount of memory to dedicate to engine hunk and datacache (in mb)" );
ConVar mem_max_heapsize( "mem_max_heapsize", "512", 0, "Maximum amount of memory to dedicate to engine hunk and datacache (in mb)" );
ConVar mem_max_heapsize_dedicated( "mem_max_heapsize_dedicated", "64", 0, "Maximum amount of memory to dedicate to engine hunk and datacache, for dedicated server (in mb)" );

#define MINIMUM_WIN_MEMORY			(unsigned)(mem_min_heapsize.GetInt()*1024*1024)
#define MAXIMUM_WIN_MEMORY			MAX( (unsigned)(mem_max_heapsize.GetInt()*1024*1024), MINIMUM_WIN_MEMORY )
#define MAXIMUM_DEDICATED_MEMORY	(unsigned)(mem_max_heapsize_dedicated.GetInt()*1024*1024)

DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_SERVER_LOG, "ServerLog", LCF_DO_NOT_ECHO );

char *CheckParm(const char *psz, char **ppszValue = NULL);
void SeedRandomNumberGenerator( bool random_invariant );
void Con_ColorPrintf( const Color& clr, const char *fmt, ... );

void COM_ShutdownFileSystem( void );
void COM_InitFilesystem( const char *pFullModPath );

modinfo_t			gmodinfo;

extern HWND			*pmainwindow;
char				gszDisconnectReason[256];
char				gszExtendedDisconnectReason[256];
bool				gfExtendedError = false;
uint8				g_eSteamLoginFailure = 0;
bool				g_bV3SteamInterface = false;
CreateInterfaceFn	g_AppSystemFactory = NULL;

static bool			s_bIsDedicated = false;
ConVar *sv_noclipduringpause = NULL;

// Special mode where the client uses a console window and has no graphics. Useful for stress-testing a server
// without having to round up 32 people.
bool g_bTextMode = false;

// Set to true when we exit from an error.
bool g_bInErrorExit = false;

static FileFindHandle_t	g_hfind = FILESYSTEM_INVALID_FIND_HANDLE;

// The extension DLL directory--one entry per loaded DLL
CSysModule *g_GameDLL = NULL;

// Prototype of an global method function
typedef void (DLLEXPORT * PFN_GlobalMethod)( edict_t *pEntity );

IServerGameDLL	*serverGameDLL = NULL;
bool g_bServerGameDLLGreaterThanV5;
IServerGameEnts *serverGameEnts = NULL;

IServerGameClients *serverGameClients = NULL;
int g_iServerGameClientsVersion = 0;	// This matches the number at the end of the interface name (so for "ServerGameClients004", this would be 4).

IHLTVDirector	*serverGameDirector = NULL;
IReplayDirector	*serverReplayDirector = NULL;

IServerGameTags *serverGameTags = NULL;

void Sys_InitArgv( char *lpCmdLine );
void Sys_ShutdownArgv( void );

extern bool s_bIsDedicatedServer;

//-----------------------------------------------------------------------------
// Purpose: Compare file times
// Input  : ft1 - 
//			ft2 - 
// Output : int
//-----------------------------------------------------------------------------
int Sys_CompareFileTime( long ft1, long ft2 )
{
	if ( ft1 < ft2 )
	{
		return -1;
	}
	else if ( ft1 > ft2 )
	{
		return 1;
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Is slash?
//-----------------------------------------------------------------------------
inline bool IsSlash( char c )
{
	return ( c == '\\') || ( c == '/' );
}


//-----------------------------------------------------------------------------
// Purpose: Create specified directory
// Input  : *path - 
// Output : void Sys_mkdir
//-----------------------------------------------------------------------------
void Sys_mkdir( const char *path, const char *pPathID /*= 0*/ )
{
	char testpath[ MAX_OSPATH ];

	// Remove any terminal backslash or /
	Q_strncpy( testpath, path, sizeof( testpath ) );
	int nLen = Q_strlen( testpath );
	if ( (nLen > 0) && IsSlash( testpath[ nLen - 1 ] ) )
	{
		testpath[ nLen - 1 ] = 0;
	}

	// Look for URL
	if ( !pPathID )
	{
		pPathID = "MOD";
	}

	if ( IsSlash( testpath[0] ) && IsSlash( testpath[1] ) )
	{
		pPathID = NULL;
	}

	Q_FixSlashes( testpath );

	if ( g_pFileSystem->FileExists( testpath, pPathID ) )
	{
		// if there is a file of the same name as the directory we want to make, just kill it
		if ( !g_pFileSystem->IsDirectory( testpath, pPathID ) )
		{
			g_pFileSystem->RemoveFile( testpath, pPathID );
		}
	}

	g_pFileSystem->CreateDirHierarchy( path, pPathID );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *path - 
//			*basename - 
// Output : char *Sys_FindFirst
//-----------------------------------------------------------------------------
const char *Sys_FindFirst(const char *path, char *basename, int namelength )
{
	if (g_hfind != FILESYSTEM_INVALID_FIND_HANDLE)
	{
		Sys_Error ("Sys_FindFirst without close");
		g_pFileSystem->FindClose(g_hfind);		
	}

	const char* psz = g_pFileSystem->FindFirst(path, &g_hfind);
	if (basename && psz)
	{
		Q_FileBase(psz, basename, namelength );
	}

	return psz;
}

//-----------------------------------------------------------------------------
// Purpose: Sys_FindFirst with a path ID filter.
//-----------------------------------------------------------------------------
const char *Sys_FindFirstEx( const char *pWildcard, const char *pPathID, char *basename, int namelength )
{
	if (g_hfind != FILESYSTEM_INVALID_FIND_HANDLE)
	{
		Sys_Error ("Sys_FindFirst without close");
		g_pFileSystem->FindClose(g_hfind);		
	}

	const char* psz = g_pFileSystem->FindFirstEx( pWildcard, pPathID, &g_hfind);
	if (basename && psz)
	{
		Q_FileBase(psz, basename, namelength );
	}

	return psz;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *basename - 
// Output : char *Sys_FindNext
//-----------------------------------------------------------------------------
const char* Sys_FindNext(char *basename, int namelength)
{
	const char *psz = g_pFileSystem->FindNext(g_hfind);
	if ( basename && psz )
	{
		Q_FileBase(psz, basename, namelength );
	}

	return psz;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void Sys_FindClose
//-----------------------------------------------------------------------------

void Sys_FindClose(void)
{
	if ( FILESYSTEM_INVALID_FIND_HANDLE != g_hfind )
	{
		g_pFileSystem->FindClose(g_hfind);
		g_hfind = FILESYSTEM_INVALID_FIND_HANDLE;
	}
}


//-----------------------------------------------------------------------------
// Purpose: OS Specific initializations
//-----------------------------------------------------------------------------
void Sys_Init( void )
{
	// Set default FPU control word to truncate (chop) mode for optimized _ftol()
	// This does not "stick", the mode is restored somewhere down the line.
//	Sys_TruncateFPU();	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Sys_Shutdown( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: Print to system console
// Input  : *fmt - 
//			... - 
// Output : void Sys_Printf
//-----------------------------------------------------------------------------
void Sys_Printf(char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr,fmt);
	Q_vsnprintf (text, sizeof( text ), fmt, argptr);
	va_end (argptr);
		
	if ( developer.GetInt() )
	{
#ifdef _WIN32
		wchar_t unicode[2048];
		::MultiByteToWideChar(CP_UTF8, 0, text, -1, unicode, sizeof( unicode ) / sizeof(wchar_t));
		unicode[(sizeof( unicode ) / sizeof(wchar_t)) - 1] = L'\0';
		OutputDebugStringW( unicode );
		Sleep( 0 );
#else
		fprintf( stderr, "%s", text );
#endif
	}

	if ( s_bIsDedicated )
	{
		printf( "%s", text );
	}
}


bool Sys_MessageBox(const char *title, const char *info, bool bShowOkAndCancel)
{
#ifdef _WIN32

	if (IDOK == ::MessageBox(NULL, title, info, MB_ICONEXCLAMATION | (bShowOkAndCancel ? MB_OKCANCEL : MB_OK)))
	{
		return true;
	}
	return false;

#elif defined( LINUX ) && !defined( DEDICATED )

	int buttonid = 0;
	SDL_MessageBoxData messageboxdata = { 0 };
	SDL_MessageBoxButtonData buttondata[] =
	{
		{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,	1,	"OK"		},
		{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,	0,	"Cancel"	},
	};

	messageboxdata.window = GetAssertDialogParent();
	messageboxdata.title = title;
	messageboxdata.message = info;
	messageboxdata.numbuttons = bShowOkAndCancel ? 2 : 1;
	messageboxdata.buttons = buttondata;

	SDL_ShowMessageBox( &messageboxdata, &buttonid );
	return ( buttonid == 1 );

#elif defined ( POSIX )

	Warning( "%s\n", info );
	return true;

#else
#error "implement me"
#endif
}

bool g_bUpdateMinidumpComment = true;

#if !defined(NO_STEAM) && !defined(DEDICATED) && !defined(LINUX)
void BuildMinidumpComment( char const *pchSysErrorText );
#endif

//-----------------------------------------------------------------------------
// Purpose: Exit engine with error
// Input  : *error - 
//			... - 
// Output : void Sys_Error
//-----------------------------------------------------------------------------
void Sys_Error_Internal( bool bMinidump, const char *error, va_list argsList )
{
	char		text[1024];
	static      bool bReentry = false; // Don't meltdown

	Q_vsnprintf( text, sizeof( text ), error, argsList );

	if ( bReentry )
	{
		fprintf( stderr, "%s\n", text );
		return;
	}

	bReentry = true;

	if ( s_bIsDedicated )
	{
		printf( "%s\n", text );
	}
	else
	{
		Sys_Printf( "%s\n", text );
	}

	g_bInErrorExit = true;
	
#if !defined( DEDICATED )
	if ( IsPC() && videomode )
		videomode->Shutdown();
#endif

	if ( IsPC() &&
		 !CommandLine()->FindParm( "-makereslists" ) &&
		 !CommandLine()->FindParm( "-nomessagebox" ) )
	{
#ifdef _WIN32
		::MessageBox( NULL, text, "Engine Error", MB_OK | MB_TOPMOST );
#elif defined ( LINUX )
		Sys_MessageBox( "Engine Error", text, false );
#endif
	}

	if ( IsPC() )
	{
		DebuggerBreakIfDebugging();
	}
	else
	{
		DebuggerBreak(); 
	}

#if !defined( _X360 )

#if !defined(NO_STEAM) && !defined(DEDICATED) && !defined(LINUX)
	Status_Update();
	BuildMinidumpComment( text );
	g_bUpdateMinidumpComment = false;
#endif

	if ( bMinidump && !Plat_IsInDebugSession() && !CommandLine()->FindParm( "-nominidumps") )
	{
#ifdef WIN32
		// MiniDumpWrite() has problems capturing the calling thread's context 
		// unless it is called with an exception context.  So fake an exception.
		__try
		{
			RaiseException
				(
				0,							// dwExceptionCode
				EXCEPTION_NONCONTINUABLE,	// dwExceptionFlags
				0,							// nNumberOfArguments,
				NULL						// const ULONG_PTR* lpArguments
				);

			// Never get here (non-continuable exception)
		}
		// Write the minidump from inside the filter (GetExceptionInformation() is only 
		// valid in the filter)
		__except ( SteamAPI_WriteMiniDump( 0, GetExceptionInformation(), build_number() ), EXCEPTION_EXECUTE_HANDLER )
		{
			
			// We always get here because the above filter evaluates to EXCEPTION_EXECUTE_HANDLER
		}
#elif defined( OSX )
	// Doing this doesn't quite work the way we want because there is no "crashing" thread
	// and we see "No thread was identified as the cause of the crash; No signature could be created because we do not know which thread crashed" on the back end
	//SteamAPI_WriteMiniDump( 0, NULL, build_number() );
	printf("\n ##### Sys_Error: %s", text );
	fflush(stdout );
	
	int *p = 0;
	*p = 0xdeadbeef;
#elif defined( LINUX )
	// Doing this doesn't quite work the way we want because there is no "crashing" thread
	// and we see "No thread was identified as the cause of the crash; No signature could be created because we do not know which thread crashed" on the back end
	//SteamAPI_WriteMiniDump( 0, NULL, build_number() );
	int *p = 0;
	*p = 0xdeadbeef;
#else
//!!BUG!! "need minidump impl on sys_error"
#endif
	}
#endif // _X360

	host_initialized = false;
	Plat_ExitProcess( 100 );
}


//-----------------------------------------------------------------------------
// Purpose: Exit engine with error
// Input  : *error - 
//			... - 
// Output : void Sys_Error
//-----------------------------------------------------------------------------
void Sys_Error(const char *error, ...)
{
	va_list		argptr;
	
	va_start( argptr, error );
	Sys_Error_Internal( true, error, argptr );
	va_end( argptr );
	
}


//-----------------------------------------------------------------------------
// Purpose: Exit engine with error
// Input  : *error - 
//			... - 
// Output : void Sys_Error
//-----------------------------------------------------------------------------
void Sys_Exit(const char *error, ...)
{
	va_list		argptr;
	
	va_start( argptr, error );
	Sys_Error_Internal( false, error, argptr );
	va_end( argptr );
	
}


bool IsInErrorExit()
{
	return g_bInErrorExit;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msec - 
// Output : void Sys_Sleep
//-----------------------------------------------------------------------------
void Sys_Sleep( int msec )
{
	ThreadSleep( msec );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hInst - 
//			ulInit - 
//			lpReserved - 
// Output : BOOL WINAPI   DllMain
//-----------------------------------------------------------------------------
#if defined(_WIN32) && !defined( _X360 )
BOOL WINAPI DllMain(HANDLE hInst, ULONG ulInit, LPVOID lpReserved)
{
	InitCRTMemDebug();
	if (ulInit == DLL_PROCESS_ATTACH)
	{
	} 
	else if (ulInit == DLL_PROCESS_DETACH)
	{
	}

	return TRUE;
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Allocate memory for engine hunk
// Input  : *parms - 
//-----------------------------------------------------------------------------
void Sys_InitMemory( void )
{
	// Allow overrides
	if ( CommandLine()->FindParm( "-minmemory" ) )
	{
		host_parms.memsize = MINIMUM_WIN_MEMORY;
		return;
	}

	host_parms.memsize = 0;

#ifdef _WIN32
#if (_MSC_VER > 1200)
	// MSVC 6.0 doesn't support GlobalMemoryStatusEx()
	if ( IsPC() )
	{
		OSVERSIONINFOEX osvi;
		ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

		if ( GetVersionEx ((OSVERSIONINFO *)&osvi) )
		{
			if ( osvi.dwPlatformId >= VER_PLATFORM_WIN32_NT && osvi.dwMajorVersion >= 5 )
			{
				MEMORYSTATUSEX	memStat;
				ZeroMemory(&memStat, sizeof(MEMORYSTATUSEX));
				memStat.dwLength = sizeof(MEMORYSTATUSEX);
				if ( GlobalMemoryStatusEx( &memStat ) )
				{
					if ( memStat.ullTotalPhys > 0xFFFFFFFFUL )
					{
						host_parms.memsize = 0xFFFFFFFFUL;
					}
					else
					{
						host_parms.memsize = memStat.ullTotalPhys;
					}
				}
			}
		}
	}
#endif // (_MSC_VER > 1200)

	if ( !IsGameConsole() )
	{
		if ( host_parms.memsize == 0 )
		{
			MEMORYSTATUS lpBuffer;
			// Get OS Memory status
			lpBuffer.dwLength = sizeof(MEMORYSTATUS);
			GlobalMemoryStatus( &lpBuffer );

			if ( lpBuffer.dwTotalPhys <= 0 )
			{
				host_parms.memsize = MAXIMUM_WIN_MEMORY;
			}
			else
			{
				host_parms.memsize = lpBuffer.dwTotalPhys;
			}	
		}
		if ( host_parms.memsize < ONE_HUNDRED_TWENTY_EIGHT_MB )
		{
			Sys_Error( "Available memory less than 128MB!!! %i\n", host_parms.memsize );
		}

		// take one quarter the physical memory
		if ( host_parms.memsize <= 512*1024*1024)
		{
			host_parms.memsize >>= 2;
			// Apply cap of 64MB for 512MB systems
			// this keeps the code the same as HL2 gold
			// but allows us to use more memory on 1GB+ systems
			if (host_parms.memsize > MAXIMUM_DEDICATED_MEMORY)
			{
				host_parms.memsize = MAXIMUM_DEDICATED_MEMORY;
			}
		}
		else
		{
			// just take one quarter, no cap
			host_parms.memsize >>= 2;
		}

		// At least MINIMUM_WIN_MEMORY mb, even if we have to swap a lot.
		if (host_parms.memsize < MINIMUM_WIN_MEMORY)
		{
			host_parms.memsize = MINIMUM_WIN_MEMORY;
		}

		// Apply cap
		if (host_parms.memsize > MAXIMUM_WIN_MEMORY)
		{
			host_parms.memsize = MAXIMUM_WIN_MEMORY;
		}
	}
	else
	{
		host_parms.memsize = 128*1024*1024;
	}
#elif defined ( DEDICATED )
	// hard code 32 mb for dedicated servers
	host_parms.memsize = MAXIMUM_DEDICATED_MEMORY;

#elif defined(POSIX)
	uint64_t memsize = ONE_HUNDRED_TWENTY_EIGHT_MB;

#if defined(OSX)
	int mib[2] = { CTL_HW, HW_MEMSIZE };
	u_int namelen = sizeof(mib) / sizeof(mib[0]);
	size_t len = sizeof(memsize);

	if (sysctl(mib, namelen, &memsize, &len, NULL, 0) < 0) 
	{
		memsize = ONE_HUNDRED_TWENTY_EIGHT_MB;
	}
#elif defined(LINUX)
	const int fd = open("/proc/meminfo", O_RDONLY);
	if (fd < 0)
	{
		Sys_Error( "Can't open /proc/meminfo (%s)!\n", strerror(errno) );
	}

	char buf[1024 * 16];
	const ssize_t br = read(fd, buf, sizeof (buf));
	close(fd);
	if (br < 0)
	{
		Sys_Error( "Can't read /proc/meminfo (%s)!\n", strerror(errno) );
	}
	buf[br] = '\0';

	// Split up the buffer by lines...
	char *line = buf;
	for (char *ptr = buf; *ptr; ptr++)
	{
		if (*ptr == '\n')
		{
			// we've got a complete line.
			*ptr = '\0';
			unsigned long long ull = 0;
			if (sscanf(line, "MemTotal: %llu kB", &ull) == 1)
			{
				// found it!
				memsize = ((uint64_t) ull) * 1024;
				break;
			}
			line = ptr;
		}
	}

#else
#error Write me.
#endif

	if ( memsize > 0xFFFFFFFFUL )
	{
		host_parms.memsize = 0xFFFFFFFFUL;
	}
	else
	{
		host_parms.memsize = memsize;
	}
	
	if ( host_parms.memsize < ONE_HUNDRED_TWENTY_EIGHT_MB )
	{
		Sys_Error( "Available memory less than 128MB!!! %i\n", host_parms.memsize );
	}
	
	// take one quarter the physical memory
	if ( host_parms.memsize <= 512*1024*1024)
	{
		host_parms.memsize >>= 2;
		// Apply cap of 64MB for 512MB systems
		// this keeps the code the same as HL2 gold
		// but allows us to use more memory on 1GB+ systems
		if (host_parms.memsize > MAXIMUM_DEDICATED_MEMORY)
		{
			host_parms.memsize = MAXIMUM_DEDICATED_MEMORY;
		}
	}
	else
	{
		// just take one quarter, no cap
		host_parms.memsize >>= 2;
	}
	
	// At least MINIMUM_WIN_MEMORY mb, even if we have to swap a lot.
	if (host_parms.memsize < MINIMUM_WIN_MEMORY)
	{
		host_parms.memsize = MINIMUM_WIN_MEMORY;
	}
	
	// Apply cap
	if (host_parms.memsize > MAXIMUM_WIN_MEMORY)
	{
		host_parms.memsize = MAXIMUM_WIN_MEMORY;
	}
	
#elif defined( _PS3 )
	host_parms.memsize = 128*1024*1024;
#else
	#error Write me.
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parms - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void Sys_ShutdownMemory( void )
{
	host_parms.memsize = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Sys_InitAuthentication( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Sys_ShutdownAuthentication( void )
{
}

//-----------------------------------------------------------------------------
// Debug library spew output
//-----------------------------------------------------------------------------
#ifdef _PS3
#include "tls_ps3.h"
#define g_bInSpew GetTLSGlobals()->bEngineConsoleIsInSpew
#else
CTHREADLOCALINT g_bInSpew;
#endif

#include "tier1/fmtstr.h"

static ConVar sys_minidumpspewlines( "sys_minidumpspewlines", "500", FCVAR_RELEASE, "Lines of crash dump console spew to keep." );

static CUtlLinkedList< CUtlString > g_SpewHistory;
static int g_nSpewLines = 1;
static CThreadFastMutex g_SpewMutex;

static void AddSpewRecord( char const *pMsg )
{
#if !defined( DEDICATED ) && !defined( _GAMECONSOLE )
	CUtlString str;
	str.Format( "%d(%f):  %s", g_nSpewLines, Plat_FloatTime(), pMsg );

	AUTO_LOCK_FM( g_SpewMutex );
	++g_nSpewLines;
	if ( g_SpewHistory.Count() > sys_minidumpspewlines.GetInt() )
	{
		g_SpewHistory.Remove( g_SpewHistory.Head() );
	}


	g_SpewHistory.AddToTail( str );
#endif
}

void GetSpew( char *buf, size_t buflen )
{
	AUTO_LOCK_FM( g_SpewMutex );

	// Walk list backward
	char *pcur = buf;
	int remainder = (int)buflen - 1;

	// Walk backward(
	for ( int i = g_SpewHistory.Tail(); i != g_SpewHistory.InvalidIndex(); i = g_SpewHistory.Previous( i ) )
	{
		const CUtlString &rec = g_SpewHistory[ i ];
		int len = rec.Length();
		int tocopy = MIN( len, remainder );

		if ( tocopy <= 0 )
			break;
		
		Q_memcpy( pcur, rec.String(), tocopy );
		remainder -= tocopy;
		pcur += tocopy;

		if ( remainder <= 0 )
			break;
	}
	*pcur = 0;
}



#ifdef _PS3
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_PSGL, "PSGL" );
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_VJOBS, "VJOBS" );
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_PHYSICS, "PHYSICS" );
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_LOADING, "LOADING" );
#endif

class CEngineConsoleLoggingListener : public ILoggingListener
{
public:
	virtual void Log( const LoggingContext_t *pContext, const tchar *pMessage )
	{
		if ( ( pContext->m_Flags & LCF_DO_NOT_ECHO ) != 0 )
		{
			if ( pContext->m_ChannelID == LOG_SERVER_LOG )
			{
				g_Log.Print( pMessage );
			}
			return;
		}
#if defined( _PS3 ) && !defined( _CERT )
		if( pContext->m_ChannelID == LOG_PSGL )
		{
			uint wrote;
			// write to TTY USR1
			sys_tty_write( SYS_TTYP3, pMessage, V_strlen(pMessage), &wrote );
			return;
		}
		else if( pContext->m_ChannelID == LOG_VJOBS )
		{
			uint wrote;
			// write to TTY USR2
			sys_tty_write( SYS_TTYP4, pMessage, V_strlen(pMessage), &wrote );
			return;
		}
		else if( pContext->m_ChannelID == LOG_PHYSICS )
		{
			uint wrote;
			// write to TTY USR3
			sys_tty_write( SYS_TTYP5, pMessage, V_strlen(pMessage), &wrote );
			return;
		}
		else if( pContext->m_ChannelID == LOG_LOADING )
		{
			uint wrote;
			// write to TTY USR4
			sys_tty_write( SYS_TTYP6, pMessage, V_strlen(pMessage), &wrote );
			return;
		}
		// NOTE: SYS_TTYP13 is taken by vxconsole
		COMPILE_TIME_ASSERT( SYS_TTYP13 == 13 );
#endif
		

		bool suppress = g_bInSpew ? true : false;

		g_bInSpew = true;

		AddSpewRecord( pMessage );

		if ( !suppress )
		{
			// If this is a dedicated server, then we have taken over its spew function, but we still
			// want its vgui console to show the spew, so pass it into the dedicated server.
			if ( dedicated )
			{
				if ( ! IsChildProcess() )							// do NOT let subprocesses output to stdout.
				{
					// This is not actually a varargs-style printf function; it simply takes a char*
					dedicated->Sys_Printf( (char *) pMessage );		// stupid header has char * instead of const char *
				}
				else
				{
#ifdef _LINUX
					SendStringToParentProcess( va( "#%02d:%s", g_nForkID, pMessage ) );
#endif
				}

			}

#ifndef _CERT
			if ( g_bTextMode )
			{
				printf( "%s", pMessage );
			}
#endif
			Color spewColor = pContext->m_Color;
			switch ( pContext->m_Severity )
			{
#ifndef DEDICATED
			case LS_MESSAGE:
				if ( pContext->m_Color == UNSPECIFIED_LOGGING_COLOR )
				{
#if !defined( _X360 )
					spewColor.SetColor( 255, 255, 255, 255 );
#else
					spewColor.SetColor( 0, 0, 0, 255 );
#endif
				}
				break;
			case LS_WARNING:
				spewColor.SetColor( 255, 90, 90, 255 );
				break;
			case LS_ASSERT:
				spewColor.SetColor( 255, 20, 20, 255 );
				break;
			case LS_ERROR:
				spewColor.SetColor( 20, 70, 255, 255 );
				break;
#endif
			}
			
			Con_ColorPrintf( spewColor, "%s", pMessage );
		}

		g_bInSpew = false;

		if ( pContext->m_Severity == LS_ERROR )
		{
			Sys_Error( "%s", pMessage );
		}
	}
};

static CEngineConsoleLoggingListener s_EngineLoggingListener;
static CFileLoggingListener s_FileLoggingListener;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CFileLoggingListener, IFileLoggingListener, FILELOGGINGLISTENER_INTERFACE_VERSION, s_FileLoggingListener );

void DeveloperChangeCallback( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	// Set the "developer" spew group to the value...
	ConVarRef var( pConVar );
	int val = var.GetInt();

	LoggingSystem_SetChannelSpewLevelByTag( "Developer", val >= 1 ? LS_MESSAGE : LS_ERROR );
	LoggingSystem_SetChannelSpewLevelByTag( "DeveloperVerbose", val >= 2 ? LS_MESSAGE : LS_ERROR );
}

//-----------------------------------------------------------------------------
// Purpose: factory comglomerator, gets the client, server, and gameui dlls together
//-----------------------------------------------------------------------------
void *GameFactory( const char *pName, int *pReturnCode )
{
	void *pRetVal = NULL;

	// first ask the app factory
	pRetVal = g_AppSystemFactory( pName, pReturnCode );
	if (pRetVal)
		return pRetVal;

	// ask matchmaking
	extern CreateInterfaceFn g_pfnMatchmakingFactory;
	pRetVal = g_pfnMatchmakingFactory( pName, pReturnCode );
	if (pRetVal)
		return pRetVal;

#ifndef DEDICATED
	// now ask the client dll
	if (ClientDLL_GetFactory())
	{
		pRetVal = ClientDLL_GetFactory()( pName, pReturnCode );
		if (pRetVal)
			return pRetVal;
	}

	// gameui.dll
	if (EngineVGui()->GetGameUIFactory())
	{
		pRetVal = EngineVGui()->GetGameUIFactory()( pName, pReturnCode );
		if (pRetVal)
			return pRetVal;
	}
#endif	
	// server dll factory access would go here when needed
	
	// ask vjobs
#ifdef ENGINE_MANAGES_VJOBS
	extern CreateInterfaceFn g_pfnVjobsFactory;
	pRetVal = g_pfnVjobsFactory( pName, pReturnCode );
	if( pRetVal )
		return pRetVal;
#endif

	return NULL;
}



// factory instance
CreateInterfaceFn g_GameSystemFactory = GameFactory;


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *lpOrgCmdLine - 
//			launcherFactory - 
//			*pwnd - 
//			bIsDedicated - 
// Output : int
//-----------------------------------------------------------------------------
int Sys_InitGame( CreateInterfaceFn appSystemFactory, const char* pBaseDir, void *pwnd, int bIsDedicated )
{
#ifdef BENCHMARK
	if ( bIsDedicated )
	{
		Error( "Dedicated server isn't supported by this benchmark!" );
	}
#endif

	extern void InitMathlib( void );
	InitMathlib();
	
	FileSystem_SetWhitelistSpewFlags();

	// Activate console, non-dev spew
	// Must happen before developer.InstallChangeCallback because that callback may reset it 
	LoggingSystem_SetChannelSpewLevelByTag( "Console", LS_MESSAGE );
	LoggingSystem_PushLoggingState();
	LoggingSystem_RegisterLoggingListener( &s_EngineLoggingListener );
	LoggingSystem_RegisterLoggingListener( &s_FileLoggingListener );

	// Install debug spew output....
	developer.InstallChangeCallback( DeveloperChangeCallback );
	
	// Assume failure
	host_initialized = false;
	// Grab main window pointer
	pmainwindow = (HWND *)pwnd;

	// Remember that this is a dedicated server
	s_bIsDedicated = bIsDedicated ? true : false;

	memset( &gmodinfo, 0, sizeof( modinfo_t ) );

	static char s_pBaseDir[256];
	Q_strncpy( s_pBaseDir, pBaseDir, sizeof( s_pBaseDir ) );
	Q_strlower( s_pBaseDir );
	Q_FixSlashes( s_pBaseDir );
	host_parms.basedir = s_pBaseDir;

#ifdef LINUX
	if ( CommandLine()->FindParm ( "-pidfile" ) )
	{	
		FileHandle_t pidFile = g_pFileSystem->Open( CommandLine()->ParmValue ( "-pidfile", "srcds.pid" ), "w+" );
		if ( pidFile )
		{
			char dir[MAX_PATH];
			getcwd( dir, sizeof(dir) );
			g_pFileSystem->FPrintf( pidFile, "%i\n", getpid() );
			g_pFileSystem->Close(pidFile);
		}
		else
		{
			Warning("Unable to open pidfile (%s)\n", CommandLine()->CheckParm ( "-pidfile" ));
		}
	}
#endif


	// Initialize clock
	TRACEINIT( Sys_Init(), Sys_Shutdown() );

#if defined(_DEBUG)
	if ( IsPC() )
	{
		if( !CommandLine()->FindParm( "-nodttest" ) && !CommandLine()->FindParm( "-dti" ) )
		{
			RunDataTableTest();	
		}
	}
#endif

	// NOTE: Can't use COM_CheckParm here because it hasn't been set up yet.
	SeedRandomNumberGenerator( CommandLine()->FindParm( "-random_invariant" ) != 0 );

	TRACEINIT( Sys_InitMemory(), Sys_ShutdownMemory() );

	TRACEINIT( Host_Init( s_bIsDedicated ), Host_Shutdown() );

	if ( !host_initialized )
	{
		return 0;
	}

	TRACEINIT( Sys_InitAuthentication(), Sys_ShutdownAuthentication() );

	MapReslistGenerator_BuildMapList();

#if !defined(NO_STEAM) && !defined(DEDICATED) && !defined(LINUX)
	Status_Update();
	BuildMinidumpComment( NULL );
#endif


	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Sys_ShutdownGame( void )
{
	TRACESHUTDOWN( Sys_ShutdownAuthentication() );

	TRACESHUTDOWN( Host_Shutdown() );

	TRACESHUTDOWN( Sys_ShutdownMemory() );

	// TRACESHUTDOWN( Sys_ShutdownArgv() );

	TRACESHUTDOWN( Sys_Shutdown() );

	// Remove debug spew output....
	developer.RemoveChangeCallback( DeveloperChangeCallback );
	LoggingSystem_PopLoggingState();
}

//
// Try to load a single DLL.  If it conforms to spec, keep it loaded, and add relevant
// info to the DLL directory.  If not, ignore it entirely.
//

CreateInterfaceFn g_ServerFactory;


static bool LoadThisDll( char *szDllFilename, bool bServerOnly )
{
	CSysModule *pDLL = NULL;

	// check signature, don't let users with modified binaries connect to secure servers, they will get VAC banned
	if ( !bServerOnly && !Host_AllowLoadModule( szDllFilename, "GAMEBIN", false ) )
	{
		// not supposed to load this but we will anyway
		Host_DisallowSecureServers();
	}

	// Load DLL, ignore if cannot
	// ensures that the game.dll is running under Steam
	// this will have to be undone when we want mods to be able to run
	if ((pDLL = g_pFileSystem->LoadModule(szDllFilename, "GAMEBIN", false)) == NULL)
	{
		ConMsg("Failed to load %s\n", szDllFilename);
		goto IgnoreThisDLL;
	}

	// Load interface factory and any interfaces exported by the game .dll
	g_ServerFactory = Sys_GetFactory( pDLL );
	if ( g_ServerFactory )
	{
		g_bServerGameDLLGreaterThanV5 = true;
		serverGameDLL = (IServerGameDLL*)g_ServerFactory(INTERFACEVERSION_SERVERGAMEDLL, NULL);
		if ( !serverGameDLL )
		{
#ifdef REL_TO_STAGING_MERGE_TODO
			// Need to merge eiface for this.
			// g_bServerGameDLLGreaterThanV5 is true, so we get better stringtables
			g_bServerGameDLLGreaterThanV5 = true;
			serverGameDLL = (IServerGameDLL*)g_ServerFactory(INTERFACEVERSION_SERVERGAMEDLL_VERSION_5, NULL);
#endif			
			if ( !serverGameDLL )
			{
				Msg( "Could not get IServerGameDLL interface from library %s", szDllFilename );
				goto IgnoreThisDLL;
			}
		}


		serverGameEnts = (IServerGameEnts*)g_ServerFactory(INTERFACEVERSION_SERVERGAMEENTS, NULL);
		if ( !serverGameEnts )
		{
			ConMsg( "Could not get IServerGameEnts interface from library %s", szDllFilename );
			goto IgnoreThisDLL;
		}
		
		serverGameClients = (IServerGameClients*)g_ServerFactory(INTERFACEVERSION_SERVERGAMECLIENTS, NULL);
		if ( serverGameClients )
		{
			g_iServerGameClientsVersion = 4;
		}
		else
		{
			// Try the previous version.
			const char *pINTERFACEVERSION_SERVERGAMECLIENTS_V3 = "ServerGameClients003";
			serverGameClients = (IServerGameClients*)g_ServerFactory(pINTERFACEVERSION_SERVERGAMECLIENTS_V3, NULL);
			if ( serverGameClients )
			{
				g_iServerGameClientsVersion = 3;
			}
			else
			{
				ConMsg( "Could not get IServerGameClients interface from library %s", szDllFilename );
				goto IgnoreThisDLL;
			}
		}
		serverGameDirector = (IHLTVDirector*)g_ServerFactory(INTERFACEVERSION_HLTVDIRECTOR, NULL);
		if ( !serverGameDirector )
		{
			ConMsg( "Could not get IHLTVDirector interface from library %s", szDllFilename );
			// this is not a critical 
		}

		serverReplayDirector = (IReplayDirector*)g_ServerFactory(INTERFACEVERSION_REPLAYDIRECTOR, NULL);
		if ( !serverReplayDirector )
		{
			ConMsg( "Could not get IReplayDirector interface from library %s", szDllFilename );
			// this is not a critical 
		}

		serverGameTags = (IServerGameTags*)g_ServerFactory(INTERFACEVERSION_SERVERGAMETAGS, NULL);
		// Possible that this is NULL - optional interface
	}
	else
	{
		ConMsg( "Could not find factory interface in library %s", szDllFilename );
		goto IgnoreThisDLL;
	}

	g_GameDLL = pDLL;
	return true;

IgnoreThisDLL:
	if (pDLL != NULL)
	{
		g_pFileSystem->UnloadModule(pDLL);
		serverGameDLL = NULL;
		serverGameEnts = NULL;
		serverGameClients = NULL;
	}
	return false;
}

//
// Scan DLL directory, load all DLLs that conform to spec.
//
void LoadEntityDLLs( const char *szBaseDir, bool bServerOnly )
{
	memset( &gmodinfo, 0, sizeof( modinfo_t ) );
	gmodinfo.version = 1;
	gmodinfo.svonly  = true;

	// Run through all DLLs found in the extension DLL directory
	g_GameDLL = NULL;
	sv_noclipduringpause = NULL;

	// Listing file for this game.
	KeyValues *modinfo = new KeyValues("modinfo");
	if (modinfo->LoadFromFile(g_pFileSystem, "gameinfo.txt"))
	{
		Q_strncpy( gmodinfo.szInfo, modinfo->GetString("url_info"), sizeof( gmodinfo.szInfo ) );
		Q_strncpy( gmodinfo.szDL, modinfo->GetString("url_dl"), sizeof( gmodinfo.szDL ) );
		gmodinfo.version = modinfo->GetInt("version");
		gmodinfo.size = modinfo->GetInt("size");
		gmodinfo.svonly = modinfo->GetBool("svonly");
		gmodinfo.cldll = modinfo->GetBool("cldll");
		Q_strncpy( gmodinfo.szHLVersion, modinfo->GetString("hlversion"), sizeof( gmodinfo.szHLVersion ) );
	}
	modinfo->deleteThis();
	
	// Load the game .dll
	char szDllFilename[ MAX_PATH ];

#if defined( _WIN32 )
	// [mpritchar] cstrike15 - we now look for server_valve.dll { Valve's datacenter specific version of server }
	//    first and load it if we find it, otherwise load server.dll

	if ( s_bIsDedicatedServer && !CommandLine()->FindParm( "-novalveds" ) )
	{
		Q_snprintf( szDllFilename, sizeof( szDllFilename ), "server_valve" DLL_EXT_STRING );
		LoadThisDll( szDllFilename, bServerOnly );
	}

	if ( !serverGameDLL )
	{
		Q_snprintf( szDllFilename, sizeof( szDllFilename ), "server" DLL_EXT_STRING );
		LoadThisDll( szDllFilename, bServerOnly );
	}

#elif defined( _PS3 )
	Q_snprintf( szDllFilename, sizeof( szDllFilename ), "server" DLL_EXT_STRING );
	LoadThisDll( szDllFilename, bServerOnly );
#elif defined( LINUX )
	if ( s_bIsDedicatedServer && !CommandLine()->FindParm( "-novalveds" ) )
	{
		Q_snprintf( szDllFilename, sizeof( szDllFilename ), "server_valve" );
		LoadThisDll( szDllFilename, bServerOnly );
	}
	if ( !serverGameDLL )
	{
		Q_snprintf( szDllFilename, sizeof( szDllFilename ), "server"  );
		LoadThisDll( szDllFilename, bServerOnly );
	}
#elif defined( POSIX )
	Q_snprintf( szDllFilename, sizeof( szDllFilename ), "server" );
	LoadThisDll( szDllFilename, bServerOnly );
#else
	#error "define server.dll type"
#endif


	if ( serverGameDLL )
	{
		Msg("Game.dll loaded for \"%s\"\n", (char *)serverGameDLL->GetGameDescription());
	}
}

//-----------------------------------------------------------------------------
// Purpose: Retrieves a string value from the registry
//-----------------------------------------------------------------------------
#if defined(_WIN32)
void Sys_GetRegKeyValueUnderRoot( HKEY rootKey, const char *pszSubKey, const char *pszElement, char *pszReturnString, int nReturnLength, const char *pszDefaultValue )
{
	LONG lResult;           // Registry function result code
	HKEY hKey;              // Handle of opened/created key
	char szBuff[128];       // Temp. buffer
	ULONG dwDisposition;    // Type of key opening event
	DWORD dwType;           // Type of key
	DWORD dwSize;           // Size of element data

	// Assume the worst
	Q_strncpy(pszReturnString, pszDefaultValue, nReturnLength );

	// Create it if it doesn't exist.  (Create opens the key otherwise)
	lResult = RegCreateKeyEx(
		rootKey,	// handle of open key 
		pszSubKey,			// address of name of subkey to open 
		0ul,					// DWORD ulOptions,	  // reserved 
		"String",			// Type of value
		REG_OPTION_NON_VOLATILE, // Store permanently in reg.
		KEY_ALL_ACCESS,		// REGSAM samDesired, // security access mask 
		NULL,
		&hKey,				// Key we are creating
		&dwDisposition);    // Type of creation
	
	if (lResult != ERROR_SUCCESS)  // Failure
		return;

	// First time, just set to Valve default
	if (dwDisposition == REG_CREATED_NEW_KEY)
	{
		// Just Set the Values according to the defaults
		lResult = RegSetValueEx( hKey, pszElement, 0, REG_SZ, (CONST BYTE *)pszDefaultValue, Q_strlen(pszDefaultValue) + 1 ); 
	}
	else
	{
		// We opened the existing key. Now go ahead and find out how big the key is.
		dwSize = nReturnLength;
		lResult = RegQueryValueEx( hKey, pszElement, 0, &dwType, (unsigned char *)szBuff, &dwSize );

		// Success?
		if (lResult == ERROR_SUCCESS)
		{
			// Only copy strings, and only copy as much data as requested.
			if (dwType == REG_SZ)
			{
				Q_strncpy(pszReturnString, szBuff, nReturnLength);
				pszReturnString[nReturnLength - 1] = '\0';
			}
		}
		else
		// Didn't find it, so write out new value
		{
			// Just Set the Values according to the defaults
			lResult = RegSetValueEx( hKey, pszElement, 0, REG_SZ, (CONST BYTE *)pszDefaultValue, Q_strlen(pszDefaultValue) + 1 ); 
		}
	};

	// Always close this key before exiting.
	RegCloseKey(hKey);

}


//-----------------------------------------------------------------------------
// Purpose: Retrieves a DWORD value from the registry
//-----------------------------------------------------------------------------
void Sys_GetRegKeyValueUnderRootInt( HKEY rootKey, const char *pszSubKey, const char *pszElement, long *plReturnValue, const long lDefaultValue )
{
	LONG lResult;           // Registry function result code
	HKEY hKey;              // Handle of opened/created key
	ULONG dwDisposition;    // Type of key opening event
	DWORD dwType;           // Type of key
	DWORD dwSize;           // Size of element data

	// Assume the worst
	// Set the return value to the default
	*plReturnValue = lDefaultValue; 

	// Create it if it doesn't exist.  (Create opens the key otherwise)
	lResult = RegCreateKeyEx(
		rootKey,	// handle of open key 
		pszSubKey,			// address of name of subkey to open 
		0ul,					// DWORD ulOptions,	  // reserved 
		"String",			// Type of value
		REG_OPTION_NON_VOLATILE, // Store permanently in reg.
		KEY_ALL_ACCESS,		// REGSAM samDesired, // security access mask 
		NULL,
		&hKey,				// Key we are creating
		&dwDisposition);    // Type of creation

	if (lResult != ERROR_SUCCESS)  // Failure
		return;

	// First time, just set to Valve default
	if (dwDisposition == REG_CREATED_NEW_KEY)
	{
		// Just Set the Values according to the defaults
		lResult = RegSetValueEx( hKey, pszElement, 0, REG_DWORD, (CONST BYTE *)&lDefaultValue, sizeof( DWORD ) ); 
	}
	else
	{
		// We opened the existing key. Now go ahead and find out how big the key is.
		dwSize = sizeof( DWORD );
		lResult = RegQueryValueEx( hKey, pszElement, 0, &dwType, (unsigned char *)plReturnValue, &dwSize );

		// Success?
		if (lResult != ERROR_SUCCESS)
			// Didn't find it, so write out new value
		{
			// Just Set the Values according to the defaults
			lResult = RegSetValueEx( hKey, pszElement, 0, REG_DWORD, (LPBYTE)&lDefaultValue, sizeof( DWORD ) ); 
		}
	};

	// Always close this key before exiting.
	RegCloseKey(hKey);

}


void Sys_SetRegKeyValueUnderRoot( HKEY rootKey, const char *pszSubKey, const char *pszElement, const char *pszValue )
{
	LONG lResult;           // Registry function result code
	HKEY hKey;              // Handle of opened/created key
	//char szBuff[128];       // Temp. buffer
	ULONG dwDisposition;    // Type of key opening event
	//DWORD dwType;           // Type of key
	//DWORD dwSize;           // Size of element data

	// Create it if it doesn't exist.  (Create opens the key otherwise)
	lResult = RegCreateKeyEx(
		rootKey,			// handle of open key 
		pszSubKey,			// address of name of subkey to open 
		0ul,					// DWORD ulOptions,	  // reserved 
		"String",			// Type of value
		REG_OPTION_NON_VOLATILE, // Store permanently in reg.
		KEY_ALL_ACCESS,		// REGSAM samDesired, // security access mask 
		NULL,
		&hKey,				// Key we are creating
		&dwDisposition);    // Type of creation
	
	if (lResult != ERROR_SUCCESS)  // Failure
		return;

	// First time, just set to Valve default
	if (dwDisposition == REG_CREATED_NEW_KEY)
	{
		// Just Set the Values according to the defaults
		lResult = RegSetValueEx( hKey, pszElement, 0, REG_SZ, (CONST BYTE *)pszValue, Q_strlen(pszValue) + 1 ); 
	}
	else
	{
		/*
		// FIXE:  We might want to support a mode where we only create this key, we don't overwrite values already present
		// We opened the existing key. Now go ahead and find out how big the key is.
		dwSize = nReturnLength;
		lResult = RegQueryValueEx( hKey, pszElement, 0, &dwType, (unsigned char *)szBuff, &dwSize );

		// Success?
		if (lResult == ERROR_SUCCESS)
		{
			// Only copy strings, and only copy as much data as requested.
			if (dwType == REG_SZ)
			{
				Q_strncpy(pszReturnString, szBuff, nReturnLength);
				pszReturnString[nReturnLength - 1] = '\0';
			}
		}
		else
		*/
		// Didn't find it, so write out new value
		{
			// Just Set the Values according to the defaults
			lResult = RegSetValueEx( hKey, pszElement, 0, REG_SZ, (CONST BYTE *)pszValue, Q_strlen(pszValue) + 1 ); 
		}
	};

	// Always close this key before exiting.
	RegCloseKey(hKey);
}
#endif

void Sys_GetRegKeyValue( char *pszSubKey, char *pszElement,	char *pszReturnString, int nReturnLength, char *pszDefaultValue )
{
#if defined(_WIN32)
	Sys_GetRegKeyValueUnderRoot( HKEY_CURRENT_USER, pszSubKey, pszElement, pszReturnString, nReturnLength, pszDefaultValue );
#else
	//hushed Assert( !"Impl me" );
	Q_strncpy( pszReturnString, pszDefaultValue, nReturnLength );
#endif
}

void Sys_GetRegKeyValueInt( char *pszSubKey, char *pszElement, long *plReturnValue, long lDefaultValue)
{
#if defined(_WIN32)
	Sys_GetRegKeyValueUnderRootInt( HKEY_CURRENT_USER, pszSubKey, pszElement, plReturnValue, lDefaultValue );
#else
	//hushed Assert( !"Impl me" );
	*plReturnValue = lDefaultValue;
#endif
}

void Sys_SetRegKeyValue( char *pszSubKey, char *pszElement,	const char *pszValue )
{
#if defined(_WIN32)
	Sys_SetRegKeyValueUnderRoot( HKEY_CURRENT_USER, pszSubKey, pszElement, pszValue );
#else
	//hushed Assert( !"Impl me" );
#endif
}

#define SOURCE_ENGINE_APP_CLASS "Valve.Source"

void Sys_CreateFileAssociations( int count, FileAssociationInfo *list )
{
#if defined(_WIN32)
	if ( IsX360() )
		return;

	char appname[ 512 ];

	GetModuleFileName( 0, appname, sizeof( appname ) );
	Q_FixSlashes( appname );
	Q_strlower( appname );

	char quoted_appname_with_arg[ 512 ];
	Q_snprintf( quoted_appname_with_arg, sizeof( quoted_appname_with_arg ), "\"%s\" \"%%1\"", appname );
	char base_exe_name[ 256 ];
	Q_FileBase( appname, base_exe_name, sizeof( base_exe_name) );
	Q_DefaultExtension( base_exe_name, ".exe", sizeof( base_exe_name ) );

	// HKEY_CLASSES_ROOT/Valve.Source/shell/open/command == "u:\tf2\hl2.exe" "%1" quoted
	Sys_SetRegKeyValueUnderRoot( HKEY_CLASSES_ROOT, va( "%s\\shell\\open\\command", SOURCE_ENGINE_APP_CLASS ), "", quoted_appname_with_arg );
	// HKEY_CLASSES_ROOT/Applications/hl2.exe/shell/open/command == "u:\tf2\hl2.exe" "%1" quoted
	Sys_SetRegKeyValueUnderRoot( HKEY_CLASSES_ROOT, va( "Applications\\%s\\shell\\open\\command", base_exe_name ), "", quoted_appname_with_arg );

	for ( int i = 0; i < count ; i++ )
	{
		FileAssociationInfo *fa = &list[ i ];
		char binding[32];
		binding[0] = 0;
		// Create file association for our .exe
		// HKEY_CLASSES_ROOT/.dem == "Valve.Source"
		Sys_GetRegKeyValueUnderRoot( HKEY_CLASSES_ROOT, fa->extension, "", binding, sizeof(binding), "" );
		if ( Q_strlen( binding ) == 0 )
		{
			Sys_SetRegKeyValueUnderRoot( HKEY_CLASSES_ROOT, fa->extension, "", SOURCE_ENGINE_APP_CLASS );
		}
	}
#endif
}

void Sys_NoCrashDialog()
{
#if defined(_WIN32)
	::SetErrorMode(SetErrorMode(SEM_NOGPFAULTERRORBOX) | SEM_NOGPFAULTERRORBOX);
#endif
}

void Sys_TestSendKey( const char *pKey )
{
#if defined(_WIN32)
	int key = pKey[0];
	if ( pKey[0] == '\\' && pKey[1] == 'r' )
	{
		key = VK_RETURN;
	}

	HWND hWnd = (HWND)game->GetMainWindow();
	PostMessageA( hWnd, WM_KEYDOWN, key, 0 );
	PostMessageA( hWnd, WM_KEYUP, key, 0 );

	//void Key_Event (int key, bool down);
	//Key_Event( key, 1 );
	//Key_Event( key, 0 );
#endif
}

void Sys_OutputDebugString(const char *msg)
{
	Plat_DebugString( msg );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void UnloadEntityDLLs( void )
{
	if ( !g_GameDLL )
		return;

	// Unlink the cvars associated with game DLL
	FileSystem_UnloadModule( g_GameDLL );
	g_GameDLL = NULL;
	serverGameDLL = NULL;
	serverGameEnts = NULL;
	serverGameClients = NULL;
	sv_noclipduringpause = NULL;
}

CON_COMMAND( star_memory, "Dump memory stats" )
{
	// get a current stat of available memory
	// 32 MB is reserved and fixed by OS, so not reporting to allow memory loggers sync
#ifdef LINUX
	struct mallinfo memstats = mallinfo( );
	Msg( "sbrk size: %.2f MB, Used: %.2f MB, #mallocs = %d\n",
		 memstats.arena / ( 1024.0 * 1024.0), memstats.uordblks / ( 1024.0 * 1024.0 ), memstats.hblks );
#elif OSX
	struct mstats memstats = mstats( );
	Msg( "Available %.2f MB, Used: %.2f MB, #mallocs = %d\n",
		 memstats.bytes_free / ( 1024.0 * 1024.0), memstats.bytes_used / ( 1024.0 * 1024.0 ), memstats.chunks_used );
#elif defined( _PS3 )
	Msg( "Memory info on PS3: not implemented.\n" );
#else
	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof( MEMORYSTATUSEX );
	GlobalMemoryStatusEx( &statex );
	Msg( "Available: %.2f MB, Used: %.2f MB, Free: %.2f MB\n", 
		statex.ullTotalPhys/( 1024.0f*1024.0f ) - 32.0f,
		( statex.ullTotalPhys - statex.ullAvailPhys )/( 1024.0f*1024.0f ) - 32.0f, 
		statex.ullAvailPhys/( 1024.0f*1024.0f ) );
#endif
}

#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )
//NOTE: These convars are here because they can't be directly in tier0.
//	They're more like one-way convars in that they send off the changes, but might not have the same starting value as the actual value
//	So if you change the defaults, change the defaults in tier0/dbg.cpp to match.
//	I considered adding some callback functionality to reinforce the bond a bit better. But that seems hairy.
static void warningcallstacks_enable_callback( IConVar *var, const char *pOldValue, float flOldValue )
{
	_Warning_AlwaysSpewCallStack_Enable( ((ConVar *)var)->GetBool() );
}
ConVar warningcallstacks_enable( "warningcallstacks_enable", "0", FCVAR_DEVELOPMENTONLY, "All Warning()/DevWarning()/... calls will attach a callstack", warningcallstacks_enable_callback );

static void warningcallstacks_length_callback( IConVar *var, const char *pOldValue, float flOldValue )
{
	_Warning_AlwaysSpewCallStack_Length( ((ConVar *)var)->GetInt() );
}
ConVar warningcallstacks_length( "warningcallstacks_length", "5", FCVAR_DEVELOPMENTONLY, "Length of automatic warning callstacks", warningcallstacks_length_callback );

static void errorcallstacks_enable_callback( IConVar *var, const char *pOldValue, float flOldValue )
{
	_Error_AlwaysSpewCallStack_Enable( ((ConVar *)var)->GetBool() );
}
ConVar errorcallstacks_enable( "errorcallstacks_enable", "0", FCVAR_DEVELOPMENTONLY, "All Error() calls will attach a callstack", errorcallstacks_enable_callback );

static void errorcallstacks_length_callback( IConVar *var, const char *pOldValue, float flOldValue )
{
	_Error_AlwaysSpewCallStack_Length( ((ConVar *)var)->GetInt() );
}
ConVar errorcallstacks_length( "errorcallstacks_length", "20", FCVAR_DEVELOPMENTONLY, "Length of automatic error callstacks", errorcallstacks_length_callback );
#endif //#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )


