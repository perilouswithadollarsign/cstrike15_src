// Copyright © 2010, Valve Corporation, All rights reserved. ========

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier1/convar.h"
#include "ps3/ps3gcmlabels.h"
#include "ps3gcmstate.h"
#include "spugcm.h"
#include "rsxflip.h"

CFlipHandler g_flipHandler;

ConVar r_drop_user_commands( "r_drop_user_commands", "0" );
ConVar r_ps3_mlaa( "r_ps3_mlaa", "1" ); // 

ConVar r_ps3_vblank_miss_threshold( "r_ps3_vblank_miss_threshold", "0.08", FCVAR_DEVELOPMENTONLY, "How much % of vsync time is allowed after vblank for frames that missed vsync to tear and flip immediately" );

#if GCM_ALLOW_TIMESTAMPS
int32 g_ps3_timestampBeginIdx = GCM_REPORT_TIMESTAMP_FRAME_FIRST;
#endif

#if 0 // defined(_DEBUG)
char ALIGN16 g_flipLog[256][32] ALIGN16_POST;
uint g_flipLogIdx = 0;
#define FLIP_LOG(MSG,...)																   \
{																							\
	uint nLogIdx = cellAtomicIncr32( &g_flipLogIdx ) & ( ARRAYSIZE( g_flipLog ) - 1 );		 \
	int nCount = V_snprintf( g_flipLog[nLogIdx], sizeof( g_flipLog[nLogIdx] ), MSG,  ##__VA_ARGS__ ); \
	int zeroSize = sizeof( g_flipLog[0] ) - 4 - nCount;				   \
	V_memset( g_flipLog[nLogIdx] + nCount, 0, zeroSize );               \
	*(uint32*)( g_flipLog[nLogIdx] + sizeof( g_flipLog[0] ) - 4 ) = __mftb(); \
}
#define ENABLE_FLIP_LOG 1
#define FlipAssert( X ) do{if(!(X))DebuggerBreak();}while(false)
uint g_flipUserCommands[1024][2];
#else
#define FLIP_LOG(MSG,...)
#define FlipAssert( X )
#define ENABLE_FLIP_LOG 0
#endif


void CEdgePostWorkload::Kick( void * dst, uint nSetLabel )
{
	if( !m_isInitialized )
		return;

	extern ConVar r_ps3_mlaa;
	FLIP_LOG("mlaa %d,mode=%Xh,label=%d", nSetLabel, g_flipHandler.m_nMlaaFlagsThisFrame, *m_mlaaContext.rsxLabelAddress );
	edgePostMlaaWait( &m_mlaaContext );
	FlipAssert( vec_all_eq( *g_spuGcm.m_pMlaaBufferCookie, g_vuSpuGcmCookie ) );
	//FLIP_LOG("mlaa init %d", nSetLabel );
	edgePostInitializeWorkload( &m_workload, m_stages, STAGE_COUNT );
	bool isMlaaRelativeEdgeDetection = true;
	uint8
		nMlaaThresholdBase (0x0a), // from Edge sample: these are pretty good threshold values, but you might find better ones...
		nMlaaThresholdFactor(0x59), 
		nMlaaAbsoluteThreshold(0x20);
		
	uint nWidth = g_ps3gcmGlobalState.m_nRenderSize[0], nHeight = g_ps3gcmGlobalState.m_nRenderSize[1];
	FlipAssert( nWidth <= 1280 && nWidth >= 640 && nHeight <= 720 && nHeight >= 480 );

	//FLIP_LOG("mlaa prep %d", nSetLabel );
	edgePostMlaaPrepareWithRelativeThreshold( &m_mlaaContext, g_spuGcm.m_pMlaaBuffer, IsResultInMainMemory()? g_spuGcm.m_pMlaaBufferOut : dst,
		nWidth, nHeight,
		g_ps3gcmGlobalState.m_nSurfaceRenderPitch,
		isMlaaRelativeEdgeDetection?nMlaaThresholdBase:nMlaaAbsoluteThreshold,
		isMlaaRelativeEdgeDetection?nMlaaThresholdFactor:0,
		g_flipHandler.m_nMlaaFlagsThisFrame,
		nSetLabel );

	//FLIP_LOG("mlaa kick %d", nSetLabel );
	edgePostMlaaKickTasks( &m_mlaaContext );
	FLIP_LOG("mlaa kicked %d,label=%d", nSetLabel, *m_mlaaContext.rsxLabelAddress );
	FlipAssert( vec_all_eq( *g_spuGcm.m_pMlaaBufferCookie, g_vuSpuGcmCookie ) );
}


void RsxInterruptFifo::Init()
{
	m_nGet = m_nPut = 0;
}

uint RsxInterruptFifo::Queue( uint8 nCause, uint8 nSurfaceFlipIdx )
{
	Event_t event;
	event.m_nCause = nCause;
	event.m_nSurfaceFlipIdx = nSurfaceFlipIdx;
	return Queue( event );
}


uint RsxInterruptFifo::Queue( const Event_t &event )
{
	while( ( m_nPut - m_nGet ) >= MAX_EVENT_COUNT - 1 )
	{
		sys_timer_usleep( 100 ); // this should NEVER happen
	}

	#if ENABLE_FLIP_LOG
	switch( event.m_nCause )
	{
	case GCM_USERCMD_POSTPROCESS:
		FLIP_LOG( "queue:post %d", event.m_nSurfaceFlipIdx );
		break;
	case GCM_USERCMD_FLIPREADY:
		FLIP_LOG( "queue:flip %d sys%d", event.m_nSurfaceFlipIdx, g_flipHandler.m_nSystemFlipId[ event.m_nSurfaceFlipIdx ] );
		break;
	default:
		FLIP_LOG("Unknown event %d", event.m_nCause );
		break;
	}
	#endif

	m_queue[ m_nPut & ( MAX_EVENT_COUNT - 1 ) ] = event;
	return ++m_nPut; // Should be atomic if there are multiple event producer threads
}




uint RsxInterruptFifo::GetPutMarker()const
{
	return m_nPut;
}

int RsxInterruptFifo::HasEvents( uint nMarker )
{
	uint nGet = m_nGet;
	Assert( int( nMarker - nGet ) >= 0 );
	return int( nMarker - nGet );
}

RsxInterruptFifo::Event_t & RsxInterruptFifo::PeekEvent()
{
	uint nGet = m_nGet;
	Assert( nGet != m_nPut );
	return m_queue[ nGet & ( MAX_EVENT_COUNT - 1 ) ];
}

const RsxInterruptFifo::Event_t RsxInterruptFifo::DequeueEvent( )
{
	Event_t event = PeekEvent();
	m_nGet++; // should be atomic if there's more than one consumer
	return event;
}

void RsxInterruptFifo::QueueRsxInterrupt()
{
	uint32 *pReplace = NULL;
	#if ENABLE_FLIP_LOG
	//FLIP_LOG( "q%X", m_nPut );
	g_flipUserCommands[ m_nPut & ( ARRAYSIZE( g_flipUserCommands ) - 1 ) ][ 0 ] = m_nPut;
	pReplace = &g_flipUserCommands[ m_nPut & ( ARRAYSIZE( g_flipUserCommands ) - 1 ) ][ 1 ];
	*pReplace = uint32( gCellGcmCurrentContext->current );
	#endif
/*
	if( IsCert() // don't deliberately drop anything in CERT
	|| 0 == r_drop_user_commands.GetInt() // don't drop anything if drop==0
	|| ( ( rand() % 100 ) >= r_drop_user_commands.GetInt() ) // drop 1% means in 99% of cases we still want to SetUserCommand
	)
		GCM_FUNC( cellGcmSetUserCommand, m_nPut );
	GCM_FUNC( cellGcmSetWriteTextureLabel, GCM_LABEL_LAST_INTERRUPT_GET, m_nPut );
*/
	// directly putting it to SPUGCM queue instead of routing it through GCM_FUNC
	g_spuGcm.GetDrawQueue()->Push3( SPUDRAWQUEUE_QUEUE_RSX_INTERRUPT_METHOD | GCM_LABEL_LAST_INTERRUPT_GET, m_nPut, ( uintp )pReplace );
}




void CFlipHandler::Init()
{
	m_interruptFifo.Init();
	
/*
	V_memset( m_nDebugStates, 0, sizeof( m_nDebugStates ) );
	m_nDebugStates[RENDERING_SURFACE] = -1;
*/

	m_nFlipSurfaceIdx = 0;
	m_nFlipSurfaceCount = 0;
	m_nVblankCounter = 100;  // how many vblanks since the last flip?
	m_bEdgePostResultAlreadyInLocalMemory = false;
	m_nMlaaFlagsThisFrame = 0; // disable MLAA before the first BeginScene() is called
	m_nMlaaFlagMaskNextFrame = ~0u;

	for( int i = 0; i < ARRAYSIZE( m_surfaceEdgePost ) ; ++i ) // initially, the post processing of surfaces is disabled
		m_surfaceEdgePost[i] = 0;

	// simulated initial state: we just flipped to surface 1, then 2, thus leaving surface 1 (then 0) available to render into
	// event[1] may not be set for MLAA mode because in order to start rendering into surface 0 (which we're rendering into), we "waited" for event 1

	for ( int j = 2; j < ARRAYSIZE( m_evFlipReady ); ++ j )
		m_evFlipReady[j].Set();
	
	//m_nLastFlippedSurfaceIdx = CPs3gcmDisplay::SURFACE_COUNT - 1 ;
	m_pLastInterruptGet = cellGcmGetLabelAddress( GCM_LABEL_LAST_INTERRUPT_GET );
	*m_pLastInterruptGet = 0;
	

	cellGcmSetVBlankHandler( INTERRUPT_VBlankHandler );
	cellGcmSetUserHandler( INTERRUPT_UserHandler );
}


void CFlipHandler::Shutdown()
{
	cellGcmSetVBlankHandler( NULL );
	cellGcmSetUserHandler( NULL );
}

//////////////////////////////////////////////////////////////////////////
// 1. draw PS/3 system menus into the surface
// 2. queue a reliable "flip ready" event for GCM interrupt thread to process and flip surface to this 
//
void CFlipHandler::QmsPrepareFlipSubmit( GcmUserCommandEnum_t nEvent, uint surfaceFlipIdx )
{
	uint32 nSystemFlipId = GCM_FUNC_NOINLINE( cellGcmSetPrepareFlip, surfaceFlipIdx ); 
	m_nSystemFlipId[surfaceFlipIdx] = nSystemFlipId;
	Assert( !m_evFlipReady[ surfaceFlipIdx ].Check() );
	m_interruptFifo.Queue( nEvent, surfaceFlipIdx );
}


ConVar r_ps3_mlaa_pulse( "r_ps3_mlaa_pulse", "0" );
enum EdgePostFlags_t {
	EDGE_POST_MLAA_FLAG_MASK = ( EDGE_POST_MLAA_MODE_ENABLED | EDGE_POST_MLAA_MODE_SHOW_EDGES | EDGE_POST_MLAA_MODE_SINGLE_SPU_TRANSPOSE | EDGE_POST_MLAA_MODE_TRANSPOSE_64 )
};


void CFlipHandler::BeginScene()
{
#if GCM_ALLOW_TIMESTAMPS
	if ( g_ps3_timestampBeginIdx >= 0 )
	{
		GCM_FUNC( cellGcmSetTimeStamp, g_ps3_timestampBeginIdx );
		g_ps3_timestampBeginIdx = -1;
	}
#endif
	m_nMlaaFlagsThisFrame = r_ps3_mlaa.GetInt() & EDGE_POST_MLAA_FLAG_MASK;
	if( int nPulse = r_ps3_mlaa_pulse.GetInt() )
	{
		if( 1 & ( g_spuGcm.m_nFrame / nPulse ) )
		{
			m_nMlaaFlagsThisFrame = 0; // disable for 16 frames = 1/2 second 
		}
	}
	m_nMlaaFlagsThisFrame &= m_nMlaaFlagMaskNextFrame;

	//m_nMlaaFlagMaskNextFrame = (uint)-1;
}

void CFlipHandler::TransferMlaaResultIfNecessary( uint nSurfacePrevFlipIdx )
{
	if( m_bEdgePostResultAlreadyInLocalMemory )
		return;
	
	if( g_edgePostWorkload.ShouldUseLabelForSynchronization() )
	{
		GCM_FUNC( cellGcmSetWaitLabel, GCM_LABEL_EDGEPOSTMLAA, nSurfacePrevFlipIdx );
	}
	else
	{
		// wait for SPU to finish post-processing previous surface
		uint32 *pPrevJts = &g_spuGcm.m_pEdgePostRsxLock[ nSurfacePrevFlipIdx ];
		if( *pPrevJts != CELL_GCM_RETURN() )
		{
			GCM_FUNC( cellGcmSetCallCommand, uintp( pPrevJts ) + g_spuGcmShared.m_nIoOffsetDelta );
		}
	}

	//
	// NOTE: we can start post-processing before SetPrepareFlip, it only makes sense since we don't always use interrupt to do so
	// if we ever do proper synchronization with SPU workload, we should kick Edge Post here, before SetPrepareFlip
	//

	if( g_edgePostWorkload.IsResultInMainMemory() )
	{
		CPs3gcmLocalMemoryBlockSystemGlobal & prevSurfaceColor = g_ps3gcmGlobalState.m_display.surfaceColor[nSurfacePrevFlipIdx];
		GCM_FUNC( cellGcmSetTransferImage, CELL_GCM_TRANSFER_MAIN_TO_LOCAL,
			prevSurfaceColor.Offset(), g_ps3gcmGlobalState.m_nSurfaceRenderPitch, 0, 0,			
			uintp( g_spuGcm.m_pMlaaBufferOut ) + g_ps3gcmGlobalState.m_nIoOffsetDelta, g_ps3gcmGlobalState.m_nSurfaceRenderPitch, 0, 0,
			g_ps3gcmGlobalState.m_nRenderSize[0], g_ps3gcmGlobalState.m_nRenderSize[1], 
			4 );
	}
	m_bEdgePostResultAlreadyInLocalMemory = true;
}




bool CFlipHandler::QmsAdviceBeforeDrawPrevFramebuffer()
{
	uint nSurfacePrevFlipIdx = g_ps3gcmGlobalState.m_display.PrevSurfaceIndex( 1 );
	uint8 prevPostProcessed = m_surfaceEdgePost[nSurfacePrevFlipIdx];
	if( prevPostProcessed ) // did previous surface need post-processing?
	{
		// we'd actually be free to start MLAA here instead of in Flip, for the cost of one more RSX->PPU interrupt
		// but we don't do that because we only may do so when the LAST player draws, and we don't know if this post processing
		// that will now start is related to the LAST player

		// we don't need to do that until flip if we're using deferred queue
		// although if we're using deferred queue and we run out of space there, we stop using it, replay it and start defer-render into previous frame
		TransferMlaaResultIfNecessary( nSurfacePrevFlipIdx ); 
	
		// do the post-processing on this frame, in the mean time render into previous frame
		return true;

	}
	return false; // there's no need to switch surfaces now
}



void CFlipHandler::Flip()
{
#if GCM_ALLOW_TIMESTAMPS
	OnFrameTimestampAvailableMST( 1.0f );
#endif

	extern ConVar mat_vsync;
	m_bVSync = mat_vsync.GetBool();

	g_ps3gcmGlobalState.CmdBufferFlush( CPs3gcmGlobalState::kFlushForcefully );
	g_spuGcm.GetDrawQueue()->Push1( SPUDRAWQUEUE_FRAMEEVENT_METHOD | SDQFE_END_FRAME );

	uint surfaceFlipIdx = g_ps3gcmGlobalState.m_display.surfaceFlipIdx, nSurfaceNextFlipIdx = g_ps3gcmGlobalState.m_display.NextSurfaceIndex( 1 ), nSurfaceAfterNextFlipIdx = g_ps3gcmGlobalState.m_display.NextSurfaceIndex( 2 ), nSurfacePrevFlipIdx = g_ps3gcmGlobalState.m_display.PrevSurfaceIndex( 1 );
	
/*	
	uint nScreenWidth = g_ps3gcmGlobalState.m_nRenderSize[0];
	uint nScreenY = 40;
	g_ps3gcmGlobalState.DrawDebugStripe( nScreenWidth * surfaceFlipIdx / 3, nScreenY, 0, nScreenWidth / 3, 4 );
	g_ps3gcmGlobalState.DrawDebugStripe( ( g_spuGcm.m_nFrame & 0xF ) * ( nScreenWidth / 16 ), 34, 0, ( nScreenWidth / 16 ) * ( 1 + m_nFlipSurfaceCount ), 1 );
*/	
	// let interrupt know we're ready to post-process the new frame, and we wanna flip the previous frame

	//g_ps3gcmGlobalState.CmdBufferFinish();
	uint32 * pThisJts = g_spuGcm.m_pEdgePostRsxLock + surfaceFlipIdx; // may be NULL + idx
	Assert( !g_spuGcm.m_pEdgePostRsxLock || *pThisJts == CELL_GCM_RETURN() );
	
	uint8 prevPostProcessed = m_surfaceEdgePost[nSurfacePrevFlipIdx];
	uint8 thisPostProcess = g_spuGcm.m_pMlaaBuffer ? ( uint8 ) ( m_nMlaaFlagsThisFrame & EDGE_POST_MLAA_FLAG_MASK ): 0 ;
	
	if( prevPostProcessed ) // did previous surface need post-processing?
	{
		TransferMlaaResultIfNecessary( nSurfacePrevFlipIdx );
		//if( g_spuGcm.m_bUseDeferredDrawQueue )
		{
			// now is the time to execute all the deferred commands, if there are any
			// NOTE: this will often do nothing , because current frame would've flushed previous frame deferred commands already
			//       right before starting writing its own
			g_spuGcm.ExecuteDeferredDrawQueue( 1 );
		}
		//g_ps3gcmGlobalState.DrawDebugStripe( nScreenWidth * surfaceFlipIdx / 3, 44, surfaceFlipIdx, nScreenWidth / 3, 2, -1 );
		
		// prepare flip of previous frame - Edge Post processed buffer
		// the previous frame was post-processed; we'll prepare flip on it. 
		QmsPrepareFlipSubmit( GCM_USERCMD_FLIPREADY, nSurfacePrevFlipIdx );
	}
	else
	{
		// if previous frame wasn't post-processed, don't flip it because we don't want to flip the same framebuffer twice (although we probably could)
		// so we don't have anything to flip here, but have a frame to post-process
		g_spuGcm.ExecuteDeferredDrawQueue( 1 );
	}

	m_surfaceEdgePost[surfaceFlipIdx] = thisPostProcess; // is post-process required for this surface ?
	if( thisPostProcess )
	{
		if( !( m_nMlaaFlagsThisFrame & EDGE_POST_MLAA_MODE_ENABLED ) )
		{
			m_bEdgePostResultAlreadyInLocalMemory = true; // don't attempt to transfer the results; we don't _really_ do edge post processing, so we consider the results are in memory already
		}
		else
		{
			// EDGE POST TODO: JTS - the previous EdgePost must release it. To avoid overwriting edge post buffer before it finished tranferring back to local memory
			// to release JTS from the future, we can use a separate ring buffer "JTS-RET" sequences and just call into it here.
			// or we can wait for a label and set it from SPU
			// as a simplification, we can just wait for edge post to finish synchronously on ppu
			// we can also use a mutex of sorts and insert JTS here only when edge post is not finished yet

			// we only can start transferring the image after the SPU is done streaming previous frame (if previous frame was post-processed)
			// so wait for SPU to release previous frame, if it was post-processed.

			// Also, if SPU didn't finish post-processing, then we need to synchronize (wait on RSX for SPU to be done)
			// but in many cases SPU will be done by now, so we don't need to spend 900+ns in RSX front-end on CALL+RET

			if( !g_edgePostWorkload.ShouldUseLabelForSynchronization() )
			{
				*pThisJts = CELL_GCM_JUMP( uintp( pThisJts ) + g_spuGcmShared.m_nIoOffsetDelta ); // this will be  JTS for SPU to overwrite when post-processing of this frame is done
			}

			CPs3gcmLocalMemoryBlockSystemGlobal & surfaceColor = g_ps3gcmGlobalState.m_display.surfaceColor[surfaceFlipIdx];
			GCM_FUNC( cellGcmSetTransferImage, CELL_GCM_TRANSFER_LOCAL_TO_MAIN,
				uintp( g_spuGcm.m_pMlaaBuffer ) + g_ps3gcmGlobalState.m_nIoOffsetDelta, g_ps3gcmGlobalState.m_nSurfaceRenderPitch, 0, 0,
				surfaceColor.Offset(), g_ps3gcmGlobalState.m_nSurfaceRenderPitch, 0, 0,			
				g_ps3gcmGlobalState.m_nRenderSize[0], g_ps3gcmGlobalState.m_nRenderSize[1], 
				4 );

			// This frame was rendered and transferred to main memory; we'll let interrupt thread know it's ready for Edge Post processing
			m_interruptFifo.Queue( GCM_USERCMD_POSTPROCESS, surfaceFlipIdx );
			m_bEdgePostResultAlreadyInLocalMemory = false;
		}
	}
	else
	{
		// we aren't post-processing this frame, so we need to just prepare flip and flip this framebuffer
		g_spuGcm.ExecuteDeferredDrawQueue( 0 );
		QmsPrepareFlipSubmit( GCM_USERCMD_FLIPREADY, surfaceFlipIdx );
		m_bEdgePostResultAlreadyInLocalMemory = true; // don't attempt to transfer the results; we don't do edge post - processing, so we consider the results are in memory already
	}
	
	g_spuGcm.FlipDeferredDrawQueue( );

	if( thisPostProcess && !prevPostProcessed )
	{
		// we absolutely MUST reset RSX state before the next frame. 
		// QmsPrepareFlipSubmit() does that by definition, but if we don't call it in this Flip (i.e. when !prevPostProcessed && thisPostProcess)
		// we must FORCE RSX state reset
		g_spuGcm.GetDrawQueue()->Push1( SPUDRAWQUEUE_RESETRSXSTATE_METHOD );
	}


#if GCM_ALLOW_TIMESTAMPS
	{
		// The current frame has just finished, insert a timestamp instruction right before flip
		GCM_FUNC( cellGcmSetTimeStamp, surfaceFlipIdx * 2 + GCM_REPORT_TIMESTAMP_FRAME_FIRST + 1 );
		g_ps3_timestampBeginIdx = nSurfaceNextFlipIdx * 2 + GCM_REPORT_TIMESTAMP_FRAME_FIRST;
	}
#endif
	m_interruptFifo.QueueRsxInterrupt();
	g_ps3gcmGlobalState.CmdBufferFlush( CPs3gcmGlobalState::kFlushEndFrame );
	//g_ps3gcmGlobalState.CmdBufferFinish();

	//
	// Make sure that the next framebuffer is free to render into. For that to be so,
	// the flip should happen from the next to the buffer after next. When that happens, 
	// the TV shows the buffer after next, and the next buffer is not visible to the user, 
	// so it's allowed to render into the next buffer.
	//
	FLIP_LOG( "ev Wait %d", nSurfaceAfterNextFlipIdx );
	m_evFlipReady[ nSurfaceAfterNextFlipIdx ].Wait();
	m_evFlipReady[ nSurfaceAfterNextFlipIdx ].Reset();
	FLIP_LOG( "Draw %d, ev Reset %d", nSurfaceNextFlipIdx, nSurfaceAfterNextFlipIdx );

#if GCM_ALLOW_TIMESTAMPS
	{
		// Since the previous flip completely finished, we can grab its timestamps now
		uint32 uiLastFrameTimestampIdx = ( nSurfaceAfterNextFlipIdx ) * 2 + GCM_REPORT_TIMESTAMP_FRAME_FIRST;
		uint64 uiStartTimestamp = cellGcmGetTimeStamp( uiLastFrameTimestampIdx );
		uint64 uiEndTimestamp = cellGcmGetTimeStamp( uiLastFrameTimestampIdx + 1 );
		uint64 uiRsxTimeInNanoSeconds = uiEndTimestamp - uiStartTimestamp;

		OnFrameTimestampAvailableRsx( uiRsxTimeInNanoSeconds / 1000000.0f );
	}
#endif
}


bool IsRsxReadyForNoninteractiveRefresh( )
{
	uint nSurfaceAfterNextFlipIdx = g_ps3gcmGlobalState.m_display.NextSurfaceIndex( 2 );
	return g_flipHandler.m_evFlipReady[ nSurfaceAfterNextFlipIdx ].Check();
	
	// if we are 3 vblanks past last flip already, another refresh would be welcome ; if we have no surfaces to flip in this case, we are most likely ready to flip right away
	// another thing to check is the interrupt FIFO: if it's not idle, let's just postpone being ready
	// return g_flipHandler.m_nVblankCounter > 3 && g_flipHandler.m_nFlipSurfaceCount == 0 && g_flipHandler.m_interruptFifo.IsIdle();
}



void CFlipHandler::TryFlipVblank()
{
	// artificially simulate an interrupt for cause, because there's suspicion it was dropped
	//
	// only attempt to generate artificial interrupts if our ready flip queue is empty, otherwise there's no need
	// to tap the narrow 15.6Mb/s bus

	uint nMarker = *g_flipHandler.m_pLastInterruptGet;
	m_nVblankCounter ++;
	#if ENABLE_FLIP_LOG
	static int m_nLastFlipLogIdx = 0;
	if( m_nLastFlipLogIdx != g_flipLogIdx )
	{
		V_snprintf( g_flipLog[m_nLastFlipLogIdx], sizeof( g_flipLog[m_nLastFlipLogIdx] ), "%X.Vblanks ..%d", nMarker, m_nVblankCounter );
	}
	else
	{
		m_nLastFlipLogIdx = cellAtomicIncr32( &g_flipLogIdx ) & ( ARRAYSIZE( g_flipLog ) - 1 );
		V_snprintf( g_flipLog[m_nLastFlipLogIdx], sizeof( g_flipLog[m_nLastFlipLogIdx] ), "%X.Vblank %d", nMarker, m_nVblankCounter );
	}
	#endif
	TryPumpEvents( nMarker, 1 );
}



bool CFlipHandler::TryFlipSurface( uint isVblank )
{
	if( !m_nFlipSurfaceCount )
	{
		return false;
	}
	if( m_bVSync )
	{
		if( m_nVblankCounter < m_nPresentFrequency )
		{
			//FLIP_LOG( "no flip: %d vblanks", m_nVblankCounter, m_nPresentFrequency );
			return false;
		}

		if( !isVblank )
		{
			double flVSyncInterval = m_flVBlankTimestamp - m_flVBlankTimestamp0, flMissThreshold = r_ps3_vblank_miss_threshold.GetFloat() * flVSyncInterval;
			double flMiss = Plat_FloatTime() - m_flVBlankTimestamp;
			if ( flMiss > flMissThreshold )
			{
				FLIP_LOG("no flip: %.2fms miss", flMiss * 1000 );
				return false; // wait for another vsync, missed by too much
			}
		}
	}

	// flip the surface immediately 
	uint nSystemFlipId = m_nSystemFlipId[ m_nFlipSurfaceIdx ];
	cellGcmSetFlipImmediate( nSystemFlipId );

#ifdef GCM_ALLOW_TIMESTAMPS
	// Collect time since previous flip
	double flFlipImmediateTimestamp = Plat_FloatTime();
	OnFrameTimestampAvailableFlip( ( flFlipImmediateTimestamp - m_flFlipImmediateTimestamp ) * 1000.0f );
	m_flFlipImmediateTimestamp = flFlipImmediateTimestamp;
#endif

	FLIP_LOG( isVblank ? "vFlip%u, ev Set %u" : "_Flip%u, ev Set %u", nSystemFlipId, m_nFlipSurfaceIdx );
	// Release PPU QMS thread waiting for this flip
	m_evFlipReady[m_nFlipSurfaceIdx].Set();

	m_nFlipSurfaceIdx = ( m_nFlipSurfaceIdx + 1 ) % CPs3gcmDisplay::SURFACE_COUNT;
	m_nFlipSurfaceCount--;
	m_nVblankCounter = 0;
	return true;
}


void CFlipHandler::TryPumpEvents( uint nMarker, uint isVblank )
{
	if ( m_mutexOfInterruptThread.TryLock() )
	{
		PumpEventsUnsafe( nMarker );
		TryFlipSurface( isVblank ); // this will often be duplicate call		
		g_flipHandler.m_mutexOfInterruptThread.Unlock();
	}
}

void CFlipHandler::PumpEventsUnsafe( uint nMarker )
{
	while( m_interruptFifo.HasEvents( nMarker ) )
	{
		if( !OnRsxInterrupt( m_interruptFifo.DequeueEvent() ) )
			break;
	}
}

bool RsxInterruptFifo::IsValidMarker( uint nMarker )
{
	return ( nMarker - m_nGet ) <= MAX_EVENT_COUNT;
}


bool CFlipHandler::OnRsxInterrupt( const RsxInterruptFifo::Event_t event )
{
	switch( event.m_nCause )
	{
	case GCM_USERCMD_POSTPROCESS:
		{
			// start edge post processing phase here; we can't do the flip yet because we didn't post-process the buffer yet

			// Simulating MLAA job running and adding the cause to the end of the array some time in the nearest (4-5ms) future
			void * pColorSurface = g_ps3gcmGlobalState.m_display.surfaceColor[event.m_nSurfaceFlipIdx].DataInLocalMemory();
			if( true )
			{
				// g_spuGcm.SyncMlaa();
				g_edgePostWorkload.Kick( pColorSurface, event.m_nSurfaceFlipIdx );
			}
			else
			{
				FLIP_LOG( "mlaa sync %d", event.m_nSurfaceFlipIdx );
				g_spuGcm.SyncMlaa( pColorSurface );
				g_spuGcm.m_pEdgePostRsxLock[event.m_nSurfaceFlipIdx] = CELL_GCM_RETURN(); // this will be poked by the SPU job
			}
		}
		break;

	case GCM_USERCMD_FLIPREADY:
		FlipAssert( ( m_nFlipSurfaceIdx + m_nFlipSurfaceCount ) % CPs3gcmDisplay::SURFACE_COUNT == event.m_nSurfaceFlipIdx );
		FLIP_LOG( "flip ready %d:sys%d", event.m_nSurfaceFlipIdx, m_nSystemFlipId[event.m_nSurfaceFlipIdx] );
		m_nFlipSurfaceCount++;
		break;
	}

	return true;
}

void CFlipHandler::INTERRUPT_VBlankHandler( const uint32 head )
{
	double flVBlankTimestampSave = g_flipHandler.m_flVBlankTimestamp;
	g_flipHandler.m_flVBlankTimestamp = Plat_FloatTime();
	g_flipHandler.m_flVBlankTimestamp0 = flVBlankTimestampSave;
	g_flipHandler.TryFlipVblank( );
}

void CFlipHandler::INTERRUPT_UserHandler( const uint32 nMarker )
{
	if( g_flipHandler.m_interruptFifo.IsValidMarker( nMarker ) )
	{
		//FLIP_LOG( "%X.UserInterrupt", nMarker );
		g_flipHandler.TryPumpEvents( nMarker, 0 );
	}
	else
	{
		// invalid  marker: this marker has already happened; skip it
		//FLIP_LOG( "%X.ERROR.UserInterrupt", nMarker );
		DebuggerBreak();
	}
}

void Ps3gcmFlip_SetFlipPresentFrequency( int nNumVBlanks )
{
	if ( g_flipHandler.m_nPresentFrequency != nNumVBlanks )
	{
		nNumVBlanks = MAX( 1, nNumVBlanks );
		nNumVBlanks = MIN( 12, nNumVBlanks );
		if ( g_flipHandler.m_nPresentFrequency != nNumVBlanks )
		{
			g_flipHandler.m_nPresentFrequency = nNumVBlanks;
		}
	}
}



/*
void CFlipHandler::OnState( int nState, int nValue )
{
	m_nDebugStates[nState] = nValue;
	if( m_nDebugStates[RENDERING_SURFACE] == m_nDebugStates[DISPLAYING_SURFACE] )
		DebuggerBreak();
}*/
