//=========== Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Mesh class operations.
//
//===========================================================================//

#include "mesh.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlhash.h"

// simplest mesh type - array of vec3
static CMeshVertexAttribute g_PositionAttribute = {0,VERTEX_ELEMENT_POSITION};

// CMesh - utility mesh class implementation
CMesh::CMesh() : 
	m_pVerts(NULL), m_pAttributes(NULL), m_pIndices(NULL), m_nVertexCount(0), m_nVertexStrideFloats(0),
	m_nAttributeCount(0), m_nIndexCount(0), m_bAllocatedMeshData(false)
{
}

CMesh::~CMesh()
{
	FreeAllMemory();
}


// free anything we allocated
void CMesh::FreeAllMemory()
{
	if ( m_bAllocatedMeshData )
	{
		delete[] m_pVerts;
		delete[] m_pAttributes;
		delete[] m_pIndices;
	}
	m_pVerts = NULL;
	m_pAttributes = NULL;
	m_pIndices = NULL;
	m_bAllocatedMeshData = false;
	m_nAttributeCount = 0;
	m_nVertexStrideFloats = 0;
	m_nVertexCount = 0;
	m_nIndexCount = 0;
}

void CMesh::AllocateMesh( int nVertexCount, int nIndexCount, int nVertexStride, CMeshVertexAttribute *pAttributes, int nAtrributeCount )
{
	FreeAllMemory();
	if ( !pAttributes )
	{
		pAttributes = &g_PositionAttribute;
		nAtrributeCount = 1;
	}

	m_nVertexCount = nVertexCount;
	m_nVertexStrideFloats = nVertexStride;
	m_nIndexCount = nIndexCount;
	m_nAttributeCount = nAtrributeCount;

	// allocate the mesh data, mark as allocated so it will be freed on destruct
	m_bAllocatedMeshData = true;
	m_pVerts = new float[nVertexStride * nVertexCount];
	m_pIndices = new uint32[nIndexCount];
	m_pAttributes = new CMeshVertexAttribute[nAtrributeCount];

	for ( int i = 0; i < nAtrributeCount; i++ )
	{
		m_pAttributes[i] = pAttributes[i];
	}
}

void CMesh::AllocateAndCopyMesh( int nInputVertexCount, const float *pInputVerts, int nInputIndexCount, const uint32 *pInputIndices, int nVertexStride, CMeshVertexAttribute *pAttributes, int nAtrributeCount )
{
	AllocateMesh( nInputVertexCount, nInputIndexCount, nVertexStride, pAttributes, nAtrributeCount );
	V_memcpy( m_pVerts, pInputVerts, GetTotalVertexSizeInBytes() );
	V_memcpy( m_pIndices, pInputIndices, GetTotalIndexSizeInBytes() );
}


void CMesh::InitExternalMesh( float *pVerts, int nVertexCount, uint32 *pIndices, int nIndexCount, int nVertexStride, CMeshVertexAttribute *pAttributes, int nAtrributeCount )
{
	if ( !pAttributes )
	{
		pAttributes = &g_PositionAttribute;
		nAtrributeCount = 1;
	}
	FreeAllMemory();
	m_bAllocatedMeshData = false;
	m_nVertexCount = nVertexCount;
	m_nVertexStrideFloats = nVertexStride;
	m_nIndexCount = nIndexCount;
	m_nAttributeCount = nAtrributeCount;
	m_pVerts = pVerts;
	m_pIndices = pIndices;
	m_pAttributes = pAttributes;
}

void CMesh::AppendMesh( const CMesh &inputMesh )
{
	Assert( inputMesh.m_nAttributeCount == m_nAttributeCount );
	Assert( inputMesh.m_nVertexStrideFloats == m_nVertexStrideFloats );

	// Find total sizes
	int nTotalIndices = m_nIndexCount + inputMesh.m_nIndexCount;
	int nTotalVertices = m_nVertexCount + inputMesh.m_nVertexCount;

	float *pNewVB = new float[ nTotalVertices * m_nVertexStrideFloats ];
	uint32 *pNewIB = new uint32[ nTotalIndices ];

	Q_memcpy( pNewVB, m_pVerts, m_nVertexCount * m_nVertexStrideFloats * sizeof( float ) );
	Q_memcpy( pNewIB, m_pIndices, m_nIndexCount * sizeof( uint32 ) );

	int nCurrentIndex = m_nIndexCount;
	
	// copy vertices over
	Q_memcpy( pNewVB + m_nVertexCount * m_nVertexStrideFloats, 
			  inputMesh.m_pVerts, 
			  inputMesh.m_nVertexCount * m_nVertexStrideFloats * sizeof( float ) );

	for ( int i=0; i<inputMesh.m_nIndexCount; ++i )
	{
		pNewIB[ nCurrentIndex ] = inputMesh.m_pIndices[ i ] + m_nVertexCount;
		nCurrentIndex ++;
	}

	delete []m_pVerts;
	delete []m_pIndices;
	m_pVerts = pNewVB;
	m_pIndices = pNewIB;

	m_nVertexCount = nTotalVertices;
	m_nIndexCount = nTotalIndices;
}

bool CMesh::CalculateBounds( Vector *pMinOut, Vector *pMaxOut, int nStartVertex, int nVertexCount ) const
{
	Assert( pMinOut && pMaxOut );

	Vector vMin( FLT_MAX, FLT_MAX, FLT_MAX );
	Vector vMax( -FLT_MAX, -FLT_MAX, -FLT_MAX );

	int nPosOffset = FindFirstAttributeOffset( VERTEX_ELEMENT_POSITION );
	if ( nPosOffset < 0 )
		return false;

	float *pPositions = ( m_pVerts + nStartVertex * m_nVertexStrideFloats + nPosOffset );

	if ( nVertexCount == 0 )
		nVertexCount = m_nVertexCount;

	for ( int v=0; v<nVertexCount; ++v )
	{
		vMin.x = MIN( pPositions[0], vMin.x );
		vMin.y = MIN( pPositions[1], vMin.y );
		vMin.z = MIN( pPositions[2], vMin.z );

		vMax.x = MAX( pPositions[0], vMax.x );
		vMax.y = MAX( pPositions[1], vMax.y );
		vMax.z = MAX( pPositions[2], vMax.z );

		pPositions += m_nVertexStrideFloats;
	}

	*pMinOut = vMin;
	*pMaxOut = vMax;
	return true;
}

struct EdgeHash_t
{
	inline bool operator==( const EdgeHash_t& src ) const { return src.m_nV0 == m_nV0 && src.m_nV1 == m_nV1; }

	int m_nV0;
	int m_nV1;
	int m_nTri0;
	int m_nTri1;
};

