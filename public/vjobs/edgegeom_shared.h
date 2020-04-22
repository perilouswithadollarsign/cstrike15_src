//========== Copyright © Valve Corporation, All rights reserved. ========
#if !defined( JOB_EDGE_GEOM_SHARED_HDR ) && defined( _PS3 )
#define JOB_EDGE_GEOM_SHARED_HDR


#include "ps3/spu_job_shared.h"
#include "vjobs/pcring.h"


#define EDGEGEOMRING_DEBUG_TRACE 128  // set to 0 to disable debug trace

// the max allocation, in bytes, made from job_edgegeom
#define EDGEGEOMRING_MAX_ALLOCATION ( 32 * 1024 )

#define EDGEGEOMRING_JOBQUEUE_TAG_COUNT 4

struct ALIGN16 EdgeGeomDebugTrace_t
{
	uint8 m_nAllocResult;
	uint8 m_nQueueTag;	
	uint16 m_nJobId;
	uint32 m_eaEdgeGeomJts;
	uint32 m_nPut;
	uint32 m_nEnd;
	uint32 m_nTagSignal[EDGEGEOMRING_JOBQUEUE_TAG_COUNT];
}ALIGN16_POST;

struct ALIGN16 CEdgeGeomRing_Mutable
{
	SysFifo m_ibvbRing; // edge geom index buffer/vertex buffer ring
	
	// WARNING. Although logically there may be any (2^x) number of queue tags for edgegeom jobs, 
	//          The SPURS must only see 0 and 1 (even and odd) tags. Even tags must synchronize with even,
	//          odd tags must synchronize with odd.
	uint32 m_ibvbRingSignal[EDGEGEOMRING_JOBQUEUE_TAG_COUNT];
	uint32 m_nMaxJobId[EDGEGEOMRING_JOBQUEUE_TAG_COUNT];
	
	uint m_nAtomicCollisionSpins;
	uint m_nRsxWaitSpins;
	uint m_nRingIncarnation;
	uint m_nUsedSpus;
	#if EDGEGEOMRING_DEBUG_TRACE
	uint m_nNextDebugTrace;
	#endif
}
ALIGN16_POST;

struct ALIGN128 CEdgeGeomRing: public CEdgeGeomRing_Mutable
{
	// immutable part	
	uint m_eaLocalBaseAddress;
	uint m_nIoOffsetDelta;
	uint m_nIbvbRingLabel;
	uint32 * m_eaIbvbRingLabel;
	uint m_eaThis;
	uint m_nDebuggerBreakMask;

	#if EDGEGEOMRING_DEBUG_TRACE
	uint m_nUseCounter;
	EdgeGeomDebugTrace_t *m_eaDebugTrace;
	bool m_enableDebugTrace;
	#endif
public:

	uintp Allocate( CellGcmContextData *pGcmCtx, uint nBytes, uint nTag );
	void Test();
#ifndef SPU	
	void Init( void* eaBuffer, uint nBufferSize, uint nIoOffsetDelta, void * eaLocalBaseAddress, uint nLabel );
	void Shutdown();
#endif
}
ALIGN128_POST;


enum EdgeGeomJobConstEnum_t
{
	EDGEGEOMJOB_SCRATCH_SIZE = 64 * 1024
};


struct CEdgeGeomFeeder
{
	uint m_nJobQueueTag; // the tag we're spawning jobs with, currently
	uint m_nSpawnedJobsWithTag; // number of jobs we spawned with the tag
	
	// max bytes that the jobs spawned with current tag can allocate; 
	// must not exceed 1/8 of the full ring buffer size because there may be 4 full tag switches between setting label
	uint m_nSpawnedJobsWithTagReserveAllocate; 
	uint m_nTotalEdgeGeomJobCounter;
	
	uint m_nIbvbRingSize;
	
	bool Tick( uint nReserve );
#ifndef SPU
	void Init( uint nIbvbRingSize );
#endif	
};



inline bool CEdgeGeomFeeder::Tick( uint nReserve )
{
	++m_nTotalEdgeGeomJobCounter;
	++m_nSpawnedJobsWithTag;
	m_nSpawnedJobsWithTagReserveAllocate += nReserve; // tentatively add the reserve to the same tag..
	//if( ++m_nSpawnedJobsWithTag > 6 )
	if( m_nSpawnedJobsWithTagReserveAllocate > m_nIbvbRingSize / 8 )
	{
		m_nJobQueueTag = ( m_nJobQueueTag + 1 ) & ( EDGEGEOMRING_JOBQUEUE_TAG_COUNT - 1 );
		m_nSpawnedJobsWithTag = 0;
		m_nSpawnedJobsWithTagReserveAllocate = nReserve;
		return true ;// !( m_nJobQueueTag & 1 ); // only insert labels in one tag, to make sure they insert in serial fashion
	}
	else
	{
		return false;
	}
}


namespace job_edgegeom
{
	enum FlagEnum_t
	{
		FLAG_SWITCH_JOBQUEUE_TAG = 0x80,
		FLAG_SKIP_VERTEX_CACHE_INVALIDATE = 0x40000000
	};
}

#endif
