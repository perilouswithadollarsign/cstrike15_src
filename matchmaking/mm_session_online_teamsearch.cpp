//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_framework.h"

#include "vstdlib/random.h"
#include "fmtstr.h"
#include "steam_datacenterjobs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar mm_teamsearch_errortime( "mm_teamsearch_errortime", "3.0", FCVAR_DEVELOPMENTONLY, "Time team search is in error state until it self-cancels" );
ConVar mm_teamsearch_nostart( "mm_teamsearch_nostart", "0", FCVAR_DEVELOPMENTONLY, "Team search will fake cancel before searching for server" );

#ifndef _GAMECONSOLE
ConVar sv_search_team_key( "sv_search_team_key", "public", FCVAR_RELEASE, "When initiating team search, set this key to match with known opponents team" );
#endif

//
//
// Specialized implementation of SysSessions for team search
//
//

template < typename TBaseSession >
class CSysSessionStubForTeamSearch : public TBaseSession
{
public:
	explicit CSysSessionStubForTeamSearch( KeyValues *pSettings, CMatchSessionOnlineTeamSearch *pMatchSession ) :
		TBaseSession( pSettings ),
		m_pMatchSession( pMatchSession )
	{
	}

protected:
	virtual void OnSessionEvent( KeyValues *notify )
	{
		if ( !notify )
			return;

		if ( m_pMatchSession )
			m_pMatchSession->OnSessionEvent( notify );

		notify->deleteThis();
	}

protected:
	// No voice
	virtual void Voice_ProcessTalkers( KeyValues *pMachine, bool bAdd ) {}
	virtual void Voice_CaptureAndTransmitLocalVoiceData() {}
	virtual void Voice_Playback( KeyValues *msg, XUID xuidSrc ) OVERRIDE {}
	virtual void Voice_UpdateLocalHeadsetsStatus() {}
	virtual void Voice_UpdateMutelist() {}

protected:
	// No p2p connections, just two hosts talking
	virtual void XP2P_Interconnect() {}

protected:
	CMatchSessionOnlineTeamSearch *m_pMatchSession;
};

typedef CSysSessionStubForTeamSearch< CSysSessionClient > CSysTeamSearchClient;
typedef CSysSessionStubForTeamSearch< CSysSessionHost > CSysTeamSearchHost;



//
//
// CMatchSessionOnlineTeamSearch implementation
//
//

CMatchSessionOnlineTeamSearch::CMatchSessionOnlineTeamSearch( KeyValues *pSettings, CMatchSessionOnlineHost *pHost ) :
	m_pHostSession( pHost ),
	m_eState( STATE_SEARCHING ),
	m_pSysSessionHost( NULL ),
	m_pSysSessionClient( NULL ),
	m_pDsSearcher( NULL ),
	m_flActionTime( 0.0f ),
	m_pUpdateHostSessionPacket( NULL ),
	m_autodelete_pUpdateHostSessionPacket( m_pUpdateHostSessionPacket ),
	m_iLinkState( 0 ),
	m_xuidLinkPeer( 0ull ),
#ifdef _X360
	m_pXlspConnection( NULL ),
	m_pXlspCommandBatch( NULL ),
#endif
	m_flCreationTime( Plat_FloatTime() )
{
	m_pSettings = pSettings->MakeCopy();
	m_autodelete_pSettings.Assign( m_pSettings );

#ifndef _GAMECONSOLE
	m_pSettings->SetString( "options/searchteamkey", sv_search_team_key.GetString() );
#endif

	if ( IsPC() )
	{
		// On PC limit server selection for team games to official and listen
		if ( Q_stricmp( m_pSettings->GetString( "options/server" ), "listen" ) )
			m_pSettings->SetString( "options/server", "official" );
	}

	DevMsg( "Created CMatchSessionOnlineTeamSearch:\n" );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );
}

CMatchSessionOnlineTeamSearch::~CMatchSessionOnlineTeamSearch()
{
	DevMsg( "Destroying CMatchSessionOnlineTeamSearch.\n" );
}

CMatchSearcher * CMatchSessionOnlineTeamSearch::OnStartSearching()
{
	return new CMatchSearcher_OnlineTeamSearch( this, m_pSettings->MakeCopy() );
}

