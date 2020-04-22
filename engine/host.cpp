//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "tier0/fasttimer.h"

#ifdef _WIN32
#if !defined(_X360)
#include "winlite.h"
#endif
#include "tier0/memdbgon.h" // needed because in release builds crtdbg.h is handled specially if USE_MEM_DEBUG is defined
#include "tier0/memdbgoff.h"
#include <crtdbg.h>   // For getting at current heap size

// For maximum compatibility use PSAPI v1
// #define PSAPI_VERSION 1
// #include "psapi.h"
// #pragma comment(lib, "psapi.lib")
#endif

#include "tier1/fmtstr.h"
#include "vstdlib/jobthread.h"
#include "vstdlib/random.h"

#include "server.h"
#include "host_jmp.h"
#include "screen.h"
#include "keys.h"
#include "cdll_int.h"
#include "eiface.h"
#include "sv_main.h"
#include "master.h"
#include "sv_log.h"
#include "shadowmgr.h"
#include "zone.h"
#include "gl_cvars.h"
#include "sv_filter.h"
#include "ivideomode.h"
#include "vprof_engine.h"
#include "iengine.h"
#include "matchmaking/imatchframework.h"
#include "tier2/tier2.h"
#include "enginethreads.h"
#include "steam/steam_api.h"
#include "LoadScreenUpdate.h"
#include "datacache/idatacache.h"
#include "profile.h"
#include "dbginput.h"
#include "filesystem.h"
#include "tier0/microprofiler.h"

#include "checksum_sha1.h"

#if !defined DEDICATED
#include "voice.h"
#include "sound.h"
#include "vaudio/ivaudio.h"
#endif

#include "icvar.h"

#include "sys.h"
#include "client.h"
#include "cl_pred.h"
#include "netconsole.h"
#include "console.h"
#include "view.h"
#include "host.h"
#include "decal.h"
#include "gl_matsysiface.h"
#include "gl_shader.h"
#include "sys_dll.h"
#include "cmodel_engine.h"
#ifndef DEDICATED
#include "con_nprint.h"
#endif
#include "filesystem.h"
#include "filesystem_engine.h"
#include "traceinit.h"
#include "host_saverestore.h"
#include "l_studio.h"
#include "cl_demo.h"
#include "cdll_engine_int.h"
#include "host_cmd.h"
#include "host_state.h"
#include "dt_instrumentation.h"
#include "dt_instrumentation_server.h"
#include "const.h"
#include "bitbuf_errorhandler.h"
#include "soundflags.h"
#include "enginestats.h"
#include "tier1/strtools.h"
#include "testscriptmgr.h"
#include "tmessage.h"
#include "tier0/vprof.h"
#include "tier0/etwprof.h"
#include "tier0/icommandline.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "MapReslistGenerator.h"
#include "DownloadListGenerator.h"
#include "download.h"
#include "staticpropmgr.h"
#include "GameEventManager.h"
#include "iprediction.h"
#include "netmessages.h"
#include "cl_main.h"
#include "hltvserver.h"
#include "hltvtest.h"
#if defined( REPLAY_ENABLED )
#include "replayserver.h"
#include "replayhistorymanager.h"
#endif
#include "sys_mainwind.h"
#include "host_phonehome.h"
#ifndef DEDICATED
#include "vgui_baseui_interface.h"
#include "cl_steamauth.h"
#endif
#include "sv_remoteaccess.h" // NotifyDedicatedServerUI()
#include "snd_audio_source.h"
#include "sv_steamauth.h"
#include "MapReslistGenerator.h"
#include "DevShotGenerator.h"
#include "sv_plugin.h"
#include "toolframework/itoolframework.h"
#include "ienginetoolinternal.h"
#include "inputsystem/iinputsystem.h"
#include "vgui_askconnectpanel.h"
#include "cvar.h"
#include "saverestoretypes.h"
#include "filesystem/IQueuedLoader.h"
#include "filesystem/IXboxInstaller.h"
#include "soundservice.h"
#include "steam/isteamremotestorage.h"
#include "ConfigManager.h"
#include "materialsystem/idebugtextureinfo.h"
#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif
#include "engine/ips3frontpanelled.h"
#include "audio_pch.h"
#include "platforminputdevice.h"
#include "status.h"

#ifdef _X360
#include "xbox/xbox_console.h"
#define _XBOX
#include <xtl.h>
#undef _XBOX
#endif

#if defined ( _GAMECONSOLE )
#include "GameUI/IGameUI.h"
#include "vgui_baseui_interface.h"
#endif

#include "matchmaking/mm_helpers.h"
#include "ixboxsystem.h"
#if defined( INCLUDE_SCALEFORM )
#include "scaleformui/scaleformui.h"
#endif

extern IXboxSystem *g_pXboxSystem;
extern ConVar cl_cloud_settings;

#ifndef DEDICATED
extern IVAudio *vaudio;
void *g_pMilesAudioEngineRef;
#endif

#ifdef _PS3
#include "ps3/ps3_helpers.h"
#endif 

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
void NET_Init( bool bDedicated );
void CL_SetPagedPoolInfo();
extern char	*CM_EntityString( void );
bool XBX_SetProfileDefaultSettings( int iController );
extern ConVar host_map;
extern ConVar sv_cheats;

bool g_bDedicatedServerBenchmarkMode = false;

int host_frameticks = 0;
int host_tickcount = 0;
int host_currentframetick = 0;
bool g_bLowViolence = false;
static bool g_bAllowSecureServers = true;

#if defined( INCLUDE_SCALEFORM )
const char g_szDefaultScaleformMovieName[] = "resource/flash/MainUIRootMovie.swf";
const char g_szDefaultScaleformCursorName[] = "resource/flash/Cursor.swf";
#endif

#ifdef USE_SDL
	#include "appframework/ilaunchermgr.h"
	extern ILauncherMgr *g_pLauncherMgr;
#endif

// Engine player info, no game related infos here
BEGIN_BYTESWAP_DATADESC( player_info_s )
	DEFINE_FIELD( version, FIELD_INTEGER64 ),
	DEFINE_FIELD( xuid, FIELD_INTEGER64 ),
	DEFINE_ARRAY( name, FIELD_CHARACTER, MAX_PLAYER_NAME_LENGTH ),
	DEFINE_FIELD( userID, FIELD_INTEGER ),
	DEFINE_ARRAY( guid, FIELD_CHARACTER, SIGNED_GUID_LEN + 1 ),
	DEFINE_FIELD( friendsID, FIELD_INTEGER ),
	DEFINE_ARRAY( friendsName, FIELD_CHARACTER, MAX_PLAYER_NAME_LENGTH ),
	DEFINE_FIELD( fakeplayer, FIELD_BOOLEAN ),
	DEFINE_FIELD( ishltv, FIELD_BOOLEAN ),
#if defined( REPLAY_ENABLED )
	DEFINE_FIELD( isreplay, FIELD_BOOLEAN ),
#endif
	DEFINE_ARRAY( customFiles, FIELD_INTEGER, MAX_CUSTOM_FILES ),
	DEFINE_FIELD( filesDownloaded, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

//------------------------------------------
enum
{
	FRAME_SEGMENT_INPUT = 0,
	FRAME_SEGMENT_CLIENT,
	FRAME_SEGMENT_SERVER,
	FRAME_SEGMENT_RENDER,
	FRAME_SEGMENT_SOUND,
	FRAME_SEGMENT_CLDLL,
	FRAME_SEGMENT_CMD_EXECUTE,

	NUM_FRAME_SEGMENTS,
};

class CFrameTimer
{
public:
	void ResetDeltas();

	CFrameTimer() : swaptime(0), framestarttime(0), framestarttimeduration(0)
	{
		m_flFPSVariability = 0;
		m_flFPSStdDeviationSeconds = 0;
		m_flFPSStdDeviationFrameStartTimeSeconds = 0;

		ResetDeltas();
	}

	void MarkFrameStartTime()
	{
		double newframestarttime = Sys_FloatTime();
		framestarttimeduration = framestarttime ? ( newframestarttime - framestarttime ) : 0.0; 
		framestarttime = newframestarttime;
	}
	void MarkFrame();
	void StartFrameSegment( int i )
	{
		starttime[i] = Sys_FloatTime();
	}

	void EndFrameSegment( int i )
	{
		double dt = Sys_FloatTime() - starttime[i];
		deltas[ i ] += dt;
	}
	void MarkSwapTime( )
	{
		double newswaptime = Sys_FloatTime();
		frametime = newswaptime - swaptime;
		swaptime = newswaptime;

		ComputeFrameVariability();
		g_EngineStats.SetFrameTime( frametime );
		g_EngineStats.SetFPSVariability( m_flFPSVariability );

		host_frametime_stddeviation = m_flFPSStdDeviationSeconds;
		host_framestarttime_stddeviation = m_flFPSStdDeviationFrameStartTimeSeconds;
		host_frameendtime_computationduration = newswaptime - framestarttime;
	}

	float GetServerSimulationFrameTime()
	{
		return m_flLastServerTime;
	}

private:
	enum
	{
		FRAME_HISTORY_COUNT = 50		
	};

	friend void Host_Speeds();
	void ComputeFrameVariability();

	double time_base;
	double times[9];
	double swaptime;
	double framestarttime;
	double framestarttimeduration;
	double frametime;
	double m_flFPSVariability;
	double m_flFPSStdDeviationSeconds;
	double m_flFPSStdDeviationFrameStartTimeSeconds;
	double starttime[NUM_FRAME_SEGMENTS];
	double deltas[NUM_FRAME_SEGMENTS];

	float m_flLastServerTime;

	float m_pFrameStartTimeHistory[FRAME_HISTORY_COUNT];
	float m_pFrameTimeHistory[FRAME_HISTORY_COUNT];
	int m_nFrameTimeHistoryIndex;
};


static CFrameTimer g_HostTimes;

float Host_GetServerSimulationFrameTime()
{
	return g_HostTimes.GetServerSimulationFrameTime();
}


//------------------------------------------


float host_time = 0.0;

static ConVar	violence_hblood( "violence_hblood","1", 0, "Draw human blood" );
static ConVar	violence_hgibs( "violence_hgibs","1", 0, "Show human gib entities" );
static ConVar	violence_ablood( "violence_ablood","1", 0, "Draw alien blood" );
static ConVar	violence_agibs( "violence_agibs","1", 0, "Show alien gib entities" );

static bool GetDefaultSubtitlesState()
{
	return XBX_IsLocalized() && !XBX_IsAudioLocalized();
}

// Marked as FCVAR_USERINFO so that the server can cull CC messages before networking them down to us!!!
ConVar closecaption( "closecaption", GetDefaultSubtitlesState() ? "1" : "0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE, "Enable close captioning." );
ConVar cl_configversion( "cl_configversion", "8", FCVAR_DEVELOPMENTONLY, "Configuration layout version. Bump this to force a reset of the PS3 save game / settings." );
extern ConVar sv_unlockedchapters;

void Snd_Restart_f()
{
#ifndef DEDICATED

	extern bool snd_firsttime;

	CUtlVector<musicsave_t> music;

	// Ask sound system for current music tracks
	S_GetCurrentlyPlayingMusic( music );

	S_Shutdown();
	snd_firsttime = true;
	GetBaseLocalClient().ClearSounds();
	S_Init();

	for ( int i = 0; i < music.Count(); i++ )
	{
		S_RestartSong( &music[i] );
	}
	// Do this or else it won't have anything in the cache.
	if ( audiosourcecache && sv.GetMapName()[0] )
	{
		audiosourcecache->LevelInit( sv.GetMapName() );
	}

	// Flush soundscapes so they don't stop. We don't insert text in the buffer here because 
	// cl_soundscape_flush is normally cheat-protected.
	ConCommand *pCommand = (ConCommand*)dynamic_cast< const ConCommand* >( g_pCVar->FindCommand( "cl_soundscape_flush" ) );
	if ( pCommand )
	{
		char const *argv[ 1 ] = { "cl_soundscape_flush" };

		CCommand cmd( 1, argv, kCommandSrcCode );
		pCommand->Dispatch( cmd );
	}

#ifndef NO_VOICE
	Voice_ForceInit();
#endif // NO_VOICE

#endif
}
void Snd_Restart_Cmd()
{
	if( !sv_cheats.GetBool() )
	{
		Msg("Warning: The console command \"snd_restart\" will not work with sv_cheats 0\n");
		return;
	}
	Snd_Restart_f();
}
static ConCommand snd_restart( "snd_restart", Snd_Restart_Cmd, "Restart sound system." );

// In other C files.
void Shader_Shutdown( void );
void R_Shutdown( void );

bool g_bAbortServerSet = false;

#ifdef _WIN32
static bool s_bInitPME = false;
#endif

static ConVar mem_test_quiet( "mem_test_quiet", "0", 0, "Don't print stats when memtesting" );
static ConVar mem_test_each_frame( "mem_test_each_frame", "0", 0, "Run heap check at end of every frame\n" );

extern void Host_PrintMemoryStatus( const char *mapname );

const char *GetMapName( void )
{
	static char mapname[ 256 ];
	const char *pTest = sv.GetMapName();
	if ( !pTest || !pTest[0] )
	{
		// possibly at menu
		pTest = "nomap";
	}
	Q_FileBase( pTest, mapname, sizeof( mapname ) );
	return mapname;
}

CON_COMMAND( mem_dump, "Dump memory stats to text file." )
{
	ConMsg("Writing memory stats to file memstats.txt\n");
	const char *mapname = GetMapName();
	Host_PrintMemoryStatus( mapname );
	g_pMemAlloc->DumpStatsFileBase( mapname );
	// Dump memory about other memory, chiefly from CMemoryStack.
	DumpMemoryInfoStats();
}

CON_COMMAND( mem_verify, "Verify the validity of the heap" )
{
	g_pMemAlloc->heapchk();
}

CON_COMMAND( mem_compact, "" )
{
	g_pMemAlloc->CompactHeap();
}

CON_COMMAND( mem_incremental_compact, "" )
{
	g_pMemAlloc->CompactIncremental();
}

CON_COMMAND( mem_eat, "" )
{
	MemAlloc_Alloc( 1024*1024 );
}

ConVar mem_incremental_compact_rate( "mem_incremental_compact_rate", ".5", FCVAR_CHEAT, "Rate at which to attempt internal heap compation" );

static bool MemTest()
{
	bool verbose = ( !mem_test_quiet.GetBool() && !mem_test_each_frame.GetBool() );
	if ( verbose )
	{
		Msg( "\nBegin mem_test\n" );
		Host_PrintMemoryStatus( GetMapName() );
	}

	bool result = g_pMemAlloc->CrtCheckMemory() ? true : false;

	if ( verbose )
	{
		Msg( "\nEnd mem_test\n" );
	}

	return result;
}

CON_COMMAND( mem_test, "" )
{
	MemTest();
}

static ConVar mem_test_every_n_seconds( "mem_test_every_n_seconds", "0", 0, "Run heap check at a specified interval\n" );
static ConVar singlestep( "singlestep", "0", FCVAR_CHEAT, "Run engine in single step mode ( set next to 1 to advance a frame )" );
static ConVar next( "next", "0", FCVAR_CHEAT, "Set to 1 to advance to next frame ( when singlestep == 1 )" );
// Print a debug message when the client or server cache is missed
ConVar host_showcachemiss( "host_showcachemiss", "0", 0, "Print a debug message when the client or server cache is missed." );
static ConVar mem_dumpstats( "mem_dumpstats", "0", 0, "Dump current and max heap usage info to console at end of frame ( set to 2 for continuous output )\n" );
static ConVar host_ShowIPCCallCount( "host_ShowIPCCallCount", "0", 0, "Print # of IPC calls this number of times per second. If set to -1, the # of IPC calls is shown every frame." );

ConVar vprof_server_spike_threshold( "vprof_server_spike_threshold", "999.0" );
ConVar vprof_server_thread( "vprof_server_thread", "0" );

#if defined( RAD_TELEMETRY_ENABLED )

static void OnChangeTelemetryPause ( IConVar *var, const char *pOldValue, float flOldValue )
{
	TM_PAUSE( TELEMETRY_LEVEL0, 1 );
}

static void OnChangeTelemetryResume ( IConVar *var, const char *pOldValue, float flOldValue )
{
	TM_PAUSE( TELEMETRY_LEVEL0, 0 );
}

static void OnChangeTelemetryLevel ( IConVar *var, const char *pOldValue, float flOldValue )
{
	char* pIEnd;
	const char *pLevel = (( ConVar* )var)->GetString();

	TelemetrySetLevel( strtoul( pLevel, &pIEnd, 0 ) );
}

static void OnChangeTelemetryFrameCount ( IConVar *var, const char *pOldValue, float flOldValue )
{
	char* pIEnd;
	const char *pFrameCount = (( ConVar* )var)->GetString();

	g_Telemetry.FrameCount = strtoul( pFrameCount, &pIEnd, 0 );
	Msg( " TELEMETRY: Setting Telemetry FrameCount: '%d'\n", g_Telemetry.FrameCount );
}

static void OnChangeTelemetryServer ( IConVar *var, const char *pOldValue, float flOldValue )
{
	const char *pServerAddress = (( ConVar* )var)->GetString();

	Q_strncpy( g_Telemetry.ServerAddress, pServerAddress, ARRAYSIZE( g_Telemetry.ServerAddress ) );
	Msg( " TELEMETRY: Setting Telemetry server: '%s'\n", pServerAddress );
}

static void OnChangeTelemetryZoneFilterVal ( IConVar *var, const char *pOldValue, float flOldValue )
{
	char* pIEnd;
	const char *pFilterValue = (( ConVar* )var)->GetString();

	g_Telemetry.ZoneFilterVal = strtoul( pFilterValue, &pIEnd, 0 );
	Msg( " TELEMETRY: Setting Telemetry ZoneFilterVal: '%d'\n", g_Telemetry.ZoneFilterVal );
}

static void OnChangeTelemetryDemoStart ( IConVar *var, const char *pOldValue, float flOldValue )
{
	char* pIEnd;
	const char *pVal = (( ConVar* )var)->GetString();

	g_Telemetry.DemoTickStart = strtoul( pVal, &pIEnd, 0 );
	if( g_Telemetry.DemoTickStart > 2000 )
	{
		char cmd[ 256 ]; 

		// If we're far away from the start of the demo file, then jump to ~1000 ticks before.
		Q_snprintf( cmd, sizeof( cmd ), "demo_gototick %d", g_Telemetry.DemoTickStart - 1000 ); 
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), cmd, kCommandSrcCode, 100 ); 
	}
	Msg( " TELEMETRY: Setting Telemetry DemoTickStart: '%d'\n", g_Telemetry.DemoTickStart );
}

static void OnChangeTelemetryDemoEnd ( IConVar *var, const char *pOldValue, float flOldValue )
{
	char* pIEnd;
	const char *pVal = (( ConVar* )var)->GetString();

	g_Telemetry.DemoTickEnd = strtoul( pVal, &pIEnd, 0 );
	Msg( " TELEMETRY: Setting Telemetry DemoTickEnd: '%d'\n", g_Telemetry.DemoTickEnd );
}

ConVar telemetry_pause( "telemetry_pause", "0", FCVAR_RELEASE, "Pause Telemetry", OnChangeTelemetryPause );
ConVar telemetry_resume( "telemetry_resume", "0", FCVAR_RELEASE, "Resume Telemetry", OnChangeTelemetryResume );
ConVar telemetry_filtervalue( "telemetry_filtervalue", "500", FCVAR_RELEASE, "Set Telemetry ZoneFilterVal (MicroSeconds)", OnChangeTelemetryZoneFilterVal );
ConVar telemetry_framecount( "telemetry_framecount", "0", 0, "Set Telemetry count of frames to capture", OnChangeTelemetryFrameCount );
ConVar telemetry_level( "telemetry_level", "0", FCVAR_RELEASE, "Set Telemetry profile level: 0 being off. Hight bit set for mask: 0x8#######", OnChangeTelemetryLevel );
ConVar telemetry_server( "telemetry_server", "localhost", FCVAR_RELEASE, "Set Telemetry server", OnChangeTelemetryServer );
ConVar telemetry_demostart( "telemetry_demostart", "0", 0, "When playing demo, start telemetry on tick #", OnChangeTelemetryDemoStart );
ConVar telemetry_demoend( "telemetry_demoend", "0", 0, "When playing demo, stop telemetry on tick #", OnChangeTelemetryDemoEnd );

#endif

static unsigned g_MainThreadId = ThreadGetCurrentId();

extern bool gfBackground;

static bool host_checkheap = false;

CCommonHostState host_state;

//-----------------------------------------------------------------------------
#if defined(_X360)
static bool g_bGimped = false;
static int64 *g_pRange1 = NULL;
static int64 *g_pRange2 = NULL;

CON_COMMAND(cache_gimp, "Gimp the cache")
{
	if ( g_bGimped )
	{
		XUnlockL2( XLOCKL2_INDEX_XPS );
		XUnlockL2( XLOCKL2_INDEX_TITLE );
		XPhysicalFree( g_pRange1 );
		XPhysicalFree( g_pRange2 );

		g_pRange1 = g_pRange2 = NULL;

		g_bGimped = false;
	}
	else
	{
		g_pRange1 = ( int64* )XPhysicalAlloc( XLOCKL2_LOCK_SIZE_2_WAYS, MAXULONG_PTR, XLOCKL2_LOCK_SIZE_2_WAYS,	PAGE_READWRITE | MEM_LARGE_PAGES );
		g_pRange2 = ( int64* )XPhysicalAlloc( XLOCKL2_LOCK_SIZE_2_WAYS, MAXULONG_PTR, XLOCKL2_LOCK_SIZE_2_WAYS,	PAGE_READWRITE | MEM_LARGE_PAGES );

		g_bGimped = true;
		XLockL2( XLOCKL2_INDEX_XPS, g_pRange1, XLOCKL2_LOCK_SIZE_2_WAYS, XLOCKL2_LOCK_SIZE_2_WAYS, 0 );
		XLockL2( XLOCKL2_INDEX_TITLE, g_pRange2, XLOCKL2_LOCK_SIZE_2_WAYS, XLOCKL2_LOCK_SIZE_2_WAYS, 0 );

		for ( int i = 0; i < XLOCKL2_LOCK_SIZE_2_WAYS/8; i++ )
		{
			g_pRange1[i] = 0;
			g_pRange2[i] = 0;
		}
	}
}
#endif

//-----------------------------------------------------------------------------

enum HostThreadMode
{
	HTM_DISABLED,
	HTM_DEFAULT,
	HTM_FORCED,
};

// On the PS3, we want host_thead_mode and threaded_sound off.
ConVar host_thread_mode( "host_thread_mode", ( IsPlatformX360() || IsPlatformPS3() ) ? "1" : "0", FCVAR_DEVELOPMENTONLY, "Run the host in threaded mode, (0 == off, 1 == if multicore, 2 == force)" );
ConVar host_threaded_sound( "host_threaded_sound", ( IsPlatformX360() || IsPlatformPS3()) ? "1" : "0", 0, "Run the sound on a thread (independent of mix)" );
ConVar host_threaded_sound_simplethread( "host_threaded_sound_simplethread", ( IsPlatformPS3()) ? "1" : "0", 0, "Run the sound on a simple thread not a jobthread" );
extern ConVar threadpool_affinity;
void OnChangeThreadAffinity( IConVar *var, const char *pOldValue, float flOldValue )
{
	if ( g_pThreadPool->NumThreads() )
	{
		g_pThreadPool->Distribute( threadpool_affinity.GetBool() );
	}
}

ConVar threadpool_affinity( "threadpool_affinity", "1", 0, "Enable setting affinity", 0, 0, 0, 0, &OnChangeThreadAffinity );

extern ConVar threadpool_reserve;
CThreadEvent g_ReleaseThreadReservation( true );
CInterlockedInt g_NumReservedThreads;

void ThreadPoolReserverFunction()
{
	g_ReleaseThreadReservation.Wait();
	--g_NumReservedThreads;
}

void ReserveThreads( int nToReserve )
{
	nToReserve = clamp( nToReserve, 0, g_pThreadPool->NumThreads() );
	g_ReleaseThreadReservation.Set();

	while ( g_NumReservedThreads != 0 )
	{
		ThreadSleep( 0 );
	}

	g_ReleaseThreadReservation.Reset();

	while ( nToReserve-- )
	{
		g_NumReservedThreads++;
		g_pThreadPool->QueueCall( &ThreadPoolReserverFunction )->Release();
	}

	Msg( "%d threads being reserved\n", (int)g_NumReservedThreads );
}

void OnChangeThreadReserve( IConVar *var, const char *pOldValue, float flOldValue )
{
	ReserveThreads( threadpool_reserve.GetInt() );
}

ConVar threadpool_reserve( "threadpool_reserve", "0", 0, "Consume the specified number of threads in the thread pool", 0, 0, 0, 0, &OnChangeThreadReserve );

CON_COMMAND( threadpool_cycle_reserve, "Cycles threadpool reservation by powers of 2" )
{
	int nCores = g_pThreadPool->NumThreads() + 1;
	int nAvailableCores = nCores - g_NumReservedThreads;
	Assert( nAvailableCores );
	int ratio = nCores / nAvailableCores;
	ratio *= 2;
	if ( ratio > nCores )
	{
		ReserveThreads( 0 );
	}
	else
	{
		ReserveThreads( nCores - nCores / ratio );
	}
}

CON_COMMAND( thread_test_tslist, "" )
{
	int nLengthList = ( args.ArgC() == 1 ) ? 1 : atoi( args.Arg( 1 ) );
	int nTests = ( args.ArgC() == 2 ) ? 1 : atoi( args.Arg( 2 ) );
	RunTSListTests( nLengthList, nTests );
}

CON_COMMAND( thread_test_tsqueue, "" )
{
	int nLengthList = ( args.ArgC() < 2 ) ? 10000 : atoi( args.Arg( 1 ) );
	int nTests = ( args.ArgC() < 3 ) ? 1 : atoi( args.Arg( 2 ) );
	RunTSQueueTests( nLengthList, nTests );
}

CON_COMMAND( threadpool_run_tests, "" )
{
	int nTests = ( args.ArgC() == 1 ) ? 1 : atoi( args.Arg( 1 ) );
	for ( int i = 0; i < nTests; i++ )
	{
		RunThreadPoolTests();
	}
}

//-----------------------------------------------------------------------------

/*
A server can always be started, even if the system started out as a client
to a remote system.

A client can NOT be started if the system started as a dedicated server.
Memory is cleared / released when a server or client begins, not when they end.
*/


// Ear position + orientation
#ifndef DEDICATED
CAudioState s_AudioState;

bool CAudioState::IsAnyPlayerUnderwater() const
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		if ( GetPerUser().m_bIsUnderwater )
			return true;
	}
	return false;
}

AudioState_t &CAudioState::GetPerUser( int nSlot /*= -1*/ )
{
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		return m_PerUser[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
	}
	return m_PerUser[ nSlot ];
}

