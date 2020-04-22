//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#if defined( REPLAY_ENABLED )
#include <tier1/strtools.h>
#include <eiface.h>
#include <bitbuf.h>
#include <time.h>
#include "replaydemo.h"
#include "replayserver.h"
#include "demo.h"
#include "host_cmd.h"
#include "demofile/demoformat.h"
#include "filesystem_engine.h"
#include "net.h"
#include "networkstringtable.h"
#include "dt_common_eng.h"
#include "host.h"
#include "server.h"
#include "networkstringtableclient.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar replay_record_directly_to_disk( "replay_record_directly_to_disk", "0", FCVAR_HIDDEN );

extern CNetworkStringTableContainer *networkStringTableContainerServer;
extern CGlobalVars g_ServerGlobalVariables;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CReplayDemoRecorder::CReplayDemoRecorder( CReplayServer* pServer )
{
	m_bIsRecording = false;

	Assert( pServer );
	m_pReplayServer = pServer;

	m_nStartTick = -1;
}

CReplayDemoRecorder::~CReplayDemoRecorder()
{
	StopRecording();
}

void CReplayDemoRecorder::GetUniqueDemoFilename( char* pOut, int nLength )
{
	Assert( pOut );
	tm today; 
	Plat_GetLocalTime( &today );
	Q_snprintf( pOut, nLength, "replay-%04i%02i%02i-%02i%02i%02i-%s.dem", 
		1900 + today.tm_year, today.tm_mon+1, today.tm_mday, 
		today.tm_hour, today.tm_min, today.tm_sec, m_pReplayServer->GetMapName() ); 
}

void CReplayDemoRecorder::StartAutoRecording() 
{
	char fileName[MAX_OSPATH];

	GetUniqueDemoFilename( fileName, sizeof(fileName) );

	StartRecording( fileName, false );
}

void CReplayDemoRecorder::StartRecording( const char *filename, bool bContinuously ) 
{
	StopRecording();	// stop if we're already recording
	
	if ( !m_DemoFile.Open( filename, false, !replay_record_directly_to_disk.GetInt() ) )
	{
		ConMsg ("StartRecording: couldn't open demo file %s.\n", filename );
		return;
	}

	ConMsg ("Recording Replay demo to %s...\n", filename);

	demoheader_t *dh = &m_DemoFile.m_DemoHeader;

	// open demo header file containing sigondata
	Q_memset( dh, 0, sizeof(demoheader_t) );

	Q_strncpy( dh->demofilestamp, DEMO_HEADER_ID, sizeof(dh->demofilestamp) );
	dh->demoprotocol = DEMO_PROTOCOL;
	dh->networkprotocol = GetHostVersion();

	Q_strncpy( dh->mapname, m_pReplayServer->GetMapName(), sizeof( dh->mapname ) );

	char szGameDir[MAX_OSPATH];
	Q_strncpy(szGameDir, com_gamedir, sizeof( szGameDir ) );
	Q_FileBase ( szGameDir, dh->gamedirectory, sizeof( dh->gamedirectory ) );

	Q_strncpy( dh->servername, host_name.GetString(), sizeof( dh->servername ) );

	Q_strncpy( dh->clientname, "Replay Demo", sizeof( dh->servername ) );

	// write demo file header info
	m_DemoFile.WriteDemoHeader();
	
	dh->signonlength = WriteSignonData(); // demoheader will be written when demo is closed

	m_nFrameCount = 0;

	// Using this tickcount allows us to sync up client-side recorded ragdolls later with replay demos on clients
	m_nStartTick = g_ServerGlobalVariables.tickcount;

	// Demo playback should read this as an incoming message.
	// Write the client's realtime value out so we can synchronize the reads.
	m_DemoFile.WriteCmdHeader( dem_synctick, 0, 0 );

	// This tells the demo buffer (see demobuffer.cpp) that we are done writing all signon data, so that
	// it can maintain be maintained separately, so that we can write a demo after stale frames have been
	// removed.
	m_DemoFile.NotifySignonComplete();

	m_bIsRecording = true;

	m_SequenceInfo = 1;
	m_nDeltaTick = -1;
}

