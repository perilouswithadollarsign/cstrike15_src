//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_framework.h"

#include "fmtstr.h"

#include "netmessages_signon.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Disable the creation of a listen server for online play.
ConVar mm_disable_listen_server( "mm_disable_listen_server", "0", FCVAR_DEVELOPMENTONLY );

//
// CMatchSessionOnlineHost
//
// Implementation of an online session of a host machine
//

CMatchSessionOnlineHost::CMatchSessionOnlineHost( KeyValues *pSettings ) :
	m_pSettings( pSettings->MakeCopy() ),
	m_autodelete_pSettings( m_pSettings ),
	m_pSysData( new KeyValues( "SysSessionData", "type", "host" ) ),
	m_autodelete_pSysData( m_pSysData ),
	m_eState( STATE_INIT ),
	m_pSysSession( NULL ),
	m_pDsSearcher( NULL ),
	m_pTeamSearcher( NULL ),
	m_pMatchSearcher( NULL )
{
	DevMsg( "Created CMatchSessionOnlineHost:\n" );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );

	// Generate an encryption cookie
	unsigned char chEncryptionCookie[ sizeof( uint64 ) ] = {};
	for ( int j = 0; j < ARRAYSIZE( chEncryptionCookie ); ++ j )
		chEncryptionCookie[j] = RandomInt( 1, 254 );
	m_pSysData->SetUint64( "crypt", * reinterpret_cast< uint64 * >( chEncryptionCookie ) );

	InitializeGameSettings();
}

CMatchSessionOnlineHost::CMatchSessionOnlineHost( CSysSessionClient *pSysSession, KeyValues *pExtendedSettings ) :
	m_pSettings( pExtendedSettings->FindKey( "settings" ) ),
	m_autodelete_pSettings( m_pSettings ),
	m_pSysData( new KeyValues( "SysSessionData", "type", "host" ) ),
	m_autodelete_pSysData( m_pSysData ),
	m_eState( STATE_LOBBY ), // it's at least lobby, we'll figure out later
	m_pSysSession( NULL ),
	m_pDsSearcher( NULL ),
	m_pTeamSearcher( NULL ),
	m_pMatchSearcher( NULL )
{
	Assert( m_pSettings );

	KeyValues::AutoDelete autodelete( pExtendedSettings );
	pExtendedSettings->RemoveSubKey( m_pSettings ); // it's now our settings

	// Carry over encryption cookie
	uint64 ullCrypt = pExtendedSettings->GetUint64( "crypt" );
	if ( ullCrypt )
		m_pSysData->SetUint64( "crypt", ullCrypt );

	// Install our session
	g_pMMF->SetCurrentMatchSession( this );

	// Check if the game is in a non-lobby state
	char const *szState = pExtendedSettings->GetString( "state", "" );
	if ( !Q_stricmp( szState, "game" ) )
		m_eState = STATE_GAME;
	else if ( !Q_stricmp( szState, "ending" ) )
		m_eState = STATE_ENDING;

	// Now we need to create the system session to reflect the client session passed
	m_pSysSession = new CSysSessionHost( pSysSession, m_pSettings );
	if ( ullCrypt )
		m_pSysSession->SetCryptKey( ullCrypt );
	pSysSession->Destroy();

	// Now we need to clean up some transient leftovers from incomplete operations
	// started by previous host
	MigrateGameSettings();

	// Show the state
	DevMsg( "Migrated into CMatchSessionOnlineHost:\n" );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );
}

CMatchSessionOnlineHost::~CMatchSessionOnlineHost()
{
	DevMsg( "Destroying CMatchSessionOnlineHost:\n" );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );
}

KeyValues * CMatchSessionOnlineHost::GetSessionSystemData()
{
	// Setup our sys data
	m_pSysData->SetUint64( "xuidReserve", m_pSysSession ? m_pSysSession->GetReservationCookie() : 0ull );
	m_pSysData->SetUint64( "xuidHost", m_pSysSession ? m_pSysSession->GetHostXuid() : 0ull );

	switch ( m_eState )
	{
	case STATE_LOBBY:
		m_pSysData->SetString( "state", "lobby" );
		break;
	case STATE_GAME:
		m_pSysData->SetString( "state", "game" );
		break;
	default:
		m_pSysData->SetString( "state", "" );
		break;
	}

	return m_pSysData;
}

KeyValues * CMatchSessionOnlineHost::GetSessionSettings()
{
	return m_pSettings;
}

void CMatchSessionOnlineHost::UpdateSessionSettings( KeyValues *pSettings )
{
	if ( m_eState < STATE_LOBBY )
	{
		Warning( "CMatchSessionOnlineHost::UpdateSessionSettings is unavailable in state %d!\n", m_eState );
		Assert( !"CMatchSessionOnlineHost::UpdateSessionSettings is unavailable!\n" );
		return;
	}

	// Let the title extend the game update keys
	g_pMMF->GetMatchTitleGameSettingsMgr()->ExtendGameSettingsUpdateKeys( m_pSettings, pSettings );
	m_pSettings->MergeFrom( pSettings );

	DevMsg( "CMatchSessionOnlineHost::UpdateSessionSettings\n");
	KeyValuesDumpAsDevMsg( m_pSettings );

	if ( m_pSysSession )
	{
		m_pSysSession->UpdateMembersInfo();
		m_pSysSession->OnUpdateSessionSettings( pSettings );
	}

	// Broadcast the update to everybody interested
	MatchSession_BroadcastSessionSettingsUpdate( pSettings );
}

void CMatchSessionOnlineHost::UpdateTeamProperties( KeyValues *pSettings )
{
	m_pSysSession->UpdateTeamProperties( pSettings );
}

void CMatchSessionOnlineHost::Command( KeyValues *pCommand )
{
	char const *szCommand, *szRun;
	szCommand = pCommand->GetName();
	szRun = pCommand->GetString( "run", "" );

	if ( !Q_stricmp( szRun, "all" ) || !Q_stricmp( szRun, "clients" ) || !Q_stricmp( szRun, "xuid" ) )
	{
		if ( m_pSysSession )
		{
			m_pSysSession->Command( pCommand );
			return;
		}
	}
	else if ( !*szRun || !Q_stricmp( szRun, "local" ) || !Q_stricmp( szRun, "host" ) )
	{
		OnRunCommand( pCommand );
		return;
	}

	Warning( "CMatchSessionOnlineClient::Command( %s ) unhandled!\n", szCommand );
	Assert( !"CMatchSessionOnlineClient::Command" );
}

