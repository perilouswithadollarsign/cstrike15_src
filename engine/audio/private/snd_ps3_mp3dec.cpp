//========= Copyright © Valve Corporation, All rights reserved. ============//
#include "snd_ps3_mp3dec.h"
#include "vjobs/root.h"
#include "filesystem_engine.h"
#include "filesystem.h"
#include <cell/mstream.h>
#include "Mp3DecLibPpu.h"
#include "mp3declib.h"

extern IVJobs * g_pVJobs;

Mp3DecMgr g_mp3dec[NUMBER_OF_MP3_DECODER_SLOTS];

void Mp3DecMgr::Init()
{
	g_pVJobs->Register( this );
}

#if 0 // def _DEBUG
#undef Assert
#define Assert(X)  do{if( !(X) ) {Msg("Assertion failed\n%s:%d\n%s\n", __FILE__, __LINE__, #X); DebuggerBreak();};}while(0)
#endif


void Mp3DecMgr::Shutdown()
{
	Finish();		
	g_pVJobs->Unregister( this );
}

void Mp3DecMgr::OnVjobsInit() // gets called after m_pRoot was created and assigned
{
	COMPILE_TIME_ASSERT( !( ( JOBLET_COUNT - 1 ) & JOBLET_COUNT ) ); // JOBLET_COUNT must be a power of 2
	V_memset( &m_jobWorker, 0, sizeof( m_jobWorker ) );
	V_memset( m_joblets, 0, sizeof( m_joblets ) );
	m_nMaxSpuWorkers = 1;				// Use only one SPU at a time (that way each job is implicitly dependent on the previous jobs to be completed).
	m_jobWorker.header = *m_pRoot->m_pJobMp3Dec;
	m_jobWorker.header.sizeScratch = 114 * 1024 / 16;
	m_jobWorker.header.sizeInOrInOut = job_mp3dec::IOBUFFER_SIZE;
	job_mp3dec::JobParams_t *pParams = GetWorkerJobParams();
	m_pWorkerParams = pParams;
	m_nDecoderSize = 0;
	m_nAllocatedNotKicked = 0;
	int nError = cellMP3IntegratedDecoderGetRequiredSize( &m_nDecoderSize );
	if( nError )
	{
		Warning( "cannot initialize mp3 decoding, error %d\n", nError );
	}
	pParams->m_eaDecoder = MemAlloc_AllocAligned( m_nDecoderSize, 128 );
	pParams->m_eaJoblets = m_joblets;
	cellMP3IntegratedDecoderInit( pParams->m_eaDecoder, m_nDecoderSize );
	
	m_nDecoderCrc = CRC32_ProcessSingleBuffer( pParams->m_eaDecoder, m_nDecoderSize );
	
	if( CommandLine()->FindParm( "-msftest" ) )
	{
		const char * pTestFiles[] = {
			"amb_muffled_lo_mach_14.mp3",
			"amb_muffled_lo_mach_15.mp3",
			"amb_muffled_lo_mach_17.mp3",
			"portal_4000_degrees_kelvin.msf",
			"portal_android_hell.msf",
			"portal_no_cake_for_you.msf",
			"portal_party_escort.msf",
			"portal_procedural_jiggle_bone.msf",
			"portal_self_esteem_fund.msf",
			"portal_still_alive.msf",
			"portal_stop_what_you_are_doing.msf",
			"portal_subject_name_here.msf",
			"portal_taste_of_blood.msf",
			"portal_you_cant_escape_you_know.msf",
			"portal_youre_not_a_good_person.msf",
			"error.mps",
			"amb_metal_imp_warehouse_39.mps",
			"aa2.msf",
			"aa1.msf"
		};
		for( int i = 0; i < ARRAYSIZE( pTestFiles ); i++ )
		{
			const char * pTestFile = pTestFiles[i];
			//g_pFileSystem->FileExists( pTestFile );
			//StartMsfTest( pTestFile, 0 );
			//StartMsfTest( pTestFile, "_skip1_noctx.wav", 3 );
			StartMsfTest( pTestFile, "_dec.wav", 1 );
			//StartMsfTest( pTestFile, "_ctx0.wav", 1 );
		}
	}
}





