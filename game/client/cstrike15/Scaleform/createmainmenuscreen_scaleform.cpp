
//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: [jason] The "Press Start" Screen in Scaleform.
//
//=============================================================================//
#include "cbase.h"

#ifdef IS_WINDOWS_PC
#include <winlite.h>
#endif

#if defined( INCLUDE_SCALEFORM )

#include "basepanel.h"
#include "createmainmenuscreen_scaleform.h"
#include "messagebox_scaleform.h"
#include "../gameui/cstrike15/cstrike15basepanel.h"
#include "../engine/filesystem_engine.h"
#include <vgui/ILocalize.h>
#include "vgui/ISystem.h"

#if defined( _X360 )
#include "xbox/xbox_launch.h"
#else
#include "xbox/xboxstubs.h"
#endif

#include "engineinterface.h"
#include "modinfo.h"
#include "gameui_interface.h"
#include "tier1/utlbuffer.h"
#include "filesystem.h"
#include "vgui/ILocalize.h"
#include "cs_shareddefs.h"
#include "inputsystem/iinputsystem.h"
#include "cs_player_rank_mgr.h"
#include "achievements_cs.h"
#include "cs_player_rank_shared.h"
#include "gametypes.h"
#include "econ_item_inventory.h"
#include "econ_gcmessages.h"

#include "cstrike15_gcmessages.pb.h"
#include "cstrike15_gcconstants.h"
#include "engine/inetsupport.h"

#include "itempickup_scaleform.h"

// SF ChromeHTML Test
// #include "blog_scaleform.h"

using namespace vgui;

// for SRC
#include "vstdlib/random.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CCreateMainMenuScreenScaleform* CCreateMainMenuScreenScaleform::m_pInstance = NULL;

SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( BasePanelRunCommand ),
	SFUI_DECL_METHOD( IsMultiplayerPrivilegeEnabled ),
	SFUI_DECL_METHOD( LaunchTraining ),
	SFUI_DECL_METHOD( ViewMapInWorkshop ),
	SFUI_DECL_METHOD( GetPreviousLevel ),
SFUI_END_GAME_API_DEF( CCreateMainMenuScreenScaleform, MainMenu )
;

// Cache for tracking the presence of pending game invites so that the blocking ps3 system code does not need
// to be called unnecessarily
ConVar cl_invitation_pending( "cl_invitation_pending", "0", FCVAR_CLIENTDLL | FCVAR_HIDDEN );

//
// expose some build state constants to the UI as convars
//

ConVar play_with_friends_enabled( "play_with_friends_enabled", "1", FCVAR_CLIENTDLL | FCVAR_RELEASE );
ConVar key_bind_version( "key_bind_version", "0", FCVAR_CLIENTDLL | FCVAR_RELEASE | FCVAR_HIDDEN | FCVAR_ARCHIVE );

#ifdef OSX
ConVar osx_force_raw_input_once( "osx_force_raw_input_once", "0", FCVAR_CLIENTDLL | FCVAR_RELEASE | FCVAR_HIDDEN | FCVAR_ARCHIVE );
#endif

static const float TIMER_RETURN_GAME_IS_UNLOCKED = -1.0f;
static const float TIMER_RETURN_TRIAL_MODE_EXPIRED = 0.0f;

void CCreateMainMenuScreenScaleform::LoadDialog( void )
{
	if ( !m_pInstance )
	{
		static bool g_bEnteredMainMenuOnce = false;

		// [jason] Release the full-screen movie slot every time we return to the main menu from in-game,  
		//	 to prevent memory leaks caused by fragmentation in the Scaleform heap
		//  Do NOT dump the heap if we have a priority message open, or the user will miss this dialog box
		if ( g_bEnteredMainMenuOnce && !CMessageBoxScaleform::IsPriorityMessageOpen() )
		{
			ScaleformReleaseFullScreenAndCursor( g_pScaleformUI );

			// Force the UI tint convar to reset now, so the main menu picks it up
			static ConVarRef sf_ui_tint( "sf_ui_tint" );
			int actualValue = sf_ui_tint.GetInt();
			sf_ui_tint.SetValue( 0 );
			sf_ui_tint.SetValue( actualValue );
		}

		g_bEnteredMainMenuOnce = true;

		m_pInstance = new CCreateMainMenuScreenScaleform( );
		SFUI_REQUEST_ELEMENT( SF_FULL_SCREEN_SLOT, g_pScaleformUI, CCreateMainMenuScreenScaleform, m_pInstance, MainMenu );		

		m_pInstance->m_bVisible = true;
	}

	g_pInputSystem->SetSteamControllerMode( "MenuControls", m_pInstance ); 
}

