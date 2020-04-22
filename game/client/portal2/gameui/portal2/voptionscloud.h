//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VOPTIONSCLOUD_H__
#define __VOPTIONSCLOUD_H__

#include "basemodui.h"

namespace BaseModUI {

class BaseModHybridButton;

class OptionsCloud : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( OptionsCloud, CBaseModFrame );

public:
	OptionsCloud( vgui::Panel *pParent, const char *pPanelName );
	~OptionsCloud();

protected:
	virtual void OnCommand( const char *pCommand );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void OnThink();

private:
	void UpdateFooter();
	int m_numSavesSelected;
};

};

#endif // __VOPTIONS_H__