//===== Copyright (c) 1996-2010, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "audio_pch.h"
#include <AudioToolbox/AudioQueue.h>
#include <AudioToolbox/AudioFile.h>
#include <AudioToolbox/AudioFormat.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern bool snd_firsttime;
extern bool MIX_ScaleChannelVolume( paintbuffer_t *ppaint, channel_t *pChannel, int volume[CCHANVOLUMES], int mixchans );

#define NUM_BUFFERS_SOURCES		128
#define	BUFF_MASK				(NUM_BUFFERS_SOURCES - 1 )
#define	BUFFER_SIZE			0x0400


//-----------------------------------------------------------------------------
//
// NOTE: This only allows 16-bit, stereo wave out
//
//-----------------------------------------------------------------------------
class CAudioDeviceAudioQueue : public CAudioDeviceBase
{
public:
	CAudioDeviceAudioQueue()
	{
		m_pName = "AudioQueue";
		m_nChannels = 2;
		m_nSampleBits = 16;
		m_nSampleRate = 44100;
		m_bIsActive = true;
	}

	bool		IsActive( void );
	bool		Init( void );
	void		Shutdown( void );
	int			GetOutputPosition( void );
	void		Pause( void );
	void		UnPause( void );

	int64		PaintBegin( float mixAheadTime, int64 soundtime, int64 paintedtime );
	void		PaintEnd( void );
	void		ClearBuffer( void );
	void		UpdateListener( const Vector& position, const Vector& forward, const Vector& right, const Vector& up );

	void		TransferSamples( int end );

	int			DeviceSampleCount( void )	{ return m_deviceSampleCount; }

	void BufferCompleted() { m_buffersCompleted++; }
	void SetRunning( bool bState ) { m_bRunning = bState; }
	
private:
	void	OpenWaveOut( void );
	void	CloseWaveOut( void );
	bool	ValidWaveOut( void ) const;
	bool	BIsPlaying();

	AudioStreamBasicDescription m_DataFormat;
	AudioQueueRef               m_Queue;
	AudioQueueBufferRef         m_Buffers[NUM_BUFFERS_SOURCES];
	
	int		m_SndBufSize;
	
	void *m_sndBuffers;
	
	CInterlockedInt	m_deviceSampleCount;

	int			m_buffersSent;
	int			m_buffersCompleted;
	int			m_pauseCount;
	bool		m_bSoundsShutdown;
	
	bool m_bFailed;
	bool m_bRunning;
	
	bool m_bSurround;
	bool m_bSurroundCenter;
	bool m_bHeadphone;
};

CAudioDeviceAudioQueue *wave = NULL;


static void AudioCallback(void *pContext, AudioQueueRef pQueue, AudioQueueBufferRef pBuffer)
{
	if ( wave )
		wave->BufferCompleted();
}


IAudioDevice *Audio_CreateMacAudioQueueDevice( void )
{
	wave = new CAudioDeviceAudioQueue;
	if ( wave->Init() )
		return wave;
	
	delete wave;
	wave = NULL;
	
	return NULL;
}


void OnSndSurroundCvarChanged2( IConVar *pVar, const char *pOldString, float flOldValue );
void OnSndSurroundLegacyChanged2( IConVar *pVar, const char *pOldString, float flOldValue );

//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CAudioDeviceAudioQueue::Init( void )
{
	m_SndBufSize = 0;
	m_sndBuffers = NULL;
	m_pauseCount = 0;

	m_bIsHeadphone = false;
	m_buffersSent = 0;
	m_buffersCompleted = 0;
	m_pauseCount = 0;
	m_bSoundsShutdown = false;
	m_bFailed = false;
	m_bRunning = false;
	
	m_Queue = NULL;
	
	static bool first = true;
	if ( first )
	{
		snd_surround.SetValue( 2 );
		snd_surround.InstallChangeCallback( &OnSndSurroundCvarChanged2 );
		snd_legacy_surround.InstallChangeCallback( &OnSndSurroundLegacyChanged2 );
		first = false;
	}
	
	OpenWaveOut();

	if ( snd_firsttime )
	{
		DevMsg( "Wave sound initialized\n" );
	}
	return ValidWaveOut() && !m_bFailed;
}

void CAudioDeviceAudioQueue::Shutdown( void )
{
	CloseWaveOut();
}


//-----------------------------------------------------------------------------
// WAV out device
//-----------------------------------------------------------------------------
inline bool CAudioDeviceAudioQueue::ValidWaveOut( void ) const 
{ 
	return m_sndBuffers != 0 && m_Queue; 
}