const AudioState_t &CAudioState::GetPerUser( int nSlot /*= -1*/ ) const
{
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		return m_PerUser[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
	}
	return m_PerUser[ nSlot ];
}

#endif  // DEDICATED

engineparms_t host_parms;

bool		host_initialized = false;		// true if into command execution

float		host_frametime = 0.0f;
float		host_frametime_unbounded = 0.0f;
float		host_frametime_stddeviation = 0.0f;
float		host_framestarttime_stddeviation = 0.0f;
float		host_frameendtime_computationduration = 0.0f;
float		host_frametime_unscaled = 0.0f;
double		realtime = 0;			// without any filtering or bounding
double		host_idealtime = 0;		// "ideal" server time assuming perfect tick rate
float		host_nexttick = 0;		// next server tick in this many ms
float		host_jitterhistory[128] = { 0 };
unsigned int host_jitterhistorypos = 0;

int			host_framecount;
static int	host_hunklevel;

CGameClient	*host_client;			// current client

jmp_buf 	host_abortserver;
jmp_buf     host_enddemo;

static ConVar	host_profile( "host_profile","0" );

ConVar	skill( "skill","1", FCVAR_ARCHIVE, "Game skill level (1-3).", true, 1, true, 3 );			// 1 - 3
ConVar	host_timescale( "host_timescale","1.0", FCVAR_REPLICATED | FCVAR_CHEAT, "Prescale the clock by this amount." );

ConVar	host_limitlocal( "host_limitlocal", "0", 0, "Apply cl_cmdrate and cl_updaterate to loopback connection" );
ConVar	host_framerate( "host_framerate","0", FCVAR_REPLICATED | FCVAR_CHEAT, "Set to lock per-frame time elapse." );
ConVar	host_speeds( "host_speeds","0", 0, "Show general system running times." );		// set for running times

ConVar  developer( "developer", "0", FCVAR_RELEASE, "Set developer message level");

ConVar	deathmatch( "deathmatch","0", FCVAR_NOTIFY, "Running a deathmatch server." );	// 0, 1, or 2
ConVar	coop( "coop","0", FCVAR_NOTIFY, "Cooperative play." );			// 0 or 1
ConVar	r_ForceRestore( "r_ForceRestore", "0", 0 );

CON_COMMAND_F( display_elapsedtime, "Displays how much time has elapsed since the game started", FCVAR_CHEAT )
{
	Msg( "Elapsed time: %.2f\n", realtime );
}

CON_COMMAND( host_timer_report, "Spew CPU timer jitter for the last 128 frames in microseconds (dedicated only)" )
{
	if ( sv.IsDedicated() )
	{
		for (int i = 1; i <= ARRAYSIZE( host_jitterhistory ); ++i)
		{
			unsigned int slot = ( i + host_jitterhistorypos ) % ARRAYSIZE( host_jitterhistory );
			Msg( "%7d\n", ( int ) ( host_jitterhistory[ slot ] * 1000000 ) );
		}
	}
}

ConVar host_runframe_input_parcelremainder( "host_runframe_input_parcelremainder", "1" ); //putting this on a ConVar only because we're shipping so soon

#ifndef DEDICATED
void CL_CheckToDisplayStartupMenus(); // in cl_main.cpp
#endif


bool GetFileFromRemoteStorage( ISteamRemoteStorage *pRemoteStorage, const char *pszRemoteFileName, const char *pszLocalFileName, char const *pathID )
{
	bool bSuccess = false;

	// check if file exists in Steam Cloud first
	int32 nFileSize = pRemoteStorage->GetFileSize( pszRemoteFileName );

	if ( nFileSize > 0 )
	{
		CUtlMemory<char> buf( 0, nFileSize );
		if ( pRemoteStorage->FileRead( pszRemoteFileName, buf.Base(), nFileSize ) == nFileSize )
		{
			FileHandle_t hFile = g_pFileSystem->Open( pszLocalFileName, "wb", pathID );
			if( hFile )
			{
				bSuccess = g_pFileSystem->Write( buf.Base(), nFileSize, hFile ) == nFileSize;
				g_pFileSystem->Close( hFile );

				if ( bSuccess )
				{
					DevMsg( "[Cloud]: SUCCEESS retrieved %s from remote storage into %s\n", pszRemoteFileName, pszLocalFileName );
				}
				else
				{
					DevMsg( "[Cloud]: FAILED retrieved %s from remote storage into %s\n", pszRemoteFileName, pszLocalFileName );
				}
			}
		}
	}

	return bSuccess;
}


void CCommonHostState::SetWorldModel( model_t *pModel )
{
	if ( worldmodel == pModel )
		return;

	worldmodel = pModel;
	if ( pModel )
	{
		worldbrush = pModel->brush.pShared;
	}
	else
	{
		worldbrush = NULL;
	}
}

#ifndef DEDICATED
void Host_SetAudioState( const AudioState_t &audioState )
{
	memcpy( &s_AudioState.GetPerUser(), &audioState, sizeof(AudioState_t) );
}
#endif

bool Host_IsLocalServer()
{
	return sv.IsActive();
}

bool Host_IsSinglePlayerGame( void )
{
	if ( sv.IsActive() )
	{
		return !sv.IsMultiplayer();
	}
	else
	{
#ifdef DEDICATED
		return false;
#else
		return GetBaseLocalClient().m_nMaxClients == 1;
#endif
	}
}

/*
================
Host_EndGame
================
*/
void Host_EndGame (bool bShowMainMenu, const char *message, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,message);
	Q_vsnprintf (string,sizeof(string),message,argptr);
	va_end (argptr);
	ConMsg ("Host_EndGame: %s\n",string);

#ifndef DEDICATED
	scr_disabled_for_loading = true;
	int oldn = GetBaseLocalClient().demonum;
	GetBaseLocalClient().demonum = -1;
#endif

	Host_Disconnect(bShowMainMenu);

#ifndef DEDICATED
	GetBaseLocalClient().demonum = oldn;
#endif

	if ( sv.IsDedicated() )
	{
		Sys_Error ("Host_EndGame: %s\n",string);	// dedicated servers exit
		return;
	}
	bool bDemoEnd = false;
#ifndef DEDICATED
	if (GetBaseLocalClient().demonum != -1)
	{
		bDemoEnd = true;
	}
#endif
	if ( bDemoEnd )
	{
#ifndef DEDICATED
		CL_NextDemo ();
#endif		
		longjmp (host_enddemo, 1);
	}
	else
	{

#ifndef DEDICATED
		scr_disabled_for_loading = false;
#endif
		if ( g_bAbortServerSet )
		{
			longjmp (host_abortserver, 1);
		}
	}
}

/*
================
Host_Error

This shuts down both the client and server
================
*/
void Host_Error (const char *error, ...)
{
	va_list		argptr;
	char		string[1024];
	static	bool inerror = false;

	if (inerror)
	{
		Sys_Error ("Host_Error: recursively entered");
	}
	inerror = true;

#ifndef DEDICATED
	//	CL_WriteMessageHistory();	TODO must be done by network layer
#endif

	va_start (argptr,error);
	Q_vsnprintf(string,sizeof(string),error,argptr);
	va_end (argptr);

	if ( sv.IsDedicated() )
	{
		// dedicated servers just exit
		Sys_Error( "Host_Error: %s\n", string );
		return;
	}

#ifndef DEDICATED
	// Reenable screen updates
	SCR_EndLoadingPlaque ();		
#endif
	ConMsg( "\nHost_Error: %s\n\n", string );

	Host_Disconnect(true);

#ifndef DEDICATED
	GetBaseLocalClient().demonum = -1;
#endif

	inerror = false;

	if ( g_bAbortServerSet )
	{
		longjmp (host_abortserver, 1);
	}
}

#ifndef DEDICATED
class CHostSubscribeForProfileEvents : public IMatchEventsSink
{
public:
	CHostSubscribeForProfileEvents() : m_bSubscribed( false ) {}

public:
	void Subscribe( bool bSubscribe );
	virtual void OnEvent( KeyValues *pEvent );

public:
	bool m_bSubscribed;
};

void CHostSubscribeForProfileEvents::Subscribe( bool bSubscribe )
{
	if ( bSubscribe == m_bSubscribed )
		return;
	if ( !g_pMatchFramework )
		return;

	if ( bSubscribe )
		g_pMatchFramework->GetEventsSubscription()->Subscribe( this );
	else
		g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );

	m_bSubscribed = bSubscribe;
}

void CHostSubscribeForProfileEvents::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( !Q_stricmp( szEvent, "OnProfileDataLoaded" ) )
	{
		int iController = pEvent->GetInt( "iController" );
		Host_ReadConfiguration( iController, false );
	}
	if ( !Q_stricmp( szEvent, "OnProfileDataLoadFailed" ) )
	{
#if defined ( _X360 )
		int iController = pEvent->GetInt( "iController" );
		int iSlot = XBX_GetSlotByUserId( iController );
		
		ECommandMsgBoxSlot slot = CMB_SLOT_FULL_SCREEN;
		char cmdLine[80];

		if ( iSlot == 0 )
		{
			sprintf( cmdLine, "boot_to_start_and_reset_config 0" );
			slot = CMB_SLOT_PLAYER_0;
		}
		else
		{
			sprintf( cmdLine, "boot_to_start_and_reset_config 1" );
			slot = CMB_SLOT_PLAYER_1;
		}
		if ( !GetGameUI()->IsInLevel() )
		{
			slot = CMB_SLOT_FULL_SCREEN;
		}

		if ( !g_pXboxSystem->IsArcadeTitleUnlocked() )
		{
			GetGameUI()->CreateCommandMsgBoxInSlot( 
				slot, 
				"#SFUI_GameUI_ProfileDataLoadFailedTitle", 
				"#SFUI_GameUI_ProfileDataLoadFailedTrialMsg", 
				true, 
				false,  
				"boot_to_start", 
				NULL,
				NULL, 
				NULL );

			return;
		}

		GetGameUI()->CreateCommandMsgBoxInSlot( 
			slot, 
			"#SFUI_GameUI_ProfileDataLoadFailedTitle", 
			"#SFUI_GameUI_ProfileDataLoadFailedMsg", 
			true, 
			true, 
			cmdLine, 
			"boot_to_start", 
			NULL, 
			NULL );
#endif
	}
	if ( !Q_stricmp( szEvent, "OnProfileDataWriteFailed" ) )
	{
#if defined ( _X360 )
		int iController = pEvent->GetInt( "iController" );
		int iSlot = XBX_GetSlotByUserId( iController );

		ECommandMsgBoxSlot slot = CMB_SLOT_FULL_SCREEN;
		if ( GetGameUI()->IsInLevel() )
		{
			if ( iSlot == 0 )
			{
				slot = CMB_SLOT_PLAYER_0;
			}
			else
			{
				slot = CMB_SLOT_PLAYER_1;
			}
		}

		// are we in trial mode?
		if ( !g_pXboxSystem->IsArcadeTitleUnlocked() )
		{
			GetGameUI()->CreateCommandMsgBoxInSlot( 
				slot, 
				"#SFUI_TrialMUPullTitle", 
				"#SFUI_TrialMUPullMsg", 
				true, 
				false, 
				"boot_to_start", 
				NULL, 
				NULL, 
				NULL );

			return;
		}

		GetGameUI()->CreateCommandMsgBoxInSlot( 
			slot, 
			"#SFUI_GameUI_ProfileDataWriteFailedTitle", 
			"#SFUI_GameUI_ProfileDataWriteFailedMsg", 
			true, 
			false, 
			NULL, 
			NULL, 
			NULL, 
			NULL );
#endif
	}

#if defined ( _X360 )

	else if ( !Q_stricmp( szEvent, "OnProfilesChanged" ) )
	{

		// $TODO(hpe) this needs reworked for split screen; currently, will reset configs for all players when one active player signs out
		Host_ResetGlobalConfiguration();
		for ( DWORD iSplitscreenSlot = 0; iSplitscreenSlot < XBX_GetNumGameUsers(); ++ iSplitscreenSlot )
		{
			Host_ResetConfiguration( XBX_GetUserId( iSplitscreenSlot ) );
		}

	}

#endif

#if defined ( _PS3 )

	else if ( !Q_stricmp( szEvent, "ResetConfiguration" ) )
	{
		int iController = pEvent->GetInt( "iController" );
		// $TODO(hpe) this needs reworked for split screen; currently, will reset configs for all players when one active player signs out
		Host_ResetGlobalConfiguration();
		Host_ResetConfiguration( iController );
		Host_WriteConfiguration( iController, "" );
	}

#endif

}
#endif

#ifdef _GAMECONSOLE
void Console_UpdateNotificationPosition()
{
	static ConVarRef closecaption( "closecaption" );
	static ConVarRef cc_subtitles( "cc_subtitles" );
	bool bSubtitled = ( ( closecaption.IsValid() && closecaption.GetBool() ) ||
		( cc_subtitles.IsValid() && cc_subtitles.GetBool() ) );
#ifdef _X360
	XNotifyPositionUI( bSubtitled ? XNOTIFYUI_POS_TOPRIGHT : XNOTIFYUI_POS_BOTTOMCENTER );
	Msg( "XNotifyPositionUI: %s\n", bSubtitled ? "TOPRIGHT" : "BOTTOMCENTER" );
#endif
}
#endif

void Host_SubscribeForProfileEvents( bool bSubscribe )
{
#ifndef DEDICATED
	static CHostSubscribeForProfileEvents s_HostSubscribeForProfileEvents;
	s_HostSubscribeForProfileEvents.Subscribe( bSubscribe );
#endif
}

#ifndef DEDICATED
//******************************************
// UseDefuaultBinding
//
// If the config.cfg file is not present, this
// function is called to set the default key
// bindings to match those defined in kb_def.lst
//******************************************
void UseDefaultBindings( void )
{
	FileHandle_t f;
	char szFileName[ _MAX_PATH ];
	char token[ 1024 ];
	char szKeyName[ 256 ];

	// read kb_def file to get default key binds
	Q_snprintf( szFileName, sizeof( szFileName ), "%skb_def.lst", SCRIPT_DIR );
	f = g_pFileSystem->Open( szFileName, "r");
	if ( !f )
	{
		ConMsg( "Couldn't open kb_def.lst\n" );
		return;
	}

	// read file into memory
	int size = g_pFileSystem->Size(f);
	char *startbuf = new char[ size ];
	g_pFileSystem->Read( startbuf, size, f );
	g_pFileSystem->Close( f );

	const char *buf = startbuf;
	while ( 1 )
	{
		buf = COM_ParseFile( buf, token, sizeof( token ) );
		if ( strlen( token ) <= 0 )
			break;
		Q_strncpy ( szKeyName, token, sizeof( szKeyName ) );

		buf = COM_ParseFile( buf, token, sizeof( token ) );
		if ( strlen( token ) <= 0 )  // Error
			break;

		// finally, bind key
		Key_SetBinding ( g_pInputSystem->StringToButtonCode( szKeyName ), token );
	}
	delete [] startbuf;		// cleanup on the way out
}

static bool g_bConfigCfgExecuted[MAX_SPLITSCREEN_CLIENTS];

enum SyncCvarValueWithPlayerTitleDataPolicy_t
{
	CVARWRITETD,
	CVARREADTD
};
static void SyncCvarValueWithPlayerTitleData( IPlayerLocal *pPlayer, ConVar *cv, TitleDataFieldsDescription_t const *pField, SyncCvarValueWithPlayerTitleDataPolicy_t eCV )
{
	switch( pField->m_eDataType )
	{
	case TitleDataFieldsDescription_t::DT_float:
		if ( eCV == CVARWRITETD )
			TitleDataFieldsDescriptionSetValue<float>( pField, pPlayer, cv->GetFloat() );
		else if ( eCV == CVARREADTD )
			cv->SetValue( TitleDataFieldsDescriptionGetValue<float>( pField, pPlayer ) );
		break;
	case TitleDataFieldsDescription_t::DT_uint32:
		if ( eCV == CVARWRITETD )
			TitleDataFieldsDescriptionSetValue<int32>( pField, pPlayer, cv->GetInt() );
		else if ( eCV == CVARREADTD )
			cv->SetValue( TitleDataFieldsDescriptionGetValue<int32>( pField, pPlayer ) );
		break;
	case TitleDataFieldsDescription_t::DT_uint16:
		if ( eCV == CVARWRITETD )
			TitleDataFieldsDescriptionSetValue<int16>( pField, pPlayer, cv->GetInt() );
		else if ( eCV == CVARREADTD )
			cv->SetValue( TitleDataFieldsDescriptionGetValue<int16>( pField, pPlayer ) );
		break;
	case TitleDataFieldsDescription_t::DT_uint8:
		if ( eCV == CVARWRITETD )
			TitleDataFieldsDescriptionSetValue<int8>( pField, pPlayer, cv->GetInt() );
		else if ( eCV == CVARREADTD )
			cv->SetValue( TitleDataFieldsDescriptionGetValue<int8>( pField, pPlayer ) );
		break;
	case TitleDataFieldsDescription_t::DT_BITFIELD:
		if ( eCV == CVARWRITETD )
			TitleDataFieldsDescriptionSetBit( pField, pPlayer, cv->GetBool() );
		else if ( eCV == CVARREADTD )
			cv->SetValue( !!TitleDataFieldsDescriptionGetBit( pField, pPlayer ) );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Write out our 360 exclusive settings to internal storage
//-----------------------------------------------------------------------------
void Host_WriteConfiguration_Console( const int iController, bool bVideoConfig )
{
	// sb: FIXME(hpe) only write if we had a valid read so we don't accidentally wipe out valid data
#ifdef _GAMECONSOLE
	DevMsg( "Host_WriteConfiguration_Console for ctrlr%d (%s)\n", iController, bVideoConfig ? "video" : "controls" );

	if ( iController < 0 )
		return;

	if ( !g_pMatchFramework || !g_pMatchFramework->GetMatchTitle() )
		return;

	IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
	if ( !pPlayer )
		return;

#if defined ( _X360 )
	if ( !pPlayer->IsTitleDataValid() )
		return;
#endif

#ifndef GAME_DLL
	if ( g_pXboxSystem->IsArcadeTitleUnlocked() )
	{
		IGameEvent *event = g_GameEventManager.CreateEvent( "write_game_titledata" );
		if ( event )
		{
			event->SetInt( "controllerId", iController );
			g_GameEventManager.FireEventClientSide( event );
		}
	}
#endif

	int iSlot = XBX_GetSlotByUserId( iController );

	// check version number for title data block 3
	TitleDataFieldsDescription_t const *fields = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();
	if ( !fields )
		return;

#if defined ( _X360 )

	TitleDataFieldsDescription_t const *versionField = TitleDataFieldsDescriptionFindByString( fields, "TITLEDATA.BLOCK3.VERSION" );
	if ( !versionField || versionField->m_eDataType != TitleDataFieldsDescription_t::DT_uint16)
	{
		Warning( "Host_WriteConfiguration_Console missing or incorrect type TITLEDATA.BLOCK3.VERSION\n" );
		return;
	}

	ConVarRef cl_titledataversionblock3 ( "cl_titledataversionblock3" );
	TitleDataFieldsDescriptionSetValue<uint16>( versionField, pPlayer, cl_titledataversionblock3.GetInt() );

#endif

	// On consoles - store guest user's convars in primary user's profile data
	char const *szUsrField = "";
	if ( XBX_GetUserIsGuest( iSlot ) && !bVideoConfig )
	{
		IPlayerLocal *pPlayerPrimary = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetPrimaryUserId() );
		if ( pPlayerPrimary )
		{
			if ( TitleDataFieldsDescription_t const *pOffset = TitleDataFieldsDescriptionFindByString( fields, TITLE_DATA_PREFIX "CFG.usrSS.version" ) )
			{
				pPlayer = pPlayerPrimary;
				szUsrField = "SS";
			}
		}
	}

	int numLoops = 1;
#if defined (CSTRIKE15)
	// Console CStrike15 we want to always save both usr and sys Convars
	numLoops = 2;
#endif

	for ( int loopCount=0; loopCount<numLoops; ++loopCount )
	{
		// second pass toggle bVideoConfig to go from sys to usr or vice versa
		if ( loopCount == 1 )
			bVideoConfig = !bVideoConfig;

		CUtlVector< ConVar * > arrCVars;
		cv->WriteVariables( NULL, iSlot, !bVideoConfig, &arrCVars );
		for ( int k = 0; k < arrCVars.Count(); ++ k )
		{
			ConVar *cvSave = arrCVars[k];
			char const *cvSaveName = cvSave->GetBaseName();
			CFmtStr sFieldLookup( TITLE_DATA_PREFIX "CFG.%s%s.%s", bVideoConfig ? "sys" : "usr", szUsrField, cvSaveName );
			TitleDataFieldsDescription_t const *pField = TitleDataFieldsDescriptionFindByString( fields, sFieldLookup );
			if ( !pField )
			{
				Warning( "Host_WriteConfiguration_Console (%s#%d) - cannot save cvar %s\n", bVideoConfig ? "video" : "ctrlr", iController, cvSaveName );
				continue;
			}
			SyncCvarValueWithPlayerTitleData( pPlayer, cvSave, pField, CVARWRITETD );
		}

		// reset the input parameter
		if ( loopCount == 1 )
			bVideoConfig = !bVideoConfig;
	}

	// Update the version number for the settings.  This is the main version number for PS3.
	if ( TitleDataFieldsDescription_t const *pVersion = TitleDataFieldsDescriptionFindByString( fields, TITLE_DATA_PREFIX "CFG.sys.version" ) )
	{
		TitleDataFieldsDescriptionSetValue<int32>( pVersion, pPlayer, cl_configversion.GetInt() );
	}

	// Let the player manager save the user data
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfilesWriteOpportunity", "reason", "settings" ) );

	// Update notifications position
	Console_UpdateNotificationPosition();
#endif // #ifdef _GAMECONSOLE
}

bool Host_WasConfigCfgExecuted( const int iController )
{
	int iIndex = iController;
	if ( iIndex < 0 )
		iIndex = 0;
	Assert( iIndex >= 0 && iIndex < MAX_SPLITSCREEN_CLIENTS );

	return g_bConfigCfgExecuted[iIndex];
}

void Host_SetConfigCfgExecuted( const int iController, bool bExecuted = true )
{
	int iIndex = iController;
	if ( iIndex < 0 )
		iIndex = 0;
	Assert( iIndex >= 0 && iIndex < MAX_SPLITSCREEN_CLIENTS );

	g_bConfigCfgExecuted[iIndex] = bExecuted;
}

void Host_ResetGlobalConfiguration()
{
#ifdef _GAMECONSOLE
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
#endif

#ifndef _GAMECONSOLE
	// We exec our global default configuration for non-consoles
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), "exec config.global" PLATFORM_EXT ".cfg game\n" );
	Cbuf_Execute();
#else
	CUtlVector< ConVar * > arrCVars;
	cv->WriteVariables( NULL, 0, false, &arrCVars );
	for ( int k = 0; k < arrCVars.Count(); ++ k )
	{
		ConVar *cvSave = arrCVars[k];
		cvSave->Revert();
		DevMsg( "Console reset global configuration: %s = \"%s\"\n", cvSave->GetName(), cvSave->GetString() );
	}
#endif

#ifdef _GAMECONSOLE
	// Update notifications position
	Console_UpdateNotificationPosition();
#endif
}

void Host_ResetConfiguration( const int iController )
{

#if defined ( _GAMECONSOLE )

#ifndef SPLIT_SCREEN_STUBS

	int iSlot = XBX_GetSlotByUserId( iController );
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( iSlot );

#endif

	// First, we exec our default configuration
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), "exec config" PLATFORM_EXT ".cfg game\n" );
	Cbuf_Execute();

#if defined ( _X360 )

	// This will wipe out all achievement and stats but they'll be loaded from the xlast title data sync.
	IGameEvent *event = g_GameEventManager.CreateEvent( "reset_game_titledata" );
	if ( event )
	{
		event->SetInt( "controllerId", iController );
		g_GameEventManager.FireEventClientSide( event );
	}

	// Get and set all our default setting we care about from the Xbox
	XBX_SetProfileDefaultSettings( iController );

	IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
	if ( pPlayer )
	{
		pPlayer->SetIsTitleDataValid( true );
	}

#else

#if defined( _PS3 )

	char szScratch[MAX_PATH];
	int iAllDevices = -1;
	Q_snprintf( szScratch, sizeof(szScratch), "cl_reset_ps3_bindings %d %d", iController, iAllDevices );
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), szScratch );
	Cbuf_Execute();

#endif // _PS3


	// Get and set all our default setting we care about.
	XBX_SetProfileDefaultSettings( iController );

#endif // _X360


#endif // _GAMECONSOLE

}

/*
===============
Host_WriteConfiguration

Writes key bindings and archived cvars to config.cfg
===============
*/

