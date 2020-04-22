//===== Copyright  1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "uigamedata.h"

#if defined( _WIN32 ) && !defined( _GAMECONSOLE )
#include "winlite.h"
#endif

#include <stdio.h>

#include "threadtools.h"

#include "basepanel.h"
#include "engineinterface.h"
#include "vguisystemmoduleloader.h"

#include "vgui/IInput.h"
#include "vgui/ILocalize.h"
#include "vgui/IPanel.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "filesystem.h"
#include "gameconsole.h"
#include "gameui_interface.h"
#include <game/client/iviewport.h>

using namespace vgui;

#include "gameconsole.h"
#include "modinfo.h"

#include "IGameUIFuncs.h"
#include "backgroundmenubutton.h"
#include "vgui_controls/AnimationController.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/MenuItem.h"
#include "vgui_controls/PHandle.h"
#include "vgui_controls/MessageBox.h"
#include "vgui_controls/QueryBox.h"
#include "vgui_controls/ControllerMap.h"
#include "tier0/icommandline.h"
#include "tier1/convar.h"
#include "newgamedialog.h"
#include "bonusmapsdialog.h"
#include "loadgamedialog.h"
#include "savegamedialog.h"
#include "optionsdialog.h"
#include "createmultiplayergamedialog.h"

#include "changegamedialog.h"
#include "backgroundmenubutton.h"
#include "playerlistdialog.h"
#include "benchmarkdialog.h"
#include "loadcommentarydialog.h"
#include "bonusmapsdatabase.h"
#include "engine/IEngineSound.h"
#include "bitbuf.h"
#include "tier1/fmtstr.h"
#include "inputsystem/iinputsystem.h"
#include "ixboxsystem.h"
#include "optionssubaudio.h"
#if defined( _X360 )
#include "../common/xlast_csgo/csgo.spa.h"
#endif
#include "iachievementmgr.h"
#include "customtabexplanationdialog.h"
#include "loadingscreen_scaleform.h"
// dgoodenough - limit this to X360 only
// PS3_BUILDFIX
#if defined( _X360 )
#include "xbox/xbox_launch.h"
#else
#include "xbox/xboxstubs.h"
#endif

#if defined( _PS3)
#include <sysutil/sysutil_userinfo.h>
#endif

#include "matchmaking/imatchframework.h"
#include "matchmaking/mm_helpers.h"

#include "tier1/utlstring.h"
#include "steam/steam_api.h"
#include "byteswap.h"
#include "cdll_client_int.h"
#include "gametypes.h"

#include "game/client/IGameClientExports.h"

#include "VGuiMatSurface/IMatSystemSurface.h"

#include "cbase.h"
#include "cs_shareddefs.h"

#include "itempickup_scaleform.h"

#include "checksum_sha1.h"


#undef MessageBox	// Windows helpfully #define's this to MessageBoxA, we're using vgui::MessageBox

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define MAIN_MENU_INDENT_X360 10

ConVar vgui_message_dialog_modal( "vgui_message_dialog_modal", "1", FCVAR_ARCHIVE );

ConVar ui_test_community_matchmaking( "ui_test_community_matchmaking", "0", FCVAR_DEVELOPMENTONLY );

static CBaseModPanel	*g_pBasePanel = NULL;
static float		g_flAnimationPadding = 0.01f;

extern const char *COM_GetModDirectory( void );

extern bool bSteamCommunityFriendsVersion;

#ifdef _PS3
static CellUserInfoUserStat s_userStat;
#endif

CGameMenuItem::CGameMenuItem(vgui::Menu *parent, const char *name)  : BaseClass(parent, name, "GameMenuItem") 
{
	m_bRightAligned = false;
}

void CGameMenuItem::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	// make fully transparent
	SetFgColor(GetSchemeColor("MainMenu.TextColor", pScheme));
	SetBgColor(Color(0, 0, 0, 0));
	SetDefaultColor(GetSchemeColor("MainMenu.TextColor", pScheme), Color(0, 0, 0, 0));
	SetArmedColor(GetSchemeColor("MainMenu.ArmedTextColor", pScheme), Color(0, 0, 0, 0));
	SetDepressedColor(GetSchemeColor("MainMenu.DepressedTextColor", pScheme), Color(0, 0, 0, 0));
	SetContentAlignment(Label::a_west);
	SetBorder(NULL);
	SetDefaultBorder(NULL);
	SetDepressedBorder(NULL);
	SetKeyFocusBorder(NULL);

	vgui::HFont hMainMenuFont = pScheme->GetFont( "MainMenuFont", false);
	if ( hMainMenuFont )
	{
		SetFont( hMainMenuFont );
	}
	else
	{
		SetFont( pScheme->GetFont( "MenuLarge", false ) );
	}
	SetTextInset(0, 0);
	SetArmedSound("UI/buttonrollover.wav");
	SetDepressedSound("UI/buttonclick.wav");
	SetReleasedSound("UI/buttonclickrelease.wav");
	SetButtonActivationType(Button::ACTIVATE_ONPRESSED);

	if ( GameUI().IsConsoleUI() )
	{
		SetArmedColor(GetSchemeColor("MainMenu.ArmedTextColor", pScheme), GetSchemeColor("Button.ArmedBgColor", pScheme));
		SetTextInset( MAIN_MENU_INDENT_X360, 0 );
	}

	if (m_bRightAligned)
	{
		SetContentAlignment(Label::a_east);
	}
}

void CGameMenuItem::PaintBackground()
{
	if ( !GameUI().IsConsoleUI() )
	{
		BaseClass::PaintBackground();
	}
	else
	{
		if ( !IsArmed() || !IsVisible() || GetParent()->GetAlpha() < 32 )
			return;

		int wide, tall;
		GetSize( wide, tall );

		DrawBoxFade( 0, 0, wide, tall, GetButtonBgColor(), 1.0f, 255, 0, true );
		DrawBoxFade( 2, 2, wide - 4, tall - 4, Color( 0, 0, 0, 96 ), 1.0f, 255, 0, true );
	}
}

void CGameMenuItem::SetRightAlignedText(bool state)
{
	m_bRightAligned = state;
}

//-----------------------------------------------------------------------------
// Purpose: General purpose 1 of N menu
//-----------------------------------------------------------------------------
class CGameMenu : public vgui::Menu
{
public:
	DECLARE_CLASS_SIMPLE( CGameMenu, vgui::Menu );

	CGameMenu(vgui::Panel *parent, const char *name) : BaseClass(parent, name) 
	{
		if ( GameUI().IsConsoleUI() )
		{
			// shows graphic button hints
			m_pConsoleFooter = new CFooterPanel( parent, "MainMenuFooter" );

			int iFixedWidth = 245;

// dgoodenough - limit this to X360 only
// PS3_BUILDFIX
#if defined( _X360 )
			// In low def we need a smaller highlight
			XVIDEO_MODE videoMode;
			XGetVideoMode( &videoMode );
			if ( !videoMode.fIsHiDef )
			{
				iFixedWidth = 240;
			}
			else
			{
				iFixedWidth = 350;
			}
#endif

			SetFixedWidth( iFixedWidth );
		}
		else
		{
			m_pConsoleFooter = NULL;
		}
	}

	virtual void ApplySchemeSettings(IScheme *pScheme)
	{
		BaseClass::ApplySchemeSettings(pScheme);

		// make fully transparent
		SetMenuItemHeight(atoi(pScheme->GetResourceString("MainMenu.MenuItemHeight")));
		SetBgColor(Color(0, 0, 0, 0));
		SetBorder(NULL);
	}

	virtual void LayoutMenuBorder()
	{
	}

	virtual void SetVisible(bool state)
	{
		// force to be always visible
		BaseClass::SetVisible(true);
		// move us to the back instead of going invisible
		if (!state)
		{
			ipanel()->MoveToBack(GetVPanel());
		}
	}

	virtual int AddMenuItem(const char *itemName, const char *itemText, const char *command, Panel *target, KeyValues *userData = NULL)
	{
		MenuItem *item = new CGameMenuItem(this, itemName);
		item->AddActionSignalTarget(target);
		item->SetCommand(command);
		item->SetText(itemText);
		item->SetUserData(userData);
		return BaseClass::AddMenuItem(item);
	}

	virtual int AddMenuItem(const char *itemName, const char *itemText, KeyValues *command, Panel *target, KeyValues *userData = NULL)
	{
		CGameMenuItem *item = new CGameMenuItem(this, itemName);
		item->AddActionSignalTarget(target);
		item->SetCommand(command);
		item->SetText(itemText);
		item->SetRightAlignedText(true);
		item->SetUserData(userData);
		return BaseClass::AddMenuItem(item);
	}

	virtual void SetMenuItemBlinkingState( const char *itemName, bool state )
	{
		for (int i = 0; i < GetChildCount(); i++)
		{
			Panel *child = GetChild(i);
			MenuItem *menuItem = dynamic_cast<MenuItem *>(child);
			if (menuItem)
			{
				if ( Q_strcmp( menuItem->GetCommand()->GetString("command", ""), itemName ) == 0 )
				{
					menuItem->SetBlink( state );
				}
			}
		}
		InvalidateLayout();
	}

	virtual void OnCommand(const char *command)
	{
		if (!stricmp(command, "Open"))
		{
			MoveToFront();
			RequestFocus();
		}
		else
		{
			BaseClass::OnCommand(command);
		}
	}

	virtual void OnKeyCodePressed( KeyCode code )
	{
		if ( IsGameConsole() )
		{
			if ( GetAlpha() != 255 )
			{
				SetEnabled( false );
				// inhibit key activity during transitions
				return;
			}

			SetEnabled( true );

			if ( code == KEY_XBUTTON_B || code == KEY_XBUTTON_START )
			{
				if ( GameUI().IsInLevel() )
				{
					GetParent()->OnCommand( "ResumeGame" );
				}
				return;
			}
		}
	
		BaseClass::OnKeyCodePressed( code );

		// HACK: Allow F key bindings to operate even here
		if ( IsPC() && code >= KEY_F1 && code <= KEY_F12 )
		{
			// See if there is a binding for the FKey
			const char *binding = gameuifuncs->GetBindingForButtonCode( code );
			if ( binding && binding[0] )
			{
				// submit the entry as a console commmand
				char szCommand[256];
				Q_strncpy( szCommand, binding, sizeof( szCommand ) );
				engine->ClientCmd_Unrestricted( szCommand, true );
			}
		}
	}

	virtual void OnKillFocus()
	{
		BaseClass::OnKillFocus();

		// force us to the rear when we lose focus (so it looks like the menu is always on the background)
		surface()->MovePopupToBack(GetVPanel());
	}

	void ShowFooter( bool bShow )
	{
		if ( m_pConsoleFooter )
		{
			m_pConsoleFooter->SetVisible( bShow );
		}
	}

	void UpdateMenuItemState( bool isInGame, bool isMultiplayer )
	{
		bool isSteam = IsPC() && ( CommandLine()->FindParm("-steam") != 0 );
		bool bIsConsoleUI = GameUI().IsConsoleUI();

		// disabled save button if we're not in a game
		for (int i = 0; i < GetChildCount(); i++)
		{
			Panel *child = GetChild(i);
			MenuItem *menuItem = dynamic_cast<MenuItem *>(child);
			if (menuItem)
			{
				bool shouldBeVisible = true;
				// filter the visibility
				KeyValues *kv = menuItem->GetUserData();
				if (!kv)
					continue;

				if (!isInGame && kv->GetInt("OnlyInGame") )
				{
					shouldBeVisible = false;
				}
				if ( isInGame && kv->GetInt( "MainMenuOnly" ) )
				{
					shouldBeVisible = false;
				}
				else if (isMultiplayer && kv->GetInt("notmulti"))
				{
					shouldBeVisible = false;
				}
				else if (isInGame && !isMultiplayer && kv->GetInt("notsingle"))
				{
					shouldBeVisible = false;
				}
				else if (isSteam && kv->GetInt("notsteam"))
				{
					shouldBeVisible = false;
				}
				else if ( !bIsConsoleUI && kv->GetInt( "ConsoleOnly" ) )
				{
					shouldBeVisible = false;
				}
				else if ( bIsConsoleUI && kv->GetInt( "PCOnly" ) )
				{
					shouldBeVisible = false;
				}

				menuItem->SetVisible( shouldBeVisible );
			}
		}

		if ( !isInGame )
		{
			// Sort them into their original order
			for ( int j = 0; j < GetChildCount() - 2; j++ )
			{
				MoveMenuItem( j, j + 1 );
			}
		}
		else
		{
			// Sort them into their in game order
			for ( int i = 0; i < GetChildCount(); i++ )
			{
				for ( int j = i; j < GetChildCount() - 2; j++ )
				{
					int iID1 = GetMenuID( j );
					int iID2 = GetMenuID( j + 1 );

					MenuItem *menuItem1 = GetMenuItem( iID1 );
					MenuItem *menuItem2 = GetMenuItem( iID2 );

					KeyValues *kv1 = menuItem1->GetUserData();
					KeyValues *kv2 = menuItem2->GetUserData();

					if ( kv1->GetInt("InGameOrder") > kv2->GetInt("InGameOrder") )
						MoveMenuItem( iID2, iID1 );
				}
			}
		}

		InvalidateLayout();

		if ( m_pConsoleFooter )
		{
			// update the console footer
			const char *pHelpName;
			if ( !isInGame )
				pHelpName = "MainMenu";
			else
				pHelpName = "GameMenu";

			if ( !m_pConsoleFooter->GetHelpName() || V_stricmp( pHelpName, m_pConsoleFooter->GetHelpName() ) )
			{
				// game menu must re-establish its own help once it becomes re-active
				m_pConsoleFooter->SetHelpNameAndReset( pHelpName );
				m_pConsoleFooter->AddNewButtonLabel( "#GameUI_Action", "#GameUI_Icons_A_BUTTON" );
				if ( isInGame )
				{
					m_pConsoleFooter->AddNewButtonLabel( "#GameUI_Close", "#GameUI_Icons_B_BUTTON" );
				}
			}
		}
	}

	MESSAGE_FUNC_PTR( OnCursorEnteredMenuItem, "CursorEnteredMenuItem", VPanel);

private:
	CFooterPanel *m_pConsoleFooter;
};

//-----------------------------------------------------------------------------
// Purpose: Respond to cursor entering a menuItem.
//-----------------------------------------------------------------------------
void CGameMenu::OnCursorEnteredMenuItem(vgui::Panel* VPanel)
{
	VPANEL menuItem = (VPANEL)VPanel;
	MenuItem *item = static_cast<MenuItem *>(ipanel()->GetPanel(menuItem, GetModuleName()));

	// [jason] Disable the cursor enter if the menu item is invisible and disabled
	if ( item && !item->IsEnabled() )
		return;

	KeyValues *pCommand = item->GetCommand();
	if ( !pCommand->GetFirstSubKey() )
		return;
	const char *pszCmd = pCommand->GetFirstSubKey()->GetString();
	if ( !pszCmd || !pszCmd[0] )
		return;

	BaseClass::OnCursorEnteredMenuItem( VPanel );
}

static CBackgroundMenuButton* CreateMenuButton( CBaseModPanel *parent, const char *panelName, const wchar_t *panelText )
{
	CBackgroundMenuButton *pButton = new CBackgroundMenuButton( parent, panelName );
	pButton->SetProportional(true);
	pButton->SetCommand("OpenGameMenu");
	pButton->SetText(panelText);

	return pButton;
}

bool g_bIsCreatingNewGameMenuForPreFetching = false;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBaseModPanel::CBaseModPanel( const char *panelName ) : Panel(NULL, panelName )
{	
	g_pBasePanel = this;
	m_bLevelLoading = false;
	m_eBackgroundState = BACKGROUND_INITIAL;
	m_flTransitionStartTime = 0.0f;
	m_flTransitionEndTime = 0.0f;
	m_flFrameFadeInTime = 0.5f;
	m_bRenderingBackgroundTransition = false;
	m_bFadingInMenus = false;
	m_bEverActivated = false;
	m_iGameMenuInset = 24;
	m_bPlatformMenuInitialized = false;
	m_bHaveDarkenedBackground = false;
	m_bHaveDarkenedTitleText = true;
	m_bForceTitleTextUpdate = true;
	m_BackdropColor = Color(0, 0, 0, 128);
	m_pConsoleAnimationController = NULL;
	m_pConsoleControlSettings = NULL;
	m_bCopyFrameBuffer = false;
	m_bUseRenderTargetImage = false;
	m_ExitingFrameCount = 0;
	m_bXUIVisible = false;
	m_bUseMatchmaking = false;
	m_bRestartFromInvite = false;
	m_bRestartSameGame = false;
	m_bUserRefusedSignIn = false;
	m_bUserRefusedStorageDevice = false;
	m_bWaitingForUserSignIn = false;
	m_bReturnToMPGameMenuOnDisconnect = false;
	m_bForceQuitToDesktopOnDisconnect = false;

#if !defined(NO_STEAM) && defined(_PS3)

	m_bStatsLoaded = false;

#else

	m_bStatsLoaded = true;

#endif

	m_bWaitingForStorageDeviceHandle = false;
	m_bNeedStorageDeviceHandle = false;
	m_bStorageBladeShown = false;
	m_iStorageID = XBX_INVALID_STORAGE_ID;
	m_pAsyncJob = NULL;
	m_pStorageDeviceValidatedNotify = NULL;
	m_bStartScreenPlayerSigninCompleted = false;
	m_bMainMenuShown = true;
	
	// [jason] Flags to enable/disable scaleform screens during the startup sequence (start screen, mainmenu)
	m_bForceStartScreen = false;
	m_bShowStartScreen = true;
	m_bScaleformMainMenuEnabled = true; 
	m_bScaleformPauseMenuEnabled = true;	
	m_bBypassStartScreen = false;

    m_iIntroMovieButtonPressed = -1;
    m_bIntroMovieWaitForButtonToClear = false;

	m_primaryUserId = -1;

	// any platforms that go straight to main menu, no start screen, should set this flag:
	if ( CommandLine()->FindParm( "-nostartscreen" ) || 
		 IsPC() ||
		 IsLinux() ||
		 IsOSX() )
	{
		m_bBypassStartScreen = true;
	}

	// "map" parameter bypasses only the start screen, to get your directly into the level
	if ( CommandLine()->FindParm( "+map" ) )
	{
		m_bShowStartScreen = false;
	}

	// these options disable start screen as well as the front-end menus
	bool bNoScaleformFrontEnd = ( CommandLine()->FindParm( "-hijack" ) ) ||
								( IsPlatformWindowsPC() &&  CommandLine()->FindParm( "-tools" ) ) || 
								( CommandLine()->FindParm( "-no_scaleform_menu_on_boot" ) );

	if ( bNoScaleformFrontEnd )
	{
		m_bShowStartScreen = false;
		m_bScaleformMainMenuEnabled = false;
	}

	if ( GameUI().IsConsoleUI() )
	{
		m_pConsoleAnimationController = new AnimationController( this );
		m_pConsoleAnimationController->SetScriptFile( GetVPanel(), "scripts/GameUIAnimations.txt" );
		m_pConsoleAnimationController->SetAutoReloadScript( IsDebug() );

		m_pConsoleControlSettings = new KeyValues( "XboxDialogs.res" );
		if ( !m_pConsoleControlSettings->LoadFromFile( g_pFullFileSystem, "resource/UI/XboxDialogs.res", "GAME" ) )
		{
			Error( "Failed to load UI control settings!\n" );
		}
	}

	m_pGameMenuButtons.AddToTail( CreateMenuButton( this, "GameMenuButton", ModInfo().GetGameTitle() ) );
	m_pGameMenuButtons.AddToTail( CreateMenuButton( this, "GameMenuButton2", ModInfo().GetGameTitle2() ) );
#ifdef CS_BETA
	if ( !ModInfo().NoCrosshair() ) // hack to not show the BETA for HL2 or HL1Port
	{
		m_pGameMenuButtons.AddToTail( CreateMenuButton( this, "BetaButton", L"BETA" ) );
	}
#endif // CS_BETA

	m_pGameMenu = NULL;
	m_pGameLogo = NULL;

// 2016-Apr-18 <vitaliy> -- this looks like a bunch of legacy code, we cannot run with any version of Steam Client, but the latest
// 	if ( SteamClient() )
// 	{
// 		HSteamPipe steamPipe = SteamClient()->CreateSteamPipe();
// 		ISteamUtils *pUtils = SteamClient()->GetISteamUtils( steamPipe, "SteamUtils002" );
// 		if ( pUtils )
// 		{
			bSteamCommunityFriendsVersion = true;
// 		}
// 
// 		SteamClient()->BReleaseSteamPipe( steamPipe );
// 	}

	CreateGameMenu();
	CreateGameLogo();

	// Bonus maps menu blinks if something has been unlocked since the player last opened the menu
	// This is saved as persistant data, and here is where we check for that
	CheckBonusBlinkState();

	// start the menus fully transparent
	SetMenuAlpha( 0 );

	if ( GameUI().IsConsoleUI() )
	{
		// do any costly resource prefetching now....
		// force the new dialog to get all of its chapter pics
		g_bIsCreatingNewGameMenuForPreFetching = true;
		m_hNewGameDialog = new CNewGameDialog( this, false );
		m_hNewGameDialog->MarkForDeletion();
		g_bIsCreatingNewGameMenuForPreFetching = false;

#if 0
		m_hOptionsDialog_Xbox = new COptionsDialogXbox( this );
		m_hOptionsDialog_Xbox->MarkForDeletion();

		m_hControllerDialog = new CControllerDialog( this );
		m_hControllerDialog->MarkForDeletion();
#endif
		
        if ( !IsStartScreenEnabled() && !IsScaleformIntroMovieEnabled() && !m_bBypassStartScreen )
		{
			ArmFirstMenuItem();
			m_pConsoleAnimationController->StartAnimationSequence( "InitializeUILayout" );
		}
	}

	// Record data used for rich presence updates
	if ( IsGameConsole() )
	{
//#if defined( CSTRIKE15 )
		m_bSinglePlayer = false;
//#else
		// DWenger - Pulled this out for now to get things to compile for cs15
		/*
		// Get our active mod directory name
		const char *pGameName = CommandLine()->ParmValue( "-game", "hl2" );;

		// Set the game we're playing
		m_iGameID = CONTEXT_GAME_GAME_HALF_LIFE_2;
		m_bSinglePlayer = true;
		if ( Q_stristr( pGameName, "episodic" ) )
		{
			m_iGameID = CONTEXT_GAME_GAME_EPISODE_ONE;
		}
		else if ( Q_stristr( pGameName, "ep2" ) )
		{
			m_iGameID = CONTEXT_GAME_GAME_EPISODE_TWO;
		}
		else if ( Q_stristr( pGameName, "portal" ) )
		{
			m_iGameID = CONTEXT_GAME_GAME_PORTAL;
		}
		else if ( Q_stristr( pGameName, "tf" ) )
		{
			m_iGameID = CONTEXT_GAME_GAME_TEAM_FORTRESS;
			m_bSinglePlayer = false;
		}
		*/
//#endif // CSTRIKE15
	}

	m_pCodeVersionLabel = new Label( this, "CodeVersionLabel", "" );
	m_pContentVersionLabel = new Label( this, "ContentVersionLabel", "" );
}

