//===== Copyright  1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "audio_pch.h"

#if !DEDICATED

#include "tier0/dynfunction.h"
#include "avi/ibik.h"

// prevent some conflicts in SDL headers...
#undef M_PI
#include <stdint.h>
#ifndef _STDINT_H_
#define _STDINT_H_ 1
#endif

#include "SDL.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IVEngineClient* engineClient;
extern bool snd_firsttime;
extern bool MIX_ScaleChannelVolume( paintbuffer_t *ppaint, channel_t *pChannel, int volume[CCHANVOLUMES], int mixchans );
extern void S_SpatializeChannel( int nSlot, int volume[6], int master_vol, const Vector *psourceDir, float gain, float mono );

ConVar snd_mute_losefocus("snd_mute_losefocus", "1", FCVAR_ARCHIVE);
static ConVar sdl_speaker_channels( "sdl_speaker_channels", "-1", FCVAR_RELEASE|FCVAR_ARCHIVE );

static void OnSDLSpeakerChannelsChanged( IConVar *pVar, const char *pOldString, float flOldValue )
{
	if ( atoi( pOldString ) != sdl_speaker_channels.GetInt() )
	{
		g_pSoundServices->RestartSoundSystem();
	}
}

// 64K is about 1/3 second at 16-bit, stereo, 44100 Hz
// 44k: UNDONE - need to double buffers now that we're playing back at 44100?
#define	WAV_BUFFERS             64
#define	WAV_MASK				(WAV_BUFFERS - 1)
#define	WAV_BUFFER_SIZE			0x0400

#if 0
#define debugsdl printf
#else
static inline void debugsdl(const char *fmt, ...) {}
#endif


//-----------------------------------------------------------------------------
//
// NOTE: This only allows 16-bit, stereo wave out  (!!! FIXME: but SDL supports 7.1, etc, too!)
//
//-----------------------------------------------------------------------------
class CAudioDeviceSDLAudio : public CAudioDeviceBase
{
public:
	CAudioDeviceSDLAudio();
	virtual ~CAudioDeviceSDLAudio();

	bool		IsActive( void );
	bool		Init( void );
	void		Shutdown( void );
	void		PaintEnd( void );
	int			GetOutputPosition( void );
	void		ChannelReset( int entnum, int channelIndex, float distanceMod );
	void		Pause( void );
	void		UnPause( void );
	float		MixDryVolume( void );
	bool		Should3DMix( void );
	void		StopAllSounds( void );

	int64		PaintBegin( float mixAheadTime, int64 soundtime, int64 paintedtime );
	void		ClearBuffer( void );
	void		MixBegin( int sampleCount );
	void		MixUpsample( int sampleCount, int filtertype );
	void		Mix8Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );
	void		Mix8Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );
	void		Mix16Mono( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );
	void		Mix16Stereo( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );

	void		TransferSamples( int end );
	void		SpatializeChannel( int nSlot, int volume[CCHANVOLUMES/2], int master_vol, const Vector& sourceDir, float gain, float mono);
	void		ApplyDSPEffects( int idsp, portable_samplepair_t *pbuffront, portable_samplepair_t *pbufrear, portable_samplepair_t *pbufcenter, int samplecount );

	const char *DeviceName( void )			{ return "SDL"; }
	int			DeviceChannels( void )		{ return m_numChannels; }
	int			DeviceSampleBits( void )	{ return 16; }
	int			DeviceSampleBytes( void )	{ return 2; }
	int			DeviceDmaSpeed( void )		{ return SOUND_DMA_SPEED; }
	int			DeviceSampleCount( void )	{ return m_deviceSampleCount; }

private:
	SDL_AudioDeviceID m_devId;

	static void SDLCALL AudioCallbackEntry(void *userdata, Uint8 * stream, int len);
	void AudioCallback(Uint8 *stream, int len);

	void	OpenWaveOut( void );
	void	CloseWaveOut( void );
	void	AllocateOutputBuffers();
	void	FreeOutputBuffers();
	bool	ValidWaveOut( void ) const;

	void TransferSurround16( void *pOutput, const portable_samplepair_t *pfront, const portable_samplepair_t *prear, const portable_samplepair_t *pcenter, int64 lpaintedtime, int64 endtime, int nChannels);

	int			m_numChannels;

	int			m_deviceSampleCount;

	int			m_buffersSent;
	int			m_pauseCount;
	int			m_readPos;
	int			m_partialWrite;

	// Memory for the wave data
	uint8_t		*m_pBuffer;

	bool m_bSurround;
	bool m_bSurroundCenter;
	bool m_bHeadphone;
};

