//========== Copyright © Valve Corporation, All rights reserved. ========
#ifndef VJOB_SPURS_UTILS_HDR
#define VJOB_SPURS_UTILS_HDR

#ifndef _PS3
#error "This is PS3 specific header"
#endif


#include "vjobs_interface.h"
#include "ps3/vjobutils_shared.h"

#ifdef VJOBS_ON_SPURS 
#include "tier0/logging.h"
#include "tier0/dbg.h"
#include "spu_printf.h"
#include "tier0/memalloc.h"
#include <cell/spurs.h>
#include <cell/spurs/system_workload.h>
using namespace ::cell::Spurs;

DECLARE_LOGGING_CHANNEL( LOG_VJOBS );

extern const char * HumanReadableCellResult( int nCellResult );
inline int CellVerify( int nResult, const char * pCmd, LoggingSeverity_t severity )
{
	Assert( nResult == CELL_OK );
	if( nResult != CELL_OK )
	{
		InternalMsg( LOG_VJOBS, severity, "Cell error %08X (%s) in\n\t%s\n", nResult, HumanReadableCellResult(nResult), pCmd );
	}
	return nResult;
}

inline int CellVerify2( int nResult, const char * pCmd, const char * pFile, const int nLine, LoggingSeverity_t severity )
{
	Assert( nResult == CELL_OK );
	if( nResult != CELL_OK )
	{
		InternalMsg( LOG_VJOBS, severity, "Cell error %08X (%s) in\n\t%s\n\t%s:%d", nResult, HumanReadableCellResult(nResult), pCmd, pFile, nLine );
	}
	return nResult;
}

#define CELL_VERIFY( CMD )	CellVerify2( CMD, #CMD, __FILE__, __LINE__, LS_ASSERT )
#define CELL_MUST_SUCCEED( CMD )	CellVerify2( CMD, #CMD, __FILE__, __LINE__, LS_ERROR )
#define CELL_MUST_SUCCEED2( CMD, DESC )	CellVerify2( CMD, DESC, __FILE__, __LINE__, LS_ERROR )


inline CellSpursJob64* NewJob64( const CellSpursJobHeader & jobHeader )
{
	CellSpursJob64 * pJob = (CellSpursJob64 *)MemAlloc_AllocAligned( sizeof(CellSpursJob64), sizeof(CellSpursJob64) );
	pJob->header = jobHeader;
	pJob->workArea.dmaList[0] = 0;
	pJob->workArea.dmaList[1] = 0;
	return pJob;
};



inline CellSpursJob128* NewJob128( const CellSpursJobHeader & jobHeader )
{
	CellSpursJob128 * pJob = (CellSpursJob128 *)MemAlloc_AllocAligned( sizeof(CellSpursJob128), sizeof(CellSpursJob128) );
	__dcbz( pJob );
	pJob->header = jobHeader;
	return pJob;
};

inline CellSpursJob256* NewJob256( const CellSpursJobHeader & jobHeader )
{
	CellSpursJob256 * pJob = (CellSpursJob256 *)MemAlloc_AllocAligned( sizeof(CellSpursJob256), sizeof(CellSpursJob256) );
	__dcbz( pJob );
	__dcbz( ((uint8*)pJob) + 128 );
	pJob->header = jobHeader;
	return pJob;
};

template <typename T>
inline void DeleteJob( T * pJob )
{
	MemAlloc_FreeAligned( pJob );
}


#endif // VJOBS_ON_SPURS 

DECLARE_LOGGING_CHANNEL(LOG_VJOBS);

#endif