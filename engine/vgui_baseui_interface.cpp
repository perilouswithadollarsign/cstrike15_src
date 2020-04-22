//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Implements all the functions exported by the GameUI dll
//
// $NoKeywords: $
//===========================================================================//


#include "client_pch.h"

#include "tier0/platform.h"

#ifdef IS_WINDOWS_PC
#include "winlite.h"

#elif OSX
	#include <Carbon/Carbon.h>
#endif
#include "appframework/ilaunchermgr.h"
#include <vgui_controls/Panel.h>
#include <vgui_controls/EditablePanel.h>
#include <matsys_controls/matsyscontrols.h>
#include <vgui/Cursor.h>
#include <vgui_controls/PHandle.h>
#include "keys.h"
#include "console.h"
#include "gl_matsysiface.h"
#include "cdll_engine_int.h"
#include "demo.h"
#include "sys_dll.h"
#include "sound.h"
#include "soundflags.h"
#include "filesystem_engine.h"
#include "igame.h"
#include "con_nprint.h"
#include "vgui_DebugSystemPanel.h"
#include "tier0/vprof.h"
#include "cl_demoactionmanager.h"
#include "enginebugreporter.h"
#include "engineperftools.h"
#include "icolorcorrectiontools.h"
#include "tier0/icommandline.h"
#include "client.h"
#include "server.h"
#include "sys.h" // Sys_GetRegKeyValue()
#include "vgui_drawtreepanel.h"
#include "vgui_vprofpanel.h"
#include "vgui/vgui.h"
#include "vgui/IInput.h"
#include <vgui/IInputInternal.h>
#include "vgui_controls/AnimationController.h"
#include "vgui_vprofgraphpanel.h"
#include "vgui_texturebudgetpanel.h"
#include "vgui_budgetpanel.h"
#include "Steam.h" // for SteamGetUser()
#include "ivideomode.h"
#include "cl_pluginhelpers.h"
#include "cl_main.h" // CL_IsHL2Demo()
#include "cl_steamauth.h"
#include "inputsystem/iinputstacksystem.h"

// interface to gameui dll
#include <GameUI/IGameUI.h>
#include <GameUI/IGameConsole.h>

// interface to expose vgui root panels
#include <ienginevgui.h>
#include "VGuiMatSurface/IMatSystemSurface.h"

#include "cl_texturelistpanel.h"
#include "cl_demouipanel.h"
#include "cl_foguipanel.h"
#include "cl_txviewpanel.h"

// vgui2 interface
// note that GameUI project uses ..\public\vgui and ..\public\vgui_controls, not ..\utils\vgui\include
#include <vgui/vgui.h>
#include <vgui/Cursor.h>
#include <keyvalues.h>
#include <vgui/ILocalize.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>
#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include <vgui_controls/EditablePanel.h>

#include <vgui_controls/MenuButton.h>
#include <vgui_controls/Menu.h>
#include <vgui_controls/PHandle.h>

#include "IVguiModule.h"
#include "vgui_baseui_interface.h"
#include "vgui_DebugSystemPanel.h"
#include "toolframework/itoolframework.h"
#include "filesystem/IQueuedLoader.h"
#include "LoadScreenUpdate.h"
#include "tier0/etwprof.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

#include "vgui_askconnectpanel.h"
#include "tier1/tokenset.h"

#if defined( INCLUDE_SCALEFORM )
#include "scaleformui/scaleformui.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

extern IVEngineClient *engineClient;
extern HWND *pmainwindow;
extern bool g_bTextMode;
static int g_syncReportLevel = -1;

static void VGui_PlaySound(const char *pFileName);

void VGui_ActivateMouse();

extern CreateInterfaceFn g_AppSystemFactory;

// functions to reference GameUI and GameConsole functions, from GameUI.dll
IGameUI *staticGameUIFuncs = NULL;
IGameUI* GetGameUI( void )
{
	return staticGameUIFuncs;
}

IGameConsole *staticGameConsole = NULL;

// cache some of the state we pass through to matsystemsurface, for visibility
bool s_bWindowsInputEnabled = true;

ConVar r_drawvgui( "r_drawvgui", "1", FCVAR_CHEAT, "Enable the rendering of vgui panels" );
ConVar gameui_xbox( "gameui_xbox", "0", 0 );

// Tracks whether console window is open or not - true as soon as we receive the request to open it, until after it has shutdown
ConVar cv_console_window_open( "console_window_open", NULL, FCVAR_HIDDEN, "Is the console window active" );
ConVar cv_ignore_ui_activate_key( "ignore_ui_activate_key", NULL, FCVAR_HIDDEN, "When set will ignore UI activation key" );
ConVar cv_vguipanel_active( "vgui_panel_active", NULL, FCVAR_HIDDEN, "Is a vgui panel currently active" );
ConVar cv_server_browser_dialog_open( "server_browser_dialog_open", NULL, FCVAR_HIDDEN, "Is the server browser window active" );

void Con_CreateConsolePanel( Panel *parent );
void CL_CreateEntityReportPanel( Panel *parent );
void ClearIOStates( void );

#ifdef IHV_DEMO
// Enabled for IHV demos
void CreateWatermarkPanel( Panel *parent );
#endif

// turn this on if you're tuning progress bars
// #define ENABLE_LOADING_PROGRESS_PROFILING

#define PT( x ) #x, x

static tokenset_t< LevelLoadingProgress_e > g_ProgressTokens[]=
{
	{ PT( PROGRESS_DEFAULT ) },
	{ PT( PROGRESS_NONE ) },
	{ PT( PROGRESS_CHANGELEVEL ) },
	{ PT( PROGRESS_SPAWNSERVER ) },
	{ PT( PROGRESS_LOADWORLDMODEL ) },
	{ PT( PROGRESS_CRCMAP ) },
	{ PT( PROGRESS_CRCCLIENTDLL ) },
	{ PT( PROGRESS_CREATENETWORKSTRINGTABLES ) },
	{ PT( PROGRESS_PRECACHEWORLD ) },
	{ PT( PROGRESS_CLEARWORLD ) },
	{ PT( PROGRESS_LEVELINIT ) },
	{ PT( PROGRESS_PRECACHE ) },
	{ PT( PROGRESS_ACTIVATESERVER ) },
	{ PT( PROGRESS_BEGINCONNECT ) },
	{ PT( PROGRESS_SIGNONCHALLENGE ) },
	{ PT( PROGRESS_SIGNONCONNECT ) },
	{ PT( PROGRESS_SIGNONCONNECTED ) },
	{ PT( PROGRESS_PROCESSSERVERINFO ) },
	{ PT( PROGRESS_PROCESSSTRINGTABLE ) },
	{ PT( PROGRESS_SIGNONNEW ) },
	{ PT( PROGRESS_SENDCLIENTINFO ) },
	{ PT( PROGRESS_SENDSIGNONDATA ) },
	{ PT( PROGRESS_SIGNONSPAWN ) },
	{ PT( PROGRESS_CREATEENTITIES ) },
	{ PT( PROGRESS_FULLYCONNECTED ) },
	{ PT( PROGRESS_PRECACHELIGHTING ) },
	{ PT( PROGRESS_READYTOPLAY ) },
	{ PT( PROGRESS_HIGHESTITEM ) },
	{ NULL, PROGRESS_INVALID }
};
//-----------------------------------------------------------------------------
// Purpose: Console command to hide the gameUI, most commonly called from gameUI.dll
//-----------------------------------------------------------------------------
CON_COMMAND( gameui_hide, "Hides the game UI" )
{
	EngineVGui()->HideGameUI();
}

