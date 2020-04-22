//========== Copyright © Valve Corporation, All rights reserved. ========
// This is the central hub for controlling SPU activities relating to 
// RSX/graphics processing/rendering
//
#include "spugcm.h"
#include "ps3/ps3gcmmemory.h"
#include "fpcpatcher_spu.h"
#include "ps3gcmstate.h"
#include "vjobs/root.h"
#include "ps3/ps3gcmlabels.h"
#include "ps3/vjobutils_shared.h"
#include "vjobs/jobparams_shared.h"
#include "vjobs/ibmarkup_shared.h"
#include "inputsystem/iinputsystem.h"
#include <sysutil/sysutil_common.h>
#include <sysutil/sysutil_sysparam.h>
#include <cell/pad.h>
#include <materialsystem/imaterialsystem.h>
#include "fpcpatcher_spu.h"
#include "dxabstract.h"
#include "rsxflip.h"

extern IVJobs * g_pVJobs;
CSpuGcmSharedState g_spuGcmShared; 

CSpuGcm g_spuGcm;
static int s_nFinishLabelValue = 0, s_nStopAtFinishLabelValue = -1;
CEdgeGeomRing g_edgeGeomRing;
ApplicationInstantCountersInfo_t g_aici;
CEdgePostWorkload g_edgePostWorkload;

#define PCB_RING_CTX ( *gCellGcmCurrentContext )

void FillNops( struct CellGcmContextData *context )
{
	while( context->current < context->end )
		*( context->current++ ) = CELL_GCM_METHOD_NOP;
}

int32_t SpuGcmCommandBufferReserveCallback( struct CellGcmContextData *context, uint32_t nCount )
{
	return g_spuGcm.OnGcmCommandBufferReserveCallback( context, nCount );
}

void SpuGcmDebugFinish( CellGcmContextData *thisContext )
{
	Assert( thisContext == &PCB_RING_CTX );
	g_spuGcm.CmdBufferFinish();
}


void StallAndWarning( const char * pWarning )
{
	sys_timer_usleep( 30 );
	if( g_spuGcmShared.m_enableStallWarnings )
	{
		Warning( "Stall: %s\n", pWarning );
	}
}


//#endif

void CSpuGcm::CreateRsxBuffers()
{
	//////////////////////////////////////////////////////////////////////////
	// Create Fragment program patch buffers
	//
	uint nFpcpRingBufferSize = CalculateMemorySizeFromCmdLineParam( "-fpcpRingSize", 512 * 1024, 32 * 1024 );
	Msg("Fpcp ring size: %d bytes \n", nFpcpRingBufferSize );
	m_fpcpRingBuffer.Alloc( kAllocPs3GcmShader, nFpcpRingBufferSize );
	g_spuGcmShared.m_fpcpRing.SetRsxBuffer( m_fpcpRingBuffer.DataInLocalMemory(), nFpcpRingBufferSize, nFpcpRingBufferSize / 4, nFpcpRingBufferSize / 4096 ); 
	uint nEdgeRingBufferSize = CalculateMemorySizeFromCmdLineParam( "-edgeRingSize", 2 * 1024 * 1024, 1536 * 1024 );
	Msg("Edge ring size: %d bytes\n", nEdgeRingBufferSize );
	m_edgeGeomRingBuffer.Alloc( kAllocPs3GcmEdgeGeomBuffer, nEdgeRingBufferSize );
	if( nEdgeRingBufferSize < 8 * EDGEGEOMRING_MAX_ALLOCATION )
	{
		Error( "EdgeGeom has ring buffer that won't fit 8 jobs, which is a minimum. %u ( %u ) < 8 * %u\n", nEdgeRingBufferSize, m_edgeGeomRingBuffer.Size(), EDGEGEOMRING_MAX_ALLOCATION );
	}
	if( nEdgeRingBufferSize < 6 * 8 * EDGEGEOMRING_MAX_ALLOCATION )
	{
		Warning( "EdgeGeom has ring buffer that may block job_edgegeom performance. %u ( %u ) < 6 SPUs * 8 segments * %u\n", nEdgeRingBufferSize, m_edgeGeomRingBuffer.Size(), EDGEGEOMRING_MAX_ALLOCATION );
	}
}


const vec_uint4 g_vuSpuGcmCookie = (vec_uint4){0x04291978,0xC00CC1EE,0x04291978,0xC00CC1EE};
void CSpuGcm::CreateIoBuffers()
{
	const uint nCmdBufferOverfetchSlack = 1024;
	uint nFpRingIoBufferSize = 16 * 1024;
	uint nFpRingBufferSize = Max( nFpRingIoBufferSize, nCmdBufferOverfetchSlack ); // this buffer is RSX-write-only, at the end of mapped memory, it acts as an overfetch slack, too, so it must be at least the size of the slack
	g_spuGcmShared.m_fpcpRing.SetIoBuffer( g_ps3gcmGlobalState.IoMemoryPrealloc( nFpRingIoBufferSize, nFpRingBufferSize ), nFpRingIoBufferSize );
	
	m_pMlaaBufferCookie = NULL;
	m_pMlaaBuffer = NULL;
	m_pMlaaBufferOut = NULL;
	m_pEdgePostRsxLock = NULL;
	
	if( !CommandLine()->FindParm( "-noMlaa" ) )
	//if( CommandLine()->FindParm( "-edgeMlaa" ) )
	{
		uint nSizeofEdgePostBuffer = g_ps3gcmGlobalState.GetRenderSurfaceBytes( 128 );
		m_pMlaaBuffer = g_ps3gcmGlobalState.IoMemoryPrealloc( 128, nSizeofEdgePostBuffer + sizeof( g_vuSpuGcmCookie ) + sizeof( uint32 ) * CPs3gcmDisplay::SURFACE_COUNT );
		if( m_pMlaaBuffer )
		{
			m_pMlaaBufferOut = m_pMlaaBuffer;//( void* )( uintp( m_pMlaaBuffer ) + nSizeofEdgePostBuffer );
			m_pMlaaBufferCookie = ( vec_uint4* ) ( uintp( m_pMlaaBufferOut ) + nSizeofEdgePostBuffer );
			*m_pMlaaBufferCookie = g_vuSpuGcmCookie;
			m_pEdgePostRsxLock = ( uint32* )( m_pMlaaBufferCookie + 1 );
		}
		else
		{
			// if MlaaBuffer is NULL, it just means we're in the pass of computing the IO memory requirements
		}
	}
}



//
// memory optimization: IO memory has slack, use it if it's big enough
//
void CSpuGcm::UseIoBufferSlack( uint nIoBufferSlac )
{
	uint nSpuDrawQueueSize = CalculateMemorySizeFromCmdLineParam( "-spuDrawRingSize", 512 * 1024, 32 * 1024 );
	Msg( "SPU draw queue size: %d Kb\n" , nSpuDrawQueueSize / 1024 );
	uint nSpuDrawQueueDeferredSize = CalculateMemorySizeFromCmdLineParam( "-spuDrawDeferredRingSize", 210 * 1024, 32 * 1024 );
	Msg( "SPU draw deferred queue size: %d Kb\n" , nSpuDrawQueueDeferredSize / 1024 );

	m_nSpuDrawQueueSelector = 0;
	m_spuDrawQueues[0].Init( nSpuDrawQueueSize, &g_spuGcmShared.m_nSpuDrawGet[0], OnSpuDrawQueueFlush, OnSpuDrawQueueStall );
	m_spuDrawQueues[1].Init( nSpuDrawQueueDeferredSize, &g_spuGcmShared.m_nSpuDrawGet[1], OnSpuDrawQueueFlushDeferred, OnSpuDrawQueueStallDeferredDelegator );

	for( uint i = 0; i < ARRAYSIZE( m_pDeferredStates ); ++i )
		m_pDeferredStates[i] = ( DeferredState_t * ) g_ps3gcmGlobalState.IoSlackAlloc( 128, sizeof( DeferredState_t ) );

	for( uint i = 0; i < ARRAYSIZE( m_pDeferredQueueCursors ); ++i )
		m_pDeferredQueueCursors[i] = m_spuDrawQueues[1].GetCursor();
	m_pDeferredQueueSegment = m_pDeferredQueueCursors[0];
	m_pDeferredChunkSubmittedTill[1] = m_spuDrawQueues[1].GetCursor();
	for( uint i = 0; i < ARRAYSIZE( m_spuDrawQueues ); ++i )
		g_spuGcmShared.m_nSpuDrawGet[i] = m_spuDrawQueues[i].GetSignal();
}


static fltx4 g_vertexProgramConstants[CELL_GCM_VTXPRG_MAX_CONST];
// static uint s_nLastCtxBufferCookie = 0;
// static uint s_nCtxBufferSegmentSubmitTime = 0;  // divide by 2 and it'll be the weighted average of 79.8MHz ticks between segment submissions

