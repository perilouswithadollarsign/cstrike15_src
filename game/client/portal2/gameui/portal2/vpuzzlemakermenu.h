//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#ifndef __VPUZZLEMAKERMENU_H__
#define __VPUZZLEMAKERMENU_H__

#if defined( PORTAL2_PUZZLEMAKER )

#include "basemodui.h"
#include "vgui_controls/label.h"
#include "vgui_controls/imagepanel.h"


namespace BaseModUI {

class CPuzzleMakerMenu : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( CPuzzleMakerMenu, CBaseModFrame );

public:
	CPuzzleMakerMenu( vgui::Panel *pParent, const char *pPanelName );
	~CPuzzleMakerMenu();

	static void FixFooter();

protected:
	virtual void	Activate();
	virtual void	ApplySchemeSettings( vgui::IScheme* pScheme );
	virtual void	OnKeyCodePressed( vgui::KeyCode code );
	virtual void	OnCommand( const char *pCommand );
	virtual void	OnThink();
	virtual void	PaintBackground();
	virtual vgui::Panel *NavigateBack();

private:
	void	UpdateFooter( void );
	void	UpdateSpinner( void );

	vgui::Label					*m_pEmployeeTitle;		// title for the player

	vgui::IImage				*m_pAvatar;
	vgui::ImagePanel			*m_pAvatarSpinner;
	vgui::ImagePanel			*m_pEmployeeImage;		// Avatar on the badge

	float						m_flRetryAvatarTime;

	uint64						m_nSteamID;
};

};

#endif // PORTAL2_PUZZLEMAKER

#endif // __VPUZZLEMAKERMENU_H__
