//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ========//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include <windows.h> 
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <eh.h>
#include "isys.h"
#include "conproc.h"
#include "dedicated.h"
#include "engine_hlds_api.h"
#include "checksum_md5.h"
#include "tier0/dbg.h"
#include "tier0/stacktools.h"
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "inputsystem/iinputsystem.h"
#include "SteamAppStartup.h"
#include "console/textconsole.h"
#include "vgui/vguihelpers.h"
#include "appframework/appframework.h"
#include "materialsystem/imaterialsystem.h"
#include "istudiorender.h"
#include "vgui/ivgui.h"
#include "console/TextConsoleWin32.h"
#include "icvar.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "vphysics_interface.h"
#include "filesystem.h"
#include "vscript/ivscript.h"
#include "steam/steam_api.h"

extern CTextConsoleWin32 console;
extern bool g_bVGui;

//-----------------------------------------------------------------------------
// Purpose: Implements OS Specific layer ( loosely )
//-----------------------------------------------------------------------------
class CSys : public ISys
{
public:
	virtual		~CSys( void );

	virtual bool LoadModules( CDedicatedAppSystemGroup *pAppSystemGroup );

	void		Sleep( int msec );
	bool		GetExecutableName( char *out );
	void		ErrorMessage( int level, const char *msg );

	void		WriteStatusText( char *szText );
	void		UpdateStatus( int force );

	long		LoadLibrary( char *lib );
	void		FreeLibrary( long library );

	bool		CreateConsoleWindow( void );
	void		DestroyConsoleWindow( void );

	void		ConsoleOutput ( char *string );
	char		*ConsoleInput (void);
	void		Printf(const char *fmt, ...);
};