// Fills in an adjacency array with the 3 edge adjacent triangles for each triangle in the array.
// The input pAdjacencyOut must be at least 3 * nFaces => ( 3 * m_nIndexCount / 3 ) => m_nIndexCount in size.
// Any boundary edges are given an edge adjacent triangle index of -1.
bool CMesh::CalculateAdjacency( int *pAdjacencyOut, int nSizeAdjacencyOut ) const
{
	Assert( pAdjacencyOut );
	if ( nSizeAdjacencyOut != m_nIndexCount )
		return false;

	CUtlScalarHash<EdgeHash_t> edgeHash;
	edgeHash.Init( m_nIndexCount * 2 );

	int nFaces = m_nIndexCount / 3;
	int nIndex = 0;
	for ( int f=0; f<nFaces; ++f )
	{
		for ( int i=0; i<3; ++i )
		{
			int nV0 = m_pIndices[ nIndex + ( i ) ];
			int nV1 = m_pIndices[ nIndex + ( i + 1 ) % 3 ];

			if ( nV1 < nV0 )
			{
				Swap( nV1, nV0 );
			}

			EdgeHash_t tmp;
			tmp.m_nV0 = nV0;
			tmp.m_nV1 = nV1;
			tmp.m_nTri0 = f;
			tmp.m_nTri1 = -1;

			uint nHashKey = VertHashKey( nV0, nV1 );
			UtlHashFastHandle_t edgeHashIndex = edgeHash.Find( nHashKey, tmp );
			if ( edgeHash.InvalidHandle() == edgeHashIndex )
			{
				edgeHash.Insert( nHashKey, tmp );
			}
			else
			{
				EdgeHash_t &edge = edgeHash.Element( edgeHashIndex );
				edge.m_nTri1 = f;
			}
		}

		nIndex += 3;
	}

	// Now that we have an edge datastructure, fill out edge-adjacency
	nIndex = 0;
	for ( int f=0; f<nFaces; ++f )
	{
		for ( int i=0; i<3; ++i )
		{
			int nV0 = m_pIndices[ nIndex + ( i ) ];
			int nV1 = m_pIndices[ nIndex + ( i + 1 ) % 3 ];

			if ( nV1 < nV0 )
			{
				Swap( nV1, nV0 );
			}

			EdgeHash_t tmp;
			tmp.m_nV0 = nV0;
			tmp.m_nV1 = nV1;
			tmp.m_nTri0 = -1;
			tmp.m_nTri1 = -1;

			uint nHashKey = VertHashKey( nV0, nV1 );
			UtlHashFastHandle_t edgeHashIndex = edgeHash.Find( nHashKey, tmp );
			Assert( edgeHash.InvalidHandle() != edgeHashIndex );
		
			EdgeHash_t &edge = edgeHash.Element( edgeHashIndex );
			if ( edge.m_nTri0 == f )
			{
				pAdjacencyOut[ nIndex + i ] = edge.m_nTri1;
			}
			else
			{
				pAdjacencyOut[ nIndex + i ] = edge.m_nTri0;
			}
		}

		nIndex += 3;
	}

	return true;
}

// Fills in a list of all faces that contain that particular vertex.  The input pFacesPerVertex, must
// contain at least m_nVertexCount elements of type CUtlLinkedList<int>.
bool CMesh::CalculateIndicentFacesForVertices( CUtlLinkedList<int> *pFacesPerVertex, int nFacesPerVertexSize ) const
{
	Assert( pFacesPerVertex );
	if ( nFacesPerVertexSize != m_nVertexCount )
		return false;

	int nFaces = m_nIndexCount / 3;
	int nIndex = 0;
	for ( int f=0; f<nFaces; ++f )
	{
		int v0 = m_pIndices[ nIndex ]; nIndex++;
		int v1 = m_pIndices[ nIndex ]; nIndex++;
		int v2 = m_pIndices[ nIndex ]; nIndex++;

		pFacesPerVertex[ v0 ].AddToTail( f );
		pFacesPerVertex[ v1 ].AddToTail( f );
		pFacesPerVertex[ v2 ].AddToTail( f );
	}

	return true;
}

struct InputDataForVertexElement_t
{
	char *m_pSemanticName;
	int m_nSemanticIndex;
};

InputDataForVertexElement_t g_pElementData[] = 
{
	{ "POSITION",		0 },	// VERTEX_ELEMENT_POSITION		= 0,
	{ "POSITION",		0 },	// VERTEX_ELEMENT_POSITION4D	= 1,
	{ "NORMAL",			0 },	// VERTEX_ELEMENT_NORMAL		= 2,
	{ "NORMAL",			0 },    // VERTEX_ELEMENT_NORMAL4D		= 3,
	{ "COLOR",			0 },    // VERTEX_ELEMENT_COLOR		= 4,
	{ "SPECULAR",		0 },    // VERTEX_ELEMENT_SPECULAR		= 5,
	{ "TANGENT",		0 },    // VERTEX_ELEMENT_TANGENT_S	= 6,
	{ "BINORMAL",		0 },    // VERTEX_ELEMENT_TANGENT_T	= 7,
	{ "WRINKLE",		0 },    // VERTEX_ELEMENT_WRINKLE		= 8,
	{ "BLENDINDICES",	0 },    // VERTEX_ELEMENT_BONEINDEX	= 9,
	{ "BLENDWEIGHT",	0 },    // VERTEX_ELEMENT_BONEWEIGHTS1	= 10,
	{ "BLENDWEIGHT",	1 },    // VERTEX_ELEMENT_BONEWEIGHTS2	= 11,
	{ "BLENDWEIGHT",	2 },    // VERTEX_ELEMENT_BONEWEIGHTS3	= 12,
	{ "BLENDWEIGHT",	3 },    // VERTEX_ELEMENT_BONEWEIGHTS4	= 13,
	{ "TEXCOORD",		8 },    // VERTEX_ELEMENT_USERDATA1	= 14,
	{ "TEXCOORD",		9 },    // VERTEX_ELEMENT_USERDATA2	= 15,
	{ "TEXCOORD",		10 },   // VERTEX_ELEMENT_USERDATA3	= 16,
	{ "TEXCOORD",		11 },   // VERTEX_ELEMENT_USERDATA4	= 17,
	{ "TEXCOORD",		0 },    // VERTEX_ELEMENT_TEXCOORD1D_0	= 18,
	{ "TEXCOORD",		1 },    // VERTEX_ELEMENT_TEXCOORD1D_1	= 19,
	{ "TEXCOORD",		2 },    // VERTEX_ELEMENT_TEXCOORD1D_2	= 20,
	{ "TEXCOORD",		3 },    // VERTEX_ELEMENT_TEXCOORD1D_3	= 21,
	{ "TEXCOORD",		4 },    // VERTEX_ELEMENT_TEXCOORD1D_4	= 22,
	{ "TEXCOORD",		5 },    // VERTEX_ELEMENT_TEXCOORD1D_5	= 23,
	{ "TEXCOORD",		6 },    // VERTEX_ELEMENT_TEXCOORD1D_6	= 24,
	{ "TEXCOORD",		7 },    // VERTEX_ELEMENT_TEXCOORD1D_7	= 25,
	{ "TEXCOORD",		0 },    // VERTEX_ELEMENT_TEXCOORD2D_0	= 26,
	{ "TEXCOORD",		1 },    // VERTEX_ELEMENT_TEXCOORD2D_1	= 27,
	{ "TEXCOORD",		2 },    // VERTEX_ELEMENT_TEXCOORD2D_2	= 28,
	{ "TEXCOORD",		3 },    // VERTEX_ELEMENT_TEXCOORD2D_3	= 29,
	{ "TEXCOORD",		4 },    // VERTEX_ELEMENT_TEXCOORD2D_4	= 30,
	{ "TEXCOORD",		5 },    // VERTEX_ELEMENT_TEXCOORD2D_5	= 31,
	{ "TEXCOORD",		6 },    // VERTEX_ELEMENT_TEXCOORD2D_6	= 32,
	{ "TEXCOORD",		7 },    // VERTEX_ELEMENT_TEXCOORD2D_7	= 33,
	{ "TEXCOORD",		0 },    // VERTEX_ELEMENT_TEXCOORD3D_0	= 34,
	{ "TEXCOORD",		1 },    // VERTEX_ELEMENT_TEXCOORD3D_1	= 35,
	{ "TEXCOORD",		2 },    // VERTEX_ELEMENT_TEXCOORD3D_2	= 36,
	{ "TEXCOORD",		3 },    // VERTEX_ELEMENT_TEXCOORD3D_3	= 37,
	{ "TEXCOORD",		4 },    // VERTEX_ELEMENT_TEXCOORD3D_4	= 38,
	{ "TEXCOORD",		5 },    // VERTEX_ELEMENT_TEXCOORD3D_5	= 39,
	{ "TEXCOORD",		6 },    // VERTEX_ELEMENT_TEXCOORD3D_6	= 40,
	{ "TEXCOORD",		7 },    // VERTEX_ELEMENT_TEXCOORD3D_7	= 41,
	{ "TEXCOORD",		0 },    // VERTEX_ELEMENT_TEXCOORD4D_0	= 42,
	{ "TEXCOORD",		1 },    // VERTEX_ELEMENT_TEXCOORD4D_1	= 43,
	{ "TEXCOORD",		2 },    // VERTEX_ELEMENT_TEXCOORD4D_2	= 44,
	{ "TEXCOORD",		3 },    // VERTEX_ELEMENT_TEXCOORD4D_3	= 45,
	{ "TEXCOORD",		4 },    // VERTEX_ELEMENT_TEXCOORD4D_4	= 46,
	{ "TEXCOORD",		5 },    // VERTEX_ELEMENT_TEXCOORD4D_5	= 47,
	{ "TEXCOORD",		6 },    // VERTEX_ELEMENT_TEXCOORD4D_6	= 48,
	{ "TEXCOORD",		7 },    // VERTEX_ELEMENT_TEXCOORD4D_7	= 49,
};

