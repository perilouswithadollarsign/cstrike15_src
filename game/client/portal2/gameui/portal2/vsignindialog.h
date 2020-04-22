//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VSIGNINDIALOG_H__
#define __VSIGNINDIALOG_H__

#include "basemodui.h"

namespace BaseModUI {

class SignInDialog : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( SignInDialog, CBaseModFrame );

public:
	SignInDialog(vgui::Panel *parent, const char *panelName);
	~SignInDialog();

protected:
	virtual void OnCommand(const char *command);
	virtual void OnThink();
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void LoadLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

private:
	vgui::Panel* NavigateBack( int numSlotsRequested );
	void UpdateFooter();

	float			m_flTimeAutoClose;
	vgui::Panel		*m_pBtnSignIn;
};

}

#endif // __VSIGNINDIALOG_H__