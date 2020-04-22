//===== Copyright  1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Main panel for CS:GO UI
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "cstrike15basepanel.h"

#include "engineinterface.h"
#include "steam/steam_api.h"
#include "vguisystemmoduleloader.h"

#include "gameui_interface.h"
#include "uigamedata.h"
#include "cdll_client_int.h"

#include "vgui/ILocalize.h"

#if defined(INCLUDE_SCALEFORM)
#include "createstartscreen_scaleform.h" 
#include "createlegalanim_scaleform.h" 
#include "createmainmenuscreen_scaleform.h"
#include "messagebox_scaleform.h"
#include "options_scaleform.h"
#include "motion_calibration_scaleform.h"
#include "howtoplaydialog_scaleform.h"
#include "medalstatsdialog_scaleform.h"
#include "leaderboardsdialog_scaleform.h"
#include "overwatchresolution_scaleform.h"
#include "HUD/sfhudcallvotepanel.h"
#include "upsell_scaleform.h"
#include "splitscreensignon.h"
#include "messagebox_scaleform.h"
#include "itempickup_scaleform.h"
#endif

#if defined ( _PS3 )

#include "ps3/saverestore_ps3_api_ui.h"
#include "sysutil/sysutil_savedata.h"
#include "sysutil/sysutil_gamecontent.h"
#include "cell/sysmodule.h"
static int s_nPs3SaveStorageSizeKB = 5*1024;
static int s_nPs3TrophyStorageSizeKB = 0;

#include "steamoverlay/isteamoverlaymgr.h"

#endif


#include "cdll_util.h"
#include "c_baseplayer.h"
#include "c_cs_player.h"
#include "inputsystem/iinputsystem.h"

#include "cstrike15_gcmessages.pb.h"

#if defined( _X360 )
#include "xparty.h" // For displaying the Party Voice -> Game Voice notification, per requirements
#include "xbox/xbox_launch.h"
#endif

#include "cs_gamerules.h"
#include "clientmode_csnormal.h"
#include "c_cs_playerresource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CCStrike15BasePanel *g_pCStrike15BasePanel = NULL;

