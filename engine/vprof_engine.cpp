//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: VProf engine integration
//
//===========================================================================//

#include "tier0/platform.h"
#include "sys.h"
#include "vprof_engine.h"
#include "sv_main.h"
#include "iengine.h"
#include "basetypes.h"
#include "convar.h"
#include "cmd.h"
#include "tier1/strtools.h"
#include "con_nprint.h"
#include "tier0/vprof.h"
#include "materialsystem/imaterialsystem.h"
#ifndef DEDICATED
#include "vgui_baseui_interface.h"
#include "vgui_vprofpanel.h"
#endif
#include "utlvector.h"
#include "sv_remoteaccess.h"
#include "ivprofexport.h"
#include "vprof_record.h"
#include "filesystem_engine.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"
#include "debugoverlay.h"
#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef VPROF_ENABLED
void VProfExport_StartOrStop();

static ConVar vprof_dump_spikes( "vprof_dump_spikes","0", 0, "Framerate at which vprof will begin to dump spikes to the console. 0 = disabled, negative to reset after dump" );
static ConVar vprof_dump_spikes_terse( "vprof_dump_spikes_terse","0", 0, "Whether to use most terse output" );
static ConVar vprof_dump_spikes_hierarchy( "vprof_dump_spikes_hiearchy","0", 0, "Set to 1 to get a hierarchy report whith vprof_dump_spikes" );
static ConVar vprof_dump_spikes_node( "vprof_dump_spikes_node","", 0, "Node to start report from when doing a dump spikes" );
static ConVar vprof_dump_spikes_budget_group( "vprof_dump_spikes_budget_group","", 0, "Budget gtNode to start report from when doing a dump spikes" );
static ConVar vprof_dump_oninterval( "vprof_dump_oninterval", "0", 0, "Interval (in seconds) at which vprof will batch up data and dump it to the console." );

static void (*g_pfnDeferredOp)();

static void ExecuteDeferredOp() 
{
	if ( g_pfnDeferredOp )
	{
		(*g_pfnDeferredOp)();
		g_pfnDeferredOp = NULL;
	}
}

unsigned g_VProfTargetThread = ThreadGetCurrentId();
	
const double MAX_SPIKE_REPORT = 1.0;
const int MAX_SPIKE_REPORT_FRAMES = 10;
static double LastSpikeTime = 0;
static int LastSpikeFrame = 0;
static ConVar vprof_counters( "vprof_counters", "0", 0 );
static ConVar vprof_counters_show_minmax( "vprof_counters_show_minmax", "0", 0 );

extern bool con_debuglog;
extern ConVar con_logfile;
static bool g_fVprofOnByUI;
static bool g_bVProfNoVSyncOff = false;

class ConsoleLogger
{
public:
	ConsoleLogger( void )
	{
#ifndef _X360
#if !defined( DEDICATED )
		m_condebugEnabled = con_debuglog;
#else
		m_condebugEnabled = false;
#endif
		if ( !m_condebugEnabled )
		{
			g_pFileSystem->CreateDirHierarchy( "vprof" );
			while ( 1 )
			{
				++m_index;
				const char *fname = va( "vprof/vprof%d.txt", m_index );
				if ( g_pFileSystem->FileExists( fname ) )
				{
					continue;
				}

#if !defined( DEDICATED )
				con_logfile.SetValue( fname );
#endif
				break;
			}
		}
#endif
	}

	~ConsoleLogger()
	{
#ifndef _X360
		if ( !m_condebugEnabled )
		{
#if !defined( DEDICATED )
			con_logfile.SetValue( "" );
#endif
		}
#endif
	}

private:
	static int m_index;
	bool m_condebugEnabled;
};

int ConsoleLogger::m_index = 0;

static float s_flIntervalStartTime = 0.0f;
static bool g_bDumpCounters = false;

CON_COMMAND(vprof_dump_counters, "Dump vprof counters to the console" )
{
	g_bDumpCounters = true;
}

