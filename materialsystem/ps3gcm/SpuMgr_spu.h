//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#ifndef INCLUDED_SPUMGR_SPU_H
#define INCLUDED_SPUMGR_SPU_H

//--------------------------------------------------------------------------------------------------
// Headers
//--------------------------------------------------------------------------------------------------

#include <stdint.h>
#include <string.h>
#include <spu_intrinsics.h>
#include <spu_mfcio.h>
#include <stdlib.h>
#include <cell/atomic.h>

#include "SpuMgr_dma.h"

#include <libsn_spu.h>

//--------------------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------------------

#define DEBUG_ASSERT(val) Assert(val)
#define DEBUG_ERROR(val) Assert(val)

#define Msg(...)
#define Error(...)
#define DebuggerBreak() snPause()

#include <sys/integertypes.h>

//Short aliases
typedef	int8_t							s8;		
typedef	uint8_t							u8;		
typedef	int16_t							s16;	
typedef	uint16_t						u16;	
typedef	int32_t							s32;	
typedef	uint32_t						u32;	
typedef	uint32_t						u64[2];	
typedef float							f32;	
typedef double							f64;	
typedef int								BOOL;

typedef	s8		int8;		
typedef	u8		uint8;		
typedef	s16		int16;	
typedef	u16		uint16;	
typedef	s32		int32;	
typedef	u32		uint32;	
typedef	u64		uint64;	

typedef unsigned int uintp;
typedef unsigned int uint;
typedef vector float fltx4 ;
#define INT_MAX	0x7fffffff

#define DECL_ALIGN(x)			__attribute__( ( aligned( x ) ) )
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

#define  FORCEINLINE inline /* __attribute__ ((always_inline)) */

#define IsPlatformPS3()		1
#define IsPlatformPS3_PPU()	0
#define IsPlatformPS3_SPU()	1
#define IsPlatformX360()	0
#define IsPlatformOSX()		0

#define RESTRICT
#define V_memset			__builtin_memset
#define V_memcpy			memcpy

void SPU_memcpy( void *pBuf1, void *pBuf2 );
#define MemAlloc_AllocAligned(size, align) gSpuMgr.MemAlign(align, size)

#define ARRAYSIZE(p)	(sizeof(p)/sizeof(p[0]))
#define MIN( a, b ) ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
#define MAX( a, b ) ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )

//--------------------------------------------------------------------------------------------------
// Task handle
//--------------------------------------------------------------------------------------------------

class SpuTaskHandle
{
public:
	uint32_t							m_spuId;
	uint64_t							m_ppuThread;
	uint32_t							m_intrTag;
	uint32_t							m_interruptThread;
	uint32_t							m_lock;
	uint32_t							m_memcpyLock;
};	


//--------------------------------------------------------------------------------------------------
// SpuMgr
//--------------------------------------------------------------------------------------------------

class SpuMgr
{
public:

	// Init/Term

	int Init();	
	void Term();

	// MFC Atomic Update functionality
	// Currently provides functionality to read/write up to 
	// one cache line (128 bytes) of main mem
	inline void MFCAGet(void *ls, uint32_t ea, uint32_t size);
	inline void MFCAPut(void *ls, uint32_t ea, uint32_t size);

	//
	// DMA functionality
	//

	// tagId is a value between 0 and 31 that can be used to group
	// dma requests together

	void DmaGetSAFE(void *ls, uint32_t ea, uint32_t size, uint32_t tagId);
	void DmaGetUNSAFE(void *ls, uint32_t ea, uint32_t size, uint32_t tagId);
	void DmaPut(uint32_t ea, void *ls, uint32_t size, uint32_t tagId);
	void DmaSmallPut(uint32_t ea, void *ls, uint32_t size, uint32_t tagId);

	void DmaGetList(void *ls, DMAList *pLS_List, uint32_t sizeList, uint32_t tagId);
	void DmaPutList(void *ls, DMAList* pLS_List, uint32_t sizeList, uint32_t tagId);

	inline int DmaDone(uint32_t dmaTagMask, bool bBlocking = true);
	
	// DmaSync
	// All earlier store instructions are forced to complete 
	// before proceeding. This function ensures that all stores to
	// to local storage are visible to the MFC or PPU.
	inline void DmaSync()
	{
		__asm("dsync");
	}
	
	//
	// Mailbox functions - see SpuMgr_ppu.h for a descrition of mailboxes
	//

	int WriteMailbox(uint32_t val, bool bBlocking = true);	
	int WriteIntrMailbox(uint32_t val, bool bBlocking = true);

