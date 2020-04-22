//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#ifndef __VSOUNDTEST_H__
#define __VSOUNDTEST_H__

#include "basemodui.h"

namespace BaseModUI {

class CSoundTest : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( CSoundTest, CBaseModFrame );

public:
	CSoundTest( vgui::Panel *pParent, const char *pPanelName );
	~CSoundTest();

protected:
	virtual void	Activate();
	virtual void	ApplySchemeSettings( vgui::IScheme* pScheme );
	virtual void	OnKeyCodePressed( vgui::KeyCode code );
	virtual void	OnCommand( const char *pCommand );
	virtual void	OnThink();

private:
	void	UpdateFooter();
};

};

#endif // __VSOUNDTEST_H__
