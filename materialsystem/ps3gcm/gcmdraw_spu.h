#ifndef INCLUDED_GCMDRAW_SPU_H
#define INCLUDED_GCMDRAW_SPU_H
//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

//--------------------------------------------------------------------------------------------------
// Headers
//--------------------------------------------------------------------------------------------------

#ifdef SPU
#include "SpuMgr_spu.h"
#else
#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "cell\gcm.h"
#include "SpuMgr_ppu.h"
#endif

//--------------------------------------------------------------------------------------------------
// Defines for the DMA tags
//--------------------------------------------------------------------------------------------------

#define SPU_DMAGET_TAG						0
#define SPU_DMAGET_TAG_WAIT					( 1 << SPU_DMAGET_TAG )

#define SPU_DMAPUT_TAG						1
#define SPU_DMAPUT_TAG_WAIT					( 1 << SPU_DMAPUT_TAG )


#endif // INCLUDED_GCMDRAW_SPU_H