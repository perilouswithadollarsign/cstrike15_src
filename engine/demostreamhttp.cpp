//========= Copyright (c), Valve Corporation, All rights reserved. ============//

#include "client_pch.h"
#include "demostreamhttp.h"
#include "cl_steamauth.h"
#include "tier1/keyvaluesjson.h"
#include "tier0/memalloc.h"
#include "cl_demo.h"
#include "sv_steamauth.h"
#include "engine_gcmessages.pb.h"


static ISteamHTTP *s_pSteamHTTP = NULL;
ConVar demo_debug( "demo_debug", "0", 0, "Demo debug info." );
ConVar tv_playcast_origin_auth( "tv_playcast_origin_auth", "", FCVAR_RELEASE | FCVAR_HIDDEN, "Get request X-Origin-Auth string" );
ConVar tv_playcast_max_rcvage( "tv_playcast_max_rcvage", "15", FCVAR_RELEASE | FCVAR_HIDDEN );
ConVar tv_playcast_max_rtdelay( "tv_playcast_max_rtdelay", "55", FCVAR_RELEASE | FCVAR_HIDDEN );
ConVar tv_playcast_delay_prediction( "tv_playcast_delay_prediction", "1", FCVAR_RELEASE );

CDemoStreamHttp::CDemoStreamHttp() :
	m_nState( STATE_IDLE ),
	m_pStreamSignup( NULL ),
	m_pClient( NULL ),
	m_bSyncFromGc( false ),
	m_flBroadcastKeyframeInterval( 3 )
{
	V_memset( &m_SyncResponse, 0, sizeof( m_SyncResponse ) );
	m_dSyncTimeoutEnd = -1;
}

void CDemoStreamHttp::StartStreaming( const char *pUrl, SyncParams_t syncParams )
{
	if ( !PrepareForStreaming( pUrl ) )
		return;
	m_SyncParams = syncParams;
	SendSync( );
}

bool CDemoStreamHttp::PrepareForStreaming( const char * pUrl )
{
	StopStreaming();

	if ( pUrl[ 0 ] == 'g' && pUrl[ 1 ] == 'c' && pUrl[ 2 ] == '-' )
	{
		m_Url = pUrl + 3;
		m_bSyncFromGc = true;
	}
	else
	{
		m_Url = pUrl;
		m_bSyncFromGc = false;
	}

	m_Url.StripTrailingSlash();

#ifndef DEDICATED
	s_pSteamHTTP = Steam3Client().SteamHTTP();
#endif

	if ( !s_pSteamHTTP )
		s_pSteamHTTP = Steam3Server().SteamHTTP();


	if ( !s_pSteamHTTP )
	{
		DevMsg( "Cannot get Steam HTTP interface\n" );
		return false ;
	}

	DevMsg( "Broadcast: Synchronizing stream\n" );
	m_nState = STATE_SYNC;
	return true;
}

// this is only called in special cases for debugging, to play back stale contents
void CDemoStreamHttp::StartStreamingCached( const char *pUrl, int nFragment)
{
	if ( !PrepareForStreaming( pUrl ) )
		return;

	m_nState = STATE_START;
	// guess parameters of the stream
	int nStartTick = 1;
	m_nDemoProtocol = 4; // DEMO_PROTOCOL == 4 is where I started writing this
	int nSignupFragment = 0;

	m_SyncParams.m_nStartFragment = 0;
	m_SyncResponse.flTicksPerSecond = 128;
	m_SyncResponse.flKeyframeInterval = 3;
	m_SyncResponse.flRealTimeDelay = 0;
	m_SyncResponse.flReceiveAge = 0;
	m_SyncResponse.nFragment = nFragment;
	m_SyncResponse.nSignupFragment = nSignupFragment;
	m_SyncResponse.nStartTick = nStartTick;
	m_SyncResponse.dPlatTimeReceived = Plat_FloatTime();

	SendGet( CFmtStr( "/%d/start", nSignupFragment ), new CStartRequest( ) );
	BeginBuffering( nFragment );
}

