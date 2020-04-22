//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
// Gcm renderer state and util functions
//
//==================================================================================================

#ifndef SPU
#define CELL_GCM_MEMCPY memcpy						// PPU SNC has no such intrinsic
#endif

#include "sys/memory.h"
#include "sysutil/sysutil_sysparam.h"
#include "cell/sysmodule.h"

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier1/utlbuffer.h"
#include "cell/gcm.h"
#include "gcmconfig.h"
#include "ps3gcmmemory.h"
#include "gcmstate.h"
#include "gcmlabels.h"
#include "gcmdrawstate.h"

#include "ps3/ps3_helpers.h"
#include <cell/gem.h> // PS3 move controller lib
#include "inputsystem/iinputsystem.h"
#include "memdbgon.h"

//--------------------------------------------------------------------------------------------------
// Golobals, GCM context, flip control init proto
//--------------------------------------------------------------------------------------------------

ALIGN128 CPs3gcmGlobalState			g_ps3gcmGlobalState ALIGN128_POST;

ALIGN16 CellGcmContextData  gGcmContext ALIGN16_POST;
CellGcmContextData*			gpGcmContext;

CellGcmContextData			gCallContext;
CellGcmContextData*			gpCallContext = &gCallContext;

static void Gcm_InitFlipControl(void);

static volatile uint32_t *s_label_call_cmd_ring_seg;	// pointer to the call cmd label
volatile uint32_t *g_label_fppatch_ring_seg;			// Fp pacth label

//--------------------------------------------------------------------------------------------------
// Empty Ps
//--------------------------------------------------------------------------------------------------

