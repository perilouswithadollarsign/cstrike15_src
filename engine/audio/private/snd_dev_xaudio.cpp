//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: X360 XAudio Version
//
//=====================================================================================//


#include "audio_pch.h"
#include "snd_dev_xaudio.h"
#include "utllinkedlist.h"
#include "server.h"
#include "client.h"
#include "matchmaking/imatchframework.h"
#include "tier2/tier2.h"
#include "smartptr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// The outer code mixes in PAINTBUFFER_SIZE (# of samples) chunks (see MIX_PaintChannels), we will never need more than
// that many samples in a buffer.  This ends up being about 20ms per buffer
#define XAUDIO2_BUFFER_SAMPLES	PAINTBUFFER_SIZE
// buffer return has a latency, so need a decent pool
#define MAX_XAUDIO2_BUFFERS		32

#define SURROUND_HEADPHONES		0
#define SURROUND_STEREO			2
#define SURROUND_DIGITAL5DOT1	5

// 5.1 means there are a max of 6 channels
#define MAX_DEVICE_CHANNELS		6

ConVar snd_xaudio_spew_buffers( "snd_xaudio_spew_buffers", "0", 0, "Spew XAudio buffer delivery" );

extern IVEngineClient *engineClient;

//-----------------------------------------------------------------------------
// Implementation of XAudio
//-----------------------------------------------------------------------------
class CAudioXAudio : public CAudioDeviceBase
{
public:
	CAudioXAudio() : m_pXAudio2(NULL), m_pMasteringVoice(NULL), m_pSourceVoice(NULL), m_pOutputBuffer(NULL)
	{
		memset( m_Buffers, 0, sizeof(m_Buffers) ); //	COMPILE_TIME_ASSERT( sizeof(m_Buffers) == sizeof(XAUDIO2_BUFFER)*MAX_XAUDIO2_BUFFERS  );
		m_pName = "XAudio2 Device";
		m_nChannels = 2;
		m_nSampleBits = 16;
		m_nSampleRate = 44100;
		m_bIsActive = true;
	}
	~CAudioXAudio( void );

	bool		IsActive( void ) { return true; }
	bool		Init( void );
	void		Shutdown( void );

	void		Pause( void );
	void		UnPause( void );
	int64		PaintBegin( float mixAheadTime, int64 soundtime, int64 paintedtime );
	int			GetOutputPosition( void );
	void		ClearBuffer( void );
	void		TransferSamples( int64 end );

	const char *DeviceName( void );
	int			DeviceDmaSpeed( void )		{ return m_nSampleRate; }	
	int			DeviceSampleCount( void )	{ return m_deviceSampleCount; }

	void		XAudio2BufferCallback( int hCompletedBuffer );

	static		CAudioXAudio *m_pSingleton;

	CXboxVoice	*GetVoiceData( void ) { return &m_VoiceData; }
	IXAudio2	*GetXAudio2( void ) { return m_pXAudio2; }

private:
	int			TransferStereo( const portable_samplepair_t *pFront, int paintedTime, int endTime, char *pOutptuBuffer );
	int			TransferSurroundInterleaved( const portable_samplepair_t *pFront, const portable_samplepair_t *pRear, const portable_samplepair_t *pCenter, int paintedTime, int endTime, char *pOutputBuffer );

	int			m_deviceSampleCount;				// count of mono samples in output buffer
	int			m_clockDivider;
	
	IXAudio2				*m_pXAudio2;
	IXAudio2MasteringVoice	*m_pMasteringVoice;
	IXAudio2SourceVoice		*m_pSourceVoice;

	XAUDIO2_BUFFER		m_Buffers[MAX_XAUDIO2_BUFFERS];
	BYTE				*m_pOutputBuffer;
	int					m_bufferSizeBytes;			// size of a single hardware output buffer, in bytes
	CInterlockedUInt	m_BufferTail;
	CInterlockedUInt	m_BufferHead;

	CXboxVoice			m_VoiceData;
};
CAudioXAudio *CAudioXAudio::m_pSingleton = NULL;

class XAudio2VoiceCallback : public IXAudio2VoiceCallback
{
public:
    XAudio2VoiceCallback() {}
    ~XAudio2VoiceCallback() {}

    void OnStreamEnd() {}

    void OnVoiceProcessingPassEnd() {}

    void OnVoiceProcessingPassStart( UINT32 SamplesRequired ) {}

    void OnBufferEnd( void *pBufferContext ) 
	{
		CAudioXAudio::m_pSingleton->XAudio2BufferCallback( (int)pBufferContext );
	}

    void OnBufferStart( void *pBufferContext ) {}

    void OnLoopEnd( void *pBufferContext ) {}

    void OnVoiceError( void *pBufferContext, HRESULT Error ) {}
};
XAudio2VoiceCallback s_XAudio2VoiceCallback;

