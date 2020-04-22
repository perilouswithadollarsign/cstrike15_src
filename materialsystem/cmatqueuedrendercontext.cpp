//========== Copyright  2005, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#include "pch_materialsystem.h"

#include "tier1/functors.h"
#include "tier1/fmtstr.h"
#include "itextureinternal.h"

#ifndef _PS3
#define MATSYS_INTERNAL
#endif

#include "cmatqueuedrendercontext.h"
#include "cmaterialsystem.h" // @HACKHACK

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


ConVar mat_report_queue_status( "mat_report_queue_status", "0", FCVAR_MATERIAL_SYSTEM_THREAD );

#if defined( _PS3 ) || defined( _OSX )
#define g_pShaderAPI ShaderAPI()
#endif

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

#if defined( _WIN32 ) && !defined( _WIN64 )
void FastCopy( byte *pDest, const byte *pSrc, size_t nBytes )
{
	if ( !nBytes )
	{
		return;
	}

#if !defined( _X360 )
	if ( (size_t)pDest % 16 == 0 && (size_t)pSrc % 16 == 0 )
	{
		const int BYTES_PER_FULL = 128;
		int nBytesFull = nBytes - ( nBytes % BYTES_PER_FULL );
		for ( byte *pLimit = pDest + nBytesFull; pDest < pLimit; pDest += BYTES_PER_FULL, pSrc += BYTES_PER_FULL )
		{
			// memcpy( pDest, pSrc, BYTES_PER_FULL);
			__asm
			{
				mov esi, pSrc
				mov edi, pDest

				movaps xmm0, [esi + 0]
				movaps xmm1, [esi + 16]
				movaps xmm2, [esi + 32]
				movaps xmm3, [esi + 48]
				movaps xmm4, [esi + 64]
				movaps xmm5, [esi + 80]
				movaps xmm6, [esi + 96]
				movaps xmm7, [esi + 112]

				movntps [edi + 0], xmm0
				movntps [edi + 16], xmm1
				movntps [edi + 32], xmm2
				movntps [edi + 48], xmm3
				movntps [edi + 64], xmm4
				movntps [edi + 80], xmm5
				movntps [edi + 96], xmm6
				movntps [edi + 112], xmm7
			}
		}
		nBytes -= nBytesFull;
	}

	if ( nBytes )
	{
		memcpy( pDest, pSrc, nBytes );
	}
#else
	if ( (size_t)pDest % 4 == 0 && nBytes % 4 == 0 )
	{
		XMemCpyStreaming_WriteCombined( pDest, pSrc, nBytes );
	}
	else
	{
		// work around a bug in memcpy
		if ((size_t)pDest % 2 == 0 && nBytes == 4)
		{
			*(reinterpret_cast<short *>(pDest)) = *(reinterpret_cast<const short *>(pSrc));
			*(reinterpret_cast<short *>(pDest)+1) = *(reinterpret_cast<const short *>(pSrc)+1);
		}
		else
		{
			memcpy( pDest, pSrc, nBytes );
		}
	}
#endif
}
#else
#define FastCopy memcpy
#endif

class CCachedPerFrameMeshData;
//-----------------------------------------------------------------------------
// Queued mesh, used for dynamic meshes
//-----------------------------------------------------------------------------
class CMatQueuedMesh : public IMesh
{
	// Methods of IVertexBuffer, called from the main (client) thread
public:
	virtual int VertexCount() const;
	virtual VertexFormat_t GetVertexFormat() const;
	virtual bool IsDynamic() const												{ return true; } 
	virtual void BeginCastBuffer( VertexFormat_t format )						{ CannotSupport(); }
	virtual void EndCastBuffer( )												{ CannotSupport(); }
	virtual int GetRoomRemaining() const										{ CannotSupport(); return 0; }
	virtual void * AccessRawHardwareDataStream( uint8 nRawStreamIndex, uint32 numBytes, uint32 uiFlags, void *pvContext ) {	CannotSupport(); return NULL; }
	virtual bool Lock( int nVertexCount, bool bAppend, VertexDesc_t &desc )		{ CannotSupport(); return false; }
	virtual void Unlock( int nVertexCount, VertexDesc_t &desc )					{ CannotSupport(); }
	virtual void Spew( int nVertexCount, const VertexDesc_t &desc )				{ }
	virtual void ValidateData( int nVertexCount, const VertexDesc_t & desc )	{ }

	// Methods of IIndexBuffer, called from the main (client) thread
public:
	virtual int IndexCount() const;
	virtual MaterialIndexFormat_t IndexFormat() const												{ CannotSupport(); return MATERIAL_INDEX_FORMAT_16BIT; }
	virtual void BeginCastBuffer( MaterialIndexFormat_t format )									{ CannotSupport(); }
	virtual bool Lock( int nMaxIndexCount, bool bAppend, IndexDesc_t& desc )						{ CannotSupport(); return false; }
	virtual void Unlock( int nWrittenIndexCount, IndexDesc_t& desc )								{ CannotSupport(); }
	virtual void ModifyBegin( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t& desc )	{ CannotSupport(); }
	virtual void ModifyEnd( IndexDesc_t& desc )														{ CannotSupport(); }
	virtual void Spew( int nIndexCount, const IndexDesc_t & desc )									{ }
	virtual void ValidateData( int nIndexCount, const IndexDesc_t &desc )							{ }
	virtual IMesh *GetMesh()																		{ return this; }

	// Methods of IMesh, called from the main (client) thread
public:
	virtual void SetPrimitiveType( MaterialPrimitiveType_t type );
	virtual void Draw( int firstIndex = -1, int numIndices = 0 );
	virtual void SetColorMesh( IMesh *pColorMesh, int nVertexOffset );
	virtual void Draw( CPrimList *pLists, int nLists )																					{ CannotSupport(); }
	virtual void CopyToMeshBuilder( int iStartVert, int nVerts, int iStartIndex, int nIndices, int indexOffset, CMeshBuilder &builder )	{ CannotSupport(); }
	virtual void Spew( int numVerts, int numIndices, const MeshDesc_t & desc )															{ }
	virtual void ValidateData( int numVerts, int numIndices, const MeshDesc_t & desc )													{ }
	virtual void LockMesh( int numVerts, int numIndices, MeshDesc_t& desc, MeshBuffersAllocationSettings_t *pSettings = 0 );
	virtual void ModifyBegin( int firstVertex, int numVerts, int firstIndex, int numIndices, MeshDesc_t& desc )							{ CannotSupport(); }
	virtual void ModifyEnd( MeshDesc_t& desc )																							{ CannotSupport(); }
	virtual void UnlockMesh( int numVerts, int numIndices, MeshDesc_t& desc );
	virtual void ModifyBeginEx( bool bReadOnly, int firstVertex, int numVerts, int firstIndex, int numIndices, MeshDesc_t& desc )		{ CannotSupport(); }
	virtual void SetFlexMesh( IMesh *pMesh, int nVertexOffset );
	virtual void DisableFlexMesh();
	virtual void MarkAsDrawn();
	virtual void DrawModulated( const Vector4D &vecDiffuseModulation, int firstIndex = -1, int numIndices = 0 );
	virtual unsigned int ComputeMemoryUsed() { return 0; }
	virtual ICachedPerFrameMeshData *GetCachedPerFrameMeshData();
	virtual void ReconstructFromCachedPerFrameMeshData( ICachedPerFrameMeshData *pData );

	// Other methods called from the main (client) thread
public:
	CMatQueuedMesh( CMatQueuedRenderContext *pOwner, IMatRenderContextInternal *pHardwareContext, bool bFlexMesh );
	byte *GetVertexData()				{ return m_pVertexData; }
	uint16 *GetIndexData()				{ return m_pIndexData; }
	void InvalidateAuxMeshSet()			{ m_bCanSetAuxMeshes = false; }
	int GetVertexSize()					{ return m_VertexSize; }
	bool OnGetDynamicMesh( VertexFormat_t vertexFormat, unsigned flags, IMesh* pVertexOverride, IMesh* pIndexOverride, IMaterialInternal *pMaterial, int nHWSkinBoneCount );
	void QueueBuild( );
	void FreeBuffers();

	// These methods are called from the material system thread
	// Using the prefix MST_ to indicate this.
public:
	struct MST_MeshInfo_t
	{
		IMaterial *m_pMaterial;
		VertexFormat_t m_VertexFormat;
		uint8 m_nFlags;
		bool m_bExternalVB : 1;
		bool m_bExternalIB : 1;
		IMesh* m_pVertexOverride;
		IMesh* m_pIndexOverride;
		uint8 *m_pVertexData;
		int m_nVertexCount;
		int m_nVertexSizeInBytes;
		uint16 *m_pIndexData;
		int m_nIndexCount;
		MaterialPrimitiveType_t m_nPrimitiveType;
	};

	struct MST_DrawInfo_t
	{
		MaterialPrimitiveType_t m_Type;
		int m_nFirstIndex;
		int m_nIndexCount;
	};

	struct MST_DrawModulatedInfo_t
	{
		MaterialPrimitiveType_t m_Type;
		Vector4D m_vecDiffuseModulation;
		int m_nFirstIndex;
		int m_nIndexCount;
	};

	IMesh *MST_DetachActualMesh()		{ IMesh *p = m_pMSTActualMesh; m_pMSTActualMesh = NULL; return p; }
	IMesh *MST_GetActualMesh()			{ return m_pMSTActualMesh; }
	IMesh *MST_SetupExternalMesh( const MST_MeshInfo_t &info );
	IMesh *MST_SetupDynamicMesh( const MST_MeshInfo_t &info, IMesh *pExternalMesh );
	int MST_GetActualVertexOffsetInBytes() const { return m_nMSTActualVertexOffsetInBytes; }
	void MST_CopyDynamicVB( const MeshDesc_t &desc, const uint8 *pVertexData, size_t nSizeInBytes );
	void MST_CopyDynamicIB( const MeshDesc_t &desc, const uint16 *pIndexData, int nIndices );
	void MST_BuildDynamicBuffers( const MST_MeshInfo_t &info );
	void MST_Draw( const MST_DrawInfo_t &info );
	void MST_DrawModulated( const MST_DrawModulatedInfo_t &info );

	// Member variables accessible from the material thread
private:
	int m_nMSTActualVertexOffsetInBytes;
	IMesh *m_pMSTActualMesh;

	// Member variables accessible from the main thread
private:

	CLateBoundPtr<IMesh> m_pLateBoundMesh;

	CMatQueuedRenderContext *m_pOwner;
	CMatCallQueue *m_pCallQueue;
	IMatRenderContextInternal *m_pHardwareContext;

	// The vertex format we're using...
	VertexFormat_t m_VertexFormat;

	byte *m_pVertexData;
	uint16 *m_pIndexData;

	int m_nVerts;
	int m_nIndices;

	unsigned short m_VertexSize;
	uint8 m_nFlags;
	bool m_bExternalVB;
	bool m_bExternalIB;
	bool m_bFlexMesh;
	bool m_bCanSetAuxMeshes;
	MaterialPrimitiveType_t m_Type;

	IMesh *m_pVertexOverride;
	IMesh *m_pIndexOverride;

	static unsigned short gm_ScratchIndexBuffer;
};

enum MatQueuedMeshFlags_t
{
	MQM_BUFFERED	= ( 1 << 0 ),
};

