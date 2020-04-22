//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "client_pch.h"
#include "cl_demo.h"
#include "cl_broadcast.h"
#include "baseautocompletefilelist.h"
#include "demostreamhttp.h"
#include "dt_common_eng.h"
#include "matchmaking/imatchframework.h"

extern ConVar demo_debug;
extern CNetworkStringTableContainer *networkStringTableContainerClient;
extern bool IsControlCommand( unsigned char cmd );
extern ConVar tv_playcast_delay_prediction;
extern ConVar tv_playcast_max_rcvage;

CBroadcastPlayer s_ClientBroadcastPlayer;

CBroadcastPlayer::CBroadcastPlayer() :
	m_DemoStrider( NULL )
{
	m_bPlayingBack = false;
	m_flPlaybackRateModifier = 1.0f;
	m_nSkipToTick = -1;
	m_flAutoResumeTime = 0.0f;
	m_bPlaybackPaused = false;
	m_nPacketTick = 0;
	m_nStartHostTick = 0; 
	m_nStreamStartTick = 0;
	m_bInterpolateView = false;
	m_DemoStream.SetClient( this );
	m_nStreamState = STREAM_STOP;
	m_bPacketReadSuspended = false;
	m_dResyncTimerStart = 0;
	m_bIgnoreDemoStopCommand = false;
}

CBroadcastPlayer::~CBroadcastPlayer()
{
	if ( m_bPlayingBack )
	{
		StopPlayback();
		if ( g_ClientDLL )
		{
			g_ClientDLL->OnDemoPlaybackStop();
		}
	}
}


IDemoStream * CBroadcastPlayer::GetDemoStream()
{
	return &m_DemoStream;
}




void CBroadcastPlayer::StartStreaming( const char *url, const char *options )
{
	if ( !options )
		options = "";

	m_bSkipSync = ( NULL != V_strstr( options, "c" ) ); // playback akamai cached content (sync isn't cached)
	m_bIgnoreDemoStopCommand = ( NULL != V_strstr( options, "b" ) );
	int nPlayFromFragment = 0;
	if ( const char *pFrame = V_strstr( options, "f" ) )
		nPlayFromFragment = V_atoi( pFrame + 1 );
	else if ( NULL != V_strstr( options, "a" ) )
		nPlayFromFragment = 1;

	StopPlayback();
	if ( g_pClientDemoPlayer )
		g_pClientDemoPlayer->StopPlayback();
	demoplayer = this;

	if ( CDemoPlaybackParameters_t *pParams = const_cast< CDemoPlaybackParameters_t * >( GetDemoPlaybackParameters() ) )
	{
		// the format of the id is MatchId# where # is a one-digit GoTv instance index
		pParams->m_uiLiveMatchID = CDemoStreamHttp::GetStreamId( url ).m_nMatchId;
	}

	if ( StartStreamingInternal() )
	{
		g_ClientDLL->OnDemoPlaybackStart( url );
		if ( m_bSkipSync )
		{
			int nGuessedStartFragment = Max( 1, nPlayFromFragment );
			m_DemoStream.StartStreamingCached( url, nGuessedStartFragment );
		}
		else
		{
			if ( nPlayFromFragment > 0 )
				m_DemoStream.StartStreaming( url, CDemoStreamHttp::SyncParams_t( nPlayFromFragment ) );
			else
				m_DemoStream.StartStreaming( url );
		}
	}
}


bool CBroadcastPlayer::OnEngineGotvSyncPacket( const CEngineGotvSyncPacket *pPkt )
{
	return m_DemoStream.OnEngineGotvSyncPacket( pPkt );
}