void CCreateMainMenuScreenScaleform::UnloadDialog( void )
{
	if ( m_pInstance && m_pInstance->FlashAPIIsValid() )
	{
		g_pInputSystem->SetSteamControllerMode( NULL, m_pInstance ); 

		m_pInstance->HideImmediate();
		m_pInstance->RemoveFlashElement();
	}
}

void CCreateMainMenuScreenScaleform::UpdateDialog()
{
	if ( m_pInstance )
		m_pInstance->Tick();
}

CCreateMainMenuScreenScaleform::CCreateMainMenuScreenScaleform( void ) :
	m_pConfirmDialog( NULL ),
	m_bVisible( false ),
	m_bHideOnLoad( false ),
	m_bTrainingRequested( false ),
	m_uiClientHelloRequestedTimestampMS( 0 ),
	m_iPreviousPlayerLevel( -1 )
{
}

void CCreateMainMenuScreenScaleform::FlashLoaded( void )
{
	// $TODO: Call into any necessary Initializers on the action script side
}

#ifdef IS_WINDOWS_PC
ConVar cl_error_message_check_xboxdvr( "cl_error_message_check_xboxdvr", "0", FCVAR_ARCHIVE | FCVAR_HIDDEN );
#endif

void CCreateMainMenuScreenScaleform::FlashReady( void )
{
	// Make sure we check for invites after the main menu is fully ready
	extern float g_flReadyToCheckForPCBootInvite;
	g_flReadyToCheckForPCBootInvite = Plat_FloatTime();

	g_pScaleformUI->AddDeviceDependentObject( m_pInstance );

	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	Show();

	// If we requested to hide the screen before it finished loading, process that hide request now
	//	NOTE: because of design decisions made in the layout of this screen, we must always call Show
	//	when flash loads BEFORE we can call hide.  If you don't do this, the screen will not 
	//	be initialized correctly and when you show it later it will not function properly
	if ( m_bHideOnLoad )
	{
		HideImmediate();
		m_bHideOnLoad = false;
	}

	// Do Windows Xbox DVR check and warn the user
#ifdef IS_WINDOWS_PC
	static bool s_bOneTimeWarning = false;
	if ( !s_bOneTimeWarning )
	{
		s_bOneTimeWarning = true;

		struct tm tmNow;
		Plat_GetLocalTime( &tmNow );
		time_t tgm = Plat_timegm( &tmNow );
		int nTimeDelta = tgm - cl_error_message_check_xboxdvr.GetInt();
		if ( ( ( nTimeDelta >= 31 * 24 * 3600 ) || ( nTimeDelta <= -31 * 24 * 3600 ) )
			&& ( Plat_GetOSVersion() >= PLAT_OS_VERSION_WIN7 ) )
		{
			LONG lRegResult = 0;
			DWORD dwRegAllowGameDVR = 1, cbRegAllowGameDVR = sizeof( dwRegAllowGameDVR );
			DWORD dwRegAppCaptureEnabled = 0, cbRegAppCaptureEnabled = sizeof( dwRegAppCaptureEnabled );
			DWORD dwRegGameDVR_Enabled = 0, cbRegGameDVR_Enabled = sizeof( dwRegGameDVR_Enabled );
			HKEY hKey;

			RegOpenKeyEx( HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows\\GameDVR\\AllowGameDVR", NULL, KEY_READ, &hKey );
			lRegResult = ::RegQueryValueEx( hKey, "AllowGameDVR", NULL, NULL, ( BYTE * ) &dwRegAllowGameDVR, &cbRegAllowGameDVR );
			if ( lRegResult != ERROR_SUCCESS )
				dwRegAllowGameDVR = 1;
			DevMsg( "XboxDVRCheck: AllowGameDVR = 0x%08X (e0x%08X)\n", dwRegAllowGameDVR, lRegResult );
			RegCloseKey( hKey );

			RegOpenKeyEx( HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\GameDVR", NULL, KEY_READ, &hKey );
			lRegResult = ::RegQueryValueEx( hKey, "AppCaptureEnabled", NULL, NULL, ( BYTE * ) &dwRegAppCaptureEnabled, &cbRegAppCaptureEnabled );
			if ( lRegResult != ERROR_SUCCESS )
				dwRegAppCaptureEnabled = 0;
			DevMsg( "XboxDVRCheck: AppCaptureEnabled = 0x%08X (e0x%08X)\n", dwRegAppCaptureEnabled, lRegResult );
			RegCloseKey( hKey );

			RegOpenKeyEx( HKEY_CURRENT_USER, "System\\GameConfigStore", NULL, KEY_READ, &hKey );
			lRegResult = ::RegQueryValueEx( hKey, "GameDVR_Enabled", NULL, NULL, ( BYTE * ) &dwRegGameDVR_Enabled, &cbRegGameDVR_Enabled );
			if ( lRegResult != ERROR_SUCCESS )
				dwRegGameDVR_Enabled = 0;
			DevMsg( "XboxDVRCheck: GameDVR_Enabled = 0x%08X (e0x%08X)\n", dwRegGameDVR_Enabled, lRegResult );
			RegCloseKey( hKey );

			if ( dwRegAllowGameDVR && ( dwRegAppCaptureEnabled || dwRegGameDVR_Enabled ) )
			{	// Our user may have a framerate problem, tell them!
				CCommandMsgBox::CreateAndShow( "#SFUI_XboxDVR_Title", "#SFUI_XboxDVR_Explain", true, true, "error_message_explain_xboxdvr", "error_message_silence_xboxdvr" );
			}
		}
	}
#endif

	// Check if we should do Perfect World notification for the user
	OnEvent( KeyValues::AutoDeleteInline( new KeyValues( "GcLogonNotificationReceived" ) ) );

// SF ChromeHTML Test
//	if ( !CBlogScaleform::IsActive() )
//	{
//		CBlogScaleform::LoadDialog( );
//	}
}

#ifdef IS_WINDOWS_PC
CON_COMMAND_F( error_message_explain_xboxdvr, "Take user to Steam support article", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_HIDDEN )
{
	vgui::system()->ShellExecute( "open", "https://support.steampowered.com/kb_article.php?ref=6239-DZCB-8600" );
}
CON_COMMAND_F( error_message_silence_xboxdvr, "Take user to Steam support article", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_HIDDEN )
{
	struct tm tmNow;
	Plat_GetLocalTime( &tmNow );
	time_t tgm = Plat_timegm( &tmNow );
	cl_error_message_check_xboxdvr.SetValue( ( int ) tgm );
	engine->ClientCmd_Unrestricted( "host_writeconfig\n" );
}
#endif


void CCreateMainMenuScreenScaleform::PostUnloadFlash( void )
{
	// this is called when the dialog has finished animating, and
	// is ready for use to execute the server command.

	// it gets called no matter how the dialog was dismissed.

	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );

	g_pScaleformUI->RemoveDeviceDependentObject( this );

	g_pInputSystem->SetSteamControllerMode( NULL, m_pInstance ); 

	m_pInstance = NULL;
	delete this;
}

void CCreateMainMenuScreenScaleform::Show( void )
{
	GameUI().SetBackgroundMusicDesired( true );

	// [dkorus] reset our input device
	g_pInputSystem->ResetCurrentInputDevice(); 

	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", NULL, 0 );
		}

		m_bVisible = true;

		static bool bFirstShow = true;
// 		if ( bFirstShow && ( GCClientSystem()->GetGCClient()->GetConnectionStatus() != GCConnectionStatus_HAVE_SESSION || !g_GC2ClientHello.has_account_id() ) )
// 		{
// 			 m_uiClientHelloRequestedTimestampMS = Plat_MSTime();
// 		}

		bFirstShow = false;

// 		if ( CStorePanel::IsPricesheetLoaded() )
// 		{
// 			SCALEFORM_COMPONENT_BROADCAST_EVENT( Store, PriceSheetChanged );
// 		}
	}

	PerformKeyRebindings();
}

