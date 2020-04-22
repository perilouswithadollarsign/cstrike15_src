//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "mm_framework.h"

#include "leaderboards.h"

#include "fmtstr.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

//
// Definition of leaderboard request queue class
//

class CLeaderboardRequestQueue : public ILeaderboardRequestQueue
{
public:
	CLeaderboardRequestQueue();
	~CLeaderboardRequestQueue();

	// ILeaderboardRequestQueue
public:
	virtual void Request( KeyValues *pRequest );
	virtual void Update();

public:
	KeyValues * GetFinishedRequest();

protected:
	CUtlVector< KeyValues * > m_arrRequests;
	bool m_bQueryRunning;
	KeyValues *m_pFinishedRequest;

	void OnStartNewQuery();
	void OnSubmitQuery();
	void OnQueryFinished();
	void Cleanup();

protected:
#ifdef _X360

	// Leaderboard query data
	CUtlVector< XUID > m_arrXuids;
	CUtlVector< XUSER_STATS_SPEC > m_arrSpecs;
	CUtlBuffer m_bufResults;
	XOVERLAPPED m_xOverlapped;

	void ProcessResults( XUSER_STATS_READ_RESULTS const *pResults );

	// Leaderboard description data
	CUtlVector< KeyValues * > m_arrViewDescriptions;

	KeyValues * FindViewDescription( DWORD dwViewId );

#elif !defined( NO_STEAM )

	KeyValues *m_pViewDescription;

	CCallResult< CLeaderboardRequestQueue, LeaderboardFindResult_t > m_CallbackOnLeaderboardFindResult;
	void Steam_OnLeaderboardFindResult( LeaderboardFindResult_t *p, bool bError );

	CCallResult< CLeaderboardRequestQueue, LeaderboardScoresDownloaded_t > m_CallbackOnLeaderboardScoresDownloaded;
	void Steam_OnLeaderboardScoresDownloaded( LeaderboardScoresDownloaded_t *p, bool bError );

	void ProcessResults( LeaderboardEntry_t const &lbe );

#endif
};
CLeaderboardRequestQueue g_LeaderboardRequestQueue;
ILeaderboardRequestQueue *g_pLeaderboardRequestQueue = &g_LeaderboardRequestQueue;




//
// Implementation of leaderboard request queue class
//

CLeaderboardRequestQueue::CLeaderboardRequestQueue() :
	m_bQueryRunning( false ),
#if !defined( _X360 ) && !defined( NO_STEAM )
	m_pViewDescription( NULL ),
#endif
	m_pFinishedRequest( NULL )
{
}

CLeaderboardRequestQueue::~CLeaderboardRequestQueue()
{
	Cleanup();
}

void CLeaderboardRequestQueue::Request( KeyValues *pRequest )
{
	if ( !pRequest )
		return;

	DevMsg( "CLeaderboardRequestQueue::Request\n" );
	KeyValuesDumpAsDevMsg( pRequest, 1 );

	m_arrRequests.AddToTail( pRequest->MakeCopy() );
}

void CLeaderboardRequestQueue::Update()
{
	if ( m_bQueryRunning )
	{
#ifdef _X360
		if ( XHasOverlappedIoCompleted( &m_xOverlapped ) )
		{
			OnQueryFinished();
		}
#endif
	}
	else if ( m_arrRequests.Count() )
	{
		OnStartNewQuery();
	}
	else if ( m_arrRequests.NumAllocated() )
	{
		Cleanup();
	}
}

KeyValues * CLeaderboardRequestQueue::GetFinishedRequest()
{
	return m_pFinishedRequest;
}

