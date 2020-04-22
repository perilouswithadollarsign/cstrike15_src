//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "c_csrootpanel.h"
#include <vgui_controls/Controls.h>
#include <vgui/IVGui.h>
#include "clientmode_csnormal.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
C_CSRootPanel::C_CSRootPanel( vgui::VPANEL parent, int slot, const char *panelName /*= "CounterStrike Root Panel"*/ )
	: BaseClass( NULL, panelName ), m_nSplitSlot( slot )
{
	SetParent( parent );
	SetPaintEnabled( false );
	SetPaintBorderEnabled( false );
	SetPaintBackgroundEnabled( false );

	// This panel does post child painting
	SetPostChildPaintEnabled( true );

	int w, h;
	surface()->GetScreenSize( w, h );

	// Make it screen sized
	SetBounds( 0, 0, w, h );
	SetZPos( 0 );

	// Ask for OnTick messages
	vgui::ivgui()->AddTickSignal( GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_CSRootPanel::~C_CSRootPanel( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_CSRootPanel::PostChildPaint()
{
	BaseClass::PostChildPaint();

	// Draw all panel effects
	RenderPanelEffects();
}

//-----------------------------------------------------------------------------
// Purpose: For each panel effect, check if it wants to draw and draw it on
//  this panel/surface if so
//-----------------------------------------------------------------------------
void C_CSRootPanel::RenderPanelEffects( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_CSRootPanel::OnTick( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Reset effects on level load/shutdown
//-----------------------------------------------------------------------------
void C_CSRootPanel::LevelInit( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_CSRootPanel::LevelShutdown( void )
{
}

void C_CSRootPanel::PaintTraverse( bool Repaint, bool allowForce /*= true*/ )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( m_nSplitSlot);
	BaseClass::PaintTraverse( Repaint, allowForce );
}

void C_CSRootPanel::OnThink()
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( m_nSplitSlot );
	BaseClass::OnThink();
}