void CDemoStreamHttp::SendSync( int nResync )
{
	Assert( m_nState == STATE_SYNC );
#ifndef DEDICATED
	if ( m_bSyncFromGc )
	{
		GotvHttpStreamId_t params = GetStreamId( m_Url );
		DevMsg( "Requesting sync from GC, start fragment %d match id %llu instance %d\n", m_SyncParams.m_nStartFragment, params.m_nMatchId, params.m_nInstanceId );
		CEngineGotvSyncPacket msg;
		msg.set_match_id( params.m_nMatchId );
		msg.set_instance_id( params.m_nInstanceId );
		if ( m_SyncParams.m_nStartFragment > 0 )
		{
			msg.set_currentfragment( m_SyncParams.m_nStartFragment );
		}
		g_ClientDLL->EngineGotvSyncPacket( &msg );
		m_dSyncTimeoutEnd = Plat_FloatTime() + 10;
	}
	else
#endif
	{
		char request[ 128 ];
		m_SyncParams.PrintSyncRequest( request, sizeof( request ) );
		DevMsg( "Requesting sync from relay %s\n", request );
		SendGet( request, new CSyncRequest( m_SyncParams, nResync ) );
	}
}

void CDemoStreamHttp::Resync( )
{
	m_nState = STATE_SYNC;
	SendSync( 1 );
}

void CDemoStreamHttp::Update()
{
	if ( m_nState == STATE_SYNC && m_bSyncFromGc && m_dSyncTimeoutEnd > 0 && m_dSyncTimeoutEnd < Plat_FloatTime() )
	{
		StopStreaming();
	}

	if ( m_nState == STATE_RANDOM_WAIT_AND_SYNC && m_bSyncFromGc && m_dSyncTimeoutEnd > 0 && m_dSyncTimeoutEnd < Plat_FloatTime() )
	{
		m_nState = STATE_SYNC;
		SendSync( 0 );
	}
}

// result from "/start" request
void CDemoStreamHttp::OnStart( HTTPRequestHandle hRequest )
{
	m_pStreamSignup = MakeBuffer( hRequest );
	m_nStreamSignupFragment = m_SyncResponse.nSignupFragment;
	if ( !m_pStreamSignup )
	{
		DevMsg( "Broadcast failed to start: cannot retrieve startup packet data\n" );
		StopStreaming();
		return;
	}

	DevMsg( "Received signup fragment %d\n", m_SyncResponse.nSignupFragment );
	if ( m_pClient )
	{
		m_pClient->OnDemoStreamStart( GetStreamStartReference(), 0 );
	}
}

void CDemoStreamHttp::OnFragmentRequestSuccess( HTTPRequestHandle hRequest, int nFragment, FragmentTypeEnum_t nType )
{
	Fragment_t &fragment = Fragment( nFragment );
	fragment.ClearStreaming( nType );
	if ( Buffer_t *pBuf = MakeBuffer( hRequest ) )
	{
		fragment.SetField( nType, pBuf );
	}
	else
	{
		DevMsg( "Broadcast playback failed to retrieve %s frame of fragment %d", AsString( nType ), nFragment ); // TODO: implement fault-tolerant recovery; we can request the next fragment's full frame
		StopStreaming();
	}
}

void CDemoStreamHttp::OnFragmentRequestFailure( EHTTPStatusCode nErrorCode, int nFragment, FragmentTypeEnum_t nType )
{
	Fragment_t &fragment = Fragment( nFragment );
	fragment.ClearStreaming( nType );
	// TODO: Retry streaming gracefully, implement timeouts, skip fragment and download the next full frame if needed
}

