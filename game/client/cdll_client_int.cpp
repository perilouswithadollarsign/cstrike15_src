//======= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======
//
//
//===============================================================================

#include "cbase.h"
#include <crtmemdebug.h>
#include "vgui_int.h"
#include "clientmode.h"
#include "iinput.h"
#include "iviewrender.h"
#include "ivieweffects.h"
#include "ivmodemanager.h"
#include "prediction.h"
#include "clientsideeffects.h"
#include "particlemgr.h"
#include "steam/steam_api.h"
#include "smoke_fog_overlay.h"
#include "view.h"
#include "ienginevgui.h"
#include "iefx.h"
#include "enginesprite.h"
#include "networkstringtable_clientdll.h"
#include "voice_status.h"
#include "filesystem.h"
#include "filesystem/IXboxInstaller.h"
#include "c_te_legacytempents.h"
#include "c_rope.h"
#include "engine/ishadowmgr.h"
#include "engine/IStaticPropMgr.h"
#include "hud_basechat.h"
#include "hud_crosshair.h"
#include "view_shared.h"
#include "env_wind_shared.h"
#include "detailobjectsystem.h"
#include "soundenvelope.h"
#include "c_basetempentity.h"
#include "materialsystem/imaterialsystemstub.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "c_soundscape.h"
#include "engine/ivdebugoverlay.h"
#include "vguicenterprint.h"
#include "iviewrender_beams.h"
#include "tier0/vprof.h"
#include "engine/IEngineTrace.h"
#include "engine/ivmodelinfo.h"
#include "physics.h"
#include "usermessages.h"
#include "gamestringpool.h"
#include "c_user_message_register.h"
#include "IGameUIFuncs.h"
#include "saverestoretypes.h"
#include "saverestore.h"
#include "physics_saverestore.h"
#include "igameevents.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "kbutton.h"
#include "tier0/icommandline.h"
#include "vstdlib/jobthread.h"
#include "gamerules_register.h"
#include "game/client/iviewport.h"
#include "vgui_controls/AnimationController.h"
#include "bitmap/tgawriter.h"
#include "c_world.h"
#include "perfvisualbenchmark.h"	
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "hud_closecaption.h"
#include "colorcorrectionmgr.h"
#include "physpropclientside.h"
#include "panelmetaclassmgr.h"
#include "c_vguiscreen.h"
#include "imessagechars.h"
#include "game/client/IGameClientExports.h"
#include "client_factorylist.h"
#include "ragdoll_shared.h"
#include "rendertexture.h"
#include "view_scene.h"
#include "iclientmode.h"
#include "con_nprint.h"
#include "inputsystem/iinputsystem.h"
#include "appframework/IAppSystemGroup.h"
#include "scenefilecache/ISceneFileCache.h"
#include "tier3/tier3.h"
#include "avi/iavi.h"
#include "avi/iquicktime.h"
#include "ihudlcd.h"
#include "toolframework_client.h"
#include "hltvcamera.h"
#include "hltvreplaysystem.h"
#if defined( REPLAY_ENABLED )
#include "replaycamera.h"
#include "replay_ragdoll.h"
#include "replay_ragdoll.h"
#include "qlimits.h"
#include "engine/ireplayhistorymanager.h"
#endif
#include "ixboxsystem.h"
#include "matchmaking/imatchframework.h"
#include "cdll_bounded_cvars.h"
#include "matsys_controls/matsyscontrols.h"
#include "GameStats.h"
#include "videocfg/videocfg.h"
#include "tier2/tier2_logging.h"
#include "Sprite.h"
#include "hud_savestatus.h"
#include "vgui_video.h"
#if defined( CSTRIKE15 )
#include "gametypes/igametypes.h"
#include "c_keyvalue_saver.h"
#include "cs_workshop_manager.h"
#include "c_team.h"
#include "cstrike15/fatdemo.h"
#endif

#include "mumble.h"

#include "vscript/ivscript.h"
#include "activitylist.h"
#include "eventlist.h"
#ifdef GAMEUI_UISYSTEM2_ENABLED
#include "gameui.h"
#endif
#ifdef GAMEUI_EMBEDDED
#if defined( PORTAL2 )
#ifdef GAMEUI_UISYSTEM2_ENABLED
#include "gameui/basemodpanel.h"
#else
#include "portal2/gameui/portal2/basemodpanel.h"
#endif
#elif defined( SWARM_DLL )
#include "swarm/gameui/swarm/basemodpanel.h"
#elif defined( CSTRIKE15 )
#include "cstrike15/gameui/basepanel.h"
#else
#error "GAMEUI_EMBEDDED"
#endif
#endif

#include "game_controls/igameuisystemmgr.h"

#ifdef DEMOPOLISH_ENABLED
#include "demo_polish/demo_polish.h"
#endif

#include "imaterialproxydict.h"
#include "vjobs_interface.h"
#include "tier0/miniprofiler.h" 
#include "engine/iblackbox.h"
#include "c_rumble.h"
#include "viewpostprocess.h"
#include "cstrike15_gcmessages.pb.h"

#include "achievements_and_stats_interface.h"

#if defined ( CSTRIKE15 )
#include "iachievementmgr.h"
#include "hud.h"
#include "hud_element_helper.h"
#include "Scaleform/HUD/sfhud_chat.h"
#include "Scaleform/HUD/sfhud_radio.h"
#include "Scaleform/options_scaleform.h"
#include "Scaleform/loadingscreen_scaleform.h"
#include "Scaleform/HUD/sfhud_deathnotice.h"
#endif

#ifdef PORTAL
#include "PortalRender.h"
#endif

#ifdef PORTAL2
#include "portal_util_shared.h"
#include "portal_base2d_shared.h"
#include "c_keyvalue_saver.h"
#include "portal2/gameui/portal2/steamoverlay/isteamoverlaymgr.h"
extern void ProcessPortalTeleportations( void );
#if defined( PORTAL2_PUZZLEMAKER )
#include "puzzlemaker/puzzlemaker.h"
#endif // PORTAL2_PUZZLEMAKER
#endif

#ifdef INFESTED_PARTICLES
#include "c_asw_generic_emitter.h"
#endif

#if defined( CSTRIKE15 )
#include "p4lib/ip4.h"
#endif

#ifdef INFESTED_DLL
#include "missionchooser/iasw_mission_chooser.h"
#endif

#include "clientsteamcontext.h"

#include "tier1/utldict.h"
#include "keybindinglistener.h"

#ifdef SIXENSE
#include "sixense/in_sixense.h"
#endif

#if defined(_PS3)
#include "buildrenderables_PS3.h"
#endif
#include "viewrender.h"

#include "irendertorthelperobject.h"
#include "iloadingdisc.h"

#include "bannedwords.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IClientMode *GetClientModeNormal();

DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_CONSOLE, "Console" );

// IF YOU ADD AN INTERFACE, EXTERN IT IN THE HEADER FILE.
IVEngineClient	*engine = NULL;
IVModelRender *modelrender = NULL;
IVEfx *effects = NULL;
IVRenderView *render = NULL;
IVDebugOverlay *debugoverlay = NULL;
IMaterialSystemStub *materials_stub = NULL;
IDataCache *datacache = NULL;
IVModelInfoClient *modelinfo = NULL;
IEngineVGui *enginevgui = NULL;
INetworkStringTableContainer *networkstringtable = NULL;
ISpatialPartition* partition = NULL;
IFileSystem *filesystem = NULL;
IShadowMgr *shadowmgr = NULL;
IStaticPropMgrClient *staticpropmgr = NULL;
IEngineSound *enginesound = NULL;
IUniformRandomStream *random = NULL;
static CGaussianRandomStream s_GaussianRandomStream;
CGaussianRandomStream *randomgaussian = &s_GaussianRandomStream;
ISharedGameRules *sharedgamerules = NULL;
IEngineTrace *enginetrace = NULL;
IFileLoggingListener *filelogginglistener = NULL;
IGameUIFuncs *gameuifuncs = NULL;
IGameEventManager2 *gameeventmanager = NULL;
ISoundEmitterSystemBase *soundemitterbase = NULL;
IInputSystem *inputsystem = NULL;
ISceneFileCache *scenefilecache = NULL;
IXboxSystem *xboxsystem = NULL;	// Xbox 360 only
IAvi *avi = NULL;
IBik *bik = NULL;
IQuickTime *pQuicktime = NULL;
IVJobs * g_pVJobs = NULL;
IRenderToRTHelper *g_pRenderToRTHelper = NULL;

#if defined( INCLUDE_SCALEFORM )
IScaleformUI* g_pScaleformUI = NULL;
#endif

IUploadGameStats *gamestatsuploader = NULL;
IBlackBox *blackboxrecorder = NULL;
#ifdef INFESTED_DLL
IASW_Mission_Chooser *missionchooser = NULL;
#endif
#if defined( REPLAY_ENABLED )
IReplayHistoryManager *g_pReplayHistoryManager = NULL;
#endif

AchievementsAndStatsInterface* g_pAchievementsAndStatsInterface = NULL;

IScriptManager *scriptmanager = NULL;

IGameSystem *SoundEmitterSystem();
IGameSystem *ToolFrameworkClientSystem();
IViewRender *GetViewRenderInstance();

DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_VJOBS, "VJOBS" );

bool g_bEngineIsHLTV = false;

static bool g_bRequestCacheUsedMaterials = false;
void RequestCacheUsedMaterials()
{
	g_bRequestCacheUsedMaterials = true;
}

void ProcessCacheUsedMaterials()
{
	if ( !g_bRequestCacheUsedMaterials )
		return;

	g_bRequestCacheUsedMaterials = false;
	if ( materials )
	{
        materials->CacheUsedMaterials();
	}
}

static bool g_bHeadTrackingEnabled = false;

bool IsHeadTrackingEnabled()
{
#if defined( HL2_CLIENT_DLL )
	return g_bHeadTrackingEnabled;
#else
	return false;
#endif
}

void VGui_ClearVideoPanels();

// String tables
INetworkStringTable *g_pStringTableParticleEffectNames = NULL;
INetworkStringTable *g_pStringTableExtraParticleFiles = NULL;
INetworkStringTable *g_StringTableEffectDispatch = NULL;
INetworkStringTable *g_StringTableVguiScreen = NULL;
INetworkStringTable *g_pStringTableMaterials = NULL;
INetworkStringTable *g_pStringTableInfoPanel = NULL;
INetworkStringTable *g_pStringTableClientSideChoreoScenes = NULL;
INetworkStringTable *g_pStringTableMovies = NULL;

static CGlobalVarsBase dummyvars( true );
// So stuff that might reference gpGlobals during DLL initialization won't have a NULL pointer.
// Once the engine calls Init on this DLL, this pointer gets assigned to the shared data in the engine
CGlobalVarsBase *gpGlobals = &dummyvars;
class CHudChat;
class CViewRender;

static C_BaseEntityClassList *s_pClassLists = NULL;
C_BaseEntityClassList::C_BaseEntityClassList()
{
	m_pNextClassList = s_pClassLists;
	s_pClassLists = this;
}
C_BaseEntityClassList::~C_BaseEntityClassList()
{
}

// Any entities that want an OnDataChanged during simulation register for it here.
class CDataChangedEvent
{
public:
	CDataChangedEvent() {}
	CDataChangedEvent( IClientNetworkable *ent, DataUpdateType_t updateType, int *pStoredEvent )
	{
		if ( ent != NULL )
		{
			m_nEntityIndex = ent->entindex();
		}
		else
		{
			m_nEntityIndex = -1;
		}

		m_UpdateType = updateType;
		m_pStoredEvent = pStoredEvent;
	}

	IClientNetworkable *GetEntity()
	{
		if ( m_nEntityIndex == -1 )
			return NULL;

		return ClientEntityList().GetClientNetworkable( m_nEntityIndex );
	}

	int                  m_nEntityIndex;
	DataUpdateType_t	m_UpdateType;
	int					*m_pStoredEvent;
};

ISaveRestoreBlockHandler *GetEntitySaveRestoreBlockHandler();
ISaveRestoreBlockHandler *GetViewEffectsRestoreBlockHandler();
ISaveRestoreBlockHandler *GetGameInstructorRestoreBlockHandler();

CUtlLinkedList<CDataChangedEvent, unsigned short> g_DataChangedEvents;
CUtlLinkedList<CDataChangedEvent, unsigned short> g_DataChangedPostEvents;
bool g_bDataChangedPostEventsAllowed = false;


class IMoveHelper;

void DispatchHudText( const char *pszName );

static ConVar s_CV_ShowParticleCounts("showparticlecounts", "0", 0, "Display number of particles drawn per frame");
// static ConVar s_cl_team("cl_team", "default", FCVAR_ARCHIVE, "Default team when joining a game");
// static ConVar s_cl_class("cl_class", "default", FCVAR_ARCHIVE, "Default class when joining a game");
static ConVar cl_join_advertise( "cl_join_advertise", "1", FCVAR_ARCHIVE, "Advertise joinable game in progress to Steam friends, otherwise need a Steam invite (2: all servers, 1: official servers, 0: none)" );

// Physics system
bool g_bLevelInitialized;
bool g_bTextMode = false;
ClientFrameStage_t g_CurFrameStage = FRAME_UNDEFINED;

static ConVar *g_pcv_ThreadMode = NULL;

// implements ACTIVE_SPLITSCREEN_PLAYER_GUARD (cdll_client_int.h)
CSetActiveSplitScreenPlayerGuard::CSetActiveSplitScreenPlayerGuard( char const *pchContext, int nLine ) :
	CVGuiScreenSizeSplitScreenPlayerGuard( false, engine->GetActiveSplitScreenPlayerSlot(), engine->GetActiveSplitScreenPlayerSlot() )
{
	m_bChanged = true;
	m_pchContext = pchContext;
	m_nLine = nLine;
	m_nSaveSlot = engine->SetActiveSplitScreenPlayerSlot( -1 );
	m_bSaveGetLocalPlayerAllowed = engine->SetLocalPlayerIsResolvable( pchContext, nLine, false );
}

CSetActiveSplitScreenPlayerGuard::CSetActiveSplitScreenPlayerGuard( char const *pchContext, int nLine, int slot, int nOldSlot, bool bSetVguiScreenSize ) :
	CVGuiScreenSizeSplitScreenPlayerGuard( bSetVguiScreenSize, slot, nOldSlot )
{
	if ( nOldSlot == slot && engine->IsLocalPlayerResolvable() )
	{
		m_bChanged = false;
		return;
	}

	m_bChanged = true;
	m_pchContext = pchContext;
	m_nLine = nLine;
	m_nSaveSlot = engine->SetActiveSplitScreenPlayerSlot( slot >= 0 ? slot : 0 );
	m_bSaveGetLocalPlayerAllowed = engine->SetLocalPlayerIsResolvable( pchContext, nLine, slot >= 0 );
}

CSetActiveSplitScreenPlayerGuard::CSetActiveSplitScreenPlayerGuard( char const *pchContext, int nLine, C_BaseEntity *pEntity, int nOldSlot, bool bSetVguiScreenSize ) :
	CVGuiScreenSizeSplitScreenPlayerGuard( bSetVguiScreenSize, pEntity, nOldSlot )
{
	int slot = C_BasePlayer::GetSplitScreenSlotForPlayer( pEntity );
	if ( slot == -1 )
	{
		m_bChanged = false;
		return;
	}

	if ( nOldSlot == slot && engine->IsLocalPlayerResolvable())
	{
		m_bChanged = false;
		return;
	}

	m_bChanged = true;
	m_pchContext = pchContext;
	m_nLine = nLine;
	m_nSaveSlot = engine->SetActiveSplitScreenPlayerSlot( slot >= 0 ? slot : 0 );
	m_bSaveGetLocalPlayerAllowed = engine->SetLocalPlayerIsResolvable( pchContext, nLine, slot >= 0 );
}


CSetActiveSplitScreenPlayerGuard::~CSetActiveSplitScreenPlayerGuard()
{
	if ( !m_bChanged )
		return;

	engine->SetActiveSplitScreenPlayerSlot( m_nSaveSlot );
	engine->SetLocalPlayerIsResolvable( m_pchContext, m_nLine, m_bSaveGetLocalPlayerAllowed );
}

static CUtlRBTree< const char *, int > g_Hacks( 0, 0, DefLessFunc( char const * ) );

CON_COMMAND( cl_dumpsplithacks, "Dump split screen workarounds." )
{
	for ( int i = g_Hacks.FirstInorder(); i != g_Hacks.InvalidIndex(); i = g_Hacks.NextInorder( i ) )
	{
		Msg( "%s\n", g_Hacks[ i ] );
	}
}

CHackForGetLocalPlayerAccessAllowedGuard::CHackForGetLocalPlayerAccessAllowedGuard( char const *pszContext, bool bOldState )
{
	if ( bOldState )
	{
		m_bChanged = false;
		return;
	}

	m_bChanged = true;
	m_pszContext = pszContext;
	if ( g_Hacks.Find( pszContext ) == g_Hacks.InvalidIndex() )
	{
		g_Hacks.Insert( pszContext );
	}
	m_bSaveGetLocalPlayerAllowed = engine->SetLocalPlayerIsResolvable( pszContext, 0, true );
}

CHackForGetLocalPlayerAccessAllowedGuard::~CHackForGetLocalPlayerAccessAllowedGuard()
{
	if ( !m_bChanged )
		return;
	engine->SetLocalPlayerIsResolvable( m_pszContext, 0, m_bSaveGetLocalPlayerAllowed );
}

CVGuiScreenSizeSplitScreenPlayerGuard::CVGuiScreenSizeSplitScreenPlayerGuard( bool bActive, int slot, int nOldSlot )
{
	if ( !bActive )
	{
		m_bNoRestore = true;
		return;
	}

	if ( vgui::surface()->IsScreenSizeOverrideActive() && nOldSlot == slot && engine->IsLocalPlayerResolvable() )
	{
		m_bNoRestore = true;
		return;
	}

	m_bNoRestore = false;
	vgui::surface()->GetScreenSize( m_nOldSize[ 0 ], m_nOldSize[ 1 ] );
	int x, y, w, h;
	VGui_GetHudBounds( slot >= 0 ? slot : 0, x, y, w, h );
	m_bOldSetting = vgui::surface()->ForceScreenSizeOverride( true, w, h );
}

CVGuiScreenSizeSplitScreenPlayerGuard::CVGuiScreenSizeSplitScreenPlayerGuard( bool bActive, C_BaseEntity *pEntity, int nOldSlot )
{
	if ( !bActive )
	{
		m_bNoRestore = true;
		return;
	}

	int slot = C_BasePlayer::GetSplitScreenSlotForPlayer( pEntity );
	if ( vgui::surface()->IsScreenSizeOverrideActive() && nOldSlot == slot && engine->IsLocalPlayerResolvable() )
	{
		m_bNoRestore = true;
		return;
	}

	m_bNoRestore = false;
	vgui::surface()->GetScreenSize( m_nOldSize[ 0 ], m_nOldSize[ 1 ] );
	// Get size for this user
	int x, y, w, h;
	VGui_GetHudBounds( slot >= 0 ? slot : 0, x, y, w, h );
	m_bOldSetting = vgui::surface()->ForceScreenSizeOverride( true, w, h );
}

CVGuiScreenSizeSplitScreenPlayerGuard::~CVGuiScreenSizeSplitScreenPlayerGuard()
{
	if ( m_bNoRestore )
		return;
	vgui::surface()->ForceScreenSizeOverride( m_bOldSetting, m_nOldSize[ 0 ], m_nOldSize[ 1 ] );
}

CVGuiAbsPosSplitScreenPlayerGuard::CVGuiAbsPosSplitScreenPlayerGuard( int slot, int nOldSlot, bool bInvert /*=false*/ )
{
	if ( nOldSlot == slot && engine->IsLocalPlayerResolvable() && vgui::surface()->IsScreenPosOverrideActive() )
	{
		m_bNoRestore = true;
		return;
	}

	m_bNoRestore = false;

	// Get size for this user
	int x, y, w, h;
	VGui_GetHudBounds( slot, x, y, w, h );
	if ( bInvert )
	{
		x = -x;
		y = -y;
	}

	vgui::surface()->ForceScreenPosOffset( true, x, y );
}