//-----------------------------------------------------------------------------
// Used for caching off the memory pointers, and vertex/index data info for a
// CMatQueuedMesh.
//-----------------------------------------------------------------------------
class CCachedPerFrameMeshData : public ICachedPerFrameMeshData
{
public:
	virtual void Free()
	{
		delete this;
	}

	CMatQueuedMesh::MST_MeshInfo_t m_meshInfo;

	CCachedPerFrameMeshData()
	{
		m_meshInfo.m_pMaterial = NULL;
	}

	~CCachedPerFrameMeshData()
	{
	}
};

//----------------------------------------------------------------------------
// Static members
//----------------------------------------------------------------------------
unsigned short CMatQueuedMesh::gm_ScratchIndexBuffer;


//----------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------
CMatQueuedMesh::CMatQueuedMesh( CMatQueuedRenderContext *pOwner, IMatRenderContextInternal *pHardwareContext, bool bFlexMesh )
 :	m_pLateBoundMesh( &m_pMSTActualMesh ),
	m_pOwner( pOwner ), 
	m_pCallQueue( pOwner->GetCallQueueInternal() ),
	m_pHardwareContext( pHardwareContext ),
	m_pVertexData( NULL ),
	m_pIndexData( NULL ),
	m_nVerts( 0 ),
	m_nIndices( 0 ),
	m_VertexSize( 0 ),
	m_Type( MATERIAL_TRIANGLES ),
	m_pVertexOverride( NULL ),
	m_pIndexOverride ( NULL ),
	m_pMSTActualMesh( NULL ),
	m_nMSTActualVertexOffsetInBytes( 0 ),
	m_VertexFormat( 0 ),
	m_bFlexMesh( bFlexMesh ),
	m_bCanSetAuxMeshes( false ),
	m_nFlags( 0 ),
	m_bExternalIB( false ),
	m_bExternalVB( false )
{
}


//----------------------------------------------------------------------------
//
// Methods that run in the material system thread
//
//----------------------------------------------------------------------------


//----------------------------------------------------------------------------
// Gets the external mesh + points it at the already-allocated data
//----------------------------------------------------------------------------
IMesh *CMatQueuedMesh::MST_SetupExternalMesh( const MST_MeshInfo_t &info )
{
#ifndef MS_NO_DYNAMIC_BUFFER_COPY
	return NULL;
#else
	// If we don't have real external data on either VB or IB channel, no dynamic mesh
	if ( !info.m_bExternalVB && !info.m_bExternalIB )
		return NULL;

	// Ok, at at least one of VB or IB (maybe both) are external data.
	// In this case, the other buffer may be an override, NULL, or dynamic data
	// We are always going to treat external data as an override of a dynamic mesh
	// because that way colormesh/flexmesh state tracking is simpler: it
	// always goes through the dynamic mesh
	ExternalMeshInfo_t extInfo;
	extInfo.m_pMaterial = info.m_pMaterial;
	extInfo.m_bFlexMesh = m_bFlexMesh;
	extInfo.m_VertexFormat = info.m_VertexFormat;
	extInfo.m_pVertexOverride = NULL;
	extInfo.m_pIndexOverride = NULL;
	IMesh *pExternalMesh = g_pShaderAPI->GetExternalMesh( extInfo );

	// Now make the external mesh point at the externally allocated data
	ExternalMeshData_t data;
	memset( &data, 0, sizeof(data) );
	if ( info.m_bExternalVB )
	{
		data.m_pVertexData = info.m_pVertexData;
		data.m_nVertexCount = info.m_nVertexCount;
		data.m_nVertexSizeInBytes = info.m_nVertexSizeInBytes;
	}
	if ( info.m_bExternalIB )
	{
		data.m_pIndexData = info.m_pIndexData;
		data.m_nIndexCount = info.m_nIndexCount;
	}
	g_pShaderAPI->SetExternalMeshData( pExternalMesh, data );

	return pExternalMesh;
#endif
}


//-----------------------------------------------------------------------------
// Copies queued vertex buffer data into the actual dynamic buffer
//-----------------------------------------------------------------------------
FORCEINLINE void CMatQueuedMesh::MST_CopyDynamicVB( const MeshDesc_t &desc, const uint8 *pVertexData, size_t nSizeInBytes )
{
	void *pDest;
	if ( desc.m_VertexSize_Position != 0 )
	{
		pDest = desc.m_pPosition;
	}
	else
	{
#define FindMin( desc, pCurrent, tag )	( ( desc.m_VertexSize_##tag != 0 ) ? MIN( pCurrent, desc.m_p##tag )  : pCurrent )

		pDest = (void *)((byte *)~0);

		pDest = FindMin( desc, pDest, BoneWeight );
		pDest = FindMin( desc, pDest, BoneMatrixIndex );
		pDest = FindMin( desc, pDest, Normal );
		pDest = FindMin( desc, pDest, Color );
		pDest = FindMin( desc, pDest, Specular );
		pDest = FindMin( desc, pDest, TangentS );
		pDest = FindMin( desc, pDest, TangentT );
		pDest = FindMin( desc, pDest, Wrinkle );

		for ( int i = 0; i < VERTEX_MAX_TEXTURE_COORDINATES; i++ )
		{
			if ( desc.m_VertexSize_TexCoord[i] && desc.m_pTexCoord[i] < pDest )
			{
				pDest = desc.m_pTexCoord[i];
			}
		}

#undef FindMin
	}

	Assert( pDest );
	if ( pDest )
	{
		FastCopy( (byte *)pDest, pVertexData, nSizeInBytes );
	}
}


//-----------------------------------------------------------------------------
// Copies queued index buffer data into the actual dynamic buffer
//-----------------------------------------------------------------------------
FORCEINLINE void CMatQueuedMesh::MST_CopyDynamicIB( const MeshDesc_t &desc, const uint16 *pIndexData, int nIndices )
{
	if ( !desc.m_nFirstVertex )
	{
		// AssertMsg(desc.m_pIndices & 0x03 == 0,"desc.m_pIndices is misaligned in CMatQueuedMesh::ExecuteDefferedBuild\n");
		FastCopy( (byte *)desc.m_pIndices, (const byte *)pIndexData, nIndices * sizeof(*pIndexData) );
		return;
	}

	ALIGN16 uint16 tempIndices[16];

	int i = 0;
	if ( (size_t)desc.m_pIndices % 4 == 2 )
	{
		desc.m_pIndices[i] = pIndexData[i] + desc.m_nFirstVertex;
		i++;
	}
	while ( i < nIndices )
	{
		int nToCopy = MIN( ARRAYSIZE(tempIndices), nIndices - i );
		for ( int j = 0; j < nToCopy; j++ )
		{
			tempIndices[j] = pIndexData[i+j] + desc.m_nFirstVertex;
		}
		FastCopy( (byte *)(desc.m_pIndices + i), (byte *)tempIndices, nToCopy * sizeof(uint16)  );
		i += nToCopy;
	}
}


//-----------------------------------------------------------------------------
// Gets the actual dynamic buffer (if necessary), and copies queued data into it
//-----------------------------------------------------------------------------
IMesh *CMatQueuedMesh::MST_SetupDynamicMesh( const MST_MeshInfo_t &info, IMesh *pExternalMesh )
{
	IMesh *pVertexOverride = info.m_pVertexOverride;
	IMesh *pIndexOverride = info.m_pIndexOverride;

	int nVerticesToLock = info.m_nVertexCount;
	int nIndicesToLock = info.m_nIndexCount;

#ifdef MS_NO_DYNAMIC_BUFFER_COPY
	// We are always going to treat external data as an override of a dynamic mesh
	// because that way colormesh/flexmesh state tracking is simpler: it
	// always goes through the dynamic mesh
	if ( info.m_bExternalVB )
	{
		pVertexOverride = pExternalMesh;
		nVerticesToLock = 0;
	}
	if ( info.m_bExternalIB )
	{
		pIndexOverride = pExternalMesh;
		nIndicesToLock = 0;
	}
#endif

	// Gets the dynamic mesh
	IMesh *pDynamicMesh;
	if ( !m_bFlexMesh )
	{
		pDynamicMesh = m_pHardwareContext->GetDynamicMeshEx( info.m_VertexFormat, ( ( info.m_nFlags & MQM_BUFFERED ) != 0 ), pVertexOverride, pIndexOverride, info.m_pMaterial );
	}
	else
	{
		pDynamicMesh = m_pHardwareContext->GetFlexMesh();
	}

	// Copies the buffers into the actual dynamic mesh
	if ( !nVerticesToLock && !nIndicesToLock )
		return pDynamicMesh;

	// Primitive type doesn't get sent down until the draw call.  
	// Because we lock based upon the primitive type (whether we're points or instanced quads), we
	// don't have a valid primitive type for our dynamic mesh, so set it here.  This will get
	// redundantly set on the draw call.
	pDynamicMesh->SetPrimitiveType( info.m_nPrimitiveType );

	MeshDesc_t desc;
	pDynamicMesh->LockMesh( nVerticesToLock, nIndicesToLock, desc );
	m_nMSTActualVertexOffsetInBytes = desc.m_nFirstVertex * desc.m_ActualVertexSize;
	if ( nVerticesToLock && desc.m_ActualVertexSize ) // if !desc.m_ActualVertexSize, device lost
	{
		MST_CopyDynamicVB( desc, info.m_pVertexData, info.m_nVertexCount * info.m_nVertexSizeInBytes );
	}

	if ( nIndicesToLock && ( info.m_pIndexData != &gm_ScratchIndexBuffer ) && desc.m_nIndexSize )
	{
		MST_CopyDynamicIB( desc, info.m_pIndexData, info.m_nIndexCount );
	}

	pDynamicMesh->UnlockMesh( nVerticesToLock, nIndicesToLock, desc );
	return pDynamicMesh;
}


//-----------------------------------------------------------------------------
// Main entry point for setting up the dynamic buffers on the material thread
//-----------------------------------------------------------------------------
void CMatQueuedMesh::MST_BuildDynamicBuffers( const MST_MeshInfo_t &info )
{
	// Think about the 4 cases each for VB and IB:
	// 1) External, 2) Dynamic, 3) Override, 4) Empty
	// We identify them this way:
	//		if info.m_pVertex/IndexOverride, it's an override
	//		else if info.m_nVertex/IndexCount == 0 it's empty
	//		else if info.m_bExternalIB/VB, it's external
	//		else it's dynamic
	// In general, any time we encounter an external buffer, we always have to make one.
	// Any time we encounter a dynamic buffer, we have to make one.
	// The strange cases are 
	//	* one is ext, the other is dynamic. 
	//		In that case, we make both and pass the external as an override to the dynamic.
	//	* Both are overrides, or one is an override and the other is empty
	//		In that case, we make only a dynamic and use the override(s)
	//	* Both are empty
	//		This is an illegal case.

	// We need data at least *somewhere* for this to make sense.
	Assert( info.m_pVertexOverride || info.m_nVertexCount || info.m_pIndexOverride || info.m_nIndexCount );

	// First, see if we need any external buffers. Then see if we need
	// dynamic buffers also
	IMesh *pExternalMesh = MST_SetupExternalMesh( info );
	m_pMSTActualMesh = MST_SetupDynamicMesh( info, pExternalMesh );

	// At this point, we're done with the data. We can free it.
	if ( info.m_pVertexData && !info.m_bExternalVB )
	{
		m_pOwner->FreeVertices( info.m_pVertexData, info.m_nVertexCount, info.m_nVertexSizeInBytes );
	}
	if ( info.m_pIndexData && !info.m_bExternalIB && ( info.m_pIndexData != &gm_ScratchIndexBuffer ) )
	{
		m_pOwner->FreeIndices( (uint8*)info.m_pIndexData, info.m_nIndexCount, sizeof(uint16) );
	}

	if ( info.m_pMaterial )
	{
		info.m_pMaterial->DecrementReferenceCount();
	}
}