void CCreateMainMenuScreenScaleform::OnEvent( KeyValues *pEvent )
{
	/* Removed for partner depot */
}

void CCreateMainMenuScreenScaleform::Hide( void )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", 0, NULL );
		}

		m_bVisible = false;
	}
	else
	{
		// Set the deferred hide flag so we immediately hide the window once loaded
		m_bHideOnLoad = true;
	}
}

void CCreateMainMenuScreenScaleform::HideImmediate( void )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanelImmediate", 0, NULL );
		}             

		m_bVisible = false;
	}
	else
	{
		// Set the deferred hide flag so we immediately hide the window once loaded
		m_bHideOnLoad = true;
	}
}

void CCreateMainMenuScreenScaleform::ShowPanel( bool bShow, bool immediate /*= false*/ )
{
	if ( bShow && !m_pInstance )
	{
		// On the first show request, ensure we create the dialog if it doesn't exist
		LoadDialog();
	}

	if ( m_pInstance && !m_pInstance->FlashAPIIsValid() && immediate && !bShow )
	{
		m_pInstance->m_bHideOnLoad = true;
	}
	else if ( m_pInstance && bShow != m_pInstance->m_bVisible )
	{
		if ( bShow )
		{
			m_pInstance->Show();
		}
		else
		{
			if ( immediate )
			{
				m_pInstance->HideImmediate();
			}
			else
			{
				m_pInstance->Hide();
			}
		}
	}
}

