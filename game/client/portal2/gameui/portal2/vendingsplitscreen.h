//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VENDINGSPLITSCREENDIALOG_H__
#define __VENDINGSPLITSCREENDIALOG_H__

#include "basemodui.h"

namespace BaseModUI {

class EndingSplitscreenDialog : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( EndingSplitscreenDialog, CBaseModFrame );

public:
	EndingSplitscreenDialog(vgui::Panel *parent, const char *panelName);

	void OnThink();

	virtual void PaintBackground() {}
	virtual void Paint() {}

protected:
	float m_flTimeStart;
};

}

#endif // __VENDINGSPLITSCREENDIALOG_H__