//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Uploads KeyValue stats to the new SteamWorks gamestats system.
//
//=============================================================================

#include "cbase.h"
#include "cdll_int.h"
#include "tier2/tier2.h"
#include <time.h>

#include "c_playerresource.h"

#include "steam/isteamutils.h"

#include "steamworks_gamestats_client.h"
#include "icommandline.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

ConVar	steamworks_sessionid_client( "steamworks_sessionid_client", "0", FCVAR_HIDDEN, "The client session ID for the new steamworks gamestats." );
extern ConVar steamworks_sessionid_server;

static CSteamWorksGameStatsClient g_SteamWorksGameStatsClient;

extern ConVar developer;

void Show_Steam_Stats_Session_ID( void )
{
	DevMsg( "Client session ID (%s).\n", steamworks_sessionid_client.GetString() );
	DevMsg( "Server session ID (%s).\n", steamworks_sessionid_server.GetString() );
}
static ConCommand ShowSteamStatsSessionID( "ShowSteamStatsSessionID", Show_Steam_Stats_Session_ID, "Prints out the game stats session ID's (developer convar must be set to non-zero).", FCVAR_DEVELOPMENTONLY );

//-----------------------------------------------------------------------------
// Purpose: Clients store the server's session IDs so we can associate client rows with server rows.
//-----------------------------------------------------------------------------
void ServerSessionIDChangeCallback( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	if ( var.IsValid() )
	{
		// Treat the variable as a string, since the sessionID is 64 bit and the convar int interface is only 32 bit.
		const char* pVarString = var.GetString();
		uint64 newServerSessionID = Q_atoi64( pVarString );
		GetSteamWorksGameStatsClient().SetServerSessionID( newServerSessionID );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns a reference to the global object
//-----------------------------------------------------------------------------
CSteamWorksGameStatsClient& GetSteamWorksGameStatsClient()
{
	return g_SteamWorksGameStatsClient;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor. Sets up the steam callbacks accordingly depending on client/server dll 
//-----------------------------------------------------------------------------
CSteamWorksGameStatsClient::CSteamWorksGameStatsClient() : BaseClass( "CSteamWorksGameStatsClient", "steamworks_sessionid_client" )
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: Reset uploader state.
//-----------------------------------------------------------------------------
void CSteamWorksGameStatsClient::Reset()
{
	BaseClass::Reset();

	m_HumanCntInGame = 0;
	m_FriendCntInGame = 0;
	memset( m_pzPlayerName, 0, ARRAYSIZE(m_pzPlayerName) );
	steamworks_sessionid_client.SetValue( 0 );
	ClearServerSessionID();
}

//-----------------------------------------------------------------------------
// Purpose: Init function from CAutoGameSystemPerFrame and must return true.
//-----------------------------------------------------------------------------
bool CSteamWorksGameStatsClient::Init()
{
	BaseClass::Init();

	// TODO: This event doesn't exist in dota. Hook it up?
	//ListenForGameEvent( "client_disconnect" );

	steamworks_sessionid_server.InstallChangeCallback( ServerSessionIDChangeCallback );

	return true;
}

void CSteamWorksGameStatsClient::AddSessionIDsToTable( int iTableID )
{
	// Our client side session.
	WriteInt64ToTable( m_SessionID, iTableID, "SessionID" );

	// The session of the server we are connected to.
	WriteInt64ToTable( m_ServerSessionID, iTableID, "ServerSessionID" );
}

//-----------------------------------------------------------------------------
// Purpose: Event handler for gathering basic info as well as ending sessions.
//-----------------------------------------------------------------------------
void CSteamWorksGameStatsClient::FireGameEvent( IGameEvent *event )
{
	if ( !event )
		return;

	const char *pEventName = event->GetName();
	if ( FStrEq( "client_disconnect", pEventName ) )
	{
		ClientDisconnect();
	}	
	else if ( FStrEq( "player_changename", pEventName ) )
	{
		V_strncpy( m_pzPlayerName, event->GetString( "newname", "No Player Name" ), sizeof( m_pzPlayerName ) );
	}
	else
	{
		BaseClass::FireGameEvent( event );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Sets the server session ID but ONLY if it's not 0. We are using this to avoid a race 
// 			condition where a server sends their session stats before a client does, thereby,
//			resetting the client's server session ID to 0.
//-----------------------------------------------------------------------------
void CSteamWorksGameStatsClient::SetServerSessionID( uint64 serverSessionID )
{
	if ( !serverSessionID )
		return;

	if ( serverSessionID != m_ActiveSession.m_ServerSessionID )
	{
		m_ActiveSession.m_ServerSessionID = serverSessionID;
		m_ActiveSession.m_ConnectTime = GetTimeSinceEpoch();
		m_ActiveSession.m_DisconnectTime = 0;

		m_iServerConnectCount++;
	}

	m_ServerSessionID = serverSessionID;
}

//-----------------------------------------------------------------------------
// Purpose:	Writes the disconnect time to the current server session entry.
//-----------------------------------------------------------------------------
void CSteamWorksGameStatsClient::ClientDisconnect()
{
	Assert( 0 ); // i want to remove this.
	if ( m_ActiveSession.m_ServerSessionID == 0 )
		return;

	m_SteamWorksInterface = GetInterface();
	if ( !m_SteamWorksInterface )
		return;

	if ( !IsCollectingAnyData() )
		return;

	uint64 ulRowID = 0;
	m_SteamWorksInterface->AddNewRow( &ulRowID,	m_SessionID, "ClientSessionLookup" );
	WriteInt64ToTable(	m_SessionID,							ulRowID,	"SessionID" );
	WriteInt64ToTable(	m_ActiveSession.m_ServerSessionID,		ulRowID,	"ServerSessionID" );
	WriteIntToTable(	m_ActiveSession.m_ConnectTime,			ulRowID,	"ConnectTime" );
	WriteIntToTable(	GetTimeSinceEpoch(),					ulRowID,	"DisconnectTime" );
	m_SteamWorksInterface->CommitRow( ulRowID );

	m_ActiveSession.Reset();
}

//-----------------------------------------------------------------------------
// Purpose: Uploads any end of session rows.
//-----------------------------------------------------------------------------
void CSteamWorksGameStatsClient::WriteSessionRow()
{
	m_SteamWorksInterface = GetInterface();
	if ( !m_SteamWorksInterface )
		return;

	m_SteamWorksInterface->AddSessionAttributeInt64( m_SessionID, "ServerSessionID", m_ServerSessionID );
	m_SteamWorksInterface->AddSessionAttributeString( m_SessionID, "ServerIP", m_pzServerIP );
	m_SteamWorksInterface->AddSessionAttributeString( m_SessionID, "ServerName", m_pzHostName );
	m_SteamWorksInterface->AddSessionAttributeString( m_SessionID, "StartMap", m_pzMapStart );

	m_SteamWorksInterface->AddSessionAttributeString( m_SessionID, "PlayerName", m_pzPlayerName );
	m_SteamWorksInterface->AddSessionAttributeInt( m_SessionID, "PlayersInGame", m_HumanCntInGame );
	m_SteamWorksInterface->AddSessionAttributeInt( m_SessionID, "FriendsInGame", m_FriendCntInGame );

	BaseClass::WriteSessionRow();
}

void CSteamWorksGameStatsClient::OnSteamSessionIssued( GameStatsSessionIssued_t *pResult, bool bError )
{
	BaseClass::OnSteamSessionIssued( pResult, bError );
	m_FriendCntInGame = GetFriendCountInGame();

	CBasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	const char *pPlayerName = pPlayer ? pPlayer->GetPlayerName() : "unknown";
	V_strncpy( m_pzPlayerName, pPlayerName, sizeof( m_pzPlayerName ) );
}

//-----------------------------------------------------------------------------
// Purpose: Reports client's perf data at the end of a client session.
//-----------------------------------------------------------------------------
void CSteamWorksGameStatsClient::AddClientPerfData( KeyValues *pKV )
{
	m_SteamWorksInterface = GetInterface();
	if ( !m_SteamWorksInterface )
		return;

	if ( !IsCollectingAnyData() )
		return;

	RTime32 currentTime = GetTimeSinceEpoch();

	uint64 uSessionID = m_SessionID;
	uint64 ulRowID = 0;

	m_SteamWorksInterface->AddNewRow( &ulRowID, uSessionID, "CSGOClientPerfData" );

	if ( !ulRowID )
		return;

	WriteInt64ToTable(	m_SessionID,						ulRowID,	"SessionID" );
	//	WriteInt64ToTable(	m_ServerSessionID,					ulRowID,	"ServerSessionID" );
	WriteIntToTable(	currentTime,						ulRowID,	"TimeSubmitted" );
	//WriteStringToTable( pKV->GetString( "Map/mapname" ),	ulRowID,	"MapID");
	WriteIntToTable(	pKV->GetInt( "appid" ),				ulRowID,	"AppID");
	WriteFloatToTable(	pKV->GetFloat( "Map/perfdata/AvgFPS" ), ulRowID, "AvgFPS");
	WriteFloatToTable(	pKV->GetFloat( "map/perfdata/MinFPS" ), ulRowID, "MinFPS");
	WriteFloatToTable(	pKV->GetFloat( "Map/perfdata/MaxFPS" ), ulRowID, "MaxFPS");
	WriteFloatToTable(	pKV->GetFloat( "Map/perfdata/StdDevFPS" ), ulRowID, "StdDevFPS");
	WriteInt64ToTable( pKV->GetUint64( "Map/perfdata/FrameHistAll" ), ulRowID, "FrameHistAll" );
	WriteInt64ToTable( pKV->GetUint64( "Map/perfdata/FrameHistGame1" ), ulRowID, "FrameHistGame1" );
	WriteInt64ToTable( pKV->GetUint64( "Map/perfdata/FrameHistGame2" ), ulRowID, "FrameHistGame2" );
	WriteInt64ToTable( pKV->GetUint64( "Map/perfdata/FrameHistGame3" ), ulRowID, "FrameHistGame3" );
	WriteInt64ToTable( pKV->GetUint64( "Map/perfdata/FrameHistGame4" ), ulRowID, "FrameHistGame4" );
	WriteStringToTable( pKV->GetString( "CPUID" ), ulRowID, "CPUID" );
	WriteFloatToTable(	pKV->GetFloat( "CPUGhz" ),			ulRowID,	"CPUGhz");
	WriteInt64ToTable( pKV->GetUint64( "CPUModel" ),		ulRowID, "CPUModel" );
	WriteInt64ToTable( pKV->GetUint64( "CPUFeatures0" ),	ulRowID, "CPUFeatures0" );
	WriteInt64ToTable( pKV->GetUint64( "CPUFeatures1" ),	ulRowID, "CPUFeatures1" );
	WriteInt64ToTable( pKV->GetUint64( "CPUFeatures2" ),	ulRowID, "CPUFeatures2" );
	WriteIntToTable( pKV->GetInt( "NumCores" ), ulRowID, "NumCores" );
	WriteStringToTable( pKV->GetString( "GPUDrv" ),			ulRowID,	"GPUDrv");
	WriteIntToTable(	pKV->GetInt( "GPUVendor" ),			ulRowID,	"GPUVendor");
	WriteIntToTable(	pKV->GetInt( "GPUDeviceID" ),		ulRowID,	"GPUDeviceID");
	WriteIntToTable(	pKV->GetInt( "GPUDriverVersion" ),	ulRowID,	"GPUDriverVersion");
	WriteIntToTable(	pKV->GetInt( "DxLvl" ),				ulRowID,	"DxLvl");
	WriteIntToTable(	pKV->GetInt( "IsSplitScreen" ),		ulRowID,	"IsSplitScreen");
	WriteIntToTable(	pKV->GetBool( "Map/Windowed" ),		ulRowID,	"Windowed");
	WriteIntToTable(	pKV->GetBool( "Map/WindowedNoBorder" ), ulRowID, "WindowedNoBorder");
	WriteIntToTable(	pKV->GetInt( "width" ),				ulRowID,	"Width");
	WriteIntToTable(	pKV->GetInt( "height" ),			ulRowID,	"Height");
	WriteIntToTable(	pKV->GetInt( "Map/UsedVoice" ),		ulRowID,	"Usedvoiced");
	WriteStringToTable( pKV->GetString( "Map/Language" ),	ulRowID,	"Language");
	WriteFloatToTable(	pKV->GetFloat( "Map/perfdata/AvgServerPing" ), ulRowID, "AvgServerPing");
	WriteIntToTable(	pKV->GetInt( "Map/Caption" ),		ulRowID,	"IsCaptioned");
	WriteIntToTable(	pKV->GetInt( "IsPC" ),				ulRowID,	"IsPC");
	WriteIntToTable(	pKV->GetInt( "Map/Cheats" ),		ulRowID,	"Cheats");
	WriteIntToTable(	pKV->GetInt( "Map/MapTime" ),		ulRowID,	"MapTime");
	WriteFloatToTable(	pKV->GetFloat( "Map/perfdata/AvgMainThreadTime" ), ulRowID, "AvgMainThreadTime");
	WriteFloatToTable(	pKV->GetFloat( "Map/perfdata/StdDevMainThreadTime" ), ulRowID, "StdMainThreadTime");
	WriteFloatToTable( pKV->GetFloat( "Map/perfdata/AvgMainThreadWaitTime" ), ulRowID, "AvgMainThreadWaitTime" );
	WriteFloatToTable( pKV->GetFloat( "Map/perfdata/StdDevMainThreadWaitTime" ), ulRowID, "StdMainThreadWaitTime" );
	WriteFloatToTable( pKV->GetFloat( "Map/perfdata/AvgRenderThreadTime" ), ulRowID, "AvgRenderThreadTime" );
	WriteFloatToTable( pKV->GetFloat( "Map/perfdata/StdDevRenderThreadTime" ), ulRowID, "StdRenderThreadTime" );
	WriteFloatToTable( pKV->GetFloat( "Map/perfdata/AvgRenderThreadWaitTime" ), ulRowID, "AvgRenderThreadWaitTime" );
	WriteFloatToTable( pKV->GetFloat( "Map/perfdata/StdDevRenderThreadWaitTime" ), ulRowID, "StdRenderThreadWaitTime" );
	WriteFloatToTable( pKV->GetFloat( "Map/perfdata/PercentageCPUThrottledToLevel1" ), ulRowID, "PercentageCPUThrottledToLevel1");
	WriteFloatToTable( pKV->GetFloat( "Map/perfdata/PercentageCPUThrottledToLevel2" ), ulRowID, "PercentageCPUThrottledToLevel2");

	m_SteamWorksInterface->CommitRow( ulRowID );
}


//-----------------------------------------------------------------------------
// Purpose: Reports client's game event stats.
//-----------------------------------------------------------------------------
bool CSteamWorksGameStatsClient::AddCsgoGameEventStat( char const *szMapName, char const *szEvent, Vector const &pos, QAngle const &ang, uint64 ullData, int16 nRound, int16 numRoundSecondsElapsed )
{
	m_SteamWorksInterface = GetInterface();
	if ( !m_SteamWorksInterface )
		return false;

	if ( !IsCollectingAnyData() )
		return false;

	// UNUSED: RTime32 currentTime = GetTimeSinceEpoch();

	uint64 uSessionID = m_SessionID;
	uint64 ulRowID = 0;

	m_SteamWorksInterface->AddNewRow( &ulRowID, uSessionID, "CSGOGameEvent" );

	if ( !ulRowID )
		return false;

	WriteInt64ToTable( m_SessionID, ulRowID, "SessionID" );
	static int s_nCounter = 0;
	WriteIntToTable( ++ s_nCounter, ulRowID, "EventCount" );

	WriteStringToTable( szEvent, ulRowID, "EventID" );
	WriteStringToTable( szMapName, ulRowID, "MapID" );

	WriteIntToTable( (int16) Clamp<float>( pos[0], -MAX_COORD_FLOAT, MAX_COORD_FLOAT ), ulRowID, "PosX" );
	WriteIntToTable( (int16) Clamp<float>( pos[1], -MAX_COORD_FLOAT, MAX_COORD_FLOAT ), ulRowID, "PosY" );
	WriteIntToTable( (int16) Clamp<float>( pos[2], -MAX_COORD_FLOAT, MAX_COORD_FLOAT ), ulRowID, "PosZ" );
	WriteFloatToTable( ang[0], ulRowID, "ViewX" );
	WriteFloatToTable( ang[1], ulRowID, "ViewY" );
	WriteFloatToTable( ang[2], ulRowID, "ViewZ" );
	WriteIntToTable( nRound, ulRowID, "GameRound" );
	WriteIntToTable( numRoundSecondsElapsed, ulRowID, "GameTime" );
	WriteInt64ToTable( ullData, ulRowID, "Data" );

	m_SteamWorksInterface->CommitRow( ulRowID );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Reports client's VPK load stats.
//-----------------------------------------------------------------------------
void CSteamWorksGameStatsClient::AddVPKLoadStats( KeyValues *pKV )
{
	m_SteamWorksInterface = GetInterface();
	if ( !m_SteamWorksInterface )
		return;

	if ( !IsCollectingAnyData() )
		return;

	RTime32 currentTime = GetTimeSinceEpoch();

	uint64 uSessionID = m_SessionID;
	uint64 ulRowID = 0;

	m_SteamWorksInterface->AddNewRow( &ulRowID, uSessionID, "CSGOClientVPKFileStats" );

	if ( !ulRowID )
		return;

	WriteInt64ToTable(	m_SessionID,						ulRowID,	"SessionID" );
	WriteIntToTable(	currentTime,						ulRowID,	"TimeSubmitted" );
	WriteIntToTable(	pKV->GetInt( "BytesReadFromCache" ),	ulRowID,	"BytesReadFromCache");
	WriteIntToTable(	pKV->GetInt( "ItemsReadFromCache" ),	ulRowID,	"ItemsReadFromCache");
	WriteIntToTable(	pKV->GetInt( "DiscardsFromCache" ),		ulRowID,	"DiscardsFromCache");
	WriteIntToTable(	pKV->GetInt( "AddedToCache" ),			ulRowID,	"AddedToCache");
	WriteIntToTable(	pKV->GetInt( "CacheMisses" ),			ulRowID,	"CacheMisses");
	WriteIntToTable(	pKV->GetInt( "FileErrorCount" ),		ulRowID,	"FileErrorCount");
	WriteIntToTable(	pKV->GetInt( "FileErrorsCorrected" ),	ulRowID,	"FileErrorsCorrected");
	WriteIntToTable(	pKV->GetInt( "FileResultsDifferent" ),	ulRowID,	"FileResultsDifferent");

	m_SteamWorksInterface->CommitRow( ulRowID );
}

//-----------------------------------------------------------------------------
// Purpose: Reports any VPK file load errors
//-----------------------------------------------------------------------------
void CSteamWorksGameStatsClient::AddVPKFileLoadErrorData( KeyValues *pKV )
{
	m_SteamWorksInterface = GetInterface();
	if ( !m_SteamWorksInterface )
		return;

	if ( !IsCollectingAnyData() )
		return;

	RTime32 currentTime = GetTimeSinceEpoch();

	uint64 uSessionID = m_SessionID;
	uint64 ulRowID = 0;

	for ( KeyValues *pkvSubKey = pKV->GetFirstTrueSubKey(); pkvSubKey != NULL; pkvSubKey = pkvSubKey->GetNextTrueSubKey() )
	{

		m_SteamWorksInterface->AddNewRow( &ulRowID, uSessionID, "CSGOClientVPKFileError" );

		if ( !ulRowID )
			return;

		WriteInt64ToTable(	m_SessionID,						ulRowID,	"SessionID" );
		WriteIntToTable(	currentTime,						ulRowID,	"TimeSubmitted" );
		WriteIntToTable(	pkvSubKey->GetInt( "PackFileID" ),			ulRowID,	"PackFileID");
		WriteIntToTable(	pkvSubKey->GetInt( "PackFileNumber" ),		ulRowID,	"PackFileNumber");
		WriteIntToTable(	pkvSubKey->GetInt( "FileFraction" ),		ulRowID,	"FileFraction");
		WriteStringToTable(	pkvSubKey->GetString( "ChunkMd5Master" ),	ulRowID,	"ChunkMd5MasterText");
		WriteStringToTable(	pkvSubKey->GetString( "ChunkMd5First" ),	ulRowID,	"ChunkMd5FirstText");
		WriteStringToTable(	pkvSubKey->GetString( "ChunkMd5Second" ),	ulRowID,	"ChunkMd5SecondText");

		m_SteamWorksInterface->CommitRow( ulRowID );
	}
}

//-------------------------------------------------------------------------------------------------
/**
*	Purpose:	Calculates the number of friends in the game
*/
int CSteamWorksGameStatsClient::GetFriendCountInGame()
{
	// Get the number of steam friends in game
	int friendsInOurGame = 0;

#if !defined( NO_STEAM )

	// Do we have access to the steam API?
	if ( AccessToSteamAPI() )
	{
		CSteamID m_SteamID = steamapicontext->SteamUser()->GetSteamID();
		// Let's get our game info so we can use that to test if our friends are connected to the same game as us
		FriendGameInfo_t myGameInfo;
		steamapicontext->SteamFriends()->GetFriendGamePlayed( m_SteamID, &myGameInfo );
		CSteamID myLobby = steamapicontext->SteamMatchmaking()->GetLobbyOwner( myGameInfo.m_steamIDLobby );

		// This returns the number of friends that are playing a game
		int activeFriendCnt = steamapicontext->SteamFriends()->GetFriendCount( k_EFriendFlagImmediate );

		// Check each active friend's lobby ID to see if they are in our game
		for ( int h=0; h< activeFriendCnt ; ++h )
		{
			FriendGameInfo_t friendInfo;
			CSteamID friendID = steamapicontext->SteamFriends()->GetFriendByIndex( h, k_EFriendFlagImmediate );

			if ( steamapicontext->SteamFriends()->GetFriendGamePlayed( friendID, &friendInfo ) )
			{
				// Does our friend have a valid lobby ID?
				if ( friendInfo.m_gameID.IsValid() )
				{
					// Get our friend's lobby info
					CSteamID friendLobby = steamapicontext->SteamMatchmaking()->GetLobbyOwner( friendInfo.m_steamIDLobby );

					// Double check the validity of the friend lobby ID then check to see if they are in our game
					if ( friendLobby.IsValid() && myLobby == friendLobby )
					{
						++friendsInOurGame;
					}
				}
			}
		}
	}

#endif // !NO_STEAM

	return friendsInOurGame;
}
