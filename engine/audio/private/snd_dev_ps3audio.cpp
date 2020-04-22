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

#include "vjobs/sndupsampler_shared.h"

// from PS3
#include <sysutil/sysutil_sysparam.h>
#include <cell/audio.h>
#include <sys/event.h>
#include <cell/sysmodule.h> 
#include <cell/mic.h>
#include <cell/voice.h>

#include <vjobs_interface.h>
#include <vjobs/root.h>
#include <ps3/vjobutils.h>

#include <vjobs/sndupsampler_shared.h>
#include <ps3/vjobutils_shared.h>

#include "engine/IPS3FrontPanelLED.h"

#include <tier0/microprofiler.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#include "sys/tty.h"

// The outer code mixes in PAINTBUFFER_SIZE (# of samples) chunks (see MIX_PaintChannels), we will never need more than
// that many samples in a buffer.  This ends up being about 20ms per buffer
#define XAUDIO2_BUFFER_SAMPLES	PAINTBUFFER_SIZE

#define ENABLE_SOUND_DEBUG 0
#define SOUND_PC_CELP_SUPPORTED

extern IVEngineClient *engineClient;
extern IVJobs * g_pVJobs;

ConVar snd_ps3_back_channel_multiplier( "snd_ps3_back_channel_multiplier", "0" );

//-----------------------------------------------------------------------------
// An ugly workaround: our sound mixing library outputs at 44.1khz; PS3 inputs
// at 48khz. We'll probably need to replace the mixer wholesale, but for now,
// add an upsampling stage to the pipeline.
//-----------------------------------------------------------------------------
class CAudioIckyUpsampler // a non-virtual class
{
public:
	// local constants
	enum ConstEnum_t
	{
		INPUTFREQ  = job_sndupsampler::INPUTFREQ,
		OUTPUTFREQ = job_sndupsampler::OUTPUTFREQ,
		BUFFERSIZE = job_sndupsampler::BUFFERSIZE, // < input samples, should be a power of two
	};
	
	CThreadMutex m_mutex;

	CAudioIckyUpsampler();
	~CAudioIckyUpsampler(); // NON-VIRTUAL .. mark virtual if you inherit from here

	// initialize to stereo or surround. Can be called again to reinitialize.
	void Init( bool surround );
		
	// take in some number of input samples. Returns the number actually consumed (the buffer might be full)
	int IngestStereo( portable_samplepair_t *pInputSamples, int numSamples ) RESTRICT;
	int IngestSurround( portable_samplepair_t *pInputStereo, portable_samplepair_t *pInputSurround, portable_samplepair_t *pInputCenter, int numSamples ) RESTRICT;

	// upsample and emit the left and right channels into an interleaved float stream.
	// scalar gets multiplied onto each sample as it's written (ie to rescale -32767..32767 -> -1..1
	// returns number of samples actually emitted.
	// assumes PS3 interleaving (
	/*
	// you specify a pointer to the left channel, an offset (in words) between the left and right 
	// channel, and the stride between successive samples. Returns the number of samples actually
	// emitted.
	// scalar gets multiplied onto each sample as it's written (ie to rescale -32767..32767 -> -1..1
	int Excrete( uint nSamplesRequested, float *pOutSamples, int nLeftToRightOffset, int nstride, float scalar );
	*/
	// We output stereo in a surround channel.
	int ExcreteStereo( uint nSamplesRequested, libaudio_sample_surround_t *pOutSamples, float scalar ) RESTRICT;
	int ExcreteSurround(  uint nSamplesRequested, libaudio_sample_surround_t *pOutSamples, float scalar  ) RESTRICT;

	/// dump all data
	void Flush();

	// the number of INPUT samples already in the buffer
	inline int Count();
	inline int SpaceRemaining() { return BUFFERSIZE - Count(); }
	int OutputSamplesAvailable(); 	
	inline bool IsSurround() { return m_bSurround; }
public:
	/// ring buffers: one interleaved pair for stereo, or five channels for surround.
	/// I compress them back to signed shorts to conserve memory bandwidth.
	typedef job_sndupsampler::stereosample_t stereosample_t;
	typedef job_sndupsampler::surroundsample_t surroundsample_t;

	union // the pointer to the input buffer of samples at 44k
	{
		stereosample_t *m_p44kStereoIn;
		surroundsample_t *m_p44kSurroundIn;
		uintp m_ea44kSamplesIn;
	};

	// size in bytes of an input sample for the current channel mode 
	inline int InputSampleSizeof( ) { return m_bSurround ? sizeof(surroundsample_t) : sizeof(stereosample_t); }

	CInterlockedInt m_ringHead; ///< "head" of the buffer, the first sample Excrete() will look at
	CInterlockedInt m_ringCount; ///< number of samples in the buffer
	float m_fractionalSamples; ///< we will have some fractional number of samples still waiting to be sent. ie 0.2f means we still have temporally 80% of the HEAD sample to emit.
	bool m_bSurround; // set to surround sound mode
};

CAudioIckyUpsampler::CAudioIckyUpsampler()
	: m_ringHead( 0 )
	, m_ringCount( 0 )
	, m_fractionalSamples( 0 )
	, m_p44kStereoIn( NULL )
	, m_bSurround( false )
{
}


CAudioIckyUpsampler::~CAudioIckyUpsampler()
{
	if ( IsSurround() ) 
		MemAlloc_FreeAligned( m_p44kSurroundIn );
	else
		MemAlloc_FreeAligned( m_p44kStereoIn );
	m_p44kStereoIn = NULL;
}

void CAudioIckyUpsampler::Init( bool surround )
{
	// there's a buffer already, toss it. (in certain cases could just recycle it, but it's unlikely that this is ever actually called twice)
	if ( m_p44kStereoIn != NULL ) 
	{
		MemAlloc_FreeAligned( m_p44kStereoIn );
		m_p44kStereoIn = NULL;
	}
	
	// Note : we need to allocate aligned memory so that when DMA'ing ring buffer that wraps around,
	//        there's no hole at the wrap point (so the end and start of the buffer must be aligned

	if ( m_bSurround = surround ) 
	{	// we are in surround mode
		m_p44kSurroundIn = ( surroundsample_t* ) MemAlloc_AllocAligned( sizeof( surroundsample_t ) * BUFFERSIZE, 16 );
		memset( m_p44kSurroundIn, 0, sizeof(surroundsample_t)*BUFFERSIZE );
	}
	else
	{	// stereo mode
		m_p44kStereoIn = ( stereosample_t* )MemAlloc_AllocAligned( sizeof( stereosample_t ) * BUFFERSIZE, 16 );
		memset( m_p44kStereoIn, 0, sizeof(stereosample_t)*BUFFERSIZE );
	}
}

void CAudioIckyUpsampler::Flush()
{
	AUTO_LOCK( m_mutex ); // TODO: get rid of this lock, use head-tail counters instead of head+count
	m_fractionalSamples = 0;
	m_ringHead = m_ringCount = 0;
}

inline int CAudioIckyUpsampler::Count()
{
	return m_ringCount;
}

int CAudioIckyUpsampler::IngestStereo( portable_samplepair_t * RESTRICT pInputSamples, int numSamples ) RESTRICT 
{
	if ( IsSurround() )
	{
		Error( "Audio: Tried to ingest stereo data into a surround upsampler!\n" );
	}

	const int nSamplesToConsume = MIN( numSamples, SpaceRemaining() ); 
	int i,o;
	for ( i = 0, o = (m_ringHead + m_ringCount)%BUFFERSIZE  ; 
		  i < nSamplesToConsume								; 
		  ++i, o = (o + 1)%BUFFERSIZE						) // power-of-two mods become ands 
	{
		m_p44kStereoIn[ o ].left = pInputSamples[i].left;
		m_p44kStereoIn[ o ].right = pInputSamples[i].right;
	}

	m_ringCount += nSamplesToConsume;
	return nSamplesToConsume;	
	

	// ------------------------------------------
	// PURELY FOR EXAMPLE: a basic sine wave test
	if ( false ) {
		int q = 0 ;
		while ( q++ < numSamples && m_ringCount < BUFFERSIZE )
		{
			int i = (m_ringHead + m_ringCount)%BUFFERSIZE;
		
			float f = sin( ( i & 0x100 ? (2.0/256.0) : (2.0/128.0)) * M_PI * i ) * 20000;
			m_p44kStereoIn[i].left = f;
			m_p44kStereoIn[i].right = f;
			
			++m_ringCount;
		}
		return numSamples; 
	}
}

int CAudioIckyUpsampler::IngestSurround( portable_samplepair_t * RESTRICT pInputStereo, portable_samplepair_t * RESTRICT pInputSurround, portable_samplepair_t * RESTRICT pInputCenterAsPairs, int numSamples ) RESTRICT 
{
	if ( !IsSurround() )
	{
		Error( "Audio: Tried to ingest stereo data into a surround upsampler!\n" );
	}

	const int nSamplesToConsume = MIN( numSamples, SpaceRemaining() ); 
	int i,o;
	for ( i = 0, o = (m_ringHead + m_ringCount)%BUFFERSIZE  ; 
		i < nSamplesToConsume								; 
		++i, o = (o + 1)%BUFFERSIZE						) // power-of-two mods become ands 
	{
		// hopefully the compiler knows how to pipeline these intelligently ( should be >1 IPC )
		m_p44kSurroundIn[ o ].left = pInputStereo[i].left;
		m_p44kSurroundIn[ o ].right = pInputStereo[i].right;
		m_p44kSurroundIn[ o ].center = pInputCenterAsPairs[i].left;		// Center is stored as left / right, but only left contains the necessary information
		m_p44kSurroundIn[ o ].surleft = pInputSurround[i].left;
		m_p44kSurroundIn[ o ].surright = pInputSurround[i].right;
	}

	m_ringCount += nSamplesToConsume;
	return nSamplesToConsume;	
}

