//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VCONTROLLEROPTIONSBUTTONS_H__
#define __VCONTROLLEROPTIONSBUTTONS_H__

#include "basemodui.h"
#include "VFlyoutMenu.h"

class VControlsListPanel;

// reserved (needs to be safely large enough but less than 8 bits due to TD storage)
// replicated in host.cpp
#define CONTROLLER_BUTTONS_SPECCUSTOM 100

namespace BaseModUI {

class ControllerOptionsButtons : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( ControllerOptionsButtons, CBaseModFrame );

public:
	ControllerOptionsButtons( vgui::Panel *parent, const char *panelName );
	~ControllerOptionsButtons();

	void ResetCustomToDefault();

	// Trap row selection message
	MESSAGE_FUNC_INT( ItemSelected, "ItemSelected", itemID );
	MESSAGE_FUNC_INT( ItemLeftClick, "ItemLeftClick", itemID );
	MESSAGE_FUNC_INT( ItemDoubleLeftClick, "ItemDoubleLeftClick", itemID );

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnCommand(const char *command);
	virtual void OnKeyCodePressed(vgui::KeyCode code);
	virtual void OnThink();
	virtual void SetDataSettings( KeyValues *pSettings );
	virtual void OnMousePressed(vgui::MouseCode code);

	Panel* NavigateBack();

	MESSAGE_FUNC_HANDLE( OnHybridButtonNavigatedTo, "OnHybridButtonNavigatedTo", button );

private:
	void RecalculateBindingLabels();
	void UpdateFooter();
	void SetupControlStates( bool bCustom );
	void PopulateCustomBindings();
	int BindingNameToIndex( const char *pName );
	bool SetActionFromBinding( int nItemID, bool bSelected );
	void SaveCustomBindings();
	const wchar_t *ButtonCodeToString( ButtonCode_t buttonCode );
	bool FinishCustomizingButtons();
	bool SelectPreviousBinding( int nItemID );
	bool SelectNextBinding( int nItemID );

	int m_iActiveUserSlot;
	int m_nRecalculateLabelsTicks; // used to delay polling the values until we've flushed the command buffer 

	int m_nActionColumnWidth;
	int m_nButtonColumnWidth;

	vgui::HFont	m_hKeyFont;
	vgui::HFont	m_hHeaderFont;

	VControlsListPanel *m_pCustomBindList;

	bool m_bCustomizingButtons;
	bool m_bDirtyCustomConfig;
};

extern void ResetControllerConfig();

};

#endif
