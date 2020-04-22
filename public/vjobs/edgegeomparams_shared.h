//========== Copyright © Valve Corporation, All rights reserved. ========
#if !defined( VJOBS_EDGEGEOM_JOBPARAMS_SHARED_HDR ) && defined( _PS3 )
#define VJOBS_EDGEGEOM_JOBPARAMS_SHARED_HDR

#include "ps3/spu_job_shared.h"
#include "edge/geom/edgegeom_structs.h"

struct SpuGcmEdgeGeomParams_t;

namespace job_edgegeom
{

	struct JobParams_t
	{
		uint32 m_nFlags;
		uint32 m_eaEdgeGcmControl;
		uint32 m_numEdgeIndices;
		uint32 m_numEdgeVertices;
		
		EdgeGeomViewportInfo m_edgeGeomViewportInfo;
		EdgeGeomLocalToWorldMatrix m_edgeGeomLocalToWorldMatrix;

		uint32 m_nAdjustOutputIndices; // this gets added to the output indices, to adjust for the skipped vertices

		uint32 m_uiGcmCount;
		uint32 m_nLocalMemoryIndexBuffer;
		uint32 m_nCmdBufferHoleBytes;
		uint32 m_nQueueTag;
		uint32 m_eaEdgeGeomJts;

		uint32 m_eaEdgeDmaInputBase;
		uint32 m_nEdgeDmaInputIdx;
		uint32 m_nEdgeDmaInputVtx;
		uint32 m_nEdgeDmaInputEnd;
		uint32 m_nEdgeDmaIoBufferSize;
		uint32 m_nExecutedOnSpu; // this must be 0xFFFF FFFF before it's executed, and the SpuId when it's executed
		
		uint32 m_nEdgeJobId;
		
		uint16 m_uiMarkupVersionFlags;
		uint8 m_uiGcmMode;
		uint8 m_uiCullFlavor;
	};

	struct ALIGN128 JobDescriptor_t
	{
		CellSpursJobHeader header;
		enum { DMA_LIST_CAPACITY = 1 + 4 + 4 + 4 + 1 };
		union {
			uint64_t dmaList[DMA_LIST_CAPACITY];
			uint64_t userData[DMA_LIST_CAPACITY];
		} workArea;
		// pad it so that params END exactly at the end of the structure, this leaves the maximum safety slack
		// between the params and DMA list just in case the DMA list overflows
		uint8 paddingToMakeJobDescriptorBigEnough[ sizeof(JobParams_t) ];
		
		//uint8 padding[ 127 & ~( sizeof( CellSpursJobHeader ) + sizeof( uint64 ) * DMA_LIST_CAPACITY ) ];
		//JobParams_t params;
		
	}ALIGN128_POST;

	inline JobParams_t * GetJobParams( void *pJob )
	{
		COMPILE_TIME_ASSERT( sizeof( JobDescriptor_t ) <= 896 ); // the absolute maximum for the job descriptor
		return VjobGetJobParams< JobParams_t, JobDescriptor_t >( pJob );
	}	
}


#endif