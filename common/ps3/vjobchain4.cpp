//========== Copyright © Valve Corporation, All rights reserved. ========

#include "ps3/vjobchain4.h"

vec_uint4 g_cellSpursJts16 = ( vec_uint4 ){ uint( CELL_SPURS_JOB_COMMAND_JTS >> 32 ), uint( CELL_SPURS_JOB_COMMAND_JTS ), uint( CELL_SPURS_JOB_COMMAND_JTS >> 32 ), uint( CELL_SPURS_JOB_COMMAND_JTS ) };

#ifndef SPU

#include "ps3/vjobutils.h"
#include "vjobs/root.h"
#include "tier1/strtools.h"
#include "tier0/miniprofiler.h"

int VjobChain4::Init( VJobsRoot * pRoot, uint nMaxContention, uint nMinCommandsPerBuffer, uint8_t nVjobChainPriority[8], uint nSizeOfJobDescriptor, uint nMaxGrabbedJob, const char* pName, uint nDmaTags )
{
	m_pName = pName;
	m_eaThis = this;
	// we need at least 4 commands
	uint nBufferSize = sizeof( VjobChain4BufferHeader_t ) + sizeof( uint64 ) * MAX( nMinCommandsPerBuffer, VjobChain4Buffer_t::VERBATIM_COMMAND_COUNT + 2 ); // +2 is for user's command and JTN
	nBufferSize = AlignValue( nBufferSize, 128 );
	m_nMaxCommandsPerBuffer = ( nBufferSize - sizeof( VjobChain4BufferHeader_t ) ) / sizeof( uint64 );
	
	uint nAllocationSize = sizeof( cell::Spurs::JobChain ) + nBufferSize * BUFFER_COUNT;
	m_pSpursJobChain = ( cell::Spurs::JobChain* )MemAlloc_AllocAligned( nAllocationSize, 128 );
	V_memset( m_pSpursJobChain, 0, nAllocationSize );
	m_pBuffers[0] = ( VjobChain4Buffer_t * )( m_pSpursJobChain + 1 );
	m_nFrontBuffer = 0;
	m_pFrontBuffer = m_pBuffers[0];
	for( int i = 1; i < BUFFER_COUNT; ++i )
	{
		m_pBuffers[i] = ( VjobChain4Buffer_t * )( uintp( m_pBuffers[ i - 1 ] ) + nBufferSize );
	}
	
	cell::Spurs::JobChainAttribute attr;
	attr.initialize( &attr, m_pFrontBuffer->m_spursCommands, nSizeOfJobDescriptor, nMaxGrabbedJob, nVjobChainPriority, nMaxContention, true, nDmaTags, nDmaTags + 1, false, Max<uint>( 256, nSizeOfJobDescriptor ), 1 );
	attr.setName( pName );
	CELL_MUST_SUCCEED( JobChain::createWithAttribute( &pRoot->m_spurs, m_pSpursJobChain, &attr ) );

	for( int i = 0; i < BUFFER_COUNT; ++i )
	{
		Assert( !( uintp( m_pBuffers[i] ) & 0x7F ) );
		m_pBuffers[i]->Init( pRoot, m_pSpursJobChain, m_nMaxCommandsPerBuffer );
	}
	
	*StartCommandBuffer( 0 ) = CELL_SPURS_JOB_COMMAND_NOP;
	CELL_MUST_SUCCEED( m_pSpursJobChain->run() );
	
	#ifdef _DEBUG
	sys_timer_usleep( 100 );
	#endif

	m_nSpinWaitNotify = 0;
	return CELL_OK;
}



void VjobChain4Buffer_t::Init( VJobsRoot * pRoot, cell::Spurs::JobChain * pSpursJobChain, uint nMaxCommandsPerBuffer )
{
	Assert( 0 == ( 0x7F & uintp( &m_jobNotify ) ) );
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
	Assert( nCommands == VjobChain4Buffer_t::VERBATIM_COMMAND_COUNT );
	while( nCommands < nMaxCommandsPerBuffer )
	{
		m_spursCommands[nCommands++] = CELL_SPURS_JOB_COMMAND_JTS;
	}
}


void VjobChain4::End()
{
	Assert( m_nFrontBufferCommandCount < m_nMaxCommandsPerBuffer );
	m_pFrontBuffer->m_spursCommands[ m_nFrontBufferCommandCount ] = CELL_SPURS_JOB_COMMAND_END;
	m_pSpursJobChain->shutdown();
}

void VjobChain4::Join()
{
	Assert( m_pFrontBuffer->m_spursCommands[ m_nFrontBufferCommandCount ] == CELL_SPURS_JOB_COMMAND_END );
	m_pSpursJobChain->join();

	MemAlloc_FreeAligned( m_pSpursJobChain );
	m_pSpursJobChain = NULL;
}


