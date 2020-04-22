//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: PS3 MP3 Decoding
//
//=====================================================================================//

#include "audio_pch.h"
#include "tier1/mempool.h"
#include "tier1/circularbuffer.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlstack.h"
#include "filesystem/iqueuedloader.h"
#include "snd_ps3_mp3dec.h"
#include "mp3declib.h"
#include "xwvfile.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//#define DEBUG_PS3_MP3

// Need at least this amount of decoded pcm samples before mixing can commence.
// This needs to be able to cover the initial mix request, while a new decode cycle is in flight.
#define MP3_POLL_RATE	15
#define MIN_READYTOMIX	( ( 2 * MP3_POLL_RATE ) * 0.001f )

// circular staging buffer to drain MP3 decoder and stage until mixer requests
// must be large enough to hold the slowest expected mixing frame worth of samples
#define PCM_STAGING_BUFFER_TIME		200

// in milliseconds
#define MIX_IO_DATA_TIMEOUT			10000	// async i/o from BluRay could be very late - much later than DVD
#define MIX_DECODER_TIMEOUT			3000	// decoder might be very busy

#define MP3_FILE_HEADER_SIZE			2048
#define MP3_INPUT_BUFFER_SIZE			( Mp3FrameHeader::MAX_MP3_FRAME_LENGTH )		// No MP3 frame should be bigger than that.
																						// Normally it is up to 1254 bytes for 320 Kbits at 44.1 KHz stereo.
																						// We reserve more, but in reality we are at 160 Kbits per second so we could reduce this further.
																						// Change this size when using a temporary format (as we have much bigger frame at that point).
#define FAKE_MP3_INPUT_BUFFER_SIZE		( 10 * 1024 )									// For the temp format

// The output size is calculated dynamically depending if we are in mono or stereo.

// diagnostic errors
enum Mp3Error
{
	ERROR_MP3_DECODING_SUBMITTED	=	1,
	ERROR_MP3_NO_DECODING_SUBMITTED	=	0,
	// All the errors should be a negative number
	ERROR_MP3_IO_DATA_TIMEOUT		=	-1,		// async i/o taking too long to deliver MP3 frames
	ERROR_MP3_IO_NO_MP3_DATA		=	-3,		// async i/o failed to deliver any block
	ERROR_MP3_DECODER_TIMEOUT		=	-4,		// decoder taking too long to decode MP3 frames
	ERROR_MP3_NO_PCM_DATA			=	-10,	// no MP3 decoded pcm data ready
	ERROR_MP3_CANT_DECODE			=	-13,	// Error while decoding MP3
};

ConVar snd_ps3_mp3_spew_warnings( "snd_ps3_mp3_spew_warnings", "0" );
ConVar snd_ps3_mp3_spew_startup( "snd_ps3_mp3_spew_startup", "0" );
ConVar snd_ps3_mp3_spew_mixers( "snd_ps3_mp3_spew_mixers", "0" );
ConVar snd_ps3_mp3_spew_decode( "snd_ps3_mp3_spew_decode", "0" );
ConVar snd_ps3_mp3_spew_drain( "snd_ps3_mp3_spew_drain", "0" );
ConVar snd_ps3_mp3_recover_from_exhausted_stream( "snd_ps3_mp3_recover_from_exhausted_stream", "1", 0, "If 1, recovers when the stream is exhausted when playing PS3 MP3 sounds (prevents music or ambiance sounds to stop if too many sounds are played). Set to 0, to stop the sound otherwise." );
#ifdef DEBUG_PS3_MP3
ConVar snd_ps3_mp3_record( "snd_ps3_mp3_record", "0" );
#endif

extern ConVar snd_delay_for_choreo_enabled;
extern ConVar snd_mixahead;
extern float g_fDelayForChoreo;
extern uint32 g_nDelayForChoreoLastCheckInMs;
extern int g_nDelayForChoreoNumberOfSoundsPlaying;
extern ConVar snd_async_stream_spew_delayed_start_time;
extern ConVar snd_async_stream_spew_delayed_start_filter;
extern ConVar snd_async_stream_spew_exhausted_buffer;
extern ConVar snd_async_stream_spew_exhausted_buffer_time;

// Used to store the frames that are waiting decoding but will be discarded as the sound is not needed anymore.
class CFrameInfo;
static CUtlQueue< CFrameInfo * > g_FramesFinishingDecoding[NUMBER_OF_MP3_DECODER_SLOTS];
static CUtlVector< job_mp3dec::Context_t * > g_ContextsUsedInDecoding[NUMBER_OF_MP3_DECODER_SLOTS];
class CAudioMixerWavePs3Mp3;
static CUtlFixedLinkedList< CAudioMixerWavePs3Mp3 * >	g_Ps3Mp3MixerList;

//-----------------------------------------------------------------------------
// Purpose: Mixer for MP3 encoded audio (PS3 specific implementation)
//-----------------------------------------------------------------------------

/*
	Calculating the MP3 frame length and output samples is often incorrectly documented and a bit tricky.
	This code uses a simpler version and will work for 44100 and 22050 Hz sounds.

	Here is an implementation from SONY (rather complete) for all cases. We will have to rewrite it if we wanted something similar for a non PS3 game.

	int MP3DecoderGetPacketInfo( const void* pMp3Data, CellMP3DecoderPacketInfo* pPacketInfo )
	{
		const short tabsel_123[2][3][16] = {
			{
			 {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,},
			 {0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,},
			 {0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,}
			},

			{
			 {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,},
			 {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,},
			 {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,}
			}
		};

		const long freqs[9] = { 44100, 48000, 32000, 22050, 24000, 16000 , 11025 , 12000 , 8000 };
		const float kfOneOverFreq[9] = { 1.0f/44100.0f, 1.0f/48000.0f, 1.0f/32000.0f, 1.0f/22050.0f, 1.0f/24000.0f, 1.0f/16000.0f , 1.0f/11025.0f , 1.0f/12000.0f , 1.0f/8000.0f };


		CellMP3DecoderPacketInfo* hdr = pPacketInfo;

		unsigned int MP3Header;
		unsigned int *ptr;
		unsigned int lsf;
		unsigned int mpeg25;
		char * cptr;
		int size, s1, s2, s3, s4;
		int nFreqInd;

		cptr=(char *)pMp3Data;

		hdr->Tag=0;
		hdr->ID3=0;
		if ((cptr[0] == 'I')&&(cptr[1] == 'D')&&(cptr[2] == '3'))
		{
			hdr->ID3=(cptr[3]<<8)+cptr[4];
			if (cptr[3] == 1) //this is a ID3v1 tag, always 128 bytes in size
			{
				hdr->PacketSizeBytes=128;
				hdr->OutputSizeBytes = 0;
				return(0);
			}

			s1=(int)cptr[9];
			s2=(int)cptr[8]<<7;
			s3=(int)cptr[7]<<14;
			s4=(int)cptr[6]<<21;
			size=s1+s2+s3+s4;

			hdr->PacketSizeBytes = size + 10;
			return(0);
		}

		if ((cptr[0] == 'T')&&(cptr[1] == 'A')&&(cptr[2] == 'G'))
		{
			hdr->Tag=1;
			hdr->PacketSizeBytes =128;
			hdr->OutputSizeBytes = 0;
			return(0);
		}

		ptr=(unsigned int*)pMp3Data;
		MP3Header=*ptr;

		if(MP3Header & (1<<20)) 
		{
			lsf = (MP3Header & (1<<19)) ? 0x0 : 0x1;
			mpeg25 = 0;
		}
		else 
		{
			lsf = 1;
			mpeg25 = 1;
		}

		//set decode layer
		hdr->Layer = 4-((MP3Header>>17)&3);

		if (mpeg25)
		{
			nFreqInd = 6 + ((MP3Header>>10)&0x3);
		}
		else
		{
			nFreqInd = ((MP3Header>>10)&0x3) + (lsf*3);
		}

		hdr->Sync = MP3Header;
		hdr->ID = ((MP3Header>>19)&0x3);
		hdr->ProtBit = ((MP3Header>>16)&0x1);
		hdr->BitRate = ((MP3Header>>12)&0xf);
		hdr->PadBit = ((MP3Header>>9)&0x1);
		hdr->PrivBit = ((MP3Header>>8)&0x1);
		hdr->Mode = ((MP3Header>>6)&0x3);
		hdr->ModeExt = ((MP3Header>>4)&0x3);
		hdr->Copy = ((MP3Header>>3)&0x1);
		hdr->Home = ((MP3Header>>2)&0x1);
		hdr->Emphasis = MP3Header & 0x3;

		if(!hdr->BitRate) 
		{
			hdr->OutputSizeBytes = 0;
			return (-1); // Invalid MP3 header
		}

		hdr->Frequency=freqs[nFreqInd];
		switch(hdr->Layer) 
		{
		case 1:
			hdr->BitRate=tabsel_123[lsf][0][hdr->BitRate];
			hdr->PacketSizeBytes = hdr->BitRate*12000;
			hdr->PacketSizeBytes *= kfOneOverFreq[nFreqInd];
			hdr->PacketSizeBytes = ((hdr->PacketSizeBytes+hdr->PadBit)<<2);
			break;

		case 2:
			hdr->BitRate=tabsel_123[lsf][1][hdr->BitRate];
			hdr->PacketSizeBytes = hdr->BitRate * 144000;
			hdr->PacketSizeBytes *= kfOneOverFreq[nFreqInd];
			hdr->PacketSizeBytes += hdr->PadBit;// - 4;
			break;

		case 3:

			hdr->BitRate=tabsel_123[lsf][2][hdr->BitRate];
			hdr->PacketSizeBytes = hdr->BitRate * 144000;
			hdr->PacketSizeBytes /= hdr->Frequency<<(lsf);
			hdr->PacketSizeBytes = hdr->PacketSizeBytes + hdr->PadBit;
			break; 

		default: //unknown layer
			return (-1); // Invalid MP3 header
		}
	    
		// outsize
		int nOutSize = 1152;
		if( hdr->ID == 3 ) // mpeg 1 & layer 1
		{
			if( hdr->Layer == 1)
				nOutSize = 384;
		}
		else if( hdr->ID == 2 )
		{
			if( hdr->Layer == 1 )
				nOutSize = 192;
			else if( hdr->Layer == 3 )
				nOutSize = 576;
		}
	    
		// samples * bytes (16bit) * channels
		int channels = 1;
		if( hdr->Mode != 3 )
			channels = 2;
		hdr->OutputSizeBytes = nOutSize * 2 * channels;

		return 0;
	}

 */

