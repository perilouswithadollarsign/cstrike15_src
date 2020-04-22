//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "audio_pch.h"
#include <OpenAL/al.h>
#include <OpenAL/alc.h>


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern bool snd_firsttime;

#define NUM_BUFFERS_SOURCES		128
#define	BUFF_MASK				(NUM_BUFFERS_SOURCES - 1 )
#define	BUFFER_SIZE			0x0400


//-----------------------------------------------------------------------------
//
// NOTE: This only allows 16-bit, stereo wave out
//
//-----------------------------------------------------------------------------
class CAudioDeviceOpenAL : public CAudioDeviceBase
{
public:
	CAudioDeviceOpenAL()
	{
		m_pName = "OpenAL Audio";
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

private:
	void	OpenWaveOut( void );
	void	CloseWaveOut( void );
	bool	ValidWaveOut( void ) const;

	ALuint  m_Buffer[NUM_BUFFERS_SOURCES];
	ALuint  m_Source[1];
	int		m_SndBufSize;
	
	void *m_sndBuffers;
	
	int			m_deviceSampleCount;

	int			m_buffersSent;
	int			m_buffersCompleted;
	int			m_pauseCount;

	bool			m_bHeadphone;
	bool			m_bSurround;
	bool			m_bSurroundCenter;
};


IAudioDevice *Audio_CreateOpenALDevice( void )
{
	static CAudioDeviceOpenAL *wave = NULL;
	if ( !wave )
	{
		wave = new CAudioDeviceOpenAL;
	}
	
	if ( wave->Init() )
		return wave;
	
	delete wave;
	wave = NULL;
	
	return NULL;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CAudioDeviceOpenAL::Init( void )
{
	m_SndBufSize = 0;
	m_sndBuffers = NULL;
	m_pauseCount = 0;

	m_bIsHeadphone = false;
	m_buffersSent = 0;
	m_buffersCompleted = 0;
	m_pauseCount = 0;
	
	OpenWaveOut();

	if ( snd_firsttime )
	{
		DevMsg( "Wave sound initialized\n" );
	}
	return ValidWaveOut();
}

void CAudioDeviceOpenAL::Shutdown( void )
{
	CloseWaveOut();
}


//-----------------------------------------------------------------------------
// WAV out device
//-----------------------------------------------------------------------------
inline bool CAudioDeviceOpenAL::ValidWaveOut( void ) const 
{ 
	return m_sndBuffers != 0; 
}


//-----------------------------------------------------------------------------
// Opens the windows wave out device
//-----------------------------------------------------------------------------
void CAudioDeviceOpenAL::OpenWaveOut( void )
{
	m_buffersSent = 0;
	m_buffersCompleted = 0;
	
	ALenum      error;
	ALCcontext    *newContext = NULL;
	ALCdevice    *newDevice = NULL;
	
	// Create a new OpenAL Device
	// Pass NULL to specify the system‚use default output device
	const ALCchar *initStr = (const ALCchar *)"\'( (sampling-rate 44100 ))";
    
	newDevice = alcOpenDevice(initStr);
	if (newDevice != NULL)
	{
		// Create a new OpenAL Context
		// The new context will render to the OpenAL Device just created 
		ALCint attr[] = { ALC_FREQUENCY, SampleRate(), ALC_SYNC, AL_FALSE, 0 };
		
		newContext = alcCreateContext(newDevice, attr );
		if (newContext != NULL)
		{
			// Make the new context the Current OpenAL Context
			alcMakeContextCurrent(newContext);
			
			// Create some OpenAL Buffer Objects
			alGenBuffers( NUM_BUFFERS_SOURCES, m_Buffer);
			if((error = alGetError()) != AL_NO_ERROR) 
			{
				DevMsg("Error Generating Buffers: ");
				return;
			}
			
			// Create some OpenAL Source Objects
			alGenSources(1, m_Source);
			if(alGetError() != AL_NO_ERROR) 
			{
				DevMsg("Error generating sources! \n");
				return;
			}
			
			alListener3f( AL_POSITION,0.0f,0.0f,0.0f);
			int i;
			for ( i = 0; i < 1; i++ )
			{
				alSource3f( m_Source[i],AL_POSITION,0.0f,0.0f,0.0f );
				alSourcef( m_Source[i], AL_PITCH, 1.0f );
				alSourcef( m_Source[i], AL_GAIN, 1.0f );
			}
			
		}
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
void CAudioDeviceOpenAL::CloseWaveOut( void ) 
{ 
	if ( ValidWaveOut() )
	{
		ALCcontext  *context = NULL;
		ALCdevice  *device = NULL;
	
		alSourceStop( m_Source[0] );
		
		// Delete the Sources
		alDeleteSources(1, m_Source);
		// Delete the Buffers
		alDeleteBuffers(NUM_BUFFERS_SOURCES, m_Buffer);
		
		//Get active context
		context = alcGetCurrentContext();
		//Get device for active context
		device = alcGetContextsDevice(context);
		alcMakeContextCurrent( NULL );
		alcSuspendContext(context);
		//Release context
		alcDestroyContext(context);
		//Close device
		alcCloseDevice(device);	
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
int64 CAudioDeviceOpenAL::PaintBegin( float mixAheadTime, int64 soundtime, int64 paintedtime )
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
void CAudioDeviceOpenAL::PaintEnd( void )
{
	if ( !m_sndBuffers )
		return;
	
	ALint state;
	ALenum      error;
	int iloop;
	
	int	cblocks = 4 << 1; 
	ALint processed = 1;
	while ( processed > 0 )
	{
		alGetSourcei( m_Source[ 0 ], AL_BUFFERS_PROCESSED, &processed);
		if ( processed > 0 )
		{
			ALuint tempVal = 0;
			alSourceUnqueueBuffers( m_Source[ 0 ], 1, &tempVal );
			error = alGetError();
			if ( error != AL_NO_ERROR && error != AL_INVALID_NAME ) 
			{
				DevMsg( "Error alSourceUnqueueBuffers %d\n", error );
			}
			else
			{
				m_buffersCompleted++;	// this buffer has been played
			}
		}
	}
	
	//
	// submit a few new sound blocks
	//
	// 44K sound support
	while (((m_buffersSent - m_buffersCompleted) >> SAMPLE_16BIT_SHIFT) < cblocks)
	{	
		int iBuf = m_buffersSent&BUFF_MASK; 
		alBufferData( m_Buffer[iBuf], AL_FORMAT_STEREO16, (char *)m_sndBuffers + iBuf*BUFFER_SIZE, BUFFER_SIZE, SampleRate() );
		if ( (error = alGetError()) != AL_NO_ERROR ) 
		{
			DevMsg( "Error alBufferData %d %d\n", iBuf, error );
		}  
		
		alSourceQueueBuffers( m_Source[0], 1, &m_Buffer[iBuf] );
		if ( (error = alGetError() ) != AL_NO_ERROR ) 
		{
			DevMsg( "Error alSourceQueueBuffers %d %d\n", iBuf, error );
		}  
		m_buffersSent++;
	}
	
	// make sure the stream is playing
	alGetSourcei( m_Source[ 0 ], AL_SOURCE_STATE, &state);
	if ( state != AL_PLAYING )
	{
		Warning( "Restarting sound playback\n" );
		alSourcePlay( m_Source[0] );
		if((error = alGetError()) != AL_NO_ERROR) 
		{
			DevMsg( "Error alSourcePlay %d\n", error );
		}  
	}
}

int CAudioDeviceOpenAL::GetOutputPosition( void )
{
	int s = m_buffersSent * BUFFER_SIZE;

	s >>= SAMPLE_16BIT_SHIFT;

	s &= (DeviceSampleCount()-1);

	return s / ChannelCount();
}


//-----------------------------------------------------------------------------
// Pausing
//-----------------------------------------------------------------------------
void CAudioDeviceOpenAL::Pause( void )
{
	m_pauseCount++;
	if (m_pauseCount == 1)
	{
		alSourceStop( m_Source[0] );
	}
}


void CAudioDeviceOpenAL::UnPause( void )
{
	if ( m_pauseCount > 0 )
	{
		m_pauseCount--;
	}
	
	if ( m_pauseCount == 0 )
		alSourcePlay( m_Source[0] );
}

bool CAudioDeviceOpenAL::IsActive( void )
{
	return ( m_pauseCount == 0 );
}


void CAudioDeviceOpenAL::ClearBuffer( void )
{
	if ( !m_sndBuffers )
		return;

	Q_memset( m_sndBuffers, 0x0, DeviceSampleCount() * DeviceSampleBytes() );
}

void CAudioDeviceOpenAL::UpdateListener( const Vector& position, const Vector& forward, const Vector& right, const Vector& up )
{
}



void CAudioDeviceOpenAL::TransferSamples( int end )
{
	int64	lpaintedtime = g_paintedtime;
	int64	endtime = end;
	
	// resumes playback...

	if ( m_sndBuffers )
	{
		S_TransferStereo16( m_sndBuffers, PAINTBUFFER, lpaintedtime, endtime );
	}
}


