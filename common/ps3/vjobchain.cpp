//========== Copyright © Valve Corporation, All rights reserved. ========

#include "vjobchain.h"
#include "vjobutils.h"
#include "vjobs/root.h"
#include "tier1/strtools.h"
#include "tier0/miniprofiler.h"

CMiniProfiler g_mpRun, g_mpJoin, g_mpPush, g_mpPush2;

int VjobChain::Init( VJobsRoot * pRoot, uint nMaxContention, const char* pFormatName, ... )
{
	Assert( !( uintp( &m_guard ) & 0x7F ) );
	{
		va_list args
		va_start( args, pFormatName );
		V_vsnprintf( m_name, sizeof( m_name ), pFormatName, args );
	}
	
	cell::Spurs::JobChainAttribute attr;
	uint8_t nVjobChainPriority[8] = {0,12,12,12,12,12,0,0}; // priority lower than the main job queue, in order to yield
	attr.initialize( &attr, m_headBlock, 128, 1, nVjobChainPriority, nMaxContention, true, 0,1, false, 256, 1 );
	attr.setName( m_name );
	m_pLastBlock = NULL; // NOT running by default
	CELL_MUST_SUCCEED( JobChain::createWithAttribute( &pRoot->m_spurs, &m_spursJobChain, &attr ) );
	
	V_memset( &m_notifyArea, 0, sizeof( m_notifyArea ) );
	V_memset( &m_jobNotify, 0, sizeof( m_jobNotify ) );
	m_jobNotify.header = *( pRoot->m_pJobNotify );
	m_jobNotify.header.useInOutBuffer = 1;
	Assert( !( uint( &m_jobNotify ) & 63 ) ); // should be 64-byte aligned
	AddInputDma( &m_jobNotify, sizeof( m_notifyArea ), &m_notifyArea );
	m_notifyArea.m_nCopyFrom = 1;
	m_notifyArea.m_nCopyTo   = 0;	  // SPU will mark copyTo = 1, PPU will mark it back to 0; at this time, we may actually mark the notify as completed
	m_jobNotify.workArea.userData[1] = 0; // function: default 
	
	
	CELL_MUST_SUCCEED( m_guard.initialize( &m_spursJobChain, &m_guard, 1 /*notifyCount*/, 1 /*requestSpuCount(ignored)*/, 1 /*autoReset*/ ) );
	
	m_headBlock[0] = CELL_SPURS_JOB_COMMAND_SYNC; // wait for all previous list commands to finish
	m_headBlock[1] = CELL_SPURS_JOB_COMMAND_JOB( &m_jobNotify ); 
	m_headBlock[2] = CELL_SPURS_JOB_COMMAND_GUARD( &m_guard );
	m_headBlock[BLOCK_COMMANDS] = ( uint64 )-1ll;
	
	CELL_MUST_SUCCEED( m_spursJobChain.run() );
	
	m_nSpinWaitNotify = 0;
	return CELL_OK;
}

int VjobChain::Run()
{
	if( !IsRunning() )
	{
		CMiniProfilerGuard mpg( &g_mpRun );
		Assert( m_notifyArea.m_nCopyTo ); // the jobchain must be joined if its not in Running state
		m_notifyArea.m_nCopyTo = 0;
		m_pLastBlock = m_headBlock;
		m_nCurrentBlockCommands = 3; // right after the SYNC-JOB(notify)-GUARD prefix 
		m_headBlock[m_nCurrentBlockCommands] = CELL_SPURS_JOB_COMMAND_JTS;
		// __lwsync(); // make sure we complete sync reset and write JTS before notify - probably not necessary because the guard should have a barrier for sure
		m_guard.notify(); // let the jobchain go
		
		return CELL_OK;
	}
	else
	{
		return CELL_OK; // it's valid to try to run a running chain in our interface...
	}
}




int VjobChain::End( )
{
	if( IsRunning() )
	{
		Assert( m_pLastBlock[m_nCurrentBlockCommands] == CELL_SPURS_JOB_COMMAND_JTS );
		Assert( m_notifyArea.m_nCopyTo == 0 ); // make sure we reset sync correctly
		m_pLastBlock[m_nCurrentBlockCommands] = CELL_SPURS_JOB_COMMAND_RESET_PC( m_headBlock );
		return 0;
	}
	else
		return -1; // you should not end non-running instance
}

void VjobChain::Shutdown()
{
	if( IsRunning() )
	{
		m_pLastBlock[m_nCurrentBlockCommands] = CELL_SPURS_JOB_COMMAND_END;
	}
	m_spursJobChain.shutdown();
	m_spursJobChain.join();
}