void PreUpdateProfile( float filteredtime )
{
	Assert( g_VProfCurrentProfile.AtRoot() );

	ExecuteDeferredOp();
	VProfExport_StartOrStop();
	VProfRecord_StartOrStop();

	if ( g_VProfCurrentProfile.GetTargetThreadId() != g_VProfTargetThread )
	{
		g_VProfCurrentProfile.SetTargetThreadId( g_VProfTargetThread );
	}

	// Check to see if it is time to dump the data and restart collection.
	if ( g_VProfCurrentProfile.IsEnabled() && ( vprof_dump_oninterval.GetFloat() != 0.0f ) )
	{
		float flCurrentTime = eng->GetCurTime();
		float flIntervalTime = vprof_dump_oninterval.GetFloat();

		g_VProfCurrentProfile.MarkFrame();

		if ( ( s_flIntervalStartTime + flIntervalTime ) < flCurrentTime )
		{
			// Dump the current profile.
			g_VProfCurrentProfile.OutputReport( VPRT_SUMMARY | VPRT_LIST_BY_TIME | VPRT_LIST_BY_AVG_TIME | VPRT_LIST_BY_TIME_LESS_CHILDREN | VPRT_LIST_TOP_ITEMS_ONLY );

			// Stop the current profile.
			g_VProfCurrentProfile.Stop();

			// Reset and restart the current profile.
			g_VProfCurrentProfile.Reset();
			g_VProfCurrentProfile.Start();
			s_flIntervalStartTime = flCurrentTime;
		}
	}

	if( g_VProfCurrentProfile.IsEnabled() && vprof_dump_spikes.GetFloat() )
	{
		float spikeThreash = fabsf( vprof_dump_spikes.GetFloat() );
		g_VProfCurrentProfile.MarkFrame();
		bool bSuppressRestart = false;
		if ( g_VProfSignalSpike || eng->GetFrameTime() > ( 1.f / spikeThreash ) )
		{
			if( g_VProfSignalSpike || ( Sys_FloatTime() - LastSpikeTime > MAX_SPIKE_REPORT && g_ServerGlobalVariables.framecount > LastSpikeFrame + MAX_SPIKE_REPORT_FRAMES ) )
			{
				ConsoleLogger consoleLog;
				Msg( "******** Spike on frame %d at time %.3f ", g_ServerGlobalVariables.framecount, Plat_FloatTime() );
				if ( vprof_dump_spikes_hierarchy.GetBool() )
				{
					g_VProfCurrentProfile.OutputReport( VPRT_HIERARCHY_TIME_PER_FRAME_AND_COUNT_ONLY,
						( vprof_dump_spikes_node.GetString()[0] ) ? vprof_dump_spikes_node.GetString() : NULL,
						( vprof_dump_spikes_budget_group.GetString()[0] ) ? g_VProfCurrentProfile.BudgetGroupNameToBudgetGroupID( vprof_dump_spikes_budget_group.GetString() ) : -1 );
				}
				else
				{
					int flags;
					if ( !vprof_dump_spikes_terse.GetBool() )
					{
						flags = VPRT_SUMMARY | VPRT_LIST_BY_TIME | VPRT_LIST_BY_AVG_TIME | VPRT_LIST_BY_TIME_LESS_CHILDREN | VPRT_LIST_TOP_ITEMS_ONLY;
					}
					else
					{
						flags = VPRT_LIST_BY_TIME | VPRT_LIST_TOP_ITEMS_ONLY;
					}
					g_VProfCurrentProfile.OutputReport( flags,
						( vprof_dump_spikes_node.GetString()[0] ) ? vprof_dump_spikes_node.GetString() : NULL,
						( vprof_dump_spikes_budget_group.GetString()[0] ) ? g_VProfCurrentProfile.BudgetGroupNameToBudgetGroupID( vprof_dump_spikes_budget_group.GetString() ) : -1 );
				}

				LastSpikeTime = Sys_FloatTime();
				LastSpikeFrame = g_ServerGlobalVariables.framecount;

				if ( vprof_dump_spikes.GetFloat() < 0.0 )
				{
					vprof_dump_spikes.SetValue( 0.0f );
					// g_VProfCurrentProfile.Stop();
					g_fVprofOnByUI = false;
					bSuppressRestart = true;
				}
			}
			g_VProfSignalSpike = false;
		}

		int iStartDepth = 0;
		do 
		{
			g_VProfCurrentProfile.Stop();
			iStartDepth++;
		} while( g_VProfCurrentProfile.IsEnabled() );

		if (!bSuppressRestart)
		{

			g_VProfCurrentProfile.Reset();

			while ( iStartDepth-- )
			{
				g_VProfCurrentProfile.Start();
			}
		}

		Assert( g_VProfCurrentProfile.AtRoot() );
		Assert( g_VProfCurrentProfile.IsEnabled() );
	}

	int nCounterType = vprof_counters.GetInt();
	if ( nCounterType || g_bDumpCounters )
	{
		int i;
		int n = g_VProfCurrentProfile.GetNumCounters();
		int nprintIndex = 0;
		int static nCycle = 0;
		for ( i = 0; i < n; i++ )
		{
			if ( g_VProfCurrentProfile.GetCounterGroup( i ) != ( nCounterType - 1 ) )
				continue;
			const char *pName;
			int val;
			pName = g_VProfCurrentProfile.GetCounterNameAndValue( i, val );
			if ( g_bDumpCounters )
			{
				Msg("VPROF: %s = %d\n", pName, val );
			}
			if ( !vprof_counters_show_minmax.GetBool() )
			{
				if ( IsPC() )
				{
					Con_NPrintf( nprintIndex, "%s = %d\n", pName, val );
				}
				else if ( IsGameConsole() )
				{
#ifndef DEDICATED
					CDebugOverlay::AddScreenTextOverlay( 0.05f, 0.05f, nprintIndex, 0.001f, 255, 255, 255, 255,  CFmtStr( "%s = %d", pName, val ) );
#endif
				}
			}
			else
			{
				static CUtlVector<int> history[30];

				history[nCycle].EnsureCount( n );
				history[nCycle][i] = val;

				int valMin = val;
				int valMax = val;
				for (int j = 0; j < 30; j++)
				{
					history[j].EnsureCount( n );
					valMin = MIN( valMin, history[j][i] );
					valMax = MAX( valMax, history[j][i] );
				}

				if ( IsPC() )
				{
					Con_NPrintf( nprintIndex, "%s = %6d (%6d:%6d)\n", pName, val, valMin, valMax );
				}
				else if ( IsGameConsole() )
				{
#ifndef DEDICATED
					CDebugOverlay::AddScreenTextOverlay( 0.05f, 0.05f, nprintIndex, 0.001f, 255, 255, 255, 255,  CFmtStr( "%s = %6d (%6d:%6d)", pName, val, valMin, valMax ) );
#endif
				}
			}
			nprintIndex++;
		}
		nCycle = (nCycle + 1) % 30;
	}
	g_VProfCurrentProfile.ResetCounters( COUNTER_GROUP_DEFAULT );
	g_VProfCurrentProfile.ResetCounters( COUNTER_GROUP_TEXTURE_PER_FRAME );
	g_VProfCurrentProfile.ResetCounters( COUNTER_GROUP_GRAPHICS_PER_FRAME );

	// This MUST come before GetVProfPanel()->UpdateProfile(), because UpdateProfile uses the data we snapshot here.
	VProfExport_SnapshotVProfHistory();
#ifdef VPROF_ENABLED
	VProfRecord_Snapshot();
#endif

#ifndef DEDICATED
	// Update the vgui panel
	if ( GetVProfPanel() )
		GetVProfPanel()->UpdateProfile( filteredtime );
#endif
	g_bDumpCounters = false;
}

void PostUpdateProfile()
{
	if ( g_VProfCurrentProfile.IsEnabled() && !vprof_dump_spikes.GetFloat() && !vprof_dump_oninterval.GetFloat() )
	{
		g_VProfCurrentProfile.MarkFrame();
	}
}

