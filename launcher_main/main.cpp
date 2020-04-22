//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: A redirection tool that allows the DLLs to reside elsewhere.
//
//=====================================================================================//

#if defined( _WIN32 ) && !defined( _X360 )
#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <direct.h>
#endif
#if defined( _X360 )
#define _XBOX
#include <xtl.h>
#include <xbdm.h>
#undef _XBOX
#include <stdio.h>
#include <assert.h>
#include "xbox\xbox_core.h"
#include "xbox\xbox_launch.h"
#elif defined( SN_TARGET_PS3 )
#include "../public/ps3_pathinfo.h"
#include <stddef.h>
#include <cell/fios/fios_common.h>
#include <cell/fios/fios_memory.h>
#include <cell/fios/fios_configuration.h>
#include <cell/fios/fios_time.h>
#include <sys/tty.h>
#include <sys/ppu_thread.h>
#include "tier0/vprof_sn.h"
#include "errorrenderloop.h"
//#if defined( VPROF_SN_LEVEL )
#include "sn/libsntuner.h"
#include "libsn.h"
//#endif
#endif
#ifdef POSIX
#include <stdio.h>
#include <stdlib.h>
#ifndef SN_TARGET_PS3
#include <dlfcn.h>
#endif // SN_TARGET_PS3
#include <limits.h>
#include <string.h>
#define MAX_PATH PATH_MAX
#endif

#include "tier0/platform.h"
#include "tier0/basetypes.h"

#if defined( VPCGAME )
#define _VPCGAME_STRING_HACK2(x) #x
#define _VPCGAME_STRING_HACK1(x) _VPCGAME_STRING_HACK2(x)
#define VPCGAME_STRING _VPCGAME_STRING_HACK1(VPCGAME)
#endif

#ifdef WIN32
typedef int (*LauncherMain_t)( HINSTANCE hInstance, HINSTANCE hPrevInstance, 
							  LPSTR lpCmdLine, int nCmdShow );
#elif POSIX
typedef int (*LauncherMain_t)( int argc, char **argv );
#else
#error
#endif


#ifdef WIN32
// hinting the nvidia driver to use the dedicated graphics card in an optimus configuration
// for more info, see: http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
extern "C" { _declspec( dllexport ) DWORD NvOptimusEnablement = 0x00000001; }

// same thing for AMD GPUs using v13.35 or newer drivers
extern "C" { __declspec( dllexport ) int AmdPowerXpressRequestHighPerformance = 1; }

#endif



//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------
#if !defined( _X360 )

#ifdef WIN32

static char *GetBaseDir( const char *pszBuffer )
{
	static char	basedir[ MAX_PATH ];
	char szBuffer[ MAX_PATH ];
	size_t j;
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

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	// Must add 'bin' to the path....
	char* pPath = getenv("PATH");

	// Use the .EXE name to determine the root directory
	char moduleName[ MAX_PATH ];
	char szBuffer[4096];
	if ( !GetModuleFileName( hInstance, moduleName, MAX_PATH ) )
	{
		MessageBox( 0, "Failed calling GetModuleFileName", "Launcher Error", MB_OK );
		return 0;
	}

	// Get the root directory the .exe is in
	char* pRootDir = GetBaseDir( moduleName );

	const char* pBinPath = 
#ifdef _WIN64
		"\\x64"
#else
		""
#endif
	;

#ifdef _DEBUG
	int len = 
#endif
	_snprintf( szBuffer, sizeof( szBuffer ), "PATH=%s\\bin%s\\;%s", pRootDir, pBinPath, pPath );
	szBuffer[sizeof( szBuffer ) - 1] = '\0';
	assert( len < sizeof( szBuffer ) );
	_putenv( szBuffer );

	// Assemble the full path to our "launcher.dll"
	_snprintf( szBuffer, sizeof( szBuffer ), "%s\\bin%s\\launcher.dll", pRootDir, pBinPath );
	szBuffer[sizeof( szBuffer ) - 1] = '\0';

	// STEAM OK ... filesystem not mounted yet
#if defined(_X360)
	HINSTANCE launcher = LoadLibrary( szBuffer );
#else
	HINSTANCE launcher = LoadLibraryEx( szBuffer, NULL, LOAD_WITH_ALTERED_SEARCH_PATH );
#endif
	if ( !launcher )
	{
		char *pszError;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&pszError, 0, NULL);

		char szBuf[1024];
		_snprintf(szBuf, sizeof( szBuf ), "Failed to load the launcher DLL:\n\n%s", pszError);
		szBuf[sizeof( szBuf ) - 1] = '\0';
		MessageBox( 0, szBuf, "Launcher Error", MB_OK );

		LocalFree(pszError);
		return 0;
	}

	LauncherMain_t main = (LauncherMain_t)GetProcAddress( launcher, "LauncherMain" );
	return main( hInstance, hPrevInstance, lpCmdLine, nCmdShow );
}

