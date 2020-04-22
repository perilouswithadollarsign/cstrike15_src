//===== Copyright 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_framework.h"

#include "vstdlib/random.h"
#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static ConVar mm_ignored_sessions_forget_time( "mm_ignored_sessions_forget_time", "600", FCVAR_DEVELOPMENTONLY );
static ConVar mm_ignored_sessions_forget_pass( "mm_ignored_sessions_forget_pass", "5", FCVAR_DEVELOPMENTONLY );

class CIgnoredSessionsMgr
{
public:
	CIgnoredSessionsMgr();

public:
	void Reset();
	void OnSearchStarted();
	bool IsIgnored( XNKID xid );
	void Ignore( XNKID xid );

protected:
	struct SessionSearchPass_t
	{
		double m_flTime;
		int m_nSearchCounter;
	};

	static bool XNKID_LessFunc( const XNKID &lhs, const XNKID &rhs )
	{
		return ( (uint64 const&) lhs ) < ( (uint64 const&) rhs );
	}

	CUtlMap< XNKID, SessionSearchPass_t > m_IgnoredSessionsAndTime;
	int m_nSearchCounter;
};

static CIgnoredSessionsMgr g_IgnoredSessionsMgr;
static CUtlMap< uint32, float > g_mapValidatedWhitelistCacheTimestamps( DefLessFunc( uint32 ) );

CON_COMMAND_F( mm_ignored_sessions_reset, "Reset ignored sessions", FCVAR_DEVELOPMENTONLY )
{
	g_IgnoredSessionsMgr.Reset();
	DevMsg( "Reset ignored sessions" );
}

CIgnoredSessionsMgr::CIgnoredSessionsMgr() :
	m_IgnoredSessionsAndTime( XNKID_LessFunc ),
	m_nSearchCounter( 0 )
{
}

void CIgnoredSessionsMgr::Reset()
{
	m_nSearchCounter = 0;
	m_IgnoredSessionsAndTime.RemoveAll();
}

void CIgnoredSessionsMgr::OnSearchStarted()
{
	++ m_nSearchCounter;

	double fNow = Plat_FloatTime();
	double const fKeepIgnoredTime = mm_ignored_sessions_forget_time.GetFloat();
	int const numIgnoredSearches = mm_ignored_sessions_forget_pass.GetInt();

	// Keep sessions for only so long...
	for ( int x = m_IgnoredSessionsAndTime.FirstInorder();
		x != m_IgnoredSessionsAndTime.InvalidIndex(); )
	{
		SessionSearchPass_t ssp = m_IgnoredSessionsAndTime.Element( x );
		int xNext = m_IgnoredSessionsAndTime.NextInorder( x );
		if ( fabs( fNow - ssp.m_flTime ) > fKeepIgnoredTime &&
			m_nSearchCounter - ssp.m_nSearchCounter > numIgnoredSearches )
		{
			m_IgnoredSessionsAndTime.RemoveAt( x );
		}
		x = xNext;
	}
}

bool CIgnoredSessionsMgr::IsIgnored( XNKID xid )
{
	return ( m_IgnoredSessionsAndTime.Find( xid ) != m_IgnoredSessionsAndTime.InvalidIndex() );
}

void CIgnoredSessionsMgr::Ignore( XNKID xid )
{
	if ( ( const uint64 & )xid == 0ull )
		return;

	SessionSearchPass_t ssp = { Plat_FloatTime(), m_nSearchCounter };
	m_IgnoredSessionsAndTime.InsertOrReplace( xid, ssp );
}



//
// CMatchSessionOnlineSearch
//
// Implementation of an online session search (aka matchmaking)
//

CMatchSessionOnlineSearch::CMatchSessionOnlineSearch( KeyValues *pSettings ) :
	m_pSettings( pSettings->MakeCopy() ),
	m_autodelete_pSettings( m_pSettings ),
	m_eState( STATE_INIT ),
	m_pSysSession( NULL ),
	m_pMatchSearcher( NULL ),
	m_result( RESULT_UNDEFINED ),
	m_pSysSessionConTeam (NULL),
#if !defined( NO_STEAM )
	m_pServerListListener( NULL ),
#endif
	m_flInitializeTimestamp( 0.0f )
{
	DevMsg( "Created CMatchSessionOnlineSearch:\n" );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );
}