#if defined( VPROF_VXCONSOLE_EXISTS )
void UpdateVXConsoleProfile()
{
	g_VProfCurrentProfile.VXProfileUpdate();
}
#endif

static bool g_fVprofCacheMissOnByUI = false;
static char g_szDefferedArg1[128];
static char g_szDefferedArg2[128];

#define DEFERRED_CON_COMMAND( cmd, help )									\
	static void cmd##_Impl();												\
	CON_COMMAND(cmd, help)													\
	{																		\
		g_pfnDeferredOp = cmd##_Impl;										\
		Q_strncpy( g_szDefferedArg1, args[1], sizeof(g_szDefferedArg1) );	\
		Q_strncpy( g_szDefferedArg2, args[2], sizeof(g_szDefferedArg2) );	\
	}																		\
	static void cmd##_Impl()

CON_COMMAND_F( spike,"generates a fake spike", FCVAR_CHEAT )
{
	Sys_Sleep(1000);
}

CON_COMMAND( vprof_vtune_group, "enable vtune for a particular vprof group (\"disable\" to disable)" )
{
	if( args.ArgC() != 2 )
	{
		Warning( "vprof_vtune_group groupName (disable to turn off)\n" );
		return;
	}
	const char *pArg = args[ 1 ];
	if( Q_stricmp( pArg, "disable" ) == 0 )
	{
		g_VProfCurrentProfile.DisableVTuneGroup();
	}
	else
	{
		g_VProfCurrentProfile.EnableVTuneGroup( args[ 1 ] );
	}
}

CON_COMMAND( vprof_dump_groupnames, "Write the names of all of the vprof groups to the console." )
{
	int n = g_VProfCurrentProfile.GetNumBudgetGroups();
	int i;
	for( i = 0; i < n; i++ )
	{
		Msg( "group %d: \"%s\"\n", i, g_VProfCurrentProfile.GetBudgetGroupName( i ) );
	}
}

DEFERRED_CON_COMMAND( vprof_cachemiss, "Toggle VProf cache miss checking" )
{
	if ( !g_fVprofCacheMissOnByUI )
	{
		Msg("VProf cache miss enabled.\n");
		g_VProfCurrentProfile.PMEEnable( true );
		g_fVprofCacheMissOnByUI = true;
	}
	else
	{
		Msg("VProf cache miss disabled.\n");
		g_VProfCurrentProfile.PMEEnable( false );
		g_fVprofCacheMissOnByUI = false;
	}
}

DEFERRED_CON_COMMAND( vprof_cachemiss_on, "Turn on VProf cache miss checking" )
{
	if ( !g_fVprofCacheMissOnByUI )
	{
		Msg("VProf cache miss enabled.\n");
		g_VProfCurrentProfile.PMEEnable( true );
		g_fVprofCacheMissOnByUI = true;
	}
}

DEFERRED_CON_COMMAND( vprof_cachemiss_off, "Turn off VProf cache miss checking" )
{
	if ( g_fVprofCacheMissOnByUI )
	{
		Msg("VProf cache miss disabled.\n");
		g_VProfCurrentProfile.PMEEnable( false );
		g_fVprofCacheMissOnByUI = false;
	}
}

DEFERRED_CON_COMMAND( vprof, "Toggle VProf profiler" )
{
	if ( !g_fVprofOnByUI )
	{
		Msg("VProf enabled.\n");
		g_VProfCurrentProfile.Start();
		g_fVprofOnByUI = true;
	}
	else
	{
		Msg("VProf disabled.\n");
		g_VProfCurrentProfile.Stop();
		g_fVprofOnByUI = false;
	}
}

#ifdef _X360
DEFERRED_CON_COMMAND( vprof_360_novsync_off, "Leaves vsync on when vxconsole brings up showbudget." )
{
	g_bVProfNoVSyncOff = !g_bVProfNoVSyncOff;
	Msg("VProf novsync auto setting %s.\n", g_bVProfNoVSyncOff ? "disabled" : "enabled" );
}

DEFERRED_CON_COMMAND( vprof_360_show_time, "Shows time in vprof" )
{
	g_VProfCurrentProfile.VXConsoleReportMode( CVProfile::VXCONSOLE_REPORT_TIME );
}

DEFERRED_CON_COMMAND( vprof_360_show_cachemiss, "Shows cachemisses in vprof" )
{
	if ( !g_fVprofCacheMissOnByUI )
	{
		Msg("VProf cache miss enabled.\n");
		g_VProfCurrentProfile.PMEEnable( true );
		g_fVprofCacheMissOnByUI = true;
	}

	g_VProfCurrentProfile.VXConsoleReportMode( CVProfile::VXCONSOLE_REPORT_L2CACHE_MISSES );
}

DEFERRED_CON_COMMAND( vprof_360_show_loadhitstore, "Shows load-hit-stores in vprof" )
{
	if ( !g_fVprofCacheMissOnByUI )
	{
		Msg("VProf cache miss enabled.\n");
		g_VProfCurrentProfile.PMEEnable( true );
		g_fVprofCacheMissOnByUI = true;
	}

	g_VProfCurrentProfile.VXConsoleReportMode( CVProfile::VXCONSOLE_REPORT_LOAD_HIT_STORE );
}

DEFERRED_CON_COMMAND( vprof_360_time_scale, "Scale used when displaying time (0 = use default)" )
{
	float flScale = atof(g_szDefferedArg1);
	if ( flScale <= 0.0f )
	{
		flScale = 1000.0f;
	}
	g_VProfCurrentProfile.VXConsoleReportScale( CVProfile::VXCONSOLE_REPORT_TIME, flScale );
}

DEFERRED_CON_COMMAND( vprof_360_cachemiss_scale, "Scale used when displaying cachemisses (0 = use default)" )
{
	float flScale = atof(g_szDefferedArg1);
	if ( flScale <= 0.0f )
	{
		flScale = 1.0f;
	}
	g_VProfCurrentProfile.VXConsoleReportScale( CVProfile::VXCONSOLE_REPORT_L2CACHE_MISSES, flScale );
}

