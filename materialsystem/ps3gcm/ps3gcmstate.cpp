//========== Copyright © 2010, Valve Corporation, All rights reserved. ========


#include "dxabstract.h"
#include "ps3gcmstate.h"
#include "utlmap.h"
#include "ps3/ps3gcmlabels.h"
#include "spugcm.h"
#include "memdbgon.h"
#include <sysutil/sysutil_sysparam.h>
#include "ps3/ps3_helpers.h"

//////////////////////////////////////////////////////////////////////////

#define MB (1024*1024)

// the safety margin, in bytes, for the command buffer
// we may use the 16 bytes beyond the buffer to write jump-back commands
const uint s_nCmdSizeSafetyMargin = 32;


CPs3gcmGlobalState g_ps3gcmGlobalState;
uint8 g_dataShaderPsEmpty[] = {
#include "shader_ps_empty.h"
};

int32_t GcmContextPermCallbackError(struct CellGcmContextData *pCtx, uint32_t nAlloc )
{
	Error( "Unexpected error while filling permanent cmd buffer: %d Kb filled up, choked on allocating %d words. RefCount=%d. Permanent cmd buffer size is not enough, or there's a leak.\n", uintp(pCtx->current)-uintp(pCtx->begin), nAlloc, g_ps3gcmGlobalState.m_nCmdBufferRefCount );
	return EINVAL; // it's not CELL_OK
}

#if GCM_CTX_UNSAFE_MODE
static int32_t Ps3gcmGlobalCommandBufferReserveCallback( struct CellGcmContextData *context, uint32_t nCount )
{
	nCount;
	g_ps3gcmGlobalState.CmdBufferReservationCallback( context );
	return CELL_OK;
}
#undef cellGcmFlush	// we need it to implement our command buffer flush

uint32 CPs3gcmGlobalState::GetRsxControlNextReferenceValue()
{
	static uint32 s_uiReferenceValue;
	return ++ s_uiReferenceValue;
}

#endif

static inline uint32 Ps3gcmGlobalCommandBufferUsageSegmentBytes( uint32 nCmdSize )
{
#if GCM_CTX_UNSAFE_MODE
	// Triple buffer in unsafe mode
	return ( nCmdSize / 3 ) & ~127;
#else
	// Default 32 Kb buffers in safe mode
	return 32768;
#endif
}
static inline uint32 Ps3gcmGlobalCommandBufferUsageCookie( struct CellGcmContextData *context, void *pIoAddress, uint32 nCmdSize )
{
	// Note only which portion is in use by RSX when switching to the next segment:
	uint32 const offBegin = ( ( char * ) context->begin ) - ( ( char * ) pIoAddress );
	uint32 const numSegmentBytes = Ps3gcmGlobalCommandBufferUsageSegmentBytes( nCmdSize );
	return ( offBegin / numSegmentBytes );
}

uint32 CalculateMemorySizeFromCmdLineParam( char const *pCmdParamName, uint32 nDefaultValue, uint32 nMinValue )
{
	uint nCmdSize = 0;
	if ( const char *p = CommandLine()->ParmValue( pCmdParamName, (const char *)NULL ) )
	{
		for(; isdigit( *p ) ; ++p )
		{
			nCmdSize = nCmdSize * 10 + (*p-'0');
		}		
		switch( *p )
		{
		case '\0':
		case 'm':
		case 'M':
			nCmdSize *= 1024 * 1024;
			break;
		case 'k':
		case 'K':
			nCmdSize *= 1024;
			break;
		case 'b':
		case 'B':
			break;
		default:
			nCmdSize = 0;
			break;
		}
	}
	else
	{
		return nDefaultValue;
	}
	return MAX( nCmdSize, nMinValue );
}

