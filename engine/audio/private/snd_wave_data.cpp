//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "audio_pch.h"
#include "utllinkedlist.h"
#include "utldict.h"
#include "filesystem/IQueuedLoader.h"
#include "cdll_int.h"
#include "mempool.h"
#include "memstack.h"
#include "ienginetoolinternal.h"
#include "vstdlib/jobthread.h"
#include "tier3/tier3.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IVEngineClient *engineClient;
extern IFileSystem *g_pFileSystem;
extern double realtime;

// console streaming buffer implementation, appropriate for high latency and low memory
// shift this many buffers through the wave

// We need to be careful with these settings as depending of the values, this may reduce the number we can play simultaneously.
//	If per sound, 2 buffers of 32 Kb are allocated, then we can have 64 different sounds simultaneously.
//  If per sound, 4 buffers of 64 Kb are allocated, then we can have 16 different sounds simultaneously.

// For non-looped sounds, only 2 buffers are needed for streaming.
// For looped sounds, we may need up to 4 buffers. (assuming that the last sound frame is at the beginning of the buffer, and the first loop sound frame is at the end of a buffer).

#if IsX360()
# define STREAM_BUFFER_COUNT				2
#if !defined( CSTRIKE15 )
# define STREAM_BUFFER_DATASIZE				( 2 * XBOX_DVD_ECC_SIZE )		// On X360 DVD, to reduce the number of seeks we actually read 64 Kb at a time instead of 32 Kb
																			// This increases the memory in term of temp buffer, it reduces the number of different sounds stored in the cache
#else
# define STREAM_BUFFER_DATASIZE				( 1 * XBOX_DVD_ECC_SIZE )		// CSTRIKE15 is not shipping on a DVD
#endif

// XBox has enough memory, and needs to lighten the i/o for the DVD (not using the HDD on this product)
// NOTE: in the case of asian languages, we need to spend an extra 7MB loading a large font file - this is taken from the audio cache.
# define CONSOLE_STREAMING_AUDIO_PORTAL2	( 20 * 1024 * 1024 )
# define ASIAN_FONT_MEMORY_USAGE			(  7 * 1024 * 1024 )
#elif IsPS3()
# define STREAM_BUFFER_COUNT				4				// Don't use memory for 4 buffers but 3, as in most case it is enough. In rare cases, we may have a latency issue.
															// However due to the latency introduced by the saving mechanism (also using the HHD), let's use a bit more for looped sounds.
															// Note that looped sounds on PS3 are not MP3 encoded but WAV encoded, so they are much bigger, we'll use 4 instead of 3 due to the save latency.
# define STREAM_BUFFER_DATASIZE				( 32 * 1024 )	// Blu-Ray has a block size of 64 Kb. We could reduce this to 32 Kb to allow more different sounds, but this would make the loading less optimal.
															// We actually reduced it to 32 Kb (like for X360) as we are now using FIOS for caching on the HDD. 32 Kb is a good size for the HDD.
// We should be good in term of memory on PS3 now.
# define CONSOLE_STREAMING_AUDIO_PORTAL2	( 4 * 1024 * 1024 )
# define ASIAN_FONT_MEMORY_USAGE			( 0 )
#else
# define STREAM_BUFFER_COUNT				4
# define STREAM_BUFFER_DATASIZE				( 64 * 1024 )
# define CONSOLE_STREAMING_AUDIO_PORTAL2	( 4 * 1024 * 1024 )
# define ASIAN_FONT_MEMORY_USAGE			( 0 )
#endif

// PC single buffering implementation
// UNDONE: Allocate this in cache instead?
#define SINGLE_BUFFER_SIZE		( 16 * 1024 )

// console streaming pool
#define CONSOLE_STREAMING_AUDIO_DEFAULT			( 4 * 1024 * 1024 )
#define CONSOLE_STREAMING_AUDIO_TF				( 12 * 1024 * 1024 )
#define CONSOLE_STREAMING_AUDIO_LEFT4DEAD		( 10 * 1024 * 1024 )
#define CONSOLE_STREAMING_AUDIO_LEFT4DEAD_DVD	( 12 * 1024 * 1024 )
#define CONSOLE_STREAMING_AUDIO_CSTRIKE15		( 9 * 1024 * 1024 )

// console static pool
#define CONSOLE_STATIC_AUDIO_DEFAULT			( 8.7 * 1024 * 1024 )
#define CONSOLE_STATIC_AUDIO_PORTAL2			( 2 * 1024 * 1024 )
#define CONSOLE_STATIC_AUDIO_CSTRIKE15			( 0.3 * 1024 * 1024 )

#define DEFAULT_WAV_MEMORY_CACHE				( 20 * 1024 * 1024 )

ConVar snd_async_spew_blocking( "snd_async_spew_blocking", "0", 0, "Spew message to console any time async sound loading blocks on file i/o." );
ConVar snd_async_fullyasync( "snd_async_fullyasync", "1", 0, "All playback is fully async (sound doesn't play until data arrives)." );
ConVar snd_async_stream_spew( "snd_async_stream_spew", "0", 0, "Spew streaming info ( 0=Off, 1=streams, 2=buffers" );
ConVar snd_async_stream_fail( "snd_async_stream_fail", "0", 0, "Spew stream pool failures." );
ConVar snd_async_stream_purges( "snd_async_stream_purges", "0", 0, "Spew stream pool purges." );
ConVar snd_async_stream_static_alloc( "snd_async_stream_static_alloc", "0", 0, "If 1, spews allocations on the static alloc pool. Set to 0 for no spew." );
ConVar snd_async_stream_recover_from_exhausted_stream( "snd_async_stream_recover_from_exhausted_stream", "1", 0, "If 1, recovers when the stream is exhausted when playing PCM sounds (prevents music or ambiance sounds to stop if too many sounds are played). Set to 0, to stop the sound otherwise." );
ConVar snd_async_stream_spew_exhausted_buffer( "snd_async_stream_spew_exhausted_buffer", IsCert() ? "0" : "1", 0, "If 1, spews warnings when the buffer is exhausted (recommended). Set to 0 for no spew (for debugging purpose only)." );
ConVar snd_async_stream_spew_exhausted_buffer_time( "snd_async_stream_spew_exhausted_buffer_time", "1000", 0, "Number of milliseconds between each exhausted buffer spew." );
ConVar snd_async_stream_spew_delayed_start_time( "snd_async_stream_spew_delayed_start_time", "500", 0, "Spew any asynchronous sound that starts with more than N milliseconds delay. By default spew when there is more than 500 ms delay." );
ConVar snd_async_stream_spew_delayed_start_filter( "snd_async_stream_spew_delayed_start_filter", "vo", 0, "Filter used to spew sounds that starts late. Use an empty string \"\" to display all sounds. By default only the VO are displayed.");
extern ConVar snd_report_loop_sound;

#define SndAlignReads() 1

void MaybeReportMissingWav( char const *wav );

// xbox pools its transient fixed sized streaming buffers
static CUtlMemoryPool *g_pAudioStreamPool;
static CMemoryStack *g_pAudioStaticPool;



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct asyncwaveparams_t
{
	asyncwaveparams_t() : 
		bPrefetch( false ), 
		bCanBeQueued( false ),
		bIsTransient( false ),
		bIsStaticPooled( false )
	{}

	FileNameHandle_t	hFilename;	// handle to sound item name (i.e. not with sound\ prefix)
	int					datasize;
	int					seekpos;
	int					alignment;
	unsigned int		bPrefetch : 1;
	unsigned int		bCanBeQueued : 1;
	unsigned int		bIsTransient : 1;
	unsigned int		bIsStaticPooled : 1;
};

//-----------------------------------------------------------------------------
// Purpose: Builds a cache of the data bytes for a specific .wav file
//-----------------------------------------------------------------------------
class CAsyncWaveData
{
public:
	explicit CAsyncWaveData();

	void DestroyResource();
	CAsyncWaveData *GetData();
	unsigned int Size();

	static void AsyncCallback( const FileAsyncRequest_t &asyncRequest, int numReadBytes, FSAsyncStatus_t err );
	static void QueuedLoaderCallback( void *pContext, void *pContext2, const void *pData, int nSize, LoaderError_t loaderError );
	static CAsyncWaveData *CreateResource( const asyncwaveparams_t &params );
	static unsigned int EstimatedSize( const asyncwaveparams_t &params );

	void OnAsyncCompleted( const FileAsyncRequest_t* asyncFilePtr, int numReadBytes, FSAsyncStatus_t err );
	bool BlockingCopyData( void *destbuffer, int destbufsize, int startoffset, int count );
	bool BlockingGetDataPointer( void **ppData );
	void SetAsyncPriority( int priority );
	void StartAsyncLoading( const asyncwaveparams_t& params );
	bool GetPostProcessed();
	void SetPostProcessed( bool proc );

	bool IsCurrentlyLoading();
	char const *GetFileName();

	// Data
public:
	int					m_nDataSize;		// bytes requested
	int					m_nReadSize;		// bytes actually read
	void				*m_pvData;			// target buffer
	void				*m_pAlloc;			// memory of buffer (base may not match)
	FileAsyncRequest_t	m_async;
	FSAsyncControl_t	m_hAsyncControl;
	float				m_start;			// time at request invocation
	float				m_arrival;			// time at data arrival
	FileNameHandle_t	m_hFileNameHandle;
	int					m_nBufferBytes;		// size of any pre-allocated target buffer
	BufferHandle_t		m_hBuffer;			// used to dequeue the buffer after lru
	unsigned int		m_bLoaded : 1;
	unsigned int		m_bMissing : 1;
	unsigned int		m_bPostProcessed : 1;
	unsigned int		m_bIsTransient : 1;
	unsigned int		m_bIsStaticPooled : 1;
};

enum WaveCacheAddFlags_t
{
	WCAF_LOCK		= ( 1 << 0 ),
	WCAF_DEFAULT	= 0,
};

struct WaveCacheStatus_t
{
	int nBytes;
};

struct WaveCacheLimits_t
{
	int nMaxBytes;
};

struct WaveCache_t
{
	CAsyncWaveData *m_pWaveData;
	unsigned int	m_nDataSize;
	unsigned int	m_nAgeStamp;
	unsigned int	m_nLockCount;
	int				m_hUnlock;
};

// Replacement for wavedata hosted by datacache which had some costlier aspects. When small purges needed
// to occur for the console streaming solution, the datacache's global lru list would need to be iterated.
// This replacement class attempts to provide the minimal necessary thread safe behavior to provide just
// the service that the wavedatacache uses. The lru list (only unlocked resources) is maintained by an age
// count, not link order.
class CWaveCache
{
public:
	CWaveCache()
	{
		m_nCurrentMemorySize = 0;
		m_nMaxMemorySize = UINT_MAX;
		m_nAgeStamp = 1;

		// reserve the 0 entry, want to use 0 as invalid to remain compliant to datacache code
		// which used 0 as an invalid handle
		WaveCacheHandle_t hData = m_HandleTable.AddHandle();
		Assert( hData == 0 );
		hData;
	}

	void Init( unsigned int nMemorySize )
	{
#if defined(LINUX) && !defined(DEDICATED)
		m_nMaxMemorySize = -1;
#else
		// can be -1 (i.e. unlimited, no implicit purge occurs via create)
		m_nMaxMemorySize = nMemorySize;
#endif
	}

	void Shutdown()
	{
	}

	CAsyncWaveData *CacheGet( WaveCacheHandle_t hData )
	{
		if ( hData == INVALID_WAVECACHE_HANDLE )
		{
			// must trap, the 0 entry is a valid index
			return NULL;
		}

		AUTO_LOCK_FM( m_WaveCacheMutex );

		WaveCache_t *pCacheData = m_HandleTable.GetHandle( hData );
		if ( !pCacheData )
			return NULL;

		pCacheData->m_nAgeStamp = m_nAgeStamp++;

		return pCacheData->m_pWaveData;
	}

	CAsyncWaveData *CacheGetNoTouch( WaveCacheHandle_t hData )
	{
		if ( hData == INVALID_WAVECACHE_HANDLE )
		{
			// must trap, the 0 entry is a valid index
			return NULL;
		}

		AUTO_LOCK_FM( m_WaveCacheMutex );

		WaveCache_t *pCacheData = m_HandleTable.GetHandle( hData );
		if ( !pCacheData )
			return NULL;

		return pCacheData->m_pWaveData;
	}

	CAsyncWaveData *CacheLock( WaveCacheHandle_t hData )
	{
		if ( hData == INVALID_WAVECACHE_HANDLE )
		{
			// must trap, the 0 entry is a valid index
			return NULL;
		}

		AUTO_LOCK_FM( m_WaveCacheMutex );

		WaveCache_t *pCacheData = m_HandleTable.GetHandle( hData );
		if ( !pCacheData )
			return NULL;

		pCacheData->m_nAgeStamp = m_nAgeStamp++;

		pCacheData->m_nLockCount++;
		if ( pCacheData->m_nLockCount == 1 && pCacheData->m_hUnlock != m_UnlockedList.InvalidIndex() )
		{
			// remove from unlocked list
			m_UnlockedList.Remove( pCacheData->m_hUnlock );
			pCacheData->m_hUnlock = m_UnlockedList.InvalidIndex();
		}

		return pCacheData->m_pWaveData;
	}

	int CacheUnlock( WaveCacheHandle_t hData )
	{
		if ( hData == INVALID_WAVECACHE_HANDLE )
		{
			// must trap, the 0 entry is a valid index
			return 0;
		}

		AUTO_LOCK_FM( m_WaveCacheMutex );

		WaveCache_t *pCacheData = m_HandleTable.GetHandle( hData );
		if ( !pCacheData )
			return 0;

		if ( pCacheData->m_nLockCount > 0 )
		{
			pCacheData->m_nLockCount--;
			if ( pCacheData->m_nLockCount == 0 )
			{
				// add to unlocked list
				pCacheData->m_hUnlock = m_UnlockedList.AddToTail( hData );
			}
		}

		return pCacheData->m_nLockCount;
	}

	WaveCacheHandle_t CacheCreate( asyncwaveparams_t params, WaveCacheAddFlags_t flags = WCAF_DEFAULT )
	{
		AUTO_LOCK_FM( m_WaveCacheMutex );

		if ( m_nMaxMemorySize != UINT_MAX )
		{
			unsigned int nBytesToPurge = CAsyncWaveData::EstimatedSize( params );

			if ( m_nCurrentMemorySize + nBytesToPurge > m_nMaxMemorySize )
			{
				Purge( nBytesToPurge );

				if ( m_nCurrentMemorySize + nBytesToPurge > m_nMaxMemorySize )
				{
					// not enough memory
					return INVALID_WAVECACHE_HANDLE;
				}
			}
		}

		CAsyncWaveData *pWaveData = CAsyncWaveData::CreateResource( params );
		if ( !pWaveData )
		{
			return INVALID_WAVECACHE_HANDLE;
		}

		WaveCache_t *pCacheData = new WaveCache_t;

		pCacheData->m_pWaveData = pWaveData;
		pCacheData->m_nDataSize = pWaveData->Size();
		pCacheData->m_nAgeStamp = m_nAgeStamp++;

		m_nCurrentMemorySize += pCacheData->m_nDataSize;

		WaveCacheHandle_t hData = m_HandleTable.AddHandle();

		if ( flags & WCAF_LOCK )
		{
			pCacheData->m_nLockCount = 1;
			pCacheData->m_hUnlock = m_UnlockedList.InvalidIndex();
		}
		else
		{
			pCacheData->m_nLockCount = 0;
			pCacheData->m_hUnlock = m_UnlockedList.AddToTail( hData );
		}

		m_HandleTable.SetHandle( hData, pCacheData );

		return hData;
	}

	bool CacheRemove( WaveCacheHandle_t hData )
	{
		if ( hData == INVALID_WAVECACHE_HANDLE )
		{
			// must trap, the 0 entry is a valid index
			return false;
		}

		AUTO_LOCK_FM( m_WaveCacheMutex );

		WaveCache_t *pCacheData = m_HandleTable.GetHandle( hData );
		if ( !pCacheData || pCacheData->m_nLockCount != 0 )
			return false;

		if ( snd_async_stream_purges.GetBool() )
		{
			Msg( "CacheRemove: Age:%d %s\n", pCacheData->m_nAgeStamp, pCacheData->m_pWaveData->GetFileName() );
		}

		pCacheData->m_pWaveData->DestroyResource();

		m_nCurrentMemorySize -= pCacheData->m_nDataSize;
		
		if ( pCacheData->m_hUnlock != m_UnlockedList.InvalidIndex() )
		{
			m_UnlockedList.Remove( pCacheData->m_hUnlock );
		}

		m_HandleTable.RemoveHandle(	hData );

		delete pCacheData;

		return true;
	}

	void BreakLock( WaveCacheHandle_t hData ) 
	{
		if ( hData == INVALID_WAVECACHE_HANDLE )
		{
			// must trap, the 0 entry is a valid index
			return;
		}

		AUTO_LOCK_FM( m_WaveCacheMutex );

		WaveCache_t *pCacheData = m_HandleTable.GetHandle( hData );
		if ( !pCacheData )
			return;

		if ( pCacheData->m_nLockCount != 0 )
		{
			pCacheData->m_nLockCount = 0;

			// add to unlocked list
			pCacheData->m_hUnlock = m_UnlockedList.AddToTail( hData );
		}
	}

	void Age( WaveCacheHandle_t hData )
	{
		if ( hData == INVALID_WAVECACHE_HANDLE )
		{
			// must trap, the 0 entry is a valid index
			return;
		}

		AUTO_LOCK_FM( m_WaveCacheMutex );

		WaveCache_t *pCacheData = m_HandleTable.GetHandle( hData );
		if ( !pCacheData )
			return;

		// the oldest sounds are ones that have not been touched
		pCacheData->m_nAgeStamp = 0;
	}