	int WriteMailboxChannel(uint32_t val, uint32_t channel, bool bBlocking /* = true */);
	int ReadMailbox(uint32_t *pVal, bool bBlocking = true);

	bool Lock();
	void Unlock();
	bool MemcpyLock();
	void MemcpyUnlock();

	// Decrementer access, for time stamps
	inline uint32_t ReadDecr(void);

	// mem mgr

	void *Malloc( uint32_t size )
	{
		m_mallocCount++;
		void *ptr = malloc( size );
		DEBUG_ASSERT( ptr );
		return ptr;
	}

	void *MemAlignUNSAFE(uint32_t boundary, uint32_t size )
	{
		m_mallocCount++;
		void *ptr = memalign( boundary, size );
		return ptr;
	}

	void *MemAlign( uint32_t boundary, uint32_t size )
	{
		void *ptr = MemAlignUNSAFE(boundary, size);
		DEBUG_ERROR( ptr );
		return ptr;
	}

	void Free( void *pData )
	{
		m_mallocCount--;
		free( pData );
	}

	uint32_t GetMallocCount()
	{
		return m_mallocCount;
	}

	// counters to help us keep track of how much data we are moving

	inline void ResetBytesTransferred()
	{
		m_bytesRequested = 0;
		m_bytesTransferred = 0;
		m_numDMATransfers = 0;
	}
	
	// Private data and member functions
	void _DmaGet(void *ls, uint32_t ea, uint32_t size, uint32_t tagId);

	uint32_t m_lock[32] __attribute__ ((aligned(128)));
	uint32_t m_lockEA;
	uint32_t m_memcpyLock[32] __attribute__ ((aligned(128)));
	uint32_t m_memcpyLockTest;// __attribute__ ((aligned(128)));
	uint32_t m_memcpyLockEA;

	uint32_t m_bytesRequested;
	uint32_t m_bytesTransferred;
	uint32_t m_numDMATransfers;
	uint32_t m_mallocCount;

	uint8_t  m_MFCACacheLine[128]	__attribute__ ((aligned(128)));
};

//--------------------------------------------------------------------------------------------------
// 
//--------------------------------------------------------------------------------------------------