DEFERRED_CON_COMMAND( vprof_360_loadhitstore_scale, "Scale used when displaying load-hit-stores (0 = use default)" )
{
	float flScale = atof(g_szDefferedArg1);
	if ( flScale <= 0.0f )
	{
		flScale = 0.1f;
	}
	g_VProfCurrentProfile.VXConsoleReportScale( CVProfile::VXCONSOLE_REPORT_LOAD_HIT_STORE, flScale );
}
#endif // 360

DEFERRED_CON_COMMAND( vprof_on, "Turn on VProf profiler" )
{
	if ( !g_fVprofOnByUI )
	{
		Msg("VProf enabled.\n");

		g_VProfCurrentProfile.Start();
		g_fVprofOnByUI = true;
		if ( IsX360() && !g_bVProfNoVSyncOff )
		{
			ConVarRef mat_vsyncref( "mat_vsync" );
			if ( mat_vsyncref.GetBool() )
			{
				Warning( "Disabling vsync (via mat_vsync) to increase profiling accuracy.\n" );
				mat_vsyncref.SetValue( false );
			}
		}
	}
}

CON_COMMAND( budget_toggle_group, "Turn a budget group on/off" )
{
	if( args.ArgC() != 2 )
	{
		return;
	}

	int budgetGroup = g_VProfCurrentProfile.BudgetGroupNameToBudgetGroupIDNoCreate( args[1] );

	if ( budgetGroup == -1 )
	{
		return;
	}

	g_VProfCurrentProfile.HideBudgetGroup( budgetGroup, !(g_VProfCurrentProfile.GetBudgetGroupFlags( budgetGroup ) & BUDGETFLAG_HIDDEN) );
}

DEFERRED_CON_COMMAND( vprof_off, "Turn off VProf profiler" )
{
	if ( g_fVprofOnByUI )
	{
		Msg("VProf disabled.\n");
		g_VProfCurrentProfile.Stop();
		g_fVprofOnByUI = false;

		// alien swarm has special behavior for certain testing scenarios: 
		// generate a report after turning off vprof
		if ( g_szDefferedArg1[0] && stricmp("infested",g_szDefferedArg1) == 0 )
		{
			ConsoleLogger consoleLog;
			g_VProfCurrentProfile.OutputReport( VPRT_FULL & ~VPRT_HIERARCHY, NULL );
		}

#if defined( _X360 )
		// disable all updating
		g_VProfCurrentProfile.VXEnableUpdateMode( 0xFFFFFFFF, false );
#endif
	}
}

DEFERRED_CON_COMMAND( vprof_reset, "Reset the stats in VProf profiler" )
{
	Msg("VProf reset.\n");
	g_VProfCurrentProfile.Reset();

#ifndef DEDICATED
	if ( GetVProfPanel() )
	{
		GetVProfPanel()->Reset();
	}
#endif
}

DEFERRED_CON_COMMAND(vprof_reset_peaks, "Reset just the peak time in VProf profiler")
{
	Msg("VProf peaks reset.\n");
	g_VProfCurrentProfile.ResetPeaks();
}

DEFERRED_CON_COMMAND(vprof_generate_report, "Generate a report to the console.")
{
	g_VProfCurrentProfile.Pause();
	ConsoleLogger consoleLog;
	g_VProfCurrentProfile.OutputReport( VPRT_FULL & ~VPRT_HIERARCHY, (g_szDefferedArg1[0]) ? g_szDefferedArg1 : NULL );
	g_VProfCurrentProfile.Resume();
}

DEFERRED_CON_COMMAND(vprof_generate_report_budget, "Generate a report to the console based on budget group.")
{
	if ( !g_szDefferedArg1[0] )
	{
		return;
	}
	g_VProfCurrentProfile.Pause();
	ConsoleLogger consoleLog;
	g_VProfCurrentProfile.OutputReport( VPRT_FULL & ~VPRT_HIERARCHY, NULL, g_VProfCurrentProfile.BudgetGroupNameToBudgetGroupID( g_szDefferedArg1 ) );
	g_VProfCurrentProfile.Resume();
}

DEFERRED_CON_COMMAND(vprof_generate_report_hierarchy, "Generate a report to the console.")
{
	g_VProfCurrentProfile.Pause();
	ConsoleLogger consoleLog;
	g_VProfCurrentProfile.OutputReport( VPRT_HIERARCHY, (g_szDefferedArg1[0]) ? g_szDefferedArg1 : NULL );
	g_VProfCurrentProfile.Resume();
}

DEFERRED_CON_COMMAND(vprof_generate_report_hierarchy_per_frame_and_count_only, "Generate a minimal hiearchical report to the console.")
{
	g_VProfCurrentProfile.Pause();
	ConsoleLogger consoleLog;
	g_VProfCurrentProfile.OutputReport( VPRT_HIERARCHY_TIME_PER_FRAME_AND_COUNT_ONLY );
	g_VProfCurrentProfile.Resume();
}

DEFERRED_CON_COMMAND(vprof_generate_report_AI, "Generate a report to the console.")
{
	// This is an unfortunate artifact of deferred commands not supporting arguments
	g_VProfCurrentProfile.Pause();
	ConsoleLogger consoleLog;
	g_VProfCurrentProfile.OutputReport( (VPRT_FULL & ~VPRT_HIERARCHY), "NPCs" );
	g_VProfCurrentProfile.Resume();
}

DEFERRED_CON_COMMAND(vprof_generate_report_AI_only, "Generate a report to the console.")
{
	// This is an unfortunate artifact of deferred commands not supporting arguments
	g_VProfCurrentProfile.Pause();
	ConsoleLogger consoleLog;
	g_VProfCurrentProfile.OutputReport( (VPRT_FULL & ~VPRT_HIERARCHY), "NPCs", g_VProfCurrentProfile.BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_NPCS ) );
	g_VProfCurrentProfile.Resume();
}