ColorFormat_t GetColorFormatForVertexElement( VertexElement_t element )
{
	int nBytes = GetVertexElementSize( element, VERTEX_COMPRESSION_NONE );
	if ( nBytes % sizeof( float ) != 0 )
	{
		return COLOR_FORMAT_UNKNOWN;
	}

	int nFloats = nBytes / sizeof( float );
	switch ( nFloats )
	{
	case 1:
		return COLOR_FORMAT_R32_FLOAT;
	case 2:
		return COLOR_FORMAT_R32G32_FLOAT;
	case 3:
		return COLOR_FORMAT_R32G32B32_FLOAT;
	case 4:
		return COLOR_FORMAT_R32G32B32A32_FLOAT;
	}

	return COLOR_FORMAT_UNKNOWN;
}

bool CMesh::CalculateInputLayoutFromAttributes( RenderInputLayoutField_t *pOutFields, int *pInOutNumFields ) const
{
	if ( *pInOutNumFields < m_nAttributeCount )
		return false;

	for ( int a=0; a<m_nAttributeCount; ++a )
	{
		InputDataForVertexElement_t elementData = g_pElementData[ m_pAttributes[ a ].m_nType ];

		pOutFields[ a ].m_Format = GetColorFormatForVertexElement( m_pAttributes[ a ].m_nType );
		pOutFields[ a ].m_nInstanceStepRate = 0;
		pOutFields[ a ].m_nOffset = m_pAttributes[ a ].m_nOffsetFloats * sizeof( float );
		pOutFields[ a ].m_nSemanticIndex = elementData.m_nSemanticIndex;
		pOutFields[ a ].m_nSlot = 0;
		pOutFields[ a ].m_nSlotType = RENDER_SLOT_PER_VERTEX;
		Q_strncpy( pOutFields[ a ].m_pSemanticName, elementData.m_pSemanticName, RENDER_INPUT_LAYOUT_FIELD_SEMANTIC_NAME_SIZE );
	}

	*pInOutNumFields = m_nAttributeCount;

	return true;
}

int CMesh::FindFirstAttributeOffset( VertexElement_t nType ) const
{
	for ( int a=0; a<m_nAttributeCount; ++a )
	{
		if ( m_pAttributes[ a ].m_nType == nType )
		{
			return m_pAttributes[ a ].m_nOffsetFloats;
		}
	}

	return -1;
}

void CMesh::RestrideVertexBuffer( int nNewStrideFloats )
{
	float *pNewMemory = new float[ nNewStrideFloats * m_nVertexCount ];
	int nMinStride = MIN( nNewStrideFloats, m_nVertexStrideFloats ) * sizeof( float );

	float *pNewStart = pNewMemory;
	for ( int i=0; i<m_nVertexCount; ++i )
	{
		Q_memcpy( pNewStart, GetVertex( i ), nMinStride );
		pNewStart += nNewStrideFloats;
	}

	delete []m_pVerts;
	m_pVerts = pNewMemory;
	m_nVertexStrideFloats = nNewStrideFloats;

	m_bAllocatedMeshData = true;
}

void CMesh::AddAttributes( CMeshVertexAttribute *pAttributes, int nAttributeCount )
{
	Assert( nAttributeCount );

	CMeshVertexAttribute *pNewAttributes = new CMeshVertexAttribute[ m_nAttributeCount + nAttributeCount ];
	for ( int a=0; a<m_nAttributeCount; ++a )
	{
		pNewAttributes[ a ] = m_pAttributes[ a ];
	}

	int nNewStrideFloats = m_nVertexStrideFloats;
	for ( int a=0; a<nAttributeCount; ++a )
	{
		nNewStrideFloats = MAX( pAttributes[ a ].m_nOffsetFloats + GetVertexElementSize( pAttributes[ a ].m_nType, VERTEX_COMPRESSION_NONE ) / (int)sizeof( float ), nNewStrideFloats );
		pNewAttributes[ m_nAttributeCount + a ] = pAttributes[ a ];
	}

	delete []m_pAttributes;
	m_pAttributes = pNewAttributes;
	m_nAttributeCount += nAttributeCount;

	if ( nNewStrideFloats > m_nVertexStrideFloats )
	{
		RestrideVertexBuffer( nNewStrideFloats );
	}
}

typedef CUtlVector<int> CIntVector;
void CMesh::CalculateTangents()
{
	int nPositionOffset = FindFirstAttributeOffset( VERTEX_ELEMENT_POSITION );
	int nNormalOffset = FindFirstAttributeOffset( VERTEX_ELEMENT_NORMAL );
	int nTexOffset = FindFirstAttributeOffset( VERTEX_ELEMENT_TEXCOORD2D_0 );
	if ( nTexOffset == -1 )
		nTexOffset = FindFirstAttributeOffset( VERTEX_ELEMENT_TEXCOORD3D_0 );

	if ( nPositionOffset == -1 || nTexOffset == -1 || nNormalOffset == -1 )
	{
		Msg( "Need valid position, normal, and texcoord when creating tangent frames!\n" );
		return;
	}

	// Look for a space to store tangents
	int nTangentOffset = FindFirstAttributeOffset( VERTEX_ELEMENT_TANGENT_WITH_FLIP );
	if ( nTangentOffset == -1 )
	{
		nTangentOffset = m_nVertexStrideFloats;

		// Add a tangent
		CMeshVertexAttribute attribute;
		attribute.m_nOffsetFloats = m_nVertexStrideFloats;
		attribute.m_nType = VERTEX_ELEMENT_TANGENT_WITH_FLIP;
		AddAttributes( &attribute, 1 );
	}

	// Calculate tangent ( pulled from studiomdl ).  We've left it this way for now to keep any weirdness from studiomdl that we've come to rely on.
	// In the future we should remove the vertToFaceMap and just accumulate tangents inplace in the vertices.
 
	// TODO: fix this function to iterate over quads as well as triangles
	int nIndicesPerFace = 3;

	int nFaces = m_nIndexCount / nIndicesPerFace;
	int nMaxIter = nIndicesPerFace;

	CUtlVector<CIntVector> vertToFaceMap;
	vertToFaceMap.AddMultipleToTail( m_nVertexCount );
	int index = 0;
	uint32 *pIndices = m_pIndices;
	for( int faceID = 0; faceID < nFaces; faceID++ )
	{
		for ( int i=0; i<nMaxIter; ++i )
		{
			vertToFaceMap[ pIndices[ index + i ] ].AddToTail( faceID );
		}

		index += nIndicesPerFace;
	}

	CUtlVector<Vector> faceSVect;
	CUtlVector<Vector> faceTVect;
	faceSVect.AddMultipleToTail( nFaces );
	faceTVect.AddMultipleToTail( nFaces );

	index = 0;
	for ( int f=0; f<nFaces; ++f )
	{
		Vector vPos[3];
		Vector2D vTex[3];
		for ( int i=0; i<nMaxIter; ++i )
		{
			float *pVertex = GetVertex( pIndices[ index + i ] );
			vPos[i] = *( ( Vector* )( pVertex + nPositionOffset ) );
			vTex[i] = *( ( Vector2D* )( pVertex + nTexOffset ) );
		}

		CalcTriangleTangentSpace( vPos[0], vPos[1], vPos[2],
								  vTex[0], vTex[1], vTex[2],
								  faceSVect[f], faceTVect[f] );

		index += nIndicesPerFace;
	}

	// Calculate an average tangent space for each vertex.
	for( int vertID = 0; vertID < m_nVertexCount; vertID++ )
	{
		float *pVertex = GetVertex( vertID );
		const Vector &normal = *( ( Vector* )( pVertex + nNormalOffset ) );
		Vector4D &finalSVect = *( ( Vector4D* )( pVertex + nTangentOffset ) );
		Vector sVect, tVect;

		sVect.Init( 0.0f, 0.0f, 0.0f );
		tVect.Init( 0.0f, 0.0f, 0.0f );
		for( int faceID = 0; faceID < vertToFaceMap[vertID].Count(); faceID++ )
		{
			sVect += faceSVect[vertToFaceMap[vertID][faceID]];
			tVect += faceTVect[vertToFaceMap[vertID][faceID]];
		}

		// Make an orthonormal system.
		// Need to check if we are left or right handed.
		Vector tmpVect;
		CrossProduct( sVect, tVect, tmpVect );
		bool leftHanded = DotProduct( tmpVect, normal ) < 0.0f;
		if( !leftHanded )
		{
			CrossProduct( normal, sVect, tVect );
			CrossProduct( tVect, normal, sVect );
			VectorNormalize( sVect );
			VectorNormalize( tVect );
			finalSVect[0] = sVect[0];
			finalSVect[1] = sVect[1];
			finalSVect[2] = sVect[2];
			finalSVect[3] = 1.0f;
		}
		else
		{
			CrossProduct( sVect, normal, tVect );
			CrossProduct( normal, tVect, sVect );
			VectorNormalize( sVect );
			VectorNormalize( tVect );
			finalSVect[0] = sVect[0];
			finalSVect[1] = sVect[1];
			finalSVect[2] = sVect[2];
			finalSVect[3] = -1.0f;
		}
	}
}

