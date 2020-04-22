//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//
#ifndef __VPUZZLEMAKEREXITCONFIRMATION_H__
#define __VPUZZLEMAKEREXITCONFIRMATION_H__

#if defined( PORTAL2_PUZZLEMAKER )

#include "basemodui.h"

namespace BaseModUI
{

enum ExitConfirmationReason_t
{
	EXIT_REASON_EXIT_FROM_VGUI,
	EXIT_REASON_OPEN,
	EXIT_REASON_NEW,
	EXIT_REASON_EXIT_FROM_PUZZLEMAKER_UI,
	EXIT_REASON_QUIT_GAME,
	EXIT_REASON_JOIN_COOP_GAME
};

class CPuzzleMakerExitConfirmation : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( CPuzzleMakerExitConfirmation, CBaseModFrame );

public:
	CPuzzleMakerExitConfirmation(vgui::Panel *parent, const char *panelName);
	~CPuzzleMakerExitConfirmation();

	void SetReason( ExitConfirmationReason_t eReason);

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnCommand(const char *command);
	virtual void OnKeyCodePressed(vgui::KeyCode code);
#ifndef _GAMECONSOLE
	virtual void OnKeyCodeTyped( vgui::KeyCode code );
#endif
	virtual void OnOpen();
	virtual void PaintBackground();

private:
	void FixLayout();
	void UpdateFooter();

	void PuzzleMakerExitSave();
	void PuzzleMakerExitDiscard();
	void PuzzleMakerExitCancel();

	vgui::Label		*m_pLblMessage;

	bool			m_bNeedsMoveToFront;
	bool			m_bValid;

	vgui::HFont		m_hMessageFont;
	int				m_nTextOffsetX;
	int				m_nIconOffsetY;

	ExitConfirmationReason_t m_eReason;
};

}

#endif //PORTAL2_PUZZLEMAKER

#endif //__VPUZZLEMAKEREXITCONFIRMATION_H__
