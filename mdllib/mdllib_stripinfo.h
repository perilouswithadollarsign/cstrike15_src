//====== Copyright c 1996-2007, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
//=============================================================================//

#ifndef MDLLIB_STRIPINFO_H
#define MDLLIB_STRIPINFO_H
#ifdef _WIN32
#pragma once
#endif

#include "mdllib/mdllib.h"
#include "mdllib_utils.h"
#include "UtlSortVector.h"


//
// CMdlStripInfo
//	Implementation of IMdlStripInfo interface
//
class CMdlStripInfo : public IMdlStripInfo
{
public:
	CMdlStripInfo();
	~CMdlStripInfo() { m_ps3studioBatches.PurgeAndDeleteElements(); }

	//
	// Serialization
	//
public:
	// Save the strip info to the buffer (appends to the end)
	virtual bool Serialize( CUtlBuffer &bufStorage ) const;

	// Load the strip info from the buffer (reads from the current position as much as needed)
	virtual bool UnSerialize( CUtlBuffer &bufData );

	//
	// Stripping info state
	//
public:
	// Returns the checksums that the stripping info was generated for:
	//	plChecksumOriginal		if non-NULL will hold the checksum of the original model submitted for stripping
	//	plChecksumStripped		if non-NULL will hold the resulting checksum of the stripped model
	virtual bool GetCheckSum( long *plChecksumOriginal, long *plChecksumStripped ) const;

	//
	// Stripping
	//
public:

	//
	// StripHardwareVertsBuffer
	//	The main function that strips the vhv buffer
	//		vhvBuffer		- vhv buffer, updated, size reduced
	//
	virtual bool StripHardwareVertsBuffer( CUtlBuffer &vhvBuffer );

	//
	// StripModelBuffer
	//	The main function that strips the mdl buffer
	//		mdlBuffer		- mdl buffer, updated
	//
	virtual bool StripModelBuffer( CUtlBuffer &mdlBuffer );

	//
	// StripVertexDataBuffer
	//	The main function that strips the vvd buffer
	//		vvdBuffer		- vvd buffer, updated, size reduced
	//
	virtual bool StripVertexDataBuffer( CUtlBuffer &vvdBuffer );

	//
	// StripOptimizedModelBuffer
	//	The main function that strips the vtx buffer
	//		vtxBuffer		- vtx buffer, updated, size reduced
	//
	virtual bool StripOptimizedModelBuffer( CUtlBuffer &vtxBuffer );

	//
	// Release the object with "delete this"
	//
public:
	virtual void DeleteThis();



public:
	void Reset();


public:
	enum Mode
	{
		MODE_UNINITIALIZED	= 0,
		MODE_NO_CHANGE		= 1,
		MODE_STRIP_LOD_1N	= 2,
		MODE_PS3_PARTITIONS	= 3,
		MODE_PS3_FORMAT_BASIC = 4,
	};

	//
	// Internal data used for stripping
	//
public:
	int m_eMode;
	long m_lChecksumOld, m_lChecksumNew;
	CGrowableBitVec m_vtxVerts;
	CUtlSortVector< unsigned short, CLessSimple< unsigned short > > m_vtxIndices;

	//
	// PS3 partitioning data
	//
public:
	struct Ps3studioPartition_t
	{
		CUtlVector< uint16 > m_arrLocalIndices;
		CUtlVector< uint32 > m_arrVertOriginalIndices;
		CUtlVector< uint32 > m_arrStripLocalOriginalIndices;
		uint32 m_nIoBufferSize;

		// -- not serialized in .vsi --
		uint32 m_nEdgeDmaInputIdx;
		uint32 m_nEdgeDmaInputVtx;
		uint32 m_nEdgeDmaInputEnd;
		// compressed idx information
		uint8 *m_pEdgeCompressedIdx;
		uint16 m_uiEdgeCompressedIdxDmaTagSize[2];
		// compressed vtx information
		uint8 *m_pEdgeCompressedVtx;
		uint32 *m_pEdgeCompressedVtxFixedOffsets;
		uint16 m_uiEdgeCompressedVtxDmaTagSize[3];
		uint32 m_uiEdgeCompressedVtxFixedOffsetsSize;
	};
	struct Ps3studioBatch_t
	{
		CUtlVector< Ps3studioPartition_t * > m_arrPartitions;
		uint32 m_uiModelIndexOffset;
		uint32 m_uiVhvIndexOffset;

		~Ps3studioBatch_t() { m_arrPartitions.PurgeAndDeleteElements(); }
	};
	CUtlVector< Ps3studioBatch_t * > m_ps3studioBatches;
	CUtlVector< uint32 > m_ps3studioStripGroupHeaderBatchOffset;

	//
	// Mesh ranges fixup
	//
public:
	struct MdlRangeItem
	{
		/* implicit */ MdlRangeItem( int offOld = 0, int numOld = 0, int offNew = 0, int numNew = 0 ) :
			m_offOld( offOld ), m_offNew( offNew ), m_numOld( numOld ), m_numNew( numNew ) {}

		int m_offOld, m_offNew;
		int m_numOld, m_numNew;

		bool operator < ( MdlRangeItem const &x ) const { return m_offNew < x.m_offNew; }
	};
	CUtlSortVector< CMdlStripInfo::MdlRangeItem, CLessSimple< CMdlStripInfo::MdlRangeItem > > m_vtxMdlOffsets;

};


#endif // #ifndef MDLLIB_STRIPINFO_H
