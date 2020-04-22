//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "mdllib_common.h"
#include "mdllib_stripinfo.h"
#include "mdllib_utils.h"

#include "studio.h"
#include "optimize.h"

#include "smartptr.h"

#include "edge/libedgegeomtool/libedgegeomtool.h"

#include "vjobs/ibmarkup_shared.h"

DECLARE_LOGGING_CHANNEL( LOG_ModelLib );

#define DmaTagsListSize2(var) ( (var)[0] + (var)[1] )
#define DmaTagsListSize3(var) ( DmaTagsListSize2(var) + (var)[2] )

static void Helper_SwapInsideIndexBuffer_forPs3( void *pvMemory, uint32 uiSize )
{
	uint16 *puiMemory = reinterpret_cast< uint16 * >( pvMemory );
	uiSize /= 2;

	for ( uint32 j = 0; j < uiSize/2; ++ j )
	{
		uint16 uiTemp = puiMemory[j];
		puiMemory[j] = puiMemory[ uiSize - j - 1 ];
		puiMemory[ uiSize - j - 1 ] = uiTemp;
	}
}

static void Helper_SwapOptimizedIndexBufferMarkup_forPs3( OptimizedModel::OptimizedIndexBufferMarkupPs3_t *pMarkup )
{
	for ( int j = 0; j < pMarkup->m_numPartitions; ++ j )
	{
		OptimizedModel::OptimizedIndexBufferMarkupPs3_t::Partition_t &partition = pMarkup->m_partitions[j];
		Helper_SwapInsideIndexBuffer_forPs3( &partition.m_numIndicesToSkipInIBs, sizeof( partition.m_numIndicesToSkipInIBs ) );
		Helper_SwapInsideIndexBuffer_forPs3( &partition.m_numVerticesToSkipInVBs, sizeof( partition.m_numVerticesToSkipInVBs ) );
		Helper_SwapInsideIndexBuffer_forPs3( &partition.m_nIoBufferSize, sizeof( partition.m_nIoBufferSize ) );
		Helper_SwapInsideIndexBuffer_forPs3( &partition.m_numIndices, sizeof( partition.m_numIndices ) );
		Helper_SwapInsideIndexBuffer_forPs3( &partition.m_numVertices, sizeof( partition.m_numVertices ) );
		Helper_SwapInsideIndexBuffer_forPs3( &partition.m_nEdgeDmaInputIdx, sizeof( partition.m_nEdgeDmaInputIdx ) );
		Helper_SwapInsideIndexBuffer_forPs3( &partition.m_nEdgeDmaInputVtx, sizeof( partition.m_nEdgeDmaInputVtx ) );
		Helper_SwapInsideIndexBuffer_forPs3( &partition.m_nEdgeDmaInputEnd, sizeof( partition.m_nEdgeDmaInputEnd ) );
	}

	Helper_SwapInsideIndexBuffer_forPs3( &pMarkup->m_uiHeaderCookie, sizeof( pMarkup->m_uiHeaderCookie ) );
	Helper_SwapInsideIndexBuffer_forPs3( &pMarkup->m_uiVersionFlags, sizeof( pMarkup->m_uiVersionFlags ) );
	Helper_SwapInsideIndexBuffer_forPs3( &pMarkup->m_numBytesMarkup, sizeof( pMarkup->m_numBytesMarkup ) );
	Helper_SwapInsideIndexBuffer_forPs3( &pMarkup->m_numPartitions, sizeof( pMarkup->m_numPartitions ) );
	Helper_SwapInsideIndexBuffer_forPs3( &pMarkup->m_numIndicesTotal, sizeof( pMarkup->m_numIndicesTotal ) );
	Helper_SwapInsideIndexBuffer_forPs3( &pMarkup->m_numVerticesTotal, sizeof( pMarkup->m_numVerticesTotal ) );
	Helper_SwapInsideIndexBuffer_forPs3( &pMarkup->m_nEdgeDmaInputOffsetPerStripGroup, sizeof( pMarkup->m_nEdgeDmaInputOffsetPerStripGroup ) );
	Helper_SwapInsideIndexBuffer_forPs3( &pMarkup->m_nEdgeDmaInputSizePerStripGroup, sizeof( pMarkup->m_nEdgeDmaInputSizePerStripGroup ) );
}

struct ModelBatchKey_t
{
	ModelBatchKey_t() { V_memset( this, 0, sizeof( *this ) ); }
	ModelBatchKey_t( int bp, int mdl, int lod, int mesh, int sgroup, int strip) { V_memset( this, 0, sizeof( *this ) ); iBodyPart = bp; iModel = mdl; iLod = lod; iMesh = mesh; iStripGroup = sgroup; iStrip = strip; }
	ModelBatchKey_t( ModelBatchKey_t const & x ) { V_memcpy( this, &x, sizeof( *this ) ); }
	ModelBatchKey_t& operator=( ModelBatchKey_t const &x ) { if ( &x != this ) V_memcpy( this, &x, sizeof( *this ) ); return *this; }
	int iBodyPart;
	int iModel;
	int iLod;
	int iMesh;
	int iStripGroup;
	int iStrip;
	bool operator <( ModelBatchKey_t const &x ) const { return V_memcmp( this, &x, sizeof( *this ) ) < 0; }
};