bool CReplayDemoRecorder::IsRecording()
{
	return m_bIsRecording;
}

void CReplayDemoRecorder::StopRecording( const CGameInfo *pGameInfo )
{
	if ( !m_bIsRecording )
		return;

	// Demo playback should read this as an incoming message.
	m_DemoFile.NotifyBeginFrame();
		m_DemoFile.WriteCmdHeader( dem_stop, GetRecordingTick(), 0 );
	m_DemoFile.NotifyEndFrame();

	// update demo header info
	// This stuff gets computed in demobuffer.cpp
// 	m_DemoFile.m_DemoHeader.playback_ticks = GetRecordingTick();
// 	m_DemoFile.m_DemoHeader.playback_time =  host_state.interval_per_tick *	GetRecordingTick();
// 	m_DemoFile.m_DemoHeader.playback_frames = m_nFrameCount;

	// write updated version
	m_DemoFile.WriteDemoHeader();

	m_DemoFile.Close();

	m_bIsRecording = false;

	// clear writing data buffer
	if ( m_MessageData.GetBasePointer() )
	{
		delete [] m_MessageData.GetBasePointer();
		m_MessageData.StartWriting( NULL, 0 );
	}
	
// 	ConMsg("Completed Replay demo \"%s\", recording time %.1f\n",
// 		m_DemoFile.m_szFileName,
// 		m_DemoFile.m_DemoHeader.playback_time );
}

void CReplayDemoRecorder::DumpToFile( char const *filename )
{
	// No need to write header here, since the demo buffer's dump function writes the adjusted header
	m_DemoFile.DumpBufferToFile( filename, m_DemoFile.m_DemoHeader );
}

CDemoFile *CReplayDemoRecorder::GetDemoFile()
{
	return &m_DemoFile;
}

int CReplayDemoRecorder::GetRecordingTick()
{
	return g_ServerGlobalVariables.tickcount - m_nStartTick;
}

void CReplayDemoRecorder::WriteServerInfo()
{
	byte		buffer[ NET_MAX_PAYLOAD ];
	bf_write	msg( "CReplayDemoRecorder::WriteServerInfo", buffer, sizeof( buffer ) );

	SVC_ServerInfo serverinfo;	// create serverinfo message

	// on the master demos are using sv object, on relays replay
	CSVCMsg_ServerInfo_t serverinfo;	// create serverinfo message
	
	m_pReplayServer->FillServerInfo( serverinfo ); // fill rest of info message
	
	serverinfo.WriteToBuffer( msg );

	// send first tick
	CNETMsg_Tick_t signonTick( m_nSignonTick, 0, 0 );
	signonTick.WriteToBuffer( msg );

	// Write replicated ConVars to non-listen server clients only
	NETMsg_SetConVar_t convars;
	// build a list of all replicated convars
	Host_BuildConVarUpdateMessage( convars.m_ConVars, FCVAR_REPLICATED, true );

	// for Replay server demos write set "replay_transmitall 1" even
	// if it's off for the real broadcast
	NetMessageCvar_t acvar;
	Q_strncpy( acvar.name, "replay_transmitall", MAX_OSPATH );
	Q_strncpy( acvar.value, "1", MAX_OSPATH );
	convars.m_ConVars.AddToTail( acvar );

	// write convars to demo
	convars.WriteToBuffer( msg );

	// write stringtable baselines
#ifndef SHARED_NET_STRING_TABLES
	m_pReplayServer->m_StringTables->WriteBaselines( pServer->GetMapName(), msg );
#endif

	// send signon state
	NET_SignonState signonMsg( SIGNONSTATE_NEW, pServer->GetSpawnCount() );
	signonMsg.WriteToBuffer( msg );

	WriteMessages( dem_signon, msg );
}

void CReplayDemoRecorder::RecordCommand( const char *cmdstring )
{
	if ( !IsRecording() )
		return;

	if ( !cmdstring || !cmdstring[0] )
		return;

	m_DemoFile.WriteConsoleCommand( cmdstring, GetRecordingTick(), 0 );
}