// [jason] For tracking the last team played by the main user (TEAM_CT by default)
ConVar player_teamplayedlast( "player_teamplayedlast", "3", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS );
// for tracking whether the player dismissed the community server warning message
ConVar player_nevershow_communityservermessage( "player_nevershow_communityservermessage", "0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS );

static bool s_bSteamOverlayPositionNeedsToBeSet = true;
static void FnSteamOverlayChangeCallback( IConVar *var, const char *pOldValue, float flOldValue )
{
	s_bSteamOverlayPositionNeedsToBeSet = true;
}
ConVar ui_steam_overlay_notification_position( "ui_steam_overlay_notification_position", "topleft", FCVAR_ARCHIVE, "Steam overlay notification position", FnSteamOverlayChangeCallback );

float g_flReadyToCheckForPCBootInvite = 0;
static ConVar connect_lobby( "connect_lobby", "", FCVAR_HIDDEN, "Sets the lobby ID to connect to on start." );


#ifdef _PS3

static CPS3SaveRestoreAsyncStatus s_PS3SaveAsyncStatus;
enum SaveInitializeState_t
{
	SIS_DEFAULT,
	SIS_INIT_REQUESTED,
	SIS_FINISHED
};
static SaveInitializeState_t s_ePS3SaveInitState;

#endif

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CBaseModPanel *BasePanel()
{
	return g_pCStrike15BasePanel;
}

//-----------------------------------------------------------------------------
// Purpose: singleton accessor (constructs)
//-----------------------------------------------------------------------------
CBaseModPanel *BasePanelSingleton()
{
	if ( !g_pCStrike15BasePanel )
	{
		g_pCStrike15BasePanel = new CCStrike15BasePanel();
	}
	return g_pCStrike15BasePanel;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CCStrike15BasePanel::CCStrike15BasePanel() :
	BaseClass( "CStrike15BasePanel" ),
	m_pSplitScreenSignon( NULL ),
	m_OnClosedCommand( ON_CLOSED_NULL ),
	m_bMigratingActive( false ),
	m_bShowRequiredGameVoiceChannelUI( false ),
    m_bNeedToStartIntroMovie( true ),
    m_bTestedStaticIntroMovieDependencies( false ),
	m_bStartLogoIsShowing( false ),
	m_bServerBrowserWarningRaised( false ),
	m_bCommunityQuickPlayWarningRaised( false ),
	m_bCommunityServerWarningIssued( false ),
	m_bGameIsShuttingDown( false )
{
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

#if defined ( _X360 )
	if ( xboxsystem )
		xboxsystem->UpdateArcadeTitleUnlockStatus();
#endif
}

//-----------------------------------------------------------------------------
CCStrike15BasePanel::~CCStrike15BasePanel()
{
	m_bGameIsShuttingDown = true;

	StopListeningForAllEvents();

	// [jason] Release any screens that may still be active on shutdown so we don't leak memory
#if defined(INCLUDE_SCALEFORM)
	DismissAllMainMenuScreens();
#endif
}

#if defined( _X360 )
void CCStrike15BasePanel::Xbox_PromptSwitchToGameVoiceChannel( void )
{
	// Clear this flag by default - only enable it if we actually raise the UI
	m_bShowRequiredGameVoiceChannelUI = false;

	// We must be in an Online game in order to proceed
	bool bOnline = g_pMatchFramework && g_pMatchFramework->IsOnlineGame();

	if ( bOnline && Xbox_IsPartyChatEnabled() )
	{
		if ( BaseModUI::CUIGameData::Get()->IsXUIOpen() )
		{
			// Wait for the XUI to go away before prompting for this
			m_GameVoiceChannelRecheckTimer.Start( 1.f );
		}
		else
		{
			m_bShowRequiredGameVoiceChannelUI = true;
			XShowRequiredGameVoiceChannelUI();	
		}
	}
}

bool CCStrike15BasePanel::Xbox_IsPartyChatEnabled( void )
{
	XPARTY_USER_LIST xpUserList;
	if  ( XPartyGetUserList( &xpUserList ) != XPARTY_E_NOT_IN_PARTY )
	{
		for ( DWORD idx = 0; idx < xpUserList.dwUserCount; ++idx )
		{
			// Detect if the local user is in the Party, and is using Party Voice channel
			if ( xpUserList.Users[idx].dwFlags & ( XPARTY_USER_ISLOCAL | XPARTY_USER_ISINPARTYVOICE ) )
			{
				return true;
			}
		}
	}
	return false;
}
#endif

void CCStrike15BasePanel::OnEvent( KeyValues *pEvent )
{
	const char *pEventName = pEvent->GetName();

#if defined( _X360 )
	//
	// Handler for switching the Xbox LIVE voice channel (Game vs Party chat channels)
	if ( !Q_strcmp( pEventName, "OnLiveVoicechatAway" ) )
	{
		bool bNotTitleChat = ( pEvent->GetInt( "NotTitleChat", 0 ) != 0 );

		// If we switched to non-Game Chat, then prompt the user to switch back if he's in an Online game
		if ( bNotTitleChat )
		{
			Xbox_PromptSwitchToGameVoiceChannel();
		}
		else
		{
			m_bShowRequiredGameVoiceChannelUI = false;
			m_GameVoiceChannelRecheckTimer.Invalidate();
		}
	}

	// Detect the next system XUI closed event
	if ( !Q_stricmp( pEventName, "OnSysXUIEvent" ) 	&&
		 !Q_stricmp( "closed", pEvent->GetString( "action", "" ) ) )
	{
		if ( m_bShowRequiredGameVoiceChannelUI )
		{
			// If it was the game voice channel UI that closed, wait a few ticks to see if Game Chat is re-enabled
			m_GameVoiceChannelRecheckTimer.Start( 1.0f );
		}		
	}
#endif

	if ( !Q_strcmp( pEventName, "OnSysSigninChange" ) )
	{
		if ( !Q_stricmp( "signout", pEvent->GetString( "action", "" ) ) )
		{
			int primaryID = pEvent->GetInt( "user0", -1 );

			ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
			if ( XBX_GetSlotByUserId( primaryID ) != -1 )
			{
				// go to the start screen
				// $TODO ?? display message stating you got booted back to start cuz you signed out
				if ( GameUI().IsInLevel() ) 
				{
					m_bForceStartScreen = true;
					engine->ClientCmd_Unrestricted( "disconnect" );
				}
				else
				{
					HandleOpenCreateStartScreen();			
				}
			}
		}
		if ( !Q_stricmp( "signin", pEvent->GetString( "action", "" ) ) )
		{
#if defined ( _X360 )
			xboxsystem->UpdateArcadeTitleUnlockStatus();
#endif
		}

		UpdateRichPresenceInfo();
	}
#if defined( _X360 )
	else if ( !Q_stricmp( pEventName, "OnEngineDisconnectReason" ) )
	{
		if ( char const *szDisconnectHdlr = pEvent->GetString( "disconnecthdlr", NULL ) )
		{
			if ( !Q_stricmp( szDisconnectHdlr, "lobby" ) )
			{
				// Make sure the main menu is hidden
				CCreateMainMenuScreenScaleform::ShowPanel( false, true );

				// Flag the main menu to stay hidden during migration, unless we cancel
				m_bMigratingActive = true;

				OnOpenMessageBox( "#SFUI_MainMenu_MigrateHost_Title", "#SFUI_MainMenu_MigrateHost_Message", "#SFUI_MainMenu_MigrateHost_Navigation", (MESSAGEBOX_FLAG_CANCEL | MESSAGEBOX_FLAG_BOX_CLOSED), this );
			}
		}
		else
		{
			// Clear the migrating flag in all other disconnect cases
			m_bMigratingActive = false;

			// Clear the flags governing game voice channel for Xbox when we disconnect as well
			m_bShowRequiredGameVoiceChannelUI = false;
			m_GameVoiceChannelRecheckTimer.Invalidate();
		}
	}
	else if ( !Q_stricmp( pEventName, "OnEngineLevelLoadingStarted" ) || !V_stricmp( pEventName, "LoadingScreenOpened" ) )
	{
		// Clear the migrating flag once level loading starts
		m_bMigratingActive = false;
	}
#endif // _X360
	else if ( !Q_stricmp( pEventName, "OnDemoFileEndReached" ) )
	{
		if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
		{
			if ( pParameters->m_bAnonymousPlayerIdentity &&
				pParameters->m_uiLockFirstPersonAccountID &&
				pParameters->m_uiCaseID )
			{
				SFHudOverwatchResolutionPanel::LoadDialog();
			}
		}
		engine->ClientCmd_Unrestricted( "disconnect" );
	}
}

void CCStrike15BasePanel::FireGameEvent( IGameEvent *event )
{
	const char *type = event->GetName();
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( StringHasPrefix( type, "player_team" ) )
	{
		if ( pLocalPlayer )
		{
			int newTeam = event->GetInt( "team" );
			int userId = event->GetInt( "userid" );

			if ( pLocalPlayer->GetUserID() == userId )
			{
				if ( newTeam == TEAM_CT || newTeam == TEAM_TERRORIST )
					player_teamplayedlast.SetValue( newTeam );
			}
		}
	}
	else if ( StringHasPrefix( type, "cs_game_disconnected" ) )
	{
		if ( GameUI().IsInLevel() )
		{
			// Ensure we remove any pending dialogs as soon as we receive notification that the client is disconnecting
			//	(fixes issue with the quit dialog staying up when you "disconnect" via console window)
			//  Passing in false to indicate we do not wish to dismiss CCommandMsgBoxes, which indicate error codes/kick reasons/etc
			CMessageBoxScaleform::UnloadAllDialogs( false );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
// Avatars conversion to rgb
//
#include "imageutils.h"
CON_COMMAND_F( cl_avatar_convert_rgb, "Converts all png avatars in the avatars directory to rgb", FCVAR_RELEASE | FCVAR_CHEAT )
{
	FileFindHandle_t hFind = NULL;
	for ( char const *szFileName = g_pFullFileSystem->FindFirst( "avatars/*.png", &hFind );
		szFileName && *szFileName; szFileName = g_pFullFileSystem->FindNext( hFind ) )
	{
		CFmtStr sFile( "avatars/%s", szFileName );
		char chRgbFile[MAX_PATH] = {};
		V_strcpy_safe( chRgbFile, sFile );
		V_SetExtension( chRgbFile, ".rgb", MAX_PATH );

		int width = 0, height = 0;
		ConversionErrorType cet = CE_ERROR_WRITING_OUTPUT_FILE;
		unsigned char *pbImgData = ImgUtl_ReadPNGAsRGBA( sFile, width, height, cet );

		if ( ( cet == CE_SUCCESS ) && ( width == 64 ) && ( height == 64 ) && pbImgData )
		{
			// trim alpha for size
			for ( int y = 0; y < 64; ++y ) for ( int x = 0; x < 64; ++x )
			{
				V_memmove( pbImgData + y * 64 * 3 + x * 3, pbImgData + y * 64 * 4 + x * 4, 3 );
			}
			CUtlBuffer bufWriteExternal( pbImgData, 64*64*3, CUtlBuffer::READ_ONLY );
			bufWriteExternal.SeekPut( CUtlBuffer::SEEK_HEAD, 64*64*3 );
			if ( g_pFullFileSystem->WriteFile( chRgbFile, "MOD", bufWriteExternal ) )
			{
				Msg( "Converted rgb '%s'->'%s'.\n", sFile.Access(), chRgbFile );
			}
			else
			{
				Warning( "Failed to save converted rgb '%s'->'%s'.\n", sFile.Access(), chRgbFile );
			}
		}
		else
		{
			Warning( "Invalid conversion source '%s' (%d/%p; %dx%d), expecting 64x64 PNG.\n", sFile.Access(), cet, pbImgData, width, height );
		}
		
		if ( pbImgData )
		{
			free( pbImgData );
		}
	}
	g_pFullFileSystem->FindClose( hFind );
}


#if defined(INCLUDE_SCALEFORM)

void CCStrike15BasePanel::OnOpenCreateStartScreen( void )
{
	CCreateStartScreenScaleform::LoadDialog( );

	m_bStartLogoIsShowing = false;

	// If the player has already signed in, start loading the player stats stuff here.
#if !defined(NO_STEAM) && defined(_PS3)
	PerformPS3GameBootWork();
#endif

}

void CCStrike15BasePanel::DismissStartScreen()
{
	CCreateStartScreenScaleform::UnloadDialog( );
}

bool CCStrike15BasePanel::IsStartScreenActive()
{
	return CCreateStartScreenScaleform::IsActive();
}

void CCStrike15BasePanel::OnOpenCreateMainMenuScreen( void )
{
	// We cannot call this from the constructor because the gameeventmgr is not constructed yet;
	//	so instead, we call it when we load the main menu - this does get hit every time we load the main menu
	//	but it has no effect after the first time it is called.
	ListenForGameEvent( "player_team" );
	ListenForGameEvent( "cs_game_disconnected" );

	if ( IsScaleformMainMenuEnabled() )
	{
		// Disable splitscreen sign in prompt
		//m_pSplitScreenSignon = new SplitScreenSignonWidget();
		CCreateMainMenuScreenScaleform::LoadDialog( );
	}
	else
	{
		CBaseModPanel::OnOpenCreateMainMenuScreen();
	}
}

void CCStrike15BasePanel::DismissMainMenuScreen( void )
{
	// Allow the dismiss to proceed even if IsScaleformMainMenuEnabled is false, in case we toggle it at run-time
	CCreateMainMenuScreenScaleform::UnloadDialog( );
}

void CCStrike15BasePanel::DismissAllMainMenuScreens( bool bHideMainMenuOnly )
{
	// Either hide the menus, or tear them down, depending on the context
	if ( bHideMainMenuOnly )
	{
		if ( CCStrike15BasePanel::IsScaleformMainMenuActive() )
			CCreateMainMenuScreenScaleform::ShowPanel( false, true );
	}
	else
	{
		CCStrike15BasePanel::DismissMainMenuScreen();
		CCStrike15BasePanel::DismissPauseMenu();
	}

	// Close all menu screens that may have been opened from main or pause menu

	DismissScaleformIntroMovie();

	CBaseModPanel::OnCloseServerBrowser();

	CCreateStartScreenScaleform::UnloadDialog();

	CCreateMedalStatsDialogScaleform::UnloadDialog();

	CCreateLeaderboardsDialogScaleform::UnloadDialog();

	COptionsScaleform::UnloadDialog();

	CUpsellScaleform::UnloadDialog();

	CHowToPlayDialogScaleform::UnloadDialog();

	CMessageBoxScaleform::UnloadAllDialogs();

	SFHudOverwatchResolutionPanel::UnloadDialog();
}

void CCStrike15BasePanel::RestoreMainMenuScreen( void )
{
	if ( IsX360() && m_bMigratingActive )
		return;

	if ( m_bGameIsShuttingDown )
		return;

	// If we're part of a team lobby, returning to main menu should take us directly to the lobby screen
	if ( InTeamLobby() )
	{
		OnOpenCreateLobbyScreen();
	}
	else if ( IsScaleformMainMenuEnabled() )
	{
		CCreateMainMenuScreenScaleform::RestorePanel();
	}
}

void CCStrike15BasePanel::RestoreMPGameMenu( void )
{
	DismissPauseMenu();
	engine->ClientCmd_Unrestricted( "disconnect" );
	OnOpenCreateMultiplayerGameDialog();
	m_bReturnToMPGameMenuOnDisconnect = false;
}

void CCStrike15BasePanel::ShowScaleformMainMenu( bool bShow )
{
	if ( bShow )
	{
		// Do not allow the screen to display when it has been disabled
		if ( !IsScaleformMainMenuEnabled() )
			return;

		if ( IsX360() && m_bMigratingActive )
			return;
	}

	CCreateMainMenuScreenScaleform::ShowPanel( bShow );
}

bool CCStrike15BasePanel::IsScaleformMainMenuActive( void )
{
	return CCreateMainMenuScreenScaleform::IsActive();
}

void CCStrike15BasePanel::OnOpenCreateSingleplayerGameDialog( bool bMatchmakingFilter )
{
	/* Removed for partner depot */
}

void CCStrike15BasePanel::OnOpenCreateMultiplayerGameDialog( void )
{
	if (IsPC() && !IsScaleformMainMenuEnabled())
	{
		// Continue to support the vgui create server dialog
		CBaseModPanel::OnOpenCreateMultiplayerGameDialog();
	}
}

void CCStrike15BasePanel::OnOpenCreateMultiplayerGameCommunity( void )
{
	// disable the warning
	m_bCommunityQuickPlayWarningRaised = true;
	if ( !m_bCommunityServerWarningIssued && player_nevershow_communityservermessage.GetBool() == 0 )
	{
		OnOpenMessageBoxThreeway( "#SFUI_MainMenu_ServerBrowserWarning_Title", "#SFUI_MainMenu_ServerBrowserWarning_Text2", "#SFUI_MainMenu_ServerBrowserWarning_Legend", "#SFUI_MainMenu_ServerBrowserWarning_NeverShow", ( MESSAGEBOX_FLAG_OK  |  MESSAGEBOX_FLAG_CANCEL | MESSAGEBOX_FLAG_TERTIARY ), this );	
		m_bCommunityQuickPlayWarningRaised = true;
	}
	else
	{
		DoCommunityQuickPlay();
	}
}


void CCStrike15BasePanel::DoCommunityQuickPlay( void )
{
	/* Removed for partner depot */
	return;
}

void CCStrike15BasePanel::OnOpenServerBrowser()
{
#if !defined(_GAMECONSOLE)
	if ( !m_bCommunityServerWarningIssued && player_nevershow_communityservermessage.GetBool() == 0 )
	{
		OnOpenMessageBoxThreeway( "#SFUI_MainMenu_ServerBrowserWarning_Title", "#SFUI_MainMenu_ServerBrowserWarning_Text2", "#SFUI_MainMenu_ServerBrowserWarning_Legend", "#SFUI_MainMenu_ServerBrowserWarning_NeverShow", ( MESSAGEBOX_FLAG_OK  |  MESSAGEBOX_FLAG_CANCEL | MESSAGEBOX_FLAG_TERTIARY ), this );	
		m_bServerBrowserWarningRaised = true;
	}
	else
	{
		g_VModuleLoader.ActivateModule("Servers");
	}
#endif
}

void CCStrike15BasePanel::OnOpenCreateLobbyScreen( bool bIsHost )
{
	/* Removed for partner depot */
}

void CCStrike15BasePanel::OnOpenLobbyBrowserScreen( bool bIsHost )
{
	/* Removed for partner depot */
}

void CCStrike15BasePanel::UpdateLobbyScreen( )
{
	/* Removed for partner depot */
}

void CCStrike15BasePanel::UpdateMainMenuScreen()
{
	CCreateMainMenuScreenScaleform::UpdateDialog();
}

void CCStrike15BasePanel::UpdateLobbyBrowser( )
{
	/* Removed for partner depot */
}

void CCStrike15BasePanel::OnOpenMessageBox( char const *pszTitle, char const *pszMessage, char const *pszButtonLegend, DWORD dwFlags, IMessageBoxEventCallback *pEventCallback, CMessageBoxScaleform** ppInstance, wchar_t const *pszWideMessage )
{
	CMessageBoxScaleform::LoadDialog( pszTitle, pszMessage, pszButtonLegend, dwFlags, pEventCallback, ppInstance, pszWideMessage );
}

void CCStrike15BasePanel::OnOpenMessageBoxInSlot( int slot, char const *pszTitle, char const *pszMessage, char const *pszButtonLegend, DWORD dwFlags, IMessageBoxEventCallback *pEventCallback, CMessageBoxScaleform** ppInstance )
{
	CMessageBoxScaleform::LoadDialogInSlot( slot, pszTitle, pszMessage, pszButtonLegend, dwFlags, pEventCallback, ppInstance );
}

void CCStrike15BasePanel::OnOpenMessageBoxThreeway( char const *pszTitle, char const *pszMessage, char const *pszButtonLegend, char const *pszThirdButtonLabel, DWORD dwFlags, IMessageBoxEventCallback *pEventCallback, CMessageBoxScaleform** ppInstance )
{
	CMessageBoxScaleform::LoadDialogThreeway( pszTitle, pszMessage, pszButtonLegend, pszThirdButtonLabel, dwFlags, pEventCallback, ppInstance );
}

void CCStrike15BasePanel::CreateCommandMsgBox( const char* pszTitle, const char* pszMessage, bool showOk, bool showCancel, const char* okCommand, const char* cancelCommand, const char* closedCommand, const char* pszLegend )
{
	CCommandMsgBox::CreateAndShow( pszTitle, pszMessage, showOk, showCancel, okCommand, cancelCommand, closedCommand, pszLegend );
}

void CCStrike15BasePanel::CreateCommandMsgBoxInSlot( ECommandMsgBoxSlot slot, const char* pszTitle, const char* pszMessage, bool showOk, bool showCancel, const char* okCommand, const char* cancelCommand, const char* closedCommand, const char* pszLegend )
{
	CCommandMsgBox::CreateAndShowInSlot( slot, pszTitle, pszMessage, showOk, showCancel, okCommand, cancelCommand, closedCommand, pszLegend );
}


void CCStrike15BasePanel::ShowMatchmakingStatus( void )
{
	// MatchmakingStatus will auto delete itself when the message box is dismissed/torn down.
	new CMatchmakingStatus();
}

extern ConVar devCheatSkipInputLocking;

bool CCStrike15BasePanel::ShowLockInput( void )
{
#if defined ( _X360 )
	return false;
#endif
	if ( devCheatSkipInputLocking.GetBool() )
		return false;

#if defined( WIN32 ) 
	if ( g_pScaleformUI )
	{
		g_pScaleformUI->LockMostRecentInputDevice( SF_FULL_SCREEN_SLOT );
	}
#endif

	// open the message box, but make sure we don't have a selected device and aren't already sampling for a device
	if( g_pInputSystem->GetCurrentInputDevice( ) == INPUT_DEVICE_NONE &&
		!g_pInputSystem->IsSamplingForCurrentDevice( ) )
	{
		// skip the message box and just lock input if theres only one control type enabled
		InputDevice_t singleDeviceConnected = g_pInputSystem->IsOnlySingleDeviceConnected( );

#if defined( _PS3 )

		// under PS3, if no devices are connected, assume gamepad
		if ( !singleDeviceConnected && g_pInputSystem->GetConnectedInputDevices( ) == INPUT_DEVICE_NONE )
		{
			singleDeviceConnected = INPUT_DEVICE_GAMEPAD;
		}

#endif

		if ( singleDeviceConnected != INPUT_DEVICE_NONE )
		{
			g_pInputSystem->SetCurrentInputDevice( singleDeviceConnected );

#if defined( _PS3 )

			// Loads the correct bindings based on which device is active.
			for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
			{
				engine->ExecuteClientCmd( VarArgs( "cl_read_ps3_bindings %d %d", i, (int)singleDeviceConnected ) );
			}

#endif // _PS3

#if defined( WIN32 )
			if( singleDeviceConnected != INPUT_DEVICE_GAMEPAD )
			{
				ConVarRef var( "joystick" );
				if( var.IsValid( ) )
					var.SetValue( 0 );
			}
#endif  

			

			InputDevice_t currentInputDevice = g_pInputSystem->GetCurrentInputDevice();

			bool hasLockedIntoMotionController = currentInputDevice == INPUT_DEVICE_PLAYSTATION_MOVE || 
												 currentInputDevice == INPUT_DEVICE_SHARPSHOOTER;

			if ( hasLockedIntoMotionController )
			{
				new CMessageBoxCalibrateNotification();
			}

			return hasLockedIntoMotionController;
		}
		else
		{
			// will auto delete after message box is dismissed
			new CMessageBoxLockInput( );
		}



		return true;
	}
	
	return false;	
}

void CCStrike15BasePanel::OnOpenPauseMenu( void )
{
	CBaseModPanel::OnOpenPauseMenu();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCStrike15BasePanel::OnOpenMouseDialog()
{
	COptionsScaleform::ShowMenu( true, COptionsScaleform::DIALOG_TYPE_MOUSE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCStrike15BasePanel::OnOpenKeyboardDialog()
{
	COptionsScaleform::ShowMenu( true, COptionsScaleform::DIALOG_TYPE_KEYBOARD );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCStrike15BasePanel::OnOpenControllerDialog( void )
{
	COptionsScaleform::ShowMenu( true, COptionsScaleform::DIALOG_TYPE_CONTROLLER );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCStrike15BasePanel::OnOpenMotionControllerMoveDialog()
{
	COptionsScaleform::ShowMenu( true, COptionsScaleform::DIALOG_TYPE_MOTION_CONTROLLER_MOVE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCStrike15BasePanel::OnOpenMotionControllerSharpshooterDialog()
{
	COptionsScaleform::ShowMenu( true, COptionsScaleform::DIALOG_TYPE_MOTION_CONTROLLER_SHARPSHOOTER );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCStrike15BasePanel::OnOpenMotionControllerDialog()
{
	COptionsScaleform::ShowMenu( true, COptionsScaleform::DIALOG_TYPE_MOTION_CONTROLLER );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCStrike15BasePanel::OnOpenMotionCalibrationDialog()
{
	CMotionCalibrationScaleform::LoadDialog();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCStrike15BasePanel::OnOpenVideoSettingsDialog()
{
	COptionsScaleform::ShowMenu( true, COptionsScaleform::DIALOG_TYPE_VIDEO );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCStrike15BasePanel::OnOpenOptionsQueued()
{
	COptionsScaleform::ShowMenu( true, COptionsScaleform::DIALOG_TYPE_NONE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCStrike15BasePanel::OnOpenAudioSettingsDialog()
{
	COptionsScaleform::ShowMenu( true, COptionsScaleform::DIALOG_TYPE_AUDIO );
}

void CCStrike15BasePanel::OnOpenSettingsDialog( void )
{
	COptionsScaleform::ShowMenu( true, COptionsScaleform::DIALOG_TYPE_SETTINGS );
}

void CCStrike15BasePanel::OnOpenHowToPlayDialog( void )
{
	CHowToPlayDialogScaleform::LoadDialog();
}

void CCStrike15BasePanel::DismissPauseMenu( void )
{
	/* Removed for partner depot */
}

void CCStrike15BasePanel::RestorePauseMenu( void )
{
	/* Removed for partner depot */
}

void CCStrike15BasePanel::ShowScaleformPauseMenu( bool bShow )
{
	/* Removed for partner depot */
}

bool CCStrike15BasePanel::IsScaleformPauseMenuActive( void )
{
	/* Removed for partner depot */
	return false;
}

bool CCStrike15BasePanel::IsScaleformPauseMenuVisible( void )
{
	/* Removed for partner depot */
	return false;
}

void CCStrike15BasePanel::OnOpenDisconnectConfirmationDialog( void )
{	
#if defined( INCLUDE_SCALEFORM )
	char const *szTitle = "#SFUI_PauseMenu_ExitGameConfirmation_Title";
	char const *szMessageDefault = "#SFUI_PauseMenu_ExitGameConfirmation_Message";
	char const *szMessage = szMessageDefault;
	if ( engine->IsHLTV() || engine->IsPlayingDemo() )
	{
		szTitle = "#SFUI_PauseMenu_ExitGameConfirmation_TitleWatch";
		szMessage = "#SFUI_PauseMenu_ExitGameConfirmation_MessageWatch";

		if ( engine->GetDemoPlaybackParameters() && engine->GetDemoPlaybackParameters()->m_bAnonymousPlayerIdentity )
		{
			szTitle = "#SFUI_PauseMenu_ExitGameConfirmation_TitleOverwatch";
			szMessage = "#SFUI_PauseMenu_ExitGameConfirmation_MessageOverwatch";
		}
	}
	else if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() )
	{
		szTitle = "#SFUI_PauseMenu_ExitGameConfirmation_TitleQueuedMatchmaking";
		szMessage = "#SFUI_PauseMenu_ExitGameConfirmation_MessageQueuedMatchmaking";
		
		if ( CSGameRules()->IsPlayingCooperativeGametype() )
		{
			szTitle = "#SFUI_PauseMenu_ExitGameConfirmation_TitleQueuedGuardian";
			szMessage = "#SFUI_PauseMenu_ExitGameConfirmation_MessageQueuedGuardian";
		}
	}

	if ( ( szMessage == szMessageDefault ) && CSGameRules() && CSGameRules()->IsQuestEligible()
		&& ( CSGameRules()->GetGamePhase() != GAMEPHASE_MATCH_ENDED )
		&& !CSGameRules()->IsWarmupPeriod() )
	{
		// See if we have a mission progress?
		bool bMissionProgress = false;
		for ( uint32 i = ClientModeCSNormal::sm_mapQuestProgressUncommitted.FirstInorder();
			i != ClientModeCSNormal::sm_mapQuestProgressUncommitted.InvalidIndex();
			i = ClientModeCSNormal::sm_mapQuestProgressUncommitted.NextInorder( i ) )
		{
			if ( ClientModeCSNormal::sm_mapQuestProgressUncommitted.Element( i ).m_numNormalPoints > 0 )
			{
				bMissionProgress = true;
				break;
			}
		}

		if ( bMissionProgress )
		{
			szMessage = "#SFUI_PauseMenu_ExitGameConfirmation_MessageMission";
		}
		else
		{
			// Check if local user has non-zero score?
			C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
			C_CS_PlayerResource *cs_PR = static_cast< C_CS_PlayerResource * >( g_PR );
			if ( pLocalPlayer && cs_PR )
			{
				if ( cs_PR->GetScore( pLocalPlayer->entindex() ) > 0 )
				{
					szMessage = "#SFUI_PauseMenu_ExitGameConfirmation_MessageXP";
				}
			}
		}
	}

	OnOpenMessageBox( szTitle, szMessage,
		"#SFUI_PauseMenu_ExitGameConfirmation_Navigation", ( MESSAGEBOX_FLAG_OK  |  MESSAGEBOX_FLAG_CANCEL | MESSAGEBOX_FLAG_BOX_CLOSED | MESSAGEBOX_FLAG_AUTO_CLOSE_ON_DISCONNECT ), this );
#else
	BaseClass::OnOpenDisconnectConfirmationDialog();
#endif
}

void CCStrike15BasePanel::OnOpenQuitConfirmationDialog( bool bForceToDesktop )
{	
#if defined( INCLUDE_SCALEFORM )
	m_bForceQuitToDesktopOnDisconnect = bForceToDesktop;
	OnOpenMessageBox( "#SFUI_MainMenu_ExitGameConfirmation_Title", "#SFUI_MainMenu_ExitGameConfirmation_Message", "#SFUI_MainMenu_ExitGameConfirmation_Navigation", ( MESSAGEBOX_FLAG_OK  |  MESSAGEBOX_FLAG_CANCEL | MESSAGEBOX_FLAG_BOX_CLOSED ), this );
#else
	BaseClass::OnOpenDisconnectConfirmationDialog();
#endif
}


bool CCStrike15BasePanel::OnMessageBoxEvent( MessageBoxFlags_t buttonPressed )
{
	//Special handling for the Server Browser prompt. if we go through here, we return early and never hit the rest of the code
	if ( m_bServerBrowserWarningRaised )
	{
		if ( (buttonPressed & MESSAGEBOX_FLAG_OK) || (buttonPressed & MESSAGEBOX_FLAG_TERTIARY) )
		{
			if (buttonPressed & MESSAGEBOX_FLAG_TERTIARY)
			{
				player_nevershow_communityservermessage.SetValue( true );
			}

			m_bCommunityServerWarningIssued = true;
			g_VModuleLoader.ActivateModule("Servers");
		}

		if ( buttonPressed & MESSAGEBOX_FLAG_CANCEL )
		{
			if ( GameUI().IsInLevel() )
			{
				RestorePauseMenu();
			}
			else
			{
				RestoreMainMenuScreen();
			}
		}
		
		m_bServerBrowserWarningRaised = false;
		return true;
	}

	/*
	( ( CCStrike15BasePanel* )BasePanel() )->OnOpenMessageBoxThreeway( "#SFUI_LobbyGameSettings_Title", 
	"#SFUI_LobbyGameSettings_Text", 
	"#SFUI_LobbyGameSettings_Help", 
	"#SFUI_LobbyGameSettings_QMButton",
	( MESSAGEBOX_FLAG_OK  |  MESSAGEBOX_FLAG_CANCEL | MESSAGEBOX_FLAG_TERTIARY ), 
	this, &m_pConfirmDialog );
	*/

	if ( m_bCommunityQuickPlayWarningRaised )
	{
		if ( (buttonPressed & MESSAGEBOX_FLAG_OK) || (buttonPressed & MESSAGEBOX_FLAG_TERTIARY) )
		{
			if (buttonPressed & MESSAGEBOX_FLAG_TERTIARY)
			{
				player_nevershow_communityservermessage.SetValue( true );
			}

			m_bCommunityServerWarningIssued = true;
			DoCommunityQuickPlay();
		}

		if ( buttonPressed & MESSAGEBOX_FLAG_CANCEL )
		{
			if ( GameUI().IsInLevel() )
			{
				RestorePauseMenu();
			}
			else
			{
				RestoreMainMenuScreen();
			}
		}

		m_bCommunityQuickPlayWarningRaised = false;
		return true;
	}


	if ( buttonPressed & MESSAGEBOX_FLAG_OK )
	{
		if ( GameUI().IsInLevel() && !m_bForceQuitToDesktopOnDisconnect )
		{
			// "Exit Game" prompt (disconnect) from within the level
			if ( m_bReturnToMPGameMenuOnDisconnect )
				m_OnClosedCommand = ON_CLOSED_DISCONNECT_TO_MP_GAME_MENU;
			else
				m_OnClosedCommand = ON_CLOSED_DISCONNECT;
		}
		else
		{
			// "Quit game" prompt from main menu
			m_OnClosedCommand = ON_CLOSED_QUIT;
		}
	}

	else if ( buttonPressed & MESSAGEBOX_FLAG_CANCEL )
	{
		// Clear the migrating status, so we can open main menu
		if ( IsX360() && m_bMigratingActive )
		{
			// Close the multiplayer session to abort the server starting
			g_pMatchFramework->CloseSession();

			m_bMigratingActive = false;

			m_OnClosedCommand = ON_CLOSED_RESTORE_MAIN_MENU;
		}
		else if ( GameUI().IsInLevel() )
		{
			m_OnClosedCommand = ON_CLOSED_RESTORE_PAUSE_MENU;
		}
		else
		{
			m_OnClosedCommand = ON_CLOSED_RESTORE_MAIN_MENU;
		}

		m_bReturnToMPGameMenuOnDisconnect = false;
		m_bForceQuitToDesktopOnDisconnect = false;
	}

	else if ( buttonPressed & MESSAGEBOX_FLAG_BOX_CLOSED )
	{
		switch ( m_OnClosedCommand )
		{
			case ON_CLOSED_DISCONNECT:
				{
#if defined( _X360 )
					for ( int i=0; i<XUSER_MAX_COUNT; ++i )
					{
						char cmdLine[80];
						Q_snprintf( cmdLine, 80, "host_writeconfig_ss %d", i );
						engine->ClientCmd_Unrestricted( cmdLine );
					}
#endif
					// Dismiss the pause menu first, so it doesn't restore itself when this dialog goes away
					DismissPauseMenu();
					engine->ClientCmd_Unrestricted( "disconnect" );
					break;
				}
			case ON_CLOSED_QUIT:
				{
					ConVarRef xbox_arcade_title_unlocked( "xbox_arcade_title_unlocked" );
					bool bResult = xbox_arcade_title_unlocked.GetBool();

#if defined( _X360 )
					bResult = xboxsystem && xboxsystem->IsArcadeTitleUnlocked();
#elif defined ( _PS3 )
					//$TODO: Hook up PS3 trial mode check
#endif

					if ( bResult )
					{
						RunMenuCommand( "QuitNoConfirm" );
					}
					else
					{
						// In trial mode. Show UpSell
						RunMenuCommand( "OpenUpsellDialog" );
					}
				}

				break;

			case ON_CLOSED_RESTORE_PAUSE_MENU:
				RestorePauseMenu();
				break;

			case ON_CLOSED_RESTORE_MAIN_MENU:
				RestoreMainMenuScreen();
				break;

			case ON_CLOSED_DISCONNECT_TO_MP_GAME_MENU:
				RestoreMPGameMenu();
				break;

			default:
				if ( GameUI().IsInLevel() )
				{
					RestorePauseMenu();
				}
				else
				{
					RestoreMainMenuScreen();
				}
				break;
		}

		m_OnClosedCommand = ON_CLOSED_NULL;
	}

	return true;
}


void CCStrike15BasePanel::OnOpenMedalsDialog( )
{
	CCreateMedalStatsDialogScaleform::LoadDialog( CCreateMedalStatsDialogScaleform::eDialogType_Medals );
}

void CCStrike15BasePanel::OnOpenStatsDialog( )
{
	CCreateMedalStatsDialogScaleform::LoadDialog( CCreateMedalStatsDialogScaleform::eDialogType_Stats_Last_Match );
}

void CCStrike15BasePanel::CloseMedalsStatsDialog( )
{
	CCreateMedalStatsDialogScaleform::UnloadDialog( );
}

void CCStrike15BasePanel::OnOpenLeaderboardsDialog( )
{
	CCreateLeaderboardsDialogScaleform::LoadDialog( );
}

void CCStrike15BasePanel::OnOpenCallVoteDialog( )
{
	SFHudCallVotePanel::LoadDialog();
}

void CCStrike15BasePanel::OnOpenMarketplace( )
{
#ifdef _X360
	// $TODO Replace placeholder offer ID with real one
	engine->ClientCmd( VarArgs("x360_marketplace_offer %d 0x1111111 dl", XSHOWMARKETPLACEDOWNLOADITEMS_ENTRYPOINT_PAIDITEMS ) );
#endif  // _X360

}

void CCStrike15BasePanel::UpdateLeaderboardsDialog( )
{
	CCreateLeaderboardsDialogScaleform::UpdateDialog( );
}

void CCStrike15BasePanel::CloseLeaderboardsDialog( )
{
	CCreateLeaderboardsDialogScaleform::UnloadDialog( );
}

void CCStrike15BasePanel::OnOpenUpsellDialog( void )
{
	CUpsellScaleform::ShowMenu( true );
}

void CCStrike15BasePanel::StartExitingProcess( void )
{
	if (m_pSplitScreenSignon)
	{
		m_pSplitScreenSignon->RemoveFlashElement();
		m_pSplitScreenSignon = NULL;
	}

	CBaseModPanel::StartExitingProcess();
}

void CCStrike15BasePanel::RunFrame( void )
{
#if defined( _X360 )
	
	// We have a pending game voice channel check to perform - either prompt user to switch back, or if they failed to 
	//	OK switching back to game voice then quit the game - so run that here:
	if ( !IsLevelLoading() && m_GameVoiceChannelRecheckTimer.HasStarted() && m_GameVoiceChannelRecheckTimer.IsElapsed() )
	{
		m_GameVoiceChannelRecheckTimer.Invalidate();

		if ( BaseModUI::CUIGameData::Get()->IsXUIOpen() )
		{
			// If the Xbox guide is still open, set a timer to wait for it to go away
			m_GameVoiceChannelRecheckTimer.Start( 1.f );
		}
		else
		{
			if ( m_bShowRequiredGameVoiceChannelUI && Xbox_IsPartyChatEnabled() )
			{
				// If we've already prompted the user to switch to Game Chat, but they haven't approved it, disconnect now
				engine->ClientCmd_Unrestricted( "disconnect" );
			}
			else
			{
				// Most likely, we tried to prompt to switch to Game Chat while the guide was open 
				// It's closed now, so prompt the user to switch again
				Xbox_PromptSwitchToGameVoiceChannel();
			}
		}
	}

#endif

#ifdef _PS3

#ifndef NO_STEAM

	if ( ( s_ePS3SaveInitState == SIS_INIT_REQUESTED ) && s_PS3SaveAsyncStatus.JobDone() )
	{
		s_PS3SaveAsyncStatus.m_bDone = false;
		OnGameBootSaveContainerReady();
	}

#endif

#endif

	if ( m_pSplitScreenSignon )
	{
		m_pSplitScreenSignon->Update();
	}

	// Handles making sure pupups get the proper updates.  Usually just dealing with showing and hiding.
	PopupManager::Update();

	CBaseModPanel::RunFrame();

	// Make sure stats are loaded before showing the start logo.
	if ( IsStartScreenEnabled() && GetStatsLoaded() && !m_bStartLogoIsShowing )
	{
		// If on PS3, we now show the start menu option.  This is only hidden on PS3 in the scaleform so we can
		// delay showing it until stats are loaded.
		m_bStartLogoIsShowing = CCreateStartScreenScaleform::ShowStartLogo();
	}

#ifndef NO_STEAM
	if ( s_bSteamOverlayPositionNeedsToBeSet )
	{
		s_bSteamOverlayPositionNeedsToBeSet = false;

		ENotificationPosition ePos = k_EPositionTopLeft;
		if ( !V_stricmp( ui_steam_overlay_notification_position.GetString(), "topright" ) )
			ePos = k_EPositionTopRight;
		else if ( !V_stricmp( ui_steam_overlay_notification_position.GetString(), "bottomright" ) )
			ePos = k_EPositionBottomRight;
		else if ( !V_stricmp( ui_steam_overlay_notification_position.GetString(), "bottomleft" ) )
			ePos = k_EPositionBottomLeft;
		else if ( !V_stricmp( ui_steam_overlay_notification_position.GetString(), "topleft" ) )
			ePos = k_EPositionTopLeft;
		steamapicontext->SteamUtils()->SetOverlayNotificationPosition( ePos );
	}
#endif

	static bool s_bConnectLobbyChecked = false;
	if ( IsPC() && !s_bConnectLobbyChecked && g_flReadyToCheckForPCBootInvite && ( ( Plat_FloatTime() - g_flReadyToCheckForPCBootInvite ) > 2.5f ) )
	{
		s_bConnectLobbyChecked = true;

		// push initial rich presence string
		( void ) clientdll->GetRichPresenceStatusString();

		// if we were launched with "+connect_lobby <lobbyid>" on the command line, join that lobby immediately
		uint64 nLobbyID = 0ull;
		sscanf( CommandLine()->ParmValue( "+connect_lobby", "" ), "%llu", &nLobbyID );
		if ( nLobbyID != 0 )
		{
			Msg( "Connecting to lobby: 0x%llX\n", nLobbyID );
			KeyValues *kvEvent = new KeyValues( "OnSteamOverlayCall::LobbyJoin" );
			kvEvent->SetUint64( "sessionid", nLobbyID );
			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( kvEvent );
		}
		// Check if we were launched with +connect command and connect to that server
		else if ( char const *szConnectAdr = CommandLine()->ParmValue( "+connect" ) )
		{
			Msg( "Executing deferred connect command: %s\n", szConnectAdr );
			engine->ExecuteClientCmd( CFmtStr( "connect %s -%s\n", szConnectAdr, "ConnectStringOnCommandline" ) );
		}
		// Check if we were launched with +playcast command and connect to that server
		else if ( char const *szPlaycastUrl = CommandLine()->ParmValue( "+playcast" ) )
		{
			Msg( "Executing deferred playcast command: %s\n", szPlaycastUrl );
			engine->ExecuteClientCmd( CFmtStr( "playcast %s%s%s\n", ((szPlaycastUrl[0]=='"') ? "" : "\""), szPlaycastUrl, ((szPlaycastUrl[0]=='"') ? "" : "\"" )) );
		}
		else if ( char const *szEconActionPreview = CommandLine()->ParmValue( "+csgo_econ_action_preview" ) )
		{
			Msg( "Executing deferred econ action preview: %s\n", szEconActionPreview );
			engine->ExecuteClientCmd( CFmtStr( "csgo_econ_action_preview %s\n", szEconActionPreview ) );
		}
		else if ( char const *szDownloadMatch = CommandLine()->ParmValue( "+csgo_download_match" ) )
		{
			Msg( "Executing deferred download match: %s\n", szDownloadMatch );
			engine->ExecuteClientCmd( CFmtStr( "csgo_download_match %s\n", szDownloadMatch ) );
		}
		else if ( char const *szPlayDemoFirstArgumentParm = CommandLine()->ParmValue( "+playdemo" ) )
		{
			// need to handle more than one parameter on +playdemo commands
			if ( int nPlayDemoParm = CommandLine()->FindParm( "+playdemo" ) )
			{
				CUtlBuffer build( 0, 0, CUtlBuffer::TEXT_BUFFER );
				build.PutString( szPlayDemoFirstArgumentParm );

				// append all the stuff upafter the first argument (which is handled above) until the next command line argument (starting with a + or -)
				// this handles paths with spaces as well as the optional 2 extra parameters for playdemo
				for ( int i = nPlayDemoParm + 2; i < CommandLine()->ParmCount(); i++ )
				{
					char const *szAppendParameter = CommandLine()->GetParm( i );
					if ( ( szAppendParameter[0] != '+' ) && ( szAppendParameter[0] != '-' ) )
					{
						build.PutString( CFmtStr( " %s", szAppendParameter ) );
					}
					else
					{
						break;
					}
				}
				build.PutChar( '\0' );

				Msg( "Executing deferred playdemo: %s\n", (char *)build.Base() );
				engine->ExecuteClientCmd( CFmtStr( "playdemo %s\n", (char *)build.Base() ) );
			}
		}
		else if ( char const *gcconnect = strstr( CommandLine()->GetCmdLine(), "+gcconnect" ) )
		{
			Msg( "Executing deferred gcconnect: %s\n", gcconnect );
			/* Removed for partner depot */
		}
	}
}

void CCStrike15BasePanel::LockInput( void )
{
	if ( m_pSplitScreenSignon )
	{
		m_pSplitScreenSignon->RevertUIToOnePlayerMode();
	}

	CBaseModPanel::LockInput();
}

void CCStrike15BasePanel::UnlockInput( void )
{
	if ( m_pSplitScreenSignon )
	{
		m_pSplitScreenSignon->RevertUIToOnePlayerMode();
	}

	CBaseModPanel::UnlockInput();
}

void CCStrike15BasePanel::CheckIntroMovieStaticDependencies( void )
{
    m_bTestedStaticIntroMovieDependencies = true;

#if defined( _X360 )
	if ( ( XboxLaunch()->GetLaunchFlags() & LF_WARMRESTART ) )
	{
		// xbox does not play intro startup videos if it restarted itself
        m_bNeedToStartIntroMovie = false;
		return;
	}
#endif

  	if ( Plat_IsInBenchmarkMode() )
    {
        m_bNeedToStartIntroMovie = false;
		return;
    }

    if ( engine->IsInEditMode() ||
        CommandLine()->CheckParm( "-dev" ) || 
        CommandLine()->CheckParm( "-novid" ) || 
        CommandLine()->CheckParm( "-allowdebug" ) ||
        CommandLine()->CheckParm( "-console" ) ||
        CommandLine()->CheckParm( "-toconsole" ) )
    {
        m_bNeedToStartIntroMovie = false;
        return;
    }


}

bool CCStrike15BasePanel::IsScaleformIntroMovieEnabled( void )
{
    if ( !m_bNeedToStartIntroMovie && !CCreateLegalAnimScaleform::IsActive() )
        return false;

    if ( !m_bTestedStaticIntroMovieDependencies )
    {
        CheckIntroMovieStaticDependencies();
        if ( !m_bNeedToStartIntroMovie )
        {
            return false;
        }
    }

    return true;
}

void CCStrike15BasePanel::CreateScaleformIntroMovie( void )
{
    if ( IsScaleformIntroMovieEnabled() )
    {
        if ( !CCreateLegalAnimScaleform::IsActive() )
        {
            CCreateLegalAnimScaleform::CreateIntroMovie();
            ShowMainMenu( false );
            m_bNeedToStartIntroMovie = false;
            m_iIntroMovieButtonPressed = CheckForAnyKeyPressed( !IsX360() );
            if ( m_iIntroMovieButtonPressed != -1 )
                m_bIntroMovieWaitForButtonToClear = true;
            else
                m_bIntroMovieWaitForButtonToClear = false;
        }
    }
}

void CCStrike15BasePanel::DismissScaleformIntroMovie( void )
{
    if ( IsScaleformIntroMovieEnabled() )
    {
        CCreateLegalAnimScaleform::DismissAnimation();
    }
}

void CCStrike15BasePanel::OnPlayCreditsVideo( void )
{
    if ( !CCreateLegalAnimScaleform::IsActive() )
    {
        GameUI().SetBackgroundMusicDesired( false );
        CCreateLegalAnimScaleform::CreateCreditsMovie();
        m_iIntroMovieButtonPressed = CheckForAnyKeyPressed( !IsX360() );
        if ( m_iIntroMovieButtonPressed != -1 )
            m_bIntroMovieWaitForButtonToClear = true;
        else
            m_bIntroMovieWaitForButtonToClear = false;
    }
}




#if !defined(NO_STEAM) && defined(_PS3)

void CCStrike15BasePanel::ShowFatalError( uint32 unSize )
{
	if ( unSize > 0 )
	{
		int nKbRequired = int( (unSize + 1023) / 1024 );
		int nMbRequired = AlignValue( nKbRequired, 1024 )/1024;
		wchar_t const *szNoSpacePart1 = g_pVGuiLocalize->Find( "#SFUI_Boot_Error_NOSPACE1" );
		wchar_t const *szNoSpacePart2 = g_pVGuiLocalize->Find( "#SFUI_Boot_Error_NOSPACE2" );
		if ( szNoSpacePart1 && szNoSpacePart2 )
		{
			wchar_t wszBuffer[MAX_SCALEFORM_MESSAGE_BOX_LENGTH];
			int nLen1 = Q_wcslen( szNoSpacePart1 );
			int nLen2 = Q_wcslen( szNoSpacePart2 );
			AssertMsg( nLen1 + nLen2 + 100 < MAX_SCALEFORM_MESSAGE_BOX_LENGTH, "Message is too large for the buffer and the message box.");
			Q_wcsncpy( wszBuffer, szNoSpacePart1, 2 * ( nLen1 + nLen2 + 100 ) );
			Q_snwprintf( wszBuffer + Q_wcslen( wszBuffer ), 2*100, L"%u", nMbRequired );
			Q_wcsncpy( wszBuffer + Q_wcslen( wszBuffer ), szNoSpacePart2, 2*( nLen2 + 1 ) );
			
			// Set the body of the message to be the same as the title until we actually set the message.
			OnOpenMessageBox( "#SFUI_MsgBx_AttractDeviceFullC", "#SFUI_Boot_ErrorFatal", " ", MESSAGEBOX_FLAG_INVALID, this, NULL, wszBuffer );

			return;
		}
	}

	OnOpenMessageBox( "#SFUI_Boot_Error_Title", "#SFUI_Boot_ErrorFatal", " ", MESSAGEBOX_FLAG_INVALID, this);
}

void CCStrike15BasePanel::OnGameBootSaveContainerReady()
{
	s_ePS3SaveInitState = SIS_FINISHED;
	if ( s_PS3SaveAsyncStatus.GetSonyReturnValue() < 0 )
	{
		// We've got an error!
		Warning( "OnGameBootSaveContainerReady error: 0x%X\n", s_PS3SaveAsyncStatus.GetSonyReturnValue() );
		char const *szFmt = "#SFUI_Boot_Error_SAVE_GENERAL";
		switch ( s_PS3SaveAsyncStatus.GetSonyReturnValue() )
		{
		case CELL_SAVEDATA_ERROR_NOSPACE:
		case CELL_SAVEDATA_CBRESULT_ERR_NOSPACE:
		case CELL_SAVEDATA_ERROR_SIZEOVER:
			ShowFatalError( (s_PS3SaveAsyncStatus.m_uiAdditionalDetails ? s_PS3SaveAsyncStatus.m_uiAdditionalDetails : s_nPs3SaveStorageSizeKB ) * 1024 );
			return;
		case CELL_SAVEDATA_ERROR_BROKEN:
		case CELL_SAVEDATA_CBRESULT_ERR_BROKEN:
			szFmt = "#SFUI_Boot_Error_BROKEN";
			break;
		case CPS3SaveRestoreAsyncStatus::CELL_SAVEDATA_ERROR_WRONG_USER:
			szFmt = "#SFUI_Boot_Error_WRONG_USER";
			break;
		}

		// Set the body of the message to be the same as the title until we actually set the message.
		OnOpenMessageBox( "#SFUI_Boot_Save_Error_Title", szFmt, " ", MESSAGEBOX_FLAG_INVALID, this );

		return;
	}
	if ( s_PS3SaveAsyncStatus.m_nCurrentOperationTag != kSAVE_TAG_INITIALIZE )
	{
		ShowFatalError( s_nPs3TrophyStorageSizeKB );
		return;
	}

	CUtlBuffer *pInitialDataBuffer = GetPs3SaveSteamInfoProvider()->GetInitialLoadBuffer();

#ifndef NO_STEAM
	CMessageBoxScaleform::UnloadAllDialogs( true );
	OnOpenMessageBox("#SFUI_PS3_LOADING_TITLE", "#SFUI_PS3_LOADING_PROFILE_DATA", "", MESSAGEBOX_FLAG_INVALID, this );


	m_CallbackOnUserStatsReceived.Register( this, &CCStrike15BasePanel::Steam_OnUserStatsReceived );
	steamapicontext->SteamUserStats()->SetUserStatsData( pInitialDataBuffer->Base(), pInitialDataBuffer->TellPut() );
	steamapicontext->SteamUserStats()->RequestCurrentStats();
#endif


	pInitialDataBuffer->Purge();
}


void CCStrike15BasePanel::PerformPS3GameBootWork()
{
	static bool s_bBootOnce = false;
	if ( s_bBootOnce )
	{
		return;
	}
	s_bBootOnce = true;

	CMessageBoxScaleform::UnloadAllDialogs( true );
	OnOpenMessageBox("#SFUI_PS3_LOADING_TITLE", "#SFUI_PS3_LOADING_INSTALLING_TROPHIES", "", MESSAGEBOX_FLAG_INVALID, this );

	// Install PS3 trophies
	m_CallbackOnPS3TrophiesInstalled.Register( this, &CCStrike15BasePanel::Steam_OnPS3TrophiesInstalled );
	steamapicontext->SteamUserStats()->InstallPS3Trophies();	
}

void CCStrike15BasePanel::Steam_OnPS3TrophiesInstalled( PS3TrophiesInstalled_t *pParam )
{
	m_CallbackOnPS3TrophiesInstalled.Unregister();

	s_PS3SaveAsyncStatus.m_nCurrentOperationTag = kSAVE_TAG_INITIALIZE;
	EResult eResult = pParam->m_eResult;
	if ( eResult == k_EResultDiskFull )
	{
		s_PS3SaveAsyncStatus.m_nCurrentOperationTag = kSAVE_TAG_UNKNOWN;
		s_nPs3TrophyStorageSizeKB += ( pParam->m_ulRequiredDiskSpace + 1023 )/ 1024;
		s_nPs3SaveStorageSizeKB += s_nPs3TrophyStorageSizeKB;
		eResult = k_EResultOK; // report cumulative space required after save container gets created
	}

	if ( eResult == k_EResultOK )
	{
		
		CMessageBoxScaleform::UnloadAllDialogs( true );
		OnOpenMessageBox("#SFUI_PS3_LOADING_TITLE", "#SFUI_PS3_LOADING_INIT_SAVE_UTILITY", "", MESSAGEBOX_FLAG_INVALID, this );

		ps3saveuiapi->Initialize( &s_PS3SaveAsyncStatus, GetPs3SaveSteamInfoProvider(), true, s_nPs3SaveStorageSizeKB );
		s_ePS3SaveInitState = SIS_INIT_REQUESTED;
	}
	else
	{
		ShowFatalError( 0 );
	}

	// Let the overlay finally activate
	if ( g_pISteamOverlayMgr )
		g_pISteamOverlayMgr->GameBootReady();
}

void CCStrike15BasePanel::Steam_OnUserStatsReceived( UserStatsReceived_t *pParam )
{
	CMessageBoxScaleform::UnloadAllDialogs( true );


	m_CallbackOnUserStatsReceived.Unregister();

	// We've finally finished loading all the stats and such so we can tell the start screen to finish
	// "signing in".
	CBaseModPanel::SetStatsLoaded( true );
}


#endif	// !NO_STEAM && _PS3

#endif	// INCLUDE_SCALEFORM
