//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VMainMenu.h"
#include "EngineInterface.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "VFlyoutMenu.h"
#include "VGenericConfirmation.h"
#include "VGenericWaitScreen.h"
#include "VQuickJoin.h"
#include "basemodpanel.h"
#include "UIGameData.h"
#include "VGameSettings.h"
#include "VSteamCloudConfirmation.h"
#include "vaddonassociation.h"

#include "VSignInDialog.h"
#include "VGuiSystemModuleLoader.h"
#include "VAttractScreen.h"
#include "gamemodes.h"
#include "transitionpanel.h"

#include "vgui/ILocalize.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Tooltip.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Image.h"

#include "filesystem.h"
#include "cegclientwrapper.h"

#ifndef NO_STEAM
#include "steam/isteamremotestorage.h"
#endif
#include "materialsystem/materialsystem_config.h"

#include "vgui/ISurface.h"
#include "tier0/icommandline.h"
#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

//=============================================================================
static ConVar connect_lobby( "connect_lobby", "", FCVAR_HIDDEN, "Sets the lobby ID to connect to on start." );
static ConVar ui_old_options_menu( "ui_old_options_menu", "0", FCVAR_HIDDEN, "Brings up the old tabbed options dialog from Keyboard/Mouse when set to 1." );
static ConVar ui_play_online_browser( "ui_play_online_browser",

#if defined( _DEMO ) && !defined( _X360 )
									 "0",
									 FCVAR_NONE,
#else
									 "1",
									 FCVAR_RELEASE,
#endif
									 "Whether play online displays a browser or plain search dialog." );

#ifndef _CERT
extern ConVar ui_sp_map_default;
#endif

ConVar cm_play_intro_video( "cm_play_intro_video", "1", FCVAR_ARCHIVE );

void Demo_DisableButton( Button *pButton );
void OpenGammaDialog( VPANEL parent );

class CPlaySPSelectStorageDevice : public CChangeStorageDevice
{
public:
	explicit CPlaySPSelectStorageDevice();

public:
	virtual void DeviceChangeCompleted( bool bChanged );
};

CPlaySPSelectStorageDevice:: CPlaySPSelectStorageDevice () :
	CChangeStorageDevice( XBX_GetPrimaryUserId() )
{
	// Allow non-involved controller
	m_bAnyController = false;

	// Don't force to re-select, just reload configs
	m_bForce = false;
}

void CPlaySPSelectStorageDevice::DeviceChangeCompleted( bool bChanged )
{
	CChangeStorageDevice::DeviceChangeCompleted( bChanged );

	CBaseModFrame *pWnd = CBaseModPanel::GetSingleton().GetWindow( WT_MAINMENU );
	if ( pWnd )
	{
		pWnd->PostMessage( pWnd, new KeyValues( "MsgOpenSinglePlayer" ) );
	}
}

//=============================================================================
char const * MainMenu::m_szPreferredControlName = "BtnPlaySolo";
CEG_NOINLINE MainMenu::MainMenu( Panel *parent, const char *panelName ):
	BaseClass( parent, panelName, true, true, false, false )
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );
	SetPaintBackgroundEnabled( true );

	CEG_PROTECT_MEMBER_FUNCTION( MainMenu_MainMenu );

	SetTitle( "", false );
	SetMoveable( false );
	SetSizeable( false );

	SetFooterEnabled( true );

	AddFrameListener( this );

	m_iQuickJoinHelpText = MMQJHT_NONE;

	m_nTileWidth = 0;
	m_nTileHeight = 0;
	m_nPinFromLeft = 0;
	m_nPinFromBottom = 0;
	m_nFooterOffsetY = 0;
}

//=============================================================================
MainMenu::~MainMenu()
{
	RemoveFrameListener( this );
}

