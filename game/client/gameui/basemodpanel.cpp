//========= Copyright  1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "cbase.h"
#include "basemodpanel.h"
#include "uigamedata.h"

#include "./GameUI/IGameUI.h"
#include "ienginevgui.h"
#include "engine/ienginesound.h"
#include "EngineInterface.h"
#include "tier0/dbg.h"
#include "ixboxsystem.h"
#include "GameUI_Interface.h"
#include "game/client/IGameClientExports.h"
#include "gameui/igameconsole.h"
#include "inputsystem/iinputsystem.h"
#include "FileSystem.h"
#include "filesystem/IXboxInstaller.h"

#ifdef _GAMECONSOLE
	#include "xbox/xbox_launch.h"
#endif

#include "gameconsole.h"
#include "vgui/ISystem.h"
#include "vgui/ISurface.h"
#include "vgui/ILocalize.h"
#include "vgui_controls/AnimationController.h"
#include "vguimatsurface/imatsystemsurface.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "tier0/icommandline.h"
#include "fmtstr.h"
#include "smartptr.h"

// Embedded GameUI
#include "../gameui.h"
#include "game_controls/igameuisystemmgr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace BaseModUI;
using namespace vgui;

//setup in GameUI_Interface.cpp
extern class IMatchSystem *matchsystem;
extern const char *COM_GetModDirectory( void );
extern IGameConsole *IGameConsole();

//=============================================================================
CBaseModPanel* CBaseModPanel::m_CFactoryBasePanel = 0;

#ifndef _CERT
#ifdef _GAMECONSOLE
ConVar ui_gameui_debug( "ui_gameui_debug", "1" );
#else
ConVar ui_gameui_debug( "ui_gameui_debug", "0", FCVAR_RELEASE );
#endif
int UI_IsDebug()
{
	return (*(int *)(&ui_gameui_debug)) ? ui_gameui_debug.GetInt() : 0;
}
#endif

#if defined( _GAMECONSOLE )
static void InstallStatusChanged( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	// spew out status
	if ( ((ConVar *)pConVar)->GetBool() && g_pXboxInstaller )
	{
		g_pXboxInstaller->SpewStatus();
	}
}
ConVar xbox_install_status( "xbox_install_status", "0", 0, "Show install status", InstallStatusChanged );
#endif

// Use for show demos to force the correct campaign poster
ConVar demo_campaign_name( "demo_campaign_name", "L4D2C5", FCVAR_DEVELOPMENTONLY, "Short name of campaign (i.e. L4D2C5), used to show correct poster in demo mode." );

ConVar ui_lobby_noresults_create_msg_time( "ui_lobby_noresults_create_msg_time", "2.5", FCVAR_DEVELOPMENTONLY );

//=============================================================================

void SetGameUiEmbeddedScreen( char const *szBaseName )
{
	if ( !szBaseName || !*szBaseName )
	{
		g_pGameUISystemMgr->ReleaseAllGameUIScreens();
		g_pGameUISystemMgr->SetGameUIVisible( false );
		return;
	}

	IGameUISystem *pGameUiSystem = g_pGameUISystemMgr->LoadGameUIScreen(
		KeyValues::AutoDeleteInline( new KeyValues( szBaseName ) ) );
	pGameUiSystem;
	g_pGameUISystemMgr->SetGameUIVisible( true );
}

//=============================================================================
CBaseModPanel::CBaseModPanel(): BaseClass(0, "CBaseModPanel"),
	m_bClosingAllWindows( false ),
	m_lastActiveUserId( 0 )
{
#if !defined( _GAMECONSOLE ) && !defined( NOSTEAM )
	// Set Steam overlay position
	if ( steamapicontext && steamapicontext->SteamUtils() )
	{
		steamapicontext->SteamUtils()->SetOverlayNotificationPosition( k_EPositionTopRight );
	}

	// Set special DLC parameters mask
	static ConVarRef mm_dlcs_mask_extras( "mm_dlcs_mask_extras" );
	if ( mm_dlcs_mask_extras.IsValid() && steamapicontext && steamapicontext->SteamUtils() )
	{
		int iDLCmask = mm_dlcs_mask_extras.GetInt();

		// Low Violence and Germany (or bordering countries) = CS.GUNS
		char const *cc = steamapicontext->SteamUtils()->GetIPCountry();
		char const *ccGuns = ":DE:DK:PL:CZ:AT:CH:FR:LU:BE:NL:";
		if ( engine->IsLowViolence() && Q_stristr( ccGuns, CFmtStr( ":%s:", cc ) ) )
		{
			// iDLCmask |= ( 1 << ? );
		}

		// PreOrder DLC AppId Ownership = BAT
		if ( steamapicontext->SteamApps()->BIsSubscribedApp( 565 ) )
		{
			// iDLCmask |= ( 1 << ? );
		}

		mm_dlcs_mask_extras.SetValue( iDLCmask );
	}

#endif

	MakePopup( false );

	Assert(m_CFactoryBasePanel == 0);
	m_CFactoryBasePanel = this;

	g_pVGuiLocalize->AddFile( "Resource/basemodui_%language%.txt");
	g_pVGuiLocalize->AddFile( "Resource/basemodui_tu_%language%.txt" );

	m_LevelLoading = false;
	
	// delay 3 frames before doing activation on initialization
	// needed to allow engine to exec startup commands (background map signal is 1 frame behind) 
	m_DelayActivation = 3;

	m_UIScheme = vgui::scheme()->LoadSchemeFromFileEx( 0, "resource/BaseModScheme.res", "BaseModScheme" );
	SetScheme( m_UIScheme );

	// Only one user on the PC, so set it now
	SetLastActiveUserId( IsPC() ? 0 : -1 );

	// Precache critical font characters for the 360, dampens severity of these runtime i/o hitches
	IScheme *pScheme = vgui::scheme()->GetIScheme( m_UIScheme );
	m_hDefaultFont = pScheme->GetFont( "Default", true );
	vgui::surface()->PrecacheFontCharacters( m_hDefaultFont, NULL );
	vgui::surface()->PrecacheFontCharacters( pScheme->GetFont( "DefaultBold", true ), NULL );
	vgui::surface()->PrecacheFontCharacters( pScheme->GetFont( "DefaultLarge", true ), NULL );
	vgui::surface()->PrecacheFontCharacters( pScheme->GetFont( "FrameTitle", true ), NULL );

	m_bWarmRestartMode = false;
	m_ExitingFrameCount = 0;

	m_flBlurScale = 0;
	m_flLastBlurTime = 0;

	// Background movie
	m_BIKHandle = BIKHANDLE_INVALID;
	m_pMovieMaterial = NULL;
	m_flU0 = m_flV0 = m_flU1 = m_flV1 = 0.0f;
	m_bMovieFailed = false;

	m_iBackgroundImageID = -1;
	m_iFadeToBackgroundImageID = -1;

	m_backgroundMusic = "";
	m_nBackgroundMusicGUID = 0;
	m_bFadeMusicUp = false;

	m_flMovieFadeInTime = 0;
	m_iMovieTransitionImage = 0;

	// Subscribe to event notifications
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );
}

//=============================================================================
CBaseModPanel::~CBaseModPanel()
{
	// Unsubscribe from event notifications
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );

	Assert(m_CFactoryBasePanel == this);
	m_CFactoryBasePanel = 0;

	// Free our movie resources
	ShutdownBackgroundMovie();

	surface()->DestroyTextureID( m_iBackgroundImageID );
	surface()->DestroyTextureID( m_iFadeToBackgroundImageID );

	// Shutdown UI game data
	CUIGameData::Shutdown();
}

