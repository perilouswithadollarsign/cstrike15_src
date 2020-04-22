//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
// Gcm renderer state and util functions
//
//==================================================================================================

#ifndef INCLUDED_GCMSTATE_H
#define INCLUDED_GCMSTATE_H

#ifndef SPU

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "cell\gcm.h"
#include "gcmconfig.h"
#include "ps3gcmmemory.h"
#include "dxabstract_def.h"

#include "spumgr_ppu.h"

#else

#include "spumgr_spu.h"

#endif

//--------------------------------------------------------------------------------------------------
// Misc
//--------------------------------------------------------------------------------------------------

template <typename T>
inline T Min( T a, T b )
{
	return a < b ? a : b;
}

template <typename T>
inline T Max( T a, T b )
{
	return a > b ? a : b;
}

template <typename T>
inline void Swap( T& a , T & b )
{
	T c = a; a = b; b = c;
}

//--------------------------------------------------------------------------------------------------
// Literals
//--------------------------------------------------------------------------------------------------

// IO Memory (page isze is 1MB, so make these add up to 1MB

#define GCM_MAINPOOLSIZE    (0 * 0x100000)		    // IO memory for main pool
#define GCM_DEFCMDBUFFSIZE	(1 * 0x200000)			// Default command buff (must be pow 2)

#define GCM_CALLCMDBUFFSIZE (2 * 0x10000)			// 256 K of cmd buffer to call to
													// Used for DrawprimUP													
#define GCM_CALLCMDSEGSIZE  0x8000					// 32K segmentation

#define GCM_PATCHBUFFSIZE   ((2 * 0x100000) - GCM_CALLCMDBUFFSIZE)
#define GCM_PATCHSEGSIZE    0x8000					

#define GCM_IOSIZE (GCM_MAINPOOLSIZE + GCM_DEFCMDBUFFSIZE + GCM_CALLCMDBUFFSIZE + GCM_PATCHBUFFSIZE)

//--------------------------------------------------------------------------------------------------
// Display Structure
//--------------------------------------------------------------------------------------------------

struct CPs3gcmDisplay
{
	uint32 surfaceFlipIdx;	// which scanout color buffer will be presented with flip
	enum EnumConst_t { SURFACE_COUNT = 2 };
	CPs3gcmLocalMemoryBlockSystemGlobal surfaceColor[SURFACE_COUNT];	// scanout color buffers for double-buffering 
	// (need one more to avoid overwriting old buffer)
	CPs3gcmLocalMemoryBlockSystemGlobal surfaceDepth;					// depth buffer

	void Flip()
	{
		surfaceFlipIdx = NextSurfaceIndex();
	}

	uint NextSurfaceIndex( int nFrame = 1 )const
	{
		return ( surfaceFlipIdx + nFrame ) % SURFACE_COUNT;
	}

	uint PrevSurfaceIndex( int nFrame )const
	{
		int nResult = int( surfaceFlipIdx + 1000000 * SURFACE_COUNT - nFrame ) % int( SURFACE_COUNT );
		Assert( uint( nResult ) < SURFACE_COUNT ); // if this is negative, it means we did ( ( something ) mod 2 ) mod 3, which makes no sense in this context
		return uint( nResult );
	}
};

//--------------------------------------------------------------------------------------------------
// Global GCM state class
//--------------------------------------------------------------------------------------------------

struct CPs3gcmGlobalState
{
	//--------------------------------------------------------------------------------------------------
	// Memory
	// RSX Local, plus one block of memory mapped into RSX (IO mem)
	// Main memory pool is within the IO mem and is used for textures until it fills...
	//--------------------------------------------------------------------------------------------------

	// RSX local memory
	void * m_pLocalBaseAddress;	// RSX Local Memory Base Address
	uint32 m_nLocalBaseOffset;	// cellGcmAddressToOffset( m_pLocalBaseAddress )
	uint32 m_nLocalSize;		// RSX Local Memory Size

	// IO memory mapped into RSX
	void * m_pIoAddress;		// RSX IO buffer, base address
	uint32 m_nIoSize;			// RSX IO total size [including CMD buffer]
	uint32 m_nIoOffsetDelta;    // add this to EA to get Io Offset

	// Call Cmd Buffer
	void*  m_pCallCmdBuffer;
	uint32 m_nCallCmdBufferoffset;
	uint32 m_nCallWritePos;				// Current posn (offset)
	uint32 m_nCallReadSegment;

	// main memory pool buffer
	void * m_pRsxMainMemoryPoolBuffer;
	uint32 m_nRsxMainMemoryPoolBufferSize;

    // Pointer to the draw states
    uint32  m_eaDrawStates;

	//--------------------------------------------------------------------------------------------------
	// SPU Task
	//--------------------------------------------------------------------------------------------------

	SpuTaskHandle				m_spuHandle;
	
	//--------------------------------------------------------------------------------------------------
	// Patched Shaders
	//--------------------------------------------------------------------------------------------------