bool CBroadcastPlayer::StartStreamingInternal()
{
	SCR_BeginLoadingPlaque();

	// Disconnect from server or stop running one
	int oldn = GetBaseLocalClient().demonum;
	GetBaseLocalClient().demonum = -1;
	Host_Disconnect( false );

	// set current demo player to client demo player
	// disconnect before loading demo, to avoid sometimes loading into game instead of demo
	GetBaseLocalClient().Disconnect( false );

	GetBaseLocalClient().demonum = oldn;
	GetBaseLocalClient().m_nSignonState = SIGNONSTATE_CONNECTED;
	ResyncDemoClock();
	// create a fake channel with a NULL address (no encryption keys in demos)
	GetBaseLocalClient().m_NetChannel = NET_CreateNetChannel( NS_CLIENT, NULL, "BROADCAST", &GetBaseLocalClient(), NULL, false );

	if ( !GetBaseLocalClient().m_NetChannel )
	{
		ConMsg( "Broadcast Player: failed to create demo net channel\n" );
		m_DemoStream.StopStreaming();
		GetBaseLocalClient().demonum = -1;		// stop demo loop
		Host_Disconnect( true );
		SCR_EndLoadingPlaque();
		return false;
	}
	GetBaseLocalClient().m_NetChannel->SetTimeout( -1.0f );	// never timeout
	m_bPlayingBack = true;
	m_bPlaybackPaused = false;
	m_flAutoResumeTime = 0.0f;
	m_flPlaybackRateModifier = 1.0f;
	m_bInterpolateView = false;
	m_nStartHostTick = -1;
	m_nStreamFragment = -1;
	m_nStreamStartTick = -1;

	V_memset( &m_DemoPacket, 0, sizeof( m_DemoPacket ) );

	// setup demo packet data buffer
	m_DemoPacket.data = NULL;
	m_DemoPacket.from.SetAddrType( NSAT_NETADR );
	m_DemoPacket.from.m_adr.SetType( NA_LOOPBACK );
	m_DemoStrider.Set( NULL );

	GetBaseLocalClient().chokedcommands = 0;
	GetBaseLocalClient().lastoutgoingcommand = -1;
	GetBaseLocalClient().m_flNextCmdTime = net_time;

	m_dResyncTimerStart = Plat_FloatTime();
	m_nStreamState = STREAM_SYNC;
	return true;
}

bool CBroadcastPlayer::OnDemoStreamRestarting()
{	
	return StartStreamingInternal();
}

void CBroadcastPlayer::ResyncStream()
{
	if ( Plat_FloatTime() - m_dResyncTimerStart > m_DemoStream.GetBroadcastKeyframeInterval() )
	{
		m_dResyncTimerStart = Plat_FloatTime();
		DevMsg( "Stream Re-sync @%d...\n", m_nStreamFragment );
		m_nStreamFragment++; // resync from the next fragment
		m_nStreamState = STREAM_SYNC;
		m_DemoStream.Resync( );
	}
}


void CBroadcastPlayer::OnDemoStreamStart( const DemoStreamReference_t &start, int nResync )
{
	m_dResyncTimerStart = Plat_FloatTime();
	m_nStreamState = nResync ? STREAM_FULLFRAME : STREAM_START;
	m_nStartHostTick = host_tickcount;
	m_nStreamStartTick = start.nTick;
	m_nStreamFragment = start.nFragment;
}



void CBroadcastPlayer::OnDemoStreamStop()
{
	if ( m_bPlayingBack )
	{
		m_bPlayingBack = false;
	}
	m_flAutoResumeTime = 0.0f;
	m_flPlaybackRateModifier = 1.0f;
	m_DemoPacket.data = NULL;
	m_DemoStrider.Set( NULL );
	m_nStreamState = STREAM_STOP;

	if ( g_pMatchFramework )
	{
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnDemoFileEndReached" ) );
	}

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetBaseLocalClient().Disconnect( true );
	}

	demoplayer = g_pClientDemoPlayer;

	g_ClientDLL->OnDemoPlaybackStop();
}


CON_COMMAND( playcast, "Play a broadcast" )
{
	if ( args.ArgC() < 2 )
	{
		ConMsg( "Usage: playcast <url>\n" );
		return;
	}

	if ( !net_time && !NET_IsMultiplayer() )
	{
		ConMsg( "Deferring playcast command!\n" );
		return;
	}

	s_ClientBroadcastPlayer.StartStreaming( args[ 1 ], args.ArgC() >= 3 ? args[2] : "" );
}


void CBroadcastPlayer::PausePlayback( float seconds )
{
	m_bPlaybackPaused = true;

	if ( seconds > 0.0f )
	{
		// Use true clock since everything else is frozen
		m_flAutoResumeTime = Sys_FloatTime() + seconds;
	}
	else
	{
		m_flAutoResumeTime = 0.0f;
	}
}