	int GetLockCount( WaveCacheHandle_t hData )
	{
		if ( hData == INVALID_WAVECACHE_HANDLE )
		{
			// must trap, the 0 entry is a valid index
			return 0;
		}

		AUTO_LOCK_FM( m_WaveCacheMutex );

		WaveCache_t *pCacheData = m_HandleTable.GetHandle( hData );
		if ( !pCacheData )
			return 0;

		return pCacheData->m_nLockCount;
	}

	unsigned int GetAgeStamp( WaveCacheHandle_t hData )
	{
		if ( hData == INVALID_WAVECACHE_HANDLE )
		{
			// must trap, the 0 entry is a valid index
			return 0;
		}

		AUTO_LOCK_FM( m_WaveCacheMutex );

		WaveCache_t *pCacheData = m_HandleTable.GetHandle( hData );
		if ( !pCacheData )
			return 0;

		return pCacheData->m_nAgeStamp;
	}

	void Purge( unsigned int nBytesToPurge )
	{
		AUTO_LOCK_FM( m_WaveCacheMutex );

		// keep purging the oldest unlocked until desired memory is available
		// trying to keep this as cheap as possible, so no sorting, just an iteration to find oldest
		unsigned int nBytesPurged = 0;
		while ( nBytesPurged < nBytesToPurge )
		{
			unsigned int nCandidateSize = 0;
			unsigned int nCandidateAge = UINT_MAX;
			WaveCacheHandle_t hCandidate = INVALID_WAVECACHE_HANDLE;
			for ( int hUnlock = m_UnlockedList.Head(); hUnlock != m_UnlockedList.InvalidIndex(); hUnlock = m_UnlockedList.Next( hUnlock ) )
			{
				WaveCacheHandle_t hData = m_UnlockedList[hUnlock];
				WaveCache_t *pCacheData = m_HandleTable.GetHandle( hData );
				if ( pCacheData && pCacheData->m_nAgeStamp < nCandidateAge )
				{
					nCandidateAge = pCacheData->m_nAgeStamp;
					nCandidateSize = pCacheData->m_nDataSize;
					hCandidate = hData;
				}
			}

			if ( hCandidate != INVALID_WAVECACHE_HANDLE && CacheRemove( hCandidate ) )
			{
				nBytesPurged += nCandidateSize;
			}
			else
			{
				// there are no unlocked candidates
				// or the qualified remove failed
				break;
			}
		}
	}

	void Flush()
	{
		AUTO_LOCK_FM( m_WaveCacheMutex );

		CUtlVector< WaveCacheHandle_t > purgeList( 0, m_UnlockedList.Count() );
		for ( int hUnlock = m_UnlockedList.Head(); hUnlock != m_UnlockedList.InvalidIndex(); hUnlock = m_UnlockedList.Next( hUnlock ) )
		{
			purgeList.AddToTail( m_UnlockedList[hUnlock] );
		}

		// remove all unlocked resources
		for ( int i = 0; i < purgeList.Count(); i++ )
		{
			CacheRemove( purgeList[i] );
		}
	}

	void CacheLockMutex()
	{
		m_WaveCacheMutex.Lock();
	}

	void CacheUnlockMutex()
	{
		m_WaveCacheMutex.Unlock();
	}

	void GetStatus( WaveCacheStatus_t *pStatus, WaveCacheLimits_t *pLimits )
	{
		pStatus->nBytes = m_nCurrentMemorySize;
		pLimits->nMaxBytes = m_nMaxMemorySize;
	}

private:
	unsigned int m_nCurrentMemorySize;
	unsigned int m_nMaxMemorySize;

	unsigned int m_nAgeStamp;

	CThreadFastMutex m_WaveCacheMutex;

	// 2048 simultaneous sound handles in the cache
	CUtlHandleTable< WaveCache_t, 11 > m_HandleTable;

	CUtlLinkedList< WaveCacheHandle_t > m_UnlockedList;
};
CWaveCache s_WaveCache;

//-----------------------------------------------------------------------------
// Purpose: C'tor
//-----------------------------------------------------------------------------
CAsyncWaveData::CAsyncWaveData() :
	m_nDataSize( 0 ),
	m_nReadSize( 0 ),
	m_pvData( 0 ),
	m_pAlloc( 0 ),
	m_hBuffer( INVALID_BUFFER_HANDLE ),
	m_nBufferBytes( 0 ),
	m_hAsyncControl( NULL ),
	m_bLoaded( false ),
	m_bMissing( false ),
	m_start( 0.0 ),
	m_arrival( 0.0 ),
	m_bPostProcessed( false ),
	m_bIsTransient( false ),
	m_bIsStaticPooled( false ),
	m_hFileNameHandle( 0 )
{
}

