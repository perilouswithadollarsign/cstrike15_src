//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#if defined( PORTAL2_PUZZLEMAKER )

#include "vplaytestuploadwait.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "EngineInterface.h"
#include "IGameUIFuncs.h"
#include "gameui_util.h"
#include "vgui/ISurface.h"
#include "VGenericConfirmation.h"
#include "vgui_controls/imagepanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;
using namespace BaseModUI;

CPlaytestUploadWait::CPlaytestUploadWait( Panel *pParent, const char *pPanelName ):
BaseClass( pParent, pPanelName )
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );
	SetPaintBackgroundEnabled( false );
	SetFooterEnabled( false );

	m_pSpinner = NULL;
}

CPlaytestUploadWait::~CPlaytestUploadWait()
{
	// repause the game behind the UI
	engine->ClientCmd_Unrestricted( "setpause nomsg" );
	GameUI().AllowEngineHideGameUI();
}

void CPlaytestUploadWait::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// set up the working spinner
	m_pSpinner = dynamic_cast< ImagePanel* >( FindChildByName( "WorkingAnim" ) );
	if ( m_pSpinner )
	{
		m_pSpinner->SetVisible( false );
	}
}

void CPlaytestUploadWait::Activate()
{
	BaseClass::Activate();
	// unpause the game behind the UI so the playtest entity can do its work
	engine->ClientCmd_Unrestricted( "unpause nomsg" );
	GameUI().PreventEngineHideGameUI();
}

void CPlaytestUploadWait::OnKeyCodePressed( KeyCode code )
{
	// do not process any input
	return;
}

void CPlaytestUploadWait::OnCommand(const char *command)
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

void CPlaytestUploadWait::OnThink()
{
	BaseClass::OnThink();
	// update the progress of the spinner
	ClockSpinner();
}

void CPlaytestUploadWait::ClockSpinner( void )
{
	// update the spinner
	if ( m_pSpinner )
	{
		int nAnimFrame = ( uint64 )( Plat_FloatTime() * 10 );
		m_pSpinner->SetFrame( nAnimFrame );
		m_pSpinner->SetVisible( true ); 
	}
}

#endif // PORTAL2_PUZZLEMAKER
