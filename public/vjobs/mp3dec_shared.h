//========== Copyright © Valve Corporation, All rights reserved. ========
#ifndef VJOBS_MP3DEC_SHARED_HDR
#define VJOBS_MP3DEC_SHARED_HDR

#include "ps3/spu_job_shared.h"
#ifdef SPU
#include "Mp3DecSpuLib.h"
#else
#include "mp3declib.h"
#endif

extern uint8 s_mp3_bitrate_8000[2][16];
extern uint16 s_mp3_samplingrate_div50[2][4];

struct Mp3FrameHeader
{
	// http://www.mars.org/pipermail/mad-dev/2002-January/000425.html
	// The absolute theoretical maximum frame size is 2881 bytes: MPEG 2.5 Layer II,
	// 8000 Hz @ 160 kbps, with a padding slot
	enum ConstEnum_t{
		MAX_FRAME_LENGTH = 2881,
		// Theoretical frame sizes for Layer III range from 24 to 1441 bytes, but there
		// is a "soft" limit imposed by the standard of 960 bytes
		MAX_MP3_FRAME_LENGTH = 1441	 
	};

	// http://mpgedit.org/mpgedit/mpeg_format/mpeghdr.htm is a good reference as to what the following bitfields mean
	// WARNING
	// this struct works in debugger perfectly, but if you use it in the SPU code, it'll sometimes be wrong
	// don't use it for anything other than debugging
	/*union
	{
		struct	
		{	
			uint m_nFrameSync		:11; // all bits must be set at all times , or it's not really a frame header
			uint m_nAudioVersion	: 2;
			uint m_nLayerDesc		: 2;
			uint m_nProtection		: 1;		
			uint m_nBitrate			: 4;
			uint m_nSamplingRate	: 2;
			uint m_nPadding			: 1;
			uint m_nPrivateBit		: 1;
			uint m_nChannelMode		: 2;	 // 01 (joint) isn't supported in mp3dec from Sony
			uint m_nModeExtension	: 2;
			uint m_nCopyright		: 1;
			uint m_nOriginal		: 1;
			uint m_nEmphasis		: 2;
		};
	*/
		uint8 m_bits[4];
	//};
	
	//AAAAAAAA AAABBCCD EEEEFFGH IIJJKLMM
	uint CheckSync()const{ return IsCorrectHeader( m_bits ); }
	uint GetAudioVersionId()const
	{
		return ( m_bits[1] >> 3 ) & 3;
	}
	uint GetLayerDescId()const
	{
		return ( m_bits[1] >> 1 ) & 3;
	}
	uint GetProtection()const
	{
		return m_bits[1] & 1;
	}
	uint GetBitrateId()const
	{
		return m_bits[2] >> 4;
	}
	uint GetSamplingRateId()const
	{
		return ( m_bits[2] >> 2 ) & 3;
	}
	uint GetPadding()const
	{
		return ( m_bits[2] >> 1 ) & 1;
	}
	uint GetPrivateBit()const
	{
		return ( m_bits[2] ) & 1;
	}
	uint GetChannelModeId()const
	{
		return ( m_bits[3] >> 6 ) & 3;
	}
	uint GetModeExtensionId()const
	{
		return ( m_bits[3] >> 4 ) & 3;
	}
	uint GetCopyright()const
	{
		return ( m_bits[3] >> 3 ) & 1;
	}
	uint GetOriginal()const
	{
		return ( m_bits[3] >> 2 ) & 1;
	}
	uint GetEmphasisId()const
	{
		return ( m_bits[3] ) & 3;
	}
	

	inline uint GetFrameLengthIncludingHeader( bool bUsePadding = true )const
	{
		COMPILE_TIME_ASSERT( sizeof( *this ) == 4 );
		Assert( CheckSync() && GetAudioVersionId() >= 2 && GetLayerDescId() == 1 ); // version2 , layer 3
		// 1 kbps = 1024 bits per second = 128 bytes per second
		uint nAudioVersion = GetAudioVersionId() & 1, nBitrate = GetBitrateId(), nSamplingRateId = GetSamplingRateId();
		uint bitrate_8000 = s_mp3_bitrate_8000[ nAudioVersion ][ nBitrate ];
		uint samplingrate_50 = s_mp3_samplingrate_div50[ nAudioVersion ][ nSamplingRateId ];
		uint a;
		// TODO: Change the table so we don't have to do this test
		if ( nAudioVersion == 1 )
		{
			a = ( 144 * 8 * 20 ) * bitrate_8000;
		}
		else
		{
			a = ( 72 * 8 * 20 ) * bitrate_8000;
		}

		Assert( a > 0 && samplingrate_50 > 0 );

		uint nLength = a / samplingrate_50;
		if ( bUsePadding )
		{
			nLength += GetPadding();
		}

		return nLength;
	}

