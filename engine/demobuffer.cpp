//======= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======
//
// TODO:
//  - USE A MEMPOOL OF SOME SORT
//  - GET STALE FRAME REMOVAL WORKING
//  - MAKE THREAD SAFE - NEED TO BE ABLE TO WRITE A .DEM FILE ON A SEP THREAD AND
//		NOT BE THROWING AWAY ELEMENTS THAT ARE BEING READ.  NEED A FLAG IN DATACHUNK
//		FOR WHETHER AN INSTANCE IS "IN USE," IE BEING READ TO WRITE TO A DEM FILE.
//
//===============================================================================

#include "demobuffer.h"
#include "edict.h"
#include "host.h"
#include "tier1/mempool.h"
#include "vstdlib/jobthread.h"

#include "replayserver.h"	// TODO: Remove
#include "sv_client.h"
#ifndef DEDICATED
#include "cdll_int.h"
#include "client.h"
#endif

#include "tier0/memdbgon.h"		// NOTE: Must go last!

#ifndef DEDICATED
#if !defined( _DEBUG )
// These are the buffers defining how demo data is flushed to disk:
// We allocate 5 MB buffer (worth about 5 min of gameplay)
// once over 4 MB is used up we will commit the first 1 MB to disk
// This throttles disk IO, and ensures that the last 3 MB are always
// contained in memory and not committed to disk to prevent file peeking
// for cheating purposes during gameplay
#define DISK_DEMO_BUFFER_TOTAL_SIZE (5*1024*1024)
#define DISK_DEMO_BUFFER_FLUSH_CONSIDER_SIZE (4*1024*1024)
#define DISK_DEMO_BUFFER_FLUSH_TODISK_SIZE (1*1024*1024)
#else
// Smaller sizes in debug engine.dll build to hammer on the subsystems involved
#define DISK_DEMO_BUFFER_TOTAL_SIZE (5*20*1024)
#define DISK_DEMO_BUFFER_FLUSH_CONSIDER_SIZE (4*20*1024)
#define DISK_DEMO_BUFFER_FLUSH_TODISK_SIZE (1*20*1024)
#endif
#endif

//-----------------------------------------------------------------------------
// Specialty class with overrides for stream buffer
//-----------------------------------------------------------------------------
class CDiskDemoBuffer : public IDemoBuffer
{
public:
	CDiskDemoBuffer()
	:	m_pBuffer( NULL )
	{
		m_nDecodedOffset = -1;
	}

	~CDiskDemoBuffer()
	{
		m_pBuffer->Close();
		delete m_pBuffer;
	}

	virtual bool Init( DemoBufferInitParams_t const& params )
	{
		// Convert to proper type
		StreamDemoBufferInitParams_t const* pParams = dynamic_cast< StreamDemoBufferInitParams_t const* >( &params );		Assert( pParams );

		// Allocate buffer
		m_pBuffer = new CUtlStreamBuffer();
		if ( !m_pBuffer )
			return false;

#ifndef DEDICATED
		// Force a very large memory buffer on the clients, this prevents peeking into the demo stream
		m_pBuffer->EnsureCapacity( DISK_DEMO_BUFFER_TOTAL_SIZE );
#endif

		// Demo files are always little endian
		m_pBuffer->SetBigEndian( false );
		m_bufDecoded.SetBigEndian( false );

		// Open the file
//		m_pBuffer->Open( pParams->pFilename, pParams->pszPath, pParams->nFlags, pParams->nOpenFileFlags );	// For main integration...
		m_pBuffer->Open( pParams->pFilename, pParams->pszPath, pParams->nFlags );
		m_nDecodedOffset = -1;

		m_pPlaybackParams = NULL;
#ifndef DEDICATED
		extern IDemoPlayer *demoplayer;
		extern IBaseClientDLL *g_ClientDLL;
		if ( demoplayer && g_ClientDLL )
		{
			m_pPlaybackParams = demoplayer->GetDemoPlaybackParameters();
		}
#endif

		return IsInitialized();
	}

