//========== Copyright © Valve Corporation, All rights reserved. ========
// the constants and declarations needed for the SPU draw queue
//
#ifndef VJOBS_SPUDRAWQUEUE_SHARED_HDR
#define VJOBS_SPUDRAWQUEUE_SHARED_HDR

#include "ps3/ps3_gcm_config.h"
#include "ps3/ps3_gcm_shared.h"
#include "ps3/dxabstract_gcm_shared.h"



enum SpuDrawQueueEnum_t
{
	SPUDRAWQUEUE_NOP                    = 0,
	SPUDRAWQUEUE_METHOD_MASK            = 0xFF000000,
	
	SPUDRAWQUEUE_NOPCOUNT_MASK          = 0x0000FFFF,
	SPUDRAWQUEUE_NOPCOUNT_METHOD        = 0x01000000,

	SPUDRAWQUEUE_SETRENDERSTATE_MASK    = 0x000003FF,
	SPUDRAWQUEUE_SETRENDERSTATE_METHOD  = 0x02000000,
	
	SPUDRAWQUEUE_SETSAMPLERSTATE_MASK   = 0x000000FF, // 4 bits for Sampler, 4 bits for Type
	SPUDRAWQUEUE_SETSAMPLERSTATE_METHOD = 0x03000000,
	
	SPUDRAWQUEUE_SETVIEWPORT_MASK       = 0x0000FFFF,
	SPUDRAWQUEUE_SETVIEWPORT_METHOD     = 0x04000000,
	
	SPUDRAWQUEUE_SETTEXTURE_MASK						= 0x000000FF,
	SPUDRAWQUEUE_SETTEXTURE_METHOD						= 0x05000000,

	SPUDRAWQUEUE_RESETTEXTURE_MASK						= 0x000000FF,
	SPUDRAWQUEUE_RESETTEXTURE_METHOD					= 0x06000000,
	
	SPUDRAWQUEUE_RESETSURFACETOKNOWNDEFAULTSTATE_METHOD = 0x07000000,
	
	SPUDRAWQUEUE_SETPIXELSHADER_METHOD                  = 0x08000000,
	SPUDRAWQUEUE_SETVERTEXSHADER_METHOD                 = 0x09000000,
	
	SPUDRAWQUEUE_SETVERTEXSHADERCONSTANT_B_MASK			= 0x0000FFFF,
	SPUDRAWQUEUE_SETVERTEXSHADERCONSTANT_B_METHOD			= 0x0A000000,
	
	SPUDRAWQUEUE_UNSETVERTEXSTREAMSOURCE_MASK			= 0x0000000F,
	SPUDRAWQUEUE_UNSETVERTEXSTREAMSOURCE_METHOD			= 0x0B000000,

	SPUDRAWQUEUE_SETVERTEXSTREAMSOURCE_MASK				= 0x0000000F,
	SPUDRAWQUEUE_SETVERTEXSTREAMSOURCE_METHOD			= 0x0C000000,

	SPUDRAWQUEUE_SETSAMPLERSTATEPART1_MASK				= 0x0000000F,
	SPUDRAWQUEUE_SETSAMPLERSTATEPART1_METHOD			= 0x0D000000,
	
	SPUDRAWQUEUE_SETSCISSORRECT_METHOD					= 0x0E000000,
	
	SPUDRAWQUEUE_UPDATESURFACE_METHOD					= 0x0F000000,
	SPUDRAWQUEUE_UPDATESURFACE_MASK                     = 0x0000FFFF,
	
	
	SPUDRAWQUEUE_DRAWINDEXEDPRIMITIVE_METHOD            = 0x10000000, // unused!
	
	
	SPUDRAWQUEUE_CLEAR_METHOD                           = 0x11000000,
	SPUDRAWQUEUE_CLEAR_MASK                             = D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET,
	
	SPUDRAWQUEUE_SETVERTEXSHADERCONSTANT_F_MASK			= 0x00FFFFFF,
	SPUDRAWQUEUE_SETVERTEXSHADERCONSTANT_F_METHOD		= 0x12000000,
	
	SPUDRAWQUEUE_VERTEXBUFFERNEWOFFSET_METHOD           = 0x15000000,
	
	SPUDRAWQUEUE_BEGINZCULLREPORT_METHOD				= 0x16000000,
	SPUDRAWQUEUE_ENDZCULLREPORT_METHOD					= 0x17000000,
	
