//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

// HACKHACK fix this include
#if defined( _WIN32 ) && !defined( _X360 )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "tier0/vprof.h"
#include "server.h"
#include "host_cmd.h"
#include "keys.h"
#include "screen.h"
#include "vengineserver_impl.h"
#include "host_saverestore.h"
#include "sv_filter.h"
#include "gl_matsysiface.h"
#include "pr_edict.h"
#include "world.h"
#include "checksum_engine.h"
#include "const.h"
#include "sv_main.h"
#include "host.h"
#include "demo.h"
#include "cdll_int.h"
#include "networkstringtableserver.h"
#include "networkstringtableclient.h"
#include "host_state.h"
#include "string_t.h"
#include "tier0/dbg.h"
#include "testscriptmgr.h"
#include "r_local.h"
#include "PlayerState.h"
#include "enginesingleuserfilter.h"
#include "profile.h"
#include "protocol.h"
#include "cl_main.h"
#include "sv_steamauth.h"
#include "zone.h"
#include "GameEventManager.h"
#include "datacache/idatacache.h"
#include "sys_dll.h"
#include "cmd.h"
#include "tier0/icommandline.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "icliententitylist.h"
#include "icliententity.h"
#include "hltvserver.h"
#if defined( REPLAY_ENABLED )
#include "replayserver.h"
#endif
#include "cdll_engine_int.h"
#include "cl_steamauth.h"
#include "cl_splitscreen.h"
#ifndef DEDICATED
#include "vgui_baseui_interface.h"
#endif
#include "sound.h"
#include "voice.h"
#include "sv_rcon.h"
#if defined( _X360 )
#include "xbox/xbox_console.h"
#include "xbox/xbox_launch.h"
#elif defined( _PS3 )
#include "ps3/ps3_console.h"
#include "tls_ps3.h"
#endif
#include "filesystem/IQueuedLoader.h"
#include "filesystem/IXboxInstaller.h"
#include "toolframework/itoolframework.h"
#include "fmtstr.h"
#include "tier3/tier3.h"
#include "matchmaking/imatchframework.h"
#include "tier2/tier2.h"
#include "shaderapi/gpumemorystats.h"
#include "snd_audio_source.h"
#include "netconsole.h"
#include "tier2/fileutils.h"

#if POSIX
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include "ixboxsystem.h"
extern IXboxSystem *g_pXboxSystem;

extern IVEngineClient *engineClient;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define STEAM_PREFIX "STEAM_"

#ifndef DEDICATED
bool g_bInEditMode = false;
bool g_bInCommentaryMode = false;
#endif
KeyValues *g_pLaunchOptions = NULL;

void PerformKick( cmd_source_t commandSource, int iSearchIndex, char* szSearchString, bool bForceKick, const char* pszMessage );

ConVar host_name_store( "host_name_store", "1", FCVAR_RELEASE, "Whether hostname is recorded in game events and GOTV." );
ConVar host_players_show( "host_players_show", "1", FCVAR_RELEASE, "How players are disclosed in server queries: 0 - query disabled, 1 - show only max players count, 2 - show all players" );
ConVar host_info_show( "host_info_show", "1", FCVAR_RELEASE, "How server info gets disclosed in server queries: 0 - query disabled, 1 - show only general info, 2 - show full info" );
ConVar host_rules_show( "host_rules_show", "1", FCVAR_RELEASE, "How server rules get disclosed in server queries: 0 - query disabled, 1 - query enabled" );
static void HostnameChanged( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	Steam3Server().NotifyOfServerNameChange();

	if ( sv.IsActive() && host_name_store.GetBool() )
	{
		// look up the descriptor first to avoid a DevMsg for HL2 and mods that don't define a
		// hostname_change event
		CGameEventDescriptor *descriptor = g_GameEventManager.GetEventDescriptor( "hostname_changed" );
		if ( descriptor )
		{
			IGameEvent *event = g_GameEventManager.CreateEvent( "hostname_changed" );
			if ( event )
			{
				ConVarRef var( pConVar );
				event->SetString( "hostname", var.GetString() );
				g_GameEventManager.FireEvent( event );
			}
		}
	}
}
ConVar host_name( "hostname", "", FCVAR_RELEASE, "Hostname for server.", false, 0.0f, false, 0.0f, HostnameChanged );
ConVar host_map( "host_map", "", FCVAR_RELEASE, "Current map name." );

bool CanShowHostTvStatus()
{
	if ( !serverGameDLL )
		return true;

	if ( serverGameDLL->IsValveDS() )
	{
		// By default OFFICIAL server will NOT print TV information in "status" output
		// Running with -display_tv_status will reveal GOTV information
		static bool s_bCanShowHostTvStatusOFFICIAL = !!CommandLine()->FindParm( "-display_tv_status" );
		return s_bCanShowHostTvStatusOFFICIAL;
	}
	else
	{
		// By default COMMUNITY server will print TV information in "status" output
		// Running with -disable_tv_status will conceal GOTV information
		static bool s_bCanShowHostTvStatusCOMMUNITY = !CommandLine()->FindParm( "-disable_tv_status" );
		return s_bCanShowHostTvStatusCOMMUNITY;
	}
}

#ifdef _PS3
ConVar ps3_host_quit_graceperiod( "ps3_host_quit_graceperiod", "7", FCVAR_DEVELOPMENTONLY, "Time granted for save operations to finish" );
ConVar ps3_host_quit_debugpause( "ps3_host_quit_debugpause", "0", FCVAR_DEVELOPMENTONLY, "Time to stall quit for debug purposes" );
#endif

ConVar voice_recordtofile("voice_recordtofile", "0", FCVAR_RELEASE, "Record mic data and decompressed voice data into 'voice_micdata.wav' and 'voice_decompressed.wav'");
ConVar voice_inputfromfile("voice_inputfromfile", "0",FCVAR_RELEASE, "Get voice input from 'voice_input.wav' rather than from the microphone.");

uint GetSteamAppID()
{
#ifndef DEDICATED
	if ( Steam3Client().SteamUtils() )
		return Steam3Client().SteamUtils()->GetAppID();
#endif

	if ( Steam3Server().SteamGameServerUtils() )
		return Steam3Server().SteamGameServerUtils()->GetAppID();

	return 215;	// defaults to Source SDK Base (215) if no steam.inf can be found.
}

EUniverse GetSteamUniverse()
{
#ifndef DEDICATED
	if ( Steam3Client().SteamUtils() )
		return Steam3Client().SteamUtils()->GetConnectedUniverse();
#endif

	if ( Steam3Server().SteamGameServerUtils() )
		return Steam3Server().SteamGameServerUtils()->GetConnectedUniverse();

	return k_EUniverseInvalid;
}

// Globals
int	gHostSpawnCount = 0;

// If any quit handlers balk, then aborts quit sequence
bool EngineTool_CheckQuitHandlers();

#if defined( _GAMECONSOLE )
void Host_Quit_f (void);
void PS3_sysutil_callback_forwarder( uint64 uiStatus, uint64 uiParam );
void Quit_gameconsole_f( bool bWarmRestart, bool bUnused )
{
#if defined( _DEMO )
	if ( Host_IsDemoExiting() )
	{
		// for safety, only want to play this under demo exit conditions
		// which guaranteed us a safe exiting context
		game->PlayVideoListAndWait( "media/DemoUpsellVids.txt" );
	}
#endif

#if defined( _X360 ) && defined( _DEMO )
	// demo version has to support variants of the launch structures
	// demo version must reply with exact demo launch structure if provided
	unsigned int launchID;
	int launchSize;
	void *pLaunchData;
	bool bValid = XboxLaunch()->GetLaunchData( &launchID, &pLaunchData, &launchSize );
	if ( bValid && Host_IsDemoHostedFromShell() )
	{
		XboxLaunch()->SetLaunchData( pLaunchData, launchSize, LF_UNKNOWNDATA );
		g_pMaterialSystem->PersistDisplay();
		XBX_DisconnectConsoleMonitor();

		const char *pImageName = XLAUNCH_KEYWORD_DEFAULT_APP;
		if ( launchID == LAUNCH_DATA_DEMO_ID )
		{
			pImageName = ((LD_DEMO*)pLaunchData)->szLauncherXEX;
		}
		XboxLaunch()->Launch( pImageName );
		return;
	}
#endif

#ifdef _X360
	// must be first, will cause a reset of the launch if we have never been re-launched
	// all further XboxLaunch() operations MUST be writes, otherwise reset
	int launchFlags = LF_EXITFROMGAME;

	// block until the installer stops
	g_pXboxInstaller->IsStopped( true );
	if ( g_pXboxInstaller->IsFullyInstalled() )
	{
		launchFlags |= LF_INSTALLEDTOCACHE;
	}

	// allocate the full payload
	int nPayloadSize = XboxLaunch()->MaxPayloadSize();
	byte *pPayload = (byte *)stackalloc( nPayloadSize );
	V_memset( pPayload, 0, sizeof( nPayloadSize ) );

	// payload is at least the command line
	// any user data needed must be placed AFTER the command line
	const char *pCmdLine = CommandLine()->GetCmdLine();
	int nCmdLineLength = (int)strlen( pCmdLine ) + 1;
	V_memcpy( pPayload, pCmdLine, min( nPayloadSize, nCmdLineLength ) );

	// add any other data here to payload, after the command line
	// ...

	// Collect settings to preserve across restarts
	int numGameUsers = XBX_GetNumGameUsers();
	char slot2ctrlr[4];
	char slot2guest[4];
	int ctrlr2storage[4];

	for ( int k = 0; k < 4; ++ k )
	{
		slot2ctrlr[k] = (char) XBX_GetUserId( k );
		slot2guest[k] = (char) XBX_GetUserIsGuest( k );
		ctrlr2storage[k] = XBX_GetStorageDeviceId( k );
	}

	// storage device may have changed since previous launch
	XboxLaunch()->SetStorageID( ctrlr2storage );

	// Close the storage devices
	g_pXboxSystem->CloseAllContainers();

	DWORD nUserID = XBX_GetPrimaryUserId();
	XboxLaunch()->SetUserID( nUserID );
	XboxLaunch()->SetSlotUsers( numGameUsers, slot2ctrlr, slot2guest );

	if ( bWarmRestart )
	{
		// a restart is an attempt at a hidden reboot-in-place
		launchFlags |= LF_WARMRESTART;
	}

	// set our own data and relaunch self
	bool bLaunch = XboxLaunch()->SetLaunchData( pPayload, nPayloadSize, launchFlags );
#if defined( _DEMO )
	bLaunch = true;
#endif
	if ( bLaunch )
	{
		// Can't send anything to VXConsole; about to abandon connection
		// VXConsole tries to respond but can't and throws the timeout crash
//		COM_TimestampedLog( "Launching: \"%s\" Flags: 0x%8.8x", pCmdLine, XboxLaunch()->GetLaunchFlags() );
		g_pMaterialSystem->PersistDisplay();
		XBX_DisconnectConsoleMonitor();
#if defined( CSTRIKE15 )
		XboxLaunch()->SetLaunchData( NULL, 0, 0 );
		XboxLaunch()->Launch( XLAUNCH_KEYWORD_DASH_ARCADE );
#else
		XboxLaunch()->Launch();
#endif // defined( CSTRIKE15 )
	}
#elif defined( _PS3 )
	// TODO: preserve when a "restart" is requested!
	Assert( !bWarmRestart );
	Assert( !bUnused );
	if ( bWarmRestart )
	{
		DevWarning( "TODO: PS3 quit_x360 restart is not implemented yet!\n" );
	}

	// Prevent re-entry
	static bool s_bQuitPreventReentry = false;
	if ( s_bQuitPreventReentry )
		return;
	s_bQuitPreventReentry = true;

	// We must go into single-threaded rendering
	Host_AllowQueuedMaterialSystem( false );
	
	// Make sure everybody received the EXITGAME callback, might happen multiple times now
	float const flTimeStampStart = Plat_FloatTime();
	float flGracePeriod = ps3_host_quit_graceperiod.GetFloat();
	flGracePeriod = MIN( 7, flGracePeriod );
	flGracePeriod = MAX( 0, flGracePeriod );
	float const flTimeStampForceShutdown = flTimeStampStart + flGracePeriod;
	uint64 uiLastCountdownNotificationSent = 0;
	for ( ; ; )
	{
		enum ShutdownSystemsWait_t
		{
			kSysSaveRestore,
			kSysSaveUtilV2,
			kSysSteamClient,
			kSysDebugPause,
			kSysShutdownSystemsCount
		};
		char const *szSystems[kSysShutdownSystemsCount] = {0};
		char const *szSystemsRequiredState[kSysShutdownSystemsCount] = {0};
		
		// Poll systems whether they are ready to shutdown
		if ( saverestore && saverestore->IsSaveInProgress() )
			szSystems[kSysSaveRestore] = "saverestore";
		extern bool SaveUtilV2_CanShutdown();
		if ( !SaveUtilV2_CanShutdown() )
			szSystems[kSysSaveUtilV2] = "SaveUtilV2";
		if ( Steam3Client().SteamUtils() && !Steam3Client().SteamUtils()->BIsReadyToShutdown() )
			szSystems[kSysSteamClient] = "steamclient";
		if ( ( ps3_host_quit_debugpause.GetFloat() > 0 ) && ( Plat_FloatTime() < flTimeStampStart + ps3_host_quit_debugpause.GetFloat() ) )
			szSystems[kSysDebugPause] = "debugpause";

		if ( !Q_memcmp( szSystemsRequiredState, szSystems, sizeof( szSystemsRequiredState ) ) )
		{
			DevMsg( "PS3 shutdown procedure: all systems ready (%.2f sec elapsed)\n", ( Plat_FloatTime() - flTimeStampStart ) );
			break;
		}

		uint64 uiCountdownNotification = 1 + ( flTimeStampForceShutdown - Plat_FloatTime() );
		if ( uiCountdownNotification != uiLastCountdownNotificationSent )
		{
			uiLastCountdownNotificationSent = uiCountdownNotification;
			PS3_sysutil_callback_forwarder( CELL_SYSUTIL_REQUEST_EXITGAME, uiCountdownNotification );
			DevWarning( "PS3 shutdown procedure: %.2f sec elapsed...\n", ( Plat_FloatTime() - flTimeStampStart ) );
			int nNotReadySystemsCount = 0;
			for ( int jj = 0; jj < ARRAYSIZE( szSystems ); ++ jj )
			{
				if ( szSystems[jj] )
				{
					DevWarning( "    system not ready  : %s\n", szSystems[jj] );
					++ nNotReadySystemsCount;
				}
			}
			DevWarning( "PS3 shutdown procedure: waiting for %d systems to be ready for shutdown (%.2f sec remaining)...\n", nNotReadySystemsCount, ( flTimeStampForceShutdown - Plat_FloatTime() ) );
		}

		if ( Plat_FloatTime() >= flTimeStampForceShutdown )
		{
			DevWarning( "FORCING PS3 SHUTDOWN PROCEDURE: NOT ALL SYSTEMS READY (%.2f sec elapsed)...\n", ( Plat_FloatTime() - flTimeStampStart ) );
			break;
		}

		// Perform blank vsync'ed flips
		static ConVarRef mat_vsync( "mat_vsync" );
		mat_vsync.SetValue( true );
		g_pMaterialSystem->SetFlipPresentFrequency( 1 ); // let it flip every VSYNC, we let interrupt handler throttle this loop to conform with TCR#R092 [no more than 60 fps]
		
		// Dummy frame
		g_pMaterialSystem->BeginFrame( 1.0f/60.0f );
		CMatRenderContextPtr pRenderContext;
		pRenderContext.GetFrom( g_pMaterialSystem );
		pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
		pRenderContext->ClearBuffers( true, true, true );
		pRenderContext.SafeRelease();
		g_pMaterialSystem->EndFrame();
		g_pMaterialSystem->SwapBuffers();

		// Pump system event queue
		XBX_ProcessEvents();
		XBX_DispatchEventsQueue();
	}
	
	// QUIT
	Warning( "[PS3 SYSTEM] REQUEST EXITGAME INITIATING QUIT @ %.3f\n", Plat_FloatTime() );
	Host_Quit_f();
#else
	Assert( 0 );
#error
#endif
}