	virtual void NotifySignonComplete() {}

	virtual void WriteHeader( void const *pData, int nSize )
	{
		// Byteswap
		demoheader_t littleEndianHeader = *((demoheader_t*)pData);
		ByteSwap_demoheader_t( littleEndianHeader );

		// Goto file start
		SeekPut( true, 0 );

		// Write
		Put( pData, nSize );
	}

	virtual void				NotifyBeginFrame() {}
	virtual void				NotifyEndFrame() {}

	virtual void				PutChar( char c )						{ m_pBuffer->PutChar( c ); }
	virtual void				PutUnsignedChar( unsigned char uc )		{ m_pBuffer->PutUnsignedChar( uc ); }
	virtual void				PutInt( int i )							{ m_pBuffer->PutInt( i ); }

	virtual void				WriteTick( int nTick )					{ m_pBuffer->PutInt( nTick ); }

	virtual char				GetChar() OVERRIDE
	{
		COnTheFlyDemoBufferReadInfo readRequest( m_pPlaybackParams, m_pBuffer, &m_bufDecoded, &m_nDecodedOffset, sizeof( char ) );
		return readRequest.GetReadBuffer()->GetChar();
	}
	virtual unsigned char		GetUnsignedChar() OVERRIDE
	{
		COnTheFlyDemoBufferReadInfo readRequest( m_pPlaybackParams, m_pBuffer, &m_bufDecoded, &m_nDecodedOffset, sizeof( unsigned char ) );
		return readRequest.GetReadBuffer()->GetUnsignedChar();
	}
	virtual int					GetInt() OVERRIDE
	{
		COnTheFlyDemoBufferReadInfo readRequest( m_pPlaybackParams, m_pBuffer, &m_bufDecoded, &m_nDecodedOffset, sizeof( int ) );
		return readRequest.GetReadBuffer()->GetInt();
	}

	virtual void				Get( void* pMem, int size )	OVERRIDE
	{
		COnTheFlyDemoBufferReadInfo readRequest( m_pPlaybackParams, m_pBuffer, &m_bufDecoded, &m_nDecodedOffset, size );
		readRequest.GetReadBuffer()->Get( pMem, size );
	}
	virtual void				Put( const void* pMem, int size )
	{
		m_pBuffer->Put( pMem, size );

#ifndef DEDICATED
		if ( ( size > 0 ) && ( m_pBuffer->TellPut() > 0 ) &&
			( ( ( ( char* ) m_pBuffer->PeekPut() ) - ( ( char * ) m_pBuffer->Base() ) ) > (
				( GetBaseLocalClient().IsActive() && GetBaseLocalClient().ishltv ) ? 2048 : DISK_DEMO_BUFFER_FLUSH_CONSIDER_SIZE
				) ) )
		{	// Periodically try to flush the demo buffer to disk
			m_pBuffer->TryFlushToFile( DISK_DEMO_BUFFER_FLUSH_TODISK_SIZE );
		}
#endif
	}

	virtual bool				IsValid() const							{ return m_pBuffer && m_pBuffer->IsValid(); }
	virtual bool				IsInitialized() const					{ return IsValid() && m_pBuffer->IsOpen(); }

	inline CUtlBuffer::SeekType_t GetSeekType( bool bAbsolute )			{ return bAbsolute ? CUtlBuffer::SEEK_HEAD : CUtlBuffer::SEEK_CURRENT; }

	// Change where I'm writing (put)/reading (get)
	virtual void				SeekPut( bool bAbsolute, int offset )	{ m_pBuffer->SeekPut( GetSeekType( bAbsolute ), offset ); }
	virtual void				SeekGet( bool bAbsolute, int offset )	{ m_pBuffer->SeekGet( GetSeekType( bAbsolute ), offset ); }

