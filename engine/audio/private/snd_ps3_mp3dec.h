//========= Copyright © Valve Corporation, All rights reserved. ============//
#ifndef ENGINE_AUDIO_PRIVATE_SND_PS3_MP3DEC_HDR
#define ENGINE_AUDIO_PRIVATE_SND_PS3_MP3DEC_HDR

#ifndef _PS3
#error "This header is for PS/3 target only"
#endif

#include "vjobs_interface.h"
#include "vjobs/mp3dec_shared.h"
#include "sys/timer.h"
#include "tier1/checksum_crc.h"

typedef job_mp3dec::Joblet_t Mp3DecJoblet;
typedef job_mp3dec::Context_t Mp3DecContext;

struct Mp3DecMgr: public VJobInstance
{
	void Init();
	void Shutdown();
	
	void OnVjobsInit(); // gets called after m_pRoot was created and assigned
	void OnVjobsShutdown(); // gets called before m_pRoot is about to be destructed and NULL'ed
	
	Mp3DecJoblet * NewDecode( uint nFlags = Mp3DecJoblet::FLAGS_MONO_OR_STEREO );
	void KickPending();
	void DeleteDecode( Mp3DecJoblet *pJoblet );
	
	void Wait( Mp3DecJoblet * pJoblet ){ while( !pJoblet->IsComplete() ) sys_timer_usleep( 60 ); }
	
	// wait for all pending jobslets to finish
	void Finish(){ while( GetWorkerJobParams()->m_nPut != GetWorkerJobParams()->m_nGet ) sys_timer_usleep( 60 ); }
	
	void EnterWorkerLock(){ cellAtomicIncr32( &GetWorkerJobParams()->m_nWorkerLock ); }
	void LeaveWorkerLock(){ cellAtomicDecr32( &GetWorkerJobParams()->m_nWorkerLock ); }

	int GetNumberOfJobletsForDebugging() const { return JOBLET_COUNT; }
	const Mp3DecJoblet & GetJobletForDebugging( int nJobLetIndex ) const { return m_joblets[ nJobLetIndex ]; }

protected:
	job_mp3dec::JobParams_t *GetWorkerJobParams(){ return job_mp3dec::GetJobParams( &m_jobWorker ); }
	void StartMsfTest( const char * pInputFile, const char * pOutputExtension, int nMode );
protected:
	CRC32_t m_nDecoderCrc;
	uint m_nAllocatedNotKicked;
	int m_nDecoderSize;
	uint m_nMaxSpuWorkers;
	job_mp3dec::JobParams_t *m_pWorkerParams;
	job_mp3dec::JobDescriptor_t m_jobWorker;
	enum ConstEnum_t{ JOBLET_COUNT = job_mp3dec::JobParams_t::JOBLET_COUNT };
	Mp3DecJoblet m_joblets[JOBLET_COUNT];
};

const int NUMBER_OF_MP3_DECODER_SLOTS = 4;		// We will allow for 4 decoding at a time (using up to 4 SPUs)
extern Mp3DecMgr g_mp3dec[NUMBER_OF_MP3_DECODER_SLOTS];

struct RiffWavHeader
{
	uint32 m_nRiff; // 'RIFF'
	uint32 m_nChunkSizeLE;
	uint32 m_nFormat; // 'WAVE'

	uint32 m_nSubchunk1ID; // 'fmt '
	uint32 m_nSubchunk1SizeLE; 
	uint16 m_nAudioFormat; // 1
	uint16 m_nNumChannels;
	uint32 m_nSampleRate;
	uint32 m_nByteRate;
	uint16 m_nBlockAlign;
	uint16 m_nBitsPerSample;
	uint32 m_nSubchankData;
	uint32 m_nSubchunk2Size;

	static uint32 Swap32( uint32 a )
	{
		return ( a >> 24 ) | ( ( a >> 8 ) & 0xFF00 ) | ( ( a << 8 ) & 0xFF0000 ) | ( a << 24 );
	}

	static uint16 Swap16( uint16 a )
	{
		return ( ( a & 0xFF ) << 8 ) | ( ( a >> 8 ) & 0xFF );
	}


	void Init( uint nNumSamples, uint nNumChannels, uint nBitsPerSample, uint nSampleRate ) // == NumSamples * NumChannels * BitsPerSample/8
	{
		m_nRiff = 0x52494646;
		uint nDataSize = nNumSamples * nNumChannels * nBitsPerSample/8;
		m_nChunkSizeLE = Swap32( 36 + nDataSize );
		m_nFormat = 0x57415645;

		m_nSubchunk1ID = 0x666d7420;
		m_nSubchunk1SizeLE = Swap32( 16 );
		m_nAudioFormat = 0x0100; // PCM 
		m_nNumChannels = Swap16( nNumChannels );
		m_nSampleRate = Swap32( nSampleRate );
		m_nByteRate = Swap32( nSampleRate * nNumChannels * nBitsPerSample / 8 );
		m_nBlockAlign = Swap16( nNumChannels * nBitsPerSample / 8 );
		m_nBitsPerSample = Swap16( nBitsPerSample );
		m_nSubchankData = 0x64617461; // 'data'
		m_nSubchunk2Size = Swap32( nNumSamples * nNumChannels * nBitsPerSample / 8 );
		COMPILE_TIME_ASSERT( sizeof( *this ) == 44 );
	}
};

// This function skips ID3 tag version2.x
uint8* SkipId3Tag( uint8 * pMp3header );




#endif