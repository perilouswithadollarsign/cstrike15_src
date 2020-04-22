//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "cbase.h"

#include "VOptions.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "VGenericConfirmation.h"
#include "vgui_controls/Button.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

Options::Options( Panel *parent, const char *panelName ):
BaseClass( parent, panelName )
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	SetDialogTitle( "#PORTAL2_MainMenu_Options" );

	SetFooterEnabled( true );
	UpdateFooter();
}

Options::~Options()
{
}

void Options::Activate()
{
	BaseClass::Activate();
	UpdateFooter();
}

void Options::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		int nButtons = FB_BBUTTON;
		if ( IsGameConsole() )
		{
			nButtons |= FB_ABUTTON;
		}

		pFooter->SetButtons( nButtons );
		pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
	}
}

void Options::OnCommand( const char *pCommand )
{
	int iUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	int iController = XBX_GetUserId( iUserSlot );

	if ( UI_IsDebug() )
	{
		Msg("[GAMEUI] Handling options menu command %s from user%d ctrlr%d\n", pCommand, iUserSlot, iController );
	}

	if ( IsGameConsole() && !V_strcmp( pCommand, "AudioVideo" ) )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_AUDIOVIDEO, this );
	}
	else if ( !V_strcmp( pCommand, "Controller" ) )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_CONTROLLER, this, true,
			KeyValues::AutoDeleteInline( new KeyValues( "Settings", "slot", iUserSlot ) ) );
	}
	else if ( !IsGameConsole() && !V_stricmp( pCommand, "Audio" ) )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_AUDIO, this, true );
	}
	else if ( !IsGameConsole() && !V_stricmp( pCommand, "Video" ) )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_VIDEO, this, true );
	}
	else if ( !IsGameConsole() && !V_stricmp( pCommand, "KeyboardMouse" ) )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_KEYBOARDMOUSE, this, true );
	}
}