//=============================================================================
CBaseModPanel& CBaseModPanel::GetSingleton()
{
	Assert(m_CFactoryBasePanel != 0);
	return *m_CFactoryBasePanel;
}

//=============================================================================
CBaseModPanel* CBaseModPanel::GetSingletonPtr()
{
	return m_CFactoryBasePanel;
}

//=============================================================================
void CBaseModPanel::ReloadScheme()
{
}

bool CBaseModPanel::IsLevelLoading()
{
	return m_LevelLoading;
}

#if defined( _GAMECONSOLE ) && defined( _DEMO )
void CBaseModPanel::OnDemoTimeout()
{
	if ( !engine->IsInGame() && !engine->IsConnected() && !engine->IsDrawingLoadingImage() )
	{
		// exit is terminal and unstoppable
		StartExitingProcess( false );
	}
	else
	{
		engine->ExecuteClientCmd( "disconnect" );
	}
}
#endif

bool CBaseModPanel::ActivateBackgroundEffects()
{
	// PC needs to keep start music, can't loop MP3's
	if ( IsPC() && !IsBackgroundMusicPlaying() )
	{
		StartBackgroundMusic( 1.0f );
		m_bFadeMusicUp = false;
	}

	// bring up the video if we haven't before
	if ( m_BIKHandle == BIKHANDLE_INVALID )
	{
		if ( !InitBackgroundMovie() )
		{
			// couldn't start movie, don't do the music either
			return false;
		}
		
		if ( IsGameConsole() && !IsBackgroundMusicPlaying() )
		{
			// only xbox's fades non-playing music up
			m_bFadeMusicUp = StartBackgroundMusic( 0 );
		}
		else
		{
			m_bFadeMusicUp = false;
		}

		m_flMovieFadeInTime = 0;
	}

	return true;
}

//=============================================================================
void CBaseModPanel::OnGameUIActivated()
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CBaseModPanel::OnGameUIActivated( delay = %d )\n", m_DelayActivation );
	}

	if ( m_DelayActivation )
	{
		return;
	}

	COM_TimestampedLog( "CBaseModPanel::OnGameUIActivated()" );

#if defined( _GAMECONSOLE )
	if ( !engine->IsInGame() && !engine->IsConnected() && !engine->IsDrawingLoadingImage() )
	{
#if defined( _DEMO )
		if ( engine->IsDemoExiting() )
		{
			// just got activated, maybe from a disconnect
			// exit is terminal and unstoppable
			SetVisible( true );
			StartExitingProcess( false );
			return;
		}
#endif
		if ( !GameUI().IsInLevel() && !GameUI().IsInBackgroundLevel() )
		{
			// not using a background map
			// start the menu movie and music now, as the main menu is about to open
			// these are very large i/o operations on the xbox
			// they must occur before the installer takes over the DVD
			// otherwise the transfer rate is so slow and we sync stall for 10-15 seconds
			ActivateBackgroundEffects();
		}
		// the installer runs in the background during the main menu
		g_pXboxInstaller->Start();

#if defined( _DEMO )
		// ui valid can now adhere to demo timeout rules
		engine->EnableDemoTimeout( true );
#endif
	}
#endif

	SetVisible( true );

	if ( !IsGameConsole() && IsLevelLoading() )
	{
		// Ignore UI activations when loading poster is up
		return;
	}
	else if ( ( !m_LevelLoading && !engine->IsConnected() ) || GameUI().IsInBackgroundLevel() )
	{
		OpenFrontScreen();
	}
	else if ( engine->IsConnected() && !m_LevelLoading )
	{
		SetGameUiEmbeddedScreen( "ingamemenu" );
	}
}

void CBaseModPanel::OpenFrontScreen()
{
	char const *szScreen = NULL;
#ifdef _GAMECONSOLE
	// make sure we are in the startup menu.
	if ( !GameUI().IsInBackgroundLevel() )
	{
		engine->ClientCmd( "startupmenu" );
	}

	if ( g_pMatchFramework->GetMatchSession() )
	{
		Warning( "CBaseModPanel::OpenFrontScreen during active game ignored!\n" );
		return;
	}

	if( XBX_GetNumGameUsers() > 0 )
	{
		if ( 0 ) // ( CL4DFrame *pAttractScreen = GetWindow( WT_ATTRACTSCREEN ) )
		{
			szScreen = "attractscreen";
		}
		else
		{
			szScreen = "mainmenu";
		}
	}
	else
	{
		szScreen = "attractscreen";
	}
#else
	szScreen = "mainmenu";
#endif // _GAMECONSOLE

	if( szScreen )
	{
		SetGameUiEmbeddedScreen( NULL );	// TEMP HACK: only devconsole interferes with this event being fired multiple times in main menu
		//	need a better fix for devconsole to be a little smarter about gameui activation events

		SetGameUiEmbeddedScreen( szScreen );
	}
}

//=============================================================================
void CBaseModPanel::OnGameUIHidden()
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CBaseModPanel::OnGameUIHidden()\n" );
	}

#if defined( _GAMECONSOLE )
	// signal the installer to stop
	g_pXboxInstaller->Stop();
#endif

	SetVisible(false);

	// Close all gameui screens
	SetGameUiEmbeddedScreen( NULL );

	// Free the movie resouces
	ShutdownBackgroundMovie();
}

//=============================================================================
void CBaseModPanel::RunFrame()
{
	if ( s_NavLock > 0 )
	{
		--s_NavLock;
	}

	GetAnimationController()->UpdateAnimations( Plat_FloatTime() );

	CUIGameData::Get()->RunFrame();

	if ( m_DelayActivation )
	{
		m_DelayActivation--;
		if ( !m_LevelLoading && !m_DelayActivation )
		{
			if ( UI_IsDebug() )
			{
				Msg( "[GAMEUI] Executing delayed UI activation\n");
			}
			OnGameUIActivated();
		}
	}

	bool bDoBlur = true;
	bDoBlur = false;
#if 0 // TODO: UI: determine blur
	WINDOW_TYPE wt = GetActiveWindowType();
	switch ( wt )
	{
	case WT_NONE:
	case WT_MAINMENU:
	case WT_LOADINGPROGRESSBKGND:
	case WT_LOADINGPROGRESS:
	case WT_AUDIOVIDEO:
		bDoBlur = false;
		break;
	}
	if ( GetWindow( WT_ATTRACTSCREEN ) || ( enginevguifuncs && !enginevguifuncs->IsGameUIVisible() ) )
	{
		// attract screen might be open, but not topmost due to notification dialogs
		bDoBlur = false;
	}
#endif

	if ( !bDoBlur )
	{
		bDoBlur = GameClientExports()->ClientWantsBlurEffect();
	}

	float nowTime = Plat_FloatTime();
	float deltaTime = nowTime - m_flLastBlurTime;
	if ( deltaTime > 0 )
	{
		m_flLastBlurTime = nowTime;
		m_flBlurScale += deltaTime * bDoBlur ? 0.05f : -0.05f;
		m_flBlurScale = clamp( m_flBlurScale, 0, 0.85f );
		engine->SetBlurFade( m_flBlurScale );
	}

	if ( IsGameConsole() && m_ExitingFrameCount )
	{
#if 0 // TODO: UI: CTransitionScreen
		CTransitionScreen *pTransitionScreen = static_cast< CTransitionScreen* >( GetWindow( WT_TRANSITIONSCREEN ) );
		if ( pTransitionScreen && pTransitionScreen->IsTransitionComplete() )
		{
			// totally obscured, safe to shutdown movie
			ShutdownBackgroundMovie();

			if ( m_ExitingFrameCount > 1 )
			{
				m_ExitingFrameCount--;
				if ( m_ExitingFrameCount == 1 )
				{
					// enough frames have transpired, send the single shot quit command
					if ( m_bWarmRestartMode )
					{
						// restarts self, skips any intros
						engine->ClientCmd_Unrestricted( "quit_x360 restart\n" );
					}
					else
					{
						// cold restart, quits to any startup app
						engine->ClientCmd_Unrestricted( "quit_x360\n" );
					}
				}
			}
		}
#endif
	}
}


