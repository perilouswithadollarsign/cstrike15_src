//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "vgui_int.h"
#include "ienginevgui.h"
#include "c_csrootpanel.h"
#include "view_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static C_CSRootPanel *g_pCSRootPanel[ MAX_SPLITSCREEN_PLAYERS ];
static C_CSRootPanel *g_pFullscreenRootPanel;

void VGui_GetPanelList( CUtlVector< Panel * > &list )
{
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		list.AddToTail( g_pCSRootPanel[ i ] );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGUI_CreateClientDLLRootPanel( void )
{
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		g_pCSRootPanel[ i ] = new C_CSRootPanel( enginevgui->GetPanel( PANEL_CLIENTDLL ), i );
	}

	g_pFullscreenRootPanel = new C_CSRootPanel( enginevgui->GetPanel( PANEL_CLIENTDLL ), 0, "Fullscreen Root Panel" );
	g_pFullscreenRootPanel->SetZPos( 1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGUI_DestroyClientDLLRootPanel( void )
{
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		delete g_pCSRootPanel[ i ];
		g_pCSRootPanel[ i ] = NULL;
	}

	delete g_pFullscreenRootPanel;
	g_pFullscreenRootPanel = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Game specific root panel
// Output : vgui::Panel
//-----------------------------------------------------------------------------
vgui::VPANEL VGui_GetClientDLLRootPanel( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return g_pCSRootPanel[ GET_ACTIVE_SPLITSCREEN_SLOT() ]->GetVPanel();
}

//-----------------------------------------------------------------------------
// Purpose: Fullscreen root panel for shared hud elements during splitscreen
// Output : vgui::Panel
//-----------------------------------------------------------------------------
vgui::Panel *VGui_GetFullscreenRootPanel( void )
{
	return g_pFullscreenRootPanel;
}

vgui::VPANEL VGui_GetFullscreenRootVPANEL( void )
{
	return g_pFullscreenRootPanel->GetVPanel();
}

//-----------------------------------------------------------------------------
// Purpose: Link into multiple sub views if client is rendering from multiple locations
// Output : CViewSetup linked list
//-----------------------------------------------------------------------------
void VGui_GetAllSubViews( int nSlot, const CViewSetup &orig_view, CUtlLinkedList< CViewSetup > &subviews, CUtlVector< vrect_t > &letterbox )
{
	subviews.AddToTail( orig_view );
	return;
}