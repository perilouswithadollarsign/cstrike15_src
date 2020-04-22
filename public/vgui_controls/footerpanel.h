#ifndef FOOTERPANEL_H
#define FOOTERPANEL_H

#ifdef _WIN32
#pragma once
#endif


#include "vgui_controls/EditablePanel.h"

namespace vgui
{
//-----------------------------------------------------------------------------
// Purpose: Panel that acts as background for button icons and help text in the UI
//-----------------------------------------------------------------------------
class CFooterPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CFooterPanel, vgui::EditablePanel );

public:
	CFooterPanel( Panel *parent, const char *panelName );
	virtual ~CFooterPanel();

	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	ApplySettings( KeyValues *pResourceData );
	virtual void	Paint( void );
	virtual void	PaintBackground( void );

	// caller tags the current hint, used to assist in ownership
	void			SetHelpNameAndReset( const char *pName );
	const char		*GetHelpName();

	void			AddButtonsFromMap( vgui::Frame *pMenu );
	void			SetStandardDialogButtons();
	void			AddNewButtonLabel( const char *text, const char *icon );
	void			ShowButtonLabel( const char *name, bool show = true );
	void			SetButtonText( const char *buttonName, const char *text );
	void			ClearButtons();
	void			SetButtonGap( int nButtonGap ){ m_nButtonGap = nButtonGap; }
	void			UseDefaultButtonGap(){ m_nButtonGap = m_nButtonGapDefault; }
	void			SetButtonPinRight( int nButtonPinRight ) { m_ButtonPinRight = nButtonPinRight; }

	//=============================================================================
	// HPE_BEGIN:
	// [smessick]
	//=============================================================================

	// Returns the number of button labels.
	int				GetButtonLabelCount( void ) const { return m_ButtonLabels.Count(); }

	// Sets the pin right location with adjustments based on the current
	// screen width and height. The given pixel offset is assumed to be based on 
	// a 640x480 screen.
	void			SetButtonPinRightProportional( int nButtonPinRight );

	// Center the footer horizontally.
	void			SetCenterHorizontal( bool bCenterHorizontal ) { m_bCenterHorizontal = bCenterHorizontal; }

	// Set the y offset from the top of the screen.
	// The given pixel offset is assumed to be based on a 640x480 screen.
	void			SetButtonOffsetFromTopProportional( int yOffset );

	//=============================================================================
	// HPE_END
	//=============================================================================

private:
	struct ButtonLabel_t
	{
		bool	bVisible;
		char	name[MAX_PATH];
		wchar_t	text[MAX_PATH];
		wchar_t	icon[2];			// icon is a single character
	};

	CUtlVector< ButtonLabel_t* > m_ButtonLabels;

	vgui::Label		*m_pSizingLabel;		// used to measure font sizes

	bool			m_bPaintBackground;		// fill the background?
	bool			m_bCenterHorizontal;	// center buttons horizontally?
	int				m_ButtonPinRight;		// if not centered, this is the distance from the right margin that we use to start drawing buttons (right to left)
	int				m_nButtonGap;			// space between buttons when drawing
	int				m_nButtonGapDefault;		// space between buttons (initial value)
	int				m_FooterTall;			// height of the footer
	int				m_ButtonOffsetFromTop;	// how far below the top the buttons should be drawn
	int				m_ButtonSeparator;		// space between the button icon and text
	int				m_TextAdjust;			// extra adjustment for the text (vertically)...text is centered on the button icon and then this value is applied

	char			m_szTextFont[64];		// font for the button text
	char			m_szButtonFont[64];		// font for the button icon
	char			m_szFGColor[64];		// foreground color (text)
	char			m_szBGColor[64];		// background color (fill color)
	
	vgui::HFont		m_hButtonFont;
	vgui::HFont		m_hTextFont;
	char			*m_pHelpName;
};
}
#endif //FOOTERPANEL_H