//-----------------------------------------------------------------------------
// Purpose: Console command to activate the gameUI, most commonly called from gameUI.dll
//-----------------------------------------------------------------------------
CON_COMMAND( gameui_activate, "Shows the game UI" )
{
	EngineVGui()->ActivateGameUI();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( gameui_preventescape, "Escape key doesn't hide game UI" )
{
	EngineVGui()->SetNotAllowedToHideGameUI( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( gameui_allowescapetoshow, "Escape key allowed to show game UI" )
{
	EngineVGui()->SetNotAllowedToShowGameUI( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( gameui_preventescapetoshow, "Escape key doesn't show game UI" )
{
	EngineVGui()->SetNotAllowedToShowGameUI( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( gameui_allowescape, "Escape key allowed to hide game UI" )
{
	EngineVGui()->SetNotAllowedToHideGameUI( false );
}

//-----------------------------------------------------------------------------
// Purpose: Console command to enable progress bar for next load
//-----------------------------------------------------------------------------
void BaseUI_ProgressEnabled_f()
{
	EngineVGui()->EnabledProgressBarForNextLoad();
}
static ConCommand progress_enable("progress_enable", &BaseUI_ProgressEnabled_f );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEnginePanel : public EditablePanel
{
	typedef EditablePanel BaseClass;
public:
	CEnginePanel( Panel *pParent, const char *pName ) : BaseClass( pParent, pName )
	{
		//m_bCanFocus = true;
		SetMouseInputEnabled( true );
		SetKeyBoardInputEnabled( true );
	}

	CEnginePanel( VPANEL parent, const char *pName ) : BaseClass( NULL, pName )
	{
		SetParent( parent );

		//m_bCanFocus = true;
		SetMouseInputEnabled( true );
		SetKeyBoardInputEnabled( true );
	}

	void EnableMouseFocus( bool state )
	{
		//m_bCanFocus = state;
		SetMouseInputEnabled( state );
		SetKeyBoardInputEnabled( state );
	}
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CStaticPanel : public Panel
{
	typedef Panel BaseClass;

public:
	CStaticPanel( Panel *pParent, const char *pName ) : Panel( pParent, pName )
	{
		SetCursor( dc_none );
		SetKeyBoardInputEnabled( false );
		SetMouseInputEnabled( false );
	}
};

vgui::VPanelHandle g_DrawTreeSelectedPanel;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CFocusOverlayPanel : public Panel
{
	typedef Panel BaseClass;

public:
	CFocusOverlayPanel( Panel *pParent, const char *pName );

	virtual void PostChildPaint( void );
	static void GetColorForSlot( int slot, int& r, int& g, int& b )
	{
		r = (int)( 124.0 + slot * 47.3 ) & 255;
		g = (int)( 63.78 - slot * 71.4 ) & 255;
		b = (int)( 188.42 + slot * 13.57 ) & 255;
	}

	bool DrawTitleSafeOverlay( void );
	bool DrawFocusPanelList( void );
	bool DrawKeyFocusPanel( void );
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTransitionEffectPanel : public EditablePanel
{
	typedef EditablePanel BaseClass;

public:
	CTransitionEffectPanel( Panel *pParent, const char *pName ) : BaseClass( pParent, pName )
	{
		SetBounds( 0, 0, videomode->GetModeWidth(), videomode->GetModeHeight() );
		SetPaintBorderEnabled( false );
		SetPaintBackgroundEnabled( false );
		SetPaintEnabled( true );
		SetVisible( true );
		SetCursor( dc_none );
		SetMouseInputEnabled( false );
		SetKeyBoardInputEnabled( true );
	}

	bool IsEffectEnabled()
	{
		return staticGameUIFuncs->IsTransitionEffectEnabled();
	}
};


//-----------------------------------------------------------------------------
//
// Purpose: Centerpoint for handling all user interface in the engine
//
//-----------------------------------------------------------------------------
class CEngineVGui : public IEngineVGuiInternal
{
public:
	CEngineVGui();
	~CEngineVGui();

	// Methods of IEngineVGui
	virtual VPANEL GetPanel( VGuiPanel_t type );

	// Methods of IEngineVGuiInternal
	virtual void Init();
	virtual void Connect();
	virtual void Shutdown();
	virtual bool SetVGUIDirectories();
	virtual bool IsInitialized() const;
	virtual bool Key_Event( const InputEvent_t &event );
	virtual void UpdateButtonState( const InputEvent_t &event );
	virtual void BackwardCompatibility_Paint();
	virtual void Paint( PaintMode_t mode );
	virtual void PostInit();

	CreateInterfaceFn GetGameUIFactory()
	{
		return m_GameUIFactory;
	}
	
	// handlers for game UI (main menu)
	virtual void ActivateGameUI();
	virtual bool HideGameUI();
	virtual bool IsGameUIVisible();

	// console
	virtual void ShowConsole();
	virtual void HideConsole();
	virtual bool IsConsoleVisible();
	virtual void ClearConsole();

	// level loading
	virtual void OnLevelLoadingStarted( char const *levelName, bool bLocalServer );
	virtual void OnLevelLoadingFinished();
	virtual void NotifyOfServerConnect(const char *game, int IP, int connectionPort, int queryPort);
	virtual void NotifyOfServerDisconnect();
	virtual void UpdateProgressBar(LevelLoadingProgress_e progress, bool showDialog = true );
	virtual void UpdateCustomProgressBar( float progress, const wchar_t *desc );
	virtual void StartCustomProgress();
	virtual void FinishCustomProgress();
	virtual void UpdateSecondaryProgressBarWithFile( float progress, const char *pDesc, int nBytesTotal );
	virtual void UpdateSecondaryProgressBar( float progress, const wchar_t *desc );
	virtual void StartLoadingScreenForCommand( const char* command );
	virtual void StartLoadingScreenForKeyValues( KeyValues* keyValues );

	virtual void EnabledProgressBarForNextLoad()
	{
		m_bShowProgressDialog = true;
	}

	// Should pause?
	virtual bool ShouldPause();
	virtual void ShowErrorMessage();

	virtual void SetNotAllowedToHideGameUI( bool bNotAllowedToHide )
	{
		m_bNotAllowedToHideGameUI = bNotAllowedToHide;
	}

	virtual void SetNotAllowedToShowGameUI( bool bNotAllowedToShow )
	{
		m_bNotAllowedToShowGameUI = bNotAllowedToShow;
	}

	virtual void HideLoadingPlaque( void )
	{
		if ( scr_drawloading )
		{
			OnLevelLoadingFinished();
			S_OnLoadScreen( false );
		}

		S_PreventSound(false);//it is now safe to use audio again.

		scr_disabled_for_loading = false;
		scr_drawloading = false;
	}

	void SetGameDLLPanelsVisible( bool show )
	{
		if ( !staticGameDLLPanel )
		{
			return;
		}

		staticGameDLLPanel->SetVisible( show );
	}

	// Allows the level loading progress to show map-specific info
	virtual void SetProgressLevelName( const char *levelName );

	virtual void OnToolModeChanged( bool bGameMode );
	virtual InputContextHandle_t GetGameUIInputContext() { return m_hGameUIInputContext; }

	virtual void NeedConnectionProblemWaitScreen();
	virtual void ShowPasswordUI( char const *pchCurrentPW );

	void SetProgressBias( float bias );
	void UpdateProgressBar( float progress, const char *pszDesc = NULL, bool showDialog = true );

	virtual bool IsPlayingFullScreenVideo();

private:
	Panel *GetRootPanel( VGuiPanel_t type );
	void SetEngineVisible( bool state );
	void DrawMouseFocus( void );
	void DrawKeyFocus( void );
	void CreateVProfPanels( Panel *pParent );
	void DestroyVProfPanels( );
	void HideVProfPanels();

	virtual void Simulate();

	// debug overlays
	bool IsDebugSystemVisible();
	void HideDebugSystem();

	bool IsShiftKeyDown();
	bool IsAltKeyDown();
	bool IsCtrlKeyDown();

	CON_COMMAND_MEMBER_F( CEngineVGui, "debugsystemui", ToggleDebugSystemUI, "Show/hide the debug system UI.", FCVAR_CHEAT );

	void PreparePanel( Panel *panel, int nZPos, bool bVisible = true );

private:
	enum { MAX_NUM_FACTORIES = 5 };
	CreateInterfaceFn m_FactoryList[MAX_NUM_FACTORIES];
	int m_iNumFactories;

	CSysModule *m_hStaticGameUIModule;
	CreateInterfaceFn m_GameUIFactory;

	// top level VGUI2 panel
	CStaticPanel *staticPanel;

	// base level panels for other subsystems, rooted on staticPanel
	CEnginePanel *staticClientDLLPanel;
	CEnginePanel *staticClientDLLToolsPanel;
	CEnginePanel *staticGameUIPanel;
	CEnginePanel *staticGameUIBackgroundPanel;
	CEnginePanel *staticGameDLLPanel;

	// Want engine tools to be on top of other engine panels
	CEnginePanel *staticEngineToolsPanel;
	CDebugSystemPanel *staticDebugSystemPanel;
	CEnginePanel *staticSteamOverlayPanel;
	CFocusOverlayPanel *staticFocusOverlayPanel;
	CTransitionEffectPanel *staticTransitionPanel;

#ifdef VPROF_ENABLED
	CVProfPanel *m_pVProfPanel;
	CBudgetPanelEngine *m_pBudgetPanel;
	CTextureBudgetPanel *m_pTextureBudgetPanel;
#endif

	// progress bar
	bool m_bShowProgressDialog;
	LevelLoadingProgress_e m_eLastProgressPoint;

	// progress bar debugging
	int m_nLastProgressPointRepeatCount;
	double m_flLoadingStartTime;
	struct LoadingProgressEntry_t
	{
		double flTime;
		LevelLoadingProgress_e eProgress;
	};
	CUtlVector<LoadingProgressEntry_t> m_LoadingProgress;

	bool					m_bSaveProgress : 1;
	bool					m_bNoShaderAPI : 1;
	// game ui hiding control
	bool					m_bNotAllowedToHideGameUI : 1;
	bool					m_bNotAllowedToShowGameUI : 1;

	IInputInternal *m_pInputInternal;

	// used to start the progress from an arbitrary position
	float					m_ProgressBias;

	InputContextHandle_t m_hGameUIInputContext;

	IMaterial *m_pConstantColorMaterial;
};


//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
static CEngineVGui g_EngineVGuiImp;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CEngineVGui, IEngineVGui, VENGINE_VGUI_VERSION, g_EngineVGuiImp );

IEngineVGuiInternal *EngineVGui()
{
	return &g_EngineVGuiImp;
}

//-----------------------------------------------------------------------------
// The loader progress is updated by the queued loader. It uses an initial
// reserved portion of the bar.
//-----------------------------------------------------------------------------
#define PROGRESS_RESERVE 0.50f
class CLoaderProgress : public ILoaderProgress
{
public:
	CLoaderProgress()
	{ 
		// initialize to disabled state
		m_SnappedProgress = -1;
		m_flLastProgress = 0.0f;
	}

	void BeginProgress()
	{
		g_EngineVGuiImp.SetProgressBias( 0 );
		m_SnappedProgress = 0;
	}

	void UpdateProgress( float progress, bool bForce )
	{
		if ( !bForce )
		{
			m_flLastProgress = progress;
		}
		if ( m_SnappedProgress == - 1 && !bForce )
		{
			// not enabled
			return;
		}	

		int snappedProgress = progress * 15;

		// Need excessive updates on the console to keep the XBox slider inny bar/XMB active
		if ( !IsGameConsole() && ( snappedProgress <= m_SnappedProgress ) )
		{
			// prevent excessive updates
			return;
		}
		m_SnappedProgress = snappedProgress;

		// up to reserved
		g_EngineVGuiImp.UpdateProgressBar( bForce ? 1.0f : ( PROGRESS_RESERVE * progress ) );
	}

	void EndProgress()
	{
		// the normal legacy bar now picks up after reserved region
		g_EngineVGuiImp.SetProgressBias( PROGRESS_RESERVE );
		m_SnappedProgress = -1;
	}

	void PauseNonInteractiveProgress( bool bPause )
	{
		PauseLoadingUpdates( bPause );
	}

private:
	int m_SnappedProgress;
	float m_flLastProgress;
};
static CLoaderProgress s_LoaderProgress;

	
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CEngineVGui::CEngineVGui()
{
	staticPanel = NULL;
	staticClientDLLToolsPanel = NULL;
	staticClientDLLPanel = NULL;
	staticGameDLLPanel = NULL;
	staticGameUIPanel = NULL;
	staticGameUIBackgroundPanel = NULL;
	staticEngineToolsPanel = NULL;
	staticDebugSystemPanel = NULL;
	staticSteamOverlayPanel = NULL;
	staticFocusOverlayPanel = NULL;
	staticTransitionPanel = NULL;

	m_hGameUIInputContext = INPUT_CONTEXT_HANDLE_INVALID;
	m_hStaticGameUIModule = NULL;
	m_GameUIFactory = NULL;
	
#ifdef VPROF_ENABLED
	m_pVProfPanel = NULL;
#endif

	m_bShowProgressDialog = false;
	m_bSaveProgress = false;
	m_bNoShaderAPI = false;
	m_bNotAllowedToHideGameUI = false;
	m_bNotAllowedToShowGameUI = false;
	m_pInputInternal = NULL;
	m_ProgressBias = 0;
	m_pConstantColorMaterial = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CEngineVGui::~CEngineVGui()
{
}


//-----------------------------------------------------------------------------
// add all the base search paths used by VGUI (platform, skins directory, language dirs)
//-----------------------------------------------------------------------------
bool CEngineVGui::SetVGUIDirectories()
{
	// add vgui skins directory last
#if defined(_WIN32)
	if ( IsPC() )
	{
		char temp[ 512 ];
		char skin[128];
		skin[0] = 0;
		Sys_GetRegKeyValue("Software\\Valve\\Steam", "Skin", skin, sizeof(skin), "");
		if (strlen(skin) > 0)
		{
			sprintf( temp, "%s/platform/skins/%s", GetBaseDirectory(), skin );
			g_pFileSystem->AddSearchPath( temp, "SKIN" );
		}
	}
#endif

	return true;
}

void CEngineVGui::PreparePanel( Panel *panel, int nZPos, bool bVisible /*= true*/ )
{
	panel->SetBounds( 0, 0, videomode->GetModeWidth(), videomode->GetModeHeight() );
	panel->SetPaintBorderEnabled(false);
	panel->SetPaintBackgroundEnabled(false);
	panel->SetPaintEnabled(false);
	panel->SetVisible(true);
	panel->SetCursor( dc_none );
	panel->SetVisible( bVisible );
	panel->SetZPos( nZPos );
}

//-----------------------------------------------------------------------------
// Setup the base vgui panels
//-----------------------------------------------------------------------------
void CEngineVGui::Init()
{
	const char *szDllName = "";
	
	if ( CommandLine()->FindParm( "-gameuidll" ) )
	{
		COM_TimestampedLog( "Loading gameui.dll" );

		// load the GameUI dll
		szDllName = "gameui";
		m_hStaticGameUIModule = g_pFileSystem->LoadModule(szDllName, "GAMEBIN", true); // LoadModule() does a GetLocalCopy() call
		m_GameUIFactory = Sys_GetFactory(m_hStaticGameUIModule);
		if ( !m_GameUIFactory )
		{
			Error( "Could not load: %s\n", szDllName );
		}
	}
	else
	{
		// Get the gameui interfaces from client.dll
		extern CreateInterfaceFn g_ClientFactory;
		m_GameUIFactory = g_ClientFactory;
		szDllName = "client";
	}
	
	// get the initialization func
	staticGameUIFuncs = (IGameUI *)m_GameUIFactory(GAMEUI_INTERFACE_VERSION, NULL);
	if (!staticGameUIFuncs )
	{
		Error( "Could not get IGameUI interface %s from %s\n", GAMEUI_INTERFACE_VERSION, szDllName );
	}

	if ( IsPC() )
	{
		staticGameConsole = (IGameConsole *)m_GameUIFactory(GAMECONSOLE_INTERFACE_VERSION, NULL);
		if ( !staticGameConsole )
		{
			Sys_Error( "Could not get IGameConsole interface %s from %s\n", GAMECONSOLE_INTERFACE_VERSION, szDllName );
		}
	}

	// Create UI Input contexts
	// NOTE: The GameUI context may or may not be used by the client
	// so we'll start it out disabled
	m_hGameUIInputContext = g_pInputStackSystem->PushInputContext();
	g_pInputStackSystem->EnableInputContext( m_hGameUIInputContext, false );
	InputContextHandle_t hVGuiInputContext = g_pInputStackSystem->PushInputContext();
	g_pMatSystemSurface->SetInputContext( hVGuiInputContext );

	VGui_InitMatSysInterfacesList( "BaseUI", &g_AppSystemFactory, 1 );
	
#ifdef OSX
	if ( Steam3Client().SteamApps() )
	{
		// just follow the language steam wants you to be
		const char *lang = Steam3Client().SteamApps()->GetCurrentGameLanguage();
		if ( lang && Q_strlen(lang) )
			vgui::system()->SetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Language", lang );
	}
#endif
	
	COM_TimestampedLog( "AttachToWindow" );

	// Need to be able to play sounds through vgui
	g_pMatSystemSurface->InstallPlaySoundFunc( VGui_PlaySound );

	COM_TimestampedLog( "Load Scheme File" );

	// load scheme
	const char *pStr = "Resource/SourceScheme.res";
	if ( !scheme()->LoadSchemeFromFile( pStr, "Tracker" ))
	{
		Sys_Error( "Error loading file %s\n", pStr );
		return;
	}

	if ( IsGameConsole() )
	{
		CCommand ccommand;
		if ( CL_ShouldLoadBackgroundLevel( ccommand ) )
		{
			// Must be before the game ui base panel starts up
			// This is a hint to avoid the menu pop due to the impending background map
			// that the game ui is not aware of until 1 frame later.
			staticGameUIFuncs->SetProgressOnStart();
		}
	}

	COM_TimestampedLog( "ivgui()->Start()" );

	// Start the App running
	ivgui()->Start();
	ivgui()->SetSleep(false);

	bool bTools = CommandLine()->CheckParm( "-tools" ) != NULL;

	// setup base panel for the whole VGUI System
	// The root panel for everything ( NULL parent makes it a child of the embedded panel )

	// Ideal hierarchy:

	// Root -- staticPanel
#if defined( TOOLFRAMEWORK_VGUI_REFACTOR )

	//		staticGameUIBackgroundPanel ( loading image only ) (zpos 0)
	//      staticClientDLLPanel ( zpos == 25 )
	//		staticClientDLLPanelFullscreen ( zpos == 26 )
	//		staticGameUIPanel ( GameUI stuff, zpos 40 )
	//		staticClientDLLToolsPanel ( zpos == 28 )
	//		staticGameDLLPanel ( zpos == 50 )  [ Tool Framework Root ]
	//		staticEngineToolsPanel ( zpos == 75 )
	//		staticDebugSystemPanel ( Engine debug stuff ) zpos == 125 )
	//		staticSteamOverlayPanel (STEAM OVERLAY ON PS3) zpos = 140
	//		staticFocusOverlayPanel (zpos == 150)
#endif

	COM_TimestampedLog( "Building Panels (staticPanel)" );
	staticPanel = new CStaticPanel( NULL, "staticPanel" );	
	staticPanel->SetParent( surface()->GetEmbeddedPanel() );
	PreparePanel( staticPanel, 0 );

	COM_TimestampedLog( "Building Panels (staticGameUIBackgroundPanel)" );
	staticGameUIBackgroundPanel = new CEnginePanel( staticPanel, "GameUI Background Panel" );
	PreparePanel( staticGameUIBackgroundPanel, 0 );

	COM_TimestampedLog( "Building Panels (staticClientDLLPanel)" );
	staticClientDLLPanel = new CEnginePanel( staticPanel, "staticClientDLLPanel" );
	PreparePanel( staticClientDLLPanel, 25, false );
	staticClientDLLPanel->SetKeyBoardInputEnabled( false );	// popups in the client DLL can enable this.

	// This panel has it's own animation controller, which makes it 1.1Mb to instance
	//  which is too much on the console which doesn't support plugins anyway.
	if ( IsPC() )
	{
		COM_TimestampedLog( "Building Panels (CreateAskConnectPanel)" );
		CreateAskConnectPanel( staticPanel->GetVPanel() );
	}

	COM_TimestampedLog( "Building Panels (staticClientDLLToolsPanel)" );
	staticClientDLLToolsPanel = new CEnginePanel( staticPanel, "staticClientDLLToolsPanel" );
	PreparePanel( staticClientDLLToolsPanel, 28 );
	// popups in the client DLL can enable this.
	staticClientDLLToolsPanel->SetKeyBoardInputEnabled( false );	

	COM_TimestampedLog( "Building Panels (staticGameUIPanel)" );
	staticGameUIPanel = new CEnginePanel( staticPanel, "GameUI Panel" );
#if defined( TOOLFRAMEWORK_VGUI_REFACTOR )
	PreparePanel( staticGameUIPanel, 40 );
#else
	PreparePanel( staticGameUIPanel, 100 );
#endif

	COM_TimestampedLog( "Building Panels (staticGameDLLPanel)" );
	staticGameDLLPanel = new CEnginePanel( staticPanel, "staticGameDLLPanel" );
#if defined( TOOLFRAMEWORK_VGUI_REFACTOR )
	PreparePanel( staticGameDLLPanel, 50 );
#else
	PreparePanel( staticGameDLLPanel, 135 );
#endif
	staticGameDLLPanel->SetKeyBoardInputEnabled( false );	// popups in the game DLL can enable this.

	COM_TimestampedLog( "Building Panels (Engine Tools)" );

	staticEngineToolsPanel = new CEnginePanel( bTools ? staticGameDLLPanel->GetVPanel() : staticPanel->GetVPanel(), "Engine Tools" );
#if defined( TOOLFRAMEWORK_VGUI_REFACTOR )
	PreparePanel( staticEngineToolsPanel, 75 );
#else
	PreparePanel( staticEngineToolsPanel, 100 );
#endif
	staticEngineToolsPanel->SetKeyBoardInputEnabled( false );	// popups in the game DLL can enable this.
	staticEngineToolsPanel->SetMouseInputEnabled( false );	// popups in the game DLL can enable this.

	if ( IsPC() )
	{
		COM_TimestampedLog( "Building Panels (staticDebugSystemPanel)" );

		staticDebugSystemPanel = new CDebugSystemPanel( staticPanel, "Engine Debug System" );
		staticDebugSystemPanel->SetZPos( 125 );

		// Install demo playback/editing UI
		CDemoUIPanel::InstallDemoUI( staticEngineToolsPanel );
		//CDemoUIPanel2::Install( staticClientDLLPanel, staticEngineToolsPanel, true );

		// Install fog control panel UI
		CFogUIPanel::InstallFogUI( staticEngineToolsPanel );

		// Install texture view panel
		TxViewPanel::Install( staticEngineToolsPanel );

/*
		COM_TimestampedLog( "Install bug reporter" );

		// Create and initialize bug reporting system
		bugreporter->InstallBugReportingUI( staticGameUIPanel, IEngineBugReporter::BR_AUTOSELECT );
		bugreporter->Init();
*/

		COM_TimestampedLog( "Install perf tools" );

		// Create a performance toolkit system
		perftools->InstallPerformanceToolsUI( staticEngineToolsPanel );
		perftools->Init();

		// Create a color correction UI
		colorcorrectiontools->InstallColorCorrectionUI( staticEngineToolsPanel );
		colorcorrectiontools->Init();
	}

	COM_TimestampedLog( "Building Panels (staticTransitionPanel)" );

	staticTransitionPanel = new CTransitionEffectPanel( staticPanel, "TransitionEffect" );
	staticTransitionPanel->SetZPos( 135 );

	if ( IsPS3() )
	{
		COM_TimestampedLog( "Building Panels (staticSteamOverlayPanel)" );

		staticSteamOverlayPanel = new CEnginePanel( staticPanel, "Steam Overlay" );
		staticSteamOverlayPanel->SetZPos( 140 );
		staticSteamOverlayPanel->SetKeyBoardInputEnabled( false );
		staticSteamOverlayPanel->SetMouseInputEnabled( false );
	}

	COM_TimestampedLog( "Building Panels (FocusOverlayPanel)" );

	// Make sure this is on top of everything
	staticFocusOverlayPanel = new CFocusOverlayPanel( staticPanel, "FocusOverlayPanel" );
	staticFocusOverlayPanel->SetBounds( 0, 0, videomode->GetModeWidth(), videomode->GetModeHeight() );
	staticFocusOverlayPanel->SetZPos( 150 );
	staticFocusOverlayPanel->MoveToFront();

	COM_TimestampedLog( "Building Panels (console, entity report, drawtree, texturelist, vprof)" );

	// Create engine vgui panels
	if ( IsPC() )
	{
#ifdef IHV_DEMO
// 		if ( GetSteamUniverse() == k_EUniversePublic ) // IHV_DEMO - we must remove this before we publicly release
// 		{
// 			CreateWatermarkPanel( staticEngineToolsPanel );
// 		}
#endif

		Con_CreateConsolePanel( staticEngineToolsPanel );
		CL_CreateEntityReportPanel( staticEngineToolsPanel );
		VGui_CreateDrawTreePanel( staticEngineToolsPanel );
		CL_CreateTextureListPanel( staticEngineToolsPanel );
		CreateVProfPanels( staticEngineToolsPanel );
	}
#ifndef _CERT
	else if ( IsGameConsole() )
	{
		Con_CreateConsolePanel( staticEngineToolsPanel );
		CL_CreateEntityReportPanel( staticEngineToolsPanel );

		if ( IsX360() )
		{
			CreateVProfPanels( staticGameDLLPanel );
		}
	}
#endif // !_CERT





	



	staticEngineToolsPanel->LoadControlSettings( "scripts/EngineVGuiLayout.res" );
	// Loading the .res will show some panels which really should stay hidden
	HideVProfPanels();

	COM_TimestampedLog( "materials->CacheUsedMaterials()" );

	// This material is used by CPotteryWheelPanel::Paint() to copy stencil to the render target's alpha. Not sure of the best place to put it, but this needs to be done sometime before the used materials are precached.
	m_pConstantColorMaterial = materials->FindMaterial( "dev/constant_color", TEXTURE_GROUP_OTHER, true );
	if ( m_pConstantColorMaterial )
	{
		m_pConstantColorMaterial->IncrementReferenceCount();
	}
		
	// Make sure that these materials are in the materials cache
	materials->CacheUsedMaterials();

	COM_TimestampedLog( "g_pVGuiLocalize->AddFile" );


	// load the base localization file
	g_pVGuiLocalize->AddFile( "Resource/valve_%language%.txt" );

	char szFileName[MAX_PATH];

	// We also want to load the localization file for the base game.  Nomrally, all these values would already be in valve_language.txt, but
	// with CSGO we decided to move them into csgo_language (which is NOT a mod).
	Q_snprintf( szFileName, sizeof( szFileName ) - 1, "resource/%s_%%language%%.txt", GetCurrentGame() );
	szFileName[ sizeof( szFileName ) - 1 ] = '\0';
	g_pVGuiLocalize->AddFile( szFileName );


	// don't need to load the "valve" localization file twice
	// Each mod can have its own language.txt in addition to the valve_%%langauge%%.txt file under defaultgamedir.
	// load mod-specific localization file for kb_act.lst, user.scr, settings.scr, etc.
	Q_snprintf( szFileName, sizeof( szFileName ) - 1, "resource/%s_%%language%%.txt", GetCurrentMod() );
	szFileName[ sizeof( szFileName ) - 1 ] = '\0';
	g_pVGuiLocalize->AddFile( szFileName );

	// Load a low-violence-specific string file to override strings in the mod string file
	if ( g_bLowViolence )
	{
		Q_snprintf( szFileName, sizeof( szFileName ) - 1, "resource/%s_%%language%%_lv.txt", GetCurrentMod() );
		szFileName[ sizeof( szFileName ) - 1 ] = '\0';
		g_pVGuiLocalize->AddFile( szFileName );
	}

	COM_TimestampedLog( "staticGameUIFuncs->Initialize" );

	staticGameUIFuncs->Initialize( g_GameSystemFactory );

	COM_TimestampedLog( "staticGameUIFuncs->Start" );
	staticGameUIFuncs->Start();

	// setup console
	if ( staticGameConsole )
	{
		staticGameConsole->Initialize();
#if defined( TOOLFRAMEWORK_VGUI_REFACTOR )
		staticGameConsole->SetParent(staticEngineToolsPanel->GetVPanel());
#elif !defined (CSTRIKE15)
		staticGameConsole->SetParent(staticGameUIPanel->GetVPanel());
#endif
	}

	if ( IsGameConsole() )
	{
		// provide an interface for loader to send progress notifications
		g_pQueuedLoader->InstallProgress( &s_LoaderProgress ); 
	}

	// show the game UI
	COM_TimestampedLog( "ActivateGameUI()" );
	ActivateGameUI();

	if ( staticGameConsole && 
		!CommandLine()->CheckParm( "-forcestartupmenu" ) && 
		!CommandLine()->CheckParm( "-hideconsole" ) &&
		( CommandLine()->FindParm( "-toconsole" ) || CommandLine()->FindParm( "-console" ) || CommandLine()->FindParm( "-rpt" ) || CommandLine()->FindParm( "-allowdebug" ) ) )
	{
		// activate the console
		staticGameConsole->Activate();
	}

	m_bNoShaderAPI = CommandLine()->FindParm( "-noshaderapi" ) ? true : false;
}

void CEngineVGui::PostInit()
{
	staticGameUIFuncs->PostInit();
#if defined( _GAMECONSOLE )
	g_pMatSystemSurface->ClearTemporaryFontCache();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: connects interfaces in gameui
//-----------------------------------------------------------------------------
void CEngineVGui::Connect()
{
	m_pInputInternal = (IInputInternal *)g_GameSystemFactory( VGUI_INPUTINTERNAL_INTERFACE_VERSION,  NULL );
	staticGameUIFuncs->Connect( g_GameSystemFactory );

	#if OSX
//		g_pLauncherMgr = (ILauncherMgr *)g_GameSystemFactory(  COCOAMGR_INTERFACE_VERSION, NULL );	
	#endif
	#if LINUX
//		g_pLauncherMgr = (ILauncherMgr *)g_GameSystemFactory(  LINUXMGR_INTERFACE_VERSION, NULL );	
	#endif
	#if defined( _WIN32 ) && defined( DX_TO_GL_ABSTRACTION )
//		g_pLauncherMgr = (ILauncherMgr *)g_GameSystemFactory(  WINMGR_INTERFACE_VERSION, NULL );	
	#endif
}

//-----------------------------------------------------------------------------
// Create/destroy the vprof panels
//-----------------------------------------------------------------------------
void CEngineVGui::CreateVProfPanels( Panel *pParent )
{
#ifdef _CERT
	if ( IsGameConsole() )
		return;
#endif

#ifdef VPROF_ENABLED
	m_pVProfPanel = new CVProfPanel( pParent, "VProfPanel" );
	m_pBudgetPanel = new CBudgetPanelEngine( pParent, "BudgetPanel" );
	CreateVProfGraphPanel( pParent );
	m_pTextureBudgetPanel = new CTextureBudgetPanel( pParent, "TextureBudgetPanel" );
#endif
}

void CEngineVGui::HideVProfPanels()
{
#ifdef _CERT
	if ( IsGameConsole() )
		return;
#endif

#ifdef VPROF_ENABLED
	m_pVProfPanel->SetVisible( false );
	m_pBudgetPanel->SetVisible( false );
	HideVProfGraphPanel();
	m_pTextureBudgetPanel->SetVisible( false );
#endif
}

void CEngineVGui::DestroyVProfPanels( )
{
#ifdef _CERT
	if ( IsGameConsole() )
		return;
#endif

#ifdef VPROF_ENABLED
	if ( m_pVProfPanel )
	{
		delete m_pVProfPanel;
		m_pVProfPanel = NULL;
	}
	if ( m_pBudgetPanel )
	{
		delete m_pBudgetPanel;
		m_pBudgetPanel = NULL;
	}
	DestroyVProfGraphPanel();

	if ( m_pTextureBudgetPanel )
	{
		delete m_pTextureBudgetPanel;
		m_pTextureBudgetPanel = NULL;
	}
#endif
}


//-----------------------------------------------------------------------------
// Are we initialized?
//-----------------------------------------------------------------------------
bool CEngineVGui::IsInitialized() const
{
	return staticPanel != NULL;
}

extern bool g_bUsingLegacyAppSystems;
//-----------------------------------------------------------------------------
// Purpose: Called to Shutdown the game UI system
//-----------------------------------------------------------------------------
void CEngineVGui::Shutdown()
{
	if ( m_pConstantColorMaterial )
	{
		m_pConstantColorMaterial->DecrementReferenceCount();
		m_pConstantColorMaterial = NULL;
	}

	if ( IsPC() && CL_IsHL2Demo() ) // if they are playing the demo then open the storefront on shutdown
	{
		system()->ShellExecute("open", "steam://store_demo/220");
	}

	if ( IsPC() && CL_IsPortalDemo() ) // if they are playing the demo then open the storefront on shutdown
	{
		vgui::system()->ShellExecute("open", "steam://store_demo/400");
	}

	DestroyVProfPanels();
	bugreporter->Shutdown();
	colorcorrectiontools->Shutdown();
	perftools->Shutdown();

	demoaction->Shutdown();

	if ( g_PluginManager )
	{
		g_PluginManager->Shutdown();
	}

	// HACK HACK: There was a bug in the old versions of the viewport which would crash in the case where the client .dll hadn't been fully unloaded, so
	//  we'll leak this panel here instead!!!
	if ( g_bUsingLegacyAppSystems )
	{
		staticClientDLLPanel->SetParent( (VPANEL)0 );
	}

	if ( staticGameConsole )
	{
		staticGameConsole->Shutdown();
		staticGameConsole = NULL;
	}

	staticGameUIPanel = NULL;
	staticClientDLLToolsPanel = NULL;
	staticClientDLLPanel	= NULL;
	staticEngineToolsPanel = NULL;
	staticDebugSystemPanel = NULL;
	staticSteamOverlayPanel = NULL;
	staticFocusOverlayPanel = NULL;
	staticGameDLLPanel = NULL;

	// This will delete the engine subpanel since it's a child
	delete staticPanel;
	staticPanel = NULL;

	// Give panels a chance to settle so things
	//  Marked for deletion will actually get deleted
	{
		FORCE_DEFAULT_SPLITSCREEN_PLAYER_GUARD;
		ivgui()->RunFrame();
	}

	// unload the gameUI
	staticGameUIFuncs->Shutdown();
	staticGameUIFuncs = NULL;

	// stop the App running
	ivgui()->Stop();
	
	// Disable the input contexts
	if ( m_hGameUIInputContext != INPUT_CONTEXT_HANDLE_INVALID )
	{
		g_pMatSystemSurface->SetInputContext( INPUT_CONTEXT_HANDLE_INVALID );
		g_pInputStackSystem->PopInputContext(); // VGui
		g_pInputStackSystem->PopInputContext(); // GameUI
		m_hGameUIInputContext = INPUT_CONTEXT_HANDLE_INVALID;
	}

	// unload the dll
	if ( m_hStaticGameUIModule )
	{
		Sys_UnloadModule(m_hStaticGameUIModule);
	}

	m_hStaticGameUIModule = NULL;
	m_GameUIFactory = NULL;
	m_pInputInternal = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Retrieve specified root panel
//-----------------------------------------------------------------------------
inline Panel *CEngineVGui::GetRootPanel( VGuiPanel_t type )
{
	if ( sv.IsDedicated() )
	{
		return NULL;
	}

	switch ( type )
	{
	default:
	case PANEL_ROOT:
		return staticPanel;
	case PANEL_CLIENTDLL:
		return staticClientDLLPanel;
	case PANEL_GAMEUIDLL:
		return staticGameUIPanel;
	case PANEL_TOOLS:
		return staticEngineToolsPanel;
	case PANEL_GAMEDLL:
		return staticGameDLLPanel;
	case PANEL_CLIENTDLL_TOOLS:
		return staticClientDLLToolsPanel;
	case PANEL_GAMEUIBACKGROUND:
		return staticGameUIBackgroundPanel;
	case PANEL_TRANSITIONEFFECT:
		return staticTransitionPanel;
	case PANEL_STEAMOVERLAY:
		return staticSteamOverlayPanel;
	}
}

VPANEL CEngineVGui::GetPanel( VGuiPanel_t type )
{
	return GetRootPanel( type )->GetVPanel();
}

//-----------------------------------------------------------------------------
// Purpose: Toggle engine panel active/inactive
//-----------------------------------------------------------------------------
void CEngineVGui::SetEngineVisible( bool state )
{
	if ( staticClientDLLPanel )
	{
		staticClientDLLPanel->SetVisible( state );
	}
}


//-----------------------------------------------------------------------------
// Should pause?
//-----------------------------------------------------------------------------
bool CEngineVGui::ShouldPause()
{
	if ( IsPC() )
	{
		return bugreporter->ShouldPause() || perftools->ShouldPause();
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Shows any GameUI related panels
//-----------------------------------------------------------------------------
void CEngineVGui::ActivateGameUI()
{
	if ( m_bNotAllowedToShowGameUI )
		return;

	if (!staticGameUIFuncs)
		return;

	// clear any keys that might be stuck down
	ClearIOStates();

	staticGameUIPanel->SetVisible(true);
	staticGameUIBackgroundPanel->SetVisible( true );
	staticGameUIPanel->MoveToFront();	

	staticClientDLLPanel->SetVisible(false);
	staticClientDLLPanel->SetMouseInputEnabled(false);

	surface()->SetCursor( dc_arrow );

	//staticGameDLLPanel->SetVisible( true );
	//staticGameDLLPanel->SetMouseInputEnabled( true );

	SetEngineVisible( false );

	staticGameUIFuncs->OnGameUIActivated();
	
//Reapplying this hack so that the game doesn't pause when the player opens up the menu.
//This existed initially but was removed with the Portal 2 integration.
#if defined( CSTRIKE15 )
	if ( sv.IsPlayingSoloAgainstBots() )
	{
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "pause\n" );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Hides an Game UI related features (not client UI stuff tho!)
//-----------------------------------------------------------------------------
bool CEngineVGui::HideGameUI()
{
	if ( m_bNotAllowedToHideGameUI )
		return false;

	if (!staticGameUIFuncs)
		return false;

	const char *levelName = engineClient->GetLevelName();
	bool bInNonBgLevel = levelName && levelName[0] && !engineClient->IsLevelMainMenuBackground();
	if ( bInNonBgLevel )
	{
		staticGameUIPanel->SetVisible(false);
		staticGameUIBackgroundPanel->SetVisible( false );
#if !defined( TOOLFRAMEWORK_VGUI_REFACTOR )
		staticGameUIPanel->SetPaintBackgroundEnabled(false);
#endif

		staticClientDLLPanel->SetVisible(true);
		staticClientDLLPanel->MoveToFront();
		staticClientDLLPanel->SetMouseInputEnabled(true);

		//staticGameDLLPanel->SetVisible( false );
		//staticGameDLLPanel->SetMouseInputEnabled(false);

		SetEngineVisible( true );

		staticGameUIFuncs->OnGameUIHidden();
	}

#if defined (CSTRIKE15)
	// Decouple the pause menu and console window: ensure we hide the console when the game UI is hidden (eg. when we hit ESC key)
	HideConsole();
#endif // CSTRIKE15

	// Tracker 18820:  Pulling up options/console was perma-pausing the background levels, now we
	//  unpause them when you hit the Esc key even though the UI remains...
	if ( levelName && 
		 levelName[0] && 
		 ( ( engineClient->GetMaxClients() <= 1 ) || sv.IsPlayingSoloAgainstBots() ) && 
		 engineClient->IsPaused() )
	{
		Cbuf_AddText(Cbuf_GetCurrentPlayer(), "unpause\n");
	}

	VGui_MoveDrawTreePanelToFront();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Hides the game console (but not the complete GameUI!)
//-----------------------------------------------------------------------------
void CEngineVGui::HideConsole()
{
	if ( IsGameConsole() )
		return;

	if ( staticGameConsole )
	{
		staticGameConsole->Hide();
	}
}

//-----------------------------------------------------------------------------
// Purpose: shows the console
//-----------------------------------------------------------------------------
void CEngineVGui::ShowConsole()
{
	if ( IsGameConsole() )
		return;

	if ( staticGameConsole )
	{
		staticGameConsole->Activate();
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the console is currently open
//-----------------------------------------------------------------------------
bool CEngineVGui::IsConsoleVisible()
{
	if ( IsPC() )
	{
		return staticGameConsole && staticGameConsole->IsConsoleVisible();
	}
	else
	{
		// xbox has no drop down console
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: clears all text from the console
//-----------------------------------------------------------------------------
void CEngineVGui::ClearConsole()
{
	if ( staticGameConsole )
	{
		staticGameConsole->Clear();
	}
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CEngineVGui::IsGameUIVisible() 
{
	return staticGameUIPanel && staticGameUIPanel->IsVisible();
}


// list of progress bar strings
struct LoadingProgressDescription_t
{
	LevelLoadingProgress_e eProgress;	// current progress
	int nPercent;						// % of the total time this is at
	int nRepeat;						// number of times this is expected to repeat (usually 0)
	const char *pszDesc;				// user description of progress
};

LoadingProgressDescription_t g_ListenServerLoadingProgressDescriptions[] =
{	
	{ PROGRESS_NONE,						0,		0,		NULL },
	{ PROGRESS_SPAWNSERVER,					5,		0,		"#LoadingProgress_SpawningServer" },
	{ PROGRESS_LOADWORLDMODEL,				8,		5,		"#LoadingProgress_LoadMap" },
	{ PROGRESS_CREATENETWORKSTRINGTABLES,	12,		0,		NULL },
	{ PROGRESS_PRECACHEWORLD,				15,		0,		"#LoadingProgress_PrecacheWorld" },
	{ PROGRESS_CLEARWORLD,					16,		20,		NULL },
	{ PROGRESS_LEVELINIT,					20,		200,	"#LoadingProgress_LoadResources" },
	{ PROGRESS_ACTIVATESERVER,				50,		0,		NULL },
	{ PROGRESS_SIGNONCHALLENGE,				51,		0,		"#LoadingProgress_Connecting" },
	{ PROGRESS_SIGNONCONNECT,				55,		0,		NULL },
	{ PROGRESS_SIGNONCONNECTED,				56,		1,		"#LoadingProgress_SignonLocal" },
	{ PROGRESS_PROCESSSERVERINFO,			58,		0,		NULL },
	{ PROGRESS_PROCESSSTRINGTABLE,			60,		3,		NULL },	// 16
	{ PROGRESS_SIGNONNEW,					63,		200,	NULL },
	{ PROGRESS_SENDCLIENTINFO,				80,		1,		NULL },
	{ PROGRESS_SENDSIGNONDATA,				81,		1,		"#LoadingProgress_SignonDataLocal" },
	{ PROGRESS_SIGNONSPAWN,					83,		10,		NULL },
	{ PROGRESS_CREATEENTITIES,				85,		3,		NULL },
	{ PROGRESS_FULLYCONNECTED,				86,		0,		NULL },
	{ PROGRESS_PRECACHELIGHTING,			87,		50,		NULL },
	{ PROGRESS_READYTOPLAY,					95,		100,	NULL },
	{ PROGRESS_HIGHESTITEM,					100,	0,		NULL },
};

LoadingProgressDescription_t g_RemoteConnectLoadingProgressDescriptions[] =
{	
	{ PROGRESS_NONE,						0,		0,		NULL },
	{ PROGRESS_CHANGELEVEL,					1,		0,		"#LoadingProgress_Changelevel" },
	{ PROGRESS_BEGINCONNECT,				5,		0,		"#LoadingProgress_BeginConnect" },
	{ PROGRESS_SIGNONCHALLENGE,				10,		0,		"#LoadingProgress_Connecting" },
	{ PROGRESS_SIGNONCONNECTED,				11,		0,		NULL },
	{ PROGRESS_PROCESSSERVERINFO,			12,		0,		"#LoadingProgress_ProcessServerInfo" },
	{ PROGRESS_PROCESSSTRINGTABLE,			15,		3,		NULL },
	{ PROGRESS_LOADWORLDMODEL,				20,		14,		"#LoadingProgress_LoadMap" },
	{ PROGRESS_SIGNONNEW,					30,		200,	"#LoadingProgress_PrecacheWorld" },
	{ PROGRESS_SENDCLIENTINFO,				60,		1,		"#LoadingProgress_SendClientInfo" },
	{ PROGRESS_SENDSIGNONDATA,				64,		1,		"#LoadingProgress_SignonData" },
	{ PROGRESS_SIGNONSPAWN,					65,		10,		NULL },
	{ PROGRESS_CREATEENTITIES,				85,		3,		NULL },
	{ PROGRESS_FULLYCONNECTED,				86,		0,		NULL },
	{ PROGRESS_PRECACHELIGHTING,			87,		50,		NULL },
	{ PROGRESS_READYTOPLAY,					95,		100,	NULL },
	{ PROGRESS_HIGHESTITEM,					100,	0,		NULL },
};

static LoadingProgressDescription_t *g_pLoadingProgressDescriptions = NULL;

//-----------------------------------------------------------------------------
// Purpose: returns current progress point description
//-----------------------------------------------------------------------------
LoadingProgressDescription_t &GetProgressDescription(LevelLoadingProgress_e eProgress)
{
	// search for the item in the current list
	int i = 0;
	while ( true )
	{
		// find the closest match
		if (g_pLoadingProgressDescriptions[i].eProgress >= eProgress)
			return g_pLoadingProgressDescriptions[i];
	
		if ( g_pLoadingProgressDescriptions[i].eProgress == PROGRESS_HIGHESTITEM )
			break;

		++i;
	}

	// not found
	return g_pLoadingProgressDescriptions[0];
}

//-----------------------------------------------------------------------------
// Purpose: transition handler
//-----------------------------------------------------------------------------
void CEngineVGui::OnLevelLoadingStarted( char const *levelName, bool bLocalServer )
{
	if (!staticGameUIFuncs)
		return;

	ConVar *pSyncReportConVar = g_pCVar->FindVar( "fs_report_sync_opens" );
	if ( pSyncReportConVar )
	{
		// If convar is set to 2, suppress warnings during level load
		g_syncReportLevel = pSyncReportConVar->GetInt();
		if ( g_syncReportLevel > 1 )
		{
			pSyncReportConVar->SetValue( 0 );
		}
	}
	
	if ( IsGameConsole() || IsPC() )
	{
		// TCR requirement, always!!!
		m_bShowProgressDialog = true;
	}

	// we've starting loading a level/connecting to a server
	staticGameUIFuncs->OnLevelLoadingStarted( levelName, m_bShowProgressDialog );

	// reset progress bar timers
	m_flLoadingStartTime = Plat_FloatTime();
	m_LoadingProgress.RemoveAll();
	m_eLastProgressPoint = PROGRESS_NONE;
	m_nLastProgressPointRepeatCount = 0;
	m_ProgressBias = 0;

	// choose which progress bar to use
	if ( !bLocalServer )
	{
		// we're connecting
		g_pLoadingProgressDescriptions = g_RemoteConnectLoadingProgressDescriptions;
	}
	else
	{
		g_pLoadingProgressDescriptions = g_ListenServerLoadingProgressDescriptions;
	}

	if ( m_bShowProgressDialog )
	{
		ActivateGameUI();
	}

	m_bShowProgressDialog = false;
}

void CEngineVGui::StartLoadingScreenForCommand( const char* command )
{
	staticGameUIFuncs->StartLoadingScreenForCommand( command );
}

void CEngineVGui::StartLoadingScreenForKeyValues( KeyValues* keyValues )
{
	staticGameUIFuncs->StartLoadingScreenForKeyValues( keyValues );
}

//-----------------------------------------------------------------------------
// Purpose: transition handler
//-----------------------------------------------------------------------------
void CEngineVGui::OnLevelLoadingFinished()
{
	if (!staticGameUIFuncs)
		return;

	staticGameUIFuncs->OnLevelLoadingFinished( gfExtendedError, gszDisconnectReason, gszExtendedDisconnectReason );
	m_eLastProgressPoint = PROGRESS_NONE;

	// clear any error message
	gfExtendedError = false;
	gszDisconnectReason[0] = 0;
	gszExtendedDisconnectReason[0] = 0;

#if defined(ENABLE_LOADING_PROGRESS_PROFILING)
	// display progress bar stats (for debugging/tuning progress bar)
	float flEndTime = (float)Plat_FloatTime();
	// add a finished entry
	LoadingProgressEntry_t &entry = m_LoadingProgress[m_LoadingProgress.AddToTail()];
	entry.flTime = flEndTime - m_flLoadingStartTime;
	entry.eProgress = PROGRESS_HIGHESTITEM;
	// dump the info
	Msg("Level load timings:\n");
	float flTotalTime = flEndTime - m_flLoadingStartTime;
	int nRepeatCount = 0;
	float flTimeTaken = 0.0f;
	float flFirstLoadProgressTime = 0.0f;
	for (int i = 0; i < m_LoadingProgress.Count() - 1; i++)
	{
		// keep track of time
		flTimeTaken += (float)m_LoadingProgress[i+1].flTime - m_LoadingProgress[i].flTime;

		// keep track of how often something is repeated
		if (m_LoadingProgress[i+1].eProgress == m_LoadingProgress[i].eProgress)
		{
			if (nRepeatCount == 0)
			{
				flFirstLoadProgressTime = m_LoadingProgress[i].flTime;
			}
			++nRepeatCount;
			continue;
		}

		// work out the time it took to do this
		if (nRepeatCount == 0)
		{
			flFirstLoadProgressTime = m_LoadingProgress[i].flTime;
		}

		int nPerc = (int)(100 * (flFirstLoadProgressTime / flTotalTime));
		int nTickPerc = (int)(100 * ((float)m_LoadingProgress[i].eProgress / (float)PROGRESS_HIGHESTITEM));
		
		// interpolated percentage is in between the real times and the most ticks
		int nInterpPerc = (nPerc + nTickPerc) / 2;
		Msg("\t%2d/%50.50s\t%.3f\t\ttime: %d%%\t\tinterp: %d%%\t\trepeat: %d\n", m_LoadingProgress[i].eProgress, g_ProgressTokens->GetNameByToken( m_LoadingProgress[i].eProgress ), flTimeTaken, nPerc, nInterpPerc, nRepeatCount);

		// reset accumlated vars
		nRepeatCount = 0;
		flTimeTaken = 0.0f;
	}
#endif // ENABLE_LOADING_PROGRESS_PROFILING


	// Restore convar setting after level load
	if ( g_syncReportLevel > 1 )
	{
		ConVar *pSyncReportConVar = g_pCVar->FindVar( "fs_report_sync_opens" );
		if ( pSyncReportConVar )
		{
			pSyncReportConVar->SetValue( g_syncReportLevel );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: transition handler
//-----------------------------------------------------------------------------
void CEngineVGui::ShowErrorMessage()
{
	if (!staticGameUIFuncs || !gfExtendedError)
		return;

	staticGameUIFuncs->OnLevelLoadingFinished( gfExtendedError, gszDisconnectReason, gszExtendedDisconnectReason );
	m_eLastProgressPoint = PROGRESS_NONE;

	// clear any error message
	gfExtendedError = false;
	gszDisconnectReason[0] = 0;
	gszExtendedDisconnectReason[0] = 0;

	HideGameUI();
}


//-----------------------------------------------------------------------------
// Purpose: Updates progress
//-----------------------------------------------------------------------------
#define LOADING_PRESENT_UPDATE_INTERVAL 0.05f
double g_flLastUpdateTime = 0.0f;
void CEngineVGui::UpdateProgressBar( LevelLoadingProgress_e progress, bool showDialog )
{
	if (!staticGameUIFuncs)
		return;

	if ( !ThreadInMainThread() )
		return;

	// Don't update in tools mode; it renders the tools too, which takes forever!
	if ( toolframework->InToolMode() )
		return;

	if (!g_pLoadingProgressDescriptions)
		return;

	// don't go backwards
	if (progress < m_eLastProgressPoint)
		return;

	bool bNewCheckpoint = progress != m_eLastProgressPoint;

	// Early time-based throttle for UpdateProgressBar
	double t = Plat_FloatTime();
    float dt = t - g_flLastUpdateTime;
    if ( ( !bNewCheckpoint && ( dt < LOADING_PRESENT_UPDATE_INTERVAL ) ) || 
		g_pMaterialSystem->IsInFrame() )
	{
		return;
	}

#if defined(ENABLE_LOADING_PROGRESS_PROFILING)
	if ( dt > 0.5f )
	{
		Msg( "%f msec gap in %s\n", 1000.0f * dt, g_ProgressTokens->GetNameByToken( progress ) );
	}
	// track the progress times, for debugging & tuning
	LoadingProgressEntry_t &entry = m_LoadingProgress[m_LoadingProgress.AddToTail()];
	entry.flTime = Plat_FloatTime() - m_flLoadingStartTime;
	entry.eProgress = progress;
#endif

	// count progress repeats
	if ( !bNewCheckpoint )
	{				         
		++m_nLastProgressPointRepeatCount;
#if defined(ENABLE_LOADING_PROGRESS_PROFILING)
		//if ( !( m_nLastProgressPointRepeatCount % 500 ) )
		{
			Msg( "Repeating %s [%d] at %f\n", g_ProgressTokens->GetNameByToken( progress ), m_nLastProgressPointRepeatCount, t - m_flLoadingStartTime );
		}
#endif
	}
	else
	{
		m_nLastProgressPointRepeatCount = 0;

#if defined(ENABLE_LOADING_PROGRESS_PROFILING)
		Msg( "Entering %s at %f\n", g_ProgressTokens->GetNameByToken( progress ), t - m_flLoadingStartTime );
#endif
	}

	// construct a string describing it
	LoadingProgressDescription_t &desc = GetProgressDescription(progress);

	// calculate partial progress
	float flPerc = desc.nPercent / 100.0f;
	if ( desc.nRepeat > 1 && m_nLastProgressPointRepeatCount )
	{
		// cap the repeat count
		m_nLastProgressPointRepeatCount = MIN(m_nLastProgressPointRepeatCount, desc.nRepeat);

		// next progress point
		float flNextPerc = GetProgressDescription((LevelLoadingProgress_e)((int)progress + 1)).nPercent / 100.0f;

		// move along partially towards the next tick
		flPerc += (flNextPerc - flPerc) * ((float)m_nLastProgressPointRepeatCount / desc.nRepeat);
	}

	// the bias allows the loading bar to have an optional reserved initial band
	// isolated from the normal progress descriptions
	flPerc = flPerc * ( 1.0f - m_ProgressBias ) + m_ProgressBias;

	// Send loading progress to the server
	GetBaseLocalClient().SendLoadingProgress( (int)(flPerc * 100) );

	UpdateProgressBar( flPerc, desc.pszDesc, showDialog );

	// Help with profiling load times.
	ETWMarkPrintf( "UpdateProgressBar to %s, stage %d, took %1.3f s", desc.pszDesc, progress, Plat_FloatTime() - g_flLastUpdateTime );

	m_eLastProgressPoint = progress;

	// NOTE: It is necessary to re-read time, since Refresh
	// may block, and if it does, it'll force a refresh every allocation
	// if we don't resample time after the block
	g_flLastUpdateTime = Plat_FloatTime();
}

//-----------------------------------------------------------------------------
// Purpose: Updates progress
//-----------------------------------------------------------------------------
void CEngineVGui::UpdateCustomProgressBar( float progress, const wchar_t *desc )
{
	char ansi[1024];
	g_pVGuiLocalize->ConvertUnicodeToANSI( desc, ansi, sizeof( ansi ) );
	UpdateProgressBar( progress, ansi );
}

void CEngineVGui::StartCustomProgress()
{
	if (!staticGameUIFuncs)
		return;

	// we've starting loading a level/connecting to a server
	staticGameUIFuncs->OnLevelLoadingStarted( NULL, true );
	m_bSaveProgress = staticGameUIFuncs->SetShowProgressText( true );
}

void CEngineVGui::FinishCustomProgress()
{
	if (!staticGameUIFuncs)
		return;

	staticGameUIFuncs->SetShowProgressText( m_bSaveProgress );
	staticGameUIFuncs->OnLevelLoadingFinished( false, "", "" );
}

void CEngineVGui::SetProgressBias( float bias )
{
	m_ProgressBias = bias;
}

void CEngineVGui::UpdateProgressBar( float progress, const char *pDesc, bool showDialog )
{
	if ( !staticGameUIFuncs )
		return;

	bool bUpdated = staticGameUIFuncs->UpdateProgressBar( progress, pDesc ? pDesc : "", showDialog );
	if ( staticGameUIFuncs->LoadingProgressWantsIsolatedRender( false ) )
	{
		while ( staticGameUIFuncs->LoadingProgressWantsIsolatedRender( true ) )
		{
			extern void V_RenderVGuiOnly();
			V_RenderVGuiOnly();

			if ( g_ClientGlobalVariables.frametime != 0.0f && g_ClientGlobalVariables.frametime != 0.1f)
			{
				static ConVarRef host_timescale( "host_timescale" );
				float timeScale = host_timescale.GetFloat() * sv.GetTimescale();
				if ( timeScale <= 0.0f )
					timeScale = 1.0f;

				g_pScaleformUI->RunFrame( g_ClientGlobalVariables.frametime / timeScale );
			}
			else
			{
				break;
			}
		}
	}
	else if ( bUpdated )
	{
		g_pScaleformUI->RunFrame( 0 );
		// re-render vgui on screen
		extern void V_RenderVGuiOnly();
		V_RenderVGuiOnly();
	}
}

void CEngineVGui::UpdateSecondaryProgressBarWithFile( float progress, const char *pPath, int nBytesTotal )
{
	if ( !pPath )
		return;

	char szFile[MAX_PATH];
	V_strcpy_safe( szFile, pPath );
	//V_StripFilename(szFile);
	wchar_t wszPercent[ 10 ];
	V_snwprintf( wszPercent, ARRAYSIZE( wszPercent ), L"%d%%",  (int)(100*progress) );
	wchar_t wszMegs[ 64 ];
	
	char szBytes[32];
	V_strcpy_safe( szBytes, V_pretifymem(nBytesTotal) );
	V_strtowcs( szBytes, -1, wszMegs, ARRAYSIZE( wszMegs ) );

	wchar_t wszFile[ MAX_PLAYER_NAME_LENGTH ];
	g_pVGuiLocalize->ConvertANSIToUnicode( szFile, wszFile, sizeof( wszFile ) );
	wchar_t szWideBuff[ 256 ];
	g_pVGuiLocalize->ConstructString( szWideBuff, sizeof( szWideBuff ), g_pVGuiLocalize->Find( "#SFUI_DownLoading_" ), 3, wszFile, wszPercent, wszMegs );
	UpdateSecondaryProgressBar( progress, szWideBuff );
}

void CEngineVGui::UpdateSecondaryProgressBar( float progress, const wchar_t *desc )
{
	if ( !staticGameUIFuncs )
		return;

	bool bUpdated = staticGameUIFuncs->UpdateSecondaryProgressBar( progress, desc ? desc : L"" );
	if ( staticGameUIFuncs->LoadingProgressWantsIsolatedRender( false ) )
	{
		while ( staticGameUIFuncs->LoadingProgressWantsIsolatedRender( true ) )
		{
			extern void V_RenderVGuiOnly();
			V_RenderVGuiOnly();

			if ( g_ClientGlobalVariables.frametime != 0.0f && g_ClientGlobalVariables.frametime != 0.1f)
			{
				static ConVarRef host_timescale( "host_timescale" );
				float timeScale = host_timescale.GetFloat() * sv.GetTimescale();
				if ( timeScale <= 0.0f )
					timeScale = 1.0f;

				g_pScaleformUI->RunFrame( g_ClientGlobalVariables.frametime / timeScale );
			}
			else
			{
				break;
			}
		}
	}
	else if ( bUpdated )
	{
		g_pScaleformUI->RunFrame( 0 );
		// re-render vgui on screen
		extern void V_RenderVGuiOnly();
		V_RenderVGuiOnly();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns 1 if the key event is handled, 0 if the engine should handle it
//-----------------------------------------------------------------------------
void CEngineVGui::UpdateButtonState( const InputEvent_t &event )
{
	m_pInputInternal->UpdateButtonState( event );
}

		
//-----------------------------------------------------------------------------
// Purpose: Returns 1 if the key event is handled, 0 if the engine should handle it
//-----------------------------------------------------------------------------
bool CEngineVGui::Key_Event( const InputEvent_t &event )
{
	bool bDown = ( event.m_nType == IE_ButtonPressed ) || ( event.m_nType == IE_ButtonDoubleClicked );
	ButtonCode_t code = (ButtonCode_t)event.m_nData;

	if ( IsPC() && IsShiftKeyDown() )
	{
		switch( code )
		{
		case KEY_F1:
			if ( bDown )
			{
				Cbuf_AddText( Cbuf_GetCurrentPlayer(), "debugsystemui" );
			}
			return true;

		case KEY_F2:
			if ( bDown )
			{
				Cbuf_AddText( Cbuf_GetCurrentPlayer(), "demoui" );
			}
			return true;
		}
	}

#if defined( _WIN32 )
	// Ignore alt tilde, since the Japanese IME uses this to toggle itself on/off
	if ( IsPC() && code == KEY_BACKQUOTE && ( IsAltKeyDown() || IsCtrlKeyDown() ) )
		return event.m_nType != IE_ButtonReleased;
#endif
			   
	// ESCAPE toggles game ui
	bool isConsole = IsX360() || IsPS3();
	ButtonCode_t uiToggleKey = isConsole ? KEY_XBUTTON_START : KEY_ESCAPE;
	ButtonCode_t baseButtonCode = GetBaseButtonCode( code );

	// make sure PS3 supports the console default press (START) or ESCAPE
	bool isUIToggleKey = ( baseButtonCode == uiToggleKey ) || 
						 ( IsPS3() && baseButtonCode == KEY_ESCAPE ) ;

	// Hitting the Start button on any xbox controller brings up the game ui, so translate to base code space
	if ( bDown && isUIToggleKey )
	{
		if ( IsPC() )
		{
			if ( cv_console_window_open.GetBool() )
			{
				HideConsole();
			}
			else if ( IsGameUIVisible()  )
			{
				// gameui_hide on console start button but not ESC key.
				if ( baseButtonCode != KEY_ESCAPE )
				{
					// Don't allow hiding of the game ui if there's no level
					const char *pLevelName = engineClient->GetLevelName();
					if ( pLevelName && pLevelName[0] )
					{
						Cbuf_AddText( Cbuf_GetCurrentPlayer(), "gameui_hide" );
						if ( IsDebugSystemVisible() )
						{
							Cbuf_AddText( Cbuf_GetCurrentPlayer(), "debugsystemui 0" );
						}
					}
				}
			}
			else if ( !cv_ignore_ui_activate_key.GetBool() )
			{
				Cbuf_AddText( Cbuf_GetCurrentPlayer(), "gameui_activate" );
			}
			return true;
		}
		if ( IsGameConsole() && !IsGameUIVisible() )
		{
			// console UI does not toggle, engine does "show", but UI needs to handle "hide"
			Cbuf_AddText( Cbuf_GetCurrentPlayer(), "gameui_activate" );
			return true;
		}
	}

	if ( g_pMatSystemSurface && g_pMatSystemSurface->HandleInputEvent( event ) )
	{
		// always let the engine handle the console keys
		// FIXME: Do a lookup of the key bound to toggleconsole
		// want to cache it off so the lookup happens only when keys are bound?
		if ( IsPC() && ( code == KEY_BACKQUOTE ) )
			return false;
		return event.m_nType != IE_ButtonReleased; // don't trap key up events
	}
	return false;
}

void CEngineVGui::Simulate()
{
	if ( IsGameConsole() && r_drawvgui.GetInt() == 0 )
	{
		return;
	}

	toolframework->VGui_PreSimulateAllTools();

	if ( staticPanel )
	{
		VPROF_BUDGET( "CEngineVGui::Simulate", "VGUI_Simulate" );

		// update vgui animations
		//!! currently this has to be done once per dll, because the anim controller object is in a lib;
		//!! need to make it globally pumped (gameUI.dll has it's own version of this)
		GetAnimationController()->UpdateAnimations( Sys_FloatTime() );

		int w, h;
#if defined( USE_SDL ) || defined( OSX )
		uint width,height;
		g_pLauncherMgr->RenderedSize( width, height, false );	// false = get
		w = width;
		h = height;

#elif defined( WIN32 ) 
		if ( ::IsIconic( *pmainwindow ) )
		{
			w = videomode->GetModeWidth();
			h = videomode->GetModeHeight();
		}
		else
		{
			RECT rect;
			::GetClientRect(*pmainwindow, &rect);

			w = rect.right;
			h = rect.bottom;
		}
#elif defined( _PS3 )
		g_pMaterialSystem->GetBackBufferDimensions( w, h );
#else
#error
#endif
		// don't hold this reference over RunFrame()
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->Viewport( 0, 0, w, h );
		}

		staticGameUIFuncs->RunFrame();
		surface()->CalculateMouseVisible();
		{
			FORCE_DEFAULT_SPLITSCREEN_PLAYER_GUARD;
			ivgui()->RunFrame();
		}

		// Some debugging helpers
		DrawMouseFocus();
		DrawKeyFocus();
		VGui_UpdateDrawTreePanel();
		VGui_UpdateTextureListPanel();

		VGui_ActivateMouse();
	}

	toolframework->VGui_PostSimulateAllTools();
}

void CEngineVGui::BackwardCompatibility_Paint()
{
	Paint( (PaintMode_t)(PAINT_UIPANELS | PAINT_INGAMEPANELS) );
}

class CVGuiPaintHelper
{
public:

	void	AddUIPanel( VPANEL panel );
	void	Paint( VPANEL rootPanel, PaintMode_t mode );

private:

	struct Entry_t
	{
		bool	m_bWasVisible;
		VPANEL	m_pVPanel;
		VPANEL	m_Parent;
	};

	typedef void (CVGuiPaintHelper::*mapFunc)( Entry_t &entry );

	void	MapOverEntries( mapFunc mapper );

	// Mapping functions
	void	MapHide( Entry_t &entry );
	void	MapRestore( Entry_t &entry );
	void	MapPaintTraverse( Entry_t &entry );

	CUtlVector< Entry_t >	m_Entries;
};

void CVGuiPaintHelper::AddUIPanel( VPANEL panel )
{
	Entry_t e;
	e.m_pVPanel = panel;
	e.m_bWasVisible = ipanel()->IsVisible( panel );
	e.m_Parent = ipanel()->GetParent( panel );

	m_Entries.AddToTail( e );
}

void CVGuiPaintHelper::MapOverEntries( mapFunc mapper )
{
	for ( int i = 0; i < m_Entries.Count(); ++i )
	{
		(this->*mapper)( m_Entries[ i ] );
	}
}

void CVGuiPaintHelper::MapHide( Entry_t &entry )
{
	ipanel()->SetVisible( entry.m_pVPanel, false );
}

void CVGuiPaintHelper::MapRestore( Entry_t &entry )
{
	ipanel()->SetVisible( entry.m_pVPanel, entry.m_bWasVisible );
}

void CVGuiPaintHelper::MapPaintTraverse( Entry_t &entry )
{
	ipanel()->SetParent( entry.m_pVPanel, 0 );
	surface()->PaintTraverseEx( entry.m_pVPanel, true );
	ipanel()->SetParent( entry.m_pVPanel, entry.m_Parent );
}

void CVGuiPaintHelper::Paint( VPANEL rootPanel, PaintMode_t mode )
{
	if ( mode & PAINT_UIPANELS )
	{
		MapOverEntries( &CVGuiPaintHelper::MapHide );
		surface()->PaintTraverseEx( rootPanel, true );
		MapOverEntries( &CVGuiPaintHelper::MapRestore );
	}

	if ( mode & PAINT_INGAMEPANELS )
	{
		bool bSaveVisible = ipanel()->IsVisible( rootPanel );
		ipanel()->SetVisible( rootPanel, false );
		MapOverEntries( &CVGuiPaintHelper::MapPaintTraverse );
		ipanel()->SetVisible( rootPanel, bSaveVisible );
	}
}
//-----------------------------------------------------------------------------
// Purpose: paints all the vgui elements
//-----------------------------------------------------------------------------
void CEngineVGui::Paint( PaintMode_t mode )
{
	VPROF_BUDGET( "CEngineVGui::Paint", VPROF_BUDGETGROUP_OTHER_VGUI );

	if ( !staticPanel )
		return;

	// setup the base panel to cover the screen
	VPANEL pVPanel = surface()->GetEmbeddedPanel();
	if ( !pVPanel )
		return;
	
	bool drawVgui = IsGameConsole() ? ( r_drawvgui.GetInt() == 1 ) : r_drawvgui.GetBool();

	// Don't draw the console at all if vgui is off during a time demo
	if ( demoplayer->IsPlayingTimeDemo() && !drawVgui )
	{
		return;
	}

	if ( !drawVgui || m_bNoShaderAPI )
	{
		return;
	}

	int w, h;
#if defined( USE_SDL ) || defined ( OSX )
	uint width,height;
	g_pLauncherMgr->RenderedSize( width, height, false );	// false = get
	w = width;
	h = height;

#elif defined( WIN32 )
	if ( ::IsIconic( *pmainwindow ) )
	{
		w = videomode->GetModeWidth();
		h = videomode->GetModeHeight();
	}
	else
	{
		RECT rect;
		::GetClientRect(*pmainwindow, &rect);

		w = rect.right;
		h = rect.bottom;
	}
#elif defined( _PS3 )
	g_pMaterialSystem->GetBackBufferDimensions( w, h );
#else
#error
#endif
	Panel *panel = staticPanel;
	panel->SetBounds(0, 0, w, h);
	panel->Repaint();

	toolframework->VGui_PreRenderAllTools( mode );

	// Paint both ( backward compatibility support )
	CVGuiPaintHelper helper;

	VPANEL fullscreenClientDLLPanel = ClientDLL_GetFullscreenClientDLLVPanel();

	switch( mode )
	{
	case PAINT_UIPANELS:
		{
			// AddUIPanel excludes the 2 non-fullscreen panels from rendering in this pass
			int childcount = ipanel()->GetChildCount( staticClientDLLPanel->GetVPanel() );
			for ( int i = 0; i < childcount; i++ )
			{
				// child always returns null, not in the same module!
				VPANEL child = ipanel()->GetChild( staticClientDLLPanel->GetVPanel(), i );

				if ( child && child != fullscreenClientDLLPanel )
				{
					helper.AddUIPanel( child );
				}
			}
		}
		break;

	case PAINT_INGAMEPANELS:
		{
			int childcount = ipanel()->GetChildCount( staticClientDLLPanel->GetVPanel() );
			for ( int i = 0; i < childcount; i++ )
			{
				// child always returns null, not in the same module!
				VPANEL child = ipanel()->GetChild( staticClientDLLPanel->GetVPanel(), i );

				if ( child && child != fullscreenClientDLLPanel )
				{
					int iMessageContext = ipanel()->GetMessageContextId( child );

					if ( iMessageContext == splitscreen->GetActiveSplitScreenPlayerSlot() )
					{
						helper.AddUIPanel( child );
					}			
				}
			}
		}
		break;

	default:
		break;
	}

#if defined( TOOLFRAMEWORK_VGUI_REFACTOR )
	helper.AddUIPanel( staticGameUIPanel->GetVPanel() );
#endif
	helper.AddUIPanel( staticClientDLLToolsPanel->GetVPanel() );

	// Prevent Steam Overlay from rendering prematurely
	if ( staticSteamOverlayPanel )
	{
		ipanel()->SetVisible( staticSteamOverlayPanel->GetVPanel(), false );
	}

	if ( staticTransitionPanel )
	{
		ipanel()->SetVisible( staticTransitionPanel->GetVPanel(), false );
	}

	// It's either the full screen, or just the client .dll stuff
	helper.Paint( pVPanel, mode );

	if ( staticTransitionPanel && ( mode & PAINT_UIPANELS ) && staticTransitionPanel->IsEffectEnabled() )
	{
		ipanel()->SetVisible( staticTransitionPanel->GetVPanel(), true );
		VPANEL vPanelRememberParent = ipanel()->GetParent( staticTransitionPanel->GetVPanel() );
		ipanel()->SetParent( staticTransitionPanel->GetVPanel(), 0 );
		surface()->PaintTraverseEx( staticTransitionPanel->GetVPanel(), false );
		ipanel()->SetParent( staticTransitionPanel->GetVPanel(), vPanelRememberParent );
	}

	// Paint Steam Overlay
	if ( staticSteamOverlayPanel && ( mode & PAINT_UIPANELS ) )
	{
		ipanel()->SetVisible( staticSteamOverlayPanel->GetVPanel(), true );
		VPANEL vPanelRememberParent = ipanel()->GetParent( staticSteamOverlayPanel->GetVPanel() );
		ipanel()->SetParent( staticSteamOverlayPanel->GetVPanel(), 0 );
		surface()->PaintTraverseEx( staticSteamOverlayPanel->GetVPanel(), false );
		ipanel()->SetParent( staticSteamOverlayPanel->GetVPanel(), vPanelRememberParent );
	}

	toolframework->VGui_PostRenderAllTools( mode );
}

bool CEngineVGui::IsDebugSystemVisible( void )
{
	return staticDebugSystemPanel ? staticDebugSystemPanel->IsVisible() : false;
}

void CEngineVGui::HideDebugSystem( void )
{
	if ( staticDebugSystemPanel )
	{
		staticDebugSystemPanel->SetVisible( false );
		SetEngineVisible( true );
	}
}


void CEngineVGui::ToggleDebugSystemUI( const CCommand &args )
{
	if ( !staticDebugSystemPanel )
		return;

	bool bVisible;
	if ( args.ArgC() == 1 )
	{
		// toggle the game UI
		bVisible = !IsDebugSystemVisible();
	}
	else
	{
		bVisible = atoi( args[1] ) != 0;
	}

	if ( !bVisible )
	{
		staticDebugSystemPanel->SetVisible( false );
		SetEngineVisible( true );
	}
	else
	{
		// clear any keys that might be stuck down
		ClearIOStates();
		staticDebugSystemPanel->SetVisible( true );
		SetEngineVisible( false );
	}
}

bool CEngineVGui::IsShiftKeyDown( void )
{
	if ( !input() )
		return false;

	return input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT );
}

bool CEngineVGui::IsAltKeyDown( void )
{
	if ( !input() )
		return false;

	return input()->IsKeyDown( KEY_LALT ) || input()->IsKeyDown( KEY_RALT );
}

bool CEngineVGui::IsCtrlKeyDown( void )
{
	if ( !input() )
		return false;

	return input()->IsKeyDown( KEY_LCONTROL ) || input()->IsKeyDown( KEY_RCONTROL );
}


//-----------------------------------------------------------------------------
// Purpose: notification
//-----------------------------------------------------------------------------
void CEngineVGui::NotifyOfServerConnect(const char *game, int IP, int connectionPort, int queryPort)
{
	if (!staticGameUIFuncs)
		return;

	staticGameUIFuncs->OnConnectToServer2(game, IP, connectionPort, queryPort);
}

//-----------------------------------------------------------------------------
// Purpose: notification
//-----------------------------------------------------------------------------
void CEngineVGui::NotifyOfServerDisconnect()
{
	if (!staticGameUIFuncs)
		return;

	staticGameUIFuncs->OnDisconnectFromServer( g_eSteamLoginFailure );
	g_eSteamLoginFailure = 0;
}

//-----------------------------------------------------------------------------
// A helper to play sounds through vgui
//-----------------------------------------------------------------------------
void VGui_PlaySound( const char *pFileName )
{
	//Put '+' on the front of sounds to make them play without spatialization.
	char buf[2048];
	Q_snprintf( buf, sizeof( buf ), "%c%s", '+', pFileName );

	// Point at origin if they didn't specify a sound source.
	Vector vDummyOrigin;
	vDummyOrigin.Init();

	CSfxTable *pSound = (CSfxTable*)S_PrecacheSound( buf );
	if ( pSound )
	{
		S_MarkUISound( pSound );

		ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );

		StartSoundParams_t params;
		params.staticsound = IsGameConsole() ? true : false;
		params.soundsource = GetLocalClient().GetViewEntity();
		params.entchannel = CHAN_AUTO;
		params.pSfx = pSound;
		params.origin = vDummyOrigin;
		params.pitch = PITCH_NORM;
		params.soundlevel = SNDLVL_IDLE;
		params.flags = 0;
		params.fvol = 1.0f;

		S_StartSound( params );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGui_ActivateMouse()
{
	if ( !g_ClientDLL )
		return;

	// Don't mess with mouse if not active
	if ( !game->IsActiveApp() )
	{
		g_ClientDLL->IN_DeactivateMouse ();
		return;
	}
		
	/* 
	//
	// MIKE AND ALFRED: these panels should expose whether they want mouse input or not and 
	// CalculateMouseVisible will take them into account.
	//
	// If showing game ui, make sure nothing else is hooking it
	if ( Base().IsGameUIVisible() || Base().IsDebugSystemVisible() )
	{
		g_ClientDLL->IN_DeactivateMouse();
		return;
	}
	*/
			
	if ( surface()->IsCursorLocked() && !g_bTextMode )
	{
		g_ClientDLL->IN_ActivateMouse ();
	}
	else
	{
		g_ClientDLL->IN_DeactivateMouse ();
	}
}

static ConVar mat_drawTitleSafe( "mat_drawTitleSafe", "0", 0, "Enable title safe overlay" );

CUtlVector< VPANEL > g_FocusPanelList;
ConVar vgui_drawfocus( "vgui_drawfocus", "0", 0, "Report which panel is under the mouse." );

VPANEL g_KeyFocusPanel = NULL;
ConVar vgui_drawkeyfocus( "vgui_drawkeyfocus", "0", 0, "Report which panel has keyboard focus." );

CFocusOverlayPanel::CFocusOverlayPanel( Panel *pParent, const char *pName ) : Panel( pParent, pName )
{
	SetPaintEnabled( false );
	SetPaintBorderEnabled( false );
	SetPaintBackgroundEnabled( false );

	MakePopup();

	SetPostChildPaintEnabled( true );
	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( false );
}

bool CFocusOverlayPanel::DrawTitleSafeOverlay( void )
{
	if ( !mat_drawTitleSafe.GetBool() )
		return false;

	int backBufferWidth, backBufferHeight;
	materials->GetBackBufferDimensions( backBufferWidth, backBufferHeight );

	int x, y, x1, y1, insetX, insetY;

	if ( IsX360() || !IsPS3() )
	{
		// Required Xbox360 Title safe is TCR documented at inner 90% (RED)
		insetX = 0.05f * backBufferWidth;
		insetY = 0.05f * backBufferHeight;

		x = insetX;
		y = insetY;
		x1 = backBufferWidth - insetX;
		y1 = backBufferHeight - insetY;

		vgui::surface()->DrawSetColor( 255, 0, 0, 255 );
		vgui::surface()->DrawOutlinedRect( x, y, x1, y1 );

		// Suggested Xbox360 Title Safe is TCR documented at inner 85% (YELLOW)
		insetX = 0.075f * backBufferWidth;
		insetY = 0.075f * backBufferHeight;

		x = insetX;
		y = insetY;
		x1 = backBufferWidth - insetX;
		y1 = backBufferHeight - insetY;

		vgui::surface()->DrawSetColor( 255, 255, 0, 255 );
		vgui::surface()->DrawOutlinedRect( x, y, x1, y1 );
	}
	else if ( IsPS3() )
	{
		// Required PS3 Title Safe is TCR documented at inner 85% (RED)
		insetX = 0.075f * backBufferWidth;
		insetY = 0.075f * backBufferHeight;

		x = insetX;
		y = insetY;
		x1 = backBufferWidth - insetX;
		y1 = backBufferHeight - insetY;

		vgui::surface()->DrawSetColor( 255, 0, 0, 255 );
		vgui::surface()->DrawOutlinedRect( x, y, x1, y1 );
	}

	return true;
}

void CFocusOverlayPanel::PostChildPaint( void )
{
	BaseClass::PostChildPaint();

	bool bNeedsMoveToFront = false;

	if ( g_DrawTreeSelectedPanel )
	{
		int x, y, x1, y1;
		ipanel()->GetClipRect( g_DrawTreeSelectedPanel, x, y, x1, y1 );
		surface()->DrawSetColor( Color( 255, 0, 0, 255 ) );
		surface()->DrawOutlinedRect( x, y, x1, y1 );

		bNeedsMoveToFront = true;
	}

	if ( DrawTitleSafeOverlay() )
	{
		bNeedsMoveToFront = true;
	}

	if ( DrawFocusPanelList() )
	{
		bNeedsMoveToFront = true;
	}

	if ( DrawKeyFocusPanel() )
	{
		bNeedsMoveToFront = true;
	}

	if ( bNeedsMoveToFront )
	{
		// will be valid for the next frame
		MoveToFront();
	}
}

bool CFocusOverlayPanel::DrawKeyFocusPanel( void )
{
	if( !vgui_drawkeyfocus.GetBool() )
		return false;

	if ( g_KeyFocusPanel )
	{
		int x, y, x1, y1;
		ipanel()->GetClipRect( g_KeyFocusPanel, x, y, x1, y1 );

		if ( (x1 - x) == videomode->GetModeWidth() && 
			 (y1 - y) == videomode->GetModeHeight() )
		{
			x++;
			y++;
			x1--;
			y1--;
		}

		const int FOCUS_BLINK_PERIOD = 500;
		if ( system()->GetTimeMillis() % FOCUS_BLINK_PERIOD > FOCUS_BLINK_PERIOD/2 )
		{
			surface()->DrawSetColor( Color( 0, 0, 0, 255 ) );
		}
		else
		{
			surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
		}

		surface()->DrawOutlinedRect( x, y, x1, y1 );
	}

	return true;
}

bool CFocusOverlayPanel::DrawFocusPanelList( void )
{
	if( !vgui_drawfocus.GetBool() )
		return false;

	int c = g_FocusPanelList.Count();
	if ( c <= 0 )
		return false;

	int slot = 0;
	int fullscreeninset = 0;

	for ( int i = 0; i < c; i++ )
	{
		if ( slot > 31 )
			break;

		VPANEL vpanel = g_FocusPanelList[ i ];
		if ( !vpanel )
			continue;

		if ( !ipanel()->IsFullyVisible( vpanel ) )
			return false;

		// Convert panel bounds to screen space
		int r, g, b;
		GetColorForSlot( slot, r, g, b );

		int x, y, x1, y1;
		ipanel()->GetClipRect( vpanel, x, y, x1, y1 );

		if ( (x1 - x) == videomode->GetModeWidth() && 
			 (y1 - y) == videomode->GetModeHeight() )
		{
			x += fullscreeninset;
			y += fullscreeninset;
			x1 -= fullscreeninset;
			y1 -= fullscreeninset;
			fullscreeninset++;
		}
		surface()->DrawSetColor( Color( r, g, b, 255 ) );
		surface()->DrawOutlinedRect( x, y, x1, y1 );

		slot++;
	}

	return true;
}


static void VGui_RecursiveFindPanels( CUtlVector< VPANEL  >& panelList, VPANEL check, char const *panelname )
{
	Panel *panel = ipanel()->GetPanel( check, "ENGINE" );
	if ( !panel )
		return;

	if ( StringHasPrefixCaseSensitive( panel->GetName(), panelname ) )
	{
		panelList.AddToTail( panel->GetVPanel() );
	}

	int childcount = panel->GetChildCount();
	for ( int i = 0; i < childcount; i++ )
	{
		Panel *child = panel->GetChild( i );
		VGui_RecursiveFindPanels( panelList, child->GetVPanel(), panelname );
	}
}

void VGui_FindNamedPanels( CUtlVector< VPANEL >& panelList, char const *panelname )
{
	VPANEL embedded = surface()->GetEmbeddedPanel();

	// faster version of code below
	// checks through each popup in order, top to bottom windows
	int c = surface()->GetPopupCount();
	for (int i = c - 1; i >= 0; i--)
	{
		VPANEL popup = surface()->GetPopup(i);
		if ( !popup )
			continue;

		if ( embedded == popup )
			continue;

		VGui_RecursiveFindPanels( panelList, popup, panelname );
	}

	VGui_RecursiveFindPanels( panelList, embedded, panelname );
}

CON_COMMAND( vgui_togglepanel, "show/hide vgui panel by name." )
{
	if ( args.ArgC() < 2 )
	{
		ConMsg( "Usage:  vgui_showpanel panelname\n" );
		return;
	}

	bool flip = false;
	bool fg = true;
	bool bg = true;

	if ( args.ArgC() == 5 )
	{
		flip = atoi( args[ 2 ] ) ? true : false;
		fg = atoi( args[ 3 ] ) ? true : false;
		bg = atoi( args[ 4 ] ) ? true : false;
	}
		
	char const *panelname = args[ 1 ];
	if ( !panelname || !panelname[ 0 ] )
		return;

	CUtlVector< VPANEL > panelList;

	VGui_FindNamedPanels( panelList, panelname );
	if ( !panelList.Count() )
	{
		ConMsg( "No panels starting with %s\n", panelname );
		return;
	}

	for ( int i = 0; i < panelList.Count(); i++ )
	{
		VPANEL p = panelList[ i ];
		if ( !p )
			continue;

		Panel *panel = ipanel()->GetPanel( p, "ENGINE");
		if ( !panel )
			continue;

		Msg( "Toggling %s\n", panel->GetName() );

		if ( fg )
		{
			panel->SetPaintEnabled( flip );
		}
		if ( bg )
		{
			panel->SetPaintBackgroundEnabled( flip );
		}
	}
}

static void VGui_RecursePanel( CUtlVector< VPANEL >& panelList, int x, int y, VPANEL check, bool include_hidden )
{
	if( !include_hidden && !ipanel()->IsVisible( check ) )
	{
		return;
	}

	if ( ipanel()->IsWithinTraverse( check, x, y, false ) )
	{
		if ( panelList.Find( check ) == panelList.InvalidIndex() )
			panelList.AddToTail( check );
	}

	int childcount = ipanel()->GetChildCount( check );
	for ( int i = 0; i < childcount; i++ )
	{
		VPANEL child = ipanel()->GetChild( check, i );
		VGui_RecursePanel( panelList, x, y, child, include_hidden );
	}
}

void CEngineVGui::DrawKeyFocus( void )
{
	VPROF( "CEngineVGui::DrawKeyFocus" );

	if ( !vgui_drawkeyfocus.GetBool() )
		return;

	staticFocusOverlayPanel->MoveToFront();
	g_KeyFocusPanel = input()->GetFocus();
}

void CEngineVGui::DrawMouseFocus( void )
{
	VPROF( "CEngineVGui::DrawMouseFocus" );

	g_FocusPanelList.RemoveAll();

	if ( !vgui_drawfocus.GetBool() )
		return;
	
	staticFocusOverlayPanel->MoveToFront();

	bool include_hidden = vgui_drawfocus.GetInt() == 2;

	int x, y;
	input()->GetCursorPos( x, y );

	VPANEL embedded = surface()->GetEmbeddedPanel();

	if ( surface()->IsCursorVisible() && surface()->IsWithin(x, y) )
	{
		// faster version of code below
		// checks through each popup in order, top to bottom windows
		int c = surface()->GetPopupCount();
		for (int i = c - 1; i >= 0; i--)
		{
			VPANEL popup = surface()->GetPopup(i);
			if ( !popup )
				continue;

			if ( popup == embedded )
				continue;
			if ( !ipanel()->IsVisible( popup ) )
				continue;

			VGui_RecursePanel( g_FocusPanelList, x, y, popup, include_hidden );
		}

		VGui_RecursePanel( g_FocusPanelList, x, y, embedded, include_hidden );
	}

	// Now draw them
	con_nprint_t np;
	np.time_to_live = 1.0f;
	
	int c = g_FocusPanelList.Count();

	int slot = 0;
	for ( int i = 0; i < c; i++ )
	{
		if ( slot > 31 )
			break;

		VPANEL vpanel = g_FocusPanelList[ i ];
		if ( !vpanel )
			continue;

		np.index = slot;

		int r, g, b;
		CFocusOverlayPanel::GetColorForSlot( slot, r, g, b );

		np.color[ 0 ] = r / 255.0f;
		np.color[ 1 ] = g / 255.0f;
		np.color[ 2 ] = b / 255.0f;

		bool bThisOne = false;
		if ( input()->GetMouseFocus()  == vpanel )
		{
			bThisOne = true;
		}

		Con_NXPrintf( &np, "%s %3i:  %s(vpanel%d)(ctx%d)\n", bThisOne ? "-->" : "   ", slot + 1, ipanel()->GetName(vpanel), vpanel, ipanel()->GetMessageContextId( vpanel ) );

		slot++;
	}

	while ( slot <= 31 )
	{
		Con_NPrintf( slot, "" );
		slot++;
	}
}

void VGui_SetGameDLLPanelsVisible( bool show )
{
	EngineVGui()->SetGameDLLPanelsVisible( show );
}

void CEngineVGui::SetProgressLevelName( const char *levelName )
{
//	staticGameUIFuncs->SetProgressLevelName( levelName );
}

void CEngineVGui::OnToolModeChanged( bool bGameMode )
{
//#if defined( TOOLFRAMEWORK_VGUI_REFACTOR )
	staticEngineToolsPanel->SetParent( bGameMode ? staticPanel->GetVPanel() : staticGameDLLPanel->GetVPanel() );
	staticEngineToolsPanel->SetMouseInputEnabled( false );
	staticEngineToolsPanel->SetKeyBoardInputEnabled( false );

//#endif
}

void CEngineVGui::NeedConnectionProblemWaitScreen()
{
	return staticGameUIFuncs->NeedConnectionProblemWaitScreen();
}

void CEngineVGui::ShowPasswordUI( char const *pchCurrentPW )
{
//	staticGameUIFuncs->ShowPasswordUI( pchCurrentPW );
}

bool CEngineVGui::IsPlayingFullScreenVideo()
{
	return staticGameUIFuncs->IsPlayingFullScreenVideo();
}

//-----------------------------------------------------------------------------
// Dump the panel hierarchy
//-----------------------------------------------------------------------------
void DumpPanels_r( VPANEL panel, int level, bool bVisibleOnly )
{
	int i;

	const char *pName = ipanel()->GetName( panel );

	bool bVisible = ipanel()->IsVisible( panel );
	if ( bVisibleOnly && !bVisible )
	{
		// cull
		return;
	}

	char indentBuff[32];
	for ( i = 0; i < level; i++ )
	{
		indentBuff[i] = '.';
	}
	indentBuff[i] = '\0';
	
	ConMsg( "%s%s%s\n", indentBuff, pName[0] ? pName : "???", bVisible ? " (Visible)" : " (Not Visible)" );

	int childcount = ipanel()->GetChildCount( panel );
	for ( i = 0; i < childcount; i++ )
	{
		VPANEL child = ipanel()->GetChild( panel, i );
		DumpPanels_r( child, level+1, bVisibleOnly );
	}
}
CON_COMMAND( vgui_dump_panels, "vgui_dump_panels [visible]" )
{
	bool bVisibleOnly = ( args.ArgC() == 2 ) && ( !V_stricmp( args[1], "visible" ) );

	VPANEL embedded = surface()->GetEmbeddedPanel();
	DumpPanels_r( embedded, 0, bVisibleOnly );
}

