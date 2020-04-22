//========= Copyright © 1996-2004, Valve LLC, All rights reserved. ============
//
// This is the common include file to be included in SPU jobs.
// It takes care to remap/emulate some SPU-specific functinality on PPU
//
#ifndef PS3_SPU_JOB_SHARED_HDR
#define PS3_SPU_JOB_SHARED_HDR

#ifdef _PS3

#include <ps3/ps3_platform.h>
#include <cell/spurs/job_chain.h>
#include <cell/spurs/job_queue.h>
#include <cell/spurs/job_queue_port2.h>
#include <cell/dma/types.h>

// 
// NOTE: Enable the following block for debugging GCM on SPU; works as of SDK 350
//
#if 0 && defined( __SPU__ )
#include <cell/gcm/gcm_macros.h>

#undef CELL_GCM_ASSERT
#undef CELL_GCM_ASSERTS
#define CELL_GCM_ASSERT(condition)  Assert( condition )
#define CELL_GCM_ASSERTS(condition, description) AssertSpuMsg( condition, description )
#define CELL_GCM_ASSERT_ENABLE
#endif

enum DmaTagEnum_t
{
	DMATAG_SYNC = 2, // used for synchronous transfers, where we need the transfer to finish very soon/immediately after issuing
	DMATAG_TEXTURES = 3,
	DMATAG_SHADERS = 4,
	DMATAG_SCRATCH = 5, // used for DMA PUTs from Scratch memory, so we need to wait for this to finish before job finishes
	
	// each jobchain needs 2 dma tags, up to tag 30
//	DMATAG_EDGE_JOBCHAIN = 8,
//	DMATAG_FPCP_JOBCHAIN = 10,
//	DMATAG_GCM_JOBCHAIN = 12,

	DMATAG_ANIM				= 8,	// non immediate dma's
	DMATAG_BUILDINDICES		= 8,   
	DMATAG_BUILDRENDERABLES	= 8,


}; // shouldn't overlap with the tags used by the workload

// Enable this define to disable assert. This may be necessary to detect timing issues in DEBUG and RELEASE,
// or incorrectly generated code from compiler. When LSGUARD is enabled, we disable asserts to force potential issues.
#ifdef USE_LSGUARD
# define DISABLE_ASSERT
#endif

template <typename T>
inline T* AddBytes( T* p, int nBytes )
{
	return ( T* )( int( p ) + nBytes );
}

template <typename T>
inline T Min( T a, T b )
{
	return a < b ? a : b;
}

template <typename T>
inline T Max( T a, T b )
{
	return a > b ? a : b;
}

template <typename T>
inline void Swap( T& a , T & b )
{
	T c = a; a = b; b = c;
}


// <sergiy> should I port platform.h to SPU?
#ifdef SPU
#include <cell/spurs/job_context.h>
#include "cell/spurs/common.h"
#include <cell/atomic.h>
#include <spu_intrinsics.h>
#include <vmx2spu.h>

#define PPU_ONLY(X) 
#define SPU_ONLY(X)	X
#define vector __vector

void CheckBufferOverflow_Impl();
void CheckDmaGet_Impl( const void * pBuffer, size_t nSize );

#if defined(_CERT) || defined(DISABLE_ASSERT)
# define VjobSpuLog(...)
# define DebuggerBreak()
# define Warning(...)
# define CheckBufferOverflow()
# define CheckDmaGet(p, size)
#else
# include <spu_printf.h>
# define VjobSpuLog( MSG, ... ) spu_printf( "[%d]" MSG, cellSpursGetCurrentSpuId(), ##__VA_ARGS__ )
# define Msg( MSG, ... ) spu_printf( "[%d]" MSG, cellSpursGetCurrentSpuId(), ##__VA_ARGS__ )
#ifndef BASETYPES_H
	#define DebuggerBreak() __asm volatile ("stopd $0,$0,$0")
#endif
# define Warning( MSG, ... ) spu_printf( "[%d] Warning: " MSG, cellSpursGetCurrentSpuId(), ##__VA_ARGS__ )
# define CELL_DMA_ASSERT_VERBOSE
# define CheckBufferOverflow()	CheckBufferOverflow_Impl()
# define CheckDmaGet(p, size)	CheckDmaGet_Impl( p, size )
#endif

#define LWSYNC_PPU_ONLY()

#define VJOB_IOBUFFER_DMATAG g_stInfo->dmaTag // fake DMA tag

