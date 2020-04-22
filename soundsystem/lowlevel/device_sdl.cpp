#include "basetypes.h"
#include "commonmacros.h"

#include "SDL.h"

#include "mix.h"
#include "soundsystem/lowlevel.h"

#define DEFAULT_DEVICE_NAME "SDLDefaultDevice"
#define DEFAULT_DEVICE_NAME_WIDE L"SDLDefaultDevice"

class CAudioSDL : public IAudioDevice2
{
public:
	CAudioSDL()
	{
		m_nDeviceID = 0;
		m_nDeviceIndex = -1;
		m_nBufferSizeBytes = 0;
		m_nBufferCount = 0;
		m_bIsActive = true;
		m_bIsHeadphone = false;
		m_bSupportsBufferStarvationDetection = false;
		m_bIsCaptureDevice = false;

		m_nReadBuffer = m_nWriteBuffer = 0;
		m_nPartialRead = 0;
		m_bAudioStarted = false;
		m_bSilenced = false;
		m_fSilencedVol = 1.0f;
		V_memset( m_pBuffer, 0, sizeof( m_pBuffer ) );
	};

	~CAudioSDL();

	bool		Init( const audio_device_init_params_t &params );
	void		OutputBuffer( int nChannels, CAudioMixBuffer *pChannelArray );
	void		Shutdown();
	int			QueuedBufferCount();
	int			EmptyBufferCount();
	void		CancelOutput();
	void		WaitForComplete();
	void		UpdateFocus( bool bWindowHasFocus );
	void		ClearBuffer();
	const wchar_t *GetDeviceID() const;
	void		OutputDebugInfo() const;

	virtual bool		SetShouldPlayWhenNotInFocus( bool bPlayEvenWhenNotInFocus )
	{
		m_savedParams.m_bPlayEvenWhenNotInFocus = bPlayEvenWhenNotInFocus;
		return true;
	}

	// inline methods
	inline int BytesPerSample() { return BitsPerSample()>>3; }

	void FillAudioBuffer( Uint8 *buf, int len );


private:

	// no copies of this class ever
	CAudioSDL( const CAudioSDL & ); 
	CAudioSDL & operator=( const CAudioSDL & ); 

	int	SamplesPerBuffer() { return MIX_BUFFER_SIZE; }
	int BytesPerBuffer() { return m_nBufferSizeBytes; }

	SDL_AudioDeviceID m_nDeviceID;
	SDL_AudioSpec m_deviceSpec;

	audio_device_init_params_t m_savedParams;

	int			m_nDeviceIndex;
	uint		m_nBufferSizeBytes;
	uint		m_nBufferCount;
	wchar_t		m_deviceID[256];

	enum { kNumBuffers = 32 };
	short		*m_pBuffer[ kNumBuffers ];
	int m_nReadBuffer, m_nWriteBuffer;
	int m_nPartialRead;
	bool m_bAudioStarted;
	CThreadMutex m_mutexBuffer;

	bool m_bSilenced;
	float m_fSilencedVol;
};

CAudioSDL::~CAudioSDL()
{
	Shutdown();
	for ( int i = 0; i != kNumBuffers; ++i )
	{
		if ( m_pBuffer[ i ] )
		{
			MemAlloc_FreeAligned( m_pBuffer[ i ] );
		}
	}
}

static void AudioCallback( void *userdata, Uint8 *stream, int len )
{
	CAudioSDL *dev = reinterpret_cast<CAudioSDL*>( userdata );
	dev->FillAudioBuffer( stream, len );
}

