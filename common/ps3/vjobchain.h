//========== Copyright © Valve Corporation, All rights reserved. ========
#if !defined( VJOBS_CHAINUTILS_HDR ) && defined( _PS3 )
#define VJOBS_CHAINUTILS_HDR

#include "tier0/platform.h"
#include <cell/spurs.h>
#include "ps3/job_notify.h"
struct VJobsRoot;

//
// The chain consists of blocks of commands; the head block has SYNC-JOB(notify)-GUARD sequence
// initially the chain waits on the GUARD , and is ready to "run". "Run" means releasing the guard.
// I'm using GUARD instead of JTS because patching JTS will render it unpatcheable until jobchain execution completes;
// and if it's unpatcheable, it effectively can't be used as a guard for the next cycle 
// Each block refers to the next one by inserting NEXT in the very last slot
// the last block is pointed to by m_pLastBlock, which is NULL initially (before entering the "run" state) 
// 
//
struct ALIGN128 VjobChain
{
public:
	cell::Spurs::JobChain m_spursJobChain;
	cell::Spurs::JobGuard m_guard;
	CellSpursJob64 m_jobNotify;
	job_notify::NotifyArea_t m_notifyArea;
	
	enum { BLOCK_COMMANDS = 256 };
	uint64 m_headBlock[BLOCK_COMMANDS+1]; // in one variant, the first entry in this list is used for END command; overwrite it with NOP to release the list
	uint64 * m_pLastBlock;
	uint m_nCurrentBlockCommands;
	uint m_nSpinWaitNotify;
	
	char m_name[16];
public:
	int Init( VJobsRoot * pRoot, uint nMaxContention, const char* pFormatName, ... );
	bool IsRunning()const { return m_pLastBlock != NULL; }
	
	int Run();
	void Push( uint64 nCommand );
	void Push( const uint64 * nCommands, uint nCommandCount );
	int End( );
	int Join();
	void Shutdown();
	
	JobChain & Jobchain() { return m_spursJobChain; }
	
}ALIGN128_POST;


// VjobChain2 hosts 2 jobchains and double-buffers between them
class VjobChain2
{
public:
	int Init( VJobsRoot * pRoot, uint nMaxContention, const char* pName );
	void Begin();															 
	void End();
	void Shutdown();

	VjobChain& Jobchain(){ return m_vjobChainRing[m_nCurrentChain]; }

protected:
	enum{VJOB_CHAINS = 2};
	VjobChain *m_vjobChainRing; // may be more than double-buffered if necessary
	uint m_nCurrentChain;
};


//#define VJOBCHAIN3_GUARD

struct ALIGN128 VjobBufferHeader_t
{
public:
	#ifdef VJOBCHAIN3_GUARD
	cell::Spurs::JobGuard m_guard;
	#endif
	CellSpursJob64 m_jobNotify;
	job_notify::NotifyArea_t m_notifyArea;
	#ifdef _DEBUG
	CellSpursJob64 m_jobNotify2;
	job_notify::NotifyArea_t m_notifyArea2;
	#endif
}
ALIGN128_POST;

struct VjobBuffer_t: public VjobBufferHeader_t
{
public:
	enum ConstEnum_t
	{
		VERBATIM_COMMAND_COUNT = 2 // we employ syncronization scheme: SYNC, JOB(notify),  ... 
	#ifdef VJOBCHAIN3_GUARD // we add GUARD, ...
		+ 1
	#endif
	#ifdef _DEBUG
		+ 1 // we add JOB(notify2), ... 
	#else
		
	#endif
	};
	uint64 m_spursCommands[16]; // there will be at least verbatim commands, a user command, and a NEXT

	void Init( VJobsRoot * pRoot, cell::Spurs::JobChain * pSpursJobChain );
};

// VjobChain3 has only 1 jobchain but double-buffers it to facilitate continuous wait-free execution
class VjobChain3
{
protected:
	enum ConstEnum_t {
		BUFFER_COUNT = 4
	};
	cell::Spurs::JobChain *m_pSpursJobChain;
	VjobBuffer_t * m_pBuffers[BUFFER_COUNT];
	VjobBuffer_t * m_pFrontBuffer;
	uint m_nFrontBuffer; // the buffer currently in use
	
	uint m_nMaxCommandsPerBuffer; // max count of commands fitting into one buffer
	uint m_nFrontBufferCommandCount; // count of commands in the current front buffer
	uint m_nSpinWaitNotify; // did we spin waiting for job_notify ? if we did, we probably need to increase the command buffer size
	uint64 m_nLastCommandPushed; // at the beginning of the scene, it's considered to be synced up

	
	const char * m_pName;

public:

	int Init( VJobsRoot * pRoot, uint nMaxContention, uint nMinCommandsPerBuffer, uint8_t nVjobChainPriority[8], const char* pName, uint nDmaTags );
	uint64* Push( uint64 nCommand );
	uint64* PushSyncJobSync( uint64 nCommand );
	void PushSync();
	void Shutdown(){End();Join();}
	void End();
	void Join();

protected:
	void WaitForEntryNotify( VjobBuffer_t * pBuffer );
	uint64* StartCommandBuffer( uint nNext1Buffer, uint64 nInsertCommand );
	uint64* SwapCommandBuffer( uint64 nInsertCommand );
};



inline uint64* VjobChain3::Push( uint64 nCommand )
{
	uint64 * pInsertionPoint;
	if( m_nFrontBufferCommandCount == m_nMaxCommandsPerBuffer - 1 )
	{
		// time to switch the buffer
		pInsertionPoint = SwapCommandBuffer( nCommand );
	}
	else
	{
		m_pFrontBuffer->m_spursCommands[ m_nFrontBufferCommandCount + 1 ] = CELL_SPURS_JOB_COMMAND_JTS;
		__lwsync();	// Important: this sync ensures that both the command header AND JTS are written before SPU sees them
		pInsertionPoint = &m_pFrontBuffer->m_spursCommands[ m_nFrontBufferCommandCount ];
		*pInsertionPoint = nCommand;
		m_nFrontBufferCommandCount ++;
	}
	m_nLastCommandPushed = nCommand;
	return pInsertionPoint;
}


inline void VjobChain3::PushSync()
{
	Push( CELL_SPURS_JOB_COMMAND_LWSYNC );
}


inline uint64* VjobChain3::PushSyncJobSync( uint64 nCommand )
{
	if( m_nLastCommandPushed != CELL_SPURS_JOB_COMMAND_LWSYNC )
	{
		// we need to wait for previous jobs to finish in order to patch the state efficiently
		// todo: double-buffer the states to avoid stalls, but only if we become SPU-bound here (un
		Push( CELL_SPURS_JOB_COMMAND_LWSYNC );
	}
	uint64 * pInsertionPoint = Push( nCommand );

	// this is instead of stalling successor because I'm not sure if it stalls all logical successors (some of which may be picked up by other SPUs)
	// the SYNC here will ensure completion of the previous job before the new jobs will be pushed
	Push( CELL_SPURS_JOB_COMMAND_LWSYNC );
	
	return pInsertionPoint;
}



#endif