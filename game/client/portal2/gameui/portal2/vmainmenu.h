//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VMAINMENU_H__
#define __VMAINMENU_H__

#include "basemodui.h"
#include "VFlyoutMenu.h"

namespace BaseModUI {

class MainMenu : public CBaseModFrame, public IBaseModFrameListener, public FlyoutMenuListener
{
	DECLARE_CLASS_SIMPLE( MainMenu, CBaseModFrame );

public:
	MainMenu(vgui::Panel *parent, const char *panelName);
	~MainMenu();

	void Activate();

	void UpdateVisibility();

	MESSAGE_FUNC_CHARPTR( OpenMainMenuJoinFailed, "OpenMainMenuJoinFailed", msg );
	MESSAGE_FUNC( MsgOpenSinglePlayer, "MsgOpenSinglePlayer" );
	MESSAGE_FUNC( MsgOpenCoopMode, "MsgOpenCoopMode" );
	
	//flyout menu listener
	virtual void OnNotifyChildFocus( vgui::Panel* child );
	virtual void OnFlyoutMenuClose( vgui::Panel* flyTo );
	virtual void OnFlyoutMenuCancelled();

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnCommand(const char *command);
	virtual void OnKeyCodePressed(vgui::KeyCode code);
	virtual void OnThink();
	virtual void OnOpen();
	virtual void RunFrame();
	virtual void PaintBackground();
	virtual void OnNavigateTo( const char* panelName );
#if !defined( _GAMECONSOLE )
	virtual void OnMousePressed( vgui::MouseCode code );
#endif

	void	Demo_DisableButtons( void );

private:
	static void AcceptCommentaryRulesCallback();
	static void AcceptSplitscreenDisableCallback();
	static void AcceptVersusSoftLockCallback();
	static void AcceptQuitGameCallback();
	void SetFooterState();
	void MarkTiles();

	enum MainMenuQuickJoinHelpText
	{
		MMQJHT_NONE,
		MMQJHT_QUICKMATCH,
		MMQJHT_QUICKSTART,
	};
	
	int	m_iQuickJoinHelpText;

	int	m_nTileWidth;
	int m_nTileHeight;
	int m_nPinFromBottom;
	int m_nPinFromLeft;
	int	m_nFooterOffsetY;

public:
	static char const *m_szPreferredControlName;
};

}

#endif // __VMAINMENU_H__