//-----------------------------------------------------------------------------
// Main entry point for calling draw
//-----------------------------------------------------------------------------
void CMatQueuedMesh::MST_Draw( const MST_DrawInfo_t &info )
{
	m_pMSTActualMesh->SetPrimitiveType( info.m_Type );
	m_pMSTActualMesh->Draw( info.m_nFirstIndex, info.m_nIndexCount );
}


//-----------------------------------------------------------------------------
// Main entry point for calling draw modulated
//-----------------------------------------------------------------------------
void CMatQueuedMesh::MST_DrawModulated( const MST_DrawModulatedInfo_t &info )
{
	m_pMSTActualMesh->SetPrimitiveType( info.m_Type );
	m_pMSTActualMesh->DrawModulated( info.m_vecDiffuseModulation, info.m_nFirstIndex, info.m_nIndexCount );
}


//-----------------------------------------------------------------------------
//
// Methods called by the main (client) thread
//
//-----------------------------------------------------------------------------
bool CMatQueuedMesh::OnGetDynamicMesh( VertexFormat_t vertexFormat, unsigned flags, IMesh* pVertexOverride, IMesh* pIndexOverride, IMaterialInternal *pMaterial, int nHWSkinBoneCount )
{
	FreeBuffers();

	m_pVertexOverride = pVertexOverride;
	m_pIndexOverride = pIndexOverride;
	m_nFlags = flags;

	if ( !m_bFlexMesh )
	{
		if ( pVertexOverride )
		{
			m_VertexFormat = pVertexOverride->GetVertexFormat();
		}
		else
		{
			// Remove VERTEX_FORMAT_COMPRESSED from the material's format (dynamic meshes don't
			// support compression, and all materials should support uncompressed verts too)
			VertexFormat_t materialFormat = pMaterial->GetVertexFormat() & ~VERTEX_FORMAT_COMPRESSED;
			m_VertexFormat = ( vertexFormat != 0 ) ? vertexFormat : materialFormat;

			if ( vertexFormat != 0 )
			{
				int nVertexFormatBoneWeights = NumBoneWeights( vertexFormat );
				if ( nHWSkinBoneCount < nVertexFormatBoneWeights )
				{
					nHWSkinBoneCount = nVertexFormatBoneWeights;
				}
			}
			// Force the requested number of bone weights
			m_VertexFormat &= ~VERTEX_BONE_WEIGHT_MASK;
			m_VertexFormat |= VERTEX_BONEWEIGHT( nHWSkinBoneCount );
			if ( nHWSkinBoneCount > 0 )
			{
				m_VertexFormat |= VERTEX_BONE_INDEX;
			}
		}
	}
	else
	{
		m_VertexFormat = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_USE_EXACT_FORMAT;
		if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 92 )
		{
			m_VertexFormat |= VERTEX_WRINKLE;
		}
	}

	m_VertexSize = g_pShaderAPI->VertexFormatSize( m_VertexFormat );
	return true;
}

int CMatQueuedMesh::VertexCount() const
{
	return m_VertexSize ? m_nVerts : 0;
}

int CMatQueuedMesh::IndexCount() const
{
	return m_nIndices;
}

void CMatQueuedMesh::SetPrimitiveType( MaterialPrimitiveType_t type )
{
	// NOTE: Have to just hold onto the type here. We might not actually
	// have our meshes set up in the material system thread at this point
	// because we don't know if it's an external or a dynamic mesh
	// until unlock.
	m_Type = type;
//		m_pCallQueue->QueueCall( m_pLateBoundMesh, &IMesh::SetPrimitiveType, type );
}

void CMatQueuedMesh::LockMesh( int numVerts, int numIndices, MeshDesc_t& desc, MeshBuffersAllocationSettings_t *pSettings )
{
	if ( m_pVertexOverride )
	{
		numVerts = 0;
	}
	if ( m_pIndexOverride )
	{
		numIndices = 0;
	}

	m_nVerts = numVerts;
	m_nIndices = numIndices;

	if( numVerts > 0 )
	{
		Assert( m_VertexSize );
		Assert( !m_pVertexData );
		m_pVertexData = (byte *)m_pOwner->AllocVertices( numVerts, m_VertexSize, &m_bExternalVB );
		Assert( (uintp)m_pVertexData % 16 == 0 );

		// Compute the vertex index..
		desc.m_nFirstVertex = 0;
		static_cast< VertexDesc_t* >( &desc )->m_nOffset = 0;
		// Set up the mesh descriptor
		g_pShaderAPI->ComputeVertexDescription( m_pVertexData, m_VertexFormat, desc );
	}
	else
	{
		m_bExternalVB = false;
		desc.m_nFirstVertex = 0;
		static_cast< VertexDesc_t* >( &desc )->m_nOffset = 0;
		// Set up the mesh descriptor
		g_pShaderAPI->ComputeVertexDescription( 0, 0, desc );
	}

	if ( m_Type != MATERIAL_POINTS && numIndices > 0 )
	{
		Assert( !m_pIndexData );
		m_pIndexData = (uint16*)m_pOwner->AllocIndices( numIndices, sizeof(uint16), &m_bExternalIB );
		desc.m_pIndices = m_pIndexData;
		desc.m_nIndexSize = 1;
		desc.m_nFirstIndex = 0;
		static_cast< IndexDesc_t* >( &desc )->m_nOffset = 0;
	}
	else
	{
		m_bExternalIB = false;
		desc.m_pIndices = &gm_ScratchIndexBuffer;
		desc.m_nIndexSize = 0;
		desc.m_nFirstIndex = 0;
		static_cast< IndexDesc_t* >( &desc )->m_nOffset = 0;
	}
}

void CMatQueuedMesh::UnlockMesh( int numVerts, int numIndices, MeshDesc_t& desc )
{
	if ( m_pVertexData && numVerts < m_nVerts )
	{
		m_pVertexData = m_pOwner->ReallocVertices( m_pVertexData, m_nVerts, numVerts, m_VertexSize, m_bExternalVB );
		m_nVerts = numVerts;
	}

	if ( m_pIndexData && numIndices < m_nIndices )
	{
		m_pIndexData = (uint16*)m_pOwner->ReallocIndices( (byte*)m_pIndexData, m_nIndices, numIndices, sizeof(uint16), m_bExternalIB );
		m_nIndices = numIndices;
	}

	// Once we've unlocked the mesh, fire it off to the materialsystem thread.
	// The rules of mesh locking is that once Lock() is called, there's no
	// guarantee that any of the previous mesh still exists. However, since
	// flex meshes + dynamic meshes are separate concepts, the *flex* mesh can
	// remain if the dynamic mesh is locked, hence the queued mat render context
	// maintains two queued meshes, one for the flex, one for normal.
	QueueBuild( );
}


//-----------------------------------------------------------------------------
// Queues the buffers up to be 
//-----------------------------------------------------------------------------
void CMatQueuedMesh::QueueBuild( )
{
	if ( !m_pVertexOverride && !m_nVerts && !m_pIndexOverride && !m_nIndices )
		return;

	MST_MeshInfo_t info;
	info.m_pMaterial = m_pOwner->GetCurrentMaterialInternal();
	info.m_nFlags = 0; //m_nFlags;

	info.m_pVertexData = m_pVertexData;
	info.m_nVertexCount = m_nVerts;
	info.m_VertexFormat = m_VertexFormat;
	info.m_nVertexSizeInBytes = m_VertexSize;
	info.m_pVertexOverride = m_pVertexOverride;
	info.m_bExternalVB = m_bExternalVB;

	info.m_pIndexData = m_pIndexData;
	info.m_nIndexCount = m_nIndices;
	info.m_bExternalIB = m_bExternalIB;
	info.m_pIndexOverride = m_pIndexOverride;
	info.m_nPrimitiveType = m_Type;
	Assert( info.m_pIndexData || ( m_nIndices == 0 ) );
					  
	if ( info.m_pMaterial )
	{
		info.m_pMaterial->IncrementReferenceCount();
	}

	m_pCallQueue->QueueCall( this, &CMatQueuedMesh::MST_BuildDynamicBuffers, info );
	m_bCanSetAuxMeshes = true;
}


//-----------------------------------------------------------------------------
// Associates flex/color meshes with the dynamic mesh.
//-----------------------------------------------------------------------------
void CMatQueuedMesh::SetColorMesh( IMesh *pColorMesh, int nVertexOffset )
{
	// This only works on the main dynamic mesh
	Assert( !m_bFlexMesh );

	// This cannot be called in between the call to GetDynamicMesh + UnlockMesh;
	// the late bound mesh is in an indeterminant state during that point
	Assert( m_bCanSetAuxMeshes );
	m_pCallQueue->QueueCall( m_pLateBoundMesh, &IMesh::SetColorMesh, pColorMesh, nVertexOffset );
}

void CMatQueuedMesh::SetFlexMesh( IMesh *pMesh, int nVertexOffset )
{
	// This only works on the main dynamic mesh
	Assert( !m_bFlexMesh );

	// This cannot be called in between the call to GetDynamicMesh + UnlockMesh;
	// the late bound mesh is in an indeterminant state during that point
	Assert( m_bCanSetAuxMeshes );
	m_pCallQueue->QueueCall( m_pLateBoundMesh, &IMesh::SetFlexMesh, pMesh, nVertexOffset );
}

void CMatQueuedMesh::DisableFlexMesh()
{
	// This only works on the color mesh
	Assert( !m_bFlexMesh );

	// This cannot be called in between the call to GetDynamicMesh + UnlockMesh;
	// the late bound mesh is in an indeterminant state during that point
	Assert( m_bCanSetAuxMeshes );
	m_pCallQueue->QueueCall( m_pLateBoundMesh, &IMesh::DisableFlexMesh );
}

void CMatQueuedMesh::Draw( int firstIndex, int numIndices )
{
	if ( ( !m_nVerts && !m_pVertexOverride ) && ( !m_nIndices && !m_pIndexOverride ) )
	{
		MarkAsDrawn();
		return;
	}

	if ( ( m_Type == MATERIAL_INSTANCED_QUADS ) || ( m_Type == MATERIAL_POINTS ) )
	{
		if ( !m_nVerts && !m_pVertexOverride )
		{
			MarkAsDrawn();
			return;
		}
	}

	MST_DrawInfo_t info;
	info.m_Type = m_Type;
	info.m_nFirstIndex = firstIndex;
	info.m_nIndexCount = numIndices;

	m_pCallQueue->QueueCall( this, &CMatQueuedMesh::MST_Draw, info );
}

void CMatQueuedMesh::DrawModulated( const Vector4D &vecDiffuseModulation, int firstIndex, int numIndices )
{
	if ( ( !m_nVerts && !m_pVertexOverride ) && ( !m_nIndices && !m_pIndexOverride ) )
	{
		MarkAsDrawn();
		return;
	}

	if ( ( m_Type == MATERIAL_INSTANCED_QUADS ) || ( m_Type == MATERIAL_POINTS ) )
	{
		if ( !m_nVerts && !m_pVertexOverride )
		{
			MarkAsDrawn();
			return;
		}
	}

	MST_DrawModulatedInfo_t info;
	info.m_Type = m_Type;
	info.m_vecDiffuseModulation = vecDiffuseModulation;
	info.m_nFirstIndex = firstIndex;
	info.m_nIndexCount = numIndices;

	m_pCallQueue->QueueCall( this, &CMatQueuedMesh::MST_DrawModulated, info );
}