//=============================================================================
void CBaseModPanel::OnLevelLoadingStarted( char const *levelName, bool bShowProgressDialog )
{
	Assert( !m_LevelLoading );

#if defined( _GAMECONSOLE )
	// stop the installer
	g_pXboxInstaller->Stop();
	g_pXboxInstaller->SpewStatus();

	// If the installer has finished while we are in the menus, then this is the ONLY place we
	// know that there is no open files and we can redirect the search paths
	if ( g_pXboxInstaller->ForceCachePaths() )
	{
		// the search paths got changed
		// notify other systems who may have hooked absolute paths
		engine->SearchPathsChangedAfterInstall();
	}
#endif

	// close all UI screens
	SetGameUiEmbeddedScreen( NULL );

	// Stop the background movie
	ShutdownBackgroundMovie();

	DevMsg( 2, "[GAMEUI] OnLevelLoadingStarted - opening loading progress (%s)...\n",
		levelName ? levelName : "<< no level specified >>" );

	//
	// If playing on listen server then "levelName" is set to the map being loaded,
	// so it is authoritative - it might be a background map or a real level.
	//
	if ( levelName )
	{
		// Derive the mission info from the server game details
		KeyValues *pGameSettings = g_pMatchFramework->GetMatchNetworkMsgController()->GetActiveServerGameDetails( NULL );
		if ( !pGameSettings )
		{
			// In this particular case we need to speculate about game details
			// this happens when user types "map c5m2 versus easy" from console, so there's no
			// active server spawned yet, nor is the local client connected to any server.
			// We have to force server DLL to apply the map command line to the settings and then
			// speculatively construct the settings key.
			if ( IServerGameDLL *pServerDLL = ( IServerGameDLL * ) g_pMatchFramework->GetMatchExtensions()->GetRegisteredExtensionInterface( INTERFACEVERSION_SERVERGAMEDLL ) )
			{
				KeyValues *pApplyServerSettings = new KeyValues( "::ExecGameTypeCfg" );
				KeyValues::AutoDelete autodelete_pApplyServerSettings( pApplyServerSettings );

				pApplyServerSettings->SetString( "map/mapname", levelName );

				pServerDLL->ApplyGameSettings( pApplyServerSettings );
			}

			// Now we can retrieve all the settings from convars here!
			static ConVarRef r_mp_gamemode( "mp_gamemode" );
			if ( r_mp_gamemode.IsValid() )
			{
				pGameSettings = new KeyValues( "CmdLineSettings" );
				pGameSettings->SetString( "game/mode", r_mp_gamemode.GetString() );
			}
		}
		
		KeyValues::AutoDelete autodelete_pGameSettings( pGameSettings );
		if ( pGameSettings )
		{
			// It is critical to get map info by the actual levelname that is being loaded, because
			// for level transitions the server is still in the old map and the game settings returned
			// will reflect the old state of the server.
			// - pChapterInfo = g_pMatchExtPortal2->GetMapInfoByBspName( pGameSettings, levelName, &pMissionInfo );
			// - Q_strncpy( chGameMode, pGameSettings->GetString( "game/mode", "" ), ARRAYSIZE( chGameMode ) );
		}

		// Let the ui nuggets know the loading map
		IGameUIScreenControllerFactory *pFactory = g_pGameUISystemMgr->GetScreenControllerFactory( "loadingprogress" );
		if ( pFactory && pFactory->GetControllerInstancesCount() )
		{
			KeyValues *kvEvent = new KeyValues( "OnLevelLoadingProgress" );
			KeyValues::AutoDelete autodelete_kvEvent( kvEvent );
			kvEvent->SetString( "map", levelName );
			kvEvent->SetFloat( "progress", 0.0f );

			for ( int j = 0; j < pFactory->GetControllerInstancesCount(); ++ j )
			{
				pFactory->GetControllerInstance(j)->BroadcastEventToScreens( kvEvent );
			}
		}
	}
	
	m_LevelLoading = true;

	// Bring up the level loading screen
	SetGameUiEmbeddedScreen( "loadingbar" );
}

void CBaseModPanel::OnEngineLevelLoadingSession( KeyValues *pEvent )
{
#if 0 // TODO: UI: OnEngineLevelLoadingSession
	// We must keep the default loading poster because it will be replaced by
	// the real campaign loading poster shortly
	float flProgress = 0.0f;
	if ( LoadingProgress *pLoadingProgress = static_cast<LoadingProgress*>( GetWindow( WT_LOADINGPROGRESS ) ) )
	{
		flProgress = pLoadingProgress->GetProgress();
		pLoadingProgress->Close();
		m_Frames[ WT_LOADINGPROGRESS ] = NULL;
	}
	CloseAllWindows( CLOSE_POLICY_DEFAULT );

	// Pop up a fake bkgnd poster
	if ( LoadingProgress *pLoadingProgress = static_cast<LoadingProgress*>( OpenWindow( WT_LOADINGPROGRESSBKGND, NULL ) ) )
	{
		pLoadingProgress->SetLoadingType( LoadingProgress::LT_POSTER );
		pLoadingProgress->SetProgress( flProgress );
	}
#endif
}

//=============================================================================
void CBaseModPanel::OnLevelLoadingFinished( KeyValues *kvEvent )
{
	int bError = kvEvent->GetInt( "error" );
	const char *failureReason = kvEvent->GetString( "reason" );
	
	Assert( m_LevelLoading );

	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CBaseModPanel::OnLevelLoadingFinished( %s, %s )\n", bError ? "Had Error" : "No Error", failureReason );
	}

#if defined( _GAMECONSOLE )
	if ( GameUI().IsInBackgroundLevel() )
	{
		// start the installer when running the background map has finished
		g_pXboxInstaller->Start();
	}
#endif

	// Let the ui nuggets know
	IGameUIScreenControllerFactory *pFactory = g_pGameUISystemMgr->GetScreenControllerFactory( "loadingprogress" );
	if ( pFactory && pFactory->GetControllerInstancesCount() )
	{
		KeyValues *kvEvent = new KeyValues( "OnLevelLoadingProgress" );
		KeyValues::AutoDelete autodelete_kvEvent( kvEvent );
		kvEvent->SetFloat( "progress", 1.0f );

		for ( int j = 0; j < pFactory->GetControllerInstancesCount(); ++ j )
		{
			pFactory->GetControllerInstance(j)->BroadcastEventToScreens( kvEvent );
		}
	}

	// Close all embedded gameui screens
	SetGameUiEmbeddedScreen( NULL );

	m_LevelLoading = false;

	// - CBaseModFrame *pFrame = CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION );
	// - if ( !pFrame )
	{
		// no confirmation up, hide the UI
		GameUI().HideGameUI();
	}