CMatchSearcher_OnlineTeamSearch::CMatchSearcher_OnlineTeamSearch( CMatchSessionOnlineTeamSearch *pSession, KeyValues *pSettings ) :
	CMatchSearcher_OnlineSearch( pSession, pSettings )
{
	// Search settings cannot be locked as it will interfere with syssessions
	m_pSettings->SetString( "system/lock", "" );

	// Team versus searchable lobbies are public
	m_pSettings->SetString( "system/access", "public" );

	// If we happen to host the session it will be a special teamlink session
	m_pSettings->SetString( "system/netflag", "teamlink" );

	if ( IMatchSession *pMainSession = g_pMatchFramework->GetMatchSession() )
	{
		KeyValues *kvSystemData = pMainSession->GetSessionSystemData();
		m_pSettings->SetUint64( "System/dependentlobby", kvSystemData->GetUint64( "xuidReserve" ) );
	}

	// Double the number of slots since we assume two teams playing
	m_pSettings->SetInt( "Members/numSlots", g_pMatchFramework->GetMatchTitle()->GetTotalNumPlayersSupported() );

	// Let the title extend the game settings
	g_pMMF->GetMatchTitleGameSettingsMgr()->InitializeGameSettings( m_pSettings, "search_onlineteam" );

	DevMsg( "CMatchSearcher_OnlineTeamSearch title adjusted settings:\n" );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );
}

void CMatchSearcher_OnlineTeamSearch::StartSearchPass( KeyValues *pSearchPass )
{
#ifndef _GAMECONSOLE
	pSearchPass->SetString( "Filter=/options:searchteamkey", sv_search_team_key.GetString() );
#endif

	CMatchSearcher_OnlineSearch::StartSearchPass( pSearchPass );
}

void CMatchSessionOnlineTeamSearch::OnSearchEvent( KeyValues *pNotify )
{
	if ( !pNotify )
		return;

	if ( m_pHostSession )
		m_pHostSession->OnEvent( pNotify );
	
	pNotify->deleteThis();
}

void CMatchSessionOnlineTeamSearch::OnSessionEvent( KeyValues *pEvent )
{
	OnEvent( pEvent );

	// Searching state is handled entirely by the base class
	if ( m_eState == STATE_SEARCHING )
		return;

	char const *szEvent = pEvent->GetName();

	if ( !Q_stricmp( "mmF->SysSessionUpdate", szEvent ) )
	{
		if ( m_pSysSessionHost && pEvent->GetPtr( "syssession", NULL ) == m_pSysSessionHost )
		{
			// We had a session error
			if ( char const *szError = pEvent->GetString( "error", NULL ) )
			{
				// Destroy the session
				m_pSysSessionHost->Destroy();
				m_pSysSessionHost = NULL;

				// Handle error
				m_eState = STATE_ERROR;
				m_flActionTime = Plat_FloatTime() + mm_teamsearch_errortime.GetFloat();
				OnSearchEvent( new KeyValues(
					"OnMatchSessionUpdate",
						"state", "progress",
						"progress", "searcherror"
					) );
				return;
			}

			// This is our session
			switch ( m_eState )
			{
			case STATE_CREATING:
				// Session created successfully and we are awaiting peer
				m_eState = STATE_AWAITING_PEER;
				OnSearchEvent( new KeyValues(
					"OnMatchSessionUpdate",
						"state", "progress",
						"progress", "searchawaitingpeer"
					) );
				break;
			}
		}
	}
	else if ( !Q_stricmp( "mmF->SysSessionCommand", szEvent ) )
	{
		if ( m_pSysSessionHost && pEvent->GetPtr( "syssession", NULL ) == m_pSysSessionHost )
		{
			KeyValues *pCommand = pEvent->GetFirstTrueSubKey();
			if ( pCommand )
			{
				OnRunSessionCommand( pCommand );
			}
		}
		if ( m_pSysSessionClient && pEvent->GetPtr( "syssession", NULL ) == m_pSysSessionClient )
		{
			KeyValues *pCommand = pEvent->GetFirstTrueSubKey();
			if ( pCommand )
			{
				OnRunSessionCommand( pCommand );
			}
		}
	}
	else if ( !Q_stricmp( "OnPlayerMachinesConnected", szEvent ) )
	{
		// Another team is challenging us
		m_eState = STATE_LINK_HOST;
		m_xuidLinkPeer = pEvent->GetUint64( "id" );
		OnSearchEvent( new KeyValues(
			"OnMatchSessionUpdate",
				"state", "progress",
				"progress", "searchlinked"
			) );

		// Lock the session to prevent other teams challenges
		m_pSettings->SetString( "system/lock", "linked" );
		m_pSysSessionHost->OnUpdateSessionSettings( KeyValues::AutoDeleteInline( KeyValues::FromString(
			"update",
			" update { "
				" system { "
					" lock linked "
				" } "
			" } "
			) ) );

		LinkHost().LinkInit();
	}
	else if ( !Q_stricmp( "OnPlayerRemoved", szEvent ) )
	{
		// Assert( m_xuidLinkPeer == pEvent->GetUint64( "xuid" ) );
		// the peer gave up before we finished negotiation, start the process all over
		ResetAndRestartTeamSearch();
	}
}

