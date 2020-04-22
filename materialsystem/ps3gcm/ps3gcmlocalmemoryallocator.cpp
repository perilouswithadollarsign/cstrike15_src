//========== Copyright © 2010, Valve Corporation, All rights reserved. ========

#include "dxabstract.h"
#include "ps3gcmstate.h"
#include "utlmap.h"
#include "ps3/ps3gcmlabels.h"
#include "sys/tty.h"
#include "convar.h"
//#include "vjobs/spudrawqueue_shared.h"
#include "spugcm.h"

#include "memdbgon.h"

PLATFORM_OVERRIDE_MEM_ALLOC_INTERNAL_PS3_IMPL

//////////////////////////////////////////////////////////////////////////

#if 1 // #ifndef _CERT
#define TRACK_ALLOC_STATS 1
#endif

#ifdef GCMLOCALMEMORYBLOCKDEBUG
ConVar r_ps3_gcmnocompact( "r_ps3_gcmnocompact", "0" );
ConVar r_ps3_gcmlowcompact( "r_ps3_gcmlowcompact", "0" );
#endif

static CThreadFastMutex s_AllocMutex;
static int32 s_uiGcmLocalMemoryAllocatorMutexLockCount;
struct CGcmLocalMemoryAllocatorMutexLockCounter_t
{
	CGcmLocalMemoryAllocatorMutexLockCounter_t() { Assert( s_uiGcmLocalMemoryAllocatorMutexLockCount >= 0 ); ++ s_uiGcmLocalMemoryAllocatorMutexLockCount; }
	~CGcmLocalMemoryAllocatorMutexLockCounter_t() { Assert( s_uiGcmLocalMemoryAllocatorMutexLockCount > 0 ); -- s_uiGcmLocalMemoryAllocatorMutexLockCount; }
};
#define PS3ALLOCMTX AUTO_LOCK( s_AllocMutex ); CGcmLocalMemoryAllocatorMutexLockCounter_t aLockCounter;
bool IsItSafeToRefreshFrontBufferNonInteractivePs3()
{
	// NOTE: only main thread can refresh front buffer
	if ( !ThreadInMainThread() )
		return false;
	
	AUTO_LOCK( s_AllocMutex );
	Assert( s_uiGcmLocalMemoryAllocatorMutexLockCount >= 0 );
	return s_uiGcmLocalMemoryAllocatorMutexLockCount <= 0;
}

struct CPs3gcmLocalMemoryBlockMutable : public CPs3gcmLocalMemoryBlock
{
	inline uint32 & MutableOffset() { return m_nLocalMemoryOffset; }
	inline uint32 & MutableSize() { return m_uiSize; }
	inline CPs3gcmAllocationType_t & MutableType() { return m_uType; }
	inline uint32 & MutableIndex() { return m_uiIndex; }
};

#ifdef GCMLOCALMEMORYBLOCKDEBUG
static const uint64 g_GcmLocalMemoryBlockDebugCookieAllocated = 0xA110CA7EDA110CA7ull;
static const uint64 g_GcmLocalMemoryBlockDebugCookieFree = 0xFEEFEEFEEFEEFEEFllu;
#endif

struct CPs3gcmLocalMemoryAllocator
{
	//////////////////////////////////////////////////////////////////////////
	//
	// Allocated memory tracking
	//
	uint32 m_nOffsetMin;	// RSX Local Memory allocated by Initialization that will never be released
	uint32 m_nOffsetMax;	// Ceiling of allocatable RSX Local Memory (because the top portion is reserved for zcull/etc.), top portion managed separately
	uint32 m_nOffsetUnallocated; // RSX Local Memory offset of not yet allocated memory (between Min and Max)

	CUtlVector< CPs3gcmLocalMemoryBlockMutable * > m_arrAllocations;	// Sorted array of all allocations

	//////////////////////////////////////////////////////////////////////////
	//
	// Free blocks tracking
	//
	struct LocalMemoryAllocation_t
	{
		CPs3gcmLocalMemoryBlockMutable m_block;
		uint32 m_uiFenceNumber;
		LocalMemoryAllocation_t *m_pNext;
	};

	LocalMemoryAllocation_t *m_pPendingFreeBlock;
	LocalMemoryAllocation_t *m_pFreeBlock;

	static uint32 sm_uiFenceNumber;
	uint32 m_uiFenceLastKnown;
	static uint32 volatile *sm_puiFenceLocation;

	//////////////////////////////////////////////////////////////////////////
	//
	// Implementation
	//
	inline bool Alloc( CPs3gcmLocalMemoryBlockMutable * RESTRICT pBlock );
	inline void Free( CPs3gcmLocalMemoryBlockMutable * RESTRICT pBlock );
	inline uint32 Reclaim( bool bForce = false );
	inline void Compact();

	// Helper methods
	inline LocalMemoryAllocation_t * FindFreeBlock( uint32 uiAlignBytes, uint32 uiSize );
	inline bool IsFenceCompleted( uint32 uiCurrentFenceValue, uint32 uiCheckStoredFenceValue );
	inline void TrackAllocStats( CPs3gcmAllocationType_t uAllocType, int nDelta );
#ifdef GCMLOCALMEMORYBLOCKDEBUG
	inline void ValidateAllBlocks();
#endif
}
g_ps3gcmLocalMemoryAllocator[kGcmAllocPoolCount];
uint32 CPs3gcmLocalMemoryAllocator::sm_uiFenceNumber;
uint32 volatile * CPs3gcmLocalMemoryAllocator::sm_puiFenceLocation;

// RSX memory usage stats tracking:
static GPUMemoryStats g_RsxMemoryStats;
struct GPUMemoryStats_Pool
{
	int nDefaultPoolSize;
	int nDefaultPoolUsed;
	int nRTPoolUsed;
	int nDynamicPoolUsed;
	int nMainMemUsed;
	int nUnknownPoolUsed;
};
GPUMemoryStats_Pool g_RsxMemoryStats_Pool;

static inline uint32 Ps3gcmHelper_ComputeTiledAreaMemorySize( uint32 nCount, uint32 w, uint32 h, uint32 bpp )
{
	uint32 nTilePitch = cellGcmGetTiledPitchSize( w * bpp );
	uint32 uiSize = nTilePitch * AlignValue( h, 32 );
	uiSize *= nCount;
	uiSize = AlignValue( uiSize, PS3GCMALLOCATIONALIGN( kAllocPs3gcmColorBufferMisc ) );
	return uiSize;
}