int VjobChain::Join()
{
	if( IsRunning() )
	{
	#ifdef _DEBUG
		CellSpursJobChainInfo info;
		m_spursJobChain.getInfo( &info );
	#endif
		CMiniProfilerGuard mpg( &g_mpJoin );
		
		// wait for reset sync notification to come through
		volatile job_notify::NotifyArea_t *pNotify = &m_notifyArea;
		Assert( pNotify->m_nCopyFrom );
		while( !pNotify->m_nCopyTo )
		{
			++m_nSpinWaitNotify;
		}
		if( m_nSpinWaitNotify )
		{
			// <HACK> Sergiy : I'm taking this out for now because jobchain double-buffering is effectively temporarily hosed
			// Warning( "VjobChain %s: stall in join, %d spins\n", m_name, m_nSpinWaitNotify );
			m_nSpinWaitNotify = 0;
		}

		// free up the memory of the jobs that are now known to have dispatched
		if( m_headBlock != m_pLastBlock )
		{
			uint64 *pBlock = m_headBlock;
			do
			{
				Assert( pBlock[BLOCK_COMMANDS] == (uint64)-1ll );
				uint64 eaNext = pBlock[BLOCK_COMMANDS - 1];
				Assert( ( eaNext & 0xFFFFFFFF00000007ull ) == 3 );
				pBlock = ( uint64 * )( uintp( eaNext ) & ~7 );
				Assert( pBlock[BLOCK_COMMANDS] == (uint64)-1ll );
				delete[]pBlock;
			}
			while( pBlock != m_pLastBlock );
		}
		m_pLastBlock = NULL; // idle state
		return CELL_OK;
	}
	else
		return 0; // valid to join twice;
}


void VjobChain::Push( uint64 nCommand )
{
	Assert( IsRunning() );
	uint64 * pNextCommand = &m_pLastBlock[m_nCurrentBlockCommands++]; // JTS to patch
	if( m_nCurrentBlockCommands < BLOCK_COMMANDS )
	{
		CMiniProfilerGuard mpg( &g_mpPush );
		pNextCommand[1] = CELL_SPURS_JOB_COMMAND_JTS;
		__lwsync(); // ordering: create JobHeader, insert next JTS --> patch current JTS with JOB command
		*pNextCommand = nCommand;
	}
	else
	{
		CMiniProfilerGuard mpg( &g_mpPush2 );
		m_pLastBlock = new uint64[BLOCK_COMMANDS+1];
		m_pLastBlock[BLOCK_COMMANDS] = ( uint64 )-1ll; // marker
		m_pLastBlock[0] = nCommand;
		m_pLastBlock[m_nCurrentBlockCommands = 1] = CELL_SPURS_JOB_COMMAND_JTS;
		__lwsync();	// ordering: create JobHeader, allocate & reset new segment with JOB command in it --> patch JTS in old segment with NEXT command
		*pNextCommand = CELL_SPURS_JOB_COMMAND_NEXT( m_pLastBlock );
	}
}

void VjobChain::Push( const uint64 * nCommands, uint nCommandCount )
{
	// todo: make it more optimal by removing extra lwsync's
	for( uint i = 0; i < nCommandCount; ++i )
	{
		Push( nCommands[i] );
	}
}


int VjobChain2::Init( VJobsRoot * pRoot, uint nMaxContention, const char* pName )
{
	m_vjobChainRing = ( VjobChain * )MemAlloc_AllocAligned( VJOB_CHAINS * sizeof( VjobChain ), 128 );

	for( uint i = 0; i < 2; ++i )
	{
		int nError = m_vjobChainRing[i].Init( pRoot, nMaxContention, "%s%d", pName, i );
		if(	nError )
			return nError;
	}

	m_nCurrentChain = 0;
	return 0;		
}


void VjobChain2::Begin()
{
	m_vjobChainRing[( m_nCurrentChain + 1 ) % VJOB_CHAINS].Join();
	VjobChain & jobchain = Jobchain();
	jobchain.Join(); // join the jobchain that we'll be using now
	jobchain.Run();
}

void VjobChain2::End()
{
	VjobChain & jobchain = Jobchain();
	jobchain.End();
	m_nCurrentChain = ( m_nCurrentChain + 1 ) % VJOB_CHAINS; // swap the job chain
}

void VjobChain2::Shutdown()
{
	for( uint i = 0; i < 2; ++i )
	{
		m_vjobChainRing[i].Shutdown();
	}
	MemAlloc_FreeAligned( m_vjobChainRing );
}