void CMatchSessionOnlineTeamSearch::ResetAndRestartTeamSearch()
{
	m_eState = STATE_ERROR;
	m_flActionTime = 0.0f;
	OnSearchEvent( new KeyValues(
		"OnMatchSessionUpdate",
			"state", "progress",
			"progress", "restart"
		) );
}

void CMatchSessionOnlineTeamSearch::RememberHostSessionUpdatePacket( KeyValues *pPacket )
{
	if ( m_pUpdateHostSessionPacket )
		m_pUpdateHostSessionPacket->deleteThis();
	
	m_pUpdateHostSessionPacket = pPacket;
	m_autodelete_pUpdateHostSessionPacket.Assign( m_pUpdateHostSessionPacket );
}

void CMatchSessionOnlineTeamSearch::ApplyHostSessionUpdatePacket()
{
	if ( m_pUpdateHostSessionPacket )
	{
		KeyValues *pUpdatePkt = m_pUpdateHostSessionPacket;
		m_pUpdateHostSessionPacket = NULL;
		m_autodelete_pUpdateHostSessionPacket.Assign( NULL );

		m_pHostSession->UpdateSessionSettings( pUpdatePkt );

		pUpdatePkt->deleteThis();
	}
}

void CMatchSessionOnlineTeamSearch::OnRunSessionCommand( KeyValues *pCommand )
{
	switch ( m_eState )
	{
	case STATE_LINK_HOST:
		LinkHost().LinkCommand( pCommand );
		break;
	case STATE_LINK_CLIENT:
		LinkClient().LinkCommand( pCommand );
		break;
	}
}

CMatchSessionOnlineTeamSearchLinkHost & CMatchSessionOnlineTeamSearch::LinkHost()
{
	return static_cast< CMatchSessionOnlineTeamSearchLinkHost & >( *this );
}

CMatchSessionOnlineTeamSearchLinkClient & CMatchSessionOnlineTeamSearch::LinkClient()
{
	return static_cast< CMatchSessionOnlineTeamSearchLinkClient & >( *this );
}

CSysSessionBase * CMatchSessionOnlineTeamSearch::LinkSysSession()
{
	switch ( m_eState )
	{
	case STATE_LINK_HOST:
		return m_pSysSessionHost;
	case STATE_LINK_CLIENT:
		return m_pSysSessionClient;
	default:
		Assert( !m_pSysSessionClient || !m_pSysSessionHost );
		if ( m_pSysSessionHost )
			return m_pSysSessionHost;
		else if ( m_pSysSessionClient )
			return m_pSysSessionClient;
		else
			return NULL;
	}
}

CSysSessionClient * CMatchSessionOnlineTeamSearch::OnBeginJoiningSearchResult()
{
	return new CSysTeamSearchClient( m_pSettings, this );
}

void CMatchSessionOnlineTeamSearch::OnSearchCompletedSuccess( CSysSessionClient *pSysSession, KeyValues *pSettings )
{
	// Push the settings back into searcher since we are going to be using them
	m_pSettings = pSettings;
	m_autodelete_pSettings.Assign( m_pSettings );
	m_pSysSessionClient = pSysSession;

	// Established connection with another team
	m_eState = STATE_LINK_CLIENT;
	m_xuidLinkPeer = pSysSession->GetHostXuid();
	OnSearchEvent( new KeyValues(
		"OnMatchSessionUpdate",
			"state", "progress",
			"progress", "searchlinked"
		) );
	
	LinkClient().LinkInit();
}