#pragma message("TODO: obvious optimization opportunities")
// TODO: move to SPU  -- many obvious optimization opportunities
float g_flSumStereo;
int CAudioIckyUpsampler::ExcreteStereo( uint nSamplesRequested, libaudio_sample_surround_t * RESTRICT pOutSamples, float scalar ) RESTRICT 
{
	Assert( !IsSurround() );
	// do a braindead linear interpolation
	float t = m_fractionalSamples;
	float tstep = ((float)INPUTFREQ)/((float) OUTPUTFREQ);
	const int nSamplesToEmit = MIN( OutputSamplesAvailable(), nSamplesRequested );

	int curSamp = m_ringHead;
	int nextSamp = (curSamp + 1) & (BUFFERSIZE - 1);
	int n44kSamplesConsumed = 0;
	//  iterator
	for ( int iRemain = nSamplesToEmit ; 
		  iRemain > 0				   ; 
		  t += tstep, --iRemain )
	{
		// if we've passed a sample boundary, go onto the next sample
		if ( t >= 1.0f ) // ugh, fcmp, it's just a hacky prototype
		{
			t -= 1.0f;
			curSamp = nextSamp;
			nextSamp = (curSamp + 1) & (BUFFERSIZE - 1);
			++n44kSamplesConsumed;
		}

		pOutSamples->left  = Lerp( t, m_p44kStereoIn[curSamp].left,  m_p44kStereoIn[nextSamp].left )  * scalar; // left 
		pOutSamples->right = Lerp( t, m_p44kStereoIn[curSamp].right, m_p44kStereoIn[nextSamp].right ) * scalar; // right
		// Fill the remaining with blank
		pOutSamples->center  = 0;
		pOutSamples->subwoofer = 0;
		pOutSamples->leftsurround  = 0;
		pOutSamples->rightsurround = 0;
		pOutSamples->leftextend = 0;
		pOutSamples->rightextend = 0;

		g_flSumStereo += pOutSamples->left * pOutSamples->left + pOutSamples->right * pOutSamples->right;
		pOutSamples++;
	}

	// Make sure to update correctly the variables after the last sample (after all, we did t += tstep).
	if ( t >= 1.0f )
	{
		t -= 1.0f;
		curSamp = nextSamp;
		++n44kSamplesConsumed;
	}
	m_fractionalSamples = t;
	
	m_ringHead = curSamp;
	m_ringCount -= n44kSamplesConsumed;
	return nSamplesToEmit;
}


int  CAudioIckyUpsampler::ExcreteSurround(  uint nSamplesRequested, libaudio_sample_surround_t *pOutSamples, float scalar  ) RESTRICT
{
	Assert( IsSurround() );

	// do a braindead linear interpolation
	float t = m_fractionalSamples;
	float tstep = ((float)INPUTFREQ)/((float) OUTPUTFREQ);
	const int nSamplesToEmit = MIN( OutputSamplesAvailable(), nSamplesRequested );

	int curSamp = m_ringHead;
	int nextSamp = (curSamp + 1) & (BUFFERSIZE - 1);
	int n44kSamplesConsumed = 0;
	const float flBackChannelMultipler = snd_ps3_back_channel_multiplier.GetFloat();
	//  iterator
	for ( int iRemain = nSamplesToEmit ; 
		iRemain > 0				   ; 
		t += tstep, --iRemain )
	{
		// if we've passed a sample boundary, go onto the next sample
		if ( t >= 1.0f ) // ugh, fcmp, it's just a hacky prototype
		{
			t -= 1.0f;
			curSamp = nextSamp;
			nextSamp = (curSamp + 1) & (BUFFERSIZE - 1);
			++n44kSamplesConsumed;
		}

		pOutSamples->left  = Lerp( t, m_p44kSurroundIn[curSamp].left,  m_p44kSurroundIn[nextSamp].left )  * scalar; // left 
		pOutSamples->right = Lerp( t, m_p44kSurroundIn[curSamp].right, m_p44kSurroundIn[nextSamp].right ) * scalar; // right
		pOutSamples->center  = Lerp( t, m_p44kSurroundIn[curSamp].center,  m_p44kSurroundIn[nextSamp].center )  * scalar; // left 
		pOutSamples->subwoofer = 0;		// As on X360, let the HW mix the sub from the main channel since we don't have any sub-specific sounds, or direct sub-addressing
		float fLeftSurround = Lerp( t, m_p44kSurroundIn[curSamp].surleft,  m_p44kSurroundIn[nextSamp].surleft )  * scalar; // left 
		float fRightSurround = Lerp( t, m_p44kSurroundIn[curSamp].surright, m_p44kSurroundIn[nextSamp].surright ) * scalar; // right
		pOutSamples->leftsurround  = fLeftSurround;
		pOutSamples->rightsurround = fRightSurround;
		pOutSamples->leftextend = fLeftSurround * flBackChannelMultipler;
		pOutSamples->rightextend = fRightSurround * flBackChannelMultipler;
		pOutSamples++;
	}

	// Make sure to update correctly the variables after the last sample (after all, we did t += tstep).
	if ( t >= 1.0f )
	{
		t -= 1.0f;
		curSamp = nextSamp;
		++n44kSamplesConsumed;
	}
	m_fractionalSamples = t;
	
	m_ringHead = curSamp;
	m_ringCount -= n44kSamplesConsumed;
	return nSamplesToEmit;
}

int CAudioIckyUpsampler::OutputSamplesAvailable()
{
	// returns the number of output samples we can generate (notice this rounds down, ie we may leave some fractional samples for next time)
	return ( ( m_ringCount - 1 ) * OUTPUTFREQ) / INPUTFREQ;
}

//-----------------------------------------------------------------------------
// Implementation of an audio device on top of PS3's libaudio
//-----------------------------------------------------------------------------
class CAudioPS3LibAudio: public CAudioDeviceBase, public VJobInstance
{
public:
	~CAudioPS3LibAudio( void );
	CAudioPS3LibAudio( void );

	bool		IsActive( void ) { return true; }
	bool		Init( void ) ;
	void		Shutdown( void );

	void		Pause( void );
	void		UnPause( void );
	int64		PaintBegin( float mixAheadTime, int64 soundtime, int64 paintedtime );
	void		PaintEnd( void );
	int			GetOutputPosition( void );
	void		ClearBuffer( void );
	void		TransferSamples( int64 end ) ;

	virtual const char *DeviceName( void );
	virtual int			DeviceChannels( void );
	virtual int			DeviceSampleBits( void );
	virtual int			DeviceSampleBytes( void );
	virtual int			DeviceDmaSpeed( void );
	virtual int			DeviceSampleCount( void );

	CEngineVoicePs3	*GetVoiceData( void ) { return &m_VoiceData; }
	static		CAudioPS3LibAudio *m_pSingleton;

	static void	QuerySystemAudioConfiguration( bool bForceRefreshCache = false ); // retrieves information from the OS about what output devices are connected. Caches result.


	virtual void OnVjobsInit(){ s_LibAudioThread.OnVjobsInit( m_pRoot ); }
	virtual void OnVjobsShutdown() { s_LibAudioThread.OnVjobsShutdown(); }

protected:

	inline float *GetAddressForBlockIdx( unsigned nBlock );
	inline uint GetPortBufferSize()const;
	inline float *GetNextReadBlockAddress();
	int UpdateBufferCount( void );  // update the count of how many filled buffers remain to be read. return m_nFilledBuffers to avoid a LHS

#if 0 // These functions were deprecated by the move to a DMA thread:
	// blit the audio data to the hardware. returns the number of samples written.
	int TransferStereo( const portable_samplepair_t *pFrontBuffer, int nSamplesToWrite, int nStartBlock );
	int TransferSurroundInterleaved( const portable_samplepair_t *pFrontBuffer, const portable_samplepair_t *pRearBuffer, const int *pCenterBuffer, 
		int nSamplesToWrite, int nStartBlock );
#endif


	int			m_deviceChannels;					// channels per ring port ( either 2 for stereo or 8 for surround -- PS3's libaudio has no concept of 5.1 ) 

	// information from the OS.
	// right now this is all a little redundant because we are only using the "primary" sound device,
	// which always has exactly one logical device even if there are multiple things attached. But it
	// will be significant if we eventually create another class to handle "secondary" outputs, in which
	// case the members below should not be static, and QuerySystemAudioConfiguration() will need to take
	// a flag specifying CELL_AUDIO_OUT_PRIMARY or CELL_AUDIO_OUT_SECONDARY. You'll also need to 
	// touch the init/shutdown functions so that you only intialize the OS's audio lib once.
	static bool	s_bCellAudioStateInitialized;
	static CUtlVectorConservative<CellAudioOutState> s_vCellAudioOutputDeviceStates;
	uint32_t m_nPortNum; // the "port number" given us by Cell
	sys_addr_t m_pPortBufferAddr; // base address of the audio port's ring buffer
	float * m_pDebugBuffer;
	volatile uint64_t* m_pReadIndexAddr; // pointer to an int containing the index of next block in the buffer that the hardware will read
	
	char m_nPhysicalOutChannels;
	CInterlockedInt m_nTotalBlocksPlayed;

	/// ----- THE FOLLOWING ARE CONTROLLED BY ONE MUTEX ----------
	CAudioIckyUpsampler m_upsampler;
	/// ----------------------------------------------------------


	class CAudioPS3DMAThread : public CThread
	{
	public:
		CAudioPS3DMAThread( );
		bool Setup( CAudioPS3LibAudio *pOwner );

		CInterlockedInt m_eHalt; // owner can signal halt with this by setting it to TRUE
		
		job_sndupsampler::JobDescriptor_t * m_pSndUpsamplerJobDescriptor;		
		job_sndupsampler::JobOutput_t * GetSndUpsamplerJobOutput() { return ( job_sndupsampler::JobOutput_t * )( m_pSndUpsamplerJobDescriptor + 1 ); }
		job_sndupsampler::JobParams_t * GetSndUpsamplerJobParams() { return job_sndupsampler::GetJobParams( m_pSndUpsamplerJobDescriptor ); }
		
		void OnVjobsInit( VJobsRoot* pRoot );
		void OnVjobsShutdown();
	protected:
		virtual int Run();
		virtual void OnExit(); // clean up
		int OutputData( int nCurrentAudioBlockReadIndex ) RESTRICT; // dump sounds from the upsampler into the DAC. return number of blocks written.
		
		int m_nFilledBuffers; // the number of block that are full of data but haven't been played yet.
		int m_nPreviouslySeenNextToUpdateBufferIndex;
		
		// the event queue (for counting how many blocks get played -- we could also use this for async sound writing) 
		sys_event_queue_t m_AudioEventQueue;
		sys_ipc_key_t m_AudioIPCKey;
		CAudioPS3LibAudio *m_pOwner;

		enum { STACKSIZE=4095 };
	};

	// a tiny thread whose only job in existence is to feed libaudio from our upsampling buffer.
	// it has to be this way because the system only caches two events from libaudio, and so
	// we can't know how many blocks have been played if we happen to be polled less than once
	// every ten milliseconds. 
	static CAudioPS3DMAThread s_LibAudioThread;
	friend class CAudioPS3DMAThread;

	CEngineVoicePs3 m_VoiceData;
};

CAudioPS3LibAudio *CAudioPS3LibAudio::m_pSingleton = NULL;


CAudioPS3LibAudio::CAudioPS3DMAThread::CAudioPS3DMAThread() 
	: m_nFilledBuffers(0)
	, m_eHalt(false)
	, m_pSndUpsamplerJobDescriptor( NULL )
{
}