int32 CPs3gcmGlobalState::Init()
{
	MEM_ALLOC_CREDIT_( "GCM INIT" );
	m_nIoLocalOffsetEmptyFragmentProgramSetupRoutine = 0;
	
	///////////////////////////////////////////////////////////////////////////////////
	// Negotiate video output resolution, setup display gamma, surface pitch and so on
	//
	if( int nError= InitVideo() )
		return nError;

	//////////////////////////////////////////////////////////////////////////
	//
	// Allocate GCM IO & CMD buffers
	//
	
	// Default sizes for IO and CMD buffers
	#if GCM_CTX_UNSAFE_MODE
	m_nCmdSize = 3*MB;
	#else
	m_nCmdSize = 512 * 1024; // TEST: 256Kb or less is for testing only; production should be at least 512Kb; NOTE: TEST WITH < 128k !!
	#endif
	m_nCmdSize = CalculateMemorySizeFromCmdLineParam( "-gcmSizeCMD", m_nCmdSize, m_nCmdSize );

	const uint nMaxIoMappedMemorySize = 256*MB;

	// start with nothing preallocated
	m_nIoSize = m_nIoSizeNotPreallocated = nMaxIoMappedMemorySize;
	m_pIoAddress = NULL;
	CreateIoBuffers();	// let the buffers creation routine calculate the required buffers size

	uint32 nMinIoMemoryRequired = nMaxIoMappedMemorySize - ( m_nIoSizeNotPreallocated - m_nCmdSize );
	uint32 nIoMemoryAllocated = AlignValue( nMinIoMemoryRequired, MB ); // the size should not change here in release!
	m_nIoSize = m_nIoSizeNotPreallocated = nIoMemoryAllocated;

	// Try to allocate main memory that will be mapped to IO address space
	Msg( "======== GCM IO memory allocated  @0x%p   size = %d MB ========\n", m_pIoAddress, m_nIoSize / 1024 / 1024 );
	sys_addr_t pIoAddress = NULL;
	int nError = sys_memory_allocate( m_nIoSize, SYS_MEMORY_PAGE_SIZE_1M, &pIoAddress );
	if ( CELL_OK != nError )
	{
		Msg( "sys_memory_allocate failed to allocate %d bytes (err: %d)\n", m_nIoSize, nError );
		return nError;
	}
	m_pIoAddress = (void *)pIoAddress;

	CreateIoBuffers(); // Create the IO buffers for real now

	// Determine how much memory has been used:
	uint nCmdSizeSlackInitial = ( m_nIoSizeNotPreallocated - m_nCmdSize ); // we can use this much more for anyting, and the rest for cmd buffer (which is not preallocated in CreateIOBuffers())
	Assert( nCmdSizeSlackInitial < MB );

	g_spuGcm.UseIoBufferSlack( nCmdSizeSlackInitial );
	Assert( m_nIoSizeNotPreallocated >= m_nCmdSize ); // we should have enough space for cmd buffer, even thought we may use up some slack
	uint nCmdSizeSlackRemaining = ( m_nIoSizeNotPreallocated - m_nCmdSize ) & ( MB - 1 ); // we need this much for cmd buffer (not preallocated in CreateIOBuffers())
	
	Msg( "IO Buffer slack: was %u kB, used %u kB, reusing remaining %u kB for cmd buffer\n", nCmdSizeSlackInitial / 1024, ( nCmdSizeSlackInitial - nCmdSizeSlackRemaining ) / 1024, nCmdSizeSlackRemaining / 1024 );

	// leave space for all buffers, and use the rest of the space for cmd buffer
	// we need the cmd buffer to be 16-byte aligned and have 16 byte slack in the end . Let's make it 32 for good measure
	m_nCmdSize = ( m_nIoSizeNotPreallocated - s_nCmdSizeSafetyMargin ) & -16;
	m_nCmdBufferRefCount = 0;
	
	if( int nError = InitGcm() )
		return nError;
	

	// Retrieve RSX local memory config
	CellGcmConfig rsxConfig;
	cellGcmGetConfiguration( &rsxConfig );
	m_pLocalBaseAddress = rsxConfig.localAddress;
	m_nLocalSize = rsxConfig.localSize;
	cellGcmAddressToOffset( m_pLocalBaseAddress, &m_nLocalBaseOffset );
	Assert( m_nLocalBaseOffset == 0 );

	// Initialize allocator/tracker
	extern void Ps3gcmLocalMemoryAllocator_Init();
	Ps3gcmLocalMemoryAllocator_Init();

	CreateRsxBuffers();
	CreateEmptyPixelShader();
	CreateDebugStripeTextureBuffer();

	g_spuGcm.OnGcmInit();

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
	g_spuGcm.CreateRsxBuffers();
}

void CPs3gcmGlobalState::DrawDebugStripe( uint nScreenX, uint nScreenY, uint nStripeY, uint nStripeWidth, uint nStripeHeight, int nNext )
{
	if( nScreenX < m_nRenderSize[0] )
	{
		uint nDstOffset = m_display.surfaceColor[ m_display.NextSurfaceIndex( nNext + CPs3gcmDisplay::SURFACE_COUNT ) ].Offset();
		uint nSrcOffset = m_debugStripeImageBuffer.Offset();
		uint nRenderStripeWidth = MIN( ( uint )( m_nRenderSize[0] - nScreenX - 4 ), nStripeWidth );
		Assert( nScreenX + nRenderStripeWidth < ( uint )m_nRenderSize[0] );
		
		//static int x0 = 128, y0 = 64,x1 = 0,y1 = 0, w1 = 128, h1 = 1;
		Assert( nStripeY + nStripeHeight <= 4 );
		
		GCM_FUNC( cellGcmSetTransferImage, CELL_GCM_TRANSFER_LOCAL_TO_LOCAL,
			nDstOffset,
			m_nSurfaceRenderPitch,
			/*x0,y0,*/nScreenX, nScreenY, 
			nSrcOffset,
			m_nRenderSize[0] * 4,
			/*x1,y1*/0, nStripeY, nRenderStripeWidth, nStripeHeight,
			4 );
	}
}

void CPs3gcmGlobalState::CreateDebugStripeTextureBuffer()
{
	const int s_nDebugScanlineColor[] = { 0xFFFFFFFF, 0xFFFF80FF, 0xFF00FF00, 0xFFFFFF00 };
	
	m_debugStripeImageBuffer.Alloc( kAllocPs3gcmTextureData, m_nRenderSize[0] * 4 * ARRAYSIZE( s_nDebugScanlineColor ) );
	
	uint32 * pEaLocalDebugScanline = ( uint32* ) m_debugStripeImageBuffer.DataInLocalMemory();
	for( uint y = 0; y < ARRAYSIZE( s_nDebugScanlineColor ); ++y )
	{
		for( uint nX = 0; nX < m_nRenderSize[0]; ++nX )
		{
			uint nColor = s_nDebugScanlineColor[y];
			switch( nX % 100 )
			{
			case 98:case 99: case 1:case 2: nColor = 0x80808080; break;// 
			case 0: nColor = 0; break;// 
			case 50: nColor = 0xFF800000; break; // 
			default:
				switch( nX % 10 )
				{
				case 8: case 9: case 1: case 2: nColor = 0xFFFFFF80; break;// grey;
				case 0: nColor = 0xFF40C0FF; break;// 
				}
				break;
			}
			pEaLocalDebugScanline[nX + m_nRenderSize[0] * y] = nColor;
		}
	}
}