//=============================================================================
CEG_NOINLINE void MainMenu::OnCommand( const char *command )
{
	int iUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();

	if ( UI_IsDebug() )
	{
		Msg("[GAMEUI] Handling main menu command %s from user%d ctrlr%d\n",
			command, iUserSlot, XBX_GetUserId( iUserSlot ) );
	}

	bool bOpeningFlyout = false;

	CEG_PROTECT_MEMBER_FUNCTION( MainMenu_OnCommand );

	if ( char const *szQuickMatch = StringAfterPrefix( command, "QuickMatch_" ) )
	{
		if ( CheckAndDisplayErrorIfNotLoggedIn() ||
			CUIGameData::Get()->CheckAndDisplayErrorIfNotSignedInToLive( this ) )
			return;

		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			" system { "
				" network LIVE "
			" } "
			" game { "
				" mode = "
			" } "
			" options { "
				" action quickmatch "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		pSettings->SetString( "game/mode", szQuickMatch );

		// TCR: We need to respect the default difficulty
		if ( GameModeHasDifficulty( szQuickMatch ) )
			pSettings->SetString( "game/difficulty", GameModeGetDefaultDifficulty( szQuickMatch ) );

		g_pMatchFramework->MatchSession( pSettings );
	}
	else if ( char const *szCustomMatch = StringAfterPrefix( command, "CustomMatch_" ) )
	{
		if ( CheckAndDisplayErrorIfNotLoggedIn() ||
			 CUIGameData::Get()->CheckAndDisplayErrorIfNotSignedInToLive( this ) )
			return;

		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			" system { "
				" network LIVE "
			" } "
			" game { "
				" mode = "
			" } "
			" options { "
				" action custommatch "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		pSettings->SetString( "game/mode", szCustomMatch );

		CBaseModPanel::GetSingleton().OpenWindow(
			ui_play_online_browser.GetBool() ? WT_FOUNDPUBLICGAMES : WT_GAMESETTINGS,
			this, true, pSettings );
	}
	else if ( char const *szFriendsMatch = StringAfterPrefix( command, "FriendsMatch_" ) )
	{
		if ( CheckAndDisplayErrorIfNotLoggedIn() )
			return;

		if ( StringHasPrefix( szFriendsMatch, "team" ) &&
			CUIGameData::Get()->CheckAndDisplayErrorIfNotSignedInToLive( this ) )
			// Team games require to be signed in to LIVE
			return;

		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			" game { "
				" mode = "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		pSettings->SetString( "game/mode", szFriendsMatch );

		CBaseModPanel::GetSingleton().OpenWindow( WT_ALLGAMESEARCHRESULTS, this, true, pSettings );
	}	
	else if ( char const *szGroupServer = StringAfterPrefix( command, "GroupServer_" ) )
	{
		if ( CheckAndDisplayErrorIfNotLoggedIn() )
			return;

		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			" game { "
				// " mode = "
			" } "
			" options { "
				" action groupserver "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		if ( *szGroupServer )
			pSettings->SetString( "game/mode", szGroupServer );

		CBaseModPanel::GetSingleton().OpenWindow( WT_STEAMGROUPSERVERS, this, true, pSettings );
	}
	else if ( char const *szLeaderboards = StringAfterPrefix( command, "Leaderboards_" ) )
	{
		if ( CheckAndDisplayErrorIfNotLoggedIn() ||
			CUIGameData::Get()->CheckAndDisplayErrorIfOffline( this,
			"#L4D360UI_MainMenu_SurvivalLeaderboards_Tip_Disabled" ) )
			return;

		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			" game { "
				" mode = "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		pSettings->SetString( "game/mode", szLeaderboards );

		CBaseModPanel::GetSingleton().OpenWindow( WT_LEADERBOARD, this, true, pSettings );
	}
	else if( !Q_strcmp( command, "VersusSoftLock" ) )
	{
		OnCommand( "FlmVersusFlyout" );
		return;
	}
	else if ( !Q_strcmp( command, "SurvivalCheck" ) )
	{
		OnCommand( "FlmSurvivalFlyout" );
		return;
	}
	else if ( !Q_strcmp( command, "ScavengeCheck" ) )
	{
		OnCommand( "FlmScavengeFlyout" );
		return;
	}
	else if ( !Q_strcmp( command, "SoloPlay" ) )
	{
		m_szPreferredControlName = "BtnPlaySolo";

#ifdef _GAMECONSOLE
		DWORD dwDevice = XBX_GetStorageDeviceId( XBX_GetPrimaryUserId() );
		if ( !XBX_GetPrimaryUserIsGuest() && ( dwDevice == XBX_INVALID_STORAGE_ID ) )
		{
			// Trigger storage device selector
			CUIGameData::Get()->SelectStorageDevice( new CPlaySPSelectStorageDevice() );
		}
		else
#endif
		{
			MsgOpenSinglePlayer();
		}
	}
	else if ( !Q_strcmp( command, "CoopPlay" ) )
	{
		m_szPreferredControlName = "BtnCoOp";

		if ( !IsGameConsole() || g_pFullFileSystem->IsSpecificDLCPresent( 1 ) )
		{
			// They have the DLC!
			MsgOpenCoopMode();
		}
		else
		{
#ifdef _GAMECONSOLE
			// They don't have the DLC!
			if ( XBX_GetPrimaryUserIsGuest() )
			{
				CUIGameData::Get()->InitiateSplitscreenPartnerDetection( "coop" );
			}
			else
#endif
			{
				CBaseModPanel::GetSingleton().OpenWindow( WT_STARTCOOPGAME, this );
			}
		}
	}
	else if ( !Q_strcmp( command, "DeveloperCommentary" ) )
	{
#ifdef _GAMECONSOLE
		if ( XBX_GetNumGameUsers() > 1 )
		{
			GenericConfirmation* confirmation = 
				static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

			GenericConfirmation::Data_t data;

			data.pWindowTitle = "#L4D360UI_MainMenu_SplitscreenDisableConf";
			data.pMessageText = "#L4D360UI_Extras_Commentary_ss_Msg";

			data.bOkButtonEnabled = true;
			data.pfnOkCallback = &AcceptSplitscreenDisableCallback;
			data.bCancelButtonEnabled = true;

			confirmation->SetUsageData(data);
			return;
		}
#endif
		// Explain the rules of commentary
		GenericConfirmation* confirmation = 
			static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#GAMEUI_CommentaryDialogTitle";
		data.pMessageText = "#L4D360UI_Commentary_Explanation";

		data.bOkButtonEnabled = true;
		data.pfnOkCallback = &AcceptCommentaryRulesCallback;
		data.bCancelButtonEnabled = true;

		confirmation->SetUsageData(data);
		NavigateFrom();
	}
#if 0
	else if ( !Q_strcmp( command, "StatsAndAchievements" ) )
	{
		// If PC make sure that the Steam user is logged in
		if ( CheckAndDisplayErrorIfNotLoggedIn() )
			return;

#ifdef _GAMECONSOLE
		// If 360 make sure that the user is not a guest
		if ( XBX_GetUserIsGuest( CBaseModPanel::GetSingleton().GetLastActiveUserId() ) )
		{
			GenericConfirmation* confirmation = 
				static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );
			GenericConfirmation::Data_t data;
			data.pWindowTitle = "#L4D360UI_MsgBx_AchievementsDisabled";
			data.pMessageText = "#L4D360UI_MsgBx_GuestsUnavailableToGuests";
			data.bOkButtonEnabled = true;
			confirmation->SetUsageData(data);

			return;
		}
#endif //_GAMECONSOLE
		CBaseModPanel::GetSingleton().OpenWindow( WT_ACHIEVEMENTS, this, true );
	}