	// Where am I writing (put)/reading (get)?
	virtual int					TellPut( ) const						{ return m_pBuffer->TellPut(); }
	virtual int					TellGet( ) const						{ return m_pBuffer->TellGet(); }

	virtual int					TellMaxPut( ) const						{ return m_pBuffer->TellMaxPut(); }

	virtual void				UpdateStartTick( int& nStartTick ) const {}
	virtual void				DumpToFile( char const* pFilename, const demoheader_t &header ) const {}

private:
	CUtlStreamBuffer *m_pBuffer;
	CUtlBuffer m_bufDecoded;
	int m_nDecodedOffset;
	CDemoPlaybackParameters_t const *m_pPlaybackParams;

	class COnTheFlyDemoBufferReadInfo
	{
	public:
		COnTheFlyDemoBufferReadInfo( CDemoPlaybackParameters_t const *pPlaybackParams, CUtlBuffer *pRawData, CUtlBuffer *pDecodeCache, int *pDecodedOffset, int numBytesRequired )
		{
			m_nReadFromBufferOriginalSeekPos = 0;
#ifndef DEDICATED
			if ( pPlaybackParams )
			{
				// Read from the nearest 16-byte aligned location
				int nOriginalGet = pRawData->TellGet();
				if ( ( (*pDecodedOffset) < 0 ) || // nothing decoded
					( nOriginalGet < (*pDecodedOffset) ) || // reading earlier
					( nOriginalGet + numBytesRequired > (*pDecodedOffset) + pDecodeCache->TellPut() ) ) // could read beyond decoded buffer
				{
					int nNearestAlignedLocation = nOriginalGet &~0xF;
					int blockRead = ( nOriginalGet + numBytesRequired - nNearestAlignedLocation + 0xF )&~0xF;
					blockRead = MAX( 1024, blockRead ); // decrypt chunks of 1K bytes at a time

					*pDecodedOffset = nNearestAlignedLocation;
					pDecodeCache->EnsureCapacity( blockRead );
					int numBytesSeekBack = nOriginalGet - nNearestAlignedLocation;
					if ( numBytesSeekBack )
						pRawData->SeekGet( pRawData->SEEK_CURRENT, - numBytesSeekBack ); // seek back

					int numBytes = MIN( blockRead, pRawData->TellMaxPut() - nNearestAlignedLocation );
					pRawData->Get( pDecodeCache->Base(), numBytes );
					m_nReadFromBufferOriginalSeekPos += -numBytes+numBytesSeekBack;
					pDecodeCache->SeekPut( pDecodeCache->SEEK_HEAD, numBytes );

					// Decode the chunk
					extern IBaseClientDLL *g_ClientDLL;
					g_ClientDLL->PrepareSignedEvidenceData( pDecodeCache->Base(), numBytes, pPlaybackParams );
				}

				int nSeekInDecodedBuffer = nOriginalGet - *pDecodedOffset;
				pDecodeCache->SeekGet( pDecodeCache->SEEK_HEAD, nSeekInDecodedBuffer );

				//
				// Set the read state
				//
				m_pReadFromBuffer = pDecodeCache;
				m_pSeekSyncBuffer = pRawData;
				m_nReadFromBufferOriginalSeekPos += -nSeekInDecodedBuffer;

				return;
			}
#endif

			//
			// Read raw state
			//
			m_pReadFromBuffer = pRawData;
			m_pSeekSyncBuffer = NULL;
		}
		~COnTheFlyDemoBufferReadInfo()
		{
			if ( m_pSeekSyncBuffer && m_pReadFromBuffer )
				m_pSeekSyncBuffer->SeekGet( m_pSeekSyncBuffer->SEEK_CURRENT, m_pReadFromBuffer->TellGet() + m_nReadFromBufferOriginalSeekPos );
		}