void Ps3gcmLocalMemoryAllocator_Init()
{
	PS3ALLOCMTX

	if ( !CPs3gcmLocalMemoryAllocator::sm_puiFenceLocation )
	{
		CPs3gcmLocalMemoryAllocator::sm_puiFenceLocation = cellGcmGetLabelAddress( GCM_LABEL_MEMORY_FREE );
		*CPs3gcmLocalMemoryAllocator::sm_puiFenceLocation = 0;
	}
	
	// Pool boundaries
	uint32 uiGcmAllocBegin = g_ps3gcmGlobalState.m_nLocalBaseOffset;
	uint32 uiGcmAllocEnd = uiGcmAllocBegin + g_ps3gcmGlobalState.m_nLocalSize;

	// Memory should be allocated for large frame buffers
	uint32 uiMemorySizeBuffer[2] = { MAX( 1280, g_ps3gcmGlobalState.m_nRenderSize[0] ), MAX( 720, g_ps3gcmGlobalState.m_nRenderSize[1] ) };
	uint32 uiFactor[2] = { uiMemorySizeBuffer[0]*uiMemorySizeBuffer[1], 1280*720 };

	// Configuration of pool memory (can be #ifdef'd for every game)
	static const uint32 s_PoolMemoryLayout[/*kGcmAllocPoolCount*/] =
	{
#if defined( CSTRIKE15 )
		// mhansen - We had to adjust the memory values a bit for cstrike15 to get a map to load
		// PS3_BUILDFIX - We need to revisit this to determine the proper size later on
		// mdonofrio - render target allocations revisited for PS3
		// potential to save some more (~12Mb) from TiledColourFB (only need two really.
		// wait for other rendering optimisation/rework to be finished first before attempting.
		
		/*kGcmAllocPoolDefault			= */	0,
		/*kGcmAllocPoolDynamicNewPath	= */	5 * 1024 * 1024,	// 5 MB
		/*kGcmAllocPoolDynamic			= */	11 * 1024 * 1024,	// 11 MB
		/*kGcmAllocPoolTiledColorFB		= */	Ps3gcmHelper_ComputeTiledAreaMemorySize( 2 + CPs3gcmDisplay::SURFACE_COUNT, uiMemorySizeBuffer[0], uiMemorySizeBuffer[1], 4 ),	// 3 buffers allocated in CreateRSXBuffers + 2 _rt_fullFrameFB - can probably get this down to 2 if we 1. don't use MLAA and 2. we clean up the post-pro rendering to use the front buffer as a textureand 3. tidy up aliasing for rt_fullframeFB and rt_fullFrameFB1
		/*kGcmAllocPoolTiledColorFBQ	= */	Ps3gcmHelper_ComputeTiledAreaMemorySize( 2, uiMemorySizeBuffer[0]/4, uiMemorySizeBuffer[1]/4, 4 ),	// fits 2 1/4 size framebuffer textures
		/*kGcmAllocPoolTiledColor512	= */	0,
		/*kGcmAllocPoolTiledColorMisc	= */	Ps3gcmHelper_ComputeTiledAreaMemorySize( 1, 640, 640, 4 ) + Ps3gcmHelper_ComputeTiledAreaMemorySize( 2, 1024, 512, 4) + Ps3gcmHelper_ComputeTiledAreaMemorySize(1, 32, 32, 4), // //  1x 1/2 size smoke/fog buffer, 2xWater(1024x512x32bpp), EyeGlint(32x32x32bpp), *Monitor(256x256x32bpp), *RTTFlashlightShadows(864x864x8bpp) - * we don't need these for CS15
		/*kGcmAllocPoolTiledD24S8		= */	Ps3gcmHelper_ComputeTiledAreaMemorySize( 2, uiMemorySizeBuffer[0], uiMemorySizeBuffer[1], 4 ), // only 2 depth buffer targets required (current and saved off), + reserve space for 1/2 size depth buffer for smoke/fog  
		/*kGcmAllocPoolMainMemory		= */	0,	// configured based on mapped IO memory
		/*kGcmAllocPoolMallocMemory		= */	0,	// using malloc
#else
		/*kGcmAllocPoolDefault			= */	0,
		/*kGcmAllocPoolDynamicNewPath	= */	5 * 1024 * 1024,	// 5 MB
		/*kGcmAllocPoolDynamic			= */	10 * 1024 * 1024,	// 10 MB
		/*kGcmAllocPoolTiledColorFB		= */	Ps3gcmHelper_ComputeTiledAreaMemorySize( 2 * CPs3gcmDisplay::SURFACE_COUNT, uiMemorySizeBuffer[0], uiMemorySizeBuffer[1], 4 ),	// fits 6	of full framebuffer textures
		/*kGcmAllocPoolTiledColorFBQ	= */	Ps3gcmHelper_ComputeTiledAreaMemorySize( 4, uiMemorySizeBuffer[0]/4, uiMemorySizeBuffer[1]/4, 4 ),	// fits 4	quarters of framebuffer textures
		/*kGcmAllocPoolTiledColor512	= */	Ps3gcmHelper_ComputeTiledAreaMemorySize( 2, 512, 512, 4 ),	// fits 2   512x512 RGBA textures
		/*kGcmAllocPoolTiledColorMisc	= */	5 * 1024 * 1024,	//  5 MB
		/*kGcmAllocPoolTiledD24S8		= */	uint64( 15 * 1024 * 1024 ) * uiFactor[0]/uiFactor[1],	// 15 MB
		/*kGcmAllocPoolMainMemory		= */	0,	// configured based on mapped IO memory
		/*kGcmAllocPoolMallocMemory		= */	0,	// using malloc
#endif
	};
	COMPILE_TIME_ASSERT( ARRAYSIZE( s_PoolMemoryLayout ) == ARRAYSIZE( g_ps3gcmLocalMemoryAllocator ) );

	for ( int j = ARRAYSIZE( g_ps3gcmLocalMemoryAllocator ); j -- > 0; )
	{
		const uint32 uiSize = AlignValue( s_PoolMemoryLayout[j], 1024 * 1024 ); // Align it on 1 MB boundaries, all our pools are large
		g_ps3gcmLocalMemoryAllocator[ j ].m_nOffsetMax = uiGcmAllocEnd;
		uiGcmAllocEnd -= uiSize;
		g_ps3gcmLocalMemoryAllocator[ j ].m_nOffsetMin =
			g_ps3gcmLocalMemoryAllocator[ j ].m_nOffsetUnallocated = uiGcmAllocEnd;
	}

	// Default pool setup (rest of local memory)
	g_ps3gcmLocalMemoryAllocator[ kGcmAllocPoolDefault ].m_nOffsetMax = uiGcmAllocEnd;
	g_ps3gcmLocalMemoryAllocator[ kGcmAllocPoolDefault ].m_nOffsetMin =
		g_ps3gcmLocalMemoryAllocator[ kGcmAllocPoolDefault ].m_nOffsetUnallocated = uiGcmAllocBegin;

	// Main memory mapped pool
	g_ps3gcmLocalMemoryAllocator[ kGcmAllocPoolMainMemory ].m_nOffsetMin =
		g_ps3gcmLocalMemoryAllocator[ kGcmAllocPoolMainMemory ].m_nOffsetUnallocated = uint32( g_ps3gcmGlobalState.m_pRsxMainMemoryPoolBuffer ) + g_ps3gcmGlobalState.m_nIoOffsetDelta;
	g_ps3gcmLocalMemoryAllocator[ kGcmAllocPoolMainMemory ].m_nOffsetMax = g_ps3gcmLocalMemoryAllocator[ kGcmAllocPoolMainMemory ].m_nOffsetMin + g_ps3gcmGlobalState.m_nRsxMainMemoryPoolBufferSize;

	// Store initial capacity for memory stats tracking:
	g_RsxMemoryStats.nGPUMemSize = g_ps3gcmGlobalState.m_nLocalSize;
	g_RsxMemoryStats_Pool.nDefaultPoolSize = g_ps3gcmLocalMemoryAllocator[ kGcmAllocPoolDefault ].m_nOffsetMax - g_ps3gcmLocalMemoryAllocator[ kGcmAllocPoolDefault ].m_nOffsetMin;

	//
	// Setup preset tiled regions
	//
	{
		CPs3gcmAllocationPool_t ePool = kGcmAllocPoolTiledColorFB;
		uint8 uiBank = 0;	// bank 0..3
		uint32 nRenderPitch = cellGcmGetTiledPitchSize( g_ps3gcmGlobalState.m_nRenderSize[0] * 4 );
		uint8 uiTileIndex = ePool - kGcmAllocPoolTiledColorFB;
		cellGcmSetTileInfo( uiTileIndex, CELL_GCM_LOCATION_LOCAL,
			g_ps3gcmLocalMemoryAllocator[ ePool ].m_nOffsetMin,
			g_ps3gcmLocalMemoryAllocator[ ePool ].m_nOffsetMax - g_ps3gcmLocalMemoryAllocator[ ePool ].m_nOffsetMin,
			nRenderPitch, CELL_GCM_COMPMODE_DISABLED,
			( g_ps3gcmLocalMemoryAllocator[ ePool ].m_nOffsetMin - g_ps3gcmLocalMemoryAllocator[ kGcmAllocPoolTiledColorFB ].m_nOffsetMin ) / 0x10000, // The area base + size/0x10000 will be allocated as the tag area.
			uiBank );
		cellGcmBindTile( uiTileIndex );
	}
	{
		CPs3gcmAllocationPool_t ePool = kGcmAllocPoolTiledColorFBQ;
		uint8 uiBank = 1;	// bank 0..3
		uint32 nRenderPitch = cellGcmGetTiledPitchSize( g_ps3gcmGlobalState.m_nRenderSize[0] * 4 / 4 );
		uint8 uiTileIndex = ePool - kGcmAllocPoolTiledColorFB;
		cellGcmSetTileInfo( uiTileIndex, CELL_GCM_LOCATION_LOCAL,
			g_ps3gcmLocalMemoryAllocator[ ePool ].m_nOffsetMin,
			g_ps3gcmLocalMemoryAllocator[ ePool ].m_nOffsetMax - g_ps3gcmLocalMemoryAllocator[ ePool ].m_nOffsetMin,
			nRenderPitch, CELL_GCM_COMPMODE_DISABLED,
			( g_ps3gcmLocalMemoryAllocator[ ePool ].m_nOffsetMin - g_ps3gcmLocalMemoryAllocator[ kGcmAllocPoolTiledColorFB ].m_nOffsetMin ) / 0x10000, // The area base + size/0x10000 will be allocated as the tag area.
			uiBank );
		cellGcmBindTile( uiTileIndex );
	}
	{
		CPs3gcmAllocationPool_t ePool = kGcmAllocPoolTiledColor512;
		uint8 uiBank = 2;	// bank 0..3
		uint32 nRenderPitch = cellGcmGetTiledPitchSize( 512 * 4 );
		uint8 uiTileIndex = ePool - kGcmAllocPoolTiledColorFB;
		cellGcmSetTileInfo( uiTileIndex, CELL_GCM_LOCATION_LOCAL,
			g_ps3gcmLocalMemoryAllocator[ ePool ].m_nOffsetMin,
			g_ps3gcmLocalMemoryAllocator[ ePool ].m_nOffsetMax - g_ps3gcmLocalMemoryAllocator[ ePool ].m_nOffsetMin,
			nRenderPitch, CELL_GCM_COMPMODE_DISABLED,
			( g_ps3gcmLocalMemoryAllocator[ ePool ].m_nOffsetMin - g_ps3gcmLocalMemoryAllocator[ kGcmAllocPoolTiledColorFB ].m_nOffsetMin ) / 0x10000, // The area base + size/0x10000 will be allocated as the tag area.
			uiBank );
		cellGcmBindTile( uiTileIndex );
	}

#ifndef _CERT
	static const char * s_PoolMemoryNames[] =
	{
		/*kGcmAllocPoolDefault			= */	"Default Pool",
		/*kGcmAllocPoolDynamicNewPath	= */	"Dynamic New ",
		/*kGcmAllocPoolDynamic			= */	"Dynamic IBVB",
		/*kGcmAllocPoolTiledColorFB		= */	"FullFrameRTs",
		/*kGcmAllocPoolTiledColorFBQ	= */	"1/4Frame RTs",
		/*kGcmAllocPoolTiledColor512	= */	"512x512 RTs ",
		/*kGcmAllocPoolTiledColorMisc	= */	"All Misc RTs",
		/*kGcmAllocPoolTiledD24S8		= */	"DepthStencil",
		/*kGcmAllocPoolMainMemory		= */	"Main Memory ",
		/*kGcmAllocPoolMallocMemory		= */	"MallocMemory",
	};
	COMPILE_TIME_ASSERT( ARRAYSIZE( s_PoolMemoryNames ) == ARRAYSIZE( g_ps3gcmLocalMemoryAllocator ) );

	Msg( "RSX Local Memory layout:\n" );
	for ( int j = 0; j < ARRAYSIZE( g_ps3gcmLocalMemoryAllocator ); ++ j )
	{
		Msg( "    %s    0x%08X - 0x%08X   [ %9.3f MB ]\n",
			s_PoolMemoryNames[j],
			g_ps3gcmLocalMemoryAllocator[ j ].m_nOffsetMin,
			g_ps3gcmLocalMemoryAllocator[ j ].m_nOffsetMax,
			(g_ps3gcmLocalMemoryAllocator[ j ].m_nOffsetMax - g_ps3gcmLocalMemoryAllocator[ j ].m_nOffsetMin) / 1024.f / 1024.f );
	}
	Msg( "Total size: %d MB\n", g_ps3gcmGlobalState.m_nLocalSize / 1024 / 1024 );
#endif
}

