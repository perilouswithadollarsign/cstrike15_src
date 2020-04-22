//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
// Local memory manager
//
//==================================================================================================

#ifndef INCLUDED_PS3GCMMEMORY_H
#define INCLUDED_PS3GCMMEMORY_H

#ifndef SPU

#include "tier1/strtools.h"
#include "shaderapi/gpumemorystats.h"

#include "cell/gcm.h"
#include "gcmconfig.h"

#else



#endif

//--------------------------------------------------------------------------------------------------
// Externals
//--------------------------------------------------------------------------------------------------

#ifndef SPU	

extern void GetGPUMemoryStats( GPUMemoryStats &stats );
extern void Ps3gcmLocalMemoryAllocator_Init();

#endif

//--------------------------------------------------------------------------------------------------
// Memory Pools, Types and LocalMemoryBlock
//--------------------------------------------------------------------------------------------------

enum CPs3gcmAllocationPool_t
{
	kGcmAllocPoolDefault,
	kGcmAllocPoolDynamicNewPath,
	kGcmAllocPoolDynamic,
	kGcmAllocPoolTiledColorFB,		// Frame-buffer tiled color memory (should be first preset tiled region)
	kGcmAllocPoolTiledColorFBQ,		// Quarter-frame-buffer tiled color memory
	kGcmAllocPoolTiledColor512,		// 512x512 tiled color memory
	kGcmAllocPoolTiledColorMisc,	// Last tiled color region
	kGcmAllocPoolTiledD24S8,
	kGcmAllocPoolMainMemory,		// Pool in the main RSX-mapped IO memory
	kGcmAllocPoolMallocMemory,		// Pool in malloc-backed non-RSX-mapped memory
	kGcmAllocPoolCount
};

#define PS3GCMALLOCATIONPOOL( uType ) ( (CPs3gcmAllocationPool_t)( ( ((uint32)(uType)) >> 28 ) & 0xF ) )
#define PS3GCMALLOCATIONALIGN( uType ) ( ((uint32)(uType)) & 0xFFFFFF )
#define PS3GCMALLOCATIONTYPE( uAlign, ePool, iType ) (((uint32)(uAlign))&0xFFFFFF) | ( (((uint32)(iType))&0xF) << 24 ) | ( (((uint32)(ePool))&0xF) << 28 )

enum CPs3gcmAllocationType_t
{
	// Default pool
	kAllocPs3gcmTextureData0 =			PS3GCMALLOCATIONTYPE( 128,		kGcmAllocPoolMainMemory,		0 ),
	kAllocPs3gcmTextureData =			PS3GCMALLOCATIONTYPE( 128,		kGcmAllocPoolDefault,		1 ),
	kAllocPs3GcmVertexBuffer =			PS3GCMALLOCATIONTYPE( 32,		kGcmAllocPoolDefault,		2 ), 
	kAllocPs3GcmIndexBuffer =			PS3GCMALLOCATIONTYPE( 32,		kGcmAllocPoolDefault,		3 ), 
	kAllocPs3GcmShader =				PS3GCMALLOCATIONTYPE( 128,		kGcmAllocPoolDefault,		4 ), 
	kAllocPs3GcmEdgeGeomBuffer =		PS3GCMALLOCATIONTYPE( 128,		kGcmAllocPoolDefault,		5 ),

	// Dynamic pool
	kAllocPs3GcmVertexBufferDynamic =	PS3GCMALLOCATIONTYPE( 32,		kGcmAllocPoolDynamic,		1 ), 
	kAllocPs3GcmIndexBufferDynamic =	PS3GCMALLOCATIONTYPE( 32,		kGcmAllocPoolDynamic,		2 ), 
	kAllocPs3GcmDynamicBufferPool =		PS3GCMALLOCATIONTYPE( 32,		kGcmAllocPoolDynamicNewPath, 1 ),

	// Malloc memory pool
	kAllocPs3GcmVertexBufferDma =		PS3GCMALLOCATIONTYPE( 32,		kGcmAllocPoolMallocMemory,	1 ),
	kAllocPs3GcmIndexBufferDma =		PS3GCMALLOCATIONTYPE( 32,		kGcmAllocPoolMallocMemory,	2 ),

	// Tiled pools
	kAllocPs3gcmColorBufferFB =			PS3GCMALLOCATIONTYPE( 64,		kGcmAllocPoolTiledColorFB,	1 ),
	kAllocPs3gcmColorBufferFBQ =		PS3GCMALLOCATIONTYPE( 64,		kGcmAllocPoolTiledColorFBQ,	1 ),
	kAllocPs3gcmColorBuffer512 =		PS3GCMALLOCATIONTYPE( 64,		kGcmAllocPoolTiledColor512,	1 ),
	kAllocPs3gcmColorBufferMisc =		PS3GCMALLOCATIONTYPE( 64*1024,	kGcmAllocPoolTiledColorMisc,1 ),
	kAllocPs3gcmDepthBuffer =			PS3GCMALLOCATIONTYPE( 64*1024,	kGcmAllocPoolTiledD24S8,	1 ),
};