//-----------------------------------------------------------------------------
// Purpose: Xbox 360 - Get the console UI keyvalues to pass to LoadControlSettings()
//-----------------------------------------------------------------------------
KeyValues *CBaseModPanel::GetConsoleControlSettings( void )
{
	return m_pConsoleControlSettings;
}

//-----------------------------------------------------------------------------
// Purpose: Causes the first menu item to be armed
//-----------------------------------------------------------------------------
void CBaseModPanel::ArmFirstMenuItem( void )
{
	UpdateGameMenus();

	// Arm the first item in the menu
	for ( int i = 0; i < m_pGameMenu->GetItemCount(); ++i )
	{
		if ( m_pGameMenu->GetMenuItem( i )->IsVisible() )
		{
			m_pGameMenu->SetCurrentlyHighlightedItem( i );
			break;
		}
	}
}

CBaseModPanel::~CBaseModPanel()
{
	g_pBasePanel = NULL;
}

static char *g_rgValidCommands[] =
{
	"OpenGameMenu",
	"OpenPlayerListDialog",
	"OpenNewGameDialog",
	"OpenLoadGameDialog",
	"OpenSaveGameDialog",
	"OpenCustomMapsDialog",
	"OpenOptionsDialog",
	"OpenBenchmarkDialog",
	"OpenServerBrowser",
	"OpenFriendsDialog",
	"OpenLoadDemoDialog",
	"OpenCreateMultiplayerGameDialog",
	"OpenCreateMultiplayerGameCommunity",
	"OpenChangeGameDialog",
	"OpenLoadCommentaryDialog",
	"Quit",
	"QuitNoConfirm",
	"ResumeGame",
	"Disconnect",

	"OpenCreateSinglePlayerGameDialog",
	"OpenCreateStartScreen",
	"ShowInvitePartyUI",
	"ShowJoinPartyUI",
	"ShowInviteFriendsUI",	
	"ShowPlayerSelectionScoreboard",
	"OpenMotionCalibrationDialog",
};

static void CC_GameMenuCommand( const CCommand &args )
{
	int c = args.ArgC();
	if ( c < 2 )
	{
		Msg( "Usage:  gamemenucommand <commandname>\n" );
		return;
	}

	if ( !g_pBasePanel )
	{
		return;
	}

	vgui::ivgui()->PostMessage( g_pBasePanel->GetVPanel(), new KeyValues("Command", "command", args[1] ), NULL);
}

static bool UtlStringLessFunc( const CUtlString &lhs, const CUtlString &rhs )
{
	return Q_stricmp( lhs.String(), rhs.String() ) < 0;
}
static int CC_GameMenuCompletionFunc( char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	char const *cmdname = "gamemenucommand";

	char *substring = (char *)partial;
	if ( Q_strstr( partial, cmdname ) )
	{
		substring = (char *)partial + strlen( cmdname ) + 1;
	}

	int checklen = Q_strlen( substring );

	CUtlRBTree< CUtlString > symbols( 0, 0, UtlStringLessFunc );

	int i;
	int c = ARRAYSIZE( g_rgValidCommands );
	for ( i = 0; i < c; ++i )
	{
		if ( Q_strnicmp( g_rgValidCommands[ i ], substring, checklen ) )
			continue;

		CUtlString str;
		str = g_rgValidCommands[ i ];

		symbols.Insert( str );

		// Too many
		if ( symbols.Count() >= COMMAND_COMPLETION_MAXITEMS )
			break;
	}

	// Now fill in the results
	int slot = 0;
	for ( i = symbols.FirstInorder(); i != symbols.InvalidIndex(); i = symbols.NextInorder( i ) )
	{
		char const *name = symbols[ i ].String();

		char buf[ 512 ];
		Q_strncpy( buf, name, sizeof( buf ) );
		Q_strlower( buf );

		Q_snprintf( commands[ slot++ ], COMMAND_COMPLETION_ITEM_LENGTH, "%s %s",
			cmdname, buf );
	}

	return slot;
}

static ConCommand gamemenucommand( "gamemenucommand", CC_GameMenuCommand, "Issue game menu command.", 0, CC_GameMenuCompletionFunc );

CON_COMMAND_F( quit_prompt, "Exit the engine.", FCVAR_NONE )
{
	BasePanel()->OnCommand( "quittodesktop" );
}

//-----------------------------------------------------------------------------
// Purpose: paints the main background image
//-----------------------------------------------------------------------------
void CBaseModPanel::PaintBackground()
{
	if ( !GameUI().IsInLevel() || m_ExitingFrameCount )
	{
		// not in the game or loading dialog active or exiting, draw the ui background
		DrawBackgroundImage();
	}
	else if ( IsGameConsole() )
	{
		// only valid during loading from level to level
		m_bUseRenderTargetImage = false;
	}

	// [jason] Do not render the background alpha while Scaleform Menus are up
	if ( !BasePanel()->IsScaleformMainMenuActive() && !BasePanel()->IsScaleformPauseMenuActive() )
	{
		if ( m_flBackgroundFillAlpha )
		{
			int swide, stall;
			surface()->GetScreenSize(swide, stall);
			surface()->DrawSetColor(0, 0, 0, m_flBackgroundFillAlpha);
			surface()->DrawFilledRect(0, 0, swide, stall);
		}
	}
}