CON_COMMAND( quit_gameconsole, "" )
{
	Quit_gameconsole_f( 
		args.FindArg( "restart" ) != NULL, 
		args.FindArg( "invite" ) != NULL );
}
#endif

// store arbitrary launch arguments in KeyValues to avoid having to add code for every new
//   launch parameter (like edit mode, commentary mode, background, etc. do)
void SetLaunchOptions( const CCommand &args )
{
	if ( g_pLaunchOptions )
	{
		g_pLaunchOptions->deleteThis();
	}
	g_pLaunchOptions = new KeyValues( "LaunchOptions" );
	for ( int i = 0 ; i < args.ArgC() ; i++ )
	{
		g_pLaunchOptions->SetString( va("Arg%d", i), args[i] );
	}
}

/*
==================
Host_Quit_f
==================
*/
void Host_Quit_f (void)
{
#if !defined(DEDICATED)
	if ( !EngineTool_CheckQuitHandlers() )
	{
		return;
	}
#endif

	HostState_Shutdown();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( _restart, "Shutdown and restart the engine." )
{
	/*
	// FIXME:  How to handle restarts?
#ifndef DEDICATED
	if ( !EngineTool_CheckQuitHandlers() )
	{
		return;
	}
#endif
	*/

	HostState_Restart();
}

#ifndef DEDICATED
//-----------------------------------------------------------------------------
// A console command to spew out driver information
//-----------------------------------------------------------------------------
void Host_LightCrosshair (void);

static ConCommand light_crosshair( "light_crosshair", Host_LightCrosshair, "Show texture color at crosshair", FCVAR_CHEAT );

void Host_LightCrosshair (void)
{
	Vector endPoint;
	Vector lightmapColor;

	// max_range * sqrt(3)
	VectorMA( MainViewOrigin(), COORD_EXTENT * 1.74f, MainViewForward(), endPoint );
	
	R_LightVec( MainViewOrigin(), endPoint, true, lightmapColor );
	int r = LinearToTexture( lightmapColor.x );
	int g = LinearToTexture( lightmapColor.y );
	int b = LinearToTexture( lightmapColor.z );

	ConMsg( "Luxel Value: %d %d %d\n", r, g, b );
}
#endif

/*
==================
Host_Status_PrintClient

Print client info to console 
==================
*/
void Host_Status_PrintClient( IClient *client, bool bShowAddress, void (*print) (const char *fmt, ...) )
{
	INetChannelInfo *nci = client->GetNetChannel();

	const char *state = "challenging";

	if ( client->IsActive() )
		state = "active";
	else if ( client->IsSpawned() )
		state = "spawning";
	else if ( client->IsConnected() )
		state = "connecting";
	
	if ( nci != NULL )
	{
		print( "# %2i %i \"%s\" %s %s %i %i %s %d", 
			client->GetUserID(), client->GetPlayerSlot() + 1, client->GetClientName(), client->GetNetworkIDString(), COM_FormatSeconds( nci->GetTimeConnected() ),
			(int)(1000.0f*nci->GetAvgLatency( FLOW_OUTGOING )), (int)(100.0f*nci->GetAvgLoss(FLOW_INCOMING)), state, (int)nci->GetDataRate() );

		if ( bShowAddress ) 
		{
			print( " %s", nci->GetAddress() );
		}
	}
	else
	{
		print( "#%2i \"%s\" %s %s %.0f", 
			client->GetUserID(), client->GetClientName(), client->GetNetworkIDString(), state, client->GetUpdateRate() );
	}
	
	print( "\n" );

}

typedef void ( *FnPrintf_t )(const char *fmt, ...);

void Host_Client_Printf(const char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,fmt);
	Q_vsnprintf (string, sizeof( string ), fmt,argptr);
	va_end (argptr);

	host_client->ClientPrintf( "%s", string );
}

static void Host_Client_PrintfStub(const char *fmt, ...)
{
}

void Host_PrintStatus( cmd_source_t commandSource, void ( *print )(const char *fmt, ...), bool bShort )
{
	bool bWithAddresses = ( ( commandSource != kCommandSrcNetClient ) && ( commandSource != kCommandSrcNetServer ) && ( print == ConMsg ) ); // guarantee to never print for remote

	IClient	*client;
	int j;

	if ( !print ) { return; }

	// ============================================================
	// Server status information.
	print( "hostname: %s\n", host_name.GetString() );

	const char *pchSecureReasonString = "";
	const char *pchUniverse = "";
	bool bGSSecure = Steam3Server().BSecure();
	if ( !bGSSecure && Steam3Server().BWantsSecure() )
	{
		if ( Steam3Server().BLoggedOn() )
		{
			pchSecureReasonString = "(secure mode enabled, connected to Steam3)";
		}
		else
		{
			pchSecureReasonString = "(secure mode enabled, disconnected from Steam3)";
		}
	}

	switch ( GetSteamUniverse() )
	{
		case k_EUniversePublic:
			pchUniverse = "";
			break;
		case k_EUniverseBeta:
			pchUniverse = "(beta)";
			break;
		case k_EUniverseInternal:
			pchUniverse = "(internal)";
			break;
		case k_EUniverseDev:
			pchUniverse = "(dev)";
			break;
			/* no such universe anymore
		case k_EUniverseRC:
			pchUniverse = "(rc)";
			break;
			*/
		default:
			pchUniverse = "(unknown)";
			break;
	}
	
	if ( bWithAddresses )
	{
		print( "version : %s/%d %d/%d %s %s %s %s\n",
			Sys_GetVersionString(), GetHostVersion(),
			GetServerVersion(), build_number(),
			bGSSecure ? "secure" : "insecure", pchSecureReasonString,
			Steam3Server().GetGSSteamID().IsValid() ? Steam3Server().GetGSSteamID().Render() : "[INVALID_STEAMID]", pchUniverse );
	}
	else
	{
		print( "version : %s %s\n",
			Sys_GetVersionString(),
			bGSSecure ? "secure" : "insecure" );
	}

	if ( NET_IsMultiplayer() )
	{
		CUtlString sPublicIPInfo;
		if ( !Steam3Server().BLanOnly() )
		{
			uint32 unPublicIP = Steam3Server().GetPublicIP();
			if ( ( unPublicIP != 0 ) && bWithAddresses && sv.IsDedicated() )
			{
				netadr_t addr;
				addr.SetIP( unPublicIP );
				sPublicIPInfo.Format("  (public ip: %s)", addr.ToString( true ) );
			}
		}
		print( "udp/ip  : %s:%i%s\n", net_local_adr.ToString(true), sv.GetUDPPort(), sPublicIPInfo.String() );
		static ConVarRef sv_steamdatagramtransport_port( "sv_steamdatagramtransport_port" );
		if ( bWithAddresses && sv_steamdatagramtransport_port.GetInt() > 0 )
		{
			print( "sdt     : =%s on port %d\n", Steam3Server().GetGSSteamID().Render(), sv_steamdatagramtransport_port.GetInt() );
		}

		const char *osType =
#if defined( WIN32 )
			"Windows";
#elif defined( _LINUX )
		"Linux";
#elif defined( PLATFORM_OSX )
		"OSX";
#else
		"Unknown";
#endif

		print( "os      :  %s\n", osType );

		char const *serverType = sv.IsHLTV() ? "hltv" : ( sv.IsDedicated() ? ( serverGameDLL->IsValveDS() ? "official dedicated" : "community dedicated" ) : "listen" );
		print( "type    :  %s\n", serverType );
	}

#ifndef DEDICATED												// no client on dedicated server
	if ( !sv.IsDedicated() && GetBaseLocalClient().IsConnected() )
	{
		print( "map     : %s at: %d x, %d y, %d z\n", sv.GetMapName(), (int)MainViewOrigin()[0], (int)MainViewOrigin()[1], (int)MainViewOrigin()[2]);
	}
#else
	{
		print( "map     : %s\n", sv.GetMapName() );
	}
#endif

	if ( CanShowHostTvStatus() )
	{
		for ( CActiveHltvServerIterator hltv; hltv; hltv.Next() )
		{
			print( "gotv[%i]:  port %i, delay %.1fs, rate %.1f\n", hltv.GetIndex(), hltv->GetUDPPort(), hltv->GetDirector() ? hltv->GetDirector()->GetDelay() : 0.0f, hltv->GetSnapshotRate() );
		}
	}

#if defined( REPLAY_ENABLED )
	if ( replay && replay->IsActive() )
	{
		print( "replay:  port %i, delay %.1fs\n", replay->GetUDPPort(), replay->GetDirector()->GetDelay() );
	}
#endif

	int nHumans;
	int nMaxHumans;
	int nBots;

	sv.GetMasterServerPlayerCounts( nHumans, nMaxHumans, nBots );

	print( "players : %i humans, %i bots (%i/%i max) (%s)\n\n",
		nHumans, nBots, nMaxHumans, sv.GetNumGameSlots(), sv.IsHibernating() ? "hibernating" : "not hibernating" );
	// ============================================================
#if SUPPORT_NET_CONSOLE
	if ( g_pNetConsoleMgr && g_pNetConsoleMgr->IsActive() && bWithAddresses )
	{
		print( "netcon  :  %s:%i\n", net_local_adr.ToString( true), g_pNetConsoleMgr->GetAddress().GetPort() );
	}
#endif

	// Early exit for this server.
	if ( bShort )
	{
		for ( j=0 ; j < sv.GetClientCount() ; j++ )
		{
			client = sv.GetClient( j );

			if ( !client->IsActive() )
				continue;

			if ( !bWithAddresses && !CanShowHostTvStatus() && client->IsHLTV() )
				continue;

			print( "#%i - %s\n" , j + 1, client->GetClientName() );
		}
		return;
	}

	// the header for the status rows
	print( "# userid name uniqueid connected ping loss state rate" );
	if ( bWithAddresses )
	{
		print( " adr" ); 
	}
	print( "\n" );

	for ( j=0 ; j < sv.GetClientCount() ; j++ )
	{
		client = sv.GetClient( j );

		if ( !client->IsConnected() )
			continue; // not connected yet, maybe challenging

		if ( !CanShowHostTvStatus() && client->IsHLTV() )
			continue;
		
		Host_Status_PrintClient( client, bWithAddresses, print );
	}
	print( "#end\n" );

}