CVGuiAbsPosSplitScreenPlayerGuard::~CVGuiAbsPosSplitScreenPlayerGuard()
{
	if ( m_bNoRestore )
		return;
	vgui::surface()->ForceScreenPosOffset( false, 0, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: interface for gameui to modify voice bans
//-----------------------------------------------------------------------------
class CGameClientExports : public IGameClientExports
{
public:
	// ingame voice manipulation
	bool IsPlayerGameVoiceMuted(int playerIndex)
	{
		return GetClientVoiceMgr()->IsPlayerBlocked(playerIndex);
	}

	void MutePlayerGameVoice(int playerIndex)
	{
		if ( !GetClientVoiceMgr()->IsPlayerBlocked( playerIndex ) )
		{
			GetClientVoiceMgr()->SetPlayerBlockedState( playerIndex );
		}
	}

	void UnmutePlayerGameVoice(int playerIndex)
	{
		if ( GetClientVoiceMgr()->IsPlayerBlocked( playerIndex ) )
		{
			GetClientVoiceMgr()->SetPlayerBlockedState( playerIndex );
		}
	}
	

	void OnGameUIActivated( void )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "gameui_activated" );
		if ( event )
		{
			gameeventmanager->FireEventClientSide( event );
		}
	}

	void OnGameUIHidden( void )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "gameui_hidden" );
		if ( event )
		{
			gameeventmanager->FireEventClientSide( event );
		}
	}

	// if true, the gameui applies the blur effect
	bool ClientWantsBlurEffect( void )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
		if ( GetViewPortInterface()->GetActivePanel() && GetViewPortInterface()->GetActivePanel()->WantsBackgroundBlurred() )
			return true;

		return false;
	}
	
    void CreateAchievementsPanel( vgui::Panel* pParent )
    {
        if (g_pAchievementsAndStatsInterface)
        {
            g_pAchievementsAndStatsInterface->CreatePanel( pParent );
        }
    }

    void DisplayAchievementPanel()
    {
        if (g_pAchievementsAndStatsInterface)
        {
            g_pAchievementsAndStatsInterface->DisplayPanel();
        }
    }

    void ShutdownAchievementPanel()
    {
        if (g_pAchievementsAndStatsInterface)
        {
            g_pAchievementsAndStatsInterface->ReleasePanel();
        }
    }

	int GetAchievementsPanelMinWidth( void ) const
	{
        if ( g_pAchievementsAndStatsInterface )
        {
            return g_pAchievementsAndStatsInterface->GetAchievementsPanelMinWidth();
        }

		return 0;
	}
};

EXPOSE_SINGLE_INTERFACE( CGameClientExports, IGameClientExports, GAMECLIENTEXPORTS_INTERFACE_VERSION );

class CClientDLLSharedAppSystems : public IClientDLLSharedAppSystems
{
public:
	CClientDLLSharedAppSystems()
	{
		AddAppSystem( "soundemittersystem" , SOUNDEMITTERSYSTEM_INTERFACE_VERSION );
		AddAppSystem( "scenefilecache", SCENE_FILE_CACHE_INTERFACE_VERSION );
		
#ifdef GAMEUI_UISYSTEM2_ENABLED
		AddAppSystem( "client", GAMEUISYSTEMMGR_INTERFACE_VERSION );
#endif
#ifdef INFESTED_DLL
		AddAppSystem( "missionchooser", ASW_MISSION_CHOOSER_VERSION );
#endif
	}

	virtual int	Count()
	{
		return m_Systems.Count();
	}
	virtual char const *GetDllName( int idx )
	{
		return m_Systems[ idx ].m_pModuleName;
	}
	virtual char const *GetInterfaceName( int idx )
	{
		return m_Systems[ idx ].m_pInterfaceName;
	}
private:
	void AddAppSystem( char const *moduleName, char const *interfaceName )
	{
		AppSystemInfo_t sys;
		sys.m_pModuleName = moduleName;
		sys.m_pInterfaceName = interfaceName;
		m_Systems.AddToTail( sys );
	}

	CUtlVector< AppSystemInfo_t >	m_Systems;
};

EXPOSE_SINGLE_INTERFACE( CClientDLLSharedAppSystems, IClientDLLSharedAppSystems, CLIENT_DLL_SHARED_APPSYSTEMS );


//-----------------------------------------------------------------------------
// Helper interface for voice.
//-----------------------------------------------------------------------------
class CHLVoiceStatusHelper : public IVoiceStatusHelper
{
public:
	virtual void GetPlayerTextColor(int entindex, int color[3])
	{
		color[0] = color[1] = color[2] = 128;
	}

	virtual void UpdateCursorState()
	{
	}

	virtual bool			CanShowSpeakerLabels()
	{
		return true;
	}
};
static CHLVoiceStatusHelper g_VoiceStatusHelper;

//-----------------------------------------------------------------------------
// Code to display which entities are having their bones setup each frame.
//-----------------------------------------------------------------------------

ConVar cl_ShowBoneSetupEnts( "cl_ShowBoneSetupEnts", "0", 0, "Show which entities are having their bones setup each frame." );

class CBoneSetupEnt
{
public:
	char m_ModelName[128];
	int m_Index;
	int m_Count;
};

bool BoneSetupCompare( const CBoneSetupEnt &a, const CBoneSetupEnt &b )
{
	return a.m_Index < b.m_Index;
}

CUtlRBTree<CBoneSetupEnt> g_BoneSetupEnts( BoneSetupCompare );


void TrackBoneSetupEnt( C_BaseAnimating *pEnt )
{
#ifdef _DEBUG
	if ( !cl_ShowBoneSetupEnts.GetInt() )
		return;

	CBoneSetupEnt ent;
	ent.m_Index = pEnt->entindex();
	unsigned short i = g_BoneSetupEnts.Find( ent );
	if ( i == g_BoneSetupEnts.InvalidIndex() )
	{
		Q_strncpy( ent.m_ModelName, modelinfo->GetModelName( pEnt->GetModel() ), sizeof( ent.m_ModelName ) );
		ent.m_Count = 1;
		g_BoneSetupEnts.Insert( ent );
	}
	else
	{
		g_BoneSetupEnts[i].m_Count++;
	}
#endif
}

void DisplayBoneSetupEnts()
{
#ifdef _DEBUG
	if ( !cl_ShowBoneSetupEnts.GetInt() )
		return;

	unsigned short i;
	int nElements = 0;
	for ( i=g_BoneSetupEnts.FirstInorder(); i != g_BoneSetupEnts.LastInorder(); i=g_BoneSetupEnts.NextInorder( i ) )
		++nElements;
		
	engine->Con_NPrintf( 0, "%d bone setup ents (name/count/entindex) ------------", nElements );

	con_nprint_s printInfo;
	printInfo.time_to_live = -1;
	printInfo.fixed_width_font = true;
	printInfo.color[0] = printInfo.color[1] = printInfo.color[2] = 1;
	
	printInfo.index = 2;
	for ( i=g_BoneSetupEnts.FirstInorder(); i != g_BoneSetupEnts.LastInorder(); i=g_BoneSetupEnts.NextInorder( i ) )
	{
		CBoneSetupEnt *pEnt = &g_BoneSetupEnts[i];
		
		if ( pEnt->m_Count >= 3 )
		{
			printInfo.color[0] = 1;
			printInfo.color[1] = printInfo.color[2] = 0;
		}
		else if ( pEnt->m_Count == 2 )
		{
			printInfo.color[0] = (float)200 / 255;
			printInfo.color[1] = (float)220 / 255;
			printInfo.color[2] = 0;
		}
		else
		{
			printInfo.color[0] = printInfo.color[0] = printInfo.color[0] = 1;
		}
		engine->Con_NXPrintf( &printInfo, "%25s / %3d / %3d", pEnt->m_ModelName, pEnt->m_Count, pEnt->m_Index );
		printInfo.index++;
	}

	g_BoneSetupEnts.RemoveAll();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: engine to client .dll interface
//-----------------------------------------------------------------------------
class CHLClient : public IBaseClientDLL
{
public:
	CHLClient();

	virtual int						Connect( CreateInterfaceFn appSystemFactory, CGlobalVarsBase *pGlobals );
	virtual void                    Disconnect();
	virtual int						Init( CreateInterfaceFn appSystemFactory, CGlobalVarsBase *pGlobals );

	virtual void					PostInit();
	virtual void					Shutdown( void );

	virtual void					LevelInitPreEntity( const char *pMapName );
	virtual void					LevelInitPostEntity();
	virtual void					LevelShutdown( void );

	virtual ClientClass				*GetAllClasses( void );

	virtual int						HudVidInit( void );
	virtual void					HudProcessInput( bool bActive );
	virtual void					HudUpdate( bool bActive );
	virtual void					HudReset( void );
	virtual void					HudText( const char * message );

	// Mouse Input Interfaces
	virtual void					IN_ActivateMouse( void );
	virtual void					IN_DeactivateMouse( void );
	virtual void					IN_Accumulate( void );
	virtual void					IN_ClearStates( void );
	virtual bool					IN_IsKeyDown( const char *name, bool& isdown );
	// Raw signal
	virtual int						IN_KeyEvent( int eventcode, ButtonCode_t keynum, const char *pszCurrentBinding );
	virtual void					IN_SetSampleTime( float frametime );
	// Create movement command
	virtual void					CreateMove ( int sequence_number, float input_sample_frametime, bool active );
	virtual void					ExtraMouseSample( float frametime, bool active );
	virtual bool					WriteUsercmdDeltaToBuffer( int nSlot, bf_write *buf, int from, int to, bool isnewcommand );	
	virtual void					EncodeUserCmdToBuffer( int nSlot, bf_write& buf, int slot );
	virtual void					DecodeUserCmdFromBuffer( int nSlot, bf_read& buf, int slot );


	virtual void					View_Render( vrect_t *rect );
	virtual void					RenderView( const CViewSetup &view, int nClearFlags, int whatToDraw );
	virtual void					View_Fade( ScreenFade_t *pSF );
	
	virtual void					SetCrosshairAngle( const QAngle& angle );

	virtual void					InitSprite( CEngineSprite *pSprite, const char *loadname );
	virtual void					ShutdownSprite( CEngineSprite *pSprite );

	virtual int						GetSpriteSize( void ) const;

	virtual void					VoiceStatus( int entindex, int iSsSlot, qboolean bTalking );
	
	virtual bool					PlayerAudible( int iPlayerIndex );

	virtual void					InstallStringTableCallback( const char *tableName );

	virtual void					FrameStageNotify( ClientFrameStage_t curStage );

	virtual bool					DispatchUserMessage( int msg_type, int32 nFlags, int size, const void *msg );

	// Save/restore system hooks
	virtual CSaveRestoreData  *SaveInit( int size );
	virtual void			SaveWriteFields( CSaveRestoreData *, const char *, void *, datamap_t *, typedescription_t *, int );
	virtual void			SaveReadFields( CSaveRestoreData *, const char *, void *, datamap_t *, typedescription_t *, int );
	virtual void			PreSave( CSaveRestoreData * );
	virtual void			Save( CSaveRestoreData * );
	virtual void			WriteSaveHeaders( CSaveRestoreData * );
	virtual void			ReadRestoreHeaders( CSaveRestoreData * );
	virtual void			Restore( CSaveRestoreData *, bool );
	virtual void			DispatchOnRestore();
	virtual void			WriteSaveGameScreenshot( const char *pFilename );

	// Given a list of "S(wavname) S(wavname2)" tokens, look up the localized text and emit
	//  the appropriate close caption if running with closecaption = 1
	virtual void			EmitSentenceCloseCaption( char const *tokenstream );
	virtual void			EmitCloseCaption( char const *captionname, float duration );

	virtual CStandardRecvProxies* GetStandardRecvProxies();

	virtual bool			CanRecordDemo( char *errorMsg, int length ) const;
	virtual bool			CanStopRecordDemo( char *errorMsg, int length ) const;

	virtual void			OnDemoRecordStart( char const* pDemoBaseName );
	virtual void			OnDemoRecordStop();
	virtual void			OnDemoPlaybackStart( char const* pDemoBaseName );
	virtual void			OnDemoPlaybackRestart();
	virtual void			OnDemoPlaybackStop();
	virtual void			SetDemoPlaybackHighlightXuid( uint64 xuid, bool bLowlights );
	virtual void			ShowHighlightSkippingMessage( bool bState, int nCurrentTick = 0, int nTickStart = 0, int nTickStop = 0 );

	virtual void			RecordDemoPolishUserInput( int nCmdIndex );

	// Cache replay ragdolls
	virtual bool			CacheReplayRagdolls( const char* pFilename, int nStartTick );

	// Send a message to the replay browser
	virtual void			ReplayUI_SendMessage( KeyValues *pMsg );

	// Get the client replay factory
	virtual IReplayFactory *GetReplayFactory();

	// Clear out the local player's replay pointer so it doesn't get deleted
	virtual void			ClearLocalPlayerReplayPtr();

	virtual bool			ShouldDrawDropdownConsole();

	// Get client screen dimensions
	virtual int				GetScreenWidth();
	virtual int				GetScreenHeight();

	// save game screenshot writing
	virtual void			WriteSaveGameScreenshotOfSize( const char *pFilename, int width, int height, bool bCreatePowerOf2Padded/*=false*/, bool bWriteVTF/*=false*/ );

	// Write a .VTF screenshot to disk for the replay system
	virtual void			WriteReplayScreenshot( WriteReplayScreenshotParams_t &params );

	// Reallocate memory for replay screenshots - called if user changes resolution or if the convar "replay_screenshotresolution" changes
	virtual void			UpdateReplayScreenshotCache();

	// Gets the location of the player viewpoint
	virtual bool			GetPlayerView( CViewSetup &playerView );

	virtual bool			ShouldHideLoadingPlaque( void );

	virtual void			InvalidateMdlCache();

	virtual void			OnActiveSplitscreenPlayerChanged( int nNewSlot );
	virtual void			OnSplitScreenStateChanged();
	virtual int 			GetSpectatorTarget( ClientDLLObserverMode_t* pObserverMode );
	virtual void			CenterStringOff();


	virtual void			OnScreenSizeChanged( int nOldWidth, int nOldHeight );
	virtual IMaterialProxy *InstantiateMaterialProxy( const char *proxyName );

	virtual vgui::VPANEL	GetFullscreenClientDLLVPanel( void );
	virtual void			MarkEntitiesAsTouching( IClientEntity *e1, IClientEntity *e2 );
	virtual void			OnKeyBindingChanged( ButtonCode_t buttonCode, char const *pchKeyName, char const *pchNewBinding );
	virtual bool			HandleGameUIEvent( const InputEvent_t &event );
	
	virtual bool			GetSoundSpatialization( SpatializationInfo_t& info );

public:
	void					PrecacheMaterial( const char *pMaterialName );
	void					PrecacheMovie( const char *pMovieName );

	virtual void			SetBlurFade( float scale );
	
	virtual void			ResetHudCloseCaption();

	virtual void			Hud_SaveStarted();

	virtual void			ShutdownMovies();

	virtual void			GetStatus( char *buffer, int bufsize );

#if defined ( CSTRIKE15 )
	virtual bool			IsChatRaised( void );
	virtual bool			IsRadioPanelRaised( void );
	virtual bool			IsBindMenuRaised( void );
	virtual bool			IsTeamMenuRaised( void );
	virtual bool			IsLoadingScreenRaised( void );
#endif

#if defined(_PS3)
	virtual int				GetDrawFlags( void );
	virtual int				GetBuildViewID( void );
	virtual bool			IsSPUBuildWRJobsOn( void );
	virtual void			CacheFrustumData( Frustum_t *pFrustum, Frustum_t *pAreaFrustum, void *pRenderAreaBits, int numArea, bool bViewerInSolidSpace );
	virtual void			*GetBuildViewVolumeCuller( void );
	virtual Frustum_t		*GetBuildViewFrustum( void );
	virtual Frustum_t		*GetBuildViewAreaFrustum( void );
	virtual unsigned char	*GetBuildViewRenderAreaBits( void );
#else
	virtual bool			IsBuildWRThreaded( void );
	virtual void			QueueBuildWorldListJob( CJob* pJob );
	virtual void			CacheFrustumData( const Frustum_t& frustum, const CUtlVector< Frustum_t, CUtlMemoryAligned< Frustum_t,16 > >& aeraFrustums );
	virtual const Frustum_t* GetBuildViewFrustum( void ) const;
	virtual const CUtlVector< Frustum_t, CUtlMemoryAligned< Frustum_t,16 > >* GetBuildViewAeraFrustums( void ) const;
#endif

	virtual bool IsSubscribedMap( const char *pchMapName, bool bOnlyOnDisk );
	virtual bool IsFeaturedMap( const char *pchMapName, bool bOnlyOnDisk );

	virtual void DownloadCommunityMapFile( PublishedFileId_t id );
	virtual float GetUGCFileDownloadProgress( PublishedFileId_t id );

	virtual void RecordUIEvent( const char* szEvent );
	virtual void OnHltvReplay( const CSVCMsg_HltvReplay  &msg ) OVERRIDE { g_HltvReplaySystem.OnHltvReplay( msg ); }
	virtual void OnHltvReplayTick() OVERRIDE { g_HltvReplaySystem.OnHltvReplayTick(); }
	virtual int GetHltvReplayDelay() OVERRIDE { return g_HltvReplaySystem.GetHltvReplayDelay(); }
	virtual void OnDemoPlaybackTimeJump();

	// Inventory access
	virtual float FindInventoryItemWithMaxAttributeValue( char const *szItemType, char const *szAttrClass );
	virtual void DetermineSubscriptionKvToAdvertise( KeyValues *kvLocalPlayer );

	// Overwatchsupport for engine
	virtual bool ValidateSignedEvidenceHeader( char const *szKey, void const *pvHeader, CDemoPlaybackParameters_t *pPlaybackParameters );
	virtual void PrepareSignedEvidenceData( void *pvData, int numBytes, CDemoPlaybackParameters_t const *pPlaybackParameters );
	virtual bool ShouldSkipEvidencePlayback( CDemoPlaybackParameters_t const *pPlaybackParameters );

	// Scaleform slot controller
	virtual IScaleformSlotInitController * GetScaleformSlotInitController();

	virtual bool IsConnectedUserInfoChangeAllowed( IConVar *pCvar );
	virtual void OnCommandDuringPlayback( char const *cmd );

	virtual void RetireAllPlayerDecals( bool bRenderContextValid );

	virtual void EngineGotvSyncPacket( const CEngineGotvSyncPacket *pPkt ) OVERRIDE;

	virtual void OnTickPre( int tickcount ) OVERRIDE;

	virtual char const * GetRichPresenceStatusString();

	virtual int GetInEyeEntity() const;

private:
	void					UncacheAllMaterials();
	void					UncacheAllMovies();
	void					ResetStringTablePointers();

	CUtlRBTree< IMaterial * >	m_CachedMaterials;
	CUtlSymbolTable				m_CachedMovies;

	CHudCloseCaption		*m_pHudCloseCaption;
};


CHLClient gHLClient;
IBaseClientDLL *clientdll = &gHLClient;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CHLClient, IBaseClientDLL, CLIENT_DLL_INTERFACE_VERSION, gHLClient );


//-----------------------------------------------------------------------------
// Precaches a material
//-----------------------------------------------------------------------------
void PrecacheMaterial( const char *pMaterialName )
{
	gHLClient.PrecacheMaterial( pMaterialName );
}

//-----------------------------------------------------------------------------
// Converts a previously precached material into an index
//-----------------------------------------------------------------------------
int GetMaterialIndex( const char *pMaterialName )
{
	if (pMaterialName)
	{
		int nIndex = g_pStringTableMaterials->FindStringIndex( pMaterialName );
		Assert( nIndex >= 0 );
		if (nIndex >= 0)
			return nIndex;
	}

	// This is the invalid string index
	return 0;
}

//-----------------------------------------------------------------------------
// Converts precached material indices into strings
//-----------------------------------------------------------------------------
const char *GetMaterialNameFromIndex( int nIndex )
{
	if (nIndex != (g_pStringTableMaterials->GetMaxStrings() - 1))
	{
		return g_pStringTableMaterials->GetString( nIndex );
	}
	else
	{
		return NULL;
	}
}

//-----------------------------------------------------------------------------
// Precaches a movie
//-----------------------------------------------------------------------------
void PrecacheMovie( const char *pMovieName )
{
	gHLClient.PrecacheMovie( pMovieName );
}

//-----------------------------------------------------------------------------
// Converts a previously precached movie into an index
//-----------------------------------------------------------------------------
int GetMovieIndex( const char *pMovieName )
{
	if ( pMovieName )
	{
		int nIndex = g_pStringTableMovies->FindStringIndex( pMovieName );
		Assert( nIndex >= 0 );
		if ( nIndex >= 0 )
		{
			return nIndex;
		}
	}

	// This is the invalid string index
	return 0;
}

//-----------------------------------------------------------------------------
// Converts precached movie indices into strings
//-----------------------------------------------------------------------------
const char *GetMovieNameFromIndex( int nIndex )
{
	if ( nIndex != ( g_pStringTableMovies->GetMaxStrings() - 1 ) )
	{
		return g_pStringTableMovies->GetString( nIndex );
	}
	else
	{
		return NULL;
	}
}


//-----------------------------------------------------------------------------
// Precaches a particle system
//-----------------------------------------------------------------------------
int PrecacheParticleSystem( const char *pParticleSystemName )
{
	int nIndex = g_pStringTableParticleEffectNames->AddString( false, pParticleSystemName );
	g_pParticleSystemMgr->PrecacheParticleSystem( nIndex, pParticleSystemName );
	return nIndex;
}


//-----------------------------------------------------------------------------
// Converts a previously precached particle system into an index
//-----------------------------------------------------------------------------
int GetParticleSystemIndex( const char *pParticleSystemName )
{
	if ( pParticleSystemName )
	{
		int nIndex = g_pStringTableParticleEffectNames->FindStringIndex( pParticleSystemName );
		if ( nIndex != INVALID_STRING_INDEX )
			return nIndex;
		DevWarning("Client: Missing precache for particle system \"%s\"!\n", pParticleSystemName );
	}

	// This is the invalid string index
	return 0;
}