void CBroadcastPlayer::ResumePlayback()
{
	m_bPlaybackPaused = false;
	m_flAutoResumeTime = 0.0f;
}

void CBroadcastPlayer::StopPlayback()
{
	m_nStreamState = STREAM_STOP;
	m_DemoStream.StopStreaming();
	g_ClientDLL->OnDemoPlaybackStop();
}


bool CBroadcastPlayer::PreparePacket( void )
{
	Assert( !m_DemoStrider.Get() || ( m_DemoStrider.Get() >= m_DemoBuffer->Base() && m_DemoStrider.Get() <= m_DemoBuffer->End() ) );
	double dTime = Plat_FloatTime();
	if ( !m_DemoStrider.Get() || m_DemoStrider.Get() >= m_DemoBuffer->End() )
	{
		// get the next packet
		switch ( m_nStreamState )
		{
		case STREAM_START:
			if ( CDemoStreamHttp::Buffer_t *pBuffer = m_DemoStream.GetStreamSignupBuffer() )
			{
				SetDemoBuffer( pBuffer );
				m_nStreamState = STREAM_FULLFRAME;
				if ( tv_playcast_delay_prediction.GetBool() )
				{
					m_nStreamState = STREAM_MAP_LOADED;
				}
			}
			else
				return false; // not implemented yet: the signup buffer isn't precached
			break;

		case STREAM_MAP_LOADED:
			{// recompute the start frame
				DemoStreamReference_t startRef = m_DemoStream.GetStreamStartReference( true );
				if ( demo_debug.GetBool() )
					Msg( "playcast: Adjusting fragment %d->%d, tick %d->%d\n", m_nStreamFragment, startRef.nFragment, m_nStreamStartTick, startRef.nTick );

				m_nStartHostTick = host_tickcount - startRef.nSkipTicks;
				m_nStreamStartTick = startRef.nTick;
				m_nStreamFragment = startRef.nFragment;
				m_DemoStream.BeginBuffering( m_nStreamFragment );
				m_nStreamState = STREAM_WAITING_FOR_KEYFRAME;
				m_dResyncTimerStart = m_dDelayedPrecacheTimeStart = dTime;
				return false; // the data isn't ready yet
			}

			break;

		case STREAM_WAITING_FOR_KEYFRAME:
			
			if ( m_dDelayedPrecacheTimeStart + tv_playcast_max_rcvage.GetFloat() < dTime )
			{
				ResyncStream(); // try to resync when the resync timeout passes; maybe the full fragment is about to come in
				return false;
			}

			if ( CDemoStreamHttp::Buffer_t *pBuffer = m_DemoStream.GetFragmentBuffer( m_nStreamFragment, CDemoStreamHttp::FRAGMENT_FULL ) )
			{
				// is the delta precached?
				if ( m_DemoStream.GetFragmentBuffer( m_nStreamFragment, CDemoStreamHttp::FRAGMENT_DELTA ) )
				{// the data is ready
					if ( demo_debug.GetBool() )
						Msg( "playcast: Fragment %d keyframe and delta frames ready, delayed precache took %.2f sec\n", m_nStreamFragment, dTime - m_dDelayedPrecacheTimeStart );
					SetDemoBuffer( pBuffer );
					m_nStreamState = STREAM_FULLFRAME;
					return true;
				}
			}

			return false;

		case STREAM_FULLFRAME:
			m_DemoStream.ReleaseFragment( m_nStreamFragment - 1 );
			if ( CDemoStreamHttp::Buffer_t *pBuffer = m_DemoStream.GetFragmentBuffer( m_nStreamFragment, CDemoStreamHttp::FRAGMENT_FULL ) )
			{
				if ( demo_debug.GetBool() )
					Msg( "playcast: Play keyframe Fragment %d\n", m_nStreamFragment );
				SetDemoBuffer( pBuffer );
				m_nStreamState = STREAM_BEFORE_DELTAFRAMES;
			}
			else 
			{
				ResyncStream();
				return false; 
			}
			break;

		case STREAM_BEFORE_DELTAFRAMES:
			if ( g_ClientDLL )
				g_ClientDLL->OnDemoPlaybackTimeJump();
			m_nStreamState = STREAM_DELTAFRAMES;
			// fall through and start streaming deltaframes now

		case STREAM_DELTAFRAMES:
			m_DemoStream.ReleaseFragment( m_nStreamFragment - 1 );
			if ( CDemoStreamHttp::Buffer_t *pBuffer = m_DemoStream.GetFragmentBuffer( m_nStreamFragment, CDemoStreamHttp::FRAGMENT_DELTA ) )
			{
				if ( demo_debug.GetBool() )
					Msg( "playcast: Play delta Fragment %d\n", m_nStreamFragment );
				SetDemoBuffer( pBuffer );
				m_nStreamState = STREAM_DELTAFRAMES;
				m_nStreamFragment++; // TODO : count the ticks, not fragments
				for ( int i = 1; i <= 4; ++i )
					m_DemoStream.RequestFragment( m_nStreamFragment + i, CDemoStreamHttp::FRAGMENT_DELTA );
			}
			else
			{
				// we don't have the delta... we can request the full fragment and restart, or we could do a partial re-sync (we don't need to reload the level, hence the partial)
				ResyncStream();
				return false;
			}
			break;
		}
	}
	m_dResyncTimerStart = dTime;
	return true;
}


