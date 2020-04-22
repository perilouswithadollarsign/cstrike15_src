//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#ifndef __VPORTALLEADERBOARD_H__
#define __VPORTALLEADERBOARD_H__

#include "basemodui.h"
#include "VGenericPanelList.h"
#include "portal2_leaderboard_manager.h"

using namespace vgui;
using namespace BaseModUI;

class CDialogListButton;
class CPortalLeaderboardGraphPanel;
class CPortalChallengeStatsPanel;
namespace BaseModUI
{
	class CPortalHUDLeaderboard;
}


// enum for what piece of the UI is selected
typedef enum
{
	SELECT_CHAPTER,
	SELECT_MAP,
	SELECT_LEADERBOARD,
	SELECT_AVATAR,
	SELECT_NONE
} PanelSelection_t;

typedef enum
{
	STATE_MAIN_MENU,
	STATE_PAUSE_MENU,
	STATE_END_OF_LEVEL,
	STATE_TRIGGERED,
	STATE_START_OF_LEVEL,
	STATE_COUNT
} LeaderboardState_t;

namespace BaseModUI {


class CPortalLeaderboardPanel;

//=============================================================================
class CLeaderboardMapItem : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CLeaderboardMapItem, vgui::EditablePanel );
public:
	CLeaderboardMapItem( vgui::Panel *pParent, const char *pPanelName );

	void SetChapterAndMapInfo( int nChapterNumber, int nMapNumber, int nMapIndex, int nUnlockedChapters = -1 );

	bool IsSelected( void ) { return m_bSelected; }
	void SetSelected( bool bSelected ) { m_bSelected = bSelected; }

	bool HasMouseover( void ) { return m_bHasMouseover; }
	void SetHasMouseover( bool bHasMouseover );
	void OnKeyCodePressed( vgui::KeyCode code );

	int GetChapterNumber() { return m_nChapterNumber; }
	int GetMapIndex() { return m_nMapIndex; }
	int GetMapNumber() { return m_nMapNumber; }
	bool IsLocked() { return m_bLocked; }

	virtual void NavigateTo();
	virtual void NavigateFrom();

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PaintBackground();
	virtual void OnCursorEntered();
	virtual void OnCursorExited() { SetHasMouseover( false ); }
	
	

	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnMouseDoublePressed( vgui::MouseCode code );
	void PerformLayout();

protected:
	void DrawListItemLabel( vgui::Label *pLabel );

	CPortalLeaderboardPanel	*m_pLeaderboard;

	GenericPanelList	*m_pListCtrlr;
	vgui::HFont			m_hTextFont;
	vgui::HFont			m_hFriendsListFont;
	vgui::HFont			m_hFriendsListSmallFont;
	vgui::HFont			m_hFriendsListVerySmallFont;
	
	int					m_nChapterNumber;
	int					m_nMapIndex;
	int					m_nMapNumber;
	bool				m_bLocked;

	Color				m_BaseColor;
	Color				m_TextColor;
	Color				m_FocusColor;
	Color				m_DisabledColor;
	Color				m_CursorColor;
	Color				m_LockedColor;
	Color				m_MouseOverCursorColor;

	Color				m_LostFocusColor;

	bool				m_bSelected;
	bool				m_bHasMouseover;

	int					m_nTextOffsetY;
};

//=============================================================================
class CAvatarPanelItem : public CLeaderboardMapItem
{
	DECLARE_CLASS_SIMPLE( CAvatarPanelItem, CLeaderboardMapItem );
public:
	CAvatarPanelItem( vgui::Panel *pParent, const char *pPanelName );
	~CAvatarPanelItem();
	void SetPlayerData( const PortalLeaderboardItem_t *pData, LeaderboardType type );
#if !defined( NO_STEAM )
	void SetPlayerData( CSteamID playerID, int nScore, LeaderboardType type );
#elif defined( _X360 )
	void SetPlayerData( XUID playerID, int nScore, LeaderboardType type, int nController );
#endif

	void OnKeyCodePressed( vgui::KeyCode code );
	int GetAvatarIndex() { return m_nAvatarIndex; }
	void SetAvatarIndex( int nIndex ) { m_nAvatarIndex =  nIndex; }
	virtual bool ActivateSelectedItem();
	void SetBaseColor( Color baseColor ) { m_BaseColor = baseColor; }
	void SetMouseOverCursorColor( Color mouseOverCursorColor ) { m_MouseOverCursorColor = mouseOverCursorColor; }
	void SetScoreLegend( const char *pLegend );
	void SetAsHUDElement( bool bHudElement );
	virtual void NavigateTo();
	virtual void NavigateFrom();

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnCursorMoved( int x, int y );
	virtual void OnCursorExited( void );

private:
	bool IsMouseOnGamerPicture( void );