// This function skips ID3 tag version2.x
uint8* SkipId3Tag( uint8 * pMp3header )
{
	uint32 tmp;
	uint32 size=0;

	if ( (pMp3header[0]!='I') ||
		(pMp3header[1]!='D') ||
		(pMp3header[2]!='3')
		)
	{
		Msg( "ID3 tag v2.x not found\n" );
		while( *pMp3header != 0xFF || ( pMp3header[1] & 0xE0 ) != 0xE0 )
			++pMp3header; // sync up with mp3 bitstream
		return pMp3header; // ID3 tag not found, these are probably MP3 frames going right here
	}

	for(uint i=0;i<4;i++) {
		tmp = (pMp3header[i+6] & 0x7f);
		tmp<<=(7*(3-i));
		size|=tmp;
	}

	// skip 10 bytes of the header, and the size is the size of the data after the header (the tag)
	return pMp3header + 10 + size;
}




void ValidateMp3( uint8 * pMp3Frames, uint8 * pMp3FramesEnd )
{
	uint nPadding[2] = {0,0};
	for( uint8* p = pMp3Frames; p < pMp3FramesEnd; )
	{
		Mp3FrameHeader * pHdr = ( Mp3FrameHeader * )p;
		Assert( pHdr->CheckSync() );
		Mp3FrameHeader *pNext = ( Mp3FrameHeader * )( p + pHdr->CorrectFrameLength( pMp3FramesEnd ) );
		if( uintp( pNext + 1 ) >= uintp( pMp3FramesEnd ) )
			break;
		Assert( pNext->CheckSync() );
		nPadding[pHdr->GetPadding()]++;
		p = (uint8*)pNext;
	}
	
	Msg( "MP3 validation: %d padded, %d unpadded\n", nPadding[1], nPadding[0] );
}