void CReplayDemoRecorder::RecordServerClasses( ServerClass *pClasses )
{
	byte data[NET_MAX_PAYLOAD];
	bf_write buf( data, sizeof(data) );

	// Send SendTable info.
	DataTable_WriteSendTablesBuffer( pClasses, &buf );

	// Send class descriptions.
	DataTable_WriteClassInfosBuffer( pClasses, &buf );

	// Now write the buffer into the demo file
	m_DemoFile.WriteNetworkDataTables( &buf, GetRecordingTick() );
}

void CReplayDemoRecorder::RecordCustomData( int iCallbackIndex, const void *pData, size_t iDataLength )
{
	m_DemoFile.WriteCustomData( iCallbackIndex, pData, iDataLength, GetRecordingTick() );
}

void CReplayDemoRecorder::RecordStringTables()
{
	byte data[256 * 1024];
	bf_write buf( data, sizeof(data) );

	networkStringTableContainerServer->WriteStringTables( buf );

	// Now write the buffer into the demo file
	m_DemoFile.WriteStringTables( &buf, GetRecordingTick() );
}

int CReplayDemoRecorder::WriteSignonData()
{
	int start = m_DemoFile.GetCurPos( false );

	// on the master demos are using sv object, on relays replay
	CBaseServer *pServer = (CBaseServer*)&sv;

	m_nSignonTick = pServer->m_nTickCount;		

	WriteServerInfo();

	RecordServerClasses( serverGameDLL->GetAllServerClasses() );
	RecordStringTables();

	byte		buffer[ NET_MAX_PAYLOAD ];
	bf_write	msg( "CReplayDemo::WriteSignonData", buffer, sizeof( buffer ) );

	// use your class infos, CRC is correct
	CSVCMsg_ClassInfo_t classmsg;
	classmsg.set_create_on_client( true );
	classmsg.WriteToBuffer( msg );

	// Write the regular signon now
	msg.WriteBits( m_pReplayServer->m_Signon.GetData(), m_pReplayServer->m_Signon.GetNumBitsWritten() );

	// write new state
	NET_SignonState signonMsg1( SIGNONSTATE_PRESPAWN, pServer->GetSpawnCount() );
	signonMsg1.WriteToBuffer( msg );

	WriteMessages( dem_signon, msg ); 
	msg.Reset();

	// set view entity
	CSVCMsg_SetView_t viewent;
	viewent.set_entity_index( m_pReplayServer->m_nViewEntity );
	viewent.WriteToBuffer( msg );

	// Spawned into server, not fully active, though
	NET_SignonState signonMsg2( SIGNONSTATE_SPAWN, pServer->GetSpawnCount() );
	signonMsg2.WriteToBuffer( msg );

	WriteMessages( dem_signon, msg ); 
	
	return m_DemoFile.GetCurPos( false ) - start;
}


void CReplayDemoRecorder::WriteFrame( CReplayFrame *pFrame )
{
	byte		buffer[ NET_MAX_PAYLOAD ];
	bf_write	msg( "CReplayDemo::RecordFrame", buffer, sizeof( buffer ) );

	DevMsg( "CReplayDemoRecorder::WriteFrame, tick %d\n", GetRecordingTick() );

	//first write reliable data
	bf_write *data = &pFrame->m_Messages[REPLAY_BUFFER_RELIABLE];
	if ( data->GetNumBitsWritten() )
		msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten() );

	//now send snapshot data

	// send tick time
	CNETMsg_Tick_t tickmsg( pFrame->tick_count, host_frametime_unbounded, host_frametime_stddeviation );
	tickmsg.WriteToBuffer( msg );


#ifndef SHARED_NET_STRING_TABLES
	// Update shared client/server string tables. Must be done before sending entities
	sv.m_StringTables->WriteUpdateMessage( NULL, MAX( m_nSignonTick, m_nDeltaTick ), msg );
