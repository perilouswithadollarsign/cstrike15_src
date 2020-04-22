//===== Copyright  1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

// fopen is needed to write steam_appid.txt
#undef fopen
#include <stdio.h>

#if defined(OSX) || defined(LINUX) || (defined (WIN32) && defined( DX_TO_GL_ABSTRACTION ))
	#include "appframework/ilaunchermgr.h"
#endif

#if defined( _WIN32 ) 
#if !defined( _X360 )
#include "winlite.h"
#endif
#elif defined(LINUX)
#elif defined( _PS3 )
#elif defined(OSX)
#include <Carbon/Carbon.h>
#else
#error
#endif
#include "quakedef.h"
#include "idedicatedexports.h"
#include "engine_launcher_api.h"
#include "ivideomode.h"
#include "common.h"
#include "iregistry.h"
#include "keys.h"
#include "cdll_engine_int.h"
#include "traceinit.h"
#include "iengine.h"
#include "igame.h"
#include "tier1/fmtstr.h"
#include "engine_hlds_api.h"
#include "filesystem_engine.h"
#include "tier0/icommandline.h"
#include "cl_main.h"
#include "client.h"
#include "tier3/tier3.h"
#include "MapReslistGenerator.h"
#include "toolframework/itoolframework.h"
#include "DevShotGenerator.h"
#include "gl_shader.h"
#include "l_studio.h"
#include "IHammer.h"
#ifdef _WIN32
#include "vgui/ILocalize.h"
#endif
#include "sys_dll.h"
#include "materialsystem/materialsystem_config.h"
#include "server.h"
#include "avi/iavi.h"
#include "avi/ibik.h"
#include "datacache/idatacache.h"
#include "vphysics_interface.h"
#include "inputsystem/iinputsystem.h"
#include "appframework/IAppSystemGroup.h"
#include "tier0/systeminformation.h"
#ifdef _WIN32
#include "VGuiMatSurface/IMatSystemSurface.h"
#endif
#include "steam/steam_api.h"

// This is here just for legacy support of older .dlls!!!
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "eiface.h"
#include "matchmaking/imatchframework.h"
#include "profile.h"
#include "status.h"

#include "vjobs_interface.h"

#ifndef DEDICATED
#include "sys_mainwind.h"
#include "vgui/ISystem.h"
#include "vgui_controls/Controls.h"
#include "IGameUIFuncs.h"
#include "cl_steamauth.h"

#endif // DEDICATED

#if defined( INCLUDE_SCALEFORM )
#include "scaleformui/scaleformui.h"
#endif

#if defined(_WIN32)
#include <eh.h>
#include <imm.h>
#endif

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#else
#include "xbox/xboxstubs.h"
#endif

#if defined( _PS3 ) && !defined( NO_STEAM )
#include "ps3_pathinfo.h"
#include "steam/steamps3params_internal.h"
SteamPS3ParamsInternal_t g_EngineSteamPS3ParamsInternal;
SteamPS3Params_t g_EngineSteamPS3Params;
#endif

#ifdef _PS3
#include <np.h>
#include "ps3_cstrike15/ps3_title_id.h"
#include "ps3/saverestore_ps3_api_ui.h"
#endif


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
IDedicatedExports *dedicated = NULL;
extern CreateInterfaceFn g_AppSystemFactory;
IHammer *g_pHammer = NULL;
IPhysics *g_pPhysics = NULL;
#if defined(OSX) || defined(LINUX) || (defined (WIN32) && defined( DX_TO_GL_ABSTRACTION ))
ILauncherMgr *g_pLauncherMgr = NULL;
#endif
IAvi *avi = NULL;
IBik *bik = NULL;
#ifdef _PS3
IPS3SaveRestoreToUI *ps3saveuiapi = NULL;
#endif
#if defined( INCLUDE_SCALEFORM )
IScaleformUI* g_pScaleformUI = NULL;
#endif

#ifndef DEDICATED
extern CreateInterfaceFn g_ClientFactory;
#endif

bool g_bRunningFromPerforce;
AppId_t g_unSteamAppID = k_uAppIdInvalid;

CSysModule *g_pMatchmakingDllModule = NULL;
CreateInterfaceFn g_pfnMatchmakingFactory = NULL;

IVJobs * g_pVJobs = NULL;

#ifdef ENGINE_MANAGES_VJOBS
CSysModule *g_pVjobsDllModule = NULL;
CreateInterfaceFn g_pfnVjobsFactory = NULL;
bool g_bVjobsReload = false;
bool g_bVjobsTest = false; // this is temporary, debug-only variable
#endif


IMatchFramework *g_pIfaceMatchFramework = NULL;
bool s_bIsDedicatedServer = false;

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
void Host_GetHostInfo(float *fps, int *nActive, int *nMaxPlayers, char *pszMap, int maxlen );
const char *Key_BindingForKey( ButtonCode_t code );
void COM_ShutdownFileSystem( void );
void COM_InitFilesystem( const char *pFullModPath );
void Host_ReadPreStartupConfiguration();
void EditorToggle_f();

#ifdef _WIN32
HWND *pmainwindow = NULL;
#elif OSX
WindowRef pmainwindow;
#elif LINUX
void *pmainwindow = NULL;
#elif defined( _PS3 )
void *pmainwindow = NULL;
#else
#error
#endif

//-----------------------------------------------------------------------------
// ConVars and console commands
//-----------------------------------------------------------------------------
#if !defined(DEDICATED)
static ConCommand editor_toggle( "editor_toggle", EditorToggle_f, "Disables the simulation and returns focus to the editor", FCVAR_CHEAT );
#endif



#ifndef DEDICATED
//-----------------------------------------------------------------------------
// Purpose: exports an interface that can be used by the launcher to run the engine
//			this is the exported function when compiled as a blob
//-----------------------------------------------------------------------------
void EXPORT F( IEngineAPI **api )
{
	CreateInterfaceFn factory = Sys_GetFactoryThis();	// This silly construction is necessary to prevent the LTCG compiler from crashing.
	*api = ( IEngineAPI * )(factory(VENGINE_LAUNCHER_API_VERSION, NULL));
}
#endif // DEDICATED

extern bool cs_initialized;
extern int			lowshift;
static char	*empty_string = "";

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
extern void SCR_UpdateScreen(void);
extern bool g_bMajorMapChange; 
extern bool g_bPrintingKeepAliveDots;

