//=========== Copyright © Valve Corporation, All rights reserved. ===========//
//
// Purpose: Mesh types for meshutils library
//
//===========================================================================//

#ifndef MESH_H
#define MESH_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"
#include "tier1/utllinkedlist.h"
#include "mathlib/vector.h"
#include "materialsystem/imaterial.h"
#include "rendersystem/irenderdevice.h"
#include "bitvec.h"

class CDmeModel;
class CDmeMesh;


#define VERTEX_ELEMENT_TANGENT_WITH_FLIP VERTEX_ELEMENT_TEXCOORD4D_7

// our usual distance epsilon is 1/32 of an inch
const float ONE_32ND_UNIT = 0.03125f;

// UVChart for unique parameterization
struct UVChart_t
{
	Vector4D						m_vPlane;
	CUtlVector<int>					m_TriangleList;
	Vector2D						m_vMinUV;
	Vector2D						m_vMaxUV;
	int								m_nVertexStart;
	int								m_nVertexCount;
};

// Atlas chart data for atlasing
// TODO: we should probably pull atlasing out into a more visible library
struct AtlasChart_t
{
	Vector2D						m_vMaxTextureSize;
	Vector2D						m_vAtlasMin;
	Vector2D						m_vAtlasMax;
	bool							m_bAtlased;
};

// the mesh library only supports a single stream, with multiple attributes. 
// NOTE: Each attribute must be an array of floats
class CMeshVertexAttribute
{
public:
	int						m_nOffsetFloats;	// Offset is in # of floats
	VertexElement_t			m_nType;

	inline bool IsJointWeight() const // { return !V_stricmp( m_name.Get(), "blendweight" ) || !V_stricmp( m_name.Get(), "blendweights" ); }
	{
		return m_nType == VERTEX_ELEMENT_BONEWEIGHTS1 || m_nType == VERTEX_ELEMENT_BONEWEIGHTS2 || m_nType == VERTEX_ELEMENT_BONEWEIGHTS3 || m_nType == VERTEX_ELEMENT_BONEWEIGHTS4;
	}
	inline bool IsJointIndices() const //{ return !V_stricmp( m_name.Get(), "blendindices" ); }
	{
		return m_nType == VERTEX_ELEMENT_BONEINDEX;
	}
	inline bool IsClothEnable()const //{ return !V_stricmp( m_name.Get(), "cloth_enable" ); }
	{
		return false; // needs to be string-based
	}
	inline bool IsPositionRemap()const //{ return !V_stricmp( m_name.Get(), "dme_position_map" ); }
	{
		return false; // needs to be string-based
	}
};

class CMesh
{
public:
	CMesh();
	~CMesh();

	// -----------------------------------------------------------------------------------------
	// Allocate vertex and index space for a mesh, this memory will automatically be freed in the destructor
	void AllocateMesh( int nVertexCount, int nIndexCount, int nVertexStride, CMeshVertexAttribute *pAttributes, int nAtrributeCount );
	void AllocateAndCopyMesh( int nInputVertexCount, const float *pInputVerts, int nInputIndexCount, const uint32 *pInputIndices, int nVertexStride, CMeshVertexAttribute *pAttributes, int nAtrributeCount );
	void FreeAllMemory();

	// -----------------------------------------------------------------------------------------
	// If you don't supply the attribute definition it assumes the vertices are just a position vector
	// NOTE: VertexStrideFloats is 3 for a normal vector of 3 floats
	// External meshes are allocated by external code and not freed in the destructor
	void InitExternalMesh( float *pVerts, int nVertexCount, uint32 *pIndices, int nIndexCount, int nVertexStrideFloats = 3, CMeshVertexAttribute *pAttributes = NULL, int nAtrributeCount = 0 );

	inline int IndexCount() const
	{
		return m_nIndexCount;
	}
	inline int VertexCount() const
	{
		return m_nVertexCount;
	}
	inline void SetVertexPosition( int nIndex, const Vector & v )
	{
		*( Vector * )GetVertex( nIndex ) = v ;
	}