#if 0 // TODO: UI: handle errors after loading
	// if we are loading into the lobby, then skip the UIActivation code path
	// this can happen if we accepted an invite to player who is in the lobby while we were in-game
	if ( WT_GAMELOBBY != GetActiveWindowType() )
	{
		// if we are loading into the front-end, then activate the main menu (or attract screen, depending on state)
		// or if a message box is pending force open game ui
		if ( GameUI().IsInBackgroundLevel() || pFrame )
		{
			GameUI().OnGameUIActivated();
		}
	}

	if ( bError )
	{
		GenericConfirmation* pMsg = ( GenericConfirmation* ) OpenWindow( WT_GENERICCONFIRMATION, NULL, false );		
		if ( pMsg )
		{
			GenericConfirmation::Data_t data;
			data.pWindowTitle = "#L4D360UI_MsgBx_DisconnectedFromServer";			
			data.bOkButtonEnabled = true;
			data.pMessageText = failureReason;
			pMsg->SetUsageData( data );
		}		
	}
#endif
}

class CMatchSessionCreationAsyncOperation : public IMatchAsyncOperation
{
public:
	CMatchSessionCreationAsyncOperation() : m_eState( AOS_RUNNING ) {}

public:
	virtual bool IsFinished() { return false; }
	virtual AsyncOperationState_t GetState() { return m_eState; }
	virtual uint64 GetResult() { return 0ull; }
	virtual void Abort();
	virtual void Release() { Assert( 0 ); } // we are a global object, cannot release

public:
	IMatchAsyncOperation * Prepare() { m_eState = AOS_RUNNING; return this; }

protected:
	AsyncOperationState_t m_eState;
}
g_MatchSessionCreationAsyncOperation;

void CMatchSessionCreationAsyncOperation::Abort()
{
	m_eState = AOS_ABORTING;
	
	Assert( g_pMatchFramework->GetMatchSession() );
	g_pMatchFramework->CloseSession();

#if 0 // TODO: UI: Abort session create
	CBaseModPanel::GetSingleton().CloseAllWindows();
	CBaseModPanel::GetSingleton().OpenFrontScreen();
#endif
}

void CBaseModPanel::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( !Q_stricmp( "OnMatchSessionUpdate", szEvent ) )
	{
		char const *szState = pEvent->GetString( "state", "" );
		if ( !Q_stricmp( "ready", szState ) )
		{
			// Session has finished creating:
			IMatchSession *pSession = g_pMatchFramework->GetMatchSession();
			if ( !pSession )
				return;

			KeyValues *pSettings = pSession->GetSessionSettings();
			if ( !pSettings )
				return;

			char const *szNetwork = pSettings->GetString( "system/network", "" );
			int numLocalPlayers = pSettings->GetInt( "members/numPlayers", 1 );
			
			// TODO: UI: session has been created!
			// - WINDOW_TYPE wtGameLobby = WT_GAMELOBBY;
			if ( !Q_stricmp( "offline", szNetwork ) &&
				 numLocalPlayers <= 1 )
			{
				// We have a single-player offline session
				// - wtGameLobby = WT_GAMESETTINGS;
			}

			// We have created a session
			// - CloseAllWindows();

			// Special case when we are creating a public session after empty search
			if ( !Q_stricmp( pSettings->GetString( "options/createreason" ), "searchempty" ) &&
				 !Q_stricmp( pSettings->GetString( "system/access" ), "public" ) )
			{
				// We are creating a public lobby after our search turned out empty
				char const *szWaitScreenText = "#Matchmaking_NoResultsCreating";
				REFERENCE(szWaitScreenText);
				// - CUIGameData::Get()->OpenWaitScreen( szWaitScreenText, ui_lobby_noresults_create_msg_time.GetFloat() );
				// - CUIGameData::Get()->CloseWaitScreen( NULL, NULL );

				// Delete the "createreason" key from the session settings
				pSession->UpdateSessionSettings( KeyValues::AutoDeleteInline( KeyValues::FromString( "delete",
					" delete { options { createreason delete } } " ) ) );
			}

			// - CBaseModFrame *pLobbyWindow = OpenWindow( wtGameLobby, NULL, true, pSettings ); // derive from session
			// - if ( CBaseModFrame *pWaitScreen = GetWindow( WT_GENERICWAITSCREEN ) )
			{
				// Normally "CloseAllWindows" above would take down the waitscreen, but
				// we could pop it up for the special case of empty search results
				// - pWaitScreen->SetNavBack( pLobbyWindow );
			}

			// Check for a special case when we lost connection to host and that's why we are going to lobby
			if ( KeyValues *pOnEngineDisconnectReason = g_pMatchFramework->GetEventsSubscription()->GetEventData( "OnEngineDisconnectReason" ) )
			{
				if ( !Q_stricmp( "lobby", pOnEngineDisconnectReason->GetString( "disconnecthdlr" ) ) )
				{
					// - CUIGameData::Get()->OpenWaitScreen( "#L4D360UI_MsgBx_DisconnectedFromServer" );
					// - CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
				}
			}
		}
		else if ( !Q_stricmp( "created", szState ) )
		{
			//
			// This section of code catches when we just connected to a lobby that
			// is playing a campaign that we do not have installed.
			// In this case we abort loading, forcefully close all windows including
			// loading poster and game lobby and display the download info msg.
			//
#if 1 // TODO: UI: connected to dlc session
			return;
#else
			IMatchSession *pSession = g_pMatchFramework->GetMatchSession();
			if ( !pSession )
				return;

			KeyValues *pSettings = pSession->GetSessionSettings();

			KeyValues *pInfoMission = NULL;
			KeyValues *pInfoChapter = GetMapInfoRespectingAnyChapter( pSettings, &pInfoMission );

			// If we do not have a valid chapter/mission, then we need to quit
			if ( pInfoChapter && pInfoMission &&
				( !*pInfoMission->GetName() || pInfoMission->GetInt( "version" ) == pSettings->GetInt( "game/missioninfo/version", -1 ) ) )
				return;

			if ( pSettings )
				pSettings = pSettings->MakeCopy();

			engine->ExecuteClientCmd( "disconnect" );
			g_pMatchFramework->CloseSession();

			CloseAllWindows( CLOSE_POLICY_EVEN_MSGS | CLOSE_POLICY_EVEN_LOADING );
			OpenFrontScreen();

			const char *szCampaignWebsite = pSettings->GetString( "game/missioninfo/website", NULL );
			if ( szCampaignWebsite && *szCampaignWebsite )
			{
				OpenWindow( WT_DOWNLOADCAMPAIGN,
					GetWindow( CBaseModPanel::GetSingleton().GetActiveWindowType() ),
					true, pSettings );
			}
			else
			{
				GenericConfirmation::Data_t data;

				data.pWindowTitle = "#L4D360UI_Lobby_MissingContent";
				data.pMessageText = "#L4D360UI_Lobby_MissingContent_Message";
				data.bOkButtonEnabled = true;

				GenericConfirmation* confirmation = 
					static_cast< GenericConfirmation* >( OpenWindow( WT_GENERICCONFIRMATION, NULL, true ) );

				confirmation->SetUsageData(data);
			}
#endif
		}
		else if ( !Q_stricmp( "progress", szState ) )
		{
			struct WaitText_t
			{
				char const *m_szProgress;
				char const *m_szText;
				int m_eCloseAllWindowsFlags;
			};

			// TODO: UI: session create progress
			// - int eDefaultFlags = CLOSE_POLICY_EVEN_MSGS | CLOSE_POLICY_KEEP_BKGND;
			int eDefaultFlags = -1;
			WaitText_t arrWaits[] = {
				{ "creating",	"#Matchmaking_creating",	eDefaultFlags },
				{ "joining",	"#Matchmaking_joining",		eDefaultFlags },
				{ "searching",	"#Matchmaking_searching",	eDefaultFlags },
			};

			char const *szProgress = pEvent->GetString( "progress", "" );
			WaitText_t const *pWaitText = NULL;
			for ( int k = 0; k < ARRAYSIZE( arrWaits ); ++ k )
			{
				if ( !Q_stricmp( arrWaits[k].m_szProgress, szProgress ) )
				{
					pWaitText = &arrWaits[k];
					break;
				}
			}

			// Wait screen options to cancel async process
			KeyValues *pSettings = new KeyValues( "WaitScreen" );
			KeyValues::AutoDelete autodelete_pSettings( pSettings );
			pSettings->SetPtr( "options/asyncoperation", g_MatchSessionCreationAsyncOperation.Prepare() );

			// For PC we don't want to cancel lobby creation
			if ( IsPC() && !Q_stricmp( "creating", szProgress ) )
				pSettings = NULL;

			// Put up a wait screen
			if ( pWaitText )
			{
				if ( pWaitText->m_eCloseAllWindowsFlags != -1 )
				{
					// - CloseAllWindows( pWaitText->m_eCloseAllWindowsFlags );
				}

				char const *szWaitScreenText = pWaitText->m_szText;
				float flMinDisplayTime = 0.0f;
				REFERENCE(flMinDisplayTime);
				if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
				{
					KeyValues *pMatchSettings = pMatchSession->GetSessionSettings();
					if ( !Q_stricmp( szProgress, "creating" ) &&
						 !Q_stricmp( pMatchSettings->GetString( "options/createreason" ), "searchempty" ) &&
						 !Q_stricmp( pMatchSettings->GetString( "system/access" ), "public" ) )
					{
						// We are creating a public lobby after our search turned out empty
						szWaitScreenText = "#Matchmaking_NoResultsCreating";
					}
				}

				// - CUIGameData::Get()->OpenWaitScreen( szWaitScreenText, flMinDisplayTime, pSettings );
			}
			else if ( !Q_stricmp( "searchresult", szProgress ) )
			{
				char const *arrText[] = { "#Matchmaking_SearchResults",
					"#Matchmaking_SearchResults1", "#Matchmaking_SearchResults2", "#Matchmaking_SearchResults3" };
				int numResults = pEvent->GetInt( "numResults", 0 );
				if ( numResults < 0 || numResults >= ARRAYSIZE( arrText ) )
					numResults = 0;
				// TODO: UI: session search progress
				// - CUIGameData::Get()->OpenWaitScreen( arrText[numResults], 0.0f, pSettings );
			}
		}
	}
	else if ( !Q_stricmp( "OnEngineLevelLoadingSession", szEvent ) )
	{
		OnEngineLevelLoadingSession( pEvent );
	}
	else if ( !Q_stricmp( "OnEngineLevelLoadingFinished", szEvent ) )
	{
		OnLevelLoadingFinished( pEvent );
	}
}

