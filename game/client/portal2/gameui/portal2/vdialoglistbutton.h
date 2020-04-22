//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VDIALOGLISTBUTTON_H__
#define __VDIALOGLISTBUTTON_H__

#include "basemodui.h"
#include "portal2_leaderboard_manager.h"


class CDialogListButton : public vgui::Button
{
public:
	DECLARE_CLASS_SIMPLE( CDialogListButton, vgui::Button );

	enum State
	{
		Enabled = 0,
		Disabled,
		Focus,
		FocusDisabled,
		//Open,			//when a hybrid button has spawned a flyout menu
		NUMSTATES		//must always be last!
	};


	enum ListSelectionChange_t
	{
		SELECT_PREV,
		SELECT_NEXT,
	};

	struct DialogListItem_t
	{
		DialogListItem_t()
		{
			m_bEnabled = true;
		}

		CUtlString	m_String;
		CUtlString	m_StringParm1;
		CUtlString	m_CommandString;
		bool		m_bEnabled;
	};

	CDialogListButton( Panel *parent, const char *panelName, const char *text, Panel *pActionSignalTarget = NULL, const char *pCmd = NULL);
	CDialogListButton( Panel *parent, const char *panelName, const wchar_t *text, Panel *pActionSignalTarget = NULL, const char *pCmd = NULL);
	virtual ~CDialogListButton();

	State			GetCurrentState();
	int				GetOriginalTall() { return m_originalTall; }
	
	void		NavigateTo( );
	void		NavigateFrom( );

	void		SetCurrentSelection( const char *pText );
	void		SetCurrentSelectionIndex( int nIndex );
	void		SetArrowsAlwaysVisible( bool bArrowsVisible ) { m_bArrowsAlwaysVisible = bArrowsVisible; }
	void		SetNextSelection();
	void		SetPreviousSelection();
	void		ModifySelectionString( const char *pCommand, const char *pNewText );
	void		EnableListItem( const char *pText, bool bEnable );
	int			GetCurrentSelectionIndex() { return m_nDialogListCurrentIndex; }
	bool		GetListSelectionString( const char *pCommand, char *pOutBuff, int nOutBuffSize );
	void		ModifySelectionStringParms( const char *pCommand, const char *pParm1 );
	void		ForceCurrentSelectionCommand();
	void		SetCanWrap( bool bWrap ) { m_bCanWrap = bWrap; }
	void		SetDrawAsDualStateButton( bool bDualStateButton ) { m_bDrawAsDualStateButton = bDualStateButton; }

	int			GetListItemCount() { return m_DialogListItems.Count(); }
	void		SetListItemText( int nIndex, const char *pText );
	

	virtual void ApplySettings( KeyValues *inResourceData );

	int			GetWideAtOpen() { return m_nWideAtOpen; }

	void		SetClrEnabledOverride( Color clr ) { m_clrEnabledOverride = clr; }

	void		SetPostCheckMark( bool bEnable ) { m_bPostCheckMark = bEnable; }

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void OnKeyCodeReleased( vgui::KeyCode keycode );
	virtual void Paint();

private:
	void		PaintButtonEx();
	void		ChangeDialogListSelection( ListSelectionChange_t eNext );
	void		DrawDialogListButton( Color textColor );
	bool		GetDialogListButtonCenter( int &x, int &y );
	void		GetStringAtIndex( int nIndex, wchar_t *szString, int nArraySize );

	int			m_originalTall;
	int			m_textInsetX;
	int			m_textInsetY;
	int			m_arrowInsetX;
	int			m_nListInsetX;

	CPanelAnimationVar( int, m_nListNumber, "list_number", "0" );
	// which list in resource file to populate with

	bool		m_isNavigateTo; //to help cure flashing
	

	vgui::HFont	m_hTextFont;
	vgui::HFont	m_hTextFontSmall;
	vgui::HFont	m_hSymbolFont;

	int			m_nTextFontHeight;

	int			m_nDialogListCurrentIndex;

	int			m_iSelectedArrow;			// texture ids for the arrow
	int			m_iUnselectedArrow;
	int			m_iSelectedArrowSize;		// size to draw the arrow

	bool		m_bArrowsAlwaysVisible;

	int			m_nWideAtOpen;

	Color		m_clrEnabledOverride;
	Color		m_TextColor;
	Color		m_FocusColor;
	Color		m_CursorColor;
	Color		m_BaseColor;
	Color		m_DisabledColor;
	Color		m_FocusDisabledColor;
	Color		m_ListButtonActiveColor;
	Color		m_ListButtonInactiveColor;
	Color		m_ListButtonNonFocusColor;

	bool		m_bAllCaps;
	bool		m_bPostCheckMark;
	bool		m_bCanWrap;
	bool		m_bDrawAsDualStateButton;
	int			m_nDualStateCenter;

	int			m_nCursorHeight;
	int			m_nMultiline;

	int			m_nEnabledImageId;
	int			m_nFocusImageId;

	int				m_nBitmapFrame;
	float			m_flLastBitmapAnimTime;
	unsigned int	m_nBitmapFrameCache;

	CUtlVector< DialogListItem_t > m_DialogListItems;
};

#endif //__VDIALOGLISTBUTTON_H__