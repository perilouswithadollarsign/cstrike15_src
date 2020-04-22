//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
// Raw SPU management
//
//==================================================================================================

#ifndef INCLUDED_SPUMGR_PPU_H
#define INCLUDED_SPUMGR_PPU_H

//--------------------------------------------------------------------------------------------------
// Headers
//--------------------------------------------------------------------------------------------------

#include <sys/spu_initialize.h>
#include <sys/raw_spu.h>
#include <sys/spu_utility.h>
#include <sys/ppu_thread.h>
#include <sys/interrupt.h>
#include <sys/raw_spu.h>
#include <sys/sys_time.h>

#include <cell/spurs.h>

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

extern "C"
{
	extern void (*g_snRawSPULockHandler) (void);
	extern void (*g_snRawSPUUnlockHandler) (void);
	extern void (*g_snRawSPUNotifyCreation) (unsigned int uID);
	extern void (*g_snRawSPUNotifyDestruction) (unsigned int uID);
	extern void (*g_snRawSPUNotifyElfLoad) (unsigned int uID, unsigned int uEntry, const char *pFileName);
	extern void (*g_snRawSPUNotifyElfLoadNoWait) (unsigned int uID, unsigned int uEntry, const char *pFileName);
	extern void (*g_snRawSPUNotifyElfLoadAbs) (unsigned int uID, unsigned int uEntry, const char *pFileName);
	extern void (*g_snRawSPUNotifyElfLoadAbsNoWait) (unsigned int uID, unsigned int uEntry, const char *pFileName);
	extern void (*g_snRawSPUNotifySPUStopped) (unsigned int uID);
	extern void (*g_snRawSPUNotifySPUStarted) (unsigned int uID);
};

//--------------------------------------------------------------------------------------------------
// Fwd refs
//--------------------------------------------------------------------------------------------------

class CellSpurs2;
class SpuTaskHandle;

//--------------------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------------------

#define MAX_RAW_SPUS 5

// Class 2 Interrupt Status Register (INT_Stat_class2)
// Described in CBE architecture v10 on page 259
#define INTR_PPU_MB_SHIFT   0
#define INTR_STOP_SHIFT     1
#define INTR_HALT_SHIFT     2
#define INTR_DMA_SHIFT      3
#define INTR_SPU_MB_SHIFT   4 
#define INTR_PPU_MB_MASK    (0x1 << INTR_PPU_MB_SHIFT)
#define INTR_STOP_MASK      (0x1 << INTR_STOP_SHIFT)
#define INTR_HALT_MASK      (0x1 << INTR_HALT_SHIFT)
#define INTR_DMA_MASK       (0x1 << INTR_DMA_SHIFT)
#define INTR_SPU_MB_MASK    (0x1 << INTR_SPU_MB_SHIFT)

// thread priority for interrupt handler threads
#define INTR_HANDLER_THREAD_PRIORITY	200
#define INTR_HANDLER_THREAD_STACK_SIZE  0x4000

#define SPUMGR_IS_ALIGNED(val, align)	(((val) & ((align) - 1)) == 0)
#define SPUMGR_ALIGN_UP(val, align)		(((val) + ((align)-1)) & ~((align) - 1))
#define SPUMGR_ALIGN_DOWN(val, align)	((val) & ~((align) - 1))

//--------------------------------------------------------------------------------------------------
// Overide sys_raw_spu_mmio_read / write, since they draw out another bug in SNC :(
//--------------------------------------------------------------------------------------------------

#define sys_raw_spu_mmio_read(spu, regoffset) spumgr_mmio_read(spu, regoffset)
extern uint32_t spumgr_mmio_read(uint32_t spu, uint32_t regoffset);

#define sys_raw_spu_mmio_write(spu, regoffset, value) spumgr_mmio_write(spu, regoffset, value)
extern void spumgr_mmio_write(int id, int offset, uint32_t value);

//--------------------------------------------------------------------------------------------------
// Types
//--------------------------------------------------------------------------------------------------

typedef int CreateSPUTaskCallback(SpuTaskHandle *pTask);

// SpuStatusRegister
// Described in CBE architecture v10 on page 87
typedef union SpuStatusRegister
{
	struct
	{
		uint32_t	m_sc								: 16;
		uint32_t	m_reserved2							: 5;
		uint32_t	m_isolateExitStatus					: 1;
		uint32_t	m_isolateLoadStatus					: 1;
		uint32_t	m_reserved1							: 1;
		uint32_t	m_isolationStatus					: 1;
		uint32_t	m_illegalChannelInstructionDetected	: 1;
		uint32_t	m_invalidInstructionDetected		: 1;
		uint32_t	m_singleStepStatus					: 1;
		uint32_t	m_waitStatus						: 1;
		uint32_t	m_haltStatus						: 1;
		uint32_t	m_programStopAndSignalStatus		: 1;
		uint32_t	m_runStatus							: 1;
	};
	uint32_t	m_val;
} SpuStatusRegister;

