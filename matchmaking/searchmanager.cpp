//========= Copyright � 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef _X360
#include "xbox/xboxstubs.h"
#endif

#include "mm_framework.h"
#include "match_searcher.h"

#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
#include "steam/matchmakingtypes.h"
#endif

#include "proto_oob.h"
#include "fmtstr.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#pragma warning (disable : 4355 )

static ConVar mm_match_search_update_interval( "mm_match_search_update_interval", "10", FCVAR_DEVELOPMENTONLY, "Interval between matchsearcher updates." );

//
// Match searching overrides
//

class CMatchSearcher_SearchMgr : public CMatchSearcher
{
public:
	CMatchSearcher_SearchMgr( CSearchManager *pMgr, KeyValues *pSettings );

public:
	virtual void StartSearchPass( KeyValues *pSearchPass );
	virtual void OnSearchEvent( KeyValues *pNotify );
	virtual void OnSearchDone();

protected:
	CSearchManager *m_pMgr;
};

class CMatchSearchResultItem : public IMatchSearchResult
{
public:
	explicit CMatchSearchResultItem( XUID xuid, KeyValues *pDetails );
	~CMatchSearchResultItem();

public:
	virtual XUID GetOnlineId() { return m_xuid; }
	virtual KeyValues * GetGameDetails() { return m_pDetails; }
	virtual bool IsJoinable() { return m_pDetails != NULL; }
	virtual void Join();

protected:
	XUID m_xuid;
	KeyValues *m_pDetails;
};


//
// Pool of search managers
//

typedef CUtlVector< CSearchManager * > SearchManagerPool;

static SearchManagerPool & GetSearchManagerPool()
{
	static SearchManagerPool s_smp;
	return s_smp;
}

void CSearchManager::UpdateAll()
{
	SearchManagerPool &smp = GetSearchManagerPool();
	for ( int k = 0; k < smp.Count(); )
	{
		CSearchManager *pUpdate = smp[k];
		pUpdate->Update();
		
		if ( smp.IsValidIndex( k ) && smp[k] == pUpdate )
			++ k;
	}
}

//
// Search manager implementation
//

CSearchManager::CSearchManager( KeyValues *pSearchParams ) :
	m_pSettings( pSearchParams ? pSearchParams->MakeCopy() : NULL ),
	m_pSearcher( NULL ),
	m_eState( STATE_IDLE )
{
	GetSearchManagerPool().AddToTail( this );
	
	DevMsg( "Created CSearchManager(%p):\n", this );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );
}

CSearchManager::~CSearchManager()
{
	GetSearchManagerPool().FindAndRemove( this );

	DevMsg( "Destroying CSearchManager(%p):\n", this );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );

	if ( m_pSearcher )
		m_pSearcher->Destroy();
	m_pSearcher = NULL;

	if ( m_pSettings )
		m_pSettings->deleteThis();
	m_pSettings = NULL;

	ClearResults( m_arrResults );
}

void CSearchManager::ClearResults( CUtlVector< IMatchSearchResult * > &arr )
{
	for ( int k = 0; k < arr.Count(); ++ k )
	{
		IMatchSearchResult *p = arr[k];
		CMatchSearchResultItem *pItem = ( CMatchSearchResultItem * ) p;
		delete pItem;
	}
	arr.RemoveAll();
}

void CSearchManager::EnableResultsUpdate( bool bEnable, KeyValues *pSearchParams /* = NULL */ )
{
	if ( bEnable && pSearchParams )
	{
		if ( m_pSettings )
			m_pSettings->deleteThis();

		m_pSettings = pSearchParams->MakeCopy();
	}
	
	if ( !bEnable && m_pSettings )
	{
		m_pSettings->deleteThis();
	}

	switch ( m_eState )
	{
	case STATE_PAUSED:
		m_eState = STATE_IDLE;
		break;

	case STATE_SEARCHING:
		m_eState = STATE_SEARCHING_CRITERIA_UPDATED;
		break;
	}
}

int CSearchManager::GetNumResults()
{
	return m_arrResults.Count();
}

IMatchSearchResult * CSearchManager::GetResultByIndex( int iResultIdx )
{
	return m_arrResults.IsValidIndex( iResultIdx ) ? m_arrResults[ iResultIdx ] : NULL;
}

IMatchSearchResult * CSearchManager::GetResultByOnlineId( XUID xuidResultOnline )
{
	return GetResultById( m_arrResults, xuidResultOnline );
}

IMatchSearchResult * CSearchManager::GetResultById( CUtlVector< IMatchSearchResult * > &arr, XUID id )
{
	for ( int k = 0; k < arr.Count(); ++ k )
	{
		IMatchSearchResult *pResult = arr[k];
		if ( pResult && ( pResult->GetOnlineId() == id ) )
			return pResult;
	}
	return NULL;
}

void CSearchManager::Destroy()
{
	if ( m_pSearcher )
		m_pSearcher->Destroy();
	m_pSearcher = NULL;

	delete this;
}

void CSearchManager::OnEvent( KeyValues *pEvent )
{
}