void CMatchSessionOnlineHost::OnRunCommand( KeyValues *pCommand )
{
	char const *szCommand = pCommand->GetName();

	if ( !Q_stricmp( "Start", szCommand ) )
	{
		if ( ( m_eState == STATE_LOBBY ) && !pCommand->GetUint64( "_remote_xuidsrc" ) )
		{
			OnRunCommand_Start();
			return;
		}
	}
	if ( !Q_stricmp( "Match", szCommand ) )
	{
		if ( ( m_eState == STATE_LOBBY ) && !pCommand->GetUint64( "_remote_xuidsrc" ) )
		{
			OnRunCommand_Match();
			return;
		}
	}
	if ( !Q_stricmp( "Cancel", szCommand ) )
	{
		if (  !pCommand->GetUint64( "_remote_xuidsrc" ) )
		{
			switch ( m_eState )
			{
			case STATE_STARTING:
				OnRunCommand_Cancel_DsSearch();
				return;
			case STATE_MATCHING:
				OnRunCommand_Cancel_Match();
				return;
			}
		}
	}
	if ( !Q_stricmp( "Kick", szCommand ) )
	{
		if ( m_pSysSession && !pCommand->GetUint64( "_remote_xuidsrc" ) )
		{
			m_pSysSession->KickPlayer( pCommand );
			return;
		}
	}
	if ( !Q_stricmp( "Migrate", szCommand ) )
	{
		if ( m_pSysSession ) // TODO: research who sends the "Migrate" command and how secure it is?
		{
			m_pSysSession->Migrate( pCommand );
			return;
		}
	}
	if ( !Q_stricmp( "QueueConnect", szCommand ) )
	{
		if ( ( m_eState == STATE_LOBBY ) && !pCommand->GetUint64( "_remote_xuidsrc" ) )
		{
			OnRunCommand_QueueConnect( pCommand );
			return;
		}
	}

	//
	// Let the title-specific matchmaking handle the command
	//
	CUtlVector< KeyValues * > arrPlayersUpdated;
	arrPlayersUpdated.SetCount( m_pSettings->GetInt( "members/numPlayers", 0 ) );
	memset( arrPlayersUpdated.Base(), 0, arrPlayersUpdated.Count() * sizeof( KeyValues * ) );

	g_pMMF->GetMatchTitleGameSettingsMgr()->ExecuteCommand( pCommand, GetSessionSystemData(), m_pSettings, arrPlayersUpdated.Base() );

	// Now notify the framework about player updated
	for ( int k = 0; k < arrPlayersUpdated.Count(); ++ k )
	{
		if ( !arrPlayersUpdated[k] )
			break;

		Assert( m_pSysSession );
		if ( m_pSysSession )
		{
			m_pSysSession->OnPlayerUpdated( arrPlayersUpdated[k] );
		}
	}

	//
	// Send the command as event for handling
	//
	KeyValues *pEvent = pCommand->MakeCopy();
	pEvent->SetName( CFmtStr( "Command::%s", pCommand->GetName() ) );
	g_pMatchEventsSubscription->BroadcastEvent( pEvent );
}

void CMatchSessionOnlineHost::OnRunCommand_Start()
{
	// First of all flip our state
	m_eState = STATE_STARTING;

	// Now modify the state of our game
	UpdateSessionSettings( KeyValues::AutoDeleteInline ( KeyValues::FromString(
		"update",
		" update { "
			" system { "
				" lock starting "
			" } "
		" } "
		) ) );

	// Prepare lobby for game
	OnGamePrepareLobbyForGame();

	// Search for a server
	m_pDsSearcher = new CDsSearcher( m_pSettings, m_pSysSession->GetReservationCookie(), this );
}

void CMatchSessionOnlineHost::OnRunCommand_Match()
{
	// First of all flip our state
	m_eState = STATE_MATCHING;

	// Now modify the state of our game
	UpdateSessionSettings( KeyValues::AutoDeleteInline ( KeyValues::FromString(
		"update",
		" update { "
			" system { "
				" lock matching "
			" } "
		" } "
		) ) );

	// Prepare lobby for game
	OnGamePrepareLobbyForGame();

	// Search for opponents
	if ( m_pTeamSearcher )
	{
		m_pTeamSearcher->Destroy();
		m_pTeamSearcher = NULL;
	}

	if ( m_pMatchSearcher )
	{
		m_pMatchSearcher->Destroy();
		m_pMatchSearcher = NULL;
	}

	KeyValues *teamMatch = m_pSettings->FindKey( "options/conteammatch" );
	if ( teamMatch )
	{
		const char *serverType = m_pSettings->GetString( "options/server", "official" );
		if ( Q_stricmp( serverType, "listen" ) )
		{
			m_pMatchSearcher = new CMatchSessionOnlineSearch( m_pSettings );
		}
		else
		{
			// Transition into loading state
			m_eState = STATE_LOADING;
			StartListenServerMap();
		}
	}
	else
	{
		m_pTeamSearcher = new CMatchSessionOnlineTeamSearch( m_pSettings, this );
	}
}

void CMatchSessionOnlineHost::OnRunCommand_Cancel_Match()
{
	// Destroy the matchmaking search state
	Assert( m_pTeamSearcher );
	if ( m_pTeamSearcher )
	{
		m_pTeamSearcher->Destroy();
		m_pTeamSearcher = NULL;
	}

	if ( m_pMatchSearcher )
	{
		m_pMatchSearcher->Destroy();
		m_pMatchSearcher = NULL;
	}

	// Flip the state back to lobby
	m_eState = STATE_LOBBY;

	// Now unlock the state of our game
	UpdateSessionSettings( KeyValues::AutoDeleteInline ( KeyValues::FromString(
		"update",
		" update { "
			" system { "
				" lock #empty# "
			" } "
		" } "
		) ) );
}

void CMatchSessionOnlineHost::OnRunCommand_Cancel_DsSearch()
{
	// Destroy the dedicated search state
	Assert( m_pDsSearcher );
	if ( m_pDsSearcher )
	{
		m_pDsSearcher->Destroy();
		m_pDsSearcher = NULL;
	}

	// Flip the state back to lobby
	m_eState = STATE_LOBBY;

	// Now unlock the state of our game
	UpdateSessionSettings( KeyValues::AutoDeleteInline ( KeyValues::FromString(
		"update",
		" update { "
			" system { "
				" lock #empty# "
			" } "
		" } "
		) ) );
}