// [Main] called from the context of main thread
//
void CAudioPS3LibAudio::CAudioPS3DMAThread::OnVjobsInit( VJobsRoot* pRoot )
{
	Assert( !m_pSndUpsamplerJobDescriptor );
	m_pSndUpsamplerJobDescriptor = ( job_sndupsampler::JobDescriptor_t * )MemAlloc_AllocAligned( sizeof( job_sndupsampler::JobDescriptor_t ) + sizeof( job_sndupsampler::JobOutput_t ), 128 );
	m_pSndUpsamplerJobDescriptor->header = *pRoot->m_pJobSndUpsampler;
	m_pSndUpsamplerJobDescriptor->header.useInOutBuffer = 0;
	job_sndupsampler::JobParams_t * pJobParams = job_sndupsampler::GetJobParams( m_pSndUpsamplerJobDescriptor );
	pJobParams->m_flMaxSumStereo = 500;
	pJobParams->m_nDebuggerBreak = 0;
	pJobParams->m_deviceChannels = m_pOwner->m_deviceChannels;
	Assert( pJobParams->m_deviceChannels == m_pOwner->m_deviceChannels );
	pJobParams->m_bIsSurround = m_pOwner->IsSurround();
	pJobParams->m_nCellAudioBlockSamplesLog2 = COMPILE_TIME_LOG2( CELL_AUDIO_BLOCK_SAMPLES );

	cellAudioCreateNotifyEventQueue( &m_AudioEventQueue, &m_AudioIPCKey );
	cellAudioSetNotifyEventQueue( m_AudioIPCKey );

	Start(STACKSIZE);
}


// [Main] called from the context of main thread
//
void CAudioPS3LibAudio::CAudioPS3DMAThread::OnVjobsShutdown()
{
	m_eHalt = true ;
	Join();

	cellAudioRemoveNotifyEventQueue( m_AudioIPCKey );
	sys_event_queue_destroy( m_AudioEventQueue, SYS_EVENT_QUEUE_DESTROY_FORCE  );

	Assert( m_pSndUpsamplerJobDescriptor );
	if( m_pSndUpsamplerJobDescriptor )
	{
		m_pSndUpsamplerJobDescriptor = NULL;

		job_sndupsampler::JobOutput_t * pJobOutput = GetSndUpsamplerJobOutput();	
		MemAlloc_FreeAligned( m_pSndUpsamplerJobDescriptor );
		m_pSndUpsamplerJobDescriptor = NULL;
	}
}



bool CAudioPS3LibAudio::CAudioPS3DMAThread::Setup( CAudioPS3LibAudio *pOwner )
{
	if ( m_pOwner )
	{
		AssertMsg( false, "Tried to make two CAudioPS3LibAudio s\n");
		return false;
	}
	SetName("CAudioPS3DMAThread");
	
	m_pOwner = pOwner;

	return true;
}

void CAudioPS3LibAudio::CAudioPS3DMAThread::OnExit()
{
	// NOTE: please don't destruct resources created in the context of main thread here; Setup() is called in main thread; OnExit is called from the audio thread.
	//       destroy those resources where you join the thread , the same way they are created before the thread Start() is called
	// NOTE: we need to shutdown AFTER exiting the thread, to avoid accessing thread structure during/after shutdown
	Assert( m_pSndUpsamplerJobDescriptor );
}



int CAudioPS3LibAudio::CAudioPS3DMAThread::Run() 
{
	// this needs to be a high priority thread since it is realtime
	SetPriority( 5 );
	m_nPreviouslySeenNextToUpdateBufferIndex = *(m_pOwner->m_pReadIndexAddr);
	while ( !m_eHalt )
	{
		// wait for the audio engine to signal that it's mixed out a buffer
		// and is ready for another
		sys_event_t audioevent;
		int waitresult = sys_event_queue_receive( m_AudioEventQueue, &audioevent, 5000000 );
		if ( waitresult == ETIMEDOUT )
		{
			// warnings are commented out here because CUtlString::Format() allocates a 4096byte buffer and thereby blows
			// the thread stack!
			// Warning("Went five seconds without a libaudio event, wtf?\n");
			continue;
		}
		else if ( waitresult != CELL_OK )
		{
			Error("Failed to wait on libaudio buffer, error %x\n", waitresult );
			return -1;
		}

		// down here, we successfully received an event
		// make sure we still haven't been halted
		if ( m_eHalt )
			break;

		int nextBufferToBeRead = *(m_pOwner->m_pReadIndexAddr);
		if ( nextBufferToBeRead != (m_nPreviouslySeenNextToUpdateBufferIndex + 1) % CELLAUDIO_PORT_BUFFER_BLOCKS )
		{
			// warnings are commented out here because CUtlString::Format() allocates a 4096byte buffer and thereby blows
			// the thread stack!
			// Warning( "Missed an audio buffer being sent through! previously seen was %d now is %d\n",
			//	m_nPreviouslySeenNextToUpdateBufferIndex, nextBufferToBeRead );
			// flush everything and start over
			m_nFilledBuffers = 0;
		}
		else
		{
			// one more buffer has been eaten
			m_nFilledBuffers = m_nFilledBuffers <= 0 ? 0 : m_nFilledBuffers - 1;
		}

		// let's avoid a sys call in non-debug configurations
		if ( !ENABLE_SOUND_DEBUG || ( CPS3FrontPanelLED::GetSwitches() & CPS3FrontPanelLED::kPS3SWITCH2 ) == 0 )
		{
			int bufsRead = OutputData( nextBufferToBeRead );
			// Msg("Emited %d bufs\n", bufsRead);
			m_nFilledBuffers += bufsRead;
		}
		else
		{
			// An example of a dead simple sine wave test:
			m_pOwner->m_upsampler.Flush();
			float * pOut = m_pOwner->GetAddressForBlockIdx( nextBufferToBeRead );
			// brain-dead test, just a low note
			for ( int n = 0 ; n <  CELL_AUDIO_BLOCK_SAMPLES ; ++n )
			{
				float f = 0.4f * sin( (4.0 / (double)CELL_AUDIO_BLOCK_SAMPLES) * M_PI * n );
				pOut[n*2 + 0] = f;
				pOut[n*2 + 1] = f;
				m_nFilledBuffers = 1;
			}
/*
			char buffer[200];
			int len = V_snprintf( buffer, sizeof( buffer) , "Snd %x, @%p\n", CELL_AUDIO_BLOCK_SAMPLES * 2 * sizeof( float ), pOut );
			uint wrote;
			sys_tty_write( SYS_TTYP3, buffer, len, &wrote );
*/
		}
		m_nPreviouslySeenNextToUpdateBufferIndex = nextBufferToBeRead; // *(m_pOwner->m_pReadIndexAddr); // reread the volatile

		// Update the voice buffer here too
		Audio_GetXVoice()->PlayPortInterruptHandler();
	}
	return 0;
}


ConVar sound_on_spu( "sound_on_spu", "1" );
#define AssertRt(X) //while(!(X)){DebuggerBreak();break;}


uint g_nOutputDataBlocksWritten = 0;

ConVar sound_sleep_rt_thread( "sound_sleep_rt_thread", "30" );

