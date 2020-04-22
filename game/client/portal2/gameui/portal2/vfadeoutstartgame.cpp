//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: Transition from the menu into the game, cannot be stopped
//
//=====================================================================================//

#include "vfadeoutstartgame.h"
#include "vfooterpanel.h"
#include "vgui/ISurface.h"
#include "transitionpanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

CFadeOutStartGame::CFadeOutStartGame( vgui::Panel *pParent, const char *pPanelName ) : 
	BaseClass( pParent, pPanelName, false, false )
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );
	SetFooterEnabled( false );
	SetPaintBackgroundEnabled( true );

	AddFrameListener( this );

	m_bStarted = false;
	m_nFrames = 0;

	int screenWide, screenTall;
	surface()->GetScreenSize( screenWide, screenTall );

	SetPos( 0, 0 );
	SetSize( screenWide, screenTall );

	// marking this very early, solves some odd timing problems getting to this screen where the effect is not as stable as desired
	CBaseModPanel::GetSingleton().GetTransitionEffectPanel()->MarkTilesInRect( 0, 0, screenWide, screenTall, WT_FADEOUTSTARTGAME );

	UpdateFooter();
}

CFadeOutStartGame::~CFadeOutStartGame()
{
	RemoveFrameListener( this );
}

void CFadeOutStartGame::SetDataSettings( KeyValues *pSettings )
{
	m_MapName = pSettings->GetString( "map" );
	m_LoadFilename = pSettings->GetString( "loadfilename" );
	m_ReasonString = pSettings->GetString( "reason" );
}

void CFadeOutStartGame::Activate()
{
	BaseClass::Activate();
	UpdateFooter();
}

void CFadeOutStartGame::OnKeyCodePressed( KeyCode code )
{
	// absorb all key presses, this dialog cannot be canceled
}

void CFadeOutStartGame::RunFrame()
{
	// the transition effect may be disabled, ensure the game starts
	// otherwise, hold off until the transition effect has completed
	if ( !CBaseModPanel::GetSingleton().GetTransitionEffectPanel()->IsEffectEnabled() )
	{
		StartGame();
	}
	else if ( m_nFrames > 1 && !CBaseModPanel::GetSingleton().GetTransitionEffectPanel()->IsEffectActive() )
	{
		StartGame();
	}
}

void CFadeOutStartGame::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		// ensure the footer stays off
		pFooter->SetButtons( 0 );
	}
}

void CFadeOutStartGame::PaintBackground()
{
	// count the frames to ensure painting has occurred
	m_nFrames++;

	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );
	CBaseModPanel::GetSingleton().GetTransitionEffectPanel()->MarkTilesInRect( x, y, wide, tall, WT_FADEOUTSTARTGAME );

	// black out the entire screen
	surface()->DrawSetColor( Color( 0, 0, 0, 255 ) );
	surface()->DrawFilledRect( x, y, wide, tall );
}

void CFadeOutStartGame::StartGame()
{
	if ( !m_bStarted )
	{
		const char *pReason = m_ReasonString.Get();
		if ( !pReason || !pReason[0] )
		{
			// for safety, try to figure one out
			if ( !m_LoadFilename.IsEmpty() )
			{
				pReason = "load";
			}
			else
			{
				pReason = "newgame";
			}
		}

		// fires only once
		m_bStarted = true;

		IMatchSession *pSession = g_pMatchFramework->GetMatchSession();
		char const *szSessionMapName;
		szSessionMapName = pSession ? pSession->GetSessionSettings()->GetString( "game/map" ) : "";

		if ( !IsGameConsole() && 
			!m_LoadFilename.IsEmpty() && 
			GameUI().IsInLevel() && 
			szSessionMapName[0] &&
			!V_stricmp( szSessionMapName, m_MapName.Get() ) )
		{
			engine->ExecuteClientCmd( CFmtStr( "load %s", m_LoadFilename.Get() ) );
		}
		else if ( !V_stricmp( pReason, "coop_community" ) )
		{
			char szCommand[MAX_PATH];
			V_snprintf( szCommand, ARRAYSIZE( szCommand ), "map %s", m_MapName.Get() );
			engine->ClientCmd_Unrestricted( szCommand );
		}
		else if ( !V_stricmp( pReason, "coop_puzzlemaker_preview" ) )
		{
			char szCommand[MAX_PATH];
			V_snprintf( szCommand, ARRAYSIZE( szCommand ), "ss_map %s *mp", m_MapName.Get() );
			engine->ClientCmd_Unrestricted( szCommand );
		}
		else if ( !V_stricmp( pReason, "sp_puzzlemaker_preview" ) )
		{
			char szCommand[MAX_PATH];
			V_snprintf( szCommand, ARRAYSIZE( szCommand ), "map %s *sp", m_MapName.Get() );
			engine->ClientCmd_Unrestricted( szCommand );
		}
		else
		{
			CUIGameData::Get()->InitiateSinglePlayerPlay( 
				m_MapName, 
				!m_LoadFilename.IsEmpty() ? m_LoadFilename.Get() : NULL, 
				pReason );
		}
	}
}