bool CDemoStreamHttp::OnEngineGotvSyncPacket( const CEngineGotvSyncPacket *pPkt )
{
	GotvHttpStreamId_t streamId = GetStreamId( m_Url );

	if ( streamId.m_nMatchId != pPkt->match_id() || streamId.m_nInstanceId != pPkt->instance_id() )
	{
		Warning( "Ignoring unexpected sync from gc, match %llu:%d, expected %llu:%d\n", pPkt->match_id(), pPkt->instance_id(), streamId.m_nMatchId, streamId.m_nInstanceId );
		return false;
	}

	if ( m_nState != STATE_SYNC && m_nState != STATE_RANDOM_WAIT_AND_SYNC ) // we should be waiting for a sync in some way. In case of WAIT_AND_SYNC, maybe GC will send us a packet even though we didn't ask for it, while we're waiting to re-ask for a sync..
	{
		Warning( "Ignoring unexpected sync from gc, match %llu:%d\n", pPkt->match_id(), pPkt->instance_id() );
		return false;
	}

	if ( !pPkt->has_tick() )
	{
		// the packet is empty, which means: wait for a few seconds
		m_nState = STATE_RANDOM_WAIT_AND_SYNC;
		float flDelay = pPkt->rtdelay() * RandomFloat( 0.5f, 1.5f );
		m_dSyncTimeoutEnd = Plat_FloatTime() + flDelay;
		DevMsg( "Waiting %.2f seconds\n", flDelay );
		return true; // we actually successfully processed the packet
	}

	m_nDemoProtocol = 4;
	m_SyncResponse.flTicksPerSecond = pPkt->tickrate();
	m_SyncResponse.flKeyframeInterval = pPkt->has_keyframe_interval() ? pPkt->keyframe_interval() : 3.0f;
	m_SyncResponse.nStartTick = pPkt->tick();
	m_SyncResponse.flRealTimeDelay = pPkt->rtdelay();
	m_SyncResponse.flReceiveAge = pPkt->rcvage();
	m_SyncResponse.nFragment = pPkt->currentfragment();
	m_SyncResponse.nSignupFragment = pPkt->signupfragment();
	m_SyncResponse.dPlatTimeReceived = Plat_FloatTime();

	return OnSync( 0 );
}



// result from "/sync" request arrived
bool CDemoStreamHttp::OnSync( const char *pBuffer, int nBufferSize, int nResync )
{
	if ( m_nState != STATE_SYNC )
	{
		Warning( "Ignoring unexpected sync, %d bytes, resync %d\n", nBufferSize, nResync );
		return false;
	}
	KeyValuesJSONParser json( pBuffer, nBufferSize );
	if ( KeyValues *pSync = json.ParseFile() )
	{
		m_SyncResponse.flKeyframeInterval = pSync->GetFloat( "keyframe_interval", 3.0f );
		m_SyncResponse.nStartTick = pSync->GetInt( "tick", -1 );
		m_SyncResponse.flRealTimeDelay = pSync->GetFloat( "rtdelay", 0 );
		m_SyncResponse.flReceiveAge = pSync->GetFloat( "rcvage", 0 );
		m_SyncResponse.nFragment = pSync->GetInt( "fragment", 1 );
		m_SyncResponse.nSignupFragment = pSync->GetInt( "signup_fragment", 0 );
		m_SyncResponse.flTicksPerSecond = pSync->GetInt( "tps", 0 );
		m_SyncResponse.dPlatTimeReceived = Plat_FloatTime();
		m_nDemoProtocol = pSync->GetInt( "protocol", 4 ); // DEMO_PROTOCOL == 4 is where I started writing this
		
		delete pSync; pSync = NULL;

		return OnSync( nResync );
	}
	else
	{
		DevMsg( "Broadcast sync: malformed response: %s\n", pBuffer );
		StopStreaming();
		return false;
	}
}