//=============================================================================
bool CBaseModPanel::UpdateProgressBar( float progress, const char *statusText )
{
	if ( !m_LevelLoading )
	{
		// Warning( "WARN: CBaseModPanel::UpdateProgressBar called outside of level loading, discarded!\n" );
		return false;
	}

	// Need to call this periodically to collect sign in and sign out notifications,
	// do NOT dispatch events here in the middle of loading and rendering!
	if ( ThreadInMainThread() )
	{
		XBX_ProcessEvents();
	}

	IGameUIScreenControllerFactory *pFactory = g_pGameUISystemMgr->GetScreenControllerFactory( "loadingprogress" );
	if ( pFactory && pFactory->GetControllerInstancesCount() )
	{
		KeyValues *kvEvent = new KeyValues( "OnLevelLoadingProgress" );
		KeyValues::AutoDelete autodelete_kvEvent( kvEvent );
		kvEvent->SetFloat( "progress", progress );

		for ( int j = 0; j < pFactory->GetControllerInstancesCount(); ++ j )
		{
			pFactory->GetControllerInstance(j)->BroadcastEventToScreens( kvEvent );
		}
	}

	// update required
	return true;
}

void CBaseModPanel::SetLastActiveUserId( int userId )
{
	if ( m_lastActiveUserId != userId )
	{
		DevWarning( "SetLastActiveUserId: %d -> %d\n", m_lastActiveUserId, userId );
	}

	m_lastActiveUserId = userId;
}

int CBaseModPanel::GetLastActiveUserId( )
{
	return m_lastActiveUserId;
}

//-----------------------------------------------------------------------------
// Purpose: moves the game menu button to the right place on the taskbar
//-----------------------------------------------------------------------------
static void BaseUI_PositionDialog(vgui::PHandle dlg)
{
	if (!dlg.Get())
		return;

	int x, y, ww, wt, wide, tall;
	vgui::surface()->GetWorkspaceBounds( x, y, ww, wt );
	dlg->GetSize(wide, tall);

	// Center it, keeping requested size
	dlg->SetPos(x + ((ww - wide) / 2), y + ((wt - tall) / 2));
}

//=============================================================================
void CBaseModPanel::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetBgColor(pScheme->GetColor("Blank", Color(0, 0, 0, 0)));

	char filename[MAX_PATH];
	engine->GetStartupImage( filename, sizeof( filename ) );
	m_iBackgroundImageID = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile( m_iBackgroundImageID, filename, true, false );

	const AspectRatioInfo_t &aspectRatioInfo = materials->GetAspectRatioInfo();
	bool bIsWidescreen = aspectRatioInfo.m_bIsWidescreen;

	m_iFadeToBackgroundImageID = -1;
	const char *pWhich = V_stristr( filename, "background" );
	if ( pWhich )
	{
		int nWhich = atoi( pWhich + 10 );
		if ( nWhich )
		{
			bool bIsWidescreen = ( V_stristr( pWhich, "widescreen" ) != NULL );
			CFmtStr pFadeFilename( "vgui/maps/background%02d_exit%s", nWhich, ( bIsWidescreen ? "_widescreen" : "" ) );
			m_iFadeToBackgroundImageID = surface()->CreateNewTextureID();
			surface()->DrawSetTextureFile( m_iFadeToBackgroundImageID, pFadeFilename, true, false );
		}
	}

	// Recalculate the movie parameters if our video size has changed
	CalculateMovieParameters();

	bool bUseMono = false;
	bUseMono; // silence warnings on non-demo, PC build
#if defined( _GAMECONSOLE )
	// cannot use the very large stereo version during the install
	 bUseMono = g_pXboxInstaller->IsInstallEnabled() && !g_pXboxInstaller->IsFullyInstalled();
#if defined( _DEMO )
	bUseMono = true;
#endif
#endif
	
	// TODO: GetBackgroundMusic
#if 0
	char backgroundMusic[MAX_PATH];
	engine->GetBackgroundMusic( backgroundMusic, sizeof( backgroundMusic ), bUseMono );

	// the precache will be a memory or stream wave as needed 
	// on 360 the sound system will detect the install state and force it to a memory wave to finalize the the i/o now
	// it will be a stream resource if the installer is dormant
	// On PC it will be a streaming MP3
	if ( enginesound->PrecacheSound( backgroundMusic, true, false ) )
	{
		// successfully precached
		m_backgroundMusic = backgroundMusic;
	}
#endif
}