void Mp3DecMgr::StartMsfTest( const char * pInputFile, const char *pExt, int nMode )
{
	FileHandle_t fh = g_pFileSystem->OpenEx( pInputFile, "rb", FSOPEN_NEVERINPACK );
	if( fh == FILESYSTEM_INVALID_HANDLE )
		return;
	CUtlBuffer msf;
	if( !g_pFileSystem->ReadToBuffer( fh, msf ) )
	{	
		Warning("Cannot load test msf file\n");
		return;
	}
	g_pFileSystem->Close( fh );
	uint8 * pMp3Frames = (uint8*)msf.Base(), *pMp3FramesEnd = pMp3Frames + msf.Size();

	{
		CellMSMSFHeader * pMsfHeader = (CellMSMSFHeader *)msf.Base();
		if( pMsfHeader->header[0] == 'M' && pMsfHeader->header[1] == 'S' && pMsfHeader->header[2] == 'F' )
		{
			if( pMsfHeader->compressionType != CELL_MS_MP3 )
			{
				Warning("Invalid compression type %d\n", pMsfHeader->compressionType );
			}
			// one of the samples comments that 0x10 is the bit responsible for -loop option at the time of compilation. Documentation states "bit 4" , so it means all the bits in documentation are little-endian.
			// See MSWrapResource.cpp : 173  (romaji)  MSF fairu sakuseiji ni -loop wotsuketakadoukano handan
			Msg( "Testing %d-channel MP3 @%dHz, %d loops %s %s %s\n", pMsfHeader->channels, pMsfHeader->sampleRate, pMsfHeader->miscInfo & 0xF, pMsfHeader->miscInfo & 0x10 ? "-loop":"(no -loop)", pMsfHeader->miscInfo & 0x20 ? "VBR":"CBR", pMsfHeader->miscInfo & 0x40 ? "joint stereo":"" );
			pMp3Frames = (uint8*)( pMsfHeader + 1 );
		}
		else
		{
			pMp3Frames = SkipId3Tag( (uint8*)msf.Base() );
		}
	}
	
	char outputFile[256];
	V_snprintf( outputFile, sizeof( outputFile ), "/app_home/%s", pInputFile );
	V_strncpy( V_strrchr( outputFile, '.' ), pExt, sizeof( outputFile ) );
	
	FILE *fOut = ( nMode & 1 ) ? fopen( outputFile, "wb" ) : NULL;
	RiffWavHeader hdr;
	if( fOut )
	{
		fwrite( &hdr, 1, sizeof( hdr ), fOut );
	}
	else
	{
		V_strcpy( outputFile, "<null>" );
	}

	while( GetWorkerJobParams()->m_nWorkers > 1 )
		sys_timer_usleep(100);
	m_nMaxSpuWorkers = 1; // so that context is serialized
	Mp3DecContext * pMp3Context = NULL;
	pMp3Context = (Mp3DecContext * )MemAlloc_AllocAligned( sizeof( Mp3DecContext ), 128 );
	if( pMp3Context ) pMp3Context->Init();

	Msg("Decompressing %s into %s\n", pInputFile, outputFile );
	// 	CUtlVector<uint16> wav;
	// 	wav.EnsureCapacity( 32*1024*1024 );
	uint nTotalSamples = 0;
	
	CUtlVector<int16> arrWave;
	
	uint nChannelFlags = Mp3DecJoblet::FLAG_STEREO;

	ValidateMp3( pMp3Frames, pMp3FramesEnd );
	
	if( nMode == 4 )
		return;

	Mp3FrameHeader * pMp3FrameHeader = ( Mp3FrameHeader *)pMp3Frames;
	uint nSamplingRate = pMp3FrameHeader->GetFrameSamplingRate();
	float flBitrateSum = 0;
	uint nBitrateFrames = 0;
	uint nTickStart = __mftb();
	
	uint nBatchFrames = 1;
	EnterWorkerLock();
	const uint nMaxSkipFrames = 1;
	uint nSkipFrames = 0;
	uint8 * pPreviousFrame[nMaxSkipFrames+1];
	
	while( pMp3Frames < pMp3FramesEnd )
	{
		if( nMode & 2 )
		{
			const uint nMaxParallelJoblets = JOBLET_COUNT;
			arrWave.SetCount( nBatchFrames * nMaxParallelJoblets * 0x901 );
			Mp3DecJoblet *pDec[nMaxParallelJoblets];
			uint nDecCount = 0;
			for( uint i = 0; i < JOBLET_COUNT; ++i )
			{
				Assert( !m_joblets[i].IsAllocated() );
			}
			
			for( nDecCount = 0; nDecCount < nMaxParallelJoblets; ++nDecCount )
			{
				pPreviousFrame[0] = pMp3Frames;
				if( pMp3Frames + 4 >= pMp3FramesEnd )
					break;
				uint nFrameLength = 0, nBatchedFrames = 0;
				
				uint8 * pLastFrame = pMp3Frames;
				while( nBatchedFrames < nBatchFrames )
				{
					Mp3FrameHeader* pFrame = ( Mp3FrameHeader* )( pMp3Frames + nFrameLength );
					if( !pFrame->CheckSync() || pMp3Frames + nFrameLength > pMp3FramesEnd )
						break;
					pLastFrame = pMp3Frames + nFrameLength;
					nFrameLength += pFrame->CorrectFrameLength( pMp3FramesEnd );
					flBitrateSum += pFrame->GetBitrateKbps();
					nBitrateFrames ++;
					nBatchedFrames ++;
				}
				
				if( nFrameLength == 0 )
					break;
				
				pDec[nDecCount] = NewDecode( nChannelFlags | Mp3DecJoblet::FLAG_LITTLE_ENDIAN | Mp3DecJoblet::FLAG_FULL_MP3_FRAMES_ONLY );
				pDec[nDecCount]->m_eaMp3 = pMp3Context ? pMp3Frames : pPreviousFrame[nSkipFrames];
				pDec[nDecCount]->m_eaMp3End = pMp3Frames + nFrameLength;
				pDec[nDecCount]->m_eaWave = arrWave.Base() + nDecCount * 0x901 * nBatchFrames;
				pDec[nDecCount]->m_eaWaveEnd = arrWave.Base() + nDecCount * 0x901 * nBatchFrames + 0x900 * nBatchFrames;
				pDec[nDecCount]->m_eaContext = pMp3Context;
				pDec[nDecCount]->m_nSkipSamples = pMp3Context ? 0 : nSkipFrames * 0x480;
				
				pMp3Frames += nFrameLength;
				KickPending();
				for( uint i = nMaxSkipFrames; i-->0; )
					pPreviousFrame[i+1] = pPreviousFrame[i];
				nSkipFrames = MIN( nMaxSkipFrames, nSkipFrames + 1 );				
			}
			
			if( nDecCount == 0 )
				break;// finished
				
			for( uint i = 0 ; i < nDecCount; ++i )
			{
				Wait( pDec[i] );
				Assert( pDec[i]->m_nFlags & pDec[i]->FLAG_DECODE_COMPLETE );
				Assert( pDec[i]->m_eaWavePut == pDec[i]->m_eaWaveEnd || pDec[i]->m_eaWavePut == pDec[i]->m_eaWave || i + 1 == nDecCount );
				uint nSamplesDecoded = pDec[i]->m_eaWavePut - pDec[i]->m_eaWave;
				Assert( nSamplesDecoded <= 0x900 * nBatchFrames );
				nTotalSamples += nSamplesDecoded;
				if( fOut )
				{
					fwrite( pDec[i]->m_eaWave, ( uintp( pDec[i]->m_eaWavePut ) - uintp( pDec[i]->m_eaWave ) ) & -2, 1, fOut );
				}
				DeleteDecode( pDec[i] );
			}
			for( uint i = 0; i < JOBLET_COUNT; ++i )
			{
				Assert( !m_joblets[i].IsAllocated() );
			}
		}
		else
		{
			Mp3DecJoblet *pDec = NewDecode( nChannelFlags | Mp3DecJoblet::FLAG_LITTLE_ENDIAN /*| Mp3DecJoblet::FLAG_FULL_MP3_FRAMES_ONLY*/ );

			arrWave.SetCount( 0x900 );
			
			uint nFrameSize = ((Mp3FrameHeader*)pMp3Frames)->CorrectFrameLength( pMp3FramesEnd );
			uint8 * pFrameCopy = new uint8[ nFrameSize ];
			V_memcpy( pFrameCopy, pMp3Frames, nFrameSize );
			Msg("Decoding %u-byte frame @%p..", nFrameSize, pMp3Frames );
			
			pDec->m_eaMp3 = pFrameCopy;
			pDec->m_eaMp3End = pFrameCopy + nFrameSize;
			pDec->m_eaWave = arrWave.Base();
			pDec->m_eaWaveEnd = arrWave.Base() + arrWave.Count();
			pDec->m_eaContext = pMp3Context;
			
			KickPending();
			
			Wait( pDec );

			nChannelFlags = pDec->m_nFlags & Mp3DecJoblet::FLAGS_MONO_OR_STEREO; // choose whichever (mono or stereo) the job decoded
			
			uint nSamplesDecoded = pDec->m_eaWavePut - pDec->m_eaWave;
			Msg( "%d chan, %d samples\n", nChannelFlags, nSamplesDecoded / nChannelFlags );
			nTotalSamples += nSamplesDecoded;
			if( fOut )
			{
				fwrite( arrWave.Base(), ( uintp( pDec->m_eaWavePut ) - uintp( pDec->m_eaWave ) ) & -2, 1, fOut );
			}
			pMp3Frames += pDec->m_eaMp3Get - pDec->m_eaMp3;

			DeleteDecode( pDec );
			delete[]pFrameCopy;

			if( pDec->m_nFlags & pDec->FLAG_DECODE_ERROR )
			{
				Warning("Mp3 Decoder Error\n");
				break;
			}
			if( pDec->m_eaWavePut <= pDec->m_eaWave )
			{
				break; // nothing was decoded
			}
		}
	}
	
	float flBitrate = nBitrateFrames ? flBitrateSum / nBitrateFrames : 0;
	
	LeaveWorkerLock();
	
	if( pMp3Context )
		MemAlloc_FreeAligned( pMp3Context );
	
	const char * pszSampleCh = "mono";
	uint nChannelCount = 1;
	if( nChannelFlags & Mp3DecJoblet::FLAG_STEREO )
	{
		pszSampleCh = "stereo";
		nChannelCount = 2;
	}
	nTotalSamples /= nChannelCount;
	float flSeconds = (nTotalSamples) / float( nSamplingRate );

	uint nTicksTotal = __mftb() - nTickStart;
	if( fOut )
		Msg( "Writing %dHz %.1f second Riff Wave File, %d %s samples\n", nSamplingRate, flSeconds, nTotalSamples, pszSampleCh );
	else
	{
		Msg( "%d %s samples @%dHz @%.1f kbps = %.1f seconds in %.2f ms, ratio = %.2f%%\n", nTotalSamples, pszSampleCh, nSamplingRate, flBitrate, flSeconds, nTicksTotal / 79800.0f, 100 * ( nTicksTotal / 79800000.0f ) / ( flSeconds ) );
	}
	hdr.Init( nTotalSamples, nChannelCount, 16, nSamplingRate );
	if( fOut )
	{
		fseek( fOut, 0, SEEK_SET );
		fwrite( &hdr, 1, sizeof( hdr ), fOut );
		fclose( fOut );
	}
}

