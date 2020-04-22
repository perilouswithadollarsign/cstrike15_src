//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Uploads KeyValue stats to the new SteamWorks gamestats system.
//
//=============================================================================

#include "cbase.h"

#if !defined( _GAMECONSOLE )

#include "cdll_int.h"
#include "tier2/tier2.h"
#include <time.h>

#ifdef	GAME_DLL
#include "gameinterface.h"
#include "steamworks_gamestats_server.h"
#elif	CLIENT_DLL
#include "c_playerresource.h"
#include "steamworks_gamestats_client.h"
#endif

#include "steam/isteamutils.h"

#include "steamworks_gamestats.h"
#include "achievementmgr.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

// This is used to replicate our server id to the client so that client data can be associated with the server's.
ConVar	steamworks_sessionid_server( "steamworks_sessionid_server", "0", FCVAR_REPLICATED | FCVAR_HIDDEN, "The server session ID for the new steamworks gamestats." );

// This is used to show when the steam works is uploading stats
#define steamworks_show_uploads 0

// This is a stop gap to disable the steam works game stats from ever initializing in the event that we need it
#define steamworks_stats_disable 0

// This is used to control when the stats get uploaded. If we wait until the end of the session, we miss out on all the stats if the server crashes. If we upload as we go, then we will have the data
#define steamworks_immediate_upload 1

//-----------------------------------------------------------------------------
// Purpose: Returns the time since the epoch
//-----------------------------------------------------------------------------
time_t CSteamWorksGameStatsUploader::GetTimeSinceEpoch( void )
{
#if !defined( NO_STEAM )
	if ( steamapicontext && steamapicontext->SteamUtils() )
		return steamapicontext->SteamUtils()->GetServerRealTime();
	else
#endif
	{
		// Default to system time.
		time_t aclock;
		time( &aclock );
		return aclock;
	}
}
//-----------------------------------------------------------------------------
// Purpose: Constructor. Sets up the steam callbacks accordingly depending on client/server dll 
//-----------------------------------------------------------------------------
CSteamWorksGameStatsUploader::CSteamWorksGameStatsUploader( const char *pszSystemName, const char *pszSessionConVarName ) : CAutoGameSystemPerFrame( pszSystemName )
{
	m_sSessionConVarName = pszSessionConVarName;
	m_pSessionConVar = NULL;
}