CMatchSessionOnlineSearch::CMatchSessionOnlineSearch() :
	m_pSettings( NULL ),
	m_autodelete_pSettings( (KeyValues*)NULL ),
	m_eState( STATE_INIT ),
	m_pSysSession( NULL ),
	m_pMatchSearcher( NULL ),
	m_result( RESULT_UNDEFINED ),
	m_pSysSessionConTeam (NULL),
	m_flInitializeTimestamp( 0.0f )
{
}

CMatchSessionOnlineSearch::~CMatchSessionOnlineSearch()
{
	if ( m_pMatchSearcher )
		m_pMatchSearcher->Destroy();
	m_pMatchSearcher = NULL;

	DevMsg( "Destroying CMatchSessionOnlineSearch:\n" );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );
}

KeyValues * CMatchSessionOnlineSearch::GetSessionSettings()
{
	return m_pSettings;
}

void CMatchSessionOnlineSearch::UpdateSessionSettings( KeyValues *pSettings )
{
	Warning( "CMatchSessionOnlineSearch::UpdateSessionSettings is unavailable in state %d!\n", m_eState );
	Assert( !"CMatchSessionOnlineSearch::UpdateSessionSettings is unavailable!\n" );
}

void CMatchSessionOnlineSearch::Command( KeyValues *pCommand )
{
	Warning( "CMatchSessionOnlineSearch::Command is unavailable!\n" );
	Assert( !"CMatchSessionOnlineSearch::Command is unavailable!\n" );
}

uint64 CMatchSessionOnlineSearch::GetSessionID()
{
	return 0;
}

#if !defined( NO_STEAM )
extern volatile uint32 *g_hRankingSetupCallHandle;
void CMatchSessionOnlineSearch::SetupSteamRankingConfiguration()
{
	KeyValues *kvNotification = new KeyValues( "SetupSteamRankingConfiguration" );
	kvNotification->SetPtr( "settingsptr", m_pSettings );
	kvNotification->SetPtr( "callhandleptr", ( void * ) &g_hRankingSetupCallHandle );

	g_pMatchEventsSubscription->BroadcastEvent( kvNotification );
}

bool CMatchSessionOnlineSearch::IsSteamRankingConfigured() const
{
	return !g_hRankingSetupCallHandle || !*g_hRankingSetupCallHandle;
}
#endif

extern ConVar mm_session_sys_ranking_timeout;
extern ConVar mm_session_search_qos_timeout;

void CMatchSessionOnlineSearch::Update()
{
	switch ( m_eState )
	{
	case STATE_INIT:
		if ( !m_flInitializeTimestamp )
		{
			m_flInitializeTimestamp = Plat_FloatTime();
#if !defined( NO_STEAM )
			SetupSteamRankingConfiguration();
#endif
		}
#if !defined( NO_STEAM )
		if ( !IsSteamRankingConfigured() && ( Plat_FloatTime() < m_flInitializeTimestamp + mm_session_sys_ranking_timeout.GetFloat() ) )
			break;
#endif

		m_eState = STATE_SEARCHING;

		// Kick off the search
		g_IgnoredSessionsMgr.OnSearchStarted();
		m_pMatchSearcher = OnStartSearching();
		
		// Update our settings with match searcher
		m_pSettings->deleteThis();
		m_pSettings = m_pMatchSearcher->GetSearchSettings()->MakeCopy();
		m_autodelete_pSettings.Assign( m_pSettings );

		// Run the first frame update on the searcher
		m_pMatchSearcher->Update();
		break;

	case STATE_SEARCHING:
		// Waiting for session search to complete
		m_pMatchSearcher->Update();
		break;

	case STATE_JOIN_NEXT:
		StartJoinNextFoundSession();
		break;

#if !defined( NO_STEAM )
	case STATE_VALIDATING_WHITELIST:
		if ( Plat_FloatTime() > m_flInitializeTimestamp + mm_session_search_qos_timeout.GetFloat() )
		{
			DevWarning( "Steam whitelist validation timed out.\n" );
			Steam_OnDedicatedServerListFetched();
		}
		break;
#endif

	case STATE_JOINING:
		// Waiting for the join negotiation
		if ( m_pSysSession )
		{
			m_pSysSession->Update();
		}

		if (m_pSysSessionConTeam)
		{
			m_pSysSessionConTeam->Update();

			switch ( m_pSysSessionConTeam->GetResult() )
			{
			case CSysSessionConTeamHost::RESULT_SUCCESS:
				OnSearchCompletedSuccess( NULL, m_pSettings );
				break;

			case CSysSessionConTeamHost::RESULT_FAIL:
				
				m_pSysSessionConTeam->Destroy();
				m_pSysSessionConTeam = NULL;
				// Try next session
				m_eState = STATE_JOIN_NEXT;
				break;
			}
		}
		break;
	}
}