void CMatchSessionOnlineTeamSearch::OnSearchCompletedEmpty( KeyValues *pSettings )
{
	// Push the settings back into searcher since we are going to be searching again
	m_pSettings = pSettings;
	m_autodelete_pSettings.Assign( m_pSettings );

	// Idle out for some time
	m_eState = STATE_CREATING;

	// Notify everybody that our search idled out
	OnSearchEvent( new KeyValues(
		"OnMatchSessionUpdate",
			"state", "progress",
			"progress", "searchidle"
		) );

	// Allocate the hosting object to accept matches
	m_pSysSessionHost = new CSysTeamSearchHost( m_pSettings, this );
}

void CMatchSessionOnlineTeamSearch::DebugPrint()
{
	DevMsg( "CMatchSessionOnlineTeamSearch::CMatchSessionOnlineSearch\n" );
	
	CMatchSessionOnlineSearch::DebugPrint();

	DevMsg( "CMatchSessionOnlineTeamSearch [ state=%d ]\n", m_eState );
	DevMsg( "    linkstate: %d\n", m_iLinkState );
	DevMsg( "    linkpeer:  %llx\n", m_xuidLinkPeer );
	DevMsg( "    actiontime:%.3f\n", m_flActionTime ? m_flActionTime - Plat_FloatTime() : 0.0f );

	if ( m_pDsSearcher )
		DevMsg( "TeamSearch: Dedicated search in progress\n" );
	else
		DevMsg( "TeamSearch: Dedicated search not active\n" );

	DevMsg( "TeamSearch: SysSession host state:\n" );
	if ( m_pSysSessionHost )
		m_pSysSessionHost->DebugPrint();
	else
		DevMsg( "SysSession is NULL\n" );

	DevMsg( "TeamSearch: SysSession client state:\n" );
	if ( m_pSysSessionClient )
		m_pSysSessionClient->DebugPrint();
	else
		DevMsg( "SysSession is NULL\n" );
}

void CMatchSessionOnlineTeamSearch::OnEvent( KeyValues *pEvent )
{
	CMatchSessionOnlineSearch::OnEvent( pEvent );

	// Let the dedicated search handle the responses too
	if ( m_pDsSearcher )
		m_pDsSearcher->OnEvent( pEvent );
}

void CMatchSessionOnlineTeamSearch::Update()
{
	switch ( m_eState )
	{
	case STATE_ERROR:
		if ( m_flActionTime && Plat_FloatTime() > m_flActionTime )
		{
			m_flActionTime = 0.0f;
			if ( m_pHostSession )
				m_pHostSession->Command( KeyValues::AutoDeleteInline( new KeyValues( "Stop" ) ) );
		}
		return;

	case STATE_AWAITING_PEER:
		break;

	case STATE_LINK_CLIENT:
		LinkClient().LinkUpdate();
		break;

	case STATE_LINK_HOST:
		LinkHost().LinkUpdate();
		break;
	}

	CMatchSessionOnlineSearch::Update();

	if ( CSysSessionBase *pLinkSysSession = LinkSysSession() )
		pLinkSysSession->Update();
}

void CMatchSessionOnlineTeamSearch::Destroy()
{
	if ( m_pSysSessionHost )
	{
		m_pSysSessionHost->Destroy();
		m_pSysSessionHost = NULL;
	}
	if ( m_pSysSessionClient )
	{
		m_pSysSessionClient->Destroy();
		m_pSysSessionClient = NULL;
	}
	if ( m_pDsSearcher )
	{
		m_pDsSearcher->Destroy();
		m_pDsSearcher = NULL;
	}

#ifdef _X360
	if ( m_pXlspCommandBatch )
	{
		m_pXlspCommandBatch->Destroy();
		m_pXlspCommandBatch = NULL;
	}
	if ( m_pXlspConnection )
	{
		m_pXlspConnection->Destroy();
		m_pXlspConnection = NULL;
	}
#endif

	CMatchSessionOnlineSearch::Destroy();
}


//////////////////////////////////////////////////////////////////////////
//
// Link base implementation
//
//