void CSpuGcm::OnGcmInit()
{
	if( 127 & uintp( g_ps3gcmGlobalState.m_pLocalBaseAddress ) )
	{
		Error( "Local addresses map to main memory without proper 128-byte alignment! Some DMA assumptions are broken!!\n" );
	}
	if( 127 & uintp( g_ps3gcmGlobalState.m_nIoOffsetDelta ) )
	{
		Error( "IO addresses map to local memory without proper 128-byte alignment! Some DMA assumptions are broken!!\n" );
	}
	V_memset( &g_spuGcmShared.m_dxGcmState, 0, sizeof( g_spuGcmShared.m_dxGcmState ) );
	V_memset( &g_spuGcmShared.m_cachedRenderState, 0, sizeof( g_spuGcmShared.m_cachedRenderState ) );
	
	m_nPcbringWaitSpins = 0;
	m_pPcbringBuffer = NULL;
	m_eaLastJobThatUpdatesSharedState = 0;
	g_spuGcmShared.m_enableStallWarnings = ( CommandLine()->FindParm( "-enableStallWarnings" ) != 0 );
	
	g_spuGcmShared.m_edgeGeomFeeder.Init( m_edgeGeomRingBuffer.Size() );
	g_edgeGeomRing.Init( m_edgeGeomRingBuffer.DataInLocalMemory(), m_edgeGeomRingBuffer.Size(), g_ps3gcmGlobalState.m_nIoOffsetDelta, g_ps3gcmGlobalState.m_pLocalBaseAddress, GCM_LABEL_EDGEGEOMRING );
	g_spuGcmShared.m_eaEdgeGeomRing = &g_edgeGeomRing;

	g_spuGcmShared.m_fpcpRing.OnGcmInit( g_ps3gcmGlobalState.m_nIoOffsetDelta );
	g_spuGcmShared.m_nDrawLayerBits = g_spuGcmShared.LAYER_RENDER;
	g_spuGcmShared.m_nDrawLayerPredicates = g_spuGcmShared.LAYER_RENDER_AND_Z;
	
	g_spuGcmShared.m_nLastRsxInterruptValue = 0;
	
	if( m_pEdgePostRsxLock )
	{
		for( uint i = 0; i < CPs3gcmDisplay::SURFACE_COUNT; ++i )
		{
			m_pEdgePostRsxLock[i] = CELL_GCM_RETURN(); // assume previous flips already happened			
		}
	}

	g_pVJobs->Register( this );
	
	m_zPass.Init();
	m_bUseDeferredDrawQueue = true;
	BeginGcmStateTransaction();
	g_pixelShaderPatcher.InitLocal( g_spuGcmShared.m_fpcpRing.GetRsxBuffer(), g_spuGcmShared.m_fpcpRing.GetRsxBufferSize() );
	g_spuGcmShared.m_eaFpcpSharedState              = g_pixelShaderPatcher.m_state.m_pSharedState;
	g_spuGcmShared.m_nFpcpBufferMask                = g_spuGcmShared.m_eaFpcpSharedState->m_nBufferMask;
	g_spuGcmShared.m_eaLocalBaseAddress				= (uint32)g_ps3gcmGlobalState.m_pLocalBaseAddress;
	g_spuGcmShared.m_cachedRenderState.m_nDisabledSamplers				= 0;
	g_spuGcmShared.m_cachedRenderState.m_nSetTransformBranchBits		= 0;
	g_spuGcmShared.m_nDebuggerRunMask				= SPUGCM_DEBUG_MODE ? 2 : 0;
	g_spuGcmShared.m_eaLastJobThatUpdatedMe			= 0;
	g_spuGcmShared.m_nFpcPatchCounterOfLastSyncJob	= g_pixelShaderPatcher.m_nFpcPatchCounterOfLastSyncJob;
	g_spuGcmShared.m_nFpcPatchCounter				= g_pixelShaderPatcher.m_nFpcPatchCounter;
	g_spuGcmShared.m_nFpcpStartRangesAfterLastSync  = g_spuGcmShared.m_eaFpcpSharedState->m_nStartRanges;
	g_spuGcmShared.m_eaZPassSavedState               = NULL;
	
	g_spuGcmShared.m_nIoLocalOffsetEmptyFragmentProgramSetupRoutine = g_ps3gcmGlobalState.m_nIoLocalOffsetEmptyFragmentProgramSetupRoutine;
	g_spuGcmShared.m_eaPs3texFormats				= g_ps3texFormats;
	
	g_spuGcmShared.m_eaVertexProgramConstants = g_vertexProgramConstants;
	
	m_nGcmFlushJobScratchSize = 0;
	m_nFrame = 0;

	// we shouldn't have used this format yet
	Assert( g_ps3texFormats[PS3_TEX_CANONICAL_FORMAT_COUNT].m_gcmPitchPer4X == 0 );
	Assert( g_ps3texFormats[PS3_TEX_CANONICAL_FORMAT_COUNT-1].m_gcmPitchPer4X != 0 );
	Assert( !( 0xF & uintp( g_spuGcmShared.m_eaPs3texFormats ) ) );
	
	Assert( g_spuGcmShared.m_nIoLocalOffsetEmptyFragmentProgramSetupRoutine );
	COMPILE_TIME_ASSERT( !GCM_CTX_UNSAFE_MODE );
	{
		m_pFinishLabel = cellGcmGetLabelAddress( GCM_LABEL_SPUGCM_FINISH );
		*m_pFinishLabel = s_nFinishLabelValue;

		uint nSysringBytes = g_ps3gcmGlobalState.m_nCmdSize - SYSTEM_CMD_BUFFER_RESERVED_AREA - 16 - sizeof( SysringWrapSequence::Tail_t ); // 16 bytes for the JTN to wrap the buffer around, and to be able to DMA it in 16-byte chunks
		nSysringBytes &= -16; // make it 16-byte aligned
		uint eaSysringBuffer = uintp( g_ps3gcmGlobalState.m_pIoAddress ) + SYSTEM_CMD_BUFFER_RESERVED_AREA;
		uint32 * pSysringBufferEnd = ( uint32* )( eaSysringBuffer + nSysringBytes );
		*pSysringBufferEnd =  // this is not strictly needed...
		g_spuGcmShared.m_sysringWrap.m_tail.m_nJumpToBegin                   = CELL_GCM_JUMP( SYSTEM_CMD_BUFFER_RESERVED_AREA ); 
		V_memset( g_spuGcmShared.m_sysringWrap.m_tail.m_nNops, 0, sizeof( g_spuGcmShared.m_sysringWrap.m_tail.m_nNops ) );
		Assert( !( 0xF & uint( &g_spuGcmShared.m_sysringWrap ) ) );
		
		
		//COMPILE_TIME_ASSERT( SPUGCM_USE_SET_REFERENCE_FOR_SYSRING_SIGNAL );
		//g_spuGcmShared.m_pEaSysringEndLabel = ( uint32* ) cellGcmGetLabelAddress( GCM_LABEL_SYSRING_END );
		//*g_spuGcmShared.m_pEaSysringEndLabel = g_spuGcmShared.m_sysring.m_nEnd; // pretend we finished all processing
		
		//g_spuGcmShared.m_nSysringSegmentWords = ( g_ps3gcmGlobalState.m_nCmdSize - nSysringCmdBufferSystemArea ) / sizeof( uint32 ) / g_spuGcmShared.NUM_SYSTEM_SEGMENTS;
		//g_spuGcmShared.m_nSysringSegmentWords &= -16; // make it aligned, at least -4 words but may be more for easier debugging (more round numbers)
		g_spuGcmShared.m_nIoOffsetDelta       = g_ps3gcmGlobalState.m_nIoOffsetDelta;
		g_spuGcmShared.m_nSysringWaitSpins    = 0;
		g_spuGcmShared.m_nSysringPuts         = 0;
		g_spuGcmShared.m_nSysringSegmentSizeLog2  = 29 - __cntlzw( g_ps3gcmGlobalState.m_nCmdSize ); // make 4 subsegments; guarantee segment switch whenever the ring wraps around
		// we need AT LEAST 2 segments and each segment must be AT LEAST 1kb - for performant and reliable operation; 
		Assert( ( g_ps3gcmGlobalState.m_nCmdSize >> g_spuGcmShared.m_nSysringSegmentSizeLog2 ) > 2 && ( g_ps3gcmGlobalState.m_nCmdSize >> g_spuGcmShared.m_nSysringSegmentSizeLog2 ) < 8  && g_spuGcmShared.m_nSysringSegmentSizeLog2 >= 10 );
		//g_spuGcmShared.m_nSysringPut          = 0;
		//g_spuGcmShared.m_nSysringEnd          = g_spuGcmShared.NUM_SYSTEM_SEGMENTS; // pretend we got the whole buffer already
		g_spuGcmShared.m_nDebuggerBreakMask     = 0x00000000;
		g_spuGcmShared.m_nDebugLastSeenGet      = 0xFEFEFEFE;

		uint nPcbringSize = SPUGCM_DEFAULT_PCBRING_SIZE;
		COMPILE_TIME_ASSERT( !( SPUGCM_DEFAULT_PCBRING_SIZE & ( SPUGCM_DEFAULT_PCBRING_SIZE - 1 ) ) );
		g_spuGcmShared.m_nPcbringSize = nPcbringSize ;
		// 12 extra bytes are allocated for buffer alignment code to avoid writing past end of the buffer ; 4 more bytes are for the cookie
		//m_pPcbringBuffer = ( uint32 * )MemAlloc_AllocAligned( nPcbringSize + 12 + 4, 0x10 );
		//*AddBytes( m_pPcbringBuffer, g_spuGcmShared.m_nPcbringSize + 12 ) = 0x1234ABCD;
		m_nPcbringBegin = 0;
		g_spuGcmShared.m_nPcbringEnd = g_spuGcmShared.m_nPcbringSize; // consider the full ring buffer already processed on SPU and free: this End is the end of "free to use" area
		// these is the max count of words needed to align the cmd buffer and insert any write-labels/set-reference-values
		// we need to add at least 3 to the count, in case we align current pointer in the process ( because we may need to submit )
		// also, we want this segment size to fit inside the between-segment signal
		m_nMaxPcbringSegmentBytes = Min<uint>( ( ( nPcbringSize - 32 - SPUGCM_SIZEOF_SYSRING_ENDOFSEGMENT_SIGNAL_COMMAND ) / 4 ) & -16, ( 1 << g_spuGcmShared.m_nSysringSegmentSizeLog2 ) - SPUGCM_SIZEOF_SYSRING_ENDOFSEGMENT_SIGNAL_COMMAND - 12 ); // 
		// we definitely need PCBring segment to fit well into local store
		m_nMaxPcbringSegmentBytes = Min<uint>( m_nMaxPcbringSegmentBytes, SPUGCM_LSRING_SIZE / 2 );
		m_nMaxPcbringSegmentBytes = Min<uint>( m_nMaxPcbringSegmentBytes, SPUGCM_MAX_PCBRING_SEGMENT_SIZE );
		m_nMaxPcbringSegmentBytes &= -16; // make it 16-byte aligned..

		cellGcmReserveMethodSize( gCellGcmCurrentContext, 3 ); // we need at most ( 2 words for reference command + ) 3 words for alignment
		
		// align the buffer on 16-byte boundary, because we manage it in 16-byte increments
		while( 0xF & uintp( gCellGcmCurrentContext->current ) )
		{
			*( gCellGcmCurrentContext->current++ ) = CELL_GCM_METHOD_NOP;
		}
		
		g_spuGcmShared.m_sysring.Init( eaSysringBuffer, nSysringBytes, uint( gCellGcmCurrentContext->current ) - eaSysringBuffer );
		g_spuGcmShared.m_sysringRo.Init( GCM_LABEL_SYSRING_SIGNAL );
		g_spuGcmShared.m_nSysringWrapCounter = 0;
		g_spuGcmShared.m_eaGcmControlRegister = cellGcmGetControlRegister();
		g_spuGcmShared.m_eaSysringLabel = cellGcmGetLabelAddress( GCM_LABEL_SYSRING_SIGNAL );
		g_spuGcmShared.m_eaDebugLabel[0] = cellGcmGetLabelAddress( GCM_LABEL_DEBUG0 );
		g_spuGcmShared.m_eaDebugLabel[1] = cellGcmGetLabelAddress( GCM_LABEL_DEBUG1 );
		g_spuGcmShared.m_eaDebugLabel[2] = cellGcmGetLabelAddress( GCM_LABEL_DEBUG2 );
		*g_spuGcmShared.m_eaSysringLabel = g_spuGcmShared.m_sysring.GetSignal(); // pretend we executed WriteLabel
		g_spuGcmShared.m_nLastSignal            = g_spuGcmShared.m_sysring.GetInvalidSignal();
	#if SPU_GCM_DEBUG_TRACE
		g_spuGcmShared.m_nDebugTraceBufferNext  = 0;
		g_spuGcmShared.m_eaDebugTraceBuffer     = ( SpuGcmDebugTrace_t* )MemAlloc_AllocAligned( g_spuGcmShared.DEBUG_BUFFER_COUNT * sizeof( SpuGcmDebugTrace_t ), 16 );
	#endif
		if( SPUGCM_USE_SET_REFERENCE_FOR_SYSRING_SIGNAL )
		{
			g_spuGcmShared.m_eaGcmControlRegister->ref = g_spuGcmShared.m_sysring.m_nEnd;// pretend we finished all processing
		}
		
	#ifdef _DEBUG
		m_nJobsPushed = 0;
		// fill in JTS in the rest of the buffer
		for( uint32 * pSlack = gCellGcmCurrentContext->current; pSlack < pSysringBufferEnd; ++pSlack )
			*pSlack = CELL_GCM_JUMP( uintp( pSlack ) - uintp( g_ps3gcmGlobalState.m_pIoAddress ) );
	#endif
		// set reference BEFORE we switch to sysring
		uint nGcmPut = uintp( gCellGcmCurrentContext->current ) + g_spuGcmShared.m_nIoOffsetDelta;
		Assert( !( 0xF & nGcmPut ) );
		__sync();
		g_spuGcmShared.m_eaGcmControlRegister->put = nGcmPut;
		// wait for RSX to reach this point, then switch to the new command buffer scheme
		int nAttempts = 0;
		while( g_spuGcmShared.m_eaGcmControlRegister->get != nGcmPut )
		{
			sys_timer_usleep(1000);
			if( ++nAttempts > 1000 )
			{
				Warning( "Cannot properly wait for RSX in OnGcmInit(%X!=%X); assuming everything's all right anyway.\n", g_spuGcmShared.m_eaGcmControlRegister->get, nGcmPut );
				break; // don't wait forever..
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Switch to PPU Command Buffer RING 
		//
		// set reference BEFORE we switch to sysring; wait for all RSX initialization to go through before switching
		PCB_RING_CTX.begin = PCB_RING_CTX.current = NULL;//m_pPcbringBuffer;
		// we need to at least double-buffer to avoid deadlocks while waiting to submit a Pcbring segment
		// Each segment ends with a reference value update, and we need that update to unblock a piece of memory for use by subsequent submits
		Assert( GetMaxPcbringSegmentBytes() <= nPcbringSize / 2 ); 
		PCB_RING_CTX.end = NULL;//AddBytes( m_pPcbringBuffer, GetMaxPcbringSegmentBytes() );
		PCB_RING_CTX.callback = SpuGcmCommandBufferReserveCallback;

	#ifdef CELL_GCM_DEBUG // [
		gCellGcmDebugCallback = SpuGcmDebugFinish;
		cellGcmDebugCheckEnable( CELL_GCM_TRUE );
	#endif // ]
	}
}







inline signed int CSpuGcm::GetPcbringAvailableBytes()const
{
	int nReallyAvailable = int32( *(volatile uint32*)&g_spuGcmShared.m_nPcbringEnd ) - int32( m_nPcbringBegin );
	#ifdef DBGFLAG_ASSERT
	Assert( uint( nReallyAvailable ) <= g_spuGcmShared.m_nPcbringSize );
	static int s_nLastPcbringAvailableBytes = -1;
	s_nLastPcbringAvailableBytes = nReallyAvailable;
	#endif
	Assert( nReallyAvailable >= 0 );
	return nReallyAvailable;
}



int CSpuGcm::OnGcmCommandBufferReserveCallback( struct CellGcmContextData *context, uint32_t nReserveCount )
{
	FillNops(context);
	// IMPORTANT: we only allocate the necessary number of words here, no more no less
	//            if we over-allocate, we may end up reordering commands in SPU draw queue following after GCM_FUNC commands
	uint nReserve = nReserveCount; 
	uint32 * pDrawQueueCommand = GetDrawQueue()->AllocWords( nReserve + 1 );
	*pDrawQueueCommand = SPUDRAWQUEUE_GCMCOMMANDS_METHOD | nReserve;
	context->begin = context->current = pDrawQueueCommand + 1;
	context->end = context->begin + nReserve;
	if( IsDebug() )
		V_memset( context->current, 0xFE, nReserve * 4 );
	return CELL_OK;
}



void CSpuGcm::BeginGcmStateTransaction()
{
	m_nCurrentBatch = BATCH_GCMSTATE;
	SetCurrentBatchCursor( GetDrawQueue()->GetCursor() );
}



void CSpuGcm::PushStateFlushJob( SpuDrawQueue * pDrawQueue, uint nResultantSpuDrawQueueSignal, uint32 *pCursorBegin, uint32 * pCursorEnd )
{
	// only submit the job if there are any commands in the state command buffer
	CellSpursJob128 * pJob = m_jobPool128.Alloc( *m_pRoot->m_pGcmStateFlush );
	job_gcmstateflush::JobParams_t * pJobParams = job_gcmstateflush::GetJobParams( pJob );
	pJob->header.useInOutBuffer = 1;
	CDmaListConstructor dmaConstructor( pJob->workArea.dmaList );
	dmaConstructor.AddInputDma( sizeof( g_spuGcmShared ), &g_spuGcmShared );				 // dma[0]; must be the first to be 128-byte aligned for atomics
	uint nSizeofDrawQueueUploadBytes = pDrawQueue->Collect( pCursorBegin, pCursorEnd, dmaConstructor );
	Assert( !( nSizeofDrawQueueUploadBytes & 3 ) );
	
	dmaConstructor.AddSizeInOrInOut( 48 + SPUGCM_LSRING_SIZE ); // 16 bytes for alignment; 16 for lsZero; 16 for lsTemp;
	COMPILE_TIME_ASSERT( sizeof( CPs3gcmTextureLayout::Format_t ) == 16 );
	dmaConstructor.AddCacheDma( g_nPs3texFormatCount * sizeof( CPs3gcmTextureLayout::Format_t ), g_ps3texFormats )	;
	dmaConstructor.FinishIoBuffer( &pJob->header, pJobParams );

	pJobParams->m_nSkipDrawQueueWords = ( uintp( pCursorBegin ) / sizeof( uint32 ) ) & 3;
	pJobParams->m_nSizeofDrawQueueUploadWords = nSizeofDrawQueueUploadBytes / sizeof( uint32 ) ;
	Assert( uint( pJobParams->m_nSizeofDrawQueueUploadWords ) == nSizeofDrawQueueUploadBytes / sizeof( uint32 ) ); // make sure it fits into uint16
	pJobParams->m_nSpuDrawQueueSignal = nResultantSpuDrawQueueSignal;

	#ifdef DBGFLAG_ASSERT
	SpuDrawQueue * pSignalDrawQueue = &m_spuDrawQueues[ nResultantSpuDrawQueueSignal & 3 ? 1 : 0 ]; (void)pSignalDrawQueue;
	Assert( pSignalDrawQueue->IsValidCursor( (uint32*)( nResultantSpuDrawQueueSignal & ~3 ) ) );
	#endif
	uint nResultantSpuDrawQueueIndex = nResultantSpuDrawQueueSignal & 3;
	m_pDeferredChunkSubmittedTill[ nResultantSpuDrawQueueIndex ] = ( uint32* )( nResultantSpuDrawQueueSignal & ~3 );


	Assert( CELL_OK == cellSpursCheckJob( (const CellSpursJob256 *)pJob, sizeof( *pJob ), 256 ) );
	m_eaLastJobThatUpdatesSharedState = ( uintp )pJob;

	pJob->header.sizeScratch = m_nGcmFlushJobScratchSize;
	m_nGcmFlushJobScratchSize = 0;
	PushSpuGcmJob( pJob );

	if( SPUGCM_DEBUG_MODE )
	{
		// in SPUGCM_DEBUG_MODE, we execute all jobs and wait for them to complete. So, the GET pointer should always trail our pNext pointer
		Assert( g_spuGcmShared.m_nSpuDrawGet[nResultantSpuDrawQueueIndex] == ( nResultantSpuDrawQueueSignal & ~3 ) );
	}
}


void CSpuGcm::GcmStateFlush( )
{
	Assert( m_nCurrentBatch == BATCH_GCMSTATE );
	if( IsDeferredDrawQueue() )
	{
		Warning( "Unexpected Flush in deferred spu draw queue\n" );
		OpenDeferredChunk();
	}
	else
	{
		if( GetCurrentBatchCursor() != GetDrawQueue()->GetCursor() )
		{
			FillNops( &PCB_RING_CTX );
			Assert( GetDrawQueue() == &m_spuDrawQueues[0] );
			PushStateFlushJob( &m_spuDrawQueues[0], m_spuDrawQueues[0].GetSignal(), GetCurrentBatchCursor(), GetDrawQueue()->GetCursor() );

			BeginGcmStateTransaction();
			ZPassCheckpoint( 6 );
		}
	}
}


void CSpuGcm::PushSpuGcmJob( CellSpursJob128 * pJob )
{
#ifdef _DEBUG
	m_nJobsPushed++;
#endif
	PushSpuGcmJobCommand( CELL_SPURS_JOB_COMMAND_JOB( pJob ) );
	if( SPUGCM_DEBUG_MODE )
	{
		if( !m_zPass )
		{
			// in ZPass_Z the job doesn't free its descriptor
			// in ZPass_Render, we don't start the jobs through here
			// so we can't use this spin-wait to wait for the job to complete
			while( *( volatile uint64* )&pJob->header.eaBinary )
			{
				sys_timer_usleep( 60 );
			}
		}
	
		while( g_spuGcmShared.m_eaLastJobThatUpdatedMe != uintp( pJob ) )
		{
			sys_timer_usleep( 60 );
		}
	}
}



void CSpuGcm::PushSpuGcmJobCommand( uint64 nCommand )
{
	if( m_zPass )
	{
		m_zPass.PushCommand( nCommand );
	}
	else
	{
		m_jobSink.PushSyncJobSync( nCommand );
	}
}



void CSpuGcm::ZPassCheckpoint( uint nReserveSlots )
{
	if( m_zPass )
	{
		uint nFreeSubchainSlots = m_zPass.GetSubchainCapacity();
		if( nFreeSubchainSlots < 2 * nReserveSlots )
		{
			ExecuteOnce( Warning("Aborting Z prepass: not enough room for commands in zpass sub-job-chain (%d left).\n", nFreeSubchainSlots ) );
			AbortZPass(); // initiate Abort sequence of ZPass; reentrant
		}
		uint nFreeJobDescriptors = m_jobPool128.GetReserve( m_zPass.m_nJobPoolMarker );
		if( nFreeJobDescriptors < nReserveSlots )
		{
			ExecuteOnce( Warning("Aborting Z prepass: not enough room for job descriptors in m_jobPool128 (%d left)\n", nFreeJobDescriptors ) );
			AbortZPass();
		}
	}
}



void CSpuGcm::OnSetPixelShaderConstant()
{
	Assert( !IsDeferredDrawQueue() );
	if( m_zPass )
	{
		if( !m_zPass.m_isInEndZPass )
		{
			if( g_pixelShaderPatcher.GetJournalSpaceLeftSince( m_zPass.m_nFpcpStateEndOfJournalIdxAtZPassBegin ) < 512 )
			{
				ExecuteOnce( Warning( "Performance Warning: Too many pixel shader constants set inside ZPass; aborting ZPass\n" ) );
				AbortZPass();
			}
		}
	}
	else
	{
		// we have space for 48kB (3k of constants) in FPCP; 
		// every SetPixelShaderConstant may add 97 constants (96 values, 1 header)
		if( g_pixelShaderPatcher.GetJournalSpaceUsedSince( m_nFpcpStateEndOfJournalIdxAtSpuGcmJob ) > ( 32*1024 / 16 ) || g_pixelShaderPatcher.GetJournalSpaceLeftSince( m_nFpcpStateEndOfJournalIdxAtSpuGcmJob ) < 512 )
		{
			ExecuteOnce( Warning("Performance Warning: SetPixelShaderConstantF called for %d constants, but no draw calls were issued. Flushing FPCP state.\n", g_pixelShaderPatcher.GetJournalSpaceUsedSince( m_nFpcpStateEndOfJournalIdxAtSpuGcmJob ) ) );
			// flush GCM with only one purpose: make it flush the patcher
			GetDrawQueue()->Push2( SPUDRAWQUEUE_FLUSH_FPCP_JOURNAL, g_pixelShaderPatcher.GetStateEndOfJournalIdx() );
			GcmStateFlush();
		}
	}
}

void CSpuGcm::OnSpuDrawQueueStallDeferredDelegator( SpuDrawQueue *pDrawQueue, uint32 * pGet, uint nWords )
{
	g_spuGcm.OnSpuDrawQueueStallDeferred( pDrawQueue, pGet, nWords );
}

void CSpuGcm::OnSpuDrawQueueStallDeferred( SpuDrawQueue *pDrawQueue, uint32 * pGet, uint nWords )
{
	// we need to try to wait for the previous deferred batch to finish
	// in any case we should be prepared for "out of space" condition
	// in which case we'll just execute all deferred commands right now
	if( pGet == m_pDeferredChunkSubmittedTill[1] )
	{
		// we have nothing else to wait for, we need to free the space by executing deferred commands now
		 // full flush (this frame only, since previous frame was flushed the first time we called DrawQueueDeferred()
		FillNops( &PCB_RING_CTX ); // switching draw queues, preallocated gcm context no longer usable
		
		// the only deferred chunk that can resize is GCMFLUSH
		// and handling it is pretty easy: we can either execute whatever it collected so far
		if( m_pDeferredChunkHead )
		{
			// sanity check: we shouldn't have chunks as big as 64KB
			Assert( m_spuDrawQueues[1].Length( m_pCurrentBatchCursor[1], m_pDeferredChunkHead ) <= 64*1024 );
			Assert( *m_pDeferredChunkHead == SPUDRAWQUEUE_DEFERRED_GCMFLUSH_METHOD && m_pDeferredChunkHead == m_pDeferredQueueCursors[0] );
		}

		// temporarily switch to normal queue state in order to replay the deferred queue commands and purge them
		uint32 * pDeferredQueueSegment = m_pDeferredQueueSegment;
		m_nSpuDrawQueueSelector = 0;
		Assert( m_pCurrentBatchCursor[0] == m_spuDrawQueues[0].GetCursor() );
		BeginGcmStateTransaction(); // this transaction is beginning in Normal draw queue; Deferred queue is currently in "frozen" state (almost out of memory)

		g_flipHandler.QmsAdviceBeforeDrawPrevFramebuffer();
		// flush previous frame first, and if it doesn't change Get , flush this frame
		ExecuteDeferredDrawQueue( 1 );
			extern void DxDeviceForceUpdateRenderTarget( );
			DxDeviceForceUpdateRenderTarget( ); // recover main render target, as it was screwed up by execution of previous frame's commands
		ExecuteDeferredDrawQueue( 0 );
		m_nFramesToDisableDeferredQueue = 1;

		// return to the deferred state after purging the queue. During purging the deferred queue, DrawQueue(Normal|Deferred) could not have been called
		// this "unfreezes" the deferred queue, which should by now be almost-all-free(	or pending, depending on how fast SPUs will chew through it)
		Assert( m_pDeferredQueueSegment == pDeferredQueueSegment );
		
		// we executed up to this point (last opened chunk), we discard everything before it.
		// the last opened chunk is perfectly fine to begin the queue segment, so we pretend we began deferred queue there
		m_pDeferredQueueSegment = m_pDeferredQueueCursors[0];
		
		m_nSpuDrawQueueSelector = 1;
	}
}



void CSpuGcm::OnSpuDrawQueueFlushDeferred( SpuDrawQueue *pDrawQueue )
{
	// break up long GCM chunks
	Assert( pDrawQueue == g_spuGcm.GetDrawQueue() ); 
	Assert( !g_spuGcm.m_pDeferredChunkHead || ( *g_spuGcm.m_pDeferredChunkHead & ~SPUDRAWQUEUE_DEFERRED_GCMFLUSH_MASK ) == SPUDRAWQUEUE_DEFERRED_GCMFLUSH_METHOD ); // this is the only chunk we allocate incrementally
	
	// prevent this from being called recursively: reset flush watermark before doing anything else
	pDrawQueue->SetFlushWatermarkFrom( pDrawQueue->GetCursor() );
		
	g_spuGcm.OpenDeferredChunk();
}

void CSpuGcm::OnSpuDrawQueueStall( SpuDrawQueue *pDrawQueue, uint32 * pGet, uint32 nWords )
{
	Assert( pDrawQueue == &g_spuGcm.m_spuDrawQueues[0] );
	StallAndWarning( "SpuDrawQueue stall: PPU is waiting for SPU, and SPU is probably waiting for RSX\n"/*, nWords, pGet, g_spuGcm.m_spuDrawQueues[0].GetCursor()*/ );
}

void CSpuGcm::OnSpuDrawQueueFlush( SpuDrawQueue *pDrawQueue )
{
	// currently, there's only one and it's 
	Assert( pDrawQueue == g_spuGcm.GetDrawQueue() ); 
	g_spuGcm.GcmStateFlush();
}



void CSpuGcm::OnSpuDrawQueueFlushInZPass()
{
	//
	// flush watermark has changed now (it changes on every collect())
	// override flush watermark to flush before we reach ZPass cursor, 
	// and if it's impossible, then Abort ZPass - we don't have enough space
	// in SPU GCM buffer
	//
	// Take care not to flush excessively when pusing the last few commands into 
	// SPUGCM draw buffer because we can be doing that right around flush watermark
	// frequently
	//
	
	uint32 * pOldFlushWatermark = GetDrawQueue()->GetFlushWatermark();

	GcmStateFlush();

	uint32 * pNewFlushWatermark = GetDrawQueue()->GetFlushWatermark();
	
	if( pNewFlushWatermark < pOldFlushWatermark ? pNewFlushWatermark >= m_zPass.m_pCursor || pOldFlushWatermark <= m_zPass.m_pCursor : pOldFlushWatermark <= m_zPass.m_pCursor && m_zPass.m_pCursor <= pNewFlushWatermark )
	{
		// the next flush will be too late; 
		// NOTE: we can recover up to 32KB by adjusting the flush watermark here, but I have bigger fish to fry, so we'll just abort ZPass right now and here 
		AbortZPass();
	}
}

void CSpuGcm::OnSpuDrawQueueFlushInZPass( SpuDrawQueue *pDrawQueue )
{
	// TODO: check if cursor is intersected and potentially EndZPass()
	Assert( pDrawQueue == g_spuGcm.GetDrawQueue() ); 
	g_spuGcm.OnSpuDrawQueueFlushInZPass();
}

void SpuGcmCommandBufferFlush()
{
	g_spuGcm.CmdBufferFlush();
}



SpuDrawHeader_t * CSpuGcm::BeginDrawBatch()
{
	SpuDrawHeader_t * pDrawHeader;
	if( IsDeferredDrawQueue() )
	{
		uintp eaSpuDrawHeader = ( uintp ) OpenDeferredChunk( SPUDRAWQUEUE_DEFERRED_DRAW_METHOD, 3 + ( sizeof( SpuDrawHeader_t ) + sizeof( IDirect3DVertexDeclaration9 * /*pVertDecl*/ ) ) / sizeof( uint32 ) );
		pDrawHeader = ( SpuDrawHeader_t * ) AlignValue( eaSpuDrawHeader, 16 );
	}
	else
	{
		GcmStateFlush();
		// we must be in the default batch transaction, and it must be empty so that we can switch the transaction type
		Assert( m_nCurrentBatch == BATCH_GCMSTATE && GetCurrentBatchCursor() == GetDrawQueue()->GetCursor() );
		pDrawHeader = GetDrawQueue()->AllocAligned<SpuDrawHeader_t>();
	}
	m_nCurrentBatch = BATCH_DRAW;
	Assert( GetDrawQueue()->IsValidCursor( (uint32*)( pDrawHeader + 1 ) ) );
	SetCurrentBatchCursor( ( uint32* ) pDrawHeader );
	return pDrawHeader;
}



CellSpursJob128 * CSpuGcm::PushDrawBatchJob( uint nResultantSpuDrawQueueSignal, SpuDrawHeader_t * pDrawHeader, IDirect3DVertexDeclaration9 *pVertDecl, OptimizedModel::OptimizedIndexBufferMarkupPs3_t *pIbMarkup )
{
	CellSpursJob128 * pJob = m_jobPool128.Alloc( *m_pRoot->m_pDrawIndexedPrimitive );
	pJob->header.useInOutBuffer = 1;
	// we'll DMA get textures and layouts inside the job; we'll need space for DMA elements to do so
	pJob->header.sizeScratch    = AlignValue( sizeof( JobDrawIndexedPrimitiveScratch_t ), 128 ) / 16;

	CDmaListConstructor dmaConstructor( pJob->workArea.dmaList );
	dmaConstructor.AddInputDma( sizeof( g_spuGcmShared ), &g_spuGcmShared );				 // dma[0]; must be the first to be 128-byte aligned for atomics
	dmaConstructor.AddInputDma( sizeof( *pVertDecl ), pVertDecl );                           // dma[1]
	dmaConstructor.AddInputDma( sizeof( *pDrawHeader ), pDrawHeader );						 // dma[2]

	COMPILE_TIME_ASSERT( sizeof( g_spuGcmShared ) < 16 * 1024 && sizeof( *pVertDecl ) < 16 * 1024 && sizeof( *pDrawHeader ) < 16 * 1024 );

	// pIbMarkup = pDrawHeader->m_eaIbMarkup;
	if ( pIbMarkup )
	{
		uint nIbMarkupBytes = ( pIbMarkup->m_numPartitions * sizeof( OptimizedModel::OptimizedIndexBufferMarkupPs3_t::Partition_t ) + sizeof( OptimizedModel::OptimizedIndexBufferMarkupPs3_t ) );
		dmaConstructor.AddInputDma( ( nIbMarkupBytes + 31 ) & -16, ( const void* )( uintp( pIbMarkup ) & -16 ) ); // dma[3]
	}

	//dmaConstructor.AddInputDmaLarge( SPUGCM_LSRING_SIZE, nUsefulBytesAligned, PCB_RING_CTX.begin );   // dma[4,5,6,7]
	dmaConstructor.AddSizeInOrInOut( SPUGCM_LSRING_SIZE );
	COMPILE_TIME_ASSERT( SPUGCM_LSRING_SIZE / (16*1024) <= 4 );
	// usage of the IO buffer slack:
	// alignment, sync signal, wrap sequence, alignment, RSX PUT control register output, SPURS job command output
	dmaConstructor.AddSizeInOrInOut(
		128  // potential misalignment of command buffer, for double-bandwidth DMA to command buffer (not used now)
		+ sizeof( SysringWrapSequence )  // is it accounted for in the LSRING_SLACK? 
		+ 16   // lsResetDrawBatch
		+ 16   // lsTempRsxPut
		+ 16   // g_lsDummyRead
		);
	COMPILE_TIME_ASSERT( sizeof( CPs3gcmTextureLayout::Format_t ) == 16 );
	dmaConstructor.AddCacheDma( g_nPs3texFormatCount * sizeof( CPs3gcmTextureLayout::Format_t ), g_ps3texFormats )	; // dma[8]
	dmaConstructor.FinishIoBuffer( &pJob->header );
	pJob->header.sizeStack = 16 * 1024 / 16;

	pDrawHeader->m_nPs3texFormatCount     = g_nPs3texFormatCount; // for reference; is not strictly needed here
	pDrawHeader->m_nUsefulCmdBytes        = 0;//nUsefulBytes;
	pDrawHeader->m_nPcbringBegin          = 0;//m_nPcbringBegin;	 // note: this is the post-updated buffer counter!
	pDrawHeader->m_nResultantSpuDrawGet   = nResultantSpuDrawQueueSignal;
	
	#ifdef DBGFLAG_ASSERT
	SpuDrawQueue * pSignalDrawQueue = &m_spuDrawQueues[ nResultantSpuDrawQueueSignal & 3 ? 1 : 0 ];(void)pSignalDrawQueue;
	Assert( pSignalDrawQueue->IsValidCursor( (uint32*)( nResultantSpuDrawQueueSignal & ~3 ) ) );
	#endif
	uint nResultantSpuDrawQueueIndex = nResultantSpuDrawQueueSignal & 3;
	m_pDeferredChunkSubmittedTill[ nResultantSpuDrawQueueIndex ] = ( uint32* )( nResultantSpuDrawQueueSignal & ~3 );

	Assert( CELL_OK == cellSpursCheckJob( (const CellSpursJob256 *)pJob, sizeof( *pJob ), 256 ) );
	m_eaLastJobThatUpdatesSharedState = ( uintp )pJob;
	//PCB_RING_CTX.begin = PCB_RING_CTX.current = pSkipTo;   // submitted; now when needed, we'll wait for SPU to reply through shared state
	//Assert( PCB_RING_CTX.begin <= PCB_RING_CTX.end );

	PushSpuGcmJob( pJob );
	
	// after this job runs, it spawns FPCP job, which will advance the FPCP state
	m_nFpcpStateEndOfJournalIdxAtSpuGcmJob = g_pixelShaderPatcher.GetStateEndOfJournalIdx();

	if( SPUGCM_DEBUG_MODE )
	{
		// in SPUGCM_DEBUG_MODE, we execute all jobs and wait for them to complete. So, the GET pointer should always trail our pNext pointer
		Assert( g_spuGcmShared.m_nSpuDrawGet[ nResultantSpuDrawQueueIndex ] == ( nResultantSpuDrawQueueSignal & ~3 ) );
	}

	return pJob;
}


// BUG: pVertDecl may be released right after this call, we need to copy it somewhere or addref
void CSpuGcm::SubmitDrawBatch( IDirect3DVertexDeclaration9 *pVertDecl, OptimizedModel::OptimizedIndexBufferMarkupPs3_t *pIbMarkup )
{
	Assert( m_nCurrentBatch == BATCH_DRAW );
	SpuDrawHeader_t * pDrawHeader = ( SpuDrawHeader_t * )GetCurrentBatchCursor();

	if ( pIbMarkup )
	{
		Assert( pIbMarkup->kHeaderCookie == pIbMarkup->m_uiHeaderCookie );
		// real markup exists in this index buffer
		pDrawHeader->m_eaIbMarkup       = pIbMarkup;
		pDrawHeader->m_nIbMarkupPartitions = pIbMarkup->m_numPartitions;
	}
	else
	{
		pDrawHeader->m_eaIbMarkup       = NULL;
		pDrawHeader->m_nIbMarkupPartitions = 0;
	}
	
	if( IsDeferredDrawQueue() )
	{
		*( ( IDirect3DVertexDeclaration9 ** )( pDrawHeader + 1 ) ) = pVertDecl;
		OpenDeferredChunk();
		m_nCurrentBatch = BATCH_GCMSTATE;
		ValidateDeferredQueue();
	}
	else
	{
		PushDrawBatchJob( GetDrawQueue()->GetSignal(), pDrawHeader, pVertDecl, pIbMarkup );
	
		BeginGcmStateTransaction();
		ZPassCheckpoint( 8 );

		if ( SPUGCM_DEBUG_MODE )
		{
			GCM_FUNC( cellGcmSetWriteBackEndLabel, GCM_LABEL_DEBUG0, (uint)pDrawHeader );
			CmdBufferFinish();
			volatile uint32 * pDebugLabel = cellGcmGetLabelAddress( GCM_LABEL_DEBUG0 );
			while( *pDebugLabel != ( uint ) pDrawHeader )
			{
				// this may happen due to latency , but it won't be an infinite loop
				//Msg( "Hmmmm... WriteLabel; Finish(); but label isn't set yet! 0x%X != 0x%X\n", *pDebugLabel, (uint)pDrawHeader );
				continue;
			}
		}
	}
}


bool ZPass::CanBegin( )
{
	if( m_pCursor )
	{
		return false; // already begun
	}
	
	// we need at least some memory to store the job descriptor pointers
	if( GetSubchainCapacity( ) < 32 )
	{
		Warning( "Cannot begin ZPass: zpass job subchain buffer is full\n" );
		return false;
	}
	
	// we need a buffer in spuDrawQueue to store "ZPass begin, switch, end" commands
	// we may potentially need the space to store the whole state before ZPass, too

	return true;
}


void ZPass::Begin( uint32 * pCursor )
{
	m_pCursor = pCursor;
	m_nDrawPassSubchain = m_nPut;
	m_pSubchain = GetCurrentCommandPtr();
	*m_pSubchain = CELL_SPURS_JOB_COMMAND_JTS;
	m_nFpcpStateEndOfJournalIdxAtZPassBegin = g_pixelShaderPatcher.GetStateEndOfJournalIdx();
}

void ZPass::PushCommand( uint64 nCommand )
{
	Validate();
	Assert( GetSubchainCapacity() > 2 );
	uint64 * pLwsync = GetCurrentCommandPtr();
	m_nPut++;
	uint64 * pCommand = GetCurrentCommandPtr();
	m_nPut++;
	uint64 * pJts = GetCurrentCommandPtr();
	Validate();
	
	*pJts = CELL_SPURS_JOB_COMMAND_JTS;
	*pCommand = nCommand;
	__lwsync();
	*pLwsync = CELL_SPURS_JOB_COMMAND_LWSYNC;  // release the previous JTS
}



bool CSpuGcm::BeginZPass( )
{
	if( !IsDeferredDrawQueue() && m_zPass.CanBegin() )
	{
		// debug - do not checkin
// 		while( g_pixelShaderPatcher.GetJournalSpaceLeftSince( g_spuGcmShared.m_nFpcpStartRangesAfterLastSync ) > 20 )
// 		{
// 			g_pixelShaderPatcher.SetFragmentRegisterBlock(95, 1, (const float*)&g_spuGcmShared.m_eaFpcpSharedState->m_reg[95] );
// 		}
		if( m_nFpcpStateEndOfJournalIdxAtSpuGcmJob != g_pixelShaderPatcher.GetStateEndOfJournalIdx() )
		{
			GetDrawQueue()->Push2( SPUDRAWQUEUE_FLUSH_FPCP_JOURNAL, g_pixelShaderPatcher.GetStateEndOfJournalIdx() );
		}


		// this is where we start commands that we'll need to replay
		uint32 * pCursorBegin = GetDrawQueue()->GetCursor();
		uint nSafetyBufferWords = 4 ; // buffer so that when we come around, we can insert EndZPostPass method command (at least 3 words)
		uint nCommandWords = 			  2  // command : the command and EA of ZPassSavedState_t
			+ nSafetyBufferWords
			+ 4  // alignment buffer for ZPassSavedState_t
			+ sizeof( ZPassSavedState_t );
		m_zPass.m_nJobPoolMarker = m_jobPool128.GetMarker();

		uint32 * pCmdBeginZPrepass = GetDrawQueue()->AllocWords( nCommandWords );
		pCmdBeginZPrepass[0] = SPUDRAWQUEUE_BEGINZPREPASS_METHOD | ( SPUDRAWQUEUE_BEGINZPREPASS_MASK & nCommandWords );
		ZPassSavedState_t  * pSavedState = ( ZPassSavedState_t * )AlignValue( uintp( pCmdBeginZPrepass + 2 + nSafetyBufferWords ), 16 );
		pCmdBeginZPrepass[1] = ( uintp )pSavedState;
		m_zPass.m_pSavedState = pSavedState;
		
		// 
		// WARNING.
		// 
		// SPUDRAWQUEUE_BEGINZPREPASS_METHOD  must be the last method that modifies g_spuGcmShared.m_dxGcmState in a job_gcmflush SpuDrawQueue.
		// This is because its implementation doesn't wait for DMA put to finish.
		//
		
		GCM_PERF_PUSH_MARKER( "ZPass_Z" );
		CmdBufferFlush();

		// actually begin; don't let anyone overwrite the commands after cursor 
		m_zPass.Begin( pCursorBegin );
		GetDrawQueue()->PushFlushCallback( OnSpuDrawQueueFlushInZPass );
		PushSpuGcmCallSubchain( m_zPass.m_pSubchain ); // call all those SPUGCM jobs for the first time
		return true;
	}
	else
		return false;
}

void CSpuGcm::SetPredication( uint nPredicationMask ) // D3DPRED_* mask
{
	uint32 * pCmd = GetDrawQueue()->AllocWords( 1 );
	*pCmd = SPUDRAWQUEUE_PREDICATION_METHOD | ( SPUDRAWQUEUE_PREDICATION_MASK & nPredicationMask );
}




void CSpuGcm::EndZPass( bool bPopMarker )
{
	if( m_zPass && !m_zPass.m_isInEndZPass )
	{
		m_zPass.m_isInEndZPass = 1;
		GetDrawQueue()->PopFlushCallback();
		
		// as a precaution, since we don't need watermark-flush callbacks for the duration of this function, we'll disable it to avoid recursive flushes
		GetDrawQueue()->PushFlushCallback( OnSpuDrawQueueFlushDoNothing );
		
		// flush whatever state we may have.. it's not really needed to replay it twice, but whatever. we do need to replay it the 2nd time, and we can't just skip on it easily now in the 1st pass
		CmdBufferFlush();
		m_zPass.PushCommand( CELL_SPURS_JOB_COMMAND_RET );
		m_zPass.End(); //  at this point, there's no more "Z prepass". There's just a bunch of SPUGCM commands waiting to be executed
		
		// replay from cursor
		uint32 * pCmdEndZPrepass = GetDrawQueue()->AllocWords( 2 );
		//m_nGcmFlushJobScratchSize = MAX( m_nGcmFlushJobScratchSize, CELL_GCM_VTXPRG_MAX_CONST );
		pCmdEndZPrepass[0] = SPUDRAWQUEUE_ENDZPREPASS_METHOD;
		pCmdEndZPrepass[1] = ( uintp )m_zPass.m_pSavedState;
		if( bPopMarker )
		{
			GCM_PERF_POP_MARKER( /*"ZPass_Z"*/ ); 
			GCM_PERF_MARKER( "ZPass_ZEnd" );
		}
		else
		{
			GCM_PERF_MARKER( "ZPass_Abort" );
		}

		CmdBufferFlush(); // commit the "End Z Prepass" command. NOTE: we don't want to commit it twice, so we End ZPass BEFORE we commit this command

		// even though Z Prepass is ended now, all those commands and their memory are still intact
		// re-execute them here now
		PushSpuGcmCallSubchain( m_zPass.m_pSubchain ); // call all those SPUGCM jobs again!

		GetDrawQueue()->PopFlushCallback();
		// SPUGCM ring release point: after this point, we can simply wait for more space to become available in SPUGCM draw command ring
		
		// Do we need to really end the render pass? 
		// Hopefully not, because hopefully it'll just organically be indistinguishable from the non-Z-prepassed rendering		

		uint32 * pCmdEndZPostPass = GetDrawQueue()->AllocWords( 3 );
		pCmdEndZPostPass[0] = SPUDRAWQUEUE_ENDZPOSTPASS_METHOD;
		pCmdEndZPostPass[1] = m_zPass.m_nPut;
		pCmdEndZPostPass[2] = (uintp)&m_zPass.m_nGet;
		GCM_PERF_MARKER( bPopMarker ? "ZPass_RenderEnd" : "AbortedZPass_RenderEnd" );
		CmdBufferFlush();
		
		m_zPass.m_isInEndZPass = 0;
		
	}
	else
	{
		if( bPopMarker )
		{
			GCM_PERF_POP_MARKER( );
		}
	}
}



void ZPass::Init()
{
	m_nDummy = 0;
	m_pCursor = NULL;
	m_nJobs = 2048;
	m_pJobs = (uint64*)MemAlloc_AllocAligned( ( m_nJobs + 1 )* sizeof( uint64 ), 16 );
	m_pJobs[m_nJobs] = CELL_SPURS_JOB_COMMAND_NEXT( m_pJobs );
	m_nGet = 0;
	m_nPut = 0;
	m_isInEndZPass = 0;
}

void ZPass::Shutdown()
{
	MemAlloc_FreeAligned( m_pJobs );
}


//#endif

uint g_nEdgeJobChainMaxContention = 5;

void CSpuGcm::OnVjobsInit()
{
	int nJobPoolCount = Max<uint>( 256, g_spuGcmShared.m_fpcpRing.GetMaxJobsPerSegment() * 4 );
	int nCmdLineJobPoolCount = CommandLine()->ParmValue( "-spugcmJobPool", nJobPoolCount );
	if( nCmdLineJobPoolCount > nJobPoolCount && !( nCmdLineJobPoolCount & ( nCmdLineJobPoolCount - 1 ) ) )
	{
		Msg("Increasing spugcm cjob pool count from %d to %d\n", nJobPoolCount, nCmdLineJobPoolCount );
		nJobPoolCount = nCmdLineJobPoolCount;
	}
	// priority lower than the main job queue, in order to yield
	if( int nError = m_jobSink.Init( m_pRoot, 1, nJobPoolCount, ( uint8_t* )&m_pRoot->m_nSpugcmChainPriority, "spugcm", DMATAG_GCM_JOBCHAIN ) )
	{
		Error( "Cannot init SpuGcm, cell error %d\n", nError );
	}

	COMPILE_TIME_ASSERT( sizeof( job_edgegeom::JobDescriptor_t ) == 512 );
	if( int nError = g_spuGcmShared.m_edgeJobChain.Init( m_pRoot, g_nEdgeJobChainMaxContention, 128, ( uint8_t* )&m_pRoot->m_nEdgeChainPriority, sizeof( job_edgegeom::JobDescriptor_t ), CELL_SPURS_JOBQUEUE_DEFAULT_MAX_GRAB, "edge", DMATAG_EDGE_JOBCHAIN ) )
	{
		Error(" Cannot init SpuGcm, edge jobchain, error %d\n", nError );
	}
	
	if( int nError = g_spuGcmShared.m_fpcpJobChain.Init( m_pRoot, 1, 512, ( uint8_t* )&m_pRoot->m_nFpcpChainPriority, 128, CELL_SPURS_JOBQUEUE_DEFAULT_MAX_GRAB, "fpcp", DMATAG_FPCP_JOBCHAIN ) )
	{
		Error(" Cannot init SpuGcm, fpcp jobchain, error %d\n", nError );
	}

	if( nJobPoolCount < g_spuGcmShared.m_fpcpRing.GetMaxJobsPerSegment() * 4 ) // we need at least this much to avoid at least most stalls
	{
		Error( "Job pool count %d is too small! With %d jobs per segment, make it at least %d\n", nJobPoolCount, g_spuGcmShared.m_fpcpRing.GetMaxJobsPerSegment(), g_spuGcmShared.m_fpcpRing.GetMaxJobsPerSegment() * 4 );
	}

	

	m_jobPool128.Init( nJobPoolCount ); 
	g_spuGcmShared.m_jobPoolEdgeGeom.Init( 128 );
	
	g_spuGcmShared.m_jobFpcPatch2 = *( m_pRoot->m_pFpcPatch2 );
	g_spuGcmShared.m_jobEdgeGeom = *( m_pRoot->m_pEdgeGeom );

	if( m_pMlaaBuffer )	
	{
		g_edgePostWorkload.OnVjobsInit( m_pRoot );
	}
}


#if 0 // priorities test
bool PriorityTest_t::Test( class VjobChain4 *pJobChain )
{
	m_notify.m_nCopyFrom = 1;
	m_notify.m_nCopyTo = 0;
	uint nTick0 = __mftb();
	pJobChain->Run();
	uint nTick1 = __mftb();
	*( pJobChain->Push() ) = CELL_SPURS_JOB_COMMAND_JOB( &m_job );
	uint nTick2 = __mftb(), nTick3;
	do
	{
		nTick3 = __mftb();
		if( nTick3 - nTick2 > 79800000 * 5 )
		{
			Msg("%s:HANG\n", pJobChain->GetName());
			return false;
		}
	}
	while( !*(volatile uint32*)&m_notify.m_nCopyTo );

	Msg("%s[%d]:%5.0f+%5.0f(run=%5.0f)\n", pJobChain->GetName(), m_notify.m_nSpuId, (nTick2-nTick1)*40.1f, (nTick3-nTick2)*40.1f, (nTick1 - nTick0) * 40.1f );
	return true;
}


void CSpuGcm::TestPriorities()
{
	PriorityTest_t * pTest = (PriorityTest_t*)MemAlloc_AllocAligned( sizeof( PriorityTest_t ), 128 );

	V_memset( &pTest->m_job, 0, sizeof( pTest->m_job ) );
	pTest->m_job.header = *(m_pRoot->m_pJobNotify);
	pTest->m_job.header.useInOutBuffer = 1;
	AddInputDma( &pTest->m_job, sizeof( pTest->m_notify ), &pTest->m_notify );
	pTest->m_job.workArea.userData[1] = 0; // function: default 

	for( uint i  = 0; i < 50; ++ i)
	{
		if( !pTest->Test( &g_spuGcmShared.m_edgeJobChain ) )
			return ; // leak
		if( ! pTest->Test( &g_spuGcmShared.m_fpcpJobChain ) )
			return ; // leak
	}

	MemAlloc_FreeAligned( pTest );
}
#endif

void CSpuGcm::OnVjobsShutdown() // gets called before m_pRoot is about to be destructed and NULL'ed
{
	CmdBufferFinish();
	g_edgePostWorkload.OnVjobsShutdown( m_pRoot );
		
	// in case of priority issues with job chains (when experimenting with reload_vjobs), let's first end and then join all workloads
	m_jobSink.End();
	g_spuGcmShared.m_fpcpJobChain.End();
	g_spuGcmShared.m_edgeJobChain.End();

	m_jobSink.Join();
	g_spuGcmShared.m_fpcpJobChain.Join();
	g_spuGcmShared.m_edgeJobChain.Join();
	
	m_jobPool128.Shutdown();
	
	g_spuGcmShared.m_jobPoolEdgeGeom.Shutdown();
}

void CSpuGcm::Shutdown()
{
	g_pVJobs->Unregister( this ); // note: this will also call VjobsShutdown, which will join all SPU workloads and effectively call CmdBufferFinish();
	g_edgeGeomRing.Shutdown();
	if( m_pPcbringBuffer )
	{
		MemAlloc_FreeAligned( m_pPcbringBuffer );
	}
	m_spuDrawQueues[1].Shutdown();
	m_spuDrawQueues[0].Shutdown();
#if SPU_GCM_DEBUG_TRACE
	MemAlloc_FreeAligned( g_spuGcmShared.m_eaDebugTraceBuffer );
#endif
	m_zPass.Shutdown();

	for( uint i = 0; i < ARRAYSIZE( m_pDeferredStates ); ++i )
	{
		g_ps3gcmGlobalState.IoSlackFree( m_pDeferredStates[i] );
	}
}


void CSpuGcm::BeginScene()
{
	DrawQueueNormal();
	if( m_nFramesToDisableDeferredQueue > 0 )
	{
		m_nFramesToDisableDeferredQueue-- ;
	}
}


void CSpuGcm::EndScene()
{
	g_aici.m_nCpuActivityMask = g_edgeGeomRing.m_nUsedSpus;
	g_edgeGeomRing.m_nUsedSpus = 0;

	g_aici.m_nDeferredWordsAllocated = m_spuDrawQueues[1].m_nAllocWords - m_nDeferredQueueWords;
	m_nDeferredQueueWords = m_spuDrawQueues[1].m_nAllocWords;
	

	if( m_zPass )	
	{
		ExecuteNTimes( 100, Warning( "SpuGcm:EndScene must Abort ZPass; mismatched BeginZPass/EndZPass\n" ) );
		AbortZPass();
	}
	
	if( g_spuGcmShared.m_enableStallWarnings )
	{
	
		if( m_jobPool128.m_nWaitSpins > 100 )
		{
			if( g_spuGcmShared.m_enableStallWarnings )
			{
				Warning( "SpuGcm: %d spins in job pool, PPU is really ahead of SPU and (probably) RSX.\n", m_jobPool128.m_nWaitSpins );
			}
		}
		m_jobPool128.m_nWaitSpins = 0;
		
	/*
		if( m_jobPool256.m_nWaitSpins )
		{
			if( g_spuGcmShared.m_enableStallWarnings )
			{
				Warning( "SpuGcm: %d spins in job pool 256, PPU is really ahead of SPU and (probably) RSX.\n", m_jobPool256.m_nWaitSpins );
			}
			m_jobPool256.m_nWaitSpins = 0;
		}
	*/

		if( m_nPcbringWaitSpins > 100 )
		{
			if( g_spuGcmShared.m_enableStallWarnings )
			{
				Warning( "SpuGcm: %d spins in PcbRing, PPU is waiting for SPU (possibly) waiting for RSX\n", m_nPcbringWaitSpins );
			}
		}
		m_nPcbringWaitSpins = 0;
	}
	m_nFrame++;
	
	COMPILE_TIME_ASSERT( ARRAYSIZE( m_pDeferredStates ) == 2 ); // we need to rotate the array if it's not 2-element
	Swap( m_pDeferredStates[0], m_pDeferredStates[1] );

	extern ConVar r_ps3_mlaa;
	m_bUseDeferredDrawQueue = m_pMlaaBuffer && !( r_ps3_mlaa.GetInt() & 16 );
}





void CSpuGcm::CmdBufferFinish()
{
#ifdef CELL_GCM_DEBUG // [
	extern void (*fnSaveCellGcmDebugCallback)(struct CellGcmContextData*) = gCellGcmDebugCallback;
	gCellGcmDebugCallback = NULL;   // disable recursive callback 
#endif // ]

	s_nFinishLabelValue++;
	GCM_FUNC( cellGcmSetWriteBackEndLabel, GCM_LABEL_SPUGCM_FINISH, s_nFinishLabelValue );
	CmdBufferFlush();
	Assert( s_nStopAtFinishLabelValue != s_nFinishLabelValue );

	// now wait for RSX to reach
	uint nSpins = 0;
	uint nTbStart = __mftb();
	volatile uint32 * pLastJobUpdate = &g_spuGcmShared.m_eaLastJobThatUpdatedMe;
	while( ( s_nFinishLabelValue != *m_pFinishLabel ) ||
		( *pLastJobUpdate != m_eaLastJobThatUpdatesSharedState ) )
	{
		sys_timer_usleep( 30 ); // don't hog the PPU
		++nSpins;
#ifndef _CERT
		if( nSpins && ( nSpins % 100000 == 0 ) )
		{
			Warning(
				"** SpuGcm detected an SPU/RSX hang. **\n"
				);
		}
#endif
	}

	uint nTbEnd = __mftb();
	
	if( nSpins > 1000 )
	{
		Warning( "Long wait (%d us / %d spins) in CmdBufferFinish()\n", ( nTbEnd - nTbStart ) / 80, nSpins ); 
	}
	
#ifdef CELL_GCM_DEBUG // [
	gCellGcmDebugCallback = fnSaveCellGcmDebugCallback;
#endif // ]
}



void CSpuGcm::SyncMlaa( void * pLocalSurface )
{
	uint nInSurfaceOffset = ( g_ps3gcmGlobalState.m_nRenderSize[1]/2 * g_ps3gcmGlobalState.m_nSurfaceRenderPitch ) & -16;
	vec_int4 * pIn = ( vec_int4 * )( ( uintp( m_pMlaaBuffer ) + nInSurfaceOffset ) ), *pOut = ( vec_int4 * ) ( uintp( pLocalSurface ) + nInSurfaceOffset );
	
	
	uint nRowWidth = g_ps3gcmGlobalState.m_nSurfaceRenderPitch/64, nExclude = ( m_nFrame % ( nRowWidth - 2 ) ) + 1;
	for( uint nRow = 0; nRow < 4; ++nRow )
	{
		vec_int4 * pRowIn = AddBytes( pIn, g_ps3gcmGlobalState.m_nSurfaceRenderPitch * nRow );
		vec_int4 * pRowOut = AddBytes( pOut, g_ps3gcmGlobalState.m_nSurfaceRenderPitch * nRow );
		for( uint i = 0; i < nExclude; i ++ )
		{
			vec_int4 *input = pRowIn + i * 4, *output = pRowOut + i * 4;
			output[0] = vec_nor( input[0], input[0] );
			output[1] = vec_nor( input[1], input[1] );
			output[2] = vec_nor( input[2], input[2] );
			output[3] = vec_nor( input[3], input[3] );
		}
		
		for( uint i = nExclude + 1; i < nRowWidth ; ++i )
		{
			vec_int4 *input = pRowIn + i*4, *output = pRowOut + i*4;
			output[0] = vec_nor( input[0], input[0] );
			output[1] = vec_nor( input[1], input[1] );
			output[2] = vec_nor( input[2], input[2] );
			output[3] = vec_nor( input[3], input[3] );
		}
	}	
}


void CSpuGcm::CloseDeferredChunk()
{
	Assert( m_nSpuDrawQueueSelector == 1 );
	uint32 * pDeferredQueueCursor = m_spuDrawQueues[1].GetCursor();
	if( m_pDeferredChunkHead )
	{
		#ifdef _DEBUG
		m_nChunksClosedInSegment++;
		#endif
		// mark the previous chunk with its end
		m_pDeferredChunkHead[1] = ( uint32 )pDeferredQueueCursor;
		m_pDeferredChunkHead = NULL;
	}
	m_pDeferredQueueCursors[0] = pDeferredQueueCursor;
	ValidateDeferredQueue();
}



#if SPUGCM_DEBUG_MODE
uint g_nDeferredChunks[0x800][4], g_nDeferredChunkCount = 0;
#endif




uint32* CSpuGcm::OpenDeferredChunk( uint nHeader, uint nAllocExtra )
{
	Assert( IsValidDeferredHeader( nHeader ) );
	Assert( m_nSpuDrawQueueSelector == 1 );
	
	// skip allocation of the new chunk if the current chunk is empty
	if( !m_pDeferredChunkHead || m_pDeferredChunkHead + SPUDRAWQUEUE_DEFERRED_HEADER_WORDS != GetDrawQueue()->GetCursor() || nAllocExtra > 0 )
	{
		// we don't have an empty chunk already; allocate more
		CloseDeferredChunk();
		m_pDeferredChunkHead = GetDrawQueue()->AllocWords( SPUDRAWQUEUE_DEFERRED_HEADER_WORDS + nAllocExtra );
	}
	m_pDeferredChunkHead[0] = nHeader; // just flush state by default
	m_nDeferredChunkHead = nHeader;
	m_pDeferredChunkHead[1] = ( uintp )GetDrawQueue()->GetCursor();

	ValidateDeferredQueue();

	#ifdef _DEBUG
	if( SPUDRAWQUEUE_DEFERRED_HEADER_WORDS > 2 )
	{
		m_pDeferredChunkHead[2] = GetDrawQueue()->m_nAllocCount;
	}
	#endif
	#if SPUGCM_DEBUG_MODE
	uint nIdx = (g_nDeferredChunkCount++)%(ARRAYSIZE(g_nDeferredChunks));
	Assert( nIdx < ARRAYSIZE(g_nDeferredChunks) );
	uint * pDebug = g_nDeferredChunks[nIdx];
	pDebug[0] = nHeader;
	pDebug[1] = (uint32)m_pDeferredChunkHead;
	pDebug[2] = nAllocExtra;
	pDebug[3] = GetDrawQueue()->m_nAllocCount;
	#endif
	GetDrawQueue()->SetFlushWatermarkFrom( m_pDeferredChunkHead );
	return m_pDeferredChunkHead + SPUDRAWQUEUE_DEFERRED_HEADER_WORDS;
}


void CSpuGcm::DrawQueueNormal( bool bExecuteDeferredQueueSegment )
{
	if( m_nSpuDrawQueueSelector != 0 )
	{
		FillNops( &PCB_RING_CTX ); // switching draw queues, preallocated gcm context no longer usable
		Assert( *m_pDeferredChunkHead == SPUDRAWQUEUE_DEFERRED_GCMFLUSH_METHOD );
		GetDrawQueue()->Push1( SPUDRAWQUEUE_PERF_MARKER_DrawNormal );
		CloseDeferredChunk();
		m_pDeferredQueueCursors[0] = m_spuDrawQueues[1].GetCursor();
		/*uint nBytesInSegment = m_spuDrawQueues[1].Length( m_pDeferredQueueSegment, m_pDeferredQueueCursors[0] );
		Msg( "DrawQueueNormal %p..%p=%.1fKB (%p,%p)\n", m_pDeferredQueueSegment, m_pDeferredQueueCursors[0], 
			nBytesInSegment / 1024.0f,
			m_pDeferredQueueCursors[1], m_pDeferredQueueCursors[2] );*/
		m_nSpuDrawQueueSelector = 0;
		if( m_pDeferredQueueSegment && bExecuteDeferredQueueSegment )
		{
			ExecuteDeferredDrawQueueSegment( m_pDeferredQueueSegment, m_pDeferredQueueCursors[0], false );
			m_pDeferredQueueSegment = NULL;
		}

		Assert( m_pCurrentBatchCursor[0] == m_spuDrawQueues[0].GetCursor() );
		m_pDeferredChunkHead = NULL;
		BeginGcmStateTransaction();
	}						
	if( m_nFramesToDisableDeferredQueue > 0 )
	{
		ExecuteDeferredDrawQueue( 0 );
	}
}


/*
void CSpuGcm::DisableMlaaForTwoFrames()
{
	g_flipHandler.DisableMlaaForTwoFrames();
	m_nFramesToDisableDeferredQueue = 2; // this frame and next will have disabled deferred queue
	DrawQueueNormal();
}
*/

void CSpuGcm::DisableMlaa()
{
	DrawQueueNormal( false );
	// we could, but we don't have to flush the previous frame:
	// we'll do that at Flip, the same way we do it every time
	g_flipHandler.DisableMlaa();
}

void CSpuGcm::DisableMlaaPermanently()
{
	DrawQueueNormal( false );

	g_flipHandler.QmsAdviceBeforeDrawPrevFramebuffer();
	// flush previous frame first
	ExecuteDeferredDrawQueue( 1 );

	g_flipHandler.DisableMlaaPermannetly();
	g_flipHandler.DisableMlaa();

	extern void DxDeviceForceUpdateRenderTarget( );
	DxDeviceForceUpdateRenderTarget( ); // recover main render target, as it was screwed up by execution of previous frame's commands
	ExecuteDeferredDrawQueue( 0 );
}



CSpuGcm::DrawQueueDeferred_Result CSpuGcm::DrawQueueDeferred() // may flush previous frame deferred queue the first time
{
	DrawQueueDeferred_Result result;
	if( m_bUseDeferredDrawQueue && ( m_nFramesToDisableDeferredQueue == 0 ) && ( m_nSpuDrawQueueSelector != 1 ) )
	{
		FillNops( &PCB_RING_CTX ); // switching draw queues, preallocated gcm context no longer usable
		// do we have anything in the deferred queue?
		result.isFirstInFrame = m_pDeferredQueueCursors[0] == m_pDeferredQueueCursors[1];
		GetDrawQueue()->Push1( SPUDRAWQUEUE_PERF_MARKER_DrawDeferred );
		if( result.isFirstInFrame )
		{
			GetDrawQueue()->Push2( SPUDRAWQUEUE_DEFER_STATE, uintp( m_pDeferredStates[0] ) );
		}
		// before we dive into deferred queue, we flush the current queue, because we'll have to restart current queue when we dive out of deferred queue
		// this will also make sure that any state dump required for deferred queue to execute will be dumped before deferred queue will try to execute
		GcmStateFlush();
		Assert( m_pCurrentBatchCursor[0] == m_spuDrawQueues[0].GetCursor() );
		//ExecuteDeferredDrawQueue( 1 ); // dubious: we might want to execute this in the end of the frame to avoid undesirable state changes 
		m_nSpuDrawQueueSelector = 1;
		BeginGcmStateTransaction();
		m_pDeferredQueueSegment = m_spuDrawQueues[1].GetCursor();
		#ifdef _DEBUG
		m_nChunksClosedInSegment = 0;
		#endif
		//Msg( "DrawQueueDeferred %p / %.1f KB free...", m_pDeferredQueueSegment, m_spuDrawQueues[1].Length( m_pDeferredQueueSegment, m_spuDrawQueues[1].m_pGet ) );
		OpenDeferredChunk( SPUDRAWQUEUE_DEFERRED_GCMFLUSH_METHOD );
		if( result.isFirstInFrame ) // we defer the "UNDEFER" command in here
		{
			GetDrawQueue()->Push2( SPUDRAWQUEUE_UNDEFER_STATE, uintp( m_pDeferredStates[0] ) );
		}
	}
	else
	{	
		result.isFirstInFrame = false;
	}
	return result;
}



// returns: true if some memory will be freed up by SPU by poking into corresponding GET pointer later
bool CSpuGcm::ExecuteDeferredDrawQueue( uint nPrevious )
{
	Assert( !IsDeferredDrawQueue() );
	
	// just copy the commands to the main spugcm buffer
	Assert( m_pDeferredQueueCursors[0] == m_spuDrawQueues[1].GetCursor() || m_pDeferredQueueCursors[0] == m_pDeferredChunkHead );
	
	uint32 * pCmdEnd = m_pDeferredQueueCursors[nPrevious];//, *pCmdEnd = ( ( nPrevious == 0 ) ? m_spuDrawQueues[1].GetCursor() : m_pDeferredQueueCursors[ nPrevious - 1 ] );
	uint32 * pCmdBegin = m_pDeferredQueueCursors[ARRAYSIZE(m_pDeferredQueueCursors)-1];
	if( pCmdEnd == pCmdBegin )
		return false;
	//Msg( "ExecuteDeferredDrawQueue(%d) %p..%p=%.1fKB\n", nPrevious, pCmdBegin, pCmdEnd, m_spuDrawQueues[1].Length( pCmdBegin, pCmdEnd ) );
		
	FillNops( &PCB_RING_CTX );
	#if defined( _DEBUG ) && !defined( _CERT )
	m_spuDrawQueues[0].Push1( SPUDRAWQUEUE_PERF_MARKER_AAReplay );
	#endif
	
	GcmStateFlush();
	Assert( m_pCurrentBatchCursor[0] == m_spuDrawQueues[0].GetCursor() );// we're not deferred; so, GcmStateFlush calls BeginGcmStateTransaction that will reset the current batch cursor

	bool bMoveGet = ExecuteDeferredDrawQueueSegment( pCmdBegin, pCmdEnd, true );

	#if defined( _DEBUG ) && !defined( _CERT )
	m_spuDrawQueues[0].Push1( SPUDRAWQUEUE_PERF_MARKER_AAReplayEnd );
	SetCurrentBatchCursor( GetDrawQueue()->GetCursor() );
	#endif
	
	// forget about previously executed frames/chunks
	for( uint i = nPrevious + 1; i < ARRAYSIZE( m_pDeferredQueueCursors ); ++i )
		m_pDeferredQueueCursors[i] = pCmdEnd;

	return bMoveGet;
}


bool CSpuGcm::ExecuteDeferredDrawQueueSegment( uint32 * pCmdBegin, uint32 * pCmdEnd, bool bExecuteDraws )
{
	Assert( m_nCurrentBatch == BATCH_GCMSTATE );
	// if we're in deferred queue, we should switch to normal queue before drawing from deferred to normal queue
	Assert( !IsDeferredDrawQueue() );
	bool bMoveGet = false;
	uint nResultantSpuDrawQueueIndex = bExecuteDraws ? 1 : 2; // [2] is a dummy write-only resultant "GET" register..

#if SPUGCM_DEBUG_MODE
	uint nDeferredChunkDebugIdx = 0xFFFFFFFF;
	for( uint i = 1;i <= ARRAYSIZE( g_nDeferredChunks ); ++i )
	{
		uint j = ( g_nDeferredChunkCount - i ) & ( ARRAYSIZE( g_nDeferredChunks ) - 1 );
		if( g_nDeferredChunks[j][1] == uintp( pCmdBegin ) )
		{
			nDeferredChunkDebugIdx = j;
			break;
		}
	}
	Assert( nDeferredChunkDebugIdx < ARRAYSIZE( g_nDeferredChunks ) );
#endif

	SpuDrawQueue *pDrawQueue = &m_spuDrawQueues[1];
	for( uint32 * pCmd = pDrawQueue->NormalizeCursor( pCmdBegin ), * pCmdNormalizedEnd = pDrawQueue->NormalizeCursor( pCmdEnd ), *pPrev = pCmd; pCmd != pCmdNormalizedEnd; )
	{
		if( !IsCert() && !pDrawQueue->IsValidCursor( pCmd ) ) 
			DebuggerBreakIfDebugging();
		uint nCmd = *pCmd;
		if( nCmd == 0 )
		{
			pCmd++;
		}
		else if( ( nCmd & SPUDRAWQUEUE_METHOD_MASK ) == SPUDRAWQUEUE_NOPCOUNT_METHOD )
		{
			pCmd += 1 + ( nCmd & SPUDRAWQUEUE_NOPCOUNT_MASK );
		}
		else
		{
			uint32 * pNext = (uint32*)pCmd[1], *pCmdHeaderEnd = pCmd + SPUDRAWQUEUE_DEFERRED_HEADER_WORDS;
			Assert( m_spuDrawQueues[1].IsValidCursor( pNext ) );

#if SPUGCM_DEBUG_MODE
			for( uint i = 0; ; ++i )
			{
				uint j = ( nDeferredChunkDebugIdx + i ) & ( ARRAYSIZE( g_nDeferredChunks ) - 1 );
				if( g_nDeferredChunks[j][1] == uintp( pCmd ) )
				{
					nDeferredChunkDebugIdx = j;
					break;
				}
				if( i >= ARRAYSIZE( g_nDeferredChunks ) ) // stop if we don't find the  debug idx
				{
					DebuggerBreak();
					break;
				}
			}
#endif

			switch ( nCmd & SPUDRAWQUEUE_DEFERRED_METHOD_MASK )
			{
			case SPUDRAWQUEUE_DEFERRED_SET_FP_CONST_METHOD:
				{
					uint nStartRegister = ( nCmd >> 12 ) & 0xFFF, nRegisterCount = nCmd & 0xFFF;
					Assert( nStartRegister < 96 && nRegisterCount <= 96 );
					OnSetPixelShaderConstant();
					g_pixelShaderPatcher.SetFragmentRegisterBlock( nStartRegister, nRegisterCount, ( const float* )pCmdHeaderEnd );
					//m_dirtyCachesMask |= DxAbstractGcmState_t::kDirtyPxConstants;
				}
				break;
				

			case SPUDRAWQUEUE_DEFERRED_GCMFLUSH_METHOD:
				if( nCmd == SPUDRAWQUEUE_DEFERRED_GCMFLUSH_METHOD || bExecuteDraws )
				{
					PushStateFlushJob( pDrawQueue, uint( pNext ) | nResultantSpuDrawQueueIndex, pCmdHeaderEnd, pNext );
					Assert( m_pDeferredChunkSubmittedTill[nResultantSpuDrawQueueIndex] == pNext );
					bMoveGet = true;
				}
				break;

			case SPUDRAWQUEUE_DEFERRED_DRAW_METHOD:
				if( bExecuteDraws )
				{
					Assert( nCmd == SPUDRAWQUEUE_DEFERRED_DRAW_METHOD );
					SpuDrawHeader_t * pDrawHeader = ( SpuDrawHeader_t * )AlignValue( uintp( pCmdHeaderEnd ), 16 );
					// at the time we set up these deferred calls, we don't track the FPCP journal, so we need to refresh the indices referring into it here
					pDrawHeader->m_nFpcpEndOfJournalIdx = g_pixelShaderPatcher.GetStateEndOfJournalIdx();
					CellSpursJob128 * pDrawJob = PushDrawBatchJob( uint( pNext ) | nResultantSpuDrawQueueIndex, pDrawHeader, *( IDirect3DVertexDeclaration9** )( pDrawHeader + 1 ), pDrawHeader->m_eaIbMarkup );
					Assert( m_pDeferredChunkSubmittedTill[nResultantSpuDrawQueueIndex] == pNext ); 
					bMoveGet = true;
				}
				break;
			}
			pPrev = pCmd;
			pCmd = pNext;	
		}
		pCmd = pDrawQueue->NormalizeCursor( pCmd );
	}

	return bMoveGet;
}


void CSpuGcm::FlipDeferredDrawQueue()
{
	//Msg( "FlipDeferredDrawQueue {%p,%p,%p} Frame=%d\n", m_pDeferredQueueCursors[0], m_pDeferredQueueCursors[1], m_pDeferredQueueCursors[2], m_nFrame );
	Assert( !IsDeferredDrawQueue() );
	m_pDeferredQueueCursors[0] = m_spuDrawQueues[1].GetCursor();
	for( uint i = ARRAYSIZE( m_pDeferredQueueCursors ); i-- > 1; )
	{
		m_pDeferredQueueCursors[ i ] = m_pDeferredQueueCursors[ i - 1 ];
	}
}



void CEdgePostWorkload::OnVjobsInit( VJobsRoot* pRoot )
{
	uint numSpus = 5, nScratchSize = EDGE_POST_MLAA_HANDLER_SPU_BUFFER_SIZE( numSpus ) * 3;
	m_pMlaaScratch = MemAlloc_AllocAligned( nScratchSize, EDGE_POST_MLAA_HANDLER_BUFFER_ALIGN );
	int nOk = edgePostMlaaInitializeContext( &m_mlaaContext, numSpus, &pRoot->m_spurs, ( uint8_t* )&pRoot->m_nEdgePostWorkloadPriority, GCM_LABEL_EDGEPOSTMLAA, m_pMlaaScratch, nScratchSize );
	if( nOk != CELL_OK )
	{
		Warning("Cannot initialize MLAA, error %d\n", nOk );
		edgePostMlaaDestroyContext( &m_mlaaContext );
		MemAlloc_FreeAligned( m_pMlaaScratch );
		return;
	}
	m_isInitialized = true;
	
	
}

void CEdgePostWorkload::OnVjobsShutdown( VJobsRoot* pRoot )
{
	if( m_isInitialized )
	{
		edgePostMlaaDestroyContext( &m_mlaaContext );
		MemAlloc_FreeAligned( m_pMlaaScratch );
		m_isInitialized = false;
	}
}






int32_t GhostGcmCtxCallback( struct CellGcmContextData *pContext, uint32_t nCount )
{
	Error("Trying to allocate %d more words in the ghost context\n", nCount );
	return CELL_ERROR_ERROR_FLAG;
}

enum TruePauseStateEnum_t
{
	TRUE_PAUSE_NONE,
	TRUE_PAUSE_SPINNING,
	TRUE_PAUSE_LOCKED0, // locked, Shoulder and X buttons down
	TRUE_PAUSE_LOCKED1, // locked, Shoulder button up
	TRUE_PAUSE_SINGLE_STEP
};					  

TruePauseStateEnum_t g_nTruePauseState = TRUE_PAUSE_NONE;


bool CSpuGcm::TruePause()
{
	switch( g_nTruePauseState )
	{
		case TRUE_PAUSE_NONE:
			g_nTruePauseState = TRUE_PAUSE_SPINNING;
		case TRUE_PAUSE_SINGLE_STEP:
			break; // re-entering after single step
		default:
			g_nTruePauseState = TRUE_PAUSE_NONE;
			return false; // inconsistent state, don't try to continue
	}

	CmdBufferFinish(); // this'll put the end marker to the last frame.
	g_spuGcmShared.m_sysring.NotifyRsxGet( g_spuGcmShared.m_eaGcmControlRegister->get );
	
	//Assert( g_spuGcmShared.m_sysring.m_nPut == g_spuGcmShared.m_sysring.m_nEnd );
	const uint nReserve = 0x1000;
	if( !g_spuGcmShared.m_sysring.CanPutNoWrap( nReserve ) )
	{
		if( !g_spuGcmShared.m_sysring.CanWrapAndPut( nReserve ) )
		{
			Msg( "Cannot replay because sysring wraps around right here and you got unlucky. If you get this a lot, ask Sergiy to implement/fix wrap-around replay\n" );
			return false;
		}
		g_spuGcmShared.WrapSequence();
	}
	
	int nReplayFrames = 2;
	
	if( !g_spuGcmShared.CanReplayPastFrames( nReplayFrames, nReserve ) )
	{
		uint nSysringBytesNeeded = 0;
		Warning( "Cannot replay frames: %d frames didn't fit into command buffer of %d bytes and was generated and executed in multiple passes/segments\n", nReplayFrames, g_ps3gcmGlobalState.m_nCmdSize );
		return false;
	}
	
	// all relevant SPU, RSX activity ceased at this point
	uintp eaEnd = g_spuGcmShared.m_sysring.EaPut();
	uint32 * pEnd = (uint32*)eaEnd;
	uint nIoOffsetEnd = eaEnd + g_spuGcmShared.m_nIoOffsetDelta;
	
	//nOffsetBeginFrame = g_spuGcmShared.m_sysring.PutToEa( g_spuGcmShared.GetPastFrame(2).m_nSysringBegin ) + g_spuGcmShared.m_nIoOffsetDelta;

	//uint nSurfaceFlipIndex = g_ps3gcmGlobalState.m_display.surfaceFlipIdx, nSurfaceFlipAltIndex = g_ps3gcmGlobalState.m_display.PrevSurfaceIndex();
	
	//CPs3gcmLocalMemoryBlock &altSurface = g_ps3gcmGlobalState.m_display.surfaceColor[nSurfaceFlipAltIndex];
	//V_memset( altSurface.DataInAnyMemory(), 0, altSurface.Size() );
	
	int nCurrentReplayFrame = 1;
	// Note: we probably shouldn't start with the frame rendering in the same surface as the last frame flipped..

	uint32 * pReplayLabelReset = (uint32*)g_spuGcmShared.m_sysring.EaPut();
	uint nReplayLabelResetIoOffset = uintp( pReplayLabelReset ) + g_spuGcmShared.m_nIoOffsetDelta;
	CellGcmContextData ghostCtx;
	ghostCtx.current = ghostCtx.begin = pReplayLabelReset;
	uint32 * pGhostAreaEnd = ghostCtx.end = ghostCtx.begin + ( nReserve / sizeof( uint32 ) );
	ghostCtx.callback = GhostGcmCtxCallback;
	
	cellGcmSetWriteBackEndLabel( &ghostCtx, GCM_LABEL_REPLAY,  0  );
	
	uint32 * pReplayGhostArea = ghostCtx.current;
	uint nReplayGhostAreaIoOffset = uintp( pReplayGhostArea ) + g_spuGcmShared.m_nIoOffsetDelta;
	
	g_spuGcmShared.m_sysring.Put( uintp( pReplayGhostArea ) - uintp( pReplayLabelReset ) );
	Assert( g_spuGcmShared.m_sysring.EaPut() == uintp( pReplayGhostArea ) );
	
	volatile uint32 * pLabelReplay = cellGcmGetLabelAddress( GCM_LABEL_REPLAY );
	*pLabelReplay = 0xFFFFFFFF;
	__sync();
	
	bool isFirstIteration = true;

	do	
	{
		g_spuGcmShared.m_eaGcmControlRegister->put = nReplayGhostAreaIoOffset;

		while( *pLabelReplay != 0 )
			continue;
		// we're now synchronized at the beginning of ghost area
		switch( g_nTruePauseState )
		{
			case TRUE_PAUSE_NONE:
				return false;
			case TRUE_PAUSE_SINGLE_STEP:
				if( !isFirstIteration )
				{
					return true;
				}
				break;
		}

		const BeginFrameRecord_t &pastFrame = g_spuGcmShared.GetPastFrame( nCurrentReplayFrame );
		int nOffsetBeginFrame = uintp( pastFrame.m_eaBegin ) + g_spuGcmShared.m_nIoOffsetDelta, nOffsetEndFrame = uintp( pastFrame.m_eaEnd ) + g_spuGcmShared.m_nIoOffsetDelta;
		
		Msg("frame@ %X..%X ", nOffsetBeginFrame , nOffsetEndFrame );

		ghostCtx.current = ghostCtx.begin = pReplayGhostArea;
		ghostCtx.end = pGhostAreaEnd;

		*( ghostCtx.current++ ) = CELL_GCM_JUMP( nOffsetBeginFrame ); // jump to the beginning of the frame we want to replay
		uint32 nOffsetReturnFromFrame = uintp( ghostCtx.current ) + g_spuGcmShared.m_nIoOffsetDelta;

		Assert( pastFrame.m_eaEnd[0] == 0 && pastFrame.m_eaEnd[1] == 0 && pastFrame.m_eaEnd[2] == 0 && pastFrame.m_eaEnd[3] == 0 ); // we expect 4 NOPs at the end of the frame
		Assert( pastFrame.m_eaBegin[0] == 0 && pastFrame.m_eaBegin[1] == 0 && pastFrame.m_eaBegin[2] == 0 && pastFrame.m_eaBegin[3] == 0 ); // we expect 4 NOPs at the beginning of the frame
		pastFrame.m_eaEnd[0] = CELL_GCM_JUMP( nOffsetReturnFromFrame ); // return to replay area after rendering the whole frame
		cellGcmSetWriteBackEndLabel( &ghostCtx, GCM_LABEL_REPLAY,  1  );
		
		__sync();
		uint32 nTickStart = __mftb(); // let's start rendering (replaying) the captured GCM frame 
		g_spuGcmShared.m_eaGcmControlRegister->put = uintp( ghostCtx.current ) + g_spuGcmShared.m_nIoOffsetDelta;
		while( *pLabelReplay != 1 )
			continue;		
		
		int nSurfaceFlipIndex = g_ps3gcmGlobalState.m_display.PrevSurfaceIndex( nCurrentReplayFrame ); 
		Assert( nSurfaceFlipIndex >= 0 );
		
		while ( cellGcmGetFlipStatus() != CELL_GCM_DISPLAY_FLIP_STATUS_DONE )
		{
			// Wait for the previous flip to completely finish
			ThreadSleep( 1 );
		}

		cellGcmResetFlipStatus();	// Need to reset GCM flip status

		// start flipping
		cellGcmSetFlip( &ghostCtx, nSurfaceFlipIndex );
		
		cellGcmSetWriteBackEndLabel( &ghostCtx, GCM_LABEL_REPLAY,  2  );

		int nOffsetEndOfFlip = uintp( ghostCtx.current ) + g_spuGcmShared.m_nIoOffsetDelta;

		cellGcmSetWriteBackEndLabel( &ghostCtx, GCM_LABEL_REPLAY,  3  ); // reset label

		*( ghostCtx.current++ ) = CELL_GCM_JUMP( nReplayLabelResetIoOffset );

		__sync();
		
		g_spuGcmShared.m_eaGcmControlRegister->put = nOffsetEndOfFlip;
		Msg( "[%d.%d] flip@ %X..%X. ", nCurrentReplayFrame, nSurfaceFlipIndex, nReplayGhostAreaIoOffset, nOffsetEndOfFlip );

		while( *pLabelReplay != 2 )
			continue;

		uint32 nFrameEnd = __mftb();	Msg( "%.2f ..ms.\n", ( nFrameEnd - nTickStart ) / 79800.0f );

		while ( cellGcmGetFlipStatus() != CELL_GCM_DISPLAY_FLIP_STATUS_DONE )
		{
			// Wait for the previous flip to completely finish
			ThreadSleep( 1 );
		}
		
		uint32 nFlipEnd = __mftb();	Msg( "%.2f ms.\n", ( nFlipEnd - nTickStart ) / 79800.0f );
		

		pastFrame.m_eaEnd[0] = CELL_GCM_METHOD_NOP;
		__sync();
		nCurrentReplayFrame = ( nCurrentReplayFrame + nReplayFrames - 1 ) % nReplayFrames;
		
		int bContinueProcessing = 0;
		CellPadData padData;
		do
		{
			int nError = cellPadGetData( 0, &padData );
			if( nError )
			{
				Msg( "Error 0x%X trying to get pad data, aborting true pause\n", nError );
				g_nTruePauseState = TRUE_PAUSE_NONE;
				return false;
			}
			else
			{
				if( padData.len >= 3 )
				{
					int isL1Down = padData.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R1;
					int isTriangleDown = padData.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_UP;
					int isCrossDown = padData.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_DOWN;
					int isCircleDown = padData.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_RIGHT;
					
					bContinueProcessing = isTriangleDown; // go into infinite loop here if the triangle is down
					
					int isLockDown = isCrossDown, isSingleStepDown = isCircleDown, isPauseDown = isL1Down;

					if( g_nTruePauseState != TRUE_PAUSE_SINGLE_STEP && isSingleStepDown )
					{
						g_nTruePauseState = TRUE_PAUSE_SINGLE_STEP;
						bContinueProcessing = false; // return to render a single step
					}
					
					switch( g_nTruePauseState )
					{
					case TRUE_PAUSE_LOCKED1:
					case TRUE_PAUSE_LOCKED0:
						if( isPauseDown )
						{
							if( g_nTruePauseState == TRUE_PAUSE_LOCKED1 )
							{
								g_nTruePauseState = TRUE_PAUSE_NONE; // second press on the shoulder releases the lock
								bContinueProcessing = false;
							}
						}
						else
						{
							if( g_nTruePauseState == TRUE_PAUSE_LOCKED0 )
							{
								g_nTruePauseState = TRUE_PAUSE_LOCKED1; // promote: shoulder isn't pressed any more
							}
						}
						
						break;

					case TRUE_PAUSE_SPINNING:
						
						if( isLockDown )
						{
							g_nTruePauseState = TRUE_PAUSE_LOCKED0;
						}
						else if( isSingleStepDown )
						{
							g_nTruePauseState = TRUE_PAUSE_SINGLE_STEP;
							bContinueProcessing = false; // do the single step
						}
						else if( !isPauseDown )
						{
							if( isFirstIteration )
							{
								g_nTruePauseState = TRUE_PAUSE_LOCKED1;	 // assume we go into locked state if L1 wasn't pressed the very first frame
							}
							else
							{
								g_nTruePauseState = TRUE_PAUSE_NONE;
								bContinueProcessing = false;
							}
						}
						break;

					case TRUE_PAUSE_SINGLE_STEP:
						// we skipped one render frame; go into normal spinning state as soon as the user depresses circle
						if( !isSingleStepDown )
						{
							if( isPauseDown )
							{
								g_nTruePauseState = TRUE_PAUSE_SPINNING; // the shoulder is still down, so the user didn't decide yet if they want to let the game go
							}
							else
							{
								g_nTruePauseState = TRUE_PAUSE_LOCKED1; // we let the shoulder go, so it must be a locked state
							}
						}
						
						break;
					}
				}
			}
			isFirstIteration = false;
		}
		while( bContinueProcessing );
	}
	while( true );
	
	return false;
}

static ConVar spugcm_validatedeferredqueue( "spugcm_validatedeferredqueue", "0" );

void CSpuGcm::ValidateDeferredQueue()
{
#ifdef _DEBUG
	if( !spugcm_validatedeferredqueue.GetBool() )
		return;
	uint32 * pCmdEnd = m_pDeferredChunkHead;
	if( !pCmdEnd )
		pCmdEnd = m_pDeferredQueueCursors[0];
	pCmdEnd = m_spuDrawQueues[1].NormalizeCursor( pCmdEnd );
	Assert( m_spuDrawQueues[1].IsValidCursor( pCmdEnd ) );
	uint32 * pCmdBegin = m_pDeferredQueueCursors[ARRAYSIZE(m_pDeferredQueueCursors)-1];
	uint nWraps = 0;
	for( uint32 * pCmd = pCmdBegin; pCmd != pCmdEnd;  )
	{
		uint nCmd = *pCmd;
		if( nCmd == 0 )
		{
			pCmd++;
		}
		else if( ( nCmd & SPUDRAWQUEUE_METHOD_MASK ) == SPUDRAWQUEUE_NOPCOUNT_METHOD )
		{
			pCmd += 1 + ( nCmd & SPUDRAWQUEUE_NOPCOUNT_MASK );
		}
		else
		{
			Assert( IsValidDeferredHeader( nCmd ) );
			Assert( nWraps == 0 || pCmd < pCmdBegin );
			Assert( m_spuDrawQueues[ 1 ].IsValidCursor( pCmd ) );
			uint32 * pNext = ( uint32* )pCmd[ 1 ];
			Assert( m_spuDrawQueues[ 1 ].IsValidCursor( pNext ) );
			if( pNext < pCmd )
			{
				Assert( nWraps == 0 );
				nWraps++;
			}
			pCmd = pNext;
		}
		pCmd = m_spuDrawQueues[1].NormalizeCursor( pCmd );
	}
#endif
}