void Ps3gcmLocalMemoryAllocator_Reclaim()
{
	PS3ALLOCMTX
	for ( int k = 0; k < ARRAYSIZE( g_ps3gcmLocalMemoryAllocator ); ++ k )
		g_ps3gcmLocalMemoryAllocator[ k ].Reclaim();
}

void Ps3gcmLocalMemoryAllocator_Compact()
{
#define PS3GCMCOMPACTPROFILE 0
#if PS3GCMCOMPACTPROFILE
	float flTimeStart = Plat_FloatTime();
	uint32 uiFree = g_ps3gcmLocalMemoryAllocator[0].m_nOffsetUnallocated;
#endif

	// Let RSX wait for final flip
	GCM_FUNC( cellGcmSetWaitFlip );

	// Let PPU wait for all RSX commands done (include waitFlip)
	g_ps3gcmGlobalState.CmdBufferFinish();

#if PS3GCMCOMPACTPROFILE
	float flTimeWait = Plat_FloatTime() - flTimeStart;
#endif

	{
		PS3ALLOCMTX
		for ( int k = 0; k < ARRAYSIZE( g_ps3gcmLocalMemoryAllocator ); ++ k )
		{
			g_ps3gcmLocalMemoryAllocator[ k ].Compact();
		}
	}
	
#if PS3GCMCOMPACTPROFILE
	float flTimePrepareTransfer = Plat_FloatTime() - flTimeStart;
#endif

	// Wait for all RSX memory to be transferred
	g_ps3gcmGlobalState.CmdBufferFinish();

#if PS3GCMCOMPACTPROFILE
	float flTimeDone = Plat_FloatTime() - flTimeStart;
	char chBuffer[64];
	Q_snprintf( chBuffer, ARRAYSIZE( chBuffer ), "COMPACT: %0.3f / %0.3f / %0.3f sec\n",
		flTimeWait, flTimePrepareTransfer, flTimeDone );
	uint32 dummy;
	sys_tty_write( SYS_TTYP6, chBuffer, Q_strlen( chBuffer ), &dummy );
	Q_snprintf( chBuffer, ARRAYSIZE( chBuffer ), "COMPACT: %0.3f -> %0.3f MB (%0.3f MB free)\n",
		uiFree / 1024.f / 1024.f, g_ps3gcmLocalMemoryAllocator[0].m_nOffsetUnallocated / 1024.f / 1024.f,
		(g_ps3gcmLocalMemoryAllocator[0].m_nOffsetMax - g_ps3gcmLocalMemoryAllocator[0].m_nOffsetUnallocated) / 1024.f / 1024.f );
	sys_tty_write( SYS_TTYP6, chBuffer, Q_strlen( chBuffer ), &dummy );
#endif
}