#include <cell/spurs/common.h>

#define VjobDmaPut cellDmaPut
#define VjobDmaGet cellDmaGet
#define VjobDmaGetf cellDmaGetf
#define VjobDmaListGet cellDmaListGet
#define VjobDmaLargePut cellDmaLargePut
#define VjobDmaLargePutf cellDmaLargePutf
//#define VjobDmaLargePutb cellDmaLargePutb
#define VjobDmaPutf cellDmaPutf
#define VjobDmaSmallPut cellDmaSmallPut
#define VjobDmaSmallPutf cellDmaSmallPutf
//#define VjobDmaSmallPutb cellDmaSmallPutb
#define VjobDmaSmallGet cellDmaSmallGet
#define VjobWaitTagStatusAll cellDmaWaitTagStatusAll
#define VjobWaitTagStatusImmediate cellDmaWaitTagStatusImmediate
#define VjobDmaGetUint32 cellDmaGetUint32
#define VjobDmaPutUint32 cellDmaPutUint32
#define VjobDmaGetUint64 cellDmaGetUint64
#define VjobDmaPutUint64 cellDmaPutUint64
#define VjobDmaUnalignedPutf cellDmaUnalignedPutf
#define VjobDmaUnalignedPut cellDmaUnalignedPut

#define VjobDmaPutfUintTemplate(SIZE, value, ea, tag, tid, rid)																				\
	do {																																		\
	uint64_t __cellDma_ea  = ea;																											\
	uint32_t __cellDma_tag = tag;																											\
	qword _buf = (qword)spu_splats(value);																									\
	cellDmaDataAssert(__cellDma_ea,sizeof(uint##SIZE##_t),__cellDma_tag);																	\
	cellDmaAndWait(cellDmaEa2Ls(__cellDma_ea,&_buf),__cellDma_ea,sizeof(uint##SIZE##_t),__cellDma_tag,MFC_CMD_WORD(tid,rid,MFC_PUTF_CMD));	\
	} while(0)

#define VjobDmaPutfUint8(value, ea, tag) cellDmaPutUintTemplate(8, ((uint8_t)value), ea, tag, 0, 0)
#define VjobDmaPutfUint16(value, ea, tag) cellDmaPutUintTemplate(16, ((uint16_t)value), ea, tag, 0, 0)
#define VjobDmaPutfUint32(value, ea, tag) cellDmaPutUintTemplate(32, ((uint32_t)value), ea, tag, 0, 0)
#define VjobDmaPutfUint64(value, ea, tag) cellDmaPutUintTemplate(64, ((uint64_t)value), ea, tag, 0, 0)



#define VjobSpuId() int( cellSpursGetCurrentSpuId() )

#define V_memset __builtin_memset
#define V_memcpy __builtin_memcpy

#if !defined ARRAYSIZE
	#define ARRAYSIZE( ARRAY ) ( sizeof( ARRAY ) / sizeof( ( ARRAY )[0] ) )
#endif

typedef signed int int32;
typedef unsigned int uint;
typedef signed char int8;
typedef unsigned char uint8;
typedef signed short int16;
typedef unsigned short uint16;
typedef signed int int32;
typedef unsigned int uint32;
typedef signed long long int64;
typedef unsigned long long uint64;
typedef unsigned int uintp;

typedef vector float fltx4 ;

#define INT_MAX	0x7fffffff

#define DECL_ALIGN(x)			__attribute__( ( aligned( x ) ) )



#ifndef BASETYPES_H

#define ALIGN16 DECL_ALIGN(16)
#define ALIGN16_POST
#define ALIGN128 DECL_ALIGN(128)
#define ALIGN128_POST
template <typename T>
inline T AlignValue( T val, uintp alignment )
{
	return ( T )( ( ( uintp )val + alignment - 1 ) & ~( alignment - 1 ) );
}

#define ALIGN_VALUE( val, alignment ) ( ( val + alignment - 1 ) & ~( alignment - 1 ) ) 

inline bool IsPowerOfTwo( uint x )
{
	return ( x & ( x - 1 ) ) == 0;
}

#endif

#define  FORCEINLINE inline /* __attribute__ ((always_inline)) */

#define IsPlatformPS3()		1
#define IsPlatformPS3_PPU()	0
#define IsPlatformPS3_SPU()	1
#define IsPlatformX360()	0
#define IsPlatformOSX()		0

#if !defined RESTRICT
	#define RESTRICT
#endif

#define V_memset			__builtin_memset
#define V_memcpy			__builtin_memcpy

inline void VjobPpuRereadEA( uintp ea ){}

#if defined(_CERT) || defined(DISABLE_ASSERT)

#define Assert(x) ((void)(0))
#define AssertSpuMsg(x,MSG,...)((void)0)
#ifndef DBG_H
	#define COMPILE_TIME_ASSERT( pred )	// to avoid any unpredictable affects in the optimizer
#endif

#else

#define DBGFLAG_ASSERT
#ifndef DBG_H
	#define Assert(x) do{if( !( x ) ) { spu_printf( "Assert on SPU[%d](" #x ")\n", cellSpursGetCurrentSpuId() ); DebuggerBreak(); } }while(0)
#endif
#define AssertSpuMsg(x,MSG,...) do{if( !( x ) ) { spu_printf( "Assert on SPU[%d](" #x "), " MSG, cellSpursGetCurrentSpuId(), ## __VA_ARGS__ ); DebuggerBreak(); } }while(0)
#ifndef DBG_H
	#define COMPILE_TIME_ASSERT( pred )	switch(0){case 0:case pred:;}
#endif
#endif

// mimic the PPU class on SPU
// template< int bytesAlignment, class T >
// class CAlignedNewDelete : public T
// {public:
// }

// WARNING: SLOWNESS. DO NOT USE IN PRODUCTION.
inline void DebugMemcpyEa( uint eaDest, uint eaSrc, uint nSize, void *lsScratch )
{
	Assert( ! ( 0xF & ( eaSrc | eaDest | nSize ) ) );
	uint nBytesLeft = nSize, nOffset = 0;
	while( nBytesLeft )
	{
		uint nChunk = Min<uint>( 16 * 1024, nBytesLeft );
		VjobDmaGet( lsScratch, eaSrc + nOffset, nChunk, DMATAG_SYNC, 0, 0 );
		VjobWaitTagStatusAll( 1 << DMATAG_SYNC );
		VjobDmaPut( lsScratch, eaDest + nOffset, nChunk, DMATAG_SYNC, 0, 0 );
		VjobWaitTagStatusAll( 1 << DMATAG_SYNC );
		nBytesLeft -= nChunk;
		nOffset += nChunk;
	}
}

#define vec_to_uint32(X) si_to_uint( ( qword )( X ) ) 


#define VjobQueuePort2PushJob( eaPort, eaJob, sizeDesc, tag, dmaTag, flag ) cellSpursJobQueuePort2PushJob( (uintp)( eaPort ), (uintp)( eaJob ), ( sizeDesc ), ( tag ), ( dmaTag ), ( flag ) )
#define VjobQueuePort2PushSync( eaPort2, tagMask, dmaTag, flag ) cellSpursJobQueuePort2PushSync( ( uintp ) ( eaPort2), ( tagMask ), ( dmaTag ), ( flag ) )


inline void VjobQueuePort2PushJobBlocking( CellSpursJobQueuePort2 *eaPort2, CellSpursJobHeader *eaJob, size_t sizeDesc, uint nQueueTag, uint nDmaTag )
{
	int nError;

	for(;;)
	{
		nError = cellSpursJobQueuePort2PushJob( uintp( eaPort2 ), uintp( eaJob ) , sizeDesc, nQueueTag, nDmaTag, CELL_SPURS_JOBQUEUE_FLAG_NON_BLOCKING );
		if( nError != CELL_SPURS_JOB_ERROR_AGAIN )
		{
			break;
		}
	}
	if ( nError != CELL_OK )
	{
		VjobSpuLog( "Cannot push job, error %d. RSX is going to hang, then SPUs, then PPU.\n", nError );
		DebuggerBreak();
	}
}



inline void VjobQueuePort2PushSyncBlocking( CellSpursJobQueuePort2 *eaPort2, unsigned tagMask, uint nDmaTag )
{
	int nError;

	for(;;)
	{
		nError = cellSpursJobQueuePort2PushSync( uintp( eaPort2 ), tagMask, nDmaTag, CELL_SPURS_JOBQUEUE_FLAG_NON_BLOCKING );
		if( nError != CELL_SPURS_JOB_ERROR_AGAIN )
		{
			break;
		}
	}
	if ( nError != CELL_OK )
	{
		VjobSpuLog( "Cannot push job, error %d. RSX is going to hang, then SPUs, then PPU.\n", nError );
		DebuggerBreak();
	}
}

#else

#include "tier0/platform.h"
#include "tier1/strtools.h"
#include "mathlib/ssemath.h"
#include <altivec.h>
#include <cell/spurs/job_context_types.h>

inline uint32_t GetCurrentSpuId()
{
	return 0xFFFFFFFF;
}
using namespace ::cell::Spurs;
extern void VjobSpuLog( const char * p, ... );

#define VJOB_IOBUFFER_DMATAG 0 // fake DMA tag

#define PPU_ONLY(X) X
#define SPU_ONLY(X)	


#ifdef _DEBUG
#define AssertSpuMsg(x,MSG,...) do { if( !( x ) ) { Warning( "Assert(" #x "), " MSG, ## __VA_ARGS__ ); DebuggerBreak(); } }while( 0 )
#else
#define AssertSpuMsg(x,MSG,...)
#endif


#define VjobQueuePort2PushJob( eaPort, eaJob, sizeDesc, tag, dmaTag, flag ) cellSpursJobQueuePort2PushJob( (CellSpursJobQueuePort2 *)( eaPort ), (CellSpursJobHeader *)( eaJob ), ( sizeDesc ), ( tag ), ( flag ) )
#define VjobQueuePort2PushSync( eaPort2, tagMask, dmaTag, flag ) cellSpursJobQueuePort2PushSync( (CellSpursJobQueuePort2 *) ( eaPort2), ( tagMask ), ( flag ) )

inline void VjobQueuePort2PushJobBlocking( CellSpursJobQueuePort2 *eaPort2, CellSpursJobHeader *eaJob, size_t sizeDesc, uint nQueueTag, uint nDmaTag )
{
	int nError = cellSpursJobQueuePort2PushJob( eaPort2, eaJob, sizeDesc, nQueueTag, 0 );// synchronous call
	(void) nError;
	Assert( nError == CELL_OK );
}

inline void VjobQueuePort2PushSyncBlocking( CellSpursJobQueuePort2 *eaPort2, unsigned tagMask, uint nDmaTag )
{
	int nError = cellSpursJobQueuePort2PushSync( eaPort2, tagMask, 0 ); // synchronous call
	(void) nError;
	Assert( nError == CELL_OK );
}


#define VjobSpuId() -1

#define LWSYNC_PPU_ONLY() __lwsync()

extern void VjobDmaPut(
    const void * ls,
    uint64_t ea,
    uint32_t size,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
);

extern void VjobDmaGet(
	void * ls,
	uint64_t ea,
	uint32_t size,
	uint32_t tag,
	uint32_t tid,
	uint32_t rid
);

extern void VjobDmaGetf(
	void * ls,
	uint64_t ea,
	uint32_t size,
	uint32_t tag,
	uint32_t tid,
	uint32_t rid
);

extern void VjobDmaListGet(
	void *ls,
	uint64_t ea,
	const CellDmaListElement *list,
	uint32_t listSize,
	uint32_t tag,
	uint32_t tid,
	uint32_t rid
);


extern void VjobDmaLargePut(
    const void * ls,
    uint64_t ea,
    uint32_t size,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
);

extern void VjobDmaLargePutf(
    const void * ls,
    uint64_t ea,
    uint32_t size,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
);

extern void VjobDmaLargePutb(
    const void * ls,
    uint64_t ea,
    uint32_t size,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
);

extern void VjobDmaPutf(
   const void * ls,
   uint64_t ea,
   uint32_t size,
   uint32_t tag,
   uint32_t tid,
   uint32_t rid
);

extern void VjobDmaSmallPut(
    const void * ls,
    uint64_t ea,
    uint32_t size,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
);

extern void VjobDmaSmallGet(
	void * ls,
	uint64_t ea,
	uint32_t size,
	uint32_t tag,
	uint32_t tid,
	uint32_t rid
);


extern void VjobDmaSmallPutb(
    const void * ls,
    uint64_t ea,
    uint32_t size,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
);

extern void VjobDmaSmallPutf(
    const void * ls,
    uint64_t ea,
    uint32_t size,
    uint32_t tag,
    uint32_t tid,
    uint32_t rid
);


// NOTE: implementation must wait for tag 
uint32_t VjobDmaGetUint32(
	uint64_t ea,
	uint32_t tag,
	uint32_t tid,
	uint32_t rid
);

void VjobDmaPutUint32(
	uint32_t value,
	uint64_t ea,
	uint32_t tag,
	uint32_t tid,
	uint32_t rid
);

uint64_t VjobDmaGetUint64(
	uint64_t ea,
	uint32_t tag,
	uint32_t tid,
	uint32_t rid
);

void VjobDmaPutUint64(
	uint64_t value,
	uint64_t ea,
	uint32_t tag,
	uint32_t tid,
	uint32_t rid
);

void VjobDmaUnalignedPutf(
	const void *ls,
	uint64_t ea,
	uint32_t size,
	uint32_t tag,
	uint32_t tid,
	uint32_t rid
);

void VjobDmaUnalignedPut(
	const void *ls,
	uint64_t ea,
	uint32_t size,
	uint32_t tag,
	uint32_t tid,
	uint32_t rid
);


// These functions are empty because I'm too lazy to implement deferred DMA emulation ...
inline uint VjobWaitTagStatusAll( uint nTagMask ){ return nTagMask;}
inline uint VjobWaitTagStatusImmediate( uint nTagMask ) { return nTagMask ; }


#define VjobDmaPutfUint8(value, ea, tag) *(uint8*)ea = (uint8)value
#define VjobDmaPutfUint16(value, ea, tag) *(uint16*)ea = (uint16)value
#define VjobDmaPutfUint32(value, ea, tag) *(uint32*)ea = (uint32)value
#define VjobDmaPutfUint64(value, ea, tag) *(uint64*)ea = (uint64)value


void VjobPushJob( void ( *pfnMain )( CellSpursJobContext2 * stInfo, CellSpursJob256 * job ), CellSpursJob128 * job );
extern void VjobSpuLog( const char * p, ... );
extern void VjobPpuRereadEA( uintp ea );

inline void DebugMemcpyEa( uint eaDest, uint eaSrc, uint nSize, void *lsScratch )
{
	Assert( ! ( 0xF & ( eaSrc | eaDest | nSize ) ) );
	memcpy( (void*)eaDest, (void*)eaSrc, nSize );
}

extern void TestAlignBuffer();

#define vec_to_uint32(X) (*(uint32*)&(X))

#endif // SPU


#define VjobDmaEa2Ls16(ea, ls) ((uintptr_t)(ls)+((uint32_t)(ea)&15))
#define VjobDmaEa2Ls128(ea, ls) ((uintptr_t)(ls)+((uint32_t)(ea)&127))

inline uint32* PrepareSmallPut32( vector unsigned int * lsAligned, volatile uint32 * eaUnaligned, uint32 nInitialValue )
{
	Assert( !( 3 & uint( lsAligned ) ) );
	uint32 * ls = ( uint32* )VjobDmaEa2Ls16( eaUnaligned, lsAligned );
	*ls = nInitialValue;
	return ls;
}

inline uint64* PrepareSmallPut64( vector unsigned int * lsAligned, volatile uint64 * eaUnaligned, uint64 nInitialValue )
{
	Assert( !( 7 & uint( lsAligned ) ) );
	uint64 * ls = ( uint64* )VjobDmaEa2Ls16( eaUnaligned, lsAligned );
	*ls = nInitialValue;
	return ls;
}



extern CellSpursJobContext2* g_stInfo;

#ifndef IsDebug
# ifdef _DEBUG
#  define IsDebug() true
# else
#  define IsDebug() false
# endif
#endif

#ifndef IsCert
# ifdef _CERT
#  define IsCert() true
# else
#  define IsCert() false
# endif
#endif

extern uint g_nBreakMask ;
#ifdef _CERT
# define BreakOn( nId )
#else
# define BreakOn( nId ) do				\
{										\
	if( g_nBreakMask & ( 1 << nId ) )	\
		DebuggerBreak();				\
}while( 0 )



#endif

inline void VjobDebugSpinCycles( uint nCycles )
{
	if( !IsCert() )
	{
		#ifdef SPU
			uint nStart = spu_read_decrementer();
			while( nStart - spu_read_decrementer() < nCycles / 40 )
				continue;
		#else
			sys_timer_usleep( nCycles / 3200 );
/*
			uint nStart = __mftb();
			
			while( __mftb() - nStart() < nCycles / 40 )
				continue;
*/
		#endif
	}
}


// this is the DMA list element without notify or reserved fields, so that it's easy to fill it in 
// and be sure there is no garbage left (in notify and reserved fields) and there are no bit field operations (to store size, which is effectively only 14-bit value)
struct BasicDmaListElement_t
{
	uint32 size;
	uint32 eal;
};


// shifts unaligned pBuffer of given size left by 0..15 bytes to make it aligned
// returns the aligned pointer, pBuffer & -16
extern void* AlignBuffer( void * pBuffer, uint nBytes);


//
// Adds constant nAdd to the given unaligned buffer of uint16's
// 
extern void UnalignedBufferAddU16( uint16 * pBuffer, uint nCount, uint16 nAdd );

// SpursJob_t must be one of CellSpursJob64, CellSpursJob128, CellSpursJob256,...
// JobParam_t is the parameter structure passed to the job
template < typename JobParam_t , typename SpursJob_t >
inline JobParam_t * VjobGetJobParams( void * pJob )
{
	Assert( sizeof( JobParam_t ) + sizeof( CellSpursJobHeader ) <= sizeof( SpursJob_t ) );
	JobParam_t * pJobParams = ( JobParam_t* ) ( uintp( pJob ) + ( sizeof( SpursJob_t ) - sizeof( JobParam_t ) ) );
	Assert( uintp( pJobParams + 1 ) == uintp( pJob ) + sizeof( SpursJob_t ) );
	return pJobParams;
}

extern void UnalignedBufferAddU16( );


template <uint n> struct Log2{};
template<>struct Log2<8> {enum{VALUE=3};};
template<>struct Log2<16>{enum{VALUE=4};};
template<>struct Log2<32>{enum{VALUE=5};};
template<>struct Log2<256>{enum{VALUE=8};};

#define COMPILE_TIME_LOG2(VAL) ( Log2<VAL>::VALUE )

inline void ZeroMemAligned( void * p, uint nSize )
{
	Assert( !( ( uintp( p ) | nSize ) & 15 ) );
	for( uint i = 0; i < nSize; i += 16 )
	{
		*( vec_uint4* )( uintp( p ) + i ) = (vec_uint4){0,0,0,0};
	}
}

inline void CopyMemAligned( void * pDst, const void * pSrc, uint nSize )
{
	Assert( !( ( uintp( pDst ) | uintp( pSrc ) | nSize ) & 15 ) );
	for( uint i = 0; i < nSize; i += 16 )
	{
		*( vec_uint4* )( uintp( pDst ) + i ) = *( vec_uint4* )( uintp( pSrc ) + i );
	}
}


///////////////////////////////////////////////////////////////////////////
//
//   Reference implementation
//
template <uint nBitCount>
class CBitArray
{
public:
	void Clear()
	{
		for( uint i = 0; i < ( nBitCount >> 7 ); ++i )
		{
			m_qword[i] = ( vec_uint4 ){0,0,0,0};
		}
		//m_nSetCount = 0;
	}
	void SetRange( uint nStart, uint nEnd )
	{
		nEnd = Min( nEnd, nBitCount );
		if( nStart > nEnd )
			return;
		//m_nSetCount = Max( nEnd, m_nSetCount );
		
		uint nMask = uint( -1 ) >> ( nStart & 0x1F );
		for( uint i = ( nStart >> 5 ); i < ( nEnd >> 5); ++i )
		{
			m_u32[i] |= nMask;
			nMask = uint( -1 );
		}
		nMask &= ~( uint( -1 ) >> ( nEnd & 0x1F ) );
		m_u32[ nEnd >> 5 ] |= nMask;
	}
	//uint GetSetCount()const{return m_nSetCount;}
	
	uint GetFirst1( uint nFrom )const
	{
		for( uint i = nFrom; i < nBitCount; ++i )
			if( GetBit( i ) )
				return i;
		return nBitCount;
	}

	uint GetFirst0( uint nFrom )const
	{
		for( uint i = nFrom; i < nBitCount; ++i )
			if( !GetBit( i ) )
				return i;
		return nBitCount;
	}
	
	uint GetBit( uint n )const
	{
		return m_u32[ n >> 5 ] & ( 0x80000000 >> ( n & 0x1F ) );
	}
protected:
	union
	{
		vec_uint4 m_qword[ ( nBitCount + 127 ) / 128 ];
		uint32 m_u32[ ( nBitCount + 31 ) / 32 ];
	};
	//uint m_nSetCount;
};

#endif // _PS3

#endif