static CSys g_Sys;
ISys *sys = &g_Sys;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSys::~CSys()
{
	sys = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msec - 
//-----------------------------------------------------------------------------
void CSys::Sleep( int msec )
{
	::Sleep( msec );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *lib - 
// Output : long
//-----------------------------------------------------------------------------
long CSys::LoadLibrary( char *lib )
{
	void *hDll = ::LoadLibrary( lib );

	if ( hDll )
		StackToolsNotify_LoadedLibrary( lib );

	return (long)hDll;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : library - 
//-----------------------------------------------------------------------------
void CSys::FreeLibrary( long library )
{
	if ( !library )
		return;

	::FreeLibrary( (HMODULE)library );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *out - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSys::GetExecutableName( char *out )
{
	if ( !::GetModuleFileName( ( HINSTANCE )GetModuleHandle( NULL ), out, 256 ) )
	{
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : level - 
//			*msg - 
//-----------------------------------------------------------------------------
void CSys::ErrorMessage( int level, const char *msg )
{
	MessageBox( NULL, msg, "Half-Life", MB_OK );
	PostQuitMessage(0);	
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : force - 
//-----------------------------------------------------------------------------
void CSys::UpdateStatus( int force )
{
	static double tLast = 0.0;
	double	tCurrent;
	char	szPrompt[256];
	int		n, nMax;
	char	szMap[64];
	char	szHostname[128];
	float	fps;

	if ( !engine )
		return;

	tCurrent = Sys_FloatTime();

	if ( !force )
	{
		if ( ( tCurrent - tLast ) < 0.5f )
			return;
	}

	tLast = tCurrent;

	engine->UpdateStatus( &fps, &n, &nMax, szMap, sizeof( szMap ) );
	engine->UpdateHostname( szHostname, sizeof( szHostname ) );

	console.SetTitle( szHostname );

	Q_snprintf( szPrompt, sizeof( szPrompt ), "%.1f fps %2i/%2i on map %16s", (float)fps, n, nMax, szMap);

	console.SetStatusLine(szPrompt);
	console.UpdateStatus();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *string - 
// Output : void CSys::ConsoleOutput
//-----------------------------------------------------------------------------
void CSys::ConsoleOutput (char *string)
{
	if ( g_bVGui )
	{
		VGUIPrintf( string );
	}
	else
	{
		console.Print(string);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CSys::Printf(const char *fmt, ...)
{
	// Dump text to debugging console.
	va_list argptr;
	char szText[1024];

	va_start (argptr, fmt);
	Q_vsnprintf (szText, sizeof( szText ), fmt, argptr);
	va_end (argptr);

	// Get Current text and append it.
	ConsoleOutput( szText );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char *
//-----------------------------------------------------------------------------
char *CSys::ConsoleInput (void)
{
	return console.GetLine();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szText - 
//-----------------------------------------------------------------------------
void CSys::WriteStatusText( char *szText )
{
	SetConsoleTitle( szText );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSys::CreateConsoleWindow( void )
{
	if ( !AllocConsole () )
	{
		return false;
	}
	
	InitConProc();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSys::DestroyConsoleWindow( void )
{
	FreeConsole ();

	// shut down QHOST hooks if necessary
	DeinitConProc ();
}


//-----------------------------------------------------------------------------
// Loading modules used by the dedicated server.
//-----------------------------------------------------------------------------
bool CSys::LoadModules( CDedicatedAppSystemGroup *pAppSystemGroup )
{
	AppSystemInfo_t appSystems[] = 
	{
		{ "engine.dll",				CVAR_QUERY_INTERFACE_VERSION },	// NOTE: This one must be first!!
		{ "soundemittersystem.dll",	SOUNDEMITTERSYSTEM_INTERFACE_VERSION },
		{ "inputsystem.dll",		INPUTSYSTEM_INTERFACE_VERSION },
		{ "inputsystem.dll",		INPUTSTACKSYSTEM_INTERFACE_VERSION },
		{ "materialsystem.dll",		MATERIAL_SYSTEM_INTERFACE_VERSION },
		{ "studiorender.dll",		STUDIO_RENDER_INTERFACE_VERSION },
		{ "vphysics.dll",			VPHYSICS_INTERFACE_VERSION },
		{ "datacache.dll",			DATACACHE_INTERFACE_VERSION },
		{ "datacache.dll",			MDLCACHE_INTERFACE_VERSION },
		{ "datacache.dll",			STUDIO_DATA_CACHE_INTERFACE_VERSION },
		{ "vgui2.dll",				VGUI_IVGUI_INTERFACE_VERSION },
#ifndef DOTA_DLL
		{ "vscript.dll",			VSCRIPT_INTERFACE_VERSION },
#endif
		{ "engine.dll",				VENGINE_HLDS_API_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	if ( !pAppSystemGroup->AddSystems( appSystems ) ) 
		return false;
	
	engine = (IDedicatedServerAPI *)pAppSystemGroup->FindSystem( VENGINE_HLDS_API_VERSION );

	IMaterialSystem* pMaterialSystem = (IMaterialSystem*)pAppSystemGroup->FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION );
	pMaterialSystem->SetShaderAPI( "shaderapiempty.dll" );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool NET_Init( void )
{
	// Startup winock
	WORD version = MAKEWORD( 1, 1 );
	WSADATA wsaData;

	int err = WSAStartup( version, &wsaData );
	if ( err != 0 )
	{
		char msg[ 256 ];
		Q_snprintf( msg, sizeof( msg ), "Winsock 1.1 unavailable...\n" );
		sys->Printf( "%s", msg );
		Plat_DebugString( msg );
		return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void NET_Shutdown( void )
{
	// Kill winsock
	WSACleanup();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hInstance - 
//			hPrevInstance - 
//			lpszCmdLine - 
//			nCmdShow - 
// Output : int PASCAL
//-----------------------------------------------------------------------------
int main(int argc, char **argv); // in sys_ded.cpp
static char *GetBaseDir( const char *pszBuffer )
{
	static char	basedir[ MAX_PATH ];
	char szBuffer[ MAX_PATH ];
	int j;
	char *pBuffer = NULL;

	strcpy( szBuffer, pszBuffer );

	pBuffer = strrchr( szBuffer,'\\' );
	if ( pBuffer )
	{
		*(pBuffer+1) = '\0';
	}

	strcpy( basedir, szBuffer );

	j = strlen( basedir );
	if (j > 0)
	{
		if ( ( basedir[ j-1 ] == '\\' ) || 
			 ( basedir[ j-1 ] == '/' ) )
		{
			basedir[ j-1 ] = 0;
		}
	}

	return basedir;
}

void MiniDumpFunction( unsigned int nExceptionCode, EXCEPTION_POINTERS *pException )
{
	SteamAPI_WriteMiniDump( nExceptionCode, pException, 0 );
}

extern "C" __declspec(dllexport) int DedicatedMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	SetAppInstance( hInstance );

	// Check that we are running on Win32
	OSVERSIONINFO	vinfo;
	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if ( !GetVersionEx ( &vinfo ) )
		return -1;

	if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32s )
		return -1;

	int argc, iret = -1;
	LPWSTR * argv= CommandLineToArgvW(GetCommandLineW(),&argc);
	CommandLine()->CreateCmdLine( GetCommandLine() );

	if ( !Plat_IsInDebugSession() && !CommandLine()->FindParm( "-nominidumps") )
	{
		// This warning is not actually true in this context.
#pragma warning( suppress : 4535 ) // warning C4535: calling _set_se_translator() requires /EHa
		_set_se_translator( MiniDumpFunction );

		try  // this try block allows the SE translator to work
		{
			iret = main(argc,(char **)argv);
		}
		catch( ... )
		{
			return -1;
		}
	}
	else
	{
		iret = main(argc,(char **)argv);
	}

	GlobalFree( argv );
	return iret;
}