void Host_WriteConfiguration( const int iController, const char *filename )
{
	// Set the joystick being force disabled just as we write the config
	// This allows us to chose this option in the menu with a controller without accidentally disabling our only mode of input
	static ConVarRef joystick_force_disabled( "joystick_force_disabled" );
	static ConVarRef joystick_force_disabled_set_from_options( "joystick_force_disabled_set_from_options" );
	if ( joystick_force_disabled.IsValid() && joystick_force_disabled_set_from_options.IsValid() )
	{
		if ( joystick_force_disabled.GetBool() != joystick_force_disabled_set_from_options.GetBool() )
		{
			joystick_force_disabled.SetValue( joystick_force_disabled_set_from_options.GetBool() );
		}
	}

	if ( !filename )
		filename = "config.cfg";
	// Write to internal storage on the 360
	if ( IsGameConsole() )
	{
		Host_WriteConfiguration_Console( iController, false );
		return;
	}
	
	if ( !host_initialized )
	{
		return;
	}
	
	if ( Host_WasConfigCfgExecuted( iController ) == false )
	{
		return;
	}

	// If in map editing mode don't save configuration
	if (g_bInEditMode)
	{
		ConMsg( "skipping %s output when in map edit mode\n", filename );
		return;
	}

	// dedicated servers initialize the host but don't parse and set the
	// config.cfg cvars
	if ( !sv.IsDedicated() )
	{
		if ( IsPC() && Key_CountBindings() <= 1 )
		{
			ConMsg( "skipping %s output, no keys bound\n", filename );
			return;
		}

		// force any queued convar changes to flush before reading/writing them
		UpdateMaterialSystemConfig();

		// Generate a new .cfg file.
		char		szFileName[MAX_PATH];
		CUtlBuffer	configBuff( 0, 0, CUtlBuffer::TEXT_BUFFER);

		Q_snprintf( szFileName, sizeof(szFileName), "cfg/%s", filename );
		g_pFileSystem->CreateDirHierarchy( "cfg", "USRLOCAL" );
		if ( g_pFileSystem->FileExists( szFileName, "USRLOCAL" ) && !g_pFileSystem->IsFileWritable( szFileName, "USRLOCAL" ) )
		{
			ConMsg( "Config file %s is read-only!!\n", szFileName );
			return;
		}
		
		// Always throw away all keys that are left over.
		configBuff.Printf( "unbindall\n" );

		Key_WriteBindings( configBuff );
		ConVarUtilities->WriteVariables( &configBuff );

#if !defined( DEDICATED )
		bool down;
		for ( int hh = 0; hh < host_state.max_splitscreen_players; ++hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
			if ( g_ClientDLL->IN_IsKeyDown( "in_jlook", down ) && down )
			{
				configBuff.Printf( "cmd%d +jlook\n", hh+1 );
			}
		}
#endif // DEDICATED

		if ( !configBuff.TellMaxPut() )
		{
			// nothing to write
			return;
		}

#if defined(NO_STEAM)
		AssertMsg( false, "SteamCloud not available on Xbox 360. Badger Martin to fix this." );
#else
		ISteamRemoteStorage *pRemoteStorage =
#ifdef _PS3
			::SteamRemoteStorage();
#else
			Steam3Client().SteamClient() ? (ISteamRemoteStorage *) Steam3Client().SteamClient()->GetISteamGenericInterface(
			SteamAPI_GetHSteamUser(), SteamAPI_GetHSteamPipe(), STEAMREMOTESTORAGE_INTERFACE_VERSION ):NULL;
#endif

		if ( pRemoteStorage )
		{
			int32 availableBytes, totalBytes = 0;
			if ( pRemoteStorage->GetQuota( &totalBytes, &availableBytes ) )
			{
				if ( totalBytes > 0 )
				{

					if ( cl_cloud_settings.GetInt() != -1 && ( cl_cloud_settings.GetInt() & STEAMREMOTESTORAGE_CLOUD_CONFIG ) )
					{
						// TODO put MOD dir in pathname
						if ( pRemoteStorage->FileWrite( szFileName, configBuff.Base(), configBuff.TellMaxPut() ) )
						{
							// Refresh local copy so that on next game boot Host_ReadPreStartupConfiguration() will avoid loading the stale config.cfg
							// which will only get downloaded & refreshed later during Host_ReadConfiguration()
							GetFileFromRemoteStorage( pRemoteStorage, "cfg/config.cfg", "cfg/config.cfg", "USRLOCAL" );

							DevMsg( "[Cloud]: SUCCEESS saving %s in remote storage\n", szFileName );
						}
						else
						{
							// probably a quota issue. TODO what to do ?
							DevMsg( "[Cloud]: FAILED saving %s in remote storage\n", szFileName );
						}
					}
				}
			}

			// even if SteamCloud worked we still safe the same file locally
		}
#endif


		// make a persistent copy that async will use and free
		char *tempBlock = new char[configBuff.TellMaxPut()];
		Q_memcpy( tempBlock, configBuff.Base(), configBuff.TellMaxPut() );

		// async write the buffer, and then free it
		char		szFileNameToWriteLocal[MAX_PATH] = {};
		g_pFileSystem->GetSearchPath( "USRLOCAL", false, szFileNameToWriteLocal, sizeof( szFileNameToWriteLocal ) );
		V_strcat_safe( szFileNameToWriteLocal, szFileName );
		g_pFileSystem->AsyncWrite( szFileNameToWriteLocal, tempBlock, configBuff.TellMaxPut(), true );

		ConMsg( "Host_WriteConfiguration: Wrote %s\n", szFileName );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Retrieve and set any defaults from the user's gamer profile
//-----------------------------------------------------------------------------
bool XBX_SetProfileDefaultSettings( int iController )
{
	// These defined values can't play nicely with the PC, so we need to ignore them for that build target
#ifdef _GAMECONSOLE
	UserProfileData upd = {0};
	if ( IPlayerLocal *pPlayerLocal = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController ) )
	{
		upd = pPlayerLocal->GetPlayerProfileData();
	}

	//
	// Skill
	//

	int nSkillSetting = upd.difficulty;
	int nResultSkill = 2;
#ifdef _X360
	switch( nSkillSetting )
	{
	case XPROFILE_GAMER_DIFFICULTY_HARD:
		nResultSkill = 3;
		break;
	
	case XPROFILE_GAMER_DIFFICULTY_EASY:
	default:
		nResultSkill = 1;
		break;
	}
#endif

	// If the mod has no difficulty setting, only easy is allowed
	KeyValues *modinfo = new KeyValues("ModInfo");
	if ( modinfo->LoadFromFile( g_pFileSystem, "gameinfo.txt" ) )
	{
		if ( stricmp(modinfo->GetString("nodifficulty", "0"), "1") == 0 )
			nResultSkill = 1;
	}
	modinfo->deleteThis();

	char szScratch[MAX_PATH];
	Q_snprintf( szScratch, sizeof(szScratch), "skill %d", nResultSkill );
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), szScratch );

	// 
	// Movement control
	//

	int nMovementControl = !!upd.action_movementcontrol;

	Q_snprintf( szScratch, sizeof(szScratch), "joy_movement_stick %d", nMovementControl );
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), szScratch );

	Cbuf_AddText( Cbuf_GetCurrentPlayer(), "joyadvancedupdate" );

	// 
	// Y-Inversion
	//

	int nYinvert = !!upd.yaxis;
	
	Q_snprintf( szScratch, sizeof(szScratch), "joy_inverty %d", nYinvert );
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), szScratch );
	
	//
	// Vibration control
	//

	int nVibration = !!upd.vibration;

	Q_snprintf( szScratch, sizeof(szScratch), "cl_rumblescale %d", nVibration );
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), szScratch );

	// Execute all commands we've queued up
	Cbuf_Execute();
#endif // _GAMECONSOLE

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Read our configuration from the 360, filling in defaults on our first run
//-----------------------------------------------------------------------------
void Host_ReadConfiguration_Console( const int iController )
{

#ifdef _GAMECONSOLE

	if ( iController < 0 )
		return;

	int iSlot = XBX_GetSlotByUserId( iController );

	DevMsg( "Host_ReadConfiguration_Console maps ctrlr%d to slot%d\n", iController, iSlot );

	if ( !g_pMatchFramework || !g_pMatchFramework->GetMatchTitle() )
		return;

	IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
	if ( !pPlayer )
		return;

	TitleDataFieldsDescription_t const *fields = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();
	if ( !fields )
		return;
	
#if defined( _X360 )

	Host_ResetConfiguration( iController );

	// dont read any config info if the load was not valid
	if ( !pPlayer->IsTitleDataValid() )
		return;

	// if the trial block is fresh, reset the trial timer to clear out any previous signed in players time
	if ( !pPlayer->IsTitleDataBlockValid( 2 ) )
	{
		SplitScreenConVarRef xbox_arcade_remaining_trial_time("xbox_arcade_remaining_trial_time");
		xbox_arcade_remaining_trial_time.SetValue( iSlot, xbox_arcade_remaining_trial_time.GetDefault() );
	}

	if ( pPlayer->IsFreshPlayerProfile() )
		return;

#ifndef GAME_DLL

	if ( pPlayer->IsTitleDataBlockValid( 0 ) && pPlayer->IsTitleDataBlockValid( 1 ) )
	{
		IGameEvent *event = g_GameEventManager.CreateEvent( "read_game_titledata" );
		if ( event )
		{
			event->SetInt( "controllerId", iController );
			g_GameEventManager.FireEventClientSide( event );
		}
	}

#endif //GAME_DLL


	// check version numbers

	ConVarRef cl_titledataversionblock3 ( "cl_titledataversionblock3" );
	TitleDataFieldsDescription_t const *versionField3 = TitleDataFieldsDescriptionFindByString( fields, "TITLEDATA.BLOCK3.VERSION" );

#if !defined( _CERT )

	// check to see if profile versions match; reset if not
	ConVarRef cl_titledataversionblock1 ( "cl_titledataversionblock1" );
	ConVarRef cl_titledataversionblock2 ( "cl_titledataversionblock2" );
	TitleDataFieldsDescription_t const *versionField1 = TitleDataFieldsDescriptionFindByString( fields, "TITLEDATA.BLOCK1.VERSION" );
	if ( !versionField1 || versionField1->m_eDataType != TitleDataFieldsDescription_t::DT_uint16)
	{
		Warning( "Host_ReadConfiguration_Console missing or incorrect type TITLEDATA.BLOCK1.VERSION\n" );
		return;
	}

	TitleDataFieldsDescription_t const *versionField2 = TitleDataFieldsDescriptionFindByString( fields, "TITLEDATA.BLOCK2.VERSION" );
	if ( !versionField2 || versionField2->m_eDataType != TitleDataFieldsDescription_t::DT_uint16)
	{
		Warning( "Host_ReadConfiguration_Console missing or incorrect type TITLEDATA.BLOCK2.VERSION\n" );
		return;
	}
	
	bool versionValid = true;

	if ( g_pXboxSystem->IsArcadeTitleUnlocked() )
	{
		if ( ( pPlayer->IsTitleDataBlockValid( 0 ) && cl_titledataversionblock1.GetInt() != TitleDataFieldsDescriptionGetValue<uint16>( versionField1, pPlayer ) ) ||
			 ( pPlayer->IsTitleDataBlockValid( 1 ) && cl_titledataversionblock2.GetInt() != TitleDataFieldsDescriptionGetValue<uint16>( versionField2, pPlayer ) ) ||
			 ( pPlayer->IsTitleDataBlockValid( 2 ) && cl_titledataversionblock3.GetInt() != TitleDataFieldsDescriptionGetValue<uint16>( versionField3, pPlayer ) ) )
		{
			versionValid = false;
		}
	}
	else
	{
		if ( pPlayer->IsTitleDataBlockValid( 2 ) && cl_titledataversionblock3.GetInt() != TitleDataFieldsDescriptionGetValue<uint16>( versionField3, pPlayer ) )
		{
			versionValid = false;
		}
	}

	if ( !versionValid )
	{
		Warning( "ProfileVersion is out of date; your profile has been reset\n" );

		// zero out title data buffer to remove any stale data since we are basically nuking this profile data
		IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
		if ( pPlayer )
		{
			pPlayer->ClearBufTitleData();
		}

		Host_ResetConfiguration( iController );
		Host_WriteConfiguration( iController, "" );
		GetGameUI()->ShowMessageDialog( "Your profile version was out of date; the profile has been reset.", "Profile Version Mismatch" );
		return;
	}

#endif // !_CERT

	if ( !versionField3 || versionField3->m_eDataType != TitleDataFieldsDescription_t::DT_uint16)
	{
		Warning( "Host_ReadConfiguration_Console missing or incorrect type TITLEDATA.BLOCK3.VERSION\n" );
		return;
	}
	
	if ( cl_titledataversionblock3.GetInt() != TitleDataFieldsDescriptionGetValue<uint16>( versionField3, pPlayer ) )
	{
		Warning( "Host_ReadConfiguration_Console wrong version number for TITLEDATA.BLOCK3.VERSION; expected %d, got %d\n", 
				cl_titledataversionblock3.GetInt(), TitleDataFieldsDescriptionGetValue<uint16>( versionField3, pPlayer ) );
		return;
	}

#else

	// If not on 360, we read the game title data reguardless.
	IGameEvent *event = g_GameEventManager.CreateEvent( "read_game_titledata" );
	if ( event )
	{
		event->SetInt( "controllerId", iController );
		g_GameEventManager.FireEventClientSide( event );
	}

#endif // _X360

	// usr data
	{
		bool bVideoConfig = false;

		char const *szUsrField = "";
		if ( XBX_GetUserIsGuest( iSlot ) && !bVideoConfig )
		{
			IPlayerLocal *pPlayerPrimary = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetPrimaryUserId() );
			if ( pPlayerPrimary )
			{
				if ( TitleDataFieldsDescription_t const *pOffset = TitleDataFieldsDescriptionFindByString( fields, TITLE_DATA_PREFIX "CFG.usrSS.version" ) )
				{
					pPlayer = pPlayerPrimary;
					szUsrField = "SS";
				}
			}
		}

		bool bConfigVersionValid = true;

		CUtlVector< ConVar * > arrCVars;
		cv->WriteVariables( NULL, iSlot, !bVideoConfig, &arrCVars );
		if ( bConfigVersionValid )
		{
			for ( int k = 0; k < arrCVars.Count(); ++ k )
			{
				ConVar *cvSave = arrCVars[k];
				char const *cvSaveName = cvSave->GetBaseName();
				CFmtStr sFieldLookup( TITLE_DATA_PREFIX "CFG.%s%s.%s", bVideoConfig ? "sys" : "usr", szUsrField, cvSaveName );
				TitleDataFieldsDescription_t const *pField = TitleDataFieldsDescriptionFindByString( fields, sFieldLookup );
				if ( !pField )
				{
					Warning( "Host_ReadConfiguration_Console (%s#%d) - cannot read cvar %s\n", bVideoConfig ? "video" : "ctrlr", iController, cvSaveName );
					continue;
				}
				SyncCvarValueWithPlayerTitleData( pPlayer, cvSave, pField, CVARREADTD );

				if ( !Q_stricmp( cvSaveName, "joy_cfg_preset" ) ) // special code to handle joystick config presets
				{
					Cbuf_AddText( Cbuf_GetCurrentPlayer(), CFmtStr( "cmd%d exec joy_preset_%d" PLATFORM_EXT ".cfg\n", iSlot + 1, cvSave->GetInt() ) );
				}
			}
		}
	}

	// sys data
	if ( !XBX_GetUserIsGuest( iSlot ) && ( XBX_GetPrimaryUserId() == iController ) )
	{
		bool bVideoConfig = true;
		char const *szUsrField = "";

		bool bConfigVersionValid = true;

		CUtlVector< ConVar * > arrCVars;
		cv->WriteVariables( NULL, iSlot, !bVideoConfig, &arrCVars );
		if ( bConfigVersionValid )
		{
			for ( int k = 0; k < arrCVars.Count(); ++ k )
			{
				ConVar *cvSave = arrCVars[k];
				char const *cvSaveName = cvSave->GetBaseName();
				CFmtStr sFieldLookup( TITLE_DATA_PREFIX "CFG.%s%s.%s", bVideoConfig ? "sys" : "usr", szUsrField, cvSaveName );
				TitleDataFieldsDescription_t const *pField = TitleDataFieldsDescriptionFindByString( fields, sFieldLookup );
				if ( !pField )
				{
					Warning( "Host_ReadConfiguration_Console (%s#%d) - cannot read cvar %s\n", bVideoConfig ? "video" : "ctrlr", iController, cvSaveName );
					continue;
				}
				SyncCvarValueWithPlayerTitleData( pPlayer, cvSave, pField, CVARREADTD );
			}
		}
	}

	// Update notifications position
	Console_UpdateNotificationPosition();
#endif // _GAMECONSOLE
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : false - 
//-----------------------------------------------------------------------------
void Host_ReadConfiguration( const int iController, const bool readDefault )
{
	if ( sv.IsDedicated() )
		return;

	// Rebind keys and set cvars
	if ( !g_pFileSystem )
	{
		Sys_Error( "Host_ReadConfiguration:  g_pFileSystem == NULL\n" );
	}

	// Handle the console case
#ifdef _GAMECONSOLE
	{
		Host_ReadConfiguration_Console( iController );
		return;
	}
#else
	bool saveconfig = false;

	ISteamRemoteStorage *pRemoteStorage = Steam3Client().SteamClient() ? (ISteamRemoteStorage *)Steam3Client().SteamClient()->GetISteamGenericInterface(
		SteamAPI_GetHSteamUser(), SteamAPI_GetHSteamPipe(), STEAMREMOTESTORAGE_INTERFACE_VERSION ):NULL;

	if ( pRemoteStorage )
	{
		// if cloud settings is default but remote storage does not exist yet, set it to sync all because this is the first
		// computer the game is run on--default to copying everything to the cloud
		if ( cl_cloud_settings.GetInt() == -1 && !pRemoteStorage->FileExists( "cfg/config.cfg" ) )
		{
			DevMsg( "[Cloud]: Default setting with remote data non-existent, sync all\n" );
			cl_cloud_settings.SetValue( STEAMREMOTESTORAGE_CLOUD_ALL );
		}

		if ( cl_cloud_settings.GetInt() != -1 && ( cl_cloud_settings.GetInt() & STEAMREMOTESTORAGE_CLOUD_CONFIG ) )
		{
			// config files are run through the exec command which got pretty complicated with all the splitscreen
			// stuff. Steam UFS doens't support split screen well (2 users ?)
			GetFileFromRemoteStorage( pRemoteStorage, "cfg/config.cfg", "cfg/config.cfg", "USRLOCAL" );
		}
	}

	if ( g_pFileSystem->FileExists( "//usrlocal/cfg/config.cfg" ) )
	{
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "exec config.cfg usrlocal\n" );
	}
	else if ( g_pFileSystem->FileExists( "//mod/cfg/config.cfg" ) )
	{
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "exec config.cfg mod\n" );
	}
	else
	{
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "exec config_default.cfg\n" );
		saveconfig = true;
	}
	
	Cbuf_Execute();

	// check to see if we actually set any keys, if not, load defaults from kb_def.lst
	// so we at least have basics setup.
	int nNumBinds = Key_CountBindings();
	if ( nNumBinds == 0 )
	{
		UseDefaultBindings();
	}

	Key_SetBinding( KEY_ESCAPE, "cancelselect" );

	// Make sure that something is always bound to console
	if (NULL == Key_NameForBinding("toggleconsole"))
	{
		// If nothing is bound to it then bind it to '
		Key_SetBinding( KEY_BACKQUOTE, "toggleconsole" );
	}

	SetupDefaultAskConnectAcceptKey();

	Host_SetConfigCfgExecuted( iController );

	if ( saveconfig )
	{
		// An ugly hack, but we can probably save this safely
		bool saveinit = host_initialized;
		host_initialized = true;
		Host_WriteConfiguration( iController, "config.cfg" );
		host_initialized = saveinit;
	}
#endif
}

CON_COMMAND( host_writeconfig, "Store current settings to config.cfg (or specified .cfg file)." )
{
	if ( args.ArgC() > 2 )
	{
		ConMsg( "Usage:  writeconfig <filename.cfg>\n" );
		return;
	}

	if ( args.ArgC() == 2 )
	{
		char const *filename = args[ 1 ];
		if ( !filename || !filename[ 0 ] )
		{
			return;
		}

		char outfile[ MAX_QPATH ];
		// Strip path and extension from filename
		Q_FileBase( filename, outfile, sizeof( outfile ) );
		Host_WriteConfiguration( -1, va( "%s.cfg", outfile ) );
	}
	else
	{
		Host_WriteConfiguration( -1, "config.cfg" );
	}
}

CON_COMMAND( host_writeconfig_ss, "Store current settings to config.cfg (or specified .cfg file) with first param as splitscreen index." )
{
	if ( args.ArgC() <= 1 || args.ArgC() > 3)
	{
		ConMsg( "Usage:  writeconfig <controller index> <filename.cfg>\n" );
		return;
	}

	if ( args.ArgC() == 3 )
	{
		char const *filename = args[ 2 ];
		if ( !filename || !filename[ 0 ] )
		{
			return;
		}

		char outfile[ MAX_QPATH ];
		// Strip path and extension from filename
		Q_FileBase( filename, outfile, sizeof( outfile ) );

		int controller = atoi( args[1] );

		Host_WriteConfiguration( controller, va( "%s.cfg", outfile ) );
	}
	else if ( args.ArgC() == 2 )
	{
		int controller = atoi( args[1] );
		Host_WriteConfiguration( controller, "config.cfg" );
	}
	else
	{
		Host_WriteConfiguration( -1, "config.cfg" );
	}
}

CON_COMMAND( host_reset_config, "reset config (for testing) with param as splitscreen index." )
{
	if ( args.ArgC() <= 1 || args.ArgC() > 2)
	{
		ConMsg( "Usage:  host_reset_config <controller index>\n" );
		return;
	}

	else if ( args.ArgC() == 2 )
	{
		int controller = atoi( args[1] );
		Host_ResetConfiguration( controller );
	}

}

#ifdef _GAMECONSOLE
CON_COMMAND( host_writeconfig_video_ss, "Store current video settings to config.cfg (or specified .cfg file) with first param as controller index." )
{
	if ( args.ArgC() != 2 )
	{
		ConMsg( "Usage:  writeconfig <controller index>\n" );
		return;
	}

	int controller = atoi( args[1] );
	Host_WriteConfiguration_Console( controller, true );
}
#endif

#endif

//-----------------------------------------------------------------------------
// Purpose: Does a quick parse of the config.cfg to read cvars that
//			need to be read before any games systems are initialized
//			assumes only cvars and filesystem are initialized
//-----------------------------------------------------------------------------
void Host_ReadPreStartupConfiguration()
{
	FileHandle_t f = NULL;
	if ( IsGameConsole() )
	{
		// 360 config is less restrictive and can be anywhere in the game path
		f = g_pFileSystem->Open( "//game/cfg/config" PLATFORM_EXT ".cfg", "rt" );
	}
	else
	{
		f = g_pFileSystem->Open( "//usrlocal/cfg/config.cfg", "rt" );
		if ( !f )
			f = g_pFileSystem->Open( "//mod/cfg/config.cfg", "rt" );
	}

	if ( !f )
		return;

	// read file into memory
	int size = g_pFileSystem->Size(f);
	char *configBuffer = new char[ size + 1 ];
	g_pFileSystem->Read( configBuffer, size, f );
	configBuffer[size] = 0;
	g_pFileSystem->Close( f );

	// parse out file
	static const char *s_PreStartupConfigConVars[] =
	{
		"sv_unlockedchapters",		// needed to display the startup graphic while loading
		"snd_legacy_surround",		// needed to init the sound system
		"gameui_xbox",				// needed to initialize the correct UI
		"save_in_memory",			// needed to preread data from the correct location in UI
#if defined( USE_SDL )
		"sdl_displayindex"			// needed to set multimonitor displayindex for SDL
#endif
	};

	// loop through looking for all the cvars to apply
	for (int i = 0; i < ARRAYSIZE(s_PreStartupConfigConVars); i++)
	{
		const char *search = Q_stristr(configBuffer, s_PreStartupConfigConVars[i]);
		if (search)
		{
			// read over the token
			search = COM_Parse(search);

			// read the value
			COM_Parse(search);

			// apply the value
			ConVar *var = (ConVar *)g_pCVar->FindVar( s_PreStartupConfigConVars[i] );
			if ( var )
			{
				var->SetValue( com_token );
			}
		}
	}

	// free
	delete [] configBuffer;
}

void Host_RecomputeSpeed_f( void )
{
	ConMsg( "Recomputing clock speed...\n" );

	CClockSpeedInit::Init();
	ConMsg( "Clock speed: %.0f Mhz\n", CFastTimer::GetClockSpeed() / 1000000.0 );
}

static ConCommand recompute_speed( "recompute_speed", Host_RecomputeSpeed_f, "Recomputes clock speed (for debugging purposes).", FCVAR_CHEAT );

void DTI_Flush_f()
{
	DTI_Flush();
	ServerDTI_Flush();
	FlushDeltaBitsTrackingData();
}

static ConCommand dti_flush( "dti_flush", DTI_Flush_f, "Write out the datatable instrumentation files (you must run with -dti for this to work)." );

