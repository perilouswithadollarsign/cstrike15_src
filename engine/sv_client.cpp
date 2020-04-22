//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "server_pch.h"
#include "framesnapshot.h"
#include "checksum_engine.h"
#include "sv_main.h"
#include "GameEventManager.h"
#include "networkstringtable.h"
#include "demo.h"
#include "PlayerState.h"
#include "tier0/vprof.h"
#include "sv_packedentities.h"
#include "LocalNetworkBackdoor.h"
#include "testscriptmgr.h"
#include "hltvserver.h"
#if defined( REPLAY_ENABLED )
#include "replayserver.h"
#endif
#include "pr_edict.h"
#include "logofile_shared.h"
#include "dt_send_eng.h"
#include "sv_plugin.h"
#include "download.h"
#include "cmodel_engine.h"
#include "tier1/commandbuffer.h"
#include "gl_cvars.h"
#include "tier2/tier2.h"
#include "matchmaking/imatchframework.h"
#include "audio/public/vox.h"	// TERROR: for net_showreliablesounds
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "ihltv.h"
#include "tier1/utlstringtoken.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern CNetworkStringTableContainer *networkStringTableContainerServer;

static ConVar	sv_timeout( "sv_timeout", "65", 0, "After this many seconds without a message from a client, the client is dropped" );
static ConVar	sv_maxrate( "sv_maxrate", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Max bandwidth rate allowed on server, 0 == unlimited", true, 0, true, MAX_RATE );
static ConVar	sv_minrate( "sv_minrate", STRINGIFY( MIN_RATE ), FCVAR_REPLICATED | FCVAR_RELEASE, "Min bandwidth rate allowed on server, 0 == unlimited", true, 0, true, MAX_RATE );
       
       ConVar	sv_maxupdaterate( "sv_maxupdaterate", "64", FCVAR_REPLICATED | FCVAR_RELEASE, "Maximum updates per second that the server will allow" ); // we need to be able to set max rate to 128
	   ConVar	sv_minupdaterate( "sv_minupdaterate", "64", FCVAR_REPLICATED | FCVAR_RELEASE, "Minimum updates per second that the server will allow" );

	   ConVar	sv_stressbots("sv_stressbots", "0", FCVAR_RELEASE, "If set to 1, the server calculates data and fills packets to bots. Used for perf testing.");
	   ConVar	sv_replaybots( "sv_replaybots", "1", FCVAR_RELEASE, "If set to 1, the server records data needed to replay network stream from bot's perspective" );
static ConVar	sv_allowdownload ("sv_allowdownload", "1", FCVAR_RELEASE, "Allow clients to download files");
static ConVar	sv_allowupload ("sv_allowupload", "1", FCVAR_RELEASE, "Allow clients to upload customizations files");
	   ConVar	sv_sendtables ( "sv_sendtables", "0", FCVAR_DEVELOPMENTONLY, "Force full sendtable sending path." );
#if HLTV_REPLAY_ENABLED
	   ConVar	spec_replay_enable( "spec_replay_enable", "0", FCVAR_RELEASE|FCVAR_REPLICATED, "Enable Killer Replay, requires hltv server running." );
#endif
	   ConVar	spec_replay_message_time( "spec_replay_message_time", "9.5", FCVAR_RELEASE | FCVAR_REPLICATED, "How long to show the message about Killer Replay after death. The best setting is a bit shorter than spec_replay_autostart_delay + spec_replay_leadup_time + spec_replay_winddown_time" );
	   ConVar	spec_replay_rate_limit( "spec_replay_rate_limit", "3", FCVAR_RELEASE | FCVAR_REPLICATED, "Minimum allowable pause between replay requests in seconds" );

static ConVar	ss_voice_hearpartner( "ss_voice_hearpartner", "0", 0, "Route voice between splitscreen players on same system." );

ConVar sv_max_dropped_packets_to_process( "sv_max_dropped_packets_to_process", "10", FCVAR_RELEASE, "Max dropped packets to process. Lower settings prevent lagged players from simulating too far in the past. Setting of 0 disables cap." );

ConVar cl_allowdownload( "cl_allowdownload", "1", FCVAR_ARCHIVE, "Client downloads customization files" );

static ConVar	sv_quota_stringcmdspersecond( "sv_quota_stringcmdspersecond", "40", FCVAR_RELEASE, "How many string commands per second clients are allowed to submit, 0 to disallow all string commands" );

// TERROR:
static ConVar	net_showreliablesounds( "net_showreliablesounds", "0", FCVAR_CHEAT );
       ConVar 	replay_debug( "replay_debug", "0", FCVAR_RELEASE | FCVAR_REPLICATED);

	   ConVar	sv_allow_legacy_cmd_execution_from_client( "sv_allow_legacy_cmd_execution_from_client", "0", FCVAR_RELEASE, "Enables old concommand execution behavior allowing remote clients to run any command not explicitly flagged as disallowed." );

extern ConVar sv_maxreplay;
extern ConVar tv_snapshotrate;
extern ConVar tv_transmitall;
extern ConVar sv_pure_kick_clients;
extern ConVar sv_pure_trace;

#if defined( REPLAY_ENABLED )
extern ConVar replay_snapshotrate;
extern ConVar replay_transmitall;
#endif

// static ConVar sv_failuretime( "sv_failuretime", "0.5", 0, "After this long without a packet from client, don't send any more until client starts sending again" );

static const char * s_clcommands[] = 
{
	"status",
	"pause",
	"setpause",
	"unpause",
	"ping",
	"rpt_server_enable",
	"rpt_client_enable",
#ifndef DEDICATED 
	"rpt",
	"rpt_connect",
	"rpt_password",
	"rpt_screenshot",
	"rpt_download_log",
#endif
	"ss_connect",
	"ss_disconnect",
#if defined( REPLAY_ENABLED )
	"request_replay_demo",
#endif
	NULL,
};


// Used on the server and on the client to bound its cl_rate cvar.
int ClampClientRate( int nRate )
{
	// Apply mod specific clamps
	if ( sv_maxrate.GetInt() > 0 )
	{
		nRate = MIN( nRate, sv_maxrate.GetInt() );
	}

	if ( sv_minrate.GetInt() > 0 )
	{
		nRate = MAX( nRate, sv_minrate.GetInt() );
	}

	// Apply overall clamp
	nRate = clamp( nRate, MIN_RATE, MAX_RATE );

	return nRate;
}

// Validate minimum number of required clients to be connected to a server
enum ValidateMinRequiredClients_t
{
	VALIDATE_SPAWN,
	VALIDATE_DISCONNECT
};
void SV_ValidateMinRequiredClients( ValidateMinRequiredClients_t eReason )
{
	// FIXME: This gives false positives for drops and disconnects. (kwd)
	return;

	if ( !IsX360() && !sv.IsDedicatedForXbox() )
		return;

	static ConVarRef s_director_min_start_players( "director_min_start_players", true );
	if ( !s_director_min_start_players.IsValid() )
		return;

	int numRequiredByDirector = s_director_min_start_players.GetInt();
	if ( numRequiredByDirector <= 0 )
		return;

	switch ( eReason )
	{
	case VALIDATE_SPAWN:
		{
			// If at least one client has already spawned in the server, if there is a required
			// minimum number of players and some of the players are not yet connected to server
			// then most likely they dropped out or failed to connect, need to lower the minimum
			// number of required players and proceed with game.
			
			int numConnected = sv.GetClientCount();
			int numInState = 0;
			
			// Determine how many clients are above signon state NEW
			for ( int j = 0 ; j < numConnected ; j++ )
			{
				IClient *client = sv.GetClient( j );
				if ( !client )
					continue;

				if ( !client->IsSpawned() )
					continue;

				++ numInState;
			}

			if ( numRequiredByDirector > numInState )
			{
				s_director_min_start_players.SetValue( numInState );
				ConMsg( "SV_ValidateMinRequiredClients: spawn: lowered min start players to %d.\n",
					numInState );
			}
		}
		break;

	case VALIDATE_DISCONNECT:
		{
			// If somebody disconnects from the server we decrement the minimum number of players required
			-- numRequiredByDirector;
			s_director_min_start_players.SetValue( numRequiredByDirector );
			ConMsg( "SV_ValidateMinRequiredClients: disconnect: lowered min start players to %d.\n",
				numRequiredByDirector );
		}
		break;
	}


}


CGameClient::CGameClient(int slot, CBaseServer *pServer )
{
	Clear();

	m_nClientSlot = slot;
	m_nEntityIndex = slot+1;
	m_Server = pServer;
	m_pCurrentFrame = NULL;
	m_bIsInReplayMode = false;

	// NULL out data we'll never use.
	memset( &m_PrevPackInfo, 0, sizeof( m_PrevPackInfo ) );
	m_PrevPackInfo.m_pTransmitEdict = &m_PrevTransmitEdict;
	m_flTimeClientBecameFullyConnected = -1.0f;
	m_flLastClientCommandQuotaStart = -1.0f;
	m_numClientCommandsInQuota = 0;
}

CGameClient::~CGameClient()
{

}

bool CGameClient::CLCMsg_ClientInfo( const CCLCMsg_ClientInfo& msg )
{
	BaseClass::CLCMsg_ClientInfo( msg );

	if ( m_bIsHLTV )
	{
		Disconnect( "CLCMsg_ClientInfo: SourceTV can not connect to game directly.\n" );
		return false;
	}

#if defined( REPLAY_ENABLED )
	if ( m_bIsReplay )
	{
		Disconnect( "CLCMsg_ClientInfo: Replay can not connect to game directly.\n" );
		return false;
	}
#endif

	if ( sv_allowupload.GetBool() )
	{
		// download all missing customizations files from this client;
		DownloadCustomizations();
	}
	return true;
}

bool CGameClient::CLCMsg_Move( const CCLCMsg_Move& msg )
{
	// Don't process usercmds until the client is active. If we do, there can be weird behavior
	// like the game trying to send reliable messages to the client and having those messages discarded.
	if ( !IsActive() )
		return true;	

	if ( m_LastMovementTick == sv.m_nTickCount )  
	{
		// Only one movement command per frame, someone is cheating.
		return true;
	}

	m_LastMovementTick = sv.m_nTickCount; 

	INetChannel *netchan = sv.GetBaseUserForSplitClient( this )->m_NetChannel;
	int totalcmds = msg.num_backup_commands() + msg.num_new_commands();

	// Decrement drop count by held back packet count
	int netdrop = netchan->GetDropNumber();

	// Dropped packet count is reported by clients 
	if ( sv_max_dropped_packets_to_process.GetInt() )
		netdrop = Clamp( netdrop, 0, sv_max_dropped_packets_to_process.GetInt() );

	bool ignore = !sv.IsActive();
#ifdef DEDICATED
	bool paused = sv.IsPaused();
#else
	bool paused = sv.IsPaused() || ( !sv.IsMultiplayer() && Con_IsVisible() );
#endif

	// Make sure player knows of correct server time
	g_ServerGlobalVariables.curtime = sv.GetTime();
	g_ServerGlobalVariables.frametime = host_state.interval_per_tick;

	//	COM_Log( "sv.log", "  executing %i move commands from client starting with command %i(%i)\n",
	//		numcmds, 
	//		m_Client->netchan->incoming_sequence,
	//		m_Client->netchan->incoming_sequence & SV_UPDATE_MASK );

	bf_read DataIn( &msg.data()[0], msg.data().size() );

	serverGameClients->ProcessUsercmds
		( 
		edict,								// Player edict
		&DataIn,
		msg.num_new_commands(),
		totalcmds,							// Commands in packet
		netdrop,							// Number of dropped commands
		ignore,								// Don't actually run anything
		paused								// Run, but don't actually do any movement
		);


	if ( DataIn.IsOverflowed() )
	{
		Disconnect( "ProcessUsercmds:  Overflowed reading usercmd data (check sending and receiving code for mismatches)!\n" );
		return false;
	}

	return true;
}

bool CGameClient::CLCMsg_VoiceData( const CCLCMsg_VoiceData& msg )
{
	serverGameClients->ClientVoice( edict );

	SV_BroadcastVoiceData( this, msg );

	return true;
}

bool CGameClient::CLCMsg_CmdKeyValues( const CCLCMsg_CmdKeyValues& msg )
{
	KeyValues *keyvalues = CmdKeyValuesHelper::CLCMsg_GetKeyValues( msg );
	KeyValues::AutoDelete autodelete_keyvalues( keyvalues );

	serverGameClients->ClientCommandKeyValues( edict, keyvalues );
	
	if ( IsX360() || NET_IsDedicatedForXbox() )
	{
		// See if a player was removed
		if ( !Q_stricmp( keyvalues->GetName(), "OnPlayerRemovedFromSession" ) )
		{
			XUID xuid = keyvalues->GetUint64( "xuid", 0ull );
			for ( int iPlayerIndex = 0; iPlayerIndex < sv.GetClientCount(); iPlayerIndex++ )
			{
				CBaseClient *pClient = (CBaseClient *) sv.GetClient( iPlayerIndex );
				if ( !pClient || !xuid || pClient->GetClientXuid() != xuid )
					continue;
				
				pClient->Disconnect( "Player removed from host session\n" );
				return true;
			}
		}
	}

	MEM_ALLOC_CREDIT();
	KeyValues *pEvent = new KeyValues( "Server::CmdKeyValues" );
	pEvent->AddSubKey( autodelete_keyvalues.Detach() );
	pEvent->SetPtr( "edict", edict );
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( pEvent );

	return true;
}

bool CGameClient::CLCMsg_HltvReplay( const CCLCMsg_HltvReplay &msg )
{
	int nRequest = msg.request();
	if ( nRequest == REPLAY_EVENT_STUCK_NEED_FULL_UPDATE )
	{
		if ( m_nForceWaitForTick > 0 )
		{
			// <sergiy> if we are indeed waiting for tick confirmation, the client may indeed be stuck. Let them have another update. This is prone to a mildly annoying attack: client can go into a loop requesting updates, which will raise server's CPU usage and traffic and may cause server to skip ticks on high-load casual servers. So later on, I should probably rate-limit this. But hopefully I'll find why the client gets stuck before too long.
			UpdateAcknowledgedFramecount( -1 );
		}
	}
	else if ( nRequest )
	{
		ClientReplayEventParams_t params( nRequest );
		if ( params.m_flSlowdownRate > 0.01f && params.m_flSlowdownRate < 10.0f && params.m_flSlowdownLength > 0.01f &&  params.m_flSlowdownLength <= 5.0f )
		{
			// keep defaults in suspicious cases
			params.m_flSlowdownRate = msg.slowdown_rate();
			params.m_flSlowdownLength = msg.slowdown_length();
		}
		params.m_nPrimaryTargetEntIndex = msg.primary_target_ent_index();
		params.m_flEventTime = msg.event_time();
		serverGameClients->ClientReplayEvent( edict, params );
	}
	else
	{
		if ( IsHltvReplay() )
			m_HltvReplayStats.nUserCancels++;
		StopHltvReplay();
	}
	return true;
}

bool CGameClient::SVCMsg_UserMessage( const CSVCMsg_UserMessage &msg )
{
	serverGameClients->ClientSvcUserMessage( edict, msg.msg_type(), msg.passthrough(), msg.msg_data().size(), &msg.msg_data()[0] );
	return true;
}

bool CGameClient::CLCMsg_RespondCvarValue( const CCLCMsg_RespondCvarValue& msg )
{
	if ( msg.cookie() > 0 )
	{
		if ( g_pServerPluginHandler )
			g_pServerPluginHandler->OnQueryCvarValueFinished( ( EQueryCvarValueStatus )msg.cookie(), edict, ( EQueryCvarValueStatus )msg.status_code(), msg.name().c_str(), msg.value().c_str() );
	}
	else
	{
		// Negative cookie means the game DLL asked for the value.
		if ( serverGameDLL && g_bServerGameDLLGreaterThanV5 )
		{
#ifdef REL_TO_STAGING_MERGE_TODO
			serverGameDLL->OnQueryCvarValueFinished( msg.cookie(), edict, msg.status_code(), msg.name().c_str(), msg.value().c_str() );
#endif
		}
	}

	return true;
}

#include "pure_server.h"
bool CGameClient::CLCMsg_FileCRCCheck( const CCLCMsg_FileCRCCheck& msg )
{
	// Ignore this message if we're not in pure server mode...
	if ( !sv.IsInPureServerMode() )
		return true;

	char warningStr[1024] = {0};

	// first check against all the other files users have sent
	FileHash_t filehash;
	V_memcpy( filehash.m_md5contents.bits, msg.md5().c_str(), MD5_DIGEST_LENGTH );
	filehash.m_crcIOSequence = msg.crc();
	filehash.m_eFileHashType = msg.file_hash_type();
	filehash.m_cbFileLen = msg.file_len();
	filehash.m_nPackFileNumber = msg.pack_file_number();
	filehash.m_PackFileID = msg.pack_file_id();

	const char *path = CCLCMsg_FileCRCCheck_t::GetPath( msg );
	const char *fileName = CCLCMsg_FileCRCCheck_t::GetFileName( msg );
	if ( g_PureFileTracker.DoesFileMatch( path, fileName, msg.file_fraction(), &filehash, GetNetworkID() ) )
	{
		// track successful file
	}
	else
	{
		V_snprintf( warningStr, sizeof( warningStr ), "Pure server: file [%s]\\%s does not match the server's file.", path, fileName );
	}
	// still ToDo:
	// 1. make sure the user sends some files
	// 2. make sure the user doesnt skip any files
	// 3. make sure the user sends the right files...

	if ( warningStr[0] )
	{
		if ( serverGameDLL )
		{
			serverGameDLL->OnPureServerFileValidationFailure( edict, path, fileName, filehash.m_crcIOSequence, filehash.m_eFileHashType,
				filehash.m_cbFileLen, filehash.m_nPackFileNumber, filehash.m_PackFileID );
		}

		if ( sv_pure_kick_clients.GetInt() )
		{
			Disconnect( warningStr );
		}
		else
		{
			ClientPrintf( "Warning: %s\n", warningStr );
			if ( sv_pure_trace.GetInt() >= 1 )
			{
				Msg( "[%s] %s\n", GetNetworkIDString(), warningStr );
			}
		}		
	}
	else
	{
		if ( sv_pure_trace.GetInt() == 2 )
		{
			Msg( "Pure server CRC check: client %s passed check for [%s]\\%s\n", GetClientName(), path, fileName );
		}
	}

	return true;
}
void CGameClient::DownloadCustomizations()
{
	if ( !cl_allowdownload.GetBool() )
		return; // client doesn't want to download any customizations

	for ( int i=0; i<MAX_CUSTOM_FILES; i++ )
	{
		if ( m_nCustomFiles[i].crc == 0 )
			continue; // slot not used

		CCustomFilename hexname( m_nCustomFiles[i].crc );

		if ( g_pFileSystem->FileExists( hexname.m_Filename ) )
			continue; // we already have it

		// we don't have it, request download from client

		m_nCustomFiles[i].reqID = m_NetChannel->RequestFile( hexname.m_Filename, false );
	}
}

void CGameClient::Connect(const char * szName, int nUserID, INetChannel *pNetChannel, bool bFakePlayer, CrossPlayPlatform_t clientPlatform, const CMsg_CVars *pVecCvars /*= NULL*/)
{
	BaseClass::Connect( szName, nUserID, pNetChannel, bFakePlayer, clientPlatform, pVecCvars );

	edict = EDICT_NUM( m_nEntityIndex );
	
	// init PackInfo
	m_PackInfo.m_pClientEnt = edict;
	m_PackInfo.m_nPVSSize = sizeof( m_PackInfo.m_PVS );
				
	// fire global game event
	IGameEvent *event = g_GameEventManager.CreateEvent( "player_connect" );
	{
		event->SetInt( "userid", m_UserID );
		event->SetInt( "index", m_nClientSlot );
		event->SetString( "name", m_Name );
		event->SetUint64( "xuid", GetClientXuid() );
		event->SetString( "networkid", GetNetworkIDString() ); 	
		// event->SetString( "address", m_NetChannel?m_NetChannel->GetAddress():"none" );
		event->SetInt( "bot", m_bFakePlayer?1:0 );
		g_GameEventManager.FireEvent( event );
	}
}

static ConVar sv_maxclientframes( "sv_maxclientframes", "128" );
static ConVar sv_extra_client_connect_time( "sv_extra_client_connect_time", "15.0", 0, 
	"Seconds after client connect during which extra frames are buffered to prevent non-delta'd update" );

void CGameClient::SetupPackInfo( CFrameSnapshot *pSnapshot )
{
	Assert( !IsHltvReplay() );
	// Compute Vis for each client
	m_PackInfo.m_nPVSSize = (GetCollisionBSPData()->numclusters + 7) / 8;
	serverGameClients->ClientSetupVisibility( (edict_t *)m_pViewEntity,
		m_PackInfo.m_pClientEnt, m_PackInfo.m_PVS, m_PackInfo.m_nPVSSize );

	// This is the frame we are creating, i.e., the next
	// frame after the last one that the client acknowledged

	m_pCurrentFrame = AllocateFrame();
	m_pCurrentFrame->Init( pSnapshot );

	m_PackInfo.m_pTransmitEdict = &m_pCurrentFrame->transmit_entity;

	// if this client is the HLTV or Replay client, add the nocheck PVS bit array
	// normal clients don't need that extra array
#if defined( REPLAY_ENABLED )
	if ( IsHLTV() || IsReplay() )
#else
	if ( IsHLTV() )
#endif
	{
		// the hltv client doesn't has a ClientFrame list
		m_pCurrentFrame->transmit_always = new CBitVec<MAX_EDICTS>;
		m_PackInfo.m_pTransmitAlways = m_pCurrentFrame->transmit_always;
	}
	else
	{
		m_PackInfo.m_pTransmitAlways = NULL;
	}

	// Add frame to ClientFrame list 

	int nMaxFrames = MAX( sv_maxclientframes.GetInt(), MAX_CLIENT_FRAMES );

	// Only do this on dedicated servers (360 dedicated servers are !IsX360 so this check isn't strictly necessary)
	//  and non-x360 am servers due to concerns over memory growth on the consoles.
	if ( ( !IsX360() || sv.IsDedicated() ) &&
		( m_flTimeClientBecameFullyConnected != -1.0f ) &&
		( realtime - m_flTimeClientBecameFullyConnected ) < sv_extra_client_connect_time.GetFloat() )
	{
		// For 15 seconds, the max will go from 128 (default) to 450 (assuming 0.0333 world tick interval)
		// or to 960 (assuming 0.015625, 64 fps).
		// In practice during changelevel on 360 I've seen it get up to 210 or so which is only 60% greater
		//  than the 128 frame max default, so 450 seems like it should capture all cases.
		nMaxFrames = MAX( nMaxFrames, (int)( sv_extra_client_connect_time.GetFloat() / m_Server->GetTickInterval() ) );
		// Msg( "Allowing up to %d frames for player for %f more seconds\n", nMaxFrames, realtime - m_flTimeClientBecameFullyConnected );
	}

	if ( sv_maxreplay.GetFloat() > 0 )
	{
		// if the server has replay features enabled, allow a way bigger frame buffer
		nMaxFrames = MAX( nMaxFrames, sv_maxreplay.GetFloat() / m_Server->GetTickInterval() );
	}

	// During the startup period we retain additional frames. Once nMaxFrames drops we need to
	// purge the extra frames or else we may permanently be using too much memory.
	int frameCount = AddClientFrame( m_pCurrentFrame );
	while ( nMaxFrames < frameCount )
	{
		// If the client has more than nMaxFrames frames, the server will start to eat too much memory.
		RemoveOldestFrame(); 
		--frameCount;
	}
		
	m_PackInfo.m_AreasNetworked = 0;
	int areaCount = g_AreasNetworked.Count();
	for ( int j = 0; j < areaCount; j++ )
	{
		// Msg("CGameClient::SetupPackInfo: too much areas (%i)", areaCount );
		AssertOnce( m_PackInfo.m_AreasNetworked < MAX_WORLD_AREAS );

		if ( m_PackInfo.m_AreasNetworked >= MAX_WORLD_AREAS )
			break;

		m_PackInfo.m_Areas[m_PackInfo.m_AreasNetworked] = g_AreasNetworked[ j ];
		m_PackInfo.m_AreasNetworked++;
	}

	
	CM_SetupAreaFloodNums( m_PackInfo.m_AreaFloodNums, &m_PackInfo.m_nMapAreas );
}

ConVar spec_replay_rate_base( "spec_replay_rate_base", "1", FCVAR_RELEASE | FCVAR_REPLICATED, "Base time scale of Killer Replay.Experimental." );

void CGameClient::SetupHltvFrame( int nServerTick )
{
	Assert( m_nHltvReplayDelay && m_pHltvReplayServer );
	int nReplayTick = nServerTick - m_nHltvReplayDelay; 

	CClientFrame *pFrame = m_pHltvReplayServer->ExpandAndGetClientFrame( nReplayTick, false );
	if ( !pFrame )
		return;

	m_pCurrentFrame = pFrame;
}

void CGameClient::SetupPrevPackInfo()
{
	Assert( !IsHltvReplay() );
	memcpy( &m_PrevTransmitEdict, m_PackInfo.m_pTransmitEdict, sizeof( m_PrevTransmitEdict ) );
	
	// Copy the relevant fields into m_PrevPackInfo.
	m_PrevPackInfo.m_AreasNetworked = m_PackInfo.m_AreasNetworked;
	memcpy( m_PrevPackInfo.m_Areas, m_PackInfo.m_Areas, sizeof( m_PackInfo.m_Areas[0] ) * m_PackInfo.m_AreasNetworked );

	m_PrevPackInfo.m_nPVSSize = m_PackInfo.m_nPVSSize;
	memcpy( m_PrevPackInfo.m_PVS, m_PackInfo.m_PVS, m_PackInfo.m_nPVSSize );
	
	m_PrevPackInfo.m_nMapAreas = m_PackInfo.m_nMapAreas;
	memcpy( m_PrevPackInfo.m_AreaFloodNums, m_PackInfo.m_AreaFloodNums, m_PackInfo.m_nMapAreas * sizeof( m_PackInfo.m_nMapAreas ) );
}


/*
================
CheckRate

Make sure channel rate for active client is within server bounds
================
*/
void CGameClient::SetRate(int nRate, bool bForce )
{
	if ( !bForce )
	{
		nRate = ClampClientRate( nRate );
	}

	BaseClass::SetRate( nRate, bForce );
}
void CGameClient::SetUpdateRate( float fUpdateRate, bool bForce )
{
	if ( !bForce )
	{
		if ( CHLTVServer *hltv = GetAnyConnectedHltvServer() )
		{
			// Clients connected to our HLTV server will receive updates at tv_snapshotrate
			fUpdateRate = hltv->GetSnapshotRate();
		}
		else
		{
			if ( sv_maxupdaterate.GetFloat() > 0 )
			{
				fUpdateRate = clamp( fUpdateRate, 1, sv_maxupdaterate.GetFloat() );
			}

			if ( sv_minupdaterate.GetInt() > 0 )
			{
				fUpdateRate = clamp( fUpdateRate, sv_minupdaterate.GetFloat(), 128.0f );
			}
		}
	}

	BaseClass::SetUpdateRate( fUpdateRate, bForce );
}


void CGameClient::UpdateUserSettings()
{
	// set voice loopback
	m_bVoiceLoopback = m_ConVars->GetInt( "voice_loopback", 0 ) != 0;

	BaseClass::UpdateUserSettings();

	// Give entity dll a chance to look at the changes.
	// Do this after BaseClass::UpdateUserSettings() so name changes like prepending a (1)
	// take effect before the server dll sees the name.
	g_pServerPluginHandler->ClientSettingsChanged( edict );
}



//-----------------------------------------------------------------------------
// Purpose: A File has been received, if it's a logo, send it on to any other players who need it
//  and return true, otherwise, return false
// Input  : *cl - 
//			*filename - 
// Output : Returns true on success, false on failure.
/*-----------------------------------------------------------------------------
bool CGameClient::ProcessIncomingLogo( const char *filename )
{
	char crcfilename[ 512 ];
	char logohex[ 16 ];
	Q_binarytohex( (byte *)&logo, sizeof( logo ), logohex, sizeof( logohex ) );

	Q_snprintf( crcfilename, sizeof( crcfilename ), "materials/decals/downloads/%s.vtf", logohex );

	// It's not a logo file?
	if ( Q_strcasecmp( filename, crcfilename ) )
	{
		return false;
	}

	// First, make sure crc is valid
	CRC32_t check;
	CRC_File( &check, crcfilename );
	if ( check != logo )
	{
		ConMsg( "Incoming logo file didn't match player's logo CRC, ignoring\n" );
		// Still note that it was a logo!
		return true;
	}

	// Okay, looks good, see if any other players need this logo file
	SV_SendLogo( check );
	return true;
} */

bool CGameClient::IsHearingClient( int index ) const
{
#if defined( REPLAY_ENABLED )
	if ( IsHLTV() || IsReplay() )
#else
	if ( IsHLTV() )
#endif
		return true;

	if ( index == GetPlayerSlot() )
		return m_bVoiceLoopback;

	CGameClient *pClient = sv.Client( index );

	// Don't send voice from one splitscreen partner to another on the same box
	if ( !ss_voice_hearpartner.GetBool() && 
		IsSplitScreenPartner( pClient ) )
	{
		return false;
	}

	return pClient->m_VoiceStreams.Get( GetPlayerSlot() ) != 0;
}

bool CGameClient::IsProximityHearingClient( int index ) const
{
	CGameClient *pClient = sv.Client( index );
	return pClient->m_VoiceProximity.Get( GetPlayerSlot() ) != 0;
}

void CGameClient::Inactivate( void )
{
	if ( edict && !edict->IsFree() )
	{
		m_Server->RemoveClientFromGame( this );
	}

	if ( IsHLTV() )
	{	
		if ( CHLTVServer *hltv = GetAnyConnectedHltvServer() )
		{
			hltv->Changelevel( true );
		}
	}

	m_nHltvReplayDelay = 0;
	m_pHltvReplayServer = NULL;
	m_nHltvReplayStopAt = 0;
	m_nHltvReplayStartAt = 0;
	m_nHltvLastSendTick = 0;	// last send tick, don't send ticks twice

#if defined( REPLAY_ENABLED )
	if ( IsReplay() )
	{
		replay->Changelevel();
	}
#endif

	BaseClass::Inactivate();

	m_Sounds.Purge();
	ConVarRef voice_verbose( "voice_verbose" );
	if ( voice_verbose.GetBool() )
	{
		Msg( "* CGameClient::Inactivate:  Clearing m_VoiceStreams/m_VoiceProximity for %s (%s)\n", GetClientName(), GetNetChannel() ? GetNetChannel()->GetAddress() : "null" );
	}
	m_VoiceStreams.ClearAll();
	m_VoiceProximity.ClearAll();


	DeleteClientFrames( -1 ); // delete all
}

bool CGameClient::UpdateAcknowledgedFramecount(int tick)
{
	// free old client frames which won't be used anymore
	if ( tick != m_nDeltaTick )
	{
		// delta tick changed, free all frames smaller than tick
		int removeTick = tick;
		
		if ( sv_maxreplay.GetFloat() > 0 )
			removeTick -= (sv_maxreplay.GetFloat() / m_Server->GetTickInterval() ); // keep a replay buffer

		if ( removeTick > 0 )
		{
			DeleteClientFrames( removeTick );	
		}
	}

	return BaseClass::UpdateAcknowledgedFramecount( tick );
}



void CGameClient::Clear()
{
	if ( m_bIsHLTV )
	{
		if ( CHLTVServer *hltv = GetAnyConnectedHltvServer() )
		{
			hltv->Shutdown();
		}
	}
	
#if defined( REPLAY_ENABLED )
	if ( m_bIsReplay )
	{
		replay->Shutdown();
	}
#endif

	BaseClass::Clear();

	m_HltvQueuedMessages.PurgeAndDeleteElements();

	// free all frames
	DeleteClientFrames( -1 );

	m_Sounds.Purge();
	ConVarRef voice_verbose( "voice_verbose" );
	if ( voice_verbose.GetBool() )
	{
		Msg( "* CGameClient::Clear:  Clearing m_VoiceStreams/m_VoiceProximity for %s (%s)\n", GetClientName(), GetNetChannel() ? GetNetChannel()->GetAddress() : "null" );
	}
	m_VoiceStreams.ClearAll();
	m_VoiceProximity.ClearAll();
	edict = NULL;
	m_pViewEntity = NULL;
	m_bVoiceLoopback = false;
	m_LastMovementTick = 0;
	m_nSoundSequence = 0;
	m_flTimeClientBecameFullyConnected = -1.0f;
	m_flLastClientCommandQuotaStart = -1.0f;
	m_numClientCommandsInQuota = 0;
	m_nHltvReplayDelay = 0;
	m_pHltvReplayServer = NULL;
	m_nHltvReplayStopAt = 0;
	m_nHltvReplayStartAt = 0;
	m_nHltvLastSendTick = 0;
	m_flHltvLastReplayRequestTime = -spec_replay_message_time.GetFloat();
}

void CGameClient::Reconnect( void )
{
	// If the client was connected before, tell the game .dll to disconnect him/her.
	sv.RemoveClientFromGame( this );

	BaseClass::Reconnect();
}

void CGameClient::PerformDisconnection( const char *pReason )
{
	// notify other clients of player leaving the game
	// send the username and network id so we don't depend on the CBasePlayer pointer
	IGameEvent *event = g_GameEventManager.CreateEvent( "player_disconnect" );

	if ( event )
	{
		event->SetInt("userid", GetUserID() );
		event->SetString("reason", pReason );
		event->SetString("name", GetClientName() );
		event->SetUint64("xuid", GetClientXuid() );
		event->SetString("networkid", GetNetworkIDString() ); 
		g_GameEventManager.FireEvent( event );
	}

	m_Server->RemoveClientFromGame( this );

	int nDisconnectSignonState = GetSignonState();
	BaseClass::PerformDisconnection( pReason );
	if ( nDisconnectSignonState >= SIGNONSTATE_NEW )
	{
		SV_ValidateMinRequiredClients( VALIDATE_DISCONNECT );
	}

	m_nHltvReplayDelay = 0;
	m_pHltvReplayServer = NULL;
	m_nHltvReplayStopAt = 0;
	m_nHltvReplayStartAt = 0;
	m_nHltvLastSendTick = 0;	
}

HltvReplayStats_t m_DisconnectedClientsHltvReplayStats;

void CGameClient::Disconnect( const char *fmt )
{
	// Remember what state we had when "Disconnect" got called
	int nDisconnectSignonState = GetSignonState();

	if ( nDisconnectSignonState == SIGNONSTATE_NONE )
		return;	// no recursion

	m_DisconnectedClientsHltvReplayStats += m_HltvReplayStats;
	m_HltvReplayStats.Reset();

	BaseClass::Disconnect( fmt );
}

bool CGameClient::ProcessSignonStateMsg( int state, int spawncount )
{
	if ( state == SIGNONSTATE_SPAWN || state == SIGNONSTATE_CHANGELEVEL )
	{
		StopHltvReplay();
	}
	else if ( state == SIGNONSTATE_CONNECTED )
	{
		if ( !CheckConnect() )
			return false;

		// Allow long enough time-out to load a map
		float flTimeout = SIGNON_TIME_OUT;
		if ( sv.IsDedicatedForXbox() )
			flTimeout = SIGNON_TIME_OUT_360;

		m_NetChannel->SetTimeout( flTimeout );
		m_NetChannel->SetFileTransmissionMode( false );
		m_NetChannel->SetMaxBufferSize( true, NET_MAX_PAYLOAD );
	}
	else if ( state == SIGNONSTATE_NEW )
	{
		if ( !sv.IsMultiplayer() )
		{
			// local client as received and create string tables,
			// now link server tables to client tables
			SV_InstallClientStringTableMirrors();
		}
	}
	else if ( state == SIGNONSTATE_FULL )
	{
		if ( sv.m_bLoadgame )
		{
			// If this game was loaded from savegame, finish restoring game now
			sv.FinishRestore();
		}

		m_NetChannel->SetTimeout( sv_timeout.GetFloat() ); // use smaller timeout limit
		m_NetChannel->SetFileTransmissionMode( true );

		g_pServerPluginHandler->ClientFullyConnect( edict );
	}

	return BaseClass::ProcessSignonStateMsg( state, spawncount );
}

void CGameClient::SendSound( SoundInfo_t &sound, bool isReliable )
{
#if defined( REPLAY_ENABLED )
	if ( IsFakeClient() && !IsHLTV() && !IsReplay() && !IsSplitScreenUser() )
#else
	if ( IsFakeClient() && !IsHLTV() && !IsSplitScreenUser() )
#endif
	{
		return; // dont send sound messages to bots
	}

	// don't send sound messages while client is replay mode
	if ( m_bIsInReplayMode )
	{
		return;
	}

	// reliable sounds are send as single messages
	if ( isReliable )
	{
		CSVCMsg_Sounds_t *sndmsg = new CSVCMsg_Sounds_t;

		m_nSoundSequence = ( m_nSoundSequence + 1 ) & SOUND_SEQNUMBER_MASK;	// increase own sound sequence counter
		sound.nSequenceNumber = 0; // don't transmit nSequenceNumber for reliable sounds

		sndmsg->set_reliable_sound( true );

		sound.WriteDelta( NULL, *sndmsg, sv.GetFinalTickTime() );

		if ( net_showreliablesounds.GetBool() )
		{
			const char *name = "<Unknown>";
			if ( sound.bIsSentence )
			{
				name = VOX_SentenceNameFromIndex( sound.nSoundNum );
			}
			else
			{
				if( sound.nFlags & SND_IS_SCRIPTHANDLE )
				{
					name = sound.pszName;
				}
				else
				{
					name = sv.GetSound( sound.nSoundNum );
				}
			}
			Warning( "reliable%s %s %d/%d/%d/%s\n",
				((sound.nFlags & SND_STOP) != 0)?" stop":"",
				(sound.bIsSentence)?"sentence":"sound",
				sound.nEntityIndex, sound.nChannel, sound.nSoundNum, name );
		}

		// send reliable sound as single message
		SendNetMsg( *sndmsg, true );

		if ( m_nHltvReplayDelay )
			m_HltvQueuedMessages.AddToTail( sndmsg );
		else
			delete sndmsg;

		return;
	}

	sound.nSequenceNumber = m_nSoundSequence;

	m_Sounds.AddToTail( sound );	// queue sounds until snapshot is send
}

void CGameClient::WriteGameSounds( bf_write &buf, int nMaxSounds )
{
	if ( m_Sounds.Count() <= 0 )
		return;

	CSVCMsg_Sounds_t msg;

	msg.SetReliable( false );
	int nSoundCount = FillSoundsMessage( msg, nMaxSounds );
	msg.WriteToBuffer( buf );

	if ( IsTracing() )
	{
		TraceNetworkData( buf, "Sounds [count=%d]", nSoundCount );
	}
}

static ConVar sv_sound_discardextraunreliable( "sv_sound_discardextraunreliable", "1" );

int	CGameClient::FillSoundsMessage(CSVCMsg_Sounds &msg, int nMaxSounds )
{
	int i, count = m_Sounds.Count();

	// Discard events if we have too many to signal with 8 bits
	if ( count > nMaxSounds )
		count = nMaxSounds;

	// Nothing to send
	if ( !count )
		return 0;

	SoundInfo_t defaultSound;
	SoundInfo_t *pDeltaSound = &defaultSound;

	msg.set_reliable_sound( false );

	float finalTickTime = m_Server->GetFinalTickTime();
	for ( i = 0 ; i < count; i++ )
	{
		SoundInfo_t &sound = m_Sounds[ i ];
		sound.WriteDelta( pDeltaSound, msg, finalTickTime );
		pDeltaSound = &m_Sounds[ i ];
	}

	// remove added events from list
	if ( sv_sound_discardextraunreliable.GetBool() )
	{
		if ( m_Sounds.Count() != count )
		{
			DevMsg( 2, "Warning! Dropped %i unreliable sounds for client %s.\n" , m_Sounds.Count() - count, m_Name );
		}
		m_Sounds.RemoveAll();
	}
	else
	{
		int remove = m_Sounds.Count() - ( count + nMaxSounds );
		if ( remove > 0 )
		{
			DevMsg( 2, "Warning! Dropped %i unreliable sounds for client %s.\n" , remove, m_Name );
			count+= remove;
		}

		if ( count > 0 )
		{
			m_Sounds.RemoveMultiple( 0, count );
		}
	}

	Assert( m_Sounds.Count() <= nMaxSounds );

	return msg.sounds_size();
}

bool CGameClient::CheckConnect( void )
{
	// Allow the game dll to reject this client.
	char szRejectReason[128];
	Q_strncpy( szRejectReason, "Connection rejected by game", sizeof( szRejectReason ) );

	if ( !g_pServerPluginHandler->ClientConnect( edict, m_Name, m_NetChannel->GetAddress(), szRejectReason, sizeof( szRejectReason ) ) )
	{
		// Reject the connection and drop the client.
		Disconnect( szRejectReason );
		return false;
	}

	return BaseClass::CheckConnect();
}

void CGameClient::ActivatePlayer( void )
{
	BaseClass::ActivatePlayer();

	COM_TimestampedLog( "CGameClient::ActivatePlayer -start" );

	// call the spawn function
	if ( !sv.m_bLoadgame )
	{
		g_ServerGlobalVariables.curtime = sv.GetTime();

		COM_TimestampedLog( "g_pServerPluginHandler->ClientPutInServer" );

		g_pServerPluginHandler->ClientPutInServer( edict, m_Name );
	}

    COM_TimestampedLog( "g_pServerPluginHandler->ClientActive" );

	g_pServerPluginHandler->ClientActive( edict, sv.m_bLoadgame );

	COM_TimestampedLog( "g_pServerPluginHandler->ClientSettingsChanged" );

	g_pServerPluginHandler->ClientSettingsChanged( edict );

	COM_TimestampedLog( "GetTestScriptMgr()->CheckPoint" );

	GetTestScriptMgr()->CheckPoint( "client_connected" );

	// don't send signonstate to client, client will switch to FULL as soon 
	// as the first full entity update packets has been received

	// fire a activate event
	IGameEvent *event = g_GameEventManager.CreateEvent( "player_activate" );

	if ( event )
	{
		event->SetInt( "userid", GetUserID() );
		g_GameEventManager.FireEvent( event );
	}

	COM_TimestampedLog( "CGameClient::ActivatePlayer -end" );

	// We'll let them have additional snapshots so the remote can connect w/o requiring a second 
	// non-delta update (since the client gets the uncompressed world, then does a bunch of loading
	// which can take 7-10 seconds, then ack's a NET_Tick/delta tick which is way out of date by then
	m_flTimeClientBecameFullyConnected = realtime;
}

bool CGameClient::SendSignonData( void )
{
	bool bClientHasdifferentTables = false;

	if ( sv.m_FullSendTables.IsOverflowed() )
	{
		Host_Error( "Send Table signon buffer overflowed %i bytes!!!\n", sv.m_FullSendTables.GetNumBytesWritten() );
		return false;
	}

	if ( SendTable_GetCRC() != (CRC32_t)0 )
	{
		bClientHasdifferentTables =  m_nSendtableCRC != SendTable_GetCRC();
	}

#ifdef _DEBUG
	if ( sv_sendtables.GetInt() == 2 )
	{
		// force sending class tables, for debugging
		bClientHasdifferentTables = true; 
	}
#endif

	// Write the send tables & class infos if needed
	if ( bClientHasdifferentTables )
	{
		if ( sv_sendtables.GetBool() )
		{
			// send client class table descriptions so it can rebuild tables
			ConDMsg("Client sent different SendTable CRC, sending full tables.\n" );
			m_NetChannel->SendData( sv.m_FullSendTables );
		}
		else
		{
			Disconnect( "Server uses different class tables" );
			return false;
		}
	}
	else
	{
		// use your class infos, CRC is correct
		CSVCMsg_ClassInfo_t classmsg;
		classmsg.set_create_on_client( true );
		m_NetChannel->SendNetMsg( classmsg );
	}

	if ( !BaseClass::SendSignonData()	)
		return false;

	m_nSoundSequence = 1; // reset sound sequence numbers after signon block

	return true;
}


void CGameClient::SpawnPlayer( void )
{
	SV_ValidateMinRequiredClients( VALIDATE_SPAWN );

	// run the entrance script
	if ( sv.m_bLoadgame )
	{	// loaded games are fully inited already
		// if this is the last client to be connected, unpause
		sv.SetPaused( false );
	}
	else
	{
		// set up the edict
		Assert( serverGameEnts );
		serverGameEnts->FreeContainingEntity( edict );
		InitializeEntityDLLFields( edict );
	}

	// restore default client entity and turn off replay mdoe
	m_nEntityIndex = m_nClientSlot+1;
	m_bIsInReplayMode = false;

	// set view entity
	CSVCMsg_SetView_t setView;
	setView.set_entity_index( m_nEntityIndex );
	SendNetMsg( setView );

	BaseClass::SpawnPlayer();
}

CClientFrame *CGameClient::GetDeltaFrame( int nTick )
{
	Assert ( !IsHLTV() ); // has no ClientFrames
#if defined( REPLAY_ENABLED )
	Assert ( !IsReplay() );  // has no ClientFrames
#endif	

	if ( m_bIsInReplayMode )
	{
		int followEntity; 

		serverGameClients->GetReplayDelay( edict, followEntity );

		Assert( followEntity > 0 );

		CGameClient *pFollowEntity = sv.Client( followEntity-1 );

		if ( pFollowEntity )
			return pFollowEntity->GetClientFrame( nTick );
	}

	return GetClientFrame( nTick );
}

void CGameClient::WriteViewAngleUpdate()
{
//
// send the current viewpos offset from the view entity
//
// a fixangle might get lost in a dropped packet.  Oh well.

	if ( IsFakeClient() && !IsSplitScreenUser() )
		return;

	Assert( serverGameClients );
	CPlayerState *pl = serverGameClients->GetPlayerState( edict );
	Assert( pl || IsSplitScreenUser() );
	if ( !pl )
		return;

	if ( pl->fixangle != FIXANGLE_NONE )
	{
		if ( pl->fixangle == FIXANGLE_RELATIVE )
		{
			CSVCMsg_FixAngle_t fixAngle;
			fixAngle.set_relative( true );
			fixAngle.mutable_angle()->set_x( pl->anglechange.x );
			fixAngle.mutable_angle()->set_y( pl->anglechange.y );
			fixAngle.mutable_angle()->set_z( pl->anglechange.z );
			m_NetChannel->SendNetMsg( fixAngle );
			pl->anglechange.Init(); // clear
		}
		else
		{
			CSVCMsg_FixAngle_t fixAngle;
			fixAngle.set_relative( false );
			fixAngle.mutable_angle()->set_x( pl->v_angle.x );
			fixAngle.mutable_angle()->set_y( pl->v_angle.y );
			fixAngle.mutable_angle()->set_z( pl->v_angle.z );
			m_NetChannel->SendNetMsg( fixAngle );
		}
		
		pl->fixangle = FIXANGLE_NONE;
	}
}

/*
===================
SV_ValidateClientCommand

Determine if passed in user command is valid.
===================
*/
bool CGameClient::IsEngineClientCommand( const CCommand &args ) const
{
	if ( args.ArgC() == 0 )
		return false;

	for ( int i = 0; s_clcommands[i] != NULL; ++i )
	{
		if ( !Q_strcasecmp( args[0], s_clcommands[i] ) )
			return true;
	}

	return false;
}

bool CGameClient::SendNetMsg( INetMessage &msg, bool bForceReliable, bool bVoice )
{
	if ( m_bIsHLTV )
	{
		if ( CHLTVServer* hltv = GetAnyConnectedHltvServer() )
		{// pass this message to HLTV
			return hltv->SendNetMsg( msg, bForceReliable, bVoice );
		}
		else
		{
			Warning("HLTV client has no HLTV server connected\n");
			return false;
		}
	}
#if defined( REPLAY_ENABLED )
	if ( m_bIsReplay )
	{
		// pass this message to replay
		return replay->SendNetMsg( msg, bForceReliable, bVoice );
	}
#endif
	if ( IsHltvReplay() )
	{
		if ( msg.GetType() != svc_VoiceData ) // let the voice messages through
		{
			Assert( !bVoice );
			bool bResult = true;

			if ( msg.GetType() == svc_UserMessage )
			{
				// chat: see UTIL_SayText2Filter(), Say_Host()  and "player_say" GameMessage
				CSVCMsg_UserMessage_t &userMessageHeader = ( CSVCMsg_UserMessage_t & )msg;
				// Only send through those user messages that require real-time timeline on the client.
				switch ( userMessageHeader.msg_type() )
				{
				case 22: // CS_UM_RadioText
				case 5: //CS_UM_SayText
				case 6: // CS_UM_SayText2
				case 7: // CS_UM_TextMsg
				case 18: // CS_UM_RawAudio
					// mark this message as real-time and send with that flag, to distinguish it from the replay messages
					userMessageHeader.set_passthrough( 1 );
					bResult = BaseClass::SendNetMsg( msg, bForceReliable, bVoice );
					userMessageHeader.clear_passthrough();
					break;
				}
			}

			return bResult; // just ignore all (other) new messages: we'll take them from hltv later if needed
		}
		Assert( bVoice );
	}

	return BaseClass::SendNetMsg( msg, bForceReliable, bVoice );
}

static CUtlStringToken s_HltvUnskippableEvents[] = {
	"round_start",
	"begin_new_match",
	"game_newmap"
};

static CUtlStringToken s_HltvQueueableEvents[] = {
	"teamplay_round_start","teamplay_round_end",
	MakeStringToken( "endmatch_cmm_start_reveal_items" ),
	"announce_phase_end",
	"cs_match_end_restart",
	"round_freeze_end"
};

static CUtlStringToken s_HltvPassThroughRealtimeEvents[] = {
	"teamplay_broadcast_audio",
	"player_chat",
	"player_say",
	"player_death",
	"round_mvp",
	"round_end"
};


bool IsInList( const char *pEventName, const char **ppList, int nListCount )
{
	for ( int i = 0; i < nListCount; ++i )
	{
		if ( !V_strcmp( pEventName, ppList[ i ] ) )
		{
			return true;
		}
	}
	return false;
}

bool IsInList( CUtlStringToken eventName, const CUtlStringToken *pTokenList, int nListCount )
{
	for ( int i = 0; i < nListCount; ++i )
	{
		if ( eventName == pTokenList[ i ] )
		{
			return true;
		}
	}
	return false;
}

void CGameClient::FireGameEvent( IGameEvent *event )
{
	if ( IsHltvReplay() )
	{
		const char *pEventName = event->GetName();  // please don't fold the string variable, it's useful for debugging
		CUtlStringToken eventName = MakeStringToken( pEventName );

		if ( IsInList( eventName, s_HltvQueueableEvents, ARRAYSIZE( s_HltvQueueableEvents ) ) )
		{
			if ( event->IsReliable() ) // skip this event if it's not reliable
			{
				CSVCMsg_GameEvent_t *pEventMsg = new CSVCMsg_GameEvent_t;
				if ( g_GameEventManager.SerializeEvent( event, pEventMsg ) )
				{
					m_HltvQueuedMessages.AddToTail( pEventMsg );
				}
				else
				{
					delete pEventMsg;
				}
			}
			return;
		}
		else if ( IsInList( eventName, s_HltvUnskippableEvents, ARRAYSIZE( s_HltvUnskippableEvents ) ) )
		{
			Msg( "%s (%d) is skipping replay in progress (%d/%d) due to event %s\n", m_Name, m_UserID, sv.GetTick() - m_nHltvReplayDelay - m_nHltvReplayStartAt, m_nHltvReplayStopAt - m_nHltvReplayStartAt, pEventName );
			StopHltvReplay();
			return BaseClass::FireGameEvent( event );
		}
		else if ( IsInList( eventName, s_HltvPassThroughRealtimeEvents, ARRAYSIZE( s_HltvPassThroughRealtimeEvents ) ) )
		{
			return BaseClass::FireGameEvent( event, true ); // mark the event as real-time pass-through so that on the other end the client knows it's happening in real time, and it's not a replayed-back event
		}
		else
		{
			// For now, ignore game events by default, if they are sent while we're in a replay.
			return;
		}
	}

	return BaseClass::FireGameEvent( event );
}

bool CGameClient::ExecuteStringCommand( const char *pCommandString )
{
	// first let the baseclass handle it
	if ( BaseClass::ExecuteStringCommand( pCommandString ) )
		return true;
	
	// Determine whether the command is appropriate
	CCommand args;
	if ( !args.Tokenize( pCommandString, kCommandSrcNetClient ) )
		return false;

	if ( args.ArgC() == 0 )
		return false;

	// Disallow all string commands from client
	// Special case for 0 here, as we don't kick in this case.
	int cmdQuota = sv_quota_stringcmdspersecond.GetInt();
	if ( cmdQuota == 0 )
		return false;

	// Client is about to execute a string command, check if we need to reset quota
	if ( realtime - m_flLastClientCommandQuotaStart >= 1.0 )
	{
		// reset quota
		m_flLastClientCommandQuotaStart = realtime;
		m_numClientCommandsInQuota = 0;
	}
	++ m_numClientCommandsInQuota;
	if ( m_numClientCommandsInQuota > cmdQuota )
	{
		// Disconnect player for Denial-of-service attack

		// REI: Remove this define when we unify trunk/staging (trunk uses enum reasons for disconnection)
		// REI: See network_connection.proto for where this is supposed to come from
		#define NETWORK_DISCONNECT_SERVER_DOS "#GameUI_Disconnect_TooManyCommands"
		Disconnect( NETWORK_DISCONNECT_SERVER_DOS );
		return false;
	}

	if ( IsEngineClientCommand( args ) )
	{
		Cmd_ExecuteCommand( CBUF_SERVER, args, m_nClientSlot );
		return true;
	}
	
	// FIXME: This logic seem strange; why can't we just go through Cmd_ExecuteCommand?  We should check for
	//        permission (cheat, sponly, gamedll since we are coming from client code) there, right?
	const ConCommandBase *pCommand = g_pCVar->FindCommandBase( args[ 0 ] );
	if ( pCommand && pCommand->IsCommand() && pCommand->IsFlagSet( FCVAR_GAMEDLL ) )
	{
		// Allow cheat commands in singleplayer, debug, or multiplayer with sv_cheats on
		// NOTE: Don't bother with rpt stuff; commands that matter there shouldn't have FCVAR_GAMEDLL set
		if ( pCommand->IsFlagSet( FCVAR_CHEAT ) )
		{
			if ( sv.IsMultiplayer() && !CanCheat() )
				return false;
		}
		else if ( !sv_allow_legacy_cmd_execution_from_client.GetBool() && !pCommand->IsFlagSet( FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS ) 
#if !defined DEDICATED
			 && ( sv.IsDedicated() || m_nClientSlot != GetBaseLocalClient().m_nPlayerSlot )
#endif
			 )
		{
#if DEVELOPMENT_ONLY 
			Warning( "WARNING: Client sent concommand %s which is not flagged as executable, ignoring\n" );
#endif
			return false;
		}

		if ( pCommand->IsFlagSet( FCVAR_SPONLY ) )
		{
			if ( sv.IsMultiplayer() )
			{
				return false;
			}
		}


		// REI 7/25/2016:
		//    I added this here; this state is normally set by Cmd_ExecuteCommand when executing
		//    a kCmdSrcNetClient command.
		//
		//    Is there a reason this code path goes directly to Cmd_Dispatch instead
		//    of the usual path through Cmd_ExecuteCommand?
		cmd_clientslot = m_nClientSlot;

		g_pServerPluginHandler->SetCommandClient( m_nClientSlot );
		Cmd_Dispatch( pCommand, args );
	}
	else
	{
		g_pServerPluginHandler->ClientCommand( edict, args ); // TODO pass client id and string
	}

	return true;
}

extern ConVar sv_multiplayer_maxsounds;

bool CGameClient::SendSnapshot( CClientFrame * pFrame )
{
	if ( IsHltvReplay() )
	{
		return SendHltvReplaySnapshot( pFrame );
	}

	if ( m_bIsHLTV )
	{
		SNPROF( "SendSnapshot - HLTV" );

		CHLTVServer *hltv = GetAnyConnectedHltvServer();
		// pack sounds to one message
		if ( m_Sounds.Count() > 0 )
		{
			CSVCMsg_Sounds_t sounds;
			sounds.SetReliable( false );
			FillSoundsMessage( sounds, m_Server->IsMultiplayer() ? sv_multiplayer_maxsounds.GetInt() : 255 );
			hltv->SendNetMsg( sounds );
		}

		int maxEnts = tv_transmitall.GetBool()?255:64;
		CSVCMsg_TempEntities_t tempentsmsg;
		hltv->WriteTempEntities( this, pFrame->GetSnapshot(), m_pLastSnapshot.GetObject(), tempentsmsg, maxEnts );
		if ( tempentsmsg.num_entries() )
		{
			tempentsmsg.WriteToBuffer( *hltv->GetBuffer( HLTV_BUFFER_TEMPENTS ) );
		}

		// add snapshot to HLTV server frame list
		hltv->AddNewDeltaFrame( pFrame );

		// remember this snapshot
		m_pLastSnapshot = pFrame->GetSnapshot(); 

		Assert( !GetHltvReplayDelay() ); // hltv master client shouldn't be in killer replay mode
		// fake acknowledgement, remove ClientFrame reference immediately 
		UpdateAcknowledgedFramecount( pFrame->tick_count );
		
		return true;
	}

#if defined( REPLAY_ENABLED )
	if ( m_bIsReplay )
	{
		SNPROF( "SendSnapshot - Replay" );

		char *buf = (char *)_alloca( NET_MAX_PAYLOAD );

		// pack sounds to one message
		if ( m_Sounds.Count() > 0 )
		{
			CSVCMsg_Sounds_t sounds;

			sounds.SetReliable( false );
			FillSoundsMessage( sounds, m_Server->IsMultiplayer() ? sv_multiplayer_maxsounds.GetInt() : 255 );
			replay->SendNetMsg( sounds );
		}

		int maxEnts = replay_transmitall.GetBool()?255:64;
		replay->WriteTempEntities( this, pFrame->GetSnapshot(), m_pLastSnapshot.GetObject(), *replay->GetBuffer( REPLAY_BUFFER_TEMPENTS ), maxEnts );

		// add snapshot to Replay server frame list
		replay->AddNewFrame( pFrame );

		// remember this snapshot
		m_pLastSnapshot = pFrame->GetSnapshot(); 

		// fake acknowledgement, remove ClientFrame reference immediately 
		UpdateAcknowledgedFramecount( pFrame->tick_count );

		return true;
	}
#endif

	// update client viewangles update
	WriteViewAngleUpdate();

	bool bRet;

	bRet = BaseClass::SendSnapshot( pFrame );

	if ( bRet )
	{
		// Send messages that were queued up during replay - they need fully created entities, which is why I'm sending them after SendSnapshot
		for ( INetMessage * pMessage : m_HltvQueuedMessages )
		{
			if ( m_NetChannel )
			{
				if ( !m_NetChannel->SendNetMsg( *pMessage, true ) ) // we only queue reliable messages
					break;
			}
		}
		m_HltvQueuedMessages.PurgeAndDeleteElements();
		//////////////////////////////////////////////////////////////////////////

		if ( IsFakeClient() )
		{
			Assert( !GetHltvReplayDelay() ); // fake clients should not be in "killer replay" mode
			// fake acknowledgement, remove ClientFrame reference immediately 
			UpdateAcknowledgedFramecount( pFrame->tick_count );
		}
	}

	return bRet;
}

//ConVar replay_hltv_voice( "replay_hltv_voice", "0", FCVAR_RELEASE );

bool CGameClient::SendHltvReplaySnapshot( CClientFrame * pFrame )
{
	Assert( IsHltvReplay() );
	VPROF_BUDGET( "CGameClient::SendHltvReplaySnapshot", "HLTV" );

	byte		buf[ NET_MAX_PAYLOAD ];
	bf_write	msg( "CGameClient::SendHltvReplaySnapshot", buf, sizeof( buf ) );

	// if we send a full snapshot (no delta-compression) before, wait until client
	// received and acknowledge that update. don't spam client with full updates

	if ( m_pLastSnapshot == pFrame->GetSnapshot() )
	{
		// never send the same snapshot twice
		m_NetChannel->Transmit();
		return false;
	}

	if ( m_nForceWaitForTick > 0 )
	{
		// just continue transmitting reliable data
		Assert( !m_bFakePlayer );	// Should never happen
		m_NetChannel->Transmit();
		return false;
	}

	CClientFrame	*pDeltaFrame = m_pHltvReplayServer->GetDeltaFrame( m_nDeltaTick ); // NULL if delta_tick is not found
	CHLTVFrame		*pLastFrame = ( CHLTVFrame* )m_pHltvReplayServer->GetDeltaFrame( m_nHltvLastSendTick );

	if ( pLastFrame )
	{
		// start first frame after last send
		pLastFrame = ( CHLTVFrame* )pLastFrame->m_pNext;
	}

	// add all reliable messages between ]lastframe,currentframe]
	// add all tempent & sound messages between ]lastframe,currentframe]
	while ( pLastFrame && pLastFrame->tick_count <= pFrame->tick_count )
	{
		m_NetChannel->SendData( pLastFrame->m_Messages[ HLTV_BUFFER_RELIABLE ], true );

		if ( pDeltaFrame )
		{
			// if we send entities delta compressed, also send unreliable data
			m_NetChannel->SendData( pLastFrame->m_Messages[ HLTV_BUFFER_UNRELIABLE ], false );
			// we skip the voice messages here because we don't want to hear the 10-second-delayed voice.
			// we might want to send the real-time voice though
			// if ( replay_hltv_voice.GetBool() )
			// {
			// 	int nVoiceBits = pLastFrame->m_Messages[ HLTV_BUFFER_VOICE ].m_nDataBits;
			// 	if ( nVoiceBits > 0 )
			// 	{
			// 		//Msg( "replaying voice: %d bits\n", nVoiceBits );
			// 		m_NetChannel->SendData( pLastFrame->m_Messages[ HLTV_BUFFER_VOICE ], false ); // we separate voice, even though it's simply more unreliable data, because we don't send it in replay
			// 	}
			// }
		}

		pLastFrame = ( CHLTVFrame* )pLastFrame->m_pNext;
	}

	// now create client snapshot packet

	// send tick time
	CNETMsg_Tick_t tickmsg( pFrame->tick_count, host_frameendtime_computationduration, host_frametime_stddeviation, host_framestarttime_stddeviation );
	tickmsg.set_hltv_replay_flags( 1 );
	tickmsg.WriteToBuffer( msg );

	// Update shared client/server string tables. Must be done before sending entities
	m_Server->m_StringTables->WriteUpdateMessage( NULL, GetMaxAckTickCount(), msg );

	// TODO delta cache whole snapshots, not just packet entities. then use net_Align
	// send entity update, delta compressed if deltaFrame != NULL
	{
		CSVCMsg_PacketEntities_t packetmsg;
		m_pHltvReplayServer->WriteDeltaEntities( this, pFrame, pDeltaFrame, packetmsg );

		packetmsg.WriteToBuffer( msg );
	}

	// write message to packet and check for overflow
	if ( msg.IsOverflowed() )
	{
		if ( !pDeltaFrame )
		{
			// if this is a reliable snapshot, drop the client
			//Disconnect( NETWORK_DISCONNECT_SNAPSHOTOVERFLOW );

			
			return false;
		}
		else
		{
			// unreliable snapshots may be dropped
			ConMsg( "WARNING: msg overflowed for %s\n", m_Name );
			msg.Reset();
		}
	}

	// remember this snapshot
	m_pLastSnapshot = pFrame->GetSnapshot();
	m_nHltvLastSendTick = pFrame->tick_count;

	// Don't send the datagram to fakeplayers
	if ( m_bFakePlayer )
	{
		m_nDeltaTick = pFrame->tick_count;
		return true;
	}

	bool bSendOK;

	// is this is a full entity update (no delta) ?
	if ( !pDeltaFrame )
	{
		if ( replay_debug.GetInt() > 10 )
			Msg( "HLTV send full frame %d: %d bytes\n", pFrame->tick_count, ( msg.m_iCurBit + 7 ) / 8 );
		// transmit snapshot as reliable data chunk
		bSendOK = m_NetChannel->SendData( msg );
		bSendOK = bSendOK && m_NetChannel->Transmit();

		// remember this tickcount we send the reliable snapshot
		// so we can continue sending other updates if this has been acknowledged
		m_nForceWaitForTick = pFrame->tick_count;
	}
	else
	{
		if ( replay_debug.GetInt() > 10 )
			Msg( "HLTV send %d-delta of frame %d: %d bytes\n", pFrame->tick_count - pDeltaFrame->tick_count, pFrame->tick_count, ( msg.m_iCurBit + 7 ) / 8 );
		// just send it as unreliable snapshot
		bSendOK = m_NetChannel->SendDatagram( &msg ) > 0;
	}

	if ( !bSendOK )
	{
		Disconnect( "Snapshot error" );
		return false;
	}

	return true;
}



bool CGameClient::CanStartHltvReplay()
{
	CActiveHltvServerIterator hltv;
	if ( hltv && !IsFakeClient() )
	{
		int nOldestHltvTick = hltv->GetOldestTick();
		return ( nOldestHltvTick > 0 ); // we should have some ticks in history to proceed successfully
	}
	return false;
}

void CGameClient::ResetReplayRequestTime()
{
	m_flHltvLastReplayRequestTime = -spec_replay_message_time.GetFloat();
}

bool CGameClient::StartHltvReplay( const HltvReplayParams_t &params )
{
	m_HltvReplayStats.nStartRequests++;
	CActiveHltvServerIterator hltv;
	if ( hltv && !IsFakeClient() )
	{
		if ( !params.m_bAbortCurrentReplay && m_nHltvReplayDelay )
		{
			// we're already in replay, do not abort it to start a new one
			DevMsg( "Hltv Replay failure: already in replay\n" );
			m_HltvReplayStats.nFailedReplays[ HltvReplayStats_t::FAILURE_ALREADY_IN_REPLAY ]++;
			return false;
		}

		int nServerTick = sv.GetTick();
		float flRealTime = Plat_FloatTime();
		if ( fabsf( flRealTime - m_flHltvLastReplayRequestTime ) <= spec_replay_rate_limit.GetFloat() )
		{
			DevMsg( "Hltv Replay failure: requests are rate limited to no more than 1 per %g seconds\n", spec_replay_rate_limit.GetFloat() );
			m_HltvReplayStats.nFailedReplays[ HltvReplayStats_t::FAILURE_TOO_FREQUENT ]++;
			return false;
		}

		int nOldestHltvTick = hltv->GetOldestTick();
		if ( nOldestHltvTick > 0 ) // we should have some ticks in history to proceed successfully
		{
			// GetMaxAckTickCount() cannot be older than signon tick, and I don't know how much logic quietly relies on it, so to make it easier on myself I just won't allow reaching further into the past than the signon time, at least for the first iteration of replay
			int nOldestClientTick = Max( m_nSignonTick, nOldestHltvTick );

			float flTickInterval = sv.GetTickInterval();
			int nDesiredReplayDelay = params.m_flDelay / flTickInterval, nNewReplayDelay = nDesiredReplayDelay;
			int nNewReplayStopAt = nServerTick + params.m_flStopAt / flTickInterval;

			Assert( hltv->m_CurrentFrame->tick_count >= hltv->m_nFirstTick ); // first known tick should have happened at or before the time of the current recorded HLTV frame
			if ( nServerTick - nNewReplayDelay < nOldestClientTick )
			{
				nNewReplayDelay = nServerTick - nOldestClientTick;
			}

			if ( nNewReplayDelay <= 0 || nNewReplayStopAt <= nServerTick - nNewReplayDelay )
			{
				m_HltvReplayStats.nFailedReplays[ HltvReplayStats_t::FAILURE_NO_FRAME ]++;
				nNewReplayDelay = nNewReplayStopAt = 0;
				m_pCurrentFrame = NULL;
			}
			else
			{
				m_pCurrentFrame = hltv->ExpandAndGetClientFrame( nServerTick - nNewReplayDelay, false );
				if ( m_pCurrentFrame )
				{
					nNewReplayDelay = nServerTick - m_pCurrentFrame->tick_count;
				}
				else
				{
					m_HltvReplayStats.nFailedReplays[ HltvReplayStats_t::FAILURE_NO_FRAME2 ]++;
					nNewReplayDelay = nNewReplayStopAt = 0;
				}
			}

			if ( !m_pCurrentFrame || abs( nNewReplayDelay - nDesiredReplayDelay ) > 64 )
			{
				DevMsg( "Hltv replay delay %u cannot match the requested delay %u\n", nNewReplayDelay, nDesiredReplayDelay );
				m_HltvReplayStats.nFailedReplays[ HltvReplayStats_t::FAILURE_CANNOT_MATCH_DELAY ]++;
				nNewReplayDelay = nNewReplayStopAt = 0; // couldn't find anything decently approaching the desired delay in history
			}

			// now commit all the changes if needed
			m_nHltvReplayStopAt = nNewReplayStopAt;
			m_nHltvReplayStartAt = nServerTick;
			if ( nNewReplayDelay != m_nHltvReplayDelay )
			{
				CSVCMsg_HltvReplay_t msg;
				msg.set_delay( nNewReplayDelay );
				msg.set_primary_target( params.m_nPrimaryTargetEntIndex );
				msg.set_replay_stop_at( nNewReplayStopAt );
				msg.set_replay_start_at( m_nHltvReplayStartAt );

				if ( params.m_flSlowdownRate > 1.0f / 16.0f && params.m_flSlowdownBeginAt + 0.125f < params.m_flSlowdownEndAt )
				{
					m_flHltvReplaySlowdownRate = params.m_flSlowdownRate;
					m_nHltvReplaySlowdownBeginAt = Max<int>( nServerTick - nNewReplayDelay, nServerTick + params.m_flSlowdownBeginAt / flTickInterval );
					m_nHltvReplaySlowdownEndAt = Max<int>( m_nHltvReplaySlowdownBeginAt, nServerTick + params.m_flSlowdownEndAt / flTickInterval );
					msg.set_replay_slowdown_rate( m_flHltvReplaySlowdownRate );
					msg.set_replay_slowdown_begin( m_nHltvReplaySlowdownBeginAt );
					msg.set_replay_slowdown_end( m_nHltvReplaySlowdownEndAt );
				}
				else
				{
					m_flHltvReplaySlowdownRate = 1.0f;
					m_nHltvReplaySlowdownBeginAt = 0;
					m_nHltvReplaySlowdownEndAt = 0;
				}

				SendNetMsg( msg, true );
				//if( nNewReplayDelay) ExecuteStringCommand( "spectate" );

				if ( replay_debug.GetBool() )
					Msg( "Start HLMV Replay at %d, delay %d, until %d\n", nServerTick, nNewReplayDelay, nNewReplayStopAt );
				m_nHltvReplayDelay = nNewReplayDelay;
				m_pHltvReplayServer = hltv;
				m_nDeltaTick = -1;
				if ( m_nStringTableAckTick > nServerTick - nNewReplayDelay )
					m_nStringTableAckTick = 0; // need to reset the stringtables, as they were updated in the future relative to the delayed stream
				m_pLastSnapshot = NULL;
				m_nHltvLastSendTick = 0;
				FreeBaselines();
				// all these data become invalid once we start sending HLTV packets from the past
				m_PackInfo.Reset();
				m_PrevPackInfo.Reset();
				m_pCurrentFrame = NULL;
				DeleteClientFrames( -1 ); // Should we clean up all the frames? Seems logical, as we'll never need them
				m_flHltvLastReplayRequestTime = flRealTime;
				m_HltvReplayStats.nSuccessfulStarts++;
				return true;
			}
		}
		else
		{
			DevMsg( "Hltv Replay failure: HLTV frame is not ready\n" );
			m_HltvReplayStats.nFailedReplays[ HltvReplayStats_t::FAILURE_FRAME_NOT_READY ]++;
		}
	}
	return false;
}


CBaseClient *CGameClient::GetPropCullClient()
{
	return GetHltvReplayDelay() ? m_pHltvReplayServer->m_MasterClient : this;
}


static char s_HltvReplayBuffers[ 8 ][ 256 ];
uint s_nLastHltvReplayBuffer = 0;

const char *HltvReplayStats_t::AsString()const
{
	if ( !nSuccessfulStarts && !nStartRequests && !nFullReplays && !nUserCancels && !nStopRequests )
	{
		return "";
	}

	s_nLastHltvReplayBuffer++;
	if ( s_nLastHltvReplayBuffer >= ARRAYSIZE( s_HltvReplayBuffers ) )
		s_nLastHltvReplayBuffer = 0;
	char *pBuffer = s_HltvReplayBuffers[ s_nLastHltvReplayBuffer ];

	CUtlString fails = ":";
	int nTotalFailures = 0;
	for ( int i = 0; i < NUM_FAILURES; ++i )
	{
		if ( i )
			fails += ",";

		if ( nFailedReplays[ i ] )
		{
			fails.Append( CFmtStr( "%u", nFailedReplays[ i ] ) );
			nTotalFailures += nFailedReplays[ i ];
		}
	}
	
	if ( nNetAbortReplays )
	{
		fails.Append( CFmtStr( "[%u!]", nNetAbortReplays ) );
		nTotalFailures += nNetAbortReplays;
	}

	if ( !nTotalFailures )
		fails = "";
	
	V_snprintf( pBuffer, sizeof( s_HltvReplayBuffers[ s_nLastHltvReplayBuffer ] ), "%u/%u started, %u full, %u cancels / %u stops, %u fails%s", nSuccessfulStarts, nStartRequests, nFullReplays, nUserCancels, nStopRequests, nTotalFailures, fails.Get() );

	return pBuffer;
}


void CGameClient::StepHltvReplayStatus( int nServerTick )
{
	if ( IsHltvReplay() )
	{
		if ( m_nHltvLastSendTick >= m_nHltvReplayStopAt )
		{
			m_HltvReplayStats.nFullReplays++;
			StopHltvReplay();
		}
		else if ( m_nForceWaitForTick > 0 )
		{
			if ( !m_pCurrentFrame || m_pCurrentFrame->tick_count >= m_nHltvReplayStopAt )
			{
				// client doesn't respond or there's no current frame -both indicating some problem. And we're past the time alotted for replay - we should just abort.
				Msg( "Client %d (eidx %d, user id %d) %s - aborting wait for ack for tick %d, stopping replay at %d>=%d\n", m_nClientSlot, m_nEntityIndex, m_UserID, m_Name, m_nForceWaitForTick, m_pCurrentFrame ? m_pCurrentFrame->tick_count : 0, m_nHltvReplayStopAt );
				m_HltvReplayStats.nNetAbortReplays++;
				StopHltvReplay();
			}
		}
	}
}

void CGameClient::StopHltvReplay()
{
	if ( IsHltvReplay() )
	{
		m_HltvReplayStats.nStopRequests++;
		if ( m_nHltvLastSendTick < m_nHltvReplayStopAt )
			m_HltvReplayStats.nAbortStopRequests++;
		m_nHltvReplayStopAt = 0;
		m_nHltvReplayDelay = 0;
		m_nDeltaTick = -1;
		m_nForceWaitForTick = -1;
		m_pLastSnapshot = NULL; // it doesn't matter what last snapshot we sent; we need to send a full frame update
		m_nHltvLastSendTick = 0;
		FreeBaselines();
		m_pCurrentFrame = NULL;
		DeleteClientFrames( -1 ); // Should we clean up all the frames? Seems logical, as we'll never need them
		Assert( CountClientFrames() == 0 ); // we shouldn't have used the client frame manager to send HLTV stream to client
	}
	// just in case, send the end-of-hltv replay message even if we are not in replay
	{
		CSVCMsg_HltvReplay_t msg;
		SendNetMsg( msg, true );
	}
}




//-----------------------------------------------------------------------------
// This function contains all the logic to determine if we should send a datagram
// to a particular client
//-----------------------------------------------------------------------------

bool CGameClient::ShouldSendMessages( void )
{
	if ( m_bIsHLTV )
	{
		// calc snapshot interval
		if ( CHLTVServer *hltv = GetAnyConnectedHltvServer() )
		{
			int nSnapshotInterval = 1.0f / ( m_Server->GetTickInterval() * hltv->GetSnapshotRate() );
			// I am the HLTV client, record every nSnapshotInterval tick
			return ( sv.m_nTickCount >= ( hltv->m_nLastTick + nSnapshotInterval ) );
		}
		else
		{
			return false; // something is wrong, it'll assert in GetAnyConnectedHltvServer()..
		}
	}
	
#if defined( REPLAY_ENABLED )
	if ( m_bIsReplay )
	{
		// calc snapshot interval
		int nSnapshotInterval = 1.0f / ( m_Server->GetTickInterval() * replay_snapshotrate.GetFloat() );

		// I am the Replay client, record every nSnapshotInterval tick
		return ( sv.m_nTickCount >= (replay->m_nLastTick + nSnapshotInterval) );
	}
#endif

	// If sv_stressbots is true, then treat a bot more like a regular client and do deltas and such for it.
	if( !sv_replaybots.GetBool() && IsFakeClient() )
	{
		if ( !sv_stressbots.GetBool() )
			return false;
	}

	return BaseClass::ShouldSendMessages();
}

void CGameClient::FileReceived( const char *fileName, unsigned int transferID, bool bIsReplayDemoFile /* = false */  )
{
	//check if file is one of our requested custom files
	for ( int i=0; i<MAX_CUSTOM_FILES; i++ )
	{
		if ( m_nCustomFiles[i].reqID == transferID )
		{
			m_nFilesDownloaded++;

			// broadcast update to other clients so they start downlaoding this file
			m_Server->UserInfoChanged( m_nClientSlot );
			return;
		}
	}

	Msg( "CGameClient::FileReceived: %s not wanted.\n", fileName );
}

void CGameClient::FileRequested(const char *fileName, unsigned int transferID, bool bIsReplayDemoFile /* = false */ )
{
	DevMsg( "File '%s' requested from client %s.\n", fileName, m_NetChannel->GetAddress() );

	if ( sv_allowdownload.GetBool() )
	{
		m_NetChannel->SendFile( fileName, transferID, bIsReplayDemoFile );
	}
	else
	{
		m_NetChannel->DenyFile( fileName, transferID, bIsReplayDemoFile );
	}
}

void CGameClient::FileDenied(const char *fileName, unsigned int transferID, bool bIsReplayDemoFile /* = false */  )
{
	ConMsg( "Downloading file '%s' from client %s failed.\n", fileName, GetClientName() );
}

void CGameClient::FileSent(const char *fileName, unsigned int transferID, bool bIsReplayDemoFile /* = false */  )
{
	ConMsg( "Sent file '%s' to client %s.\n", fileName, GetClientName() );
}

void CGameClient::PacketStart(int incoming_sequence, int outgoing_acknowledged)
{
	for ( int i = 1; i < host_state.max_splitscreen_players; ++i )
	{
		if ( !m_SplitScreenUsers[ i ] )
			continue;

		m_SplitScreenUsers[ i ]->PacketStart( incoming_sequence, outgoing_acknowledged );
	}

	// make sure m_LastMovementTick != sv.tickcount
	m_LastMovementTick = ( sv.m_nTickCount - 1 );

	host_client = this;

	// During connection, only respond if client sends a packet
	m_bReceivedPacket = true; 
}

void CGameClient::PacketEnd()
{
	// Fix up clock in case prediction/etc. code reset it.
	g_ServerGlobalVariables.frametime = host_state.interval_per_tick;
}

void CGameClient::ConnectionClosing(const char *reason)
{
	SV_RedirectEnd();

	Disconnect( (reason!=NULL)?reason:"Connection closing" );	
}

void CGameClient::ConnectionCrashed(const char *reason)
{
	if ( m_Name[0] && IsConnected() )
	{
		SV_RedirectEnd();

		Disconnect( (reason!=NULL)?reason:"Connection lost" );	
	}
}

CClientFrame *CGameClient::GetSendFrame()
{
	CClientFrame *pFrame = m_pCurrentFrame;

	// just return if replay is disabled
	if ( sv_maxreplay.GetFloat() <= 0 || IsHltvReplay() )
		return pFrame;
			
	int followEntity;

	int delayTicks = serverGameClients->GetReplayDelay( edict, followEntity );

	bool isInReplayMode = ( delayTicks > 0 );

	if ( isInReplayMode != m_bIsInReplayMode )
	{
		// force a full update when modes are switched
		m_nDeltaTick = -1; 

		m_bIsInReplayMode = isInReplayMode;

		if ( isInReplayMode )
		{
			m_nEntityIndex = followEntity;
		}
		else
		{
			m_nEntityIndex = m_nClientSlot+1;
		}
	}

	Assert( (m_nClientSlot+1 == m_nEntityIndex) || isInReplayMode );

	if ( isInReplayMode )
	{
		CGameClient *pFollowPlayer = sv.Client( followEntity-1 );

		if ( !pFollowPlayer )
			return NULL;

		pFrame = pFollowPlayer->GetClientFrame( sv.GetTick() - delayTicks, false );

		if ( !pFrame )
			return NULL;

		if ( m_pLastSnapshot == pFrame->GetSnapshot() )
			return NULL;
	}

	return pFrame;
}

bool CGameClient::IgnoreTempEntity( CEventInfo *event )
{
	// in replay mode replay all temp entities
	if ( m_bIsInReplayMode )
		return false;

	return BaseClass::IgnoreTempEntity( event );
}


const CCheckTransmitInfo* CGameClient::GetPrevPackInfo()
{
	Assert( !IsHltvReplay() ); // we don't maintain this data during Hltv-fed replay
	return &m_PrevPackInfo;
}

// This code is useful for verifying that the networking of soundinfo_t stuff isn't borked.
#if 0  

#include "vstdlib/random.h"

class CTestSoundInfoNetworking
{
public:

	CTestSoundInfoNetworking();

	void RunTest();

private:

	void CreateRandomSounds( int nCount );
	void CreateRandomSound( SoundInfo_t &si );
	void Compare( const SoundInfo_t &s1, const SoundInfo_t &s2 );

	CUtlVector< SoundInfo_t >	m_Sounds;

	CUtlVector< SoundInfo_t >	m_Received;
};

static CTestSoundInfoNetworking g_SoundTest;

CON_COMMAND( st, "sound test" )
{
	int nCount = 1;
	if ( args.ArgC() >= 2 )
	{
		nCount = clamp( Q_atoi( args.Arg( 1 ) ), 1, 100000 );
	}

	for ( int i = 0 ; i < nCount; ++i )
	{
		if ( !( i % 100 ) && i > 0 )
		{
			Msg( "Running test %d %f %% done\n",
				i, 100.0f * (float)i/(float)nCount );
		}
		g_SoundTest.RunTest();
	}
}
CTestSoundInfoNetworking::CTestSoundInfoNetworking()
{
}

void CTestSoundInfoNetworking::CreateRandomSound( SoundInfo_t &si )
{
	int entindex = RandomInt( 0, MAX_EDICTS - 1 );
	int channel = RandomInt( 0, 7 );
	int soundnum = RandomInt( 0, MAX_SOUNDS - 1 );
	Vector org = RandomVector( -16383, 16383 );
	Vector dir = RandomVector( -1.0f, 1.0f );
	float flVolume = RandomFloat( 0.1f, 1.0f );
	bool bLooping = RandomInt( 0, 100 ) < 5;
	int nPitch = RandomInt( 0, 100 ) < 5 ? RandomInt( 95, 105 ) : 100;
	Vector lo = RandomInt( 0, 100 ) < 5 ? RandomVector( -16383, 16383 ) : org;
	int speaker = RandomInt( 0, 100 ) < 2 ? RandomInt( 0, MAX_EDICTS - 1 ) : -1;
	soundlevel_t level = soundlevel_t(RandomInt( 70, 150 ));

	si.Set( entindex, channel, "foo.wav", org, dir, flVolume, level, bLooping, nPitch, lo, speaker );

	si.nFlags = ( 1 << RandomInt( 0, 6 ) );
	si.nSoundNum = soundnum;
	si.bIsSentence = RandomInt( 0, 1 );
	si.bIsAmbient = RandomInt( 0, 1 );
	si.fDelay = RandomInt( 0, 100 ) < 2 ? RandomFloat( -0.1, 0.1f ) : 0.0f;
}

void CTestSoundInfoNetworking::CreateRandomSounds( int nCount )
{
	m_Sounds.Purge();
	m_Sounds.EnsureCount( nCount );

	for ( int i = 0; i < nCount; ++i )
	{
		SoundInfo_t &si = m_Sounds[ i ];
		CreateRandomSound( si );
	}
}

void CTestSoundInfoNetworking::RunTest()
{
	int m_nSoundSequence = 0;

	CreateRandomSounds( 512 );

	SoundInfo_t defaultSound; defaultSound.SetDefault();
	SoundInfo_t *pDeltaSound = &defaultSound;

	CSVCMsg_Sounds_t msg;

	msg.set_reliable_sound( false );
	msg.SetReliable( false );

	for ( int i = 0 ; i < m_Sounds.Count(); i++ )
	{
		SoundInfo_t &sound = m_Sounds[ i ];
		sound.WriteDelta( pDeltaSound, msg.m_DataOut );
		pDeltaSound = &m_Sounds[ i ];
	}

	// Now read them out
	defaultSound.SetDefault();
	pDeltaSound = &defaultSound;

	msg.m_DataIn.StartReading( buf, msg.m_DataOut.GetNumBytesWritten(), 0, msg.m_DataOut.GetNumBitsWritten() );

	SoundInfo_t sound;

	for ( int i=0; i<msg.m_nNumSounds; i++ )
	{
		sound.ReadDelta( pDeltaSound, msg.m_DataIn );

		pDeltaSound = &sound;	// copy delta values

		if ( msg.m_bReliableSound )
		{
			// client is incrementing the reliable sequence numbers itself
			m_nSoundSequence = ( m_nSoundSequence + 1 ) & SOUND_SEQNUMBER_MASK;

			Assert ( sound.nSequenceNumber == 0 );

			sound.nSequenceNumber = m_nSoundSequence;
		}

		// Add no ambient sounds to sorted queue, will be processed after packet has been completly parsed
		// CL_AddSound( sound );
		m_Received.AddToTail( sound );
	}

	
	// Now validate them
	for ( int i = 0 ; i < msg.m_nNumSounds; ++i )
	{
		SoundInfo_t &server = m_Sounds[ i ];
		SoundInfo_t &client = m_Received[ i ];

		Compare( server, client );
	}

	m_Sounds.Purge();
	m_Received.Purge();
}

void CTestSoundInfoNetworking::Compare( const SoundInfo_t &s1, const SoundInfo_t &s2 )
{
	bool bSndStop = s2.nFlags == SND_STOP;
	
	if ( !bSndStop && s1.nSequenceNumber != s2.nSequenceNumber )
	{
		Msg( "seq number mismatch %d %d\n", s1.nSequenceNumber, s2.nSequenceNumber );
	}


	if ( s1.nEntityIndex != s2.nEntityIndex )
	{
		Msg( "ent mismatch %d %d\n", s1.nEntityIndex, s2.nEntityIndex );
	}

	if ( s1.nChannel != s2.nChannel )
	{
		Msg( "channel mismatch %d %d\n", s1.nChannel, s2.nChannel );
	}

	Vector d;

	d = s1.vOrigin - s2.vOrigin;

	if ( !bSndStop && d.Length() > 32.0f )
	{
		Msg( "origin mismatch [%f] (%f %f %f) != (%f %f %f)\n", d.Length(), s1.vOrigin.x, s1.vOrigin.y, s1.vOrigin.z, s2.vOrigin.x, s2.vOrigin.y, s2.vOrigin.z );
	}

	// Vector			vDirection;
	float delta = fabs( s1.fVolume - s2.fVolume );

	if ( !bSndStop && delta > 1.0f )
	{
		Msg( "vol mismatch %f %f\n", s1.fVolume, s2.fVolume );
	}


	if ( !bSndStop && s1.Soundlevel != s2.Soundlevel )
	{
		Msg( "sndlvl mismatch %d %d\n", s1.Soundlevel, s2.Soundlevel );
	}

	// bLooping; 

	if ( s1.bIsSentence != s2.bIsSentence )
	{
		Msg( "sentence mismatch %d %d\n", s1.bIsSentence ? 1 : 0, s2.bIsSentence ? 1 : 0 );
	}
	if ( s1.bIsAmbient != s2.bIsAmbient )
	{
		Msg( "ambient mismatch %d %d\n", s1.bIsAmbient ? 1 : 0, s2.bIsAmbient ? 1 : 0 );
	}

	if ( !bSndStop && s1.nPitch != s2.nPitch )
	{
		Msg( "pitch mismatch %d %d\n", s1.nPitch, s2.nPitch );
	}

	// Vector			vListenerOrigin;

	if ( s1.nFlags != s2.nFlags )
	{
		Msg( "flags mismatch %d %d\n", s1.nFlags, s2.nFlags );
	}

	if ( s1.nSoundNum != s2.nSoundNum )
	{
		Msg( "soundnum mismatch %d %d\n", s1.nSoundNum, s2.nSoundNum );
	}

	delta = fabs( s1.fDelay - s2.fDelay );

	if ( !bSndStop && delta > 0.020f )
	{
		Msg( "delay mismatch %f %f\n", s1.fDelay, s2.fDelay );
	}


	if ( !bSndStop && s1.nSpeakerEntity != s2.nSpeakerEntity )
	{
		Msg( "speakerentity mismatch %d %d\n", s1.nSpeakerEntity, s2.nSpeakerEntity );
	}
}

#endif
