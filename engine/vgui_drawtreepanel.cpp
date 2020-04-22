//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "client_pch.h"

#include <vgui/IClientPanel.h>
#include <vgui_controls/TreeView.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/CheckButton.h>
#include "convar.h"
#include "tier0/vprof.h"
#include "vgui_baseui_interface.h"
#include "vgui_helpers.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

bool g_bForceRefresh = true;

void ChangeCallback_RefreshDrawTree( IConVar *var, const char *pOldString, float flValue )
{
	g_bForceRefresh = true;
}

// Bunch of vgui debugging stuff
static ConVar vgui_drawtree( "vgui_drawtree", "0", FCVAR_CHEAT, "Draws the vgui panel hiearchy to the specified depth level." );
static ConVar vgui_drawtree_visible( "vgui_drawtree_visible", "1", 0, "Draw the visible panels.", ChangeCallback_RefreshDrawTree );
static ConVar vgui_drawtree_hidden( "vgui_drawtree_hidden", "0", 0, "Draw the hidden panels.", ChangeCallback_RefreshDrawTree );
static ConVar vgui_drawtree_popupsonly( "vgui_drawtree_popupsonly", "0", 0, "Draws the vgui popup list in hierarchy(1) or most recently used(2) order.", ChangeCallback_RefreshDrawTree );
static ConVar vgui_drawtree_freeze( "vgui_drawtree_freeze", "0", 0, "Set to 1 to stop updating the vgui_drawtree view.", ChangeCallback_RefreshDrawTree );
static ConVar vgui_drawtree_panelptr( "vgui_drawtree_panelptr", "0", 0, "Show the panel pointer values in the vgui_drawtree view.", ChangeCallback_RefreshDrawTree );
static ConVar vgui_drawtree_panelalpha( "vgui_drawtree_panelalpha", "0", 0, "Show the panel alpha values in the vgui_drawtree view.", ChangeCallback_RefreshDrawTree );
static ConVar vgui_drawtree_render_order( "vgui_drawtree_render_order", "0", 0, "List the vgui_drawtree panels in render order.", ChangeCallback_RefreshDrawTree );
static ConVar vgui_drawtree_bounds( "vgui_drawtree_bounds", "0", 0, "Show panel bounds.", ChangeCallback_RefreshDrawTree );
static ConVar vgui_drawtree_draw_selected( "vgui_drawtree_draw_selected", "0", 0, "Highlight the selected panel", ChangeCallback_RefreshDrawTree );
static ConVar vgui_drawtree_scheme( "vgui_drawtree_scheme", "0", 0, "Show scheme file for each panel", ChangeCallback_RefreshDrawTree );
extern ConVar vgui_drawfocus;


void vgui_drawtree_on_f()
{
	vgui_drawtree.SetValue( 1 );
}
void vgui_drawtree_off_f()
{
	vgui_drawtree.SetValue( 0 );
}
ConCommand vgui_drawtree_on( "+vgui_drawtree", vgui_drawtree_on_f );
ConCommand vgui_drawtree_off( "-vgui_drawtree", vgui_drawtree_off_f );


extern CUtlVector< VPANEL > g_FocusPanelList;
extern VPanelHandle g_DrawTreeSelectedPanel;


class CVGuiTree : public TreeView
{
public:
	typedef TreeView BaseClass;	


	CVGuiTree( Panel *parent, const char *pName ) : 
		BaseClass( parent, pName )
	{
		
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		SetFont( pScheme->GetFont( "ConsoleText", false ) );
		//SetBgColor( Color( 0, 0, 0, 175 ) );
		SetPaintBackgroundEnabled( false );
	}
};


class CDrawTreeFrame : public Frame
{
public:
	DECLARE_CLASS_SIMPLE( CDrawTreeFrame, Frame );
	
