//========== Copyright © Valve Corporation, All rights reserved. ========
#include "ps3/spu_job_shared.h"

uint g_nBreakMask = 0;


void* AlignBuffer( void * pBuffer, uint nBytes )
{
	if( !( uintp( pBuffer ) & 15 ) )
	{
		return pBuffer;
	}
	
	Assert( nBytes < 232*1024 ); // sanity check
	
	vector int *pBegin = ( vector int * )( pBuffer ), *pEnd = ( vector int* )( uintp( pBuffer ) + nBytes );
	vector int vLast = *pBegin;
	vector int *pLast = pBegin;
	
	vector unsigned char vShuf = vec_lvsl( 0, (uint8*)pBuffer );
	while( pLast < pEnd )
	{
		vector int * pNext = pLast + 1;
		vector int vNext = *pNext;
		*pLast = vec_perm( vLast, vNext, vShuf );
		
		pLast = pNext;
		vLast = vNext;
	}
	
	return ( void* )( uintp( pBuffer ) & -16 );
}

//
// Adds constant nAdd to the given unaligned buffer of uint16's
// 
void UnalignedBufferAddU16( uint16 * pBuffer, uint nCount, uint16 nAdd )
{
#ifdef SPU
	if( nCount )
	{
		uint16 *pBufferEnd = pBuffer + nCount;
		vector unsigned short vuAdd = vec_splat_u16( nAdd );
		vector unsigned short vuLeft = spu_rlmaskqwbyte( vuAdd, -( 0xF & int( pBuffer ) ) );
		vector unsigned short vuRight = spu_slqwbyte( vuAdd, 0xF & -int( pBufferEnd ) );
		vector unsigned short * pLeft = ( vector unsigned short * )( uintp( pBuffer ) & -16 ), * pRight = ( vector unsigned short* )( uintp( pBufferEnd - 1 ) & -16 );
		if( pLeft == pRight )
		{
			*pLeft = vec_add( *pLeft, vec_and( vuLeft, vuRight ) );
		}
		else
		{
			*pLeft = vec_add( *pLeft, vuLeft );
			*pRight = vec_add( *pRight, vuRight );
			for( vector unsigned short * p = pLeft + 1; p < pRight; ++p )
			{
				*p = vec_add( *p, vuAdd );
			}
		}
	}
#else
	for( uint i = 0; i < nCount; ++i )
	{
		pBuffer[i] += nAdd;
	}
#endif
}


void TestUnalignedBufferAddU16( )
{
	uint16 ALIGN16 test[8 * 6] ALIGN16_POST;
	for( uint l = 0; l <= 8; ++l )
	{
		for( uint e = l; e < ARRAYSIZE( test ); ++e )
		{
			V_memset( test, 0, sizeof( test ) );
			UnalignedBufferAddU16( test + l, e - l, e+1 );
			for( uint t = 0; t < l; ++ t )
				Assert( test[t] == 0 );
			for( uint t = l; t < e; ++t )
				Assert( test[t] == e+1 );
			for( uint t = e; t < ARRAYSIZE( test ); ++t )
				Assert( test[t] == 0 );
		}
	}
}

#ifndef SPU

void TestAlignBuffer()
{
	for( uint i = 0; i < 16; ++i )
	{
		uint8 ALIGN16 test[16 * 10] ALIGN16_POST;
		for( uint j = i; j < sizeof( test ); ++j )
			test[j] = uint8( j - i );
		uint8 * pBeginTest = (uint8*)AlignBuffer( test + i, sizeof( test ) - 16 );
		Assert( pBeginTest == test );
		for( uint j = 0; j < sizeof( test ) - 16; ++j )
			Assert( test[j] == uint8( j ) );
	}
}



CellSpursJobContext2* g_stInfo = NULL;