bool CAudioSDL::Init( const audio_device_init_params_t &params )
{
	m_savedParams = params;

	int nDeviceCount = SDL_GetNumAudioDevices( 0 );
	if ( !nDeviceCount )
		return false;

	m_nDeviceIndex = -1;
	if ( params.m_bOverrideDevice )
	{
        if ( wcscmp( params.m_overrideDeviceName, DEFAULT_DEVICE_NAME_WIDE ) == 0 )
        {
            m_nDeviceIndex = -1;
        }
        else
        {
            for( int i = 0; i < nDeviceCount; ++i )
            {
                const char *devName = SDL_GetAudioDeviceName( i, 0 );
                if ( devName == NULL )
                {
                    continue;
                }

                wchar_t devNameWide[AUDIO_DEVICE_NAME_MAX];
                Q_UTF8ToWString( devName, devNameWide, sizeof( devNameWide ) );
                if ( wcscmp( devNameWide, params.m_overrideDeviceName ) == 0 )
                {
                    m_nDeviceIndex = i;
                    break;
                }
            }
		}
	}

#if 0
	// setup the format structure
	{
		int nChannels = details.InputChannels;
		if ( params.m_bOverrideSpeakerConfig )
		{
			nChannels = SpeakerConfigValueToChannelCount( params.m_nOverrideSpeakerConfig );
			if ( params.m_nOverrideSpeakerConfig == 0 )
			{
				m_bIsHeadphone = true;
			}
		}
		m_nChannels = nChannels;
	}
#endif

	SDL_AudioSpec desired;
	desired.freq = int(MIX_DEFAULT_SAMPLING_RATE);
	desired.format = AUDIO_S16SYS;
	desired.channels = 2;
	desired.samples = 1024;
	desired.callback = AudioCallback;
	desired.userdata = this;

	m_nDeviceID = SDL_OpenAudioDevice( m_nDeviceIndex == -1 ? NULL : SDL_GetAudioDeviceName( m_nDeviceIndex, 0 ), 0, &desired, &m_deviceSpec, SDL_AUDIO_ALLOW_ANY_CHANGE );

	const char *pDeviceNameUTF8 = m_nDeviceIndex == -1 ? DEFAULT_DEVICE_NAME : SDL_GetAudioDeviceName( m_nDeviceIndex, 0 );
	Q_UTF8ToWString( pDeviceNameUTF8, m_deviceID, sizeof( m_deviceID ) );

	// UNDONE: Have the device report MIX_DEFAULT_SAMPLING_RATE to the outside world
	// and build an SDL_AudioCVT to convert from the engine's mix to the SDL device's audio output?

	// BUGBUG: Assert this for now
	Assert( m_deviceSpec.channels == 2 );
	Assert( m_deviceSpec.freq == int(MIX_DEFAULT_SAMPLING_RATE) );

	m_nChannels = m_deviceSpec.channels;
	m_nSampleBits = SDL_AUDIO_BITSIZE( m_deviceSpec.format );
	m_nSampleRate = m_deviceSpec.freq;
	m_bIsActive = true;
	m_bIsHeadphone = false;
	m_pName = "SDL Audio";

	//m_nBufferCount = params.m_nOutputBufferCount;
	m_nBufferCount = 1;
	int nBufferSize = MIX_BUFFER_SIZE * m_nChannels * BytesPerSample();
	m_nBufferSizeBytes = nBufferSize;
	
	for ( int i = 0; i != kNumBuffers; ++i )
	{
		m_pBuffer[ i ] = (short *)MemAlloc_AllocAligned( nBufferSize * m_nBufferCount, 16 );
	}

	m_nReadBuffer = m_nWriteBuffer = 0;
	m_nPartialRead = 0;
	m_bAudioStarted = false;

	// start audio playback
	SDL_PauseAudioDevice( m_nDeviceID, 0 );

#if defined( LINUX ) && defined( INCLUDE_SCALEFORM )
	// Send the obtained audio device details to scaleform
    if ( g_pScaleformUI )
    {
        g_pScaleformUI->SDLSetAudioSpec( sizeof(m_deviceSpec), &m_deviceSpec );
    }
#endif

	return true;
}