	CDrawTreeFrame( Panel *parent, const char *pName ) : 
		BaseClass( parent, pName )
	{
		// Init the frame.
		SetTitle( "VGUI Hierarchy", false );
		SetMenuButtonVisible( false );
		
		// Create the tree control itself.
		m_pTree = SETUP_PANEL( new CVGuiTree( this, "Tree view" ) );
		m_pTree->SetVisible( true );

		// Init the buttons.
		m_pShowVisible = SETUP_PANEL( new CConVarCheckButton( this, "show visible", "Show Visible" ) );
		m_pShowVisible->SetVisible( true );
		m_pShowVisible->SetConVar( &vgui_drawtree_visible );

		m_pShowHidden = SETUP_PANEL( new CConVarCheckButton( this, "show hidden", "Show Hidden" ) );
		m_pShowHidden->SetVisible( true );
		m_pShowHidden->SetConVar( &vgui_drawtree_hidden );

		m_pPopupsOnly = SETUP_PANEL( new CConVarCheckButton( this, "popups only", "Popups Only" ) );
		m_pPopupsOnly->SetVisible( true );
		m_pPopupsOnly->SetConVar( &vgui_drawtree_popupsonly );

		m_pDrawFocus = SETUP_PANEL( new CConVarCheckButton( this, "draw focus", "Highlight MouseOver" ) );
		m_pDrawFocus->SetVisible( true );
		m_pDrawFocus->SetConVar( &vgui_drawfocus );

		m_pFreeze = SETUP_PANEL( new CConVarCheckButton( this, "freeze option", "Freeze" ) );
		m_pFreeze->SetVisible( true );
		m_pFreeze->SetConVar( &vgui_drawtree_freeze );

		m_pShowPanelPtr = SETUP_PANEL( new CConVarCheckButton( this, "panel ptr option", "Show Addresses" ) );
		m_pShowPanelPtr->SetVisible( true );
		m_pShowPanelPtr->SetConVar( &vgui_drawtree_panelptr );

		m_pShowPanelAlpha = SETUP_PANEL( new CConVarCheckButton( this, "panel alpha option", "Show Alpha" ) );
		m_pShowPanelAlpha->SetVisible( true );
		m_pShowPanelAlpha->SetConVar( &vgui_drawtree_panelalpha );

		m_pRenderOrder = SETUP_PANEL( new CConVarCheckButton( this, "render order option", "In Render Order" ) );
		m_pRenderOrder->SetVisible( true );
		m_pRenderOrder->SetConVar( &vgui_drawtree_render_order );

		m_pShowBounds = SETUP_PANEL( new CConVarCheckButton( this, "show panel bounds", "Show Panel Bounds" ) );
		m_pShowBounds->SetVisible( true );
		m_pShowBounds->SetConVar( &vgui_drawtree_bounds );

		m_pHighlightSelected = SETUP_PANEL( new CConVarCheckButton( this, "highlight selected", "Highlight Selected" ) );
		m_pHighlightSelected->SetVisible( true );
		m_pHighlightSelected->SetConVar( &vgui_drawtree_draw_selected );

		m_pShowScheme = SETUP_PANEL( new CConVarCheckButton( this, "show scheme", "Show Scheme" ) );
		m_pShowScheme->SetVisible( true );
		m_pShowScheme->SetConVar( &vgui_drawtree_scheme );
		
		int r,g,b,a;
		GetBgColor().GetColor( r, g, b, a );
		a = 128;
		SetBgColor( Color( r, g, b, a ) );
	}