	struct SkinningDataFields_t
	{
		SkinningDataFields_t ():m_nBoneWeights( -1 ),  m_nBoneIndices( -1 ){}
		bool HasSkinningData() const { return m_nBoneIndices >= 0; }
		int m_nBoneWeights,	m_nBoneIndices;
	};


	struct ClothDataFields_t
	{
		ClothDataFields_t() : m_nClothEnable( -1 ), m_nPositionRemap( -1 ) {}
		bool HasClothData() const { return m_nClothEnable >= 0 && m_nPositionRemap >= 0; }
		int m_nClothEnable, m_nPositionRemap;
	};

	// accessing a single attribute of a single vertex (attribute may be scalar or an array of T; scalar and array of size 1 are one and the same)
	template <typename T>
	class CSingleVertexFieldAccessor
	{
	public:
		CSingleVertexFieldAccessor( T*pBase, int nCount ) : m_pBase( pBase ), m_nCount( nCount ) {}
	protected:
		T *m_pBase;
		int m_nCount;
	public:
		T& operator [] ( int i ) { AssertDbg( i < m_nCount ); return m_pBase[ i ]; }
		const T& operator [] ( int i ) const { AssertDbg( i < m_nCount ); return m_pBase[ i ]; }
		T& operator *( ){ AssertDbg( m_nCount > 0 ); return *m_pBase; }
		const T& operator *( ) const { AssertDbg( m_nCount > 0 ); return *m_pBase; }
		T& Tail() { AssertDbg( m_nCount > 0 ); return m_pBase[ m_nCount - 1 ]; }
		const T& Tail() const { AssertDbg( m_nCount > 0 ); return m_pBase[ m_nCount - 1 ]; }

		int Count()const { return m_nCount; }
	};

	template <typename T>
	class CScalarAttrArray;

	// an accessor for a common attribute of all vertices  (e.g. blendweights, or position, or normal)
	template < typename T >
	class CAttrArray
	{
	protected:
		friend class CScalarAttrArray < T > ;


		float *m_pBase; // this points to the first vertex field (first vertex + field offset)
		int m_nStride; // this is the stride between fields
		int m_nElementCount;
		int m_nAttrCount;
	public:
		CAttrArray( float *pBase = NULL, int nStride = 0, int nVertexCount = 0, int nAttrCount = 0 )
			: m_pBase( pBase )
			, m_nStride( nStride )
			, m_nElementCount( nVertexCount )
			, m_nAttrCount( nAttrCount )
		{ }


		struct Range_t
		{
			T m_Min;
			T m_Max;
			Range_t();
			void Append( T val )
			{
				if ( val < m_Min )
					m_Min = val;
				if ( val > m_Max )
					m_Max = val;
			}
		};
		Range_t GetRange()const
		{
			Range_t range;

			for ( int v = 0; v < m_nElementCount; ++v )
			{
				const T *pValues = ( const T* )( m_pBase + m_nStride * v );
				for ( int na = 0; na < m_nAttrCount; ++na )
				{
					range.Append( pValues[ na ] );
				}
			}
			return range;
		}
	public:
		// converting to bool to use this in if() to check for NULL
		operator bool() const { return m_pBase != NULL; }
		CSingleVertexFieldAccessor< T > operator []( int nVertex ) { AssertDbg( nVertex < m_nElementCount ); return CSingleVertexFieldAccessor< T >( ( T* )( m_pBase + m_nStride * nVertex ), m_nAttrCount ); }
		CSingleVertexFieldAccessor< const T > operator []( int nVertex ) const { AssertDbg( nVertex < m_nElementCount ); return CSingleVertexFieldAccessor< const T >( ( const T* )( m_pBase + m_nStride * nVertex ), m_nAttrCount ); }

		int GetElementCount()const { return m_nElementCount; }
		int GetAttrCount()const { return m_nAttrCount; }
	};