void Ps3gcmLocalMemoryAllocator_CompactWithReason( char const *szReason )
{
	double flTimeCompactStart = Plat_FloatTime();
	DevMsg( "====== GCM LOCAL MEMORY COMPACT : %s =====\n", szReason );
	uint32 uiFreeMemoryBeforeCompact = g_ps3gcmLocalMemoryAllocator[0].m_nOffsetUnallocated;
	DevMsg( "RSX Local Memory Free:  %0.3f MB; compacting...\n", (g_ps3gcmLocalMemoryAllocator[0].m_nOffsetMax - g_ps3gcmLocalMemoryAllocator[0].m_nOffsetUnallocated) / 1024.f / 1024.f );

	Ps3gcmLocalMemoryAllocator_Compact();

	DevMsg( "RSX Local Memory Compacted %0.3f MB in %0.3f sec\n",
		(uiFreeMemoryBeforeCompact - g_ps3gcmLocalMemoryAllocator[0].m_nOffsetUnallocated) / 1024.f / 1024.f,
		Plat_FloatTime() - flTimeCompactStart );
	DevMsg( "RSX Local Memory Free:  %0.3f MB\n", (g_ps3gcmLocalMemoryAllocator[0].m_nOffsetMax - g_ps3gcmLocalMemoryAllocator[0].m_nOffsetUnallocated) / 1024.f / 1024.f );
}


bool CPs3gcmLocalMemoryBlock::Alloc()
{
	PS3ALLOCMTX
	return g_ps3gcmLocalMemoryAllocator[PS3GCMALLOCATIONPOOL(m_uType)].Alloc( reinterpret_cast< CPs3gcmLocalMemoryBlockMutable * >( this ) );
}

void CPs3gcmLocalMemoryBlock::Free()
{
	PS3ALLOCMTX
	g_ps3gcmLocalMemoryAllocator[PS3GCMALLOCATIONPOOL(m_uType)].Free( reinterpret_cast< CPs3gcmLocalMemoryBlockMutable * >( this ) );
}


//////////////////////////////////////////////////////////////////////////
//
// Private implementation of PS3 local memory allocator
//

