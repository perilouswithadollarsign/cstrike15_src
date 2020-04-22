//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VFADEOUTTOECONUI_H__
#define __VFADEOUTTOECONUI_H__

#include "basemodui.h"

class ImagePanel;

namespace BaseModUI {

class CFadeOutToEconUI : public CBaseModFrame, public IBaseModFrameListener
{
	DECLARE_CLASS_SIMPLE( CFadeOutToEconUI, CBaseModFrame );

public:
	CFadeOutToEconUI( vgui::Panel *pParent, const char *pPanelName );
	~CFadeOutToEconUI();

protected:
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void Activate();
	virtual void RunFrame();
	virtual void PaintBackground();

private:
	void		StartEconUI();
	void		UpdateFooter();

	bool		m_bStarted;
	int			m_nFrames;
};

};

#endif