int VjobChain3::Init( VJobsRoot * pRoot, uint nMaxContention, uint nMinCommandsPerBuffer, uint8_t nVjobChainPriority[8], const char* pName, uint nDmaTags )
{
	m_pName = pName;
	const uint nSizeOfJobDescriptor = 128, nMaxGrabbedJob = 4;
	// we need at least 4 commands
	uint nBufferSize = sizeof( VjobBufferHeader_t ) + sizeof( uint64 ) * MAX( nMinCommandsPerBuffer, VjobBuffer_t::VERBATIM_COMMAND_COUNT + 2 ); // +2 is for user's command and JTN
	nBufferSize = AlignValue( nBufferSize, 128 );
	m_nMaxCommandsPerBuffer = ( nBufferSize - sizeof( VjobBufferHeader_t ) ) / sizeof( uint64 );
	
	uint nAllocationSize = sizeof( cell::Spurs::JobChain ) + nBufferSize * BUFFER_COUNT;
	m_pSpursJobChain = ( cell::Spurs::JobChain* )MemAlloc_AllocAligned( nAllocationSize, 128 );
	V_memset( m_pSpursJobChain, 0, nAllocationSize );
	m_pBuffers[0] = ( VjobBuffer_t * )( m_pSpursJobChain + 1 );
	m_nFrontBuffer = 0;
	m_pFrontBuffer = m_pBuffers[0];
	for( int i = 1; i < BUFFER_COUNT; ++i )
	{
		m_pBuffers[i] = ( VjobBuffer_t * )( uintp( m_pBuffers[ i - 1 ] ) + nBufferSize );
	}
	
	cell::Spurs::JobChainAttribute attr;
	attr.initialize( &attr, m_pFrontBuffer->m_spursCommands, nSizeOfJobDescriptor, nMaxGrabbedJob, nVjobChainPriority, nMaxContention, true, nDmaTags, nDmaTags + 1, false, 256, 1 );
	attr.setName( pName );
	CELL_MUST_SUCCEED( JobChain::createWithAttribute( &pRoot->m_spurs, m_pSpursJobChain, &attr ) );

	for( int i = 0; i < BUFFER_COUNT; ++i )
	{
		Assert( !( uintp( m_pBuffers[i] ) & 0x7F ) );
		m_pBuffers[i]->Init( pRoot, m_pSpursJobChain );
	}
	
	StartCommandBuffer( 0, CELL_SPURS_JOB_COMMAND_NOP );
	CELL_MUST_SUCCEED( m_pSpursJobChain->run() );
	
	#ifdef _DEBUG
	sys_timer_usleep( 100 );
	#endif

	m_nSpinWaitNotify = 0;
	return CELL_OK;
}



void VjobBuffer_t::Init( VJobsRoot * pRoot, cell::Spurs::JobChain * pSpursJobChain )
{
	m_jobNotify.header = *( pRoot->m_pJobNotify );
	m_jobNotify.header.useInOutBuffer = 1;
	AddInputDma( &m_jobNotify, sizeof( m_notifyArea ), &m_notifyArea );
	m_notifyArea.m_nCopyFrom = 1;
	// SPU will mark copyTo = 1, PPU will mark it back to 0; at this time, we may actually mark the notify as completed; 1 means "previous buffer is free"
	// Then we'll start command buffer, which will reset the ready flag. But then we run the jobchain, which will run job_notify and set the flag back again, thus starting the ring
	m_notifyArea.m_nCopyTo   = 1;	  
	m_jobNotify.workArea.userData[1] = 0; // function: default 

	uint nCommands = 0;
	m_spursCommands[nCommands++] = CELL_SPURS_JOB_COMMAND_SYNC; // wait for all previous list commands to finish
	m_spursCommands[nCommands++] = CELL_SPURS_JOB_COMMAND_JOB( &m_jobNotify ); 
#ifdef VJOBCHAIN3_GUARD
	Assert( !( uintp( &m_guard ) & -128 ) );
	CELL_MUST_SUCCEED( m_guard.initialize( pSpursJobChain, &m_guard, 1 /*notifyCount*/, 1 /*requestSpuCount(ignored)*/, 1 /*autoReset*/ ) );
	m_spursCommands[nCommands++] = CELL_SPURS_JOB_COMMAND_GUARD( &m_guard );
#endif
	#ifdef _DEBUG
	m_jobNotify2.header = *( pRoot->m_pJobNotify );
	m_jobNotify2.header.useInOutBuffer = 1;
	AddInputDma( &m_jobNotify2, sizeof( m_notifyArea2 ), &m_notifyArea2 );
	m_jobNotify2.workArea.userData[1] = 0; // function: default 
	m_notifyArea2.m_nCopyFrom = 1;
	m_notifyArea2.m_nCopyTo   = 0; // just for debugging, to see when this job gets executed
	m_spursCommands[nCommands++] = CELL_SPURS_JOB_COMMAND_JOB( &m_jobNotify2 );
	#endif
	Assert( nCommands == VjobBuffer_t::VERBATIM_COMMAND_COUNT );
	m_spursCommands[VjobBuffer_t::VERBATIM_COMMAND_COUNT] = CELL_SPURS_JOB_COMMAND_JTS;
}