	uint8*						m_pPatchBuff;
	uint32						m_nPatchIdx;			// Write index for this frames patch buffer
	uint32						m_nPatchReadSeg;

	//--------------------------------------------------------------------------------------------------
	// Empty pixel shader
	//--------------------------------------------------------------------------------------------------

	CPs3gcmLocalMemoryBlock		m_pShaderPsEmptyBuffer;
	CgBinaryProgram				*m_pShaderPsEmpty;	// empty pixel shader
	uint32						m_nPsEmptyShaderControl0;
	uint32						m_nPsEmptyAttributeInputMask;


	//--------------------------------------------------------------------------------------------------
	// Flip data
	//--------------------------------------------------------------------------------------------------

	uint32						m_flipMode;				// Holds 30 or 60
	uint32						m_frameNo;
	uint32						m_finishIdx;
	bool						m_fastFlip;

	//--------------------------------------------------------------------------------------------------
	// Display
	//--------------------------------------------------------------------------------------------------

	// Display size, aspect, pitch
	uint16 						m_nRenderSize[2];	// with & height of the render buffer
	float  						m_flRenderAspect;	// aspect ratio of the output device
	uint32 						m_nSurfaceRenderPitch;
	
	CPs3gcmDisplay				m_display;

	//--------------------------------------------------------------------------------------------------
	// Methods
	//--------------------------------------------------------------------------------------------------

public:
	int32 						Init();
	void  						Shutdown();

	void  						BeginScene();
	void  						EndScene();
	void  						Flip();
	void						SetFastFlip(bool onoff);

	static int32_t				CmdBufferFull(struct CellGcmContextData * pGcmContext, uint32_t size);

	// DrawPrimUP puts a drawprimup call into the call buffer, with a label and RET.
	// It's called from the gcmdrawstate which then sends a drawcall packet to the SPU
	uint32						DrawPrimitiveUP(D3DPRIMITIVETYPE nPrimitiveType,UINT nPrimitiveCount,
												CONST void *pVertexStreamZeroData, UINT nVertexStreamZeroStride );

	// GetRenderSurfaceBytes Note:
	// Height alignment must be 32 for tiled surfaces on RSX
	//                         128 for Edge Post MLAA 
	//                          64 for Edge Post MLAA with EDGE_POST_MLAA_MODE_TRANSPOSE_64 flag set
	inline uint GetRenderSurfaceBytes( uint nHeightAlignment = 32 ) const ; 

private:

	int  InitGcm();
	int  InitVideo();

	void CreateRsxBuffers();		// Display buffers and defaut allocated RTs etc..
	void CreateIoBuffers();			// Allocs IO memory (mapped in Initgcm)
};


//--------------------------------------------------------------------------------------------------
// Inlines
//--------------------------------------------------------------------------------------------------

inline uint CPs3gcmGlobalState::GetRenderSurfaceBytes( uint nHeightAlignment) const 
{ 
	return m_nSurfaceRenderPitch * AlignValue( m_nRenderSize[1], nHeightAlignment ); 
}

//--------------------------------------------------------------------------------------------------
// Extern Globals
//--------------------------------------------------------------------------------------------------

extern CellGcmContextData			gGcmContext;
extern CellGcmContextData*			gpGcmContext;
extern CPs3gcmGlobalState			g_ps3gcmGlobalState;
extern CellGcmContextData			gCallContext;
extern CellGcmContextData*			gpCallContext;

//--------------------------------------------------------------------------------------------------
// Memory block funcs that need access to g_ps3gcmGlobalState
//--------------------------------------------------------------------------------------------------

#ifndef SPU

inline char * CPs3gcmLocalMemoryBlock::DataInLocalMemory() const
{
	Assert( IsLocalMemory() );
	return
		( m_nLocalMemoryOffset - g_ps3gcmGlobalState.m_nLocalBaseOffset ) +
		( char * ) g_ps3gcmGlobalState.m_pLocalBaseAddress;
}

inline char * CPs3gcmLocalMemoryBlock::DataInMainMemory() const
{
	Assert( !IsLocalMemory() && IsRsxMappedMemory() );
	return
		m_nLocalMemoryOffset +
		( ( char * ) g_ps3gcmGlobalState.m_pIoAddress );
}

inline char * CPs3gcmLocalMemoryBlock::DataInMallocMemory() const
{
	Assert( !IsLocalMemory() && !IsRsxMappedMemory() );
	return ( char * ) m_nLocalMemoryOffset;
}

inline char * CPs3gcmLocalMemoryBlock::DataInAnyMemory() const
{
	switch ( PS3GCMALLOCATIONPOOL( m_uType ) )
	{
	default: return DataInLocalMemory();
	case kGcmAllocPoolMainMemory: return DataInMainMemory();
	case kGcmAllocPoolMallocMemory: return DataInMallocMemory();
	}

}

#endif

#endif // INCLUDED_GCMSTATE_H