//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef LOADGAMEDIALOG_H
#define LOADGAMEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "basesavegamedialog.h"
#include "savegamedialog.h"
#include "savegamebrowserdialog.h"
#include "basepanel.h"

//-----------------------------------------------------------------------------
// Purpose: Displays game loading options
//-----------------------------------------------------------------------------
class CLoadGameDialog : public CBaseSaveGameDialog
{
	DECLARE_CLASS_SIMPLE( CLoadGameDialog, CBaseSaveGameDialog );

public:
	explicit CLoadGameDialog(vgui::Panel *parent);
	~CLoadGameDialog();

	virtual void OnCommand( const char *command );
};

//
//
//

class CLoadGameDialogXbox : public CSaveGameBrowserDialog
{
	DECLARE_CLASS_SIMPLE( CLoadGameDialogXbox, CSaveGameBrowserDialog );

public:
	explicit 		CLoadGameDialogXbox( vgui::Panel *parent );
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	OnCommand(const char *command);
	virtual void	PerformSelectedAction( void );
	virtual void	PerformDeletion( void );
	virtual void	UpdateFooterOptions( void );

private:
	void			DeleteSaveGame( const SaveGameDescription_t *pSaveDesc );
};

#endif // LOADGAMEDIALOG_H
