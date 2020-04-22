//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VCONTROLLEROPTIONSSTICKS_H__
#define __VCONTROLLEROPTIONSSTICKS_H__

#include "basemodui.h"

namespace BaseModUI {

class ControllerOptionsSticks : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( ControllerOptionsSticks, CBaseModFrame );

public:
	ControllerOptionsSticks(vgui::Panel *parent, const char *panelName);
	~ControllerOptionsSticks();

	MESSAGE_FUNC_HANDLE( OnHybridButtonNavigatedTo, "OnHybridButtonNavigatedTo", button );

protected:
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	OnCommand( const char *command );
	virtual void	OnKeyCodePressed( vgui::KeyCode code );
	virtual Panel	*NavigateBack();
	virtual void	SetDataSettings( KeyValues *pSettings );

private:
	void SetImageState( int nStickState );
	void UpdateFooter();
	void UpdateButtonNames();

	int m_iActiveUserSlot;
};

};

#endif