void CMatchSessionOnlineSearch::Destroy()
{
	// Stop the search
	if ( m_pMatchSearcher )
	{
		m_pMatchSearcher->Destroy();
		m_pMatchSearcher = NULL;
	}

	// If we are in the middle of connecting,
	// abort
	if ( m_pSysSession )
	{
		m_pSysSession->Destroy();
		m_pSysSession = NULL;
	}

	if ( m_pSysSessionConTeam )
	{
		m_pSysSessionConTeam->Destroy();
		m_pSysSessionConTeam = NULL;
	}

#if !defined( NO_STEAM )
	if ( m_pServerListListener )
	{
		m_pServerListListener->Destroy();
		m_pServerListListener = NULL;
	}
#endif

	delete this;
}

void CMatchSessionOnlineSearch::DebugPrint()
{
	DevMsg( "CMatchSessionOnlineSearch [ state=%d ]\n", m_eState );
	
	DevMsg( "System data:\n" );
	KeyValuesDumpAsDevMsg( GetSessionSystemData(), 1 );

	DevMsg( "Settings data:\n" );
	KeyValuesDumpAsDevMsg( GetSessionSettings(), 1 );
	
	if ( m_pSysSession )
		m_pSysSession->DebugPrint();
	else
		DevMsg( "SysSession is NULL\n" );

	DevMsg( "Search results outstanding: %d\n", m_arrSearchResults.Count() );
}

void CMatchSessionOnlineSearch::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( !Q_stricmp( "mmF->SysSessionUpdate", szEvent ) )
	{
		if ( m_pSysSession && pEvent->GetPtr( "syssession", NULL ) == m_pSysSession )
		{
			// This is our session
			switch ( m_eState )
			{
			case STATE_JOINING:
				// Session was creating
				if ( char const *szError = pEvent->GetString( "error", NULL ) )
				{
					// Destroy the session
					m_pSysSession->Destroy();
					m_pSysSession = NULL;

					// Go ahead and join next available session
					m_eState = STATE_JOIN_NEXT;
				}
				else
				{
					// We have received an entirely new "settings" data and copied that to our "settings" data
					m_eState = STATE_CLOSING;

					// Now we need to create a new client session
					CSysSessionClient *pSysSession = m_pSysSession;
					KeyValues *pSettings = m_pSettings;

					// Release ownership of the resources since new match session now owns them
					m_pSysSession = NULL;
					m_pSettings = NULL;
					m_autodelete_pSettings.Assign( NULL );

					OnSearchCompletedSuccess( pSysSession, pSettings );
					return;
				}
				break;
			}
		}
	}
}

void CMatchSessionOnlineSearch::OnSearchEvent( KeyValues *pNotify )
{
	g_pMatchEventsSubscription->BroadcastEvent( pNotify );
}

CSysSessionClient * CMatchSessionOnlineSearch::OnBeginJoiningSearchResult()
{
	return new CSysSessionClient( m_pSettings );
}

void CMatchSessionOnlineSearch::OnSearchDoneNoResultsMatch()
{
	m_eState = STATE_CLOSING;	

	// Reset ignored session tracker
	g_IgnoredSessionsMgr.Reset();

	// Just go ahead and create the session
	KeyValues *pSettings = m_pSettings;

	m_pSettings = NULL;
	m_autodelete_pSettings.Assign( NULL );

	OnSearchCompletedEmpty( pSettings );
}