class CFrameInfo
{
public:
	CFrameInfo( int nFormat, int nInputBufferAllocatedSize, int nOutputBufferAllocatedSize );
	~CFrameInfo();

	void * GetInputBuffer() const;
	int GetInputBufferSize() const;
	void SetInputBufferSize( int nInputBufferSize );
	int GetInputBufferAllocatedSize() const;

	void * GetOutputBuffer() const;
	int GetOutputBufferSize() const;
	void SetOutputBufferSize( int nOutputBufferSize );
	int GetOutputBufferAllocatedSize() const;

	bool IsDecodingDone() const;
	void SetDecodingDone( bool bDecodingDone );

	void SetDataOffset( int nDataOffset );
	int GetDataOffset() const;

	void SetJoblet( Mp3DecJoblet * pJoblet );
	Mp3DecJoblet * GetJoblet() const;

private:
	// Start do not implement
	CFrameInfo();
	CFrameInfo( const CFrameInfo & other );
	CFrameInfo & operator=( const CFrameInfo & other );
	// End do not implement

	int m_nFormat;
	char * m_pInputBuffer;
	int m_nInputBufferSize;
	int m_nInputBufferAllocatedSize;
	char * m_pOutputBuffer;
	int m_nOutputBufferSize;
	int m_nOutputBufferAllocatedSize;
	int m_nDataOffset;
	bool m_bDecodingDone;
	Mp3DecJoblet * m_pJoblet;
};

FORCEINLINE
CFrameInfo::CFrameInfo( int nFormat, int nInputBufferAllocatedSize, int nOutputBufferAllocatedSize )
	:
	m_nFormat( nFormat ),
	m_nInputBufferSize( -1 ),
	m_nInputBufferAllocatedSize( nInputBufferAllocatedSize ),
	m_nOutputBufferSize( -1 ),
	m_nOutputBufferAllocatedSize( nOutputBufferAllocatedSize ),
	m_nDataOffset( 0 ),
	m_bDecodingDone( false ),
	m_pJoblet( NULL )
{
	m_pInputBuffer = new char[ nInputBufferAllocatedSize ];
	m_pOutputBuffer = new char[ nOutputBufferAllocatedSize ];
}

FORCEINLINE
CFrameInfo::~CFrameInfo()
{
	Assert( m_pJoblet == NULL );
	Assert( m_bDecodingDone == false );		// At that point the frame should be available
	delete[] m_pInputBuffer;
	delete[] m_pOutputBuffer;
}

FORCEINLINE
void * CFrameInfo::GetInputBuffer() const
{
	return m_pInputBuffer;
}

FORCEINLINE
int CFrameInfo::GetInputBufferSize() const
{
	return m_nInputBufferSize;
}

FORCEINLINE
void CFrameInfo::SetInputBufferSize( int nInputBufferSize )
{
	Assert( nInputBufferSize <= m_nInputBufferAllocatedSize );	// If we assert here and we are using the temp format
																// it is probably because the input buffer allocated is too small.
																// It is optimized for MP3 and not for the temp format.
	m_nInputBufferSize = nInputBufferSize;
}

FORCEINLINE
int CFrameInfo::GetInputBufferAllocatedSize() const
{
	return m_nInputBufferAllocatedSize;
}

FORCEINLINE
void * CFrameInfo::GetOutputBuffer() const
{
	return m_pOutputBuffer;
}

FORCEINLINE
int CFrameInfo::GetOutputBufferSize() const
{
	return m_nOutputBufferSize;
}

FORCEINLINE
void CFrameInfo::SetOutputBufferSize( int nOutputBufferSize )
{
	Assert( nOutputBufferSize <= m_nOutputBufferAllocatedSize );
	m_nOutputBufferSize = nOutputBufferSize;
}

FORCEINLINE
int CFrameInfo::GetOutputBufferAllocatedSize() const
{
	return m_nOutputBufferAllocatedSize;
}

FORCEINLINE
bool CFrameInfo::IsDecodingDone() const
{
	Assert( ( m_pJoblet == NULL ) == m_bDecodingDone );		// Make sure we are in sync
															// Valid for the TEMP format as well as it is decoded as soon as it is submitted
	return m_bDecodingDone;
}

FORCEINLINE
void CFrameInfo::SetDecodingDone( bool bDecodingDone )
{
	m_bDecodingDone = bDecodingDone;
}

FORCEINLINE
void CFrameInfo::SetDataOffset( int nDataOffset )
{
	m_nDataOffset = nDataOffset;
}

FORCEINLINE
int CFrameInfo::GetDataOffset() const
{
	return m_nDataOffset;
}

FORCEINLINE
void CFrameInfo::SetJoblet( Mp3DecJoblet * pJoblet )
{
	m_pJoblet = pJoblet;
}

FORCEINLINE
Mp3DecJoblet * CFrameInfo::GetJoblet() const
{
	return m_pJoblet;
}

// Frees the frame infos that were still decoding when we called ~CDecoder().
// We can't not block in ~CDecoder() as in some maps, some sounds are stopped in the middle every few frames,
// and this creates a few ms busy wait. We are just keeping tabs here and will clear them as necessary.
// The best is to call this in blocking manner when the whole sound system is shutdown.
// If non-blocking, we are only going to clear the frames that have been decoded.
// We can't re-use the frames as part of the pools as given the various output sizes (mono/stereo, 22KHz and 44KHz), we would have different sizes.
// Reserving for the worse case scenario will consume more memory that necessary, and invalidate some debug checks.
void HandleRemainingFrameInfos( int nMp3DecoderSlot, bool bBlocking )
{
	int nNumberOfContexts = g_ContextsUsedInDecoding[nMp3DecoderSlot].Count();
	if ( nNumberOfContexts >= 64 )
	{
		// If we have way too many contexts going on (from frames that were not decoded in time)
		// let's block, so we fix pathological cases. This should not happen in normal game.
		bBlocking = true;
	}

	while ( g_FramesFinishingDecoding[nMp3DecoderSlot].Count() != 0 )
	{
		CFrameInfo * pFrameInfo = g_FramesFinishingDecoding[nMp3DecoderSlot].Head();		// Don't remove it yet as it may not be done.
		Mp3DecJoblet * pJoblet = pFrameInfo->GetJoblet();
		if ( pJoblet != NULL )
		{
			if ( pJoblet->IsComplete() == false )
			{
				if ( bBlocking )
				{
					// Decoding is still going, wait the end...
					g_mp3dec[nMp3DecoderSlot].Wait( pJoblet );
				}
				else
				{
					// We are not done and we can't block, let's not continue further as other frames won't probably be decoded either...
					return;
				}
			}
			if ( pJoblet->HasDecodingError() )
			{
				Warning( "[Sound MP3] Error when decoding MP3 frame.");
				Assert( false );
			}

			// Although this checks are not necessary, we keep them to make sure that we have proper decoding till the end
			int nInputBytesUsed = pJoblet->m_eaMp3Get - pJoblet->m_eaMp3;
			Assert( nInputBytesUsed == pFrameInfo->GetInputBufferSize() );

			int nNumberOfSamples = pJoblet->m_eaWavePut - pJoblet->m_eaWave;
			pFrameInfo->SetOutputBufferSize( nNumberOfSamples * sizeof( short ) );
			// For MP3 it should exactly match as we allocate for mono or stereo as needed
			Assert( pFrameInfo->GetOutputBufferSize() == pFrameInfo->GetOutputBufferAllocatedSize() );
			pFrameInfo->SetJoblet( NULL );				// This will indicate it is done
			pFrameInfo->SetDecodingDone( false );		// Set correct state for no assert during frame destruction

			if ( snd_ps3_mp3_spew_decode.GetBool() )
			{
				Warning("[Sound MP3] Recycle decoded joblet (%08X).\n", (int)pJoblet );
			}
			g_mp3dec[nMp3DecoderSlot].DeleteDecode( pJoblet );
		}
		else
		{
			Assert( false );		// Should not happen as these frames are not normally processed
		}

		g_FramesFinishingDecoding[nMp3DecoderSlot].RemoveAtHead();		// We took care of it, remove it from the list
		delete pFrameInfo;
	}

	// If we reach here, it means that we finished decoding all frames (and did not have to skip some).
	// So all contexts can be freed now as none of them are in use anymore.
	for (int i = 0 ; i < nNumberOfContexts ; ++i )
	{
		job_mp3dec::Context_t * pContext = g_ContextsUsedInDecoding[nMp3DecoderSlot][i];
		MemAlloc_FreeAligned( pContext );
	}
	g_ContextsUsedInDecoding[nMp3DecoderSlot].RemoveAll( );
}

// Instead of using references we use pointers so the valid state does not need to be explicit.
class CDecoder
{
public:
	CDecoder( int nFormat, int nNumberOfChannels, int nSampleRate, int * pInitialBytesToSkip );
	~CDecoder();

	// Gets the format currently handled by the decoder.
	int GetFormat() const;

	// Gets a new frame info from the decoder.
	// Returns NULL if the decoder is running out of available frame info.
	// GetInputBuffer() and GetInputBufferAllocatedSize() will return expected values.
	// GetInputBufferSize() will return -1, as it has to be filled by the caller before calling Decode().
	CFrameInfo * GetNewFrameInfo();
	// Asynchronously decodes a frame.
	// CFrameInfo must have been allocated from GetNewFrameInfo().
	// Decode() and Retrieval() are in the same order (like a FIFO stack).
	bool Decode( CFrameInfo * bufferInfo );

	// This function needs to get called regularly.
	// The current implementation will free the joblets that are done decoding
	// (so they can be used for other sounds). Otherwise some sounds that are playing slowly will hold
	// onto joblets that have finished for a long period of time.
	bool Process();