inline bool CPs3gcmLocalMemoryAllocator::Alloc( CPs3gcmLocalMemoryBlockMutable * RESTRICT pBlock )
{
	TrackAllocStats( pBlock->MutableType(), pBlock->MutableSize() );

	uint32 uAlignBytes = PS3GCMALLOCATIONALIGN( pBlock->MutableType() );
	Assert( IsPowerOfTwo( uAlignBytes ) );

	double flAllocatorStallTime = 0.0f;
	bool bCompactPerformed = true;

#ifdef GCMLOCALMEMORYBLOCKDEBUG
	bCompactPerformed = !r_ps3_gcmlowcompact.GetBool();
#endif

retry_allocation:
	// Try to find a free block
	if ( LocalMemoryAllocation_t *pFreeBlock = FindFreeBlock( uAlignBytes, pBlock->MutableSize() ) )
	{
		pBlock->MutableOffset() = pFreeBlock->m_block.MutableOffset();
		pBlock->MutableIndex() = pFreeBlock->m_block.MutableIndex();
#ifdef GCMLOCALMEMORYBLOCKDEBUG
		if ( m_arrAllocations[ pBlock->MutableIndex() ] != &pFreeBlock->m_block )
			Error( "<vitaliy> GCM Local Memory Allocator Error (attempt to reuse invalid free block)!" );
#endif
		m_arrAllocations[ pBlock->MutableIndex() ] = reinterpret_cast< CPs3gcmLocalMemoryBlockMutable * >( pBlock );
		delete pFreeBlock;
	}
	else if ( this != &g_ps3gcmLocalMemoryAllocator[ kGcmAllocPoolMallocMemory ] )
	{
		// Allocate new block
		uint32 uiOldUnallocatedEdge = m_nOffsetUnallocated;
		uint32 uiFreeBlock = ( m_nOffsetUnallocated + uAlignBytes - 1 ) & ~( uAlignBytes - 1 );

		// Check if there's enough space in this pool for the requested block
		if ( uiFreeBlock + pBlock->MutableSize() > m_nOffsetMax )
		{
			// There's not enough space in this pool
			if ( m_pPendingFreeBlock )
			{
				// There are pending free blocks, we just need to wait for
				// RSX to finish rendering using them
				if ( !flAllocatorStallTime )
				{
					flAllocatorStallTime = Plat_FloatTime();
					g_ps3gcmGlobalState.CmdBufferFlush( CPs3gcmGlobalState::kFlushForcefully );
				}
				while ( Reclaim() < pBlock->MutableSize() && m_pPendingFreeBlock )
				{
					ThreadSleep( 1 );
				}
				goto retry_allocation;
			}
			else if ( !bCompactPerformed )
			{
				// Let PPU wait for all RSX commands done
				g_ps3gcmGlobalState.CmdBufferFinish();

				uint32 uiFragmentedFreeSpace = m_nOffsetMax - m_nOffsetUnallocated;
				for ( LocalMemoryAllocation_t *pFreeFragment = m_pFreeBlock; pFreeFragment; pFreeFragment = pFreeFragment->m_pNext )
					uiFragmentedFreeSpace += pFreeFragment->m_block.MutableSize();
				Warning(
					"**************** GCM LOCAL MEMORY LOW *****************\n"
					"<vitaliy> GCM Local Memory Allocator#%d pool compacting!\n"
					"  Requested allocation %u bytes.\n"
					"  Pool capacity %u bytes.\n"
					"  Free fragmented space %u bytes.\n"
					"  Unallocated %u bytes.\n"
					"  Used %u bytes.\n",
					this - g_ps3gcmLocalMemoryAllocator,
					( uint32 ) pBlock->MutableSize(),
					m_nOffsetMax - m_nOffsetMin,
					uiFragmentedFreeSpace,
					m_nOffsetMax - m_nOffsetUnallocated,
					m_nOffsetUnallocated - m_nOffsetMin
					);
				Compact();
				Warning( "  ---> Compacted pool#%d has %u unallocated bytes.\n",
					this - g_ps3gcmLocalMemoryAllocator,
					m_nOffsetMax - m_nOffsetUnallocated );
				bCompactPerformed = true;

				// Wait for all RSX memory to be transferred
				g_ps3gcmGlobalState.CmdBufferFinish();
				goto retry_allocation;
			}
			else
			{
				// Main memory pool returns failure so caller can try local pool.

				if (this == &g_ps3gcmLocalMemoryAllocator[ kGcmAllocPoolMainMemory ]) return false;

				uint32 uiFragmentedFreeSpace = m_nOffsetMax - m_nOffsetUnallocated;
				for ( LocalMemoryAllocation_t *pFreeFragment = m_pFreeBlock; pFreeFragment; pFreeFragment = pFreeFragment->m_pNext )
					uiFragmentedFreeSpace += pFreeFragment->m_block.MutableSize();
				Error(
					"********* OUT OF GCM LOCAL MEMORY ********************\n"
					"<vitaliy> GCM Local Memory Allocator#%d pool exhausted!\n"
					"  Failed allocation %u bytes.\n"
					"  Pool capacity %u bytes.\n"
					"  Free fragmented space %u bytes.\n"
					"  Unallocated %u bytes.\n"
					"  Used %u bytes.\n",
					this - g_ps3gcmLocalMemoryAllocator,
					( uint32 ) pBlock->MutableSize(),
					m_nOffsetMax - m_nOffsetMin,
					uiFragmentedFreeSpace,
					m_nOffsetMax - m_nOffsetUnallocated,
					m_nOffsetUnallocated - m_nOffsetMin
					);
			}
		}

		// update the pointer to "unallocated" realm
		m_nOffsetUnallocated = uiFreeBlock + pBlock->MutableSize();

		// this is the last allocation so far
		pBlock->MutableIndex() = m_arrAllocations.AddToTail( reinterpret_cast< CPs3gcmLocalMemoryBlockMutable * >( pBlock ) );
		pBlock->MutableOffset() = uiFreeBlock;
	}
	else
	{
		MEM_ALLOC_CREDIT_( "GCM Malloc Pool" );
		void *pvMallocMemory = MemAlloc_AllocAligned( pBlock->MutableSize(), uAlignBytes );
		pBlock->MutableOffset() = (uint32) pvMallocMemory;
		pBlock->MutableIndex() = ~0;
	}

	if ( flAllocatorStallTime )
		g_ps3gcmGlobalState.m_flAllocatorStallTimeWaitingRSX += Plat_FloatTime() - flAllocatorStallTime;

#ifdef GCMLOCALMEMORYBLOCKDEBUG
	// PS3 doesn't allow more than 8 zcull regions (index 0..7)
	if ( g_ps3gcmLocalMemoryAllocator[kGcmAllocPoolTiledD24S8].m_arrAllocations.Count() > 8 )
		Error( "PS3 number of zcull regions exceeded!\n" );
	// PS3 doesn't allow more than 15 tiles regions (index 0..14)
	if ( g_ps3gcmLocalMemoryAllocator[kGcmAllocPoolTiledD24S8].m_arrAllocations.Count() +
			g_ps3gcmLocalMemoryAllocator[kGcmAllocPoolTiledColorMisc].m_arrAllocations.Count() +
			( kGcmAllocPoolTiledColorMisc - kGcmAllocPoolTiledColorFB )
			> 15 )
		Error( "PS3 number of tiled regions exceeded!\n" );
	pBlock->m_dbgGuardCookie = g_GcmLocalMemoryBlockDebugCookieAllocated;
#endif

	return true;

}

inline void CPs3gcmLocalMemoryAllocator::Free( CPs3gcmLocalMemoryBlockMutable * RESTRICT pBlock )
{
#ifdef GCMLOCALMEMORYBLOCKDEBUG
	if ( !pBlock ||
		pBlock->m_dbgGuardCookie != g_GcmLocalMemoryBlockDebugCookieAllocated ||
		( ( pBlock->MutableIndex() != ~0 ) && ( m_arrAllocations[ pBlock->MutableIndex() ] != pBlock ) ) )
		{
			//DebuggerBreak();
			Error( "<vitaliy> Attempt to free not allocated GCM local memory block!" );
		}
	pBlock->m_dbgGuardCookie = g_GcmLocalMemoryBlockDebugCookieFree;
#endif

	LocalMemoryAllocation_t *pDealloc = new LocalMemoryAllocation_t;
	pDealloc->m_block = *pBlock;
	pDealloc->m_uiFenceNumber = ++ sm_uiFenceNumber;
	pDealloc->m_pNext = m_pPendingFreeBlock;
	GCM_FUNC( cellGcmSetWriteBackEndLabel, GCM_LABEL_MEMORY_FREE, sm_uiFenceNumber );
	m_pPendingFreeBlock = pDealloc;

	TrackAllocStats( pBlock->MutableType(), - pBlock->MutableSize() );
	if ( pBlock->MutableIndex() != ~0 )
	{
		#ifdef GCMLOCALMEMORYBLOCKDEBUG
		if ( m_arrAllocations[ pBlock->MutableIndex() ] != pBlock )
			Error( "<vitaliy> GCM Local Memory Allocator Error (freeing block that is not properly registered)!" );
		#endif
		m_arrAllocations[ pBlock->MutableIndex() ] = &pDealloc->m_block;
	}

#ifdef GCMLOCALMEMORYBLOCKDEBUG
	pBlock->MutableOffset() = ~0;
	pBlock->MutableIndex() = ~0;
#endif
}