void CCreateMainMenuScreenScaleform::BasePanelRunCommand( SCALEFORM_CALLBACK_ARGS_DECL )
{
	char RunCommandStr[1024];
	V_strncpy( &RunCommandStr[0], pui->Params_GetArgAsString( obj, 0 ), sizeof( RunCommandStr ) );

	// Is this dialog being brought up over top of the main menu panel, requiring us to hide it?
	bool bHideMainPanel = ( ( pui->Params_GetNumArgs( obj ) > 1 ) &&
						    ( Q_stricmp( pui->Params_GetArgAsString( obj, 1 ), "bHidePanel" ) == 0 ) );

	if ( bHideMainPanel )
	{
		ShowPanel( false );
	}

	// Run slotted command
	{
		char slotnumber[2];
		slotnumber[0] = '0'+GET_ACTIVE_SPLITSCREEN_SLOT();
		slotnumber[1] = 0;
		BasePanel()->PostMessage( BasePanel(), new KeyValues( "RunSlottedMenuCommand", "slot", slotnumber, "command", RunCommandStr ) );
	}
}

void CCreateMainMenuScreenScaleform::IsMultiplayerPrivilegeEnabled( SCALEFORM_CALLBACK_ARGS_DECL )
{
	bool bEnabled = true;

	bool bDisplayWarningBox  = ( ( pui->Params_GetNumArgs( obj ) > 0 ) &&
								 ( Q_stricmp( pui->Params_GetArgAsString( obj, 0 ), "bShowWarning" ) == 0 ) );

#if defined( _X360 )
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
	int userID = XBX_GetActiveUserId();
	BOOL EnabledFlag;
	XUserCheckPrivilege( userID, XPRIVILEGE_MULTIPLAYER_SESSIONS, &EnabledFlag );

	bEnabled = ( EnabledFlag == TRUE );
#endif
#if defined( _PS3 )
	bEnabled = !engine->PS3_IsUserRestrictedFromOnline();
#endif

	if ( !bEnabled && bDisplayWarningBox )
	{
//		ShowPanel( false );
#if defined( _PS3 )
		( ( CCStrike15BasePanel* )BasePanel() )->OnOpenMessageBox( "#SFUI_GameUI_OnlineErrorMessageTitle_PS3", "#SFUI_GameUI_NotOnlineEnabled_PS3", "#SFUI_GameUI_ErrorDismiss",  MESSAGEBOX_FLAG_OK, this, &m_pConfirmDialog );
#else
		( ( CCStrike15BasePanel* )BasePanel() )->OnOpenMessageBox( "#SFUI_GameUI_OnlineErrorMessageTitle", "#SFUI_GameUI_NotOnlineEnabled", "#SFUI_GameUI_ErrorDismiss",  MESSAGEBOX_FLAG_OK, this, &m_pConfirmDialog );
#endif
	}

	m_pScaleformUI->Params_SetResult( obj, bEnabled );
}