//-----------------------------------------------------------------------------
// Create XAudio Device
//-----------------------------------------------------------------------------
IAudioDevice *Audio_CreateXAudioDevice( bool bInitVoice )
{
	MEM_ALLOC_CREDIT();

	if ( CommandLine()->CheckParm( "-nosound" ) )
	{
		// respect forced lack of audio
		return NULL;
	}

	if ( !CAudioXAudio::m_pSingleton )
	{
		CAudioXAudio::m_pSingleton = new CAudioXAudio;
		if ( !CAudioXAudio::m_pSingleton->Init() )
		{
			AssertMsg( false, "Failed to init CAudioXAudio\n" );
			delete CAudioXAudio::m_pSingleton;
			CAudioXAudio::m_pSingleton = NULL;
		}
	}

	// need to support early init of XAudio (for bink startup video) without the voice
	// voice requires matchmaking which is not available at this early point
	// this defers the voice init to a later engine init mark
	if ( bInitVoice && CAudioXAudio::m_pSingleton )
	{
		CAudioXAudio::m_pSingleton->GetVoiceData()->VoiceInit();
	}

	return CAudioXAudio::m_pSingleton;
}

CXboxVoice *Audio_GetXVoice( void )
{
	if ( CAudioXAudio::m_pSingleton )
	{
		return CAudioXAudio::m_pSingleton->GetVoiceData();
	}

	return NULL;
}

