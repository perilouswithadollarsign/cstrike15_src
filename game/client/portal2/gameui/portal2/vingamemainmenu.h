//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VINGAMEMAINMENU_H__
#define __VINGAMEMAINMENU_H__

#include "basemodui.h"
#include "VFlyoutMenu.h"

namespace BaseModUI {

class InGameMainMenu : public CBaseModFrame, public FlyoutMenuListener
{
	DECLARE_CLASS_SIMPLE( InGameMainMenu, CBaseModFrame );

public:
	InGameMainMenu( vgui::Panel *parent, const char *panelName );
	~InGameMainMenu();

	// Public methods
	void OnCommand(const char *command);

	// Overrides
	virtual void OnKeyCodePressed(vgui::KeyCode code);
	virtual void OnOpen();
	virtual void OnClose();
	virtual void OnThink();
	virtual void PerformLayout();
	virtual void Unpause();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Activate();

	//flyout menu listener
	virtual void OnNotifyChildFocus( vgui::Panel* child );
	virtual void OnFlyoutMenuClose( vgui::Panel* flyTo );
	virtual void OnFlyoutMenuCancelled();

	MESSAGE_FUNC( OnGameUIHidden, "GameUIHidden" );	// called when the GameUI is hidden
	MESSAGE_FUNC( MsgPreGoToHub, "MsgPreGoToHub" );
	MESSAGE_FUNC( MsgGoToHub, "MsgGoToHub" );
	MESSAGE_FUNC( MsgPreGoToCalibration, "MsgPreGoToCalibration" );
	MESSAGE_FUNC( MsgGoToCalibration, "MsgGoToCalibration" );
	MESSAGE_FUNC( MsgPreRestartLevel, "MsgPreRestartLevel" );
	MESSAGE_FUNC( MsgRestartLevel, "MsgRestartLevel" );

#if defined( PORTAL2_PUZZLEMAKER )
	MESSAGE_FUNC( MsgPreSkipToNextLevel, "MsgPreSkipToNextLevel" );
	MESSAGE_FUNC_PARAMS( MsgRequestMapRating, "MsgRequestMapRating", pParams );
	MESSAGE_FUNC_CHARPTR( MapDownloadAborted, "MapDownloadAborted", msg );
	MESSAGE_FUNC_CHARPTR( MapDownloadFailed, "MapDownloadFailed", msg );
	MESSAGE_FUNC_CHARPTR( MapDownloadComplete, "MapDownloadComplete", msg );	
#endif // PORTAL2_PUZZLEMAKER

	MESSAGE_FUNC_PARAMS( MsgWaitingForOpen, "MsgWaitingForOpen", pParams );
	MESSAGE_FUNC_PARAMS( MsgWaitingForExit, "MsgWaitingForExit", pParams );

	MESSAGE_FUNC( MsgLeaveGameConfirm, "MsgLeaveGameConfirm" );
	
private:

	bool	IsInCoopGame() const;	// Checks if in a coop game and not in the puzzlemaker

	KeyValues::AutoDelete m_autodelete_pResourceLoadConditions;

	bool	m_bCanViewGamerCard;

	void	SetFooterState();
	void	SetupPartnerInScience();
	void	UpdateSaveState();

#if defined( PORTAL2_PUZZLEMAKER )	
	void	ProceedToNextMap();
	void	OnSkipSinglePlayer( const PublishedFileInfo_t *pNextMapFileInfo );
	void	OnSkipCoop( const PublishedFileInfo_t *pNextMapFileInfo );
#endif // PORTAL2_PUZZLEMAKER
};

}

#endif // __VINGAMEMAINMENU_H__
