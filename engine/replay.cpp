//========= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#if defined( REPLAY_ENABLED )
#include "replay.h"
#include "convar.h"
#include "cmd.h"
#include "qlimits.h"
#include "client.h"
#include "server.h"
#include "enginesingleuserfilter.h"
#include "cdll_engine_int.h"
#include "filesystem.h"
#include "replayhistorymanager.h"
#include "toolframework/itoolframework.h"
#include "replayserver.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

CON_COMMAND_F( loadsfm, "Test -- Loads the SFM", 0 )	// TEST
{
	toolframework->LoadFilmmaker();
}

//----------------------------------------------------------------------------------------

ConVar replay_enable( "replay_enable", "0", FCVAR_REPLICATED, "Enable Replay recording on server" );
ConVar replay_snapshotrate("replay_snapshotrate", "16", 0, "Snapshots broadcasted per second" );

static ConVar replay_autoretry( "replay_autoretry", "1", 0, "Relay proxies retry connection after network timeout" );
static ConVar replay_timeout( "replay_timeout", "30", 0, "SourceTV connection timeout in seconds." );

//----------------------------------------------------------------------------------------

bool Replay_IsEnabled()
{
	return replay_enable.GetInt() != 0;
}

//----------------------------------------------------------------------------------------

void Replay_OnFileSendComplete( const char *pFilename, int nSize )
{
	if ( !replay || !replay->IsActive() )
	{
		AssertMsg( 0, "Calling Replay_OnFileSendComplete() with inactive Replay!" );
		return;
	}

	// TODO - how can we get a client index here?  Do we already have the state in each CNetChan
	// and not need to duplicate it in a bitvec?

	// Clear client's download bit
//	replay->m_vecClientsDownloading.Set( nClientSlot, false );
}

//----------------------------------------------------------------------------------------

CON_COMMAND_F( request_replay_demo, "Request a replay demo from the server.", FCVAR_GAMEDLL )
{
	if ( !Replay_IsEnabled() )
	{
		Msg( "Replay is not enabled.\n" );
		return;
	}

	//------------------------- SERVER ONLY -------------------------

	//
	// TODO: There should be two paths through this function - one for when a user requests a SPECIFIC FILE,
	// and one where the user wants file based on a dump of whatever was recorded in the last N seconds.
	//

	if ( !replay || !replay->m_DemoRecorder.IsRecording() )
	{
		SVC_Print msg( "The server is not currently recording replay demos.\n" );
		CEngineSingleUserFilter filter( cmd_clientslot + 1, true );
		sv.BroadcastMessage( msg, filter );
		return;
	}

	// Make sure client slot is valid
	if ( cmd_clientslot < 0 )
	{
		AssertMsg( 0, "request_replay_demo: Bad client slot." );
		Warning( "request_replay_demo: Bad client slot, %d.", cmd_clientslot );
		return;
	}

	// Already downloading a file?
	// TODO: need to keep track of a user who is constantly requesting or is that automatic behavior?
	if ( replay->m_vecClientsDownloading.IsBitSet( cmd_clientslot ) )
	{
		SVC_Print msg( "Request denied.  You are already downloading.\n" );
		CEngineSingleUserFilter filter( cmd_clientslot + 1, true );
		sv.BroadcastMessage( msg, filter );
		return;
	}

	// Set the download bit
	// TODO
//	replay->m_vecClientsDownloading.Set( cmd_clientslot, true );

	// Dump current demo buffer to a .dem file for the client
	char szFilename[MAX_OSPATH];
	replay->m_DemoRecorder.GetUniqueDemoFilename( szFilename, sizeof(szFilename) );
	replay->m_DemoRecorder.DumpToFile( szFilename );

	// Add to history
	// TODO: Pass in proper demo length here
	// TODO: Write me.
	extern ConVar replay_demolifespan;
	CServerReplayHistoryEntryData *pNewServerEntry = new CServerReplayHistoryEntryData();
	tm now;
	Plat_GetLocalTime( &now );
	time_t now_time_t = mktime( &now );
	pNewServerEntry->m_nRecordTime = static_cast< uint64 >( now_time_t );
	pNewServerEntry->m_nLifeSpan = replay_demolifespan.GetInt() * 24 * 3600;
	pNewServerEntry->m_DemoLength.SetSeconds( 0 );
	V_strcpy( pNewServerEntry->m_szFilename, szFilename );
	V_strcpy( pNewServerEntry->m_szMapName, sv.GetMapName() );
	pNewServerEntry->m_uClientSteamId = sv.Client( cmd_clientslot )->m_SteamID.ConvertToUint64();
	pNewServerEntry->m_nBytesTransferred = 0;
	pNewServerEntry->m_bTransferComplete = false;
	pNewServerEntry->m_nSize = g_pFullFileSystem->Size( szFilename );
	pNewServerEntry->m_bTransferring = false;
	pNewServerEntry->m_nTransferId = -1;
	pNewServerEntry->m_nFileStatus = CServerReplayHistoryEntryData::FILESTATUS_EXISTS;
 	g_pServerReplayHistoryManager->RecordEntry( pNewServerEntry );

	// Extract the file stem
	char szDemoStem[260];
	Q_StripExtension( szFilename, szDemoStem, sizeof(szDemoStem) );
	char szCommand[288];
	Assert( replay->m_DemoRecorder.m_nStartTick >= 0 );
	V_sprintf_safe(
		szCommand, "replay_cache_ragdolls %s %d %d",
		szDemoStem,
		(int)replay->m_DemoRecorder.m_nStartTick,
		g_pFullFileSystem->Size( szFilename )
	);	// NOTE: m_nStartTick is updated as the "oldest" frame's tick count

	// Setup a message
	CNETMsg_StringCmd_t msg( szCommand );

	// Send the file stem back to the client for merging ragdoll/etc. with demo file
	CEngineSingleUserFilter filter( cmd_clientslot + 1, true );
	sv.BroadcastMessage( msg, filter );

	Msg( "Wrote Replay demo file to disk, %s.\n", szFilename );
}

