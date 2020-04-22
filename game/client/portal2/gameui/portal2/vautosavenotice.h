//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VAUTOSAVENOTICE_H__
#define __VAUTOSAVENOTICE_H__

#include "basemodui.h"

class ImagePanel;

namespace BaseModUI {

class CAutoSaveNotice : public CBaseModFrame, public IBaseModFrameListener
{
	DECLARE_CLASS_SIMPLE( CAutoSaveNotice, CBaseModFrame );

public:
	CAutoSaveNotice( vgui::Panel *pParent, const char *pPanelName );
	~CAutoSaveNotice();

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void SetDataSettings( KeyValues *pSettings );
	virtual void Activate();
	virtual void RunFrame();

private:
	void		UpdateFooter();
	void		ClockAnim();

	vgui::ImagePanel	*m_pAutoSaveIcon;
	vgui::Label			*m_pStatusLabel;

	float		m_flAutoContinueTimeout;
	float		m_flLastEngineTime;
	int			m_nCurrentSpinnerValue;

	int			m_nRevolutions;
	int			m_nInstallStatus;

	CUtlString	m_MapName;
	CUtlString	m_LoadFilename;
	CUtlString	m_ReasonString;
};

};

#endif