void Sys_ShowProgressTicks(char* specialProgressMsg)
{
#ifdef LATER
#define MAX_NUM_TICS 40

	static long numTics = 0;

	// Nothing to do if not using Steam
	if ( !g_pFileSystem->IsSteam() )
		return;

	// Update number of tics to show...
	numTics++;
	if ( isDedicated )
	{
		if ( g_bMajorMapChange )
		{
			g_bPrintingKeepAliveDots = TRUE;
			Msg(".");
		}
	}
	else
	{
		int i;
		int numTicsToPrint = numTics % (MAX_NUM_TICS-1);
		char msg[MAX_NUM_TICS+1];

		Q_strncpy(msg, ".", sizeof(msg));

		// Now add in the growing number of tics...
		for ( i = 1 ; i < numTicsToPrint ; i++ )
		{
			Q_strncat(msg, ".", sizeof(msg), COPY_ALL_CHARACTERS);
		}

		SCR_UpdateScreen();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClearIOStates( void )
{
#ifndef DEDICATED
	if ( g_ClientDLL ) 
	{
		g_ClientDLL->IN_ClearStates();
	}
#endif
}

void MoveConsoleWindowToFront()
{
#ifdef _WIN32
// TODO: remove me!!!!!

	// Move the window to the front.
	HINSTANCE hInst = LoadLibrary( "kernel32.dll" );
	if ( hInst )
	{
		typedef HWND (*GetConsoleWindowFn)();
		GetConsoleWindowFn fn = (GetConsoleWindowFn)GetProcAddress( hInst, "GetConsoleWindow" );
		if ( fn )
		{
			HWND hwnd = fn();
			ShowWindow( hwnd, SW_SHOW );
			UpdateWindow( hwnd );
			SetWindowPos( hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
		}
		FreeLibrary( hInst );
	}
#endif
}

#if defined( _WIN32 ) && !defined( _X360 )
#include <conio.h>
#endif
CUtlVector<char> g_TextModeLine;

char NextGetch()
{
	return -1;
	// NOTE: for some reason, kbhit() KILLS performance on the client.. when using it, the client
	// goes so slow that it's player's motion is all jerky. If we need input, probably the
	// best thing to do is to hook the console window's wndproc and get the keydown messages.
	/*
	// Sort of hacky to overload the gamemsg loop with these messages, but it does the trick.
	if ( VCRGetMode() == VCR_Playback )
	{
		unsigned int uMsg, wParam;
		long lParam;
		if ( VCRHook_PlaybackGameMsg( uMsg, wParam, lParam ) )
		{
			Assert( uMsg == 0xFFFF );
			return (char)wParam;
		}
		else
		{
			return -1;
		}
	}
	else
	{
		if ( kbhit() )
		{
			char ch = getch();
			VCRHook_RecordGameMsg( 0xFFFF, ch, 0 );
			return ch;
		}
		else
		{
			VCRHook_RecordEndGameMsg();
			return -1;
		}
	}
	*/
}

void EatTextModeKeyPresses()
{
	if ( !g_bTextMode )
		return;
	
	static bool bFirstRun = true;
	if ( bFirstRun )
	{
		bFirstRun = false;
		MoveConsoleWindowToFront();
	}

	char ch;
	while ( (ch = NextGetch()) != -1 )
	{
		if ( ch == 8 )
		{
			// Backspace..
			if ( g_TextModeLine.Count() )
			{
				g_TextModeLine.Remove( g_TextModeLine.Count() - 1 );
			}
		}
		else if ( ch == '\r' )
		{
			// Finish the line.
			if ( g_TextModeLine.Count() )
			{
				g_TextModeLine.AddMultipleToTail( 2, "\n" );
				Cbuf_AddText( Cbuf_GetCurrentPlayer(), g_TextModeLine.Base() );
				g_TextModeLine.Purge();
			}
			printf( "\n" );
		}
		else
		{
			g_TextModeLine.AddToTail( ch );
		}

		printf( "%c", ch );
	}	
}


//-----------------------------------------------------------------------------
// The SDK launches the game with the full path to gameinfo.txt, so we need
// to strip off the path.
//-----------------------------------------------------------------------------
const char *GetModDirFromPath( const char *pszPath )
{
	char *pszSlash = Q_strrchr( pszPath, '\\' );
	if ( pszSlash )
	{
		return pszSlash + 1;
	}
	else if ( ( pszSlash  = Q_strrchr( pszPath, '/' ) ) != NULL )
	{
		return pszSlash + 1;
	}

	// Must just be a mod directory already.
	return pszPath;
}

//-----------------------------------------------------------------------------
// Purpose: Main entry
//-----------------------------------------------------------------------------
#ifndef DEDICATED
#include "gl_matsysiface.h"
#endif

//-----------------------------------------------------------------------------
// Inner loop: initialize, shutdown main systems, load steam to 
//-----------------------------------------------------------------------------
class CModAppSystemGroup : public CAppSystemGroup
{
	typedef CAppSystemGroup BaseClass;
public:
	// constructor
	CModAppSystemGroup( bool bServerOnly, CAppSystemGroup *pParentAppSystem = NULL )
		: BaseClass( pParentAppSystem ),
		m_bServerOnly( bServerOnly )
	{
	}

	CreateInterfaceFn GetFactory()
	{
		return CAppSystemGroup::GetFactory();
	}

	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit();
	virtual int Main();
	virtual void PostShutdown();
	virtual void Destroy();

private:

	bool IsServerOnly() const
	{
		return m_bServerOnly;
	}
	bool ModuleAlreadyInList( CUtlVector< AppSystemInfo_t >& list, const char *moduleName, const char *interfaceName );

	bool AddLegacySystems();
	bool	m_bServerOnly;
};

#ifndef DEDICATED
//-----------------------------------------------------------------------------
//
// Main engine interface exposed to launcher
//
//-----------------------------------------------------------------------------
class CEngineAPI : public CTier3AppSystem< IEngineAPI >
{
	typedef CTier3AppSystem< IEngineAPI > BaseClass;

public:
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// This function must be called before init
	virtual bool SetStartupInfo( StartupInfo_t &info );

	virtual int Run( );

	// Sets the engine to run in a particular editor window
	virtual void SetEngineWindow( void *hWnd );

	// Posts a console command
	virtual void PostConsoleCommand( const char *pConsoleCommand );

	// Are we running the simulation?
	virtual bool IsRunningSimulation( ) const;

	// Start/stop running the simulation
	virtual void ActivateSimulation( bool bActive );

	// Reset the map we're on
	virtual void SetMap( const char *pMapName );

	bool MainLoop();

private:
	int RunListenServer();

	// Hooks a particular mod up to the registry
	void SetRegistryMod( const char *pModName );

	// One-time setup, based on the initially selected mod
	// FIXME: This should move into the launcher!
	bool OnStartup( void *pInstance, const char *pStartupModName );
	void OnShutdown();

	// Initialization, shutdown of a mod.
	bool ModInit( const char *pModName, const char *pGameDir );
	void ModShutdown();

	// Initializes, shuts down the registry
	bool InitRegistry( const char *pModName );
	void ShutdownRegistry();

	// Handles there being an error setting up the video mode
	InitReturnVal_t HandleSetModeError();

	// Purpose: Message pump when running stand-alone
	void PumpMessages();

	// Purpose: Message pump when running with the editor
	void PumpMessagesEditMode( bool &bIdle, long &lIdleCount );

	// Activate/deactivates edit mode shaders
	void ActivateEditModeShaders( bool bActive );

private:
	void *m_hEditorHWnd;
	bool m_bRunningSimulation;
	StartupInfo_t m_StartupInfo;
};


//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------
static CEngineAPI s_EngineAPI;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CEngineAPI, IEngineAPI, VENGINE_LAUNCHER_API_VERSION, s_EngineAPI );


//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CEngineAPI::Connect( CreateInterfaceFn factory ) 
{ 
	// Store off the app system factory...
	g_AppSystemFactory = factory;

	if ( !BaseClass::Connect( factory ) )
		return false;

	g_pFileSystem = g_pFullFileSystem;
	if ( !g_pFileSystem )
		return false;

#ifndef DBGFLAG_STRINGS_STRIP
	g_pFileSystem->SetWarningFunc( Warning );
#endif

	if ( !Shader_Connect( true ) )
		return false;

	g_pPhysics = (IPhysics*)factory( VPHYSICS_INTERFACE_VERSION, NULL );

	if( IsPS3() )
	{
		// only PS/3 uses vjobs.prx
		g_pVJobs = (IVJobs *)factory( VJOBS_INTERFACE_VERSION, NULL );
	}

	g_pSoundEmitterSystem = (ISoundEmitterSystemBase *)factory(SOUNDEMITTERSYSTEM_INTERFACE_VERSION, NULL);
	
#if defined( INCLUDE_SCALEFORM )
	g_pScaleformUI = ( IScaleformUI* ) factory( SCALEFORMUI_INTERFACE_VERSION, NULL );
#endif

	if ( IsPC() && !IsPosix() )
	{
		avi = (IAvi*)factory( AVI_INTERFACE_VERSION, NULL );
		if ( !avi )
			return false;
	}

#if ( !defined( _GAMECONSOLE ) || defined( BINK_ENABLED_FOR_CONSOLE ) ) && defined( BINK_VIDEO )
	bik = (IBik*)factory( BIK_INTERFACE_VERSION, NULL );
	if ( !bik )
		return false;
#endif

#ifdef _PS3
	ps3saveuiapi = (IPS3SaveRestoreToUI *)factory( IPS3SAVEUIAPI_VERSION_STRING, NULL );
	if ( !ps3saveuiapi )
		return false;
#endif

	if ( !g_pStudioRender || !g_pDataCache || !g_pPhysics || !g_pMDLCache || !g_pMatSystemSurface || !g_pInputSystem || !g_pSoundEmitterSystem)
	{
		Warning( "Engine wasn't able to acquire required interfaces!\n" );
		return false;
	}

	if (!g_pStudioRender)
	{
		Sys_Error( "Unable to init studio render system version %s\n", STUDIO_RENDER_INTERFACE_VERSION );
		return false;
	}

	g_pHammer = (IHammer*)factory( INTERFACEVERSION_HAMMER, NULL );

#if defined( USE_SDL )
	g_pLauncherMgr = (ILauncherMgr *)factory( SDLMGR_INTERFACE_VERSION, NULL );
#elif defined( OSX )
	g_pLauncherMgr = (ILauncherMgr *)factory( COCOAMGR_INTERFACE_VERSION, NULL );
#endif
	
	ConnectMDLCacheNotify();

	return true; 
}

void CEngineAPI::Disconnect() 
{
	DisconnectMDLCacheNotify();

	g_pHammer = NULL;
	g_pPhysics = NULL;
	g_pSoundEmitterSystem = NULL;

	Shader_Disconnect();

	g_pFileSystem = NULL;

	BaseClass::Disconnect();

	g_AppSystemFactory = NULL;
}


//-----------------------------------------------------------------------------
// Query interface
//-----------------------------------------------------------------------------
void *CEngineAPI::QueryInterface( const char *pInterfaceName )
{
	// Loading the engine DLL mounts *all* engine interfaces
	CreateInterfaceFn factory = Sys_GetFactoryThis();	// This silly construction is necessary
	return factory( pInterfaceName, NULL );				// to prevent the LTCG compiler from crashing.
}


static bool WantsFullMemoryDumps()
{
#if defined( _WIN32 )
	return CommandLine()->FindParm( "-full_memory_dumps" ) ? true : false;
#else
	return false;
#endif
}


//-----------------------------------------------------------------------------
// Sets startup info
//-----------------------------------------------------------------------------
bool CEngineAPI::SetStartupInfo( StartupInfo_t &info ) 
{
	g_bTextMode = info.m_bTextMode;

	// Set up the engineparms_t which contains global information about the mod
	host_parms.basedir = const_cast<char*>( info.m_pBaseDirectory );

	// Copy off all the startup info
	m_StartupInfo = info;

#if defined( _PS3 ) && !defined( NO_STEAM )
	{
		bool bPublicUniverse = !CommandLine()->FindParm( "-steamBeta" );
		
		//////// DISABLE FOR SHIP! //////////
		// BETA:   bPublicUniverse = !!CommandLine()->FindParm( "-steamPublic" ); // default=beta; require -steamPublic to run against public
		// PUBLIC: bPublicUniverse = !CommandLine()->FindParm( "-steamBeta" ); // default=public; require -steamBeta to run against beta
		
		if ( bPublicUniverse )
		{
			Msg( "Connecting to Steam Public\n" );
			g_EngineSteamPS3Params.m_nAppId = 710;
		}
		else
		{
			Msg( "Connecting to Steam Beta\n" );
			g_EngineSteamPS3Params.m_nAppId = 710;
			g_EngineSteamPS3ParamsInternal.m_nVersion = STEAM_PS3_PARAMS_INTERNAL_VERSION;
			g_EngineSteamPS3ParamsInternal.m_eUniverse = k_EUniverseBeta;
			g_EngineSteamPS3ParamsInternal.m_pchCMForce = "";
			g_EngineSteamPS3ParamsInternal.m_bAutoReloadVGUIResources = false;
			g_EngineSteamPS3Params.pReserved = &g_EngineSteamPS3ParamsInternal;
		}

		if ( int nSteamTTY = CommandLine()->ParmValue( "-steamTTY", int(0) ) )
			g_EngineSteamPS3Params.m_cSteamInputTTY = nSteamTTY;

		g_EngineSteamPS3Params.m_unVersion = STEAM_PS3_CURRENT_PARAMS_VER;
		Q_strncpy( g_EngineSteamPS3Params.m_rgchInstallationPath, g_pPS3PathInfo->PrxPath(), sizeof( g_EngineSteamPS3Params.m_rgchInstallationPath ) );
		Q_strncpy( g_EngineSteamPS3Params.m_rgchSystemCache, g_pPS3PathInfo->SystemCachePath(), sizeof( g_EngineSteamPS3Params.m_rgchSystemCache ) );
		Q_strncpy( g_EngineSteamPS3Params.m_rgchGameData, g_pPS3PathInfo->SystemCachePath(), sizeof( g_EngineSteamPS3Params.m_rgchGameData ) );
		Q_strncpy( g_EngineSteamPS3Params.m_rgchNpServiceID, PS3_GAME_SERVICE_ID, STEAM_PS3_SERVICE_ID_MAX );
		Q_strncpy( g_EngineSteamPS3Params.m_rgchNpCommunicationID, PS3_GAME_COMMUNICATION_ID, STEAM_PS3_COMMUNICATION_ID_MAX );
		const SceNpCommunicationSignature npcommsign = PS3_GAME_COMMUNICATION_SIGNATURE;
		Q_memcpy( g_EngineSteamPS3Params.m_rgchNpCommunicationSig, &npcommsign, STEAM_PS3_COMMUNICATION_SIG_MAX );
		Q_strncpy( g_EngineSteamPS3Params.m_rgchSteamLanguage, XBX_GetLanguageString(), STEAM_PS3_LANGUAGE_MAX );
		
		if ( !V_stricmp( g_pPS3PathInfo->GetParamSFO_TitleID(), PS3_GAME_TITLE_ID_WW_SCEE ) )
			Q_strncpy( g_EngineSteamPS3Params.m_rgchRegionCode, "SCEE", STEAM_PS3_REGION_CODE_MAX );
		else if ( !V_stricmp( g_pPS3PathInfo->GetParamSFO_TitleID(), PS3_GAME_TITLE_ID_WW_SCEJ ) )
			Q_strncpy( g_EngineSteamPS3Params.m_rgchRegionCode, "SCEJ", STEAM_PS3_REGION_CODE_MAX );
		else
			Q_strncpy( g_EngineSteamPS3Params.m_rgchRegionCode, "SCEA", STEAM_PS3_REGION_CODE_MAX );

		MEM_ALLOC_CREDIT_( "STEAM: g_EngineSteamPS3Params.m_sysNetInitInfo" );
		g_EngineSteamPS3Params.m_sysNetInitInfo.m_bNeedInit = true;		
		g_EngineSteamPS3Params.m_sysNetInitInfo.m_nMemorySize = 512 * 1024;
		g_EngineSteamPS3Params.m_sysNetInitInfo.m_pMemory = malloc( g_EngineSteamPS3Params.m_sysNetInitInfo.m_nMemorySize );
		g_EngineSteamPS3Params.m_sysSysUtilUserInfo.m_bNeedInit = true;

		g_EngineSteamPS3Params.m_sysJpgInitInfo.m_bNeedInit = true;
		g_EngineSteamPS3Params.m_sysPngInitInfo.m_bNeedInit = true;
		g_EngineSteamPS3Params.m_bIncludeNewsPage = false;
#if defined(NO_STEAM_PS3_OVERLAY)
		g_EngineSteamPS3Params.m_bPersonaStateOffline = true;
#else
		g_EngineSteamPS3Params.m_bPersonaStateOffline = false;
#endif
	}
#endif

	// Needs to be done prior to init material system config
	TRACEINIT( COM_InitFilesystem( m_StartupInfo.m_pInitialMod ), COM_ShutdownFileSystem() );

	//
	// VPK content-shadowing overrides
	// See below:
	//		CDedicatedServerAPI::ModInit
	//	to ensure that dedicated servers also mount the VPKs to check clients CRCs
	//

	// This is the client initializing, if we are running in lowviolence mode then inject the lowviolence VPK at the head of search path
	if ( CommandLine()->FindParm( "-pakxv_lowviolence" ) )
	{
		g_pFullFileSystem->AddSearchPath( "lowviolence", "COMPAT:GAME", PATH_ADD_TO_HEAD );
	}

	if ( CommandLine()->FindParm( "-perfectworld" ) )
	{
		g_pFullFileSystem->AddSearchPath( "perfectworld", "COMPAT:GAME", PATH_ADD_TO_HEAD );
	}

	// Enable file tracking - client always does this in case it connects to a pure server.
	// server only does this if sv_pure is set
	if ( IsPC() )
	{
		KeyValues *modinfo = new KeyValues("ModInfo");
		if ( modinfo->LoadFromFile( g_pFileSystem, "gameinfo.txt" ) )
		{
			// If it's not singleplayer_only
			if ( V_stricmp( modinfo->GetString("type", "singleplayer_only"), "singleplayer_only") == 0 )
			{
				DevMsg( "Disabling whitelist file tracking in filesystem...\n" );
				g_pFileSystem->EnableWhitelistFileTracking( false, false, false );
			}
			else
			{
				DevMsg( "Enabling whitelist file tracking in filesystem...\n" );
				g_pFileSystem->EnableWhitelistFileTracking( true, false, false );
			}
		}
		modinfo->deleteThis();
	}

	//
	// Configure breakpad
	//

	// Parse AppID from steam.inf file
	extern void Sys_Version( bool bDedicated );
	Sys_Version( false );

#if !defined( NO_STEAM ) && !defined( _GAMECONSOLE )
	if ( !CommandLine()->FindParm( "-nobreakpad" ) )
	{
		// AppID of the client will be automatically used
		extern int32 GetHostVersion();
		extern int32 GetClientVersion();
		CFmtStr fmtClientVersion( "%d.%d", GetHostVersion(), GetClientVersion() );
		Msg( "Using breakpad minidump system %u/%s\n", g_unSteamAppID, fmtClientVersion.Access() );
		SteamAPI_UseBreakpadCrashHandler( fmtClientVersion.Access(), __DATE__, __TIME__, WantsFullMemoryDumps(), NULL, NULL );
	}
#endif // !NO_STEAM && !_GAMECONSOLE

	// turn on the Steam3 API early so we can query app data up front
#if !defined( DEDICATED ) && !defined( NO_STEAM )
	TRACEINIT( Steam3Client().Activate(), Steam3Client().Shutdown() );
	if ( IsPS3() )
	{
#if !defined( CSTRIKE15 )
		// TODO: PS3_BUILDFIX: We probably want to turn this back on after we get the steam client working for CStrike15
		// this is only relevant for PS3
		if ( !Steam3Client().IsInitialized() )
		{
			return false;
		}
#endif // CSTRIKE15
	}

	if ( !Steam3Client().IsInitialized() || !Steam3Client().SteamUser() ||
		!Steam3Client().SteamUser()->GetSteamID().IsValid() || !Steam3Client().SteamUser()->GetSteamID().BIndividualAccount() || !Steam3Client().SteamUser()->GetSteamID().GetAccountID() )
	{
		Error( "FATAL ERROR: Failed to connect with local Steam Client process!\n\nPlease make sure that you are running latest version of Steam Client.\nYou can check for Steam Client updates using Steam main menu:\n             Steam > Check for Steam Client Updates..." );
		return false;
	}

	//
	// Setup a search path for USRLOCAL data (configs / save games / etc.) that isn't intended to be shared across multiple accounts
	//
	if ( g_pFileSystem )
	{
		char chUserLocalDataFolder[ MAX_PATH ] = {};
		if ( char const * pszLocalOverride = getenv( "USRLOCAL" DLLExtTokenPaste2( VPCGAMECAPS ) ) )
		{
			Msg( "USRLOCAL path using environment setting '%s':\n%s\n", "USRLOCAL" DLLExtTokenPaste2( VPCGAMECAPS ), pszLocalOverride );
			g_pFileSystem->AddSearchPath( pszLocalOverride, "USRLOCAL" );
		}
		else if ( Steam3Client().SteamUser()->GetUserDataFolder( chUserLocalDataFolder, sizeof( chUserLocalDataFolder ) ) )
		{
			Msg( "USRLOCAL path using Steam profile data folder:\n%s\n", chUserLocalDataFolder );
			g_pFileSystem->AddSearchPath( chUserLocalDataFolder, "USRLOCAL" );
		}
		else
		{
			Warning( "USRLOCAL path not found!\n" );
		}
	}
#endif

	return true;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
InitReturnVal_t CEngineAPI::Init() 
{
	if ( CommandLine()->FindParm( "-sv_benchmark" ) != 0 )
	{
		Plat_SetBenchmarkMode( true );
	}

	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	m_bRunningSimulation = false;

	// Initialize the FPU control word
#if !defined( DEDICATED ) && !defined( _X360 ) && !defined( _PS3 ) && !defined( PLATFORM_64BITS ) && !defined( LINUX ) && !defined(__clang__)
	_asm
	{
		fninit
	}
#endif

	SetupFPUControlWord();

	// This creates the videomode singleton object, it doesn't depend on the registry
	VideoMode_Create();

	// Initialize the editor hwnd to render into
	m_hEditorHWnd = NULL;

	// One-time setup
	// FIXME: OnStartup + OnShutdown should be removed + moved into the launcher
	// or the launcher code should be merged into the engine into the code in OnStartup/OnShutdown
	if ( !OnStartup( m_StartupInfo.m_pInstance, m_StartupInfo.m_pInitialMod ) )
	{
		return HandleSetModeError();
	}

#if defined( POSIX ) && !defined( _PS3 )
    // on OSX by the time we've initialized cl_language we've already initialized the 
    // font manager and made a bunch of language-related decisions.
    // on windows, the code sniffs the registry directly (slightly evil) and doesn't have
    // this problem.
    if ( Steam3Client().SteamApps() )
    {
        extern ConVar cl_language;
        cl_language.SetValue( Steam3Client().SteamApps()->GetCurrentGameLanguage() );
    }
#endif
	return INIT_OK; 
}

void CEngineAPI::Shutdown() 
{
	VideoMode_Destroy();
	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Sets the engine to run in a particular editor window
//-----------------------------------------------------------------------------
void CEngineAPI::SetEngineWindow( void *hWnd )
{
	if ( !InEditMode() )
		return;

	// Detach input from the previous editor window
	game->InputDetachFromGameWindow();

	m_hEditorHWnd = hWnd;
	videomode->SetGameWindow( m_hEditorHWnd );
}


//-----------------------------------------------------------------------------
// Posts a console command
//-----------------------------------------------------------------------------
void CEngineAPI::PostConsoleCommand( const char *pCommand )
{
	Assert( 0 ); // This isn't being used, but I'm assuming it would be for a dedicated server type console, so issueing the command into the server's buffer may make sense.
	Cbuf_AddText( CBUF_SERVER, pCommand );
}

	
//-----------------------------------------------------------------------------
// Is the engine currently rinning?
//-----------------------------------------------------------------------------
bool CEngineAPI::IsRunningSimulation() const
{
	return (eng->GetState() == IEngine::DLL_ACTIVE);
}


//-----------------------------------------------------------------------------
// Reset the map we're on
//-----------------------------------------------------------------------------
void CEngineAPI::SetMap( const char *pMapName )
{
//	if ( !Q_stricmp( sv.mapname, pMapName ) )
//		return;

	char buf[MAX_PATH];
	Q_snprintf( buf, MAX_PATH, "map %s", pMapName );
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), buf );
}


//-----------------------------------------------------------------------------
// Start/stop running the simulation
//-----------------------------------------------------------------------------
void CEngineAPI::ActivateSimulation( bool bActive )
{
	// FIXME: Not sure what will happen in this case
	if ( ( eng->GetState() != IEngine::DLL_ACTIVE )	&&
		 ( eng->GetState() != IEngine::DLL_PAUSED ) )
	{
		return;
	}

	bool bCurrentlyActive = (eng->GetState() != IEngine::DLL_PAUSED);
	if ( bActive == bCurrentlyActive )
		return;

	// FIXME: Should attachment/detachment be part of the state machine in IEngine?
	if ( !bActive )
	{
		eng->SetNextState( IEngine::DLL_PAUSED );

		// Detach input from the previous editor window
		game->InputDetachFromGameWindow();
	}
	else
	{
		eng->SetNextState( IEngine::DLL_ACTIVE );

		// Start accepting input from the new window
		// FIXME: What if the attachment fails?
		game->InputAttachToGameWindow();
	}
}

	
//-----------------------------------------------------------------------------
// Purpose: Message pump when running stand-alone
//-----------------------------------------------------------------------------
void CEngineAPI::PumpMessages()
{
#if defined( WIN32 ) && !defined( USE_SDL )
	MSG msg;
	while ( PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE ) )
	{
#if defined( INCLUDE_SCALEFORM )
		if ( g_pScaleformUI )
		{
			// Scaleform IME requirement. Pass these messages to GFxIME BEFORE any TranlsateMessage/DispatchMessage.
			if ( (msg.message == WM_KEYDOWN) || (msg.message == WM_KEYUP) || ImmIsUIMessage( NULL, msg.message, msg.wParam, msg.lParam ) 
				|| (msg.message == WM_LBUTTONDOWN) || (msg.message == WM_LBUTTONUP) )
			{
				g_pScaleformUI->PreProcessKeyboardEvent( (size_t)msg.hwnd, msg.message, msg.wParam, msg.lParam );
			}
		}
#endif

		TranslateMessage( &msg );
		DispatchMessageW( &msg );

	}
#elif defined( OSX ) || defined( USE_SDL )
	g_pLauncherMgr->PumpWindowsMessageLoop();
#else
#error
#endif

	// Get input from attached devices
	g_pInputSystem->PollInputState( GetBaseLocalClient().IsActive() );

	// NOTE: Under some implementations of Win9x, 
	// dispatching messages can cause the FPU control word to change
	if ( IsPC() )
	{
		SetupFPUControlWord();
	}

	game->DispatchAllStoredGameMessages();

	if ( IsPC() )
	{
		EatTextModeKeyPresses();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Message pump when running stand-alone
//-----------------------------------------------------------------------------
void CEngineAPI::PumpMessagesEditMode( bool &bIdle, long &lIdleCount )
{

	if ( bIdle && !g_pHammer->HammerOnIdle( lIdleCount++ ) )
	{
		bIdle = false;
	}

#ifdef WIN32
	MSG msg;
	while ( PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE) )
	{
		if ( msg.message == WM_QUIT )
		{
			eng->SetQuitting( IEngine::QUIT_TODESKTOP );
			break;
		}

		if ( msg.hwnd == (HWND)game->GetMainWindow() )
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		else
		{
			if ( !g_pHammer->HammerPreTranslateMessage(&msg) )
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}

		// Reset idle state after pumping idle message.
		if ( g_pHammer->HammerIsIdleMessage(&msg) )
		{
			bIdle = true;
			lIdleCount = 0;
		}
	}
#elif defined( OSX ) && defined( PLATFORM_64BITS )
	// Do nothing, but let someone know we're doing nothing.
	Assert( !"OSX-64 not implemented." );

#elif defined( OSX )
	EventRef theEvent;
	EventTargetRef theTarget;
	EventTime eventTimeout = kEventDurationNoWait;
	
	theTarget = GetEventDispatcherTarget();
    while ( ReceiveNextEvent( 0, NULL, eventTimeout, true, &theEvent ) == noErr)		
	{
		OSErr ret = SendEventToEventTarget (theEvent, theTarget);
		if ( ret != noErr )
		{
			EventRecord clevent;
			ConvertEventRefToEventRecord( theEvent, &clevent);
			if ( clevent.what==kHighLevelEvent ) 
			{
				AEProcessAppleEvent( &clevent );
			}
		}
	 
		ReleaseEvent(theEvent);
	}
#elif defined( _PS3 )
#elif defined( LINUX )
#else
#error
#endif


	// NOTE: Under some implementations of Win9x, 
	// dispatching messages can cause the FPU control word to change
	SetupFPUControlWord();

	game->DispatchAllStoredGameMessages();
}

//-----------------------------------------------------------------------------
// Activate/deactivates edit mode shaders
//-----------------------------------------------------------------------------
void CEngineAPI::ActivateEditModeShaders( bool bActive )
{
	if ( InEditMode() && ( g_pMaterialSystemConfig->bEditMode != bActive ) )
	{
		MaterialSystem_Config_t config = *g_pMaterialSystemConfig;
		config.bEditMode = bActive;
		OverrideMaterialSystemConfig( config );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Message pump
//-----------------------------------------------------------------------------
bool CEngineAPI::MainLoop()
{
	bool bIdle = true;
	long lIdleCount = 0;

	// Main message pump
	while ( true )
	{
		// Pump messages unless someone wants to quit
		if ( eng->GetQuitting() != IEngine::QUIT_NOTQUITTING )
		{
			if ( eng->GetQuitting() != IEngine::QUIT_TODESKTOP )
				return true;
			return false;
		}

		// Pump the message loop
		if ( !InEditMode() )
		{
			PumpMessages();
		}
		else
		{
			PumpMessagesEditMode( bIdle, lIdleCount );
		}

		// Deactivate edit mode shaders
		ActivateEditModeShaders( false );

		eng->Frame();

		// Reactivate edit mode shaders (in Edit mode only...)
		ActivateEditModeShaders( true );

		if ( InEditMode() )
		{
			g_pHammer->RunFrame();
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Initializes, shuts down the registry
//-----------------------------------------------------------------------------
bool CEngineAPI::InitRegistry( const char *pModName )
{
	if ( IsPC() )
	{
		char szRegSubPath[MAX_PATH];
		Q_snprintf( szRegSubPath, sizeof(szRegSubPath), "%s\\%s", "Source", pModName );
		return registry->Init( szRegSubPath );
	}
	return true;
}

void CEngineAPI::ShutdownRegistry( )
{
	if ( IsPC() )
	{
		registry->Shutdown( );
	}
}

#if defined( _PS3 )
int PS3_WindowProc_Proxy( xevent_t const &ev );
#endif

//-----------------------------------------------------------------------------
// One-time setup, based on the initially selected mod
// FIXME: This should move into the launcher!
//-----------------------------------------------------------------------------
bool CEngineAPI::OnStartup( void *pInstance, const char *pStartupModName )
{
	// This fixes a bug on certain machines where the input will 
	// stop coming in for about 1 second when someone hits a key.
	// (true means to disable priority boost)
#ifdef WIN32
	if ( IsPC() )
	{
		SetThreadPriorityBoost( GetCurrentThread(), true ); 
	}
#endif

	// FIXME: Turn videomode + game into IAppSystems?

	// Try to create the window
	COM_TimestampedLog( "game->Init" );

	splitscreen->Init();

	// This has to happen before CreateGameWindow to set up the instance
	// for use by the code that creates the window
	if ( !game->Init( pInstance ) )
	{
		goto onStartupError;
	}

	// Try to create the window
	COM_TimestampedLog( "videomode->Init" );

	// This needs to be after Shader_Init and registry->Init
	// This way mods can have different default video settings
	if ( !videomode->Init( ) )
	{
		goto onStartupShutdownGame;
	}

	COM_TimestampedLog( "InitRegistry" );
	
	// We need to access the registry to get various settings (specifically,
	// InitMaterialSystemConfig requires it).
	if ( !InitRegistry( pStartupModName ) )
	{
		goto onStartupShutdownVideoMode;
	}

	COM_TimestampedLog( "materials->ModInit" );

	materials->ModInit();

	COM_TimestampedLog( "InitMaterialSystemConfig" );

	// Setup the material system config record, CreateGameWindow depends on it
	// (when we're running stand-alone)
	InitMaterialSystemConfig( InEditMode() );
	
	{
#if defined( _X360 )
		XBX_NotifyCreateListener( XNOTIFY_ALL );
#elif defined( _PS3 )
		ps3syscbckeventhdlr_t hdlr = { PS3_WindowProc_Proxy };
		XBX_NotifyCreateListener( reinterpret_cast< uint64 >( &hdlr ) );
#endif
	}

	COM_TimestampedLog( "ShutdownRegistry" );

	ShutdownRegistry();
	return true;

	// Various error conditions
onStartupShutdownVideoMode:
	videomode->Shutdown();

onStartupShutdownGame:
	game->Shutdown();

onStartupError:
	return false;
}


//-----------------------------------------------------------------------------
// One-time shutdown (shuts down stuff set up in OnStartup)
// FIXME: This should move into the launcher!
//-----------------------------------------------------------------------------
void CEngineAPI::OnShutdown()
{
	if ( videomode )
	{
		videomode->Shutdown();
	}

	// Shut down the game
	game->Shutdown();

	materials->ModShutdown();
	TRACESHUTDOWN( COM_ShutdownFileSystem() );

	splitscreen->Shutdown();

#ifdef _PS3
	XBX_NotifyCreateListener( 0 );
#endif
}

static bool IsValveMod( const char *pModName )
{
	// Figure out if we're running a Valve mod or not.
	return ( Q_stricmp( pModName, "cstrike" ) == 0 ||
		Q_stricmp( pModName, "dod" ) == 0 ||
		Q_stricmp( pModName, "hl1mp" ) == 0 ||
		Q_stricmp( pModName, "tf" ) == 0 ||
		Q_stricmp( pModName, "hl2mp" ) == 0 ||
		Q_stricmp( pModName, "csgo" ) == 0 );
}

//-----------------------------------------------------------------------------
// Initialization, shutdown of a mod.
//-----------------------------------------------------------------------------
bool CEngineAPI::ModInit( const char *pModName, const char *pGameDir )
{
	COM_TimestampedLog( "ModInit" );

	// Set up the engineparms_t which contains global information about the mod
	host_parms.mod = COM_StringCopy( GetModDirFromPath( pModName ) );
	host_parms.game = COM_StringCopy( pGameDir );

	// By default, restrict server commands in Valve games and don't restrict them in mods.
	bool bRestrictCommands = IsValveMod( host_parms.mod );
	GetBaseLocalClient().m_bRestrictServerCommands = bRestrictCommands;
	GetBaseLocalClient().m_bRestrictClientCommands = bRestrictCommands;

	// build the registry path we're going to use for this mod
	InitRegistry( pModName );

	// This sets up the game search path, depends on host_parms
	TRACEINIT( MapReslistGenerator_Init(), MapReslistGenerator_Shutdown() );
#if !defined( _X360 )
	TRACEINIT( DevShotGenerator_Init(), DevShotGenerator_Shutdown() );
#endif

	COM_TimestampedLog( "Host_ReadPreStartupConfiguration - Start" );

	// Slam cvars based on mod/config.cfg
	Host_ReadPreStartupConfiguration();

	COM_TimestampedLog( "Host_ReadPreStartupConfiguration - Finish" );
	// Create the game window now that we have a search path
	// FIXME: Deal with initial window width + height better
	if ( !videomode || !videomode->CreateGameWindow( g_pMaterialSystemConfig->m_VideoMode.m_Width, g_pMaterialSystemConfig->m_VideoMode.m_Height, g_pMaterialSystemConfig->Windowed(), g_pMaterialSystemConfig->NoWindowBorder() ) )
	{
		return false;
	}

	return true;
}

void CEngineAPI::ModShutdown()
{
	COM_StringFree(host_parms.mod);
	COM_StringFree(host_parms.game);
	
	// Stop accepting input from the window
	game->InputDetachFromGameWindow();

#if !defined( _X360 )
	TRACESHUTDOWN( DevShotGenerator_Shutdown() );
#endif
	TRACESHUTDOWN( MapReslistGenerator_Shutdown() );

	ShutdownRegistry();
}


//-----------------------------------------------------------------------------
// Purpose: Handles there being an error setting up the video mode
// Output : Returns true on if the engine should restart, false if it should quit
//-----------------------------------------------------------------------------
InitReturnVal_t CEngineAPI::HandleSetModeError()
{
	// show an error, see if the user wants to restart
	if ( CommandLine()->FindParm( "-safe" ) )
	{
		Sys_MessageBox( "Failed to set video mode.\n\nThis game has a minimum requirement of DirectX 7.0 compatible hardware.\n", "Video mode error", false );
		return INIT_FAILED;
	}
	
	if ( CommandLine()->FindParm( "-autoconfig" ) )
	{
		if ( Sys_MessageBox( "Failed to set video mode - falling back to safe mode settings.\n\nGame will now restart with the new video settings.", "Video - safe mode fallback", true ))
		{
			CommandLine()->AppendParm( "-safe", NULL );
			return (InitReturnVal_t)INIT_RESTART;
		}
		return INIT_FAILED;
	}

	if ( Sys_MessageBox( "Failed to set video mode - resetting to defaults.\n\nGame will now restart with the new video settings.", "Video mode warning", true ) )
	{
		CommandLine()->AppendParm( "-autoconfig", NULL );
		return (InitReturnVal_t)INIT_RESTART;
	}

	return INIT_FAILED;
}

//-----------------------------------------------------------------------------
// Purpose: Main loop for non-dedicated servers
//-----------------------------------------------------------------------------
int CEngineAPI::RunListenServer()
{
	//
	// NOTE: Systems set up here should depend on the mod 
	// Systems which are mod-independent should be set up in the launcher or Init()
	//

	// Innocent until proven guilty
	int nRunResult = RUN_OK;

	// Happens every time we start up and shut down a mod
	if ( ModInit( m_StartupInfo.m_pInitialMod, m_StartupInfo.m_pInitialGame ) )
	{
		CModAppSystemGroup modAppSystemGroup( false, m_StartupInfo.m_pParentAppSystemGroup );

#ifdef USE_HACKY_MATERIAL_SYSTEM_TEST
		RunMaterialSystemTest();
#endif

		// Store off the app system factory...
		g_AppSystemFactory = modAppSystemGroup.GetFactory();

		nRunResult = modAppSystemGroup.Run();

		g_AppSystemFactory = NULL;

		// Shuts down the mod
		ModShutdown();

		// Disconnects from the editor window
		videomode->SetGameWindow( NULL );
	}

	// Closes down things that were set up in OnStartup
	// FIXME: OnStartup + OnShutdown should be removed + moved into the launcher
	// or the launcher code should be merged into the engine into the code in OnStartup/OnShutdown
	OnShutdown();

	return nRunResult;
}

#if 0
CON_COMMAND( bigalloc, "huge alloc crash" )
{
	Msg( "pre-crash %d\n", g_pMemAlloc->MemoryAllocFailed() );
	void *buf = malloc( UINT_MAX );
	Msg( "post-alloc %d\n", g_pMemAlloc->MemoryAllocFailed() );
	*(int *)buf = 0;
}
#endif

#if defined( _PS3 ) && !defined(NO_STEAM) && !defined(_CERT)
CON_COMMAND_F( steam_login_new_acct, "logs in and creates a new account if necessary", FCVAR_DEVELOPMENTONLY )
{
	Steam3Client().SteamUser()->LogOnAndCreateNewSteamAccountIfNeeded( false );
}

CON_COMMAND_F( steam_login_link_acct, "<username> <password>", FCVAR_DEVELOPMENTONLY )
{
	if ( args.ArgC() != 3 )
		return;

	Steam3Client().SteamUser()->LogOnAndLinkSteamAccountToPSN( false, args[1], args[2] );
}

CON_COMMAND_F( steam_login, "log into steam with an already linked account", FCVAR_DEVELOPMENTONLY )
{
	Steam3Client().SteamUser()->LogOn( false );
}
#endif

CON_COMMAND( reload_vjobs, "reload vjobs module" )
{
	const char * pModuleName = "vjobs" DLL_EXT_STRING;
	extern CAppSystemGroup *s_pCurrentAppSystem;
	if( g_pVJobs )
	{
		MaterialLock_t matlock = materials->Lock();
		g_pVJobs->BeforeReload();
		s_pCurrentAppSystem->ReloadModule( pModuleName );
		g_pVJobs->AfterReload();
		materials->Unlock( matlock );
	}
	else
	{
		Warning("vjobs interface not connected\n");
	}		
}

CON_COMMAND( render_blanks, "render N blank frames" )
{
	uint nFrames = 0;
	if( *args[1] )
		nFrames = atoi( args[1] );
	if( !nFrames || nFrames > 1000 )
	{
		nFrames = 10;
		Msg("Clamping to %d frames", nFrames );
	}
	MaterialLock_t matlock = materials->Lock();
	materials->SpinPresent( nFrames );
	materials->Unlock( matlock );	
}


extern void S_ClearBuffer();
#endif // #ifndef DEDICATED

extern bool g_bUpdateMinidumpComment;
void GetSpew( char *buf, size_t buflen );

#if defined( _X360 )
#define DUMP_COMMENT_SIZE 3500
#else
// should not exceed 32K, since current breakpad minidump reading code limits total comment size to 32k.
#define DUMP_COMMENT_SIZE 32768
#endif

// Turn this to 1 to allow for > 3kb of comments in dumps
static ConVar sys_minidumpexpandedspew( "sys_minidumpexpandedspew", "0" );

extern "C" void __cdecl FailSafe( unsigned int uStructuredExceptionCode, struct _EXCEPTION_POINTERS * pExceptionInfo	)
{
	// Nothing, this just catches a crash when creating the comment block
}

class CErrorText
{
public:
	explicit CErrorText( int size ) : 
		m_Size( size ),
		m_errorText( new char[ size ] )
	{
		Q_memset( m_errorText, 0x00, m_Size );
	}

	~CErrorText()
	{
		delete[] m_errorText;
	}

	void BuildComment( char const *pchSysErrorText )
	{
#if !defined( _PS3 )
#ifdef IS_WINDOWS_PC
		// This warning is not actually true in this context.
#pragma warning( suppress : 4535 ) // warning C4535: calling _set_se_translator() requires /EHa
		_se_translator_function curfilter = _set_se_translator( &FailSafe );
#endif

		try 
		{
			int nSize = m_Size;
			nSize = MIN( nSize, DUMP_COMMENT_SIZE );

			Q_memset( m_errorText, 0x00, nSize );

			if ( pchSysErrorText )
			{
				// Strip trailing return character (note this overwrites the return character built into the string)
				char *pchFixup = (char *)pchSysErrorText;
				int lenFixed = Q_strlen( pchFixup );
				if ( pchFixup[ lenFixed - 1 ] == '\n' )
				{
					pchFixup[ lenFixed - 1 ] = 0;
				}
				V_snprintf( m_errorText, nSize, "\nSys_Error( %s )\n", pchFixup );
			}
			else
			{
				V_snprintf( m_errorText, nSize, "\nCrash\n" );
			}

			V_strncat( m_errorText, Status_GetBuffer(), nSize );

			// Latch in case below stuff crashes
#if !defined( NO_STEAM )
			SteamAPI_SetMiniDumpComment( m_errorText );
#endif

			bool bExtendedSpew = sys_minidumpexpandedspew.GetBool();
			if ( bExtendedSpew )
			{
				try
				{
					
					V_strncat( m_errorText, "\nConVars (non-default)\n\n", nSize );
					char header[ 128 ];
					V_snprintf( header, sizeof( header ), "%25.25s %25.25s %25.25s\n", "var", "value", "default" );
					V_strncat( m_errorText, header, nSize );

					int len = V_strlen( m_errorText );
					if ( len < nSize )
					{
						int remainder = nSize - len;
						char *pbuf = m_errorText + len;

						ICvar::Iterator iter( g_pCVar );
						for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
						{
							ConCommandBase *var = iter.Get();
							if ( var->IsCommand() )
								continue;

							const ConVar *cvar = ( const ConVar * )var;
							if ( cvar->GetFlags() & ( FCVAR_SERVER_CANNOT_QUERY | FCVAR_PROTECTED ) )
								continue;

							if ( !( cvar->GetFlags() & FCVAR_NEVER_AS_STRING ) )
							{
								char var1[ MAX_OSPATH ];
								char var2[ MAX_OSPATH ];

								Q_strncpy( var1, Host_CleanupConVarStringValue( cvar->GetString() ), sizeof( var1 ) );
								Q_strncpy( var2, Host_CleanupConVarStringValue( cvar->GetDefault() ), sizeof( var2 ) );

								if ( !Q_stricmp( var1, var2 ) )
									continue;
							}
							else
							{
								if ( cvar->GetFloat() == Q_atof( cvar->GetDefault() ) )
									continue;
							}

							char cvarcmd[ MAX_OSPATH ];
							
							int add = 0;
							if ( !( cvar->GetFlags() & FCVAR_NEVER_AS_STRING ) )
							{
								add = Q_snprintf( cvarcmd, sizeof(cvarcmd),"%25.25s %25.25s %25.25s\n",
									cvar->GetName(), Host_CleanupConVarStringValue( cvar->GetString() ), cvar->GetDefault() );
							}
							else
							{
								add = Q_snprintf( cvarcmd, sizeof(cvarcmd),"%25.25s %25.25f %25.25f\n",
								cvar->GetName(), cvar->GetFloat(), Q_atof( cvar->GetDefault() ) );
							}

							int toCopy = MIN( add, remainder );
							if ( toCopy <= 0 )
								break;

							Q_memcpy( pbuf, cvarcmd, toCopy );

							pbuf += toCopy;
							remainder -= toCopy;
							if ( remainder <= 0 )
								break;
						}
						*pbuf = 0;
					}

					V_strncat( m_errorText, "\nConsole History (reversed)\n\n", nSize );

					// Get console
					len = V_strlen( m_errorText );
					
					if ( len < nSize )
					{
						GetSpew( m_errorText + len, nSize - len );
					}
				}
				catch ( ... )
				{
					Q_strncat( m_errorText, "exception thrown building console/convar history!!!\n", nSize );
				}

#if !defined( NO_STEAM )
				SteamAPI_SetMiniDumpComment( m_errorText );
#endif
			}
		}
		catch ( ... )
		{
			// Oh oh
		}
		
#ifdef IS_WINDOWS_PC
		_set_se_translator( curfilter );
#endif

#endif
	}

public:
	int		m_Size;
	char	*m_errorText;
};

static CErrorText errorText( DUMP_COMMENT_SIZE );

void BuildMinidumpComment( char const *pchSysErrorText )
{
	errorText.BuildComment( pchSysErrorText );
}

#ifndef DEDICATED

extern "C" void __cdecl WriteMiniDumpUsingExceptionInfo( unsigned int uStructuredExceptionCode, struct _EXCEPTION_POINTERS * pExceptionInfo	)
{
	// TODO: dynamically set the minidump comment from contextual info about the crash (i.e current VPROF node)?
#if !defined( NO_STEAM ) && !defined( _PS3 )

	if ( g_bUpdateMinidumpComment )
	{
		Status_Update();
		BuildMinidumpComment( NULL );
	}

	SteamAPI_WriteMiniDump( uStructuredExceptionCode, pExceptionInfo, build_number() );
	// Clear DSound Buffers so the sound doesn't loop while the game shuts down
// 	try
// 	{
// 		S_ClearBuffer();
// 	}
// 	catch ( ... )
// 	{
// 	}
#endif
} 

extern "C" void __cdecl WriteMiniDump( void	);

//-----------------------------------------------------------------------------
// Purpose: Main 
//-----------------------------------------------------------------------------
int CEngineAPI::Run()
{
	if ( CommandLine()->FindParm("-insecure") )
	{
		extern void Host_DisallowSecureServers();
		Host_DisallowSecureServers();
	}

#ifdef _X360
	return RunListenServer(); // don't handle exceptions on 360 (because if we do then minidumps won't work at all)
#elif defined ( _WIN32 )
	if ( !Plat_IsInDebugSession() && !CommandLine()->FindParm( "-nominidumps") )
	{
		// This warning is not actually true in this context.
#pragma warning( suppress : 4535 ) // warning C4535: calling _set_se_translator() requires /EHa
		_set_se_translator( WriteMiniDumpUsingExceptionInfo );

		try  // this try block allows the SE translator to work
		{
			return RunListenServer();
		}
		catch( ... )
		{
#if defined(_WIN32) && !defined( _X360 )
			// We don't want global destructors in our process OR in any DLL to get executed.
			// _exit() avoids calling global destructors in our module, but not in other DLLs.
			TerminateProcess( GetCurrentProcess(), 100 );
#else
			_exit( 100 );
#endif
			return RUN_OK;
		}
	}
	else
	{
		return RunListenServer();
	}
#elif defined( _PS3 )
	return RunListenServer();
#else
	Assert( !"Impl minidump handling on Posix" );
	return RunListenServer();
#endif
}
#endif // DEDICATED

bool g_bUsingLegacyAppSystems = false;

bool CModAppSystemGroup::AddLegacySystems()
{
	g_bUsingLegacyAppSystems = true;

	AppSystemInfo_t appSystems[] = 
	{
		{ "soundemittersystem"  DLL_EXT_STRING, SOUNDEMITTERSYSTEM_INTERFACE_VERSION },
		{ "", "" }					// Required to terminate the list
	};

	if ( !AddSystems( appSystems ) ) 
		return false;

#if !defined( _LINUX ) && !defined( _GAMECONSOLE )
//	if ( CommandLine()->FindParm( "-tools" ) )
	{
		AppModule_t toolFrameworkModule = LoadModule( "engine" DLL_EXT_STRING );

		if ( !AddSystem( toolFrameworkModule, VTOOLFRAMEWORK_INTERFACE_VERSION ) )
			return false;
	}
#endif

	return true;
}

//-----------------------------------------------------------------------------
static int VerToInt( const char *pszVersion )
{
	char szOut[ 32 ];
	const char *pIn = pszVersion;
	char *pOut = szOut;

	if ( !pszVersion || strlen( pszVersion ) > sizeof( szOut ) ) // double check we won't overflow the buffer
	{
		return 0;
	}

	while ( *pIn )
	{
		if ( *pIn != '.' )
		{
			*pOut++ = *pIn;
		}
		pIn++;
	}
	*pOut = '\0';

	return atoi( szOut );
}


#define VERSION_KEY "PatchVersion="
#define CLIENT_VERSION_KEY "ClientVersion="
#define SERVER_VERSION_KEY "ServerVersion="
#define PRODUCT_KEY "ProductName="
#define APPID_KEY "AppID="
#define PRODUCT_STRING "valve"
#define VERSION_STRING "1.0.1.0"
#define FS_MAGIC_NUM_KEY "FSKey="

static CUtlString g_sVersionString;
static CUtlString g_sProductString;
static int32 sHostVersion;
static int32 sClientVersion;
static int32 sServerVersion;

//-----------------------------------------------------------------------------
const char *GetHostVersionString()
{
	return g_sVersionString.String();
}

//-----------------------------------------------------------------------------
const char *GetHostProductString()
{
	return g_sProductString.String();
}

int32 GetHostVersion()
{
	return sHostVersion;
}

//-----------------------------------------------------------------------------
int32 GetClientVersion()
{
	// if we are running from perforce, we report 0 as version so checking isn't enforced
	if ( g_bRunningFromPerforce )
	{
		return 0;
	}
	return sClientVersion;
}

//-----------------------------------------------------------------------------
int32 GetServerVersion()
{
	// if we are running from perforce, we report 0 as version so checking isn't enforced
	if ( g_bRunningFromPerforce )
	{
		return 0;
	}
	return sServerVersion;
}

const char *Sys_GetVersionString()
{
	return g_sVersionString.String();
}

const char *Sys_GetProductString()
{
	return g_sProductString;
}

static bool ParseSteamInfFile( const char *szFileName, AppId_t &unSteamAppID )
{
	char *buffer;
	int bufsize = 0;
	FileHandle_t fp = NULL;
	const char *pbuf = NULL;
	const int numKeysExpected = 5; // number of expected keys
	int gotKeys = 0;

	// Mod's steam.inf is first option, the the steam.inf in the game GCF. 
	fp = g_pFileSystem->Open( szFileName, "r" );
	if ( fp )
	{
		bufsize = g_pFileSystem->Size( fp );
		buffer = ( char * )stackalloc( bufsize + 1 );
		Assert( buffer );

		int iBytesRead = g_pFileSystem->Read( buffer, bufsize, fp );
		g_pFileSystem->Close( fp );

		buffer[iBytesRead] = '\0';

		// Read 
		pbuf = buffer;

		while ( 1 )
		{
			pbuf = COM_Parse( pbuf );
			if ( !pbuf )
				break;

			if ( Q_strlen( com_token )  <= 0 )
				break;


			if ( !Q_strnicmp( com_token, VERSION_KEY, Q_strlen( VERSION_KEY ) ) )
			{
				char buf[ 256 ];
				Q_strncpy( buf, com_token+Q_strlen( VERSION_KEY ), sizeof( buf ) - 1 );
				buf[ sizeof( buf ) - 1 ] = '\0';

				g_sVersionString = buf;
				sHostVersion = VerToInt( buf );

				gotKeys++;
				continue;
			}

			if ( !Q_strnicmp( com_token, CLIENT_VERSION_KEY, Q_strlen( CLIENT_VERSION_KEY ) ) )
			{
				char buf[ 256 ];
				Q_strncpy( buf, com_token+Q_strlen( CLIENT_VERSION_KEY ), sizeof( buf ) - 1 );
				buf[ sizeof( buf ) - 1 ] = '\0';

				sClientVersion = atoi( buf );

				gotKeys++;
				continue;
			}

			if ( !Q_strnicmp( com_token, SERVER_VERSION_KEY, Q_strlen( SERVER_VERSION_KEY ) ) )
			{
				char buf[ 256 ];
				Q_strncpy( buf, com_token+Q_strlen( SERVER_VERSION_KEY ), sizeof( buf ) - 1 );
				buf[ sizeof( buf ) - 1 ] = '\0';

				sServerVersion = atoi( buf );

				gotKeys++;
				continue;
			}

			if ( !Q_strnicmp( com_token, PRODUCT_KEY, Q_strlen( PRODUCT_KEY ) ) )
			{
				char buf[ 256 ];
				Q_strncpy( buf, com_token+Q_strlen( PRODUCT_KEY ), sizeof( buf ) - 1 );
				buf[ sizeof( buf ) - 1 ] = '\0';

				g_sProductString = buf;

				gotKeys++;
				continue;
			}

			// Steam reads the AppID out of steam_appid.txt
			if ( !Q_strnicmp( com_token, APPID_KEY, Q_strlen( APPID_KEY ) ) )
			{
				char szAppID[32];
				Q_strncpy( szAppID, com_token + Q_strlen( APPID_KEY ), sizeof( szAppID ) - 1 );
				unSteamAppID = atoi(szAppID);
				gotKeys++;
				continue;
			}
		}
	}

	return gotKeys == numKeysExpected;
}


//-----------------------------------------------------------------------------
static bool ParsePerforceInfFile( const char *szFileName, uint64 &unFileSystemMagicNumber )
{
	char *buffer;
	int bufsize = 0;
	FileHandle_t fp = NULL;

	// Mod's steam.inf is first option, the the steam.inf in the game GCF. 
	fp = g_pFileSystem->Open( szFileName, "r" );
	if ( fp )
	{
		bufsize = g_pFileSystem->Size( fp );
		buffer = ( char * )_alloca( bufsize + 1 );
		Assert( buffer );

		int iBytesRead = g_pFileSystem->Read( buffer, bufsize, fp );
		g_pFileSystem->Close( fp );

		buffer[iBytesRead] = '\0';

		// Read 
		const char *pbuf = buffer;

		while ( 1 )
		{
			pbuf = COM_Parse( pbuf );
			if ( !pbuf )
				break;

			if ( Q_strlen( com_token )  <= 0 )
				break;

			// Get the magic number that allows us to run without Steam internally. This magic number should only live in the Perforce.inf
			// file which is never shipped. 
			if ( !Q_strnicmp( com_token, FS_MAGIC_NUM_KEY, Q_strlen( FS_MAGIC_NUM_KEY ) ) )
			{
				char szFSKey[64];
				Q_strncpy( szFSKey, com_token + Q_strlen( FS_MAGIC_NUM_KEY ), sizeof( szFSKey ) - 1 );
				unFileSystemMagicNumber = atoi(szFSKey);
				return true;
			}
		}
	}
	return false;
}


void Sys_Version( bool bDedicated )
{
#if defined( _X360 )
	// [Forrest] $FIXME Hack: The Xbox doesn't have a steam.inf file from which to load a patch version.
	// However, if GetHostVersion doesn't match between the Xbox and the PC dedicated server then they can't connect.
	// Perhaps the version checks should be removed when an Xbox is connected, but for now I'll just hard-code a matching version.
	sHostVersion = 10040;
#endif

#if !defined( _X360 )
	g_sVersionString = VERSION_STRING;
	g_sProductString = PRODUCT_STRING;

	uint64 unFSMagicNumber = 0;

	if ( !ParseSteamInfFile( "steam.inf", g_unSteamAppID ) )
	{
		Sys_Error( "Unable to load version from steam.inf" );
	}

	// if we aren't launched by Steam try reading a local perforce inf file
	// this lets us tell the internal staging/main builds to use the right app ids
	// if we arent launched by steam - we havent resolved our universe yet so we dont know
	// what it is - so check perforce.inf always for now
	if ( !g_pFileSystem->IsSteam() )
		ParsePerforceInfFile( "perforce.inf", unFSMagicNumber );

	// If the magic number is found in the perforce.inf then we are running from Perforce
	g_bRunningFromPerforce = ( unFSMagicNumber == 2190015756ull / 2 );

	if ( g_unSteamAppID != k_uAppIdInvalid && ( !g_pFileSystem->IsSteam() || bDedicated || g_bRunningFromPerforce ) )
	{
		// steamclient.dll doesn't know about steam.inf files in mod folder,
		// it excepts a steam_appid.txt in the root directory if the game is
		// not started through Steam. So we create one there containing the
		// current AppID 
		FILE *f = fopen( "steam_appid.txt", "wb" );
		if ( f )
		{
			char rgchAppID[256];
			Q_snprintf( rgchAppID, sizeof(rgchAppID), "%u\n", g_unSteamAppID );
			fwrite( rgchAppID, Q_strlen(rgchAppID)+1, 1, f );
			fclose( f );
		}
	}

#ifdef _WIN32
	else
	{		
		AssertMsg( !g_pFileSystem->FileExists( "perforce.inf" ), "<mod dir>\\perforce.inf included in a steam cache, remove it!" );
	}
#endif // _WIN32
#endif 
}


#ifdef ENGINE_MANAGES_VJOBS
void LoadVjobsModule()
{
	Assert( !g_pVjobsDllModule );
	g_pVjobsDllModule = g_pFileSystem->LoadModule( "vjobs" DLL_EXT_STRING, "EXECUTABLE_PATH", false );
	if( !g_pVjobsDllModule )
	{
		Sys_Error( "Could not load vjobs library\n" );
	}
	g_pfnVjobsFactory = Sys_GetFactory( g_pVjobsDllModule );
	if( !g_pfnVjobsFactory )
	{
		Sys_Error( "Could not get vjobs factory\n" );
	}
	IVJobs * pVJobs = ( IVJobs* )( *g_pfnVjobsFactory )( VJOBS_INTERFACE_VERSION, NULL );
	Assert( g_pVJobs == pVJobs || !g_pVJobs );
	g_pVJobs = pVJobs;
}

void ReloadDlls()
{
// 	if( g_bVjobsTest )
// 	{
// 		g_pVJobs->SetRunTarget( RUN_TARGET_SATELLITE_CPU );
// 		g_pVJobs->StartTest();
// 		g_pVJobs->SetRunTarget( RUN_TARGET_MAIN_CPU );
// 		g_pVJobs->StartTest();
// 		g_bVjobsTest = false;
// 	}
	if( g_bVjobsReload )
	{
		g_bVjobsReload = false;
		if( g_pVJobs )
		{
			g_pVJobs->BeforeReload();
		}
		if( g_pVjobsDllModule )
		{
			g_pFileSystem->UnloadModule( g_pVjobsDllModule );
			g_pVjobsDllModule = NULL;
		}
		LoadVjobsModule();
		if( g_pVJobs )
		{
			g_pVJobs->AfterReload();
		}
	}
}

#endif

//-----------------------------------------------------------------------------
// Instantiate all main libraries
//-----------------------------------------------------------------------------
bool CModAppSystemGroup::Create()
{
	COM_TimestampedLog( "CModAppSystemGroup::Create() - Start" );

	// If we're not running from Perforce check if we need to restart under Steam
	if ( !g_bRunningFromPerforce && g_unSteamAppID != k_uAppIdInvalid && SteamAPI_RestartAppIfNecessary( g_unSteamAppID ) )
	{
		Plat_ExitProcess( EXIT_SUCCESS );
	}
	
#ifdef ENGINE_MANAGES_VJOBS
	//////////////////////////////////////////////////////////////////////////
	//
	// Vjobs
	//
	LoadVjobsModule();	
	AddSystem( g_pVJobs, VJOBS_INTERFACE_VERSION ); // this is done only once; g_pVJobs doesn't change even after multiple reloads of VJobs.prx
#endif
	//////////////////////////////////////////////////////////////////////////
	//
	// Matchmaking
	//

	Assert ( !g_pMatchmakingDllModule );

	// Check the signature on the client dll.  If this fails we load it anyway but put this client
	// into insecure mode so it won't connect to secure servers and get VAC banned
	if ( !IsServerOnly() && !Host_AllowLoadModule( "matchmaking" DLL_EXT_STRING, "GAMEBIN", false ) )
	{
		// not supposed to load this but we will anyway
		Host_DisallowSecureServers();
	}

	// loads the matchmaking.dll
	g_pMatchmakingDllModule = g_pFileSystem->LoadModule(
		IsServerOnly() ? ( "matchmaking_ds" DLL_EXT_STRING ) : ( "matchmaking" DLL_EXT_STRING ),
		"GAMEBIN", false );

	if ( g_pMatchmakingDllModule )
	{
		g_pfnMatchmakingFactory = Sys_GetFactory( g_pMatchmakingDllModule );
		if ( g_pfnMatchmakingFactory )
		{
			g_pIfaceMatchFramework = ( IMatchFramework * ) g_pfnMatchmakingFactory( IMATCHFRAMEWORK_VERSION_STRING, NULL );

			if ( !g_pIfaceMatchFramework )
			{
				if( IsPS3() )
					return false;
				else
					Sys_Error( "Could not get matchmaking.dll interface from library matchmaking" );
			}
			
			// matchmaking.dll wasn't loaded by the time tier2 libraries were connecting,
			// set it up in engine now
			g_pMatchFramework = g_pIfaceMatchFramework;
		}
		else
		{
			if( IsPS3() )
				return false;
			else
				Sys_Error( "Could not find factory interface in library matchmaking" );
		}
	}
	else
	{	
		// library failed to load
		if( IsPS3() )
			return false;
		else
			Sys_Error( "Could not load library matchmaking" );
	}

	AddSystem( g_pIfaceMatchFramework, IMATCHFRAMEWORK_VERSION_STRING );
	
	Host_SubscribeForProfileEvents( true );

	//////////////////////////////////////////////////////////////////////////
	//
	// Client/server
	//

#ifndef DEDICATED
	if ( !IsServerOnly() )
	{
		if ( !ClientDLL_Load() )
			return false;
		ClientDLL_Connect();
	}
#endif

	if ( !ServerDLL_Load( IsServerOnly() ) )
	{
#ifndef DEDICATED
		if ( !IsServerOnly() )
		{
			ClientDLL_Disconnect();
		}
#endif
	
		return false;
	}

	IClientDLLSharedAppSystems *clientSharedSystems = 0;

#ifndef DEDICATED
	if ( !IsServerOnly() )
	{
		clientSharedSystems = ( IClientDLLSharedAppSystems * )g_ClientFactory( CLIENT_DLL_SHARED_APPSYSTEMS, NULL );
		if ( !clientSharedSystems )
			return AddLegacySystems();
	}
#endif

	IServerDLLSharedAppSystems *serverSharedSystems = ( IServerDLLSharedAppSystems * )g_ServerFactory( SERVER_DLL_SHARED_APPSYSTEMS, NULL );
	if ( !serverSharedSystems )
	{
		Assert( !"Expected both game and client .dlls to have or not have shared app systems interfaces!!!" );
		return AddLegacySystems();
	}

	// Load game and client .dlls and build list then
	CUtlVector< AppSystemInfo_t >	systems;

	int i;
	int serverCount = serverSharedSystems->Count();
	for ( i = 0 ; i < serverCount; ++i )
	{
		const char *dllName = serverSharedSystems->GetDllName( i );
		const char *interfaceName = serverSharedSystems->GetInterfaceName( i );

		AppSystemInfo_t info;
		info.m_pModuleName = dllName;
		info.m_pInterfaceName = interfaceName;

		systems.AddToTail( info );
	}

	if ( !IsServerOnly() )
	{
		int clientCount = clientSharedSystems->Count();
		for ( i = 0 ; i < clientCount; ++i )
		{
			const char *dllName = clientSharedSystems->GetDllName( i );
			const char *interfaceName = clientSharedSystems->GetInterfaceName( i );

			if ( ModuleAlreadyInList( systems, dllName, interfaceName ) )
				continue;

			AppSystemInfo_t info;
			info.m_pModuleName = dllName;
			info.m_pInterfaceName = interfaceName;

			systems.AddToTail( info );
		}
	}

	AppSystemInfo_t info;
	info.m_pModuleName = "";
	info.m_pInterfaceName = "";
	systems.AddToTail( info );

	if ( !AddSystems( systems.Base() ) ) 
		return false;

#if !defined( _LINUX ) && !defined( _GAMECONSOLE )
//	if ( CommandLine()->FindParm( "-tools" ) )
	{
		AppModule_t toolFrameworkModule = LoadModule( "engine" DLL_EXT_STRING );

		if ( !AddSystem( toolFrameworkModule, VTOOLFRAMEWORK_INTERFACE_VERSION ) )
			return false;
	}
#endif

	COM_TimestampedLog( "CModAppSystemGroup::Create() - Finish" );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Fixme, we might need to verify if the interface names differ for the client versus the server
// Input  : list - 
//			*moduleName - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CModAppSystemGroup::ModuleAlreadyInList( CUtlVector< AppSystemInfo_t >& list, const char *moduleName, const char *interfaceName )
{
	for ( int i = 0; i < list.Count(); ++i )
	{
		if ( !Q_stricmp( list[ i ].m_pModuleName, moduleName ) )
		{
			if ( Q_stricmp( list[ i ].m_pInterfaceName, interfaceName ) )
			{
				Error( "Game and client .dlls requesting different versions '%s' vs. '%s' from '%s'\n",
					list[ i ].m_pInterfaceName, interfaceName, moduleName );
			}
			return true;
		}
	}

	return false;
}

bool CModAppSystemGroup::PreInit()
{
	return true;
}

void SV_ShutdownGameDLL();
int CModAppSystemGroup::Main()
{
	int nRunResult = RUN_OK;

	if ( IsServerOnly() )
	{
		// Start up the game engine
		if ( eng->Load( true, host_parms.basedir ) )
		{
			// If we're using STEAM, pass the map cycle list as resource hints...
#if LATER
			if ( g_pFileSystem->IsSteam() )
			{
				char *hints;
				if ( BuildMapCycleListHints(&hints) )
				{
					g_pFileSystem->HintResourceNeed(hints, true);
				}
				if ( hints )
				{
					free(hints);
				}
			}
#endif
			// Dedicated server drives frame loop manually
			dedicated->RunServer();

			SV_ShutdownGameDLL();
		}
	}
	else
	{
		eng->SetQuitting( IEngine::QUIT_NOTQUITTING );

		COM_TimestampedLog( "eng->Load" );

		// Start up the game engine
		if ( eng->Load( false, host_parms.basedir ) )					
		{
#if !defined(DEDICATED)
			toolframework->ServerInit( g_ServerFactory );

			if ( s_EngineAPI.MainLoop() )
			{
				nRunResult = RUN_RESTART;
			}

			// unload systems
			eng->Unload();

			toolframework->ServerShutdown();
#endif
			SV_ShutdownGameDLL();
		}
	}
	
	return nRunResult;
}

void CModAppSystemGroup::PostShutdown()
{
}

void CModAppSystemGroup::Destroy() 
{
	if ( g_pMatchFramework )
	{
		TRACESHUTDOWN( g_pMatchFramework->Shutdown() );
		g_pMatchFramework = NULL;
	}

	// unload game and client .dlls
	ServerDLL_Unload();
#ifndef DEDICATED
	if ( !IsServerOnly() )
	{
		ClientDLL_Unload();
	}
#endif
	
	/// Matchmaking

	Host_SubscribeForProfileEvents( false );

	FileSystem_UnloadModule( g_pMatchmakingDllModule );

	g_pIfaceMatchFramework = NULL;
	g_pMatchmakingDllModule = NULL;
	g_pfnMatchmakingFactory = NULL;
	g_pMatchFramework = NULL;

	/// vjobs

#ifdef ENGINE_MANAGES_VJOBS
	if( g_pVjobsDllModule )
	{
		g_pFileSystem->UnloadModule( g_pVjobsDllModule );
		g_pVjobsDllModule = NULL;
		g_pfnVjobsFactory = NULL;
		g_pVJobs = NULL;
	}
#endif
}

//-----------------------------------------------------------------------------
// Console command to toggle back and forth between the engine running or not
//-----------------------------------------------------------------------------
#ifndef DEDICATED
void EditorToggle_f()
{
	// Will switch back to the editor
	bool bActive = (eng->GetState() != IEngine::DLL_PAUSED);
	s_EngineAPI.ActivateSimulation( !bActive );
}
#endif // DEDICATED



//-----------------------------------------------------------------------------
//
// Purpose: Expose engine interface to launcher	for dedicated servers
//
//-----------------------------------------------------------------------------
class CDedicatedServerAPI : public CTier3AppSystem< IDedicatedServerAPI >
{
	typedef CTier3AppSystem< IDedicatedServerAPI > BaseClass;

public:
	CDedicatedServerAPI() :
	  m_pDedicatedServer( 0 )
	{
	}
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );

	virtual bool ModInit( ModInfo_t &info );
	virtual void ModShutdown( void );

	virtual bool RunFrame( void );

	virtual void AddConsoleText( char *text );
	virtual void UpdateStatus(float *fps, int *nActive, int *nMaxPlayers, char *pszMap, int maxlen );
	virtual void UpdateHostname(char *pszHostname, int maxlen);

	virtual void SetSubProcessID( int nID, int nChildSocketHandle );

	static void PreMinidumpCallback( void *pvContext );
	void PreMinidumpCallbackImpl();

private:
	int BuildMapCycleListHints( char **hints );

	CModAppSystemGroup *m_pDedicatedServer;
};

void CDedicatedServerAPI::SetSubProcessID( int nId, int nChildSocketHandle )
{
	g_nForkID = nId;
	g_nSocketToParentProcess = nChildSocketHandle;

}

// Static method
void CDedicatedServerAPI::PreMinidumpCallback( void *pvContext )
{
	if ( !pvContext )
	{
		return;
	}
	
	((CDedicatedServerAPI *)pvContext)->PreMinidumpCallbackImpl();
}


void CDedicatedServerAPI::PreMinidumpCallbackImpl()
{
	EndWatchdogTimer(); // Uploading the dump can take a while, turn off our watchdog

	// Win32 dedicated servers build a minidump comment in the exception handler itself
#if defined( LINUX )
	fprintf( stderr, "PreMinidumpCallback: updating dump comment\n" );
	BuildMinidumpComment( NULL );
#endif
}


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
EXPOSE_SINGLE_INTERFACE( CDedicatedServerAPI, IDedicatedServerAPI, VENGINE_HLDS_API_VERSION );

bool g_bIsVGuiBasedDedicatedServer = false; // Assume use convar
//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CDedicatedServerAPI::Connect( CreateInterfaceFn factory ) 
{ 
	if ( CommandLine()->FindParm( "-sv_benchmark" ) != 0 )
	{
		Plat_SetBenchmarkMode( true );
	}

	// Store off the app system factory...
	g_AppSystemFactory = factory;

	if ( !BaseClass::Connect( factory ) )
		return false;

	dedicated = ( IDedicatedExports * )factory( VENGINE_DEDICATEDEXPORTS_API_VERSION, NULL );
	if ( !dedicated )
		return false;

	g_pFileSystem = g_pFullFileSystem;

#ifndef DBGFLAG_STRINGS_STRIP
	g_pFileSystem->SetWarningFunc( Warning );
#endif

	if ( !Shader_Connect( false ) )
		return false;

	if ( !g_pStudioRender )
	{
		Sys_Error( "Unable to init studio render system version %s\n", STUDIO_RENDER_INTERFACE_VERSION );
		return false;
	}

	g_pPhysics = (IPhysics*)factory( VPHYSICS_INTERFACE_VERSION, NULL );

	g_pSoundEmitterSystem = (ISoundEmitterSystemBase*)factory( SOUNDEMITTERSYSTEM_INTERFACE_VERSION, NULL);

#if defined( DEDICATED )
	if ( !g_pDataCache || !g_pPhysics || !g_pMDLCache ) 
#else
	if ( !g_pDataCache || !g_pPhysics || !g_pMDLCache || !g_pSoundEmitterSystem)
#endif
	{
		Warning( "Engine wasn't able to acquire required interfaces!\n" );
		return false;
	}

	ConnectMDLCacheNotify();

#ifndef DEDICATED
	splitscreen->Init();
#endif

	return true; 
}

void CDedicatedServerAPI::Disconnect() 
{
#ifndef DEDICATED
	splitscreen->Shutdown();
#endif

	DisconnectMDLCacheNotify();

	g_pPhysics = NULL;
	g_pSoundEmitterSystem = NULL;

	Shader_Disconnect();

	g_pFileSystem = NULL;

	ConVar_Unregister();

	dedicated = NULL;

	BaseClass::Disconnect();

	g_AppSystemFactory = NULL;
}

//-----------------------------------------------------------------------------
// Query interface
//-----------------------------------------------------------------------------
void *CDedicatedServerAPI::QueryInterface( const char *pInterfaceName )
{
	// Loading the engine DLL mounts *all* engine interfaces
	CreateInterfaceFn factory = Sys_GetFactoryThis();	// This silly construction is necessary
	return factory( pInterfaceName, NULL );				// to prevent the LTCG compiler from crashing.
}

//-----------------------------------------------------------------------------
// Creates the hint list for a multiplayer map rotation from the map cycle.
// The map cycle message is a text string with CR/CRLF separated lines.
//	-removes comments
//	-removes arguments
//-----------------------------------------------------------------------------
const char *szCommonPreloads  = "MP_Preloads";
const char *szReslistsBaseDir = "reslists2";
const char *szReslistsExt     = ".lst";

int CDedicatedServerAPI::BuildMapCycleListHints(char **hints)
{
	char szMap[ MAX_OSPATH + 2 ]; // room for one path plus <CR><LF>
	unsigned int length;
	char szMod[MAX_OSPATH];

	// Determine the mod directory.
	Q_FileBase(com_gamedir, szMod, sizeof( szMod ) );

	// Open mapcycle.txt
	char cszMapCycleTxtFile[MAX_OSPATH];
	Q_snprintf(cszMapCycleTxtFile, sizeof( cszMapCycleTxtFile ), "%s\\mapcycle.txt", szMod);
	FileHandle_t pFile = g_pFileSystem->Open(cszMapCycleTxtFile, "rb");
	if ( pFile == FILESYSTEM_INVALID_HANDLE )
	{
		ConMsg("Unable to open %s", cszMapCycleTxtFile);
		return 0;
	}

	// Start off with the common preloads.
	Q_snprintf(szMap, sizeof( szMap ), "%s\\%s\\%s%s\r\n", szReslistsBaseDir, szMod, szCommonPreloads, szReslistsExt);
	int hintsSize = strlen(szMap) + 1;
	*hints = (char*)malloc( hintsSize );
	if ( *hints == NULL )
	{
		ConMsg("Unable to allocate memory for map cycle hints list");
		g_pFileSystem->Close( pFile );
		return 0;
	}
	Q_strncpy( *hints, szMap, hintsSize );
		
	// Read in and parse mapcycle.txt
	length = g_pFileSystem->Size(pFile);
	if ( length )
	{
		char *pStart = (char *)malloc(length);
		if ( pStart && ( 1 == g_pFileSystem->Read(pStart, length, pFile) )
		   )
		{
			const char *pFileList = pStart;

			while ( 1 )
			{
				pFileList = COM_Parse( pFileList );
				if ( strlen( com_token ) <= 0 )
					break;

				Q_strncpy(szMap, com_token, sizeof(szMap));

				// Any more tokens on this line?
				if ( COM_TokenWaiting( pFileList ) )
				{
					pFileList = COM_Parse( pFileList );
				}

				char mapLine[sizeof(szMap)];
				Q_snprintf(mapLine, sizeof(mapLine), "%s\\%s\\%s%s\r\n", szReslistsBaseDir, szMod, szMap, szReslistsExt);
				*hints = (char*)realloc(*hints, strlen(*hints) + 1 + strlen(mapLine) + 1); // count NULL string terminators
				if ( *hints == NULL )
				{
					ConMsg("Unable to reallocate memory for map cycle hints list");
					g_pFileSystem->Close( pFile );
					return 0;
				}
				Q_strncat(*hints, mapLine, hintsSize, COPY_ALL_CHARACTERS);
			}
		}
	}

	g_pFileSystem->Close(pFile);

	// Tack on <moddir>\mp_maps.txt to the end to make sure we load reslists for all multiplayer maps we know of
	Q_snprintf(szMap, sizeof( szMap ), "%s\\%s\\mp_maps.txt\r\n", szReslistsBaseDir, szMod);
	*hints = (char*)realloc(*hints, strlen(*hints) + 1 + strlen(szMap) + 1); // count NULL string terminators
	Q_strncat( *hints, szMap, hintsSize, COPY_ALL_CHARACTERS );

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 0 == normal, 1 == dedicated server
//			*instance - 
//			*basedir - 
//			*cmdline - 
//			launcherFactory - 
//-----------------------------------------------------------------------------
AppId_t g_nDedicatedServerAppIdBreakpad = 0;
bool CDedicatedServerAPI::ModInit( ModInfo_t &info )
{
	g_bIsVGuiBasedDedicatedServer = dedicated->IsGuiDedicatedServer();
	s_bIsDedicatedServer = true;

	//
	// Configure breakpad
	// this must be done after mod search path chain is configured
	//
	// Parse AppID from steam.inf file
	Sys_Version( true );

#if !defined( NO_STEAM ) && !defined( _GAMECONSOLE )
	if ( !CommandLine()->FindParm( "-nobreakpad" ) )
	{
		bool bValveDS = false;
		if ( !CommandLine()->FindParm( "-novalveds" ) )
		{
			char const *szDllFilename = "server_valve" DLL_EXT_STRING;
			if ( g_pFileSystem->FileExists( szDllFilename, "GAMEBIN" ) )
				bValveDS = true;
		}
		
		// Override reporting AppID based on CS:GO depot mappings
		switch ( g_unSteamAppID )
		{
		case 710:	// Trunk / debug (fake appids)
			g_nDedicatedServerAppIdBreakpad = bValveDS ? 712 : 711;
			break;
		case 730:	// Rel public / pcbeta
			g_nDedicatedServerAppIdBreakpad = bValveDS ? 741 : 740;
			break;
		case 268440:// Staging
			g_nDedicatedServerAppIdBreakpad = bValveDS ? 268480 : 268460;
			break;
		}
		if ( g_nDedicatedServerAppIdBreakpad )	// Override breakpad AppID
			SteamAPI_SetBreakpadAppID( g_nDedicatedServerAppIdBreakpad );

		// Build a custom version string
		CFmtStr fmtServerVersion( "%d.%d.D%c", GetHostVersion(), GetServerVersion(),
			(bValveDS ? 'V' : 'C')
			);
		Msg( "Using breakpad minidump system %u/%s\n", g_nDedicatedServerAppIdBreakpad ? g_nDedicatedServerAppIdBreakpad : g_unSteamAppID, fmtServerVersion.Access() );
		SteamAPI_UseBreakpadCrashHandler( fmtServerVersion.Access(), __DATE__, __TIME__, false /*full_memory_dumps*/, &__g_CDedicatedServerAPI_singleton, &CDedicatedServerAPI::PreMinidumpCallback );

		if ( g_nDedicatedServerAppIdBreakpad )	// Actually force breakpad interfaces to load
			SteamAPI_SetBreakpadAppID( g_nDedicatedServerAppIdBreakpad );
	}
#endif // !NO_STEAM && !_GAMECONSOLE

	eng->SetQuitting( IEngine::QUIT_NOTQUITTING );

	// Set up the engineparms_t which contains global information about the mod
	host_parms.basedir = const_cast<char*>(info.m_pBaseDirectory);
	host_parms.mod = const_cast<char*>(GetModDirFromPath(info.m_pInitialMod));
	host_parms.game = const_cast<char*>(info.m_pInitialGame);

	g_bTextMode = info.m_bTextMode;

	TRACEINIT( COM_InitFilesystem( info.m_pInitialMod ), COM_ShutdownFileSystem() );
	// set this up as early as possible, if the server isn't going to run pure, stop CRCing bits as we load them
	// this happens even before the ConCommand's are processed, but we need to be sure to either CRC every file
	// that is loaded, or not bother doing any
	// Note that this mirrors g_sv_pure_mode from sv_main.cpp
	int pure_mode = 1; // default to on, +sv_pure 0 or -sv_pure 0 will turn it off
	if ( CommandLine()->CheckParm("+sv_pure") )
		pure_mode = CommandLine()->ParmValue( "+sv_pure", 1 );
	else if ( CommandLine()->CheckParm("-sv_pure") )
		pure_mode = CommandLine()->ParmValue( "-sv_pure", 1 );
	if ( pure_mode )
	{
		// Mount all compatibility VPKs as well at the tail of the VPK search chain for verifying sv_pure clients
		//
		// See above:
		//		CEngineAPI::SetStartupInfo
		//	for how clients enable VPK content-shadowing
		//
		char const *szCompatibilityVPKs[] = {
			"lowviolence",
			"perfectworld"
		};
		for ( int j = 0; j < Q_ARRAYSIZE( szCompatibilityVPKs ); ++ j )
		{	// Add in same order listed to tail
			g_pFullFileSystem->AddSearchPath( szCompatibilityVPKs[ j ], "COMPAT:GAME", PATH_ADD_TO_TAIL );
		}
		g_pFullFileSystem->EnableWhitelistFileTracking( true, true, CommandLine()->FindParm( "-sv_pure_verify_hashes" ) ? true : false );
	}
	else
		g_pFullFileSystem->EnableWhitelistFileTracking( false, false, false );

	materials->ModInit();

	// Setup the material system config record, CreateGameWindow depends on it
	// (when we're running stand-alone)
#ifndef DEDICATED
	InitMaterialSystemConfig( true );						// !!should this be called standalone or not?
#endif

	// Initialize general game stuff and create the main window
	if ( game->Init( NULL ) )
	{
		m_pDedicatedServer = new CModAppSystemGroup( true, info.m_pParentAppSystemGroup );

		// Store off the app system factory...
		g_AppSystemFactory = m_pDedicatedServer->GetFactory();

		m_pDedicatedServer->Run();
		return true;
	}

	return false;
}

void CDedicatedServerAPI::ModShutdown( void )
{
	if ( m_pDedicatedServer )
	{
		delete m_pDedicatedServer;
		m_pDedicatedServer = NULL;
	}

	g_AppSystemFactory = NULL;

	// Unload GL, Sound, etc.
	eng->Unload();

	// Shut down memory, etc.
	game->Shutdown();

	materials->ModShutdown();
	TRACESHUTDOWN( COM_ShutdownFileSystem() );
}

bool CDedicatedServerAPI::RunFrame( void )
{
	// Bail if someone wants to quit.
	if ( eng->GetQuitting() != IEngine::QUIT_NOTQUITTING )
	{
		return false;
	}

	// Run engine frame
	eng->Frame();
	return true;
}

void CDedicatedServerAPI::AddConsoleText( char *text )
{
	Cbuf_AddText( CBUF_SERVER, text );
}

void CDedicatedServerAPI::UpdateStatus(float *fps, int *nActive, int *nMaxPlayers, char *pszMap, int maxlen )
{
	Host_GetHostInfo( fps, nActive, nMaxPlayers, pszMap, maxlen );
}

void CDedicatedServerAPI::UpdateHostname(char *pszHostname, int maxlen)
{
	if ( pszHostname && ( maxlen > 0 ) )
	{
		Q_strncpy( pszHostname, sv.GetName(), maxlen );
	}
}

#ifndef DEDICATED

class CGameUIFuncs : public IGameUIFuncs
{
public:
	bool IsKeyDown( const char *keyname, bool& isdown )
	{
		isdown = false;
		if ( !g_ClientDLL )
			return false;

		return g_ClientDLL->IN_IsKeyDown( keyname, isdown );
	}

	const char	*GetBindingForButtonCode( ButtonCode_t code )
	{
		return ::Key_BindingForKey( code );
	}

	virtual ButtonCode_t GetButtonCodeForBind( const char *bind, int userId )
	{
		const char *pKeyName = Key_NameForBinding( bind , userId );
		if ( !pKeyName )
			return KEY_NONE;
		return g_pInputSystem->StringToButtonCode( pKeyName ) ;
	}

	void GetVideoModes( struct vmode_s **ppListStart, int *pCount )
	{
		if ( videomode )
		{
			*pCount = videomode->GetModeCount();
			*ppListStart = videomode->GetMode( 0 );
		}
		else
		{
			*pCount = 0;
			*ppListStart = NULL;
		}
	}

	void GetDesktopResolution( int &width, int &height )
	{
		int refreshrate;
		game->GetDesktopInfo( width, height, refreshrate );
	}

	virtual void SetFriendsID( uint friendsID, const char *friendsName )
	{
		GetLocalClient().SetFriendsID( friendsID, friendsName );
	}

	bool IsConnectedToVACSecureServer()
	{
		if ( GetBaseLocalClient().IsConnected() )
			return Steam3Client().BGSSecure();
		return false;
	}
};

EXPOSE_SINGLE_INTERFACE( CGameUIFuncs, IGameUIFuncs, VENGINE_GAMEUIFUNCS_VERSION );

#endif