void CMatchSessionOnlineTeamSearchLinkBase::LinkUpdate()
{
	switch ( m_iLinkState )
	{
	case STATE_SEARCHING_DEDICATED:
		Assert( m_pDsSearcher );
		if ( m_pDsSearcher )
		{
			m_pDsSearcher->Update();
			if ( m_pDsSearcher->IsFinished() )
			{
				OnDedicatedSearchFinished();
				return;
			}
		}
		break;

	case STATE_HOSTING_LISTEN_SERVER:
		if ( KeyValues *pServer = m_pHostSession->GetSessionSettings()->FindKey( "server" ) )
		{
			// Listen server has started and we should notify the link peer
			if ( KeyValues *pPeerCommand = new KeyValues( "TeamSearchLink::ListenClient" ) )
			{
				KeyValues::AutoDelete autodelete( pPeerCommand );
				pPeerCommand->SetString( "run", "xuid" );
				pPeerCommand->SetUint64( "runxuid", m_xuidLinkPeer );
				
				pPeerCommand->AddSubKey( pServer->MakeCopy() );
				pPeerCommand->SetString( "server/server", "externalpeer" ); // for other team it is not a listen server, but externalpeer
				pPeerCommand->SetInt( "server/team", 2 );	// other team is nominated team 2
				
				LinkSysSession()->Command( pPeerCommand );
			}
			m_iLinkState = STATE_LINK_FINISHED;
		}
		break;
	}
}

void CMatchSessionOnlineTeamSearchLinkBase::LinkCommand( KeyValues *pCommand )
{
	char const *szCommand = pCommand->GetName();

	if ( !Q_stricmp( "TeamSearchLink::Dedicated", szCommand ) ||
		 !Q_stricmp( "TeamSearchLink::ListenClient", szCommand ) )
	{
		Assert( m_iLinkState == STATE_WAITING_FOR_PEER_SERVER );
		KeyValues *pServerInfo = pCommand->FindKey( "server", false );
		if ( !pServerInfo )
		{
			ResetAndRestartTeamSearch();
			return;
		}

		// Apply host session update packet
		ApplyHostSessionUpdatePacket();
		
		// Notify our team about the server
		if ( KeyValues *pOurTeamNotify = new KeyValues( CFmtStr( "TeamSearchResult::%s", szCommand + strlen( "TeamSearchLink::" ) ) ) )
		{
			pOurTeamNotify->AddSubKey( pServerInfo->MakeCopy() );
			OnSearchEvent( pOurTeamNotify );
		}
		return;
	}
}

void CMatchSessionOnlineTeamSearchLinkBase::StartHostingListenServer()
{
	if ( mm_teamsearch_nostart.GetBool() )
	{
		// Fake-cancel the session for debugging
		m_pHostSession->Command( KeyValues::AutoDeleteInline( new KeyValues( "Cancel" ) ) );
		return;
	}

	m_iLinkState = STATE_HOSTING_LISTEN_SERVER;

	// Update host session settings packet
	ApplyHostSessionUpdatePacket();
	OnSearchEvent( new KeyValues( "TeamSearchResult::ListenHost" ) );
}

void CMatchSessionOnlineTeamSearchLinkBase::StartDedicatedServerSearch()
{
	if ( mm_teamsearch_nostart.GetBool() )
	{
		// Fake-cancel the session for debugging
		m_pHostSession->Command( KeyValues::AutoDeleteInline( new KeyValues( "Cancel" ) ) );
		return;
	}

	m_iLinkState = STATE_SEARCHING_DEDICATED;

	// Notify everybody that we are searching for dedicated server now
	OnSearchEvent( new KeyValues(
		"OnMatchSessionUpdate",
			"state", "progress",
			"progress", "dedicated"
		) );

	m_pDsSearcher = new CDsSearcher( m_pSettings, m_pHostSession->GetSessionSystemData()->GetUint64( "xuidReserve" ), NULL );
}