DEFERRED_CON_COMMAND(vprof_generate_report_map_load, "Generate a report to the console.")
{
	// This is an unfortunate artifact of deferred commands not supporting arguments
	g_VProfCurrentProfile.Pause();
	ConsoleLogger consoleLog;
	g_VProfCurrentProfile.OutputReport( VPRT_FULL, "Host_NewGame" );
	g_VProfCurrentProfile.Resume();
}

#ifdef VPROF_VXCONSOLE_EXISTS

CON_COMMAND( vx_vprof_update, "" )
{
	if ( args.ArgC() < 2 )
		return;

	const char *pArg = args[1];
	if ( !Q_stricmp( pArg, "cpu" ) )
	{
		g_VProfCurrentProfile.VXEnableUpdateMode( VPROF_UPDATE_BUDGET, true );
	}
	else if ( !Q_stricmp( pArg, "texture" ) )
	{
		g_VProfCurrentProfile.VXEnableUpdateMode( VPROF_UPDATE_TEXTURE_GLOBAL, true );
		g_VProfCurrentProfile.VXEnableUpdateMode( VPROF_UPDATE_TEXTURE_PERFRAME, false );
	}
	else if ( !Q_stricmp( pArg, "texture_frame" ) )
	{
		g_VProfCurrentProfile.VXEnableUpdateMode( VPROF_UPDATE_TEXTURE_PERFRAME, true );
		g_VProfCurrentProfile.VXEnableUpdateMode( VPROF_UPDATE_TEXTURE_GLOBAL, false );
	}
}

CON_COMMAND( vx_vprof_nodeslist, "" )
{
	g_VProfCurrentProfile.VXSendNodes();
}
#endif

#ifdef _X360
DEFERRED_CON_COMMAND( vprof_360_enable_counters, "Enable 360 L2 and LHS counters for a node" )
{
	g_VProfCurrentProfile.Pause();
	if ( g_VProfCurrentProfile.PMCEnableL2Upon(g_szDefferedArg1 ) )
	{
		g_VProfCurrentProfile.DumpEnabledPMCNodes();
		// Msg("PMC enabled for only node %s\n", g_szDefferedArg1);
	}
	else
	{
		Warning( "Node not found.\n" );
	}
	g_VProfCurrentProfile.Resume();
}

DEFERRED_CON_COMMAND( vprof_360_enable_counters_recursive, "Enable 360 L2 and LHS counters for a node and all subnodes" )
{
	g_VProfCurrentProfile.Pause();
	if ( g_VProfCurrentProfile.PMCEnableL2Upon( g_szDefferedArg1, true ) )
	{
		g_VProfCurrentProfile.DumpEnabledPMCNodes();
		// Msg("PMC enabled for only node %s\n", g_szDefferedArg1);
	}
	else
	{
		Warning( "Node not found.\n" );
	}
	g_VProfCurrentProfile.Resume();
}

DEFERRED_CON_COMMAND( vprof_360_disable_counters, "Disable 360 L2 and LHS counters for a node. Specify 'all' to mean all nodes." )
{
	g_VProfCurrentProfile.Pause();
	if ( stricmp( g_szDefferedArg1, "all" ) == 0 )
	{
		g_VProfCurrentProfile.PMCDisableAllNodes();
	}
	else
	{
		if ( g_VProfCurrentProfile.PMCDisableL2Upon( g_szDefferedArg1, false ) )
		{
			g_VProfCurrentProfile.DumpEnabledPMCNodes();
			// Msg("PMC enabled for only node %s\n", g_szDefferedArg1);
		}
		else
		{
			Warning( "Node not found.\n" );
		}
	}
	g_VProfCurrentProfile.Resume();
}

DEFERRED_CON_COMMAND( vprof_360_disable_counters_recursive, "Disable 360 L2 and LHS counters for a node and all children." )
{
	g_VProfCurrentProfile.Pause();

	if ( g_VProfCurrentProfile.PMCDisableL2Upon( g_szDefferedArg1, true) )
	{
		g_VProfCurrentProfile.DumpEnabledPMCNodes();
		// Msg("PMC enabled for only node %s\n", g_szDefferedArg1);
	}
	else
	{
		Warning( "Node not found.\n" );
	}

	g_VProfCurrentProfile.Resume();
}

DEFERRED_CON_COMMAND( vprof_360_report_counters, "Report L2/LHS info for specified node" )
{
	g_VProfCurrentProfile.Pause();
	ConsoleLogger consoleLog;

	CVProfNode *pNode = g_VProfCurrentProfile.FindNode( g_VProfCurrentProfile.GetRoot(), g_szDefferedArg1 );
	if ( pNode )
	{
		Msg( "NODE %s\n\tL2 misses: %d\n\tLHS misses: %d\n", g_szDefferedArg1, pNode->GetL2CacheMisses(), pNode->GetLoadHitStores() );
	}
	else
	{
		Warning( "Node %s not found.", g_szDefferedArg1 );
	}

	g_VProfCurrentProfile.Resume();
}

DEFERRED_CON_COMMAND( vprof_360_cpu_trace_enable, "Usage: vprof_360_cpu_trace_enable <\"node\">. Enable CPU tracing during scope of node. Do this before calling vprof_360_cpu_trace_go." )
{	
	CVProfNode *RESTRICT upon = g_VProfCurrentProfile.CPUTraceEnableForNode( g_szDefferedArg1 );
	if ( upon )
	{
		Msg( "%s will be traced from start to end. Make sure vprof is enabled, and enter \nvprof_360_cpu_trace_go <filename> to engage!\n", upon->GetName() );
	}
	else
	{
		Warning( "Missing node %s. Run VProf to instance the node and wrap in \"double-quotes\". \n", g_szDefferedArg1 );
	}
}

DEFERRED_CON_COMMAND( vprof_360_cpu_trace_disable, "Disable CPU tracing on all nodes." )
{
	g_VProfCurrentProfile.CPUTraceDisableAllNodes();
	g_VProfCurrentProfile.SetCPUTraceEnabled( CVProfile::kDisabled );
}