void CAudioSDL::OutputBuffer( int nChannels, CAudioMixBuffer *pChannelArray )
{
	AUTO_LOCK( m_mutexBuffer );
	m_bAudioStarted = true;
	if ( ( (m_nWriteBuffer+1) % kNumBuffers ) == m_nReadBuffer )
	{
		// Filled up with data, can't take anymore.
		// This shouldn't really happen unless SDL stops consuming data for us or the
		// game pushes data at us at an unreasonable rate.
		return;
	}

	short *pWaveData = m_pBuffer[ m_nWriteBuffer ];
	m_nWriteBuffer = (m_nWriteBuffer+1) % kNumBuffers;

	if ( nChannels == 2 && nChannels == m_nChannels )
	{
		ConvertFloat32Int16_Clamp_Interleave2( pWaveData, pChannelArray[0].m_flData, pChannelArray[1].m_flData, MIX_BUFFER_SIZE );
	}
	else 
	{
		ConvertFloat32Int16_Clamp_InterleaveStride( pWaveData, m_nChannels, MIX_BUFFER_SIZE, pChannelArray[0].m_flData, nChannels, MIX_BUFFER_SIZE );
	}

#if defined( LINUX ) && defined( INCLUDE_SCALEFORM )
    if ( g_pScaleformUI )
    {
        g_pScaleformUI->SDLMixAudio( nChannels, pWaveData, m_nBufferSizeBytes );
    }
#endif

//	Old way of sending data, by queueing it. Now we do it by providing it in the callback.
//	SDL_QueueAudio( m_nDeviceID, m_pBuffer, m_nBufferSizeBytes );
}

void CAudioSDL::Shutdown()
{
	if ( m_nDeviceID > 0 )
	{
		SDL_CloseAudioDevice( m_nDeviceID );
		m_nDeviceID = 0;
	}
}

int CAudioSDL::QueuedBufferCount()
{
	AUTO_LOCK( m_mutexBuffer );
	if ( m_nWriteBuffer >= m_nReadBuffer )
	{
		return m_nWriteBuffer - m_nReadBuffer;
	}
	else
	{
		return (kNumBuffers - m_nReadBuffer) + m_nWriteBuffer;
	}
}

int CAudioSDL::EmptyBufferCount()
{
	return (kNumBuffers - QueuedBufferCount()) - 1;
}

void CAudioSDL::CancelOutput()
{
//	SDL_ClearQueuedAudio( m_nDeviceID );
}

void CAudioSDL::WaitForComplete()
{
	while( QueuedBufferCount() )
	{
		ThreadSleep(0);
	}
}

void CAudioSDL::UpdateFocus( bool bWindowHasFocus )
{
	m_bSilenced = !bWindowHasFocus && !m_savedParams.m_bPlayEvenWhenNotInFocus;
}

void CAudioSDL::ClearBuffer()
{
}

const wchar_t* CAudioSDL::GetDeviceID() const
{
	return L"SDL Device";
}

void CAudioSDL::OutputDebugInfo() const
{
	fprintf(stderr, "SDL Audio Device\n" );
	fprintf(stderr, "Channels:\t%d\n", ChannelCount() );
	fprintf(stderr, "Bits/Sample:\t%d\n", BitsPerSample() );
	fprintf(stderr, "Rate:\t\t%d\n", SampleRate() );
}