#endif

void VjobChain4::WaitForEntryNotify( VjobChain4Buffer_t * eaBuffer )
{
	volatile job_notify::NotifyArea_t *eaNotify = &eaBuffer->m_notifyArea;
	// it doesn't matter what DMA tag we'll use for synchronous DMA get
	Assert( VjobDmaGetUint32( (uint)&eaNotify->m_nCopyFrom, DMATAG_SYNC, 0, 0 ) );
	while( !VjobDmaGetUint32( (uint)&eaNotify->m_nCopyTo, DMATAG_SYNC, 0, 0 ) )
	{
		++m_nSpinWaitNotify;
		#ifndef SPU
		sys_timer_usleep( 30 );
		#endif
	}
	if( m_nSpinWaitNotify )
	{
		VjobSpuLog( "VjobChain: stall in WaitForEntryNotify, %d spins\n", m_nSpinWaitNotify );
		m_nSpinWaitNotify = 0;
	}
}



uint64* VjobChain4::SwapCommandBuffer(  )
{
	uint64 * eaSpursIsSpinningHere = &m_pFrontBuffer->m_spursCommands[ m_nFrontBufferCommandCount ];
	
	Assert( m_nFrontBufferCommandCount < m_nMaxCommandsPerBuffer );
	uint nNext1Buffer = ( m_nFrontBuffer + 1 ) % BUFFER_COUNT, nNext2Buffer = ( m_nFrontBuffer + 2 ) % BUFFER_COUNT;
	VjobChain4Buffer_t * eaNext1Buffer = m_pBuffers[ nNext1Buffer ], * eaNext2Buffer = m_pBuffers[ nNext2Buffer ];
	
	// before we can declare the next1 buffer "front", we need to make sure it's fully ready to accept commands, i.e. that it was fully read by SPURS
	// for that, we check the next2 buffer notification area
	
	WaitForEntryNotify( eaNext1Buffer );
	WaitForEntryNotify( eaNext2Buffer );

	// if next2 buffer has been notified, next1 must have been notified long ago
	Assert( VjobDmaGetUint32( (uint)&eaNext1Buffer->m_notifyArea.m_nCopyTo, DMATAG_SYNC, 0, 0 ) );
	uint64* pInsertionPoint = StartCommandBuffer( nNext1Buffer );
	Assert( eaNext1Buffer == m_pFrontBuffer );
	// implicit lwsync is here
	VjobDmaPutfUint64( CELL_SPURS_JOB_COMMAND_NEXT( eaNext1Buffer->m_spursCommands ), (uint)eaSpursIsSpinningHere, DMATAG_SYNC ); // jump to the next buffer
	
	return pInsertionPoint;
}

#ifndef SPU
void FillSpursJts( uint64 * eaCommands, uint nBufferCount )
{
	for( uint i = 0; i < nBufferCount; ++i )
		eaCommands[ i ] = CELL_SPURS_JOB_COMMAND_JTS;
}
#endif

//
// Initializes the buffer BEFORE the jobchain can jump to it. It's important to only jump to the next buffer	
// after this function returns (either by inserting NEXT into previous buffer, or by call Run() on the jobchain)
// because this function lacks the necessary synchronization to operate safely on a buffer in-flight
//
uint64* VjobChain4::StartCommandBuffer( uint nNext1Buffer )
{
	m_nFrontBuffer = nNext1Buffer;
	m_pFrontBuffer = m_pBuffers[ nNext1Buffer ];
	// the ready marker is presumed to be present; SPURS must have gone through this buffer in the previous ring, otherwise we can't use it
	Assert( VjobDmaGetUint32( (uint)&m_pFrontBuffer->m_notifyArea.m_nCopyTo, DMATAG_SYNC, 0, 0) == 1 );
	// reset the ready marker; SPURS didn't get to this buffer yet (we're about to reuse them and we didn't jump to it yet)
	VjobDmaPutfUint32( 0, (uint)&m_pFrontBuffer->m_notifyArea.m_nCopyTo, DMATAG_SYNC ); 
	uint64 * eaCommand = &m_pFrontBuffer->m_spursCommands[ VjobChain4Buffer_t::VERBATIM_COMMAND_COUNT ];
	//VjobDmaPutfUint64( nInsertCommand, (uint)eaCommand, DMATAG_SYNC );
	
	FillSpursJts( eaCommand, m_nMaxCommandsPerBuffer - VjobChain4Buffer_t::VERBATIM_COMMAND_COUNT );
	m_nFrontBufferCommandCount = VjobChain4Buffer_t::VERBATIM_COMMAND_COUNT + 1;
	LWSYNC_PPU_ONLY();
	return eaCommand;
}


