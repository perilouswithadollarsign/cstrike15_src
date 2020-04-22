//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

//--------------------------------------------------------------------------------------------------
// Headers
//--------------------------------------------------------------------------------------------------

#include "sys/memory.h"
#include "sysutil/sysutil_sysparam.h"
#include "cell/sysmodule.h"

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier1/utlbuffer.h"

#include <sys/timer.h>
#include <sys/spu_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <cell/cell_fs.h>
#include <cell/atomic.h>
#include <string.h>

#include "ps3_pathinfo.h"

#include <cell/spurs/control.h>

#include "SpuMgr_ppu.h"

#include "memdbgon.h"

typedef uint32_t uint32;

#define ASSERT Assert

//--------------------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------------------

// Spu Mailbox Status Register
// Described in CBE architecture chapter 8.6.3 SPU Mailbox Status Register (SPU_Mbox_Stat)

#define SPU_IN_MBOX_COUNT_SHIFT (8) 
#define SPU_IN_MBOX_COUNT (0xFF << SPU_IN_MBOX_COUNT_SHIFT)

#define SPU_OUT_MBOX_COUNT (0xFF)

#define SPU_OUT_INTR_MBOX_COUNT_SHIFT (16) 
#define SPU_OUT_INTR_MBOX_COUNT (0xFF << SPU_OUT_INTR_MBOX_COUNT_SHIFT)

//--------------------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------------------

// SPU manager instance
SpuMgr gSpuMgr;

//--------------------------------------------------------------------------------------------------
// DmaCheckAlignment
//	Checks restrictions specified in SpuMgr::DmaGet
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

#else //!_CERT
	return 1;
#endif //!_CERT
}

//--------------------------------------------------------------------------------------------------
// Internal functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// handle_syscall
//
//	interrupt handler to handle SPU interrupts
//	see Handle SPU Interrupts Lv2-Uders_manual_e P34
//--------------------------------------------------------------------------------------------------

void handle_syscall (uint64_t arg)
{
	sys_raw_spu_t id = arg;
	uint64_t stat;
	int ret;

#ifndef _CERT
	g_snRawSPULockHandler();
#endif

	// Create a tag to handle class 2 interrupt, SPU halts fall in
	// this category

	ret = sys_raw_spu_get_int_stat(id, 2, &stat);
	if (ret)
	{
#ifndef _CERT
		g_snRawSPUUnlockHandler();
#endif
		sys_interrupt_thread_eoi();
	}

	//
	// SPU Stop-and-Signal Instruction Trap
	// This interrupt occurs when the SPU executes a stop-and-signal 
	// instruction.
	//
	
	if (stat & INTR_STOP_MASK)	//stop
	{
		//We've hit a stop, so what kind of value is it?
		uint32_t signalVal = GetStopSignal( id );
	
		switch ( signalVal )
		{
		case 0x3:

			// it was a stop that is in the SPU code to signal to the PPU

			// do any processing for the user defined stop here
			// if we do not restart the SPU then we need to call g_snRawSPUNotifySPUStopped(id) 
			// to inform the debugger that SPU has stopped

			//restart the SPU
			sys_raw_spu_mmio_write( id, SPU_RunCntl, 0x1 );
			break;

		default:
#ifndef _CERT
			g_snRawSPUNotifySPUStopped(id);
#endif
			break;
		}
	}
	else if (stat & INTR_HALT_MASK)	// halt
	{
#ifndef _CERT
		g_snRawSPUNotifySPUStopped(id);
#endif
	}

	// Other class 2 interrupts could be handled here
	// ...

	//
	// Must reset interrupt status bit of those not handled.  
	//
	ret = sys_raw_spu_set_int_stat(id, 2, stat);
	if (ret)
	{
#ifndef _CERT
		g_snRawSPUUnlockHandler();
#endif
		sys_interrupt_thread_eoi();
	}

	//
	// End of interrupt
	//
#ifndef _CERT
	g_snRawSPUUnlockHandler();
#endif
	sys_interrupt_thread_eoi();
}