void CMatchSessionOnlineTeamSearchLinkBase::OnDedicatedSearchFinished()
{
	Assert( m_pDsSearcher );
	CDsSearcher::DsResult_t dsResult = m_pDsSearcher->GetResult();

	m_pDsSearcher->Destroy();
	m_pDsSearcher = NULL;

	if ( !dsResult.m_bDedicated )
	{
		StartHostingListenServer();
		return;
	}

	//
	// Serialize the information about the dedicated server
	//
	KeyValues *pServerInfo = new KeyValues( "server" );
	dsResult.CopyToServerKey( pServerInfo );
	pServerInfo->SetUint64( "reservationid", m_pHostSession->GetSessionSystemData()->GetUint64( "xuidReserve" ) );

	// Apply host session update packet
	ApplyHostSessionUpdatePacket();

	//
	// Notify the peer team about the server
	//
	if ( KeyValues *pPeerCommand = new KeyValues( "TeamSearchLink::Dedicated" ) )
	{
		KeyValues::AutoDelete autodelete( pPeerCommand );
		pPeerCommand->SetString( "run", "xuid" );
		pPeerCommand->SetUint64( "runxuid", m_xuidLinkPeer );
		pServerInfo->SetInt( "team", 2 );
		pPeerCommand->AddSubKey( pServerInfo->MakeCopy() );
		LinkSysSession()->Command( pPeerCommand );
	}

	// Notify our team about the server
	if ( KeyValues *pOurTeamNotify = new KeyValues( "TeamSearchResult::Dedicated" ) )
	{
		pOurTeamNotify->SetPtr( "dsresult", &dsResult );
		pServerInfo->SetInt( "team", 1 );
		pOurTeamNotify->AddSubKey( pServerInfo );
		OnSearchEvent( pOurTeamNotify );
	}

	m_iLinkState = STATE_LINK_FINISHED;
}

void CMatchSessionOnlineTeamSearchLinkBase::StartWaitingForPeerServer()
{
	m_iLinkState = STATE_WAITING_FOR_PEER_SERVER;

	// Notify everybody that peer is searching for game server
	OnSearchEvent( new KeyValues(
		"OnMatchSessionUpdate",
			"state", "progress",
			"progress", "peerserver"
		) );
}



//////////////////////////////////////////////////////////////////////////
//
// Link host implementation
//
//

void CMatchSessionOnlineTeamSearchLinkHost::LinkInit()
{
	m_iLinkState = STATE_SUBMIT_STATS;
}

void CMatchSessionOnlineTeamSearchLinkHost::LinkUpdate()
{
	switch ( m_iLinkState )
	{
	case STATE_SUBMIT_STATS:
		{
#ifdef _X360
			m_pXlspConnection = new CXlspConnection( false );
			CUtlVector< KeyValues * > arrCommands;
#endif
			if ( KeyValues *pCmd = new KeyValues( "stat_agg" ) )
			{
				int numSeconds = int( 1 + Plat_FloatTime() - m_flCreationTime );
				numSeconds = ClampArrayBounds( numSeconds, 30 * 60 ); // 30 minutes
				pCmd->SetInt( "search_team_time", numSeconds );

#ifdef _X360
				arrCommands.AddToTail( pCmd );
#elif !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
				CGCClientJobUpdateStats *pJob = new CGCClientJobUpdateStats( pCmd );
				pJob->StartJob( NULL );
#else
				pCmd->deleteThis();
#endif

			}
#ifdef _X360
			m_pXlspCommandBatch = new CXlspConnectionCmdBatch( m_pXlspConnection, arrCommands );
#endif
		}
		m_iLinkState = STATE_REPORTING_STATS;
		break;

	case STATE_REPORTING_STATS:
#ifdef _X360
		m_pXlspCommandBatch->Update();
		if ( !m_pXlspCommandBatch->IsFinished() )
			return;

		m_pXlspCommandBatch->Destroy();
		m_pXlspCommandBatch = NULL;

		m_pXlspConnection->Destroy();
		m_pXlspConnection = NULL;
#endif
		m_iLinkState = STATE_CONFIRM_JOIN;
		break;

	case STATE_CONFIRM_JOIN:
		if ( KeyValues *pPeerCommand = new KeyValues( "TeamSearchLink::HostConfirmJoinReady" ) )
		{
			KeyValues::AutoDelete autodelete( pPeerCommand );
			pPeerCommand->SetString( "run", "xuid" );
			pPeerCommand->SetUint64( "runxuid", m_xuidLinkPeer );
			pPeerCommand->AddSubKey( m_pHostSession->GetSessionSettings()->MakeCopy() );	// Submit a copy of our session settings as well
			m_pSysSessionHost->Command( pPeerCommand );
		}
		m_iLinkState = STATE_CONFIRM_JOIN_WAIT;
		break;

	case STATE_CONFIRM_JOIN_WAIT:
		// Waiting for client to tell us how to select server
		break;
	}

	CMatchSessionOnlineTeamSearchLinkBase::LinkUpdate();
}

