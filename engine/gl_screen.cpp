//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: master for refresh, status bar, console, chat, notify, etc
//
//=====================================================================================//


#include "render_pch.h"
#include "client.h"
#include "console.h"
#include "screen.h"
#include "sound.h"
#include "sbar.h"
#include "debugoverlay.h"
#include "cdll_int.h"
#include "gl_matsysiface.h"
#include "cdll_engine_int.h"
#include "demo.h"
#include "cl_main.h"
#include "vgui_baseui_interface.h"
#include "con_nprint.h"
#include "sys_mainwind.h"
#include "ivideomode.h"
#include "lightcache.h"
#include "toolframework/itoolframework.h"
#include "datacache/idatacache.h"
#include "sys_dll.h"
#include "host.h"
#include "MapReslistGenerator.h"
#include "tier1/callqueue.h"
#include "tier0/icommandline.h"
#include "matchmaking/imatchframework.h"
#include "cl_steamauth.h"

#include "scaleformui/scaleformui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar g_cv_miniprofiler_dump( "miniprofiler_dump", "0" );
#if defined( _X360 )
ConVar g_cv_frame_pcm( "frame_pcm", "0" );
bool g_started_frame_pcm = false;
#endif
DLL_IMPORT void PublishAllMiniProfilers(int nHistoryMax);

// In other C files.
extern bool V_CheckGamma( void );
extern void	V_RenderView( void );
extern void V_RenderVGuiOnly( void );

extern bool HostState_IsTransitioningToLoad();

bool		scr_initialized;		// ready to draw
bool		scr_disabled_for_loading;
bool		scr_drawloading;
int			scr_nextdrawtick;		// A hack to let things settle on reload/reconnect
float		scr_loadingStartTime;