void CMatchSessionOnlineSearch::OnSearchCompletedSuccess( CSysSessionClient *pSysSession, KeyValues *pSettings )
{
	m_result = RESULT_SUCCESS;

	// Note that m_pSysSessionConTeam will be NULL if this is an individual joining a
	// match that was started with con team
	KeyValues *teamMatch = pSettings->FindKey( "options/conteammatch" );
	if ( teamMatch && m_pSysSessionConTeam )
	{
		DevMsg( "OnlineSearch - ConTeam host reserved session\n" );
		KeyValuesDumpAsDevMsg( pSettings );
		int numPlayers, sides[10];
		uint64 playerIds[10];

		if ( !m_pSysSessionConTeam->GetPlayerSidesAssignment( &numPlayers, playerIds, sides ) )
		{
			// Something went badly wrong, bail
			m_result = RESULT_FAIL;
		}
		else
		{
			// Send out a "joinsession" event. This is picked up by the host and sent out
			// to all machines machines in lobby. 
			KeyValues *joinSession = new KeyValues(
				"OnMatchSessionUpdate",
				"state", "joinconteamsession"		
				);
#if defined (_X360)
			
			uint64 sessionId = pSettings->GetUint64( "options/sessionid", 0 ); 
			const char *sessionInfo = pSettings->GetString( "options/sessioninfo", "" );
			
			joinSession->SetUint64( "sessionid", sessionId );
			joinSession->SetString( "sessioninfo", sessionInfo );

			// Unpack sessionHostData
			KeyValues *pSessionHostData = (KeyValues*)pSettings->GetPtr( "options/sessionHostData" );
			if ( pSessionHostData )
			{
				KeyValues *pSessionHostDataUnpacked = joinSession->CreateNewKey();
				pSessionHostDataUnpacked->SetName("sessionHostDataUnpacked");	
				pSessionHostData->CopySubkeys( pSessionHostDataUnpacked );
			}

#else
			joinSession->SetUint64( "sessionid", m_pSysSessionConTeam->GetSessionID() );
#endif			
			KeyValues *pTeamMembers = joinSession->CreateNewKey();
			pTeamMembers->SetName( "teamMembers" );
			pTeamMembers->SetInt( "numPlayers", numPlayers );
			
			// Assign players to different teams
			for ( int i = 0; i < numPlayers; i++ )
			{
				KeyValues *pTeamPlayer = pTeamMembers->CreateNewKey();
				pTeamPlayer->SetName( CFmtStr( "player%d", i ) );
				pTeamPlayer->SetUint64( "xuid", playerIds[i] );
				pTeamPlayer->SetInt( "team", sides[i] );
			}

			OnSearchEvent( joinSession );
		}
	}
	else
	{
		// Destroy our instance and point at the new match interface
		CMatchSessionOnlineClient *pNewSession = new CMatchSessionOnlineClient( pSysSession, pSettings );
		g_pMMF->SetCurrentMatchSession( pNewSession );
	
		this->Destroy();

		DevMsg( "OnlineSearch - client fully connected to session, search finished.\n" );
		pNewSession->OnClientFullyConnectedToSession();
	}	
}

void CMatchSessionOnlineSearch::OnSearchCompletedEmpty( KeyValues *pSettings )
{
	KeyValues::AutoDelete autodelete_pSettings( pSettings );

	m_result = RESULT_FAIL;

	KeyValues *notify = new KeyValues(
		"OnMatchSessionUpdate",
		"state", "progress",
		"progress", "searchempty"
		);
	notify->SetPtr( "settingsptr", pSettings );

	OnSearchEvent( notify );
	if ( !Q_stricmp( pSettings->GetString( "options/searchempty" ), "close" ) )
	{
		g_pMatchFramework->CloseSession();
		return;
	}

	// If this is a team session then stop here and let the team host decide what
	// to do next
	KeyValues *teamMatch = pSettings->FindKey( "options/conteammatch" );
	if ( teamMatch )
	{
		return;
	}

	// Preserve the "options/bypasslobby" key
	bool bypassLobby = pSettings->GetBool( "options/bypasslobby", false );

	// Preserve the "options/server" key
	char serverType[64];
	const char *prevServerType = pSettings->GetString( "options/server", NULL );
	if ( prevServerType )
	{
		Q_strncpy( serverType, prevServerType, sizeof( serverType ) );
	}

	// Remove "options" key
	if ( KeyValues *kvOptions = pSettings->FindKey( "options" ) )
	{
		pSettings->RemoveSubKey( kvOptions );
		kvOptions->deleteThis();
	}
	
	pSettings->SetString( "options/createreason", "searchempty" );
	if ( bypassLobby )
	{
		pSettings->SetBool( "options/bypasslobby", bypassLobby );
	}
	if ( prevServerType )
	{
		pSettings->SetString( "options/server", serverType );
	}

	DevMsg( "Search completed empty - creating a new session\n" );
	KeyValuesDumpAsDevMsg( pSettings );

	g_pMatchFramework->CreateSession( pSettings );
}


void CMatchSessionOnlineSearch::UpdateTeamProperties( KeyValues *pTeamProperties )
{
}