#endif
	else if ( !Q_strcmp( command, "FlmExtrasFlyoutCheck" ) )
	{
		if ( IsGameConsole() && CUIGameData::Get()->SignedInToLive() )
			OnCommand( "FlmExtrasFlyout_Live" );
		else
			OnCommand( "FlmExtrasFlyout_Simple" );
		return;
	}
	else if ( char const *szInviteType = StringAfterPrefix( command, "InviteUI_" ) )
	{
		if ( IsGameConsole() )
		{
			CUIGameData::Get()->OpenInviteUI( szInviteType );
		}
		else
		{
			CUIGameData::Get()->ExecuteOverlayCommand( "LobbyInvite" );
		}
	}
	else if (!Q_strcmp(command, "Game"))
	{
		CBaseModPanel::GetSingleton().OpenWindow(WT_GAMEOPTIONS, this, true );
	}
	else if (!Q_strcmp(command, "AudioVideo"))
	{
		m_szPreferredControlName = "BtnOptions";
		CBaseModPanel::GetSingleton().OpenWindow(WT_AUDIOVIDEO, this, true );
	}
	else if (!Q_strcmp(command, "Controller"))
	{
		m_szPreferredControlName = "BtnOptions";
		CBaseModPanel::GetSingleton().OpenWindow(WT_CONTROLLER, this, true,
			KeyValues::AutoDeleteInline( new KeyValues( "Settings", "slot", iUserSlot ) ) );
	}
	else if (!Q_strcmp(command, "Storage"))
	{
		m_szPreferredControlName = "BtnOptions";

#ifdef _GAMECONSOLE
		if ( XBX_GetUserIsGuest( iUserSlot ) )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_INVALID );
			return;
		}
#endif
		// Trigger storage device selector
		CUIGameData::Get()->SelectStorageDevice( new CChangeStorageDevice( XBX_GetUserId( iUserSlot ) ) );
	}
	else if (!Q_strcmp(command, "Credits"))
	{
#ifdef _GAMECONSOLE
		if ( XBX_GetNumGameUsers() > 1 )
		{
			GenericConfirmation* confirmation = 
				static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

			GenericConfirmation::Data_t data;

			data.pWindowTitle = "#L4D360UI_MainMenu_SplitscreenDisableConf";
			data.pMessageText = "#L4D360UI_Extras_Credits_ss_Msg";

			data.bOkButtonEnabled = true;
			data.pfnOkCallback = &AcceptSplitscreenDisableCallback;
			data.bCancelButtonEnabled = true;

			confirmation->SetUsageData(data);
			return;
		}
#endif
		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			" system { "
				" network offline "
			" } "
			" game { "
				" mode coop "
				" map credits "
			" } "
			" options { "
				" play credits "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		g_pMatchFramework->CreateSession( pSettings );
	}
	else if (!Q_strcmp(command, "QuitGame"))
	{
		if ( IsPC() )
		{
			GenericConfirmation* confirmation = 
				static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

			GenericConfirmation::Data_t data;

			data.pWindowTitle = "#L4D360UI_MainMenu_Quit_Confirm";
			data.pMessageText = "#L4D360UI_MainMenu_Quit_ConfirmMsg";

			data.bOkButtonEnabled = true;
			data.pfnOkCallback = &AcceptQuitGameCallback;
			data.pOkButtonText = "#PORTAL2_ButtonAction_Quit";
			data.bCancelButtonEnabled = true;

			confirmation->SetUsageData(data);

			NavigateFrom();
		}

		if ( IsGameConsole() )
		{
			engine->ExecuteClientCmd( "demo_exit" );
		}
	}
	else if ( !Q_stricmp( command, "QuitGame_NoConfirm" ) )
	{
		if ( IsPC() )
		{
			engine->ClientCmd( "quit" );
		}
	}
	else if ( !Q_strcmp( command, "EnableSplitscreen" ) )
	{
		Msg( "Enabling splitscreen from main menu...\n" );

		CBaseModPanel::GetSingleton().CloseAllWindows();
		CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GOSPLITSCREEN );
		CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL, true );
	}
	else if ( !Q_strcmp( command, "DisableSplitscreen" ) )
	{
		GenericConfirmation* confirmation = 
			static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#L4D360UI_MainMenu_SplitscreenDisableConf";
		data.pMessageText = "#L4D360UI_MainMenu_SplitscreenDisableConfMsg";

		data.bOkButtonEnabled = true;
		data.pfnOkCallback = &AcceptSplitscreenDisableCallback;
		data.bCancelButtonEnabled = true;

		confirmation->SetUsageData(data);
	}
	else if ( !Q_strcmp( command, "DisableSplitscreen_NoConfirm" ) )
	{
		Msg( "Disabling splitscreen from main menu...\n" );

		CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GAMESTART  );
		OnCommand( "ActivateAttractScreen" );
	}
	else if (!Q_strcmp(command, "ChangeGamers"))	// guest SIGN-IN command
	{
		CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GUESTSIGNIN, XBX_GetUserId( iUserSlot ) );
		OnCommand( "ActivateAttractScreen" );
	}
	else if (!Q_strcmp(command, "ActivateAttractScreen"))
	{
		if ( IsGameConsole() )
		{
			Close();
			CBaseModPanel::GetSingleton().CloseAllWindows();
			CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GAMESTART );
			CBaseModFrame *pWnd = CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL, true );
			if ( pWnd )
			{
				pWnd->PostMessage( pWnd, new KeyValues( "ChangeGamers" ) );
			}
		}		
	}
	else if (!Q_strcmp(command, "Audio"))
	{
		if ( ui_old_options_menu.GetBool() )
		{
			CBaseModPanel::GetSingleton().OpenOptionsDialog( this );
		}
		else
		{
			// audio options dialog, PC only
			CBaseModPanel::GetSingleton().OpenWindow(WT_AUDIO, this, true );
		}
	}
	else if (!Q_strcmp(command, "Video"))
	{
		if ( ui_old_options_menu.GetBool() )
		{
			CBaseModPanel::GetSingleton().OpenOptionsDialog( this );
		}
		else
		{
			// video options dialog, PC only
			CBaseModPanel::GetSingleton().OpenWindow(WT_VIDEO, this, true );
		}
	}
	else if (!Q_strcmp(command, "Brightness"))
	{
		if ( ui_old_options_menu.GetBool() )
		{
			CBaseModPanel::GetSingleton().OpenOptionsDialog( this );
		}
		else
		{
			// brightness options dialog, PC only
			OpenGammaDialog( GetVParent() );
		}
	}
	else if (!Q_strcmp(command, "KeyboardMouse"))
	{
		if ( ui_old_options_menu.GetBool() )
		{
			CBaseModPanel::GetSingleton().OpenOptionsDialog( this );
		}
		else
		{
			// standalone keyboard/mouse dialog, PC only
			CBaseModPanel::GetSingleton().OpenWindow(WT_KEYBOARDMOUSE, this, true );
		}
	}
	else if (!Q_strcmp(command, "MultiplayerSettings"))
	{
		if ( ui_old_options_menu.GetBool() )
		{
			CBaseModPanel::GetSingleton().OpenOptionsDialog( this );
		}
		else
		{
			// standalone multiplayer settings dialog, PC only
			CBaseModPanel::GetSingleton().OpenWindow(WT_MULTIPLAYER, this, true );
		}
	}
	else if (!Q_strcmp(command, "CloudSettings"))
	{
		// standalone cloud settings dialog, PC only
		CBaseModPanel::GetSingleton().OpenWindow(WT_CLOUD, this, true );
	}
	else if (!Q_strcmp(command, "SeeAll"))
	{
		if ( CheckAndDisplayErrorIfNotLoggedIn() )
			return;

		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			" game { "
			// passing empty game settings to indicate no preference
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		CBaseModPanel::GetSingleton().OpenWindow( WT_ALLGAMESEARCHRESULTS, this, true, pSettings );
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
	}
	else if ( !Q_strcmp( command, "OpenServerBrowser" ) )
	{
		if ( CheckAndDisplayErrorIfNotLoggedIn() )
			return;

		// on PC, bring up the server browser and switch it to the LAN tab (tab #5)
		engine->ClientCmd( "openserverbrowser" );
	}
	else if ( !Q_strcmp( command, "DemoConnect" ) )
	{
		g_pMatchFramework->GetMatchTitle()->PrepareClientForConnect( NULL );
		engine->ClientCmd( CFmtStr( "connect %s", demo_connect_string.GetString() ) );
	}
	else if (command && command[0] == '#')
	{
		// Pass it straight to the engine as a command
		engine->ClientCmd( command+1 );
	}
	else if( !Q_strcmp( command, "Addons" ) )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_ADDONS, this, true );
	}
	else if ( !V_stricmp( command, "Options" ) )
	{
		m_szPreferredControlName = "BtnOptions";
		CBaseModPanel::GetSingleton().OpenWindow( WT_OPTIONS, this, true );
	}
	else if ( !V_stricmp( command, "Extras" ) )
	{
		m_szPreferredControlName = "BtnExtras";
		CBaseModPanel::GetSingleton().OpenWindow( WT_EXTRAS, this, true );
	}
	else if ( !V_stricmp( command, "EconUI" ) )
	{
		m_szPreferredControlName = "BtnEconUI";
		CBaseModPanel::GetSingleton().OpenWindow( WT_FADEOUTTOECONUI, this, false );
	}
