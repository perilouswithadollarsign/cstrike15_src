//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_framework.h"

#include "vstdlib/random.h"
#include "fmtstr.h"

#include "netmessages_signon.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//
// CMatchSessionOnlineClient
//
// Implementation of an online session of a host machine
//

void CMatchSessionOnlineClient::Init()
{
	KeyValues *psessionHostDataUnpacked = m_pSettings->FindKey( "sessionHostDataUnpacked" );
	if ( psessionHostDataUnpacked )
	{
		m_pSettings->SetPtr( "options/sessionHostData", psessionHostDataUnpacked );
	}
}

CMatchSessionOnlineClient::CMatchSessionOnlineClient( KeyValues *pSettings ) :
	m_pSettings( pSettings->MakeCopy() ),
	m_autodelete_pSettings( m_pSettings ),
	m_pSysData( new KeyValues( "SysSessionData", "type", "client" ) ),
	m_autodelete_pSysData( m_pSysData ),
	m_eState( STATE_INIT ),
	m_pSysSession( NULL )
{
	DevMsg( "Created CMatchSessionOnlineClient:\n" );
	Init();

	KeyValuesDumpAsDevMsg( m_pSettings, 1 );

	InitializeGameSettings();
}

CMatchSessionOnlineClient::CMatchSessionOnlineClient( CSysSessionClient *pSysSession, KeyValues *pSettings ) :
	m_pSettings( pSettings ),
	m_autodelete_pSettings( m_pSettings ),
	m_pSysData( new KeyValues( "SysSessionData", "type", "client" ) ),
	m_autodelete_pSysData( m_pSysData ),
	m_eState( STATE_LOBBY ),
	m_pSysSession( pSysSession )
{
	DevMsg( "Converted sys session into CMatchSessionOnlineClient:\n" );
	Init();
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );
}

CMatchSessionOnlineClient::CMatchSessionOnlineClient( CSysSessionHost *pSysSession, KeyValues *pExtendedSettings ) :
	m_pSettings( pExtendedSettings->FindKey( "settings" ) ),
	m_autodelete_pSettings( m_pSettings ),
	m_pSysData( new KeyValues( "SysSessionData", "type", "client" ) ),
	m_autodelete_pSysData( m_pSysData ),
	m_eState( STATE_LOBBY ), // it's at least lobby, we'll figure out later
	m_pSysSession( NULL )
{
	DevMsg( "Migrating into CMatchSessionOnlineClient...\n" );
	Init();
	Assert( m_pSettings );

	KeyValues::AutoDelete autodelete( pExtendedSettings );
	pExtendedSettings->RemoveSubKey( m_pSettings ); // it's now our settings

	// Install our session
	g_pMMF->SetCurrentMatchSession( this );

	// Check if the game is in a non-lobby state
	char const *szState = pExtendedSettings->GetString( "state", "" );
	if ( !Q_stricmp( szState, "game" ) )
		m_eState = STATE_GAME;
	else if ( !Q_stricmp( szState, "ending" ) )
		m_eState = STATE_ENDING;

	// Now we need to create the system session to reflect the client session passed
	m_pSysSession = new CSysSessionClient( pSysSession, m_pSettings );
	pSysSession->Destroy();

	// Show the state
	DevMsg( "Migrated into CMatchSessionOnlineClient:\n" );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );
}

CMatchSessionOnlineClient::~CMatchSessionOnlineClient()
{
	DevMsg( "Destroying CMatchSessionOnlineClient:\n" );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );
}

KeyValues * CMatchSessionOnlineClient::GetSessionSystemData()
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

KeyValues * CMatchSessionOnlineClient::GetSessionSettings()
{
	return m_pSettings;
}

void CMatchSessionOnlineClient::UpdateSessionSettings( KeyValues *pSettings )
{
	// Avoid a warning and assert for queue state manipulation
	if ( pSettings->GetFirstSubKey()->GetString( "game/mmqueue", NULL ) )
		return;

	// Otherwise warn
	Warning( "CMatchSessionOnlineClient::UpdateSessionSettings is unavailable in state %d!\n", m_eState );
	Assert( !"CMatchSessionOnlineClient::UpdateSessionSettings is unavailable!\n" );
}