void CLeaderboardRequestQueue::OnQueryFinished()
{
#ifdef _X360
	// Submit results for processing
	XUSER_STATS_READ_RESULTS const *pResults = ( XUSER_STATS_READ_RESULTS const * ) m_bufResults.Base();
	if ( pResults && pResults->dwNumViews > 0 && pResults->pViews )
		ProcessResults( pResults );
#endif

	// Query is no longer running
	m_bQueryRunning = false;

	DevMsg( "CLeaderboardRequestQueue::OnQueryFinished\n" );
	KeyValuesDumpAsDevMsg( m_pFinishedRequest, 1 );

	// Stuff the data into the players
#ifdef _X360
	for ( int iCtrlr = 0; iCtrlr < XUSER_MAX_COUNT; ++ iCtrlr )
	{
		XUID xuid = 0;
		XUSER_SIGNIN_INFO xsi;
		if ( XUserGetSigninState( iCtrlr ) != eXUserSigninState_NotSignedIn &&
			ERROR_SUCCESS == XUserGetSigninInfo( iCtrlr, XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY, &xsi ) &&
			!(xsi.dwInfoFlags & XUSER_INFO_FLAG_GUEST) )
			xuid = xsi.xuid;
#else
	int iCtrlr = XBX_GetPrimaryUserId();
	{
		XUID xuid = g_pPlayerManager->GetLocalPlayer( iCtrlr )->GetXUID();
#endif

		KeyValues *pUserViews = NULL;
		if ( xuid )
			pUserViews = m_pFinishedRequest->FindKey( CFmtStr( "%llx", xuid ) );

		if ( pUserViews )
		{
			IPlayerLocal *pPlayer = g_pPlayerManager->GetLocalPlayer( iCtrlr );
			if ( pPlayer )
				(( PlayerLocal * ) pPlayer)->OnLeaderboardRequestFinished( pUserViews );
		}

	}
}

void CLeaderboardRequestQueue::Cleanup()
{
#ifdef _X360
	// Clear view descriptions
	while ( m_arrViewDescriptions.Count() )
	{
		m_arrViewDescriptions.Head()->deleteThis();
		m_arrViewDescriptions.FastRemove( 0 );
	}

	m_arrXuids.Purge();
	m_arrSpecs.Purge();
	m_bufResults.Purge();
	m_arrViewDescriptions.Purge();
#elif !defined( NO_STEAM )
	if ( m_pViewDescription )
		m_pViewDescription->deleteThis();
	m_pViewDescription = NULL;
#endif

	// Clear requests
	while ( m_arrRequests.Count() )
	{
		m_arrRequests.Head()->deleteThis();
		m_arrRequests.FastRemove( 0 );
	}

	m_arrRequests.Purge();

	// Clear finished result
	if ( m_pFinishedRequest )
		m_pFinishedRequest->deleteThis();
	m_pFinishedRequest = NULL;
}

void CLeaderboardRequestQueue::OnStartNewQuery()
{
	// When we are starting a new query we need to get rid of the old finished result
	if ( m_pFinishedRequest )
		m_pFinishedRequest->deleteThis();
	m_pFinishedRequest = NULL;

#if !defined( NO_STEAM ) && !defined( SWDS )
	extern CInterlockedInt g_numSteamLeaderboardWriters;
	if ( g_numSteamLeaderboardWriters )
		return; // yield to writers that can alter the leaderboard
#endif

	DevMsg( "CLeaderboardRequestQueue::OnStartNewQuery preparing request...\n" );

#ifdef _X360
	// Prepare the XUIDs first
	m_arrXuids.RemoveAll();
	for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
	{
		int iCtrlr = XBX_GetUserId( k );
		XUSER_SIGNIN_INFO xsi;
		if ( XUserGetSigninState( iCtrlr ) != eXUserSigninState_NotSignedIn &&
			ERROR_SUCCESS == XUserGetSigninInfo( iCtrlr, XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY, &xsi ) &&
			!(xsi.dwInfoFlags & XUSER_INFO_FLAG_GUEST) )
		{
			m_arrXuids.AddToTail( xsi.xuid );
			DevMsg( "    XUID: %s (%llx)\n", xsi.szUserName, xsi.xuid );
		}
	}

	// Clear view descriptions
	while ( m_arrViewDescriptions.Count() )
	{
		m_arrViewDescriptions.Head()->deleteThis();
		m_arrViewDescriptions.FastRemove( 0 );
	}

	// Prepare the spec
	m_arrSpecs.RemoveAll();
	for ( int q = 0; q < m_arrRequests.Count(); ++ q )
	{
		KeyValues *pRequest = m_arrRequests[q];
		KeyValues::AutoDelete autodelete_pRequest( pRequest );
		m_arrRequests.Remove( q -- );

		char const *szViewName = pRequest->GetName();

		KeyValues *pDescription = g_pMMF->GetMatchTitle()->DescribeTitleLeaderboard( szViewName );
		if ( !pDescription )
		{
			DevWarning( "   View %s failed to allocate description!\n", szViewName );
		}
		KeyValues::AutoDelete autodelete_pDescription( pDescription );

		// See if we already have a request for this view
		DWORD dwViewId = pDescription->GetInt( ":id" );
		for ( int k = 0; k < m_arrSpecs.Count(); ++ k )
		{
			if ( m_arrSpecs[k].dwViewId == dwViewId )
			{
				dwViewId = 0;
				break;
			}
		}

		// If we already have a request for this view, then continue
		if ( !dwViewId )
			continue;

		// Otherwise add this view to the spec
		XUSER_STATS_SPEC xss = {0};
		xss.dwViewId = dwViewId;
		Assert( !xss.dwNumColumnIds );

		m_arrSpecs.AddToTail( xss );
		pDescription->SetString( ":name", szViewName );
		m_arrViewDescriptions.AddToTail( pDescription );
		autodelete_pDescription.Assign( NULL );	// don't autodelete now

		DevMsg( "    View: %s (%d)\n", szViewName, dwViewId );
	}
#elif !defined( NO_STEAM )
	// Clear view descriptions
	if ( m_pViewDescription )
		m_pViewDescription->deleteThis();
	m_pViewDescription = NULL;

	for ( int q = 0; q < m_arrRequests.Count(); ++ q )
	{
		KeyValues *pRequest = m_arrRequests[q];
		KeyValues::AutoDelete autodelete_pRequest( pRequest );
		m_arrRequests.Remove( q -- );

		char const *szViewName = pRequest->GetName();

		m_pViewDescription = g_pMMF->GetMatchTitle()->DescribeTitleLeaderboard( szViewName );
		if ( !m_pViewDescription )
		{
			DevWarning( "   View %s failed to allocate description!\n", szViewName );
			continue;
		}

		m_pViewDescription->SetString( ":name", szViewName );

		SteamAPICall_t hCall = steamapicontext->SteamUserStats()->FindLeaderboard( szViewName );
		m_CallbackOnLeaderboardFindResult.Set( hCall, this, &CLeaderboardRequestQueue::Steam_OnLeaderboardFindResult );
		m_bQueryRunning = true;
		break;
	}
#endif

	// Clean up all the requests in the queue
	DevMsg( "CLeaderboardRequestQueue::OnStartNewQuery - request prepared.\n" );

	// Run the query
	OnSubmitQuery();
}

void CLeaderboardRequestQueue::OnSubmitQuery()
{
#ifdef _X360
	if ( m_arrXuids.Count() && m_arrSpecs.Count() )
	{
		DWORD dwBytes = 0;
		DWORD ret = XUserReadStats( 0,
			m_arrXuids.Count(), m_arrXuids.Base(),
			m_arrSpecs.Count(), m_arrSpecs.Base(),
			&dwBytes, NULL, NULL );

		if ( ret == ERROR_INSUFFICIENT_BUFFER )
		{
			ZeroMemory( &m_xOverlapped, sizeof( m_xOverlapped ) );
			m_bufResults.EnsureCapacity( dwBytes );

			ret = XUserReadStats( 0,
				m_arrXuids.Count(), m_arrXuids.Base(),
				m_arrSpecs.Count(), m_arrSpecs.Base(),
				&dwBytes, ( PXUSER_STATS_READ_RESULTS ) m_bufResults.Base(),
				&m_xOverlapped );
		}

		if ( ret == ERROR_IO_PENDING )
		{
			DevMsg( "CLeaderboardRequestQueue::OnSubmitQuery - query submitted...\n" );
			m_bQueryRunning = true;
		}
		else
		{
			DevWarning( "CLeaderboardRequestQueue::OnSubmitQuery - failed [code = %d, bytes = %d]\n", ret, dwBytes );
		}
	}
#endif
}

#ifdef _X360
void CLeaderboardRequestQueue::ProcessResults( XUSER_STATS_READ_RESULTS const *pResults )
{
	/*
	901D41D61DC61					// XUID %llx
	{
		survival_c5m2_park
		{
			:rank		=	923		// uint64
			:rows		=	999		// uint64
			:rating		=	600		// uint64
			besttime	=	600		// uint64
		}
		... more views ...
	}
	... more users ...
	*/
	DevMsg( "LeaderboardRequestQueue: ProcessResults ( %d views )\n", pResults->dwNumViews );
	for ( DWORD k = 0; k < pResults->dwNumViews; ++ k )
	{
		XUSER_STATS_VIEW const *pView = &pResults->pViews[k];
		Assert( pView );

		KeyValues *pViewDesc = FindViewDescription( pView->dwViewId );
		Assert( pViewDesc );
		if ( !pViewDesc )
		{
			Warning( "LeaderboardRequestQueue: ProcessResults has no view description for view %d!\n", pView->dwViewId );
			continue;
		}

		char const *szViewName = pViewDesc->GetString( ":name" );
		DevMsg( "    Processing view %d ( %s ), total rows = %d\n", pView->dwViewId, szViewName, pView->dwTotalViewRows );

		for ( DWORD r = 0; r < pView->dwNumRows; ++ r )
		{
			XUSER_STATS_ROW const *pRow = &pView->pRows[r];
			if ( !pRow->dwRank && !pRow->i64Rating )
			{
				DevMsg( "        Gamer %s (%llx) not in view\n", pRow->szGamertag, pRow->xuid );
				continue;	// gamer is not present in the leaderboard
			}
			DevMsg( "        Gamer %s (%llx) data loaded: rank=%d, rating=%lld\n",
				pRow->szGamertag, pRow->xuid, pRow->dwRank, pRow->i64Rating );

			// Gamer is present in the leaderboard and should be included in the results
			if ( !m_pFinishedRequest )
				m_pFinishedRequest = new KeyValues( "Leaderboard" );

			// Find or create the view for this gamer
			KeyValues *pUserInfo = m_pFinishedRequest->FindKey( CFmtStr( "%llx/%s", pRow->xuid, szViewName ), true );

			// Set his rank and rating
			pUserInfo->SetUint64( ":rank", pRow->dwRank );
			pUserInfo->SetUint64( ":rows", pView->dwTotalViewRows );
			pUserInfo->SetUint64( ":rating", pRow->i64Rating );

			if ( char const *szName = pViewDesc->GetString( ":rating/name", NULL ) )
			{
				if ( szName[0] )
					pUserInfo->SetUint64( szName, pRow->i64Rating );
			}

			// Process additional columns that were retrieved
			Assert( !pRow->dwNumColumns );
			// 			for ( DWORD c = 0; c < pRow->dwNumColumns; ++ c )
			// 			{
			// 				XUSER_STATS_COLUMN const *pCol = pRow->pColumns[ c ];
			// 				KeyValues *pColDesc = FindViewColumnDesc( pViewDesc, pCol->wColumnId )
			// 			}
		}
	}
	DevMsg( "LeaderboardRequestQueue: ProcessResults finished.\n" );
}
#elif !defined( NO_STEAM )
void CLeaderboardRequestQueue::ProcessResults( LeaderboardEntry_t const &lbe )
{
	/*
	901D41D61DC61					// XUID %llx
	{
		survival_c5m2_park
		{
			:rank		=	923		// uint64
			:rows		=	999		// uint64
			:rating		=	600		// uint64
			besttime	=	600		// uint64
		}
		... more views ...
	}
	... more users ...
	*/
	KeyValues *pViewDesc = m_pViewDescription;
	Assert( pViewDesc );
	if ( !pViewDesc )
	{
		Warning( "LeaderboardRequestQueue: ProcessResults has no view description for view!\n" );
		return;
	}

	char const *szViewName = pViewDesc->GetString( ":name" );
	DevMsg( "    Processing view %s\n", szViewName );

	DevMsg( "        Gamer data loaded: rank=%d, score=%d\n",
		lbe.m_nGlobalRank, lbe.m_nScore );

	// Gamer is present in the leaderboard and should be included in the results
	if ( !m_pFinishedRequest )
		m_pFinishedRequest = new KeyValues( "Leaderboard" );

	// Find or create the view for this gamer
	KeyValues *pUserInfo = m_pFinishedRequest->FindKey( CFmtStr( "%llx/%s",
		g_pPlayerManager->GetLocalPlayer( XBX_GetPrimaryUserId() )->GetXUID(), szViewName ), true );

	// Set user score
	pUserInfo->SetUint64( pViewDesc->GetString( ":score" ), lbe.m_nScore );

	DevMsg( "LeaderboardRequestQueue: ProcessResults finished.\n" );
}
#endif

#ifdef _X360

KeyValues * CLeaderboardRequestQueue::FindViewDescription( DWORD dwViewId )
{
	if ( !dwViewId )
		return NULL;

	for ( int k = 0; k < m_arrViewDescriptions.Count(); ++ k )
	{
		KeyValues *pDesc = m_arrViewDescriptions[k];
		if ( pDesc->GetInt( ":id" ) == ( int ) dwViewId )
			return pDesc;
	}

	return NULL;
}

#elif !defined( NO_STEAM )

void CLeaderboardRequestQueue::Steam_OnLeaderboardFindResult( LeaderboardFindResult_t *p, bool bError )
{
	if ( bError || !p->m_bLeaderboardFound )
	{
		DevMsg( "Steam leaderboard was not found.\n" );
		OnQueryFinished();
		return;
	}

	// Download the data
	SteamAPICall_t hCall = steamapicontext->SteamUserStats()->DownloadLeaderboardEntries( p->m_hSteamLeaderboard,
		k_ELeaderboardDataRequestGlobalAroundUser, 0, 0 );
	m_CallbackOnLeaderboardScoresDownloaded.Set( hCall, this, &CLeaderboardRequestQueue::Steam_OnLeaderboardScoresDownloaded );
}

void CLeaderboardRequestQueue::Steam_OnLeaderboardScoresDownloaded( LeaderboardScoresDownloaded_t *p, bool bError )
{
	// Fetch the data if found and no error
	LeaderboardEntry_t lbe;
	if ( !bError &&
		p->m_cEntryCount == 1 &&
		steamapicontext->SteamUserStats()->GetDownloadedLeaderboardEntry( p->m_hSteamLeaderboardEntries, 0, &lbe, NULL, 0 ) )
	{
		ProcessResults( lbe );
	}

	OnQueryFinished();
}

#endif