void CMatchSessionOnlineTeamSearchLinkHost::LinkCommand( KeyValues *pCommand )
{
	char const *szCommand = pCommand->GetName();

	if ( !Q_stricmp( "TeamSearchLink::TeamLinkSessionUpdate", szCommand ) )
	{
		if ( KeyValues *pUpdatePkt = pCommand->GetFirstTrueSubKey() )
		{
			m_pSettings->MergeFrom( pUpdatePkt );
			RememberHostSessionUpdatePacket( pUpdatePkt->MakeCopy() );
		}
	}
	else if ( !Q_stricmp( "TeamSearchLink::HostHosting", szCommand ) )
	{
		// We are going to host
		char const *szLinkHostServer = m_pSettings->GetString( "options/server", "" );
		if ( !Q_stricmp( szLinkHostServer, "listen" ) )
			StartHostingListenServer();
		else
			StartDedicatedServerSearch();
		return;
	}
	else if ( !Q_stricmp( "TeamSearchLink::ClientHosting", szCommand ) )
	{
		// Our peer is going to host
		StartWaitingForPeerServer();
		return;
	}

	CMatchSessionOnlineTeamSearchLinkBase::LinkCommand( pCommand );
}


//////////////////////////////////////////////////////////////////////////
//
// Link client implementation
//
//

void CMatchSessionOnlineTeamSearchLinkClient::LinkInit()
{
	m_iLinkState = STATE_WAITING_FOR_HOST_READY;
}

void CMatchSessionOnlineTeamSearchLinkClient::LinkUpdate()
{
	switch ( m_iLinkState )
	{
	case STATE_WAITING_FOR_HOST_READY:
		// We are waiting for host to report statistics and get the merged teamlink lobby ready
		return;

	case STATE_CONFIRM_JOIN:
		// m_pSettings has been set to the aggregate of host settings
		// and our members, so we can inspect host's preference of the Server to
		// decide who is going to search for the game server
		{
			char const *szLinkHostServer = m_pSettings->GetString( "options/server", "" );
			char const *szOurServer = m_pHostSession->GetSessionSettings()->GetString( "options/server", "" );
			if ( Q_stricmp( szLinkHostServer, "listen" ) &&
				!Q_stricmp( szOurServer, "listen" ) )
			{
				// Host doesn't care, but we need listen server
				m_pSysSessionClient->Command( KeyValues::AutoDeleteInline( new KeyValues(
					"TeamSearchLink::ClientHosting", "run", "host" ) ) );
				StartHostingListenServer();
			}
			else
			{
				// Let host find the server
				m_pSysSessionClient->Command( KeyValues::AutoDeleteInline( new KeyValues(
					"TeamSearchLink::HostHosting", "run", "host" ) ) );
				StartWaitingForPeerServer();
			}
		}
		return;
	}

	CMatchSessionOnlineTeamSearchLinkBase::LinkUpdate();
}

void CMatchSessionOnlineTeamSearchLinkClient::LinkCommand( KeyValues *pCommand )
{
	char const *szCommand = pCommand->GetName();

	if ( !Q_stricmp( "TeamSearchLink::HostConfirmJoinReady", szCommand ) )
	{
		// Host has sent us full settings of the host session,
		// let's try to reconcile and summarize the settings and prepare an update package if needed
		KeyValues *pRemoteSettings = pCommand->GetFirstTrueSubKey();
		KeyValues *pLocalSettings = m_pHostSession->GetSessionSettings();

		if ( KeyValues *pUpdatePkt = g_pMMF->GetMatchTitleGameSettingsMgr()->PrepareTeamLinkForGame( pLocalSettings, pRemoteSettings ) )
		{
			if ( KeyValues *pHostCmd = new KeyValues( "TeamSearchLink::TeamLinkSessionUpdate", "run", "host" ) )
			{
				pHostCmd->AddSubKey( pUpdatePkt->MakeCopy() );
				m_pSysSessionClient->Command( KeyValues::AutoDeleteInline( pHostCmd ) );
			}

			m_pSettings->MergeFrom( pUpdatePkt );
			RememberHostSessionUpdatePacket( pUpdatePkt );
		}

		// Host is now ready
		if ( m_iLinkState == STATE_WAITING_FOR_HOST_READY )
			m_iLinkState = STATE_CONFIRM_JOIN;
		return;
	}

	CMatchSessionOnlineTeamSearchLinkBase::LinkCommand( pCommand );
}