void CBroadcastPlayer::ReadCmdHeader( unsigned char& cmd, int& tick, int &nPlayerSlot )
{
	if ( !m_DemoStrider.Get() || m_DemoStrider.Get() + 6 > m_DemoBuffer->End() )
	{
		cmd = dem_stop; 
		tick = m_nPacketTick;
		nPlayerSlot = 0;
	}
	else
	{
		cmd = m_DemoStrider.StrideUnaligned<uint8>();
		tick = m_DemoStrider.StrideUnaligned<int32>();
		nPlayerSlot = m_DemoStrider.StrideUnaligned<uint8>();

		if ( demo_debug.GetInt() > 2 )
		{
			if ( cmd == dem_packet )
				DevMsg( "playcast: Packet tick %d size %u\n", tick, *( uint32* )m_DemoStrider.Get() );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Read in next demo message and send to local client over network channel, if it's time.
// Output : bool 
//-----------------------------------------------------------------------------
netpacket_t *CBroadcastPlayer::ReadPacket( void )
{
	int			tick = 0;
	byte		cmd = dem_signon;
	uint8* curpos = NULL;

	m_DemoStream.Update();
	if ( m_DemoStream.IsIdle() )
	{
		m_bPlayingBack = false;
		Host_EndGame( true, "Tried to read a demo message with no demo file\n" );
		return NULL;
	}

	// dropped: timedemo

	// If game is still shutting down, then don't read any demo messages from file quite yet
	if ( HostState_IsGameShuttingDown() )
	{
		return NULL;
	}

	Assert( IsPlayingBack() );

	// External editor has paused playback
	if ( CheckPausedPlayback() )
		return NULL;

	if ( m_nStreamState <= STREAM_SYNC )
		return NULL; // waiting for data

	// dropped: highlights

	bool bStopReading = false;

	while ( !bStopReading )
	{
		if ( !PreparePacket() )
			return NULL; // packet is not ready

		curpos = m_DemoStrider.Get();

		int nPlayerSlot = 0;
		ReadCmdHeader( cmd, tick, nPlayerSlot );
		tick -= m_nStreamStartTick;

		m_nPacketTick = tick;

		// always read control commands 
		if ( !IsControlCommand( cmd ) )
		{
			int playbacktick = GetPlaybackTick();

			if ( GetBaseLocalClient().IsActive() &&
					( tick > playbacktick ) && !IsSkipping() )
			{
				// is not time yet
				bStopReading = true;
			}

			if ( bStopReading )
			{
				demoaction->Update( false, playbacktick, TICKS_TO_TIME( playbacktick ) );
				m_DemoStrider.Set( curpos ); // go back to start of current demo command
				return NULL;   // Not time yet, dont return packet data.
			}
		}

		// COMMAND HANDLERS
		switch ( cmd )
		{
		case dem_synctick: // currently not used, but may need to rethink it in the future
			ResyncDemoClock();
			break;
		
		case dem_stop:
			if ( !m_bIgnoreDemoStopCommand )
			{
				if ( demo_debug.GetBool() )
				{
					Msg( "playcast: %d dem_stop\n", tick );
				}

				if ( g_pMatchFramework )
				{
					g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnDemoFileEndReached" ) );
				}

				FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
				{
					ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
					GetBaseLocalClient().Disconnect( true );
				}
				return NULL;
			}
			break;

		case dem_consolecmd: // currently not used, not tested
			{
				ACTIVE_SPLITSCREEN_PLAYER_GUARD( nPlayerSlot );
				
				if ( const char *command = m_DemoStrider.StrideString( m_DemoBuffer->End() ) )
				{
					if ( demo_debug.GetBool() )
					{
						Msg( "playcast: %d dem_consolecmd [%s]\n", tick, command );
					}

					Cbuf_AddText( Cbuf_GetCurrentPlayer(), command, kCommandSrcDemoFile );
					Cbuf_Execute();
				}
			}
			break;
		case dem_datatables:
			{
				if ( demo_debug.GetBool() )
				{
					Msg( "playcast: %d dem_datatables\n", tick );
				}
				// support for older engine demos
				bf_read buf( m_DemoStrider.Get(), GetReminingStrideLength() );
				if ( !DataTable_LoadDataTablesFromBuffer( &buf, m_DemoStream.GetDemoProtocol() ) )
				{
					Host_Error( "Error parsing network data tables during demo playback." );
				}
				m_DemoStrider.Stride<uint8>( buf.GetNumBytesRead() );
			}
			break;
		case dem_stringtables:
			{
				bf_read buf( m_DemoStrider.Get(), GetReminingStrideLength() );
				if ( !networkStringTableContainerClient->ReadStringTables( buf ) )
				{
					Host_Error( "Error parsing string tables during demo playback." );
				}
				m_DemoStrider.Stride<uint8>( buf.GetNumBytesRead() );
			}
			break;
		case dem_usercmd:
			{
				Assert( !"Not used, Not tested - probably doesn't work correctly!" );
				ACTIVE_SPLITSCREEN_PLAYER_GUARD( nPlayerSlot );
				if ( demo_debug.GetBool() )
				{
					Msg( "playcast: %d dem_usercmd\n", tick );
				}
				int nCmdNumber = m_DemoStrider.StrideUnaligned<int32>(), nUserCmdLength = m_DemoStrider.StrideUnaligned<int32>();
				if ( m_DemoStrider.Get() + nUserCmdLength > m_DemoBuffer->End() )
				{
					Warning( "Invalid user cmd length %d\n", nUserCmdLength );
					m_DemoStrider.Set( m_DemoBuffer->End() ); // invalid user cmd length
				}
				else
				{
					bf_read msg( "ReadUserCmd", m_DemoStrider.Stride<uint8>( nUserCmdLength ), nUserCmdLength );
					g_ClientDLL->DecodeUserCmdFromBuffer( nPlayerSlot, msg, nCmdNumber );

					// Note, we need to have the current outgoing sequence correct so we can do prediction
					//  correctly during playback
					GetBaseLocalClient().lastoutgoingcommand = nCmdNumber;
				}
			}
			break;
		case dem_customdata:
			{
				int iCallbackIndex = m_DemoStrider.StrideUnaligned<int32>(), nSize = m_DemoStrider.StrideUnaligned<int32>();
				m_DemoStrider.Stride<uint8>( nSize );
				Warning( "Unable to decode custom demo data %d bytes, callback %d not supported.\n", nSize, iCallbackIndex );
			}
			break;
		default:
			{
				bStopReading = true;

				if ( IsSkipping() )
				{
					// adjust playback host_tickcount when skipping
					m_nStartHostTick = host_tickcount - tick;
				}
			}
			break;
		}
	}

	if ( cmd == dem_packet )
	{
		// remember last frame we read a dem_packet update
		//m_nTimeDemoCurrentFrame = host_framecount;
	}

	//int inseq, outseqack, outseq = 0;

	// we're skipping cmd info in this protocol: see CHLTVBroadcast::WriteMessages, it doesn't write seq

	//GetBaseLocalClient().m_NetChannel->SetSequenceData( outseq, inseq, outseqack );

	int length = m_DemoStrider.StrideUnaligned< int32 >();
	if ( length < 0 || uint( length ) > GetReminingStrideLength() )
	{
		Warning( "Invalid broadcast packet size %d in fragment buffer size %d\n", length, m_DemoBuffer->m_nSize );
		StrideDemoPacket();
		return NULL;
	}
	else
	{
		StrideDemoPacket( length );

		if ( demo_debug.GetInt() > 2 )
		{
			Msg( "playcast: %d network packet [%d]\n", tick, length );
		}

		if ( length > 0 )
		{
			m_DemoPacket.received = realtime;

			if ( demo_debug.GetInt() > 2 )
			{
				Msg( "playcast: Demo message, tick %i, %i bytes\n", GetPlaybackTick(), length );
			}
		}

		// Try and jump ahead one frame
		m_bInterpolateView = true; //ParseAheadForInterval( tick, 8 ); TODO: NOT IMPLEMENTED, camera transitions will be jerky

		// ConMsg( "Reading message for %i : %f skip %i\n", m_nFrameCount, fElapsedTime, forceskip ? 1 : 0 );

		return &m_DemoPacket;
	}
}

void CBroadcastPlayer::SetDemoBuffer( CDemoStreamHttp::Buffer_t * pBuffer )
{
	m_DemoStrider.Set( pBuffer->Base() );
	m_DemoBuffer = pBuffer;
	m_DemoPacket.received = realtime;

	m_DemoPacket.data = NULL;
	m_DemoPacket.size = 0;
	m_DemoPacket.message.StartReading( NULL, 0 ); // message must be set up later
}

void CBroadcastPlayer::StrideDemoPacket( int nLength )
{
	m_DemoPacket.data = m_DemoStrider.Stride<unsigned char>( nLength );
	m_DemoPacket.size = nLength;
	m_DemoPacket.message.StartReading( m_DemoPacket.data, m_DemoPacket.size );
}

void CBroadcastPlayer::StrideDemoPacket( )
{
	StrideDemoPacket( GetReminingStrideLength() );
}

uint CBroadcastPlayer::GetReminingStrideLength()
{
	return m_DemoBuffer->End() - m_DemoStrider.Get();
}

CDemoPlaybackParameters_t Helper_GetBroadcastPlayerDemoPlaybackParameters()
{
	CDemoPlaybackParameters_t params = {};
	params.m_bPlayingLiveRemoteBroadcast = true;
	params.m_numRoundStop = 999;
	return params;
}

CDemoPlaybackParameters_t const * CBroadcastPlayer::GetDemoPlaybackParameters()
{
	static CDemoPlaybackParameters_t s_params = Helper_GetBroadcastPlayerDemoPlaybackParameters();
	return &s_params;
}

void CBroadcastPlayer::SetPacketReadSuspended( bool bSuspendPacketReading )
{
	if ( m_bPacketReadSuspended == bSuspendPacketReading )
		return; // same state

	m_bPacketReadSuspended = bSuspendPacketReading;
	if ( !m_bPacketReadSuspended )
		ResyncDemoClock(); // Make sure we resync demo clock when we resume packet reading
}

int CBroadcastPlayer::GetPlaybackStartTick( void )
{
	return m_nStartHostTick;
}

int CBroadcastPlayer::GetPlaybackTick( void )
{
	return host_tickcount - m_nStartHostTick;
}

int CBroadcastPlayer::GetPlaybackDeltaTick( void )
{
	return host_tickcount - m_nStartHostTick;
}

int CBroadcastPlayer::GetPacketTick()
{
	return m_nPacketTick;
}


void CBroadcastPlayer::ResyncDemoClock()
{
	m_nStartHostTick = host_tickcount;
	m_nPreviousTick = m_nStartHostTick;
}

bool CBroadcastPlayer::CheckPausedPlayback( void )
{
	if ( m_bPacketReadSuspended )
		return true; // When packet reading is suspended it trumps all other states

	return m_bPlaybackPaused;
}

float CBroadcastPlayer::GetPlaybackTimeScale()
{
	return m_flPlaybackRateModifier;
}

void CBroadcastPlayer::SetPlaybackTimeScale( float timescale )
{
	m_flPlaybackRateModifier = timescale;
}

bool CBroadcastPlayer::IsPlaybackPaused( void ) const
{
	return m_bPlayingBack && m_bPlaybackPaused;
}