void Mp3DecMgr::OnVjobsShutdown() // gets called before m_pRoot is about to be destructed and NULL'ed
{
	Finish();
	job_mp3dec::JobParams_t *pParams = GetWorkerJobParams();
	
	if( m_nDecoderCrc != CRC32_ProcessSingleBuffer( pParams->m_eaDecoder, m_nDecoderSize ) )
	{
		Warning( "MP3 Decoder is corrupted; please tell Sergiy\n" );
	}
	MemAlloc_FreeAligned( pParams->m_eaDecoder );
}


Mp3DecJoblet * Mp3DecMgr::NewDecode( uint nFlags )
{
	job_mp3dec::JobParams_t *pParams = GetWorkerJobParams();
	// there are JOBLET_COUNT joblets in the ring buffer. The first m_nAllocatedNotKicked (counting from m_nGet index)
	// are already taken (allocated) and we cannot wait for them or allocate them because they aren't even kicked yet
	// So somebody later will kick them, but for now we have to let them be. 
	// Cycle through the remaining joblets and find one that's free and allocate it (return a pointer to it)
	int nSleepCounter = 0;
	for( uint i = m_nAllocatedNotKicked; i < JOBLET_COUNT; ++i )
	{
		// let's try to see if this joblet with this index is available for allocation
		uint nTryAllocateIndex = pParams->m_nPut + i;
		while( nTryAllocateIndex - pParams->m_nGet >= JOBLET_COUNT )
		{
			// this joblet is in previous ring of the ring buffer. SPU is working on it. Perhaps it's free,
			// but even if it is, we need to let SPU realize that and advance m_nGet pointer. 

			// this joblet logically is not allocated yet, but it occupies the same space in memory as one of the joblets previously allocated
			// in the previous ring of the joblet ring buffer.

			// there are probably workers working on this joblet, but by this line they may have exited. If they did
			// then the queue must be empty (put == get)
			Assert( pParams->m_nWorkers || pParams->m_nPut == pParams->m_nGet );
			// at all times, put and get must be within this distance (the size of the ring buffer)
			Assert( pParams->m_nPut - pParams->m_nGet <= JOBLET_COUNT );
			// wait for SPU to advance get pointer
			sys_timer_usleep( 60 ); 
			++nSleepCounter;
		}

		// if this joblet is free, we can now use it because SPU is past this point
		Mp3DecJoblet *pNextJoblet = &m_joblets[ nTryAllocateIndex & ( JOBLET_COUNT - 1 ) ];
		
		#ifdef _DEBUG
		Mp3DecJoblet jobletState;
		__sync(); // try to flush pending DMA's, to increase the probability of atomic copy (still not guaranteed, but it's for debugging only)
		V_memcpy( &jobletState, pNextJoblet, sizeof( jobletState ) );
		#endif
		
		if( !pNextJoblet->IsAllocated() )
		{
			// we found a joblet that is not allocated and is not worked by SPU. Return it.
			V_memset( pNextJoblet, 0, sizeof( *pNextJoblet ) );
			pNextJoblet->m_nFlags    = nFlags | Mp3DecJoblet::FLAG_ALLOCATED;
			m_nAllocatedNotKicked++; // we'll need to kick this joblet
			return pNextJoblet;
		}
		else
		{
			// we found a joblet that spu finished working on, but it's not free. We must skip it.
			m_nAllocatedNotKicked++;
		}
	}
	if ( nSleepCounter >= 8 )
	{
		// If we had to wait more than 0.5 ms, let's print something...
		Warning( " Mp3DecMgr::NewDecode() waited for more than %f\n", (float)nSleepCounter * 0.060f );
	}
	return NULL;
}