inline void SpuMgr::MFCAGet(void *ls, uint32_t ea, uint32_t size)
{
	// get start of cache line
	uint32_t eaAligned = SPUMGR_ALIGN_DOWN(ea, 0x80);

	// get offset to given ea
	uint32_t eaOffset = ea - eaAligned;

	// check size to read
	DEBUG_ASSERT(size + eaOffset <= 0x80);

	// read cache line
	spu_mfcdma64(&m_MFCACacheLine[0], 0, eaAligned, 128, 0, MFC_GETLLAR_CMD);	

	// wait for completion - this is a blocking read
	spu_readch(MFC_RdAtomicStat);

	// copy out data
	memcpy(ls, &m_MFCACacheLine[eaOffset], size);
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

inline void SpuMgr::MFCAPut(void *ls, uint32_t ea, uint32_t size)
{
	// get start of cache line
	uint32_t eaAligned = SPUMGR_ALIGN_DOWN(ea, 0x80);

	// get offset to given ea
	uint32_t eaOffset = ea - eaAligned;

	// check size to write
	DEBUG_ASSERT(size + eaOffset <= 0x80);

	// atmoic update - read cache line and reserve it, update it, 
	// conditionally write it back until write succeeds
	// if write succeeds then spu_readch(MFC_RdAtomicStat) returns 0
	do 
	{
		// read cache line
		spu_mfcdma64(&m_MFCACacheLine[0], 0, eaAligned, 128, 0, MFC_GETLLAR_CMD);	

		// wait for completion - this is a blocking read
		spu_readch(MFC_RdAtomicStat);

		spu_dsync();

		// update cache line
		memcpy(&m_MFCACacheLine[eaOffset], ls, size);

		// dsync to make sure it's commited to LS
		spu_dsync();

		// write it back
		spu_mfcdma64(&m_MFCACacheLine[0], 0, eaAligned, 128, 0, MFC_PUTLLC_CMD);

	} while (__builtin_expect(spu_readch(MFC_RdAtomicStat), 0));
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

inline int SpuMgr::DmaDone(uint32_t dmaTagMask, bool bBlocking /*=true*/)
{
	// From Cell Broadband Engine Architecture V1.0 Chapter 9.3.1 "Procedures for Determining the Status of Tag Groups"
	//
	// For polling for the completion of an MFC command or for the completion of a group of MFC commands, the
	// basic procedure is as follows:
	//	1. Clear any pending tag status update requests by:
	//		• Writing a ‘0’ to the MFC Write Tag Status Update Request Channel (see page 116)
	//		• Reading the channel count associated with the MFC Write Tag Status Update Request Channel (see
	//		  page 116), until a value of ‘1’ is returned
	//		• Reading the MFC Read Tag-Group Status Channel (see page 117) and discarding the tag status
	//		  data.
	//	2. Enable the tag groups of interest by writing the MFC Write Tag-Group Query Mask Channel (see page
	//	   114) with the appropriate mask data (only needed if a new tag-group mask is required).
	//	3. Request an immediate tag status update by writing the MFC Write Tag Status Update Request Channel
	//	   (see page 116) with a value of ‘0’.
	//	4. Perform a read of the MFC Read Tag-Group Status Channel (see page 117). The data returned is the
	//	   current status of each tag group with the tag-group mask applied.
	//	5. Repeat steps 3 and 4 until the tag group or the tag groups of interest are complete.
	//
	// Note
	//	MFC Write Tag Status Update Request Channel = MFC_WrTagUpdate
	//	MFC Read Tag-Group Status Channel			= MFC_RdTagStat
	//	MFC Write Tag-Group Query Mask Channel		= MFC_WrTagMask

	// Here we go...

	// 1. Clear any pending tag status update requests	
	spu_writech(MFC_WrTagUpdate, 0);
	do {} while (spu_readchcnt(MFC_WrTagUpdate) == 0);
	spu_readch(MFC_RdTagStat);

	// 2. Enable the tag groups of interest
	spu_writech(MFC_WrTagMask, dmaTagMask); 

	uint32_t dmaDone = 0;

	do
	{
		// 3. Request an immediate tag status update
		spu_writech(MFC_WrTagUpdate, 0);

		// 4. Perform a read of the MFC Read Tag-Group Status Channel
		uint32_t tagGroupStat = spu_readch(MFC_RdTagStat);

		dmaDone = (tagGroupStat == dmaTagMask);

	} while (bBlocking && !dmaDone);
	
	return !dmaDone;
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

inline int SpuMgr::WriteMailbox(uint32_t val, bool bBlocking /* = true */)
{
	uint32_t mboxAvailable;

	do
	{
		mboxAvailable = spu_readchcnt(SPU_WrOutMbox);
	} while (bBlocking && !mboxAvailable);

	if (mboxAvailable)
	{
		spu_writech(SPU_WrOutMbox, val);
	}
	
	return !mboxAvailable;
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

inline int SpuMgr::WriteIntrMailbox(uint32_t val, bool bBlocking /* = true */)
{
	uint32_t mboxAvailable;

	do
	{
		mboxAvailable = spu_readchcnt(SPU_WrOutIntrMbox);

	} while (bBlocking && !mboxAvailable);

	if (mboxAvailable)
	{
		spu_writech(SPU_WrOutIntrMbox, val);
	}

	return !mboxAvailable;
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

inline int SpuMgr::ReadMailbox(uint32_t *pVal, bool bBlocking /* = true */)
{
	uint32_t mailAvailable;

	do
	{
		mailAvailable = spu_readchcnt(SPU_RdInMbox);
	} while (bBlocking && !mailAvailable);

	if (mailAvailable)
		*pVal = spu_readch(SPU_RdInMbox);

	return !mailAvailable;
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

inline uint32_t SpuMgr::ReadDecr(void)
{
	return spu_readch(SPU_RdDec);
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

inline bool SpuMgr::Lock()
{
	return cellAtomicCompareAndSwap32( m_lock, m_lockEA, 0, 1 ) == 0;
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

inline void SpuMgr::Unlock()
{
	cellAtomicCompareAndSwap32( m_lock, m_lockEA, 1, 0 );
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

inline bool SpuMgr::MemcpyLock()
{
	return cellAtomicCompareAndSwap32( m_memcpyLock, m_memcpyLockEA, 0, 1 ) == 0;
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

inline void SpuMgr::MemcpyUnlock()
{
	cellAtomicCompareAndSwap32( m_memcpyLock, m_memcpyLockEA, 1, 0 );
}

//--------------------------------------------------------------------------------------------------
// Externs
//--------------------------------------------------------------------------------------------------

extern SpuMgr gSpuMgr;

#endif // INCLUDED_SPUMGR_SPU_H