IXAudio2 *Audio_GetXAudio2( void )
{
	if ( CAudioXAudio::m_pSingleton )
	{
		return CAudioXAudio::m_pSingleton->GetXAudio2();
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CAudioXAudio::~CAudioXAudio( void )
{
	m_pSingleton = NULL;
}

//-----------------------------------------------------------------------------
// Initialize XAudio
//-----------------------------------------------------------------------------
bool CAudioXAudio::Init( void )
{
	XAUDIOSPEAKERCONFIG xAudioConfig = 0;
	XGetSpeakerConfig( &xAudioConfig );
	snd_surround.SetValue( ( xAudioConfig & XAUDIOSPEAKERCONFIG_DIGITAL_DOLBYDIGITAL ) ? SURROUND_DIGITAL5DOT1 : SURROUND_STEREO );

	m_bHeadphone = false;
	m_bSurround = false;
	m_bSurroundCenter = false;

	switch ( snd_surround.GetInt() )
	{
	case SURROUND_HEADPHONES:
		m_bHeadphone = true;
		m_nChannels = 2;
		break;

	default:
	case SURROUND_STEREO:
		m_nChannels = 2;
		break;

	case SURROUND_DIGITAL5DOT1:
		m_bSurround = true;	
		m_bSurroundCenter = true;
		m_nChannels = 6;
		break;
	}

	Assert( m_nChannels <= MAX_DEVICE_CHANNELS );

	m_nSampleBits = 16;
	m_nSampleRate = SOUND_DMA_SPEED;

    // initialize the XAudio Engine
	// Both threads on core 2
	m_pXAudio2 = NULL;
	HRESULT hr = XAudio2Create( &m_pXAudio2, 0, XboxThread5 );
	if ( FAILED( hr ) )
		return false;

	// create the mastering voice, this will upsample to the devices target hw output rate
    m_pMasteringVoice = NULL;
	hr = m_pXAudio2->CreateMasteringVoice( &m_pMasteringVoice );
	if ( FAILED( hr ) )
        return false;
	
	// 16 bit PCM
	WAVEFORMATEX waveFormatEx = { 0 };
	waveFormatEx.wFormatTag = WAVE_FORMAT_PCM;
	waveFormatEx.nChannels = m_nChannels;
	waveFormatEx.nSamplesPerSec = m_nSampleRate;
	waveFormatEx.wBitsPerSample = 16;
	waveFormatEx.nBlockAlign = ( waveFormatEx.nChannels * waveFormatEx.wBitsPerSample ) / 8;
	waveFormatEx.nAvgBytesPerSec = waveFormatEx.nSamplesPerSec * waveFormatEx.nBlockAlign;
	waveFormatEx.cbSize = 0;

	m_pSourceVoice = NULL;
	hr = m_pXAudio2->CreateSourceVoice( 
			&m_pSourceVoice, 
			&waveFormatEx, 
			0,
			XAUDIO2_DEFAULT_FREQ_RATIO,
			&s_XAudio2VoiceCallback,
			NULL,
			NULL );
	if ( FAILED( hr ) )
        return false;

	float volumes[MAX_DEVICE_CHANNELS];
	for ( int i = 0; i < MAX_DEVICE_CHANNELS; i++ )
	{
		if ( !m_bSurround && i >= 2 )
		{
			volumes[i] = 0;
		}
		else
		{
			volumes[i] = 1.0;
		}
	}
	m_pSourceVoice->SetChannelVolumes( m_nChannels, volumes );

	m_bufferSizeBytes = XAUDIO2_BUFFER_SAMPLES * (m_nSampleBits/8) * m_nChannels;
	m_pOutputBuffer = new BYTE[MAX_XAUDIO2_BUFFERS * m_bufferSizeBytes];
	ClearBuffer();

	V_memset( m_Buffers, 0, MAX_XAUDIO2_BUFFERS * sizeof( XAUDIO2_BUFFER ) );
	for ( int i = 0; i < MAX_XAUDIO2_BUFFERS; i++ )
	{
		m_Buffers[i].pAudioData = m_pOutputBuffer + i*m_bufferSizeBytes;
		m_Buffers[i].pContext = (LPVOID)i;
	}
	m_BufferHead = 0;
	m_BufferTail = 0;

	// number of mono samples output buffer may hold
	m_deviceSampleCount = MAX_XAUDIO2_BUFFERS * (m_bufferSizeBytes/(DeviceSampleBytes()));
	
	// NOTE: This really shouldn't be tied to the # of bufferable samples.
	// This just needs to be large enough so that it doesn't fake out the sampling in 
	// GetSoundTime().  Basically GetSoundTime() assumes a cyclical time stamp and finds wraparound cases
	// but that means it needs to get called much more often than once per cycle.  So this number should be
	// much larger than the framerate in terms of output time
	m_clockDivider = m_deviceSampleCount / ChannelCount();

	// not really part of XAudio2, but mixer xma lacks one-time init, so doing it here
	XMAPlaybackInitialize();

	hr = m_pSourceVoice->Start( 0 );
    if ( FAILED( hr ) )
		return false;

	DevMsg( "XAudio Device Initialized:\n" );
	DevMsg( "   %s\n"
			"   %d channel(s)\n"
			"   %d bits/sample\n"
			"   %d samples/sec\n", DeviceName(), ChannelCount(), BitsPerSample(), DeviceDmaSpeed() );

	// success
	return true;
}

//-----------------------------------------------------------------------------
// Shutdown XAudio
//-----------------------------------------------------------------------------
void CAudioXAudio::Shutdown( void )
{
	if ( m_pSourceVoice )
	{
		m_pSourceVoice->Stop( 0 );
		m_pSourceVoice->DestroyVoice();
		m_pSourceVoice = NULL;
		delete[] m_pOutputBuffer;
	}

	if ( m_pMasteringVoice )
	{
		m_pMasteringVoice->DestroyVoice();
		m_pMasteringVoice = NULL;
	}

	// need to release ref to XAudio2
	m_VoiceData.VoiceShutdown();

	if ( m_pXAudio2 )
	{
		m_pXAudio2->Release();
		m_pXAudio2 = NULL;
	}

	if ( this == CAudioXAudio::m_pSingleton )
	{
		CAudioXAudio::m_pSingleton = NULL;
	}
}

//-----------------------------------------------------------------------------
// XAudio has completed a buffer. Assuming these are sequential
//-----------------------------------------------------------------------------
void CAudioXAudio::XAudio2BufferCallback( int hCompletedBuffer )
{
	// buffer completion expected to be sequential
	Assert( hCompletedBuffer == (int)( m_BufferTail % MAX_XAUDIO2_BUFFERS ) );

	m_BufferTail++;

	if ( snd_xaudio_spew_buffers.GetBool() )
	{
		if ( m_BufferTail == m_BufferHead )
		{
			Warning( "XAudio: Starved\n" );
		}
		else
		{
			Msg( "XAudio: Buffer Callback, Submit: %2d, Free: %2d\n", m_BufferHead - m_BufferTail, MAX_XAUDIO2_BUFFERS - ( m_BufferHead - m_BufferTail ) );
		}
	}

	if ( m_BufferTail == m_BufferHead )
	{
		// very bad, out of buffers, xaudio is starving
		// mix thread didn't keep up with audio clock and submit buffers
		// submit a silent buffer to keep stream playing and audio clock running
		int head = m_BufferHead++;
		XAUDIO2_BUFFER *pBuffer = &m_Buffers[head % MAX_XAUDIO2_BUFFERS];
		V_memset( (void *)pBuffer->pAudioData, 0, m_bufferSizeBytes );
		pBuffer->AudioBytes = m_bufferSizeBytes;
		m_pSourceVoice->SubmitSourceBuffer( pBuffer );
	}
}

//-----------------------------------------------------------------------------
// Return the "write" cursor.  Used to clock the audio mixing.
// The actual hw write cursor and the number of samples it fetches is unknown.
//-----------------------------------------------------------------------------
int	CAudioXAudio::GetOutputPosition( void )
{
	XAUDIO2_VOICE_STATE state;

	state.SamplesPlayed = 0;
	m_pSourceVoice->GetState( &state );

	return ( state.SamplesPlayed % m_clockDivider );
}

//-----------------------------------------------------------------------------
// Pause playback
//-----------------------------------------------------------------------------
void CAudioXAudio::Pause( void )
{
	if ( m_pSourceVoice )
	{
		m_pSourceVoice->Stop( 0 );
	}
}

//-----------------------------------------------------------------------------
// Resume playback
//-----------------------------------------------------------------------------
void CAudioXAudio::UnPause( void )
{
	if ( m_pSourceVoice )
	{
		m_pSourceVoice->Start( 0 );
	}
}

//-----------------------------------------------------------------------------
// Calc the paint position
//-----------------------------------------------------------------------------
int64 CAudioXAudio::PaintBegin( float mixAheadTime, int64 soundtime, int64 paintedtime )
{
	//  soundtime = total full samples that have been played out to hardware at dmaspeed
	//  paintedtime = total full samples that have been mixed at speed

	//  endtime = target for full samples in mixahead buffer at speed	
	int mixaheadtime = mixAheadTime * DeviceDmaSpeed();
	int64 endtime = soundtime + mixaheadtime;
	if ( endtime <= paintedtime )
	{
		return endtime;
	}

	int fullsamps = MAX_XAUDIO2_BUFFERS * (m_bufferSizeBytes /( DeviceSampleBytes() * ChannelCount() ));

	if ( ( endtime - soundtime ) > fullsamps )
	{
		endtime = soundtime + fullsamps;
	}
	if ( ( endtime - paintedtime ) & 0x03 )
	{
		// The difference between endtime and painted time should align on 
		// boundaries of 4 samples.  This is important when upsampling from 11khz -> 44khz.
		endtime -= ( endtime - paintedtime ) & 0x03;
	}

	return endtime;
}

//-----------------------------------------------------------------------------
// Fill the output buffers with silence
//-----------------------------------------------------------------------------
void CAudioXAudio::ClearBuffer( void )
{
	V_memset( m_pOutputBuffer, 0, MAX_XAUDIO2_BUFFERS * m_bufferSizeBytes );
}

//-----------------------------------------------------------------------------
// Fill the output buffer with L/R samples
//-----------------------------------------------------------------------------
int CAudioXAudio::TransferStereo( const portable_samplepair_t *pFrontBuffer, int paintedTime, int endTime, char *pOutputBuffer )
{
	int linearCount;
	int i;
	int	val;

	int volumeFactor = S_GetMasterVolume() * 256;

	int *pFront = (int *)pFrontBuffer;
	short *pOutput = (short *)pOutputBuffer;
	
	// get size of output buffer in full samples (LR pairs)
	// number of sequential sample pairs that can be wrriten
	linearCount = m_bufferSizeBytes/( DeviceSampleBytes() * ChannelCount() );		

	// clamp output count to requested number of samples
	if ( linearCount > endTime - paintedTime )
	{
		linearCount = endTime - paintedTime;
	}

	// linearCount is now number of mono 16 bit samples (L and R) to xfer.
	linearCount <<= 1;

	// transfer mono 16bit samples multiplying each sample by volume.
	for ( i=0; i<linearCount; i+=2 )
	{
		// L Channel
		val = ( pFront[i] * volumeFactor ) >> 8;
		*pOutput++ = iclip( val );

		// R Channel
		val = ( pFront[i+1] * volumeFactor ) >> 8;
		*pOutput++ = iclip( val );
	}

	return linearCount * DeviceSampleBytes();
}

//-----------------------------------------------------------------------------
// Fill the output buffer with interleaved surround samples
//-----------------------------------------------------------------------------
int CAudioXAudio::TransferSurroundInterleaved( const portable_samplepair_t *pFrontBuffer, const portable_samplepair_t *pRearBuffer, const portable_samplepair_t *pCenterBuffer, int paintedTime, int endTime, char *pOutputBuffer )
{
	int linearCount;
	int i, j;
	int	val;

	int volumeFactor = S_GetMasterVolume() * 256;

	int *pFront = (int *)pFrontBuffer;
	int *pRear = (int *)pRearBuffer;
	int *pCenter = (int *)pCenterBuffer;
	short *pOutput = (short *)pOutputBuffer;
	
	// number of mono samples per channel
	// number of sequential samples that can be wrriten
	linearCount = m_bufferSizeBytes/( DeviceSampleBytes() * ChannelCount() );		

	// clamp output count to requested number of samples
	if ( linearCount > endTime - paintedTime )	
	{	
		linearCount = endTime - paintedTime;
	}

	for ( i = 0, j = 0; i < linearCount; i++, j += 2 )
	{
		// FL
		val = ( pFront[j] * volumeFactor ) >> 8;
		*pOutput++ = iclip( val );

		// FR
		val = ( pFront[j+1] * volumeFactor ) >> 8;
		*pOutput++ = iclip( val );

		// Center
		val = ( pCenter[j] * volumeFactor) >> 8;
		*pOutput++ = iclip( val );

		// Let the hardware mix the sub from the main channels since
		// we don't have any sub-specific sounds, or direct sub-addressing
		*pOutput++ = 0;

		// RL
		val = ( pRear[j] * volumeFactor ) >> 8;
		*pOutput++ = iclip( val );

		// RR
		val = ( pRear[j+1] * volumeFactor ) >> 8;
		*pOutput++ = iclip( val );
	}

	return linearCount * DeviceSampleBytes() * ChannelCount();
}

//-----------------------------------------------------------------------------
// Transfer up to a full paintbuffer (PAINTBUFFER_SIZE) of samples out to the xaudio buffer(s).
//-----------------------------------------------------------------------------
void CAudioXAudio::TransferSamples( int64 endTime )
{
	XAUDIO2_BUFFER *pBuffer;

	if ( m_BufferHead - m_BufferTail >= MAX_XAUDIO2_BUFFERS )
	{
		DevWarning( "XAudio: No Free Buffers!\n" );
		return;
	}

	int sampleCount = endTime - g_paintedtime;
	if ( sampleCount > XAUDIO2_BUFFER_SAMPLES )
	{
		DevWarning( "XAudio: Overflowed mix buffer!\n" );
		endTime = g_paintedtime + XAUDIO2_BUFFER_SAMPLES;
	}

	unsigned int nBuffer = m_BufferHead++;
	pBuffer = &m_Buffers[nBuffer % MAX_XAUDIO2_BUFFERS];

	if ( !m_bSurround )
	{
		pBuffer->AudioBytes = TransferStereo( PAINTBUFFER, g_paintedtime, endTime, (char *)pBuffer->pAudioData );
	}
	else
	{
		pBuffer->AudioBytes = TransferSurroundInterleaved( PAINTBUFFER, REARPAINTBUFFER, CENTERPAINTBUFFER, g_paintedtime, endTime, (char *)pBuffer->pAudioData );
	}

    // submit buffer
	m_pSourceVoice->SubmitSourceBuffer( pBuffer );
}

//-----------------------------------------------------------------------------
// Get our device name
//-----------------------------------------------------------------------------
const char *CAudioXAudio::DeviceName( void )
{ 
	if ( m_bSurround )
	{
		return "XAudio: 5.1 Channel Surround";
	}

	return "XAudio: Stereo"; 
}

CXboxVoice::CXboxVoice()
{
	m_pXHVEngine = NULL;
}

//-----------------------------------------------------------------------------
// Initialize Voice
//-----------------------------------------------------------------------------
void CXboxVoice::VoiceInit( void )
{
	if ( !m_pXHVEngine )
	{
		// Set the processing modes
		XHV_PROCESSING_MODE rgMode = XHV_VOICECHAT_MODE;

		// Set up parameters for the voice chat engine
		XHV_INIT_PARAMS xhvParams = {0};
		xhvParams.dwMaxRemoteTalkers = g_pMatchFramework->GetMatchTitle()->GetTotalNumPlayersSupported();
		xhvParams.dwMaxLocalTalkers = XUSER_MAX_COUNT;
		xhvParams.localTalkerEnabledModes = &rgMode;
		xhvParams.remoteTalkerEnabledModes = &rgMode;
		xhvParams.dwNumLocalTalkerEnabledModes = 1;
		xhvParams.dwNumRemoteTalkerEnabledModes = 1;
		xhvParams.pXAudio2 = CAudioXAudio::m_pSingleton->GetXAudio2();

		// Create the engine
		HRESULT hr = XHV2CreateEngine( &xhvParams, NULL, &m_pXHVEngine );
		if ( hr != S_OK )
		{
			Warning( "Couldn't load XHV engine!\n" );
		}
	}

	memset( m_bUserRegistered, 0 ,sizeof( m_bUserRegistered ) );

	// Reset voice data for all ctrlrs
	for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
	{
		VoiceResetLocalData( k );
	}
}

void CXboxVoice::VoiceShutdown( void )
{
	if ( !m_pXHVEngine )
		return;
	
	m_pXHVEngine->Release();
	m_pXHVEngine = NULL;
}

void CXboxVoice::AddPlayerToVoiceList( XUID xPlayer, int iController, uint64 uiFlags )
{
#ifdef _X360
	XHV_PROCESSING_MODE local_proc_mode = XHV_VOICECHAT_MODE;

	if ( !xPlayer && iController >= 0 && iController < XUSER_MAX_COUNT )
	{
		// If was registered from previous time, then unregister
		if( m_bUserRegistered[ iController ] )
		{
			m_pXHVEngine->UnregisterLocalTalker( iController );
			m_bUserRegistered[ iController ] = false;
		}

		// Now register local talker
		if ( m_pXHVEngine->RegisterLocalTalker( iController ) == S_OK )
		{
			m_bUserRegistered[ iController ] = true;
			m_pXHVEngine->StartLocalProcessingModes( iController, &local_proc_mode, 1 );
		}
	}
	else if ( xPlayer )
	{
		HRESULT res = m_pXHVEngine->RegisterRemoteTalker( xPlayer, NULL, NULL, NULL );
		
		ConVarRef voice_verbose( "voice_verbose" );
		if ( voice_verbose.GetBool() )
		{
			Msg( "* CXboxVoice::AddPlayerToVoiceList: Registering %llx returned 0x%08x\n", xPlayer, res );
		}

		if ( res == S_OK )
		{
			m_pXHVEngine->StartRemoteProcessingModes( xPlayer, &local_proc_mode, 1 );
		}
	}
	else
	{
		Assert( 0 );
	}
#endif
}

void CXboxVoice::RemovePlayerFromVoiceList( XUID xPlayer, int iController )
{
	if ( !xPlayer && iController >= 0 && iController < XUSER_MAX_COUNT )
	{
		if( m_bUserRegistered[ iController ] )
		{
			m_pXHVEngine->UnregisterLocalTalker( iController );
			m_bUserRegistered[ iController ] = false;
		}
	}
	else if ( xPlayer )
	{
		m_pXHVEngine->UnregisterRemoteTalker( xPlayer );
	}
	else
	{
		Assert( 0 );
	}
}

#ifdef _X360
ConVar voice_xplay_debug( "voice_xplay_debug", "0" );
#endif

void CXboxVoice::PlayIncomingVoiceData( XUID xuid, const byte *pbData, unsigned int dwDataSize, const bool *bAudiblePlayers )
{
	ConVarRef voice_verbose( "voice_verbose" );
#ifdef _X360
	static double s_flLastCheckedTime = 0.0;
	static const double s_flCheckFreq = 0.5;

	double flTime = Plat_FloatTime();
	if ( flTime - s_flLastCheckedTime > s_flCheckFreq )
	{
		s_flLastCheckedTime = flTime;

		// Register all idle signed in players
		for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
		{
			bool bSignedIn = !( XUserGetSigninState( k ) == eXUserSigninState_NotSignedIn );
			if ( m_bUserRegistered[k] == bSignedIn )
				continue;

			XHV_PROCESSING_MODE local_proc_mode = XHV_VOICECHAT_MODE;
			if ( bSignedIn )
			{
				if ( m_pXHVEngine->RegisterLocalTalker( k ) == S_OK )
				{
					m_bUserRegistered[ k ] = true;
					m_pXHVEngine->StartLocalProcessingModes( k, &local_proc_mode, 1 );
				}
			}
			else
			{
				m_pXHVEngine->UnregisterLocalTalker( k );
				m_bUserRegistered[ k ] = false;
			}

			// Notify matchmaking that local talkers have changed
			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnSysMuteListChanged" ) );
		}
	}
#endif

	for ( DWORD dwSlot = 0; dwSlot < XBX_GetNumGameUsers(); ++ dwSlot )
	{
		int iCtrlr = XBX_GetUserId( dwSlot );
		IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iCtrlr );
		if ( pPlayer && pPlayer->GetXUID() == xuid )
		{
			 //Hack: Don't play stuff that comes from ourselves.
			if ( voice_verbose.GetBool() )
			{
				Msg( "* CXboxVoice::PlayIncomingVoiceData: dropping voice data from self with dwBytes %u\n", dwDataSize );
			}
			 return;
		}
	}

	if ( g_pMatchFramework->GetMatchSystem()->GetMatchVoice()->CanPlaybackTalker( xuid ) )
	{
		// Set the playback priority for talkers that we can't hear due to game rules.
		for ( DWORD dwSlot = 0; dwSlot < XBX_GetNumGameUsers(); ++dwSlot )
		{
			bool bCanHearPlayer = !bAudiblePlayers || bAudiblePlayers[dwSlot];

			// Check if the client thinks the player is audible
			if ( !bAudiblePlayers )
			{
				for ( int iClient = 0; iClient < GetBaseLocalClient().m_nMaxClients; iClient++ )
				{
					int iIndex = iClient + 1;
					player_info_t infoClient;
					if ( engineClient->GetPlayerInfo( iIndex, &infoClient ) )
					{
						if ( xuid == infoClient.xuid )
						{
							bCanHearPlayer = g_pSoundServices->GetPlayerAudible( iIndex );
							break;
						}
					}
				}
			}

			if ( voice_xplay_debug.GetBool() )
			{
				DevMsg( "XVoiceDebug: %llx -> %d = %s\n", xuid, XBX_GetUserId( dwSlot ), bCanHearPlayer ? "yes" : "no" );
			}
			m_pXHVEngine->SetPlaybackPriority( xuid, XBX_GetUserId( dwSlot ), bCanHearPlayer ? XHV_PLAYBACK_PRIORITY_MAX : XHV_PLAYBACK_PRIORITY_NEVER );
		}

		// For idle users who are simply signed in on the console, but do not participate in the game
		for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
		{
			if ( m_bUserRegistered[k] && ( XBX_GetSlotByUserId( k ) < 0 ) )
			{
				if ( voice_xplay_debug.GetBool() )
				{
					DevMsg( "XVoiceDebug: %llx -> %d = idle\n", xuid, k );
				}
				m_pXHVEngine->SetPlaybackPriority( xuid, k, XHV_PLAYBACK_PRIORITY_NEVER );
			}
		}
	}
	else
	{
		// Cannot submit voice for the player that we are not allowed to playback!
		if ( voice_xplay_debug.GetBool() )
		{
			DevMsg( "XVoiceDebug: %llx muted\n", xuid );
		}
		return;
	}

	DWORD dwApiSize = dwDataSize;
	HRESULT hrSubmit = m_pXHVEngine->SubmitIncomingChatData( xuid, pbData, &dwApiSize );
	if ( voice_verbose.GetBool() )
	{
		Msg( "* CXboxVoice::PlayIncomingVoiceData: SubmitIncomingChatData with %u bytes returned 0x%08x and copied dwBytes %u\n", dwDataSize, hrSubmit, dwApiSize );
	}
}


void CXboxVoice::UpdateHUDVoiceStatus( void )
{
	for ( int iClient = 0; iClient < GetBaseLocalClient().m_nMaxClients; iClient++ )
	{
		// Information about local client if it's a local client speaking
		bool bLocalClient = false;
		int iSsSlot = -1;
		int iCtrlr = -1;

		// Detection if it's a local client
		for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
		{
			CClientState &cs = GetLocalClient( k );
			if ( cs.m_nPlayerSlot == iClient )
			{
				bLocalClient = true;
				iSsSlot = k;
				iCtrlr = XBX_GetUserId( iSsSlot );
			}
		}

		// Convert into index and XUID
		int iIndex = iClient + 1;
		XUID xid =  NULL;
		player_info_t infoClient;
		if ( engineClient->GetPlayerInfo( iIndex, &infoClient ) )
		{
			xid = infoClient.xuid;
		}
		
		// [jason] skip any bots - we don't want to override their voice status from the chatter
		if ( infoClient.fakeplayer )
			continue;

		if ( !xid )
			// No XUID means no VOIP
		{
			g_pSoundServices->OnChangeVoiceStatus( iIndex, -1, false );
			if ( bLocalClient )
				g_pSoundServices->OnChangeVoiceStatus( iIndex, iSsSlot, false );
			continue;
		}

		// Determine talking status
		bool bTalking = false;

		if ( bLocalClient )
		{
			//Make sure the player's own "remote" label is not on.
			g_pSoundServices->OnChangeVoiceStatus( iIndex, -1, false );
			iIndex = -1; // issue notification as ent=-1
			bTalking = IsLocalPlayerTalking( iCtrlr );
		}
		else
		{
			bTalking = IsPlayerTalking( xid );
		}
	
		g_pSoundServices->OnChangeVoiceStatus( iIndex, iSsSlot, bTalking );
	}
}

bool CXboxVoice::VoiceUpdateData( int iCtrlr  )
{
	DWORD dwNumPackets = 0;
	DWORD dwBytes = 0;
	WORD wVoiceBytes = 0;
	bool bShouldSend = false;
	DWORD dwVoiceFlags = m_pXHVEngine->GetDataReadyFlags();

	//Update UI stuff.
	UpdateHUDVoiceStatus();

	while ( 1 )
	{
		int i = iCtrlr;

		if ( IsHeadsetPresent( i ) == false )
			break;

		if ( !(dwVoiceFlags & ( 1 << i )) )
			break;
  
		dwBytes = m_ChatBufferSize - m_wLocalDataSize[i];

		if( dwBytes < XHV_VOICECHAT_MODE_PACKET_SIZE )
		{
			bShouldSend = true;
		}
		else
		{
			HRESULT hr = m_pXHVEngine->GetLocalChatData( i, m_ChatBuffer[i] + m_wLocalDataSize[i], &dwBytes, &dwNumPackets );
			
			ConVarRef voice_verbose( "voice_verbose" );
			if ( voice_verbose.GetBool() )
			{
				Msg( "* CXboxVoice::VoiceUpdateData: GetLocalChatData for user %d returned 0x%08x with dwBytes %u and dwNumPackets %u\n", i, hr, dwBytes, dwNumPackets );
			}

			m_wLocalDataSize[i] += ((WORD)dwBytes) & MAXWORD;

			if( m_wLocalDataSize[i] > ( ( m_ChatBufferSize * 7 ) / 10 ) )
			{
				bShouldSend = true;
			}
		}

		wVoiceBytes += m_wLocalDataSize[i] & MAXWORD;
		break;
	}

	return  bShouldSend || 
		( wVoiceBytes && 
		( GetTickCount() - m_dwLastVoiceSend[ iCtrlr ] ) > MAX_VOICE_BUFFER_TIME );
}

void CXboxVoice::SetPlaybackPriority( XUID remoteTalker, int iController, int iAllowPlayback )
{
	XHV_PLAYBACK_PRIORITY playbackPriority = iAllowPlayback ? XHV_PLAYBACK_PRIORITY_MAX : XHV_PLAYBACK_PRIORITY_NEVER;

	// DevMsg( "[XAUDIO] SetPlaybackPriority %I64x %d %08X\n", remoteTalker, dwUserIndex, playbackPriority );
	m_pXHVEngine->SetPlaybackPriority( remoteTalker, iController, playbackPriority );

#ifdef _X360
	// When setting NEVER playback priority, make sure it is set for all controllers
	if ( playbackPriority == XHV_PLAYBACK_PRIORITY_NEVER )
	{
		for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
		{
			m_pXHVEngine->SetPlaybackPriority( remoteTalker, k, playbackPriority );
		}
	}
#endif
}

void CXboxVoice::GetRemoteTalkers( int *pNumTalkers, XUID *pRemoteTalkers )
{
	m_pXHVEngine->GetRemoteTalkers( (DWORD*)pNumTalkers, pRemoteTalkers );
}

void CXboxVoice::GetVoiceData( int iCtrlr, CCLCMsg_VoiceData_t *pMessage )
{
	IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iCtrlr );
	pMessage->set_xuid( pPlayer ? pPlayer->GetXUID() : 0ull );
	if ( !pMessage->xuid() )
		return;

	if ( !m_wLocalDataSize[ iCtrlr ] ) 
		return;

	pMessage->set_data( m_ChatBuffer[ iCtrlr ], m_wLocalDataSize[ iCtrlr ] );
}

