//========== Copyright © Valve Corporation, All rights reserved. ========

#include "vjobs/pcring.h"
#include "vjobs/edgegeom_shared.h"


#ifdef SPU	

#if EDGEGEOMRING_DEBUG_TRACE
#include "vjobs/edgegeomparams_shared.h" // debug
namespace job_edgegeom{ extern JobParams_t * g_lsJobParams; }
#endif

void CEdgeGeomRing::Test()
{
	for( ;; )
	{
		cellDmaGetllar( this, m_eaThis, 0, 0 );
		//if( 0 == __builtin_expect( spu_readch( MFC_RdAtomicStat ), 0 ) )
		uint nStatusGetllar = cellDmaWaitAtomicStatus();(void)nStatusGetllar;
		#if EDGEGEOMRING_DEBUG_TRACE
		m_nUseCounter++;
		#endif

		cellDmaPutllc( this, m_eaThis, 0, 0 );
		uint nStatusPutllc = cellDmaWaitAtomicStatus();
		if( 0 == __builtin_expect( nStatusPutllc, 0 ) )
		{
			break; // succeeded
		}
		
		//VjobSpuLog("job_edgegeom Test failed(%d,%d)\n", nStatusGetllar, nStatusPutllc );
	}
}


struct ALIGN16 FifoSnapshot_t
{
	uint32 m_nSignal;
	uint32 m_nPut;
	uint32 m_nEnd;
	uint32 m_nRingIncarnation;	
	
	void Snapshot( CEdgeGeomRing * pRing )
	{
		m_nPut             = pRing->m_ibvbRing.m_nPut;
		m_nEnd             = pRing->m_ibvbRing.m_nEnd;
		m_nRingIncarnation = pRing->m_nRingIncarnation;
		// update the signal, since we're spinning
		m_nSignal = cellDmaGetUint32( uintp( pRing->m_eaIbvbRingLabel ), DMATAG_SYNC, 0, 0 );
	}
}
ALIGN16_POST;