void CMatchSessionOnlineHost::OnRunCommand_StartDsSearchFinished()
{
	// Retrieve the result and destroy the searcher
	Assert( m_pDsSearcher );

	CDsSearcher::DsResult_t dsResult = m_pDsSearcher->GetResult();

	if ( dsResult.m_bAborted )
	{
		OnRunCommand_Cancel_DsSearch();
		return;
	}

	m_pDsSearcher->Destroy();
	m_pDsSearcher = NULL;

	// Handle console team matchmaking case here - if we did not find a ds then
	// just go back to the lobby
	KeyValues *teamMatch = m_pSettings->FindKey( "options/conteammatch" );
	if ( teamMatch && !dsResult.m_bDedicated )
	{
		OnRunCommand_Cancel_DsSearch();
		OnRunCommand_Match(); // restart the search
		return;
	}

#if defined _GAMECONSOLE

	if ( !dsResult.m_bDedicated &&
		m_pSettings->GetString( "server/server", NULL ) )
	{
		// We should be connecting to the dedicated server, but we
		// failed to reserve it, just bail out with an error
		KeyValues *notify = new KeyValues( "mmF->SysSessionUpdate" );
		notify->SetPtr( "syssession", m_pSysSession );
		notify->SetString( "error", "n/a" );
		g_pMatchEventsSubscription->BroadcastEvent( notify );
		return;
	}

	if ( !dsResult.m_bDedicated )
	{
		// Transition into loading state
		m_eState = STATE_LOADING;
		StartListenServerMap();
		return;
	}

#else

	// On PC if we fail to find or reserve a DS then bail
	if ( !dsResult.m_bDedicated )
	{
		// We should be connecting to the dedicated server, but we
		// failed to reserve it, just bail out with an error
		KeyValues *notify = new KeyValues( "mmF->SysSessionUpdate" );
		notify->SetPtr( "syssession", m_pSysSession );
		notify->SetString( "error", "Could not find or connect to a DS" );
		g_pMatchEventsSubscription->BroadcastEvent( notify );
		return;
	}

#endif
	//
	// We have reserved a dedicated server
	//

	// Prepare the update - creating the "server" key signals that
	// a game server is available
	KeyValues *kvUpdate = KeyValues::FromString(
		"update",
		" update { "
			" system { "
				" lock #empty# "
			" } "
			" server { "
				" server dedicated "
			" } "
		" } "
		);
	KeyValues::AutoDelete autodelete( kvUpdate );
	KeyValues *kvServer = kvUpdate->FindKey( "update/server" );

	//
	// Publish the addresses of the server
	//
	bool bWasEncrypted = ( '$' == m_pSettings->GetString( "server/adronline" )[0] );
	dsResult.CopyToServerKey( kvServer, bWasEncrypted ? m_pSysData->GetUint64( "crypt" ) : 0ull );

	// Remove the lock from the session and allow joins and trigger clients connect
	UpdateSessionSettings( kvUpdate );

	// Add server info to lobby settings
	m_pSysSession->UpdateServerInfo( m_pSettings );

	//
	// Actually connect to game server
	//
	ConnectGameServer( &dsResult );
}

void CMatchSessionOnlineHost::OnRunCommand_StartListenServerStarted( uint32 externalIP )
{
	// If this is a team match and we do not yet have a public IP, because the server
	// logon is not complete, then return and wait for another call when the
	// server public IP is available
// 	KeyValues *teamMatch = m_pSettings->FindKey( "options/conteammatch" );
// 	if ( teamMatch != NULL ) // <vitaliy - we probably don't care here, we'll use P2P anyways in public> && externalIP == 0)
// 	{
// 		return;
// 	}

	// Switch the state
	m_eState = STATE_GAME;

	// Prepare the update - creating the "server" key signals that
	// a game server is available
	KeyValues *kvUpdate = KeyValues::FromString(
		"update",
		" update { "
			" system { "
				" lock #empty# "
			" } "
			" server { "
				" server listen "
			" } "
		" } "
		);
	KeyValues::AutoDelete autodelete( kvUpdate );
	KeyValues *kvServer = kvUpdate->FindKey( "update/server" );

	if ( !IsX360() )
	{
		// Let everybody know server info of the server that they should join
		INetSupport::ServerInfo_t si;
		memset( &si, 0, sizeof( si ) );
		g_pMatchExtensions->GetINetSupport()->GetServerInfo( &si );

		if ( externalIP != 0)
		{
			si.m_netAdrOnline.SetIP( externalIP );
		}

		kvServer->SetString( "adrlocal", MatchSession_EncryptAddressString( si.m_netAdr.ToString(), m_pSysData->GetUint64( "crypt" ) ) );
		kvServer->SetString( "adronline", MatchSession_EncryptAddressString( si.m_netAdrOnline.ToString(), m_pSysData->GetUint64( "crypt" ) ) );

		// For listen servers we also expose our Steam ID for libjingle
		kvServer->SetUint64( "xuid", g_pPlayerManager->GetLocalPlayer( XBX_GetPrimaryUserId() )->GetXUID() );
	}
	
	// Set the server reservation appropriately for clients to connect
	uint64 uiReservationCookie = m_pSysSession->GetReservationCookie();
	g_pMatchExtensions->GetINetSupport()->UpdateServerReservation( uiReservationCookie );
	
	kvServer->SetUint64( "reservationid", uiReservationCookie );
	if ( m_pTeamSearcher )	// for team-on-team game the listen server is team 1
		kvServer->SetInt( "team", 1 );

	if ( m_pTeamSearcher )
	{
		if ( CSysSessionHost *pSysSessionHost = dynamic_cast< CSysSessionHost * >( m_pTeamSearcher->LinkSysSession() ) )
		{
#ifdef _X360
			char chSessionInfo[ XSESSION_INFO_STRING_LENGTH ] = {0};
			pSysSessionHost->GetHostSessionInfo( chSessionInfo );
			kvServer->SetString( "sessioninfo", chSessionInfo );
#endif
		}
	}

	// Remove the lock from the session and allow joins and trigger clients connect
	UpdateSessionSettings( kvUpdate );

	// Add server info to lobby settings
	m_pSysSession->UpdateServerInfo( m_pSettings );
	
	// Mark the local session as active
	SetSessionActiveGameplayState( true, NULL );

	// Run the extra Update on the teamsearcher if it is alive
	if ( m_pTeamSearcher )
		m_pTeamSearcher->Update();

	InviteTeam();
}

void CMatchSessionOnlineHost::OnRunCommand_QueueConnect( KeyValues *pCommand )
{
	char const *szConnectAddress = pCommand->GetString( "adronline", "0.0.0.0" );
	uint64 uiReservationId = pCommand->GetUint64( "reservationid" );
	bool bAutoCloseSession = pCommand->GetBool( "auto_close_session" );

	// Switch the state
	m_eState = STATE_GAME;

	MatchSession_PrepareClientForConnect( m_pSettings, uiReservationId );

	// Mark gameplay state as active
	SetSessionActiveGameplayState( true, szConnectAddress );

	// Close the session, potentially resetting a bunch of state
	if ( bAutoCloseSession )
		g_pMatchFramework->CloseSession();

	// Determine reservation settings required
	g_pMatchExtensions->GetINetSupport()->UpdateClientReservation( uiReservationId, 0ull );

	// Issue the connect command
	g_pMatchExtensions->GetIVEngineClient()->StartLoadingScreenForCommand( CFmtStr( "connect %s", szConnectAddress ) );
}

