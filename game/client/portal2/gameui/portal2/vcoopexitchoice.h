//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#ifndef __VCOOPEXITCHOICE_H__
#define __VCOOPEXITCHOICE_H__

#include "basemodui.h"

namespace BaseModUI {

class CCoopExitChoice : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( CCoopExitChoice, CBaseModFrame );

public:
	CCoopExitChoice( vgui::Panel *pParent, const char *pPanelName );
	~CCoopExitChoice();

	MESSAGE_FUNC( MsgPreGoToHub, "MsgPreGoToHub" );
	MESSAGE_FUNC( MsgGoToHub, "MsgGoToHub" );

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

#endif // __VCOOPEXITCHOICE_H__