	SPUDRAWQUEUE_GCMCOMMANDS_METHOD                     = 0x18000000,
	SPUDRAWQUEUE_GCMCOMMANDS_MASK                       = 0x00FFFFFF, // word count
	
	SPUDRAWQUEUE_BEGINZPREPASS_METHOD                   = 0x19000000,
	SPUDRAWQUEUE_BEGINZPREPASS_MASK                     = 0x00000FFF,
	
	SPUDRAWQUEUE_PREDICATION_METHOD                     = 0x1A000000,
	SPUDRAWQUEUE_PREDICATION_MASK                       = 0x00FFFFFF,

	SPUDRAWQUEUE_ENDZPREPASS_METHOD                     = 0x1B000000,
	SPUDRAWQUEUE_ENDZPREPASS_MASK                       = 0x00FFFFFF,

	SPUDRAWQUEUE_ENDZPOSTPASS_METHOD                    = 0x1C000000,
	SPUDRAWQUEUE_ENDZPOSTPASS_MASK                      = 0x0000FFFF,
	
	SPUDRAWQUEUE_TRANSFER_METHOD                        = 0x1D000000,
	SPUDRAWQUEUE_TRANSFER_MASK                          = 0x00000003, // transfer mode
	
	SPUDRAWQUEUE_RELOAD_ZCULL_METHOD                    = 0x1E000000,
	SPUDRAWQUEUE_RELOAD_ZCULL_MASK                      = 0x000000FF,
	
	SPUDRAWQUEUE_FLUSH_FPCP_JOURNAL                     = 0x1F000000,
	
	SPUDRAWQUEUE_FRAMEEVENT_METHOD                      = 0x20000000,
	SPUDRAWQUEUE_FRAMEEVENT_MASK                        = 0x00000001,
	
	//SPUDRAWQUEUE_SET_FP_CONSTS_METHOD                   = 0x21000000,
	//SPUDRAWQUEUE_SET_FP_CONSTS_MASK                     = 0x0000FFFF,
	
	SPUDRAWQUEUE_QUEUE_RSX_INTERRUPT_METHOD             = 0x21000000,
	SPUDRAWQUEUE_QUEUE_RSX_INTERRUPT_MASK               = 0x000000FF,
	
	SPUDRAWQUEUE_PERF_MARKER_METHOD                     = 0x22000000,
	SPUDRAWQUEUE_PERF_MARKER_MASK                       = 0x00000FFF,
	SPUDRAWQUEUE_PERF_MARKER_AAReplay                   = SPUDRAWQUEUE_PERF_MARKER_METHOD | 1,
	SPUDRAWQUEUE_PERF_MARKER_AAReplayEnd                = SPUDRAWQUEUE_PERF_MARKER_METHOD | 2,
	SPUDRAWQUEUE_PERF_MARKER_DrawNormal                 = SPUDRAWQUEUE_PERF_MARKER_METHOD | 3,
	SPUDRAWQUEUE_PERF_MARKER_DrawDeferred               = SPUDRAWQUEUE_PERF_MARKER_METHOD | 4,
	
	SPUDRAWQUEUE_DEFER_STATE                            = 0x23000000,
	SPUDRAWQUEUE_UNDEFER_STATE                          = 0x24000000,
	
	SPUDRAWQUEUE_DRAW_INLINE_METHOD                     = 0x30000000,
	SPUDRAWQUEUE_DRAW_INLINE_MASK                       = 0x00FFFFFF,
	
	SPUDRAWQUEUE_DEBUGRECTANGLE_METHOD                  = 0x31000000,

	SPUDRAWQUEUE_FLUSHTEXTURECACHE_METHOD               = 0x32000000,

	SPUDRAWQUEUE_RESETRSXSTATE_METHOD                   = 0x33000000,

	// Deferred queue commands, interpreted on PPU and never getting to SPU job_gcmflush
	// they have the same format, except they're followed by 1 or 2 words of extra data ( SPUDRAWQUEUE_DEFERRED_HEADER_WORDS - 1 )
	
	SPUDRAWQUEUE_DEFERRED_METHOD_MASK   = SPUDRAWQUEUE_METHOD_MASK,

	SPUDRAWQUEUE_DEFERRED_GCMFLUSH_METHOD               = 0x40000000,
	SPUDRAWQUEUE_DEFERRED_GCMFLUSH_MASK                 = 0x00000001,
	SPUDRAWQUEUE_DEFERRED_GCMFLUSH_DRAW_METHOD			= 0x40000001, // this flag means "flush this chunk only when really drawing the queue". Isn't executed when simply updating state (when the queue is recorded to be deferred, it still needs to be semi-executed to update the GCM state)
	
