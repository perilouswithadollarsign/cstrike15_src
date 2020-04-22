//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#include "vsoundtest.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "EngineInterface.h"
#include "IGameUIFuncs.h"
#include "gameui_util.h"
#include "vgui/ISurface.h"
#include "VGenericConfirmation.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

CSoundTest::CSoundTest( Panel *pParent, const char *pPanelName ):
BaseClass( pParent, pPanelName )
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	SetDialogTitle( "#PORTAL2_SoundTest_Title" );

	SetFooterEnabled( true );
	UpdateFooter();
}

CSoundTest::~CSoundTest()
{
}

void CSoundTest::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	UpdateFooter();
}

void CSoundTest::Activate()
{
	BaseClass::Activate();

	UpdateFooter();
}

void CSoundTest::OnKeyCodePressed( KeyCode code )
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

void CSoundTest::OnCommand(const char *command)
{
	if ( !V_stricmp( "Cancel", command ) || !V_stricmp( "Back", command ) )
	{
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void CSoundTest::OnThink()
{
	BaseClass::OnThink();
}

void CSoundTest::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		pFooter->SetButtons( FB_BBUTTON );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
	}
}