	inline uint GetFrameSamplingRate() const
	{
		return s_mp3_samplingrate_div50[ GetAudioVersionId()& 1][ GetSamplingRateId() ] * 50;
	}
	inline uint GetBitrateKbps()const
	{
		uint nAudioVersion = GetAudioVersionId() & 1, nBitrateId = GetBitrateId();
		return s_mp3_bitrate_8000[ nAudioVersion ][ nBitrateId ] * 8;
	}

	// Checks that the header is similar. Padding differences are ignored.
	// This will not work with VBR encoding.
	inline bool IsSimilar(const Mp3FrameHeader & otherHeader) const
	{
		// TODO: Could be optimized. Although I doubt this is actually necessary.
		bool b0 = m_bits[0] == otherHeader.m_bits[0];
		bool b1 = m_bits[1] == otherHeader.m_bits[1];
		bool b2 = (m_bits[2] & 0xFD) == (otherHeader.m_bits[2] & 0xFD);
		bool b3 = m_bits[3] == otherHeader.m_bits[3];
		return b0 & b1 & b2 & b3;
	}

	static bool IsCorrectHeader( const uint8 * h )
	{
		uint8 h0 = h[0], h1 = h[1];
		// must be 11111111 1111x01x for V1 or V2, Layer 3 header
		return ( h0 == 0xFF && ( h1 & 0xF6 ) == 0xF2 );
	}

	inline uint CorrectFrameLength( uint nLength, const uint8 * pStreamEnd )const
	{
		const uint8 * pFrameEnd = ( ( const uint8 * ) this ) + nLength;
		if( pStreamEnd >= pFrameEnd + 1 + 2 )
		{
			if( IsCorrectHeader( pFrameEnd ) )
				return nLength;
			for( uint d = 1; d < 2; ++d )
			{
				if( IsCorrectHeader( pFrameEnd - d ) )
					return nLength - d;
				if( IsCorrectHeader( pFrameEnd + d ) )
					return nLength + d;
			}
		}
		return nLength;
	}

	// scan the byte stream to find the next header
	inline uint CorrectFrameLength( const uint8 * pStreamEnd)const
	{
		uint nLength = GetFrameLengthIncludingHeader();
		return CorrectFrameLength( nLength, pStreamEnd );
	}
};






namespace job_mp3dec
{
	enum ConstEnum_t
	{
		MP3_FRAME_SAMPLE_COUNT = 0x480,
		// we need space for stereo mp3 frame, 16 bits per sample, 
		// and then 127 bytes on each side of it for misalignment,
		// and then 127 bytes more for misalignment of this whole buffer size
		// for smoother output with less pointless copying, specify more local store when initializing the job descriptor
		IOBUFFER_SIZE = ( MP3_FRAME_SAMPLE_COUNT * 2 * sizeof( int16 ) + 3 * 127 ) & -128
	};
	
	struct ALIGN16 Context_t
	{
		CellMP3Context m_context[2];
		int32 m_nInternalMp3Count;
		int32 m_nLastBytesRead;
		int32 m_nLastBytesWritten;
		uint32 m_nTotalBytesRead;
		uint32 m_nTotalBytesWritten;
		
		void Init();
	}ALIGN16_POST;

	// a joblet may not be "allocated"; if it's "not complete" AND "allocated", only then will it be processed
	// it may be deallocated any time after it's complete; it may be appended any time after it's complete; it may not suddenly become incomplete 
	struct ALIGN16 Joblet_t
	{
		enum FlagEnum_t
		{
			FLAG_DEBUG_STOP      = 0x1000,
			FLAG_DEBUG_SPIN      = 0x2000,
			FLAG_DECODE_INVALID_FRAME = 0x4000,
			FLAG_DECODE_INCOMPLETE_FRAME = 0x8000,
			