#elif defined( SN_TARGET_PS3 )

#if defined( __GCC__ )
#define COMPILER_GCC
#elif defined( __SNC__ )
#define COMPILER_SNC
#endif
#include "../public/tls_ps3.h"
#include "sys/process.h"

// We need to avoid printf before we
// configure our custom memory allocator
#define printf(...) ((void)0)
#include "../common/ps3/ps3_helpers.h"

#ifdef APPCHANGELISTVERSION
// write the changelist number into the executable so that the GUID changes between builds. 
// previously, setting the version number via the SYS_MODULE_INFO was good enough to do
// this, but not after sdk 350. 
volatile unsigned int clnumber = APPCHANGELISTVERSION;
__attribute__ ((noinline)) void DummyFuncForUpdatingGUIDs( char *pOut )
{
	sprintf( pOut, "%x", clnumber );
}
#else // absent appchangelistversion, invent one
volatile unsigned int clnumber = 0;
#define DUMMY_VER_STRING __DATE__ " " __TIME__
volatile char dummyVersionDateString[] = DUMMY_VER_STRING "\n";
__attribute__ ((noinline)) void DummyFuncForUpdatingGUIDs( char *pOut )
{
	sprintf( pOut, DUMMY_VER_STRING );
}
#undef DUMMY_VER_STRING
#endif

// 1 Mb stack is maximum allowed for the main thread
// and we will make good use of it
SYS_PROCESS_PARAM( 1000, 1 * 1024 * 1024 )

/////////////////////////////////////////////////////////////////////////////////////////////////////
// All thread-local storage must reside in the ELF and be exported for PRXes to use it
/////////////////////////////////////////////////////////////////////////////////////////////////////
__thread TLSGlobals gTLSGlobals =
{
	// TLS values/flags
	/*nThreadLocalStateIndex*/		0,
	/*TLSValues*/					{ NULL },
	/*TLSFlags*/					{ false },
	/*bWaitObjectsCreated*/			false,
	/*WaitObjectsSemaphore*/		0,
	/*pCurThread*/					NULL,
	/*nThreadID*/					0,
	
	// Engine TLS data (zip/console/splitslot)
	/*uiEngineZipLastErrorZ*/		0,
	/*bEngineConsoleIsInSpew*/		false,
	/*pEngineSplitSlot*/			NULL,

	// Malloc debugging TLS data
	/*pMallocDbgInfoStack*/			NULL,
	/*nMallocDbgInfoStackDepth*/	0,

	// Filesystem read filename buffer
	/*pFileSystemReadFilename*/		NULL,

	// Material system render context
	/*pMaterialSystemRenderContext*/ NULL,

	// Physics virtual mesh frame locks
	/*pPhysicsVirtualMeshFrameLocks*/ NULL,
	
	/*bNormalQuitRequested*/ false
};
TLSGlobals *GetTLSGlobals_ELF() { return &gTLSGlobals; }

extern CPs3ContentPathInfo g_Ps3GameDataPathInfo;

template< typename BaseStruct >
struct PS3MainParameters : public BaseStruct
{
	PS3MainParameters() { memset( this, 0, sizeof( BaseStruct ) ); BaseStruct::cbSize = sizeof( BaseStruct ); }
};

struct PS3_Launch_t
{
	explicit PS3_Launch_t( char const *szPrxName, PS3_PrxLoadParametersBase_t *pParams ) :
		m_szPrxName( szPrxName ), m_pPrxParams( pParams )
	{
		m_iResult = PS3_PrxLoad( m_szPrxName, m_pPrxParams );
		if ( m_iResult < CELL_OK ) 
		{
			printf( "ERROR: %s PRX load failed: 0x%08x\n", m_szPrxName, m_iResult );
		}
		else
		{
			printf( "Loaded: %s (0x%08x)\n", m_szPrxName, m_iResult );
		}
	}

	char const *m_szPrxName;
	PS3_PrxLoadParametersBase_t *m_pPrxParams;
	int m_iResult;
};