void CMatchSessionOnlineHost::ConnectGameServer( CDsSearcher::DsResult_t *pDsResult )
{
	// Switch the state
	m_eState = STATE_GAME;

	MatchSession_PrepareClientForConnect( m_pSettings );

	//
	// Resolve server information
	//
	MatchSessionServerInfo_t msInfo = {0};
	if ( pDsResult )
		msInfo.m_dsResult = *pDsResult;

	// Flags
	uint uiResolveServerInfoFlags = msInfo.RESOLVE_CONNECTSTRING | msInfo.RESOLVE_QOS_RATE_PROBE;
	if ( !pDsResult )
		uiResolveServerInfoFlags |= msInfo.RESOLVE_DSRESULT;
	if ( m_pTeamSearcher )
		uiResolveServerInfoFlags |= msInfo.RESOLVE_ALLOW_EXTPEER;

	// Client session
	CSysSessionBase *pSysSessionForGameServer = m_pSysSession;
	if ( m_pTeamSearcher )
		pSysSessionForGameServer = m_pTeamSearcher->LinkSysSession();

	if ( !MatchSession_ResolveServerInfo( m_pSettings, pSysSessionForGameServer, msInfo, uiResolveServerInfoFlags, m_pSysData->GetUint64( "crypt" ) ) )
	{
		if ( m_pTeamSearcher )
		{
			m_eState = STATE_MATCHINGRESTART;
			return;
		}

		// Destroy the session
		m_pSysSession->Destroy();
		m_pSysSession = NULL;

		// Handle error
		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate",
			"state", "error", "error", "connect" ) );
		return;
	}

	// Determine reservation settings required
	g_pMatchExtensions->GetINetSupport()->UpdateClientReservation( msInfo.m_uiReservationCookie, msInfo.m_xuidJingle );

	// Mark gameplay state as active
	SetSessionActiveGameplayState( true, msInfo.m_szSecureServerAddress );

	// Issue the connect command
	g_pMatchExtensions->GetIVEngineClient()->StartLoadingScreenForCommand( msInfo.m_szConnectCmd );	

	// Tell the rest of the team
	InviteTeam();
}

void CMatchSessionOnlineHost::StartListenServerMap()
{
	UpdateSessionSettings( KeyValues::AutoDeleteInline ( KeyValues::FromString(
		"update",
		" update { "
			" system { "
				" lock loading "
			" } "
		" } "
		" delete { "
			" server delete "
		" } "
		) ) );

	// Note: in case of a team-on-team game we don't yet have "server" key
	// and don't put the explicit team nomination, but the title must designate
	// the listen server host to be on team 1.
	MatchSession_PrepareClientForConnect( m_pSettings );

	// Before starting a listen server map ensure we have the map name set
	g_pMMF->GetMatchTitleGameSettingsMgr()->SetBspnameFromMapgroup( m_pSettings );

	if ( !mm_disable_listen_server.GetBool() )
	{
		bool bResult = g_pMatchFramework->GetMatchTitle()->StartServerMap( m_pSettings );
		if ( !bResult )
		{
			Warning( "Failed to start server map!\n" );
			KeyValuesDumpAsDevMsg( m_pSettings, 1 );
			Assert( 0 );
			g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "error", "error", "nomap" ) );
		}
		Msg( "Succeeded in starting server map!\n" );
	}
	else
	{
		Msg( "Failed to start server map because mm_disable_listen_server was enabled.\n" );
		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "error", "error", "listen server disabled" ) );
	}
}

void CMatchSessionOnlineHost::OnEndGameToLobby()
{
	m_eState = STATE_ENDING;

	// Remove server information
	UpdateSessionSettings( KeyValues::AutoDeleteInline( KeyValues::FromString(
		"updatedelete",
		" update { "
			" system { "
				" lock endgame "
			" } "
		" } "
		" delete { "
			" server delete "
		" } "
		) ) );

	// Issue the disconnect command
	g_pMatchExtensions->GetIVEngineClient()->ExecuteClientCmd( "disconnect" );
	g_pMatchExtensions->GetINetSupport()->UpdateServerReservation( 0ull );

	// Mark gameplay state as inactive
	SetSessionActiveGameplayState( false, NULL );
}

void CMatchSessionOnlineHost::SetSessionActiveGameplayState( bool bActive, char const *szSecureServerAddress )
{
	if ( m_pTeamSearcher )
	{
		// Mark gameplay state as inactive
		if ( !bActive )
		{
			m_pTeamSearcher->Destroy();
			m_pTeamSearcher = NULL;
		}
		else
		{
			// TODO: m_pTeamSearcher->SetSessionActiveGameplayState
		}
	}

	m_pSysSession->SetSessionActiveGameplayState( bActive, szSecureServerAddress );
}

void CMatchSessionOnlineHost::OnGamePrepareLobbyForGame()
{
	// Remember which players will get updated
	CUtlVector< KeyValues * > arrPlayersUpdated;
	arrPlayersUpdated.SetCount( m_pSettings->GetInt( "members/numPlayers", 0 ) );
	memset( arrPlayersUpdated.Base(), 0, arrPlayersUpdated.Count() * sizeof( KeyValues * ) );

	g_pMMF->GetMatchTitleGameSettingsMgr()->PrepareLobbyForGame( m_pSettings, arrPlayersUpdated.Base() );

	//
	// Now notify the framework about player updated
	//

	for ( int k = 0; k < arrPlayersUpdated.Count(); ++ k )
	{
		if ( !arrPlayersUpdated[k] )
			break;

		Assert( m_pSysSession );
		if ( m_pSysSession )
		{
			m_pSysSession->OnPlayerUpdated( arrPlayersUpdated[k] );
		}
	}
}

void CMatchSessionOnlineHost::OnGamePlayerMachinesConnected( int numMachines )
{
	if ( m_eState != STATE_GAME )
		return;

	// Remember which players will get updated
	CUtlVector< KeyValues * > arrPlayersUpdated;
	arrPlayersUpdated.SetCount( m_pSettings->GetInt( "members/numPlayers", 0 ) );
	memset( arrPlayersUpdated.Base(), 0, arrPlayersUpdated.Count() * sizeof( KeyValues * ) );

	g_pMMF->GetMatchTitleGameSettingsMgr()->PrepareLobbyForGame( m_pSettings, arrPlayersUpdated.Base() );

#ifdef _DEBUG
	// Theoretically only new machines should be affected by the callback,
	// so in debug mode we are verifying that.
	// The logic is actually somewhat complicated - see sys_session implementation
	// of order in which OnPlayerMachinesConnected and OnPlayerUpdated are fired.
	// All adjustments to the connecting players should be made before OnPlayerUpdated
	// gets fired so that OnPlayerUpdated("joined") was fired with all valid settings.

	// Current total number of machines
	int numMachinesTotal = m_pSettings->GetInt( "members/numMachines", 0 );

	// For the players from the old machines we would need to send updates
	for ( int k = 0; k < arrPlayersUpdated.Count(); ++ k )
	{
		if ( !arrPlayersUpdated[k] )
			break;
		
		bool bNewMachine = false;
		XUID xuidPlayer = arrPlayersUpdated[k]->GetUint64( "xuid" );
		KeyValues *pMachine = NULL;
		SessionMembersFindPlayer( m_pSettings, xuidPlayer, &pMachine );
		if ( pMachine )
		{
			char const *szMachine = pMachine->GetName();
			if ( char const *szMachineNumber = StringAfterPrefix( szMachine, "machine" ) )
			{
				int iMachineNumber = atoi( szMachineNumber );
				if ( iMachineNumber >= numMachinesTotal - numMachines )
					bNewMachine = true;
			}
		}

		Assert( bNewMachine );
		if ( !bNewMachine )
		{
			Assert( m_pSysSession );
			if ( m_pSysSession )
			{
				m_pSysSession->OnPlayerUpdated( arrPlayersUpdated[k] );
			}
		}
	}
#endif
}

uint64 CMatchSessionOnlineHost::GetSessionID()
{
	if( m_pSysSession )
	{
		return m_pSysSession->GetSessionID();
	}

	return 0;
}