void VjobChain3::WaitForEntryNotify( VjobBuffer_t * pBuffer )
{
	volatile job_notify::NotifyArea_t *pNotify = &pBuffer->m_notifyArea;
	Assert( pNotify->m_nCopyFrom );
	while( !pNotify->m_nCopyTo )
	{
		++m_nSpinWaitNotify;
		sys_timer_usleep( 30 );
	}
	if( m_nSpinWaitNotify )
	{
		Warning( "VjobChain %s: stall in WaitForEntryNotify, %d spins\n", m_pName, m_nSpinWaitNotify );
		m_nSpinWaitNotify = 0;
	}
}



uint64* VjobChain3::SwapCommandBuffer( uint64 nInsertCommand )
{
	uint64 * pSpursIsSpinningHere = &m_pFrontBuffer->m_spursCommands[ m_nFrontBufferCommandCount ];
	
	Assert( m_nFrontBufferCommandCount < m_nMaxCommandsPerBuffer );
	uint nNext1Buffer = ( m_nFrontBuffer + 1 ) % BUFFER_COUNT, nNext2Buffer = ( m_nFrontBuffer + 2 ) % BUFFER_COUNT;
	VjobBuffer_t * pNext1Buffer = m_pBuffers[ nNext1Buffer ], * pNext2Buffer = m_pBuffers[ nNext2Buffer ];
	
	// before we can declare the next1 buffer "front", we need to make sure it's fully ready to accept commands, i.e. that it was fully read by SPURS
	// for that, we check the next2 buffer notification area
	
	WaitForEntryNotify( pNext1Buffer );
	WaitForEntryNotify( pNext2Buffer );

	// if next2 buffer has been notified, next1 must have been notified long ago
	Assert( pNext1Buffer->m_notifyArea.m_nCopyTo );
	uint64* pInsertionPoint = StartCommandBuffer( nNext1Buffer, nInsertCommand );
	Assert( pNext1Buffer == m_pFrontBuffer );
	// implicit lwsync is here
	*pSpursIsSpinningHere = CELL_SPURS_JOB_COMMAND_NEXT( pNext1Buffer->m_spursCommands ); // jump to the next buffer
	
	return pInsertionPoint;
}
	
	
uint64* VjobChain3::StartCommandBuffer( uint nNext1Buffer, uint64 nInsertCommand )
{
	m_nFrontBuffer = nNext1Buffer;
	m_pFrontBuffer = m_pBuffers[ nNext1Buffer ];
	// the ready marker is presumed to be present; SPURS must have gone through this buffer in the previous ring, otherwise we can't use it
	Assert( m_pFrontBuffer->m_notifyArea.m_nCopyTo == 1 );
	// reset the ready marker; SPURS didn't get to this buffer yet (we're about to reuse them and we didn't jump to it yet)
	m_pFrontBuffer->m_notifyArea.m_nCopyTo = 0; 
	#ifdef _DEBUG
	m_pFrontBuffer->m_notifyArea2.m_nCopyFrom++;
	m_pFrontBuffer->m_notifyArea2.m_nCopyTo = 0;
	#endif
	uint64 * pCommand = &m_pFrontBuffer->m_spursCommands[ VjobBuffer_t::VERBATIM_COMMAND_COUNT ];
	*pCommand = nInsertCommand;
	m_pFrontBuffer->m_spursCommands[ VjobBuffer_t::VERBATIM_COMMAND_COUNT + 1 ] = CELL_SPURS_JOB_COMMAND_JTS;
	m_nFrontBufferCommandCount = VjobBuffer_t::VERBATIM_COMMAND_COUNT + 1;
	#ifdef VJOBCHAIN3_GUARD
	m_pFrontBuffer->m_guard.notify(); // let the jobchain go through
	// implicit lwsync is here
	#else
	__lwsync();
	#endif
	return pCommand;
}


void VjobChain3::End()
{
	Assert( m_nFrontBufferCommandCount < m_nMaxCommandsPerBuffer );
	m_pFrontBuffer->m_spursCommands[ m_nFrontBufferCommandCount ] = CELL_SPURS_JOB_COMMAND_END;
	m_pSpursJobChain->shutdown();
}

void VjobChain3::Join()
{
	Assert( m_pFrontBuffer->m_spursCommands[ m_nFrontBufferCommandCount ] == CELL_SPURS_JOB_COMMAND_END );
	m_pSpursJobChain->join();
	
	MemAlloc_FreeAligned( m_pSpursJobChain );
	m_pSpursJobChain = NULL;
}
