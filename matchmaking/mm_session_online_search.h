//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef MM_SESSION_ONLINE_SEARCH_H
#define MM_SESSION_ONLINE_SEARCH_H
#ifdef _WIN32
#pragma once
#endif

//
// CMatchSessionOnlineSearch
//
// Implementation of an online session search (aka matchmaking)
//

class CMatchSearcher_OnlineSearch;

class CMatchSessionOnlineSearch : public IMatchSessionInternal
{
	// Methods of IMatchSession
public:
	// Get an internal pointer to session system-specific data
	virtual KeyValues * GetSessionSystemData() { return NULL; }
	
	// Get an internal pointer to session settings
	virtual KeyValues * GetSessionSettings();

	// Update session settings, only changing keys and values need
	// to be passed and they will be updated
	virtual void UpdateSessionSettings( KeyValues *pSettings );

	virtual void UpdateTeamProperties( KeyValues *pTeamProperties );

	// Issue a session command
	virtual void Command( KeyValues *pCommand );

	virtual uint64 GetSessionID();

	// Run a frame update
	virtual void Update();

	// Destroy the session object
	virtual void Destroy();

	// Debug print a session object
	virtual void DebugPrint();

	// Check if another session is joinable
	virtual bool IsAnotherSessionJoinable( char const *pszAnotherSessionInfo ) { return true; }

	// Process event
	virtual void OnEvent( KeyValues *pEvent );

	enum Result
	{
		RESULT_UNDEFINED,
		RESULT_SUCCESS,
		RESULT_FAIL
	};

	Result GetResult() { return m_result; }

	
public:
	explicit CMatchSessionOnlineSearch( KeyValues *pSettings );
	virtual ~CMatchSessionOnlineSearch();

protected:
	CMatchSessionOnlineSearch();	// for derived classes construction

	//
	// Overrides when search is used as a nested object
	//
protected:
	virtual CMatchSearcher *OnStartSearching();
	virtual void OnSearchCompletedEmpty( KeyValues *pSettings );
	virtual void OnSearchCompletedSuccess( CSysSessionClient *pSysSession, KeyValues *pSettings );
	virtual void OnSearchEvent( KeyValues *pNotify );
	virtual CSysSessionClient * OnBeginJoiningSearchResult();

protected:
	void StartJoinNextFoundSession();
	void ValidateSearchResultWhitelist();
	void ConnectJoinLobbyNextFoundSession();
	void OnSearchDoneNoResultsMatch();

protected:
	KeyValues *m_pSettings;
	KeyValues::AutoDelete m_autodelete_pSettings;

	friend class CMatchSearcher_OnlineSearch;
	CMatchSearcher *m_pMatchSearcher;

	Result m_result;

	enum State_t
	{
		STATE_INIT,
		STATE_SEARCHING,
		STATE_JOIN_NEXT,
#if !defined( NO_STEAM )
		STATE_VALIDATING_WHITELIST,
#endif
		STATE_JOINING,
		STATE_CLOSING,		
	};
	State_t m_eState;

	CUtlVector< CMatchSearcher::SearchResult_t const * > m_arrSearchResults;

	CSysSessionClient *m_pSysSession;
	CSysSessionConTeamHost *m_pSysSessionConTeam;

#if !defined( NO_STEAM )
	void SetupSteamRankingConfiguration();
	bool IsSteamRankingConfigured() const;

	class CServerListListener : public ISteamMatchmakingServerListResponse
	{
	public:
		explicit CServerListListener( CMatchSessionOnlineSearch *pDsSearcher, CUtlVector< MatchMakingKeyValuePair_t > &filters );
		void Destroy();

	public:
		// Server has responded ok with updated data
		virtual void ServerResponded( HServerListRequest hReq, int iServer ) { HandleServerResponse( hReq, iServer, true ); }
		// Server has failed to respond
		virtual void ServerFailedToRespond( HServerListRequest hReq, int iServer )  { HandleServerResponse( hReq, iServer, false ); }
		// A list refresh you had initiated is now 100% completed
		virtual void RefreshComplete( HServerListRequest hReq, EMatchMakingServerResponse response );

	protected:
		void HandleServerResponse( HServerListRequest hReq, int iServer, bool bResponded );
		CMatchSessionOnlineSearch *m_pOuter;
		HServerListRequest m_hRequest;
	};
	friend class CServerListListener;
	CServerListListener *m_pServerListListener;
	void Steam_OnDedicatedServerListFetched();
#endif

	float m_flInitializeTimestamp;
};

class CMatchSearcher_OnlineSearch : public CMatchSearcher
{
public:
	CMatchSearcher_OnlineSearch( CMatchSessionOnlineSearch *pSession, KeyValues *pSettings );

public:
	virtual void OnSearchEvent( KeyValues *pNotify );
	virtual void OnSearchDone();

protected:
	CMatchSessionOnlineSearch *m_pSession;
};

#endif
