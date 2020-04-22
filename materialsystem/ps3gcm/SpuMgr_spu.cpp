//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

//--------------------------------------------------------------------------------------------------
// Headers
//--------------------------------------------------------------------------------------------------

#include "SpuMgr_spu.h"
#include <cell/atomic.h>

#ifndef _CERT
#include <libsn_spu.h>
#endif

#include <stdlib.h>
#include <string.h>

//--------------------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------------------

// singleton instance
SpuMgr gSpuMgr __attribute__((aligned(128)));
unsigned char gUnalignedMem[16] __attribute__((aligned(16)));
MemCpyHeader gMemCpyHeader __attribute__((aligned(16)));

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

void SPU_memcpy( void *pBuf1, void *pBuf2 )
{	
	uint32_t header;

	gSpuMgr.ReadMailbox( &header );

	gSpuMgr.MemcpyLock();

	gSpuMgr.DmaGetUNSAFE( &gMemCpyHeader, header, sizeof( MemCpyHeader ), 0 );
	gSpuMgr.DmaDone( 0x1 );

	DEBUG_ERROR( ( gMemCpyHeader.src & 0xf ) == 0 );

	uint32_t sizeAligned;
	uint32_t sizeAlignedDown;
	uint32_t dstAlignedDown;
	uint32_t offset;

	memcpy( gUnalignedMem, gMemCpyHeader.cacheLine, 16 );

	while ( gMemCpyHeader.size > 8192 )
	{
		sizeAligned		= 8192;
		dstAlignedDown	= SPUMGR_ALIGN_DOWN( gMemCpyHeader.dst, 16 );
		offset			= gMemCpyHeader.dst - dstAlignedDown;

		gSpuMgr.DmaGetUNSAFE( pBuf1, gMemCpyHeader.src, sizeAligned, 0 );
		gSpuMgr.DmaDone( 0x1 );

		if ( offset )
		{
			memcpy( pBuf2, gUnalignedMem, offset );
		}

		memcpy( (void *) ( (uint32_t) pBuf2 + offset ), pBuf1, sizeAligned );

		gSpuMgr.DmaSync();
		gSpuMgr.DmaPut( dstAlignedDown, pBuf2, SPUMGR_ALIGN_UP( sizeAligned + offset, 16 ), 0 );
		gSpuMgr.DmaDone( 0x1 );

		sizeAlignedDown = SPUMGR_ALIGN_DOWN( sizeAligned + offset, 16 );
		memcpy( gUnalignedMem, (void *) ( (uint32_t) pBuf2 + sizeAlignedDown ), 16 );

		gMemCpyHeader.size -= sizeAligned;

		gMemCpyHeader.dst += 8192;
		gMemCpyHeader.src += 8192;
	}

	sizeAligned		= SPUMGR_ALIGN_UP( gMemCpyHeader.size, 16 );
	dstAlignedDown	= SPUMGR_ALIGN_DOWN( gMemCpyHeader.dst, 16 );
	offset			= gMemCpyHeader.dst - dstAlignedDown;

	gSpuMgr.DmaGetUNSAFE( pBuf1, gMemCpyHeader.src, sizeAligned, 0 );
	gSpuMgr.DmaDone( 0x1 );

	if ( offset )
	{
		memcpy( pBuf2, gUnalignedMem, offset );
	}

	memcpy( (void *) ( (uint32_t) pBuf2 + offset ), pBuf1, gMemCpyHeader.size );

	sizeAligned = SPUMGR_ALIGN_UP( gMemCpyHeader.size + offset, 16 );

	gSpuMgr.DmaSync();
	gSpuMgr.DmaPut( dstAlignedDown, pBuf2, sizeAligned, 0 );
	gSpuMgr.DmaDone( 0x1 );

	if ( gMemCpyHeader.blocking )
	{
		gSpuMgr.WriteMailbox( 0 );
	}

	gSpuMgr.MemcpyUnlock();
}


//--------------------------------------------------------------------------------------------------
// DmaCheckAlignment
//   
//   	Checks restrictions specified in SpuMgr::DmaGet
//--------------------------------------------------------------------------------------------------