//--------------------------------------------------------------------------------------
// Calculates an unnormalized tangent space
//--------------------------------------------------------------------------------------
#define SMALL_FLOAT 1e-12
void CalcTriangleTangentSpaceL( const Vector &p0, const Vector &p1, const Vector &p2,
							   const Vector2D &t0, const Vector2D &t1, const Vector2D& t2,
							   Vector &sVect, Vector &tVect )
{
	/* Compute the partial derivatives of X, Y, and Z with respect to S and T. */
	sVect.Init( 0.0f, 0.0f, 0.0f );
	tVect.Init( 0.0f, 0.0f, 0.0f );

	// x, s, t
	Vector edge01( p1.x - p0.x, t1.x - t0.x, t1.y - t0.y );
	Vector edge02( p2.x - p0.x, t2.x - t0.x, t2.y - t0.y );

	Vector cross;
	CrossProduct( edge01, edge02, cross );
	if ( fabs( cross.x ) > SMALL_FLOAT )
	{
		sVect.x += -cross.y / cross.x;
		tVect.x += -cross.z / cross.x;
	}

	// y, s, t
	edge01.Init( p1.y - p0.y, t1.x - t0.x, t1.y - t0.y );
	edge02.Init( p2.y - p0.y, t2.x - t0.x, t2.y - t0.y );

	CrossProduct( edge01, edge02, cross );
	if ( fabs( cross.x ) > SMALL_FLOAT )
	{
		sVect.y += -cross.y / cross.x;
		tVect.y += -cross.z / cross.x;
	}

	// z, s, t
	edge01.Init( p1.z - p0.z, t1.x - t0.x, t1.y - t0.y );
	edge02.Init( p2.z - p0.z, t2.x - t0.x, t2.y - t0.y );

	CrossProduct( edge01, edge02, cross );
	if( fabs( cross.x ) > SMALL_FLOAT )
	{
		sVect.z += -cross.y / cross.x;
		tVect.z += -cross.z / cross.x;
	}
}

bool CMesh::CalculateTangentSpaceWorldLengthsPerFace( Vector2D *pLengthsOut, int nLengthsOut, float flMaxWorldPerUV )
{
	int nPositionOffset = FindFirstAttributeOffset( VERTEX_ELEMENT_POSITION );
	int nTexOffset = FindFirstAttributeOffset( VERTEX_ELEMENT_TEXCOORD2D_0 );
	if ( nTexOffset == -1 )
		nTexOffset = FindFirstAttributeOffset( VERTEX_ELEMENT_TEXCOORD3D_0 );

	if ( nPositionOffset == -1 || nTexOffset == -1 )
	{
		Msg( "Need valid position and texcoord when creating world space tangent lengths!\n" );
		return false;
	}

	// TODO: fix this to eventually iterate over quads
	int nIndicesPerFace = 3;

	int nFaces = m_nIndexCount / nIndicesPerFace;
	int nMaxIter = nIndicesPerFace;

	if ( nLengthsOut < nFaces )
	{
		return false;
	}

	// Get the textures
	float flMaxUnitsX = flMaxWorldPerUV;
	float flMaxUnitsY = flMaxWorldPerUV;

	Vector sdir;
	Vector tdir;
	int index = 0;
	uint32 *pIndices = m_pIndices;
	for ( int f=0; f<nFaces; ++f )
	{
		Vector2D vMin( FLT_MAX, FLT_MAX );

		// find the min UV
		Vector vPos[3];
		Vector2D vTex[3];
		for ( int i=0; i<nMaxIter; ++i )
		{
			float *pVertex = GetVertex( pIndices[ index + i ] );
			vPos[i] = *( ( Vector* )( pVertex + nPositionOffset ) );
			vTex[i] = *( ( Vector2D* )( pVertex + nTexOffset ) );
		}

		// calc tan-space
		CalcTriangleTangentSpaceL( vPos[0], vPos[1], vPos[2],
								   vTex[0], vTex[1], vTex[2],
								   sdir, tdir );

		pLengthsOut[ f ].x = MIN( flMaxUnitsX, sdir.Length() );
		pLengthsOut[ f ].y = MIN( flMaxUnitsY, sdir.Length() );

		index += nIndicesPerFace;
	}

	return true;
}

bool CMesh::CalculateFaceCenters( Vector *pCentersOut, int nCentersOut )
{
	int nPositionOffset = FindFirstAttributeOffset( VERTEX_ELEMENT_POSITION );
	if ( nPositionOffset == -1 )
	{
		Msg( "Need valid position to calculate face centers!\n" );
		return false;
	}

	// TODO: fix this to eventually iterate over quads
	int nIndicesPerFace = 3;

	int nFaces = m_nIndexCount / nIndicesPerFace;
	float fIndicesPerFace = (float)nIndicesPerFace;

	if ( nCentersOut < nFaces )
	{
		return false;
	}

	int index = 0;
	uint32 *pIndices = m_pIndices;
	for ( int f=0; f<nFaces; ++f )
	{
		pCentersOut[ f ] = Vector(0,0,0);

		for ( int i=0; i<nIndicesPerFace; ++i )
		{
			float *pVertex = GetVertex( pIndices[ index ] );
			Vector &vPos = *( ( Vector* )( pVertex + nPositionOffset ) );

			pCentersOut[ f ] += vPos;
			index++;
		}

		pCentersOut[ f ] /= fIndicesPerFace;
	}

	return true;
}

void DuplicateMesh( CMesh *pMeshOut, const CMesh &inputMesh )
{
	Assert( pMeshOut );
	pMeshOut->AllocateMesh( inputMesh.m_nVertexCount, inputMesh.m_nIndexCount, inputMesh.m_nVertexStrideFloats, inputMesh.m_pAttributes, inputMesh.m_nAttributeCount );

	Q_memcpy( pMeshOut->m_pVerts, inputMesh.m_pVerts, inputMesh.m_nVertexCount * inputMesh.m_nVertexStrideFloats * sizeof( float ) );
	Q_memcpy( pMeshOut->m_pIndices, inputMesh.m_pIndices, inputMesh.m_nIndexCount * sizeof( uint32 ) );
	pMeshOut->m_materialName = inputMesh.m_materialName;
}