bool CDemoStreamHttp::OnSync( int nResync )
{
	if ( nResync && !IsDebug() && m_SyncResponse.flReceiveAge > tv_playcast_max_rcvage.GetFloat() && m_SyncResponse.flRealTimeDelay > tv_playcast_max_rtdelay.GetFloat() )
	{
		DevMsg( "Broadcast resync %d: the stream seems to have stopped (rcvage %.1f, rtdelay %.1f)\n", nResync, m_SyncResponse.flReceiveAge, m_SyncResponse.flRealTimeDelay );
		StopStreaming();
		return false;
	}
	else if ( /*nTick < 0 || nEndTick < nTick || flSkip < 0 ||*/ m_SyncResponse.nFragment < m_SyncResponse.nSignupFragment || m_SyncResponse.nSignupFragment < 0 )
	{
		DevMsg( "Broadcast m_SyncResponse: unexpected response. fragment %d must be at/after start fragment %d\n", m_SyncResponse.nFragment, m_SyncResponse.nSignupFragment );
		StopStreaming();
		return false;
	}
	else
	{
		DevMsg( "Broadcast: Buffering stream tick %d fragment %d signup fragment %d\n", m_SyncResponse.nStartTick, m_SyncResponse.nSignupFragment, m_SyncResponse.nSignupFragment );
		m_nState = STATE_START;
		m_dSyncTimeoutEnd = -1;
		m_flBroadcastKeyframeInterval = m_SyncResponse.flKeyframeInterval;
		if ( nResync )
		{
			if ( !m_pClient )
			{
				DevMsg( "Broadcast resync failed: Client not connected to Stream\n" );
				StopStreaming();
				return false;
			}
			if ( m_SyncResponse.nSignupFragment == m_nStreamSignupFragment )
			{
				Assert( m_pStreamSignup.IsValid() && m_pClient );
				m_pClient->OnDemoStreamStart( GetStreamStartReference(), nResync );
			}
			else
			{
				if ( !m_pClient->OnDemoStreamRestarting() )
				{
					StopStreaming();
					return false;
				}
				DevMsg( "Resync %d response requires full stream restart because signup fragment changed from %d to %d\n", nResync, m_nStreamSignupFragment, m_SyncResponse.nSignupFragment );
				SendGet( CFmtStr( "/%d/start", m_SyncResponse.nSignupFragment ), new CStartRequest( ) );
			}
		}
		else
		{
			Assert( !m_pStreamSignup ); // when we're restarting, we don't need the start fragment, we already initialized
			SendGet( CFmtStr( "/%d/start", m_SyncResponse.nSignupFragment ), new CStartRequest( ) );
		}
		if ( nResync || !tv_playcast_delay_prediction.GetBool() )
		{
			int nFragment = m_SyncResponse.nFragment;
			BeginBuffering( nFragment );
		}
		return true;
	}
}


void CDemoStreamHttp::BeginBuffering( int nFragment )
{
	RequestFragment( nFragment, FRAGMENT_FULL );
	for ( int i = 0; i <= 4; ++i )
		RequestFragment( nFragment + i, FRAGMENT_DELTA );
}

void CDemoStreamHttp::RequestFragment( int nFragment, FragmentTypeEnum_t nType )
{
	Fragment_t &fragment = Fragment( nFragment );
	if ( !fragment.GetField( nType ) && !fragment.IsStreaming(nType) )
	{
		fragment.SetStreaming( nType );
		SendGet( CFmtStr( nType == FRAGMENT_FULL ? "/%d/full" : "/%d/delta", nFragment ), new CFragmentRequest( nFragment, nType ) );
	}
}

void CDemoStreamHttp::ReleaseFragment( int nFragment )
{
	UtlHashHandle_t it = m_FragmentCache.Find( nFragment );
	if ( it != m_FragmentCache.InvalidHandle() )
	{
		m_FragmentCache[ it ].ResetBuffers();
		m_FragmentCache.RemoveByHandle( it );
	}
}