struct CPs3gcmLocalMemoryBlockSystemGlobal;

struct ALIGN16 CPs3gcmLocalMemoryBlock
{
public:
	CPs3gcmLocalMemoryBlock() {}

#if 0
#define GCMLOCALMEMORYBLOCKDEBUG
	uint64 m_dbgGuardCookie;			// Debug cookie used to guard when calling code let block go out of scope without freeing it
#endif

protected:
	uint32 m_nLocalMemoryOffset;		// Offset in RSX local memory
	uint32 m_uiSize;					// Actual allocation size, might be larger than requested allocation size
	CPs3gcmAllocationType_t m_uType;	// Allocation type with required alignment
	uint32 m_uiIndex;					// Index of the allocation in allocation tracking system

	bool Alloc();						// Internal implementation of Local Memory Allocator

	// Prevent copying (since patch-back mechanism needs to access the allocated blocks)
	CPs3gcmLocalMemoryBlock( CPs3gcmLocalMemoryBlock const &x ) { V_memcpy( this, &x, sizeof( CPs3gcmLocalMemoryBlock ) ); }
	CPs3gcmLocalMemoryBlock& operator =( CPs3gcmLocalMemoryBlock const &x ) { V_memcpy( this, &x, sizeof( CPs3gcmLocalMemoryBlock ) ); return *this; }

public:
	inline void Assign( CPs3gcmLocalMemoryBlockSystemGlobal const &x ) { V_memcpy( this, &x, sizeof( CPs3gcmLocalMemoryBlock ) ); }
	inline bool Alloc( CPs3gcmAllocationType_t uType, uint32 uiSize ) { m_uType = uType; m_uiSize = uiSize; return Alloc(); }
	inline void AttachToExternalMemory( CPs3gcmAllocationType_t uType, uint32 nOffset, uint32 uiSize ) { m_uType = uType; m_uiSize = uiSize; m_nLocalMemoryOffset = nOffset; m_uiIndex = ~0; }
	void Free();
	void FreeAndAllocNew() { Free(); Alloc(); }

	inline uint32 Offset() const { return m_nLocalMemoryOffset; }
	inline uint32 Size() const { return m_uiSize; }

	inline bool IsLocalMemory() const { return PS3GCMALLOCATIONPOOL( m_uType ) < kGcmAllocPoolMainMemory; }
	inline bool IsRsxMappedMemory() const { return PS3GCMALLOCATIONPOOL( m_uType ) < kGcmAllocPoolMallocMemory; }
	inline uint8 GcmMemoryLocation() const { return IsLocalMemory() ? CELL_GCM_LOCATION_LOCAL : CELL_GCM_LOCATION_MAIN; }

	#ifndef SPU
	char * DataInLocalMemory() const;
	char * DataInMainMemory() const;
	char * DataInMallocMemory() const;
	char * DataInAnyMemory() const;
	#endif

	// Tiled memory access
	uint32 TiledMemoryTagAreaBase() const;
	uint32 TiledMemoryIndex() const;

	// Zcull memory access
	uint32 ZcullMemoryIndex() const;
	uint32 ZcullMemoryStart() const;
} ALIGN16_POST;

struct CPs3gcmLocalMemoryBlockSystemGlobal : public CPs3gcmLocalMemoryBlock
{
public:
	CPs3gcmLocalMemoryBlockSystemGlobal() {}

private:
	// Prevent copying (since patch-back mechanism needs to access the allocated blocks)
	CPs3gcmLocalMemoryBlockSystemGlobal( CPs3gcmLocalMemoryBlock const &x );
	CPs3gcmLocalMemoryBlockSystemGlobal& operator =( CPs3gcmLocalMemoryBlockSystemGlobal const &x );
};

//--------------------------------------------------------------------------------------------------
// Buffer (used by IB and VBs)
//--------------------------------------------------------------------------------------------------

struct CPs3gcmBuffer
{
	CPs3gcmLocalMemoryBlock m_lmBlock;

public:
	inline uint32 Offset() { return m_lmBlock.Offset(); }
public:
#ifndef SPU
	static CPs3gcmBuffer * New( uint32 uiSize, CPs3gcmAllocationType_t uType );
	void Release();
#endif
};

#endif // INCLUDED_PS3GCMMEMORY_H