// Shifts the UVs of an entire triangle to the origin defined by the smallest set of UV coordinates.
// The mesh must be de-indexed to do this.  This ensures our UVs are as close to the origin as possible.
bool RationalizeUVsInPlace( CMesh *pMesh )
{
	// We need to have texcoords to rationalize
	int nTexcoordOffset = pMesh->FindFirstAttributeOffset( VERTEX_ELEMENT_TEXCOORD2D_0 );
	if ( nTexcoordOffset < 0 )
	{
		nTexcoordOffset = pMesh->FindFirstAttributeOffset( VERTEX_ELEMENT_TEXCOORD3D_0 );
	}
	if ( nTexcoordOffset < 0 )
	{
		return false;
	}

	if ( pMesh->m_nVertexCount != pMesh->m_nIndexCount )
		return false;
	
	int nFaces = pMesh->m_nIndexCount / 3;
	float *pVertices = pMesh->m_pVerts;
	for ( int f=0; f<nFaces; ++f )
	{
		Vector2D vMin(FLT_MAX,FLT_MAX);

		// find the min UV
		float *pTriVerts = pVertices;
		for ( int i=0; i<3; ++i )
		{
			float flU = pVertices[ nTexcoordOffset     ];
			float flV = pVertices[ nTexcoordOffset + 1 ];
			
			vMin.x = MIN( vMin.x, flU );
			vMin.y = MIN( vMin.y, flV );

			pVertices += pMesh->m_nVertexStrideFloats;
		}

		// clamp to the nearest whole rep
		Vector2D vMinFloor;
		vMinFloor.x = floor( vMin.x );
		vMinFloor.y = floor( vMin.y );

		// rationalize UVs across the face
		for ( int i=0; i<3; ++i )
		{
			pTriVerts[ nTexcoordOffset     ] -= vMinFloor.x;
			pTriVerts[ nTexcoordOffset + 1 ] -= vMinFloor.y;

			pTriVerts += pMesh->m_nVertexStrideFloats;
		}
	}

	return true;
}

// Shifts the UVs of an entire triangle to the origin defined by the smallest set of UV coordinates.
// The mesh must be de-indexed to do this.  This ensures our UVs are as close to the origin as possible.
bool RationalizeUVs( CMesh *pRationalMeshOut, const CMesh &inputMesh )
{
	// We need our mesh de-indexed to rationalize UVs since we'll be shifting
	// UVs on a per-triangle basis
	if ( inputMesh.m_nVertexCount != inputMesh.m_nIndexCount )
	{
		DeIndexMesh( pRationalMeshOut, inputMesh );
	}
	else
	{
		pRationalMeshOut->AllocateMesh( inputMesh.m_nIndexCount, inputMesh.m_nIndexCount, inputMesh.m_nVertexStrideFloats, inputMesh.m_pAttributes, inputMesh.m_nAttributeCount );
	}

	return RationalizeUVsInPlace( pRationalMeshOut );
}

// Removes the need for an index buffer by storing redundant vertices
// back into the vertex buffer.
void DeIndexMesh( CMesh *pMeshOut, const CMesh &inputMesh )
{
	Assert( pMeshOut );
	pMeshOut->AllocateMesh( inputMesh.m_nIndexCount, inputMesh.m_nIndexCount, inputMesh.m_nVertexStrideFloats, inputMesh.m_pAttributes, inputMesh.m_nAttributeCount );

	float *pOutVerts = pMeshOut->m_pVerts;
	for ( int i=0; i<inputMesh.m_nIndexCount; ++i )
	{
		CopyVertex( pOutVerts, inputMesh.GetVertex( inputMesh.m_pIndices[ i ] ), inputMesh.m_nVertexStrideFloats );
		pOutVerts += inputMesh.m_nVertexStrideFloats;

		pMeshOut->m_pIndices[ i ] = i;
	}
}

// Combines two compatibles meshes into one.  The two meshes must have the same vertex stride.
bool ConcatMeshes( CMesh *pMeshOut, CMesh **ppMeshIn, int nInputMeshes, 
				   CMeshVertexAttribute *pAttributeOverride, int nAttributeOverrideCount, int nStrideOverride )
{
	Assert( pMeshOut && ppMeshIn && nInputMeshes > 1 );

	// Find total sizes
	int nTotalIndices = 0;
	int nTotalVertices = 0;
	int nAttributes = 0;
	int nStrideFloats = 0;
	CMeshVertexAttribute *pAttributes = NULL;
	for ( int m=0; m<nInputMeshes; ++m )
	{
		CMesh *pMesh = ppMeshIn[m];
		nTotalIndices += pMesh->m_nIndexCount;
		nTotalVertices += pMesh->m_nVertexCount;
		if ( m == 0 )
		{
			nAttributes = pMesh->m_nAttributeCount;
			nStrideFloats = pMesh->m_nVertexStrideFloats;
			pAttributes = pMesh->m_pAttributes;
		}
		else if ( nStrideOverride == 0 )
		{
			if ( nStrideFloats != pMesh->m_nVertexStrideFloats )
			{
				Warning( "Trying to concatenate differently strided meshes!\n" );
				return false;
			}
		}
	}

	if ( pAttributeOverride && nAttributeOverrideCount > 0 )
	{
		pAttributes = pAttributeOverride;
		nAttributes = nAttributeOverrideCount;
	}
	if ( nStrideOverride > 0 )
	{
		nStrideFloats = nStrideOverride;
	}
	
	pMeshOut->AllocateMesh( nTotalVertices, nTotalIndices, nStrideFloats, pAttributes, nAttributes );

	int nCurrentVertex = 0;
	int nCurrentIndex = 0;
	for ( int m=0; m<nInputMeshes; ++m )
	{
		CMesh *pMesh = ppMeshIn[m];

		for ( int v=0; v<pMesh->m_nVertexCount; ++v )
		{
			CopyVertex( pMeshOut->GetVertex( nCurrentVertex + v ), pMesh->GetVertex( v ), nStrideFloats );
		}

		for ( int i=0; i<pMesh->m_nIndexCount; ++i )
		{
			pMeshOut->m_pIndices[ nCurrentIndex ] = pMesh->m_pIndices[ i ] + nCurrentVertex;
			nCurrentIndex ++;
		}

		nCurrentVertex += pMesh->m_nVertexCount;
	}

	return true;
}

// Recursively tessellates a triangle if it's UV ranges are greater than 1
void TessellateTriangle( CUtlBuffer *pOutTriangles, const float **ppVertsIn, int nTexcoordOffset, int nVertexStride )
{
	// Find the longest and second longest edges
	float flLongestUV = -1;
	float flSecondLongestUV = -1;
	int nLongestEdge = -1;
	int nSecondLongestEdge = -1;
	for ( int e=0; e<3; ++e )
	{
		int eNext = ( e + 1 ) % 3;
		float flUDelta = fabs( ppVertsIn[ eNext ][ nTexcoordOffset     ] - ppVertsIn[ e ][ nTexcoordOffset     ] );
		float flVDelta = fabs( ppVertsIn[ eNext ][ nTexcoordOffset + 1 ] - ppVertsIn[ e ][ nTexcoordOffset + 1 ] );

		if ( flUDelta > flLongestUV )
		{
			flSecondLongestUV = flLongestUV;
			nSecondLongestEdge = nLongestEdge;

			flLongestUV = flUDelta;
			nLongestEdge = e;
		}
		else if ( flUDelta > flSecondLongestUV )
		{
			flSecondLongestUV = flUDelta;
			nSecondLongestEdge = e;
		}

		if ( flVDelta > flLongestUV )
		{
			flSecondLongestUV = flLongestUV;
			nSecondLongestEdge = nLongestEdge;

			flLongestUV = flVDelta;
			nLongestEdge = e;
		}
		else if( flVDelta > flSecondLongestUV )
		{
			flSecondLongestUV = flVDelta;
			nSecondLongestEdge = e;
		}
	}

	Assert( nLongestEdge > -1 && nSecondLongestEdge > -1 );
	
	static const int pCornerTable[3][3] = 
	{
		-1, // 0, 0
		1, // 0, 1
		0, // 0, 2

		1, // 1, 0
		-1, // 1, 1
		2, // 1, 2

		0, // 2, 0
		2, // 2, 1
		-1, // 2, 2
	};

	if ( flLongestUV > 1.0f )
	{
		// Subdivide in half
		float *pEdgeVertex0 = new float[ nVertexStride ];
		float *pEdgeVertex1 = new float[ nVertexStride ];

		// Find the vertex that is the corner of the two longest edges
		int nCornerVertex = pCornerTable[ nLongestEdge ][ nSecondLongestEdge ];
		Assert( nCornerVertex > -1 );
		int nCornerPlus1 = ( nCornerVertex + 1 ) % 3;
		int nCornerPlus2 = ( nCornerVertex + 2 ) % 3;

		// Cut the two longest edges in half
		LerpVertex( pEdgeVertex0, ppVertsIn[ nCornerVertex ], ppVertsIn[ nCornerPlus1  ], 0.5f, nVertexStride );
		LerpVertex( pEdgeVertex1, ppVertsIn[ nCornerPlus2  ], ppVertsIn[ nCornerVertex ], 0.5f, nVertexStride );

		// Test the 3 children
		const float *pVerts0[3] = { ppVertsIn[ nCornerVertex ], pEdgeVertex0, pEdgeVertex1 };
		TessellateTriangle( pOutTriangles, pVerts0, nTexcoordOffset, nVertexStride );

		const float *pVerts1[3] = { pEdgeVertex0, ppVertsIn[ nCornerPlus1 ], ppVertsIn[ nCornerPlus2 ] };
		TessellateTriangle( pOutTriangles, pVerts1, nTexcoordOffset, nVertexStride );

		const float *pVerts2[3] = { pEdgeVertex0, ppVertsIn[ nCornerPlus2 ], pEdgeVertex1 };
		TessellateTriangle( pOutTriangles, pVerts2, nTexcoordOffset, nVertexStride );

		delete []pEdgeVertex0;
		delete []pEdgeVertex1;
	}
	else
	{
		// This triangle is OK
		pOutTriangles->Put( ppVertsIn[ 0 ], nVertexStride * sizeof( float ) );
		pOutTriangles->Put( ppVertsIn[ 1 ], nVertexStride * sizeof( float ) );
		pOutTriangles->Put( ppVertsIn[ 2 ], nVertexStride * sizeof( float ) );
	}
}