static void SyncDmaListTransfer( void * pDmaList, uint nDmaListSize, void * pTarget, uint nTargetMaxSize )
{
	Assert( !( nDmaListSize & 7 ) && !( uintp( pDmaList ) & 0xF ) );
	//uintp dmaTarget = ( uintp ) pTarget, dmaTargetEnd = dmaTarget + nTargetMaxSize;

	CellSpursJobInputList * pInputDmaList = ( CellSpursJobInputList* )pDmaList, *pInputDmaListEnd = ( CellSpursJobInputList * )( uintp( pDmaList ) + nDmaListSize );

	uintp lsDmaTarget = ( uintp ) pTarget, lsDmaTargetEnd = lsDmaTarget + nTargetMaxSize;
	for ( CellSpursJobInputList * pDmaElement = pInputDmaList; pDmaElement < pInputDmaListEnd; pDmaElement++ )
	{
		Assert( pDmaElement->asInputList.size <= 16 * 1024 ); // max size of a DMA element
		uintp lsDmaEnd = lsDmaTarget + pDmaElement->asInputList.size;
		Assert( lsDmaEnd <= lsDmaTargetEnd );
		V_memcpy( ( void* )lsDmaTarget, ( const void* ) pDmaElement->asInputList.eal, pDmaElement->asInputList.size );
		lsDmaTarget = AlignValue( lsDmaEnd, 16 ); // for small transfers, we must stalign every transfer by 16
	}
}


void VjobPushJob( void ( *pfnMain )( CellSpursJobContext2 * stInfo, CellSpursJob256 * job ), CellSpursJob128 * job )
{
	CellSpursJobContext2 info;
	V_memset( &info, 0, sizeof( info ) );
	void * ioBuffer = MemAlloc_AllocAligned( job->header.sizeInOrInOut, 16 );
	info.ioBuffer = ioBuffer;
	info.eaJobDescriptor = ( uintp ) job;

	CellSpursJob256 jobCopy;
	V_memcpy( &jobCopy, job, sizeof( *job ) );

	SyncDmaListTransfer( job->workArea.dmaList, job->header.sizeDmaList, ioBuffer, job->header.sizeInOrInOut );

	g_stInfo = &info;
	pfnMain( &info, ( CellSpursJob256* ) job );
	g_stInfo = NULL;

	MemAlloc_FreeAligned( ioBuffer );
}


void VjobSpuLog( const char * p, ... )
{
	va_list args;
	va_start( args, p );
	char szBuffer[2048];
	V_vsnprintf( szBuffer, sizeof( szBuffer ), p, args );
	Msg( "SPU-on-PPU: %s\n", szBuffer );
	va_end( args );
}



#define Check(b) if(!(b))DebuggerBreak();


void VjobDmaPut(
    const void * ls,
    uint64_t ea,
    uint32_t size,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
)
{
	Check( !( size & 0xF )  && size <= 16 * 1024 );
	Check( !( ea & 0xF ) && !( uintp( ls ) & 0xF ) );
	V_memcpy( ( void* )( uintp )ea, ls, size );
}


void VjobDmaLargePut(
    const void * ls,
    uint64_t ea,
    uint32_t size,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
)
{
	Check( !( size & 0xF ) && size <= 240 * 1024 );
	Check( !( ea & 0xF ) && !( uintp( ls ) & 0xF ) );
	V_memcpy( ( void* )( uintp )ea, ls, size );
}

void VjobDmaLargePutf(
    const void * ls,
    uint64_t ea,
    uint32_t size,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
)
{
	VjobDmaLargePut( ls, ea, size, tag, tid, rid );
}

void VjobDmaUnalignedPutf(
	const void *ls,
	uint64_t ea,
	uint32_t size,
	uint32_t tag,
	uint32_t tid,
	uint32_t rid
)
{
	Assert( 0 == ( 0xF & ( uintp( ls ) ^ ea ) ) );
	V_memcpy( (void*)(uintp)ea, ls, size );
}


void VjobDmaUnalignedPut(
						  const void *ls,
						  uint64_t ea,
						  uint32_t size,
						  uint32_t tag,
						  uint32_t tid,
						  uint32_t rid
						  )
{
	Assert( 0 == ( 0xF & ( uintp( ls ) ^ ea ) ) );
	V_memcpy( (void*)(uintp)ea, ls, size );
}


void VjobDmaLargePutb(
    const void * ls,
    uint64_t ea,
    uint32_t size,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
)
{
	VjobDmaLargePut( ls, ea, size, tag, tid, rid );
}