#endif

	// get delta frame
	CClientFrame *deltaFrame = m_pReplayServer->GetClientFrame( m_nDeltaTick ); // NULL if m_nDeltaTick is not found or -1
//	CClientFrame *deltaFrame = NULL; // For full update

	// send entity update, delta compressed if deltaFrame != NULL
	sv.WriteDeltaEntities( m_pReplayServer->m_MasterClient, pFrame, deltaFrame, msg );

	// send all unreliable temp ents between last and current frame
	CFrameSnapshot * fromSnapshot = deltaFrame?deltaFrame->GetSnapshot():NULL;
	sv.WriteTempEntities( m_pReplayServer->m_MasterClient, pFrame->GetSnapshot(), fromSnapshot, msg, 255 );

	// write sound data
	data = &pFrame->m_Messages[REPLAY_BUFFER_SOUNDS];
	if ( data->GetNumBitsWritten() )
		msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten()  );

	// write voice data
	data = &pFrame->m_Messages[REPLAY_BUFFER_VOICE];
	if ( data->GetNumBitsWritten() )
		msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten()  );

	// last write unreliable data
	data = &pFrame->m_Messages[REPLAY_BUFFER_UNRELIABLE];
	if ( data->GetNumBitsWritten() )
		msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten()  );

	// update delta tick just like fakeclients do
	m_nDeltaTick = pFrame->tick_count;

	// write packet to demo file
	m_DemoFile.NotifyBeginFrame();
		WriteMessages( dem_packet, msg ); 
	m_DemoFile.NotifyEndFrame();
}

void CReplayDemoRecorder::WriteMessages( unsigned char cmd, bf_write &message )
{
	int len = message.GetNumBytesWritten();

	if (len <= 0)
		return;

	// fill last bits in last byte with NOP if necessary
	int nRemainingBits = message.GetNumBitsWritten() % 8;
	if ( nRemainingBits > 0 &&  nRemainingBits <= (8-NETMSG_TYPE_BITS) )
	{
		message.WriteUBitLong( net_NOP, NETMSG_TYPE_BITS );
	}

	Assert( len < NET_MAX_MESSAGE );

	// if signondata read as fast as possible, no rewind
	// and wait for packet time
	// byte cmd = (m_pDemoFileHeader != NULL)  ? dem_signon : dem_packet;

	if ( cmd == dem_packet )
	{
		m_nFrameCount++;
	}

	// write command & time
	m_DemoFile.WriteCmdHeader( cmd, GetRecordingTick(), 0 ); 
	
	// write NULL democmdinfo just to keep same format as client demos
	democmdinfo_t info;
	Q_memset( &info, 0, sizeof( info ) );
	m_DemoFile.WriteCmdInfo( info );

	// write continously increasing sequence numbers
	m_DemoFile.WriteSequenceInfo( m_SequenceInfo, m_SequenceInfo );
	m_SequenceInfo++;
	
	// Output the buffer.  Skip the network packet stuff.
	m_DemoFile.WriteRawData( (char*)message.GetBasePointer(), len );
	
	if ( replay_debug.GetInt() > 1 )
	{
		Msg( "Writing Replay demo message %i bytes at file pos %i\n", len, m_DemoFile.GetCurPos( false ) );
	}
}

void CReplayDemoRecorder::RecordMessages(bf_read &data, int bits)
{
	// create buffer if not there yet
	if ( m_MessageData.GetBasePointer() == NULL )
	{
		m_MessageData.StartWriting( new unsigned char[NET_MAX_PAYLOAD], NET_MAX_PAYLOAD );
	}
	
	if ( bits>0 )
	{
		m_MessageData.WriteBitsFromBuffer( &data, bits );
		Assert( !m_MessageData.IsOverflowed() );
	}
}

void CReplayDemoRecorder::RecordPacket()
{
	Assert( !"Does this ever get called?  I can't find anywhere where it does." );
	if( m_MessageData.GetBasePointer() )
	{
		WriteMessages( dem_packet, m_MessageData );
		m_MessageData.Reset(); // clear message buffer
	}
}

#endif