#if defined( PORTAL2_PUZZLEMAKER )
	else if ( !V_stricmp( command, "CommunityPlay" ) )
	{
		m_szPreferredControlName = "BtnPlayCommunityMaps";
		CBaseModPanel::GetSingleton().OpenWindow( WT_COMMUNITYMAP, this, true );
	}
	else if ( !V_stricmp( command, "PlaytestDemos" ) )
	{
		m_szPreferredControlName = "BtnPlaytestDemos";
		CBaseModPanel::GetSingleton().OpenWindow( WT_PLAYTESTDEMOS, this, true );
	}
	else if ( !V_stricmp( command, "CreateChambers" ) )
	{
		// Play a video
		if ( cm_play_intro_video.GetBool() )
		{
			KeyValues *pSettings = new KeyValues( "MoviePlayer" );
			KeyValues::AutoDelete autodelete_pSettings( pSettings );
			pSettings->SetString( "video", "media/intro_movie.bik" );
			pSettings->SetBool( "letterbox", true );
			pSettings->SetBool( "editormenu_onclose", true );
			CBaseModPanel::GetSingleton().OpenWindow( WT_MOVIEPLAYER, this, false, pSettings );
			cm_play_intro_video.SetValue( 0 );
		}
		else
		{		
			m_szPreferredControlName = "BtnCreateChambers";
			CBaseModPanel::GetSingleton().OpenWindow( WT_EDITORMAINMENU, this, true );
		}
	}