void CMatchSessionOnlineClient::UpdateTeamProperties( KeyValues *pSettings )
{
	m_pSysSession->UpdateTeamProperties( pSettings );
}

void CMatchSessionOnlineClient::Command( KeyValues *pCommand )
{
	char const *szCommand, *szRun;
	szCommand = pCommand->GetName();
	szRun = pCommand->GetString( "run", "" );

	if ( !Q_stricmp( szRun, "host" ) || !Q_stricmp( szRun, "all" ) || !Q_stricmp( szRun, "xuid" ) )
	{
		if ( m_pSysSession )
		{
			m_pSysSession->Command( pCommand );
			return;
		}
	}
	else if ( !*szRun || !Q_stricmp( szRun, "local" ) )
	{
		OnRunCommand( pCommand );
		return;
	}

	Warning( "CMatchSessionOnlineClient::Command( %s ) unhandled!\n", szCommand );
	Assert( !"CMatchSessionOnlineClient::Command" );
}

void CMatchSessionOnlineClient::OnRunCommand( KeyValues *pCommand )
{
	char const *szCommand = pCommand->GetName();
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

	// Client-side command cannot update the players
	g_pMMF->GetMatchTitleGameSettingsMgr()->ExecuteCommand( pCommand, GetSessionSystemData(), m_pSettings, NULL );

	// Send the command as event for handling
	KeyValues *pEvent = pCommand->MakeCopy();
	pEvent->SetName( CFmtStr( "Command::%s", pCommand->GetName() ) );
	g_pMatchEventsSubscription->BroadcastEvent( pEvent );
}

uint64 CMatchSessionOnlineClient::GetSessionID()
{
	if( m_pSysSession )
	{
		return m_pSysSession->GetSessionID();
	}

	return 0;
}

void CMatchSessionOnlineClient::Update()
{
	switch ( m_eState )
	{
	case STATE_INIT:
		m_eState = STATE_CREATING;

		// Adjust invite-based settings
		uint64 uiInviteFlags = g_pMMF->GetLastInviteFlags();
		m_pSettings->SetUint64( "members/joinflags", uiInviteFlags );

		// Session is creating
		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues(
			"OnMatchSessionUpdate",
				"state", "progress",
				"progress", "joining"
			) );

		// Trigger session creation
		m_pSysSession = new CSysSessionClient( m_pSettings );
		break;
	}

	if ( m_pSysSession )
	{
		m_pSysSession->Update();
	}
}

void CMatchSessionOnlineClient::Destroy()
{
	if ( m_eState != STATE_MIGRATE )
	{
		g_pMatchExtensions->GetINetSupport()->UpdateClientReservation( 0ull, 0ull );
	}

	if ( m_eState == STATE_GAME || m_eState == STATE_ENDING )
	{
		m_eState = STATE_INIT; // the object is being deleted, but prevent signon state change handlers
		g_pMatchExtensions->GetIVEngineClient()->ExecuteClientCmd( "disconnect" );
	}

	if ( m_pSysSession )
	{
		m_pSysSession->Destroy();
		m_pSysSession = NULL;
	}

	delete this;
}

void CMatchSessionOnlineClient::DebugPrint()
{
	DevMsg( "CMatchSessionOnlineClient [ state=%d ]\n", m_eState );
	
	DevMsg( "System data:\n" );
	KeyValuesDumpAsDevMsg( GetSessionSystemData(), 1 );
	
	DevMsg( "Settings data:\n" );
	KeyValuesDumpAsDevMsg( GetSessionSettings(), 1 );
	
	if ( m_pSysSession )
		m_pSysSession->DebugPrint();
	else
		DevMsg( "SysSession is NULL\n" );
}

bool CMatchSessionOnlineClient::IsAnotherSessionJoinable( const char *pszAnotherSessionInfo )
{
#ifdef _X360
	if ( m_pSysSession )
	{
		XSESSION_INFO xsi;
		if ( m_pSysSession->GetHostNetworkAddress( xsi ) )
		{
			XSESSION_INFO xsiAnother;
			MMX360_SessionInfoFromString( xsiAnother, pszAnotherSessionInfo );
			if ( !memcmp( &xsiAnother.sessionID, &xsi.sessionID, sizeof( xsi.sessionID ) ) )
				return false;
		}
	}
#endif
	return true;
}

