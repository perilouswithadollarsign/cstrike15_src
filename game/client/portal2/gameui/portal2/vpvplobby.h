//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VPVPLOBBY_H__
#define __VPVPLOBBY_H__

#include "basemodui.h"
#include "matchmaking/imatchframework.h"

namespace BaseModUI {

class GenericPanelList;
class BaseModHybridButton;

class PvpLobby : public CBaseModFrame, public IMatchEventsSink
{
	DECLARE_CLASS_SIMPLE( PvpLobby, CBaseModFrame );

public:
	PvpLobby(vgui::Panel *parent, const char *panelName, KeyValues *pSettings );
	~PvpLobby();

	// IMatchEventsSink implementation
public:
	virtual void OnEvent( KeyValues *pEvent );

public:
	virtual void OnThink();

	void OnCommand( const char *command );
	virtual void NavigateToChild( Panel *pNavigateTo );

	void OnKeyCodePressed( vgui::KeyCode code );

	void OnOpen();
	void OnClose();
	void QuickMatch();
	void ActivateFriendsList( int iDirection );
	void AllowFriendsAccess();

	MESSAGE_FUNC_PARAMS( MsgNoLongerHosting, "NoLongerHosting", pSettings );
	MESSAGE_FUNC_PARAMS( MsgNoValidMissionChapter, "NoValidMissionChapter", pSettings );
	MESSAGE_FUNC( MsgChangeGameSettings, "ChangeGameSettings" );
	MESSAGE_FUNC_CHARPTR( OnItemSelected, "OnItemSelected", panelName );

#ifndef NO_STEAM
	STEAM_CALLBACK( PvpLobby, Steam_OnPersonaStateChanged, PersonaStateChange_t, m_CallbackPersonaStateChanged );
#endif

protected:
	virtual void ApplySchemeSettings( vgui::IScheme* pScheme );

	// void NavigateTo_ChatHandler( Panel *pNavigateTo ); //special functionality for navigating the menu and chatting at the same time

	void ClockSpinner();
	void UpdateFooterButtons();
	void SetStateText( const char *pText, const wchar_t *pFormattedText );
	void SetLobbyLeaderText();

	void LeaveLobby();

	void ApplyUpdatedSettings( KeyValues *kvUpdate );
	void Activate();

	void Player_AddOrUpdate( KeyValues *pPlayer );
	void Player_Remove( XUID xuid );

	vgui::Label * GetSettingsSummaryLabel( char const *szContentType );

	bool UpdateFriendsList();
	bool SweepAndAddNewFriends();
	void AddFriendsToList();
	bool AddFriendFromDetails( const void *pfi );
	void SortListItems();

private:
	void SetupLobbySettings( KeyValues *pSettings );

	KeyValues *m_pSettings;
	KeyValues *m_pLobbyDetailsLayout;
	KeyValues::AutoDelete m_autodelete_pLobbyDetailsLayout;
	KeyValues::AutoDelete m_autodelete_pResourceLoadConditions;
	bool	m_bNoCommandHandling;
	bool	m_bSubscribedForEvents;

	char	m_StateText[256];
	wchar_t	m_FormattedStateText[256];

	int m_nMsgBoxId;
	XUID m_xuidPlayerFlyout;

	int m_nRefreshListCap;
	float m_flLastRefreshTime, m_flAutoRefreshTime;
	float m_flSearchStartedTime, m_flSearchEndTime;

	CPanelAnimationVarAliasType( int, m_iTitleXOffset, "title_xpos", "0", "proportional_int" )

	GenericPanelList* m_pListFriends;
	GenericPanelList* m_pListFriendsArray[2];

	enum AddFriendsRule_t
	{
		ADD_FRIENDS_ALL = -1,
		ADD_FRIENDS_PSN = 0,
		ADD_FRIENDS_STEAM = 1,
	};
	AddFriendsRule_t m_eAddFriendsRule;

	bool m_bAnimatingLists;
	float m_flAnimationTimeStamp;
	int m_nAnimatingTargetWidth[2];
	int m_nAnimatingSourceWidth[2];
};


}

#endif // __VPVPLOBBY_H__