	CPortalHUDLeaderboard *m_pHUDLeaderboard;
	LeaderboardType m_leaderboardType;
	StatType_t m_statType;
	int	m_nAvatarIndex;

	uint64 m_nSteamID;
	XUID m_nXUID;

	Label *m_pGamerName;
	Label *m_pGamerScore;
	Label *m_pScoreLegend;
	vgui::ImagePanel *m_pGamerAvatar;
};

//=============================================================================
class CPortalLeaderboardPanel : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( CPortalLeaderboardPanel, CBaseModFrame );

public:
	CPortalLeaderboardPanel( vgui::Panel *pParent, const char *pPanelName, bool bSinglePlayer );
	~CPortalLeaderboardPanel();
	int GetCurrentChapterNumber() { return m_nCurrentChapterNumber; }
	int GetCurrentMapIndex() { return m_nMapIndex; }
	void UpdateLeaderboards() { m_pLeaderboard = 0; m_bNeedsUpdate = true; }
	void SetMapIndex( int nMapIndex ) { m_nMapIndex = nMapIndex; }
	virtual void	OnKeyCodePressed( vgui::KeyCode code );
	void SelectPanel( PanelSelection_t selectedPanel, bool bForceItemSelect = false );
	void ClearAvatarSelection();
	void ClearStats();
	void SetPanelSelection( PanelSelection_t selectedPanel );
	bool IsSinglePlayer() { return m_bSinglePlayerMode; }
	LeaderboardState_t GetCurrentLeaderboardState() { return m_leaderboardState; }

	MESSAGE_FUNC( MsgPreChangeLevel, "MsgPreChangeLevel" );
	MESSAGE_FUNC( MsgChangeLevel, "MsgChangeLevel" );
	MESSAGE_FUNC( MsgRetryMap, "MsgRetryMap" );
	MESSAGE_FUNC( MsgGoToNext, "MsgGoToNext" );
	
	static void ResetTempScoreUpdates( void );
	void	UpdateFooter();

	virtual void OnClose();

protected:
	virtual void	Activate();
	virtual void	ApplySchemeSettings( vgui::IScheme* pScheme );
	
	virtual void	OnCommand( const char *pCommand );
	virtual void	OnThink();
	virtual void	OnMousePressed( vgui::MouseCode code );

	virtual void SetDataSettings( KeyValues *pSettings );
	
   void	SetNextMap();
	void SetMapList();
	void SetPanelStats();
	void SetGraphData( CPortalLeaderboardGraphPanel *pGraphPanel, LeaderboardType graphType );
	void UpdateStatPanel();
	const char* GetCurrentMapName();
	int GetIndexOfMap( int nMapNumber );
	void OpenCoopLobby();
	void ReturnToMainMenu();
	void StartSPGame();
	bool StartSelectedMap();
	void ClockSpinner( bool bVisible );

	bool IsMapLocked( int nChapterNumber, int nMapNumber, bool bSinglePlayer );
	bool IsCurrentMapLocked();
	bool IsInHub();

	CPanelAnimationVarAliasType( int, m_nStatHeight, "statHeight", "34", "proportional_int" );

private:

	KeyValues::AutoDelete m_autodelete_pResourceLoadConditions;

	int					m_nCurrentChapterNumber;
	int					m_nMapIndex;

	char				m_szNextMap[ MAX_MAP_NAME ];

	int					m_nUnlockedSPChapters;

	GenericPanelList	*m_pMapList;
	GenericPanelList	*m_pStatList;

	CDialogListButton	*m_pChapterListButton;
	CDialogListButton	*m_pLeaderboardListButton;

	CPortalChallengeStatsPanel *m_pChallengeStatsPanel;

	CPortalLeaderboardGraphPanel *m_pPortalGraph;
	CPortalLeaderboardGraphPanel *m_pTimeGraph;

	Label				*m_pInvalidLabel;
	Label				*m_pInvalidLabel2;
	ImagePanel			*m_pWorkingAnim;

	Label				*m_pEveryoneLabel;

	bool				m_bNeedsUpdate;
	bool				m_bNeedsMapItemSelect;

	bool				m_bMapPanelSelected;

	LeaderboardState_t m_leaderboardState;
	bool			   m_bSinglePlayerMode;

	PanelSelection_t m_currentSelection;

	// the current leaderboard data
	CPortalLeaderboard* m_pLeaderboard;
	LeaderboardType m_CurrentLeaderboardType;

	bool m_bCheated;
	bool m_bOnline;
	bool m_bCommittedAction;
};


void AddAvatarPanelItem( CPortalLeaderboard *pLeaderboard, BaseModUI::GenericPanelList *pStatLists, const PortalLeaderboardItem_t *pData, int nScore, LeaderboardType nType, int nPlayerType, int nAvatarIndex, int nHeight, int nSlot, bool bHUDElement = false );

};

#endif // __VPORTALLEADERBOARD_H__