	bool HasSkinningData() const;
	SkinningDataFields_t GetSkinningDataFields() const;
	ClothDataFields_t GetClothDataFields() const;
	float GetVertexJointSumWeight( const SkinningDataFields_t &skinData, int nVertex, const CVarBitVec &jointSet ); // get the weight of this vertex at this joint; 1.0 for vertices rigdly bound to the given bone, 0.0 for vertices not bound to this bone
	
	void AppendMesh( const CMesh &inputMesh );

	inline int TriangleCount() const { return m_nIndexCount / 3; }
	inline float *GetVertex(int nIndex) { return m_pVerts + (nIndex * m_nVertexStrideFloats); }
	inline const float *GetVertex(int nIndex) const { return m_pVerts + (nIndex * m_nVertexStrideFloats); }
	inline const Vector &GetVertexPosition(int nIndex) const { return *(Vector *)(m_pVerts + (nIndex * m_nVertexStrideFloats)); }

	inline size_t GetVertexSizeInBytes() const { return m_nVertexStrideFloats * sizeof(float); }
	inline size_t GetTotalVertexSizeInBytes() const { return GetVertexSizeInBytes() * m_nVertexCount; }
	inline size_t GetTotalIndexSizeInBytes() const { return sizeof(uint32) * m_nIndexCount; }

	Vector4D PlaneFromTriangle( int nTriangle ) const;
	bool CalculateBounds( Vector *pMinOut, Vector *pMaxOut, int nStartVertex = 0, int nVertexCount = 0 ) const;
	bool CalculateAdjacency( int *pAdjacencyOut, int nSizeAdjacencyOut ) const;
	bool CalculateIndicentFacesForVertices( CUtlLinkedList<int> *pFacesPerVertex, int nFacesPerVertexSize ) const;
	bool CalculateInputLayoutFromAttributes( RenderInputLayoutField_t *pOutFields, int *pInOutNumFields ) const;
	int FindFirstAttributeOffset( VertexElement_t nType ) const;
	void AddAttributes( CMeshVertexAttribute *pAttributes, int nAttributeCount );
	void CalculateTangents();
	bool CalculateTangentSpaceWorldLengthsPerFace( Vector2D *pLengthsOut, int nLengthsOut, float flMaxWorldPerUV = 256.0f );
	bool CalculateFaceCenters( Vector *pCentersOut, int nCentersOut );
	int GetAttrSizeFloats( int nAttribute );

	float					*m_pVerts;
	CMeshVertexAttribute	*m_pAttributes;
	uint32					*m_pIndices;
	int						m_nVertexCount;
	int						m_nVertexStrideFloats;	// Stride is in # of floats
	int						m_nAttributeCount;
	int						m_nIndexCount;
	CUtlString				m_materialName;

private:
	void RestrideVertexBuffer( int nNewStrideFloats );
	
	bool					m_bAllocatedMeshData;
};

struct GridVolume_t
{
	Vector m_vMinBounds;
	Vector m_vMaxBounds;
};

struct IndexRange_t
{
	int m_nStartIndex;
	int m_nIndexCount;
};

bool ClipMeshToHalfSpace( CMesh *pMeshBack, CMesh *pMeshFront, const CMesh &inputMesh, Vector4D &vClipPlane );
void DuplicateMesh( CMesh *pMeshOut, const CMesh &inputMesh );
bool RationalizeUVsInPlace( CMesh *pMesh );
bool RationalizeUVs( CMesh *pRationalMeshOut, const CMesh &inputMesh );
void DeIndexMesh( CMesh *pMeshOut, const CMesh &inputMesh );
bool ConcatMeshes( CMesh *pMeshOut, CMesh **ppMeshIn, int nInputMeshes,
				   CMeshVertexAttribute *pAttributeOverride = NULL, int nAttributeOverrideCount = 0, int nStrideOverride = 0 );
