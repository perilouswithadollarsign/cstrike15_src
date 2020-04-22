//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#ifndef INCLUDED_SPUMGR_DMA_H
#define INCLUDED_SPUMGR_DMA_H

//--------------------------------------------------------------------------------------------------
// Headers
//--------------------------------------------------------------------------------------------------

#include <stdint.h>

#ifdef SPU
//#include <Stdshader_spu/Inc/debug_spu.h> // MH
#else
#include <debug/inc/debug.h>
#endif

//--------------------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------------------

#define SPUMGR_IS_ALIGNED(val, align)	(((val) & ((align) - 1)) == 0)
#define SPUMGR_ALIGN_UP(val, align)		(((val) + ((align)-1)) & ~((align) - 1))
#define SPUMGR_ALIGN_DOWN(val, align)	((val) & ~((align) - 1))

#define SPUMGR_MSG_MEMCPY				0x000000ff

#define Assert(val) // MH

//--------------------------------------------------------------------------------------------------
// Types
//--------------------------------------------------------------------------------------------------

struct MemCpyHeader
{
	uint32_t src;
	uint32_t dst;
	uint32_t size;
	uint32_t blocking;
	uint8_t	 cacheLine[16];
};

//--------------------------------------------------------------------------------------------------
// Classes
//--------------------------------------------------------------------------------------------------

struct DMAList
{
	uint32_t stallAndNotify	:1;
	uint32_t reserved		:16;
	uint32_t size			:15;
	uint32_t ea;
};

//--------------------------------------------------------------------------------------------------
// DmaCheckAlignment
//	Checks restrictions specified in SpuMgr::DmaGet
//--------------------------------------------------------------------------------------------------

int DmaCheckAlignment(uint32_t src, uint32_t dest, uint32_t size);

//--------------------------------------------------------------------------------------------------
//SetupDmaListEntry 
//
//	Note that this function increments input ptr by number of entries added,
//	which will be > 1 if size > 16K
//--------------------------------------------------------------------------------------------------

inline void SetupDmaListEntry(uint32_t stall, uint32_t ea, uint32_t size, DMAList **pDmaList)
{
	// check alignment; don't pass in NULL for dest
	if (!DmaCheckAlignment(ea, 0x10, size))
	{
		Assert(0);
	}

	Assert((size & 0xF) == 0);	// for lists input sizes must be multiple of 16 bytes

	while (size)
	{
		uint32_t dmaSize = 0x4000;
		dmaSize = size < dmaSize? size: dmaSize;

		(*pDmaList)->stallAndNotify = stall;
		(*pDmaList)->size			= dmaSize;
		(*pDmaList)->ea				= ea;

		size -= dmaSize;
		ea += dmaSize;
		(*pDmaList)++;
	}
}

#endif // INCLUDED_SPUMGR_DMA_H