//-----------------------------------------------------------------------------
// Host_Status_f
//-----------------------------------------------------------------------------
CON_COMMAND( status, "Display map and connection status." )
{
	void (*print) (const char *fmt, ...) = Host_Client_PrintfStub;

	if ( args.Source() != kCommandSrcNetClient )
	{
		if ( !sv.IsActive() )
		{
			Cmd_ForwardToServer( args );
			return;
		}
#ifndef DBGFLAG_STRINGS_STRIP
		print = ConMsg;
#endif
	}
	else
	{
		print = Host_Client_Printf;
	}

	bool bShort = false;
	if ( args.ArgC() == 2 )
	{
		if ( !Q_stricmp( args[1], "short" ) )
		{
			bShort = true;
		}
	}

	Host_PrintStatus( args.Source(), print, bShort );
}

CON_COMMAND( hltv_replay_status, "Show Killer Replay status and some statistics, works on listen or dedicated server." )
{
	HltvReplayStats_t hltvStats;

	for ( int j = 0; j < sv.GetClientCount(); j++ )
	{
		IClient *client = sv.GetClient( j );

		if ( !client->IsConnected() )
			continue; // not connected yet, maybe challenging

		if ( !CanShowHostTvStatus() && client->IsHLTV() )
			continue;

		INetChannelInfo *nci = client->GetNetChannel();

		const char *state = "challenging";

		if ( client->IsActive() )
			state = "active";
		else if ( client->IsSpawned() )
			state = "spawning";
		else if ( client->IsConnected() )
			state = "connecting";

		if ( nci != NULL )
		{
			ConMsg( "# %2i %i \"%s\" %s %s %s %s",
				client->GetUserID(), client->GetPlayerSlot() + 1, client->GetClientName(), client->GetNetworkIDString(), COM_FormatSeconds( nci->GetTimeConnected() ),
				state, client->GetHltvReplayStatus() );
			if ( client->GetHltvReplayDelay() )
				ConMsg( ", in replay NOW" );
		}
		else
		{
			ConMsg( "#%2i \"%s\" %s %s %s",
				client->GetUserID(), client->GetClientName(), client->GetNetworkIDString(), state, client->GetHltvReplayStatus() );
		}

		if ( CGameClient *pClient = dynamic_cast< CGameClient * >( client ) )
		{
			if ( pClient->m_HltvReplayStats.nStartRequests )
				hltvStats += pClient->m_HltvReplayStats;
			if ( pClient->m_nForceWaitForTick > 0 )
			{
				ConMsg( ", force-waiting for tick %d - server tick %d, current frame %d", pClient->m_nForceWaitForTick, sv.GetTick(), pClient->m_pCurrentFrame ? pClient->m_pCurrentFrame->tick_count : 0 );
			}
		}

		ConMsg( "\n" );
	}

	extern HltvReplayStats_t m_DisconnectedClientsHltvReplayStats;
	if ( m_DisconnectedClientsHltvReplayStats.nClients > 1 )
		ConMsg( "%u disconnected clients: %s\n", m_DisconnectedClientsHltvReplayStats.nClients - 1, m_DisconnectedClientsHltvReplayStats.AsString() );

	if ( hltvStats.nClients > 0 )
	{
		ConMsg( "%u current clients: %s\n", hltvStats.nClients, hltvStats.AsString() );
	}
}



#if defined( _X360 )
CON_COMMAND( vx_mapinfo, "" )
{
	Vector org;
	QAngle ang;
	const char *pName;

	if ( GetBaseLocalClient().IsActive() )
	{
		pName = GetBaseLocalClient().m_szLevelNameShort;
		org = MainViewOrigin();
		VectorAngles( MainViewForward(), ang );
		IClientEntity *localPlayer = entitylist->GetClientEntity( GetBaseLocalClient().m_nPlayerSlot + 1 );
		if ( localPlayer )
		{
			org = localPlayer->GetAbsOrigin();
		}
	}
	else
	{
		pName = "";
		org.Init();
		ang.Init();
	}

	// HACK: This is only relevant for portal2. 
	Msg( "BUG REPORT PORTAL POSITIONS:\n" );
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), "portal_report\n" );

	// send to vxconsole
	xMapInfo_t mapInfo;
	mapInfo.position[0] = org[0];
	mapInfo.position[1] = org[1];
	mapInfo.position[2] = org[2];
	mapInfo.angle[0]    = ang[0];
	mapInfo.angle[1]    = ang[1];
	mapInfo.angle[2]    = ang[2];
	mapInfo.build       = build_number();
	mapInfo.skill       = skill.GetInt();

	// generate the qualified path where .sav files are expected to be written
	char savePath[MAX_PATH];
	V_snprintf( savePath, sizeof( savePath ), "%s", saverestore->GetSaveDir() );
	V_StripTrailingSlash( savePath );
	g_pFileSystem->RelativePathToFullPath( savePath, "MOD", mapInfo.savePath, sizeof( mapInfo.savePath ) );
	V_FixSlashes( mapInfo.savePath );

	if ( pName[0] )
	{
		// generate the qualified path from where the map was loaded
		char mapPath[MAX_PATH];
		V_snprintf( mapPath, sizeof( mapPath ), "maps/%s" PLATFORM_EXT ".bsp", pName );
		g_pFileSystem->GetLocalPath( mapPath, mapInfo.mapPath, sizeof( mapInfo.mapPath ) );
		V_FixSlashes( mapInfo.mapPath );
	}
	else
	{
		mapInfo.mapPath[0] = '\0';
	}

	mapInfo.details[0] = '\0';

	ConVarRef host_thread_mode( "host_thread_mode" );
	ConVarRef mat_queue_mode( "mat_queue_mode" );
	ConVarRef snd_surround_speakers( "snd_surround_speakers" );

	V_strncat(
		mapInfo.details,
		CFmtStr( "Build: %d\n", build_number() ),
		sizeof( mapInfo.details ) );

	XVIDEO_MODE videoMode;
	XGetVideoMode( &videoMode );
	V_strncat(
		mapInfo.details,
		CFmtStr( "Display: %dx%d (%s)\n", videoMode.dwDisplayWidth, videoMode.dwDisplayHeight, videoMode.fIsWideScreen ? "widescreen" : "normal" ),
		sizeof( mapInfo.details ) );

	int backbufferWidth, backbufferHeight;
	materials->GetBackBufferDimensions( backbufferWidth, backbufferHeight );
	V_strncat(
		mapInfo.details,
		CFmtStr( "BackBuffer: %dx%d\n", backbufferWidth, backbufferHeight ),
		sizeof( mapInfo.details ) );

	// audio info
	const char *pAudioInfo = "Unknown";
	switch ( snd_surround_speakers.GetInt() )
	{
	case 2:
		pAudioInfo = "Stereo";
		break;
	case 5:
		pAudioInfo = "5.1 Digital Surround";
		break;
	}
	V_strncat(
		mapInfo.details,
		CFmtStr( "Audio: %s\n", pAudioInfo ),
		sizeof( mapInfo.details ) );

	// ui language
	V_strncat(
		mapInfo.details,
		CFmtStr( "UI: %s\n", cl_language.GetString() ),
		sizeof( mapInfo.details ) );

	// cvars
	V_strncat(
		mapInfo.details,
		CFmtStr( "host_thread_mode: %d\n", host_thread_mode.GetInt() ),
		sizeof( mapInfo.details ) );
	V_strncat(
		mapInfo.details,
		CFmtStr( "mat_queue_mode: %d\n", mat_queue_mode.GetInt() ),
		sizeof( mapInfo.details ) );

	XBX_rMapInfo( &mapInfo );
}
#elif defined( _PS3 )
#include "ps3/ps3_sn.h"
CON_COMMAND( vx_mapinfo, "" )
{
	Vector org;
	QAngle ang;
	const char *pName;

	if ( GetBaseLocalClient().IsActive() )
	{
		pName = GetBaseLocalClient().m_szLevelNameShort;
		org = MainViewOrigin();
		VectorAngles( MainViewForward(), ang );
		IClientEntity *localPlayer = entitylist->GetClientEntity( GetBaseLocalClient().m_nPlayerSlot + 1 );
		if ( localPlayer )
		{
			org = localPlayer->GetAbsOrigin();
		}
	}
	else
	{
		pName = "";
		org.Init();
		ang.Init();
	}

	// HACK: This is only relevant for portal2. 
	Msg( "BUG REPORT PORTAL POSITIONS:\n" );
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), "portal_report\n" );

	// send to vxconsole
	xMapInfo_t mapInfo;
	mapInfo.position[0] = org[0];
	mapInfo.position[1] = org[1];
	mapInfo.position[2] = org[2];
	mapInfo.angle[0]    = ang[0];
	mapInfo.angle[1]    = ang[1];
	mapInfo.angle[2]    = ang[2];
	mapInfo.build       = build_number();
	mapInfo.skill       = skill.GetInt();

	// generate the qualified path where .sav files are expected to be written
	char savePath[MAX_PATH];
	V_snprintf( savePath, sizeof( savePath ), "%s", saverestore->GetSaveDir() );
	V_StripTrailingSlash( savePath );
	g_pFileSystem->RelativePathToFullPath( savePath, "MOD", mapInfo.savePath, sizeof( mapInfo.savePath ) );
	V_FixSlashes( mapInfo.savePath );

	if ( pName[0] )
	{
		// generate the qualified path from where the map was loaded
		char mapPath[MAX_PATH];
		V_snprintf( mapPath, sizeof( mapPath ), "maps/%s" PLATFORM_EXT ".bsp", pName );
		g_pFileSystem->GetLocalPath( mapPath, mapInfo.mapPath, sizeof( mapInfo.mapPath ) );
		V_FixSlashes( mapInfo.mapPath );
	}
	else
	{
		mapInfo.mapPath[0] = '\0';
	}

	mapInfo.details[0] = '\0';

	ConVarRef host_thread_mode( "host_thread_mode" );
	ConVarRef mat_queue_mode( "mat_queue_mode" );
	ConVarRef snd_surround_speakers( "snd_surround_speakers" );

	V_strncat(
		mapInfo.details,
		CFmtStr( "Build: %d\n", build_number() ),
		sizeof( mapInfo.details ) );

	/*
	XVIDEO_MODE videoMode;
	XGetVideoMode( &videoMode );
	V_strncat(
		mapInfo.details,
		CFmtStr( "Display: %dx%d (%s)\n", videoMode.dwDisplayWidth, videoMode.dwDisplayHeight, videoMode.fIsWideScreen ? "widescreen" : "normal" ),
		sizeof( mapInfo.details ) );

	int backbufferWidth, backbufferHeight;
	materials->GetBackBufferDimensions( backbufferWidth, backbufferHeight );
	V_strncat(
		mapInfo.details,
		CFmtStr( "BackBuffer: %dx%d\n", backbufferWidth, backbufferHeight ),
		sizeof( mapInfo.details ) );
	*/

	// audio info
	const char *pAudioInfo = "Unknown";
	switch ( snd_surround_speakers.GetInt() )
	{
	case 2:
		pAudioInfo = "Stereo";
		break;
	case 5:
		pAudioInfo = "5.1 Digital Surround";
		break;
	}
	V_strncat(
		mapInfo.details,
		CFmtStr( "Audio: %s\n", pAudioInfo ),
		sizeof( mapInfo.details ) );

	// ui language
	V_strncat(
		mapInfo.details,
		CFmtStr( "UI: %s\n", cl_language.GetString() ),
		sizeof( mapInfo.details ) );

	// cvars
	V_strncat(
		mapInfo.details,
		CFmtStr( "host_thread_mode: %d\n", host_thread_mode.GetInt() ),
		sizeof( mapInfo.details ) );
	V_strncat(
		mapInfo.details,
		CFmtStr( "mat_queue_mode: %d\n", mat_queue_mode.GetInt() ),
		sizeof( mapInfo.details ) );

	XBX_rMapInfo( &mapInfo );
}


