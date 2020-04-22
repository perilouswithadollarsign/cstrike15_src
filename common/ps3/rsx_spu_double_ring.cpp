//================ Copyright (c) 1996-2010 Valve Corporation. All Rights Reserved. =================
//
// Double ring buffer used for bidirectional communication between RSX and SPU
// One ring buffer is (externally managed) jobchain(s) that call into entries in IO address space
// that RSX patches (changing from JTS to RET). The other ring buffer is supposedly in local memory
// and is supposedly split into segments. RSX consumes the local memory buffer, and releases it 
// segment-by-segment. SPU runs ahead of it and produces the segments and notifies the RSX 
// using JTS external to the classes here
//
//
#include <cell/spurs.h>
#include "ps3/dxabstract_gcm_shared.h"
#include "ps3/rsx_spu_double_ring.h"
#include "ps3/vjobchain4.h"
#include "vjobs/pcring.h"
#include "ps3/ps3gcmlabels.h"

// Can be LWSYNC or NOP if followed by RET
// Must be RET otherwise
// JTS->LWSYNC mutation allows for only 4-byte inline transfer from RSX, guaranteeing atomicity
#define MUTABLE_GUARD_COMMAND  CELL_SPURS_JOB_COMMAND_LWSYNC 

#ifndef	SPU
void RsxSpuDoubleRing::SetIoBuffer( void * pIoBuffer, uint nIoBufferByteSize )
{
	m_pIoBuffer = ( IoBufferEntry_t * )pIoBuffer;
	m_nIoBufferNextIndex = 0;
	m_nIoBufferCount = nIoBufferByteSize / sizeof( *m_pIoBuffer );
	Assert( !( m_nIoBufferCount & ( m_nIoBufferCount - 1 ) ) );

	// Don't initialize if IO buffer is being measured
	if ( !m_pIoBuffer )
		return;
	// init all to RET, which means it's released and most of them ready to be reused
	for( int i = 0; i < m_nIoBufferCount; ++i )
	{
		m_pIoBuffer[i].m_nMutableGuard = MUTABLE_GUARD_COMMAND;
		m_pIoBuffer[i].m_nConstRet     = CELL_SPURS_JOB_COMMAND_RET;
	}
}


void RsxSpuDoubleRing::OnGcmInit( uint nIoBufferOffsetDelta )
{
	m_nIoBufferOffsetDelta = nIoBufferOffsetDelta;
}


void RsxSpuDoubleRing::SetRsxBuffer( void * eaRsxBuffer, uint nRsxBufferSize, uint nIdealSegmentSize, uint nMaxJobsPerSegment )
{
	if( nIdealSegmentSize & ( nIdealSegmentSize - 1 ) )
	{
		Error( "RsxSpuDoubleRing: invalid ideal segment size %d, must be a power of 2\n", nIdealSegmentSize );
	}
	if( nIdealSegmentSize > nRsxBufferSize / 2 )
	{
		Error( "RsxSpuDoubleRing: invalid ideal segment size %d (full buffer size %d), must be at most half the buffer size", nIdealSegmentSize, nRsxBufferSize );
	}

	m_nMaxSegmentsPerRing = nRsxBufferSize / MIN( nIdealSegmentSize, nMaxJobsPerSegment * 128 );

	if( m_nIoBufferCount < /*ARRAYSIZE( m_pIoBaseGuards )*/4 * m_nMaxSegmentsPerRing ) // + 1 for the initial slot
	{
		Error( "RsxSpuDoubleRing: IO buffer is too small: there may be up to %d segments per ring, and there are only %d IO guard (JTS-RET) elements. Make IO buffer at least %u bytes large.\n", m_nMaxSegmentsPerRing, m_nIoBufferCount, 4 * m_nMaxSegmentsPerRing * sizeof( *m_pIoBuffer ) );
	}

	m_nIdealSegmentSize = nIdealSegmentSize;
	m_nMaxJobsPerSegment = nMaxJobsPerSegment;


	m_eaRsxBuffer = ( uintp )eaRsxBuffer;
	m_eaRsxBufferEnd = m_eaRsxBuffer + nRsxBufferSize;

	m_nIoBufferNextIndex = 0;
	m_nRingRsxNextSegment = 0; // consider the rsx ring already done

	// nothing is allocated by SPU 
	m_eaRingSpuBase = m_eaRsxBufferEnd;
	// we consider that the last segment was signaled beyond the end of this segment
	m_eaRingSpuLastSegment = m_eaRsxBufferEnd;

	m_nRingSpuJobCount = 0;	

	// this segment is for reference to the bottom of rsx buffer only
	// the whole RSX buffer is free for SPU to use
	m_eaRingRsxBase = m_eaRsxBuffer; 

	m_ringSpu.EnsureCapacity( m_nMaxSegmentsPerRing );
	m_ringRsx.EnsureCapacity( m_nMaxSegmentsPerRing );
}
#endif


