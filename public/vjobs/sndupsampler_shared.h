//========== Copyright © Valve Corporation, All rights reserved. ========
#if !defined( JOB_SNDUPSAMPLER_SHARED_HDR ) && defined( _PS3 )
#define JOB_SNDUPSAMPLER_SHARED_HDR


#include "ps3/spu_job_shared.h"


// the PS3 audio buffer can have either 8 or 16 blocks of 256 samples each. 
// The only sample frequency allowed is 48khz, so that's around 23 or 46 milliseconds.
#define CELLAUDIO_PORT_BUFFER_BLOCKS		16

#define SURROUND_HEADPHONES		0
#define SURROUND_STEREO			2
#define SURROUND_DIGITAL5DOT1	5
#define SURROUND_DIGITAL7DOT1	7

// 7.1 means there are a max of 6 channels
#define MAX_DEVICE_CHANNELS		8



/// the libaudio buffers are simply large arrays of float samples.
/// there's only two configurations: two-channel and eight-channel.
/*
 * This is disabled as now we are only outputting surround.
 * The stereo is pushed in the left and right of the surround structure.
 *
struct libaudio_sample_stereo_t
{
	float left;
	float right;
};
*/

struct libaudio_sample_surround_t
{
	float left;
	float right;
	float center;
	float subwoofer;
	float leftsurround;
	float rightsurround;
	float leftextend;
	float rightextend;
};


namespace job_sndupsampler
{
	
	typedef CellSpursJob256 JobDescriptor_t;

	enum ConstEnum_t
	{
		INPUTFREQ = 44100,
		OUTPUTFREQ = 48000,
		BUFFERSIZE = 256 * 16, // < input samples, should be a power of two
		
		OUTBUFFERSAMPLES
	};
	
	struct BlockTrace_t
	{
		float m_fractionalSamplesBefore;
		uint32 m_n44kSamplesConsumed;
	};

	struct ALIGN16 JobOutput_t
	{
		uint32 m_nBlockWritten;
		float m_fractionalSamples;
		int32 m_ringHead;
		int32 m_n44kSamplesConsumed;
		BlockTrace_t m_trace[0x10];
	}
	ALIGN16_POST;

	/// ring buffers: one interleaved pair for stereo, or five channels for surround.
	/// I compress them back to signed shorts to conserve memory bandwidth.
	struct stereosample_t  
	{
		int16 left, right;
	};
	struct surroundsample_t  
	{
		int16 left, right, center, surleft, surright;
	};

	struct JobParams_t
	{
		sys_addr_t m_eaPortBufferAddr; // may change when user switches surround <-> stereo
		
		int32 m_availableInputBlocks;
		int32 m_nextBlockIdxToWrite;
		float m_volumeFactor;
		float m_fractionalSamples;
		int32 m_ringHead;
		int32 m_ringCount;
		float m_flMaxSumStereo;
		float m_flBackChannelMultipler;
		
		uint8 m_eaInputSamplesBegin0xF;
		uint8 m_nDebuggerBreak;

		int8 m_deviceChannels;
		int8 m_nCellAudioBlockSamplesLog2;
		int8 m_bIsSurround;
	public:

		inline float *GetEffectiveAddressForBlockIdx( unsigned nBlock )const
		{ 
			nBlock %= CELLAUDIO_PORT_BUFFER_BLOCKS;
			return reinterpret_cast<float *>( m_eaPortBufferAddr + ( ( nBlock * sizeof(float) * m_deviceChannels ) << m_nCellAudioBlockSamplesLog2 ) );
		}
		
		inline bool IsSurround() const
		{
			return m_bIsSurround;
		}

		inline int OutputSamplesAvailable()
		{
			// returns the number of output samples we can generate (notice this rounds down, ie we may leave some fractional samples for next time)
			return ( ( m_ringCount - 1 ) * OUTPUTFREQ ) / INPUTFREQ;
		}
	};
	inline JobParams_t * GetJobParams( void *pJob )
	{
		return VjobGetJobParams< JobParams_t, JobDescriptor_t >( pJob );
	}	

}

#endif