		CUtlBuffer * GetReadBuffer() const { return m_pReadFromBuffer; }
	private:
		CUtlBuffer *m_pReadFromBuffer;
		CUtlBuffer *m_pSeekSyncBuffer;
		int m_nReadFromBufferOriginalSeekPos;
	};
};


//-----------------------------------------------------------------------------
// Specialty class with overrides for stream buffer
//-----------------------------------------------------------------------------
#if defined( REPLAY_ENABLED )
class CMemoryDemoBuffer : public IDemoBuffer
{
private:
	static int const CACHE_SIZE	= 1024 * 512;

	uint8*	m_pDataCache;			// Data cache for temporary writing
	uint8*	m_pWrite;				// Current write position (based on m_pDataCache)
	int		m_nBufferSize;			// Total buffer size
	int		m_nMaxPut;				// What's the most I've ever written?
	int		m_nCurrentTickOffset;	// Offset (relative to m_pDataCache) of tick - needed so we can rewrite ticks before dumping to disk

	struct DataChunk_t
	{
		int		nCurrentTickOffset;
		int		nTickcount;
		int		nDeltaTickcount;
		int		nSize;
		uint8	pData[1];
	};

	inline DataChunk_t* AllocDataChunk( int nSize, int nCurrentTickOffset )
	{
		Assert( nCurrentTickOffset >= 0 );

		int nActualSize = sizeof(DataChunk_t) + nSize - 1;		Assert( nActualSize < CACHE_SIZE );

		DataChunk_t* pNewFrame = (DataChunk_t*)new uint8[ nActualSize ]; 
		pNewFrame->nSize = nSize;
		pNewFrame->nCurrentTickOffset = nCurrentTickOffset;
		pNewFrame->nTickcount = -1;

		// TODO: pass in the delta tick - get rid of #include "replay" etc.
		pNewFrame->nDeltaTickcount = ( replay && replay->m_MasterClient ) ? replay->m_MasterClient->m_nDeltaTick : -1;

		return pNewFrame;
	}

	bool m_bSignonComplete;

	DataChunk_t* m_pSignonData;

	typedef unsigned short Iterator_t;
	CUtlLinkedList< DataChunk_t*, Iterator_t > m_lstFrames;	// Represents a list of demo frames

	inline int GetTickCount()
	{
		extern CGlobalVars g_ServerGlobalVariables;
		return g_ServerGlobalVariables.tickcount;
	}

	void RemoveStaleFrames()
	{
		// Don't remove any frames in the midst of a write operation
		if ( m_nWriteCount > 0 )
			return;

		extern ConVar replay_movielength;

#ifdef _DEBUG
		int nNumFramesRemoved = 0;
#endif

		// Here we remove any frames that are beyond the length of the movie.
		Iterator_t i = m_lstFrames.Head();
		while ( i != m_lstFrames.InvalidIndex() )
		{
			if ( m_lstFrames[ i ]->nTickcount >= GetTickCount() - TIME_TO_TICKS( replay_movielength.GetInt() ) )
				break;

			m_lstFrames.Remove( i );
			i = m_lstFrames.Head();
#ifdef _DEBUG
			++nNumFramesRemoved;
#endif
		}

#ifdef _DEBUG
		if ( nNumFramesRemoved > 0 )
		{
			DevMsg( "Replay: Removed %d frames(s) from recording buffer.\n", nNumFramesRemoved );
		}
#endif
	}

public:
	CMemoryDemoBuffer()
	:	
		m_nBufferSize( 0 ),
		m_nMaxPut( 0 ),
		m_nCurrentTickOffset( -1 ),
		m_pWrite( NULL ),
		m_pSignonData( NULL ),
		m_bSignonComplete( false )
	{
	}

	~CMemoryDemoBuffer()
	{
		delete [] m_pDataCache;
		delete m_pSignonData;

		// Free all list entries
		m_lstFrames.PurgeAndDeleteElements();
	}