//----------------------------------------------------------------------------------------

CON_COMMAND_F( replay_cache_ragdolls, "Cache ragdolls to disk", FCVAR_HIDDEN | FCVAR_DONTRECORD | FCVAR_SERVER_CAN_EXECUTE )
{
	if ( args.ArgC() != 4 )
	{
		AssertMsg( 0, "Bad number of arguments to replay_cache_ragdolls!\n" );
		Warning( "Bad number of arguments to replay_cache_ragdolls!\n" );
		return;
	}

	// Tell client to cache ragdolls
	char szRagdollCacheFilename[MAX_OSPATH];
	const char *pBaseFilename = args[1];
	V_snprintf( szRagdollCacheFilename, sizeof( szRagdollCacheFilename ), "%s.dmx", pBaseFilename );
	int nStartTick = V_atoi( args[2] );
	g_ClientDLL->CacheReplayRagdolls( szRagdollCacheFilename, nStartTick );

	char szDemoFilename[MAX_OSPATH];
	V_snprintf( szDemoFilename, sizeof( szDemoFilename ), "%s.dem", pBaseFilename );

	// Record in client history
	extern ConVar replay_demolifespan;
	CClientReplayHistoryEntryData *pNewEntry = new CClientReplayHistoryEntryData();
	if ( !pNewEntry )
		return;
	tm now;
	Plat_GetLocalTime( &now );
	time_t now_time_t = mktime( &now );
	pNewEntry->m_nRecordTime = static_cast< int >( now_time_t );
	pNewEntry->m_nLifeSpan = replay_demolifespan.GetInt() * 24 * 3600;
	pNewEntry->m_DemoLength.SetSeconds( 0 );
	V_strcpy( pNewEntry->m_szFilename, szDemoFilename );
	V_strcpy( pNewEntry->m_szMapName, GetBaseLocalClient().m_szLevelName );
	V_strcpy( pNewEntry->m_szServerAddress, GetBaseLocalClient().m_NetChannel->GetAddress() );
	pNewEntry->m_nBytesTransferred = 0;
	pNewEntry->m_bTransferComplete = false;
	pNewEntry->m_nSize = atoi( args[3] );
	pNewEntry->m_bTransferring = false;
	pNewEntry->m_nTransferId = -1;
	if ( !g_pClientReplayHistoryManager->RecordEntry( pNewEntry ) )
	{
		Warning( "Replay: Failed to record entry.\n" );
		return;
	}

	// Attempt to download immediately
	pNewEntry->BeginDownload();
	
	DevMsg( "Requesting file %s from %s ( %s ).\n", szDemoFilename, GetBaseLocalClient().m_NetChannel->GetName(), GetBaseLocalClient().m_NetChannel->GetAddress() );
}

//----------------------------------------------------------------------------------------

#endif