static CAudioDeviceSDLAudio *g_wave = NULL;

//-----------------------------------------------------------------------------
// Constructor (just lookup SDL entry points, real work happens in this->Init())
//-----------------------------------------------------------------------------
CAudioDeviceSDLAudio::CAudioDeviceSDLAudio()
{
	m_devId = 0;
	m_numChannels = 2;
}

//-----------------------------------------------------------------------------
// Destructor. Make sure our global pointer gets set to NULL.
//-----------------------------------------------------------------------------
CAudioDeviceSDLAudio::~CAudioDeviceSDLAudio()
{
	g_wave = NULL;
}

//-----------------------------------------------------------------------------
// Class factory
//-----------------------------------------------------------------------------
IAudioDevice *Audio_CreateSDLAudioDevice( void )
{
	if ( !g_wave )
	{
		g_wave = new CAudioDeviceSDLAudio;
		Assert( g_wave );
	}

	if ( g_wave && !g_wave->Init() )
	{
		delete g_wave;
		g_wave = NULL;
	}

	return g_wave;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CAudioDeviceSDLAudio::Init( void )
{
	// If we've already got a device open, then return. This allows folks to call
	//	Audio_CreateSDLAudioDevice() multiple times. CloseWaveOut() will free the
	//  device, and set m_devId to 0.
	if( m_devId )
		return true;
	
	m_bSurround = false;
	m_bSurroundCenter = false;
	m_bHeadphone = false;
	m_buffersSent = 0;
	m_pauseCount = 0;
	m_pBuffer = NULL;
	m_readPos = 0;
	m_partialWrite = 0;
	m_devId = 0;

	OpenWaveOut();

	if ( snd_firsttime )
	{
		DevMsg( "Wave sound initialized\n" );
	}

	return ValidWaveOut();
}

void CAudioDeviceSDLAudio::Shutdown( void )
{
	CloseWaveOut();
}


//-----------------------------------------------------------------------------
// WAV out device
//-----------------------------------------------------------------------------
inline bool CAudioDeviceSDLAudio::ValidWaveOut( void ) const 
{ 
	return m_devId != 0;
}


//-----------------------------------------------------------------------------
// Opens the windows wave out device
//-----------------------------------------------------------------------------
void CAudioDeviceSDLAudio::OpenWaveOut( void )
{
	debugsdl("SDLAUDIO: OpenWaveOut...\n");

#ifndef WIN32
	// Set these environment variables, in case we're using PulseAudio.
	const char *appname = "Counter-Strike: Global Offensive";  // !!! FIXME: get this from elsewhere.
	setenv("PULSE_PROP_application.name", appname, 1);
	setenv("PULSE_PROP_media.role", "game", 1);
#endif

	// !!! FIXME: specify channel map, etc
	// !!! FIXME: set properties (role, icon, etc).

	//#define SDLAUDIO_FAIL(fnstr) do { DevWarning(fnstr " failed"); CloseWaveOut(); return; } while (false)
	//#define SDLAUDIO_FAIL(fnstr) do { printf("SDLAUDIO: " fnstr " failed: %s\n", SDL_GetError ? SDL_GetError() : "???"); CloseWaveOut(); return; } while (false)
	#define SDLAUDIO_FAIL(fnstr) do { printf("SDLAUDIO: " fnstr " failed: %s\n", SDL_GetError() ); CloseWaveOut(); return; } while (false)

	if (!SDL_WasInit(SDL_INIT_AUDIO))
	{
		if (SDL_InitSubSystem(SDL_INIT_AUDIO))
			SDLAUDIO_FAIL("SDL_InitSubSystem(SDL_INIT_AUDIO)");
	}

	char target[128];
	debugsdl("SDLAUDIO: Using SDL audio target '%s'\n", SDL_GetCurrentAudioDriver());

	int nChannelsDesired = sdl_speaker_channels.GetInt();
	if ( nChannelsDesired == 1 )
	{
		//This really means 'headphones' which we want 2 channels for
		nChannelsDesired = 2;
		m_bHeadphone = true;
	}
	else if ( nChannelsDesired <= 0 )
	{
		nChannelsDesired = 6;
	}

	// Open an audio device...
	//  !!! FIXME: let user specify a device?
	// !!! FIXME: we can handle quad, 5.1, 7.1, etc here.

	SDL_AudioSpec obtained;

	m_devId = 0;
	for ( m_devId = 0; m_devId == 0 && nChannelsDesired > 0; nChannelsDesired -= 2 )
	{
		SDL_AudioSpec desired;
		memset(&desired, '\0', sizeof (desired));
		desired.freq = SOUND_DMA_SPEED;
		desired.format = AUDIO_S16SYS;
		desired.samples = 2048;
		desired.channels = nChannelsDesired;
		desired.callback = &CAudioDeviceSDLAudio::AudioCallbackEntry;
		desired.userdata = this;
		m_devId = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
	}

	if (!m_devId)
		SDLAUDIO_FAIL("SDL_OpenAudioDevice()");
	

	m_bSurround = obtained.channels >= 4;
	m_bSurroundCenter = obtained.channels >= 6;

	m_numChannels = obtained.channels;

	if ( sdl_speaker_channels.GetInt() == 1 && m_numChannels == 2 )
	{
		//'headphones' setting.
		sdl_speaker_channels.SetValue( 1 );
	}
	else
	{
		sdl_speaker_channels.SetValue( m_numChannels );
	}

	#undef SDLAUDIO_FAIL

	// We're now ready to feed audio data to SDL!
	AllocateOutputBuffers();
	SDL_PauseAudioDevice(m_devId, 0);

#if defined( BINK_VIDEO ) && defined( LINUX )
	// Tells Bink to use SDL for its audio decoding
	if ( g_pBIK && g_pBIK->SetSDLDevice( obtained.freq, obtained.format, obtained.channels ) == 0 )
	{
		Assert( 0 );
	}
#endif

	static bool first_time = true;
	if ( first_time )
	{
		first_time = false;

		sdl_speaker_channels.InstallChangeCallback( &OnSDLSpeakerChannelsChanged );
	}
}

//-----------------------------------------------------------------------------
// Closes the windows wave out device
//-----------------------------------------------------------------------------
void CAudioDeviceSDLAudio::CloseWaveOut( void ) 
{ 
	// none of these SDL_* functions are available to call if this is false.
	if (m_devId)
	{
		SDL_CloseAudioDevice(m_devId);
		m_devId = 0;
	}
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	FreeOutputBuffers();
}

//-----------------------------------------------------------------------------
// Allocate output buffers
//-----------------------------------------------------------------------------
void CAudioDeviceSDLAudio::AllocateOutputBuffers()
{
	// Allocate and lock memory for the waveform data.  
	const int nBufferSize = (WAV_BUFFER_SIZE * WAV_BUFFERS * DeviceChannels())/2;
	m_pBuffer = new uint8_t[nBufferSize];
	memset(m_pBuffer, '\0', nBufferSize);
	m_readPos = 0;
	m_partialWrite = 0;
	m_deviceSampleCount = nBufferSize / DeviceSampleBytes();
}


//-----------------------------------------------------------------------------
// Free output buffers
//-----------------------------------------------------------------------------
void CAudioDeviceSDLAudio::FreeOutputBuffers()
{
	delete[] m_pBuffer;
	m_pBuffer = NULL;
}


//-----------------------------------------------------------------------------
// Mixing setup
//-----------------------------------------------------------------------------
int64 CAudioDeviceSDLAudio::PaintBegin( float mixAheadTime, int64 soundtime, int64 paintedtime )
{
	//  soundtime - total samples that have been played out to hardware at dmaspeed
	//  paintedtime - total samples that have been mixed at speed
	//  endtime - target for samples in mixahead buffer at speed
	unsigned int endtime = soundtime + mixAheadTime * DeviceDmaSpeed();
	
	int samps = DeviceSampleCount() / DeviceChannels();

	if ((int)(endtime - soundtime) > samps)
		endtime = soundtime + samps;

	if ((endtime - paintedtime) & 0x3)
	{
		// The difference between endtime and painted time should align on 
		// boundaries of 4 samples.  This is important when upsampling from 11khz -> 44khz.
		endtime -= (endtime - paintedtime) & 0x3;
	}

	return endtime;
}

void CAudioDeviceSDLAudio::AudioCallbackEntry(void *userdata, Uint8 *stream, int len)
{
	((CAudioDeviceSDLAudio *) userdata)->AudioCallback(stream, len);
}

void CAudioDeviceSDLAudio::AudioCallback(Uint8 *stream, int len)
{
	if (!m_devId)
	{
		debugsdl("SDLAUDIO: uhoh, no audio device!\n");
		return;  // can this even happen?
	}

	const int totalWriteable = len;
	Uint8 *stream_orig = stream;
	debugsdl("SDLAUDIO: writable size is %d.\n", totalWriteable);

	Assert(len <= (WAV_BUFFERS * WAV_BUFFER_SIZE));

	while (len > 0)
	{
		// spaceAvailable == bytes before we overrun the end of the ring buffer.
		const int spaceAvailable = ((WAV_BUFFERS * WAV_BUFFER_SIZE * DeviceChannels())/2 - m_readPos);
		const int writeLen = (len < spaceAvailable) ? len : spaceAvailable;

		if (writeLen > 0)
		{
			const uint8_t *buf = m_pBuffer + m_readPos;
			debugsdl("SDLAUDIO: Writing %d bytes...\n", writeLen);

			#if 0
			static FILE *io = NULL;
			if (io == NULL) io = fopen("dumpplayback.raw", "wb");
			if (io != NULL) { fwrite(buf, writeLen, 1, io); fflush(io); }
			#endif

			if ( engineClient->IsActiveApp() == false && snd_mute_losefocus.GetBool() )
			{
				// The game is in the background and we are set to mute
				// while in background so just play silence.
				memset(stream, 0, writeLen);
			}
			else
			{
				memcpy(stream, buf, writeLen);
			}

			stream += writeLen;
			len -= writeLen;
			Assert(len >= 0);
		}

		m_readPos = (m_readPos + writeLen) % ((WAV_BUFFERS * WAV_BUFFER_SIZE * DeviceChannels())/2);  // if still bytes to write to stream, we're rolling around the ring buffer.
	}

#if defined( BINK_VIDEO ) && defined( LINUX )
	// Mix in Bink movie audio if that stuff is playing.
	g_pBIK->SDLMixerAudioCallback( stream_orig, totalWriteable );
#endif

	// Translate between bytes written and buffers written.
	m_partialWrite += totalWriteable;
	m_buffersSent += m_partialWrite / WAV_BUFFER_SIZE;
	m_partialWrite %= WAV_BUFFER_SIZE;
}


//-----------------------------------------------------------------------------
// Actually performs the mixing
//-----------------------------------------------------------------------------
void CAudioDeviceSDLAudio::PaintEnd( void )
{
	debugsdl("SDLAUDIO: PaintEnd...\n");

#if 0  // !!! FIXME: this is the 1.3 headers, but not implemented yet in SDL.
	if (SDL_AudioDeviceConnected(m_devId) != 1)
	{
		debugsdl("SDLAUDIO: Audio device was disconnected!\n");
		Shutdown();
	}
#endif
}

int CAudioDeviceSDLAudio::GetOutputPosition( void )
{
	return (m_readPos >> SAMPLE_16BIT_SHIFT)/DeviceChannels();
}


//-----------------------------------------------------------------------------
// Pausing
//-----------------------------------------------------------------------------
void CAudioDeviceSDLAudio::Pause( void )
{
	m_pauseCount++;
	if (m_pauseCount == 1)
	{
		debugsdl("SDLAUDIO: PAUSE\n");
		SDL_PauseAudioDevice(m_devId, 1);
	}
}


void CAudioDeviceSDLAudio::UnPause( void )
{
	if ( m_pauseCount > 0 )
	{
		m_pauseCount--;
		if (m_pauseCount == 0)
		{
			debugsdl("SDLAUDIO: UNPAUSE\n");
			SDL_PauseAudioDevice(m_devId, 0);
		}
	}
}

bool CAudioDeviceSDLAudio::IsActive( void )
{
	return ( m_pauseCount == 0 );
}

float CAudioDeviceSDLAudio::MixDryVolume( void )
{
	return 0;
}


bool CAudioDeviceSDLAudio::Should3DMix( void )
{
	return false;
}


void CAudioDeviceSDLAudio::ClearBuffer( void )
{
	int		clear;

	if ( !m_pBuffer )
		return;

	clear = 0;

	Q_memset(m_pBuffer, clear, DeviceSampleCount() * DeviceSampleBytes() );
}


void CAudioDeviceSDLAudio::MixBegin( int sampleCount )
{
	MIX_ClearAllPaintBuffers( sampleCount, false );
}


void CAudioDeviceSDLAudio::MixUpsample( int sampleCount, int filtertype )
{
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();
	int ifilter = ppaint->ifilter;
	
	Assert (ifilter < CPAINTFILTERS);

	S_MixBufferUpsample2x( sampleCount, ppaint->pbuf, &(ppaint->fltmem[ifilter][0]), CPAINTFILTERMEM, filtertype );

	if ( ppaint->fsurround )
	{
		Assert( ppaint->pbufrear );
		S_MixBufferUpsample2x( sampleCount, ppaint->pbufrear, &(ppaint->fltmemrear[ifilter][0]), CPAINTFILTERMEM, filtertype );

		if ( ppaint->fsurround_center )
		{
			Assert( ppaint->pbufcenter );
			S_MixBufferUpsample2x( sampleCount, ppaint->pbufcenter, &(ppaint->fltmemcenter[ifilter][0]), CPAINTFILTERMEM, filtertype );
		}
	}

	ppaint->ifilter++;
}

void CAudioDeviceSDLAudio::Mix8Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();

	if (!MIX_ScaleChannelVolume( ppaint, pChannel, volume, 1))
		return;

	Mix8MonoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, (byte *)pData, inputOffset, rateScaleFix, outCount );

	if ( ppaint->fsurround )
	{
		Assert( ppaint->pbufrear );
		Mix8MonoWavtype( pChannel, ppaint->pbufrear  + outputOffset, &volume[IREAR_LEFT], (byte *)pData, inputOffset, rateScaleFix, outCount );

		if ( ppaint->fsurround_center )
		{
			Assert( ppaint->pbufcenter );
			Mix8MonoWavtype( pChannel, ppaint->pbufcenter  + outputOffset, &volume[IFRONT_CENTER], (byte *)pData, inputOffset, rateScaleFix, outCount );
		}
	}
}