DEFERRED_CON_COMMAND( vprof_360_cpu_trace_go, "Usage: vprof_360_cpu_trace_go <filename>. Will record one CPU trace of the node specified in vprof_360_cpu_trace_enable, dumping it to e:/filename.pix2." )
{
	if ( !g_fVprofOnByUI )
	{
		Msg( "VProf enabled.\n" );
		g_VProfCurrentProfile.Start();
		g_fVprofOnByUI = true;

		ConVarRef mat_vsyncref( "mat_vsync" );
		if ( mat_vsyncref.GetBool() )
		{
			Warning( "Disabling vsync (via mat_vsync) to increase profiling accuracy.\n" );
			mat_vsyncref.SetValue( false );
		}
	}

	if ( g_VProfCurrentProfile.CPUTraceGetEnabledNode() == NULL || g_VProfCurrentProfile.CPUTraceGetEnabledNode() == g_VProfCurrentProfile.GetRoot() )
	{
		Msg( "Defaulting PIX trace node to CEngine::Frame\n" );
		g_VProfCurrentProfile.CPUTraceEnableForNode( "CEngine::Frame" );
	}

	if ( g_VProfCurrentProfile.CPUTraceGetEnabledNode() != NULL )
	{
		if ( !g_szDefferedArg1[0] )
		{
			SYSTEMTIME systemTime;
			GetLocalTime( &systemTime );
			V_snprintf( g_szDefferedArg1, ARRAYSIZE(g_szDefferedArg1), "vprof_%d_%d_%d_%d_%d_%d", systemTime.wMonth, systemTime.wDay, systemTime.wHour, systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds );
		}
		const char *filename = g_VProfCurrentProfile.SetCPUTraceFilename( g_szDefferedArg1 );
		g_VProfCurrentProfile.SetCPUTraceEnabled( CVProfile::kFirstHitNode );
		Msg( "Trace will be written to %s\n", filename );
	}
	else
	{
		Warning( "Usage: vprof_360_cpu_trace_enable <\"node\">.\n" );
	}
}

DEFERRED_CON_COMMAND( vprof_360_cpu_trace_go_repeat, "Usage: vprof_360_cpu_trace_go_repeat <filename>. For each time the node specified in vprof_360_cpu_trace_enable is hit during the next frame, dump a CPU trace to e:/filenameXXXX.pix2." )
{
	if ( g_VProfCurrentProfile.CPUTraceGetEnabledNode() != NULL )
	{
		const char *filename = g_VProfCurrentProfile.SetCPUTraceFilename( g_szDefferedArg1 );
		g_VProfCurrentProfile.SetCPUTraceEnabled( CVProfile::kAllNodesInFrame_WaitingForMark );
		Msg( "Trace will be written to %s%.4d ... \n", filename, g_VProfCurrentProfile.GetMultiTraceIndex() );
	}
	else
	{
		Warning( "Usage: vprof_360_cpu_trace_enable <\"node\">.\n" );
	}
}


DEFERRED_CON_COMMAND( vprof_360_cpu_trace_go_multiframe, "Usage: vprof_360_cpu_trace_go_multiframe <framecount> <filename>. For each time the node specified in vprof_360_cpu_trace_enable is hit during the next frame, dump a CPU trace to e:/filenameXXXX.pix2." )
{
	int nNumFrames = Q_atoi( g_szDefferedArg1 );
	if ( nNumFrames >= 1 && nNumFrames < 1000 )
	{
		if ( g_VProfCurrentProfile.CPUTraceGetEnabledNode() != NULL )
		{
			const char *filename = g_VProfCurrentProfile.SetCPUTraceFilename( g_szDefferedArg2 );
			g_VProfCurrentProfile.SetCPUTraceEnabled( CVProfile::kAllNodesInFrame_WaitingForMarkMultiFrame, true, nNumFrames );
			Msg( "Trace will be written to %s%.4d ... \n", filename, g_VProfCurrentProfile.GetMultiTraceIndex() );
			return;
		}
	}
	
	Warning( "Usage: vprof_360_cpu_trace_go_multiframe <framecount> <filename>.\n" );
}


DEFERRED_CON_COMMAND( vx_vprof_trace, "" )
{
	CVProfNode *RESTRICT pNode = g_VProfCurrentProfile.CPUTraceEnableForNode( g_szDefferedArg1 );
	if ( !pNode )
	{
		Warning( "vx_vprof_trace: Missing Node %s\n", g_szDefferedArg1 );
		return;
	}

	vprof_on_Impl();
	g_VProfCurrentProfile.SetCPUTraceFilename( "capture" );
	g_VProfCurrentProfile.SetCPUTraceEnabled( CVProfile::kFirstHitNode, true );
}
#endif



// ------------------------------------------------------------------------------------------------------------------------------------ //
// Exports for the dedicated server UI.
// ------------------------------------------------------------------------------------------------------------------------------------ //
class CVProfExport : public IVProfExport
{
public:

	CVProfExport()
	{
		m_nListeners = 0;
		m_bStart = m_bStop = false;
		m_BudgetFlagsFilter = 0;
	}

	inline CVProfile* GetActiveVProfile()
	{
		return g_pVProfileForDisplay;
	}
	
	inline bool CanShowBudgetGroup( int iGroup )
	{
		return ( GetActiveVProfile()->GetBudgetGroupFlags( iGroup ) & m_BudgetFlagsFilter ) != 0;
	}

	virtual void AddListener()
	{
		++m_nListeners;
		if ( m_nListeners == 1 )
			m_bStart = true;		// Defer the command till vprof is ready.
	}

	virtual void RemoveListener()
	{
		--m_nListeners;
		if ( m_nListeners == 0 )
			m_bStop = true;			// Defer the command till vprof is ready.
	}

	virtual void SetBudgetFlagsFilter( int filter )
	{
		m_BudgetFlagsFilter = filter;
	}

