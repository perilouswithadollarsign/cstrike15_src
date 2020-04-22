//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SAVEGAMEDIALOG_H
#define SAVEGAMEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "basesavegamedialog.h"
#include "savegamebrowserdialog.h"

//-----------------------------------------------------------------------------
// Purpose: Save game dialog
//-----------------------------------------------------------------------------
class CSaveGameDialog : public CBaseSaveGameDialog
{
	DECLARE_CLASS_SIMPLE( CSaveGameDialog, CBaseSaveGameDialog );

public:
	explicit CSaveGameDialog( vgui::Panel *parent );
	~CSaveGameDialog();

	virtual void Activate();
	static void FindSaveSlot( char *buffer, int bufsize );

protected:
	virtual void OnCommand( const char *command );
	virtual void OnScanningSaveGames();
};

#define SAVE_NUM_ITEMS 4

//
//
//
// dgoodenough - GCC apparently does not consider "friend class foo;" as a forward declaration
// of class foo.  So we work around this by providing an explicit forward declaration.
// PS3_BUILDFIX
class CAsyncCtxSaveGame;

class CSaveGameDialogXbox : public CSaveGameBrowserDialog
{
	DECLARE_CLASS_SIMPLE( CSaveGameDialogXbox, CSaveGameBrowserDialog );

public:
	explicit 		CSaveGameDialogXbox( vgui::Panel *parent );
	virtual void	PerformSelectedAction( void );
	virtual void	UpdateFooterOptions( void );
	virtual void	OnCommand( const char *command );
	virtual void	OnDoneScanningSaveGames( void );

private:
	friend class CAsyncCtxSaveGame;
	void			InitiateSaving();
	void			SaveCompleted( CAsyncCtxSaveGame *pCtx );

private:
	bool					m_bGameSaving;
	bool					m_bNewSaveAvailable;
	SaveGameDescription_t	m_NewSaveDesc;
};

#endif // SAVEGAMEDIALOG_H