void CPs3gcmGlobalState::CreateEmptyPixelShader()
{
	//////////////////////////////////////////////////////////////////////////
	//
	// Create an empty pixel shader, upload ucode to local memory
	//
	m_pShaderPsEmpty = reinterpret_cast< CgBinaryProgram * >( g_dataShaderPsEmpty );
	m_pShaderPsEmptyBuffer.Alloc( kAllocPs3GcmShader, m_pShaderPsEmpty->ucodeSize );
	V_memcpy( m_pShaderPsEmptyBuffer.DataInLocalMemory(), ( (char*)m_pShaderPsEmpty ) + m_pShaderPsEmpty->ucode, m_pShaderPsEmpty->ucodeSize );
	
	CellGcmContextData * permBufferContext = CmdBufferAlloc( );
	uint32 * pBeginPermCmdBuffer = permBufferContext->current;
	m_nIoLocalOffsetEmptyFragmentProgramSetupRoutine = CmdBufferToIoOffset( pBeginPermCmdBuffer );
	cellGcmSetFragmentProgramInline( permBufferContext, ( CGprogram ) m_pShaderPsEmpty, m_pShaderPsEmptyBuffer.Offset() );
	cellGcmSetReturnCommand( permBufferContext );
}


void CPs3gcmGlobalState::CreateIoBuffers()
{
	//////////////////////////////////////////////////////////////////////////////
	//
	// Preallocate IO memory buffers, compute the max memory available to the command buffer 
	// THe memory is allocated from the end towards the beginning (where the cmd buffer is)
	// and the cmd buffer occupies all the extra memory at the start
	//
	uint nPermanentCmdBufferSize = 16 * 1024; // reduce by padding in the end, to keep nice alignment
	
	g_spuGcm.CreateIoBuffers();	

	// NOTE: the buffer MUST have 1KB padding in the end to prevent overfetch RSX crash!
	m_cmdBufferPermContext.begin = m_cmdBufferPermContext.current = ( uint32* ) IoMemoryPrealloc( 128, nPermanentCmdBufferSize );
	m_cmdBufferPermContext.end = ( uint32* )( uintp( m_cmdBufferPermContext.begin ) + nPermanentCmdBufferSize );
	m_cmdBufferPermContext.callback = GcmContextPermCallbackError;

	// RSX data transfer buffer
	m_nRsxDataTransferBufferSize = 0*MB;
	m_nRsxDataTransferBufferSize = CalculateMemorySizeFromCmdLineParam( "-gcmSizeTransfer", m_nRsxDataTransferBufferSize );
	m_pRsxDataTransferBuffer = IoMemoryPrealloc( 1, m_nRsxDataTransferBufferSize );

	// RSX main memory pool buffer
	m_nRsxMainMemoryPoolBufferSize = PS3GCM_VBIB_IN_IO_MEMORY ? 60 * MB : 0 * MB; 
	m_pRsxMainMemoryPoolBuffer = m_nRsxMainMemoryPoolBufferSize ? IoMemoryPrealloc( 128, m_nRsxMainMemoryPoolBufferSize ) : NULL;
}



int CPs3gcmGlobalState::InitVideo()
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


//////////////////////////////////////////////////////////////////////////
//
// Init GCM
//
int CPs3gcmGlobalState::InitGcm()
{
	m_nFlushCounter = 0;
	int32 result = cellGcmInit( m_nCmdSize, m_nIoSize, m_pIoAddress );
	if ( result < CELL_OK )
		return result;

	g_pGcmSharedData->m_nIoMemorySize = m_nIoSize;
	g_pGcmSharedData->m_pIoMemory     = m_pIoAddress;

	// Set the flip mode
	cellGcmSetFlipMode( CELL_GCM_DISPLAY_HSYNC );

	cellGcmAddressToOffset( m_pIoAddress, &m_nIoOffsetDelta );
	m_nIoOffsetDelta -= uintp( m_pIoAddress );
	
	if( CmdBufferToIoOffset( gCellGcmCurrentContext->begin ) != SYSTEM_CMD_BUFFER_RESERVED_AREA )
	{
		Warning(
			"********************************************************************************\n"
			"**********Unexpected GCM system command buffer begin 0x%08x*****************\n"
			"********************************************************************************\n",
			CmdBufferToIoOffset( gCellGcmCurrentContext->begin )
		);
	}
	
#if GCM_CTX_UNSAFE_MODE
	// Set custom callback function
	GCM_CTX->callback = Ps3gcmGlobalCommandBufferReserveCallback;
	m_pCurrentCmdBufferSegmentRSX = ( uint32_t const volatile * ) cellGcmGetLabelAddress( GCM_LABEL_GLOBAL_CMD_BUFFER_BEGIN );
	*const_cast< uint32_t * >( m_pCurrentCmdBufferSegmentRSX ) = Ps3gcmGlobalCommandBufferUsageCookie( GCM_CTX, m_pIoAddress, m_nCmdSize );

	// In unsafe mode split command buffer into segments
	GCM_CTX->end = ( uint32 * )( Ps3gcmGlobalCommandBufferUsageSegmentBytes( m_nCmdSize ) + ( ( char * ) m_pIoAddress ) ) - 4;

	m_pCurrentCmdBufferUnflushedBeginRSX = GCM_CTX->begin;
	
#endif

	return CELL_OK;
}



void CPs3gcmGlobalState::Shutdown()
{
	// Let RSX wait for final flip
	GCM_FUNC( cellGcmSetWaitFlip );

#if GCM_CTX_UNSAFE_MODE
	// Let PPU wait for all commands done (include waitFlip)
	uint32 rsxref = GetRsxControlNextReferenceValue();
	GCM_FUNC( cellGcmFinish, rsxref );
#else	
	g_spuGcm.Shutdown();
#endif

	cellSysmoduleUnloadModule( CELL_SYSMODULE_AVCONF_EXT );
}

void CPs3gcmGlobalState::CmdBufferFlush( CmdBufferFlushType_t eFlushType )
{
	Assert( !g_spuGcm.IsDeferredDrawQueue() );

	g_spuGcm.CmdBufferFlush();
	m_nFlushCounter++;

	if ( eFlushType == kFlushEndFrame )
	{
		extern void Ps3gcmLocalMemoryAllocator_Reclaim();
		Ps3gcmLocalMemoryAllocator_Reclaim();
	}
}


