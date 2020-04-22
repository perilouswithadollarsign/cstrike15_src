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

//
// CMatchSessionOfflineCustom
//
// Implementation of an offline session
// that allows customization before the actual
// game commences (like playing commentary mode
// or playing single-player)
//

CMatchSessionOfflineCustom::CMatchSessionOfflineCustom( KeyValues *pSettings ) :
	m_pSettings( pSettings->MakeCopy() ),
	m_autodelete_pSettings( m_pSettings ),
	m_eState( STATE_INIT ),
	m_bExpectingServerReload( false )
{
	DevMsg( "Created CMatchSessionOfflineCustom:\n" );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );

	InitializeGameSettings();
}

CMatchSessionOfflineCustom::~CMatchSessionOfflineCustom()
{
	DevMsg( "Destroying CMatchSessionOfflineCustom:\n" );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );
}

KeyValues * CMatchSessionOfflineCustom::GetSessionSettings()
{
	return m_pSettings;
}

void CMatchSessionOfflineCustom::UpdateSessionSettings( KeyValues *pSettings )
{
	// Extend the update keys
	g_pMMF->GetMatchTitleGameSettingsMgr()->ExtendGameSettingsUpdateKeys( m_pSettings, pSettings );
	m_pSettings->MergeFrom( pSettings );

	// Broadcast the update to everybody interested
	MatchSession_BroadcastSessionSettingsUpdate( pSettings );
}

void CMatchSessionOfflineCustom::UpdateTeamProperties( KeyValues *pTeamProperties )
{
}

void CMatchSessionOfflineCustom::Command( KeyValues *pCommand )
{
	char const *szCommand = pCommand->GetName();

	if ( !Q_stricmp( "Start", szCommand ) && m_eState < STATE_RUNNING )
	{
		m_eState = STATE_RUNNING;

		OnGamePrepareLobbyForGame();

		UpdateSessionSettings( KeyValues::AutoDeleteInline( KeyValues::FromString(
				"update",
				" update { "
					" server { "
						" server listen "
					" } "
				" } "
				) ) );

		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnProfilesWriteOpportunity", "reason", "sessionstart"
			) );
		
		bool bResult = g_pMatchFramework->GetMatchTitle()->StartServerMap( m_pSettings );
		if ( !bResult )
		{
			Warning( "Failed to start server map!\n" );
			KeyValuesDumpAsDevMsg( m_pSettings, 1 );
			Assert( 0 );
			g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "error", "error", "nomap" ) );
		}
		Msg( "Succeeded in starting server map!\n" );
		return;
	}
	if ( !Q_stricmp( "QueueConnect", szCommand ) )
	{
		char const *szConnectAddress = pCommand->GetString( "adronline", "0.0.0.0" );
		uint64 uiReservationId = pCommand->GetUint64( "reservationid" );
		bool bAutoCloseSession = pCommand->GetBool( "auto_close_session" );
		Assert( bAutoCloseSession );
		if ( bAutoCloseSession )
		{
			// Switch the state
			m_eState = STATE_RUNNING;

			MatchSession_PrepareClientForConnect( m_pSettings, uiReservationId );

			// Close the session, potentially resetting a bunch of state
			if ( bAutoCloseSession )
				g_pMatchFramework->CloseSession();

			// Determine reservation settings required
			g_pMatchExtensions->GetINetSupport()->UpdateClientReservation( uiReservationId, 0ull );

			// Issue the connect command
			g_pMatchExtensions->GetIVEngineClient()->StartLoadingScreenForCommand( CFmtStr( "connect %s", szConnectAddress ) );

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

		// Notify the framework about player updated
		KeyValues *kvEvent = new KeyValues( "OnPlayerUpdated" );
		kvEvent->SetUint64( "xuid", arrPlayersUpdated[k]->GetUint64( "xuid" ) );
		g_pMatchEventsSubscription->BroadcastEvent( kvEvent );
	}

	//
	// Send the command as event for handling
	//
	KeyValues *pEvent = pCommand->MakeCopy();
	pEvent->SetName( CFmtStr( "Command::%s", pCommand->GetName() ) );
	g_pMatchEventsSubscription->BroadcastEvent( pEvent );
}

uint64 CMatchSessionOfflineCustom::GetSessionID()
{
	return 0;
}

void CMatchSessionOfflineCustom::Update()
{
	switch ( m_eState )
	{
	case STATE_INIT:
		m_eState = STATE_CONFIG;
		
		// Let everybody know that the session is now ready
		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "ready", "transition", "offlineinit" ) );
		break;
	}
}

