//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( VCONTROLSLISTPANEL_H )
#define VCONTROLSLISTPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/SectionedListPanel.h>

//-----------------------------------------------------------------------------
// Purpose: Special list subclass to handle drawing of trap mode prompt on top of
//			lists client area
//-----------------------------------------------------------------------------
class VControlsListPanel : public vgui::SectionedListPanel
{
public:
	// Construction
					VControlsListPanel( vgui::Panel *parent, const char *listName );
	virtual			~VControlsListPanel();

	// Start/end capturing
	virtual void	StartCaptureMode(vgui::HCursor hCursor = NULL);
	virtual void	EndCaptureMode(vgui::HCursor hCursor = NULL);
	virtual bool	IsCapturing();

	// Set which item should be associated with the prompt
	virtual void	SetItemOfInterest(int itemID);
	virtual int		GetItemOfInterest();

	virtual void	OnMousePressed(vgui::MouseCode code);
	virtual void	OnMouseDoublePressed(vgui::MouseCode code);

	virtual void	OnKeyCodePressed( vgui::KeyCode code );

	virtual void	PerformLayout();

	virtual void	PostChildPaint();

	void			ResetToTop();

	void			SetInternalItemFont( vgui::HFont hFont );

	bool			IsLeftArrowHighlighted() { return m_bLeftArrowHighlighted; }
	bool			IsRightArrowHighlighted() { return m_bRightArrowHighlighted; }

private:
	void ApplySchemeSettings(vgui::IScheme *pScheme );

	// Are we showing the prompt?
	bool			m_bCaptureMode;
	// If so, where?
	int				m_nClickRow;
	// Font to use for showing the prompt
	vgui::HFont		m_hFont;
	// panel used to edit
	class CInlineEditPanel *m_pInlineEditPanel;
	int m_iMouseX, m_iMouseY;

	typedef vgui::SectionedListPanel BaseClass;

	vgui::Label		*m_pLblDownArrow;
	vgui::Label		*m_pLblUpArrow;
	vgui::ScrollBar	*m_pSectionedScrollBar;

	int				m_nScrollArrowInset;
	int				m_nButtonOffset;

	vgui::HFont		m_hButtonFont;
	vgui::HFont		m_hItemFont;
	vgui::HFont		m_hSymbolFont;

	Color			m_SelectedTextColor;
	Color			m_ListButtonActiveColor;
	Color			m_ListButtonInactiveColor;

	bool			m_bLeftArrowHighlighted;
	bool			m_bRightArrowHighlighted;
};

#endif // VCONTROLSLISTPANEL_H