CON_COMMAND( vx_screenshot, "" )
{
#if 1
	g_pMaterialSystem->TransmitScreenshotToVX( );
#else
	// COMPILE_TIME_ASSERT( sizeof(g_pfnSwapBufferMarker) == 8);
	union FunctionPointerIsReallyADescriptor
	{
		void (*pFunc_t)();
		struct
		{
			uint32 funcaddress;
			int32 iToc;
		} fn8;
	};
	
	FunctionPointerIsReallyADescriptor *pBreakpoint = (FunctionPointerIsReallyADescriptor *)g_pfnSwapBufferMarker;

	// breakpoint.pFunc_t = g_pfnSwapBufferMarker;

	uint64	uBPAddress;
	/// Address of a pointer that points to the image in memory
	char *		pFrameBuffer;
	/// Width of image
	uint32		uWidth;
	/// Height of image
	uint32		uHeight;
	/// Image pitch (as described in CellGCMSurface) - in bytes
	uint32		uPitch;
	/// Image colour settings (0 = X8R8G8B8, 1 = X8B8G8R8, 2 = R16G16B16X16)
	IMaterialSystem::VRAMScreenShotInfoColor_t		colour	;

	// get one of the screen buffers. Since we breakpoint the game anyway I don't think 
	// it really matters if we're two out of date. (For this test, anyway.)
	g_pMaterialSystem->GetVRAMScreenShotInfo( &pFrameBuffer, &uWidth, &uHeight, &uPitch, &colour );
	g_pValvePS3Console->VRAMDumpingInfo( (uint64)pBreakpoint->fn8.funcaddress,
		(uint64)pFrameBuffer, uWidth, uHeight, uPitch, colour );
#endif
}	
#endif


//-----------------------------------------------------------------------------
// Host_Ping_f
//-----------------------------------------------------------------------------
CON_COMMAND( ping, "Display ping to server." )
{
	if ( args.Source() != kCommandSrcNetClient )
	{
		Cmd_ForwardToServer( args );
		return;
	}

	host_client->ClientPrintf( "Client ping times:\n" );

	for ( int i=0; i< sv.GetClientCount(); i++ )
	{
		IClient	*client = sv.GetClient(i);

		if ( !client->IsConnected() || client->IsFakeClient() )
			continue;

		host_client->ClientPrintf ("%4.0f ms : %s\n", 
			1000.0f * client->GetNetChannel()->GetAvgLatency( FLOW_OUTGOING ), client->GetClientName() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : editmode - 
//-----------------------------------------------------------------------------
extern void GetPlatformMapPath( const char *pMapPath, char *pPlatformMapPath, int maxLength );

bool CL_HL2Demo_MapCheck( const char *name )
{
	if ( IsPC() && CL_IsHL2Demo() && !sv.IsDedicated() )
	{
		if (    !Q_stricmp( name, "d1_trainstation_01" ) || 
				!Q_stricmp( name, "d1_trainstation_02" ) || 
				!Q_stricmp( name, "d1_town_01" ) || 
				!Q_stricmp( name, "d1_town_01a" ) || 
				!Q_stricmp( name, "d1_town_02" ) || 
				!Q_stricmp( name, "d1_town_03" ) ||
				!Q_stricmp( name, "background01" ) ||
				!Q_stricmp( name, "background03" ) 
			)
		{
			return true;
		}
		return false;
	}

	return true;
}

bool CL_PortalDemo_MapCheck( const char *name )
{
	if ( IsPC() && CL_IsPortalDemo() && !sv.IsDedicated() )
	{
		if (    !Q_stricmp( name, "testchmb_a_00" ) || 
				!Q_stricmp( name, "testchmb_a_01" ) || 
				!Q_stricmp( name, "testchmb_a_02" ) || 
				!Q_stricmp( name, "testchmb_a_03" ) || 
				!Q_stricmp( name, "testchmb_a_04" ) || 
				!Q_stricmp( name, "testchmb_a_05" ) ||
				!Q_stricmp( name, "testchmb_a_06" ) ||
				!Q_stricmp( name, "background1" ) 
			)
		{
			return true;
		}
		return false;
	}

	return true;
}

enum EMapFlags
{
	EMAP_NONE = 0,

	EMAP_EDIT_MODE = (1<<0),
	EMAP_BACKGROUND = (1<<1),
	EMAP_COMMENTARY = (1<<2),
	EMAP_SPLITSCREEN = (1<<3)

};

int _Host_Map_f_CompletionFunc( char const *cmdname, char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] );

// Note, leaves name alone if no match possible
static bool Host_Map_Helper_FuzzyName( const CCommand &args, char *name, size_t bufsize )
{
	char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ];
	CUtlString argv0;
	argv0 = args.Arg( 0 );
	argv0 += " ";

	if ( _Host_Map_f_CompletionFunc( argv0, args[1], commands ) > 0 )
	{
		Q_strncpy( name, &commands[ 0 ][ argv0.Length() ], bufsize );
		return true;
	}
	return false;
}

void Host_Changelevel_f( const CCommand &args );
void Host_Map_Helper( const CCommand &args, EMapFlags flags )
{
	char	name[MAX_QPATH];

	if (args.ArgC() < 2)
	{
		Warning("No map specified\n");
		return;
	}

	if ( ( sv.IsActive() && !sv.IsSinglePlayerGame() && !sv.IsLevelMainMenuBackground() ) ||
		 ( sv.IsActive() && sv.IsDedicated() ) )
	{
		// Using the 'map' command while in a map disconnects all players.
		// Ease the pain of this common error by forwarding to the correct command.
		Host_Changelevel_f( args );
		return;
	}
	
	bool bBackground = ( flags & EMAP_BACKGROUND ) != 0;
	bool bSplitScreenConnect = ( flags & EMAP_SPLITSCREEN ) != 0;

	char ppath[ MAX_QPATH ];

	// If there is a .bsp on the end, strip it off!
	Q_StripExtension( args[ 1 ], ppath, sizeof( ppath ) );

	// Call with quiet flag for initial search
	if ( !modelloader->Map_IsValid( ppath, true ) )
	{
		Host_Map_Helper_FuzzyName( args, ppath, sizeof( ppath ) );
		if ( !modelloader->Map_IsValid( ppath ) )
		{
			Warning( "map load failed: %s not found or invalid\n", ppath );
			return;
		}
	}

	GetPlatformMapPath( ppath, name, sizeof( name ) );

	// If I was in edit mode reload config file
	// to overwrite WC edit key bindings
#if !defined(DEDICATED)
	bool bCommentary = ( flags & EMAP_COMMENTARY ) != 0;
	bool bEditmode = ( flags & EMAP_EDIT_MODE ) != 0;
	if ( !bEditmode )
	{
		if ( g_bInEditMode )
		{
			// Re-read config from disk
			Host_ReadConfiguration( -1, false );
			g_bInEditMode = false;
		}
	}
	else
	{
		g_bInEditMode = true;
	}

	g_bInCommentaryMode = bCommentary;
#endif

	SetLaunchOptions( args );
	if ( !CL_HL2Demo_MapCheck( name ) )
	{
		Warning( "map load failed: %s not found or invalid\n", name );
		return;	
	}

	if ( !CL_PortalDemo_MapCheck( name ) )
	{
		Warning( "map load failed: %s not found or invalid\n", name );
		return;	
	}

#ifdef DEDICATED
	if ( sv.IsDedicated() )
#else
	// Stop demo loop
	GetBaseLocalClient().demonum = -1;
	if ( GetBaseLocalClient().m_nMaxClients == 0 || sv.IsDedicated() )
#endif
	{
		Host_Disconnect( false );	// stop old game

		HostState_NewGame( name, false, bBackground, bSplitScreenConnect );
	}

	if (args.ArgC() == 10)
	{
		if (Q_stricmp(args[2], "setpos") == 0
			&& Q_stricmp(args[6], "setang") == 0) 
		{
			Vector newpos;
			newpos.x = atof( args[3] );
			newpos.y = atof( args[4] );
			newpos.z = atof( args[5] );

			QAngle newangle;
			newangle.x = atof( args[7] );
			newangle.y = atof( args[8] );
			newangle.z = atof( args[9] );
			
			HostState_SetSpawnPoint(newpos, newangle);
		}
	}
}


// Handle a map command from the console.  Active clients are kicked off.
void Host_Map_f( const CCommand &args )
{
	Host_Map_Helper( args, (EMapFlags)0 );
}

// Handle a map group command from the console
void Host_MapGroup_f( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		Warning( "Host_MapGroup_f: No mapgroup specified\n" );
		return;
	}

	Msg( "Setting mapgroup to '%s'\n", args[1] );

	HostState_SetMapGroupName( args[1] );
}

// Handle smap command to connect multiple splitscreen users at once
void Host_SplitScreen_Map_f( const CCommand &args )
{
#ifndef _DEMO
	Host_Map_Helper( args, EMAP_SPLITSCREEN );
#endif
}