void CMatchSessionOnlineClient::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( !Q_stricmp( "OnEngineClientSignonStateChange", szEvent ) )
	{
		int iOldState = pEvent->GetInt( "old", 0 );
		int iNewState = pEvent->GetInt( "new", 0 );

		if ( iOldState >= SIGNONSTATE_CONNECTED &&
			iNewState  < SIGNONSTATE_CONNECTED )
		{
			if ( m_eState == STATE_GAME )
			{
				// Lost connection from server or explicit disconnect
				DevMsg( "OnEngineClientSignonStateChange\n" );
				g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "mmF->CloseSession" ) );
			}
			else if ( m_eState == STATE_ENDING )
			{
				m_eState = STATE_LOBBY;
				g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "ready", "transition", "clientendgame" ) );
			}
		}
	}
	else if ( !Q_stricmp( "OnEngineDisconnectReason", szEvent ) )
	{
		if ( m_eState == STATE_GAME )
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
		if ( m_eState == STATE_GAME )
		{
			OnEndGameToLobby();
		}
	}
	else if ( !Q_stricmp( "OnMatchSessionUpdate", szEvent ) )
	{
		char const *szState = pEvent->GetString( "state", "" );
		if ( !Q_stricmp( "updated", szState ) )
		{
			switch ( m_eState )
			{
			case STATE_LOBBY:
				if ( KeyValues *pServer = pEvent->FindKey( "update/server" ) )
				{
					// If we are in a con team match we only connect to game servers
					// in response to an invite from the host so don't do anything
					KeyValues *pConTeamMatch = m_pSettings->FindKey("options/conteammatch");
					if ( !pConTeamMatch ||
						( pEvent->GetUint64( "update/server/xuid" ) && pEvent->GetUint64( "update/server/reservationid" ) ) )
					{	
						// Game server has become available - connect to it!
						ConnectGameServer();
					}
				}
				break;
			
			case STATE_GAME:
				if ( KeyValues *pServer = pEvent->FindKey( "delete/server" ) )
				{
					// Lobby leader has disconnected from game server, disconnect too
					OnEndGameToLobby();
				}
				break;
			}
		}
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
	else if ( !Q_stricmp( "mmF->SysSessionUpdate", szEvent ) )
	{
		if ( m_pSysSession && pEvent->GetPtr( "syssession", NULL ) == m_pSysSession )
		{
			// This is our session
			if ( char const *szError = pEvent->GetString( "error", NULL ) )
			{
				// Destroy the session
				m_pSysSession->Destroy();
				m_pSysSession = NULL;
				m_eState = STATE_CREATING;

				// Handle error
				KeyValues *kvUpdateError = pEvent->MakeCopy();
				kvUpdateError->SetName( "OnMatchSessionUpdate" );
				kvUpdateError->SetString( "state", "error" );
				g_pMatchEventsSubscription->BroadcastEvent( kvUpdateError );
				return;
			}

			if ( uint64 ullCrypt = pEvent->GetUint64( "crypt" ) )
				m_pSysData->SetUint64( "crypt", ullCrypt );

			switch ( m_eState )
			{
			case STATE_CREATING:
				// Session was creating
				// Now we are at least in the lobby
				m_eState = STATE_LOBBY;
				OnClientFullyConnectedToSession();
				return;
				
			default:
				if ( char const *szAction = pEvent->GetString( "action", NULL ) )
				{
					if ( !Q_stricmp( "host", szAction ) )
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
						if ( uint64 ullCrypt = m_pSysData->GetUint64( "crypt" ) )
							pExtendedSettings->SetUint64( "crypt", ullCrypt );
						pExtendedSettings->AddSubKey( m_pSettings );
						
						// Release ownership of the resources since new match session now owns them
						m_pSettings = NULL;
						m_autodelete_pSettings.Assign( NULL );

						CSysSessionClient *pSysSession = m_pSysSession;
						m_pSysSession = NULL;

						// Destroy our instance and create the new match interface
						m_eState = STATE_MIGRATE;
						g_pMMF->SetCurrentMatchSession( NULL );
						this->Destroy();

						// Now we need to create the new host session that will install itself
						IMatchSession *pNewHost = new CMatchSessionOnlineHost( pSysSession, pExtendedSettings );
						Assert( g_pMMF->GetMatchSession() == pNewHost );
						pNewHost;
						return;
					}
				}
				break;
			}

			DevWarning( "Unhandled mmF->SysSessionUpdate!\n" );
			Assert( !"Unhandled mmF->SysSessionUpdate!\n" );
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

void CMatchSessionOnlineClient::OnClientFullyConnectedToSession()
{
	g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "created" ) );

	// Check whether the game server is available (i.e. game in progress)
	if ( KeyValues *pServer = m_pSettings->FindKey( "server" ) )
	{
		ConnectGameServer();
	}
	else
	{
		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "ready", "transition", "clientconnect" ) );
	}
}

