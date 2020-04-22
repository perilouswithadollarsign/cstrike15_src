//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#ifndef __VPORTALLEADERBOARDHUD_H__
#define __VPORTALLEADERBOARDHUD_H__

#include "basemodui.h"
#include "portal2_leaderboard.h"
#include "vportalleaderboard.h"
#include "vgui_controls/imagepanel.h"

namespace BaseModUI
{
	class GenericPanelList;
}

class CPortalLeaderboardGraphPanel;

namespace BaseModUI {

// TODO: Rename to CPortalLeaderboardHUD once old HUD is gone
class CPortalHUDLeaderboard : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( CPortalHUDLeaderboard, CBaseModFrame );

public:
	CPortalHUDLeaderboard( vgui::Panel *pParent, const char *pPanelName );
	~CPortalHUDLeaderboard();

	MESSAGE_FUNC( MsgPreGoToHub, "MsgPreGoToHub" );
	MESSAGE_FUNC( MsgGoToHub, "MsgGoToHub" );

	MESSAGE_FUNC( MsgGoToNext, "MsgGoToNext" );
	MESSAGE_FUNC( MsgRetryMap, "MsgRetryMap" );

	virtual void	OnKeyCodePressed( vgui::KeyCode code );
	virtual int		GetAvatarLegendOffset() { return m_nAvatarLegendOffset; }

	virtual void OnClose();

protected:
	virtual void	Activate();
	virtual void	ApplySchemeSettings( vgui::IScheme* pScheme );
	virtual void	OnCommand( const char *pCommand );
	virtual void	OnThink();
	virtual void	Update();
	virtual void	SetDataSettings( KeyValues *pSettings );

	CPanelAnimationVarAliasType( int, m_nStatHeight, "statHeight", "38", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nAvatarLegendOffset, "legend_offset", "15", "proportional_int" );

private:
	void	UpdateFooter();
	void	UpdateLeaderboard( LeaderboardType type );
	void	UpdateStatPanel();
	void	ClearData();
	void	SetMapTitle();
	void	SetNextMap();
	bool	IsMapLocked( int nChapterNumber, int nMapNumber, bool bSinglePlayer );
	void	ReturnToMainMenu();
	void	GoToHub();

	void ClockSpinner( bool bVisible );

	CPortalLeaderboardGraphPanel *m_pGraphPanels[NUM_LEADERBOARDS];
	BaseModUI::GenericPanelList    *m_pStatLists[NUM_LEADERBOARDS];

	char m_szNextMap[ MAX_MAP_NAME ];

	CPortalLeaderboard *m_pLeaderboard;

	CPortalChallengeStatsPanel *m_pChallengeStatsPanel;

	Label *m_pInvalidLabel;
	Label *m_pInvalidLabel2;
	ImagePanel *m_pWorkingAnim;

	Label *m_pFewestPortalsLabel;
	Label *m_pFastestTimeLabel;
	Label *m_pEveryoneLabel;

	bool m_bNeedsUpdate;
	bool m_bEnabled;
	bool m_bOnline;
	bool m_bCheated;
	bool m_bCommittedAction;
	int m_nSelectedPanelIndex;
	LeaderboardState_t m_leaderboardState;
};

};

#endif // __VSOUNDTEST_H__