void CPs3gcmGlobalState::CmdBufferFinish()
{
#if GCM_CTX_UNSAFE_MODE
	CmdBufferFlush( CPs3gcmGlobalState::kFlushForcefully );
	uint32 rsxref = g_ps3gcmGlobalState.GetRsxControlNextReferenceValue();
	GCM_FUNC( cellGcmFinish, rsxref );
#else
	g_spuGcm.CmdBufferFinish();
#endif	
}


#if GCM_CTX_UNSAFE_MODE
void CPs3gcmGlobalState::CmdBufferReservationCallback( struct CellGcmContextData *context )
{
	enum CmdBufferSize_t
	{
		kJumpCmdSize = 1 * sizeof(uint32),
		kSegmentReservedSize = kJumpCmdSize,
	};

	uint32 const uiOldRsxCookie = Ps3gcmGlobalCommandBufferUsageCookie( context, m_pIoAddress, m_nCmdSize );
	uint32 const uiSegmentSize = Ps3gcmGlobalCommandBufferUsageSegmentBytes( m_nCmdSize );
	uint32 uiNewBegin = ( ( char * ) context->end ) - ( ( char * ) m_pIoAddress ) + kSegmentReservedSize;
	uint32 uiNewEnd = uiNewBegin + uiSegmentSize - kSegmentReservedSize;
	if ( uiNewBegin >= m_nCmdSize )
	{
		uiNewBegin = SYSTEM_CMD_BUFFER_RESERVED_AREA;
		uiNewEnd = uiSegmentSize - kSegmentReservedSize;
	}

	// Let RSX go ahead and grab the currently full segment
	cellGcmFlush( context );
	
	// Insert the jump command to jump into the new segment
	cellGcmSetJumpCommandUnsafeInline( context, uiNewBegin );

	// Prepare the settings for the new segment
	context->begin = reinterpret_cast< uint32_t * >( ( ( char * ) m_pIoAddress ) + uiNewBegin );
	context->end = reinterpret_cast< uint32_t * >( ( ( char * ) m_pIoAddress ) + uiNewEnd );
	context->current = context->begin;

	// Make sure that RSX is not using the new context segment
	uint32 const uiNewRsxCookie = Ps3gcmGlobalCommandBufferUsageCookie( context, m_pIoAddress, m_nCmdSize );
	if ( uiNewRsxCookie != uiOldRsxCookie )
	{
		while ( uiNewRsxCookie == *m_pCurrentCmdBufferSegmentRSX )
			sys_timer_usleep( 30 );	// RSX is still using the segments with the new cookie

		// The first word in the new segment will be to have RSX set label marking the segment
		// as being used by RSX
		cellGcmSetWriteCommandLabelUnsafeInline( context, GCM_LABEL_GLOBAL_CMD_BUFFER_BEGIN, uiNewRsxCookie );
	}

#if GCM_CTX_UNSAFE_MODE
	m_pCurrentCmdBufferUnflushedBeginRSX = context->current;
#endif
}
#endif


//////////////////////////////////////////////////////////////////////////


void * CPs3gcmGlobalState::IoMemoryPrealloc( uint nAlign, uint nSize )
{
	Assert( !( nAlign & ( nAlign - 1 ) ) );
	// we never need to allocate with alignment greater than the base of IO address;
	// hence we can align the not-preallocated size only
	Assert( !( ( nAlign - 1 ) & uintp( m_pIoAddress ) ) );
	m_nIoSizeNotPreallocated = ( m_nIoSizeNotPreallocated - nSize ) & ~( nAlign - 1 );
	uintp eaIoAddress = uintp( m_pIoAddress ) + m_nIoSizeNotPreallocated;
	Assert( !( eaIoAddress & ( nAlign - 1 ) ) );
	return m_pIoAddress ? ( void* )eaIoAddress : NULL;
}


void * CPs3gcmGlobalState::IoSlackAlloc( uint nAlign, uint nSize )
{
	Assert( m_pIoAddress ); // don't call this in the first pass of CreateIoBuffers() !
	
	Assert( !( nAlign & ( nAlign - 1 ) ) ); // alignment must be sane!
	
	// we never need to allocate with alignment greater than the base of IO address;
	// hence we can align the not-preallocated size only
	Assert( !( ( nAlign - 1 ) & uintp( m_pIoAddress ) ) );
	
	// preview what the new free io size will be ( may be negative if nSize is too big )
	signed int nNewIoSizeNotPreallocated = ( m_nIoSizeNotPreallocated - nSize ) & ~( nAlign - 1 );
	
	if( nNewIoSizeNotPreallocated > signed( m_nCmdSize + s_nCmdSizeSafetyMargin ) )
	{
		// we still have enough room left for command buffer, so we can allocate this memory and it'll be in IO slack
		Msg( "Saving %d kB, using IO memory slack\n", nSize / 1024 );
		m_nIoSizeNotPreallocated = nNewIoSizeNotPreallocated;
		uintp eaIoAddress = uintp( m_pIoAddress ) + m_nIoSizeNotPreallocated;
		Assert( !( eaIoAddress & ( nAlign - 1 ) ) );
		return ( void* )eaIoAddress;
	}
	else
	{
		return MemAlloc_AllocAligned( nSize, nAlign );
	}
}