bool TessellateOnWrappedUV( CMesh *pMeshOut, const CMesh &inputMesh );
// returns the mesh containing the convex hull of the input mesh
void ConvexHull3D( CMesh *pOutMesh, const CMesh &inputMesh, float flCoplanarEpsilon = ONE_32ND_UNIT );
void FitOBBToMesh( matrix3x4_t *pCenter, Vector *pExtents, const CMesh &inputMesh, float flCoplanarEpsilon = ONE_32ND_UNIT );
// Builds a mesh of the hull defined by an array of planes (pointing out of the solid - plane eq is Ax + By + Cz = D). Optionally, outputs the index of the originating input plane for each triangle in the mesh.
void HullFromPlanes( CMesh *pOutMesh, CUtlVector<uint32> *pTrianglePlaneIndex, const float *pPlanes, int nPlaneCount, int nPlaneStrideFloats, float flCoplanarEpsilon = ONE_32ND_UNIT );
// Same as above but work on a temporary list of planes described as fltx4 (the buffer will be modified in place). No stride needed in this case, ABCD = XYZW
void HullFromPlanes_SIMD( CMesh *pOutMesh, CUtlVector<uint16> *pTrianglePlaneIndex, fltx4 *pPlanes, int nPlaneCount, float flCoplanarEpsilon = ONE_32ND_UNIT );

// TODO: Extreme welds can cause faces to flip/invert.  Should we add an option to detect and avoid this?
bool WeldVertices( CMesh *pMeshOut, const CMesh &inputMesh, float *pEpsilons, int nEpsilons );

// This removes any degenerate triangles and puts the vertices in access order
void CleanMesh( CMesh *pMeshOut, const CMesh &inputMesh );

// partitions the mesh into an array of meshes, each with <= nMaxVertex vertices
void SplitMesh( CUtlVector<CMesh> &list, const CMesh &input, int nMaxVertex );

// Create a unique parameterization of the mesh
bool CreateUniqueUVParameterization( CMesh *pMeshOut, const CMesh &inputMesh, float flFaceAngleThreshold, int nAtlasTextureSizeX, int nAtlasTextureSizeY, float flGutterSize );

// Pack a set of charts into an atlas
int PackChartsIntoAtlas( AtlasChart_t *pCharts, int nCharts, int nAtlasTextureSizeX, int nAtlasTextureSizeY, int nAtlasGrow );

// Creates a list of AABBs for a give volume given a target AABB size
void CreateGridCellsForVolume( CUtlVector< GridVolume_t > &outputVolumes, const Vector &vMinBounds, const Vector &vMaxBounds, const Vector &vGridSize );

//--------------------------------------------------------------------------------------
// For a list of AABBs, find all indices in the mesh that belong to each AABB.  Then
// coalesce those indices into a single buffer in order of the input AABBs and return a
// list of ranges of the indices in the output mesh that are needed in each input volume
//--------------------------------------------------------------------------------------
void CreatedGriddedIndexRangesFromMesh( CMesh *pOutputMesh, CUtlVector< IndexRange_t > &outputRanges, const CMesh &inputMesh, CUtlVector< GridVolume_t > &inputVolumes );

// Per-vertex ops
void CopyVertex( float *pOut, const float *pIn, int nFloats );
void SubtractVertex( float *pOutput, const float *pLeft, const float *pRight, int nFloats );
void AddVertex( float *pOutput, const float *pLeft, const float *pRight, int nFloats );
void MultiplyVertex( float *pOutput, const float *pLeft, const float* pRight, float nFloats );
void AddVertexInPlace( float *pLeft, const float *pRight, int nFloats );
void MultiplyVertexInPlace( float *pLeft, float flRight, int nFloats );
void LerpVertex( float *pOutput, const float *pLeft, const float *pRight, float flLerp, int nFloats );
void BaryCentricVertices( float *pOutput, float *p0, float *p1, float *p2, float flU, float flV, float flW, int nFloats );


// simple hashing function for edges
inline uint32 VertHashKey( int nV0, int nV1 )
{
	uint32 nHash = ((uint32(nV0) >> 16) | (uint32(nV0)<<16)) ^ uint32(nV1);
	nHash = ieqsel( nHash, uint32(~0), uint32( 0 ), nHash ); // don't ever return ~0
	return nHash;
}

	
#endif // MESH_H