void CAudioDeviceSDLAudio::Mix8Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();

	if (!MIX_ScaleChannelVolume( ppaint, pChannel, volume, 2 ))
		return;

	Mix8StereoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, (byte *)pData, inputOffset, rateScaleFix, outCount );

	if ( ppaint->fsurround )
	{
		Assert( ppaint->pbufrear );
		Mix8StereoWavtype( pChannel, ppaint->pbufrear  + outputOffset, &volume[IREAR_LEFT], (byte *)pData, inputOffset, rateScaleFix, outCount );

		if ( ppaint->fsurround_center )
		{
			Assert( ppaint->pbufcenter );
			Mix8StereoWavtype( pChannel, ppaint->pbufcenter  + outputOffset, &volume[IFRONT_CENTER], (byte *)pData, inputOffset, rateScaleFix, outCount );
		}
	}
}


void CAudioDeviceSDLAudio::Mix16Mono( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();

	if (!MIX_ScaleChannelVolume( ppaint, pChannel, volume, 1 ))
		return;

	Mix16MonoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, pData, inputOffset, rateScaleFix, outCount );

	if ( ppaint->fsurround )
	{
		Assert( ppaint->pbufrear );
		Mix16MonoWavtype( pChannel, ppaint->pbufrear  + outputOffset, &volume[IREAR_LEFT], pData, inputOffset, rateScaleFix, outCount );

		if ( ppaint->fsurround_center )
		{
			Assert( ppaint->pbufcenter );
			Mix16MonoWavtype( pChannel, ppaint->pbufcenter  + outputOffset, &volume[IFRONT_CENTER], pData, inputOffset, rateScaleFix, outCount );
		}
	}
}


