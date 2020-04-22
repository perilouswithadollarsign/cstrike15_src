//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// replayclient.cpp: implementation of the CReplayClient class.
//
// $NoKeywords: $
//
//===========================================================================//
#if defined( REPLAY_ENABLED )
#include <tier0/vprof.h>
#include "replayclient.h"
#include "netmessages.h"
#include "replayserver.h"
#include "framesnapshot.h"
#include "networkstringtable.h"
#include "dt_send_eng.h"
#include "GameEventManager.h"
#include "cmd.h"
#include "ireplaydirector.h"
#include "host.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar replay_maxrate( "replay_maxrate", "8000", 0, "Max SourceTV spectator bandwidth rate allowed, 0 == unlimited" );
static ConVar replay_relaypassword( "replay_relaypassword", "", FCVAR_NOTIFY | FCVAR_PROTECTED | FCVAR_DONTRECORD, "SourceTV password for relay proxies" );
static ConVar replay_chattimelimit( "replay_chattimelimit", "8", 0, "Limits spectators to chat only every n seconds" );
static ConVar replay_chatgroupsize( "replay_chatgroupsize", "0", 0, "Set the default chat group size" );

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CReplayClient::CReplayClient(int slot, CBaseServer *pServer)
{
	Clear();

	Assert( replay == pServer );

	m_nClientSlot = slot;
	m_Server = pServer;
	m_pReplay = dynamic_cast<CReplayServer*>(pServer);
	m_nEntityIndex = m_pReplay->GetReplaySlot() + 1;
	m_nLastSendTick = 0;
	m_fLastSendTime = 0.0f;
	m_flLastChatTime = 0.0f;
	m_bNoChat = false;

	if ( replay_chatgroupsize.GetInt() > 0  )
	{
		Q_snprintf( m_szChatGroup, sizeof(m_szChatGroup), "group%d", slot%replay_chatgroupsize.GetInt()  );
	}
	else
	{
		Q_strncpy( m_szChatGroup, "all", sizeof(m_szChatGroup) );
	}
}

CReplayClient::~CReplayClient()
{

}

bool CReplayClient::SendSignonData( void )
{
	// check class table CRCs
	if ( m_nSendtableCRC != SendTable_GetCRC() )
	{
		Disconnect( "Server uses different class tables" );
		return false;
	}
	else
	{
		// use your class infos, CRC is correct
		CSVCMsg_ClassInfo_t classmsg;
		classmsg.set_create_on_client( true );
		m_NetChannel->SendNetMsg( classmsg );;
	}

	 return CBaseClient::SendSignonData();
}

bool CHLTVClient::CLCMsg_ClientInfo( const CCLCMsg_ClientInfo& msg )
{
	if ( !CBaseClient::CLCMsg_ClientInfo( msg ) )
		return false;

	return true;
}

bool CReplayClient::CLCMsg_Move( const CCLCMsg_Move& msg )
{
	// Replay clients can't move
	return true;
}

bool CReplayClient::CLCMsg_ListenEvents( const CCLCMsg_ListenEvents& msg )
{
	// Replay clients can't subscribe to events, we just send them
	return true;
}

bool CReplayClient::CLCMsg_RespondCvarValue( const CCLCMsg_RespondCvarValue& msg );
{
	return true;
}

bool CReplayClient::CLCMsg_FileCRCCheck( const CCLCMsg_FileCRCCheck& msg )
{
	return true;
}

bool CReplayClient::CLCMsg_VoiceData(const CCLCMsg_VoiceData& msg)
{
	// Replay clients can't speak
	return true;
}

void CReplayClient::ConnectionClosing(const char *reason)
{
	Disconnect ( (reason!=NULL)?reason:"Connection closing" );	
}

void CReplayClient::ConnectionCrashed(const char *reason)
{
	Disconnect ( (reason!=NULL)?reason:"Connection lost" );	
}

void CReplayClient::PacketStart(int incoming_sequence, int outgoing_acknowledged)
{
	// During connection, only respond if client sends a packet
	m_bReceivedPacket = true; 
}