//-----------------------------------------------------------------------------
// called by the mac audioqueue code when we run out of playback buffers
//-----------------------------------------------------------------------------
void AudioQueueIsRunningCallback( void* inClientData, AudioQueueRef inAQ, AudioQueuePropertyID inID)
{
    CAudioDeviceAudioQueue* audioqueue = (CAudioDeviceAudioQueue*)inClientData;
	
	UInt32 running = 0;
	UInt32 size;
	OSStatus err = AudioQueueGetProperty(inAQ, kAudioQueueProperty_IsRunning, &running, &size);
	audioqueue->SetRunning( running != 0 );
	//DevWarning( "AudioQueueStart %d\n", running );
}




//-----------------------------------------------------------------------------
// Opens the windows wave out device
//-----------------------------------------------------------------------------
void CAudioDeviceAudioQueue::OpenWaveOut( void )
{
	if ( m_Queue ) 
		return;
		
	m_buffersSent = 0;
	m_buffersCompleted = 0;
		
    m_DataFormat.mSampleRate       = SOUND_DMA_SPEED;
    m_DataFormat.mFormatID         = kAudioFormatLinearPCM;
    m_DataFormat.mFormatFlags      = kAudioFormatFlagIsSignedInteger|kAudioFormatFlagIsPacked;
    m_DataFormat.mBytesPerPacket   = 4; // 16-bit samples * 2 channels
    m_DataFormat.mFramesPerPacket  = 1;
    m_DataFormat.mBytesPerFrame    = 4; // 16-bit samples * 2 channels
    m_DataFormat.mChannelsPerFrame = 2;
    m_DataFormat.mBitsPerChannel   = 16;
    m_DataFormat.mReserved         = 0;
	
    // Create the audio queue that will be used to manage the array of audio
    // buffers used to queue samples.
    OSStatus err = AudioQueueNewOutput(&m_DataFormat, AudioCallback, this, NULL, NULL, 0, &m_Queue);	
	if ( err != noErr) 
	{
		DevMsg( "Failed to create AudioQueue output %d\n", err );
		m_bFailed = true;
		return;
	}
		
    for ( int i = 0; i < NUM_BUFFERS_SOURCES; ++i) 
	{
        err = AudioQueueAllocateBuffer( m_Queue, BUFFER_SIZE,&(m_Buffers[i]));
		if ( err != noErr) 
		{
			DevMsg( "Failed to AudioQueueAllocateBuffer output %d (%i)\n", err,i );
			m_bFailed = true;
		}
		
        m_Buffers[i]->mAudioDataByteSize = BUFFER_SIZE;        
        Q_memset( m_Buffers[i]->mAudioData, 0, BUFFER_SIZE );
    }
	
    err = AudioQueuePrime( m_Queue, 0, NULL);
	if ( err != noErr) 
	{
		DevMsg( "Failed to create AudioQueue output %d\n", err );
		m_bFailed = true;
		return;
	}
	
	AudioQueueSetParameter( m_Queue, kAudioQueueParam_Volume, 1.0);
	
	err = AudioQueueAddPropertyListener( m_Queue, kAudioQueueProperty_IsRunning, AudioQueueIsRunningCallback, this );
	if ( err != noErr) 
	{
		DevMsg( "Failed to create AudioQueue output %d\n", err );
		m_bFailed = true;
		return;
	}
	
	m_SndBufSize = NUM_BUFFERS_SOURCES*BUFFER_SIZE;
	m_deviceSampleCount = m_SndBufSize / DeviceSampleBytes();
	
	if ( !m_sndBuffers )
	{
		m_sndBuffers = malloc( m_SndBufSize );
		memset( m_sndBuffers, 0x0, m_SndBufSize );
	}
}


//-----------------------------------------------------------------------------
// Closes the windows wave out device
//-----------------------------------------------------------------------------
void CAudioDeviceAudioQueue::CloseWaveOut( void ) 
{ 
	if ( ValidWaveOut() )
	{
		AudioQueueStop(m_Queue, true);
		m_bRunning = false;
		
		AudioQueueRemovePropertyListener( m_Queue, kAudioQueueProperty_IsRunning, AudioQueueIsRunningCallback, this );

		for ( int i = 0; i < NUM_BUFFERS_SOURCES; i++ )
			AudioQueueFreeBuffer( m_Queue, m_Buffers[i]);

		AudioQueueDispose( m_Queue, true);
		
		m_Queue = NULL;
	}
	
	if ( m_sndBuffers )
	{
		free( m_sndBuffers );
		m_sndBuffers = NULL;
	}
}



