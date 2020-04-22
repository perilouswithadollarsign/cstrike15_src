//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef OPTIONSDIALOG_H
#define OPTIONSDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/PropertyDialog.h"


enum OptionsDialogTabStyle
{
	OPTIONS_DIALOG_ALL_TABS,
	OPTIONS_DIALOG_ONLY_BINDING_TABS,

	OPTIONS_DIALOG_TAB_STYLE_TOTAL
};


//-----------------------------------------------------------------------------
// Purpose: Holds all the game option pages
//-----------------------------------------------------------------------------
class COptionsDialog : public vgui::PropertyDialog
{
	DECLARE_CLASS_SIMPLE( COptionsDialog, vgui::PropertyDialog );

public:
	COptionsDialog(vgui::Panel *parent, OptionsDialogTabStyle iTabStyle = OPTIONS_DIALOG_ALL_TABS );
	~COptionsDialog();

	void Run();
	virtual void Activate();
	void OpenGammaDialog();

	MESSAGE_FUNC( OnGameUIHidden, "GameUIHidden" );	// called when the GameUI is hidden

protected:
	// [jason] Overriding to let Scaleform know that a vgui dialog closed
	virtual void OnFinishedClose( void );

private:
	class COptionsSubAudio *m_pOptionsSubAudio;
	class COptionsSubVideo *m_pOptionsSubVideo;
};


struct OptionData_t;

#endif // OPTIONSDIALOG_H