void CMatchSessionOnlineSearch::StartJoinNextFoundSession()
{
	if ( !m_arrSearchResults.Count() )
	{
		OnSearchDoneNoResultsMatch();
		return;
	}

	// Session is joining
	KeyValues *notify = new KeyValues(
		"OnMatchSessionUpdate",
			"state", "progress",
			"progress", "searchresult"
		);
	notify->SetInt( "numResults", m_arrSearchResults.Count() );
	OnSearchEvent( notify );

	// Peek at the next search result
	CMatchSearcher::SearchResult_t const &sr = *m_arrSearchResults.Head();

	// Register it into the ignored session pool
	g_IgnoredSessionsMgr.Ignore( sr.GetXNKID() );

	// Make a validation query
	ValidateSearchResultWhitelist();
}

static uint32 OfficialWhitelistClientCachedAddress( uint32 uiServerIP )
{
	/** Removed for partner depot **/
	return 0;
}

void CMatchSessionOnlineSearch::ValidateSearchResultWhitelist()
{
#if !defined( NO_STEAM )
	// In case of official matchmaking we need to validate that the server
	// that we are about to join actually is whitelisted official server
	if ( !m_pSettings->GetInt( "game/hosted" ) )
	{
		// Peek at the next search result
		CMatchSearcher::SearchResult_t const &sr = *m_arrSearchResults.Head();
		if ( ( g_mapValidatedWhitelistCacheTimestamps.Find( sr.m_svAdr.GetIPHostByteOrder() ) == g_mapValidatedWhitelistCacheTimestamps.InvalidIndex() ) &&
			!OfficialWhitelistClientCachedAddress( sr.m_svAdr.GetIPHostByteOrder() )
			)
		{
			// This server needs to be validated
			m_flInitializeTimestamp = Plat_FloatTime();
			m_eState = STATE_VALIDATING_WHITELIST;

			CUtlVector< MatchMakingKeyValuePair_t > filters;
			filters.EnsureCapacity( 10 );
			filters.AddToTail( MatchMakingKeyValuePair_t( "gamedir", COM_GetModDirectory() ) );
			filters.AddToTail( MatchMakingKeyValuePair_t( "addr", sr.m_svAdr.ToString() ) );
			filters.AddToTail( MatchMakingKeyValuePair_t( "white", "1" ) );

			m_pServerListListener = new CServerListListener( this, filters );
			return;
		}
	}
#endif
	ConnectJoinLobbyNextFoundSession();
}

#if !defined( NO_STEAM )

CMatchSessionOnlineSearch::CServerListListener::CServerListListener( CMatchSessionOnlineSearch *pDsSearcher, CUtlVector< MatchMakingKeyValuePair_t > &filters ) :
	m_pOuter( pDsSearcher ),
	m_hRequest( NULL )
{
	MatchMakingKeyValuePair_t *pFilter = filters.Base();
	DevMsg( 1, "Requesting dedicated whitelist validation...\n" );
	for (int i = 0; i < filters.Count(); i++)
	{
		DevMsg("Filter %d: %s=%s\n", i, filters.Element(i).m_szKey, filters.Element(i).m_szValue);
	}
	m_hRequest = steamapicontext->SteamMatchmakingServers()->RequestInternetServerList(
		( AppId_t ) g_pMatchFramework->GetMatchTitle()->GetTitleID(), &pFilter,
		filters.Count(), this );
}

void CMatchSessionOnlineSearch::CServerListListener::Destroy()
{
	m_pOuter = NULL;

	if ( m_hRequest )
		steamapicontext->SteamMatchmakingServers()->ReleaseRequest( m_hRequest );
	m_hRequest = NULL;

	delete this;
}

void CMatchSessionOnlineSearch::CServerListListener::HandleServerResponse( HServerListRequest hReq, int iServer, bool bResponded )
{
	// Register the result
	if ( bResponded )
	{
		gameserveritem_t *pServer = steamapicontext->SteamMatchmakingServers()
			->GetServerDetails( hReq, iServer );
		DevMsg( 1, "Successfully validated whitelist for %s...\n", pServer->m_NetAdr.GetConnectionAddressString() );
		g_mapValidatedWhitelistCacheTimestamps.InsertOrReplace( pServer->m_NetAdr.GetIP(), Plat_FloatTime() );
	}
}

void CMatchSessionOnlineSearch::CServerListListener::RefreshComplete( HServerListRequest hReq, EMatchMakingServerResponse response )
{
	if ( m_pOuter )
	{
		m_pOuter->Steam_OnDedicatedServerListFetched();
	}
}