void RsxSpuDoubleRing::InternalGuardAndLock( VjobChain4 * pSyncChain, uintp eaRsxMem )
{
	if( m_nRingRsxNextSegment >= m_ringRsx.Count() )
	{
		// if we exhausted all RSX ring segments, it only may mean that m_eaRingRsxBase == m_eaRsxBuffer, so this can not happen
		VjobSpuLog(
			"RsxSpuDoubleRing::InternalGuardAndLock: Unexpected error in RSX-SPU double ring, something's very wrong\n"
			"Please tell Sergiy the following numbers: %d,%d,%d,%d. @%X,@%X\n",
			m_nRingRsxNextSegment, m_ringRsx.Count(), m_ringSpu.Count(), m_ringSpu.GetCapacity(),
			m_eaRingRsxBase, m_eaRingSpuBase
		);
	}

	// the next most common case when we have to wait for RSX: we don't have to switch the ring because there's plenty of space still available
	// find the next segment to wait for ( may skip several segments )
	
	Assert( m_nRingRsxNextSegment < m_ringRsx.Count() );
	Segment_t segment;
	
	if( m_nRingRsxNextSegment >= m_ringRsx.Count() || m_ringSpu.Count() >= m_ringSpu.GetCapacity() )
	{
		VjobSpuLog(
			"RsxSpuDoubleRing::InternalGuardAndLock() hit an error condition, but will try to continue\n"
			"Please tell Sergiy the following numbers: %d>=%d|%d>=%d. @%X,@%X\n",
			m_nRingRsxNextSegment, m_ringRsx.Count(), m_ringSpu.Count(), m_ringSpu.GetCapacity(),
			m_eaRingRsxBase, m_eaRingSpuBase
		);
	}
	
	for( ; ; )
	{
		segment = m_ringRsx[m_nRingRsxNextSegment++];
		Assert( segment.m_eaBase < m_eaRingRsxBase );
		if( eaRsxMem >= segment.m_eaBase )
		{
			break; // we found the segment to wait on
		}
		if( m_nRingRsxNextSegment >= m_ringRsx.Count() )
		{
			// we exhausted all segments in the ring, so wait for the last segment and assume that'll be the end of this ring
			segment.m_eaBase = m_eaRsxBuffer;
			break;
		}
	}

	// we either found the segment to wait on here, or exhausted all segments from the RSX ring.
	// even if we exhausted all segments, it still means we found the LAST segment and we'll use that segment as the guard

	uint64 * eaCall = pSyncChain->Push( ); // wait for the RSX to finish rendering from this memory before writing into it
	VjobDmaPutfUint64( CELL_SPURS_JOB_COMMAND_CALL( segment.m_pSpuJts ), (uint32)eaCall, VJOB_IOBUFFER_DMATAG );

	m_eaRingSpuBase = eaRsxMem;
	m_eaRingRsxBase = segment.m_eaBase;
}


// Important side effects: may add to m_ringSpu
void RsxSpuDoubleRing::InternalSwitchRing( VjobChain4 * pSyncChain )
{
	// if we haven't already, we need to wait for the segment 0 to avoid racing over it with SPU (to ensure serialization)
	if( m_nRingRsxNextSegment < m_ringRsx.Count() )
	{
		// this should be a very rare occurence, because we don't normally jump across multiple segments; usually we have many allocations in a single segment
		uint64 * eaCall = pSyncChain->Push( );
		VjobDmaPutfUint64( CELL_SPURS_JOB_COMMAND_CALL( m_ringRsx.Tail().m_pSpuJts ), (uint32)eaCall, VJOB_IOBUFFER_DMATAG );
	}

	if( m_eaRingSpuBase < m_eaRingSpuLastSegment )
	{
		// since the last segment was created, there were allocations. Create a new segment to sync up to those allocations
		Assert( m_eaRsxBuffer <= m_eaRingSpuBase );
		m_eaRingSpuBase = m_eaRsxBuffer;
		CommitSpuSegment( );
	}
	else
	{
		// since the last segment was created, there were NO allocations. Extend the last segment to include the slack we're dropping now
		m_ringSpu.Tail().m_eaBase = m_eaRsxBuffer;
	}

	// now we switch the ring  : SPU ring becomes RSX ring, RSX ring retires
	//m_ringRsx.RemoveAll();
	//m_ringRsx.Swap( m_ringSpu );
	m_ringRsx.Assign( m_ringSpu );
	m_ringSpu.RemoveAll();
	
	AssertSpuMsg( m_ringRsx.Count() >= 2, "RSX ring has only %d segments! Something is very wrong with RSX-SPU double-ring\n", m_ringRsx.Count() );
	Assert( m_ringRsx.Count() < m_nMaxSegmentsPerRing );
/*
	for( uint i = ARRAYSIZE( m_pIoBaseGuards ); i--> 1;  ) // range: ARRAYSIZE( m_pIoBaseGuards ) - 1 ... 1
	{
		m_pIoBaseGuards[i] = m_pIoBaseGuards[i - 1];
	}
	m_pIoBaseGuards[0] = m_ringRsx.Tail().m_pSpuJts;
*/
	m_eaRingSpuBase        = m_eaRsxBufferEnd;
	m_eaRingSpuLastSegment = m_eaRsxBufferEnd;
	m_eaRingRsxBase        = m_eaRsxBufferEnd;
	m_nRingSpuJobCount     = 0;
	m_nRingRsxNextSegment  = 0;

	// IMPORTANT RSX L2 CACHE INVALIDATION POINT
	// we've run out of a ring; start a new one, invalidate the texture cache because we're using it for fragment programs and
	// the new ring will reuse the same memory which can be in RSX L2 cache, which doesn't invalidate when we DMA the new content into the new ring
	GCM_FUNC( cellGcmSetInvalidateTextureCache, CELL_GCM_INVALIDATE_TEXTURE );
}



