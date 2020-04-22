//========== Copyright © Valve Corporation, All rights reserved. ========
//
// This job chain wrapper is an infinite job chain with dynamic job submission
// It is single-threaded and not very fast in the current implementation,
//   so it'll need to be optimized if we ever become SPU bound and use it often.
// To make it multithreaded, the easiest way is to lock every operation
// with a compare-exchange flag and a spin-wait, and use getllar to update "this" and the flag atomically
//
#if !defined( VJOBSCHAINU4_HDR ) && defined( _PS3 )
#define VJOBSCHAINU4_HDR

#include <cell/spurs.h>
#include "ps3/job_notify.h"
#include "ps3/spu_job_shared.h"
struct VJobsRoot;


struct ALIGN128 VjobChain4BufferHeader_t
{
public:
	CellSpursJob64 m_jobNotify;
	job_notify::NotifyArea_t m_notifyArea;
}
ALIGN128_POST;

struct VjobChain4Buffer_t: public VjobChain4BufferHeader_t
{
public:
	enum ConstEnum_t
	{
		VERBATIM_COMMAND_COUNT = 2 // we employ syncronization scheme: SYNC, JOB(notify),  ... 
	};
	uint64 m_spursCommands[32]; // there will be at least verbatim commands, a user command, and a NEXT

	void Init( VJobsRoot * pRoot, cell::Spurs::JobChain * pSpursJobChain, uint nMaxCommandsPerBuffer );
};

// VjobChain4 has only 1 jobchain but double-buffers it to facilitate continuous wait-free execution
class ALIGN16 VjobChain4
{
protected:
	enum ConstEnum_t {
		BUFFER_COUNT = 4
	};
	VjobChain4 * m_eaThis;
	cell::Spurs::JobChain *m_pSpursJobChain;
	VjobChain4Buffer_t * m_pBuffers[BUFFER_COUNT];
	VjobChain4Buffer_t * m_pFrontBuffer;
	uint m_nFrontBuffer; // the buffer currently in use
	
	uint m_nMaxCommandsPerBuffer; // max count of commands fitting into one buffer
	uint m_nFrontBufferCommandCount; // count of commands in the current front buffer
	uint m_nSpinWaitNotify; // did we spin waiting for job_notify ? if we did, we probably need to increase the command buffer size

	
	const char * m_pName;

public:
	#ifndef SPU
	int Init( VJobsRoot * pRoot, uint nMaxContention, uint nMinCommandsPerBuffer, uint8_t nVjobChainPriority[8], uint nSizeOfJobDescriptor, uint nMaxGrabbedJob, const char* pName, uint nDmaTags );
	void Shutdown(){End();Join();}
	void End();
	void Join();
	void Run(){ m_pSpursJobChain->run();}
	#endif
	uint64* Push( );
	const char * GetName()const {return m_pName;}

protected:
	void WaitForEntryNotify( VjobChain4Buffer_t * eaBuffer );
	uint64* StartCommandBuffer( uint nNext1Buffer );
	uint64* SwapCommandBuffer(  );
}
ALIGN128_POST;



inline uint64* VjobChain4::Push(  )
{
	uint64 * pInsertionPoint;
	Assert( m_nFrontBufferCommandCount < m_nMaxCommandsPerBuffer );
	if( m_nFrontBufferCommandCount == m_nMaxCommandsPerBuffer - 1 )
	{
		// time to switch the buffer
		pInsertionPoint = SwapCommandBuffer(  );
	}
	else
	{
		Assert( VjobDmaGetUint64( (uint)&m_pFrontBuffer->m_spursCommands[ m_nFrontBufferCommandCount + 1 ], DMATAG_SYNC, 0, 0 ) == CELL_SPURS_JOB_COMMAND_JTS );
		LWSYNC_PPU_ONLY();	// Important: this sync ensures that both the command header AND JTS are written before SPU sees them
		pInsertionPoint = &m_pFrontBuffer->m_spursCommands[ m_nFrontBufferCommandCount ];
		m_nFrontBufferCommandCount ++;
	}
	return pInsertionPoint;
}

extern vec_uint4 g_cellSpursJts16;

extern void FillSpursJts( uint64 * eaCommands, uint nBufferCount );

#endif