inline bool CPs3gcmLocalMemoryAllocator::IsFenceCompleted( uint32 uiCurrentFenceValue, uint32 uiCheckStoredFenceValue )
{
#if GCM_ALLOW_NULL_FLIPS
	extern bool g_ps3_nullflips;
	if ( g_ps3_nullflips )
		return true;
#endif
	// Needs to handle the counter wrapping around
	return ( ( uiCurrentFenceValue - m_uiFenceLastKnown ) >= ( uiCheckStoredFenceValue - m_uiFenceLastKnown ) );
}

inline uint32 CPs3gcmLocalMemoryAllocator::Reclaim( bool bForce )
{
	uint32 uiLargestBlockSizeReclaimed = 0;
	uint32 uiCurrentFenceValue = *sm_puiFenceLocation;

	// Walk pending free blocks and see if they are no longer
	// in use by RSX:
	LocalMemoryAllocation_t **p = &m_pPendingFreeBlock;
	if ( !bForce ) while ( (*p) && !IsFenceCompleted( uiCurrentFenceValue, (*p)->m_uiFenceNumber ) )
		p = &( (*p)->m_pNext );

	// Now p is pointing to the chain of free blocks
	// chain that has been completed (due to the nature of
	// pushing new deallocation at the head of the pending
	// list)
	if ( *p )
	{
		LocalMemoryAllocation_t *pCompletedChain = *p;
		*p = NULL;	// Terminate the chain

		// Handle the special case of malloc reclaim - free all memory
		if ( this == &g_ps3gcmLocalMemoryAllocator[ kGcmAllocPoolMallocMemory ] )
		{
			MEM_ALLOC_CREDIT_( "GCM Malloc Pool" );
			for ( LocalMemoryAllocation_t *pActualFree = pCompletedChain; pActualFree; )
			{
				MemAlloc_FreeAligned( pActualFree->m_block.DataInMallocMemory() );
				LocalMemoryAllocation_t *pDelete = pActualFree;
				pActualFree = pActualFree->m_pNext;
				delete pDelete;
			}
			pCompletedChain = NULL;
		}

		// Relink the completed pending chain into
		// the free blocks chain
		LocalMemoryAllocation_t **ppFree = &m_pFreeBlock;
		while ( *ppFree )
			ppFree = &( (*ppFree)->m_pNext );
		*ppFree = pCompletedChain;

		// Recompute actual free sizes of the completed chain
		// Actual free size is the delta between block offset and next block offset
		// When there's no next block then its delta between block offset and unallocated edge
		for ( LocalMemoryAllocation_t *pActualFree = pCompletedChain; pActualFree; pActualFree = pActualFree->m_pNext )
		{
			uint32 uiIdx = pActualFree->m_block.MutableIndex() + 1;
			uint32 uiNextOffset = m_nOffsetUnallocated;
			if ( uiIdx < m_arrAllocations.Count() )
			{
				CPs3gcmLocalMemoryBlockMutable * RESTRICT pNextBlock = m_arrAllocations[ uiIdx ];
				uiNextOffset = pNextBlock->Offset();
			}
			uint32 uiActualBlockSize = uiNextOffset - pActualFree->m_block.Offset();
			pActualFree->m_block.MutableSize() = uiActualBlockSize;
			uiLargestBlockSizeReclaimed = MAX( uiLargestBlockSizeReclaimed, uiActualBlockSize );
		}
	}

	// Remember the last known fence value
	m_uiFenceLastKnown = uiCurrentFenceValue;

#ifdef GCMLOCALMEMORYBLOCKDEBUG
	ValidateAllBlocks();
#endif

	return uiLargestBlockSizeReclaimed;
}

inline CPs3gcmLocalMemoryAllocator::LocalMemoryAllocation_t * CPs3gcmLocalMemoryAllocator::FindFreeBlock( uint32 uiAlignBytes, uint32 uiSize )
{
	LocalMemoryAllocation_t **ppBest = NULL;
	uint32 uiSizeMax = uiSize * 11/10;	// we don't want to inflate requested size by > 10%
	for ( LocalMemoryAllocation_t **p = &m_pFreeBlock;
		(*p);
		p = &( (*p)->m_pNext ) )
	{
		if ( (*p)->m_block.MutableSize() >= uiSize && (*p)->m_block.MutableSize() <= uiSizeMax &&
			!( (*p)->m_block.Offset() & ( uiAlignBytes - 1 ) ) )
		{
			if ( !ppBest || ( (*p)->m_block.MutableSize() <= (*ppBest)->m_block.MutableSize() ) )
			{
				ppBest = p;
			}
		}
	}
	if ( ppBest )
	{
		LocalMemoryAllocation_t *pFree = (*ppBest);
		(*ppBest) = pFree->m_pNext;
		pFree->m_pNext = NULL;
		return pFree;
	}
	return NULL;
}

inline bool TrackAllocStats_Pool( CPs3gcmAllocationType_t uAllocType, int nDelta )
{
	CPs3gcmAllocationPool_t pool = PS3GCMALLOCATIONPOOL( uAllocType );
	int *stat = &g_RsxMemoryStats_Pool.nUnknownPoolUsed;
	bool bInRSXMem = true;
	switch( pool )
	{
	case kGcmAllocPoolDefault:
		stat = &g_RsxMemoryStats_Pool.nDefaultPoolUsed;
		break;
	case kGcmAllocPoolDynamicNewPath:
	case kGcmAllocPoolDynamic:
		stat = &g_RsxMemoryStats_Pool.nDynamicPoolUsed;
		break;
	case kGcmAllocPoolTiledColorFB:
	case kGcmAllocPoolTiledColorFBQ:
	case kGcmAllocPoolTiledColor512:
	case kGcmAllocPoolTiledColorMisc:
	case kGcmAllocPoolTiledD24S8:
		stat = &g_RsxMemoryStats_Pool.nRTPoolUsed;
		break;
	case kGcmAllocPoolMainMemory: // Unused, unless PS3GCM_VBIB_IN_IO_MEMORY set to 1
	case kGcmAllocPoolMallocMemory:
		stat = &g_RsxMemoryStats_Pool.nMainMemUsed;
		bInRSXMem = false; // In main memory!
		break;
	}
	*stat += nDelta;
	Assert( 0 <= (int)*stat );

	// Report free memory only from the default pool (the other pools are pre-sized to fixed limits, and all
	// geom/textures go into the default pool, so that's where content-driven variation/failures will occur)
	g_RsxMemoryStats.nGPUMemFree = g_RsxMemoryStats_Pool.nDefaultPoolSize - g_RsxMemoryStats_Pool.nDefaultPoolUsed;

	return bInRSXMem;
}