void CMatchSessionOnlineHost::InviteTeam()
{
	return; // this path is no longer needed

	DevMsg( "InviteTeam\n" );
	KeyValuesDumpAsDevMsg( m_pSettings );

	KeyValues *teamMatch = m_pSettings->FindKey( "options/conteammatch" );
	if ( teamMatch == NULL )
	{
		return;
	}

	// Reserve session for team
	int numTeamPlayers = m_pSettings->GetInt( "members/numPlayers" );
	uint64 teamResKey = m_pSettings->GetUint64( "members/machine0/id", 0 );
	m_pSysSession->ReserveTeamSession( teamResKey, numTeamPlayers - 1);

	// Update lobby settings so that non-team members can join via matchmaking
	KeyValues *pUpdate = g_pMMF->GetMatchTitleGameSettingsMgr()->ExtendTeamLobbyToGame( m_pSettings );
	UpdateSessionSettings( pUpdate );
	pUpdate->deleteThis();

	g_pMMF->GetMatchTitleGameSettingsMgr()->PrepareForSessionCreate( m_pSettings );
	
	// Set all the properties of the new session and send them across
	KeyValues *pSettings = KeyValues::FromString(
		"update",
		" update { "
			" options { "
				" action joinsession "
			" } "
		" } "
	);
	KeyValues::AutoDelete autodelete_settings( pSettings );

	pUpdate = pSettings->FindKey( "update" );

	KeyValues *pOptions = pUpdate->FindKey( "options" );
	pOptions->SetUint64( "teamResKey", teamResKey );
	
#ifdef _X360
	char chSessionInfo[ XSESSION_INFO_STRING_LENGTH ] = {0};
	m_pSysSession->GetHostSessionInfo( chSessionInfo );
	pOptions->SetString( "sessioninfo", chSessionInfo );
	pOptions->SetUint64( "sessionid", m_pSysSession->GetHostSessionId() );
#else

	pOptions->SetUint64( "sessionid", m_pSysSession->GetSessionID() );

#endif

	// Set up "teammembers" key
	KeyValues *pTeamMembers = pUpdate->CreateNewKey();
	pTeamMembers->SetName( "teamMembers" );

	// Iterate "members" key
	KeyValues *pSessionMembers = m_pSettings->FindKey( "members" );
	pTeamMembers->SetInt( "numPlayers", numTeamPlayers );

	// Choose a side to join - in keyvalues, team CT = 1, team T = 2
	int team = RandomInt(1, 2);	
	int otherTeam = 3 - team;
	
	for ( int i = 0; i < numTeamPlayers; i++ )
	{
		if ( i == 5)
		{
			// Switch teams
			team = otherTeam;
		}

		KeyValues *pTeamPlayer = pTeamMembers->CreateNewKey();
		pTeamPlayer->SetName( CFmtStr( "player%d", i ) );

		KeyValues *pSessionMember = pSessionMembers->FindKey( CFmtStr( "machine%d", i ) );

		uint64 playerId = pSessionMember->GetUint64( "id" );
		pTeamPlayer->SetUint64( "xuid", playerId );
		pTeamPlayer->SetInt( "team", team );			
	}
	
	KeyValues *notify = new KeyValues( "SysSession::OnUpdate" );
	KeyValues::AutoDelete autodelete_notify( notify );

	notify->AddSubKey( pUpdate->MakeCopy() );

	m_pSysSession->SendMessage( notify );

	// Make sure we update host settings to include "conteam" otherwise host
	// won't know what side to join
	m_pSettings->SetInt( "conteam", team );
}

void CMatchSessionOnlineHost::Update()
{
	switch ( m_eState )
	{
	case STATE_INIT:
		m_eState = STATE_CREATING;

		// Session is creating
		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues(
			"OnMatchSessionUpdate",
				"state", "progress",
				"progress", "creating"
			) );

		// Trigger session creation
		m_pSysSession = new CSysSessionHost( m_pSettings );
		m_pSysSession->SetCryptKey( m_pSysData->GetUint64( "crypt" ) );
		break;

	case STATE_STARTING:
		Assert( m_pDsSearcher );
		if ( m_pDsSearcher )
		{
			m_pDsSearcher->Update();
			
			if ( m_pDsSearcher->IsFinished() )
			{
				OnRunCommand_StartDsSearchFinished();
				return;
			}
		}
		break;

	case STATE_MATCHING:
		Assert( m_pTeamSearcher );
		if ( m_pTeamSearcher )
		{
			m_pTeamSearcher->Update();
		}

		if ( m_pMatchSearcher )
		{
			m_pMatchSearcher->Update();
			
			CMatchSessionOnlineSearch::Result result = m_pMatchSearcher->GetResult();

			if ( result == CMatchSessionOnlineSearch::RESULT_FAIL )
			{
				KeyValues *teamMatch = m_pSettings->FindKey( "options/conteammatch" );
				if ( teamMatch )
				{
					OnRunCommand_Cancel_Match();
					OnRunCommand_Start();
				}
			}
			else if (result == CMatchSessionOnlineSearch::RESULT_SUCCESS )
			{
				m_pMatchSearcher->Destroy();
				m_pMatchSearcher = NULL;
			}
		}

		break;

	case STATE_MATCHINGRESTART:
		OnRunCommand_Match();
		break;

	case STATE_LOBBY:
		// If we're in the lobby and are setup to bypass the lobby, then send a start command now.
		if ( m_pSettings->GetBool( "options/bypasslobby", false ) )
		{
			OnRunCommand_Start();
		}
		break;
	}

	if ( m_pSysSession )
	{
		m_pSysSession->Update();
	}
}

void CMatchSessionOnlineHost::Destroy()
{
	g_pMatchExtensions->GetINetSupport()->UpdateClientReservation( 0ull, 0ull );

	if ( m_eState > STATE_LOBBY )
	{
		char const *szServerType = m_pSettings->GetString( "server/server", "listen" );
		if ( !Q_stricmp( szServerType, "listen" ) )
		{
			if ( IGameEvent *pEvent = g_pMatchExtensions->GetIGameEventManager2()->CreateEvent( "server_pre_shutdown" ) )
			{
				pEvent->SetString( "reason", "quit" );
				g_pMatchExtensions->GetIGameEventManager2()->FireEvent( pEvent );
			}
		}
		
		g_pMatchExtensions->GetIVEngineClient()->ExecuteClientCmd( "disconnect" );
	}

	if ( m_pMatchSearcher )
	{
		m_pMatchSearcher->Destroy();
		m_pMatchSearcher = NULL;
	}

	if ( m_pTeamSearcher )
	{
		m_pTeamSearcher->Destroy();
		m_pTeamSearcher = NULL;
	}

	if ( m_pDsSearcher )
	{
		m_pDsSearcher->Destroy();
		m_pDsSearcher = NULL;
	}

	if ( m_pSysSession )
	{
		m_pSysSession->Destroy();
		m_pSysSession = NULL;
	}

	delete this;

	g_pMatchExtensions->GetINetSupport()->UpdateServerReservation( 0ull );
}