uintp CEdgeGeomRing::Allocate( CellGcmContextData *pGcmCtx, uint nBytesUnaligned, uint nQueueTag )
{
	// allocate in aligned chunks to make it all aligned
	uint nBytesAligned = AlignValue( nBytesUnaligned, 32 );
	AssertSpuMsg( nBytesAligned <= EDGEGEOMRING_MAX_ALLOCATION, "job_edgegeom allocates %u > %u from edge", nBytesAligned, EDGEGEOMRING_MAX_ALLOCATION );
	
	uintp eaAllocation = 0;
	
	SysFifo::PreparePutEnum_t nResult = SysFifo::PUT_PREPARE_FAILED;
	uint nStatusGetllar, nStatusPutllc;
	uint nSpins = 0, nAtomicCollisionEvent = 0, nWaitRsxSpins = 0;
	uint nStoredSignal;						   
	
	uint nSpuFlag = 1 << VjobSpuId();
	
	union 
	{
		FifoSnapshot_t fields;
		__vector int vi4;
	}snapshot;
	snapshot.vi4 = (__vector int){-1,-1,-1,-1};
	uint32 nJobId = job_edgegeom::g_lsJobParams->m_nEdgeJobId;

	for(;; nSpins ++)
	{
		cellDmaGetllar( this, m_eaThis, 0, 0 );
		//if( 0 == __builtin_expect( spu_readch( MFC_RdAtomicStat ), 0 ) )
		nStatusGetllar = cellDmaWaitAtomicStatus();
		{
			// reservation succeeded
			Assert( m_ibvbRing.m_nPut != 0xFFFFFFFF );
			if( snapshot.fields.m_nPut == m_ibvbRing.m_nPut && snapshot.fields.m_nRingIncarnation == m_nRingIncarnation )
			{
				// the put didn't change, ring incarnation didn't change.
				// Therefore, nobody changed this object - between 
				// last getllar, getting signal and this getllar,
				// so it's atomic if we update the signal now.
				m_ibvbRing.NotifySignalSafe( snapshot.fields.m_nSignal );
			}

			nResult = m_ibvbRing.PreparePut( nBytesAligned );
			if( nResult != SysFifo::PUT_PREPARE_FAILED )
			{
				eaAllocation = m_ibvbRing.EaPut();
				m_ibvbRing.Put( nBytesAligned );
				nStoredSignal = m_ibvbRing.GetSignal();
				m_ibvbRingSignal[nQueueTag] = nStoredSignal;
				m_nAtomicCollisionSpins += nAtomicCollisionEvent;
				m_nRsxWaitSpins += nWaitRsxSpins;
				m_nUsedSpus |= nSpuFlag;
				
				if( ( ( signed int )( nJobId - m_nMaxJobId[nQueueTag] ) ) > 0 )
				{
					m_nMaxJobId[nQueueTag] = nJobId;
				}
				
				if( nResult == SysFifo::PUT_PREPARED_WRAPPED )
				{
					m_nRingIncarnation++; // we allocated, wrapping
				}
				
				#if EDGEGEOMRING_DEBUG_TRACE
				m_nUseCounter++;
				COMPILE_TIME_ASSERT( !( EDGEGEOMRING_DEBUG_TRACE & ( EDGEGEOMRING_DEBUG_TRACE - 1 ) ) );
				m_nNextDebugTrace = ( m_nNextDebugTrace + 1 ) & ( EDGEGEOMRING_DEBUG_TRACE - 1 );
				#endif

				cellDmaPutllc( this, m_eaThis, 0, 0 );
				nStatusPutllc = cellDmaWaitAtomicStatus();
				if( 0 == __builtin_expect( nStatusPutllc, 0 ) )
				{
					break; // succeeded
				}
			}
			else
			{
				nWaitRsxSpins ++;
			}

		}
		snapshot.fields.Snapshot( this );

		if( nSpins == 100000 && !IsCert() )
		{
			// VjobSpuLog( "job_edgegeom Allocate spinning: %d, %d, signal 0x%X;\n", nStatusGetllar, nStatusPutllc, nLastSeenSignal );
			// DebuggerBreak();
		}
	}
	if( nResult == SysFifo::PUT_PREPARED_WRAPPED )
	{
		// need to clear cache
		cellGcmSetInvalidateVertexCacheInline( pGcmCtx );
		//VjobSpuLog( "job_edgegeom Allocate wrapped ring, invalidated vertex cache\n" );
	}
	else
	{
		Assert( nResult == SysFifo::PUT_PREPARED_NOWRAP );
	}

	Assert( nStoredSignal == m_ibvbRing.GetSignal() );
	//VjobSpuLog( "alloc %X, signal %X, prev6 signal:%X, pcring put %x end %x\n", eaAllocation, nStoredSignal, m_ibvbRingSignal[(nTag-1)&3], m_ibvbRing.m_nPut, m_ibvbRing.m_nEnd );
	#if EDGEGEOMRING_DEBUG_TRACE

	if( m_eaDebugTrace && m_enableDebugTrace )
	{
		EdgeGeomDebugTrace_t trace;
		trace.m_nAllocResult  = (uint8)nResult;
		trace.m_nQueueTag     = (uint8)job_edgegeom::g_lsJobParams->m_nQueueTag;
		trace.m_nJobId        = job_edgegeom::g_lsJobParams->m_nEdgeJobId;
		trace.m_nPut          = m_ibvbRing.m_nPut;
		trace.m_nEnd          = m_ibvbRing.m_nEnd;
		trace.m_eaEdgeGeomJts = job_edgegeom::g_lsJobParams->m_eaEdgeGeomJts;
		for( uint i = 0; i < EDGEGEOMRING_JOBQUEUE_TAG_COUNT; ++i )
			trace.m_nTagSignal[i] = m_ibvbRingSignal[i];
		VjobDmaPutf( &trace, uintp( m_eaDebugTrace + m_nNextDebugTrace ), sizeof( trace ), VJOB_IOBUFFER_DMATAG, 0, 0 );
		VjobWaitTagStatusAll( 1 << VJOB_IOBUFFER_DMATAG );
	}
	#endif
	return eaAllocation;
}

#else