int CAudioPS3LibAudio::CAudioPS3DMAThread::OutputData( int nCurrentAudioBlockReadIndex ) RESTRICT 
{
	Assert( m_pSndUpsamplerJobDescriptor ); // the SPU job code must be loaded/streamed in already
	// grab the upsampler mutex
	int availableInputSamples = m_pOwner->m_upsampler.OutputSamplesAvailable(); // at the 48khz rate
	if( availableInputSamples <= 0 )
	{
		return 0;
	}

	int nextBlockIdxToWrite = ( nCurrentAudioBlockReadIndex + m_nFilledBuffers ) % CELLAUDIO_PORT_BUFFER_BLOCKS;
	float volumeFactor = S_GetMasterVolume() / 32767.0f;

	// flush as many samples out to the device as possible
	// now output as many full audio blocks @48khz as we can

	int emptyblocks = CELLAUDIO_PORT_BUFFER_BLOCKS - m_nFilledBuffers;

	int availableInputBlocks = availableInputSamples / CELL_AUDIO_BLOCK_SAMPLES; // rounds down 

	// don't emit more blocks than the DAC has room for
	availableInputBlocks = MIN( availableInputBlocks, emptyblocks );
	
	if( availableInputBlocks <= 0 )
	{
		return 0;
	}
	
	uint blocksWrit = 0;
	if( int nSoundOnSpu = sound_on_spu.GetInt() )
	{
		job_sndupsampler::JobOutput_t * pJobOutput = GetSndUpsamplerJobOutput();
		job_sndupsampler::JobParams_t * pJobParams = job_sndupsampler::GetJobParams( m_pSndUpsamplerJobDescriptor );
		float * pSpuResult = ENABLE_SOUND_DEBUG && nSoundOnSpu > 1 ? m_pOwner->m_pDebugBuffer : NULL;
		uint n44kSamplesAvailable = m_pOwner->m_upsampler.m_ringCount;
		{
			MICRO_PROFILE( g_mpOutputData );
			AssertRt( pJobParams->m_deviceChannels == m_pOwner->m_deviceChannels );
			
			pJobParams->m_eaPortBufferAddr     = ( pSpuResult ? (sys_addr_t)pSpuResult : m_pOwner->m_pPortBufferAddr ); // may change when user switches surround <-> stereo
			
			pJobParams->m_availableInputBlocks = availableInputBlocks;
			pJobParams->m_volumeFactor         = volumeFactor;
			pJobParams->m_nextBlockIdxToWrite  = nextBlockIdxToWrite;
			pJobParams->m_fractionalSamples    = m_pOwner->m_upsampler.m_fractionalSamples;
			pJobParams->m_flBackChannelMultipler = snd_ps3_back_channel_multiplier.GetFloat();
			
			if( ENABLE_SOUND_DEBUG  )
			{
				// let's avoid a sys call (sys_gpio_get) in non-debug configurations
				if( ( CPS3FrontPanelLED::GetSwitches() & CPS3FrontPanelLED::kPS3SWITCH3 ) == 0 )
				{
					pJobParams->m_nDebuggerBreak &= 0x7F;
				}
				else
				{
					pJobParams->m_nDebuggerBreak |= 0x80;
				}
			}
			
			uint nSampleSizeIn, nSampleSizeOutLog2;
			// align the number of samples so that we don't have to deal with unaligned blocks

			if( m_pOwner->m_upsampler.IsSurround() )
			{
				nSampleSizeOutLog2 = COMPILE_TIME_LOG2( sizeof( libaudio_sample_surround_t ) );
				nSampleSizeIn = sizeof( job_sndupsampler::surroundsample_t );
			}
			else
			{
				nSampleSizeOutLog2 = COMPILE_TIME_LOG2( sizeof( libaudio_sample_surround_t ) );
				nSampleSizeIn = sizeof( job_sndupsampler::stereosample_t );
			}
			
			CDmaListConstructor dmaConstructor( m_pSndUpsamplerJobDescriptor->workArea.dmaList );
			int nRingHead = m_pOwner->m_upsampler.m_ringHead;
			uintp ea44kSamplesIn = m_pOwner->m_upsampler.m_ea44kSamplesIn;
			uint eaInputSamplesBegin = ea44kSamplesIn + ( nRingHead * nSampleSizeIn );
			pJobParams->m_eaInputSamplesBegin0xF = uint8( eaInputSamplesBegin & 0xF );

			pJobParams->m_ringHead             = nRingHead;
			pJobParams->m_ringCount            = n44kSamplesAvailable;

			if( nRingHead + n44kSamplesAvailable > CAudioIckyUpsampler::BUFFERSIZE )
			{
				// split into 2 transactions
				dmaConstructor.AddInputDmaLargeRegion( (void*)( eaInputSamplesBegin & -16 ), (void*)( ea44kSamplesIn + ( CAudioIckyUpsampler::BUFFERSIZE * nSampleSizeIn ) ) );
				// this is always aligned
				dmaConstructor.AddInputDmaLarge( AlignValue( ( nRingHead + n44kSamplesAvailable - CAudioIckyUpsampler::BUFFERSIZE ) * nSampleSizeIn, 16 ), ( void * )ea44kSamplesIn );
			}
			else
			{
				dmaConstructor.AddInputDmaLargeUnalignedRegion( (void*)eaInputSamplesBegin, (void*)( eaInputSamplesBegin + ( n44kSamplesAvailable * nSampleSizeIn ) ) );
			}
			
			m_pSndUpsamplerJobDescriptor->header.sizeOut = 
					sizeof( job_sndupsampler::JobOutput_t ) // job output structure
				+ ( ( 2 * CELL_AUDIO_BLOCK_SAMPLES ) << nSampleSizeOutLog2 ) // output samples
				+   16									   // potential misalignment of output samples ( stereo samples are 8 bytes each )
				;
			dmaConstructor.FinishInBuffer( &m_pSndUpsamplerJobDescriptor->header, pJobParams );
								    
			pJobOutput->m_nBlockWritten = 0xFFFFFFFF; // invalid value
			
			// NOTE: if we need to wait for the job, we need to push it with CELL_SPURS_JOBQUEUE_FLAG_SYNC_JOB flag!
			if( int nError = m_pOwner->m_pRoot->m_queuePortSound.pushJob( &m_pSndUpsamplerJobDescriptor->header, sizeof( job_sndupsampler::JobDescriptor_t ), 0, 0 ) )
			{
				Warning( "Cannot start Sound job, error 0x%X\n", nError );
			}
		}
		//m_pOwner->m_pRoot->m_queuePortSound.sync( 0 );	// NOTE! if we use sync() here, we need to push job with CELL_SPURS_JOBQUEUE_FLAG_SYNC_JOB flag!
		sys_timer_usleep( sound_sleep_rt_thread.GetInt() );
		while( *(volatile uint32*)( &pJobOutput->m_nBlockWritten ) == 0xFFFFFFFF ); // must be synchronized by now
		{
			sys_timer_usleep( 100 ); // wait for the job to complete; we aren't in a hurry
		}

		AssertRt( n44kSamplesAvailable > pJobOutput->m_n44kSamplesConsumed ); // we must never consume the last sample, because we won't be able to extrapolate the value
		AssertRt( m_pOwner->m_upsampler.m_ringCount >= n44kSamplesAvailable );
		AUTO_LOCK( m_pOwner->m_upsampler.m_mutex ); // TODO: get rid of this lock, use head-tail counters instead of head+count
		if( !m_pOwner->m_upsampler.m_ringCount )
		{
			// if the ring count was flushed, don't do any checks - it's useless
			blocksWrit = pJobOutput->m_nBlockWritten;
		}
		else if( ENABLE_SOUND_DEBUG && pSpuResult ) 
		{
			int nRingCountBegin = m_pOwner->m_upsampler.m_ringCount, nRingHeadBegin = m_pOwner->m_upsampler.m_ringHead;
			// STEREO!
			while ( availableInputBlocks>0 )
			{
				// all sorts of obvious ways to optimize this (explicit iterators, VMX, SPU, etc)
				float * RESTRICT pWriteDest = (m_pOwner->GetAddressForBlockIdx( nextBlockIdxToWrite ));
				float *buffer = new float[CELL_AUDIO_BLOCK_SAMPLES * sizeof( libaudio_sample_surround_t ) / sizeof( float )];

				AssertRt( fabsf( m_pOwner->m_upsampler.m_fractionalSamples - pJobOutput->m_trace[blocksWrit].m_fractionalSamplesBefore ) < 1e-2f );
				int nRingHeadBefore = m_pOwner->m_upsampler.m_ringHead, nRingCountBefore = m_pOwner->m_upsampler.m_ringCount;
				float fractionalSamplesBefore = m_pOwner->m_upsampler.m_fractionalSamples;
				
				if( ENABLE_SOUND_DEBUG )
				{
					m_pOwner->m_upsampler.m_ringHead = nRingHeadBefore;
					m_pOwner->m_upsampler.m_ringCount = nRingCountBefore;
					m_pOwner->m_upsampler.m_fractionalSamples = fractionalSamplesBefore;
				}
				
				int samplesWritten = m_pOwner->IsSurround() ? 
					m_pOwner->m_upsampler.ExcreteSurround( CELL_AUDIO_BLOCK_SAMPLES, reinterpret_cast<libaudio_sample_surround_t *>(buffer), volumeFactor ) :
					m_pOwner->m_upsampler.ExcreteStereo( CELL_AUDIO_BLOCK_SAMPLES, reinterpret_cast<libaudio_sample_surround_t *>(buffer), volumeFactor ) ;
				AssertRt( samplesWritten == CELL_AUDIO_BLOCK_SAMPLES );
				int nSamplesConsumedSoFar = nRingCountBegin - m_pOwner->m_upsampler.m_ringCount;
				AssertRt( nSamplesConsumedSoFar == pJobOutput->m_trace[blocksWrit].m_n44kSamplesConsumed );
				
				float * pSpuResultBlock = pSpuResult + ( pWriteDest - ( float * )m_pOwner->m_pPortBufferAddr );
				for( uint i = 0; i < CELL_AUDIO_BLOCK_SAMPLES * m_pOwner->m_deviceChannels;  )
				{
					if( fabsf( buffer[i] - pSpuResultBlock[i] ) > 1e-6f )
					{
						DebuggerBreakIfDebugging();
						pJobOutput->m_nBlockWritten = 0xFFFFFFFF; // invalid value
						int nError = m_pOwner->m_pRoot->m_queuePortSound.pushJob( &m_pSndUpsamplerJobDescriptor->header, sizeof( job_sndupsampler::JobDescriptor_t ), 0, 0 );
						while( pJobOutput->m_nBlockWritten == 0xFFFFFFFF )
							continue;
						continue;
					}
					++i;
				}

				--availableInputBlocks;
				nextBlockIdxToWrite = (nextBlockIdxToWrite + 1)%CELLAUDIO_PORT_BUFFER_BLOCKS;
				++blocksWrit;
				delete []buffer;
			}
			
			AssertRt( blocksWrit == pJobOutput->m_nBlockWritten );
			AssertRt( m_pOwner->m_upsampler.m_ringHead == pJobOutput->m_ringHead );
			AssertRt( fabsf( m_pOwner->m_upsampler.m_fractionalSamples - pJobOutput->m_fractionalSamples ) < 1e-6f );
		}
		else
		{
			blocksWrit = pJobOutput->m_nBlockWritten;
			m_pOwner->m_upsampler.m_ringCount -= pJobOutput->m_n44kSamplesConsumed;
			m_pOwner->m_upsampler.m_ringHead = pJobOutput->m_ringHead;
			m_pOwner->m_upsampler.m_fractionalSamples = pJobOutput->m_fractionalSamples;
		}		
	}
	else
	{
		// STEREO!
		AUTO_LOCK( m_pOwner->m_upsampler.m_mutex ); // TODO: get rid of this lock, use head-tail counters instead of head+count
		MICRO_PROFILE( g_mpOutputData );
		g_flSumStereo = 0;
		while ( availableInputBlocks>0 )
		{
			// all sorts of obvious ways to optimize this (explicit iterators, VMX, SPU, etc)
			float * RESTRICT pWriteDest = (m_pOwner->GetAddressForBlockIdx( nextBlockIdxToWrite ));

			int samplesWritten = m_pOwner->IsSurround() ? 
				m_pOwner->m_upsampler.ExcreteSurround( CELL_AUDIO_BLOCK_SAMPLES, reinterpret_cast<libaudio_sample_surround_t *>(pWriteDest), volumeFactor ) :
				m_pOwner->m_upsampler.ExcreteStereo( CELL_AUDIO_BLOCK_SAMPLES, reinterpret_cast<libaudio_sample_surround_t *>(pWriteDest), volumeFactor ) ;
			AssertRt( samplesWritten == CELL_AUDIO_BLOCK_SAMPLES );

			--availableInputBlocks;
			nextBlockIdxToWrite = (nextBlockIdxToWrite + 1)%CELLAUDIO_PORT_BUFFER_BLOCKS;
			++blocksWrit;
		}
		
		if( ENABLE_SOUND_DEBUG && g_flSumStereo > GetSndUpsamplerJobParams()->m_flMaxSumStereo )
		{
			char buffer[200];
			int len = V_snprintf( buffer, sizeof( buffer) , "PPU Stereo %g\n", g_flSumStereo );
			uint wrote;
			sys_tty_write( SYS_TTYP3, buffer, len, &wrote );
		}
	}
	m_pOwner->m_nTotalBlocksPlayed += blocksWrit;

	g_nOutputDataBlocksWritten += blocksWrit;		

	return blocksWrit;
}

CAudioPS3LibAudio::CAudioPS3DMAThread CAudioPS3LibAudio::s_LibAudioThread;

bool CAudioPS3LibAudio::s_bCellAudioStateInitialized = false;
CUtlVectorConservative<CellAudioOutState> CAudioPS3LibAudio::s_vCellAudioOutputDeviceStates(0,1);

// NULL out the pointers so that failures are more immediate, evident
CAudioPS3LibAudio::CAudioPS3LibAudio( void ) : m_deviceChannels( 0 )
	, m_nPortNum( -1 )
	, m_nPhysicalOutChannels( 0 )
	, m_pPortBufferAddr( NULL )
	, m_pReadIndexAddr( NULL )
	, m_nTotalBlocksPlayed( 0 )
{
//	AssertMsg( false, "Please implement CAudioPS3LibAudio\n" ); 
}