void CMatchSessionOnlineHost::DebugPrint()
{
	DevMsg( "CMatchSessionOnlineHost [ state=%d ]\n", m_eState );
	
	DevMsg( "System data:\n" );
	KeyValuesDumpAsDevMsg( GetSessionSystemData(), 1 );
	
	DevMsg( "Settings data:\n" );
	KeyValuesDumpAsDevMsg( GetSessionSettings(), 1 );

	if ( m_pDsSearcher )
		DevMsg( "Dedicated search in progress\n" );
	else
		DevMsg( "Dedicated search not active\n" );
	
	if ( m_pSysSession )
		m_pSysSession->DebugPrint();
	else
		DevMsg( "SysSession is NULL\n" );

	if ( m_pTeamSearcher )
		m_pTeamSearcher->DebugPrint();
	else
		DevMsg( "TeamSearch is NULL\n" );
}

bool CMatchSessionOnlineHost::IsAnotherSessionJoinable( const char *pszAnotherSessionInfo )
{
#ifdef _X360
	if ( m_pSysSession )
	{
		char chHostInfo[ XSESSION_INFO_STRING_LENGTH ] = {0};
		m_pSysSession->GetHostSessionInfo( chHostInfo );
		
		XSESSION_INFO xsi, xsiAnother;
		MMX360_SessionInfoFromString( xsi, chHostInfo );
		MMX360_SessionInfoFromString( xsiAnother, pszAnotherSessionInfo );
		if ( !memcmp( &xsiAnother.sessionID, &xsi.sessionID, sizeof( xsi.sessionID ) ) )
			return false;
	}
#endif
	return true;
}

