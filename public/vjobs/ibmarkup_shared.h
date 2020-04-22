//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
#ifndef VJOBS_IBMARKUP_SHARED_HDR
#define VJOBS_IBMARKUP_SHARED_HDR

namespace OptimizedModel
{

//
// On PS3 index buffer is laid out very specially:
//
// Header:
// 0xFFFE 0xFFFE 0xFFFE 0xFFFE
//
#ifdef _WIN32
#pragma warning( push )
#pragma warning( disable : 4200 )
#endif

struct OptimizedIndexBufferMarkupPs3_t
{
	static const uint64 kHeaderCookie = 0xFFFEFFFEFFFEFFFEull;
	static const uint16 kVersion1 = 0x0001;

	uint64 m_uiHeaderCookie;
	uint16 m_uiVersionFlags;
	uint16 m_numBytesMarkup;
	uint32 m_numPartitions;
	uint32 m_numIndicesTotal;
	uint32 m_numVerticesTotal;
	uint32 m_nEdgeDmaInputOffsetPerStripGroup;
	uint32 m_nEdgeDmaInputSizePerStripGroup;

	struct Partition_t
	{
		uint32 m_numIndicesToSkipInIBs;
		uint32 m_numVerticesToSkipInVBs;
		
		uint32 m_nIoBufferSize;
		uint32 m_numIndices;
		uint32 m_numVertices;

		uint32 m_nEdgeDmaInputIdx;
		uint32 m_nEdgeDmaInputVtx;
		uint32 m_nEdgeDmaInputEnd;
	};
	Partition_t m_partitions[0];
};

#ifdef _WIN32
#pragma warning( pop )
#endif


}

#endif