//
// StripModelBuffers
//	The main function that strips the model buffers
//		mdlBuffer			- mdl buffer, updated, no size change
//		vvdBuffer			- vvd buffer, updated, size reduced
//		vtxBuffer			- vtx buffer, updated, size reduced
//		ppStripInfo			- if nonzero on return will be filled with the stripping info
//
bool CMdlLib::PrepareModelForPs3( CUtlBuffer &mdlBuffer, CUtlBuffer &vvdBuffer, CUtlBuffer &vtxBuffer, IMdlStripInfo **ppStripInfo )
{
	DECLARE_PTR( byte, mdl, BYTE_OFF_PTR( mdlBuffer.Base(), mdlBuffer.TellGet() ) );
	DECLARE_PTR( byte, vvd, BYTE_OFF_PTR( vvdBuffer.Base(), vvdBuffer.TellGet() ) );
	DECLARE_PTR( byte, vtx, BYTE_OFF_PTR( vtxBuffer.Base(), vtxBuffer.TellGet() ) );

	int vvdLength = vvdBuffer.TellPut() - vvdBuffer.TellGet();
	int vtxLength = vtxBuffer.TellPut() - vtxBuffer.TellGet();

	//
	// ===================
	// =================== Modify the checksum and check if further processing is needed
	// ===================
	//

	DECLARE_PTR( studiohdr_t, mdlHdr, mdl );
	DECLARE_PTR( vertexFileHeader_t, vvdHdr, vvd );
	DECLARE_PTR( OptimizedModel::FileHeader_t, vtxHdr, vtx );
	
	long checksumOld = mdlHdr->checksum;

	// Don't do anything if the checksums don't match
	if ( ( mdlHdr->checksum != vvdHdr->checksum ) ||
		 ( mdlHdr->checksum != vtxHdr->checkSum ) )
	{
		Log_Msg( LOG_ModelLib, "ERROR: [PrepareModelForPs3] checksum mismatch!\n" );
		return false;
	}

	// Modify the checksums
	mdlHdr->checksum ^= ( mdlHdr->checksum * 123333 );
	vvdHdr->checksum ^= ( vvdHdr->checksum * 123333 );
	vtxHdr->checkSum ^= ( vtxHdr->checkSum * 123333 );

	long checksumNew = mdlHdr->checksum;

	// Allocate the model stripping info
	CMdlStripInfo msi;
	CMdlStripInfo *pMsi;

	if ( ppStripInfo )
	{
		if ( *ppStripInfo )
		{
			pMsi = ( CMdlStripInfo * ) ( *ppStripInfo );
			pMsi->Reset();
		}
		else
		{
			*ppStripInfo = pMsi = new CMdlStripInfo;
		}
	}
	else
	{
		pMsi = &msi;
	}

	// Set the basic stripping info settings
	pMsi->m_lChecksumOld = checksumOld;
	pMsi->m_lChecksumNew = checksumNew;
	mdlHdr->flags &= ~STUDIOHDR_FLAGS_PS3_EDGE_FORMAT;
	if ( !vvdHdr->numFixups )
		vvdHdr->fixupTableStart = 0;

	//
	// Early outs
	//

	if ( mdlHdr->numbones != 1 )
	{
		Log_Msg( LOG_ModelLib, "No special stripping - the model has %d bone(s).\n", mdlHdr->numbones );
		pMsi->m_eMode = CMdlStripInfo::MODE_PS3_FORMAT_BASIC;
		return true;
	}

	if ( CommandLine()->FindParm( "-mdllib_ps3_noedge" ) )
	{
		pMsi->m_eMode = CMdlStripInfo::MODE_PS3_FORMAT_BASIC;
		return true;
	}

	bool const bEmitIndices = !!CommandLine()->FindParm( "-mdllib_ps3_edgeidxbuf" );
	bool const bUncompressedIndices = !!CommandLine()->FindParm( "-mdllib_ps3_edgeidxbufUncompressed" );
	bool const bUncompressedVertices = true || !!CommandLine()->FindParm( "-mdllib_ps3_edgevtxbufUncompressed" );
	struct EdgeGeomFreeMemoryTracker_t : public CUtlVector< void * >
	{
		~EdgeGeomFreeMemoryTracker_t() { for ( int k = 0; k < Count(); ++ k ) if ( void *p = Element( k ) ) edgeGeomFree( p ); }
	} ps3edgeGeomFreeMemoryTracker;

	// Otherwise do stripping
	pMsi->m_eMode = CMdlStripInfo::MODE_PS3_PARTITIONS;
	mdlHdr->flags |= STUDIOHDR_FLAGS_PS3_EDGE_FORMAT;

	CByteswap ps3byteswap;
	ps3byteswap.SetTargetBigEndian( true );	// PS3 target is big-endian

	
	//
	// ===================
	// =================== Build out table of LODx vertexes
	// ===================
	//

	CUtlVector< CMdlStripInfo::Ps3studioBatch_t * > &ps3studioBatches = pMsi->m_ps3studioBatches;
	typedef CUtlMap< ModelBatchKey_t, int > ModelBatch2idx;
	ModelBatch2idx mapMBI( DefLessFunc( ModelBatchKey_t ) );
	uint32 numVhvOriginalModelVertices = 0;
	{

	DECLARE_PTR( mstudiovertex_t, vvdVertexSrc, BYTE_OFF_PTR( vvdHdr, vvdHdr->vertexDataStart ) );

	ITERATE_CHILDREN2( OptimizedModel::BodyPartHeader_t, mstudiobodyparts_t, vtxBodyPart, mdlBodyPart, vtxHdr, mdlHdr, pBodyPart, pBodypart, numBodyParts )
		ITERATE_CHILDREN2( OptimizedModel::ModelHeader_t, mstudiomodel_t, vtxModel, mdlModel, vtxBodyPart, mdlBodyPart, pModel, pModel, numModels )

			if ( ( mdlModel->vertexindex%sizeof( mstudiovertex_t ) ) ||
				 ( mdlModel->tangentsindex%sizeof( Vector4D ) ) ||
				 ( (mdlModel->vertexindex/sizeof( mstudiovertex_t )) != (mdlModel->tangentsindex/sizeof( Vector4D )) ) )
			{
				Error( "PrepareModelForPs3: invalid model setup [mdlModel_idx=%d]!\n", vtxModel_idx );
			}
			uint32 uiModelIndexOffset = (mdlModel->vertexindex/sizeof( mstudiovertex_t ));
			ITERATE_CHILDREN( OptimizedModel::ModelLODHeader_t, vtxLod, vtxModel, pLOD, numLODs )
				ITERATE_CHILDREN2( OptimizedModel::MeshHeader_t, mstudiomesh_t, vtxMesh, mdlMesh, vtxLod, mdlModel, pMesh, pMesh, numMeshes )
					ITERATE_CHILDREN( OptimizedModel::StripGroupHeader_t, vtxStripGroup, vtxMesh, pStripGroup, numStripGroups )
						pMsi->m_ps3studioStripGroupHeaderBatchOffset.AddToTail( ps3studioBatches.Count() );
						ITERATE_CHILDREN( OptimizedModel::StripHeader_t, vtxStrip, vtxStripGroup, pStrip, numStrips )

							//
							// Compute triangle indices list for edge
							//
							if( ( vtxStrip->numIndices % 3 ) != 0 )
							{
								Error( "PrepareModelForPs3: invalid number of indices/3! [%d]\n", vtxStrip->numIndices );
							}
							
							CUtlVector< uint32 > arrEdgeInputIndices;
							CUtlVector< uint32 > arrStripLocalEdgeInputIndex;
							arrEdgeInputIndices.SetCount( vtxStrip->numIndices );
							arrStripLocalEdgeInputIndex.EnsureCapacity( vtxStrip->numIndices );
							for ( int i = 0; i < vtxStrip->numIndices; ++ i )
							{
								unsigned short *vtxIdx = CHILD_AT( vtxStripGroup, pIndex, vtxStrip->indexOffset + i );
								OptimizedModel::Vertex_t *vtxVertex = CHILD_AT( vtxStripGroup, pVertex, *vtxIdx );
								
								uint32 uiInputIdx = vtxVertex->origMeshVertID + mdlMesh->vertexoffset;
								arrEdgeInputIndices[i] = uiInputIdx;
								
								if ( arrStripLocalEdgeInputIndex.Count() <= uiInputIdx )
									arrStripLocalEdgeInputIndex.SetCountNonDestructively( uiInputIdx + 1 );
								arrStripLocalEdgeInputIndex[uiInputIdx] = *vtxIdx;
							}

							//
							// Compute triangle centroids for the batch
							//
							float *pEdgeTriangleCentroids = NULL;
							float *flModelSourceVertices = reinterpret_cast< float * >( vvdVertexSrc ) + uiModelIndexOffset;
							uint16 uiModelSourceVerticesPositionIdx = reinterpret_cast< float * >( &vvdVertexSrc->m_vecPosition.x ) - reinterpret_cast< float * >( vvdVertexSrc );
							edgeGeomComputeTriangleCentroids(
								flModelSourceVertices, sizeof( mstudiovertex_t ) / sizeof( float ),
								uiModelSourceVerticesPositionIdx,
								arrEdgeInputIndices.Base(),
								vtxStrip->numIndices / 3,
								&pEdgeTriangleCentroids
							);
						
							//
							// Let EDGE tool framework perform partitioning of our studio batch
							//
							EdgeGeomPartitionerInput egpi;
							
							egpi.m_numTriangles = vtxStrip->numIndices / 3;
							egpi.m_triangleList = arrEdgeInputIndices.Base();

							egpi.m_numInputAttributes = 1;
							egpi.m_numOutputAttributes = 0;
							egpi.m_inputVertexStride[0] = 12;
							egpi.m_inputVertexStride[1] = 0;
							egpi.m_outputVertexStride = 0;
							egpi.m_skinningFlavor = kSkinNone;
							egpi.m_indexListFlavor = kIndexesU16TriangleListCW;
							egpi.m_skinningMatrixFormat = kMatrix4x4RowMajor;
							
							egpi.m_cacheOptimizerCallback = CommandLine()->FindParm( "-mdllib_ps3_edgenokcache" ) ? 0 : edgeGeomKCacheOptimizerHillclimber;
							EdgeGeomKCacheOptimizerHillclimberUserData edgeCacheOptimizerUserData =
							{
								400, // iterations (100 iterations give about 1% improvement)
								kIndexesU16TriangleListCW, // indexes type
								1, 0 // RSX input and output attributes count
							};
							egpi.m_cacheOptimizerUserData = &edgeCacheOptimizerUserData;
							egpi.m_customDataSizeCallback = NULL;
							egpi.m_triangleCentroids = pEdgeTriangleCentroids;
							egpi.m_skinningMatrixIndexesPerVertex = NULL; // no skinning
							egpi.m_deltaStreamVertexStride = 0;	// no blendshapes
							egpi.m_blendedVertexIndexes = NULL; // no skinning
							egpi.m_numBlendedVertexes = 0;	// no blendshapes
							egpi.m_customCommandBufferHoleSizeCallback = 0; // no custom data

							// Partition our geometry
							EdgeGeomPartitionerOutput egpo;
							edgeGeomPartitioner( egpi, &egpo );

							//
							// Represent partitioned geometry as mdllib batch description
							//
							CMdlStripInfo::Ps3studioBatch_t *ps3studioBatch = new CMdlStripInfo::Ps3studioBatch_t;
							ps3studioBatch->m_uiModelIndexOffset = uiModelIndexOffset;
							ps3studioBatch->m_uiVhvIndexOffset = numVhvOriginalModelVertices;
							int iBatchIdx = ps3studioBatches.AddToTail( ps3studioBatch );
							ModelBatchKey_t mbk( vtxBodyPart_idx, vtxModel_idx, vtxLod_idx, vtxMesh_idx, vtxStripGroup_idx, vtxStrip_idx );
							Assert( mapMBI.Find( mbk ) == mapMBI.InvalidIndex() );
							mapMBI.Insert( mbk, iBatchIdx );
							EdgeGeomPartitionerOutput egpoConsumable = egpo;
							while ( egpoConsumable.m_numPartitions -- > 0 )
							{
								CMdlStripInfo::Ps3studioPartition_t *ps3partition = new CMdlStripInfo::Ps3studioPartition_t;
								ps3studioBatch->m_arrPartitions.AddToTail( ps3partition );
								
								ps3partition->m_nIoBufferSize = * ( egpoConsumable.m_ioBufferSizePerPartition ++ );
								
								ps3partition->m_arrLocalIndices.EnsureCapacity( *egpoConsumable.m_numTrianglesPerPartition );
								
								// Compress index information
								edgeGeomMakeIndexBuffer( egpoConsumable.m_triangleListOut, *egpoConsumable.m_numTrianglesPerPartition,
									kIndexesCompressedTriangleListCW,
									&ps3partition->m_pEdgeCompressedIdx, ps3partition->m_uiEdgeCompressedIdxDmaTagSize );
								ps3edgeGeomFreeMemoryTracker.AddToTail( ps3partition->m_pEdgeCompressedIdx );

								// Compress vertex information
								EdgeGeomAttributeId edgeGeomAttributeIdSpuVB[1] = { EDGE_GEOM_ATTRIBUTE_ID_POSITION };
								uint16 edgeGeomAttributeIndexSpuVB[1] = { uiModelSourceVerticesPositionIdx };
								EdgeGeomSpuVertexFormat edgeGeomSpuVertexFmt;
								V_memset( &edgeGeomSpuVertexFmt, 0, sizeof( edgeGeomSpuVertexFmt ) );
								edgeGeomSpuVertexFmt.m_numAttributes = 1;
								edgeGeomSpuVertexFmt.m_vertexStride = 3 * 16 / 8; // compressing to 16-bit
								{
									EdgeGeomSpuVertexAttributeDefinition &attr = edgeGeomSpuVertexFmt.m_attributeDefinition[0];
									attr.m_type = kSpuAttr_FixedPoint;
									attr.m_count = 3;
									attr.m_attributeId = EDGE_GEOM_ATTRIBUTE_ID_POSITION;
									attr.m_byteOffset = 0;
									attr.m_fixedPointBitDepthInteger[0] = 0;
									attr.m_fixedPointBitDepthInteger[1] = 0;
									attr.m_fixedPointBitDepthInteger[2] = 0;
									attr.m_fixedPointBitDepthFractional[0] = 16;
									attr.m_fixedPointBitDepthFractional[1] = 16;
									attr.m_fixedPointBitDepthFractional[2] = 16;
								}
								edgeGeomMakeSpuVertexBuffer( flModelSourceVertices, sizeof( mstudiovertex_t ) / sizeof( float ),
									edgeGeomAttributeIndexSpuVB, edgeGeomAttributeIdSpuVB, 1,
									egpoConsumable.m_originalVertexIndexesPerPartition, *egpoConsumable.m_numUniqueVertexesPerPartition,
									edgeGeomSpuVertexFmt, &ps3partition->m_pEdgeCompressedVtx, ps3partition->m_uiEdgeCompressedVtxDmaTagSize,
									&ps3partition->m_pEdgeCompressedVtxFixedOffsets, &ps3partition->m_uiEdgeCompressedVtxFixedOffsetsSize );
								ps3edgeGeomFreeMemoryTracker.AddToTail( ps3partition->m_pEdgeCompressedVtx );
								ps3edgeGeomFreeMemoryTracker.AddToTail( ps3partition->m_pEdgeCompressedVtxFixedOffsets );

								// Copy index buffer to partition data
								while ( ( *egpoConsumable.m_numTrianglesPerPartition ) -- > 0 )
								{
									for ( int kTriangleIndex = 0; kTriangleIndex < 3; ++ kTriangleIndex )
									{
										uint32 uiEgpoIndex = * ( egpoConsumable.m_triangleListOut ++ );
										if ( uiEgpoIndex > 0xFFFE )
										{
											Error( "PrepareModelForPs3: index in partition exceeding 0xFFFE! [%u]\n", uiEgpoIndex );
										}
										ps3partition->m_arrLocalIndices.AddToTail( uiEgpoIndex );
									}
								}
								egpoConsumable.m_numTrianglesPerPartition ++;

								ps3partition->m_arrVertOriginalIndices.EnsureCapacity( *egpoConsumable.m_numUniqueVertexesPerPartition );
								while ( ( *egpoConsumable.m_numUniqueVertexesPerPartition ) -- > 0 )
								{
									uint32 uiEgpoIndex = * ( egpoConsumable.m_originalVertexIndexesPerPartition ++ );
									ps3partition->m_arrVertOriginalIndices.AddToTail( uiEgpoIndex );
									ps3partition->m_arrStripLocalOriginalIndices.AddToTail( arrStripLocalEdgeInputIndex[uiEgpoIndex] );
								}
								egpoConsumable.m_numUniqueVertexesPerPartition ++;
							}

							//
							// Free EDGE memory
							//
							edgeGeomFree( pEdgeTriangleCentroids );
							edgeGeomFree( egpo.m_numTrianglesPerPartition );
							edgeGeomFree( egpo.m_triangleListOut );
							edgeGeomFree( egpo.m_originalVertexIndexesPerPartition );
							edgeGeomFree( egpo.m_numUniqueVertexesPerPartition );
							edgeGeomFree( egpo.m_ioBufferSizePerPartition );

						ITERATE_END
						numVhvOriginalModelVertices += vtxStripGroup->numVerts;

					ITERATE_END
				ITERATE_END
			ITERATE_END		
		ITERATE_END
	ITERATE_END

	}

	//
	// Now that we have partitions we need to extend index layouts of every strip
	// to account for special markup data describing the partitions layout.
	//
	uint32 numPartitions = 0, numVertices = 0, numEdgeDmaInputBytesTotal = 0;
	enum {
		kToolEdgeDmaAlignIdx = 16,
		kToolEdgeDmaAlignVtx = 16,
		kToolEdgeDmaAlignXfr = 16,
		kToolEdgeDmaAlignGlobal = (1<<8),
	};
	for ( int iBatch = 0; iBatch < ps3studioBatches.Count(); ++ iBatch )
	{
		CMdlStripInfo::Ps3studioBatch_t &batch = *ps3studioBatches[iBatch];
		numPartitions += batch.m_arrPartitions.Count();
		for ( int iPartition = 0; iPartition < batch.m_arrPartitions.Count(); ++ iPartition )
		{
			CMdlStripInfo::Ps3studioPartition_t &partition = *batch.m_arrPartitions[iPartition];
			numVertices += partition.m_arrVertOriginalIndices.Count();

			// Must be IDX-aligned
			numEdgeDmaInputBytesTotal = AlignValue( numEdgeDmaInputBytesTotal, kToolEdgeDmaAlignIdx );
			// Edge will need all indices
			uint32 numBytesIndexStream = bUncompressedIndices ? ( sizeof( uint16 ) * partition.m_arrLocalIndices.Count() ) : DmaTagsListSize2( partition.m_uiEdgeCompressedIdxDmaTagSize );
			numEdgeDmaInputBytesTotal += numBytesIndexStream;

			if ( !bUncompressedVertices && partition.m_pEdgeCompressedVtxFixedOffsets && partition.m_uiEdgeCompressedVtxFixedOffsetsSize )
			{
				// Must be VTX-aligned
				numEdgeDmaInputBytesTotal = AlignValue( numEdgeDmaInputBytesTotal, kToolEdgeDmaAlignVtx );
				numEdgeDmaInputBytesTotal += partition.m_uiEdgeCompressedVtxFixedOffsetsSize;
			}

			// Must be VTX-aligned
			numEdgeDmaInputBytesTotal = AlignValue( numEdgeDmaInputBytesTotal, kToolEdgeDmaAlignVtx );
			// Edge will need all positions
			uint32 numBytesVertexStream = bUncompressedVertices ? 3 * sizeof( float ) * partition.m_arrVertOriginalIndices.Count() : DmaTagsListSize3( partition.m_uiEdgeCompressedVtxDmaTagSize );
			numEdgeDmaInputBytesTotal += numBytesVertexStream;

			// Must be XFR-aligned
			numEdgeDmaInputBytesTotal = AlignValue( numEdgeDmaInputBytesTotal, kToolEdgeDmaAlignXfr );
		}
	}
	// Must be 256-byte aligned
	numEdgeDmaInputBytesTotal = AlignValue( numEdgeDmaInputBytesTotal, kToolEdgeDmaAlignGlobal );

	
	//
	// ===================
	// =================== Process MDL file
	// ===================
	//

	{

	int numCumulativeMeshVertices = 0;

	ITERATE_CHILDREN2( OptimizedModel::BodyPartHeader_t, mstudiobodyparts_t, vtxBodyPart, mdlBodyPart, vtxHdr, mdlHdr, pBodyPart, pBodypart, numBodyParts )
		ITERATE_CHILDREN2( OptimizedModel::ModelHeader_t, mstudiomodel_t, vtxModel, mdlModel, vtxBodyPart, mdlBodyPart, pModel, pModel, numModels )

			// need to update numvertices
			int update_studiomodel_numvertices = 0;
			mdlModel->vertexindex = numCumulativeMeshVertices * sizeof( mstudiovertex_t );
			mdlModel->tangentsindex = numCumulativeMeshVertices * sizeof( Vector4D );

			// Process each mesh separately (NOTE: loop reversed: MESH, then LOD)
			ITERATE_CHILDREN( mstudiomesh_t, mdlMesh, mdlModel, pMesh, nummeshes )

				// need to update numvertices
				int update_studiomesh_numvertices = 0;
				V_memset( mdlMesh->vertexdata.numLODVertexes, 0, sizeof( mdlMesh->vertexdata.numLODVertexes ) );

				ITERATE_CHILDREN( OptimizedModel::ModelLODHeader_t, vtxLod, vtxModel, pLOD, numLODs )
					if ( vtxLod->numMeshes <= mdlMesh_idx ) continue;
					OptimizedModel::MeshHeader_t *vtxMesh = vtxLod->pMesh( mdlMesh_idx );

					ITERATE_CHILDREN( OptimizedModel::StripGroupHeader_t, vtxStripGroup, vtxMesh, pStripGroup, numStripGroups )
						ITERATE_CHILDREN( OptimizedModel::StripHeader_t, vtxStrip, vtxStripGroup, pStrip, numStrips ) vtxStrip;

							CMdlStripInfo::Ps3studioBatch_t &batch = *ps3studioBatches[ mapMBI[ mapMBI.Find( ModelBatchKey_t( vtxBodyPart_idx, vtxModel_idx, vtxLod_idx, mdlMesh_idx, vtxStripGroup_idx, vtxStrip_idx ) ) ] ];
							int numBatchVertices = 0;
							for( int iPartition = 0; iPartition < batch.m_arrPartitions.Count(); ++ iPartition )
							{
								numBatchVertices += batch.m_arrPartitions[iPartition]->m_arrVertOriginalIndices.Count();
							}

							update_studiomesh_numvertices += numBatchVertices;
							mdlMesh->vertexdata.numLODVertexes[ vtxLod_idx ] += numBatchVertices;

						ITERATE_END
					ITERATE_END

				ITERATE_END

				mdlMesh->numvertices = update_studiomesh_numvertices;
				numCumulativeMeshVertices += update_studiomesh_numvertices;
				mdlMesh->vertexoffset = update_studiomodel_numvertices;
				update_studiomodel_numvertices += update_studiomesh_numvertices;

				// Adjust LOD vertex counts
				for ( int iRootLodVertexes = MAX_NUM_LODS; iRootLodVertexes -- > 1; )
				{
					mdlMesh->vertexdata.numLODVertexes[iRootLodVertexes-1] += mdlMesh->vertexdata.numLODVertexes[iRootLodVertexes];
				}
				for ( int iRootLodVertexes = 0; iRootLodVertexes < MAX_NUM_LODS; ++ iRootLodVertexes )
				{
					if ( !mdlMesh->vertexdata.numLODVertexes[iRootLodVertexes] && iRootLodVertexes )
						mdlMesh->vertexdata.numLODVertexes[iRootLodVertexes] = mdlMesh->vertexdata.numLODVertexes[iRootLodVertexes-1];
				}

			ITERATE_END
			
			mdlModel->numvertices = update_studiomodel_numvertices;

		ITERATE_END
	ITERATE_END

	}


	
	//
	// ===================
	// =================== Process VVD file
	// ===================
	//

	DECLARE_PTR( mstudiovertex_t, vvdVertexSrc, BYTE_OFF_PTR( vvdHdr, vvdHdr->vertexDataStart ) );
	DECLARE_PTR( Vector4D, vvdTangentSrc, vvdHdr->tangentDataStart ? BYTE_OFF_PTR( vvdHdr, vvdHdr->tangentDataStart ) : NULL );
	
	// Apply the fixups first of all
	if ( vvdHdr->numFixups )
	{
		CArrayAutoPtr< byte > memTempVVD( new byte[ vvdLength ] );
		DECLARE_PTR( mstudiovertex_t, vvdVertexNew, BYTE_OFF_PTR( memTempVVD.Get(), vvdHdr->vertexDataStart ) );
		DECLARE_PTR( Vector4D, vvdTangentNew, BYTE_OFF_PTR( memTempVVD.Get(), vvdHdr->tangentDataStart ) );
		DECLARE_PTR( vertexFileFixup_t, vvdFixup, BYTE_OFF_PTR( vvdHdr, vvdHdr->fixupTableStart ) );
		for ( int k = 0; k < vvdHdr->numFixups; ++ k )
		{
			memcpy( vvdVertexNew, vvdVertexSrc + vvdFixup[ k ].sourceVertexID, vvdFixup[ k ].numVertexes * sizeof( *vvdVertexNew ) );
			vvdVertexNew += vvdFixup[ k ].numVertexes;
			if ( vvdTangentSrc )
			{
				memcpy( vvdTangentNew, vvdTangentSrc + vvdFixup[ k ].sourceVertexID, vvdFixup[ k ].numVertexes * sizeof( *vvdTangentNew ) );
				vvdTangentNew += vvdFixup[ k ].numVertexes;
			}
		}

		// Move back the memory after fixups were applied
		vvdVertexSrc  ? memcpy( vvdVertexSrc, BYTE_OFF_PTR( memTempVVD.Get(), vvdHdr->vertexDataStart ), vvdHdr->numLODVertexes[0] * sizeof( *vvdVertexSrc ) ) : 0;
		vvdTangentSrc ? memcpy( vvdTangentSrc, BYTE_OFF_PTR( memTempVVD.Get(), vvdHdr->tangentDataStart ), vvdHdr->numLODVertexes[0] * sizeof( *vvdTangentSrc ) ) : 0;
	}
	
	vvdHdr->vertexDataStart -= ALIGN_VALUE( sizeof( vertexFileFixup_t ) * vvdHdr->numFixups, 16 );
	vvdHdr->numFixups = 0;

	if ( vvdHdr->id != MODEL_VERTEX_FILE_ID )
		Error( "PrepareModelForPs3: unsupported VVD structure!\n" );

	//
	// Now loop through batches and reorder vertices
	//
	{

	uint32 numNewVvdBufferBytesBase = vvdHdr->vertexDataStart + numVertices * sizeof( *vvdVertexSrc ) + ( vvdTangentSrc ? ( numVertices * sizeof( *vvdTangentSrc ) ) : 0 );
	numNewVvdBufferBytesBase = AlignValue( numNewVvdBufferBytesBase, 1 << 8 );
	uint32 numNewVvdBufferBytes = numNewVvdBufferBytesBase + numEdgeDmaInputBytesTotal;
	if ( ( numNewVvdBufferBytesBase >> 8 ) & ~0xFFFF )
		Error( "PrepareModelForPs3: VVD buffer DMA offset too large!\n" );
	if ( ( numEdgeDmaInputBytesTotal >> 8 ) & ~0x7FFF )
		Error( "PrepareModelForPs3: VVD buffer DMA size too large!\n" );
	if ( numEdgeDmaInputBytesTotal & 0xFF )
		Error( "PrepareModelForPs3: VVD buffer DMA size incorrect!\n" );
	vvdHdr->fixupTableStart = ( numNewVvdBufferBytesBase >> 8 ) | ( numEdgeDmaInputBytesTotal << 8 ) | 0x80000000;
	CArrayAutoPtr< byte > memTempVVD( new byte[ numNewVvdBufferBytes ] );
	for ( int k = 0; k < MAX_NUM_LODS; ++ k )
		vvdHdr->numLODVertexes[k] = numVertices;	// Root LOD unsupported
	V_memcpy( memTempVVD.Get(), vvdHdr, vvdHdr->vertexDataStart );
	DECLARE_PTR( mstudiovertex_t, vvdVertexNew, BYTE_OFF_PTR( memTempVVD.Get(), vvdHdr->vertexDataStart ) );
	DECLARE_PTR( Vector4D, vvdTangentNew, BYTE_OFF_PTR( memTempVVD.Get(), vvdHdr->vertexDataStart + numVertices * sizeof( *vvdVertexSrc ) ) );
	DECLARE_PTR( uint8, vvdEdgeDmaInputNew, BYTE_OFF_PTR( memTempVVD.Get(), numNewVvdBufferBytesBase ) );

	ITERATE_CHILDREN2( OptimizedModel::BodyPartHeader_t, mstudiobodyparts_t, vtxBodyPart, mdlBodyPart, vtxHdr, mdlHdr, pBodyPart, pBodypart, numBodyParts )
		ITERATE_CHILDREN2( OptimizedModel::ModelHeader_t, mstudiomodel_t, vtxModel, mdlModel, vtxBodyPart, mdlBodyPart, pModel, pModel, numModels )
			// Process each mesh separately (NOTE: loop reversed: MESH, then LOD)
			ITERATE_CHILDREN( mstudiomesh_t, mdlMesh, mdlModel, pMesh, nummeshes ) mdlMesh;
				ITERATE_CHILDREN( OptimizedModel::ModelLODHeader_t, vtxLod, vtxModel, pLOD, numLODs )
					if ( vtxLod->numMeshes <= mdlMesh_idx ) continue;
					OptimizedModel::MeshHeader_t *vtxMesh = vtxLod->pMesh( mdlMesh_idx );
					ITERATE_CHILDREN( OptimizedModel::StripGroupHeader_t, vtxStripGroup, vtxMesh, pStripGroup, numStripGroups )
						ITERATE_CHILDREN( OptimizedModel::StripHeader_t, vtxStrip, vtxStripGroup, pStrip, numStrips ) vtxStrip;

							CMdlStripInfo::Ps3studioBatch_t &batch = *ps3studioBatches[ mapMBI[ mapMBI.Find( ModelBatchKey_t( vtxBodyPart_idx, vtxModel_idx, vtxLod_idx, mdlMesh_idx, vtxStripGroup_idx, vtxStrip_idx ) ) ] ];
							for( int iPartition = 0; iPartition < batch.m_arrPartitions.Count(); ++ iPartition )
							{
								CMdlStripInfo::Ps3studioPartition_t &partition = *batch.m_arrPartitions[iPartition];
								
								// Must be IDX-aligned
								DECLARE_UPDATE_PTR( uint8, vvdEdgeDmaInputNew, BYTE_OFF_PTR( memTempVVD.Get(), AlignValue( BYTE_DIFF_PTR( memTempVVD.Get(), vvdEdgeDmaInputNew ), kToolEdgeDmaAlignIdx ) ) );
								partition.m_nEdgeDmaInputIdx = BYTE_DIFF_PTR( memTempVVD.Get(), vvdEdgeDmaInputNew ) - numNewVvdBufferBytesBase;
								
								// EDGE DMA INDEX DATA
								if ( bUncompressedIndices )
								{
									ps3byteswap.SwapBufferToTargetEndian<uint16>( (uint16*) vvdEdgeDmaInputNew,
										partition.m_arrLocalIndices.Base(), partition.m_arrLocalIndices.Count() );
									vvdEdgeDmaInputNew += partition.m_arrLocalIndices.Count() * sizeof( uint16 );
								}
								else
								{
									V_memcpy( vvdEdgeDmaInputNew, partition.m_pEdgeCompressedIdx, DmaTagsListSize2( partition.m_uiEdgeCompressedIdxDmaTagSize ) );
									vvdEdgeDmaInputNew += DmaTagsListSize2( partition.m_uiEdgeCompressedIdxDmaTagSize );
								}

								// Must be VTX-aligned
								DECLARE_UPDATE_PTR( uint8, vvdEdgeDmaInputNew, BYTE_OFF_PTR( memTempVVD.Get(), AlignValue( BYTE_DIFF_PTR( memTempVVD.Get(), vvdEdgeDmaInputNew ), kToolEdgeDmaAlignVtx ) ) );
								partition.m_nEdgeDmaInputVtx = BYTE_DIFF_PTR( memTempVVD.Get(), vvdEdgeDmaInputNew ) - numNewVvdBufferBytesBase;

								for ( int iOrigVert = 0; iOrigVert < partition.m_arrVertOriginalIndices.Count(); ++ iOrigVert )
								{
									uint32 uiOrigVertIdx = partition.m_arrVertOriginalIndices[iOrigVert];
									uiOrigVertIdx += batch.m_uiModelIndexOffset;
									
									memcpy( vvdVertexNew, vvdVertexSrc + uiOrigVertIdx, sizeof( *vvdVertexNew ) );
									++ vvdVertexNew;
									
									if ( vvdTangentSrc )
									{
										memcpy( vvdTangentNew, vvdTangentSrc + uiOrigVertIdx, sizeof( *vvdTangentNew ) );
										++ vvdTangentNew;
									}

									// EDGE DMA VERTEX DATA
									if ( bUncompressedVertices )
									{
										ps3byteswap.SwapBufferToTargetEndian<float>( (float*) vvdEdgeDmaInputNew,
											(float*) &vvdVertexSrc[uiOrigVertIdx].m_vecPosition.x, 3 );
										vvdEdgeDmaInputNew += 3 * sizeof( float );
									}
								}

								// EDGE DMA VERTEX DATA
								if ( !bUncompressedVertices )
								{
									if ( partition.m_pEdgeCompressedVtxFixedOffsets && partition.m_uiEdgeCompressedVtxFixedOffsetsSize )
									{
										V_memcpy( vvdEdgeDmaInputNew, partition.m_pEdgeCompressedVtxFixedOffsets, partition.m_uiEdgeCompressedVtxFixedOffsetsSize );
										vvdEdgeDmaInputNew += partition.m_uiEdgeCompressedVtxFixedOffsetsSize;
										DECLARE_UPDATE_PTR( uint8, vvdEdgeDmaInputNew, BYTE_OFF_PTR( memTempVVD.Get(), AlignValue( BYTE_DIFF_PTR( memTempVVD.Get(), vvdEdgeDmaInputNew ), kToolEdgeDmaAlignVtx ) ) );
									}
									V_memcpy( vvdEdgeDmaInputNew, partition.m_pEdgeCompressedVtx, DmaTagsListSize3( partition.m_uiEdgeCompressedVtxDmaTagSize ) );
									vvdEdgeDmaInputNew += DmaTagsListSize3( partition.m_uiEdgeCompressedVtxDmaTagSize );
								}

								// Must be XFR-aligned
								DECLARE_UPDATE_PTR( uint8, vvdEdgeDmaInputNew, BYTE_OFF_PTR( memTempVVD.Get(), AlignValue( BYTE_DIFF_PTR( memTempVVD.Get(), vvdEdgeDmaInputNew ), kToolEdgeDmaAlignXfr ) ) );
								partition.m_nEdgeDmaInputEnd = BYTE_DIFF_PTR( memTempVVD.Get(), vvdEdgeDmaInputNew ) - numNewVvdBufferBytesBase;
								Assert( vvdEdgeDmaInputNew <= BYTE_OFF_PTR( memTempVVD.Get(), numNewVvdBufferBytes ) );
							}
						ITERATE_END
					ITERATE_END
				ITERATE_END
			ITERATE_END	
		ITERATE_END
	ITERATE_END

	//
	// Copy back the newly created VVD buffer
	//
	vvdBuffer.EnsureCapacity( numNewVvdBufferBytes + vvdBuffer.TellGet() );
	V_memcpy( BYTE_OFF_PTR( vvdBuffer.Base(), vvdBuffer.TellGet() ), memTempVVD.Get(), numNewVvdBufferBytes );
	vvdBuffer.SeekPut( vvdBuffer.SEEK_HEAD, vvdBuffer.TellGet() + numNewVvdBufferBytes );

	Log_Msg( LOG_ModelLib, " Increased VVD size by %d bytes.\n", numNewVvdBufferBytes - vvdLength );
	
	// Since we could have caused a buffer reallocation, update cached pointers and values
	DECLARE_UPDATE_PTR( byte, vvd, BYTE_OFF_PTR( vvdBuffer.Base(), vvdBuffer.TellGet() ) );
	DECLARE_UPDATE_PTR( vertexFileHeader_t, vvdHdr, vvd );
	DECLARE_UPDATE_PTR( mstudiovertex_t, vvdVertexSrc, BYTE_OFF_PTR( vvdHdr, vvdHdr->vertexDataStart ) );
	if ( vvdTangentSrc )
	{
		vvdHdr->tangentDataStart = vvdHdr->vertexDataStart + numVertices * sizeof( *vvdVertexSrc );
		DECLARE_UPDATE_PTR( Vector4D, vvdTangentSrc, BYTE_OFF_PTR( vvdHdr, vvdHdr->tangentDataStart ) );
	}
	vvdLength = numNewVvdBufferBytes;

	}
	
	//
	// ===================
	// =================== Process VTX file
	// ===================
	//

	size_t vtxOffIndexBuffer = ~size_t(0), vtxOffIndexBufferEnd = 0;
	size_t vtxOffVertexBuffer = ~size_t(0), vtxOffVertexBufferEnd = 0;
	CMemoryMovingTracker vtxMemMove( CMemoryMovingTracker::MEMORY_MODIFY );

	{

	ITERATE_CHILDREN( OptimizedModel::BodyPartHeader_t, vtxBodyPart, vtxHdr, pBodyPart, numBodyParts )
		ITERATE_CHILDREN( OptimizedModel::ModelHeader_t, vtxModel, vtxBodyPart, pModel, numModels )
			ITERATE_CHILDREN( OptimizedModel::ModelLODHeader_t, vtxLod, vtxModel, pLOD, numLODs )
				ITERATE_CHILDREN( OptimizedModel::MeshHeader_t, vtxMesh, vtxLod, pMesh, numMeshes )
					ITERATE_CHILDREN( OptimizedModel::StripGroupHeader_t, vtxStripGroup, vtxMesh, pStripGroup, numStripGroups )

						size_t offIndex = BYTE_DIFF_PTR( vtxHdr, CHILD_AT( vtxStripGroup, pIndex, 0 ) );
						size_t offIndexEnd = BYTE_DIFF_PTR( vtxHdr, CHILD_AT( vtxStripGroup, pIndex, vtxStripGroup->numIndices ) );
						size_t offVertex = BYTE_DIFF_PTR( vtxHdr, CHILD_AT( vtxStripGroup, pVertex, 0 ) );
						size_t offVertexEnd = BYTE_DIFF_PTR( vtxHdr, CHILD_AT( vtxStripGroup, pVertex, vtxStripGroup->numVerts ) );

						if ( offIndex < vtxOffIndexBuffer )
							vtxOffIndexBuffer = offIndex;
						if ( offIndexEnd > vtxOffIndexBufferEnd )
							vtxOffIndexBufferEnd = offIndexEnd;
						if ( offVertex < vtxOffVertexBuffer )
							vtxOffVertexBuffer = offVertex;
						if ( offVertexEnd > vtxOffVertexBufferEnd )
							vtxOffVertexBufferEnd = offVertexEnd;

						uint32 uiNumVertsInStripGroupUpdate = 0;
						uint32 uiNumIndicesInStripGroupUpdate = 0;

						ITERATE_CHILDREN( OptimizedModel::StripHeader_t, vtxStrip, vtxStripGroup, pStrip, numStrips )
							
							CMdlStripInfo::Ps3studioBatch_t &batch = *ps3studioBatches[ mapMBI[ mapMBI.Find( ModelBatchKey_t( vtxBodyPart_idx, vtxModel_idx, vtxLod_idx, vtxMesh_idx, vtxStripGroup_idx, vtxStrip_idx ) ) ] ];
							uint16 *vtxIdx = CHILD_AT( vtxStripGroup, pIndex, vtxStrip->indexOffset );
							
							uint32 uiNumExtraBytes = sizeof( OptimizedModel::OptimizedIndexBufferMarkupPs3_t ) +
								batch.m_arrPartitions.Count() * sizeof( OptimizedModel::OptimizedIndexBufferMarkupPs3_t::Partition_t );
							uiNumExtraBytes = ( ( uiNumExtraBytes + 5 ) / 6 ) * 6; // make it even number of 3 indices
							
							vtxStrip->numVerts = 0; // Since we are not adjusting the verts here properly, zero them out

							uint32 uiNumIndicesInPartitions = 0;
							for ( int k = 0; k < batch.m_arrPartitions.Count(); ++ k )
							{
								uiNumIndicesInPartitions += batch.m_arrPartitions[k]->m_arrLocalIndices.Count();
								uiNumVertsInStripGroupUpdate += batch.m_arrPartitions[k]->m_arrVertOriginalIndices.Count();
							}

							uiNumIndicesInStripGroupUpdate += uiNumExtraBytes / sizeof( uint16 ) + ( bEmitIndices ? uiNumIndicesInPartitions : 0 );
							vtxMemMove.RegisterBytes( vtxIdx, uiNumExtraBytes - ( bEmitIndices ? 0 : ( uiNumIndicesInPartitions * sizeof( uint16 ) ) ) );

						ITERATE_END

						vtxMemMove.RegisterBytes( vtxStripGroup->pVertex(0), ( uiNumVertsInStripGroupUpdate - vtxStripGroup->numVerts ) * sizeof( *vtxStripGroup->pVertex(0) ) );
						
						vtxStripGroup->numIndices = uiNumIndicesInStripGroupUpdate;
						vtxStripGroup->numVerts = uiNumVertsInStripGroupUpdate;

					ITERATE_END
				ITERATE_END
			ITERATE_END
		ITERATE_END
	ITERATE_END

	}

	//
	// Now enlarge the VTX buffer
	//
	vtxBuffer.EnsureCapacity( vtxBuffer.TellGet() + vtxLength + vtxMemMove.GetNumBytesRegistered() );
	vtxBuffer.SeekPut( vtxBuffer.SEEK_HEAD, vtxBuffer.TellGet() + vtxLength + vtxMemMove.GetNumBytesRegistered() );

	DECLARE_UPDATE_PTR( byte, vtx, BYTE_OFF_PTR( vtxBuffer.Base(), vtxBuffer.TellGet() ) );
	vtxMemMove.RegisterBaseDelta( vtxHdr, vtx );
	DECLARE_UPDATE_PTR( OptimizedModel::FileHeader_t, vtxHdr, vtx );
	vtxMemMove.Finalize();

	// By now should have scheduled all memmove information
	Log_Msg( LOG_ModelLib, " Increased VTX size by %d bytes.\n", vtxMemMove.GetNumBytesRegistered() );

	//
	// Fixup all the offsets
	//
	ITERATE_CHILDREN( OptimizedModel::MaterialReplacementListHeader_t, vtxMatList, vtxHdr, pMaterialReplacementList, numLODs )
		ITERATE_CHILDREN( OptimizedModel::MaterialReplacementHeader_t, vtxMat, vtxMatList, pMaterialReplacement, numReplacements )
			vtxMat->replacementMaterialNameOffset = vtxMemMove.ComputeOffset( vtxMat, vtxMat->replacementMaterialNameOffset );
		ITERATE_END
		vtxMatList->replacementOffset = vtxMemMove.ComputeOffset( vtxMatList, vtxMatList->replacementOffset );
	ITERATE_END
	ITERATE_CHILDREN( OptimizedModel::BodyPartHeader_t, vtxBodyPart, vtxHdr, pBodyPart, numBodyParts )
		ITERATE_CHILDREN( OptimizedModel::ModelHeader_t, vtxModel, vtxBodyPart, pModel, numModels )
			ITERATE_CHILDREN( OptimizedModel::ModelLODHeader_t, vtxLod, vtxModel, pLOD, numLODs )
				ITERATE_CHILDREN( OptimizedModel::MeshHeader_t, vtxMesh, vtxLod, pMesh, numMeshes )
					ITERATE_CHILDREN( OptimizedModel::StripGroupHeader_t, vtxStripGroup, vtxMesh, pStripGroup, numStripGroups )
						
						ITERATE_CHILDREN( OptimizedModel::StripHeader_t, vtxStrip, vtxStripGroup, pStrip, numStrips )
							vtxStrip->indexOffset =
								vtxMemMove.ComputeOffset( vtxStripGroup, vtxStripGroup->indexOffset + vtxStrip->indexOffset ) -
								vtxMemMove.ComputeOffset( vtxStripGroup, vtxStripGroup->indexOffset );
							vtxStrip->vertOffset =
								vtxMemMove.ComputeOffset( vtxStripGroup, vtxStripGroup->vertOffset + vtxStrip->vertOffset ) -
								vtxMemMove.ComputeOffset( vtxStripGroup, vtxStripGroup->vertOffset );
							vtxStrip->boneStateChangeOffset = vtxMemMove.ComputeOffset( vtxStrip, vtxStrip->boneStateChangeOffset );
						ITERATE_END

						vtxStripGroup->vertOffset = vtxMemMove.ComputeOffset( vtxStripGroup, vtxStripGroup->vertOffset );
						vtxStripGroup->indexOffset = vtxMemMove.ComputeOffset( vtxStripGroup, vtxStripGroup->indexOffset );
						vtxStripGroup->stripOffset = vtxMemMove.ComputeOffset( vtxStripGroup, vtxStripGroup->stripOffset );
						vtxStripGroup->topologyOffset = vtxMemMove.ComputeOffset( vtxStripGroup, vtxStripGroup->topologyOffset );
					ITERATE_END
					vtxMesh->stripGroupHeaderOffset = vtxMemMove.ComputeOffset( vtxMesh, vtxMesh->stripGroupHeaderOffset );
				ITERATE_END
				vtxLod->meshOffset = vtxMemMove.ComputeOffset( vtxLod, vtxLod->meshOffset );
			ITERATE_END
			vtxModel->lodOffset = vtxMemMove.ComputeOffset( vtxModel, vtxModel->lodOffset );
		ITERATE_END
		vtxBodyPart->modelOffset = vtxMemMove.ComputeOffset( vtxBodyPart, vtxBodyPart->modelOffset );
	ITERATE_END
	vtxHdr->materialReplacementListOffset = vtxMemMove.ComputeOffset( vtxHdr, vtxHdr->materialReplacementListOffset );
	vtxHdr->bodyPartOffset = vtxMemMove.ComputeOffset( vtxHdr, vtxHdr->bodyPartOffset );

	// Perform final memory move
	vtxMemMove.MemMove( vtxHdr, vtxLength );
	vtxBuffer.SeekPut( vtxBuffer.SEEK_HEAD, vtxBuffer.TellGet() + vtxLength );

	//
	// Fill the newly insterted gaps with real data
	//
	{

	OptimizedModel::Vertex_t optVertex;
	for ( int j = 0; j < MAX_NUM_BONES_PER_VERT; ++ j )
		optVertex.boneWeightIndex[j] = j, optVertex.boneID[j] = 0;
	optVertex.numBones = 1; // we are processing only 1-bone models
	uint32 uiEdgeDmaInputOffsetPerStripGroup = 0;

	ITERATE_CHILDREN2( OptimizedModel::BodyPartHeader_t, mstudiobodyparts_t, vtxBodyPart, mdlBodyPart, vtxHdr, mdlHdr, pBodyPart, pBodypart, numBodyParts )
		ITERATE_CHILDREN2( OptimizedModel::ModelHeader_t, mstudiomodel_t, vtxModel, mdlModel, vtxBodyPart, mdlBodyPart, pModel, pModel, numModels )
			
			uint32 uiRunningOriginalVertexId = 0;
			// Process each mesh separately (NOTE: loop reversed: MESH, then LOD)
			ITERATE_CHILDREN( mstudiomesh_t, mdlMesh, mdlModel, pMesh, nummeshes )
				ITERATE_CHILDREN( OptimizedModel::ModelLODHeader_t, vtxLod, vtxModel, pLOD, numLODs )
					if ( vtxLod->numMeshes <= mdlMesh_idx ) continue;
					OptimizedModel::MeshHeader_t *vtxMesh = vtxLod->pMesh( mdlMesh_idx );
					
					ITERATE_CHILDREN( OptimizedModel::StripGroupHeader_t, vtxStripGroup, vtxMesh, pStripGroup, numStripGroups )

						uint32 uiStipGroupFilledVerts = 0;
						uint32 uiRunningEdgeDmaInputEnd = uiEdgeDmaInputOffsetPerStripGroup;
						CUtlVector< OptimizedModel::OptimizedIndexBufferMarkupPs3_t * > arrGroupMarkups;

						ITERATE_CHILDREN( OptimizedModel::StripHeader_t, vtxStrip, vtxStripGroup, pStrip, numStrips )
							
							CMdlStripInfo::Ps3studioBatch_t &batch = *ps3studioBatches[ mapMBI[ mapMBI.Find( ModelBatchKey_t( vtxBodyPart_idx, vtxModel_idx, vtxLod_idx, mdlMesh_idx, vtxStripGroup_idx, vtxStrip_idx ) ) ] ];
							uint16 *vtxIdx = CHILD_AT( vtxStripGroup, pIndex, vtxStrip->indexOffset );
							
							uint32 uiNumExtraBytes = sizeof( OptimizedModel::OptimizedIndexBufferMarkupPs3_t ) +
								batch.m_arrPartitions.Count() * sizeof( OptimizedModel::OptimizedIndexBufferMarkupPs3_t::Partition_t );
							uiNumExtraBytes = ( ( uiNumExtraBytes + 5 ) / 6 ) * 6; // make it even number of 3 indices

							// Write the "markup" header
							DECLARE_PTR( OptimizedModel::OptimizedIndexBufferMarkupPs3_t, pMarkup, vtxIdx );
							arrGroupMarkups.AddToTail( pMarkup );
							pMarkup->m_uiHeaderCookie = pMarkup->kHeaderCookie;
							pMarkup->m_uiVersionFlags = pMarkup->kVersion1;
							pMarkup->m_numBytesMarkup = uiNumExtraBytes;
							pMarkup->m_numPartitions = batch.m_arrPartitions.Count();
							pMarkup->m_numIndicesTotal = 0;
							pMarkup->m_numVerticesTotal = 0;
							pMarkup->m_nEdgeDmaInputOffsetPerStripGroup = uiEdgeDmaInputOffsetPerStripGroup; // updated later
							pMarkup->m_nEdgeDmaInputSizePerStripGroup = 0; // filled later
							
							uint32 uiSkipIdx = uiNumExtraBytes / sizeof( uint16 ), uiSkipVerts = 0;
							for ( int k = 0; k < batch.m_arrPartitions.Count(); ++ k )
							{
								CMdlStripInfo::Ps3studioPartition_t &partition = *batch.m_arrPartitions[k];
								pMarkup->m_partitions[k].m_numIndicesToSkipInIBs = uiSkipIdx;
								pMarkup->m_partitions[k].m_numVerticesToSkipInVBs = uiSkipVerts;
								pMarkup->m_partitions[k].m_nIoBufferSize = partition.m_nIoBufferSize;
								pMarkup->m_partitions[k].m_numIndices = partition.m_arrLocalIndices.Count();
								pMarkup->m_partitions[k].m_numVertices = partition.m_arrVertOriginalIndices.Count();
								
								if ( uiRunningEdgeDmaInputEnd == uiEdgeDmaInputOffsetPerStripGroup )
								{
									pMarkup->m_nEdgeDmaInputOffsetPerStripGroup =
										uiEdgeDmaInputOffsetPerStripGroup =
										partition.m_nEdgeDmaInputIdx;
								}
								pMarkup->m_partitions[k].m_nEdgeDmaInputIdx = partition.m_nEdgeDmaInputIdx - uiEdgeDmaInputOffsetPerStripGroup;
								pMarkup->m_partitions[k].m_nEdgeDmaInputVtx = partition.m_nEdgeDmaInputVtx - uiEdgeDmaInputOffsetPerStripGroup;
								pMarkup->m_partitions[k].m_nEdgeDmaInputEnd = partition.m_nEdgeDmaInputEnd - uiEdgeDmaInputOffsetPerStripGroup;
								uiRunningEdgeDmaInputEnd = partition.m_nEdgeDmaInputEnd;

								pMarkup->m_numIndicesTotal += pMarkup->m_partitions[k].m_numIndices;
								pMarkup->m_numVerticesTotal += pMarkup->m_partitions[k].m_numVertices;

								if ( bEmitIndices )
								{
									//
									// Write raw partition indices
									//
									for ( int j = 0; j < partition.m_arrLocalIndices.Count(); ++ j )
									{
										vtxIdx[ uiSkipIdx + j ] = partition.m_arrLocalIndices[j];
									}
									uiSkipIdx += partition.m_arrLocalIndices.Count();
								}

								//
								// Advance written verts
								//
								uiSkipVerts += partition.m_arrVertOriginalIndices.Count();
							}
							vtxStrip->numIndices = uiSkipIdx;

							// Fill out our Vertex_t structures
							bool bWarningSpewed = false;
							if ( uiSkipVerts > vtxStripGroup->numVerts )
							{
								Warning( "WARNING: %s: VTX strip group overflow %d/%d\n", mdlHdr->pszName(), uiSkipVerts, vtxStripGroup->numVerts );
								Assert( !"VTX strip group overflow!" );
								bWarningSpewed = true;
							}
							for ( int j = 0; j < uiSkipVerts; ++ j )
							{
								optVertex.origMeshVertID = uiRunningOriginalVertexId - mdlMesh->vertexoffset;
								if ( !bWarningSpewed && (
									( uiRunningOriginalVertexId < mdlMesh->vertexoffset ) ||
									( uint32( optVertex.origMeshVertID ) != uint32( uiRunningOriginalVertexId - mdlMesh->vertexoffset ) ) ||
									( mdlMesh->numvertices <= optVertex.origMeshVertID )
									) )
								{
									Warning( "WARNING: %s: VTX vertex indirection overflow id=%d, off=%d, idx=%d/%d\n", mdlHdr->pszName(), uiRunningOriginalVertexId, mdlMesh->vertexoffset, uint32( optVertex.origMeshVertID ), mdlMesh->numvertices );
									Assert( !"VTX vertex indirection overflow!" );
									bWarningSpewed = true;
								}
								uiRunningOriginalVertexId ++;
								V_memcpy( vtxStripGroup->pVertex( uiStipGroupFilledVerts + j ), &optVertex, sizeof( optVertex ) );
							}
							uiStipGroupFilledVerts += uiSkipVerts;

						ITERATE_END
						for ( int jjj = 0; jjj < arrGroupMarkups.Count(); ++ jjj )
						{
							arrGroupMarkups[jjj]->m_nEdgeDmaInputSizePerStripGroup = uiRunningEdgeDmaInputEnd - uiEdgeDmaInputOffsetPerStripGroup;
							
							// Need pre-swapping inside index buffer
							Helper_SwapOptimizedIndexBufferMarkup_forPs3( arrGroupMarkups[jjj] );
						}
						uiEdgeDmaInputOffsetPerStripGroup = uiRunningEdgeDmaInputEnd;

					ITERATE_END
				ITERATE_END
			ITERATE_END
		ITERATE_END
	ITERATE_END

	}


	//
	// ===================
	// =================== Validate that we have valid information for VHV processing
	// ===================
	//
	{

	// Keep track of which verts got touched
	CGrowableBitVec arrTouchedOriginalVerts;
	uint32 uiDebugOriginalVertsPresent = numVhvOriginalModelVertices;
	for ( uint32 iMesh = 0, iBatch = 0; iMesh < pMsi->m_ps3studioStripGroupHeaderBatchOffset.Count(); ++ iMesh )
	{
		uint32 numVerts = 0;
		uint32 iBatchEnd = ( iMesh < pMsi->m_ps3studioStripGroupHeaderBatchOffset.Count() - 1 )
			? pMsi->m_ps3studioStripGroupHeaderBatchOffset[iMesh+1] : pMsi->m_ps3studioBatches.Count();
		iBatchEnd = MIN( iBatchEnd, pMsi->m_ps3studioBatches.Count() );
		for ( ; iBatch < iBatchEnd; ++ iBatch )
		{
			CMdlStripInfo::Ps3studioBatch_t &batch = *pMsi->m_ps3studioBatches[iBatch];
			for ( uint32 iPartition = 0; iPartition < batch.m_arrPartitions.Count(); ++ iPartition )
			{
				CMdlStripInfo::Ps3studioPartition_t &partition = *batch.m_arrPartitions[iPartition];
				numVerts += partition.m_arrVertOriginalIndices.Count();
				for ( uint32 iVertIndex = 0; iVertIndex < partition.m_arrVertOriginalIndices.Count(); ++ iVertIndex )
				{
					// uint32 uiOrigVertIndex = partition.m_arrVertOriginalIndices[iVertIndex];
					uint32 uiOrigVertIndex = partition.m_arrStripLocalOriginalIndices[iVertIndex];
					uiOrigVertIndex += batch.m_uiVhvIndexOffset;
					arrTouchedOriginalVerts.GrowSetBit( uiOrigVertIndex );
				}
			}
		}
	}
	{
		uint32 uiDebugTouchedOriginalVerts = arrTouchedOriginalVerts.GetNumBits();
		if ( uiDebugTouchedOriginalVerts == uiDebugOriginalVertsPresent )
		{
			for ( uint32 iDebugOrigVert = 0; iDebugOrigVert < uiDebugOriginalVertsPresent; ++ iDebugOrigVert )
			{
				if ( !arrTouchedOriginalVerts.IsBitSet( iDebugOrigVert ) )
				{
					Warning( "WARNING: %s: VHV source vertex %d/%d skipped\n", mdlHdr->pszName(), iDebugOrigVert, uiDebugOriginalVertsPresent );
					Assert( !"VHV source vertex skipped!" );
					break;
				}
			}
		}
		else
		{
			Warning( "WARNING: %s: VHV expected vertex count mismatch %d!=%d\n", mdlHdr->pszName(), uiDebugTouchedOriginalVerts, uiDebugOriginalVertsPresent );
			Assert( !"VHV expected vertex count mismatch!" );
		}
	}

	}

	// Done
	return true;
}

/*

If the changelist comment includes the following line:

*** force rebuild *** 

then it will trigger a full content rebuild which is very
useful when changing output formats.

*/