static const char *LauncherMainSPRXPath( const char *modulename, char *buf, int buflen = CELL_GAME_PATH_MAX ) // formats a path to the module. returns a pointer to the buf param for convenience. 
{
	snprintf( buf, buflen, "%s/%s" DLL_EXT_STRING, g_Ps3GameDataPathInfo.PrxPath(), modulename  );
	return buf;
}

#if defined( SN_TARGET_PS3 )
void TunerMarkerPush( const char * pName )
{
//#if defined( VPROF_SN_LEVEL )
	snPushMarker( pName );
//#endif
}

void TunerMarkerPop()
{
//#if defined( VPROF_SN_LEVEL )
	snPopMarker();
//#endif
}

// this is debug-only counter; never use it for anything other than debugging!
uint64_t g_nDebugSwapBufferCount = 0; 
void TunerSwapBufferMarker()
{
	// this dummy function is only required as a patch-through for Tuner that attaches to the game after the game has been started, as a convenience funciton/
	// it must be called at every frame boundary (presumably at/after psglSwap )
	g_nDebugSwapBufferCount++;
}
#endif

PS3_PrxModuleEntry_t *g_pPrxModulesList = NULL;
PS3_PrxModuleEntry_t ** PS3_PrxGetModulesList() { return &g_pPrxModulesList; }


void TestThreadProc( uint64_t id )
{
	printf( "Hello from PPU thread %lld\n", id );
	sys_ppu_thread_exit( id );
}

void TestThreads( int nLevel = 0)
{
	printf("testing threads\n");
	const int numThreads = 20;
	sys_ppu_thread_t id[numThreads];
	for( int i = 0;i < numThreads; ++i )
	{
		if( nLevel > 0 )
			TestThreads( nLevel - 1 );
		if( CELL_OK != sys_ppu_thread_create( &id[i], TestThreadProc, i, 1001, 64*1024, SYS_PPU_THREAD_CREATE_JOINABLE, "SimpleThread" ) )
		{
			printf("ERROR: cannot create thread\n");
			return;
		}
	}
	
	for( int i = 0;i < numThreads; ++i )
	{
		uint64_t res;
		sys_ppu_thread_join( id[i], &res );
		if( res != i )
		{
			printf("ERROR: invalid thread return value\n");
			return;
		}
	}
}