#endif // PORTAL2_PUZZLEMAKER
	else if ( char const *szContentName = StringAfterPrefix( command, "GetDownloadableContent_" ) )
	{
		CUIGameData::Get()->GetDownloadableContent( szContentName );
	}
	else
	{
		const char *pchCommand = command;
		if ( !Q_strcmp(command, "FlmOptionsFlyout") )
		{
			m_szPreferredControlName = "BtnOptions";
#ifdef _GAMECONSOLE
			if ( XBX_GetPrimaryUserIsGuest() )
			{
				pchCommand = "FlmOptionsGuestFlyout";
			}
#endif
		}
		else if ( !Q_strcmp(command, "FlmVersusFlyout") )
		{
			command = "VersusSoftLock";
		}
		else if ( !Q_strcmp( command, "FlmSurvivalFlyout" ) )
		{
			command = "SurvivalCheck";
		}
		else if ( !Q_strcmp( command, "FlmScavengeFlyout" ) )
		{
			command = "ScavengeCheck";
		}
		else if ( StringHasPrefix( command, "FlmExtrasFlyout_" ) )
		{
			command = "FlmExtrasFlyoutCheck";
		}

		// does this command match a flyout menu?
		BaseModUI::FlyoutMenu *flyout = dynamic_cast< FlyoutMenu* >( FindChildByName( pchCommand ) );
		if ( flyout )
		{
			bOpeningFlyout = true;

			// If so, enumerate the buttons on the menu and find the button that issues this command.
			// (No other way to determine which button got pressed; no notion of "current" button on PC.)
			for ( int iChild = 0; iChild < GetChildCount(); iChild++ )
			{
				bool bFound = false;
				GameModes *pGameModes = dynamic_cast< GameModes *>( GetChild( iChild ) );
				if ( pGameModes )
				{
					for ( int iGameMode = 0; iGameMode < pGameModes->GetNumGameInfos(); iGameMode++ )
					{
						BaseModHybridButton *pHybrid = pGameModes->GetHybridButton( iGameMode );
						if ( pHybrid && pHybrid->GetCommand() && !Q_strcmp( pHybrid->GetCommand()->GetString( "command"), command ) )
						{
							pHybrid->NavigateFrom();
							// open the menu next to the button that got clicked
							flyout->OpenMenu( pHybrid );
							flyout->SetListener( this );
							bFound = true;
							break;
						}
					}
				}

				if ( !bFound )
				{
					BaseModHybridButton *hybrid = dynamic_cast<BaseModHybridButton *>( GetChild( iChild ) );
					if ( hybrid && hybrid->GetCommand() && !Q_strcmp( hybrid->GetCommand()->GetString( "command"), command ) )
					{
						hybrid->NavigateFrom();
						// open the menu next to the button that got clicked
						flyout->OpenMenu( hybrid );
						flyout->SetListener( this );
						break;
					}
				}
			}
		}
		else
		{
			BaseClass::OnCommand( command );
		}
	}

	if( !bOpeningFlyout )
	{
		FlyoutMenu::CloseActiveMenu(); //due to unpredictability of mouse navigation over keyboard, we should just close any flyouts that may still be open anywhere.
	}
}

void MainMenu::OnNavigateTo( const char* panelName )
{
	BaseClass::OnNavigateTo( panelName );

	static char const * arrControlsPersist[] = { "BtnPlaySolo", "BtnCoOp", "BtnOptions", "BtnQuit" };
	m_szPreferredControlName = arrControlsPersist[0];
	for ( int k = 1; k < ARRAYSIZE( arrControlsPersist ); ++ k )
	{
		if ( !Q_stricmp( panelName, arrControlsPersist[k] ) )
			m_szPreferredControlName = arrControlsPersist[k];
	}
}

//=============================================================================
void MainMenu::OpenMainMenuJoinFailed( const char *msg )
{
	// This is called when accepting an invite or joining friends game fails
	CUIGameData::Get()->OpenWaitScreen( msg );
	CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
}

//=============================================================================
void MainMenu::OnNotifyChildFocus( vgui::Panel* child )
{
}

void MainMenu::OnFlyoutMenuClose( vgui::Panel* flyTo )
{
	SetFooterState();
}

void MainMenu::OnFlyoutMenuCancelled()
{
}

//=============================================================================
CEG_NOINLINE void MainMenu::OnKeyCodePressed( KeyCode code )
{
	BaseModUI::CBaseModPanel::GetSingleton().ResetAttractDemoTimeout();

	int userId = GetJoystickForCode( code );
	BaseModUI::CBaseModPanel::GetSingleton().SetLastActiveUserId( userId );

	CEG_PROTECT_VIRTUAL_FUNCTION( MainMenu_OnKeyCodePressed );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		// Capture the B key so it doesn't play the cancel sound effect
		break;

	case KEY_XBUTTON_Y:
		{
#if defined( _X360 )
			CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
			if ( pFooter && ( pFooter->GetButtons() & FB_YBUTTON ) )
			{
				// Trigger storage device selector
				CUIGameData::Get()->SelectStorageDevice( new CChangeStorageDevice( XBX_GetPrimaryUserId() ) );
			}
#endif
		}
		break;

	case KEY_XBUTTON_BACK:
#ifdef _GAMECONSOLE
		if ( XBX_GetNumGameUsers() > 1 )
		{
			OnCommand( "DisableSplitscreen" );
		}
#endif
		break;

	case KEY_XBUTTON_INACTIVE_START:
#ifdef _GAMECONSOLE
		if ( !XBX_GetPrimaryUserIsGuest() &&
			 userId != (int) XBX_GetPrimaryUserId() &&
			 userId >= 0 &&
			 CUIGameData::Get()->CanPlayer2Join() )
		{
			// Pass the index of controller which wanted to join splitscreen
			CBaseModPanel::GetSingleton().CloseAllWindows();
			CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GOSPLITSCREEN, userId );
			CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL, true );
		}
#endif
		break;

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

//=============================================================================
void MainMenu::OnThink()
{
	// need to change state of flyout if user suddenly disconnects
	// while flyout is open
	BaseModUI::FlyoutMenu *flyout = dynamic_cast< FlyoutMenu* >( FindChildByName( "FlmCampaignFlyout" ) );
	if ( flyout )
	{
		BaseModHybridButton *pButton = dynamic_cast< BaseModHybridButton* >( flyout->FindChildButtonByCommand( "QuickMatchCoOp" ) );
		if ( pButton )
		{
			if ( !CUIGameData::Get()->SignedInToLive() )
			{
				pButton->SetText( "#L4D360UI_QuickStart" );
				if ( m_iQuickJoinHelpText != MMQJHT_QUICKSTART )
				{
					m_iQuickJoinHelpText = MMQJHT_QUICKSTART;
				}
			}
			else
			{
				pButton->SetText( "#L4D360UI_QuickMatch" );
				if ( m_iQuickJoinHelpText != MMQJHT_QUICKMATCH )
				{
					m_iQuickJoinHelpText = MMQJHT_QUICKMATCH;
				}
			}
		}
	}

	if ( IsPC() )
	{
		FlyoutMenu *pFlyout = dynamic_cast< FlyoutMenu* >( FindChildByName( "FlmOptionsFlyout" ) );
		if ( pFlyout )
		{
			const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
			pFlyout->SetControlEnabled( "BtnBrightness", !config.Windowed() );
		}
	}

	if ( IsGameConsole() )
	{
		// Need to keep updating since attract movie panel can change state
		SetFooterState();
	}

	BaseClass::OnThink();
}

