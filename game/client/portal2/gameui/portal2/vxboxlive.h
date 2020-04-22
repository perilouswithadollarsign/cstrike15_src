//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VXBOXLIVE_H__
#define __VXBOXLIVE_H__

#include "basemodui.h"

namespace BaseModUI {

class BaseModHybridButton;

class XboxLiveOptions : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( XboxLiveOptions, CBaseModFrame );

public:
	XboxLiveOptions( vgui::Panel *pParent, const char *pPanelName );
	~XboxLiveOptions();

protected:
	virtual void OnCommand( const char *pCommand );
	virtual void OnClose();
	virtual void Activate();
	virtual void OnThink();

private:
	void UpdateFooter();
	void RedeemSteamCode();
	int m_nMsgBoxId;

#ifdef _PS3
	bool m_bVirtualKeyboardStarted;
	#if !defined( NO_STEAM )
	CCallResult< XboxLiveOptions, RegisterActivationCodeResponse_t > m_CallbackOnRegisterActivationCodeResponse;
	void Steam_OnRegisterActivationCodeResponse( RegisterActivationCodeResponse_t *p, bool bError );
	#endif
#endif
};

};

#endif // __VXBOXLIVE_H__