			FLAG_DECODE_WAV_SCATTER = 0x400,
			FLAG_DECODE_MP3_GATHER = 0x200,
			
			FLAG_DECODE_INIT_CONTEXT = 0x100, // don't take the context from main memory, init it in local to zeroes and then DMA it out
			FLAG_DECODE_ERROR    = 0x80, // an error happened during decode; COMPLETE bits is still set, you may read the buffer ends
			FLAG_DECODE_EMPTY    = 0x40, // empty input or output stream
			FLAG_DECODE_COMPLETE = 0x20, // decoding complete; you may read the buffer ends
			
			FLAG_ALLOCATED       = 0x10,
			
			FLAG_LITTLE_ENDIAN   = 0x08,
			FLAG_FULL_MP3_FRAMES_ONLY = 4,

			//  input: means m_eaWav will accept mono
			//  output: means m_eaWav points to mono samples
			FLAG_MONO = 1,

			//  input: means m_eaWav will accept stereo
			//  output: means m_eaWav points to pairs of samples "left, right"
			FLAG_STEREO = 2,
			
			FLAGS_MONO_OR_STEREO = FLAG_MONO | FLAG_STEREO
			
		};
		uint32 m_nFlags;
		
		uint32 m_nSkipSamples;

		// input: max number of mp3 frames to decode;
		// output: number of frames decoded
		//uint32 m_nMp3Frames; 

		// input buffer
		uint8 * m_eaMp3;  
		// output: the last decoded frame, for warming up
		uint8 * m_eaMp3Last; 
		// output: the end of the buffer that has been read
		uint8 *m_eaMp3Get;
		// input: the end of the buffer allowed to read
		uint8 * m_eaMp3End; 

		// output buffer
		int16 * m_eaWave; 
		// output: the end of the buffer written to
		int16 *m_eaWavePut;
		// intput: the end of the buffer allocated for writing
		int16 * m_eaWaveEnd;
		
		// In/Out - 2 contexts
		Context_t *m_eaContext;
		
		bool NeedDecode()const { return ( m_nFlags & ( FLAG_DECODE_ERROR | FLAG_DECODE_COMPLETE | FLAG_ALLOCATED ) ) == FLAG_ALLOCATED; } 
		bool IsAllocated()const{ return (m_nFlags & FLAG_ALLOCATED) != 0; }
		bool IsComplete()const { return (m_nFlags & FLAG_DECODE_COMPLETE) != 0; }
		bool HasDecodingError()const   { return (m_nFlags & FLAG_DECODE_ERROR) != 0; }

	} ALIGN16_POST;

	typedef CellSpursJob128 JobDescriptor_t;

	struct ALIGN16 JobParams_t
	{
		void *m_eaDecoder;
		Joblet_t *m_eaJoblets;
		
		// joblet get index;
		// SPU: volatile makes no sense because it's only operated on in LS; it's changed by multiple SPUs, though
		// PPU: m_nGet can change at any time by SPU 
		PPU_ONLY( volatile ) uint32 m_nGet;
		
		// how many jobs are started and didn't decide to quit yet.
		// when this is down to 0 AFTER we advanced our m_nPut (don't forget the barrier!), we have to spawn more jobs
		PPU_ONLY( volatile ) uint32 m_nWorkers;
		
		// joblet put index ; 
		// SPU: volatile makes no sense because it's only operated on in LS
		// PPU: volatile makes no sense because PPU uses atomics to access it, and SPU never changes this value
		uint32 m_nPut; 
		
		//uint32 m_nJobletIndexMask;
		enum ConstEnum_t {JOBLET_COUNT = 64 * 4 };		// Each sound uses 4 joblets at a time. Can decode 64 sounds in a row.

		uint32 m_nDebuggerBreak;
		
		uint32 m_nWorkerTotal;  // total workers that ever existed (not including m_nWorkers, which are the currently started up workers)
		uint32 m_nJobletsAcquired;
		
		uint32 m_nWorkerLock; // min workers to hold on to
	}ALIGN16_POST;

	inline JobParams_t * GetJobParams( void *pJob )
	{
		return VjobGetJobParams< JobParams_t, JobDescriptor_t >( pJob );
	}	
}

#endif