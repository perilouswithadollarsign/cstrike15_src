//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "quakedef.h"
#include <assert.h>
#include "engine_launcher_api.h"
#include "iengine.h"
#include "ivideomode.h"
#include "igame.h"
#include "vmodes.h"
#include "modes.h"
#include "sys.h"
#include "host.h"
#include "keys.h"
#include "cdll_int.h"
#include "host_state.h"
#include "cdll_engine_int.h"
#include "sys_dll.h"
#include "tier0/vprof.h"
#include "profile.h"
#include "gl_matsysiface.h"
#include "vprof_engine.h"
#include "server.h"
#include "cl_demo.h"
#include "toolframework/itoolframework.h"
#include "toolframework/itoolsystem.h"
#include "inputsystem/iinputsystem.h"
#include "gl_cvars.h"
#include "filesystem_engine.h"
#include "tier0/cpumonitoring.h"
#ifndef DEDICATED
#include "vgui_baseui_interface.h"
#endif
#ifdef _PS3
#include <sysutil/sysutil_sysparam.h>
#endif
#include "tier0/etwprof.h"

#include "steam/steam_api.h"
#include "appframework/ilaunchermgr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
void Sys_ShutdownGame( void );
int Sys_InitGame( CreateInterfaceFn appSystemFactory, 
			char const* pBaseDir, void *pwnd, int bIsDedicated );

// sleep time when not focus
ConVar engine_no_focus_sleep( "engine_no_focus_sleep", "50", FCVAR_ARCHIVE );


#define DEFAULT_FPS_MAX	300
static int s_nDesiredFPSMax = DEFAULT_FPS_MAX;
static bool s_bFPSMaxDrivenByPowerSavings = false;

// Dedicated server fps locking to tickrate values
extern float host_nexttick;

//-----------------------------------------------------------------------------
// ConVars and ConCommands
//-----------------------------------------------------------------------------
static void fps_max_callback( IConVar *var, const char *pOldValue, float flOldValue )
{
	// Only update s_nDesiredFPSMax when not driven by the mat_powersavingsmode ConVar (see below)
	if ( !s_bFPSMaxDrivenByPowerSavings )
	{
		s_nDesiredFPSMax = ( (ConVar *)var)->GetInt();
	}
}
ConVar fps_max( "fps_max", STRINGIFY( DEFAULT_FPS_MAX ), FCVAR_RELEASE, "Frame rate limiter", fps_max_callback );

// When set, this ConVar (typically driven from the advanced video settings) will drive fps_max (see above) to
// half of the refresh rate, if the user hasn't otherwise set fps_max (via console, commandline etc)
static void mat_powersavingsmode_callback( IConVar *var, const char *pOldValue, float flOldValue )
{
	s_bFPSMaxDrivenByPowerSavings = true;
	int nRefresh = s_nDesiredFPSMax;

	if ( ( (ConVar *)var)->GetBool() )
	{
		MaterialVideoMode_t mode;
		materials->GetDisplayMode( mode );
		nRefresh = MAX( 30, ( mode.m_RefreshRate + 1 ) >> 1 ); // Half of display refresh rate (min of 30Hz)
	}

	fps_max.SetValue( nRefresh );
	s_bFPSMaxDrivenByPowerSavings = false;
}
static ConVar mat_powersavingsmode( "mat_powersavingsmode", "0", FCVAR_ARCHIVE, "Power Savings Mode", mat_powersavingsmode_callback );

ConVar sleep_when_meeting_framerate( "sleep_when_meeting_framerate", IsGameConsole() ? "0" : "1", FCVAR_NONE, "Sleep instead of spinning if we're meeting the desired framerate." );

static ConVar fps_max_splitscreen( "fps_max_splitscreen", STRINGIFY( DEFAULT_FPS_MAX ), 0, "Frame rate limiter, splitscreen" );
#if !defined( DEDICATED )
static ConVar fps_max_menu( "fps_max_menu", "120", FCVAR_RELEASE, "Frame rate limiter, main menu" );
#endif
static ConVar async_serialize( "async_serialize", "0", 0, "Force async reads to serialize for profiling" );
#define ShouldSerializeAsync() async_serialize.GetBool()

static ConVar vx_do_not_throttle_events( "vx_do_not_throttle_events", "0", 0, "Force VXConsole updates every frame; smoother vprof data on PS3 but at a slight (~0.2ms) perf cost." );