void CEdgeGeomRing::Init( void* eaBuffer, uint nBufferSize, uint nIoOffsetDelta, void * eaLocalBaseAddress, uint nLabel )
{
	COMPILE_TIME_ASSERT( sizeof( CEdgeGeomRing_Mutable ) <= 128 ); // we need to fit into 128 bytes so that atomics work
	m_ibvbRing.Init( (uintp)eaBuffer, nBufferSize );
	m_eaLocalBaseAddress = (uint) eaLocalBaseAddress;
	m_nIoOffsetDelta     = nIoOffsetDelta;
	m_nIbvbRingLabel = nLabel;
	m_eaIbvbRingLabel = cellGcmGetLabelAddress( nLabel );
	*m_eaIbvbRingLabel = m_ibvbRing.GetSignal();
	m_ibvbRingSignal[0] = m_ibvbRing.GetSignal();
	for( uint i = 0; i < EDGEGEOMRING_JOBQUEUE_TAG_COUNT; ++i )
	{	
		m_ibvbRingSignal[i] = m_ibvbRingSignal[0];
	}
	V_memset( m_nMaxJobId, 0xFF, sizeof( m_nMaxJobId ) );
	m_eaThis = (uint) this;
	m_nDebuggerBreakMask = 0;
	m_nAtomicCollisionSpins = 0;
	m_nRsxWaitSpins = 0;
	m_nRingIncarnation   = 0;

#if EDGEGEOMRING_DEBUG_TRACE
	m_nUseCounter        = 0;
	m_eaDebugTrace = NULL;
	m_eaDebugTrace = ( EdgeGeomDebugTrace_t* )MemAlloc_AllocAligned( sizeof( EdgeGeomDebugTrace_t ) * EDGEGEOMRING_DEBUG_TRACE, 16 * 16 * 16 );
	m_nNextDebugTrace = 0;
	m_enableDebugTrace = true;
#endif
}

void CEdgeGeomRing::Shutdown()
{
#if EDGEGEOMRING_DEBUG_TRACE
	MemAlloc_FreeAligned( m_eaDebugTrace );
#endif
}

void CEdgeGeomFeeder::Init( uint nIbvbRingSize )
{
	m_nJobQueueTag = 0;
	m_nSpawnedJobsWithTag = 0;
	m_nTotalEdgeGeomJobCounter = 0;
	m_nSpawnedJobsWithTagReserveAllocate = 0;
	m_nIbvbRingSize = nIbvbRingSize;
}


void CEdgeGeomRing::Test()
{
#if EDGEGEOMRING_DEBUG_TRACE
	m_nUseCounter++;
#endif
}

uintp CEdgeGeomRing::Allocate( CellGcmContextData *pGcmCtx, uint nBytesUnaligned, uint nQueueTag )
{
	// this is not an actively supported and tested code path! It only exists here for single-threaded PPU-on-SPU mode debugging. Bit rot possible!
	DebuggerBreak(); // this is not an actively supported and tested code path! It only exists here for single-threaded PPU-on-SPU mode debugging. Bit rot possible!
	Warning( "this is not an actively supported and tested code path! It only exists here for single-threaded PPU-on-SPU mode debugging. Bit rot possible\n" );
	
	// allocate in aligned chunks to make it all aligned
	uint nBytesAligned = AlignValue( nBytesUnaligned, 32 );
	AssertSpuMsg( nBytesAligned <= EDGEGEOMRING_MAX_ALLOCATION, "job_edgegeom allocates %u > %u from edge", nBytesAligned, EDGEGEOMRING_MAX_ALLOCATION );
	uint nLastSeenSignal = m_ibvbRing.GetInvalidSignal();
	uintp eaAllocation = 0;

	for(;;)
	{
		V_memcpy( this, (void*)m_eaThis, sizeof( *this ) );

		// emulate: reservation succeeded
		if( nLastSeenSignal != m_ibvbRing.GetInvalidSignal() )
		{
			m_ibvbRing.NotifySignal( nLastSeenSignal );
		}

		SysFifo::PreparePutEnum_t nResult = m_ibvbRing.PreparePut( nBytesAligned );
		if( nResult != SysFifo::PUT_PREPARE_FAILED )
		{
			if( nResult == SysFifo::PUT_PREPARED_WRAPPED )
			{
				// need to clear cache
				cellGcmSetInvalidateVertexCacheInline( pGcmCtx );
			}
			eaAllocation = m_ibvbRing.EaPut();
			m_ibvbRing.Put( nBytesAligned );
			m_ibvbRingSignal[nQueueTag] = m_ibvbRing.GetSignal();

			V_memcpy( (void*)m_eaThis, this, sizeof( *this ) );
			break; // succeeded
		}

		// update the signal, since we're spinning
		nLastSeenSignal = VjobDmaGetUint32( uintp( m_eaIbvbRingLabel ), DMATAG_SYNC, 0, 0 );
	}

	return eaAllocation;
}

#endif