	virtual bool Init( DemoBufferInitParams_t const& params )
	{
		m_pDataCache = new uint8[ CACHE_SIZE ];
		m_pWrite = m_pDataCache;
		return true;
	}

	virtual bool IsInitialized() const	{ return m_pDataCache != NULL; }

	virtual bool IsValid() const		{ return IsInitialized(); }

	virtual void WriteHeader( const void *pData, int nSize )
	{
		// There is no need to write the header until we dump the file to disk.

		/*
		// NOTE: Byteswap happens in dump

		// The header gets written twice, once at demo start, and once at demo stop.
		// If this is the first time, just write to the beginning of the cache
		if ( !m_bSignonComplete )
		{
			Assert( m_pWrite == m_pDataCache );		// Make sure this is the first thing we're writing
			Put( pData, nSize );
		}
		else	// Otherwise, write to the beginning of the header
		{
			AssertValidWritePtr( m_pHeaderData, nSize );
			V_memcpy( m_pHeaderData, pData, nSize );
		}
		*/
	}

	virtual void NotifySignonComplete()
	{
		Assert( !m_pSignonData );

		// Compute size and allocate
		int nSize = m_pWrite - m_pDataCache;				Assert( nSize >= 0 );
		m_pSignonData = AllocDataChunk( nSize, -1 );

		// NOTE: No need to set m_pSignonData->nTickcount.

		// We're done with signon data, copy it over from the cache
		V_memcpy( m_pSignonData->pData, m_pDataCache, nSize );

		m_pWrite = NULL;
		m_bSignonComplete = true;
	}

	virtual void NotifyBeginFrame()
	{
		if ( !m_bSignonComplete )
			return;

		Assert( m_pWrite == 0 );
		m_pWrite = m_pDataCache;
	}

	virtual void NotifyEndFrame()
	{
		if ( !m_bSignonComplete )
			return;

 		RemoveStaleFrames();

		// Allocate a new data chunk
		int nSize = m_pWrite - m_pDataCache;				Assert( nSize >= 0 );
		DataChunk_t* pNewFrame = AllocDataChunk( nSize, m_nCurrentTickOffset );

		// Set the time
		pNewFrame->nTickcount = GetTickCount();

		// Copy data from cache to new frame
		V_memcpy( pNewFrame->pData, m_pDataCache, nSize );

		// Add new frame to list
		m_lstFrames.AddToTail( pNewFrame );

#ifdef _DEBUG
		m_pWrite = NULL;
#endif
	}

	// Change where I'm writing (put)/reading (get)
	virtual void SeekGet( bool bAbsolute, int offset )
	{
		// Don't call this.
		Assert( 0 );
	}

	virtual void SeekPut( bool bAbsolute, int nOffset )
	{
		// The only time this should get called is if we are about to write the header.
		Assert( bAbsolute && nOffset == 0 );
	}

	// Where am I writing (put)/reading (get)?
	virtual int					TellPut( ) const			{ return m_pWrite - m_pDataCache; }
	virtual int					TellGet( ) const			{ Assert( 0 ); return 0; }

	// What's the most I've ever written?
	virtual int					TellMaxPut( ) const	{ return m_nMaxPut; }

	// Get functions should never get called.
	virtual char				GetChar()					{ Assert( 0 ); return 0; }
	virtual unsigned char		GetUnsignedChar()			{ Assert( 0 ); return 0; }
	virtual int					GetInt()					{ Assert( 0 ); return 0; }
	virtual void				Get( void* pMem, int size )	{ Assert( 0 ); }

	virtual void PutChar( char c )							{ Put( &c, sizeof( c ) ); }
	virtual void PutUnsignedChar( unsigned char uc ) 		{ Put( &uc, sizeof( uc ) ); }
	virtual void PutInt( int i )							{ Put( &i, sizeof( i ) ); }
	virtual void Put( const void* pMem, int nSize )
	{
		Assert( m_pWrite - m_pDataCache + nSize < CACHE_SIZE );
		V_memcpy( m_pWrite, pMem, nSize );
		m_pWrite += nSize;
		m_nBufferSize += nSize;
		m_nMaxPut = MAX( m_nMaxPut, nSize );
	}

