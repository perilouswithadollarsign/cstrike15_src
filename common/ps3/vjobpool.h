//========== Copyright © Valve Corporation, All rights reserved. ========
#if !defined( VJOBPOOL_HDR ) && defined( _PS3 )
#define VJOBPOOL_HDR



#include "ps3/spu_job_shared.h"
#include "cell/atomic.h"
//#include "ps3/vjobutils.h"



template < typename Job, uint nAlignment = 128 >
class VjobPool
{
public:

#ifndef SPU
	void Init( uint nJobs ) // must be integer-power-of-2
	{
		m_nJobIndexMask = nJobs - 1;
		Assert( !( m_nJobIndexMask & nJobs ) );
		uint nSizeToAllocate = sizeof( Job ) * nJobs;
		m_eaPool = ( Job* )MemAlloc_AllocAligned( nSizeToAllocate, nAlignment );
		V_memset( m_eaPool, 0, nSizeToAllocate );
		m_nNextJob = 0;
		m_nWaitSpins = 0;
	}
	
	void Shutdown()
	{
		// wait for all jobs to finish
		for ( uint i = 0; i <= m_nJobIndexMask; ++i )
		{
			volatile const uint64 * pEa = ( const uint64* ) & m_eaPool[i];
			while ( *pEa )
			{
				continue;
			}
		}
		MemAlloc_FreeAligned( m_eaPool );
		m_eaPool = NULL;
	}
	Job * Alloc( const CellSpursJobHeader & header ) // the job header is freed when its eaBinary is overwritten with zeroes from within the job
	{
		Job * pJob = Alloc();
		V_memcpy( pJob, &header, sizeof( header ) );
		return pJob;
	}
#endif

	Job * Alloc( ); // the job header is freed when its eaBinary is overwritten with zeroes from within the job
	
	uint GetMarker()const { return m_nNextJob; }
	uint GetCapacity()const { return m_nJobIndexMask + 1; }
	int GetReserve( uint nMarker )const { return int( GetCapacity() - ( m_nNextJob - nMarker ) ); }

public:
	uint m_nWaitSpins; // debug field
protected:
	Job * m_eaPool;
	uint m_nJobIndexMask;

	// this is just a hint
	uint m_nNextJob;
};


template < typename Job, uint nAlignment >
Job * VjobPool<Job, nAlignment>::Alloc( ) // the job header is freed when its eaBinary is overwritten with zeroes from within the job
{
	uint nNextJob = m_nNextJob;
	Job * eaJob = NULL;
	for( ;; )
	{
		eaJob = &m_eaPool[ ( nNextJob & m_nJobIndexMask ) ];
	#ifdef SPU
		char ALIGN128 temp[128] ALIGN128_POST;
		// it's important to fill in and compare the first 64 bits, because that's where eaBinary is, and the first 32 bits (MSB) may be zero 
		if( 0 == cellAtomicCompareAndSwap64( (uint64*)temp, ( uintp )eaJob, 0, ~0ull ) )
		{
			break;
		}
		++m_nWaitSpins;
	#else
		// it's important to fill in and compare the first 64 bits, because that's where eaBinary is, and the first 32 bits (MSB) may be zero 
		if( 0 == cellAtomicCompareAndSwap64( ( uint64* )eaJob, 0, ~0ull ) )
		{
			break;
		}
		
		if( ( ( ( ++nNextJob ) ^ m_nNextJob ) & m_nJobIndexMask ) == 0 )
		{
			// we've made the full circle; yield
			m_nWaitSpins++;
			sys_timer_usleep( 30 );
		}
	#endif
	}
	// this is just a hint
	m_nNextJob = nNextJob + 1;
	
	return eaJob;
}


#endif