//--------------------------------------------------------------------------------------------------
// Classes
//--------------------------------------------------------------------------------------------------

class SpuTaskHandle
{
public:
	sys_raw_spu_t					m_spuId;
	sys_ppu_thread_t				m_ppuThread;
	sys_interrupt_tag_t				m_intrTag;
	sys_interrupt_thread_handle_t	m_interruptThread;
	uint32_t							m_lock;
	uint32_t							m_memcpyLock;
};	

//--------------------------------------------------------------------------------------------------
// SpuMgr
// 
// Provides functionality for running raw spu tasks. For this purpose it creates
// and manages a raw spu pool
// 
// Currently we assume a simple setup where app loads an elf on to a raw spu,
// after which the spu starts running the elf and continues to do so thereafter.
// The ppu->spu and spu->ppu communication is explicitly handled by the app 
// and the spu program using SpuMgr methods
// 
// Currently all DMA transfer is supposed to be initiated by the SPUs which is 
// why SpuMgr does not provide any DMA functionality
//--------------------------------------------------------------------------------------------------

class SpuMgr
{
public:

	// Init/Term

	int Init(int numRawSpu);	
	void Term();

	// Create/Destroy tasks

	int CreateSpuTask(const char *path, SpuTaskHandle *pTask, CreateSPUTaskCallback *pfnCallback = NULL);	
	void DestroySpuTask(SpuTaskHandle *pTask);

	//
	// Helper functions to communicate with the SPU
	// As we build more functionality into the SPU mgr it is
	// possible that we will need to expose less of 
	// these low-level functions
	//

	//
	// Mailbox functions
	//

	//
	// The SPU Inbound Mailbox is a 4-level FIFO structure for communication from the 
	// PPU to SPU, and can	hold up to four 32-bit messages. 
	// If there are already four messages in the mailbox the last message will be 
	// overwritten...but we can check for a full mailbox and prevent this
	int WriteMailbox(SpuTaskHandle *pTask, uint32_t val, bool bBlocking = true);

	// The SPU Outbound Mailbox can hold one 32-bit message for SPU-to-PPU communication.
	int ReadMailbox(SpuTaskHandle *pTask, uint32_t *pVal, bool bBlocking = true);

	// The SPU Outbound Interrupt Mailbox can hold one 32-bit message for SPU-to-PPU communication.
	int ReadIntrMailbox(SpuTaskHandle *pTask, uint32_t *pVal, bool bBlocking = true);
	//
	// Access to local store - note that this involves MMIO which will be slow
	// so need to use DMA instead for any significant data transfer. This
	// mechanism may be useful for writing some small amount of data such
	// as some constants etc into LS
	//

	int WriteLS(SpuTaskHandle *pTask, uint32_t lsOffset, void *pData, uint32_t size);
	int ReadLS(SpuTaskHandle *pTask, uint32_t lsOffset, void *pData, uint32_t size);

	bool Lock( SpuTaskHandle *pTask );
	void Unlock( SpuTaskHandle *pTask );

	bool MemcpyLock( SpuTaskHandle *pTask );
	void MemcpyUnlock( SpuTaskHandle *pTask );


// 	CellSpurs2		m_exclusiveSpusSpurs;		// SPURS instance running on SPUs used exclusively by the application
// 	CellSpurs2		m_preemptedSpuSpurs;		// SPURS instance running on an SPU shared with the OS (may be preempted by it occasionally)

private:

	uint32_t				m_numSpus;
	uint32_t				m_spuInUse[MAX_RAW_SPUS];
	sys_raw_spu_t	m_spuIds[MAX_RAW_SPUS];

	int	ReadMailboxChannel(SpuTaskHandle *pTask, uint32_t *pVal, 
		uint32_t countMask, uint32_t channel, bool bBlocking = true);
};

//--------------------------------------------------------------------------------------------------
// Externs
//--------------------------------------------------------------------------------------------------

extern SpuMgr gSpuMgr;

//--------------------------------------------------------------------------------------------------
// DmaCheckAlignment
//	Checks restrictions specified in SpuMgr::DmaGet
//--------------------------------------------------------------------------------------------------

int DmaCheckAlignment(uint32_t src, uint32_t dest, uint32_t size);


//--------------------------------------------------------------------------------------------------
// GetStopSignal
//--------------------------------------------------------------------------------------------------

inline uint32_t GetStopSignal( sys_raw_spu_t idSpu )
{
	SpuStatusRegister status;
	status.m_val = sys_raw_spu_mmio_read(idSpu, SPU_Status);
	uint32_t stopSignal = status.m_sc;
	return stopSignal;
}

#endif // INCLUDED_SPUMGR_PPU_H