// Subdivide triangles when their UV coordinates exceed the [0..1] range
bool TessellateOnWrappedUV( CMesh *pMeshOut, const CMesh &inputMesh )
{
	// We need to have texcoords to rationalize
	int nTexcoordOffset = inputMesh.FindFirstAttributeOffset( VERTEX_ELEMENT_TEXCOORD2D_0 );
	if ( nTexcoordOffset < 0 )
	{
		nTexcoordOffset = inputMesh.FindFirstAttributeOffset( VERTEX_ELEMENT_TEXCOORD3D_0 );
	}
	if ( nTexcoordOffset < 0 )
	{
		return false;
	}

	// Tessellate the triangles
	CUtlBuffer triangleBuffer;
	for ( int i=0; i<inputMesh.m_nIndexCount; i += 3 )
	{
		const float *pVerts[3];

		pVerts[ 0 ] = inputMesh.GetVertex( inputMesh.m_pIndices[ i     ] );
		pVerts[ 1 ] = inputMesh.GetVertex( inputMesh.m_pIndices[ i + 1 ] );
		pVerts[ 2 ] = inputMesh.GetVertex( inputMesh.m_pIndices[ i + 2 ] );

		TessellateTriangle( &triangleBuffer, pVerts, nTexcoordOffset, inputMesh.m_nVertexStrideFloats );
	}

	int nNewVertices = triangleBuffer.TellPut() / ( inputMesh.m_nVertexStrideFloats * sizeof( float ) );
	
	pMeshOut->AllocateMesh( nNewVertices, nNewVertices, inputMesh.m_nVertexStrideFloats, inputMesh.m_pAttributes, inputMesh.m_nAttributeCount );
	Q_memcpy( pMeshOut->m_pVerts, triangleBuffer.Base(), triangleBuffer.TellPut() );
	for ( int i=0; i<nNewVertices; ++i )
	{
		pMeshOut->m_pIndices[ i ] = i;
	}

	return RationalizeUVsInPlace( pMeshOut );
}

bool VertexMatches( const float *pV0, const float *pV1, const float *pEpsilons, int nEpsilons )
{
	for ( int i = 0; i < nEpsilons; ++i )
	{
		float flDelta = pV0[i] - pV1[i];
		if ( fabs( flDelta ) > pEpsilons[ i ] )
			return false;
	}
	return true;
}

class CVertexKDNode
{
public:	
	uint32 m_nChildren[2];
	int32 m_nAxis;
	float m_flSplit;
	
	inline bool IsLeaf() const { return m_nAxis == 0xFF ? true : false; }
	inline void InitAsSplit( float flSplit, int nAxis, int nCount )
	{
		m_nAxis = nAxis;
		m_flSplit = flSplit;
		m_nChildren[0] = uint32(~0);
		m_nChildren[1] = uint32(~0);
	}

	inline int GetLeafVertCount() const { Assert(IsLeaf()); return m_nChildren[1]; }
	inline int GetLeafVertStart() const { Assert(IsLeaf()); return m_nChildren[0]; }
	void InitAsLeaf( int nStart, int nCount )
	{
		m_nAxis = 0xFF;
		m_flSplit = 0;
		m_nChildren[0] = nStart;
		m_nChildren[1] = nCount;
	}
};

class CVertexKDTree
{
public:
	CVertexKDTree() {}
	void BuildMidpoint( const float *pVerts, int nVertexCount, int nVertexStrideFloats );
	void FindVertsInBox( CUtlVectorFixedGrowable<const float *, 64> &list, const Vector &mins, const Vector &maxs, int nStartNode = 0 );

private:
	int FindMidpointIndex( int nStart, int nCount, int nAxis, float flSplit );
	void ComputeBounds( Vector *pMins, Vector *pMaxs, int nStart, int nCount );
	int BuildNode( int nStart, int nCount );	 // recursive

	CUtlVector<CVertexKDNode> m_tree;
	CUtlVector<const float *> m_vertexList;
};

int CVertexKDTree::FindMidpointIndex( int nStart, int nCount, int nAxis, float flSplit )
{
	// partition the verts in this run on the axis with the greatest extent
	const float **pBase = m_vertexList.Base() + nStart;
	int nMid = nCount/2;
	int nEnd = nCount;
	for ( int i = nMid; i < nEnd; i++ )
	{
		if ( pBase[i][nAxis] < flSplit )
		{
			const float *pSwap = pBase[nMid];
			pBase[nMid] = pBase[i];
			pBase[i] = pSwap;
			nMid++;
		}
	}
	for ( int i = nMid-1; i >= 0; i-- )
	{
		if ( pBase[i][nAxis] >= flSplit )
		{
			const float *pSwap = pBase[nMid-1];
			pBase[nMid-1] = pBase[i];
			pBase[i] = pSwap;
			nMid--;
		}
	}

	return nMid + nStart;
}

void CVertexKDTree::FindVertsInBox( CUtlVectorFixedGrowable<const float *, 64> &list, const Vector &mins, const Vector &maxs, int nStartNode )
{
	const CVertexKDNode &node = m_tree[nStartNode];
	if ( node.IsLeaf() )
	{
		int nVertCount = node.GetLeafVertCount();
		int nVertStart = node.GetLeafVertStart();
		// check each vert at this leaf
		for ( int i = 0; i < nVertCount; i++ )
		{
			const float *pVert = m_vertexList[nVertStart + i];
			// is point in box, add to tail
			if ( pVert[0] >= mins.x && pVert[0] <= maxs.x && pVert[1] >= mins.y && pVert[1] <= maxs.y && pVert[2] >= mins.z && pVert[2] <= maxs.z )
			{
				list.AddToTail( pVert );
			}
		}
	}
	else
	{
		// recurse to the sides of the tree that contain the box
		int nAxis = node.m_nAxis;
		if ( mins[nAxis] <= node.m_flSplit )
		{
			FindVertsInBox( list, mins, maxs, node.m_nChildren[0] );
		}
		if ( maxs[nAxis] >= node.m_flSplit )
		{
			FindVertsInBox( list, mins, maxs, node.m_nChildren[1] );
		}
	}
}