//-----------------------------------------------------------------------------
// handle a map_edit <servername> command from the console. 
// Active clients are kicked off.
// UNDONE: protect this from use if not in dev. mode
//-----------------------------------------------------------------------------
#ifndef DEDICATED
CON_COMMAND( map_edit, "" )
{
	Host_Map_Helper( args, EMAP_EDIT_MODE );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Runs a map as the background
//-----------------------------------------------------------------------------
void Host_Map_Background_f( const CCommand &args )
{
	Host_Map_Helper( args, EMAP_BACKGROUND );
}


//-----------------------------------------------------------------------------
// Purpose: Runs a map in commentary mode
//-----------------------------------------------------------------------------
void Host_Map_Commentary_f( const CCommand &args )
{
	Host_Map_Helper( args, EMAP_COMMENTARY );
}


//-----------------------------------------------------------------------------
// Restarts the current server for a dead player
//-----------------------------------------------------------------------------
CON_COMMAND( restart, "Restart the game on the same level (add setpos to jump to current view position on restart)." )
{
	if ( 
#if !defined(DEDICATED)
		demoplayer->IsPlayingBack() || 
#endif
		!sv.IsActive() )
		return;

	if ( sv.IsMultiplayer() )
		return;

	bool bRememberLocation = ( args.ArgC() == 2 && !Q_stricmp( args[1], "setpos" ) );

	bool bSplitScreenConnect = GET_NUM_SPLIT_SCREEN_PLAYERS() == 2 ;

	Host_Disconnect(false);	// stop old game

	if ( !CL_HL2Demo_MapCheck( sv.GetMapName() ) )
	{
		Warning( "map load failed: %s not found or invalid\n", sv.GetMapName() );
		return;	
	}

	if ( !CL_PortalDemo_MapCheck( sv.GetMapName() ) )
	{
		Warning( "map load failed: %s not found or invalid\n", sv.GetMapName() );
		return;	
	}

	HostState_NewGame( sv.GetMapName(), bRememberLocation, false, bSplitScreenConnect );
}


//-----------------------------------------------------------------------------
// Restarts the current server for a dead player
//-----------------------------------------------------------------------------
CON_COMMAND( reload, "Reload the most recent saved game (add setpos to jump to current view position on reload).")
{
#ifndef DEDICATED
	const char *pSaveName;
	char name[MAX_OSPATH];
#endif

	if ( 
#if !defined(DEDICATED)
		demoplayer->IsPlayingBack() || 
#endif
		!sv.IsActive() )
		return;

	if ( sv.IsMultiplayer() )
		return;

	if ( !serverGameDLL->SupportsSaveRestore() )
		return;

	bool remember_location = false;
	if ( args.ArgC() == 2 && 
		!Q_stricmp( args[1], "setpos" ) )
	{
		remember_location = true;
	}

	// See if there is a most recently saved game
	// Restart that game if there is
	// Otherwise, restart the starting game map
#ifndef DEDICATED
	pSaveName = saverestore->FindRecentSave( name, sizeof( name ) );

	// Put up loading plaque
  	SCR_BeginLoadingPlaque();

	{
		// Prepare the offline session for server reload
		KeyValues *pEvent = new KeyValues( "OnEngineClientSignonStatePrepareChange" );
		pEvent->SetString( "reason", "reload" );
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( pEvent );
	}

	Host_Disconnect( false );	// stop old game

	if ( pSaveName && saverestore->SaveFileExists( pSaveName ) )
	{
		HostState_LoadGame( pSaveName, remember_location, false );
	}
	else
#endif
	{
		if ( !CL_HL2Demo_MapCheck( host_map.GetString() ) )
		{
			Warning( "map load failed: %s not found or invalid\n", host_map.GetString() );
			return;	
		}

		if ( !CL_PortalDemo_MapCheck( host_map.GetString() ) )
		{
			Warning( "map load failed: %s not found or invalid\n", host_map.GetString() );
			return;	
		} 

#if !defined( DEDICATED )
		if ( pSaveName && pSaveName[0] )
		{
			Warning( "SAVERESTORE PROBLEM: %s not found!  Starting new game in %s\n", pSaveName, host_map.GetString() );
		}
#endif

		HostState_NewGame( host_map.GetString(), remember_location, false, false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Goes to a new map, taking all clients along
// Output : void Host_Changelevel_f
//-----------------------------------------------------------------------------
void Host_Changelevel_f( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		ConMsg( "changelevel <levelname> : continue game on a new level\n" );
		return;
	}

	if ( !sv.IsActive() )
	{
		ConMsg( "Can't changelevel, not running server\n" );
		return;
	}

	char mapname[MAX_PATH];
	Q_StripExtension( args[ 1 ], mapname, sizeof( mapname ) );

	bool bMapMustExist = true;
	static ConVarRef sv_workshop_allow_other_maps( "sv_workshop_allow_other_maps" );
	if ( StringHasPrefix( mapname, "workshop" ) && ( ( mapname[8] == '/' ) || ( mapname[8] == '\\' ) ) &&
		sv_workshop_allow_other_maps.GetBool() )
		bMapMustExist = false;

	if ( bMapMustExist && !modelloader->Map_IsValid( mapname, true ) )
	{
		Host_Map_Helper_FuzzyName( args, mapname, sizeof( mapname ) );
		if ( !modelloader->Map_IsValid( mapname ) )
		{
			Warning( "changelevel failed: %s not found\n", mapname );
			return;
		}
	}

	if ( !CL_HL2Demo_MapCheck( mapname ) )
	{
		Warning( "changelevel failed: %s not found\n", mapname );
		return;	
	}

	if ( !CL_PortalDemo_MapCheck( mapname ) )
	{
		Warning( "changelevel failed: %s not found\n", mapname );
		return;	
	}

	SetLaunchOptions( args );

	HostState_ChangeLevelMP( mapname, args[2] );
}

//-----------------------------------------------------------------------------
// Purpose: Changing levels within a unit, uses save/restore
//-----------------------------------------------------------------------------
void Host_Changelevel2_f( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		ConMsg ("changelevel2 <levelname> : continue game on a new level in the unit\n");
		return;
	}

	if ( !sv.IsActive() )
	{
		ConMsg( "Can't changelevel2, not in a map\n" );
		return;
	}

	if ( !g_pVEngineServer->IsMapValid( args[1] ) )
	{
		if ( !CL_IsHL2Demo() || (CL_IsHL2Demo() && !(!Q_stricmp( args[1], "d1_trainstation_03" ) || !Q_stricmp( args[1], "d1_town_02a" ))) )	
		{
			Warning( "changelevel2 failed: %s not found\n", args[1] );
			return;
		}
	}

#if !defined(DEDICATED)
	// needs to be before CL_HL2Demo_MapCheck() check as d1_trainstation_03 isn't a valid map
	if ( IsPC() && CL_IsHL2Demo() && !sv.IsDedicated() && !Q_stricmp( args[1], "d1_trainstation_03" ) ) 
	{
		void CL_DemoTransitionFromTrainstation();
		CL_DemoTransitionFromTrainstation();
		return; 
	}

	// needs to be before CL_HL2Demo_MapCheck() check as d1_trainstation_03 isn't a valid map
	if ( IsPC() && CL_IsHL2Demo() && !sv.IsDedicated() && !Q_stricmp( args[1], "d1_town_02a" ) && !Q_stricmp( args[2], "d1_town_02_02a" )) 
	{
		void CL_DemoTransitionFromRavenholm();
		CL_DemoTransitionFromRavenholm();
		return; 
	}

	if ( IsPC() && CL_IsPortalDemo() && !sv.IsDedicated() && !Q_stricmp( args[1], "testchmb_a_07" ) ) 
	{
		void CL_DemoTransitionFromTestChmb();
		CL_DemoTransitionFromTestChmb();
		return; 
	}

#endif

	// allow a level transition to d1_trainstation_03 so the Host_Changelevel() can act on it
	if ( !CL_HL2Demo_MapCheck( args[1] ) ) 
	{
		Warning( "changelevel failed: %s not found\n", args[1] );
		return;	
	}

	SetLaunchOptions( args );

	HostState_ChangeLevelSP( args[1], args[2] );
}


// On PS/3, due to Matchmaking framework event architecture, Host_Disconnect is called recursively on quit.
// Bad things happen. Since it's not really necessary to disconnect recursively, we'll track and prevent it on PS/3 during shutdown.
int g_nHostDisconnectReentrancyCounter = 0;
class CDisconnectReentrancyCounter
{
public:
	CDisconnectReentrancyCounter() { g_nHostDisconnectReentrancyCounter++ ;}
	~CDisconnectReentrancyCounter() { g_nHostDisconnectReentrancyCounter-- ;}
};

//-----------------------------------------------------------------------------
// Purpose: Shut down client connection and any server
//-----------------------------------------------------------------------------
void Host_Disconnect( bool bShowMainMenu )
{
#ifdef _PS3
	if ( GetTLSGlobals()->bNormalQuitRequested )
	{
		if ( g_nHostDisconnectReentrancyCounter != 0 )
		{
			return; // do not disconnect recursively on QUIT
		}
	}
#endif


	IGameEvent *disconnectEvent = g_GameEventManager.CreateEvent( "cs_game_disconnected" );

	if ( disconnectEvent )
		g_GameEventManager.FireEventClientSide( disconnectEvent );

	CDisconnectReentrancyCounter autoReentrancyCounter;
	
#if !defined( DEDICATED )
	if ( bShowMainMenu )
	{
		// exiting game
		// ensure commentary state gets cleared
		g_bInCommentaryMode = false;
	}
#endif

	if ( IsGameConsole() )
	{
		g_pQueuedLoader->EndMapLoading( false );
	}

	// Switch to single-threaded rendering during shutdown to
	// avoid synchronization issues between destructed objects
	// and the renderer
	Host_AllowQueuedMaterialSystem( false );

#ifndef DEDICATED
	if ( !sv.IsDedicated() )
	{
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
			GetLocalClient().Disconnect( bShowMainMenu );
		}
	}
#endif

#if !defined( DEDICATED )
	if ( g_ClientDLL && bShowMainMenu )
	{
		// forcefully stop any of the full screen video panels used for loading or whatever
		// this is a safety precaution to ensure we don't orpan any of the hacky global video panels
		g_ClientDLL->ShutdownMovies();
	}
#endif 

	HostState_GameShutdown();

#ifndef DEDICATED
	if ( !sv.IsDedicated() )
	{
		if ( bShowMainMenu && !engineClient->IsDrawingLoadingImage() && ( GetBaseLocalClient().demonum == -1 ) )
		{
			if ( IsGameConsole() )
			{
				// Reset larger configuration material system memory (for map) back down for ui work
				// This must be BEFORE ui gets-rectivated below
				materials->ResetTempHWMemory( true );
			}

#ifdef _PS3
			if ( GetTLSGlobals()->bNormalQuitRequested )
			{
					return; // do not disconnect recursively on QUIT
			}
#endif

			EngineVGui()->ActivateGameUI();
		}
	}
#endif
}
  
//-----------------------------------------------------------------------------
// Kill the client and any local server.
//-----------------------------------------------------------------------------
CON_COMMAND_F( disconnect, "Disconnect game from server.", FCVAR_SERVER_CAN_EXECUTE )
{
#ifndef DEDICATED
	GetBaseLocalClient().demonum = -1;
#endif

	if ( args.ArgC() > 1 )
	{
		COM_ExplainDisconnection( false, "%s", args[ 1 ] );
	}

	Host_Disconnect(true);
}

CON_COMMAND( version, "Print version info string." )
{
	ConMsg( "Protocol version %i [%i/%i]\nExe version %s (%s)\n", GetHostVersion(), GetServerVersion(), GetClientVersion(), Sys_GetVersionString(), Sys_GetProductString() );
	ConMsg( "Exe build: " __TIME__ " " __DATE__ " (%i) (%i)\n", build_number(), GetSteamAppID() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( pause, "Toggle the server pause state." )
{
#if !defined( CLIENT_DLL )

#ifndef DEDICATED
	if ( !sv.IsDedicated() )
	{
		if ( !GetBaseLocalClient().m_szLevelName[ 0 ] )
			return;
	}
#endif

	if ( !sv.IsPausable() )
		return;

	// toggle paused state
	sv.SetPaused( !sv.IsPaused() );
	
	// send text message who paused the game
	if ( host_client )
		sv.BroadcastPrintf( "%s %s the game\n", host_client->GetClientName(), sv.IsPaused() ? "paused" : "unpaused" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( setpause, "Set the pause state of the server." )
{
#if !defined( CLIENT_DLL )

#ifndef DEDICATED
	if ( !sv.IsDedicated() )
	{
		if ( !GetBaseLocalClient().m_szLevelName[ 0 ] )
			return;
	}
#endif

	if ( !sv.IsPausable() )
		return;

	sv.SetPaused( true );

	if ( !args.FindArg( "nomsg" ) )
	{
		// send text message who paused the game
		if ( host_client )
			sv.BroadcastPrintf( "%s paused the game\n", host_client->GetClientName() );
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( unpause, "Unpause the game." )
{
#if !defined( CLIENT_DLL )

#ifndef DEDICATED
	if ( !sv.IsDedicated() )
	{
		if ( !GetBaseLocalClient().m_szLevelName[ 0 ] )
			return;
	}
#endif

	if ( !sv.IsPaused() )
		return;

	sv.SetPaused( false );

	if ( !args.FindArg( "nomsg" ) )
	{
		// send text messaage who unpaused the game
		if ( host_client )
			sv.BroadcastPrintf( "%s unpaused the game\n", host_client->GetClientName() );
	}
#endif
}

//-----------------------------------------------------------------------------
// Kicks a user off of the server using their userid or uniqueid
//-----------------------------------------------------------------------------
CON_COMMAND( kickid_ex, "Kick a player by userid or uniqueid, provide a force-the-kick flag and also assign a message." )
{
	const char	*pszArg1 = NULL, *pszMessage = NULL;
	int			iSearchIndex = -1;
	char		szSearchString[128];
	int			argsStartNum = 1;
	bool		bSteamID = false;
	bool		bForce = false;

	if ( args.ArgC() <= 1 )
	{
		ConMsg( "Usage:  kickid_ex < userid | uniqueid > < force ( 0 / 1 ) > { message }\n" );
		return;
	}

	// get the first argument
	pszArg1 = args[1];

	// if the first letter is a charcter then
	// we're searching for a uniqueid ( e.g. STEAM_ )
	if ( *pszArg1 < '0' || *pszArg1 > '9' )
	{
		// SteamID (need to reassemble it)
		if ( StringHasPrefix( pszArg1, STEAM_PREFIX ) && Q_strstr( args[2], ":" ) )
		{
			Q_snprintf( szSearchString, sizeof( szSearchString ), "%s:%s:%s", pszArg1, args[3], args[5] );
			argsStartNum = 5;
			bSteamID = true;
		}
		// some other ID (e.g. "UNKNOWN", "STEAM_ID_PENDING", "STEAM_ID_LAN")
		// NOTE: assumed to be one argument
		else
		{
			Q_snprintf( szSearchString, sizeof( szSearchString ), "%s", pszArg1 );
		}
	}
	// this is a userid
	else
	{
		iSearchIndex = Q_atoi( pszArg1 );
	}

	// check for game type and game mode
	if ( args.ArgC() > argsStartNum )
	{
		if ( atoi( args[ argsStartNum + 1 ] ) == 1 )
		{
			bForce = true;
		}

		argsStartNum++;
	}

	// check for a message
	if ( args.ArgC() > argsStartNum )
	{
		int j;
		int dataLen = 0;

		pszMessage = args.ArgS();
		for ( j = 1; j <= argsStartNum; j++ )
		{
			dataLen += Q_strlen( args[j] ) + 1; // +1 for the space between args
		}

		if ( bSteamID )
		{
			dataLen -= 5; // SteamIDs don't have spaces between the args[) values
		}

		if ( dataLen > Q_strlen( pszMessage ) ) // saftey check
		{
			pszMessage = NULL;
		}
		else
		{
			pszMessage += dataLen;
		}
	}

	PerformKick( args.Source(), iSearchIndex, szSearchString, bForce, pszMessage );
}

//-----------------------------------------------------------------------------
// Kicks a user off of the server using their userid or uniqueid
//-----------------------------------------------------------------------------
CON_COMMAND( kickid, "Kick a player by userid or uniqueid, with a message." )
{
	const char	*pszArg1 = NULL, *pszMessage = NULL;
	int			iSearchIndex = -1;
	char		szSearchString[128];
	int			argsStartNum = 1;
	bool		bSteamID = false;

	if ( args.ArgC() <= 1 )
	{
		ConMsg( "Usage:  kickid < userid | uniqueid > { message }\n" );
		return;
	}

	// get the first argument
	pszArg1 = args[1];

	// if the first letter is a charcter then
	// we're searching for a uniqueid ( e.g. STEAM_ )
	if ( *pszArg1 < '0' || *pszArg1 > '9' )
	{
		// SteamID (need to reassemble it)
		if ( StringHasPrefix( pszArg1, STEAM_PREFIX ) && Q_strstr( args[2], ":" ) )
		{
			Q_snprintf( szSearchString, sizeof( szSearchString ), "%s:%s:%s", pszArg1, args[3], args[5] );
			argsStartNum = 5;
			bSteamID = true;
		}
		// some other ID (e.g. "UNKNOWN", "STEAM_ID_PENDING", "STEAM_ID_LAN")
		// NOTE: assumed to be one argument
		else
		{
			Q_snprintf( szSearchString, sizeof( szSearchString ), "%s", pszArg1 );
		}
	}
	// this is a userid
	else
	{
		iSearchIndex = Q_atoi( pszArg1 );
	}

	// check for a message
	if ( args.ArgC() > argsStartNum )
	{
		int j;
		int dataLen = 0;

		pszMessage = args.ArgS();
		for ( j = 1; j <= argsStartNum; j++ )
		{
			dataLen += Q_strlen( args[j] ) + 1; // +1 for the space between args
		}

		if ( bSteamID )
		{
			dataLen -= 5; // SteamIDs don't have spaces between the args[) values
		}

		if ( dataLen > Q_strlen( pszMessage ) ) // saftey check
		{
			pszMessage = NULL;
		}
		else
		{
			pszMessage += dataLen;
		}
	}

	PerformKick( args.Source(), iSearchIndex, szSearchString, false, pszMessage );
}

void PerformKick( cmd_source_t commandSource, int iSearchIndex, char* szSearchString, bool bForceKick, const char* pszMessage )
{
	IClient		*client = NULL;
	char		*who = "Console";

	// find this client
	int i;
	for ( i = 0; i < sv.GetClientCount(); i++ )
	{
		client = sv.GetClient( i );

		if ( !client->IsConnected() )
		{
			continue;
		}

		// searching by UserID
		if ( iSearchIndex != -1 )
		{
			if ( client->GetUserID() == iSearchIndex )
			{
				// found!
				break;
			}
		}
		// searching by UniqueID
		else	
		{
			if ( Q_stricmp( client->GetNetworkIDString(), szSearchString ) == 0 ) 
			{
				// found!
				break;
			}
		}
	}

	// now kick them
	if ( i < sv.GetClientCount() )
	{
		if ( client->IsSplitScreenUser() && client->GetSplitScreenOwner() )
		{
			client = client->GetSplitScreenOwner();
		}

		if ( commandSource == kCommandSrcNetClient )
		{
			who = host_client->m_Name;
		}

		if ( host_client == client && !sv.IsDedicated() && !bForceKick )
		{
			// can't kick yourself!
			return;
		}

		if ( iSearchIndex != -1 || !client->IsFakeClient() )
		{
			if ( pszMessage )
			{
				client->Disconnect( CFmtStr( "Kicked by %s : %s", who, pszMessage ) );
			}
			else
			{
				client->Disconnect( CFmtStr( "Kicked by %s", who ) );
			}
		}
	}
	else
	{
		if ( iSearchIndex != -1 )
		{
			ConMsg( "userid \"%d\" not found\n", iSearchIndex );
		}
		else
		{
			ConMsg( "uniqueid \"%s\" not found\n", szSearchString );			
		}
	}
}

/*
==================
Host_Kick_f

Kicks a user off of the server using their name
==================
*/
CON_COMMAND( kick, "Kick a player by name." )
{
	char		*who = "Console";
	char		*pszName = NULL;
	IClient		*client = NULL;
	int			i = 0;
	char		name[64];

	if ( args.ArgC() <= 1 )
	{
		ConMsg( "Usage:  kick < name >\n" );
		return;
	}

	// copy the name to a local buffer
	memset( name, 0, sizeof(name) );
	Q_strncpy( name, args.ArgS(), sizeof(name) );
	pszName = name;

	// safety check
	if ( pszName && pszName[0] != 0 )
	{
		//HACK-HACK
		// check for the name surrounded by quotes (comes in this way from rcon)
		int len = Q_strlen( pszName ) - 1; // (minus one since we start at 0)
		if ( pszName[0] == '"' && pszName[len] == '"' )
		{
			// get rid of the quotes at the beginning and end
			pszName[len] = 0;
			pszName++;
		}

		for ( i = 0; i < sv.GetClientCount(); i++ )
		{
			client = sv.GetClient(i);

			if ( !client->IsConnected() )
				continue;

			// found!
			if ( Q_strcasecmp( client->GetClientName(), pszName ) == 0 ) 
				break;
		}

		// now kick them
		if ( i < sv.GetClientCount() )
		{
			if ( client->IsSplitScreenUser() && client->GetSplitScreenOwner() )
			{
				client = client->GetSplitScreenOwner();
			}

			if ( args.Source() == kCommandSrcNetClient )
			{
				who = host_client->m_Name;
			}

			// can't kick yourself!
			if ( host_client == client && !sv.IsDedicated() )
				return;

			client->Disconnect( CFmtStr( "Kicked by %s", who ) );
		}
		else
		{
			ConMsg( "Can't kick \"%s\", name not found\n", pszName );
		}
	}
}

/*
===============================================================================

DEBUGGING TOOLS

===============================================================================
*/

void Host_PrintMemoryStatus( const char *mapname )
{
	const float MB = 1.0f / ( 1024*1024 );
	Assert( mapname );
#ifdef PLATFORM_LINUX
	struct mallinfo memstats = mallinfo( );
	Msg( "[MEMORYSTATUS] [%s] Operating system reports sbrk size: %.2f MB, Used: %.2f MB, #mallocs = %d\n",
		mapname, MB*memstats.arena, MB*memstats.uordblks, memstats.hblks );
#elif defined(PLATFORM_OSX)
	struct mstats stats = mstats();
	Msg( "[MEMORYSTATUS] [%s] Operating system reports  Used: %.2f MB, Free: %.2f Total: %.2f\n",
		mapname, MB*stats.bytes_used, MB*stats.bytes_free, MB*stats.bytes_total );
#elif defined( _PS3 )

	// NOTE: for PS3 nFreeMemory can be negative (on a devkit, we can use more memory than a retail kit has)
	int nUsedMemory, nFreeMemory, nAvailable;
	g_pMemAlloc->GlobalMemoryStatus( (size_t *)&nUsedMemory, (size_t *)&nFreeMemory );
	nAvailable = nUsedMemory + nFreeMemory;
	Msg( "[MEMORYSTATUS] [%s] Operating system reports Available: %.2f MB, Used: %.2f MB, Free: %.2f MB\n", 
		mapname, MB*nAvailable, MB*nUsedMemory, MB*nFreeMemory );
#elif defined(PLATFORM_WINDOWS)
	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof(statex);
	GlobalMemoryStatusEx( &statex );
	Msg( "[MEMORYSTATUSEX] [%s] Operating system reports Physical Available: %.2f MB, Physical Used: %.2f MB, Physical Free: %.2f MB\n Virtual Size: %.2f, Virtual Free: %.2f MB, PageFile Size: %.2f, PageFile Free: %.2f MB\n", 
		mapname, MB*statex.ullTotalPhys, MB*( statex.ullTotalPhys - statex.ullAvailPhys ),  MB*statex.ullAvailPhys, MB*statex.ullTotalVirtual, MB*statex.ullAvailVirtual, MB*statex.ullTotalPageFile, MB*statex.ullAvailPageFile );
#endif

	if ( IsPS3() )
	{
		// Include stats on GPU memory usage
		GPUMemoryStats stats;
		materials->GetGPUMemoryStats( stats );
		g_pMemAlloc->SetStatsExtraInfo( mapname, CFmtStr( "%d %d %d %d %d %d %d",
			stats.nGPUMemSize, stats.nGPUMemFree, stats.nTextureSize, stats.nRTSize, stats.nVBSize, stats.nIBSize, stats.nUnknown ) );
		Msg( "[MEMORYSTATUS] [%s] RSX memory: total %.1fkb, free %.1fkb, textures %.1fkb, render targets %.1fkb, vertex buffers %.1fkb, index buffers %.1fkb, unknown %.1fkb\n",
			mapname, stats.nGPUMemSize/1024.0f, stats.nGPUMemFree/1024.0f, stats.nTextureSize/1024.0f, stats.nRTSize/1024.0f, stats.nVBSize/1024.0f, stats.nIBSize/1024.0f, stats.nUnknown/1024.0f );
	}
	else
	{
		g_pMemAlloc->SetStatsExtraInfo( mapname, "" );
	}

	int nTotal = g_pMemAlloc->GetSize( 0 );
	if (nTotal == -1)
	{
		Msg( "Internal heap corrupted!\n" );
	}
	else
	{
		Msg( "Internal heap reports: %5.2f MB (%d bytes)\n", nTotal/(1024.0f*1024.0f), nTotal );
	}

	Msg( "\nHunk Memory Used:\n" );
	Hunk_Print();

	Msg( "\nDatacache reports:\n" );
	g_pDataCache->OutputReport( DC_SUMMARY_REPORT, NULL );
}

//-----------------------------------------------------------------------------
// Dump memory stats
//-----------------------------------------------------------------------------
CON_COMMAND( memory, "Print memory stats." )
{
	ConMsg( "Heap Used:\n" );
	int nTotal = g_pMemAlloc->GetSize( 0 );
	if (nTotal == -1)
	{
		ConMsg( "Corrupted!\n" );
	}
	else
	{
		ConMsg( "%5.2f MB (%d bytes)\n", nTotal/(1024.0f*1024.0f), nTotal );
	}

#ifdef VPROF_ENABLED
	ConMsg("\nVideo Memory Used:\n");
	CVProfile *pProf = &g_VProfCurrentProfile;
	int prefixLen = V_strlen( "TexGroup_Global_" );
	float total = 0.0f;
	for ( int i=0; i < pProf->GetNumCounters(); i++ )
	{
		if ( pProf->GetCounterGroup( i ) == COUNTER_GROUP_TEXTURE_GLOBAL )
		{
			float value = pProf->GetCounterValue( i ) * (1.0f/(1024.0f*1024.0f) );
			total += value;
			const char *pName = pProf->GetCounterName( i );
			if ( StringHasPrefix( pName, "TexGroup_Global_" ) )
			{
				pName += prefixLen;
			}
			ConMsg( "%5.2f MB: %s\n", value, pName );
		}
	}
	ConMsg("------------------\n");
	ConMsg( "%5.2f MB: total\n", total );
#endif

	ConMsg( "\nHunk Memory Used:\n" );
	Hunk_Print();
}

/*
===============================================================================

DEMO LOOP CONTROL

===============================================================================
*/


#ifndef DEDICATED

//MOTODO move all demo commands to demoplayer


//-----------------------------------------------------------------------------
// Purpose: Gets number of valid demo names
// Output : int
//-----------------------------------------------------------------------------
int Host_GetNumDemos()
{
	int c = 0;
#ifndef DEDICATED
	for ( int i = 0; i < MAX_DEMOS; ++i )
	{
		const char *demoname = GetBaseLocalClient().demos[ i ];
		if ( !demoname[ 0 ] )
			break;

		++c;
	}
#endif
	return c;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Host_PrintDemoList()
{
	int count = Host_GetNumDemos();

#ifndef DEDICATED
	int next = GetBaseLocalClient().demonum;
	if ( next >= count || next < 0 )
	{
		next = 0;
	}

	for ( int i = 0; i < MAX_DEMOS; ++i )
	{
		const char *demoname = GetBaseLocalClient().demos[ i ];
		if ( !demoname[ 0 ] )
			break;

		bool isnextdemo = next == i ? true : false;

		DevMsg( "%3s % 2i : %20s\n", isnextdemo ? "-->" : "   ", i, GetBaseLocalClient().demos[ i ] );
	}
#endif

	if ( !count )
	{
		DevMsg( "No demos in list, use startdemos <demoname> <demoname2> to specify\n" );
	}
}


#ifndef DEDICATED
//-----------------------------------------------------------------------------
//
// Con commands related to demos, not available on dedicated servers
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Specify list of demos for the "demos" command
//-----------------------------------------------------------------------------
CON_COMMAND( startdemos, "Play demos in demo sequence." )
{
	int	c = args.ArgC() - 1;
	if (c > MAX_DEMOS)
	{
		Msg ("Max %i demos in demoloop\n", MAX_DEMOS);
		c = MAX_DEMOS;
	}
	Msg ("%i demo(s) in loop\n", c);

	for ( int i=1 ; i<c+1 ; i++ )
	{
		Q_strncpy( GetBaseLocalClient().demos[i-1], args[i], sizeof(GetBaseLocalClient().demos[0]) );
	}

	GetBaseLocalClient().demonum = 0;

	Host_PrintDemoList();

	if ( !sv.IsActive() && !demoplayer->IsPlayingBack() )
	{
		CL_NextDemo ();
	}
	else
	{
		GetBaseLocalClient().demonum = -1;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Return to looping demos, optional resume demo index
//-----------------------------------------------------------------------------
CON_COMMAND( demos, "Demo demo file sequence." )
{
	CClientState &cl = GetBaseLocalClient();
	int oldn = cl.demonum;
	cl.demonum = -1;
	Host_Disconnect(false);
	cl.demonum = oldn;

	if (cl.demonum == -1)
		cl.demonum = 0;

	if ( args.ArgC() == 2 )
	{
		int numdemos = Host_GetNumDemos();
		if ( numdemos >= 1 )
		{
			cl.demonum = clamp( Q_atoi( args[1] ), 0, numdemos - 1 );
			DevMsg( "Jumping to %s\n", cl.demos[ cl.demonum ] );
		}
	}

	Host_PrintDemoList();

	CL_NextDemo ();
}

//-----------------------------------------------------------------------------
// Purpose: Stop current demo
//-----------------------------------------------------------------------------
CON_COMMAND_F( stopdemo, "Stop playing back a demo.", FCVAR_DONTRECORD )
{
	if ( !demoplayer->IsPlayingBack() )
		return;
	
	Host_Disconnect (true);
}

//-----------------------------------------------------------------------------
// Purpose: Skip to next demo
//-----------------------------------------------------------------------------
CON_COMMAND( nextdemo, "Play next demo in sequence." )
{
	if ( args.ArgC() == 2 )
	{
		int numdemos = Host_GetNumDemos();
		if ( numdemos >= 1 )
		{
			GetBaseLocalClient().demonum = clamp( Q_atoi( args[1] ), 0, numdemos - 1 );
			DevMsg( "Jumping to %s\n", GetBaseLocalClient().demos[ GetBaseLocalClient().demonum ] );
		}
	}
	Host_EndGame( false, "Moving to next demo..." );
}

//-----------------------------------------------------------------------------
// Purpose: Print out the current demo play order
//-----------------------------------------------------------------------------
CON_COMMAND( demolist, "Print demo sequence list." )
{
	Host_PrintDemoList();
}


//-----------------------------------------------------------------------------
// Purpose: Host_Soundfade_f
//-----------------------------------------------------------------------------
CON_COMMAND_F( soundfade, "Fade client volume.", FCVAR_SERVER_CAN_EXECUTE )
{
	float percent;
	float inTime, holdTime, outTime;

	if (args.ArgC() != 3 && args.ArgC() != 5)
	{
		Msg("soundfade <percent> <hold> [<out> <int>]\n");
		return;
	}

	percent = clamp( atof(args[1]), 0.0f, 100.0f );
	
	holdTime = MAX( 0.0f, atof(args[2]) );

	inTime = 0.0f;
	outTime = 0.0f;
	if (args.ArgC() == 5)
	{
		outTime = MAX( 0.0f, atof(args[3]) );
		inTime = MAX( 0.0f, atof( args[4]) );
	}

	S_SoundFade( percent, holdTime, outTime, inTime );
}

#endif // !DEDICATED

#endif


//-----------------------------------------------------------------------------
// Shutdown the server
//-----------------------------------------------------------------------------
CON_COMMAND( killserver, "Shutdown the server." )
{
	Host_Disconnect(true);
	
	if ( !sv.IsDedicated() )
	{
		// close network sockets and reopen if multiplayer game
		NET_SetMultiplayer( false );
		NET_SetMultiplayer( !!( g_pMatchFramework->GetMatchTitle()->GetTitleSettingsFlags() & MATCHTITLE_SETTING_MULTIPLAYER ) );
	}
}

// [hpe:jason] Enable ENGINE_VOICE for Cstrike 1.5, all platforms
#if defined( CSTRIKE15 ) 
	ConVar voice_vox( "voice_vox", "false", FCVAR_DEVELOPMENTONLY ); // Controls open microphone (no push to talk) settings
	#undef NO_ENGINE_VOICE
#else
	#define NO_ENGINE_VOICE
#endif

#ifdef NO_ENGINE_VOICE
ConVar voice_ptt( "voice_ptt", "-1.0", FCVAR_DEVELOPMENTONLY ); // Time when ptt key was released, 0 means to keep transmitting voice
#endif

#if !defined(DEDICATED)
void Host_VoiceRecordStart_f(void)
{
#ifdef NO_ENGINE_VOICE
	voice_ptt.SetValue( 0 );
#else
	ConVarRef voice_vox( "voice_vox" );

	if ( voice_vox.GetBool() == true )
		return;

	int iSsSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	if ( GetLocalClient( iSsSlot ).IsActive() )
	{
		const char *pUncompressedFile = NULL;
		const char *pDecompressedFile = NULL;
		const char *pInputFile = NULL;

		if (voice_recordtofile.GetInt())
		{
			pUncompressedFile = "voice_micdata.wav";
			pDecompressedFile = "voice_decompressed.wav";
		}

		if (voice_inputfromfile.GetInt())
		{
			pInputFile = "voice_input.wav";
		}
#if !defined( NO_VOICE )
		if (Voice_RecordStart(pUncompressedFile, pDecompressedFile, pInputFile))
		{
		}
#endif
	}
#endif // #ifndef NO_ENGINE_VOICE
}

void Host_VoiceRecordStop_f( const CCommand &args )
{
#ifdef NO_ENGINE_VOICE
	voice_ptt.SetValue( (float) Plat_FloatTime() );
#else
	ConVarRef voice_vox( "voice_vox" );

	if ( voice_vox.GetBool() == true )
		return;

	int iSsSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	if ( GetLocalClient( iSsSlot ).IsActive() )
	{
#if !defined( NO_VOICE )
		if (Voice_IsRecording())
		{
			CL_SendVoicePacket(true);
			Voice_RecordStop();
		}

		if ( args.ArgC() == 2 && V_strcasecmp( args[1], "force" ) == 0 )
		{
			// do nothing
		}
		else
		{

			voice_vox.SetValue( 0 );
		}
#endif
	}
#endif // #ifndef NO_ENGINE_VOICE
}
#endif

// TERROR: adding a toggle for voice
void Host_VoiceToggle_f( const CCommand &args )
{
#ifdef NO_ENGINE_VOICE
	voice_ptt.SetValue( (float) ( voice_ptt.GetFloat() ? 0.0f : Plat_FloatTime() ) );
#endif
#if !defined( DEDICATED ) && !defined( NO_ENGINE_VOICE )
#if !defined( NO_VOICE )
	if ( GetBaseLocalClient().IsActive() )
	{
		bool bToggle = false;

		if ( args.ArgC() == 2 && V_strcasecmp( args[1], "on" ) == 0 )
		{
			bToggle = true;
		}

		if ( Voice_IsRecording() && bToggle == false )
		{
			CL_SendVoicePacket(true);
			Voice_RecordStop();
		}
		else if ( bToggle == true && Voice_IsRecording() == false )
		{
			const char *pUncompressedFile = NULL;
			const char *pDecompressedFile = NULL;
			const char *pInputFile = NULL;

			if (voice_recordtofile.GetInt())
			{
				pUncompressedFile = "voice_micdata.wav";
				pDecompressedFile = "voice_decompressed.wav";
			}

			if (voice_inputfromfile.GetInt())
			{
				pInputFile = "voice_input.wav";
			}

			Voice_RecordStart( pUncompressedFile, pDecompressedFile, pInputFile );
		}
	}
#endif
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Wrapper for modelloader->Print() function call
//-----------------------------------------------------------------------------
CON_COMMAND( listmodels, "List loaded models." )
{
	modelloader->Print();
}

/*
==================
Host_IncrementCVar
==================
*/
CON_COMMAND_F( incrementvar, "Increment specified convar value.", FCVAR_DONTRECORD )
{
	if( args.ArgC() != 5 )
	{
		Warning( "Usage: incrementvar varName minValue maxValue delta\n" );
		return;
	}

	const char *varName = args[ 1 ];
	if( !varName )
	{
		ConDMsg( "Host_IncrementCVar_f without a varname\n" );
		return;
	}

	ConVar *var = ( ConVar * )g_pCVar->FindVar( varName );
	if( !var )
	{
		ConDMsg( "cvar \"%s\" not found\n", varName );
		return;
	}

	float currentValue = var->GetFloat();
	float startValue = atof( args[ 2 ] );
	float endValue = atof( args[ 3 ] );
	float delta = atof( args[ 4 ] );
	float newValue = currentValue + delta;
	if( newValue > endValue )
	{
		newValue = startValue;
	}
	else if ( newValue < startValue )
	{
		newValue = endValue;
	}

	// Conver incrementvar command to direct sets to avoid any problems with state in a demo loop.
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), va("%s %f", varName, newValue) );

	ConDMsg( "%s = %f\n", var->GetName(), newValue );
}


//-----------------------------------------------------------------------------
// Host_MultiplyCVar_f
//-----------------------------------------------------------------------------
CON_COMMAND_F( multvar, "Multiply specified convar value.", FCVAR_DONTRECORD )
{
	if (( args.ArgC() != 5 ))
	{
		Warning( "Usage: multvar varName minValue maxValue factor\n" );
		return;
	}

	const char *varName = args[ 1 ];
	if( !varName )
	{
		ConDMsg( "multvar without a varname\n" );
		return;
	}

	ConVar *var = ( ConVar * )g_pCVar->FindVar( varName );
	if( !var )
	{
		ConDMsg( "cvar \"%s\" not found\n", varName );
		return;
	}

	float currentValue = var->GetFloat();
	float startValue = atof( args[ 2 ] );
	float endValue = atof( args[ 3 ] );
	float factor = atof( args[ 4 ] );
	float newValue = currentValue * factor;
	if( newValue > endValue )
	{
		newValue = endValue;
	}
	else if ( newValue < startValue )
	{
		newValue = startValue;
	}

	// Conver incrementvar command to direct sets to avoid any problems with state in a demo loop.
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), va("%s %f", varName, newValue) );

	ConDMsg( "%s = %f\n", var->GetName(), newValue );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( dumpstringtables, "Print string tables to console." )
{
	SV_PrintStringTables();
#ifndef DEDICATED
	CL_PrintStringTables();
#endif
}

CON_COMMAND( stringtabledictionary, "Create dictionary for current strings." )
{
	if ( !sv.IsActive() )
	{
		Warning( "stringtabledictionary: only valid when running a map\n" );
		return;
	}

	SV_CreateDictionary( sv.GetMapName() );
}

// Register shared commands
CON_COMMAND_F( quit, "Exit the engine.", FCVAR_NONE )
{ 
	if ( args.ArgC() > 1 && V_strcmp( args[ 1 ], "prompt" ) == 0 )
	{
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "quit_prompt" );
		return;
	}

	Host_Quit_f();
}

static ConCommand cmd_exit("exit", Host_Quit_f, "Exit the engine.");

#ifndef DEDICATED
#ifdef VOICE_OVER_IP
static ConCommand startvoicerecord("+voicerecord", Host_VoiceRecordStart_f);
static ConCommand endvoicerecord("-voicerecord", Host_VoiceRecordStop_f);
static ConCommand togglevoicerecord("voicerecord_toggle", Host_VoiceToggle_f);
#endif // VOICE_OVER_IP

#endif

//-----------------------------------------------------------------------------
// Purpose: Force a null pointer crash. useful for testing minidumps
//-----------------------------------------------------------------------------
CON_COMMAND_F( crash, "Cause the engine to crash (Debug!!)", FCVAR_CHEAT )
{ 
	Msg( "forcing crash\n" );
#if defined( _X360 )
	DmCrashDump( FALSE );
#else
	char *p = 0;
	*p = 0;
#endif
}

CON_COMMAND_F( spincycle, "Cause the engine to spincycle (Debug!!)", FCVAR_CHEAT )
{ 
	if ( args.ArgC() > 1 )
	{
		const char *pParam = args.Arg( 1 );
		if ( pParam && *pParam && pParam[ V_strlen( pParam ) - 1 ] == 's' )
		{
			float flSeconds = V_atof( pParam );
			if ( flSeconds > 0 )
			{
				Msg( "Sleeping for %.3f seconds\n", flSeconds );
				ThreadSleep( flSeconds * 1000 );
				return;
			}
		}
	}

	int numCycles = ( args.ArgC() > 1 ) ? Q_atoi( args.Arg(1) ) : 10;
	Msg( "forcing spincycle for %d cycles\n", numCycles );
	for( int k = 0; k < numCycles; ++ k )
	{
		( void ) RandomInt( 0, numCycles );
	}
}

#if POSIX
CON_COMMAND_F( forktest, "Cause the engine to fork and wait for child PID, parameter can be passed for requested exit code (Debug!!)", FCVAR_CHEAT )
{
	EndWatchdogTimer();	// End the watchdog in case child takes too long

	int nExitCodeRequested = ( args.ArgC() > 1 ) ? Q_atoi( args.Arg(1) ) : 0;
	
	pid_t pID = fork();
	Msg( "forktest: Forked, pID = %d\n", (int) pID );
	if ( pID == 0 )  // are we the forked child?
	{
		//
		// Enumerate all open file descriptors that are not #0 (stdin), #1 (stdout), #2 (stderr)
		// and close them all.
		// This will close all sockets and file handles that can result in process hanging
		// when network events occur on the machine and make NFS handles go bad.
		//
		if ( !CommandLine()->FindParm( "-forkfdskeepall" ) )
		{
			FileFindHandle_t hFind = NULL;
			CUtlVector< int > arrHandlesToClose;
			for ( char const *szFileName = g_pFullFileSystem->FindFirst( "/proc/self/fd/*", &hFind );
				szFileName && *szFileName; szFileName = g_pFullFileSystem->FindNext( hFind ) )
			{
				int iFdHandle = Q_atoi( szFileName );
				if ( ( iFdHandle > 2 ) && ( arrHandlesToClose.Find( iFdHandle ) == arrHandlesToClose.InvalidIndex() ) )
					arrHandlesToClose.AddToTail( iFdHandle );
			}
			g_pFullFileSystem->FindClose( hFind );
			FOR_EACH_VEC( arrHandlesToClose, idxFd )
			{
				::close( arrHandlesToClose[idxFd] );
			}

			if ( !CommandLine()->FindParm( "-forkfdskeepstd" ) )
			{
				// Explicitly close #0 (stdin), #1 (stdout), #2 (stderr) and reopen them to /dev/null to consume 0-1-2 FDs (Posix spec requires to return lowest FDs first)
				::close( 0 );
				::close( 1 );
				::close( 2 );
				::open("/dev/null", O_RDONLY);
				::open("/dev/null", O_RDWR);
				::open("/dev/null", O_RDWR);
			}
		}

		Msg( "Child finished successfully!\n" );
		syscall( SYS_exit, nExitCodeRequested );		// don't do a normal c++ exit, don't want to call destructors, etc.
		Warning( "Forked child just called SYS_exit.\n" );
	}
	else
	{
		int nRet = -1;
		int nWait = waitpid( pID, &nRet, 0 );
		Msg( "Parent finished wait: %d, ret: %d, exit: %d, code: %d\n", nWait, nRet, WIFEXITED( nRet ), WEXITSTATUS( nRet ) );
	}
}
#endif

CON_COMMAND_F( flush, "Flush unlocked cache memory.", FCVAR_CHEAT )
{
#if !defined( DEDICATED )
	g_ClientDLL->InvalidateMdlCache();
#endif // DEDICATED
	serverGameDLL->InvalidateMdlCache();
	g_pDataCache->Flush( true );
#if !defined( DEDICATED )
	wavedatacache->Flush();
#endif
}

CON_COMMAND_F( flush_locked, "Flush unlocked and locked cache memory.", FCVAR_CHEAT )
{
#if !defined( DEDICATED )
	g_ClientDLL->InvalidateMdlCache();
#endif // DEDICATED
	serverGameDLL->InvalidateMdlCache();
	g_pDataCache->Flush( false );
#if !defined( DEDICATED )
	wavedatacache->Flush();
#endif
}

CON_COMMAND( cache_print, "cache_print [section]\nPrint out contents of cache memory." )
{
	const char *pszSection = NULL;
	if ( args.ArgC() == 2 )
	{
		pszSection = args[ 1 ];
	}
	g_pDataCache->OutputReport( DC_DETAIL_REPORT, pszSection );
}

CON_COMMAND( cache_print_lru, "cache_print_lru [section]\nPrint out contents of cache memory." )
{
	const char *pszSection = NULL;
	if ( args.ArgC() == 2 )
	{
		pszSection = args[ 1 ];
	}
	g_pDataCache->OutputReport( DC_DETAIL_REPORT_LRU, pszSection );
}

CON_COMMAND( cache_print_summary, "cache_print_summary [section]\nPrint out a summary contents of cache memory." )
{
	const char *pszSection = NULL;
	if ( args.ArgC() == 2 )
	{
		pszSection = args[ 1 ];
	}
	g_pDataCache->OutputReport( DC_SUMMARY_REPORT, pszSection );
}

#if defined( _X360 )
CON_COMMAND( vx_datacache_list, "vx_datacache_list" )
{
	g_pDataCache->OutputReport( DC_DETAIL_REPORT_VXCONSOLE, NULL );
}
#endif

#ifndef _DEMO
#ifndef DEDICATED
// NOTE: As of shipping the 360 version of L4D, this command will not work correctly. See changelist 612757 (terror src) for why.
CON_COMMAND_F( ss_connect, "If connected with available split screen slots, connects a split screen player to this machine.", FCVAR_DEVELOPMENTONLY )
{
	if ( host_state.max_splitscreen_players == 1 )
	{
		if ( toolframework->InToolMode() )
		{
			Msg( "Can't ss_connect, split screen not supported when running -tools mode.\n" );
		}
		else
		{
			Msg( "Can't ss_connect, game does not support split screen.\n" );
		}
		return;
	}

	if ( !GetBaseLocalClient().IsConnected() )
	{
		Msg( "Can't ss_connect, not connected to game.\n" );
		return;
	}
	

	int nSlot = 1;
#ifndef DEDICATED
	while ( splitscreen->IsValidSplitScreenSlot( nSlot ) )
	{
		++nSlot;
	}
#endif

	if ( nSlot >= host_state.max_splitscreen_players )
	{
		Msg( "Can't ss_connect, no more split screen player slots!\n" );
		return;
	}

	// Grab convars for next available slot
	CCLCMsg_SplitPlayerConnect_t msg;
	Host_BuildUserInfoUpdateMessage( nSlot, msg.mutable_convars(), false );

	GetBaseLocalClient().m_NetChannel->SendNetMsg( msg );
}


CON_COMMAND_F( ss_disconnect, "If connected with available split screen slots, connects a split screen player to this machine.", FCVAR_DEVELOPMENTONLY )
{
	if ( args.Source() == kCommandSrcNetClient )
	{
#ifndef DEDICATED
		host_client->SplitScreenDisconnect( args );
#endif
		return;
	}

	// Get the first valid slot
	int nSlot = -1;
	for ( int i = 1; i < host_state.max_splitscreen_players; ++i )
	{
		if ( IS_VALID_SPLIT_SCREEN_SLOT( i ) )
		{
			nSlot = i;
			break;

		}
	}

	if ( args.ArgC() > 1 )
	{
		int cmdslot = Q_atoi( args.Arg( 1 ) );
		if ( IS_VALID_SPLIT_SCREEN_SLOT( cmdslot ) )
		{
			nSlot = cmdslot;
		}
		else
		{
			Msg( "Can't ss_disconnect, slot %d not active\n", cmdslot );
			return;
		}
	}

	if ( ! IS_VALID_SPLIT_SCREEN_SLOT( nSlot ) )
	{
		Msg( "Can't ss_disconnect, no split screen users active\n" );
		return;
	}

	char buf[ 256 ];
	Q_snprintf( buf, sizeof( buf ), "ss_disconnect %d\n", nSlot );

	CCommand argsClient;
	argsClient.Tokenize( buf, kCommandSrcCode );
	Cmd_ForwardToServer( argsClient );
#ifndef DEDICATED
	splitscreen->SetDisconnecting( nSlot, true );
#endif
}

#endif
#endif

#if 0
CON_COMMAND_F( infinite_loop, "Hang server with an infinite loop to test crash recovery.", FCVAR_CHEAT )
{
	for(;;)
	{
		ThreadSleep( 500 );
	}
}

CON_COMMAND_F( null_ptr_references, "Produce a null ptr reference.", FCVAR_CHEAT )
{
	*((int *) 0 ) = 77;
}
#endif


