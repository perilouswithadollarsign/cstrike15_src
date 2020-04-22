#pragma once
#include "afxwin.h"
#include "color.h"

namespace vgui
{
	class EditablePanel;
	typedef unsigned long HCursor;
}

class CVGuiWnd 
{

public:
	CVGuiWnd(void);
	~CVGuiWnd(void);

public:

	void				SetMainPanel( vgui::EditablePanel * pPanel );
	vgui::EditablePanel	*GetMainPanel();	// returns VGUI main panel
	vgui::EditablePanel *CreateDefaultPanel();

	void				SetParentWindow(CWnd *pParent);
	CWnd				*GetParentWnd();	// return CWnd handle

	void				SetCursor(vgui::HCursor cursor);
	void				SetCursor(const char *filename);

	void				SetRepaintInterval( int msecs );
	int					GetVGuiContext();

	// The Hammer 2D views basically ignore vgui input. They're only there to render on top of.
	// When we pass true here, CMatSystemSurface::RunFrame ignores all the input events.
	// If we pass false (as the model browser does), then it does process input events and send them to the vgui panels.
	// Eventually, we could change the 2D views' input events to come from the input system instead of from MFC.
	virtual bool		IsModal()
	{
		return false;
	}

protected:

	void DrawVGuiPanel();  // overridden to draw this view
	long WindowProcVGui( UINT message, WPARAM wParam, LPARAM lParam ); //
	
	vgui::EditablePanel	*m_pMainPanel;
	CWnd		*m_pParentWnd;
	int			m_hVGuiContext;
	bool		m_bIsDrawing;
	Color		m_ClearColor;
	bool		m_bClearZBuffer;
};

class CVGuiPanelWnd: public CWnd, public CVGuiWnd
{
protected:
	DECLARE_DYNCREATE(CVGuiPanelWnd)

public:

	// Generated message map functions
	//{{AFX_MSG(CVGuiViewModel)
	BOOL OnEraseBkgnd(CDC* pDC);
	//}}AFX_MSG

	virtual LRESULT WindowProc( UINT message, WPARAM wParam, LPARAM lParam );

	// See CVGuiWnd's function for a description but this basically tells Hammer to actually pass vgui messages to our panel.
	virtual bool IsModal()
	{
		return true;
	}

	DECLARE_MESSAGE_MAP()
};