void CAudioDeviceSDLAudio::Mix16Stereo( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();

	if (!MIX_ScaleChannelVolume( ppaint, pChannel, volume, 2 ))
		return;

	Mix16StereoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, pData, inputOffset, rateScaleFix, outCount );

	if ( ppaint->fsurround )
	{
		Assert( ppaint->pbufrear );
		Mix16StereoWavtype( pChannel, ppaint->pbufrear  + outputOffset, &volume[IREAR_LEFT], pData, inputOffset, rateScaleFix, outCount );

		if ( ppaint->fsurround_center )
		{
			Assert( ppaint->pbufcenter );
			Mix16StereoWavtype( pChannel, ppaint->pbufcenter  + outputOffset, &volume[IFRONT_CENTER], pData, inputOffset, rateScaleFix, outCount );
		}
	}
}


void CAudioDeviceSDLAudio::ChannelReset( int entnum, int channelIndex, float distanceMod )
{
}


void CAudioDeviceSDLAudio::TransferSamples( int end )
{
	int		lpaintedtime = g_paintedtime;
	int		endtime = end;
	
	// resumes playback...

	if ( m_pBuffer )
	{
		if ( m_bSurround )
		{
			TransferSurround16( m_pBuffer, PAINTBUFFER, REARPAINTBUFFER, CENTERPAINTBUFFER, lpaintedtime, endtime, DeviceChannels() );
		}
		else
		{
			S_TransferStereo16( m_pBuffer, PAINTBUFFER, lpaintedtime, endtime );
		}
	}
}

