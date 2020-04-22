//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#include "cbase.h"
#include "vcoopexitchoice.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "EngineInterface.h"
#include "IGameUIFuncs.h"
#include "gameui_util.h"
#include "vgui/ISurface.h"
#include "VGenericConfirmation.h"
#include "vportalleaderboard.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

extern int g_nPortalScoreTempUpdate;
extern int g_nTimeScoreTempUpdate;

//=============================================================================
static void LeaveGameOkCallback()
{
	COM_TimestampedLog( "Exit Game" );

	CUIGameData::Get()->GameStats_ReportAction( "challenge_quit", engine->GetLevelNameShort(), g_nTimeScoreTempUpdate );

	CPortalLeaderboardPanel* self = 
		static_cast< CPortalLeaderboardPanel* >( CBaseModPanel::GetSingleton().GetWindow( WT_PORTALLEADERBOARDHUD ) );

	if ( self )
	{
		self->Close();
	}

	GameUI().HideGameUI();

	if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
	{
		// Closing an active session results in disconnecting from the game.
		g_pMatchFramework->CloseSession();
	}
	else
	{
		// On PC people can be playing via console bypassing matchmaking
		// and required session settings, so to leave game duplicate
		// session closure with an extra "disconnect" command.
		engine->ExecuteClientCmd( "disconnect" );
	}

	GameUI().ActivateGameUI();
	GameUI().AllowEngineHideGameUI();


	CBaseModPanel::GetSingleton().CloseAllWindows();
	CBaseModPanel::GetSingleton().OpenFrontScreen();
}




//=============================================================================
static void GoToHubOkCallback()
{
	GameUI().AllowEngineHideGameUI();
	CPortalLeaderboardPanel* pSelf = 
		static_cast< CPortalLeaderboardPanel* >( CBaseModPanel::GetSingleton().GetWindow( WT_COOPEXITCHOICE ) );

	if ( pSelf )
	{
		bool bWaitScreen =  CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_WaitScreen_GoingToHub", 0.0f, NULL );
		pSelf->PostMessage( pSelf, new KeyValues( "MsgPreGoToHub" ), bWaitScreen ? 2.0f : 0.0f );
	}
}



CCoopExitChoice::CCoopExitChoice( Panel *pParent, const char *pPanelName ):
BaseClass( pParent, pPanelName )
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	//SetDialogTitle( "#PORTAL2_SoundTest_Title" );

	SetFooterEnabled( true );
	UpdateFooter();
}

CCoopExitChoice::~CCoopExitChoice()
{
}

void CCoopExitChoice::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	UpdateFooter();
}

void CCoopExitChoice::Activate()
{
	BaseClass::Activate();

	UpdateFooter();
}

void CCoopExitChoice::OnKeyCodePressed( KeyCode code )
{
	int joystick = GetJoystickForCode( code );
	int userId = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	if ( joystick != userId || joystick < 0 )
	{	
		return;
	}

	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		BaseClass::OnKeyCodePressed( code );
		break;

	default:
		BaseClass::OnKeyCodePressed(code);
		break;
	}
}

void CCoopExitChoice::OnCommand(const char *command)
{
	if ( !V_stricmp( "Cancel", command ) || !V_stricmp( "Back", command ) )
	{
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
	else if ( !V_stricmp("BtnExitToMainMenu", command ) )
	{
		// main menu confirmation panel
		GenericConfirmation* confirmation = 
			static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#L4D360UI_LeaveMultiplayerConf";
		data.pMessageText = "#L4D360UI_LeaveMultiplayerConfMsg";
		if ( GameRules() && GameRules()->IsMultiplayer() )
			data.pMessageText = "#L4D360UI_LeaveMultiplayerConfMsgOnline";
#ifdef _GAMECONSOLE
		if ( XBX_GetNumGameUsers() > 1 )
			data.pMessageText = "#L4D360UI_LeaveMultiplayerConfMsgSS";
#endif
		data.bOkButtonEnabled = true;
		data.pOkButtonText = "#PORTAL2_ButtonAction_Exit";
		data.pfnOkCallback = &LeaveGameOkCallback;
		data.bCancelButtonEnabled = true;

		confirmation->SetUsageData(data);
	}
	else if ( !V_stricmp( "BtnGoToHub", command ) )
	{
		CUIGameData::Get()->GameStats_ReportAction( "challenge_hub", engine->GetLevelNameShort(), g_nTimeScoreTempUpdate );
		
		GenericConfirmation* confirmation = 
			static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#Portal2UI_GoToHubQ";
		data.pMessageText = "#Portal2UI_GoToHubConfMsg";
		data.pfnOkCallback = &GoToHubOkCallback;

		data.bOkButtonEnabled = true;
		data.bCancelButtonEnabled = true;

		confirmation->SetUsageData(data);
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void CCoopExitChoice::OnThink()
{
	BaseClass::OnThink();
}

void CCoopExitChoice::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		pFooter->SetButtons( FB_ABUTTON | FB_BBUTTON );
		pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
	}
}

void CCoopExitChoice::MsgPreGoToHub()
{
	engine->ServerCmd( "pre_go_to_hub" );
	PostMessage( this, new KeyValues( "MsgGoToHub" ), 1.0f );
}

void CCoopExitChoice::MsgGoToHub()
{
	engine->ServerCmd( "go_to_hub" );
}