void CBaseModPanel::DrawColoredText( vgui::HFont hFont, int x, int y, unsigned int color, const char *pAnsiText )
{
	wchar_t szconverted[256];
	int len = g_pVGuiLocalize->ConvertANSIToUnicode( pAnsiText, szconverted, sizeof( szconverted ) );
	if ( len <= 0 )
	{
		return;
	}

	int r = ( color >> 24 ) & 0xFF;
	int g = ( color >> 16 ) & 0xFF;
	int b = ( color >> 8 ) & 0xFF;
	int a = ( color >> 0 ) & 0xFF;

	vgui::surface()->DrawSetTextFont( hFont );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawSetTextColor( r, g, b, a );
	vgui::surface()->DrawPrintText( szconverted, len );
}

void CBaseModPanel::DrawCopyStats()
{
#if defined( _GAMECONSOLE )
	int wide, tall;
	GetSize( wide, tall );

	int xPos = 0.1f * wide;
	int yPos = 0.1f * tall;

	// draw copy status
	char textBuffer[256];
	const CopyStats_t *pCopyStats = g_pXboxInstaller->GetCopyStats();	

	V_snprintf( textBuffer, sizeof( textBuffer ), "Version: %d (%s)", g_pXboxInstaller->GetVersion(), XBX_GetLanguageString() );
	DrawColoredText( m_hDefaultFont, xPos, yPos, 0xffff00ff, textBuffer );
	yPos += 20;

	V_snprintf( textBuffer, sizeof( textBuffer ), "DVD Hosted: %s", g_pFullFileSystem->IsDVDHosted() ? "Enabled" : "Disabled" );
	DrawColoredText( m_hDefaultFont, xPos, yPos, 0xffff00ff, textBuffer );
	yPos += 20;

	bool bDrawProgress = true;
	if ( g_pFullFileSystem->IsInstalledToXboxHDDCache() )
	{
		DrawColoredText( m_hDefaultFont, xPos, yPos, 0x00ff00ff, "Existing Image Found." );
		yPos += 20;
		bDrawProgress = false;
	}
	if ( !g_pXboxInstaller->IsInstallEnabled() )
	{
		DrawColoredText( m_hDefaultFont, xPos, yPos, 0xff0000ff, "Install Disabled." );
		yPos += 20;
		bDrawProgress = false;
	}
	if ( g_pXboxInstaller->IsFullyInstalled() )
	{
		DrawColoredText( m_hDefaultFont, xPos, yPos, 0x00ff00ff, "Install Completed." );
		yPos += 20;
	}

	if ( bDrawProgress )
	{
		yPos += 20;
		V_snprintf( textBuffer, sizeof( textBuffer ), "From: %s (%.2f MB)", pCopyStats->m_srcFilename, (float)pCopyStats->m_ReadSize/(1024.0f*1024.0f) );
		DrawColoredText( m_hDefaultFont, xPos, yPos, 0xffff00ff, textBuffer );
		V_snprintf( textBuffer, sizeof( textBuffer ), "To: %s (%.2f MB)", pCopyStats->m_dstFilename, (float)pCopyStats->m_WriteSize/(1024.0f*1024.0f)  );
		DrawColoredText( m_hDefaultFont, xPos, yPos + 20, 0xffff00ff, textBuffer );

		float elapsed = 0;
		float rate = 0;
		if ( pCopyStats->m_InstallStartTime )
		{
			elapsed = (float)(GetTickCount() - pCopyStats->m_InstallStartTime) * 0.001f;
		}
		if ( pCopyStats->m_InstallStopTime )
		{
			elapsed = (float)(pCopyStats->m_InstallStopTime - pCopyStats->m_InstallStartTime) * 0.001f;
		}
		if ( elapsed )
		{
			rate = pCopyStats->m_TotalWriteSize/elapsed;
		}
		V_snprintf( textBuffer, sizeof( textBuffer ), "Progress: %d/%d MB Elapsed: %d secs (%.2f MB/s)", pCopyStats->m_BytesCopied/(1024*1024), g_pXboxInstaller->GetTotalSize()/(1024*1024), (int)elapsed, rate/(1024.0f*1024.0f) );
		DrawColoredText( m_hDefaultFont, xPos, yPos + 40, 0xffff00ff, textBuffer );
	}
#endif
}

//-----------------------------------------------------------------------------
// Returns true if menu background movie is valid
//-----------------------------------------------------------------------------
bool CBaseModPanel::IsMenuBackgroundMovieValid( void )
{
	if ( !m_bMovieFailed && m_BIKHandle != BIKHANDLE_INVALID )
	{
		return true;
	}

	return false;
}

//=============================================================================
void CBaseModPanel::CalculateMovieParameters( void )
{
	if ( m_BIKHandle == BIKHANDLE_INVALID )
		return;
	
	m_flU0 = m_flV0 = 0.0f;
	g_pBIK->GetTexCoordRange( m_BIKHandle, &m_flU1, &m_flV1 );

	m_pMovieMaterial = g_pBIK->GetMaterial( m_BIKHandle );

	int nWidth, nHeight;
	g_pBIK->GetFrameSize( m_BIKHandle, &nWidth, &nHeight );

	float flFrameRatio = ( (float) GetWide() / (float) GetTall() );
	float flVideoRatio = ( (float) nWidth / (float) nHeight );
	
	if ( flVideoRatio > flFrameRatio )
	{
		// Width must be adjusted
		float flImageWidth = (float) GetTall() * flVideoRatio;
		const float flSpanScaled = ( m_flU1 - m_flU0 ) * GetWide() / flImageWidth;
		m_flU0 = ( m_flU1 - flSpanScaled ) / 2.0f;
		m_flU1 = m_flU0 + flSpanScaled;
	}
	else if ( flVideoRatio < flFrameRatio )
	{
		// Height must be adjusted
		float flImageHeight = (float) GetWide() * ( (float) nHeight / (float) nWidth );
		const float flSpanScaled = ( m_flV1 - m_flV0 ) * GetTall() / flImageHeight;
		m_flV0 = ( m_flV1 - flSpanScaled ) / 2.0f;
		m_flV1 = m_flV0 + flSpanScaled;
	}
}

//=============================================================================
bool CBaseModPanel::InitBackgroundMovie( void )
{
	if ( m_bMovieFailed || m_ExitingFrameCount )
	{
		// prevent constant i/o testing after failure condition
		// do not restart the movie (after its been stopped), we are trying to stabilize the app for exit
		return false;
	}

	if ( CommandLine()->FindParm( "-nomenuvid" ) )
	{
		// mimic movie i/o failure, render will fallback to use product image
		m_bMovieFailed = false;
		return false;
	}

	static bool bFirstTime = true;
	if ( bFirstTime )
	{
		// one time only, on app startup transition from the product image
		m_iMovieTransitionImage = m_iBackgroundImageID;
		bFirstTime = false;
	}
	else
	{
		// otherwise use the blur fade in
		m_iMovieTransitionImage = m_iFadeToBackgroundImageID;
	}

	// Grab our scheme to get the filename from
	IScheme *pScheme = vgui::scheme()->GetIScheme( m_UIScheme );
	if ( pScheme == NULL )
		return false;

	// Destroy any previously allocated video
	if ( m_BIKHandle != BIKHANDLE_INVALID )
	{
		g_pBIK->DestroyMaterial( m_BIKHandle );
		m_BIKHandle = BIKHANDLE_INVALID;
	}

	const char *pFilename;
	char movieFilename[MAX_PATH] = {0};
	pFilename = movieFilename;
	// TODO: engine->GetBackgroundMovie( movieFilename, sizeof( movieFilename ) );
	if ( !g_pFullFileSystem->FileExists( movieFilename, "GAME" ) )
	{
		// bgnd movie not available, fallback and try this one
		pFilename = pScheme->GetResourceString( "BackgroundMovie" );
	}

	COM_TimestampedLog( "Load Background Movie - %s", pFilename );

	// Load and create our BINK video
	// This menu background movie needs to loop and !!reside!! in memory (CRITICAL: Xbox is installing to HDD, w/o this it will frag the drive)
#ifndef _GAMECONSOLE 
	// Address bug caused by searchpath manipulation 
	materials ? materials->UncacheAllMaterials() : NULL;
#endif
	m_BIKHandle = BIKHANDLE_INVALID; // TODO: g_pBIK->CreateMaterial( "VideoBIKMaterial_Background", pFilename, "GAME", BIK_LOOP | BIK_PRELOAD );
	if ( m_BIKHandle == BIKHANDLE_INVALID )
	{
		m_bMovieFailed = true;
		return false;
	}
	
	COM_TimestampedLog( "Load Background Movie - End" );

	// Find frame size and letterboxing information
	CalculateMovieParameters();

	return true;
}