void CCreateMainMenuScreenScaleform::ViewMapInWorkshop( SCALEFORM_CALLBACK_ARGS_DECL )
{
	int idx = (int)pui->Params_GetArgAsNumber( obj, 0 );
	if ( idx >= 0 )
	{
		g_CSGOWorkshopMaps.ViewCommunityMapInWorkshop( idx );
	}
}

void CCreateMainMenuScreenScaleform::GetPreviousLevel( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, m_iPreviousPlayerLevel );
}

void *g_pvPassedEngineArray = NULL; // this array gets passed from engine and should be reported to GC
void CCreateMainMenuScreenScaleform::Tick()
{
	// Check if we need to use saved reconnect data because we cannot talk to GC
	if ( m_uiClientHelloRequestedTimestampMS &&
		( int( Plat_MSTime() - m_uiClientHelloRequestedTimestampMS ) > 6000 ) )
	{
		m_uiClientHelloRequestedTimestampMS = 0;
	}

	// Tick every half-second
	static double s_dblTickTime = 0;
	double dblTimeNow = Plat_FloatTime();
	if ( ( dblTimeNow - s_dblTickTime ) > 0.5 )
	{
		s_dblTickTime = dblTimeNow;
	/* Removed for partner depot */
	}
}

bool CCreateMainMenuScreenScaleform::OnMessageBoxEvent( MessageBoxFlags_t buttonPressed )
{
	if ( !m_pConfirmDialog )
		return false;

	if ( buttonPressed & MESSAGEBOX_FLAG_OK )
	{
		m_pConfirmDialog->Hide();
		m_pConfirmDialog = NULL;

		ShowPanel( true );
	}

	// $TODO: Handle other button presses if we have context-sensitive prompts

	return true;
}

void CCreateMainMenuScreenScaleform::RestorePanel( void )
{
	// [dkorus] reset our input device
	g_pInputSystem->ResetCurrentInputDevice(); 

	if ( m_pInstance && !m_pInstance->m_bVisible )
	{
		m_pInstance->InnerRestorePanel();
	}
}

void CCreateMainMenuScreenScaleform::InnerRestorePanel( void )
{
    GameUI().SetBackgroundMusicDesired( true );

    if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "restorePanel", NULL, 0 );
		}

		m_bVisible = true;
	}
}

void CCreateMainMenuScreenScaleform::LaunchTraining( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_bTrainingRequested = true;
	if ( !BasePanel()->ShowLockInput() )
	{
		DoLaunchTraining();
	}
}