void CReplayClient::PacketEnd()
{
	
}

void CReplayClient::FileRequested(const char *fileName, unsigned int transferID, bool isReplayDemoFile)
{
	DevMsg( "CReplayClient::FileRequested: %s.\n", fileName );
	m_NetChannel->DenyFile( fileName, transferID, isReplayDemoFile );
}

void CReplayClient::FileDenied(const char *fileName, unsigned int transferID, bool isReplayDemoFile)
{
	DevMsg( "CReplayClient::FileDenied: %s.\n", fileName );
}

void CReplayClient::FileReceived( const char *fileName, unsigned int transferID, bool isReplayDemoFile )
{
	DevMsg( "CReplayClient::FileReceived: %s.\n", fileName );
}

void CReplayClient::FileSent(const char *fileName, unsigned int transferID, bool isReplayDemoFile )
{
	DevMsg( "CReplayClient::FileSent: %s.\n", fileName );
}

CClientFrame *CReplayClient::GetDeltaFrame( int nTick )
{
	return m_pReplay->GetDeltaFrame( nTick );
}


bool CReplayClient::ExecuteStringCommand( const char *pCommandString )
{
	// first let the baseclass handle it
	if ( CBaseClient::ExecuteStringCommand( pCommandString ) )
		return true;

	if ( !pCommandString || !pCommandString[0] )
		return true;

	CCommand args;
	if ( !args.Tokenize( pCommandString ) )
		return true;

	const char *cmd = args[ 0 ];

	if ( !Q_stricmp( cmd, "spec_next" ) || 
		 !Q_stricmp( cmd, "spec_prev" ) ||
		 !Q_stricmp( cmd, "spec_mode" ) )
	{
		ClientPrintf("Camera settings can't be changed during a live broadcast.\n");
		return true;
	}
	
	if ( !Q_stricmp( cmd, "say" ) && args.ArgC() > 1 )
	{
		// if replay_chattimelimit = 0, chat is turned off
		if ( replay_chattimelimit.GetFloat() <= 0 )
			return true;

		if ( (m_flLastChatTime + replay_chattimelimit.GetFloat()) > net_time )
			return true;

		m_flLastChatTime = net_time;

		char chattext[128];

		Q_snprintf( chattext, sizeof(chattext), "%s : %s", GetClientName(), args[1]  );
		
		m_pReplay->BroadcastLocalChat( chattext, m_szChatGroup );
		
		return true;
	}
	else if ( !Q_strcmp( cmd, "replay_chatgroup" )  )
	{
		if (  args.ArgC() > 1 )
		{
			Q_strncpy( m_szChatGroup, args[1], sizeof(m_szChatGroup) );
		}
		else
		{
			ClientPrintf("Your current chat group is \"%s\"\n", m_szChatGroup );
		}
		return true;
	}
	else if ( !Q_strcmp( cmd, "status" ) )
	{
		char	gd[MAX_OSPATH];
		Q_FileBase( com_gamedir, gd, sizeof( gd ) );
		
		ClientPrintf("SourceTV Master \"%s\", delay %.0f\n", 
			m_pReplay->GetName(),	m_pReplay->GetDirector()->GetDelay() );

		ClientPrintf("IP %s:%i, Online %s, Version %i (%s)\n",
			net_local_adr.ToString( true ), m_pReplay->GetUDPPort(),
			COM_FormatSeconds( m_pReplay->GetOnlineTime() ), build_number(),
#ifdef _WIN32
			"Win32" );
#else
			"Linux" );
#endif

		ClientPrintf("Game Time %s, Mod \"%s\", Map \"%s\", Players %i\n", COM_FormatSeconds( m_pReplay->GetTime() ),
			gd, m_pReplay->GetMapName(), m_pReplay->GetNumPlayers() );
	}
	else
	{
		DevMsg( "CReplayClient::ExecuteStringCommand: Unknown command %s.\n", pCommandString );
	}

	return true;
}