void CPs3gcmGlobalState::IoSlackFree( void * eaMemory )
{
	if( eaMemory && !IsIoMemory( eaMemory ) )
	{
		MemAlloc_FreeAligned( eaMemory );
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Texture layouts
//

//===============================================================================

#ifdef _CERT
#define GLMTEX_FMT_DESC( x )
#else
#define GLMTEX_FMT_DESC( x ) x ,
#endif

#define CELL_GCM_REMAP_MODE_OIO(order, inputARGB, outputARGB) \
	(((order)<<16)|((inputARGB))|((outputARGB)<<8))
#define REMAPO( x ) CELL_GCM_TEXTURE_REMAP_ORDER_X##x##XY
#define REMAP4(a,r,g,b) (((a)<<0)|((r)<<2)|((g)<<4)|((b)<<6))

#define REMAP_ARGB  REMAP4( CELL_GCM_TEXTURE_REMAP_FROM_A, CELL_GCM_TEXTURE_REMAP_FROM_R, CELL_GCM_TEXTURE_REMAP_FROM_G, CELL_GCM_TEXTURE_REMAP_FROM_B )
#define REMAP_4		REMAP4( CELL_GCM_TEXTURE_REMAP_REMAP, CELL_GCM_TEXTURE_REMAP_REMAP, CELL_GCM_TEXTURE_REMAP_REMAP, CELL_GCM_TEXTURE_REMAP_REMAP )
#define REMAP_13	REMAP4( CELL_GCM_TEXTURE_REMAP_ONE, CELL_GCM_TEXTURE_REMAP_REMAP, CELL_GCM_TEXTURE_REMAP_REMAP, CELL_GCM_TEXTURE_REMAP_REMAP )
#define REMAP_4X(x)	REMAP4( x, x, x, x )
#define REMAP_13X(y, x)	REMAP4( y, x, x, x )

#define REMAP_ALL_DEFAULT CELL_GCM_REMAP_MODE_OIO( REMAPO(Y), REMAP_ARGB, REMAP_4 )
#define REMAP_ALL_DEFAULT_X CELL_GCM_REMAP_MODE_OIO( REMAPO(X), REMAP_ARGB, REMAP_4 )

#define CAP( x ) CPs3gcmTextureLayout::Format_t::kCap##x

CPs3gcmTextureLayout::Format_t g_ps3texFormats[PS3_TEX_MAX_FORMAT_COUNT] = 
{
	// summ-name						d3d-format
	//			gcmRemap
	//									gcmFormat
	//			gcmPitchPer4X			gcmFlags

	{ GLMTEX_FMT_DESC("_D16")			D3DFMT_D16,
		REMAP_ALL_DEFAULT,
		8,
		CELL_GCM_TEXTURE_DEPTH16,
		0 },
	
	{ GLMTEX_FMT_DESC("_D24X8")			D3DFMT_D24X8,
		REMAP_ALL_DEFAULT,
		16,
		CELL_GCM_TEXTURE_DEPTH24_D8,
		0 },

	{ GLMTEX_FMT_DESC("_D24S8")			D3DFMT_D24S8,
		REMAP_ALL_DEFAULT,
		16,
		CELL_GCM_TEXTURE_DEPTH24_D8,
		0 },

	{ GLMTEX_FMT_DESC("_A8R8G8B8")		D3DFMT_A8R8G8B8,
		REMAP_ALL_DEFAULT,
		16,
		CELL_GCM_TEXTURE_A8R8G8B8,
		CAP(SRGB) },

	{ GLMTEX_FMT_DESC("_X8R8G8B8")		D3DFMT_X8R8G8B8,
		REMAP_ALL_DEFAULT,
		16,
		CELL_GCM_TEXTURE_A8R8G8B8,
		CAP(SRGB) },

	{ GLMTEX_FMT_DESC("_X1R5G5B5")		D3DFMT_X1R5G5B5,
		CELL_GCM_REMAP_MODE_OIO( REMAPO(X), REMAP_ARGB, REMAP_13 ),
		8,
		CELL_GCM_TEXTURE_R5G6B5,
		0 },

	{ GLMTEX_FMT_DESC("_A1R5G5B5")		D3DFMT_A1R5G5B5,
		REMAP_ALL_DEFAULT_X,
		8,
		CELL_GCM_TEXTURE_A1R5G5B5,
		0 },

	{ GLMTEX_FMT_DESC("_L8")			D3DFMT_L8,
		CELL_GCM_REMAP_MODE_OIO( REMAPO(Y), REMAP_4X(CELL_GCM_TEXTURE_REMAP_FROM_B), REMAP_13 ),
		4,
		CELL_GCM_TEXTURE_B8,
		0 },

	{ GLMTEX_FMT_DESC("_A8L8")			D3DFMT_A8L8,
		CELL_GCM_REMAP_MODE_OIO( REMAPO(Y), REMAP_13X( CELL_GCM_TEXTURE_REMAP_FROM_G, CELL_GCM_TEXTURE_REMAP_FROM_B), REMAP_4 ),
		8,
		CELL_GCM_TEXTURE_G8B8,
		0 },

	{ GLMTEX_FMT_DESC("_DXT1")			D3DFMT_DXT1,
		CELL_GCM_REMAP_MODE_OIO( REMAPO(Y), REMAP_ARGB, REMAP_13 ),
		8,
		CELL_GCM_TEXTURE_COMPRESSED_DXT1,
		CAP(SRGB) | CAP(4xBlocks) },

	{ GLMTEX_FMT_DESC("_DXT3")			D3DFMT_DXT3,
		REMAP_ALL_DEFAULT,
		16,
		CELL_GCM_TEXTURE_COMPRESSED_DXT23,
		CAP(SRGB) | CAP(4xBlocks) },

	{ GLMTEX_FMT_DESC("_DXT5")			D3DFMT_DXT5,
		REMAP_ALL_DEFAULT,
		16,
		CELL_GCM_TEXTURE_COMPRESSED_DXT45,
		CAP(SRGB) | CAP(4xBlocks) },

	{ GLMTEX_FMT_DESC("_A16B16G16R16F")	D3DFMT_A16B16G16R16F,
		REMAP_ALL_DEFAULT_X,
		32,
		CELL_GCM_TEXTURE_W16_Z16_Y16_X16_FLOAT,
		0 },

	{ GLMTEX_FMT_DESC("_A16B16G16R16")	D3DFMT_A16B16G16R16,
		REMAP_ALL_DEFAULT_X,
		64,
		CELL_GCM_TEXTURE_W32_Z32_Y32_X32_FLOAT,
		0 },

	{ GLMTEX_FMT_DESC("_A32B32G32R32F")	D3DFMT_A32B32G32R32F,
		REMAP_ALL_DEFAULT_X,
		64,
		CELL_GCM_TEXTURE_W32_Z32_Y32_X32_FLOAT,
		0 },

	{ GLMTEX_FMT_DESC("_R8G8B8")		D3DFMT_R8G8B8,
		CELL_GCM_REMAP_MODE_OIO( REMAPO(Y),
			REMAP4( CELL_GCM_TEXTURE_REMAP_FROM_B, CELL_GCM_TEXTURE_REMAP_FROM_A, CELL_GCM_TEXTURE_REMAP_FROM_R, CELL_GCM_TEXTURE_REMAP_FROM_G ),
			REMAP_13 ),
		16,
		CELL_GCM_TEXTURE_A8R8G8B8,
		CAP(SRGB) },

	{ GLMTEX_FMT_DESC("_A8")			D3DFMT_A8,
		CELL_GCM_REMAP_MODE_OIO( REMAPO(Y),
			REMAP4( CELL_GCM_TEXTURE_REMAP_FROM_B, CELL_GCM_TEXTURE_REMAP_FROM_R, CELL_GCM_TEXTURE_REMAP_FROM_B, CELL_GCM_TEXTURE_REMAP_FROM_B ),
			REMAP_13X( CELL_GCM_TEXTURE_REMAP_REMAP, CELL_GCM_TEXTURE_REMAP_ZERO ) ),
		4,
			CELL_GCM_TEXTURE_B8,
		0 },

	{ GLMTEX_FMT_DESC("_R5G6B5")		D3DFMT_R5G6B5,
		CELL_GCM_REMAP_MODE_OIO( REMAPO(Y),
			REMAP4( CELL_GCM_TEXTURE_REMAP_FROM_B, CELL_GCM_TEXTURE_REMAP_FROM_A, CELL_GCM_TEXTURE_REMAP_FROM_R, CELL_GCM_TEXTURE_REMAP_FROM_G ),
			REMAP_13 ),
		16,
		CELL_GCM_TEXTURE_A8R8G8B8,
		CAP(SRGB) },

	{ GLMTEX_FMT_DESC("_Q8W8V8U8")		D3DFMT_Q8W8V8U8,
		REMAP_ALL_DEFAULT,
		16,
		CELL_GCM_TEXTURE_A8R8G8B8,
		CAP(SRGB) },
};

uint g_nPs3texFormatCount = PS3_TEX_CANONICAL_FORMAT_COUNT;

#undef CAP
#undef GLMTEX_FMT_DESC

static bool Ps3texLayoutLessFunc( CPs3gcmTextureLayout::Key_t const &a, CPs3gcmTextureLayout::Key_t const &b )
{
	return ( memcmp( &a, &b, sizeof( CPs3gcmTextureLayout::Key_t ) ) < 0 );
}
static CUtlMap< CPs3gcmTextureLayout::Key_t, CPs3gcmTextureLayout const * > s_ps3texLayouts( Ps3texLayoutLessFunc );

CPs3gcmTextureLayout const * CPs3gcmTextureLayout::New( Key_t const &k )
{
	// SPUGCM shared area must be initialized BEFORE anyone calls this function
	Assert( g_spuGcmShared.m_eaPs3texFormats == g_ps3texFormats );

	// look up 'key' in the map and see if it's a hit, if so, bump the refcount and return
	// if not, generate a completed layout based on the key, add to map, set refcount to 1, return that
	unsigned short index = s_ps3texLayouts.Find( k );
	if ( index != s_ps3texLayouts.InvalidIndex() )
	{
		CPs3gcmTextureLayout const *layout = s_ps3texLayouts[ index ];
		++ layout->m_refCount;
		return layout;
	}

	// Need to generate complete information about the texture layout
	uint8 nMips = ( k.m_texFlags & kfMip ) ? k.m_nActualMipCount : 1;
	uint8 nFaces = ( k.m_texFlags & kfTypeCubeMap ) ? 6 : 1;
	uint32 nSlices = nMips * nFaces;

	// Allocate layout memory
	size_t numLayoutBytes = sizeof( CPs3gcmTextureLayout ) + nSlices * sizeof( Slice_t );
	CPs3gcmTextureLayout *layout = ( CPs3gcmTextureLayout * ) MemAlloc_AllocAligned( numLayoutBytes, 16 );
	memset( layout, 0, numLayoutBytes );
	memcpy( &layout->m_key, &k, sizeof( Key_t ) );
	layout->m_refCount = 1;

	// Find the format descriptor
	for ( int j = 0; j < PS3_TEX_CANONICAL_FORMAT_COUNT; ++ j )
	{
		if ( g_ps3texFormats[j].m_d3dFormat == k.m_texFormat )
		{
			layout->m_nFormat = j;
			break;
		}
		Assert( j != PS3_TEX_CANONICAL_FORMAT_COUNT - 1 );
	}

	layout->m_mipCount = nMips;

	//
	// Slices
	//
	bool bSwizzled = layout->IsSwizzled();
	size_t fmtPitch = layout->GetFormatPtr()->m_gcmPitchPer4X;
	size_t fmtPitchBlock = ( layout->GetFormatPtr()->m_gcmCaps & CPs3gcmTextureLayout::Format_t::kCap4xBlocks ) ? 16 : 4;
	size_t numDataBytes = 0;
	Slice_t *pSlice = &layout->m_slices[0];
	for ( int face = 0; face < nFaces; ++ face )
	{
		// For cubemaps every next face in swizzled addressing
		// must be aligned on 128-byte boundary
		if ( bSwizzled )
		{
			numDataBytes = ( numDataBytes + 127 ) & ~127;
		}

		for ( int mip = 0; mip < nMips; ++ mip, ++ pSlice )
		{
			for ( int j = 0; j < ARRAYSIZE( k.m_size ); ++ j )
			{
				pSlice->m_size[j] = k.m_size[j] >> mip;
				pSlice->m_size[j] = MAX( pSlice->m_size[j], 1 );
			}
			pSlice->m_storageOffset = numDataBytes;

			size_t numTexels;
			// For linear layout textures every mip row must be padded to the
			// width of the original highest level mip so that the pitch was
			// the same for every mip
			if ( bSwizzled )
				numTexels = ( pSlice->m_size[0] * pSlice->m_size[1] * pSlice->m_size[2] );
			else
				numTexels = ( k.m_size[0] * pSlice->m_size[1] * pSlice->m_size[2] );

			size_t numBytes = ( numTexels * fmtPitch ) / fmtPitchBlock;

			if ( layout->GetFormatPtr()->m_gcmCaps & CPs3gcmTextureLayout::Format_t::kCap4xBlocks )
			{
				// Ensure the size of the smallest mipmap levels of DXT1/3/5 textures (the 1x1 and 2x2 mips) is accurately computed.
				numBytes = MAX( numBytes, fmtPitch );
			}

			pSlice->m_storageSize = MAX( numBytes, 1 );
			
			numDataBytes += pSlice->m_storageSize;
		}
	}
	// Make the total size 128-byte aligned
	// Realistically it is required only for depth textures
	numDataBytes = ( numDataBytes + 127 ) & ~127;

	//
	// Tiled and ZCull memory adjustments
	//
	layout->m_gcmAllocType = kAllocPs3gcmTextureData;
	if ( layout->IsTiledMemory() )
	{
		if( g_nPs3texFormatCount >= PS3_TEX_MAX_FORMAT_COUNT )
		{
			Error("Modified ps3 format array overflow. Increase PS3_TEX_MAX_FORMAT_COUNT appropriately and recompile\n");
		}
		Format_t *pModifiedFormat = &g_ps3texFormats[g_nPs3texFormatCount];
		V_memcpy( pModifiedFormat, layout->GetFormatPtr(), sizeof( Format_t ) );
		layout->m_nFormat = g_nPs3texFormatCount;
		g_nPs3texFormatCount ++;

		if ( k.m_texFlags & kfTypeDepthStencil )
		{
			//
			// Tiled Zcull Surface
			//
			uint32 zcullSize[2] = { AlignValue( k.m_size[0], 64 ), AlignValue( k.m_size[1], 64 ) };
			uint32 nDepthPitch = cellGcmGetTiledPitchSize( zcullSize[0] * 4 );
			pModifiedFormat->m_gcmPitchPer4X = nDepthPitch;

			uint32 uDepthBufferSize32bpp = nDepthPitch * zcullSize[1];
			uDepthBufferSize32bpp = AlignValue( uDepthBufferSize32bpp, PS3GCMALLOCATIONALIGN( kAllocPs3gcmDepthBuffer ) );
			
			Assert( uDepthBufferSize32bpp >= numDataBytes );
			numDataBytes = uDepthBufferSize32bpp;

			layout->m_gcmAllocType = kAllocPs3gcmDepthBuffer;
		}
		else
		{
			//
			// Tiled Color Surface
			//
			uint32 nTiledPitch = cellGcmGetTiledPitchSize( k.m_size[0] * layout->GetFormatPtr()->m_gcmPitchPer4X / 4 );
			pModifiedFormat->m_gcmPitchPer4X = nTiledPitch;

			if ( k.m_size[0] == 512 && k.m_size[1] == 512 && k.m_size[2] == 1 )
				layout->m_gcmAllocType = kAllocPs3gcmColorBuffer512;
			else if ( k.m_size[0] == g_ps3gcmGlobalState.m_nRenderSize[0] && k.m_size[1] == g_ps3gcmGlobalState.m_nRenderSize[1] && k.m_size[2] == 1 )
				layout->m_gcmAllocType = kAllocPs3gcmColorBufferFB;
			else if ( k.m_size[0] == g_ps3gcmGlobalState.m_nRenderSize[0]/4 && k.m_size[1] == g_ps3gcmGlobalState.m_nRenderSize[1]/4 && k.m_size[2] == 1 )
				layout->m_gcmAllocType = kAllocPs3gcmColorBufferFBQ;
			else
				layout->m_gcmAllocType = kAllocPs3gcmColorBufferMisc;

			uint32 uRenderSize = nTiledPitch * AlignValue( k.m_size[1], 32 ); // 32-line vertical alignment required in local memory
			if ( layout->m_gcmAllocType == kAllocPs3gcmColorBufferMisc )
				uRenderSize = AlignValue( uRenderSize, PS3GCMALLOCATIONALIGN( kAllocPs3gcmColorBufferMisc ) );

			Assert( uRenderSize >= numDataBytes );
			numDataBytes = uRenderSize;
		}
	}

	layout->m_storageTotalSize = numDataBytes;

	//
	// Finished creating the layout information
	//

#ifndef _CERT
	// generate summary
	// "target, format, +/- mips, base size"
	char scratch[1024];

	char	*targetname = targetname = "2D  ";
	if ( layout->IsVolumeTex() )
		targetname = "3D  ";
	if ( layout->IsCubeMap() )
		targetname = "CUBE";

	sprintf( scratch, "[%s %s %dx%dx%d mips=%d slices=%d flags=%02X%s]",
		targetname,
		layout->GetFormatPtr()->m_formatSummary,
		layout->m_key.m_size[0], layout->m_key.m_size[1], layout->m_key.m_size[2],
		nMips,
		nSlices,
		layout->m_key.m_texFlags,
		(layout->m_key.m_texFlags & kfSrgbEnabled) ? " SRGB" : ""
		);
	layout->m_layoutSummary = strdup( scratch );
#endif

	// then insert into map. disregard returned index.
	s_ps3texLayouts.Insert( k, layout );

	return layout;
}

void CPs3gcmTextureLayout::Release() const
{
	-- m_refCount;
	// keep the layout in the map for easy access
	Assert( m_refCount >= 0 );
}

//////////////////////////////////////////////////////////////////////////
//
// Texture management
//

CPs3gcmTexture * CPs3gcmTexture::New( CPs3gcmTextureLayout::Key_t const &key )
{
	//
	// Allocate a new layout for the texture
	//
	CPs3gcmTextureLayout const *pLayout = CPs3gcmTextureLayout::New( key );
	if ( !pLayout )
	{
		Debugger();
		return NULL;
	}

	CPs3gcmTexture *tex = (CPs3gcmTexture *)MemAlloc_AllocAligned( sizeof( CPs3gcmTexture ), 16 );
	memset( tex, 0, sizeof( CPs3gcmTexture ) ); // NOTE: This clears the CPs3gcmLocalMemoryBlock

	tex->m_layout = pLayout;
	CPs3gcmAllocationType_t uAllocationType = pLayout->m_gcmAllocType;

	if ( key.m_texFlags & CPs3gcmTextureLayout::kfNoD3DMemory )
	{
		if ( ( uAllocationType == kAllocPs3gcmDepthBuffer ) || ( uAllocationType == kAllocPs3gcmColorBufferMisc ) )
		{
			Assert( 0 );
			Warning( "ERROR: (CPs3gcmTexture::New) depth/colour buffers should not be marked with kfNoD3DMemory!\n" );
		}
		else
		{
			// Early-out, storage will be allocated later (via IDirect3DDevice9::AllocateTextureStorage)
			return tex;
		}
	}

	tex->Allocate();

	return tex;
}

void CPs3gcmTexture::Release()
{
	// Wait for RSX to finish using the texture memory
	// and free it later
	if ( m_lmBlock.Size() )
	{
		m_lmBlock.Free();
	}
	m_layout->Release();
	MemAlloc_FreeAligned( this );
}

bool CPs3gcmTexture::Allocate()
{
	if ( m_lmBlock.Size() )
	{
		// Already allocated!
		Assert( 0 );
		Warning( "ERROR: CPs3gcmTexture::Allocate called twice!\n" );
		return true;
	}

	CPs3gcmAllocationType_t uAllocationType = m_layout->m_gcmAllocType;
	const CPs3gcmTextureLayout::Key_t & key = m_layout->m_key;

	m_lmBlock.Alloc( uAllocationType, m_layout->m_storageTotalSize );

	if ( m_layout->IsTiledMemory() )
	{
		if ( uAllocationType == kAllocPs3gcmDepthBuffer )
		{
			bool bIs16BitDepth = ( m_layout->GetFormatPtr()->m_gcmFormat == CELL_GCM_TEXTURE_DEPTH16 ) || ( m_layout->m_nFormat == CELL_GCM_TEXTURE_DEPTH16_FLOAT );

			uint32 zcullSize[2] = { AlignValue( key.m_size[0], 64 ), AlignValue( key.m_size[1], 64 ) };

			uint32 uiZcullIndex = m_lmBlock.ZcullMemoryIndex();
			cellGcmBindZcull( uiZcullIndex,
				m_lmBlock.Offset(),
				zcullSize[0], zcullSize[1],
				m_lmBlock.ZcullMemoryStart(),
				bIs16BitDepth ? CELL_GCM_ZCULL_Z16 : CELL_GCM_ZCULL_Z24S8,
				CELL_GCM_SURFACE_CENTER_1,
				CELL_GCM_ZCULL_LESS,
				CELL_GCM_ZCULL_LONES,
				CELL_GCM_SCULL_SFUNC_ALWAYS,
				0, 0	// sRef, sMask
				);

			uint32 uiTileIndex = m_lmBlock.TiledMemoryIndex();
			cellGcmSetTileInfo( uiTileIndex, CELL_GCM_LOCATION_LOCAL, m_lmBlock.Offset(),
				m_layout->m_storageTotalSize, m_layout->DefaultPitch(), bIs16BitDepth ? CELL_GCM_COMPMODE_DISABLED : CELL_GCM_COMPMODE_Z32_SEPSTENCIL_REGULAR,
				m_lmBlock.TiledMemoryTagAreaBase(), // The area base + size/0x10000 will be allocated as the tag area.
				1 );	// Misc depth buffers on bank 1
			cellGcmBindTile( uiTileIndex );
		}
		else if ( uAllocationType == kAllocPs3gcmColorBufferMisc )
		{
			uint32 uiTileIndex = m_lmBlock.TiledMemoryIndex();
			cellGcmSetTileInfo( uiTileIndex, CELL_GCM_LOCATION_LOCAL, m_lmBlock.Offset(),
				m_layout->m_storageTotalSize, m_layout->DefaultPitch(), CELL_GCM_COMPMODE_DISABLED,
				m_lmBlock.TiledMemoryTagAreaBase(), // The area base + size/0x10000 will be allocated as the tag area.
				1 );	// Tile misc color buffers on bank 1
			cellGcmBindTile( uiTileIndex );
		}
	}

#ifdef _DEBUG
	memset( Data(), 0, m_layout->m_storageTotalSize );	// initialize texture data to BLACK in DEBUG
#endif

	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Buffer management
//

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

