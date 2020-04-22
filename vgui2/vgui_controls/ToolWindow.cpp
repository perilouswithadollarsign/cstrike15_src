//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <vgui/KeyCode.h>
#include <keyvalues.h>
#include "vgui/IInput.h"
#include "vgui/MouseCode.h"
#include "vgui/ISurface.h"

#include <vgui_controls/ToolWindow.h>
#include <vgui_controls/PropertySheet.h>

#include "tier1/tokenset.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

CUtlVector< ToolWindow * > ToolWindow::s_ToolWindows;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : int
//-----------------------------------------------------------------------------
int ToolWindow::GetToolWindowCount()
{
	return s_ToolWindows.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : PropertySheet
//-----------------------------------------------------------------------------
ToolWindow *ToolWindow::GetToolWindow( int index )
{
	return s_ToolWindows[ index ];
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
ToolWindow::ToolWindow(
	Panel *parent, 
	bool contextlabel,
	IToolWindowFactory *factory /*= 0*/, 
	Panel *page /*= NULL*/, 
	char const *title /*= NULL */,
	bool contextMenu /*=false*/,
	bool inGlobalList /*= true*/ ) : 
	BaseClass( parent, "ToolWindow" ), 
	m_bStickyEdges( true ),
	m_pFactory( NULL )
{
	if ( inGlobalList )
	{
		s_ToolWindows.AddToTail( this );
	}

	// create the property sheet
	m_pPropertySheet = new PropertySheet(this, "ToolWindowSheet", true );
	m_pPropertySheet->ShowContextButtons( contextlabel );
	m_pPropertySheet->AddPage( page, title, 0, contextMenu );
	m_pPropertySheet->AddActionSignalTarget(this);
	m_pPropertySheet->SetSmallTabs( true );
	m_pPropertySheet->SetKBNavigationEnabled( false );

	SetSmallCaption( true );

	SetMenuButtonResponsive(false);
	SetMinimizeButtonVisible(false);
	SetCloseButtonVisible(true);
	SetMoveable( true );
	SetSizeable(true);

	SetClipToParent( false );
	SetVisible( true );

	SetDeleteSelfOnClose( true );

	SetTitle( "", false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
ToolWindow::~ToolWindow()
{
	// These don't actually kill the children of the property sheet
	m_pPropertySheet->RemoveAllPages();

	s_ToolWindows.FindAndRemove( this );
}

//-----------------------------------------------------------------------------
// Purpose: Pass through to sheet
// Input  :  - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ToolWindow::IsDraggableTabContainer() const
{
	return m_pPropertySheet->IsDraggableTab();
}

//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to the PropertySheet this dialog encapsulates
// Output : PropertySheet *
//-----------------------------------------------------------------------------
PropertySheet *ToolWindow::GetPropertySheet()
{
	return m_pPropertySheet;
}

//-----------------------------------------------------------------------------
// Purpose: Gets a pointer to the currently active page.
// Output : Panel
//-----------------------------------------------------------------------------
Panel *ToolWindow::GetActivePage()
{
	return m_pPropertySheet->GetActivePage();
}

void ToolWindow::SetActivePage( Panel *page )
{
	m_pPropertySheet->SetActivePage( page );
}

//-----------------------------------------------------------------------------
// Purpose: Wrapped function
//-----------------------------------------------------------------------------
void ToolWindow::AddPage(Panel *page, const char *title, bool contextMenu)
{
	m_pPropertySheet->AddPage(page, title, 0, contextMenu );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *page - 
//-----------------------------------------------------------------------------
void ToolWindow::RemovePage( Panel *page )
{
	m_pPropertySheet->RemovePage( page );
	if ( m_pPropertySheet->GetNumPages() == 0 )
	{
		MarkForDeletion();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets up the sheet
//-----------------------------------------------------------------------------
void ToolWindow::PerformLayout()
{
	BaseClass::PerformLayout();

	int x, y, wide, tall;
	GetClientArea(x, y, wide, tall);
	m_pPropertySheet->SetBounds(x, y, wide, tall);
	m_pPropertySheet->InvalidateLayout(); // tell the propertysheet to redraw!
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Overrides build mode so it edits the sub panel
//-----------------------------------------------------------------------------
void ToolWindow::ActivateBuildMode()
{
	// no subpanel, no build mode
	EditablePanel *panel = dynamic_cast<EditablePanel *>(GetActivePage());
	if (!panel)
		return;

	panel->ActivateBuildMode();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ToolWindow::RequestFocus(int direction)
{
    m_pPropertySheet->RequestFocus(direction);
}

void ToolWindow::OnSetFocus()
{
	m_pPropertySheet->RequestFocus();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *factory - 
//-----------------------------------------------------------------------------
void ToolWindow::SetToolWindowFactory( IToolWindowFactory *factory )
{
	m_pFactory = factory;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : IToolWindowFactory
//-----------------------------------------------------------------------------
IToolWindowFactory *ToolWindow::GetToolWindowFactory()
{
	return m_pFactory;
}

//-----------------------------------------------------------------------------
// Purpose: To fill the space left by other tool windows
// Input  :  edge: 0=all, 1=top, 2=right, 3=bottom, 4=left
// Output : 
//-----------------------------------------------------------------------------

void ToolWindow::Grow( int edge, int from_x, int from_y )
{
	int status_h = 24;
	int menubar_h = 27;

	int sw, sh;
	surface()->GetScreenSize( sw, sh );

	int old_x, old_y, old_w, old_h;
	GetBounds( old_x, old_y, old_w, old_h );

	int new_x, new_y, new_w, new_h;
	new_x = old_x;
	new_y = old_y;
	new_w = old_w;
	new_h = old_h;

	int c = GetToolWindowCount();

	// grow up
	if ( ( edge == 0 ) || ( edge == 1 ) )
	{
		// first shrink the edge back to the grow point
		if ( from_y >= 0 )
		{
			old_h = old_h - ( from_y - old_y );
			old_y = from_y;
		}

		// now grow the edge as far as it can go
		new_h = old_h + ( old_y - menubar_h );
		new_y = menubar_h;

		for ( int i = 0 ; i < c; ++i )
		{
			ToolWindow *tw = GetToolWindow( i );
			Assert( tw );
			if ( ( !tw ) || ( tw == this ) )
				continue;

			// Get panel bounds
			int x, y, w, h;
			tw->GetBounds( x, y, w, h );

			// grow it
			if ( ( ( ( old_x > x ) && ( old_x < x + w ) )
				|| ( ( old_x + old_w > x ) && ( old_x + old_w < x + w ) )
				|| ( ( old_x <= x ) && old_x + old_w >= x + w ))
				&& ( ( old_y >= y + h ) && ( new_y < y + h ) ) )
			{
				new_h = old_h + ( old_y - ( y + h ) );
				new_y = y + h;
			}
		}
		old_h = new_h;
		old_y = new_y;
	}

	// grow right
	if ( ( edge == 0 ) || ( edge == 2 ) )
	{
		// first shrink the edge back to the grow point
		if ( from_x >= 0 )
		{
			old_w = from_x - old_x;
		}

		// now grow the edge as far as it can go
		new_w = sw - old_x;

		for ( int i = 0 ; i < c; ++i )
		{
			ToolWindow *tw = GetToolWindow( i );
			Assert( tw );
			if ( ( !tw ) || ( tw == this ) )
				continue;

			// Get panel bounds
			int x, y, w, h;
			tw->GetBounds( x, y, w, h );

			// grow it
			if ( ( ( ( old_y > y ) && ( old_y < y + h ) )
				|| ( ( old_y + old_h > y ) && ( old_y + old_h < y + h ) )
				|| ( ( old_y <= y ) && old_y + old_h >= y + h ))
				&& ( ( old_x + old_w <= x ) && ( new_w > x - old_x ) ) )
			{
				new_w = x - old_x;
			}
		}
		old_w = new_w;
	}

	// grow down
	if ( ( edge == 0 ) || ( edge == 3 ) )
	{
		// first shrink the edge back to the grow point
		if ( from_y >= 0 )
		{
			old_h = from_y - old_y;
		}

		// now grow the edge as far as it can go
		new_h = sh - old_y - status_h;

		for ( int i = 0 ; i < c; ++i )
		{
			ToolWindow *tw = GetToolWindow( i );
			Assert( tw );
			if ( ( !tw ) || ( tw == this ) )
				continue;

			// Get panel bounds
			int x, y, w, h;
			tw->GetBounds( x, y, w, h );

			// grow it
			if ( ( ( ( old_x > x ) && ( old_x < x + w ) )
				|| ( ( old_x + old_w > x ) && ( old_x + old_w < x + w ) )
				|| ( ( old_x <= x ) && old_x + old_w >= x + w ))
				&& ( ( old_y + old_h <= y ) && ( new_h > y - old_y ) ) )
			{
				new_h = y - old_y;
			}
		}
		old_h = new_h;
	}

	// grow left
	if ( ( edge == 0 ) || ( edge == 4 ) )
	{
		// first shrink the edge back to the grow point
		if ( from_x >= 0 )
		{
			old_w = old_w - ( from_x - old_x );
			old_x = from_x;
		}

		// now grow the edge as far as it can go
		new_w = old_w + old_x;
		new_x = 0;

		for ( int i = 0 ; i < c; ++i )
		{
			ToolWindow *tw = GetToolWindow( i );
			Assert( tw );
			if ( ( !tw ) || ( tw == this ) )
				continue;

			// Get panel bounds
			int x, y, w, h;
			tw->GetBounds( x, y, w, h );

			// grow it
			if ( ( ( ( old_y > y ) && ( old_y < y + h ) )
				|| ( ( old_y + old_h > y ) && ( old_y + old_h < y + h ) )
				|| ( ( old_y <= y ) && old_y + old_h >= y + h ))
				&& ( ( old_x >= x + w ) && ( new_x < x + w ) ) )
			{
				new_w = old_w + ( old_x - ( x + w ) );
				new_x = x + w;
			}
		}
		old_w = new_w;
		old_x = new_x;
	}

	// Set panel bounds
	SetBounds( new_x, new_y, new_w, new_h );

}

//-----------------------------------------------------------------------------
// Purpose: Calls Grow based on where the mouse is.
//          over titlebar: grows all edges ( from mouse pos )
//          over edge grab area: grows just that edge
//          over corner grab area: grows the two adjacent edges
// Input  : 
// Output : 
//-----------------------------------------------------------------------------

void ToolWindow::GrowFromClick()
{
	int mx, my;
	input()->GetCursorPos( mx, my );

	int esz, csz, brsz, ch;
	esz = GetDraggerSize();
	csz = GetCornerSize();
	brsz = GetBottomRightSize();
	ch = GetCaptionHeight();

	int x, y, w, h;
	GetBounds( x, y, w, h );

	// upper right
	if ( ( mx > x+w-csz-1 ) && ( my < y+csz ) )
	{
		Grow(1);
		Grow(2);
	}
	// lower right (the big one)
	else if ( ( mx > x+w-brsz-1 ) && ( my > y+h-brsz-1 ) )
	{
		Grow(2);
		Grow(3);
	}
	// lower left
	else if ( ( mx < x+csz ) && ( my > y+h-csz-1 ) )
	{
		Grow(3);
		Grow(4);
	}
	// upper left
	else if ( ( mx < x+csz ) && ( my < y+csz ) )
	{
		Grow(4);
		Grow(1);
	}
	// top edge
	else if ( my < y+esz )
	{
		Grow(1);
	}
	// right edge
	else if ( mx > x+w-esz-1 )
	{
		Grow(2);
	}
	// bottom edge
	else if ( my > y+h-esz-1 )
	{
		Grow(3);
	}
	// left edge
	else if ( mx < x+esz )
	{
		Grow(4);
	}
	// otherwise (if over the grab bar), grow all edges (from the clicked point)
	else if ( my < y + ch )
	{
		Grow(0, mx, my);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : 
//-----------------------------------------------------------------------------

void ToolWindow::OnMouseDoublePressed( MouseCode code )
{
	GrowFromClick();
}

void ToolWindow::OnMousePressed( MouseCode code )
{
	switch ( code )
	{
	case MOUSE_MIDDLE:
		GrowFromClick();
		break;
	default:
		BaseClass::OnMousePressed( code );
	}
}

void ToolWindow::OnPageChanged( )
{
	//char szTitle[ 256 ];
	//m_pPropertySheet->GetActiveTabTitle( szTitle, sizeof( szTitle ) );
	//SetTitle( szTitle, false );
}

static void GetParentSpaceExtents( Panel *p, int bounds[ 4 ] )
{
	// Get parent space bounds
	p->GetBounds( bounds[ 0 ], bounds[ 1 ], bounds[ 2 ], bounds[ 3 ] );

	// x1, y1
	bounds[ 2 ] += bounds[ 0 ];
	bounds[ 3 ] += bounds[ 1 ];
}

static ESharedEdge GetOppositeEdge( ESharedEdge eEdge )
{
	switch ( eEdge )
	{
	default:
		break;
	case TOOLWINDOW_LEFT:
		return TOOLWINDOW_RIGHT;
	case TOOLWINDOW_TOP:
		return TOOLWINDOW_BOTTOM;
	case TOOLWINDOW_RIGHT:
		return TOOLWINDOW_LEFT;
	case TOOLWINDOW_BOTTOM:
		return TOOLWINDOW_TOP;
	}

	Assert( 0 );
	return TOOLWINDOW_NONE;
}

static void GetParentSpaceEdge( Panel *p, int bounds[ 4 ], ESharedEdge eEdge )
{
	GetParentSpaceExtents( p, bounds );
	// Collapse down to single edge
	bounds[ GetOppositeEdge( eEdge ) ] = bounds[ eEdge ];
}

static const tokenset_t< ESharedEdge > s_EdgeTypes[] =
{						 
	{ "TOOLWINDOW_NONE",		TOOLWINDOW_NONE     },                         
	{ "TOOLWINDOW_LEFT",		TOOLWINDOW_LEFT     },                         
	{ "TOOLWINDOW_TOP",			TOOLWINDOW_TOP		}, 
	{ "TOOLWINDOW_RIGHT",		TOOLWINDOW_RIGHT    }, 
	{ "TOOLWINDOW_BOTTOM",		TOOLWINDOW_BOTTOM	},
	{ NULL,						TOOLWINDOW_NONE		}
};

void ToolWindow::MoveSibling( ToolWindow *pSibling, ESharedEdge eEdge, int dpixels )
{
	int bounds[ 4 ];
	GetParentSpaceExtents( pSibling, bounds );
	bounds[ eEdge ] += dpixels;

	// Convert back to local pos and width/height
	bounds[ 2 ] = bounds[ 2 ] - bounds[ 0 ];
	bounds[ 3 ] = bounds[ 3 ] - bounds[ 1 ];

	pSibling->SetBounds( bounds[ 0 ], bounds[ 1 ], bounds[ 2 ], bounds[ 3 ] );
}

struct TWEdgePair_t
{
	ToolWindow *m_pWindow;
	ESharedEdge m_EdgeType;

	static bool Less( const TWEdgePair_t &lhs, const TWEdgePair_t &rhs )
	{
		if ( lhs.m_pWindow < rhs.m_pWindow )
			return true;
		if ( lhs.m_pWindow > rhs.m_pWindow )
			return false;

		return lhs.m_EdgeType < rhs.m_EdgeType;
	}
};

static bool SpanOverlaps( int line1[ 4 ], int line2[ 4 ] )
{
	bool bXOverlap = ( MAX( line1[ 0 ], line2[ 0 ] ) - MIN( line1[ 2 ], line2[ 2 ] ) ) <= 0 ? true : false; 
	bool bYOverlap = ( MAX( line1[ 1 ], line2[ 1 ] ) - MIN( line1[ 3 ], line2[ 3 ] ) ) <= 0 ? true : false; 
	
	return bXOverlap && bYOverlap;
}

// Find any parallel edges that overlap and aren't already in the tree.  If we add an edge, we recursively see if anything else overlaps it	on the same line
void ToolWindow::FindOverlappingEdges_R( CUtlVector< ToolWindow * > &vecSiblings, CUtlRBTree< TWEdgePair_t > &rbCurrentEdges, ESharedEdge eEdgeType, int line[ 4 ] )
{
	// L/R or T/B
	for ( int et = 0; et < 2; ++et )
	{
		for ( int i = 0; i < vecSiblings.Count(); ++i )
		{
			ToolWindow *pSibling = vecSiblings[ i ];

			int edgeline[ 4 ];
			GetParentSpaceEdge( pSibling, edgeline, eEdgeType );
			if ( SpanOverlaps( edgeline, line ) )
			{
				TWEdgePair_t ep;
				ep.m_pWindow = pSibling;
				ep.m_EdgeType = eEdgeType;

				if ( rbCurrentEdges.Find( ep ) == rbCurrentEdges.InvalidIndex() )
				{
					/*
					Msg( "Found overlap %d, %d, %d, %d with %d %d %d %d on edge %s\n",
						edgeline[ 0 ],edgeline[ 1 ],edgeline[ 2 ],edgeline[ 3 ],
						line[ 0 ],line[ 1 ],line[ 2 ],line[ 3 ],
						s_EdgeTypes->GetNameByToken( eEdgeType ) );
					*/
				
					rbCurrentEdges.Insert( ep );
					// Extend search out along the line!!!
					FindOverlappingEdges_R( vecSiblings, rbCurrentEdges, eEdgeType, edgeline );
				}
			}
		}

		// Try opposite on next pass
		eEdgeType = GetOppositeEdge( eEdgeType );
	}
}

// Override Frame method in order to grow adjoining sibling tool windows if possible
void ToolWindow::OnGripPanelMoved( int nNewX, int nNewY, int nNewW, int nNewH )
{
	bool bShiftDown = input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT );
	if ( IsStickEdgesEnabled() && !bShiftDown )
	{
		// Snag old values
		int x, y, w, h;
		GetBounds( x, y, w, h );

		// Get list of siblings
		CUtlVector< ToolWindow * > vecSiblings;
		GetSiblingToolWindows( vecSiblings );

		// Msg( "%d siblings\n", vecSiblings.Count() );
		if ( vecSiblings.Count() > 0 )
		{
			// Determine what type of grip panel movement(s) occurred
			int d[ 4 ];
			d[ 0 ] = nNewX - x;		// L
			d[ 1 ] = nNewY - y;		// T
			d[ 2 ] = ( nNewX + nNewW ) - ( x + w );		// R
			d[ 3 ] = ( nNewY + nNewH ) - ( y + h );		// B

			// For each moved edge, find and edges along the same line that overlap. If we find an overlap, check it for additional overlaps against the siblings:
			//
			//    ------------------------------------		e.g:  clicking at the base of panel B should find the "top" of panel C, which then recurses and gets the bottom line of panel A as well 
			//    |                |				  |
			//    |       A        |		B	      |
			//    |                |				  |
			//    |                |				  |
			//    |-----------------xxxxxxxxxxxxxxxxxx|
			//    |         C      				      |
			//    |                				      |
			//    -------------------------------------
			//

			for ( int edge = 0; edge < 4; ++edge )
			{
				if ( !d[ edge ] )
					continue;

				CUtlRBTree< TWEdgePair_t > rbEdges( 0, 0, TWEdgePair_t::Less );

				ESharedEdge eEdge = (ESharedEdge)( edge );
				
				// Msg( "movement on edge %s is %d pixels\n", s_EdgeTypes->GetNameByToken( eEdge ), d[ edge ] );

				// Now find any overlapping ones along the same "line"
				int edgeline[ 4 ];
				GetParentSpaceEdge( this, edgeline, eEdge );
				FindOverlappingEdges_R( vecSiblings, rbEdges, eEdge, edgeline );
				 
				FOR_EACH_UTLRBTREE( rbEdges, i )
				{
					TWEdgePair_t &ep = rbEdges[ i ];
					// Msg( "moving tw %p on edge %s\n", ep.m_pWindow, s_EdgeTypes->GetNameByToken( ep.m_EdgeType ) );
					// Move the sibling
					MoveSibling( ep.m_pWindow, ep.m_EdgeType, d[ edge ] );
				}
			}
		}
	}

	// Move panel in question
	BaseClass::OnGripPanelMoved( nNewX, nNewY, nNewW, nNewH );
}

void ToolWindow::GetSiblingToolWindows( CUtlVector< ToolWindow * > &vecSiblings )
{
	Panel *parent = GetParent();
	if ( NULL == parent )
		return;

	int nChildCount = parent->GetChildCount();
	for ( int i = 0 ; i < nChildCount; ++i )
	{
		ToolWindow *pChildTool = dynamic_cast< ToolWindow * >( parent->GetChild( i ) );
		if ( !pChildTool || 
			 !pChildTool->IsVisible() || 
			 pChildTool == this )
			continue;
		vecSiblings.AddToTail( pChildTool );
	}
}

void ToolWindow::OnGripPanelMoveFinished()
{
}

// Static method
void ToolWindow::EnableStickyEdges( bool bEnable )
{
	m_bStickyEdges = bEnable;
}

bool ToolWindow::IsStickEdgesEnabled() const
{
	return m_bStickyEdges;
}