void CMatchSessionOnlineClient::OnEndGameToLobby()
{
	m_eState = STATE_ENDING;

	// Issue the disconnect command
	g_pMatchExtensions->GetIVEngineClient()->ExecuteClientCmd( "disconnect" );
	
	// Mark gameplay state as inactive
	m_pSysSession->SetSessionActiveGameplayState( false, NULL );
}

void CMatchSessionOnlineClient::InitializeGameSettings()
{
	// Initialize only the settings required to connect...

	if ( KeyValues *kv = m_pSettings->FindKey( "system", true ) )
	{
		KeyValuesAddDefaultString( kv, "network", "LIVE" );
		KeyValuesAddDefaultString( kv, "access", "public" );
	}

	if ( KeyValues *pMembers = m_pSettings->FindKey( "members", true ) )
	{
		pMembers->SetInt( "numMachines", 1 );

		int numPlayers = 1;
#ifdef _GAMECONSOLE
		numPlayers = XBX_GetNumGameUsers();
#endif
		pMembers->SetInt( "numPlayers", numPlayers );
		pMembers->SetInt( "numSlots", numPlayers );

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

	// Let the title extend the game settings
	g_pMMF->GetMatchTitleGameSettingsMgr()->InitializeGameSettings( m_pSettings, "client" );

	DevMsg( "CMatchSessionOnlineClient::InitializeGameSettings adjusted settings:\n" );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );
}

void CMatchSessionOnlineClient::OnRunCommand_QueueConnect( KeyValues *pCommand )
{
	char const *szConnectAddress = pCommand->GetString( "adronline", "0.0.0.0" );
	uint64 uiReservationId = pCommand->GetUint64( "reservationid" );
	bool bAutoCloseSession = pCommand->GetBool( "auto_close_session" );

	// Switch the state
	m_eState = STATE_GAME;

	MatchSession_PrepareClientForConnect( m_pSettings, uiReservationId );

	// Mark gameplay state as active
	m_pSysSession->SetSessionActiveGameplayState( true, szConnectAddress );

	// Close the session, potentially resetting a bunch of state
	if ( bAutoCloseSession )
		g_pMatchFramework->CloseSession();

	// Determine reservation settings required
	g_pMatchExtensions->GetINetSupport()->UpdateClientReservation( uiReservationId, 0ull );

	// Issue the connect command
	g_pMatchExtensions->GetIVEngineClient()->StartLoadingScreenForCommand( CFmtStr( "connect %s", szConnectAddress ) );
}

void CMatchSessionOnlineClient::ConnectGameServer()
{
	// Switch the state
	m_eState = STATE_GAME;

	MatchSession_PrepareClientForConnect( m_pSettings );

	//
	// Resolve server information
	//
	MatchSessionServerInfo_t msInfo = {0};
	if ( !MatchSession_ResolveServerInfo( m_pSettings, m_pSysSession, msInfo, msInfo.RESOLVE_DEFAULT, m_pSysData->GetUint64( "crypt" ) ) )
	{
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
	m_pSysSession->SetSessionActiveGameplayState( true, msInfo.m_szSecureServerAddress );

	// Issue the connect command
	g_pMatchExtensions->GetIVEngineClient()->ClientCmd_Unrestricted( msInfo.m_szConnectCmd );
}

