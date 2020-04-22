//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VHYBRIDBUTTON_H__
#define __VHYBRIDBUTTON_H__

#include "basemodui.h"

//=============================================================================
// The Hybrid Button has the ability to either display a texture , solid color
//	or gradient color depending on it's state.
//=============================================================================

namespace BaseModUI
{

class BaseModHybridButton : public vgui::Button
{
public:
	DECLARE_CLASS_SIMPLE( BaseModHybridButton, vgui::Button );

	enum State
	{
		Enabled = 0,
		Disabled,
		Focus,
		FocusDisabled,
		Open,			//when a hybrid button has spawned a flyout menu
		NUMSTATES		//must always be last!
	};

	enum ButtonStyle_t
	{
		BUTTON_DEFAULT = 0,
		BUTTON_MAINMENU,		// item strictly on the main menu
		BUTTON_LEFTINDIALOG,	// button inside of a dialog, left aligned
		BUTTON_DIALOGLIST,
		BUTTON_FLYOUTITEM,		// item inside of a flyout
		BUTTON_DROPDOWN,		// button that has an associated value with an optional flyout
		BUTTON_GAMEMODE,		// button on game mode carousel
		BUTTON_VIRTUALNAV,		// virtual navigation button
		BUTTON_MIXEDCASE,		// mixed case button (Steam link dialog, double-height cursor)
		BUTTON_MIXEDCASEDEFAULT,// mixed case default button
		BUTTON_BITMAP
	};

	enum EnableCondition
	{
		EC_ALWAYS,
		EC_LIVE_REQUIRED,
		EC_NOTFORDEMO,
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

	BaseModHybridButton( Panel *parent, const char *panelName, const char *text, Panel *pActionSignalTarget = NULL, const char *pCmd = NULL );
	BaseModHybridButton( Panel *parent, const char *panelName, const wchar_t *text, Panel *pActionSignalTarget = NULL, const char *pCmd = NULL );
	virtual ~BaseModHybridButton();

	State			GetCurrentState();
	int				GetOriginalTall() { return m_originalTall; }
	ButtonStyle_t	GetStyle() { return m_nStyle; }
	
	//used by flyout menus to indicate this button has spawned a flyout.
	void		SetOpen();
	void		SetClosed();
	void		NavigateTo( );
	void		NavigateFrom( );

	// only applies to drop down style
	void		EnableDropdownSelection( bool bEnable );
	void		SetShowDropDownIndicator( bool bShowIndicator ) { m_bShowDropDownIndicator = bShowIndicator; }
	void		SetOverrideDropDownIndicator( bool bOverrideDropDownIndicator ) { m_bOverrideDropDownIndicator = bOverrideDropDownIndicator; }
	void		SetCurrentSelection( const char *pText );
	void		ModifySelectionString( const char *pCommand, const char *pNewText );
	void		EnableListItem( const char *pText, bool bEnable );
	bool		GetListSelectionString( const char *pCommand, char *pOutBuff, int nOutBuffSize );
	void		ModifySelectionStringParms( const char *pCommand, const char *pParm1 );

	virtual void ApplySettings( KeyValues *inResourceData );

	int			GetWideAtOpen() { return m_nWideAtOpen; }

	void		SetClrEnabledOverride( Color clr ) { m_clrEnabledOverride = clr; }

	void		SetPostCheckMark( bool bEnable ) { m_bPostCheckMark = bEnable; }

	void		SetUseAlternateTiles( bool bUseAlternate ) { m_bUseAlternateTiles = bUseAlternate; }

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void OnKeyCodeReleased( vgui::KeyCode keycode );
	virtual void Paint();
	virtual void OnThink();
	virtual void OnCursorEntered();
	virtual void OnCursorExited();
	virtual void FireActionSignal( void );

	MESSAGE_FUNC( OnSiblingHybridButtonOpened, "OnSiblingHybridButtonOpened" );

	virtual Panel* NavigateUp();
	virtual Panel* NavigateDown();
	virtual Panel* NavigateLeft();
	virtual Panel* NavigateRight();

	Color		m_clrEnabledOverride;
	Color		m_TextColor;
	Color		m_FocusColor;
	Color		m_CursorColor;
	Color		m_DisabledColor;
	Color		m_FocusDisabledColor;
	Color		m_ListButtonActiveColor;
	Color		m_ListButtonInactiveColor;

private:
	void		PaintButtonEx();
	void		ChangeDialogListSelection( ListSelectionChange_t eNext );
	void		DrawDialogListButton( Color textColor );
	bool		GetDialogListButtonCenter( int &x, int &y );

	int			m_originalTall;
	int			m_textInsetX;
	int			m_textInsetY;
	int			m_nListInsetX;

	bool		m_isOpen;
	bool		m_isNavigateTo; //to help cure flashing
	bool		m_bOnlyActiveUser;
	bool		m_bIgnoreButtonA;

	enum UseIndex_t
	{
		USE_NOBODY = -2,		// Nobody can use the button
		USE_EVERYBODY = -1,		// Everybody can use the button
		USE_SLOT0 = 0,			// Only Slot0 can use the button
		USE_SLOT1,				// Only Slot1 can use the button
		USE_SLOT2,				// Only Slot2 can use the button
		USE_SLOT3,				// Only Slot3 can use the button
		USE_PRIMARY = 0xFF,		// Only primary user can use the button
	};

	int m_iUsablePlayerIndex;
	EnableCondition mEnableCondition;
	
	ButtonStyle_t m_nStyle;

	vgui::HFont	m_hTextFont;
	vgui::HFont	m_hSymbolFont;

	int			m_nTextFontHeight;

	int			m_nDialogListCurrentIndex;
	bool		m_bShowDropDownIndicator;		// down arrow used for player names that can be clicked on
	bool		m_bOverrideDropDownIndicator;	// down arrow used for player names that can be clicked on
	bool		m_bUseAlternateTiles;			// Use an alternate color scheme under different menu conditions
	int			m_iSelectedArrow;				// texture ids for the arrow
	int			m_iUnselectedArrow;
	int			m_iSelectedArrowSize;			// size to draw the arrow

	int			m_nWideAtOpen;

	bool		m_bAllCaps;
	bool		m_bPostCheckMark;

	int			m_nCursorHeight;
	int			m_nMultiline;

	int			m_nEnabledImageId;
	int			m_nFocusImageId;

	int				m_nBitmapFrame;
	float			m_flLastBitmapAnimTime;
	unsigned int	m_nBitmapFrameCache;

	CUtlVector< DialogListItem_t > m_DialogListItems;
};

}; //namespace BaseModUI

#endif //__VHYBRIDBUTTON_H__