//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef MESHBASE_H
#define MESHBASE_H

#ifdef _WIN32
#pragma once
#endif


#include "materialsystem/imesh.h"
#include "materialsystem/imaterial.h"


struct VertexStreamSpec_t;


//-----------------------------------------------------------------------------
// Base vertex buffer
//-----------------------------------------------------------------------------
abstract_class CVertexBufferBase : public IVertexBuffer
{
	// Methods of IVertexBuffer
public:
	virtual void Spew( int nVertexCount, const VertexDesc_t &desc );
	virtual void ValidateData( int nVertexCount, const VertexDesc_t& desc );

public:
	// constructor, destructor
	CVertexBufferBase( const char *pBudgetGroupName );
	virtual ~CVertexBufferBase();

	// Displays the vertex format
	static void PrintVertexFormat( VertexFormat_t vertexFormat );

	// Used to construct vertex data
	static void ComputeVertexDescription( unsigned char *pBuffer, VertexFormat_t vertexFormat, VertexDesc_t &desc );

	// Returns the vertex format size 
	static int VertexFormatSize( VertexFormat_t vertexFormat );

protected:
	const char *m_pBudgetGroupName;
};


//-----------------------------------------------------------------------------
// Base index buffer
//-----------------------------------------------------------------------------
abstract_class CIndexBufferBase : public IIndexBuffer
{
	// Methods of IIndexBuffer
public:
	virtual void Spew( int nIndexCount, const IndexDesc_t &desc );
	virtual void ValidateData( int nIndexCount, const IndexDesc_t& desc );
	virtual IMesh* GetMesh() { return NULL; }

	// Other public methods
public:
	// constructor, destructor
	CIndexBufferBase( const char *pBudgetGroupName );
	virtual ~CIndexBufferBase() {}

protected:
	const char *m_pBudgetGroupName;
};


//-----------------------------------------------------------------------------
// Base mesh
//-----------------------------------------------------------------------------
class CMeshBase : public IMesh
{
	// Methods of IMesh
public:

	// Other public methods that need to be overridden
public:
	// Begins a pass
	virtual void BeginPass( ) = 0;

	// Draws a single pass of the mesh
	virtual void RenderPass( const unsigned char *pInstanceCommandBuffer ) = 0;

	// DrawPrims
	virtual void DrawPrims( const unsigned char *pInstanceCommandBuffer ) = 0;

	// Does it have a color mesh?
	virtual bool HasColorMesh() const = 0;

	// Does it have a flex mesh?
	virtual bool HasFlexMesh() const = 0;

	// Is it using a vertex ID?
	virtual bool IsUsingVertexID() const = 0;

	// Is it using tessellation for higher-order surfaces?
#if ENABLE_TESSELLATION
	virtual TessellationMode_t GetTessellationType() const = 0;
#else
	TessellationMode_t GetTessellationType() const { return TESSELLATION_MODE_DISABLED; }
#endif

	// Are vertex data streams specified in a custom manner?
	virtual VertexStreamSpec_t *GetVertexStreamSpec() const = 0;

	virtual IMesh *GetMesh() { return this; }

	virtual void * AccessRawHardwareDataStream( uint8 nRawStreamIndex, uint32 numBytes, uint32 uiFlags, void *pvContext ) { return NULL; }

	virtual ICachedPerFrameMeshData *GetCachedPerFrameMeshData() { return NULL; }
	virtual void ReconstructFromCachedPerFrameMeshData( ICachedPerFrameMeshData *pData ) {}
public:
	// constructor, destructor
	CMeshBase();
	virtual ~CMeshBase();

};