	virtual void PerformLayout()
	{
		BaseClass::PerformLayout();

		int x, y, w, t;
		GetClientArea( x, y, w, t );
		
		int yOffset = y;

		// Align the check boxes.
		m_pShowVisible->SetPos( x, yOffset );
		m_pShowVisible->SetWide( w/2 );
		yOffset += m_pShowVisible->GetTall();

		m_pShowHidden->SetPos( x, yOffset );
		m_pShowHidden->SetWide( w/2 );
		yOffset += m_pShowHidden->GetTall();

		m_pPopupsOnly->SetPos( x, yOffset );
		m_pPopupsOnly->SetWide( w/2 );
		yOffset += m_pPopupsOnly->GetTall();

		m_pDrawFocus->SetPos( x, yOffset );
		m_pDrawFocus->SetWide( w/2 );
		yOffset += m_pDrawFocus->GetTall();

		m_pShowBounds->SetPos( x, yOffset );
		m_pShowBounds->SetWide( w/2 );
		yOffset += m_pShowBounds->GetTall();

		m_pShowScheme->SetPos( x, yOffset );
		m_pShowScheme->SetWide( w/2 );
		yOffset += m_pShowScheme->GetTall();

		m_pTree->SetBounds( x, yOffset, w, t - (yOffset - y) );

		// Next column..
		yOffset = y;

		m_pFreeze->SetPos( x + w/2, yOffset );
		m_pFreeze->SetWide( w/2 );
		yOffset += m_pFreeze->GetTall();

		m_pShowPanelPtr->SetPos( x + w/2, yOffset );
		m_pShowPanelPtr->SetWide( w/2 );
		yOffset += m_pShowPanelPtr->GetTall();

		m_pShowPanelAlpha->SetPos( x + w/2, yOffset );
		m_pShowPanelAlpha->SetWide( w/2 );
		yOffset += m_pShowPanelAlpha->GetTall();
			   
		m_pRenderOrder->SetPos( x + w/2, yOffset );
		m_pRenderOrder->SetWide( w/2 );
		yOffset += m_pRenderOrder->GetTall();

		m_pHighlightSelected->SetPos( x + w/2, yOffset );
		m_pHighlightSelected->SetWide( w/2 );
		yOffset += m_pHighlightSelected->GetTall();
	}

	MESSAGE_FUNC( OnItemSelected, "TreeViewItemSelected" )
	{
		RecalculateSelectedHighlight();
	}

	void RecalculateSelectedHighlight( void )
	{
		Assert( m_pTree );

		if ( !vgui_drawtree_draw_selected.GetBool() || m_pTree->GetSelectedItemCount() != 1 )
		{
			// clear the selection
			g_DrawTreeSelectedPanel = 0;
		}
		else
		{
			CUtlVector< int > list;
			m_pTree->GetSelectedItems( list );

			Assert( list.Count() == 1 );

			KeyValues *data = m_pTree->GetItemData( list.Element(0) );

			if ( data )
			{
				g_DrawTreeSelectedPanel = (data) ? (VPANEL)data->GetInt( "PanelPtr", 0 ) : 0;
			}
			else
			{
				g_DrawTreeSelectedPanel = 0;
			}
		}
	}

	virtual void OnClose()
	{
		vgui_drawtree.SetValue( 0 );
		g_DrawTreeSelectedPanel = 0;

		// fixme - g_DrawTreeSelectedPanel has a potential crash if you hilight a panel, and then spam hud_reloadscheme
		// you will sometimes end up on a different panel or on garbage.
	}

public:
	CVGuiTree *m_pTree;	
	CConVarCheckButton *m_pShowVisible;
	CConVarCheckButton *m_pShowHidden;
	CConVarCheckButton *m_pPopupsOnly;
	CConVarCheckButton *m_pFreeze;
	CConVarCheckButton *m_pShowPanelPtr;
	CConVarCheckButton *m_pShowPanelAlpha;
	CConVarCheckButton *m_pRenderOrder;
	CConVarCheckButton *m_pDrawFocus;
	CConVarCheckButton *m_pShowBounds;
	CConVarCheckButton *m_pHighlightSelected;
	CConVarCheckButton *m_pShowScheme;
};


CDrawTreeFrame *g_pDrawTreeFrame = 0;