//=============================================================================
void MainMenu::OnOpen()
{
	static bool s_bConnectLobbyChecked = false;
	if ( IsPC() && connect_lobby.GetString()[0] && !s_bConnectLobbyChecked )
	{
		// if we were launched with "+connect_lobby <lobbyid>" on the command line, join that lobby immediately
		uint64 nLobbyID = 0ull;
		sscanf( connect_lobby.GetString(), "%llu", &nLobbyID );
		if ( nLobbyID != 0 )
		{
			Msg( "Connecting to lobby: 0x%llX\n", nLobbyID );
			KeyValues *kvEvent = new KeyValues( "OnSteamOverlayCall::LobbyJoin" );
			kvEvent->SetUint64( "sessionid", nLobbyID );
			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( kvEvent );
		}
		// clear the convar so we don't try to join that lobby every time we return to the main menu
		connect_lobby.SetValue( "" );
	}
	s_bConnectLobbyChecked = true;

	BaseClass::OnOpen();

	SetFooterState();

#if !defined( _GAMECONSOLE ) && !defined( PORTAL2 )
	// Portal 2 clouds config by default
	bool bSteamCloudVisible = false;

	{
		static CGameUIConVarRef cl_cloud_settings( "cl_cloud_settings" );
		if ( cl_cloud_settings.GetInt() == -1 )
		{
			CBaseModPanel::GetSingleton().OpenWindow( WT_STEAMCLOUDCONFIRM, this, false );
			bSteamCloudVisible = true;
		}
	}

	if ( !bSteamCloudVisible )
	{
		if ( AddonAssociation::CheckAndSeeIfShouldShow() )
		{
			CBaseModPanel::GetSingleton().OpenWindow( WT_ADDONASSOCIATION, this, false );
		}
	}
#endif
}

//=============================================================================
void MainMenu::RunFrame()
{
	BaseClass::RunFrame();
}

//=============================================================================
CEG_NOINLINE void MainMenu::Activate()
{
	CEG_PROTECT_MEMBER_FUNCTION( MainMenu_Activate );

	BaseClass::Activate();

#ifdef _GAMECONSOLE
	OnFlyoutMenuClose( NULL );
#endif

	vgui::Panel *firstPanel = FindChildByName( m_szPreferredControlName );
	if ( firstPanel )
	{
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom();
		}
		firstPanel->NavigateTo();
	}
}

//=============================================================================
void MainMenu::PaintBackground() 
{
	MarkTiles();
}

void MainMenu::SetFooterState()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		int screenWide, screenTall;
		surface()->GetScreenSize( screenWide, screenTall );

		pFooter->SetPosition( m_nPinFromLeft, screenTall - m_nPinFromBottom + m_nFooterOffsetY );

		FooterButtons_t buttons = 0;
			
		if ( IsGameConsole() )
		{
			buttons |= FB_ABUTTON;
		}
		
#if defined( _X360 )
		DWORD dwDevice = XBX_GetStorageDeviceId( XBX_GetPrimaryUserId() );
		if ( !XBX_GetPrimaryUserIsGuest() &&
			!V_stricmp( m_szPreferredControlName, "BtnPlaySolo" ) &&
			( dwDevice != XBX_INVALID_STORAGE_ID ) )
		{
			buttons |= FB_YBUTTON;
		}
#endif

		buttons |= FB_STEAM_SELECT;

		pFooter->SetButtons( buttons );
		pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		
		if ( IsX360() )
		{
			pFooter->SetButtonText( FB_YBUTTON, "#PORTAL2_ChangeStorageDevice" );
		}
	}
}

//=============================================================================
CEG_NOINLINE void MainMenu::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	GetDialogTileSize( m_nTileWidth, m_nTileHeight );

	m_nPinFromBottom = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "Dialog.PinFromBottom" ) ) );
	m_nPinFromLeft = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "Dialog.PinFromLeft" ) ) );
	m_nFooterOffsetY = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "FooterPanel.OffsetY" ) ) );

	const char *pSettings = "Resource/UI/BaseModUI/mainmenu_new.res";
#if !defined( _GAMECONSOLE )
	if ( !g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetPrimaryUserId() ) )
	{
		pSettings = "Resource/UI/BaseModUI/MainMenuStub.res";
	}
#endif

	LoadControlSettings( pSettings );

	CEG_PROTECT_MEMBER_FUNCTION( MainMenu_ApplySchemeSettings );

