//===== Copyright � 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef MATCH_SEARCHER_H
#define MATCH_SEARCHER_H
#ifdef _WIN32
#pragma once
#endif

#if !defined( NO_STEAM )
#include "steam/steam_api.h"
#endif // _X360

//
// CMatchSearcher
//

class CMatchSearcher
#ifdef _X360
	: public IDormantOperation
#endif
#if !defined (NO_STEAM)
	: public IMatchAsyncOperationCallback
#endif
{
public:
	explicit CMatchSearcher( KeyValues *pSettings );
	virtual ~CMatchSearcher();

public:
	// Run a frame update
	virtual void Update();

	// Destroy the object
	virtual void Destroy();

	//
	// Overrides
	//
public:
	// Obtain adjusted match search settings
	virtual KeyValues * GetSearchSettings();

	// Event broadcasting
	virtual void OnSearchEvent( KeyValues *pNotify );

protected:

#ifdef _X360

public:
	struct SearchResult_t
	{
		inline XNKID GetXNKID() const { return m_info.sessionID; }
		KeyValues * GetGameDetails() const { return m_pGameDetails; }

		XSESSION_INFO m_info;
		KeyValues *m_pGameDetails;
	};

protected:
	CUtlVector< XUSER_CONTEXT > m_arrContexts;
	CUtlVector< XUSER_PROPERTY > m_arrProperties;

	float m_flQosTimeout;
	XNQOS *m_pQosResults;

	CUtlBuffer m_bufSearchResultHeader;
	XSESSION_SEARCHRESULT_HEADER * GetXSearchResult() { return ( XSESSION_SEARCHRESULT_HEADER * ) m_bufSearchResultHeader.Base(); }
	XOVERLAPPED m_xOverlapped;
	CJob *m_pCancelOverlappedJob;

	void Live_OnSessionSearchCompleted();

	void Live_CheckSearchResultsQos();
	void Live_OnQosCheckCompleted();

	virtual bool UpdateDormantOperation();

#elif !defined( NO_STEAM )

public:
	struct SearchResult_t
	{
		inline XNKID GetXNKID() const { return ( const XNKID & ) m_uiLobbyId; }
		KeyValues * GetGameDetails() const;

		uint64 m_uiLobbyId;
		mutable KeyValues *m_pGameDetails;
		netadr_t m_svAdr;
		int m_svPing;
		int m_numPlayers;
		IMatchAsyncOperation *m_pAsyncOperationPingWeakRef;
	};

protected:
	CCallResult< CMatchSearcher, LobbyMatchList_t > m_CallbackOnLobbyMatchListReceived;
	void Steam_OnLobbyMatchListReceived( LobbyMatchList_t *p, bool bError );

#else

public:
	struct SearchResult_t
	{
		inline XNKID GetXNKID() const { return ( const XNKID & ) m_uiLobbyId; }
		KeyValues * GetGameDetails() const { return m_pGameDetails; }

		uint64 m_uiLobbyId;
		KeyValues *m_pGameDetails;
	};

#endif

protected:
	KeyValues *m_pSettings;
	KeyValues::AutoDelete m_autodelete_pSettings;

	KeyValues *m_pSessionSearchTree;
	KeyValues::AutoDelete m_autodelete_pSessionSearchTree;

	KeyValues *m_pSearchPass;
	
#if !defined( NO_STEAM )
	
	uint32 m_uiQosTimeoutStartMS;
	uint32 m_uiQosPingLastMS;
	CUtlVector< IMatchAsyncOperation * > m_arrOutstandingAsyncOperation;

	STEAM_CALLBACK_MANUAL( CMatchSearcher, Steam_OnLobbyDataReceived, LobbyDataUpdate_t, m_CallbackOnLobbyDataReceived );

	// Callback for server reservation check
	virtual void OnOperationFinished( IMatchAsyncOperation *pOperation );

#endif

	enum State_t
	{
		STATE_INIT,
		STATE_SEARCHING,
#ifdef _X360
		STATE_CHECK_QOS,
#endif

#if !defined( NO_STEAM )
		STATE_WAITING_LOBBY_DATA_AND_PING,
#endif

		STATE_DONE
	};
	State_t m_eState;

	CUtlVector< SearchResult_t > m_arrSearchResults;
	CUtlVector< SearchResult_t > m_arrSearchResultsAggregate;

protected:
	void InitializeSettings();
	
	void StartSearch();
	virtual void StartSearchPass( KeyValues *pSearchPass );
	void AggregateSearchPassResults();
	
	virtual void OnSearchPassDone( KeyValues *pSearchPass );
	virtual void OnSearchDone();

	//
	// Results retrieval overrides
	//
public:
	virtual bool IsSearchFinished() const;
	virtual int GetNumSearchResults() const;
	virtual SearchResult_t const & GetSearchResult( int idx ) const;
};


#endif // MATCH_SEARCHER_H

