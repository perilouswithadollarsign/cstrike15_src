//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef MM_SESSION_ONLINE_TEAM_SEARCH_H
#define MM_SESSION_ONLINE_TEAM_SEARCH_H
#ifdef _WIN32
#pragma once
#endif


class CMatchSessionOnlineTeamSearch;
class CMatchSessionOnlineTeamSearchLinkHost;
class CMatchSessionOnlineTeamSearchLinkClient;

//
// CMatchSessionOnlineTeamSearch
//
// Implementation of an online team session search (aka team-on-team matchmaking)
//

class CMatchSessionOnlineTeamSearch : public CMatchSessionOnlineSearch
{
public:
	explicit CMatchSessionOnlineTeamSearch( KeyValues *pSettings, CMatchSessionOnlineHost *pHost );
	virtual ~CMatchSessionOnlineTeamSearch();

	//
	// Overrides when search is used as a nested object
	//
protected:
	virtual CMatchSearcher * OnStartSearching();
	virtual void OnSearchCompletedSuccess( CSysSessionClient *pSysSession, KeyValues *pSettings );
	virtual void OnSearchCompletedEmpty( KeyValues *pSettings );
	virtual void OnSearchEvent( KeyValues *pNotify );
	virtual CSysSessionClient * OnBeginJoiningSearchResult();

	// Hooks for the nested sys sessions
public:
	virtual void OnSessionEvent( KeyValues *pNotify );
	CSysSessionBase * LinkSysSession();

	//
	// Match session overrides
	//
public:
	// Run a frame update
	virtual void Update();

	// Destroy the session object
	virtual void Destroy();

	// Debug print a session object
	virtual void DebugPrint();

	// Process event
	virtual void OnEvent( KeyValues *pEvent );

protected:
	void OnRunSessionCommand( KeyValues *pCommand );

	CMatchSessionOnlineTeamSearchLinkHost & LinkHost();
	CMatchSessionOnlineTeamSearchLinkClient & LinkClient();

	void ResetAndRestartTeamSearch();

	void RememberHostSessionUpdatePacket( KeyValues *pPacket );
	void ApplyHostSessionUpdatePacket();

protected:
	enum State_t
	{
		STATE_SEARCHING,
		STATE_CREATING,
		STATE_AWAITING_PEER,
		STATE_LINK_HOST,
		STATE_LINK_CLIENT,
		STATE_ERROR,
	};
	State_t m_eState;
	int m_iLinkState;
	XUID m_xuidLinkPeer;

	CMatchSessionOnlineHost *m_pHostSession;	// parent object that contains our TeamSearch
	
	CSysSessionHost *m_pSysSessionHost;			// nested SysSessionHost object when we are hosting
	CSysSessionClient *m_pSysSessionClient;		// nested SysSessionClient object when we are peer
	CDsSearcher *m_pDsSearcher;					// for searching dedicated servers
	float m_flActionTime; // time to perform action of the current state

	KeyValues *m_pUpdateHostSessionPacket;		// packet that should be applied to host session before the game commences
												// this packet is the case of team sessions being reconciled and unknown settings resolved
	KeyValues::AutoDelete m_autodelete_pUpdateHostSessionPacket;

	// LINK HOST STATE
	float m_flCreationTime;
#ifdef _X360
	CXlspConnection *m_pXlspConnection;
	CXlspConnectionCmdBatch *m_pXlspCommandBatch;
#endif
};

class CMatchSessionOnlineTeamSearchLinkBase : public CMatchSessionOnlineTeamSearch
{
private:
	CMatchSessionOnlineTeamSearchLinkBase();

public:
	enum State_t
	{
		STATE_HOSTING_LISTEN_SERVER,
		STATE_WAITING_FOR_PEER_SERVER,
		STATE_SEARCHING_DEDICATED,
		STATE_LINK_FINISHED,
		STATE_LINK_BASE_LAST
	};

protected:
	void StartHostingListenServer();
	void StartDedicatedServerSearch();
	void StartWaitingForPeerServer();

	void OnDedicatedSearchFinished();

public:
	void LinkCommand( KeyValues *pCommand );
	void LinkUpdate();
};

class CMatchSessionOnlineTeamSearchLinkHost : public CMatchSessionOnlineTeamSearchLinkBase
{
private:
	CMatchSessionOnlineTeamSearchLinkHost();

public:
	enum State_t
	{
		STATE_LINK_INITIAL = STATE_LINK_BASE_LAST,
		STATE_SUBMIT_STATS,
		STATE_REPORTING_STATS,
		STATE_CONFIRM_JOIN,
		STATE_CONFIRM_JOIN_WAIT,
	};

public:
	void LinkInit();
	void LinkCommand( KeyValues *pCommand );
	void LinkUpdate();
};

class CMatchSessionOnlineTeamSearchLinkClient : public CMatchSessionOnlineTeamSearchLinkBase
{
private:
	CMatchSessionOnlineTeamSearchLinkClient();

public:
	enum State_t
	{
		STATE_LINK_INITIAL = STATE_LINK_BASE_LAST,
		STATE_WAITING_FOR_HOST_READY,
		STATE_CONFIRM_JOIN,
	};

public:
	void LinkInit();
	void LinkCommand( KeyValues *pCommand );
	void LinkUpdate();
};

class CMatchSearcher_OnlineTeamSearch : public CMatchSearcher_OnlineSearch
{
public:
	CMatchSearcher_OnlineTeamSearch( CMatchSessionOnlineTeamSearch *pSession, KeyValues *pSettings );

protected:
	virtual void StartSearchPass( KeyValues *pSearchPass );
};

#endif
