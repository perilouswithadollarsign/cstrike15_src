//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "vcoopmode.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "vgui_controls/Button.h"
#include "KeyValues.h"
#include "vgenericconfirmation.h"
#include "filesystem.h"
#include "vportalleaderboard.h"
#include "uigamedata.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;


CCoopMode::CCoopMode( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName, false, true )
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );

	SetDialogTitle( "#PORTAL2_CoopMode_Header" );
	SetFooterEnabled( true );
}


CCoopMode::~CCoopMode()
{
	// Unsubscribe from event notifications
	//g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );

	//RemoveFrameListener( this );
}


void CCoopMode::OnCommand( char const *pCommand )
{
	if ( !V_stricmp( pCommand, "OpenStandardCoopDialog" ) )
	{
#ifdef _GAMECONSOLE

		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			"mode coop"
			);
		KeyValues::AutoDelete autodelete( pSettings );

		if ( XBX_GetPrimaryUserIsGuest() )
			CUIGameData::Get()->InitiateSplitscreenPartnerDetection( "coop" );
		else
			CBaseModPanel::GetSingleton().OpenWindow( WT_STARTCOOPGAME, this, true, pSettings );
#else
		CUIGameData::Get()->InitiateOnlineCoopPlay( this, "playonline", "coop" );
#endif
		return;
	}
	else if ( !V_stricmp( pCommand, "OpenChallengeModeDialog" ) )
	{
		KeyValues *pSettings = new KeyValues( "Setting" );
		pSettings->SetInt( "state", STATE_MAIN_MENU );
		CBaseModPanel::GetSingleton().OpenWindow( WT_PORTALCOOPLEADERBOARD, this, true, KeyValues::AutoDeleteInline( pSettings ) );
		return;
	}
	else if ( !V_stricmp( pCommand, "Back" ) )
	{
		// Act as though 360 back button was pressed
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		return;
	}

	BaseClass::OnCommand( pCommand );
}


void CCoopMode::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	bool bPrimaryUserIsGuest = false;
#if defined( _GAMECONSOLE )
	bPrimaryUserIsGuest = ( XBX_GetPrimaryUserIsGuest() != 0 );
#endif
	SetControlEnabled( "BtnLoadGame", !bPrimaryUserIsGuest );

	vgui::Panel *pPanel = FindChildByName( "BtnStandardMode" );
	if ( pPanel )
	{
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom();
		}
		pPanel->NavigateTo();
	}

	UpdateFooter();

	//m_bFullySetup = true;
}

void CCoopMode::OnKeyCodePressed( vgui::KeyCode code )
{
	// On PC they can start splitscreen by pressing X on a second controller
	if ( IsPC() )
	{
		switch ( GetBaseButtonCode( code ) )
		{
		case KEY_XBUTTON_X:
			if ( GetJoystickForCode( code ) == 1 )
			{
				engine->ClientCmd( "ss_map mp_coop_lobby_3" );
				return;
			}
		}
	}

	BaseClass::OnKeyCodePressed( code );
}

void CCoopMode::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		int visibleButtons = FB_BBUTTON;
		if ( IsGameConsole() )
		{
			visibleButtons |= FB_ABUTTON;
		}

		pFooter->SetButtons( visibleButtons );
		pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
	}
}



void BaseModUI::CCoopMode::Activate()
{
	UpdateFooter();
	BaseClass::Activate();
}