void CVertexKDTree::ComputeBounds( Vector *pMins, Vector *pMaxs, int nStart, int nCount )
{
	Vector mins = *(Vector *)m_vertexList[nStart];
	Vector maxs = mins;
	for ( int i = 1; i < nCount; i++ )
	{
		mins.x = MIN(mins.x, m_vertexList[i+nStart][0]);
		maxs.x = MAX(maxs.x, m_vertexList[i+nStart][0]);
		mins.y = MIN(mins.y, m_vertexList[i+nStart][1]);
		maxs.y = MAX(maxs.y, m_vertexList[i+nStart][1]);
		mins.z = MIN(mins.z, m_vertexList[i+nStart][2]);
		maxs.z = MAX(maxs.z, m_vertexList[i+nStart][2]);
	}
	if ( pMins )
	{
		*pMins = mins;
	}
	if ( pMaxs )
	{
		*pMaxs = maxs;
	}
}

inline int GreatestAxis( const Vector &v )
{
	if ( v.x >= v.y )
	{
		return v.x > v.z ? 0 : 2;
	}
	return v.y > v.z ? 1 : 2;
}

int CVertexKDTree::BuildNode( int nStart, int nCount )
{
	if ( nCount > 8 )
	{
		Vector mins, maxs;
		ComputeBounds( &mins, &maxs, nStart, nCount );
		int nAxis = GreatestAxis( maxs - mins );
		float flSplit = 0.5f * (maxs[nAxis] + mins[nAxis]);
		int nSplit = FindMidpointIndex( nStart, nCount, nAxis, flSplit );
		int nLeftCount = nSplit - nStart;
		int nRightCount = (nStart + nCount) - nSplit;
		if ( nLeftCount != 0 && nRightCount != 0 )
		{
			int nIndex = m_tree.AddToTail();
			int nLeft = BuildNode( nStart, nLeftCount );
			int nRight = BuildNode( nSplit, nRightCount );
			m_tree[nIndex].InitAsSplit( flSplit, nAxis, nCount );
			m_tree[nIndex].m_nChildren[0] = nLeft;
			m_tree[nIndex].m_nChildren[1] = nRight;
			return nIndex;
		}
	}
	int nIndex = m_tree.AddToTail();
	m_tree[nIndex].InitAsLeaf( nStart, nCount );
	return nIndex;
}

void CVertexKDTree::BuildMidpoint( const float *pVerts, int nVertexCount, int nVertexStrideFloats )
{
	m_vertexList.SetCount( nVertexCount );
	for ( int i = 0; i < nVertexCount; i++ )
	{
		m_vertexList[i] = pVerts + i * nVertexStrideFloats;
	}

	BuildNode( 0, nVertexCount );
}


// TODO: Extreme welds can cause faces to flip/invert.  Should we add an option to detect and avoid this?
bool WeldVertices( CMesh *pMeshOut, const CMesh &inputMesh, float *pEpsilons, int nEpsilons )
{
	// Must have epsilons for at least the first three position components
	if ( nEpsilons != inputMesh.m_nVertexStrideFloats )
		return false;

	CVertexKDTree searchTree;
	searchTree.BuildMidpoint( inputMesh.m_pVerts, inputMesh.m_nVertexCount, inputMesh.m_nVertexStrideFloats );
	CUtlVector< const float* > inOrderVertices;
	CUtlVector<uint32> remapTable;
	const uint32 nEmptyValue = uint32(~0);
	remapTable.SetCount( inputMesh.m_nVertexCount );
	remapTable.FillWithValue( nEmptyValue );

	Vector mins, maxs;
	CUtlVectorFixedGrowable<const float *, 64> list;

	for ( int i = 0; i < inputMesh.m_nVertexCount; i++ )
	{
		// skip if already welded this vertex
		if ( remapTable[i] != nEmptyValue )
			continue;

		// build weld box around vert
		const float *pVertex = inputMesh.GetVertex(i);
		mins.x = pVertex[0] - pEpsilons[0];
		mins.y = pVertex[1] - pEpsilons[1];
		mins.z = pVertex[2] - pEpsilons[2];
		maxs.x = pVertex[0] + pEpsilons[0];
		maxs.y = pVertex[1] + pEpsilons[1];
		maxs.z = pVertex[2] + pEpsilons[2];
		list.RemoveAll();
		searchTree.FindVertsInBox( list, mins, maxs );
		for ( int j = 0; j < list.Count(); j++ )
		{
			const float *pCheck = list[j];
			// only check lower indexed vertices, the opposite check will happen in future iterations
			if ( pCheck >= pVertex )
				continue;

			int nMatchIndex = (list[j] - inputMesh.m_pVerts) / inputMesh.m_nVertexStrideFloats;
			Assert(remapTable[nMatchIndex] != nEmptyValue);
			// match the output vert instead of the input - this vert may have been welded to some other vert
			pCheck = inOrderVertices[remapTable[nMatchIndex]];
			if ( VertexMatches( pVertex, pCheck, pEpsilons, inputMesh.m_nVertexStrideFloats ) )
			{
				remapTable[i] = remapTable[nMatchIndex];
				break;
			}
		}
		if ( remapTable[i] == nEmptyValue )
		{
			int nVertexIndex = inOrderVertices.AddToTail( pVertex );
			remapTable[i] = nVertexIndex;
		}
	}
	// allocate enough for all new verts
	int nNewVerts = inOrderVertices.Count();
	pMeshOut->AllocateMesh( nNewVerts, inputMesh.m_nIndexCount, inputMesh.m_nVertexStrideFloats, inputMesh.m_pAttributes, inputMesh.m_nAttributeCount );

	// copy the welded verts out
	for ( int v=0; v<nNewVerts; ++v )
	{
		CopyVertex( pMeshOut->GetVertex( v ), inOrderVertices[ v ], inputMesh.m_nVertexStrideFloats );
	}

	// now remap the indices
	for ( int i=0; i<inputMesh.m_nIndexCount; ++i )
	{
		pMeshOut->m_pIndices[ i ] = remapTable[ inputMesh.m_pIndices[ i ] ];
		Assert( pMeshOut->m_pIndices[i] < (uint32)nNewVerts );
	}

	return true;
}


void CleanMesh( CMesh *pMeshOut, const CMesh &inputMesh )
{
	CUtlVector<uint32> indexMap;
	indexMap.SetCount( inputMesh.m_nVertexCount );
	const uint32 nUnusedIndex = ~0UL;
	for ( int i = 0; i < inputMesh.m_nVertexCount; i++ )
	{
		indexMap[i] = nUnusedIndex;
	}

	// build a compact map of vertices
	uint32 nVertexOut = 0;
	int nIndexOut = 0;
	for ( int i = 0; i < inputMesh.m_nIndexCount; i += 3 )
	{
		// skip degenerate triangles
		int nV0 = inputMesh.m_pIndices[i+0];
		int nV1 = inputMesh.m_pIndices[i+1];
		int nV2 = inputMesh.m_pIndices[i+2];
		if ( nV0 == nV1 || nV1 == nV2 || nV0 == nV2 )
			continue;

		if ( indexMap[nV0] == nUnusedIndex )
		{
			indexMap[nV0] = nVertexOut++;
		}
		if ( indexMap[nV1] == nUnusedIndex )
		{
			indexMap[nV1] = nVertexOut++;
		}
		if ( indexMap[nV2] == nUnusedIndex )
		{
			indexMap[nV2] = nVertexOut++;
		}
		nIndexOut += 3;
	}

	// allocate the cleaned mesh now that we know its size
	pMeshOut->AllocateMesh( nVertexOut, nIndexOut, inputMesh.m_nVertexStrideFloats, inputMesh.m_pAttributes, inputMesh.m_nAttributeCount );
	nIndexOut = 0;
	for ( int i = 0; i < inputMesh.m_nIndexCount; i += 3 )
	{
		// skip degenerate triangles (again)
		int nV0 = inputMesh.m_pIndices[i+0];
		int nV1 = inputMesh.m_pIndices[i+1];
		int nV2 = inputMesh.m_pIndices[i+2];
		if ( nV0 == nV1 || nV1 == nV2 || nV0 == nV2 )
			continue;

		// copy and remap the indices
		pMeshOut->m_pIndices[ nIndexOut++ ] = indexMap[ nV0 ];
		pMeshOut->m_pIndices[ nIndexOut++ ] = indexMap[ nV1 ];
		pMeshOut->m_pIndices[ nIndexOut++ ] = indexMap[ nV2 ];
	}
	Assert( nIndexOut == pMeshOut->m_nIndexCount );

	// copy out the vertices in order by use
	for ( int v = 0; v < inputMesh.m_nVertexCount; ++v )
	{
		if ( indexMap[v] == nUnusedIndex )
			continue;
		uint32 nVOut = indexMap[v];
		CopyVertex( pMeshOut->GetVertex( nVOut ), inputMesh.GetVertex(v), inputMesh.m_nVertexStrideFloats );
	}
}