void VGui_RecursivePrintTree( 
	VPANEL current, 
	KeyValues *pCurrentParent,
	int popupDepthCounter )
{
	if ( !current )
		return;

	IPanel *ipanel = vgui::ipanel();

	if ( !vgui_drawtree_visible.GetInt() && ipanel->IsVisible( current ) )
		return;
	else if ( !vgui_drawtree_hidden.GetInt() && !ipanel->IsVisible( current ) )
		return;
	else if ( popupDepthCounter <= 0 && ipanel->IsPopup( current ) )
		return;	
	
	KeyValues *pNewParent = pCurrentParent;
	KeyValues *pVal = pCurrentParent->CreateNewKey();
		
	// Bind data to pVal.
	CUtlString name;

	const char *pInputName = ipanel->GetName( current );

	name = ( pInputName && *pInputName ) ? pInputName : "(no name)";

	if ( ipanel->IsMouseInputEnabled( current ) )
	{
		name += ", +m";
	}

	if ( ipanel->IsKeyBoardInputEnabled( current ) )
	{
		name += ", +k";
	}

	int dtb = vgui_drawtree_bounds.GetInt();
	if ( dtb > 0 )
	{
		name += ", ";

		int x, y, w, h;
		ipanel->GetSize( current, w, h );
		if ( dtb == 1 )
		{
			ipanel->GetPos( current, x, y );
			name += CFmtStr( "[%-4i %-4i %-4i %-4i]", x, y, w, h );
		}
		else
		{
			ipanel->GetAbsPos( current, x, y );
			name += CFmtStr( "abs [%d][%-4i %-4i %-4i %-4i]", ipanel->GetMessageContextId( current ), x, y, w, h );
		}
	}

	if ( vgui_drawtree_panelptr.GetBool() )
	{
		name += CFmtStr( " - [0x%x]", current );
	}
	
	if (vgui_drawtree_panelalpha.GetBool() )
	{
		KeyValues *kv = new KeyValues("alpha");
		ipanel->RequestInfo( current, kv );
		name += CFmtStr( " - [%d]", kv->GetInt("alpha") );
		kv->deleteThis();
	}

	if ( vgui_drawtree_scheme.GetBool() )
	{
		HScheme hScheme = ipanel->GetScheme( current );
		if ( hScheme )
		{
			IScheme *pScheme = scheme()->GetIScheme( hScheme );
			if ( pScheme )
			{
				name += CFmtStr( " [%s - %s]", pScheme->GetName(), pScheme->GetFileName() );
			}
		}
	}

	pVal->SetString( "Text", name.String() );
	pVal->SetInt( "PanelPtr", current );

	pNewParent = pVal;
	
	// Don't recurse past the tree itself because the tree control uses a panel for each
	// panel inside itself, and it'll infinitely create panels.
	if ( current == g_pDrawTreeFrame->m_pTree->GetVPanel() )
		return;


	int count = ipanel->GetChildCount( current );
	for ( int i = 0; i < count ; i++ )
	{
		VPANEL panel = ipanel->GetChild( current, i );
		VGui_RecursivePrintTree( panel, pNewParent, popupDepthCounter-1 );
	}
}


bool UpdateItemState(
	TreeView *pTree, 
	int iChildItemId, 
	KeyValues *pSub )
{
	bool bRet = false;
	IPanel *ipanel = vgui::ipanel();

	KeyValues *pItemData = pTree->GetItemData( iChildItemId );
	if ( pItemData->GetInt( "PanelPtr" ) != pSub->GetInt( "PanelPtr" ) ||
		Q_stricmp( pItemData->GetString( "Text" ), pSub->GetString( "Text" ) ) != 0 )
	{
		pTree->ModifyItem( iChildItemId, pSub );
		bRet = true;
	}

	// Ok, this is a new panel.
	VPANEL vPanel = pSub->GetInt( "PanelPtr" );

	int iBaseColor[3] = { 255, 255, 255 };
	if ( ipanel->IsPopup( vPanel ) )
	{
		iBaseColor[0] = 255;	iBaseColor[1] = 255;	iBaseColor[2] = 0;
	}

	if ( g_FocusPanelList.Find( vPanel ) != -1 )
	{
		iBaseColor[0] = 0;		iBaseColor[1] = 255;	iBaseColor[2] = 0;
		pTree->ExpandItem( iChildItemId, true );
	}

	if ( !ipanel->IsVisible( vPanel ) )
	{
		iBaseColor[0] >>= 1;	iBaseColor[1] >>= 1;	iBaseColor[2] >>= 1;
	}

	pTree->SetItemFgColor( iChildItemId, Color( iBaseColor[0], iBaseColor[1], iBaseColor[2], 255 ) );
	return bRet;
}