inline void CPs3gcmLocalMemoryAllocator::TrackAllocStats( CPs3gcmAllocationType_t uAllocType, int nDelta )
{
#if TRACK_ALLOC_STATS
	// Early-out for allocations not in RSX memory:
	if ( !TrackAllocStats_Pool( uAllocType, nDelta ) )
		return;

	unsigned int *stat = &g_RsxMemoryStats.nUnknown;
	switch( uAllocType )
	{
	case kAllocPs3gcmColorBufferMisc:
	case kAllocPs3gcmColorBufferFB:
	case kAllocPs3gcmColorBufferFBQ:
	case kAllocPs3gcmColorBuffer512:
	case kAllocPs3gcmDepthBuffer:
		stat = &g_RsxMemoryStats.nRTSize;
		break;
	case kAllocPs3gcmTextureData:
	case kAllocPs3gcmTextureData0:
		stat = &g_RsxMemoryStats.nTextureSize;
		break;
	case kAllocPs3GcmVertexBuffer:
		stat = &g_RsxMemoryStats.nVBSize;
		break;
	case kAllocPs3GcmIndexBuffer:
		stat = &g_RsxMemoryStats.nIBSize;
		break;

	case kAllocPs3GcmShader:
	case kAllocPs3GcmEdgeGeomBuffer:
	case kAllocPs3GcmVertexBufferDynamic:
	case kAllocPs3GcmIndexBufferDynamic:
	case kAllocPs3GcmDynamicBufferPool:
	case kAllocPs3GcmVertexBufferDma:
	case kAllocPs3GcmIndexBufferDma:
		// Treat these as misc unless they become big/variable
		break;
	}
	*stat += nDelta;
	Assert( 0 <= (int)*stat );
#endif // TRACK_ALLOC_STATS
}

#ifdef GCMLOCALMEMORYBLOCKDEBUG
#define VALIDATECONDITION( x ) if( !( x ) ) { Error( "<vitaliy> GCM Local Memory Allocation block %p index %d is corrupt [line %d]!\n", pBlock, k, __LINE__ ); }
inline void CPs3gcmLocalMemoryAllocator::ValidateAllBlocks()
{
	// Traverse the allocated list and validate debug guards and patch-back indices
	CUtlVector< uint32 > arrFreeBlocksIdx;
	uint32 uiLastAllocatedOffset = m_nOffsetMin;
	for ( int k = 0, kEnd = m_arrAllocations.Count(); k < kEnd; ++ k )
	{
		CPs3gcmLocalMemoryBlockMutable * RESTRICT pBlock = m_arrAllocations[k];
		VALIDATECONDITION( pBlock );
		VALIDATECONDITION( pBlock->m_dbgGuardCookie == g_GcmLocalMemoryBlockDebugCookieAllocated || pBlock->m_dbgGuardCookie == g_GcmLocalMemoryBlockDebugCookieFree );
		VALIDATECONDITION( pBlock->MutableIndex() < m_arrAllocations.Count() );
		VALIDATECONDITION( pBlock->MutableIndex() == k );
		VALIDATECONDITION( m_arrAllocations[ pBlock->MutableIndex() ] == pBlock );
		VALIDATECONDITION( pBlock->Offset() >= uiLastAllocatedOffset );
		uiLastAllocatedOffset = pBlock->Offset() + pBlock->MutableSize();
		VALIDATECONDITION( uiLastAllocatedOffset <= m_nOffsetMax );
		if ( pBlock->m_dbgGuardCookie == g_GcmLocalMemoryBlockDebugCookieFree )
			arrFreeBlocksIdx.AddToTail( k );
	}
	
	// Traverse free lists and validate
	LocalMemoryAllocation_t * arrFree[] = { m_pPendingFreeBlock, m_pFreeBlock };
	for ( int j = 0; j < ARRAYSIZE( arrFree ); ++ j )
	for ( LocalMemoryAllocation_t *p = arrFree[j]; p; p = p->m_pNext )
	{
		int k = j;
		CPs3gcmLocalMemoryBlockMutable * RESTRICT pBlock = &p->m_block;
		VALIDATECONDITION( pBlock );
		VALIDATECONDITION( pBlock->m_dbgGuardCookie == g_GcmLocalMemoryBlockDebugCookieFree );
		k = pBlock->MutableIndex();
		if ( pBlock->MutableIndex() != ~0 )
		{
			VALIDATECONDITION( pBlock->MutableIndex() < m_arrAllocations.Count() );
			VALIDATECONDITION( m_arrAllocations[ pBlock->MutableIndex() ] == pBlock );
			VALIDATECONDITION( arrFreeBlocksIdx.FindAndFastRemove( pBlock->MutableIndex() ) );
		}
	}

	int k = 0;
	void *pBlock = 0;
	VALIDATECONDITION( !arrFreeBlocksIdx.Count() );
}
#endif

