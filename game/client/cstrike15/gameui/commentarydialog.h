//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef COMMENTARYDIALOG_H
#define COMMENTARYDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"
#include "utlvector.h"
#include <vgui/KeyCode.h>

class CGameChapterPanel;
class CSkillSelectionDialog;

//-----------------------------------------------------------------------------
// Purpose: Handles selection of commentary mode on/off
//-----------------------------------------------------------------------------
class CCommentaryDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CCommentaryDialog, vgui::Frame );

public:
	explicit CCommentaryDialog(vgui::Panel *parent);
	~CCommentaryDialog();

	virtual void OnClose( void );
	virtual void OnCommand( const char *command );
	virtual void OnKeyCodePressed(vgui::KeyCode code);
};

//-----------------------------------------------------------------------------
// Purpose: Small dialog to remind players on method of changing commentary mode
//-----------------------------------------------------------------------------
class CPostCommentaryDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CPostCommentaryDialog, vgui::Frame );

public:
	explicit CPostCommentaryDialog(vgui::Panel *parent);
	~CPostCommentaryDialog();

	virtual void OnFinishedClose( void );
	virtual void OnKeyCodePressed(vgui::KeyCode code);

private:
	bool m_bResetPaintRestrict;
};

#endif // COMMENTARYDIALOG_H
