//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef TOOLWINDOW_H
#define TOOLWINDOW_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/vgui.h>
#include <vgui_controls/Frame.h>

struct TWEdgePair_t;

namespace vgui
{

class ToolWindow;
class PropertySheet;

// So that an app can have a "custom" tool window class created during window drag/drop operations on the property sheet
class IToolWindowFactory
{
public:
	virtual ToolWindow *InstanceToolWindow( Panel *parent, bool contextLabel, Panel *firstPage, char const *title, bool contextMenu ) = 0;
};

enum ESharedEdge
{
	TOOLWINDOW_NONE = -1,
	TOOLWINDOW_LEFT = 0,  // The other window's left side is 'shared' with our right side
	TOOLWINDOW_TOP,
	TOOLWINDOW_RIGHT,
	TOOLWINDOW_BOTTOM,
};
  
//-----------------------------------------------------------------------------
// Purpose: Simple frame that holds a property sheet
//-----------------------------------------------------------------------------
class ToolWindow : public Frame
{
	DECLARE_CLASS_SIMPLE( ToolWindow, Frame );

public:
	ToolWindow(Panel *parent, bool contextLabel, IToolWindowFactory *factory = 0, Panel *page = NULL, char const *title = NULL, bool contextMenu = false, bool inGlobalList = true );

	~ToolWindow();

	virtual bool IsDraggableTabContainer() const;

	// returns a pointer to the PropertySheet this dialog encapsulates 
	PropertySheet *GetPropertySheet();

	// wrapper for PropertySheet interface
	void AddPage(Panel *page, const char *title, bool contextMenu );
	void RemovePage( Panel *page );
	Panel *GetActivePage();
	void SetActivePage( Panel *page );

	void SetToolWindowFactory( IToolWindowFactory *factory );
	IToolWindowFactory *GetToolWindowFactory();

	static int GetToolWindowCount();
	static ToolWindow *GetToolWindow( int index );

	static CUtlVector< ToolWindow * > s_ToolWindows;

	virtual void Grow( int edge = 0, int from_x = -1, int from_y = -1 );
	virtual void GrowFromClick();

	void GetSiblingToolWindows( CUtlVector< ToolWindow * > &vecSiblings );

	void EnableStickyEdges( bool bEnable );
	bool IsStickEdgesEnabled() const;

protected:
	// vgui overrides
	virtual void PerformLayout();
	virtual void ActivateBuildMode();
	virtual void RequestFocus(int direction = 0);
	virtual void OnSetFocus();

	virtual void OnMousePressed(MouseCode code);
	virtual void OnMouseDoublePressed(MouseCode code);

	MESSAGE_FUNC( OnPageChanged, "PageChanged" );

	// Override Frame method in order to grow sibling tool windows if possible
	virtual void OnGripPanelMoved( int nNewX, int nNewY, int nNewW, int nNewH );
	virtual void OnGripPanelMoveFinished();

private:

	void FindOverlappingEdges_R( CUtlVector< ToolWindow * > &vecSiblings, CUtlRBTree< TWEdgePair_t > &rbCurrentEdges, ESharedEdge eEdgeType, int line[ 4 ] );
	void MoveSibling( ToolWindow *pSibling, ESharedEdge eEdge, int dpixels );

	PropertySheet		*m_pPropertySheet;
	IToolWindowFactory	*m_pFactory;
	bool				m_bStickyEdges : 1;
};

}; // vgui


#endif // TOOLWINDOW_H