int CreateDefaultInterruptHandler(SpuTaskHandle *pTask)
{
	int res = 0;

	//
	// Create a SPU interrupt handler thread, an interrupt tag,
	// and associate it with the thread
	//

	// create thread

	if (sys_ppu_thread_create(&pTask->m_ppuThread, handle_syscall, 
		0, INTR_HANDLER_THREAD_PRIORITY, INTR_HANDLER_THREAD_STACK_SIZE, 
		SYS_PPU_THREAD_CREATE_INTERRUPT, "Interrupt PPU Thread"))
	{
		res = 1;
		goto xit;
	}

	// create interrupt tag for handling class 2 interrupts from this spu

	if (sys_raw_spu_create_interrupt_tag(pTask->m_spuId, 2, SYS_HW_THREAD_ANY, &pTask->m_intrTag))
	{
		res = 1;
		goto xit;
	}

	// associate interrupt tag with thread

	if (sys_interrupt_thread_establish(&pTask->m_interruptThread, pTask->m_intrTag, 
		pTask->m_ppuThread, pTask->m_spuId))
	{
		res = 1;
		goto xit;
	}

	// Set interrupt mask - enable Halt, Stop-and-Signal interrupts
	if (sys_raw_spu_set_int_mask(pTask->m_spuId, 2, INTR_STOP_MASK | INTR_HALT_MASK))
	{
		res = 1;
		goto xit;
	}

xit:
	return res;
}

//--------------------------------------------------------------------------------------------------
// Class Methods
//--------------------------------------------------------------------------------------------------

int SpuMgr::Init(int numRawSpu)
{
	// Need at least 2 SPUs for SPURS instances
	ASSERT(numRawSpu < 5);


	// Run SPURS on all SPUs that are not in raw mode

	// Creating two SPURS instances. One with a thread group of 5 - numRawSpu threads and one
	// with a thread group of 1 thread. 
	
	// The instance with a single thread is designed to be singled out as the preemption victim
	// when the OS needs to use an SPU. We ensure this by giving it a lower priority than the
	// dedicated SPURS instance.


	// Init dedicated SPUs SPURS instance
// 	CellSpursAttribute attr;
// 	int32 ret = cellSpursAttributeInitialize(&attr, 5 - numRawSpu, 99, 2, false);
// 	ASSERT(ret == CELL_OK);
// 	ret = cellSpursAttributeEnableSpuPrintfIfAvailable(&attr);
// 	ASSERT(ret == CELL_OK);
// 	ret = cellSpursAttributeSetNamePrefix(&attr, "gameSpusSpurs", std::strlen("gameSpusSpurs"));
// 	ASSERT(ret == CELL_OK);
// 	ret = cellSpursInitializeWithAttribute2(&m_exclusiveSpusSpurs, &attr);
// 	ASSERT(ret == CELL_OK);

	// Init pre-emption SPU SPURS instance
// 	ret = cellSpursAttributeInitialize(&attr, 1, 100, 2, false);
// 	ASSERT(ret == CELL_OK);
// 	ret = cellSpursAttributeEnableSpuPrintfIfAvailable(&attr);
// 	ASSERT(ret == CELL_OK);
// 	ret = cellSpursAttributeSetNamePrefix(&attr, "sharedSpuSpurs", std::strlen("sharedSpuSpurs"));
// 	ASSERT(ret == CELL_OK);
// 	ret = cellSpursInitializeWithAttribute2(&m_preemptedSpuSpurs, &attr);
// 	ASSERT(ret == CELL_OK);


    int res = 0;
	
	// set up members
	m_numSpus = 0;
	
	// Initialize SPUs
	if (sys_spu_initialize(6, numRawSpu) != SUCCEEDED) 
	{
		res = 1;
		goto xit;
	}

	// Create raw spus
	for (; m_numSpus < (uint32)numRawSpu; m_numSpus++)
	{
		if (sys_raw_spu_create(&m_spuIds[m_numSpus], NULL) != SUCCEEDED)
		{
			Error("Unable to create saw spu\n");

			res = 1;
			goto xit;
		}

#ifndef _CERT
		g_snRawSPUNotifyCreation(m_spuIds[m_numSpus]);
#endif
		m_spuInUse[m_numSpus] = 0;
	}

xit:
	return res;
}