// kick ALL pending allocated not kicked jobs
void Mp3DecMgr::KickPending()
{
	if( !m_nAllocatedNotKicked )
		return;
	job_mp3dec::JobParams_t *pParams = GetWorkerJobParams();
	__lwsync(); // order the previous writes with submitting this joblet for processing
	uint nNewPut = cellAtomicAdd32( &pParams->m_nPut, m_nAllocatedNotKicked ) + m_nAllocatedNotKicked;
	m_nAllocatedNotKicked = 0;
	Assert( nNewPut == pParams->m_nPut );
	__lwsync(); // order joblet submission with starting another job
	uint nWorkersNeeded = MIN( m_nMaxSpuWorkers, ( nNewPut - pParams->m_nGet ) / 8 + 1 );
	while( pParams->m_nWorkers < nWorkersNeeded )
	{
		cellAtomicIncr32( ( uint32* ) &pParams->m_nWorkers );
		// spawn another worker
		m_pRoot->m_queuePortSound.pushJob( &m_jobWorker.header, sizeof( m_jobWorker ), 0, 0 );
	}
}



void Mp3DecMgr::DeleteDecode( Mp3DecJoblet *pJoblet )
{
	// free it up!
	Wait( pJoblet );
	Assert( pJoblet->IsComplete() && pJoblet->IsAllocated() );
	pJoblet->m_nFlags = 0; // it's free now, even if it's in the list of joblets to process
}