void IncrementalUpdateTree( 
	TreeView *pTree, 
	KeyValues *pValues )
{
	if ( !g_bForceRefresh && vgui_drawtree_freeze.GetInt() )
		return;

	g_bForceRefresh = false;

	bool bInvalidateLayout = IncrementalUpdateTree( pTree, pValues, UpdateItemState, -1 );
	
	pTree->ExpandItem( pTree->GetRootItemIndex(), true );

	if ( g_pDrawTreeFrame )
		g_pDrawTreeFrame->RecalculateSelectedHighlight();
	
	if ( bInvalidateLayout )
		pTree->InvalidateLayout();
}


bool WillPanelBeVisible( VPANEL hPanel )
{
	while ( hPanel )
	{
		if ( !vgui::ipanel()->IsVisible( hPanel ) )
			return false;

		hPanel = vgui::ipanel()->GetParent( hPanel );
	}
	return true;
}


void VGui_AddPopupsToKeyValues( KeyValues *pCurrentParent )
{
	// 'twould be nice if we could get the Panel* from the VPANEL, but we can't.
	int count = surface()->GetPopupCount();
	for ( int i=0; i < count; i++ )
	{
		VPANEL vPopup = surface()->GetPopup( i );
		if ( vgui_drawtree_hidden.GetInt() || WillPanelBeVisible( vPopup ) )
		{
			VGui_RecursivePrintTree( 
				vPopup, 
				pCurrentParent,
				1 );
		}
	}
}


void VGui_FillKeyValues( KeyValues *pCurrentParent )
{
	if ( !EngineVGui()->IsInitialized() )
		return;

	// Figure out the root panel to start at. 
	// If they specified a name for a root panel, then use that one.
	VPANEL hBase = surface()->GetEmbeddedPanel();

	if ( vgui_drawtree_popupsonly.GetInt() )
	{
		VGui_AddPopupsToKeyValues( pCurrentParent );
	}
	else if ( vgui_drawtree_render_order.GetInt() )
	{
		VGui_RecursivePrintTree( 
			hBase, 
			pCurrentParent,
			0 );

		VGui_AddPopupsToKeyValues( pCurrentParent );
	}
	else
	{
		VGui_RecursivePrintTree( 
			hBase, 
			pCurrentParent,
			99999 );
	}
}


void VGui_DrawHierarchy( void )
{
	VPROF( "VGui_DrawHierarchy" );

	if ( IsGameConsole() )
		return;

	if ( vgui_drawtree.GetInt() <= 0 )
	{
		g_pDrawTreeFrame->SetVisible( false );
		return;
	}

	g_pDrawTreeFrame->SetVisible( true );

	// Now reconstruct the tree control.
	KeyValues *pRoot = new KeyValues("");
	pRoot->SetString( "Text", "<shouldn't see this>" );
	
	VGui_FillKeyValues( pRoot );

	// Now incrementally update the tree control so we can preserve which nodes are open.
	IncrementalUpdateTree( g_pDrawTreeFrame->m_pTree, pRoot );

	// Delete the keyvalues.
	pRoot->deleteThis();
}


void VGui_CreateDrawTreePanel( Panel *parent )
{
	int widths = 300;
	
	g_pDrawTreeFrame = SETUP_PANEL( new CDrawTreeFrame( parent, "DrawTreeFrame" ) );
	g_pDrawTreeFrame->SetVisible( false );
	g_pDrawTreeFrame->SetBounds( parent->GetWide() - widths, 0, widths, parent->GetTall() - 10 );
	
	g_pDrawTreeFrame->MakePopup( false, false );
	g_pDrawTreeFrame->SetKeyBoardInputEnabled( true );
	g_pDrawTreeFrame->SetMouseInputEnabled( true );
}


void VGui_MoveDrawTreePanelToFront()
{
	if ( g_pDrawTreeFrame )
		g_pDrawTreeFrame->MoveToFront();
}


void VGui_UpdateDrawTreePanel()
{
	VGui_DrawHierarchy();
}


void vgui_drawtree_clear_f()
{
	if ( g_pDrawTreeFrame && g_pDrawTreeFrame->m_pTree )
		g_pDrawTreeFrame->m_pTree->RemoveAll();
}

ConCommand vgui_drawtree_clear( "vgui_drawtree_clear", vgui_drawtree_clear_f );