//=============================================================================
void CBaseModPanel::ShutdownBackgroundMovie( void )
{
	if ( m_BIKHandle != BIKHANDLE_INVALID )
	{
		// FIXME: Make sure the m_pMaterial is actually destroyed at this point!
		g_pBIK->DestroyMaterial( m_BIKHandle );
		m_BIKHandle = BIKHANDLE_INVALID;
	}

	// allow a retry
	m_bMovieFailed = false;

	ReleaseBackgroundMusic();
}

//=============================================================================
bool CBaseModPanel::RenderBackgroundMovie( float *pflFadeDelta )
{
	// goes from [0..1]
	// provided to the caller to track the movie fade in
	// callers may have other overlay elements to sync
	*pflFadeDelta = 1.0f;

	if ( IsGameConsole() &&  m_BIKHandle == BIKHANDLE_INVALID )
	{
		// should have already started, cannot be started now
		return false;
	}

	if ( IsPC() )
	{
		// Bring up the video if we haven't before or Alt+Tab has made it invalid
		// The Xbox cannot start the movie at this point, the installer may be using the DVD
		if ( !ActivateBackgroundEffects() )
		{
			return false;
		}
	}

	if ( !m_flMovieFadeInTime )
	{
		// do the fade a little bit after the movie starts (needs to be stable)
		// the product overlay will fade out
		m_flMovieFadeInTime	= Plat_FloatTime() + TRANSITION_TO_MOVIE_DELAY_TIME;
	}

	// There are cases where our texture may never have been rendered (immediately alt+tabbing away on startup).  This check allows us to 
	// recalculate the correct UVs in that case.
	if ( m_flU1 == 0.0f || m_flV1 == 0.0f )
	{
		CalculateMovieParameters();
	}

	// Update our frame, but only if Bink is ready for us to process another frame.
	// We aren't really swapping here, but ReadyForSwap is a good way to throttle.
	// We'd rather throttle this way so that we don't limit the overall frame rate of the system.
	if ( false ) // TODO: if ( g_pBIK->ReadyForSwap( m_BIKHandle ) )
	{
		if ( g_pBIK->Update( m_BIKHandle ) == false )
		{
			// Issue a close command
			ShutdownBackgroundMovie();
			return false;
		}
	}

	// Draw the polys to draw the movie out
	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	IMaterial *pMaterial = g_pBIK->GetMaterial( m_BIKHandle );

	pRenderContext->Bind( pMaterial, NULL );

	CMeshBuilder meshBuilder;
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	float flLeftX = 0;
	float flRightX = GetWide()-1;

	float flTopY = 0;
	float flBottomY = GetTall()-1;

	// Map our UVs to cut out just the portion of the video we're interested in
	float flLeftU = m_flU0;
	float flTopV = m_flV0;

	// We need to subtract off a pixel to make sure we don't bleed
	float flRightU = m_flU1 - ( 1.0f / (float) GetWide() );
	float flBottomV = m_flV1 - ( 1.0f / (float) GetTall() );

	// Get the current viewport size
	int vx, vy, vw, vh;
	pRenderContext->GetViewport( vx, vy, vw, vh );

	// map from screen pixel coords to -1..1
	flRightX = FLerp( -1, 1, 0, vw, flRightX );
	flLeftX = FLerp( -1, 1, 0, vw, flLeftX );
	flTopY = FLerp( 1, -1, 0, vh ,flTopY );
	flBottomY = FLerp( 1, -1, 0, vh, flBottomY );

	for ( int corner=0; corner<4; corner++ )
	{
		bool bLeft = (corner==0) || (corner==3);
		meshBuilder.Position3f( (bLeft) ? flLeftX : flRightX, (corner & 2) ? flBottomY : flTopY, 0.0f );
		meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
		meshBuilder.TexCoord2f( 0, (bLeft) ? flLeftU : flRightU, (corner & 2) ? flBottomV : flTopV );
		meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
		meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
		meshBuilder.Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();

	if ( IsGameConsole() )
	{
		// The product screen has more range than the movie, so the fade works better if the overlay draws after and fades out.
		// Fading the product screen out, modulate alpha [1..0]
		float flFadeDelta = RemapValClamped( Plat_FloatTime(), m_flMovieFadeInTime, m_flMovieFadeInTime + TRANSITION_TO_MOVIE_FADE_TIME, 1.0f, 0.0f );
		if ( flFadeDelta > 0.0f )
		{
			surface()->DrawSetColor( 255, 255, 255, flFadeDelta * 255.0f );
			surface()->DrawSetTexture( m_iMovieTransitionImage );
			surface()->DrawTexturedRect( 0, 0, GetWide(), GetTall() );
		}

		// goes from [0..1]
		flFadeDelta = 1.0f - flFadeDelta;
		if ( m_bFadeMusicUp )
		{
			CBaseModPanel::GetSingleton().UpdateBackgroundMusicVolume( flFadeDelta );
			if ( flFadeDelta >= 1.0f )
			{
				// stop updating
				m_bFadeMusicUp = false;
			}
		}

		*pflFadeDelta = flFadeDelta;
	}

#if defined( ENABLE_BIK_PERF_SPEW ) && ENABLE_BIK_PERF_SPEW
	{
		// timing debug code for bink playback
		static double flPreviousTime = -1.0;
		double flTime = Plat_FloatTime();
		double flDeltaTime = flTime - flPreviousTime;
		if ( flDeltaTime > 0.0 )
		{
			Warning( "%0.2lf sec*60 %0.2lf fps\n", flDeltaTime * 60.0, 1.0 / flDeltaTime );
		}
		flPreviousTime = flTime;
	}
#endif

	return true;
}

//=============================================================================
void CBaseModPanel::PaintBackground()
{
	int wide, tall;
	GetSize( wide, tall );

	if ( !m_LevelLoading &&
		!GameUI().IsInLevel() &&
		!GameUI().IsInBackgroundLevel() )
	{
		if ( engine->IsTransitioningToLoad() )
		{
			// ensure the background is clear
			// the loading progress is about to take over in a few frames
			// this keeps us from flashing a different graphic
			surface()->DrawSetColor( 0, 0, 0, 255 );
			surface()->DrawSetTexture( m_iBackgroundImageID );
			surface()->DrawTexturedRect( 0, 0, wide, tall );
		}
		else
		{
			// Render the background movie
			float flFadeDelta;
			if ( !RenderBackgroundMovie( &flFadeDelta ) )
			{
				// movie failed used product screen as background
				surface()->DrawSetColor( 255, 255, 255, 255 );
				surface()->DrawSetTexture( m_iBackgroundImageID );
				surface()->DrawTexturedRect( 0, 0, wide, tall );
			}
		}
	}

	
	// Update and render the new UI
	if ( g_pGameUIGameSystem )
	{
		Rect_t uiViewport;
		uiViewport.x		= 0;
		uiViewport.y		= 0;
		uiViewport.width	= wide;
		uiViewport.height	= tall;
		g_pGameUISystemMgr->RunFrame();
		// Need to use realtime so animations will play even when paused.
		g_pGameUIGameSystem->Render( uiViewport, gpGlobals->realtime );
	}
	

#if defined( _GAMECONSOLE )
	if ( !m_LevelLoading && !GameUI().IsInLevel() && xbox_install_status.GetBool() )
	{
		DrawCopyStats();
	}
#endif
}

void CBaseModPanel::OnCommand(const char *command)
{
	if ( !Q_stricmp( command, "QuitRestartNoConfirm" ) )
	{
		if ( IsGameConsole() )
		{
			StartExitingProcess( false );
		}
	}
	else if ( !Q_stricmp( command, "RestartWithNewLanguage" ) )
	{
		if ( !IsGameConsole() )
		{
			// TODO: UI: RestartWithNewLanguage
			// - const char *pUpdatedAudioLanguage = Audio::GetUpdatedAudioLanguage();
			const char *pUpdatedAudioLanguage = "english";

			if ( pUpdatedAudioLanguage[ 0 ] != '\0' )
			{
				char szSteamURL[50];
				char szAppId[50];

				// hide everything while we quit
				SetVisible( false );
				vgui::surface()->RestrictPaintToSinglePanel( GetVPanel() );
				engine->ClientCmd_Unrestricted( "quit\n" );

				// Construct Steam URL. Pattern is steam://run/<appid>/<language>. (e.g. Ep1 In French ==> steam://run/380/french)
				Q_strcpy(szSteamURL, "steam://run/");
				itoa( engine->GetAppID(), szAppId, 10 );
				Q_strcat( szSteamURL, szAppId, sizeof( szSteamURL ) );
				Q_strcat( szSteamURL, "/", sizeof( szSteamURL ) );
				Q_strcat( szSteamURL, pUpdatedAudioLanguage, sizeof( szSteamURL ) );

				// Set Steam URL for re-launch in registry. Launcher will check this registry key and exec it in order to re-load the game in the proper language
				vgui::system()->SetRegistryString("HKEY_CURRENT_USER\\Software\\Valve\\Source\\Relaunch URL", szSteamURL );
			}
		}
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

bool CBaseModPanel::RequestInfo( KeyValues *data )
{
	if ( !Q_stricmp( "InputControlState", data->GetName() ) )
	{
		data->SetInt( "passthrough", 1 );
		return true;
	}

	return BaseClass::RequestInfo( data );
}

bool CBaseModPanel::IsReadyToWriteConfig( void )
{
	// For cert we only want to write config files is it has been at least 3 seconds
#ifdef _GAMECONSOLE
	static ConVarRef r_host_write_last_time( "host_write_last_time" );
	return ( Plat_FloatTime() > r_host_write_last_time.GetFloat() + 3.05f );
#endif
	return false;
}

//=============================================================================
// Start system shutdown. Cannot be stopped.
// A Restart is cold restart, plays the intro movie again.
//=============================================================================
void CBaseModPanel::StartExitingProcess( bool bWarmRestart )
{
	if ( !IsGameConsole() )
	{
		// xbox only
		Assert( 0 );
		return;
	}

	if ( m_ExitingFrameCount )
	{
		// already fired
		return;
	}

#if defined( _GAMECONSOLE )
	// signal the installer to stop
	g_pXboxInstaller->Stop();
#endif

	// cold restart or warm
	m_bWarmRestartMode = bWarmRestart;

	// the exiting screen will transition to obscure all the game and UI
	// TODO: UI: - OpenWindow( WT_TRANSITIONSCREEN, 0, false );

	// must let a non trivial number of screen swaps occur to stabilize image
	// ui runs in a constrained state, while shutdown is occurring
	m_ExitingFrameCount = 15;

	// exiting cannot be stopped
	// do not allow any input to occur
	g_pInputSystem->DetachFromWindow();

	// start shutting down systems
	engine->StartXboxExitingProcess();
}

void CBaseModPanel::OnSetFocus()
{
	BaseClass::OnSetFocus();
	if ( IsPC() )
	{
		GameConsole().Hide();
	}
}

void CBaseModPanel::OnMovedPopupToFront()
{
	if ( IsPC() )
	{
		GameConsole().Hide();
	}
}

bool CBaseModPanel::IsBackgroundMusicPlaying()
{
	if ( m_backgroundMusic.IsEmpty() )
		return false;

	if ( m_nBackgroundMusicGUID == 0 )
		return false;
	
	return enginesound->IsSoundStillPlaying( m_nBackgroundMusicGUID );
}

// per Morasky
#define BACKGROUND_MUSIC_DUCK	0.15f

bool CBaseModPanel::StartBackgroundMusic( float fVol )
{
	if ( IsBackgroundMusicPlaying() )
		return true;
	
	if ( m_backgroundMusic.IsEmpty() )
		return false;

	// trying to exit, cannot start it
	if ( m_ExitingFrameCount )
		return false;
	
	m_nBackgroundMusicGUID = 0; // TODO: enginesound->EmitAmbientSound( m_backgroundMusic, BACKGROUND_MUSIC_DUCK * fVol );
	return ( m_nBackgroundMusicGUID != 0 );
}

void CBaseModPanel::UpdateBackgroundMusicVolume( float fVol )
{
	if ( !IsBackgroundMusicPlaying() )
		return;

	// mixes too loud against soft ui sounds
	enginesound->SetVolumeByGuid( m_nBackgroundMusicGUID, BACKGROUND_MUSIC_DUCK * fVol );
}

void CBaseModPanel::ReleaseBackgroundMusic()
{
	if ( m_backgroundMusic.IsEmpty() )
		return;

	if ( m_nBackgroundMusicGUID == 0 )
		return;

	// need to stop the sound now, do not queue the stop
	// we must release the 2-5 MB held by this resource
	// TODO: enginesound->StopSoundByGuid( m_nBackgroundMusicGUID, true );
#if defined( _GAMECONSOLE )
	// TODO: enginesound->UnloadSound( m_backgroundMusic );
#endif

	m_nBackgroundMusicGUID = 0;
}

void CBaseModPanel::SafeNavigateTo( Panel *pExpectedFrom, Panel *pDesiredTo, bool bAllowStealFocus )
{
	Panel *pOriginalFocus = ipanel()->GetPanel( GetCurrentKeyFocus(), GetModuleName() );
	bool bSomeoneElseHasFocus = pOriginalFocus && (pOriginalFocus != pExpectedFrom);
	bool bActuallyChangingFocus = (pExpectedFrom != pDesiredTo);
	bool bNeedToReturnKeyFocus = !bAllowStealFocus && bSomeoneElseHasFocus && bActuallyChangingFocus;

	pDesiredTo->NavigateTo();

	if ( bNeedToReturnKeyFocus )
	{
		pDesiredTo->NavigateFrom();
		pOriginalFocus->NavigateTo();
	}
}