int MainImpl( int argc, char *argv[] )
{
// #ifdef _CERT // possibly enable it for ship to disable command line cheating?
#if 0
	// Disable command line support for shipping unless -certcmdline specified
	if ( ( argc > 1 ) && !strcmp( argv[1], "-certcmdline" ) )
	{
		argc = 1;
	}
#endif

	// this is the very first timing message, before tier0 is even initialized and we can use any 
	// logging or timing facilities; this is the baseline to measure loading times
	cell::fios::abstime_t fiosLaunchTime = cell::fios::FIOSGetCurrentTime();
	
#ifndef _CERT
	{
		double flTime = cell::fios::FIOSAbstimeToMicroseconds( fiosLaunchTime ) * 1e-6;
		char buffer[4096];
		int nMessageSize = snprintf(buffer, sizeof(buffer), "--------------------------------------\n 0.0000 / %8.4f : launcher_main(", flTime );
		for( int i = 0;i < argc && nMessageSize < sizeof(buffer)-4; ++i )
		{
			// add delimiters
			if( i > 0 )
				buffer[nMessageSize++] = ',';
			nMessageSize += snprintf( buffer + nMessageSize, sizeof(buffer) - nMessageSize, " \"%s\"", argv[i] );
		}																				    
		nMessageSize += snprintf(buffer + nMessageSize, sizeof(buffer) - nMessageSize, " )\n" );
		unsigned wrote;
		sys_tty_write( SYS_TTYP6, buffer, nMessageSize, &wrote );
	}
#endif

	// this is a dummy operation to force the compiler to not elide the
	// changelist GUID. It's really necessary, because otherwise the compiler
	// will notice the function isn't called, and elide it, and the GUID string,
	// altogether. Because the compiler "can't" know what argc will be, it has
	// to compile in the function call here. This is the only way to guarantee
	// that different builds will have different GUIDs, because we don't often
	// change launcher_main between versions, and the PRXes don't get individual
	// GUIDs in the dump. 
	// Don't pass in more than one million commandline 
	// parameters or this will corrupt the one millionth.
	if ( argc > 100000000 )
	{
		DummyFuncForUpdatingGUIDs( argv[100000000] );
	}



	char path[CELL_GAME_PATH_MAX];
	bool bDevHddCfgOnly = false;
	bool bRunLauncherMain = true;
	bool bSupportPathLegacyArgs = true;
	bool bEnableMlaa = true;

#ifdef HDD_BOOT
	unsigned int uiInitFlags = CPs3ContentPathInfo::INIT_PRX_ON_HDD | CPs3ContentPathInfo::INIT_IMAGE_ON_HDD;
#else
	unsigned int uiInitFlags = CPs3ContentPathInfo::INIT_RETAIL_MODE;		// assume that if no arguments are specified we are going to run in retail mode
#endif


	// uncomment the following in order to allow starting game in /app_home from XMB (shortcutting creating disk image)
	// uiInitFlags = CPs3ContentPathInfo::INIT_PRX_APP_HOME | CPs3ContentPathInfo::INIT_IMAGE_APP_HOME;

	for ( int k = 0; k < argc; ++ k )
	{
		if( !strcmp( "-noMlaa", argv[k] ) )
		{
			bEnableMlaa = false;
		}
		else if ( !strcmp( "-errorrenderloop", argv[k] ) )
		{
			return -1;
		}
		else if ( !strcmp( "-devhddcfgonly", argv[k] ) )
		{
			bDevHddCfgOnly = true;
		}
		else if ( !strcmp( "-nolaunchermain", argv[k] ) )
		{
			bRunLauncherMain = false;
		}
		else if ( !strcmp( "-syscacheclear", argv[k] ) )
		{
			uiInitFlags |= CPs3ContentPathInfo::INIT_SYS_CACHE_CLEAR;
		}
		else if ( !strncmp( "-path_retail", argv[k], 12 ) )
		{
			bSupportPathLegacyArgs = false;
			if ( argv[k][12] )
				uiInitFlags &=~CPs3ContentPathInfo::INIT_RETAIL_MODE;
		}
		else if ( !strncmp( "-path_prx_", argv[k], 10 ) )
		{
			char chPrxPathMode = argv[k][10];
			switch ( chPrxPathMode )
			{
			case 'h': uiInitFlags |= CPs3ContentPathInfo::INIT_PRX_ON_HDD; break;
			case 'b': uiInitFlags |= CPs3ContentPathInfo::INIT_PRX_ON_BDVD; break;
			case 'a': uiInitFlags |= CPs3ContentPathInfo::INIT_PRX_APP_HOME; break;
			}
		}
		else if ( !strncmp( "-path_img_", argv[k], 10 ) )
		{
			char chPrxPathMode = argv[k][10];
			switch ( chPrxPathMode )
			{
			case 'h': uiInitFlags |= CPs3ContentPathInfo::INIT_IMAGE_ON_HDD; break;
			case 'b': uiInitFlags |= CPs3ContentPathInfo::INIT_IMAGE_ON_BDVD; break;
			case 'a': uiInitFlags |= CPs3ContentPathInfo::INIT_IMAGE_APP_HOME; break;
			}
		}
				// LEGACY PARAMETERS because people are used to running with them (need to clean up some time later)
				else if ( bSupportPathLegacyArgs && !strcmp( "-dev", argv[k] ) )
				{
					uiInitFlags &=~CPs3ContentPathInfo::INIT_RETAIL_MODE;
					uiInitFlags |= CPs3ContentPathInfo::INIT_IMAGE_APP_HOME;
					uiInitFlags |= CPs3ContentPathInfo::INIT_PRX_APP_HOME;
				}
				else if ( bSupportPathLegacyArgs && !strcmp( "-ps3hd", argv[k] ) )
				{
					uiInitFlags &=~CPs3ContentPathInfo::INIT_RETAIL_MODE;
					uiInitFlags |= CPs3ContentPathInfo::INIT_IMAGE_ON_HDD;
					uiInitFlags |= CPs3ContentPathInfo::INIT_PRX_APP_HOME;
				}
				else if ( bSupportPathLegacyArgs && !strcmp( "-nops3hd", argv[k] ) )
				{
					uiInitFlags &=~CPs3ContentPathInfo::INIT_RETAIL_MODE;
					uiInitFlags |= CPs3ContentPathInfo::INIT_IMAGE_APP_HOME;
					uiInitFlags |= CPs3ContentPathInfo::INIT_PRX_APP_HOME;
				}
				else if ( bSupportPathLegacyArgs && !strcmp( "-dev_bdvd", argv[k] ) )
				{
					uiInitFlags &=~CPs3ContentPathInfo::INIT_RETAIL_MODE;
					uiInitFlags |= CPs3ContentPathInfo::INIT_IMAGE_ON_BDVD;
					uiInitFlags |= CPs3ContentPathInfo::INIT_PRX_APP_HOME;
				}
				// END LEGACY PARAMETERS
	}

	// uncomment the following to hard code for Eurogamer (as if running -dev)
#ifdef _PS3
//	uiInitFlags &=~CPs3ContentPathInfo::INIT_RETAIL_MODE;
#endif

	int iPathInfoInitResult = g_Ps3GameDataPathInfo.Init( uiInitFlags );
	if ( iPathInfoInitResult < 0 )
		return iPathInfoInitResult;

	if ( bDevHddCfgOnly )
		return 0;

	PS3MainParameters< PS3_LoadTier0_Parameters_t > tier0;
	tier0.pfnGetTlsGlobals = GetTLSGlobals_ELF;
	tier0.pPS3PathInfo = &g_Ps3GameDataPathInfo;
	tier0.fiosLaunchTime = fiosLaunchTime;
	tier0.nCLNumber = clnumber;
	tier0.pfnPushMarker = TunerMarkerPush;
	tier0.pfnPopMarker = TunerMarkerPop;
	tier0.pfnSwapBufferMarker = TunerSwapBufferMarker;
	tier0.ppPrxModulesList = PS3_PrxGetModulesList();
	tier0.m_pGcmSharedData = &g_gcmSharedData;
	

#ifndef _CERT

	tier0.snRawSPULockHandler = snRawSPULockHandler;
	tier0.snRawSPUUnlockHandler = snRawSPUUnlockHandler;
	tier0.snRawSPUNotifyCreation = snRawSPUNotifyCreation;
	tier0.snRawSPUNotifyDestruction = snRawSPUNotifyDestruction;
	tier0.snRawSPUNotifyElfLoad = snRawSPUNotifyElfLoad;
	tier0.snRawSPUNotifyElfLoadNoWait = snRawSPUNotifyElfLoadNoWait;
	tier0.snRawSPUNotifyElfLoadAbs = snRawSPUNotifyElfLoadAbs;
	tier0.snRawSPUNotifyElfLoadAbsNoWait = snRawSPUNotifyElfLoadAbsNoWait;
	tier0.snRawSPUNotifySPUStopped = snRawSPUNotifySPUStopped;
	tier0.snRawSPUNotifySPUStarted = snRawSPUNotifySPUStarted;

#endif

	(void)bEnableMlaa; // we'll use it if we need to init GCM and start rendering right away
/*
	g_gcmSharedData.m_nIoMemorySize = bEnableMlaa ? 5 * 1024 * 1024 : 1 * 1024 * 1024;
	sys_addr_t pIoAddress = NULL;
	int nError = sys_memory_allocate( g_gcmSharedData.m_nIoMemorySize, SYS_MEMORY_PAGE_SIZE_1M, &pIoAddress );
	if( CELL_OK != nError || !pIoAddress )
	{
		// cannot allocate IO memory
		return -2;
	}
	
	int32 result = cellGcmInit( m_nCmdSize, m_nIoSize, m_pIoAddress );
	if ( result < CELL_OK )
		return result;

	g_gcmSharedData.m_pIoMemory = ( void* )pIoAddress;
*/
	
	PS3_Launch_t tier0Launch( LauncherMainSPRXPath( "tier0", path ), &tier0 );
	if( tier0Launch.m_iResult < CELL_OK )
	{
		return -1;
	}

	int iAppRetCode = 0;
	PS3MainParameters< PS3_PrxLoadParametersBase_t > vstdlib;
	PS3_Launch_t vstdlibLaunch( LauncherMainSPRXPath( "vstdlib", path ), &vstdlib );
	if( vstdlibLaunch.m_iResult >= CELL_OK )
	{
		
	#ifndef NO_STEAM
		PS3MainParameters< PS3_PrxLoadParametersBase_t > steamapi;
		PS3_Launch_t steamapiLaunch( LauncherMainSPRXPath( "steam_api", path ), &steamapi );
		if ( steamapiLaunch.m_iResult >= CELL_OK )
		{
	#endif
			PS3MainParameters< PS3_LoadLauncher_Parameters_t > launcher;
			PS3_Launch_t launcherLaunch( LauncherMainSPRXPath( "launcher", path ), &launcher );

			if ( launcher.pfnLauncherMain )
			{
				printf( "Launching...\n" );
				iAppRetCode = bRunLauncherMain ? (*launcher.pfnLauncherMain)( argc, argv ) : 0;

				printf( "Shutting down...\n" );
				launcher.pfnLauncherShutdown();
			}
			else
			{
				printf( "ERROR: failed to obtain LauncherMain entry point!\n" );
			}
			PS3_PrxUnload( launcher.sysPrxId );
		
	#ifndef NO_STEAM
			PS3_PrxUnload( steamapi.sysPrxId );
		}
	#endif

		PS3_PrxUnload( vstdlib.sysPrxId );
	}

	// Before tier0 unloads make sure that there are no modules remaining loaded
	#if !defined( _CERT )
	for ( PS3_PrxModuleEntry_t *pEntry = *PS3_PrxGetModulesList(); pEntry; pEntry = pEntry->pNextModule )
	{
		if ( strstr( pEntry->chName, "/tier0" DLL_EXT_STRING ) )
			continue;

		unsigned int dummy;
		char const *szWarnMsg = "EXITING WITH PRX MODULE: ";
		sys_tty_write( SYS_TTYP6, szWarnMsg, strlen( szWarnMsg ), &dummy );
		
		sys_tty_write( SYS_TTYP6, pEntry->chName, strlen( pEntry->chName ), &dummy );
		
		szWarnMsg = "\n";
		sys_tty_write( SYS_TTYP6, szWarnMsg, strlen( szWarnMsg ), &dummy );
	}
	#endif

	tier0.pfnTier0Shutdown();
	PS3_PrxUnload( tier0.sysPrxId );

	return iAppRetCode;
}