	// Indicates the minimum number of bytes the decoder can currently retrieve.
	// It is either 0 if no frame has been decoded or a frame size.
	// As the decoder does not handle retrieving of partial frames, we either retrieve a full frame or none.
	int GetMinimumNumberOfAvailableBytes() const;
	// Retrieves the next fully decoded frame (if available). Returns NULL if no decoded frame is available.
	CFrameInfo * RetrieveDecodedFrameInfo();
	// Recycles the frame info back to the decoder, so it can be re-used later.
	void FreeFrameInfo( CFrameInfo * bufferInfo );
	// Indicates that the decoder does not have any frame to decode or ready to be retrieved.
	bool IsDone() const;

	bool IsNextFrameHeaderAvailable() const;
	Mp3FrameHeader RetrieveNextFrameHeader();
	void StoreNextFrameHeader(const Mp3FrameHeader & frameHeader);
	void ClearNextFrameHeader();

	bool WasErrorReported() const;
	void ErrorReported();

	int GetNumberOfOutputSamplesPerFrame() const;

private:
	// Start do not implement
	CDecoder();
	CDecoder( const CDecoder & other );
	CDecoder & operator=( const CDecoder & other );
	// End do not implement

	CUtlStack< CFrameInfo * > m_AvailableFrames;
	mutable CUtlQueue< CFrameInfo * > m_FramesBeingDecoded;
	int m_nNumberOfCreatedBuffers;
	int * m_pInitialBytesToSkip;
	int16 m_nFormat;
	int16 m_nNumberOfChannels;
	job_mp3dec::Context_t * m_pContext;
	Mp3FrameHeader m_NextFrameHeader;
	int m_nSampleRate;
	int m_nNumberOfOutputSamplesPerFrame;
	int m_nNumberOfJobletsDecoding;
	bool m_bFirstDecoding;
	bool m_bNextFrameHeaderAvailable;
	bool m_bErrorReported;
	int8 m_nMp3DecoderSlot;

	static int8 s_nCurrentMp3DecoderSlot;

	// A MP3 frame is 1152 samples long.
	// At 22.05 KHz, it represents around 52 ms of sound. By decoding 4 frames, we have around 209 ms of sound available (i.e. 4 co-op frames).
	// At 44.1 KHz, it represents around 26 ms of sound. By decoding 4 frames, we have around 104 ms of sound available (i.e. 2 co-op frames).
	static const int MAX_NUMBER_OF_BUFFERS = 6;
};

int8 CDecoder::s_nCurrentMp3DecoderSlot = 0;

// Change this at a later point to use a pool (so several mixers can re-use the same CFrameInfo).
// Will reduce memory consumption for small sounds, or when a sound finishes and another start.

CDecoder::CDecoder( int nFormat, int nNumberOfChannels, int nSampleRate, int * pInitialBytesToSkip )
	:
	m_AvailableFrames(),
	m_FramesBeingDecoded(),
	m_nNumberOfCreatedBuffers( 0 ),
	m_pInitialBytesToSkip( pInitialBytesToSkip ),
	m_nFormat( nFormat ),
	m_nNumberOfChannels( nNumberOfChannels ),
	m_nSampleRate( nSampleRate ),
	m_nNumberOfJobletsDecoding( 0 ),
	m_bFirstDecoding( true ),
	m_bNextFrameHeaderAvailable( false ),
	m_bErrorReported( false )
{
	// Will be zeroed by the SPU the first joblet we kick
	m_pContext = (job_mp3dec::Context_t *)MemAlloc_AllocAligned( sizeof(job_mp3dec::Context_t), 128 );

	m_nNumberOfOutputSamplesPerFrame = NUMBER_OF_SAMPLES_PER_MP3_FRAME * m_nNumberOfChannels;
	if ( m_nFormat == WAVE_FORMAT_MP3 )
	{
		if ( m_nSampleRate == 22050 )
		{
			m_nNumberOfOutputSamplesPerFrame /= 2;
		}
	}
	m_nMp3DecoderSlot = s_nCurrentMp3DecoderSlot++;			// Each sound will use a different slot in round-robin manner
															// That way, over time, we are going several SPUs in parallel
	s_nCurrentMp3DecoderSlot %= NUMBER_OF_MP3_DECODER_SLOTS;
}

CDecoder::~CDecoder()
{
	bool bIsDecodingDelayed = false;
	// Wait for the remaining sounds to be decoded before we can continue further...
	while ( m_FramesBeingDecoded.Count() != 0)
	{
		Process();		// Process it so we can recycle the joblets

		CFrameInfo * pFrameInfo = RetrieveDecodedFrameInfo();
		if ( pFrameInfo != NULL )
		{
			FreeFrameInfo( pFrameInfo );
		}
		else
		{
			// It means the decoding is still going, push it in another list so we can clean it later
			pFrameInfo = m_FramesBeingDecoded.RemoveAtHead();
			Assert( pFrameInfo->IsDecodingDone() == false );
			g_FramesFinishingDecoding[m_nMp3DecoderSlot].Insert( pFrameInfo );
			// Update this, although now it is not as relevant as before
			--m_nNumberOfJobletsDecoding;
			bIsDecodingDelayed = true;
		}
	}

	while ( m_AvailableFrames.Count() != 0 )
	{
		CFrameInfo * pFrameInfo = NULL;
		m_AvailableFrames.Pop( pFrameInfo );
		delete pFrameInfo;
	}

	if ( bIsDecodingDelayed == false )
	{
		// We can free the context 
		MemAlloc_FreeAligned( m_pContext );
	}
	else
	{
		// We had to delay decoding on some frames, so we need the context too, don't free it right away.
		g_ContextsUsedInDecoding[m_nMp3DecoderSlot].AddToTail( m_pContext );
	}

	if ( m_nNumberOfJobletsDecoding != 0 )
	{
		Warning( "[Sound MP3] Leaking joblets!\n" );
	}
	
	HandleRemainingFrameInfos( m_nMp3DecoderSlot, false );
}

int CDecoder::GetFormat() const
{
	return m_nFormat;
}

// It seems that we won't need mutexes in this code as it is accessed only from one thread.
CFrameInfo * CDecoder::GetNewFrameInfo()
{
	CFrameInfo * pFrameInfo = NULL;
	if ( m_AvailableFrames.Count() != 0)
	{
		m_AvailableFrames.Pop( pFrameInfo );
		return pFrameInfo;
	}
	else
	{
		if ( m_nNumberOfCreatedBuffers >= MAX_NUMBER_OF_BUFFERS )
		{
			return NULL;
		}
		// Calculate the exact size depending of the mono and stereo (as not to waste memory).
		// TODO: Implement a pool for the frames to not fragment the memory (although we need to make sure we handle gracefully mono and stereo).
		CFrameInfo * pFrameInfo;
		const int nOutputBufferSize = m_nNumberOfOutputSamplesPerFrame * sizeof(short);
		switch ( m_nFormat)
		{
		case WAVE_FORMAT_MP3:
			pFrameInfo = new CFrameInfo( m_nFormat, MP3_INPUT_BUFFER_SIZE, nOutputBufferSize );
			break;
		case WAVE_FORMAT_TEMP:
			// Input buffer size is same size as output in this case.
			pFrameInfo = new CFrameInfo( m_nFormat, nOutputBufferSize, nOutputBufferSize );
			break;
		default:
			Assert( false );
			pFrameInfo = NULL;
			break;
		}
		++m_nNumberOfCreatedBuffers;
		return pFrameInfo;
	}
}

bool CDecoder::Decode( CFrameInfo * pFrameInfo )
{
	Assert( pFrameInfo->GetInputBufferSize() > 0 );

	// If we skip samples during Decode(), we need to take in account that the context is used by several MP3 frames in a row.
	// Skipping everything except the last 4 frames should be good enough.
	const int MIN_NUMBER_OF_FRAMES_TO_SKIP = 4;
	if ( *m_pInitialBytesToSkip >= MIN_NUMBER_OF_FRAMES_TO_SKIP * pFrameInfo->GetOutputBufferAllocatedSize() )
	{
		// We have to skip many more frames, it is safe to completely discard that one.
		// Let's free it (that way we are not limited by the max number of simultaneous frames, and the SPU decoding time).
		*m_pInitialBytesToSkip -= pFrameInfo->GetOutputBufferAllocatedSize();
		FreeFrameInfo( pFrameInfo );
		return true;		// And we assumes that the decoding is correct.
	}

	// Note that we can't skip the samples during Decode() as it would not set the context correctly.
	// When we would start the sound, the context will be still set to zero so the sound would not be exactly the same
	// in the first MP3 frame. Instead, we are going to skip when we fill the circular buffer.

	switch (m_nFormat)
	{
	case WAVE_FORMAT_MP3:
		{
			int nFlags = m_bFirstDecoding ? Mp3DecJoblet::FLAG_DECODE_INIT_CONTEXT : 0;				// The first time, we will ask the SPU to clear the context
			nFlags |= (m_nNumberOfChannels == 2) ? Mp3DecJoblet::FLAG_STEREO : Mp3DecJoblet::FLAG_MONO;
			Mp3DecJoblet * pJoblet = g_mp3dec[m_nMp3DecoderSlot].NewDecode( nFlags );
			if ( pJoblet != NULL )
			{
				++m_nNumberOfJobletsDecoding;

				pJoblet->m_nSkipSamples = 0;
				pJoblet->m_eaMp3 = ( uint8 * )( pFrameInfo->GetInputBuffer() );
				pJoblet->m_eaMp3End = pJoblet->m_eaMp3 + pFrameInfo->GetInputBufferSize();
				pJoblet->m_eaWave = ( int16 * )( pFrameInfo->GetOutputBuffer() );
				pJoblet->m_eaWaveEnd = pJoblet->m_eaWave + ( pFrameInfo->GetOutputBufferAllocatedSize() / sizeof(int16) );
				pJoblet->m_eaContext = m_pContext;			// We have only one context for all the frames on the same sound.
				pFrameInfo->SetJoblet( pJoblet );
				g_mp3dec[m_nMp3DecoderSlot].KickPending();
				m_FramesBeingDecoded.Insert( pFrameInfo );

				m_bFirstDecoding = false;																// Not first decoding anymore, won't reset the context next time

				if ( snd_ps3_mp3_spew_decode.GetBool() )
				{
					Warning( "[Sound MP3] Start decoding of joblet (%08X).\n", (int)pJoblet );
				}
				return true;
			}
			else
			{
				Assert( false );
				Warning( "[Sound MP3] No available joblet for MP3 decompression. Skip the frame.\n" );

				int nNumberOfAllocated = 0;
				int nNumberOfCompleted = 0;
				int nNumberOfDecodingErrors = 0;

				int nNumberOfJoblets = g_mp3dec[m_nMp3DecoderSlot].GetNumberOfJobletsForDebugging();
				for ( int i = 0 ; i < nNumberOfJoblets ; ++i )
				{
					const Mp3DecJoblet & joblet = g_mp3dec[m_nMp3DecoderSlot].GetJobletForDebugging( i );
					if ( joblet.IsAllocated() )
					{
						++nNumberOfAllocated;
					}
					if ( joblet.IsComplete() )
					{
						++nNumberOfCompleted;
					}
					if ( joblet.HasDecodingError() )
					{
						++nNumberOfDecodingErrors;
					}
				}
				int nNumberOfFree = nNumberOfJoblets - nNumberOfAllocated;
				Warning( "            Summary of current joblets: Free: %d - Allocated: %d - Completed: %d - Decoding errors: %d\n", nNumberOfFree, nNumberOfAllocated, nNumberOfCompleted, nNumberOfDecodingErrors );

				FreeFrameInfo( pFrameInfo );

				// There are some cases where the MP3 decoder ring buffer can put itself in a bad state.
				// And never allocate anything even if everything is free. Call KickPending() even if we did not get a joblet to reset the MP3 decoder state.
				g_mp3dec[m_nMp3DecoderSlot].KickPending();
				return false;
			}
		}

	case WAVE_FORMAT_TEMP:
		{
			// This implementation does not have anything to decode, we copy straight from one buffer to the other.
			int nBytesToCopy = MIN( pFrameInfo->GetInputBufferSize(), pFrameInfo->GetInputBufferAllocatedSize() );
			int8 * pInputBuffer = (int8 *)pFrameInfo->GetInputBuffer();
#if _DEBUG
			int nHeader = *(int *)pInputBuffer;
			Assert( ( nHeader == 0x12345678 ) || ( nHeader == 0x78563412 ) );
#endif
			const int HEADER_TO_SKIP = sizeof( int );
			nBytesToCopy -= HEADER_TO_SKIP;
			pInputBuffer += HEADER_TO_SKIP;
			memcpy( pFrameInfo->GetOutputBuffer(), pFrameInfo->GetInputBuffer(), nBytesToCopy );
			pFrameInfo->SetOutputBufferSize( nBytesToCopy );
			pFrameInfo->SetDecodingDone( true );
			m_FramesBeingDecoded.Insert( pFrameInfo );
			return true;
		}

	default:
		Assert( false );
		return false;
	}
}

