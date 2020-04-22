//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VFADEOUTSTARTGAME_H__
#define __VFADEOUTSTARTGAME_H__

#include "basemodui.h"

class ImagePanel;

namespace BaseModUI {

class CFadeOutStartGame : public CBaseModFrame, public IBaseModFrameListener
{
	DECLARE_CLASS_SIMPLE( CFadeOutStartGame, CBaseModFrame );

public:
	CFadeOutStartGame( vgui::Panel *pParent, const char *pPanelName );
	~CFadeOutStartGame();

protected:
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void SetDataSettings( KeyValues *pSettings );
	virtual void Activate();
	virtual void RunFrame();
	virtual void PaintBackground();

private:
	void		StartGame();
	void		UpdateFooter();

	CUtlString	m_MapName;
	CUtlString	m_LoadFilename;
	CUtlString	m_ReasonString;

	bool		m_bStarted;
	int			m_nFrames;
};

};

#endif