void CCreateMainMenuScreenScaleform::DoLaunchTraining( void )
{
	BasePanel()->SetSinglePlayer( true );
	KeyValues *pSettings = KeyValues::FromString( "Settings", "System { network offline }Game { type training mode training mapgroupname mg_training1 }Options { action create }Contexts {}Properties {}" );
	KeyValues::AutoDelete autodelete( pSettings );

	BasePanel()->SetSinglePlayer( true );

	g_pMatchFramework->CreateSession( pSettings );
	IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
	if ( pMatchSession )
	{
		pMatchSession->Command( KeyValues::AutoDeleteInline( new KeyValues( "Start" ) ) );
	}
	else
	{
		Warning( "CCreateMainMenuScreenScaleform: unable to create single player session.\n" );
		BasePanel()->RestoreMainMenuScreen();
	}
}

void CCreateMainMenuScreenScaleform::DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd )
{
}

void CCreateMainMenuScreenScaleform::DeviceLost( void )
{
}

// #define REMAP_COMMAND( const char *oldCommand, const char *newCommand ) \
// 	const char *pszKey##oldCommand = engine->Key_LookupBindingExact(#oldCommand); \
// 	const char *pszNewKey##oldCommand = engine->Key_LookupBindingExact(#newCommand); \
// 	if ( pszKey##oldCommand && !pszNewKey##oldCommand ) \
// { \
// 	Msg( "Rebinding key %s to new command " #newCommand ".\n", pszKey##oldCommand ); \
// 	engine->ClientCmd_Unrestricted( VarArgs( "bind \"%s\" \"" #newCommand "\"\n", pszKey##oldCommand ) ); \
// }

void RebindKey( const char* szOldBind, const char* szNewBind, ButtonCode_t *buttons, uint32 unButtonCount )
{
	const char *pszKey = engine->Key_LookupBinding( szOldBind );
	const char *pszNewKey = engine->Key_LookupBinding( szNewBind );
	if ( pszKey && !pszNewKey )
	{
		Msg( "Rebinding key %s to new command %s.\n", pszKey, szNewBind );
		engine->ClientCmd_Unrestricted( VarArgs( "bind %s \"%s\"\nhost_writeconfig\n", pszKey, szNewBind ) );
	}
	else if ( !pszKey && !pszNewKey )
	{
		for ( uint32 i = 0; i < unButtonCount; i++ )
		{
			if ( !engine->Key_BindingForKey( buttons[ i ] ) )
			{
				Msg( "%s was not bound, binding to key %c.\n", szNewBind, buttons[ i ] - KEY_A + 'a' );
				engine->ClientCmd_Unrestricted( VarArgs( "bind %c \"%s\"\nhost_writeconfig\n", buttons[ i ] - KEY_A + 'a', szNewBind ) );
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Rebinds any binds for old commands to their new commands.
//-----------------------------------------------------------------------------
void CCreateMainMenuScreenScaleform::PerformKeyRebindings( void )
{
	//static ConVarRef key_bind_version( "key_bind_version" );
	
	// For testing
	// key_bind_version.SetValue( 0 );
	
	if ( key_bind_version.GetInt() < 1 )
	{
		ButtonCode_t buttons[ 6 ] = { KEY_F, KEY_T, KEY_V, KEY_G, KEY_B, KEY_H };
		RebindKey( "impulse 100", "+lookatweapon", buttons, ARRAYSIZE( buttons ) );
		key_bind_version.SetValue( 1 );
	}

	if ( key_bind_version.GetInt() < 2 )
	{
		ButtonCode_t buttons[ 6 ] = { KEY_T, KEY_F, KEY_V, KEY_G, KEY_B, KEY_H };
		RebindKey( "impulse 201", "+spray_menu", buttons, ARRAYSIZE( buttons) );
		key_bind_version.SetValue( 2 );
	}

#ifdef OSX
	// On OSX we want to force raw input on once after the 64-bit port since
	// we think it's the right choice for most users. Users can still revert
	// this setting if that's what they want, though.
	static ConVarRef osx_force_raw_input_once( "osx_force_raw_input_once" );

	if ( osx_force_raw_input_once.GetInt() == 0 )
	{
		static ConVarRef rawinput( "m_rawinput" );
		rawinput.SetValue( 1 );
		osx_force_raw_input_once.SetValue( 1 );
	}
#endif
}


#endif // INCLUDE_SCALEFORM