	virtual void WriteTick( int nTick )
	{
		// Cache the relative position of the tick in memory for the given frame
		m_nCurrentTickOffset = m_pWrite - m_pDataCache;

		// Write the tick
		PutInt( nTick );
	}

	//
	// For thread safety - this counter keeps us from removing stale frames while writing a .dem file.
	// The idea here is that if the counter is anything but zero we should not remove stale frames.
	// We increment before creating a new job, and decrement from within that job, at the end.  We 
	// pass an iterator as an argument into the job's constructor so we know where to stop iterating
	// across the frame list.
	//
	mutable CInterlockedIntT<int> m_nWriteCount;

	//
	// Threaded .dem file write
	//
	class CDemWriteJob : public CJob
	{
	public:
		CDemWriteJob( const CMemoryDemoBuffer *pDemobuffer, const char *pFilename, Iterator_t itTail, const demoheader_t &header )
			: m_pDemobuffer( pDemobuffer ), m_pFilename( pFilename ), m_itTail( itTail ), m_Header( header )
		{
			Assert( pFilename && pFilename[0] );
		}

		virtual JobStatus_t DoExecute()
		{
			// TODO: Does it make sense to return JOB_OK here even on failure?
			JobStatus_t nResult = JOB_OK;

			if ( m_pFilename && m_pFilename[0] != '\0' )
			{
				// Open the file
				CUtlStreamBuffer buf( m_pFilename, NULL );
				if ( !buf.IsOpen() )
				{
					Warning( "demobuffer: Failed to open file for writing, %s\n", m_pFilename );
				}
				else
				{
					// NOTE: We include the sync tick frame as part of our signon data, which makes the header signon length vary by 6 bytes
					// (2 chars and 1 int) from our own signon data size. 
					const int nTickSyncFrameSize = 6;
					Assert( m_pDemobuffer->m_pSignonData->nSize == m_Header.signonlength + nTickSyncFrameSize );

					// Compute adjusted time/ticks/frames, since we may have removed stale frames
					const CUtlLinkedList< CMemoryDemoBuffer::DataChunk_t*, CMemoryDemoBuffer::Iterator_t > &lstFrames = m_pDemobuffer->m_lstFrames;
					Iterator_t itHead = lstFrames.Head();
					Iterator_t itTail = lstFrames.Tail();

					Assert( itHead != lstFrames.InvalidIndex() );
					Assert( itTail != lstFrames.InvalidIndex() );

					const DataChunk_t *pHead = lstFrames.Element( itHead );
					const DataChunk_t *pTail = lstFrames.Element( itTail );

					demoheader_t littleEndianHeader = m_Header;
					littleEndianHeader.playback_time = TICKS_TO_TIME( pTail->nTickcount - pHead->nTickcount );
					littleEndianHeader.playback_ticks = pTail->nTickcount - pHead->nTickcount;
					littleEndianHeader.playback_frames = lstFrames.Count();

					// Byteswap
					ByteSwap_demoheader_t( littleEndianHeader );

					// Write header
					buf.Put( &littleEndianHeader, sizeof( littleEndianHeader ) );

					// Write signon data
					AssertValidReadPtr( m_pDemobuffer->m_pSignonData );
					buf.Put( m_pDemobuffer->m_pSignonData->pData, m_pDemobuffer->m_pSignonData->nSize );

#if 1
					Iterator_t itStart = m_pDemobuffer->m_lstFrames.Head();
#else
					// TEST: Skip the first one
					Iterator_t itStart = m_pDemobuffer->m_lstFrames.Next( m_lstFrames.Head() );
#endif

					// Get first recording tick (NOTE: not start global tick).  Recording ticks start at 0 but
					// when we remove stale frames the first recording tick becomes greater than zero.  We use
					// nStartTick here to shift all frame recording ticks down nStartTick ticks.
					int nStartTick;
					if ( itHead != m_pDemobuffer->m_lstFrames.InvalidIndex() )
					{
						DataChunk_t *pFrame = m_pDemobuffer->m_lstFrames[ itHead ];
						int *pTickData = reinterpret_cast< int * >( pFrame->pData + pFrame->nCurrentTickOffset );
						V_memcpy( &nStartTick, pTickData, sizeof( int ) );
					}

					// Write frames
					for ( Iterator_t i = itStart; i != m_pDemobuffer->m_lstFrames.InvalidIndex(); i = m_pDemobuffer->m_lstFrames.Next( i ) )
					{
						DataChunk_t *pFrame = m_pDemobuffer->m_lstFrames[ i ];			Assert( pFrame->nSize >= 0 );

						// Overwrite the tick for the given frame if one exists
						if ( nStartTick > 0 && pFrame->nCurrentTickOffset >= 0 )
						{
							int nNewTick;
							int *pTickData = reinterpret_cast< int * >( pFrame->pData + pFrame->nCurrentTickOffset );
							
							// Copy tick from buffer to nNewTick
							V_memcpy( &nNewTick, pTickData, sizeof( int ) );

							// Subtract out start tick
							nNewTick -= nStartTick;

							// Copy back to buffer
							V_memcpy( pTickData, &nNewTick, sizeof( int ) );
						}

						buf.Put( pFrame->pData, pFrame->nSize );
					}

					// Write dem_stop cmd
					buf.PutUnsignedChar( dem_stop );
					buf.PutInt( m_pDemobuffer->m_lstFrames.Element( m_pDemobuffer->m_lstFrames.Tail() )->nTickcount );
					buf.PutChar( 0 );

					buf.Close();
				}
			}

			// Decrement the write counter
			m_pDemobuffer->m_nWriteCount--;

			return nResult;
		}