//-----------------------------------------------------------------------------
// Converts precached particle system indices into strings
//-----------------------------------------------------------------------------
const char *GetParticleSystemNameFromIndex( int nIndex )
{
	if ( nIndex < g_pStringTableParticleEffectNames->GetMaxStrings() )
		return g_pStringTableParticleEffectNames->GetString( nIndex );
	return "error";
}


//-----------------------------------------------------------------------------
// Precache-related methods for effects
//-----------------------------------------------------------------------------
void PrecacheEffect( const char *pEffectName )
{
	// Bring in dependent resources
	g_pPrecacheSystem->Cache( g_pPrecacheHandler, DISPATCH_EFFECT, pEffectName, true, RESOURCE_LIST_INVALID, true );
}


//-----------------------------------------------------------------------------
// Returns true if host_thread_mode is set to non-zero (and engine is running in threaded mode)
//-----------------------------------------------------------------------------
bool IsEngineThreaded()
{
	if ( g_pcv_ThreadMode )
	{
		return g_pcv_ThreadMode->GetBool();
	}
	return false;
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CHLClient::CHLClient() 
{
	// Kinda bogus, but the logic in the engine is too convoluted to put it there
	g_bLevelInitialized = false;
	m_pHudCloseCaption = NULL;

	SetDefLessFunc( m_CachedMaterials );
}


extern IGameSystem *ViewportClientSystem();

// enable threaded init functions on x360
static ConVar cl_threaded_init("cl_threaded_init", IsGameConsole() ? "1" : "0");

bool InitParticleManager()
{
	if (!ParticleMgr()->Init(MAX_TOTAL_PARTICLES, materials))
		return false;

	return true;
}

CEG_NOINLINE bool InitGameSystems( CreateInterfaceFn appSystemFactory )
{
	CEG_ENCRYPT_FUNCTION( InitGameSystems );

	if (!VGui_Startup( appSystemFactory ))
		return false;

	// Keep the spinner going.
	materials->RefreshFrontBufferNonInteractive();

	vgui::VGui_InitMatSysInterfacesList( "ClientDLL", &appSystemFactory, 1 );

	// Keep the spinner going.
	materials->RefreshFrontBufferNonInteractive();

	// Add the client systems.	

	// Client Leaf System has to be initialized first, since DetailObjectSystem uses it
	IGameSystem::Add( GameStringSystem() );
	IGameSystem::Add( g_pPrecacheRegister );
	IGameSystem::Add( SoundEmitterSystem() );
	IGameSystem::Add( ToolFrameworkClientSystem() );
	IGameSystem::Add( ClientLeafSystem() );
	IGameSystem::Add( DetailObjectSystem() );
	IGameSystem::Add( ViewportClientSystem() );
	IGameSystem::Add( g_pClientShadowMgr );
	IGameSystem::Add( g_pColorCorrectionMgr );
#ifdef GAMEUI_UISYSTEM2_ENABLED
	IGameSystem::Add( g_pGameUIGameSystem );
#endif
	IGameSystem::Add( ClientThinkList() );
	IGameSystem::Add( ClientSoundscapeSystem() );
	IGameSystem::Add( PerfVisualBenchmark() );
	IGameSystem::Add( MumbleSystem() );

#if defined( CLIENT_DLL ) && defined( COPY_CHECK_STRESSTEST )
	IGameSystem::Add( GetPredictionCopyTester() );
#endif

	ActivityList_Init();
	ActivityList_RegisterSharedActivities();
	EventList_Init();
	EventList_RegisterSharedEvents();

	modemanager->Init( );

	// Load the ClientScheme just once
	vgui::scheme()->LoadSchemeFromFileEx( VGui_GetFullscreenRootVPANEL(), "resource/ClientScheme.res", "ClientScheme");

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		GetClientMode()->InitViewport();

		if ( hh == 0 )
		{
			GetFullscreenClientMode()->InitViewport();
		}
	}

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		GetHud().Init();
	}

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		GetClientMode()->Init();

		if ( hh == 0 )
		{
			GetFullscreenClientMode()->Init();
		}
	}

	if ( !IGameSystem::InitAllSystems() )
		return false;

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		GetClientMode()->Enable();

		if ( hh == 0 )
		{
			GetFullscreenClientMode()->EnableWithRootPanel( VGui_GetFullscreenRootVPANEL() );
		}
	}	

	// Each mod is required to implement this
	view = GetViewRenderInstance();
	if ( !view )
	{
		Error( "GetViewRenderInstance() must be implemented by game." );
	}

	view->Init();
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		GetViewEffects()->Init();
	}

	C_BaseTempEntity::PrecacheTempEnts();

	input->Init_All();

	VGui_CreateGlobalPanels();

	InitSmokeFogOverlay();

	// Register user messages..
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		CUserMessageRegisterBase::RegisterAll();
	}

	ClientVoiceMgr_Init();

	// Embed voice status icons inside chat element
	{
		vgui::VPANEL parent = enginevgui->GetPanel( PANEL_CLIENTDLL );
		GetClientVoiceMgr()->Init( &g_VoiceStatusHelper, parent );
	}

	if ( !PhysicsDLLInit( appSystemFactory ) )
		return false;

	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetEntitySaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetPhysSaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetViewEffectsRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetGameInstructorRestoreBlockHandler() );

	ClientWorldFactoryInit();

	// Keep the spinner going.
	materials->RefreshFrontBufferNonInteractive();

	return true;
}

CON_COMMAND( cl_modemanager_reload, "Reloads the panel metaclasses for vgui screens." )
{
	modemanager->Init();
}

//CEG_PROTECT_FUNCTION( InitGameSystems );

//-----------------------------------------------------------------------------
// Purpose: Called when the DLL is first loaded.
// Input  : engineFactory - 
// Output : int
//-----------------------------------------------------------------------------
int CHLClient::Connect( CreateInterfaceFn appSystemFactory, CGlobalVarsBase *pGlobals )
{
	if( !STEAMWORKS_INITCEGLIBRARY() )
	{
		return false;
	}
	STEAMWORKS_REGISTERTHREAD();

	InitCRTMemDebug();
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

#ifdef SIXENSE
	g_pSixenseInput = new SixenseInput;
#endif

	// Hook up global variables
	gpGlobals = pGlobals;

	ConnectTier1Libraries( &appSystemFactory, 1 );
	ConnectTier2Libraries( &appSystemFactory, 1 );
	ConnectTier3Libraries( &appSystemFactory, 1 );

#if defined( INCLUDE_SCALEFORM )
	g_pScaleformUI = ( IScaleformUI* ) appSystemFactory( SCALEFORMUI_INTERFACE_VERSION, NULL );
#endif

#ifndef NO_STEAM
	#ifndef _PS3
	SteamAPI_InitSafe();
	SteamAPI_SetTryCatchCallbacks( false ); // We don't use exceptions, so tell steam not to use try/catch in callback handlers
	#endif

	ClientSteamContext().Activate();

	if ( !ClientSteamContext().SteamFriends() || !ClientSteamContext().SteamUser() || !ClientSteamContext().SteamUtils() ||
		!ClientSteamContext().SteamNetworking() || !ClientSteamContext().SteamMatchmaking() ||
		!ClientSteamContext().SteamUser()->GetSteamID().IsValid()	// << this is catching the case when Steam client is running, but showing logon username/password screen
		)
	{
		fprintf( stderr, "FATAL ERROR: This game requires latest version of Steam to be running!\nYour Steam Client can be updated using Steam > Check for Steam Client Updates...\n" );
#if IS_WINDOWS_PC || ( defined ( LINUX ) && !defined ( DEDICATED ) )
		Error( "FATAL ERROR: Failed to connect with local Steam Client process!\n\nPlease make sure that you are running latest version of Steam Client.\nYou can check for Steam Client updates using Steam main menu:\n             Steam > Check for Steam Client Updates..." );
#endif
		Plat_ExitProcess( 100 );
		return false;
	}

#endif

	// Initialize the console variables.
	ConVar_Register( FCVAR_CLIENTDLL );

	return true;
}


void CHLClient::Disconnect()
{
	ConVar_Unregister( );

	STEAMWORKS_UNREGISTERTHREAD();
	STEAMWORKS_TERMCEGLIBRARY();
}