ICachedPerFrameMeshData *CMatQueuedMesh::GetCachedPerFrameMeshData()
{
	// Short-circuit if we have no vertices
	if ( m_nVerts == 0 )
		return NULL;

	CCachedPerFrameMeshData *pNewPerFrameData = new CCachedPerFrameMeshData();

	MST_MeshInfo_t &info = pNewPerFrameData->m_meshInfo;
	info.m_pMaterial = m_pOwner->GetCurrentMaterialInternal();
	info.m_nFlags = 0;

	info.m_pVertexData = m_pVertexData;
	info.m_nVertexCount = m_nVerts;
	info.m_VertexFormat = m_VertexFormat;
	info.m_nVertexSizeInBytes = m_VertexSize;
	info.m_pVertexOverride = m_pVertexOverride;
	info.m_bExternalVB = m_bExternalVB;

	info.m_pIndexData = m_pIndexData;
	info.m_nIndexCount = m_nIndices;
	info.m_bExternalIB = m_bExternalIB;
	info.m_pIndexOverride = m_pIndexOverride;
	info.m_nPrimitiveType = m_Type;
	Assert( info.m_pIndexData || ( m_nIndices == 0 ) );

	return pNewPerFrameData;
}

void CMatQueuedMesh::ReconstructFromCachedPerFrameMeshData( ICachedPerFrameMeshData *pData )
{
	CCachedPerFrameMeshData *pCachedData = (CCachedPerFrameMeshData*)pData;

	m_pVertexData = pCachedData->m_meshInfo.m_pVertexData;
	m_nVerts = pCachedData->m_meshInfo.m_nVertexCount;
	m_VertexFormat = pCachedData->m_meshInfo.m_VertexFormat;
	m_pVertexOverride = pCachedData->m_meshInfo.m_pVertexOverride;
	m_bExternalVB = pCachedData->m_meshInfo.m_bExternalVB;

	m_pIndexData = pCachedData->m_meshInfo.m_pIndexData;
	m_nIndices = pCachedData->m_meshInfo.m_nIndexCount;
	m_bExternalIB = pCachedData->m_meshInfo.m_bExternalIB;
	m_pIndexOverride = pCachedData->m_meshInfo.m_pIndexOverride;
	m_Type = pCachedData->m_meshInfo.m_nPrimitiveType;

	if( pCachedData->m_meshInfo.m_pVertexOverride || pCachedData->m_meshInfo.m_nVertexCount || pCachedData->m_meshInfo.m_pIndexOverride || pCachedData->m_meshInfo.m_nIndexCount )
	{
		if ( pCachedData->m_meshInfo.m_pMaterial )
		{
			pCachedData->m_meshInfo.m_pMaterial->IncrementReferenceCount();
		}

		m_pCallQueue->QueueCall( this, &CMatQueuedMesh::MST_BuildDynamicBuffers, pCachedData->m_meshInfo );
		m_bCanSetAuxMeshes = true;
	}
}

void CMatQueuedMesh::MarkAsDrawn()
{
	if ( m_bCanSetAuxMeshes )
	{
		m_pCallQueue->QueueCall( m_pLateBoundMesh, &IMesh::MarkAsDrawn );
	}
	FreeBuffers();
}

void CMatQueuedMesh::FreeBuffers()
{
	if ( m_pIndexData && ( m_pIndexData != &gm_ScratchIndexBuffer ) )
	{
		m_pOwner->FreeIndices( (byte*)m_pIndexData, m_nIndices, sizeof(uint16) );
	}
	m_nIndices = 0;
	m_pIndexData = NULL;

	if ( m_pVertexData )
	{
		m_pOwner->FreeVertices( m_pVertexData, m_nVerts, m_VertexSize );
	}

	m_nVerts = 0;
	m_VertexFormat = 0;
	m_VertexSize = 0;
	m_pVertexData = NULL;
	m_bCanSetAuxMeshes = false;
	m_nFlags = 0;
	m_bExternalIB = false;
	m_bExternalVB = false;
}

VertexFormat_t CMatQueuedMesh::GetVertexFormat() const
{
	return m_VertexFormat;
}


//-----------------------------------------------------------------------------
// Index buffer
//-----------------------------------------------------------------------------
class CMatQueuedIndexBuffer : public IIndexBuffer
{
	// Inherited from IIndexBuffer
public:
	virtual int IndexCount() const;
	virtual MaterialIndexFormat_t IndexFormat() const;
	virtual bool IsDynamic() const;
	virtual void BeginCastBuffer( MaterialIndexFormat_t format );
	virtual void EndCastBuffer();
	virtual int GetRoomRemaining() const;
	virtual bool Lock( int nMaxIndexCount, bool bAppend, IndexDesc_t &desc );
	virtual void Unlock( int nWrittenIndexCount, IndexDesc_t &desc );
	virtual void ModifyBegin( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t& desc );
	virtual void ModifyEnd( IndexDesc_t& desc );
	virtual void Spew( int nIndexCount, const IndexDesc_t &desc );
	virtual void ValidateData( int nIndexCount, const IndexDesc_t &desc );
	virtual IMesh *GetMesh() { return NULL; }
	
	// Other public methods, accessible from the main thread
public:
	CMatQueuedIndexBuffer( CMatQueuedRenderContext *pOwner, IMatRenderContextInternal *pHardwareContext );
	virtual ~CMatQueuedIndexBuffer();
	const void *GetIndexData() const;

	// These methods are expected to only be accessed from the render thread
public:
	int RT_GetIndexStart() const;
	IIndexBuffer* RT_GetDynamicIndexBuffer();

private:
	void ReleaseBuffer();
	void FreeIndexData( int nIndexCount, MaterialIndexFormat_t fmt, void *pIndexData );

	// These methods run in the render thread
	void RT_CopyIndexData( int nIndexCount, MaterialIndexFormat_t fmt, void *pIndexData, bool bIsExternal );
	void RT_FreeIndexData( int nIndexCount, MaterialIndexFormat_t fmt, void *pIndexData );

	CMatQueuedRenderContext *m_pOwner;
	CMatCallQueue *m_pCallQueue;
	IMatRenderContextInternal *m_pHardwareContext;
	void *m_pIndexData;
	int m_nIndices;
	bool m_bExternalIB;
	MaterialIndexFormat_t m_nIndexFormat;

	// This must only be accessed from the render thread
	int m_nRTStartIndex;
	IIndexBuffer *m_pRTDynamicIndexBuffer;
};


//----------------------------------------------------------------------------
// Constructor, destructor
//----------------------------------------------------------------------------
CMatQueuedIndexBuffer::CMatQueuedIndexBuffer( CMatQueuedRenderContext *pOwner, IMatRenderContextInternal *pHardwareContext ) :
	m_pOwner( pOwner ), 
	m_pCallQueue( pOwner->GetCallQueueInternal() ),
	m_pHardwareContext( pHardwareContext ),
	m_pIndexData( NULL ),
	m_nIndices( 0 ),
	m_nIndexFormat( MATERIAL_INDEX_FORMAT_UNKNOWN ),
	m_nRTStartIndex( -1 ),
	m_pRTDynamicIndexBuffer( 0 ),
	m_bExternalIB( false )
{
}

CMatQueuedIndexBuffer::~CMatQueuedIndexBuffer()
{
	if ( m_pIndexData )
	{
		FreeIndexData( m_nIndices, m_nIndexFormat, m_pIndexData );
		m_pIndexData = NULL;
		m_nIndices = 0;
	}
}

const void *CMatQueuedIndexBuffer::GetIndexData() const
{ 
	return m_pIndexData; 
}

int CMatQueuedIndexBuffer::IndexCount() const
{
	return m_nIndices;
}

MaterialIndexFormat_t CMatQueuedIndexBuffer::IndexFormat() const
{
	return m_nIndexFormat;
}

bool CMatQueuedIndexBuffer::IsDynamic() const
{
	// Queued index buffers are only interesting for dynamic index buffers
	// static ones need not use this
	return true;
}

void CMatQueuedIndexBuffer::BeginCastBuffer( MaterialIndexFormat_t format )
{
	// Recasting this buffer has the effect of causing us to not need this index data any more
	ReleaseBuffer();
	m_nIndexFormat = format;
}

void CMatQueuedIndexBuffer::EndCastBuffer()
{
	ReleaseBuffer();
	m_nIndexFormat = MATERIAL_INDEX_FORMAT_UNKNOWN;
}

// Returns the number of indices that can still be written into the buffer
int CMatQueuedIndexBuffer::GetRoomRemaining() const
{
	return m_pOwner->GetMaxIndicesToRender() - m_nIndices;
}

int CMatQueuedIndexBuffer::RT_GetIndexStart() const
{
	return m_nRTStartIndex;
}

IIndexBuffer* CMatQueuedIndexBuffer::RT_GetDynamicIndexBuffer()
{
	return m_pRTDynamicIndexBuffer;
}


void CMatQueuedIndexBuffer::FreeIndexData( int nIndexCount, MaterialIndexFormat_t fmt, void *pIndexData )
{
	if ( pIndexData )
	{
		Assert( fmt != MATERIAL_INDEX_FORMAT_UNKNOWN );
		int nIndexSize = ( fmt == MATERIAL_INDEX_FORMAT_16BIT ) ? sizeof(uint16) : sizeof(uint32);
		m_pOwner->FreeIndices( (byte*)pIndexData, nIndexCount, nIndexSize );
	}
}

void CMatQueuedIndexBuffer::RT_CopyIndexData( int nIndexCount, MaterialIndexFormat_t fmt, void *pIndexData, bool bIsExternal )
{
#ifdef MS_NO_DYNAMIC_BUFFER_COPY
	if ( bIsExternal )
	{
		Assert( !m_pRTDynamicIndexBuffer );
		Assert( fmt == MATERIAL_INDEX_FORMAT_16BIT || fmt == MATERIAL_INDEX_FORMAT_UNKNOWN );
		m_pRTDynamicIndexBuffer = g_pShaderAPI->GetExternalIndexBuffer( nIndexCount, (uint16*)pIndexData );
		m_nRTStartIndex = 0;
		return;
	}
#endif

	IndexDesc_t desc;
	Assert( !m_pRTDynamicIndexBuffer );
	m_pRTDynamicIndexBuffer = m_pHardwareContext->GetDynamicIndexBuffer();
	if ( !m_pRTDynamicIndexBuffer->Lock( nIndexCount, false, desc ) )
	{
		m_pRTDynamicIndexBuffer = NULL;
		m_nRTStartIndex = -1;
		return;
	}

	int nIndexSize = sizeof( uint16 ) * desc.m_nIndexSize;
	m_nRTStartIndex = desc.m_nOffset / nIndexSize;
	if ( pIndexData && desc.m_nIndexSize )
	{
		FastCopy( (byte *)desc.m_pIndices, (byte *)pIndexData, nIndexCount * nIndexSize );
	}
	m_pRTDynamicIndexBuffer->Unlock( nIndexCount, desc );
}

void CMatQueuedIndexBuffer::RT_FreeIndexData( int nIndexCount, MaterialIndexFormat_t fmt, void *pIndexData )
{
	FreeIndexData( nIndexCount, fmt, pIndexData );
	m_pRTDynamicIndexBuffer = NULL;
	m_nRTStartIndex = -1;
}

void CMatQueuedIndexBuffer::ReleaseBuffer()
{
	if ( m_pIndexData )
	{
		m_pCallQueue->QueueCall( this, &CMatQueuedIndexBuffer::RT_FreeIndexData, m_nIndices, m_nIndexFormat, m_pIndexData );
		m_pIndexData = 0;
		m_nIndices = 0;
		m_bExternalIB = false;
	}
}

bool CMatQueuedIndexBuffer::Lock( int nMaxIndexCount, bool bAppend, IndexDesc_t &desc )
{
	// Locking this buffer has the effect of causing us to not need this index data any more
	ReleaseBuffer();

	// Append mode is not supported. We need to kill it altogether.
	if ( bAppend )
		return false;

	m_nIndices = nMaxIndexCount;
	int nIndexSize = ( m_nIndexFormat == MATERIAL_INDEX_FORMAT_16BIT ) ? sizeof(uint16) : sizeof(uint32);
	m_pIndexData = m_pOwner->AllocIndices( nMaxIndexCount, nIndexSize, &m_bExternalIB );
	desc.m_pIndices = (uint16*)m_pIndexData;
	desc.m_nIndexSize = nIndexSize >> 1;
	desc.m_nFirstIndex = 0;
	desc.m_nOffset = 0;
	return ( m_pIndexData != 0 );
}

void CMatQueuedIndexBuffer::Unlock( int nWrittenIndexCount, IndexDesc_t &desc )
{
	if ( m_pIndexData && nWrittenIndexCount < m_nIndices )
	{
		m_pIndexData = m_pOwner->ReallocIndices( (byte*)m_pIndexData, m_nIndices, nWrittenIndexCount, desc.m_nIndexSize * sizeof(uint16), m_bExternalIB );
	}
	m_nIndices = nWrittenIndexCount;

	// Unlocking this buffer has the effect of queuing a call to 
	// write these indices into the dynamic indexbuffer, caching off the
	// base index it was written into
	if ( m_pIndexData )
	{
		m_pCallQueue->QueueCall( this, &CMatQueuedIndexBuffer::RT_CopyIndexData, m_nIndices, m_nIndexFormat, m_pIndexData, m_bExternalIB );
	}
}

void CMatQueuedIndexBuffer::ModifyBegin( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t& desc )
{
	CannotSupport();
}

void CMatQueuedIndexBuffer::ModifyEnd( IndexDesc_t& desc )
{
	CannotSupport();
}

void CMatQueuedIndexBuffer::Spew( int nIndexCount, const IndexDesc_t &desc  )
{
}

void CMatQueuedIndexBuffer::ValidateData( int nIndexCount, const IndexDesc_t &desc )
{
}



//-----------------------------------------------------------------------------
//
// MatQueuedRenderContext starts here
//
//-----------------------------------------------------------------------------
#ifdef MS_NO_DYNAMIC_BUFFER_COPY
CMemoryStack CMatQueuedRenderContext::s_Vertices[RENDER_CONTEXT_STACKS];
CMemoryStack CMatQueuedRenderContext::s_Indices[RENDER_CONTEXT_STACKS];

#ifdef _PS3
CPs3gcmLocalMemoryBlock s_RSXMemory;
#endif

int CMatQueuedRenderContext::s_nCurStack = 0;
bool CMatQueuedRenderContext::s_bInitializedStacks = false;


#define DYNAMIC_VERTEX_BUFFER_BLOCK_SIZE	128 * 1024 
#define DYNAMIC_INDEX_BUFFER_BLOCK_SIZE		 16 * 1024

#define DYNAMIC_VERTEX_BUFFER_TOTAL_SIZE	(1536+128) * 1024
#define DYNAMIC_INDEX_BUFFER_TOTAL_SIZE		(128+128) * 1024

#define DYNAMIC_VERTEX_BUFFER_ALIGNMENT		16
#define DYNAMIC_INDEX_BUFFER_ALIGNMENT		4
#endif

void AllocateScratchRSXMemory()
{
#if _PS3
	s_RSXMemory.Alloc( kAllocPs3GcmDynamicBufferPool, 
		RENDER_CONTEXT_STACKS * ( ( DYNAMIC_VERTEX_BUFFER_TOTAL_SIZE + DYNAMIC_VERTEX_BUFFER_ALIGNMENT ) +
		( DYNAMIC_INDEX_BUFFER_TOTAL_SIZE + DYNAMIC_INDEX_BUFFER_ALIGNMENT ) ) );
#endif
}

void CMatQueuedRenderContext::Init( CMaterialSystem *pMaterialSystem, CMatRenderContextBase *pHardwareContext )
{
	BaseClass::Init();

	m_pMaterialSystem = pMaterialSystem;
	m_pHardwareContext = pHardwareContext;

	m_pQueuedMesh = new CMatQueuedMesh( this, pHardwareContext, false );
	m_pQueuedFlexMesh = new CMatQueuedMesh( this, pHardwareContext, true );
	m_pQueuedIndexBuffer = new CMatQueuedIndexBuffer( this, pHardwareContext );

	MEM_ALLOC_CREDIT();

#ifdef MS_NO_DYNAMIC_BUFFER_COPY
	if ( !s_bInitializedStacks )
	{
#if _PS3
		uint8 *pMem = (uint8*)s_RSXMemory.DataInLocalMemory();
#endif
		// NOTE: Allocation size must be at least double DYNAMIC_VERTEX_BUFFER_BLOCK_SIZE
		// or DYNAMIC_INDEX_BUFFER_BLOCK_SIZE to avoid massive overflow
		for ( int i = 0; i < RENDER_CONTEXT_STACKS; ++i )
		{
			CFmtStr verticesName( "CMatQueuedRenderContext::s_Vertices[%d]", i );
			CFmtStr indicesName(  "CMatQueuedRenderContext::s_Vertices[%d]", i );
#ifdef _X360
			s_Vertices[i].InitPhysical( (const char *)verticesName, DYNAMIC_VERTEX_BUFFER_TOTAL_SIZE, 0, DYNAMIC_VERTEX_BUFFER_ALIGNMENT, PAGE_WRITECOMBINE );
			s_Indices[i].InitPhysical( (const char *)indicesName, DYNAMIC_INDEX_BUFFER_TOTAL_SIZE, 0, DYNAMIC_INDEX_BUFFER_ALIGNMENT, PAGE_WRITECOMBINE );
#elif defined( _PS3 )
			s_Vertices[i].InitPhysical( (const char *)verticesName, DYNAMIC_VERTEX_BUFFER_TOTAL_SIZE, 0, DYNAMIC_VERTEX_BUFFER_ALIGNMENT, (uint32)pMem );
			pMem += DYNAMIC_VERTEX_BUFFER_TOTAL_SIZE + DYNAMIC_VERTEX_BUFFER_ALIGNMENT;
			s_Indices[i].InitPhysical( (const char *)indicesName, DYNAMIC_INDEX_BUFFER_TOTAL_SIZE, 0, DYNAMIC_INDEX_BUFFER_ALIGNMENT, (uint32)pMem );
			pMem += DYNAMIC_INDEX_BUFFER_TOTAL_SIZE + DYNAMIC_INDEX_BUFFER_ALIGNMENT;
#else
#pragma error
#endif
		}
		s_bInitializedStacks = true;
	}
	s_nCurStack++;
	if ( s_nCurStack > 2 )
	{
		s_nCurStack = 0;
	}
	m_pVertices = &s_Vertices[ s_nCurStack ];
	m_pIndices = &s_Indices[ s_nCurStack ];
#endif

	unsigned int vertSize	= 16 * 1024 * 1024;
	unsigned int indSize	= 16 * 1024 * 1024;
#ifdef DX_TO_GL_ABSTRACTION
	vertSize	= 12 * 1024 * 1024;
	indSize		= 4 * 1024 * 1024;
#endif

	m_Vertices.Init( "CMatQueuedRenderContext::m_Vertices", vertSize, 128 * 1024 );
	m_Indices.Init( "CMatQueuedRenderContext::m_Indices",  indSize, 128 * 1024 );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::Shutdown()
{
	if ( !m_pHardwareContext )
		return;

	Assert( !m_pCurrentMaterial );

	delete m_pQueuedMesh;
	delete m_pQueuedFlexMesh;
	delete m_pQueuedIndexBuffer;
	m_pMaterialSystem = NULL;
	m_pHardwareContext = NULL;
	m_pQueuedMesh = NULL;
	m_pQueuedFlexMesh = NULL;
	m_pQueuedIndexBuffer = NULL;

	m_Vertices.Term();
	m_Indices.Term();

	BaseClass::Shutdown();
	Assert(m_queue.Count() == 0);
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::CompactMemory()
{
	BaseClass::CompactMemory();

	m_Vertices.FreeAll();
	m_Indices.FreeAll();
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::CycleDynamicBuffers( )
{
#ifdef MS_NO_DYNAMIC_BUFFER_COPY
	s_nCurStack++;
	if ( s_nCurStack > 2 )
	{
		s_nCurStack = 0;
	}
	m_pVertices = &s_Vertices[ s_nCurStack ];
	m_pIndices = &s_Indices[ s_nCurStack ];
	g_pShaderAPI->FlushGPUCache( m_pVertices->GetBase(), m_pVertices->GetUsed() );
	g_pShaderAPI->FlushGPUCache( m_pIndices->GetBase(), m_pIndices->GetUsed() );
	m_pVertices->FreeAll( false );
	m_pIndices->FreeAll( false );
#endif
}

void CMatQueuedRenderContext::BeginQueue( CMatRenderContextBase *pInitialState )
{
	if ( !pInitialState )
	{
		pInitialState = m_pHardwareContext;
	}

	CycleDynamicBuffers();

	CMatRenderContextBase::InitializeFrom( pInitialState );
	g_pShaderAPI->GetBackBufferDimensions( m_WidthBackBuffer, m_HeightBackBuffer );
	m_FogMode = pInitialState->GetFogMode();
	m_nBoneCount = pInitialState->GetCurrentNumBones();
	pInitialState->GetFogDistances( &m_flFogStart, &m_flFogEnd, &m_flFogZ );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::EndQueue( bool bCallQueued )
{
	if ( bCallQueued )
	{
		CallQueued();
	}
	int i;

	if ( m_pCurrentMaterial )
	{
		m_pCurrentMaterial = NULL;
	}

	if ( m_pUserDefinedLightmap )
	{
		m_pUserDefinedLightmap = NULL;
	}

	if ( m_pLocalCubemapTexture )
	{
		m_pLocalCubemapTexture = NULL;
	}

	for ( i = 0; i < MAX_FB_TEXTURES; i++ )
	{
		if ( m_pCurrentFrameBufferCopyTexture[i] )
		{
			m_pCurrentFrameBufferCopyTexture[i] = NULL;
		}
	}

	for ( i = 0; i < m_RenderTargetStack.Count(); i++ )
	{
		for ( int j = 0; j < MAX_RENDER_TARGETS; j++ )
		{
			if ( m_RenderTargetStack[i].m_pRenderTargets[j] )
			{
				m_RenderTargetStack[i].m_pRenderTargets[j] = NULL;
			}
		}
	}

	m_RenderTargetStack.Clear();

	m_ScissorRectStack.Clear();
}


void CMatQueuedRenderContext::Bind( IMaterial *iMaterial, void *proxyData )
{
	if ( !iMaterial )
	{
		if( !g_pErrorMaterial )
			return;
		iMaterial = static_cast<IMaterialInternal *>( g_pErrorMaterial );
	}
	else
	{
		iMaterial = static_cast< IMaterialInternal*>( iMaterial )->GetRealTimeVersion(); //always work with the real time versions of materials internally
	}

	m_pCurrentMaterial = static_cast< IMaterialInternal*>( iMaterial );
	m_pCurrentProxyData = proxyData;

	if ( !m_pCurrentMaterial->HasQueueFriendlyProxies() )
	{
		// We've always gotta call the bind proxy (assuming there is one)
		// so we can copy off the material vars at this point.
		// However in case the material must have proxies bound on QMS then we don't call bind proxies
		// now and rely on queued material bind to setup proxies on QMS.
		m_pCurrentMaterial->CallBindProxy( proxyData, &m_CallQueueExternal );
	}

	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::Bind, iMaterial, proxyData );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::BeginRender()
{
	if ( ++m_iRenderDepth == 1 )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::BeginRender );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::EndRender()
{
	// This can fail if someone holds a render context pointer over a video mode change.  Find it if you hit this.
	Assert(m_pHardwareContext);
	if ( --m_iRenderDepth == 0 )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::EndRender );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::CallQueued( bool bTermAfterCall )
{
	if ( mat_report_queue_status.GetBool() )
	{
#ifndef MS_NO_DYNAMIC_BUFFER_COPY
		Msg( "%d calls queued for %d bytes in parameters and overhead, %d bytes verts, %d bytes indices, %d bytes other\n", 
			m_queue.Count(), m_queue.GetMemoryUsed(), Vertices().GetUsed(), Indices().GetUsed(), RenderDataSizeUsed() );
#else
		Msg( "%d calls queued for %d bytes in parameters and overhead, %d bytes verts, %d bytes indices, %d bytes other\n", 
			m_queue.Count(), m_queue.GetMemoryUsed(), Vertices().GetUsed() + m_Vertices.GetUsed(), Indices().GetUsed() + m_Indices.GetUsed(), RenderDataSizeUsed() );
#endif
	}

	m_queue.CallQueued();

#if defined( MS_NO_DYNAMIC_BUFFER_COPY ) && !defined( _CERT )
	static int s_nFrameCount = 0;
	static int s_nVBOverflowCount = 0;
	static int s_nIBOverflowCount = 0;

	++s_nFrameCount;
	if ( m_Vertices.GetUsed() > 0 )
	{
		++s_nVBOverflowCount;
	}

	if ( m_Indices.GetUsed() > 0 )
	{
		++s_nIBOverflowCount;
	}

	if ( s_nFrameCount > 1024 )
	{
		static bool s_bVBWarned = false;
		static bool s_bIBWarned = false;
		if ( !s_bVBWarned )
		{
			if ( ( (float)s_nVBOverflowCount / (float)s_nFrameCount ) > 0.1f )
			{
				for ( int w = 0; w < 30; ++w )
				{
					Msg( "[Ignore this for splitscreen] Tell Brian to up the VB memory (and which map this occurred on)!\n" );
				}
				s_bVBWarned = true;
			}
		}
		if ( !s_bIBWarned )
		{
			if ( ( (float)s_nIBOverflowCount / (float)s_nFrameCount ) > 0.1f )
			{
				for ( int w = 0; w < 30; ++w )
				{
					Msg( "[Ignore this for splitscreen] Tell Brian to up the IB memory (and which map this occurred on)!\n" );
				}
				s_bIBWarned = true;
			}
		}
	}
#endif

#if 0
	static int s_nVHisto[ 33 ];
	static int s_nIHisto[ 9 ];
	static int s_nHistoCount;
	int nMem = ( Vertices().GetUsed() + m_Vertices.GetUsed() + ( 64 * 1024 ) - 1 ) / ( 64 * 1024 );
	nMem = clamp( nMem, 0, 32 );
	s_nVHisto[ nMem ]++;
	nMem = ( Indices().GetUsed() + m_Indices.GetUsed() + ( 32 * 1024 ) - 1 ) / ( 32 * 1024 );
	nMem = clamp( nMem, 0, 8 );
	s_nIHisto[ nMem ]++;
	if ( ( ++s_nHistoCount % 1024 ) == 0 )
	{
		Msg( "Verts:" );
		bool bFound = false;
		for( int i = 32; i >= 0; --i )
		{
			if ( s_nVHisto[i] )
			{
				bFound = true;
			}
			if ( !bFound )
				continue;
			Msg( "[%dk %d] ", i * 64, s_nVHisto[i] );
		}
		Msg( "\n" );
		Msg( "Indices: " );
		bFound = false;
		for( int i = 8; i >= 0; --i )
		{
			if ( s_nIHisto[i] )
			{
				bFound = true;
			}
			if ( !bFound )
				continue;
			Msg( "[%dk %d] ", i * 32, s_nIHisto[i] );
		}
		Msg( "\n" );
	}
#endif

	m_Vertices.FreeAll( false );
	m_Indices.FreeAll( false );

	if ( bTermAfterCall )
	{
		Shutdown();
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::FlushQueued()
{
	m_queue.Flush();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
ICallQueue *CMatQueuedRenderContext::GetCallQueue()
{
	return &m_CallQueueExternal;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SetRenderTargetEx( int nRenderTargetID, ITexture *pNewTarget ) 
{
	CMatRenderContextBase::SetRenderTargetEx( nRenderTargetID, pNewTarget );

	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetRenderTargetEx, nRenderTargetID, pNewTarget );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::GetRenderTargetDimensions( int &width, int &height) const
{
	// Target at top of stack
	ITexture *pTOS = m_RenderTargetStack.Top().m_pRenderTargets[0];

	// If top of stack isn't the back buffer, get dimensions from the texture
	if ( pTOS != NULL )
	{
		width = pTOS->GetActualWidth();
		height = pTOS->GetActualHeight();
	}
	else // otherwise, get them from the shader API
	{
		width = m_WidthBackBuffer;
		height = m_HeightBackBuffer;
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::Viewport( int x, int y, int width, int height )
{
	CMatRenderContextBase::Viewport( x, y, width, height );
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::Viewport, x, y, width, height );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SetLights( int nCount, const LightDesc_t *pLights )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetLights, nCount, m_queue.CopyArray( pLights, nCount ) );
}

void CMatQueuedRenderContext::SetLightingState( const MaterialLightingState_t &state )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetLightingState, RefToVal( state ) );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SetLightingOrigin( Vector vLightingOrigin )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetLightingOrigin, vLightingOrigin );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SetAmbientLightCube( LightCube_t cube )
{
	// FIXME: does compiler do the right thing, is envelope needed?
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetAmbientLightCube, m_queue.CopyArray( &cube[0], 6 ) );
}

//-----------------------------------------------------------------------------
// Bone count
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SetNumBoneWeights( int nBoneCount )
{
	m_nBoneCount = nBoneCount;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetNumBoneWeights, nBoneCount );
}

int	CMatQueuedRenderContext::GetCurrentNumBones( ) const
{
	return m_nBoneCount;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::FogMode( MaterialFogMode_t fogMode )
{
	m_FogMode = fogMode;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::FogMode, fogMode );
}

void CMatQueuedRenderContext::FogStart( float fStart )
{
	m_flFogStart = fStart;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::FogStart, fStart );
}

void CMatQueuedRenderContext::FogEnd( float fEnd )
{
	m_flFogEnd = fEnd;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::FogEnd, fEnd );
}

void CMatQueuedRenderContext::FogMaxDensity( float flMaxDensity )
{
	m_flFogMaxDensity = flMaxDensity;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::FogMaxDensity, flMaxDensity );
}

void CMatQueuedRenderContext::SetFogZ( float fogZ )
{
	m_flFogZ = fogZ;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetFogZ, fogZ );
}

MaterialFogMode_t CMatQueuedRenderContext::GetFogMode( void )
{
	return m_FogMode;
}

void CMatQueuedRenderContext::FogColor3f( float r, float g, float b )
{
	FogColor3ub( clamp( (int)(r * 255.0f), 0, 255 ), clamp( (int)(g * 255.0f), 0, 255 ), clamp( (int)(b * 255.0f), 0, 255 ) );
}

void CMatQueuedRenderContext::FogColor3fv( float const* rgb )
{
	FogColor3ub( clamp( (int)(rgb[0] * 255.0f), 0, 255 ), clamp( (int)(rgb[1] * 255.0f), 0, 255 ), clamp( (int)(rgb[2] * 255.0f), 0, 255 ) );
}

void CMatQueuedRenderContext::FogColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
	m_FogColor.r = r;
	m_FogColor.g = g;
	m_FogColor.b = b;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::FogColor3ub, r, g, b );
}

void CMatQueuedRenderContext::FogColor3ubv( unsigned char const* rgb )
{
	FogColor3ub( rgb[0], rgb[1], rgb[2] );
}

void CMatQueuedRenderContext::GetFogColor( unsigned char *rgb )
{
	rgb[0] = m_FogColor.r;
	rgb[1] = m_FogColor.g;
	rgb[2] = m_FogColor.b;
}

void CMatQueuedRenderContext::GetFogDistances( float *fStart, float *fEnd, float *fFogZ )
{
	if( fStart )
		*fStart = m_flFogStart;

	if( fEnd )
		*fEnd = m_flFogEnd;

	if( fFogZ )
		*fFogZ = m_flFogZ;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::GetViewport( int& x, int& y, int& width, int& height ) const
{
	// Verify valid top of RT stack
	Assert ( m_RenderTargetStack.Count() > 0 );

	// Grab the top of stack
	const RenderTargetStackElement_t& element = m_RenderTargetStack.Top();

	// If either dimension is negative, set to full bounds of current target
	if ( (element.m_nViewW < 0) || (element.m_nViewH < 0) )
	{
		// Viewport origin at target origin
		x = y = 0;

		// If target is back buffer
		if ( element.m_pRenderTargets[0] == NULL )
		{
			width = m_WidthBackBuffer;
			height = m_HeightBackBuffer;
		}
		else // if target is texture
		{
			width = element.m_pRenderTargets[0]->GetActualWidth();
			height = element.m_pRenderTargets[0]->GetActualHeight();
		}
	}
	else // use the bounds from the stack directly
	{
		x = element.m_nViewX;
		y = element.m_nViewY;
		width = element.m_nViewW;
		height = element.m_nViewH;
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SyncToken( const char *p )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SyncToken, m_queue.Copy( p ) );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IIndexBuffer* CMatQueuedRenderContext::GetDynamicIndexBuffer()
{
	return m_pQueuedIndexBuffer;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IMesh* CMatQueuedRenderContext::GetDynamicMesh( bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride, IMaterial *pAutoBind )
{
	return GetDynamicMeshEx( 0, buffered, pVertexOverride, pIndexOverride, pAutoBind );
}

IMesh* CMatQueuedRenderContext::GetDynamicMeshEx( VertexFormat_t vertexFormat, bool bBuffered, IMesh* pVertexOverride, IMesh* pIndexOverride, IMaterial *pAutoBind )
{
	if( pAutoBind )
	{
		Bind( pAutoBind, NULL );
	}

	if ( pVertexOverride && pIndexOverride )
	{
		// Use the new batch API
		DebuggerBreak();
		return NULL;
	}

	if ( pVertexOverride )
	{
		if ( CompressionType( pVertexOverride->GetVertexFormat() ) != VERTEX_COMPRESSION_NONE )
		{
			// UNDONE: support compressed dynamic meshes if needed (pro: less VB memory, con: time spent compressing)
			DebuggerBreak();
			return NULL;
		}
	}

	// For anything more than 1 bone, imply the last weight from the 1 - the sum of the others.
	int nCurrentBoneCount = GetCurrentNumBones();
	Assert( nCurrentBoneCount <= 4 );
	if ( nCurrentBoneCount > 1 )
	{
		--nCurrentBoneCount;
	}

	m_pQueuedMesh->OnGetDynamicMesh( vertexFormat, ( bBuffered ) ? MQM_BUFFERED : 0, pVertexOverride, pIndexOverride, GetCurrentMaterialInternal(), nCurrentBoneCount );
	return m_pQueuedMesh;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CMatQueuedRenderContext::GetMaxVerticesToRender( IMaterial *pMaterial )
{
	pMaterial = ((IMaterialInternal *)pMaterial)->GetRealTimeVersion(); //always work with the real time version of materials internally.

	// Be conservative, assume no compression (in here, we don't know if the caller will used a compressed VB or not)
	// FIXME: allow the caller to specify which compression type should be used to compute size from the vertex format
	//        (this can vary between multiple VBs/Meshes using the same material)
	VertexFormat_t materialFormat = pMaterial->GetVertexFormat() & ~VERTEX_FORMAT_COMPRESSED;
	int nVertexFormatSize = g_pShaderAPI->VertexFormatSize( materialFormat );
	if ( nVertexFormatSize == 0 )
	{
		Warning( "bad vertex size for material %s\n", pMaterial->GetName() );
		return 65535;
	}

#ifdef MS_NO_DYNAMIC_BUFFER_COPY
	int nDynamicVBSize = DYNAMIC_VERTEX_BUFFER_BLOCK_SIZE;
#else
	int nDynamicVBSize = g_pShaderAPI->GetCurrentDynamicVBSize();
#endif
	int maxVerts = nDynamicVBSize / nVertexFormatSize;
	if ( maxVerts > 65535 )
	{
		maxVerts = 65535;
	}
	return maxVerts;
}

int CMatQueuedRenderContext::GetMaxIndicesToRender( )
{
#ifdef MS_NO_DYNAMIC_BUFFER_COPY
	return DYNAMIC_INDEX_BUFFER_BLOCK_SIZE / sizeof(uint16);
#else
	return INDEX_BUFFER_SIZE;
#endif
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices )
{
	Assert( !bMaxUntilFlush );

#ifdef MS_NO_DYNAMIC_BUFFER_COPY
	int nDynamicVBSize = DYNAMIC_VERTEX_BUFFER_BLOCK_SIZE;
	int nDynamicIBSize = DYNAMIC_INDEX_BUFFER_BLOCK_SIZE;
#else
	int nDynamicVBSize = g_pShaderAPI->GetCurrentDynamicVBSize();
	int nDynamicIBSize = INDEX_BUFFER_SIZE * sizeof(uint16);
#endif

	*pMaxVerts = nDynamicVBSize / m_pQueuedMesh->GetVertexSize();
	if ( *pMaxVerts > 65535 )
	{
		*pMaxVerts = 65535;
	}
	*pMaxIndices = nDynamicIBSize / sizeof(uint16);
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IMesh *CMatQueuedRenderContext::GetFlexMesh()
{
	m_pQueuedFlexMesh->OnGetDynamicMesh( 0, 0, NULL, NULL, NULL, 0 );
	return m_pQueuedFlexMesh;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
OcclusionQueryObjectHandle_t CMatQueuedRenderContext::CreateOcclusionQueryObject()
{
	OcclusionQueryObjectHandle_t h = g_pOcclusionQueryMgr->CreateOcclusionQueryObject();
	m_queue.QueueCall( g_pOcclusionQueryMgr, &COcclusionQueryMgr::OnCreateOcclusionQueryObject, h );
	return h;
}

int CMatQueuedRenderContext::OcclusionQuery_GetNumPixelsRendered( OcclusionQueryObjectHandle_t h )
{
	m_queue.QueueCall( g_pOcclusionQueryMgr, &COcclusionQueryMgr::OcclusionQuery_IssueNumPixelsRenderedQuery, h );
	return g_pOcclusionQueryMgr->OcclusionQuery_GetNumPixelsRendered( h, false );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SetFlashlightState( const FlashlightState_t &s, const VMatrix &m )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetFlashlightState, RefToVal( s ), RefToVal( m ) );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMatQueuedRenderContext::EnableClipping( bool bEnable )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::EnableClipping, bEnable );
	return BaseClass::EnableClipping( bEnable );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::UserClipTransform( const VMatrix &m )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::UserClipTransform, RefToVal( m ) );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::GetWindowSize( int &width, int &height ) const
{
	width = m_WidthBackBuffer;
	height = m_HeightBackBuffer;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::DrawScreenSpaceRectangle( 
	IMaterial *pMaterial,
	int destx, int desty,
	int width, int height,
	float src_texture_x0, float src_texture_y0,			// which texel you want to appear at
	// destx/y
	float src_texture_x1, float src_texture_y1,			// which texel you want to appear at
	// destx+width-1, desty+height-1
	int src_texture_width, int src_texture_height,		// needed for fixup
	void *pClientRenderable,
	int nXDice, int nYDice )							// Amount to tessellate the quad
{
	IMaterial *pRealTimeVersionMaterial = ((IMaterialInternal *)pMaterial)->GetRealTimeVersion();
	pRealTimeVersionMaterial->CallBindProxy( pClientRenderable, &m_CallQueueExternal );
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::DrawScreenSpaceRectangle, pMaterial, destx, desty, width, height, src_texture_x0, src_texture_y0, src_texture_x1, src_texture_y1,	src_texture_width, src_texture_height, pClientRenderable, nXDice, nYDice );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::LoadBoneMatrix( int i, const matrix3x4_t &m )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::LoadBoneMatrix, i, RefToVal( m ) );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::CopyRenderTargetToTextureEx( ITexture *pTexture, int i, Rect_t *pSrc, Rect_t *pDst )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::CopyRenderTargetToTextureEx, pTexture, i, ( pSrc ) ? &m_queue.Copy(*pSrc) : NULL, ( pDst ) ? &m_queue.Copy(*pDst) : NULL );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::CopyTextureToRenderTargetEx( int i, ITexture *pTexture, Rect_t *pSrc, Rect_t *pDst )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::CopyTextureToRenderTargetEx, i, pTexture, CUtlEnvelope<Rect_t>(pSrc), CUtlEnvelope<Rect_t>(pDst) );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMatQueuedRenderContext::OnDrawMesh( IMesh *pMesh, int firstIndex, int numIndices )
{
	Assert( pMesh != m_pQueuedMesh );
	void (IMesh::*pfnDraw)( int, int) = &IMesh::Draw; // need assignment to disambiguate overloaded function
	m_queue.QueueCall( pMesh, pfnDraw, firstIndex, numIndices );
	return false;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMatQueuedRenderContext::OnDrawMesh( IMesh *pMesh, CPrimList *pLists, int nLists )
{
	CMatRenderData< CPrimList > rdPrimList( this, nLists, pLists );
	m_queue.QueueCall( this, &CMatQueuedRenderContext::DeferredDrawPrimList, pMesh, rdPrimList.Base(), nLists );
	return false;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMatQueuedRenderContext::OnDrawMeshModulated( IMesh *pMesh, const Vector4D& vecDiffuseModulation, int firstIndex, int numIndices )
{
	Assert( pMesh != m_pQueuedMesh );
	m_queue.QueueCall( pMesh, &IMesh::DrawModulated, vecDiffuseModulation, firstIndex, numIndices );
	return false;
}


void CMatQueuedRenderContext::DeferredDrawPrimList( IMesh *pMesh, CPrimList *pLists, int nLists )
{
	Assert( pMesh != m_pQueuedMesh );
	pMesh->Draw( pLists, nLists );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::DeferredSetFlexMesh( IMesh *pStaticMesh, int nVertexOffsetInBytes )
{
 	pStaticMesh->SetFlexMesh( m_pQueuedFlexMesh->MST_GetActualMesh(), m_pQueuedFlexMesh->MST_GetActualVertexOffsetInBytes() );
}

bool CMatQueuedRenderContext::OnSetFlexMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes )
{
	Assert( pMesh == m_pQueuedFlexMesh || !pMesh );
	if ( pMesh )
	{
		m_queue.QueueCall( this, &CMatQueuedRenderContext::DeferredSetFlexMesh, pStaticMesh, nVertexOffsetInBytes );
	}
	else
	{
		m_queue.QueueCall( pStaticMesh, &IMesh::SetFlexMesh, (IMesh *)NULL, 0 );
	}
	return false;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMatQueuedRenderContext::OnSetColorMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes )
{
	Assert( pStaticMesh != m_pQueuedMesh && pStaticMesh != m_pQueuedFlexMesh );
	m_queue.QueueCall( pStaticMesh, &IMesh::SetColorMesh, pMesh, nVertexOffsetInBytes );
	return false;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMatQueuedRenderContext::OnSetPrimitiveType( IMesh *pMesh, MaterialPrimitiveType_t type )
{
	Assert( pMesh != m_pQueuedMesh && pMesh != m_pQueuedFlexMesh );
	m_queue.QueueCall( pMesh, &IMesh::SetPrimitiveType, type );
	return false;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
inline void CMatQueuedRenderContext::QueueMatrixSync()
{
	void (IMatRenderContext::*pfnLoadMatrix)( const VMatrix & ) = &IMatRenderContext::LoadMatrix; // need assignment to disambiguate overloaded function
	m_queue.QueueCall( m_pHardwareContext, pfnLoadMatrix, RefToVal( AccessCurrentMatrix() ) );
}

void CMatQueuedRenderContext::MatrixMode( MaterialMatrixMode_t mode )
{
	CMatRenderContextBase::MatrixMode( mode );
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::MatrixMode, mode );
}

void CMatQueuedRenderContext::PushMatrix()
{
	CMatRenderContextBase::PushMatrix();
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::PushMatrix );
}

void CMatQueuedRenderContext::PopMatrix()
{
	CMatRenderContextBase::PopMatrix();
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::PopMatrix );
}

void CMatQueuedRenderContext::LoadMatrix( const VMatrix& matrix )
{
	CMatRenderContextBase::LoadMatrix( matrix );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::LoadMatrix( const matrix3x4_t& matrix )
{
	CMatRenderContextBase::LoadMatrix( matrix );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::MultMatrix( const VMatrix& matrix )
{
	CMatRenderContextBase::MultMatrix( matrix );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::MultMatrix( const matrix3x4_t& matrix )
{
	CMatRenderContextBase::MultMatrix( VMatrix( matrix ) );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::MultMatrixLocal( const VMatrix& matrix )
{
	CMatRenderContextBase::MultMatrixLocal( matrix );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::MultMatrixLocal( const matrix3x4_t& matrix )
{
	CMatRenderContextBase::MultMatrixLocal( VMatrix( matrix ) );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::LoadIdentity()
{
	CMatRenderContextBase::LoadIdentity();
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::LoadIdentity );
}

void CMatQueuedRenderContext::Ortho( double left, double top, double right, double bottom, double zNear, double zFar )
{
	CMatRenderContextBase::Ortho( left, top, right, bottom, zNear, zFar );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::PerspectiveX( double flFovX, double flAspect, double flZNear, double flZFar )
{
	CMatRenderContextBase::PerspectiveX( flFovX, flAspect, flZNear, flZFar );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::PerspectiveOffCenterX( double flFovX, double flAspect, double flZNear, double flZFar, double bottom, double top, double left, double right )
{
	CMatRenderContextBase::PerspectiveOffCenterX( flFovX, flAspect, flZNear, flZFar, bottom, top, left, right );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::PickMatrix( int x, int y, int nWidth, int nHeight )
{
	CMatRenderContextBase::PickMatrix( x, y, nWidth, nHeight );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::Rotate( float flAngle, float x, float y, float z )
{
	CMatRenderContextBase::Rotate( flAngle, x, y, z );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::Translate( float x, float y, float z )
{
	CMatRenderContextBase::Translate( x, y, z );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::Scale( float x, float y, float z )
{
	CMatRenderContextBase::Scale( x, y, z );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::BeginBatch( IMesh* pIndices ) 
{
	Assert( pIndices == (IMesh *)m_pQueuedMesh );
	m_pQueuedMesh->InvalidateAuxMeshSet();
	m_queue.QueueCall( this, &CMatQueuedRenderContext::DeferredBeginBatch );
}

void CMatQueuedRenderContext::BindBatch( IMesh* pVertices, IMaterial *pAutoBind ) 
{
	Assert( pVertices != (IMesh *)m_pQueuedMesh );
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::BindBatch, pVertices, pAutoBind );
}

void CMatQueuedRenderContext::DrawBatch(MaterialPrimitiveType_t primType, int firstIndex, int numIndices )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::DrawBatch, primType, firstIndex, numIndices );
}

void CMatQueuedRenderContext::EndBatch()
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::EndBatch );
}

void CMatQueuedRenderContext::DeferredBeginBatch( )
{
	m_pHardwareContext->BeginBatch( m_pQueuedMesh->MST_DetachActualMesh() );
}


//-----------------------------------------------------------------------------
// Memory allocation calls for queued mesh, et. al.
//-----------------------------------------------------------------------------
byte *CMatQueuedRenderContext::AllocVertices( int nVerts, int nVertexSize, bool *pExternalVB )
{
	MEM_ALLOC_CREDIT();
	size_t nSizeInBytes = nVerts * nVertexSize;
	byte *pMemory = ( byte * )Vertices().Alloc( nSizeInBytes, false );

#ifdef MS_NO_DYNAMIC_BUFFER_COPY
	if ( nSizeInBytes > DYNAMIC_VERTEX_BUFFER_BLOCK_SIZE )
	{
		Warning( "AllocVertices: Tried to allocate vertex buffer too large! (%d/%d)\n", nSizeInBytes, DYNAMIC_VERTEX_BUFFER_BLOCK_SIZE );
	}

	if ( pMemory )
	{
		*pExternalVB = true;
	}
	else
	{
		*pExternalVB = false;
		pMemory = ( byte * )m_Vertices.Alloc( nSizeInBytes, false );
	}
#else
	*pExternalVB = false;
#endif

	Assert( pMemory );
	return pMemory;
}

byte *CMatQueuedRenderContext::AllocIndices( int nIndices, int nIndexSize, bool *pExternalIB )
{
	MEM_ALLOC_CREDIT();
	size_t nSizeInBytes = nIndices * nIndexSize;
	byte *pMemory = ( byte * )Indices().Alloc( nSizeInBytes, false );

#ifdef MS_NO_DYNAMIC_BUFFER_COPY
	if ( nSizeInBytes > DYNAMIC_INDEX_BUFFER_BLOCK_SIZE )
	{
		Warning( "AllocIndices: Tried to allocate index buffer too large! (%d/%d)\n", nSizeInBytes, DYNAMIC_INDEX_BUFFER_BLOCK_SIZE );
	}

	if ( pMemory )
	{
		*pExternalIB = true;
	}
	else
	{
		*pExternalIB = false;
		pMemory = ( byte * )m_Indices.Alloc( nSizeInBytes, false );
	}
#else
	*pExternalIB = false;
#endif

	Assert( pMemory );
	return pMemory;
}

byte *CMatQueuedRenderContext::ReallocVertices( byte *pVerts, int nVertsOld, int nVertsNew, int nVertexSize, bool bExternalMemory )
{
	Assert( nVertsNew <= nVertsOld );

	if ( nVertsNew < nVertsOld )
	{
		unsigned nBytes = ( ( nVertsOld - nVertsNew ) * nVertexSize );
#ifdef MS_NO_DYNAMIC_BUFFER_COPY
		CMemoryStack &stack = bExternalMemory ? Vertices() : m_Vertices;
#else
		CMemoryStack &stack = Vertices();
#endif
		stack.FreeToAllocPoint( stack.GetCurrentAllocPoint() - nBytes, false ); // memstacks 128 bit aligned
	}
	return pVerts;
}

byte *CMatQueuedRenderContext::ReallocIndices( byte *pIndices, int nIndicesOld, int nIndicesNew, int nIndexSize, bool bExternalMemory )
{
	Assert( nIndicesNew <= nIndicesOld );
	if ( nIndicesNew < nIndicesOld )
	{
		unsigned nBytes = ( ( nIndicesOld - nIndicesNew ) * nIndexSize );
#ifdef MS_NO_DYNAMIC_BUFFER_COPY
		CMemoryStack &stack = bExternalMemory ? Indices() : m_Indices;
#else
		CMemoryStack &stack = Indices();
#endif
		stack.FreeToAllocPoint( stack.GetCurrentAllocPoint() - nBytes, false ); // memstacks 128 bit aligned
	}
	return pIndices;
}

void CMatQueuedRenderContext::FreeVertices( byte *pVerts, int nVerts, int nVertexSize )
{
	// free at end of call dispatch
}

void CMatQueuedRenderContext::FreeIndices( byte *pIndices, int nIndices, int nIndexSize )
{
	// free at end of call dispatch
}


//------------------------------------------------------------------------------
// Called from rendering thread, fixes up dynamic buffers
//------------------------------------------------------------------------------
void CMatQueuedRenderContext::DeferredDrawInstances( int nInstanceCount, const MeshInstanceData_t *pConstInstance )
{
	MeshInstanceData_t *pInstance = const_cast<MeshInstanceData_t*>( pConstInstance );

	// Adjust the instances pointing to the dynamic index buffer
	IIndexBuffer *pDynamicIndexBuffer = m_pQueuedIndexBuffer->RT_GetDynamicIndexBuffer();
	int nStartIndex = m_pQueuedIndexBuffer->RT_GetIndexStart();
	if ( ( nStartIndex < 0 ) || !pDynamicIndexBuffer )
		return;

	for ( int i = 0; i < nInstanceCount; ++i )
	{
		MeshInstanceData_t &instance = pInstance[i];

		// FIXME: Make dynamic vertex buffers work!
		Assert( !instance.m_pVertexBuffer->IsDynamic() );
		if ( !instance.m_pIndexBuffer->IsDynamic() )
			continue;
		
		instance.m_pIndexBuffer = pDynamicIndexBuffer;
		instance.m_nIndexOffset += nStartIndex;
	}

	m_pHardwareContext->DrawInstances( nInstanceCount, pConstInstance );
}


//------------------------------------------------------------------------------
// Draws instances with different meshes
//------------------------------------------------------------------------------
void CMatQueuedRenderContext::DrawInstances( int nInstanceCount, const MeshInstanceData_t *pInstance )
{
	CMatRenderData< MeshInstanceData_t > renderData( this );
	if ( !IsRenderData( pInstance ) )
	{
		renderData.Lock( nInstanceCount );
		memcpy( renderData.Base(), pInstance, nInstanceCount * sizeof(MeshInstanceData_t) );
		pInstance = renderData.Base();
	}

#ifdef _DEBUG
	for ( int i = 0; i < nInstanceCount; ++i )
	{
		Assert( !pInstance[i].m_pPoseToWorld || IsRenderData( pInstance[i].m_pPoseToWorld ) );
		Assert( !pInstance[i].m_pLightingState || IsRenderData( pInstance[i].m_pLightingState ) );
		Assert( !pInstance[i].m_pBoneRemap || IsRenderData( pInstance[i].m_pBoneRemap ) );
		Assert( !pInstance[i].m_pStencilState || IsRenderData( pInstance[i].m_pStencilState ) );
	}
#endif

	m_queue.QueueCall( this, &CMatQueuedRenderContext::DeferredDrawInstances, nInstanceCount, pInstance );
}


//------------------------------------------------------------------------------
// Color correction related methods
//------------------------------------------------------------------------------
ColorCorrectionHandle_t CMatQueuedRenderContext::AddLookup( const char *pName )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	ColorCorrectionHandle_t hCC = ColorCorrectionSystem()->AddLookup( pName );
	m_pMaterialSystem->Unlock( hLock );
	return hCC;
}

bool CMatQueuedRenderContext::RemoveLookup( ColorCorrectionHandle_t handle )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	bool bRemoved = ColorCorrectionSystem()->RemoveLookup( handle );
	m_pMaterialSystem->Unlock( hLock );
	return bRemoved;
}

ColorCorrectionHandle_t CMatQueuedRenderContext::FindLookup( const char *pName )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	ColorCorrectionHandle_t hCC = ColorCorrectionSystem()->FindLookup( pName );
	m_pMaterialSystem->Unlock( hLock );
	return hCC;
}


void CMatQueuedRenderContext::LockLookup( ColorCorrectionHandle_t handle )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	ColorCorrectionSystem()->LockLookup( handle );
	m_pMaterialSystem->Unlock( hLock );
}

void CMatQueuedRenderContext::LoadLookup( ColorCorrectionHandle_t handle, const char *pLookupName )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	ColorCorrectionSystem()->LoadLookup( handle, pLookupName );
	m_pMaterialSystem->Unlock( hLock );
}

void CMatQueuedRenderContext::UnlockLookup( ColorCorrectionHandle_t handle )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	ColorCorrectionSystem()->UnlockLookup( handle );
	m_pMaterialSystem->Unlock( hLock );
}

// NOTE: These are synchronous calls!  The rendering thread is stopped, the current queue is drained and the pixels are read
// NOTE: We should also have a queued read pixels in the API for doing mid frame reads (as opposed to screenshots)
void CMatQueuedRenderContext::ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	g_pShaderAPI->ReadPixels( x, y, width, height, data, dstFormat, pRenderTargetTexture );
	m_pMaterialSystem->Unlock( hLock );
}

void CMatQueuedRenderContext::ReadPixelsAsync( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture, CThreadEvent *pPixelsReadEvent )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::ReadPixelsAsync, x, y, width, height, data, dstFormat, pRenderTargetTexture, pPixelsReadEvent );
}

void CMatQueuedRenderContext::ReadPixelsAsyncGetResult( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, CThreadEvent *pGetResultEvent )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::ReadPixelsAsyncGetResult, x, y, width, height, data, dstFormat, pGetResultEvent );
}

void CMatQueuedRenderContext::ReadPixelsAndStretch( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *pBuffer, ImageFormat dstFormat, int nDstStride )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	g_pShaderAPI->ReadPixels( pSrcRect, pDstRect, pBuffer, dstFormat, nDstStride );
	m_pMaterialSystem->Unlock( hLock );
}