//-----------------------------------------------------------------------------
// Utility method for VertexDesc_t (don't want to expose it in public, in imesh.h)
// We've split this into two versions, one for computing size-only, and the standard
// version which computes field offsets and sizes.
// The code would be cleaner if we split it into two functions, but for now we'll keep
// all of the if( !bSizeOnly ) for easier maintenance if we need to change any of the fields.
//-----------------------------------------------------------------------------
template< bool bSizeOnly >
inline int ComputeVertexDesc( unsigned char * pBuffer, VertexFormat_t vertexFormat, VertexDesc_t & desc )
{
	int i;
	int *pVertexSizesToSet[64];
	int nVertexSizesToSet = 0;
	static ALIGN32 ModelVertexDX8_t temp[4];
	float *dummyData = (float*)&temp; // should be larger than any CMeshBuilder command can set.

	// Determine which vertex compression type this format specifies (affects element sizes/decls):
	VertexCompressionType_t compression = CompressionType( vertexFormat );
	int nNumBoneWeights = NumBoneWeights( vertexFormat );

	if ( !bSizeOnly )
	{
		desc.m_CompressionType = compression;
		desc.m_NumBoneWeights = nNumBoneWeights;
	}

	// We use fvf instead of flags here because we may pad out the fvf
	// vertex structure to optimize performance
	int offset = 0;
	// NOTE: At the moment, we assume that if you specify wrinkle, you also specify position
	Assert( ( ( vertexFormat & VERTEX_WRINKLE ) == 0 ) || ( ( vertexFormat & VERTEX_POSITION ) != 0 ) );
	if ( vertexFormat & VERTEX_POSITION )
	{
		if ( !bSizeOnly )
		{
			// UNDONE: compress position+wrinkle to SHORT4N, and roll the scale into the transform matrices
			desc.m_pPosition = reinterpret_cast<float*>(pBuffer);
			pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_Position;
		}

		VertexElement_t posElement = (vertexFormat & VERTEX_FORMAT_PAD_POS_NORM ) != 0 ? VERTEX_ELEMENT_POSITION4D : VERTEX_ELEMENT_POSITION;
		offset += GetVertexElementSize( posElement, compression );

		if ( vertexFormat & VERTEX_WRINKLE )
		{
			if ( !bSizeOnly )
			{
				desc.m_pWrinkle = reinterpret_cast<float*>( pBuffer + offset );
				pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_Wrinkle;
			}

			offset += GetVertexElementSize( VERTEX_ELEMENT_WRINKLE, compression );
		}
		else if ( !bSizeOnly )
		{
			desc.m_pWrinkle = dummyData;
			desc.m_VertexSize_Wrinkle = 0;
		}
	}
	else if ( !bSizeOnly )
	{
		desc.m_pPosition = dummyData;
		desc.m_VertexSize_Position = 0;
		desc.m_pWrinkle = dummyData;
		desc.m_VertexSize_Wrinkle = 0;
	}

	// Bone weights/matrix indices
	Assert( ( nNumBoneWeights == 2 ) || ( nNumBoneWeights == 0 ) );

	// We assume that if you have any indices/weights, you have exactly two of them
	Assert( ( ( nNumBoneWeights == 2 ) && ( ( vertexFormat & VERTEX_BONE_INDEX ) != 0 ) ) ||
		( ( nNumBoneWeights == 0 ) && ( ( vertexFormat & VERTEX_BONE_INDEX ) == 0 ) ) );

	if ( ( vertexFormat & VERTEX_BONE_INDEX ) != 0 )
	{
		if ( nNumBoneWeights > 0 )
		{
			// Always exactly two weights
			Assert( nNumBoneWeights == 2 );
			if ( !bSizeOnly )
			{
				desc.m_pBoneWeight = reinterpret_cast<float*>(pBuffer + offset);
				pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_BoneWeight;
			}

			offset += GetVertexElementSize( VERTEX_ELEMENT_BONEWEIGHTS2, compression );
		}
		else if ( !bSizeOnly )
		{
			desc.m_pBoneWeight = dummyData;
			desc.m_VertexSize_BoneWeight = 0;
		}

		if ( !bSizeOnly )
		{
			desc.m_pBoneMatrixIndex = pBuffer + offset;
			pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_BoneMatrixIndex;
		}

		offset += GetVertexElementSize( VERTEX_ELEMENT_BONEINDEX, compression );
	}
	else if ( !bSizeOnly )
	{
		desc.m_pBoneWeight = dummyData;
		desc.m_VertexSize_BoneWeight = 0;

		desc.m_pBoneMatrixIndex = (unsigned char*)dummyData;
		desc.m_VertexSize_BoneMatrixIndex = 0;
	}

	if ( vertexFormat & VERTEX_NORMAL )
	{
		if ( !bSizeOnly )
		{
			desc.m_pNormal = reinterpret_cast<float*>(pBuffer + offset);
			pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_Normal;
		}

		// See PackNormal_[SHORT2|UBYTE4|HEND3N] in mathlib.h for the compression algorithm

		VertexElement_t normalElement = (vertexFormat & VERTEX_FORMAT_PAD_POS_NORM ) != 0 ? VERTEX_ELEMENT_NORMAL4D : VERTEX_ELEMENT_NORMAL;
		offset += GetVertexElementSize( normalElement, compression );
	}
	else if ( !bSizeOnly )
	{
		desc.m_pNormal = dummyData;
		desc.m_VertexSize_Normal = 0;
	}

	if ( vertexFormat & VERTEX_COLOR )
	{
		if ( !bSizeOnly )
		{
			desc.m_pColor = pBuffer + offset;
			pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_Color;
		}

		offset += GetVertexElementSize( VERTEX_ELEMENT_COLOR, compression );
	}
	else if ( !bSizeOnly )
	{
		desc.m_pColor = (unsigned char*)dummyData;
		desc.m_VertexSize_Color = 0;
	}

	if ( vertexFormat & VERTEX_SPECULAR )
	{
		if ( !bSizeOnly )
		{
			desc.m_pSpecular = pBuffer + offset;
			pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_Specular;
		}

		offset += GetVertexElementSize( VERTEX_ELEMENT_SPECULAR, compression );
	}
	else if ( !bSizeOnly )
	{
		desc.m_pSpecular = (unsigned char*)dummyData;
		desc.m_VertexSize_Specular = 0;
	}

	// Set up texture coordinates
	static const VertexElement_t texCoordElements[4] = { VERTEX_ELEMENT_TEXCOORD1D_0, VERTEX_ELEMENT_TEXCOORD2D_0, VERTEX_ELEMENT_TEXCOORD3D_0, VERTEX_ELEMENT_TEXCOORD4D_0 };
	for ( i = 0; i < VERTEX_MAX_TEXTURE_COORDINATES; ++i )
	{
		// FIXME: compress texcoords to SHORT2N/SHORT4N, with a scale rolled into the texture transform
		int nSize = TexCoordSize( i, vertexFormat );
		if ( nSize != 0 )
		{
			if ( !bSizeOnly )
			{
				desc.m_pTexCoord[i] = reinterpret_cast<float*>(pBuffer + offset);
				pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_TexCoord[i];
			}

			VertexElement_t texCoordElement = (VertexElement_t)( texCoordElements[ nSize - 1 ] + i );
			offset += GetVertexElementSize( texCoordElement, compression );
		}
		else if ( !bSizeOnly )
		{
			desc.m_pTexCoord[i] = dummyData;
			desc.m_VertexSize_TexCoord[i] = 0;
		}
	}

	// Binormal + tangent...
	// Note we have to put these at the end so the vertex is FVF + stuff at end
	if ( vertexFormat & VERTEX_TANGENT_S )
	{
		// UNDONE: use normal compression here (use mem_dumpvballocs to see if this uses much memory)
		if ( !bSizeOnly )
		{
			desc.m_pTangentS = reinterpret_cast<float*>(pBuffer + offset);
			pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_TangentS;
		}
		offset += GetVertexElementSize( VERTEX_ELEMENT_TANGENT_S, compression );

	}
	else if ( !bSizeOnly )
	{
		desc.m_pTangentS = dummyData;
		desc.m_VertexSize_TangentS = 0;
	}

	if ( vertexFormat & VERTEX_TANGENT_T )
	{
		// UNDONE: use normal compression here (use mem_dumpvballocs to see if this uses much memory)
		if ( !bSizeOnly )
		{
			desc.m_pTangentT = reinterpret_cast<float*>(pBuffer + offset);
			pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_TangentT;
		}
		offset += GetVertexElementSize( VERTEX_ELEMENT_TANGENT_T, compression );
	}
	else if ( !bSizeOnly )
	{
		desc.m_pTangentT = dummyData;
		desc.m_VertexSize_TangentT = 0;
	}

	// User data..
	int userDataSize = UserDataSize( vertexFormat );
	if ( userDataSize > 0 )
	{
		if ( !bSizeOnly )
		{
			desc.m_pUserData = reinterpret_cast<float*>(pBuffer + offset);
			pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_UserData;
		}

		VertexElement_t userDataElement = (VertexElement_t)( VERTEX_ELEMENT_USERDATA1 + ( userDataSize - 1 ) );
		// See PackNormal_[SHORT2|UBYTE4|HEND3N] in mathlib.h for the compression algorithm
		offset += GetVertexElementSize( userDataElement, compression );
	}
	else if ( !bSizeOnly )
	{
		desc.m_pUserData = dummyData;
		desc.m_VertexSize_UserData = 0;
	}

	// We always use vertex sizes which are half-cache aligned (16 bytes)
	// x360 compressed vertexes are not compatible with forced alignments
	bool bCacheAlign = ( vertexFormat & VERTEX_FORMAT_USE_EXACT_FORMAT ) == 0;
	if ( bCacheAlign && ( offset > 16 ) && IsPC() )
	{
		offset = (offset + 0xF) & (~0xF);
	}
	desc.m_ActualVertexSize = offset;

	if ( !bSizeOnly )
	{
		// Now set the m_VertexSize for all the members that were actually valid.
		Assert( nVertexSizesToSet < sizeof(pVertexSizesToSet)/sizeof(pVertexSizesToSet[0]) );
		for ( int iElement=0; iElement < nVertexSizesToSet; iElement++ )
		{
			*pVertexSizesToSet[iElement] = offset;
		}
	}

	return offset;
}

#endif // MESHBASE_H