#ifdef WIN32
static void cpu_frequency_monitoring_callback( IConVar *var, const char *pOldValue, float flOldValue )
{
	// Set the specified interval for CPU frequency monitoring
	SetCPUMonitoringInterval( (unsigned)( ( (ConVar *)var)->GetFloat() * 1000 ) );
}
ConVar cpu_frequency_monitoring( "cpu_frequency_monitoring", "0", FCVAR_RELEASE, "Set CPU frequency monitoring interval in seconds. Zero means disabled.", true, 0.0f, true, 10.0f, cpu_frequency_monitoring_callback );
#endif

float		host_filtered_time_history[128] = { 0 };
unsigned int host_filtered_time_history_pos = 0;
CON_COMMAND( host_filtered_time_report, "Dumps time spent idle in previous frames in ms(dedicated only)." )
{
	if ( sv.IsDedicated() )
	{
		for (int i = 1; i <= ARRAYSIZE( host_filtered_time_history ); ++i)
		{
			unsigned int slot = ( i + host_filtered_time_history_pos) % ARRAYSIZE( host_filtered_time_history );
			Msg( "%.4f\n", ( host_filtered_time_history[ slot ] * 1000 ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEngine : public IEngine
{
public:
					CEngine( void );
	virtual			~CEngine( void );

	bool			Load( bool dedicated, const char *basedir );

	virtual void	Unload( void );
	virtual EngineState_t GetState( void );
	virtual void	SetNextState( EngineState_t iNextState );

	void			Frame( void );

	float			GetFrameTime( void );
	float			GetCurTime( void );
	
	bool			TrapKey_Event( ButtonCode_t key, bool down );
	void			TrapMouse_Event( int buttons, bool down );

	void			StartTrapMode( void );
	bool			IsTrapping( void );
	bool			CheckDoneTrapping( ButtonCode_t& key );

	int				GetQuitting( void );
	void			SetQuitting( int quittype );

private:
	bool			FilterTime( float t );

	int				m_nQuitting;

	EngineState_t	m_nDLLState;
	EngineState_t	m_nNextDLLState;

	double			m_flCurrentTime;
	float			m_flFrameTime;
	double			m_flPreviousTime;
	float			m_flFilteredTime;
	float			m_flMinFrameTime; // Expected duration of a frame, or zero if it is unlimited.
#ifdef _GAMECONSOLE
	float           m_flTimeSinceLastXBXProcessEventsCall;
#endif

#if WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND
	STEAM_CALLBACK( CEngine, OnGameOverlayActivated, GameOverlayActivated_t, m_CallbackGameOverlayActivated );
#endif

};

static CEngine g_Engine;

IEngine *eng = ( IEngine * )&g_Engine;
//IEngineAPI *engine = NULL;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CEngine::CEngine( void )
#if WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND
: m_CallbackGameOverlayActivated( this, &CEngine::OnGameOverlayActivated )
#endif
{
	m_nDLLState			= DLL_INACTIVE;
	m_nNextDLLState		= DLL_INACTIVE;

	m_flCurrentTime		= 0.0;
	m_flFrameTime		= 0.0f;
	m_flPreviousTime	= 0.0;
	m_flFilteredTime	= 0.0f;
	m_flMinFrameTime	= 0.0f;
#ifdef _GAMECONSOLE
	m_flTimeSinceLastXBXProcessEventsCall = 1.0e19;			// make ti call on first frame
#endif

	m_nQuitting			= QUIT_NOTQUITTING;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEngine::~CEngine( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEngine::Unload( void )
{
	Sys_ShutdownGame();

	m_nDLLState			= DLL_INACTIVE;
	m_nNextDLLState		= DLL_INACTIVE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CEngine::Load( bool dedicated, const char *rootdir )
{
	bool success = false;

	// Activate engine
	// NOTE: We must bypass the 'next state' block here for initialization to work properly.
	m_nDLLState = m_nNextDLLState = DLL_ACTIVE;

	if ( Sys_InitGame( 
		g_AppSystemFactory,
		rootdir, 
		game->GetMainWindowAddress(), 
		dedicated ) )
	{
		success = true;

		UpdateMaterialSystemConfig();
	}

	return success;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CEngine::FilterTime( float dt )
{
	// Dedicated servers will lock fps max to tick rate essentially
	if ( sv.IsDedicated() && !g_bDedicatedServerBenchmarkMode )
	{
		m_flMinFrameTime = host_nexttick;
		return ( dt >= host_nexttick );
	}

	m_flMinFrameTime = 0.0f;

	// Dedicated's tic_rate regulates server frame rate.  Don't apply fps filter here.
	// Only do this restriction on the client. Prevents clients from accomplishing certain
	// hacks by pausing their client for a period of time.
	if ( IsPC() && !sv.IsDedicated() && !CanCheat() && ( fps_max.GetFloat() < 30 ) && !Host_IsSinglePlayerGame() )
	{
		// Don't do anything if fps_max=0 (which means it's unlimited).
		if ( fps_max.GetFloat() != 0.0f )
		{
			Warning( "sv_cheats is 0 and fps_max is being limited to a minimum of 30 (or set to 0).\n" );
			fps_max.SetValue( 30.0f );
		}
	}

	float fps = fps_max.GetFloat();
#ifdef _GAMECONSOLE
	static bool bInitializedFpsMax;
	static float flRefreshRate = 0;
	if ( !bInitializedFpsMax )
	{
		bInitializedFpsMax = true;
		{
		#ifdef _X360
			XVIDEO_MODE videoMode;
			XGetVideoMode( &videoMode );
			flRefreshRate = videoMode.RefreshRate;
		#elif defined( _PS3 )
			CellVideoOutState videoOutState;
			if ( cellVideoOutGetState( CELL_VIDEO_OUT_PRIMARY, 0, &videoOutState) >= CELL_OK )
			{
				struct { int rrFlag; float flRate; }
				arrRefreshRates[] = {
					{ CELL_VIDEO_OUT_REFRESH_RATE_59_94HZ, 59.94f },
					{ CELL_VIDEO_OUT_REFRESH_RATE_60HZ, 60.00f },
					{ CELL_VIDEO_OUT_REFRESH_RATE_50HZ, 50.00f },
					{ CELL_VIDEO_OUT_REFRESH_RATE_30HZ, 30.00f },
				};
				for ( int jj = 0; jj < ARRAYSIZE( arrRefreshRates ); ++ jj )
				{
					if ( arrRefreshRates[jj].rrFlag & videoOutState.displayMode.refreshRates )
					{
						flRefreshRate = arrRefreshRates[jj].flRate;
						break;
					}
				}
				if ( !flRefreshRate )
				{
					Warning( "Failed to determine PS3 video out refresh rate, assuming 59.94 Hz\n" );
					flRefreshRate = 59.94f;
				}
			}
			else
			{
				bInitializedFpsMax = false;
			}
		#else
			#error
		#endif
		}

// Taken fps_max out since we'll use the presentation interval to force max 30fps
// This gives us a much smoother frametime and ensure we don't drop a frame
// due to the inaccuracy of fps_max
//
//		if ( flRefreshRate > 49 )
//		{	
//			float fpsMax = flRefreshRate / 2.0f, fpsSplitscreenMax = flRefreshRate / 2.0f;
//			DevMsg( "Setting fps_max to %f and fps_splitscreen_max to %f (from defaults of %f/%f ) to match refresh rate of %f\n", fpsMax, fpsSplitscreenMax, fps_max.GetFloat(), fps_max_splitscreen.GetFloat(), flRefreshRate );
//			fps_max.SetValue( fpsMax );
//			fps_max_splitscreen.SetValue( fpsSplitscreenMax );
//		}

	}

	bool bSplitscreen = false;
	// Need a smarter way of doing this
	for ( int i = 1; i < splitscreen->GetNumSplitScreenPlayers(); i++ )
	{
		if ( splitscreen->IsValidSplitScreenSlot( i ) )
		{
			bSplitscreen = true;
			break;
		}
	}

	if ( !bSplitscreen )
	{
		fps = fps_max.GetFloat();
	}
	else
	{
		fps = fps_max_splitscreen.GetFloat();
	}
#endif

#if !defined( DEDICATED )
	extern IVEngineClient *engineClient;
	if ( engineClient && !engineClient->IsConnected() && ( fps_max_menu.GetFloat() < fps ) )
	{
		fps = fps_max_menu.GetFloat();
	}
#endif

#ifdef _PS3
	{
		int nPresentFrequency = 1;
		if ( fps > 1.0f )
		{
			nPresentFrequency = int( (flRefreshRate + 1.0) / fps );
			nPresentFrequency = MAX( 1, nPresentFrequency );
		}
		g_pMaterialSystem->SetFlipPresentFrequency( nPresentFrequency );
	}
#endif
	if ( fps > 0.0f )
	{
		// Limit fps to withing tolerable range
//		fps = max( MIN_FPS, fps ); // red herring - since we're only checking if dt < 1/fps, clamping against MIN_FPS has no effect
		fps = MIN( MAX_FPS, fps );

		float minframetime = 1.0 / fps;
		
		m_flMinFrameTime = minframetime;

		if (
#if !defined(DEDICATED)
		    !demoplayer->IsPlayingTimeDemo() && 
#endif
			!g_bDedicatedServerBenchmarkMode && 
			dt < minframetime )
		{
			// framerate is too high
			return false;
		}
	}

	return true;
}

extern void PS3_PollSaveSystem();
//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
void CEngine::Frame( void )
{
	// yield the CPU for a little while when paused, minimized, or not the focus
	// FIXME:  Move this to main windows message pump?
	static ConVarRef cl_embedded_stream_video_playing( "cl_embedded_stream_video_playing" );
	if ( IsPC() && !game->IsActiveApp() && !sv.IsDedicated()
		&& !( cl_embedded_stream_video_playing.IsValid() && cl_embedded_stream_video_playing.GetBool() )
		&& engine_no_focus_sleep.GetInt() > 0 )
	{
		g_pInputSystem->SleepUntilInput( engine_no_focus_sleep.GetInt() );
	}

	// Get current time
	m_flCurrentTime	= Sys_FloatTime();

	// Watch for data from the CPU frequency monitoring system and print it to the console.
	const CPUFrequencyResults frequency = GetCPUFrequencyResults();
	static double s_lastFrequencyTimestamp;
	if ( frequency.m_timeStamp > s_lastFrequencyTimestamp )
	{
		s_lastFrequencyTimestamp = frequency.m_timeStamp;
		Msg( "~CPU Freq: %1.3f GHz    Percent of requested: %3.1f%%    Minimum percent seen: %3.1f%%\n",
					frequency.m_GHz, frequency.m_percentage, frequency.m_lowestPercentage );
	}

	// Determine dt since we last checked
	float dt = m_flCurrentTime - m_flPreviousTime;
	if ( sv.IsDedicated() && ( dt < 0 ) )
	{
		// ... but if the clock ever went backwards due to a bug,
		// we'd have no idea how much time has elapsed, so just 
		// catch up to the next scheduled server tick.
		dt = host_nexttick;
	}

#ifdef _GAMECONSOLE
#define XBOX_PROCESS_EVENTS_MAXINTERVAL  0.2 				// 1/5 sec
		// handle Xbox system messages process xbox events occasionally. every frame is too often -
		// makes this code add up to something
		m_flTimeSinceLastXBXProcessEventsCall += MAX( 0, dt );
		if ( m_flTimeSinceLastXBXProcessEventsCall > XBOX_PROCESS_EVENTS_MAXINTERVAL || vx_do_not_throttle_events.GetBool() )
		{
			XBX_ProcessEvents();
			XBX_DispatchEventsQueue();
			m_flTimeSinceLastXBXProcessEventsCall = 0.;
		}
#endif

	// Remember old time
	m_flPreviousTime = m_flCurrentTime;

	// Accumulate current time delta into the true "frametime"
	m_flFrameTime += dt;

	// If the time is < 0, that means we've restarted. 
	// Set the new time high enough so the engine will run a frame
	if ( m_flFrameTime < 0.0f )
		return;

	// If the frametime is still too short, don't pass through
	if ( !FilterTime( m_flFrameTime ) )
	{
#ifdef POSIX
		double fSleepNS = ( m_flMinFrameTime - m_flFrameTime ) * 1000000000.0;
		unsigned nSleepNS = (unsigned)floor( fSleepNS );
		if ( nSleepNS && sleep_when_meeting_framerate.GetInt() )
		{
			TM_ZONE( TELEMETRY_LEVEL0, TMZF_NONE, "Engine Nano Sleep" );
			ThreadNanoSleep( nSleepNS );
		}
#else //POSIX
		float fSleepMS = ( m_flMinFrameTime - m_flFrameTime ) * 1000;
		unsigned nSleepMS = (unsigned)floor( fSleepMS );
		if ( nSleepMS && sleep_when_meeting_framerate.GetInt() )
		{
			TM_ZONE( TELEMETRY_LEVEL0, TMZF_NONE, "Engine Sleep" );
			ThreadSleep( nSleepMS );
		}
#endif //POSIX
		m_flFilteredTime += dt;
		return;
	}

    TM_ZONE( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __PRETTY_FUNCTION__ );

	if ( ShouldSerializeAsync() )
	{
		static ConVar *pSyncReportConVar = g_pCVar->FindVar( "fs_report_sync_opens" );
		bool bReportingSyncOpens = ( pSyncReportConVar && pSyncReportConVar->GetInt() );
		int reportLevel = 0;
		if ( bReportingSyncOpens )
		{
			reportLevel = pSyncReportConVar->GetInt();
			pSyncReportConVar->SetValue( 0 );
		}
		g_pFileSystem->AsyncFinishAll();
		if ( bReportingSyncOpens )
		{
			pSyncReportConVar->SetValue( reportLevel );
		}
	}

#ifdef VPROF_ENABLED
	PreUpdateProfile( m_flFilteredTime );
#endif
	
	// Record previous swallowed time counts.
	host_filtered_time_history[ host_filtered_time_history_pos ] = m_flFilteredTime;
	host_filtered_time_history_pos = ( host_filtered_time_history_pos + 1 ) % ARRAYSIZE(host_filtered_time_history);
	// Reset swallowed time...
	m_flFilteredTime = 0.0f;

#ifndef DEDICATED
	if ( !sv.IsDedicated() )
	{
		ClientDLL_FrameStageNotify( FRAME_START );
	}
#endif

#ifdef VPROF_ENABLED
	PostUpdateProfile();
#endif

	TelemetryTick();

	ETWRenderFrameMark( sv.IsDedicated() );

	{ // profile scope

	VPROF_BUDGET( "CEngine::Frame", VPROF_BUDGETGROUP_OTHER_UNACCOUNTED );
#ifdef RAD_TELEMETRY_ENABLED
	TmU64 time0 = tmFastTime();
#endif

	switch( m_nDLLState )
	{
	case DLL_PAUSED:			// paused, in hammer
	case DLL_INACTIVE:			// no dll
		break;

	case DLL_ACTIVE:			// engine is focused
	case DLL_CLOSE:				// closing down dll
	case DLL_RESTART:			// engine is shutting down but will restart right away
		// Run the engine frame
		HostState_Frame( m_flFrameTime );
		break;
	}

	// Has the state changed?
	if ( m_nNextDLLState != m_nDLLState )
	{
		m_nDLLState = m_nNextDLLState;

		// Do special things if we change to particular states
		switch( m_nDLLState )
		{
		case DLL_CLOSE:
			SetQuitting( QUIT_TODESKTOP );
			break;
		case DLL_RESTART:
			SetQuitting( QUIT_RESTART );
			break;
		}
	}
	
#ifdef RAD_TELEMETRY_ENABLED
	float time = ( tmFastTime() - time0 ) * g_Telemetry.flRDTSCToMilliSeconds;
	if( time > 0.5f )
	{
		tmPlot( TELEMETRY_LEVEL0, TMPT_TIME_MS, 0, time, "CEngine::Frame(ms)" );
	}
#endif

	} // profile scope

	// Reset for next frame
	m_flFrameTime = 0.0f;

#if defined( VPROF_ENABLED ) && defined( VPROF_VXCONSOLE_EXISTS )
	UpdateVXConsoleProfile();
#endif
	// reload dlls that are marked for reload; currently for debug purposes only
#ifdef ENGINE_MANAGES_VJOBS
	extern void ReloadDlls();
	ReloadDlls();
#endif
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEngine::EngineState_t CEngine::GetState( void )
{
	return m_nDLLState;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEngine::SetNextState( EngineState_t iNextState )
{
	m_nNextDLLState = iNextState;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CEngine::GetFrameTime( void )
{
	return m_flFrameTime;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CEngine::GetCurTime( void )
{
	return m_flCurrentTime;
}


//-----------------------------------------------------------------------------
// Purpose: Flag that we are in the process of quiting
//-----------------------------------------------------------------------------
void CEngine::SetQuitting( int quittype )
{
	m_nQuitting = quittype;
}


//-----------------------------------------------------------------------------
// Purpose: Check whether we are ready to exit
//-----------------------------------------------------------------------------
int CEngine::GetQuitting( void )
{
	return m_nQuitting;
}

#if WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND
//-----------------------------------------------------------------------------
// Purpose: The overlay doesn't properly work on OS X 64-bit because a bunch of 
// Cocoa functions that we hook were never ported to 64-bit. Until that is fixed,
// we basically have to work around this by making sure the cursor is visible 
// and set to something that is reasonable for usage in the overlay. 
//-----------------------------------------------------------------------------
void CEngine::OnGameOverlayActivated( GameOverlayActivated_t *pGameOverlayActivated )
{
	Assert( pGameOverlayActivated );
	if ( pGameOverlayActivated->m_bActive )
		g_pLauncherMgr->ForceSystemCursorVisible();
	else
		g_pLauncherMgr->UnforceSystemCursorVisible();
}
#endif // WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND
