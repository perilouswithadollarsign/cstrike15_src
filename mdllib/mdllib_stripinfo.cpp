//====== Copyright c 1996-2007, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
//=============================================================================//

#include "mdllib_common.h"
#include "mdllib_stripinfo.h"
#include "mdllib_utils.h"

#include "studio.h"
#include "optimize.h"

#include "materialsystem/imaterial.h"
#include "materialsystem/hardwareverts.h"

#include "smartptr.h"



//////////////////////////////////////////////////////////////////////////
//
// CMdlStripInfo implementation
//
//////////////////////////////////////////////////////////////////////////


CMdlStripInfo::CMdlStripInfo() :
	m_eMode( MODE_UNINITIALIZED ),
	m_lChecksumOld( 0 ),
	m_lChecksumNew( 0 )
{
	NULL;
}


bool CMdlStripInfo::Serialize( CUtlBuffer &bufStorage ) const
{
	char chHeader[ 4 ] = { 'M', 'A', 'P', m_eMode };
	bufStorage.Put( chHeader, sizeof( chHeader ) );

	switch ( m_eMode )
	{
	default:
	case MODE_UNINITIALIZED:
		return true;
	
	case MODE_NO_CHANGE:
	case MODE_PS3_FORMAT_BASIC:
		bufStorage.PutInt( m_lChecksumOld );
		bufStorage.PutInt( m_lChecksumNew );
		return true;

	case MODE_STRIP_LOD_1N:
		bufStorage.PutInt( m_lChecksumOld );
		bufStorage.PutInt( m_lChecksumNew );

		bufStorage.PutInt( m_vtxVerts.GetNumBits() );
		for ( uint32 const *pdwBase = m_vtxVerts.Base(), *pdwEnd = pdwBase + m_vtxVerts.GetNumDWords();
			pdwBase < pdwEnd; ++ pdwBase )
			bufStorage.PutUnsignedInt( *pdwBase );

		bufStorage.PutInt( m_vtxIndices.Count() );
		for ( unsigned short const *pusBase = m_vtxIndices.Base(), *pusEnd = pusBase + m_vtxIndices.Count();
			pusBase < pusEnd; ++ pusBase )
			bufStorage.PutUnsignedShort( *pusBase );

		bufStorage.PutInt( m_vtxMdlOffsets.Count() );
		for ( MdlRangeItem const *pmri = m_vtxMdlOffsets.Base(), *pmriEnd = pmri + m_vtxMdlOffsets.Count();
			pmri < pmriEnd; ++ pmri )
			bufStorage.PutInt( pmri->m_offOld ),
			bufStorage.PutInt( pmri->m_offNew ),
			bufStorage.PutInt( pmri->m_numOld ),
			bufStorage.PutInt( pmri->m_numNew );

		return true;

	case MODE_PS3_PARTITIONS:
		bufStorage.PutInt( m_lChecksumOld );
		bufStorage.PutInt( m_lChecksumNew );
		
		bufStorage.PutUnsignedInt( m_ps3studioBatches.Count() );
		for ( int k = 0; k < m_ps3studioBatches.Count(); ++ k )
		{
			Ps3studioBatch_t &batch = *m_ps3studioBatches[k];
			bufStorage.PutUnsignedInt( batch.m_arrPartitions.Count() );
			for ( int j = 0; j < batch.m_arrPartitions.Count(); ++ j )
			{
				Ps3studioPartition_t &partition = *batch.m_arrPartitions[j];
				bufStorage.PutUnsignedInt( partition.m_arrLocalIndices.Count() );
				for ( int nn = 0; nn < partition.m_arrLocalIndices.Count(); ++ nn )
				{
					bufStorage.PutUnsignedShort( partition.m_arrLocalIndices[nn] );
				}
				bufStorage.PutUnsignedInt( partition.m_arrVertOriginalIndices.Count() );
				for ( int nn = 0; nn < partition.m_arrVertOriginalIndices.Count(); ++ nn )
				{
					bufStorage.PutUnsignedInt( partition.m_arrVertOriginalIndices[nn] );
				}
				bufStorage.PutUnsignedInt( partition.m_arrStripLocalOriginalIndices.Count() );
				for ( int nn = 0; nn < partition.m_arrStripLocalOriginalIndices.Count(); ++ nn )
				{
					bufStorage.PutUnsignedInt( partition.m_arrStripLocalOriginalIndices[nn] );
				}
				bufStorage.PutUnsignedInt( partition.m_nIoBufferSize );
			}
			bufStorage.PutUnsignedInt( batch.m_uiModelIndexOffset );
			bufStorage.PutUnsignedInt( batch.m_uiVhvIndexOffset );
		}

		bufStorage.PutUnsignedInt( m_ps3studioStripGroupHeaderBatchOffset.Count() );
		for ( int k = 0; k < m_ps3studioStripGroupHeaderBatchOffset.Count(); ++ k )
		{
			bufStorage.PutUnsignedInt( m_ps3studioStripGroupHeaderBatchOffset[k] );
		}
		return true;
	}
}

bool CMdlStripInfo::UnSerialize( CUtlBuffer &bufData )
{
	char chHeader[ 4 ];
	bufData.Get( chHeader, sizeof( chHeader ) );

	if ( memcmp( chHeader, "MAP", 3 ) )
		return false;

	m_eMode = chHeader[3];
	switch ( chHeader[3] )
	{
	default:
		return false;
	
	case MODE_UNINITIALIZED:
		m_lChecksumOld = 0;
		m_lChecksumNew = 0;
		return true;

	case MODE_NO_CHANGE:
	case MODE_PS3_FORMAT_BASIC:
		m_lChecksumOld = bufData.GetInt();
		m_lChecksumNew = bufData.GetInt();
		return true;

	case MODE_STRIP_LOD_1N:
		m_lChecksumOld = bufData.GetInt();
		m_lChecksumNew = bufData.GetInt();

		m_vtxVerts.Resize( bufData.GetInt(), true );
		for ( uint32 *pdwBase = m_vtxVerts.Base(), *pdwEnd = pdwBase + m_vtxVerts.GetNumDWords();
			pdwBase < pdwEnd; ++ pdwBase )
			*pdwBase = bufData.GetUnsignedInt();

		m_vtxIndices.SetCount( bufData.GetInt() );
		for ( unsigned short *pusBase = m_vtxIndices.Base(), *pusEnd = pusBase + m_vtxIndices.Count();
			pusBase < pusEnd; ++ pusBase )
			*pusBase = bufData.GetUnsignedShort();

		m_vtxMdlOffsets.SetCount( bufData.GetInt() );
		for ( MdlRangeItem *pmri = m_vtxMdlOffsets.Base(), *pmriEnd = pmri + m_vtxMdlOffsets.Count();
			pmri < pmriEnd; ++ pmri )
			pmri->m_offOld = bufData.GetInt(),
			pmri->m_offNew = bufData.GetInt(),
			pmri->m_numOld = bufData.GetInt(),
			pmri->m_numNew = bufData.GetInt();

		return true;

	case MODE_PS3_PARTITIONS:
		m_lChecksumOld = bufData.GetInt();
		m_lChecksumNew = bufData.GetInt();

		m_ps3studioBatches.SetCount( bufData.GetUnsignedInt() );
		for ( int k = 0; k < m_ps3studioBatches.Count(); ++ k )
		{
			m_ps3studioBatches[k] = new Ps3studioBatch_t;
			Ps3studioBatch_t &batch = *m_ps3studioBatches[k];
			batch.m_arrPartitions.SetCount( bufData.GetUnsignedInt() );
			for ( int j = 0; j < batch.m_arrPartitions.Count(); ++ j )
			{
				batch.m_arrPartitions[j] = new Ps3studioPartition_t;
				Ps3studioPartition_t &partition = *batch.m_arrPartitions[j];
				partition.m_arrLocalIndices.SetCount( bufData.GetUnsignedInt() );
				for ( int nn = 0; nn < partition.m_arrLocalIndices.Count(); ++ nn )
				{
					partition.m_arrLocalIndices[nn] = bufData.GetUnsignedShort();
				}
				partition.m_arrVertOriginalIndices.SetCount( bufData.GetUnsignedInt() );
				for ( int nn = 0; nn < partition.m_arrVertOriginalIndices.Count(); ++ nn )
				{
					partition.m_arrVertOriginalIndices[nn] = bufData.GetUnsignedInt();
				}
				partition.m_arrStripLocalOriginalIndices.SetCount( bufData.GetUnsignedInt() );
				for ( int nn = 0; nn < partition.m_arrStripLocalOriginalIndices.Count(); ++ nn )
				{
					partition.m_arrStripLocalOriginalIndices[nn] = bufData.GetUnsignedInt();
				}
				partition.m_nIoBufferSize = bufData.GetUnsignedInt();
			}
			batch.m_uiModelIndexOffset = bufData.GetUnsignedInt();
			batch.m_uiVhvIndexOffset = bufData.GetUnsignedInt();
		}

		m_ps3studioStripGroupHeaderBatchOffset.SetCount( bufData.GetUnsignedInt() );
		for ( int k = 0; k < m_ps3studioStripGroupHeaderBatchOffset.Count(); ++ k )
		{
			m_ps3studioStripGroupHeaderBatchOffset[k] = bufData.GetUnsignedInt();
		}
		return true;
	}
}


// Returns the checksums that the stripping info was generated for:
//	plChecksumOriginal		if non-NULL will hold the checksum of the original model submitted for stripping
//	plChecksumStripped		if non-NULL will hold the resulting checksum of the stripped model
bool CMdlStripInfo::GetCheckSum( long *plChecksumOriginal, long *plChecksumStripped ) const
{
	if ( m_eMode == MODE_UNINITIALIZED )
		return false;

	if ( plChecksumOriginal )
		*plChecksumOriginal = m_lChecksumOld;

	if ( plChecksumStripped )
		*plChecksumStripped = m_lChecksumNew;

	return true;
}


static inline uint32 Helper_SwapVhvColorForPs3( uint32 uiColor )
{
	// Swapping R and B channels
	return
		( ( ( uiColor >> 0 ) & 0xFF ) << 16 ) |
		( ( ( uiColor >> 8 ) & 0xFF ) << 8 ) |
		( ( ( uiColor >> 16 ) & 0xFF ) << 0 ) |
		( ( ( uiColor >> 24 ) & 0xFF ) << 24 );
}

//
// StripHardwareVertsBuffer
//	The main function that strips the vhv buffer
//		vhvBuffer		- vhv buffer, updated, size reduced
//
bool CMdlStripInfo::StripHardwareVertsBuffer( CUtlBuffer &vhvBuffer )
{
	if ( m_eMode == MODE_UNINITIALIZED )
		return false;

	//
	// Recover vhv header
	//
	DECLARE_PTR( HardwareVerts::FileHeader_t, vhvHdr, BYTE_OFF_PTR( vhvBuffer.Base(), vhvBuffer.TellGet() ) );
	int vhvLength = vhvBuffer.TellPut() - vhvBuffer.TellGet();

	if ( vhvHdr->m_nChecksum != m_lChecksumOld )
	{
		Log_Msg( LOG_ModelLib, "ERROR: [StripHardwareVertsBuffer] checksum mismatch!\n" );
		return false;
	}

	vhvHdr->m_nChecksum = m_lChecksumNew;


	// No remapping required
	if ( m_eMode == MODE_NO_CHANGE )
		return true;

	// Basic PS3 remapping required
	if ( m_eMode == MODE_PS3_FORMAT_BASIC )
	{
		DECLARE_PTR( uint32, pVertDataSrc, BYTE_OFF_PTR( vhvHdr, AlignValue( sizeof( *vhvHdr ) + vhvHdr->m_nMeshes * sizeof( HardwareVerts::MeshHeader_t ), 512 ) ) );
		DECLARE_PTR( uint32, pVertDataEnd, BYTE_OFF_PTR( vhvHdr, vhvLength ) );
		while ( pVertDataSrc + 1 <= pVertDataEnd )
		{
			* ( pVertDataSrc ++ ) = Helper_SwapVhvColorForPs3( *pVertDataSrc );
		}
		return true;
	}
	
	if ( m_eMode == MODE_STRIP_LOD_1N )
	{
		//
		// Now reconstruct the vhv structures to do the mapping
		//

		CMemoryMovingTracker vhvRemove( CMemoryMovingTracker::MEMORY_REMOVE );
		size_t vhvVertOffset = ~size_t( 0 ), vhvEndMeshOffset = sizeof( HardwareVerts::FileHeader_t );
		int numMeshesRemoved = 0, numVertsRemoved = 0;

		ITERATE_CHILDREN( HardwareVerts::MeshHeader_t, vhvMesh, vhvHdr, pMesh, m_nMeshes )
			if ( vhvMesh->m_nOffset < vhvVertOffset )
				vhvVertOffset = vhvMesh->m_nOffset;
			if ( BYTE_DIFF_PTR( vhvHdr, vhvMesh + 1 ) > vhvEndMeshOffset )
				vhvEndMeshOffset = BYTE_DIFF_PTR( vhvHdr, vhvMesh + 1 );
			if ( !vhvMesh->m_nLod )
				continue;
			vhvRemove.RegisterBytes( BYTE_OFF_PTR( vhvHdr, vhvMesh->m_nOffset ), vhvMesh->m_nVertexes * vhvHdr->m_nVertexSize );
			vhvRemove.RegisterElements( vhvMesh );
			numVertsRemoved += vhvMesh->m_nVertexes;
			++ numMeshesRemoved;
		ITERATE_END
		vhvRemove.RegisterBytes( BYTE_OFF_PTR( vhvHdr, vhvEndMeshOffset ), vhvVertOffset - vhvEndMeshOffset );	// Padding
		vhvRemove.RegisterBytes( BYTE_OFF_PTR( vhvHdr, vhvVertOffset + vhvHdr->m_nVertexes * vhvHdr->m_nVertexSize ), vhvLength - ( vhvVertOffset + vhvHdr->m_nVertexes * vhvHdr->m_nVertexSize ) );

		vhvRemove.Finalize();
		Log_Msg( LOG_ModelLib, " Stripped %d vhv bytes.\n", vhvRemove.GetNumBytesRegistered() );

		// Verts must be aligned from hdr, length must be aligned from hdr
		size_t vhvNewVertOffset = vhvRemove.ComputeOffset( vhvHdr, vhvVertOffset );
		size_t vhvAlignedVertOffset = ALIGN_VALUE( vhvNewVertOffset, 4 );

		ITERATE_CHILDREN( HardwareVerts::MeshHeader_t, vhvMesh, vhvHdr, pMesh, m_nMeshes )
			vhvMesh->m_nOffset = vhvRemove.ComputeOffset( vhvHdr, vhvMesh->m_nOffset ) + vhvAlignedVertOffset - vhvNewVertOffset;
		ITERATE_END
		vhvHdr->m_nMeshes -= numMeshesRemoved;
		vhvHdr->m_nVertexes -= numVertsRemoved;

		// Remove the memory
		vhvRemove.MemMove( vhvHdr, vhvLength );	// All padding has been removed

		size_t numBytesNewLength = vhvLength + vhvAlignedVertOffset - vhvNewVertOffset;
		size_t numAlignedNewLength = ALIGN_VALUE( numBytesNewLength, 4 );

		// Now reinsert the padding
		CInsertionTracker vhvInsertPadding;
		vhvInsertPadding.InsertBytes( BYTE_OFF_PTR( vhvHdr, vhvNewVertOffset ), vhvAlignedVertOffset - vhvNewVertOffset );
		vhvInsertPadding.InsertBytes( BYTE_OFF_PTR( vhvHdr, vhvLength ), numAlignedNewLength - numBytesNewLength );
		
		vhvInsertPadding.Finalize();
		Log_Msg( LOG_ModelLib, " Inserted %d alignment bytes.\n", vhvInsertPadding.GetNumBytesInserted() );

		vhvInsertPadding.MemMove( vhvHdr, vhvLength );


		// Update the buffer length
		vhvBuffer.SeekPut( CUtlBuffer::SEEK_CURRENT, vhvBuffer.TellGet() + vhvLength - vhvBuffer.TellPut() );

		Log_Msg( LOG_ModelLib, " Reduced vhv buffer by %d bytes.\n", vhvRemove.GetNumBytesRegistered() - vhvInsertPadding.GetNumBytesInserted() );
		return true;
	}

	if ( m_eMode == MODE_PS3_PARTITIONS )
	{
		//
		// Complex partitions processing
		//

		// Expect number of meshes in VHV header to match
		if ( !vhvHdr->m_nMeshes || vhvHdr->m_nMeshes != m_ps3studioStripGroupHeaderBatchOffset.Count() )
		{
			Log_Msg( LOG_ModelLib, " Mismatching vhv buffer mesh count( vhv=%d, vsi=%d ).\n", vhvHdr->m_nMeshes, m_ps3studioStripGroupHeaderBatchOffset.Count() );
			return false;
		}

		// Count total number of vertices
		uint32 uiTotalVerts = 0;
		for ( int k = 0; k < m_ps3studioBatches.Count(); ++ k )
			for ( int j = 0; j < m_ps3studioBatches[k]->m_arrPartitions.Count(); ++ j )
				uiTotalVerts += m_ps3studioBatches[k]->m_arrPartitions[j]->m_arrVertOriginalIndices.Count();

		// Now allocate enough target buffer space to fit all the verts
		uint32 uiRequiredBufferSize = sizeof( HardwareVerts::FileHeader_t ) + vhvHdr->m_nMeshes*sizeof( HardwareVerts::MeshHeader_t );
		uiRequiredBufferSize = AlignValue( uiRequiredBufferSize, 512 ); // start actual data stream on 512-boundary
		uint32 uiTotalBufferSize = AlignValue( uiRequiredBufferSize + 4 * uiTotalVerts, 512 );
		
		// Copy off the source buffer
		CUtlBuffer bufSrcCopy;
		bufSrcCopy.EnsureCapacity( MAX( uiTotalBufferSize, vhvLength ) );
		V_memcpy( bufSrcCopy.Base(), vhvHdr, vhvLength );

		// We know where the first mesh's vertices should start
		if ( vhvHdr->pMesh(0)->m_nOffset != uiRequiredBufferSize || uiTotalBufferSize < vhvLength )
		{
			Log_Msg( LOG_ModelLib, " Unexpected vhv buffer mesh offset.\n" );
			return false;
		}

		vhvBuffer.EnsureCapacity( vhvBuffer.TellGet() + uiTotalBufferSize );
		vhvBuffer.SeekPut( CUtlBuffer::SEEK_CURRENT, uiTotalBufferSize - vhvLength );
		DECLARE_UPDATE_PTR( HardwareVerts::FileHeader_t, vhvHdr, BYTE_OFF_PTR( vhvBuffer.Base(), vhvBuffer.TellGet() ) );
		DECLARE_PTR( HardwareVerts::FileHeader_t, vhvHdrSrc, bufSrcCopy.Base() );

		//
		// === update the actual VHV vertices
		//
		DECLARE_PTR( uint32, pVertDataSrc, BYTE_OFF_PTR( vhvHdrSrc, uiRequiredBufferSize ) );
		DECLARE_PTR( uint32, pVertDataDst, BYTE_OFF_PTR( vhvHdr, uiRequiredBufferSize ) );
#ifdef _DEBUG
		// Keep track of which verts got touched
		CGrowableBitVec arrTouchedOriginalVerts;
		uint32 uiDebugOriginalVertsPresent = 0;
		for ( uint32 iDebugMesh = 0; iDebugMesh < vhvHdr->m_nMeshes; ++ iDebugMesh )
			uiDebugOriginalVertsPresent += vhvHdr->pMesh(iDebugMesh)->m_nVertexes;
#endif
		for ( uint32 iMesh = 0, iBatch = 0; iMesh < m_ps3studioStripGroupHeaderBatchOffset.Count(); ++ iMesh )
		{
			uint32 numVerts = 0;
			vhvHdr->pMesh(iMesh)->m_nOffset = BYTE_DIFF_PTR( vhvHdr, pVertDataDst );
			uint32 iBatchEnd = ( iMesh < m_ps3studioStripGroupHeaderBatchOffset.Count() - 1 )
				? m_ps3studioStripGroupHeaderBatchOffset[iMesh+1] : m_ps3studioBatches.Count();
			iBatchEnd = MIN( iBatchEnd, m_ps3studioBatches.Count() );
			for ( ; iBatch < iBatchEnd; ++ iBatch )
			{
				Ps3studioBatch_t &batch = *m_ps3studioBatches[iBatch];
				// uint32 arrForcedColors[] = { 0xFF200000, 0xFFFF0000, 0xFFFFFF00, 0xFF002000, 0xFF00FF00, 0xFF00FFFF, 0xFF000020, 0xFF0000FF, 0xFFFF00FF  };
				for ( uint32 iPartition = 0; iPartition < batch.m_arrPartitions.Count(); ++ iPartition )
				{
					Ps3studioPartition_t &partition = *batch.m_arrPartitions[iPartition];
					numVerts += partition.m_arrVertOriginalIndices.Count();
					for ( uint32 iVertIndex = 0; iVertIndex < partition.m_arrVertOriginalIndices.Count(); ++ iVertIndex )
					{
						// uint32 uiOrigVertIndex = partition.m_arrVertOriginalIndices[iVertIndex];
						uint32 uiOrigVertIndex = partition.m_arrStripLocalOriginalIndices[iVertIndex];
						uiOrigVertIndex += batch.m_uiVhvIndexOffset;
						uint32 uiColor = pVertDataSrc[uiOrigVertIndex];
						Assert( BYTE_DIFF_PTR( vhvHdrSrc, pVertDataSrc[uiOrigVertIndex] ) < vhvLength );
						// uiColor = arrForcedColors[iPartition%ARRAYSIZE(arrForcedColors)];
						*( pVertDataDst ++ ) = Helper_SwapVhvColorForPs3( uiColor );
						Assert( BYTE_DIFF_PTR( vhvHdr, pVertDataDst ) <= uiTotalBufferSize );
#ifdef _DEBUG
						arrTouchedOriginalVerts.GrowSetBit( uiOrigVertIndex );
#endif
					}
				}
			}
			vhvHdr->pMesh(iMesh)->m_nVertexes = numVerts;
		}
#ifdef _DEBUG
		{
			uint32 uiDebugTouchedOriginalVerts = arrTouchedOriginalVerts.GetNumBits();
			for ( uint32 iDebugOrigVert = 0; iDebugOrigVert < uiDebugOriginalVertsPresent; ++ iDebugOrigVert )
			{
				Assert( arrTouchedOriginalVerts.IsBitSet( iDebugOrigVert ) );
			}
			Assert( uiDebugTouchedOriginalVerts == uiDebugOriginalVertsPresent );
		}
#endif

		return true;
	}

	// Done
	return false;
}

//
// StripModelBuffer
//	The main function that strips the mdl buffer
//		mdlBuffer		- mdl buffer, updated
//
bool CMdlStripInfo::StripModelBuffer( CUtlBuffer &mdlBuffer )
{
	if ( m_eMode == MODE_UNINITIALIZED )
		return false;

	//
	// Recover mdl header
	//
	DECLARE_PTR( studiohdr_t, mdlHdr, BYTE_OFF_PTR( mdlBuffer.Base(), mdlBuffer.TellGet() ) );

	if ( mdlHdr->checksum != m_lChecksumOld )
	{
		Log_Msg( LOG_ModelLib, "ERROR: [StripModelBuffer] checksum mismatch!\n" );
		return false;
	}

	mdlHdr->checksum = m_lChecksumNew;


	// No remapping required
	if ( m_eMode == MODE_NO_CHANGE )
		return true;
	if ( m_eMode != MODE_STRIP_LOD_1N )
		return false;

	//
	// Do the model buffer stripping
	//

	CUtlSortVector< unsigned short, CLessSimple< unsigned short > > &srcIndices = m_vtxIndices;

	ITERATE_CHILDREN( mstudiobodyparts_t, mdlBodyPart, mdlHdr, pBodypart, numbodyparts )
		ITERATE_CHILDREN( mstudiomodel_t, mdlModel, mdlBodyPart, pModel, nummodels )
			
			Log_Msg( LOG_ModelLib, " Stripped %d vertexes (was: %d, now: %d).\n", mdlModel->numvertices - srcIndices.Count(), mdlModel->numvertices, srcIndices.Count() );

			mdlModel->numvertices = srcIndices.Count();

			ITERATE_CHILDREN( mstudiomesh_t, mdlMesh, mdlModel, pMesh, nummeshes )
				
				mdlMesh->numvertices = srcIndices.FindLess( mdlMesh->vertexoffset + mdlMesh->numvertices );
				mdlMesh->vertexoffset = srcIndices.FindLess( mdlMesh->vertexoffset ) + 1;
				mdlMesh->numvertices -= mdlMesh->vertexoffset - 1;

				// Truncate the number of vertexes
				for ( int k = 0; k < ARRAYSIZE( mdlMesh->vertexdata.numLODVertexes ); ++ k )
					mdlMesh->vertexdata.numLODVertexes[ k ] = mdlMesh->numvertices;

			ITERATE_END
		ITERATE_END
	ITERATE_END

	//
	// Update bones not to mention anything below LOD0
	//
	ITERATE_CHILDREN( const mstudiobone_t, mdlBone, mdlHdr, pBone, numbones )
		((mstudiobone_t *)mdlBone)->flags &= ( BONE_USED_BY_VERTEX_LOD0 | ~BONE_USED_BY_VERTEX_MASK );
	ITERATE_END

	Log_Msg( LOG_ModelLib, " Updated %d bone(s).\n", mdlHdr->numbones );

	return true;
}


//
// StripVertexDataBuffer
//	The main function that strips the vvd buffer
//		vvdBuffer		- vvd buffer, updated, size reduced
//
bool CMdlStripInfo::StripVertexDataBuffer( CUtlBuffer &vvdBuffer )
{
	if ( m_eMode == MODE_UNINITIALIZED )
		return false;

	//
	// Recover vvd header
	//
	DECLARE_PTR( vertexFileHeader_t, vvdHdr, BYTE_OFF_PTR( vvdBuffer.Base(), vvdBuffer.TellGet() ) );
	int vvdLength = vvdBuffer.TellPut() - vvdBuffer.TellGet();

	if ( vvdHdr->checksum != m_lChecksumOld )
	{
		Log_Msg( LOG_ModelLib, "ERROR: [StripVertexDataBuffer] checksum mismatch!\n" );
		return false;
	}

	vvdHdr->checksum = m_lChecksumNew;


	// No remapping required
	if ( m_eMode == MODE_NO_CHANGE )
		return true;
	if ( m_eMode != MODE_STRIP_LOD_1N )
		return false;

	//
	// Do the vertex data buffer stripping
	//

	CUtlSortVector< unsigned short, CLessSimple< unsigned short > > &srcIndices = m_vtxIndices;
	int mdlNumVerticesOld = vvdHdr->numLODVertexes[ 0 ];

	vvdHdr->numLODs = 1;
	for ( int k = 0; k < ARRAYSIZE( vvdHdr->numLODVertexes ); ++ k )
		vvdHdr->numLODVertexes[ k ] = srcIndices.Count();

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
		vvdVertexSrc  ? memcpy( vvdVertexSrc, BYTE_OFF_PTR( memTempVVD.Get(), vvdHdr->vertexDataStart ), mdlNumVerticesOld * sizeof( *vvdVertexSrc ) ) : 0;
		vvdTangentSrc ? memcpy( vvdTangentSrc, BYTE_OFF_PTR( memTempVVD.Get(), vvdHdr->tangentDataStart ), mdlNumVerticesOld * sizeof( *vvdTangentSrc ) ) : 0;
	}
	
	vvdHdr->vertexDataStart -= ALIGN_VALUE( sizeof( vertexFileFixup_t ) * vvdHdr->numFixups, 16 );
	vvdHdr->numFixups = 0;
	DECLARE_PTR( mstudiovertex_t, vvdVertexNew, BYTE_OFF_PTR( vvdHdr, vvdHdr->vertexDataStart ) );
	for ( int k = 0; k < srcIndices.Count(); ++ k )
		vvdVertexNew[ k ] = vvdVertexSrc[ srcIndices[ k ] ];

	size_t newVertexDataSize = srcIndices.Count() * sizeof( mstudiovertex_t );
	int vvdLengthOld = vvdLength;
	vvdLength = vvdHdr->vertexDataStart + newVertexDataSize;

	if ( vvdTangentSrc )
	{
		// Move the tangents
		vvdHdr->tangentDataStart = vvdLength;
		DECLARE_PTR( Vector4D, vvdTangentNew, BYTE_OFF_PTR( vvdHdr, vvdHdr->tangentDataStart ) );

		for ( int k = 0; k < srcIndices.Count(); ++ k )
			vvdTangentNew[ k ] = vvdTangentSrc[ srcIndices[ k ] ];

		vvdLength += srcIndices.Count() * sizeof( Vector4D );
	}
	
	vvdBuffer.SeekPut( CUtlBuffer::SEEK_CURRENT, vvdBuffer.TellGet() + vvdLength - vvdBuffer.TellPut() );

	Log_Msg( LOG_ModelLib, " Stripped %d vvd bytes.\n", vvdLengthOld - vvdLength );

	return true;
}

//
// StripOptimizedModelBuffer
//	The main function that strips the vtx buffer
//		vtxBuffer		- vtx buffer, updated, size reduced
//
bool CMdlStripInfo::StripOptimizedModelBuffer( CUtlBuffer &vtxBuffer )
{
	if ( m_eMode == MODE_UNINITIALIZED )
		return false;

	//
	// Recover vtx header
	//
	DECLARE_PTR( OptimizedModel::FileHeader_t, vtxHdr, BYTE_OFF_PTR( vtxBuffer.Base(), vtxBuffer.TellGet() ) );
	int vtxLength = vtxBuffer.TellPut() - vtxBuffer.TellGet();

	if ( vtxHdr->checkSum != m_lChecksumOld )
	{
		Log_Msg( LOG_ModelLib, "ERROR: [StripOptimizedModelBuffer] checksum mismatch!\n" );
		return false;
	}

	vtxHdr->checkSum = m_lChecksumNew;

	// No remapping required
	if ( m_eMode == MODE_NO_CHANGE )
		return true;

	if ( m_eMode != MODE_STRIP_LOD_1N )
		return false;

	//
	// Do the optimized model buffer stripping
	//

	CUtlSortVector< unsigned short, CLessSimple< unsigned short > > &srcIndices = m_vtxIndices;
	CUtlSortVector< CMdlStripInfo::MdlRangeItem, CLessSimple< CMdlStripInfo::MdlRangeItem > > &arrMdlOffsets = m_vtxMdlOffsets;

	size_t vtxOffIndexBuffer = ~size_t(0), vtxOffIndexBufferEnd = 0;
	size_t vtxOffVertexBuffer = ~size_t(0), vtxOffVertexBufferEnd = 0;
	CMemoryMovingTracker vtxRemove( CMemoryMovingTracker::MEMORY_REMOVE );
	CUtlVector< size_t > vtxOffIndex;
	CUtlVector< size_t > vtxOffVertex;

	vtxRemove.RegisterElements( CHILD_AT( vtxHdr, pMaterialReplacementList, 1 ), vtxHdr->numLODs - 1 );
	ITERATE_CHILDREN( OptimizedModel::MaterialReplacementListHeader_t, vtxMatList, vtxHdr, pMaterialReplacementList, numLODs )
		if ( !vtxMatList_idx ) continue;
		vtxRemove.RegisterElements( CHILD_AT( vtxMatList, pMaterialReplacement, 0 ), vtxMatList->numReplacements );
		ITERATE_CHILDREN( OptimizedModel::MaterialReplacementHeader_t, vtxMat, vtxMatList, pMaterialReplacement, numReplacements )
			char const *szName = vtxMat->pMaterialReplacementName();
			vtxRemove.RegisterElements( szName, szName ? strlen( szName ) + 1 : 0 );
		ITERATE_END
	ITERATE_END

	ITERATE_CHILDREN( OptimizedModel::BodyPartHeader_t, vtxBodyPart, vtxHdr, pBodyPart, numBodyParts )
		ITERATE_CHILDREN( OptimizedModel::ModelHeader_t, vtxModel, vtxBodyPart, pModel, numModels )
		
			vtxRemove.RegisterElements( CHILD_AT( vtxModel, pLOD, 1 ), vtxModel->numLODs - 1 );
			ITERATE_CHILDREN( OptimizedModel::ModelLODHeader_t, vtxLod, vtxModel, pLOD, numLODs )
				if ( !vtxLod_idx )	// Process only lod1-N
					continue;
				
				vtxRemove.RegisterElements( CHILD_AT( vtxLod, pMesh, 0 ), vtxLod->numMeshes );
				ITERATE_CHILDREN( OptimizedModel::MeshHeader_t, vtxMesh, vtxLod, pMesh, numMeshes )
					vtxRemove.RegisterElements( CHILD_AT( vtxMesh, pStripGroup, 0 ), vtxMesh->numStripGroups );
					ITERATE_CHILDREN( OptimizedModel::StripGroupHeader_t, vtxStripGroup, vtxMesh, pStripGroup, numStripGroups )
						vtxRemove.RegisterElements( CHILD_AT( vtxStripGroup, pStrip, 0 ), vtxStripGroup->numStrips );
						ITERATE_CHILDREN( OptimizedModel::StripHeader_t, vtxStrip, vtxStripGroup, pStrip, numStrips )
							vtxRemove.RegisterElements( CHILD_AT( vtxStrip, pBoneStateChange, 0 ), vtxStrip->numBoneStateChanges );
						ITERATE_END
					ITERATE_END
				ITERATE_END

			ITERATE_END

			// Use all lods to determine the ranges of vertex and index buffers.
			// We rely on the fact that vertex and index buffers are laid out as one solid memory block for all lods.
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

						if ( !vtxLod_idx )
						{
							vtxOffIndex.AddToTail( offIndex );
							vtxOffIndex.AddToTail( offIndexEnd );
							vtxOffVertex.AddToTail( offVertex );
							vtxOffVertex.AddToTail( offVertexEnd );
						}

					ITERATE_END
				ITERATE_END
			ITERATE_END

		ITERATE_END
	ITERATE_END

	// Fixup the vertex buffer
	DECLARE_PTR( OptimizedModel::Vertex_t, vtxVertexBuffer, BYTE_OFF_PTR( vtxHdr, vtxOffVertexBuffer ) );
	DECLARE_PTR( OptimizedModel::Vertex_t, vtxVertexBufferEnd, BYTE_OFF_PTR( vtxHdr, vtxOffVertexBufferEnd ) );
	CUtlVector< int > vtxIndexDeltas;
	vtxIndexDeltas.EnsureCapacity( vtxVertexBufferEnd - vtxVertexBuffer );
	int vtxNumVertexRemoved = 0;
	for ( OptimizedModel::Vertex_t *vtxVertexElement = vtxVertexBuffer; vtxVertexElement < vtxVertexBufferEnd; ++ vtxVertexElement )
	{
		size_t const off = BYTE_DIFF_PTR( vtxHdr, vtxVertexElement );
		bool bUsed = false;
		for ( int k = 0; k < vtxOffVertex.Count(); k += 2 )
		{
			if ( off >= vtxOffVertex[ k ] && off < vtxOffVertex[ k + 1 ] )
			{
				bUsed = true;
				break;
			}
		}
		if ( !bUsed )
		{
			// Index is not in use
			vtxRemove.RegisterElements( vtxVertexElement );
			vtxIndexDeltas.AddToTail( 0 );
			vtxNumVertexRemoved ++;
		}
		else
		{	// Index is in use and must be remapped
			// Find the mesh where this index belongs
			int iMesh = arrMdlOffsets.FindLessOrEqual( MdlRangeItem( 0, 0, vtxVertexElement - vtxVertexBuffer ) );
			Assert( iMesh >= 0 && iMesh < arrMdlOffsets.Count() );
			
			MdlRangeItem &mri = arrMdlOffsets[ iMesh ];
			Assert( ( vtxVertexElement - vtxVertexBuffer >= mri.m_offNew ) && ( vtxVertexElement - vtxVertexBuffer < mri.m_offNew + mri.m_numNew ) );
			
			Assert( m_vtxVerts.IsBitSet( vtxVertexElement->origMeshVertID + mri.m_offOld ) );
			vtxVertexElement->origMeshVertID = srcIndices.Find( vtxVertexElement->origMeshVertID + mri.m_offOld ) - mri.m_offNew;
			Assert( vtxVertexElement->origMeshVertID < mri.m_numNew );
			vtxIndexDeltas.AddToTail( vtxNumVertexRemoved );
		}
	}

	// Fixup the index buffer
	DECLARE_PTR( unsigned short, vtxIndexBuffer, BYTE_OFF_PTR( vtxHdr, vtxOffIndexBuffer ) );
	DECLARE_PTR( unsigned short, vtxIndexBufferEnd, BYTE_OFF_PTR( vtxHdr, vtxOffIndexBufferEnd ) );
	for ( unsigned short *vtxIndexElement = vtxIndexBuffer; vtxIndexElement < vtxIndexBufferEnd; ++ vtxIndexElement )
	{
		size_t const off = BYTE_DIFF_PTR( vtxHdr, vtxIndexElement );
		bool bUsed = false;
		for ( int k = 0; k < vtxOffIndex.Count(); k += 2 )
		{
			if ( off >= vtxOffIndex[ k ] && off < vtxOffIndex[ k + 1 ] )
			{
				bUsed = true;
				break;
			}
		}
		if ( !bUsed )
		{
			// Index is not in use
			vtxRemove.RegisterElements( vtxIndexElement );
		}
		else
		{
			// Index is in use and must be remapped
			*vtxIndexElement -= vtxIndexDeltas[ *vtxIndexElement ];
		}
	}

	// By now should have scheduled all removal information
	vtxRemove.Finalize();
	Log_Msg( LOG_ModelLib, " Stripped %d vtx bytes.\n", vtxRemove.GetNumBytesRegistered() );

	//
	// Fixup all the offsets
	//
	ITERATE_CHILDREN( OptimizedModel::MaterialReplacementListHeader_t, vtxMatList, vtxHdr, pMaterialReplacementList, numLODs )
		ITERATE_CHILDREN( OptimizedModel::MaterialReplacementHeader_t, vtxMat, vtxMatList, pMaterialReplacement, numReplacements )
			vtxMat->replacementMaterialNameOffset = vtxRemove.ComputeOffset( vtxMat, vtxMat->replacementMaterialNameOffset );
		ITERATE_END
		vtxMatList->replacementOffset = vtxRemove.ComputeOffset( vtxMatList, vtxMatList->replacementOffset );
	ITERATE_END
	ITERATE_CHILDREN( OptimizedModel::BodyPartHeader_t, vtxBodyPart, vtxHdr, pBodyPart, numBodyParts )
		ITERATE_CHILDREN( OptimizedModel::ModelHeader_t, vtxModel, vtxBodyPart, pModel, numModels )
			ITERATE_CHILDREN( OptimizedModel::ModelLODHeader_t, vtxLod, vtxModel, pLOD, numLODs )
				ITERATE_CHILDREN( OptimizedModel::MeshHeader_t, vtxMesh, vtxLod, pMesh, numMeshes )
					ITERATE_CHILDREN( OptimizedModel::StripGroupHeader_t, vtxStripGroup, vtxMesh, pStripGroup, numStripGroups )
						
						ITERATE_CHILDREN( OptimizedModel::StripHeader_t, vtxStrip, vtxStripGroup, pStrip, numStrips )
							vtxStrip->indexOffset =
								vtxRemove.ComputeOffset( vtxStripGroup, vtxStripGroup->indexOffset + vtxStrip->indexOffset ) -
								vtxRemove.ComputeOffset( vtxStripGroup, vtxStripGroup->indexOffset );
							vtxStrip->vertOffset =
								vtxRemove.ComputeOffset( vtxStripGroup, vtxStripGroup->vertOffset + vtxStrip->vertOffset ) -
								vtxRemove.ComputeOffset( vtxStripGroup, vtxStripGroup->vertOffset );
							vtxStrip->boneStateChangeOffset = vtxRemove.ComputeOffset( vtxStrip, vtxStrip->boneStateChangeOffset );
						ITERATE_END

						vtxStripGroup->vertOffset = vtxRemove.ComputeOffset( vtxStripGroup, vtxStripGroup->vertOffset );
						vtxStripGroup->indexOffset = vtxRemove.ComputeOffset( vtxStripGroup, vtxStripGroup->indexOffset );
						vtxStripGroup->stripOffset = vtxRemove.ComputeOffset( vtxStripGroup, vtxStripGroup->stripOffset );
					ITERATE_END
					vtxMesh->stripGroupHeaderOffset = vtxRemove.ComputeOffset( vtxMesh, vtxMesh->stripGroupHeaderOffset );
				ITERATE_END
				vtxLod->meshOffset = vtxRemove.ComputeOffset( vtxLod, vtxLod->meshOffset );
			ITERATE_END
			vtxModel->lodOffset = vtxRemove.ComputeOffset( vtxModel, vtxModel->lodOffset );
			vtxModel->numLODs = 1;
		ITERATE_END
		vtxBodyPart->modelOffset = vtxRemove.ComputeOffset( vtxBodyPart, vtxBodyPart->modelOffset );
	ITERATE_END
	vtxHdr->materialReplacementListOffset = vtxRemove.ComputeOffset( vtxHdr, vtxHdr->materialReplacementListOffset );
	vtxHdr->bodyPartOffset = vtxRemove.ComputeOffset( vtxHdr, vtxHdr->bodyPartOffset );
	vtxHdr->numLODs = 1;

	// Perform final memory move
	vtxRemove.MemMove( vtxHdr, vtxLength );

	vtxBuffer.SeekPut( CUtlBuffer::SEEK_CURRENT, vtxBuffer.TellGet() + vtxLength - vtxBuffer.TellPut() );

	return true;
}





//////////////////////////////////////////////////////////////////////////
//
// Auxilliary methods
//
//////////////////////////////////////////////////////////////////////////

void CMdlStripInfo::DeleteThis()
{
	delete this;
}

void CMdlStripInfo::Reset()
{
	m_eMode = MODE_UNINITIALIZED;
	m_lChecksumOld = 0;
	m_lChecksumNew = 0;

	m_vtxVerts.Resize( 0 );
	m_vtxIndices.RemoveAll();

	m_ps3studioBatches.PurgeAndDeleteElements();
}