int CHLClient::Init( CreateInterfaceFn appSystemFactory, CGlobalVarsBase *pGlobals )
{
	STEAMWORKS_TESTSECRETALWAYS();
	STEAMWORKS_SELFCHECK();

	COM_TimestampedLog( "ClientDLL factories - Start" );
	// We aren't happy unless we get all of our interfaces.
	// please don't collapse this into one monolithic boolean expression (impossible to debug)
	if ( (engine = (IVEngineClient *)appSystemFactory( VENGINE_CLIENT_INTERFACE_VERSION, NULL )) == NULL )
		return false;
	if ( (modelrender = (IVModelRender *)appSystemFactory( VENGINE_HUDMODEL_INTERFACE_VERSION, NULL )) == NULL )
		return false;
	if ( (effects = (IVEfx *)appSystemFactory( VENGINE_EFFECTS_INTERFACE_VERSION, NULL )) == NULL )
		return false;
	if ( (enginetrace = (IEngineTrace *)appSystemFactory( INTERFACEVERSION_ENGINETRACE_CLIENT, NULL )) == NULL )
		return false;
	if ( (filelogginglistener = (IFileLoggingListener *)appSystemFactory(FILELOGGINGLISTENER_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (render = (IVRenderView *)appSystemFactory( VENGINE_RENDERVIEW_INTERFACE_VERSION, NULL )) == NULL )
		return false;
	if ( (debugoverlay = (IVDebugOverlay *)appSystemFactory( VDEBUG_OVERLAY_INTERFACE_VERSION, NULL )) == NULL )
		return false;
	if ( (datacache = (IDataCache*)appSystemFactory(DATACACHE_INTERFACE_VERSION, NULL )) == NULL )
		return false;
	if ( !mdlcache )
		return false;
	if ( (modelinfo = (IVModelInfoClient *)appSystemFactory(VMODELINFO_CLIENT_INTERFACE_VERSION, NULL )) == NULL )
		return false;
	if ( (enginevgui = (IEngineVGui *)appSystemFactory(VENGINE_VGUI_VERSION, NULL )) == NULL )
		return false;
	if ( (networkstringtable = (INetworkStringTableContainer *)appSystemFactory(INTERFACENAME_NETWORKSTRINGTABLECLIENT,NULL)) == NULL )
		return false;
	if ( (::partition = (ISpatialPartition *)appSystemFactory(INTERFACEVERSION_SPATIALPARTITION, NULL)) == NULL )
		return false;
	if ( (shadowmgr = (IShadowMgr *)appSystemFactory(ENGINE_SHADOWMGR_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (staticpropmgr = (IStaticPropMgrClient *)appSystemFactory(INTERFACEVERSION_STATICPROPMGR_CLIENT, NULL)) == NULL )
		return false;
	if ( (enginesound = (IEngineSound *)appSystemFactory(IENGINESOUND_CLIENT_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (filesystem = (IFileSystem *)appSystemFactory(FILESYSTEM_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (random = (IUniformRandomStream *)appSystemFactory(VENGINE_CLIENT_RANDOM_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (gameuifuncs = (IGameUIFuncs * )appSystemFactory( VENGINE_GAMEUIFUNCS_VERSION, NULL )) == NULL )
		return false;
	if ( (gameeventmanager = (IGameEventManager2 *)appSystemFactory(INTERFACEVERSION_GAMEEVENTSMANAGER2,NULL)) == NULL )
		return false;
	if ( (soundemitterbase = (ISoundEmitterSystemBase *)appSystemFactory(SOUNDEMITTERSYSTEM_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (inputsystem = (IInputSystem *)appSystemFactory(INPUTSYSTEM_INTERFACE_VERSION, NULL)) == NULL )
		return false;
#if defined ( AVI_VIDEO )		
	if ( IsPC() && !IsPosix() && (avi = (IAvi *)appSystemFactory(AVI_INTERFACE_VERSION, NULL)) == NULL )
		return false;
#endif
#if ( !defined( _GAMECONSOLE ) || defined( BINK_ENABLED_FOR_CONSOLE ) ) && defined( BINK_VIDEO )
	if ( (bik = (IBik *)appSystemFactory(BIK_INTERFACE_VERSION, NULL)) == NULL )
		return false;
#endif
#if defined( QUICKTIME_VIDEO )
	if ( (pQuicktime = (IQuickTime*)appSystemFactory( QUICKTIME_INTERFACE_VERSION, NULL)) == NULL )
		return false;
#endif
	if ( (scenefilecache = (ISceneFileCache *)appSystemFactory( SCENE_FILE_CACHE_INTERFACE_VERSION, NULL )) == NULL )
		return false;
	if ( (blackboxrecorder = (IBlackBox *)appSystemFactory(BLACKBOX_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (xboxsystem = (IXboxSystem *)appSystemFactory( XBOXSYSTEM_INTERFACE_VERSION, NULL )) == NULL )
		return false;

	if ( (g_pRenderToRTHelper = (IRenderToRTHelper *)appSystemFactory( RENDER_TO_RT_HELPER_INTERFACE_VERSION, NULL )) == NULL )
		return false;
	if ( !g_pRenderToRTHelper->Init() )
		return false;

#if defined( CSTRIKE15 )
	if ( ( g_pGameTypes = (IGameTypes *)appSystemFactory( VENGINE_GAMETYPES_VERSION, NULL )) == NULL )
		return false;

	// load the p4 lib - not doing it in CS:GO to prevent extra .dlls from being loaded
	CSysModule *m_pP4Module = Sys_LoadModule( "p4lib" );
	if ( m_pP4Module )
	{
		CreateInterfaceFn factory = Sys_GetFactory( m_pP4Module );
		if ( factory )
		{
			p4 = ( IP4 * )factory( P4_INTERFACE_VERSION, NULL );

			if ( p4 )
			{
				p4->Connect( appSystemFactory );
				p4->Init();
			}
		}
	}
#endif

#if defined( REPLAY_ENABLED )
	if ( IsPC() && (g_pReplayHistoryManager = (IReplayHistoryManager *)appSystemFactory( REPLAYHISTORYMANAGER_INTERFACE_VERSION, NULL )) == NULL )
		return false;
#endif
#ifndef _GAMECONSOLE
	if ( ( gamestatsuploader = (IUploadGameStats *)appSystemFactory( INTERFACEVERSION_UPLOADGAMESTATS, NULL )) == NULL )
		return false;
#endif
	if (!g_pMatSystemSurface)
		return false;

#ifdef INFESTED_DLL
	if ( (missionchooser = (IASW_Mission_Chooser *)appSystemFactory(ASW_MISSION_CHOOSER_VERSION, NULL)) == NULL )
		return false;
#endif

#ifdef TERROR
	if ( ( g_pMatchExtL4D = ( IMatchExtL4D * ) appSystemFactory( IMATCHEXT_L4D_INTERFACE, NULL ) ) == NULL )
		return false;
	if ( !g_pMatchFramework )
		return false;
//  if client.dll needs to register any matchmaking extensions do it here:
// 	if ( IMatchExtensions *pIMatchExtensions = g_pMatchFramework->GetMatchExtensions() )
// 		pIMatchExtensions->RegisterExtensionInterface(
// 		INTERFACEVERSION_SERVERGAMEDLL, static_cast< IServerGameDLL * >( this ) );
#endif

#ifdef PORTAL2
	if ( !g_pMatchFramework )
		return false;
	GameInstructor_Init();
	//  if client.dll needs to register any matchmaking extensions do it here:
	// 	if ( IMatchExtensions *pIMatchExtensions = g_pMatchFramework->GetMatchExtensions() )
	// 		pIMatchExtensions->RegisterExtensionInterface(
	// 		INTERFACEVERSION_SERVERGAMEDLL, static_cast< IServerGameDLL * >( this ) );
#endif // PORTAL2

#ifdef CSTRIKE15
	if ( !g_pMatchFramework )
		return false;
	GameInstructor_Init();
#endif

	if ( !g_pMatchFramework )
		return false;
	if ( IMatchExtensions *pIMatchExtensions = g_pMatchFramework->GetMatchExtensions() )
		pIMatchExtensions->RegisterExtensionInterface(
		CLIENT_DLL_INTERFACE_VERSION, static_cast< IBaseClientDLL * >( this ) );

#if defined(_PS3)
	// VJOBS is used to spawn jobs on SPUs
	if ( (g_pVJobs = (IVJobs *)appSystemFactory( VJOBS_INTERFACE_VERSION, NULL )) == NULL)
		return false;

	// init client SPURS jobs
	g_pBuildRenderablesJob->Init();
#endif

	if ( !CommandLine()->CheckParm( "-noscripting") )
	{
		scriptmanager = (IScriptManager *)appSystemFactory( VSCRIPT_INTERFACE_VERSION, NULL );
	}

	factorylist_t factories;
	factories.appSystemFactory = appSystemFactory;
	FactoryList_Store( factories );

	COM_TimestampedLog( "soundemitterbase->Connect" );
	// Yes, both the client and game .dlls will try to Connect, the soundemittersystem.dll will handle this gracefully
	if ( !soundemitterbase->Connect( appSystemFactory ) )
	{
		return false;
	}

#if defined( ALLOW_TEXT_MODE )
 	if ( CommandLine()->FindParm( "-textmode" ) )
 		g_bTextMode = true;
#endif

	if ( CommandLine()->FindParm( "-makedevshots" ) )
		g_MakingDevShots = true;

	if ( CommandLine()->FindParm( "-headtracking" ) )
		g_bHeadTrackingEnabled = true;

	// Not fatal if the material system stub isn't around.
	materials_stub = (IMaterialSystemStub*)appSystemFactory( MATERIAL_SYSTEM_STUB_INTERFACE_VERSION, NULL );

	if( !g_pMaterialSystemHardwareConfig )
		return false;

	// Hook up the gaussian random number generator
	s_GaussianRandomStream.AttachToStream( random );

	g_pcv_ThreadMode = g_pCVar->FindVar( "host_thread_mode" );


#if defined( PORTAL2 )
	engine->EnablePaintmapRender();
#endif

	COM_TimestampedLog( "InitGameSystems" );

	bool bInitSuccess = false;
	if ( cl_threaded_init.GetBool() )
	{
		CFunctorJob *pGameJob = new CFunctorJob( CreateFunctor( InitParticleManager ) );
		g_pThreadPool->AddJob( pGameJob );
		bInitSuccess = InitGameSystems( appSystemFactory );

		// While the InitParticleManager job isn't finished, try to update the spinner
		while( !pGameJob->IsFinished() )
		{
			g_pMaterialSystem->RefreshFrontBufferNonInteractive();
			
			// Don't just update as fast as we can
			ThreadSleep( 50 );
		}
		pGameJob->Release();
	}
	else
	{
		COM_TimestampedLog( "ParticleMgr()->Init" );
		if (!ParticleMgr()->Init(MAX_TOTAL_PARTICLES, materials))
			return false;
		COM_TimestampedLog( "InitGameSystems - Start" );
		bInitSuccess = InitGameSystems( appSystemFactory );
		COM_TimestampedLog( "InitGameSystems - End" );
	}


#ifdef INFESTED_PARTICLES	// let the emitter cache load in our standard
	g_ASWGenericEmitterCache.PrecacheTemplates();
#endif

	COM_TimestampedLog( "C_BaseAnimating::InitBoneSetupThreadPool" );

	C_BaseAnimating::InitBoneSetupThreadPool();

	// This is a fullscreen element, so only lives on slot 0!!!
	m_pHudCloseCaption = GET_FULLSCREEN_HUDELEMENT( CHudCloseCaption );

#if defined( PORTAL2_PUZZLEMAKER )
	// This must be called after all other VGui initialization (i.e after InitGameSystems)
	g_PuzzleMaker.Init();
#endif // PORTAL2_PUZZLEMAKER

#if defined( CSTRIKE15 )
	// Load the game types.
	g_pGameTypes->Initialize();
#endif

	//
	// Censoring banlist loads here
	//
	bool bLoadBannedWords = !!CommandLine()->FindParm( "-perfectworld" );
	bLoadBannedWords |= !!CommandLine()->FindParm( "-usebanlist" );
	if ( bLoadBannedWords )
	{
		g_BannedWords.InitFromFile( "banlist.res" );
	}

	COM_TimestampedLog( "ClientDLL Init - Finish" );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Called after client & server DLL are loaded and all systems initialized
//-----------------------------------------------------------------------------
CEG_NOINLINE void CHLClient::PostInit()
{
	CEG_PROTECT_VIRTUAL_FUNCTION( CHLCLient_PostInit );

	Init_GCVs();

	COM_TimestampedLog( "IGameSystem::PostInitAllSystems - Start" );
	IGameSystem::PostInitAllSystems();
	COM_TimestampedLog( "IGameSystem::PostInitAllSystems - Finish" );

	// Turn 7L rendering optimisations ON if "-rdropt" is specified on the command line
	if ( CommandLine()->FindParm( "-rdropt" ) != 0 )
	{
		engine->ExecuteClientCmd( "toggleRdrOpt" );
	}

#ifdef SIXENSE
	// allow sixnese input to perform post-init operations
		g_pSixenseInput->PostInit();
#endif

	STEAMWORKS_TESTSECRETALWAYS();
}

//-----------------------------------------------------------------------------
// Purpose: Called when the client .dll is being dismissed
//-----------------------------------------------------------------------------
CEG_NOINLINE void CHLClient::Shutdown( void )
{
	if ( g_pRenderToRTHelper )
	{
		g_pRenderToRTHelper->Shutdown();
	}

#ifdef PORTAL2
	GameInstructor_Shutdown();
#endif

    if ( g_pAchievementsAndStatsInterface )
    {
        g_pAchievementsAndStatsInterface->ReleasePanel();
    }

	ActivityList_Free();
	EventList_Free();

	VGui_ClearVideoPanels();

#ifdef SIXENSE
		g_pSixenseInput->Shutdown();
		delete g_pSixenseInput;
		g_pSixenseInput = NULL;
#endif

	C_BaseAnimating::ShutdownBoneSetupThreadPool();
	ClientWorldFactoryShutdown();

	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetGameInstructorRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetViewEffectsRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetPhysSaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetEntitySaveRestoreBlockHandler() );

	ClientVoiceMgr_Shutdown();


	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		GetClientMode()->Disable();

		if ( hh == 0 )
		{
			GetFullscreenClientMode()->Disable();
		}
	}

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		GetClientMode()->Shutdown();

		if ( hh == 0 )
		{
			GetFullscreenClientMode()->Shutdown();
		}
	}

	CEG_PROTECT_VIRTUAL_FUNCTION( CHLClient_Shutdown );

	input->Shutdown_All();
	C_BaseTempEntity::ClearDynamicTempEnts();
	TermSmokeFogOverlay();
	view->Shutdown();
	g_pParticleSystemMgr->UncacheAllParticleSystems();

	UncacheAllMovies();
	UncacheAllMaterials();

	IGameSystem::ShutdownAllSystems();

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		GetHud().Shutdown();
	}

	VGui_Shutdown();
	
	ParticleMgr()->Term();
	
#ifdef USE_BLOBULATOR
	Blobulator::ShutdownBlob();
#endif // USE_BLOBULATOR
	
	ClearKeyValuesCache();

#ifndef NO_STEAM
	ClientSteamContext().Shutdown();
	// SteamAPI_Shutdown(); << Steam shutdown is controlled by engine
#endif
	
	DisconnectTier3Libraries( );
	DisconnectTier2Libraries( );
	ConVar_Unregister();
	DisconnectTier1Libraries( );

	gameeventmanager = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//  Called when the game initializes
//  and whenever the vid_mode is changed
//  so the HUD can reinitialize itself.
// Output : int
//-----------------------------------------------------------------------------
int CHLClient::HudVidInit( void )
{
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetHud().VidInit();
	}

	GetClientVoiceMgr()->VidInit();

	return 1;
}

//-----------------------------------------------------------------------------
// Method used to allow the client to filter input messages before the 
// move record is transmitted to the server
//-----------------------------------------------------------------------------
void CHLClient::HudProcessInput( bool bActive )
{
	GetClientMode()->ProcessInput( bActive );
}

//-----------------------------------------------------------------------------
// Purpose: Called when shared data gets changed, allows dll to modify data
// Input  : bActive - 
//-----------------------------------------------------------------------------
void CHLClient::HudUpdate( bool bActive )
{
	float frametime = gpGlobals->frametime;

	GetClientVoiceMgr()->Frame( frametime );

	ASSERT_LOCAL_PLAYER_NOT_RESOLVABLE();

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetHud().UpdateHud( bActive );
	}

	ASSERT_LOCAL_PLAYER_NOT_RESOLVABLE();

	{
		C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, false ); 
		IGameSystem::UpdateAllSystems( frametime );
	}

	// run vgui animations
	vgui::GetAnimationController()->UpdateAnimations( Plat_FloatTime() );

	if ( hudlcd )
	{
		hudlcd->SetGlobalStat( "(time_int)", VarArgs( "%d", (int)gpGlobals->curtime ) );
		hudlcd->SetGlobalStat( "(time_float)", VarArgs( "%.2f", gpGlobals->curtime ) );
	}

	// I don't think this is necessary any longer, but I will leave it until
	// I can check into this further.
	C_BaseTempEntity::CheckDynamicTempEnts();

#ifdef SIXENSE
	// If we're not connected, update sixense so we can move the mouse cursor when in the menus
	if( !engine->IsConnected() || engine->IsPaused() )
	{
		g_pSixenseInput->SixenseFrame( 0, NULL ); 
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Called to restore to "non"HUD state.
//-----------------------------------------------------------------------------
void CHLClient::HudReset( void )
{
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetHud().VidInit();
	}

	PhysicsReset();
}

//-----------------------------------------------------------------------------
// Purpose: Called to add hud text message
//-----------------------------------------------------------------------------
void CHLClient::HudText( const char * message )
{
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		DispatchHudText( message );
	}
}


//-----------------------------------------------------------------------------
// Handler for input events for the new game ui system
//-----------------------------------------------------------------------------
bool CHLClient::HandleGameUIEvent( const InputEvent_t &inputEvent )
{
#ifdef GAMEUI_UISYSTEM2_ENABLED
	// TODO: when embedded UI will be used for HUD, we will need it to maintain
	// a separate screen for HUD and a separate screen stack for pause menu & main menu.
	// for now only render embedded UI in pause menu & main menu
	BaseModUI::CBaseModPanel *pBaseModPanel = BaseModUI::CBaseModPanel::GetSingletonPtr();
	if ( !pBaseModPanel || !pBaseModPanel->IsVisible() )
		return false;

	return g_pGameUIGameSystem->RegisterInputEvent( inputEvent );
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Process any extra client specific, non-entity based sound positioning.
//-----------------------------------------------------------------------------
bool CHLClient::GetSoundSpatialization(  SpatializationInfo_t& info )
{

#ifdef PORTAL2

// 	Vector vListenerOrigin;
// 	VectorCopy( info.info.vListenerOrigin, vListenerOrigin );

	Vector vSoundOrigin;
	if( info.pOrigin )
	{
		VectorCopy( *info.pOrigin, vSoundOrigin );
	}
	else
	{
		VectorCopy( info.info.vOrigin, vSoundOrigin );
	}
	UTIL_Portal_VectorToGlobalTransforms( vSoundOrigin, info.m_pUtlVecMultiOrigins );
	

// 	// portal2
// 	Vector vListenerOrigin, vSoundOrigin;
// 
// 	VectorCopy( info.info.vListenerOrigin, vListenerOrigin );
// 	if( info.pOrigin )
// 	{
// 		VectorCopy( *info.pOrigin, vSoundOrigin );
// 	}
// 	else
// 	{
// 		VectorCopy( info.info.vOrigin, vSoundOrigin );
// 	}
// 
// 	CPortal_Base2D *pShortestDistPortal_Out = NULL;
// 	float flMinDist = UTIL_Portal_ShortestDistanceSqr( vListenerOrigin , vSoundOrigin, &pShortestDistPortal_Out  );
// 
// 	if( pShortestDistPortal_Out )
// 	{
// 		if( pShortestDistPortal_Out->m_bActivated )
// 		{
// 			CPortal_Base2D *pLinkedPortal = pShortestDistPortal_Out->m_hLinkedPortal.Get();
// 			if( pLinkedPortal != NULL )
// 			{
// 
// 				VMatrix matrixFromPortal;
// 				MatrixInverseTR( pShortestDistPortal_Out->MatrixThisToLinked(), matrixFromPortal );
// 
// 				Vector vPoint1Transformed = matrixFromPortal * vSoundOrigin;
// 				if( info.pOrigin )
// 				{
// 					*info.pOrigin = vPoint1Transformed;
// 				}
// 				else
// 				{
// 					info.info.vOrigin = vPoint1Transformed;
// 				}
// 
// 				//DevMsg("****sound portaled: %f %f %f\n", vPoint1Transformed[0], vPoint1Transformed[1], vPoint1Transformed[2] );
// 
// 			}
// 		}
// 	}
#endif

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHLClient::ShouldDrawDropdownConsole()
{
#if defined( TF_CLIENT_DLL )
	extern ConVar hud_freezecamhide;
	extern bool IsTakingAFreezecamScreenshot();

	if ( hud_freezecamhide.GetBool() && IsTakingAFreezecamScreenshot() )
	{
		return false;
	}
#endif

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : ClientClass
//-----------------------------------------------------------------------------
ClientClass *CHLClient::GetAllClasses( void )
{
	return g_pClientClassHead;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLClient::IN_ActivateMouse( void )
{
	input->ActivateMouse();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLClient::IN_DeactivateMouse( void )
{
	input->DeactivateMouse();
}

extern ConVar in_forceuser;
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLClient::IN_Accumulate ( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( in_forceuser.GetInt() );
	input->AccumulateMouse( GET_ACTIVE_SPLITSCREEN_SLOT() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLClient::IN_ClearStates ( void )
{
	input->ClearStates();
}

//-----------------------------------------------------------------------------
// Purpose: Engine can query for particular keys
// Input  : *name - 
//-----------------------------------------------------------------------------
bool CHLClient::IN_IsKeyDown( const char *name, bool& isdown )
{
	kbutton_t *key = input->FindKey( name );
	if ( !key )
	{
		return false;
	}
	
	isdown = ( key->GetPerUser().state & 1 ) ? true : false;

	// Found the key by name
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Engine can issue a key event
// Input  : eventcode - 
//			keynum - 
//			*pszCurrentBinding - 
// Output : int
//-----------------------------------------------------------------------------
int CHLClient::IN_KeyEvent( int eventcode, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	return input->KeyEvent( eventcode, keynum, pszCurrentBinding );
}

void CHLClient::ExtraMouseSample( float frametime, bool active )
{
	bool bSave = C_BaseEntity::IsAbsRecomputationsEnabled();
	C_BaseEntity::EnableAbsRecomputations( true );

	// [mariod] - testing, see note in c_baseanimating.cpp
	//C_BaseAnimating::EnableNewBoneSetupRequest( false );

	ABS_QUERY_GUARD( true );

	C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, false ); 

	MDLCACHE_CRITICAL_SECTION();
	input->ExtraMouseSample( frametime, active );

	// [mariod] - testing, see note in c_baseanimating.cpp
	//C_BaseAnimating::EnableNewBoneSetupRequest( true );


	C_BaseEntity::EnableAbsRecomputations( bSave );
}

void CHLClient::IN_SetSampleTime( float frametime )
{
	input->Joystick_SetSampleTime( frametime );
	input->IN_SetSampleTime( frametime );

#ifdef SIXENSE
		g_pSixenseInput->ResetFrameTime( frametime );
#endif
}
//-----------------------------------------------------------------------------
// Purpose: Fills in usercmd_s structure based on current view angles and key/controller inputs
// Input  : frametime - timestamp for last frame
//			*cmd - the command to fill in
//			active - whether the user is fully connected to a server
//-----------------------------------------------------------------------------
void CHLClient::CreateMove ( int sequence_number, float input_sample_frametime, bool active )
{

	Assert( C_BaseEntity::IsAbsRecomputationsEnabled() );
	Assert( C_BaseEntity::IsAbsQueriesValid() );

	C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, false ); 

	// [mariod] - testing, see note in c_baseanimating.cpp
	//C_BaseAnimating::EnableNewBoneSetupRequest( false );

	MDLCACHE_CRITICAL_SECTION();
	input->CreateMove( sequence_number, input_sample_frametime, active );

	// [mariod] - testing, see note in c_baseanimating.cpp
	//C_BaseAnimating::EnableNewBoneSetupRequest( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *buf - 
//			from - 
//			to - 
//-----------------------------------------------------------------------------
bool CHLClient::WriteUsercmdDeltaToBuffer( int nSlot, bf_write *buf, int from, int to, bool isnewcommand )
{
	return input->WriteUsercmdDeltaToBuffer( nSlot, buf, from, to, isnewcommand );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buf - 
//			buffersize - 
//-----------------------------------------------------------------------------
void CHLClient::EncodeUserCmdToBuffer( int nSlot, bf_write& buf, int slot )
{
	input->EncodeUserCmdToBuffer( nSlot, buf, slot );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buf - 
//			buffersize - 
//			slot - 
//-----------------------------------------------------------------------------
void CHLClient::DecodeUserCmdFromBuffer( int nSlot, bf_read& buf, int slot )
{
	input->DecodeUserCmdFromBuffer( nSlot, buf, slot );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLClient::View_Render( vrect_t *rect )
{
	VPROF( "View_Render" );

	// UNDONE: This gets hit at startup sometimes, investigate - will cause NaNs in calcs inside Render()
	if ( rect->width == 0 || rect->height == 0 )
		return;

	view->Render( rect );

#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
	UpdatePerfStats();
#endif
}


//-----------------------------------------------------------------------------
// Gets the location of the player viewpoint
//-----------------------------------------------------------------------------
bool CHLClient::GetPlayerView( CViewSetup &playerView )
{
	playerView = *view->GetPlayerViewSetup();
	return true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CHLClient::InvalidateMdlCache()
{
	C_BaseAnimating *pAnimating;
	for ( C_BaseEntity *pEntity = ClientEntityList().FirstBaseEntity(); pEntity; pEntity = ClientEntityList().NextBaseEntity(pEntity) )
	{
		pAnimating = pEntity->GetBaseAnimating();
		if ( pAnimating )
		{
			pAnimating->InvalidateMdlCache();
		}
	}
	CStudioHdr::CActivityToSequenceMapping::ResetMappings();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSF - 
//-----------------------------------------------------------------------------
void CHLClient::View_Fade( ScreenFade_t *pSF )
{
	if ( pSF != NULL )
	{
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
			GetViewEffects()->Fade( *pSF );
		}
	}
}

// CPU level
//-----------------------------------------------------------------------------
void ConfigureCurrentSystemLevel( );
void OnCPULevelChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConfigureCurrentSystemLevel();
}


#if defined( DX_TO_GL_ABSTRACTION )
static ConVar cpu_level( "cpu_level", "3", 0, "CPU Level - Default: High", OnCPULevelChanged );
#else
static ConVar cpu_level( "cpu_level", "2", 0, "CPU Level - Default: High", OnCPULevelChanged );
#endif

CPULevel_t GetCPULevel()
{
	if ( IsX360() )
		return CPU_LEVEL_360;
	if ( IsPS3() )
		return CPU_LEVEL_PS3;

	return GetActualCPULevel();
}

CPULevel_t GetActualCPULevel()
{
	// Should we cache system_level off during level init?
	CPULevel_t nSystemLevel = (CPULevel_t)clamp( cpu_level.GetInt(), 0, CPU_LEVEL_PC_COUNT-1 );
	return nSystemLevel;
}



//-----------------------------------------------------------------------------
// GPU level
//-----------------------------------------------------------------------------
void OnGPULevelChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConfigureCurrentSystemLevel();
}

static ConVar gpu_level( "gpu_level", "3", 0, "GPU Level - Default: High", OnGPULevelChanged );
GPULevel_t GetGPULevel()
{
	if ( IsX360() )
		return GPU_LEVEL_360;
	if ( IsPS3() )
		return GPU_LEVEL_PS3;

	// Should we cache system_level off during level init?
	GPULevel_t nSystemLevel = (GPULevel_t)clamp( gpu_level.GetInt(), 0, GPU_LEVEL_PC_COUNT-1 );
	return nSystemLevel;
}


//-----------------------------------------------------------------------------
// System Memory level
//-----------------------------------------------------------------------------
void OnMemLevelChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConfigureCurrentSystemLevel();
}

#if defined( DX_TO_GL_ABSTRACTION )
static ConVar mem_level( "mem_level", "3", 0, "Memory Level - Default: High", OnMemLevelChanged );
#else
static ConVar mem_level( "mem_level", "2", 0, "Memory Level - Default: High", OnMemLevelChanged );
#endif

MemLevel_t GetMemLevel()
{
	if ( IsX360() )
		return MEM_LEVEL_360;
	if ( IsPS3() )
		return MEM_LEVEL_PS3;
	
#ifdef POSIX
	const int nMinMemLevel = 2;
#else
	const int nMinMemLevel = 0;
#endif

	// Should we cache system_level off during level init?
	MemLevel_t nSystemLevel = (MemLevel_t)clamp( mem_level.GetInt(), nMinMemLevel, MEM_LEVEL_PC_COUNT-1 );
	return nSystemLevel;
}

//-----------------------------------------------------------------------------
// GPU Memory level
//-----------------------------------------------------------------------------
void OnGPUMemLevelChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConfigureCurrentSystemLevel();
}

#if defined( DX_TO_GL_ABSTRACTION )
static ConVar gpu_mem_level( "gpu_mem_level", "3", 0, "Memory Level - Default: High", OnGPUMemLevelChanged );
#else
static ConVar gpu_mem_level( "gpu_mem_level", "2", 0, "Memory Level - Default: High", OnGPUMemLevelChanged );
#endif

GPUMemLevel_t GetGPUMemLevel()
{
	if ( IsX360() )
		return GPU_MEM_LEVEL_360;
	if ( IsPS3() )
		return GPU_MEM_LEVEL_PS3;

	// Should we cache system_level off during level init?
	GPUMemLevel_t nSystemLevel = (GPUMemLevel_t)clamp( gpu_mem_level.GetInt(), 0, GPU_MEM_LEVEL_PC_COUNT-1 );
	return nSystemLevel;
}

ConVar cl_disable_splitscreen_cpu_level_cfgs_in_pip( "cl_disable_splitscreen_cpu_level_cfgs_in_pip", "1", 0, "" );

CEG_NOINLINE void ConfigureCurrentSystemLevel()
{
	CEG_ENCRYPT_FUNCTION( ConfigureCurrentSystemLevel );

	int nCPULevel = GetCPULevel();
	if ( nCPULevel == CPU_LEVEL_360 )
	{
		nCPULevel = CONSOLE_SYSTEM_LEVEL_360;
	}
	else if ( nCPULevel == CPU_LEVEL_PS3 )
	{
		nCPULevel = CONSOLE_SYSTEM_LEVEL_PS3;
	}

	int nGPULevel = GetGPULevel();
	if ( nGPULevel == GPU_LEVEL_360 )
	{
		nGPULevel = CONSOLE_SYSTEM_LEVEL_360;
	}
	else if ( nGPULevel == GPU_LEVEL_PS3 )
	{
		nGPULevel = CONSOLE_SYSTEM_LEVEL_PS3;
	}

	int nMemLevel = GetMemLevel();
	if ( nMemLevel == MEM_LEVEL_360 )
	{
		nMemLevel = CONSOLE_SYSTEM_LEVEL_360;
	}
	else if ( nMemLevel == MEM_LEVEL_PS3 )
	{
		nMemLevel = CONSOLE_SYSTEM_LEVEL_PS3;
	}

	int nGPUMemLevel = GetGPUMemLevel();
	if ( nGPUMemLevel == GPU_MEM_LEVEL_360 )
	{
		nGPUMemLevel = CONSOLE_SYSTEM_LEVEL_360;
	}
	else if ( nGPUMemLevel == GPU_MEM_LEVEL_PS3 )
	{
		nGPUMemLevel = CONSOLE_SYSTEM_LEVEL_PS3;
	}

#if defined ( TERROR )
	char szModName[32] = "left4dead";
#elif defined ( PAINT )
	char szModName[32] = "paint";
#elif defined ( PORTAL2 )
	char szModName[32] = "portal2";
#elif defined( INFESTED_DLL )
	char szModName[32] = "infested";
#elif defined ( HL2_EPISODIC )
	char szModName[32] = "ep2";
#elif defined ( TF_CLIENT_DLL )
	char szModName[32] = "tf";
#elif defined ( DOTA_CLIENT_DLL )
	char szModName[32] = "dota";
#elif defined ( SDK_CLIENT_DLL )
	char szModName[32] = "sdk";
#elif defined ( SOB_CLIENT_DLL )
	char szModName[32] = "ep2";
#elif defined ( CSTRIKE15 )
	char szModName[32] = "csgo";
#endif

	bool bVGUIIsSplitscreen = VGui_IsSplitScreen();
	if ( cl_disable_splitscreen_cpu_level_cfgs_in_pip.GetBool() && bVGUIIsSplitscreen && VGui_IsSplitScreenPIP() )
	{
		bVGUIIsSplitscreen = false;
	}
	
	UpdateSystemLevel( nCPULevel, nGPULevel, nMemLevel, nGPUMemLevel, bVGUIIsSplitscreen, szModName );

	if ( engine )
	{
		engine->ConfigureSystemLevel( nCPULevel, nGPULevel );
	}

	C_BaseEntity::UpdateVisibilityAllEntities();
	if ( view )
	{
		view->InitFadeData();
	}
}

//CEG_PROTECT_FUNCTION( ConfigureCurrentSystemLevel );

//-----------------------------------------------------------------------------
// Purpose: Per level init
//-----------------------------------------------------------------------------
CEG_NOINLINE void CHLClient::LevelInitPreEntity( char const* pMapName )
{
	// HACK: Bogus, but the logic is too complicated in the engine
	if (g_bLevelInitialized)
		return;
	g_bLevelInitialized = true;

	engine->TickProgressBar();

	// this is a "connected" state
	g_HltvReplaySystem.StopHltvReplay( );

	input->LevelInit();

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetViewEffects()->LevelInit();
	}

	// Tell mode manager that map is changing
	modemanager->LevelInit( pMapName );
	ParticleMgr()->LevelInit();

	ClientVoiceMgr_LevelInit();

	if ( hudlcd )
	{
		hudlcd->SetGlobalStat( "(mapname)", pMapName );
	}

	C_BaseTempEntity::ClearDynamicTempEnts();
	clienteffects->Flush();
	view->LevelInit();
	tempents->LevelInit();
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		ResetToneMapping(1.0);
	}

	CEG_PROTECT_MEMBER_FUNCTION( CHLClient_LevelInitPreEntity );

	IGameSystem::LevelInitPreEntityAllSystems(pMapName);

	ResetWindspeed();

#if !defined( NO_ENTITY_PREDICTION )

	//[msmith] Portal 2 wanted to turn off prediction for local clients.
	//         We want to keep it for CStrike15 (and probably other games) because of the noticable lag without it, particularly when firing a weapon.
#if defined( PORTAL2 )
	
	// don't do prediction if single player!
	// don't set direct because of FCVAR_USERINFO
	if ( gpGlobals->IsRemoteClient() && gpGlobals->maxClients > 1 )

#else

	// Turn off prediction if we're in split screen because there are a lot of third person animation and render issues with prediction.
	if ( !engine->IsSplitScreenActive() )

#endif

	{
		if ( !cl_predict->GetInt() )
		{
			engine->ClientCmd( "cl_predict 1" );
		}
	}
	else
	{
		if ( cl_predict->GetInt() )
		{
			engine->ClientCmd( "cl_predict 0" );
		}
	}
#endif

	// Check low violence settings for this map
	g_RagdollLVManager.SetLowViolence( pMapName );

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );

		engine->TickProgressBar();

		GetHud().LevelInit();

		if ( GetViewPortInterface() )
		{
			GetViewPortInterface()->LevelInit();
		}
	}

#if defined( REPLAY_ENABLED )
	// Initialize replay ragdoll recorder
	if ( !engine->IsPlayingDemo() )
	{
		CReplayRagdollRecorder::Instance().Init();
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Per level init
//-----------------------------------------------------------------------------
CEG_NOINLINE void CHLClient::LevelInitPostEntity( )
{
	ABS_QUERY_GUARD( true );

	IGameSystem::LevelInitPostEntityAllSystems();
	C_PhysPropClientside::RecreateAll();
	C_Sprite::RecreateAllClientside();

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetCenterPrint()->Clear();
	}

	g_HltvReplaySystem.OnLevelInit();

	CEG_PROTECT_MEMBER_FUNCTION( CHLClient_LevelInitPostEntity );
}

//-----------------------------------------------------------------------------
// Purpose: Reset our global string table pointers
//-----------------------------------------------------------------------------
void CHLClient::ResetStringTablePointers()
{
	g_pStringTableParticleEffectNames = NULL;
	g_StringTableEffectDispatch = NULL;
	g_StringTableVguiScreen = NULL;
	g_pStringTableMaterials = NULL;
	g_pStringTableInfoPanel = NULL;
	g_pStringTableClientSideChoreoScenes = NULL;
	g_pStringTableMovies = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Per level de-init
//-----------------------------------------------------------------------------
CEG_NOINLINE void CHLClient::LevelShutdown( void )
{
	g_HltvReplaySystem.OnLevelShutdown();

	// HACK: Bogus, but the logic is too complicated in the engine
	if (!g_bLevelInitialized)
	{
		ResetStringTablePointers();
		return;
	}

	g_bLevelInitialized = false;

	// this is a "connected" state
	g_HltvReplaySystem.StopHltvReplay();

	// Disable abs recomputations when everything is shutting down
	CBaseEntity::EnableAbsRecomputations( false );

	// Level shutdown sequence.
	// First do the pre-entity shutdown of all systems
	IGameSystem::LevelShutdownPreEntityAllSystems();

	C_Sprite::DestroyAllClientside();
	C_PhysPropClientside::DestroyAll();

	modemanager->LevelShutdown();

	// Remove temporary entities before removing entities from the client entity list so that the te_* may
	// clean up before hand.
	tempents->LevelShutdown();

	// Now release/delete the entities
	cl_entitylist->Release();

	C_BaseEntityClassList *pClassList = s_pClassLists;
	while ( pClassList )
	{
		pClassList->LevelShutdown();
		pClassList = pClassList->m_pNextClassList;
	}

	CEG_ENCRYPT_FUNCTION( CHLClient_LevelShutdown );

	// Now do the post-entity shutdown of all systems
	IGameSystem::LevelShutdownPostEntityAllSystems();

	view->LevelShutdown();
	beams->ClearBeams();
	ParticleMgr()->RemoveAllEffects();
	
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		StopAllRumbleEffects( hh );
	}

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetHud().LevelShutdown();
	}

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetCenterPrint()->Clear();
	}

	ClientVoiceMgr_LevelShutdown();

	messagechars->Clear();

	g_pParticleSystemMgr->LevelShutdown();
	g_pParticleSystemMgr->UncacheAllParticleSystems();

	UncacheAllMovies();
	UncacheAllMaterials();

	// string tables are cleared on disconnect from a server, so reset our global pointers to NULL
	ResetStringTablePointers();

	CStudioHdr::CActivityToSequenceMapping::ResetMappings();

#if defined( REPLAY_ENABLED )
	// Shutdown the ragdoll recorder
	CReplayRagdollRecorder::Instance().Shutdown();
	CReplayRagdollCache::Instance().Shutdown();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Engine received crosshair offset ( autoaim )
// Input  : angle - 
//-----------------------------------------------------------------------------
void CHLClient::SetCrosshairAngle( const QAngle& angle )
{
#ifndef INFESTED_DLL
	CHudCrosshair *crosshair = GET_HUDELEMENT( CHudCrosshair );
	if ( crosshair )
	{
		crosshair->SetCrosshairAngle( angle );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Helper to initialize sprite from .spr semaphor
// Input  : *pSprite - 
//			*loadname - 
//-----------------------------------------------------------------------------
void CHLClient::InitSprite( CEngineSprite *pSprite, const char *loadname )
{
	if ( pSprite )
	{
		pSprite->Init( loadname );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSprite - 
//-----------------------------------------------------------------------------
void CHLClient::ShutdownSprite( CEngineSprite *pSprite )
{
	if ( pSprite )
	{
		pSprite->Shutdown();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Tells engine how much space to allocate for sprite objects
// Output : int
//-----------------------------------------------------------------------------
int CHLClient::GetSpriteSize( void ) const
{
	return sizeof( CEngineSprite );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : entindex - 
//			bTalking - 
//-----------------------------------------------------------------------------
void CHLClient::VoiceStatus( int entindex, int iSsSlot, qboolean bTalking )
{
	GetClientVoiceMgr()->UpdateSpeakerStatus( entindex, iSsSlot, !!bTalking );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : entindex - 
//-----------------------------------------------------------------------------
bool CHLClient::PlayerAudible( int iPlayerIndex )
{
	return GetClientVoiceMgr()->IsPlayerAudible( iPlayerIndex );
}


//-----------------------------------------------------------------------------
// Called when the string table for materials changes
//-----------------------------------------------------------------------------
void OnMaterialStringTableChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
	// Make sure this puppy is precached
	gHLClient.PrecacheMaterial( newString );
	RequestCacheUsedMaterials();
}

//-----------------------------------------------------------------------------
// Called when the string table for movies changes
//-----------------------------------------------------------------------------
void OnMovieStringTableChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
	// Make sure this puppy is precached
	gHLClient.PrecacheMovie( newString );
}

//-----------------------------------------------------------------------------
// Called when the string table for dispatch effects changes
//-----------------------------------------------------------------------------
void OnEffectStringTableChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
	// Make sure this puppy is precached
	g_pPrecacheSystem->Cache( g_pPrecacheHandler, DISPATCH_EFFECT, newString, true, RESOURCE_LIST_INVALID, true );
	RequestCacheUsedMaterials();
}


//-----------------------------------------------------------------------------
// Called when the string table for particle systems changes
//-----------------------------------------------------------------------------
void OnParticleSystemStringTableChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
	// Make sure this puppy is precached
	g_pParticleSystemMgr->PrecacheParticleSystem( stringNumber, newString );
	RequestCacheUsedMaterials();
}

//-----------------------------------------------------------------------------
// Called when the string table for particle files changes
//-----------------------------------------------------------------------------
void OnPrecacheParticleFile( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
	g_pParticleSystemMgr->ShouldLoadSheets( true );
	g_pParticleSystemMgr->ReadParticleConfigFile( newString, true, false );
	g_pParticleSystemMgr->DecommitTempMemory();
}

//-----------------------------------------------------------------------------
// Called when the string table for VGUI changes
//-----------------------------------------------------------------------------
void OnVguiScreenTableChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
	// Make sure this puppy is precached
	vgui::Panel *pPanel = PanelMetaClassMgr()->CreatePanelMetaClass( newString, 100, NULL, NULL );
	if ( pPanel )
		PanelMetaClassMgr()->DestroyPanelMetaClass( pPanel );
}

//-----------------------------------------------------------------------------
// Purpose: Preload the string on the client (if single player it should already be in the cache from the server!!!)
// Input  : *object - 
//			*stringTable - 
//			stringNumber - 
//			*newString - 
//			*newData - 
//-----------------------------------------------------------------------------
void OnSceneStringTableChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
}

//-----------------------------------------------------------------------------
// Purpose: Hook up any callbacks here, the table definition has been parsed but 
//  no data has been added yet
//-----------------------------------------------------------------------------
void CHLClient::InstallStringTableCallback( const char *tableName )
{
	// Here, cache off string table IDs
	if (!Q_strcasecmp(tableName, "VguiScreen"))
	{
		// Look up the id 
		g_StringTableVguiScreen = networkstringtable->FindTable( tableName );

		// When the material list changes, we need to know immediately
		g_StringTableVguiScreen->SetStringChangedCallback( NULL, OnVguiScreenTableChanged );
	}
	else if (!Q_strcasecmp(tableName, "Materials"))
	{
		// Look up the id 
		g_pStringTableMaterials = networkstringtable->FindTable( tableName );

		// When the material list changes, we need to know immediately
		g_pStringTableMaterials->SetStringChangedCallback( NULL, OnMaterialStringTableChanged );
	}
	else if ( !Q_strcasecmp( tableName, "EffectDispatch" ) )
	{
		g_StringTableEffectDispatch = networkstringtable->FindTable( tableName );

		// When the material list changes, we need to know immediately
		g_StringTableEffectDispatch->SetStringChangedCallback( NULL, OnEffectStringTableChanged );
	}
	else if ( !Q_strcasecmp( tableName, "InfoPanel" ) )
	{
		g_pStringTableInfoPanel = networkstringtable->FindTable( tableName );
	}
	else if ( !Q_strcasecmp( tableName, "Scenes" ) )
	{
		g_pStringTableClientSideChoreoScenes = networkstringtable->FindTable( tableName );
		networkstringtable->SetAllowClientSideAddString( g_pStringTableClientSideChoreoScenes, true );
		g_pStringTableClientSideChoreoScenes->SetStringChangedCallback( NULL, OnSceneStringTableChanged );
	}
	else if ( !Q_strcasecmp( tableName, "ParticleEffectNames" ) )
	{
		g_pStringTableParticleEffectNames = networkstringtable->FindTable( tableName );
		networkstringtable->SetAllowClientSideAddString( g_pStringTableParticleEffectNames, true );
		// When the particle system list changes, we need to know immediately
		g_pStringTableParticleEffectNames->SetStringChangedCallback( NULL, OnParticleSystemStringTableChanged );
	}
	else if ( !Q_strcasecmp( tableName, "ExtraParticleFilesTable" ) )
	{
		g_pStringTableExtraParticleFiles = networkstringtable->FindTable( tableName );
		networkstringtable->SetAllowClientSideAddString( g_pStringTableExtraParticleFiles, true );
		// When the particle system list changes, we need to know immediately
		g_pStringTableExtraParticleFiles->SetStringChangedCallback( NULL, OnPrecacheParticleFile );
	}
	else if ( !Q_strcasecmp( tableName, "Movies" ) )
	{
		// Look up the id 
		g_pStringTableMovies = networkstringtable->FindTable( tableName );

		// When the movie list changes, we need to know immediately
		g_pStringTableMovies->SetStringChangedCallback( NULL, OnMovieStringTableChanged );
	}
	else
	{
		// Pass tablename to gamerules last if all other checks fail
		InstallStringTableCallback_GameRules( tableName );
	}
}


//-----------------------------------------------------------------------------
// Material precache
//-----------------------------------------------------------------------------
void CHLClient::PrecacheMaterial( const char *pMaterialName )
{
	Assert( pMaterialName );

	int nLen = Q_strlen( pMaterialName );
	char *pTempBuf = (char*)stackalloc( nLen + 1 );
	memcpy( pTempBuf, pMaterialName, nLen + 1 );
	char *pFound = Q_strstr( pTempBuf, ".vmt\0" );
	if ( pFound )
	{
		*pFound = 0;
	}

	RANDOM_CEG_TEST_SECRET_PERIOD( 0x0400, 0x0fff );
		
	IMaterial *pMaterial = materials->FindMaterial( pTempBuf, TEXTURE_GROUP_PRECACHED );
	if ( !IsErrorMaterial( pMaterial ) )
	{
		int idx = m_CachedMaterials.Find( pMaterial );
		if ( idx == m_CachedMaterials.InvalidIndex() )
		{
			pMaterial->IncrementReferenceCount();
			m_CachedMaterials.Insert( pMaterial );
		}
	}
	else
	{
		if (IsOSX())
		{
			printf("\n ##### CHLClient::PrecacheMaterial could not find material %s (%s)", pMaterialName, pTempBuf );
		}
	}
}

void CHLClient::UncacheAllMaterials()
{
	for ( int i = m_CachedMaterials.FirstInorder(); i != m_CachedMaterials.InvalidIndex(); i = m_CachedMaterials.NextInorder( i ) )
	{
		m_CachedMaterials[i]->DecrementReferenceCount();
	}
	m_CachedMaterials.RemoveAll();
}

//-----------------------------------------------------------------------------
// Movie precache
//-----------------------------------------------------------------------------
void CHLClient::PrecacheMovie( const char *pMovieName )
{
	if ( m_CachedMovies.Find( pMovieName ) != UTL_INVAL_SYMBOL )
	{
		// already precached
		return;
	}
	
	// hint the movie system to precache our movie resource
	m_CachedMovies.AddString( pMovieName );
#if defined( BINK_VIDEO )
	g_pBIK->PrecacheMovie( pMovieName );
#endif
}

void CHLClient::UncacheAllMovies()
{
#if defined( BINK_VIDEO )
	for ( int i = 0; i < m_CachedMovies.GetNumStrings(); i++ )
	{
		g_pBIK->EvictPrecachedMovie( m_CachedMovies.String( i ) );
	}
#endif
	m_CachedMovies.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------

bool CHLClient::DispatchUserMessage( int msg_type, int32 nPassthroughFlags, int size, const void *msg )
{
	return UserMessages()->DispatchUserMessage( msg_type, nPassthroughFlags, size, msg );
}

bool BSerializeUserMessageToSVCMSG( CSVCMsg_UserMessage &svcmsg, int nType, const ::google::protobuf::Message &msg )
{
	if ( !msg.IsInitialized() || !svcmsg.IsInitialized() )
	{
		return false;
	}

	int size = msg.ByteSize();
	svcmsg.set_msg_type( nType );
	svcmsg.mutable_msg_data()->resize( size );
	if ( !msg.SerializeWithCachedSizesToArray( ( uint8* ) &( *svcmsg.mutable_msg_data() )[ 0 ] ) )
	{
		return false;
	}

	return true;
}

void SimulateEntities()
{
	VPROF_BUDGET("Client SimulateEntities", VPROF_BUDGETGROUP_CLIENT_SIM);

	// Service timer events (think functions).
  	ClientThinkList()->PerformThinkFunctions();

	C_BaseEntity::SimulateEntities();
}


bool AddDataChangeEvent( IClientNetworkable *ent, DataUpdateType_t updateType, int *pStoredEvent )
{
	VPROF( "AddDataChangeEvent" );

	Assert( ent );

	if ( updateType == DATA_UPDATE_POST_UPDATE )
	{
		Assert( g_bDataChangedPostEventsAllowed );
		*pStoredEvent = g_DataChangedPostEvents.AddToTail( CDataChangedEvent( ent, updateType, pStoredEvent ) );
		return true;
	}

	// Make sure we don't already have an event queued for this guy.
	if ( *pStoredEvent >= 0 )
	{
		if ( uint( *pStoredEvent ) < uint( g_DataChangedEvents.Count() ) )
		{
			Assert( g_DataChangedEvents[ *pStoredEvent ].GetEntity() == ent );

			// DATA_UPDATE_CREATED always overrides DATA_UPDATE_CHANGED.
			if ( updateType == DATA_UPDATE_CREATED )
				g_DataChangedEvents[ *pStoredEvent ].m_UpdateType = updateType;
		}
	
		return false;
	}
	else
	{
		*pStoredEvent = g_DataChangedEvents.AddToTail( CDataChangedEvent( ent, updateType, pStoredEvent ) );
		return true;
	}
}


void ClearDataChangedEvent( int iStoredEvent )
{
	if ( iStoredEvent != -1 )
		g_DataChangedEvents.Remove( iStoredEvent );
}


void ProcessOnDataChangedEvents()
{
	VPROF_("ProcessOnDataChangedEvents", 1, VPROF_BUDGETGROUP_CLIENT_SIM, false, BUDGETFLAG_CLIENT);
	int nSave = GET_ACTIVE_SPLITSCREEN_SLOT();
	bool bSaveAccess = engine->SetLocalPlayerIsResolvable( __FILE__, __LINE__, false );

	g_bDataChangedPostEventsAllowed = true;
	FOR_EACH_LL( g_DataChangedEvents, i )
	{
		CDataChangedEvent *pEvent = &g_DataChangedEvents[i];

		// Reset their stored event identifier.		
		*pEvent->m_pStoredEvent = -1;

		IClientNetworkable *pEntity = pEvent->GetEntity();
		if ( pEntity )
		{
			// Send the event.
			pEntity->OnDataChanged( pEvent->m_UpdateType );
		}
	}
	g_bDataChangedPostEventsAllowed = false;

	FOR_EACH_LL( g_DataChangedPostEvents, i )
	{
		CDataChangedEvent *pEvent = &g_DataChangedPostEvents[ i ];

		// Reset their stored event identifier.		
		*pEvent->m_pStoredEvent = -1;

		IClientNetworkable *pEntity = pEvent->GetEntity();
		if ( pEntity )
		{
			// Send the event.
			pEntity->OnDataChanged( pEvent->m_UpdateType );
		}
	}

	//now run through the list again and initialize any predictables that are ready
	FOR_EACH_LL( g_DataChangedEvents, i )
	{
		IClientNetworkable *pNetworkable = g_DataChangedEvents[i].GetEntity();
		if ( !pNetworkable )
			continue;
		
		IClientUnknown *pUnk = pNetworkable->GetIClientUnknown();
		if ( !pUnk )
			continue;

		C_BaseEntity *pEntity = pUnk->GetBaseEntity();
		if ( !pEntity )
			continue;

		pEntity->CheckInitPredictable( "ProcessOnDataChangedEvents()" );
	}

	g_DataChangedEvents.Purge();
	g_DataChangedPostEvents.Purge();

	engine->SetActiveSplitScreenPlayerSlot( nSave );
	engine->SetLocalPlayerIsResolvable( __FILE__, __LINE__, bSaveAccess );
}

void PurgeOnDataChangedEvents()
{
	if ( int nPurging = g_DataChangedEvents.Count() )
	{
		DevMsg( "Purging %d data change events without processing\n", nPurging );
	}
	g_DataChangedEvents.Purge();
}

void UpdateClientRenderableInPVSStatus()
{
	// Vis for this view should already be setup at this point.

	// For each client-only entity, notify it if it's newly coming into the PVS.
	CUtlLinkedList<CClientEntityList::CPVSNotifyInfo,unsigned short> &theList = ClientEntityList().GetPVSNotifiers();
	FOR_EACH_LL( theList, i )
	{
		CClientEntityList::CPVSNotifyInfo *pInfo = &theList[i];

		if ( pInfo->m_InPVSStatus & INPVS_YES )
		{
			// Ok, this entity already thinks it's in the PVS. No need to notify it.
			// We need to set the INPVS_YES_THISFRAME flag if it's in this frame at all, so we 
			// don't tell the entity it's not in the PVS anymore at the end of the frame.
			if ( !( pInfo->m_InPVSStatus & INPVS_THISFRAME ) )
			{
				if ( g_pClientLeafSystem->IsRenderableInPVS( pInfo->m_pRenderable ) )
				{
					pInfo->m_InPVSStatus |= INPVS_THISFRAME;
				}
			}
		}
		else
		{
			// This entity doesn't think it's in the PVS yet. If it is now in the PVS, let it know.
			if ( g_pClientLeafSystem->IsRenderableInPVS( pInfo->m_pRenderable ) )
			{
				pInfo->m_InPVSStatus |= ( INPVS_YES | INPVS_THISFRAME | INPVS_NEEDSNOTIFY );
			}
		}
	}	
}

void UpdatePVSNotifiers()
{
	MDLCACHE_CRITICAL_SECTION();

	// At this point, all the entities that were rendered in the previous frame have INPVS_THISFRAME set
	// so we can tell the entities that aren't in the PVS anymore so.
	CUtlLinkedList<CClientEntityList::CPVSNotifyInfo,unsigned short> &theList = ClientEntityList().GetPVSNotifiers();
	FOR_EACH_LL( theList, i )
	{
		CClientEntityList::CPVSNotifyInfo *pInfo = &theList[i];

		// If this entity thinks it's in the PVS, but it wasn't in the PVS this frame, tell it so.
		if ( pInfo->m_InPVSStatus & INPVS_YES )
		{
			if ( pInfo->m_InPVSStatus & INPVS_THISFRAME )
			{
				if ( pInfo->m_InPVSStatus & INPVS_NEEDSNOTIFY )
				{
					pInfo->m_pNotify->OnPVSStatusChanged( true );
				}
				// Clear it for the next time around.
				pInfo->m_InPVSStatus &= ~( INPVS_THISFRAME | INPVS_NEEDSNOTIFY );
			}
			else
			{
				pInfo->m_InPVSStatus &= ~INPVS_YES;
				pInfo->m_pNotify->OnPVSStatusChanged( false );
			}
		}
	}
}


void OnRenderStart()
{
	CMatRenderContextPtr pRenderContext( materials );

	PIXEVENT( pRenderContext, "OnRenderStart" );
	VPROF( "OnRenderStart" );

	MDLCACHE_CRITICAL_SECTION();
	MDLCACHE_COARSE_LOCK();

#ifdef PORTAL
	g_pPortalRender->UpdatePortalPixelVisibility(); //updating this one or two lines before querying again just isn't cutting it. Update as soon as it's cheap to do so.
#endif

	// [mariod] - testing, see note in c_baseanimating.cpp
	C_BaseAnimating::EnableInvalidateBoneCache( true );

	::partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, true );
	C_BaseEntity::SetAbsQueriesValid( false );

	Rope_ResetCounters();

	// Interpolate server entities and move aiments.
	{
		PREDICTION_TRACKVALUECHANGESCOPE( "interpolation" );
		C_BaseEntity::InterpolateServerEntities();
	}

	{
		// vprof node for this bloc of math
		VPROF( "OnRenderStart: dirty bone caches");
		// Invalidate any bone information.
		C_BaseAnimating::InvalidateBoneCaches();
		C_BaseFlex::InvalidateFlexCaches();

		C_BaseEntity::SetAbsQueriesValid( true );
		C_BaseEntity::EnableAbsRecomputations( true );

		// Enable access to all model bones except view models.
		// This is necessary for aim-ent computation to occur properly
		C_BaseAnimating::PushAllowBoneAccess( true, false, "OnRenderStart->CViewRender::SetUpView" ); // pops in CViewRender::SetUpView

		// FIXME: This needs to be done before the player moves; it forces
		// aiments the player may be attached to to forcibly update their position
		C_BaseEntity::MarkAimEntsDirty();
	}

	// Make sure the camera simulation happens before OnRenderStart, where it's used.
	// NOTE: the only thing that happens in CAM_Think is thirdperson related code.
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		input->CAM_Think();
	}

	C_BaseAnimating::PopBoneAccess( "OnRenderStart->CViewRender::SetUpView" ); // pops the (true, false) bone access set in OnRenderStart

	// Enable access to all model bones until rendering is done
	C_BaseAnimating::PushAllowBoneAccess( true, true, "CViewRender::SetUpView->OnRenderEnd" ); // pop is in OnRenderEnd()



#ifdef DEMOPOLISH_ENABLED
	// Update demo polish subsystem if necessary
	DemoPolish_Think();
#endif
	
	// This will place all entities in the correct position in world space and in the KD-tree
	// NOTE: Doing this before view->OnRenderStart() because the player can be in hierarchy with
	// a client-side animated entity.  So the viewport position is dependent on this animation sometimes.
	C_BaseAnimating::UpdateClientSideAnimations();

	// This will place the player + the view models + all parent
	// entities	at the correct abs position so that their attachment points
	// are at the correct location
	view->OnRenderStart();
	::partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, false );

	// Process OnDataChanged events.
	ProcessOnDataChangedEvents();

	// Reset the overlay alpha. Entities can change the state of this in their think functions.
	g_SmokeFogOverlayAlpha = 0;	

	g_SmokeFogOverlayColor.Init( 0.0f, 0.0f, 0.0f );

	{
		// Simulate all the entities.
		PIXEVENT( pRenderContext, "SimulateEntities" );
		SimulateEntities();
	}

	{
		PIXEVENT( pRenderContext, "PhysicsSimulate" );
		PhysicsSimulate();
	}

	C_BaseAnimating::ThreadedBoneSetup();

	{
		VPROF_("Client TempEnts", 0, VPROF_BUDGETGROUP_CLIENT_SIM, false, BUDGETFLAG_CLIENT);
		// This creates things like temp entities.
		engine->FireEvents();

		// Update temp entities
		tempents->Update();

		// Update temp ent beams...
		beams->UpdateTempEntBeams();
		
		// Lock the frame from beam additions
		SetBeamCreationAllowed( false );
	}

	// Update particle effects (eventually, the effects should use Simulate() instead of having
	// their own update system).
	{
		VPROF_BUDGET( "ParticleMgr()->Simulate", VPROF_BUDGETGROUP_PARTICLE_SIMULATION );
		ParticleMgr()->Simulate( gpGlobals->frametime );
	}

	// Now that the view model's position is setup and aiments are marked dirty, update
	// their positions so they're in the leaf system correctly.
	C_BaseEntity::CalcAimEntPositions();

	// For entities marked for recording, post bone messages to IToolSystems
	if ( ToolsEnabled() )
	{
		C_BaseEntity::ToolRecordEntities();
	}

#if defined( REPLAY_ENABLED )
	// This will record any ragdolls if Replay mode is enabled on the server
	CReplayRagdollRecorder::Instance().Think();
	CReplayRagdollCache::Instance().Think();
#endif

	// update dynamic light state. Necessary for light cache to work properly for d- and elights
	engine->UpdateDAndELights();

	// Finally, link all the entities into the leaf system right before rendering.
	C_BaseEntity::AddVisibleEntities();

	g_pClientLeafSystem->RecomputeRenderableLeaves();
	g_pClientShadowMgr->ReprojectShadows();
	g_pClientShadowMgr->AdvanceFrame();
	g_pClientLeafSystem->DisableLeafReinsertion( true );
}

void OnRenderEnd()
{
	g_pClientLeafSystem->DisableLeafReinsertion( false );

	// Disallow access to bones (access is enabled in CViewRender::SetUpView).
	C_BaseAnimating::PopBoneAccess( "CViewRender::SetUpView->OnRenderEnd" );
	
	// [mariod] - testing, see note in c_baseanimating.cpp
	C_BaseAnimating::EnableInvalidateBoneCache( false );

	UpdatePVSNotifiers();

	DisplayBoneSetupEnts();
}



void CHLClient::FrameStageNotify( ClientFrameStage_t curStage )
{
	g_CurFrameStage = curStage;
	g_bEngineIsHLTV = engine->IsHLTV();

	switch( curStage )
	{
	default:
		break;

	case FRAME_RENDER_START:
		{
			VPROF( "CHLClient::FrameStageNotify FRAME_RENDER_START" );

			engine->SetLocalPlayerIsResolvable( __FILE__, __LINE__, false );

			// Last thing before rendering, run simulation.
			OnRenderStart();
		}
		break;
		
	case FRAME_RENDER_END:
		{
			VPROF( "CHLClient::FrameStageNotify FRAME_RENDER_END" );
			OnRenderEnd();

			engine->SetLocalPlayerIsResolvable( __FILE__, __LINE__, false );

			PREDICTION_SPEWVALUECHANGES();
		}
		break;
		
	case FRAME_NET_FULL_FRAME_UPDATE_ON_REMOVE:
		{
			ParticleMgr()->RemoveAllNewEffects(); // new effects linger past entity recreation, creating a flurry of asserts, that's not good
			Assert( g_DataChangedEvents.Count() == 0 );	// this may only happen when we didn't render anything between getting an update and then a full frame update 
			// NOTE: There are no entities at this point if it's a full frame update! 
			PurgeOnDataChangedEvents();
		}
		break;

	case FRAME_NET_UPDATE_START:
		{
			VPROF( "CHLClient::FrameStageNotify FRAME_NET_UPDATE_START" );
			// disabled all recomputations while we update entities
			C_BaseEntity::EnableAbsRecomputations( false );
			C_BaseEntity::SetAbsQueriesValid( false );
			Interpolation_SetLastPacketTimeStamp( engine->GetLastTimeStamp() );
			::partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, true );

			PREDICTION_STARTTRACKVALUE( "netupdate" );
		}
		break;
	case FRAME_NET_UPDATE_END:
		{
			ProcessCacheUsedMaterials();

			// reenable abs recomputation since now all entities have been updated
			C_BaseEntity::EnableAbsRecomputations( true );
			C_BaseEntity::SetAbsQueriesValid( true );
			::partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, false );

			PREDICTION_ENDTRACKVALUE();
		}
		break;
	case FRAME_NET_UPDATE_POSTDATAUPDATE_START:
		{
			VPROF( "CHLClient::FrameStageNotify FRAME_NET_UPDATE_POSTDATAUPDATE_START" );
			PREDICTION_STARTTRACKVALUE( "postdataupdate" );
		}
		break;
	case FRAME_NET_UPDATE_POSTDATAUPDATE_END:
		{
			if ( !C_BasePlayer::GetLocalPlayer( 0 ) )
			{
				// this is only possible when we're playing back the past, when our killer hasn't been spawned yet
				if ( C_BasePlayer *pPlayer = UTIL_PlayerByIndex( engine->GetLocalPlayer() ) )
				{
					pPlayer->SetAsLocalPlayer();
				}
				else
				{
					Warning( "No local player %d after full frame update\n", engine->GetLocalPlayer() );
					for ( int i = 0; i < MAX_PLAYERS; ++i )
						if ( C_BasePlayer*p = UTIL_PlayerByIndex( i ) )
						{
							Msg( "Setting fallback player %s as local player\n", p->GetPlayerName() );
							p->SetAsLocalPlayer();
							break;
						}
				}
			}
			VPROF( "CHLClient::FrameStageNotify FRAME_NET_UPDATE_POSTDATAUPDATE_END" );
#if defined( PORTAL )
			ProcessPortalTeleportations();
#endif
			PREDICTION_ENDTRACKVALUE();
			// Let prediction copy off pristine data
			prediction->PostEntityPacketReceived();
			HLTVCamera()->PostEntityPacketReceived();
#if defined( REPLAY_ENABLED )
			ReplayCamera()->PostEntityPacketReceived();
#endif
		}
		break;
	case FRAME_START:
		{
			// Mark the frame as open for client fx additions
			SetFXCreationAllowed( true );
			SetBeamCreationAllowed( true );
			C_BaseEntity::CheckCLInterpChanged();
			engine->SetLocalPlayerIsResolvable( __FILE__, __LINE__, false );
			if ( !g_bEngineIsHLTV )
			{
				// if we stopped Hltv Replay abruptly for some reason (maybe an issue with the connection to the server) - just reset the Hltv Replay state
				g_HltvReplaySystem.StopHltvReplay();
			}
			g_HltvReplaySystem.Update();
		}
		break;
	}
}

CSaveRestoreData *SaveInit( int size );

// Save/restore system hooks
CSaveRestoreData  *CHLClient::SaveInit( int size )
{
	return ::SaveInit(size);
}

void CHLClient::SaveWriteFields( CSaveRestoreData *pSaveData, const char *pname, void *pBaseData, datamap_t *pMap, typedescription_t *pFields, int fieldCount )
{
	CSave saveHelper( pSaveData );
	saveHelper.WriteFields( pname, pBaseData, pMap, pFields, fieldCount );
}

void CHLClient::SaveReadFields( CSaveRestoreData *pSaveData, const char *pname, void *pBaseData, datamap_t *pMap, typedescription_t *pFields, int fieldCount )
{
	CRestore restoreHelper( pSaveData );
	restoreHelper.ReadFields( pname, pBaseData, pMap, pFields, fieldCount );
}

void CHLClient::PreSave( CSaveRestoreData *s )
{
	g_pGameSaveRestoreBlockSet->PreSave( s );
}

void CHLClient::Save( CSaveRestoreData *s )
{
	CSave saveHelper( s );
	g_pGameSaveRestoreBlockSet->Save( &saveHelper );
}

void CHLClient::WriteSaveHeaders( CSaveRestoreData *s )
{
	CSave saveHelper( s );
	g_pGameSaveRestoreBlockSet->WriteSaveHeaders( &saveHelper );
	g_pGameSaveRestoreBlockSet->PostSave();
}

CEG_NOINLINE void CHLClient::ReadRestoreHeaders( CSaveRestoreData *s )
{
	CRestore restoreHelper( s );

	CEG_ENCRYPT_FUNCTION( CHLClient_ReadRestoreHeaders );

	g_pGameSaveRestoreBlockSet->PreRestore();
	g_pGameSaveRestoreBlockSet->ReadRestoreHeaders( &restoreHelper );
}

CEG_NOINLINE void CHLClient::Restore( CSaveRestoreData *s, bool b )
{
	CRestore restore(s);
	g_pGameSaveRestoreBlockSet->Restore( &restore, b );
	g_pGameSaveRestoreBlockSet->PostRestore();

	CEG_PROTECT_VIRTUAL_FUNCTION( CHLClient_Restore );
}

static CUtlVector<EHANDLE> g_RestoredEntities;

void AddRestoredEntity( C_BaseEntity *pEntity )
{
	if ( !pEntity )
		return;

	g_RestoredEntities.AddToTail( pEntity );
}

void CHLClient::DispatchOnRestore()
{
	for ( int i = 0; i < g_RestoredEntities.Count(); i++ )
	{
		if ( g_RestoredEntities[i] != NULL )
		{
			MDLCACHE_CRITICAL_SECTION();
			g_RestoredEntities[i]->OnRestore();
		}
	}
	g_RestoredEntities.RemoveAll();
}

void CHLClient::WriteSaveGameScreenshot( const char *pFilename )
{
	// Single player doesn't support split screen yet!!!
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
	view->WriteSaveGameScreenshot( pFilename );
}

// Given a list of "S(wavname) S(wavname2)" tokens, look up the localized text and emit
//  the appropriate close caption if running with closecaption = 1
void CHLClient::EmitSentenceCloseCaption( char const *tokenstream )
{
	extern ConVar closecaption;
	
	if ( !closecaption.GetBool() )
		return;

	if ( m_pHudCloseCaption )
	{
		m_pHudCloseCaption->ProcessSentenceCaptionStream( tokenstream );
	}
}


void CHLClient::EmitCloseCaption( char const *captionname, float duration )
{
	extern ConVar closecaption;

	if ( !closecaption.GetBool() )
		return;

	if ( m_pHudCloseCaption )
	{
		RANDOM_CEG_TEST_SECRET_PERIOD( 0x40, 0xff )
		m_pHudCloseCaption->ProcessCaption( captionname, duration );
	}
}

CStandardRecvProxies* CHLClient::GetStandardRecvProxies()
{
	return &g_StandardRecvProxies;
}

bool CHLClient::CanRecordDemo( char *errorMsg, int length ) const
{
	if ( GetClientModeNormal() )
	{
		return GetClientModeNormal()->CanRecordDemo( errorMsg, length );
	}

	return true;
}

bool CHLClient::CanStopRecordDemo( char *errorMsg, int length ) const
{
	if ( CSGameRules() )
	{
		if ( CSGameRules()->IsWarmupPeriod() )
			return true;	// can always stop in warmup

		if ( CSGameRules()->IsRoundOver() )
			return true;	// can stop when round is over

		if ( CSGameRules()->IsPlayingGunGameProgressive() )
			return true;	// can always stop during arms race

		if ( CSGameRules()->IsPlayingGunGameDeathmatch() )
			return true;	// can always stop during deathmatch

		if ( engine->IsClientLocalToActiveServer() )
			return true;	// listen server clients can always stop whenever

		Q_strncpy( errorMsg, "Demo recording will stop as soon as the round is over.", length );
		CSGameRules()->MarkClientStopRecordAtRoundEnd( true );
		return false;
	}

	return true;
}

void CHLClient::OnDemoRecordStart( char const* pDemoBaseName )
{
#ifdef DEMOPOLISH_ENABLED
	if ( IsDemoPolishEnabled() )
	{
		if ( !CDemoPolishRecorder::Instance().Init( pDemoBaseName ) )
		{
			CDemoPolishRecorder::Instance().Shutdown();
		}
	}
#endif

	if ( CSGameRules() )
	{
		// If client was previously marked to stop recording at round end then mark it now as not requiring to stop
		CSGameRules()->MarkClientStopRecordAtRoundEnd( false );
	}
}

void CHLClient::OnDemoRecordStop()
{
#ifdef DEMOPOLISH_ENABLED
	if ( DemoPolish_GetRecorder().m_bInit )
	{
		DemoPolish_GetRecorder().Shutdown();
	}
#endif
}

void CHLClient::OnDemoPlaybackStart( char const* pDemoBaseName )
{
#ifdef DEMOPOLISH_ENABLED
	if ( IsDemoPolishEnabled() )
	{
		Assert( pDemoBaseName );
		if ( !DemoPolish_GetController().Init( pDemoBaseName ) )
		{
			DemoPolish_GetController().Shutdown();
		}
	}
#endif

#if defined( REPLAY_ENABLED )
	// Load any ragdoll override frames from disk
	char szRagdollFile[MAX_OSPATH];
	V_snprintf( szRagdollFile, sizeof(szRagdollFile), "%s.dmx", pDemoBaseName );
	CReplayRagdollCache::Instance().Init( szRagdollFile );
#endif

	KeyValues *pImportantEvents = new KeyValues( "DemoImportantEvents" );
	if ( pImportantEvents )
	{
		pImportantEvents->LoadFromFile( g_pFullFileSystem, "resource/DemoImportantEvents.res" );
		engine->SetDemoImportantEventData( pImportantEvents );
		pImportantEvents->deleteThis();
	}
	g_HltvReplaySystem.OnDemoPlayback( true );
}

void CHLClient::OnDemoPlaybackRestart()
{
	/* Removed for partner depot */
}

void CHLClient::OnDemoPlaybackStop()
{
#ifdef DEMOPOLISH_ENABLED
	if ( DemoPolish_GetController().m_bInit )
	{
		DemoPolish_GetController().Shutdown();
	}
#endif

#if defined( REPLAY_ENABLED )
	CReplayRagdollCache::Instance().Shutdown();
#endif
	g_HltvReplaySystem.OnDemoPlayback( false );
}


void CHLClient::SetDemoPlaybackHighlightXuid( uint64 xuid, bool bLowlights )
{
	g_HltvReplaySystem.SetDemoPlaybackHighlightXuid( xuid, bLowlights );
}


void CHLClient::ShowHighlightSkippingMessage( bool bState, int nCurrentTick, int nTickStart, int nTickStop )
{
	loadingdisc->SetFastForwardVisible( bState, true );
	g_HltvReplaySystem.SetDemoPlaybackFadeBrackets( nCurrentTick, nTickStart, nTickStop );
}

void CHLClient::RecordDemoPolishUserInput( int nCmdIndex )
{
#ifdef DEMOPOLISH_ENABLED
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	Assert( engine->IsRecordingDemo() );
	Assert( IsDemoPolishEnabled() );	// NOTE: cl_demo_polish_enabled checked in engine.
	
	CUserCmd const* pUserCmd = input->GetUserCmd( nSlot, nCmdIndex );
	Assert( pUserCmd );
	if ( pUserCmd )
	{
		DemoPolish_GetRecorder().RecordUserInput( pUserCmd );
	}
#endif
}

bool CHLClient::CacheReplayRagdolls( const char* pFilename, int nStartTick )
{
#if defined( REPLAY_ENABLED )
	return Replay_CacheRagdolls( pFilename, nStartTick );
#else
	return false;
#endif
}

void CHLClient::ReplayUI_SendMessage( KeyValues *pMsg )
{
#if defined( REPLAY_ENABLED ) && defined( TF_CLIENT_DLL )
	const char *pType = pMsg->GetString( "type", NULL );
	if ( !V_stricmp( pType, "newentry" ) )
	{
		if ( pMsg->GetInt( "showinputdlg", 0 ) )
		{
			// Get a name for the replay, saves to disk, add thumbnail to replay browser
			ShowReplayInputPanel( pMsg );
		}
		else
		{
			// Just add the thumbnail if the replay browser exists
			CReplayBrowserPanel* pReplayBrowser = ReplayUI_GetBrowserPanel();
			if ( pReplayBrowser )
			{
				pReplayBrowser->SendMessage( pMsg );
			}
		}
	}

	// If we're deleting a replay notify the replay browser if necessary
	else if ( !V_stricmp( pType, "delete" ) )
	{
		CReplayBrowserPanel* pReplayBrowser = ReplayUI_GetBrowserPanel();
		if ( pReplayBrowser )
		{
			pReplayBrowser->SendMessage( pMsg );
		}
	}

	// Confirm that the user wants to quit, even if there are unrendered replays
	else if ( !V_stricmp( pType, "confirmquit" ) )
	{
//		ReplayUI_ShowConfirmQuitDlg();

		// Until rendering actually works, just quit - don't display the confirmation dialog
		engine->ClientCmd_Unrestricted( "quit" );
	}
	
	else if ( !V_stricmp( pType, "display_message" ) )
	{
		const char *pLocalizeStr = pMsg->GetString( "localize" );

		// Display a message?
		if ( pLocalizeStr && pLocalizeStr[0] )
		{
			char szLocalized[256];
			g_pVGuiLocalize->ConvertUnicodeToANSI( g_pVGuiLocalize->Find( pLocalizeStr ), szLocalized, sizeof(szLocalized) );
			g_pClientMode->DisplayReplayMessage( szLocalized, -1.0f, NULL, pMsg->GetString( "sound", NULL ), false );
		}
	}

	else if ( !V_stricmp( pType, "render_start" ) )
	{
		extern void ReplayUI_OpenReplayEditPanel();
		ReplayUI_OpenReplayEditPanel();
	}

	else if ( !V_stricmp( pType, "render_complete" ) )
	{
		extern void ReplayUI_HideRenderEditPanel();
		ReplayUI_HideRenderEditPanel();
		ReplayUI_ReloadBrowser();
	}

	// Delete the KeyValues unless delete is explicitly set to 0
	if ( pMsg->GetInt( "should_free", 1 ) )
	{
		pMsg->deleteThis();
	}
#endif
}

// Get the client replay factory
IReplayFactory *CHLClient::GetReplayFactory()
{
#if defined( REPLAY_ENABLED ) && defined( TF_CLIENT_DLL ) // FIXME: Need run-time check for whether replay is enabled
	extern IReplayFactory *g_pReplayFactory;
	return g_pReplayFactory;
#else
	return NULL;
#endif
}

// Clear out the local player's replay pointer so it doesn't get deleted
void CHLClient::ClearLocalPlayerReplayPtr()
{
#if defined( REPLAY_ENABLED )
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	Assert( pLocalPlayer );

	pLocalPlayer->ClearCachedReplayPtr();
#endif
}


int CHLClient::GetScreenWidth()
{
	return ScreenWidth();
}

int CHLClient::GetScreenHeight()
{
	return ScreenHeight();
}

void CHLClient::WriteSaveGameScreenshotOfSize( const char *pFilename, int width, int height, bool bCreatePowerOf2Padded /* = false */, bool bWriteVTF /* = false */ )
{
	view->WriteSaveGameScreenshotOfSize( pFilename, width, height /*, bCreatePowerOf2Padded, bWriteVTF */ );
}

void CHLClient::WriteReplayScreenshot( WriteReplayScreenshotParams_t &params )
{
#if defined( TF_CLIENT_DLL ) && defined( REPLAY_ENABLED ) // FIXME: Need run-time check for whether replay is enabled
	view->WriteReplayScreenshot( params );
#endif
}

void CHLClient::UpdateReplayScreenshotCache()
{
#if defined( TF_CLIENT_DLL ) && defined( REPLAY_ENABLED ) // FIXME: Need run-time check for whether replay is enabled
	view->UpdateReplayScreenshotCache();
#endif
}


// See RenderViewInfo_t
void CHLClient::RenderView( const CViewSetup &setup, int nClearFlags, int whatToDraw )
{
	VPROF("RenderView");
	view->RenderView( setup, setup, nClearFlags, whatToDraw );
}

bool CHLClient::ShouldHideLoadingPlaque( void )
{
	return false;

}

void CHLClient::OnActiveSplitscreenPlayerChanged( int nNewSlot )
{
}

CEG_NOINLINE void CHLClient::OnSplitScreenStateChanged()
{
	VGui_OnSplitScreenStateChanged();
	IterateRemoteSplitScreenViewSlots_Push( true );
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( i );
		GetClientMode()->Layout();
		GetHud().OnSplitScreenStateChanged();
	}
	IterateRemoteSplitScreenViewSlots_Pop();

	CEG_ENCRYPT_FUNCTION( CHLClient_OnSplitScreenStateChanged );

	GetFullscreenClientMode()->Layout( true );

#if 0
	// APS: This cannot be done in this way, the schemes also need to be reloaded as they hook the fonts and
	// various font metrics for custom drawing/sizing etc.
	// See CMatSystemSurface::OnScreenSizeChanged() which is doing the better thing.
	// However, that is still not good enough due to questionable code in CScheme::ReloadFontGlyphs() which
	// prevents the reload due to it's cached concept of the screensize not changing, except the purge is already partialy done,
	// which results in a broken font state.
	// Instead, we are doing the same thing the consoles do (which cannot change video sizes) which is to NOT ditch the glyphs
	// when changing split screen state. When SS for PC gets turned into a primary concept this can be revisited.
	vgui::surface()->ResetFontCaches();
#endif

	// Update visibility for all ents so that the second viewport for the split player guy looks right, etc.
	C_BaseEntityIterator iterator;
	C_BaseEntity *pEnt;
	while ( (pEnt = iterator.Next()) != NULL )	
	{
		pEnt->UpdateVisibility();
	}
}

int CHLClient::GetSpectatorTarget( ClientDLLObserverMode_t* pObserverMode )
{
	if ( pObserverMode )
	{
		*pObserverMode = CLIENT_DLL_OBSERVER_NONE;
	}

	C_CSPlayer *pPlayer = GetLocalOrInEyeCSPlayer();

	if ( pPlayer != NULL )
	{
		return pPlayer->entindex();
	}

	return -1;
}

void CHLClient::CenterStringOff()
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetCenterPrint()->Clear();
	}

}

void CHLClient::OnScreenSizeChanged( int nOldWidth, int nOldHeight )
{
	// Tell split screen system
	VGui_OnScreenSizeChanged();
}

IMaterialProxy *CHLClient::InstantiateMaterialProxy( const char *proxyName )
{
#ifdef GAMEUI_UISYSTEM2_ENABLED
	IMaterialProxy *pProxy = g_pGameUIGameSystem->CreateProxy( proxyName );
	if ( pProxy )
		return pProxy;
#endif
	return GetMaterialProxyDict().CreateProxy( proxyName );
}

vgui::VPANEL CHLClient::GetFullscreenClientDLLVPanel( void )
{
	return VGui_GetFullscreenRootVPANEL();
}

int XBX_GetActiveUserId()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return XBX_GetUserId( GET_ACTIVE_SPLITSCREEN_SLOT() );
}

//-----------------------------------------------------------------------------
// Purpose: Marks entities as touching
// Input  : *e1 - 
//			*e2 - 
//-----------------------------------------------------------------------------
void CHLClient::MarkEntitiesAsTouching( IClientEntity *e1, IClientEntity *e2 )
{
	CBaseEntity *entity = e1->GetBaseEntity();
	CBaseEntity *entityTouched = e2->GetBaseEntity();
	if ( entity && entityTouched )
	{
		trace_t tr;
		UTIL_ClearTrace( tr );
		tr.endpos = (entity->GetAbsOrigin() + entityTouched->GetAbsOrigin()) * 0.5;
		entity->PhysicsMarkEntitiesAsTouching( entityTouched, tr );
	}
}

class CKeyBindingListenerMgr : public IKeyBindingListenerMgr
{
public:
	struct BindingListeners_t
	{
		BindingListeners_t()
		{
		}

		BindingListeners_t( const BindingListeners_t &other )
		{
			m_List.CopyArray( other.m_List.Base(), other.m_List.Count() );
		}

		CUtlVector< IKeyBindingListener * > m_List;
	};

	// Callback when button is bound
	virtual void AddListenerForCode( IKeyBindingListener *pListener, ButtonCode_t buttonCode )
	{
		CUtlVector< IKeyBindingListener * > &list = m_CodeListeners[ buttonCode ];
		if ( list.Find( pListener ) != list.InvalidIndex() )
			return;
		list.AddToTail( pListener );
	}

	// Callback whenver binding is set to a button
	virtual void AddListenerForBinding( IKeyBindingListener *pListener, char const *pchBindingString )
	{
		int idx = m_BindingListeners.Find( pchBindingString );
		if ( idx == m_BindingListeners.InvalidIndex() )
		{
			idx = m_BindingListeners.Insert( pchBindingString );
		}

		CUtlVector< IKeyBindingListener * > &list = m_BindingListeners[ idx ].m_List;
		if ( list.Find( pListener ) != list.InvalidIndex() )
			return;
		list.AddToTail( pListener );
	}

	virtual void RemoveListener( IKeyBindingListener *pListener )
	{
		for ( int i = 0; i < ARRAYSIZE( m_CodeListeners ); ++i )
		{
			CUtlVector< IKeyBindingListener * > &list = m_CodeListeners[ i ];
			list.FindAndRemove( pListener );
		}

		for ( int i = m_BindingListeners.First(); i != m_BindingListeners.InvalidIndex(); i = m_BindingListeners.Next( i ) )
		{
			CUtlVector< IKeyBindingListener * > &list = m_BindingListeners[ i ].m_List;
			list.FindAndRemove( pListener );
		}
	}

	void OnKeyBindingChanged( ButtonCode_t buttonCode, char const *pchKeyName, char const *pchNewBinding )
	{
		int nSplitScreenSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

		CUtlVector< IKeyBindingListener * > &list = m_CodeListeners[ buttonCode ];
		for ( int i = 0 ; i < list.Count(); ++i )
		{
			list[ i ]->OnKeyBindingChanged( nSplitScreenSlot, buttonCode, pchKeyName, pchNewBinding );
		}

		int idx = m_BindingListeners.Find( pchNewBinding );
		if ( idx != m_BindingListeners.InvalidIndex() )
		{
			CUtlVector< IKeyBindingListener * > &list = m_BindingListeners[ idx ].m_List;
			for ( int i = 0 ; i < list.Count(); ++i )
			{
				list[ i ]->OnKeyBindingChanged( nSplitScreenSlot, buttonCode, pchKeyName, pchNewBinding );
			}
		}
	}

private:
	CUtlVector< IKeyBindingListener * > m_CodeListeners[ BUTTON_CODE_COUNT ];
	CUtlDict< BindingListeners_t, int > m_BindingListeners;
};

static CKeyBindingListenerMgr g_KeyBindingListenerMgr;

IKeyBindingListenerMgr *g_pKeyBindingListenerMgr = &g_KeyBindingListenerMgr;
void CHLClient::OnKeyBindingChanged( ButtonCode_t buttonCode, char const *pchKeyName, char const *pchNewBinding )
{
	g_KeyBindingListenerMgr.OnKeyBindingChanged( buttonCode, pchKeyName, pchNewBinding );
}

void CHLClient::SetBlurFade( float scale )
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetClientMode()->SetBlurFade( scale );
	}
}

void CHLClient::ResetHudCloseCaption()
{
	if ( !IsGameConsole() )
	{
		// only xbox needs to force the close caption system to remount
		return;
	}

	if ( m_pHudCloseCaption )
	{
		// force the caption dictionary to remount
		m_pHudCloseCaption->InitCaptionDictionary( NULL, true );
	}
}

void CHLClient::Hud_SaveStarted()
{
	CHudSaveStatus *pSaveStatus = GET_FULLSCREEN_HUDELEMENT( CHudSaveStatus );
	if ( pSaveStatus )
	{
		pSaveStatus->SaveStarted();
	}
}

void CHLClient::ShutdownMovies()
{
	//VGui_StopAllVideoPanels();
}

void CHLClient::GetStatus( char *buffer, int bufsize )
{
	UTIL_GetClientStatusText( buffer, bufsize );
}

#if defined ( CSTRIKE15 )
bool CHLClient::IsChatRaised( void )
{
	SFHudChat* pChat = GET_HUDELEMENT( SFHudChat );

	if ( pChat == NULL )
	{
		return false;
	}
	else
	{
		return pChat->ChatRaised();
	}
}

bool CHLClient::IsRadioPanelRaised( void )
{
	SFHudRadio* pRadio = GET_HUDELEMENT( SFHudRadio );

	if ( pRadio == NULL )
	{
		return false;
	}
	else
	{
		return pRadio->PanelRaised();
	}
}


bool CHLClient::IsBindMenuRaised( void )
{
	return COptionsScaleform::IsBindMenuRaised();
}

bool CHLClient::IsTeamMenuRaised( void )
{
	if ( !GetViewPortInterface() )
	{
		return false;
	}

	IViewPortPanel * pTeam = GetViewPortInterface()->FindPanelByName( PANEL_TEAM );
	if ( pTeam && pTeam->IsVisible() )
	{
		return true;
	}

	return false;
}

bool CHLClient::IsLoadingScreenRaised( void )
{
	return CLoadingScreenScaleform::IsOpen();
}

#endif // CSTRIKE15

#if defined(_PS3)

int CHLClient::GetDrawFlags( void )
{
	return g_viewBuilder.GetDrawFlags();
}

int CHLClient::GetBuildViewID( void )
{
	return g_viewBuilder.GetBuildViewID();
}

bool CHLClient::IsSPUBuildWRJobsOn( void )
{
	return g_viewBuilder.IsSPUBuildRWJobsOn();
}

void CHLClient::CacheFrustumData( Frustum_t *pFrustum, Frustum_t *pAreaFrustum, void *pRenderAreaBits, int numArea, bool bViewerInSolidSpace )
{
	g_viewBuilder.CacheFrustumData( pFrustum, pAreaFrustum, pRenderAreaBits, numArea, bViewerInSolidSpace );
}

void *CHLClient::GetBuildViewVolumeCuller( void )
{
	return g_viewBuilder.GetBuildViewVolumeCuller();
}

Frustum_t *CHLClient::GetBuildViewFrustum( void )
{
	return g_viewBuilder.GetBuildViewFrustum();
}

Frustum_t *CHLClient::GetBuildViewAreaFrustum( void )
{
	return g_viewBuilder.GetBuildViewAreaFrustum();
}

unsigned char *CHLClient::GetBuildViewRenderAreaBits( void )
{
	return g_viewBuilder.GetBuildViewRenderAreaBits();
}

#else

bool CHLClient::IsBuildWRThreaded( void )
{
	return g_viewBuilder.GetBuildWRThreaded();
}

void CHLClient::QueueBuildWorldListJob( CJob* pJob )
{
	g_viewBuilder.QueueBuildWorldListJob( pJob );
}

void CHLClient::CacheFrustumData( const Frustum_t& frustum, const CUtlVector< Frustum_t, CUtlMemoryAligned< Frustum_t,16 > >& aeraFrustums )
{
	g_viewBuilder.CacheFrustumData( frustum, aeraFrustums );
}

const Frustum_t* CHLClient::GetBuildViewFrustum( void ) const
{
	return g_viewBuilder.GetBuildViewFrustum();
}

const CUtlVector< Frustum_t, CUtlMemoryAligned< Frustum_t,16 > >* CHLClient::GetBuildViewAeraFrustums( void ) const
{
	return g_viewBuilder.GetBuildViewAeraFrustums();
}

#endif // PS3, CConcurrentView helper fns


bool CHLClient::IsSubscribedMap( const char *pchMapName, bool bOnlyOnDisk )
{
#if !defined ( NO_STEAM ) && defined( CSTRIKE15 )
	return g_CSGOWorkshopMaps.IsSubscribedMap( pchMapName, bOnlyOnDisk );
#endif
	return false;
}

bool CHLClient::IsFeaturedMap( const char *pchMapName, bool bOnlyOnDisk )
{
#if !defined ( NO_STEAM ) && defined( CSTRIKE15 )
	return g_CSGOWorkshopMaps.IsFeaturedMap( pchMapName, bOnlyOnDisk );
#endif
	return false;
}

void CHLClient::DownloadCommunityMapFile( PublishedFileId_t id )
{
#if !defined ( NO_STEAM ) && defined( CSTRIKE15 )
	g_CSGOWorkshopMaps.DownloadMapFile( id );
#endif
}

float CHLClient::GetUGCFileDownloadProgress( PublishedFileId_t id )
{
#if !defined ( NO_STEAM ) && defined( CSTRIKE15 )
	return g_CSGOWorkshopMaps.GetFileDownloadProgress( id );
#endif
}

void CHLClient::RecordUIEvent( const char* szEvent )
{
	/* Removed for partner depot */
}


void CHLClient::OnDemoPlaybackTimeJump()
{
	if ( C_HLTVCamera *pCamera = HLTVCamera() )
	{
		//Cbuf_AddText( Cbuf_GetCurrentPlayer(), "spec_prev;spec_mode 4;+showscores;-showscores\n" ); // go to the next/prev available player; just go to some player, if they've already connected
		pCamera->SetMode( 4 );
		pCamera->SpecNextPlayer( true );
	}
}

// Inventory access
float CHLClient::FindInventoryItemWithMaxAttributeValue( char const *szItemType, char const *szAttrClass )
{
	CCSPlayerInventory *pLocalInv = CSInventoryManager()->GetLocalCSInventory();
	return pLocalInv ? pLocalInv->FindInventoryItemWithMaxAttributeValue( szItemType, szAttrClass ) : -1.0f;
}

void CHLClient::DetermineSubscriptionKvToAdvertise( KeyValues *kvLocalPlayer )
{
	/* Removed for partner depot */
}

class CHLClientAutoRichPresenceUpdateOnConnect
{
public:
	CHLClientAutoRichPresenceUpdateOnConnect() : m_SteamCallback_OnServersConnected( this, &CHLClientAutoRichPresenceUpdateOnConnect::Steam_OnServersConnected ) {}

	STEAM_CALLBACK( CHLClientAutoRichPresenceUpdateOnConnect, Steam_OnServersConnected, SteamServersConnected_t, m_SteamCallback_OnServersConnected )
	{
		( void ) clientdll->GetRichPresenceStatusString();
	}
};

char const * CHLClient::GetRichPresenceStatusString()
{
	ISteamFriends *pf = steamapicontext->SteamFriends();
	if ( !pf )
		return "";

	bool bConnectedToServer = engine->IsInGame();

	static CHLClientAutoRichPresenceUpdateOnConnect s_RPUpdater; // construct auto RP updater upon first rich presence update
	
	// Status string
	static CFmtStr sRichPresence;
	sRichPresence.Clear();

	// Map (Dust II, Office, etc.)
	char const *szMap = NULL;
	char const *szGameMap = NULL;
	
	if ( bConnectedToServer )
	{
		szMap = engine->GetLevelNameShort();
		szGameMap = szMap;
		{	// Resolve known map names
				 if ( !V_stricmp( szGameMap, "cs_assault"		)	) szGameMap = "Assault"					;
			else if ( !V_stricmp( szGameMap, "cs_italy"			)	) szGameMap = "Italy"					;
			else if ( !V_stricmp( szGameMap, "cs_militia"		)	) szGameMap = "Militia"					;
			else if ( !V_stricmp( szGameMap, "cs_office"		)	) szGameMap = "Office"					;
			else if ( !V_stricmp( szGameMap, "de_aztec"			)	) szGameMap = "Aztec"					;
			else if ( !V_stricmp( szGameMap, "de_dust"			)	) szGameMap = "Dust"					;
			else if ( !V_stricmp( szGameMap, "de_dust2"			)	) szGameMap = "Dust II"					;
			else if ( !V_stricmp( szGameMap, "de_mirage"		)	) szGameMap = "Mirage"					;
			else if ( !V_stricmp( szGameMap, "de_overpass"		)	) szGameMap = "Overpass"				;
			else if ( !V_stricmp( szGameMap, "de_cbble"			)	) szGameMap = "Cobblestone"				;
			else if ( !V_stricmp( szGameMap, "de_train"			)	) szGameMap = "Train"					;
			else if ( !V_stricmp( szGameMap, "de_inferno"		)	) szGameMap = "Inferno"					;
			else if ( !V_stricmp( szGameMap, "de_nuke"			)	) szGameMap = "Nuke"					;
			else if ( !V_stricmp( szGameMap, "de_shorttrain"	)	) szGameMap = "Shorttrain"				;
			else if ( !V_stricmp( szGameMap, "de_shortdust"		)	) szGameMap = "Shortdust"				;
			else if ( !V_stricmp( szGameMap, "de_vertigo"		)	) szGameMap = "Vertigo"					;
			else if ( !V_stricmp( szGameMap, "de_balkan"		)	) szGameMap = "Balkan"					;
			else if ( !V_stricmp( szGameMap, "random"			)	) szGameMap = "Random"					;
			else if ( !V_stricmp( szGameMap, "ar_baggage"		)	) szGameMap = "Baggage"					;
			else if ( !V_stricmp( szGameMap, "ar_monastery"		)	) szGameMap = "Monastery"				;
			else if ( !V_stricmp( szGameMap, "ar_shoots"		)	) szGameMap = "Shoots"					;
			else if ( !V_stricmp( szGameMap, "de_embassy"		)	) szGameMap = "Embassy"					;
			else if ( !V_stricmp( szGameMap, "de_bank"			)	) szGameMap = "Bank"					;
			else if ( !V_stricmp( szGameMap, "de_lake"			)	) szGameMap = "Lake"					;
			else if ( !V_stricmp( szGameMap, "de_depot"			)	) szGameMap = "Depot"					;
			else if ( !V_stricmp( szGameMap, "de_safehouse"		)	) szGameMap = "Safehouse"				;
			else if ( !V_stricmp( szGameMap, "de_sugarcane"		)	) szGameMap = "Sugarcane"				;
			else if ( !V_stricmp( szGameMap, "de_stmarc"		)	) szGameMap = "St. Marc"				;
			else if ( !V_stricmp( szGameMap, "training1"		)	) szGameMap = "Weapons Course"			;
			else if ( !V_stricmp( szGameMap, "cs_museum"		)	) szGameMap = "Museum"					;
			else if ( !V_stricmp( szGameMap, "cs_thunder"		)	) szGameMap = "Thunder"					;
			else if ( !V_stricmp( szGameMap, "de_favela"		)	) szGameMap = "Favela"					;
			else if ( !V_stricmp( szGameMap, "cs_downtown"		)	) szGameMap = "Downtown"				;
			else if ( !V_stricmp( szGameMap, "de_seaside"		)	) szGameMap = "Seaside"					;
			else if ( !V_stricmp( szGameMap, "de_library"		)	) szGameMap = "Library"					;
			else if ( !V_stricmp( szGameMap, "cs_motel"			)	) szGameMap = "Motel"					;
			else if ( !V_stricmp( szGameMap, "de_cache"			)	) szGameMap = "Cache"					;
			else if ( !V_stricmp( szGameMap, "de_ali"			)	) szGameMap = "Ali"						;
			else if ( !V_stricmp( szGameMap, "de_ruins"			)	) szGameMap = "Ruins"					;
			else if ( !V_stricmp( szGameMap, "de_chinatown"		)	) szGameMap = "Chinatown"				;
			else if ( !V_stricmp( szGameMap, "de_gwalior"		)	) szGameMap = "Gwalior"					;
			else if ( !V_stricmp( szGameMap, "cs_agency"		)	) szGameMap = "Agency"					;
			else if ( !V_stricmp( szGameMap, "cs_siege"			)	) szGameMap = "Siege"					;
			else if ( !V_stricmp( szGameMap, "de_blackgold"		)	) szGameMap = "Black Gold"				;
			else if ( !V_stricmp( szGameMap, "de_castle"		)	) szGameMap = "Castle"					;
			else if ( !V_stricmp( szGameMap, "de_overgrown"		)	) szGameMap = "Overgrown"				;
			else if ( !V_stricmp( szGameMap, "de_mist"			)	) szGameMap = "Mist"					;
			else if ( !V_stricmp( szGameMap, "cs_rush"			)	) szGameMap = "Rush"					;
			else if ( !V_stricmp( szGameMap, "cs_insertion"		)	) szGameMap = "Insertion"				;
			else if ( !V_stricmp( szGameMap, "cs_workout"		)	) szGameMap = "Workout"					;
			else if ( !V_stricmp( szGameMap, "cs_backalley"		)	) szGameMap = "Back Alley"				;
			else if ( !V_stricmp( szGameMap, "de_bazaar"		)	) szGameMap = "Bazaar"					;
			else if ( !V_stricmp( szGameMap, "de_marquis"		)	) szGameMap = "Marquis"					;
			else if ( !V_stricmp( szGameMap, "de_season"		)	) szGameMap = "Season"					;
			else if ( !V_stricmp( szGameMap, "de_facade"		)	) szGameMap = "Facade"					;
			else if ( !V_stricmp( szGameMap, "de_log"			)	) szGameMap = "Log"						;
			else if ( !V_stricmp( szGameMap, "de_rails"			)	) szGameMap = "Rails"					;
			else if ( !V_stricmp( szGameMap, "de_resort"		)	) szGameMap = "Resort"					;
			else if ( !V_stricmp( szGameMap, "de_zoo"			)	) szGameMap = "Zoo"						;
			else if ( !V_stricmp( szGameMap, "cs_cruise"		)	) szGameMap = "Cruise"					;
			else if ( !V_stricmp( szGameMap, "de_coast"			)	) szGameMap = "Coast"					;
			else if ( !V_stricmp( szGameMap, "de_empire"		)	) szGameMap = "Empire"					;
			else if ( !V_stricmp( szGameMap, "de_mikla"			)	) szGameMap = "Mikla"					;
			else if ( !V_stricmp( szGameMap, "de_royal"			)	) szGameMap = "Royal"					;
			else if ( !V_stricmp( szGameMap, "de_santorini"		)	) szGameMap = "Santorini"				;
			else if ( !V_stricmp( szGameMap, "de_tulip"			)	) szGameMap = "Tulip"					;
			else if ( !V_stricmp( szGameMap, "gd_crashsite"		)	) szGameMap = "Crashsite"				;
			else if ( !V_stricmp( szGameMap, "gd_lake"			)	) szGameMap = "Lake"					;
			else if ( !V_stricmp( szGameMap, "gd_bank"			)	) szGameMap = "Bank"					;
			else if ( !V_stricmp( szGameMap, "gd_cbble"			)	) szGameMap = "Cobblestone"				;
			else if ( !V_stricmp( szGameMap, "gd_sugarcane"		)	) szGameMap = "Sugarcane"				;
			else if ( !V_stricmp( szGameMap, "coop_cementplant"	)	) szGameMap = "Phoenix Compound"		;
		}
	}

	// Map group
	char const *szMapGroup = NULL;
	char const *szGameMapGroup = NULL;
	
	if ( bConnectedToServer )
	{
		szMapGroup = engine->GetMapGroupName();
		szGameMapGroup = szMapGroup;
		{
				 if ( !V_stricmp( szGameMapGroup, "mg_active"		)	) szGameMapGroup = "Active Duty"			;
			else if ( !V_stricmp( szGameMapGroup, "mg_reserves"		)	) szGameMapGroup = "Reserves"				;
			else if ( !V_stricmp( szGameMapGroup, "mg_op_06"		)	) szGameMapGroup = "Operation Bloodhound"	;
		}
	}
	
	// Game mode (Arms Race, Demolition, etc.)
	char const *szMode = NULL;
	char const *szGameMode = NULL;
	
	if ( bConnectedToServer )
	{
		extern ConVar game_type;
		extern ConVar game_mode;
		switch ( game_type.GetInt() )
		{
		case 0:
			switch ( game_mode.GetInt() )
			{
			case 1:
				szMode = "competitive";
				szGameMode = "Competitive";
				break;
			default:
				szMode = "casual";
				szGameMode = "Casual";
				break;
			}
			break;
		case 1:
			switch ( game_mode.GetInt() )
			{
			case 0:
				szMode = "gungameprogressive";
				szGameMode = "Arms Race";
				break;
			case 1:
				szMode = "gungametrbomb";
				szGameMode = "Demolition";
				break;
			default:
				szMode = "deathmatch";
				szGameMode = "Deathmatch";
				break;
			}
			break;
		case 2:
			szMode = "training";
			szGameMode = "Weapons Course";
			break;
		case 4:
			szMode = "cooperative";
			szGameMode = "Mission";
			break;
		default:
			szMode = "custom";
			szGameMode = "Custom";
			break;
		}
	}
	
	// Score of the match
	char chScore[64] = {};
	char const *szScore = NULL;
	if ( bConnectedToServer && !g_bEngineIsHLTV && CSGameRules() )
	{
		// Append the score using local player's team first, or CT first
		bool bLocalPlayerT = false;
		if ( C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer() )
		{
			bLocalPlayerT = ( pLocalPlayer->GetTeamNumber() == TEAM_TERRORIST );
		}

		C_Team *tTeam = GetGlobalTeam( TEAM_TERRORIST );
		int nTscore = tTeam ? tTeam->Get_Score() : 0;
		C_Team *ctTeam = GetGlobalTeam( TEAM_CT );
		int nCTscore = ctTeam ? ctTeam->Get_Score() : 0;

		if ( nTscore + nCTscore > 0 )
		{
			V_sprintf_safe( chScore, "[ %d : %d ]", bLocalPlayerT ? nTscore : nCTscore, bLocalPlayerT ? nCTscore : nTscore );
			szScore = chScore;
		}
	}

	// Server type
	char const *szServerType = NULL; // V for Valve, P for Pinion
	char const *szConnectAddress = NULL;
	bool bCanInvite = false;
	bool bCanWatch = false;
	bool bPlayingDemo = engine->IsPlayingDemo();
	CDemoPlaybackParameters_t const *pDemoPlaybackParameters = bPlayingDemo ? engine->GetDemoPlaybackParameters() : NULL;
	bool bWatchingLiveBroadcast = bPlayingDemo && pDemoPlaybackParameters && pDemoPlaybackParameters->m_bPlayingLiveRemoteBroadcast;
	if ( bConnectedToServer )
	{
		if ( bWatchingLiveBroadcast )
		{
			if ( CSGameRules() && CSGameRules()->IsValveDS() )
			{
				szServerType = "kv";

				// Some official game modes aren't watchable
				if ( !CSGameRules()->IsPlayingCooperativeGametype() )
					bCanWatch = true;
			}
		}
		else if ( !bPlayingDemo )
		{
			// Check if the connection is to a Valve official server
			INetChannelInfo *pNetChanInfo = engine->GetNetChannelInfo();
			netadr_t adrRemote( pNetChanInfo ? pNetChanInfo->GetAddress() : "127.0.0.1" );
			if ( pNetChanInfo && ( pNetChanInfo->IsLoopback() || adrRemote.IsLocalhost() ) )
			{
				szServerType = "offline";
			}
			else if ( pNetChanInfo )
			{
				szConnectAddress = pNetChanInfo->GetAddress();

				if ( CSGameRules() && CSGameRules()->IsValveDS() )
				{
					szServerType = "kv";

					// Some official game modes aren't watchable
					if ( !CSGameRules()->IsPlayingCooperativeGametype() )
						bCanWatch = true;
				}

				if ( !szServerType )
				{
					szServerType = "community";

					if ( ( steamapicontext->SteamUtils()->GetConnectedUniverse() != k_EUniversePublic ) && ( cl_join_advertise.GetInt() >= 3 ) )
					{	// cl_join_advertise 3 can override SteamBeta testing to show up as Valve servers
						szServerType = "kv";
						bCanWatch = true;
					}
				}

				bCanInvite = ( CSGameRules() && !CSGameRules()->IsQueuedMatchmaking() &&	// queued official competitive
					!engine->IsHLTV() &&			// demo preview or GOTV
					( adrRemote.GetPort() != 1 )	// SteamID steamcnx for a private match, address is not valid
					);
				if ( bCanInvite && Q_strcmp( szServerType, "community" ) )
				{
					// Make sure we don't have a max number of players
					int numPlayers = 0;
					for ( int j = 1; j <= gpGlobals->maxClients; j++ )
					{
						CBasePlayer *pPlayer = UTIL_PlayerByIndex( j );
						if ( pPlayer && !pPlayer->IsBot() )
							++numPlayers;
					}
					int numPlayersLimit = 10;
					if ( !Q_strcmp( szMode, "casual" ) )
						numPlayersLimit = 20;
					else if ( !Q_strcmp( szMode, "deathmatch" ) )
						numPlayersLimit = 16;

					if ( numPlayersLimit && ( numPlayers >= numPlayersLimit ) )
						bCanInvite = false;
				}
			}
		}
	}

	// Activity
	char const *szActivity = NULL;
	char const *szGameActivity = NULL;
	if ( bConnectedToServer && bPlayingDemo && !bWatchingLiveBroadcast )
	{
		CDemoPlaybackParameters_t const *pParams = engine->GetDemoPlaybackParameters();
		if ( pParams && pParams->m_bAnonymousPlayerIdentity )
		{
			szActivity = "overwatch";
			szGameActivity = "Overwatch";
		}
		else
		{
			szActivity = "review";
			szGameActivity = "Replaying";
		}
	}
	else if ( bConnectedToServer && engine->IsHLTV() )
	{
		szActivity = "watch";
		szGameActivity = "Watching";
	}
	else if ( szServerType && !Q_strcmp( "offline", szServerType ) )
	{
		szActivity = "offline";
		szGameActivity = "Offline";
	}
	else if ( szServerType && !Q_strcmp( "community", szServerType ) )
	{
		szActivity = "community";
		szGameActivity = "Community";
	}

	//
	// Build the special status string
	// [Activity] [Mode] [Map / MapGroup] [Score]
	//
	if ( szGameActivity && *szGameActivity )
	{
		if ( sRichPresence.Length() > 0 )
			sRichPresence.Append( ' ' );
		sRichPresence.AppendFormat( "%s", szGameActivity );
	}
	if ( szGameMode && *szGameMode )
	{
		if ( sRichPresence.Length() > 0 )
			sRichPresence.Append( ' ' );
		sRichPresence.AppendFormat( "%s", szGameMode );
	}
	if ( szGameMap && *szGameMap )
	{
		if ( sRichPresence.Length() > 0 )
			sRichPresence.Append( ' ' );
		sRichPresence.AppendFormat( "%s", szGameMap );
	}
	if ( szScore && *szScore )
	{
		if ( sRichPresence.Length() > 0 )
			sRichPresence.Append( ' ' );
		sRichPresence.AppendFormat( "%s", szScore );
	}

	if ( !sRichPresence.Length() )
	{	// Default RP
		sRichPresence.AppendFormat( "Playing CS:GO" );
	}

	pf->SetRichPresence( "status", sRichPresence.Get() );
	pf->SetRichPresence( "version", CFmtStr( "%d", engine->GetEngineBuildNumber() ) );
	pf->SetRichPresence( "time", CFmtStr( "%f", Plat_FloatTime() ) );	// cause RP upload in case we drop from Steam and reconnect
	pf->SetRichPresence( "game:act", szActivity );
	pf->SetRichPresence( "game:mode", szMode );
	pf->SetRichPresence( "game:mapgroupname", szMapGroup );
	pf->SetRichPresence( "game:map", szMap );
	pf->SetRichPresence( "game:score", szScore );
	pf->SetRichPresence( "game:server", szServerType );
	pf->SetRichPresence( "watch", bCanWatch ? "1" : NULL );

	if ( bCanInvite && szConnectAddress )
	{
		uint32 uiRandomThing = RandomInt( 1, INT_MAX );
		CUtlString strConnectHash;
		ns_address adr;
		if ( adr.SetFromString( szConnectAddress ) && adr.GetAddressType() == NSAT_PROXIED_GAMESERVER )
			strConnectHash.Format( "%u:%u:%llu", uiRandomThing, steamapicontext->SteamUser()->GetSteamID().GetAccountID(), adr.m_steamID.GetSteamID().ConvertToUint64() );
		else
			strConnectHash.Format( "%u:%u:%s", uiRandomThing, steamapicontext->SteamUser()->GetSteamID().GetAccountID(), szConnectAddress );
		CRC32_t crcConnectHash = CRC32_ProcessSingleBuffer( strConnectHash.Access(), strConnectHash.Length() );

		bool bPublicConnect = false;
		if ( !Q_strcmp( szServerType, "community" ) )
			bPublicConnect = ( cl_join_advertise.GetInt() >= 2 );
		else
			bPublicConnect = ( cl_join_advertise.GetInt() >= 1 );

		CFmtStr fmtConnectValue( "+gcconnect%08X%08X%08X",
			uiRandomThing, steamapicontext->SteamUser()->GetSteamID().GetAccountID(), crcConnectHash );

		pf->SetRichPresence( "connect", bPublicConnect ? fmtConnectValue.Access() : NULL );
		pf->SetRichPresence( "connect_private", fmtConnectValue.Access() );
	}
	else
	{
		pf->SetRichPresence( "connect", NULL );
		pf->SetRichPresence( "connect_private", NULL );
	}

	return sRichPresence.Get();
}

int CHLClient::GetInEyeEntity() const
{
	C_CSPlayer* player = GetLocalOrInEyeCSPlayer();
	if (player != nullptr)
	{
		return player->entindex();
	}
	return -1;
}


bool CHLClient::ValidateSignedEvidenceHeader( char const *szKey, void const *pvHeader, CDemoPlaybackParameters_t *pPlaybackParameters )
{
	/* Removed for partner depot */
	return true;
}

void CHLClient::PrepareSignedEvidenceData( void *pvData, int numBytes, CDemoPlaybackParameters_t const *pPlaybackParameters )
{
	/* Removed for partner depot */
}

bool CHLClient::ShouldSkipEvidencePlayback( CDemoPlaybackParameters_t const *pPlaybackParameters )
{
	/* Removed for partner depot */
	return true;
}

// Scaleform slot controller
IScaleformSlotInitController * CHLClient::GetScaleformSlotInitController()
{
	/* Removed for partner depot */
	return nullptr;
}

bool CHLClient::IsConnectedUserInfoChangeAllowed( IConVar *pCvar )
{
	return CSGameRules() ? CSGameRules()->IsConnectedUserInfoChangeAllowed( NULL ) : true;
}

void CHLClient::OnCommandDuringPlayback( char const *cmd )
{
	/* Removed for partner depot */
}

void CHLClient::RetireAllPlayerDecals( bool bRenderContextValid )
{
	extern void OnPlayerDecalsLevelShutdown();
	OnPlayerDecalsLevelShutdown();

	if ( bRenderContextValid )
	{
		// If the render context is valid (i.e. manual r_cleardecals)
		// then we should immediately update and reapply to avoid flickers
		extern void OnPlayerDecalsUpdate();
		OnPlayerDecalsUpdate();
	}
}

void CHLClient::EngineGotvSyncPacket( const CEngineGotvSyncPacket *pPkt )
{
	/* Removed for partner depot */
}

void CHLClient::OnTickPre( int tickcount )
{
#if defined( CSTRIKE15 ) && !defined( CSTRIKE_REL_BUILD )
	// We always strip this out in REL builds. 
	// We're about to tick over, notify g_pFatDemoRecorder so it can do its magic.
	g_pFatDemoRecorder->OnTickPre( tickcount );
#endif
}

class ClientJob_EMsgGCCStrike15_GotvSyncPacket : public GCSDK::CGCClientJob
{
public:
	explicit ClientJob_EMsgGCCStrike15_GotvSyncPacket( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient )
	{
	}

	virtual bool BYieldingRunJobFromMsg( GCSDK::IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CMsgGCCStrike15_GotvSyncPacket> msg( pNetPacket );
		return engine->EngineGotvSyncPacket( &msg.Body().data() );
	}
};
GC_REG_CLIENT_JOB( ClientJob_EMsgGCCStrike15_GotvSyncPacket, k_EMsgGCCStrike15_v2_GotvSyncPacket );



//-----------------------------------------------------------------------------
// Purpose: Spew application info (primarily for log file data mining)
//-----------------------------------------------------------------------------
void SpewInstallStatus( void )
{
#if defined( _X360 )
	g_pXboxInstaller->SpewStatus();
#endif
}



extern IViewRender *view;

//-----------------------------------------------------------------------------
// Purpose: interface from materialsystem to client, currently just for recording into tools
//-----------------------------------------------------------------------------
class CClientMaterialSystem : public IClientMaterialSystem
{
	virtual HTOOLHANDLE GetCurrentRecordingEntity()
	{
		if ( !ToolsEnabled() )
			return HTOOLHANDLE_INVALID;

		if ( !clienttools->IsInRecordingMode() )
			return HTOOLHANDLE_INVALID;

		const C_BaseEntity *pEnt = NULL;
		if( m_pProxyData ) //dynamic_cast not possible with void *. Just going to have to search to verify that it actually is an entity
		{
			CClientEntityList &entList = ClientEntityList();
			C_BaseEntity *pIter = entList.FirstBaseEntity();
			while( pIter )
			{
				if( (pIter == m_pProxyData) || (pIter->GetClientRenderable() == m_pProxyData) )
				{
					pEnt = pIter;
					break;
				}
				
				pIter = entList.NextBaseEntity( pIter );
			}
		}
		
		if( !pEnt && (materials->GetThreadMode() == MATERIAL_SINGLE_THREADED) )
		{
			pEnt = view->GetCurrentlyDrawingEntity();
		}

		if ( !pEnt || !pEnt->IsToolRecording() )
			return HTOOLHANDLE_INVALID;

		return pEnt->GetToolHandle();
	}
	virtual void PostToolMessage( HTOOLHANDLE hEntity, KeyValues *pMsg )
	{
		ToolFramework_PostToolMessage( hEntity, pMsg );
	}
	virtual void SetMaterialProxyData( void *pProxyData )
	{
		m_pProxyData = pProxyData;
	}

	void *m_pProxyData;
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CClientMaterialSystem s_ClientMaterialSystem;
IClientMaterialSystem *g_pClientMaterialSystem = &s_ClientMaterialSystem;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CClientMaterialSystem, IClientMaterialSystem, VCLIENTMATERIALSYSTEM_INTERFACE_VERSION, s_ClientMaterialSystem );