void VjobDmaPutf(
    const void * ls,
    uint64_t ea,
    uint32_t size,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
)
{
	Check( !( size & 0xF )  && size <= 16 * 1024 );
	Check( !( ea & 0xF ) && !( uintp( ls ) & 0xF ) );
	V_memcpy( ( void* )( uintp )ea, ls, size );
}


void VjobDmaSmallPut(
    const void * ls,
    uint64_t ea,
    uint32_t size,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
)
{
	Check( !( size & ( size - 1 ) ) );
	Check( !( 0xF & ( ea ^ uintp( ls ) ) ) );
	if ( size == 4 )
	{
		// special case to handle atomically, because we may use this to write RSX registers
		*( uint32* )( uintp )ea = *( uint32* )ls;
	}
	else
	{
		V_memcpy( ( void* )( uintp )ea, ls, size );
	}
}


void VjobDmaGet(
    void * ls,
    uint64_t ea,
    uint32_t size,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
)
{
	Check( !( size & ( size - 1 ) ) );
	Check( !( 0xF & ( ea | uintp( ls ) ) ) );
	if ( size == 4 )
	{
		// special case to handle atomically, because we may use this to read RSX registers
		*( uint32* )ls = *( uint32* )( uintp )ea;
	}
	else
	{
		V_memcpy( ls, ( const void* )( uintp )ea, size );
	}
}

void VjobDmaGetf(
   void * ls,
   uint64_t ea,
   uint32_t size,
   uint32_t tag,
   uint32_t tid,
   uint32_t rid
   )
{
	VjobDmaGet( ls, ea, size, tag, tid, rid );
}

// NOTE: implementation must wait for tag
uint32_t VjobDmaGetUint32(
    uint64_t ea,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
)
{
	return * ( volatile uint32 * )( uintp )ea;
}

void VjobDmaPutUint32(
	uint32_t value,
    uint64_t ea,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
)
{
	( * ( volatile uint32 * )( uintp )ea ) = value;
}

uint64_t VjobDmaGetUint64(
	uint64_t ea,
	uint32_t tag,
	uint32_t tid,
	uint32_t rid
)
{
	return *( volatile uint64 * )( uintp )ea;
}



void VjobDmaPutUint64(
	uint64_t value,
	uint64_t ea,
	uint32_t tag,
	uint32_t tid,
	uint32_t rid
)
{
	( * ( volatile uint64 * )( uintp )ea ) = value;
}


void VjobDmaListGet(
    void * ls,
    uint64_t ea,
    const CellDmaListElement * list,
    uint32_t listSize,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
)
{
	Check( !( listSize % 8 ) );
	uint8 * pLsTarget = ( uint8* )ls;
	for ( uint i = 0; i < listSize / 8; ++i )
	{
		uint64 nSize = list[i].size;
		VjobDmaGet( pLsTarget, ea + list[i].eal, ( uint32 )nSize, tag, tid, rid );
	}
}

void VjobDmaSmallGet(
    void * ls,
    uint64_t ea,
    uint32_t size,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
)
{
	Check( !( size & ( size - 1 ) ) );
	Check( !( 0xF & ( ea ^ uintp( ls ) ) ) );
	V_memcpy( ls, ( const void* )( uintp )ea, size );
}





void VjobDmaSmallPutf(
    const void * ls,
    uint64_t ea,
    uint32_t size,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
)
{
	Check( !( size & ( size - 1 ) ) );
	Check( !( 0xF & ( ea ^ uintp( ls ) ) ) );
	V_memcpy( ( void* )( uintp )ea, ls, size );
}

void VjobDmaSmallPutb(
    const void * ls,
    uint64_t ea,
    uint32_t size,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
)
{
	Check( !( size & ( size - 1 ) ) );
	Check( !( 0xF & ( ea ^ uintp( ls ) ) ) );
	V_memcpy( ( void* )( uintp )ea, ls, size );
}

void VjobPpuRereadEA( uintp ea )
{
	__lwsync();
	int eaContent = *( volatile int * ) ea;
	__lwsync();
}
#endif