//-----------------------------------------------------------------------------
// Create XAudio Device
//-----------------------------------------------------------------------------
IAudioDevice *Audio_CreatePS3AudioDevice( bool bInitVoice )
{
	MEM_ALLOC_CREDIT();

	if ( CommandLine()->CheckParm( "-nosound" ) )
	{
		// respect forced lack of audio
		return NULL;
	}

	if ( !CAudioPS3LibAudio::m_pSingleton )
	{
		CAudioPS3LibAudio::m_pSingleton = new CAudioPS3LibAudio;
		if ( !CAudioPS3LibAudio::m_pSingleton->Init() )
		{
			AssertMsg( false, "Failed to init CAudioPS3LibAudio\n" );
			delete CAudioPS3LibAudio::m_pSingleton;
			CAudioPS3LibAudio::m_pSingleton = NULL;
		}
	}

	// need to support early init of XAudio (for bink startup video) without the voice
	// voice requires matchmaking which is not available at this early point
	// this defers the voice init to a later engine init mark
	if ( bInitVoice && CAudioPS3LibAudio::m_pSingleton )
	{
		CAudioPS3LibAudio::m_pSingleton->GetVoiceData()->VoiceInit();
	}

	return CAudioPS3LibAudio::m_pSingleton;
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CAudioPS3LibAudio::~CAudioPS3LibAudio( void )
{
	if ( m_deviceChannels >  0 )
	{
		AssertMsg( false, "CAudioPS3LibAudio() called without being Shutdown() first! Initiating emergency purge.\n" );
		Shutdown();
	}

	g_pVJobs->Unregister( this );
	m_pSingleton = NULL;
}

void CAudioPS3LibAudio::QuerySystemAudioConfiguration( bool bForceRefreshCache )
{
	if ( !s_bCellAudioStateInitialized || bForceRefreshCache )
	{
		// flush the data we've got if necessary
		s_vCellAudioOutputDeviceStates.RemoveAll();

		const int numDevices = cellAudioOutGetNumberOfDevice( CELL_AUDIO_OUT_PRIMARY );
		if ( numDevices < 0 )
		{
			// error!
			s_vCellAudioOutputDeviceStates.RemoveAll();
			AssertMsg1( false, "cellAudioOutGetNumberOfDevice failed: %x\n", numDevices );
			return;
		}

		s_vCellAudioOutputDeviceStates.EnsureCount(numDevices); // set the count of output devices
		for ( int d = 0 ; d < s_vCellAudioOutputDeviceStates.Count() ; ++d )
		{
			int suc = cellAudioOutGetState( CELL_AUDIO_OUT_PRIMARY, d, &s_vCellAudioOutputDeviceStates[d] );
			AssertMsg2( suc == CELL_OK, "cellAudioOutGetState(%d) failed: %x\n", d, suc );
		}

		s_bCellAudioStateInitialized = true;
	}
}

// This code is from Bink's examples - they are configuring the HW mixer
// This may resolve the issue with center being lost when we are in stereo.
// Note that we do not detect DTS. Maybe we should?
static int init_audio_hardware( int minimum_chans, int &nPhysicalChannels )
{
  int ret;
  int ch_pcm;
  int ch_bit;
  CellAudioOutConfiguration a_config;

  memset( &a_config, 0, sizeof( CellAudioOutConfiguration ) );

  // Note that we would probably need to do something smarter than what this function is doing.
  // It seems we should test these cases in this order (highest to lowest quality):
  // Do we have enough channels for:
  //   - LPCM
  //   - DTS
  //   - AC3
  // If not, then we have to downmix:
  //   - If from 8 to 6 (type B downmixer)
  //       - Try LPCM encoder
  //       - Otherwise try DTS encoder	- Is this necessary?
  //       - Then the AC3 encoder		- Is this necessary?
  //   - If it does not work (or from 8 to 2, 6 to 2) - use Type A downmixer:
  //	   - Try PCM encoder

  // Current code is doing:
  //   - Do we have enough channels for LPCM
  //	   If yes, set it, done.
  //       If not
  //           Do we have support for 6 channels?
  //		     If yes, then set downmixer from 8 to 6 in LPCM mode.
  //             If not then we support only 2 channels.
  //             Do we have support for Dolby (any number of channels)?
  //				If yes, do we have enough support for requested channels?
  //					If yes, set it, done.
  //					If not, set the downmixer to 5.1, if successful, done.
  //				If not (or Dolby downmixer to 5.1 failed), downmix to 2 with LPCM.

  // first lets see how many pcm output channels we have
  ch_pcm = cellAudioOutGetSoundAvailability( CELL_AUDIO_OUT_PRIMARY,
                                             CELL_AUDIO_OUT_CODING_TYPE_LPCM,
                                             CELL_AUDIO_OUT_FS_48KHZ, 0 );
  nPhysicalChannels = ch_pcm;

  if ( ch_pcm >= minimum_chans )
  {
    a_config.channel = ch_pcm;
    a_config.encoder = CELL_AUDIO_OUT_CODING_TYPE_LPCM;
    a_config.downMixer = CELL_AUDIO_OUT_DOWNMIXER_NONE; /* No downmixer is used */
    cellAudioOutConfigure( CELL_AUDIO_OUT_PRIMARY, &a_config, NULL, 0 );
    ret = ch_pcm;
  }
  else
  {
    switch ( ch_pcm )
    {
      case 6:
        // this means we asked for 8 channels, but only 6 are available
        //   so, we'll turn on the 7.1 to 5.1 downmixer.
        a_config.channel = 6; 
        a_config.encoder = CELL_AUDIO_OUT_CODING_TYPE_LPCM;
        a_config.downMixer = CELL_AUDIO_OUT_DOWNMIXER_TYPE_B; 
        if ( cellAudioOutConfigure( CELL_AUDIO_OUT_PRIMARY, &a_config, NULL, 0 ) != CELL_OK )
        {
          return 0; // error - the downmixer didn't init
        }
        ret = 8;
        break;

    case 2:
      // ok, this means they asked for multi-channel out, but only stereo
      //   is supported.  we'll try Dolby digital first and then the downmixer
            
	  // Support for DTS - disabled as we can't test it right now
#if 0
		ch_bit = cellAudioOutGetSoundAvailability( CELL_AUDIO_OUT_PRIMARY,
			CELL_AUDIO_OUT_CODING_TYPE_DTS,
			CELL_AUDIO_OUT_FS_48KHZ, 0 );
		if ( ch_bit > 0 ) 
		{
			a_config.channel = ch_bit;
			a_config.encoder = CELL_AUDIO_OUT_CODING_TYPE_DTS;
			if ( ch_bit >= minimum_chans )
			{
				// we have enough channels to support their minimum
				a_config.downMixer = CELL_AUDIO_OUT_DOWNMIXER_NONE;
				ret = ch_bit;
			} 
			else 
			{
				// we don't have enough channels to support their minimum, so use the downmixer
				a_config.downMixer = CELL_AUDIO_OUT_DOWNMIXER_TYPE_B;			// Downmix to 5.1 channels
				ret = 8;
			}

			if ( cellAudioOutConfigure( CELL_AUDIO_OUT_PRIMARY, &a_config, NULL, 0 ) == CELL_OK )
			{
				nPhysicalChannels = ch_bit;
				break;
			}

			// if we got here the DTS encoder didn't init, so fall through to Dolby encoder
		}
#endif

      ch_bit = cellAudioOutGetSoundAvailability( CELL_AUDIO_OUT_PRIMARY,
                                                 CELL_AUDIO_OUT_CODING_TYPE_AC3,
                                                 CELL_AUDIO_OUT_FS_48KHZ, 0 );
      if ( ch_bit > 0 ) 
      {
        a_config.channel = ch_bit;
        a_config.encoder = CELL_AUDIO_OUT_CODING_TYPE_AC3;
        if ( ch_bit >= minimum_chans )
        {
          // we have enough channels to support their minimum
          a_config.downMixer = CELL_AUDIO_OUT_DOWNMIXER_NONE;
          ret = ch_bit;
        } 
        else 
        {
          // we don't have enough channels to support their minimum, so use the downmixer
          a_config.downMixer = CELL_AUDIO_OUT_DOWNMIXER_TYPE_B;			// Downmix to 5.1 channels
          ret = 8;
        }

        if ( cellAudioOutConfigure( CELL_AUDIO_OUT_PRIMARY, &a_config, NULL, 0 ) == CELL_OK )
        {
		  nPhysicalChannels = ch_bit;
		  break;
        }

        // if we got here the Dolby encoder didn't init, so fall through to downmixing to stereo
      }

      a_config.channel = 2; 
      a_config.encoder   = CELL_AUDIO_OUT_CODING_TYPE_LPCM;
      a_config.downMixer = CELL_AUDIO_OUT_DOWNMIXER_TYPE_A;				// Downmix to 2 channels
      if ( cellAudioOutConfigure( CELL_AUDIO_OUT_PRIMARY, &a_config, NULL, 0 ) != CELL_OK )
      {
        return 0; // error - downmixer didn't work
      }

      ret = 7; // downmixer does 7.0 to 2.0 downmixing...
      break;

    default:
      // some other weird case that we don't understand
      return 0;
    }
  }

/*
  // wait for the device to enable (not necessary anymore?)
  {
    CellAudioOutState a_state;

    do
    {
      if ( cellAudioOutGetState( CELL_AUDIO_OUT_PRIMARY, 0, &a_state ) != CELL_OK )
      {
        return( ret );	// If that failed, we are still returning the expected value. This call is not critical. 
      }
      sys_timer_usleep( 5000 );
    }
    while ( a_state.state != CELL_AUDIO_OUT_OUTPUT_STATE_ENABLED );
  }
*/

  // turn off copy protection stupidness
  cellAudioOutSetCopyControl( CELL_AUDIO_OUT_PRIMARY,
                              CELL_AUDIO_OUT_COPY_CONTROL_COPY_FREE );


  return( ret );
}

bool CAudioPS3LibAudio::Init( void ) 
{
	if ( this == CAudioPS3LibAudio::m_pSingleton ) // guard against possible multiples of this thing
	{
		int success = cellAudioInit();
		if ( success != CELL_OK )
		{
			Warning( "Could not initalize PS3 audio, error code: %x\n", success );
			return false;
		}
	}

	// get relevant info from the OS
	QuerySystemAudioConfiguration( false );

	// work out our surround sound mode
	// assumption: we're only looking at CELL_AUDIO_OUT_PRIMARY
	// also: we could query headphones properly from some kind of UI setting
	m_bHeadphone = snd_surround.GetInt() == SURROUND_HEADPHONES; // in case we set this from the UI

	// number of physical out channels connected
	// (remember libaudio only logically supports 2 or 8 in the ring buffer)
	// also this is an enum rather than a direct count
	const int eOutputChannels = s_vCellAudioOutputDeviceStates[0].soundMode.channel;
	switch ( eOutputChannels ) 
	{
	case CELL_AUDIO_OUT_CHNUM_2:
	default:
		m_nPhysicalOutChannels = 2;
		break;
	case CELL_AUDIO_OUT_CHNUM_4:
		m_nPhysicalOutChannels = 4;  // quad
		break;
	case CELL_AUDIO_OUT_CHNUM_6:
		m_nPhysicalOutChannels = 6; // 5.1
		break;
	case CELL_AUDIO_OUT_CHNUM_8:
		m_nPhysicalOutChannels = 8; // 7.1
		break;
	}

	int nPhysicalChannels = m_nPhysicalOutChannels;
	int nResult = init_audio_hardware( 6, nPhysicalChannels );		// Ask for 5.1, so it can down mix to stereo if necessary
																	// Then engine will support the right format regardless of what the down mixer can support
	if ( nResult != 0 )
	{
		m_nPhysicalOutChannels = nPhysicalChannels;					// We could initialize the down mixer, lets use the physical out channels.
	}

	CellAudioPortParam aparam;
	float flBackChannelMultipler = 0.0f;
	if ( m_nPhysicalOutChannels > 2 )
	{
		snd_surround.SetValue( SURROUND_DIGITAL7DOT1 ); // all internal mixing is 8 channels; downsampling occurs in hardware

		m_bSurround = m_bSurroundCenter = true;
		m_deviceChannels = 8; // you can only have stereo or 7.1
		aparam.nChannel = CELL_AUDIO_PORT_8CH;

		// If we are in 7.1, we want to fill the back channel with the same value as the side channels
		// However, if we are in 5.1, we don't want to fill it in case the downmixer uses it and creates unwanted side-effects.
		if ( m_nPhysicalOutChannels > 6 )
		{
			flBackChannelMultipler = 1.0f;
		}
	}
	else
	{
		// Because we have the down mixer for Bink, we are going to output 7.1 even for stereo only
		// That way Bink can output 5.1 normally, and the down mixer will change it to stereo.
		// Same for the engine, however the engine is going to only output stereo values.

		snd_surround.SetValue( m_bHeadphone ? SURROUND_HEADPHONES : SURROUND_STEREO );

		m_bSurround = m_bSurroundCenter = false;
		m_deviceChannels = 8;
		aparam.nChannel = CELL_AUDIO_PORT_8CH;
	}
	snd_ps3_back_channel_multiplier.SetValue( flBackChannelMultipler );

	// open the audio port
	COMPILE_TIME_ASSERT( CELLAUDIO_PORT_BUFFER_BLOCKS == 8 || CELLAUDIO_PORT_BUFFER_BLOCKS == 16 ); // You're only allowed to have either 8 or 16 blocks in the ring buffer!

	aparam.nBlock = CELLAUDIO_PORT_BUFFER_BLOCKS == 16 ? CELL_AUDIO_BLOCK_16 : CELL_AUDIO_BLOCK_8; // you can have 8 or 16 blocks in the ring buffer. 16 gives us about 80ms of buffer.
	aparam.attr = 0; // no special attributes
	aparam.level = 1; // this is actually ignored without setting  CELL_AUDIO_PORTATTR_INITLEVEL to the attributes above.
	
	int success = cellAudioPortOpen( &aparam, &m_nPortNum );
	if ( success != CELL_OK )
	{
		Warning("Could not initialize libaudio, error code %x\n", success );
	}
	else
	{
		DevMsg( "PS3 libaudio device initialized:\n" );
		DevMsg( "   %d channel(s)\n"
			"   %d bits/sample\n"
			"   %d samples/sec\n", DeviceChannels(), DeviceSampleBits(), DeviceDmaSpeed() );
	}

	// get the pointers to the start and next-block-indicator of the ring buffer
	CellAudioPortConfig configinfo;
	cellAudioGetPortConfig( m_nPortNum,	&configinfo	);

	m_pPortBufferAddr = configinfo.portAddr;
	uint nPortBufferSize = GetPortBufferSize();
	m_pDebugBuffer = ENABLE_SOUND_DEBUG ? (float*)MemAlloc_AllocAligned( nPortBufferSize, 16 ) : NULL ;
	m_pReadIndexAddr = (uint64_t*)configinfo.readIndexAddr;

	m_upsampler.Init( m_bSurround );
	s_LibAudioThread.Setup( this );
	g_pVJobs->Register( this );

	cellAudioPortStart( m_nPortNum );
	return success == CELL_OK;
}

int	CAudioPS3LibAudio::DeviceChannels( void )		
{ 
	return m_deviceChannels;
}

int	CAudioPS3LibAudio::DeviceSampleBits( void )	
{ 
	return sizeof(float)*8; // the only sound format is single precision floats. PERIOD.
}

int	CAudioPS3LibAudio::DeviceSampleBytes( void )	
{
	return sizeof(float); 
}

int	CAudioPS3LibAudio::DeviceDmaSpeed( void )		
{ 
	return SOUND_DMA_SPEED; // the upsampler fakes 44.1
}

int	CAudioPS3LibAudio::DeviceSampleCount( void )	
{ 
	return CELL_AUDIO_BLOCK_SAMPLES * CELLAUDIO_PORT_BUFFER_BLOCKS; // each block is 256 samples long. OR ELSE.
}


inline float *CAudioPS3LibAudio::GetAddressForBlockIdx( unsigned nBlock )
{ 
	nBlock %= CELLAUDIO_PORT_BUFFER_BLOCKS;
	return reinterpret_cast<float *>( m_pPortBufferAddr + ( nBlock * sizeof(float) * CELL_AUDIO_BLOCK_SAMPLES * m_deviceChannels ) );
}

uint CAudioPS3LibAudio::GetPortBufferSize()const
{
	Assert( m_deviceChannels == 2 || m_deviceChannels == 8 );
	return CELLAUDIO_PORT_BUFFER_BLOCKS * CELL_AUDIO_BLOCK_SAMPLES * sizeof( float ) * m_deviceChannels;
}

inline float *CAudioPS3LibAudio::GetNextReadBlockAddress()
{ 
	return GetAddressForBlockIdx((uint)(*m_pReadIndexAddr));
}

void CAudioPS3LibAudio::Shutdown( void )
{
	// need to release ref to voice library
	m_VoiceData.VoiceShutdown();

	Assert( s_LibAudioThread.m_pSndUpsamplerJobDescriptor );
	// unregistering with Vjobs will cascade shutdown of all SPU-related and SPU-using resources (like the audio thread - it's using SPU resources)
	// this'll happen synchronously
	g_pVJobs->Unregister( this ); 
	Assert( !s_LibAudioThread.m_pSndUpsamplerJobDescriptor ); // must be shut down already

	int suc = cellAudioPortClose(m_nPortNum);
	AssertMsg2( suc == CELL_OK, "Couldn't close libaudio port(%d): %x\n", m_nPortNum, suc );
	m_nPortNum = -1;
	m_deviceChannels = 0;
	
	m_nPhysicalOutChannels = 0;
	m_pPortBufferAddr      = NULL;
	m_pReadIndexAddr       = NULL;
	m_nTotalBlocksPlayed   = 0;

	if ( this == CAudioPS3LibAudio::m_pSingleton )
	{
		CAudioPS3LibAudio::m_pSingleton = NULL;
		cellAudioQuit();
		s_bCellAudioStateInitialized = false;
	}
}

void CAudioPS3LibAudio::Pause( void )
{
	cellAudioPortStop( m_nPortNum );
}

void CAudioPS3LibAudio::UnPause( void )
{
	cellAudioPortStart( m_nPortNum );
}

//-----------------------------------------------------------------------------
// Fill the output buffers with silence
//-----------------------------------------------------------------------------
void CAudioPS3LibAudio::ClearBuffer( void )
{
	memset( reinterpret_cast<void *>(m_pPortBufferAddr), 0, CELLAUDIO_PORT_BUFFER_BLOCKS * CELL_AUDIO_BLOCK_SAMPLES * m_deviceChannels * sizeof(float) );
	m_upsampler.Flush();
}


//-----------------------------------------------------------------------------
// Calc the paint position
//-----------------------------------------------------------------------------
int64 CAudioPS3LibAudio::PaintBegin( float mixAheadTime, int64 soundtime, int64 paintedtime )
{
	//  soundtime = total full samples that have been played out to hardware at dmaspeed
	//  paintedtime = total full samples that have been mixed at speed
	//  endtime = target for full samples in mixahead buffer at speed	
	int mixaheadsamples = mixAheadTime * DeviceDmaSpeed();

	// the upsampler will consume as much as it can 			
	int64 endtime = paintedtime + MIN(mixaheadsamples, m_upsampler.SpaceRemaining());
	// only get samples in numbers divisible by four
	endtime = endtime & ~0x3;

	return endtime;
}


void CAudioPS3LibAudio::TransferSamples( int64 endTime ) 
{
	AUTO_LOCK( m_upsampler.m_mutex );
	// first, pull as many samples into the upsampler as we can
	int sampleCountToWrite = endTime - g_paintedtime;  // copied from xaudio version. a global? wtf?
	Assert( sampleCountToWrite <= m_upsampler.SpaceRemaining() );

	if ( IsSurround() != m_upsampler.IsSurround() )
	{
		AssertMsg2( false, "Tried to write %d channels onto a %d-channel audio port!\n", m_deviceChannels, m_upsampler.IsSurround() ? 8 : 2 );
		// abort! abort!
		m_upsampler.Flush();
		return;
	}

	if ( IsSurround() )
		m_upsampler.IngestSurround( PAINTBUFFER, REARPAINTBUFFER, CENTERPAINTBUFFER, sampleCountToWrite );
	else
		m_upsampler.IngestStereo( PAINTBUFFER, sampleCountToWrite );
}

void	 CAudioPS3LibAudio::PaintEnd( void )
{}

#if 0
#pragma message("TODO: obvious optimization opportunities")
// TODO: move to SPU  -- many obvious optimization opportunities
int CAudioPS3LibAudio::TransferStereo( const portable_samplepair_t * RESTRICT pFrontBuffer, int nSamplesToWrite, int nStartBlock )
{
	// Assert that the compiler hasn't failed to pack the structs properly
	COMPILE_TIME_ASSERT( sizeof(libaudio_sample_stereo_t) == 8 );
	COMPILE_TIME_ASSERT( sizeof(libaudio_sample_surround_t) == 32 );

	AssertMsg1( m_deviceChannels == 2, "Transferred stereo onto a %d-channel audio port\n", m_deviceChannels );
	float volumeFactor = S_GetMasterVolume() / 32767.0f;
	uint blocksWrit = 0;
	while ( nSamplesToWrite>0 )
	{
		// all sorts of obvious ways to optimize this (explicit iterators, VMX, SPU, etc)
		libaudio_sample_stereo_t * RESTRICT pWriteDest = reinterpret_cast<libaudio_sample_stereo_t *>(GetAddressForBlockIdx( nStartBlock ));
		const uint batchsize = MIN( nSamplesToWrite, CELL_AUDIO_BLOCK_SAMPLES );
		for ( uint s = 0 ; s < batchsize ; ++s )
		{
			pWriteDest[s].left = pFrontBuffer[s].left * volumeFactor;
			pWriteDest[s].right = pFrontBuffer[s].right * volumeFactor;
		}
		pFrontBuffer += batchsize;

		nSamplesToWrite -= batchsize; 
		nStartBlock = (nStartBlock + 1)%CELLAUDIO_PORT_BUFFER_BLOCKS;
		++blocksWrit;
	}
	return blocksWrit;
}

// TODO: move to SPU  -- many obvious optimization opportunities
// TODO: make a LFE channel
int CAudioPS3LibAudio::TransferSurroundInterleaved( const portable_samplepair_t * RESTRICT pFrontBuffer, 
												   const portable_samplepair_t *  RESTRICT pRearBuffer, 
												   const int *  RESTRICT pCenterBuffer, 
								int nSamplesToWrite, int nStartBlock )
{
	if ( m_deviceChannels != 8 )  //!!EMERGENCY! EMERGENCY! WILL CAUSE CRASH! 
	{
		AssertMsg1( false, "Tried to write 7.1 surround onto a %d-channel audio port!\n", m_deviceChannels );
		return TransferStereo( pFrontBuffer, nSamplesToWrite, nStartBlock );	// CRASH AVERTED!
	}

	int volumeFactor = S_GetMasterVolume() * 256; // a legacy WTF
	uint blocksWrit = 0;

	while ( nSamplesToWrite>0 )
	{
		// all sorts of obvious ways to optimize this (explicit iterators, VMX, SPU, etc)
		libaudio_sample_surround_t * RESTRICT pWriteDest = reinterpret_cast<libaudio_sample_surround_t*>(GetAddressForBlockIdx( nStartBlock ));
		const uint batchsize = MIN( nSamplesToWrite, CELL_AUDIO_BLOCK_SAMPLES );
		for ( uint s = 0 ; s < batchsize ; ++s )
		{
			// perform the writes in order to be gentle on the write aggregator
			// (really I should go back and use dcbz+vmx here)
			pWriteDest[s].left = pFrontBuffer[s].left * volumeFactor;
			pWriteDest[s].right = pFrontBuffer[s].right * volumeFactor;
			pWriteDest[s].center = pCenterBuffer[s] * volumeFactor;
			pWriteDest[s].subwoofer = 0;
			pWriteDest[s].leftsurround = pRearBuffer[s].left * volumeFactor;
			pWriteDest[s].rightsurround = pRearBuffer[s].right * volumeFactor;
			pWriteDest[s].leftextend = 0;
			pWriteDest[s].rightextend = 0;
		}
		pFrontBuffer += batchsize;
		pRearBuffer += batchsize;
		pCenterBuffer += batchsize;

		nSamplesToWrite -= batchsize; 
		nStartBlock = (nStartBlock + 1)%CELLAUDIO_PORT_BUFFER_BLOCKS;
		++blocksWrit;
	}
	return blocksWrit;
}
#endif

//-----------------------------------------------------------------------------
// Get our device name
//-----------------------------------------------------------------------------
const char *CAudioPS3LibAudio::DeviceName( void )
{ 
	if ( m_bSurround )
		return "PS3 libaudio: 7.1 Channel Surround";
	else
		return "PS3 libaudio: Stereo"; 
}

int CAudioPS3LibAudio::GetOutputPosition( void )
{
	return m_nTotalBlocksPlayed * CELL_AUDIO_BLOCK_SAMPLES; // makes this effectively a 24-bit number, but the higher level code expects a count of samples, not sample blocks. 
}

#ifdef PS3_SUPPORT_XVOICE

ConVar voice_xplay_enable( "voice_xplay_enable", "1" );
ConVar voice_xplay_debug( "voice_xplay_debug", "0" );
ConVar voice_xplay_bandwidth_debug( "voice_xplay_bandwidth_debug", "0" );
ConVar voice_xplay_echo( "voice_xplay_echo", "0" );


CEngineVoicePs3::CEngineVoicePs3() :
	m_memContainer( SYS_MEMORY_CONTAINER_ID_INVALID )
{
}

//-----------------------------------------------------------------------------
// Initialize Voice
//-----------------------------------------------------------------------------
void CEngineVoicePs3::VoiceInit( void )
{
	memset( m_bUserRegistered, kVoiceNotInitialized, sizeof( m_bUserRegistered ) );

	// Reset voice data for all ctrlrs
	for ( int k = 0; k < m_numVoiceUsers; ++ k )
	{
		VoiceResetLocalData( k );
	}
}

void CEngineVoicePs3::VoiceShutdown( void )
{
	RemoveAllTalkers(); // this should shutdown the voice engine on PS3
}

void CEngineVoicePs3::AddPlayerToVoiceList( XUID xPlayer, int iController, uint64 uiFlags )
{
	if ( !voice_xplay_enable.GetBool() )
		return;
	// This should only happen if we are not initialized and matchmaking
	// is in an online session
	if ( XBX_GetNumGameUsers() != 1 )
		return;
	if ( !g_pMatchFramework )
		return;
	if ( !g_pMatchFramework->GetMatchSession() )
		return;
	if ( Q_stricmp( g_pMatchFramework->GetMatchSession()->GetSessionSettings()->GetString( "system/network" ), "LIVE" ) )
		return;

	// Any connecting player initializes the voice system
	if ( m_bUserRegistered[ GetVoiceUserIndex( iController ) ] == kVoiceNotInitialized )
	{
		//
		// Initialize syslib
		//
		int res = cellSysmoduleLoadModule( CELL_SYSMODULE_VOICE );
		if ( res != CELL_OK )
		{
			Warning( "VOICE PS3: Failed to load libvoice! Error code %d\n", res );
			return;
		}
		CellVoiceInitParam cvip;
		Q_memset( &cvip, 0, sizeof( cvip ) );
		cvip.appType = CELLVOICE_APPTYPE_GAME_1MB;
		cvip.version = CELLVOICE_VERSION_100;
		res = cellVoiceInitEx( &cvip );
		if ( res != CELL_OK )
		{
			Warning( "VOICE PS3: Failed to cellVoiceInit! Error code %d\n", res );
			cellSysmoduleUnloadModule( CELL_SYSMODULE_VOICE );
			return;
		}

		// Otherwise we have successfully initialized syslib
		m_bUserRegistered[ GetVoiceUserIndex( iController ) ] = kVoiceInit;
		DevMsg( "PS3 Voice: microphone library loaded and started\n" );

		// Create ports
		res = CreateVoicePortsLocal( uiFlags );
		if ( res < 0 )
		{
			Warning( "VOICE PS3: Failed to create voice ports! Error code %d\n", res );
			cellVoiceEnd();
			cellSysmoduleUnloadModule( CELL_SYSMODULE_VOICE );
			m_bUserRegistered[ GetVoiceUserIndex( iController ) ] = kVoiceNotInitialized;
		}
	}
	
	if ( xPlayer )
	{
		if ( m_bUserRegistered[ GetVoiceUserIndex( iController ) ] < kVoiceOpen )
		{
			DevWarning( "VOICE PS3: Cannot add remote talkers since voice system is not initialized (state %d)\n", m_bUserRegistered[ GetVoiceUserIndex( iController ) ] );
			Assert( 0 );
			return;
		}
		for ( int k = 0; k < m_arrRemoteTalkers.Count(); ++ k )
		{
			if ( m_arrRemoteTalkers[k].m_xuid == xPlayer )
				return;
		}

		RemoteTalker_t rt = { xPlayer, 0, uiFlags, 0 };
		int res = CreateVoicePortsRemote( rt );
		if ( res < 0 )
		{
			DevWarning( "VOICE PS3: Cannot add remote talker %llx! Failed to create ports, error %d\n", xPlayer, res );
			return;
		}

		m_arrRemoteTalkers.AddToTail( rt );
		DevMsg( "VOICE PS3: added remote talker %llx\n", xPlayer );
	}
}

int CEngineVoicePs3::CreateVoicePortsLocal( uint64 uiFlags )
{
	int res = CELL_OK;

	CellVoicePortParam pp;
	Q_memset( &pp, 0, sizeof( pp ) );
	pp.threshold = 100;
	pp.volume = 1.0f;

	// MIC > [IMic]
	pp.portType = CELLVOICE_PORTTYPE_IN_MIC;
	pp.device.playerId = 0;
	res = cellVoiceCreatePort( &m_portIMic, &pp );
	if ( res < 0 ) return res;
	// MIC > [IMic] > [OVoice]
	pp.portType = CELLVOICE_PORTTYPE_OUT_VOICE;
	pp.voice.bitrate = CELLVOICE_BITRATE_7300;
	res = cellVoiceCreatePort( &m_portOVoice, &pp );
	if ( res < 0 ) return res;
	res = cellVoiceConnectIPortToOPort( m_portIMic, m_portOVoice );
	if ( res < 0 ) return res;

#ifndef SOUND_PC_CELP_SUPPORTED
	// PCM output
	pp.portType = CELLVOICE_PORTTYPE_OUT_PCMAUDIO;
	pp.pcmaudio.bufSize = 8*1024;
	pp.pcmaudio.format.numChannels = 1;
	pp.pcmaudio.format.sampleAlignment = 1;
	pp.pcmaudio.format.dataType = CELLVOICE_PCM_SHORT_LITTLE_ENDIAN;
	pp.pcmaudio.format.sampleRate = CELLVOICE_SAMPLINGRATE_16000;
	res = cellVoiceCreatePort( &m_portOPcm, &pp );
	if ( res < 0 ) return res;
	res = cellVoiceConnectIPortToOPort( m_portIMic, m_portOPcm );
	if ( res < 0 ) return res;
#endif

	// Remote > [OEarphone]
	pp.portType = CELLVOICE_PORTTYPE_OUT_SECONDARY;
	pp.device.playerId = 0;
	res = cellVoiceCreatePort( &m_portOEarphone, &pp );
	if ( res < 0 ) return res;
	// IVoiceEcho
	pp.portType = CELLVOICE_PORTTYPE_IN_VOICE;
	pp.voice.bitrate = CELLVOICE_BITRATE_7300;
	res = cellVoiceCreatePort( &m_portIVoiceEcho, &pp );
	if ( res < 0 ) return res;
	res = cellVoiceConnectIPortToOPort( m_portIVoiceEcho, m_portOEarphone );
	if ( res < 0 ) return res;

	// Start
	res = sys_memory_container_create( &m_memContainer, 1024*1024 );
	if ( res < 0 ) return res;
	CellVoiceStartParam cvsp;
	Q_memset( &cvsp, 0, sizeof( cvsp ) );
	cvsp.container = m_memContainer;
	res = cellVoiceStartEx( &cvsp );
	if ( ( res < 0 ) && ( m_memContainer != SYS_MEMORY_CONTAINER_ID_INVALID ) )
	{
		sys_memory_container_destroy( m_memContainer );
		m_memContainer = SYS_MEMORY_CONTAINER_ID_INVALID;
	}
	if ( res < 0 ) return res;

	m_portOSendForRemote = m_portOVoice;
	m_bUserRegistered[ GetVoiceUserIndex( XBX_GetPrimaryUserId() ) ] = kVoiceOpen;
	return res;
}

int CEngineVoicePs3::CreateVoicePortsRemote( RemoteTalker_t &rt )
{
	VoiceResetLocalData( XBX_GetPrimaryUserId() );

	CellVoicePortParam pp;
	Q_memset( &pp, 0, sizeof( pp ) );
	pp.threshold = 100;
	pp.volume = 1.0f;
#ifndef SOUND_PC_CELP_SUPPORTED
	if ( !( rt.m_uiFlags & ENGINE_VOICE_FLAG_PS3 ) )
	{
		pp.portType = CELLVOICE_PORTTYPE_IN_PCMAUDIO;
		pp.threshold = 0; // can't have PCM threshold (needs huge buffer)
		pp.pcmaudio.bufSize = 8 * 1024;
		pp.pcmaudio.format.numChannels = 1;
		pp.pcmaudio.format.sampleAlignment = 1;
		pp.pcmaudio.format.dataType = CELLVOICE_PCM_SHORT_LITTLE_ENDIAN;
		pp.pcmaudio.format.sampleRate = CELLVOICE_SAMPLINGRATE_16000;
		m_portOSendForRemote = m_portOPcm;
	}
	else
#endif
	{
		pp.portType = CELLVOICE_PORTTYPE_IN_VOICE;
		pp.voice.bitrate = CELLVOICE_BITRATE_7300;
		m_portOSendForRemote = m_portOVoice;
	}

	int res = cellVoiceCreatePort( &rt.m_portIRemoteVoice, &pp );
	if ( res < 0 ) return res;

	res = cellVoiceConnectIPortToOPort( rt.m_portIRemoteVoice, m_portOEarphone );
	if ( res < 0 )
	{
		cellVoiceDeletePort( rt.m_portIRemoteVoice );
	}

	return res;
}

void CEngineVoicePs3::RemovePlayerFromVoiceList( XUID xPlayer, int iController )
{
	if ( !m_bUserRegistered[ GetVoiceUserIndex( iController ) ] )
		return;

	if ( xPlayer )
	{
		// Find the remote player in our list
		for ( int k = 0; k < m_arrRemoteTalkers.Count(); ++ k )
		{
			if ( m_arrRemoteTalkers[k].m_xuid == xPlayer )
			{
				cellVoiceDisconnectIPortFromOPort( m_arrRemoteTalkers[k].m_portIRemoteVoice, m_portOEarphone );
				cellVoiceDeletePort( m_arrRemoteTalkers[k].m_portIRemoteVoice );
				m_arrRemoteTalkers.Remove( k );
				DevMsg( "VOICE PS3: removed remote talker %llx\n", xPlayer );
				return;
			}
		}
		DevWarning( "CEngineVoicePs3::RemovePlayerFromVoiceList for unregistered remote talker %llx!\n", xPlayer );
		return;
	}

	// Local player removed shuts down the whole voice system

	cellVoiceEnd();
	cellSysmoduleUnloadModule( CELL_SYSMODULE_VOICE );
	if ( m_memContainer != SYS_MEMORY_CONTAINER_ID_INVALID )
	{
		sys_memory_container_destroy( m_memContainer );
		m_memContainer = SYS_MEMORY_CONTAINER_ID_INVALID;
	}

	m_bUserRegistered[ GetVoiceUserIndex( iController ) ] = kVoiceNotInitialized;
	DevMsg( "PS3 Voice: microphone library stopped and unloaded\n" );
	m_arrRemoteTalkers.Purge();
}

void CEngineVoicePs3::PlayIncomingVoiceData( XUID xuid, const byte *pbData, unsigned int dwDataSize, const bool *bAudiblePlayers )
{
	if ( !voice_xplay_enable.GetBool() )
		return;
	if ( m_bUserRegistered[ GetVoiceUserIndex( XBX_GetPrimaryUserId() ) ] < kVoiceOpen )
		return;

	for ( DWORD dwSlot = 0; dwSlot < XBX_GetNumGameUsers(); ++ dwSlot )
	{
		int iCtrlr = XBX_GetUserId( dwSlot );
		IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iCtrlr );
		if ( pPlayer && pPlayer->GetXUID() == xuid )
		{
			//Hack: Don't play stuff that comes from ourselves.
			if ( voice_xplay_echo.GetBool() && ( m_portOSendForRemote == m_portOVoice ) )
			{
				cellVoiceWriteToIPort( m_portIVoiceEcho, pbData, &dwDataSize );
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

			if ( voice_xplay_debug.GetBool() )
			{
				DevMsg( "XVoiceDebug: %llx -> %d = %s\n", xuid, XBX_GetUserId( dwSlot ), bCanHearPlayer ? "yes" : "no" );
			}
			// m_pXHVEngine->SetPlaybackPriority( xuid, XBX_GetUserId( dwSlot ), bCanHearPlayer ? XHV_PLAYBACK_PRIORITY_MAX : XHV_PLAYBACK_PRIORITY_NEVER );
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

	//
	// Save incoming stream
	//
	for ( int k = 0; k < m_arrRemoteTalkers.Count(); ++ k )
	{
		if ( m_arrRemoteTalkers[k].m_xuid == xuid )
		{
			cellVoiceWriteToIPort( m_arrRemoteTalkers[k].m_portIRemoteVoice, pbData, &dwDataSize );
			m_arrRemoteTalkers[k].m_flLastTalkTimestamp = Plat_FloatTime();
			return;
		}
	}
}

void CEngineVoicePs3::PlayPortInterruptHandler()
{
}


void CEngineVoicePs3::UpdateHUDVoiceStatus( void )
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

bool CEngineVoicePs3::VoiceUpdateData( int iCtrlr  )
{
	int32 dwBytes = 0;
	int32 wVoiceBytes = 0;
	bool bShouldSend = false;

	//Update UI stuff.
	UpdateHUDVoiceStatus();

	if ( m_bUserRegistered[ GetVoiceUserIndex( iCtrlr ) ] < kVoiceOpen )
		return false;

	if ( 1 )
	{
		int i = GetVoiceUserIndex( iCtrlr );

		dwBytes = m_ChatBufferSize - m_wLocalDataSize[i];

		if( dwBytes < ( m_ChatBufferSize/10 ) )
		{
			bShouldSend = true;
		}
		else
		{
			int res = cellVoiceReadFromOPort( m_portOSendForRemote, m_ChatBuffer[i] + m_wLocalDataSize[i], (uint32*) &dwBytes );
			if ( ( res >= 0 ) && ( dwBytes > 0 ) )
			{
				m_wLocalDataSize[i] += ( dwBytes ) & 0xFFFF;
				wVoiceBytes += ( dwBytes ) & 0xFFFF;

				if( m_wLocalDataSize[i] >= 64 ) // voice buffer enough size that it should get sent
				{
					bShouldSend = true;
				}
			}
		}
	}

	return  bShouldSend || 
		( wVoiceBytes && 
		( Plat_FloatTime() - m_dwLastVoiceSend[ GetVoiceUserIndex( iCtrlr ) ] ) > MAX_VOICE_BUFFER_TIME );
}

void CEngineVoicePs3::SetPlaybackPriority( XUID remoteTalker, int iController, int iAllowPlayback )
{
	// No muting support in voice engine
}

void CEngineVoicePs3::GetRemoteTalkers( int *pNumTalkers, XUID *pRemoteTalkers )
{
	if ( pNumTalkers )
		*pNumTalkers = m_arrRemoteTalkers.Count();

	if ( pRemoteTalkers )
	{
		for ( int k = 0; k < m_arrRemoteTalkers.Count(); ++ k )
			pRemoteTalkers[k] = m_arrRemoteTalkers[k].m_xuid;
	}
}

void CEngineVoicePs3::GetVoiceData( int iController, const byte **ppvVoiceDataBuffer, unsigned int *pnumVoiceDataBytes )
{
	iController = GetVoiceUserIndex( iController );
	*pnumVoiceDataBytes = m_wLocalDataSize[ iController ];
	*ppvVoiceDataBuffer = m_ChatBuffer[ iController ];
}

void CEngineVoicePs3::GetVoiceData( int iCtrlr, CCLCMsg_VoiceData_t *pMessage )
{
	IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iCtrlr );
	pMessage->set_xuid( pPlayer ? pPlayer->GetXUID() : 0ull );
	if ( !pMessage->xuid() )
		return;

	int iController = GetVoiceUserIndex( iCtrlr );
	if ( !m_wLocalDataSize[ iController ] ) 
		return;

	pMessage->set_data( m_ChatBuffer[ iController ], m_wLocalDataSize[ iController ] );
}

void CEngineVoicePs3::VoiceSendData( int iCtrlr, INetChannel *pChannel )
{
	if ( !pChannel )
		return;

	CCLCMsg_VoiceData_t voiceMsg;
	GetVoiceData( iCtrlr, &voiceMsg );

	pChannel->SendNetMsg( voiceMsg, false, true );
	VoiceResetLocalData( iCtrlr );
}

void CEngineVoicePs3::VoiceResetLocalData( int iCtrlr )
{
	iCtrlr = GetVoiceUserIndex( iCtrlr );

	if ( voice_xplay_bandwidth_debug.GetBool() && m_wLocalDataSize[iCtrlr] )
	{
		DevMsg( "PS3 Voice: microphone stream %0.2fKb\n", m_wLocalDataSize[iCtrlr]/1024.0f );
	}

	m_dwLastVoiceSend[ iCtrlr ] = Plat_FloatTime();
	Q_memset( m_ChatBuffer[ iCtrlr ], 0, m_ChatBufferSize );
	m_wLocalDataSize[ iCtrlr ] = 0;
}

#pragma warning(push) // warning C4800 is meaningless or worse
#pragma warning( disable: 4800 )
bool CEngineVoicePs3::IsLocalPlayerTalking( int controllerID )
{
	controllerID = GetVoiceUserIndex( controllerID );
	return ( m_wLocalDataSize[controllerID] > 0 ) || ( Plat_FloatTime() - m_dwLastVoiceSend[controllerID] <= MAX_VOICE_BUFFER_TIME );
}

bool CEngineVoicePs3::IsPlayerTalking( XUID uid )
{
	if ( !g_pMatchFramework->GetMatchSystem()->GetMatchVoice()->CanPlaybackTalker( uid ) )
		return false;

	for ( int k = 0; k < m_arrRemoteTalkers.Count(); ++ k )
	{
		if ( m_arrRemoteTalkers[k].m_xuid == uid )
		{
			return ( ( Plat_FloatTime() - m_arrRemoteTalkers[k].m_flLastTalkTimestamp ) < 0.2 );
		}
	}
	return false;
}

bool CEngineVoicePs3::IsHeadsetPresent( int id )
{
	return ( m_bUserRegistered[ GetVoiceUserIndex( id ) ] >= kVoiceInit );
}
#pragma warning(pop)

void CEngineVoicePs3::RemoveAllTalkers()
{
	RemovePlayerFromVoiceList( NULL, XBX_GetPrimaryUserId() ); // there's only one user possibly registered, unregister it
}

CEngineVoicePs3 *Audio_GetXVoice( void )
{
	if ( CAudioPS3LibAudio::m_pSingleton )
	{
		return CAudioPS3LibAudio::m_pSingleton->GetVoiceData();
	}

	return NULL;
}


EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CEngineVoicePs3, IEngineVoice,
								  IENGINEVOICE_INTERFACE_VERSION, *Audio_GetXVoice() );
#endif