	private:
		const CMemoryDemoBuffer	*m_pDemobuffer;
		const char				*m_pFilename;
		Iterator_t				m_itTail;
		const demoheader_t		&m_Header;
	};

	virtual void UpdateStartTick( int& nStartTick ) const
	{
		Iterator_t itHead = m_lstFrames.Head();
		if ( itHead == m_lstFrames.InvalidIndex() )
			return;

		nStartTick = m_lstFrames[ itHead ]->nTickcount;
	}

	virtual void DumpToFile( const char *pFilename, const demoheader_t &header ) const
	{
		Assert( !IsX360() );	// TODO: Not supporting 360 yet.  Need alternate thread pool setup to do so.

		// HACK:
		int n = m_nWriteCount;

		// Critical section
		m_nWriteCount++;

		// Start a new thread
		CDemWriteJob* pJob = new CDemWriteJob( this, pFilename, m_lstFrames.Tail(), header );
		g_pThreadPool->AddJob( pJob );

		while ( m_nWriteCount > n )
			DevMsg( "Waiting for file dump to complete\n" );
	}
};
#endif

//-----------------------------------------------------------------------------
// Factory function
//-----------------------------------------------------------------------------
IDemoBuffer *CreateDemoBuffer( bool bMemoryBuffer, const DemoBufferInitParams_t& params )
{
	IDemoBuffer *pRet;
#if defined( REPLAY_ENABLED )
	if ( bMemoryBuffer )
	{
		pRet = static_cast< IDemoBuffer* >( new CMemoryDemoBuffer() );
	}
	else
#endif
	{
		pRet = static_cast< IDemoBuffer* >( new CDiskDemoBuffer() );
	}

	if ( !pRet->Init( params ) )
	{
		delete pRet;
		return NULL;
	}

	return pRet;
}
