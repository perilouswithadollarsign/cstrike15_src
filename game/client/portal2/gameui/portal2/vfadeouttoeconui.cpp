//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: Transition from the menu into the game, cannot be stopped
//
//=====================================================================================//

#include "vfadeouttoeconui.h"
#include "vfooterpanel.h"
#include "vgui/ISurface.h"
#include "transitionpanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

CFadeOutToEconUI::CFadeOutToEconUI( vgui::Panel *pParent, const char *pPanelName ) : 
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
	CBaseModPanel::GetSingleton().GetTransitionEffectPanel()->MarkTilesInRect( 0, 0, screenWide, screenTall, WT_FADEOUTTOECONUI );

	UpdateFooter();
}

CFadeOutToEconUI::~CFadeOutToEconUI()
{
	RemoveFrameListener( this );
}

void CFadeOutToEconUI::Activate()
{
	BaseClass::Activate();
	UpdateFooter();
}

void CFadeOutToEconUI::OnKeyCodePressed( KeyCode code )
{
	// absorb all key presses, this dialog cannot be canceled
}

void CFadeOutToEconUI::RunFrame()
{
	// the transition effect may be disabled, ensure the game starts
	// otherwise, hold off until the transition effect has completed
	if ( !CBaseModPanel::GetSingleton().GetTransitionEffectPanel()->IsEffectEnabled() )
	{
		StartEconUI();
	}
	else if ( m_nFrames > 1 && !CBaseModPanel::GetSingleton().GetTransitionEffectPanel()->IsEffectActive() )
	{
		StartEconUI();
	}
}

void CFadeOutToEconUI::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		// ensure the footer stays off
		pFooter->SetButtons( 0 );
	}
}

void CFadeOutToEconUI::PaintBackground()
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

void CFadeOutToEconUI::StartEconUI()
{
	if ( !m_bStarted )
	{
		// fires only once
		m_bStarted = true;
		engine->ExecuteClientCmd( "open_econui" );
	}
}