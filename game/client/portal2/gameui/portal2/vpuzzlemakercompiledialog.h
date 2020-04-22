//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#ifndef __VPUZZLEMAKERCOMPILEDIALOG_H__
#define __VPUZZLEMAKERCOMPILEDIALOG_H__

#if defined( PORTAL2_PUZZLEMAKER )

#include "basemodui.h"
#include <vgui_controls/progressbar.h>

namespace BaseModUI
{
	
class CPuzzleMakerCompileDialog : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( CPuzzleMakerCompileDialog, CBaseModFrame );

public:
	CPuzzleMakerCompileDialog( vgui::Panel *pParent, const char *pPanelName );
	~CPuzzleMakerCompileDialog();

	void CancelAndClose();
	void SetFromShortcut( bool bFromShortcut );
	void CompileFailed( int nFailedErrorCode, const char *pszFailedProcess );
	void CompileError( const char *pszError, bool bShowHelpButton );

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme );
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void OnThink();
#ifndef _GAMECONSOLE
	virtual void OnKeyCodeTyped( vgui::KeyCode code );
#endif

private:
	void UpdateFooter();

	void UpdateHintText();

	vgui::ImagePanel *m_pSpinner;
	vgui::Label *m_pHintMsgLabel;

	vgui::ContinuousProgressBar *m_pVBSPProgress;
	vgui::ContinuousProgressBar *m_pVVISProgress;
	vgui::ContinuousProgressBar *m_pVRADProgress;

	bool m_bFromShortcut;
};

};

#endif //PORTAL2_PUZZLEMAKER

#endif //__VPUZZLEMAKERCOMPILEDIALOG_H__