//-----------------------------------------------------------------------------
// Mixing setup
//-----------------------------------------------------------------------------
int64 CAudioDeviceAudioQueue::PaintBegin( float mixAheadTime, int64 soundtime, int64 paintedtime )
{
	//  soundtime - total samples that have been played out to hardware at dmaspeed
	//  paintedtime - total samples that have been mixed at speed
	//  endtime - target for samples in mixahead buffer at speed

	int64 endtime = soundtime + mixAheadTime * SampleRate();
	
	int samps = DeviceSampleCount() >> (ChannelCount()-1);

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


//-----------------------------------------------------------------------------
// Actually performs the mixing
//-----------------------------------------------------------------------------
void CAudioDeviceAudioQueue::PaintEnd( void )
{
	int	cblocks = 8 << 1; 
	//
	// submit a few new sound blocks
	//
	// 44K sound support
	while (((m_buffersSent - m_buffersCompleted) >> SAMPLE_16BIT_SHIFT) < cblocks)
	{	
		int iBuf = m_buffersSent&BUFF_MASK; 
		
		m_Buffers[iBuf]->mAudioDataByteSize = BUFFER_SIZE;
		Q_memcpy( m_Buffers[iBuf]->mAudioData, (char *)m_sndBuffers + iBuf*BUFFER_SIZE, BUFFER_SIZE);
		
		// Queue the buffer for playback.
		OSStatus err = AudioQueueEnqueueBuffer( m_Queue, m_Buffers[iBuf], 0, NULL);
		if ( err != noErr) 
		{
			DevMsg( "Failed to AudioQueueEnqueueBuffer output %d\n", err );
		}
		
		m_buffersSent++;
	}

	
	if ( !m_bRunning )
	{
		DevMsg( "Restarting sound playback\n" );
		m_bRunning = true;
		AudioQueueStart( m_Queue, NULL);
	}

}

int CAudioDeviceAudioQueue::GetOutputPosition( void )
{
	int s = m_buffersSent * BUFFER_SIZE;

	s >>= SAMPLE_16BIT_SHIFT;

	s &= (DeviceSampleCount()-1);

	return s / ChannelCount();
}


//-----------------------------------------------------------------------------
// Pausing
//-----------------------------------------------------------------------------
void CAudioDeviceAudioQueue::Pause( void )
{
	m_pauseCount++;
	if (m_pauseCount == 1)
	{
		m_bRunning = false;
		AudioQueueStop(m_Queue, true);
	}
}


void CAudioDeviceAudioQueue::UnPause( void )
{
	if ( m_pauseCount > 0 )
	{
		m_pauseCount--;
	}
	
	if ( m_pauseCount == 0 )
	{ 
		m_bRunning = true;
		AudioQueueStart( m_Queue, NULL);
	}
}

bool CAudioDeviceAudioQueue::IsActive( void )
{
	return ( m_pauseCount == 0 );
}


void CAudioDeviceAudioQueue::ClearBuffer( void )
{
	if ( !m_sndBuffers )
		return;

	Q_memset( m_sndBuffers, 0x0, DeviceSampleCount() * DeviceSampleBytes() );
}

void CAudioDeviceAudioQueue::UpdateListener( const Vector& position, const Vector& forward, const Vector& right, const Vector& up )
{
}


bool CAudioDeviceAudioQueue::BIsPlaying()
{
	UInt32 isRunning;  
	UInt32 propSize = sizeof(isRunning);  
  
    OSStatus result = AudioQueueGetProperty( m_Queue, kAudioQueueProperty_IsRunning, &isRunning, &propSize);  
	return isRunning != 0;
}




void CAudioDeviceAudioQueue::TransferSamples( int end )
{
	int64	lpaintedtime = g_paintedtime;
	int64	endtime = end;
	
	// resumes playback...

	if ( m_sndBuffers )
	{
		S_TransferStereo16( m_sndBuffers, PAINTBUFFER, lpaintedtime, endtime );
	}
}


static uint32 GetOSXSpeakerConfig()
{
	return 2;
}

static uint32 GetSpeakerConfigForSurroundMode( int surroundMode, const char **pConfigDesc )
{
	uint32 newSpeakerConfig = 2;
	*pConfigDesc = "stereo speaker";
	return newSpeakerConfig;
}



void OnSndSurroundCvarChanged2( IConVar *pVar, const char *pOldString, float flOldValue )
{
	// if the old value is -1, we're setting this from the detect routine for the first time
	// no need to reset the device
	if ( flOldValue == -1 )
		return;
	
	// get the user's previous speaker config
	uint32 speaker_config = GetOSXSpeakerConfig();
	
	// get the new config
	uint32 newSpeakerConfig = 0;
	const char *speakerConfigDesc = "";
	
	ConVarRef var( pVar );
	newSpeakerConfig = GetSpeakerConfigForSurroundMode( var.GetInt(), &speakerConfigDesc );
	// make sure the config has changed
	if (newSpeakerConfig == speaker_config)
		return;
	
	// set new configuration
	//SetWindowsSpeakerConfig(newSpeakerConfig);
	
	Msg("Speaker configuration has been changed to %s.\n", speakerConfigDesc);
	
	// restart sound system so it takes effect
	//g_pSoundServices->RestartSoundSystem();
}

void OnSndSurroundLegacyChanged2( IConVar *pVar, const char *pOldString, float flOldValue )
{
}


