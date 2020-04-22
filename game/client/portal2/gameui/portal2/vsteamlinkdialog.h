//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VSTEAMLINKDIALOG_H__
#define __VSTEAMLINKDIALOG_H__

#include "basemodui.h"

namespace BaseModUI {

class SteamLinkDialog : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( SteamLinkDialog, CBaseModFrame );

public:
	SteamLinkDialog(vgui::Panel *parent, const char *panelName);
	~SteamLinkDialog();

protected:
	virtual void OnCommand(const char *command);
	virtual void OnThink();
	virtual void LoadLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void SetDataSettings( KeyValues *pSettings );

protected:
	void UpdateFooter();
	void SwitchToUserPwdMode( bool bForceName = false );
	void InvokeVirtualKeyboard( char const *szTitleFmt, char const *szDefaultText, bool bPassword );
	char m_chError[256];
	bool m_bAutomaticNameInput;
	bool m_bVirtualKeyboardStarted;
	bool m_bResetSteamConnectionReason;
	
	vgui::IImage *m_pAvatarImage;
	XUID m_xuidAvatarImage;
};

}

#endif // __VSTEAMLINKDIALOG_H__