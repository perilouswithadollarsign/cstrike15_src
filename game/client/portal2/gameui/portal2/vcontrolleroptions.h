//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VCONTROLLEROPTIONS_H__
#define __VCONTROLLEROPTIONS_H__

#include "basemodui.h"
#include "VFlyoutMenu.h"

namespace BaseModUI {

class SliderControl;
class DropDownMenu;

class ControllerOptions : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( ControllerOptions, CBaseModFrame );

public:
	ControllerOptions(vgui::Panel *parent, const char *panelName);
	~ControllerOptions();

	void ResetControlValues( void );
	void ResetToDefaults( void );
	void ChangeToDuckMode( int iDuckMode );

	Panel	*NavigateBack();

protected:
	virtual void ApplySchemeSettings( vgui::IScheme* pScheme );
	virtual void OnCommand(const char *command);
	virtual void OnKeyCodePressed(vgui::KeyCode code);
	virtual void OnNotifyChildFocus( vgui::Panel* child );
	virtual void OnFlyoutMenuClose( vgui::Panel* flyTo );
	virtual void OnFlyoutMenuCancelled();
	virtual void Activate();
	virtual void OnThink();
	virtual void SetDataSettings( KeyValues *pSettings );

private:
	void UpdateFooter();
	void ConfirmUseDefaults();

	SliderControl		*m_pVerticalSensitivity;
	SliderControl		*m_pHorizontalSensitivity;
	BaseModHybridButton	*m_pHorizontalLookType;
	BaseModHybridButton	*m_pVerticalLookType;
	BaseModHybridButton	*m_pDuckMode;
	BaseModHybridButton	*m_pEditButtons;
	BaseModHybridButton	*m_pEditSticks;
	BaseModHybridButton	*m_pVibration;
	BaseModHybridButton	*m_pController;

	int		m_iActiveUserSlot;
	bool	m_bDirty;
	int		m_nResetControlValuesTicks; // used to delay polling the values until we've flushed the command buffer 
};

};

#endif