int main( int argc, char *argv[] )
{

	// Init LibSn
#ifndef _CERT
	snInit();
#endif

	int nReturn = MainImpl( argc, argv );

#ifndef _PS3

// 	if( !gTLSGlobals.bNormalQuitRequested )
// 	{
// 		printf("no normal quit requested, starting error render loop\n");
// 		ErrorRenderLoop loop;
// 		loop.Run();
// 		printf("Error render loop finished\n");
// 	}

#endif


#if !defined( _CERT )
	if ( 1 )
	{
		unsigned int dummy;
		char const *szWarnMsg =
			(*PS3_PrxGetModulesList())
			? "------- WARNING: RETURNING FROM MAIN WITH PRX MODULES RUNNING --------\n"
			: "--------------------------------BYE-----------------------------------\n";
		sys_tty_write( SYS_TTYP6, szWarnMsg, strlen( szWarnMsg ), &dummy );
	}
#endif

	return nReturn;
}


#elif defined (POSIX)

int main( int argc, char *argv[] )
{
#ifdef PLATFORM_64BITS
	#ifdef OSX
		const char *pLauncherPath = "bin/osx64/launcher" DLL_EXT_STRING;
	#else
		const char *pLauncherPath = "bin/linux64/launcher" DLL_EXT_STRING;
	#endif
#else
	const char *pLauncherPath = "bin/launcher" DLL_EXT_STRING;
#endif

	void *launcher = dlopen( pLauncherPath, RTLD_NOW );
	
	if ( !launcher )
	{
		printf( "Failed to load the launcher (%s)\n", dlerror() );
		while(1);
		return 0;
	}
	
	LauncherMain_t main = (LauncherMain_t)dlsym( launcher, "LauncherMain" );
	if ( !main )
	{
		printf( "Failed to load the launcher entry proc\n" );
		while(1);
		return 0;
	}

	return main( argc, argv );
}