	SPUDRAWQUEUE_DEFERRED_DRAW_METHOD                   = 0x41000000,
	
	SPUDRAWQUEUE_DEFERRED_SET_FP_CONST_METHOD           = 0x42000000,
	SPUDRAWQUEUE_DEFERRED_SET_FP_CONST_MASK             = 0x00FFFFFF,
	
	SPUDRAWQUEUE_DEFERRED_HEADER_WORDS = 2  // set this to 3 to fill more debug data in
};


// this function must only be used in debugging
inline bool IsValidDeferredHeader( uint nCmd )
{
	switch( nCmd & SPUDRAWQUEUE_METHOD_MASK )
	{
		case SPUDRAWQUEUE_DEFERRED_GCMFLUSH_METHOD: 
			return ( nCmd & ~SPUDRAWQUEUE_DEFERRED_GCMFLUSH_MASK ) == SPUDRAWQUEUE_DEFERRED_GCMFLUSH_METHOD;
			
		case SPUDRAWQUEUE_DEFERRED_DRAW_METHOD:
			return nCmd == SPUDRAWQUEUE_DEFERRED_DRAW_METHOD;
			
		case SPUDRAWQUEUE_DEFERRED_SET_FP_CONST_METHOD:
			return true;
			
		default:
			return false;
	}
}


enum SpuDrawQueueFrameEventEnum_t
{
	SDQFE_BEGIN_FRAME = 0,
	SDQFE_END_FRAME   = 1
};

struct SpuGcmEdgeGeomParams_t;

namespace OptimizedModel
{
	struct OptimizedIndexBufferMarkupPs3_t;
}

struct ALIGN16 SpuDrawHeader_t
{
	//uint32 m_nCookie0;
	uint16 m_dirtyCachesMask;

	/*D3DPRIMITIVETYPE*/uint16 m_nType;
	int32 m_nBaseVertexIndex;
	uint32 m_nMinVertexIndex;
	uint32 m_numVertices;
	uint32 m_nStartIndex;
	uint32 m_nPrimCount;
	uint32 m_nLocalOffsetIndexBuffer;
	uint32 m_nPs3texFormatCount;
	uint32 m_nFpcpEndOfJournalIdx;
	
	uint32 m_nUsefulCmdBytes;
	uint32 m_nPcbringBegin;
	uint32 m_nResultantSpuDrawGet; // LSB is the index into the m_nSpuDrawGet[]
	
	uint32 m_nEdgeDebugFlags;
	
	//uint32 m_nSizeofPcbUploaded; // the number of useful bytes uploaded from PCBring

	//SpuGcmEdgeGeomParams_t *m_eaEdgeGeomParams; <- this is passed in job descriptor to job_edgegeom
	//uint32 m_numEdgeGeomVertices;
	//uint32 m_numEdgeIoBufferSize;

	uint32 m_eaEdgeDmaInputBase;
	
	OptimizedModel::OptimizedIndexBufferMarkupPs3_t *m_eaIbMarkup;
	uint32 m_nIbMarkupPartitions;

	uint32 m_nDrawIndexedPrimitives;
	//uint32 m_nCookie1;
}
ALIGN16_POST;



struct SpuDrawScissor_t
{
	uint16 x, y, w, h;
};

struct SpuDrawDeviceClear_t
{
	D3DCOLOR m_nColor;
	float    m_flZ;
	uint32   m_nStencil;
	uint32   m_nDepthStencilBitDepth;
};

struct SpuDrawDebugRectangle_t
{
	D3DCOLOR color;
	uint16_t x,y,w,h;
};


struct SpuDrawTransfer_t
{
	uint32 m_nLineSize;
	uint32 m_nOldOffset;
	uint32 m_nNewOffset;
};

// 
struct SpuUpdateSurface_t
{
	// if the scissor is logically disabled, set scissor to this size
	//uint16 m_nRenderTargetWidth, m_nRenderTargetHeight;
	
	CPs3gcmTextureData_t m_texC, m_texZ;
};


struct SpuSetSamplerStatePart1_t
{
	D3DSamplerDescPart1 m_desc;
	CPs3gcmTextureData_t m_tex;
};


#endif