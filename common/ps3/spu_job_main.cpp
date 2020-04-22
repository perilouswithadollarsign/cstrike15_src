//========= Copyright © 1996-2004, Valve LLC, All rights reserved. ============
// 
// This common file serves as redirection to make minimal SPU-only preparation
// of a job and call into its Main function. The Main function is hidden behind
// the unique namespace, just like all global symbols, so that multiple similar
// jobs can easily compile into and link with the vjobs.prx module that can call
// them on PPU either for debugging, for fallback case, or for main processing 
// on Xbox360
//

//#include <cell/spurs/job_chain.h>
#include <cell/spurs/job_queue.h>
#include <cell/spurs/job_context.h>
#include <spu_printf.h>

#ifdef USE_LSGUARD
#include <cell/lsguard.h>
#endif

#ifndef VJOB
#error "Please define VJOB to the project name in SPU job project. This will isolate it from other jobs when they compile into the common elf, prx or dll"
#endif

namespace VJOB
{
	extern void Main( CellSpursJobContext2* stInfo, CellSpursJob256 *job );
}

CellSpursJobContext2* g_stInfo = 0;
uint32_t g_InterlockedBuffer[32] __attribute__((aligned(128)));

#ifdef VJOB_JOBCHAIN_JOB
// JobChain job: the symbol is "job"

extern "C"
void cellSpursJobMain2(CellSpursJobContext2* stInfo, CellSpursJob256 *job)
{
	extern CellSpursJobContext2* g_stInfo;
	g_stInfo = stInfo;
	VJOB::Main( stInfo, job );
}
#else
// JobQueue job: the symbol is "jqjob"
void cellSpursJobQueueMain(
						   CellSpursJobContext2 *pContext,
						   CellSpursJob256 *pJob
						   )
{
	extern CellSpursJobContext2* g_stInfo;
	g_stInfo = pContext;
	VJOB::Main( pContext, pJob );
}

#endif

void CheckBufferOverflow_Impl()
{
	uint16_t nCause;
	int nResult;
	nResult = cellSpursJobMemoryCheckTest( &nCause );
	if ( nResult != CELL_OK )
	{
		spu_printf( "cellSpursJobMemoryCheckTest() failed = %08X\n", nResult );
		__asm volatile ("stopd $0,$0,$0");
	}

#ifdef USE_LSGUARD
	nResult = cellLsGuardCheckCorruption();
	if ( nResult != CELL_OK )
	{
		spu_printf( "cellLsGuardCheckCorruption() failed = %08X\n", nResult );
		__asm volatile ("stopd $0,$0,$0");
		cellLsGuardRehash();		// We rehash to detect the next corruption
	}
#endif
}

void CheckDmaGet_Impl( const void * pBuffer, size_t nSize )
{
#ifdef USE_LSGUARD
	int nResult;
	nResult = cellLsGuardCheckWriteAccess( pBuffer, nSize );
	if ( nResult != CELL_OK )
	{
		spu_printf( "cellLsGuardCheckWriteAccess() failed = %08X\n", nResult );
		spu_printf( "Address: %08X  -  Size: %d\n", (int)pBuffer, (int)nSize );
		__asm volatile ("stopd $0,$0,$0");
	}
#endif
}