void CMatchSessionOnlineSearch::Steam_OnDedicatedServerListFetched()
{
	if ( m_pServerListListener )
	{
		m_pServerListListener->Destroy();
		m_pServerListListener = NULL;
	}

	// Peek at the next search result
	if ( m_arrSearchResults.Count() )
	{
		CMatchSearcher::SearchResult_t const &sr = *m_arrSearchResults.Head();
		if ( g_mapValidatedWhitelistCacheTimestamps.Find( sr.m_svAdr.GetIPHostByteOrder() ) == g_mapValidatedWhitelistCacheTimestamps.InvalidIndex() )
		{
			DevWarning( 1, "Failed to validate whitelist for %s...\n", sr.m_svAdr.ToString( true ) );
			FOR_EACH_VEC_BACK( m_arrSearchResults, itSR )
			{
				if ( m_arrSearchResults[itSR]->m_svAdr.GetIPHostByteOrder() == sr.m_svAdr.GetIPHostByteOrder() )
				{
					m_arrSearchResults.Remove( itSR );
				}
			}
		}
	}

	m_eState = STATE_JOIN_NEXT;
}

#endif

void CMatchSessionOnlineSearch::ConnectJoinLobbyNextFoundSession()
{
	// Pop the search result
	CMatchSearcher::SearchResult_t const &sr = *m_arrSearchResults.Head();
	m_arrSearchResults.RemoveMultipleFromHead( 1 );

	// Set the settings to connect with
	if ( KeyValues *kvOptions = m_pSettings->FindKey( "options", true ) )
	{
#ifdef _X360
		kvOptions->SetUint64( "sessionid", ( const uint64 & ) sr.m_info.sessionID );

		char chSessionInfo[ XSESSION_INFO_STRING_LENGTH ] = {0};
		MMX360_SessionInfoToString( sr.m_info, chSessionInfo );
		kvOptions->SetString( "sessioninfo", chSessionInfo );

		kvOptions->SetPtr( "sessionHostData", sr.GetGameDetails() );

		KeyValuesDumpAsDevMsg( sr.GetGameDetails(), 1, 2 );
#else
		kvOptions->SetUint64( "sessionid", sr.m_uiLobbyId );
#endif
	}

	// Trigger client session creation
	Msg( "[MM] Joining session %llx, %d search results remaining...\n",
		m_pSettings->GetUint64( "options/sessionid", 0ull ),
		m_arrSearchResults.Count() );		

	KeyValues *teamMatch = m_pSettings->FindKey( "options/conteammatch" );
	if  ( teamMatch )
	{
		m_pSysSessionConTeam = new CSysSessionConTeamHost( m_pSettings );
	}
	else
	{
		m_pSysSession = OnBeginJoiningSearchResult();
	}

	m_eState = STATE_JOINING;
}


CMatchSearcher_OnlineSearch::CMatchSearcher_OnlineSearch( CMatchSessionOnlineSearch *pSession, KeyValues *pSettings ) :
	CMatchSearcher( pSettings ),
	m_pSession( pSession )
{
}

void CMatchSearcher_OnlineSearch::OnSearchEvent( KeyValues *pNotify )
{
	m_pSession->OnSearchEvent( pNotify );
}

void CMatchSearcher_OnlineSearch::OnSearchDone()
{
	// Let the base searcher finalize results
	CMatchSearcher::OnSearchDone();

	// Iterate over search results
	for ( int k = 0, kNum = GetNumSearchResults(); k < kNum; ++ k )
	{
		SearchResult_t const &sr = GetSearchResult( k );
		if ( !g_IgnoredSessionsMgr.IsIgnored( sr.GetXNKID() ) )
			m_pSession->m_arrSearchResults.AddToTail( &sr );
	}

	if ( !m_pSession->m_arrSearchResults.Count() )
	{
		m_pSession->OnSearchDoneNoResultsMatch();
		return;
	}

	// Go ahead and start joining the results
	DevMsg( "Establishing connection with %d search results.\n", m_pSession->m_arrSearchResults.Count() );
	m_pSession->m_eState = m_pSession->STATE_JOIN_NEXT;
}

CMatchSearcher * CMatchSessionOnlineSearch::OnStartSearching()
{
	CMatchSearcher *pMS = new CMatchSearcher_OnlineSearch( this, m_pSettings->MakeCopy() );

	// Let the title extend the game settings
	g_pMMF->GetMatchTitleGameSettingsMgr()->InitializeGameSettings( pMS->GetSearchSettings(), "search_online" );

	DevMsg( "CMatchSearcher_OnlineSearch title adjusted settings:\n" );
	KeyValuesDumpAsDevMsg( pMS->GetSearchSettings(), 1 );

	return pMS;
}