void CMatchSessionOnlineHost::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( m_pDsSearcher )
		m_pDsSearcher->OnEvent( pEvent );

	if ( m_pTeamSearcher )
		m_pTeamSearcher->OnEvent( pEvent );

	if ( m_pMatchSearcher )
		m_pMatchSearcher->OnEvent( pEvent );

	if ( !Q_stricmp( "OnEngineClientSignonStateChange", szEvent ) )
	{
		int iOldState = pEvent->GetInt( "old", 0 );
		int iNewState = pEvent->GetInt( "new", 0 );

		if ( iOldState >= SIGNONSTATE_CONNECTED &&
			iNewState  < SIGNONSTATE_CONNECTED )
		{
			if ( m_eState == STATE_LOADING || m_eState == STATE_GAME )
			{
				// Lost connection from server or explicit disconnect
				DevMsg( "OnEngineClientSignonStateChange\n" );
				g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "mmF->CloseSession" ) );
			}
			else if ( m_eState == STATE_ENDING )
			{
				// The game has successfully ended and we are back to lobby
				m_eState = STATE_LOBBY;
								
				KeyValues *pUpdate = KeyValues::FromString(
					"update",
					" update { "
						" system { "
							" lock #empty# "
						" } "
					" } "
					" delete { "
						" game { "
							" mmqueue #empty# "
						" } "
					" } "
					);
				g_pMMF->GetMatchTitleGameSettingsMgr()->ExtendGameSettingsForLobbyTransition(
					m_pSettings, pUpdate->FindKey( "update" ), true );
				UpdateSessionSettings( KeyValues::AutoDeleteInline( pUpdate ) );

				g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "ready", "transition", "hostendgame" ) );
			}
		}
	}
	else if ( !Q_stricmp( "OnEngineDisconnectReason", szEvent ) )
	{
		if ( m_eState == STATE_LOADING || m_eState == STATE_GAME )
		{
			// Lost connection from server or explicit disconnect
			char const *szReason = pEvent->GetString( "reason", "" );
			DevMsg( "OnEngineDisconnectReason %s\n", szReason );

			bool bLobbySalvagable =
				StringHasPrefix( szReason, "Connection to server timed out" ) ||
				StringHasPrefix( szReason, "Server shutting down" );

			if ( KeyValues *pDisconnectHdlr = g_pMMF->GetMatchTitleGameSettingsMgr()->PrepareClientLobbyForGameDisconnect( m_pSettings, pEvent ) )
			{
				KeyValues::AutoDelete autodelete( pDisconnectHdlr );
				char const *szDisconnectHdlr = pDisconnectHdlr->GetString( "disconnecthdlr", "" );
				if ( !Q_stricmp( szDisconnectHdlr, "destroy" ) )
					bLobbySalvagable = false;
				else if ( !Q_stricmp( szDisconnectHdlr, "lobby" ) )
					bLobbySalvagable = true;
			}

			if ( !bLobbySalvagable )
			{
				g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "mmF->CloseSession" ) );
			}
			else
			{
				// Server shutting down, try to retain the lobby
				pEvent->SetString( "disconnecthdlr", "lobby" );
				g_pMatchEventsSubscription->RegisterEventData( pEvent->MakeCopy() );
				OnEndGameToLobby();
			}
		}
	}
	else if ( !Q_stricmp( "OnEngineEndGame", szEvent ) )
	{
		if ( m_eState == STATE_LOADING || m_eState == STATE_GAME )
		{
			OnEndGameToLobby();
		}
	}
	else if ( !Q_stricmp( "OnEngineListenServerStarted", szEvent ) )
	{
		if ( m_eState == STATE_LOADING )
		{
			uint32 externalIP = pEvent->GetInt( "externalIP", 0 );
			OnRunCommand_StartListenServerStarted( externalIP );
		}
	}
	else if ( !Q_stricmp( "OnPlayerMachinesConnected", szEvent ) )
	{
		OnGamePlayerMachinesConnected( pEvent->GetInt( "numMachines" ) );
	}
	else if ( !Q_stricmp( "OnNetLanConnectionlessPacket", szEvent ) )
	{
		char const *szPacketType = pEvent->GetFirstTrueSubKey()->GetName();
		if ( m_pSysSession && m_eState > STATE_CREATING &&
			!Q_stricmp( szPacketType, "LanSearch" ) )
		{
			m_pSysSession->ReplyLanSearch( pEvent );
		}
	}
	else if ( !Q_stricmp( "OnSysMuteListChanged", szEvent ) )
	{
		if ( m_pSysSession )
			m_pSysSession->Voice_UpdateMutelist();
	}
	else if ( !Q_stricmp( "OnMatchSessionUpdate", szEvent ) )
	{
		const char *state = pEvent->GetString( "state", "" );

		if ( !Q_stricmp( "progress",  state) &&
			 ( m_pTeamSearcher || m_pMatchSearcher) && ( m_eState == STATE_MATCHING ) )
		{
			char const *szProgress = pEvent->GetString( "progress" );
			int numResults = pEvent->GetInt( "numResults", 0 );

			// Special case when communication between team gets aborted and the process needs
			// to be restarted
			if ( !Q_stricmp( "restart", szProgress ) )
			{
				m_eState = STATE_MATCHINGRESTART;
				return;
			}

			KeyValues *pUpdate = KeyValues::FromString(
				"update",
				" update { "
					" system { "
						" lock = "
					" } "
				" } "
				);

			if ( numResults > 0 )
			{
				pUpdate->SetString( "update/system/lock", CFmtStr( "matching%s%d", szProgress, numResults ) );
			}
			else
			{
				pUpdate->SetString( "update/system/lock", CFmtStr( "matching%s", szProgress ) );
			}

			// Now modify the state of our game
			UpdateSessionSettings( KeyValues::AutoDeleteInline ( pUpdate ) );
		}
		else if ( !Q_stricmp( "joinconteamsession", state) )
		{
			KeyValues *pSettings = KeyValues::FromString(
				"update",
				" update { "
					" system { "
						" network LIVE "						
					" } "
					" options { "
						" action joinsession "
					" } "
				" } "
			);

			KeyValues *pUpdate = pSettings->FindKey( "update" );

			uint64 sessionId = pEvent->GetUint64( "sessionid", 0 ); 
			const char *sessionInfo = pEvent->GetString( "sessioninfo", "" );
			
			KeyValues *pOptions = pUpdate->FindKey( "options" );
			pOptions->SetUint64( "sessionid", sessionId );
			pOptions->SetString( "sessioninfo", sessionInfo );

			uint64 teamResKey = m_pSettings->GetUint64( "members/machine0/id", 0 );
			pOptions->SetUint64( "teamResKey", teamResKey );

			KeyValues *pSessionHostDataSrc = pEvent->FindKey( "sessionHostDataUnpacked" );
			if ( pSessionHostDataSrc )
			{
				KeyValues *pSessionHostDataDst = pUpdate->CreateNewKey();
				pSessionHostDataDst->SetName( "sessionHostDataUnpacked" );

				pSessionHostDataSrc->CopySubkeys( pSessionHostDataDst );
			}

			KeyValues *pTeamMembersSrc = pEvent->FindKey( "teamMembers" );
			KeyValues *pTeamMembersDst = pUpdate->CreateNewKey();
			pTeamMembersDst->SetName( "teamMembers" );

			pTeamMembersSrc->CopySubkeys( pTeamMembersDst );

			// Now modify the state of our game
			UpdateSessionSettings( KeyValues::AutoDeleteInline ( pSettings ) );
		}
	}
	else if ( !Q_stricmp( "TeamSearchResult::ListenHost", szEvent ) )
	{
		m_eState = STATE_LOADING;
		StartListenServerMap();
	}
	else if ( !Q_stricmp( "TeamSearchResult::Dedicated", szEvent ) ||
			  !Q_stricmp( "TeamSearchResult::ListenClient", szEvent ) )
	{
		KeyValues *pServerInfo = pEvent->FindKey( "server", false );
		if ( ( m_eState == STATE_MATCHING ) &&
			 pServerInfo && m_pTeamSearcher )
		{
			// We found our dedicated server to play on, let the game commence!
			// Prepare the update - creating the "server" key signals that
			// a game server is available
			KeyValues *kvUpdate = KeyValues::FromString(
				"update",
				" update { "
					" system { "
						" lock #empty# "
					" } "
					" server { "
						" server dedicated "
					" } "
				" } "
				);
			KeyValues::AutoDelete autodelete( kvUpdate );
			kvUpdate->FindKey( "update/server" )->MergeFrom( pServerInfo, KeyValues::MERGE_KV_UPDATE );

			// Remove the lock from the session and allow joins and trigger clients connect
			UpdateSessionSettings( kvUpdate );

			//
			// Actually connect to game server
			//
			if ( void *pDsResult = pEvent->GetPtr( "dsresult" ) )
			{
				// we have a resolved secure server XNADDR
				ConnectGameServer( reinterpret_cast< CDsSearcher::DsResult_t * >( pDsResult ) );
			}
			else
			{
				// we need to resolve secure server XNADDR just like clients do
				ConnectGameServer( NULL );
			}
		}
	}
	else if ( !Q_stricmp( "mmF->SysSessionUpdate", szEvent ) )
	{
		if ( m_pSysSession && pEvent->GetPtr( "syssession", NULL ) == m_pSysSession )
		{
			// We had a session error
			if ( char const *szError = pEvent->GetString( "error", NULL ) )
			{
				State_t eSavedState = m_eState;

				// Destroy the session
				m_pSysSession->Destroy();
				m_pSysSession = NULL;

				// Handle error
				m_eState = STATE_CREATING;
				g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "error", "error", szError ) );

				// If we were in active gameplay state during migration then also disconnect
				if ( !Q_stricmp( szError, "migrate" ) &&
					( eSavedState == STATE_GAME || eSavedState == STATE_ENDING ) )
				{
					g_pMatchExtensions->GetIVEngineClient()->ExecuteClientCmd( "disconnect" );
				}
				return;
			}

			// This is our session
			switch ( m_eState )
			{
			case STATE_CREATING:
				g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "created" ) );

				// Session created successfully and we were straight on the way to a server
				if ( char const *szServer = m_pSettings->GetString( "server/server", NULL ) )
				{
					OnRunCommand_Start();
				}
				// Session created successfully and we are in the lobby
				else
				{
					m_eState = STATE_LOBBY;
					g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "ready", "transition", "hostinit" ) );
				}
				break;

			default:
				if ( char const *szAction = pEvent->GetString( "action", NULL ) )
				{
					if ( !Q_stricmp( "client", szAction ) )
					{
						KeyValues *pExtendedSettings = new KeyValues( "ExtendedSettings" );
						char const *szMigrateState = "lobby";
						switch ( m_eState )
						{
						case STATE_GAME:
							szMigrateState = "game";
							break;
						case STATE_ENDING:
							szMigrateState = "ending";
							break;
						}
						pExtendedSettings->SetString( "state", szMigrateState );
						pExtendedSettings->AddSubKey( m_pSettings );

						// Release ownership of the resources since new match session now owns them
						m_pSettings = NULL;
						m_autodelete_pSettings.Assign( NULL );

						CSysSessionHost *pSysSession = m_pSysSession;
						m_pSysSession = NULL;

						// Destroy our instance and create the new match interface
						m_eState = STATE_MIGRATE;
						g_pMMF->SetCurrentMatchSession( NULL );
						this->Destroy();

						// Now we need to create the new client session that will install itself
						IMatchSession *pNewClient = new CMatchSessionOnlineClient( pSysSession, pExtendedSettings );
						Assert( g_pMMF->GetMatchSession() == pNewClient );
						pNewClient;
						return;
					}
				}
				break;
			}
		}
	}
	else if ( !Q_stricmp( "mmF->SysSessionCommand", szEvent ) )
	{
		if ( m_pSysSession && pEvent->GetPtr( "syssession", NULL ) == m_pSysSession )
		{
			KeyValues *pCommand = pEvent->GetFirstTrueSubKey();
			if ( pCommand )
			{
				OnRunCommand( pCommand );
			}
		}
	}
}