void CXboxVoice::GetVoiceData( int iController, const byte **ppvVoiceDataBuffer, unsigned int *pnumVoiceDataBytes )
{
	*pnumVoiceDataBytes = m_wLocalDataSize[ iController ];
	*ppvVoiceDataBuffer = m_ChatBuffer[ iController ];
}

void CXboxVoice::VoiceSendData( int iCtrlr, INetChannel *pChannel )
{
	CCLCMsg_VoiceData_t voiceMsg;
	GetVoiceData( iCtrlr, &voiceMsg );

	if ( pChannel )
	{
		pChannel->SendNetMsg( voiceMsg, false, true );
		VoiceResetLocalData( iCtrlr );
	}
}

void CXboxVoice::VoiceResetLocalData( int iCtrlr )
{
	m_dwLastVoiceSend[ iCtrlr ] = GetTickCount();
	Q_memset( m_ChatBuffer[ iCtrlr ], 0, m_ChatBufferSize );
	m_wLocalDataSize[ iCtrlr ] = 0;
}

#pragma warning(push) // warning C4800 is meaningless or worse
#pragma warning( disable: 4800 )
bool CXboxVoice::IsLocalPlayerTalking( int controllerID )
{
	return m_pXHVEngine->IsLocalTalking( controllerID ) != 0;
}