#else
#error
#endif // WIN32 || POSIX


#else // X360
//-----------------------------------------------------------------------------
// 360 Quick and dirty command line parsing. Returns true if key found,
// false otherwise. Caller can optionally get next argument.
//-----------------------------------------------------------------------------
bool ParseCommandLineArg( const char *pCmdLine, const char* pKey, char* pValueBuff = NULL, int valueBuffSize = 0 )
{
	int keyLen = (int)strlen( pKey );
	const char* pArg = pCmdLine;
	for ( ;; )
	{
		// scan for match
		pArg = strstr( (char*)pArg, pKey );
		if ( !pArg )
		{
			return false;
		}
		
		// found, but could be a substring
		if ( pArg[keyLen] == '\0' || pArg[keyLen] == ' ' )
		{
			// exact match
			break;
		}

		pArg += keyLen;
	}

	if ( pValueBuff )
	{
		// caller wants next token
		// skip past key and whitespace
		pArg += keyLen;
		while ( *pArg == ' ' )
		{
			pArg++;
		}

		int i;
		for ( i=0; i<valueBuffSize; i++ )
		{
			pValueBuff[i] = *pArg;
			if ( *pArg == '\0' || *pArg == ' ' )
				break;
			pArg++;
		}
		pValueBuff[i] = '\0';
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// 360 Quick and dirty command line arg stripping.
//-----------------------------------------------------------------------------
void StripCommandLineArg( const char *pCmdLine, char *pNewCmdLine, const char *pStripArg )
{
	// cannot operate in place
	assert( pCmdLine != pNewCmdLine );

	int numTotal = strlen( pCmdLine ) + 1;
	const char* pArg = strstr( pCmdLine, pStripArg );
	if ( !pArg )
	{
		strcpy( pNewCmdLine, pCmdLine );
		return;
	}

	int numDiscard = strlen( pStripArg );
	while ( pArg[numDiscard] && ( pArg[numDiscard] != '-' && pArg[numDiscard] != '+' ) )
	{
		// eat whitespace up to the next argument
		numDiscard++;
	}

	memcpy( pNewCmdLine, pCmdLine, pArg - pCmdLine );
	memcpy( pNewCmdLine + ( pArg - pCmdLine ), (void*)&pArg[numDiscard], numTotal - ( pArg + numDiscard - pCmdLine  ) );

	// ensure we don't leave any trailing whitespace, occurs if last arg is stripped
	int len = strlen( pNewCmdLine );
	while ( len > 0 &&  pNewCmdLine[len-1] == ' ' )
	{
		len--;
	}
	pNewCmdLine[len] = '\0';
}

//-----------------------------------------------------------------------------
// 360 Conditional spew
//-----------------------------------------------------------------------------
void Spew( const char *pFormat, ... )
{
#if defined( _DEBUG )
	char	msg[2048];
	va_list	argptr;

	va_start( argptr, pFormat );
	vsprintf( msg, pFormat, argptr );
	va_end( argptr );

	OutputDebugString( msg );
#endif
}

//-----------------------------------------------------------------------------
// Get the new entry point and command line
//-----------------------------------------------------------------------------
LauncherMain_t GetLaunchEntryPoint( char *pNewCommandLine )
{
	HMODULE		hModule;
	char		*pCmdLine;

	// determine source of our invocation, internal or external
	// a valid launch payload will have an embedded command line
	// command line could be from internal restart in dev or retail mode
	CXboxLaunch xboxLaunch;
	int payloadSize;
	unsigned int launchID;
	char *pPayload;
	bool bInternalRestart = xboxLaunch.GetLaunchData( &launchID, (void**)&pPayload, &payloadSize );
	if ( !bInternalRestart || !payloadSize || launchID != VALVE_LAUNCH_ID )
	{
		// could be first time, get command line from system
		pCmdLine = GetCommandLine();
		if ( !stricmp( pCmdLine, "\"default.xex\"" ) )
		{
			// matches retail xex and no arguments, mut be first time retail launch
			pCmdLine = "default.xex";
#if defined( _MEMTEST )
			pCmdLine = "default.xex +mat_picmip 2";
#endif
		}
	}
	else
	{
		// get embedded command line from payload
		pCmdLine = pPayload;
	}

	int launchFlags = 0;
	if ( launchID == VALVE_LAUNCH_ID )
	{
		launchFlags = xboxLaunch.GetLaunchFlags();
	}
#if !defined( _CERT )
	if ( launchFlags & LF_ISDEBUGGING )
	{
		while ( !DmIsDebuggerPresent() )
		{
		}

		Sleep( 1000 );
		Spew( "Resuming debug session.\n" );
	}
#endif

	// unforunately, the xbox erases its internal store upon first fetch
	// must re-establish it so the payload that contains other data (past command line) can be accessed by the game
	// the launch data will be owned by tier0 and supplied to game
	if ( launchID == VALVE_LAUNCH_ID )
	{
		xboxLaunch.SetLaunchData( pPayload, payloadSize, launchFlags );
	}
#if defined( _DEMO )
	else if ( pPayload && payloadSize )
	{
		// not our data
		// restore the launch data as expected
		xboxLaunch.SetLaunchData( pPayload, payloadSize, LF_UNKNOWNDATA );
	}
#endif

#if defined( _DEMO )
	// the demo version cannot trust launch environment
	// Kiosk or Magazines launch in unpredictable ways with unknown paths
	// MUST slam the command line!!!
#if !defined( _CERT )
	// take the command line as specified by the debugger
	if ( !DmIsDebuggerPresent() )
	{
		pCmdLine = "default.xex";
	}
#else
	pCmdLine = "default.xex";
#endif
#endif

	// The 360 has no paths and therefore the xex must reside in the same location as the dlls.
	// Only the xex must reside locally, on the box, but the dlls can be mounted from the remote share.
	// Resolve all known implicitly loaded dlls to be explicitly loaded now to allow their remote location.
	const char *pImplicitDLLs[] =
	{
		"tier0_360.dll",
		"vstdlib_360.dll",
		"vxbdm_360.dll",
		"launcher_360.dll",
	};

	// Corresponds to pImplicitDLLs. A dll load failure is only an error if that dll is tagged as required.
	const bool bDllRequired[] = 
	{
		true,	// tier0
		true,	// vstdlib
		false,	// vxbdm
		true,	// ???
	};

	char gameName[32];
	if ( !ParseCommandLineArg( pCmdLine, "-game", gameName, sizeof( gameName ) ) )
	{
#if defined( VPCGAME_STRING )
		strcpy( gameName, VPCGAME_STRING );
#endif
	}
	else
	{
		// sanitize a possible absolute game path back to expected game name
		char *pSlash = strrchr( gameName, '\\' );
		if ( pSlash )
		{
			memcpy( gameName, pSlash+1, strlen( pSlash+1 )+1 );
		}
	}

	// resolve which application gets launched
	// default is to application
	pImplicitDLLs[ARRAYSIZE( pImplicitDLLs )-1] = "launcher_360.dll";

	// the base path is the where the game is predominantly anchored
	// game runs from dvd only
	// this can only be the d: by definition on the xbox
	const char *pBasePath = "d:";

	// load all the dlls specified
	char dllPath[MAX_PATH];
	for ( int i=0; i<ARRAYSIZE( pImplicitDLLs ); i++ )
	{
		hModule = NULL;
		sprintf( dllPath, "%s\\bin\\%s", pBasePath, pImplicitDLLs[i] );
		hModule = LoadLibrary( dllPath );
		if ( !hModule && bDllRequired[i] )
		{
			Spew( "FATAL: Failed to load dll: '%s'\n", dllPath );
			return NULL;
		}
	}

	char cleanCommandLine[1024];
	char tempCommandLine[1024];
	StripCommandLineArg( pCmdLine, tempCommandLine, "-basedir" );
	StripCommandLineArg( tempCommandLine, cleanCommandLine, "-game" );

	// HACK: For ratings build, unlock everything. Remove this for later testing
	const char *pAdditionalArgs = "";
#if defined( RATINGSBUILD )
	pAdditionalArgs = "-dev -unlockchapters mp_mark_all_maps_complete";
#endif

	// set the alternate command line
	sprintf( pNewCommandLine, "%s -basedir %s -game %s\\%s %s", cleanCommandLine, pBasePath, pBasePath, gameName, pAdditionalArgs );

	// the 'main' export is guaranteed to be at ordinal 1
	// the library is already loaded, this just causes a lookup that will resolve against the shortname
	const char *pLaunchDllName = pImplicitDLLs[ARRAYSIZE( pImplicitDLLs )-1];
	hModule = LoadLibrary( pLaunchDllName );
	LauncherMain_t main = (LauncherMain_t)GetProcAddress( hModule, (LPSTR)1 );
	if ( !main )
	{
		Spew( "FATAL: 'LauncherMain' entry point not found in %s\n", pLaunchDllName );
		return NULL;
	}

	return main;
}

//-----------------------------------------------------------------------------
// 360 Application Entry Point.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
	char newCmdLine[1024];
	LauncherMain_t newMain = GetLaunchEntryPoint( newCmdLine );
	if ( newMain )
	{
		// 360 has no concept of instances, spoof one 
		newMain( (HINSTANCE)1, (HINSTANCE)0, (LPSTR)newCmdLine, 0 );
	}
}
#endif