int DmaCheckAlignment(uint32_t src, uint32_t dest, uint32_t size)
{
#if !defined( _CERT )
	
	uint32_t align = size;
	bool error = false;

	if (size >= 16 && ((size & 0xf) == 0))
	{
		align = 16;                
	}
	else if (size == 8 || size == 4 || size == 2 || size == 1)
	{
		error = ((src & 0xF) != (dest & 0xF));
	}
	else
	{
		error = true;  // bad size
	}

	return (!error && src && dest &&
			SPUMGR_IS_ALIGNED(src, align) &&
			SPUMGR_IS_ALIGNED(dest, align));
			

#else //!FINAL
	return 1;
#endif //!FINAL
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

int SpuMgr::Init()
{
	// Start the decrementer since it is possible
	// that it has not been started by default

	const unsigned int kEventDec = 0x20;

	// Disable the decrementer event.
	unsigned int maskEvents = spu_readch(SPU_RdEventStatMask);
	spu_writech(SPU_WrEventMask, maskEvents & ~kEventDec);

	// Acknowledge any pending events and stop the decrementer.
	spu_writech(SPU_WrEventAck, kEventDec);

	// Write the decrementer value to start the decrementer.
	unsigned int decValue = spu_readch(SPU_RdDec);
	spu_writech(SPU_WrDec, decValue);

	// Enable events.
	spu_writech(SPU_WrEventMask, maskEvents | kEventDec);

	// Reset byte count
	ResetBytesTransferred();

	// reset malloc count
	m_mallocCount = 0;

	// Read the effective address of the SPU locks.
	ReadMailbox( &m_lockEA );
	ReadMailbox( &m_memcpyLockEA );

	return 0;
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

void SpuMgr::Term()
{
}

//--------------------------------------------------------------------------------------------------
// SpuMgr::DmaGet
//
// DmaGet       - alignment and size checking
// DmaGetUNSAFE - no alignment or size checking (but will assert in debug)
// _DmaGet  - handles badly aligned dma's, should be a private member really (doesn't handle small dma's)
//
// DMA restrictions
//		An MFC supports naturally aligned DMA transfer sizes of 1, 2, 4, 
//		8, and 16 bytes and multiples of 16 bytes
//		Furthermore, if size is 1, 2, 4, or 8 bytes then lower 4 bits
//		of LS and EA must match
//
//	Note:
//		Peak performance is achieved for transfers in which both the EA and 
//		the LSA are 128-byte aligned and the size of the transfer is a multiple 
//		of 128 bytes.
//--------------------------------------------------------------------------------------------------


void SpuMgr::DmaGetUNSAFE(void *ls, uint32_t ea, uint32_t size, uint32_t tagId)
{
	DEBUG_ERROR( ea < 0xd0000000 );
	DEBUG_ERROR( ea );
	DEBUG_ERROR(DmaCheckAlignment((uint32_t)ls, ea, size));

	// do the dma
	while (size)
	{
		uint32_t dmaSize = 0x4000;
		dmaSize = (size < dmaSize)? size: dmaSize;
		size -= dmaSize;

		// kick off dma
		spu_mfcdma64( (void*)ls, 0, ea, dmaSize, tagId, MFC_GET_CMD);
		m_numDMATransfers++;

		ls = (void*)((uint32_t)ls + dmaSize);
		ea += dmaSize;
	}

	// add up bytes transferred
	m_bytesRequested   += size;
	m_bytesTransferred += size;
}


//--------------------------------------------------------------------------------------------------
// SpuMgr::_DmaGet
//
//	Internal function - do not call this directly
//--------------------------------------------------------------------------------------------------

void SpuMgr::_DmaGet(void *ls, uint32_t ea, uint32_t size, uint32_t tagId)
{
	uint32_t unaligned = false;
	uint32_t eaAligned = (uint32_t)ea;
	uint32_t sizeAligned = size;
	uint32_t lsAligned = (uint32_t)ls;
	uint32_t sizeOffset = 0;
	char *pTempBuff = NULL;

	// check if src is unaligned
	if (eaAligned & 0xF)
	{
		eaAligned = eaAligned & ~0xF;	// round down
		sizeOffset = ea - eaAligned;
		sizeAligned += sizeOffset;
		unaligned = true;
	}

	// check if size is unaligned
	if (sizeAligned & 0xF)
	{
		sizeAligned = (sizeAligned + 0xF) & ~0xF;	// round up
		unaligned = true;
	}

	// if we have adjusted the size, or if ls is unaligned,
	// we need to alloc temp buffer
	if (unaligned || (lsAligned & 0xF))
	{
		pTempBuff = (char*)MemAlign(0x10, sizeAligned);

		lsAligned = (uint32_t)pTempBuff;
		unaligned = true;
	}

	// add up bytes transferred, for informational purposes
	m_bytesRequested += size;
	m_bytesTransferred += sizeAligned;

	// do the dma
	while (sizeAligned)
	{
		uint32_t dmaSize = 0x4000;
		dmaSize = (sizeAligned < dmaSize)? sizeAligned: dmaSize;
		sizeAligned -= dmaSize;

		// kick off dma
		spu_mfcdma64( (void*)lsAligned, 0, eaAligned, dmaSize, tagId, MFC_GET_CMD);
		m_numDMATransfers++;

		lsAligned += dmaSize;
		eaAligned += dmaSize;
	}

	if (unaligned)
	{
		// block for now till dma done because we do the memcpy right here
		DmaDone(1 << tagId);

		// copy data over
		memcpy(ls, pTempBuff + sizeOffset, size);

		// free temp buff
		Free(pTempBuff);
	}
}

//--------------------------------------------------------------------------------------------------
// SpuMgr::DmaGetSAFE
//
//	DMA restrictions (look at SpuMgr::DmaGetUNSAFE in this file) are 
//	handled transparently by this function
//--------------------------------------------------------------------------------------------------

void SpuMgr::DmaGetSAFE(void *ls, uint32_t ea, uint32_t size, uint32_t tagId)
{
	DEBUG_ERROR( ea );

	if( size < 0x10 )
	{
		// lowest 4 bits of address have to match regardless, &
		// size can only be 1, 2, 4 or 8 B

		if( size==0x1 || size==0x2 || size==0x4 || size==0x8 )
		{
			if( ((uint32_t)ls&0xF == ea&0xF) )
			{
				DmaGetUNSAFE(ls,ea,size,tagId);
			}
			else
			{
				// small get not aligned within a 16B block
				_DmaGet(ls,ea,size,tagId);
			}
		}
		else
		{
			// if < 16B can only get 1,2,4 or 8B
			_DmaGet(ls,ea,size,tagId);
		}
	}
	else
	{
		if( (!(size & 0xF)) &&			// has to be multiple of 16B, &
			(((uint32_t)ls&0xF)==0) &&	// ea and ls have to be 16B aligned
			((ea&0xF)==0)  )
		{
			// alignment is okay just dma
			DmaGetUNSAFE(ls,ea,size,tagId);
		}
		else
		{
			_DmaGet(ls,ea,size,tagId);
		}
	}
}

//--------------------------------------------------------------------------------------------------
// SpuMgr::DmaPut
//--------------------------------------------------------------------------------------------------

void SpuMgr::DmaPut(uint32_t ea, void *ls, uint32_t size, uint32_t tagId)
{
	DEBUG_ERROR( (ea!=0) && (ea<0xd0000000) );	// valid ea
	DEBUG_ERROR( (uint32_t)ls < 0x40000 );		// valid ls
	DEBUG_ERROR(DmaCheckAlignment((uint32_t)ls, ea, size));
	
	// do the dma
	while (size)
	{
		uint32_t dmaSize = 0x4000;
		dmaSize = (size < dmaSize)? size: dmaSize;
		size -= dmaSize;

		// initiate dma to ppu
		spu_mfcdma64( ls, 0, ea, dmaSize, tagId, MFC_PUT_CMD);

		ls = (void*)((uint32_t)ls + dmaSize);
		ea += dmaSize;
	}
}

//--------------------------------------------------------------------------------------------------
// SpuMgr::DmaSmallPut
//--------------------------------------------------------------------------------------------------

void SpuMgr::DmaSmallPut(uint32_t ea, void *ls, uint32_t size, uint32_t tagId)
{
	DEBUG_ERROR( (ea!=0) && (ea<0xd0000000) );	// valid ea
	DEBUG_ERROR( (uint32_t)ls < 0x40000 );		// valid ls
	DEBUG_ERROR(DmaCheckAlignment((uint32_t)ls, ea, size));

	uint32_t dmaSize = 1;

	if ((size % 8) == 0)
	{
		dmaSize = 8;
	}
	else if ((size % 4) == 0)
	{
		dmaSize = 4;
	}
	else if ((size % 2) == 0)
	{
		dmaSize = 2;
	}

	while (size)
	{
		size -= dmaSize;

		// initiate dma to ppu
		spu_mfcdma64( ls, 0, ea, dmaSize, tagId, MFC_PUT_CMD);

		ls = (void*)((uint32_t)ls + dmaSize);
		ea += dmaSize;
	}
}

//--------------------------------------------------------------------------------------------------
// SpuMgr::DmaGetlist
//
// Gather data scattered around main mem, MFC will run through the list, and place the elements (based on ea address and size)
// contiguously in ls.
//
// NOTE: if an individual list element size is <16B, the data will still be dma'd but the proceeding element will be placed 
// on the next 16B boundary. So it is possible to get lots of small elements, but you will be left with gaps in ls.
//
// ls - ls address of where items will be placed (contiguously)
// lsList - ls address of actual list
// sizeList - size of list in bytes (each list element is 8B (sizeof(DMAList)), so sizeList should be number of list elements // sizeof(DMAList))
// tagId - works the same way as regular DMA's
//
// Alignment and Size Restrictions:
// -ls and lsList must be 8B aligned
// -size must be a multiple of 8B (sizeof(DMAList))
// -no more than 2048 list elements
//
// light error checking right now
//--------------------------------------------------------------------------------------------------

void SpuMgr::DmaGetList(void *ls, DMAList *pLS_List, uint32_t sizeList, uint32_t tagId)
{
	DEBUG_ERROR( ((uint32_t)pLS_List&0x7) == 0 );	// ls address must be 8B aligned
	DEBUG_ERROR( ((uint32_t)ls&0x7) == 0 );			// ea so aligned also, due to offset within 16B alignment restrictions
	DEBUG_ERROR( (sizeList&0x7) == 0 );				// list size is a multiple of 8B
	DEBUG_ERROR( sizeList<(2048*sizeof(DMAList)));	// no more than 2048 list elements


	// initiate dma list
	spu_mfcdma64( ls, 0, (uint32_t)pLS_List, sizeList, tagId, MFC_GETL_CMD );
}

//--------------------------------------------------------------------------------------------------
// SpuMgr::DmaGPutlist
//   
// Scatter data held contiguously in ls, to main mem
// 
//   ls - ls address of where items exist (contiguously) to be scattered back to main mem
// lsList - ls address of actual list
// sizeList - size of list in bytes (each list element is 8B (sizeof(DMAList)), so sizeList should be number of list elements * sizeof(DMAList))
//   tagId - works the same way as regular DMA's
// 
// Alignment and Size Restrictions:
// ls and lsList must be 8B aligned, size must be a multiple of 8B (sizeof(DMAList))
// 
//   light error checking right now
//--------------------------------------------------------------------------------------------------

void SpuMgr::DmaPutList(void *ls, DMAList* pLS_List, uint32_t sizeList, uint32_t tagId)
{
	DEBUG_ERROR( ((uint32_t)pLS_List&0x7) == 0 );	// ls address must be 8B aligned
	DEBUG_ERROR( ((uint32_t)ls&0x7) == 0 );			// ea so aligned also, due to offset within 16B alignment restrictions
	DEBUG_ERROR( (sizeList&0x7) == 0 );				// list size is a multiple of 8B
	DEBUG_ERROR( sizeList<(2048*sizeof(DMAList)));	// no more than 2048 list elements

	// initiate dma list
	spu_mfcdma64( ls, 0, (uint32_t)pLS_List, sizeList, tagId, MFC_PUTL_CMD );
}
