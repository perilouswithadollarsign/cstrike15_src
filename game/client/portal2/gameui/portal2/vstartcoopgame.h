//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VSTARTCOOPGAME_H__
#define __VSTARTCOOPGAME_H__

#include "basemodui.h"
#include "vfoundgames.h"
#include "matchmaking/imatchframework.h"

class vgui::Label;

namespace BaseModUI {

class CStartCoopGame : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( CStartCoopGame, CBaseModFrame );

public:
	CStartCoopGame( vgui::Panel *parent, const char *panelName );

	static const char *CoopGameMode( void ) { return sm_szGameMode; }
	static const char *CoopChallengeMap( void ) { return sm_szChallengeMap; }

protected:
	virtual void OnCommand( char const *szCommand );
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnKeyCodePressed(vgui::KeyCode code);
	virtual void Activate();


	virtual void SetDataSettings( KeyValues *pSettings );

private:
	void UpdateFooter();

	static char sm_szGameMode[64];
	static char sm_szChallengeMap[64];
};

};

#endif