void CAudioSDL::FillAudioBuffer( Uint8 *buf, int len )
{
	m_mutexBuffer.Lock();

	bool bFailedToGetMore = false;
	while ( m_bAudioStarted && len > 0 && !bFailedToGetMore )
	{

		if ( m_nReadBuffer == m_nWriteBuffer )
		{
			m_mutexBuffer.Unlock();
/*
			if ( m_savedParams.m_pfnMixAudio != NULL )
			{
				//We are going to be starved, so ask the mixer to mix
				//some more audio for us right now. We expect this
				//call to fill our buffers up.
				m_savedParams.m_pfnMixAudio( 0.01 );
			}
*/
			m_mutexBuffer.Lock();

			if ( m_nReadBuffer == m_nWriteBuffer )
			{
				//The mixer couldn't get us more data for some reason.
				//We are starved.
				bFailedToGetMore = true;
				break;
			}
		}

		while ( len > 0 && m_nReadBuffer != m_nWriteBuffer )
		{
			int bufsize = m_nBufferSizeBytes - m_nPartialRead;
			int nbytes = len < bufsize ? len : bufsize;

			if(m_bSilenced && m_fSilencedVol <= 0.0f)
			{
				memset( buf, 0, nbytes );
			}
			else
			{
				memcpy( buf, ((unsigned char*)m_pBuffer[ m_nReadBuffer ]) + m_nPartialRead, nbytes );

				// If we are silencing or unsilencing, make the volume fade rather than
				// changing abruptly.
				static const float FadeTime = 0.5f;
				static const int FadeTick = 16;
				static const float FadeDelta = FadeTick / ( m_nChannels * m_nSampleRate * FadeTime );

				if ( m_bSilenced )
				{
					short* sbuf = reinterpret_cast<short*>(buf);
					int i = 0;
					while ( i < nbytes/2 )
					{
						sbuf[i] *= m_fSilencedVol;
						++i;
						if ( i%FadeTick == 0 && m_fSilencedVol > 0.0f )
						{
							m_fSilencedVol -= FadeDelta;
							if ( m_fSilencedVol < 0.0f )
							{
								m_fSilencedVol = 0.0f;
							}
						}
					}
				}
				else if ( m_fSilencedVol < 1.0f )
				{
					short* sbuf = reinterpret_cast<short*>(buf);
					int i = 0;
					while ( i < nbytes/2 && m_fSilencedVol < 1.0f )
					{
						sbuf[i] *= m_fSilencedVol;
						++i;

						if ( i%FadeTick == 0 && m_fSilencedVol < 1.0f )
						{
							m_fSilencedVol += FadeDelta;
						}
					}
				}
			}
	
			if ( nbytes == bufsize )
			{
				m_nReadBuffer = (m_nReadBuffer+1) % kNumBuffers;
				m_nPartialRead = 0;
			}
			else
			{
				m_nPartialRead += nbytes;
			}

			buf += nbytes;
			len -= nbytes;
		}
	}

	m_mutexBuffer.Unlock();

	if ( len > 0 )
	{
		// We have been starved of data and have to fill with silence.
		memset( buf, 0, len );
	}
}

static bool g_bInitSDLAudio = false;
static bool InitSDLAudio()
{
	if ( !g_bInitSDLAudio )
	{
		int nRet = SDL_InitSubSystem( SDL_INIT_AUDIO );
		if ( nRet < 0 )
		{
			return false;
		}
		g_bInitSDLAudio = true;
	}
	return true;
}

// enumerate the available devices so the app can select one
// fills out app-supplied list & returns count of available devices.  If the list is too small, the count
// will signal the app to call again with a larger list
int Audio_EnumerateSDLDevices( audio_device_description_t *pDeviceListOut, int nListCount )
{
	if ( !InitSDLAudio() )
		return 0;

	if ( nListCount > 0 )
	{
		audio_device_description_t& description = pDeviceListOut[0];
		Q_UTF8ToWString( DEFAULT_DEVICE_NAME, description.m_deviceName, sizeof(description.m_deviceName) );
        V_strcpy_safe( description.m_friendlyName, "#OS_Default_Device" );
		description.m_nChannelCount = 6;
		description.m_bIsDefault = true;
		description.m_bIsAvailable = true;
		description.m_nSubsystemId = AUDIO_SUBSYSTEM_SDL;
	}

	int nOutputDeviceCount = SDL_GetNumAudioDevices( 0 );

	int nIterateCount = MIN(nListCount-1, nOutputDeviceCount);
	for ( int i = 0; i < nIterateCount; i++ )
	{
		const char *pNameUTF8 = SDL_GetAudioDeviceName( i, 0 );

		audio_device_description_t& description = pDeviceListOut[i+1];

		Q_UTF8ToWString( pNameUTF8, description.m_deviceName, sizeof(description.m_deviceName) );
		Q_WStringToUTF8( description.m_deviceName, description.m_friendlyName, sizeof(description.m_friendlyName) );
		description.m_nChannelCount = 6;
		description.m_bIsDefault = false;
		description.m_bIsAvailable = true;
		description.m_nSubsystemId = AUDIO_SUBSYSTEM_SDL;
	
	}

	return nIterateCount + 1;
}

//-----------------------------------------------------------------------------
// Class factory
//-----------------------------------------------------------------------------
IAudioDevice2 *Audio_CreateSDLDevice( const audio_device_init_params_t &params )
{
	if ( !InitSDLAudio() )
		return NULL;

	CAudioSDL *pDevice = new CAudioSDL;

	if ( pDevice->Init( params ) )
	{
		return pDevice;
	}

	delete pDevice;
	return NULL;
}
