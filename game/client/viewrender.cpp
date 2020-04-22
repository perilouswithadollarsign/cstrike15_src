//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Responsible for drawing the scene
//
//===========================================================================//

#include "cbase.h"
#include "view.h"
#include "iviewrender.h"
#include "view_shared.h"
#include "ivieweffects.h"
#include "iinput.h"
#include "model_types.h"
#include "clientsideeffects.h"
#include "particlemgr.h"
#include "viewrender.h"
#include "iclientmode.h"
#include "voice_status.h"
#include "glow_overlay.h"
#include "materialsystem/imesh.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/materialsystem_config.h"
#include "detailobjectsystem.h"
#include "tier0/vprof.h"
#include "tier0/perfstats.h"
#include "tier0/threadtools.h"
#include "tier1/mempool.h"
#include "vstdlib/jobthread.h"
#include "datacache/imdlcache.h"
#include "engine/IEngineTrace.h"
#include "engine/ivmodelinfo.h"
#include "tier0/icommandline.h"
#include "view_scene.h"
#include "particles_ez.h"
#include "engine/IStaticPropMgr.h"
#include "engine/ivdebugoverlay.h"
#include "c_pixel_visibility.h"
#include "precache_register.h"
#include "c_rope.h"
#include "c_effects.h"
#include "smoke_fog_overlay.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "vgui_int.h"
#include "ienginevgui.h"
#include "ScreenSpaceEffects.h"
#include "toolframework_client.h"
#include "c_func_reflective_glass.h"
#include "keyvalues.h"
#include "renderparm.h"
#include "modelrendersystem.h"
#include "vgui/ISurface.h"
#include "tier0/cache_hints.h"
#include "c_env_cascade_light.h"

#define PARTICLE_USAGE_DEMO									// uncomment to get particle bar thing



#ifdef PORTAL
#include "portal_render_targets.h"
#include "portalrender.h"
#endif
#if defined( HL2_CLIENT_DLL ) || defined( CSTRIKE_DLL ) || defined( INFESTED_DLL ) || defined( PORTAL2 )
#define USE_MONITORS
#endif
#include "rendertexture.h"
#include "viewpostprocess.h"
#include "viewdebug.h"

#ifdef USE_MONITORS
#include "c_point_camera.h"
#endif // USE_MONITORS

#ifdef INFESTED_DLL
#include "c_asw_render_targets.h"
#include "clientmode_asw.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static void testfreezeframe_f( void )
{
	view->FreezeFrame( 3.0 );
}
static ConCommand test_freezeframe( "test_freezeframe", testfreezeframe_f, "Test the freeze frame code.", FCVAR_CHEAT );

//-----------------------------------------------------------------------------

static ConVar r_visocclusion( "r_visocclusion", "0", FCVAR_CHEAT );
extern ConVar r_flashlightdepthtexture;
extern ConVar vcollide_wireframe;
extern ConVar r_depthoverlay;
extern ConVar r_shadow_deferred;
extern ConVar mat_viewportscale;
extern ConVar mat_viewportupscale;

//-----------------------------------------------------------------------------
// Convars related to controlling rendering
//-----------------------------------------------------------------------------
static ConVar cl_maxrenderable_dist("cl_maxrenderable_dist", "3000", FCVAR_CHEAT, "Max distance from the camera at which things will be rendered" );

ConVar r_entityclips( "r_entityclips", "1" ); //FIXME: Nvidia drivers before 81.94 on cards that support user clip planes will have problems with this, require driver update? Detect and disable?
ConVar r_deferopaquefastclipped( "r_deferopaquefastclipped", "1" );

// Matches the version in the engine
static ConVar r_drawopaqueworld( "r_drawopaqueworld", "1", FCVAR_CHEAT );
static ConVar r_drawtranslucentworld( "r_drawtranslucentworld", "1", FCVAR_CHEAT );
static ConVar r_3dsky( "r_3dsky","1", 0, "Enable the rendering of 3d sky boxes" );
static ConVar r_skybox( "r_skybox","1", FCVAR_CHEAT, "Enable the rendering of sky boxes" );
ConVar r_drawviewmodel( "r_drawviewmodel","1", FCVAR_CHEAT );
static ConVar r_drawtranslucentrenderables( "r_drawtranslucentrenderables", "1", FCVAR_CHEAT );
static ConVar r_drawopaquerenderables( "r_drawopaquerenderables", "1", FCVAR_CHEAT );

static ConVar r_flashlightdepth_drawtranslucents( "r_flashlightdepth_drawtranslucents", "0", FCVAR_NONE );
extern ConVar cl_csm_translucent_shadows;
extern ConVar cl_csm_translucent_shadows_using_opaque_path;

ConVar r_flashlightvolumetrics( "r_flashlightvolumetrics", "1" );


// FIXME: This is not static because we needed to turn it off for TF2 playtests
ConVar r_DrawDetailProps( "r_DrawDetailProps", "1", 
#if defined( ALLOW_TEXT_MODE )
	FCVAR_RELEASE,
#else
	FCVAR_DEVELOPMENTONLY, 
#endif
	"0=Off, 1=Normal, 2=Wireframe" 
);

// don't use worldlistcache on PS3 is using SPU buildview jobs
ConVar r_worldlistcache( "r_worldlistcache", IsPS3() ? "0" : "1" );

extern ConVar cl_csm_shadows;
extern ConVar cl_csm_world_shadows;
extern ConVar cl_csm_world_shadows_in_viewmodelcascade;
extern ConVar cl_csm_disable_culling;
extern ConVar cl_csm_rope_shadows;

static ConVar r_drawunderwateroverlay( "r_drawunderwateroverlay", "0", FCVAR_CHEAT | FCVAR_SERVER_CAN_EXECUTE );
static ConVar r_drawscreenoverlay( "r_drawscreenoverlay", "1", FCVAR_CHEAT | FCVAR_SERVER_CAN_EXECUTE );

//-----------------------------------------------------------------------------
// Convars related to fog color
//-----------------------------------------------------------------------------
static void GetFogColor( fogparams_t *pFogParams, float *pColor, bool ignoreOverride = false, bool ignoreHDRColorScale = false );
static float GetFogMaxDensity( fogparams_t *pFogParams, bool ignoreOverride = false );
static bool GetFogEnable( fogparams_t *pFogParams, bool ignoreOverride = false );
static float GetFogStart( fogparams_t *pFogParams, bool ignoreOverride = false );
static float GetFogEnd( fogparams_t *pFogParams, bool ignoreOverride = false );
static float GetSkyboxFogStart( bool ignoreOverride = false );
static float GetSkyboxFogEnd( bool ignoreOverride = false );
static float GetSkyboxFogMaxDensity( bool ignoreOverride = false );
static void GetSkyboxFogColor( float *pColor, bool ignoreOverride = false, bool ignoreHDRColorScale = false );
static void FogOverrideCallback( IConVar *pConVar, char const *pOldString, float flOldValue );
static ConVar fog_override( "fog_override", "0", FCVAR_CHEAT, "Overrides the map's fog settings (-1 populates fog_ vars with map's values)", FogOverrideCallback );
// set any of these to use the maps fog
static ConVar fog_start( "fog_start", "-1", FCVAR_CHEAT );
static ConVar fog_end( "fog_end", "-1", FCVAR_CHEAT );
static ConVar fog_color( "fog_color", "-1 -1 -1", FCVAR_CHEAT );
static ConVar fog_enable( "fog_enable", "1", FCVAR_CHEAT );
static ConVar fog_startskybox( "fog_startskybox", "-1", FCVAR_CHEAT );
static ConVar fog_endskybox( "fog_endskybox", "-1", FCVAR_CHEAT );
static ConVar fog_maxdensityskybox( "fog_maxdensityskybox", "-1", FCVAR_CHEAT );
static ConVar fog_colorskybox( "fog_colorskybox", "-1 -1 -1", FCVAR_CHEAT );
static ConVar fog_enableskybox( "fog_enableskybox", "1", FCVAR_CHEAT );
static ConVar fog_maxdensity( "fog_maxdensity", "-1", FCVAR_CHEAT );
static ConVar fog_hdrcolorscale( "fog_hdrcolorscale", "-1", FCVAR_CHEAT );
static ConVar fog_hdrcolorscaleskybox( "fog_hdrcolorscaleskybox", "-1", FCVAR_CHEAT );
static void FogOverrideCallback( IConVar *pConVar, char const *pOldString, float flOldValue )
{
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !localPlayer )
		return;

	ConVarRef var( pConVar );
	if ( var.GetInt() == -1 )
	{
		fogparams_t *pFogParams = localPlayer->GetFogParams();

		float fogColor[3];
		fog_start.SetValue( GetFogStart( pFogParams, true ) );
		fog_end.SetValue( GetFogEnd( pFogParams, true ) );
		GetFogColor( pFogParams, fogColor, true, true );
		fog_color.SetValue( VarArgs( "%.1f %.1f %.1f", fogColor[0]*255, fogColor[1]*255, fogColor[2]*255 ) );
		fog_enable.SetValue( GetFogEnable( pFogParams, true ) );
		fog_startskybox.SetValue( GetSkyboxFogStart( true ) );
		fog_endskybox.SetValue( GetSkyboxFogEnd( true ) );
		fog_maxdensityskybox.SetValue( GetSkyboxFogMaxDensity( true ) );
		GetSkyboxFogColor( fogColor, true, true );
		fog_colorskybox.SetValue( VarArgs( "%.1f %.1f %.1f", fogColor[0]*255, fogColor[1]*255, fogColor[2]*255 ) );
		fog_enableskybox.SetValue( localPlayer->m_Local.m_skybox3d.fog.enable.Get() );
		fog_maxdensity.SetValue( GetFogMaxDensity( pFogParams, true ) );
		fog_hdrcolorscale.SetValue( pFogParams->HDRColorScale );
		fog_hdrcolorscaleskybox.SetValue( localPlayer->m_Local.m_skybox3d.fog.HDRColorScale.Get() );
	}
}

//-----------------------------------------------------------------------------
// Water-related convars
//-----------------------------------------------------------------------------
static ConVar r_debugcheapwater( "r_debugcheapwater", "0", FCVAR_CHEAT );
#ifndef _GAMECONSOLE
static ConVar r_waterforceexpensive( "r_waterforceexpensive", "0" );
#endif
static ConVar r_waterforcereflectentities( "r_waterforcereflectentities", "0" );

#if defined( _GAMECONSOLE ) && ( defined( PORTAL2 ) || defined( CSTRIKE15 ) )
// Portal 2 doesn't use refractive water in many places, and where it does, it's too expensive for consoles (and probably low-end PCs as well)
// Just force it off here so as not to mess with high-end PCs
static ConVar r_WaterDrawRefraction( "r_WaterDrawRefraction", IsPS3()? "0" : "1", 0, "Enable water refraction" );
#else
static ConVar r_WaterDrawRefraction( "r_WaterDrawRefraction", "1", 0, "Enable water refraction" );
#endif

static ConVar r_WaterDrawReflection( "r_WaterDrawReflection", "1", 0, "Enable water reflection" );

static ConVar r_ForceWaterLeaf( "r_ForceWaterLeaf", "1", 0, "Enable for optimization to water - considers view in leaf under water for purposes of culling" );
static ConVar mat_drawwater( "mat_drawwater", "1", FCVAR_CHEAT );
static ConVar mat_clipz( "mat_clipz", "1" );


//-----------------------------------------------------------------------------
// Other convars
//-----------------------------------------------------------------------------
static ConVar cl_drawmonitors( "cl_drawmonitors", "1" );
static ConVar r_eyewaterepsilon( "r_eyewaterepsilon", "7.0f", FCVAR_CHEAT );

extern ConVar cl_leveloverview;

ConVar r_fastzreject( "r_fastzreject", "0", 0, "Activate/deactivates a fast z-setting algorithm to take advantage of hardware with fast z reject. Use -1 to default to hardware settings" );

// For CSS15, simpleworldmodel_waterreflections don't work.  They were added for Portal 2.  If we want, we can look into making them work, until then, we don't enable it.
#if defined( _GAMECONSOLE )
ConVar r_simpleworldmodel_waterreflections_fullscreen( "r_simpleworldmodel_waterreflections_fullscreen", "0" );
ConVar r_simpleworldmodel_drawforrecursionlevel_fullscreen( "r_simpleworldmodel_drawforrecursionlevel_fullscreen", "-1" );
ConVar r_simpleworldmodel_drawbeyonddistance_fullscreen( "r_simpleworldmodel_drawbeyonddistance_fullscreen", "-1" );

ConVar r_simpleworldmodel_waterreflections_splitscreen( "r_simpleworldmodel_waterreflections_splitscreen", "0" );
ConVar r_simpleworldmodel_drawforrecursionlevel_splitscreen( "r_simpleworldmodel_drawforrecursionlevel_splitscreen", "2" );
ConVar r_simpleworldmodel_drawbeyonddistance_splitscreen( "r_simpleworldmodel_drawbeyonddistance_splitscreen", "600" );

ConVar r_simpleworldmodel_waterreflections_pip( "r_simpleworldmodel_waterreflections_pip", "1" );
ConVar r_simpleworldmodel_drawforrecursionlevel_pip( "r_simpleworldmodel_drawforrecursionlevel_pip", "2" );
ConVar r_simpleworldmodel_drawbeyonddistance_pip( "r_simpleworldmodel_drawbeyonddistance_pip", "600" );
#else
ConVar r_simpleworldmodel_waterreflections_fullscreen( "r_simpleworldmodel_waterreflections_fullscreen", "0" );
ConVar r_simpleworldmodel_drawforrecursionlevel_fullscreen( "r_simpleworldmodel_drawforrecursionlevel_fullscreen", "-1" );
ConVar r_simpleworldmodel_drawbeyonddistance_fullscreen( "r_simpleworldmodel_drawbeyonddistance_fullscreen", "-1" );

ConVar r_simpleworldmodel_waterreflections_splitscreen( "r_simpleworldmodel_waterreflections_splitscreen", "0" );
ConVar r_simpleworldmodel_drawforrecursionlevel_splitscreen( "r_simpleworldmodel_drawforrecursionlevel_splitscreen", "-1" );
ConVar r_simpleworldmodel_drawbeyonddistance_splitscreen( "r_simpleworldmodel_drawbeyonddistance_splitscreen", "-1" );

ConVar r_simpleworldmodel_waterreflections_pip( "r_simpleworldmodel_waterreflections_pip", "0" );
ConVar r_simpleworldmodel_drawforrecursionlevel_pip( "r_simpleworldmodel_drawforrecursionlevel_pip", "-1" );
ConVar r_simpleworldmodel_drawbeyonddistance_pip( "r_simpleworldmodel_drawbeyonddistance_pip", "-1" );
#endif

void GetSimpleWorldModelConfiguration( bool &bSimpleWorldModeWaterReflectionOut, int &nSimpleWorldModelRecursionLevelOut, float &flSimpleWorldModelDrawBeyondDistanceOut )
{
	// we only load/use the world imposters for multiplayer maps.
	if ( GameRules()->IsMultiplayer() || IsPC() )
	{
		if ( VGui_IsSplitScreen() )
		{
			if ( VGui_IsSplitScreenPIP() )
			{
				if ( GET_ACTIVE_SPLITSCREEN_SLOT() == 0 )
				{
					// We are the main view, so go ahead and use the full screen settings.
					// We definitely want to use the fullscreen setting here so that we don't pop when split goes off and on.
					bSimpleWorldModeWaterReflectionOut = r_simpleworldmodel_waterreflections_fullscreen.GetBool();
					nSimpleWorldModelRecursionLevelOut = r_simpleworldmodel_drawforrecursionlevel_fullscreen.GetInt();
					flSimpleWorldModelDrawBeyondDistanceOut = r_simpleworldmodel_drawbeyonddistance_fullscreen.GetFloat();
				}
				else
				{
					// We are not the primary view, so we must be PIP.
					bSimpleWorldModeWaterReflectionOut = r_simpleworldmodel_waterreflections_pip.GetBool();
					nSimpleWorldModelRecursionLevelOut = r_simpleworldmodel_drawforrecursionlevel_pip.GetInt();
					flSimpleWorldModelDrawBeyondDistanceOut = r_simpleworldmodel_drawbeyonddistance_pip.GetFloat();
				}
			}
			else
			{
				// We are one of two splitscreen views.
				bSimpleWorldModeWaterReflectionOut = r_simpleworldmodel_waterreflections_splitscreen.GetBool();
				nSimpleWorldModelRecursionLevelOut = r_simpleworldmodel_drawforrecursionlevel_splitscreen.GetInt();
				flSimpleWorldModelDrawBeyondDistanceOut = r_simpleworldmodel_drawbeyonddistance_splitscreen.GetFloat();
			}
		}
		else
		{
			// We aren't splitscreen of any sort, so go ahead and use the fullscreen setting.
			bSimpleWorldModeWaterReflectionOut = r_simpleworldmodel_waterreflections_fullscreen.GetBool();
			nSimpleWorldModelRecursionLevelOut = r_simpleworldmodel_drawforrecursionlevel_fullscreen.GetInt();
			flSimpleWorldModelDrawBeyondDistanceOut = r_simpleworldmodel_drawbeyonddistance_fullscreen.GetFloat();
		}
	}
	else
	{
		// We aren't multiplayer, so set the options that turn it all off.
		bSimpleWorldModeWaterReflectionOut = false;
		nSimpleWorldModelRecursionLevelOut = -1;
		flSimpleWorldModelDrawBeyondDistanceOut = -1.0f;
	}
}

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
static Vector g_vecCurrentRenderOrigin(0,0,0);
static QAngle g_vecCurrentRenderAngles(0,0,0);
static Vector g_vecCurrentVForward(0,0,0), g_vecCurrentVRight(0,0,0), g_vecCurrentVUp(0,0,0);
static VMatrix g_matCurrentCamInverse;
bool s_bCanAccessCurrentView = false;
IntroData_t *g_pIntroData = NULL;
static bool	g_bRenderingView = false;			// For debugging...
static int g_CurrentViewID = VIEW_NONE;
bool g_bRenderingScreenshot = false;

#if defined( CSTRIKE15 ) && defined(_PS3)
static ConVar r_PS3_2PassBuildDraw( "r_PS3_2PassBuildDraw", "1" );
static ConVar r_ps3_csm_disableWorldInListenServer( "r_ps3_csm_disableWorldInListenServer", "1" );

CConcurrentViewBuilderPS3 g_viewBuilder;

#define PROLOGUE_PASS_DRAWLISTS g_viewBuilder.SetDrawFlags( m_DrawFlags );

#define EPILOGUE_PASS_DRAWLISTS if( m_pWorldRenderList == NULL )\
{\
	m_pWorldRenderList = g_viewBuilder.GetWorldRenderListElement();\
}

#define SYNC_BUILDWORLD_JOB( bShadowDepth ) if( g_viewBuilder.IsSPUBuildRWJobsOn() )\
{\
	SNPROF("SyncBuildWorldJob");\
CELL_VERIFY( g_pBuildRenderablesJob->m_pRoot->m_queuePortBuildWorld[ g_viewBuilder.GetBuildViewID() ].sync( 0 ) );\
BuildWorldRenderLists_PS3_Epilogue( bShadowDepth );\
}

#define SYNC_BUILDRENDERABLES_JOB if( g_viewBuilder.IsSPUBuildRWJobsOn() )\
{\
	SNPROF("SyncBuildRenderablesJob");\
	CELL_VERIFY( g_pBuildRenderablesJob->m_pRoot->m_queuePortBuildRenderables[ g_viewBuilder.GetBuildViewID() ].sync( 0 ) );\
	BuildRenderableRenderLists_PS3_Epilogue();\
}


#define BEGIN_2PASS_BUILD_BLOCK if( g_viewBuilder.GetPassFlags() & PASS_BUILDLISTS_PS3 ) {
#define BEGIN_2PASS_DRAW_BLOCK if( g_viewBuilder.GetPassFlags() & PASS_DRAWLISTS_PS3 ) {
#define END_2PASS_BLOCK }


#define PS3_SPUPATH_INVALID( s ) if( g_viewBuilder.IsSPUBuildRWJobsOn() )\
		Warning("Rendering path not fully supported in %s or tested on SPU, and SPU jobs are enabled!\n", s);

#else

static ConVar r_2PassBuildDraw( "r_2PassBuildDraw", "1", FCVAR_DEVELOPMENTONLY );
static ConVar r_threaded_buildWRlist( "r_threaded_buildWRlist", "1", FCVAR_DEVELOPMENTONLY, "Threaded BuildWorldList and BuildRenderables list" );

CON_COMMAND_F( toggleThreadedBuildRWList, "toggleThreadedBuildRWList", FCVAR_DEVELOPMENTONLY )
{
	bool newValue = !r_threaded_buildWRlist.GetBool();
	r_threaded_buildWRlist.SetValue( newValue );
	Msg( "r_threaded_buildWRlist = %s\n", newValue? "TRUE":"FALSE" );
}

CConcurrentViewBuilder g_viewBuilder;

#define BEGIN_2PASS_BUILD_BLOCK if( g_viewBuilder.GetPassFlags() & PASS_BUILDLISTS ) {
#define BEGIN_2PASS_DRAW_BLOCK if( g_viewBuilder.GetPassFlags() & PASS_DRAWLISTS ) {
#define END_2PASS_BLOCK }

#define PROLOGUE_PASS_DRAWLISTS

#define EPILOGUE_PASS_DRAWLISTS 

#define SYNC_BUILDWORLD_JOB( bShadowDepth ) g_viewBuilder.WaitForBuildWorldListJob();\
	if( m_pWorldRenderList == NULL )\
	{\
		m_pWorldRenderList = g_viewBuilder.GetWorldRenderListElement();\
		if ( m_pWorldRenderList )\
			InlineAddRef(m_pWorldRenderList);\
	}\
	if( m_pWorldListInfo == NULL )\
	{\
		m_pWorldListInfo = g_viewBuilder.GetClientWorldListInfoElement();\
		if ( m_pWorldListInfo )\
			InlineAddRef(m_pWorldListInfo);\
	}\
	BuildWorldRenderLists_Epilogue( bShadowDepth );

#define SYNC_BUILDRENDERABLES_JOB( viewID )	g_viewBuilder.WaitForBuildRenderablesListJob();\
	if( m_pRenderables == NULL )\
	{\
		m_pRenderables = g_viewBuilder.GetRenderablesListElement();\
		if ( m_pRenderables )\
			InlineAddRef(m_pRenderables);\
	}\
	BuildRenderableRenderLists_Epilogue( viewID );

#define PS3_SPUPATH_INVALID( s )

#endif

static FrustumCache_t s_FrustumCache;
FrustumCache_t *FrustumCache( void )
{
	return &s_FrustumCache;
}


#define FREEZECAM_SNAPSHOT_FADE_SPEED 340
float g_flFreezeFlash[ MAX_SPLITSCREEN_PLAYERS ];

//-----------------------------------------------------------------------------

CON_COMMAND( r_cheapwaterstart,  "" )
{
	if( args.ArgC() == 2 )
	{
		float dist = atof( args[ 1 ] );
		view->SetCheapWaterStartDistance( dist );
	}
	else
	{
		float start, end;
		view->GetWaterLODParams( start, end );
		Warning( "r_cheapwaterstart: %f\n", start );
	}
}

CON_COMMAND( r_cheapwaterend,  "" )
{
	if( args.ArgC() == 2 )
	{
		float dist = atof( args[ 1 ] );
		view->SetCheapWaterEndDistance( dist );
	}
	else
	{
		float start, end;
		view->GetWaterLODParams( start, end );
		Warning( "r_cheapwaterend: %f\n", end );
	}
}


#ifdef PORTAL2
struct AperturePhotoViewQueue_t
{
	EHANDLE hEnt;
	ITexture *pTexture;
	int iFailedTries;
};
CUtlVector<AperturePhotoViewQueue_t> g_AperturePhotoQueue;

void Aperture_QueuePhotoView( EHANDLE hPhotoEntity, ITexture *pRenderTarget )
{
	if( pRenderTarget == NULL )
		return;

	AperturePhotoViewQueue_t temp;
	temp.hEnt = hPhotoEntity;
	temp.pTexture = pRenderTarget;
	temp.iFailedTries = 0;
	
	g_AperturePhotoQueue.AddToTail( temp );
}
#endif



static int ComputeSimpleWorldModelDrawFlags()
{
#if defined( PORTAL ) 
#if 0
	// Some spew to track portal distances
	static int nLastFrame = -1;
	static int nCurrentEntryInFrame = 0;

	if ( nLastFrame != gpGlobals->framecount )
	{
		nLastFrame = gpGlobals->framecount;
		nCurrentEntryInFrame = 0;
	}

	engine->Con_NPrintf( 1 + nCurrentEntryInFrame, "Portal %X distance: %f", g_pPortalRender->GetCurrentViewExitPortal(), g_pPortalRender->GetCurrentPortalDistanceBias() );
	++ nCurrentEntryInFrame;
#endif // 0 

	bool bSimpleWorldModeWaterReflection;
	int nSimpleWorldModelRecursionLevel;
	float flSimpleWorldModelDrawBeyondDistance;
	GetSimpleWorldModelConfiguration( bSimpleWorldModeWaterReflection, nSimpleWorldModelRecursionLevel, flSimpleWorldModelDrawBeyondDistance );
	if ( nSimpleWorldModelRecursionLevel >= 0 && g_pPortalRender->GetViewRecursionLevel() >= nSimpleWorldModelRecursionLevel )
	{
		return DF_DRAW_SIMPLE_WORLD_MODEL | DF_DRAW_SIMPLE_WORLD_MODEL_WATER | DF_FAST_ENTITY_RENDERING | DF_DRAW_ENTITITES;
	}
	else
	{
		if ( flSimpleWorldModelDrawBeyondDistance >= 0.0f )
		{
			float flDistanceBias = g_pPortalRender->GetCurrentPortalDistanceBias();
			if ( flDistanceBias > flSimpleWorldModelDrawBeyondDistance )
			{
				return DF_DRAW_SIMPLE_WORLD_MODEL | DF_DRAW_SIMPLE_WORLD_MODEL_WATER | DF_FAST_ENTITY_RENDERING | DF_DRAW_ENTITITES;
			}
		}
	}
#endif // PORTAL

	return 0;
}

//-----------------------------------------------------------------------------
// Describes a pruned set of leaves to be rendered this view. Reference counted
// because potentially shared by a number of views
//-----------------------------------------------------------------------------
struct ClientWorldListInfo_t : public CRefCounted1<WorldListInfo_t>
{
	ClientWorldListInfo_t() 
	{ 
		memset( (WorldListInfo_t *)this, 0, sizeof(WorldListInfo_t) ); 
		m_pOriginalLeafIndex = NULL;
		m_bPooledAlloc = false;
	}

#if defined(_PS3)
	void Init()
	{
		memset( (WorldListInfo_t *)this, 0, sizeof(WorldListInfo_t) ); 
		m_pOriginalLeafIndex = NULL;
		m_bPooledAlloc = false;
	}
#endif

	// Allocate a list intended for pruning
	static ClientWorldListInfo_t *AllocPooled( const ClientWorldListInfo_t &exemplar );

	// Because we remap leaves to eliminate unused leaves, we need a remap
	// when drawing translucent surfaces, which requires the *original* leaf index
	// using m_pOriginalLeafIndex[ remapped leaf index ] == actual leaf index
	uint16 *m_pOriginalLeafIndex;

private:
	virtual bool OnFinalRelease();

	bool m_bPooledAlloc;
	static CObjectPool<ClientWorldListInfo_t> gm_Pool;
};

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

class CWorldListCache
{
public:
	CWorldListCache()
	{

	}
	void Flush()
	{
		for ( int i = m_Entries.FirstInorder(); i != m_Entries.InvalidIndex(); i = m_Entries.NextInorder( i ) )
		{
			delete m_Entries[i];
		}
		m_Entries.RemoveAll();
	}

	bool Find( const CViewSetup &viewSetup, VisOverrideData_t *pVisOverrideData, int iForceViewLeaf, IWorldRenderList **ppList, ClientWorldListInfo_t **ppListInfo )
	{
		Entry_t lookup( viewSetup, pVisOverrideData, iForceViewLeaf );

		int i = m_Entries.Find( &lookup );

		if ( i != m_Entries.InvalidIndex() )
		{
			Entry_t *pEntry = m_Entries[i];
			*ppList = InlineAddRef( pEntry->pList );
			*ppListInfo = InlineAddRef( pEntry->pListInfo );
			return true;
		}

		return false;
	}

	void Add( const CViewSetup &viewSetup, VisOverrideData_t *pVisOverrideData, int iForceViewLeaf, IWorldRenderList *pList, ClientWorldListInfo_t *pListInfo )
	{
		m_Entries.Insert( new Entry_t( viewSetup, pVisOverrideData, iForceViewLeaf, pList, pListInfo ) );
	}

private:
	struct Entry_t
	{
		Entry_t( const CViewSetup &viewSetup, VisOverrideData_t *pVisOverrideData, int iForceViewLeaf, IWorldRenderList *pList = NULL, ClientWorldListInfo_t *pListInfo = NULL ) :
			pList( ( pList ) ? InlineAddRef( pList ) : NULL ),
			pListInfo( ( pListInfo ) ? InlineAddRef( pListInfo ) : NULL )
		{
            // @NOTE (toml 8/18/2006): because doing memcmp, need to fill all of the fields and the padding!
			memset( &m_bOrtho, 0, offsetof(Entry_t, pList ) - offsetof(Entry_t, m_bOrtho ) );
			m_bOrtho = viewSetup.m_bOrtho;			
			m_OrthoLeft = viewSetup.m_OrthoLeft;		
			m_OrthoTop = viewSetup.m_OrthoTop;
			m_OrthoRight = viewSetup.m_OrthoRight;
			m_OrthoBottom = viewSetup.m_OrthoBottom;
			fov = viewSetup.fov;				
			origin = viewSetup.origin;					
			angles = viewSetup.angles;				
			zNear = viewSetup.zNear;			
			zFar = viewSetup.zFar;			
			m_flAspectRatio = viewSetup.m_flAspectRatio;
			m_bOffCenter = viewSetup.m_bOffCenter;
			m_flOffCenterTop = viewSetup.m_flOffCenterTop;
			m_flOffCenterBottom = viewSetup.m_flOffCenterBottom;
			m_flOffCenterLeft = viewSetup.m_flOffCenterLeft;
			m_flOffCenterRight = viewSetup.m_flOffCenterRight;
			if ( pVisOverrideData )
			{
				memcpy( &m_VisOverride, pVisOverrideData, sizeof(m_VisOverride) );
			}
			else
			{
				memset( &m_VisOverride, 0, sizeof(m_VisOverride) );
			}
			m_iForceViewLeaf = iForceViewLeaf;
		}

		~Entry_t()
		{
			if ( pList )
			{
				pList->Release();
			}
			if ( pListInfo )
			{
				pListInfo->Release();
			}
		}

		// The fields from CViewSetup and ViewCustomVisibility_t that would actually affect the list
		int		m_iForceViewLeaf;
		VisOverrideData_t m_VisOverride;
		float	m_OrthoLeft;		
		float	m_OrthoTop;
		float	m_OrthoRight;
		float	m_OrthoBottom;
		float	fov;				
		Vector	origin;					
		QAngle	angles;				
		float	zNear;			
		float	zFar;			
		float	m_flAspectRatio;
		float	m_flOffCenterTop;
		float	m_flOffCenterBottom;
		float	m_flOffCenterLeft;
		float	m_flOffCenterRight;
		bool	m_bOrtho;			
		bool	m_bOffCenter;

		IWorldRenderList *pList;
		ClientWorldListInfo_t *pListInfo;
	};

	class CEntryComparator
	{
	public:
		// This class has to be implicitly constructible from int due to Valve's implementation of CUtlRbTree.
		// cppcheck-suppress noExplicitConstructor
		CEntryComparator( int ) {}

		bool operator!() const { return false; }
		bool operator()( const Entry_t *lhs, const Entry_t *rhs ) const 
		{ 
			return ( memcmp( lhs, rhs, sizeof(Entry_t) - ( sizeof(Entry_t) - offsetof(Entry_t, pList ) ) ) < 0 );
		}
	};

	CUtlRBTree<Entry_t *, unsigned short, CEntryComparator> m_Entries;
};

CWorldListCache g_WorldListCache;

//-----------------------------------------------------------------------------
// Standard 3d skybox view
//-----------------------------------------------------------------------------
class CSkyboxView : public CRendering3dView
{
	DECLARE_CLASS( CSkyboxView, CRendering3dView );
public:
	explicit CSkyboxView(CViewRender *pMainView) : 
		CRendering3dView( pMainView ),
		m_pSky3dParams( NULL )
	  {
	  }

	bool			Setup( const CViewSetup &view, int *pClearFlags, SkyboxVisibility_t *pSkyboxVisible );
	void			Draw();

protected:

#ifdef PORTAL
	virtual bool ShouldDrawPortals() { return false; }
#endif

	virtual SkyboxVisibility_t	ComputeSkyboxVisibility();

	bool			GetSkyboxFogEnable();

	void			Enable3dSkyboxFog( void );
	void			DrawInternal( view_id_t iSkyBoxViewID = VIEW_3DSKY, bool bInvokePreAndPostRender = true, ITexture *pRenderTarget = NULL );

	sky3dparams_t *	PreRender3dSkyboxWorld( SkyboxVisibility_t nSkyboxVisible );

	sky3dparams_t *m_pSky3dParams;
};

//-----------------------------------------------------------------------------
// 3d skybox view when drawing portals
//-----------------------------------------------------------------------------
#ifdef PORTAL
class CPortalSkyboxView : public CSkyboxView
{
	DECLARE_CLASS( CPortalSkyboxView, CSkyboxView );
public:
	CPortalSkyboxView(CViewRender *pMainView) : 
	  CSkyboxView( pMainView ),
		  m_pRenderTarget( NULL )
	  {}

	  bool			Setup( const CViewSetup &view, int *pClearFlags, SkyboxVisibility_t *pSkyboxVisible, ITexture *pRenderTarget = NULL );

	  //Skybox drawing through portals with workarounds to fix area bits, position/scaling, view id's..........
	  void			Draw();

private:
	virtual SkyboxVisibility_t	ComputeSkyboxVisibility();

	ITexture *m_pRenderTarget;
};
#endif


//-----------------------------------------------------------------------------
// Shadow depth texture
//-----------------------------------------------------------------------------
class CShadowDepthView : public CRendering3dView
{
	DECLARE_CLASS( CShadowDepthView, CRendering3dView );
public:
	explicit CShadowDepthView(CViewRender *pMainView) : CRendering3dView( pMainView ), m_bRenderWorldAndObjects( false ), m_bRenderViewModels( false ) {}

	void Setup( const CViewSetup &shadowViewIn, ITexture *pRenderTarget, ITexture *pDepthTexture, bool bRenderWorldAndObjects = true, bool bRenderViewModels = false );

	void Draw();

private:
	ITexture *m_pRenderTarget;
	ITexture *m_pDepthTexture;
	bool m_bRenderWorldAndObjects;
	bool m_bRenderViewModels;
};

//-----------------------------------------------------------------------------
// Freeze frame. Redraws the frame at which it was enabled.
//-----------------------------------------------------------------------------
class CFreezeFrameView : public CRendering3dView
{
	DECLARE_CLASS( CFreezeFrameView, CRendering3dView );
public:
	explicit CFreezeFrameView(CViewRender *pMainView) : CRendering3dView( pMainView ) {}

	void Setup( const CViewSetup &view );

	void Draw();

private:
	CMaterialReference m_pFreezeFrame;
	CMaterialReference m_TranslucentSingleColor;

	int					m_nSubRect[ 4 ];
	int					m_nScreenSize[ 2 ];
};

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class CBaseWorldView : public CRendering3dView
{
	DECLARE_CLASS( CBaseWorldView, CRendering3dView );
protected:
	explicit CBaseWorldView(CViewRender *pMainView) : CRendering3dView( pMainView ) {}

	virtual bool	AdjustView( float waterHeight );

	void			DrawSetup( IMatRenderContext *pRenderContext, float waterHeight, int flags, float waterZAdjust, int iForceViewLeaf = -1 );
	void			DrawExecute( float waterHeight, view_id_t viewID, float waterZAdjust );

	virtual void	PushView( float waterHeight );
	virtual void	PopView();
	void			SSAO_DepthPass();
};


//-----------------------------------------------------------------------------
// Draws the scene when there's no water or only cheap water
//-----------------------------------------------------------------------------
class CSimpleWorldView : public CBaseWorldView
{
	DECLARE_CLASS( CSimpleWorldView, CBaseWorldView );
public:
	explicit CSimpleWorldView(CViewRender *pMainView) : CBaseWorldView( pMainView ) {}

	void			Setup( const CViewSetup &view, int nClearFlags, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t& info, ViewCustomVisibility_t *pCustomVisibility = NULL );
	void			Draw();

private: 
	VisibleFogVolumeInfo_t m_fogInfo;

};


//-----------------------------------------------------------------------------
// Base class for scenes with water
//-----------------------------------------------------------------------------
class CBaseWaterView : public CBaseWorldView
{
	DECLARE_CLASS( CBaseWaterView, CBaseWorldView );
public:
	explicit CBaseWaterView(CViewRender *pMainView) : 
		CBaseWorldView( pMainView ),
		m_SoftwareIntersectionView( pMainView )
	{}

	//	void Setup( const CViewSetup &, const WaterRenderInfo_t& info );

protected:
	void			CalcWaterEyeAdjustments( const VisibleFogVolumeInfo_t &fogInfo, float &newWaterHeight, float &waterZAdjust, bool bSoftwareUserClipPlane );

	class CSoftwareIntersectionView : public CBaseWorldView
	{
		DECLARE_CLASS( CSoftwareIntersectionView, CBaseWorldView );
	public:
		explicit CSoftwareIntersectionView(CViewRender *pMainView) : CBaseWorldView( pMainView ) {}

		void Setup( bool bAboveWater );
		void Draw();

	private: 
		CBaseWaterView *GetOuter() { return GET_OUTER( CBaseWaterView, m_SoftwareIntersectionView ); }
	};

	friend class CSoftwareIntersectionView;

	CSoftwareIntersectionView m_SoftwareIntersectionView;

	WaterRenderInfo_t m_waterInfo;
	float m_waterHeight;
	float m_waterZAdjust;
	bool m_bSoftwareUserClipPlane;
	VisibleFogVolumeInfo_t m_fogInfo;
};


//-----------------------------------------------------------------------------
// Scenes above water
//-----------------------------------------------------------------------------
class CAboveWaterView : public CBaseWaterView
{
	DECLARE_CLASS( CAboveWaterView, CBaseWaterView );
public:
	explicit CAboveWaterView(CViewRender *pMainView) : 
		CBaseWaterView( pMainView ),
		m_ReflectionView( pMainView ),
		m_RefractionView( pMainView ),
		m_IntersectionView( pMainView )
	{}

	void Setup(  const CViewSetup &view, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t& waterInfo, ViewCustomVisibility_t *pCustomVisibility = NULL );
	void Draw();

	class CReflectionView : public CBaseWorldView
	{
		DECLARE_CLASS( CReflectionView, CBaseWorldView );
	public:
		explicit CReflectionView(CViewRender *pMainView) : CBaseWorldView( pMainView ) {}

		void Setup( bool bReflectEntities, bool bReflectOnlyMarkedEntities, bool bReflect2DSkybox );
		void Draw();

	private:
		CAboveWaterView *GetOuter() { return GET_OUTER( CAboveWaterView, m_ReflectionView ); }
	};

	class CRefractionView : public CBaseWorldView
	{
		DECLARE_CLASS( CRefractionView, CBaseWorldView );
	public:
		explicit CRefractionView(CViewRender *pMainView) : CBaseWorldView( pMainView ) {}

		void Setup();
		void Draw();

	private:
		CAboveWaterView *GetOuter() { return GET_OUTER( CAboveWaterView, m_RefractionView ); }
	};

	class CIntersectionView : public CBaseWorldView
	{
		DECLARE_CLASS( CIntersectionView, CBaseWorldView );
	public:
		explicit CIntersectionView(CViewRender *pMainView) : CBaseWorldView( pMainView ) {}

		void Setup();
		void Draw();

	private:
		CAboveWaterView *GetOuter() { return GET_OUTER( CAboveWaterView, m_IntersectionView ); }
	};


	friend class CRefractionView;
	friend class CReflectionView;
	friend class CIntersectionView;

	bool m_bViewIntersectsWater;

	CReflectionView m_ReflectionView;
	CRefractionView m_RefractionView;
	CIntersectionView m_IntersectionView;
};


//-----------------------------------------------------------------------------
// Scenes below water
//-----------------------------------------------------------------------------
class CUnderWaterView : public CBaseWaterView
{
	DECLARE_CLASS( CUnderWaterView, CBaseWaterView );
public:
	explicit CUnderWaterView(CViewRender *pMainView) : 
		CBaseWaterView( pMainView ),
		m_RefractionView( pMainView )
	{}

	void			Setup( const CViewSetup &view, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t& info, ViewCustomVisibility_t *pCustomVisibility = NULL );
	void			Draw();

	class CRefractionView : public CBaseWorldView
	{
		DECLARE_CLASS( CRefractionView, CBaseWorldView );
	public:
		explicit CRefractionView(CViewRender *pMainView) : CBaseWorldView( pMainView ) {}

		void Setup();
		void Draw();

	private:
		CUnderWaterView *GetOuter() { return GET_OUTER( CUnderWaterView, m_RefractionView ); }
	};

	friend class CRefractionView;

	bool m_bDrawSkybox; // @MULTICORE (toml 8/17/2006): remove after setup hoisted

	CRefractionView m_RefractionView;
};


//-----------------------------------------------------------------------------
// Scenes containing reflective glass
//-----------------------------------------------------------------------------
class CReflectiveGlassView : public CSimpleWorldView
{
	DECLARE_CLASS( CReflectiveGlassView, CSimpleWorldView );
public:
	explicit CReflectiveGlassView( CViewRender *pMainView ) : BaseClass( pMainView )
	{
	}

	virtual bool AdjustView( float flWaterHeight );
	virtual void PushView( float waterHeight );
	virtual void PopView( );
	void Setup( const CViewSetup &view, int nClearFlags, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t &waterInfo, const cplane_t &reflectionPlane );
	void Draw();


	cplane_t m_ReflectionPlane;
};

class CRefractiveGlassView : public CSimpleWorldView
{
	DECLARE_CLASS( CRefractiveGlassView, CSimpleWorldView );
public:
	explicit CRefractiveGlassView( CViewRender *pMainView ) : BaseClass( pMainView )
	{
	}

	virtual bool AdjustView( float flWaterHeight );
	virtual void PushView( float waterHeight );
	virtual void PopView( );
	void Setup( const CViewSetup &view, int nClearFlags, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t &waterInfo, const cplane_t &reflectionPlane );
	void Draw();

	cplane_t m_ReflectionPlane;
};




//-----------------------------------------------------------------------------
// view of a single entity by itself
//-----------------------------------------------------------------------------
#ifdef PORTAL2
class CAperturePhotoView : public CSimpleWorldView
{
	DECLARE_CLASS( CAperturePhotoView, CSimpleWorldView );
public:
	CAperturePhotoView(CViewRender *pMainView) : 
	  CSimpleWorldView( pMainView ),
		  m_pRenderTarget( NULL )
	  {}

	  bool			Setup( C_BaseEntity *pTargetEntity, const CViewSetup &view, int *pClearFlags, SkyboxVisibility_t *pSkyboxVisible, ITexture *pRenderTarget = NULL );

	  //Skybox drawing through portals with workarounds to fix area bits, position/scaling, view id's..........
	  void			Draw();

#ifdef PORTAL
	  virtual bool	ShouldDrawPortals() { return false; }
#endif

private:
	ITexture *m_pRenderTarget;
	C_BaseEntity *m_pTargetEntity;
};
#endif


//-----------------------------------------------------------------------------
// Computes draw flags for the engine to build its world surface lists
//-----------------------------------------------------------------------------
static inline unsigned long BuildEngineDrawWorldListFlags( unsigned nDrawFlags )
{
	unsigned long nEngineFlags = 0;

	if ( ( nDrawFlags & DF_SKIP_WORLD ) == 0 )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_WORLD_GEOMETRY;
	}

	if ( ( nDrawFlags & DF_SKIP_WORLD_DECALS_AND_OVERLAYS ) == 0 )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_DECALS_AND_OVERLAYS;
	}

	if ( nDrawFlags & DF_DRAWSKYBOX )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_SKYBOX;
	}

	if ( nDrawFlags & DF_RENDER_ABOVEWATER )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_STRICTLYABOVEWATER;
		nEngineFlags |= DRAWWORLDLISTS_DRAW_INTERSECTSWATER;
	}

	if ( nDrawFlags & DF_RENDER_UNDERWATER )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_STRICTLYUNDERWATER;
		nEngineFlags |= DRAWWORLDLISTS_DRAW_INTERSECTSWATER;
	}

	if ( nDrawFlags & DF_RENDER_WATER )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_WATERSURFACE;
	}

	if( nDrawFlags & DF_CLIP_SKYBOX )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_CLIPSKYBOX;
	}

	if( nDrawFlags & DF_SHADOW_DEPTH_MAP )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_SHADOWDEPTH;
		nEngineFlags &= ~DRAWWORLDLISTS_DRAW_DECALS_AND_OVERLAYS;
	}

	if( nDrawFlags & DF_RENDER_REFRACTION )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_REFRACTION;
	}

	if( nDrawFlags & DF_RENDER_REFLECTION )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_REFLECTION;
	}

	if ( nDrawFlags & ( DF_DRAW_SIMPLE_WORLD_MODEL | DF_DRAW_SIMPLE_WORLD_MODEL_WATER ) )
	{
		nEngineFlags &= ~DRAWWORLDLISTS_DRAW_WORLD_GEOMETRY;
		nEngineFlags &= ~DRAWWORLDLISTS_DRAW_DECALS_AND_OVERLAYS;
		if ( nDrawFlags & DF_DRAW_SIMPLE_WORLD_MODEL )
		{
			nEngineFlags |= DRAWWORLDLISTS_DRAW_SIMPLE_WORLD_MODEL;
		}
		if ( nDrawFlags & DF_DRAW_SIMPLE_WORLD_MODEL_WATER )
		{
			nEngineFlags |= DRAWWORLDLISTS_DRAW_SIMPLE_WORLD_MODEL_WATER;
		}
	}

	if ( nDrawFlags & DF_SSAO_DEPTH_PASS )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_SSAO | DRAWWORLDLISTS_DRAW_STRICTLYUNDERWATER | DRAWWORLDLISTS_DRAW_INTERSECTSWATER | DRAWWORLDLISTS_DRAW_STRICTLYABOVEWATER;
		nEngineFlags &= ~( DRAWWORLDLISTS_DRAW_WATERSURFACE | DRAWWORLDLISTS_DRAW_REFRACTION | DRAWWORLDLISTS_DRAW_REFLECTION | DRAWWORLDLISTS_DRAW_DECALS_AND_OVERLAYS );
	}


	return nEngineFlags;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void SetClearColorToFogColor()
{
	unsigned char ucFogColor[3];
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->GetFogColor( ucFogColor );
	if( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_INTEGER )
	{
		// @MULTICORE (toml 8/16/2006): Find a way to not do this twice in eye above water case
		float scale = LinearToGammaFullRange( pRenderContext->GetToneMappingScaleLinear().x );
		ucFogColor[0] = clamp( (float)ucFogColor[0] * scale, 0, 255);
		ucFogColor[1] = clamp( (float)ucFogColor[1] * scale, 0, 255);
		ucFogColor[2] = clamp( (float)ucFogColor[2] * scale, 0, 255);
	}
	pRenderContext->ClearColor4ub( ucFogColor[0], ucFogColor[1], ucFogColor[2], 255 );
}

//-----------------------------------------------------------------------------
// Precache of necessary materials
//-----------------------------------------------------------------------------

#ifdef HL2_CLIENT_DLL
PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheViewRender )
	PRECACHE( MATERIAL, "scripted/intro_screenspaceeffect" )
PRECACHE_REGISTER_END()
#endif

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecachePostProcessingEffects )
	PRECACHE( MATERIAL, "dev/blurfiltery_and_add_nohdr" )
	PRECACHE( MATERIAL, "dev/blurfilterx" )
	PRECACHE( MATERIAL, "dev/blurfilterx_nohdr" )
	PRECACHE( MATERIAL, "dev/blurfiltery" )
	PRECACHE( MATERIAL, "dev/blurfiltery_nohdr" )
	PRECACHE( MATERIAL, "dev/blurfiltery_nohdr_clear" )
	PRECACHE( MATERIAL, "dev/bloomadd" )
	PRECACHE( MATERIAL, "dev/clearalpha" )
	PRECACHE( MATERIAL, "dev/downsample" )
	PRECACHE( MATERIAL, "dev/downsample_non_hdr" )
	PRECACHE( MATERIAL, "dev/no_pixel_write" )
	PRECACHE( MATERIAL, "dev/lumcompare" )
	PRECACHE( MATERIAL, "dev/floattoscreen_combine" )
	PRECACHE( MATERIAL, "dev/copyfullframefb_vanilla" )
	PRECACHE( MATERIAL, "dev/copyfullframefb" )
	PRECACHE( MATERIAL, "dev/engine_post" )
	PRECACHE( MATERIAL, "dev/engine_post_splitscreen" )
	PRECACHE( MATERIAL, "dev/motion_blur" )
	PRECACHE( MATERIAL, "dev/depth_of_field" )
	PRECACHE( MATERIAL, "dev/blurgaussian_3x3" )
	PRECACHE( MATERIAL, "dev/fade_blur" )
	PRECACHE( MATERIAL, "debug/debugscreenspacewireframe" )
#if defined( INFESTED_DLL ) || defined( DOTA_DLL ) || defined( CSTRIKE_DLL )
	PRECACHE( MATERIAL, "dev/glow_color" )
	PRECACHE( MATERIAL, "dev/glow_health_color" )
	PRECACHE( MATERIAL, "dev/glow_downsample" )
	PRECACHE( MATERIAL, "dev/glow_blur_x" )
	PRECACHE( MATERIAL, "dev/glow_blur_y" )
	PRECACHE( MATERIAL, "dev/halo_add_to_screen" )
	PRECACHE( MATERIAL, "dev/glow_rim3d" )
	PRECACHE( MATERIAL, "dev/glow_edge_highlight" )
#ifdef IRONSIGHT
	PRECACHE( MATERIAL, "dev/scope_blur_x" )
	PRECACHE( MATERIAL, "dev/scope_blur_y" )
	PRECACHE( MATERIAL, "dev/scope_bluroverlay" )
	PRECACHE( MATERIAL, "dev/scope_downsample" )
	PRECACHE( MATERIAL, "dev/scope_mask" )
	PRECACHE( MATERIAL, "models/weapons/shared/scope/scope_dot_green")
	PRECACHE( MATERIAL, "models/weapons/shared/scope/scope_dot_red")
#endif
#endif // INFESTED_DLL || DOTA_DLL
#if defined( INFESTED_DLL )
	PRECACHE( MATERIAL, "engine/writestencil" )
#endif // INFSETED_DLL
PRECACHE_REGISTER_END( )

//-----------------------------------------------------------------------------
// Accessors to return the current view being rendered
//-----------------------------------------------------------------------------
const Vector &CurrentViewOrigin()
{
	Assert( s_bCanAccessCurrentView );
	return g_vecCurrentRenderOrigin;
}

const QAngle &CurrentViewAngles()
{
	Assert( s_bCanAccessCurrentView );
	return g_vecCurrentRenderAngles;
}

const Vector &CurrentViewForward()
{
	Assert( s_bCanAccessCurrentView );
	return g_vecCurrentVForward;
}

const Vector &CurrentViewRight()
{
	Assert( s_bCanAccessCurrentView );
	return g_vecCurrentVRight;
}

const Vector &CurrentViewUp()
{
	Assert( s_bCanAccessCurrentView );
	return g_vecCurrentVUp;
}

const VMatrix &CurrentWorldToViewMatrix()
{
	Assert( s_bCanAccessCurrentView );
	return g_matCurrentCamInverse;
}


//-----------------------------------------------------------------------------
// Methods to set the current view/guard access to view parameters
//-----------------------------------------------------------------------------
void AllowCurrentViewAccess( bool allow )
{
	s_bCanAccessCurrentView = allow;
}

bool IsCurrentViewAccessAllowed()
{
	return s_bCanAccessCurrentView;
}

static ConVar mat_lpreview_mode( "mat_lpreview_mode", "-1", FCVAR_CHEAT );
void SetupCurrentView( const Vector &vecOrigin, const QAngle &angles, view_id_t viewID, bool bDrawWorldNormal = false, bool bCullFrontFaces = false )
{
	// Store off view origin and angles
	g_vecCurrentRenderOrigin = vecOrigin;
	g_vecCurrentRenderAngles = angles;

	// Compute the world->main camera transform
	ComputeCameraVariables( vecOrigin, angles, 
		&g_vecCurrentVForward, &g_vecCurrentVRight, &g_vecCurrentVUp, &g_matCurrentCamInverse );

	g_CurrentViewID = viewID;
	AllowCurrentViewAccess( true );

	// Cache off fade distances
	float flScreenFadeMinSize, flScreenFadeMaxSize, flFadeDistScale;
	view->GetScreenFadeDistances( &flScreenFadeMinSize, &flScreenFadeMaxSize, &flFadeDistScale );
	modelinfo->SetViewScreenFadeRange( flScreenFadeMinSize, flScreenFadeMaxSize );

	CMatRenderContextPtr pRenderContext( materials );
#ifdef PORTAL
	if ( g_pPortalRender->GetViewRecursionLevel() == 0 )
	{
		pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_WRITE_DEPTH_TO_DESTALPHA, ((viewID == VIEW_MAIN) || (viewID == VIEW_3DSKY)) ? 1 : 0 );
	}
#else
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_WRITE_DEPTH_TO_DESTALPHA, ((viewID == VIEW_MAIN) || (viewID == VIEW_3DSKY)) ? 1 : 0 );
#endif

	if ( bDrawWorldNormal )
		pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_ENABLE_FIXED_LIGHTING, ENABLE_FIXED_LIGHTING_OUTPUTNORMAL_AND_DEPTH );

	if ( mat_lpreview_mode.GetInt() != -1 )
		pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_ENABLE_FIXED_LIGHTING, mat_lpreview_mode.GetInt() );

	if ( bCullFrontFaces )
	{
		pRenderContext->FlipCulling( true );
	}
}

view_id_t CurrentViewID()
{
	Assert( g_CurrentViewID != VIEW_ILLEGAL );
	return ( view_id_t )g_CurrentViewID;
}

//-----------------------------------------------------------------------------
// Purpose: Portal views are considered 'Main' views. This function tests a view id 
//			against all view ids used by portal renderables, as well as the main view.
//-----------------------------------------------------------------------------
bool IsMainView ( view_id_t id )
{
#if defined(PORTAL)
	return ( (id == VIEW_MAIN) || g_pPortalRender->IsPortalViewID( id ) );
#else
	return (id == VIEW_MAIN);
#endif
}

void FinishCurrentView()
{
	AllowCurrentViewAccess( false );
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
void CSimpleRenderExecutor::AddView( CRendering3dView *pView )
{
	// slightly kludgy place to put the viewBuilder frame initialisation
	g_viewBuilder.PushBuildView();

 	CBase3dView *pPrevRenderer = m_pMainView->SetActiveRenderer( pView );
	pView->Draw();
	m_pMainView->SetActiveRenderer( pPrevRenderer );

	// slightly kludgy place to put the viewBuilder frame initialisation
	g_viewBuilder.PopBuildView();
}


#if !defined( TF_CLIENT_DLL ) && !defined( INFESTED_DLL ) && !defined( DOTA_DLL ) && !defined(CSTRIKE_DLL)
static CViewRender g_ViewRender;
IViewRender *GetViewRenderInstance()
{
	return &g_ViewRender;
}
#endif


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CViewRender::CViewRender()
	: m_SimpleExecutor( this )
{
	m_flCheapWaterStartDistance = 0.0f;
	m_flCheapWaterEndDistance = 0.1f;
	m_BaseDrawFlags = 0;
	m_pActiveRenderer = NULL;
	m_pCurrentlyDrawingEntity = NULL;
	m_bAllowViewAccess = false;
	m_flOldChaseOverviewScale = 1.0f;
	m_flIdealChaseOverviewScale = 1.0f;
	m_flNextIdealOverviewScaleUpdate = 0;
	m_flSmokeOverlayAmount = 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
inline bool CViewRender::ShouldDrawEntities( void )
{
	return ( !m_pDrawEntities || (m_pDrawEntities->GetInt() != 0) );
}


//-----------------------------------------------------------------------------
// Purpose: Check all conditions which would prevent drawing the view model
// Input  : drawViewmodel - 
//			*viewmodel - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CViewRender::ShouldDrawViewModel( bool bDrawViewmodel )
{
	if ( !bDrawViewmodel )
		return false;

	if ( !r_drawviewmodel.GetBool() )
		return false;

	ASSERT_LOCAL_PLAYER_RESOLVABLE();

	if ( !C_BasePlayer::GetLocalPlayer() )
		return false;

	if ( C_BasePlayer::GetLocalPlayer()->ShouldDrawLocalPlayer() && 
		( C_BasePlayer::GetLocalPlayer()->GetObserverMode() != OBS_MODE_IN_EYE || C_BasePlayer::GetLocalPlayer()->GetObserverInterpState() == C_BasePlayer::OBSERVER_INTERP_TRAVELING ) )
		return false;

	if ( !ShouldDrawEntities() )
		return false;

	if ( render->GetViewEntity() > gpGlobals->maxClients )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CViewRender::UpdateRefractIfNeededByList( CViewModelRenderablesList::RenderGroups_t &list )
{
	int nCount = list.Count();
	for( int i=0; i < nCount; ++i )
	{
		IClientRenderable *pRenderable = list[i].m_pRenderable;
		Assert( pRenderable );
		if ( pRenderable->GetRenderFlags() & ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB )
		{
			UpdateRefractTexture();
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CViewRender::DrawRenderablesInList( CViewModelRenderablesList::RenderGroups_t &renderGroups, int flags )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();		    
#if defined( DBGFLAG_ASSERT )
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
#endif
	Assert( m_pCurrentlyDrawingEntity == NULL );
	int nCount = renderGroups.Count();
	for( int i=0; i < nCount; ++i )
	{
		IClientRenderable *pRenderable = renderGroups[i].m_pRenderable;
		Assert( pRenderable );

		// Non-view models wanting to render in view model list...
		Assert( pRenderable->ShouldDraw() );
#ifdef PORTAL
		Assert( ( g_pPortalRender->GetViewRecursionLevel() > 0 ) || !IsSplitScreenSupported() || pRenderable->ShouldDrawForSplitScreenUser( nSlot ) );
#else
		Assert( !IsSplitScreenSupported() || pRenderable->ShouldDrawForSplitScreenUser( nSlot ) );
#endif
		m_pCurrentlyDrawingEntity = pRenderable->GetIClientUnknown()->GetBaseEntity();
		int nDrawFlags = STUDIO_RENDER | flags;
		nDrawFlags |= renderGroups[i].m_InstanceData.m_bTwoPass ? STUDIO_TWOPASS : 0;
		pRenderable->DrawModel( nDrawFlags, renderGroups[i].m_InstanceData );
	}
	m_pCurrentlyDrawingEntity = NULL;
}

void CViewRender::DrawViewModelsShadowDepth( const CViewSetup &view )
{
	bool bShouldDrawPlayerViewModel = ShouldDrawViewModel( true );
	bool bShouldDrawToolViewModels = ToolsEnabled();

	if ( !bShouldDrawPlayerViewModel && !bShouldDrawToolViewModels )
		return;

	CViewModelRenderablesList list;
	ClientLeafSystem()->CollateViewModelRenderables( &list );
	CViewModelRenderablesList::RenderGroups_t &opaqueList = list.m_RenderGroups[ CViewModelRenderablesList::VM_GROUP_OPAQUE ];
	
	CViewModelRenderablesList listNormalFOV;
	CViewModelRenderablesList::RenderGroups_t &opaqueNormalFOVList = listNormalFOV.m_RenderGroups[ CViewModelRenderablesList::VM_GROUP_OPAQUE ];
	
	// Remove objects from the list that tools don't want
	// Move objects that aren't actually of the view model class into a different list so we can render them with normal FOV
	bool bRemove = ToolsEnabled() && ( !bShouldDrawPlayerViewModel || !bShouldDrawToolViewModels );

	int nOpaque = opaqueList.Count();
	for ( int i = nOpaque-1; i >= 0; --i )
	{
		IClientRenderable *pRenderable = opaqueList[ i ].m_pRenderable;
		bool bEntity = pRenderable->GetIClientUnknown()->GetBaseEntity() ? true : false;
		if ( bRemove )
		{
			if ( ( bEntity && !bShouldDrawPlayerViewModel ) || ( !bEntity && !bShouldDrawToolViewModels ) )
			{
				// Remove it
				opaqueList.FastRemove( i );
				continue;
			}
		}

		if ( !dynamic_cast<C_BaseViewModel*>( pRenderable ) )
		{
			// Copy into the no VM FOV list
			opaqueNormalFOVList.AddToTail( opaqueList[ i ] );
			opaqueList.FastRemove( i );
		}
	}
		
	// Update refract for opaque models & draw
	DrawRenderablesInList( opaqueList );
		
	// Render objects that use normal FOV
	if ( opaqueNormalFOVList.Count() > 0 )
	{
		// Update refract for opaque models & draw
		DrawRenderablesInList( opaqueNormalFOVList );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Actually draw the view model
// Input  : drawViewModel - 
//-----------------------------------------------------------------------------
#ifdef IRONSIGHT
void CViewRender::DrawViewModels( const CViewSetup &view, bool drawViewmodel, bool bDrawScopeLensMask )
#else
void CViewRender::DrawViewModels( const CViewSetup &view, bool drawViewmodel )
#endif
{
	VPROF( "CViewRender::DrawViewModel" );

#ifdef PORTAL //in portal, we'd like a copy of the front buffer without the gun in it for use with the depth doubler
	g_pPortalRender->UpdateDepthDoublerTexture( view );
#endif

	bool bShouldDrawPlayerViewModel = ShouldDrawViewModel( drawViewmodel );
	bool bShouldDrawToolViewModels = ToolsEnabled();

	if ( !bShouldDrawPlayerViewModel && !bShouldDrawToolViewModels )
		return;

	CMatRenderContextPtr pRenderContext( materials );
	MDLCACHE_CRITICAL_SECTION();
	
	#if defined( _X360 )
		pRenderContext->PushVertexShaderGPRAllocation( 32 );
	#endif

	PIXEVENT( pRenderContext, "DrawViewModels()" );

	// Restore the matrices
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();

	CViewSetup viewModelSetup( view );
	viewModelSetup.zNear = view.zNearViewmodel;
	viewModelSetup.zFar = view.zFarViewmodel;
	viewModelSetup.fov = view.fovViewmodel;
	viewModelSetup.m_flAspectRatio = engine->GetScreenAspectRatio( view.width, view.height );

	render->Push3DView( pRenderContext, viewModelSetup, 0, NULL, GetFrustum() );

#ifdef PORTAL //the depth range hack doesn't work well enough for the portal mod (and messing with the depth hack values makes some models draw incorrectly)
				//step up to a full depth clear if we're extremely close to a portal (in a portal environment)
	extern bool LocalPlayerIsCloseToPortal( void ); //defined in C_Portal_Player.cpp, abstracting to a single bool function to remove explicit dependence on c_portal_player.h/cpp, you can define the function as a "return true" in other build configurations at the cost of some perf
	bool bUseDepthHack = !LocalPlayerIsCloseToPortal();
	if( !bUseDepthHack )
		pRenderContext->ClearBuffers( false, true, false );
#else
	const bool bUseDepthHack = true;
#endif

	// FIXME: Add code to read the current depth range
	float depthmin = 0.0f;
	float depthmax = 1.0f;

	// HACK HACK:  Munge the depth range to prevent view model from poking into walls, etc.
	// Force clipped down range
	if( bUseDepthHack )
		pRenderContext->DepthRange( 0.0f, 0.1f );
	
	CViewModelRenderablesList list;
	ClientLeafSystem()->CollateViewModelRenderables( &list );
	CViewModelRenderablesList::RenderGroups_t &opaqueList = list.m_RenderGroups[ CViewModelRenderablesList::VM_GROUP_OPAQUE ];
	CViewModelRenderablesList::RenderGroups_t &translucentList = list.m_RenderGroups[ CViewModelRenderablesList::VM_GROUP_TRANSLUCENT ];

	CViewModelRenderablesList listNormalFOV;
	CViewModelRenderablesList::RenderGroups_t &opaqueNormalFOVList = listNormalFOV.m_RenderGroups[ CViewModelRenderablesList::VM_GROUP_OPAQUE ];
	CViewModelRenderablesList::RenderGroups_t &translucentNormalFOVList = listNormalFOV.m_RenderGroups[ CViewModelRenderablesList::VM_GROUP_TRANSLUCENT ];

	// Remove objects from the list that tools don't want
	// Move objects that aren't actually of the view model class into a different list so we can render them with normal FOV
	bool bRemove = ToolsEnabled() && ( !bShouldDrawPlayerViewModel || !bShouldDrawToolViewModels );

	int nOpaque = opaqueList.Count();
	for ( int i = nOpaque-1; i >= 0; --i )
	{
		IClientRenderable *pRenderable = opaqueList[ i ].m_pRenderable;
		bool bEntity = pRenderable->GetIClientUnknown()->GetBaseEntity() ? true : false;
		if ( bRemove )
		{
			if ( ( bEntity && !bShouldDrawPlayerViewModel ) || ( !bEntity && !bShouldDrawToolViewModels ) )
			{
				// Remove it
				opaqueList.FastRemove( i );
				continue;
			}
		}

		if ( !dynamic_cast<C_BaseViewModel*>( pRenderable ) )
		{
			// Copy into the no VM FOV list
			opaqueNormalFOVList.AddToTail( opaqueList[ i ] );
			opaqueList.FastRemove( i );
		}
#ifdef IRONSIGHT
		else
		{
			//we want this renderable to render a special masking shape, so we need to turn on ScopeStencilMaskMode
			C_BaseViewModel *pViewModel = dynamic_cast<C_BaseViewModel*>( pRenderable );
			if ( pViewModel )
				pViewModel->SetScopeStencilMaskMode( bDrawScopeLensMask );
		}
#endif
	}

	int nTranslucent = translucentList.Count();
	for ( int i = nTranslucent-1; i >= 0; --i )
	{
		IClientRenderable *pRenderable = translucentList[ i ].m_pRenderable;
		bool bEntity = pRenderable->GetIClientUnknown()->GetBaseEntity() ? true : false;

		if ( bRemove )
		{
			if ( ( bEntity && !bShouldDrawPlayerViewModel ) || ( !bEntity && !bShouldDrawToolViewModels ) )
			{
				// Remove it
				translucentList.FastRemove( i );
				continue;
			}
		}

		if ( !dynamic_cast<C_BaseViewModel*>( pRenderable ) )
		{
			// Copy into the no VM FOV list
			translucentNormalFOVList.AddToTail( translucentList[ i ] );
			translucentList.FastRemove( i );
		}
#ifdef IRONSIGHT
		else
		{
			//we want this renderable to render a special masking shape, so we need to turn on ScopeStencilMaskMode
			C_BaseViewModel *pViewModel = dynamic_cast<C_BaseViewModel*>( pRenderable );
			if ( pViewModel )
				pViewModel->SetScopeStencilMaskMode( bDrawScopeLensMask );
		}
#endif
	}

	g_CascadeLightManager.BeginViewModelRendering();

	// Update refract for opaque models & draw
	bool bUpdatedRefractForOpaque = UpdateRefractIfNeededByList( opaqueList );
	DrawRenderablesInList( opaqueList );

	// Update refract for translucent models (if we didn't already update it above) & draw
	if ( !bUpdatedRefractForOpaque ) // Only do this once for better perf
	{
		UpdateRefractIfNeededByList( translucentList );
	}
	DrawRenderablesInList( translucentList, STUDIO_TRANSPARENCY );

	// Reset the depth range to the original values
	if( bUseDepthHack )
		pRenderContext->DepthRange( depthmin, depthmax );

	render->PopView( pRenderContext, GetFrustum() );

	// Render objects that use normal FOV
	if ( !bDrawScopeLensMask && (opaqueNormalFOVList.Count() > 0 || translucentNormalFOVList.Count() > 0) )
	{
		viewModelSetup.fov = view.fov;
		render->Push3DView( pRenderContext, viewModelSetup, 0, NULL, GetFrustum() );

		// HACK HACK:  Munge the depth range to prevent view model from poking into walls, etc.
		// Force clipped down range
		if( bUseDepthHack )
			pRenderContext->DepthRange( 0.0f, 0.1f );

		// Update refract for opaque models & draw
		bool bUpdatedRefractForOpaque = UpdateRefractIfNeededByList( opaqueNormalFOVList );
		DrawRenderablesInList( opaqueNormalFOVList );

		// Update refract for translucent models (if we didn't already update it above) & draw
		if ( !bUpdatedRefractForOpaque ) // Only do this once for better perf
		{
			UpdateRefractIfNeededByList( translucentNormalFOVList );
		}
		DrawRenderablesInList( translucentNormalFOVList, STUDIO_TRANSPARENCY );

		// Reset the depth range to the original values
		if( bUseDepthHack )
			pRenderContext->DepthRange( depthmin, depthmax );

		render->PopView( pRenderContext, GetFrustum() );
	}

	g_CascadeLightManager.EndViewModelRendering();

	// Restore the matrices
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
	
	#if defined( _X360 )
		pRenderContext->PopVertexShaderGPRAllocation();
	#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CViewRender::ShouldDrawBrushModels( void )
{
	if ( m_pDrawBrushModels && !m_pDrawBrushModels->GetInt() )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Performs screen space effects, if any
//-----------------------------------------------------------------------------
void CViewRender::PerformScreenSpaceEffects( int x, int y, int w, int h )
{
	VPROF("CViewRender::PerformScreenSpaceEffects()");

	// FIXME: Screen-space effects are busted in the editor
	if ( engine->IsHammerRunning() )
		return;

	g_pScreenSpaceEffects->RenderEffects( x, y, w, h );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the screen space effect material (can't be done during rendering)
//-----------------------------------------------------------------------------
void CViewRender::SetScreenOverlayMaterial( IMaterial *pMaterial )
{
	m_ScreenOverlayMaterial.Init( pMaterial );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IMaterial *CViewRender::GetScreenOverlayMaterial( )
{
	return m_ScreenOverlayMaterial;
}


//-----------------------------------------------------------------------------
// Purpose: Performs screen space effects, if any
//-----------------------------------------------------------------------------
void CViewRender::PerformScreenOverlay( int x, int y, int w, int h )
{
	VPROF("CViewRender::PerformScreenOverlay()");

	if ( !r_drawscreenoverlay.GetBool() )
	{
		// As far as I can tell we don't ever draw the screen overlay on Portal2 - fading the screen in/out and blurring the screen is handled in engine_post.
		// I'm disabling it because the framebuffer now lives in _rt_FullFrameFB here on PS3 (not the backbuffer), and this is not compatible with some of the code paths in PerformScreenOverlay().
		if ( m_ScreenOverlayMaterial )
		{
			static bool s_bPrintedWarning;
			if ( !s_bPrintedWarning )
			{
				s_bPrintedWarning = true;
				Warning( "****** CViewRender::PerformScreenOverlay: Screen overlay wants to render, but it's been disabled!\n" );
				Assert( false );
			}
		}
		return;
	}

	if (m_ScreenOverlayMaterial)
	{
		if ( m_ScreenOverlayMaterial->NeedsFullFrameBufferTexture() )
		{
			DrawScreenEffectMaterial( m_ScreenOverlayMaterial, x, y, w, h );
		}
		else if ( m_ScreenOverlayMaterial->NeedsPowerOfTwoFrameBufferTexture() )
		{
			// First copy the FB off to the offscreen texture
			UpdateRefractTexture( x, y, w, h, true );

			// Now draw the entire screen using the material...
			CMatRenderContextPtr pRenderContext( materials );
			ITexture *pTexture = GetPowerOfTwoFrameBufferTexture( );
			int sw = pTexture->GetActualWidth();
			int sh = pTexture->GetActualHeight();
			pRenderContext->DrawScreenSpaceRectangle( m_ScreenOverlayMaterial, x, y, w, h,
												 0, 0, sw-1, sh-1, sw, sh );
		}
		else
		{
			byte color[4] = { 255, 255, 255, 255 };
			render->ViewDrawFade( color, m_ScreenOverlayMaterial );
		}
	}
}

void CViewRender::DrawUnderwaterOverlay( void )
{
	// Underwater overlay effect is disabled by default on Portal2 - as far as I can tell it's unused. 
	// It may need to be updated to work on PS3 (because _rt_FullFrameFB is used to hold the active framebuffer here on PS3 to avoid expensive resolves).
	if ( !r_drawunderwateroverlay.GetBool() )
		return;

	IMaterial *pOverlayMat = m_UnderWaterOverlayMaterial;

	if ( pOverlayMat )
	{
		CMatRenderContextPtr pRenderContext( materials );

		int x, y, w, h;

		pRenderContext->GetViewport( x, y, w, h );
		if ( pOverlayMat->NeedsFullFrameBufferTexture() )
		{
			DrawScreenEffectMaterial( pOverlayMat, x, y, w, h );
		}
		else if ( pOverlayMat->NeedsPowerOfTwoFrameBufferTexture() )
		{
			// First copy the FB off to the offscreen texture
			UpdateRefractTexture( x, y, w, h, true );

			// Now draw the entire screen using the material...
			CMatRenderContextPtr pRenderContext( materials );
			ITexture *pTexture = GetPowerOfTwoFrameBufferTexture( );
			int sw = pTexture->GetActualWidth();
			int sh = pTexture->GetActualHeight();
			pRenderContext->DrawScreenSpaceRectangle( pOverlayMat, x, y, w, h,
													  0, 0, sw-1, sh-1, sw, sh );
		}
		else
		{
			pRenderContext->DrawScreenSpaceRectangle( pOverlayMat, x, y, w, h,
													  0, 0, 1, 1, 1, 1 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the min/max fade distances, and distance scale
//-----------------------------------------------------------------------------
static ConVar r_fade360style( "r_fade360style", "1" );
void CViewRender::GetScreenFadeDistances( float *pMin, float *pMax, float *pScale )
{
	*pMin   = m_FadeData.m_flPixelMin;
	*pMax   = m_FadeData.m_flPixelMax;
	*pScale = m_FadeData.m_flFadeDistScale;

	// A complete, brutal hack, necessitated by our next-week ship date. 
	// On the 360, we use fade distances to deal with splitscreen. 
	// The tuning is such that the numbers used are correct for 720p.
	// We are not doing this optimization to save on fillrate; instead we are doing it
	// to save on CPU. Therefore, specifying the fades in terms of pixels is not correct.
	// If we're not running @ 720p, then we will recompute a new number based on screen res ratio.
	if ( IsGameConsole() || r_fade360style.GetInt() )
	{
		int screenWidth, screenHeight;
		g_pMaterialSystem->GetBackBufferDimensions( screenWidth, screenHeight );
		if ( screenHeight != 720 )
		{
			float flRatio = (float)screenHeight / 720.0f;
			*pMin *= flRatio;
			*pMax *= flRatio;
		}
	}

}

void CViewRender::OnScreenFadeMinSize( const CCommand &args )
{
	if ( args.ArgC() < 2 )
		return;

	m_FadeData.m_flPixelMin = atof( args[1] ) * 1000.0f;
}

void CViewRender::OnScreenFadeMaxSize( const CCommand &args )
{
	if ( args.ArgC() < 2 )
		return;

	m_FadeData.m_flPixelMax = atof( args[1] ) * 1000.0f;
}


//-----------------------------------------------------------------------------
// Purpose: Initialize the fade data.
//-----------------------------------------------------------------------------
void CViewRender::InitFadeData( void )
{
	// What system are we running.
	// L4D knocks down this convar in splitscreen mode to control the fade distances.
	// We want to use the CPU level since it may be overriden for different systems NOT the "Acutal" level since that is just the convar.
	int nSystemLevel = GetCPULevel();
	// The +1 is because the system levels start at -1 for unknown and the fade levels start at 0 for unknown / none.
	m_FadeData = g_aFadeData[nSystemLevel+1];
}

C_BaseEntity *CViewRender::GetCurrentlyDrawingEntity()
{
	return m_pCurrentlyDrawingEntity;
}

void CViewRender::SetCurrentlyDrawingEntity( C_BaseEntity *pEnt )
{
	m_pCurrentlyDrawingEntity = pEnt;
}

bool CViewRender::UpdateShadowDepthTexture( ITexture *pRenderTarget, ITexture *pDepthTexture, const CViewSetup &shadowViewIn, bool bRenderWorldAndObjects, bool bRenderViewModels )
{
	VPROF_INCREMENT_COUNTER( "shadow depth textures rendered", 1 );

	CMatRenderContextPtr pRenderContext( materials );

#if PIX_ENABLE
	char szPIXEventName[128];
	Q_snprintf( szPIXEventName, ARRAYSIZE( szPIXEventName ), "UpdateShadowDepthTexture (%s)", pDepthTexture ? pDepthTexture->GetName() : "null-depth-texture" );
	PIXEVENT( pRenderContext, szPIXEventName );
#endif

	CRefPtr<CShadowDepthView> pShadowDepthView = new CShadowDepthView( this );
	pShadowDepthView->Setup( shadowViewIn, pRenderTarget, pDepthTexture, bRenderWorldAndObjects, bRenderViewModels );

	AddViewToScene( pShadowDepthView );

	return true;
}


#if defined(CSTRIKE15) && defined(_PS3)
//-----------------------------------------------------------------------------
// Purpose: Initialise mem area for SPU BuildWorldLists, BuildRenderables
//-----------------------------------------------------------------------------
void CViewRender::InitSPUBuildRenderingJobs( void )
{
	// reset job view index
	g_viewBuilder.ResetBuildViewID();

	ClientLeafSystem()->PrepRenderablesListForSPU();
}
#endif

static bool IsThirdPersonOverview( void )
{
	return input->CAM_IsThirdPersonOverview();
}

//-----------------------------------------------------------------------------
// Purpose: Renders world and all entities, etc.
//-----------------------------------------------------------------------------
void CViewRender::ViewDrawScene( bool bDrew3dSkybox, SkyboxVisibility_t nSkyboxVisible, const CViewSetup &view, 
								int nClearFlags, view_id_t viewID, bool bDrawViewModel, int baseDrawFlags, ViewCustomVisibility_t *pCustomVisibility )
{
	VPROF( "CViewRender::ViewDrawScene" );
	SNPROF( "CViewRender::ViewDrawScene" );

	// this allows the refract texture to be updated once per *scene* on 360
	// (e.g. once for a monitor scene and once for the main scene)
	g_viewscene_refractUpdateFrame = gpGlobals->framecount - 1;

	BEGIN_2PASS_BUILD_BLOCK
	g_CascadeLightManager.PreRender();
	g_pClientShadowMgr->PreRender();
	END_2PASS_BLOCK

	// Shadowed flashlights supported on ps_2_b and up...
	if ( ( viewID == VIEW_MAIN ) && ( !view.m_bDrawWorldNormal ) )
	{
		g_CascadeLightManager.ComputeShadowDepthTextures( view );

		// On the 360, we call this even when we don't have shadow depth textures enabled, so that
		// the flashlight state gets set up properly
#if defined(_PS3)
		g_pClientShadowMgr->ComputeShadowDepthTextures( view, g_viewBuilder.GetPassFlags() & PASS_BUILDLISTS_PS3 );
#else
		g_pClientShadowMgr->ComputeShadowDepthTextures( view, ( g_viewBuilder.GetPassFlags() == PASS_BUILDLISTS ) );
#endif
	}

	m_BaseDrawFlags = baseDrawFlags;

	// After cascading shadows are drawn, early out if vision is 100% obscured by smoke.
	if ( m_flSmokeOverlayAmount >= 1 )
	{
		BEGIN_2PASS_DRAW_BLOCK
		// Free shadow depth textures for use in future view
		if ((viewID == VIEW_MAIN) && (!view.m_bDrawWorldNormal))
		{
			g_CascadeLightManager.UnlockAllShadowDepthTextures();

			g_pClientShadowMgr->UnlockAllShadowDepthTextures();
		}
		END_2PASS_BLOCK
		return;
	}

	SetupCurrentView( view.origin, view.angles, viewID, view.m_bDrawWorldNormal, view.m_bCullFrontFaces );

	BEGIN_2PASS_BUILD_BLOCK
	// Invoke pre-render methods
	IGameSystem::PreRenderAllSystems();
	END_2PASS_BLOCK

	// Start view

	unsigned int visFlags;


	SetupVis( view, visFlags, pCustomVisibility );

	if ( !bDrew3dSkybox )
	{
		if ( ( nSkyboxVisible == SKYBOX_NOT_VISIBLE ) && ( visFlags & IVRenderView::VIEW_SETUP_VIS_EX_RETURN_FLAGS_USES_RADIAL_VIS ) )
		{
			// This covers the case where we don't see a 3dskybox, yet radial vis is clipping
			// the far plane.  Need to clear to fog color in this case.
			nClearFlags |= VIEW_CLEAR_COLOR;
			SetClearColorToFogColor( );
		}
		else if ( IsX360() && ( !( nClearFlags & VIEW_CLEAR_COLOR ) ) )
		{
			// Make sure EDRAM color is always cleared to something on X360.
			// From the XDK docs on IDirect3DDevice9::Present():
			// Do not assume that the contents of extended dynamic random access memory (EDRAM) persist after calling the Present method. In 
			// debug builds, Direct3D clears the EDRAM to random values. After each call to Present, the color buffers and z-buffers are discarded.
			nClearFlags |= VIEW_CLEAR_COLOR;
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->ClearColor4ub( 0, 0, 0, 0 );
		}
	}

	bool drawSkybox = r_skybox.GetBool();
	if ( bDrew3dSkybox || ( nSkyboxVisible == SKYBOX_NOT_VISIBLE ) )
	{
		drawSkybox = false;
	}

	BEGIN_2PASS_BUILD_BLOCK
	ParticleMgr()->IncrementFrameCode();
	END_2PASS_BLOCK


	DrawWorldAndEntities( drawSkybox, view, nClearFlags, pCustomVisibility );


	// Disable fog for the rest of the stuff
	DisableFog();


	BEGIN_2PASS_DRAW_BLOCK

	// UNDONE: Don't do this with masked brush models, they should probably be in a separate list
	// render->DrawMaskEntities()


	// Here are the overlays...

	if ( !view.m_bDrawWorldNormal )
	{
		CGlowOverlay::DrawOverlays( view.m_bCacheFullSceneState );
	}

	// issue the pixel visibility tests
	if ( IsMainView( CurrentViewID() ) && !view.m_bDrawWorldNormal )
	{
		PixelVisibility_EndCurrentView();
	}

	// Draw rain..
	DrawPrecipitation();

	// Draw volumetrics for shadowed flashlights
	if ( r_flashlightvolumetrics.GetBool() && (viewID != VIEW_SHADOW_DEPTH_TEXTURE) && !view.m_bDrawWorldNormal )
	{
		g_pClientShadowMgr->DrawVolumetrics( view );
	}

	// Make sure sound doesn't stutter
	engine->Sound_ExtraUpdate();

	// Debugging info goes over the top
	CDebugViewRender::Draw3DDebuggingInfo( view );

	// Draw client side effects
	// NOTE: These are not sorted against the rest of the frame
	clienteffects->DrawEffects( gpGlobals->frametime );	

	END_2PASS_BLOCK

	// Mark the frame as locked down for client fx additions
	SetFXCreationAllowed( false );

	BEGIN_2PASS_DRAW_BLOCK
	// Invoke post-render methods
	IGameSystem::PostRenderAllSystems();
	END_2PASS_BLOCK


	FinishCurrentView();


	BEGIN_2PASS_DRAW_BLOCK
	// Free shadow depth textures for use in future view
	if ( ( viewID == VIEW_MAIN ) && ( !view.m_bDrawWorldNormal ) )
	{
		g_CascadeLightManager.UnlockAllShadowDepthTextures();

		g_pClientShadowMgr->UnlockAllShadowDepthTextures();
	}

	// Set int rendering parameters back to defaults
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_ENABLE_FIXED_LIGHTING, 0 );

	if ( view.m_bCullFrontFaces )
	{
		pRenderContext->FlipCulling( false );
	}
	END_2PASS_BLOCK

}

void CheckAndTransitionColor( float flPercent, float *pColor, float *pLerpToColor )
{
	if ( pLerpToColor[0] != pColor[0] || pLerpToColor[1] != pColor[1] || pLerpToColor[2] != pColor[2] )
	{
		float flDestColor[3];

		flDestColor[0] = pLerpToColor[0];
		flDestColor[1] = pLerpToColor[1];
		flDestColor[2] = pLerpToColor[2];

		pColor[0] = FLerp( pColor[0], flDestColor[0], flPercent );
		pColor[1] = FLerp( pColor[1], flDestColor[1], flPercent );
		pColor[2] = FLerp( pColor[2], flDestColor[2], flPercent );

		//Msg( "Actual(%f, %f, %f), Dest(%f, %f, %f), Percent(%f)\n", pColor[0], pColor[1], pColor[2], flDestColor[0], flDestColor[1], flDestColor[2], flPercent );
	}
	else
	{
		pColor[0] = pLerpToColor[0];
		pColor[1] = pLerpToColor[1];
		pColor[2] = pLerpToColor[2];
	}
}

static void GetFogColorTransition( fogparams_t *pFogParams, float *pColorPrimary, float *pColorSecondary )
{
	if ( !pFogParams )
		return;

	if ( pFogParams->lerptime >= gpGlobals->curtime )
	{
		float flPercent = MAX( 0, 1.0f - (( pFogParams->lerptime - gpGlobals->curtime ) / pFogParams->duration ) );

		float flPrimaryColorLerp[3] = { pFogParams->colorPrimaryLerpTo.GetR(), pFogParams->colorPrimaryLerpTo.GetG(), pFogParams->colorPrimaryLerpTo.GetB() };
		float flSecondaryColorLerp[3] = { pFogParams->colorSecondaryLerpTo.GetR(), pFogParams->colorSecondaryLerpTo.GetG(), pFogParams->colorSecondaryLerpTo.GetB() };

		CheckAndTransitionColor( flPercent, pColorPrimary, flPrimaryColorLerp );
		CheckAndTransitionColor( flPercent, pColorSecondary, flSecondaryColorLerp );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the fog color to use in rendering the current frame.
//-----------------------------------------------------------------------------
static void GetFogColor( fogparams_t *pFogParams, float *pColor, bool ignoreOverride, bool ignoreHDRColorScale )
{
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if ( !pbp || !pFogParams )
		return;

	bool bFogOverride = fog_override.GetBool() && !ignoreOverride;
	float HDRColorScale;
	if( bFogOverride && (fog_hdrcolorscale.GetFloat() != -1.0f) )
	{
		HDRColorScale = fog_hdrcolorscale.GetFloat();
	}
	else
	{
		HDRColorScale = pFogParams->HDRColorScale;
	}

	pColor[0] = pColor[1] = pColor[2] = -1.0f;
	const char *fogColorString = fog_color.GetString();
	if( bFogOverride && fogColorString )
	{
		sscanf( fogColorString, "%f %f %f", pColor, pColor+1, pColor+2 );
	}	

	if( (pColor[0] == -1.0f) && (pColor[1] == -1.0f) && (pColor[2] == -1.0f) ) //if not overriding fog, or if we get non-overridden fog color values
	{
		float flPrimaryColor[3] = { pFogParams->colorPrimary.GetR(), pFogParams->colorPrimary.GetG(), pFogParams->colorPrimary.GetB() };
		float flSecondaryColor[3] = { pFogParams->colorSecondary.GetR(), pFogParams->colorSecondary.GetG(), pFogParams->colorSecondary.GetB() };

		GetFogColorTransition( pFogParams, flPrimaryColor, flSecondaryColor );

		if( pFogParams->blend )
		{
			//
			// Blend between two fog colors based on viewing angle.
			// The secondary fog color is at 180 degrees to the primary fog color.
			//
			Vector forward;
			pbp->EyeVectors( &forward, NULL, NULL );
			
			Vector vNormalized = pFogParams->dirPrimary;
			VectorNormalize( vNormalized );
			pFogParams->dirPrimary = vNormalized;

			float flBlendFactor = 0.5 * forward.Dot( pFogParams->dirPrimary ) + 0.5;

			// FIXME: convert to linear colorspace
			pColor[0] = flPrimaryColor[0] * flBlendFactor + flSecondaryColor[0] * ( 1 - flBlendFactor );
			pColor[1] = flPrimaryColor[1] * flBlendFactor + flSecondaryColor[1] * ( 1 - flBlendFactor );
			pColor[2] = flPrimaryColor[2] * flBlendFactor + flSecondaryColor[2] * ( 1 - flBlendFactor );
		}
		else
		{
			pColor[0] = flPrimaryColor[0];
			pColor[1] = flPrimaryColor[1];
			pColor[2] = flPrimaryColor[2];
		}
	}

	if ( !ignoreHDRColorScale && g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE )
	{
		VectorScale( pColor, HDRColorScale, pColor );
	}

	VectorScale( pColor, 1.0f / 255.0f, pColor );
}


static float GetFogStart( fogparams_t *pFogParams, bool ignoreOverride )
{
	if( !pFogParams )
		return 0.0f;

	if( fog_override.GetInt() && !ignoreOverride )
	{
		if( fog_start.GetFloat() == -1.0f )
		{
			return pFogParams->start;
		}
		else
		{
			return fog_start.GetFloat();
		}
	}
	else
	{
		if ( pFogParams->lerptime > gpGlobals->curtime )
		{
			if ( pFogParams->start != pFogParams->startLerpTo )
			{
				if ( pFogParams->lerptime > gpGlobals->curtime )
				{
					float flPercent = MAX( 0, 1.0f - (( pFogParams->lerptime - gpGlobals->curtime ) / pFogParams->duration ) );

					return FLerp( pFogParams->start, pFogParams->startLerpTo, flPercent );
				}
				else
				{
					if ( pFogParams->start != pFogParams->startLerpTo )
					{
						pFogParams->start = pFogParams->startLerpTo;
					}
				}
			}
		}

		return pFogParams->start;
	}
}

static float GetFogEnd( fogparams_t *pFogParams, bool ignoreOverride )
{
	if( !pFogParams )
		return 0.0f;

	if( fog_override.GetInt() && !ignoreOverride )
	{
		if( fog_end.GetFloat() == -1.0f )
		{
			return pFogParams->end;
		}
		else
		{
			return fog_end.GetFloat();
		}
	}
	else
	{
		if ( pFogParams->lerptime > gpGlobals->curtime )
		{
			if ( pFogParams->end != pFogParams->endLerpTo )
			{
				if ( pFogParams->lerptime > gpGlobals->curtime )
				{
					float flPercent = MAX( 0, 1.0f - (( pFogParams->lerptime - gpGlobals->curtime ) / pFogParams->duration ) );

					float flEnd = pFogParams->end.Get();
					float flLerpTo = pFogParams->endLerpTo.Get();

					//Msg( "END = %f, LerpTo = %f, Percent = %f \n", flEnd, flLerpTo, flPercent );

					return FLerp( flEnd, flLerpTo, flPercent );
				}
				else
				{
					if ( pFogParams->end != pFogParams->endLerpTo )
					{
						pFogParams->end = pFogParams->endLerpTo;
					}
				}
			}
		}

		return pFogParams->end;
	}
}

static bool GetFogEnable( fogparams_t *pFogParams, bool ignoreOverride )
{
	if ( cl_leveloverview.GetInt() != 0 || input->CAM_IsThirdPersonOverview() )
		return false;

	// Ask the clientmode
	if ( GetClientMode()->ShouldDrawFog() == false )
		return false;

	if( fog_override.GetInt() && !ignoreOverride )
	{
		if( fog_enable.GetInt() )
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		if( pFogParams )
			return pFogParams->enable != false;

		return false;
	}
}


static float GetFogMaxDensity( fogparams_t *pFogParams, bool ignoreOverride )
{
	if( !pFogParams )
		return 1.0f;

	if ( cl_leveloverview.GetInt() != 0 || input->CAM_IsThirdPersonOverview() )
		return 1.0f;

	// Ask the clientmode
	if ( !GetClientMode()->ShouldDrawFog() )
		return 1.0f;

	if ( fog_override.GetInt() && !ignoreOverride )
	{
		if ( fog_maxdensity.GetFloat() == -1.0f )
			return pFogParams->maxdensity;
		else
			return fog_maxdensity.GetFloat();
	}
	else
	{
		if ( pFogParams->lerptime > gpGlobals->curtime )
		{
			if ( pFogParams->maxdensity != pFogParams->maxdensityLerpTo )
			{
				if ( pFogParams->lerptime > gpGlobals->curtime )
				{
					float flPercent = MAX( 0, 1.0f - (( pFogParams->lerptime - gpGlobals->curtime ) / pFogParams->duration ) );

					return FLerp( pFogParams->maxdensity, pFogParams->maxdensityLerpTo, flPercent );
				}
				else
				{
					if ( pFogParams->maxdensity != pFogParams->maxdensityLerpTo )
					{
						pFogParams->maxdensity = pFogParams->maxdensityLerpTo;
					}
				}
			}
		}

		return pFogParams->maxdensity;
	}
}



//-----------------------------------------------------------------------------
// Purpose: Returns the skybox fog color to use in rendering the current frame.
//-----------------------------------------------------------------------------
static void GetSkyboxFogColor( float *pColor, bool ignoreOverride, bool ignoreHDRColorScale )
{			   
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if( !pbp )
	{
		return;
	}
	CPlayerLocalData	*local		= &pbp->m_Local;

	bool bFogOverride = fog_override.GetBool() && !ignoreOverride;
	float HDRColorScale;
	if( bFogOverride && (fog_hdrcolorscaleskybox.GetFloat() != -1.0f) )
	{
		HDRColorScale = fog_hdrcolorscaleskybox.GetFloat();
	}
	else
	{
		HDRColorScale = local->m_skybox3d.fog.HDRColorScale;
	}

	pColor[0] = pColor[1] = pColor[2] = -1.0f;
	const char *fogColorString = fog_colorskybox.GetString();
	if( bFogOverride && fogColorString )
	{
		sscanf( fogColorString, "%f %f %f", pColor, pColor+1, pColor+2 );
	}	

	
	if( (pColor[0] == -1.0f) && (pColor[1] == -1.0f) && (pColor[2] == -1.0f) ) //if not overriding fog, or if we get non-overridden fog color values
	{
		if( local->m_skybox3d.fog.blend )
		{
			//
			// Blend between two fog colors based on viewing angle.
			// The secondary fog color is at 180 degrees to the primary fog color.
			//
			Vector forward;
			pbp->EyeVectors( &forward, NULL, NULL );

			Vector vNormalized = local->m_skybox3d.fog.dirPrimary;
			VectorNormalize( vNormalized );
			local->m_skybox3d.fog.dirPrimary = vNormalized;

			float flBlendFactor = 0.5 * forward.Dot( local->m_skybox3d.fog.dirPrimary ) + 0.5;
						 
			// FIXME: convert to linear colorspace
			pColor[0] = local->m_skybox3d.fog.colorPrimary.GetR() * flBlendFactor + local->m_skybox3d.fog.colorSecondary.GetR() * ( 1 - flBlendFactor );
			pColor[1] = local->m_skybox3d.fog.colorPrimary.GetG() * flBlendFactor + local->m_skybox3d.fog.colorSecondary.GetG() * ( 1 - flBlendFactor );
			pColor[2] = local->m_skybox3d.fog.colorPrimary.GetB() * flBlendFactor + local->m_skybox3d.fog.colorSecondary.GetB() * ( 1 - flBlendFactor );
		}
		else
		{
			pColor[0] = local->m_skybox3d.fog.colorPrimary.GetR();
			pColor[1] = local->m_skybox3d.fog.colorPrimary.GetG();
			pColor[2] = local->m_skybox3d.fog.colorPrimary.GetB();
		}
	}

	if ( !ignoreHDRColorScale && g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE )
	{
		VectorScale( pColor, HDRColorScale, pColor );
	}
	VectorScale( pColor, 1.0f / 255.0f, pColor );
}


static float GetSkyboxFogStart( bool ignoreOverride )
{
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if( !pbp )
	{
		return 0.0f;
	}
	CPlayerLocalData	*local		= &pbp->m_Local;

	if( fog_override.GetInt() && !ignoreOverride )
	{
		if( fog_startskybox.GetFloat() == -1.0f )
		{
			return local->m_skybox3d.fog.start;
		}
		else
		{
			return fog_startskybox.GetFloat();
		}
	}
	else
	{
		return local->m_skybox3d.fog.start;
	}
}

static float GetSkyboxFogEnd( bool ignoreOverride )
{
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if( !pbp )
	{
		return 0.0f;
	}
	CPlayerLocalData	*local		= &pbp->m_Local;

	if( fog_override.GetInt() && !ignoreOverride )
	{
		if( fog_endskybox.GetFloat() == -1.0f )
		{
			return local->m_skybox3d.fog.end;
		}
		else
		{
			return fog_endskybox.GetFloat();
		}
	}
	else
	{
		return local->m_skybox3d.fog.end;
	}
}


static float GetSkyboxFogMaxDensity( bool ignoreOverride )
{
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if ( !pbp )
		return 1.0f;

	CPlayerLocalData *local = &pbp->m_Local;

	if ( cl_leveloverview.GetInt() != 0 || input->CAM_IsThirdPersonOverview() )
		return 1.0f;

	// Ask the clientmode
	if ( !GetClientMode()->ShouldDrawFog() )
		return 1.0f;

	if ( fog_override.GetInt() && !ignoreOverride )
	{
		if ( fog_maxdensityskybox.GetFloat() == -1.0f )
			return local->m_skybox3d.fog.maxdensity;
		else
			return fog_maxdensityskybox.GetFloat();
	}
	else
		return local->m_skybox3d.fog.maxdensity;
}




void CViewRender::DisableFog( void )
{
	VPROF("CViewRander::DisableFog()");

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->FogMode( MATERIAL_FOG_NONE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CViewRender::SetupVis( const CViewSetup& view, unsigned int &visFlags, ViewCustomVisibility_t *pCustomVisibility )
{
	VPROF( "CViewRender::SetupVis" );

	if ( pCustomVisibility && pCustomVisibility->m_nNumVisOrigins )
	{
		// Pass array or vis origins to merge
		render->ViewSetupVisEx( ShouldForceNoVis(), pCustomVisibility->m_nNumVisOrigins, pCustomVisibility->m_rgVisOrigins, visFlags );
	}
	else
	{
		// Use render origin as vis origin by default
		render->ViewSetupVisEx( ShouldForceNoVis(), 1, &view.origin, visFlags );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Renders voice feedback and other sprites attached to players
// Input  : none
//-----------------------------------------------------------------------------
void CViewRender::RenderPlayerSprites()
{
	GetClientVoiceMgr()->DrawHeadLabels();
}

void CViewRender::DrawLetterBoxRectangles( int nSlot, const CUtlVector< vrect_t >& vecLetterBoxRectangles )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( m_WhiteMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, vecLetterBoxRectangles.Count() );

	Color clr( 0, 0, 0, 255 );

	float zpos = -99999;

	for ( int i=0; i<vecLetterBoxRectangles.Count(); ++i )
	{
		const vrect_t &r = vecLetterBoxRectangles[ i ];

		meshBuilder.Position3f( r.x, r.y, zpos );
		meshBuilder.Color4ub( clr.r(), clr.g(), clr.b(), clr.a() );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( r.x + r.width, r.y, zpos );
		meshBuilder.Color4ub( clr.r(), clr.g(), clr.b(), clr.a() );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( r.x + r.width, r.y + r.height, zpos  );
		meshBuilder.Color4ub( clr.r(), clr.g(), clr.b(), clr.a() );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( r.x, r.y + r.height, zpos );
		meshBuilder.Color4ub( clr.r(), clr.g(), clr.b(), clr.a() );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.AdvanceVertex();

	}
	meshBuilder.End();
	pMesh->Draw();

	
}

/*

1, 2, 3 and 4 are the possible letter box rectangles, if any are needed

(x,y)------------------------------------
|		|			2		|			|
|		|(view.x,view.y)----|			|
|		|					|			|
|	1	|					|		4	|
|		|					|			|
|		|___________________|(vright,vbottom)
|		|			3		|			|
|---------------------------------------| (right, bottom)
|										|
|										|
|										|
|										|
-----------------------------------------
*/
void CViewRender::GetLetterBoxRectangles( int nSlot, const CViewSetup &view, CUtlVector< vrect_t >& vecLetterBoxRectangles )
{
	// This uses the full screen size, not the hud or 3d inset size
	int x, y, w, h;
	VGui_GetPanelBounds( nSlot, x, y, w, h );

	int right = x + w;
	int bottom = y + h;

	vrect_t r;

	int vbottom = view.y + view.height;
	int vright = view.x + view.width;

	// HACK:  Adding in one extra pixel of slop at the border with the inset here...

	// Need rect # 1?
	if ( view.x != x )
	{
		r.x = x;
		r.y = y;
		r.width = view.x + 1;
		r.height = h;

		vecLetterBoxRectangles.AddToTail( r );
	}

	// Need rect # 2?
	if ( view.y != y )
	{
		r.x = view.x;
		r.y = y;
		r.width = view.width;
		r.height = view.y + 1;

		vecLetterBoxRectangles.AddToTail( r );
	}

	// Need rect # 3?
	if ( bottom != vbottom )
	{
		r.x = view.x;
		r.y = vbottom - 1;
		r.width = view.width;
		r.height = bottom - vbottom + 1;

		vecLetterBoxRectangles.AddToTail( r );
	}

	// Need rect # 4?
	if ( right != vright )
	{
		r.x = vright - 1;
		r.y = y;
		r.width = right - vright + 1;
		r.height = h;

		vecLetterBoxRectangles.AddToTail( r );
	}
}

//-----------------------------------------------------------------------------
// Sets up, cleans up the main 3D view
//-----------------------------------------------------------------------------
void CViewRender::SetupMain3DView( int nSlot, const CViewSetup &view, const CViewSetup &hudViewSetup, int &nClearFlags, ITexture *pRenderTarget )
{
	// FIXME: I really want these fields removed from CViewSetup 
	// and passed in as independent flags
	// Clear the color here if requested.

	int nDepthStencilFlags = nClearFlags & ( VIEW_CLEAR_DEPTH | VIEW_CLEAR_STENCIL );
	nClearFlags &= ~( nDepthStencilFlags ); // Clear these flags
	if ( nClearFlags & VIEW_CLEAR_COLOR )
	{
		nClearFlags |= nDepthStencilFlags; // Add them back in if we're clearing color
	}

	CMatRenderContextPtr pRenderContext( materials );
	// See if this view needs borders
	CUtlVector< vrect_t > letterbox;
	GetLetterBoxRectangles( nSlot, view, letterbox );
	if ( letterbox.Count() )
	{
		CViewSetup letterBoxViewSetup;
		letterBoxViewSetup.x = 0;
		letterBoxViewSetup.y = 0;
		VGui_GetTrueScreenSize( letterBoxViewSetup.width, letterBoxViewSetup.height );
		render->Push2DView( pRenderContext, letterBoxViewSetup, 0, pRenderTarget, GetFrustum() );
		
		DrawLetterBoxRectangles( nSlot, letterbox );

		render->PopView( pRenderContext, GetFrustum() );
	}

	// If we are using HDR, we render to the HDR backbuffer
	// instead of whatever was previously the render target
	if( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT )
	{
		// Indicates that the render target is already HDR
		if ( view.m_bHDRTarget )
		{
			render->Push3DView( pRenderContext, view, nClearFlags, pRenderTarget, GetFrustum() );
		}
		else
		{
			pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_BACK_BUFFER_INDEX, BACK_BUFFER_INDEX_HDR );
			render->Push3DView( pRenderContext, view, nClearFlags, NULL, GetFrustum() );
		}
	}
	else
	{
		render->Push3DView( pRenderContext, view, nClearFlags, NULL, GetFrustum() );
	}

	pRenderContext.SafeRelease(); // don't want to hold for long periods in case in a locking active share thread mode

	// If we didn't clear the depth here, we'll need to clear it later
	nClearFlags ^= nDepthStencilFlags; // Toggle these bits
	if ( nClearFlags & VIEW_CLEAR_COLOR )
	{
		// If we cleared the color here, we don't need to clear it later
		nClearFlags &= ~( VIEW_CLEAR_COLOR | VIEW_CLEAR_FULL_TARGET );
	}
}

void CViewRender::CleanupMain3DView( const CViewSetup &view )
{
	// Make sure we reset from the HDR rendertarget back to the main backbuffer
	CMatRenderContextPtr pRenderContext( materials );
	if( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT )
	{
		pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_BACK_BUFFER_INDEX, BACK_BUFFER_INDEX_DEFAULT );
	}

	render->PopView( pRenderContext, GetFrustum() );
	pRenderContext.SafeRelease(); // don't want to hold for long periods in case in a locking active share thread mode
}


//-----------------------------------------------------------------------------
// Queues up an overlay rendering
//-----------------------------------------------------------------------------
void CViewRender::QueueOverlayRenderView( const CViewSetup &view, int nClearFlags, int whatToDraw )
{
	// Can't have 2 in a single scene
	Assert( !m_bDrawOverlay );

    m_bDrawOverlay = true;
	m_OverlayViewSetup = view;
	m_OverlayClearFlags = nClearFlags;
	m_OverlayDrawFlags = whatToDraw;
}

//-----------------------------------------------------------------------------
// Purpose: Force the view to freeze on the next frame for the specified time
//-----------------------------------------------------------------------------
void CViewRender::FreezeFrame( float flFreezeTime )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int slot = GET_ACTIVE_SPLITSCREEN_SLOT();

	if ( flFreezeTime == 0 )
	{
		m_FreezeParams[ slot ].m_flFreezeFrameUntil = 0;
		m_FreezeParams[ slot ].m_bTakeFreezeFrame = false;
	}
	else
	{
		if ( m_FreezeParams[ slot ].m_flFreezeFrameUntil > gpGlobals->curtime )
		{
			m_FreezeParams[ slot ].m_flFreezeFrameUntil += flFreezeTime;
		}
		else
		{
			m_FreezeParams[ slot ].m_flFreezeFrameUntil = gpGlobals->curtime + flFreezeTime;
			m_FreezeParams[ slot ].m_bTakeFreezeFrame = true;	
		}
	}
}

void PositionHudPanels( CUtlVector< vgui::VPANEL > &list, const CViewSetup &view )
{
	for ( int i = 0; i < list.Count(); ++i )
	{
		vgui::VPANEL root = list[ i ];
		if ( root != 0 )
		{
			vgui::ipanel()->SetPos( root, view.x, view.y );
			vgui::ipanel()->SetSize( root, view.width, view.height );
		}
	}
}

#ifdef PARTICLE_USAGE_DEMO
static ConVar r_particle_demo( "r_particle_demo", "0", FCVAR_CHEAT );
static CNonDrawingParticleSystem *s_pDemoSystem = NULL;

void ParticleUsageDemo( void )
{
	if ( r_particle_demo.GetInt() )
	{
		if ( ! s_pDemoSystem )
		{
			s_pDemoSystem = ParticleMgr()->CreateNonDrawingEffect( "christest" );
		}
		// draw a bunch of bars
		CParticleCollection *pSystem = s_pDemoSystem->Get();
		for( int i = 0; i < pSystem->m_nActiveParticles; i++ )
		{
			Vector vecColor = pSystem->GetVectorAttributeValue( PARTICLE_ATTRIBUTE_TINT_RGB, i );
			vecColor *= 255.0;
			float flRadius = *( pSystem->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_RADIUS, i ) );
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->ClearColor4ub( vecColor.x, vecColor.y, vecColor.z, 255 );
			pRenderContext->Viewport( 0, i * 20, flRadius, 17 );
			pRenderContext->ClearBuffers( true, true );
		}
	}
	else
	{
		// its off
		if ( s_pDemoSystem )
		{
			delete s_pDemoSystem;
			s_pDemoSystem = NULL;
		}

	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: This renders the entire 3D view and the in-game hud/viewmodel
// Input  : &view - 
//			whatToDraw - 
//-----------------------------------------------------------------------------
// This renders the entire 3D view.
extern ConVar r_drawothermodels;
void CViewRender::RenderView( const CViewSetup &view, const CViewSetup &hudViewSetup, int nClearFlags, int whatToDraw )
{
	m_UnderWaterOverlayMaterial.Shutdown();					// underwater view will set

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int slot = GET_ACTIVE_SPLITSCREEN_SLOT();

	m_CurrentView = view;

	C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, true );
	VPROF( "CViewRender::RenderView" );

	// Don't want CS:GO running less than SM3
	// @wge: HACK FIXME - Not doing this on MacOSX for now...
	if ( !IsGameConsole() && !IsOSX() && !IsOpenGL() && ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 95 ) )
	{
		// We know they were running at least 9.0 when the game started...we check the 
		// value in ClientDLL_Init()...so they must be messing with their DirectX settings.
		static bool bFirstTime = true;
		if ( bFirstTime )
		{
			bFirstTime = false;
			Msg( "This game has a minimum requirement of Shader Model 3.0 and your video card must support cascaded shadow mapping to run properly.\n" );
		}
		return;
	}

	CMatRenderContextPtr pRenderContext( materials );

#if defined(_PS3)
	pRenderContext->AntiAliasingHint( AA_HINT_MESHES );

	// init SPU render job data for buildworldlists, buildrenderables
	InitSPUBuildRenderingJobs();

	g_viewBuilder.SetPassFlags( PASS_BUILDLISTS_PS3 | PASS_DRAWLISTS_PS3 );
#else

	g_viewBuilder.Init();
	g_viewBuilder.SetPassFlags( PASS_BUILDLISTS | PASS_DRAWLISTS );

	// Update bounds of all renderables
	ClientLeafSystem()->ComputeAllBounds();

#endif

	ITexture *saveRenderTarget = pRenderContext->GetRenderTarget();
	pRenderContext.SafeRelease(); // don't want to hold for long periods in case in a locking active share thread mode

	if ( !m_FreezeParams[ slot ].m_bTakeFreezeFrame && m_FreezeParams[ slot ].m_flFreezeFrameUntil > gpGlobals->curtime )
	{
		CRefPtr<CFreezeFrameView> pFreezeFrameView = new CFreezeFrameView( this );
		pFreezeFrameView->Setup( view );
		AddViewToScene( pFreezeFrameView );

		g_bRenderingView = true;
		AllowCurrentViewAccess( true );
	}
	else
	{
		g_flFreezeFlash[ slot ] = 0.0f;

	#ifdef USE_MONITORS
		if ( cl_drawmonitors.GetBool() && 
			( ( whatToDraw & RENDERVIEW_SUPPRESSMONITORRENDERING ) == 0 ) )
		{
			DrawMonitors( view );	
		}
	#endif

		g_bRenderingView = true;

		RenderPreScene( view );

		// Must be first 
		render->SceneBegin();

		g_pColorCorrectionMgr->UpdateColorCorrection();

		// Send the current tonemap scalar to the material system
		UpdateMaterialSystemTonemapScalar();

		// clear happens here probably
		SetupMain3DView( slot, view, hudViewSetup, nClearFlags, saveRenderTarget );

		g_pClientShadowMgr->UpdateSplitscreenLocalPlayerShadowSkip();

		bool bDrew3dSkybox = false;
		SkyboxVisibility_t nSkyboxVisible = SKYBOX_NOT_VISIBLE;

		// Don't bother with the skybox if we're drawing an ND buffer for the SFM
		if ( !view.m_bDrawWorldNormal )
		{
			// if the 3d skybox world is drawn, then don't draw the normal skybox
			if ( true ) // For pix event
			{
				#if PIX_ENABLE
				{
					CMatRenderContextPtr pRenderContext( materials );
					PIXEVENT( pRenderContext, "Skybox Rendering" );
				}
				#endif

				CSkyboxView *pSkyView = new CSkyboxView( this );
				if ( ( bDrew3dSkybox = pSkyView->Setup( view, &nClearFlags, &nSkyboxVisible ) ) != false )
				{
					AddViewToScene( pSkyView );
				}
				SafeRelease( pSkyView );
			}
		}

		// Force it to clear the framebuffer if they're in solid space.
		if ( ( nClearFlags & VIEW_CLEAR_COLOR ) == 0 )
		{
			MDLCACHE_CRITICAL_SECTION();
			if ( enginetrace->GetPointContents( view.origin ) == CONTENTS_SOLID )
			{
				nClearFlags |= VIEW_CLEAR_COLOR;
			}
		}

		PreViewDrawScene( view );

		// Render world and all entities, particles, etc.
		if( !g_pIntroData )
		{			
			MDLCACHE_CRITICAL_SECTION();
			#if PIX_ENABLE
			{
				CMatRenderContextPtr pRenderContext( materials );
				PIXEVENT( pRenderContext, "ViewDrawScene()" );
			}
			#endif

#if defined(_PS3)

			// Entry point for 2 pass rendering on PS3, for CSTRIKE15
			// pass 1 - build - kick off build world and renderable lists on SPU
			// pass 2 - draw - sync build jobs and draw
			// 2 passes allows the building jobs to be kicked off asap, and the rendering can then
			// be performed in parallel
			// This path is still undergoing testing, and does not support all rendering paths (refraction, proper reflection, etc)

			// sync points and other 2 pass macros near the top of this file - wrap code to be performed in on or other pass with a begin/end macro (see examples)
			
			if( r_PS3_2PassBuildDraw.GetInt() )
			{
				int numViews[2];

				SNPROF("2PassBuildWRLists");

				g_viewBuilder.Init();

				// turn on SPU BuildWorld/Renderables jobs
				g_viewBuilder.SPUBuildRWJobsOn( true );

				// reset job view index
				g_viewBuilder.ResetBuildViewID();

				// Pass 1 - Build World and Renderables Lists
				g_viewBuilder.SetPassFlags( PASS_BUILDLISTS_PS3 );
				ViewDrawScene( bDrew3dSkybox, nSkyboxVisible, view, nClearFlags, VIEW_MAIN, whatToDraw & RENDERVIEW_DRAWVIEWMODEL );

				numViews[0] = g_viewBuilder.GetBuildViewID();

				// push all stored up buildrenderable jobs 
				g_viewBuilder.PushBuildRenderableJobs();

				
				// kick off threaded audio here, this only does anything when running IsServer is true
				// helps to hide any sync on buildworld/renderable jobs
				engine->Sound_ServerUpdateSoundsPS3();

				// reset job view index
				g_viewBuilder.ResetBuildViewID();

				// Pass 2 - Draw
				g_viewBuilder.SetPassFlags( PASS_DRAWLISTS_PS3 );
				ViewDrawScene( bDrew3dSkybox, nSkyboxVisible, view, nClearFlags, VIEW_MAIN, whatToDraw & RENDERVIEW_DRAWVIEWMODEL );

				numViews[1] = g_viewBuilder.GetBuildViewID();

				if( numViews[0] != numViews[1] )
				{
					Warning("PS3 2 pass draw error - numViews mismatch, p0:%d p1:%d\n", numViews[0], numViews[1]);
				}

				// turn off SPU BuildWorld/Renderables jobs
				g_viewBuilder.SPUBuildRWJobsOn( false );

				g_viewBuilder.Purge();
			}
			else
			{
				g_viewBuilder.Init();
				g_viewBuilder.SPUBuildRWJobsOn( true );

				g_viewBuilder.SetPassFlags( PASS_BUILDLISTS_PS3 | PASS_DRAWLISTS_PS3 );
				ViewDrawScene( bDrew3dSkybox, nSkyboxVisible, view, nClearFlags, VIEW_MAIN, whatToDraw & RENDERVIEW_DRAWVIEWMODEL );

				g_viewBuilder.SPUBuildRWJobsOn( false );
				g_viewBuilder.Purge();
			}

#else
			g_viewBuilder.Init();

			// Entry point for 2 pass rendering for CSTRIKE15
			// pass 1 - build - kick off build world and renderable lists on different thread
			// pass 2 - draw - sync build jobs and draw
			// 2 passes allows the building jobs to be kicked off asap, and the rendering can then
			// be performed in parallel
			// This path is still undergoing testing, and does not support all rendering paths (refraction, proper reflection, etc)

			// sync points and other 2 pass macros near the top of this file - wrap code to be performed in on or other pass with a begin/end macro (see examples)

			if ( r_2PassBuildDraw.GetBool() )
			{
				g_viewBuilder.SetBuildWRThreaded( true );
				
				//
				// First pass - Generate build world and renderables lists
				//

				// reset job view index
				g_viewBuilder.ResetBuildViewID();

				{
					g_viewBuilder.SetPassFlags( PASS_BUILDLISTS );
					ViewDrawScene( bDrew3dSkybox, nSkyboxVisible, view, nClearFlags, VIEW_MAIN, whatToDraw & RENDERVIEW_DRAWVIEWMODEL );
				}

				g_viewBuilder.FlushBuildRenderablesListJob();

				//
				// Second pass - Draw
				//

				// reset job view index
				g_viewBuilder.ResetBuildViewID();

				{
					g_viewBuilder.SetPassFlags( PASS_DRAWLISTS );
					ViewDrawScene( bDrew3dSkybox, nSkyboxVisible, view, nClearFlags, VIEW_MAIN, whatToDraw & RENDERVIEW_DRAWVIEWMODEL );
				}

				g_viewBuilder.SetBuildWRThreaded( false );
				
			}
			else
			{
				// Single pass

				g_viewBuilder.SetPassFlags( PASS_BUILDLISTS | PASS_DRAWLISTS );
				g_viewBuilder.SetBuildWRThreaded( true );

				ViewDrawScene( bDrew3dSkybox, nSkyboxVisible, view, nClearFlags, VIEW_MAIN, whatToDraw & RENDERVIEW_DRAWVIEWMODEL );

				g_viewBuilder.SetBuildWRThreaded( false );

			}

			g_viewBuilder.Purge();
#endif
		}
		else
		{
			MDLCACHE_CRITICAL_SECTION();
			#if PIX_ENABLE
			{
				CMatRenderContextPtr pRenderContext( materials );
				PIXEVENT( pRenderContext, "ViewDrawScene_Intro()" );
			}
			#endif
			ViewDrawScene_Intro( view, nClearFlags, *g_pIntroData );
		}

		// We can still use the 'current view' stuff set up in ViewDrawScene
		AllowCurrentViewAccess( true );

		PostViewDrawScene( view );

		engine->DrawPortals();

		DisableFog();

		// Finish scene
		render->SceneEnd();

		// Draw lightsources if enabled
		render->DrawLights();

		RenderPlayerSprites();

		// Image-space motion blur and depth of field
		#if defined( _X360 )
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->PushVertexShaderGPRAllocation( 16 ); //Max out pixel shader threads
			pRenderContext.SafeRelease();
		}
		#endif
		
		Rect_t curViewport;
		if ( IsPS3() )
		{
			CMatRenderContextPtr pRenderContext( materials );
			// On PS3, the motion blur pass reads from the backbuffer and outputs to _rt_FullFrameFB. Subsequent passes then read/write 
			// _rt_FullFrameFB, then engine_post reads from _rt_FullFrameFB and outputs to the backbuffer.
			pRenderContext->GetViewport( curViewport.x, curViewport.y, curViewport.width, curViewport.height );
			pRenderContext->SetRenderTarget( materials->FindTexture( "_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET ) );
			pRenderContext->Viewport( curViewport.x, curViewport.y, curViewport.width, curViewport.height );
			
			pRenderContext.SafeRelease();
		}

		bool bPerformedMotionBlur = false;
		if ( !building_cubemaps.GetBool() )
		{
			if ( IsDepthOfFieldEnabled() )
			{
				pRenderContext.GetFrom( materials );
				{
					PIXEVENT( pRenderContext, "DoDepthOfField()" );
					DoDepthOfField( view );
				}
				pRenderContext.SafeRelease();
			}

			// We don't want to motion blur the freeze frame. It isn't going to be moving much.
			ConVarRef mat_motion_blur_enabled( "mat_motion_blur_enabled" );
			if ( ( view.m_nMotionBlurMode != MOTION_BLUR_DISABLE ) && ( mat_motion_blur_enabled.GetInt() ) && !m_FreezeParams[ slot ].m_bTakeFreezeFrame )
			{
				pRenderContext.GetFrom( materials );
				{
					PIXEVENT( pRenderContext, "DoImageSpaceMotionBlur()" );
					bPerformedMotionBlur = DoImageSpaceMotionBlur( view );
				}
				pRenderContext.SafeRelease();
			}
		}

		if ( IsPS3() && !bPerformedMotionBlur )
		{
			// Ensure the final framebuffer is always copied into the backbuffer on PS3 (this is normally done by the postprocess pass, which for whatever reason didn't happen).
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->PushRenderTargetAndViewport( NULL );
			
			UpdateScreenEffectTexture( 0, view.x, view.y, view.width, view.height );

			pRenderContext->PopRenderTargetAndViewport();

			pRenderContext.SafeRelease();
		}

		#if defined( _X360 )
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->PopVertexShaderGPRAllocation();
			pRenderContext.SafeRelease();
		}
		#endif

		RenderSmokeOverlay( true );
		DrawViewModels( view, whatToDraw & RENDERVIEW_DRAWVIEWMODEL );
		RenderSmokeOverlay( false );
		
		DrawUnderwaterOverlay();
		
		PixelVisibility_EndScene();

		#if defined( _X360 )
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->PushVertexShaderGPRAllocation( 16 ); //Max out pixel shader threads
			pRenderContext.SafeRelease();
		}
		#endif

		// Draw fade over entire screen if needed
		byte color[4];
		bool blend;
		GetViewEffects()->GetFadeParams( &color[0], &color[1], &color[2], &color[3], &blend );

		// Store off color fade params to be applied in fullscreen postprocess pass
		SetViewFadeParams( color[0], color[1], color[2], color[3], blend );
		
		// Draw an overlay to make it even harder to see inside smoke particle systems.
		DrawSmokeFogOverlay();
		
		// Overlay screen fade on entire screen
		PerformScreenOverlay( view.x, view.y, view.width, view.height );
				
		// Prevent sound stutter if going slow
		engine->Sound_ExtraUpdate();	

		if ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE )
		{
			pRenderContext.GetFrom( materials );
			pRenderContext->SetToneMappingScaleLinear(Vector(1,1,1));
			pRenderContext.SafeRelease();
		}

		if ( IsPS3() )
		{
			// On PS3, engine_post reads from _rt_FullFrameFB and outputs to the backbuffer.
			CMatRenderContextPtr pRenderContext( materials );
						
			pRenderContext->SetRenderTarget( NULL );

			pRenderContext->Viewport( curViewport.x, curViewport.y, curViewport.width, curViewport.height );

			pRenderContext.SafeRelease();
		}

#ifdef IRONSIGHT
		if ( r_drawothermodels.GetInt() == 1 && ApplyIronSightScopeEffect( view.x, view.y, view.width, view.height, &m_CurrentView, true ) )
		{
			//draw the viewmodel with a special flag that renders a special mask shape into the stencil buffer
			DrawViewModels(view, whatToDraw & RENDERVIEW_DRAWVIEWMODEL, true);

			//apply the finished blur effect over the screen, while masking out the scope lens
			ApplyIronSightScopeEffect( view.x, view.y, view.width, view.height, &m_CurrentView, false );
		}
#endif

		bool bPerformedPostProcessing = false;
		if ( !building_cubemaps.GetBool() && view.m_bDoBloomAndToneMapping )
		{
			pRenderContext.GetFrom( materials );
			{
				static bool bAlreadyShowedLoadTime = false;
				
				if ( ! bAlreadyShowedLoadTime )
				{
					bAlreadyShowedLoadTime = true;
					if ( CommandLine()->CheckParm( "-timeload" ) )
					{
						Warning( "time to initial render = %f\n", Plat_FloatTime() );
					}
				}

				PIXEVENT( pRenderContext, "DoEnginePostProcessing()" );

				bool bFlashlightIsOn = false;
				C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
				if ( pLocal )
				{
					bFlashlightIsOn = pLocal->IsEffectActive( EF_DIMLIGHT );
				}
				bPerformedPostProcessing = DoEnginePostProcessing( view.x, view.y, view.width, view.height, bFlashlightIsOn );
			}
			pRenderContext.SafeRelease();
		}

		if ( IsPS3() && !bPerformedPostProcessing )
		{
			// Ensure the final framebuffer is always copied into the backbuffer on PS3 (this is normally done by the postprocess pass).
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->PushRenderTargetAndViewport( materials->FindTexture( "_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET ) );

			Rect_t rect;
			rect.x = view.x; rect.y = view.y;
			rect.width = view.width; rect.height = view.height;

			pRenderContext->CopyRenderTargetToTextureEx( materials->FindTexture( "^PS3^BACKBUFFER", TEXTURE_GROUP_RENDER_TARGET ), 0, &rect, &rect );
			pRenderContext->PopRenderTargetAndViewport();

			pRenderContext.SafeRelease();
		}

		// And here are the screen-space effects

		if ( IsPC() )
		{
			// Grab the pre-color corrected frame for editing purposes
			engine->GrabPreColorCorrectedFrame( view.x, view.y, view.width, view.height );
		}

		PerformScreenSpaceEffects( view.x, view.y, view.width, view.height );

		#if defined( _X360 )
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->PopVertexShaderGPRAllocation();
			pRenderContext.SafeRelease();
		}
		#endif


		GetClientMode()->DoPostScreenSpaceEffects( &view );

		CleanupMain3DView( view );

		if ( m_FreezeParams[ slot ].m_bTakeFreezeFrame )
		{
			pRenderContext = materials->GetRenderContext();
			if ( IsGameConsole() )
			{
				// 360 doesn't create the Fullscreen texture
				pRenderContext->CopyRenderTargetToTextureEx( GetFullFrameFrameBufferTexture( 1 ), 0, NULL, NULL );
			}
			else
			{
				pRenderContext->CopyRenderTargetToTextureEx( GetFullscreenTexture(), 0, NULL, NULL );
			}
			pRenderContext.SafeRelease();
			m_FreezeParams[ slot ].m_bTakeFreezeFrame = false;
		}

		pRenderContext = materials->GetRenderContext();
		pRenderContext->SetRenderTarget( saveRenderTarget );
		pRenderContext.SafeRelease();

		// Draw the overlay
		if ( m_bDrawOverlay )
		{	   
			// This allows us to be ok if there are nested overlay views
			CViewSetup currentView = m_CurrentView;
			CViewSetup tempView = m_OverlayViewSetup;
			tempView.fov = ScaleFOVByWidthRatio( tempView.fov, tempView.m_flAspectRatio / ( 4.0f / 3.0f ) );
			tempView.m_bDoBloomAndToneMapping = false;				// FIXME: Hack to get Mark up and running
			tempView.m_nMotionBlurMode = MOTION_BLUR_DISABLE;		// FIXME: Hack to get Mark up and running
			m_bDrawOverlay = false;
			RenderView( tempView, hudViewSetup, m_OverlayClearFlags, m_OverlayDrawFlags );
			m_CurrentView = currentView;
		}
	}

#if defined( USE_SDL )
	if ( mat_viewportupscale.GetBool() && mat_viewportscale.GetFloat() < 1.0f ) 
	{
		CMatRenderContextPtr pRenderContext( materials );

		ITexture *pFullFrameFB1 = materials->FindTexture( "_rt_FullFrameFB1", TEXTURE_GROUP_RENDER_TARGET );

		Rect_t	DownscaleRect, UpscaleRect;

		DownscaleRect.x = view.x;
		DownscaleRect.y = view.y;
		DownscaleRect.width = view.width;
		DownscaleRect.height = view.height;

		UpscaleRect.x = view.m_nUnscaledX;
		UpscaleRect.y = view.m_nUnscaledY;
		UpscaleRect.width = view.m_nUnscaledWidth;
		UpscaleRect.height = view.m_nUnscaledHeight;

		pRenderContext->CopyRenderTargetToTextureEx( pFullFrameFB1, 0, &DownscaleRect, &DownscaleRect );
		pRenderContext->CopyTextureToRenderTargetEx( 0, pFullFrameFB1, &DownscaleRect, &UpscaleRect );
	}
#endif

	// Clear a row of pixels at the edge of the viewport if it isn't at the edge of the screen
	if ( VGui_IsSplitScreen() )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->PushRenderTargetAndViewport();

		int nScreenWidth, nScreenHeight;
		g_pMaterialSystem->GetBackBufferDimensions( nScreenWidth, nScreenHeight );

		// NOTE: view.height is off by 1 on the PC in a release build, but debug is correct! I'm leaving this here to help track this down later.
		// engine->Con_NPrintf( 25 + hh, "view( %d, %d, %d, %d ) GetBackBufferDimensions( %d, %d )\n", view.x, view.y, view.width, view.height, nScreenWidth, nScreenHeight );

		if ( view.x != 0 ) // if left of viewport isn't at 0
		{
			pRenderContext->Viewport( view.x, view.y, 1, view.height );
			pRenderContext->ClearColor3ub( 0, 0, 0 );
			pRenderContext->ClearBuffers( true, false );
		}

		if ( ( view.x + view.width ) != nScreenWidth ) // if right of viewport isn't at edge of screen
		{
			pRenderContext->Viewport( view.x + view.width - 1, view.y, 1, view.height );
			pRenderContext->ClearColor3ub( 0, 0, 0 );
			pRenderContext->ClearBuffers( true, false );
		}

		if ( view.y != 0 ) // if top of viewport isn't at 0
		{
			pRenderContext->Viewport( view.x, view.y, view.width, 1 );
			pRenderContext->ClearColor3ub( 0, 0, 0 );
			pRenderContext->ClearBuffers( true, false );
		}

		if ( ( view.y + view.height ) != nScreenHeight ) // if bottom of viewport isn't at edge of screen
		{
			pRenderContext->Viewport( view.x, view.y + view.height - 1, view.width, 1 );
			pRenderContext->ClearColor3ub( 0, 0, 0 );
			pRenderContext->ClearBuffers( true, false );
		}

		pRenderContext->PopRenderTargetAndViewport();
		pRenderContext->Release();
	}

	// Draw the 2D graphics
	m_CurrentView = hudViewSetup;
	pRenderContext = materials->GetRenderContext();

	if( IsPS3() )
	{
#if !defined( CSTRIKE15 )
		extern bool ShouldDrawHudViewfinder();
		// HUD viewfinder has complex material that isn't handled correctly by deferred queuing in material system, so we shouldn't attempt to 
		if( !ShouldDrawHudViewfinder() )
		{
			pRenderContext->AntiAliasingHint( AA_HINT_TEXT );	
		}
#else
		// mdonofrio - Ensure we don't MLAA scaleform/hud rendering
		pRenderContext->AntiAliasingHint( AA_HINT_TEXT );	
#endif // CSTRIKE15
	}

	if ( true )
	{
		PIXEVENT( pRenderContext, "2D Client Rendering" );

		render->Push2DView( pRenderContext, hudViewSetup, 0, saveRenderTarget, GetFrustum() );

		Render2DEffectsPreHUD( hudViewSetup );

		if ( whatToDraw & RENDERVIEW_DRAWHUD )
		{
			VPROF_BUDGET( "VGui_DrawHud", VPROF_BUDGETGROUP_OTHER_VGUI );
			// paint the vgui screen
			VGui_PreRender();

			CUtlVector< vgui::VPANEL > vecHudPanels;

			vecHudPanels.AddToTail( VGui_GetClientDLLRootPanel() );

			// This block is suspect - why are we resizing fullscreen panels to be the size of the hudViewSetup
			// which is potentially only half the screen
			if ( GET_ACTIVE_SPLITSCREEN_SLOT() == 0 )
			{
				vecHudPanels.AddToTail( VGui_GetFullscreenRootVPANEL() );

#if defined( TOOLFRAMEWORK_VGUI_REFACTOR )
				vecHudPanels.AddToTail( enginevgui->GetPanel( PANEL_GAMEUIDLL ) );
#endif
				vecHudPanels.AddToTail( enginevgui->GetPanel( PANEL_CLIENTDLL_TOOLS ) );
			}

			PositionHudPanels( vecHudPanels, hudViewSetup );

			// The crosshair, etc. needs to get at the current setup stuff
			AllowCurrentViewAccess( true );

			// Draw the in-game stuff based on the actual viewport being used
			render->VGui_Paint( PAINT_INGAMEPANELS );

			// Some hud elements want to position themselves based on the on-screen position
			// of simulated actors (players, physics objects, etc). LateThink() gives them
			// a chance to use the final rendering positions of those actors.
            GetHud().LateThink();

			AllowCurrentViewAccess( false );

			VGui_PostRender();

			GetClientMode()->PostRenderVGui();

#if defined( INCLUDE_SCALEFORM )
			pRenderContext->SetScaleformSlotViewport( SF_SS_SLOT( slot ), hudViewSetup.x, hudViewSetup.y, hudViewSetup.width, hudViewSetup.height );
			pRenderContext->AdvanceAndRenderScaleformSlot( SF_SS_SLOT( slot ) );
#endif
			pRenderContext->Flush();
		}

		CDebugViewRender::Draw2DDebuggingInfo( hudViewSetup );

		Render2DEffectsPostHUD( hudViewSetup );

		g_bRenderingView = false;

		// We can no longer use the 'current view' stuff set up in ViewDrawScene
		AllowCurrentViewAccess( false );

		if ( IsPC() )
		{
			CDebugViewRender::GenerateOverdrawForTesting();
		}

		render->PopView( pRenderContext, GetFrustum() );
	}

	//
	// Render a fullscreen rect to wipe alpha.
	// Software that is injecting into present chain
	// is grabbing our buffer with alpha and is able to give
	// away players positioning when players are in or behind
	// smoke.
	//
	if ( IMaterial *pMaterialClearAlpha = materials->FindMaterial( "dev/clearalpha", TEXTURE_GROUP_OTHER, true ) )
	{
		pRenderContext->DrawScreenSpaceQuad( pMaterialClearAlpha );
	}

	pRenderContext.SafeRelease();

	g_WorldListCache.Flush();
	g_viewBuilder.Purge();

	m_CurrentView = view;

#ifdef PARTICLE_USAGE_DEMO
	ParticleUsageDemo();
#endif


}

//-----------------------------------------------------------------------------
// Purpose: Renders extra 2D effects in derived classes while the 2D view is on the stack
//-----------------------------------------------------------------------------
void CViewRender::Render2DEffectsPreHUD( const CViewSetup &view )
{
}

//-----------------------------------------------------------------------------
// Purpose: Renders extra 2D effects in derived classes while the 2D view is on the stack
//-----------------------------------------------------------------------------
void CViewRender::Render2DEffectsPostHUD( const CViewSetup &view )
{
}



//-----------------------------------------------------------------------------
//
// NOTE: Below here is all of the stuff that needs to be done for water rendering
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Determines what kind of water we're going to use
//-----------------------------------------------------------------------------
void CViewRender::DetermineWaterRenderInfo( const VisibleFogVolumeInfo_t &fogVolumeInfo, WaterRenderInfo_t &info )
{
	// By default, assume cheap water (even if there's no water in the scene!)
	info.m_bCheapWater = true;
	info.m_bRefract = false;
	info.m_bReflect = false;
	info.m_bReflectEntities = false;
	info.m_bReflectOnlyMarkedEntities = false;
	info.m_bDrawWaterSurface = false;
	info.m_bOpaqueWater = true;
	info.m_bPseudoTranslucentWater = false;
	info.m_bReflect2DSkybox = false;



	IMaterial *pWaterMaterial = fogVolumeInfo.m_pFogVolumeMaterial;
	if (( fogVolumeInfo.m_nVisibleFogVolume == -1 ) || !pWaterMaterial )
		return;


	// Use cheap water if mat_drawwater is set
	info.m_bDrawWaterSurface = mat_drawwater.GetBool();
	if ( !info.m_bDrawWaterSurface )
	{
		info.m_bOpaqueWater = false;
		return;
	}

#ifdef _GAMECONSOLE
	bool bForceExpensive = false;
#else
	bool bForceExpensive = r_waterforceexpensive.GetBool();
#endif
	bool bForceReflectEntities = r_waterforcereflectentities.GetBool();

	bool bForceCheap = false;
#ifdef PORTAL
	switch( g_pPortalRender->ShouldForceCheaperWaterLevel() )
	{
	case 0: //force cheap water
		info.m_bCheapWater = true;
		bForceCheap = true;
		return;

	case 1: //downgrade level to "simple reflection"
		bForceExpensive = false;

	case 2: //downgrade level to "reflect world"
		bForceReflectEntities = false;
	
	default:
		break;
	};
#endif

	// Determine if the water surface is opaque or not
	info.m_bOpaqueWater = !pWaterMaterial->IsTranslucent();

	// The material can override the default settings though
	IMaterialVar *pForceCheapVar = pWaterMaterial->FindVar( "$forcecheap", NULL, false );
	IMaterialVar *pForceExpensiveVar = pWaterMaterial->FindVar( "$forceexpensive", NULL, false );

	if ( !bForceCheap && pForceCheapVar && pForceCheapVar->IsDefined() )
	{
		bForceCheap = ( pForceCheapVar->GetIntValueFast() != 0 );			
	}

	if ( bForceCheap )
	{
		bForceExpensive = false;
	}
	if ( !bForceCheap && pForceExpensiveVar && pForceExpensiveVar->IsDefined() )
	{
		 bForceExpensive = bForceExpensive || ( pForceExpensiveVar->GetIntValueFast() != 0 );
	}

	bool bDebugCheapWater = r_debugcheapwater.GetBool();
	if( bDebugCheapWater )
	{
		Msg( "Water material: %s dist to water: %f\nforcecheap: %s forceexpensive: %s\n", 
			pWaterMaterial->GetName(), fogVolumeInfo.m_flDistanceToWater, 
			bForceCheap ? "true" : "false", bForceExpensive ? "true" : "false" );
	}

	// Unless expensive water is active, reflections are off.
	bool bLocalReflection;
#ifdef _GAMECONSOLE
	if( !r_WaterDrawReflection.GetBool() )
#else
	if( !bForceExpensive || !r_WaterDrawReflection.GetBool() )
#endif
	{
		bLocalReflection = false;
	}
	else
	{
		IMaterialVar *pReflectTextureVar = pWaterMaterial->FindVar( "$reflecttexture", NULL, false );
		bLocalReflection = pReflectTextureVar && (pReflectTextureVar->GetType() == MATERIAL_VAR_TYPE_TEXTURE);
	}

	// Brian says FIXME: I disabled cheap water LOD when local specular is specified.
	// There are very few places that appear to actually
	// take advantage of it (places where water is in the PVS, but outside of LOD range).
	// It was 2 hours before code lock, and I had the choice of either doubling fill-rate everywhere
	// by making cheap water lod actually work (the water LOD wasn't actually rendering!!!)
	// or to just always render the reflection + refraction if there's a local specular specified.
	// Note that water LOD *does* work with refract-only water

	// Gary says: I'm reverting this change so that water LOD works on dx9 for ep2.

	// Check if the water is out of the cheap water LOD range; if so, use cheap water
#ifdef _GAMECONSOLE
	if ( !bForceExpensive && ( bForceCheap || ( fogVolumeInfo.m_flDistanceToWater >= m_flCheapWaterEndDistance ) ) )
	{
		return;
	}
#else
	if ( ( (fogVolumeInfo.m_flDistanceToWater >= m_flCheapWaterEndDistance) && !bLocalReflection ) || bForceCheap )
 		return;
#endif
	// Get the material that is for the water surface that is visible and check to see
	// what render targets need to be rendered, if any.
	if ( !r_WaterDrawRefraction.GetBool() )
	{
		info.m_bRefract = false;
	}
	else
	{
		IMaterialVar *pRefractTextureVar = pWaterMaterial->FindVar( "$refracttexture", NULL, false );
		info.m_bRefract = pRefractTextureVar && (pRefractTextureVar->GetType() == MATERIAL_VAR_TYPE_TEXTURE);

		// Refractive water can be seen through
		if ( info.m_bRefract )
		{
			info.m_bOpaqueWater = false;
		}
	}

	if ( !info.m_bRefract )
	{
		info.m_bPseudoTranslucentWater = pWaterMaterial->GetMaterialVarFlag( MATERIAL_VAR_PSEUDO_TRANSLUCENT );
		if ( info.m_bPseudoTranslucentWater )
		{
			info.m_bOpaqueWater = false;
		}
	}

	info.m_bReflect = bLocalReflection;
	if ( info.m_bReflect )
	{
		if( bForceReflectEntities )
		{
			info.m_bReflectEntities = true;
		}
		else
		{
			IMaterialVar *pReflectEntitiesVar = pWaterMaterial->FindVar( "$reflectentities", NULL, false );
			info.m_bReflectEntities = pReflectEntitiesVar && (pReflectEntitiesVar->GetIntValueFast() != 0);

			// -- PORTAL 2 console perf hack --
			//
			// Force this check on consoles even if the VMT says $reflectentities / $forceexpensive / etc.
			// Unless you explicitly put "$reflectonlymarkedentities 0" in the VMT, you're going to get this feature...
			// This may be somewhat confusing but it seems like the most straight-forward way to avoid perf regressions due to people 
			// making water VMTs naively without considering console performance.
			if ( !info.m_bReflectEntities || IsGameConsole() )
			{
				bool bFound = false;
				IMaterialVar *pReflectOnlyMarkedEntitiesVar = pWaterMaterial->FindVar( "$reflectonlymarkedentities", &bFound, false );
				info.m_bReflectOnlyMarkedEntities = IsGameConsole(); // default to using fast reflections on consoles, not on PC
				if ( pReflectOnlyMarkedEntitiesVar && bFound )
				{
					info.m_bReflectOnlyMarkedEntities = ( pReflectOnlyMarkedEntitiesVar->GetIntValueFast() != 0 );
				}

				if ( IsGameConsole() && info.m_bReflectOnlyMarkedEntities )
				{
					info.m_bReflectEntities = false;
				}
			}
		}

		IMaterialVar *pReflect2DSkybox = pWaterMaterial->FindVar( "$reflect2dskybox", NULL, false );
		info.m_bReflect2DSkybox = pReflect2DSkybox && ( pReflect2DSkybox->GetIntValueFast() != 0 );
	}

	info.m_bCheapWater = !info.m_bReflect && !info.m_bRefract;

	if( bDebugCheapWater )
	{
		Warning( "refract: %s reflect: %s\n", info.m_bRefract ? "true" : "false", info.m_bReflect ? "true" : "false" );
	}
}

//-----------------------------------------------------------------------------
// Enables/disables water depth feathering, which requires the scene's depth.
//-----------------------------------------------------------------------------
void CViewRender::EnableWaterDepthFeathing( IMaterial *pWaterMaterial, bool bEnable )
{
	if ( pWaterMaterial )
	{
		bool bFound = false;
		IMaterialVar *pDepthFeather = pWaterMaterial->FindVar( "$depth_feather", &bFound, false );
		if ( ( pDepthFeather ) && ( bFound ) )
		{
			pDepthFeather->SetIntValue( bEnable );
		}
	}
}

//-----------------------------------------------------------------------------
// Draws the world and all entities
//-----------------------------------------------------------------------------
void CViewRender::DrawWorldAndEntities( bool bDrawSkybox, const CViewSetup &viewIn, int nClearFlags, ViewCustomVisibility_t *pCustomVisibility )
{
	MDLCACHE_CRITICAL_SECTION();

	VisibleFogVolumeInfo_t fogVolumeInfo;
#ifdef PORTAL //in portal, we can't use the fog volume for the camera since it's almost never in the same fog volume as what's in front of the portal
	if( g_pPortalRender->GetViewRecursionLevel() == 0 )
	{
		render->GetVisibleFogVolume( viewIn.origin, NULL, &fogVolumeInfo );
	}
	else
	{
		render->GetVisibleFogVolume( g_pPortalRender->GetExitPortalFogOrigin(), &pCustomVisibility->m_VisData, &fogVolumeInfo );
	}
#else
	render->GetVisibleFogVolume( viewIn.origin, NULL, &fogVolumeInfo );
#endif

	WaterRenderInfo_t info;
	DetermineWaterRenderInfo( fogVolumeInfo, info );

	if ( info.m_bCheapWater )
	{		     
		// rg - This code path will probably going away soon, but for now I'm going to fix it so the water looks reasonable on PS3/X360.
		EnableWaterDepthFeathing( fogVolumeInfo.m_pFogVolumeMaterial, true );

		cplane_t glassReflectionPlane;
		if ( IsReflectiveGlassInView( viewIn, glassReflectionPlane ) )
		{								    
			CRefPtr<CReflectiveGlassView> pGlassReflectionView = new CReflectiveGlassView( this );
			pGlassReflectionView->Setup( viewIn, VIEW_CLEAR_DEPTH | VIEW_CLEAR_COLOR, bDrawSkybox, fogVolumeInfo, info, glassReflectionPlane );
			AddViewToScene( pGlassReflectionView );

			CRefPtr<CRefractiveGlassView> pGlassRefractionView = new CRefractiveGlassView( this );
			pGlassRefractionView->Setup( viewIn, VIEW_CLEAR_DEPTH | VIEW_CLEAR_COLOR, bDrawSkybox, fogVolumeInfo, info, glassReflectionPlane );
			AddViewToScene( pGlassRefractionView );
		}

		CRefPtr<CSimpleWorldView> pNoWaterView = new CSimpleWorldView( this );
		pNoWaterView->Setup( viewIn, nClearFlags, bDrawSkybox, fogVolumeInfo, info, pCustomVisibility );
		AddViewToScene( pNoWaterView );

		EnableWaterDepthFeathing( fogVolumeInfo.m_pFogVolumeMaterial, false );

		return;
	}

	Assert( !pCustomVisibility );

	// Blat out the visible fog leaf if we're not going to use it
	if ( !r_ForceWaterLeaf.GetBool() )
	{
		fogVolumeInfo.m_nVisibleFogVolumeLeaf = -1;
	}

	EnableWaterDepthFeathing( fogVolumeInfo.m_pFogVolumeMaterial, true );
	
	// We can see water of some sort
	if ( !fogVolumeInfo.m_bEyeInFogVolume )
	{
		CRefPtr<CAboveWaterView> pAboveWaterView = new CAboveWaterView( this );
		pAboveWaterView->Setup( viewIn, bDrawSkybox, fogVolumeInfo, info );
		AddViewToScene( pAboveWaterView );
	}
	else
	{
		CRefPtr<CUnderWaterView> pUnderWaterView = new CUnderWaterView( this );
		pUnderWaterView->Setup( viewIn, bDrawSkybox, fogVolumeInfo, info );

		AddViewToScene( pUnderWaterView );
	}

	EnableWaterDepthFeathing( fogVolumeInfo.m_pFogVolumeMaterial, false );
}


//-----------------------------------------------------------------------------
// Pushes a water render target
//-----------------------------------------------------------------------------
static Vector s_vSavedLinearLightMapScale(-1,-1,-1); // x<0 = no saved scale

static void SetLightmapScaleForWater(void)
{
	if (g_pMaterialSystemHardwareConfig->GetHDRType()==HDR_TYPE_INTEGER)
	{
		CMatRenderContextPtr pRenderContext( materials );
		s_vSavedLinearLightMapScale=pRenderContext->GetToneMappingScaleLinear();
		Vector t25=s_vSavedLinearLightMapScale;
		t25*=0.25;
		pRenderContext->SetToneMappingScaleLinear(t25);
	}
}

//-----------------------------------------------------------------------------
// Returns true if the view plane intersects the water
//-----------------------------------------------------------------------------
bool DoesViewPlaneIntersectWater( float waterZ, int leafWaterDataID )
{
	if ( leafWaterDataID == -1 )
		return false;

#ifdef PORTAL //when rendering portal views point/plane intersections just don't cut it.
	if( g_pPortalRender->GetViewRecursionLevel() != 0 )
		return g_pPortalRender->DoesExitPortalViewIntersectWaterPlane( waterZ, leafWaterDataID );
#endif

	CMatRenderContextPtr pRenderContext( materials );
	
	VMatrix viewMatrix, projectionMatrix, viewProjectionMatrix, inverseViewProjectionMatrix;
	pRenderContext->GetMatrix( MATERIAL_VIEW, &viewMatrix );
	pRenderContext->GetMatrix( MATERIAL_PROJECTION, &projectionMatrix );
	MatrixMultiply( projectionMatrix, viewMatrix, viewProjectionMatrix );
	MatrixInverseGeneral( viewProjectionMatrix, inverseViewProjectionMatrix );

	Vector mins, maxs;
	ClearBounds( mins, maxs );
	Vector testPoint[4];
	testPoint[0].Init( -1.0f, -1.0f, 0.0f );
	testPoint[1].Init( -1.0f, 1.0f, 0.0f );
	testPoint[2].Init( 1.0f, -1.0f, 0.0f );
	testPoint[3].Init( 1.0f, 1.0f, 0.0f );
	int i;
	bool bAbove = false;
	bool bBelow = false;
	float fudge = 7.0f;
	for( i = 0; i < 4; i++ )
	{
		Vector worldPos;
		Vector3DMultiplyPositionProjective( inverseViewProjectionMatrix, testPoint[i], worldPos );
		AddPointToBounds( worldPos, mins, maxs );
//		Warning( "viewplanez: %f waterZ: %f\n", worldPos.z, waterZ );
		if( worldPos.z + fudge > waterZ )
		{
			bAbove = true;
		}
		if( worldPos.z - fudge < waterZ )
		{
			bBelow = true;
		}
	}

	// early out if the near plane doesn't cross the z plane of the water.
	if( !( bAbove && bBelow ) )
		return false;

	Vector vecFudge( fudge, fudge, fudge );
	mins -= vecFudge;
	maxs += vecFudge;
	
	// the near plane does cross the z value for the visible water volume.  Call into
	// the engine to find out if the near plane intersects the water volume.
	return render->DoesBoxIntersectWaterVolume( mins, maxs, leafWaterDataID );
} 

#ifdef PORTAL 

//-----------------------------------------------------------------------------
// Purpose: Draw the scene during another draw scene call. We must draw our portals
//			after opaques but before translucents, so this ViewDrawScene resets the view
//			and doesn't flag the rendering as ended when it ends.
// Input  : bDrawSkybox - do we draw the skybox
//			&view - the camera view to render from
//			nClearFlags -  how to clear the buffer
//-----------------------------------------------------------------------------
void CViewRender::ViewDrawScene_PortalStencil( const CViewSetup &viewIn, ViewCustomVisibility_t *pCustomVisibility )
{
	VPROF_BUDGET( "CViewRender::ViewDrawScene_PortalStencil", "ViewDrawScene_PortalStencil" );

	CViewSetup view( viewIn );

	// Record old view stats
	Vector vecOldOrigin = CurrentViewOrigin();
	QAngle vecOldAngles = CurrentViewAngles();

	int iCurrentViewID = g_CurrentViewID;
#if defined( DBGFLAG_ASSERT )
	int iRecursionLevel = g_pPortalRender->GetViewRecursionLevel();
	Assert( iRecursionLevel > 0 );
#endif

	bool bDrew3dSkybox = false;
	SkyboxVisibility_t nSkyboxVisible = SKYBOX_NOT_VISIBLE;
	int iClearFlags = 0;

	Draw3dSkyboxworld_Portal( view, iClearFlags, bDrew3dSkybox, nSkyboxVisible );

	bool drawSkybox = r_skybox.GetBool();
	if ( bDrew3dSkybox || ( nSkyboxVisible == SKYBOX_NOT_VISIBLE ) )
	{
		drawSkybox = false;
	}

	//generate unique view ID's for each stencil view
	view_id_t iNewViewID = (view_id_t)g_pPortalRender->GetCurrentViewId();
	SetupCurrentView( view.origin, view.angles, (view_id_t)iNewViewID );
	
	// update vis data
	unsigned int visFlags;
	SetupVis( view, visFlags, pCustomVisibility );

	VisibleFogVolumeInfo_t fogInfo;
	if( g_pPortalRender->GetViewRecursionLevel() == 0 )
	{
		render->GetVisibleFogVolume( view.origin, NULL, &fogInfo );
	}
	else
	{
		render->GetVisibleFogVolume( g_pPortalRender->GetExitPortalFogOrigin(), &pCustomVisibility->m_VisData, &fogInfo );
	}

	WaterRenderInfo_t waterInfo;
	DetermineWaterRenderInfo( fogInfo, waterInfo );

	if ( waterInfo.m_bCheapWater )
	{		     
		// Only bother to enable depth feathering with water seen through up to a single portal when cheap_water is active.
		if ( g_pPortalRender->GetViewRecursionLevel() <= 1)
		{
			EnableWaterDepthFeathing( fogInfo.m_pFogVolumeMaterial, true );
		}
		
		cplane_t glassReflectionPlane;
		if ( IsReflectiveGlassInView( viewIn, glassReflectionPlane ) )
		{								    
			CRefPtr<CReflectiveGlassView> pGlassReflectionView = new CReflectiveGlassView( this );
			pGlassReflectionView->Setup( viewIn, VIEW_CLEAR_DEPTH | VIEW_CLEAR_COLOR | VIEW_CLEAR_OBEY_STENCIL, drawSkybox, fogInfo, waterInfo, glassReflectionPlane );
			AddViewToScene( pGlassReflectionView );

			CRefPtr<CRefractiveGlassView> pGlassRefractionView = new CRefractiveGlassView( this );
			pGlassRefractionView->Setup( viewIn, VIEW_CLEAR_DEPTH | VIEW_CLEAR_COLOR | VIEW_CLEAR_OBEY_STENCIL, drawSkybox, fogInfo, waterInfo, glassReflectionPlane );
			AddViewToScene( pGlassRefractionView );
		}

		CSimpleWorldView *pClientView = new CSimpleWorldView( this );
		pClientView->Setup( view, VIEW_CLEAR_OBEY_STENCIL, drawSkybox, fogInfo, waterInfo, pCustomVisibility );
		AddViewToScene( pClientView );
		SafeRelease( pClientView );

		EnableWaterDepthFeathing( fogInfo.m_pFogVolumeMaterial, false );
	}
	else
	{
		EnableWaterDepthFeathing( fogInfo.m_pFogVolumeMaterial, true );
		
		// We can see water of some sort
		if ( !fogInfo.m_bEyeInFogVolume )
		{
			CRefPtr<CAboveWaterView> pAboveWaterView = new CAboveWaterView( this );
			pAboveWaterView->Setup( viewIn, drawSkybox, fogInfo, waterInfo, pCustomVisibility );
			AddViewToScene( pAboveWaterView );
		}
		else
		{
			CRefPtr<CUnderWaterView> pUnderWaterView = new CUnderWaterView( this );
			pUnderWaterView->Setup( viewIn, drawSkybox, fogInfo, waterInfo, pCustomVisibility );
			AddViewToScene( pUnderWaterView );
		}

		EnableWaterDepthFeathing( fogInfo.m_pFogVolumeMaterial, false );
	}
	
	// Disable fog for the rest of the stuff
	DisableFog();

	CGlowOverlay::DrawOverlays( view.m_bCacheFullSceneState );

	// Draw rain..
	DrawPrecipitation();

	//prerender version only
	// issue the pixel visibility tests
	PixelVisibility_EndCurrentView();

	// Make sure sound doesn't stutter
	engine->Sound_ExtraUpdate();

	// Debugging info goes over the top
	CDebugViewRender::Draw3DDebuggingInfo( view );

	// Return to the previous view
	SetupCurrentView( vecOldOrigin, vecOldAngles, (view_id_t)iCurrentViewID );
	g_CurrentViewID = iCurrentViewID; //just in case the cast to view_id_t screwed up the id #
}

void CViewRender::Draw3dSkyboxworld_Portal( const CViewSetup &view, int &nClearFlags, bool &bDrew3dSkybox, SkyboxVisibility_t &nSkyboxVisible, ITexture *pRenderTarget ) 
{ 
	CRefPtr<CPortalSkyboxView> pSkyView = new CPortalSkyboxView( this ); 
	if ( ( bDrew3dSkybox = pSkyView->Setup( view, &nClearFlags, &nSkyboxVisible, pRenderTarget ) ) == true )
	{
		AddViewToScene( pSkyView );
	}
}

#endif //PORTAL



#ifdef PORTAL2
void CViewRender::ViewDrawPhoto( ITexture *pRenderTarget, C_BaseEntity *pTargetEntity )
{
	CRefPtr<CAperturePhotoView> pPhotoView = new CAperturePhotoView( this ); 
	int nClearFlags = VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH | VIEW_CLEAR_STENCIL;
	SkyboxVisibility_t nSkyboxVisible = SKYBOX_NOT_VISIBLE;

	CMatRenderContextPtr pRenderContext( materials );
	// pRenderContext->SetStencilEnable( false ); // FIXME: JDW

	//bool bIsDormant = pTargetEntity->IsDormant();
	//pTargetEntity->m_bDormant = false;

	//IClientRenderable *pEntRenderable = pTargetEntity->GetClientRenderable();

	bool bNoDraw = pTargetEntity->IsEffectActive( EF_NODRAW );
	if( bNoDraw )
	{
		pTargetEntity->RemoveEffects( EF_NODRAW );
	}

	bool bHandle = pTargetEntity->GetRenderHandle() == INVALID_CLIENT_RENDER_HANDLE;
	if( bHandle )
	{
		ClientLeafSystem()->AddRenderable( pTargetEntity, false, RENDERABLE_IS_OPAQUE, RENDERABLE_MODEL_UNKNOWN_TYPE );
		ClientLeafSystem()->RenderableChanged( pTargetEntity->m_hRender );
		ClientLeafSystem()->PreRender();
	}

	//bool bShouldDraw = pEntRenderable->ShouldDraw();
	//bool bIsVisible = pTargetEntity->IsVisible();

	Assert( pTargetEntity->ShouldDraw() );

	CViewSetup photoview = m_CurrentView;
	photoview.width = pRenderTarget->GetActualWidth();
	photoview.height = pRenderTarget->GetActualHeight();
	photoview.x = 0;
	photoview.y = 0;
	//photoview.origin = pCameraEnt->GetAbsOrigin();
	//photoview.angles = pCameraEnt->GetAbsAngles();
	//photoview.fov = pCameraEnt->GetFOV();
	photoview.m_bOrtho = false;
	//photoview.m_flAspectRatio = 0.0f;
	//(*this) = photoview;

	SetupCurrentView( photoview.origin, photoview.angles, VIEW_MONITOR );

	Frustum frustum;
	render->Push3DView( pRenderContext, photoview, VIEW_CLEAR_DEPTH | VIEW_CLEAR_COLOR, pRenderTarget, (VPlane *)frustum );

	//HACKHACK: need to setup a proper view
	if( pPhotoView->Setup( pTargetEntity, photoview, &nClearFlags, &nSkyboxVisible, pRenderTarget ) == true )
	{
		AddViewToScene( pPhotoView );
	}

	render->PopView( pRenderContext, frustum );

	//pTargetEntity->m_bDormant = bIsDormant;

	if( bHandle )
	{
		ClientLeafSystem()->RemoveRenderable( pTargetEntity->GetRenderHandle() );
	}

	if( bNoDraw )
	{
		pTargetEntity->AddEffects( EF_NODRAW );
	}
}
#endif

//-----------------------------------------------------------------------------
// Methods related to controlling the cheap water distance
//-----------------------------------------------------------------------------
void CViewRender::SetCheapWaterStartDistance( float flCheapWaterStartDistance )
{
	m_flCheapWaterStartDistance = flCheapWaterStartDistance;
}

void CViewRender::SetCheapWaterEndDistance( float flCheapWaterEndDistance )
{
	m_flCheapWaterEndDistance = flCheapWaterEndDistance;
}

void CViewRender::GetWaterLODParams( float &flCheapWaterStartDistance, float &flCheapWaterEndDistance )
{
	flCheapWaterStartDistance = m_flCheapWaterStartDistance;
	flCheapWaterEndDistance = m_flCheapWaterEndDistance;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &view - 
//			&introData - 
//-----------------------------------------------------------------------------
void CViewRender::ViewDrawScene_Intro( const CViewSetup &view, int nClearFlags, const IntroData_t &introData )
{
	VPROF( "CViewRender::ViewDrawScene" );

	PS3_SPUPATH_INVALID( "CViewRender::ViewDrawScene_Intro" );

	CMatRenderContextPtr pRenderContext( materials );

	// this allows the refract texture to be updated once per *scene* on 360
	// (e.g. once for a monitor scene and once for the main scene)
	g_viewscene_refractUpdateFrame = gpGlobals->framecount - 1;

	// -----------------------------------------------------------------------
	// Set the clear color to black since we are going to be adding up things
	// in the frame buffer.
	// -----------------------------------------------------------------------
	// Clear alpha to 255 so that masking with the vortigaunts (0) works properly.
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
	
	// -----------------------------------------------------------------------
	// Draw the primary scene and copy it to the first framebuffer texture
	// -----------------------------------------------------------------------	
	unsigned int visFlags;

	// NOTE: We only increment this once since time doesn't move forward.
	ParticleMgr()->IncrementFrameCode();

	if( introData.m_bDrawPrimary  )
	{
		CViewSetup playerView( view );
		playerView.origin = introData.m_vecCameraView;
		playerView.angles = introData.m_vecCameraViewAngles;
		if ( introData.m_playerViewFOV )
		{
			playerView.fov = ScaleFOVByWidthRatio( introData.m_playerViewFOV, engine->GetScreenAspectRatio( view.width, view.height ) / ( 4.0f / 3.0f ) );
		}

		g_pClientShadowMgr->PreRender();

		// Shadowed flashlights supported on ps_2_b and up...
		if ( r_flashlightdepthtexture.GetBool() )
		{
#if defined(_PS3)
			g_pClientShadowMgr->ComputeShadowDepthTextures( playerView, g_viewBuilder.GetPassFlags() & PASS_BUILDLISTS_PS3 );
#else
			g_pClientShadowMgr->ComputeShadowDepthTextures( playerView, ( g_viewBuilder.GetPassFlags() == PASS_BUILDLISTS ) );
#endif
		}

		SetupCurrentView( playerView.origin, playerView.angles, VIEW_INTRO_PLAYER );

		// Invoke pre-render methods
		IGameSystem::PreRenderAllSystems();

		// Start view, clear frame/z buffer if necessary
		SetupVis( playerView, visFlags );
		
		render->Push3DView( pRenderContext, playerView, VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH, NULL, GetFrustum() );
		DrawWorldAndEntities( true /* drawSkybox */, playerView, VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH  );
		render->PopView( pRenderContext, GetFrustum() );

		// Free shadow depth textures for use in future view
		if ( r_flashlightdepthtexture.GetBool() )
		{
			g_pClientShadowMgr->UnlockAllShadowDepthTextures();
		}
	}
	else
	{
		pRenderContext->ClearBuffers( true, true );
	}
	Rect_t actualRect;
	UpdateScreenEffectTexture( 0, view.x, view.y, view.width, view.height, false, &actualRect );

	g_pClientShadowMgr->PreRender();

	// Shadowed flashlights supported on ps_2_b and up...
	if ( r_flashlightdepthtexture.GetBool() )
	{
		g_pClientShadowMgr->ComputeShadowDepthTextures( view );
	}

	// -----------------------------------------------------------------------
	// Draw the secondary scene and copy it to the second framebuffer texture
	// -----------------------------------------------------------------------
	SetupCurrentView( view.origin, view.angles, VIEW_INTRO_CAMERA );

	// Invoke pre-render methods
	IGameSystem::PreRenderAllSystems();

	// Start view, clear frame/z buffer if necessary
	SetupVis( view, visFlags );

	// Clear alpha to 255 so that masking with the vortigaunts (0) works properly.
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );

	DrawWorldAndEntities( true /* drawSkybox */, view, VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH  );

	UpdateScreenEffectTexture( 1, view.x, view.y, view.width, view.height );

	// -----------------------------------------------------------------------
	// Draw quads on the screen for each screenspace pass.
	// -----------------------------------------------------------------------
	// Find the material that we use to render the overlays
	IMaterial *pOverlayMaterial = materials->FindMaterial( "scripted/intro_screenspaceeffect", TEXTURE_GROUP_OTHER );
	IMaterialVar *pModeVar = pOverlayMaterial->FindVar( "$mode", NULL );
	IMaterialVar *pAlphaVar = pOverlayMaterial->FindVar( "$alpha", NULL );

	pRenderContext->ClearBuffers( true, true );
	
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	
	int passID;
	for( passID = 0; passID < introData.m_Passes.Count(); passID++ )
	{
		const IntroDataBlendPass_t& pass = introData.m_Passes[passID];
		if ( pass.m_Alpha == 0 )
			continue;

		// Pick one of the blend modes for the material.
		if ( pass.m_BlendMode >= 0 && pass.m_BlendMode <= 9 )
		{
			pModeVar->SetIntValue( pass.m_BlendMode );
		}
		else
		{
			Assert(0);
		}
		// Set the alpha value for the material.
		pAlphaVar->SetFloatValue( pass.m_Alpha );
		
		// Draw a quad for this pass.
		ITexture *pTexture = GetFullFrameFrameBufferTexture( 0 );
		pRenderContext->DrawScreenSpaceRectangle( pOverlayMaterial, view.x, view.y, view.width, view.height,
											actualRect.x, actualRect.y, actualRect.x+actualRect.width-1, actualRect.y+actualRect.height-1, 
											pTexture->GetActualWidth(), pTexture->GetActualHeight() );
	}
	
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();
	
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
	
	// Draw the starfield
	// FIXME
	// blur?
	
	// Disable fog for the rest of the stuff
	DisableFog();
	
	// Here are the overlays...
	CGlowOverlay::DrawOverlays( view.m_bCacheFullSceneState );

	BEGIN_2PASS_DRAW_BLOCK
	// issue the pixel visibility tests
	PixelVisibility_EndCurrentView();
	END_2PASS_BLOCK

	// And here are the screen-space effects
	PerformScreenSpaceEffects( view.x, view.y, view.width, view.height );

	// Make sure sound doesn't stutter
	engine->Sound_ExtraUpdate();

	// Debugging info goes over the top
	CDebugViewRender::Draw3DDebuggingInfo( view );

	// Let the particle manager simulate things that haven't been simulated.
	ParticleMgr()->PostRender();

	FinishCurrentView();

	// Free shadow depth textures for use in future view
	if ( r_flashlightdepthtexture.GetBool() )
	{
		g_pClientShadowMgr->UnlockAllShadowDepthTextures();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets up scene and renders camera view
// Input  : cameraNum - 
//			&cameraView
//			*localPlayer - 
//			x - 
//			y - 
//			width - 
//			height - 
//			highend - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CViewRender::DrawOneMonitor( ITexture *pRenderTarget, int cameraNum, C_PointCamera *pCameraEnt, 
	const CViewSetup &cameraView, C_BasePlayer *localPlayer, int x, int y, int width, int height )
{
	PS3_SPUPATH_INVALID( "CViewRender::DrawOneMonitor" );

#ifdef USE_MONITORS
	VPROF_INCREMENT_COUNTER( "cameras rendered", 1 );
	// Setup fog state for the camera.
	fogparams_t oldFogParams;
	float flOldZFar = 0.0f;

	bool fogEnabled = pCameraEnt->IsFogEnabled();

	CViewSetup monitorView = cameraView;

	fogparams_t *pFogParams = NULL;

	if ( fogEnabled )
	{	
		if ( !localPlayer )
			return false;

		pFogParams = localPlayer->GetFogParams();

		// Save old fog data.
		oldFogParams = *pFogParams;
		flOldZFar = cameraView.zFar;

		pFogParams->enable = true;
		pFogParams->start = pCameraEnt->GetFogStart();
		pFogParams->end = pCameraEnt->GetFogEnd();
		pFogParams->farz = pCameraEnt->GetFogEnd();
		pFogParams->maxdensity = pCameraEnt->GetFogMaxDensity();

		unsigned char r, g, b;
		pCameraEnt->GetFogColor( r, g, b );
		pFogParams->colorPrimary.SetR( r );
		pFogParams->colorPrimary.SetG( g );
		pFogParams->colorPrimary.SetB( b );

		monitorView.zFar = pCameraEnt->GetFogEnd();
	}

	monitorView.width = width;
	monitorView.height = height;
	monitorView.x = x;
	monitorView.y = y;
	monitorView.origin = pCameraEnt->GetAbsOrigin();
	monitorView.angles = pCameraEnt->GetAbsAngles();
	monitorView.fov = pCameraEnt->GetFOV();
	monitorView.m_bOrtho = false;
	monitorView.m_flAspectRatio = pCameraEnt->UseScreenAspectRatio() ? 0.0f : 1.0f;

	// @MULTICORE (toml 8/11/2006): this should be a renderer....
	CMatRenderContextPtr pRenderContext( materials );
	
	// Monitors do not support flashlight shadows because the depth texture isn't initialized until the main view (VIEW_MAIN) renders
	FlashlightState_t nullFlashlight;
	VMatrix matIdentity;
	matIdentity.Identity();
	pRenderContext->SetFlashlightState( nullFlashlight, matIdentity );
	
 	render->Push3DView( pRenderContext, monitorView, VIEW_CLEAR_DEPTH | VIEW_CLEAR_COLOR, pRenderTarget, GetFrustum() );
	ViewDrawScene( false, SKYBOX_2DSKYBOX_VISIBLE, monitorView, 0, VIEW_MONITOR );
 	render->PopView( pRenderContext, GetFrustum() );

	// Reset the world fog parameters.
	if ( fogEnabled )
	{
		if ( pFogParams )
		{
			*pFogParams = oldFogParams;
		}
		monitorView.zFar = flOldZFar;
	}
#endif // USE_MONITORS
	return true;
}

void CViewRender::DrawMonitors( const CViewSetup &cameraView )
{
#ifdef USE_MONITORS

	// Early out if no cameras
	C_PointCamera *pCameraEnt = GetPointCameraList();
	if ( !pCameraEnt )
		return;

#ifdef _DEBUG
	g_bRenderingCameraView = true;
#endif

	// FIXME: this should check for the ability to do a render target maybe instead.
	// FIXME: shouldn't have to truck through all of the visible entities for this!!!!
	ITexture *pCameraTarget = GetCameraTexture();
	int width = pCameraTarget->GetActualWidth();
	int height = pCameraTarget->GetActualHeight();

	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	
	int cameraNum;
	for ( cameraNum = 0; pCameraEnt != NULL; pCameraEnt = pCameraEnt->m_pNext )
	{
		if ( !pCameraEnt->IsActive() || pCameraEnt->IsDormant() )
			continue;

		if ( !DrawOneMonitor( pCameraTarget, cameraNum, pCameraEnt, cameraView, player, 0, 0, width, height ) )
			continue;

		++cameraNum;
	}

	if ( IsGameConsole() && cameraNum > 0 )
	{
		// resolve render target to system memory texture
		// resolving *after* all monitors drawn to ensure a single blit using fastest resolve path
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->PushRenderTargetAndViewport( pCameraTarget );
		pRenderContext->CopyRenderTargetToTextureEx( pCameraTarget, 0, NULL, NULL );
		pRenderContext->PopRenderTargetAndViewport();
	}

#ifdef _DEBUG
	g_bRenderingCameraView = false;
#endif

#endif // USE_MONITORS
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

ClientWorldListInfo_t *ClientWorldListInfo_t::AllocPooled( const ClientWorldListInfo_t &exemplar )
{
	ClientWorldListInfo_t *pResult = gm_Pool.GetObject();

	size_t nBytes = AlignValue( ( exemplar.m_LeafCount * (sizeof(WorldListLeafData_t) + sizeof(exemplar.m_pLeafDataList[0]))), 4096 );
	byte *pMemory = (byte *)pResult->m_pLeafDataList;

	if ( pMemory )
	{
		// Previously allocated, add a reference. Otherwise comes out of GetObject as a new object with a refcount of 1
		pResult->AddRef();
	}

	if ( !pMemory || _msize( pMemory ) < nBytes )
	{
		pMemory = (byte *)realloc( pMemory, nBytes );
	}

	pResult->m_pLeafDataList = (WorldListLeafData_t*)pMemory;
	pResult->m_pOriginalLeafIndex = (uint16*)( (byte *)( pResult->m_pLeafDataList ) + exemplar.m_LeafCount * sizeof(exemplar.m_pLeafDataList[0]) );

	pResult->m_bPooledAlloc = true;

	return pResult;
}

bool ClientWorldListInfo_t::OnFinalRelease()
{
	if ( m_bPooledAlloc )
	{
		Assert( m_pLeafDataList );
		gm_Pool.PutObject( this );
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CBase3dView::CBase3dView( CViewRender *pMainView ) :
	m_pMainView( pMainView ),
	m_Frustum( pMainView->m_Frustum ),
	m_nSlot( GET_ACTIVE_SPLITSCREEN_SLOT() )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEnt - 
// Output : int
//-----------------------------------------------------------------------------
VPlane* CBase3dView::GetFrustum()
{
	// The frustum is only valid while in a RenderView call.
	// @MULTICORE (toml 8/11/2006): reimplement this when ready -- Assert(g_bRenderingView || g_bRenderingCameraView || g_bRenderingScreenshot);	
	return m_Frustum;
}


CObjectPool<ClientWorldListInfo_t> ClientWorldListInfo_t::gm_Pool;


#if defined(_PS3)
CClientRenderablesList g_RenderablesPool[ MAX_CONCURRENT_BUILDVIEWS ];
ClientWorldListInfo_t  g_WorldListInfoPool[ MAX_CONCURRENT_BUILDVIEWS ];
#endif

//-----------------------------------------------------------------------------
// Base class for 3d views
//-----------------------------------------------------------------------------
CRendering3dView::CRendering3dView(CViewRender *pMainView) :
	CBase3dView( pMainView ),
	m_pWorldRenderList( NULL ), 
#if !defined(_PS3)
	m_pRenderables( NULL ),
	m_pWorldListInfo( NULL ), 
#endif
	m_pCustomVisibility( NULL ),
	m_DrawFlags( 0 ),
	m_ClearFlags( 0 )
{

#if defined( CSTRIKE15 ) && defined(_PS3)
	BEGIN_2PASS_BUILD_BLOCK
	for( int i = 0; i < MAX_CONCURRENT_BUILDVIEWS; i++ )
	{
		m_pRenderablesList[ i ] = NULL;
		m_pWorldListInfo[ i ]   = NULL;
	}
	END_2PASS_BLOCK
#endif

}


//-----------------------------------------------------------------------------
// Sort entities in a back-to-front ordering
//-----------------------------------------------------------------------------
void CRendering3dView::Setup( const CViewSetup &setup )
{
	// @MULTICORE (toml 8/15/2006): don't reset if parameters don't require it. For now, just reset
	memcpy( static_cast<CViewSetup *>(this), &setup, sizeof( setup ) );
	ReleaseLists();

#if defined( CSTRIKE15 ) && defined(_PS3)
	// only want this statically allocated once ever really, or use a mempool
 	for( int i = 0; i < MAX_CONCURRENT_BUILDVIEWS; i++ )
 	{
 		m_pRenderablesList[ i ] = &g_RenderablesPool[ i ];
		m_pWorldListInfo[ i ]   = &g_WorldListInfoPool[ i ];
 	}
#else
	//m_pRenderables = new CClientRenderablesList; 
#endif

	m_pCustomVisibility = NULL;
}


//-----------------------------------------------------------------------------
// Sort entities in a back-to-front ordering
//-----------------------------------------------------------------------------
void CRendering3dView::ReleaseLists()
{

#if defined( CSTRIKE15 ) && defined(_PS3)
	for( int i = 0; i < MAX_CONCURRENT_BUILDVIEWS; i++ )
	{
		m_pRenderablesList[ i ] = NULL;
		m_pWorldListInfo[ i ]   = NULL;
	}
#else
	SafeRelease( m_pWorldRenderList );

	SafeRelease( m_pRenderables );
	SafeRelease( m_pWorldListInfo );
#endif

	m_pCustomVisibility = NULL;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

class SetupRenderablesListJob : public CJob
{
public:
	explicit SetupRenderablesListJob( const SetupRenderInfo_t &info ) : m_SetupInfo( info ) {}

private:

	virtual JobStatus_t	DoExecute();

	SetupRenderInfo_t m_SetupInfo;
};

JobStatus_t	SetupRenderablesListJob::DoExecute()
{
	ClientLeafSystem()->BuildRenderablesList( m_SetupInfo );
	return JOB_OK;
}

void CRendering3dView::SetupRenderablesList( int viewID, bool bFastEntityRendering, bool bDrawDepthViewNonCachedObjectsOnly )
{
//	VPROF( "CViewRender::SetupRenderablesList" );

	VPROF_BUDGET( "SetupRenderablesList", "SetupRenderablesList" );

#if !defined( _PS3 )
	// Create the list
	m_pRenderables = new CClientRenderablesList; 
	g_viewBuilder.SetRenderablesListElement( m_pRenderables );
#endif

	// Clear the list.
	int i;
	for( i=0; i < RENDER_GROUP_COUNT; i++ )
	{
#if defined( CSTRIKE15 ) && defined(_PS3)
		m_pRenderablesList[ g_viewBuilder.GetBuildViewID() ]->m_RenderGroupCounts[i] = 0;
#else
		m_pRenderables->m_RenderGroupCounts[i] = 0;
#endif
	}

	// Now collate the entities in the leaves.
	if( !m_pMainView->ShouldDrawEntities() )
		return;

	m_pMainView->IncRenderablesListsNumber();

	// Precache information used commonly in CollateRenderables
	SetupRenderInfo_t setupInfo;
	setupInfo.m_nRenderFrame = m_pMainView->BuildRenderablesListsNumber();	// only one incremented?
	setupInfo.m_nDetailBuildFrame = m_pMainView->BuildWorldListsNumber();	//
#if defined( CSTRIKE15 ) && defined(_PS3)
	setupInfo.m_pWorldListInfo = m_pWorldListInfo[ g_viewBuilder.GetBuildViewID() ];
	setupInfo.m_pRenderList    = m_pRenderablesList[ g_viewBuilder.GetBuildViewID() ];
#endif
	setupInfo.m_bDrawDetailObjects = GetClientMode()->ShouldDrawDetailObjects() && r_DrawDetailProps.GetInt();
	if ( m_bCSMView )
	{
		setupInfo.m_bDrawTranslucentObjects = cl_csm_translucent_shadows.GetBool();
	}
	else
	{
		setupInfo.m_bDrawTranslucentObjects = ( r_flashlightdepth_drawtranslucents.GetBool() || (viewID != VIEW_SHADOW_DEPTH_TEXTURE) );
	}
	setupInfo.m_nViewID = viewID;
	setupInfo.m_nBuildViewID = g_viewBuilder.GetBuildViewID();
	setupInfo.m_nOcclustionViewID = engine->GetOcclusionViewId();
	setupInfo.m_vecRenderOrigin = origin;
	setupInfo.m_vecRenderForward = CurrentViewForward();
	setupInfo.m_bFastEntityRendering = bFastEntityRendering;
	setupInfo.m_bDrawDepthViewNonCachedObjectsOnly = bDrawDepthViewNonCachedObjectsOnly;
	setupInfo.m_bCSMView = m_bCSMView;
	ComputeScreenSizeInfo( &setupInfo.m_screenSizeInfo );

	float fMaxDist = cl_maxrenderable_dist.GetFloat();

	// Shadowing light typically has a smaller farz than cl_maxrenderable_dist
	setupInfo.m_flRenderDistSq = (viewID == VIEW_SHADOW_DEPTH_TEXTURE) ? MIN(zFar, fMaxDist) : fMaxDist;
	setupInfo.m_flRenderDistSq *= setupInfo.m_flRenderDistSq;

	
	SetupRenderablesListJob *pJob = new SetupRenderablesListJob( setupInfo );
	g_viewBuilder.QueueBuildRenderablesListJob( pJob );
	pJob->Release();
}

#if defined(_PS3)
void CRendering3dView::SetupRenderablesList_PS3_Epilogue( void )
{
	ConVarRef r_PS3_SPU_buildrenderables( "r_PS3_SPU_buildrenderables" );

	if( r_PS3_SPU_buildrenderables.GetInt() )
	{
		ClientLeafSystem()->BuildRenderablesList_PS3_Epilogue();
	}
}
#endif


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
// Kinda awkward...three optional parameters at the end...
void CRendering3dView::BuildWorldRenderLists( bool bDrawEntities, int iForceViewLeaf /* = -1 */, 
	bool bUseCacheIfEnabled /* = true */, bool bShadowDepth /* = false */, float *pReflectionWaterHeight /*= NULL*/ )
{
	VPROF_BUDGET( "BuildWorldRenderLists", "World Render Setup" );

    // @MULTICORE (toml 8/18/2006): to address....
	extern void UpdateClientRenderableInPVSStatus();
	UpdateClientRenderableInPVSStatus();

#if defined(_PS3)
	int buildViewID = g_viewBuilder.GetBuildViewID();
	Assert( !m_pWorldRenderList && !m_pWorldListInfo[ buildViewID ]);

	g_viewBuilder.SetDrawFlags( m_DrawFlags );
#else

	Assert( !m_pWorldRenderList && !m_pWorldListInfo);

#endif

	m_pMainView->IncWorldListsNumber();
	// Override vis data if specified this render, otherwise use default behavior with NULL
	VisOverrideData_t* pVisData = ( m_pCustomVisibility && m_pCustomVisibility->m_VisData.m_fDistToAreaPortalTolerance != FLT_MAX ) ?  &m_pCustomVisibility->m_VisData : NULL;
	bool bUseCache = ( bUseCacheIfEnabled && r_worldlistcache.GetBool() );
	if ( m_pCustomVisibility )
	{
		iForceViewLeaf = m_pCustomVisibility->m_iForceViewLeaf;
	}

	ClientWorldListInfo_t **ppWorldListInfo;

#if defined(_PS3)
	ppWorldListInfo = &m_pWorldListInfo[ buildViewID ];
#else
	ppWorldListInfo = &m_pWorldListInfo;
#endif


	if ( !bUseCache || !g_WorldListCache.Find( *this, pVisData, iForceViewLeaf, &m_pWorldRenderList, ppWorldListInfo ) )
	{

#if defined(_PS3)
		m_pWorldRenderList = render->CreateWorldList_PS3( buildViewID );
		
		m_pWorldListInfo[ buildViewID ] = &g_WorldListInfoPool[ buildViewID ]; 
		m_pWorldListInfo[ buildViewID ]->Init();

		//g_viewBuilder.m_pWorldRenderListCache[ buildViewID ] = m_pWorldRenderList;
		g_viewBuilder.SetWorldRenderListElement( m_pWorldRenderList );

		render->BuildWorldLists( m_pWorldRenderList, m_pWorldListInfo[ buildViewID ], 
			iForceViewLeaf, pVisData, bShadowDepth, pReflectionWaterHeight );

		if ( bUseCache )
		{
			g_WorldListCache.Add( *this, pVisData, iForceViewLeaf, m_pWorldRenderList, m_pWorldListInfo[ buildViewID ] );
		}

#else
		// @MULTICORE (toml 8/18/2006): when make parallel, will have to change caching to be atomic, where follow ons receive a pointer to a list that is not yet built
		m_pWorldRenderList = render->CreateWorldList();

		m_pWorldListInfo   = new ClientWorldListInfo_t;

		g_viewBuilder.SetWorldRenderListElement( m_pWorldRenderList );
		g_viewBuilder.SetWorldListInfoElement( m_pWorldListInfo );

		render->BuildWorldLists( m_pWorldRenderList, m_pWorldListInfo, 
			iForceViewLeaf, pVisData, bShadowDepth, pReflectionWaterHeight );

		if ( bUseCache )
		{
			g_WorldListCache.Add( *this, pVisData, iForceViewLeaf, m_pWorldRenderList, m_pWorldListInfo );
		}
#endif

	}
	else
	{
		// Lists found in the cache, still need to set up the concurrent view builder
		// (Investigate: Maybe we should cache CConcurrentViewData (as it contains all the info needed for the view) instead of 
		// caching the individual list)
		g_viewBuilder.SetWorldRenderListElement( m_pWorldRenderList );
		g_viewBuilder.SetWorldListInfoElement( m_pWorldListInfo );
		g_viewBuilder.CacheFrustumData();
	}
}

#if defined(_PS3)
void CRendering3dView::BuildWorldRenderLists_PS3_Epilogue( bool bShadowDepth ) 
{
	if( !m_pWorldRenderList )
		return;

	ConVarRef r_PS3_SPU_buildworldlists( "r_PS3_SPU_buildworldlists" );

	if( r_PS3_SPU_buildworldlists.GetInt() )
	{
		render->BuildWorldLists_PS3_Epilogue( m_pWorldRenderList, m_pWorldListInfo[ g_viewBuilder.GetBuildViewID() ], bShadowDepth );
	}
}
#else
void CRendering3dView::BuildWorldRenderLists_Epilogue( bool bShadowDepth ) 
{
	if( !m_pWorldRenderList )
		return;

	render->BuildWorldLists_Epilogue( m_pWorldRenderList, m_pWorldListInfo, bShadowDepth );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Computes the actual world list info based on the render flags
//-----------------------------------------------------------------------------

class PruneWorldListInfoJob : public CJob
{
public:

	PruneWorldListInfoJob( int buildViewID, int drawFlags ) : m_nBuildViewID( buildViewID ), m_DrawFlags( drawFlags ) {}

private:

	virtual JobStatus_t	DoExecute();

	int		m_nBuildViewID;
	int		m_DrawFlags;
};

JobStatus_t	PruneWorldListInfoJob::DoExecute()
{
	ClientWorldListInfo_t *pWorldListInfo = g_viewBuilder.GetClientWorldListInfoElement( m_nBuildViewID );

	int nWaterDrawFlags = m_DrawFlags & (DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER);
	if ( nWaterDrawFlags == DF_RENDER_ABOVEWATER && !pWorldListInfo->m_bHasWater )
	{
		return JOB_OK;;
	}

	ClientWorldListInfo_t *pNewInfo = NULL;
	if ( pWorldListInfo->m_LeafCount > 0 && nWaterDrawFlags )
	{
		pNewInfo = ClientWorldListInfo_t::AllocPooled( *pWorldListInfo );

		pNewInfo->m_ViewFogVolume = pWorldListInfo->m_ViewFogVolume;
		pNewInfo->m_bHasWater = pWorldListInfo->m_bHasWater;
		pNewInfo->m_LeafCount = 0;

		if ( nWaterDrawFlags != DF_RENDER_UNDERWATER || pWorldListInfo->m_bHasWater )
		{
			// Not drawing anything? Then don't bother with renderable lists
			if ( nWaterDrawFlags != 0 )
			{
				// Create a sub-list based on the actual leaves being rendered
				bool bRenderingUnderwater = (nWaterDrawFlags & DF_RENDER_UNDERWATER) != 0;
				for ( int i = 0; i < pWorldListInfo->m_LeafCount; ++i )
				{
					bool bLeafIsUnderwater = ( pWorldListInfo->m_pLeafDataList[i].waterData != -1 );
					if ( bRenderingUnderwater == bLeafIsUnderwater )
					{
						pNewInfo->m_pLeafDataList[ pNewInfo->m_LeafCount ] = pWorldListInfo->m_pLeafDataList[ i ];
						pNewInfo->m_pOriginalLeafIndex[ pNewInfo->m_LeafCount ] = i;
						++pNewInfo->m_LeafCount;
					}
				}
			}
		}

		g_viewBuilder.SetWorldListInfoElement( pNewInfo, m_nBuildViewID );
		SafeRelease( pNewInfo );
	}
	else
	{
		//pNewInfo = new ClientWorldListInfo_t;
		pWorldListInfo->m_LeafCount = 0;
	}

	return JOB_OK;
}

void CRendering3dView::PruneWorldListInfo()
{
#if !defined(_PS3)

	// Drawing everything? Just return the world list info as-is 
	int nWaterDrawFlags = m_DrawFlags & (DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER);
	if ( nWaterDrawFlags == (DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER) )
	{
		return;
	}

	PruneWorldListInfoJob *pJob = new PruneWorldListInfoJob( g_viewBuilder.GetBuildViewID(), m_DrawFlags );
	g_viewBuilder.QueueBuildWorldListJob( pJob );
	pJob->Release();

	SafeRelease( m_pWorldListInfo );

#else

	// TODO: Port To SPU or add to epilogue pass !! 

	ConVarRef r_PS3_SPU_buildworldlists("r_PS3_SPU_buildworldlists");

	if( !( r_PS3_SPU_buildworldlists.GetInt() && g_viewBuilder.IsSPUBuildRWJobsOn() ) )
	{
		// Drawing everything? Just return the world list info as-is 
		int nWaterDrawFlags = m_DrawFlags & (DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER);
		if ( nWaterDrawFlags == (DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER) )
		{
			return;
		}

		ClientWorldListInfo_t *pWorldListInfo = m_pWorldListInfo[ g_viewBuilder.GetBuildViewID() ];

		if ( nWaterDrawFlags == DF_RENDER_ABOVEWATER && !pWorldListInfo->m_bHasWater )
			return;

		// in-place copy on PS3
		ClientWorldListInfo_t *pNewInfo = pWorldListInfo;

		int worldListInfo_LeafCount = pWorldListInfo->m_LeafCount;

		//pNewInfo->m_LeafCount     = 0;
		int newLeafCount = 0;

		if ( pWorldListInfo->m_LeafCount > 0 && nWaterDrawFlags )
 		{
		//	pNewInfo->m_pOriginalLeafIndex = (uint16*)( (byte *)( pNewInfo->m_pLeafDataList ) + pNewInfo->m_LeafCount * sizeof(pNewInfo->m_pLeafDataList[0]) );
 		}
 		else
 		{
// 			// reset
//			pNewInfo->m_pLeafDataList = NULL;
 		}


		if ( nWaterDrawFlags != DF_RENDER_UNDERWATER || pWorldListInfo->m_bHasWater )
		{
			// Not drawing anything? Then don't bother with renderable lists
			if ( nWaterDrawFlags != 0 )
			{
				// Create a sub-list based on the actual leaves being rendered
				bool bRenderingUnderwater = (nWaterDrawFlags & DF_RENDER_UNDERWATER) != 0;

				for ( int i = 0; i < worldListInfo_LeafCount; ++i )
				{
					bool bLeafIsUnderwater = ( pWorldListInfo->m_pLeafDataList[i].waterData != -1 );
					if ( bRenderingUnderwater == bLeafIsUnderwater )
					{
						pNewInfo->m_pLeafDataList[ newLeafCount ] = pWorldListInfo->m_pLeafDataList[ i ];
						//pNewInfo->m_pOriginalLeafIndex[ newLeafCount ] = i;
						++newLeafCount;
					}
				}
			}
		}

		pNewInfo->m_LeafCount = newLeafCount;

		//	m_pWorldListInfo->Release();
		//	m_pWorldListInfo = pNewInfo;

	}

#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static inline void UpdateBrushModelLightmap( IClientRenderable *pEnt )
{
	model_t *pModel = ( model_t * )pEnt->GetModel();
	render->UpdateBrushModelLightmap( pModel, pEnt );
}

void CRendering3dView::BuildRenderableRenderLists( int viewID, bool bFastEntityRendering, bool bDrawDepthViewNonCachedObjectsOnly )
{
	MDLCACHE_CRITICAL_SECTION();

	SetupRenderablesList( viewID, bFastEntityRendering, bDrawDepthViewNonCachedObjectsOnly );

	
}

#if defined(_PS3)
void CRendering3dView::BuildRenderableRenderLists_PS3_Epilogue( void )
{
	SetupRenderablesList_PS3_Epilogue();
}
#else
void CRendering3dView::BuildRenderableRenderLists_Epilogue( int viewID )
{
	if ( viewID != VIEW_SHADOW_DEPTH_TEXTURE )
	{
		MDLCACHE_CRITICAL_SECTION();

#if defined(_PS3)
		CClientRenderablesList *pRenderablesList = m_pRenderablesList[ g_viewBuilder.GetBuildViewID() ];
#else
		CClientRenderablesList *pRenderablesList = m_pRenderables;
#endif

		render->BeginUpdateLightmaps();

		// update lightmap on brush models if necessary
		for ( int i = 0; i < RENDER_GROUP_COUNT; ++i )
		{
			CClientRenderablesList::CEntry *pEntities = pRenderablesList->m_RenderGroups[i];
			int nCount = pRenderablesList->m_RenderGroupCounts[i];
			for( int j=0; j < nCount; ++j )
			{
				if ( pEntities[j].m_nModelType != RENDERABLE_MODEL_BRUSH )
					continue;
				// For two-pass, do the work in the opaque pass.
				if ( pEntities[j].m_TwoPass && ( i != RENDER_GROUP_OPAQUE) )
					continue;
				UpdateBrushModelLightmap( pEntities[j].m_pRenderable );
			}
		}

		render->EndUpdateLightmaps();
	}

	for ( int i = 0; i < RENDER_GROUP_COUNT; ++i )
	{
		CClientRenderablesList *pRenderablesList = m_pRenderables;
		int nCount = pRenderablesList->m_RenderGroupCounts[i];

		VPROF_INCREMENT_COUNTER( "NumRenderables", nCount );
	}
}
#endif

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CRendering3dView::DrawWorld( IMatRenderContext *pRenderContext, float waterZAdjust )
{
	VPROF_INCREMENT_COUNTER( "RenderWorld", 1 );
	VPROF_BUDGET( "DrawWorld", VPROF_BUDGETGROUP_WORLD_RENDERING );

	if( !r_drawopaqueworld.GetBool() )
	{
		return;
	}

	unsigned long engineFlags = BuildEngineDrawWorldListFlags( m_DrawFlags );

	if ( ( m_bCSMView ) && ( g_CascadeLightManager.GetCSMQualityMode() == CSMQUALITY_VERY_LOW ) )
	{
		engineFlags |= DRAWWORLDLISTS_DRAW_SKIP_DISPLACEMENTS;
	}

	render->DrawWorldLists( pRenderContext, m_pWorldRenderList, engineFlags, waterZAdjust );
}

//-----------------------------------------------------------------------------
// Sets up automatic z-prepass on the 360. No-op on PC.
//-----------------------------------------------------------------------------
void CRendering3dView::BeginConsoleZPass()
{
#if defined( _GAMECONSOLE )
	{
#if defined( PORTAL )
		if( g_pPortalRender->GetViewRecursionLevel() != 0 )
			return;
#endif
		// set up command buffer-based fast z rejection for 360
		if ( r_fastzreject.GetBool() && !( m_DrawFlags & DF_SHADOW_DEPTH_MAP ) )
		{
			CMatRenderContextPtr pRenderContext( materials );
			WorldListIndicesInfo_t indicesInfo; 
			render->GetWorldListIndicesInfo( &indicesInfo, m_pWorldRenderList, BuildEngineDrawWorldListFlags( m_DrawFlags ) );
			pRenderContext->BeginConsoleZPass( indicesInfo );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Finishes automatic z-prepass on the 360. Will kick of Z render and color
// render passes. No-op on PC.
//-----------------------------------------------------------------------------
void CRendering3dView::EndConsoleZPass()
{
#if defined( _GAMECONSOLE )
	{
#if defined( PORTAL )
		if( g_pPortalRender->GetViewRecursionLevel() != 0 )
			return;
#endif

		if ( r_fastzreject.GetBool() && !( m_DrawFlags & DF_SHADOW_DEPTH_MAP ) )
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->EndConsoleZPass();
		}
	}
#endif
}


CMaterialReference g_material_WriteZ; //init'ed on by CViewRender::Init()

//-----------------------------------------------------------------------------
// Fakes per-entity clip planes on cards that don't support user clip planes.
//  Achieves the effect by drawing an invisible box that writes to the depth buffer
//  around the clipped area. It's not perfect, but better than nothing.
//-----------------------------------------------------------------------------
static void DrawClippedDepthBox( IClientRenderable *pEnt, float *pClipPlane )
{
//#define DEBUG_DRAWCLIPPEDDEPTHBOX //uncomment to draw the depth box as a colorful box

	static const int iQuads[6][5] = {	{ 0, 4, 6, 2, 0 }, //always an extra copy of first index at end to make some algorithms simpler
										{ 3, 7, 5, 1, 3 },
										{ 1, 5, 4, 0, 1 },
										{ 2, 6, 7, 3, 2 },
										{ 0, 2, 3, 1, 0 },
										{ 5, 7, 6, 4, 5 } };

	static const int iLines[12][2] = {	{ 0, 1 },
										{ 0, 2 },
										{ 0, 4 },
										{ 1, 3 },
										{ 1, 5 },
										{ 2, 3 },
										{ 2, 6 },
										{ 3, 7 },
										{ 4, 6 },
										{ 4, 5 },
										{ 5, 7 },
										{ 6, 7 } };


#ifdef DEBUG_DRAWCLIPPEDDEPTHBOX
	static const float fColors[6][3] = {	{ 1.0f, 0.0f, 0.0f },
											{ 0.0f, 1.0f, 1.0f },
											{ 0.0f, 1.0f, 0.0f },
											{ 1.0f, 0.0f, 1.0f },
											{ 0.0f, 0.0f, 1.0f },
											{ 1.0f, 1.0f, 0.0f } };
#endif

	
	

	Vector vNormal = *(Vector *)pClipPlane;
	float fPlaneDist = pClipPlane[3];

	Vector vMins, vMaxs;
	pEnt->GetRenderBounds( vMins, vMaxs );

	Vector vOrigin = pEnt->GetRenderOrigin();
	QAngle qAngles = pEnt->GetRenderAngles();
	
	Vector vForward, vUp, vRight;
	AngleVectors( qAngles, &vForward, &vRight, &vUp );

	Vector vPoints[8];
	vPoints[0] = vOrigin + (vForward * vMins.x) + (vRight * vMins.y) + (vUp * vMins.z);
	vPoints[1] = vOrigin + (vForward * vMaxs.x) + (vRight * vMins.y) + (vUp * vMins.z);
	vPoints[2] = vOrigin + (vForward * vMins.x) + (vRight * vMaxs.y) + (vUp * vMins.z);
	vPoints[3] = vOrigin + (vForward * vMaxs.x) + (vRight * vMaxs.y) + (vUp * vMins.z);
	vPoints[4] = vOrigin + (vForward * vMins.x) + (vRight * vMins.y) + (vUp * vMaxs.z);
	vPoints[5] = vOrigin + (vForward * vMaxs.x) + (vRight * vMins.y) + (vUp * vMaxs.z);
	vPoints[6] = vOrigin + (vForward * vMins.x) + (vRight * vMaxs.y) + (vUp * vMaxs.z);
	vPoints[7] = vOrigin + (vForward * vMaxs.x) + (vRight * vMaxs.y) + (vUp * vMaxs.z);

	int iClipped[8];
	float fDists[8];
	for( int i = 0; i != 8; ++i )
	{
		fDists[i] = vPoints[i].Dot( vNormal ) - fPlaneDist;
		iClipped[i] = (fDists[i] > 0.0f) ? 1 : 0;
	}

	Vector vSplitPoints[8][8]; //obviously there are only 12 lines, not 64 lines or 64 split points, but the indexing is way easier like this
	int iLineStates[8][8]; //0 = unclipped, 2 = wholly clipped, 3 = first point clipped, 4 = second point clipped

	//categorize lines and generate split points where needed
	for( int i = 0; i != 12; ++i )
	{
		const int *pPoints = iLines[i];
		int iLineState = (iClipped[pPoints[0]] + iClipped[pPoints[1]]);
		if( iLineState != 1 ) //either both points are clipped, or neither are clipped
		{
			iLineStates[pPoints[0]][pPoints[1]] = 
				iLineStates[pPoints[1]][pPoints[0]] = 
					iLineState;
		}
		else
		{
			//one point is clipped, the other is not
			if( iClipped[pPoints[0]] == 1 )
			{
				//first point was clipped, index 1 has the negative distance
				float fInvTotalDist = 1.0f / (fDists[pPoints[0]] - fDists[pPoints[1]]);
				vSplitPoints[pPoints[0]][pPoints[1]] = 
					vSplitPoints[pPoints[1]][pPoints[0]] =
						(vPoints[pPoints[1]] * (fDists[pPoints[0]] * fInvTotalDist)) - (vPoints[pPoints[0]] * (fDists[pPoints[1]] * fInvTotalDist));
				
				Assert( fabs( vNormal.Dot( vSplitPoints[pPoints[0]][pPoints[1]] ) - fPlaneDist ) < 0.01f );

				iLineStates[pPoints[0]][pPoints[1]] = 3;
				iLineStates[pPoints[1]][pPoints[0]] = 4;
			}
			else
			{
				//second point was clipped, index 0 has the negative distance
				float fInvTotalDist = 1.0f / (fDists[pPoints[1]] - fDists[pPoints[0]]);
				vSplitPoints[pPoints[0]][pPoints[1]] = 
					vSplitPoints[pPoints[1]][pPoints[0]] =
						(vPoints[pPoints[0]] * (fDists[pPoints[1]] * fInvTotalDist)) - (vPoints[pPoints[1]] * (fDists[pPoints[0]] * fInvTotalDist));

				Assert( fabs( vNormal.Dot( vSplitPoints[pPoints[0]][pPoints[1]] ) - fPlaneDist ) < 0.01f );

				iLineStates[pPoints[0]][pPoints[1]] = 4;
				iLineStates[pPoints[1]][pPoints[0]] = 3;
			}
		}
	}


	CMatRenderContextPtr pRenderContext( materials );
	
#ifdef DEBUG_DRAWCLIPPEDDEPTHBOX
	pRenderContext->Bind( materials->FindMaterial( "debug/debugvertexcolor", TEXTURE_GROUP_OTHER ), NULL );
#else
	pRenderContext->Bind( g_material_WriteZ, NULL );
#endif

	CMeshBuilder meshBuilder;
	IMesh* pMesh = pRenderContext->GetDynamicMesh( false );
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 18 ); //6 sides, possible one cut per side. Any side is capable of having 3 tri's. Lots of padding for things that aren't possible

	//going to draw as a collection of triangles, arranged as a triangle fan on each side
	for( int i = 0; i != 6; ++i )
	{
		const int *pPoints = iQuads[i];

		//can't start the fan on a wholly clipped line, so seek to one that isn't
		int j = 0;
		do
		{
			if( iLineStates[pPoints[j]][pPoints[j+1]] != 2 ) //at least part of this line will be drawn
				break;

			++j;
		} while( j != 3 );

		if( j == 3 ) //not enough lines to even form a triangle
			continue;

		float *pStartPoint = NULL;
		float *pTriangleFanPoints[4]; //at most, one of our fans will have 5 points total, with the first point being stored separately as pStartPoint
		int iTriangleFanPointCount = 1; //the switch below creates the first for sure
		
		//figure out how to start the fan
		switch( iLineStates[pPoints[j]][pPoints[j+1]] )
		{
		case 0: //uncut
			pStartPoint = &vPoints[pPoints[j]].x;
			pTriangleFanPoints[0] = &vPoints[pPoints[j+1]].x;
			break;

		case 4: //second index was clipped
			pStartPoint = &vPoints[pPoints[j]].x;
			pTriangleFanPoints[0] = &vSplitPoints[pPoints[j]][pPoints[j+1]].x;
			break;

		case 3: //first index was clipped
			pStartPoint = &vSplitPoints[pPoints[j]][pPoints[j+1]].x;
			pTriangleFanPoints[0] = &vPoints[pPoints[j + 1]].x;
			break;

		default:
			Assert( false );
			break;
		};

		for( ++j; j != 3; ++j ) //add end points for the rest of the indices, we're assembling a triangle fan
		{
			switch( iLineStates[pPoints[j]][pPoints[j+1]] )
			{
			case 0: //uncut line, normal endpoint
				pTriangleFanPoints[iTriangleFanPointCount] = &vPoints[pPoints[j+1]].x;
				++iTriangleFanPointCount;
				break;

			case 2: //wholly cut line, no endpoint
				break;

			case 3: //first point is clipped, normal endpoint
				//special case, adds start and end point
				pTriangleFanPoints[iTriangleFanPointCount] = &vSplitPoints[pPoints[j]][pPoints[j+1]].x;
				++iTriangleFanPointCount;

				pTriangleFanPoints[iTriangleFanPointCount] = &vPoints[pPoints[j+1]].x;
				++iTriangleFanPointCount;
				break;

			case 4: //second point is clipped
				pTriangleFanPoints[iTriangleFanPointCount] = &vSplitPoints[pPoints[j]][pPoints[j+1]].x;
				++iTriangleFanPointCount;
				break;

			default:
				Assert( false );
				break;
			};
		}
		
		//special case endpoints, half-clipped lines have a connecting line between them and the next line (first line in this case)
		switch( iLineStates[pPoints[j]][pPoints[j+1]] )
		{
		case 3:
		case 4:
			pTriangleFanPoints[iTriangleFanPointCount] = &vSplitPoints[pPoints[j]][pPoints[j+1]].x;
			++iTriangleFanPointCount;
			break;
		};

		Assert( iTriangleFanPointCount <= 4 );

		//add the fan to the mesh
		int iLoopStop = iTriangleFanPointCount - 1;
		for( int k = 0; k != iLoopStop; ++k )
		{
			meshBuilder.Position3fv( pStartPoint );
#ifdef DEBUG_DRAWCLIPPEDDEPTHBOX
			float fHalfColors[3] = { fColors[i][0] * 0.5f, fColors[i][1] * 0.5f, fColors[i][2] * 0.5f };
			meshBuilder.Color3fv( fHalfColors );
#endif
			meshBuilder.AdvanceVertex();
			
			meshBuilder.Position3fv( pTriangleFanPoints[k] );
#ifdef DEBUG_DRAWCLIPPEDDEPTHBOX
			meshBuilder.Color3fv( fColors[i] );
#endif
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv( pTriangleFanPoints[k+1] );
#ifdef DEBUG_DRAWCLIPPEDDEPTHBOX
			meshBuilder.Color3fv( fColors[i] );
#endif
			meshBuilder.AdvanceVertex();
		}
	}

	meshBuilder.End();
	pMesh->Draw();
	pRenderContext->Flush( false );
}


//-----------------------------------------------------------------------------
// Unified bit of draw code for opaque and translucent renderables
//-----------------------------------------------------------------------------
static inline void DrawRenderable( IClientRenderable *pEnt, int flags, const RenderableInstance_t &instance, bool bShadowDepth )
{
	float *pRenderClipPlane = NULL;
	if( r_entityclips.GetBool() )
		pRenderClipPlane = pEnt->GetRenderClipPlane();

	if( pRenderClipPlane )	
	{
		CMatRenderContextPtr pRenderContext( materials );
		if( !materials->UsingFastClipping() ) //do NOT change the fast clip plane mid-scene, depth problems result. Regular user clip planes are fine though
			pRenderContext->PushCustomClipPlane( pRenderClipPlane );
		else if ( !bShadowDepth ) // in shadow-depth pass, this step is unnecessary
			DrawClippedDepthBox( pEnt, pRenderClipPlane );
		Assert( view->GetCurrentlyDrawingEntity() == NULL );
		view->SetCurrentlyDrawingEntity( pEnt->GetIClientUnknown()->GetBaseEntity() );
		pEnt->DrawModel( flags, instance );
		view->SetCurrentlyDrawingEntity( NULL );

		if( !materials->UsingFastClipping() )	
			pRenderContext->PopCustomClipPlane();
	}
	else
	{
		Assert( view->GetCurrentlyDrawingEntity() == NULL );
		view->SetCurrentlyDrawingEntity( pEnt->GetIClientUnknown()->GetBaseEntity() );
		if( bShadowDepth )
			flags |= DF_SHADOW_DEPTH_MAP;
		pEnt->DrawModel( flags, instance );
		view->SetCurrentlyDrawingEntity( NULL );
	}
}

//-----------------------------------------------------------------------------
// Draws all opaque renderables in leaves that were rendered
//-----------------------------------------------------------------------------
static inline void DrawOpaqueRenderable( IClientRenderable *pEnt, bool bTwoPass, ERenderDepthMode_t DepthMode )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();

#ifdef PORTAL
	Assert( ( g_pPortalRender->GetViewRecursionLevel() > 0 ) || !IsSplitScreenSupported() || pEnt->ShouldDrawForSplitScreenUser( GET_ACTIVE_SPLITSCREEN_SLOT() ) );
#else
	Assert( !IsSplitScreenSupported() || pEnt->ShouldDrawForSplitScreenUser( GET_ACTIVE_SPLITSCREEN_SLOT() ) );
#endif
	Assert( (pEnt->GetIClientUnknown() == NULL) || (pEnt->GetIClientUnknown()->GetIClientEntity() == NULL) || (pEnt->GetIClientUnknown()->GetIClientEntity()->IsBlurred() == false) );

	int flags = STUDIO_RENDER;
	if ( bTwoPass )
	{
		flags |= STUDIO_TWOPASS;
	}

	if ( DepthMode == DEPTH_MODE_SHADOW )
	{
		flags |= STUDIO_SHADOWDEPTHTEXTURE;
	}
	else if ( DepthMode == DEPTH_MODE_SSA0 )
	{
		flags |= STUDIO_SSAODEPTHTEXTURE;
	}
	else
	{
		float color[3];
		pEnt->GetColorModulation( color );
		render->SetColorModulation(	color );
	}

	RenderableInstance_t instance;
	instance.m_nAlpha = 255;
	DrawRenderable( pEnt, flags, instance, DepthMode == DEPTH_MODE_SHADOW );
}

//-------------------------------------


static void SetupBonesOnBaseAnimating( C_BaseAnimating *&pBaseAnimating )
{
	pBaseAnimating->SetupBones( NULL, -1, -1, gpGlobals->curtime );
}


static ConVar cl_brushfastpath( "cl_brushfastpath", "1", FCVAR_CHEAT );
static ConVar r_drawbrushmodels( "r_drawbrushmodels", "1", FCVAR_CHEAT, "Render brush models. 0=Off, 1=Normal, 2=Wireframe" );
static void DrawOpaqueRenderables_DrawBrushModels( CClientRenderablesList::CEntry **pBrushEntities, int nBrushEntityCount, ERenderDepthMode_t DepthMode, CUtlVector< CClientRenderablesList::CEntry * > *pDeferClippedOpaqueRenderables_Out )
{
	// Skip if we're not rendering brush model entities
	if ( !r_drawbrushmodels.GetBool() )
		return;

	// See about rendering brushes in the "fast path"
	int nRemainingBrushes = nBrushEntityCount;
	if ( cl_brushfastpath.GetBool() )
	{
		nRemainingBrushes = 0;
		CUtlVector< ModelRenderSystemData_t > arrBrushRenderables( (ModelRenderSystemData_t *)stackalloc( nBrushEntityCount * sizeof( ModelRenderSystemData_t ) ), nBrushEntityCount );
		for ( int i = 0; i < nBrushEntityCount; ++i )
		{
			CClientRenderablesList::CEntry *itEntity = pBrushEntities[i];
			Assert( itEntity->m_pRenderable );
			if ( !itEntity->m_pRenderable )
				continue;

			IClientModelRenderable *pModelRenderable = itEntity->m_pRenderable->GetClientModelRenderable();
			if ( !pModelRenderable )
			{
				pBrushEntities[nRemainingBrushes++] = itEntity;
				continue;
			}

			ModelRenderSystemData_t data;
			data.m_pRenderable = itEntity->m_pRenderable;
			data.m_pModelRenderable = pModelRenderable;
			data.m_InstanceData = itEntity->m_InstanceData;
			arrBrushRenderables.AddToTail( data );
		}

		ModelRenderMode_t nRenderMode = MODEL_RENDER_MODE_NORMAL;

		switch ( DepthMode )
		{
		case DEPTH_MODE_SHADOW:
			nRenderMode = MODEL_RENDER_MODE_SHADOW_DEPTH;
			break;
		case DEPTH_MODE_SSA0:
			nRenderMode = MODEL_RENDER_MODE_SSAO;
			break;
		default:
			nRenderMode = MODEL_RENDER_MODE_NORMAL;
		}


		g_pModelRenderSystem->DrawBrushModels( arrBrushRenderables.Base(), arrBrushRenderables.Count(), nRenderMode );
	}

	if( pDeferClippedOpaqueRenderables_Out )
	{
		for( int i = 0; i < nRemainingBrushes; ++i )
		{
			CClientRenderablesList::CEntry* pEntity = pBrushEntities[i];
			if( pEntity->m_pRenderable && (pEntity->m_pRenderable->GetRenderClipPlane() != NULL) )
			{
				pDeferClippedOpaqueRenderables_Out->AddToTail( pEntity );
			}
			else
			{
				DrawOpaqueRenderable( pEntity->m_pRenderable, pEntity->m_TwoPass, DepthMode );
			}
		}
	}
	else
	{
		for( int i = 0; i < nRemainingBrushes; ++i )
		{
			CClientRenderablesList::CEntry* pEntity = pBrushEntities[i];
			DrawOpaqueRenderable( pEntity->m_pRenderable, pEntity->m_TwoPass, DepthMode );
		}
	}
}

static void DrawOpaqueRenderables_DrawStaticProps( int nCount, CClientRenderablesList::CEntry **ppEntities, ERenderDepthMode_t DepthMode )
{
	if ( nCount == 0 )
		return;

	float one[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	render->SetColorModulation(	one );
	render->SetBlend( 1.0f );
	
	const int MAX_STATICS_PER_BATCH = 512;
	IClientRenderable *pStatics[ MAX_STATICS_PER_BATCH ];
	RenderableInstance_t pInstances[ MAX_STATICS_PER_BATCH ];
	
	int numScheduled = 0, numAvailable = MAX_STATICS_PER_BATCH;

	for( int i = 0; i < nCount; ++i )
	{
		CClientRenderablesList::CEntry *itEntity = ppEntities[i];
		if ( !itEntity->m_pRenderable )
			continue;

		pInstances[ numScheduled ] = itEntity->m_InstanceData;
		pStatics[ numScheduled ++ ] = itEntity->m_pRenderable;
		if ( -- numAvailable > 0 )
			continue; // place a hint for compiler to predict more common case in the loop
		
		staticpropmgr->DrawStaticProps( pStatics, pInstances, numScheduled, DepthMode == DEPTH_MODE_SHADOW, vcollide_wireframe.GetBool() );
		numScheduled = 0;
		numAvailable = MAX_STATICS_PER_BATCH;
	}
	
	if ( numScheduled )
		staticpropmgr->DrawStaticProps( pStatics, pInstances, numScheduled, DepthMode == DEPTH_MODE_SHADOW, vcollide_wireframe.GetBool() );
}

static void DrawOpaqueRenderables_Range( int nCount, CClientRenderablesList::CEntry **ppEntities, ERenderDepthMode_t DepthMode, CUtlVector< CClientRenderablesList::CEntry * > *pDeferClippedOpaqueRenderables_Out )
{
	if( pDeferClippedOpaqueRenderables_Out )
	{
		for ( int i = 0; i < nCount; ++i )
		{
			CClientRenderablesList::CEntry *itEntity = ppEntities[i]; 
			if( itEntity->m_pRenderable && (itEntity->m_pRenderable->GetRenderClipPlane() != NULL) )
			{
				pDeferClippedOpaqueRenderables_Out->AddToTail( itEntity );
			}
			else
			{
				DrawOpaqueRenderable( itEntity->m_pRenderable, ( itEntity->m_TwoPass != 0 ), DepthMode );
			}
		}
	}
	else
	{
		for ( int i = 0; i < nCount; ++i )
		{
			CClientRenderablesList::CEntry *itEntity = ppEntities[i]; 
			if ( itEntity->m_pRenderable )
			{
				DrawOpaqueRenderable( itEntity->m_pRenderable, ( itEntity->m_TwoPass != 0 ), DepthMode );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Renders all translucent world, entities, and detail objects in a particular set of leaves
//-----------------------------------------------------------------------------
//$TODO(msmith) This "fast path" (cl_modelfastpath) doesn't currently work on css15.  Alpha does not render on enemies, only teammates.
//              Investingation should be made as to why it's broken before turning it back on by default.  And we should only look to turn
//				it on if when we do turn it on there is a performance win (on PS3 and Xbox360).
ConVar cl_modelfastpath( "cl_modelfastpath", "1" );
ConVar cl_skipslowpath( "cl_skipslowpath", "0", FCVAR_CHEAT, "Set to 1 to skip any models that don't go through the model fast path" );
extern ConVar r_drawothermodels;
static void	DrawOpaqueRenderables_ModelRenderables( int nCount, ModelRenderSystemData_t* pModelRenderables, ERenderDepthMode_t DepthMode, bool bShadowDepthIncludeTranslucentMaterials )
{
	ModelRenderMode_t nRenderMode = MODEL_RENDER_MODE_NORMAL;

	switch ( DepthMode )
	{
	case DEPTH_MODE_SHADOW:
		nRenderMode = MODEL_RENDER_MODE_SHADOW_DEPTH;
		break;
	case DEPTH_MODE_SSA0:
		nRenderMode = MODEL_RENDER_MODE_SSAO;
		break;
	default:
		nRenderMode = MODEL_RENDER_MODE_NORMAL;
	}

	g_pModelRenderSystem->DrawModels( pModelRenderables, nCount, nRenderMode, bShadowDepthIncludeTranslucentMaterials );
}

static void	DrawOpaqueRenderables_NPCs( int nCount, CClientRenderablesList::CEntry **ppEntities, ERenderDepthMode_t DepthMode, CUtlVector< CClientRenderablesList::CEntry * > *pDeferClippedOpaqueRenderables_Out )
{
	DrawOpaqueRenderables_Range( nCount, ppEntities, DepthMode, pDeferClippedOpaqueRenderables_Out );
}

void CRendering3dView::DrawOpaqueRenderables( IMatRenderContext *pRenderContext, RenderablesRenderPath_t eRenderPath, ERenderDepthMode_t DepthMode, CUtlVector< CClientRenderablesList::CEntry * > *pDeferClippedOpaqueRenderables_Out, RenderGroup_t nGroup )
{
	VPROF("CViewRender::DrawOpaqueRenderables" );
	bool bShadowDepth = ( eRenderPath > 0 );


#if defined( _X360 )
	// IESTYN -------- 11/5/2010 (June '09 XDK) -----------------------------------------
	//
	//	There is currently a codegen bug in the X360 compiler, for which the below CFmtStr initialization is a workaround.
	//	  The problem appears to be that the stackallocs below cause 'eRenderPath' (when pushed onto the stack) to be
	//	corrupted to some value which is less than zero. Replacing the stackallocs with regular mallocs, or adding any
	//	additional code (e.g. this CFmtStr) which references the 'bShadowDepth' variable will cause the bug to disappear.
	//	  The original symptom was a GPU HANG on starting a Co-op game; bShadowDepth was erroneously determined to be
	//	FALSE during the shadow depth pass, so models were rendered using their regular (rather than NULL) pixel shaders,
	//	at a time when the pixel shader GPR allocation is set to its minimum value of 16.
	//
	if ( eRenderPath == 123454321 ) { CFmtStr buf( "Hocus Pocus Alakazam: %d %d", eRenderPath, bShadowDepth ); }
	//
	// IESTYN -------- 11/5/2010 (June '09 XDK) -----------------------------------------
#endif // _X360

	if ( nGroup == RENDER_GROUP_TRANSLUCENT )
	{
		if( !r_drawtranslucentworld.GetBool() )
			return;
	}
	else
	{
		if( !r_drawopaquerenderables.GetBool() )
			return;
	}

	if( !m_pMainView->ShouldDrawEntities() )
		return;

	render->SetBlend( 1 );

	//
	// Prepare to iterate over all leaves that were visible, and draw opaque things in them.	
	//
	RopeManager()->ResetRenderCache();
	g_pParticleSystemMgr->ResetRenderCache();

	// Categorize models by type
#if defined(_PS3)
	int nOpaqueRenderableCount = m_pRenderablesList[ g_viewBuilder.GetBuildViewID() ]->m_RenderGroupCounts[nGroup];
#else
	int nOpaqueRenderableCount = m_pRenderables->m_RenderGroupCounts[nGroup];
#endif

	CClientRenderablesList::CEntry** pBrushModels = (CClientRenderablesList::CEntry **)stackalloc( nOpaqueRenderableCount * sizeof( CClientRenderablesList::CEntry* ) );
	CClientRenderablesList::CEntry** pStaticProps = (CClientRenderablesList::CEntry **)stackalloc( nOpaqueRenderableCount * sizeof( CClientRenderablesList::CEntry* ) );
	CClientRenderablesList::CEntry** pOtherRenderables = (CClientRenderablesList::CEntry **)stackalloc( nOpaqueRenderableCount * sizeof( CClientRenderablesList::CEntry* ) );

#if defined(_PS3)
	CClientRenderablesList::CEntry *pOpaqueList = m_pRenderablesList[ g_viewBuilder.GetBuildViewID() ]->m_RenderGroups[nGroup];
#else
	CClientRenderablesList::CEntry *pOpaqueList = m_pRenderables->m_RenderGroups[nGroup];
#endif

	int nBrushCount = 0;
	int nStaticCount = 0;
	int nOtherCount = 0;
#ifdef _PS3
//	extern uint32 g_ps3_ShadowDepth_TextureCache;
// 7LTODO
// 	for ( int iPs3depthGeoCacheLoopCounter = 0,
// 		iPs3depthGeoCacheLoopEnd = ( ( eRenderPath != RENDERABLES_RENDER_PATH_SHADOWDEPTH_BUILD_GEOCACHE ) ? 1 : 2 );
// 		iPs3depthGeoCacheLoopCounter < iPs3depthGeoCacheLoopEnd; ++ iPs3depthGeoCacheLoopCounter )
// 	{
// 		const bool bMultipassGeoCacheLoop = ( iPs3depthGeoCacheLoopEnd > 1 );
// 		if ( eRenderPath == RENDERABLES_RENDER_PATH_SHADOWDEPTH_USE_GEOCACHE )
// 		{
// 			pRenderContext->InvokeGpuDataTransferCache( g_ps3_ShadowDepth_TextureCache | PS3GPU_DATA_TRANSFER_CACHE2REAL ); // seed the rendertarget with the cached data
// 		}
// 		else if ( iPs3depthGeoCacheLoopCounter )
// 		{
// 			pRenderContext->InvokeGpuDataTransferCache( g_ps3_ShadowDepth_TextureCache | PS3GPU_DATA_TRANSFER_REAL2CACHE ); // copy off rendertarget data into cache
// 
// 			nBrushCount = 0;
// 			nStaticCount = 0;
// 			nOtherCount = 0;
// 		}
#endif

		for ( int i = 0; i < nOpaqueRenderableCount; ++i )
		{
#ifdef _PS3
			// Only render the correct cache part of opaques each loop iteration
// 7LTODO			if ( bMultipassGeoCacheLoop && ( !iPs3depthGeoCacheLoopCounter == !!pOpaqueList[i].m_bShadowDepthNoCache) )
//				continue;
#endif
			switch( pOpaqueList[i].m_nModelType )
			{
			case RENDERABLE_MODEL_BRUSH: pBrushModels[nBrushCount++] = &pOpaqueList[i]; break; 
			case RENDERABLE_MODEL_STATIC_PROP:	pStaticProps[nStaticCount++] = &pOpaqueList[i]; break; 
			default:							pOtherRenderables[nOtherCount++] = &pOpaqueList[i]; break; 
			}
		}

		//
		// First do the brush models
		//
		if ( nGroup != RENDER_GROUP_TRANSLUCENT )
		{
			DrawOpaqueRenderables_DrawBrushModels( pBrushModels, nBrushCount, DepthMode, pDeferClippedOpaqueRenderables_Out );
		}

		if( IsPS3() )
		{
			// PS3 vertex shader processors are underpowered relative to X360, so we only want to draw low-vertex high-fill pieces of the world, but not models that have
			// a lot of vertices and not many pixels. This is why we end Z pass here on PS/3 but end it later on Xbox360
			EndConsoleZPass();
		}


		// Move all static props to modelrendersystem
		bool bUseFastPath = ( cl_modelfastpath.GetInt() != 0 );

		//
		// Sort everything that's not a static prop
		//
		CUtlVector< CClientRenderablesList::CEntry* > arrRenderEntsNpcsFirst( (CClientRenderablesList::CEntry **)stackalloc( nOtherCount * sizeof( CClientRenderablesList::CEntry ) ), nOtherCount );
		CUtlVector< ModelRenderSystemData_t > arrModelRenderables( (ModelRenderSystemData_t *)stackalloc( ( nOtherCount + nStaticCount ) * sizeof( ModelRenderSystemData_t ) ), nOtherCount + nStaticCount );

		// Queue up RENDER_GROUP_OPAQUE_ENTITY entities to be rendered later.
		CClientRenderablesList::CEntry *itEntity;
		int nRemainingOther = 0;
		if( r_drawothermodels.GetBool() )
		{
			for ( int i = 0; i < nOtherCount; ++i )
			{
				itEntity = pOtherRenderables[i];
				if ( !itEntity->m_pRenderable )
					continue;

				IClientModelRenderable *pModelRenderable = itEntity->m_pRenderable->GetClientModelRenderable();

				// FIXME: Strangely, some static props are in the non-static prop bucket
				// which is what the last case in this if statement is for
				if ( bUseFastPath && pModelRenderable )
				{
					ModelRenderSystemData_t data;
					data.m_pRenderable = itEntity->m_pRenderable;
					data.m_pModelRenderable = pModelRenderable;
					data.m_InstanceData = itEntity->m_InstanceData;
					arrModelRenderables.AddToTail( data );
					continue;
				}

				IClientUnknown *pUnknown = itEntity->m_pRenderable->GetIClientUnknown();
				C_BaseEntity *pEntity = pUnknown->GetBaseEntity();
				
				if ( pEntity && pEntity->IsNPC() )
				{
					arrRenderEntsNpcsFirst.AddToTail( itEntity );
					continue;
				}
				pOtherRenderables[nRemainingOther++] = itEntity;
			}
		}

		// Queue up the static props to be rendered later.
		int nRemainingStatics = 0;
		if ( bUseFastPath )
		{
			const int LOOKAHEAD_PREFETCH = 7;
			int nPrefetchCount = MIN(nStaticCount, LOOKAHEAD_PREFETCH);
			for ( int i = 0; i < nPrefetchCount; i++ )
			{
				PREFETCH_128( pStaticProps[i]->m_pRenderable, 0 );
			}
			int nLastPrefetch = nStaticCount - LOOKAHEAD_PREFETCH;
			for ( int i = 0; i < nStaticCount; ++i )
			{
				if ( i < nLastPrefetch )
				{
					PREFETCH_128( pStaticProps[i+LOOKAHEAD_PREFETCH]->m_pRenderable, 0 );
				}
				itEntity = pStaticProps[i];
				if ( !itEntity->m_pRenderable )
					continue;

				IClientModelRenderable *pModelRenderable = itEntity->m_pRenderable->GetClientModelRenderable();
				if ( !pModelRenderable )
				{
					pStaticProps[nRemainingStatics++] = itEntity;
					continue;
				}

				ModelRenderSystemData_t data;
				data.m_pRenderable = itEntity->m_pRenderable;
				data.m_pModelRenderable = pModelRenderable;
				data.m_InstanceData = itEntity->m_InstanceData;
				arrModelRenderables.AddToTail( data );
			}
		}
		else
		{
			nRemainingStatics = nStaticCount;
		}

		//
		// Draw model renderables now (ie. models that use the fast path)
		//					 
		DrawOpaqueRenderables_ModelRenderables( arrModelRenderables.Count(), arrModelRenderables.Base(), DepthMode, nGroup == RENDER_GROUP_TRANSLUCENT );

		// Turn off z pass here. Don't want non-fastpath models with potentially large dynamic VB requirements overwrite
		// stuff in the dynamic VB ringbuffer. We're calling EndConsoleZPass again in DrawExecute, but that's not a problem.
		// BeginConsoleZPass/EndConsoleZPass don't have to be matched exactly. The first EndConsoleZPass ends Zpass, subsequent Ends don't have any effect
		if( !IsPS3() )
		{
			// we end console ZPass earlier in this same function for PS/3, no need to call it twice
			EndConsoleZPass();
		}

		//
		// Draw static props + opaque entities that aren't using the fast path.
		//
		DrawOpaqueRenderables_Range( nRemainingOther, pOtherRenderables, DepthMode, pDeferClippedOpaqueRenderables_Out );
		DrawOpaqueRenderables_DrawStaticProps( nRemainingStatics, pStaticProps, DepthMode );

		//
		// Draw NPCs now. 7LS - TODO - don't draw NPC's into viewmodel cascade on console
		//
		DrawOpaqueRenderables_NPCs( arrRenderEntsNpcsFirst.Count(), arrRenderEntsNpcsFirst.Base(), DepthMode, pDeferClippedOpaqueRenderables_Out );

#ifdef _PS3
// 7LTODO -- Took out for loop!	}
#endif

	//
	// Ropes and particles
	//

	bool bDrawRopes = true;

	if ( bShadowDepth )
	{
		if ( m_bCSMView && !( cl_csm_rope_shadows.GetBool() && cl_csm_shadows.GetBool() ) )
			bDrawRopes = false;
		
		
		if ( m_bCSMView && !IsGameConsole() && ( g_CascadeLightManager.GetCSMQualityMode() == CSMQUALITY_VERY_LOW ) )
			bDrawRopes = false;
	}

	if ( DepthMode == DEPTH_MODE_SSA0 )
	{
		bDrawRopes = false;
	}

	if ( bDrawRopes )
	{
		RopeManager()->DrawRenderCache( pRenderContext, bShadowDepth );
	}

	g_pParticleSystemMgr->DrawRenderCache( pRenderContext, bShadowDepth );

}



void CRendering3dView::DrawDeferredClippedOpaqueRenderables( IMatRenderContext *pRenderContext, RenderablesRenderPath_t eRenderPath, ERenderDepthMode_t DepthMode, CUtlVector< CClientRenderablesList::CEntry * > *pDeferClippedOpaqueRenderables )
{
	if( pDeferClippedOpaqueRenderables )
	{
		//bool bShadowDepth = ( eRenderPath > 0 );
		int nCount = pDeferClippedOpaqueRenderables->Count();
		CClientRenderablesList::CEntry **ppEntities = pDeferClippedOpaqueRenderables->Base();

		for ( int i = 0; i < nCount; ++i )
		{
			CClientRenderablesList::CEntry *itEntity = ppEntities[i]; 
			DrawOpaqueRenderable( itEntity->m_pRenderable, ( itEntity->m_TwoPass != 0 ), DepthMode );
		}
	}
}


//-----------------------------------------------------------------------------
// Renders all translucent world + detail objects in a particular set of leaves
//-----------------------------------------------------------------------------
void CRendering3dView::DrawTranslucentWorldInLeaves( IMatRenderContext *pRenderContext,  bool bShadowDepth )
{
	if ( bShadowDepth )
		return;

	VPROF_BUDGET( "CViewRender::DrawTranslucentWorldInLeaves", VPROF_BUDGETGROUP_WORLD_RENDERING );

#if defined(_PS3)
	const ClientWorldListInfo_t& info = *m_pWorldListInfo[ g_viewBuilder.GetBuildViewID() ];
#else
	const ClientWorldListInfo_t& info = *m_pWorldListInfo;
#endif

	CUtlVectorFixedGrowable<int, 32> transSortIndexList;
	for( int iCurLeafIndex = info.m_LeafCount - 1; iCurLeafIndex >= 0; iCurLeafIndex-- )
	{
		if ( info.m_pLeafDataList[iCurLeafIndex].translucentSurfaceCount )
		{
			int nActualLeafIndex = info.m_pOriginalLeafIndex ? info.m_pOriginalLeafIndex[ iCurLeafIndex ] : iCurLeafIndex;
			Assert( nActualLeafIndex != INVALID_LEAF_INDEX );
			transSortIndexList.AddToTail(nActualLeafIndex);
		}
	}
	if ( transSortIndexList.Count() )
	{
		// Now draw the surfaces in this leaf
		render->DrawTranslucentSurfaces( pRenderContext, m_pWorldRenderList, transSortIndexList.Base(), transSortIndexList.Count(), m_DrawFlags );
	}
}


//-----------------------------------------------------------------------------
// Renders all translucent world + detail objects in a particular set of leaves
//-----------------------------------------------------------------------------
void CRendering3dView::DrawTranslucentWorldAndDetailPropsInLeaves( IMatRenderContext *pRenderContext, int iCurLeafIndex, int iFinalLeafIndex, int nEngineDrawFlags, int &nDetailLeafCount, LeafIndex_t* pDetailLeafList, bool bShadowDepth )
{
	if ( bShadowDepth )
		return;

	CUtlVectorFixedGrowable<int, 32> transSortIndexList;
	VPROF_BUDGET( "CViewRender::DrawTranslucentWorldAndDetailPropsInLeaves", VPROF_BUDGETGROUP_WORLD_RENDERING );
#if defined(_PS3)
	const ClientWorldListInfo_t& info = *m_pWorldListInfo[ g_viewBuilder.GetBuildViewID() ];
	CClientRenderablesList *pRenderablesList = m_pRenderablesList[ g_viewBuilder.GetBuildViewID() ];
#else
	const ClientWorldListInfo_t& info = *m_pWorldListInfo;
	CClientRenderablesList *pRenderablesList = m_pRenderables;
#endif
	for( ; iCurLeafIndex >= iFinalLeafIndex; iCurLeafIndex-- )
	{
		if ( info.m_pLeafDataList[iCurLeafIndex].translucentSurfaceCount )
		{
			int nActualLeafIndex = info.m_pOriginalLeafIndex ? info.m_pOriginalLeafIndex[ iCurLeafIndex ] : iCurLeafIndex;
			Assert( nActualLeafIndex != INVALID_LEAF_INDEX );
			// First draw any queued-up detail props from previously visited leaves
			if ( nDetailLeafCount )
			{
				DetailObjectSystem()->RenderTranslucentDetailObjects( pRenderablesList->m_DetailFade, CurrentViewOrigin(), CurrentViewForward(), CurrentViewRight(), CurrentViewUp(), nDetailLeafCount, pDetailLeafList );
				nDetailLeafCount = 0;
			}
			transSortIndexList.AddToTail(nActualLeafIndex);
		}

		// Queue up detail props that existed in this leaf
		if ( ClientLeafSystem()->ShouldDrawDetailObjectsInLeaf( info.m_pLeafDataList[iCurLeafIndex].leafIndex, m_pMainView->BuildWorldListsNumber() ) )
		{
			if ( transSortIndexList.Count() )
			{
				// Now draw the surfaces in this leaf
				render->DrawTranslucentSurfaces( pRenderContext, m_pWorldRenderList, transSortIndexList.Base(), transSortIndexList.Count(), nEngineDrawFlags );
			}
			pDetailLeafList[nDetailLeafCount] = info.m_pLeafDataList[iCurLeafIndex].leafIndex;
			++nDetailLeafCount;
		}
	}
	if ( transSortIndexList.Count() )
	{
		// Now draw the surfaces in this leaf
		render->DrawTranslucentSurfaces( pRenderContext, m_pWorldRenderList, transSortIndexList.Base(), transSortIndexList.Count(), nEngineDrawFlags );
	}
}

//-----------------------------------------------------------------------------
// Renders all translucent entities in the render list
//-----------------------------------------------------------------------------
static inline void DrawTranslucentRenderable( IClientRenderable *pEnt, const RenderableInstance_t &instance, bool twoPass, bool bShadowDepth )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();

#ifdef PORTAL
	Assert( ( g_pPortalRender->GetViewRecursionLevel() > 0 ) || !IsSplitScreenSupported() || pEnt->ShouldDrawForSplitScreenUser( GET_ACTIVE_SPLITSCREEN_SLOT() ) );
#else
	Assert( !IsSplitScreenSupported() || pEnt->ShouldDrawForSplitScreenUser( GET_ACTIVE_SPLITSCREEN_SLOT() ) );
#endif

	// Renderable list building should already have caught this
	Assert( instance.m_nAlpha > 0 );

	// Determine blending amount and tell engine
	float blend = (float)( instance.m_nAlpha / 255.0f );

	// Tell engine
	render->SetBlend( blend );

	float color[3];
	pEnt->GetColorModulation( color );
	render->SetColorModulation(	color );

	int flags = STUDIO_RENDER | STUDIO_TRANSPARENCY;
	if ( twoPass )
		flags |= STUDIO_TWOPASS;

	if ( bShadowDepth )
		flags |= STUDIO_SHADOWDEPTHTEXTURE;

	DrawRenderable( pEnt, flags, instance, bShadowDepth );
}


//-----------------------------------------------------------------------------
// Renders all translucent entities in the render list
//-----------------------------------------------------------------------------
void CRendering3dView::DrawTranslucentRenderablesNoWorld( bool bInSkybox )
{
	VPROF( "CViewRender::DrawTranslucentRenderablesNoWorld" );

	if ( !m_pMainView->ShouldDrawEntities() || !r_drawtranslucentrenderables.GetBool() )
		return;

	// Draw the particle singletons.
	DrawParticleSingletons( bInSkybox );

	bool bShadowDepth = (m_DrawFlags & DF_SHADOW_DEPTH_MAP ) != 0;

#if defined(_PS3)
	CClientRenderablesList::CEntry *pEntities = m_pRenderablesList[ g_viewBuilder.GetBuildViewID() ]->m_RenderGroups[RENDER_GROUP_TRANSLUCENT];
	int iCurTranslucentEntity = m_pRenderablesList[ g_viewBuilder.GetBuildViewID() ]->m_RenderGroupCounts[RENDER_GROUP_TRANSLUCENT] - 1;
#else
	CClientRenderablesList::CEntry *pEntities = m_pRenderables->m_RenderGroups[RENDER_GROUP_TRANSLUCENT];
	int iCurTranslucentEntity = m_pRenderables->m_RenderGroupCounts[RENDER_GROUP_TRANSLUCENT] - 1;
#endif

	while( iCurTranslucentEntity >= 0 )
	{
		IClientRenderable *pRenderable = pEntities[iCurTranslucentEntity].m_pRenderable;
		int nRenderFlags = pRenderable->GetRenderFlags();
		
		if ( nRenderFlags & ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB )
		{
			UpdateRefractTexture();
		}

		if ( nRenderFlags & ERENDERFLAGS_NEEDS_FULL_FB )
		{
			UpdateScreenEffectTexture();
		}

		DrawTranslucentRenderable( pRenderable, pEntities[iCurTranslucentEntity].m_InstanceData, 
			pEntities[iCurTranslucentEntity].m_TwoPass != 0, bShadowDepth );
		--iCurTranslucentEntity;
	}

	// Reset the blend state.
	render->SetBlend( 1 );
}


//-----------------------------------------------------------------------------
// Renders all translucent entities in the render list that ignore the Z buffer
//-----------------------------------------------------------------------------
void CRendering3dView::DrawNoZBufferTranslucentRenderables( void )
{
	VPROF( "CViewRender::DrawNoZBufferTranslucentRenderables" );

	if ( !m_pMainView->ShouldDrawEntities() || !r_drawtranslucentrenderables.GetBool() )
		return;

	bool bShadowDepth = (m_DrawFlags & DF_SHADOW_DEPTH_MAP ) != 0;

	// FIXME: This ignores Z. We don't need to sort it at all? Not sure about refraction here...
	// Could use fast path
#if defined(_PS3)
	CClientRenderablesList::CEntry *pEntities = m_pRenderablesList[ g_viewBuilder.GetBuildViewID() ]->m_RenderGroups[RENDER_GROUP_TRANSLUCENT_IGNOREZ];
	int iCurTranslucentEntity = m_pRenderablesList[ g_viewBuilder.GetBuildViewID() ]->m_RenderGroupCounts[RENDER_GROUP_TRANSLUCENT_IGNOREZ] - 1;
#else
	CClientRenderablesList::CEntry *pEntities = m_pRenderables->m_RenderGroups[RENDER_GROUP_TRANSLUCENT_IGNOREZ];
	int iCurTranslucentEntity = m_pRenderables->m_RenderGroupCounts[RENDER_GROUP_TRANSLUCENT_IGNOREZ] - 1;
#endif

	while( iCurTranslucentEntity >= 0 )
	{
		IClientRenderable *pRenderable = pEntities[iCurTranslucentEntity].m_pRenderable;
		int nRenderFlags = pRenderable->GetRenderFlags();
		if ( nRenderFlags & ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB )
		{
			UpdateRefractTexture();
		}

		if ( nRenderFlags & ERENDERFLAGS_NEEDS_FULL_FB )
		{
			UpdateScreenEffectTexture();
		}

		DrawTranslucentRenderable( pRenderable, pEntities[iCurTranslucentEntity].m_InstanceData,
			pEntities[iCurTranslucentEntity].m_TwoPass != 0, bShadowDepth );
		--iCurTranslucentEntity;
	}

	// Reset the blend state.
	render->SetBlend( 1 );
}


static ConVar r_unlimitedrefract( "r_unlimitedrefract", "0" );

static void UpdateNecessaryRenderTargets( int nRenderFlags )
{
	if ( nRenderFlags )
	{
		CMatRenderContextPtr pRenderContext( materials );
		ITexture *rt = pRenderContext->GetRenderTarget();

		// supress refract update
		if (
			( nRenderFlags & ERENDERFLAGS_REFRACT_ONLY_ONCE_PER_FRAME ) &&
			( nRenderFlags & ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB ) &&
			( gpGlobals->framecount == g_viewscene_refractUpdateFrame ) &&
			( r_unlimitedrefract.GetInt() == 0 ) )
		{
			nRenderFlags &= ~( ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB );
		}
		if ( rt )
		{
			if ( nRenderFlags & ERENDERFLAGS_NEEDS_FULL_FB )
			{
				UpdateScreenEffectTexture( 0, 0, 0, rt->GetActualWidth(), rt->GetActualHeight(), true );
			}
			else if ( nRenderFlags & ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB )
			{
				UpdateRefractTexture(0, 0, rt->GetActualWidth(), rt->GetActualHeight());
			}
		}
		else
		{
			if ( nRenderFlags & ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB )
			{
				UpdateRefractTexture();
			}
		}

		pRenderContext.SafeRelease();
	}
}

#ifdef PORTAL //if we're in the portal mod, we need to make a detour so we can render portal views using stencil areas
void CRendering3dView::DrawRecursivePortalViews( void )
{
	if( ShouldDrawPortals() ) //no recursive stencil views during skybox rendering (although we might be drawing a skybox while already in a recursive stencil view)
	{
		int iDrawFlagsBackup = m_DrawFlags;

		if( g_pPortalRender->DrawPortalsUsingStencils( (CViewRender *)m_pMainView ) )// @MULTICORE (toml 8/10/2006): remove this hack cast
		{
			m_DrawFlags = iDrawFlagsBackup;

			//reset visibility
			unsigned int iVisFlags = 0;
			m_pMainView->SetupVis( *this, iVisFlags, m_pCustomVisibility );		

			//recreate drawlists (since I can't find an easy way to backup the originals)
			{
				SafeRelease( m_pWorldRenderList );
				SafeRelease( m_pWorldListInfo );
				BuildWorldRenderLists( ((m_DrawFlags & DF_DRAW_ENTITITES) != 0), m_pCustomVisibility ? m_pCustomVisibility->m_iForceViewLeaf : -1 );

				AssertMsg( m_DrawFlags & DF_DRAW_ENTITITES, "It shouldn't be possible to get here if this wasn't set, needs special case investigation" );
			}

			if( r_depthoverlay.GetBool() )
			{
				CMatRenderContextPtr pRenderContext( materials );
				ITexture *pDepthTex = GetFullFrameDepthTexture();

				if ( pDepthTex )
				{
					IMaterial *pMaterial = materials->FindMaterial( "debug/showz", TEXTURE_GROUP_OTHER, true );
					IMaterialVar *BaseTextureVar = pMaterial->FindVar( "$basetexture", NULL, false );
					IMaterialVar *pDepthInAlpha = NULL;
					if( IsPC() )
					{
						pDepthInAlpha = pMaterial->FindVar( "$ALPHADEPTH", NULL, false );
						pDepthInAlpha->SetIntValue( 1 );
					}

					BaseTextureVar->SetTextureValue( pDepthTex );

					pRenderContext->OverrideDepthEnable( true, false ); //don't write to depth, or else we'll never see translucents
					pRenderContext->DrawScreenSpaceQuad( pMaterial );
					pRenderContext->OverrideDepthEnable( false, true );
				}
			}
		}
		// PS3 reads directly from the depth buffer alias texture, so we don't need to update the full-screen depth texture.
		else if ( !IsPS3() )
		{
			//done recursing in, time to go back out and do translucents
			CMatRenderContextPtr pRenderContext( materials );		
			UpdateFullScreenDepthTexture();
		}
	}
}
#endif

//-----------------------------------------------------------------------------
// Renders all translucent world, entities, and detail objects in a particular set of leaves
//-----------------------------------------------------------------------------
//$TODO(msmith) This "fast path" (cl_tlucfastpath) doesn't currently work on css15.  Alpha sorting is messed up.
//              Investingation should be made as to why it's broken before turning it back on by default.  And we should only look to turn
//				it on if when we do turn it on there is a performance win (on PS3 and Xbox360).
ConVar cl_tlucfastpath( "cl_tlucfastpath", "1" );
extern ConVar cl_colorfastpath;

void CRendering3dView::DrawTranslucentRenderables( bool bInSkybox, bool bShadowDepth )
{
#if !defined( PORTAL )
	{
		//opaques generally write depth, and translucents generally don't.
		//So immediately after opaques are done is the best time to snap off the depth buffer to a texture.
		switch ( g_CurrentViewID )
		{				 
		case VIEW_MAIN:
#ifdef _GAMECONSOLE
		case VIEW_INTRO_CAMERA:
		case VIEW_INTRO_PLAYER:
#endif
			UpdateFullScreenDepthTexture();
			break;

		default:
			materials->GetRenderContext()->SetFullScreenDepthTextureValidityFlag( false );
			break;
		}
	}
#endif

	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "DrawTranslucent" );

#if defined(_PS3)
	const ClientWorldListInfo_t& info = *m_pWorldListInfo[ g_viewBuilder.GetBuildViewID() ];
#else
	const ClientWorldListInfo_t& info = *m_pWorldListInfo;
#endif

	if ( !r_drawtranslucentworld.GetBool() || ( m_DrawFlags & ( DF_DRAW_SIMPLE_WORLD_MODEL | DF_DRAW_SIMPLE_WORLD_MODEL_WATER ) ) )
	{
		DrawTranslucentRenderablesNoWorld( bInSkybox );
		return;
	}

	int iPrevLeaf = info.m_LeafCount - 1;
	int nDetailLeafCount = 0;
	LeafIndex_t *pDetailLeafList = (LeafIndex_t*)stackalloc( info.m_LeafCount * sizeof(LeafIndex_t) );

// 	bool bDrawUnderWater = (nFlags & DF_RENDER_UNDERWATER) != 0;
// 	bool bDrawAboveWater = (nFlags & DF_RENDER_ABOVEWATER) != 0;
// 	bool bDrawWater = (nFlags & DF_RENDER_WATER) != 0;
// 	bool bClipSkybox = (nFlags & DF_CLIP_SKYBOX ) != 0;
	unsigned long nEngineDrawFlags = BuildEngineDrawWorldListFlags( m_DrawFlags & ~DF_DRAWSKYBOX );

	DetailObjectSystem()->BeginTranslucentDetailRendering();

	if ( m_pMainView->ShouldDrawEntities() && r_drawtranslucentrenderables.GetBool() )
	{
		VPROF_BUDGET( "DrawTranslucentEntities", "DrawTranslucentEntities" );

		MDLCACHE_CRITICAL_SECTION();
		// Draw the particle singletons.
		DrawParticleSingletons( bInSkybox );

#if defined(_PS3)
		int nTranslucentRenderableCount = m_pRenderablesList[ g_viewBuilder.GetBuildViewID() ]->m_RenderGroupCounts[RENDER_GROUP_TRANSLUCENT];
		CClientRenderablesList::CEntry *pEntities = m_pRenderablesList[ g_viewBuilder.GetBuildViewID() ]->m_RenderGroups[RENDER_GROUP_TRANSLUCENT];
#else
		int nTranslucentRenderableCount = m_pRenderables->m_RenderGroupCounts[RENDER_GROUP_TRANSLUCENT];
		CClientRenderablesList::CEntry *pEntities = m_pRenderables->m_RenderGroups[RENDER_GROUP_TRANSLUCENT];
#endif

		int iCurTranslucentEntity = nTranslucentRenderableCount - 1;

		int *pFastPathIndex = (int*)stackalloc( nTranslucentRenderableCount * sizeof( int ) );
		CUtlVector< ModelRenderSystemData_t > fastPathData( (ModelRenderSystemData_t *)stackalloc( nTranslucentRenderableCount * sizeof( ModelRenderSystemData_t ) ), nTranslucentRenderableCount );
		TranslucentInstanceRenderData_t *pRenderData = (TranslucentInstanceRenderData_t*)stackalloc( nTranslucentRenderableCount * sizeof( TranslucentInstanceRenderData_t ) );
		TranslucentTempData_t tempData;
		tempData.m_pColorMeshHandles = ( DataCacheHandle_t * )stackalloc( nTranslucentRenderableCount * sizeof( DataCacheHandle_t ) );
		bool bDoFastPath = cl_tlucfastpath.GetBool();
		bool bColorizeFastPath = cl_colorfastpath.GetBool();
		IMaterial *pFastPathColorMaterial = g_pModelRenderSystem->GetFastPathColorMaterial();
		if ( bDoFastPath )
		{
			for ( int i = 0; i < nTranslucentRenderableCount; ++i )
			{
				Assert( pEntities[i].m_pRenderable );
				IClientRenderable *pRenderable = pEntities[i].m_pRenderable;
				IClientModelRenderable *pModelRenderable = pRenderable->GetClientModelRenderable();

				// FIXME: Deal with brush models in the translucent path
				if ( pModelRenderable && modelinfo->GetModelType( pRenderable->GetModel() ) == mod_studio )
				{
					int j = fastPathData.AddToTail( );
					ModelRenderSystemData_t &data = fastPathData[j];
					data.m_pRenderable = pRenderable;
					data.m_pModelRenderable = pModelRenderable;
					data.m_InstanceData = pEntities[i].m_InstanceData;
					pFastPathIndex[i] = j;
				}
				else
				{
					pFastPathIndex[i] = -1;
				}
			}

			g_pModelRenderSystem->ComputeTranslucentRenderData( fastPathData.Base(), fastPathData.Count(), pRenderData, &tempData );
		}

		bool bRenderingWaterRenderTargets = ( m_DrawFlags & ( DF_RENDER_REFRACTION | DF_RENDER_REFLECTION ) ) ? true : false;

#if defined(_PS3)
		CClientRenderablesList *pRenderablesList = m_pRenderablesList[ g_viewBuilder.GetBuildViewID() ];
#else
		CClientRenderablesList *pRenderablesList = m_pRenderables;
#endif
		while( iCurTranslucentEntity >= 0 )
		{
			// Seek the current leaf up to our current translucent-entity leaf.
			int iThisLeaf = pEntities[iCurTranslucentEntity].m_iWorldListInfoLeaf;

			// First draw the translucent parts of the world up to and including those in this leaf
			DrawTranslucentWorldAndDetailPropsInLeaves( pRenderContext, iPrevLeaf, iThisLeaf, nEngineDrawFlags, nDetailLeafCount, pDetailLeafList, bShadowDepth );

			// We're traversing the leaf list backwards to get the appropriate sort ordering (back to front)
			iPrevLeaf = iThisLeaf - 1;

			// Draw all the translucent entities with this leaf.
			int nLeaf = info.m_pLeafDataList[iThisLeaf].leafIndex;

			bool bDrawDetailProps = ClientLeafSystem()->ShouldDrawDetailObjectsInLeaf( nLeaf, m_pMainView->BuildWorldListsNumber() );
			if ( bDrawDetailProps )
			{
				// Draw detail props up to but not including this leaf
				Assert( nDetailLeafCount > 0 ); 
				--nDetailLeafCount;
				Assert( pDetailLeafList[nDetailLeafCount] == nLeaf );
				DetailObjectSystem()->RenderTranslucentDetailObjects( pRenderablesList->m_DetailFade, CurrentViewOrigin(), CurrentViewForward(), CurrentViewRight(), CurrentViewUp(), nDetailLeafCount, pDetailLeafList );

				// HACK: for now just draw all the detail props in this leaf right away. They're alphatest and write z, so there's no need to manually sort them.
				DetailObjectSystem()->RenderTranslucentDetailObjectsInLeaf( pRenderablesList->m_DetailFade, CurrentViewOrigin(), CurrentViewForward(), CurrentViewRight(), CurrentViewUp(), nLeaf, NULL );

				// Draw translucent renderables in the leaf interspersed with detail props
				for( ;pEntities[iCurTranslucentEntity].m_iWorldListInfoLeaf == iThisLeaf && iCurTranslucentEntity >= 0; --iCurTranslucentEntity )
				{
					IClientRenderable *pRenderable = pEntities[iCurTranslucentEntity].m_pRenderable;

					// Draw any detail props in this leaf that's farther than the entity
					
					// HACK: for now don't do this, we already rendered all the detail props, and they should sort ok. See above comment.
					//const Vector &vecRenderOrigin = pRenderable->GetRenderOrigin();
					//DetailObjectSystem()->RenderTranslucentDetailObjectsInLeaf( pRenderablesList->m_DetailFade, CurrentViewOrigin(), CurrentViewForward(), CurrentViewRight(), CurrentViewUp(), nLeaf, &vecRenderOrigin );

					int nRenderFlags = pRenderable->GetRenderFlags();
					
					if ( ( nRenderFlags & ( ERENDERFLAGS_NEEDS_FULL_FB | ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB ) ) && !bShadowDepth )
					{
						if( bRenderingWaterRenderTargets )
						{
							continue;
						}

						UpdateNecessaryRenderTargets( nRenderFlags );
					}

					// Then draw the translucent renderable
					if ( bDoFastPath && pFastPathIndex[iCurTranslucentEntity] >= 0 )
					{
						if ( bColorizeFastPath )
						{
							g_pStudioRender->ForcedMaterialOverride( pFastPathColorMaterial );
						}
						TranslucentInstanceRenderData_t &renderData = pRenderData[ pFastPathIndex[iCurTranslucentEntity] ]; 

						int nDrawFlags = pEntities[iCurTranslucentEntity].m_TwoPass ? STUDIORENDER_DRAW_TRANSLUCENT_ONLY : STUDIORENDER_DRAW_ENTIRE_MODEL;
						g_pStudioRender->DrawModelArray( *renderData.m_pModelInfo, 1, renderData.m_pInstanceData, sizeof(StudioArrayInstanceData_t), nDrawFlags );
						g_pStudioRender->ForcedMaterialOverride( NULL );
					}
					else
					{
						DrawTranslucentRenderable( pRenderable, pEntities[iCurTranslucentEntity].m_InstanceData,
						(pEntities[iCurTranslucentEntity].m_TwoPass != 0), bShadowDepth );
					}
				}

				// Draw all remaining props in this leaf
				// HACK: for now don't do this, we already rendered all the detail props, and they should sort ok. See above comment.
				//DetailObjectSystem()->RenderTranslucentDetailObjectsInLeaf( pRenderablesList->m_DetailFade, CurrentViewOrigin(), CurrentViewForward(), CurrentViewRight(), CurrentViewUp(), nLeaf, NULL );
			}
			else
			{
				// Draw queued up detail props (we know that the list of detail leaves won't include this leaf, since ShouldDrawDetailObjectsInLeaf is false)
				// Therefore no fixup on nDetailLeafCount is required as in the above section
				DetailObjectSystem()->RenderTranslucentDetailObjects( pRenderablesList->m_DetailFade, CurrentViewOrigin(), CurrentViewForward(), CurrentViewRight(), CurrentViewUp(), nDetailLeafCount, pDetailLeafList );

				for( ;pEntities[iCurTranslucentEntity].m_iWorldListInfoLeaf == iThisLeaf && iCurTranslucentEntity >= 0; --iCurTranslucentEntity )
				{
					IClientRenderable *pRenderable = pEntities[iCurTranslucentEntity].m_pRenderable;

					int nRenderFlags = pRenderable->GetRenderFlags();

					if ( ( nRenderFlags & ( ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB | ERENDERFLAGS_NEEDS_FULL_FB ) ) && !bShadowDepth )
					{
						if( bRenderingWaterRenderTargets )
						{
							continue;
						}
						UpdateNecessaryRenderTargets( nRenderFlags );
					}

					if ( bDoFastPath && pFastPathIndex[iCurTranslucentEntity] >= 0 )
					{
						TranslucentInstanceRenderData_t &renderData = pRenderData[ pFastPathIndex[iCurTranslucentEntity] ];
						if ( renderData.m_pInstanceData )
						{
							if (bColorizeFastPath)
							{
								g_pStudioRender->ForcedMaterialOverride( pFastPathColorMaterial );
							}
							int nDrawFlags = pEntities[iCurTranslucentEntity].m_TwoPass ? STUDIORENDER_DRAW_TRANSLUCENT_ONLY : STUDIORENDER_DRAW_ENTIRE_MODEL;
							g_pStudioRender->DrawModelArray( *renderData.m_pModelInfo, 1, renderData.m_pInstanceData, sizeof(StudioArrayInstanceData_t), nDrawFlags );
							g_pStudioRender->ForcedMaterialOverride( NULL );
						}
					}
					else
					{
						DrawTranslucentRenderable( pRenderable, pEntities[iCurTranslucentEntity].m_InstanceData,
						(pEntities[iCurTranslucentEntity].m_TwoPass != 0), bShadowDepth );
					}
				}
			}

			nDetailLeafCount = 0;
		}

		if ( bDoFastPath )
		{
			g_pModelRenderSystem->CleanupTranslucentTempData( &tempData );
		}
	}

	// Draw the rest of the surfaces in world leaves
	DrawTranslucentWorldAndDetailPropsInLeaves( pRenderContext, iPrevLeaf, 0, nEngineDrawFlags, nDetailLeafCount, pDetailLeafList, bShadowDepth );

	// Draw any queued-up detail props from previously visited leaves
#if defined(_PS3)
	DetailObjectSystem()->RenderTranslucentDetailObjects( m_pRenderablesList[ g_viewBuilder.GetBuildViewID() ]->m_DetailFade, CurrentViewOrigin(), CurrentViewForward(), CurrentViewRight(), CurrentViewUp(), nDetailLeafCount, pDetailLeafList );
#else
	DetailObjectSystem()->RenderTranslucentDetailObjects( m_pRenderables->m_DetailFade, CurrentViewOrigin(), CurrentViewForward(), CurrentViewRight(), CurrentViewUp(), nDetailLeafCount, pDetailLeafList );
#endif

	// Reset the blend state.
	render->SetBlend( 1 );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CRendering3dView::EnableWorldFog( void )
{
	VPROF("CViewRender::EnableWorldFog");
	CMatRenderContextPtr pRenderContext( materials );

	fogparams_t *pFogParams = NULL;
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if ( pbp )
	{
		pFogParams = pbp->GetFogParams();
	}

	if( GetFogEnable( pFogParams ) )
	{
		float fogColor[3];
		GetFogColor( pFogParams, fogColor );
		pRenderContext->FogMode( MATERIAL_FOG_LINEAR );
		pRenderContext->FogColor3fv( fogColor );
		pRenderContext->FogStart( GetFogStart( pFogParams ) );
		pRenderContext->FogEnd( GetFogEnd( pFogParams ) );
		pRenderContext->FogMaxDensity( GetFogMaxDensity( pFogParams ) );
	}
	else
	{
		pRenderContext->FogMode( MATERIAL_FOG_NONE );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CRendering3dView::GetDrawFlags()
{
	return m_DrawFlags;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CRendering3dView::SetFogVolumeState( const VisibleFogVolumeInfo_t &fogInfo, bool bUseHeightFog )
{
	render->SetFogVolumeState( fogInfo.m_nVisibleFogVolume, bUseHeightFog );

#ifdef PORTAL

	//the idea behind fog shifting is this...
	//Normal fog simulates the effect of countless tiny particles between your viewpoint and whatever geometry is rendering.
	//But, when rendering to a portal view, there's a large space between the virtual camera and the portal exit surface.
	//This space isn't supposed to exist, and therefore has none of the tiny particles that make up fog.
	//So, we have to shift fog start/end out to align the distances with the portal exit surface instead of the virtual camera to eliminate fog simulation in the non-space
	if( g_pPortalRender->GetViewRecursionLevel() == 0 )
		return; //rendering one of the primary views, do nothing

	g_pPortalRender->ShiftFogForExitPortalView();

#endif //#ifdef PORTAL
}


//-----------------------------------------------------------------------------
// Standard 3d skybox view
//-----------------------------------------------------------------------------
SkyboxVisibility_t CSkyboxView::ComputeSkyboxVisibility()
{
	return engine->IsSkyboxVisibleFromPoint( origin );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CSkyboxView::GetSkyboxFogEnable()
{
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if( !pbp )
	{
		return false;
	}
	CPlayerLocalData	*local		= &pbp->m_Local;

	if( fog_override.GetInt() )
	{
		if( fog_enableskybox.GetInt() )
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return !!local->m_skybox3d.fog.enable;
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CSkyboxView::Enable3dSkyboxFog( void )
{
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if( !pbp )
	{
		return;
	}
	CPlayerLocalData	*local		= &pbp->m_Local;

	CMatRenderContextPtr pRenderContext( materials );

	if( GetSkyboxFogEnable() )
	{
		float fogColor[3];
		GetSkyboxFogColor( fogColor );
		float scale = 1.0f;
		if ( local->m_skybox3d.scale > 0.0f )
		{
			scale = 1.0f / local->m_skybox3d.scale;
		}
		pRenderContext->FogMode( MATERIAL_FOG_LINEAR );
		pRenderContext->FogColor3fv( fogColor );
		pRenderContext->FogStart( GetSkyboxFogStart() * scale );
		pRenderContext->FogEnd( GetSkyboxFogEnd() * scale );
		pRenderContext->FogMaxDensity( GetSkyboxFogMaxDensity() );
	}
	else
	{
		pRenderContext->FogMode( MATERIAL_FOG_NONE );
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
sky3dparams_t *CSkyboxView::PreRender3dSkyboxWorld( SkyboxVisibility_t nSkyboxVisible )
{
	if ( ( nSkyboxVisible != SKYBOX_3DSKYBOX_VISIBLE ) && r_3dsky.GetInt() != 2 )
		return NULL;

	// render the 3D skybox
	if ( !r_3dsky.GetInt() )
		return NULL;

	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();

	// No local player object yet...
	if ( !pbp )
		return NULL;

	CPlayerLocalData* local = &pbp->m_Local;
	if ( local->m_skybox3d.area == 255 )
		return NULL;

	return &local->m_skybox3d;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CSkyboxView::DrawInternal( view_id_t iSkyBoxViewID, bool bInvokePreAndPostRender, ITexture *pRenderTarget )
{
	PS3_SPUPATH_INVALID( "CSkyboxView::DrawInternal" );

	unsigned char **areabits = render->GetAreaBits();
	unsigned char *savebits;
	unsigned char tmpbits[ 32 ];
	savebits = *areabits;
	memset( tmpbits, 0, sizeof(tmpbits) );

	// set the sky area bit
	tmpbits[m_pSky3dParams->area>>3] |= 1 << (m_pSky3dParams->area&7);

	*areabits = tmpbits;

	// if you can get really close to the skybox geometry it's possible that you'll be able to clip into it
	// with this near plane.  If so, move it in a bit.  It's at 2.0 to give us more precision.  That means you 
	// need to keep the eye position at least 2 * scale away from the geometry in the skybox
	zNear = 2.0;
	zFar = MAX_TRACE_LENGTH;

	// scale origin by sky scale and translate to sky origin
	{
		float scale = (m_pSky3dParams->scale > 0) ? (1.0f / m_pSky3dParams->scale) : 1.0f;
		Vector vSkyOrigin = m_pSky3dParams->origin;
		VectorScale( origin, scale, origin );
		VectorAdd( origin, vSkyOrigin, origin );

		if( m_bCustomViewMatrix )
		{
			Vector vTransformedSkyOrigin;
			VectorRotate( vSkyOrigin, m_matCustomViewMatrix, vTransformedSkyOrigin ); //Rotate instead of transform because we haven't scale the existing offset yet

			//scale existing translation, and tack on the skybox offset (subtract because it's a view matrix)
			m_matCustomViewMatrix.m_flMatVal[0][3] = (m_matCustomViewMatrix.m_flMatVal[0][3] * scale) - vTransformedSkyOrigin.x;
			m_matCustomViewMatrix.m_flMatVal[1][3] = (m_matCustomViewMatrix.m_flMatVal[1][3] * scale) - vTransformedSkyOrigin.y;
			m_matCustomViewMatrix.m_flMatVal[2][3] = (m_matCustomViewMatrix.m_flMatVal[2][3] * scale) - vTransformedSkyOrigin.z;
		}
	}

	Enable3dSkyboxFog();

	// BUGBUG: Fix this!!!  We shouldn't need to call setup vis for the sky if we're connecting
	// the areas.  We'd have to mark all the clusters in the skybox area in the PVS of any 
	// cluster with sky.  Then we could just connect the areas to do our vis.
	//m_bOverrideVisOrigin could hose us here, so call direct
	render->ViewSetupVis( false, 1, &m_pSky3dParams->origin.Get() );
	CMatRenderContextPtr pRenderContext( materials );
	render->Push3DView( pRenderContext, (*this), m_ClearFlags, pRenderTarget, GetFrustum() );

	// For stereo rendering, we need to set the stereo depth by scaling the full matrix by the scale of the 3d skybox
	if ( materials->IsStereoActiveThisFrame() )
	{
		pRenderContext->MatrixMode( MATERIAL_VIEW );

		VMatrix matStereoScale;
		matStereoScale.Identity();
		float flScale = ( m_pSky3dParams->scale > 0 ) ? ( m_pSky3dParams->scale ) : 1.0f;
		for ( int i = 0; i < 4; i++ ) // Yes, I am setting the entire diagonal on purpose to hint to nvidia's driver the intended stereo depth
		{
			matStereoScale.m[i][i] = flScale;
		}
		pRenderContext->MultMatrix( matStereoScale );

		pRenderContext->MatrixMode( MATERIAL_MODEL );
	}

	// Store off view origin and angles
	SetupCurrentView( origin, angles, iSkyBoxViewID );

#if defined( _X360 )
	pRenderContext->PushVertexShaderGPRAllocation( 32 );
#endif

	// Invoke pre-render methods
	if ( bInvokePreAndPostRender )
	{
		IGameSystem::PreRenderAllSystems();
	}

	BEGIN_2PASS_BUILD_BLOCK
	PROLOGUE_PASS_DRAWLISTS

	render->BeginUpdateLightmaps();
	BuildWorldRenderLists( true, -1, true );
	g_viewBuilder.FlushBuildWorldListJob();
	BuildRenderableRenderLists( iSkyBoxViewID );
	render->EndUpdateLightmaps();

	END_2PASS_BLOCK


	BEGIN_2PASS_DRAW_BLOCK
	EPILOGUE_PASS_DRAWLISTS

	DrawWorld( pRenderContext, 0.0f );

	// Iterate over all leaves and render objects in those leaves
	DrawOpaqueRenderables( pRenderContext, RENDERABLES_RENDER_PATH_NORMAL, DEPTH_MODE_NORMAL, NULL );

#if defined( PORTAL )
	DrawRecursivePortalViews(); //Dave K: probably does nothing in this view, calling it anyway because it's contents were directly inside DrawTranslucentRenderables() a moment ago and I don't want to risk breaking anything
#endif

	// Iterate over all leaves and render objects in those leaves
	DrawTranslucentRenderables( true, false );
	DrawNoZBufferTranslucentRenderables();

	m_pMainView->DisableFog();

	CGlowOverlay::UpdateSkyOverlays( zFar, m_bCacheFullSceneState );

	PixelVisibility_EndCurrentView();

	// restore old area bits
	*areabits = savebits;
	pRenderContext.SafeRelease();

	// Invoke post-render methods
	if( bInvokePreAndPostRender )
	{
		IGameSystem::PostRenderAllSystems();
		FinishCurrentView();
	}

	// FIXME: Workaround to 3d skybox not depth-of-fielding properly. The real fix is for the 3d skybox dest alpha depth values
	// to be biased. Currently all I do is clear alpha to 1 after the 3D skybox path. This avoids the skybox being unblurred.
	if( IsDepthOfFieldEnabled() )
	{
		// draw a fullscreen quad setting destalpha to 1

		IMaterial *pMat = materials->FindMaterial( "dev/clearalpha", TEXTURE_GROUP_OTHER, true );
		if ( pMat != NULL )
		{
			int nWidth = 0;
			int nHeight = 0;
			int nDummy = 0;
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->GetViewport( nDummy, nDummy, nWidth, nHeight );

			pRenderContext->DrawScreenSpaceRectangle(
				pMat,
				0, 0, nWidth, nHeight,
				0, 0, nWidth-1, nHeight-1,
				nWidth, nHeight, NULL /*GetClientWorldEntity()->GetClientRenderable()*/ );
		}
	}

	END_2PASS_BLOCK


	pRenderContext.GetFrom( materials );
	render->PopView( pRenderContext, GetFrustum() );

#if defined( _X360 )
	pRenderContext->PopVertexShaderGPRAllocation();
#endif
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CSkyboxView::Setup( const CViewSetup &view, int *pClearFlags, SkyboxVisibility_t *pSkyboxVisible )
{
	BaseClass::Setup( view );

	// The skybox might not be visible from here
	*pSkyboxVisible = ComputeSkyboxVisibility();
	m_pSky3dParams = PreRender3dSkyboxWorld( *pSkyboxVisible );

	if ( !m_pSky3dParams )
	{
		return false;
	}

	// At this point, we've cleared everything we need to clear
	// The next path will need to clear depth, though.
	m_ClearFlags = *pClearFlags;
	*pClearFlags &= ~( VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH | VIEW_CLEAR_STENCIL | VIEW_CLEAR_FULL_TARGET );
	*pClearFlags |= VIEW_CLEAR_DEPTH; // Need to clear depth after rednering the skybox

	m_DrawFlags = DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER | DF_RENDER_WATER;
	if( r_skybox.GetBool() )
	{
		m_DrawFlags |= DF_DRAWSKYBOX;
	}

	return true;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CSkyboxView::Draw()
{
	VPROF_BUDGET( "CViewRender::Draw3dSkyboxworld", "3D Skybox" );

	DrawInternal();
}


#ifdef PORTAL
//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CPortalSkyboxView::Setup( const CViewSetup &view, int *pClearFlags, SkyboxVisibility_t *pSkyboxVisible, ITexture *pRenderTarget )
{
	if ( !BaseClass::Setup( view, pClearFlags, pSkyboxVisible ) )
		return false;

	m_pRenderTarget = pRenderTarget;
	return true;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
SkyboxVisibility_t CPortalSkyboxView::ComputeSkyboxVisibility()
{
	return g_pPortalRender->IsSkyboxVisibleFromExitPortal();
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CPortalSkyboxView::Draw()
{
	AssertMsg( (g_pPortalRender->GetViewRecursionLevel() != 0) && g_pPortalRender->IsRenderingPortal(), "This is designed for through-portal views. Use the regular skybox drawing code for primary views" );

	VPROF( "CViewRender::Draw3dSkyboxworld_Portal" );

	int iCurrentViewID = g_CurrentViewID;

	Frustum FrustumBackup;
	memcpy( FrustumBackup, GetFrustum(), sizeof( Frustum ) );

	CMatRenderContextPtr pRenderContext( materials );

	bool bClippingEnabled = pRenderContext->EnableClipping( false );

	//NOTE: doesn't magically map to VIEW_3DSKY at (0,0) like PORTAL_VIEWID maps to VIEW_MAIN
	view_id_t iSkyBoxViewID = (view_id_t)g_pPortalRender->GetCurrentSkyboxViewId();

	bool bInvokePreAndPostRender = false;

	DrawInternal( iSkyBoxViewID, bInvokePreAndPostRender, m_pRenderTarget );

	pRenderContext->ClearBuffersObeyStencil( false, true );

#ifdef _PS3
	// Reload z-cull after 3D skybox render if in a portal view since we just re-cleared depth values
	if ( g_pPortalRender->GetViewRecursionLevel() > 0 )
	{
		g_pPortalRender->ReloadZcullMemory();
	}
#endif // _PS3

	pRenderContext->EnableClipping( bClippingEnabled );

	memcpy( GetFrustum(), FrustumBackup, sizeof( Frustum ) );
	render->OverrideViewFrustum( FrustumBackup );

	g_CurrentViewID = iCurrentViewID;
}
#endif // PORTAL



#ifdef PORTAL2
//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CAperturePhotoView::Setup( C_BaseEntity *pTargetEntity, const CViewSetup &view, int *pClearFlags, SkyboxVisibility_t *pSkyboxVisible, ITexture *pRenderTarget )
{
	if( pTargetEntity == NULL )
		return false;

	IClientRenderable *pEntRenderable = pTargetEntity->GetClientRenderable();

	if( pEntRenderable == NULL )
		return false;

	m_pTargetEntity = pTargetEntity;

	VisibleFogVolumeInfo_t fogInfo;
	WaterRenderInfo_t waterInfo;

	memset( &fogInfo, 0, sizeof( VisibleFogVolumeInfo_t ) );
	memset( &waterInfo, 0, sizeof( WaterRenderInfo_t ) );
	waterInfo.m_bCheapWater = true;
	BaseClass::Setup( view, *pClearFlags, false, fogInfo, waterInfo, NULL );
	
	m_pRenderTarget = pRenderTarget;
	return true;
}

ConVar cl_camera_minimal_photos( "cl_camera_minimal_photos", "1", 0, "Draw just the targetted entity when taking a camera photo" );


void AddIClientRenderableToRenderList( IClientRenderable *pRenderable, CClientRenderablesList *pRenderablesList )
{
	CClientRenderablesList::CEntry renderableEntry;
	RenderGroup_t group = ClientLeafSystem()->GenerateRenderListEntry( pRenderable, renderableEntry );
	if( group == RENDER_GROUP_COUNT )
		return;

	for( int i = 0; i != pRenderablesList->m_RenderGroupCounts[group]; ++i )
	{
		if( pRenderablesList->m_RenderGroups[group][i].m_pRenderable == pRenderable )
			return; //already in the list
	}

	int iAddIndex = pRenderablesList->m_RenderGroupCounts[group];
	++pRenderablesList->m_RenderGroupCounts[group];
	pRenderablesList->m_RenderGroups[group][iAddIndex] = renderableEntry;
}

void GetAllChildRenderables( C_BaseEntity *pEntity, IClientRenderable **pKeepers, int &iKeepCount, int iKeepArraySize )
{
	IClientRenderable *pThisRenderable = pEntity->GetClientRenderable();
	
	//avoid duplicates and infinite recursion
	for( int i = 0; i != iKeepCount; ++i )
	{
		if( pThisRenderable == pKeepers[i] )
			return;
	}

	pKeepers[iKeepCount++] = pThisRenderable;

	if( pEntity->ParticleProp() )
	{
		iKeepCount += pEntity->ParticleProp()->GetAllParticleEffectRenderables( &pKeepers[iKeepCount], iKeepArraySize - iKeepCount );
	}
	if( pEntity->GetEffectEntity() != NULL )
	{
		GetAllChildRenderables( pEntity->GetEffectEntity(), pKeepers, iKeepCount, iKeepArraySize );
	}
	for( C_BaseEntity *pMoveChild = pEntity->FirstMoveChild(); pMoveChild != NULL; pMoveChild = pMoveChild->NextMovePeer() )
	{
		GetAllChildRenderables( pMoveChild, pKeepers, iKeepCount, iKeepArraySize );
	}
}

ConVar cl_blur_test( "cl_blur_test", "0", 0, "Blurs entities that have had their photo taken" );
ConVar cl_photo_disable_model_alpha_writes( "cl_photo_disable_model_alpha_writes", "1", FCVAR_ARCHIVE, "Disallows the target entity in photos from writing to the photo's alpha channel" );
//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAperturePhotoView::Draw()
{
	CMatRenderContextPtr pRenderContext( materials );

	Vector vDiff = m_pTargetEntity->GetRenderOrigin() - origin;
	Vector vMins, vMaxs;
	m_pTargetEntity->GetRenderBounds( vMins, vMaxs );
	float fGoodDist = MAX((vMaxs - vMins).Length() * (1.5f/2.0f), 20.0f );
	float fLength = vDiff.Length();
	if( fLength > fGoodDist )
	{
		//move the camera closer for a better view
#if 1 //use camera forward as offset direction
		Vector vCameraForward;
		AngleVectors( angles, &vCameraForward );

		origin = m_pTargetEntity->WorldSpaceCenter() - (vCameraForward * fGoodDist); 
#else //use existing offset direction, but shorter
		origin = m_pTargetEntity->WorldSpaceCenter() - (vDiff * (fGoodDist / fLength));
#endif
		//Vector vCameraForward;
		//AngleVectors( angles, &vCameraForward );
		//origin += vCameraForward * ((fLength - fGoodDist) * vCameraForward.Dot( vDiff / fLength ));
	}

	render->Push3DView( pRenderContext, *this, VIEW_CLEAR_DEPTH | VIEW_CLEAR_COLOR | VIEW_CLEAR_STENCIL, m_pRenderTarget, GetFrustum() );
	SetupCurrentView( origin, angles, VIEW_MONITOR );


	MDLCACHE_CRITICAL_SECTION();

	bool bDrawEverything = !cl_camera_minimal_photos.GetBool();
	//Build the world list for now because I don't want to track down crash bugs and this doesn't happen often.
	BuildWorldRenderLists( true, -1, true, false ); // @MULTICORE (toml 8/9/2006): Portal problem, not sending custom vis down
	if( !bDrawEverything )
	{
		memset( m_pRenderablesList->m_RenderGroupCounts, 0, sizeof( m_pRenderablesList->m_RenderGroupCounts ) );
	}
	
	BuildRenderableRenderLists( CurrentViewID() );

	IClientRenderable *keepHandles[MAX_EDICTS];
	int iKeepChildren = 0;
	GetAllChildRenderables( m_pTargetEntity, keepHandles, iKeepChildren, MAX_EDICTS );

	//set the target entity as the only entity in the renderables list
	{
		if( !bDrawEverything )
			memset( m_pRenderablesList->m_RenderGroupCounts, 0, sizeof( m_pRenderablesList->m_RenderGroupCounts ) );

		for( int i = 0; i != iKeepChildren; ++i )
		{
			AddIClientRenderableToRenderList( keepHandles[i], m_pRenderablesList );
		}
	}

	engine->Sound_ExtraUpdate();	// Make sure sound doesn't stutter

	m_DrawFlags = m_pMainView->GetBaseDrawFlags() | DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER;	// Don't draw water surface...

	IMaterial *pPhotoBackground = materials->FindMaterial( "photos/photo_background", TEXTURE_GROUP_CLIENT_EFFECTS, false );
	pRenderContext->DrawScreenSpaceQuad( pPhotoBackground );

	if( cl_photo_disable_model_alpha_writes.GetBool() )
		pRenderContext->OverrideAlphaWriteEnable( true, false );

	if( bDrawEverything )
		DrawWorld( pRenderContext, 0.0f );

	DrawOpaqueRenderables( pRenderContext, RENDERABLES_RENDER_PATH_NORMAL, NULL );
	if( bDrawEverything )
	{
#if defined( PORTAL )
		DrawRecursivePortalViews();
#endif
		DrawTranslucentRenderables( false, false );
	}
	else
	{
		DrawTranslucentRenderablesNoWorld( false );
	}

	if( cl_photo_disable_model_alpha_writes.GetBool() )
		pRenderContext->OverrideAlphaWriteEnable( false, false );

	IMaterial *pPhotoForeground = materials->FindMaterial( "photos/photo_foreground", TEXTURE_GROUP_CLIENT_EFFECTS, false );
	pRenderContext->DrawScreenSpaceQuad( pPhotoForeground );

	m_DrawFlags = 0;
	render->PopView( pRenderContext, GetFrustum() );

	if( cl_blur_test.GetBool() )
		m_pTargetEntity->SetBlurState( true );
}
#endif


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CShadowDepthView::Setup( const CViewSetup &shadowViewIn, ITexture *pRenderTarget, ITexture *pDepthTexture, bool bRenderWorldAndObjects, bool bRenderViewModels )
{
	BaseClass::Setup( shadowViewIn );
	m_pRenderTarget = pRenderTarget;
	m_pDepthTexture = pDepthTexture;
	m_bRenderWorldAndObjects = bRenderWorldAndObjects;
	m_bRenderViewModels = bRenderViewModels;
}


bool DrawingShadowDepthView( void ) //for easy externing
{
	return (CurrentViewID() == VIEW_SHADOW_DEPTH_TEXTURE);
}

#ifdef _PS3
struct ShadowDepthStaticGeoCacheEntry_t
{
	ShadowDepthStaticGeoCacheEntry_t() { V_memset( this, 0, sizeof( *this ) ); }
	explicit ShadowDepthStaticGeoCacheEntry_t( const CViewSetup &viewSetup )
	{
		memset( this, 0, sizeof( *this ) );
		fov = viewSetup.fov;				
		origin = viewSetup.origin;
		angles = viewSetup.angles;
		zNear = viewSetup.zNear;
		zFar = viewSetup.zFar;			
	}

	// The fields from CViewSetup and ViewCustomVisibility_t that would actually affect the list
	float	fov;
	Vector	origin;
	QAngle	angles;
	float	zNear;
	float	zFar;
};
ConVar r_flashlight_staticgeocache( "r_flashlight_staticgeocache", "0", FCVAR_DEVELOPMENTONLY );
static ShadowDepthStaticGeoCacheEntry_t g_flashlight_staticgeo_cache;
static uint32 g_flashlight_staticgeo_cache_id;
static bool g_flashlight_staticgeo_cache_valid;
#endif
						  
//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CShadowDepthView::Draw()
{
	VPROF_BUDGET( "CShadowDepthView::Draw", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

	bool bRenderWorldAndObjects = true;
#if 0
	if ( ( m_bRenderViewModels ) && ( g_CascadeLightManager.GetCSMQualityMode() < CSMQUALITY_LOW ) )
	{
		bRenderWorldAndObjects = false;
	}
#endif

	// Start view
	unsigned int visFlags;

	BEGIN_2PASS_BUILD_BLOCK
	m_pMainView->SetupVis( (*this), visFlags );  // @MULTICORE (toml 8/9/2006): Portal problem, not sending custom vis down

#if defined(_PS3)
	g_viewBuilder.SetVisFlags( visFlags );
#endif
	END_2PASS_BLOCK

	CMatRenderContextPtr pRenderContext( materials );

	BEGIN_2PASS_DRAW_BLOCK
	pRenderContext->ClearColor3ub(0xFF, 0xFF, 0xFF);
	END_2PASS_BLOCK

#if defined( _X360 )
	pRenderContext->PushVertexShaderGPRAllocation( 112 ); //almost all work is done in vertex shaders for depth rendering, max out their threads
#endif

	if( IsPC() || IsPS3() )
	{
		int nClearFlags = ( g_viewBuilder.GetPassFlags() & PASS_DRAWLISTS ) ? VIEW_CLEAR_DEPTH : 0;
		render->Push3DView( pRenderContext, (*this), nClearFlags, m_pRenderTarget, GetFrustum(), m_pDepthTexture );
	}
	else if( IsX360() )
	{
		//for the 360, the dummy render target has a separate depth buffer which we Resolve() from afterward
		render->Push3DView( pRenderContext, (*this), VIEW_CLEAR_DEPTH, m_pRenderTarget, GetFrustum() );
	}

	SetupCurrentView( origin, angles, VIEW_SHADOW_DEPTH_TEXTURE );

	MDLCACHE_CRITICAL_SECTION();

	bool bFlashlightStaticGeoCacheValid = false;
	bool bFlashlightStaticGeoCacheEnabled = 0; bFlashlightStaticGeoCacheEnabled;
#if 0
	bool bFlashlightStaticGeoCacheEnabled = r_flashlight_staticgeocache.GetBool();
	if ( bFlashlightStaticGeoCacheEnabled )
	{
		if ( g_flashlight_staticgeo_cache_valid )
		{
			ShadowDepthStaticGeoCacheEntry_t entry( *this );
			bFlashlightStaticGeoCacheValid = !V_memcmp( &entry, &g_flashlight_staticgeo_cache, sizeof( entry ) );
			if ( !bFlashlightStaticGeoCacheValid )
			{
				if ( r_flashlight_staticgeocache.GetInt() > 1 )
				{
					DevMsg( "Shadow Depth View: depth cache is stale [id=%d]\n", g_flashlight_staticgeo_cache_id );
					DevMsg( "   pos   = %.3f:%.3f:%.3f -> %.3f:%.3f:%.3f\n",
						g_flashlight_staticgeo_cache.origin.x, g_flashlight_staticgeo_cache.origin.y, g_flashlight_staticgeo_cache.origin.z,
						entry.origin.x, entry.origin.y, entry.origin.z );
					DevMsg( "   ang   = %.3f:%.3f:%.3f -> %.3f:%.3f:%.3f\n",
						g_flashlight_staticgeo_cache.angles.x, g_flashlight_staticgeo_cache.angles.y, g_flashlight_staticgeo_cache.angles.z,
						entry.angles.x, entry.angles.y, entry.angles.z );
					DevMsg( "   fov   = %.3f -> %.3f\n", g_flashlight_staticgeo_cache.fov, entry.fov );
					DevMsg( "   zNear = %.3f -> %.3f\n", g_flashlight_staticgeo_cache.zNear, entry.zNear );
					DevMsg( "   zFar  = %.3f -> %.3f\n", g_flashlight_staticgeo_cache.zFar, entry.zFar );
				}
				V_memcpy( &g_flashlight_staticgeo_cache, &entry, sizeof( entry ) );
			}
		}
		if ( !bFlashlightStaticGeoCacheValid )
		{
			++ g_flashlight_staticgeo_cache_id;
			g_flashlight_staticgeo_cache_valid = true;
			if ( r_flashlight_staticgeocache.GetInt() > 1 )
			{
				DevMsg( "Shadow Depth View: fully refreshing depth cache [id=%d]\n", g_flashlight_staticgeo_cache_id );
			}
		}
	}
	else
	{
		g_flashlight_staticgeo_cache_valid = false;
	}
#endif

	bool bRenderWorld;
	// 7LS - turn off all world rendering in view model cascade, viewmodel and renderables only
	// ambient cube lighting should take care of gross shadow areas. 
	// TODO - find a way to turn off rendering of characters into viewmodel cascade too
	if( m_bRenderViewModels && !cl_csm_world_shadows_in_viewmodelcascade.GetBool() )
	{
		bRenderWorld = false;
	}
	else
	{
		bRenderWorld = m_bRenderWorldAndObjects && !m_bCSMView || ( cl_csm_world_shadows.GetBool() && cl_csm_shadows.GetBool() ) && bRenderWorldAndObjects;
	}

#if defined(_PS3)
	// turn off world rendering into all cascades for listen server
	if( m_bCSMView && engine->IsClientLocalToActiveServer() && r_ps3_csm_disableWorldInListenServer.GetInt() )
	{
		bRenderWorld = false;
	}
#endif

	BEGIN_2PASS_BUILD_BLOCK
	PROLOGUE_PASS_DRAWLISTS

	// cache volume culler
	g_viewBuilder.CacheBuildViewVolumeCuller( m_pCSMVolumeCuller );

	if ( ( m_bCSMView ) && ( bRenderWorld ) )
	{
		render->DrawTopView( true );
		render->TopViewNoBackfaceCulling( true );
		render->TopViewNoVisCheck( true ); 	//render->TopViewNoVisCheck( false ); <- consider as an optimisation for PS3/listenserver mode
		render->TopViewBounds( Vector2D( -30000.0f, -30000.0f ), Vector2D( 30000.0f, 30000.0f ) );
		if ( !cl_csm_disable_culling.GetBool() )
		{
			render->SetTopViewVolumeCuller( g_viewBuilder.GetBuildViewVolumeCuller() );
		}
	}

	{
		BuildWorldRenderLists( true, -1, true, true ); // @MULTICORE (toml 8/9/2006): Portal problem, not sending custom vis down
		g_viewBuilder.FlushBuildWorldListJob();
		if ( !m_bCSMView )
		{
			g_viewBuilder.AddDependencyToWorldList();
		}
		BuildRenderableRenderLists( CurrentViewID(), false, bFlashlightStaticGeoCacheValid );
	}
	
	if ( ( m_bCSMView ) && ( bRenderWorld ) )
	{
		render->TopViewNoBackfaceCulling( false );
		render->TopViewNoVisCheck( false );
		render->DrawTopView( false );
		render->SetTopViewVolumeCuller( NULL );
	}

	END_2PASS_BLOCK

	BEGIN_2PASS_DRAW_BLOCK
	EPILOGUE_PASS_DRAWLISTS

	engine->Sound_ExtraUpdate();	// Make sure sound doesn't stutter

	m_DrawFlags = m_pMainView->GetBaseDrawFlags() | DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER | DF_SHADOW_DEPTH_MAP;	// Don't draw water surface...

	if ( ( m_bCSMView ) && ( bRenderWorld ) )
	{
		render->DrawTopView( true );
	}
	SYNC_BUILDWORLD_JOB( true )
	if ( ( m_bCSMView ) && ( bRenderWorld ) )
	{
		render->DrawTopView( false );
	}

	if ( ( !bFlashlightStaticGeoCacheValid ) && ( bRenderWorld ) )
	{
		DrawWorld( pRenderContext, 0.0f );
	}

	// Draw opaque and translucent renderables with appropriate override materials
	// OVERRIDE_DEPTH_WRITE is OK with a NULL material pointer
	modelrender->ForcedMaterialOverride( NULL, OVERRIDE_DEPTH_WRITE );	

	SYNC_BUILDRENDERABLES_JOB( CurrentViewID() )
	if ( m_bRenderWorldAndObjects && bRenderWorldAndObjects )
	{
		DrawOpaqueRenderables( pRenderContext,
			#ifdef _PS3
			bFlashlightStaticGeoCacheEnabled
			? ( bFlashlightStaticGeoCacheValid
				? RENDERABLES_RENDER_PATH_SHADOWDEPTH_USE_GEOCACHE
				: RENDERABLES_RENDER_PATH_SHADOWDEPTH_BUILD_GEOCACHE
			) :
			#endif
			RENDERABLES_RENDER_PATH_SHADOWDEPTH_DEFAULT, 
			DEPTH_MODE_SHADOW,
			NULL
			);
	}

	if ( m_bRenderViewModels )
	{
		m_pMainView->DrawViewModelsShadowDepth( *this );
	}

	if ( m_bCSMView )
	{
		if ( cl_csm_translucent_shadows.GetBool() && ( g_CascadeLightManager.GetCSMQualityMode() >= CSMQUALITY_MEDIUM ) )
		{
			if ( cl_csm_translucent_shadows_using_opaque_path.GetBool() ) 
			{
				// cstrike15 supports efficiently rendering translucent materials into CSM shadow buffers
				DrawOpaqueRenderables( pRenderContext,
					RENDERABLES_RENDER_PATH_SHADOWDEPTH_DEFAULT, DEPTH_MODE_SHADOW, 
					NULL, RENDER_GROUP_TRANSLUCENT );
			}
			else
			{
				DrawTranslucentRenderables( false, true );
			}
		}
	}
	else
	{
#ifndef _PS3
		// Attention PaulB/Mario: We need to remove this PS3 specific thing for CS:GO CSM, so translucent renderables can cast shadows.
		// PS3 is not supporting translucent renderables for now, will need support in static geo cache
		if ( r_flashlightdepth_drawtranslucents.GetBool() )
		{
#if defined( PORTAL )
			DrawRecursivePortalViews(); //Dave K: probably does nothing in this view, calling it anyway because it's contents were directly inside DrawTranslucentRenderables() a moment ago and I don't want to risk breaking anything
#endif
			DrawTranslucentRenderables( false, true );
		}
#endif
	}

	modelrender->ForcedMaterialOverride( 0 );

	m_DrawFlags = 0;

#if defined(_X360)
	{
		//Resolve() the depth texture here. Before the pop so the copy will recognize that the resolutions are the same

		if( m_bCSMView )
		{
			// send appropriate src/dst rects for csm rendering
			Rect_t src, dst;

			src.x = 0;
			src.y = 0;
			src.width  = width;
			src.height = height;
			
			dst.x = xCsmDstOffset;
			dst.y = yCsmDstOffset;
			dst.width  = width;
			dst.height = height;

			pRenderContext->CopyRenderTargetToTextureEx( m_pDepthTexture, -1, &src, &dst );
		}
		else
		{
			pRenderContext->CopyRenderTargetToTextureEx( m_pDepthTexture, -1, NULL, NULL );
		}

	}
#endif

	END_2PASS_BLOCK


	render->PopView( pRenderContext, GetFrustum() );

#if defined( _X360 )
	pRenderContext->PopVertexShaderGPRAllocation();
#endif

	pRenderContext->ClearColor3ub( 0, 0, 0 );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CFreezeFrameView::Setup( const CViewSetup &shadowViewIn )
{
	BaseClass::Setup( shadowViewIn );

	VGui_GetTrueScreenSize( m_nScreenSize[ 0 ], m_nScreenSize[ 1 ] );
	VGui_GetPanelBounds( GET_ACTIVE_SPLITSCREEN_SLOT(), m_nSubRect[ 0 ], m_nSubRect[ 1 ], m_nSubRect[ 2 ], m_nSubRect[ 3 ] );

	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetString( "$basetexture", IsGameConsole() ? "_rt_FullFrameFB1" : "_rt_FullScreen" );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt( "$nofog", 1 );
	pVMTKeyValues->SetInt( "$ignorez", 1 );
	m_pFreezeFrame.Init( "FreezeFrame_FullScreen", TEXTURE_GROUP_OTHER, pVMTKeyValues );
	m_pFreezeFrame->Refresh();

	m_TranslucentSingleColor.Init( "debug/debugtranslucentsinglecolor", TEXTURE_GROUP_OTHER );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFreezeFrameView::Draw( void )
{
	CMatRenderContextPtr pRenderContext( materials );

#if defined( _X360 )
	pRenderContext->PushVertexShaderGPRAllocation( 16 ); //max out pixel shader threads
#endif

	pRenderContext->DrawScreenSpaceRectangle( m_pFreezeFrame, x, y, width, height,
		m_nSubRect[ 0 ], m_nSubRect[ 1 ], m_nSubRect[ 0 ] + m_nSubRect[ 2 ] - 1, m_nSubRect[ 1 ] + m_nSubRect[ 3 ] - 1, m_nScreenSize[ 0 ], m_nScreenSize[ 1 ] );

	//Fake a fade during freezeframe view.
	if ( g_flFreezeFlash[ m_nSlot ] >= gpGlobals->curtime && 
		engine->IsTakingScreenshot() == false )
	{
		// Overlay screen fade on entire screen
		IMaterial* pMaterial = m_TranslucentSingleColor;

		int iFadeAlpha = FREEZECAM_SNAPSHOT_FADE_SPEED * ( g_flFreezeFlash[ m_nSlot ] - gpGlobals->curtime );
		
		iFadeAlpha = MIN( iFadeAlpha, 255 );
		iFadeAlpha = MAX( 0, iFadeAlpha );
		
		pMaterial->AlphaModulate( iFadeAlpha * ( 1.0f / 255.0f ) );
		pMaterial->ColorModulate( 1.0f,	1.0f, 1.0f );
		pMaterial->SetMaterialVarFlag( MATERIAL_VAR_IGNOREZ, true );

		pRenderContext->DrawScreenSpaceRectangle( pMaterial, x, y, width, height, m_nSubRect[ 0 ], m_nSubRect[ 1 ], m_nSubRect[ 0 ] + m_nSubRect[ 2 ] - 1, m_nSubRect[ 1 ] + m_nSubRect[ 3 ] - 1, m_nScreenSize[ 0 ], m_nScreenSize[ 1 ] );
	}

#if defined( _X360 )
	pRenderContext->PopVertexShaderGPRAllocation();
#endif
}


//-----------------------------------------------------------------------------
// Pops a water render target
//-----------------------------------------------------------------------------
bool CBaseWorldView::AdjustView( float waterHeight )
{
	if( m_DrawFlags & DF_RENDER_REFRACTION )
	{
		ITexture *pTexture = GetWaterRefractionTexture();

		// Use the aspect ratio of the main view! So, don't recompute it here
		x = y = 0;
		width = pTexture->GetActualWidth();
		height = pTexture->GetActualHeight();

		return true;
	}

	if( m_DrawFlags & DF_RENDER_REFLECTION )
	{
		ITexture *pTexture = GetWaterReflectionTexture();

		// Use the aspect ratio of the main view! So, don't recompute it here
		x = y = 0;
		width = pTexture->GetActualWidth();
		height = pTexture->GetActualHeight();

		float fHeightDiff = (origin[2] - waterHeight) * 2.0f;

		angles[0] = -angles[0];
		angles[2] = -angles[2];
		origin[2] -= fHeightDiff;

		// PORTAL2-specific code
		if( m_bCustomViewMatrix )
		{
			QAngle newAngles;
			Vector vNewOrigin;
			VMatrix customMatrix;

			// Recompute angles from custom view matrix (which is concatenation of view matrix and portal matrix)
			customMatrix.CopyFrom3x4( m_matCustomViewMatrix );
			customMatrix = customMatrix.Transpose();
			MatrixToAngles( customMatrix, newAngles );
			
			// Apply reflection transformation
			newAngles[0] = -newAngles[0];
			newAngles[2] = -newAngles[2];

			// Extract origin from matrix (coordinates in matrix are in view space; convert back to world space)
			vNewOrigin = -( 
				m_matCustomViewMatrix.m_flMatVal[0][3] * Vector( *( Vector * )m_matCustomViewMatrix.m_flMatVal[0] ) +
				m_matCustomViewMatrix.m_flMatVal[1][3] * Vector( *( Vector * )m_matCustomViewMatrix.m_flMatVal[1] ) +
				m_matCustomViewMatrix.m_flMatVal[2][3] * Vector( *( Vector * )m_matCustomViewMatrix.m_flMatVal[2] ) 
				);

			// Reflect position beneath water plane
			vNewOrigin[2] -= (vNewOrigin[2] - waterHeight) * 2.0f;

			VMatrix newCustomMatrix;

			newCustomMatrix.Identity();

			// Re-generate the custom view matrix from angles & origin
			MatrixRotate( newCustomMatrix, Vector( 1, 0, 0 ), -newAngles[2] );
			MatrixRotate( newCustomMatrix, Vector( 0, 1, 0 ), -newAngles[0] );
			MatrixRotate( newCustomMatrix, Vector( 0, 0, 1 ), -newAngles[1] );
			MatrixTranslate( newCustomMatrix, -vNewOrigin );

			m_matCustomViewMatrix = newCustomMatrix.As3x4();
		}

		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Pops a water render target
//-----------------------------------------------------------------------------
void CBaseWorldView::PushView( float waterHeight )
{
	float spread = 2.0f;
	if( m_DrawFlags & DF_FUDGE_UP )
	{
		waterHeight += spread;
	}
	else
	{
		waterHeight -= spread;
	}

	MaterialHeightClipMode_t clipMode = MATERIAL_HEIGHTCLIPMODE_DISABLE;
	if ( ( m_DrawFlags & DF_CLIP_Z ) && mat_clipz.GetBool() )
	{
		if( m_DrawFlags & DF_CLIP_BELOW )
		{
			clipMode = MATERIAL_HEIGHTCLIPMODE_RENDER_ABOVE_HEIGHT;
		}
		else
		{
			clipMode = MATERIAL_HEIGHTCLIPMODE_RENDER_BELOW_HEIGHT;
		}
	}

	CMatRenderContextPtr pRenderContext( materials );

	if( m_DrawFlags & DF_RENDER_REFRACTION )
	{
		pRenderContext->SetFogZ( waterHeight );
		pRenderContext->SetHeightClipZ( waterHeight );
		pRenderContext->SetHeightClipMode( clipMode );

		// Have to re-set up the view since we reset the size
		render->Push3DView( pRenderContext, *this, m_ClearFlags, GetWaterRefractionTexture(), GetFrustum() );

		return;
	}

	if( m_DrawFlags & DF_RENDER_REFLECTION )
	{
		ITexture *pTexture = GetWaterReflectionTexture();

		pRenderContext->SetFogZ( waterHeight );

		bool bSoftwareUserClipPlane = g_pMaterialSystemHardwareConfig->UseFastClipping();
		if( bSoftwareUserClipPlane && ( origin[2] > waterHeight - r_eyewaterepsilon.GetFloat() ) )
		{
			waterHeight = origin[2] + r_eyewaterepsilon.GetFloat();
		}

		pRenderContext->SetHeightClipZ( waterHeight );
		pRenderContext->SetHeightClipMode( clipMode );

		render->Push3DView( pRenderContext, *this, m_ClearFlags, pTexture, GetFrustum() );

		SetLightmapScaleForWater();
		return;
	}

	if ( m_ClearFlags & ( VIEW_CLEAR_DEPTH | VIEW_CLEAR_COLOR | VIEW_CLEAR_STENCIL ) )
	{
		if ( m_ClearFlags & VIEW_CLEAR_OBEY_STENCIL )
		{
			pRenderContext->ClearBuffersObeyStencil( ( m_ClearFlags & VIEW_CLEAR_COLOR ) ? true : false, ( m_ClearFlags & VIEW_CLEAR_DEPTH ) ? true : false );
		}
		else
		{
			if ( r_shadow_deferred.GetBool() )
			{
				// FIXME: Is there a better place to force a stencil clear for deferred shadows?
				if ( m_ClearFlags & VIEW_CLEAR_DEPTH )
					m_ClearFlags |= VIEW_CLEAR_STENCIL;
			}

			pRenderContext->ClearBuffers( ( m_ClearFlags & VIEW_CLEAR_COLOR ) ? true : false, ( m_ClearFlags & VIEW_CLEAR_DEPTH ) ? true : false, ( m_ClearFlags & VIEW_CLEAR_STENCIL ) ? true : false );
		}
	}

	pRenderContext->SetHeightClipMode( clipMode );
	if ( clipMode != MATERIAL_HEIGHTCLIPMODE_DISABLE )
	{   
		pRenderContext->SetHeightClipZ( waterHeight );
	}
}


//-----------------------------------------------------------------------------
// Pops a water render target
//-----------------------------------------------------------------------------
void CBaseWorldView::PopView()
{
	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->SetHeightClipMode( MATERIAL_HEIGHTCLIPMODE_DISABLE );
	if( m_DrawFlags & (DF_RENDER_REFRACTION | DF_RENDER_REFLECTION) )
	{
		if ( IsGameConsole() )
		{
			// these renders paths used their surfaces, so blit their results
			if ( m_DrawFlags & DF_RENDER_REFRACTION )
			{
				pRenderContext->CopyRenderTargetToTextureEx( GetWaterRefractionTexture(), NULL, NULL );
			}
			if ( m_DrawFlags & DF_RENDER_REFLECTION )
			{
				pRenderContext->CopyRenderTargetToTextureEx( GetWaterReflectionTexture(), NULL, NULL );
			}
		}

		render->PopView( pRenderContext, GetFrustum() );
		if (s_vSavedLinearLightMapScale.x>=0)
		{
			pRenderContext->SetToneMappingScaleLinear(s_vSavedLinearLightMapScale);
			s_vSavedLinearLightMapScale.x=-1;
		}
	}
}


//-----------------------------------------------------------------------------
// Draws the world + entities
//-----------------------------------------------------------------------------
void CBaseWorldView::DrawSetup( IMatRenderContext *pRenderContext, float waterHeight, int nSetupFlags, float waterZAdjust, int iForceViewLeaf )
{
	VPROF( "CBaseWorldView::DrawSetup" );

	int savedViewID = g_CurrentViewID;
	g_CurrentViewID = VIEW_ILLEGAL;

	bool bViewChanged = AdjustView( waterHeight );

	if ( bViewChanged )
	{
		render->Push3DView( pRenderContext, *this, 0, NULL, GetFrustum() );
	}

	BEGIN_2PASS_BUILD_BLOCK

	if ( ( nSetupFlags & ( DF_DRAW_SIMPLE_WORLD_MODEL | DF_DRAW_SIMPLE_WORLD_MODEL_WATER ) ) == 0 )
	{
		render->BeginUpdateLightmaps();

		bool bDrawEntities = ( nSetupFlags & DF_DRAW_ENTITITES ) != 0;
		bool bDrawReflection = ( nSetupFlags & DF_RENDER_REFLECTION ) != 0;
		bool bFastEntityRendering = ( nSetupFlags & DF_FAST_ENTITY_RENDERING ) != 0;
		BuildWorldRenderLists( bDrawEntities, iForceViewLeaf, true, false, bDrawReflection ? &waterHeight : NULL );

		PruneWorldListInfo();
		g_viewBuilder.FlushBuildWorldListJob(); // PruneWorldListInfo part of the BuildWorldList job
		g_viewBuilder.AddDependencyToWorldList();

		if ( bDrawEntities )
		{
			BuildRenderableRenderLists( savedViewID, bFastEntityRendering );
		}
		else
		{
			// DrawExecute still expect a valid m_pRenderables (empty)
			m_pRenderables = new CClientRenderablesList; 
			g_viewBuilder.SetRenderablesListElement( m_pRenderables );
		}

		render->EndUpdateLightmaps();
	}
	else 
	{
		bool bDrawEntities = ( nSetupFlags & DF_DRAW_ENTITITES ) != 0;
		bool bFastEntityRendering = ( nSetupFlags & DF_FAST_ENTITY_RENDERING ) != 0;
		// We require fast water reflections here since the other code path assumes that world lists are built, etc.
		if ( bDrawEntities && bFastEntityRendering )
		{
			// Make sure to cache frustum list as BuildRenderableRenderLists will used the cached frustum
			// (Frustum normally cached in BuildWorldRenderLists!)
			g_viewBuilder.CacheFrustumData();
			BuildRenderableRenderLists( savedViewID, bFastEntityRendering );
		}
		else
		{
			Error( "Bad stuff will happen (crashes, stack corruption, etc) because opaque renderables will attempt to render with junk data" );
		}
	}	

	END_2PASS_BLOCK

	if ( bViewChanged )
	{
		render->PopView( pRenderContext, GetFrustum() );
	}


	g_CurrentViewID = savedViewID;
}


void CBaseWorldView::DrawExecute( float waterHeight, view_id_t viewID, float waterZAdjust )
{
	BEGIN_2PASS_DRAW_BLOCK
	EPILOGUE_PASS_DRAWLISTS

	// Make sure sound doesn't stutter
	engine->Sound_ExtraUpdate();

	static ConVarRef mat_resolveFullFrameDepth( "mat_resolveFullFrameDepth" );

	// perform full cpu depth only pass into rt_fullframedepth - only do this if forced on via mat_resolveFullFrameDepth = 2, useful for compatability or debugging vs depth resolve
	if ( ( viewID == VIEW_MAIN ) && g_pMaterialSystemHardwareConfig->HasFullResolutionDepthTexture() && ( mat_resolveFullFrameDepth.GetInt() == 2 ) )
	{
		SSAO_DepthPass();
	}

	SYNC_BUILDWORLD_JOB(false)

	// @MULTICORE (toml 8/16/2006): rethink how, where, and when this is done...
	if ( !( m_DrawFlags & ( DF_DRAW_SIMPLE_WORLD_MODEL | DF_DRAW_SIMPLE_WORLD_MODEL_WATER ) ) )
	{
#if defined(_PS3)
		g_pClientShadowMgr->ComputeShadowTextures( *this, m_pWorldListInfo[ g_viewBuilder.GetBuildViewID() ]->m_LeafCount, m_pWorldListInfo[ g_viewBuilder.GetBuildViewID() ]->m_pLeafDataList );
#else
		g_pClientShadowMgr->ComputeShadowTextures( *this, m_pWorldListInfo->m_LeafCount, m_pWorldListInfo->m_pLeafDataList );
#endif
	}

	int savedViewID = g_CurrentViewID;
	g_CurrentViewID = viewID;

	// Update our render view flags.
	int iDrawFlagsBackup = m_DrawFlags;
	m_DrawFlags |= m_pMainView->GetBaseDrawFlags();

	PushView( waterHeight );

	CMatRenderContextPtr pRenderContext( materials );

#if defined( _X360 )
	pRenderContext->PushVertexShaderGPRAllocation( 32 );
#endif

	ITexture *pSaveFrameBufferCopyTexture = pRenderContext->GetFrameBufferCopyTexture( 0 );
	pRenderContext->SetFrameBufferCopyTexture( GetPowerOfTwoFrameBufferTexture() );

	if ( !( m_DrawFlags & ( DF_DRAW_SIMPLE_WORLD_MODEL | DF_DRAW_SIMPLE_WORLD_MODEL_WATER ) ) )
	{
		BeginConsoleZPass();
	}

#ifdef PORTAL
	if ( IsMainView( viewID ) )
	{
		g_pPortalRender->DrawEarlyZPortals( (CViewRender*)view );
	}
#endif // PORTAL

//#if !defined(_PS3) - trying to re-order drawing of world and renderables on PS3 (small gpu perf win, but now confuses/delays the syncing of SPU buildworld/renderable jobs)
	m_DrawFlags |= DF_SKIP_WORLD_DECALS_AND_OVERLAYS;
	DrawWorld( pRenderContext, waterZAdjust );
//#endif

	SYNC_BUILDRENDERABLES_JOB( savedViewID )

#if defined(_PS3)
	CUtlVector< CClientRenderablesList::CEntry * > arrFastClippedOpaqueRenderables( (CClientRenderablesList::CEntry **)stackalloc( m_pRenderablesList[ g_viewBuilder.GetBuildViewID() ]->m_RenderGroupCounts[RENDER_GROUP_OPAQUE] * sizeof( CClientRenderablesList::CEntry * ) ), m_pRenderablesList[ g_viewBuilder.GetBuildViewID() ]->m_RenderGroupCounts[RENDER_GROUP_OPAQUE] );
#else
	CUtlVector< CClientRenderablesList::CEntry * > arrFastClippedOpaqueRenderables( (CClientRenderablesList::CEntry **)stackalloc( m_pRenderables->m_RenderGroupCounts[RENDER_GROUP_OPAQUE] * sizeof( CClientRenderablesList::CEntry * ) ), m_pRenderables->m_RenderGroupCounts[RENDER_GROUP_OPAQUE] );
#endif
	CUtlVector< CClientRenderablesList::CEntry * > *pArrFastClippedOpaqueRenderables = (r_deferopaquefastclipped.GetBool() && !m_bDrawWorldNormal && r_entityclips.GetBool() && materials->UsingFastClipping()) ? &arrFastClippedOpaqueRenderables : NULL;

	m_DrawFlags &= ~DF_SKIP_WORLD_DECALS_AND_OVERLAYS;
	if ( m_DrawFlags & DF_DRAW_ENTITITES )
	{
		DrawOpaqueRenderables( pRenderContext, RENDERABLES_RENDER_PATH_NORMAL, DEPTH_MODE_NORMAL, pArrFastClippedOpaqueRenderables );
	}

// #if defined(_PS3)  - trying to re-order drawing of world and renderables on PS3 (small gpu perf win, but now confuses/delays the syncing of SPU buildworld/renderable jobs)
// 	m_DrawFlags |= DF_SKIP_WORLD_DECALS_AND_OVERLAYS;
// 	DrawWorld( pRenderContext, waterZAdjust );
// 	m_DrawFlags &= ~DF_SKIP_WORLD_DECALS_AND_OVERLAYS;
// #endif

	if ( !( m_DrawFlags & ( DF_DRAW_SIMPLE_WORLD_MODEL | DF_DRAW_SIMPLE_WORLD_MODEL_WATER ) ) )
	{
		EndConsoleZPass();		// DrawOpaqueRenderables currently already calls EndConsoleZPass. No harm in calling it again to make sure we're always ending it
	}

	if ( m_DrawFlags & DF_RENDER_PSEUDO_TRANSLUCENT_WATER )
	{
		if ( IsX360() )
		{
			// Update depth texture for depth-based water edge feathering.
			UpdateFullScreenDepthTexture();
		}

		int nOldFlags = m_DrawFlags;

		// In addition to decals, draw only water but not above/below water (we need to un-skip the world for this to have any effect)
		m_DrawFlags &= ~DF_RENDER_ABOVEWATER;
		m_DrawFlags &= ~DF_RENDER_UNDERWATER;
		m_DrawFlags &= ~DF_DRAWSKYBOX;
		m_DrawFlags |= DF_RENDER_WATER;
		m_DrawFlags |= DF_SKIP_WORLD_DECALS_AND_OVERLAYS;
		
		DrawWorld( pRenderContext, waterZAdjust );
		
		m_DrawFlags = nOldFlags;
	}

	// Only draw decals on opaque surfaces after now. Benefit is two-fold: Early Z benefits on PC, and
	// we're pulling out stuff that uses the dynamic VB from the 360 Z pass
	// (which can lead to rendering corruption if we overflow the dyn. VB ring buffer).
	m_DrawFlags |= DF_SKIP_WORLD;
	if ( !( m_DrawFlags & ( DF_DRAW_SIMPLE_WORLD_MODEL | DF_DRAW_SIMPLE_WORLD_MODEL_WATER ) ) )
	{
		DrawWorld( pRenderContext, waterZAdjust );
	}
	m_DrawFlags &= ~DF_SKIP_WORLD;
		
	if ( !m_bDrawWorldNormal )
	{
		if ( m_DrawFlags & DF_DRAW_ENTITITES )
		{
#if defined( PORTAL )
			DrawRecursivePortalViews();
#endif
			DrawDeferredClippedOpaqueRenderables( pRenderContext, RENDERABLES_RENDER_PATH_NORMAL, DEPTH_MODE_NORMAL, pArrFastClippedOpaqueRenderables );
			DrawTranslucentRenderables( false, false );
			DrawNoZBufferTranslucentRenderables();
		}
		else
		{
			// Draw translucent world brushes only, no entities
			DrawTranslucentWorldInLeaves( pRenderContext, false );
		}
	}

	// issue the pixel visibility tests for sub-views
	if ( !IsMainView( CurrentViewID() ) && CurrentViewID() != VIEW_INTRO_CAMERA )
	{
		PixelVisibility_EndCurrentView();
	}

	pRenderContext.GetFrom( materials );
	pRenderContext->SetFrameBufferCopyTexture( pSaveFrameBufferCopyTexture );
	PopView();

	m_DrawFlags = iDrawFlagsBackup;

	g_CurrentViewID = savedViewID;

#if defined( _X360 )
	pRenderContext->PopVertexShaderGPRAllocation();
#endif

	END_2PASS_BLOCK
}



void CBaseWorldView::SSAO_DepthPass()
{
	VPROF_BUDGET( "CSimpleWorldView::SSAO_DepthPass", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

	int savedViewID = g_CurrentViewID;
	g_CurrentViewID = VIEW_SSAO;

	SYNC_BUILDWORLD_JOB( false );

	SYNC_BUILDRENDERABLES_JOB( VIEW_MAIN );
	
	ITexture *pSSAO = materials->FindTexture( "_rt_FullFrameDepth", TEXTURE_GROUP_RENDER_TARGET );

	CMatRenderContextPtr pRenderContext( materials );

	PIXEVENT( pRenderContext, "Depth SSAO" );

	pRenderContext->ClearColor4ub( 255, 255, 255, 255 );

//	pRenderContext.SafeRelease();

	if ( IsPC() )
	{
		render->Push3DView( pRenderContext, ( *this ), VIEW_CLEAR_DEPTH | VIEW_CLEAR_COLOR, pSSAO, GetFrustum() );
	}
	else if ( IsX360() )
	{
		render->Push3DView( pRenderContext, ( *this ), VIEW_CLEAR_DEPTH | VIEW_CLEAR_COLOR, pSSAO, GetFrustum() );
	}

	MDLCACHE_CRITICAL_SECTION();

	engine->Sound_ExtraUpdate();	// Make sure sound doesn't stutter

	m_DrawFlags |= DF_SSAO_DEPTH_PASS;

	{
		VPROF_BUDGET( "DrawWorld", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );
		DrawWorld( pRenderContext, 0.0f );
	}

	// Draw opaque and translucent renderables with appropriate override materials
	// OVERRIDE_SSAO_DEPTH_WRITE is OK with a NULL material pointer
	modelrender->ForcedMaterialOverride( NULL, OVERRIDE_SSAO_DEPTH_WRITE );

	{
		VPROF_BUDGET( "DrawOpaqueRenderables", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );
		//DrawOpaqueRenderables( pRenderContext, DEPTH_MODE_SSA0 );

		DrawOpaqueRenderables( pRenderContext, RENDERABLES_RENDER_PATH_NORMAL, DEPTH_MODE_SSA0, NULL );

	}

#if 0
	if ( m_bRenderFlashlightDepthTranslucents || r_flashlightdepth_drawtranslucents.GetBool() )
	{
		VPROF_BUDGET( "DrawTranslucentRenderables", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );
		DrawTranslucentRenderables( false, true );
	}
#endif

	modelrender->ForcedMaterialOverride( 0 );

	m_DrawFlags &= ~DF_SSAO_DEPTH_PASS;

//	pRenderContext.GetFrom( materials );

	if ( IsX360() )
	{
		//Resolve() the depth texture here. Before the pop so the copy will recognize that the resolutions are the same
		pRenderContext->CopyRenderTargetToTextureEx( NULL, -1, NULL, NULL );
	}

	render->PopView( pRenderContext, GetFrustum() );

	pRenderContext.SafeRelease();

	g_CurrentViewID = savedViewID;
}





//-----------------------------------------------------------------------------
// Draws the scene when there's no water or only cheap water
//-----------------------------------------------------------------------------
void CSimpleWorldView::Setup( const CViewSetup &view, int nClearFlags, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t &waterInfo, ViewCustomVisibility_t *pCustomVisibility )
{
	BaseClass::Setup( view );

	m_ClearFlags = nClearFlags;
	m_DrawFlags = DF_DRAW_ENTITITES;

	if ( !waterInfo.m_bOpaqueWater )
	{
		m_DrawFlags |= DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER;
	}
	else
	{
		bool bViewIntersectsWater = DoesViewPlaneIntersectWater( fogInfo.m_flWaterHeight, fogInfo.m_nVisibleFogVolume );
		if( bViewIntersectsWater )
		{
			// have to draw both sides if we can see both.
			m_DrawFlags |= DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER;
		}
		else if ( fogInfo.m_bEyeInFogVolume )
		{
			m_DrawFlags |= DF_RENDER_UNDERWATER;
		}
		else
		{
			m_DrawFlags |= DF_RENDER_ABOVEWATER;
		}
	}
	if ( waterInfo.m_bDrawWaterSurface )
	{
		if ( waterInfo.m_bPseudoTranslucentWater )
		{
			m_DrawFlags |= DF_RENDER_PSEUDO_TRANSLUCENT_WATER;
		}
		else
		{
			m_DrawFlags |= DF_RENDER_WATER;
		}
	}

	if ( !fogInfo.m_bEyeInFogVolume && bDrawSkybox )
	{
		m_DrawFlags |= DF_DRAWSKYBOX;
	}

#if defined( PORTAL2 )
	m_DrawFlags |= ComputeSimpleWorldModelDrawFlags();
#endif // PORTAL2

	m_pCustomVisibility = pCustomVisibility;
	m_fogInfo = fogInfo;
}


//-----------------------------------------------------------------------------
// Draws the scene when there's no water or only cheap water
//-----------------------------------------------------------------------------
void CSimpleWorldView::Draw()
{
	VPROF( "CViewRender::ViewDrawScene_NoWater" );

	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "CSimpleWorldView::Draw" );

#if defined( _X360 )
	pRenderContext->PushVertexShaderGPRAllocation( 32 ); //lean toward pixel shader threads
#endif

	PROLOGUE_PASS_DRAWLISTS
	DrawSetup( pRenderContext, 0, m_DrawFlags, 0 );


	pRenderContext.SafeRelease();

	if ( !m_fogInfo.m_bEyeInFogVolume )
	{
		EnableWorldFog();
	}
	else
	{
		m_ClearFlags |= VIEW_CLEAR_COLOR;

		SetFogVolumeState( m_fogInfo, false );

		pRenderContext.GetFrom( materials );

		unsigned char ucFogColor[3];
		pRenderContext->GetFogColor( ucFogColor );
		pRenderContext->ClearColor4ub( ucFogColor[0], ucFogColor[1], ucFogColor[2], 255 );
	}

	pRenderContext.SafeRelease();

	DrawExecute( 0, CurrentViewID(), 0 );

	pRenderContext.GetFrom( materials );
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );

#if defined( _X360 )
	pRenderContext->PopVertexShaderGPRAllocation();
#endif


}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CBaseWaterView::CalcWaterEyeAdjustments( const VisibleFogVolumeInfo_t &fogInfo,
											 float &newWaterHeight, float &waterZAdjust, bool bSoftwareUserClipPlane )
{
	if( !bSoftwareUserClipPlane )
	{
		newWaterHeight = fogInfo.m_flWaterHeight;
		waterZAdjust = 0.0f;
		return;
	}

	newWaterHeight = fogInfo.m_flWaterHeight;
	float eyeToWaterZDelta = origin[2] - fogInfo.m_flWaterHeight;
	float epsilon = r_eyewaterepsilon.GetFloat();
	waterZAdjust = 0.0f;
	if( fabs( eyeToWaterZDelta ) < epsilon )
	{
		if( eyeToWaterZDelta > 0 )
		{
			newWaterHeight = origin[2] - epsilon;
		}
		else
		{
			newWaterHeight = origin[2] + epsilon;
		}
		waterZAdjust = newWaterHeight - fogInfo.m_flWaterHeight;
	}

	//	Warning( "view.origin[2]: %f newWaterHeight: %f fogInfo.m_flWaterHeight: %f waterZAdjust: %f\n", 
	//		( float )view.origin[2], newWaterHeight, fogInfo.m_flWaterHeight, waterZAdjust );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CBaseWaterView::CSoftwareIntersectionView::Setup( bool bAboveWater )
{
	BaseClass::Setup( *GetOuter() );

	m_DrawFlags = 0;
	m_DrawFlags = ( bAboveWater ) ? DF_RENDER_UNDERWATER : DF_RENDER_ABOVEWATER;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CBaseWaterView::CSoftwareIntersectionView::Draw()
{
	CMatRenderContextPtr pRenderContext( materials );

	PROLOGUE_PASS_DRAWLISTS
	DrawSetup( pRenderContext, GetOuter()->m_waterHeight, m_DrawFlags, GetOuter()->m_waterZAdjust );

	DrawExecute( GetOuter()->m_waterHeight, CurrentViewID(), GetOuter()->m_waterZAdjust );
}

//-----------------------------------------------------------------------------
// Draws the scene when the view point is above the level of the water
//-----------------------------------------------------------------------------
void CAboveWaterView::Setup( const CViewSetup &view, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t& waterInfo, ViewCustomVisibility_t *pCustomVisibility )
{
	BaseClass::Setup( view );

	m_bSoftwareUserClipPlane = g_pMaterialSystemHardwareConfig->UseFastClipping();

	CalcWaterEyeAdjustments( fogInfo, m_waterHeight, m_waterZAdjust, m_bSoftwareUserClipPlane );

	// BROKEN STUFF!
	if ( m_waterZAdjust == 0.0f )
	{
		m_bSoftwareUserClipPlane = false;
	}

	m_DrawFlags = DF_RENDER_ABOVEWATER | DF_DRAW_ENTITITES;
	
#ifdef PORTAL
	if ( g_pPortalRender->GetViewRecursionLevel() == 0 )
#endif
	{
		m_ClearFlags = VIEW_CLEAR_DEPTH;
	}
	

#ifdef PORTAL
	if( g_pPortalRender->ShouldObeyStencilForClears() )
		m_ClearFlags |= VIEW_CLEAR_OBEY_STENCIL;
#endif

	if ( bDrawSkybox )
	{
		m_DrawFlags |= DF_DRAWSKYBOX;
	}

	if ( waterInfo.m_bDrawWaterSurface )
	{
		if ( waterInfo.m_bPseudoTranslucentWater )
		{
			m_DrawFlags |= DF_RENDER_PSEUDO_TRANSLUCENT_WATER;
		}
		else
		{
			m_DrawFlags |= DF_RENDER_WATER;
		}
	}
	if ( !waterInfo.m_bRefract && !waterInfo.m_bOpaqueWater )
	{
		m_DrawFlags |= DF_RENDER_UNDERWATER;
	}

#if defined( PORTAL2 )
	m_DrawFlags |= ComputeSimpleWorldModelDrawFlags();
#endif // PORTAL2

	m_fogInfo = fogInfo;
	m_waterInfo = waterInfo;
	m_pCustomVisibility = pCustomVisibility;
}

		 
//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAboveWaterView::Draw()
{
	VPROF( "CViewRender::ViewDrawScene_EyeAboveWater" );

	// eye is outside of water
	
	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "CAboveWaterView::Draw" );
	
	// render the reflection
	if( m_waterInfo.m_bReflect )
	{
		m_ReflectionView.Setup( m_waterInfo.m_bReflectEntities, m_waterInfo.m_bReflectOnlyMarkedEntities, m_waterInfo.m_bReflect2DSkybox );
		m_pMainView->AddViewToScene( &m_ReflectionView );
	}
	
	bool bViewIntersectsWater = false;

	// render refraction
	if ( m_waterInfo.m_bRefract )
	{
		m_RefractionView.Setup();
		m_pMainView->AddViewToScene( &m_RefractionView );

		if( !m_bSoftwareUserClipPlane )
		{
			bViewIntersectsWater = DoesViewPlaneIntersectWater( m_fogInfo.m_flWaterHeight, m_fogInfo.m_nVisibleFogVolume );
		}
	}	

#ifdef PORTAL
	if( g_pPortalRender->ShouldObeyStencilForClears() )
		m_ClearFlags |= VIEW_CLEAR_OBEY_STENCIL;
#endif

	// NOTE!!!!!  YOU CAN ONLY DO THIS IF YOU HAVE HARDWARE USER CLIP PLANES!!!!!!
	bool bHardwareUserClipPlanes = !g_pMaterialSystemHardwareConfig->UseFastClipping();
	if( bViewIntersectsWater && bHardwareUserClipPlanes )
	{
		// This is necessary to keep the non-water fogged world from drawing underwater in 
		// the case where we want to partially see into the water.
		m_DrawFlags |= DF_CLIP_Z | DF_CLIP_BELOW;
	}

	PROLOGUE_PASS_DRAWLISTS
	// render the world
	DrawSetup( pRenderContext, m_waterHeight, m_DrawFlags, m_waterZAdjust );

	EnableWorldFog();

	DrawExecute( m_waterHeight, CurrentViewID(), m_waterZAdjust );

	if ( m_waterInfo.m_bRefract )
	{
		if ( m_bSoftwareUserClipPlane )
		{
			m_SoftwareIntersectionView.Setup( true );
			m_pMainView->AddViewToScene( &m_SoftwareIntersectionView );
		}
		else if ( bViewIntersectsWater )
		{
			m_IntersectionView.Setup();
			m_pMainView->AddViewToScene( &m_IntersectionView );
		}
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAboveWaterView::CReflectionView::Setup( bool bReflectEntities, bool bReflectOnlyMarkedEntities, bool bReflect2DSkybox )
{
	BaseClass::Setup( *GetOuter() );

	m_ClearFlags = VIEW_CLEAR_DEPTH;

	m_DrawFlags = DF_RENDER_REFLECTION | DF_CLIP_Z | DF_CLIP_BELOW | 
		DF_RENDER_ABOVEWATER;

	if ( bReflect2DSkybox )
	{
		m_DrawFlags |= DF_DRAWSKYBOX;
	}
	else
	{
		m_ClearFlags |= VIEW_CLEAR_COLOR;
	}

	bool bSimpleWorldModeWaterReflection;
	int nSimpleWorldModelRecursionLevel;
	float flSimpleWorldModelDrawBeyondDistance;
	GetSimpleWorldModelConfiguration( bSimpleWorldModeWaterReflection, nSimpleWorldModelRecursionLevel, flSimpleWorldModelDrawBeyondDistance );

	if ( bSimpleWorldModeWaterReflection )
	{
		m_DrawFlags |= DF_DRAW_SIMPLE_WORLD_MODEL | DF_FAST_ENTITY_RENDERING | DF_DRAW_ENTITITES;
	}
	else
	{
		if( bReflectEntities )
		{
			Assert( !bReflectOnlyMarkedEntities );
			m_DrawFlags |= DF_DRAW_ENTITITES;
		}
		else if ( bReflectOnlyMarkedEntities )
		{
			m_DrawFlags |= DF_FAST_ENTITY_RENDERING | DF_DRAW_ENTITITES;
		}
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAboveWaterView::CReflectionView::Draw()
{

#ifdef PORTAL
	g_pPortalRender->WaterRenderingHandler_PreReflection();
#endif

	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "CAboveWaterView::CReflectionView" );

	g_CascadeLightManager.BeginReflectionView();

	// Store off view origin and angles and set the new view
	int nSaveViewID = CurrentViewID();
	Vector vecOldOrigin = CurrentViewOrigin();
	QAngle vecOldAngles = CurrentViewAngles();
	SetupCurrentView( origin, angles, VIEW_REFLECTION );

	// Disable occlusion visualization in reflection
	bool bVisOcclusion = r_visocclusion.GetBool();
	r_visocclusion.SetValue( 0 );

	PROLOGUE_PASS_DRAWLISTS
	DrawSetup( pRenderContext, GetOuter()->m_fogInfo.m_flWaterHeight, m_DrawFlags, 0.0f, GetOuter()->m_fogInfo.m_nVisibleFogVolumeLeaf );

	BEGIN_2PASS_DRAW_BLOCK
	EnableWorldFog();
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
	END_2PASS_BLOCK

	DrawExecute( GetOuter()->m_fogInfo.m_flWaterHeight, VIEW_REFLECTION, 0.0f );

	r_visocclusion.SetValue( bVisOcclusion );
	
#ifdef PORTAL
	// deal with stencil
	g_pPortalRender->WaterRenderingHandler_PostReflection();
#endif

	// finish off the view and restore the previous view.
	SetupCurrentView( vecOldOrigin, vecOldAngles, ( view_id_t )nSaveViewID );

	g_CascadeLightManager.EndReflectionView();

	// This is here for multithreading
	pRenderContext->Flush();

}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAboveWaterView::CRefractionView::Setup()
{
	BaseClass::Setup( *GetOuter() );

	m_ClearFlags = VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH;

	m_DrawFlags = DF_RENDER_REFRACTION | DF_CLIP_Z | 
		DF_RENDER_UNDERWATER | DF_FUDGE_UP | 
		DF_DRAW_ENTITITES ;
}


// PS3 - warning - Refraction not yet fully supported on SPU BuildWorld/Renderables parallel job path
// so we disable that path for now when drawing refraction views and turn it back on at the end
//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAboveWaterView::CRefractionView::Draw()
{
#if defined(_PS3)
	BEGIN_2PASS_DRAW_BLOCK
	// don't support PruneWorldLists on SPU yet, so can't support refraction
	bool bBuildViewSPU = g_viewBuilder.IsSPUBuildRWJobsOn();
	g_viewBuilder.SPUBuildRWJobsOn( false );
#endif

	PS3_SPUPATH_INVALID( "CAboveWaterView::CRefractionView::Draw" );

#ifdef PORTAL
	g_pPortalRender->WaterRenderingHandler_PreRefraction();
#endif

	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "CAboveWaterView::CRefractionView" );

	// Store off view origin and angles and set the new view
	int nSaveViewID = CurrentViewID();
	Vector vecOldOrigin = CurrentViewOrigin();
	QAngle vecOldAngles = CurrentViewAngles();
	SetupCurrentView( origin, angles, VIEW_REFRACTION );

 	PROLOGUE_PASS_DRAWLISTS
	DrawSetup( pRenderContext, GetOuter()->m_waterHeight, m_DrawFlags, GetOuter()->m_waterZAdjust );

	BEGIN_2PASS_DRAW_BLOCK
	SetFogVolumeState( GetOuter()->m_fogInfo, true );
	SetClearColorToFogColor();
	END_2PASS_BLOCK

	DrawExecute( GetOuter()->m_waterHeight, VIEW_REFRACTION, GetOuter()->m_waterZAdjust );

#ifdef PORTAL
	// deal with stencil
	g_pPortalRender->WaterRenderingHandler_PostRefraction();
#endif

	// finish off the view.  restore the previous view.
	SetupCurrentView( vecOldOrigin, vecOldAngles, ( view_id_t )nSaveViewID );

	BEGIN_2PASS_DRAW_BLOCK
	// This is here for multithreading
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
	pRenderContext->Flush();
	END_2PASS_BLOCK

#if defined(_PS3)
	g_viewBuilder.SPUBuildRWJobsOn( bBuildViewSPU );
	END_2PASS_BLOCK
#endif
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAboveWaterView::CIntersectionView::Setup()
{
	BaseClass::Setup( *GetOuter() );
	m_DrawFlags = DF_RENDER_UNDERWATER | DF_CLIP_Z | DF_DRAW_ENTITITES;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAboveWaterView::CIntersectionView::Draw()
{
	PS3_SPUPATH_INVALID( "CAboveWaterView::CIntersectionView::Draw" );

	CMatRenderContextPtr pRenderContext( materials );

	PROLOGUE_PASS_DRAWLISTS
	DrawSetup( pRenderContext, GetOuter()->m_fogInfo.m_flWaterHeight, m_DrawFlags, 0 );

	SetFogVolumeState( GetOuter()->m_fogInfo, true );
	SetClearColorToFogColor( );

	DrawExecute( GetOuter()->m_fogInfo.m_flWaterHeight, VIEW_NONE, 0 );

	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
}


//-----------------------------------------------------------------------------
// Draws the scene when the view point is under the level of the water
//-----------------------------------------------------------------------------
void CUnderWaterView::Setup( const CViewSetup &view, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t& waterInfo, ViewCustomVisibility_t *pCustomVisibility )
{
	BaseClass::Setup( view );

	m_bSoftwareUserClipPlane = g_pMaterialSystemHardwareConfig->UseFastClipping();

	CalcWaterEyeAdjustments( fogInfo, m_waterHeight, m_waterZAdjust, m_bSoftwareUserClipPlane );

	IMaterial *pWaterMaterial = fogInfo.m_pFogVolumeMaterial;
		IMaterialVar *pScreenOverlayVar = pWaterMaterial->FindVar( "$underwateroverlay", NULL, false );
		if ( pScreenOverlayVar && ( pScreenOverlayVar->IsDefined() ) )
		{
			char const *pOverlayName = pScreenOverlayVar->GetStringValue();
			if ( pOverlayName[0] != '0' )						// fixme!!!
			{
				IMaterial *pOverlayMaterial = materials->FindMaterial( pOverlayName,  TEXTURE_GROUP_OTHER );
				m_pMainView->SetWaterOverlayMaterial( pOverlayMaterial );
			}
		}
	// NOTE: We're not drawing the 2d skybox under water since it's assumed to not be visible.

	// render the world underwater
	// Clear the color to get the appropriate underwater fog color
	m_DrawFlags = DF_FUDGE_UP | DF_RENDER_UNDERWATER | DF_DRAW_ENTITITES;

#ifdef PORTAL
	if ( g_pPortalRender->GetViewRecursionLevel() == 0 )
#endif
	{
		m_ClearFlags = VIEW_CLEAR_DEPTH;
	}

	if( !m_bSoftwareUserClipPlane )
	{
		m_DrawFlags |= DF_CLIP_Z;
	}
	if ( waterInfo.m_bDrawWaterSurface )
	{
		if ( waterInfo.m_bPseudoTranslucentWater )
		{
			m_DrawFlags |= DF_RENDER_PSEUDO_TRANSLUCENT_WATER;
		}
		else
		{
			m_DrawFlags |= DF_RENDER_WATER;
		}
	}
	if ( !waterInfo.m_bRefract && !waterInfo.m_bOpaqueWater )
	{
		m_DrawFlags |= DF_RENDER_ABOVEWATER;
	}

	m_fogInfo = fogInfo;
	m_waterInfo = waterInfo;
	m_bDrawSkybox = bDrawSkybox;
	m_pCustomVisibility = pCustomVisibility;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CUnderWaterView::Draw()
{
	// FIXME: The 3d skybox shouldn't be drawn when the eye is under water

	VPROF( "CViewRender::ViewDrawScene_EyeUnderWater" );

	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "CUnderWaterView::Draw" );

	// render refraction (out of water)
	if ( m_waterInfo.m_bRefract )
	{
		m_RefractionView.Setup( );
		m_pMainView->AddViewToScene( &m_RefractionView );
	}

	if ( !m_waterInfo.m_bRefract )
	{
		SetFogVolumeState( m_fogInfo, true );
		unsigned char ucFogColor[3];
		pRenderContext->GetFogColor( ucFogColor );
		pRenderContext->ClearColor4ub( ucFogColor[0], ucFogColor[1], ucFogColor[2], 255 );
	}

	PROLOGUE_PASS_DRAWLISTS
	DrawSetup( pRenderContext, m_waterHeight, m_DrawFlags, m_waterZAdjust );

	SetFogVolumeState( m_fogInfo, false );

	DrawExecute( m_waterHeight, CurrentViewID(), m_waterZAdjust );

	m_ClearFlags = 0;

	if( m_waterZAdjust != 0.0f && m_bSoftwareUserClipPlane && m_waterInfo.m_bRefract )
	{
		m_SoftwareIntersectionView.Setup( false );
		m_pMainView->AddViewToScene( &m_SoftwareIntersectionView );
	}
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CUnderWaterView::CRefractionView::Setup()
{
	BaseClass::Setup( *GetOuter() );
	// NOTE: Refraction renders into the back buffer, over the top of the 3D skybox
	// It is then blitted out into the refraction target. This is so that
	// we only have to set up 3d sky vis once, and only render it once also!
	m_DrawFlags = DF_CLIP_Z | 
		DF_CLIP_BELOW | DF_RENDER_ABOVEWATER | 
		DF_DRAW_ENTITITES;

	m_ClearFlags = VIEW_CLEAR_DEPTH;
	if ( GetOuter()->m_bDrawSkybox )
	{
		m_ClearFlags |= VIEW_CLEAR_COLOR;
		m_DrawFlags |= DF_DRAWSKYBOX | DF_CLIP_SKYBOX;
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CUnderWaterView::CRefractionView::Draw()
{
	PS3_SPUPATH_INVALID( "CUnderWaterView::CRefractionView::Draw" );

	CMatRenderContextPtr pRenderContext( materials );
	SetFogVolumeState( GetOuter()->m_fogInfo, true );
	unsigned char ucFogColor[3];
	pRenderContext->GetFogColor( ucFogColor );
	pRenderContext->ClearColor4ub( ucFogColor[0], ucFogColor[1], ucFogColor[2], 255 );

	PROLOGUE_PASS_DRAWLISTS
	DrawSetup( pRenderContext, GetOuter()->m_waterHeight, m_DrawFlags, GetOuter()->m_waterZAdjust );

	EnableWorldFog();

	DrawExecute( GetOuter()->m_waterHeight, VIEW_REFRACTION, GetOuter()->m_waterZAdjust );

	BEGIN_2PASS_DRAW_BLOCK
	Rect_t srcRect;
	srcRect.x = x;
	srcRect.y = y;
	srcRect.width = width;
	srcRect.height = height;

	ITexture *pTexture = GetWaterRefractionTexture();
	pRenderContext->CopyRenderTargetToTextureEx( pTexture, 0, &srcRect, NULL );
	END_2PASS_BLOCK

}


//-----------------------------------------------------------------------------
//
// Reflective glass view starts here
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Draws the scene when the view contains reflective glass
//-----------------------------------------------------------------------------
void CReflectiveGlassView::Setup( const CViewSetup &view, int nClearFlags, bool bDrawSkybox, 
	const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t &waterInfo, const cplane_t &reflectionPlane )
{
	BaseClass::Setup( view, nClearFlags, bDrawSkybox, fogInfo, waterInfo, NULL );
	m_ReflectionPlane = reflectionPlane;
}


bool CReflectiveGlassView::AdjustView( float flWaterHeight )
{
	ITexture *pTexture = GetWaterReflectionTexture();
		   
	// Use the aspect ratio of the main view! So, don't recompute it here
	x = y = 0;
	width = pTexture->GetActualWidth();
	height = pTexture->GetActualHeight();

	// Reflect the camera origin + vectors around the reflection plane 
	float flDist = DotProduct( origin, m_ReflectionPlane.normal ) - m_ReflectionPlane.dist;
	VectorMA( origin, - 2.0f * flDist, m_ReflectionPlane.normal, origin );

	Vector vecForward, vecUp;
	AngleVectors( angles, &vecForward, NULL, &vecUp );

	float flDot = DotProduct( vecForward, m_ReflectionPlane.normal );
	VectorMA( vecForward, - 2.0f * flDot, m_ReflectionPlane.normal, vecForward );

	flDot = DotProduct( vecUp, m_ReflectionPlane.normal );
	VectorMA( vecUp, - 2.0f * flDot, m_ReflectionPlane.normal, vecUp );

	VectorAngles( vecForward, vecUp, angles );
	return true;
}

void CReflectiveGlassView::PushView( float waterHeight )
{
	CMatRenderContextPtr pRenderContext( materials );
	render->Push3DView( pRenderContext, *this, m_ClearFlags, GetWaterReflectionTexture(), GetFrustum() );
	 
	Vector4D plane;
	VectorCopy( m_ReflectionPlane.normal, plane.AsVector3D() );
	plane.w = m_ReflectionPlane.dist + 0.1f;

	pRenderContext->PushCustomClipPlane( plane.Base() );
}

void CReflectiveGlassView::PopView( )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PopCustomClipPlane( );
	render->PopView( pRenderContext, GetFrustum() );
}


//-----------------------------------------------------------------------------
// Renders reflective or refractive parts of glass
//-----------------------------------------------------------------------------
void CReflectiveGlassView::Draw()
{
	VPROF( "CReflectiveGlassView::Draw" );

	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "CReflectiveGlassView::Draw" );

	// Disable occlusion visualization in reflection
	bool bVisOcclusion = r_visocclusion.GetBool();
	r_visocclusion.SetValue( 0 );
			
	BaseClass::Draw();

	r_visocclusion.SetValue( bVisOcclusion );

	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
	pRenderContext->Flush();
}

//-----------------------------------------------------------------------------
// Draws the scene when the view contains reflective glass
//-----------------------------------------------------------------------------
void CRefractiveGlassView::Setup( const CViewSetup &view, int nClearFlags, bool bDrawSkybox, 
	const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t &waterInfo, const cplane_t &reflectionPlane )
{
	BaseClass::Setup( view, nClearFlags, bDrawSkybox, fogInfo, waterInfo, NULL );
	m_ReflectionPlane = reflectionPlane;
}


bool CRefractiveGlassView::AdjustView( float flWaterHeight )
{
	ITexture *pTexture = GetWaterRefractionTexture();

	// Use the aspect ratio of the main view! So, don't recompute it here
	x = y = 0;
	width = pTexture->GetActualWidth();
	height = pTexture->GetActualHeight();
	return true;
}


void CRefractiveGlassView::PushView( float waterHeight )
{
	CMatRenderContextPtr pRenderContext( materials );
	render->Push3DView( pRenderContext, *this, m_ClearFlags, GetWaterRefractionTexture(), GetFrustum() );

	Vector4D plane;
	VectorMultiply( m_ReflectionPlane.normal, -1, plane.AsVector3D() );
	plane.w = -m_ReflectionPlane.dist + 0.1f;

	pRenderContext->PushCustomClipPlane( plane.Base() );
}


void CRefractiveGlassView::PopView( )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PopCustomClipPlane( );
	render->PopView( pRenderContext, GetFrustum() );
}



//-----------------------------------------------------------------------------
// Renders reflective or refractive parts of glass
//-----------------------------------------------------------------------------
void CRefractiveGlassView::Draw()
{
	VPROF( "CRefractiveGlassView::Draw" );

	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "CRefractiveGlassView::Draw" );

	BaseClass::Draw();

	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
	pRenderContext->Flush();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void FrustumCache_t::Add( const CViewSetup *pView, int iSlot )
{
	// Check for a valid view setup.
	if ( !pView )
		return;

	// Create the perspective frustum.
	GeneratePerspectiveFrustum( pView->origin, pView->angles, pView->zNear, pView->zFar, pView->fov, pView->m_flAspectRatio, m_Frustums[iSlot] );
}


#if defined(_PS3)
//-----------------------------------------------------------------------------
// PS3 - CConcurrentViewBuilderPS3 methods
//-----------------------------------------------------------------------------

CConcurrentViewBuilderPS3::CConcurrentViewBuilderPS3()
{ 
	m_buildViewID		= -1; 
	m_bSPUBuildRWJobsOn = false;
	m_passFlags			= 0;

	for( int lp = 0; lp < MAX_CONCURRENT_BUILDVIEWS; lp++ )
	{
		m_gAreaFrustum[lp].EnsureCapacity(16);
		m_gAreaFrustum[lp].SetCount(0);
	}
}


void CConcurrentViewBuilderPS3::Init( void ) 
{ 
	m_buildViewID		= -1; 
	m_bSPUBuildRWJobsOn = false;
	m_passFlags			= 0;

	for( int lp = 0; lp < MAX_CONCURRENT_BUILDVIEWS; lp++ )
	{
		m_gAreaFrustum[lp].EnsureCapacity(16);
		m_gAreaFrustum[lp].SetCount(0);
	}
}

void CConcurrentViewBuilderPS3::Purge( void ) 
{ 
	for( int lp = 0; lp < MAX_CONCURRENT_BUILDVIEWS; lp++ )
	{
		m_gAreaFrustum[lp].Purge();
	}
};

void CConcurrentViewBuilderPS3::ResetBuildViewID( void ) 
{ 
	m_buildViewID			= -1; 
	m_nextFreeBuildViewID	= 0; 

	m_pBuildViewStack		= m_buildViewStack - 1;
	m_buildViewStack[ 0 ]   = -1;
};


// get current view index
int CConcurrentViewBuilderPS3::GetBuildViewID( void )		
{ 
	if( m_buildViewID == -1 )
	{
		// bad view initialisation
		Warning("*** BAD BUILD VIEW INITIALIZATION ***\n"); 
		return 0;
	}

	return m_buildViewID; 
};

// call at the start of each view
void CConcurrentViewBuilderPS3::PushBuildView( void )
{
	m_pBuildViewStack++;

	if( m_pBuildViewStack >= &m_buildViewStack[MAX_CONCURRENT_BUILDVIEWS] )
	{
		Error("*** exceeded concurrent buildview push ***\n"); 
	}

	if( m_nextFreeBuildViewID >= MAX_CONCURRENT_BUILDVIEWS )
	{
		Error("*** exceeded max concurrent buildviews ***\n"); 
	}


	*m_pBuildViewStack = m_nextFreeBuildViewID;

	m_buildViewID = *m_pBuildViewStack;

	m_nextFreeBuildViewID++;
}

void CConcurrentViewBuilderPS3::PopBuildView( void )
{
	if( m_pBuildViewStack == m_buildViewStack )
	{
		m_buildViewID = *m_pBuildViewStack;
		m_pBuildViewStack--;
	}
	else
	{
		m_pBuildViewStack--;
		m_buildViewID = *m_pBuildViewStack;
	}

}

void CConcurrentViewBuilderPS3::SyncViewBuilderJobs( void )
{
	// sync all ports, only used for debugging multipass views
}



IWorldRenderList *CConcurrentViewBuilderPS3::GetWorldRenderListElement( void )
{ 
	if( m_buildViewID == -1 )
	{
		Warning( "PS3 ViewBuilder Begin/End Error - Accessing WorldRenderListElement(-1)\n" );
		return NULL;
	}
	else
	{
		return m_pWorldRenderListCache[ m_buildViewID ]; 
	}
}

void CConcurrentViewBuilderPS3::SetWorldRenderListElement( IWorldRenderList *pRenderList )
{ 
	if( m_buildViewID == -1 )
	{
		Warning( "PS3 ViewBuilder Begin/End Error - Setting WorldRenderListElement(-1)\n" );
		return;
	}
	else
	{
		m_pWorldRenderListCache[ m_buildViewID ] = pRenderList; 
	}
}

unsigned int CConcurrentViewBuilderPS3::GetVisFlags( void ) 
{ 
	if( m_buildViewID == -1 )
	{
		Warning( "PS3 ViewBuilder Begin/End Error - GetVisFlags(-1)\n" );
		return 0;
	}
	else
	{
		return m_visFlags[ m_buildViewID ]; 
	}
}

void CConcurrentViewBuilderPS3::SetVisFlags( unsigned int visFlags ) 
{ 
	if( m_buildViewID == -1 )
	{
		Warning( "PS3 ViewBuilder Begin/End Error - SetVisFlags(-1)\n" );
		return;
	}
	else
	{
		m_visFlags[ m_buildViewID ] = visFlags; 
	}
}

void* CConcurrentViewBuilderPS3::GetBuildViewVolumeCuller( void )
{ 
	if( m_buildViewID == -1 )
	{
		Warning( "PS3 ViewBuilder Begin/End Error - GetBuildViewVolumeCuller(-1)\n" );
		return NULL;
	}
	else
	{
		return &m_volumeCullerCache[ m_buildViewID ]; 
	}
}

Frustum_t *CConcurrentViewBuilderPS3::GetBuildViewFrustum( void )	
{ 
	if( m_buildViewID == -1 )
	{
		Warning( "PS3 ViewBuilder Begin/End Error - GetBuildViewFrustum(-1)\n" );
		return NULL;
	}
	else
	{
		return &m_gFrustum[ m_buildViewID ]; 
	}
}


Frustum_t *CConcurrentViewBuilderPS3::GetBuildViewAreaFrustum( void ) 
{ 
	if( m_buildViewID == -1 )
	{
		Warning( "PS3 ViewBuilder Begin/End Error - GetBuildViewAreaFrustum(-1)\n" );
		return NULL;
	}
	else
	{
		return m_gAreaFrustum[ m_buildViewID ].Base(); 
	}
}


unsigned char *CConcurrentViewBuilderPS3::GetBuildViewRenderAreaBits( void ) 
{ 
	if( m_buildViewID == -1 )
	{
		Warning( "PS3 ViewBuilder Begin/End Error - GetBuildViewRenderAreaBits(-1)\n" );
		return NULL;
	}
	else
	{
		return m_gRenderAreaBits[ m_buildViewID ]; 
	}
};

int CConcurrentViewBuilderPS3::GetNumAreaFrustum( void ) 
{ 
	if( m_buildViewID == -1 )
	{
		Warning( "PS3 ViewBuilder Begin/End Error - GetNumAreaFrustum(-1)\n" );
		return 0;
	}
	else
	{
		return m_gAreaFrustum[ m_buildViewID ].Count(); 
	}
};


Frustum_t *CConcurrentViewBuilderPS3::GetBuildViewAreaFrustumID( int frustumID ) 
{ 
	if( m_buildViewID == -1 )
	{
		Warning( "PS3 ViewBuilder Begin/End Error - GetNumAreaFrustum(-1)\n" );
		return NULL;
	}
	else
	{
		return &m_gAreaFrustum[ m_buildViewID ][ frustumID ];
	}
};

void CConcurrentViewBuilderPS3::CacheFrustumData( Frustum_t *pFrustum, Frustum_t *pAreaFrustum, void *pRenderAreaBits, int numArea, bool bViewerInSolidSpace )
{
	if( m_buildViewID == -1 )
		return;

	// cache g_Frustum
	memcpy( &m_gFrustum[ m_buildViewID ], pFrustum, sizeof(Frustum_t) );

	// cache g_RenderAreaBits
	memcpy( &m_gRenderAreaBits[ m_buildViewID ], pRenderAreaBits, sizeof(m_gRenderAreaBits[ m_buildViewID ]) );

	// cache viewerinSolidSpace
	m_bViewerInSolidSpace[ m_buildViewID ] = bViewerInSolidSpace;

	// cache g_AreaFrustum
	m_gAreaFrustum[ m_buildViewID ].CopyArray( pAreaFrustum, numArea );

}

void CConcurrentViewBuilderPS3::CacheBuildViewVolumeCuller( void *pVC )
{
	if( (m_buildViewID == -1) || (pVC == NULL) )
		return;

	memcpy( &m_volumeCullerCache[ m_buildViewID ], pVC, sizeof(CVolumeCuller) );
}


// push all buildrenderable jobs - we have descriptors and cached data ready
// renderable jobs can't run concurrently and must sync to the matching buildworldjob
void CConcurrentViewBuilderPS3::PushBuildRenderableJobs( void )
{
	SNPROF("CConcurrentViewBuilder::PushBuildRenderableJobs");

	int numViews = m_buildViewID + 1;
	unsigned int syncTagR, syncTagW, syncMask;

	syncMask = 0;

	unsigned int lastSyncTagR = 0;

	//Msg("PushBuildRenderables\n");
	for( int lp = 0; lp < numViews; lp++ )
	{
		PS3BuildRenderablesJobData *pJobData					= g_pBuildRenderablesJob->GetJobData( lp );
		job_buildrenderables::JobDescriptor_t *pJobDescriptor	= &pJobData->jobDescriptor;

		syncTagW = (lp+1);

		// alternative - none of the buildrenderable jobs will run in parallel
		if( lp > 0 )
			syncTagR = numViews+2; // magic no.
		else
			syncTagR = 0;
		
		// alternative 
		//if( lp > 1 )
		//{
		//	syncTagR = numViews+2;
		//}
		//else
		//{
		//	syncTagR = 0;
		//}

		syncMask = (0x01 << syncTagW) | (0x01 << lastSyncTagR);

		// pushsync
		CELL_VERIFY( g_pBuildRenderablesJob->m_pRoot->m_queuePortBuildRenderables[ lp ].pushSync( syncMask, 0 ) );
		//Msg("PushSync %d, syncTagW %d, syncTagR %d\n", syncMask, syncTagW, syncTagR );

		// testing syncTagR = numViews+2;

		// pushjob
		CELL_VERIFY( g_pBuildRenderablesJob->m_pRoot->m_queuePortBuildRenderables[ lp ].pushJob( &pJobDescriptor->header, sizeof(*pJobDescriptor), syncTagR, CELL_SPURS_JOBQUEUE_FLAG_SYNC_JOB ) );

		lastSyncTagR = syncTagR;
	}
}


#else // _PS3 CConcurrentViewBuilderPS3 methods

//-----------------------------------------------------------------------------
// CConcurrentViewData Methods
//-----------------------------------------------------------------------------

void CConcurrentViewData::Init()
{	
	m_pWorldRenderList			= NULL;
	m_pWorldListInfo			= NULL;
	m_pRenderablesList			= NULL;
	m_pBuildWorldListJob		= NULL;
	m_pBuildRenderablesListJob	= NULL;
	m_bWaitForWorldList			= false;
	m_volumeCuller.Clear();
	for ( int lp = 0; lp < MAX_MAP_AREAS; ++lp )
	{
		m_frustumList[lp] = NULL;
	}
}

void CConcurrentViewData::Purge()
{
	SafeRelease( m_pWorldRenderList );
	SafeRelease( m_pWorldListInfo );
	SafeRelease( m_pRenderablesList );
	SafeRelease( m_pBuildWorldListJob );
	SafeRelease( m_pBuildRenderablesListJob );
	m_AreaFrustums.RemoveAll();
}

//-----------------------------------------------------------------------------
// CConcurrentViewBuilder Methods (Non PS3)
//-----------------------------------------------------------------------------

CConcurrentViewBuilder::CConcurrentViewBuilder()
{
	Init();
}

CConcurrentViewBuilder::~CConcurrentViewBuilder()
{
	Purge();
}

void CConcurrentViewBuilder::Init()
{
	// Release lists (in case a list has been added between 2 Init calls)
	Purge();
	
	SetBuildWRThreaded( false );
	ResetBuildViewID();

	for( int lp = 0; lp < MAX_CONCURRENT_BUILDVIEWS; lp++ )
	{
		m_viewData[lp].Init();
	}

	m_pCurrentSeqJobs = NULL;
	m_pPendingSeqJobs = NULL;
}

void CConcurrentViewBuilder::Purge()
{
	// Make sure all jobs added to the global thread pool have finished 
	// in case we have a mismatched between the build and draw pass
	// ie a job has been added in the build pass but we are not waiting for
	// in the draw pass (ie it did not get drawn)
	for (int i = 0; i < mJobsAddedToThreadPool.Count(); i++)
	{
		mJobsAddedToThreadPool[i]->WaitForFinishAndRelease();
	}
	mJobsAddedToThreadPool.RemoveAll();
	
	
	for( int lp = 0; lp < MAX_CONCURRENT_BUILDVIEWS; lp++ )
	{
		m_viewData[lp].Purge();
	}

	SafeRelease( m_pCurrentSeqJobs );
	SafeRelease( m_pPendingSeqJobs );
}

void CConcurrentViewBuilder::ResetBuildViewID( void ) 
{ 
	m_buildViewID			= -1; 
	m_nextFreeBuildViewID	= 0; 

	m_pBuildViewStack		= m_buildViewStack - 1;
	m_buildViewStack[ 0 ]   = -1;
};


// get current view index
int CConcurrentViewBuilder::GetBuildViewID( void )		
{ 
	if( m_buildViewID == -1 )
	{
		// bad view initialisation
		Warning("*** BAD BUILD VIEW INITIALIZATION ***\n"); 
		return 0;
	}

	return m_buildViewID; 
};

// call at the start of each view
void CConcurrentViewBuilder::PushBuildView( void )
{
	m_pBuildViewStack++;

	if( m_pBuildViewStack >= &m_buildViewStack[MAX_CONCURRENT_BUILDVIEWS] )
	{
		Error("*** exceeded concurrent buildview push ***\n"); 
	}

	if( m_nextFreeBuildViewID >= MAX_CONCURRENT_BUILDVIEWS )
	{
		Error("*** exceeded max concurrent buildviews ***\n"); 
	}


	*m_pBuildViewStack = m_nextFreeBuildViewID;

	m_buildViewID = *m_pBuildViewStack;

	m_nextFreeBuildViewID++;
}

void CConcurrentViewBuilder::PopBuildView( void )
{
	if( m_pBuildViewStack == m_buildViewStack )
	{
		m_buildViewID = *m_pBuildViewStack;
		m_pBuildViewStack--;
	}
	else
	{
		m_pBuildViewStack--;
		m_buildViewID = *m_pBuildViewStack;
	}

}

void CConcurrentViewBuilder::CacheBuildViewVolumeCuller( const CVolumeCuller *pVC )
{
	if( m_buildViewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Setting volume culler(-1)\n" );
		return;
	}
	else if ( pVC )
	{
		m_viewData[ m_buildViewID ].m_volumeCuller = *pVC;
	}
}

const CVolumeCuller* CConcurrentViewBuilder::GetBuildViewVolumeCuller( int buildViewID /*= -1*/ ) const
{
	int viewID = ( buildViewID >= 0 ) ? buildViewID : m_buildViewID;	
	if( viewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Accessing volume culler(-1)\n" );
		return NULL;
	}
	else
	{
		const CVolumeCuller* pVolumeCuller = &m_viewData[viewID].m_volumeCuller;
		return ( pVolumeCuller->IsValid() ? pVolumeCuller : NULL ); 
	}
}

void CConcurrentViewBuilder::CacheFrustumData( const Frustum_t& frustum, const CUtlVector< Frustum_t, CUtlMemoryAligned< Frustum_t,16 > >& aeraFrustums )
{
	if( m_buildViewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Setting frustum data(-1)\n" );
	}
	else
	{
		CConcurrentViewData* pViewData = &m_viewData[m_buildViewID];

		// Copy frustum and area frustums
		pViewData->m_Frustum = frustum;
		pViewData->m_AreaFrustums = aeraFrustums;

		// Setup frustum list used by the ClientLeafSystem
		pViewData->m_frustumList[0] = &pViewData->m_Frustum;
		int count = MIN( MAX_MAP_AREAS-1, pViewData->m_AreaFrustums.Count() );
		for ( int i = 0; i < count; i++ )
		{
			if ( engine->ShouldUseAreaFrustum( i ) )
			{
				pViewData->m_frustumList[i+1] = &pViewData->m_AreaFrustums[i];
			}
			else
			{
				pViewData->m_frustumList[i+1] = &pViewData->m_Frustum;
			}
		}
	}
}

void CConcurrentViewBuilder::CacheFrustumData( void )
{
	if( m_buildViewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Setting frustum data(-1)\n" );
	}
	else
	{
		CConcurrentViewData* pViewData = &m_viewData[m_buildViewID];

		int numFrustums = engine->GetFrustumList( pViewData->m_frustumList, MAX_MAP_AREAS );

		// Copy frustum and frustum areas
		pViewData->m_Frustum = *pViewData->m_frustumList[0];
		if ( numFrustums > 1 )
		{
			for ( int i = 1; i < numFrustums; ++i )
			{
				pViewData->m_AreaFrustums.AddToTail( *pViewData->m_frustumList[i] );
			}
		}

		// Regenerate frustum list
		pViewData->m_frustumList[0] = &pViewData->m_Frustum;
		int count = MIN( MAX_MAP_AREAS-1, pViewData->m_AreaFrustums.Count() );
		for ( int i = 0; i < count; i++ )
		{
			pViewData->m_frustumList[i+1] = &pViewData->m_AreaFrustums[i];
		}
	}
}

const Frustum_t* CConcurrentViewBuilder::GetBuildViewFrustum( int buildViewID /*= -1*/ ) const
{
	int viewID = ( buildViewID >= 0 ) ? buildViewID : m_buildViewID;	
	if( viewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Accessing frustum(-1)\n" );
		return NULL;
	}
	else
	{
		return &m_viewData[viewID].m_Frustum;
	}
}

const CUtlVector< Frustum_t, CUtlMemoryAligned< Frustum_t,16 > >* CConcurrentViewBuilder::GetBuildViewAeraFrustums( int buildViewID /*= -1*/ ) const
{
	int viewID = ( buildViewID >= 0 ) ? buildViewID : m_buildViewID;	
	if( viewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Accessing aera frustums(-1)\n" );
		return NULL;
	}
	else
	{
		return &m_viewData[viewID].m_AreaFrustums;
	}
}

Frustum_t** CConcurrentViewBuilder::GetBuildViewFrustumList( int buildViewID /*= -1*/ )
{
	int viewID = ( buildViewID >= 0 ) ? buildViewID : m_buildViewID;	
	if( viewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Accessing frustum list(-1)\n" );
		return NULL;
	}
	else
	{
		return m_viewData[viewID].m_frustumList;
	}
}

IWorldRenderList *CConcurrentViewBuilder::GetWorldRenderListElement( void )
{ 
	if( m_buildViewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Accessing WorldRenderListElement(-1)\n" );
		return NULL;
	}
	else
	{
		return m_viewData[m_buildViewID].m_pWorldRenderList; 
	}
}

void CConcurrentViewBuilder::SetWorldRenderListElement( IWorldRenderList *pRenderList )
{ 
	if( m_buildViewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Setting WorldRenderListElement(-1)\n" );
		return;
	}
	else
	{
		m_viewData[m_buildViewID].m_pWorldRenderList = pRenderList;
		// Increment ref count
		InlineAddRef( pRenderList );
	}
}

ClientWorldListInfo_t *CConcurrentViewBuilder::GetClientWorldListInfoElement( int buildViewID /*= -1*/ )
{ 
	int viewID = ( buildViewID >= 0 ) ? buildViewID : m_buildViewID;	
	if( viewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Accessing ClientWorlListInfoElement(-1)\n" );
		return NULL;
	}
	else
	{
		return m_viewData[viewID].m_pWorldListInfo; 
	}
}

WorldListInfo_t *CConcurrentViewBuilder::GetWorldListInfoElement( int buildViewID  )
{ 
	if( buildViewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Accessing WorlListInfoElement(-1)\n" );
		return NULL;
	}
	else
	{
		return m_viewData[buildViewID].m_pWorldListInfo; 
	}
}

void CConcurrentViewBuilder::SetWorldListInfoElement( ClientWorldListInfo_t *pWorldListInfo, int buildViewID /*= -1*/ )
{ 
	int viewID = ( buildViewID >= 0 ) ? buildViewID : m_buildViewID;
	if( viewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Setting WorldListInfoElement(-1)\n" );
		return;
	}
	else
	{
		CConcurrentViewData* pViewData = &m_viewData[viewID];

		// Release old list if necessary
		SafeRelease( pViewData->m_pWorldListInfo );

		pViewData->m_pWorldListInfo = pWorldListInfo;
		// Increment ref count
		InlineAddRef( pWorldListInfo );
	}
}

CClientRenderablesList *CConcurrentViewBuilder::GetRenderablesListElement( int buildViewID /*= -1*/ )
{ 
	int viewID = ( buildViewID >= 0 ) ? buildViewID : m_buildViewID;	
	if( viewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Accessing RenderablesListElement(-1)\n" );
		return NULL;
	}
	else
	{
		return m_viewData[viewID].m_pRenderablesList; 
	}
}

void CConcurrentViewBuilder::SetRenderablesListElement( CClientRenderablesList *pRenderables )
{ 
	if( m_buildViewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Setting RenderablesListElement(-1)\n" );
		return; 
	}
	else
	{
		m_viewData[m_buildViewID].m_pRenderablesList = pRenderables;
		// Increment ref count
		InlineAddRef( pRenderables );
	}
}

void CConcurrentViewBuilder::QueueBuildWorldListJob( CJob* pJob )
{
	if( m_buildViewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Queueing Build World list job(-1)\n" );
		return;
	}
	else
	{
		if ( r_threaded_buildWRlist.GetBool() && GetBuildWRThreaded() )
		{
			TryRunSequentialJobs();

			CConcurrentViewData* pViewData = &m_viewData[m_buildViewID];
			if ( !pViewData->m_pBuildWorldListJob )
			{
				pViewData->m_pBuildWorldListJob = new SequentialJobs( pJob );
			}
			else
			{
				SequentialJobs* pSeqJob = (SequentialJobs *)pViewData->m_pBuildWorldListJob;
				pSeqJob->AddJob( pJob );
			}
		}
		else
		{
			pJob->Execute();
		}
	}
}

void CConcurrentViewBuilder::FlushBuildWorldListJob( void )
{
	if( m_buildViewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Flushing Build World list job(-1)\n" );
		return;
	}
	else
	{
		CConcurrentViewData* pViewData = &m_viewData[m_buildViewID];
		if ( pViewData->m_pBuildWorldListJob )
		{
			//DevMsg("***** FlushBuildWorldListJob %d\n", m_buildViewID);
			AddJobToThreadPool( pViewData->m_pBuildWorldListJob );
		}
	}
}


void CConcurrentViewBuilder::WaitForBuildWorldListJob( void )
{
	if( m_buildViewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Wait for Build World list job to finish (-1)\n" );
		return;
	}
	else
	{
		CJob* pJob = m_viewData[m_buildViewID].m_pBuildWorldListJob;
		if ( pJob )
		{
			pJob->WaitForFinish();
		}
	}
}

void CConcurrentViewBuilder::InternalWaitForBuildWorldListJob( int buildViewID )
{
	if( buildViewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Wait for Build World list job to finish (-1)\n" );
		return;
	}
	else
	{
		CJob* pJob = m_viewData[buildViewID].m_pBuildWorldListJob;
		if ( pJob && !pJob->IsFinished() )
		{
			// Called on a worker thread so do not call pJob->WaitForFinish() as it could try to 
			// execute new job while waiting (=> deadlock). Instead wait on CJob::m_CompletedEvent.
			pJob->AccessEvent()->Wait();
		}
	}
}

void CConcurrentViewBuilder::QueueBuildRenderablesListJob( CJob* pJob )
{
	if( m_buildViewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Queueing Build renderables list job(-1)\n" );
		return;
	}
	else
	{
		CConcurrentViewData* pViewData = &m_viewData[m_buildViewID];

		pViewData->m_pBuildRenderablesListJob = pJob;
		// Increment ref count
		InlineAddRef( pJob );	// Released in Purge()

		if ( r_threaded_buildWRlist.GetBool() && GetBuildWRThreaded() )
		{
			if ( pViewData->m_bWaitForWorldList )
			{
				CFunctorJob *pWaitJob = new CFunctorJob( CreateFunctor( this, &CConcurrentViewBuilder::InternalWaitForBuildWorldListJob, m_buildViewID ) );
				AddToSequentialJobs( pWaitJob );
				pWaitJob->Release();
			}

			AddToSequentialJobs( pJob );
			TryRunSequentialJobs();
		}
		else
		{
			pJob->Execute();
		}
	}
}

void CConcurrentViewBuilder::WaitForBuildRenderablesListJob( void )
{
	if( m_buildViewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Wait for Build renderables list job to finish (-1)\n" );
		return;
	}
	else
	{
		CJob* pJob = m_viewData[m_buildViewID].m_pBuildRenderablesListJob;
		if ( pJob )
		{
			pJob->WaitForFinish();
		}
	}
}

void CConcurrentViewBuilder::FlushBuildRenderablesListJob( void )
{
	if ( m_pPendingSeqJobs )
	{
		
		if ( m_pCurrentSeqJobs )
		{
			// Wait for the current job to finish before executing pending jobs
			// Use a job to wait so that the main thread can keep running
			CFunctorJob *pJob = new CFunctorJob( CreateFunctor(this, &CConcurrentViewBuilder::WaitForCurrentSequentialJobAndRunPending ) );
			AddJobToThreadPool( pJob );
			pJob->Release();
		}
		else
		{
			TryRunSequentialJobs();
		}
	}
}

void CConcurrentViewBuilder::AddDependencyToWorldList( void )
{
	if( m_buildViewID == -1 )
	{
		Warning( "ViewBuilder Begin/End Error - Add dependency to world list (-1)\n" );
		return;
	}
	else
	{
		m_viewData[m_buildViewID].m_bWaitForWorldList = true;
	}
}

void CConcurrentViewBuilder::AddToSequentialJobs( CJob* pJob )
{
	if ( !m_pPendingSeqJobs )
	{
		m_pPendingSeqJobs = new SequentialJobs();
	}
	m_pPendingSeqJobs->AddJob( pJob );
}

void CConcurrentViewBuilder::TryRunSequentialJobs( void )
{
	if ( m_pCurrentSeqJobs && m_pCurrentSeqJobs->IsFinished() )
	{
		SafeRelease( m_pCurrentSeqJobs );
		m_pCurrentSeqJobs = NULL;
	}

	if ( !m_pCurrentSeqJobs && m_pPendingSeqJobs )
	{
		m_pCurrentSeqJobs = m_pPendingSeqJobs;
		AddJobToThreadPool( m_pCurrentSeqJobs );
		m_pPendingSeqJobs = NULL;
	}
}

void CConcurrentViewBuilder::WaitForCurrentSequentialJobAndRunPending()
{
	if ( m_pCurrentSeqJobs && !m_pCurrentSeqJobs->IsFinished() )
	{
		m_pCurrentSeqJobs->AccessEvent()->Wait();
	}

	// Run pending jobs
	// m_pCurrentSeqJobs & m_pPendingSeqJobs will get released in Purge()
	if ( m_pPendingSeqJobs )
	{
		m_pPendingSeqJobs->Execute();
	}
}

void CConcurrentViewBuilder::AddJobToThreadPool( CJob* pJob )
{
	// Keeps track of all jobs added to the thread pool
	mJobsAddedToThreadPool.AddToTail( pJob );
	InlineAddRef( pJob );	// Get released in purge
	
	// Set JF_QUEUE flag to make sure that the job is not executed in CThreadPool::AddJob 
	// We want the job to be executed on a worker thread unless we are waiting for job to 
	// complete on the main thread
	pJob->SetFlags( pJob->GetFlags() | JF_QUEUE );
	g_pThreadPool->AddJob( pJob );
}

CConcurrentViewBuilder::SequentialJobs::SequentialJobs( CJob* pJob /* = NULL */ )
{
	if ( pJob )
	{
		AddJob( pJob );
	}
}

CConcurrentViewBuilder::SequentialJobs::~SequentialJobs()
{
	for ( int i = 0; i < m_jobs.Count(); i++ )
	{
		SafeRelease( m_jobs[i] );
	}
}

void CConcurrentViewBuilder::SequentialJobs::AddJob( CJob* pJob )
{
	m_jobs.AddToTail( pJob );
	InlineAddRef( pJob );
}

JobStatus_t CConcurrentViewBuilder::SequentialJobs::DoExecute()
{
	for ( int i = 0; i < m_jobs.Count(); i++ )
	{
		m_jobs[i]->Execute();
	}
	return JOB_OK;
}

#endif