void CMatchSessionOnlineHost::MigrateGameSettings()
{
	// Check if we have not been destroyed due to a failed migration
	if ( g_pMatchFramework->GetMatchSession() != this )
	{
		DevWarning( "CMatchSessionOnlineHost::MigrateGameSettings cannot run because our session is not the active session!\n" );
		return;
	}

	switch ( m_eState )
	{
	case STATE_LOBBY:
		{
			// Previous host was in LOBBY - then nothing to do
			// Previous host was in STARTING / LOADING - remove the "lock" field
			// on the session
			// Previous host was in ENDING, but this client reached lobby after
			// reacting to game event from game server - host had the "server"
			// key and could have set "lock = endgame" - remove the "lock" field
			// and "server" key on the session.

			// Check if the server or endgame information was set
			char const *szServer = m_pSettings->GetString( "server/server", NULL );
			char const *szLockType = m_pSettings->GetString( "system/lock", "" );
				
			// Remove server information
			KeyValues *kvFix = KeyValues::FromString(
				"updatedelete",
				" update { "
					" system { "
						" lock #empty# "
					" } "
				" } "
				" delete { "
					" game { mmqueue #empty# } "
					" server delete "
				" } "
				);

			// Let the title apply title-specific adjustments
			bool bEndGameTransition = ( szServer || !Q_stricmp( szLockType, "endgame" ) );
			g_pMMF->GetMatchTitleGameSettingsMgr()->ExtendGameSettingsForLobbyTransition(
				m_pSettings, kvFix->FindKey( "update" ), bEndGameTransition );

			UpdateSessionSettings( KeyValues::AutoDeleteInline( kvFix ) );
		}
		break;

	case STATE_GAME:
		{
			// Game state really just transfers straightly
		}
		break;
	case STATE_ENDING:
		{
			// Fix up the system since this machine is the new host and haven't yet
			// reached the lobby

			// Remove server information
			UpdateSessionSettings( KeyValues::AutoDeleteInline( KeyValues::FromString(
				"updatedelete",
				" update { "
					" system { "
						" lock endgame "
					" } "
				" } "
				" delete { "
					" server delete "
				" } "
				) ) );
		}
		break;

	default:
		Assert( !"CMatchSessionOnlineHost::MigrateGameSettings - unreachable!" );
		break;
	}
}

void CMatchSessionOnlineHost::InitializeGameSettings()
{
	//
	// We need to establish correct keys
	// because data coming from callers can contain all sorts
	// of auxiliary settings
	//
	m_pSettings->SetName( "settings" );
	int numSlotsCreated = m_pSettings->GetInt( "members/numSlots", 0 );
	char const *arrKeys[] = { "system", "game", "options", "server", "conteamlobby" };
	for ( KeyValues *valRoot = m_pSettings->GetFirstSubKey(); valRoot; )
	{
		KeyValues *pRootSubkey = valRoot;
		valRoot = pRootSubkey->GetNextKey();

		bool bValidKey = false;
		char const *szRootSubkeyName = pRootSubkey->GetName();
		for ( int k = 0; k < ARRAYSIZE( arrKeys ); ++ k )
		{
			if ( !Q_stricmp( arrKeys[k], szRootSubkeyName ) )
			{
				bValidKey = true;
				break;
			}
		}
		
		if ( !bValidKey )
		{
			m_pSettings->RemoveSubKey( pRootSubkey );
			pRootSubkey->deleteThis();
		}
	}

	// Since the session can be created with a minimal amount of data available
	// the session object is responsible for initializing the missing data to defaults
	// or saved values or values from gamer progress/profile or etc...

	if ( KeyValues *kv = m_pSettings->FindKey( "system", true ) )
	{
		KeyValuesAddDefaultString( kv, "network", "LIVE" );
		KeyValuesAddDefaultString( kv, "access", "public" );
	}

	bool bEncryptedServerAddressRequired = false;

	if ( KeyValues *kv = m_pSettings->FindKey( "options", true ) )
	{
		char const *szServerType = kv->GetString( "server", NULL );
		if ( !szServerType || Q_stricmp( szServerType, "official") )
			szServerType = "official";
		if ( Q_stricmp( "LIVE", m_pSettings->GetString( "system/network" ) ) )
			szServerType = "listen";
		KeyValuesAddDefaultString( kv, "server", szServerType );
		
		// Remove "action" key if it doesn't hold useful data
		if ( KeyValues *kvAction = kv->FindKey( "action" ) )
		{
			char const *szAction = kvAction->GetString();
			if ( szAction && !Q_stricmp( szAction, "crypt" ) )
				bEncryptedServerAddressRequired = true;

			if ( !szAction || !*szAction ||
				!Q_stricmp( "create", szAction ) ||
				!Q_stricmp( "crypt", szAction ) )
			{
				kv->RemoveSubKey( kvAction );
				kvAction->deleteThis();
			}
		}
	}

	// Reset the members key
	if ( KeyValues *pMembers = m_pSettings->FindKey( "members", true ) )
	{
		pMembers->SetInt( "numMachines", 1 );

		int numPlayers = 1;
#ifdef _GAMECONSOLE
		numPlayers = XBX_GetNumGameUsers();
#endif

		pMembers->SetInt( "numPlayers", numPlayers );
		pMembers->SetInt( "numSlots", MAX( numSlotsCreated, numPlayers ) );
		
		if ( KeyValues *pMachine = pMembers->FindKey( "machine0", true ) )
		{
			XUID machineid = g_pPlayerManager->GetLocalPlayer( XBX_GetPrimaryUserId() )->GetXUID();

			pMachine->SetUint64( "id", machineid );
#if defined( _PS3 ) && !defined( NO_STEAM )
			pMachine->SetUint64( "psnid", steamapicontext->SteamUser()->GetConsoleSteamID().ConvertToUint64() );
#endif
			pMachine->SetUint64( "flags", MatchSession_GetMachineFlags() );
			pMachine->SetInt( "numPlayers", numPlayers );
			pMachine->SetUint64( "dlcmask", g_pMatchFramework->GetMatchSystem()->GetDlcManager()->GetDataInfo()->GetUint64( "@info/installed" ) );
			pMachine->SetString( "tuver", MatchSession_GetTuInstalledString() );
			pMachine->SetInt( "ping", 0 );

			for ( int k = 0; k < numPlayers; ++ k )
			{
				if ( KeyValues *pPlayer = pMachine->FindKey( CFmtStr( "player%d", k ), true ) )
				{
					int iController = 0;
#ifdef _GAMECONSOLE
					iController = XBX_GetUserId( k );
#endif
					IPlayerLocal *player = g_pPlayerManager->GetLocalPlayer( iController );

					pPlayer->SetUint64( "xuid", player->GetXUID() );
					pPlayer->SetString( "name", player->GetName() );
				}
			}
		}
	}

	if ( bEncryptedServerAddressRequired )
	{
		// We need to encrypt the address if there's one
		if ( char const *szAdrOnline = MatchSession_EncryptAddressString( m_pSettings->GetString( "server/adronline" ), m_pSysData->GetUint64( "crypt" ) ) )
			m_pSettings->SetString( "server/adronline", szAdrOnline );
		if ( char const *szAdrOnline = MatchSession_EncryptAddressString( m_pSettings->GetString( "server/adrlocal" ), m_pSysData->GetUint64( "crypt" ) ) )
			m_pSettings->SetString( "server/adrlocal", szAdrOnline );
	}

	// Let the title extend the game settings
	g_pMMF->GetMatchTitleGameSettingsMgr()->InitializeGameSettings( m_pSettings, "host" );

	DevMsg( "CMatchSessionOnlineHost::InitializeGameSettings adjusted settings:\n" );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );
}