bool CXboxVoice::IsPlayerTalking( XUID uid )
{
#ifdef _X360
	if ( !g_pMatchFramework->GetMatchSystem()->GetMatchVoice()->CanPlaybackTalker( uid ) )
		return false;

	return m_pXHVEngine->IsRemoteTalking( uid ) != 0;
#else
	return false;
#endif
}

bool CXboxVoice::IsHeadsetPresent( int id )
{
	return m_pXHVEngine->IsHeadsetPresent( id ) != 0;
}
#pragma warning(pop)

void CXboxVoice::RemoveAllTalkers()
{
#ifdef _X360
	int numRemoteTalkers;
	CArrayAutoPtr< XUID > remoteTalkers( new XUID[ g_pMatchFramework->GetMatchTitle()->GetTotalNumPlayersSupported() ] );
	GetRemoteTalkers( &numRemoteTalkers, remoteTalkers.Get() );

	for ( int iRemote = 0; iRemote < numRemoteTalkers; iRemote++ )
	{
		m_pXHVEngine->UnregisterRemoteTalker( remoteTalkers[iRemote] );
	}

	for ( int i = 0; i < XUSER_MAX_COUNT; ++i )
	{
		if( !m_bUserRegistered[ i ] )
			continue;

		m_pXHVEngine->UnregisterLocalTalker( i );
		m_bUserRegistered[ i ] = false;
	}
#endif
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CXboxVoice, IEngineVoice,
								  IENGINEVOICE_INTERFACE_VERSION, *Audio_GetXVoice() );