bool CReplayClient::ShouldSendMessages( void )
{
	if ( !IsActive() )
	{
		// during signon behave like normal client
		return CBaseClient::ShouldSendMessages();
	}

	// Replay clients use snapshot rate used by Replay server, not given by Replay client

	// if the reliable message overflowed, drop the client
	if ( m_NetChannel->IsOverflowed() )
	{
		m_NetChannel->Reset();
		Disconnect ("%s overflowed reliable buffer\n", m_Name );
		return false;
	}

	// send a packet if server has a new tick we didn't already send
	bool bSendMessage = ( m_nLastSendTick != m_Server->m_nTickCount );

	// send a packet at least every 2 seconds
	if ( !bSendMessage && (m_fLastSendTime + 2.0f) < net_time )
	{
		bSendMessage = true;	// force sending a message even if server didn't update
	}

	if ( bSendMessage && !m_NetChannel->CanPacket() )
	{
		// we would like to send a message, but bandwidth isn't available yet
		// in Replay we don't send choke information, doesn't matter
		bSendMessage = false;
	}

	return bSendMessage;
}

void CReplayClient::SpawnPlayer( void )
{
	// set view entity

	CSVCMsg_SetView_t setView;
	setView.set_entity_index( m_pReplay->m_nViewEntity );
	SendNetMsg( setView );

	m_pReplay->BroadcastLocalTitle( this ); 

	m_flLastChatTime = net_time;

	CBaseClient::SpawnPlayer();
}


void CReplayClient::SetRate(int nRate, bool bForce )
{
	if ( !bForce )
	{
		if ( m_bIsReplay )
		{
			// allow higher bandwidth rates for Replay proxies
			nRate = clamp( nRate, MIN_RATE, MAX_RATE );
		}
		else if ( replay_maxrate.GetInt() > 0 )
		{
			// restrict rate for normal clients to replay_maxrate
			nRate = clamp( nRate, MIN_RATE, replay_maxrate.GetInt() );
		}
	}

	CBaseClient::SetRate( nRate, bForce );
}

void CReplayClient::SetUpdateRate( float fUpdateRate, bool bForce)
{
	// for Replay clients ignore update rate settings, speed is replay_snapshotrate
	m_fSnapshotInterval = 1.0f / 100.0f;
}

bool CReplayClient::NETMsg_SetConVar(const CNETMsg_SetConVar& msg)
{
	if ( !CBaseClient::ProcessSetConVar( msg ) )
		return false;

	// if this is the first time we get user settings, check password etc
	if ( GetSignonState() == SIGNONSTATE_CONNECTED )
	{
		const char *checkpwd = NULL; 

		m_bIsReplay = m_ConVars->GetInt( "replay_relay", 0 ) != 0;

		if ( m_bIsReplay )
		{
			// if the connecting client is a TV relay, check the password
			checkpwd = replay_relaypassword.GetString();

			if ( checkpwd && checkpwd[0] && Q_stricmp( checkpwd, "none") )
			{
				if ( Q_stricmp( m_szPassword, checkpwd ) )
				{
					Disconnect("Bad relay password");
					return false;
				}
			}
		}
		else
		{
			// if client is a normal spectator, check if we can to forward him to other relays
			if ( m_pReplay->DispatchToRelay( this ) )
			{
				return false;
			}

			// if client stays here, check the normal password
			checkpwd = m_pReplay->GetPassword();

			if ( checkpwd )
			{
	
				if ( Q_stricmp( m_szPassword, checkpwd ) )
				{
					Disconnect("Bad spectator password");
					return false;
				}
			}

			// check if server is LAN only
			if ( !m_pReplay->CheckIPRestrictions( m_NetChannel->GetRemoteAddress(), PROTOCOL_HASHEDCDKEY ) )
			{
				Disconnect( "SourceTV server is restricted to local spectators (class C).\n" );
				return false;
			}

		}
	}

	return true;
}

void CReplayClient::UpdateUserSettings()
{
	// set voice loopback
	m_bNoChat = m_ConVars->GetInt( "replay_nochat", 0 ) != 0;
	
	CBaseClient::UpdateUserSettings();
}