// partitions the mesh into an array of meshes, each with <= nMaxVertex vertices
void SplitMesh( CUtlVector<CMesh> &list, const CMesh &input, int nMaxVertex )
{
	const uint32 nUnusedIndex = uint32(~0);
	CUtlVector<uint32> indexMap;
	indexMap.SetCount( input.m_nVertexCount );
	for ( int i = 0; i < input.m_nVertexCount; i++ )
	{
		indexMap[i] = nUnusedIndex;
	}

	CUtlVectorFixedGrowable<int, 32> indexCount;
	CUtlVectorFixedGrowable<int, 32> vertexCount;

	const uint32 *pIndex = input.m_pIndices;
	const int nCount = input.m_nIndexCount;
	int nIndex = 0;
	int nIndexOut = 0;
	int nVertexOut = 0;
	// count how many you need
	while ( nIndex < nCount )
	{
		if ( indexMap[pIndex[nIndex]] == nUnusedIndex )
		{
			indexMap[pIndex[nIndex]] = nVertexOut;
			nVertexOut++;
			if ( nVertexOut >= nMaxVertex )
			{
				nIndexOut -= (nIndex%3);
				nIndex -= (nIndex%3);
				indexCount.AddToTail(nIndexOut);
				vertexCount.AddToTail(nVertexOut);
				for ( int i = 0; i < input.m_nVertexCount; i++ )
				{
					indexMap[i] = nUnusedIndex;
				}
				nVertexOut = 0;
				nIndexOut = 0;
				continue;
			}
		}
		nIndexOut++;
		nIndex++;
	}
	if ( nVertexOut > 0 && nIndexOut > 0 )
	{
		indexCount.AddToTail( nIndexOut );
		vertexCount.AddToTail( nVertexOut );
	}

	// now allocate the actual meshes and populate them
	nIndex = 0;
	int nMeshCount = indexCount.Count();
	list.SetCount( nMeshCount );
	for ( int nMesh = 0; nMesh < nMeshCount; nMesh++ )
	{
		list[nMesh].AllocateMesh( vertexCount[nMesh], indexCount[nMesh], input.m_nVertexStrideFloats, input.m_pAttributes, input.m_nAttributeCount );

		int nIndexCount = indexCount[nMesh];
		nVertexOut = 0;
		Assert( (nIndexCount%3) == 0 );
		Assert( (nIndex%3) == 0 );
		for ( int i = 0; i < input.m_nVertexCount; i++ )
		{
			indexMap[i] = nUnusedIndex;
		}
		for ( int j = 0; j < nIndexCount; j++ )
		{
			int nV0 = pIndex[nIndex];
			if ( indexMap[nV0] == nUnusedIndex )
			{
				CopyVertex( list[nMesh].GetVertex(nVertexOut), input.GetVertex(nV0), input.m_nVertexStrideFloats );
				indexMap[nV0] = nVertexOut;
				nVertexOut++;
				Assert( nVertexOut <= vertexCount[nMesh] );
			}
			list[nMesh].m_pIndices[j] = indexMap[nV0];
			nIndex++;
		}
	}
}


bool CMesh::HasSkinningData()const
{
	return GetSkinningDataFields().HasSkinningData();
}


// Find and return joint weight and joint indices attributes in a convenient struct
CMesh::SkinningDataFields_t CMesh::GetSkinningDataFields()const
{
	SkinningDataFields_t dataFields;

	for ( int a = 0; a < m_nAttributeCount; ++a )
	{
		if ( m_pAttributes[ a ].IsJointWeight() )
		{
			if ( dataFields.m_nBoneWeights < 0 /*|| m_pAttributes[ a ].m_nSemanticIndex < m_pAttributes[ dataFields.m_nBoneWeights ].m_nSemanticIndex*/ )
			{
				dataFields.m_nBoneWeights = a;
			}
		}
		if ( m_pAttributes[ a ].IsJointIndices() )
		{
			if ( dataFields.m_nBoneIndices < 0 /*|| m_pAttributes[ a ].m_nSemanticIndex < m_pAttributes[ dataFields.m_nBoneIndices ].m_nSemanticIndex*/ )
			{
				dataFields.m_nBoneIndices = a;
			}
		}
	}
	// we must have either both or none or indices but not just weights by themselves
	Assert( ( dataFields.m_nBoneWeights < 0 ) || ( dataFields.m_nBoneIndices >= 0 ) );
	return dataFields;
}


CMesh::ClothDataFields_t CMesh::GetClothDataFields() const
{
	ClothDataFields_t dataFields;
	for ( int a = 0; a < m_nAttributeCount; ++a )
	{
		if ( m_pAttributes[ a ].IsClothEnable() )
		{
			dataFields.m_nClothEnable = a;
		}
		if ( m_pAttributes[ a ].IsPositionRemap() )
		{
			dataFields.m_nPositionRemap = a;
		}
	}

	return dataFields;
}


int CMesh::GetAttrSizeFloats( int nAttribute )
{
	if ( nAttribute < 0 || nAttribute >= m_nAttributeCount )
		return 0;
	if ( nAttribute + 1 >= m_nAttributeCount )
		return m_nVertexStrideFloats - m_pAttributes[ nAttribute ].m_nOffsetFloats;
	
	return m_pAttributes[ nAttribute + 1 ].m_nOffsetFloats - m_pAttributes[ nAttribute ].m_nOffsetFloats;
}

float CMesh::GetVertexJointSumWeight( const SkinningDataFields_t &skinData, int nVertex, const CVarBitVec &jointSet )
{
	if ( skinData.m_nBoneIndices < 0 )
	{
		return 0.0f; // this isn't a skinned mesh
	}
	const CMeshVertexAttribute &attrIndices = m_pAttributes[ skinData.m_nBoneIndices ];
	int nWeightAttrSize= Min( GetAttrSizeFloats( skinData.m_nBoneIndices ), GetAttrSizeFloats( skinData.m_nBoneWeights ) );
	if ( nWeightAttrSize > 1 )
	{
		Assert( uint( skinData.m_nBoneWeights ) < uint( m_nAttributeCount ) );
		const CMeshVertexAttribute &attrWeights = m_pAttributes[ skinData.m_nBoneWeights ];
		//Assert( attrIndices.m_nSizeFloats == attrWeights.m_nSizeFloats );

		float flSumWeight = 0.0f;
		for ( int nWeightIndex = 0; nWeightIndex < nWeightAttrSize; ++nWeightIndex )
		{
			const float *pVertex = GetVertex( nVertex );
			int nAssignedJoint = *( int* )( pVertex + attrIndices.m_nOffsetFloats + nWeightIndex );
			if ( jointSet.IsBitSet( nAssignedJoint ) )
			{
				float flAssignedWeight = pVertex[ attrWeights.m_nOffsetFloats + nWeightIndex ];
				flSumWeight += flAssignedWeight;
			}
		}
		return flSumWeight; // didn't find the joint
	}
	else
	{
		// there's just one weight
		int nAssignedJoint = *( int* )( GetVertex( nVertex ) + attrIndices.m_nOffsetFloats );
		return jointSet.IsBitSet( nAssignedJoint ) ? 1.0f : 0.0f;
	}
}