// This method is going to get called regularly. We are going to use it as a way to free the Mp3 Joblet.
// We don't have to keep the joblet around until we are actively using the memory, once the jobs are done,
// we can free them as is.
// Something to consider: We could optimize this a bit by having a queue for being decoded and a queue for the decoded frames.
// This would avoid testing for NULL several times over several frames (although it would add a bit of memory overhead too).
bool CDecoder::Process()
{
	bool bDecodingSuccessful = true;
	int nCount = m_FramesBeingDecoded.Count();
	for ( int i = 0 ; i < nCount ; ++i )
	{
		CFrameInfo * pFrameInfo = m_FramesBeingDecoded[i];
		Mp3DecJoblet * pJoblet = pFrameInfo->GetJoblet();
		if ( pJoblet == NULL )
		{
			// Already finished and processed... Or not needed for the temp format.
			continue;			// Continue so we have a chance to free the joblets for the later frames.
		}
		bool bDone = pJoblet->IsComplete();
		if ( bDone == false )
		{
			// This one is not done yet, because it is a FIFO queue, the next frame is certainly not done either. Stop here.
			break;
		}
		if ( pJoblet->HasDecodingError() )
		{
			Warning( "[Sound MP3] Error when decoding MP3 frame.");
			Assert( false );
			bDecodingSuccessful = false;
		}

		int nInputBytesUsed = pJoblet->m_eaMp3Get - pJoblet->m_eaMp3;
		Assert( nInputBytesUsed == pFrameInfo->GetInputBufferSize() );

		// In any case, we are done with this joblet, update the states
		int nNumberOfSamples = pJoblet->m_eaWavePut - pJoblet->m_eaWave;
		if ( nNumberOfSamples != m_nNumberOfOutputSamplesPerFrame )
		{
			Assert( false );
//			if ( nNumberOfSamples != 0 )
			{
				// The difference can happen on the last frame (does not have useful enough information to be decoded by SONY decoder).
				// Assert only in the other cases.
				bDecodingSuccessful = false;
			}
			// Somehow there is a mismatch between the returned samples and the expected samples
			// This a bug in the SPU decoder. It does not even fill the frame properly.
			nNumberOfSamples = m_nNumberOfOutputSamplesPerFrame;
			// Clear the buffer, we won't hear the sound at all.
			memset( pFrameInfo->GetOutputBuffer(), 0, nNumberOfSamples * sizeof(short) );
		}

		pFrameInfo->SetOutputBufferSize( nNumberOfSamples * sizeof( short ) );
		// For MP3 it should exactly match as we allocate for mono or stereo as needed
		Assert( pFrameInfo->GetOutputBufferSize() == pFrameInfo->GetOutputBufferAllocatedSize() );
		pFrameInfo->SetJoblet( NULL );				// This will indicate it is done
		pFrameInfo->SetDecodingDone( true );		// And keep it coherent

		if ( snd_ps3_mp3_spew_decode.GetBool() )
		{
			Warning("[Sound MP3] Recycle decoded joblet (%08X).\n", (int)pJoblet );
		}
		g_mp3dec[m_nMp3DecoderSlot].DeleteDecode( pJoblet );

		--m_nNumberOfJobletsDecoding;
		Assert( m_nNumberOfJobletsDecoding >= 0 );
	}
	return bDecodingSuccessful;
}

int CDecoder::GetMinimumNumberOfAvailableBytes() const
{
	if ( m_FramesBeingDecoded.Count() != 0 )
	{
		CFrameInfo * pFrameInfo = m_FramesBeingDecoded.Head();
		if ( pFrameInfo != NULL )
		{
			if ( pFrameInfo->IsDecodingDone() )
			{
				Assert( pFrameInfo->GetOutputBufferSize() > 0 );
				return pFrameInfo->GetOutputBufferSize();
			}
		}
	}
	return 0;
}

CFrameInfo * CDecoder::RetrieveDecodedFrameInfo()
{
	if ( m_FramesBeingDecoded.Count() != 0 )
	{
		CFrameInfo * pFrameInfo = m_FramesBeingDecoded.Head();
		if ( pFrameInfo != NULL )
		{
			if ( pFrameInfo->IsDecodingDone() )
			{
				Assert( pFrameInfo->GetOutputBufferSize() > 0 );

				// Because it has been decoded, we can remove it from head.
				// If we called RetrieveDecodedFrameInfo() directly without calling GetMinimumNumberOfAvailableBytes() first,
				// like in ~CDecoder(), we would leak the frame info and its joblet in some cases.
				m_FramesBeingDecoded.RemoveAtHead();
				return pFrameInfo;
			}
		}
	}
	return NULL;
}

FORCEINLINE
void CDecoder::FreeFrameInfo( CFrameInfo * pFrameInfo )
{
	pFrameInfo->SetInputBufferSize( -1 );
	pFrameInfo->SetOutputBufferSize( -1 );
	pFrameInfo->SetDecodingDone( false );
	Assert( pFrameInfo->GetJoblet() == NULL );
	m_AvailableFrames.Push( pFrameInfo );
}

FORCEINLINE
bool CDecoder::IsDone() const
{
	return ( m_FramesBeingDecoded.Count() == 0 );
}

FORCEINLINE
bool CDecoder::IsNextFrameHeaderAvailable() const
{
	return m_bNextFrameHeaderAvailable;
}

FORCEINLINE
Mp3FrameHeader CDecoder::RetrieveNextFrameHeader()
{
	Assert( m_bNextFrameHeaderAvailable );
	m_bNextFrameHeaderAvailable = false;
	return m_NextFrameHeader;
}

FORCEINLINE
void CDecoder::StoreNextFrameHeader(const Mp3FrameHeader & frameHeader)
{
	Assert( m_bNextFrameHeaderAvailable == false );
	m_bNextFrameHeaderAvailable = true;
	m_NextFrameHeader = frameHeader;
}

FORCEINLINE
void CDecoder::ClearNextFrameHeader()
{
	m_bNextFrameHeaderAvailable = false;
}

FORCEINLINE
bool CDecoder::WasErrorReported() const
{
	return m_bErrorReported;
}

FORCEINLINE
void CDecoder::ErrorReported()
{
	m_bErrorReported = true;
}

FORCEINLINE
int CDecoder::GetNumberOfOutputSamplesPerFrame() const
{
	return m_nNumberOfOutputSamplesPerFrame;
}

class CAudioMixerWavePs3Mp3 : public CAudioMixerWave
{
public:
	typedef CAudioMixerWave BaseClass;

	CAudioMixerWavePs3Mp3( IWaveData *data, int nInitialStreamPosition, int nSkipInitialSamples, bool bUpdateDelayForChoreo );
	virtual ~CAudioMixerWavePs3Mp3( void );

	virtual void			Mix( IAudioDevice *pDevice, channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress );

	virtual int				GetOutputData( void **pData, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );

	virtual bool			IsSetSampleStartSupported() const;
	virtual void			SetSampleStart( int newPosition );
	virtual int				GetPositionForSave();
	virtual void			SetPositionFromSaved( int savedPosition );

	virtual int				GetMixSampleSize() { return CalcSampleSize( 16, m_NumChannels ); }

	virtual bool			IsReadyToMix();
	virtual bool			ShouldContinueMixing();

private:
	Mp3Error				GetMp3BlocksAndSubmitToDecoder( );
	int						UpdatePositionForLoopingInBytes( int *pNumRequestedSamples );
	int						RetrieveDecodedFrames( );
	int						GetPCMBytes( int nNumberOfBytesRequested, char *pData );
	bool					ReadBytesFromStreamer(int nBytesToRead, void * pBufferToFill, int nOffsetInStream);

	CDecoder				m_Decoder;

	// output buffer, decoded pcm samples, a staging circular buffer, waiting for mixer requests
	// due to staging nature, contains decoded samples from multiple input buffers
	CCircularBuffer			*m_pPCMSamples;