void CMatchSessionOfflineCustom::Destroy()
{
	if ( m_eState == STATE_RUNNING )
	{
		g_pMatchExtensions->GetIVEngineClient()->ExecuteClientCmd( "disconnect" );

		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnProfilesWriteOpportunity", "reason", "sessionend"
			) );
	}

	delete this;
}

void CMatchSessionOfflineCustom::DebugPrint()
{
	DevMsg( "CMatchSessionOfflineCustom [ state=%d ]\n", m_eState );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );
}

void CMatchSessionOfflineCustom::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( !Q_stricmp( "OnEngineClientSignonStateChange", szEvent ) )
	{
		int iOldState = pEvent->GetInt( "old", 0 );
		int iNewState = pEvent->GetInt( "new", 0 );

		if ( iOldState >= SIGNONSTATE_CONNECTED &&
			iNewState  < SIGNONSTATE_CONNECTED )
		{
			// Disconnecting from server
			DevMsg( "OnEngineClientSignonStateChange\n" );
			if ( m_bExpectingServerReload )
			{
				m_bExpectingServerReload = false;
				DevMsg( " session was expecting server reload...\n" );
				return;
			}
			g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "mmF->CloseSession" ) );
			return;
		}
	}
	else if ( !Q_stricmp( "OnEngineClientSignonStatePrepareChange", szEvent ) )
	{
		char const *szReason = pEvent->GetString( "reason" );
		if ( !Q_stricmp( "reload", szReason ) )
		{
			Assert( !m_bExpectingServerReload );
			m_bExpectingServerReload = true;
			return;
		}
		else if ( !Q_stricmp( "load", szReason ) )
		{
			char const *szLevelName = g_pMatchExtensions->GetIVEngineClient()->GetLevelName();
			if ( szLevelName && szLevelName[0] && g_pMatchExtensions->GetIVEngineClient()->IsConnected() )
			{
				Assert( !m_bExpectingServerReload );
				m_bExpectingServerReload = true;
				return;
			}
		}
	}
	else if ( !Q_stricmp( "OnEngineEndGame", szEvent ) )
	{
		DevMsg( "OnEngineEndGame\n" );
		
		// Issue the disconnect command
		g_pMatchExtensions->GetIVEngineClient()->ExecuteClientCmd( "disconnect" );
		
		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "mmF->CloseSession" ) );
		return;
	}
}

void CMatchSessionOfflineCustom::InitializeGameSettings()
{
	// Since the session can be created with a minimal amount of data available
	// the session object is responsible for initializing the missing data to defaults
	// or saved values or values from gamer progress/profile or etc...

	if ( KeyValues *kv = m_pSettings->FindKey( "system", true ) )
	{
		kv->SetString( "network", "offline" );
		kv->SetString( "access", "public" );
	}

	if ( KeyValues *kv = m_pSettings->FindKey( "options", true ) )
	{
		kv->SetString( "server", "listen" );
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
			IPlayerLocal *pPriPlayer = g_pPlayerManager->GetLocalPlayer( XBX_GetPrimaryUserId() );

			pMachine->SetUint64( "id", ( pPriPlayer ? pPriPlayer->GetXUID() : INVALID_XUID ) );
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
					if ( player )
					{
						pPlayer->SetUint64( "xuid", player->GetXUID() );
						pPlayer->SetString( "name", player->GetName() );
					}
				}
			}
		}
	}

	// Let the title extend the game settings
	g_pMMF->GetMatchTitleGameSettingsMgr()->InitializeGameSettings( m_pSettings, "host" );

	DevMsg( "CMatchSessionOfflineCustom::InitializeGameSettings adjusted settings:\n" );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );
}

void CMatchSessionOfflineCustom::OnGamePrepareLobbyForGame()
{
	// Remember which players will get updated
	CUtlVector< KeyValues * > arrPlayersUpdated;
	arrPlayersUpdated.SetCount( m_pSettings->GetInt( "members/numPlayers", 0 ) );
	memset( arrPlayersUpdated.Base(), 0, arrPlayersUpdated.Count() * sizeof( KeyValues * ) );

	g_pMMF->GetMatchTitleGameSettingsMgr()->PrepareLobbyForGame( m_pSettings, arrPlayersUpdated.Base() );

	// Notify the framework of the updates
	for ( int k = 0; k < arrPlayersUpdated.Count(); ++ k )
	{
		if ( !arrPlayersUpdated[k] )
			break;

		// Notify the framework about player updated
		KeyValues *kvEvent = new KeyValues( "OnPlayerUpdated" );
		kvEvent->SetUint64( "xuid", arrPlayersUpdated[k]->GetUint64( "xuid" ) );
		g_pMatchEventsSubscription->BroadcastEvent( kvEvent );
	}

	// Let the title prepare for connect
	MatchSession_PrepareClientForConnect( m_pSettings );
}