static bool	scr_engineevent_loadingstarted;	// whether OnEngineLevelLoadingStarted has been fired

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SCR_Init (void)
{
	scr_initialized = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SCR_Shutdown( void )
{
	scr_initialized = false;
}

//-----------------------------------------------------------------------------
// Purpose: starts loading
//-----------------------------------------------------------------------------
void SCR_BeginLoadingPlaque( const char *levelName /*= NULL*/ )
{
	if ( !scr_drawloading )
	{
		MEM_ALLOC_CREDIT();

#if defined( _DEMO ) && defined( _X360 )
		// disable demo timeouts during loading
		Host_EnableDemoTimeout( false );
#endif

		scr_loadingStartTime = Plat_FloatTime();

		// make sure game UI is allowed to show (gets disabled if chat window is up)
		EngineVGui()->SetNotAllowedToShowGameUI( false );

		// force QMS to serialize during loading
		Host_AllowQueuedMaterialSystem( false );

		scr_drawloading = true;

		S_StopAllSounds( true );
		S_PreventSound( true ); //this will stop audio from reaching the mixer until SCR_EndLoadingPlaque is called.
		S_OnLoadScreen( true );

		g_pFileSystem->AsyncFinishAll();
		g_pMDLCache->FinishPendingLoads();

		// redraw with no console and the loading plaque
		Con_ClearNotify();

		// NULL HudText clears HudMessage system
		if ( g_ClientDLL )
		{
			g_ClientDLL->CenterStringOff();
			g_ClientDLL->HudText( NULL );
		}

		// let everybody know we're starting loading
		EngineVGui()->OnLevelLoadingStarted( levelName, HostState_IsTransitioningToLoad() );
		scr_engineevent_loadingstarted = true;
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnEngineLevelLoadingStarted", "name", levelName ) );

		// Don't run any more simulation on the client!!!
		g_ClientGlobalVariables.frametime = 0.0f;

		host_framecount++;
		g_ClientGlobalVariables.framecount = host_framecount;
		// Ensure the screen is painted to reflect the loading state
		SCR_UpdateScreen();
		host_framecount++;
		g_ClientGlobalVariables.framecount = host_framecount;
		SCR_UpdateScreen();

		g_ClientGlobalVariables.frametime = GetBaseLocalClient().GetFrameTime();

		scr_disabled_for_loading = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: finished loading
//-----------------------------------------------------------------------------
void SCR_EndLoadingPlaque( void )
{
	// MATCHMAKING:UNDONE: This pattern came over from l4d but needed to change since the new clients don't have the same mission/game structure
	if ( scr_drawloading )
	{
#if defined( _DEMO ) && defined( _X360 )
		// allow demo timeouts
		Host_EnableDemoTimeout( true );
#endif

		scr_engineevent_loadingstarted = false;

		EngineVGui()->HideLoadingPlaque();

		if ( g_pMatchFramework )
		{
			scr_engineevent_loadingstarted = false;
			EngineVGui()->HideLoadingPlaque();

			KeyValues *kv = new KeyValues( "OnEngineLevelLoadingFinished" );
			if ( gfExtendedError )
			{
				kv->SetInt( "error", gfExtendedError );
				kv->SetString( "reason", gszDisconnectReason );
			}

#ifndef NO_TOOLFRAMEWORK
			if ( toolframework->InToolMode() )
			{				
				// Make a copy of the message to send to the tools framework
				KeyValues *pToolMsg = kv->MakeCopy();

				// Notify the active tool that the level has finished loading
				toolframework->PostMessage( pToolMsg );
				pToolMsg->deleteThis();
			}
#endif

			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( kv );
			return;
		}
		return;
	}
	else if ( gfExtendedError )
	{
#if !defined( CSTRIKE15 )
		if ( IsPC() )
		{
			EngineVGui()->ShowErrorMessage();
		}
#endif
	}

	if ( scr_engineevent_loadingstarted )
	{
		// Keep firing Tick event only between LoadingStarted and LoadingFinished
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnEngineLevelLoadingTick" ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: This is called every frame, and can also be called explicitly to flush
//  text to the screen.
//-----------------------------------------------------------------------------
void SCR_UpdateScreen( void )
{
	R_StudioCheckReinitLightingCache();

	// Always force the Gamma Table to be rebuilt. Otherwise,
	// we'll load textures with an all white gamma lookup table.
	V_CheckGamma();

	// This is a HACK to let things settle for a bit on level start
	// NOTE: If you remove scr_nextdrawtick, remove it from enginetool.cpp too
	if ( scr_nextdrawtick != 0 )
	{
		if ( host_tickcount < scr_nextdrawtick )
			return;

		scr_nextdrawtick = 0;
	}

	if ( scr_disabled_for_loading )
	{
		if ( !Host_IsSinglePlayerGame() )
		{
			V_RenderVGuiOnly();
		}

		// Put this here to avoid a crappy alt+tab situation:
		// 1.  g_LostVideoMemory is only cleared if we can call CheckDeviceLost
		// 2.  CheckDeviceLost can only get called if we present
		// 3.  Present can only get called in SCR_UpdateScreen if scr_disabled_for_loading is cleared
		// 4.  scr_disabled_for_loading can only be cleared if we disable the loading plaque
		// 5.  The loading plaque can only be disabled if we get a call to CL_FullyConnected
		// 6.  CL_FullyConnected only gets called if we get a snapshot from the server
		// 7.  In single player we only send snapshots if g_LostVideoMemory is cleared
		// 8.  goto step 1
		if ( !IsGameConsole() && g_LostVideoMemory )
		{
			Shader_SwapBuffers();
		}

		return;
	}

	if ( !scr_initialized || !con_initialized )
	{
		// not initialized yet
		return;				
	}

	// Let demo system overwrite view origin/angles during playback
	if ( demoplayer->IsPlayingBack() )
	{
		demoplayer->InterpolateViewpoint();
	}

	materials->BeginFrame( host_frametime );

	CMatRenderContextPtr pRenderContext;
	pRenderContext.GetFrom( materials );

	pRenderContext->RenderScaleformSlot(SF_RESERVED_BEGINFRAME_SLOT);


	if( EngineVGui()->IsGameUIVisible() || IsSteam3ClientGameOverlayActive() )
	{
		pRenderContext->AntiAliasingHint( AA_HINT_MENU );
	}

	if ( pRenderContext->GetCallQueue() )
	{
		pRenderContext->GetCallQueue()->QueueCall( g_pMDLCache, &IMDLCache::BeginCoarseLock );
	}
	pRenderContext.SafeRelease();

	EngineVGui()->Simulate();

	{
		CMatRenderContextPtr pRenderContext( materials );
		PIXEVENT( pRenderContext, "framestagenotify" );
		ClientDLL_FrameStageNotify( FRAME_RENDER_START );
	}

	Host_BeginThreadedSound();

	// Simulation meant to occur before any views are rendered
	// This needs to happen before the client DLL is called because the client DLL depends on 
	// some of the setup in FRAME_RENDER_START.
	g_EngineRenderer->FrameBegin();
	toolframework->RenderFrameBegin();

	GetBaseLocalClient().UpdateAreaBits_BackwardsCompatible();

	Shader_BeginRendering();

	// Draw world, etc.
	V_RenderView();

	pRenderContext.GetFrom( materials );
	pRenderContext->RenderScaleformSlot(SF_RESERVED_ENDFRAME_SLOT);
	pRenderContext.SafeRelease();

	CL_TakeSnapshotAndSwap();	   

	ClientDLL_FrameStageNotify( FRAME_RENDER_END );

	toolframework->RenderFrameEnd();

	g_EngineRenderer->FrameEnd();

	pRenderContext.GetFrom( materials );
	if ( pRenderContext->GetCallQueue() )
	{
		pRenderContext->GetCallQueue()->QueueCall( g_pMDLCache, &IMDLCache::EndCoarseLock );
	}
	pRenderContext.SafeRelease();

	materials->EndFrame();

#if !defined( _CERT )
	PublishAllMiniProfilers( g_cv_miniprofiler_dump.GetInt() );
#if defined( _X360 )
	if ( g_started_frame_pcm )
	{
		g_started_frame_pcm = false;
		PMCStopAndReport();
	}
	if ( g_cv_frame_pcm.GetInt() )
	{
		g_cv_frame_pcm.SetValue("0");
		g_started_frame_pcm = true;
		PMCInstallAndStart(ePMCSetup(PMC_SETUP_OVERVIEW_PB0T0 + GetCurrentProcessorNumber()));
	}
#endif
#endif

	// NOTE: It isn't super awesome to do this here, but it has to occur after
	// the client DLL where it knows its read in all entities, which happens in
	// ClientDLL_FrameStageNotify( FRAME_RENDER_START )
#ifndef DEDICATED
	if ( IsPC() )
	{
		static bool s_bTestedBuildCubemaps = false;
		CClientState &cl = GetBaseLocalClient();
		if ( !s_bTestedBuildCubemaps && cl.IsActive() )
		{
			s_bTestedBuildCubemaps = true;

			int i;
			if( (i = CommandLine()->FindParm( "-buildcubemaps" )) != 0 )
			{
				int numIterations = 1;
				if( CommandLine()->ParmCount() > i + 1 )
				{
					numIterations = atoi( CommandLine()->GetParm(i+1) );
				}
				if( numIterations == 0 )
				{
					numIterations = 1;
				}
				char cmd[ 1024 ] = { 0 };
				V_snprintf( cmd, sizeof( cmd ), "buildcubemaps %u;quit\n", numIterations );
				Cbuf_AddText( Cbuf_GetCurrentPlayer(), cmd );
			}
			else if( CommandLine()->FindParm( "-buildmodelforworld" ) )
			{
				Cbuf_AddText( Cbuf_GetCurrentPlayer(), "buildmodelforworld;quit\n" );
			}
			else if( CommandLine()->FindParm( "-navanalyze" ) )
			{
				Cbuf_AddText( Cbuf_GetCurrentPlayer(), "sv_cheats 1;nav_edit 1;nav_analyze_scripted\n" );
			}
			else if( CommandLine()->FindParm( "-navforceanalyze" ) )
			{
				Cbuf_AddText( Cbuf_GetCurrentPlayer(), "sv_cheats 1;nav_edit 1;nav_analyze_scripted force\n" );
			}
			else if ( CommandLine()->FindParm("-exit") )
			{
				Cbuf_AddText( Cbuf_GetCurrentPlayer(), "quit\n" );
			}
		}
	}
#endif
}