uint8 g_dataShaderPsEmpty[] = {
	  0x00, 0x00, 0x1B, 0x5C, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0xB0, 0x00, 0x00, 0x00, 0x01
	, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x80
	, 0x00, 0x00, 0x04, 0x18, 0x00, 0x00, 0x0A, 0xC5, 0x00, 0x00, 0x10, 0x05, 0xFF, 0xFF, 0xFF, 0xFF
	, 0x00, 0x00, 0x00, 0x56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50
	, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00
	, 0x43, 0x4F, 0x4C, 0x4F, 0x52, 0x00, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF
	, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	, 0x1E, 0x7E, 0x7E, 0x00, 0xC8, 0x00, 0x1C, 0x9D, 0xC8, 0x00, 0x00, 0x01, 0xC8, 0x00, 0x00, 0x01
	, 0x1E, 0x01, 0x01, 0x00, 0x28, 0x02, 0x1C, 0x9C, 0xC8, 0x00, 0x00, 0x01, 0xC8, 0x00, 0x00, 0x01
	, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

//--------------------------------------------------------------------------------------------------
// Global GCM state class
//
// Global state, command buffers, RSX draw display buffers etc etc 
//--------------------------------------------------------------------------------------------------

int32 CPs3gcmGlobalState::Init()
{
	MEM_ALLOC_CREDIT_( "GCM INIT" );

	Msg(">>>> Sizeof(CGcmDrawStateDma) %d \n", DRAWSTATE_SIZEOFDMA);
	Msg(">>>> Sizeof(CGcmDrawState) %d \n", sizeof(CGcmDrawState));

	// Create Raw SPU task for renderer acceleration

 	gSpuMgr.Init(1);
 	gSpuMgr.CreateSpuTask("rawspu_gcmdraw_spu.self", &m_spuHandle);

	// Default to 60Hz

	m_flipMode = 30;

	// Video : display res, video buffer, gamma, RGB colour range

	if( int nError= InitVideo() )
		return nError;

	// Alloc IO memory, Set address, size of main memory pool for RSX

	CreateIoBuffers();

	// Init GCM : Map IO memory, Create command buffers
	if( int nError = InitGcm() )
		return nError;

	// Retrieve RSX local memory config
	CellGcmConfig rsxConfig;
	cellGcmGetConfiguration( &rsxConfig );
	m_pLocalBaseAddress = rsxConfig.localAddress;
	m_nLocalSize = rsxConfig.localSize;
	cellGcmAddressToOffset( m_pLocalBaseAddress, &m_nLocalBaseOffset );
	Assert( m_nLocalBaseOffset == 0 );

	// Init local memory mgr
	Ps3gcmLocalMemoryAllocator_Init();

	// Create display buffers etc..
	CreateRsxBuffers();

	// Create Empty PS

	m_pShaderPsEmpty = reinterpret_cast< CgBinaryProgram * >( g_dataShaderPsEmpty );
	m_pShaderPsEmptyBuffer.Alloc( kAllocPs3GcmShader, m_pShaderPsEmpty->ucodeSize );
	V_memcpy( m_pShaderPsEmptyBuffer.DataInLocalMemory(), ( (char*)m_pShaderPsEmpty ) + m_pShaderPsEmpty->ucode, m_pShaderPsEmpty->ucodeSize );


	CgBinaryFragmentProgram *pCgFragmentProgram = (  CgBinaryFragmentProgram * )( uintp( m_pShaderPsEmpty ) + m_pShaderPsEmpty->program );
	m_nPsEmptyAttributeInputMask = pCgFragmentProgram->attributeInputMask;

	uint registerCount = pCgFragmentProgram->registerCount;
	// NOTE: actual register count can be modified by specifying an artificial e.g. PS3REGCOUNT48 static combo to force it to 48
	Assert( registerCount <= 48 );
	if (registerCount < 2)
	{
		// register count must be [2, 48]
		registerCount = 2;
	}

	uint8_t controlTxp = CELL_GCM_FALSE;
	uint32 shCtrl0 = ( CELL_GCM_COMMAND_CAST( controlTxp ) << CELL_GCM_SHIFT_SET_SHADER_CONTROL_CONTROL_TXP ) 
		& CELL_GCM_MASK_SET_SHADER_CONTROL_CONTROL_TXP;
	shCtrl0 |= ( 1<<10 ) | ( registerCount << 24 );
	shCtrl0 |= pCgFragmentProgram->depthReplace ? 0xE : 0x0;
	shCtrl0 |= pCgFragmentProgram->outputFromH0 ? 0x00 : 0x40;
	shCtrl0 |= pCgFragmentProgram->pixelKill ? 0x80 : 0x00;
	m_nPsEmptyShaderControl0 = shCtrl0;

	// Init glip control
	m_fastFlip = 0;
	Gcm_InitFlipControl();

    // Address of draw states
    m_eaDrawStates = uintp(gGcmDrawState);

	// Give SPU program this class
	gSpuMgr.WriteMailbox(&m_spuHandle, uintp(this));


	return CELL_OK;
}

void CPs3gcmGlobalState::CreateIoBuffers()
{
	m_nIoSize = GCM_IOSIZE;
	if ((m_nIoSize & 0xFFFFF) != 0)			// MB aligned
	{
		Error("No MB alignment %x\n\n", m_nIoSize);
	}

	// Try to allocate main memory that will be mapped to IO address space
	// Actually mapped in in GcmInit, once gcm is going

	sys_addr_t pIoAddress = NULL;
	int nError = sys_memory_allocate( m_nIoSize, SYS_MEMORY_PAGE_SIZE_1M, &pIoAddress );
	if ( CELL_OK != nError )
	{
		Error( "sys_memory_allocate failed to allocate %d bytes (err: %d)\n", m_nIoSize, nError );
	}
	m_pIoAddress = (void *)pIoAddress;

	Msg( "======== GCM IO memory allocated  @0x%p   size = %d MB ========\n", m_pIoAddress, m_nIoSize / 1024 / 1024 );

	// Call command buffer

	m_pCallCmdBuffer = (void*)(uintp(pIoAddress) + GCM_DEFCMDBUFFSIZE);

	// RSX main memory pool buffer
	m_nRsxMainMemoryPoolBufferSize = GCM_MAINPOOLSIZE; 
	m_pRsxMainMemoryPoolBuffer = (void*)(uintp(pIoAddress) + GCM_DEFCMDBUFFSIZE + GCM_CALLCMDBUFFSIZE);

	// Patch buffers

	m_pPatchBuff = (uint8*)m_pRsxMainMemoryPoolBuffer + GCM_MAINPOOLSIZE;
}

int  CPs3gcmGlobalState::InitGcm()
{
	int32 result = cellGcmInit( GCM_DEFCMDBUFFSIZE, m_nIoSize, m_pIoAddress );
	if ( result < CELL_OK )
		return result;

	gGcmContext = *gCellGcmCurrentContext;
	gpGcmContext = &gGcmContext;
	gpGcmContext->callback = CmdBufferFull;

	// Set the flip mode etc...

	// Get the offset delta
	cellGcmAddressToOffset( m_pIoAddress, &m_nIoOffsetDelta );
	m_nIoOffsetDelta -= uintp( m_pIoAddress );

	// Setup call cmd buffer

	m_nCallCmdBufferoffset =  uintp(m_pCallCmdBuffer) + m_nIoOffsetDelta;

	m_nCallWritePos = 0;
	m_nCallReadSegment = 0;
	s_label_call_cmd_ring_seg = cellGcmGetLabelAddress(GCM_LABEL_CALL_CMD_RING_SEG);
	*s_label_call_cmd_ring_seg = 0;

	// Setup Patch Buffers

	m_nPatchIdx = 0;
	m_nPatchReadSeg = 0;
	g_label_fppatch_ring_seg = cellGcmGetLabelAddress(GCM_LABEL_FPPATCH_RING_SEG);
	*g_label_fppatch_ring_seg = 0;

	return CELL_OK;

}

int  CPs3gcmGlobalState::InitVideo()
{
	//////////////////////////////////////////////////////////////////////////
	//
	// Initialize m_display
	//
	CellVideoOutState videoOutState;
	int result = cellVideoOutGetState( CELL_VIDEO_OUT_PRIMARY, 0, &videoOutState);
	if ( result < CELL_OK )
		return result;

	CellVideoOutResolution resolution;
	result = cellVideoOutGetResolution( videoOutState.displayMode.resolutionId, &resolution );
	if ( result < CELL_OK )
		return result;

	// Always output scanout in system m_display resolution
	m_nRenderSize[0] = resolution.width;
	m_nRenderSize[1] = resolution.height;

	// Handle special case: 1080p will be upsampled from 720p
	if ( resolution.height >= 720 && CommandLine()->FindParm( "-480p" ) )
	{
		m_nRenderSize[0] = 640;
		m_nRenderSize[1] = 480;
		videoOutState.displayMode.resolutionId = CELL_VIDEO_OUT_RESOLUTION_480;
	}
	else if ( resolution.height >= 1080 && !CommandLine()->FindParm( "-1080p" ) )
	{
		m_nRenderSize[0] = 1280;
		m_nRenderSize[1] = 720;
		videoOutState.displayMode.resolutionId = CELL_VIDEO_OUT_RESOLUTION_720;
	}

	//////////////////////////////////////////////////////////////////////////
	//
	// Set video output
	//
	CellVideoOutConfiguration videocfg;
	memset( &videocfg, 0, sizeof(videocfg) );
	videocfg.resolutionId = videoOutState.displayMode.resolutionId;
	videocfg.format = CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_X8R8G8B8;
	videocfg.pitch = cellGcmGetTiledPitchSize( m_nRenderSize[0] * 4 );
	m_nSurfaceRenderPitch = videocfg.pitch;

	// Configure video output
	result = cellVideoOutConfigure( CELL_VIDEO_OUT_PRIMARY, &videocfg, NULL, 0 );
	if ( result < CELL_OK )
		return result;

	// Get the new video output
	result = cellVideoOutGetState( CELL_VIDEO_OUT_PRIMARY, 0, &videoOutState );
	if ( result < CELL_OK )
		return result;
	m_flRenderAspect = ( videoOutState.displayMode.aspect == CELL_VIDEO_OUT_ASPECT_4_3 ) ? ( 4.0f/3.0f ) : ( 16.0f / 9.0f );

	// Set the gamma to deal with TV's having a darker gamma than computer monitors
	result = cellSysmoduleLoadModule( CELL_SYSMODULE_AVCONF_EXT );
	if ( result == CELL_OK )
	{
		cellVideoOutSetGamma( CELL_VIDEO_OUT_PRIMARY, 2.2f / 2.5f );
	}
	else
	{
		Warning( "***** ERROR calling cellSysmoduleLoadModule( CELL_SYSMODULE_AVCONF_EXT )! Gamma not set!\n" );
		return result;
	}

	// Output video color settings
	CellVideoOutDeviceInfo info;
	cellVideoOutGetDeviceInfo( CELL_VIDEO_OUT_PRIMARY, 0, &info );
	if ( info.rgbOutputRange == CELL_VIDEO_OUT_RGB_OUTPUT_RANGE_LIMITED )
	{
		DevMsg( "***** Video Out - Limited Range (16-235) - Gamma=%d *****\n", info.colorInfo.gamma );
	}
	else
	{
		DevMsg( "***** Video Out - Full Range (0-255) - Gamma=%d *****\n", info.colorInfo.gamma );
	}

	return CELL_OK;
}

void CPs3gcmGlobalState::CreateRsxBuffers()
{
	//////////////////////////////////////////////////////////////////////////
	//
	// Create automatic display objects
	//
	if( m_nSurfaceRenderPitch != cellGcmGetTiledPitchSize( m_nRenderSize[0] * 4 ) )
	{
		Error("Pre-computed surface render pitch %u != %u = cellGcmGetTiledPitchSize( %u * 4 ) ", m_nSurfaceRenderPitch, cellGcmGetTiledPitchSize( m_nRenderSize[0] * 4 ), m_nRenderSize[0] );
	}

	m_display.surfaceFlipIdx = 0;

	// Color buffers
	for ( int k = 0; k < ARRAYSIZE( m_display.surfaceColor ); ++ k )
	{
		uint32 nRenderSize32bpp = GetRenderSurfaceBytes(); // 32-line vertical alignment required in local memory
		m_display.surfaceColor[k].Alloc( kAllocPs3gcmColorBufferFB, nRenderSize32bpp );
		cellGcmSetDisplayBuffer( k, m_display.surfaceColor[k].Offset(), m_nSurfaceRenderPitch, m_nRenderSize[0], m_nRenderSize[1] );
	}

	// Depth buffer
	{
		uint32 zcullSize[2] = { AlignValue( m_nRenderSize[0], 64 ), AlignValue( m_nRenderSize[1], 64 ) };
		uint32 nDepthPitch = cellGcmGetTiledPitchSize( zcullSize[0] * 4 );
		uint32 uDepthBufferSize32bpp = nDepthPitch * zcullSize[1];
		uDepthBufferSize32bpp = AlignValue( uDepthBufferSize32bpp, PS3GCMALLOCATIONALIGN( kAllocPs3gcmDepthBuffer ) );
		m_display.surfaceDepth.Alloc( kAllocPs3gcmDepthBuffer, uDepthBufferSize32bpp );

		uint32 uiZcullIndex = m_display.surfaceDepth.ZcullMemoryIndex();
		cellGcmBindZcull( uiZcullIndex,
			m_display.surfaceDepth.Offset(),
			zcullSize[0], zcullSize[1],
			m_display.surfaceDepth.ZcullMemoryStart(),
			CELL_GCM_ZCULL_Z24S8,
			CELL_GCM_SURFACE_CENTER_1,
			CELL_GCM_ZCULL_LESS,
			CELL_GCM_ZCULL_LONES,
			CELL_GCM_SCULL_SFUNC_ALWAYS,
			0, 0	// sRef, sMask
			);

		uint32 uiTileIndex = m_display.surfaceDepth.TiledMemoryIndex();
		cellGcmSetTileInfo( uiTileIndex, CELL_GCM_LOCATION_LOCAL, m_display.surfaceDepth.Offset(),
			uDepthBufferSize32bpp, m_nSurfaceRenderPitch, CELL_GCM_COMPMODE_Z32_SEPSTENCIL_REGULAR,
			m_display.surfaceDepth.TiledMemoryTagAreaBase(), // The area base + size/0x10000 will be allocated as the tag area.
			3 );	// Default depth buffer on bank 3
		cellGcmBindTile( uiTileIndex );
	}
}

void  CPs3gcmGlobalState::Shutdown()
{
	
	gpGcmDrawState->EndFrame();
	gpGcmDrawState->CmdBufferFinish();

	cellGcmSetFlipHandler(NULL);
	cellGcmSetVBlankHandler(NULL);
	
	cellSysmoduleUnloadModule( CELL_SYSMODULE_AVCONF_EXT );
}

//--------------------------------------------------------------------------------------------------
// DawPrimUp code...
//--------------------------------------------------------------------------------------------------


uint32 CPs3gcmGlobalState::DrawPrimitiveUP(D3DPRIMITIVETYPE nPrimitiveType,UINT nPrimitiveCount,
										CONST void *pVertexStreamZeroData, UINT nVertexStreamZeroStride )
{
	// First Determine size required for this call

	uint32 size = 0;

	uint32 nIndexCount = GetGcmCount( nPrimitiveType, nPrimitiveCount );
	uint32 nDataWords = ( nVertexStreamZeroStride * nIndexCount + 3 ) / sizeof( uint32 );

	size = cellGcmSetWriteTextureLabelMeasureSize(size, GCM_LABEL_CALL_CMD_RING_SEG, 0 );
	size = cellGcmSetInvalidateVertexCacheMeasureSize(size);
	size = cellGcmSetDrawInlineArrayMeasureSize(size, GetGcmMode( nPrimitiveType ), nDataWords, pVertexStreamZeroData );
	size = cellGcmSetReturnCommandMeasureSize(size);

	size *=4;

	// Check there is no space in the current segment

	uint32 endPos, nextSeg, readSeg, writeSeg;

	endPos = m_nCallWritePos + size;
	writeSeg = m_nCallWritePos/GCM_CALLCMDSEGSIZE;

	if ((endPos/GCM_CALLCMDSEGSIZE) != writeSeg)
	{
		// Move to the next segment

		uint32 nextSeg = (writeSeg + 1) % (GCM_CALLCMDBUFFSIZE / GCM_CALLCMDSEGSIZE);

		// Wait for RSX to not be in this segment

		readSeg = m_nCallReadSegment;

		if(nextSeg == readSeg) readSeg = *s_label_call_cmd_ring_seg;

		gpGcmDrawState->CmdBufferFlush();

		uint32 spins = 0;
		while(nextSeg == readSeg)
		{
			spins++;	
			sys_timer_usleep(60);
			readSeg = *s_label_call_cmd_ring_seg;
		}

		//if (spins > 1) Msg("Spins %d\n", spins);

		// Move to next segmend abnd record new readSeg

		m_nCallWritePos = (nextSeg * GCM_CALLCMDSEGSIZE);
		writeSeg = nextSeg;

		m_nCallReadSegment = readSeg;

//		Msg("new Segment 0x%x\n", m_nCallWritePos);

	}

	uint32 ret = m_nCallWritePos + uintp(m_pCallCmdBuffer);

	// Write Data
	// Setup a context to do so 

	CellGcmContextData context;

	context.begin   = (uint32*)m_pCallCmdBuffer;
	context.current = (uint32*)((uint8*)m_pCallCmdBuffer + m_nCallWritePos);
	context.end		= (uint32*)((uint8*)m_pCallCmdBuffer + GCM_CALLCMDBUFFSIZE);
	context.callback = 0;

	cellGcmSetWriteTextureLabelUnsafeInline(&context, GCM_LABEL_CALL_CMD_RING_SEG, writeSeg );
	cellGcmSetInvalidateVertexCacheUnsafeInline(&context);
	cellGcmSetDrawInlineArrayUnsafeInline(&context, GetGcmMode( nPrimitiveType ), nDataWords, pVertexStreamZeroData );
	cellGcmSetReturnCommandUnsafeInline(&context);

	// Update pointers

	m_nCallWritePos += size;

	return ret;
}



//--------------------------------------------------------------------------------------------------
// Command Buffer callback
//--------------------------------------------------------------------------------------------------

#define SEGSIZE 0x40000
#define SEGMASK 0x3FFFF

int32 CPs3gcmGlobalState::CmdBufferFull(struct CellGcmContextData * pGcmContext, uint32_t size)
{
	// move to next SEGSIZE, and then wrap to start
	// Determine where the next buffer will be

	uint32 nIoAddress = (uint32)g_ps3gcmGlobalState.m_pIoAddress;

	uint32 nextBufferStart = ((uint32)pGcmContext->begin + SEGSIZE) & (~SEGMASK);
	nextBufferStart -= nIoAddress;

	nextBufferStart &= (GCM_DEFCMDBUFFSIZE-1);
	nextBufferStart = nextBufferStart ? (nextBufferStart + nIoAddress) : (SEGSIZE + nIoAddress);

	// Flush RSX to this point
	cellGcmFlushUnsafeInline(pGcmContext);

	// put jump command to beginning of next buffer
	uint32	nextBufferOffset	   = nextBufferStart - nIoAddress;
	uint32  nextBufferEndOffset    = ((nextBufferOffset + SEGSIZE) & (~SEGMASK)) - 4;

	cellGcmSetJumpCommandUnsafeInline(pGcmContext, nextBufferStart - nIoAddress );

	// get put/get/ref register address
	volatile CellGcmControl* control = cellGcmGetControlRegister();

	int count = 500000;

	// wait for RSX to finish all commands in next buffer (it's a ring buffer)
	volatile uint32_t get = (volatile uint32_t)control->get;

	while( (get < 0x1000 ) ||  ( (get >= nextBufferOffset) && (get < nextBufferEndOffset) ) )
	{
		sys_timer_usleep( 30 );
		get = (volatile uint32_t)control->get;

// 		count--;
// 		if (count < 1)
// 		{
// 			Msg("\n*****>>>> CmdBufferFull : get 0x%x : nextBufferOffset 0x%x : nextBufferEndOffset 0x%x\n", get, nextBufferOffset, nextBufferEndOffset );
// 			count = 1;
// 		}

	}


	// Set Command buffer context struct

	pGcmContext->begin = (uint32*)nextBufferStart;
	pGcmContext->end   = (uint32*)(nextBufferEndOffset + nIoAddress);
	pGcmContext->current = (uint32*)nextBufferStart;

	return CELL_OK;
}

//--------------------------------------------------------------------------------------------------
// Flip Control
//
// Summary : 
//
// Label used to cap the framerate. ie label to ensure flips no faster than 1 (60hz) or 2 (30Hz) vblanks.
// PPU blocks if previous flip not complete, so can't run too far ahead
// vblanks and flips noted by callbacks
//--------------------------------------------------------------------------------------------------

enum {

	LABEL_FLIP_CONTROL_READY=1, // when label-before-flip is released
	LABEL_FLIP_CONTROL_WAIT,	// when label-before-flip is not released
	/*
	  label_flip_control:
	  LABEL_FLIP_CONTROL_WAIT
	  => (when releasing flip by ppu) => LABEL_FLIP_CONTROL_READY,
	  => (when flip is finished by rsx) =>  LABEL_FLIP_CONTROL_WAIT,
	*/


	FLIP_STATE_V1=1,
	FLIP_STATE_FLIP_RELEASED,
	FLIP_STATE_FLIPPED,
	/*
	  flip_status sequence (30fps or slower):
	  FLIP_STATE_FLIPPED
	  (at vblank callback) => FLIP_STATE_V1
	  (at vblank callback) =<release flip>=> FLIP_STATE_FLIP_RELEASED
	  (at flip callback) => FLIP_STATE_FLIPPED
	*/

	/*
	  flip_status sequence (60fps or slower):
	  FLIP_STATE_FLIPPED
	  (at vblank callback) =<release flip>=> FLIP_STATE_FLIP_RELEASED
	  (at flip callback) => FLIP_STATE_FLIPPED
	*/
};

static volatile uint32_t *s_label_flip_control;  // pointer to the flip control label
static int s_flip_status=FLIP_STATE_FLIPPED;		// status variable to control flip

//--------------------------------------------------------------------------------------------------

static bool Gcm_ReleaseFlip(void)
{
	if (*s_label_flip_control==LABEL_FLIP_CONTROL_READY) {
		/* just in case rsx is running very slow somehow */
		/* and flip_control label is not updated even after the real flip */
		return false;
	}

	*s_label_flip_control=LABEL_FLIP_CONTROL_READY;
	return true;
}

void updateCursorPosition(const int pixelX, const int pixelY)
{
	cellGcmSetCursorPosition(pixelX, pixelY);
	int32_t result = cellGcmUpdateCursor();
	if( result == CELL_GCM_ERROR_FAILURE)
	{
		// [dkorus] this case happens until we initialize the cursor

		//Msg(" hardware cursor error: cellGcmInitCursor() has not been called\n");
	}
	else if( result == CELL_GCM_ERROR_INVALID_VALUE )
	{
		Msg(" hardware cursor error: cursor bitmap is not correctly set\n");
	}
}

void enableCursor()
{
	if (cellGcmSetCursorEnable() != CELL_OK )
	{
		Msg( "Hardware Cursor Error: trouble with enable\n" );
	}

	if ( cellGcmUpdateCursor() != CELL_OK )
	{	
		Msg( "Hardware Cursor Error: trouble with update\n" );
	}
}

static void Gcm_VblankCallbackFunction(const uint32_t head)
{
	// unused arg
	(void)head; 
	
	int pixelX, pixelY;
	if ( g_pInputSystem )
	{
		bool cursorEnabled = g_pInputSystem->GetPS3CursorPos( pixelX, pixelY );

		if( cursorEnabled )
		{
			updateCursorPosition(pixelX,pixelY);
		}
	}


	switch (s_flip_status){
	  case FLIP_STATE_FLIPPED:
		  if (g_ps3gcmGlobalState.m_flipMode == 30){
			  s_flip_status=FLIP_STATE_V1;
		  } else if (g_ps3gcmGlobalState.m_flipMode == 60){
			  if (Gcm_ReleaseFlip()){
				  s_flip_status=FLIP_STATE_FLIP_RELEASED;
			  }
		  }
		  break;
	  case FLIP_STATE_V1:
		  if (Gcm_ReleaseFlip()){
			  s_flip_status=FLIP_STATE_FLIP_RELEASED;
		  }
		  break;
	  case FLIP_STATE_FLIP_RELEASED:
		  break;
	  default:
		  assert(0);
	}
}

static void Gcm_FlipCallbackFunction(const uint32_t head)
{
	(void)head;
	switch (s_flip_status){
	  case FLIP_STATE_FLIP_RELEASED:
		  s_flip_status=FLIP_STATE_FLIPPED;
		  break;
	  default:
		  break;
	}
}


// initialize flip control state machine
static void Gcm_InitFlipControl(void)
{
	cellGcmSetFlipMode( CELL_GCM_DISPLAY_HSYNC );

	g_ps3gcmGlobalState.m_frameNo = 0;
	g_ps3gcmGlobalState.m_finishIdx = 0;
	
	s_label_flip_control=cellGcmGetLabelAddress(GCM_LABEL_FLIP_CONTROL);
	*s_label_flip_control=LABEL_FLIP_CONTROL_WAIT;

	cellGcmSetFlipHandler(Gcm_FlipCallbackFunction);
	cellGcmSetVBlankHandler(Gcm_VblankCallbackFunction);

}

//--------------------------------------------------------------------------------------------------
// Beginscene, endscene and flip
//--------------------------------------------------------------------------------------------------

uint32 gCmdBufferHighWater = 0;
uint32 gCmdBufferStart = 0;

void  CPs3gcmGlobalState::BeginScene()
{
	gCmdBufferStart = (uint32)gpGcmContext->current;

	gpGcmDrawState->BeginScene();
}

void  CPs3gcmGlobalState::EndScene()
{	
	if ( (uint32)gpGcmContext->current > gCmdBufferStart )
	{
		uint32 bytes = (uint32)gpGcmContext->current - gCmdBufferStart;
		if (bytes > gCmdBufferHighWater ) gCmdBufferHighWater = bytes;
	}

	gpGcmDrawState->EndScene();
}

float g_fliptime = 0;

void CPs3gcmGlobalState::SetFastFlip(bool onoff)
{
	m_fastFlip = onoff;
	
	g_fliptime = Plat_FloatTime();
}

extern void OnFrameTimestampAvailableRsx( float ms );

void  CPs3gcmGlobalState::Flip()
{
	cellSysutilCheckCallback();

	if(m_fastFlip)
	{
		Gcm_ReleaseFlip();

		float time = Plat_FloatTime();

		if ( (time - g_fliptime) > 0.05) goto fullflip;

		// Just end the frame, no point in flipping here...

		gpGcmDrawState->EndFrame();
		GCM_FUNC( cellGcmFlush );

		goto newframe;
	}

fullflip:

	int idx, startIdx, endIdx;

	//--------------------------------------------------------------------------------------------------
	// Ensure any buffered state, copies etc... goes to GPU
	//--------------------------------------------------------------------------------------------------

	gpGcmDrawState->EndFrame();

	//--------------------------------------------------------------------------------------------------
	// Wait for previous frame Flip
	//--------------------------------------------------------------------------------------------------

	while (cellGcmGetFlipStatus()!=0){

		g_pGcmSharedData->CheckForAudioRequest();
		g_pGcmSharedData->CheckForServerRequest();

		sys_timer_usleep(300);
	}

	// Insert end of gpu timestamp

	idx = m_frameNo&1;
	endIdx = GCM_REPORT_TIMESTAMP_FRAME_FIRST + (idx*2) + 1;
	GCM_FUNC( cellGcmSetTimeStamp, endIdx );

	//--------------------------------------------------------------------------------------------------
	// If requested, lets defrag VRAM
	//--------------------------------------------------------------------------------------------------

	if (g_pGcmSharedData->m_bDeFrag)
	{
		g_pGcmSharedData->m_bDeFrag = 0;
		extern void Ps3gcmLocalMemoryAllocator_CompactWithReason( char const *szReason );
		Ps3gcmLocalMemoryAllocator_CompactWithReason( "End of Round" );

	}

	//--------------------------------------------------------------------------------------------------
	// Get Timestamps
	//--------------------------------------------------------------------------------------------------

	if (m_frameNo)
	{
		idx = ((m_frameNo-1) & 1);
		startIdx = GCM_REPORT_TIMESTAMP_FRAME_FIRST + (idx*2);
		endIdx = startIdx+1;

		uint64 uiStartTimestamp = cellGcmGetTimeStamp( startIdx );
		uint64 uiEndTimestamp = cellGcmGetTimeStamp( endIdx );

		uint64 uiRsxTimeInNanoSeconds = uiEndTimestamp - uiStartTimestamp;
		OnFrameTimestampAvailableRsx( uiRsxTimeInNanoSeconds / 1000000.0f );
	}

	//--------------------------------------------------------------------------------------------------
	// Insert new flip command and flush gpu
	//--------------------------------------------------------------------------------------------------

	// reset FlipStatus = 1
	cellGcmResetFlipStatus();

	// queue Flip command
	GCM_FUNC( cellGcmSetFlipWithWaitLabel, m_display.surfaceFlipIdx, GCM_LABEL_FLIP_CONTROL, LABEL_FLIP_CONTROL_READY);
	m_display.Flip();

	GCM_FUNC( cellGcmSetWriteCommandLabel, GCM_LABEL_FLIP_CONTROL, LABEL_FLIP_CONTROL_WAIT);

	GCM_FUNC( cellGcmSetWaitFlip );
	GCM_FUNC( cellGcmFlush );

	extern void Ps3gcmLocalMemoryAllocator_Reclaim();
	Ps3gcmLocalMemoryAllocator_Reclaim();

	//--------------------------------------------------------------------------------------------------
	// Start a new frame
	//--------------------------------------------------------------------------------------------------

newframe:

	m_frameNo ++;

	// Insert start of gpu timestamp
	idx = m_frameNo&1;
	startIdx = GCM_REPORT_TIMESTAMP_FRAME_FIRST + (idx*2);
	GCM_FUNC( cellGcmSetTimeStamp, startIdx );

	// Put RSX into known state for start of frame
	gpGcmDrawState->ResetRsxState();

	// Moved from DX present()
	GCM_FUNC( cellGcmSetInvalidateVertexCache );
}	

//--------------------------------------------------------------------------------------------------
// Buffer management
//--------------------------------------------------------------------------------------------------

CPs3gcmBuffer * CPs3gcmBuffer::New( uint32 uiSize, CPs3gcmAllocationType_t uType )
{
	CPs3gcmBuffer * p = new CPs3gcmBuffer;
	p->m_lmBlock.Alloc( uType, uiSize );
	return p;
}

void CPs3gcmBuffer::Release()
{
	// Wait for RSX to finish using the buffer memory
	// and free it later
	m_lmBlock.Free();
	delete this;
}

