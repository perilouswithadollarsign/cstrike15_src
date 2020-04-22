//========== Copyright © 2010, Valve Corporation, All rights reserved. ========
// Global GCM-related state
//

#ifndef _PS3GCMSTATE_H_INC_
#define _PS3GCMSTATE_H_INC_

#include "ps3/ps3gcmmemory.h"
#include <cell/gcm.h>
#include "bitmap/imageformat.h"
#include "ps3/ps3_gcm_shared.h"

class CPs3gcmGlobalState
{
public:
	void * m_pIoAddress;		// RSX IO buffer, base address
	uint32 m_nIoSize;			// RSX IO total size [including CMD buffer]
	uint32 m_nIoSizeNotPreallocated; // the io total size that wasn't pre-allocated in initialization
	uint32 m_nCmdSize;			// RSX CMD buffer total size [including first reserved 4K]
	uint32 const volatile *m_pCurrentCmdBufferSegmentRSX;	// Begin offset of current CMD buffer segment being processed by RSX

#if GCM_CTX_UNSAFE_MODE
	uint32 *m_pCurrentCmdBufferUnflushedBeginRSX;			// Marks beginning of not yet flushed RSX buffer
#endif

	void * m_pLocalBaseAddress;	// RSX Local Memory Base Address
	uint32 m_nLocalBaseOffset;	// cellGcmAddressToOffset( m_pLocalBaseAddress )
	uint32 m_nLocalSize;		// RSX Local Memory Size

	uint16 m_nRenderSize[2];	// with & height of the render buffer
	float  m_flRenderAspect;	// aspect ratio of the output device
	
	uint32 m_nIoOffsetDelta;   // add this to EA to get Io Offset
	
	uint32 m_nSurfaceRenderPitch;
	
	// this is used to allocate permanent cmd buffers; to be cleared when level reloads, hopefully won't need anything more complicated than that
	// but if we do, we can make a page-chain-based (page from 128 bytes) allocator with reference count per page
	// NOTE: the buffer MUST have 1KB padding in the end to prevent overfetch RSX crash!
	CellGcmContextData m_cmdBufferPermContext;  
	
	// vertex and index data buffer
	void * m_pRsxDataTransferBuffer;
	uint32 m_nRsxDataTransferBufferSize;

	// main memory pool buffer
	void * m_pRsxMainMemoryPoolBuffer;
	uint32 m_nRsxMainMemoryPoolBufferSize;
	
	// special texture to support debug stripes
	CPs3gcmLocalMemoryBlock m_debugStripeImageBuffer;
	
	uint32 m_nCmdBufferRefCount; // how many buffers are referenced?

	CPs3gcmDisplay m_display;	// m_display objects that are created automatically
	CPs3gcmLocalMemoryBlock m_pShaderPsEmptyBuffer;
	CgBinaryProgram *m_pShaderPsEmpty;	// empty pixel shader
	uint32 m_nIoLocalOffsetEmptyFragmentProgramSetupRoutine;
	
	uint32 m_nFlushCounter;

	float m_flAllocatorStallTimeWaitingRSX;	// how long allocator ended up waiting for RSX

public:
	int32 Init();
	void Shutdown();

	void DrawDebugStripe( uint nScreenX, uint nScreenY, uint nStripeY, uint nStripeWidth, uint nStripeHeight, int nNext = 0 );
	// pre-allocate memory before command buffer is allocated	
	void * IoMemoryPrealloc( uint nAlign, uint nSize );
	void * IoSlackAlloc( uint nAlign, uint nSize );
	void IoSlackFree( void * eaMemory );
	bool IsIoMemory( void * eaMemory );
	
	uintp CmdBufferToIoOffset( void *pCmdBuffer );
	CellGcmContextData* CmdBufferAlloc( );
	void CmdBufferFreeOffset( uint32 );

	enum CmdBufferFlushType_t
	{
		kFlushForcefully,
		kFlushEndFrame
	};
	void CmdBufferFlush( CmdBufferFlushType_t eFlushType );
	void CmdBufferFinish();
	void CmdBufferReservationCallback( struct CellGcmContextData *context );

	uint32 GetRsxControlNextReferenceValue();
	
	// Note:
	// Height alignment must be 32 for tiled surfaces on RSX
	//                         128 for Edge Post MLAA 
	//                          64 for Edge Post MLAA with EDGE_POST_MLAA_MODE_TRANSPOSE_64 flag set
	uint GetRenderSurfaceBytes( uint nHeightAlignment = 32 ) const { return m_nSurfaceRenderPitch * AlignValue( m_nRenderSize[1], nHeightAlignment ); }
	
protected:
	void CreateDebugStripeTextureBuffer();
	void CreateEmptyPixelShader();
	void CreateRsxBuffers();
	void CreateIoBuffers();
	int InitVideo();
	int InitGcm();
};



inline uintp CPs3gcmGlobalState::CmdBufferToIoOffset( void * pCmdBuffer )
{
	uintp nIoOffset = uintp( pCmdBuffer ) + m_nIoOffsetDelta;
	Assert( ( uintp( pCmdBuffer ) >= uintp( m_cmdBufferPermContext.begin ) && uintp( pCmdBuffer ) < uintp( m_cmdBufferPermContext.end ) ) // can be a perm context buffer
		|| ( nIoOffset >= 4096 && nIoOffset <= m_nCmdSize ) ); // or it can be the main cmd buffer (SYSring) 
	return nIoOffset;
}

inline CellGcmContextData* CPs3gcmGlobalState::CmdBufferAlloc( )
{
	m_nCmdBufferRefCount++;
	return &m_cmdBufferPermContext;
}

inline void CPs3gcmGlobalState::CmdBufferFreeOffset( uint32 )
{
	if( !--m_nCmdBufferRefCount )
	{
		m_cmdBufferPermContext.current = m_cmdBufferPermContext.begin;
	}
}



extern CPs3gcmGlobalState g_ps3gcmGlobalState;


//////////////////////////////////////////////////////////////////////////
//
// inline implementations of PPU-only stuff
//
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



// Allow shaderapi to query GPU memory stats:
extern void GetGPUMemoryStats( GPUMemoryStats &stats );


class CmdSubBuffer: public CellGcmContextData
{
public:
	static int32_t DoNothing( struct CellGcmContextData *pContext, uint32_t nWords )
	{
		Error( "CmdSubBuffer callback @%p: trying to allocate %u words\n", pContext, nWords );
		return CELL_ERROR_ERROR_FLAG;
	}

	CmdSubBuffer( uint32 * pBuffer, uint nAllocateWords )
	{
		this->current = this->begin = pBuffer; 
		this->end = this->begin + nAllocateWords;
		this->callback = DoNothing;
	}
	
	~CmdSubBuffer()
	{
		Assert( this->current == this->end );
	}
};

extern uint32 CalculateMemorySizeFromCmdLineParam( char const *pCmdParamName, uint32 nDefaultValue, uint32 nMinValue = 0 );

inline bool CPs3gcmGlobalState::IsIoMemory( void * eaMemory )
{
	return uintp( eaMemory ) >= uintp( m_pIoAddress ) && uintp( eaMemory ) <= uintp( m_pIoAddress ) + m_nIoSize;
}

#endif // _PS3GCMSTATE_H_INC_