GotvHttpStreamId_t CDemoStreamHttp::GetStreamId( const char *pUrl )
{
	GotvHttpStreamId_t out;
	if ( !pUrl || !*pUrl )
		return out;
	const char *p = pUrl + V_strlen( pUrl ) - 1;
	if ( !V_isdigit( *p ) )
		return out;
	out.m_nInstanceId = *p - '0';

	p--;
	if ( *p != 'i' )
		return out;

	out.m_nMatchId = 0;
	uint64 digitPlace = 1;
	while ( ( --p ) >= pUrl && V_isdigit( *p ) )
	{
		if ( out.m_nMatchId > uint64( -1ll ) / 10 )
			break; // the number doesn't fit 64 bit, error out
		out.m_nMatchId += ( *p - '0' ) * digitPlace;
		digitPlace *= 10;
	}
	if ( *p != '/' )
	{
		// invalid matchid
		out.m_nMatchId = 0;
	}
	return out;
}

IDemoStreamClient::DemoStreamReference_t CDemoStreamHttp::GetStreamStartReference( bool bLagCompensation /*= false */ )
{
	IDemoStreamClient::DemoStreamReference_t start;
	start.nTick = m_SyncResponse.nStartTick;
	start.nFragment = m_SyncResponse.nFragment;

	if ( bLagCompensation )
	{
		float flSkipSeconds = ( Plat_FloatTime() - m_SyncResponse.dPlatTimeReceived + m_SyncResponse.flReceiveAge );
		if ( flSkipSeconds >= 0 && flSkipSeconds < 90 ) // if it's not  too suspiciously long interval, we can try to compensate for it
		{
			int nTotalSkipTicks = int( flSkipSeconds * m_SyncResponse.flTicksPerSecond );
			int nTicksPerFragment = int( m_SyncResponse.flKeyframeInterval * m_SyncResponse.flTicksPerSecond );
			start.nSkipTicks = nTotalSkipTicks % nTicksPerFragment;
			start.nTick += ( nTotalSkipTicks - start.nSkipTicks );
			start.nFragment += nTotalSkipTicks / nTicksPerFragment;
		}
		else
			start.nSkipTicks = 0;
	}
	else
	{
		start.nSkipTicks = int( m_SyncResponse.flReceiveAge * m_SyncResponse.flTicksPerSecond ); // Maybe GC should send rtdelay - desired_delay instead?
	}

	return start;
}

void CDemoStreamHttp::StopStreaming()
{
	if ( m_nState != STATE_IDLE )
	{
		m_nState = STATE_IDLE;
		if ( m_pClient )
		{
			m_pClient->OnDemoStreamStop();
		}
	}

	m_dSyncTimeoutEnd = -1;

	while ( m_PendingRequests.Count() )
		m_PendingRequests.Tail()->Cancel();
	
	m_pStreamSignup = NULL; // delete start Buffer_t
	FOR_EACH_HASHTABLE( m_FragmentCache, it )
	{
		m_FragmentCache.Element( it ).ResetBuffers();
	}
	m_FragmentCache.Purge();
}

CDemoStreamHttp::Buffer_t * CDemoStreamHttp::GetFragmentBuffer( int nFragment , FragmentTypeEnum_t nFragmentType )
{
	UtlHashHandle_t hFind = m_FragmentCache.Find( nFragment );
	if ( hFind == m_FragmentCache.InvalidHandle() )
		return NULL;
	return m_FragmentCache[ hFind ].GetField( nFragmentType );
}

void CDemoStreamHttp::CSyncRequest::OnSuccess( const HTTPRequestCompleted_t * pResponse )
{
	uint32 nBodySize;
	if( !s_pSteamHTTP->GetHTTPResponseBodySize( pResponse->m_hRequest, &nBodySize ) || nBodySize >= 1024 )
	{
		DevMsg( "Broadcast sync: response buffer overflow (%d bytes)\n", nBodySize );
		return;
	};

	char *pResponseBuffer = StackAlloc( char, nBodySize + 1 );
	if ( !s_pSteamHTTP->GetHTTPResponseBodyData( pResponse->m_hRequest, ( uint8* )pResponseBuffer, nBodySize ) )
	{
		DevMsg( "Broadcast sync: cannot read response body\n" );
		return;
	}
	pResponseBuffer[ nBodySize ] = '\0';
	m_pParent->OnSync( pResponseBuffer, nBodySize, m_nResync );
}