	int						m_SampleRate;
	int						m_NumChannels;
	// maximum possible decoded samples
	int						m_nNumberOfDecodedSamplesInBytes;

	// decoded sample position
	int						m_SamplePositionInBytes;
	// current data marker
	int						m_LastDataOffsetInBytes;
	int						m_DataOffsetInBytes;
	// total bytes of data
	int						m_SourceSizeInBytes;

	// Number of bytes that we have to skip at the beginning (used to reduce I/O pressure) 
	int						m_nSkipInitialBytes;

	// Used for loops
	int						m_nLoopFrameOffset;
	int						m_nLoopStart;
	int						m_nCurrentDecodedSamplePosition;

	// timers
	unsigned int			m_StartTime;
	unsigned int			m_LastSuccessfulStreamTime;
	unsigned int			m_LastDecodingStartTime;
	unsigned int			m_LastDrainTime;
	unsigned int			m_LastPollTime;

	int						m_hMixerList;
	int						m_Error;

	// Store the last samples passed to the sound engine. Used in case the streamer is behind.
	int8					m_LastSample[4];

	bool					m_bStartedMixing : 1;
	bool					m_bFinished : 1;
	bool					m_bLooped : 1;
	bool					m_bUpdateDelayForChoreo : 1;
};

CON_COMMAND( snd_ps3_mp3_info, "Spew PS3 MP3 Info" )
{
	Msg( "[Sound] Active MP3 Mixers:  %d\n", g_Ps3Mp3MixerList.Count() );
	char nameBuf[MAX_PATH];
	for ( int hMixer = g_Ps3Mp3MixerList.Head(); hMixer != g_Ps3Mp3MixerList.InvalidIndex(); hMixer = g_Ps3Mp3MixerList.Next( hMixer ) )
	{
		CAudioMixerWavePs3Mp3 *pMp3Mixer = g_Ps3Mp3MixerList[hMixer];
		Msg( "  rate:%5d ch:%1d '%s'\n", pMp3Mixer->GetSource()->SampleRate(), pMp3Mixer->GetSource()->IsStereoWav() ? 2 : 1, pMp3Mixer->GetSource()->GetFileName(nameBuf, sizeof(nameBuf)) );
	}
}

#ifdef DEBUG_PS3_MP3
static char strFileName[256];
#endif

CAudioMixerWavePs3Mp3::CAudioMixerWavePs3Mp3( IWaveData *pData, int nInitialStreamPosition, int nSkipInitialSamples, bool bUpdateDelayForChoreo )
	:
	CAudioMixerWave( pData ),
	m_Decoder( pData->Source().Format(), m_pData->Source().IsStereoWav() ? 2 : 1, m_pData->Source().SampleRate(), &m_nSkipInitialBytes )
{
	Assert( dynamic_cast<CAudioSourceWave *>(&m_pData->Source()) != NULL );

	m_Error = 0;

	m_NumChannels = m_pData->Source().IsStereoWav() ? 2 : 1;
	m_SampleRate = m_pData->Source().SampleRate();
	m_bLooped = m_pData->Source().IsLooped();
	m_nNumberOfDecodedSamplesInBytes = m_pData->Source().SampleCount();
	m_SourceSizeInBytes = m_pData->Source().DataSize();
	m_nSkipInitialBytes = nSkipInitialSamples * m_NumChannels * sizeof( short );

	const char * pChannels = ( m_NumChannels == 2 ) ? "Stereo" : "Mono";
	char strSoundName[256];
	//Msg( "[Sound] MP3 decoding started: %s - %s - %d Hz.\n", pData->Source().GetFileName( strSoundName, sizeof( strSoundName ) ), pChannels, m_SampleRate );

	m_LastDataOffsetInBytes = nInitialStreamPosition;
	m_DataOffsetInBytes = nInitialStreamPosition;
	m_SamplePositionInBytes = 0;
	if ( nInitialStreamPosition )
	{
		// Well, this is incorrect, it should be in samples and not in bytes.
		// (PS3 does not support seek table yet so this code is not exercised, but this is potentially create issues later).
		// TODO: Fix all the usage of m_SamplePositionInBytes.
		m_SamplePositionInBytes = m_pData->Source().StreamToSamplePosition( nInitialStreamPosition );

		// IMPORTANT: When initial stream position is requested we need
		// to make the MP3 mixer obeys rules of how CAudioMixerWave::GetSampleLoadRequest
		// is using its members "m_fsample_index", "m_sample_loaded_index" and "m_sample_max_loaded"
		// If the implementation of CAudioMixerWave::GetSampleLoadRequest changes, then
		// MP3 mixer should be changed accordingly. 

		CAudioMixerWave::m_fsample_index = m_SamplePositionInBytes;
		CAudioMixerWave::m_sample_loaded_index = m_SamplePositionInBytes;
		CAudioMixerWave::m_sample_max_loaded = m_SamplePositionInBytes + 1;
	}

	m_bStartedMixing = false;
	m_bFinished = false;
	m_bUpdateDelayForChoreo = bUpdateDelayForChoreo;

	m_LastPollTime = 0;
	m_LastDrainTime = 0;

	if ( m_bUpdateDelayForChoreo )							// Do not test for enabled here as we want g_nDelayForChoreoNumberOfSoundsPlaying to be always accurate
	{
		++g_nDelayForChoreoNumberOfSoundsPlaying;
		g_nDelayForChoreoLastCheckInMs = m_StartTime;		// Not necessary as g_nDelayForChoreoNumberOfSoundsPlaying != 0 prevents the Reset, but does not hurt either
	}

	m_nLoopFrameOffset = -1;
	CAudioSourceWave &source = reinterpret_cast<CAudioSourceWave &>(m_pData->Source());
	m_nLoopStart = source.GetLoopingInfo( NULL, NULL, NULL );
	m_nCurrentDecodedSamplePosition = m_SamplePositionInBytes;

	int stagingSize = PCM_STAGING_BUFFER_TIME * m_SampleRate * m_NumChannels * sizeof( short ) * 0.001f;
	m_pPCMSamples = AllocateCircularBuffer( AlignValue( stagingSize, 4 ) );

	m_StartTime = Plat_MSTime();
	m_LastSuccessfulStreamTime = 0;
	m_LastDecodingStartTime = 0;			// 0 means that we did not start a decoding yet.

	V_memset( m_LastSample, 0, sizeof( m_LastSample ) );

	m_hMixerList = g_Ps3Mp3MixerList.AddToTail( this );

#ifdef DEBUG_PS3_MP3
	if ( snd_ps3_mp3_record.GetBool() )
	{
		static int nCounter = 0;
		char * pDot = strstr( strSoundName, ".");
		if ( pDot != NULL )
		{
			// Remove ".wav" as PS3 does not save it properly a file with 2 .wav extensions
			*pDot = '\0';
		}
		sprintf( strFileName, "%s_%d.wav", strSoundName, nCounter );
		WaveCreateTmpFile( strFileName, m_SampleRate, 16, m_NumChannels );
		++nCounter;
	}
#endif

	if ( snd_ps3_mp3_spew_mixers.GetBool() )
	{
		char nameBuf[MAX_PATH];
		Warning( "[Sound MP3] 0x%8.8x (%2d), Mixer Alloc, '%s'\n", (unsigned int)this, g_Ps3Mp3MixerList.Count(), m_pData->Source().GetFileName(nameBuf, sizeof(nameBuf)) );
	}
}

CAudioMixerWavePs3Mp3::~CAudioMixerWavePs3Mp3( void )
{
	if ( m_bUpdateDelayForChoreo )										// Do not test for enabled here as we want g_nDelayForChoreoNumberOfSoundsPlaying to be always accurate
	{
		--g_nDelayForChoreoNumberOfSoundsPlaying;
		Assert( g_nDelayForChoreoNumberOfSoundsPlaying >= 0 );
		g_nDelayForChoreoLastCheckInMs = Plat_MSTime();					// Critical to make sure that we are reseting the latency at least N ms after the last choreo sound
	}

/*
	char strSoundName[256];
	if ( m_bFinished )
	{
		Msg( "[Sound MP3] Sound '%s' finished.\n", m_pData->Source().GetFileName( strSoundName, sizeof( strSoundName ) ) );
	}
	else
	{
		Msg( "[Sound MP3] Sound '%s' stopped.\n", m_pData->Source().GetFileName( strSoundName, sizeof( strSoundName ) ) );
	}
*/

	if ( m_pPCMSamples )
	{
		FreeCircularBuffer( m_pPCMSamples );
	}

	g_Ps3Mp3MixerList.Remove( m_hMixerList );

	if ( snd_ps3_mp3_spew_mixers.GetBool() )
	{
		char nameBuf[MAX_PATH];
		Warning( "[Sound MP3] 0x%8.8x (%2d), Mixer Freed, '%s'\n", (unsigned int)this, g_Ps3Mp3MixerList.Count(), m_pData->Source().GetFileName(nameBuf, sizeof(nameBuf)) );
	}
}

void CAudioMixerWavePs3Mp3::Mix( IAudioDevice *pDevice, channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress )
{
	if ( m_NumChannels == 1 )
	{
		pDevice->Mix16Mono( pChannel, (short *)pData, outputOffset, inputOffset, fracRate, outCount, timecompress );
	}
	else
	{
		pDevice->Mix16Stereo( pChannel, (short *)pData, outputOffset, inputOffset, fracRate, outCount, timecompress );
	}
}