/*
==================
Host_ShutdownServer

This only happens at the end of a game, not between levels
==================
*/
void Host_ShutdownServer( void )
{
	if ( !sv.IsActive() )
		return;

	if ( IGameEvent *event = g_GameEventManager.CreateEvent( "server_pre_shutdown" ) )
	{
		event->SetString( "reason", "restart" );
		g_GameEventManager.FireEvent( event );
	}

	// clear structures
#if !defined( DEDICATED )
	g_pShadowMgr->LevelShutdown();
#endif
	StaticPropMgr()->LevelShutdown();

	Host_FreeStateAndWorld( true );
	sv.Shutdown();// sv.Shutdown() references some heap memory, so run it before Host_FreeToLowMark()
	Host_FreeToLowMark( true ); 

#if defined( REPLAY_ENABLED )
	if ( g_pServerReplayHistoryManager )
	{
		g_pServerReplayHistoryManager->Shutdown();
		g_pServerReplayHistoryManager = NULL;
	}
#endif

	if ( IGameEvent *event = g_GameEventManager.CreateEvent( "server_shutdown" ) )
	{
		event->SetString( "reason", "restart" );
		g_GameEventManager.FireEvent( event );
	}

	g_Log.Close();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : time - 
// Output : bool
//-----------------------------------------------------------------------------
void Host_AccumulateTime( float dt )
{
	// Accumulate some time
	realtime += dt;
	bool bUseNormalTickTime = true;
#if !defined(DEDICATED)
	if ( demoplayer->IsPlayingTimeDemo() )
		bUseNormalTickTime = false;
#endif
	if ( g_bDedicatedServerBenchmarkMode )
		bUseNormalTickTime = false;

	if ( bUseNormalTickTime )
	{
		host_frametime	= dt;
	}
	else
	{
		// Used to help increase reproducibility of timedemos
		host_frametime	= host_state.interval_per_tick;
	}

	extern bool g_bReplayLoadedTools;

	float flHostTimescale = 1.0f;
	float flGameTimescale = 1.0f;

	if ( host_timescale.GetFloat() > 0.0f 
#if !defined(DEDICATED)
		&& CanCheat() 
#endif
		)
	{
		// We are allowed to modify this convar!
		flHostTimescale = host_timescale.GetFloat();
	}

	if ( flGameTimescale > 0.0f )
	{
		flGameTimescale = sv.GetTimescale();
	}

	float fullscale = flHostTimescale * flGameTimescale;

#if !defined(DEDICATED)
	if ( demoplayer->IsPlayingBack() )
	{
		// adjust time scale if playing back demo
		fullscale *= demoplayer->GetPlaybackTimeScale();
	}
#endif

#if 1
	if ( host_framerate.GetFloat() != 0 
#if !defined(DEDICATED)
		&& ( CanCheat() || demoplayer->IsPlayingBack() ) 
#endif
		)
	{	
		float fps = host_framerate.GetFloat();
		if ( fps > 1 )
		{
			fps = 1.0f/fps;
		}
		else if ( fps < -1 )
		{
			fps = 1.0f/fabsf(fps);
			if ( fps > dt )
			{
				fps = dt;
			}
		}
		host_frametime = fps;
		host_frametime_unbounded = host_frametime;
		host_frametime_unscaled = host_frametime;
	}
	else if ( fullscale != 1.0f )
	{
		host_frametime_unscaled = host_frametime;
		host_frametime *= fullscale;

		host_frametime_unbounded = host_frametime;

#ifndef NO_TOOLFRAMEWORK
		if ( CommandLine()->CheckParm( "-tools" ) == NULL && !g_bReplayLoadedTools )
		{
#endif // !NO_TOOLFRAMEWORK
			host_frametime = MIN( host_frametime, MAX_FRAMETIME * fullscale);
			host_frametime_unscaled = MIN( host_frametime_unscaled, MAX_FRAMETIME );
			host_frametime_unscaled = MAX( host_frametime_unscaled, MIN_FRAMETIME );
#ifndef NO_TOOLFRAMEWORK
		}
		else
		{
			host_frametime = MIN( host_frametime, MAX_TOOLS_FRAMETIME * fullscale);
			host_frametime_unscaled = MIN( host_frametime_unscaled, MAX_TOOLS_FRAMETIME );
			host_frametime_unscaled = MAX( host_frametime_unscaled, MIN_TOOLS_FRAMETIME );
		}
#endif // !NO_TOOLFRAMEWORK
	}
	else
#ifndef NO_TOOLFRAMEWORK
		if ( CommandLine()->CheckParm( "-tools" ) != NULL && !g_bReplayLoadedTools )
		{
			host_frametime_unbounded = host_frametime;
			host_frametime = MIN( host_frametime, MAX_TOOLS_FRAMETIME );
			host_frametime = MAX( host_frametime, MIN_TOOLS_FRAMETIME );
			host_frametime_unscaled = host_frametime;
		}
		else
#endif // !NO_TOOLFRAMEWORK
	{	// don't allow really long or short frames
		host_frametime_unbounded = host_frametime;
		host_frametime = MIN( host_frametime, MAX_FRAMETIME );
		host_frametime = MAX( host_frametime, MIN_FRAMETIME );
		host_frametime_unscaled = host_frametime;
	}
#endif // 1

	// Adjust the client clock very slightly to keep it in line with the server clock.
#ifndef DEDICATED
	float adj = GetBaseLocalClient().GetClockDriftMgr().AdjustFrameTime( host_frametime ) - host_frametime;
	host_frametime += adj;
	host_frametime_unbounded += adj;
	host_frametime_unscaled += adj;
#endif
	if ( g_pSoundServices )									// not present on linux server
		g_pSoundServices->SetSoundFrametime(dt, host_frametime);

}

#define FPS_AVG_FRAC 0.9f

float g_fFramesPerSecond = 0.0f;

// temporarily a constant until I bother to hook it to a cvar
inline static bool cl_ps3ledframerate()  // should i make the front LEDs show the framerate (divided by two)
{
#ifdef _PS3
	return (CPS3FrontPanelLED::GetSwitches() & CPS3FrontPanelLED::kPS3SWITCH3) == 0;
#else
	return false;
#endif
}

/*
==================
Host_PostFrameRate
==================
*/
void Host_PostFrameRate( float frameTime )
{
	extern int r_framecount;
	frameTime = clamp( frameTime, 0.0001f, 1.0f );

	float fps = 1.0f / frameTime;
	g_fFramesPerSecond = g_fFramesPerSecond * FPS_AVG_FRAC + ( 1.0f - FPS_AVG_FRAC ) * fps;
	if ( IsPS3() && !IsCert() )
	{
		if ( cl_ps3ledframerate() )
		{
			uint64 switches = CPS3FrontPanelLED::GetSwitches();
			int framerate = RoundFloatToInt( g_fFramesPerSecond * 0.5f );
			// two modes -- with DIP 0 set to zero,
			if (( switches & CPS3FrontPanelLED::kPS3SWITCH0 ))
			{
				// make the front LEDs display the framerate divided by two in binary code
				// (ie to scale it so that 0..31 fits into 0..15)
				framerate = framerate > 31 ? 31 : framerate ;
				CPS3FrontPanelLED::SetLEDs( framerate >> 1 );
			}
			else  // without DIP0 set on, do a snazzy KNIGHT RIDER effect
			{
				static CPS3FrontPanelLED::eLEDIndex_t kitt[6] = { CPS3FrontPanelLED::kPS3LED0, CPS3FrontPanelLED::kPS3LED1, CPS3FrontPanelLED::kPS3LED2, // CPS3FrontPanelLED::kPS3LED3,
																  CPS3FrontPanelLED::kPS3LED3, CPS3FrontPanelLED::kPS3LED2, CPS3FrontPanelLED::kPS3LED1  // , CPS3FrontPanelLED::kPS3LED0 
				};
				CPS3FrontPanelLED::SetLEDs( kitt[ ( host_framecount >> 4 ) % 6] ); 
			}
		}
		else
		{
			CPS3FrontPanelLED::SetLEDs( 0 );
		}
	}
}

/*
==================
Host_GetHostInfo
==================
*/
void Host_GetHostInfo(float *fps, int *nActive, int *nMaxPlayers, char *pszMap, int maxlen )
{
	// Count clients, report 
	int clients = sv.GetNumClients();

	*fps = g_fFramesPerSecond;
	*nActive = clients;

	if (pszMap)
	{
		if (sv.m_szMapname && sv.m_szMapname[0])
			Q_strncpy(pszMap, sv.m_szMapname, maxlen );
		else
			pszMap[0] = '\0';
	}

	*nMaxPlayers = sv.GetMaxClients();
}

static bool AppearsNumeric( char const *in )
{
	char const *p = in;
	int special[ 3 ];
	Q_memset( special, 0, sizeof( special ) );

	for ( ; *p; p++ )
	{
		if ( *p == '-' )
		{
			special[0]++;
			continue;
		}

		if ( *p == '+' )
		{
			special[1]++;
			continue;
		}

		if ( *p >= '0' && *p <= '9' )
		{
			continue;
		}

		if ( *p == '.' )
		{
			special[2]++;
			continue;
		}

		return false;
	}

	// Can't have multiple +, -, or decimals
	for ( int i = 0; i < 3; i++ )
	{
		if ( special[ i ] > 1 )
			return false;
	}

	// can't be + and - at same time
	if ( special[ 0 ] && special[ 1 ] )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: If the value is numeric, remove unnecessary trailing zeros
// Input  : *invalue - 
// Output : char const
//-----------------------------------------------------------------------------
char const * Host_CleanupConVarStringValue( char const *invalue )
{
	static char clean[ 256 ];

	Q_snprintf( clean, sizeof( clean ), "%s", invalue );

	// Don't mess with empty string
	// Otherwise, if it appears numeric and has a decimal, try to strip all zeroes after decimal
	if ( Q_strlen( clean ) >= 1 && AppearsNumeric( clean ) && Q_strstr( clean, "." ) )
	{
		char *end = clean + strlen( clean ) - 1;
		while ( *end && end >= clean )
		{
			// Removing trailing zeros
			if ( *end != '0' )
			{
				// Remove decimal, zoo
				if ( *end == '.' )
				{
					if ( end == clean )
					{
						*end = '0';
					}
					else
					{
						*end = 0;
					}
				}
				break;
			}

			*end-- = 0;
		}
	}

	return clean;
}

int Host_CountVariablesWithFlags( int flags, bool nonDefault )
{
	int c = 0;
	ICvar::Iterator iter( g_pCVar );
	for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
	{
		ConCommandBase *var = iter.Get();
		if ( var->IsCommand() )
			continue;

		const ConVar *pthiscvar = ( const ConVar * )var;

		if ( !pthiscvar->IsFlagSet( flags ) )
			continue;

		// It's == to the default value, don't count
		if ( nonDefault && !Q_strcasecmp( pthiscvar->GetDefault(), pthiscvar->GetString() ) )
			continue;

		++c;
	}

	return c;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg - 
//-----------------------------------------------------------------------------
void Host_BuildUserInfoUpdateMessage( int nSplitScreenSlot, CMsg_CVars *rCvarList, bool nonDefault )
{
	// Slot 0 does the easy version, all userinfo, except _ADDED ones
	int nRequiredFlags = FCVAR_USERINFO;
	int nDisallowedFlags = FCVAR_SS_ADDED;

	if ( nSplitScreenSlot != 0 )
	{
		// Slot one only looks at added ones
		nDisallowedFlags = FCVAR_SS;
		nRequiredFlags = FCVAR_USERINFO | FCVAR_SS_ADDED;
	}

	int count = Host_CountVariablesWithFlags( nRequiredFlags, nonDefault );
	// Nothing to send
	if ( count <= 0 )
		return;

#if !defined( _GAMECONSOLE ) && !defined( DEDICATED )
	// Add local user SteamID
	if ( Steam3Client().SteamUser() && Steam3Client().SteamUser()->GetSteamID().GetAccountID() )
	{
		++ count;
		NetMsgSetCVarUsingDictionary( rCvarList->add_cvars(), "accountid",
			CFmtStr( "%u", Steam3Client().SteamUser()->GetSteamID().GetAccountID() ) );
	}
#endif

	ICvar::Iterator iter( g_pCVar );
	for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
	{
		ConCommandBase *var = iter.Get();
		if ( var->IsCommand() )
			continue;

		const ConVar *pthiscvar = ( const ConVar * )var;

		if ( !pthiscvar->IsFlagSet( nRequiredFlags ) || pthiscvar->IsFlagSet( nDisallowedFlags ) )
			continue;

		// It's == to the default value, don't count
		if ( nonDefault && !Q_strcasecmp( pthiscvar->GetDefault(), pthiscvar->GetString() ) )
			continue;

		if ( pthiscvar->GetSplitScreenPlayerSlot() != nSplitScreenSlot )
			continue;

		NetMsgSetCVarUsingDictionary( rCvarList->add_cvars(), pthiscvar->GetBaseName(),
			Host_CleanupConVarStringValue( pthiscvar->GetString() ) );
	}

	// Too many to send, error out and have mod author get a clue.
	if ( rCvarList->cvars_size() > 255 )
	{
		Sys_Error( "Engine only supports 255 ConVars marked %i\n", FCVAR_USERINFO );
		return;
	}

	// Make sure this count matches original one
	Assert( rCvarList->cvars_size() <= count );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg - 
//-----------------------------------------------------------------------------
void Host_BuildConVarUpdateMessage( CMsg_CVars *rCvarList, int flags, bool nonDefault )
{
	int count = Host_CountVariablesWithFlags( flags, nonDefault );

	// Nothing to send
	if ( count <= 0 )
		return;

	// Too many to send, error out and have mod author get a clue.
	if ( count > 255 )
	{
		Sys_Error( "Engine only supports 255 ConVars marked %i\n", flags );
		return;
	}

	ICvar::Iterator iter( g_pCVar );
	for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
	{
		ConCommandBase *var = iter.Get();
		if ( var->IsCommand() )
			continue;

		const ConVar *pthiscvar = ( const ConVar * )var;

		if ( !pthiscvar->IsFlagSet( flags ) )
			continue;

		// It's == to the default value, don't count
		if ( nonDefault && !Q_strcasecmp( pthiscvar->GetDefault(), pthiscvar->GetString() ) )
			continue;

		NetMsgSetCVarUsingDictionary( rCvarList->add_cvars(), pthiscvar->GetName(),
			Host_CleanupConVarStringValue( pthiscvar->GetString() ) );
	}
}

#if  !defined( DEDICATED )
// FIXME: move me somewhere more appropriate
extern IVEngineClient *engineClient;
void CL_SendVoicePacket(bool bFinal)
{
#if !defined( NO_VOICE )
	if ( !Voice_IsRecording() )
		return;

	// Get whatever compressed data there is and and send it.
	char uchVoiceData[8192];
	VoiceFormat_t format;
	uint8 nSectionNumber;
	uint32 nSectionSequenceNumber;
	uint32 nUncompressedSampleOffset;
	int nLength = Voice_GetCompressedData( uchVoiceData, sizeof( uchVoiceData ), bFinal, &format, &nSectionNumber, &nSectionSequenceNumber, &nUncompressedSampleOffset  );
	if( nLength && GetBaseLocalClient().IsActive() )
	{
		CCLCMsg_VoiceData_t voiceMsg;

		voiceMsg.set_data( uchVoiceData, nLength );
		if ( format == VoiceFormat_Steam )
		{
			voiceMsg.set_format( VOICEDATA_FORMAT_STEAM );
		}
		else
		{
			voiceMsg.set_format( VOICEDATA_FORMAT_ENGINE );
		}

		player_info_t playerInfo;
		if ( engineClient->GetPlayerInfo( engineClient->GetLocalPlayer(), &playerInfo ) )
		{
			voiceMsg.set_xuid( playerInfo.xuid );
		}

		voiceMsg.set_section_number( nSectionNumber );
		voiceMsg.set_sequence_bytes( nSectionSequenceNumber );
		voiceMsg.set_uncompressed_sample_offset( nUncompressedSampleOffset );

		GetBaseLocalClient().m_NetChannel->SendNetMsg( voiceMsg );
	}
#endif
}

#if defined ( _GAMECONSOLE )
void CL_ProcessGameConsoleVoiceData()
{
	if ( g_pMatchFramework && !(g_pMatchFramework->GetMatchTitle()->GetTitleSettingsFlags() & MATCHTITLE_VOICE_INGAME) )
		return; // Title plays voice via lobby system

#if defined( _PS3 ) && defined( CLIENT_DLL )
	bool restrictedFromChat = engine->PS3_IsUserRestrictedFromChat( );
	if ( restrictedFromChat )
		return;
#endif

	if ( Audio_GetXVoice() == NULL )
		return;

	for ( int k = 0; k < XBX_GetNumGameUsers(); ++ k )
	{
		int iCtrlr = XBX_GetUserId( k );
		
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( k );

		if ( Audio_GetXVoice()->VoiceUpdateData( iCtrlr ) )
		{
			if ( GetLocalClient( k ).IsActive() )
			{
				Audio_GetXVoice()->VoiceSendData( iCtrlr, GetLocalClient( k ).m_NetChannel );
			}
		}
	}
}

#endif

void CL_ProcessVoiceData()
{
	VPROF_BUDGET( "CL_ProcessVoiceData", VPROF_BUDGETGROUP_OTHER_NETWORKING );

#if !defined( NO_VOICE )
	if ( Voice_Idle( host_state.interval_per_tick ) )
	{
		CL_SendVoicePacket( false );
	}
#endif

#if defined ( _GAMECONSOLE )
	CL_ProcessGameConsoleVoiceData();
#endif
}
#endif

/*
=====================
Host_UpdateScreen

Refresh the screen
=====================
*/
void Host_UpdateScreen( void )
{
#ifndef DEDICATED 
	if( r_ForceRestore.GetInt() )
	{
		ForceMatSysRestore();
		r_ForceRestore.SetValue(0);
	}

	// Refresh the screen
	SCR_UpdateScreen ();
#endif
}

/*
====================
Host_UpdateSounds

Update sound subsystem and cd audio
====================
*/
void Host_UpdateSounds( void )
{
	SNPROF("Host_UpdateSounds");

#if !defined( DEDICATED )
	MDLCACHE_COARSE_LOCK_(g_pMDLCache);
	// update audio
	if ( GetBaseLocalClient().IsActive() )
	{
		S_Update( &s_AudioState );	
	}
	else
	{
		S_Update( NULL );
	}
#endif
}

/*
==============================
Host_Speeds

==============================
*/
void CFrameTimer::ResetDeltas()
{
	for ( int i = 0; i < NUM_FRAME_SEGMENTS; i++ )
	{
		deltas[ i ] = 0.0f;
	}
}

void CFrameTimer::MarkFrame()
{
	double frameTime;
	double fps;

	// ConDMsg("%f %f %f\n", time1, time2, time3 );

	float fs_input = (deltas[FRAME_SEGMENT_INPUT])*1000.0;
	float fs_client = (deltas[FRAME_SEGMENT_CLIENT])*1000.0;
	float fs_server = (deltas[FRAME_SEGMENT_SERVER])*1000.0;
	float fs_render = (deltas[FRAME_SEGMENT_RENDER])*1000.0;
	float fs_sound = (deltas[FRAME_SEGMENT_SOUND])*1000.0;
	float fs_cldll = (deltas[FRAME_SEGMENT_CLDLL])*1000.0;
	float fs_exec = (deltas[FRAME_SEGMENT_CMD_EXECUTE])*1000.0;

	m_flLastServerTime = fs_server;

	ResetDeltas();

	frameTime = host_frametime;
	//frameTime /= 1000.0;
	if ( frameTime < 0.0001 )
	{
		fps = 999.0;
	}
	else
	{
		fps = 1.0 / frameTime;
	}

	if (host_speeds.GetInt())
	{
		int ent_count = 0;
		int i;
		static int last_host_tickcount;

		int ticks = host_tickcount - last_host_tickcount;
		last_host_tickcount = host_tickcount;

		// count used entities
		for (i=0 ; i<sv.num_edicts ; i++)
		{
			if (!sv.edicts[i].IsFree())
				ent_count++;
		}

		char sz[ 256 ];
		Q_snprintf( sz, sizeof( sz ),
			"%3i fps -- inp(%3.1f) sv(%3.1f) cl(%3.1f) render(%3.1f) snd(%3.1f) cl_dll(%3.1f) exec(%3.1f) ents(%d) ticks(%d)",
			(int)fps, 
			fs_input, 
			fs_server, 
			fs_client, 
			fs_render, 
			fs_sound, 
			fs_cldll, 
			fs_exec, 
			ent_count, 
			ticks );

#ifndef DEDICATED
		if ( host_speeds.GetInt() >= 2 )
		{
			Con_NPrintf ( 0, sz );
		}
		else
		{
			ConDMsg ( "%s\n", sz );
		}
#endif
	}

}

#define FRAME_TIME_FILTER_TIME 0.5f

void CFrameTimer::ComputeFrameVariability()
{
	m_pFrameTimeHistory[m_nFrameTimeHistoryIndex] = frametime;
	m_pFrameStartTimeHistory[m_nFrameTimeHistoryIndex] = framestarttimeduration;
	if ( ++m_nFrameTimeHistoryIndex >= FRAME_HISTORY_COUNT )
	{
		m_nFrameTimeHistoryIndex = 0;
	}

	// Compute a low-pass filter of the frame time over the last half-second
	// Count the number of samples that live within the last half-second
	int i = m_nFrameTimeHistoryIndex;
	int nMaxSamples = 0;
	float flTotalTime = 0.0f;
	while( (nMaxSamples < FRAME_HISTORY_COUNT) && (flTotalTime <= FRAME_TIME_FILTER_TIME) )
	{
		if ( --i < 0 )
		{
			i = FRAME_HISTORY_COUNT - 1;
		}		
		if ( m_pFrameTimeHistory[i] == 0.0f )
			break;

		flTotalTime += m_pFrameTimeHistory[i];
		++nMaxSamples;
	}

	if ( nMaxSamples == 0 )
	{
		m_flFPSVariability = 0.0f;
		m_flFPSStdDeviationSeconds = 0.0f;
		m_flFPSStdDeviationFrameStartTimeSeconds = 0.0f;
		return;
	}

	float flExponent = -2.0f / (int)nMaxSamples;

	i = m_nFrameTimeHistoryIndex;
	float flAverageTime = 0.0f;
	float flExpCurveArea = 0.0f;
	int n = 0;
	while( n < nMaxSamples )
	{
		if ( --i < 0 )
		{
			i = FRAME_HISTORY_COUNT - 1;
		}
		flExpCurveArea += exp( flExponent * n );
		flAverageTime += m_pFrameTimeHistory[i] * exp( flExponent * n );
		++n;
	}

	flAverageTime /= flExpCurveArea;

	float flAveFPS = 0.0f;
	if ( flAverageTime != 0.0f )
	{
		flAveFPS = 1.0f / flAverageTime;
	}

	float flCurrentFPS = 0.0f;
	if ( frametime != 0.0f )
	{
		flCurrentFPS = 1.0f / frametime;
	}

	// Now subtract out the current fps to get variability in FPS
	m_flFPSVariability = fabs( flCurrentFPS - flAveFPS );

	// Now compute variance/stddeviation
	double sumFrameTime = 0.0f, sumFrameStartTime = 0.0f;
	int count = 0;
	for ( int ii1 = 0; ii1 < FRAME_HISTORY_COUNT; ++ii1 )
	{
		if ( m_pFrameTimeHistory[ ii1 ] == 0.0f )
			continue;

		double ft = MIN( m_pFrameTimeHistory[ ii1 ], 0.25 );
		sumFrameTime += ft;

		ft = MIN( m_pFrameStartTimeHistory[ ii1 ], 0.25 );
		sumFrameStartTime += ft;

		++count;
	}

	if ( count <= 1 )
	{
		return;
	}

	double avgFrameTime = sumFrameTime / (double)count;
	double devSquaredFrameTime = 0.0f;
	double avgFrameStartTime = sumFrameStartTime / ( double ) count;
	double devSquaredFrameStartTime = 0.0f;
	for ( int ii2 = 0; ii2 < FRAME_HISTORY_COUNT; ++ii2 )
	{
		if ( m_pFrameTimeHistory[ ii2 ] == 0.0f )
			continue;

		double ft = MIN( m_pFrameTimeHistory[ ii2 ], 0.25 );
		double dt = ft - avgFrameTime;

		devSquaredFrameTime += ( dt * dt );

		ft = MIN( m_pFrameStartTimeHistory[ ii2 ], 0.25 );
		dt = ft - avgFrameStartTime;
		devSquaredFrameStartTime += ( dt * dt );
	}

	double variance = devSquaredFrameTime / (double)( count );
	m_flFPSStdDeviationSeconds = sqrt( variance );

	variance = devSquaredFrameStartTime / ( double ) ( count );
	m_flFPSStdDeviationFrameStartTimeSeconds = sqrt( variance );

	tmPlot( TELEMETRY_LEVEL0, TMPT_NONE, 0, m_flFPSStdDeviationSeconds * 1000.0f, "m_flFPSStdDeviationSeconds(ms)" );
	tmPlot( TELEMETRY_LEVEL0, TMPT_NONE, 0, m_flFPSStdDeviationFrameStartTimeSeconds * 1000.0f, "m_flFPSStdDeviationFrameStartTimeMS(ms)" );

//	printf("var: %.2f avg:%.6f frametime:%f\n", m_flFPSStdDeviationSeconds * 1000.0f, avg, frametime);
}

void Host_Speeds()
{
	g_HostTimes.MarkFrame();
#if !defined(DEDICATED)
	g_pClientDemoPlayer->MarkFrame( g_HostTimes.m_flFPSVariability );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: When singlestep == 1, then you must set next == 1 to run to the 
//  next frame.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool Host_ShouldRun( void )
{
	static int current_tick = -1;

	// See if we are single stepping
	if ( !singlestep.GetInt() )
	{
		return true;
	}

	// Did user set "next" to 1?
	if ( next.GetInt() )
	{
		// Did we finally finish this frame ( Host_ShouldRun is called in 3 spots to pause
		//  three different things ).
		if ( current_tick != (host_tickcount-1) )
		{
			// Okay, time to reset to halt execution again
			next.SetValue( 0 );
			return false;
		}

		// Otherwise, keep running this one frame since we are still finishing this frame
		return true;
	}
	else
	{
		// Remember last frame without "next" being reset ( time is locked )
		current_tick = host_tickcount;
		// Time is locked
		return false;
	}
}

static ConVar mem_periodicdumps( "mem_periodicdumps", "0", 0, "Write periodic memstats dumps every n seconds." );
static double g_flLastPeriodicMemDump = -1.0f;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static float g_TimeLastMemTest;
void Host_CheckDumpMemoryStats( void )
{
	if ( mem_test_each_frame.GetBool() )
	{
		if ( !MemTest() )
		{
			DebuggerBreakIfDebugging();
			Error( "Heap is corrupt\n" );
		}
	}
	else if ( mem_test_every_n_seconds.GetInt() > 0 )
	{
		float now = Plat_FloatTime();
		if ( now - g_TimeLastMemTest > mem_test_every_n_seconds.GetInt() )
		{
			g_TimeLastMemTest = now;
			if ( !MemTest() )
			{
				DebuggerBreakIfDebugging();
				Error( "Heap is corrupt\n" );
			}
		}
	}

	if ( mem_periodicdumps.GetFloat() > 0.0f )
	{
		double curtime = Plat_FloatTime();
		if ( curtime - g_flLastPeriodicMemDump > mem_periodicdumps.GetFloat() )
		{
			const char *mapname = GetMapName();
			Host_PrintMemoryStatus( mapname );
			g_pMemAlloc->DumpStatsFileBase( mapname );
			g_flLastPeriodicMemDump = curtime;
		}
	}


#if defined(_WIN32)
	if ( mem_dumpstats.GetInt() <= 0 )
		return;

	if ( mem_dumpstats.GetInt() == 1 )
		mem_dumpstats.SetValue( 0 ); // reset cvar, dump stats only once

	_CrtMemState state;
	Q_memset( &state, 0, sizeof( state ) );
	_CrtMemCheckpoint( &state );

	unsigned int size = 0;

	for ( int use = 0; use < _MAX_BLOCKS; use++)
	{
		size += state.lSizes[ use ];
	}
	Msg("MEMORY:  Run-time Heap\n------------------------------------\n");

	Msg( "\tHigh water %s\n", Q_pretifymem( state.lHighWaterCount,4 ) );
	Msg( "\tCurrent mem %s\n", Q_pretifymem( size,4 ) );
	Msg("------------------------------------\n");
	int hunk = Hunk_MallocSize();
	Msg("\tAllocated outside hunk:  %s\n", Q_pretifymem( size - hunk ) );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void _Host_SetGlobalTime()
{
	// Server
	g_ServerGlobalVariables.realtime			= realtime;
	g_ServerGlobalVariables.framecount			= host_framecount;
	g_ServerGlobalVariables.absoluteframetime	= host_frametime;
	g_ServerGlobalVariables.absoluteframestarttimestddev = host_framestarttime_stddeviation;
	g_ServerGlobalVariables.interval_per_tick	= host_state.interval_per_tick;
	g_ServerGlobalVariables.serverCount			= Host_GetServerCount();

#ifndef DEDICATED
	// Client
	g_ClientGlobalVariables.realtime			= realtime;
	g_ClientGlobalVariables.framecount			= host_framecount;
	g_ClientGlobalVariables.absoluteframetime	= host_frametime;
	g_ClientGlobalVariables.absoluteframestarttimestddev = host_framestarttime_stddeviation;
	g_ClientGlobalVariables.interval_per_tick	= host_state.interval_per_tick;
#endif
}

/*
==================
_Host_RunFrame

Runs all active servers
==================
*/

void _Host_RunFrame_Input( float accumulated_extra_samples, bool bFinalTick )
{
	VPROF_BUDGET( "_Host_RunFrame_Input", _T("Input") );

	// Run a test script?
	static bool bFirstFrame = true;
	if ( bFirstFrame )
	{
		bFirstFrame = false;
		const char *pScriptFilename = CommandLine()->ParmValue( "-testscript" );
		if ( pScriptFilename && pScriptFilename[0] )
		{
			GetTestScriptMgr()->StartTestScript( pScriptFilename );
		}

		// init the net console if we haven't yet
		InitNetConsole();
		NET_InitPostFork();
	}

	g_HostTimes.StartFrameSegment( FRAME_SEGMENT_INPUT );

#ifndef DEDICATED
	// Client can process input
	ClientDLL_ProcessInput( );

	g_HostTimes.StartFrameSegment( FRAME_SEGMENT_CMD_EXECUTE );

	// process console commands
	Cbuf_Execute ();

	g_HostTimes.EndFrameSegment( FRAME_SEGMENT_CMD_EXECUTE );

	// Send any current movement commands to server and flush reliable buffer even if not moving yet.
	CL_Move( accumulated_extra_samples, bFinalTick );

#endif

	g_HostTimes.EndFrameSegment( FRAME_SEGMENT_INPUT );
}

void _Host_ProcessVoice_Server( void )
{
	VPROF_BUDGET( "_Host_ProcessVoice_Server", VPROF_BUDGETGROUP_GAME );

	SV_ProcessVoice();
}

void _Host_RunFrame_Server( bool finaltick )
{
	VPROF_BUDGET( "_Host_RunFrame_Server", VPROF_BUDGETGROUP_GAME );
	VPROF_INCREMENT_COUNTER( "ticks", 1 );

	VPROF_TEST_SPIKE( vprof_server_spike_threshold.GetFloat() );

	// Run the Server frame ( read, run physics, respond )
	g_HostTimes.StartFrameSegment( FRAME_SEGMENT_SERVER );

	SV_Frame( finaltick );

	g_HostTimes.EndFrameSegment( FRAME_SEGMENT_SERVER );

	// Look for connectionless rcon packets on dedicated servers
	// SV_CheckRcom(); TODO 
}

void _Host_RunFrame_Server_Async( int numticks )
{
#ifdef VPROF_ENABLED
	if ( vprof_server_thread.GetBool() )
	{
		if ( g_VProfTargetThread != ThreadGetCurrentId() )
		{
			g_VProfTargetThread = ThreadGetCurrentId();
		}
	}
	else
	{
		if ( g_VProfTargetThread == ThreadGetCurrentId() )
		{
			g_VProfTargetThread = g_MainThreadId;
		}
	}
#endif

	MDLCACHE_COARSE_LOCK_(g_pMDLCache);
	for ( int tick = 0; tick < numticks; tick++ )
	{ 
		g_ServerGlobalVariables.tickcount = sv.m_nTickCount;
		g_ServerGlobalVariables.simTicksThisFrame = numticks - tick;
		bool bFinalTick = ( tick == (numticks - 1) );
		_Host_RunFrame_Server( bFinalTick );
	}
}


void _Host_RunFrame_Client( bool framefinished )
{
#ifndef DEDICATED
	VPROF( "_Host_RunFrame_Client" );

	g_HostTimes.StartFrameSegment( FRAME_SEGMENT_CLIENT );

	// Get any current state update from server, etc.
	CL_ReadPackets( framefinished );

	GetBaseLocalClient().CheckUpdatingSteamResources();
	GetBaseLocalClient().CheckFileCRCsWithServer();

	// Resend connection request if needed.
	GetBaseLocalClient().RunFrame();

	if ( CL_IsHL2Demo() || CL_IsPortalDemo() ) // don't need sv.IsDedicated() because ded servers don't run this
	{
		void CL_DemoCheckGameUIRevealTime();
		CL_DemoCheckGameUIRevealTime();
	}

	Steam3Client().RunFrame();

#if defined( _DEBUG )
	// Debug!!! FireGameEvent
	g_GameEventManager.VerifyListenerList();
#endif

	g_HostTimes.EndFrameSegment( FRAME_SEGMENT_CLIENT );

	// This takes 1 usec, so it's pretty cheap...
	CL_SetPagedPoolInfo();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Used to set limits on certain convars in multiplayer/sv_cheats mode.
// Returns true if it was called recursively and it early-outed.
//-----------------------------------------------------------------------------
bool CheckVarRange_Generic( ConVar *pVar, int minVal, int maxVal )
{
	// Don't reenter (resetting the variable when we're checking the range might cause us to reenter here).
	static bool bInFunction = false;
	if ( bInFunction )	
		return true;
	bInFunction = true;

	if ( !CanCheat() && !Host_IsSinglePlayerGame() )
	{
		int clampedValue = clamp( pVar->GetInt(), minVal, maxVal );
		if ( clampedValue != pVar->GetInt() )
		{
			Warning( "sv_cheats=0 prevented changing %s outside of the range [0,2] (was %d).\n", pVar->GetName(), pVar->GetInt() );
			pVar->SetValue( clampedValue );
		}
	}

	bInFunction = false;
	return false;
}


void CheckSpecialCheatVars()
{
	static ConVar *mat_picmip = NULL;
	if ( !mat_picmip )
		mat_picmip = g_pCVar->FindVar( "mat_picmip" );

	// In multiplayer, don't allow them to set mat_picmip > 2.	
	if ( mat_picmip )
		CheckVarRange_Generic( mat_picmip, -10, 2 );
	
	CheckVarRange_r_rootlod();
	CheckVarRange_r_lod();
}


void _Host_RunFrame_Render()
{
#ifndef DEDICATED
	VPROF( "_Host_RunFrame_Render" );

	CheckSpecialCheatVars();

	int nOrgNoRendering = mat_norendering.GetInt();

	if ( cl_takesnapshot )
	{
		// turn off no-rendering mode, if taking screenshot
		mat_norendering.SetValue( 0 );
	}

	// update video if not running in background
	g_HostTimes.StartFrameSegment( FRAME_SEGMENT_RENDER );

	CL_LatchInterpolationAmount();

	{
		VPROF( "_Host_RunFrame_Render - UpdateScreen" );
		Host_UpdateScreen();
	}
	{
		VPROF( "_Host_RunFrame_Render - CL_DecayLights" );
		CL_DecayLights ();
	}

	g_HostTimes.EndFrameSegment( FRAME_SEGMENT_RENDER );

	saverestore->OnFrameRendered();

#ifdef USE_SDL
	if ( g_pLauncherMgr )
	{
		g_pLauncherMgr->OnFrameRendered();
	}
#endif

	mat_norendering.SetValue( nOrgNoRendering );
#endif
}

void CL_FindInterpolatedAddAngle( float t, float& frac, AddAngle **prev, AddAngle **pnextangle )
{
#ifndef DEDICATED
	int c = GetLocalClient().addangle.Count();

	*prev = NULL;
	*pnextangle = NULL;

	AddAngle *pentry = NULL;
	for ( int i = 0; i < c; i++ )
	{
		AddAngle *entry = &GetLocalClient().addangle[ i ];

		*pnextangle = entry;

		// Time is earlier
		if ( t < entry->starttime )
		{
			if ( i == 0 )
			{
				*prev = *pnextangle;
				frac = 0.0f;
				return;
			}

			// Avoid div by zero
			if ( entry->starttime == pentry->starttime )
			{
				frac = 0.0f;
				return;
			}

			// Time spans the two entries
			frac = ( t - pentry->starttime ) / ( entry->starttime - pentry->starttime );
			frac = clamp( frac, 0.0f, 1.0f );
			return;
		}

		*prev = *pnextangle;
		pentry = entry;
	}
#endif
}

void CL_DiscardOldAddAngleEntries( float t )
{
#ifndef DEDICATED
	float killtime = t - host_state.interval_per_tick - 0.1f;

	for ( int i = 0; i < GetLocalClient().addangle.Count(); i++ )
	{
		AddAngle *p = &GetLocalClient().addangle[ i ];
		if ( p->starttime <= killtime )
		{
			GetLocalClient().addangle.Remove( i );
			--i;
		}
	}

	// It's safe to reset the master counter once all the entries decay
	if ( GetLocalClient().addangle.Count() == 0 )
	{
		GetLocalClient().prevaddangletotal = GetLocalClient().addangletotal = 0.0f;
	}
#endif
}

#ifndef DEDICATED
void CL_ApplyAddAngle()
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		float curtime = GetBaseLocalClient().GetTime() - host_state.interval_per_tick;

		AddAngle *prev = NULL, *pnextangle = NULL;
		float frac = 0.0f;

		float addangletotal = 0.0f;

		CL_FindInterpolatedAddAngle( curtime, frac, &prev, &pnextangle );

		if ( prev && pnextangle )
		{
			addangletotal = prev->total + frac * ( pnextangle->total - prev->total );
		}
		else
		{ 
			addangletotal = GetLocalClient().prevaddangletotal;
		}

		float amove = addangletotal - GetLocalClient().prevaddangletotal;

		// Update view angles
		GetLocalClient().viewangles[ 1 ] += amove;
		// Update client .dll view of angles
		g_pClientSidePrediction->SetLocalViewAngles( GetLocalClient().viewangles );

		// Remember last total
		GetLocalClient().prevaddangletotal = addangletotal;

		CL_DiscardOldAddAngleEntries( curtime );
	}
}
#endif

CJob *g_pSoundJob;
bool g_bAllowThreadedSound;

void _Host_RunFrame_Sound()
{
#ifndef DEDICATED

	if ( g_pSoundJob )
	{
		return;
	}

	VPROF_BUDGET( "_Host_RunFrame_Sound", VPROF_BUDGETGROUP_OTHER_SOUND );

	g_HostTimes.StartFrameSegment( FRAME_SEGMENT_SOUND );

	if ( !host_threaded_sound.GetBool() || !g_bAllowThreadedSound )
	{
		Host_UpdateSounds();
	}

	g_HostTimes.EndFrameSegment( FRAME_SEGMENT_SOUND );
#endif
}

void Host_BeginThreadedSound()
{
#ifndef DEDICATED
#ifdef _PS3

	if (sv.IsActive())
	{
		Host_UpdateSounds();
		g_pSoundJob = NULL;
		return;
	}
	else if(host_threaded_sound_simplethread.GetBool())
	{
		SNPROF("Kick Sound");
		g_pGcmSharedData->RunAudio(Host_UpdateSounds);
		g_pSoundJob = (CJob*)1;
		return;
	}

#endif

	if ( !host_threaded_sound.GetBool() || !g_bAllowThreadedSound )
	{
		return;
	}

	g_pSoundJob = new CFunctorJob( CreateFunctor( Host_UpdateSounds ) );

	IThreadPool *pSoundThreadPool;
#ifdef _X360
	pSoundThreadPool = g_pAlternateThreadPool;
#else
	pSoundThreadPool = g_pThreadPool;
#endif
	if ( IsX360() )
	{
		g_pSoundJob->SetServiceThread( g_nServerThread );
	}
	pSoundThreadPool->AddJob( g_pSoundJob );
#endif
}

void Host_EndThreadedSound()
{
	if ( !g_pSoundJob )
	{
		return;
	}

#ifdef _PS3

	if(host_threaded_sound_simplethread.GetBool())
	{
		SNPROF("Wait for Sound");
		g_pGcmSharedData->WaitForAudio();
		g_pSoundJob = NULL;
		return;
	}

#endif

	VPROF_BUDGET( "_Host_RunFrame_Sound", VPROF_BUDGETGROUP_OTHER_SOUND );
	g_pSoundJob->WaitForFinishAndRelease();
	g_pSoundJob = NULL;
}

float SV_GetSoundDuration( const char *pSample );
#ifndef DEDICATED
float AudioSource_GetSoundDuration( CSfxTable *pSfx );
#endif

float Host_GetSoundDuration( const char *pSample )
{
#ifndef DEDICATED
	// bug 27822 (crash when leaving 360 credits map)
	// If we don't check connected here, then client can be partially through disconnecting (stringtable dictionary wiped, etc)
	//  but still have the m_pStringTableDictionary pointer hanging around and then the server, on another thread,
	//  calls through this case and tries to read the memory and crashes...
	CClientState &cl = GetBaseLocalClient();

	if ( cl.IsConnected() )
	{
		int index = cl.LookupSoundIndex( pSample );
		if ( index >= 0 )
		{
			CSfxTable *pSfxTable = cl.GetSound( index );
			if ( ( pSfxTable != NULL) && pSfxTable->m_bIsLateLoad )
			{
				DevMsg( "    Reason for late load of '%s': Calling Host_GetSoundDuration().\n", pSample );
			}
			return AudioSource_GetSoundDuration( pSfxTable );
		}
	}
#endif
	return SV_GetSoundDuration( pSample );
}

CON_COMMAND( host_runofftime, "Run off some time without rendering/updating sounds\n" )
{
	if ( args.ArgC() != 2 )
	{
		ConMsg( "Usage:  host_runofftime <seconds>\n" );
		return;
	}

	if ( !sv.IsActive() )
	{
		ConMsg( "host_runofftime:  must be running a server\n" );
		return;
	}

	if ( sv.IsMultiplayer() )
	{
		ConMsg( "host_runofftime:  only valid in single player\n" );
		return;
	}

	float advanceTime = atof( args[1] );
	if ( advanceTime <= 0.0f )
		return;

	// 15 minutes is a _long_ time!!!
	if ( advanceTime > 15.0f * 60.0f )
	{
		ConMsg( "host_runofftime would run off %.2f minutes!!! ignoring\n",
			advanceTime / 60.0f );
		return;
	}

	ConMsg( "Skipping ahead for %f seconds\n", advanceTime );

	SCR_UpdateScreen();
	SCR_UpdateScreen ();
}

#if !defined( _GAMECONSOLE )
S_API int SteamGameServer_GetIPCCallCount();
#else
S_API int SteamGameServer_GetIPCCallCount() { return 0; }
#endif
void Host_ShowIPCCallCount()
{
	// If set to 0 then get out.
	if ( host_ShowIPCCallCount.GetInt() == 0 )
		return;
	
	static float s_flLastTime = 0;
	static int s_nLastTick = host_tickcount;
	static int s_nLastFrame = host_framecount;
	
	// Figure out how often they want to update.
	double flInterval = 0;
	if ( host_ShowIPCCallCount.GetFloat() > 0 )
	{
		flInterval = 1.0f / host_ShowIPCCallCount.GetFloat();
	}
	
	// This is called every frame so increment the frame counter.
	double flCurTime = Plat_FloatTime();
	if ( flCurTime - s_flLastTime >= flInterval )
	{
		uint32 callCount = 0;
#ifndef NO_STEAM
		ISteamClient *pSteamClient =
#ifdef DEDICATED
			Steam3Server().SteamClient()
#else
			Steam3Client().SteamClient()
#endif
			;
		if ( pSteamClient )
		{
			callCount = pSteamClient->GetIPCCallCount();
		}
		else
		{
			// Ok, we're a dedicated server and we need to use this to get it.
			callCount = (uint32)SteamGameServer_GetIPCCallCount();
		}
#endif

		// Avoid a divide by zero.
		int frameCount = host_framecount - s_nLastFrame;
		int tickCount = host_tickcount - s_nLastTick;
		if ( frameCount == 0 || tickCount == 0 )
			return;
			
		Msg( "host_ShowIPCCallCount: %d IPC calls in the past [%d frames, %d ticks]  Avg: [%.2f/frame, %.2f/tick]\n", 
			callCount, frameCount, tickCount, (float)callCount / frameCount, (float)callCount / tickCount );
			
		s_flLastTime = flCurTime;
		s_nLastTick = host_tickcount;
		s_nLastFrame = host_framecount;
	}
}

void Host_SetClientInSimulation( bool bInSimulation )
{
#ifndef DEDICATED
	GetBaseLocalClient().insimulation = bInSimulation;

	// Compute absolute/render time stamp
	g_ClientGlobalVariables.curtime = GetBaseLocalClient().GetTime();
	g_ClientGlobalVariables.frametime = GetBaseLocalClient().GetFrameTime();
#endif
}

static ConVar host_Sleep( "host_sleep", "0", FCVAR_CHEAT, "Force the host to sleep a certain number of milliseconds each frame." );
extern ConVar sv_alternateticks;
ConVar host_print_frame_times( "host_print_frame_times", "0" );

#ifndef DEDICATED
static void PrintHostFrameTimes( int nNumTicks, float flHostRemainder, float flMinimumTickInterval ) 
{
	const int nFrameHistorySize = 100;
	static float flFrameTimes[nFrameHistorySize];
	static int nFrameIndex = 0;

	flFrameTimes[nFrameIndex] = host_frametime;
	nFrameIndex = ( nFrameIndex + 1 ) % nFrameHistorySize;

	float flMinFrameTime = 1.0f;
	float flAvgFrameTime = 0.0f;
	for ( int i = 0; i < nFrameHistorySize; ++ i )
	{
		if ( flFrameTimes[i] < flMinFrameTime )
		{
			flMinFrameTime = flFrameTimes[i];
		}
		flAvgFrameTime += flFrameTimes[i];
	}
	flAvgFrameTime /= (float)nFrameHistorySize;

	con_nprint_t printinfo;
	printinfo.index = 1;
	printinfo.time_to_live = -1;
	printinfo.color[0] = printinfo.color[1] = printinfo.color[2] = 1.0f;
	printinfo.fixed_width_font = true;
	Con_NXPrintf( &printinfo, "ticks: %d, host_remainder: %f, host_frametime: %f, minimum interval: %f\n", nNumTicks, flHostRemainder, host_frametime, flMinimumTickInterval );
	printinfo.index = 2;
	Con_NXPrintf( &printinfo, "Running min frametime: %f, running avg frametime: %f\n", flMinFrameTime, flAvgFrameTime );
}
#endif // DEDICATED

#if !( defined( _CERT ) || defined( DEDICATED ) )
ConVar fs_enable_stats( "fs_enable_stats", "0" );
static void PrintFsStats() 
{
	const int nFrameHistorySize = 100;
	const int nStatsSize = 6;
	static int nStats[nStatsSize][nFrameHistorySize] = { 0 };
	static const char * pStatsTitle[] =
	{
		"# of seeks",
		"Time seeking",
		"# of reads",
		"Time reading",
		"Bytes read",
		"# of fopen",
	};
	enum Mode
	{
		NORMAL,
		IN_MS,
		IN_BYTES,
		SKIPPED,
	};

	static const Mode nModes[] =
	{
		NORMAL,
		IN_MS,
		NORMAL,
		IN_MS,
		IN_BYTES,
		SKIPPED,			// In the Source1 engine, there is a very strong correlation between fopen, seeks and reads. Don't display this one.
	};

	static int nFrameIndex = 0;

	IIoStats *pIoStats = g_pFileSystem->GetIoStats();
	if ( pIoStats == NULL )
	{
		con_nprint_t printinfo;
		printinfo.index = 1;
		printinfo.time_to_live = -1;
		printinfo.color[0] = printinfo.color[1] = printinfo.color[2] = 1.0f;
		printinfo.fixed_width_font = true;
		Con_NXPrintf( &printinfo, "IO stats is disabled.\n" );
		return;
	}
	nStats[0][nFrameIndex] = pIoStats->GetNumberOfFileSeeks();
	nStats[1][nFrameIndex] = pIoStats->GetTimeInFileSeek();
	nStats[2][nFrameIndex] = pIoStats->GetNumberOfFileReads();
	nStats[3][nFrameIndex] = pIoStats->GetTimeInFileReads();
	nStats[4][nFrameIndex] = pIoStats->GetFileReadTotalSize();
	nStats[5][nFrameIndex] = pIoStats->GetNumberOfFileOpens();
	pIoStats->Reset();
	nFrameIndex = ( nFrameIndex + 1 ) % nFrameHistorySize;

	int nMinStats[nStatsSize], nMaxStats[nStatsSize], nAvgStats[nStatsSize];

	for (int i = 0 ; i < nStatsSize ; ++i )
	{
		if ( nModes[i] == SKIPPED )
		{
			continue;		// Skip this one
		}
		nMinStats[i] = INT_MAX;
		nMaxStats[i] = INT_MIN;
		nAvgStats[i] = 0;

		for ( int j = 0; j < nFrameHistorySize; ++j )
		{
			if ( nStats[i][j] < nMinStats[i] )
			{
				nMinStats[i] = nStats[i][j];
			}
			else if ( nStats[i][j] > nMaxStats[i] )
			{
				nMaxStats[i] = nStats[i][j];
			}
			nAvgStats[i] += nStats[i][j];
		}
	}

	con_nprint_t printinfo;
	printinfo.index = 1;
	printinfo.time_to_live = -1;
	printinfo.color[0] = printinfo.color[1] = printinfo.color[2] = 1.0f;
	printinfo.fixed_width_font = true;
	Con_NXPrintf( &printinfo, "IO stats from the last %d frames.\n", nFrameHistorySize );
	int nPos = 3;
	for ( int i = 0 ; i < nStatsSize ; ++i )
	{
		if ( nModes[i] == SKIPPED )
		{
			continue;	// Skip this stat, not interesting...
		}
		printinfo.index = nPos++;
		float flAverage = ( float )( nAvgStats[i] ) / ( float )( nFrameHistorySize );
		const float flFrameRate = 30.0f;			// Hardcoded at this moment
		float flAveragePerSec = flAverage * flFrameRate;

		// It seems that %3.1f format is not respected (at least on PS3, switch everything to %d).
		// Also there is no point displaying min, it is pretty much always 0.
		switch ( nModes[i] )
		{
		case NORMAL:
			Con_NXPrintf( &printinfo, "%s - Avg:%5d    - Max:%5d    -%5d   /s\n", pStatsTitle[i], ( int )flAverage, nMaxStats[i], ( int )flAveragePerSec );
			//Con_NXPrintf( &printinfo, "%s - Min:%2d    - Avg:% 3.1f    - Max:%5d    -% 3.1f   /s\n", pStatsTitle[i], nMinStats[i], flAverage, nMaxStats[i], flAveragePerSec );
			break;
		case IN_MS:
			Con_NXPrintf( &printinfo, "%s - Avg:%5d ms - Max:%5d ms -%5d ms/s\n", pStatsTitle[i], ( int )flAverage, nMaxStats[i], ( int )flAveragePerSec );
			//Con_NXPrintf( &printinfo, "%s - Min:%2d ms - Avg:% 3.1f ms - Max:%5d ms -% 3.1f ms/s\n", pStatsTitle[i], nMinStats[i], flAverage, nMaxStats[i], flAveragePerSec );
			break;
		case IN_BYTES:
			{
				//int nMin = nMinStats[i] / 1024;
				flAverage /= 1024.f;
				int nMax = nMaxStats[i] / 1024;
				flAveragePerSec /= 1024.f;
				Con_NXPrintf( &printinfo, "%s - Avg:%5d Kb - Max:%5d Kb -%5d Kb/s\n", pStatsTitle[i], ( int )flAverage, nMax, ( int )flAveragePerSec );
			}
			break;

		}
	}

	extern float	g_fDelayForChoreo;
	printinfo.index = nPos++;
	Con_NXPrintf( &printinfo, "Delay for choreo: %5d ms\n", ( int )( g_fDelayForChoreo * 1000 ) );
}
#endif

#define LOG_FRAME_OUTPUT 0

void _Host_RunFrame (float time)
{
	g_HostTimes.MarkFrameStartTime();

	MDLCACHE_COARSE_LOCK_(g_pMDLCache);
	static double host_remainder = 0.0f;
	double prevremainder;
	bool shouldrender;

#if defined( RAD_TELEMETRY_ENABLED )

	if( g_Telemetry.DemoTickEnd == ( uint32 )-1 )

	{

		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "quit\n" );

	}

#endif

#ifndef DEDICATED
	CClientState &cl = GetBaseLocalClient();
#endif

	int numticks;
	{
		// Profile scope specific to the top of this function, protect from setjmp() problems
		VPROF( "_Host_RunFrame_Upto_MarkFrame" );

		if ( host_checkheap )
		{
#if defined(_WIN32)
			if ( _heapchk() != _HEAPOK )
			{
				Sys_Error( "_Host_RunFrame (top):  _heapchk() != _HEAPOK\n" );
			}
#endif
		}

		static double timeLastMemCompact;
		if ( mem_incremental_compact_rate.GetFloat() > 0 )
		{
			double curTime = Plat_FloatTime();
			if ( curTime - timeLastMemCompact > (double)mem_incremental_compact_rate.GetFloat() )
			{
				timeLastMemCompact = curTime;
				g_pMemAlloc->CompactIncremental();
			}
		}

		if ( host_Sleep.GetInt() )
		{
			Sys_Sleep( host_Sleep.GetInt() );
		}

		// Slow down the playback?	
		if ( g_iVCRPlaybackSleepInterval )
		{
			Sys_Sleep( g_iVCRPlaybackSleepInterval );
		}

		MapReslistGenerator().RunFrame();

		static int lastrunoffsecond = -1;

		if ( setjmp ( host_enddemo ) )
			return;			// demo finished.

		// decide the simulation time
		Host_AccumulateTime ( time );
		_Host_SetGlobalTime();

		shouldrender = !sv.IsDedicated();

		// FIXME:  Could track remainder as fractional ticks instead of msec
		prevremainder = host_remainder;
		if ( prevremainder < 0 )
			prevremainder = 0;

	#if !defined(DEDICATED)
		if ( !demoplayer->IsPlaybackPaused() )
	#endif
		{
			host_remainder += host_frametime;
		}

		numticks = 0;	// how many ticks we will simulate this frame
		
		// If we're using sv_alternateticks, make sure there are at least 2 ticks worth of host_remainder time to consume
#if defined( LINUX )
		bool bAlternateTicks = false;
#else
		bool bAlternateTicks = sv_alternateticks.GetBool() && ( GetBaseLocalClient().m_nMaxClients == 1 );
#endif

		float flMinimumTickInterval = bAlternateTicks ? host_state.interval_per_tick * 2.0f : host_state.interval_per_tick;
		if ( host_remainder >= flMinimumTickInterval )
		{
			numticks = (int)( floor(host_remainder / host_state.interval_per_tick ) );

			// round to nearest even ending tick in alternate ticks mode so the last
			// tick is always simulated prior to updating the network data
			if ( bAlternateTicks )
			{
				int startTick = g_ServerGlobalVariables.tickcount;
				int endTick = startTick + numticks;
				endTick = endTick & ~(0x1); // an even number of ticks, rounding down
				numticks = endTick - startTick;
			}

			host_remainder -= numticks * host_state.interval_per_tick;
		}

		host_nexttick = host_state.interval_per_tick - host_remainder;

		g_pMDLCache->MarkFrame();

#if !( defined( _CERT ) || defined( DEDICATED ) )
		if ( host_print_frame_times.GetBool() )
		{
			PrintHostFrameTimes( numticks, host_remainder, flMinimumTickInterval );
		}
		if ( fs_enable_stats.GetBool() )
		{
			PrintFsStats();
		}
#endif // !_CERT || !DEDICATED
	}

	{
		// Profile scope, protect from setjmp() problems
		VPROF( "_Host_RunFrame" );

		g_HostTimes.StartFrameSegment( FRAME_SEGMENT_CMD_EXECUTE );

		// process console commands
#if defined( _GAMECONSOLE ) && !defined( _CERT )
		static volatile bool s_bMemDebug = !!CommandLine()->FindParm( "-mem_dump_frames" );
		if ( s_bMemDebug )
			Cbuf_AddText( CBUF_FIRST_PLAYER, "mem_dump;\n" );

		static char s_chBufferConCommandHack[256] = {0};
		if ( s_chBufferConCommandHack[0] )
		{
			Cbuf_AddText( CBUF_FIRST_PLAYER, s_chBufferConCommandHack );
			s_chBufferConCommandHack[0] = 0;
		}
#endif
		Cbuf_Execute ();

		if ( NET_IsDedicatedForXbox() )
		{
			if ( NET_IsDedicated() && !NET_IsMultiplayer() )
			{
				NET_SetMultiplayer( true );
			}
		}
		else
		{
			// initialize networking after commandline & autoexec.cfg have been parsed
			NET_Init( NET_IsDedicated() );
		}

		g_HostTimes.EndFrameSegment( FRAME_SEGMENT_CMD_EXECUTE );

		// Msg( "Running %i ticks (%f remainder) for frametime %f total %f tick %f delta %f\n", numticks, remainder, host_frametime, host_time );
		g_ServerGlobalVariables.interpolation_amount = 0.0f;
#ifndef DEDICATED
		g_ClientGlobalVariables.interpolation_amount = 0.0f;

		cl.insimulation = true;
#endif

		host_frameticks = numticks;
		host_currentframetick = 0;

#if !defined( DEDICATED )
		// This is to make the tool do both sim + rendering on the initial frame
		// cl.IsActive changes in the loop below, as does scr_nextdrawtick
		// We're just caching off the state here so that we have a consistent return value
		// for enginetool->IsInGame the entire frame
		g_pEngineToolInternal->SetIsInGame( cl.IsActive() && ( scr_nextdrawtick == 0 ) );
#endif
		CJob *pGameJob = NULL;

// threaded path only supported in listen server
#ifndef DEDICATED
		if ( !IsEngineThreaded() )
#endif
		{
#ifndef DEDICATED
			if ( g_ClientDLL )
			{
				g_ClientDLL->IN_SetSampleTime(host_frametime);
			}
			g_ClientGlobalVariables.simTicksThisFrame = 1;
#endif
#ifndef DEDICATED
			cl.m_tickRemainder = host_remainder;
			cl.SetFrameTime( host_frametime );
#endif
			g_ServerGlobalVariables.simTicksThisFrame = 1;
			for ( int tick = 0; tick < numticks; tick++ )
			{ 
				// Emit an ETW event every simulation frame.
				ETWSimFrameMark( sv.IsDedicated() );

				double now = Plat_FloatTime();
				float jitter = now - host_idealtime;

				// Track jitter (delta between ideal time and actual tick execution time)
				host_jitterhistory[ host_jitterhistorypos ] = jitter;
				host_jitterhistorypos = ( host_jitterhistorypos + 1 ) % ARRAYSIZE(host_jitterhistory);

				// Very slowly decay "ideal" towards current wall clock unless delta is large
				if ( fabs( jitter ) > 1.0f )
				{
					host_idealtime = now;
				}
				else
				{
					host_idealtime = 0.99 * host_idealtime + 0.01 * now;
				}

				// process any asynchronous network traffic (TCP), set net_time
				NET_RunFrame( now );

				// Only send updates on final tick so we don't re-encode network data multiple times per frame unnecessarily
				bool bFinalTick = ( tick == (numticks - 1) );

				if ( NET_IsDedicatedForXbox() )
				{
					if ( NET_IsDedicated() && !NET_IsMultiplayer() )
					{
						NET_SetMultiplayer( true );
					}
				}
				else
				{
					// initialize networking after commandline & autoexec.cfg have been parsed
					NET_Init( NET_IsDedicated() );
				}

				g_ServerGlobalVariables.tickcount = sv.m_nTickCount;
				// NOTE:  Do we want do this at start or end of this loop?
				++host_tickcount;
				++host_currentframetick;
#ifndef DEDICATED
				g_ClientGlobalVariables.tickcount = cl.GetClientTickCount();

				// Make sure state is correct
				CL_CheckClientState();
#endif
				//-------------------
				// input processing
				//-------------------
				if ( host_runframe_input_parcelremainder.GetBool() )
				{
					//sv_alternateticks creates instances where we're guaranteed to have a first-loop prevremainder of more than a tick, creating negative frame times for input deeper down
					float fParcelRemainder = MIN( prevremainder, host_state.interval_per_tick );
					_Host_RunFrame_Input( fParcelRemainder, bFinalTick );
					prevremainder -= fParcelRemainder;
				}
				else
				{
					_Host_RunFrame_Input( prevremainder, bFinalTick );
					prevremainder = 0;
				}

				//-------------------
				//
				// server operations
				//
				//-------------------

				_Host_RunFrame_Server( bFinalTick );

				// Additional networking ops for SPLITPACKET stuff (99.9% of the time this will be an empty list of work)
				NET_SendQueuedPackets();
				//-------------------
				//
				// client operations
				//
				//-------------------
#ifndef DEDICATED
				if ( !sv.IsDedicated() )
				{
					_Host_RunFrame_Client( bFinalTick );
				}

				toolframework->Think( bFinalTick );
#endif

				host_idealtime += host_state.interval_per_tick;
			}

#if defined( VOICE_OVER_IP ) && !defined( DEDICATED )
			// Send any enqueued voice data to the server
			CL_ProcessVoiceData();

			if ( numticks == 0 )
			{
				_Host_ProcessVoice_Server();
			}
#endif // VOICE_OVER_IP && !DEDICATED

			// run HLTV (both active, and not yet connected)
			for ( CHltvServerIterator hltv; hltv; hltv.Next() )
			{
				hltv->RunFrame();
			}

			if ( hltvtest )
			{
				hltvtest->RunFrame();
			}

#if defined( REPLAY_ENABLED )
			// run replay if active
			if ( replay )
			{
				replay->RunFrame();

				if ( g_pServerReplayHistoryManager )
					g_pServerReplayHistoryManager->Update();
			}

			if ( g_pClientReplayHistoryManager && g_pClientReplayHistoryManager->IsInitialized() )
				g_pClientReplayHistoryManager->Update();
#endif

#ifndef DEDICATED
			// This is a hack to let timedemo pull messages from the queue faster than every 15 msec
			// Also when demoplayer is skipping packets to a certain tick we should process the queue
			// as quickly as we can.
			if ( numticks == 0 && ( demoplayer->IsPlayingTimeDemo() || demoplayer->IsSkipping() ) )
			{
				_Host_RunFrame_Client( true );
			}
			if ( !sv.IsDedicated() )
			{
				// This causes cl.gettime() to return the true clock being used for rendering (tickcount * rate + remainder)
				Host_SetClientInSimulation( false );
				// Now allow for interpolation on client
				g_ClientGlobalVariables.interpolation_amount = ( cl.m_tickRemainder / host_state.interval_per_tick );

				//-------------------
				// Run prediction if it hasn't been run yet
				//-------------------
				// If we haven't predicted/simulated the player (multiplayer with prediction enabled and
				//  not a listen server with zero frame lag, then go ahead and predict now
				CL_RunPrediction( PREDICTION_NORMAL );

				CL_ApplyAddAngle();

				// The mouse is always simulated for the current frame's time
				// This makes updates smooth in every case
				// continuous controllers affecting the view are also simulated this way
				// but they have a cap applied by IN_SetSampleTime() so they are not also
				// simulated during input gathering
				CL_ExtraMouseUpdate( g_ClientGlobalVariables.frametime );
			}
#endif
#if LOG_FRAME_OUTPUT
			if ( !cl.IsPaused() || !sv.IsPaused() )
			{
				Msg("=============SIM: CLIENT %5d + %d, SERVER %5d + %d\t REM: %.2f\n", cl.GetClientTickCount(), numticks, sv.m_nTickCount, numticks, host_remainder*1000.0f );
			}
#endif
		}
#ifndef DEDICATED
		else
		{
			static int numticks_last_frame = 0;
			static float host_remainder_last_frame = 0, prev_remainder_last_frame = 0, last_frame_time = 0;

			int clientticks;
			int serverticks;

			clientticks = numticks_last_frame;
			cl.m_tickRemainder = host_remainder_last_frame;
			cl.SetFrameTime( last_frame_time );
			if ( g_ClientDLL )
			{
				g_ClientDLL->IN_SetSampleTime(last_frame_time);
			}

			last_frame_time = host_frametime;

			serverticks = numticks;
			g_ClientGlobalVariables.simTicksThisFrame = clientticks;
			g_ServerGlobalVariables.simTicksThisFrame = serverticks;
			g_ServerGlobalVariables.tickcount = sv.m_nTickCount;

			// THREADED: Run Client
			// -------------------
			for ( int tick = 0; tick < clientticks; tick++ )
			{ 
				// process any asynchronous network traffic (TCP), set net_time
				NET_RunFrame(  Plat_FloatTime() );

				// Only send updates on final tick so we don't re-encode network data multiple times per frame unnecessarily
				bool bFinalTick = ( tick == (clientticks - 1) );

				if ( NET_IsDedicatedForXbox() )
				{
					if ( NET_IsDedicated() && !NET_IsMultiplayer() )
					{
						NET_SetMultiplayer( true );
					}
				}
				else
				{
					// initialize networking after commandline & autoexec.cfg have been parsed
					NET_Init( NET_IsDedicated() );
				}

				g_ClientGlobalVariables.tickcount = cl.GetClientTickCount();

				// Make sure state is correct
				CL_CheckClientState();
				// Additional networking ops for SPLITPACKET stuff (99.9% of the time this will be an empty list of work)
				NET_SendQueuedPackets();
				//-------------------
				//
				// client operations
				//
				//-------------------
				if ( !sv.IsDedicated() )
				{
					_Host_RunFrame_Client( bFinalTick );
				}
				toolframework->Think( bFinalTick );
			}

#if defined( VOICE_OVER_IP )
			// Send any enqueued voice data to the server
			CL_ProcessVoiceData();
#endif // VOICE_OVER_IP

			// This is a hack to let timedemo pull messages from the queue faster than every 15 msec
			// Also when demoplayer is skipping packets to a certain tick we should process the queue
			// as quickly as we can.
			if ( clientticks == 0 && ( demoplayer->IsPlayingTimeDemo() || demoplayer->IsSkipping() ) )
			{
				_Host_RunFrame_Client( true );
			}

			// This causes cl.gettime() to return the true clock being used for rendering (tickcount * rate + remainder)
			Host_SetClientInSimulation( false );
			// Now allow for interpolation on client
			g_ClientGlobalVariables.interpolation_amount = ( cl.m_tickRemainder / host_state.interval_per_tick );

			//-------------------
			// Run prediction if it hasn't been run yet
			//-------------------
			// If we haven't predicted/simulated the player (multiplayer with prediction enabled and
			//  not a listen server with zero frame lag, then go ahead and predict now
			CL_RunPrediction( PREDICTION_NORMAL );

			CL_ApplyAddAngle();

			Host_SetClientInSimulation( true );

			// THREADED: Run Input
			// -------------------
			int saveTick = g_ClientGlobalVariables.tickcount;

			for ( int tick = 0; tick < serverticks; tick++ )
			{
				// NOTE:  Do we want do this at start or end of this loop?
				++host_tickcount;
				++host_currentframetick;
				g_ClientGlobalVariables.tickcount = host_tickcount;
				bool bFinalTick = tick==(serverticks-1) ? true : false;
				_Host_RunFrame_Input( prevremainder, bFinalTick );
				prevremainder = 0;
				// process any asynchronous network traffic (TCP), set net_time
				NET_RunFrame(  Plat_FloatTime() );
			}

#if defined( VOICE_OVER_IP )
			if ( serverticks == 0 )
			{
				_Host_ProcessVoice_Server();
			}
#endif

			Host_SetClientInSimulation( false );

			// The mouse is always simulated for the current frame's time
			// This makes updates smooth in every case
			// continuous controllers affecting the view are also simulated this way
			// but they have a cap applied by IN_SetSampleTime() so they are not also
			// simulated during input gathering
			CL_ExtraMouseUpdate( g_ClientGlobalVariables.frametime );

			g_ClientGlobalVariables.tickcount = saveTick;
			numticks_last_frame = numticks;
			host_remainder_last_frame = host_remainder;

			// THREADED: Run Server
			// -------------------
			// set net_time once before running the server
			NET_SetTime( Plat_FloatTime() );

#ifdef _PS3
			SNPROF("Kick sv");
			g_pGcmSharedData->RunServer(_Host_RunFrame_Server_Async, serverticks);
			pGameJob = (CJob*)1;
#else

			pGameJob = new CFunctorJob( CreateFunctor( _Host_RunFrame_Server_Async, serverticks ) );
			if ( IsX360() )
			{
				pGameJob->SetServiceThread( g_nServerThread );
			}
			g_pThreadPool->AddJob( pGameJob );
#endif
#if LOG_FRAME_OUTPUT
			if ( !cl.IsPaused() || !sv.IsPaused() )
			{
				Msg("=============SIM: CLIENT %5d + %d, SERVER %5d + %d\t REM: %.2f\n", cl.GetClientTickCount(), clientticks, sv.m_nTickCount, serverticks, host_remainder*1000.0f );
			}
#endif
		}
#endif	// DEDICATED

#if defined ( INCLUDE_SCALEFORM )
		if ( g_pScaleformUI && shouldrender )
		{
			float timeScale = host_timescale.GetFloat() * sv.GetTimescale();
			if ( timeScale <= 0.0f )
				timeScale = 1.0f;

			// using realtime here, because we never want scaleform menus paused (they fail to work).
			// we may want to remove timescale here also
			static float flLastScaleformRunFrame = g_ClientGlobalVariables.realtime;
			g_pScaleformUI->RunFrame( ( g_ClientGlobalVariables.realtime - flLastScaleformRunFrame ) / timeScale );
			flLastScaleformRunFrame = g_ClientGlobalVariables.realtime;
		}
#endif

		g_Log.RunFrame();

		if ( shouldrender )
		{
#if LOG_FRAME_OUTPUT
			if ( !cl.IsPaused() || !sv.IsPaused() )
			{
				static float lastFrameTime = 0;
				float frametime = g_ClientGlobalVariables.curtime - lastFrameTime;
				Msg("RENDER AT: %6.4f: %.2fms [%.2fms implicit] frametime\n", 
					g_ClientGlobalVariables.curtime, g_ClientGlobalVariables.frametime*1000.0f, frametime * 1000.0f);
				lastFrameTime = g_ClientGlobalVariables.curtime;
			}
#endif

			//-------------------
			// rendering
			//-------------------
			_Host_RunFrame_Render();

			//-------------------
			// sound
			//-------------------
			_Host_RunFrame_Sound();

			if ( g_bVCRSingleStep )
			{
				VCR_EnterPausedState();
			}
		}

		//-------------------
		// simulation
		//-------------------
		g_HostTimes.MarkSwapTime( );
#ifndef DEDICATED
		if ( !sv.IsDedicated() )
		{
			VPROF_BUDGET( "_Host_RunFrame - ClientDLL_Update", VPROF_BUDGETGROUP_CLIENT_SIM );
			// Client-side simulation
			g_HostTimes.StartFrameSegment( FRAME_SEGMENT_CLDLL );

			ClientDLL_Update();

			g_HostTimes.EndFrameSegment( FRAME_SEGMENT_CLDLL );
		}
#endif
		if ( pGameJob )
		{
			{
				VPROF_BUDGET( "WaitForAsyncServer", "AsyncServer" );
#ifndef _PS3
				if ( Host_IsSinglePlayerGame() )
				{
					// This should change to a YieldWait if the server starts wanting to parallel process. If
					// so, will need some route for the server to queue up work it wants to execute outside
					// its frame, otherwise some of it would be performed during the yield. Right now
					// need to wait for server so we don't stall on queued AI operations (toml 7/3/2007)
					pGameJob->ExecuteAndRelease();
				}
				else
				{
					pGameJob->WaitForFinishAndRelease();
				}
#else
                SNPROF("WaitFor Sv");
				g_pGcmSharedData->WaitForServer();
#endif
			}

			SV_FrameExecuteThreadDeferred();
		}

		Host_EndThreadedSound();

		//-------------------
		// time
		//-------------------

		Host_Speeds();

		Host_UpdateMapList();

		host_framecount++;
#if !defined(DEDICATED)
		if ( !demoplayer->IsPlaybackPaused() )
		{
			host_time = host_tickcount * host_state.interval_per_tick + cl.m_tickRemainder;
		}
#endif
		Host_PostFrameRate( host_frametime );

		if ( host_checkheap )
		{
#ifdef _WIN32
			if ( _heapchk() != _HEAPOK )
			{
				Sys_Error( "_Host_RunFrame (bottom):  _heapchk() != _HEAPOK\n" );
			}
#endif
		}

		Status_CheckSendETWMark();
		Host_CheckDumpMemoryStats();

		GetTestScriptMgr()->CheckPoint( "frame_end" );
	} // Profile scope, protect from setjmp() problems

	Host_ShowIPCCallCount();
}
/*
==============================
Host_Frame

==============================
*/
void Host_RunFrame( float time )
{
	static  double	timetotal = 0;
	static  int		timecount = 0;
	static	double  timestart = 0;

#ifndef DEDICATED
	if ( !scr_drawloading && sv.IsActive() && GetBaseLocalClient().IsActive() && !sv.m_bLoadgame)
	{
		switch ( host_thread_mode.GetInt() )
		{
		case HTM_DISABLED:	g_bThreadedEngine = false;									break;
		case HTM_DEFAULT:	g_bThreadedEngine = ( g_pThreadPool->NumThreads() > 0 );	break;
		case HTM_FORCED:	g_bThreadedEngine = true;									break;
		}
#ifdef _PS3
		g_bThreadedEngine = true; // Not reqd anuymore ?
#endif 
	}
	else
#endif
	{
		g_bThreadedEngine = false;
	}
	
	{
		VPROF( "UpdateDynamicModels" );
		CMDLCacheCriticalSection critsec(g_pMDLCache);
		modelloader->UpdateDynamicModels();
	}

	if ( !host_profile.GetBool() )
	{
		_Host_RunFrame( time );
		return;
	}

	double	time1 = Sys_FloatTime();

	_Host_RunFrame( time );

	double	time2 = Sys_FloatTime();

	timetotal += time2 - time1; // time in seconds
	timecount++;

	if (timecount < 1000)
		return;

	float fps = 1000/(time2 - timestart);

	ConMsg ("host_profile : %i clients, %.1f msec, %.1f fps\n",  
		sv.GetNumClients(),  timetotal, fps );

	timecount = 0;
	timetotal = 0;
	timestart = time2;
}

//-----------------------------------------------------------------------------
// If the user passed -lv on the command-line, they want low violence mode.
//-----------------------------------------------------------------------------
bool IsLowViolence_CommandLine()
{
#if defined( _LOWVIOLENCE )
	// a low violence build can not be-undone
	return true;
#endif

	return ( CommandLine()->FindParm( "-lv" ) != 0 );
}

#ifndef DEDICATED
//-----------------------------------------------------------------------------
// A more secure means of enforcing low violence.
//-----------------------------------------------------------------------------
bool IsLowViolence_Secure()
{
	// CS:GO does not have any low violence regions. Ignore what Steam reports.
#if defined( CSTRIKE15 )
	return false;
#endif

#ifndef _GAMECONSOLE
	if ( IsPC() && Steam3Client().SteamApps() )
	{
		// let Steam determine current violence settings 		
		return Steam3Client().SteamApps()->BIsLowViolence();
	}
#endif

	if ( IsGameConsole() )
	{
#if defined( _LOWVIOLENCE )
		// a low violence build can not be-undone
		return true;
#endif
		// Users can opt into low violence mode on the command-line.
		if ( IsLowViolence_CommandLine() )
		{
			return true;
		}
	}
		
	return false;
}


//-----------------------------------------------------------------------------
// If "User Token 2" exists in HKEY_CURRENT_USER/Software/Valve/Half-Life/Settings
// then we disable gore. Obviously not very secure.
//-----------------------------------------------------------------------------
bool IsLowViolence_Registry()
{
	if ( IsGameConsole() )
	{
#if defined( _LOWVIOLENCE )
		// a low violence build can not be-undone
		return true;
#endif
		// Users can opt into low violence mode on the command-line.
		if ( IsLowViolence_CommandLine() )
		{
			return true;
		}
	}

// CS:GO does not have any low violence regions. Ignore the registry settings.
#if defined( CSTRIKE15 )
	return false;
#endif

	char szSubKey[128];
	int nBufferLen;
	char szBuffer[128];
	bool bReducedGore = false;

	memset( szBuffer, 0, 128 );

	char const *appname = "Source";
	Q_snprintf(szSubKey, sizeof( szSubKey ), "Software\\Valve\\%s\\Settings", appname );

	nBufferLen = 127;
	Q_strncpy( szBuffer, "", sizeof( szBuffer ) );

	Sys_GetRegKeyValue( szSubKey, "User Token 2", szBuffer,	nBufferLen, szBuffer );

	// Gore reduction active?
	bReducedGore = ( Q_strlen( szBuffer ) > 0 ) ? true : false;
	if ( !bReducedGore )
	{
		Sys_GetRegKeyValue( szSubKey, "User Token 3", szBuffer, nBufferLen, szBuffer );

		bReducedGore = ( Q_strlen( szBuffer ) > 0 ) ? true : false;
	}

	char gamedir[MAX_OSPATH];
	Q_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );

	// also check mod specific directories for LV changes
	Q_snprintf(szSubKey, sizeof( szSubKey ), "Software\\Valve\\%s\\%s\\Settings", appname, gamedir );

	nBufferLen = 127;
	Q_strncpy( szBuffer, "", sizeof( szBuffer ) );

	Sys_GetRegKeyValue( szSubKey, "User Token 2", szBuffer,	nBufferLen, szBuffer );
	if ( Q_strlen( szBuffer ) > 0 )
	{
		bReducedGore = true;
	}

	Sys_GetRegKeyValue( szSubKey, "User Token 3", szBuffer,	nBufferLen, szBuffer );
	if ( Q_strlen( szBuffer ) > 0 )
	{
		bReducedGore = true;
	}
	
	return bReducedGore;
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Host_CheckGore( void )
{
	bool bLowViolenceRegistry = false;
	bool bLowViolenceSecure = false;
	bool bLowViolenceCommandLine = IsLowViolence_CommandLine();

#ifndef DEDICATED
	//
	// First check the old method of enabling low violence via the registry.
	//
#ifdef WIN32
	bLowViolenceRegistry = IsLowViolence_Registry();
#endif
	//
	// Next check the new method of enabling low violence based on country of purchase
	// and other means that are inaccessible by the user.
	//
	bLowViolenceSecure = IsLowViolence_Secure();
#endif

	//
	// If either method says "yes" to low violence, we're in low violence mode.
	//
	if ( bLowViolenceRegistry || bLowViolenceSecure || bLowViolenceCommandLine )
	{
		g_bLowViolence = true;
		
		if ( bLowViolenceRegistry )
		{
			violence_hblood.SetValue( 0 );
			violence_hgibs.SetValue( 0 );
			violence_ablood.SetValue( 0 );
			violence_agibs.SetValue( 0 );
		}
	}
	else
	{
		g_bLowViolence = false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Host_InitProcessor( void )
{
	const CPUInformation& pi = GetCPUInformation();

	// Compute Frequency in Mhz: 
	char* szFrequencyDenomination = "Mhz";
	double fFrequency = pi.m_Speed / 1000000.0;

	// Adjust to Ghz if nessecary:
	if( fFrequency > 1000.0 )
	{
		fFrequency /= 1000.0;
		szFrequencyDenomination = "Ghz";
	}

	char szFeatureString[256];
	Q_strncpy( szFeatureString, pi.m_szProcessorID, sizeof( szFeatureString ) );
	Q_strncat( szFeatureString, " ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );

	if( pi.m_bSSE )
	{
		if( MathLib_SSEEnabled() ) Q_strncat(szFeatureString, "SSE ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );
		else					   Q_strncat(szFeatureString, "(SSE) ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );
	}

	if( pi.m_bSSE2 )
	{
		if( MathLib_SSE2Enabled() ) Q_strncat(szFeatureString, "SSE2 ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );
		else					   Q_strncat(szFeatureString, "(SSE2) ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );
	}

	if( pi.m_bMMX )
	{
		if( MathLib_MMXEnabled() ) Q_strncat(szFeatureString, "MMX ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );
		else					   Q_strncat(szFeatureString, "(MMX) ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );
	}

	if( pi.m_bRDTSC )	Q_strncat(szFeatureString, "RDTSC ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );
	if( pi.m_bCMOV )	Q_strncat(szFeatureString, "CMOV ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );
	if( pi.m_bFCMOV )	Q_strncat(szFeatureString, "FCMOV ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );

	// Remove the trailing space.  There will always be one.
	szFeatureString[Q_strlen(szFeatureString)-1] = '\0';

	// Dump CPU information:
	if( pi.m_nLogicalProcessors == 1 )
	{
		ConDMsg( "1 CPU, Frequency: %.01f %s,  Features: %s\n", 
			fFrequency,
			szFrequencyDenomination,
			szFeatureString
			);
	} 
	else
	{
		char buffer[256] = "";
		if( pi.m_nPhysicalProcessors != pi.m_nLogicalProcessors )
		{
			Q_snprintf(buffer, sizeof( buffer ), " (%i physical)", (int) pi.m_nPhysicalProcessors );
		}

		ConDMsg( "%i CPUs%s, Frequency: %.01f %s,  Features: %s\n", 
			(int)pi.m_nLogicalProcessors,
			buffer,
			fFrequency,
			szFrequencyDenomination,
			szFeatureString
			);
	}

#if defined( _WIN32 )
	if ( s_bInitPME )
	{
		// Initialize the performance monitoring events code.
		InitPME();
	}
#endif
}

//-----------------------------------------------------------------------------
// Specifically used by the model loading code to mark models
// touched by the current map
//-----------------------------------------------------------------------------
int Host_GetServerCount( void )
{

#ifndef DEDICATED
	if ( GetBaseLocalClient().m_nSignonState >= SIGNONSTATE_NEW ||
		 ( GetBaseLocalClient().m_nSignonState >= SIGNONSTATE_CONNECTED && GetBaseLocalClient().m_bServerInfoProcessed ) )
	{
		// the server count cannot be relied on until the server info message is processed
		// the mulitple state checking state guarantees its validity
		return GetBaseLocalClient().m_nServerCount;
	}
	else
#endif
		if ( sv.m_State >= ss_loading )
		{
			return sv.GetSpawnCount();
		}
	
	// this is unfortunate, and happens, but the caller is too early in the protocol or a demo
	// cannot identify the correct server count
	// return the same count that demo will use
	return gHostSpawnCount;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SpewInstallStatus( void )
{
	if ( IsGameConsole() && g_pFullFileSystem )
	{
#if defined( _X360 )
		Msg( "\nXbox Launched From %s.\n", g_pFullFileSystem->IsLaunchedFromXboxHDD() ? "HDD" :  "DVD" );
		if ( g_pXboxInstaller )
		{
			g_pXboxInstaller->SpewStatus();
		}
#elif defined( _PS3 )
		// This is intended to match CXboxInstaller::SpewStatus as closely as possible:
		Msg( "Install Status:\n" );
		Msg( "Version: %u (%s) (ps3)\n", XBX_GetImageChangelist(), XBX_GetLanguageString() );
		Msg( "DVD Hosted: Disabled\n" );
		// This spew is XBox-specific (could be hooked up to the FIOS installer)
		//if ( g_pFullFileSystem->IsInstalledToXboxHDDCache() )
		//{
		//	Msg( "Existing Image Found.\n" );
		//}
		//if ( !IsInstallEnabled() )
		//{
		//	Msg( "Install Enabled.\n" );
		//}
		//if ( IsFullyInstalled() )
		//{
		//	Msg( "Fully Installed.\n" );
		//}
		//Msg( "Progress: %d/%d MB\n", GetCopyStats()->m_BytesCopied/(1024*1024), GetTotalSize()/(1024*1024) );
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Host_PostInit()
{
	if ( serverGameDLL )
	{
		serverGameDLL->PostInit();
	}

#if !defined( DEDICATED )
	if ( g_ClientDLL )
	{
		g_ClientDLL->PostInit();
	}

	toolframework->PostInit();

	if ( !sv.IsDedicated() )
	{
		// vgui needs other systems to finalize
		EngineVGui()->PostInit();
	}

	if ( serverGameDLL )
	{
		serverGameDLL->PostToolsInit();
	}

	SpewInstallStatus();

	if ( IsGameConsole() )
	{
		// fully setup, can now publish cvars to vxconsole
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "getcvars" );
	}
#endif
}

#if defined( REPLAY_ENABLED )
void Replay_Init()
{
	Assert( replay == NULL );
}

void Replay_Shutdown()
{
	if ( replay )
	{
		replay->Shutdown();
		delete replay;
		replay = NULL;
	}
}
#endif

void HLTV_Init()
{
	for ( int i = 0; i < HLTV_SERVER_MAX_COUNT; ++i )
		Assert ( g_pHltvServer[ i ] == NULL );
	Assert ( hltvtest == NULL );
}

void HLTV_Shutdown()
{
	for ( int i = 0; i < HLTV_SERVER_MAX_COUNT; ++i )
	{
		if ( g_pHltvServer[ i ] )
		{
			g_pHltvServer[ i ]->Shutdown();
			delete g_pHltvServer[ i ];
			g_pHltvServer[ i ] = NULL;
		}
	}

	if ( hltvtest )
	{
		delete hltvtest;
		hltvtest = NULL;
	}
}

void InstallConVarHook( void );

int g_nForkID = 0;
int g_nSocketToParentProcess = -1;

#ifdef _LINUX
#include <sys/socket.h>
#include <sys/types.h>
void SendStringToParentProcess( char const *pMsg )
{
	Assert( IsChildProcess() );
	Assert( g_nSocketToParentProcess != -1 );
	send( g_nSocketToParentProcess, pMsg, 1 + strlen( pMsg ), MSG_DONTWAIT );
}
#endif


CDebugInputThread * g_pDebugInputThread;


//--------------------------------------------------------------------------------------------------
// PS3 QMS/Server thread
//--------------------------------------------------------------------------------------------------

#ifdef _PS3


static uint32 QMSServerThreadEntry(void* param)
{

	while(1)
	{
		// Wait on semaphore
		// For each semaphore posted, we look to see if we should run either "job"
		// It is also possible to decr the semaphore, only to have nothing to run...
		// ie The semaphore is a wakeup rather than a count of jobs

		sys_semaphore_wait(g_pGcmSharedData->m_semaphore, 0);

		g_pGcmSharedData->CheckForAudioRequest();

		g_pGcmSharedData->CheckForServerRequest();

		if (g_pGcmSharedData->m_qmsRunFlag)
		{
			g_pGcmSharedData->m_qmsRunFlag = 0;
			void* p1 = (void*)g_pGcmSharedData->m_cmat;
			void* p2 = (void*)g_pGcmSharedData->m_ptr;

			g_pGcmSharedData->m_func(p1, p2);
			g_pGcmSharedData->m_qmsDoneFlag = 1;
		}

	}

	return 0;

}

static void CreateQMSServerThread()
{

	CreateSimpleThread(QMSServerThreadEntry, 0, 0x40000);

}


#endif

// Check with steam to see if the requested file (requires full path) is a valid, signed binary
#define SignatureWarning( ... ) ((void)(0))
static bool Host_IsValidSignature( const char *pFilename, bool bAllowUnknown )
{
#if defined( DEDICATED ) || defined( _GAMECONSOLE )
	return true;
#else
	if ( Steam3Client().SteamUtils() )
	{
		SteamAPICall_t hAPICall = Steam3Client().SteamUtils()->CheckFileSignature( pFilename );
		bool bAPICallFailed = true;
		while ( !Steam3Client().SteamUtils()->IsAPICallCompleted(hAPICall, &bAPICallFailed) )
		{
			SteamAPI_RunCallbacks();
			ThreadSleep( 1 );
		}

		if( bAPICallFailed )
		{
			SignatureWarning( "CheckFileSignature API call on %s failed", pFilename );
		}
		else
		{
			CheckFileSignature_t result;
			Steam3Client().SteamUtils()->GetAPICallResult( hAPICall, &result, sizeof(result), result.k_iCallback, &bAPICallFailed );
			if( bAPICallFailed )
			{
				SignatureWarning( "CheckFileSignature API call on %s failed\n", pFilename );
			}
			else
			{
				if( result.m_eCheckFileSignature == k_ECheckFileSignatureValidSignature || result.m_eCheckFileSignature == k_ECheckFileSignatureNoSignaturesFoundForThisApp )
					return true;
				if ( bAllowUnknown && result.m_eCheckFileSignature == k_ECheckFileSignatureNoSignaturesFoundForThisFile )
					return true;
				SignatureWarning( "No valid signature found for %s\n", pFilename );
			}
		}
	}
	return false;
#endif
}


// Ask steam if it is ok to load this DLL.  Unsigned DLLs should not be loaded unless
// the client is running -insecure (testing a plugin for example)
// This keeps legitimate users with modified binaries from getting VAC banned because of them
#if defined( DEDICATED ) || defined( OSX ) || defined( LINUX )
#else
static CUtlVector< CUtlString * > g_PendingSignatureChecks;
static CUtlVector< CUtlString * > g_FailedSignatureChecks;
#endif
bool Host_AllowLoadModule( const char *pFilename, const char *pPathID, bool bAllowUnknown )
{
#if defined( DEDICATED ) || defined( OSX ) || defined( LINUX )
	// dedicated servers don't check signatures
	// OSX and Linux have no ability to check signatures
	return true;
#else
	// Allow loading plugins
	if ( sv.IsDedicated() )
		return true;

	// check signature
	bool bSignatureIsValid = false;

	if ( g_bAllowSecureServers && Steam3Client().SteamUtils() )
	{
		char szDllname[512];

		V_strncpy( szDllname, pFilename, sizeof(szDllname) );
		V_SetExtension( szDllname, DLL_EXT_STRING, sizeof(szDllname) );
		if ( pPathID )
		{

			char szFullPath[ 512 ];
			const char *pFullPath = g_pFileSystem->RelativePathToFullPath( szDllname, pPathID, szFullPath, sizeof(szFullPath) );
			if ( !pFullPath )
			{
				SignatureWarning("Can't find %s on disk\n", szDllname );
				bSignatureIsValid = false;
			}
			else
			{
				if ( CommandLine()->FindParm( "-immediatesignaturechecks" ) )
				{
					bSignatureIsValid = Host_IsValidSignature( pFullPath, bAllowUnknown );
				}
				else
				{
					g_PendingSignatureChecks.AddToTail( new CUtlString( pFullPath ) );
					bSignatureIsValid = true;
				}
			}
		}
		else
		{
			if ( CommandLine()->FindParm( "-immediatesignaturechecks" ) )
			{
				bSignatureIsValid = Host_IsValidSignature( szDllname, bAllowUnknown );
			}
			else
			{
				g_PendingSignatureChecks.AddToTail( new CUtlString( szDllname ) );
				bSignatureIsValid = true;
			}
		}
	}
	else
	{
		if ( g_bAllowSecureServers )
		{
			SignatureWarning("Steam is not active, running in -insecure mode.\n");
		}
		g_bAllowSecureServers = false;
	}

	if ( bSignatureIsValid )
		return true;

	if ( !g_bAllowSecureServers )
	{
		SignatureWarning("Loading unsigned module %s\nAccess to secure servers is disabled.\n", pFilename );
		return true;
	}
	return false;
#endif
}

bool Host_IsSecureServerAllowed()
{
	if ( CommandLine()->FindParm("-insecure") || CommandLine()->FindParm("-tools") )
		g_bAllowSecureServers = false;

	return g_bAllowSecureServers;
}

void Host_DisallowSecureServers()
{
#if !defined(DEDICATED)
	if ( g_bAllowSecureServers && ( GetBaseLocalClient().IsConnected() || sv.IsActive() ) )
	{	// Force-disconnect local client if secure mode changes mid-connection
		GetBaseLocalClient().Disconnect( true );
	}

	g_bAllowSecureServers = false;
#endif
}

void Host_FinishSecureSignatureChecks()
{
#if defined( DEDICATED ) || defined( OSX ) || defined( LINUX )
#else
	while ( g_PendingSignatureChecks.Count() )
	{
		// CSGO May 2017 checking via library loader hook
		// bool bSignatureIsValid = Host_IsValidSignature( g_PendingSignatureChecks.Head()->Get(), false );
		bool bSignatureIsValid = true;
		if ( !bSignatureIsValid )
		{
			SignatureWarning( "Loaded unsigned module %s\nAccess to secure servers is disabled.\n", g_PendingSignatureChecks.Head()->Get() );
			g_bAllowSecureServers = false;
			g_FailedSignatureChecks.AddToTail( new CUtlString( g_PendingSignatureChecks.Head()->Get() ) );
		}
		delete g_PendingSignatureChecks.Head();
		g_PendingSignatureChecks.FastRemove( 0 );
	}
	g_PendingSignatureChecks.PurgeAndDeleteElements();

	// Also check the main .exe that is running us
	TCHAR tchBufExe[ MAX_PATH ] = {};
	DWORD dwResult = GetModuleFileName( NULL, tchBufExe, MAX_PATH );
	if ( dwResult > 0 && dwResult < MAX_PATH-1 )
	{
		bool bSignatureIsValid = Host_IsValidSignature( tchBufExe, false );
		if ( !bSignatureIsValid )
		{
			SignatureWarning( "Loaded unsigned module %s\nAccess to secure servers is disabled.\n", tchBufExe );
			g_bAllowSecureServers = false;
			g_FailedSignatureChecks.AddToTail( new CUtlString( tchBufExe ) );
		}
	}
	else
	{
		SignatureWarning( "Failed to determine exe module filename.\nAccess to secure servers is disabled.\n" );
		g_bAllowSecureServers = false;
		g_FailedSignatureChecks.AddToTail( new CUtlString( "" ) );
	}

//	if ( IScaleformSlotInitController *pCtrlr = g_ClientDLL->GetScaleformSlotInitController() )
//		pCtrlr->PassSignaturesArray( &g_FailedSignatureChecks );
#endif
}

#if !defined( DEDICATED ) && defined( INCLUDE_SCALEFORM )
class CScaleformSlotInitControllerEngineImpl : public IScaleformSlotInitController
{
public:
	// A new slot has been created and InitSlot almost finished, perform final configuration
	virtual void ConfigureNewSlotPostInit( int slot )
	{
	};

	// Notification to external systems that a file was loaded by Scaleform libraries
	virtual bool OnFileLoadedByScaleform( char const *pszFilename, void *pvBuffer, int numBytesLoaded )
	{
		Host_DisallowSecureServers();
		return false;
	}

	virtual const void * GetStringUserData( const char * pchStringTableName, const char * pchKeyName, int * pLength )
	{
		int numTables = GetBaseLocalClient().m_StringTableContainer->GetNumTables();

		CNetworkStringTable *pTable = NULL;

		for ( int i = 0; i < numTables; i++ )
		{
			// iterate through server tables
			pTable = ( CNetworkStringTable* )GetBaseLocalClient().m_StringTableContainer->GetTable( i );

			if ( !pTable )
				continue;

			if ( !V_strcmp( pTable->GetTableName(), pchStringTableName ) )
				break;
		}


		if ( pTable )
		{
			int index = pTable->FindStringIndex( pchKeyName );
			if ( index != ::INVALID_STRING_INDEX )
			{
				return pTable->GetStringUserData( index, pLength );
			}
		}

		return NULL;
	}

	virtual void PassSignaturesArray( void *pvArray )
	{
		;
	}
}
g_CScaleformSlotInitControllerEngineImpl;
IScaleformSlotInitController *g_pIScaleformSlotInitControllerEngineImpl = &g_CScaleformSlotInitControllerEngineImpl;
#endif

bool Should360EmulatePS3()
{
	return ( IsX360() && CommandLine()->FindParm( "-ps3" ) ); 
}

static bool s_bDedicatedForPurposesOfThreadPool = false;

void GetThreadPoolStartParams( ThreadPoolStartParams_t &startParams )
{
	if ( IsX360() )
	{
		// 360 overrides defaults, 2 computation threads distributed to core 1 and 2
		if ( !Should360EmulatePS3() )
		{
			startParams.nThreads = 2;
			startParams.nStackSize = 256 * 1024;
			startParams.fDistribute = TRS_TRUE;
			startParams.bUseAffinityTable = true;
			startParams.iAffinityTable[ 0 ] = XBOX_PROCESSOR_2;
			startParams.iAffinityTable[ 1 ] = XBOX_PROCESSOR_4;
		}
		else
		{
			startParams.nThreads = 1;
			startParams.nStackSize = 256 * 1024;
			startParams.fDistribute = TRS_TRUE;
			startParams.bUseAffinityTable = true;
			startParams.iAffinityTable[ 0 ] = XBOX_PROCESSOR_1;

			ConVarRef cl_threaded_bone_setup( "cl_threaded_bone_setup" );
			host_threaded_sound.SetValue( 0 );
			cl_threaded_bone_setup.SetValue( 2 );
			host_thread_mode.SetValue( 0 );
			Cbuf_AddText( Cbuf_GetCurrentPlayer(), "cache_gimp\n" );
			g_nMaterialSystemThread = 0;
		}
		ThreadSetAffinity( NULL, 1 );
	}

	// Dedicated servers should not explicitly set the main thread's affinity so that machines running multiple 
	// copies of the dedicated server can load-balance properly. 
	// For now on the PC we use SetThreadIdealProcessor instead of explicity affinity
	if ( !s_bDedicatedForPurposesOfThreadPool && IsPC() )
	{
		// this will set ideal processor on each thread
		startParams.fDistribute = TRS_TRUE;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Host_Init( bool bDedicated )
{
	realtime = 0;
	host_idealtime = 0;

	// this code is called prefork. If we are going to fork, we want to delay some stuff, because we might not even be acting as a "real" server.
#ifdef _LINUX
	if ( CommandLine()->FindParm( "-fork" ) )
	{
		g_nForkID = FORK_ID_PARENT_PROCESS;
	}
#endif

#if defined(_WIN32)
	if ( CommandLine()->FindParm( "-pme" ) )
	{
		s_bInitPME = true;
	}
#endif

	if ( !bDedicated && Host_IsSecureServerAllowed() )
	{
		//
		// Signature checking code:
		//

	/** Removed for partner depot **/
	}

	ThreadPoolStartParams_t startParams;
	s_bDedicatedForPurposesOfThreadPool = bDedicated;
	GetThreadPoolStartParams( startParams );
#ifdef _PS3	
	{
		// PS3 overrides defaults, 
		if ( host_threaded_sound.GetBool() && ( !host_threaded_sound_simplethread.GetBool() ) )
		{
			startParams.nThreads = 1;
		}
		else
		{
			startParams.nThreads = 0;
		}

		// Second thread for CSGO, which is not a jobthread, so that we can more easily control "what runs where and whem"
		g_pGcmSharedData->Init();
		CreateQMSServerThread();
	}
#endif

	//////// DISABLE FOR SHIP! //////////
	if ( !IsCert() || CommandLine()->FindParm( "-dbginput" ) )
	{
		g_pDebugInputThread = new CDebugInputThread();
		g_pDebugInputThread->SetName( "Debug Input" );
		g_pDebugInputThread->Start( 0, TP_PRIORITY_HIGH );
	}

#ifndef _CERT
	if ( CommandLine()->FindParm( "-tslist" ) )
	{
		int nTests = 10000;
		Msg( "Running TSList tests\n" );
		RunTSListTests( nTests );
		Msg( "Running TSQueue tests\n" );
		RunTSQueueTests( nTests );
		Msg( "Running Thread Pool tests\n" );
		RunThreadPoolTests();
	}
#endif
	if ( g_pThreadPool )
	{
		g_pThreadPool->Start( startParams );
#ifdef _X360
		if ( !Should360EmulatePS3() )
		{
			g_pAlternateThreadPool = CreateNewThreadPool();
			startParams.iAffinityTable[0] = XBOX_PROCESSOR_3;
			startParams.iAffinityTable[1] = XBOX_PROCESSOR_5;
			g_pAlternateThreadPool->Start( startParams );
		}
#endif
	}

	// From const.h, the loaded game .dll will give us the correct value which is transmitted to the client
	host_state.interval_per_tick = DEFAULT_TICK_INTERVAL;

	InstallBitBufErrorHandler();

	InstallConVarHook();

	TRACEINIT( Con_Init(), Con_Shutdown() );

	TRACEINIT( Memory_Init(), Memory_Shutdown() );

	TRACEINIT( Cbuf_Init(), Cbuf_Shutdown() );

	TRACEINIT( Cmd_Init(), Cmd_Shutdown() );	

	TRACEINIT( g_pCVar->Init(), g_pCVar->Shutdown() ); // So we can list cvars with "cvarlst"

#ifndef DEDICATED
	TRACEINIT( V_Init(), V_Shutdown() );
#endif

	//TRACEINIT( Host_InitVCR(), Host_ShutdownVCR() );

	TRACEINIT( COM_Init(), COM_Shutdown() );

#if !defined(DEDICATED) && !defined(LEFT4DEAD)
	TRACEINIT( saverestore->Init(), saverestore->Shutdown() );
#endif

	TRACEINIT( Filter_Init(), Filter_Shutdown() );

#ifndef DEDICATED
	TRACEINIT( Key_Init(), Key_Shutdown() );
#endif

	// Check for special -dev flag
	if ( CommandLine()->FindParm( "-dev" ) || ( CommandLine()->FindParm( "-allowdebug" ) && !CommandLine()->FindParm( "-nodev" ) ) )
	{
		sv_cheats.SetValue( 1 );
		developer.SetValue( 1 );
	}
#ifdef _DEBUG
	developer.SetValue( 1 );
#endif

	if ( CommandLine()->FindParm( "-nocrashdialog" ) )
	{
		// stop the various windows error message boxes from showing up (used by the auto-builder so it doesn't block on error) 
		Sys_NoCrashDialog();
	}

	// Seed the random number generator
	float flTimeToSeed = Plat_FloatTime();
	COMPILE_TIME_ASSERT( sizeof( flTimeToSeed ) == sizeof( int ) );
	RandomSeed( ( reinterpret_cast< int & >( flTimeToSeed ) ) & 0x7FFFFFFF );
	DevMsg( "Seeded random number generator @ %d ( %.3f )\n", ( reinterpret_cast< int & >( flTimeToSeed ) ) & 0x7FFFFFFF, flTimeToSeed );

	TRACEINIT( g_pSteamSocketMgr->Init(), g_pSteamSocketMgr->Shutdown() );

	if ( CommandLine()->FindParm( "-xlsp" ) != 0 )
	{
		TRACEINIT( NET_Init( bDedicated ), NET_Shutdown() );
	}

	TRACEINIT( g_GameEventManager.Init(), g_GameEventManager.Shutdown() );

	TRACEINIT( sv.Init( bDedicated ), sv.Shutdown() );

	if ( !CommandLine()->FindParm( "-nogamedll" ) )
	{
		SV_InitGameDLL();
	}

	TRACEINIT( g_Log.Init(), g_Log.Shutdown() );

	TRACEINIT( HLTV_Init(), HLTV_Shutdown() );

#if defined( REPLAY_ENABLED )
	TRACEINIT( Replay_Init(), Replay_Shutdown() );
#endif
	ConDMsg( "Heap: %5.2f Mb\n", host_parms.memsize/(1024.0f*1024.0f) );

	// Deal with Gore Settings
	Host_CheckGore();

#if !defined( DEDICATED )
	if ( !bDedicated )
	{
		TRACEINIT( CL_Init(), CL_Shutdown() );

		// NOTE: This depends on the mod search path being set up
		TRACEINIT( InitMaterialSystem(), ShutdownMaterialSystem() );

#if defined( INCLUDE_SCALEFORM )
		extern IScaleformSlotInitController *g_pIScaleformSlotInitControllerEngineImpl;
		TRACEINIT( ScaleformInitFullScreenAndCursor(g_pScaleformUI, g_szDefaultScaleformMovieName, g_szDefaultScaleformCursorName, g_pIScaleformSlotInitControllerEngineImpl ), ScaleformReleaseFullScreenAndCursor( g_pScaleformUI ) );
#endif

		TRACEINIT( modelloader->Init(), modelloader->Shutdown() );

		TRACEINIT( StaticPropMgr()->Init(), StaticPropMgr()->Shutdown() );

		TRACEINIT( InitStudioRender(), ShutdownStudioRender() );

		TRACEINIT( g_pMatchFramework->Init(), g_pMatchFramework->Shutdown() );

		//startup vgui
		TRACEINIT( EngineVGui()->Init(), EngineVGui()->Shutdown() );

		TRACEINIT( TextMessageInit(), TextMessageShutdown() );

		TRACEINIT( ClientDLL_Init(), ClientDLL_Shutdown() );

		TRACEINIT( SCR_Init(), SCR_Shutdown() );

		TRACEINIT( R_Init(), R_Shutdown() ); 

		TRACEINIT( Decal_Init(), Decal_Shutdown() );

		// hookup interfaces
		EngineVGui()->Connect();
	}
	else
#endif
	{
		TRACEINIT( InitMaterialSystem(), ShutdownMaterialSystem() );

		TRACEINIT( modelloader->Init(), modelloader->Shutdown() );

		TRACEINIT( StaticPropMgr()->Init(), StaticPropMgr()->Shutdown() );

		TRACEINIT( InitStudioRender(), ShutdownStudioRender() );

		TRACEINIT( g_pMatchFramework->Init(), g_pMatchFramework->Shutdown() );

		TRACEINIT( Decal_Init(), Decal_Shutdown() );

#ifndef DEDICATED
		GetBaseLocalClient().m_nSignonState = SIGNONSTATE_NONE; // disable client
#endif
	}

	if ( IsPC() )
	{
#if !defined(NO_STEAM)
		// on PC, enable FCVAR_DEVELOPMENTONLY cvars if we're logged into beta or dev universes.  They remain hidden & disabled otherwise.
		EUniverse eUniverse = GetSteamUniverse();
		if ( ( eUniverse == k_EUniverseBeta ) || ( eUniverse == k_EUniverseDev ) )
		{
			ConVarUtilities->EnableDevCvars();
		}		
#endif 
	}
	else if ( IsGameConsole() )
	{
		// on 360, enable FCVAR_DEVELOPMENTONLY cvars always.  No cvars are accessible to customers on X360.
		ConVarUtilities->EnableDevCvars();
	}

#ifndef DEDICATED
	memset( g_bConfigCfgExecuted, 0, sizeof(g_bConfigCfgExecuted) );

	Host_ReadConfiguration( -1, false );
	TRACEINIT( S_Init(), S_Shutdown() );

	// Audio system initializes after matchmaking, so need to explicitly
	// set the voice interface extension
	IEngineVoice *pIEngineVoice = NULL;
#ifdef _GAMECONSOLE
	pIEngineVoice = Audio_GetXVoice();
#elif ( defined( _WIN32 ) || defined( OSX ) || defined( LINUX ) ) && !defined( NO_STEAM )
	pIEngineVoice = Audio_GetEngineVoiceSteam();
#else
	pIEngineVoice = Audio_GetEngineVoiceStub();
#endif

	if ( !pIEngineVoice )
	{
		Warning( "Using IEngineVoice extension stub!\n" );
		pIEngineVoice = Audio_GetEngineVoiceStub();
	}
	g_pMatchFramework->GetMatchExtensions()->RegisterExtensionInterface(
		IENGINEVOICE_INTERFACE_VERSION, pIEngineVoice );

	if (vaudio)
	{
		//Pin miles sound system as loaded until Host_Shutdown() is caused. This prevents any
		//possible path where MSS could unload and then somehow be reloaded.
		g_pMilesAudioEngineRef = vaudio->CreateMilesAudioEngine();
	}
#endif

	// Execute valve.rc
#ifdef _GAMECONSOLE
	// the 360 version loads a 3d background
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), "exec valve.rc\n" );
#else
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), "exec valve.rc\n" );
#endif

	// Execute mod-specfic settings, without falling back based on search path.
	// This lets us set overrides for games while letting mods of those games
	// use the default settings.
	if ( g_pFileSystem->FileExists( "//mod/cfg/modsettings.cfg" ) )
	{
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "exec modsettings.cfg mod\n" );
	}

#ifdef _GAMECONSOLE
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), "cmd2 exec config" PLATFORM_EXT ".cfg\n" );
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), "cmd1 exec config" PLATFORM_EXT ".cfg\n" );
#endif

	// Mark DLL as active
	//	eng->SetNextState( InEditMode() ? IEngine::DLL_PAUSED : IEngine::DLL_ACTIVE );

	TelemetryTick();

	// Initialize processor subsystem, and print relevant information:
	Host_InitProcessor();

	// Mark hunklevel at end of startup
	Hunk_AllocName( 0, "-HOST_HUNKLEVEL-" );
	host_hunklevel = Hunk_LowMark();

#ifdef SOURCE_MT
	if ( CommandLine()->FindParm( "-swapcores" ) && !IsPS3() )
	{
		g_nMaterialSystemThread = 1;
		g_nServerThread = 0;
	}
#endif

	Host_AllowQueuedMaterialSystem( false );

	// Finished initializing
	host_initialized = true;

	host_checkheap = CommandLine()->FindParm( "-heapcheck" ) ? true : false;

	if ( host_checkheap )
	{
#if defined( _WIN32 )
		if ( _heapchk() != _HEAPOK )
		{
			Sys_Error( "Host_Init:  _heapchk() != _HEAPOK\n" );
		}
#endif
	}

	// go directly to run state with no active game
	HostState_Init();

	// check for reslist generation
	if ( CommandLine()->FindParm( "-makereslists" ) )
	{
		MapReslistGenerator().StartReslistGeneration();
	}

	// check for devshot generation
	if ( CommandLine()->FindParm( "-makedevshots" ) )
	{
		DevShotGenerator().StartDevShotGeneration();
	}

	// if running outside of steam and NOT a dedicated server then phone home (or if "-phonehome" is passed on the command line)
	if ( !sv.IsDedicated() || CommandLine()->FindParm( "-phonehome" ) )
	{
		// In debug, only run this check if -phonehome is on the command line (so a debug build will "just work").
		if ( IsDebug() && CommandLine()->FindParm( "-phonehome" ) )
		{
			phonehome->Init();
			phonehome->Message( IPhoneHome::PHONE_MSG_ENGINESTART, NULL );
		}
	}

	Host_PostInit();
	EndLoadingUpdates();

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->SetNonInteractiveTempFullscreenBuffer( NULL, MATERIAL_NON_INTERACTIVE_MODE_STARTUP );
	pRenderContext->SetNonInteractivePacifierTexture( NULL, 0, 0, 0 );
	pRenderContext->SetNonInteractiveLogoTexture( NULL, 0, 0, 0, 0 );

	// disable future render target allocation
	g_pMaterialSystem->FinishRenderTargetAllocation();

	if ( CommandLine()->FindParm( "-profileinit" ) )
	{
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "quit\n" );
	}

	if ( !bDedicated && Host_IsSecureServerAllowed() )
	{
		Host_FinishSecureSignatureChecks();
	}

#if !defined(DEDICATED)
	if ( g_pMatchFramework && !Host_IsSecureServerAllowed() )
	{
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnClientInsecureBlocked", "reason", "init" ) );
	}
#endif

	// Official valve servers must be running with a certificate
	if ( CommandLine()->FindParm( "-certificate" ) ||
		( serverGameDLL && serverGameDLL->IsValveDS() && !CommandLine()->FindParm( "-ignore_certificate_valveds" ) ) )
	{
		const byte *pbNetEncryptPrivateKey = NULL;
		int cbNetEncryptPrivateKey = 0;
		bool bHasPrivateKey =
			NET_CryptGetNetworkCertificate( k_ENetworkCertificate_PublicKey, &pbNetEncryptPrivateKey, &cbNetEncryptPrivateKey ) &&
			NET_CryptGetNetworkCertificate( k_ENetworkCertificate_Signature, &pbNetEncryptPrivateKey, &cbNetEncryptPrivateKey ) &&
			NET_CryptGetNetworkCertificate( k_ENetworkCertificate_PrivateKey, &pbNetEncryptPrivateKey, &cbNetEncryptPrivateKey );
		if ( !bHasPrivateKey || !pbNetEncryptPrivateKey || !cbNetEncryptPrivateKey )
		{
			Warning( "NET_CryptGetNetworkCertificate is missing server certificates!\n" );
			Plat_ExitProcess( 0 );
		}
	}
}

class CAddTransitionResourcesCB : public ISaveRestoreDataCallback
{
public:
	CAddTransitionResourcesCB( const char *pLevelName, const char *pLandmarkName )
	 : m_pLevelName( pLevelName ), m_pLandMarkName( pLandmarkName ) {}

	//-----------------------------------------------------------------------------
	// Adds hints to the loader to keep resources that are in the transition volume,
	// as they may not be part of the next map's reslist.
	//-----------------------------------------------------------------------------
	virtual void Execute( CSaveRestoreData *pSaveData )
	{
		if ( !pSaveData || IsPC() || ( g_pFileSystem->GetDVDMode() == DVDMODE_OFF ) )
		{
			return;
		}

		// get the bit marked for the next level
		int transitionMask = 0;
		for ( int i = 0; i < pSaveData->levelInfo.connectionCount; i++ )
		{
			if ( !Q_stricmp( m_pLevelName, pSaveData->levelInfo.levelList[i].mapName ) && !Q_stricmp( m_pLandMarkName, pSaveData->levelInfo.levelList[i].landmarkName ) )
			{
				transitionMask = 1<<i;
				break;
			}
		}
		
		if ( !transitionMask )
		{
			// nothing to do
			return;
		}

		const char *pModelName;
		bool bHasHumans = false;
		for ( int i = 0; i < pSaveData->NumEntities(); i++ )
		{
			if ( pSaveData->GetEntityInfo(i)->flags & transitionMask )
			{
				// this entity will cross the transition and needs to be preserved
				// add to the next map's resource list which effectively keeps it from being purged
				// only care about the actual mdl and not any of its dependants
				pModelName = pSaveData->GetEntityInfo(i)->modelname.ToCStr();
				g_pQueuedLoader->AddMapResource( pModelName );

				// humans require a post pass
				if ( !bHasHumans && V_stristr( pModelName, "models/humans" ) )
				{
					bHasHumans = true;
				}
			}
		}

		if ( bHasHumans )
		{
			// the presence of any human entity in the transition needs to ensure all the human mdls stay
			int count = modelloader->GetCount();
			for ( int i = 0; i < count; i++ )
			{
				pModelName = modelloader->GetName( modelloader->GetModelForIndex( i ) );
				if ( V_stristr( pModelName, "models/humans" ) )
				{
					g_pQueuedLoader->AddMapResource( pModelName );
				}
			}
		}
	}

	const char *m_pLevelName;
	const char *m_pLandMarkName;
};

// There's a version of this in bsplib.cpp!!!  Make sure that they match.
void GetPlatformMapPath( const char *pMapPath, char *pPlatformMapPath, int maxLength )
{
	Q_strncpy( pPlatformMapPath, pMapPath, maxLength );

	// It's OK for this to be NULL on the dedicated server.
	if( g_pMaterialSystemHardwareConfig )
	{
		Q_StripExtension( pMapPath, pPlatformMapPath, maxLength );
		Q_strncat( pPlatformMapPath, ".bsp", maxLength, COPY_ALL_CHARACTERS );
	}
}


void AdjustThreadPoolThreadCount()
{
#if defined( DEDICATED ) && IsPlatformLinux()
	extern ConVar occlusion_test_async;
	int nTargetThreads = occlusion_test_async.GetInt();
	if ( nTargetThreads != g_pThreadPool->NumThreads() )
	{
		uint64 nTickStart = GetTimebaseRegister();
		Msg( "Stopping %d worker threads\n", g_pThreadPool->NumThreads() );
		g_pThreadPool->ExecuteAll();
		g_pThreadPool->Stop();

		if ( nTargetThreads != 0 )
		{
			Msg( "Starting %d worker threads\n", nTargetThreads);
			ThreadPoolStartParams_t startParams;
			GetThreadPoolStartParams( startParams );
			startParams.nThreads = Max( 1, Min( 4, nTargetThreads ) );
			startParams.bEnableOnLinuxDedicatedServer = true; // if we run with -threads parameter, we should also enable the thread pool
			if ( g_pThreadPool )
			{
				g_pThreadPool->Start( startParams );
			}
		}
		uint64 nTickEnd = GetTimebaseRegister();
		Msg( "%d threads. %s ticks\n", g_pThreadPool->NumThreads(), V_pretifynum( nTickEnd - nTickStart ) );
		int nCmdLineThreads = CommandLine()->ParmValue( "-threads", -1 );
		if ( nCmdLineThreads >= 0 )
		{
			Msg( "Note that cmd line -threads %d is specified\n", nCmdLineThreads );
		}
	}
#endif
}

void Host_Changelevel( bool loadfromsavedgame, const char *mapname, char *mapGroupName, const char *start )
{
	char			level[ MAX_QPATH ];
	char			_startspot[ MAX_QPATH ];
	char			*startspot;
	char			oldlevel[ MAX_QPATH ];
#if !defined(DEDICATED)
	bool bTransitionBySave = false;
#endif

	if ( !sv.IsActive() )
	{
		ConMsg( "Only the server may changelevel\n" );
		return;
	}

#ifndef DEDICATED
	// FIXME:  Even needed?
	if ( demoplayer->IsPlayingBack() )
	{
		ConMsg( "Changelevel invalid during demo playback\n" );
		return;
	}

	if ( !sv.IsDedicated() )
	{
		EngineVGui()->SetProgressLevelName( mapname );
	}
	SCR_BeginLoadingPlaque( mapname );
#endif

	g_pFileSystem->AsyncFinishAll();

#if !defined DEDICATED
	// stop sounds (especially looping!)
	S_StopAllSounds( true );
#endif

	char dxMapName[ MAX_PATH ];
	GetPlatformMapPath( mapname, dxMapName, MAX_PATH );
	host_map.SetValue( dxMapName );

	Q_strncpy( level, mapname, sizeof( level ) );
	if ( !start )
		startspot = NULL;
	else
	{
		Q_strncpy( _startspot, start, sizeof( _startspot ) );
		startspot = _startspot;
	}

	Warning( "---- Host_Changelevel ----\n" );
	SV_CheckForFlushMemory( sv.GetMapName(), mapname );

	if ( IsX360() )
	{
		// Reset material system temporary memory (frees up memory for map loading)
		materials->ResetTempHWMemory( true );
	}

	materials->OnLevelShutdown();

#if !defined( DEDICATED )
	// Always save as an xsave if we're on the xbox
	saverestore->SetIsXSave( IsX360() );

	// Add on time passed since the last time we kept track till this transition
	int iAdditionalSeconds = g_ServerGlobalVariables.curtime - saverestore->GetMostRecentElapsedTimeSet();
	int iElapsedSeconds = saverestore->GetMostRecentElapsedSeconds() + iAdditionalSeconds;
	int iElapsedMinutes = saverestore->GetMostRecentElapsedMinutes() + ( iElapsedSeconds / 60 );
	saverestore->SetMostRecentElapsedMinutes( iElapsedMinutes );
	saverestore->SetMostRecentElapsedSeconds( ( iElapsedSeconds % 60 ) );

	KeyValues *kvChangelevelEvent = new KeyValues( "OnHostChangeLevel" );
	kvChangelevelEvent->SetString( "map", mapname );
	kvChangelevelEvent->SetUint64( "elapsed", iElapsedMinutes * 60 + iElapsedSeconds % 60 );
	kvChangelevelEvent->SetInt( "bysave", !!bTransitionBySave );
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( kvChangelevelEvent );

	if ( bTransitionBySave )
	{
		char comment[ 80 ];
		// Pass in the total elapsed time so it gets added to the elapsed time for this map.
		serverGameDLL->GetSaveComment(
			comment,
			sizeof( comment ),
			saverestore->GetMostRecentElapsedMinutes(),
			saverestore->GetMostRecentElapsedSeconds() );

		if ( !saverestore->SaveGameSlot( "_transition", comment, false, true, mapname, startspot ) )
		{
			Warning( "Failed to save data for transition\n" );
			SCR_EndLoadingPlaque();
			return;
		}

		// Not going to load a save after the transition, so add this map's elapsed time to the total elapsed time
		int totalSeconds = g_ServerGlobalVariables.curtime + saverestore->GetMostRecentElapsedSeconds();
		saverestore->SetMostRecentElapsedMinutes( ( int )( totalSeconds / 60.0f ) + saverestore->GetMostRecentElapsedMinutes() );
		saverestore->SetMostRecentElapsedSeconds( ( int )fmod( totalSeconds, 60.0f ) );
	}
#endif

	Q_strncpy( oldlevel, sv.GetMapName(), sizeof( oldlevel ) );

#if !defined(DEDICATED)
	if ( loadfromsavedgame )
	{
		if ( !bTransitionBySave )
		{
			// This callback ensures resources in the transition volume stay
			CAddTransitionResourcesCB addTransitionResources( level, startspot );

			// save the current level's state
			if ( !saverestore->SaveGameState( true, &addTransitionResources ) )
			{
				Warning( "Failed to save data for transition\n" );
				SCR_EndLoadingPlaque();
				return;
			}
		}
	}
#endif
	g_pServerPluginHandler->LevelShutdown();

#if !defined(DEDICATED)
	audiosourcecache->LevelShutdown();
#endif

	sv.InactivateClients();

#if !defined(DEDICATED)
	saverestore->FinishAsyncSave();
#endif

	if ( sv.RestartOnLevelChange() )
	{
		Cbuf_Clear( Cbuf_GetCurrentPlayer() );
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "quit\n" );
		return;
	}

	DownloadListGenerator().OnLevelLoadStart( level );

	Msg( "*** Map Load: %s: Map Group %s", level, mapGroupName );
	if ( !sv.SpawnServer( level, mapGroupName, startspot ) )
	{
		return;
	}

#ifndef DEDICATED
	if ( loadfromsavedgame )
	{
		g_ServerGlobalVariables.curtime = sv.GetTime();

		audiosourcecache->LevelInit( level );
		g_pServerPluginHandler->LevelInit( level, CM_EntityString(), oldlevel, startspot, true, false );

		sv.SetPaused( true ); // pause until client connects
		sv.m_bLoadgame = true;
	}
	else
#endif
	{
		g_ServerGlobalVariables.curtime = sv.GetTime();
#if !defined(DEDICATED)
		audiosourcecache->LevelInit( level );
#endif
		g_pServerPluginHandler->LevelInit( level, CM_EntityString(), NULL, NULL, false, false );
	}

	SV_ActivateServer();

#if !defined(DEDICATED)
	// Offset stored elapsed time by the current elapsed time for this new map
	int maptime = sv.GetTime();
	int minutes = ( int )( maptime / 60.0f );
	int seconds = ( int )fmod( maptime, 60.0f );
	saverestore->SetMostRecentElapsedMinutes( saverestore->GetMostRecentElapsedMinutes() - minutes );
	saverestore->SetMostRecentElapsedSeconds( saverestore->GetMostRecentElapsedSeconds() - seconds );
	saverestore->ForgetRecentSave();
#endif

	NotifyDedicatedServerUI( "UpdateMap" );

	DownloadListGenerator().OnLevelLoadEnd();

	AdjustThreadPoolThreadCount();
}



/*
===============================================================================

SERVER TRANSITIONS

===============================================================================
*/
bool Host_NewGame( char *mapName, char *mapGroupName, bool loadGame, bool bBackgroundLevel, bool bSplitScreenConnect, const char *pszOldMap, const char *pszLandmark )
{
	VPROF( "Host_NewGame" );
	COM_TimestampedLog( "Host_NewGame" );

	// reset some convars if loading background map
	if ( bBackgroundLevel )
		ResetGameConVarsToDefaults();

	char previousMapName[MAX_PATH];
	char dxMapName[MAX_PATH];
	V_FixSlashes( mapName, '/' );
	GetPlatformMapPath( mapName, dxMapName, MAX_PATH );
	Q_strncpy( previousMapName, host_map.GetString(), sizeof( previousMapName ) );
	host_map.SetValue( dxMapName );

	// Setup gamemode based on the settings the map was started with
	sv.ExecGameTypeCfg( mapName );

#ifndef DEDICATED
	if ( !sv.IsDedicated() )
	{
		EngineVGui()->SetProgressLevelName( mapName );
	}
	SCR_BeginLoadingPlaque( mapName );
#endif

	Warning( "---- Host_NewGame ----\n" );
	SV_CheckForFlushMemory( previousMapName, mapName );

	if ( IsX360() )
	{
		// Reset material system temporary memory (frees up memory for map loading)
		materials->ResetTempHWMemory( true );
	}

	materials->OnLevelShutdown();

	MapReslistGenerator().OnLevelLoadStart(mapName);
	DownloadListGenerator().OnLevelLoadStart(mapName);

	if ( !loadGame )
	{
		VPROF( "Host_NewGame_HostState_RunGameInit" );
		HostState_RunGameInit();
	}

	// init network mode
	VPROF_SCOPE_BEGIN( "Host_NewGame_SpawnServer" );

	NET_SetMultiplayer( sv.IsMultiplayer() );
	if ( !sv.IsMultiplayer() )
		NET_SetMultiplayer( ( g_pMatchFramework->GetMatchTitle()->GetTitleSettingsFlags() & MATCHTITLE_SETTING_MULTIPLAYER ) != 0 );

	NET_ListenSocket( sv.m_Socket, true );	// activated server TCP socket

	// let's not have any servers with no name
	Host_EnsureHostNameSet();

	COM_TimestampedLog( "*** Map Load: %s Map %s Group", mapName, mapGroupName );
	HostState_Pre_LoadMapIntoMemory(); // A map is about to be loaded into memory
	if ( !sv.SpawnServer ( mapName, mapGroupName, NULL ) )
	{
		HostState_Post_FlushMapFromMemory(); // Map load failed, no impact on memory
		return false;
	}

	sv.m_bIsLevelMainMenuBackground = bBackgroundLevel;
	ConColorMsg( Color( 0, 255, 0 ), "Host_NewGame on map %s%s\n",
		mapName, sv.IsLevelMainMenuBackground() ? " (background map)" : "" );

	VPROF_SCOPE_END();

	// make sure the time is set
	g_ServerGlobalVariables.curtime = sv.GetTime();

	COM_TimestampedLog( "serverGameDLL->LevelInit" );

#ifndef DEDICATED
	EngineVGui()->UpdateProgressBar(PROGRESS_LEVELINIT);

	audiosourcecache->LevelInit( mapName );
#endif

	g_pServerPluginHandler->LevelInit( mapName, CM_EntityString(), pszOldMap, pszLandmark, loadGame, bBackgroundLevel );

	if ( loadGame )
	{
		sv.SetPaused( true );		// pause until all clients connect
		sv.m_bLoadgame = true;
		g_ServerGlobalVariables.curtime = sv.GetTime();
	}

	if( !SV_ActivateServer() )
	{
		return false;
	}

	// Connect the local client when a "map" command is issued.
	if ( !sv.IsDedicated() )
	{
		COM_TimestampedLog( "Stuff 'connect localhost' to console" );

		int nNumPlayers = 1;

		if( IsGameConsole() && !bBackgroundLevel )
		{
#ifdef _GAMECONSOLE
			// on the 360 we need to ask matchmaking how many players to connect.
			nNumPlayers = XBX_GetNumGameUsers();
#endif
		}
		
		if ( bSplitScreenConnect )
		{
			Assert( !bBackgroundLevel );
			nNumPlayers = host_state.max_splitscreen_players;
		}

		char str[512];
		if ( nNumPlayers > 1 )
		{
			Q_snprintf( str, sizeof( str ), "connect_splitscreen localhost:%d %d", sv.GetUDPPort(), nNumPlayers );
		}
		else
		{
			Q_snprintf( str, sizeof( str ), "connect localhost:%d", sv.GetUDPPort() );
		}
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), str );
	}
	else
	{
		// Dedicated server triggers map load here.
		GetTestScriptMgr()->CheckPoint( "FinishedMapLoad" );
	}

#ifndef DEDICATED
	if ( !loadGame )
	{
		// clear the most recent remember save, so the level will just restart if the player dies
		saverestore->ForgetRecentSave();
	}

	saverestore->SetMostRecentElapsedMinutes( 0 );
	saverestore->SetMostRecentElapsedSeconds( 0 );
#endif


	if (MapReslistGenerator().IsEnabled())
	{
		MapReslistGenerator().OnLevelLoadEnd();
	}
	DownloadListGenerator().OnLevelLoadEnd();
	return true;
}

void Host_FreeStateAndWorld( bool server )
{
	Assert( host_initialized );
	Assert( host_hunklevel );

	// If called by the client and we are running a listen server, just ignore
	if ( !server && sv.IsActive() )
		return;

	// HACKHACK: You can't clear the hunk unless the client data is free
	// since this gets called by the server, it's necessary to wipe the client
	// in case we are on a listen server
#ifndef DEDICATED
	if ( server && !sv.IsDedicated() )
	{
		CL_ClearState();
	}
#endif

	// The world model relies on the low hunk, so we need to force it to unload
	if ( host_state.worldmodel )
	{
		modelloader->UnreferenceModel( host_state.worldmodel, IModelLoader::FMODELLOADER_SERVER );
		modelloader->UnreferenceModel( host_state.worldmodel, IModelLoader::FMODELLOADER_CLIENT );
		host_state.SetWorldModel( NULL );
	}

	modelloader->UnloadUnreferencedModels();

	modelloader->UnMountCompatibilityPaths();

	g_TimeLastMemTest = 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Host_FreeToLowMark( bool server )
{
	Assert( host_initialized );
	Assert( host_hunklevel );

	// If called by the client and we are running a listen server, just ignore
	if ( !server && ( sv.IsActive() || sv.IsLoading() ) )
		return;

	CM_FreeMap();

	if ( host_hunklevel )
	{
		// See if we are going to obliterate any malloc'd pointers
		Hunk_FreeToLowMark(host_hunklevel);
	}
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Host_Shutdown(void)
{
	if ( host_checkheap )
	{
#ifdef _WIN32
		if ( _heapchk() != _HEAPOK )
		{
			Sys_Error( "Host_Shutdown (top):  _heapchk() != _HEAPOK\n" );
		}
#endif
	}

	// Check for recursive shutdown, should never happen
	static bool shutting_down = false;
	if ( shutting_down )
	{
		Msg( "Recursive shutdown!!!\n" );
		return;
	}
	shutting_down = true;
	
	if( g_pDebugInputThread )
	{
		g_pDebugInputThread->Stop();
		delete g_pDebugInputThread;
	}

	phonehome->Message( IPhoneHome::PHONE_MSG_ENGINEEND, NULL );
	phonehome->Shutdown();

#ifndef DEDICATED
	// Store active configuration settings
	Host_WriteConfiguration( -1, "config.cfg" ); 
#endif

	// Disconnect from server
	Host_Disconnect(true);

#ifndef DEDICATED
	// keep ConMsg from trying to update the screen
	scr_disabled_for_loading = true;
#endif

#if defined VOICE_OVER_IP && !defined DEDICATED && !defined( NO_VOICE )
	Voice_Deinit();
#endif // VOICE_OVER_IP

	// TODO, Trace this
	CM_FreeMap();

	host_initialized = false;

#if defined(VPROF_ENABLED)
	VProfRecord_Shutdown();
#endif

#if !defined DEDICATED
	if ( !sv.IsDedicated() )
	{
		if (vaudio && g_pMilesAudioEngineRef)
		{
			//let miles sound system exit here.
			vaudio->DestroyMilesAudioEngine(g_pMilesAudioEngineRef);
			g_pMilesAudioEngineRef = nullptr;
		}

		TRACESHUTDOWN( Decal_Shutdown() );

		TRACESHUTDOWN( R_Shutdown() );

		TRACESHUTDOWN( SCR_Shutdown() );

		if ( g_pMatchFramework )
		{
			TRACESHUTDOWN( g_pMatchFramework->Shutdown() );
		}

		TRACESHUTDOWN( ClientDLL_Shutdown() );

		TRACESHUTDOWN( TextMessageShutdown() );

		TRACESHUTDOWN( EngineVGui()->Shutdown() );

#if defined( INCLUDE_SCALEFORM )
		TRACESHUTDOWN( ScaleformReleaseFullScreenAndCursor( g_pScaleformUI ) );
#endif

		TRACESHUTDOWN( S_Shutdown() );

		TRACESHUTDOWN( StaticPropMgr()->Shutdown() );

		// Model loader must shutdown before StudioRender
		// because it calls into StudioRender
		TRACESHUTDOWN( modelloader->Shutdown() );

		TRACESHUTDOWN( ShutdownStudioRender() );

		TRACESHUTDOWN( ShutdownMaterialSystem() );

		TRACESHUTDOWN( CL_Shutdown() );
	}
	else
#endif
	{
		if ( g_pMatchFramework )
		{
			TRACESHUTDOWN( g_pMatchFramework->Shutdown() );
		}

#ifndef DEDICATED
		TRACESHUTDOWN( S_Shutdown() );
#endif

		TRACESHUTDOWN( Decal_Shutdown() );

		TRACESHUTDOWN( modelloader->Shutdown() );

		TRACESHUTDOWN( ShutdownStudioRender() );

		TRACESHUTDOWN( StaticPropMgr()->Shutdown() );

		TRACESHUTDOWN( ShutdownMaterialSystem() );
	}

#if defined( REPLAY_ENABLED )
	TRACESHUTDOWN( Replay_Shutdown() );
#endif
	TRACESHUTDOWN( HLTV_Shutdown() );

	TRACESHUTDOWN( g_Log.Shutdown() );
	
	TRACESHUTDOWN( g_GameEventManager.Shutdown() );

#if !defined( DEDICATED )
	if ( !sv.IsDedicated() )
	{
		TRACESHUTDOWN( Steam3Client().Shutdown() );
	}
#endif

	TRACESHUTDOWN( sv.Shutdown() );

	TRACESHUTDOWN( NET_Shutdown() );

	TRACESHUTDOWN( g_pSteamSocketMgr->Shutdown() );

#ifndef DEDICATED
	TRACESHUTDOWN( Key_Shutdown() );
#if !defined( _X360 )
	TRACESHUTDOWN( ShutdownMixerControls() );
#endif
#endif

	TRACESHUTDOWN( Filter_Shutdown() );

#if !defined(DEDICATED) && !defined(LEFT4DEAD)
	TRACESHUTDOWN( saverestore->Shutdown() );
#endif

	TRACESHUTDOWN( COM_Shutdown() );

	// TRACESHUTDOWN( Host_ShutdownVCR() );
#ifndef DEDICATED
	TRACESHUTDOWN( V_Shutdown() );
#endif

	TRACESHUTDOWN( g_pCVar->Shutdown() );

	TRACESHUTDOWN( Cmd_Shutdown() );

	TRACESHUTDOWN( Cbuf_Shutdown() );

	TRACESHUTDOWN( Con_Shutdown() );

	TRACESHUTDOWN( Memory_Shutdown() );

	if ( g_pThreadPool )
		g_pThreadPool->Stop();

	DTI_Term();
	ServerDTI_Term();

#if defined(_WIN32)
	if ( s_bInitPME )
	{
		ShutdownPME();
	}
#endif

	if ( host_checkheap )
	{
#ifdef _WIN32
		if ( _heapchk() != _HEAPOK )
		{
			Sys_Error( "Host_Shutdown (bottom):  _heapchk() != _HEAPOK\n" );
		}
#endif
	}
}

//-----------------------------------------------------------------------------
// Centralize access to enabling QMS.
//-----------------------------------------------------------------------------
bool Host_AllowQueuedMaterialSystem( bool bAllow )
{
#if !defined DEDICATED
	g_bAllowThreadedSound = bAllow;
	// NOTE: Moved this to materialsystem for integrating with other mqm changes
	return g_pMaterialSystem->AllowThreading( bAllow, g_nMaterialSystemThread );
#endif
}

void Host_EnsureHostNameSet()
{
	// if there is host name set, set one
	if ( host_name.GetString()[0] == 0 )
	{
		const char *szHostName = "";
#ifndef DEDICATED
		// if this is a PC listen server and there is a logged-on Steam user, use the user's Steam name as the host name
		if ( IsPC() && !sv.IsDedicated() && Steam3Client().SteamUser() )
		{
			if ( Steam3Client().SteamUser()->BLoggedOn() )
			{
				szHostName = Steam3Client().SteamFriends()->GetPersonaName();
			}
		}
#endif
		// if all else fails, use the game description as the host name
		if ( !szHostName[0] )
		{
			szHostName = serverGameDLL->GetGameDescription();
		}
		host_name.SetValue( szHostName );
	}
}
