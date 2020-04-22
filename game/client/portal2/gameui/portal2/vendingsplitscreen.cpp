//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "vendingsplitscreen.h"
#include "VAttractScreen.h"
#include "tier1/KeyValues.h"

#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"

#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;


//=============================================================================

ConVar ui_ending_splitscreen_time( "ui_ending_splitscreen_time", "1.0", FCVAR_DEVELOPMENTONLY );

EndingSplitscreenDialog::EndingSplitscreenDialog( Panel *parent, const char *panelName ):
	BaseClass( parent, panelName, false, false )
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );
	m_flTimeStart = Plat_FloatTime();
	if ( !m_flTimeStart )
		m_flTimeStart = 0.1;
}

void EndingSplitscreenDialog::OnThink()
{
	if ( !m_flTimeStart )
		return;

	if ( Plat_FloatTime() - m_flTimeStart > ui_ending_splitscreen_time.GetFloat() )
	{
		m_flTimeStart = 0;
		CBaseModPanel::GetSingleton().CloseAllWindows();
		CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL );
		return;
	}

	BaseClass::OnThink();
}