inline void WaitGuard( volatile uint64 *pGuard, uint64 nValueToWaitFor )
{
	int nAttempts = 0;
	
	while( VjobDmaGetUint64( (uint)pGuard, DMATAG_SYNC, 0, 0 ) != nValueToWaitFor )
	{
		if( 100 == nAttempts++ )
		{
			VjobSpuLog( "Stall in WaitGuard : probably not enough IO buffer memory for the SPU side ring\n" );
		}
	}
/*
	if( *pGuard != nValueToWaitFor )
	{
		g_nWaitGuardSpins++;
		extern bool g_bEnableStallWarnings;
		if( g_bEnableStallWarnings )
		{
			Warning( "Stall in WaitGuard : probably not enough IO buffer memory for the SPU side ring\n" );
		}
		while( *pGuard != nValueToWaitFor )
		{
			g_nWaitGuardSpins++;
			sys_timer_usleep( 60 );
		}
	}
*/
}


// creates a new segment in SPU ring, allocates a JTS-RET guard for it, and pushes GCM command to release it
// Assumption: the memory in eaBase and up has already been used up by RSX commands up to this point
// Important side effects: adds to m_ringSpu
void RsxSpuDoubleRing::CommitSpuSegment(  )
{
	// check that RSX ran away at least 2 segments ahead; this guarantees that there are no SPU jobs waiting to be unblocked by any IoBuffer guards
	volatile uint64 * pIoBaseGuard = &( m_pIoBuffer[( m_nIoBufferNextIndex + m_nMaxSegmentsPerRing * 3 ) & ( m_nIoBufferCount - 1 )].m_nMutableGuard ); //m_pIoBaseGuards[ ARRAYSIZE( m_pIoBaseGuards ) - 1 ];
	WaitGuard( pIoBaseGuard, MUTABLE_GUARD_COMMAND );
	
	uint64 * eaJtsRetGuard = &( m_pIoBuffer[ ( m_nIoBufferNextIndex++ ) & ( m_nIoBufferCount - 1 ) ].m_nMutableGuard );
	Assert( VjobDmaGetUint64( ( uint )eaJtsRetGuard, DMATAG_SYNC, 0, 0 ) == MUTABLE_GUARD_COMMAND );
	VjobDmaPutfUint64( CELL_SPURS_JOB_COMMAND_JTS, (uint)eaJtsRetGuard, VJOB_IOBUFFER_DMATAG );

	m_ringSpu.AddToTail( Segment_t( m_eaRingSpuBase, eaJtsRetGuard ) );
	
	// Signal from RSX to SPU that RSX is done with this segment of local buffer and will go ahead and render using the next shader
		
	void * lsCmdBufferData = NULL;
	
	GCM_CTX_RESERVE( 2 + 4 + 10 + 2 + 4 ); // don't let callback insert anything between the following commands
	//GCM_FUNC( cellGcmSetWriteBackEndLabel, GCM_LABEL_DEBUG0, uintp( eaJtsRetGuard ) );
	GCM_FUNC( cellGcmSetTransferLocation, CELL_GCM_LOCATION_MAIN );
	GCM_FUNC( cellGcmSetInlineTransferPointer, uintp( eaJtsRetGuard ) + m_nIoBufferOffsetDelta, 2, &lsCmdBufferData );
	
	// CELL_SPURS_JOB_OPCODE_RET             (7|(14 << 3))  
	
	( ( uint32* )lsCmdBufferData )[0] = ( uint32( uint64( MUTABLE_GUARD_COMMAND ) >> 32 ) ); // uint64 to avoid any compiler issues
	( ( uint32* )lsCmdBufferData )[1] = ( uint32(         MUTABLE_GUARD_COMMAND         ) );

	GCM_FUNC( cellGcmSetWriteTextureLabel, GCM_LABEL_DEBUG_FPCP_RING, uintp( eaJtsRetGuard ) );
	
	m_eaRingSpuLastSegment = m_eaRingSpuBase;
	
	m_nRingSpuJobCount = 0;
}