//-----------------------------------------------------------------------------
// Purpose: // APIS required by CDataLRU
//-----------------------------------------------------------------------------
void CAsyncWaveData::DestroyResource()
{
	if ( IsPC() )
	{
		if ( m_hAsyncControl )
		{
			if ( !m_bLoaded && !m_bMissing )
			{
				// NOTE:  We CANNOT call AsyncAbort since if the file is actually being read we'll end 
				//  up still getting a callback, but our this ptr (deleted below) will be feeefeee and we'll trash the heap 
				//  pretty bad.  So we call AsyncFinish, which will do a blocking read and will definitely succeed	
				// Block until we are finished
				g_pFileSystem->AsyncFinish( m_hAsyncControl, true );
			}
			
			g_pFileSystem->AsyncRelease( m_hAsyncControl );
			m_hAsyncControl = NULL;
		}
	}

	if ( IsGameConsole() )
	{
		if ( m_hAsyncControl )
		{
			if ( !m_bLoaded && !m_bMissing )
			{
				// force an abort
				int errStatus = g_pFileSystem->AsyncAbort( m_hAsyncControl );
				if ( errStatus != FSASYNC_ERR_UNKNOWNID )
				{
					// must wait for abort to finish before deallocating data
					g_pFileSystem->AsyncFinish( m_hAsyncControl, true );
				}
			}
			g_pFileSystem->AsyncRelease( m_hAsyncControl );
			m_hAsyncControl = NULL;
		}
		if ( m_hBuffer != INVALID_BUFFER_HANDLE )
		{
			// hint the manager that this tracked buffer is invalid
			wavedatacache->MarkBufferDiscarded( m_hBuffer );
		}
	}

	// delete buffers
	if ( IsPC() )
	{
		g_pFileSystem->FreeOptimalReadBuffer( m_pAlloc );
	}

	if ( IsGameConsole() )
	{
		if ( m_bIsTransient )
		{
			g_pAudioStreamPool->Free( m_pAlloc );
		}
		else if ( m_bIsStaticPooled )
		{
			// freed as part of pool purge
		}
		else
		{
			free( m_pAlloc );
		}
	}

	delete this;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *CAsyncWaveData::GetFileName()
{
	static char sz[MAX_PATH];

	if ( m_hFileNameHandle )	
	{
		if ( g_pFileSystem->String( m_hFileNameHandle, sz, sizeof( sz ) ) )
		{
			return sz;
		}
	}
	
	Assert( 0 );
	return "";
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CAsyncWaveData
//-----------------------------------------------------------------------------
CAsyncWaveData *CAsyncWaveData::GetData()
{ 
	return this; 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : unsigned int
//-----------------------------------------------------------------------------
unsigned int CAsyncWaveData::Size()
{ 
	int size;

	if ( IsPC() )
	{
		size = sizeof( *this ) + m_nDataSize;
	}

	if ( IsGameConsole() )
	{
		// the data size is volatile and shrinks during streaming near end of file
		// report the real constant size of this object's allocation
		size = m_nBufferBytes;
	}

	return size;
}

//-----------------------------------------------------------------------------
// Purpose: Static method for CDataLRU
// Input  : &params - 
// Output : CAsyncWaveData
//-----------------------------------------------------------------------------
CAsyncWaveData *CAsyncWaveData::CreateResource( const asyncwaveparams_t &params )
{
	MEM_ALLOC_CREDIT_( "CAsyncWaveData::CreateResource" );

	CAsyncWaveData *pData = NULL;

	if ( IsGameConsole() )
	{
		// create buffers now for re-use during streaming process
		void *pBuffer = NULL;
		int bufferSize;
		bool bIsStaticPooled = params.bIsStaticPooled;
		if ( params.bIsTransient )
		{
			// streaming transient sounds pool their fixed size dynamic buffers to lighten fragmentation
			bufferSize = STREAM_BUFFER_DATASIZE;
			pBuffer = g_pAudioStreamPool->Alloc();
			if ( !pBuffer )
			{
				// pool is empty, purge required
				// failure case detected by create logic, will drive purge and retry
				return NULL;
			}
		}
		else
		{
			// non-streaming sounds have buffers that are only transient during map transitions
			bufferSize = AlignValue( params.datasize, params.alignment );
			if ( bIsStaticPooled )
			{
				// pool these sounds
				pBuffer = g_pAudioStaticPool->Alloc( bufferSize );
				if ( snd_async_stream_static_alloc.GetBool() )
				{
					Msg( "CAsyncWavDataCache: Static Pool: %.2f MB used of %.2f MB\n", g_pAudioStaticPool->GetUsed() / ( 1024.0f * 1024.0f ), g_pAudioStaticPool->GetSize() / ( 1024.0f * 1024.0f ) );
				}
				if ( !pBuffer )
				{
					Warning( "CAsyncWaveData:: Static Pool OVERFLOW, failing to standard heap!\n" );
					// flip and fail to other heap
					bIsStaticPooled = false;
				}
			}

			if ( !bIsStaticPooled )
			{
				// use the standard heap for non-streaming non-pooled buffers
				pBuffer = new byte[bufferSize];
			}
		}

		pData = new CAsyncWaveData;
		pData->m_nBufferBytes = bufferSize;
		pData->m_pAlloc = pData->m_pvData = pBuffer;
		pData->m_bIsTransient = params.bIsTransient;
		pData->m_bIsStaticPooled = bIsStaticPooled;
	}
	else
	{
		pData = new CAsyncWaveData;
		Assert( pData );
	}

	if ( pData )
	{
		pData->StartAsyncLoading( params );
	}

	return pData;
}

//-----------------------------------------------------------------------------
// Purpose: Static method
// Input  : &params - 
// Output : static unsigned int
//-----------------------------------------------------------------------------
unsigned int CAsyncWaveData::EstimatedSize( const asyncwaveparams_t &params )
{
	int size;

	if ( IsPC() )
	{
		size = 	sizeof( CAsyncWaveData ) + params.datasize;
	}

	if ( IsGameConsole() )
	{
		// the expected size of this object's allocations
		if ( params.bIsTransient )
		{
			size = STREAM_BUFFER_DATASIZE;
		}
		else
		{
			size = AlignValue( params.datasize, params.alignment );
		}
	}

	return size;
}

//-----------------------------------------------------------------------------
// Purpose: Static method, called by thread, don't call anything non-threadsafe from handler!!!
// Input  : asyncFilePtr - 
//			numReadBytes - 
//			err - 
//-----------------------------------------------------------------------------
void CAsyncWaveData::AsyncCallback(const FileAsyncRequest_t &asyncRequest, int numReadBytes, FSAsyncStatus_t err )
{
	CAsyncWaveData *pObject = reinterpret_cast< CAsyncWaveData * >( asyncRequest.pContext );
	Assert( pObject );
	if ( pObject )
	{
		pObject->OnAsyncCompleted( &asyncRequest, numReadBytes, err );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Static method, called by thread, don't call anything non-threadsafe from handler!!!
//-----------------------------------------------------------------------------
void CAsyncWaveData::QueuedLoaderCallback( void *pContext, void *pContext2, const void *pData, int nSize, LoaderError_t loaderError )
{
	CAsyncWaveData *pObject = reinterpret_cast< CAsyncWaveData * >( pContext );
	Assert( pObject );

	pObject->OnAsyncCompleted( NULL, nSize, loaderError == LOADERERROR_NONE ? FSASYNC_OK : FSASYNC_ERR_FILEOPEN );
}

//-----------------------------------------------------------------------------
// Purpose: NOTE: THIS IS CALLED FROM A THREAD SO YOU CAN'T CALL INTO ANYTHING NON-THREADSAFE
//  such as CUtlSymbolTable/CUtlDict (many of the CUtl* are non-thread safe)!!!
// Input  : asyncFilePtr - 
//			numReadBytes - 
//			err - 
//-----------------------------------------------------------------------------
void CAsyncWaveData::OnAsyncCompleted( const FileAsyncRequest_t *asyncFilePtr, int numReadBytes, FSAsyncStatus_t err )
{
	if ( IsPC() )
	{
		// Take hold of pointer (we can just use delete[] across .dlls because we are using a shared memory allocator...)
		if ( err == FSASYNC_OK || err == FSASYNC_ERR_READING )
		{
			m_arrival = ( float )Plat_FloatTime();

			// Take over ptr
			m_pAlloc = asyncFilePtr->pData;
			if ( SndAlignReads() )
			{
				m_async.nOffset = ( m_async.nBytes - m_nDataSize );
				m_async.nBytes -= m_async.nOffset;
				m_pvData = ((byte *)m_pAlloc) + m_async.nOffset;
				m_nReadSize	= numReadBytes - m_async.nOffset;
			}
			else
			{
				m_pvData = m_pAlloc;
				m_nReadSize = numReadBytes;
			}

			// Needs to be post-processed
			m_bPostProcessed = false;

			// Finished loading
			m_bLoaded = true;
		}
		else if ( err == FSASYNC_ERR_FILEOPEN )
		{
			// SEE NOTE IN FUNCTION COMMENT ABOVE!!!
			// Tracker 22905, et al.
			// Because this api gets called from the other thread, don't spew warning here as it can
			//  cause a crash in searching CUtlSymbolTables since they use a global var for a LessFunc context!!!
			m_bMissing = true;
		}
	}

	if ( IsGameConsole() )
	{
		m_arrival = (float)Plat_FloatTime();

		// possibly reading more than intended due to alignment restriction
		m_nReadSize = numReadBytes;
		if ( m_nReadSize > m_nDataSize )
		{
			// clamp to expected, extra data is unreliable
			m_nReadSize = m_nDataSize;
		}

		if ( err != FSASYNC_OK )
		{
			// track as any error
			m_bMissing = true;
		}

		if ( err != FSASYNC_ERR_FILEOPEN )
		{
			// some data got loaded
			m_bLoaded = true;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *destbuffer - 
//			destbufsize - 
//			startoffset - 
//			count - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAsyncWaveData::BlockingCopyData( void *destbuffer, int destbufsize, int startoffset, int count )
{
	if ( !m_bLoaded )
	{
		Assert( m_hAsyncControl );
		// Force it to finish
		// It could finish between the above line and here, but the AsyncFinish call will just have a bogus id, not a big deal
		if ( snd_async_spew_blocking.GetBool() )
		{
			// Force it to finish
			float st = ( float )Plat_FloatTime();
			g_pFileSystem->AsyncFinish( m_hAsyncControl, true );
			float ed = ( float )Plat_FloatTime();
			DevMsg( "%f BCD:  Async I/O Force %s (%8.2f msec / %8.2f msec total)\n", realtime, GetFileName(), 1000.0f * (float)( ed - st ), 1000.0f * (float)( m_arrival - m_start ) );
		}
		else
		{
			g_pFileSystem->AsyncFinish( m_hAsyncControl, true );
		}
	}

	// notify on any error
	if ( m_bMissing )
	{
		// Only warn once
		m_bMissing = false;

		char fn[MAX_PATH];
		if ( g_pFileSystem->String( m_hFileNameHandle, fn, sizeof( fn ) ) )
		{
			MaybeReportMissingWav( fn );
		}
	}

	if ( !m_bLoaded )
	{
		return false;
	}
	else if ( m_arrival != 0 && snd_async_spew_blocking.GetInt() >= 2 )
	{
		DevMsg( "%f Async I/O Read successful %s (%8.2f msec)\n", realtime, GetFileName(), 1000.0f * (float)( m_arrival - m_start ) );
		m_arrival = 0;
	}

	// clamp requested to available
	if ( count > m_nReadSize )
	{
		count = m_nReadSize - startoffset;
	}

	if ( count < 0 )
	{
		return false;
	}

	// Copy data from stream buffer
	Q_memcpy( destbuffer, (char *)m_pvData + ( startoffset - m_async.nOffset ), count );

	g_pFileSystem->AsyncRelease( m_hAsyncControl );
	m_hAsyncControl = NULL;
	return true;
}


bool CAsyncWaveData::IsCurrentlyLoading()
{
	if ( m_bLoaded )
		return true;
	if ( m_bMissing )
		return false;
	FSAsyncStatus_t status = g_pFileSystem->AsyncStatus( m_hAsyncControl );
	if ( status == FSASYNC_STATUS_INPROGRESS || status == FSASYNC_OK )
		return true;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : **ppData - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAsyncWaveData::BlockingGetDataPointer( void **ppData )
{
	Assert( ppData );
	if ( !m_bLoaded )
	{
		// Force it to finish
		// It could finish between the above line and here, but the AsyncFinish call will just have a bogus id, not a big deal
		if ( snd_async_spew_blocking.GetBool() )
		{
			float st = ( float )Plat_FloatTime();
			g_pFileSystem->AsyncFinish( m_hAsyncControl, true );
			float ed = ( float )Plat_FloatTime();
			DevMsg( "%f BlockingGetDataPointer:  Async I/O Force %s (%8.2f msec / %8.2f msec total )\n", realtime, GetFileName(), 1000.0f * (float)( ed - st ), 1000.0f * (float)( m_arrival - m_start ) );
		}
		else
		{
			g_pFileSystem->AsyncFinish( m_hAsyncControl, true );
		}
	}

	// notify on any error
	if ( m_bMissing )
	{
		// Only warn once
		m_bMissing = false;

		char fn[MAX_PATH];
		if ( g_pFileSystem->String( m_hFileNameHandle, fn, sizeof( fn ) ) )
		{
			MaybeReportMissingWav( fn );
		}
	}

	if ( !m_bLoaded )
	{
		return false;
	}
	else if ( m_arrival != 0 && snd_async_spew_blocking.GetInt() >= 2 )
	{
		DevMsg( "%f Async I/O Read successful %s (%8.2f msec)\n", realtime, GetFileName(), 1000.0f * (float)( m_arrival - m_start ) );
		m_arrival = 0;
	}

	*ppData = m_pvData;

	g_pFileSystem->AsyncRelease( m_hAsyncControl );
	m_hAsyncControl = NULL;

	return true;
}

void CAsyncWaveData::SetAsyncPriority( int priority )
{
	if ( m_async.priority != priority )
	{
		m_async.priority = priority;
		g_pFileSystem->AsyncSetPriority( m_hAsyncControl, m_async.priority );
		if ( snd_async_spew_blocking.GetInt() >= 2 ) 
		{
			DevMsg( "%f Async I/O Bumped priority for %s (%8.2f msec)\n", realtime, GetFileName(), 1000.0f * (float)( Plat_FloatTime() - m_start ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : params - 
//-----------------------------------------------------------------------------
void CAsyncWaveData::StartAsyncLoading( const asyncwaveparams_t& params )
{
	Assert( IsGameConsole() || ( IsPC() && !m_bLoaded ) );

	// expected to be relative to the sound\ dir
	m_hFileNameHandle = params.hFilename;

	// build the real filename
	char szFilename[MAX_PATH];
	Q_snprintf( szFilename, sizeof( szFilename ), "sound\\%s", GetFileName() );

	int nPriority = 1;
	if ( params.bPrefetch )
	{
		// lower the priority of prefetched sounds, so they don't block immediate sounds from being loaded
		nPriority = 0;
	}

	if ( !IsGameConsole() )
	{
		m_async.pData = NULL;
		if ( SndAlignReads() )
		{
			m_async.nOffset = 0;
			m_async.nBytes = params.seekpos + params.datasize;
		}
		else
		{
			m_async.nOffset = params.seekpos;
			m_async.nBytes = params.datasize;
		}
	}
	else
	{
		Assert( params.datasize > 0 );

		// using explicit allocated buffer on xbox
		m_async.pData = m_pvData;
		m_async.nOffset = params.seekpos;
		m_async.nBytes = AlignValue( params.datasize, params.alignment ); 
	}

	m_async.pfnCallback	= AsyncCallback;	// optional completion callback
	m_async.pContext = (void *)this;		// caller's unique context
	m_async.priority = nPriority;			// inter list priority, 0=lowest
	m_async.flags = IsGameConsole() ? 0 : FSASYNC_FLAGS_ALLOCNOFREE;
	m_async.pszPathID = "GAME";

	m_bLoaded = false;
	m_bMissing = false;
	m_nDataSize = params.datasize;
	m_start = (float)Plat_FloatTime();
	m_arrival = 0;
	m_nReadSize = 0;
	m_bPostProcessed = false;

	// The async layer creates a copy of this string, ok to send a local reference
	m_async.pszFilename	= szFilename;

	char szFullName[MAX_PATH];
	if ( IsGameConsole() )
	{
		// all audio is expected be in zips
		// resolve to absolute name now, where path can be filtered to just the zips (fast find, no real i/o)
		// otherwise the dvd will do a costly seek for each zip miss due to search path fall through
		PathTypeQuery_t pathType;
		if ( !g_pFileSystem->RelativePathToFullPath( m_async.pszFilename, m_async.pszPathID, szFullName, sizeof( szFullName ), GetAudioPathFilter(), &pathType ) )
		{
			// not found, do callback now to handle error
			m_async.pfnCallback( m_async, 0, FSASYNC_ERR_FILEOPEN );
			return;
		}
		m_async.pszFilename	= szFullName;
	}

	if ( IsGameConsole() && params.bCanBeQueued )
	{
		// queued loader takes over
		LoaderJob_t loaderJob;
		loaderJob.m_pFilename = m_async.pszFilename;
		loaderJob.m_pPathID = m_async.pszPathID;
		loaderJob.m_pCallback = QueuedLoaderCallback;
		loaderJob.m_pContext = (void *)this;
		loaderJob.m_Priority = LOADERPRIORITY_DURINGPRELOAD;
		loaderJob.m_pTargetData = m_async.pData;
		loaderJob.m_nBytesToRead = m_async.nBytes;
		loaderJob.m_nStartOffset = m_async.nOffset;
		g_pQueuedLoader->AddJob( &loaderJob );
		return;
	}

	MEM_ALLOC_CREDIT();
	
	// Commence async I/O
	Assert( !m_hAsyncControl );
	g_pFileSystem->AsyncRead( m_async, &m_hAsyncControl );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAsyncWaveData::GetPostProcessed()
{
	return m_bPostProcessed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : proc - 
//-----------------------------------------------------------------------------
void CAsyncWaveData::SetPostProcessed( bool proc )
{
	m_bPostProcessed = proc;
}

//-----------------------------------------------------------------------------
// Purpose: Implements a cache of .wav / .mp3 data based on filename
//-----------------------------------------------------------------------------
class CAsyncWavDataCache : public IAsyncWavDataCache
{
public:
	CAsyncWavDataCache();
	~CAsyncWavDataCache() {}

	virtual bool			Init( unsigned int memSize );
	virtual void			Shutdown();

	// implementation that treats file as monolithic
	virtual WaveCacheHandle_t	AsyncLoadCache( char const *filename, int datasize, int startpos, bool bIsPrefetch = false );
	virtual void			PrefetchCache( char const *filename, int datasize, int startpos );
	virtual bool			CopyDataIntoMemory( char const *filename, int datasize, int startpos, void *buffer, int bufsize, int copystartpos, int bytestocopy, bool *pbPostProcessed );
	virtual bool			CopyDataIntoMemory( WaveCacheHandle_t& handle, char const *filename, int datasize, int startpos, void *buffer, int bufsize, int copystartpos, int bytestocopy, bool *pbPostProcessed );
	virtual void			SetPostProcessed( WaveCacheHandle_t handle, bool proc );
	virtual void			Unload( WaveCacheHandle_t handle );
	virtual bool			GetDataPointer( WaveCacheHandle_t& handle, char const *filename, int datasize, int startpos, void **pData, int copystartpos, bool *pbPostProcessed );
	virtual bool			IsDataLoadCompleted( WaveCacheHandle_t handle, bool *pIsValid, bool *pIsMissing );
	virtual void			RestartDataLoad( WaveCacheHandle_t* handle, char const *filename, int datasize, int startpos );
	virtual bool			IsDataLoadInProgress( WaveCacheHandle_t handle );

	// Xbox: alternate multi-buffer streaming implementation
	virtual StreamHandle_t	OpenStreamedLoad( char const *pFileName, int dataSize, int dataStart, int startPos, int loopPos, int bufferSize, int numBuffers, streamFlags_t flags, SoundError &soundError );
	virtual void			CloseStreamedLoad( StreamHandle_t hStream );
	virtual int				CopyStreamedDataIntoMemory( StreamHandle_t hStream, void *pBuffer, int bufferSize, int copyStartPos, int bytesToCopy );
	virtual bool			IsStreamedDataReady( StreamHandle_t hStream );
	virtual void			MarkBufferDiscarded( BufferHandle_t hBuffer );
	virtual void			*GetStreamedDataPointer( StreamHandle_t hStream, bool bSync );

	virtual	void			Flush( bool bTearDownStaticPool = false );

	virtual void			UpdateLoopPosition( StreamHandle_t hStream, int nLoopPosition );

	enum MemoryUsageType
	{
		SPEW_BASIC = 0,
		SPEW_MUSIC_NONSTREAMING,
		SPEW_NONSTREAMING,
		SPEW_ALL,
	};
	void					SpewMemoryUsage( MemoryUsageType level );

	// Cache helpers
	bool					GetItemName( DataCacheClientID_t clientId, const void *pItem, char *pDest, unsigned nMaxLen  );

private:
	void					Clear();

	struct CacheEntry_t
	{
		CacheEntry_t() :
			name( 0 ),
			handle( 0 )
		{
		}
		FileNameHandle_t	name;
		WaveCacheHandle_t	handle;
	};

	// tags the signature of a buffer inside a rb tree for faster than linear find
	struct BufferEntry_t
	{
		FileNameHandle_t	m_hName;
		WaveCacheHandle_t	m_hWaveData;
		int					m_StartPos;
		unsigned int		m_bIsTransient : 1;
		unsigned int		m_bCanBeShared : 1;
	};

	static bool BufferHandleLessFunc( const BufferEntry_t& lhs, const BufferEntry_t& rhs )
	{
		if ( lhs.m_bIsTransient != rhs.m_bIsTransient )
		{
			return lhs.m_bIsTransient < rhs.m_bIsTransient;
		}

		if ( lhs.m_hName != rhs.m_hName )
		{
			return lhs.m_hName < rhs.m_hName;
		}

		if ( lhs.m_StartPos != rhs.m_StartPos )
		{
			return lhs.m_StartPos < rhs.m_StartPos;
		}

		return lhs.m_bCanBeShared < rhs.m_bCanBeShared;
	}

	CUtlRBTree< BufferEntry_t, BufferHandle_t >	m_BufferList;

	// encapsulates (n) buffers for a streamed wave object
	struct StreamedEntry_t
	{
		FileNameHandle_t	m_hName;
		WaveCacheHandle_t	m_hWaveData[STREAM_BUFFER_COUNT];
		int					m_Front;			// buffer index, forever incrementing
		int					m_NextStartPos;		// predicted offset if mixing linearly
		int					m_DataSize;			// length of the data set in bytes
		int					m_DataStart;		// file offset where data set starts
		int					m_LoopStart;		// offset in data set where loop starts
		int					m_BufferSize;		// size of the buffer in bytes
		int					m_numBuffers;		// number of buffers (1 or 2) to march through
		int					m_SectorSize;		// size of sector on stream device
		bool				m_bSinglePlay;		// hint to keep same buffers
		bool				m_bIsTransient;		// hint for buffer lifetime
	};
	CUtlLinkedList< StreamedEntry_t, StreamHandle_t >	m_StreamedHandles;

	static bool CacheHandleLessFunc( const CacheEntry_t& lhs, const CacheEntry_t& rhs )
	{
		return lhs.name < rhs.name;
	}
	CUtlRBTree< CacheEntry_t, int >	m_CacheHandles;

	// these are buffers that were still in-flight at time of stream closure
	// we track these until we can remove an outstanding lock
	// some of these buffers are allowed to be reclaimed
	struct DeadBufferEntry_t
	{
		WaveCacheHandle_t	hWaveData;
		bool				bSinglePlay;
	};
	CUtlVector< DeadBufferEntry_t >	m_DeadBuffers;

	WaveCacheHandle_t		FindOrCreateBuffer( asyncwaveparams_t &params, bool bFind );		
	void					CleanupDeadBuffers( bool bSync );
	bool					m_bInitialized;

	struct  StreamData_t
	{
		int				actualCopied;
		CAsyncWaveData	*pWaveData[STREAM_BUFFER_COUNT];
		int				index;
		bool			bWaiting;
		int				copyStartPos;
	};

	bool InitializeStreamData( const StreamedEntry_t &streamedEntry, StreamData_t & streamData, int copyStartPos );
	void CopyFromCurrentBuffers( StreamedEntry_t &streamedEntry, StreamData_t & streamData, StreamHandle_t hStream, void *pBuffer, int bufferSize, int bytesToCopy );
	void PrefetchNextBuffers( StreamedEntry_t &streamedEntry, StreamData_t & streamData );
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAsyncWavDataCache::CAsyncWavDataCache() :  
	m_CacheHandles( 0, 0, CacheHandleLessFunc ),
	m_BufferList( 0, 0, BufferHandleLessFunc ),
	m_bInitialized( false )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAsyncWavDataCache::Init( unsigned int memSize )
{
	if ( m_bInitialized )
		return true;
	
	if ( IsGameConsole() )
	{	
		// xbox non-streaming audio is uncapped, all these sounds allocate as required
		// streaming audio is constrained, explicitly managed, and pooled
		unsigned int nStaticPoolSize = CONSOLE_STATIC_AUDIO_DEFAULT;

		const char *pGame = engineClient->GetGameDirectory();
		if ( !Q_stricmp( Q_UnqualifiedFileName( pGame ), "tf" ) )
		{
			memSize = CONSOLE_STREAMING_AUDIO_TF;
		}
		else if ( StringHasPrefix( Q_UnqualifiedFileName( pGame ), "left4dead2") )
		{
			if ( g_pFullFileSystem->IsDVDHosted() )
			{
				memSize = CONSOLE_STREAMING_AUDIO_LEFT4DEAD_DVD;
			}
			else
			{
				memSize = CONSOLE_STREAMING_AUDIO_LEFT4DEAD;
			}
		}
		else if ( StringHasPrefix( Q_UnqualifiedFileName( pGame ), "portal2") )
		{
			nStaticPoolSize = CONSOLE_STATIC_AUDIO_PORTAL2;
			memSize = CONSOLE_STREAMING_AUDIO_PORTAL2;
			if ( IsX360() && XBX_IsRestrictiveLanguage() )
			{
				COMPILE_TIME_ASSERT( !IsX360() || ( CONSOLE_STREAMING_AUDIO_PORTAL2 == 20*1024*1024 ) ); // Might want to revisit this tradeoff if this changes
				memSize -= ASIAN_FONT_MEMORY_USAGE;
			}
		}
		else if ( StringHasPrefix( Q_UnqualifiedFileName( pGame ), "csgo" ) )
		{
			nStaticPoolSize = CONSOLE_STATIC_AUDIO_CSTRIKE15;
			memSize = CONSOLE_STREAMING_AUDIO_CSTRIKE15;
		}	
		else
		{
			memSize = CONSOLE_STREAMING_AUDIO_DEFAULT;
		}
		
		// needs to be integral
		Assert( memSize % STREAM_BUFFER_DATASIZE == 0 );
		g_pAudioStreamPool = new CUtlMemoryPool( STREAM_BUFFER_DATASIZE, memSize/STREAM_BUFFER_DATASIZE, CUtlMemoryPool::GROW_NONE, "CAsyncWavDataCache::AudioStreamPool" );
		// force the actual pool allocation to occur on first alloc
		g_pAudioStreamPool->Clear();
	
		// create a pool to hold the non-streaming static data
		g_pAudioStaticPool = new CMemoryStack;
		g_pAudioStaticPool->Init( "g_pAudioStaticPool", nStaticPoolSize, 0, nStaticPoolSize );

		// NOTE!!!!
		// console audio section is unlimited, purges are *explicitly* invoked via FindOrCreateBuffer()
		// the create will cause the resource to be created which checks the pools, fails, and then the explicit purge occurs
		memSize = (unsigned int)-1;
	}
	else
	{
		if ( memSize < DEFAULT_WAV_MEMORY_CACHE )
		{
			memSize = DEFAULT_WAV_MEMORY_CACHE;
		}
	}

	s_WaveCache.Init( memSize );

	m_bInitialized = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::Shutdown()
{
	if ( !m_bInitialized )
	{
		return;
	}

	Clear();

	s_WaveCache.Shutdown();

	delete g_pAudioStreamPool;

	m_bInitialized = false;
}

//-----------------------------------------------------------------------------
// Purpose: Creates initial cache object if it doesn't already exist, starts async loading the actual data
//  in any case.
// Input  : *filename - 
//			datasize - 
//			startpos - 
// Output : WaveCacheHandle_t
//-----------------------------------------------------------------------------
WaveCacheHandle_t CAsyncWavDataCache::AsyncLoadCache( char const *filename, int datasize, int startpos, bool bIsPrefetch )
{
	VPROF( "CAsyncWavDataCache::AsyncLoadCache" );

	FileNameHandle_t fnh = g_pFileSystem->FindOrAddFileName( filename );

	CacheEntry_t search;
	search.name = fnh;
	search.handle = 0;

	// find or create the handle
	int idx = m_CacheHandles.Find( search );
	if ( idx == m_CacheHandles.InvalidIndex() )
	{
		idx = m_CacheHandles.Insert( search );
		Assert( idx != m_CacheHandles.InvalidIndex() );
	}

	CacheEntry_t &entry = m_CacheHandles[idx];

	// Try and pull it into cache
	CAsyncWaveData *data = s_WaveCache.CacheGet( entry.handle );
	if ( !data )
	{
		// Try and reload it
		asyncwaveparams_t	params;
		params.hFilename = fnh;
		params.datasize = datasize;
		params.seekpos = startpos;
		params.bPrefetch = bIsPrefetch;
		entry.handle = s_WaveCache.CacheCreate( params );
	}

	return entry.handle;
}


//-----------------------------------------------------------------------------
// Purpose: Reclaim a buffer. A reclaimed resident buffer is ready for play.
//-----------------------------------------------------------------------------
WaveCacheHandle_t CAsyncWavDataCache::FindOrCreateBuffer( asyncwaveparams_t &params, bool bFind )
{
	CAsyncWaveData *pWaveData;
	BufferEntry_t	search;
	BufferHandle_t	hBuffer;

	search.m_hName = params.hFilename;
	search.m_StartPos = params.seekpos;
	search.m_bIsTransient = params.bIsTransient;
	search.m_bCanBeShared = bFind;
	search.m_hWaveData = INVALID_WAVECACHE_HANDLE;

	if ( bFind )
	{
		// look for an existing buffer that matches signature (same file, offset, etc)
		int hBuffer = m_BufferList.Find( search );
		if ( hBuffer != m_BufferList.InvalidIndex() )
		{
			// found
			search.m_hWaveData = m_BufferList[hBuffer].m_hWaveData;
			if ( snd_async_stream_spew.GetInt() >= 2 )
			{
				char tempBuff[MAX_PATH];
				g_pFileSystem->String( params.hFilename, tempBuff, sizeof( tempBuff ) );
				Msg( "Found Buffer: %s, offset: %d\n", tempBuff, params.seekpos );
			}
		}
	}
	
	// each resource buffer stays locked (valid) while in use
	// a buffering stream is not subject to lru and can rely on it's buffers
	// a buffering stream may obsolete it's buffers by reducing the lock count, allowing for lru
	pWaveData = s_WaveCache.CacheLock( search.m_hWaveData );
	if ( !pWaveData )
	{
		// not in cache, create and lock
		// not found, create buffer and fill with data
		int numFails = 0;
		while ( 1 ) 
		{
			search.m_hWaveData = s_WaveCache.CacheCreate( params, WCAF_LOCK );
			if ( search.m_hWaveData != INVALID_WAVECACHE_HANDLE )
			{
				break;
			}

			if ( !params.bIsTransient )
			{
				// yikes!, creation can fail if out of system memory or the stream memory pool was full
				// only transient streams would fail if the stream pool was full
				Assert( 0 );
				return INVALID_WAVECACHE_HANDLE;
			}

			numFails++;
			if ( numFails >= 2 )
			{
				// second attempt
				// yikes!, stream pool isn't getting purged, all must be locked
				Assert( 0 );
				if ( snd_async_stream_fail.GetBool() )
				{
					Warning( "Stream pool: No buffers available! (dead:%d)\n", m_DeadBuffers.Count() );
				}
				return INVALID_WAVECACHE_HANDLE;
			}
			
			if ( numFails == 1 )
			{
				// first time failure
				// try and reclaim dead buffers before the purge
				CleanupDeadBuffers( false );
			}

			// lru purge the stream pool and retry
			s_WaveCache.Purge( STREAM_BUFFER_DATASIZE );
		}

		// add the buffer to our managed list
		hBuffer = m_BufferList.Insert( search );
		Assert( hBuffer != m_BufferList.InvalidIndex() );

		// store the handle into our managed list
		// used during a lru discard as a means to keep the list in-sync
		pWaveData = s_WaveCache.CacheGet( search.m_hWaveData );
		pWaveData->m_hBuffer = hBuffer;
	}
	else
	{
		// still in cache
		// same as requesting it and having it arrive instantly
		pWaveData->m_start = pWaveData->m_arrival = (float)Plat_FloatTime();
	}

	return search.m_hWaveData;
}

//-----------------------------------------------------------------------------
// Purpose: Load data asynchronously via multi-buffers, returns specialized handle
//-----------------------------------------------------------------------------
StreamHandle_t CAsyncWavDataCache::OpenStreamedLoad( char const *pFileName, int dataSize, int dataStart, int startPos, int loopPos, int bufferSize, int numBuffers, streamFlags_t flags, SoundError &soundError )
{
	VPROF( "CAsyncWavDataCache::OpenStreamedLoad" );

	StreamedEntry_t			streamedEntry;
	StreamHandle_t			hStream;
	asyncwaveparams_t		params;

	Assert( numBuffers > 0 && numBuffers <= STREAM_BUFFER_COUNT );

	if ( flags & STREAMED_QUEUEDLOAD )
	{
		if ( numBuffers != 1 )
		{
			// queued load mandates one buffer, caller has violated code expectations
			Assert( 0 );
			numBuffers = 1;
		}

		if ( flags & STREAMED_TRANSIENT )
		{
			// not allowed, queued loads are for static resident sounds
			Assert( 0 );
			flags &= ~STREAMED_TRANSIENT;
		}
	}

	if ( ( flags & STREAMED_TRANSIENT ) && ( bufferSize != STREAM_BUFFER_DATASIZE ) )
	{
		// stream pool mandates the pool block size, caller has violated code expectations
		Assert( 0 );
		bufferSize = STREAM_BUFFER_DATASIZE;
	}

	if ( !( flags & STREAMED_TRANSIENT ) && ( flags & STREAMED_SINGLEPLAY ) )
	{
		// singleplay is a streaming concept requiring transient buffers
		// nonsense concept if used by memory resident wavs
		// caller has violated code expectations
		Assert( 0 );
		flags &= ~STREAMED_SINGLEPLAY;
	}

	streamedEntry.m_hName = g_pFileSystem->FindOrAddFileName( pFileName );
	streamedEntry.m_Front = 0;
	streamedEntry.m_DataSize = dataSize;
	streamedEntry.m_DataStart = dataStart;
	streamedEntry.m_NextStartPos = startPos + numBuffers * bufferSize;
	streamedEntry.m_LoopStart = loopPos;
	streamedEntry.m_BufferSize = bufferSize;
	streamedEntry.m_numBuffers = numBuffers;
	streamedEntry.m_bSinglePlay = ( flags & STREAMED_SINGLEPLAY ) != 0;
	streamedEntry.m_SectorSize = ( IsGameConsole() && ( flags & STREAMED_FROMDVD ) ) ? XBOX_DVD_SECTORSIZE : 1;
	streamedEntry.m_bIsTransient = IsGameConsole() && ( flags & STREAMED_TRANSIENT ) != 0;

	bool bFindBuffer;
	if ( !( flags & STREAMED_TRANSIENT ) && !( flags & STREAMED_QUEUEDLOAD ) )
	{
		// static sounds created outside the queued loader end up in the standard heap
		// these would be UI sounds or other engine startup sounds
		// for simplicity and stability, they don't alias
		bFindBuffer = false;
	}
	else
	{
		// single play streams expect to uniquely own and thus recycle their buffers though the data
		// single play streams are guaranteed that their buffers are private and cannot be shared
		// a non-single play stream wants persisting buffers and attempts to reclaim a matching buffer
		bFindBuffer = ( streamedEntry.m_bSinglePlay == false );
	}

	// initial load populates buffers
	// mixing starts after front buffer viable
	// buffer rotation occurs after front buffer consumed
	// there should be no blocking
	params.hFilename = streamedEntry.m_hName;
	params.datasize = bufferSize;
	params.alignment = streamedEntry.m_SectorSize;
	params.bCanBeQueued = ( flags & STREAMED_QUEUEDLOAD ) != 0;
	params.bIsTransient = streamedEntry.m_bIsTransient;
	params.bIsStaticPooled = ( flags & STREAMED_QUEUEDLOAD ) != 0;

	bool bFailed = false;
	for ( int i = 0; i < numBuffers; ++i )
	{
		int nOffset = streamedEntry.m_SectorSize * ( ( startPos + i * bufferSize ) / streamedEntry.m_SectorSize );		// So it matches the alignment of the streamer after
		params.seekpos = dataStart + nOffset;
		WaveCacheHandle_t hWaveData = INVALID_WAVECACHE_HANDLE;
		if ( !bFailed )
		{
			hWaveData = FindOrCreateBuffer( params, bFindBuffer );
			bFailed = ( hWaveData == INVALID_WAVECACHE_HANDLE );
		}
		streamedEntry.m_hWaveData[i] = hWaveData;
	}

	// get a unique handle for each stream request
	hStream = m_StreamedHandles.AddToTail( streamedEntry );
	Assert( hStream != m_StreamedHandles.InvalidIndex() );

	if ( bFailed )
	{
		// partial failure, didn't get all the buffers required
		// have to do cleanup, cannot leave dangling buffers
		CloseStreamedLoad( hStream );

		// sorry, this sound will not play, system is unable to provide streaming buffers
		// streaming buffers are should have been available
		hStream = INVALID_STREAM_HANDLE;
		soundError = SE_NO_STREAM_BUFFER;
	}
	else
	{
		soundError = SE_OK;
	}

	return hStream;
}


//-----------------------------------------------------------------------------
// Purpose: Cleanup a streamed load's resources.
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::CloseStreamedLoad( StreamHandle_t hStream )
{
	VPROF( "CAsyncWavDataCache::CloseStreamedLoad" );

	if ( hStream == INVALID_STREAM_HANDLE )
	{
		return;
	}

	int	lockCount;
	StreamedEntry_t	&streamedEntry = m_StreamedHandles[hStream];
	for ( int i=0; i<streamedEntry.m_numBuffers; ++i )
	{
		WaveCacheHandle_t hWaveData = streamedEntry.m_hWaveData[i];
		if ( hWaveData == INVALID_WAVECACHE_HANDLE )
		{
			// cleaning up unexpected bad entry
			// skip over logic that would otherwise assert
			continue;
		}

		// multiple streams could be using the same buffer, keeping the lock count nonzero
		lockCount = s_WaveCache.GetLockCount( hWaveData );
		Assert( lockCount >= 1 );
		if ( lockCount > 1 )
		{
			// just remove our lock, it will eventuall free when the last consumer releases
			s_WaveCache.CacheUnlock( hWaveData );
			continue;
		}
	
		CAsyncWaveData *pBuffer = s_WaveCache.CacheGetNoTouch( hWaveData );
		if ( pBuffer )
		{
			// going to a zero lock count with an async operation in flight would cause memory corruption	
			if ( !pBuffer->m_bMissing && !pBuffer->m_bLoaded )
			{
				// still in flight, add to list of dead buffers
				// have to now track these to remove our lock when they async-finish
				int iEntryIndex = m_DeadBuffers.AddToTail();
				m_DeadBuffers[iEntryIndex].hWaveData = hWaveData;
				m_DeadBuffers[iEntryIndex].bSinglePlay =  streamedEntry.m_bSinglePlay;
				continue;
			}
		}

		lockCount = s_WaveCache.CacheUnlock( hWaveData );
		if ( streamedEntry.m_bSinglePlay )
		{
			// a buffering single play stream has no reason to reuse its own buffers and destroys them
			// these buffers are uniquely owned, so the lock count can/should only be 0 at this point
			// if !=0 the remove will respect the lock and do nothing and the buffer becomes a zombie
			Assert( lockCount == 0 );
			s_WaveCache.CacheRemove( hWaveData );
		}
	}

	m_StreamedHandles.Remove( hStream );
}

//-----------------------------------------------------------------------------
// Cleanup any streaming buffers that could not be released during the close
// because they had async data in-flight. These buffers may have become owned
// by another stream. We do this polling to avoid having to sync stop the
// buffers which would cause the game to hitch.
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::CleanupDeadBuffers( bool bSync )
{
	if ( !IsGameConsole() )
	{
		return;
	}

	for ( int iEntryIndex = 0; iEntryIndex < m_DeadBuffers.Count(); )
	{
		WaveCacheHandle_t hWaveData = m_DeadBuffers[iEntryIndex].hWaveData;
		bool bSinglePlay = m_DeadBuffers[iEntryIndex].bSinglePlay;

		int lockCount = s_WaveCache.GetLockCount( hWaveData );
		Assert( lockCount >= 1 );
		if ( lockCount > 1 )
		{
			// this buffer got re-claimed by some stream
			// just remove the oustanding lock that should have occurred during the initial close but was prevented
			// this buffer will eventually free when the last consumer releases
			s_WaveCache.CacheUnlock( hWaveData );

			// no longer tracking
			m_DeadBuffers.Remove( iEntryIndex );
			continue;
		}

		// going to a zero lock count with an async operation in flight would cause memory corruption	
		// check the buffer to ensure it has completed
		CAsyncWaveData *pBuffer = s_WaveCache.CacheGetNoTouch( hWaveData );
		if ( pBuffer )
		{
			if ( !pBuffer->m_bMissing && !pBuffer->m_bLoaded )
			{
				if ( !bSync )
				{
					// still in flight, it will eventually finish 
					// keep tracking
					iEntryIndex++;
					continue;
				}
				else
				{
					// cause a sync operation to force the async operation to finish 
					void *pData = NULL;
					pBuffer->BlockingGetDataPointer( &pData );
				}
			}
		}

		// remove the outstanding lock
		lockCount = s_WaveCache.CacheUnlock( hWaveData );

		if ( bSinglePlay )
		{
			// a buffering single play stream has no reason to reuse its own buffers and destroys them
			// these buffers are uniquely owned, so the lock count can/should only be 0 at this point
			// if !=0 the remove will respect the lock and do nothing and the buffer becomes a zombie
			Assert( lockCount == 0 );
			s_WaveCache.CacheRemove( hWaveData );
		}

		//  no longer tracking
		m_DeadBuffers.Remove( iEntryIndex );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//			datasize - 
//			startpos - 
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::PrefetchCache( char const *filename, int datasize, int startpos )
{
	// Just do an async load, but don't get cache handle
	AsyncLoadCache( filename, datasize, startpos, true );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//			datasize - 
//			startpos - 
//			*buffer - 
//			bufsize - 
//			copystartpos - 
//			bytestocopy - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAsyncWavDataCache::CopyDataIntoMemory( char const *filename, int datasize, int startpos, void *buffer, int bufsize, int copystartpos, int bytestocopy, bool *pbPostProcessed )
{
	VPROF( "CAsyncWavDataCache::CopyDataIntoMemory" );

	bool bret = false;

	// Add to caching system
	AsyncLoadCache( filename, datasize, startpos );

	FileNameHandle_t fnh = g_pFileSystem->FindOrAddFileName( filename );

	CacheEntry_t search;
	search.name = fnh;
	search.handle = 0;

	// Now look it up, it should be in the system
	int idx = m_CacheHandles.Find( search );
	if ( idx == m_CacheHandles.InvalidIndex() )
	{
		Assert( 0 );
		return bret;
	}

	// Now see if the handle has been paged out...
	return CopyDataIntoMemory( m_CacheHandles[ idx ].handle, filename, datasize, startpos, buffer, bufsize, copystartpos, bytestocopy, pbPostProcessed );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
//			*filename - 
//			datasize - 
//			startpos - 
//			*buffer - 
//			bufsize - 
//			copystartpos - 
//			bytestocopy - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAsyncWavDataCache::CopyDataIntoMemory( WaveCacheHandle_t& handle, char const *filename, int datasize, int startpos, void *buffer, int bufsize, int copystartpos, int bytestocopy, bool *pbPostProcessed )
{
	VPROF( "CAsyncWavDataCache::CopyDataIntoMemory" );

	*pbPostProcessed = false;

	bool bret = false;

	CAsyncWaveData *data = s_WaveCache.CacheLock( handle );
	if ( !data )
	{
		FileNameHandle_t fnh = g_pFileSystem->FindOrAddFileName( filename );

		CacheEntry_t search;
		search.name = fnh;
		search.handle = 0;

		// Now look it up, it should be in the system
		int idx = m_CacheHandles.Find( search );
		if ( idx == m_CacheHandles.InvalidIndex() )
		{
			Assert( 0 );
			return false;
		}

		// Try and reload it
		asyncwaveparams_t params;
		params.hFilename = fnh;
		params.datasize = datasize;
		params.seekpos = startpos;

		handle = m_CacheHandles[ idx ].handle = s_WaveCache.CacheCreate( params );
		data = s_WaveCache.CacheLock( handle );
		if ( !data )
		{
			return bret;
		}
	}

	// Cache entry exists, but if filesize == 0 then the file itself wasn't on disk...
	if ( data->m_nDataSize != 0 )
	{
		bret = data->BlockingCopyData( buffer, bufsize, copystartpos, bytestocopy );
	}

	*pbPostProcessed = data->GetPostProcessed();

	// Release lock
	s_WaveCache.CacheUnlock( handle );
	return bret;
}

bool CAsyncWavDataCache::InitializeStreamData( const StreamedEntry_t &streamedEntry, StreamData_t & streamData, int copyStartPos )
{
	for ( int i=0; i<streamedEntry.m_numBuffers; ++i )
	{
		streamData.pWaveData[i] = s_WaveCache.CacheGetNoTouch( streamedEntry.m_hWaveData[i] );
		Assert( streamData.pWaveData[i] );
		if ( streamData.pWaveData[i] == NULL )
		{
			// oops, where are our locked buffers?
			// The buffers can go away in midst of streaming if the streaming buffer pool is filled,
			// no buffers can be lru'd, and then can't provide the next stream buffer to fill.
			// There is no choice but to abort this streaming sound.
			return false;			
		}
	}
	streamData.index = streamedEntry.m_Front;
	streamData.actualCopied = 0;
	streamData.bWaiting = false;
	streamData.copyStartPos = copyStartPos;
	return true;
}

void CAsyncWavDataCache::CopyFromCurrentBuffers( StreamedEntry_t &streamedEntry, StreamData_t & streamData, StreamHandle_t hStream, void *pBuffer, int bufferSize, int bytesToCopy )
{
	int nRemainingBytesToCopy = bytesToCopy;
	while ( 1 )
	{
		// try to satisfy from the front
		CAsyncWaveData *pFront = streamData.pWaveData[streamData.index % streamedEntry.m_numBuffers];
		int bufferPos = streamData.copyStartPos - pFront->m_async.nOffset;

		// cache atomic async completion signal off to avoid coherency issues
		bool bCompleted = pFront->m_bLoaded || pFront->m_bMissing;

		if ( snd_async_stream_spew.GetInt() >= 1 )
		{
			// interval is the audio block clock rate, the block must be available within this interval
			// a faster audio rate or smaller block size implies a smaller interval
			// latency is the actual block delivery time
			// latency must not exceed the delivery interval or starving occurs and audio pops
			float nowTime = Plat_FloatTime();
			int interval = (int)(1000.0f*(nowTime-pFront->m_start));
			int latency;
			if ( bCompleted && pFront->m_bLoaded )
			{
				latency = (int)(1000.0f*(pFront->m_arrival-pFront->m_start));
			}
			else
			{
				// buffer has not arrived yet
				latency = -1;
			}
			Msg( "Stream:%2d interval:%5dms latency:%5dms offset:%d length:%d (%s)\n", hStream, interval, latency, pFront->m_async.nOffset, pFront->m_nReadSize, pFront->GetFileName() );
		}

		if ( bCompleted && pFront->m_hAsyncControl && ( pFront->m_bLoaded || pFront->m_bMissing) )
		{
			g_pFileSystem->AsyncRelease( pFront->m_hAsyncControl );
			pFront->m_hAsyncControl = NULL;
		}

		if ( bCompleted && pFront->m_bLoaded )
		{
			if ( bufferPos >= 0 && bufferPos < pFront->m_nReadSize )
			{
				int count = nRemainingBytesToCopy;
				if ( bufferPos + count > pFront->m_nReadSize )
				{
					// clamp requested to actual available
					count = pFront->m_nReadSize - bufferPos;
				}
				if ( bufferPos + count > bufferSize )
				{
					// clamp requested to caller's buffer dimension
					count = bufferSize - bufferPos;
				}

				if ( count > 0 )
				{
					// We have to test for count as in some cases it could be negative (as BufferSize gets reduced if it spans over more than one loop).
					// This is actually very rare though.

					Q_memcpy( pBuffer, (char *)pFront->m_pvData + bufferPos, count );

					// advance past consumed bytes
					streamData.actualCopied += count;
					streamData.copyStartPos += count;
					nRemainingBytesToCopy -= count;
					bufferPos += count;							// Once we copied it, update bufferPos so we can look at the next buffer if needed.
					pBuffer = (void *)((intp)pBuffer + count);	// Skip written bytes
					bufferSize -= count;
				}
				else
				{
					// Nothing else can be done. We'll see with next request.
					Warning( "%s(%d): Protecting against negative memcpy. BufferSize = %d. Buffer Pos = %d. Count = %d.\n", __FILE__, __LINE__, bufferSize, bufferPos, count );
					return;
				}
			}
		}
		else if ( bCompleted && pFront->m_bMissing )
		{
			// notify on any error
			MaybeReportMissingWav( pFront->GetFileName() );
			break;
		}
		else
		{
			// data not available
			streamData.bWaiting = true;
			break;
		}

		// cycle past obsolete or consumed buffers
		if ( bufferPos < 0 || bufferPos >= pFront->m_nReadSize )
		{
			// move to next buffer
			streamData.index++;
			if ( streamData.index - streamedEntry.m_Front >= streamedEntry.m_numBuffers )
			{
				// out of buffers
				break;
			}
		}

		if ( streamData.actualCopied == bytesToCopy )
		{
			// satisfied request
			return;
		}
	}

	// If the request is not satisfied, let's make sure that at least the buffers are in the range asked.
	// If that's not the case, then we need to make sure that the next pos retrieved is in the range.
	bool bInRange = false;
	for ( int i = 0 ; i < streamedEntry.m_numBuffers ; ++i )
	{
		CAsyncWaveData *pWaveData = streamData.pWaveData[i];
		if ( pWaveData->m_bMissing )
		{
			continue;
		}
		int bufferPos = streamData.copyStartPos - pWaveData->m_async.nOffset;
		if ( ( bufferPos >= 0 ) && ( bufferPos < pWaveData->m_nDataSize ) )
		{
			bInRange = true;
		}
	}
	if ( bInRange == false )
	{
		// None on the buffers are in range, make sure next buffer is the one we are currently requesting.
		streamedEntry.m_NextStartPos = streamData.copyStartPos - streamedEntry.m_DataStart;
	}
}

void CAsyncWavDataCache::PrefetchNextBuffers( StreamedEntry_t &streamedEntry, StreamData_t & streamData )
{
	if ( streamedEntry.m_numBuffers > 1 )
	{
		int nextStartPos;

		// restart consumed buffers
		while ( streamedEntry.m_Front < streamData.index )
		{
			if ( !streamData.actualCopied && !streamData.bWaiting )
			{
				// couldn't return any data because the buffers aren't in the right location
				nextStartPos = streamData.copyStartPos - streamedEntry.m_DataStart;
			}
			else
			{
				// get the next forecast read location
				nextStartPos = streamedEntry.m_NextStartPos;
			}

			if ( nextStartPos >= streamedEntry.m_DataSize )
			{
				// next buffer is at or past end of file 
				if ( streamedEntry.m_LoopStart >= 0 )
				{
					// wrap back around to loop position
					nextStartPos = streamedEntry.m_LoopStart;
				}
				else
				{
					// advance past consumed buffer
					streamedEntry.m_Front++;
					// start no further buffers
					break;
				}
			}

			// still valid data left to read
			// snap the buffer position to required alignment
			nextStartPos = streamedEntry.m_SectorSize * (nextStartPos/streamedEntry.m_SectorSize);

			// start loading back buffer at future location
			asyncwaveparams_t	params;
			params.hFilename = streamedEntry.m_hName;
			params.seekpos = streamedEntry.m_DataStart + nextStartPos;
			params.datasize = streamedEntry.m_DataSize - nextStartPos;
			params.alignment = streamedEntry.m_SectorSize;
			params.bIsTransient = streamedEntry.m_bIsTransient;
			if ( params.datasize > streamedEntry.m_BufferSize )
			{
				// clamp to buffer size
				params.datasize = streamedEntry.m_BufferSize;
			}

			// save next start position
			streamedEntry.m_NextStartPos = nextStartPos + params.datasize;

			int which = streamedEntry.m_Front % streamedEntry.m_numBuffers;
			if ( streamedEntry.m_bSinglePlay )
			{
				// a single play wave has no reason to persist its buffers into the lru
				// reuse buffer and restart until finished
				streamData.pWaveData[which]->StartAsyncLoading( params );
			}
			else
			{
				// release obsolete buffer to lru management
				s_WaveCache.CacheUnlock( streamedEntry.m_hWaveData[which] );

				// reclaim or create/load the desired buffer
				WaveCacheHandle_t hWaveData = FindOrCreateBuffer( params, true );
				streamedEntry.m_hWaveData[which] = hWaveData;
				if ( hWaveData == INVALID_WAVECACHE_HANDLE )
				{
					// very bad, failed to get an expected buffer
					// return whatever we have
					// retry logic will eventually get a 0 and cease sound
					return;
				}
			}
			streamData.bWaiting = true;
			streamedEntry.m_Front++;

			// Then if there is more stream buffer available, let's continue at the next position
			nextStartPos = streamData.copyStartPos - streamedEntry.m_DataStart + streamedEntry.m_BufferSize;
			streamData.copyStartPos += streamedEntry.m_BufferSize;
		}

		if ( streamData.bWaiting )
		{
			// oh no! data needed is not yet available in front buffer
			// caller requesting data faster than can be provided or caller skipped
			// can only return what has been copied thus far (could be 0)
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Copy from streaming buffers into target memory, never blocks.
//-----------------------------------------------------------------------------
int CAsyncWavDataCache::CopyStreamedDataIntoMemory( StreamHandle_t hStream, void *pBuffer, int bufferSize, int copyStartPos, int bytesToCopy )
{
	VPROF( "CAsyncWavDataCache::CopyStreamedDataIntoMemory" );

	StreamedEntry_t		&streamedEntry = m_StreamedHandles[hStream];
	if ( copyStartPos >= streamedEntry.m_DataStart + streamedEntry.m_DataSize )
	{
		// at or past end of file
		return 0;
	}

	StreamData_t streamData;
	if ( InitializeStreamData( streamedEntry, streamData, copyStartPos ) == false )
	{
		return 0;
	}

	CopyFromCurrentBuffers( streamedEntry, streamData, hStream, pBuffer, bufferSize, bytesToCopy );
	PrefetchNextBuffers( streamedEntry, streamData );
	return streamData.actualCopied;
}

//-----------------------------------------------------------------------------
// Purpose: Get the front buffer, optionally block.
// Intended for user of a single buffer stream.
//-----------------------------------------------------------------------------
void *CAsyncWavDataCache::GetStreamedDataPointer( StreamHandle_t hStream, bool bSync )
{
	void			*pData = NULL;
	CAsyncWaveData	*pFront;
	int				index;
	StreamedEntry_t &streamedEntry = m_StreamedHandles[hStream];

	index  = streamedEntry.m_Front % streamedEntry.m_numBuffers;
	pFront = s_WaveCache.CacheGetNoTouch( streamedEntry.m_hWaveData[index] );
	Assert( pFront );
	if ( !pFront )
	{
		// shouldn't happen
		return NULL;
	}

	if ( !pFront->m_bMissing && pFront->m_bLoaded )
	{
		return pFront->m_pvData;
	}

	if ( bSync && pFront->BlockingGetDataPointer( &pData ) )
	{
		return pData;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: The front buffer must be valid
//-----------------------------------------------------------------------------
bool CAsyncWavDataCache::IsStreamedDataReady( int hStream )
{
	VPROF( "CAsyncWavDataCache::IsStreamedDataReady" );

	if ( hStream == INVALID_STREAM_HANDLE )
	{
		return false;
	}

	StreamedEntry_t &streamedEntry = m_StreamedHandles[hStream];

	if ( streamedEntry.m_Front )
	{
		// already streaming, the buffers better be arriving as expected
		return true;
	}

	// only the first front buffer must be present
	CAsyncWaveData *pFront = s_WaveCache.CacheGetNoTouch( streamedEntry.m_hWaveData[0] );
	Assert( pFront );
	if ( !pFront )
	{
		// shouldn't happen
		// let the caller think data is ready, so stream can shutdown
		return true;
	}

	// regardless of any errors
	// errors handled during data fetch
	return pFront->m_bLoaded || pFront->m_bMissing;
}


//-----------------------------------------------------------------------------
// Purpose: Dequeue the buffer entry (backdoor for list management)
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::MarkBufferDiscarded( BufferHandle_t hBuffer )
{
	m_BufferList.RemoveAt( hBuffer );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
//			proc - 
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::SetPostProcessed( WaveCacheHandle_t handle, bool proc )
{
	CAsyncWaveData *data = s_WaveCache.CacheGet( handle );
	if ( data )
	{
		data->SetPostProcessed( proc );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::Unload( WaveCacheHandle_t handle )
{
	// Don't actually unload, just mark it as stale
	s_WaveCache.Age( handle );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
//			*filename - 
//			datasize - 
//			startpos - 
//			**pData - 
//			copystartpos - 
//			*pbPostProcessed - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAsyncWavDataCache::GetDataPointer( WaveCacheHandle_t& handle, char const *filename, int datasize, int startpos, void **pData, int copystartpos, bool *pbPostProcessed )
{
	VPROF( "CAsyncWavDataCache::GetDataPointer" );

	Assert( pbPostProcessed );
	Assert( pData );

	*pbPostProcessed = false;

	bool bret = false;
	*pData = NULL;

	CAsyncWaveData *data = s_WaveCache.CacheLock( handle );
	if ( !data )
	{
		FileNameHandle_t fnh = g_pFileSystem->FindOrAddFileName( filename );

		CacheEntry_t search;
		search.name = fnh;
		search.handle = 0;

		int idx = m_CacheHandles.Find( search );
		if ( idx == m_CacheHandles.InvalidIndex() )
		{
			Assert( 0 );
			return bret;
		}

		// Try and reload it
		asyncwaveparams_t params;
		params.hFilename = fnh;
		params.datasize = datasize;
		params.seekpos = startpos;

		handle = m_CacheHandles[ idx ].handle = s_WaveCache.CacheCreate( params );
		data = s_WaveCache.CacheLock( handle );
		if ( !data )
		{
			return bret;
		}
	}

	// Cache entry exists, but if filesize == 0 then the file itself wasn't on disk...
	if ( data->m_nDataSize != 0 && copystartpos < data->m_nDataSize )
	{
		if ( data->BlockingGetDataPointer( pData ) )
		{
			*pData = (char *)*pData + copystartpos;
			bret = true;
		}
	}

	*pbPostProcessed = data->GetPostProcessed();

	// Release lock
	s_WaveCache.CacheUnlock( handle );
	return bret;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
//			*filename - 
//			datasize - 
//			startpos - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAsyncWavDataCache::IsDataLoadCompleted( WaveCacheHandle_t handle, bool *pIsValid, bool *pIsMissing )
{
	VPROF( "CAsyncWavDataCache::IsDataLoadCompleted" );

	CAsyncWaveData *data = s_WaveCache.CacheGet( handle );
	if ( !data )
	{
		*pIsValid = false;
		return false;
	}
	*pIsValid = true;
	if ( pIsMissing )
	{
		*pIsMissing = data->m_bMissing;
	}
	// bump the priority
	data->SetAsyncPriority( 1 );

	return data->m_bLoaded;
}


void CAsyncWavDataCache::RestartDataLoad( WaveCacheHandle_t *pHandle, const char *pFilename, int dataSize, int startpos )
{
	CAsyncWaveData *data = s_WaveCache.CacheGet( *pHandle );
	if ( !data )
	{
		*pHandle = AsyncLoadCache( pFilename, dataSize, startpos );
	}
}

bool CAsyncWavDataCache::IsDataLoadInProgress( WaveCacheHandle_t handle )
{
	CAsyncWaveData *data = s_WaveCache.CacheGet( handle );
	if ( data )
	{
		return data->IsCurrentlyLoading();
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::Flush( bool bTearDownStaticPool )
{
	if ( !m_bInitialized )
	{
		return;
	}

	if ( IsGameConsole() )
	{
		// this will sync stop (and unlock) any buffers that could not be unlocked at stream closure
		CleanupDeadBuffers( true );
	}

	// purge all unlocked resources
	s_WaveCache.Flush();

	MemoryUsageType spewType = SPEW_BASIC;
 
	if ( IsGameConsole() )
	{
		if ( bTearDownStaticPool )
		{
			// The caller has unlocked all static resources that should have been in this pool
			// and flush should have released them.
			// This is VERY scary, with this technique there is no way to ensure all resources from this pool
			// are really unlocked unless we scan the cache section annd make queries. And if they aren't,
			// nothing can be done because this is a stack, so either way it's just fatal.
			// Tear the pool down.
			g_pAudioStaticPool->FreeAll( false );
		}

		// a flush nominally occurs during level transitions to free up memory
		// the underlying pool's blob needs to get freed to use for level transition work
		// the next first allocation will re-establish the blob
		if ( !g_pAudioStreamPool->Count() )
		{
			g_pAudioStreamPool->Clear();
		}
		else
		{
			// this is used between levels, no sounds should be occuring, lock counts *should* be 0
			// the flush should have caused the stream buffers to destroy and thus the pool to be emptied
			// if sounds are not playing, buffers that are remaining are locked and would be zombied
			// zombied buffers would just accumulate until the stream pool couldn't play any more sounds
			// spewing more details into the log to scan after playtests to track (if any) down
			Warning( "CAsyncWavDataCache: Failed to clear stream pool during flush\n" );
			// need more detailed breakdown
			spewType = SPEW_ALL;
		}
	}

	SpewMemoryUsage( spewType );
}

//-----------------------------------------------------------------------------
// Purpose: Update loop position if a more accurate value has been found.
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::UpdateLoopPosition( StreamHandle_t hStream, int nLoopPosition )
{
	StreamedEntry_t &streamedEntry = m_StreamedHandles[hStream];
	streamedEntry.m_LoopStart = nLoopPosition;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAsyncWavDataCache::GetItemName( DataCacheClientID_t clientId, const void *pItem, char *pDest, unsigned nMaxLen  )
{
	CAsyncWaveData *pWaveData = (CAsyncWaveData *)pItem;
	Q_strncpy( pDest, pWaveData->GetFileName(), nMaxLen );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Spew a cache summary to the console
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::SpewMemoryUsage( MemoryUsageType level )
{
	WaveCacheStatus_t status;
	WaveCacheLimits_t limits;
	s_WaveCache.GetStatus( &status, &limits );

	int bytesUsed = status.nBytes;
	int bytesTotal = limits.nMaxBytes;

	if ( IsPC() )
	{
		if ( level != SPEW_BASIC )
		{
			for ( int i = m_CacheHandles.FirstInorder(); m_CacheHandles.IsValidIndex(i); i = m_CacheHandles.NextInorder(i) )
			{
				char name[MAX_PATH];
				if ( !g_pFileSystem->String( m_CacheHandles[ i ].name, name, sizeof( name ) ) )
				{
					Assert( 0 );
					continue;
				}

				if ( level == SPEW_MUSIC_NONSTREAMING && V_stristr( name, "music" ) == NULL )
					continue;

				WaveCacheHandle_t &handle = m_CacheHandles[ i ].handle;
				CAsyncWaveData *data = s_WaveCache.CacheGetNoTouch( handle );
				if ( data )
				{
					Msg( "\t%16.16s : %s\n", Q_pretifymem(data->Size()),name);
				}
				else
				{
					Msg( "\t%16.16s : %s\n", "not resident",name);
				}
			}
		}

		float percent;
		if ( bytesTotal <= 0 )
		{
			// unbounded, indeterminate
			percent = 0;
			bytesTotal = 0;
		}
		else
		{
			percent = 100.0f * (float)bytesUsed / (float)bytesTotal;
		}
		Msg( "CAsyncWavDataCache:  %i .wavs total %s, %.2f %% of capacity\n", m_CacheHandles.Count(), Q_pretifymem( bytesUsed, 2 ), percent );
	}
	
	if ( IsGameConsole() )
	{
		if ( level == SPEW_BASIC )
		{
			// basic spew memory usage is the total of all the outstanding buffers
			// this isn't intended as an entirely accurate memory usage report
			Msg( "CAsyncWavDataCache: %.2f MB used\n", bytesUsed / ( 1024.0f * 1024.0f ) );
			Msg( "CAsyncWavDataCache: Static Pool: %.2f MB used of %.2f MB\n", g_pAudioStaticPool->GetUsed() / ( 1024.0f * 1024.0f ), g_pAudioStaticPool->GetSize() / ( 1024.0f * 1024.0f ) );
			Msg( "CAsyncWavDataCache: Stream Pool: %.2f MB used of %.2f MB\n", ( g_pAudioStreamPool->Count() * g_pAudioStreamPool->BlockSize() ) / ( 1024.0f * 1024.0f ), g_pAudioStreamPool->Size() / ( 1024.0f * 1024.0f ) );
			Msg( "CAsyncWavDataCache: Dead Buffers: %d\n", m_DeadBuffers.Count() );
		}
		else
		{
			// detailed spew breaks the stream buffers into resident (pooled or standard) or streaming types
			// iterate non-stream buffers
			int nonStreamBytesUsedPooled = 0;
			Msg( "\nCAsyncWavDataCache: Non-Stream Buffer List (Pooled Heap):\n" );
			for ( BufferHandle_t h = m_BufferList.FirstInorder(); h != m_BufferList.InvalidIndex(); h = m_BufferList.NextInorder( h ) )
			{
				BufferEntry_t *pBuffer = &m_BufferList[h];
				CAsyncWaveData *pData = s_WaveCache.CacheGetNoTouch( pBuffer->m_hWaveData );
				s_WaveCache.CacheLockMutex();
				if ( pData && !pData->m_bIsTransient && pData->m_bIsStaticPooled )
				{
					int lockCount = s_WaveCache.GetLockCount( pBuffer->m_hWaveData );
					unsigned int ageStamp = s_WaveCache.GetAgeStamp( pBuffer->m_hWaveData );
					Msg( "Start:%7d Size:%7d Lock:%3d Age:%4d %s\n", pBuffer->m_StartPos, pData->m_nBufferBytes, lockCount, ageStamp, pData->GetFileName() );
					nonStreamBytesUsedPooled += pData->m_nBufferBytes;
				}
				s_WaveCache.CacheUnlockMutex();
			}
			Msg( "CAsyncWavDataCache: Non-Stream Buffers (Pooled): %.2f MB used\n", (float)nonStreamBytesUsedPooled / ( 1024.0f * 1024.0f ) );

			int nonStreamBytesUsedStandard = 0;
			Msg( "\nCAsyncWavDataCache: Non-Stream Buffer List (Standard Heap):\n" );
			for ( BufferHandle_t h = m_BufferList.FirstInorder(); h != m_BufferList.InvalidIndex(); h = m_BufferList.NextInorder( h ) )
			{
				BufferEntry_t *pBuffer = &m_BufferList[h];
				CAsyncWaveData *pData = s_WaveCache.CacheGetNoTouch( pBuffer->m_hWaveData );
				s_WaveCache.CacheLockMutex();
				if ( pData && !pData->m_bIsTransient && !pData->m_bIsStaticPooled )
				{
					int lockCount = s_WaveCache.GetLockCount( pBuffer->m_hWaveData );
					unsigned int ageStamp = s_WaveCache.GetAgeStamp( pBuffer->m_hWaveData );
					Msg( "Start:%7d Size:%7d Lock:%3d Age:%4d %s\n", pBuffer->m_StartPos, pData->m_nBufferBytes, lockCount, ageStamp, pData->GetFileName() );
					nonStreamBytesUsedStandard += pData->m_nBufferBytes;
				}
				s_WaveCache.CacheUnlockMutex();
			}
			Msg( "CAsyncWavDataCache: Non-Stream Buffers (Standard): %.2f MB used\n", (float)nonStreamBytesUsedStandard / ( 1024.0f * 1024.0f ) );

			// iterate stream buffers
			int streamBytesUsed = 0;
			Msg( "\nCAsyncWavDataCache: Stream Buffer List:\n" );
			for ( BufferHandle_t h = m_BufferList.FirstInorder(); h != m_BufferList.InvalidIndex(); h = m_BufferList.NextInorder( h ) )
			{
				BufferEntry_t *pBuffer = &m_BufferList[h];
				CAsyncWaveData *pData = s_WaveCache.CacheGetNoTouch( pBuffer->m_hWaveData );
				s_WaveCache.CacheLockMutex();
				if ( pData && pData->m_bIsTransient )
				{
					int lockCount = s_WaveCache.GetLockCount( pBuffer->m_hWaveData );
					unsigned int ageStamp = s_WaveCache.GetAgeStamp( pBuffer->m_hWaveData );
					Msg( "Start:%7d Size:%7d Lock:%3d Age:%4d %s\n", pBuffer->m_StartPos, pData->m_nBufferBytes, lockCount, ageStamp, pData->GetFileName() );
					streamBytesUsed += pData->m_nBufferBytes;
				}
				s_WaveCache.CacheUnlockMutex();
			}
			Msg( "CAsyncWavDataCache: Stream Buffers: %.2f MB used\n", (float)streamBytesUsed / ( 1024.0f * 1024.0f ) );

			// the stream pool usage should match exactly with the streaming buffer iteration results
			Msg( "\nCAsyncWavDataCache: Stream Pool\n" );
			Msg( "  Block Size: %d bytes\n", g_pAudioStreamPool->BlockSize() );
			Msg( "  Blocks:     %d\n", g_pAudioStreamPool->Count() );
			Msg( "  Allocated:  %.2f MB \n", ( g_pAudioStreamPool->Count() * g_pAudioStreamPool->BlockSize() ) / ( 1024.0f * 1024.0f ) );
			Msg( "  Pool:       %.2f MB \n", g_pAudioStreamPool->Size() / ( 1024.0f * 1024.0f ) );
			Msg( "  Dead:       %d\n", m_DeadBuffers.Count() );
		}
	}
}

//-----------------------------------------------------------------------------
// This is a scary function. It doesn't check for possible outstanding async operations
// It is part of shutdown.
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::Clear()
{
	int i;
	for ( i = m_CacheHandles.FirstInorder(); m_CacheHandles.IsValidIndex(i); i = m_CacheHandles.NextInorder(i) )
	{
		CacheEntry_t& dat = m_CacheHandles[i];
		s_WaveCache.CacheRemove( dat.handle );
	}
	m_CacheHandles.RemoveAll();

	FOR_EACH_LL( m_StreamedHandles, i )
	{
		StreamedEntry_t &dat = m_StreamedHandles[i];
		for ( int j=0; j<dat.m_numBuffers; ++j )
		{
			s_WaveCache.BreakLock( dat.m_hWaveData[j] );
			s_WaveCache.CacheRemove( dat.m_hWaveData[j] );
		}
	}
	m_StreamedHandles.RemoveAll();

	for ( int i = 0; i < m_DeadBuffers.Count(); i++ )
	{
		s_WaveCache.BreakLock( m_DeadBuffers[i].hWaveData );
		s_WaveCache.CacheRemove( m_DeadBuffers[i].hWaveData );
		
	}
	m_DeadBuffers.Purge();

	m_BufferList.RemoveAll();
}


static CAsyncWavDataCache g_AsyncWaveDataCache;
IAsyncWavDataCache *wavedatacache = &g_AsyncWaveDataCache;

CON_COMMAND( snd_async_flush, "Flush all unlocked async audio data" )
{
	g_AsyncWaveDataCache.Flush();
}

CON_COMMAND( snd_async_showmem, "Show async memory stats" )
{
	g_AsyncWaveDataCache.SpewMemoryUsage( CAsyncWavDataCache::SPEW_ALL );
}

CON_COMMAND( snd_async_showmem_summary, "Show brief async memory stats" )
{
	g_AsyncWaveDataCache.SpewMemoryUsage( CAsyncWavDataCache::SPEW_BASIC );
}

CON_COMMAND( snd_async_showmem_music, "Show async memory stats for just non-streamed music" )
{
	g_AsyncWaveDataCache.SpewMemoryUsage( CAsyncWavDataCache::SPEW_MUSIC_NONSTREAMING );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFileName - 
//			dataOffset - 
//			dataSize - 
//-----------------------------------------------------------------------------
void PrefetchDataStream( const char *pFileName, int dataOffset, int dataSize )
{
	if ( IsGameConsole() )
	{
		// Xbox streaming buffer implementation does not support this "hinting"
		return;
	}

	wavedatacache->PrefetchCache( pFileName, dataSize, dataOffset );
}

//-----------------------------------------------------------------------------
// Purpose: This is an instance of a stream.
//			This contains the file handle and streaming buffer
//			The mixer doesn't know the file is streaming.  The IWaveData
//			abstracts the data access.  The mixer abstracts data encoding/format
//-----------------------------------------------------------------------------
class CWaveDataStreamAsync : public IWaveData
{
public:
	CWaveDataStreamAsync( CAudioSource &source, IWaveStreamSource *pStreamSource, const char *pFileName, int fileStart, int fileSize, CSfxTable *sfx, int startOffset, SoundError &soundError );
	~CWaveDataStreamAsync( void );

	// return the source pointer (mixer needs this to determine some things like sampling rate)
	CAudioSource &Source( void ) { return m_source; }

	// Read data from the source - this is the primary function of a IWaveData subclass
	// Get the data from the buffer (or reload from disk)
	virtual int ReadSourceData( void **pData, int64 sampleIndex, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );
	bool IsValid() { return m_bValid; }
	virtual bool IsReadyToMix();
	virtual void UpdateLoopPosition( int nLoopPosition );

private:
	CWaveDataStreamAsync( const CWaveDataStreamAsync & );

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Output : byte
	//-----------------------------------------------------------------------------
	inline byte *GetCachedDataPointer()
	{
		VPROF( "CWaveDataStreamAsync::GetCachedDataPointer" );

		CAudioSourceCachedInfo *info = m_AudioCacheHandle.Get( CAudioSource::AUDIO_SOURCE_WAV, m_pSfx->IsPrecachedSound(), m_pSfx, &m_nCachedDataSize );
		if ( !info )
		{
			Assert( !"CAudioSourceWave::GetCachedDataPointer info == NULL" );
			return NULL;
		}

		return (byte *)info->CachedData();
	}

	char const				*GetFileName();
	CAudioSource			&m_source;					// wave source
	IWaveStreamSource		*m_pStreamSource;			// streaming
	int						m_sampleSize;				// size of a sample in bytes
	int						m_waveSize;					// total number of samples in the file

	int						m_bufferSize;				// size of transfer buffer in samples
	char					*m_pBuffer;					// transfer buffer
	int64					m_sampleIndex;
	int						m_bufferCount;
	int						m_dataStart;
	int						m_dataSize;

	WaveCacheHandle_t		m_hCache;
	StreamHandle_t			m_hStream;
	FileNameHandle_t		m_hFileName;
	
	CAudioSourceCachedInfoHandle_t m_AudioCacheHandle;
	int						m_nCachedDataSize;
	CSfxTable				*m_pSfx;

	// These members are used to handle more gracefully exhausted streaming
	static const int SIZE_LAST_SAMPLE = 8;				// Support up to stereo 32 bits sample
	uint8					m_LastSample[SIZE_LAST_SAMPLE];

	unsigned int			m_bValid : 1;
};

CWaveDataStreamAsync::CWaveDataStreamAsync
	( 
		CAudioSource &source, 
		IWaveStreamSource *pStreamSource, 
		const char *pFileName, 
		int fileStart, 
		int fileSize, 
		CSfxTable *sfx,
		int startOffset,
		SoundError &soundError
	) : 
	m_source( source ), 
	m_dataStart( fileStart ), 
	m_dataSize( fileSize ), 
	m_pStreamSource( pStreamSource ), 
	m_bValid( false ), 
	m_hCache( 0 ),
	m_hStream( INVALID_STREAM_HANDLE ),
	m_hFileName( 0 ), 
	m_pSfx( sfx ),
	m_pBuffer( NULL )
{
	soundError = SE_OK;
	m_hFileName = g_pFileSystem->FindOrAddFileName( pFileName );

	// nothing in the buffer yet
	m_sampleIndex = 0;
	m_bufferCount = 0;

	m_nCachedDataSize = 0;

	if ( m_dataSize <= 0 )
	{
		DevMsg( 1, "Can't find streaming wav file: sound\\%s\n", GetFileName() );
		soundError = SE_FILE_NOT_FOUND;
		return;
	}

	if ( IsPC() )
	{
		m_hCache = wavedatacache->AsyncLoadCache( GetFileName(), m_dataSize, m_dataStart );

		m_pBuffer = new char[SINGLE_BUFFER_SIZE];
		Q_memset( m_pBuffer, 0, SINGLE_BUFFER_SIZE );

		// size of a sample
		m_sampleSize = source.SampleSize();
		// size in samples of the buffer
		m_bufferSize = SINGLE_BUFFER_SIZE / m_sampleSize;
		// size in samples (not bytes) of the wave itself
		m_waveSize = fileSize / m_sampleSize;

		m_AudioCacheHandle.Get( CAudioSource::AUDIO_SOURCE_WAV, m_pSfx->IsPrecachedSound(), m_pSfx, &m_nCachedDataSize );
	}
	
	if ( IsGameConsole() )
	{
		// size of a sample
		m_sampleSize = source.SampleSize();
		// size in samples (not bytes) of the wave itself
		m_waveSize = fileSize / m_sampleSize;

		// mark as transient to let streamer better configure the buffers for a streaming wave	
		// absence of transient indicates a memory resident wave
		streamFlags_t flags = STREAMED_FROMDVD | STREAMED_TRANSIENT;
		if ( IsGameConsole() )
		{
			char cleanName[MAX_PATH];
			V_strncpy( cleanName, pFileName, sizeof( cleanName ) );
			V_FixSlashes( cleanName, '/' );

			bool bForceToSinglePlay = false;

			if ( bForceToSinglePlay || 
				V_stristr( cleanName, "music/" ) || 
				V_stristr( cleanName, "commentary/" ) ||
				V_stristr( cleanName, "playonce/" ))
			{
				// music discards and cycles its buffers
				flags |= STREAMED_SINGLEPLAY;
			}
			else if ( !source.IsSentenceWord() && V_stristr( cleanName, "vo/" ) )
			{
				// vo discards and cycles its buffers, except for sentence sources which recur
				flags |= STREAMED_SINGLEPLAY;
			}
	}

#if IsX360()
		// the xma mixer expects quantum xma blocks
		COMPILE_TIME_ASSERT( ( STREAM_BUFFER_DATASIZE % XMA_BLOCK_SIZE ) == 0 );
		// streaming buffers must be sector compliant
		COMPILE_TIME_ASSERT( ( STREAM_BUFFER_DATASIZE % XBOX_DVD_SECTORSIZE ) == 0 );
#endif

		int transferSize = STREAM_BUFFER_DATASIZE;
		int nAlignedDataSize = AlignValue( m_dataSize, transferSize );
		int numBuffers = nAlignedDataSize / transferSize;					// Number of buffers that we would need at max

		int nNecessaryBuffers = source.IsLooped() ? 4 : 2;					// By default we need 2 buffers, but when looping the safe number is 4
		nNecessaryBuffers = MIN( nNecessaryBuffers, STREAM_BUFFER_COUNT );	// Max numbers of buffers allowed on that platform
		numBuffers = MIN( numBuffers, nNecessaryBuffers );					// Don't allocate more buffers than necessary to load completely the sound

		if ( numBuffers == 1 )
		{
			// the transfer buffer can be exact sized
			// [oliviern] I kept the old behavior but it seems to me that this would fragment the memory (and would not help in worst case anyway)
			transferSize = m_dataSize;
		}

		// allocate a transfer buffer
		// when multiple buffering, exactly matches the size of the streaming buffer
		// ensures that a streaming buffer can be entirely consumed and requeued during the transfer
		m_pBuffer = new char[transferSize];
		// size in samples of the transfer buffer
		m_bufferSize = transferSize / m_sampleSize;

		int loopStart;
		if ( source.IsLooped() )
		{
			int loopBlock;
			loopStart = m_pStreamSource->GetLoopingInfo( &loopBlock, NULL, NULL ) * m_sampleSize;

			// Note that the loop start is inaccurate for some format (like WAVE_FORMAT_MP3).
			// It is a bytes position in uncompressed samples, but this is not valid for MP3.
			// We actually update the loopStart when we parse the sound (while playing it) and we find the MP3 frame that contains the loop position.
			// We then call UpdateLoopPosition() with the corresponding MP3 frame.
			// In most cases, the full MP3 sound is going to fit in the streaming buffers. For long sounds when the looping points is far (like at the beginning),
			// loopStart is going to be updated before it is actually used.
			// There is a potential for rare cases where we will start streaming with an incorrect loop position, then update the loop point at a later time.
			// In this case, we may stream some memory for nothing or in some extreme cases, have the streamed buffers not ready in time.
			// This would happen only for the first loop.
			// The XMA format fixes that by approximating loopStart with loopBlock (because compressed samples are fortunately aligned on 2048 bytes).
#if defined( _GAMECONSOLE )
			switch ( source.Format() )
			{
#if IsX360()
			case WAVE_FORMAT_XMA:
				// xma works in blocks, mixer handles inter-block accurate loop positioning
				// block streaming will cycle from the block where the loop occurs
				loopStart = loopBlock * XMA_BLOCK_SIZE;
				break;
#endif
#if IsPS3()
			case WAVE_FORMAT_MP3:
				loopStart /= 10;		// We assume that we have a compression factor of 10.
										// With the streamed buffer, it is better to start before than after (as the current impl. reads several buffers forward).
				if ( loopStart > fileSize )
				{
					// Make sure we are in a reasonable range
					loopStart = fileSize - transferSize;
					if ( loopStart < 0 )
					{
						loopStart = 0;
					}
				}
				break;

			case WAVE_FORMAT_TEMP:
				// Uncompressed, so keep it roughly the same
				break;
#endif
			default:
				// Nothing to fix up
				break;
			}
#endif

		}
		else
		{
			// sample not looped
			loopStart = -1;
		}

		// load the file piecewise through a buffering implementation
		m_hStream = wavedatacache->OpenStreamedLoad( pFileName, m_dataSize, m_dataStart, startOffset, loopStart, STREAM_BUFFER_DATASIZE, numBuffers, flags, soundError );
		if ( m_hStream == INVALID_STREAM_HANDLE )
		{
			if ( soundError == SE_NO_STREAM_BUFFER )
			{
				DevWarning( "[Sound] Not enough stream buffer available to setup wav file: '\\%s'\n", GetFileName() );
			}
			else
			{
				DevWarning( "[Sound] Failed to setup streaming wav file: sound\\%s\n", GetFileName() );
			}
			m_bValid = false;
			return;
		}
		else
		{
			m_sampleIndex = startOffset;		// Do not forget to initialize the sample index at the correct position
												// otherwise this is going to put the whole streaming algorithm off.
												// The whole streaming layer in this file should be re-written by the way as it is needlessly complicated
												// and only works in very specific conditions.
		}
	}

	V_memset( m_LastSample, 0, sizeof( m_LastSample ) );

	m_bValid = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWaveDataStreamAsync::~CWaveDataStreamAsync( void ) 
{
	if ( IsPC() && m_source.IsPlayOnce() && m_source.CanDelete() )
	{
		m_source.SetPlayOnce( false ); // in case it gets used again
		wavedatacache->Unload( m_hCache );
	}

	if ( IsGameConsole() )
	{
		wavedatacache->CloseStreamedLoad( m_hStream ); 
	}

	delete [] m_pBuffer;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *CWaveDataStreamAsync::GetFileName()
{
	static char fn[MAX_PATH];

	if ( m_hFileName )
	{
		if ( g_pFileSystem->String( m_hFileName, fn, sizeof( fn ) ) )
		{
			return fn;
		}
	}

	Assert( 0 );
	return "";
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWaveDataStreamAsync::IsReadyToMix()
{
	if ( IsPC() )
	{
		// If not async loaded, start mixing right away
		if ( !m_source.IsAsyncLoad() && !snd_async_fullyasync.GetBool() )
		{
			return true;
		}

		bool bCacheValid;
		bool bLoaded = wavedatacache->IsDataLoadCompleted( m_hCache, &bCacheValid );
		if ( !bCacheValid )
		{
			wavedatacache->RestartDataLoad( &m_hCache, GetFileName(), m_dataSize, m_dataStart );
		}

		// When laying off a movie, all sound access is forced to be synchronous to get better 
		//  synchronization (avoids async loading random delay)
		if ( g_pEngineToolInternal->IsRecordingMovie() )
		{
			return true;
		}

		return bLoaded;
	}

	if ( IsGameConsole() )
	{
		return wavedatacache->IsStreamedDataReady( m_hStream );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CWaveDataStreamAsync::UpdateLoopPosition( int nLoopPosition )
{
	if ( m_hStream != INVALID_STREAM_HANDLE )
	{
		wavedatacache->UpdateLoopPosition( m_hStream, nLoopPosition );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Read data from the source - this is the primary function of a IWaveData subclass
//  Get the data from the buffer (or reload from disk)
// Input  : **pData - 
//			sampleIndex - 
//			sampleCount - 
//			copyBuf[AUDIOSOURCE_COPYBUF_SIZE] - 
// Output : int
//-----------------------------------------------------------------------------
int CWaveDataStreamAsync::ReadSourceData( void **pData, int64 sampleIndex, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] )
{
	int nOldBufferCount = m_bufferCount;
	int nOldSampleIndex = m_sampleIndex;

	// Current file position
	int seekpos = m_dataStart + m_sampleIndex * m_sampleSize;

	// wrap position if looping
	if ( m_source.IsLooped() )
	{
		sampleIndex = m_pStreamSource->UpdateLoopingSamplePosition( sampleIndex );
		if ( sampleIndex < m_sampleIndex )
		{
			if ( snd_report_loop_sound.GetBool() )
			{
				Warning( "[Sound] Sound \"%s\" just looped.\n", GetFileName( ) );
			}

			// looped back, buffer has no samples yet
			m_sampleIndex = sampleIndex;
			m_bufferCount = 0;

			// update file position
			seekpos = m_dataStart + sampleIndex * m_sampleSize;
		}
	}

	// UNDONE: This is an error!!
	// The mixer playing back the stream tried to go backwards!?!?!
	// BUGBUG: Just play the beginning of the buffer until we get to a valid linear position
	if ( sampleIndex < m_sampleIndex )
		sampleIndex = m_sampleIndex;

	// calc sample position relative to the current buffer
	// m_sampleIndex is the sample position of the first byte of the buffer
	sampleIndex -= m_sampleIndex;
	
	// out of range? refresh buffer
	if ( sampleIndex >= m_bufferCount )
	{
		// advance one buffer (the file is positioned here)
		m_sampleIndex += m_bufferCount;
		// next sample to load
		sampleIndex -= m_bufferCount;

		// if the remainder is greater than one buffer size, seek over it.  Otherwise, read the next chunk
		// and leave the remainder as an offset.

		// number of buffers to "skip" (as in the case where we are starting a streaming sound not at the beginning)
		int skips = sampleIndex / m_bufferSize;
		
		// If we are skipping over a buffer, do it with a seek instead of a read.
		if ( skips )
		{
			// skip directly to next position
			m_sampleIndex += sampleIndex;
			sampleIndex = 0;
		}

		// move the file to the new position
		seekpos = m_dataStart + (m_sampleIndex * m_sampleSize);

		// This is the maximum number of samples we could read from the file
		m_bufferCount = m_waveSize - m_sampleIndex;
		
		// past the end of the file?  stop the wave.
		if ( m_bufferCount <= 0 )
			return 0;

		// clamp available samples to buffer size
		if ( m_bufferCount > m_bufferSize )
			m_bufferCount = m_bufferSize;

		if ( IsPC() )
		{
			// See if we can load in the initial data right out of the cached data lump instead.
			int cacheddatastartpos = ( seekpos - m_dataStart );

			// FastGet doesn't call into IsPrecachedSound if the handle appears valid...
			CAudioSourceCachedInfo *info = m_AudioCacheHandle.FastGet();
			if ( !info )
			{
				// Full recache
				info = m_AudioCacheHandle.Get( CAudioSource::AUDIO_SOURCE_WAV, m_pSfx->IsPrecachedSound(), m_pSfx, &m_nCachedDataSize );
			}

			bool startupCacheUsed = false;

			if  ( info && 
				( m_nCachedDataSize > 0 ) && 
				( cacheddatastartpos < m_nCachedDataSize ) )
			{
				// Get a ptr to the cached data
				const byte *cacheddata = info->CachedData();
				if ( cacheddata )
				{
					// See how many samples of cached data are available (cacheddatastartpos is zero on the first read)
					int availSamples = ( m_nCachedDataSize - cacheddatastartpos ) / m_sampleSize;

					// Clamp to size of our internal buffer
					if ( availSamples > m_bufferSize )
					{
						availSamples = m_bufferSize;
					}

					// Mark how many we are returning
					m_bufferCount = availSamples;
					// Copy raw sample data directly out of cache
					Q_memcpy( m_pBuffer, ( char * )cacheddata + cacheddatastartpos, availSamples * m_sampleSize );

					startupCacheUsed = true;
				}
			}

			// Not in startup cache, grab data from async cache loader (will block if data hasn't arrived yet)
			if ( !startupCacheUsed )
			{
				bool postprocessed = false;
				
				// read in the max bufferable, available samples
				if ( !wavedatacache->CopyDataIntoMemory( 
					m_hCache, 
					GetFileName(), 
					m_dataSize, 
					m_dataStart,
					m_pBuffer, 
					m_bufferSize * m_sampleSize,
					seekpos, 
					m_bufferCount * m_sampleSize,
					&postprocessed ) )
				{
					return 0;
				}

				// do any conversion the source needs (mixer will decode/decompress)
				if ( !postprocessed )
				{
					// Note that we don't set the postprocessed flag on the underlying data, since for streaming we're copying the
					//  original data into this buffer instead.
					m_pStreamSource->UpdateSamples( m_pBuffer, m_bufferCount );
				}
			}
		}

		if ( IsGameConsole() )
		{
			if ( m_hStream != INVALID_STREAM_HANDLE )
			{
				// request available data, may get less
				// drives the buffering
				int nBytesFilled = wavedatacache->CopyStreamedDataIntoMemory( 
									m_hStream, 
									m_pBuffer, 
									m_bufferSize * m_sampleSize,
									seekpos, 
									m_bufferCount * m_sampleSize );

				if ( nBytesFilled == 0 )
				{
					// If we reach here, it means the streamer is behind
					// Let's try to recover so the sound does not stop abruptly. It only works for PCM sounds though,
					// for XMA and MP3 sounds, we have to do this at their own level (as we can't send simulated data for them).
					if ( snd_async_stream_recover_from_exhausted_stream.GetBool() && ( m_source.Format() == WAVE_FORMAT_PCM ) )
					{
						if ( m_sampleSize <= SIZE_LAST_SAMPLE )
						{
							if ( snd_async_stream_spew_exhausted_buffer.GetBool() && ( g_pQueuedLoader->IsMapLoading() == false ) )
							{
								static uint sOldTime = 0;
								uint nCurrentTime = Plat_MSTime();
								if ( nCurrentTime >= sOldTime + snd_async_stream_spew_exhausted_buffer_time.GetInt() )
								{
									Warning( "[Sound] The stream buffer is exhausted for sound '%s'. Except after loading, fill a bug to have the number of sounds played reduced.\n", GetFileName() );
									sOldTime = nCurrentTime;
								}
							}
							// This code is not optimized, but hopefully never executed.
							int nSamplesToFill = imin( m_bufferCount, AUDIOSOURCE_COPYBUF_SIZE  / m_sampleSize );
							nSamplesToFill = imin( nSamplesToFill, sampleCount );
							if ( copyBuf != NULL )
							{
								for ( int i = 0 ; i < nSamplesToFill ; ++i )
								{
									V_memcpy( &copyBuf[ i * m_sampleSize ], &m_LastSample, m_sampleSize );
								}
							}
							*pData = copyBuf;

							m_bufferCount = nOldBufferCount;		// Put back the values to the state before any change were applied.
							m_sampleIndex = nOldSampleIndex;		// In some cases, we could actually have a buffer filled with static if the sound was smaller than one buffer long.
							return nSamplesToFill;
						}
					}
				}
				else
				{
					if ( m_sampleSize <= SIZE_LAST_SAMPLE )
					{
						// Let's copy the last sample, in case next time the streamer is too late. We fill with the last sample instead of zero to reduce potential pops.
						V_memcpy( &m_LastSample, &m_pBuffer[ nBytesFilled - m_sampleSize ], m_sampleSize );
					}
				}

				// convert to number of samples in the buffer
				m_bufferCount = nBytesFilled / m_sampleSize;
			}
			else
			{
				return 0;
			}

			// do any conversion now the source needs (mixer will decode/decompress) on this buffer
			m_pStreamSource->UpdateSamples( m_pBuffer, m_bufferCount );
		}
	}

	// If we have some samples in the buffer that are within range of the request
	if ( sampleIndex < m_bufferCount )
	{
		// Get the desired starting sample
		*pData = (void *)&m_pBuffer[sampleIndex * m_sampleSize];

		// max available
		int available = m_bufferCount - sampleIndex;
		// clamp available to max requested
		if ( available > sampleCount )
			available = sampleCount;

		return available;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Iterator for wave data (this is to abstract streaming/buffering)
//-----------------------------------------------------------------------------
class CWaveDataMemoryAsync : public IWaveData
{
public:
	CWaveDataMemoryAsync( CAudioSource &source );
	~CWaveDataMemoryAsync( void ) {}
	CAudioSource &Source( void ) { return m_source; }
	
	virtual int ReadSourceData( void **pData, int64 sampleIndex, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );
	virtual bool IsReadyToMix();
	virtual void UpdateLoopPosition( int nLoopPosition );

private:
	CAudioSource		&m_source;	// pointer to source
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &source - 
//-----------------------------------------------------------------------------
CWaveDataMemoryAsync::CWaveDataMemoryAsync( CAudioSource &source ) : 
	m_source(source) 
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : **pData - 
//			sampleIndex - 
//			sampleCount - 
//			copyBuf[AUDIOSOURCE_COPYBUF_SIZE] - 
// Output : int
//-----------------------------------------------------------------------------
int CWaveDataMemoryAsync::ReadSourceData( void **pData, int64 sampleIndex, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] )
{
	return m_source.GetOutputData( pData, sampleIndex, sampleCount, copyBuf );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWaveDataMemoryAsync::IsReadyToMix()
{
	if ( !m_source.IsAsyncLoad() && !snd_async_fullyasync.GetBool() )
	{
		// Wait until we're pending at least
		if ( m_source.GetCacheStatus() == CAudioSource::AUDIO_NOT_LOADED || m_source.GetCacheStatus() == CAudioSource::AUDIO_ERROR_LOADING )
		{
			// When laying off a movie, all sound access is forced to be synchronous to get better 
			//  synchronization (avoids async loading random delay)
			if ( g_pEngineToolInternal->IsRecordingMovie() )
			{
				return true;
			}

			return false;
		}
		return true;
	}

	if ( m_source.IsCached() )
	{
		return true;
	}

	if ( IsPC() )
	{
		// Msg( "Waiting for data '%s'\n", m_source.GetFileName() );
		m_source.CacheLoad();
	}

	if ( IsGameConsole() )
	{
		// expected to be resident and valid, otherwise being called prior to load
		Assert( 0 );
	}

	return false;
}

void CWaveDataMemoryAsync::UpdateLoopPosition( int nLoopPosition )
{
	// Should not be necessary in this implementation...
	Assert( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &source - 
//			*pStreamSource - 
//			&io - 
//			*pFileName - 
//			dataOffset - 
//			dataSize - 
// Output : IWaveData
//-----------------------------------------------------------------------------
IWaveData *CreateWaveDataStream( CAudioSource &source, IWaveStreamSource *pStreamSource, const char *pFileName, int dataStart, int dataSize, CSfxTable *pSfx, int startOffset, int skipInitialSamples, SoundError &soundError )
{
	CWaveDataStreamAsync *pStream = new CWaveDataStreamAsync( source, pStreamSource, pFileName, dataStart, dataSize, pSfx, startOffset, soundError );
	if ( !pStream || !pStream->IsValid() )
	{
		delete pStream;
		pStream = NULL;
	}
	return pStream;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &source - 
// Output : IWaveData
//-----------------------------------------------------------------------------
IWaveData *CreateWaveDataMemory( CAudioSource &source )
{
	CWaveDataMemoryAsync *mem = new CWaveDataMemoryAsync( source );
	return mem;
}

// set this to zero to revert to the previous scalar code
#define PHONON_USE_SIMD 0


namespace
{
	const int PHONON_FRAME_SIZE = 1024 * 2;

	ThreadHandle_t g_hPhononThread;
	bool g_bPhononThreadExit;
	CThreadFastMutex g_phononMutex;

	IPLAudioFormat g_phononInputFormat, g_phononOutputFormat;
	IPLhandle g_phononContext = 0;

	static const int PHONON_SAMPLE_SIZE = 4;
	struct PhononJob
	{
		~PhononJob()
		{
			if (phononEffect)
			{
				iplDestroyBinauralEffect(&phononEffect);
			}
		}
		IPLHrtfInterpolation interpolation;
		int nChannels;
		int nDataWidth;

		Vector vec;
		float lerp;
		IPLhandle phononEffect;

		bool bComplete;

		CThreadFastMutex mutex;

		char inputBuf[PHONON_FRAME_SIZE * PHONON_SAMPLE_SIZE];
		short outputBuf[PHONON_FRAME_SIZE * 2];

		float fSubmitTime, fCompleteTime;
	};

	void ProcessPhononJob(PhononJob& job)
	{
		TM_ZONE( TELEMETRY_LEVEL0, TMZF_NONE, "ProcessPhononJob %d", PHONON_FRAME_SIZE );
		if (!job.phononEffect)
		{
			TM_ZONE( TELEMETRY_LEVEL0, TMZF_NONE, "iplCreateBinauralEffect" );
			IPLerror error = iplCreateBinauralEffect(g_phononContext, g_phononInputFormat, g_phononOutputFormat, &job.phononEffect);
			if (error)
			{
				ConVarRef snd_use_hrtf("snd_use_hrtf");
				snd_use_hrtf.SetValue(false);
				DevMsg("Failed to create binaural effect: %d. Turning off binaural rendering\n", (int)error);
				memset( job.outputBuf, 0, sizeof( job.outputBuf ) );
				return;
			}
		}
		float ALIGN16 phononInputBuf[PHONON_FRAME_SIZE];

		// stereo inputs are scaled by 0.5 to avoid distortion
		float flStereoScale = 0.5f;

		const short *in = (const short*)job.inputBuf;
		// base scalar code for clarity
#if !PHONON_USE_SIMD
//		if ( !PHONON_USE_SIMD )
		{
			TM_ZONE( TELEMETRY_LEVEL0, TMZF_NONE, "ConvertMultiToFloat" );
			for ( int n = 0; n != PHONON_FRAME_SIZE; ++n )
			{
				int value = 0;

				for ( int i = 0; i != job.nChannels; ++i )
				{
					value += *in / job.nChannels;
					++in;
				}

				phononInputBuf[ n ] = value / ( 65536.0 * 0.5 );
			}
		}
#else  //!PHONON_USE_SIMD
//		else
		{
			const float flSigned16ToUnitScale = 1.0f / 32768.0f;

			if ( job.nChannels == 1 )
			{
				TM_ZONE( TELEMETRY_LEVEL0, TMZF_NONE, "ConvertShortToFloat" );
				ConvertShortToFloat( phononInputBuf, in, PHONON_FRAME_SIZE, flSigned16ToUnitScale );
			}
			else
			{
				TM_ZONE( TELEMETRY_LEVEL0, TMZF_NONE, "SumStereoShortToFloat" );
				SumStereoShortToFloat( phononInputBuf, in, PHONON_FRAME_SIZE, flSigned16ToUnitScale * flStereoScale );
			}
		}
#endif //!PHONON_USE_SIMD
		float ALIGN16 phononOutputBuf[PHONON_FRAME_SIZE * 2];
		memset( phononOutputBuf, 0, sizeof( phononOutputBuf ) );

		IPLAudioBuffer input_buffer;
		input_buffer.format = g_phononInputFormat;
		input_buffer.numSamples = PHONON_FRAME_SIZE;
		input_buffer.interleavedBuffer = phononInputBuf;
		input_buffer.deinterleavedBuffer = nullptr;

		IPLAudioBuffer output_buffer;
		output_buffer.format = g_phononOutputFormat;
		output_buffer.numSamples = PHONON_FRAME_SIZE;
		output_buffer.interleavedBuffer = phononOutputBuf;
		output_buffer.deinterleavedBuffer = nullptr;

		IPLVector3 iplvec;
		iplvec.x = job.vec.x;
		iplvec.y = job.vec.y;
		iplvec.z = job.vec.z;

		{
			TM_ZONE( TELEMETRY_LEVEL0, TMZF_NONE, "iplApplyBinauralEffect" );
			iplApplyBinauralEffect( job.phononEffect, input_buffer, iplvec, job.interpolation, output_buffer );
		}

#if !PHONON_USE_SIMD
		//if ( !PHONON_USE_SIMD )
		{
			TM_ZONE( TELEMETRY_LEVEL0, TMZF_NONE, "Convert" );
			for ( int n = 0; n < PHONON_FRAME_SIZE * 2; ++n )
			{
				Assert( phononOutputBuf[ n ] > -8.0 && phononOutputBuf[ n ] < 8.0 );
				int out = static_cast<int>( phononOutputBuf[ n ] * 65536.0 / 16.0 );
				job.outputBuf[ n ] = static_cast<short>( out );
			}
		}
#else  //!PHONON_USE_SIMD
		//else
		{
			TM_ZONE( TELEMETRY_LEVEL0, TMZF_NONE, "ConvertSIMD" );
			// convert from float [-1,1] to short [-32767, 32768], but phonon may have amplified so reduce to 0.125f
			float flScaleFactorFloat = ( 32768.0f ) * ( 0.125f );
			ConvertFloatToShort( job.outputBuf, &phononOutputBuf[ 0 ], PHONON_FRAME_SIZE * 2, flScaleFactorFloat );
		}
#endif //!PHONON_USE_SIMD
	}

	void SetupPhononContext()
	{
		if (g_phononContext)
		{
			return;
		}

		ConVarRef snd_use_hrtf("snd_use_hrtf");

		IPLGlobalContext iplContext;
		memset(&iplContext, 0, sizeof(iplContext));
		iplContext.logCallback = nullptr;

		IPLDspParams dspParams;
		dspParams.samplingRate = 44100;
		dspParams.frameSize = PHONON_FRAME_SIZE;

		IPLerror phononError = iplCreate3DContext(iplContext, dspParams, nullptr, &g_phononContext);
		if (phononError != IPL_STATUS_SUCCESS)
		{
			DevMsg( "Failed to init phonon: %d\n", (int)phononError );
			snd_use_hrtf.SetValue(false);
			return;
		}

		g_phononInputFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
		g_phononInputFormat.channelLayout = IPL_CHANNELLAYOUT_MONO;
		g_phononInputFormat.numSpeakers = 1;
		g_phononInputFormat.speakerDirections = nullptr;
		g_phononInputFormat.channelOrder = IPL_CHANNELORDER_INTERLEAVED;

		g_phononOutputFormat = g_phononInputFormat;
		g_phononOutputFormat.channelLayout = IPL_CHANNELLAYOUT_STEREO;
		g_phononOutputFormat.numSpeakers = 2;
	}

	CTSQueue<PhononJob*> g_phononJobQueue, g_phononDeleteQueue;

	CThreadEvent g_eventPhononThread;

	int g_benchSecond = 0;
	float g_sleepTime = 0.0;
	float g_fMaxJobTime = 0.0;
	float g_fBenchRunway = -1.0f;
	int g_nBenchSkips = 0;
	int g_nBenchNumSounds = 0;
	int g_nBenchMaxNumSounds = 0;
	CThreadFastMutex g_mutexBenchSkips;

	ConVar snd_hrtf_benchmark("snd_hrtf_benchmark", "0", FCVAR_NONE, "");

	void PhononThread()
	{
		SetupPhononContext();

		while (!g_bPhononThreadExit)
		{
			PhononJob *job = nullptr;
			while (g_phononJobQueue.PopItem(&job))
			{
				//				float t = Plat_FloatTime();
				ProcessPhononJob(*job);
				//				DevMsg("HRTF: Processed HRTF job %p in %f\n", job, Plat_FloatTime() - t);

				if (snd_hrtf_benchmark.GetBool())
				{
					float fTime = Plat_FloatTime();
					float fElapsed = fTime - job->fSubmitTime;
					job->fCompleteTime = fTime;
					if (fElapsed > g_fMaxJobTime)
					{
						g_fMaxJobTime = fElapsed;
					}
				}

				job->mutex.Lock();
				job->bComplete = true;
				job->mutex.Unlock();
			}

			while (g_phononDeleteQueue.Count() > 0 && g_phononJobQueue.Count() == 0)
			{
				if (g_phononDeleteQueue.PopItem(&job))
				{
					delete job;
				}
			}

			if (snd_hrtf_benchmark.GetBool())
			{
				float start_sleep = Plat_FloatTime();
				g_eventPhononThread.Wait(10);
				float end_sleep = Plat_FloatTime();
				int sec = (int)end_sleep;
				if (sec != g_benchSecond)
				{
					g_sleepTime += float(sec) - start_sleep;

					g_mutexBenchSkips.Lock();
					const int nSkips = g_nBenchSkips;
					const float fRunway = g_fBenchRunway;
					g_nBenchSkips = 0;
					g_fBenchRunway = -1.0f;
					const int nSounds = g_nBenchMaxNumSounds;
					g_nBenchMaxNumSounds = g_nBenchNumSounds;
					g_mutexBenchSkips.Unlock();


					float busy = (1.0 - g_sleepTime)*100.0f;
					DevMsg("HRTF: sounds: %d busy: %0.2f%% Max latency: %0.3fms runway: %0.3fms skips: %d\n", nSounds, busy, g_fMaxJobTime*1000.0f, fRunway*1000.0f, nSkips);
					g_sleepTime = end_sleep - float(sec);

					g_fMaxJobTime = 0.0f;

					g_benchSecond = sec;
				}
				else
				{
					g_sleepTime += end_sleep - start_sleep;
				}
			}
			else
			{
				//Wait for a signal that there is more work or at
				//most 10ms.
				g_eventPhononThread.Wait(10);
			}
		}
	}

}

ConVar snd_hrtf_passthrough("snd_hrtf_passthrough", "0", FCVAR_NONE, "");
ConVar snd_hrtf_async("snd_hrtf_async", "1", FCVAR_NONE, "");
ConVar snd_hrtf_lerp_method("snd_hrtf_lerp_method", "0", FCVAR_NONE, "");

void StartPhononThread()
{
	if (!snd_hrtf_async.GetBool())
	{
		SetupPhononContext();
		return;
	}

	if (!g_hPhononThread )
	{
		g_bPhononThreadExit = false;
		g_hPhononThread = ThreadExecuteSolo("SndMixPhonon", PhononThread);
		if (!g_hPhononThread)
		{
			DevMsg("Failed to start phonon thread\n");
		}
	}
}

void ShutdownPhononThread()
{
	if (g_hPhononThread)
	{
		g_bPhononThreadExit = true;
		g_eventPhononThread.Set();
		ThreadJoin(g_hPhononThread);
		ReleaseThreadHandle(g_hPhononThread);
		g_hPhononThread = NULL;
	}
}

class CWaveDataHRTF : public IWaveData
{
public:
	CWaveDataHRTF(IWaveData* pData, hrtf_info_t* dir);
	~CWaveDataHRTF();
	
	CAudioSource &Source() { return m_pData->Source(); }

	virtual int ReadSourceData(void **pData, int64 sampleIndex, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE]);
	virtual bool IsReadyToMix()
	{
		if (snd_hrtf_passthrough.GetBool())
		{
			return m_pData->IsReadyToMix();
		}

		if (m_bAsync == false)
		{
			return m_pData->IsReadyToMix() || SamplesAvailableToRead() >= 512;
		}

		TryCompleteJob();

		bool bResult = m_bEOF || SamplesAvailableToRead() > 0;
		if (snd_hrtf_benchmark.GetBool())
		{
			if (bResult && m_bHasStartedMixing == false)
			{
				m_bHasStartedMixing = true;
			}
			else if (bResult == false && m_bHasStartedMixing && !m_bEOF)
			{
				g_mutexBenchSkips.Lock();
				++g_nBenchSkips;
				g_mutexBenchSkips.Unlock();
			}
			else if (bResult && m_bHasStartedMixing && SamplesAvailableToRead() < 512 + PHONON_FRAME_SIZE)
			{
				float fRunway = Plat_FloatTime() - m_pJob->fCompleteTime;
				g_mutexBenchSkips.Lock();
				if (fRunway < g_fBenchRunway || g_fBenchRunway == -1.0f)
				{
					g_fBenchRunway = fRunway;
				}
				g_mutexBenchSkips.Unlock();
			}
		}

		TrySubmitJob();
		TryCompleteJob();

		return bResult;
	}
	virtual void UpdateLoopPosition(int nLoopPosition)
	{
		m_pData->UpdateLoopPosition(nLoopPosition);
	}

private:
	IWaveData* m_pData;
	hrtf_info_t* m_pDir;

	void TryCompleteJob();
	void TrySubmitJob();

	PhononJob *m_pJob;
	bool m_bJobPending;

	int m_nChannels;
	int m_nDataWidth;

	int64 m_nDataStreamReadPos;
	bool m_bEOF;
	bool m_bAsync;

	int64 m_readIndex;

	char m_outputBuf[PHONON_FRAME_SIZE * PHONON_SAMPLE_SIZE * 2];
	char* m_readPos;
	char* m_writePos;

	bool m_bHasStartedMixing;

	bool m_bBenchmarking;

	int SamplesAvailableToRead() const
	{
		if (m_readPos <= m_writePos)
		{
			return (m_writePos - m_readPos)/PHONON_SAMPLE_SIZE;
		}
		else
		{
			const char *end = m_outputBuf + sizeof(m_outputBuf);
			return ((end - m_readPos) + (m_writePos - m_outputBuf)) / PHONON_SAMPLE_SIZE;
		}
	}

	int SamplesAvailableToWrite() const
	{
		return sizeof(m_outputBuf) / PHONON_SAMPLE_SIZE - SamplesAvailableToRead();
	}

	CWaveDataHRTF(const CWaveDataHRTF&);
	void operator=(const CWaveDataHRTF&);
};

CWaveDataHRTF::CWaveDataHRTF(IWaveData* pData, hrtf_info_t* dir) : m_pData(pData), m_pDir(dir), m_bJobPending(false), m_nDataStreamReadPos(0), m_bEOF(false), m_bAsync(snd_hrtf_async.GetBool()), m_bHasStartedMixing(false), m_bBenchmarking(snd_hrtf_benchmark.GetBool())
{
	if ( m_bBenchmarking )
	{
		g_mutexBenchSkips.Lock();
		g_nBenchNumSounds++;
		if ( g_nBenchNumSounds > g_nBenchMaxNumSounds )
		{
			g_nBenchMaxNumSounds = g_nBenchNumSounds;
		}

		g_mutexBenchSkips.Unlock();
	}

	m_pJob = new PhononJob;
	memset(m_pJob, 0, sizeof(*m_pJob));

	m_readIndex = 0;
	m_readPos = m_writePos = m_outputBuf;

	if (pData->Source().IsStereoWav())
	{
		m_nChannels = 2;
	}
	else
	{
		m_nChannels = 1;
	}

	m_nDataWidth = 2;
}

CWaveDataHRTF::~CWaveDataHRTF()
{
	if ( m_bBenchmarking )
	{
		g_mutexBenchSkips.Lock();
		g_nBenchNumSounds--;
		g_mutexBenchSkips.Unlock();
	}

	if (m_bAsync)
	{
		g_phononDeleteQueue.PushItem(m_pJob);
		g_eventPhononThread.Set();
	}
	else
		delete m_pJob;

	delete m_pData;
}

int CWaveDataHRTF::ReadSourceData(void **pData, int64 sampleIndex, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE])
{
	if (snd_hrtf_passthrough.GetBool())
	{
		return m_pData->ReadSourceData(pData, sampleIndex, sampleCount, copyBuf);
	}

	if (m_bAsync)
	{
		TryCompleteJob();
	}
	else
	{
		int nLastAvail = -1;
		while (SamplesAvailableToRead() < sampleCount && SamplesAvailableToRead() > nLastAvail && SamplesAvailableToWrite() > PHONON_FRAME_SIZE && m_pData->IsReadyToMix())
		{
			nLastAvail = SamplesAvailableToRead();
			TrySubmitJob();
			TryCompleteJob();
		}
	}

	const int nAvail = SamplesAvailableToRead();
	if (nAvail == 0 && m_bEOF)
	{
		*pData = nullptr;
		return 0;
	}

	int nRead = sampleCount < nAvail ? sampleCount : nAvail;
	if (nAvail < sampleCount && !m_bEOF)
	{
		DevMsg("HRTF: Not enough data, wanted %d, got %d\n", sampleCount, nAvail);
		g_mutexBenchSkips.Lock();
		++g_nBenchSkips;
		g_mutexBenchSkips.Unlock();
	}

	if (nRead > AUDIOSOURCE_COPYBUF_SIZE / PHONON_SAMPLE_SIZE)
	{
		nRead = AUDIOSOURCE_COPYBUF_SIZE / PHONON_SAMPLE_SIZE;
	}

	if (m_readIndex != sampleIndex)
	{
		Warning("Read index mismatch sampleIndex: %d vs %d\n", (int)m_readIndex, (int)sampleIndex);
	}

	m_readIndex += nRead;

	if ((m_outputBuf + sizeof(m_outputBuf)) - m_readPos >= nRead * PHONON_SAMPLE_SIZE)
	{
		memcpy(copyBuf, m_readPos, nRead*PHONON_SAMPLE_SIZE);
		m_readPos += nRead * PHONON_SAMPLE_SIZE;
	}
	else
	{
		char* out = copyBuf;
		const int at_end = (m_outputBuf + sizeof(m_outputBuf)) - m_readPos;
		const int at_begin = nRead*PHONON_SAMPLE_SIZE - at_end;
		memcpy(out, m_readPos, at_end);
		out += at_end;
		m_readPos = m_outputBuf;

		memcpy(out, m_readPos, at_begin);
		m_readPos += at_begin;
	}

	*pData = (void*)copyBuf;

	TrySubmitJob();

	return nRead;
}

void CWaveDataHRTF::TryCompleteJob()
{
	if (!m_bAsync && m_bJobPending)
	{
		ProcessPhononJob(*m_pJob);
		m_pJob->bComplete = true;
	}

	if (!m_bJobPending )
	{
		return;
	}

	m_pJob->mutex.Lock();
	const bool bComplete = m_pJob->bComplete;
	m_pJob->mutex.Unlock();

	if (!bComplete)
	{
		return;
	}

	m_pJob->bComplete = false;
	m_bJobPending = false;
	memcpy(m_writePos, m_pJob->outputBuf, sizeof( m_pJob->outputBuf ) );
	m_writePos += sizeof( m_pJob->outputBuf );
	if (m_writePos == m_outputBuf + sizeof(m_outputBuf))
	{
		m_writePos = m_outputBuf;
	}
}

void CWaveDataHRTF::TrySubmitJob()
{
	TryCompleteJob();

	if (m_bJobPending || SamplesAvailableToWrite() <= PHONON_FRAME_SIZE || m_bEOF)
	{
		return;
	}

	if (!m_pData->IsReadyToMix())
	{
		return;
	}

	char copyBuf[AUDIOSOURCE_COPYBUF_SIZE];

	char* buf = nullptr;

	int nLoaded = m_pData->ReadSourceData((void**)&buf, m_nDataStreamReadPos, PHONON_FRAME_SIZE, copyBuf);

	m_nDataStreamReadPos += nLoaded;

	if (nLoaded <= 0)
	{
		m_bEOF = true;
		return;
	}

	const int nBytesLoaded = nLoaded * sizeof(short) * m_nChannels;
	if (nLoaded < PHONON_FRAME_SIZE)
	{
		
		memcpy(m_pJob->inputBuf, buf, nBytesLoaded);
		memset(m_pJob->inputBuf + nBytesLoaded, 0, sizeof(m_pJob->inputBuf) - nBytesLoaded);
		m_bEOF = true;
	}
	else if (nLoaded == PHONON_FRAME_SIZE)
	{
		memcpy(m_pJob->inputBuf, buf, nBytesLoaded);
	}
	else
	{
		DevMsg("HRTF: Unexpected too many bytes from ReadSourceData: %d vs %d\n", PHONON_FRAME_SIZE, nLoaded);
		return;
	}

	m_pJob->nChannels = m_nChannels;
	m_pJob->nDataWidth = m_nDataWidth;
	m_pJob->interpolation = snd_hrtf_lerp_method.GetInt() == 0 && m_pDir->bilinear_filtering == false ? IPL_HRTFINTERPOLATION_NEAREST : IPL_HRTFINTERPOLATION_BILINEAR;
	m_pJob->vec = m_pDir->vec;	
	m_pJob->lerp = m_pDir->lerp;
	m_pJob->bComplete = false;

	if (snd_hrtf_benchmark.GetBool())
	{
		m_pJob->fSubmitTime = Plat_FloatTime();
	}

	m_bJobPending = true;
	

	if (m_bAsync)
	{
		g_phononJobQueue.PushItem(m_pJob);
		g_eventPhononThread.Set();
	}
	else
	{
		TryCompleteJob();
	}
}

IWaveData *CreateWaveDataHRTF(IWaveData* pData, hrtf_info_t* dir)
{
	if (dir == nullptr)
		return pData;

	CWaveDataHRTF* res = new CWaveDataHRTF(pData, dir);
	return res;
}
