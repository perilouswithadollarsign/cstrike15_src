//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VKEYBOARDMOUSE_H__
#define __VKEYBOARDMOUSE_H__

#include "basemodui.h"
#include "VFlyoutMenu.h"

namespace BaseModUI {

class DropDownMenu;
class SliderControl;
class BaseModHybridButton;

class KeyboardMouse : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( KeyboardMouse, CBaseModFrame );

public:
	KeyboardMouse(vgui::Panel *parent, const char *panelName);
	~KeyboardMouse();

protected:
	virtual void	Activate();
	virtual void	ApplySchemeSettings( vgui::IScheme* pScheme );
	virtual void	OnKeyCodePressed(vgui::KeyCode code);
	virtual void	OnCommand( const char *command );
	virtual	Panel	*NavigateBack();

private:
	void UpdateFooter( bool bEnableCloud );	

	BaseModHybridButton	*m_btnEditBindings;
	BaseModHybridButton	*m_drpMouseYInvert;
	BaseModHybridButton	*m_drpDeveloperConsole;
	BaseModHybridButton	*m_drpRawMouse;
	BaseModHybridButton	*m_drpMouseAcceleration;
	SliderControl		*m_sldMouseSensitivity;
	SliderControl		*m_sldMouseAcceleration;

	bool				m_bDirtyConfig;
};

};

#endif // __VKEYBOARDMOUSE_H__