inline void CPs3gcmLocalMemoryAllocator::Compact()
{
	GCM_PERF_PUSH_MARKER( "LocalMemory:Compact" );

#ifdef GCMLOCALMEMORYBLOCKDEBUG
	ValidateAllBlocks();

	if ( r_ps3_gcmnocompact.GetBool() )
		return;
#endif

	// Reclaim all memory (NOTE: all pending blocks must be reclaimed since both RSX and PPU have stopped rendering!)
	Reclaim();

#ifdef GCMLOCALMEMORYBLOCKDEBUG
	if ( m_pPendingFreeBlock )
		Warning( "GCM Local Memory Allocator Compact forces pending free blocks to be reclaimed.\n" );
	ValidateAllBlocks();
#endif

	if ( m_pPendingFreeBlock )
		Reclaim( true );

#ifdef GCMLOCALMEMORYBLOCKDEBUG
	if ( m_pPendingFreeBlock )
		Error( "<vitaliy> GCM Local Memory Allocator Compact requires RSX and PPU rendering to be paused! (pending free blocks have not been reclaimed)\n" );
	ValidateAllBlocks();
#endif

	// Walk the free blocks chain and patch-back NULL pointers into allocation tracking system
	while ( m_pFreeBlock )
	{
		LocalMemoryAllocation_t *p = m_pFreeBlock;
		m_pFreeBlock = p->m_pNext;
		m_arrAllocations[ p->m_block.MutableIndex() ] = NULL;
		delete p;
	}
	Assert( !m_pFreeBlock && !m_pPendingFreeBlock );

	// These are elements requiring reallocation
	uint32 uiCount = m_arrAllocations.Count();
	CPs3gcmLocalMemoryBlockMutable **pReallocationBlocks = m_arrAllocations.Base();

	// Here "correct" implementation would be to copy off m_arrAllocations vector onto stack for iteration,
	// RemoveAll from m_arrAllocations vector and allocate all blocks again.
	// We will cheat since we know that we will allocate same number of elements and directly write zero
	// into m_arrAllocations m_Size member, then we will still be able to use the memory of the vector
	// for reading blocks requiring compact reallocation, and AddToTail will still fill the vector with
	// correct data.
	struct AllocatorCompactVectorCheat : public CUtlVector< CPs3gcmLocalMemoryBlockMutable * > { inline void ResetCountPreservingMemoryContents() { m_Size = 0; } };
	( ( AllocatorCompactVectorCheat * ) ( char * ) &m_arrAllocations )->ResetCountPreservingMemoryContents();
	m_nOffsetUnallocated = m_nOffsetMin;

	// Prepare RSX for data buffer transfers in local memory
	uint nTransferMode = ( ( this - &g_ps3gcmLocalMemoryAllocator[ kGcmAllocPoolDefault ] ) < kGcmAllocPoolMainMemory ) ? CELL_GCM_TRANSFER_LOCAL_TO_LOCAL : CELL_GCM_TRANSFER_MAIN_TO_MAIN;
	Assert( nTransferMode < 4 );
	GCM_FUNC( cellGcmSetTransferDataMode, nTransferMode ); // unnecessary if we do this on SPU

	Assert( !g_spuGcm.IsDeferredDrawQueue() );

	// Reallocate all blocks
	for ( ; uiCount; -- uiCount, ++ pReallocationBlocks )
	{
		CPs3gcmLocalMemoryBlockMutable *pBlock = *pReallocationBlocks;
		if ( !pBlock )
			continue;

		uint32 nOldOffset = pBlock->Offset();

		TrackAllocStats( pBlock->MutableType(), - pBlock->MutableSize() );
		Alloc( pBlock );

		if ( nOldOffset == pBlock->Offset() )
			continue;

		// Have RSX transfer blocks data. RSX may hang if there's WriteLabel between the Format and Offset commands,
		// so reserve space for both of them up front
		SpuDrawTransfer_t * pTransfer = g_spuGcm.GetDrawQueue()->AllocWithHeader<SpuDrawTransfer_t>( SPUDRAWQUEUE_TRANSFER_METHOD | nTransferMode );
		pTransfer->m_nLineSize = pBlock->MutableSize();
		pTransfer->m_nOldOffset = nOldOffset;
		pTransfer->m_nNewOffset = pBlock->Offset();
	}

#ifdef GCMLOCALMEMORYBLOCKDEBUG
	ValidateAllBlocks();
#endif
	GCM_PERF_MARKER( "Compact:Complete" );
}

//////////////////////////////////////////////////////////////////////////
//
// Computation of tiled memory
//

uint32 CPs3gcmLocalMemoryBlock::TiledMemoryTagAreaBase() const
{
	CPs3gcmAllocationPool_t ePool = PS3GCMALLOCATIONPOOL(m_uType);
	if ( ePool == kGcmAllocPoolTiledColorMisc )	// Misc color tiles are placed at the front of tag area after preset pools
		return ( Offset() - g_ps3gcmLocalMemoryAllocator[kGcmAllocPoolTiledColorFB].m_nOffsetMin ) / 0x10000;
	if ( ePool == kGcmAllocPoolTiledD24S8 )	// Depth tiles are placed in the end of tag area (0-0x7FF is offset range)
		return 0x800 - ( Offset() - g_ps3gcmLocalMemoryAllocator[kGcmAllocPoolTiledD24S8].m_nOffsetMin + m_uiSize ) / 0x10000;
	if ( ePool == kGcmAllocPoolTiledColorFB )	// FB color tiles go first
		return ( g_ps3gcmLocalMemoryAllocator[kGcmAllocPoolTiledColorFB].m_nOffsetMin - g_ps3gcmLocalMemoryAllocator[kGcmAllocPoolTiledColorFB].m_nOffsetMin ) / 0x10000;
	if ( ePool == kGcmAllocPoolTiledColorFBQ )	// FBQ color tiles go next
		return ( g_ps3gcmLocalMemoryAllocator[kGcmAllocPoolTiledColorFBQ].m_nOffsetMin - g_ps3gcmLocalMemoryAllocator[kGcmAllocPoolTiledColorFB].m_nOffsetMin ) / 0x10000;
	if ( ePool == kGcmAllocPoolTiledColor512 )	// 512 color tiles go next
		return ( g_ps3gcmLocalMemoryAllocator[kGcmAllocPoolTiledColor512].m_nOffsetMin - g_ps3gcmLocalMemoryAllocator[kGcmAllocPoolTiledColorFB].m_nOffsetMin ) / 0x10000;

#ifdef GCMLOCALMEMORYBLOCKDEBUG
	Error( "<vitaliy> Cannot compute tiled memory tag base from a non-tiled-pool allocation!\n" );
#endif
	return ~0;
}

uint32 CPs3gcmLocalMemoryBlock::TiledMemoryIndex() const
{
	CPs3gcmAllocationPool_t ePool = PS3GCMALLOCATIONPOOL(m_uType);
	if ( ePool == kGcmAllocPoolTiledColorMisc )	// Color tiles are placed in the front
		return m_uiIndex + kGcmAllocPoolTiledColorMisc - kGcmAllocPoolTiledColorFB;
	if ( ePool == kGcmAllocPoolTiledD24S8 )	// Depth tiles are placed as last tiles
		return 14 - m_uiIndex;
	return ePool - kGcmAllocPoolTiledColorFB;
}

uint32 CPs3gcmLocalMemoryBlock::ZcullMemoryIndex() const
{
	CPs3gcmAllocationPool_t ePool = PS3GCMALLOCATIONPOOL(m_uType);
	if ( ePool == kGcmAllocPoolTiledD24S8 )	// Depth tiles are the only zcull tiles
		return m_uiIndex;

#ifdef GCMLOCALMEMORYBLOCKDEBUG
	Error( "<vitaliy> Cannot compute zcull index from a non-zcull allocation!\n" );
#endif
	return ~0;
}

uint32 CPs3gcmLocalMemoryBlock::ZcullMemoryStart() const
{
	CPs3gcmAllocationPool_t ePool = PS3GCMALLOCATIONPOOL(m_uType);
	if ( ePool == kGcmAllocPoolTiledD24S8 )	// Depth tiles are the only zcull tiles
		return ( Offset() - g_ps3gcmLocalMemoryAllocator[kGcmAllocPoolTiledD24S8].m_nOffsetMin ) / 4; // 1 byte per pixel, D24S8 is 4 bytes per pixel, implicitly 4096 aligned because offset is 64Kb aligned

#ifdef GCMLOCALMEMORYBLOCKDEBUG
	Error( "<vitaliy> Cannot compute zcull memory start from a non-zcull allocation!\n" );
#endif
	return ~0;
}


//////////////////////////////////////////////////////////////////////////
//
// Allow shaderapi to query GPU memory stats:
//

void GetGPUMemoryStats( GPUMemoryStats &stats )
{
	stats = g_RsxMemoryStats;
}