void CDemoStreamHttp::CSyncRequest::OnFailure( const HTTPRequestCompleted_t * pResponse )
{
	if ( !m_nResync || m_nResync > 5 )
	{
		CPendingRequest::OnFailure( pResponse );
	}
	else
	{
		DevMsg( "%d stream resync failed\n", m_nResync );
		m_pParent->SendSync( m_nResync + 1 ); // retry a couple times
	}
}

void CDemoStreamHttp::CPendingRequest::OnFailure( const HTTPRequestCompleted_t * pResponse )
{
	if ( !pResponse )
	{
		DevMsg( "Broadcast IO error. Please try again later.\n" );
	}
	else
	{
		DevMsg( "Broadcast Streaming error %d\n", pResponse->m_eStatusCode );
	}
	m_pParent->StopStreaming();
}

void CDemoStreamHttp::CStartRequest::OnSuccess( const HTTPRequestCompleted_t * pResponse )
{
	m_pParent->OnStart( pResponse->m_hRequest );
}



CDemoStreamHttp::Buffer_t * CDemoStreamHttp::MakeBuffer( HTTPRequestHandle hRequest )
{
	uint32 nBodySize;
	if ( !s_pSteamHTTP->GetHTTPResponseBodySize( hRequest, &nBodySize ) )
	{
		return NULL;
	}

	uint8 *pMemory = new uint8[ sizeof( Buffer_t ) + nBodySize + 1 ];
	if ( !s_pSteamHTTP->GetHTTPResponseBodyData( hRequest, pMemory + sizeof( Buffer_t ), nBodySize ) )
	{
		delete[] pMemory;
		return NULL;
	}
	pMemory[ sizeof( Buffer_t ) + nBodySize ] = '\0'; // in case we need to receive and parse some text-only packets in the future
	Buffer_t* pBuffer = ( Buffer_t* )pMemory;
	pBuffer->m_nRefCount = 0;
	pBuffer->m_nSize = nBodySize;
	return pBuffer;
}


void CDemoStreamHttp::SendGet( const char *pPath, CPendingRequest *pRequest )
{
	HTTPRequestHandle hRequest = s_pSteamHTTP->CreateHTTPRequest( k_EHTTPMethodGET, m_Url + pPath );
	s_pSteamHTTP->SetHTTPRequestNetworkActivityTimeout( hRequest, 30 );
	const char *pOriginAuth = tv_playcast_origin_auth.GetString();
	if ( pOriginAuth && *pOriginAuth )
	{
		if ( !s_pSteamHTTP->SetHTTPRequestHeaderValue( hRequest, "X-Origin-Auth", pOriginAuth ) )
		{
			Warning( "Cannot set http X-Origin-Auth for %s\n", pPath );
		}
	}
	SteamAPICall_t hCall;
	bool bSentOk = s_pSteamHTTP->SendHTTPRequest( hRequest, &hCall );
	pRequest->Init( this, hRequest, hCall );
	if ( bSentOk && hCall )
	{
		SteamAPI_RegisterCallResult( pRequest, hCall );
	}
	else
	{
		s_pSteamHTTP->ReleaseHTTPRequest( hRequest );
		DevMsg( "Broadcast streaming: unexpected failure getting %s\n", pPath );
		pRequest->OnFailure( NULL ); // IO failure
	}
}

CDemoStreamHttp::Fragment_t & CDemoStreamHttp::Fragment( int nFragment )
{
	UtlHashHandle_t it = m_FragmentCache.Insert( nFragment );
	return m_FragmentCache[ it ];
}


CDemoStreamHttp::CPendingRequest::CPendingRequest() :
	m_pParent( NULL ),
	m_hRequest( INVALID_HTTPREQUEST_HANDLE ),
	m_hCall( k_uAPICallInvalid )
{
	m_iCallback = HTTPRequestCompleted_t::k_iCallback;
}