void SpuMgr::Term()
{
	uint32 spu;

	// destroy raw spus
	for (spu = 0; spu < m_numSpus; spu++)
	{
		sys_raw_spu_destroy(m_spuIds[spu]);
	}

	// destroy the SPURS instances
// 	int ret;
// 	ret = cellSpursfinalize(&m_exclusiveSpusSpurs);
// 	ASSERT(ret == CELL_OK);
// 
// 	ret = cellSpursfinalize(&m_preemptedSpuSpurs);
// 	ASSERT(ret == CELL_OK);

	m_numSpus = 0;
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

uint32_t spumgr_mmio_read(uint32_t spu, uint32_t regoffset)
{
	uint64_t addr = get_reg_addr(spu,regoffset);
	addr &= 0xffffffffUL;
	volatile uint32_t * pAddr = (uint32_t*) addr;
	return *pAddr;
}

void spumgr_mmio_write(int spu, int regoffset, uint32_t value)
{
	uint64_t addr = get_reg_addr(spu,regoffset);
	addr &= 0xffffffffUL;
	volatile uint32_t * pAddr = (uint32_t*) addr;
	*pAddr = value;
}

//--------------------------------------------------------------------------------------------------
// Create Spu task from file based image
//--------------------------------------------------------------------------------------------------


static char modPath[MAX_PATH];

int SpuMgr::CreateSpuTask(const char *path, SpuTaskHandle *pTask, 
						  CreateSPUTaskCallback *pfnCallback /* = NULL */)
{
	int res = 0;
	int ret;
	uint32 spu;
	register uint32 spuid;
	uint32 entry;
	FILE* fp;
	void* pSpuProg = NULL;

	sys_spu_image_t img;

	pTask->m_spuId = -1;
	pTask->m_ppuThread = NULL;
	pTask->m_intrTag = NULL;
	pTask->m_interruptThread = NULL;

	// find free raw spu
	for (spu = 0; spu < m_numSpus; spu++)
	{
		if (!m_spuInUse[spu])
		{
			break;
		}
	}

	// check we found free spu
	if (spu == m_numSpus)
	{
		res = 1;
		goto xit;
	}

	// Loading an SPU program to the Raw SPU.
	//if (sys_raw_spu_load(m_spuIds[spu], path, &entry) != SUCCEEDED) 

	sprintf(modPath, "%s/%s", g_pPS3PathInfo->PrxPath(), path);
	path = modPath;

    if(strstr(path,".self"))
    {
        ret = sys_spu_image_open(&img, path);
		if(ret != CELL_OK)
		{
			// (Running on Main Thread)
			Error("Failed to open SPU program: %s\n", path);
		}
    }
    else
    {
        // Allocate mem for SPU prog

        CellFsStat stat;
        cellFsStat(path,&stat);
        pSpuProg = memalign(4096,((uint32)stat.st_size + 0x7f)&0xffffff80);
        fp = fopen(path, "rb");
        fread(pSpuProg, 1, stat.st_size, fp );
        fclose(fp);

        ret = sys_spu_image_import(&img, pSpuProg, SYS_SPU_IMAGE_PROTECT);
        if (ret != CELL_OK)
        {
            res = 1;
            goto xit;
        } 
    }

    ret = sys_raw_spu_image_load(m_spuIds[spu], &img);

	spuid = m_spuIds[spu];

	if (ret == CELL_OK)
	{
		// successfully loaded - mark spu as used and fill in o/p
		m_spuInUse[spu] = 1;
		pTask->m_spuId = spuid;
	}
	else
	{
		res = 1;
		goto xit;
	}

	//Free PPU resources used to load image
    if(pSpuProg)
    {
	    free(pSpuProg);
    }
	sys_spu_image_close(&img);

	entry = sys_raw_spu_mmio_read((uint32_t)spuid, (uint32_t)SPU_NPC);

#ifndef _CERT
	g_snRawSPUNotifyElfLoad(spuid, entry, path);
#endif

	// call callback or create default interrupt handler
	if (!pfnCallback)
	{
		res = CreateDefaultInterruptHandler(pTask);
	}
	else
	{
		res = pfnCallback(pTask);
	}

	if (res)
	{
		goto xit;
	}

	// Run the Raw SPU 
	
#ifndef _CERT
	g_snRawSPUNotifySPUStarted(m_spuIds[spu]);
#endif
	sys_raw_spu_mmio_write(spuid, SPU_NPC, entry);
	sys_raw_spu_mmio_write(spuid, SPU_RunCntl, 0x1);
	__asm("eieio");

	// Once the SPU has started, write a mailbox with the effective address of the
	// SPU lock.
	WriteMailbox( pTask, (uint32) &pTask->m_lock );
	WriteMailbox( pTask, (uint32) &pTask->m_memcpyLock );

xit:
    if(res)
    {
        // Error("Error: CreateSpuTask error attempting to load and run %s on SPU\n", path);
    }
	return res;
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

void SpuMgr::DestroySpuTask(SpuTaskHandle *pTask)
{
	if (pTask->m_spuId != -1)
	{
		// Stop the Raw spu

#ifndef _CERT
		g_snRawSPUNotifySPUStopped(pTask->m_spuId);
#endif

		sys_raw_spu_mmio_write(pTask->m_spuId, SPU_RunCntl, 0x0);
		__asm("eieio");

		// Cleanup interrupt handling mechanism

		if (pTask->m_interruptThread)
		{
			sys_interrupt_thread_disestablish(pTask->m_interruptThread);	// also kills the thread
		}
		
		if (pTask->m_intrTag)
		{
			sys_interrupt_tag_destroy(pTask->m_intrTag);
		}
	}
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

int SpuMgr::WriteMailbox(SpuTaskHandle *pTask, uint32 val, bool bBlocking /* =true */)
{
	uint32 mboxAvailable;

	do
	{
		// Check the SPU Mailbox Status Register
		mboxAvailable = sys_raw_spu_mmio_read(pTask->m_spuId, SPU_MBox_Status) & SPU_IN_MBOX_COUNT;
	} while (bBlocking && !mboxAvailable);

	if (mboxAvailable)
		sys_raw_spu_mmio_write(pTask->m_spuId, SPU_In_MBox, (std::uint32_t)val);

	return !mboxAvailable;
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

int SpuMgr::ReadMailbox(SpuTaskHandle *pTask, uint32 *pVal, bool bBlocking /* = true */)
{
	uint32 mailAvailable;

	do
	{
		// Check the SPU Mailbox Status Register
		mailAvailable = sys_raw_spu_mmio_read(pTask->m_spuId, SPU_MBox_Status) & SPU_OUT_MBOX_COUNT;
	} while (bBlocking && !mailAvailable);

	if (mailAvailable)
	{
		// Read the SPU Outbound Mailbox Register
		*pVal = sys_raw_spu_mmio_read(pTask->m_spuId, SPU_Out_MBox);
	}

	return !mailAvailable;
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

int SpuMgr::ReadIntrMailbox(SpuTaskHandle *pTask, uint32 *pVal, bool bBlocking /* = true */)
{
	uint32 mailAvailable;

	do
	{
		// Check the SPU Mailbox Status Register
		mailAvailable = sys_raw_spu_mmio_read(pTask->m_spuId, SPU_MBox_Status) & SPU_OUT_INTR_MBOX_COUNT;

	} while (bBlocking && !mailAvailable);

	if (mailAvailable)
	{
		// Read the SPU Outbound Mailbox Register
		sys_raw_spu_read_puint_mb(pTask->m_spuId, pVal);
	}

	return !mailAvailable;
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

bool SpuMgr::Lock( SpuTaskHandle *pTask )
{
	return cellAtomicCompareAndSwap32( &pTask->m_lock, 0, 1 ) == 0;
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

void SpuMgr::Unlock( SpuTaskHandle *pTask )
{
	cellAtomicCompareAndSwap32( &pTask->m_lock, 1, 0 );
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

bool SpuMgr::MemcpyLock( SpuTaskHandle *pTask )
{
	return cellAtomicCompareAndSwap32( &pTask->m_memcpyLock, 0, 1 ) == 0;
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

void SpuMgr::MemcpyUnlock( SpuTaskHandle *pTask )
{
	cellAtomicCompareAndSwap32( &pTask->m_memcpyLock, 1, 0 );
}