//-----------------------------------------------------------------------------
// Updates which background state we should be in.
//
// NOTE: These states change at funny times and overlap. They CANNOT be
// used to demarcate exact transitions.
//-----------------------------------------------------------------------------
void CBaseModPanel::UpdateBackgroundState()
{
	if ( m_ExitingFrameCount )
	{
		// trumps all, an exiting state must own the screen image
		// cannot be stopped
		SetBackgroundRenderState( BACKGROUND_EXITING );
	}
	else if ( GameUI().IsInLevel() )
	{
		SetBackgroundRenderState( BACKGROUND_LEVEL );
	}
	else if ( /*GameUI().IsInBackgroundLevel() && */ !m_bLevelLoading )
	{
		// 360 guarantees a progress bar
		// level loading is truly completed when the progress bar is gone, then transition to main menu
		if ( IsPC() || ( IsGameConsole() && !CLoadingScreenScaleform::IsOpen() ) )
		{
			SetBackgroundRenderState( BACKGROUND_MAINMENU );
		}
	}
	else if ( m_bLevelLoading )
	{
		SetBackgroundRenderState( BACKGROUND_LOADING );
	}
	else if ( m_bEverActivated && m_bPlatformMenuInitialized )
	{
		SetBackgroundRenderState( BACKGROUND_DISCONNECTED );
	}

	if ( GameUI().IsConsoleUI() )
	{
		if ( !m_ExitingFrameCount && !m_bLevelLoading && !CLoadingScreenScaleform::IsOpen() && GameUI().IsInLevel() )
		{
			// paused
			if ( m_flBackgroundFillAlpha == 0.0f )
				m_flBackgroundFillAlpha = 120.0f;
		}
		else
		{
			m_flBackgroundFillAlpha = 0;
		}

		// console ui has completely different menu/dialog/fill/fading behavior
		return;
	}

	// don't evaluate the rest until we've initialized the menus
	if ( !m_bPlatformMenuInitialized )
		return;

	// check for background fill
	// fill over the top if we have any dialogs up
	int i;
	bool bHaveActiveDialogs = false;
	bool bIsInLevel = GameUI().IsInLevel();
	for ( i = 0; i < GetChildCount(); ++i )
	{
		VPANEL child = ipanel()->GetChild( GetVPanel(), i );
		if ( child 
			&& ipanel()->IsVisible( child ) 
			&& ipanel()->IsPopup( child )
			&& child != m_pGameMenu->GetVPanel() )
		{
			bHaveActiveDialogs = true;
		}
	}
	// see if the base gameui panel has dialogs hanging off it (engine stuff, console, bug reporter)
	VPANEL parent = GetVParent();
	for ( i = 0; i < ipanel()->GetChildCount( parent ); ++i )
	{
		VPANEL child = ipanel()->GetChild( parent, i );
		if ( child 
			&& ipanel()->IsVisible( child ) 
			&& ipanel()->IsPopup( child )
			&& child != GetVPanel() )
		{
			bHaveActiveDialogs = true;
		}
	}

	// check to see if we need to fade in the background fill
	bool bNeedDarkenedBackground = (bHaveActiveDialogs || bIsInLevel);
	if ( m_bHaveDarkenedBackground != bNeedDarkenedBackground )
	{
		// fade in faster than we fade out
		float targetAlpha, duration;
		if ( bNeedDarkenedBackground )
		{
			// fade in background tint
			targetAlpha = m_BackdropColor[3];
			duration = m_flFrameFadeInTime;
		}
		else
		{
			// fade out background tint
			targetAlpha = 0.0f;
			duration = 2.0f;
		}

		m_bHaveDarkenedBackground = bNeedDarkenedBackground;
		vgui::GetAnimationController()->RunAnimationCommand( this, "m_flBackgroundFillAlpha", targetAlpha, 0.0f, duration, AnimationController::INTERPOLATOR_LINEAR );
	}

	// check to see if the game title should be dimmed
	// don't transition on level change
	if ( m_bLevelLoading )
		return;

	bool bNeedDarkenedTitleText = bHaveActiveDialogs;
	if (m_bHaveDarkenedTitleText != bNeedDarkenedTitleText || m_bForceTitleTextUpdate)
	{
		float targetTitleAlpha, duration;
		if (bHaveActiveDialogs)
		{
			// fade out title text
			duration = m_flFrameFadeInTime;
			targetTitleAlpha = 32.0f;
		}
		else
		{
			// fade in title text
			duration = 2.0f;
			targetTitleAlpha = 255.0f;
		}

		if ( m_pGameLogo )
		{
			vgui::GetAnimationController()->RunAnimationCommand( m_pGameLogo, "alpha", targetTitleAlpha, 0.0f, duration, AnimationController::INTERPOLATOR_LINEAR );
		}

		// Msg( "animating title (%d => %d at time %.2f)\n", m_pGameMenuButton->GetAlpha(), (int)targetTitleAlpha, Plat_FloatTime());
		for ( i=0; i<m_pGameMenuButtons.Count(); ++i )
		{
			vgui::GetAnimationController()->RunAnimationCommand( m_pGameMenuButtons[i], "alpha", targetTitleAlpha, 0.0f, duration, AnimationController::INTERPOLATOR_LINEAR );
		}
		m_bHaveDarkenedTitleText = bNeedDarkenedTitleText;
		m_bForceTitleTextUpdate = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets how the game background should render
//-----------------------------------------------------------------------------
void CBaseModPanel::SetBackgroundRenderState(EBackgroundState state)
{
	if ( state == m_eBackgroundState )
	{
		return;
	}

	// apply state change transition
	float frametime = Plat_FloatTime();

	m_bRenderingBackgroundTransition = false;
	m_bFadingInMenus = false;

	if ( state == BACKGROUND_EXITING )
	{
		// hide the menus
		m_bCopyFrameBuffer = false;
	}
	else if ( state == BACKGROUND_DISCONNECTED || state == BACKGROUND_MAINMENU )
	{
		if ( m_bForceStartScreen )
		{
			m_bForceStartScreen = false;
			HandleOpenCreateStartScreen();
		}
		else
		{
			// [jason] Restore the Scaleform main menu when we return to Front End
			if ( m_eBackgroundState == BACKGROUND_LEVEL || m_eBackgroundState == BACKGROUND_LOADING )
			{
				if ( IsScaleformMainMenuEnabled() )
				{
					// Do not bring main menu up if we're planning to show the start screen as well!
					if ( !IsStartScreenEnabled() )
					{
						ShowMainMenu( false );
						ShowScaleformMainMenu( true );
					}
				}
				else
				{
					ShowMainMenu( true );
				}
			}

			// menu fading
			// make the menus visible
			m_bFadingInMenus = true;
			m_flFadeMenuStartTime = frametime;
			m_flFadeMenuEndTime = frametime + 3.0f;

			if ( state == BACKGROUND_MAINMENU )
			{
				// fade background into main menu
				m_bRenderingBackgroundTransition = true;
				m_flTransitionStartTime = frametime;
				m_flTransitionEndTime = frametime + 3.0f;
			}
		}
	}
	else if ( state == BACKGROUND_LOADING )
	{
		if ( GameUI().IsConsoleUI() )
		{
			RunAnimationWithCallback( this, "InstantHideMainMenu", new KeyValues( "LoadMap" ) );
		}

		// hide the menus
		SetMenuAlpha( 0 );

		// [jason] Ensure we hide the scaleform main menu at this point
		DismissMainMenuScreen();
	}
	else if ( state == BACKGROUND_LEVEL )
	{
		// show the menus
		SetMenuAlpha( 255 );
	}

	m_eBackgroundState = state;
}

void CBaseModPanel::StartExitingProcess()
{
	// must let a non trivial number of screen swaps occur to stabilize image
	// ui runs in a constrained state, while shutdown is occurring
	m_flTransitionStartTime = Plat_FloatTime();
	m_flTransitionEndTime = m_flTransitionStartTime + 0.5f;
	m_ExitingFrameCount = 30;
	g_pInputSystem->DetachFromWindow();

	engine->StartXboxExitingProcess();
}

//-----------------------------------------------------------------------------
// Purpose: Size should only change on first vgui frame after startup
//-----------------------------------------------------------------------------
void CBaseModPanel::OnSizeChanged( int newWide, int newTall )
{
	// Recenter message dialogs
	m_MessageDialogHandler.PositionDialogs( newWide, newTall );
}

//-----------------------------------------------------------------------------
// Purpose: notifications
//-----------------------------------------------------------------------------
void CBaseModPanel::OnLevelLoadingStarted( const char *levelName, bool bShowProgressDialog )
{
	m_bLevelLoading = true;

	// $TODO: Figure out why this causes input lockup when we return to in-level
	// Dismiss any message boxes, options menus, help screens, etc if the level load is kicked off.
	//  This covers cases of the map being loaded via the console window.
	DismissAllMainMenuScreens();

	if ( IsGameConsole() && m_eBackgroundState == BACKGROUND_LEVEL )
	{
		// already in a level going to another level
		// frame buffer is about to be cleared, copy it off for ui backing purposes
		m_bCopyFrameBuffer = true;
	}
	
	// kick off the scaleform screen load if it hasn't been opened yet
	if ( !CLoadingScreenScaleform::IsOpen() )
	{
		if ( levelName )
		{
			if ( !levelName[0] )
			{
				levelName = engine->GetLevelNameShort();
			}

			char levelSettings[1024];

			V_snprintf( levelSettings, ARRAYSIZE(levelSettings),
				" Game { "
					" type %s"
					" mode %s" 
					" map %s" 
				" } "
				,

				g_pGameTypes->GetGameTypeFromInt( g_pGameTypes->GetCurrentGameType() ),
				g_pGameTypes->GetGameModeFromInt( g_pGameTypes->GetCurrentGameType(), g_pGameTypes->GetCurrentGameMode() ),
				levelName
				);

			KeyValues *pSettings = KeyValues::FromString( "Settings", levelSettings );
			KeyValues::AutoDelete autodelete( pSettings );

			CLoadingScreenScaleform::LoadDialogForKeyValues( pSettings );
		}
		else
		{
			// If we don't have a valid level name yet, just open the loading screen and await further info
			CLoadingScreenScaleform::LoadDialog( );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: notification
//-----------------------------------------------------------------------------
void CBaseModPanel::OnLevelLoadingFinished()
{
	// [jason] $FIXME: Switch back to Scaleform, unless we are still using vgui for Pause Menu
	if ( m_bScaleformPauseMenuEnabled )
	{
		ShowMainMenu( false );
	}

	m_bLevelLoading = false;

	UpdateRichPresenceInfo();

	// clear game UI state from previous matches
	if ( GetViewPortInterface() )
	{
		GetViewPortInterface()->ShowPanel( PANEL_ALL, false );
		GetViewPortInterface()->UpdateAllPanels();
	}
}

//-----------------------------------------------------------------------------
// Draws the background image.
//-----------------------------------------------------------------------------
void CBaseModPanel::DrawBackgroundImage()
{
	// [jason] Only render background if the Scaleform main menu is inactive
	if ( IsScaleformMainMenuEnabled() && IsScaleformMainMenuActive() )
		return;

	if ( IsGameConsole() && m_bCopyFrameBuffer )
	{
		// force the engine to do an image capture ONCE into this image's render target
		char filename[MAX_PATH];
		surface()->DrawGetTextureFile( m_iRenderTargetImageID, filename, sizeof( filename ) );
		engine->CopyFrameBufferToMaterial( filename );
		m_bCopyFrameBuffer = false;
		m_bUseRenderTargetImage = true;
	}

	int wide, tall;
	GetSize( wide, tall );

	float frametime = Plat_FloatTime();

	// a background transition has a running map underneath it, so fade image out
	// otherwise, there is no map and the background image stays opaque
	int alpha = 255;
	if ( m_bRenderingBackgroundTransition )
	{
		// goes from [255..0]
		alpha = (m_flTransitionEndTime - frametime) / (m_flTransitionEndTime - m_flTransitionStartTime) * 255;
		alpha = clamp( alpha, 0, 255 );
	}

	// an exiting process needs to opaquely cover everything
	if ( m_ExitingFrameCount )
	{
		// goes from [0..255]
		alpha = (m_flTransitionEndTime - frametime) / (m_flTransitionEndTime - m_flTransitionStartTime) * 255;
		alpha = 255 - clamp( alpha, 0, 255 );
	}

	int iImageID = m_iBackgroundImageID;
	if ( IsGameConsole() )
	{
		if ( m_ExitingFrameCount )
		{
			if ( !m_bRestartSameGame )
			{
				iImageID = m_iProductImageID;
			}
		}
		else if ( m_bUseRenderTargetImage )
		{
			// the render target image must be opaque, the alpha channel contents are unknown
			// it is strictly an opaque background image and never used as an overlay
			iImageID = m_iRenderTargetImageID;
			alpha = 255;
		}
	}

	surface()->DrawSetColor( 255, 255, 255, alpha );
	surface()->DrawSetTexture( iImageID );
	surface()->DrawTexturedRect( 0, 0, wide, tall );

	if ( IsGameConsole() && m_ExitingFrameCount )
	{
		// Make invisible when going back to appchooser
#if defined( __clang__ )
		m_pGameMenu->CGameMenu::BaseClass::SetVisible( false );
#else
		m_pGameMenu->BaseClass::SetVisible( false );
#endif

		IScheme *pScheme = vgui::scheme()->GetIScheme( vgui::scheme()->GetScheme( "SourceScheme" ) );
		HFont hFont = pScheme->GetFont( "ChapterTitle" );
		wchar_t *pString = g_pVGuiLocalize->Find( "#GameUI_Loading" );
		int textWide, textTall;
		surface()->GetTextSize( hFont, pString, textWide, textTall );
		surface()->DrawSetTextPos( ( wide - textWide )/2, tall * 0.50f );
		surface()->DrawSetTextFont( hFont );
		surface()->DrawSetTextColor( 255, 255, 255, alpha );
		surface()->DrawPrintText( pString, wcslen( pString ) );
	}

	// 360 always use the progress bar, TCR Requirement, and never this loading plaque
	if ( IsPC() && ( m_bRenderingBackgroundTransition || m_eBackgroundState == BACKGROUND_LOADING ) )
	{
		// draw the loading image over the top
		surface()->DrawSetColor(255, 255, 255, alpha);
		surface()->DrawSetTexture(m_iLoadingImageID);
		int twide, ttall;
		surface()->DrawGetTextureSize(m_iLoadingImageID, twide, ttall);
		surface()->DrawTexturedRect(wide - twide, tall - ttall, wide, tall);
	}

	// update the menu alpha
    if ( !IsStartScreenEnabled() && !m_bBypassStartScreen && m_bFadingInMenus && !IsScaleformIntroMovieEnabled() )
	{
		if ( GameUI().IsConsoleUI() )
		{
			m_pConsoleAnimationController->StartAnimationSequence( "OpenMainMenu" );
			m_bFadingInMenus = false;
		}
		else
		{
			// goes from [0..255]
			alpha = (frametime - m_flFadeMenuStartTime) / (m_flFadeMenuEndTime - m_flFadeMenuStartTime) * 255;
			alpha = clamp( alpha, 0, 255 );
			m_pGameMenu->SetAlpha( alpha );
			if ( alpha == 255 )
			{
				m_bFadingInMenus = false;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::CreateGameMenu()
{
	// load settings from config file
	KeyValues *datafile = new KeyValues("GameMenu");
	datafile->UsesEscapeSequences( true );	// VGUI uses escape sequences
	if (datafile->LoadFromFile( g_pFullFileSystem, "Resource/GameMenu.res" ) )
	{
		m_pGameMenu = RecursiveLoadGameMenu(datafile);
	}

	if ( !m_pGameMenu )
	{
		Error( "Could not load file Resource/GameMenu.res" );
	}
	else
	{
		// start invisible
		SETUP_PANEL( m_pGameMenu );
		m_pGameMenu->SetAlpha( 0 );
	}

	datafile->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::CreateGameLogo()
{
	if ( ModInfo().UseGameLogo() )
	{
		m_pGameLogo = new CMainMenuGameLogo( this, "GameLogo" );

		if ( m_pGameLogo )
		{
			SETUP_PANEL( m_pGameLogo );
			m_pGameLogo->InvalidateLayout( true, true );

			// start invisible
			m_pGameLogo->SetAlpha( 0 );
		}
	}
	else
	{
		m_pGameLogo = NULL;
	}
}

void CBaseModPanel::CheckBonusBlinkState()
{
#ifdef _GAMECONSOLE
	// On 360 if we have a storage device at this point and try to read the bonus data it can't find the bonus file!
	return;
#endif

	if ( BonusMapsDatabase()->GetBlink() )
	{
		if ( GameUI().IsConsoleUI() )
			SetMenuItemBlinkingState( "OpenNewGameDialog", true );	// Consoles integrate bonus maps menu into the new game menu
		else
			SetMenuItemBlinkingState( "OpenBonusMapsDialog", true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if menu items need to be enabled/disabled
//-----------------------------------------------------------------------------
void CBaseModPanel::UpdateGameMenus()
{
	// check our current state
	bool isInGame = GameUI().IsInLevel();
	bool isMulti = isInGame && (engine->GetMaxClients() > 1);

	// iterate all the menu items
	m_pGameMenu->UpdateMenuItemState( isInGame, isMulti );

	// position the menu
	InvalidateLayout();
	m_pGameMenu->SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: sets up the game menu from the keyvalues
//			the game menu is hierarchial, so this is recursive
//-----------------------------------------------------------------------------
CGameMenu *CBaseModPanel::RecursiveLoadGameMenu(KeyValues *datafile)
{
	CGameMenu *menu = new CGameMenu(this, datafile->GetName());

	// loop through all the data adding items to the menu
	for (KeyValues *dat = datafile->GetFirstSubKey(); dat != NULL; dat = dat->GetNextKey())
	{
		const char *label = dat->GetString("label", "<unknown>");
		const char *cmd = dat->GetString("command", NULL);
		const char *name = dat->GetString("name", label);

		if ( cmd && !Q_stricmp( cmd, "OpenFriendsDialog" ) && bSteamCommunityFriendsVersion )
			continue;

		menu->AddMenuItem(name, label, cmd, this, dat);
	}

	return menu;
}

//-----------------------------------------------------------------------------
// Purpose: Unlock all input, typically when sign-out occurs and we return to
//	the start screen.
//-----------------------------------------------------------------------------
void CBaseModPanel::UnlockInput( void )
{
	m_primaryUserId = -1;

#if defined( _GAMECONSOLE )
	XBX_ResetUserIdSlots();
	XBX_SetPrimaryUserId( XBX_INVALID_USER_ID );
	XBX_SetPrimaryUserIsGuest( 0 );	
	XBX_SetNumGameUsers( 0 ); // users not selected yet
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Lock input to the user who signed-in
//-----------------------------------------------------------------------------
void CBaseModPanel::LockInput( void )
{
#if defined( _GAMECONSOLE )
	// Turn off all controllers
	XBX_ClearUserIdSlots();

	// Configure the game type and controller assignments
	XBX_SetPrimaryUserId( m_primaryUserId );
	XBX_SetPrimaryUserIsGuest( 0 );

	// $TODO: handle guest account, and multiple user sign-in
	XBX_SetUserId( 0, m_primaryUserId );
	XBX_SetUserIsGuest( 0, 0 );

	XBX_SetNumGameUsers( 1 );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Perform tasks required to transition from start screen to main menu
//	via a user Start button -> Sign-in sequence
//-----------------------------------------------------------------------------
void CBaseModPanel::CompleteStartScreenSignIn( void )
{
	// [jason] Lock input first, then dismiss start screen and start the main menu
	LockInput();

	m_bStartScreenPlayerSigninCompleted = false;
	m_bWaitingForUserSignIn = false;
	m_bUserRefusedSignIn = false;

	DismissStartScreen();

	m_bShowStartScreen = false;

#if defined( _GAMECONSOLE )
	// Set the local player's name in the game.
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
	int userID = XBX_GetActiveUserId();
	bool bValidUserName = false;
	char* pUserName = NULL;

#if defined ( _X360 )
	char szGamertag[MAX_PLAYER_NAME_LENGTH];
	if (( userID != INVALID_USER_ID ) && 
		( XUserGetSigninState( userID ) != eXUserSigninState_NotSignedIn ))
	{
		if(XUserGetName( userID, szGamertag, MAX_PLAYER_NAME_LENGTH ) != ERROR_SUCCESS)
		{
			Error( "CompleteStartScreenSignIn: error getting Xbox 360 user name.\n" );
			V_strncpy( szGamertag, "unknown", sizeof( szGamertag ) );
		}
		bValidUserName = true;
		pUserName = szGamertag;
	}
#elif defined ( _PS3 )
	if ( userID != INVALID_USER_ID )
	{
		s_userStat.id = CELL_SYSUTIL_USERID_CURRENT;
		if(cellUserInfoGetStat(CELL_SYSUTIL_USERID_CURRENT,&s_userStat) != CELL_USERINFO_RET_OK)
		{
			Error( "CompleteStartScreenSignIn: error getting PS3 user name.\n" );
			V_strncpy( s_userStat.name, "unknown", CELL_USERINFO_USERNAME_SIZE );
		}
		bValidUserName = true;
		pUserName = s_userStat.name;
	}
#endif
	if(bValidUserName)
	{
		ConVarRef cl_name( "name" );
		cl_name.SetValue( pUserName );
	}
#endif // _GAMECONSOLE

	if ( IsScaleformMainMenuEnabled() )
	{
		// Display the scaleform main menu now
		OnOpenCreateMainMenuScreen( );
	}
	else
	{
		// Otherwise, bring up the vgui main menu now
		ShowMainMenu( true );
	}
	
#if defined( _GAMECONSOLE )
	// OnProfilesChanged triggers g_pPlayerManager->OnGameUsersChanged() which creates local players in matchmaking; this is called from CMatchFramework::Connect on PC only
	//  so we dont want to call  g_pPlayerManager->OnGameUsersChanged() twice on PC but needs to be called once on GAMECONSOLE so we BroadcastEvent here only if defined _GAMECONSOLE
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfilesChanged", "numProfiles", (int) XBX_GetNumGameUsers() ) );
#endif

	UpdateRichPresenceInfo();
}

//-----------------------------------------------------------------------------
// Purpose: update the taskbar a frame
//-----------------------------------------------------------------------------
// exposed here as non-constant so CEG can populate the value at DLL init time
static DWORD CEG_ALLOW_PROPER_TINT = 0xFEA4; // will override 

CEG_NOINLINE DWORD InitUiAllowProperTintFlag( void )
{
	CEG_GCV_PRE();
	CEG_ALLOW_PROPER_TINT = CEG_GET_CONSTANT_VALUE( UiAllowProperTintFlag );
	CEG_GCV_POST();

	return CEG_ALLOW_PROPER_TINT;
}


int CBaseModPanel::CheckForAnyKeyPressed( bool bCheckKeyboard )
{

// a macro to make the code below a little cleaner and easier to read
#define TEST_BUTTON( x ) \
do {\
    ButtonCode_t button = ( x );\
    if ( g_pInputSystem->IsButtonDown( button ) )\
    {\
        return button;\
    }\
} while( false )


    for ( int i = 0; i < XUSER_MAX_COUNT; i++ )
    {
        TEST_BUTTON( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_START, i ) );
        TEST_BUTTON( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_BACK, i ) );
        TEST_BUTTON( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, i ) );
        TEST_BUTTON( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, i ) );
        TEST_BUTTON( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_X, i ) );
        TEST_BUTTON( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_Y, i ) );
    }


    // Also, on PC/PS3, we allow the following keys to skip the Start Screen:
    if ( bCheckKeyboard )
    {
        TEST_BUTTON( KEY_SPACE );
        TEST_BUTTON( KEY_ENTER );
        TEST_BUTTON( KEY_ESCAPE );
    }

    return -1;

}

void CBaseModPanel::RunFrame()
{
	InvalidateLayout();
	vgui::GetAnimationController()->UpdateAnimations( Plat_FloatTime() );

	BaseModUI::CUIGameData::Get()->RunFrame();

	// CEG checks failing = Really awful looking UI
	if ( ~CEG_ALLOW_PROPER_TINT & ALLOW_PROPER_TINT_FLAG )
	{
		static ConVarRef sf_ui_tint_munge( "sf_ui_tint" );
		sf_ui_tint_munge.SetValue( 0x10 );
	}

	// Tick all screens that need to update synchronously
	UpdateLeaderboardsDialog();
	UpdateLobbyScreen();
	UpdateLobbyBrowser();
	UpdateMainMenuScreen();

    if ( IsScaleformIntroMovieEnabled() )
    {
        if ( m_bIntroMovieWaitForButtonToClear )
        {
            if ( !g_pInputSystem->IsButtonDown( ( ButtonCode_t ) m_iIntroMovieButtonPressed ) )
            {
                m_bIntroMovieWaitForButtonToClear = false;
                m_iIntroMovieButtonPressed = -1;
            }
        }
        else if ( m_iIntroMovieButtonPressed == -1 )
        {
            m_iIntroMovieButtonPressed = CheckForAnyKeyPressed( !IsX360() );
        }
        else if ( !g_pInputSystem->IsButtonDown( ( ButtonCode_t ) m_iIntroMovieButtonPressed ) )
        {
            DismissScaleformIntroMovie();
            m_iIntroMovieButtonPressed = -1;
        }
    }
    else if ( IsStartScreenEnabled() )
	{
		// If we're flagged to bypass the screen, just treat it as if sign-in naturally occurred
		if ( m_bBypassStartScreen )
		{
			m_primaryUserId = 0;
			CompleteStartScreenSignIn();
		}
		// Poll for button press to activate the game from Start Screen:
		else if ( !m_bWaitingForUserSignIn && m_bStatsLoaded )
		{
			// If for any reason main menu has snuck in behind us, make sure it is put away
			DismissMainMenuScreen();

			bool bStartPressed = false;
			bool bSignedIn = true;
			int  UserIdPressedStart = -1;

			// Any joystick can press Start initially
			for ( int i = 0; i < XUSER_MAX_COUNT && !bStartPressed; i++ )
			{
				if ( g_pInputSystem->IsButtonDown( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_START, i ) ) ||
					 g_pInputSystem->IsButtonDown( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, i ) ) )
				{

#if defined( _X360 )
					// If this user isn't signed in, we need to prompt them for it
					uint state = XUserGetSigninState( i );
					if ( state != eXUserSigninState_NotSignedIn )
					{
						m_primaryUserId = i;
					}
					else
					{
						bSignedIn = false;
					}
#elif defined( _PS3 )
					m_primaryUserId = i;
#endif

					UserIdPressedStart = i;
					bStartPressed = true;
				}
			}
	
			// Also, on PC/PS3, we allow the following keys to skip the Start Screen:
			if ( !bStartPressed && !IsX360() )
			{
				bStartPressed = (	g_pInputSystem->IsButtonDown( KEY_LBRACKET ) || 
									g_pInputSystem->IsButtonDown( KEY_SPACE ) || 
									g_pInputSystem->IsButtonDown( KEY_ENTER ) );
#ifdef _PS3	
				m_primaryUserId = 0;
				UserIdPressedStart = 0;
#endif
			}
			
#if !defined( _CERT )
			// Skip the start screen when running test scripts
			static ConVarRef testscript_running( "testscript_running");
			if ( bSignedIn && testscript_running.GetBool() )
			{
				bStartPressed = true;
				m_primaryUserId = 0;
				UserIdPressedStart = 0;
			}
#endif

			if ( bStartPressed )
			{
				vgui::surface()->PlaySound( "UI\\buttonclick.wav" );
				// FIXME: Eventually, we'll have the correct signin for all platforms, so switch over to it then
				if ( !bSignedIn && IsX360() )
				{
					if ( UserIdPressedStart > -1 )
					{
						m_primaryUserId = UserIdPressedStart;
						LockInput();
					}
					SignInFromStartScreen();
				}
				else
				{
					CompleteStartScreenSignIn();
				}
			}
		}
		
		// Handle the Start Screen sign-in completion on the next tick, so we have a frame to finish rendering the blade and Start Screen outro
		if ( m_bStartScreenPlayerSigninCompleted )
		{
			CompleteStartScreenSignIn();
		}
	}

	if ( GameUI().IsConsoleUI() )
	{
		// run the console ui animations
		m_pConsoleAnimationController->UpdateAnimations( Plat_FloatTime() );

		if ( IsGameConsole() && m_ExitingFrameCount && Plat_FloatTime() >= m_flTransitionEndTime )
		{
			if ( m_ExitingFrameCount > 1 )
			{
				m_ExitingFrameCount--;
				if ( m_ExitingFrameCount == 1 )
				{
					// enough frames have transpired, send the single shot quit command
					// If we kicked off this event from an invite, we need to properly setup the restart to account for that
					if ( m_bRestartFromInvite )
					{
						engine->ClientCmd_Unrestricted( "quit_gameconsole invite" );
					}
					else if ( m_bRestartSameGame )
					{
						engine->ClientCmd_Unrestricted( "quit_gameconsole restart" );
					}
					else
					{
						// quits to appchooser
						engine->ClientCmd_Unrestricted( "quit_gameconsole\n" );
					}
				}
			}
		}
	}

	UpdateBackgroundState();

	if ( !m_bPlatformMenuInitialized )
	{
		// check to see if the platform is ready to load yet
		if ( IsGameConsole() || g_VModuleLoader.IsPlatformReady() )
		{
			m_bPlatformMenuInitialized = true;
		}
	} 

	// Check to see if a pending async task has already finished
	if ( m_pAsyncJob && !m_pAsyncJob->m_hThreadHandle )
	{
		m_pAsyncJob->Completed();
		delete m_pAsyncJob;
		m_pAsyncJob = NULL;
	}
}

#if defined( PLATFORM_X360 )
static inline XUSER_CONTEXT CreateContextStruct( DWORD dwContextId, DWORD dwValue )
{
	XUSER_CONTEXT xUserContext = { dwContextId, dwValue };
	return xUserContext;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Tells XBox Live our user is in the current game's menu
//-----------------------------------------------------------------------------
void CBaseModPanel::UpdateRichPresenceInfo()
{
#if defined( _X360 )
	// For all other users logged into this console (not primary), set to idle to satisfy cert
	for( int i = 0; i < XUSER_MAX_COUNT; ++i )
	{
		XUSER_SIGNIN_STATE State = XUserGetSigninState( i );

		if ( State != eXUserSigninState_NotSignedIn )
		{
			// Check if they are one of our active users
			bool isActive = false;
			for ( unsigned int k = 0; k < XBX_GetNumGameUsers(); ++ k )
			{
				if ( XBX_GetUserId( k ) == i )
				{
					isActive = true;
					break;
				}
			}

			if ( !isActive )
			{
				// Set rich presence as 'idle' for users logged in that can't participate.
				//DevMsg( "Set presence to %d for user %d\n", CONTEXT_PRESENCE_IDLE, i );
				if ( !xboxsystem->UserSetContext( i, CreateContextStruct( X_CONTEXT_PRESENCE, CONTEXT_PRESENCE_IDLE ), true ) )
				{
					Warning( "BasePanel: UserSetContext failed.\n" );
				}
			}
		}
	}

	// If we're not in the level we set the presence to main menu
	if ( !GameUI().IsInLevel() )
	{
		for ( unsigned int k = 0; k < XBX_GetNumGameUsers(); ++ k )
		{
			if ( XBX_GetUserIsGuest( k ) )
				continue;
			int iCtrlr = XBX_GetUserId( k );

			// [jmh] When user is not in a level, they're either in main menu or in a team lobby
			DWORD rpContext = InTeamLobby() ? CONTEXT_PRESENCE_LOBBY : CONTEXT_PRESENCE_MAINMENU;

			//DevMsg( "Set presence to %d for user %d\n", rpContext, iCtrlr );
			if ( !xboxsystem->UserSetContext( iCtrlr, CreateContextStruct( X_CONTEXT_PRESENCE, rpContext ), true ) )
			{
				Warning( "BasePanel: UserSetContext failed.\n" );
			}
		}
	}
	else if ( m_bSinglePlayer )
	{
		// We're in a single player so set the presence to single player for all active users
		for ( unsigned int k = 0; k < XBX_GetNumGameUsers(); ++ k )
		{
			if ( XBX_GetUserIsGuest( k ) )
				continue;
			int iCtrlr = XBX_GetUserId( k );

			//DevMsg( "Set presence to %d for user %d\n", CONTEXT_PRESENCE_SINGLEPLAYER, iCtrlr );
			if ( !xboxsystem->UserSetContext( iCtrlr, CreateContextStruct( X_CONTEXT_PRESENCE, CONTEXT_PRESENCE_SINGLEPLAYER ), true ) )
			{
				Warning( "BasePanel: UserSetContext failed.\n" );
			}
		}
	}
#endif // _X360
}

//-----------------------------------------------------------------------------
// Purpose: Lays out the position of the taskbar
//-----------------------------------------------------------------------------
void CBaseModPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	// Get the screen size
	int wide, tall;
	vgui::surface()->GetScreenSize(wide, tall);

	// Get the size of the menu
	int menuWide, menuTall;
	m_pGameMenu->GetSize( menuWide, menuTall );

	int idealMenuY = m_iGameMenuPos.y;
	if ( idealMenuY + menuTall + m_iGameMenuInset > tall )
	{
		idealMenuY = tall - menuTall - m_iGameMenuInset;
	}

	int yDiff = idealMenuY - m_iGameMenuPos.y;

	for ( int i=0; i<m_pGameMenuButtons.Count(); ++i )
	{
		// Get the size of the logo text
		//int textWide, textTall;
		m_pGameMenuButtons[i]->SizeToContents();
		//vgui::surface()->GetTextSize( m_pGameMenuButtons[i]->GetFont(), ModInfo().GetGameTitle(), textWide, textTall );

		// place menu buttons above middle of screen
		m_pGameMenuButtons[i]->SetPos(m_iGameTitlePos[i].x, m_iGameTitlePos[i].y + yDiff);
		//m_pGameMenuButtons[i]->SetSize(textWide + 4, textTall + 4);
	}

	if ( m_pGameLogo )
	{
		// move the logo to sit right on top of the menu
		m_pGameLogo->SetPos( m_iGameMenuPos.x + m_pGameLogo->GetOffsetX(), idealMenuY - m_pGameLogo->GetTall() + m_pGameLogo->GetOffsetY() );
	}

	// position self along middle of screen
	if ( GameUI().IsConsoleUI() )
	{
		int posx, posy;
		m_pGameMenu->GetPos( posx, posy );
		m_iGameMenuPos.x = posx;
	}
	m_pGameMenu->SetPos(m_iGameMenuPos.x, idealMenuY);

	UpdateGameMenus();
}

//-----------------------------------------------------------------------------
// Purpose: Loads scheme information
//-----------------------------------------------------------------------------
void CBaseModPanel::ApplySchemeSettings(IScheme *pScheme)
{
	int i;
	BaseClass::ApplySchemeSettings(pScheme);

	m_iGameMenuInset = atoi(pScheme->GetResourceString("MainMenu.Inset"));
	m_iGameMenuInset *= 2;

	IScheme *pClientScheme = vgui::scheme()->GetIScheme( vgui::scheme()->GetScheme( "ClientScheme" ) );
	CUtlVector< Color > buttonColor;
	if ( pClientScheme )
	{
		m_iGameTitlePos.RemoveAll();
		for ( i=0; i<m_pGameMenuButtons.Count(); ++i )
		{
			m_pGameMenuButtons[i]->SetFont(pClientScheme->GetFont("ClientTitleFont", true));
			m_iGameTitlePos.AddToTail( coord() );
			m_iGameTitlePos[i].x = atoi(pClientScheme->GetResourceString( CFmtStr( "Main.Title%d.X", i+1 ) ) );
			m_iGameTitlePos[i].x = scheme()->GetProportionalScaledValue( m_iGameTitlePos[i].x );
			m_iGameTitlePos[i].y = atoi(pClientScheme->GetResourceString( CFmtStr( "Main.Title%d.Y", i+1 ) ) );
			m_iGameTitlePos[i].y = scheme()->GetProportionalScaledValue( m_iGameTitlePos[i].y );

			if ( GameUI().IsConsoleUI() )
				m_iGameTitlePos[i].x += MAIN_MENU_INDENT_X360;

			buttonColor.AddToTail( pClientScheme->GetColor( CFmtStr( "Main.Title%d.Color", i+1 ), Color(255, 255, 255, 255)) );
		}
#ifdef CS_BETA
		if ( !ModInfo().NoCrosshair() ) // hack to not show the BETA for HL2 or HL1Port
		{
			m_pGameMenuButtons[m_pGameMenuButtons.Count()-1]->SetFont(pClientScheme->GetFont("BetaFont", true));
		}
#endif // CS_BETA

		m_iGameMenuPos.x = atoi(pClientScheme->GetResourceString("Main.Menu.X"));
		m_iGameMenuPos.x = scheme()->GetProportionalScaledValue( m_iGameMenuPos.x );
		m_iGameMenuPos.y = atoi(pClientScheme->GetResourceString("Main.Menu.Y"));
		m_iGameMenuPos.y = scheme()->GetProportionalScaledValue( m_iGameMenuPos.y );

		m_iGameMenuInset = atoi(pClientScheme->GetResourceString("Main.BottomBorder"));
		m_iGameMenuInset = scheme()->GetProportionalScaledValue( m_iGameMenuInset );
	}
	else
	{
		for ( i=0; i<m_pGameMenuButtons.Count(); ++i )
		{
			m_pGameMenuButtons[i]->SetFont(pScheme->GetFont("TitleFont"));
			buttonColor.AddToTail( Color( 255, 255, 255, 255 ) );
		}
	}

	for ( i=0; i<m_pGameMenuButtons.Count(); ++i )
	{
		m_pGameMenuButtons[i]->SetDefaultColor(buttonColor[i], Color(0, 0, 0, 0));
		m_pGameMenuButtons[i]->SetArmedColor(buttonColor[i], Color(0, 0, 0, 0));
		m_pGameMenuButtons[i]->SetDepressedColor(buttonColor[i], Color(0, 0, 0, 0));
	}

	m_flFrameFadeInTime = atof(pScheme->GetResourceString("Frame.TransitionEffectTime"));

	// work out current focus - find the topmost panel
	SetBgColor(Color(0, 0, 0, 0));

	m_BackdropColor = pScheme->GetColor("mainmenu.backdrop", Color(0, 0, 0, 128));

	char filename[MAX_PATH];
	if ( IsGameConsole() )
	{
		// 360 uses FullFrameFB1 RT for map to map transitioning
		m_iRenderTargetImageID = surface()->CreateNewTextureID();
		surface()->DrawSetTextureFile( m_iRenderTargetImageID, "console/rt_background", false, false );
	}

	int screenWide, screenTall;
	surface()->GetScreenSize( screenWide, screenTall );
	float aspectRatio = (float)screenWide/(float)screenTall;
	bool bIsWidescreen = aspectRatio >= 1.5999f;

	// work out which background image to use
	if ( IsPC() || !IsGameConsole() )
	{
		// pc uses blurry backgrounds based on the background level
		char background[MAX_PATH];
		engine->GetMainMenuBackgroundName( background, sizeof(background) );
		Q_snprintf( filename, sizeof( filename ), "console/%s%s", background, ( bIsWidescreen ? "_widescreen" : "" ) );
	}
	else
	{
		// 360 uses hi-res game specific backgrounds
		char gameName[MAX_PATH];
		const char *pGameDir = engine->GetGameDirectory();
		V_FileBase( pGameDir, gameName, sizeof( gameName ) );
		V_snprintf( filename, sizeof( filename ), "vgui/appchooser/background_%s%s", gameName, ( bIsWidescreen ? "_widescreen" : "" ) );
	}
	m_iBackgroundImageID = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile( m_iBackgroundImageID, filename, false, false );

	if ( IsGameConsole() )
	{
		// 360 uses a product image during application exit
		V_snprintf( filename, sizeof( filename ), "vgui/appchooser/background_orange%s", ( bIsWidescreen ? "_widescreen" : "" ) );
		m_iProductImageID = surface()->CreateNewTextureID();
		surface()->DrawSetTextureFile( m_iProductImageID, filename, false, false );
	}

	if ( IsPC() )
	{
		// load the loading icon
		m_iLoadingImageID = surface()->CreateNewTextureID();
		surface()->DrawSetTextureFile( m_iLoadingImageID, "console/startup_loading", false, false );
	}

	// Load the version numers
	LoadVersionNumbers();
}

//-----------------------------------------------------------------------------
// Purpose: message handler for platform menu; activates the selected module
//-----------------------------------------------------------------------------
void CBaseModPanel::OnActivateModule(int moduleIndex)
{
	g_VModuleLoader.ActivateModule(moduleIndex);
}

void CBaseModPanel::CreateStartScreenIfNeeded( void )
{
	// [jason] If we have Start Screen enabled, present it for the first time
	if ( IsStartScreenEnabled() )
	{
		ShowMainMenu( false );

		// We don't do this step if the platform doesn't use Start Screen to bind the default profile/controller
		if ( !m_bBypassStartScreen )
		{
			// we're awaiting a start input to lock the controller and default profile
			UnlockInput();

			m_bStartScreenPlayerSigninCompleted = false;
			OnOpenCreateStartScreen();
		}
	}
	else
	{
		if ( GameUI().IsConsoleUI() )
		{
			ArmFirstMenuItem();
		}
	}
}



//-----------------------------------------------------------------------------
// Purpose: Animates menus on gameUI being shown
//-----------------------------------------------------------------------------

void CBaseModPanel::OnGameUIActivated()
{
	// If the load failed, we're going to bail out here
	if ( engine->MapLoadFailed() )
	{
		// Don't display this again until it happens again
		engine->SetMapLoadFailed( false );
		ShowMessageDialog( MD_LOAD_FAILED_WARNING );
	}

	if ( !m_bEverActivated )
	{
		// Layout the first time to avoid focus issues (setting menus visible will grab focus)
		UpdateGameMenus();
		m_bEverActivated = true;

#if defined( _GAMECONSOLE )
#pragma message( __FILE__ "(" __LINE__AS_STRING ") : warning custom: Slamming controller for xbox storage id to 0" )
		
		// Open all active containers if we have a valid storage device
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
		if ( XBX_GetActiveUserId() != XBX_INVALID_USER_ID && 
			 XBX_GetStorageDeviceId( 0 ) != XBX_INVALID_STORAGE_ID && 
			 XBX_GetStorageDeviceId( 0 ) != XBX_STORAGE_DECLINED )
		{
			// Open user settings and save game container here
			uint nRet = engine->OnStorageDeviceAttached( 0 );
			if ( nRet != ERROR_SUCCESS )
			{
				// Invalidate the device
				XBX_SetStorageDeviceId( XBX_GetActiveUserId(), XBX_INVALID_STORAGE_ID );

				// FIXME: We don't know which device failed!
				// Pop a dialog explaining that the user's data is corrupt
				BasePanel()->ShowMessageDialog( MD_STORAGE_DEVICES_CORRUPT );
			}
		}

#if 0
		// determine if we're starting up because of a cross-game invite
		int fLaunchFlags = XboxLaunch()->GetLaunchFlags();
		if ( fLaunchFlags & LF_INVITERESTART )
		{
			XNKID nSessionID;
			XboxLaunch()->GetInviteSessionID( &nSessionID );
#pragma message( __FILE__ "(" __LINE__AS_STRING ") : warning custom: Leaving invite acceptance restart behavior broken" )
			//matchmaking->JoinInviteSessionByID( nSessionID );
		}
#endif
#endif

		// Brute force check to open tf matchmaking ui.
		if ( GameUI().IsConsoleUI() )
		{
			const char *pGame = engine->GetGameDirectory();
			if ( !Q_stricmp( Q_UnqualifiedFileName( pGame ), "tf" ) )
			{
				m_bUseMatchmaking = true;
				RunMenuCommand( "OpenMatchmakingBasePanel" );
			}
		}
	}

    if ( IsScaleformIntroMovieEnabled() )
    {
        CreateScaleformIntroMovie();
    }
    else
    {
        CreateStartScreenIfNeeded();
    }

	if ( GameUI().IsInLevel() )
	{
		if ( !m_bUseMatchmaking )
		{
			// Decouple the pause menu from the console - don't pause just because we're opening the console window
			static ConVarRef cv_console_window_open( "console_window_open" );
			if ( !IsPC() || !cv_console_window_open.GetBool() )
			{
				if (m_bScaleformPauseMenuEnabled)
				{
					OnOpenPauseMenu();
				}
				else
				{
					OnCommand( "OpenPauseMenu" );
				}
			}
		}
 		else
		{
			RunMenuCommand( "OpenMatchmakingBasePanel" );
 		}

		//if ( m_hAchievementsDialog.Get() )
		//{
		//	// Achievement dialog refreshes it's data if the player looks at the pause menu
		//	m_hAchievementsDialog->OnCommand( "OnGameUIActivated" );
		//}
	}
	else // not the pause menu, update presence
	{
		// [jason] Safety check: If we're not in level, be sure that the pause menu is hidden
		if ( IsScaleformPauseMenuActive() )
		{
			DismissPauseMenu();
		}
		
		// [jason] If we are bringing the main menu UI up again (not start screen or loading) then ensure we have raised the main menu
		if ( !m_bLevelLoading && !IsScaleformIntroMovieEnabled() &&
			!IsStartScreenActive() && 
			IsScaleformMainMenuEnabled() && !IsScaleformMainMenuActive() )
		{
			ShowMainMenu( false );
			ShowScaleformMainMenu( true );
		}

		if ( IsGameConsole() )
		{
			UpdateRichPresenceInfo();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Determine if a menu command is bringing up vgui on top of Scaleform
//-----------------------------------------------------------------------------
static bool IsVguiCommandOverScaleform(const char *command)
{
	// If Scaleform isn't enabled and active for front-end/in-game, then this doesn't require any special handling
	if ( GameUI().IsInLevel() )
	{
		if ( !BasePanel()->IsScaleformPauseMenuEnabled() )
		{
			return false;
		}
	}
	else
	{
		if ( !BasePanel()->IsScaleformMainMenuEnabled( ) )
		{
			return false;
		}
	}

	// // [jason] $TODO: Remove these dialogs from the list, as they are adapted to Scaleform :)
	if ( !Q_stricmp( command, "OpenOptionsDialog" ) )
		 return true;

	return false;
}

void CBaseModPanel::RunSlottedMenuCommand( int slot, const char* command )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( slot );

	RunMenuCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: executes a menu command
//-----------------------------------------------------------------------------
void CBaseModPanel::RunMenuCommand(const char *command)
{
	// [jason] Allow the root vgui panel to display, so we can see the dialog for this command
	if ( IsVguiCommandOverScaleform( command ) )
	{
		if ( IsPC() )
		{
			// Ensure windows messages go to VGui correctly
			g_pMatSystemSurface->EnableWindowsMessages( true );
		}

		ShowMainMenu( true );
	}

	STEAMWORKS_SELFCHECK_AMORTIZE( 11 );

	if ( !Q_stricmp( command, "OpenGameMenu" ) )
	{
		if ( m_pGameMenu )
		{
			PostMessage( m_pGameMenu, new KeyValues("Command", "command", "Open") );
		}
	}
	else if ( !Q_stricmp( command, "OpenPlayerListDialog" ) )
	{
		OnOpenPlayerListDialog();
	}
	else if ( !Q_stricmp( command, "OpenNewGameDialog" ) )
	{
		OnOpenNewGameDialog();
	}
	else if ( !Q_stricmp( command, "OpenLoadGameDialog" ) )
	{
		if ( !GameUI().IsConsoleUI() )
		{
			OnOpenLoadGameDialog();
		}
		else
		{
			OnOpenLoadGameDialog_Xbox();
		}
	}
	else if ( !Q_stricmp( command, "OpenSaveGameDialog" ) )
	{
		if ( !GameUI().IsConsoleUI() )
		{
			OnOpenSaveGameDialog();
		}
		else
		{
			OnOpenSaveGameDialog_Xbox();
		}
	}
	else if ( !Q_stricmp( command, "OpenBonusMapsDialog" ) )
	{
		OnOpenBonusMapsDialog();
	}
	else if ( !Q_stricmp( command, "OpenOptionsDialog" ) )
	{
		if ( !GameUI().IsConsoleUI() )
		{
			OnOpenOptionsDialog();
		}
		else
		{
			OnOpenOptionsDialog_Xbox();
		}
	}
	else if ( !Q_stricmp( command, "OpenControllerDialog" ) )
	{
		OnOpenControllerDialog();
	}
	else if ( !Q_stricmp( command, "OpenMouseDialog" ) )
	{
		OnOpenMouseDialog();
	}
	else if ( !Q_stricmp( command, "OpenKeyboardDialog" ) )
	{
		OnOpenKeyboardDialog();
	}
	else if ( !Q_stricmp( command, "OpenMotionControllerMoveDialog" ) )
	{
		OnOpenMotionControllerMoveDialog();
	}
	else if ( !Q_stricmp( command, "OpenMotionControllerSharpshooterDialog" ) )
	{
		OnOpenMotionControllerSharpshooterDialog();
	}
	else if ( !Q_stricmp( command, "OpenMotionControllerDialog" ) )
	{
		OnOpenMotionControllerDialog();
	}
	else if ( !Q_stricmp( command, "OpenMotionCalibrationDialog" ) )
	{
		OnOpenMotionCalibrationDialog();
	}
	else if ( !Q_stricmp( command, "OpenVideoSettingsDialog" ) )
	{
		OnOpenVideoSettingsDialog();
	}
	else if ( !Q_stricmp( command, "OpenOptionsQueued" ) )
	{
		OnOpenOptionsQueued();
	}
	else if ( !Q_stricmp( command, "OpenAudioSettingsDialog" ) )
	{
		OnOpenAudioSettingsDialog();
	}
	else if ( !Q_stricmp( command, "OpenSettingsDialog" ) )
	{
		OnOpenSettingsDialog();
	}
	else if ( !Q_stricmp( command, "MakeGamePublic" ) )
	{
		OnMakeGamePublic();
	}
	else if ( !Q_stricmp( command, "OpenBenchmarkDialog" ) )
	{
		OnOpenBenchmarkDialog();
	}
	else if ( !Q_stricmp( command, "OpenServerBrowser" ) )
	{
		OnOpenServerBrowser();
	}
	else if ( !Q_stricmp( command, "CloseServerBrowser" ) )
	{
		OnCloseServerBrowser();
	}
	else if ( !Q_stricmp( command, "OpenCreateStartScreen" ) )
	{
		HandleOpenCreateStartScreen();
	}       
	else if ( !V_stricmp( command, "RestoreTopLevelMenu" ) )
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
    else if ( !V_stricmp( command, "FinishedIntroMovie" ) )
    {
        CreateStartScreenIfNeeded();
    }
#if defined( _X360 )	
	else if ( !Q_stricmp( command, "ShowInvitePartyUI" ) )
	{		
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );		
		if ( XShowPartyUI( XBX_GetActiveUserId() ) != ERROR_SUCCESS )
		{
			AssertMsg( false, "XBX_GetActiveUserId failed" );
		}
	}
	else if ( !Q_stricmp( command, "ShowJoinPartyUI" ) )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );		
		if ( XShowCommunitySessionsUI( XBX_GetActiveUserId(), XSHOWCOMMUNITYSESSION_SHOWPARTY ) != ERROR_SUCCESS )
		{
			AssertMsg( false, "XShowCommunitySessionsUI failed" );
		}
	}
	else if ( !Q_stricmp( command, "ShowInviteFriendsUI" ) )
	{		
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
		if ( XShowFriendsUI( XBX_GetActiveUserId() ) != ERROR_SUCCESS )
		{
			AssertMsg( false, "XBX_GetActiveUserId failed" );
		}	
	}
	else if ( !Q_stricmp( command, "ShowLobbyUI" ) )
	{
		// Create the lobby UI, in this case we are launching it so we create it as the Host view
		OnOpenCreateLobbyScreen( true );
	}

	else if ( !Q_stricmp( command, "ShowLobbyBrowser" ) )
	{
		OnOpenLobbyBrowserScreen( true );
	}

#else // _X360
	else if ( !Q_stricmp( command, "ShowInviteFriendsUI" ) )
	{
#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM ) // Steam overlay not available on console (PS3)
		steamapicontext->SteamFriends()->ActivateGameOverlay( "LobbyInvite" );
#endif
	}
	else if ( !Q_stricmp( command, "ShowJoinPartyUI" ) )
	{		
		OnOpenCreateLobbyScreen( true );
	}
	else if ( !Q_stricmp( command, "ShowLobbyUI" ) )
	{
		// Create the lobby UI, in this case we are launching it so we create it as the Host view
		OnOpenCreateLobbyScreen( true );
	}
#endif // !_X360          					   
	else if ( !Q_stricmp( command, "OpenCreateSinglePlayerGameDialog" ) || !Q_stricmp( command, "OpenCreateSinglePlayerGameDialog_AcceptNotConnectedToLive" ) )
	{
		if ( IsX360() )
		{
			//if ( !m_bHasConnectionToLive && !Q_stricmp( command, "OpenCreateSinglePlayerGameDialog" ) )
			//	ShowMessageDialog( MD_PROMPT_LEADERBOARD_LOST_CONNECTION_STARTING_SINGLEPLAYER );
			//else
				OnOpenCreateSingleplayerGameDialog( false );
		}
		else
		{
			OnOpenCreateSingleplayerGameDialog( false );
		}
	}
	else if ( !Q_stricmp( command, "OpenLeaderboardsDialog" ) )
	{
		OnOpenLeaderboardsDialog();
	}
	else if ( !Q_stricmp( command, "OpenCallVoteDialog" ) )
	{
		OnOpenCallVoteDialog();
	}
	else if ( !V_stricmp( command, "OpenMarketplaceDialog" ) )
	{
		OnOpenMarketplace();
	}
	else if ( !V_stricmp( command, "OpenUpsellDialog" ) )
	{
		OnOpenUpsellDialog();
	}
	else if ( !V_stricmp( command, "PlayCreditsVideo" ) )
	{
		OnPlayCreditsVideo();
	}
	else if ( !V_stricmp( command, "RestoreMainMenu" ) )
	{
		RestoreMainMenuScreen();
	}
	else if ( !Q_stricmp( command, "CloseLeaderboardsDialog" ) )
	{
		CloseLeaderboardsDialog();
	}
	else if ( !Q_stricmp( command, "OpenFriendsDialog" ) )
	{
		OnOpenFriendsDialog();
	}
	else if ( !Q_stricmp( command, "OpenLoadDemoDialog" ) )
	{
		OnOpenDemoDialog();
	}
	else if ( !Q_stricmp( command, "OpenCreateMultiplayerGameDialog" ) )
	{
		if ( !ui_test_community_matchmaking.GetBool() )
			OnOpenCreateMultiplayerGameDialog();
		else
			OnOpenCreateMultiplayerGameCommunity();
	}
	else if ( !Q_stricmp( command, "OpenCreateMultiplayerGameCommunity" ) )
	{
		OnOpenCreateMultiplayerGameCommunity();
	}
	else if ( !Q_stricmp( command, "OpenMedalsDialog" ) )
	{
		OnOpenMedalsDialog();
	}
	else if ( !Q_stricmp( command, "OpenStatsDialog" ) )
	{
		OnOpenStatsDialog();
	}
	else if ( !Q_stricmp( command, "CloseMedalsStatsDialog" ) )
	{
		CloseMedalsStatsDialog();
	}
	else if ( !Q_stricmp( command, "OpenChangeGameDialog" ) )
	{
		OnOpenChangeGameDialog();
	}
	else if ( !Q_stricmp( command, "OpenLoadCommentaryDialog" ) )
	{
		OnOpenLoadCommentaryDialog();	
	}
	else if ( !Q_stricmp( command, "OpenLoadSingleplayerCommentaryDialog" ) )
	{
		OpenLoadSingleplayerCommentaryDialog();	
	}
	else if ( !Q_stricmp( command, "OpenHowToPlayDialog" ) )
	{
		OnOpenHowToPlayDialog();
	}	
	else if ( !Q_stricmp( command, "OpenAchievementsDialog" ) )
	{
		if ( IsPC() )
		{
			if ( !steamapicontext->SteamUser() || !steamapicontext->SteamUser()->BLoggedOn() )
			{
				vgui::MessageBox *pMessageBox = new vgui::MessageBox("#GameUI_Achievements_SteamRequired_Title", "#GameUI_Achievements_SteamRequired_Message");
				pMessageBox->DoModal();
				return;
			}
			OnOpenAchievementsDialog();
		}
		else
		{
			OnOpenAchievementsDialog_Xbox();
		}
	}
	else if ( !Q_stricmp( command, "OpenCSAchievementsDialog" ) )
	{
		if ( IsPC() )
		{
			if ( !steamapicontext->SteamUser() || !steamapicontext->SteamUser()->BLoggedOn() )
			{
				vgui::MessageBox *pMessageBox = new vgui::MessageBox("#GameUI_Achievements_SteamRequired_Title", "#GameUI_Achievements_SteamRequired_Message", this );
				pMessageBox->DoModal();
				return;
			}

			OnOpenCSAchievementsDialog();
		}
	}
	else if ( !Q_stricmp( command, "OpenAchievementsBlade" ) )
	{
#if defined( _X360 )
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
		int userID = XBX_GetActiveUserId();
		XShowAchievementsUI( userID );
#endif
	}
	else if ( !Q_stricmp( command, "Quit" ) )
	{
		OnOpenQuitConfirmationDialog();
	}
	else if ( !Q_stricmp( command, "QuitToDesktop" ) )
	{
		OnOpenQuitConfirmationDialog( true );
	}
	else if ( !Q_stricmp( command, "QuitNoConfirm" ) )
	{
		if ( IsGameConsole() )
		{
			// start the shutdown process
			StartExitingProcess();
		}
		else
		{		
			// hide everything while we quit
			SetVisible( false );
			vgui::surface()->RestrictPaintToSinglePanel( GetVPanel() );
			engine->ClientCmd_Unrestricted( "quit\n" );
		}
	}
	else if ( !Q_stricmp( command, "QuitRestartNoConfirm" ) )
	{
		if ( IsGameConsole() )
		{
			// start the shutdown process
			m_bRestartSameGame = true;
			StartExitingProcess();
		}
	}
	else if ( !Q_stricmp( command, "ResumeGame" ) )
	{
		GameUI().HideGameUI();
	}
	else if ( !Q_stricmp( command, "Disconnect" ) )
	{
		//if ( IsGameConsole() )
		//{
			OnOpenDisconnectConfirmationDialog();
		//}
		//else
		//{
		//	engine->ClientCmd_Unrestricted( "disconnect" );
		//}
	}
	else if ( !Q_stricmp( command, "DisconnectNoConfirm" ) )
	{
		// $FIXME(hpe) for now, just bail; uncomment the Convar if statements when we get that path working
		engine->ClientCmd_Unrestricted( "disconnect" );
		ConVarRef commentary( "commentary" );
		//if ( commentary.IsValid() && commentary.GetBool() )
		//{
		//	engine->ClientCmd_Unrestricted( "disconnect" );
		//}
		//else
		//{
			// Leave our current session, if we have one
		//	g_pMatchFramework->CloseSession();
		//}
	}
	else if ( !Q_stricmp( command, "ReleaseModalWindow" ) )
	{
		vgui::surface()->RestrictPaintToSinglePanel(NULL);

		// [jason] $FIXME: If we had a modal vgui dialog active over Scaleform, we need to restore Scaleform afterwards
		if ( IsScaleformMainMenuActive() || IsScaleformPauseMenuActive() )
		{
			m_MessageDialogHandler.CloseAllMessageDialogs();
			NotifyVguiDialogClosed();
		}
	}
	else if ( Q_stristr( command, "engine " ) ) // $$$REI Arbitrary console command execution from UI
	{
		const char *engineCMD = strstr( command, "engine " ) + strlen( "engine " );
		if ( strlen( engineCMD ) > 0 )
		{
			engine->ClientCmd_Unrestricted( const_cast<char *>( engineCMD ) );
		}
	}
	else if ( !Q_stricmp( command, "ShowSigninUI" ) )
	{
		m_bWaitingForUserSignIn = true;
		xboxsystem->ShowSigninUI( 1, 0 ); // One user, no special flags
	}
	else if ( !Q_stricmp( command, "ShowDeviceSelector" ) )
	{
		OnChangeStorageDevice();
	}
	else if ( !Q_stricmp( command, "SignInDenied" ) )
	{
		// The user doesn't care, so re-send the command they wanted and mark that we want to skip checking
		m_bUserRefusedSignIn = true;
		if ( m_strPostPromptCommand.IsEmpty() == false )
		{
			OnCommand( m_strPostPromptCommand );		
		}
	}
	else if ( !Q_stricmp( command, "RequiredSignInDenied" ) )
	{
		m_strPostPromptCommand = "";
	}
	else if ( !Q_stricmp( command, "RequiredStorageDenied" ) )
	{
		m_strPostPromptCommand = "";
	}
	else if ( !Q_stricmp( command, "StorageDeviceDenied" ) )
	{
		// The user doesn't care, so re-send the command they wanted and mark that we want to skip checking
		m_bUserRefusedStorageDevice = true;
		IssuePostPromptCommand();

		// Set us as declined
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
		XBX_SetStorageDeviceId( XBX_GetActiveUserId(), XBX_STORAGE_DECLINED );
		m_iStorageID = XBX_INVALID_STORAGE_ID;

		if ( m_pStorageDeviceValidatedNotify )
		{
			*m_pStorageDeviceValidatedNotify = 2;
			m_pStorageDeviceValidatedNotify = NULL;
		}
	}
	else if ( !Q_stricmp( command, "clear_storage_deviceID" ) )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
		XBX_SetStorageDeviceId( XBX_GetActiveUserId(), XBX_STORAGE_DECLINED );
	}

	else if ( !Q_stricmp( command, "RestartWithNewLanguage" ) )
	{
		if ( !IsGameConsole() )
		{
			char szSteamURL[50];
			char szAppId[50];

			// hide everything while we quit
			SetVisible( false );
			vgui::surface()->RestrictPaintToSinglePanel( GetVPanel() );
			engine->ClientCmd_Unrestricted( "quit\n" );

			// Construct Steam URL. Pattern is steam://run/<appid>/<language>. (e.g. Ep1 In French ==> steam://run/380/french)
			V_strcpy(szSteamURL, "steam://run/");
// dgoodenough - use Q_snprintf on PS3
// PS3_BUILDFIX
// @wge Fix for OSX too.
#if defined( _PS3 ) || defined( _OSX ) || defined (LINUX)
			Q_snprintf( szAppId, 50, "%d", engine->GetAppID() );
			szAppId[49] = 0;
#else
			itoa( engine->GetAppID(), szAppId, 10 );
#endif
			V_strcat( szSteamURL, szAppId, sizeof( szSteamURL ) );
			V_strcat( szSteamURL, "/", sizeof( szSteamURL ) );
			V_strcat( szSteamURL, COptionsSubAudio::GetUpdatedAudioLanguage(), sizeof( szSteamURL ) );

			// Set Steam URL for re-launch in registry. Launcher will check this registry key and exec it in order to re-load the game in the proper language
			// @wge HACK FIXME - Windows specific registry code.
#if !defined( _GAMECONSOLE ) && !defined( _OSX ) && !defined (LINUX)
			HKEY hKey;

			if ( IsPC() && RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Valve\\Source", NULL, KEY_WRITE, &hKey) == ERROR_SUCCESS )
			{
				RegSetValueEx( hKey, "Relaunch URL", 0, REG_SZ, (const unsigned char *)szSteamURL, sizeof( szSteamURL ) );

				RegCloseKey(hKey);
			}
#endif
		}
	}
	else
	{
		BaseClass::OnCommand( command);
	}
}



//-----------------------------------------------------------------------------
// Purpose: Queue a command to be run when XUI Closes
//-----------------------------------------------------------------------------
void CBaseModPanel::QueueCommand( const char *pCommand )
{
	if ( m_bXUIVisible )
	{
		m_CommandQueue.AddToTail( CUtlString( pCommand ) );
	}
	else
	{
		OnCommand( pCommand );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Run all the commands in the queue
//-----------------------------------------------------------------------------
void CBaseModPanel::RunQueuedCommands()
{
	for ( int i = 0; i < m_CommandQueue.Count(); ++i )
	{
		OnCommand( m_CommandQueue[i] );
	}
	ClearQueuedCommands();
}

//-----------------------------------------------------------------------------
// Purpose: Clear all queued commands
//-----------------------------------------------------------------------------
void CBaseModPanel::ClearQueuedCommands()
{
	m_CommandQueue.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: Whether this command should cause us to prompt the user if they're not signed in and do not have a storage device
//-----------------------------------------------------------------------------
bool CBaseModPanel::IsPromptableCommand( const char *command )
{
	// Blech!
	if ( !Q_stricmp( command, "OpenNewGameDialog" ) ||
		 !Q_stricmp( command, "OpenLoadGameDialog" ) ||
		 !Q_stricmp( command, "OpenSaveGameDialog" ) ||
		 !Q_stricmp( command, "OpenBonusMapsDialog" ) ||
		 !Q_stricmp( command, "OpenOptionsDialog" ) ||
		 !Q_stricmp( command, "OpenSettingsDialog" ) ||
		 !Q_stricmp( command, "OpenControllerDialog" ) ||
		 !Q_stricmp( command, "OpenMouseDialog" ) ||
		 !Q_stricmp( command, "OpenKeyboardDialog" ) ||
		 !Q_stricmp( command, "OpenMotionControllerMoveDialog" ) ||
		 !Q_stricmp( command, "OpenMotionControllerSharpshooterDialog" ) ||
		 !Q_stricmp( command, "OpenMotionControllerDialog" ) ||
		 !Q_stricmp( command, "OpenMotionCalibrationDialog" ) ||
		 !Q_stricmp( command, "OpenVideoSettingsDialog" ) ||
		 !Q_stricmp( command, "OpenOptionsQueued" ) ||
		 !Q_stricmp( command, "OpenAudioSettingsDialog" ) ||
		 !Q_stricmp( command, "OpenLoadCommentaryDialog" ) ||
		 !Q_stricmp( command, "OpenLoadSingleplayerCommentaryDialog" ) ||
		 !Q_stricmp( command, "OpenAchievementsDialog" ) ||
		 !Q_stricmp( command, "OpenCreateSinglePlayerGameDialog" ) )
	{
		 return true;
	}

	return false;
}

#ifdef _WIN32
//-------------------------
// Purpose: Job wrapper
//-------------------------
static uintp PanelJobWrapperFn( void *pvContext )
{
	CBaseModPanel::CAsyncJobContext *pAsync = reinterpret_cast< CBaseModPanel::CAsyncJobContext * >( pvContext );

	float const flTimeStart = Plat_FloatTime();
	
	pAsync->ExecuteAsync();

	float const flElapsedTime = Plat_FloatTime() - flTimeStart;

	if ( flElapsedTime < pAsync->m_flLeastExecuteTime )
	{
		ThreadSleep( ( pAsync->m_flLeastExecuteTime - flElapsedTime ) * 1000 );
	}

	ReleaseThreadHandle( ( ThreadHandle_t ) pAsync->m_hThreadHandle );
	pAsync->m_hThreadHandle = NULL;

	return 0;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Enqueues a job function to be called on a separate thread
//-----------------------------------------------------------------------------
void CBaseModPanel::ExecuteAsync( CAsyncJobContext *pAsync )
{
	Assert( !m_pAsyncJob );
	Assert( pAsync && !pAsync->m_hThreadHandle );
	m_pAsyncJob = pAsync;

#ifdef _WIN32
	ThreadHandle_t hHandle = CreateSimpleThread( PanelJobWrapperFn, reinterpret_cast< void * >( pAsync ) );
	pAsync->m_hThreadHandle = hHandle;

#ifdef _GAMECONSOLE
	ThreadSetAffinity( hHandle, XBOX_PROCESSOR_3 );
#endif

#else
	pAsync->ExecuteAsync();
#endif
}



//-----------------------------------------------------------------------------
// Purpose: Whether this command requires the user be signed in
//-----------------------------------------------------------------------------
bool CBaseModPanel::CommandRequiresSignIn( const char *command )
{
	// Blech again!
	if ( !Q_stricmp( command, "OpenAchievementsDialog" ) || 
		 !Q_stricmp( command, "OpenLoadGameDialog" ) ||
		 !Q_stricmp( command, "OpenSaveGameDialog" ) || 
		 !Q_stricmp( command, "OpenRankingsDialog" ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Whether the command requires the user to have a valid storage device
//-----------------------------------------------------------------------------
bool CBaseModPanel::CommandRequiresStorageDevice( const char *command )
{
	// Anything which touches the storage device must prompt
	if ( !Q_stricmp( command, "OpenSaveGameDialog" ) ||
		 !Q_stricmp( command, "OpenLoadGameDialog" ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Whether the command requires the user to have a valid profile selected
//-----------------------------------------------------------------------------
bool CBaseModPanel::CommandRespectsSignInDenied( const char *command )
{
	// Anything which touches the user profile must prompt
	if ( !Q_stricmp( command, "OpenOptionsDialog" ) ||
		 !Q_stricmp( command, "OpenSettingsDialog" ) ||
		 !Q_stricmp( command, "OpenControllerDialog" ) ||
		 !Q_stricmp( command, "OpenMouseDialog" ) ||
		 !Q_stricmp( command, "OpenKeyboardDialog" ) ||
 		 !Q_stricmp( command, "OpenMotionControllerMoveDialog" ) ||  
		 !Q_stricmp( command, "OpenMotionControllerSharpshooterDialog" ) ||  
		 !Q_stricmp( command, "OpenMotionControllerDialog" ) ||  
		 !Q_stricmp( command, "OpenVideoSettingsDialog" ) || 
		 !Q_stricmp( command, "OpenOptionsQueued" ) || 
		 !Q_stricmp( command, "OpenAudioSettingsDialog" ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: A storage device has been connected, update our settings and anything else
//-----------------------------------------------------------------------------

class CAsyncCtxOnDeviceAttached : public CBaseModPanel::CAsyncJobContext
{
public:
	explicit CAsyncCtxOnDeviceAttached( int iController );
	~CAsyncCtxOnDeviceAttached();
	virtual void ExecuteAsync();
	virtual void Completed();
	int GetController() { return m_iController; }
	uint GetContainerOpenResult( void ) { return m_ContainerOpenResult; }

private:
	int  m_iController;
	uint m_ContainerOpenResult;
};

CAsyncCtxOnDeviceAttached::CAsyncCtxOnDeviceAttached( int iController ) :
	CBaseModPanel::CAsyncJobContext( 3.0f ),	// Storage device info for at least 3 seconds
	m_ContainerOpenResult( ERROR_SUCCESS ),
	m_iController( iController )
{
	BasePanel()->ShowMessageDialog( MD_CHECKING_STORAGE_DEVICE );
}

CAsyncCtxOnDeviceAttached::~CAsyncCtxOnDeviceAttached()
{
	BasePanel()->CloseMessageDialog( 0 );
}

void CAsyncCtxOnDeviceAttached::ExecuteAsync()
{
	// Asynchronously do the tasks that don't interact with the command buffer

	// Open user settings and save game container here
	m_ContainerOpenResult = engine->OnStorageDeviceAttached( GetController() );
	if ( m_ContainerOpenResult != ERROR_SUCCESS )
		return;

	// Make the QOS system initialized for multiplayer games
	if ( !ModInfo().IsSinglePlayerOnly() )
	{
#if defined( _GAMECONSOLE )
#pragma message( __FILE__ "(" __LINE__AS_STRING ") : warning custom: This is just a best guess at the right replacement to get rotted code compiling" )
		//( void ) matchmaking->GetQosWithLIVE();
		g_pMatchFramework->GetMatchNetworkMsgController()->GetQOS();		
#endif
	}
}

void CAsyncCtxOnDeviceAttached::Completed()
{
	BasePanel()->OnCompletedAsyncDeviceAttached( this );
}

void CBaseModPanel::OnDeviceAttached( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
	ExecuteAsync( new CAsyncCtxOnDeviceAttached( XBX_GetActiveUserId() ) );
}

void CBaseModPanel::OnCompletedAsyncDeviceAttached( CAsyncCtxOnDeviceAttached *job )
{
	uint nRet = job->GetContainerOpenResult();
	if ( nRet != ERROR_SUCCESS )
	{
		// Invalidate the device
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
		XBX_SetStorageDeviceId( XBX_GetActiveUserId(), XBX_INVALID_STORAGE_ID );

		// FIXME: We don't know which device failed!
		// Pop a dialog explaining that the user's data is corrupt
		BasePanel()->ShowMessageDialog( MD_STORAGE_DEVICES_CORRUPT );
	}

	// First part of the device checking completed asynchronously,
	// perform the rest of duties that require to run on main thread.
	engine->ReadConfiguration( -1, false );
	engine->ExecuteClientCmd( "refreshplayerstats" );

	BonusMapsDatabase()->ReadBonusMapSaveData();

	if ( m_hSaveGameDialog_Xbox.Get() )
	{
		m_hSaveGameDialog_Xbox->OnCommand( "RefreshSaveGames" );
	}
	if ( m_hLoadGameDialog_Xbox.Get() )
	{
		m_hLoadGameDialog_Xbox->OnCommand( "RefreshSaveGames" );
	}
	if ( m_pStorageDeviceValidatedNotify )
	{
		*m_pStorageDeviceValidatedNotify = 1;
		m_pStorageDeviceValidatedNotify = NULL;
	}

	// Finish their command
	IssuePostPromptCommand();
}

//-----------------------------------------------------------------------------
// Purpose: FIXME: Only TF takes this path...
//-----------------------------------------------------------------------------
bool CBaseModPanel::ValidateStorageDevice( void )
{
	if ( m_bUserRefusedStorageDevice == false )
	{
#if defined( _GAMECONSOLE )
#pragma message( __FILE__ "(" __LINE__AS_STRING ") : warning custom: Slamming controller for xbox storage id to 0" )
		if ( XBX_GetStorageDeviceId( 0 ) == XBX_INVALID_STORAGE_ID )
		{
			// Try to discover content on the user's storage devices
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
			DWORD nFoundDevice = xboxsystem->DiscoverUserData( XBX_GetActiveUserId(), COM_GetModDirectory() );
			if ( nFoundDevice == XBX_INVALID_STORAGE_ID )
			{
				// They don't have a device, so ask for one
				ShowMessageDialog( MD_PROMPT_STORAGE_DEVICE );
				return false;
			}
			else
			{
				// Take this device
				XBX_SetStorageDeviceId( XBX_GetActiveUserId(), nFoundDevice );
				OnDeviceAttached();
			}
			// Fall through
		}
#endif
	}
	return true;
}

bool CBaseModPanel::ValidateStorageDevice( int *pStorageDeviceValidated )
{
	if ( m_pStorageDeviceValidatedNotify )
	{
		if ( pStorageDeviceValidated != m_pStorageDeviceValidatedNotify )
		{
			*m_pStorageDeviceValidatedNotify = -1;
			m_pStorageDeviceValidatedNotify = NULL;
		}
		else
		{
			return false;
		}
	}

	if ( pStorageDeviceValidated )
	{
		if ( HandleStorageDeviceRequest( "" ) )
			return true;

		m_pStorageDeviceValidatedNotify = pStorageDeviceValidated;
		return false;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Monitor commands for certain necessary cases
// Input  : *command - What menu command we're policing
//-----------------------------------------------------------------------------
bool CBaseModPanel::HandleSignInRequest( const char *command )
{
// dgoodenough - limit this to X360
// PS3_BUILDFIX
// FIXME - do we want to have somehting here for PS3?
#if defined( _X360 )
	// If we have a post-prompt command, we're coming back into the call from that prompt
	bool bQueuedCall = ( m_strPostPromptCommand.IsEmpty() == false );

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
	XUSER_SIGNIN_INFO info;
	bool bValidUser = ( XUserGetSigninInfo( XBX_GetActiveUserId(), 0, &info ) == ERROR_SUCCESS );

	if ( bValidUser )
		return true;

	// Queued command means we're returning from a prompt or blade
	if ( bQueuedCall )
	{
		// Blade has returned with nothing
		if ( m_bUserRefusedSignIn )
			return true;
		
		// User has not denied the storage device, so ask
		ShowMessageDialog( MD_PROMPT_SIGNIN );
		m_strPostPromptCommand = command;
		
		// Do not run command
		return false;
	}
	else
	{
		// If the user refused the sign-in and we respect that on this command, we're done
		if ( m_bUserRefusedSignIn && CommandRespectsSignInDenied( command ) )
			return true;

		// If the message is required first, then do that instead
		if ( CommandRequiresSignIn( command ) )
		{
			ShowMessageDialog( MD_PROMPT_SIGNIN_REQUIRED );
			m_strPostPromptCommand = command;
			return false;
		}

		// Pop a blade out
		xboxsystem->ShowSigninUI( 1, 0 );
		m_strPostPromptCommand = command;
		m_bWaitingForUserSignIn = true;
		m_bUserRefusedSignIn = false;
		return false;	
	}
#endif // _GAMECONSOLE
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
bool CBaseModPanel::HandleStorageDeviceRequest( const char *command )
{
	// If we don't have a valid sign-in, then we do nothing!
	if ( m_bUserRefusedSignIn )
		return true;

	// If we have a valid storage device, there's nothing to prompt for
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
	if ( XBX_GetStorageDeviceId( XBX_GetActiveUserId()	) != XBX_INVALID_STORAGE_ID && XBX_GetStorageDeviceId( XBX_GetActiveUserId() ) != XBX_STORAGE_DECLINED )
		return true;

	// If we have a post-prompt command, we're coming back into the call from that prompt
	bool bQueuedCall = ( m_strPostPromptCommand.IsEmpty() == false );
	
	// Are we returning from a prompt?
	if ( bQueuedCall && m_bStorageBladeShown )
	{
		// User has declined
		if ( m_bUserRefusedStorageDevice )
			return true;

		// Prompt them
		ShowMessageDialog( MD_PROMPT_STORAGE_DEVICE );
		m_strPostPromptCommand = command;
		
		// Do not run the command
		return false;
	}
	else
	{
		// If the user refused the sign-in and we respect that on this command, we're done
		if ( m_bUserRefusedStorageDevice && CommandRespectsSignInDenied( command ) )
			return true;

#if 0 // This attempts to find user data, but may not be cert-worthy even though it's a bit nicer for the user
		// Attempt to automatically find a device
		DWORD nFoundDevice = xboxsystem->DiscoverUserData( XBX_GetPrimaryUserId(), COM_GetModDirectory() );
		if ( nFoundDevice != XBX_INVALID_STORAGE_ID )
		{
			// Take this device
			XBX_SetStorageDeviceId( XBX_GetPrimaryUserId(), nFoundDevice );
			OnDeviceAttached();
			return true;
		}
#endif // 

		// If the message is required first, then do that instead
		if ( CommandRequiresStorageDevice( command ) )
		{
			ShowMessageDialog( MD_PROMPT_STORAGE_DEVICE_REQUIRED );
			m_strPostPromptCommand = command;
			return false;
		}

		// This is a misnomer of the first order!
		OnChangeStorageDevice();
		m_strPostPromptCommand = command;
		m_bStorageBladeShown = true;
		m_bUserRefusedStorageDevice = false;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Clear the command we've queued once it has succeeded in being called
//-----------------------------------------------------------------------------
void CBaseModPanel::ClearPostPromptCommand( const char *pCompletedCommand )
{
	if ( !Q_stricmp( m_strPostPromptCommand, pCompletedCommand ) )
	{
		// All commands are executed, so stop holding this
		m_strPostPromptCommand = "";
	}
}

//-----------------------------------------------------------------------------
// Purpose: Issue our queued command to either the base panel or the matchmaking panel
//-----------------------------------------------------------------------------
void CBaseModPanel::IssuePostPromptCommand( void )
{
	// The device is valid, so launch any pending commands
	if ( m_strPostPromptCommand.IsEmpty() == false )
	{
		if ( m_bSinglePlayer )
		{
			OnCommand( m_strPostPromptCommand );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: message handler for menu selections
//-----------------------------------------------------------------------------
void CBaseModPanel::OnCommand( const char *command )
{
	if ( GameUI().IsConsoleUI() )
	{
#if defined( _GAMECONSOLE )

		// See if this is a command we need to intercept
		if ( IsPromptableCommand( command ) )
		{
			// Handle the sign in case
			if ( HandleSignInRequest( command ) == false )
				return;
			
			// Handle storage
			if ( HandleStorageDeviceRequest( command ) == false )
				return;

			// If we fall through, we'll need to track this again
			m_bStorageBladeShown = false;

			// Fall through
		}
#endif // _GAMECONSOLE

		RunAnimationWithCallback( this, command, new KeyValues( "RunMenuCommand", "command", command ) );
	
		// Clear our pending command if we just executed it
		ClearPostPromptCommand( command );
	}
	else
	{
		RunMenuCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: runs an animation sequence, then calls a message mapped function
//			when the animation is complete. 
//-----------------------------------------------------------------------------
void CBaseModPanel::RunAnimationWithCallback( vgui::Panel *parent, const char *animName, KeyValues *msgFunc )
{
	if ( !m_pConsoleAnimationController )
		return;

	m_pConsoleAnimationController->StartAnimationSequence( animName );
	float sequenceLength = m_pConsoleAnimationController->GetAnimationSequenceLength( animName );
	if ( sequenceLength )
	{
		sequenceLength += g_flAnimationPadding;
	}
	if ( parent && msgFunc )
	{
		PostMessage( parent, msgFunc, sequenceLength );
	}
}

//-----------------------------------------------------------------------------
// Purpose: trinary choice query "save & quit", "quit", "cancel"
//-----------------------------------------------------------------------------
class CSaveBeforeQuitQueryDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CSaveBeforeQuitQueryDialog, vgui::Frame );
public:
	CSaveBeforeQuitQueryDialog(vgui::Panel *parent, const char *name) : BaseClass(parent, name)
	{
		LoadControlSettings("resource/SaveBeforeQuitDialog.res");
		SetDeleteSelfOnClose(true);
		SetSizeable(false);
	}

	void DoModal()
	{
		BaseClass::Activate();
		vgui::input()->SetAppModalSurface(GetVPanel());
		MoveToCenterOfScreen();
		vgui::surface()->RestrictPaintToSinglePanel(GetVPanel());

		GameUI().PreventEngineHideGameUI();
	}

	void OnKeyCodePressed(KeyCode code)
	{
		// ESC cancels
		if ( code == KEY_ESCAPE )
		{
			Close();
		}
		else
		{
			BaseClass::OnKeyCodePressed(code);
		}
	}

	virtual void OnCommand(const char *command)
	{
		if (!Q_stricmp(command, "Quit"))
		{
			PostMessage(GetVParent(), new KeyValues("Command", "command", "QuitNoConfirm"));
		}
		else if (!Q_stricmp(command, "SaveAndQuit"))
		{
			// find a new name to save
			char saveName[128];
			CSaveGameDialog::FindSaveSlot( saveName, sizeof(saveName) );
			if ( saveName && saveName[ 0 ] )
			{
				// save the game
				char sz[ 256 ];
				Q_snprintf(sz, sizeof( sz ), "save %s\n", saveName );
				engine->ClientCmd_Unrestricted( sz );
			}

			// quit
			PostMessage(GetVParent(), new KeyValues("Command", "command", "QuitNoConfirm"));
		}
		else if (!Q_stricmp(command, "Cancel"))
		{
			Close();
		}
		else
		{
			BaseClass::OnCommand(command);
		}
	}

	virtual void OnClose()
	{
		BaseClass::OnClose();
		vgui::surface()->RestrictPaintToSinglePanel(NULL);
		GameUI().AllowEngineHideGameUI();
	}
};

//-----------------------------------------------------------------------------
// Purpose: simple querybox that accepts escape
//-----------------------------------------------------------------------------
class CQuitQueryBox : public vgui::QueryBox
{
	DECLARE_CLASS_SIMPLE( CQuitQueryBox, vgui::QueryBox );
public:
	CQuitQueryBox(const char *title, const char *info, Panel *parent) : BaseClass( title, info, parent )
	{
	}

	void DoModal( Frame* pFrameOver )
	{
		BaseClass::DoModal( pFrameOver );
		vgui::surface()->RestrictPaintToSinglePanel(GetVPanel());
		GameUI().PreventEngineHideGameUI();
	}

	void OnKeyCodePressed(KeyCode code)
	{
		// ESC cancels
		if (code == KEY_ESCAPE)
		{
			Close();
		}
		else
		{
			BaseClass::OnKeyCodePressed(code);
		}
	}

	virtual void OnClose()
	{
		BaseClass::OnClose();
		vgui::surface()->RestrictPaintToSinglePanel(NULL);
		GameUI().AllowEngineHideGameUI();
	}
};

//-----------------------------------------------------------------------------
// Purpose: asks user how they feel about quiting
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenQuitConfirmationDialog( bool bForceToDesktop )
{
	if ( GameUI().IsConsoleUI() )
	{
		if ( !GameUI().HasSavedThisMenuSession() && GameUI().IsInLevel() && engine->GetMaxClients() == 1 )
		{
			// single player, progress will be lost...
			ShowMessageDialog( MD_SAVE_BEFORE_QUIT ); 
		}
		else
		{
			if ( m_bUseMatchmaking )
			{
				ShowMessageDialog( MD_QUIT_CONFIRMATION_TF );
			}
			else
			{
				ShowMessageDialog( MD_QUIT_CONFIRMATION );
			}
		}
		return;
	}


	if ( GameUI().IsInLevel() && engine->GetMaxClients() == 1 )
	{
		// prompt for saving current game before quiting
		CSaveBeforeQuitQueryDialog *box = new CSaveBeforeQuitQueryDialog(this, "SaveBeforeQuitQueryDialog");
		box->DoModal();
	}
	else
	{
		// simple ok/cancel prompt
		QueryBox *box = new CQuitQueryBox("#GameUI_QuitConfirmationTitle", "#GameUI_QuitConfirmationText", this);
		box->SetOKButtonText("#GameUI_Quit");
		box->SetOKCommand(new KeyValues("Command", "command", "QuitNoConfirm"));
		box->SetCancelCommand(new KeyValues("Command", "command", "ReleaseModalWindow"));
		box->AddActionSignalTarget(this);
		box->DoModal();
	}
}

//-----------------------------------------------------------------------------
// Purpose: asks user how they feel about disconnecting
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenDisconnectConfirmationDialog()
{	
	// THis is for disconnecting from a multiplayer server
	Assert( m_bUseMatchmaking );
	Assert( IsGameConsole() );

	if ( GameUI().IsConsoleUI() && GameUI().IsInLevel() )
	{
		if ( engine->GetLocalPlayer() == 1 )
		{
			ShowMessageDialog( MD_DISCONNECT_CONFIRMATION_HOST );
		}
		else
		{
			ShowMessageDialog( MD_DISCONNECT_CONFIRMATION );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenNewGameDialog(const char *chapter )
{
	if ( !m_hNewGameDialog.Get() )
	{
		m_hNewGameDialog = new CNewGameDialog(this, false);
		PositionDialog( m_hNewGameDialog );
	}

	if ( chapter )
	{
		((CNewGameDialog *)m_hNewGameDialog.Get())->SetSelectedChapter(chapter);
	}

	((CNewGameDialog *)m_hNewGameDialog.Get())->SetCommentaryMode( false );
	m_hNewGameDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenBonusMapsDialog( void )
{
	if ( !m_hBonusMapsDialog.Get() )
	{
		m_hBonusMapsDialog = new CBonusMapsDialog(this);
		PositionDialog( m_hBonusMapsDialog );
	}

	m_hBonusMapsDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenLoadGameDialog()
{
	if ( !m_hLoadGameDialog.Get() )
	{
		m_hLoadGameDialog = new CLoadGameDialog(this);
		PositionDialog( m_hLoadGameDialog );
	}
	m_hLoadGameDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenLoadGameDialog_Xbox()
{
	if ( !m_hLoadGameDialog_Xbox.Get() )
	{
		m_hLoadGameDialog_Xbox = new CLoadGameDialogXbox(this);
		PositionDialog( m_hLoadGameDialog_Xbox );
	}
	m_hLoadGameDialog_Xbox->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenSaveGameDialog()
{
	if ( !m_hSaveGameDialog.Get() )
	{
		m_hSaveGameDialog = new CSaveGameDialog(this);
		PositionDialog( m_hSaveGameDialog );
	}
	m_hSaveGameDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenSaveGameDialog_Xbox()
{
	if ( !m_hSaveGameDialog_Xbox.Get() )
	{
		m_hSaveGameDialog_Xbox = new CSaveGameDialogXbox(this);
		PositionDialog( m_hSaveGameDialog_Xbox );
	}
	m_hSaveGameDialog_Xbox->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenOptionsDialog()
{
	if ( !m_hOptionsDialog.Get() )
	{
		m_hOptionsDialog = new COptionsDialog(this);
		PositionDialog( m_hOptionsDialog );
	}

	m_hOptionsDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenSettingsDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenOptionsDialog_Xbox()
{
#if 0
	if ( !m_hOptionsDialog_Xbox.Get() )
	{
		m_hOptionsDialog_Xbox = new COptionsDialogXbox( this );
		PositionDialog( m_hOptionsDialog_Xbox );
	}

	m_hOptionsDialog_Xbox->Activate();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: forces any changed options dialog settings to be applied immediately, if it's open
//-----------------------------------------------------------------------------
void CBaseModPanel::ApplyOptionsDialogSettings()
{
	if (m_hOptionsDialog.Get())
	{
		m_hOptionsDialog->ApplyChanges();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenControllerDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenMouseDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenKeyboardDialog()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenMotionControllerDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenMotionControllerMoveDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenMotionControllerSharpshooterDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenMotionCalibrationDialog()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenVideoSettingsDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenOptionsQueued()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenAudioSettingsDialog()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenBenchmarkDialog()
{
	if (!m_hBenchmarkDialog.Get())
	{
		m_hBenchmarkDialog = new CBenchmarkDialog(this, "BenchmarkDialog");
		PositionDialog( m_hBenchmarkDialog );
	}
	m_hBenchmarkDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

void CBaseModPanel::OnCloseServerBrowser()
{
#if !defined(_GAMECONSOLE)
	// HACK: Server browser is module index = 0, which may change if the list is reordered in the res file
	if ( g_VModuleLoader.GetModuleCount() > 0 && g_VModuleLoader.IsModuleVisible(0) )
		g_VModuleLoader.PostMessageToModule("Servers", new KeyValues( "RunModuleCommand", "command", "Close" ) );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenFriendsDialog()
{
	g_VModuleLoader.ActivateModule("Friends");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenDemoDialog()
{
/*	if ( !m_hDemoPlayerDialog.Get() )
	{
		m_hDemoPlayerDialog = new CDemoPlayerDialog(this);
		PositionDialog( m_hDemoPlayerDialog );
	}
	m_hDemoPlayerDialog->Activate();*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenCreateMultiplayerGameDialog()
{
	if (!m_hCreateMultiplayerGameDialog.Get())
	{
		m_hCreateMultiplayerGameDialog = new CCreateMultiplayerGameDialog(this);
		PositionDialog(m_hCreateMultiplayerGameDialog);
	}
	m_hCreateMultiplayerGameDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenCreateMultiplayerGameCommunity()
{
	//
}

//-----------------------------------------------------------------------------
// Purpose: Determine if the local player is part of a team multiplayer lobby
//-----------------------------------------------------------------------------
bool CBaseModPanel::InTeamLobby( void )
{
	if ( g_pMatchFramework )
	{
		IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
		if ( pMatchSession )
		{
			KeyValues *pSettings = pMatchSession->GetSessionSettings();
			const char *pszNetflag = pSettings->GetString( "system/netflag", NULL );
			if ( pszNetflag && !V_stricmp( pszNetflag, "teamlobby" ) )
				return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenMedalsDialog()
{
	// Do nothing, unless we're in Cstrike with Scaleform
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenStatsDialog()
{
	// Do nothing, unless we're in Cstrike with Scaleform
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::CloseMedalsStatsDialog()
{
	// Do nothing, unless we're in Cstrike with Scaleform
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenLeaderboardsDialog()
{
	// Do nothing, unless we're in Cstrike with Scaleform
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenCallVoteDialog()
{
	// Do nothing, unless we're in Cstrike with Scaleform
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenMarketplace()
{
	// Do nothing, unless we're in Cstrike with Scaleform
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::UpdateLeaderboardsDialog()
{
	// Do nothing, unless we're in Cstrike with Scaleform
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::CloseLeaderboardsDialog()
{
	// Do nothing, unless we're in Cstrike with Scaleform
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenChangeGameDialog()
{
	if (!m_hChangeGameDialog.Get())
	{
		m_hChangeGameDialog = new CChangeGameDialog(this);
		PositionDialog(m_hChangeGameDialog);
	}
	m_hChangeGameDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: [jason] Default implementation (non-Scaleform) for Start Screen
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenCreateStartScreen()
{
	// $TODO: Should we create a vgui version of this screen for non-scaleform builds?
	//	Or just bypass it and do nothing, going straight to the main menu
	m_bShowStartScreen = false;
	ShowMainMenu(true);
}

//-----------------------------------------------------------------------------
// Purpose:  Code called from opencreatestartscreen command and when signing out
//-----------------------------------------------------------------------------
void CBaseModPanel::HandleOpenCreateStartScreen()
{
	// Be sure to close any previously opened modal Scaleform screens before we restore start screen
	DismissAllMainMenuScreens();

	// [jason] Brings up the actual start screen for re-binding the primary user controller, etc.
	m_bShowStartScreen = true;
	m_bStartScreenPlayerSigninCompleted = false;
	m_bWaitingForUserSignIn = false;
	OnOpenCreateStartScreen();
}

void CBaseModPanel::DismissAllMainMenuScreens( bool bHideMainMenuOnly )
{
	NOTE_UNUSED( bHideMainMenuOnly );

	// Overloaded in Cstrike15BasePanel to perform actual screen closes
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::DismissStartScreen()
{
	// Do nothing, only the scaleform version needs to actually dismiss this scene
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseModPanel::IsStartScreenActive( void )
{
	// Overloaded in Cstrike15BasePanel for Scaleform 
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::SignInFromStartScreen()
{
	m_bWaitingForUserSignIn = true;
	m_bUserRefusedSignIn = true;
	xboxsystem->ShowSigninUI( 1, 0 ); // One user, no special flags
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::NotifySignInCompleted(int userID)
{
	// $TODO: sanity checks to verify that we have someone signed in, as needed
	if ( IsStartScreenActive() && m_bWaitingForUserSignIn )
	{
		m_primaryUserId = userID;
		m_bWaitingForUserSignIn = false;
		m_bUserRefusedSignIn = false;
		m_bStartScreenPlayerSigninCompleted = true;
	}	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::NotifySignInCancelled()
{
	// $TODO: sanity checks to verify that we have someone signed in, as needed
	if ( IsStartScreenActive() )
	{
		UnlockInput();
		m_bWaitingForUserSignIn = false;
		m_bStartScreenPlayerSigninCompleted = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: [jason] Default implementation of the main menu
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenCreateMainMenuScreen( void )
{
	ShowMainMenu( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::DismissMainMenuScreen( void )
{
	ShowMainMenu( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::RestoreMainMenuScreen( void )
{
	ShowMainMenu( true );
}

//-----------------------------------------------------------------------------
// Purpose: Restore Scaleform main menu to shown, and hide the vgui main menu
//-----------------------------------------------------------------------------
void CBaseModPanel::NotifyVguiDialogClosed( void )
{
	if ( GameUI().IsInLevel() )
	{
		if ( IsScaleformPauseMenuEnabled() && m_bMainMenuShown )
		{
			// Force the cursor to be under Scaleform control again
			if ( IsPC() )
			{
				g_pMatSystemSurface->EnableWindowsMessages( false );
			}

			ShowMainMenu( false );
			RestorePauseMenu();
		}
	}
	else
	{
		if ( IsScaleformMainMenuEnabled() && m_bMainMenuShown )
		{
			ShowMainMenu( false );
			RestoreMainMenuScreen();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Opens upsell dialog
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenUpsellDialog( void )
{
	/** Does nothing by default */
}

//-----------------------------------------------------------------------------
// Purpose: Plays the credits 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnPlayCreditsVideo()
{
	engine->ClientCmd_Unrestricted( "playvideo credits" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::ShowScaleformMainMenu( bool bShow )
{
	/** Does nothing by default */
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseModPanel::IsScaleformMainMenuActive( void )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Overridden if you want to use Scaleform to create the menu
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenPauseMenu( void )
{
	ShowMainMenu( true );
}

//-----------------------------------------------------------------------------
// Purpose: Overridden to clean up any non-vgui menu assets
//-----------------------------------------------------------------------------
void CBaseModPanel::DismissPauseMenu( void )
{
	ShowMainMenu( false );
}

//-----------------------------------------------------------------------------
// Purpose: Overridden if you want to use Scaleform to create the menu
//-----------------------------------------------------------------------------
void CBaseModPanel::RestorePauseMenu( void )
{
	ShowMainMenu( true );
}

//-----------------------------------------------------------------------------
// Purpose: To be overridden by Scaleform
//-----------------------------------------------------------------------------
void CBaseModPanel::ShowScaleformPauseMenu( bool bShow )
{
	/** Does nothing by default */
}

//-----------------------------------------------------------------------------
// Purpose: To be overridden by Scaleform
//-----------------------------------------------------------------------------
bool CBaseModPanel::IsScaleformPauseMenuActive( void )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: To be overridden by Scaleform
//-----------------------------------------------------------------------------
bool CBaseModPanel::IsScaleformPauseMenuVisible( void )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Change a private/friends match to a public one
//-----------------------------------------------------------------------------
void CBaseModPanel::OnMakeGamePublic( void )
{
	IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
	if ( !pMatchSession ) 
		return;

	// Check if this is already a public game
	KeyValues* kv = pMatchSession->GetSessionSettings();
	if ( kv )
	{
		char const *szAccess = kv->GetString( "system/access", NULL );
		if ( szAccess )
		{
			if ( !Q_stricmp( "public", szAccess ) )
			{
				return;
			}
		}
	}

	// Update it to be public now
	pMatchSession->UpdateSessionSettings( KeyValues::AutoDeleteInline( KeyValues::FromString(
				"update",
				" update { "
					" system { "
						" access public "
					" } "
				" } "
				) ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenPlayerListDialog()
{
	if (!m_hPlayerListDialog.Get())
	{
		m_hPlayerListDialog = new CPlayerListDialog(this);
		PositionDialog(m_hPlayerListDialog);
	}
	m_hPlayerListDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenLoadCommentaryDialog()
{
	if (!m_hPlayerListDialog.Get())
	{
		m_hLoadCommentaryDialog = new CLoadCommentaryDialog(this);
		PositionDialog(m_hLoadCommentaryDialog);
	}
	m_hLoadCommentaryDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OpenLoadSingleplayerCommentaryDialog()
{
	if ( !m_hNewGameDialog.Get() )
	{
		m_hNewGameDialog = new CNewGameDialog(this,true);
		PositionDialog( m_hNewGameDialog );
	}

	((CNewGameDialog *)m_hNewGameDialog.Get())->SetCommentaryMode( true );
	m_hNewGameDialog->Activate();
}

void CBaseModPanel::OnOpenAchievementsDialog()
{
	//if (!m_hAchievementsDialog.Get())
	//{
	//	m_hAchievementsDialog = new CAchievementsDialog( this );
	//	PositionDialog(m_hAchievementsDialog);
	//}
	//m_hAchievementsDialog->Activate();
}

void CBaseModPanel::OnOpenCSAchievementsDialog()
{
	if ( GameClientExports() )
	{
		int screenWide = 0;
		int screenHeight = 0;
		engine->GetScreenSize( screenWide, screenHeight );

		// [smessick] For lower resolutions, open the Steam achievements instead of the CSS achievements screen.
		if ( screenWide < GameClientExports()->GetAchievementsPanelMinWidth() )
		{
			ISteamFriends *friends = steamapicontext->SteamFriends();
			if ( friends )
			{
				// Steam overlay not available on console (PS3)
#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
				friends->ActivateGameOverlay( "Achievements" );
#endif
			}
		}
		else
		{
			// Display the CSS achievements screen.
			GameClientExports()->CreateAchievementsPanel( this );
			GameClientExports()->DisplayAchievementPanel();
		}
	}
}

void CBaseModPanel::OnOpenAchievementsDialog_Xbox()
{
#if 0
	if (!m_hAchievementsDialog.Get())
	{
		m_hAchievementsDialog = new CAchievementsDialog_XBox( this );
		PositionDialog(m_hAchievementsDialog);
	}
	m_hAchievementsDialog->Activate();
#else
	AssertMsg( false, "Fixme" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: moves the game menu button to the right place on the taskbar
//-----------------------------------------------------------------------------
void CBaseModPanel::PositionDialog(vgui::PHandle dlg)
{
	if (!dlg.Get())
		return;

	int x, y, ww, wt, wide, tall;
	vgui::surface()->GetWorkspaceBounds( x, y, ww, wt );
	dlg->GetSize(wide, tall);

	// Center it, keeping requested size
	dlg->SetPos(x + ((ww - wide) / 2), y + ((wt - tall) / 2));
}

//-----------------------------------------------------------------------------
// Purpose: Add an Xbox 360 message dialog to a dialog stack
//-----------------------------------------------------------------------------
void CBaseModPanel::ShowMessageDialog( const uint nType, vgui::Panel *pOwner )
{
	if ( pOwner == NULL )
	{
		pOwner = this;
	}

	m_MessageDialogHandler.ShowMessageDialog( nType, pOwner );
}

//-----------------------------------------------------------------------------
// Purpose: Add an Xbox 360 message dialog to a dialog stack
//-----------------------------------------------------------------------------
void CBaseModPanel::CloseMessageDialog( const uint nType )
{
	m_MessageDialogHandler.CloseMessageDialog( nType );
}

//-----------------------------------------------------------------------------
// Purpose: System notification from engine
//-----------------------------------------------------------------------------
void CBaseModPanel::SystemNotification( const int notification )
{
	if ( notification == SYSTEMNOTIFY_USER_SIGNEDIN )
	{
// dgoodenough - limit this to X360
// PS3_BUILDFIX
// FIXME - do we want to have somehting here for PS3?
#if defined( _X360 )
		// See if it was the active user who signed in
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
		uint state = XUserGetSigninState( XBX_GetActiveUserId() );
		if ( state != eXUserSigninState_NotSignedIn )
		{
			// Reset a bunch of state
			m_bUserRefusedSignIn = false;
			m_bUserRefusedStorageDevice = false;
			m_bStorageBladeShown = false;
		}	
		UpdateRichPresenceInfo();
#pragma message( __FILE__ "(" __LINE__AS_STRING ") : warning custom: Best guess at a fix for the two lines below to get rotted code compiling" )
		//engine->GetAchievementMgr()->DownloadUserData();
		//engine->GetAchievementMgr()->EnsureGlobalStateLoaded();
		engine->GetAchievementMgr()->InitializeAchievements();
#endif
	}
	else if ( notification == SYSTEMNOTIFY_USER_SIGNEDOUT  )
	{
// dgoodenough - limit this to X360
// PS3_BUILDFIX
// FIXME - do we want to have somehting here for PS3?
#if defined( _X360 )
		UpdateRichPresenceInfo();

		// See if it was the active user who signed out
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
		uint state = XUserGetSigninState( XBX_GetActiveUserId() );
		if ( state != eXUserSigninState_NotSignedIn )
		{
			return;
		}

		// Invalidate their storage ID
#pragma message( __FILE__ "(" __LINE__AS_STRING ") : warning custom: Slamming controller for xbox storage id to 0" )
		engine->OnStorageDeviceDetached( 0 );
		m_bUserRefusedStorageDevice = false;
		m_bUserRefusedSignIn = false;
		m_iStorageID = XBX_INVALID_STORAGE_ID;
		engine->GetAchievementMgr()->InitializeAchievements();
		m_MessageDialogHandler.CloseAllMessageDialogs();

#endif
		if ( GameUI().IsInLevel() )
		{
			if ( m_pGameLogo )
			{
				m_pGameLogo->SetVisible( false );
			}

			// Hide the standard game menu
			for ( int i = 0; i < m_pGameMenuButtons.Count(); ++i )
			{
				m_pGameMenuButtons[i]->SetVisible( false );
			}

			// Hide the BasePanel's button footer
			m_pGameMenu->ShowFooter( false );

			QueueCommand( "QuitNoConfirm" );
		}
		else
		{
			CloseBaseDialogs();
		}

		OnCommand( "OpenMainMenu" );
	}
	else if ( notification == SYSTEMNOTIFY_STORAGEDEVICES_CHANGED )
	{
		if ( m_hSaveGameDialog_Xbox.Get() )
			m_hSaveGameDialog_Xbox->OnCommand( "RefreshSaveGames" );
		if ( m_hLoadGameDialog_Xbox.Get() )
			m_hLoadGameDialog_Xbox->OnCommand( "RefreshSaveGames" );

		// FIXME: This code is incorrect, they do NOT need a storage device, it is only recommended that they do
		if ( GameUI().IsInLevel() )
		{
			// They wanted to use a storage device and are already playing!
			// They need a storage device now or we're quitting the game!
			m_bNeedStorageDeviceHandle = true;
			ShowMessageDialog( MD_STORAGE_DEVICES_NEEDED, this );
		}
		else
		{
			ShowMessageDialog( MD_STORAGE_DEVICES_CHANGED, this );
		}
	}
	else if ( notification == SYSTEMNOTIFY_XUIOPENING )
	{
		m_bXUIVisible = true;
	}
	else if ( notification == SYSTEMNOTIFY_XUICLOSED )
	{
		m_bXUIVisible = false;

		if ( m_bWaitingForStorageDeviceHandle )
		{
			DWORD ret = xboxsystem->GetOverlappedResult( m_hStorageDeviceChangeHandle, NULL, true );
			if ( ret != ERROR_IO_INCOMPLETE )
			{
				// Done waiting
				xboxsystem->ReleaseAsyncHandle( m_hStorageDeviceChangeHandle );
				
				m_bWaitingForStorageDeviceHandle = false;
				
				// If we selected something, validate it
				if ( m_iStorageID != XBX_INVALID_STORAGE_ID )
				{
					// Check to see if there is enough room on this storage device
					ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
					if ( xboxsystem->DeviceCapacityAdequate( XBX_GetActiveUserId(), m_iStorageID, COM_GetModDirectory() ) == false )
					{
						ShowMessageDialog( MD_STORAGE_DEVICES_TOO_FULL, this );
						m_bStorageBladeShown = false; // Show the blade again next time
						m_strPostPromptCommand = ""; // Clear the buffer, we can't return
					}
					else
					{
						m_bNeedStorageDeviceHandle = false;

						// Set the storage device
						XBX_SetStorageDeviceId( XBX_GetActiveUserId(), m_iStorageID );
						OnDeviceAttached();
					}
				}
				else
				{
					if ( m_pStorageDeviceValidatedNotify )
					{
						*m_pStorageDeviceValidatedNotify = 2;
						m_pStorageDeviceValidatedNotify = NULL;
					}
					else if ( m_bNeedStorageDeviceHandle )
					{
						// They didn't select a storage device!
						// Remind them that they must pick one or the game will shut down
						ShowMessageDialog( MD_STORAGE_DEVICES_NEEDED, this );
					}
					else
					{
						// Start off the command we queued up
						IssuePostPromptCommand();
					}
				}
			}
		}
		
		// If we're waiting for the user to sign in, and check if they selected a usable profile
		if ( m_bWaitingForUserSignIn )
		{
			// Done waiting
			m_bWaitingForUserSignIn = false;
			m_bUserRefusedSignIn = false;

			// The UI has closed, so go off and revalidate the state
			if ( m_strPostPromptCommand.IsEmpty() == false )
			{
				// Run the command again
				OnCommand( m_strPostPromptCommand );
			}
		}

		RunQueuedCommands();
	}
	else if ( notification == SYSTEMNOTIFY_INVITE_SHUTDOWN )
	{
		// Quit the current game without confirmation
		m_bRestartFromInvite = true;
		m_bXUIVisible = true;
		OnCommand( "QuitNoConfirm" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnChangeStorageDevice( void )
{
	if ( m_bWaitingForStorageDeviceHandle == false )
	{
		m_bWaitingForStorageDeviceHandle = true;
		m_hStorageDeviceChangeHandle = xboxsystem->CreateAsyncHandle();
		m_iStorageID = XBX_INVALID_STORAGE_ID;

		ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
		xboxsystem->ShowDeviceSelector( XBX_GetActiveUserId(), true, &m_iStorageID, &m_hStorageDeviceChangeHandle );
	}
}

void CBaseModPanel::OnCreditsFinished( void )
{
	if ( !IsGameConsole() )
	{
		// valid for 360 only
		Assert( 0 );
		return;
	}

	bool bExitToAppChooser = false;
	if ( bExitToAppChooser )
	{
		// unknown state from engine, force to a compliant exiting state
		// causes an complete exit out of the game back to the app launcher
		SetVisible( true );
		m_pGameMenu->SetAlpha( 0 );
		StartExitingProcess();
	}
	else
	{
		// expecting to transition from the credits back to the background map
		// prevent any possibility of using the last transition image
		m_bUseRenderTargetImage = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnGameUIHidden()
{
	// [jason] Dismiss Pause menu if we close it via ESC key, etc
	if ( IsScaleformPauseMenuEnabled() && IsScaleformPauseMenuActive() )
	{
		DismissPauseMenu();
	}

	if ( m_hOptionsDialog.Get() )
	{
		PostMessage( m_hOptionsDialog.Get(), new KeyValues( "GameUIHidden" ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the alpha of the menu panels
//-----------------------------------------------------------------------------
void CBaseModPanel::SetMenuAlpha(int alpha)
{
	if ( GameUI().IsConsoleUI() )
	{
		// handled by animation, not code
		return;
	}

	m_pGameMenu->SetAlpha(alpha);

	if ( m_pGameLogo )
	{
		m_pGameLogo->SetAlpha( alpha );
	}

	for ( int i=0; i<m_pGameMenuButtons.Count(); ++i )
	{
		m_pGameMenuButtons[i]->SetAlpha(alpha);
	}
	m_bForceTitleTextUpdate = true;
}

//-----------------------------------------------------------------------------
// Purpose: starts the game
//-----------------------------------------------------------------------------
void CBaseModPanel::FadeToBlackAndRunEngineCommand( const char *engineCommand )
{
	KeyValues *pKV = new KeyValues( "RunEngineCommand", "command", engineCommand );

	// execute immediately, with no delay
	PostMessage( this, pKV, 0 );
}

void CBaseModPanel::SetMenuItemBlinkingState( const char *itemName, bool state )
{
	for (int i = 0; i < GetChildCount(); i++)
	{
		Panel *child = GetChild(i);
		CGameMenu *pGameMenu = dynamic_cast<CGameMenu *>(child);
		if ( pGameMenu )
		{
			pGameMenu->SetMenuItemBlinkingState( itemName, state );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: runs an engine command, used for delays
//-----------------------------------------------------------------------------
void CBaseModPanel::RunEngineCommand(const char *command)
{
	engine->ClientCmd_Unrestricted(command);
}

//-----------------------------------------------------------------------------
// Purpose: runs an animation to close a dialog and cleans up after close
//-----------------------------------------------------------------------------
void CBaseModPanel::RunCloseAnimation( const char *animName )
{
	RunAnimationWithCallback( this, animName, new KeyValues( "FinishDialogClose" ) );
}

//-----------------------------------------------------------------------------
// Purpose: cleans up after a menu closes
//-----------------------------------------------------------------------------
void CBaseModPanel::FinishDialogClose( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Load the two version numbers (code and content) from file to
// display them on the main menu.
//-----------------------------------------------------------------------------
void CBaseModPanel::LoadVersionNumbers()
{
	KeyValues *pVersionSettings = new KeyValues( "MainGameMenuScreen.res" );
	if ( pVersionSettings->LoadFromFile( g_pFullFileSystem, "resource/MainGameMenuScreen.res" ) )
	{
		m_pCodeVersionLabel->ApplySettings( pVersionSettings->FindKey( "CodeVersionLabel" ) );
		m_pContentVersionLabel->ApplySettings( pVersionSettings->FindKey( "ContentVersionLabel" ) );
	}

	const unsigned int bufferSize = 255;
	const wchar_t *pInvalidVersionText = L"_ _ _ _ _ _";

	// Load the code version number.
	Assert( m_pCodeVersionLabel != NULL );
	const char *codeVersionFileName = "resource\\css_code_version.txt";
	const char *codeVersionFileNameOfficial = "resource\\css_code_version_local.txt";
	wchar_t codeVersion[bufferSize];
	codeVersion[0] = L'\0';
	bool bSuccess = LoadVersionNumber( codeVersionFileName, codeVersionFileNameOfficial, codeVersion, bufferSize );
	m_pCodeVersionLabel->SetText( bSuccess ? codeVersion : pInvalidVersionText );

	// Load the content version number.
	Assert( m_pContentVersionLabel != NULL );
	const char *contentVersionFileName = "resource\\css_content_version.txt";
	const char *contentVersionFileNameOfficial = "resource\\css_content_version_local.txt";
	wchar_t contentVersion[bufferSize];
	contentVersion[0] = L'\0';
	bSuccess = LoadVersionNumber( contentVersionFileName, contentVersionFileNameOfficial, contentVersion, bufferSize );
	m_pContentVersionLabel->SetText( bSuccess ? contentVersion : pInvalidVersionText );
}

//-----------------------------------------------------------------------------
// Purpose: Load a version number from the most recent of two files into the given buffer.
//-----------------------------------------------------------------------------
bool CBaseModPanel::LoadVersionNumber( const char *fileNameA, const char *fileNameB, wchar_t *pVersionBuffer, unsigned int versionBufferSizeBytes )
{
	Assert( fileNameA != NULL );
	Assert( fileNameB != NULL );
	Assert( pVersionBuffer != NULL );
	Assert( versionBufferSizeBytes > 0 );

	const char *fileName = NULL;

	long timeA = g_pFullFileSystem->GetFileTime( (char*)fileNameA );
	long timeB = g_pFullFileSystem->GetFileTime( (char*)fileNameB );

	if ( timeA == -1 || timeA <= timeB )
	{
		fileName = fileNameB;
	}
	else if ( timeB == -1 || timeB <= timeA )
	{
		fileName = fileNameA;
	}

	if ( !fileName )
	{
		Msg( "Error opening version files \"%s\" and \"%s\"\n", fileNameA, fileNameB );
		return false;
	}

	FileHandle_t file = g_pFullFileSystem->Open( fileName, "rb" );
	if ( !file )
	{
		Msg( "Error opening version file \"%s\"\n", fileName );
		return false;
	}

	// Get the file and buffer sizes.
	int fileSize = g_pFullFileSystem->Size( file );
	int bufferSize = g_pFullFileSystem->GetOptimalReadSize( file, fileSize + sizeof(wchar_t) );
	if ( fileSize == 0 ||
		 bufferSize == 0 )
	{
		Msg( "Invalid version file \"%s\"\n", fileName );
		g_pFullFileSystem->Close( file );
		return false;
	}

	// Read the file into the buffer.
	wchar_t *memBlock = (wchar_t *)g_pFullFileSystem->AllocOptimalReadBuffer( file, bufferSize );
	V_memset( memBlock, 0, bufferSize );
	bool bReadOK = ( g_pFullFileSystem->ReadEx( memBlock, bufferSize, fileSize, file ) != 0 );
	const unsigned int memBlockCount = bufferSize / sizeof(wchar_t);

	// Close the file.
	g_pFullFileSystem->Close( file );

	if ( !bReadOK )
	{
		Msg( "Error reading version file \"%s\"\n", fileName );
		g_pFullFileSystem->FreeOptimalReadBuffer( memBlock );
		return false;
	}

	// Check the first character, make sure this a little-endian unicode file.
	wchar_t *data = memBlock;
	wchar_t signature = LittleShort( data[0] );
	if ( !bReadOK || signature != 0xFEFF )
	{
		Msg( "Invalid non-unicode version file \"%s\"\n", fileName );
		g_pFullFileSystem->FreeOptimalReadBuffer( memBlock );
		return false;
	}

	// Ensure little-endian unicode reads correctly on all platforms.
	CByteswap byteSwap;
	byteSwap.SetTargetBigEndian( false );
	byteSwap.SwapBufferToTargetEndian( data, data, memBlockCount );

	// Remove unwanted characters from the beginning of the buffer.
	wchar_t *pTextEnd = data + memBlockCount;
	while ( data != pTextEnd )
	{
		if ( *data == L'\0' )
		{
			break;
		}
		else if ( iswalnum( *data ) != 0 ||
				  iswpunct( *data ) != 0 )
		{
			break;
		}

		++data;
	}

	// Null terminate the version string.
	wchar_t *pCur = data;
	while ( pCur != pTextEnd )
	{
		if ( iswalnum( *pCur ) != 0 ||
			 iswpunct( *pCur ) != 0 ||
			 *pCur == L' ' )
		{
			++pCur;
			continue;
		}

		*pCur = L'\0';
		break;
	}

	// Copy the contents of the file into the given text buffer.
	V_wcsncpy( pVersionBuffer, data, versionBufferSizeBytes );

	// Free the buffer.
	g_pFullFileSystem->FreeOptimalReadBuffer( memBlock );

	return true;
}

/*
//-----------------------------------------------------------------------------
// Purpose: xbox UI panel that displays button icons and help text for all menus
//-----------------------------------------------------------------------------
CFooterPanel::CFooterPanel( Panel *parent, const char *panelName ) : BaseClass( parent, panelName ) 
{
	SetVisible( true );
	SetAlpha( 0 );
	m_pHelpName = NULL;

	m_pSizingLabel = new vgui::Label( this, "SizingLabel", "" );
	m_pSizingLabel->SetVisible( false );

	m_nButtonGap = 32;
	m_nButtonGapDefault = 32;
	m_ButtonPinRight = 100;
	m_FooterTall = 80;

	int wide, tall;
	surface()->GetScreenSize(wide, tall);

	if ( tall <= 480 )
	{
		m_FooterTall = 60;
	}

	m_ButtonOffsetFromTop = 0;
	m_ButtonSeparator = 4;
	m_TextAdjust = 0;

	m_bPaintBackground = false;
	m_bCenterHorizontal = false;

	m_szButtonFont[0] = '\0';
	m_szTextFont[0] = '\0';
	m_szFGColor[0] = '\0';
	m_szBGColor[0] = '\0';
}

CFooterPanel::~CFooterPanel()
{
	SetHelpNameAndReset( NULL );

	delete m_pSizingLabel;
}

//-----------------------------------------------------------------------------
// Purpose: apply scheme settings
//-----------------------------------------------------------------------------
void CFooterPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_hButtonFont = pScheme->GetFont( ( m_szButtonFont[0] != '\0' ) ? m_szButtonFont : "GameUIButtons" );
	m_hTextFont = pScheme->GetFont( ( m_szTextFont[0] != '\0' ) ? m_szTextFont : "MenuLarge" );

	SetFgColor( pScheme->GetColor( m_szFGColor, Color( 255, 255, 255, 255 ) ) );
	SetBgColor( pScheme->GetColor( m_szBGColor, Color( 0, 0, 0, 255 ) ) );

	int x, y, w, h;
	GetParent()->GetBounds( x, y, w, h );
	SetBounds( x, h - m_FooterTall, w, m_FooterTall );
}

//-----------------------------------------------------------------------------
// Purpose: apply settings
//-----------------------------------------------------------------------------
void CFooterPanel::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	// gap between hints
	m_nButtonGap = inResourceData->GetInt( "buttongap", 32 );
	m_nButtonGapDefault = m_nButtonGap;
	m_ButtonPinRight = inResourceData->GetInt( "button_pin_right", 100 );
	m_FooterTall = inResourceData->GetInt( "tall", 80 );
	m_ButtonOffsetFromTop = inResourceData->GetInt( "buttonoffsety", 0 );
	m_ButtonSeparator = inResourceData->GetInt( "button_separator", 4 );
	m_TextAdjust = inResourceData->GetInt( "textadjust", 0 );

	m_bCenterHorizontal = ( inResourceData->GetInt( "center", 0 ) == 1 );
	m_bPaintBackground = ( inResourceData->GetInt( "paintbackground", 0 ) == 1 );

	// fonts for text and button
	Q_strncpy( m_szTextFont, inResourceData->GetString( "fonttext", "MenuLarge" ), sizeof( m_szTextFont ) );
	Q_strncpy( m_szButtonFont, inResourceData->GetString( "fontbutton", "GameUIButtons" ), sizeof( m_szButtonFont ) );

	// fg and bg colors
	Q_strncpy( m_szFGColor, inResourceData->GetString( "fgcolor", "White" ), sizeof( m_szFGColor ) );
	Q_strncpy( m_szBGColor, inResourceData->GetString( "bgcolor", "Black" ), sizeof( m_szBGColor ) );

	for ( KeyValues *pButton = inResourceData->GetFirstSubKey(); pButton != NULL; pButton = pButton->GetNextKey() )
	{
		const char *pName = pButton->GetName();

		if ( !Q_stricmp( pName, "button" ) )
		{
			// Add a button to the footer
			const char *pText = pButton->GetString( "text", "NULL" );
			const char *pIcon = pButton->GetString( "icon", "NULL" );
			AddNewButtonLabel( pText, pIcon );
		}
	}

	InvalidateLayout( false, true ); // force ApplySchemeSettings to run
}

//-----------------------------------------------------------------------------
// Purpose: adds button icons and help text to the footer panel when activating a menu
//-----------------------------------------------------------------------------
void CFooterPanel::AddButtonsFromMap( vgui::Frame *pMenu )
{
	SetHelpNameAndReset( pMenu->GetName() );

	CControllerMap *pMap = dynamic_cast<CControllerMap*>( pMenu->FindChildByName( "ControllerMap" ) );
	if ( pMap )
	{
		int buttonCt = pMap->NumButtons();
		for ( int i = 0; i < buttonCt; ++i )
		{
			const char *pText = pMap->GetBindingText( i );
			if ( pText )
			{
				AddNewButtonLabel( pText, pMap->GetBindingIcon( i ) );
			}
		}
	}
}

void CFooterPanel::SetStandardDialogButtons()
{
	SetHelpNameAndReset( "Dialog" );
	AddNewButtonLabel( "#GameUI_Action", "#GameUI_Icons_A_BUTTON" );
	AddNewButtonLabel( "#GameUI_Close", "#GameUI_Icons_B_BUTTON" );
}

//-----------------------------------------------------------------------------
// Purpose: Caller must tag the button layout. May support reserved names
// to provide stock help layouts trivially.
//-----------------------------------------------------------------------------
void CFooterPanel::SetHelpNameAndReset( const char *pName )
{
	if ( m_pHelpName )
	{
		free( m_pHelpName );
		m_pHelpName = NULL;
	}

	if ( pName )
	{
		m_pHelpName = strdup( pName );
	}

	ClearButtons();
}

//-----------------------------------------------------------------------------
// Purpose: Caller must tag the button layout
//-----------------------------------------------------------------------------
const char *CFooterPanel::GetHelpName()
{
	return m_pHelpName;
}

void CFooterPanel::ClearButtons( void )
{
	m_ButtonLabels.PurgeAndDeleteElements();
}

//-----------------------------------------------------------------------------
// Purpose: creates a new button label with icon and text
//-----------------------------------------------------------------------------
void CFooterPanel::AddNewButtonLabel( const char *text, const char *icon )
{
	ButtonLabel_t *button = new ButtonLabel_t;

	Q_strncpy( button->name, text, MAX_PATH );
	button->bVisible = true;

	// Button icons are a single character
	wchar_t *pIcon = g_pVGuiLocalize->Find( icon );
	if ( pIcon )
	{
		button->icon[0] = pIcon[0];
		button->icon[1] = '\0';
	}
	else
	{
		button->icon[0] = '\0';
	}

	// Set the help text
	wchar_t *pText = g_pVGuiLocalize->Find( text );
	if ( pText )
	{
		wcsncpy( button->text, pText, wcslen( pText ) + 1 );
	}
	else
	{
		button->text[0] = '\0';
	}

	m_ButtonLabels.AddToTail( button );
}

//-----------------------------------------------------------------------------
// Purpose: Shows/Hides a button label
//-----------------------------------------------------------------------------
void CFooterPanel::ShowButtonLabel( const char *name, bool show )
{
	for ( int i = 0; i < m_ButtonLabels.Count(); ++i )
	{
		if ( !Q_stricmp( m_ButtonLabels[ i ]->name, name ) )
		{
			m_ButtonLabels[ i ]->bVisible = show;
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Changes a button's text
//-----------------------------------------------------------------------------
void CFooterPanel::SetButtonText( const char *buttonName, const char *text )
{
	for ( int i = 0; i < m_ButtonLabels.Count(); ++i )
	{
		if ( !Q_stricmp( m_ButtonLabels[ i ]->name, buttonName ) )
		{
			wchar_t *wtext = g_pVGuiLocalize->Find( text );
			if ( text )
			{
				wcsncpy( m_ButtonLabels[ i ]->text, wtext, wcslen( wtext ) + 1 );
			}
			else
			{
				m_ButtonLabels[ i ]->text[ 0 ] = '\0';
			}
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Footer panel background rendering
//-----------------------------------------------------------------------------
void CFooterPanel::PaintBackground( void )
{
	if ( !m_bPaintBackground )
		return;

	BaseClass::PaintBackground();
}

//-----------------------------------------------------------------------------
// Purpose: Footer panel rendering
//-----------------------------------------------------------------------------
void CFooterPanel::Paint( void )
{
	// inset from right edge
	int wide = GetWide();
	int right = wide - m_ButtonPinRight;

	// center the text within the button
	int buttonHeight = vgui::surface()->GetFontTall( m_hButtonFont );
	int fontHeight = vgui::surface()->GetFontTall( m_hTextFont );
	int textY = ( buttonHeight - fontHeight )/2 + m_TextAdjust;

	if ( textY < 0 )
	{
		textY = 0;
	}

	int y = m_ButtonOffsetFromTop;

	if ( !m_bCenterHorizontal )
	{
		// draw the buttons, right to left
		int x = right;

		for ( int i = 0; i < m_ButtonLabels.Count(); ++i )
		{
			ButtonLabel_t *pButton = m_ButtonLabels[i];
			if ( !pButton->bVisible )
				continue;

			// Get the string length
			m_pSizingLabel->SetFont( m_hTextFont );
			m_pSizingLabel->SetText( pButton->text );
			m_pSizingLabel->SizeToContents();

			int iTextWidth = m_pSizingLabel->GetWide();

			if ( iTextWidth == 0 )
				x += m_nButtonGap;	// There's no text, so remove the gap between buttons
			else
				x -= iTextWidth;

			// Draw the string
			vgui::surface()->DrawSetTextFont( m_hTextFont );
			vgui::surface()->DrawSetTextColor( GetFgColor() );
			vgui::surface()->DrawSetTextPos( x, y + textY );
			vgui::surface()->DrawPrintText( pButton->text, wcslen( pButton->text ) );

			// Draw the button
			// back up button width and a little extra to leave a gap between button and text
			x -= ( vgui::surface()->GetCharacterWidth( m_hButtonFont, pButton->icon[0] ) + m_ButtonSeparator );
			vgui::surface()->DrawSetTextFont( m_hButtonFont );
			vgui::surface()->DrawSetTextColor( 255, 255, 255, 255 );
			vgui::surface()->DrawSetTextPos( x, y );
			vgui::surface()->DrawPrintText( pButton->icon, 1 );

			// back up to next string
			x -= m_nButtonGap;
		}
	}
	else
	{
		// center the buttons (as a group)
		int x = wide / 2;
		int totalWidth = 0;
		int i = 0;
		int nButtonCount = 0;

		// need to loop through and figure out how wide our buttons and text are (with gaps between) so we can offset from the center
		for ( i = 0; i < m_ButtonLabels.Count(); ++i )
		{
			ButtonLabel_t *pButton = m_ButtonLabels[i];
			if ( !pButton->bVisible )
				continue;

			// Get the string length
			m_pSizingLabel->SetFont( m_hTextFont );
			m_pSizingLabel->SetText( pButton->text );
			m_pSizingLabel->SizeToContents();

			totalWidth += vgui::surface()->GetCharacterWidth( m_hButtonFont, pButton->icon[0] );
			totalWidth += m_ButtonSeparator;
			totalWidth += m_pSizingLabel->GetWide();

			nButtonCount++; // keep track of how many active buttons we'll be drawing
		}

		totalWidth += ( nButtonCount - 1 ) * m_nButtonGap; // add in the gaps between the buttons
		x -= ( totalWidth / 2 );

		for ( i = 0; i < m_ButtonLabels.Count(); ++i )
		{
			ButtonLabel_t *pButton = m_ButtonLabels[i];
			if ( !pButton->bVisible )
				continue;

			// Get the string length
			m_pSizingLabel->SetFont( m_hTextFont );
			m_pSizingLabel->SetText( pButton->text );
			m_pSizingLabel->SizeToContents();

			int iTextWidth = m_pSizingLabel->GetWide();

			// Draw the icon
			vgui::surface()->DrawSetTextFont( m_hButtonFont );
			vgui::surface()->DrawSetTextColor( 255, 255, 255, 255 );
			vgui::surface()->DrawSetTextPos( x, y );
			vgui::surface()->DrawPrintText( pButton->icon, 1 );
			x += vgui::surface()->GetCharacterWidth( m_hButtonFont, pButton->icon[0] ) + m_ButtonSeparator;

			// Draw the string
			vgui::surface()->DrawSetTextFont( m_hTextFont );
			vgui::surface()->DrawSetTextColor( GetFgColor() );
			vgui::surface()->DrawSetTextPos( x, y + textY );
			vgui::surface()->DrawPrintText( pButton->text, wcslen( pButton->text ) );
			
			x += iTextWidth + m_nButtonGap;
		}
	}
}	

DECLARE_BUILD_FACTORY( CFooterPanel );
*/

#ifdef _GAMECONSOLE
//-----------------------------------------------------------------------------
// Purpose: Reload the resource files on the Xbox 360
//-----------------------------------------------------------------------------
void CBaseModPanel::Reload_Resources( const CCommand &args )
{
	m_pConsoleControlSettings->Clear();
	m_pConsoleControlSettings->LoadFromFile( g_pFullFileSystem, "resource/UI/XboxDialogs.res" );
}
#endif


// X360TBD: Move into a separate module when completed
CMessageDialogHandler::CMessageDialogHandler()
{
	m_iDialogStackTop = -1;
}
void CMessageDialogHandler::ShowMessageDialog( int nType, vgui::Panel *pOwner )
{
	int iSimpleFrame = 0;
	if ( ModInfo().IsSinglePlayerOnly() )
	{
		iSimpleFrame = MD_SIMPLEFRAME;
	}

	switch( nType )
	{
	case MD_SEARCHING_FOR_GAMES:
		CreateMessageDialog( MD_CANCEL|MD_RESTRICTPAINT,
							NULL, 
							"#TF_Dlg_SearchingForGames", 
							NULL,
							"CancelOperation",
							pOwner,
							true ); 
		break;

	case MD_CREATING_GAME:
		CreateMessageDialog( MD_RESTRICTPAINT,
							NULL, 
							"#TF_Dlg_CreatingGame", 
							NULL,
							NULL,
							pOwner,
							true ); 
		break;

	case MD_SESSION_SEARCH_FAILED:
		CreateMessageDialog( MD_YESNO|MD_RESTRICTPAINT, 
							NULL, 
							"#TF_Dlg_NoGamesFound", 
							"ShowSessionOptionsDialog",
							"ReturnToMainMenu",
							pOwner ); 
		break;

	case MD_SESSION_CREATE_FAILED:
		CreateMessageDialog( MD_OK, 
							NULL, 
							"#TF_Dlg_CreateFailed", 
							"ReturnToMainMenu", 
							NULL,
							pOwner );
		break;

	case MD_SESSION_CONNECTING:
		CreateMessageDialog( 0, 
							NULL, 
							"#TF_Dlg_Connecting", 
							NULL, 
							NULL,
							pOwner,
							true );
		break;

	case MD_SESSION_CONNECT_NOTAVAILABLE:
		CreateMessageDialog( MD_OK, 
							NULL, 
							"#TF_Dlg_JoinRefused", 
							"ReturnToMainMenu", 
							NULL,
							pOwner );
		break;

	case MD_SESSION_CONNECT_SESSIONFULL:
		CreateMessageDialog( MD_OK, 
							NULL, 
							"#TF_Dlg_GameFull", 
							"ReturnToMainMenu", 
							NULL,
							pOwner );
		break;

	case MD_SESSION_CONNECT_FAILED:
		CreateMessageDialog( MD_OK, 
							NULL, 
							"#TF_Dlg_JoinFailed", 
							"ReturnToMainMenu", 
							NULL,
							pOwner );
		break;

	case MD_LOST_HOST:
		CreateMessageDialog( MD_OK|MD_RESTRICTPAINT, 
							NULL, 
							"#TF_Dlg_LostHost", 
							"ReturnToMainMenu", 
							NULL,
							pOwner );
		break;

	case MD_LOST_SERVER:
		CreateMessageDialog( MD_OK|MD_RESTRICTPAINT, 
							NULL, 
							"#TF_Dlg_LostServer", 
							"ReturnToMainMenu", 
							NULL,
							pOwner );
		break;

	case MD_MODIFYING_SESSION:
		CreateMessageDialog( MD_RESTRICTPAINT, 
							NULL, 
							"#TF_Dlg_ModifyingSession", 
							NULL, 
							NULL,
							pOwner,
							true );
		break;

	case MD_SAVE_BEFORE_QUIT:
		CreateMessageDialog( MD_YESNO|iSimpleFrame|MD_RESTRICTPAINT, 
							"#GameUI_QuitConfirmationTitle", 
							"#GameUI_Console_QuitWarning", 
							"QuitNoConfirm", 
							"CloseQuitDialog_OpenMainMenu",
							pOwner );
		break;

	case MD_QUIT_CONFIRMATION:
		CreateMessageDialog( MD_YESNO|iSimpleFrame|MD_RESTRICTPAINT, 
							 "#GameUI_QuitConfirmationTitle", 
							 "#GameUI_QuitConfirmationText", 
							 "QuitNoConfirm", 
							 "CloseQuitDialog_OpenMainMenu",
							 pOwner );
		break;

	case MD_QUIT_CONFIRMATION_TF:
		CreateMessageDialog( MD_YESNO|MD_RESTRICTPAINT, 
							 "#GameUI_QuitConfirmationTitle", 
							 "#GameUI_QuitConfirmationText", 
							 "QuitNoConfirm", 
							 "CloseQuitDialog_OpenMatchmakingMenu",
							 pOwner );
		break;

	case MD_DISCONNECT_CONFIRMATION:
		CreateMessageDialog( MD_YESNO|MD_RESTRICTPAINT, 
							"", 
							"#GameUI_DisconnectConfirmationText", 
							"DisconnectNoConfirm", 
							"close_dialog",
							pOwner );
		break;

	case MD_DISCONNECT_CONFIRMATION_HOST:
		CreateMessageDialog( MD_YESNO|MD_RESTRICTPAINT, 
							"", 
							"#GameUI_DisconnectHostConfirmationText", 
							"DisconnectNoConfirm", 
							"close_dialog",
							pOwner );
		break;

	case MD_KICK_CONFIRMATION:
		CreateMessageDialog( MD_YESNO, 
							"", 
							"#TF_Dlg_ConfirmKick", 
							"KickPlayer", 
							"close_dialog",
							pOwner );
		break;

	case MD_CLIENT_KICKED:
		CreateMessageDialog( MD_OK|MD_RESTRICTPAINT, 
							"", 
							"#TF_Dlg_ClientKicked", 
							"close_dialog", 
							NULL,
							pOwner );
		break;

	case MD_EXIT_SESSION_CONFIRMATION:
		CreateMessageDialog( MD_YESNO, 
							"", 
							"#TF_Dlg_ExitSessionText", 
							"ReturnToMainMenu", 
							"close_dialog",
							pOwner );
		break;

	case MD_STORAGE_DEVICES_NEEDED:
		CreateMessageDialog( MD_YESNO|MD_WARNING|MD_COMMANDAFTERCLOSE|iSimpleFrame|MD_RESTRICTPAINT, 
							 "#GameUI_Console_StorageRemovedTitle", 
							 "#GameUI_Console_StorageNeededBody", 
							 "ShowDeviceSelector", 
							 "QuitNoConfirm",
							 pOwner );
		break;

	case MD_STORAGE_DEVICES_CHANGED:
		CreateMessageDialog( MD_YESNO|MD_WARNING|MD_COMMANDAFTERCLOSE|iSimpleFrame|MD_RESTRICTPAINT, 
							"#GameUI_Console_StorageRemovedTitle", 
							"#GameUI_Console_StorageRemovedBody", 
							"ShowDeviceSelector", 
							"clear_storage_deviceID",
							pOwner );
		break;

	case MD_STORAGE_DEVICES_TOO_FULL:
		CreateMessageDialog( MD_YESNO|MD_WARNING|MD_COMMANDAFTERCLOSE|iSimpleFrame|MD_RESTRICTPAINT, 
							 "#GameUI_Console_StorageTooFullTitle", 
							 "#GameUI_Console_StorageTooFullBody", 
							 "ShowDeviceSelector", 
							 "StorageDeviceDenied",
							 pOwner );
		break;

	case MD_PROMPT_STORAGE_DEVICE:
		CreateMessageDialog( MD_YESNO|MD_WARNING|MD_COMMANDAFTERCLOSE|iSimpleFrame|MD_RESTRICTPAINT, 
							 "#GameUI_Console_NoStorageDeviceSelectedTitle", 
							 "#GameUI_Console_NoStorageDeviceSelectedBody", 
							 "ShowDeviceSelector", 
							 "StorageDeviceDenied",
							 pOwner );
		break;

	case MD_PROMPT_STORAGE_DEVICE_REQUIRED:
		CreateMessageDialog( MD_YESNO|MD_WARNING|MD_COMMANDAFTERCLOSE|MD_SIMPLEFRAME, 
							"#GameUI_Console_NoStorageDeviceSelectedTitle", 
							"#GameUI_Console_StorageDeviceRequiredBody", 
							"ShowDeviceSelector", 
							"RequiredStorageDenied",
							pOwner );
		break;

	case MD_PROMPT_SIGNIN:
		CreateMessageDialog( MD_YESNO|MD_WARNING|MD_COMMANDAFTERCLOSE|iSimpleFrame, 
							 "#GameUI_Console_NoUserProfileSelectedTitle", 
							 "#GameUI_Console_NoUserProfileSelectedBody", 
							 "ShowSignInUI", 
							 "SignInDenied",
							 pOwner );
		break;

	case MD_PROMPT_SIGNIN_REQUIRED:
		CreateMessageDialog( MD_YESNO|MD_WARNING|MD_COMMANDAFTERCLOSE|iSimpleFrame, 
							"#GameUI_Console_NoUserProfileSelectedTitle", 
							"#GameUI_Console_UserProfileRequiredBody", 
							"ShowSignInUI", 
							"RequiredSignInDenied",
							pOwner );
		break;

	case MD_NOT_ONLINE_ENABLED:
		CreateMessageDialog( MD_YESNO|MD_WARNING, 
							"", 
							"#TF_Dlg_NotOnlineEnabled", 
							"ShowSigninUI", 
							"close_dialog",
							pOwner );
		break;

	case MD_NOT_ONLINE_SIGNEDIN:
		CreateMessageDialog( MD_YESNO|MD_WARNING, 
							"", 
							"#TF_Dlg_NotOnlineSignedIn", 
							"ShowSigninUI", 
							"close_dialog",
							pOwner );
		break;

	case MD_DEFAULT_CONTROLS_CONFIRM:
		CreateMessageDialog( MD_YESNO|MD_WARNING|iSimpleFrame|MD_RESTRICTPAINT, 
							 "#GameUI_RestoreDefaults", 
							 "#GameUI_ControllerSettingsText", 
							 "DefaultControls", 
							 "close_dialog",
							 pOwner );
		break;

	case MD_AUTOSAVE_EXPLANATION:
		CreateMessageDialog( MD_OK|MD_WARNING|iSimpleFrame|MD_RESTRICTPAINT, 
							 "#GameUI_ConfirmNewGame_Title", 
							 "#GameUI_AutoSave_Console_Explanation", 
							 "StartNewGameNoCommentaryExplanation", 
							 NULL,
							 pOwner );
		break;

	case MD_COMMENTARY_EXPLANATION:
		CreateMessageDialog( MD_OK|MD_WARNING|iSimpleFrame|MD_RESTRICTPAINT, 
							 "#GameUI_CommentaryDialogTitle", 
							 "#GAMEUI_Commentary_Console_Explanation", 
							 "StartNewGameNoCommentaryExplanation", 
							 NULL,
							 pOwner );
		break;

	case MD_COMMENTARY_EXPLANATION_MULTI:
		CreateMessageDialog( MD_OK|MD_WARNING, 
							 "#GameUI_CommentaryDialogTitle", 
							 "#GAMEUI_Commentary_Console_Explanation", 
							 "StartNewGameNoCommentaryExplanation", 
							 NULL,
							 pOwner );
		break;

	case MD_COMMENTARY_CHAPTER_UNLOCK_EXPLANATION:
		CreateMessageDialog( MD_OK|MD_WARNING|iSimpleFrame|MD_RESTRICTPAINT, 
							 "#GameUI_CommentaryDialogTitle", 
							 "#GameUI_CommentaryUnlock", 
							 "close_dialog", 
							 NULL,
							 pOwner );
		break;
		
	case MD_SAVE_BEFORE_LANGUAGE_CHANGE:
		CreateMessageDialog( MD_YESNO|MD_WARNING|MD_SIMPLEFRAME|MD_COMMANDAFTERCLOSE|MD_RESTRICTPAINT, 
							 "#GameUI_ChangeLanguageRestart_Title", 
							 "#GameUI_ChangeLanguageRestart_Info", 
							 "AcceptVocalsLanguageChange", 
							 "CancelVocalsLanguageChange",
							 pOwner );

	case MD_SAVE_BEFORE_NEW_GAME:
		CreateMessageDialog( MD_OKCANCEL|MD_WARNING|iSimpleFrame|MD_COMMANDAFTERCLOSE|MD_RESTRICTPAINT, 
							 "#GameUI_ConfirmNewGame_Title", 
							 "#GameUI_NewGameWarning", 
							 "StartNewGame", 
							 "close_dialog",
							 pOwner );
		break;

	case MD_SAVE_BEFORE_LOAD:
		CreateMessageDialog( MD_OKCANCEL|MD_WARNING|iSimpleFrame|MD_COMMANDAFTERCLOSE|MD_RESTRICTPAINT, 
							 "#GameUI_ConfirmLoadGame_Title", 
							 "#GameUI_LoadWarning", 
							 "LoadGame", 
							 "LoadGameCancelled",
							 pOwner );
		break;

	case MD_DELETE_SAVE_CONFIRM:
		CreateMessageDialog( MD_OKCANCEL|MD_WARNING|iSimpleFrame|MD_COMMANDAFTERCLOSE, 
							 "#GameUI_ConfirmDeleteSaveGame_Title", 
							 "#GameUI_ConfirmDeleteSaveGame_Info", 
							 "DeleteGame", 
							 "DeleteGameCancelled",
							 pOwner );
		break;

	case MD_SAVE_OVERWRITE:
		CreateMessageDialog( MD_OKCANCEL|MD_WARNING|iSimpleFrame|MD_COMMANDAFTERCLOSE, 
							 "#GameUI_ConfirmOverwriteSaveGame_Title", 
							 "#GameUI_ConfirmOverwriteSaveGame_Info", 
							 "SaveGame", 
							 "OverwriteGameCancelled",
							 pOwner );
		break;

	case MD_SAVING_WARNING:
		CreateMessageDialog( MD_WARNING|iSimpleFrame|MD_COMMANDONFORCECLOSE, 
							 "",
							 "#GameUI_SavingWarning", 
							 "SaveSuccess", 
							 NULL,
							 pOwner,
							 true);
		break;

	case MD_SAVE_COMPLETE:
		CreateMessageDialog( MD_OK|iSimpleFrame|MD_COMMANDAFTERCLOSE, 
							 "#GameUI_ConfirmOverwriteSaveGame_Title", 
							 "#GameUI_GameSaved", 
							 "CloseAndSelectResume", 
							 NULL,
							 pOwner );
		break;

	case MD_LOAD_FAILED_WARNING:
		CreateMessageDialog( MD_OK |MD_WARNING|iSimpleFrame, 
			"#GameUI_LoadFailed", 
			"#GameUI_LoadFailed_Description", 
			"close_dialog", 
			NULL,
			pOwner );
		break;

	case MD_OPTION_CHANGE_FROM_X360_DASHBOARD:
		CreateMessageDialog( MD_OK|iSimpleFrame|MD_RESTRICTPAINT, 
							 "#GameUI_SettingChangeFromX360Dashboard_Title", 
							 "#GameUI_SettingChangeFromX360Dashboard_Info", 
							 "close_dialog", 
							 NULL,
							 pOwner );
		break;

	case MD_STANDARD_SAMPLE:
		CreateMessageDialog( MD_OK, 
							"Standard Dialog", 
							"This is a standard dialog", 
							"close_dialog", 
							NULL,
							pOwner );
		break;

	case MD_WARNING_SAMPLE:
		CreateMessageDialog( MD_OK | MD_WARNING,
							"#GameUI_Dialog_Warning", 
							"This is a warning dialog", 
							"close_dialog", 
							NULL,
							pOwner );
		break;

	case MD_ERROR_SAMPLE:
		CreateMessageDialog( MD_OK | MD_ERROR, 
							"Error Dialog", 
							"This is an error dialog", 
							"close_dialog", 
							NULL,
							pOwner );
		break;

	case MD_STORAGE_DEVICES_CORRUPT:
		CreateMessageDialog( MD_OK | MD_WARNING | iSimpleFrame | MD_RESTRICTPAINT,
			"", 
			"#GameUI_Console_FileCorrupt", 
			"close_dialog", 
			NULL,
			pOwner );
		break;

	case MD_CHECKING_STORAGE_DEVICE:
		CreateMessageDialog( iSimpleFrame | MD_RESTRICTPAINT,
			NULL, 
			"#GameUI_Dlg_CheckingStorageDevice",
			NULL,
			NULL,
			pOwner,
			true ); 
		break;

	default:
		break;
	}
}

void CMessageDialogHandler::CloseAllMessageDialogs()
{
	for ( int i = 0; i < MAX_MESSAGE_DIALOGS; ++i )
	{
		CMessageDialog *pDlg = m_hMessageDialogs[i];
		if ( pDlg )
		{
			vgui::surface()->RestrictPaintToSinglePanel(NULL);
			if ( vgui_message_dialog_modal.GetBool() )
			{
				vgui::input()->ReleaseAppModalSurface();
			}

			pDlg->Close();
			m_hMessageDialogs[i] = NULL;
		}
	}
}

void CMessageDialogHandler::CloseMessageDialog( const uint nType )
{
	int nStackIdx = 0;
	if ( nType & MD_WARNING )
	{
		nStackIdx = DIALOG_STACK_IDX_WARNING;
	}
	else if ( nType & MD_ERROR )
	{
		nStackIdx = DIALOG_STACK_IDX_ERROR;
	}

	CMessageDialog *pDlg = m_hMessageDialogs[nStackIdx];
	if ( pDlg )
	{
		vgui::surface()->RestrictPaintToSinglePanel(NULL);
		if ( vgui_message_dialog_modal.GetBool() )
		{
			vgui::input()->ReleaseAppModalSurface();
		}

		pDlg->Close();
		m_hMessageDialogs[nStackIdx] = NULL;
	}
}

void CMessageDialogHandler::CreateMessageDialog( const uint nType, const char *pTitle, const char *pMsg, const char *pCmdA, const char *pCmdB, vgui::Panel *pCreator, bool bShowActivity /*= false*/ )
{
	int nStackIdx = 0;
	if ( nType & MD_WARNING )
	{
		nStackIdx = DIALOG_STACK_IDX_WARNING;
	}
	else if ( nType & MD_ERROR )
	{
		nStackIdx = DIALOG_STACK_IDX_ERROR;
	}

	// Can only show one dialog of each type at a time
	if ( m_hMessageDialogs[nStackIdx].Get() )
	{
		Warning( "Tried to create two dialogs of type %d\n", nStackIdx );
		return;
	}

	// Show the new dialog
	m_hMessageDialogs[nStackIdx] = new CMessageDialog( BasePanel(), nType, pTitle, pMsg, pCmdA, pCmdB, pCreator, bShowActivity );

	m_hMessageDialogs[nStackIdx]->SetControlSettingsKeys( BasePanel()->GetConsoleControlSettings()->FindKey( "MessageDialog.res" ) );

	if ( nType & MD_RESTRICTPAINT )
	{
		vgui::surface()->RestrictPaintToSinglePanel( m_hMessageDialogs[nStackIdx]->GetVPanel() );
	}

	ActivateMessageDialog( nStackIdx );	
}

//-----------------------------------------------------------------------------
// Purpose: Activate a new message dialog
//-----------------------------------------------------------------------------
void CMessageDialogHandler::ActivateMessageDialog( int nStackIdx )
{
	int x, y, wide, tall;
	vgui::surface()->GetWorkspaceBounds( x, y, wide, tall );
	PositionDialog( m_hMessageDialogs[nStackIdx], wide, tall );

	uint nType = m_hMessageDialogs[nStackIdx]->GetType();
	if ( nType & MD_WARNING )
	{
		m_hMessageDialogs[nStackIdx]->SetZPos( 75 );
	}
	else if ( nType & MD_ERROR )
	{
		m_hMessageDialogs[nStackIdx]->SetZPos( 100 );
	}

	// Make sure the topmost item on the stack still has focus
	int idx = MAX_MESSAGE_DIALOGS - 1;
	for ( idx; idx >= nStackIdx; --idx )
	{
		CMessageDialog *pDialog = m_hMessageDialogs[idx];
		if ( pDialog )
		{
			pDialog->Activate();
			if ( vgui_message_dialog_modal.GetBool() )
			{
				vgui::input()->SetAppModalSurface( pDialog->GetVPanel() );
			}
			m_iDialogStackTop = idx;
			break;
		}
	}
}

void CMessageDialogHandler::PositionDialogs( int wide, int tall )
{
	for ( int i = 0; i < MAX_MESSAGE_DIALOGS; ++i )
	{
		if ( m_hMessageDialogs[i].Get() )
		{
			PositionDialog( m_hMessageDialogs[i], wide, tall );
		}
	}
}

void CMessageDialogHandler::PositionDialog( vgui::PHandle dlg, int wide, int tall )
{
	int w, t;
	dlg->GetSize(w, t);
	dlg->SetPos( (wide - w) / 2, (tall - t) / 2 );
}			

//-----------------------------------------------------------------------------
// Purpose: Editable panel that can replace the GameMenuButtons in CBaseModPanel
//-----------------------------------------------------------------------------
CMainMenuGameLogo::CMainMenuGameLogo( vgui::Panel *parent, const char *name ) : vgui::EditablePanel( parent, name )
{
	m_nOffsetX = 0;
	m_nOffsetY = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMainMenuGameLogo::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	m_nOffsetX = inResourceData->GetInt( "offsetX", 0 );
	m_nOffsetY = inResourceData->GetInt( "offsetY", 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMainMenuGameLogo::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/GameLogo.res" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::CloseBaseDialogs( void )
{
	if ( m_hNewGameDialog.Get() )
		m_hNewGameDialog->Close();

	if ( m_hBonusMapsDialog.Get() )
		m_hBonusMapsDialog->Close();

	if ( m_hLoadGameDialog_Xbox.Get() )
		m_hLoadGameDialog_Xbox->Close();

	if ( m_hSaveGameDialog_Xbox.Get() )
		m_hSaveGameDialog_Xbox->Close();

	if ( m_hLoadCommentaryDialog.Get() )
		m_hLoadCommentaryDialog->Close();

	if ( m_hCreateMultiplayerGameDialog.Get() )
		m_hCreateMultiplayerGameDialog->Close();
}


#if !defined( CSTRIKE15 )
//-----------------------------------------------------------------------------
// Purpose: Console command to show the main menu
//-----------------------------------------------------------------------------
CON_COMMAND( show_main_menu, "Show the main menu" )
{
	BasePanel()->ShowMainMenu( true );
}

//-----------------------------------------------------------------------------
// Purpose: Console command to hide the main menu
//-----------------------------------------------------------------------------
CON_COMMAND( hide_main_menu, "Hide the main menu" )
{
	BasePanel()->ShowMainMenu( false );
}
#endif // !defined( CSTRIKE15 )

//Removed because these were used as an exploit to get to old VGUI panels and configure video options. Not relevant for end users anyways.
// -----------------------------------------------------------------------------
// Purpose: Console command to show the main menu
// -----------------------------------------------------------------------------
// CON_COMMAND( show_sf_main_menu, "Show the Scaleform main menu" )
// {
// 	BasePanel()->EnableScaleformMainMenu( true );
// 	BasePanel()->ShowMainMenu( false );
// 	BasePanel()->OnOpenCreateMainMenuScreen();
// }
// 
// 
// -----------------------------------------------------------------------------
// Purpose: Console command to hide the main menu
// -----------------------------------------------------------------------------
// CON_COMMAND( hide_sf_main_menu, "Hide the Scaleform main menu" )
// {
// 	BasePanel()->EnableScaleformMainMenu( false );
// 	BasePanel()->DismissMainMenuScreen();
// 	BasePanel()->ShowMainMenu( true );
// }

//-----------------------------------------------------------------------------
// Purpose: Show or hide the main menu
//-----------------------------------------------------------------------------
void CBaseModPanel::ShowMainMenu( bool bShow )
{
	if (m_bMainMenuShown == bShow)
		return;

	SetVisible( bShow );
	SetMenuAlpha( bShow ? 255 : 0 );

	if ( m_pGameLogo )
	{
		m_pGameLogo->SetVisible( bShow );
	}

	if ( m_pGameMenu )
	{
		m_pGameMenu->ShowFooter( bShow );

		m_pGameMenu->SetEnabled( bShow );
		m_pGameMenu->SetVisible( bShow );

		for ( int i = 0; i < m_pGameMenu->GetItemCount(); ++i )
		{
			m_pGameMenu->GetMenuItem( i )->SetEnabled( bShow );
			m_pGameMenu->GetMenuItem( i )->SetVisible( bShow );
		}
	}

	// Hide the standard game menu
	for ( int i = 0; i < m_pGameMenuButtons.Count(); ++i )
	{
		m_pGameMenuButtons[i]->SetEnabled( bShow );
		m_pGameMenuButtons[i]->SetVisible( bShow );
	}

	if ( bShow )
	{
		if ( GameUI().IsConsoleUI() )
		{
			ArmFirstMenuItem();
			m_pConsoleAnimationController->StartAnimationSequence( "InitializeUILayout" );
			m_pConsoleAnimationController->StartAnimationSequence( "OpenMainMenu" );
			m_bFadingInMenus = false;
		}
	}

	m_bMainMenuShown = bShow;
}

bool CBaseModPanel::LoadingProgressWantsIsolatedRender( bool bContextValid )
{
	return CLoadingScreenScaleform::LoadingProgressWantsIsolatedRender( bContextValid );
}