void CSearchManager::Update()
{
	switch ( m_eState )
	{
	case STATE_IDLE:
		if ( m_pSettings )
		{
			m_eState = STATE_SEARCHING;
			
			if( m_pSearcher )
				m_pSearcher->Destroy();
			m_pSearcher = new CMatchSearcher_SearchMgr( this, m_pSettings->MakeCopy() );
		}
		break;

	case STATE_SEARCHING:
	case STATE_SEARCHING_CRITERIA_UPDATED:
		Assert( m_pSearcher );
		m_pSearcher->Update();
		break;

	case STATE_PAUSED:
		if ( Plat_FloatTime() > m_flNextSearchTime &&
			!IsLocalClientConnectedToServer() )
		{
			// Trigger another search
			m_eState = STATE_IDLE;
		}
		break;
	}
}

void CSearchManager::OnSearchDone()
{
	if ( m_eState == STATE_SEARCHING_CRITERIA_UPDATED )
	{
		m_eState = STATE_IDLE;
	}
	else
	{
		m_eState = STATE_PAUSED;
		m_flNextSearchTime = Plat_FloatTime() + mm_match_search_update_interval.GetFloat();
	}

	// Prepare the new results
	IMatchTitleGameSettingsMgr *mgr = g_pMMF->GetMatchTitleGameSettingsMgr();
	CUtlVector< IMatchSearchResult * > arrResults;

	// Now traverse the results we received and roll them up
	for ( int k = 0, kNum = m_pSearcher->GetNumSearchResults(); k < kNum; ++ k )
	{
		CMatchSearcher::SearchResult_t const &sr = m_pSearcher->GetSearchResult( k );

		KeyValues *pDetails = sr.GetGameDetails();
		if ( !pDetails )
			continue;

		// Determine the rollup key
		KeyValues *pRollupKey = mgr->RollupGameDetails( pDetails, NULL, m_pSearcher->GetSearchSettings() );
		KeyValues::AutoDelete autodelete_pRollupKey( pRollupKey );
		if ( !pRollupKey )
			continue;

		// Find if we already have the rollup information for it
		XUID uidKey = pRollupKey->GetUint64( "rollupkey", 0ull );
		if ( !uidKey )
			continue;

		IMatchSearchResult *pResult = GetResultById( arrResults, uidKey );
		if ( !pResult )
		{
			pResult = new CMatchSearchResultItem( uidKey, pRollupKey );
			arrResults.AddToTail( pResult );
		}

		// Roll up these details into an already existing or newly created rollup
		mgr->RollupGameDetails( pDetails, pResult->GetGameDetails(), m_pSearcher->GetSearchSettings() );
	}

	// Now after all the rollup is finished, process ones that have not had any matching details
	for ( int k = 0; k < m_arrResults.Count(); ++ k )
	{
		IMatchSearchResult *pOldResult = m_arrResults[k];
		XUID uidKey = pOldResult->GetOnlineId();
		if ( GetResultById( arrResults, uidKey ) )
			continue; // have fresh data

		if ( !mgr->RollupGameDetails( NULL, pOldResult->GetGameDetails(), m_pSearcher->GetSearchSettings() ) )
			continue; // cannot keep this result

		arrResults.AddToTail( pOldResult );
		m_arrResults.FastRemove( k -- );
	}

	// Remove all old results, swap for new results and broadcast a notification
	ClearResults( m_arrResults );
	m_arrResults.Swap( arrResults );
}



//
// MatchSearcher override
//

CMatchSearcher_SearchMgr::CMatchSearcher_SearchMgr( CSearchManager *pMgr, KeyValues *pSettings ) :
	CMatchSearcher( pSettings ),
	m_pMgr( pMgr )
{
	// Let the title extend the game settings
	g_pMMF->GetMatchTitleGameSettingsMgr()->InitializeGameSettings( m_pSettings, "search_rollup" );

	DevMsg( "CMatchSearcher_SearchMgr title adjusted settings:\n" );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );

	// Signal that we are starting a search
	KeyValues *pNotify = new KeyValues( "OnMatchSearchMgrUpdate", "update", "searchstarted" );
	pNotify->SetPtr( "mgr", m_pMgr );
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( pNotify );
}

void CMatchSearcher_SearchMgr::StartSearchPass( KeyValues *pSearchPass )
{
	CMatchSearcher::StartSearchPass( pSearchPass );
}

void CMatchSearcher_SearchMgr::OnSearchEvent( KeyValues *pNotify )
{
	// Swallow events
	pNotify->deleteThis();
}

void CMatchSearcher_SearchMgr::OnSearchDone()
{
	CMatchSearcher::OnSearchDone();
	m_pMgr->OnSearchDone();

	// Signal that we are finished with search
	KeyValues *pNotify = new KeyValues( "OnMatchSearchMgrUpdate", "update", "searchfinished" );
	pNotify->SetPtr( "mgr", m_pMgr );
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( pNotify );
}


//
// CMatchSearchResultItem implementation
//

CMatchSearchResultItem::CMatchSearchResultItem( XUID xuid, KeyValues *pDetails ) :
	m_xuid( xuid ),
	m_pDetails( pDetails ? pDetails->MakeCopy() : NULL )
{
}

CMatchSearchResultItem::~CMatchSearchResultItem()
{
	if ( m_pDetails )
		m_pDetails->deleteThis();
	m_pDetails = NULL;
}

void CMatchSearchResultItem::Join()
{
	if ( !m_pDetails )
		return;

	// Detach details
	KeyValues *pSettings = m_pDetails;
	m_pDetails = NULL;

	// Trigger matchmaking
	g_pMatchFramework->MatchSession( pSettings );

	// Delete settings
	pSettings->deleteThis();
}