#ifdef _GAMECONSOLE
	if ( !XBX_GetPrimaryUserIsGuest() )
	{
		wchar_t wszListText[ 128 ];
		wchar_t wszPlayerName[ 128 ];

		IPlayer *player1 = NULL;
		if ( XBX_GetNumGameUsers() > 0 )
		{
			player1 = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetUserId( 0 ) );
		}

		IPlayer *player2 = NULL;
		if ( XBX_GetNumGameUsers() > 1 )
		{
			player2 = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetUserId( 1 ) );
		}

		if ( player1 )
		{
			Label *pLblPlayer1GamerTag = dynamic_cast< Label* >( FindChildByName( "LblPlayer1GamerTag" ) );
			if ( pLblPlayer1GamerTag )
			{
				g_pVGuiLocalize->ConvertANSIToUnicode( player1->GetName(), wszPlayerName, sizeof( wszPlayerName ) );
				g_pVGuiLocalize->ConstructString( wszListText, sizeof( wszListText ), g_pVGuiLocalize->Find( "#L4D360UI_MainMenu_LocalProfilePlayer1" ), 1, wszPlayerName );

				pLblPlayer1GamerTag->SetVisible( true );
				pLblPlayer1GamerTag->SetText( wszListText );
			}
		}

		if ( player2 )
		{
			Label *pLblPlayer2GamerTag = dynamic_cast< Label* >( FindChildByName( "LblPlayer2GamerTag" ) );
			if ( pLblPlayer2GamerTag )
			{
				g_pVGuiLocalize->ConvertANSIToUnicode( player2->GetName(), wszPlayerName, sizeof( wszPlayerName ) );
				g_pVGuiLocalize->ConstructString( wszListText, sizeof( wszListText ), g_pVGuiLocalize->Find( "#L4D360UI_MainMenu_LocalProfilePlayer2" ), 1, wszPlayerName );

				pLblPlayer2GamerTag->SetVisible( true );
				pLblPlayer2GamerTag->SetText( wszListText );

				// in split screen, have player2 gamer tag instead of enable, and disable
				SetControlVisible( "LblPlayer2DisableIcon", true );
				SetControlVisible( "LblPlayer2Disable", true );
				SetControlVisible( "LblPlayer2Enable", false );
			}
		}
		else
		{
			SetControlVisible( "LblPlayer2DisableIcon", false );
			SetControlVisible( "LblPlayer2Disable", false );

			// not in split screen, no player2 gamertag, instead have enable
			SetControlVisible( "LblPlayer2GamerTag", false );
			SetControlVisible( "LblPlayer2Enable", true );
		}
	}
#endif

	if ( IsPC() )
	{
		FlyoutMenu *pFlyout = dynamic_cast< FlyoutMenu* >( FindChildByName( "FlmOptionsFlyout" ) );
		if ( pFlyout )
		{
			bool bUsesCloud = false;

#ifndef NO_STEAM
			ISteamRemoteStorage *pRemoteStorage =
#ifdef _PS3
				::SteamRemoteStorage();
#else
				SteamClient()?(ISteamRemoteStorage *)SteamClient()->GetISteamGenericInterface(
				SteamAPI_GetHSteamUser(), SteamAPI_GetHSteamPipe(), STEAMREMOTESTORAGE_INTERFACE_VERSION ):NULL;
#endif

			int32 availableBytes, totalBytes = 0;
			if ( pRemoteStorage && pRemoteStorage->GetQuota( &totalBytes, &availableBytes ) )
			{
				if ( totalBytes > 0 )
				{
					bUsesCloud = true;
				}
			}
#else
			AssertMsg( false, "This branch run on a PC build without IS_WINDOWS_PC defined." );
#endif

			pFlyout->SetControlEnabled( "BtnCloud", bUsesCloud );
		}
	}

	SetFooterState();

	vgui::Panel *firstPanel = FindChildByName( m_szPreferredControlName );
	if ( firstPanel )
	{
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom();
		}
		firstPanel->NavigateTo();
	}

#if defined( _X360 ) && defined( _DEMO )
	SetControlVisible( "BtnExtras", !engine->IsDemoHostedFromShell() );
	SetControlVisible( "BtnQuit", engine->IsDemoHostedFromShell() );
#endif

	// CERT CATCH ALL JUST IN CASE!
#ifdef _GAMECONSOLE
	bool bAllUsersCorrectlySignedIn = ( XBX_GetNumGameUsers() > 0 );
	for ( int k = 0; k < ( int ) XBX_GetNumGameUsers(); ++ k )
	{
		if ( !g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetUserId( k ) ) )
			bAllUsersCorrectlySignedIn = false;
	}
	if ( !bAllUsersCorrectlySignedIn )
	{
		Warning( "======= SIGNIN FAIL SIGNIN FAIL SIGNIN FAIL SIGNIN FAIL ==========\n" );
		Assert( 0 );
		CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );
		CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GAMESTART );
		CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL, true );
		Warning( "======= SIGNIN RESET SIGNIN RESET SIGNIN RESET SIGNIN RESET ==========\n" );
	}
#endif
}

void MainMenu::MarkTiles()
{
	// determine bounds if menu was tile based
	if ( m_nTileWidth && m_nTileHeight )
	{
		for ( int i = 0; i < GetChildCount(); i++ )
		{
			BaseModHybridButton *pButton = dynamic_cast< BaseModHybridButton* >( GetChild( i ) );
			if ( pButton && pButton->IsEnabled() && pButton->IsVisible() )
			{
				int x, y, wide, tall;
				pButton->GetBounds( x, y, wide, tall );
				if ( wide && tall )
				{
					CBaseModPanel::GetSingleton().GetTransitionEffectPanel()->MarkTilesInRect( x, y, wide, tall, WT_MAINMENU );
				}
			}
		}
	}
}

const char *pDemoDisabledButtons[] = { "BtnVersus", "BtnSurvival", "BtnStatsAndAchievements", "BtnExtras" };

void MainMenu::Demo_DisableButtons( void )
{
	for ( int i = 0; i < ARRAYSIZE( pDemoDisabledButtons ); i++ )
	{
		BaseModHybridButton *pButton = dynamic_cast< BaseModHybridButton* >( FindChildByName( pDemoDisabledButtons[i] ) );

		if ( pButton )
		{
			Demo_DisableButton( pButton );
		}
	}
}

void MainMenu::AcceptCommentaryRulesCallback() 
{
	if ( MainMenu *pMainMenu = static_cast< MainMenu* >( CBaseModPanel::GetSingleton().GetWindow( WT_MAINMENU ) ) )
	{
		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			" system { "
				" network offline "
			" } "
			" game { "
				" mode coop "
				" map devtest "
			" } "
			" options { "
				" play commentary "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		g_pMatchFramework->CreateSession( pSettings );
	}

}