	virtual int GetNumBudgetGroups()
	{
		int nTotalGroups = MIN( m_Times.Count(), GetActiveVProfile()->GetNumBudgetGroups() );
		int nRet = 0;
		for ( int i=0; i < nTotalGroups; i++ )
		{
			if ( CanShowBudgetGroup( i ) )
				++nRet;
		}
		return nRet;
	}

	virtual void GetBudgetGroupInfos( CExportedBudgetGroupInfo *pInfos )
	{
		int iOut = 0;
		int nTotalGroups = MIN( m_Times.Count(), GetActiveVProfile()->GetNumBudgetGroups() );
		for ( int i=0; i < nTotalGroups; i++ )
		{
			if ( CanShowBudgetGroup( i ) )
			{
				pInfos[iOut].m_pName = GetActiveVProfile()->GetBudgetGroupName( i );
				
				int red, green, blue, alpha;
				GetActiveVProfile()->GetBudgetGroupColor( i, red, green, blue, alpha );
				pInfos[iOut].m_Color = Color( red, green, blue, alpha );

				pInfos[iOut].m_BudgetFlags = GetActiveVProfile()->GetBudgetGroupFlags( i );
				++iOut;
			}
		}
	}

	virtual void GetBudgetGroupTimes( float times[IVProfExport::MAX_BUDGETGROUP_TIMES] )
	{
		int nTotalGroups = MIN( m_Times.Count(), GetActiveVProfile()->GetNumBudgetGroups() );
		int nGroups = MIN( nTotalGroups, IVProfExport::MAX_BUDGETGROUP_TIMES );
		memset( times, 0, sizeof( times[0] ) * nGroups );

		int iOut = 0;
		for ( int i=0; i < nTotalGroups; i++ )
		{
			if ( CanShowBudgetGroup( i ) )
			{
				times[iOut] = m_Times[i];
				++iOut;
			}
		}
	}

	void GetAllBudgetGroupTimes( float *pTimes )
	{
		int nTotalGroups = GetActiveVProfile()->GetNumBudgetGroups();
		for ( int i=0; i < nTotalGroups; i++ )
		{
			pTimes[i] = CanShowBudgetGroup( i ) ? m_Times[i] : 0.0f;
		}
	}

	virtual void PauseProfile()
	{
		if ( materials )
			materials->Flush();

		g_VProfCurrentProfile.Pause();
	}

	virtual void ResumeProfile()
	{
		if ( materials )
			materials->Flush();
		
		g_VProfCurrentProfile.Resume();
	}		


public:

	void StartOrStop()
	{
		if ( m_bStart )
		{
			g_VProfCurrentProfile.Start();
			m_bStart = false;
		}

		if ( m_bStop )
		{
			g_VProfCurrentProfile.Stop();
			m_bStop = false;
		}
	}

	void CalculateBudgetGroupTimes_Recursive( CVProfNode *pNode )
	{
		// If this node's info is filtered out, then put it in its parent's budget group.
		CVProfNode *pTestNode = pNode;
		while ( pTestNode != GetActiveVProfile()->GetRoot() && 
				( !CanShowBudgetGroup( pTestNode->GetBudgetGroupID() ) || 
				    ( GetActiveVProfile()->GetBudgetGroupFlags( pTestNode->GetBudgetGroupID() ) & BUDGETFLAG_HIDDEN ) != 0 ) )
		{
			pTestNode = pTestNode->GetParent();
		}

		int groupID = pTestNode->GetBudgetGroupID();
		double nodeTime = pNode->GetPrevTimeLessChildren();
		if ( groupID >= 0 && groupID < MIN( m_Times.Count(), IVProfExport::MAX_BUDGETGROUP_TIMES ) )
		{
			m_Times[groupID] += nodeTime;
		}
		else
		{
			Assert( false );
		}

		if( pNode->GetSibling() )
		{
			CalculateBudgetGroupTimes_Recursive( pNode->GetSibling() );
		}
		if( pNode->GetChild() )
		{
			CalculateBudgetGroupTimes_Recursive( pNode->GetChild() );
		}

		if ( !VProfRecord_IsPlayingBack() )
		{
			pNode->ClearPrevTime();
		}
	}

	void SnapshotVProfHistory()
	{
		// Don't do the work if there are no listeners.
		if ( !GetActiveVProfile()->IsEnabled() )
			return;
	
		if ( m_Times.Count() < GetActiveVProfile()->GetNumBudgetGroups() )
		{
			m_Times.SetSize( GetActiveVProfile()->GetNumBudgetGroups() );
		}

		memset( m_Times.Base(), 0, sizeof( m_Times[0] ) * GetActiveVProfile()->GetNumBudgetGroups() );
		CVProfNode *pNode = GetActiveVProfile()->GetRoot();
		if( pNode && pNode->GetChild() )
		{
			CalculateBudgetGroupTimes_Recursive( pNode->GetChild() );
		}
	}

private:
	CUtlVector<float> m_Times;	// Times from the most recent snapshot.
	int m_nListeners;
	int m_BudgetFlagsFilter;	// We can only capture one type of filtered data at a time.
	bool m_bStart;
	bool m_bStop;
};

CVProfExport g_VProfExport;
IVProfExport *g_pVProfExport = &g_VProfExport;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CVProfExport, IVProfExport, VPROF_EXPORT_INTERFACE_VERSION, g_VProfExport );

void VProfExport_SnapshotVProfHistory()
{
	g_VProfExport.SnapshotVProfHistory();
}

void VProfExport_StartOrStop()
{
	g_VProfExport.StartOrStop();
}

// Used by rpt
void VProfExport_Pause()
{
	g_VProfExport.PauseProfile();
}

void VProfExport_Resume()
{
	g_VProfExport.ResumeProfile();
}


//-----------------------------------------------------------------------------
// Used to point the budget panel at remote data
//-----------------------------------------------------------------------------
void OverrideVProfExport( IVProfExport *pExport )
{
	if ( g_pVProfExport == &g_VProfExport )
	{
		g_pVProfExport = pExport;
	}
}

void ResetVProfExport( IVProfExport *pExport )
{
	if ( g_pVProfExport == pExport )
	{
		g_pVProfExport = &g_VProfExport;
	}
}