void CAudioDeviceSDLAudio::SpatializeChannel( int nSlot, int volume[CCHANVOLUMES/2], int master_vol, const Vector& sourceDir, float gain, float mono )
{
	VPROF("CAudioDeviceSDLAudio::SpatializeChannel");
	S_SpatializeChannel( nSlot, volume, master_vol, &sourceDir, gain, mono );
}

void CAudioDeviceSDLAudio::StopAllSounds( void )
{
}


void CAudioDeviceSDLAudio::ApplyDSPEffects( int idsp, portable_samplepair_t *pbuffront, portable_samplepair_t *pbufrear, portable_samplepair_t *pbufcenter, int samplecount )
{
	//SX_RoomFX( endtime, filter, timefx );
	DSP_Process( idsp, pbuffront, pbufrear, pbufcenter, samplecount );
}

void CAudioDeviceSDLAudio::TransferSurround16( void *pOutput, const portable_samplepair_t *pfront, const portable_samplepair_t *prear, const portable_samplepair_t *pcenter, int64 lpaintedtime, int64 endtime, int nChannels)
{
	const int volume = S_GetMasterVolume()*256;

	const int* snd_front = (const int*)pfront;
	const int* snd_rear = (const int*)prear;
	const int* snd_center = (const int*)pcenter;

	const int numSamples = (WAV_BUFFER_SIZE * WAV_BUFFERS)/4;
	while(lpaintedtime < endtime)
	{
		const int lpos = lpaintedtime % numSamples;
		int64 snd_linear_count = numSamples - lpos;
		if ( snd_linear_count > endtime - lpaintedtime )
		{
			snd_linear_count = endtime - lpaintedtime;
		}

		short *snd_out = (short *)pOutput + lpos*nChannels;

		for ( int i = 0; i < snd_linear_count; ++i )
		{
			const short* base = snd_out;

			*snd_out++ = (snd_front[i*2] * volume) >> 8;
			*snd_out++ = (snd_front[i*2+1] * volume) >> 8;

			if ( nChannels >= 6 )
			{
				*snd_out++ = (snd_center[i*2] * volume) >> 8;
				*snd_out++ = 0;
			}

			if ( nChannels >= 4 )
			{
				*snd_out++ = (snd_rear[i*2] * volume) >> 8;
				*snd_out++ = (snd_rear[i*2+1] * volume) >> 8;
			}

			while ( snd_out - base < nChannels )
			{
				*snd_out++ = 0;
			}
		}

		lpaintedtime += snd_linear_count;
		snd_front += snd_linear_count*2;
		snd_rear += snd_linear_count*2;
		snd_center += snd_linear_count*2;
	}
}

#endif // !DEDICATED