void MainMenu::AcceptSplitscreenDisableCallback()
{
	if ( MainMenu *pMainMenu = static_cast< MainMenu* >( CBaseModPanel::GetSingleton().GetWindow( WT_MAINMENU ) ) )
	{
		pMainMenu->OnCommand( "DisableSplitscreen_NoConfirm" );
	}
}

void MainMenu::AcceptQuitGameCallback()
{
	if ( MainMenu *pMainMenu = static_cast< MainMenu* >( CBaseModPanel::GetSingleton().GetWindow( WT_MAINMENU ) ) )
	{
		pMainMenu->OnCommand( "QuitGame_NoConfirm" );
	}
}

void MainMenu::AcceptVersusSoftLockCallback()
{
	if ( MainMenu *pMainMenu = static_cast< MainMenu* >( CBaseModPanel::GetSingleton().GetWindow( WT_MAINMENU ) ) )
	{
		pMainMenu->OnCommand( "FlmVersusFlyout" );
	}
}


#ifndef NO_STEAM
CON_COMMAND_F( openserverbrowser, "Opens server browser", 0 )
{
	bool isSteam = IsPC() && steamapicontext->SteamFriends() && steamapicontext->SteamUtils();
	if ( isSteam )
	{
		// show the server browser
		g_VModuleLoader.ActivateModule("Servers");

		// if an argument was passed, that's the tab index to show, send a message to server browser to switch to that tab
		if ( args.ArgC() > 1 )
		{
			KeyValues *pKV = new KeyValues( "ShowServerBrowserPage" );
			pKV->SetInt( "page", atoi( args[1] ) );
			g_VModuleLoader.PostMessageToAllModules( pKV );
		}
	}
}
#endif

void MainMenu::MsgOpenCoopMode()
{
	GenericConfirmation *pGenericConfirmation = static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION ) );
	GenericWaitScreen *pWaitScreen = static_cast< GenericWaitScreen* >( CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );
	if ( pWaitScreen || pGenericConfirmation )
	{
		// have to wait
		PostMessage( this, new KeyValues( "MsgOpenCoopMode" ), 0.001f );
		return;
	}

	//KeyValues *pSettings = new KeyValues( "CoopModeSelect" );		// ?
	//KeyValues::AutoDelete autodelete_pSettings( pSettings );		// ?
	CBaseModPanel::GetSingleton().OpenWindow( WT_COOPMODESELECT, this, true );

}

void MainMenu::MsgOpenSinglePlayer()
{
	bool bFoundSaveGame = false;

	GenericConfirmation *pGenericConfirmation = static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION ) );
	GenericWaitScreen *pWaitScreen = static_cast< GenericWaitScreen* >( CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );
	if ( pWaitScreen || pGenericConfirmation )
	{
		// have to wait
		PostMessage( this, new KeyValues( "MsgOpenSinglePlayer" ), 0.001f );
		return;
	}

	//
	// Storage SHOULD have already been mounted
	//
	DWORD dwDevice = XBX_GetStorageDeviceId( XBX_GetPrimaryUserId() );
	if ( XBX_DescribeStorageDevice( dwDevice ) )
	{
		// check storage
		CUtlVector< SaveGameInfo_t > saveGameInfos;
		bFoundSaveGame = CBaseModPanel::GetSingleton().GetSaveGameInfos( saveGameInfos, false );
	}

	if ( bFoundSaveGame || BaseModUI::CBaseModPanel::GetSingleton().GetChapterProgress() > 0 )
	{
		// have save game or detected progress, must let user into SP menu 
		// send the save game detection through, avoids a redundant scan by the SP menu
		KeyValues *pSettings = new KeyValues( "SinglePlayer" );
		KeyValues::AutoDelete autodelete_pSettings( pSettings );
		pSettings->SetBool( "foundsavegame", bFoundSaveGame );
		CBaseModPanel::GetSingleton().OpenWindow( WT_SINGLEPLAYER, this, true, pSettings );
	}
#ifdef _GAMECONSOLE
	else if ( XBX_GetPrimaryUserIsGuest() )
#else
	else if ( !IsGameConsole() )
#endif
	{
		// autosave notice not applicable, start the single player game immediately
		KeyValues *pSettings = new KeyValues( "FadeOutStartGame" );
		KeyValues::AutoDelete autodelete_pSettings( pSettings );
		pSettings->SetString( "map", CBaseModPanel::GetSingleton().ChapterToMapName( 1 ) );
		pSettings->SetString( "reason", "newgame" );
		CBaseModPanel::GetSingleton().OpenWindow( WT_FADEOUTSTARTGAME, this, true, pSettings );
	}
	else
	{
		// no saves or chapter progress, start the single player game immediately after the autosave notice
		KeyValues *pSettings = new KeyValues( "NewGame" );
		KeyValues::AutoDelete autodelete_pSettings( pSettings );
		pSettings->SetString( "map", CBaseModPanel::GetSingleton().ChapterToMapName( 1 ) );
		pSettings->SetString( "reason", "newgame" );
		CBaseModPanel::GetSingleton().OpenWindow( WT_AUTOSAVENOTICE, this, true, pSettings );
	}
}

#if !defined( _GAMECONSOLE )
CEG_NOINLINE void MainMenu::OnMousePressed( vgui::MouseCode code )
{
	BaseClass::OnMousePressed( code );

	if ( m_ActiveControl )
	{
		m_ActiveControl->NavigateTo();
	}
	else
	{
		vgui::Panel *pFirstPanel = FindChildByName( m_szPreferredControlName );
		if ( pFirstPanel )
		{
			pFirstPanel->NavigateTo();
		}
	}

	CEG_PROTECT_VIRTUAL_FUNCTION( MainMenu_OnMousePressed );
}
#endif