void CReplayClient::SendSnapshot( CClientFrame * pFrame )
{
	VPROF_BUDGET( "CReplayClient::SendSnapshot", "Replay" );

	byte		buf[NET_MAX_PAYLOAD];
	bf_write	msg( "CReplayClient::SendSnapshot", buf, sizeof(buf) );

	// if we send a full snapshot (no delta-compression) before, wait until client
	// received and acknowledge that update. don't spam client with full updates

	if ( m_pLastSnapshot == pFrame->GetSnapshot() )
	{
		// never send the same snapshot twice
		m_NetChannel->Transmit();	
		return;
	}

	if ( m_nForceWaitForTick > 0 )
	{
		// just continue transmitting reliable data
		Assert( !m_bFakePlayer );	// Should never happen
		m_NetChannel->Transmit();	
		return;
	}

	CClientFrame	*pDeltaFrame = GetDeltaFrame( m_nDeltaTick ); // NULL if delta_tick is not found
	CReplayFrame		*pLastFrame = (CReplayFrame*) GetDeltaFrame( m_nLastSendTick );

	if ( pLastFrame )
	{
		// start first frame after last send
		pLastFrame = (CReplayFrame*) pLastFrame->m_pNext;
	}

	// add all reliable messages between ]lastframe,currentframe]
	// add all tempent & sound messages between ]lastframe,currentframe]
	while ( pLastFrame && pLastFrame->tick_count <= pFrame->tick_count )
	{	
		m_NetChannel->SendData( pLastFrame->m_Messages[REPLAY_BUFFER_RELIABLE], true );	

		if ( pDeltaFrame )
		{
			// if we send entities delta compressed, also send unreliable data
			m_NetChannel->SendData( pLastFrame->m_Messages[REPLAY_BUFFER_UNRELIABLE], false );
		}

		pLastFrame = (CReplayFrame*) pLastFrame->m_pNext;
	}

	// now create client snapshot packet

	// send tick time
	CNETMsg_Tick_t tickmsg( pFrame->tick_count, host_frametime_unbounded, host_frametime_stddeviation );
	tickmsg.WriteToBuffer( msg );

	// Update shared client/server string tables. Must be done before sending entities
	m_Server->m_StringTables->WriteUpdateMessage( NULL, GetMaxAckTickCount(), msg );

	// TODO delta cache whole snapshots, not just packet entities. then use net_Align
	// send entity update, delta compressed if deltaFrame != NULL
	m_Server->WriteDeltaEntities( this, pFrame, pDeltaFrame, msg );

	// write message to packet and check for overflow
	if ( msg.IsOverflowed() )
	{
		if ( !pDeltaFrame )
		{
			// if this is a reliable snapshot, drop the client
			Disconnect( "ERROR! Reliable snapshot overflow." );
			return;
		}
		else
		{
			// unreliable snapshots may be dropped
			ConMsg ("WARNING: msg overflowed for %s\n", m_Name);
			msg.Reset();
		}
	}

	// remember this snapshot
	m_pLastSnapshot = pFrame->GetSnapshot();
	m_nLastSendTick = pFrame->tick_count;

	// Don't send the datagram to fakeplayers
	if ( m_bFakePlayer )
	{
		m_nDeltaTick = pFrame->tick_count;
		return;
	}

	bool bSendOK;

	// is this is a full entity update (no delta) ?
	if ( !pDeltaFrame )
	{
		// transmit snapshot as reliable data chunk
		bSendOK = m_NetChannel->SendData( msg );
		bSendOK = bSendOK && m_NetChannel->Transmit();

		// remember this tickcount we send the reliable snapshot
		// so we can continue sending other updates if this has been acknowledged
		m_nForceWaitForTick = pFrame->tick_count;
	}
	else
	{
		// just send it as unreliable snapshot
		bSendOK = m_NetChannel->SendDatagram( &msg ) > 0;
	}

	if ( !bSendOK )
	{
		Disconnect( "ERROR! Couldn't send snapshot." );
	}
}

#endif