//-----------------------------------------------------------------------------
// Looping is achieved in two passes to provide a circular view of the linear data.
// Pass1: Clamps a sample request to the end of data.
// Pass2: Snaps to the loop start, and returns the number of samples to discard, could be 0,
// up to the expected loop sample position.
// Returns the number of samples to discard, or 0.
//-----------------------------------------------------------------------------
int CAudioMixerWavePs3Mp3::UpdatePositionForLoopingInBytes( int *pNumRequestedBytes )
{
	if ( !m_bLooped )
	{
		// not looping, no fixups
		return 0;
	}

	int nNumLeadingSamplesInBytes;
	int nNumTrailingSamplesInBytes;
	CAudioSourceWave &source = reinterpret_cast<CAudioSourceWave &>(m_pData->Source());
	int loopSampleStart = source.GetLoopingInfo( NULL, &nNumLeadingSamplesInBytes, &nNumTrailingSamplesInBytes );

	int nNumRemainingBytes = ( m_nNumberOfDecodedSamplesInBytes - nNumTrailingSamplesInBytes ) - m_SamplePositionInBytes;

	// possibly straddling the end of data (and thus about to loop)
	// want to split the straddle into two regions, due to loops possibly requiring a trailer and leader of discarded samples
	if ( nNumRemainingBytes > 0 )
	{
		// first region, all the remaining samples, clamped until end of desired data
		*pNumRequestedBytes = MIN( *pNumRequestedBytes, nNumRemainingBytes );

		// nothing to discard
		return 0;
	}
	else if ( nNumRemainingBytes == 0 )
	{
		// at exact end of desired data, snap the sample position back
		// the position will be correct AFTER discarding decoded trailing and leading samples
		m_SamplePositionInBytes = loopSampleStart;
		m_nCurrentDecodedSamplePosition = loopSampleStart;		// May not be the best place to put this here...

		// clamp the request
		nNumRemainingBytes = ( m_nNumberOfDecodedSamplesInBytes - nNumTrailingSamplesInBytes ) - m_SamplePositionInBytes;
		Assert( nNumRemainingBytes >= 0 );
		*pNumRequestedBytes = MIN( *pNumRequestedBytes, nNumRemainingBytes );

		// flush these samples so the sample position is the real loop sample starting position
		return nNumTrailingSamplesInBytes + nNumLeadingSamplesInBytes;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Get and submit MP3 frame(s). The decoder must stay blocks ahead of mixer
// so the decoded samples are available for peeling.
// The MP3 buffers may be delayed from the audio data cache due to async i/o latency.
// Returns < 0 if error, 0 if no decode started, 1 if decode submitted.
//-----------------------------------------------------------------------------
Mp3Error CAudioMixerWavePs3Mp3::GetMp3BlocksAndSubmitToDecoder( )
{
	Mp3Error nStatus = ERROR_MP3_NO_DECODING_SUBMITTED;

	// With MP3, we should:
	//	- Read 4 bytes to get the frame header
	//	- Read the frame
	// We could read several frames at a time if necessary (and then decompress each one of them).

	// We are going to decompress several frames at a time
	// Depending of the size of MP3_INPUT_BUFFER_SIZE. We could change this as necessary.

	// track the currently submitted offset
	// this is used as a cheap method for save/restore because an MP3 seek table is not available
	m_LastDataOffsetInBytes = m_DataOffsetInBytes;

	// the input buffer can never be less than a single MP3 frame (buffer size is multiple blocks)
	int nBytesToRead = MIN( m_SourceSizeInBytes - m_DataOffsetInBytes, MP3_INPUT_BUFFER_SIZE );

	Mp3FrameHeader nHeader;
	if ( nBytesToRead < sizeof(nHeader) )
	{
		// With MP3, we can have some padding at the end
		if ( m_bLooped == false )
		{
			// end of file, no more data to decode
			// not an error, because decoder finishes long before samples drained
			m_bFinished = m_Decoder.IsDone();
			return nStatus;
		}

		// start from beginning of loop
		if ( m_nLoopFrameOffset != -1 )
		{
			// Used the looping position that we already found in a previous iteration
			m_DataOffsetInBytes = m_nLoopFrameOffset;
			nBytesToRead = MIN( m_SourceSizeInBytes - m_DataOffsetInBytes, MP3_INPUT_BUFFER_SIZE );
		}
	}

	while ( nBytesToRead > 0 )
	{
		if ( nBytesToRead < sizeof(nHeader))
		{
			// EOF
			if ( m_Decoder.GetFormat() == WAVE_FORMAT_TEMP )
			{
				Assert( nBytesToRead == 0 );		// In this format, the size should be exact
			}
			// In MP3, we can have some padding added to the file.
			return nStatus;
		}

		if ( m_Decoder.IsNextFrameHeaderAvailable() )
		{
			nHeader = m_Decoder.RetrieveNextFrameHeader();
		}
		else
		{
			// We did not cache it off from a previous frame, read it now
			bool bHeaderRead = ReadBytesFromStreamer( sizeof(nHeader), &nHeader, m_DataOffsetInBytes );
			if ( bHeaderRead == false )
			{
				if ( m_DataOffsetInBytes == 0 )
				{
					// This case could happen when the streamer did not start yet (for example in debug)
					return ERROR_MP3_IO_NO_MP3_DATA;
				}

#if _DEBUG
				char strSoundName[256];
				m_pData->Source().GetFileName( strSoundName, sizeof( strSoundName ) );
				Warning( "[Sound MP3] Incomplete MP3 frame - 1 - for file: '%s'.\n", strSoundName );
				Assert( false );
#endif
				return ERROR_MP3_IO_NO_MP3_DATA;		// This case can happen if the BluRay is late. Instead of stopping the sound, let's try the next iteration.
			}
		}
		// Don't shift m_DataOffset yet, let's make sure we can read the full frame...
		nBytesToRead -= sizeof( nHeader );

		int nFrameSize;

		switch ( m_Decoder.GetFormat() )
		{
		case WAVE_FORMAT_TEMP:
			{
				int * pnTempHeader = (int *)&nHeader;
				Assert( ( *pnTempHeader == 0x12345678 ) || ( *pnTempHeader == 0x78563412 ) );
				nFrameSize = m_Decoder.GetNumberOfOutputSamplesPerFrame() * sizeof(short);	// Matches the output size of an MP3 frame.
				break;
			}

		case WAVE_FORMAT_MP3:
			{
				// Remaining bytes to read (without the header as we already read it)
				// We do not want to use the padding information as it is often incorrect for 22050 Hz sound. We'll handle it correctly.
				nFrameSize = nHeader.GetFrameLengthIncludingHeader() - sizeof(nHeader);

				if ( nHeader.CheckSync() == 0 )
				{
					char strSoundName[256];
					m_pData->Source().GetFileName( strSoundName, sizeof( strSoundName ) );

					Warning( "[Sound MP3] Did not find the frame as expected for file: '%s'. Frame offset: %d.\n", strSoundName, m_DataOffsetInBytes );
					Warning( "            Frame header: %02X %02X %02X %02X\n", nHeader.m_bits[0], nHeader.m_bits[1], nHeader.m_bits[2], nHeader.m_bits[3] );
#ifndef _CERT
					DebuggerBreak();
#endif
					return ERROR_MP3_CANT_DECODE;		// We read the header successfully, but it is invalid. the streamer actually returned corrupt data (for whatever reasons).
														// At that point, we can't really recover as we would have to find the proper header. It is better to stop playing this sound.
				}
				break;
			}

		default:
			Assert( false );
			nFrameSize = nBytesToRead + 1;		// Just to skip till the end...
			break;
		}

		CFrameInfo * pFrameInfo = m_Decoder.GetNewFrameInfo();
		if ( pFrameInfo == NULL )
		{
			// The decoder does not have any more frame available, we have to wait more decoding to be done.
			m_Decoder.StoreNextFrameHeader( nHeader );		// Re-use the header we just read (so we will reduce potential side-effects with the streamer).
			break;
		}

		pFrameInfo->SetDataOffset( m_DataOffsetInBytes );

		char * pCurrentFrameBuffer = (char *)pFrameInfo->GetInputBuffer();
		// MP3 frames need the header
		*(Mp3FrameHeader *)pCurrentFrameBuffer = nHeader;
		pCurrentFrameBuffer += sizeof(nHeader);

		nFrameSize = MIN( nFrameSize, nBytesToRead );	// In some cases, like 22050 Hz sounds
														// the last frame is only half the expected size.
														// So let's make it work in this case too.

		bool bIsFrameCopied = ReadBytesFromStreamer( nFrameSize, pCurrentFrameBuffer, m_DataOffsetInBytes + sizeof(nHeader) );
		if ( bIsFrameCopied == false )
		{
#if _DEBUG
			char strSoundName[256];
			m_pData->Source().GetFileName( strSoundName, sizeof( strSoundName ) );
			Warning( "[Sound MP3] Incomplete MP3 frame - 2 - for file: '%s'.\n", strSoundName );
			Assert( false );		// This should not happen as we had earlier enough bytes for a frame
									// but after more testing with the BluRay, it can happen 
#endif

			// Because we allocated a frame info, we have to free it here (or we could leak it).
			m_Decoder.FreeFrameInfo( pFrameInfo );
			m_Decoder.StoreNextFrameHeader( nHeader );		// Re-use the header we just read (so we will reduce potential side-effects with the streamer).

			return ERROR_MP3_IO_NO_MP3_DATA;				// This can happen if the streaming is late. Instead of stopping the sound, let's try the next iteration.
		}

		nBytesToRead -= nFrameSize;
		// Update m_DataOffset only when we are sure we have a buffer read completely (in some cases, if the BluRay is really late, it could fail sending us a full frame).
		m_DataOffsetInBytes += sizeof(nHeader) + nFrameSize;
		pCurrentFrameBuffer += nFrameSize;

		nFrameSize += sizeof(nHeader);	// Add back the MP3 header as the decoder will expect it
		Assert( nFrameSize <= pFrameInfo->GetInputBufferAllocatedSize() );

		pFrameInfo->SetInputBufferSize( nFrameSize );
		if ( m_Decoder.Decode( pFrameInfo ) )
		{
			nStatus = ERROR_MP3_DECODING_SUBMITTED;
			m_LastDecodingStartTime = Plat_MSTime();			// Update this every time we send a frame info to the decoder
		}

		// We just read a frame, let's try to read another frame if we can
		nBytesToRead = MIN( m_SourceSizeInBytes - m_DataOffsetInBytes, MP3_INPUT_BUFFER_SIZE );
	}

	return nStatus;
}

//-----------------------------------------------------------------------------
// Copy N bytes from the streamer.
// Returns true if successful, false if some bytes could not be copied. (In that case we consider the whole read unsuccessful).
//-----------------------------------------------------------------------------
bool CAudioMixerWavePs3Mp3::ReadBytesFromStreamer(int nBytesToRead, void * pBufferToFill, int nOffsetInStream)
{
	char * pCurrentPointer = (char *)pBufferToFill;
	int nOffset = 0;
	while ( nBytesToRead > 0 )
	{
		void * pFrame;
		int nAvailable = m_pData->ReadSourceData( (void **)&pFrame, nOffsetInStream + nOffset, nBytesToRead, NULL );
		if ( nAvailable == 0 )
		{
			// This should not happen as we normally read if we have enough bytes to read
			// But could happen if the streamer did not start yet (especially in DEBUG).
			if ( m_DataOffsetInBytes + nOffset != 0 )
			{
				Assert( false );
			}
			return false;
		}
		V_memcpy( pCurrentPointer, pFrame, nAvailable );
		nBytesToRead -= nAvailable;
		pCurrentPointer += nAvailable;
		nOffset += nAvailable;
	}
	m_LastSuccessfulStreamTime = Plat_MSTime();
	return true;
}

//-----------------------------------------------------------------------------
// Drain the MP3 Decoder into the staging circular buffer of PCM for mixer.
// Fetch new MP3 samples for the decoder.
//-----------------------------------------------------------------------------
int CAudioMixerWavePs3Mp3::RetrieveDecodedFrames( )
{
	// We should be able to ask the state regularly (without being worried about slowing things down like X360).

	int nNumAvailableForWrite = m_pPCMSamples->GetWriteAvailable();
	for ( ; ; )
	{
		// This will return 0 if nothing has been decoded, or the size of a decoded frame.
		int nMinimumAvailable = m_Decoder.GetMinimumNumberOfAvailableBytes();
		if ( nMinimumAvailable == 0 )
		{
			break;
		}
		if ( nMinimumAvailable > nNumAvailableForWrite )
		{
			// We can't fit a whole frame in the write buffer (and don't want to send a partial frame either)
			break;
		}

		CFrameInfo * pFrameInfo = m_Decoder.RetrieveDecodedFrameInfo();
		if ( pFrameInfo == NULL )
		{
			// No more sample decoded
			Assert( false );		// Earlier we had enough available bytes, how come we can't have a frame now?
			break;
		}
		int nOutputBufferSize = pFrameInfo->GetOutputBufferSize();
		Assert( nOutputBufferSize > 0 );

		if ( m_nSkipInitialBytes < nOutputBufferSize )
		{
			// There is less than a frame to skip (and probably even nothing to skip)
			char * pBufferToWrite = ( char * )pFrameInfo->GetOutputBuffer();
			pBufferToWrite += m_nSkipInitialBytes;			// Skip initial bytes if there is some remaining
			int nBytesToWrite = nOutputBufferSize - m_nSkipInitialBytes;
			m_pPCMSamples->Write( pBufferToWrite, nBytesToWrite );
			nNumAvailableForWrite -= nBytesToWrite;

			m_nSkipInitialBytes = 0;						// Nothing to skip now
		}
		else
		{
			m_nSkipInitialBytes -= nOutputBufferSize;		// Skipped the frame completely
		}

		int nNextPosition = m_nCurrentDecodedSamplePosition + nOutputBufferSize;
		if ( ( m_nLoopStart >= m_nCurrentDecodedSamplePosition ) && ( m_nLoopStart < nNextPosition ) )
		{
			// The loop is actually within this frame.
			// If not done already, we are going to store the offset and the number of samples to skip.
			// That way, when we have to loop we know exactly what offset to read from (and how many samples to skip within a frame).
			if ( m_nLoopFrameOffset == -1 )
			{
				m_nLoopFrameOffset = pFrameInfo->GetDataOffset();

				// As we detected the loop, we update the streamer so it will properly stream the buffers after the loop
				// As the initial loop point in the streamer was inaccurate.
				m_pData->UpdateLoopPosition( m_nLoopFrameOffset );
			}
		}

		m_nCurrentDecodedSamplePosition = nNextPosition;
		m_Decoder.FreeFrameInfo( pFrameInfo );
	}

	// queue up more blocks for the decoder
	// the decoder will always finish ahead of the mixer, submit nothing, and the mixer will still be draining
	Mp3Error decodeStatus = GetMp3BlocksAndSubmitToDecoder( );
	if ( decodeStatus < 0 && decodeStatus != ERROR_MP3_IO_NO_MP3_DATA )
	{
		m_Error = decodeStatus;
		return -1;
	}
	return 1;
}

//-----------------------------------------------------------------------------
// Drain the PCM staging buffer.
// Copy bytes (numBytesToCopy && pData). Return actual copied.
// Flush Bytes (numBytesToCopy && !pData). Return actual flushed.
// Query available number of bytes (!numBytesToCopy && !pData). Returns available.
// TODO: Revisit this API, create smaller functions instead.
//-----------------------------------------------------------------------------
int CAudioMixerWavePs3Mp3::GetPCMBytes( int numBytesToCopy, char *pData )
{
	int numReadyBytes = m_pPCMSamples->GetReadAvailable();

	// peel sequential samples from the stream's staging buffer
	int numCopiedBytes = 0;
	int numRequestedBytes = MIN( numBytesToCopy, numReadyBytes );
	if ( numRequestedBytes )
	{
		int nNumberOfRequestedBytes = numRequestedBytes;
		if ( pData )
		{
			// copy to caller
			m_pPCMSamples->Read( pData, nNumberOfRequestedBytes );
		}
		else
		{
			// flush
			m_pPCMSamples->Advance( nNumberOfRequestedBytes );
		}
		numCopiedBytes += numRequestedBytes;
	}

	if ( snd_ps3_mp3_spew_drain.GetBool() )
	{
		char nameBuf[MAX_PATH];
		const char *pOperation = ( numBytesToCopy && !pData ) ? "Flushed" : "Copied";
		Warning( "[Sound MP3] 0x%8.8x, SamplePosition: %d, Ready: %d, Requested: %d, %s: %d, Elapsed: %d ms '%s'\n", 
			(unsigned int)this, m_SamplePositionInBytes, numReadyBytes, numBytesToCopy, pOperation, numCopiedBytes, Plat_MSTime() - m_LastDrainTime, m_pData->Source().GetFileName(nameBuf, sizeof(nameBuf)) );
	}
	m_LastDrainTime = Plat_MSTime();

	if ( numBytesToCopy )
	{
		// could be actual flushed or actual copied
		return numCopiedBytes;
	}

	if ( !pData )
	{
		// satisfy query for available
		return numReadyBytes;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Stall mixing until initial buffer of decoded samples are available.
//-----------------------------------------------------------------------------
bool CAudioMixerWavePs3Mp3::IsReadyToMix()
{
	if ( m_Error )
	{
		// error has been set
		// let mixer try to get unavailable samples, which causes the real abort
		return true;
	}

	if ( m_bStartedMixing )
	{
		// decoding process has started
		return true;
	}

	bool bDecodingSuccessful = m_Decoder.Process();
	if ( bDecodingSuccessful == false )
	{
		if ( m_Decoder.WasErrorReported() == false )
		{
			char strBuffer[256];
			Warning( "[Sound MP3] Decoding error with sound '%s'.\n", m_pData->Source().GetFileName( strBuffer, sizeof( strBuffer ) ) );
			m_Decoder.ErrorReported();
		}
	}

	// must have buffers in flight before mixing can begin
	if ( m_DataOffsetInBytes == m_LastDataOffsetInBytes )
	{
		// keep trying to get data, async i/o has some allowable latency
		Mp3Error decodeStatus = GetMp3BlocksAndSubmitToDecoder( );
		if ( decodeStatus < 0 && decodeStatus != ERROR_MP3_IO_NO_MP3_DATA )
		{
			char nameBuf[MAX_PATH];
			Warning( "[Sound MP3] %s(%d): Error while getting MP3 frame for sound '%s'. Error: %d.\n", __FILE__, __LINE__, m_pData->Source().GetFileName(nameBuf, sizeof(nameBuf)), decodeStatus );
			m_Error = decodeStatus;
			return true;
		}
		else if ( !decodeStatus || decodeStatus == ERROR_MP3_IO_NO_MP3_DATA )
		{
			if ( m_LastSuccessfulStreamTime != 0 )
			{
				// async streaming latency could be to blame, check watchdog
				if ( Plat_MSTime() - m_LastSuccessfulStreamTime >= MIX_IO_DATA_TIMEOUT )
				{
					char nameBuf[MAX_PATH];
					Warning( "[Sound MP3] %s(%d): Async streaming is late for sound '%s'. Will time-out.\n", __FILE__, __LINE__, m_pData->Source().GetFileName(nameBuf, sizeof(nameBuf)) );
					m_Error = ERROR_MP3_IO_DATA_TIMEOUT;
				}
			}
		}
	}

	// get the available samples ready for immediate mixing
	if ( RetrieveDecodedFrames( ) < 0 )
	{
		return true;
	}

	// can't mix until we have a minimum threshold of data or the decoder is finished
	int minBytesNeeded = m_bFinished ? 0 : MIN_READYTOMIX * m_SampleRate * GetMixSampleSize();
	int numReadyBytes = GetPCMBytes( 0, NULL );
	if ( numReadyBytes >= minBytesNeeded )
	{
		// decoder has samples ready for draining
		m_bStartedMixing = true;
		int nTimeSinceStart = Plat_MSTime() - m_StartTime;

		bool bDisplayStartupLatencyWarning = snd_ps3_mp3_spew_startup.GetBool();

		char nameBuf[MAX_PATH];
		m_pData->Source().GetFileName( nameBuf, sizeof(nameBuf) );

		if ( nTimeSinceStart > snd_async_stream_spew_delayed_start_time.GetInt() )
		{
			const char * pFilter = snd_async_stream_spew_delayed_start_filter.GetString();
			if ( *pFilter != '\0' )
			{
				bDisplayStartupLatencyWarning |= ( V_stristr( nameBuf, pFilter ) != NULL );
			}
			else
			{
				bDisplayStartupLatencyWarning = true;
			}
		}
		if ( bDisplayStartupLatencyWarning )
		{
			Warning( "[Sound MP3] Startup Latency: %d ms, Bytes Ready: %d, '%s'\n", nTimeSinceStart , numReadyBytes, nameBuf );
		}

		if ( m_bUpdateDelayForChoreo && snd_delay_for_choreo_enabled.GetBool() )
		{
			// We are playing a VO, update the choreo system accordingly for any startup latency. That way future VO will be pushed back by the same amount, so cut off will not occur.

			float fNewValue = ( ( float )( nTimeSinceStart ) / 1000.0f );
			fNewValue -= snd_mixahead.GetFloat();							// Remove the mix-ahead latency so it does not accumulate over time.

			if ( fNewValue > 0.0f )
			{
				g_fDelayForChoreo += fNewValue;								// And we accumulate the error over time.
																			// If the 1st sound sound is late 1 second, we are going to play the 2nd sound with 1 second delay.
																			// However, if the 2nd sound is 2 seconds late, then the 3rd sound will have to be played with a delay of 3 seconds (or cut the 2nd delay too early).
																			// When choreo does not play sounds for a while (currently 0.5 seconds), this counter will be reset to zero.
				if ( bDisplayStartupLatencyWarning )
				{
					Msg( "[Sound MP3] Updated IO latency compensation for VCD to %f seconds, due to startup time for sound '%s' taking %d ms.\n", g_fDelayForChoreo, nameBuf, nTimeSinceStart );
				}
			}
			Assert( g_nDelayForChoreoNumberOfSoundsPlaying >= 1 );			// Let's make sure the counter is not zero, as it will prevent the reset of the latency.
		}
		return true;
	}

	if ( m_LastDecodingStartTime != 0 )
	{
		if ( Plat_MSTime() - m_LastDecodingStartTime >= MIX_DECODER_TIMEOUT )
		{
			m_Error = ERROR_MP3_DECODER_TIMEOUT;
		}
	}

	// on startup error, let mixer start and get unavailable samples, and abort
	// otherwise hold off mixing until samples arrive
	return ( m_Error != 0 );
}

//-----------------------------------------------------------------------------
// Returns true to mix, false to stop mixer completely. Called after
// mixer requests samples.
//-----------------------------------------------------------------------------
bool CAudioMixerWavePs3Mp3::ShouldContinueMixing()
{
	// Is there anything to add?
	return BaseClass::ShouldContinueMixing();
}

//-----------------------------------------------------------------------------
// Reads existing buffer or decompress a new block when necessary.
// If no samples can be fetched, returns 0, which hints the mixer to a pending shutdown state.
// This routines operates in large buffer quantums, and nothing smaller.
//-----------------------------------------------------------------------------
int CAudioMixerWavePs3Mp3::GetOutputData( void **pData, int numSamplesToCopy, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] )
{
	if ( m_Error )
	{
		// mixer will eventually shutdown
		char strBuffer[256];
		Warning( "[Sound MP3] %s(%d): Error in sound '%s'. Error: %d.\n", __FILE__, __LINE__, m_pData->Source().GetFileName( strBuffer, sizeof( strBuffer ) ), m_Error );
		return 0;
	}

	// This function can get called in some cases directly when the mixing did not start (when there is a delay).
	// There is no seek-table implemented on PS3.

	bool bDecodingSuccessful = m_Decoder.Process();
	if ( bDecodingSuccessful == false )
	{
		if ( m_Decoder.WasErrorReported() == false )
		{
			char strBuffer[256];
			Warning( "[Sound MP3] Decoding error in sound '%s'. Error: %d\n", m_pData->Source().GetFileName( strBuffer, sizeof( strBuffer ) ), m_Error );
			m_Decoder.ErrorReported();
		}
	}

	// needs to be clocked at regular intervals
	if ( RetrieveDecodedFrames( ) < 0 )
	{
		char strBuffer[256];
		Warning( "[Sound MP3] %s(%d): Decoding error in sound '%s'. Error: %d.\n", __FILE__, __LINE__, m_pData->Source().GetFileName( strBuffer, sizeof( strBuffer ) ), m_Error );
		return 0;
	}

	// loopback may require flushing some decoded samples
	const int nSampleSize = GetMixSampleSize();
	int nNumRequestedBytes = numSamplesToCopy * nSampleSize;
	int nNumDiscardBytes = UpdatePositionForLoopingInBytes( &nNumRequestedBytes );
	if ( nNumDiscardBytes > 0 )
	{
		// loopback requires discarding samples to converge to expected loop-point
		nNumDiscardBytes -= GetPCMBytes( nNumDiscardBytes, NULL );
		if ( nNumDiscardBytes != 0 )
		{
			if ( snd_ps3_mp3_recover_from_exhausted_stream.GetBool() == false )
			{
				// For the moment, we are not enabling looping on MP3, so we are not really going to recover from this. Keep the code in case though.

				// not enough decoded data ready to flush
				// must flush these samples to achieve looping
				m_Error = ERROR_MP3_NO_PCM_DATA;

				char nameBuf[MAX_PATH];
				Warning( "[Sound MP3] %s(%d): Can't discard MP3 samples for sound '%s' as they are not available.\n", __FILE__, __LINE__, m_pData->Source().GetFileName(nameBuf, sizeof(nameBuf)) );
				return 0;
			}
		}
	}

	// can only drain as much as can be copied to caller
	nNumRequestedBytes = MIN( nNumRequestedBytes, AUDIOSOURCE_COPYBUF_SIZE );

	int nNumCopiedBytes = GetPCMBytes( nNumRequestedBytes, copyBuf );
	int nNumCopiedSamples = nNumCopiedBytes / nSampleSize;
	if ( nNumCopiedBytes )
	{
		CAudioMixerWave::m_sample_max_loaded += nNumCopiedSamples;
		CAudioMixerWave::m_sample_loaded_index += nNumCopiedSamples;

		// advance position by valid samples
		m_SamplePositionInBytes += nNumCopiedBytes;

		*pData = (void*)copyBuf;

		if ( copyBuf != NULL )
		{
#ifdef DEBUG_PS3_MP3
			if ( snd_ps3_mp3_record.GetBool() )
			{
				WaveAppendTmpFile( strFileName, copyBuf, 16, nNumCopiedSamples * m_NumChannels );
				WaveFixupTmpFile( strFileName );
			}
#endif

			// We copy the last sample in case we have to recover later of an exhausted streamer
			V_memcpy( &m_LastSample, &copyBuf[ nNumCopiedBytes - nSampleSize ], nSampleSize );
		}
	}
	else
	{
		// no samples copied
		if ( !m_bFinished && nNumRequestedBytes )
		{
			// MP3 latency error occurs when decoder not finished (not at EOF) and caller wanted samples but can't get any
			if ( snd_ps3_mp3_recover_from_exhausted_stream.GetBool() )
			{
				// Try to recover by using the last sample (to reduce potential pop).
				nNumCopiedSamples = nNumRequestedBytes / nSampleSize;
				// Code not optimized, but hopefully is not needed in normal cases.
				if ( copyBuf != NULL )
				{
					for ( int i = 0 ; i < nNumCopiedSamples ; ++i )
					{
						V_memcpy( &copyBuf[ i * nSampleSize ], &m_LastSample, nSampleSize );
					}
				}
				*pData = (void*)copyBuf;

				// Update some information used to calculate how much sound has been updated.
				// When there are some constant big latencies (testing extreme cases, like 500 ms), failure to update these will
				// create snowballing effect and force update of more and more samples (even if they are actually discarded at the end).
				// A visible consequence will be major slow down of the game, getting worse over time until the sound is finished / timed out.
				CAudioMixerWave::m_sample_max_loaded += nNumCopiedSamples;
				CAudioMixerWave::m_sample_loaded_index += nNumCopiedSamples;

				if ( snd_async_stream_spew_exhausted_buffer.GetBool() && ( g_pQueuedLoader->IsMapLoading() == false ) )
				{
					static uint sOldTime = 0;
					uint nCurrentTime = Plat_MSTime();
					if ( nCurrentTime >= sOldTime + snd_async_stream_spew_exhausted_buffer_time.GetInt() )
					{
						char nameBuf[MAX_PATH];
						Warning( "[Sound MP3] The stream buffer is exhausted for sound '%s'. Except after loading, fill a bug to have the number of sounds played reduced.\n", m_pData->Source().GetFileName(nameBuf, sizeof(nameBuf)) );
						sOldTime = nCurrentTime;
					}
				}
			}
			else
			{
				// We do not want to recover from the late streamer, the sound is going to stop (very noticeable for music and background ambiance sound).
				m_Error = ERROR_MP3_NO_PCM_DATA;

				if ( snd_ps3_mp3_spew_warnings.GetInt() )
				{
					char nameBuf[MAX_PATH];
					Warning( "[Sound MP3] 0x%8.8x, No Decoded Data Ready: %d samples needed, '%s'\n", (unsigned int)this, numSamplesToCopy, m_pData->Source().GetFileName(nameBuf, sizeof(nameBuf)) );
				}
			}
		}
	}

	return nNumCopiedSamples;
}

bool CAudioMixerWavePs3Mp3::IsSetSampleStartSupported() const
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Seek to a new position in the file
//			NOTE: In most cases, only call this once, and call it before playing
//			any data.
// Input  : newPosition - new position in the sample clocks of this sample
//-----------------------------------------------------------------------------
void CAudioMixerWavePs3Mp3::SetSampleStart( int newPosition )
{
	// Implementation not supported
	Assert( 0 );
}

int CAudioMixerWavePs3Mp3::GetPositionForSave()
{
	if ( m_bLooped )
	{
		// A looped sample cannot be saved/restored because the decoded sample position,
		// which is needed for loop calc, cannot ever be correctly restored without
		// the MP3 seek table. 
		return 0;
	}

	// This is silly and totally wrong, but doing it anyways.
	// The correct thing was to have the MP3 seek table and use
	// that to determine the correct packet. This is just a hopeful
	// nearby approximation. Music did not have the seek table at
	// the time of this code. The Seek table was added for vo
	// restoration later.
	return m_LastDataOffsetInBytes;
}

void CAudioMixerWavePs3Mp3::SetPositionFromSaved( int savedPosition )
{
	// Not used here. The Mixer creation will be given the initial startup offset.
}

//-----------------------------------------------------------------------------
// Purpose: Abstract factory function for PS3 MP3 mixers
//-----------------------------------------------------------------------------
CAudioMixer *CreatePs3Mp3Mixer( IWaveData *data, int initialStreamPosition, int skipInitialSamples, bool bUpdateDelayForChoreo )
{
	return new CAudioMixerWavePs3Mp3( data, initialStreamPosition, skipInitialSamples, bUpdateDelayForChoreo );
}