//-----------------------------------------------------------------------------
// Listener to vprof data
//-----------------------------------------------------------------------------
struct VProfListenInfo_t
{
	ra_listener_id m_nListenerId;
	float m_flLastSentVProfDataTime;
	CUtlVector< CUtlString > m_SentGroups;

	VProfListenInfo_t() : m_flLastSentVProfDataTime( 0.0f ) {}
	VProfListenInfo_t( ra_listener_id nListenerId ) : m_nListenerId( nListenerId ), m_flLastSentVProfDataTime( 0.0f ) {}
	bool operator==( const VProfListenInfo_t& src ) const { return src.m_nListenerId == m_nListenerId; }

private:
	VProfListenInfo_t( const VProfListenInfo_t& src );
};

static CUtlVector<VProfListenInfo_t> s_VProfListeners;


//-----------------------------------------------------------------------------
// Purpose: serialize and send data to remote listeners
//-----------------------------------------------------------------------------
static int FindSentGroupIndex( VProfListenInfo_t &info, const char *pGroupName )
{
	int nCount = info.m_SentGroups.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_strcmp( pGroupName, info.m_SentGroups[i].Get() ) )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: serialize and send data to remote listeners
//-----------------------------------------------------------------------------
void WriteRemoteVProfGroupData( VProfListenInfo_t &info )
{
	if ( IsX360() )
		return;

	int nGroupCount = g_pVProfileForDisplay->GetNumBudgetGroups();
	int nInitialCount = info.m_SentGroups.Count();

	// Build list of unsent groups to send
	int nSendCount = 0;
	int *pIndex = (int*)stackalloc( nGroupCount * sizeof(int) ); 
	for ( int i = 0; i < nGroupCount; ++i )
	{
		const char *pName = g_pVProfileForDisplay->GetBudgetGroupName( i );
		if ( FindSentGroupIndex( info, pName ) >= 0 )
			continue;
		int j = info.m_SentGroups.AddToTail();
		info.m_SentGroups[j] = pName;
		pIndex[nSendCount++] = i;
	}

	if ( nSendCount == 0 )
		return;

	CUtlBuffer buf( 1024, 1024 );
	buf.PutInt( nInitialCount );
	buf.PutInt( nSendCount );

	for ( int i=0; i < nSendCount; i++ )
	{
		int nIndex = pIndex[i];
		int red, green, blue, alpha;
		g_pVProfileForDisplay->GetBudgetGroupColor( nIndex, red, green, blue, alpha );
		buf.PutUnsignedChar( (unsigned char)red );
		buf.PutUnsignedChar( (unsigned char)green );
		buf.PutUnsignedChar( (unsigned char)blue );
		buf.PutUnsignedChar( (unsigned char)alpha );

		const char *pName = g_pVProfileForDisplay->GetBudgetGroupName( nIndex );
		buf.PutString( pName );
	}

	g_ServerRemoteAccess.SendVProfData( info.m_nListenerId, true, buf.Base(), buf.TellMaxPut() );
}
		 
static ConVar rpt_vprof_time( "rpt_vprof_time","0.25", FCVAR_HIDDEN | FCVAR_DONTRECORD, "" );
void WriteRemoteVProfData()
{
	if ( IsX360() )
		return;

	// Throttle sending too much data
	float flMaxDelta = rpt_vprof_time.GetFloat();
	float flTime = Plat_FloatTime();
	bool bShouldSend = false;
	int nListenerCount = s_VProfListeners.Count();
	for( int i = 0; i < nListenerCount; i++ )
	{
		if ( flTime - s_VProfListeners[i].m_flLastSentVProfDataTime >= flMaxDelta )
		{
			bShouldSend = true;
			break;
		}
	}

	if ( !bShouldSend )
		return;

	int nGroupCount = g_pVProfileForDisplay->GetNumBudgetGroups();
	int nBufSize = nGroupCount * sizeof(float);
	float *pTimes = (float*)stackalloc( nBufSize );
	g_VProfExport.GetAllBudgetGroupTimes( pTimes );

	for( int i = 0; i < nListenerCount; i++ )
	{
		if ( flTime - s_VProfListeners[i].m_flLastSentVProfDataTime < flMaxDelta )
			continue;

		WriteRemoteVProfGroupData( s_VProfListeners[i] );
		s_VProfListeners[i].m_flLastSentVProfDataTime = flTime;

		// Re-order send times to match send group order
		int nSentSize = s_VProfListeners[i].m_SentGroups.Count() * sizeof(float);
		float *pSentTimes = (float*)stackalloc( nSentSize );
		memset( pSentTimes, 0, nSentSize );
		for ( int j = 0; j < nGroupCount; ++j )
		{
			int nIndex = FindSentGroupIndex( s_VProfListeners[i], g_pVProfileForDisplay->GetBudgetGroupName( j ) );
			Assert( nIndex >= 0 );
			pSentTimes[ nIndex ] = pTimes[j];
		}
		g_ServerRemoteAccess.SendVProfData( s_VProfListeners[i].m_nListenerId, false, pSentTimes, nSentSize );
	}
}


//-----------------------------------------------------------------------------
// Purpose: add a new endpoint to send data to 
//-----------------------------------------------------------------------------
void RegisterVProfDataListener( ra_listener_id listenerID )
{
	RemoveVProfDataListener( listenerID );
	int nIndex = s_VProfListeners.AddToTail( );
	s_VProfListeners[nIndex].m_nListenerId = listenerID;
	g_VProfExport.AddListener();
	WriteRemoteVProfGroupData( s_VProfListeners[nIndex] );
}


//-----------------------------------------------------------------------------
// Purpose: remove an endpoint we are sending data to
//-----------------------------------------------------------------------------
void RemoveVProfDataListener( ra_listener_id listenerID )
{
	VProfListenInfo_t findInfo( listenerID );
	if ( s_VProfListeners.FindAndRemove( findInfo ) )
	{
		g_VProfExport.RemoveListener();
	}
}

#endif