CSteamWorksGameStatsUploader::~CSteamWorksGameStatsUploader()
{
	if ( m_pSessionConVar!=NULL )
	{
		delete m_pSessionConVar;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reset uploader state.
//-----------------------------------------------------------------------------
void CSteamWorksGameStatsUploader::Reset()
{
	ClearSessionID();

	m_ServiceTicking = false;
	m_LastServiceTick = 0;
	m_SessionIDRequestUnsent = false;
	m_SessionIDRequestPending = false;
	m_SessionID = 0;
	m_UploadedStats = false;
	m_bCollectingAny = false;
	m_bCollectingDetails = false;
	m_UserID = 0;
	m_iAppID = 0;

	//Note: Reset() no longer clears serverIP, Map, or HostName data
	//Previously, this was appropriate because sessions only began at server connect/map change
	//Today, sessions are generated every time a match restarts, leading to OGS sessions without server info
	//Instead, we only nuke/initialize during Init(), and selectively wherever ResetServerState() is called.	

	m_StartTime = 0;
	m_EndTime = 0; 	
	m_ActiveSession.Reset();
	m_iServerConnectCount = 0;	 

	for ( int i=0; i<m_StatsToSend.Count(); ++i )
	{
		m_StatsToSend[i]->deleteThis();
	}

	m_StatsToSend.RemoveAll();
}

//------------------------------------------------------------------------------------------
// Purpose: Supplemental reset function to return server IP/host name/map to default state
// during Init(), "cs_game_disconnected" event, and available if relevant for other events.
//------------------------------------------------------------------------------------------

void CSteamWorksGameStatsUploader::ResetServerState()
{
	m_iServerIP = 0;
	memset( m_pzServerIP, 0, ARRAYSIZE(m_pzServerIP) );
	memset( m_pzMapStart, 0, ARRAYSIZE(m_pzMapStart) );
	memset( m_pzHostName, 0, ARRAYSIZE(m_pzHostName) );  
}

//-----------------------------------------------------------------------------
// Purpose: Init function from CAutoGameSystemPerFrame and must return true.
//-----------------------------------------------------------------------------
bool CSteamWorksGameStatsUploader::Init()
{
	if ( !m_sSessionConVarName.IsEmpty() )
	{
		m_pSessionConVar = new ConVarRef( m_sSessionConVarName.Get() );
		if ( !m_pSessionConVar->IsValid() )
		{
			delete m_pSessionConVar;
			m_pSessionConVar = NULL;
		}
	}
	else
	{
		m_pSessionConVar = NULL;
	}

	Reset();
	ResetServerState();
	ListenForGameEvent( "server_spawn" );
	ListenForGameEvent( "hostname_changed" );
#ifdef CLIENT_DLL
	ListenForGameEvent( "player_changename" );
#endif

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Event handler for gathering basic info as well as ending sessions.
//-----------------------------------------------------------------------------
void CSteamWorksGameStatsUploader::FireGameEvent( IGameEvent *event )
{
	if ( !event )
	{
		return;
	}

	const char *pEventName = event->GetName();
	if ( FStrEq( "hostname_changed", pEventName ) )
	{
		const char *pzHostname = event->GetString( "hostname" );
		if ( pzHostname )
		{
			V_strncpy( m_pzHostName, pzHostname, sizeof( m_pzHostName ) );
		}
		else
		{
			V_strncpy( m_pzHostName, "No Host Name", sizeof( m_pzHostName ) );
		}
	}
	else if ( FStrEq( "server_spawn", pEventName ) )
	{
		const char *pzAddress = event->GetString( "address" );
		if ( pzAddress )
		{
			V_snprintf( m_pzServerIP, ARRAYSIZE(m_pzServerIP), "%s:%d", pzAddress, event->GetInt( "port" ) );
			ServerAddressToInt();
		}
		else
		{
			V_strncpy( m_pzServerIP, "No Server Address", sizeof( m_pzServerIP ) );
			m_iServerIP = 0;
		}

		const char *pzHostname = event->GetString( "hostname" );
		if ( pzHostname )
		{
			V_strncpy( m_pzHostName, pzHostname, sizeof( m_pzHostName ) );
		}
		else
		{
			V_strncpy( m_pzHostName, "No Host Name", sizeof( m_pzHostName ) );
		}
		const char *pzMapName = event->GetString( "mapname" );
		if ( pzMapName )
		{
			V_strncpy( m_pzMapStart, pzMapName, sizeof( m_pzMapStart ) );
		}
		else
		{
			V_strncpy( m_pzMapStart, "No Map Name", sizeof( m_pzMapStart ) );
		}

		m_bPassword = event->GetBool( "password" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Requests a session ID from steam.
//-----------------------------------------------------------------------------
EResult	CSteamWorksGameStatsUploader::RequestSessionID()
{
	// If we have disabled steam works game stats, don't request ids.
	if ( steamworks_stats_disable )
	{
		DevMsg( "Steamworks Stats: %s No stats collection because steamworks_stats_disable is set to 1.\n", Name() );
		return k_EResultAccessDenied;
	}

	// Do not continue if we already have a session id.
	// We must end a session before we can begin a new one.
	if ( m_SessionID )
	{
		return k_EResultOK;
	}

	// Do not continue if we are waiting a server response for this session request.
	if ( m_SessionIDRequestPending )
	{
		return k_EResultPending;
	}

	// If a request is unsent, it will be sent as soon as the steam API is available.
	m_SessionIDRequestUnsent = true;

	// Turn on polling.
	m_ServiceTicking = true;

	// If we can't use Steam at the moment, we need to wait.
	if ( !AccessToSteamAPI() )
	{
		return k_EResultNoConnection;
	}

	m_SteamWorksInterface = GetInterface();
	if ( m_SteamWorksInterface )
	{
		int accountType = GetGameStatsAccountType();

		DevMsg( "Steamworks Stats: %s Requesting session id.\n", Name() );

		m_SessionIDRequestUnsent = false;
		m_SessionIDRequestPending = true;

		// This initiates a callback that will get us our session ID.
		// Callback: Steam_OnSteamSessionInfoIssued
		SteamAPICall_t hSteamAPICall = m_SteamWorksInterface->GetNewSession( accountType, m_UserID, m_iAppID, GetTimeSinceEpoch() );
		m_CallbackSteamSessionInfoIssued.Set( hSteamAPICall, this, &CSteamWorksGameStatsUploader::Steam_OnSteamSessionInfoIssued );
	}

	return k_EResultOK;
}

//-----------------------------------------------------------------------------
// Purpose: Clears our session id and session id convar.
//-----------------------------------------------------------------------------
void CSteamWorksGameStatsUploader::ClearSessionID()
{
	m_SessionID = 0;
	if ( m_pSessionConVar != NULL )
	{
		m_pSessionConVar->SetValue( 0 );
	}
}
#ifndef	NO_STEAM

//-----------------------------------------------------------------------------
// Purpose: The steam callback used to get our session IDs.
//-----------------------------------------------------------------------------
void CSteamWorksGameStatsUploader::Steam_OnSteamSessionInfoIssued( GameStatsSessionIssued_t *pGameStatsSessionInfo, bool bError )
{
	OnSteamSessionIssued( pGameStatsSessionInfo, bError );
}

void CSteamWorksGameStatsUploader::OnSteamSessionIssued( GameStatsSessionIssued_t *pGameStatsSessionInfo, bool bError ) 
{
	if ( !m_SessionIDRequestPending )
	{
		// There is no request outstanding.
		return;
	}

	m_SessionIDRequestPending = false;

	if ( !pGameStatsSessionInfo )
	{
		// Empty callback!
		ClearSessionID();
		return;
	}

	if ( pGameStatsSessionInfo->m_eResult != k_EResultOK )
	{
		DevMsg( "Steamworks Stats: %s session id not available.\n", Name() );
		m_SessionIDRequestUnsent = true; // Queue to re-request a session ID.
		ClearSessionID();
		return;
	}

	DevMsg( "Steamworks Stats: %s Received CLIENT session id: %llu\n", Name(), pGameStatsSessionInfo->m_ulSessionID );

	m_StartTime = GetTimeSinceEpoch();

	m_SessionID = pGameStatsSessionInfo->m_ulSessionID;
	m_bCollectingAny = pGameStatsSessionInfo->m_bCollectingAny;
	m_bCollectingDetails = pGameStatsSessionInfo->m_bCollectingDetails;

	char sessionIDString[ 32 ];
	Q_snprintf( sessionIDString, sizeof( sessionIDString ), "%llu", m_SessionID );

	if ( m_pSessionConVar != NULL )
	{
		m_pSessionConVar->SetValue( sessionIDString );
	}
}

//-----------------------------------------------------------------------------
// Purpose: The steam callback to notify us that we've submitted stats.
//-----------------------------------------------------------------------------
void CSteamWorksGameStatsUploader::Steam_OnSteamSessionInfoClosed( GameStatsSessionClosed_t *pGameStatsSessionInfo, bool bError )
{
	OnSteamSessionClosed( pGameStatsSessionInfo, bError );
}

void CSteamWorksGameStatsUploader::OnSteamSessionClosed( GameStatsSessionClosed_t *pGameStatsSessionInfo, bool bError )
{
	if ( !m_UploadedStats )
		return;

	m_UploadedStats = false;
}


//-----------------------------------------------------------------------------
// Purpose: Per frame think. Used to periodically check if we have queued operations.
// For example: we may request a session id before steam is ready.
//-----------------------------------------------------------------------------
#if defined ( GAME_DLL )
void CSteamWorksGameStatsUploader::FrameUpdatePostEntityThink()
{
	if ( !m_ServiceTicking )
		return;

	if ( gpGlobals->realtime - m_LastServiceTick < 3 )
		return;
	m_LastServiceTick = gpGlobals->realtime;

	if ( !AccessToSteamAPI() )
		return;

	// Try to resend our request.
	if ( m_SessionIDRequestUnsent )
	{
		RequestSessionID();
		return;
	}

	// If we had nothing to resend, stop ticking.
	m_ServiceTicking = false;

}
#endif // GAME_DLL

#endif // !NO_STEAM

//-----------------------------------------------------------------------------
// Purpose: Opens a session: requests the session id, etc.
//-----------------------------------------------------------------------------
void CSteamWorksGameStatsUploader::StartSession()
{
	RequestSessionID();
}

//-----------------------------------------------------------------------------
// Purpose: Completes a session for the given type.
//-----------------------------------------------------------------------------
void CSteamWorksGameStatsUploader::EndSession()
{
	m_EndTime = GetTimeSinceEpoch();

	if ( !m_SessionID )
	{
		// No session to end.
		return;
	}

	m_SteamWorksInterface = GetInterface();
	if ( m_SteamWorksInterface )
	{
		DevMsg( "Steamworks Stats: %s Ending CLIENT session id: %llu\n", Name(), m_SessionID );


		// Flush any stats that haven't been sent yet.
		FlushStats();

		// Always need some data in the session row or we'll crash steam.
		WriteSessionRow();
		SteamAPICall_t hSteamAPICall = m_SteamWorksInterface->EndSession( m_SessionID, m_EndTime, 0 );
		m_CallbackSteamSessionInfoClosed.Set( hSteamAPICall, this, &CSteamWorksGameStatsUploader::Steam_OnSteamSessionInfoClosed );

		Reset();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Flush any unsent rows.
//-----------------------------------------------------------------------------
void CSteamWorksGameStatsUploader::FlushStats()
{
	for ( int i=0; i<m_StatsToSend.Count(); ++i )
	{
		ParseKeyValuesAndSendStats( m_StatsToSend[i] );
		m_StatsToSend[i]->deleteThis();
	}

	m_StatsToSend.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Uploads any end of session rows.
//-----------------------------------------------------------------------------
void CSteamWorksGameStatsUploader::WriteSessionRow()
{
	m_SteamWorksInterface = GetInterface();
	if ( !m_SteamWorksInterface )
		return;

	// The Session row is common to both client and server sessions.
	// It enables keying to other tables.

	m_SteamWorksInterface->AddSessionAttributeInt( m_SessionID, "AppID", m_iAppID );
	m_SteamWorksInterface->AddSessionAttributeInt( m_SessionID, "StartTime", m_StartTime );		
	m_SteamWorksInterface->AddSessionAttributeInt( m_SessionID, "EndTime", m_EndTime );
}


//-----------------------------------------------------------------------------
// DATA ACCESS UTILITIES
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Verifies that we have a valid interface and will attempt to obtain a new one if we don't.
//-----------------------------------------------------------------------------
bool CSteamWorksGameStatsUploader::VerifyInterface( void )
{
	if ( !m_SteamWorksInterface )
	{
		m_SteamWorksInterface = GetInterface();
		if ( !m_SteamWorksInterface )
		{
			return false; 
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Wrapper function to write an int32 to a table given the row name
//-----------------------------------------------------------------------------
EResult CSteamWorksGameStatsUploader::WriteIntToTable( const int value, uint64 iTableID, const char *pzRow )
{
	if ( !VerifyInterface() )
		return k_EResultNoConnection;

	return m_SteamWorksInterface->AddRowAttributeInt( iTableID, pzRow, value );	
}

//-----------------------------------------------------------------------------
// Purpose: Wrapper function to write an int64 to a table given the row name
//-----------------------------------------------------------------------------
EResult CSteamWorksGameStatsUploader::WriteInt64ToTable( const uint64 value, uint64 iTableID, const char *pzRow )
{
	if ( !VerifyInterface() )
		return k_EResultNoConnection;

	return m_SteamWorksInterface->AddRowAttributeInt64( iTableID, pzRow, value );
}

//-----------------------------------------------------------------------------
// Purpose: Wrapper function to write an float to a table given the row name
//-----------------------------------------------------------------------------
EResult CSteamWorksGameStatsUploader::WriteFloatToTable( const float value, uint64 iTableID, const char *pzRow )
{
	if ( !VerifyInterface() )
		return k_EResultNoConnection;

	return m_SteamWorksInterface->AddRowAttributeFloat( iTableID, pzRow, value );
}

//-----------------------------------------------------------------------------
// Purpose: Wrapper function to write an string to a table given the row name
//-----------------------------------------------------------------------------
EResult CSteamWorksGameStatsUploader::WriteStringToTable( const char *value, uint64 iTableID, const char *pzRow )
{
	if ( !VerifyInterface() )
		return k_EResultNoConnection;

	return m_SteamWorksInterface->AddRowAtributeString( iTableID, pzRow, value );
}

//-----------------------------------------------------------------------------
// STEAM ACCESS UTILITIES
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Determines if the system can connect to steam
//-----------------------------------------------------------------------------
bool CSteamWorksGameStatsUploader::AccessToSteamAPI( void )
{
#if !defined( NO_STEAM )
#ifdef	GAME_DLL
	return ( steamgameserverapicontext && steamgameserverapicontext->SteamGameServer() && steamgameserverapicontext->SteamClient() && steamgameserverapicontext->SteamGameServerUtils() );
#elif	CLIENT_DLL
	return ( steamapicontext && steamapicontext->SteamUser() && steamapicontext->SteamUser()->BLoggedOn() && steamapicontext->SteamFriends() && steamapicontext->SteamMatchmaking() );
#endif
#endif
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: There's no guarantee that your interface pointer will persist across level transitions,
//			so this function will update your interface.
//-----------------------------------------------------------------------------
ISteamGameStats* CSteamWorksGameStatsUploader::GetInterface( void )
{
#if !defined( NO_STEAM )

	HSteamUser hSteamUser = 0;
	HSteamPipe hSteamPipe = 0;

#ifdef	GAME_DLL
	if ( steamgameserverapicontext && steamgameserverapicontext->SteamGameServer() && steamgameserverapicontext->SteamGameServerUtils() )
	{
		m_UserID = steamgameserverapicontext->SteamGameServer()->GetSteamID().ConvertToUint64();
		m_iAppID = steamgameserverapicontext->SteamGameServerUtils()->GetAppID();
		hSteamUser = SteamGameServer_GetHSteamUser();
		hSteamPipe = SteamGameServer_GetHSteamPipe();
	}

	// Now let's get the interface for dedicated servers
	if ( steamgameserverapicontext && steamgameserverapicontext->SteamClient() && engine && engine->IsDedicatedServer() )
	{
		return (ISteamGameStats*) steamgameserverapicontext->SteamClient()->GetISteamGenericInterface( hSteamUser, hSteamPipe, STEAMGAMESTATS_INTERFACE_VERSION );
	}

#elif	CLIENT_DLL
	if ( steamapicontext && steamapicontext->SteamUser() && steamapicontext->SteamUtils() )
	{
		m_UserID = steamapicontext->SteamUser()->GetSteamID().ConvertToUint64();
		m_iAppID = steamapicontext->SteamUtils()->GetAppID();
		hSteamUser = steamapicontext->SteamUser()->GetHSteamUser();
		hSteamPipe = GetHSteamPipe();
	}
#endif

	// Listen server have access to SteamClient
	if ( steamapicontext && steamapicontext->SteamClient() )
	{
		return (ISteamGameStats*)steamapicontext->SteamClient()->GetISteamGenericInterface( hSteamUser, hSteamPipe, STEAMGAMESTATS_INTERFACE_VERSION );
	}

#endif // !NO_STEAM

	// If we haven't returned already, then we can't get access to the interface
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a table from the KeyValue file. Do NOT send nested KeyValue objects into this function!
//-----------------------------------------------------------------------------
EResult CSteamWorksGameStatsUploader::AddStatsForUpload( KeyValues *pKV, bool bSendImmediately )
{
	// If the stat system is disabled, then don't accept the keyvalue
	if ( steamworks_stats_disable )
	{
		if ( pKV )
		{
			pKV->deleteThis();
		}

		return k_EResultNoConnection;
	}

	if ( pKV )
	{
		// Do we want to immediately upload the stats?
		if ( bSendImmediately && steamworks_immediate_upload )
		{
			ParseKeyValuesAndSendStats( pKV );
			pKV->deleteThis();
		}
		else
		{
			m_StatsToSend.AddToTail( pKV );
		}
		return k_EResultOK;
	}

	return k_EResultFail;
}

//-----------------------------------------------------------------------------
// Purpose: Parses all the keyvalue files we've been sent and creates tables from them and uploads them
//-----------------------------------------------------------------------------
double g_rowCommitTime = 0.0f;
double g_rowWriteTime = 0.0f;
EResult CSteamWorksGameStatsUploader::ParseKeyValuesAndSendStats( KeyValues *pKV )
{
	if ( !pKV )
	{
		return k_EResultFail;
	}

	if ( !IsCollectingAnyData() )
	{
		return k_EResultFail;
	}

	// Refresh the interface in case steam has unloaded
	m_SteamWorksInterface = GetInterface();

	if ( !m_SteamWorksInterface )
	{
		DevMsg( "WARNING: Attempted to send a steamworks gamestats row when the steamworks interface was not available!" );
		return k_EResultNoConnection;
	}

	const char *pzTable = pKV->GetName();

	if ( steamworks_show_uploads )
	{
#ifdef	CLIENT_DLL
		DevMsg( "Client submitting row (%s).\n", pzTable );
#elif	GAME_DLL
		DevMsg( "Server submitting row (%s).\n", pzTable );
#endif

		KeyValuesDumpAsDevMsg( pKV, 1 );
	}

	uint64 iTableID = 0;
	m_SteamWorksInterface->AddNewRow( &iTableID, m_SessionID, pzTable );

	if ( !iTableID )
	{
		return k_EResultFail;
	}

	AddSessionIDsToTable( iTableID );

	// Now we need to loop over all the keys in pKV and add the name and value
	for ( KeyValues *pData = pKV->GetFirstSubKey() ; pData != NULL ; pData = pData->GetNextKey() )
	{
		const char *name = pData->GetName();
		CFastTimer writeTimer;
		writeTimer.Start();
		switch ( pData->GetDataType() )
		{
		case KeyValues::TYPE_STRING:	WriteStringToTable( pKV->GetString( name ), iTableID, name );
			break;
		case KeyValues::TYPE_INT:		WriteIntToTable( pKV->GetInt( name ), iTableID, name );
			break;
		case KeyValues::TYPE_FLOAT:		WriteFloatToTable( pKV->GetFloat( name ), iTableID, name );
			break;
		case KeyValues::TYPE_UINT64:	WriteInt64ToTable( pKV->GetUint64( name ), iTableID, name );
			break;
		};
		writeTimer.End();
		g_rowWriteTime += writeTimer.GetDuration().GetMillisecondsF();
	}

	CFastTimer commitTimer;
	commitTimer.Start();
	EResult res = m_SteamWorksInterface->CommitRow( iTableID );
	commitTimer.End();
	g_rowCommitTime += commitTimer.GetDuration().GetMillisecondsF();

	if ( res != k_EResultOK )
	{
		char pzMessage[MAX_PATH] = {0};
		V_snprintf( pzMessage, ARRAYSIZE(pzMessage), "Failed To Submit table %s", pzTable );
		Assert( pzMessage );
	}
	return res;
}

#ifdef	CLIENT_DLL

#endif

void CSteamWorksGameStatsUploader::ServerAddressToInt()
{
	CUtlStringList IPs;
	V_SplitString( m_pzServerIP, ".", IPs );

	if ( IPs.Count() < 4 )
	{
		// Not an actual IP.
		m_iServerIP = 0;
		return;
	}

	byte ip[4];
	m_iServerIP = 0;
	for ( int i=0; i<IPs.Count() && i<4; ++i )
	{
		ip[i] = (byte) Q_atoi( IPs[i] );
	}
	m_iServerIP = (ip[0]<<24) + (ip[1]<<16) + (ip[2]<<8) + ip[3];
}

//=============================================================================
//
// Helper functions for creating key values
//
void AddDataToKV( KeyValues* pKV, const char* name, int data )
{
	pKV->SetInt( name, data );
}
void AddDataToKV( KeyValues* pKV, const char* name, uint64 data )
{
	pKV->SetUint64( name, data );
}
void AddDataToKV( KeyValues* pKV, const char* name, float data )
{
	pKV->SetFloat( name, data );
}
void AddDataToKV( KeyValues* pKV, const char* name, bool data )
{
	pKV->SetInt( name, data ? true : false );
}
void AddDataToKV( KeyValues* pKV, const char* name, const char* data )
{
	pKV->SetString( name, data );
}
void AddDataToKV( KeyValues* pKV, const char* name, const Color& data )
{
	pKV->SetColor( name, data );
}
void AddDataToKV( KeyValues* pKV, const char* name, short data )
{
	pKV->SetInt( name, data );
}
void AddDataToKV( KeyValues* pKV, const char* name, unsigned data )
{
	pKV->SetInt( name, data );
}
void AddPositionDataToKV( KeyValues* pKV, const char* name, const Vector &data )
{
	// Append the data name to the member
	pKV->SetFloat( CFmtStr("%s%s", name, "_X"), data.x );
	pKV->SetFloat( CFmtStr("%s%s", name, "_Y"), data.y );
	pKV->SetFloat( CFmtStr("%s%s", name, "_Z"), data.z );
}

//=============================================================================//

//=============================================================================
//
// Helper functions for creating key values from arrays
//
void AddArrayDataToKV( KeyValues* pKV, const char* name, const short *data, unsigned size )
{
	for( unsigned i=0; i<size; ++i )
		pKV->SetInt( CFmtStr("%s_%d", name, i) , data[i] );
}
void AddArrayDataToKV( KeyValues* pKV, const char* name, const byte *data, unsigned size )
{
	for( unsigned i=0; i<size; ++i )
		pKV->SetInt( CFmtStr("%s_%d", name, i), data[i] );
}
void AddArrayDataToKV( KeyValues* pKV, const char* name, const unsigned *data, unsigned size )
{
	for( unsigned i=0; i<size; ++i )
		pKV->SetInt( CFmtStr("%s_%d", name, i), data[i] );
}
void AddStringDataToKV( KeyValues* pKV, const char* name, const char*data )
{
	if( name == NULL )
		return;

	pKV->SetString( name, data );
}
//=============================================================================//


void IGameStatTracker::PrintGamestatMemoryUsage( void )
{
	StatContainerList_t* pStatList = GetStatContainerList();
	if( !pStatList )
		return;

	int iListSize = pStatList->Count();

	// For every stat list being tracked, print out its memory usage
	for( int i=0; i < iListSize; ++i )
	{
		pStatList->operator []( i )->PrintMemoryUsage();
	}
}

#endif
