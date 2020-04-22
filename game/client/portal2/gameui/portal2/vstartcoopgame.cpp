//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "vstartcoopgame.h"

#include "VGenericPanelList.h"
#include "EngineInterface.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "VDropDownMenu.h"
#include "VFlyoutMenu.h"
#include "UIGameData.h"
#include "vdownloadcampaign.h"
#include "gameui_util.h"

#include "vgui/ISurface.h"
#include "vgui/IBorder.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui/ILocalize.h"
#include "VGenericConfirmation.h"
#include "VGameSettings.h"
#include "vgetlegacydata.h"

#include "fmtstr.h"
#include "smartptr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

char CStartCoopGame::sm_szGameMode[64] = {0};
char CStartCoopGame::sm_szChallengeMap[64] = {0};

CStartCoopGame::CStartCoopGame( vgui::Panel *parent, const char *panelName ) : BaseClass( parent, panelName, false, true )
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );

	SetDialogTitle( "#Portal2UI_PlayCoop_Header" );
	SetFooterEnabled( true );
	
	memset( sm_szGameMode, 0, sizeof( sm_szGameMode ) ); // make sm_szGameMode static
}

void CStartCoopGame::OnKeyCodePressed(vgui::KeyCode code)
{
	BaseClass::OnKeyCodePressed( code );

	if ( IsX360() && ( GetBaseButtonCode( code ) == KEY_XBUTTON_Y ) )
	{
		OnCommand( "BtnPlayLan" );
	}
}

void CStartCoopGame::SetDataSettings( KeyValues *pSettings )
{
	V_strncpy( sm_szGameMode, pSettings->GetString( "mode", "coop" ), sizeof( sm_szGameMode ) );
	V_strncpy( sm_szChallengeMap, pSettings->GetString( "challenge_map", "default" ), sizeof( sm_szChallengeMap ) );
}

void CStartCoopGame::OnCommand( char const *szCommand )
{
	if ( !Q_strcmp( szCommand, "BtnPlayOnline" ) )
	{
		// See if the user doesn't have privileges to invite
		// his friends, then there's no sense to go and see
		// a friends list, do the match with random online
		// partner right away
		// if ( CUIGameData::Get()->CanSendLiveGameInviteToUser( 0ull ) )
		{
			CUIGameData::Get()->InitiateOnlineCoopPlay( this, "playonline", sm_szGameMode, sm_szChallengeMap );
		}
// 		else
// 		{
// 			CUIGameData::Get()->InitiateOnlineCoopPlay( this, "quickmatch" );
// 		}
		return;
	}
	else if ( !Q_strcmp( szCommand, "BtnPlayLan" ) )
	{
		CUIGameData::Get()->InitiateOnlineCoopPlay( this, "playlan", sm_szGameMode, sm_szChallengeMap );
		return;
	}
	else if ( !Q_strcmp( szCommand, "BtnPlaySplitscreen" ) )
	{
		CUIGameData::Get()->InitiateSplitscreenPartnerDetection( sm_szGameMode, sm_szChallengeMap );
		return;
	}
	else if( !Q_strcmp( szCommand, "Back" ) )
	{
		// Act as though 360 back button was pressed
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		return;
	}

	BaseClass::OnCommand( szCommand );
}

void CStartCoopGame::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	if ( vgui::Panel *panel = FindChildByName( "BtnPlayOnline" ) )
	{
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom();
		}
		panel->NavigateTo();
	}

	UpdateFooter();
}

void CStartCoopGame::Activate()
{
	BaseClass::Activate();
	UpdateFooter();
}

void CStartCoopGame::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		pFooter->SetButtons( FB_ABUTTON | FB_BBUTTON | ( IsX360() ? FB_YBUTTON : FB_NONE ) );
		pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
		if ( IsX360() )
		{
			pFooter->SetButtonText( FB_YBUTTON, "#Portal2UI_PlayLan_Footer" );
		}
	}
}