void CDemoStreamHttp::CPendingRequest::Init( CDemoStreamHttp *pParent, HTTPRequestHandle hRequest, SteamAPICall_t hCall )
{
	m_pParent = pParent;
	m_hRequest = hRequest;
	m_hCall = hCall;
	pParent->m_PendingRequests.AddToTail( this );
}

CDemoStreamHttp::CPendingRequest::~CPendingRequest()
{
}

void CDemoStreamHttp::CPendingRequest::Run( void *pvParam )
{
	m_pParent->m_PendingRequests.FindAndFastRemove( this );
	OnSuccess( ( HTTPRequestCompleted_t * )pvParam );
	delete this;
}

void CDemoStreamHttp::CPendingRequest::Run( void *pvParam, bool bIOFailure, SteamAPICall_t hSteamAPICall )
{
	m_pParent->m_PendingRequests.FindAndFastRemove( this );
	if ( bIOFailure )
	{
		OnFailure( NULL );
	}
	else
	{
		EHTTPStatusCode nStatus = ( ( HTTPRequestCompleted_t * )pvParam )->m_eStatusCode;
		Assert( ( ( HTTPRequestCompleted_t * )pvParam )->m_hRequest == m_hRequest );
		if ( nStatus != k_EHTTPStatusCode200OK ) // we should always get a 200
		{
			OnFailure( ( HTTPRequestCompleted_t * )pvParam );
		}
		else
		{
			OnSuccess( ( HTTPRequestCompleted_t * )pvParam );
		}
	}
	delete this;
}

void CDemoStreamHttp::CPendingRequest::Cancel()
{
	SteamAPI_UnregisterCallResult( this, m_hCall );
	s_pSteamHTTP->ReleaseHTTPRequest( m_hRequest );
	m_pParent->m_PendingRequests.FindAndFastRemove( this );
	OnFailure( NULL );
	delete this;
}

void CDemoStreamHttp::CFragmentRequest::OnSuccess( const HTTPRequestCompleted_t * pResponse )
{
	m_pParent->OnFragmentRequestSuccess( pResponse->m_hRequest, m_nFragment, m_nType );
}

static const char *s_pFragmentTypeName[ CDemoStreamHttp::FRAGMENT_COUNT ] =
{
	"delta", "full"
};


void CDemoStreamHttp::CFragmentRequest::OnFailure( const HTTPRequestCompleted_t * pResponse )
{
	if ( demo_debug.GetBool() )
		DevMsg( "Failed to retrieve %s fragment %d\n", s_pFragmentTypeName[ m_nType ], m_nFragment );
	m_pParent->OnFragmentRequestFailure( pResponse ? pResponse->m_eStatusCode : k_EHTTPStatusCodeInvalid, m_nFragment, m_nType );
	// CPendingRequest::OnFailure( pResponse ); <-- the parent OnFail will stop streaming, and we don't want that because we'll retry this fragment or next for a few times before giving up
}

void CDemoStreamHttp::Fragment_t::ResetBuffers()
{
	for ( int i = 0; i < FRAGMENT_COUNT; ++i )
	{
		if ( m_pField[ i ] )
		{
			Buffer_t::Release( m_pField[ i ] );
			m_pField[ i ] = NULL;
		}
	}
}

void CDemoStreamHttp::Fragment_t::SetField( FragmentTypeEnum_t nFragment, Buffer_t *pBuffer )
{
	if ( pBuffer )
		Buffer_t::AddRef( pBuffer );
	if ( m_pField[ nFragment ] )
		Buffer_t::Release( m_pField[ nFragment ] );
	m_pField[ nFragment ] = pBuffer;
}

void CDemoStreamHttp::SyncParams_t::PrintSyncRequest(char *buffer, int nBufferSize) const
{
	if ( m_nStartFragment )
		V_snprintf(buffer, nBufferSize, "/sync?fragment=%d", m_nStartFragment );
	else
		V_strncpy( buffer, "/sync", nBufferSize );
}
