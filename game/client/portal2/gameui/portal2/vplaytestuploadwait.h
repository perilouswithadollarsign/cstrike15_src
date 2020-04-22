//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#ifndef __VPLAYTESTUPLOADWAIT_H__
#define __VPLAYTESTUPLOADWAIT_H__

#if defined( PORTAL2_PUZZLEMAKER )

#include "basemodui.h"

namespace BaseModUI {

class CPlaytestUploadWait : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( CPlaytestUploadWait, CBaseModFrame );

public:
	CPlaytestUploadWait( vgui::Panel *pParent, const char *pPanelName );
	~CPlaytestUploadWait();

protected:
	virtual void	Activate();
	virtual void	ApplySchemeSettings( vgui::IScheme* pScheme );
	virtual void	OnKeyCodePressed( vgui::KeyCode code );
	virtual void	OnCommand( const char *pCommand );
	virtual void	OnThink();

private:
	void ClockSpinner( void );

	// working animation
	vgui::ImagePanel	*m_pSpinner;
};

};

#endif // PORTAL2_PUZZLEMAKER

#endif // __VPLAYTESTUPLOADWAIT_H__
