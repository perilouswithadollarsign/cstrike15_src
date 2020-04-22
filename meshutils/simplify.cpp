//=========== Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Mesh simplification routines.
//
//===========================================================================//
#include "mathlib/vector.h"
#include "simplify.h"
#include "tier1/utlpriorityqueue.h"
#include "mathlib/cholesky.h"
#include "tier1/utlhash.h"
#include "tier0/vprof.h"
#include "memdbgon.h"

// quick vprof wrappers
static void Vprof_MarkFrame_IfEnabled()
{
#if VPROF_LEVEL > 0
	g_VProfCurrentProfile.MarkFrame();
#endif
}

static void Vprof_Start_IfEnabled()
{
#if VPROF_LEVEL > 0
	g_VProfCurrentProfile.Reset();
	g_VProfCurrentProfile.ResetPeaks();
	g_VProfCurrentProfile.Start();
#endif
}

static void Vprof_Report_IfEnabled()
{
#if VPROF_LEVEL > 0
	g_VProfCurrentProfile.Stop();
	g_VProfCurrentProfile.OutputReport( VPRT_FULL & ~VPRT_HIERARCHY, NULL );
	Msg("VPROF Trace Complete\n");
#endif
}


// This is a shared edge and its corresponding error metric
class CQEMEdge
{
public:
	CQEMEdge() { Init(0,0); }
	CQEMEdge( int nV0, int nV1 ) { Init(nV0, nV1); }
	void Init( int nV0, int nV1 )
	{
		m_bCollapsed = 0;
		m_bNonManifold = 0;
		m_flCurrentError = 0.0f;
		m_nBestIndex = 0;
		m_nReferences = 0;
		m_nVert[0] = nV0;
		m_nVert[1] = nV1;
	}

	void InitVerts( int nV0, int nV1 )
	{
		m_nVert[0] = MIN(nV0, nV1);
		m_nVert[1] = MAX(nV0, nV1);
	}

	inline void MarkCollapsed() { m_bCollapsed = true; }
	inline bool IsCollapsed() { return m_bCollapsed; }
	void UpdateError( const Vector &v0, const Vector &v1, const CQuadricError *pVertError )
	{
		m_error = pVertError[m_nVert[0]] + pVertError[m_nVert[1]];
		m_flCurrentError = m_error.ComputeError(v0);
		float err1 = m_error.ComputeError(v1);
		m_nBestIndex = 0;
		if ( err1 < m_flCurrentError )
		{
			m_vOptimal = v1;
			m_nBestIndex = 1;
			m_flCurrentError = err1;
			m_flInterp = 1.0f;
		}
		else
		{
			m_vOptimal = v0;
			m_flInterp = 0.0f;
		}
		Vector vTest = m_error.SolveForMinimumError();
		float flError = m_error.ComputeError( vTest );
		if ( flError < m_flCurrentError )
		{
			Vector vEdge = v1 - v0;
			float flLen = VectorNormalize(vEdge);
			float flDist0 = (vTest - v0).Length();
			float flDist1 = (vTest - v1).Length();
			if ( flDist0 < flLen || flDist1 < flLen )
			{
				m_vOptimal = vTest;
				m_flCurrentError = flError;

				Vector vTmp = m_vOptimal - v0;
				if ( flLen > 0.0f )
				{
					m_flInterp = (1.0f / flLen) * DotProduct( vTmp, vEdge );
					m_flInterp = clamp( m_flInterp, 0.0f, 1.0f );
				}
			}
		}
		m_flCurrentError = MAX(0.0f, m_flCurrentError);
	}
	int GetVertIndex( int nVert ) { return m_nVert[0] == nVert ? 0 : 1; }

	CQuadricError m_error;
	Vector m_vOptimal;
	float m_flCurrentError;
	float m_flInterp;
	int m_nVert[2];
	int m_nReferences;
	short m_nBestIndex;
	bool m_bCollapsed;
	bool m_bNonManifold;
};

// We maintain a sorted queue of edges to collapse.  The queue stores a copy of the error and links back to an edge
// as edges collapse, adjacent edges are updated.  Rather than searching the queue for their current errors we simply
// insert additional records.  So the copy of the error is convenient for sorting the queue and also determining if 
// this is the most up to date record for a given edge
struct edge_queue_entry_t
{
	float m_flError;
	int m_nEdgeIndex;
};

// This is the sorted queue of edges to collapse.  It includes some invalid records
// This implements the comparison function for sorting
class CEdgeQueue : public CUtlPriorityQueue<edge_queue_entry_t>
{
public:
	static bool IsLowerPriority( const edge_queue_entry_t &node1, const edge_queue_entry_t &node2 )
	{
		// edges with higher error are lower priority
		return ( node1.m_flError > node2.m_flError ) ? true : false;
	}

	CEdgeQueue( int nInitSize = 0 ) : CUtlPriorityQueue<edge_queue_entry_t>( 0, nInitSize, IsLowerPriority ) {}
};


// This is the adjacency and error metric storage class for a mesh
// Each vertex stores a list of triangles (v0,v1,v2).  v0 is the vertex storing the list, the list stores the other two
struct vertex_triangle_t
{
	uint32 nV1;
	uint32 nV2;
};

class CVertVisit
{
public:
	CUtlVector<vertex_triangle_t> m_triangles;
};

struct edge_hash_t
{
	CQEMEdge		*m_pSharedEdge;
	inline bool operator==( const edge_hash_t& src ) const { return src.m_nV0 == m_nV0 && src.m_nV1 == m_nV1; }
	uint32			m_nV0;
	uint32			m_nV1;
};

class CUniqueVertexList : public CUtlVectorFixedGrowable<uint32, 32>
{
public:
	void AddIfUnique( uint32 nVertex )
	{
		if ( Find( nVertex ) == -1 )
		{
			AddToTail( nVertex );
		}
	}
};

class CMeshVisit
{
public:
	void BuildFromMesh( const CMesh &input );
	int FindMinErrorEdge();
	void CollapseEdge( int nCollapse );
	void RemapEdge( int nVertexRemove, int nVertexConnect, int nVertexKeep );
	void ComputeVertListError( float flOpenEdgePenalty, float flMinArea, float flMaxArea, const mesh_simplifyweights_t *pWeights );
	inline void UpdateEdgeError( CQEMEdge *pEdge )
	{
		pEdge->UpdateError( GetVertexPosition(pEdge->m_nVert[0]), GetVertexPosition(pEdge->m_nVert[1]), m_errorVert.Base() );
		edge_queue_entry_t entry;
		entry.m_flError = pEdge->m_flCurrentError;
		entry.m_nEdgeIndex = pEdge - m_edgeList.Base();
		m_edgeQueue.Insert( entry );
	}
	void Get1Ring( CUniqueVertexList &list, uint32 nVertex );
	int CountSharedVerts( int nVert0, int nVert1 );
	bool IsValidCollapse( int nMinEdge );
	bool IsValidCollapseVertex( int nVertCheck, int nVertOpposite, const Vector &vReplacePos );
	int CountTotalUsedVerts();

	UtlHashFastHandle_t FindEdge( int nV0, int nV1 )
	{
		edge_hash_t tmp;
		tmp.m_nV0 = MIN(nV0, nV1);
		tmp.m_nV1 = MAX(nV0, nV1);
		uint nHashKey = VertHashKey(MIN(nV0, nV1), MAX(nV0, nV1));
		tmp.m_pSharedEdge = NULL;
		return m_edgeHash.Find( nHashKey, tmp );
	}

	bool IsOpenEdge( int nV0, int nV1 )
	{
		UtlHashFastHandle_t edgeHashIndex = FindEdge( nV0, nV1 );
		if ( m_edgeHash.InvalidHandle() != edgeHashIndex )
		{
			return m_edgeHash.Element(edgeHashIndex).m_pSharedEdge->m_nReferences < 2 ? true : false;
		}
		return false;
	}


	// Copies out the mesh in its current state
	void WriteMeshIndexList( CUtlVector<uint32> &indexOut );

	inline float *GetVertex( int nIndex ) { return m_pVertexBase + nIndex * m_nVertexStrideFloats; }
	inline Vector &GetVertexPosition(int nIndex) { return *(Vector *)GetVertex(nIndex); }

	CUtlVector<CQEMEdge>	m_edgeList;
	CUtlScalarHash<edge_hash_t> m_edgeHash;
	CUtlVector<CQuadricError> m_errorVert;
	CUtlVector<CVertVisit>	m_vertList;

	CEdgeQueue				m_edgeQueue;
	CUtlVector<float>		m_vertData;
	float					*m_pVertexBase;
	int						m_nVertexStrideFloats;
	int						m_nInputVertCount;
	int						m_nTriangleCount;
	int						m_nCollapseIndex;
	float					m_flIntegrationPenalty;
};


// Copies out the mesh in its current state
void CMeshVisit::WriteMeshIndexList( CUtlVector<uint32> &indexOut )
{
	uint32 nCurrentVertexCount = m_vertList.Count();
	int nTotalTrianglesRemaining = 0;
	for ( uint32 i = 0; i < nCurrentVertexCount; i++ )
	{
		nTotalTrianglesRemaining += m_vertList[i].m_triangles.Count();
	}
	// each triangle must be referenced 3 times, once at each vertex
	Assert( (nTotalTrianglesRemaining % 3) == 0 );

	indexOut.SetCount( nTotalTrianglesRemaining );
	int nWriteIndex = 0;
	for ( uint32 i = 0; i < nCurrentVertexCount; i++ )
	{
		for ( int j = 0; j < m_vertList[i].m_triangles.Count(); j++ )
		{
			vertex_triangle_t &tri = m_vertList[i].m_triangles[j];
			// only write each triangle once, skip the other two defs.
			// do this by only writing if v0 is the min vert index
			if ( tri.nV1 < i || tri.nV2 < i )
				continue;
			indexOut[nWriteIndex++] = i;
			indexOut[nWriteIndex++] = tri.nV1;
			indexOut[nWriteIndex++] = tri.nV2;
		}
	}
}

// Remaps one of the verts on an edge
void CMeshVisit::RemapEdge( int nVertexRemove, int nVertexConnect, int nVertexKeep )
{
	UtlHashFastHandle_t edgeHashIndex = FindEdge( nVertexRemove, nVertexConnect );
	if ( m_edgeHash.InvalidHandle() == edgeHashIndex )
		return;

	edge_hash_t tmp = m_edgeHash.Element( edgeHashIndex );
	CQEMEdge *pEdge = tmp.m_pSharedEdge;
	bool bNeedsUpdate = false;
	for ( int i = 0; i < 2; i++ )
	{
		if ( pEdge->m_nVert[i] == nVertexRemove )
		{
			pEdge->m_nVert[i] = nVertexKeep;
			bNeedsUpdate = true;
		}
	}
	if ( bNeedsUpdate )
	{
		m_edgeHash.Remove( edgeHashIndex );
		if ( pEdge->m_nVert[0] == pEdge->m_nVert[1] )
		{
			pEdge->MarkCollapsed();
		}
		else
		{
			UpdateEdgeError(pEdge);
			tmp.m_nV0 = pEdge->m_nVert[0];
			tmp.m_nV1 = pEdge->m_nVert[1];
			if ( tmp.m_nV0 > tmp.m_nV1 )	// swap so min is in m_nV0
			{
				tmp.m_nV0 = tmp.m_nV1;
				tmp.m_nV1 = pEdge->m_nVert[0];
			}
			uint nHashKey = VertHashKey(tmp.m_nV0, tmp.m_nV1);
			if ( m_edgeHash.Find( nHashKey, tmp ) != m_edgeHash.InvalidHandle() )
			{
				// another edge with these indices exists in the table, mark as collapsed
				pEdge->MarkCollapsed();
			}
			else
			{
				m_edgeHash.Insert( nHashKey, tmp );
			}
		}
	}
}

void CMeshVisit::Get1Ring( CUniqueVertexList &list, uint32 nVertex )
{
	int nTriCount = m_vertList[nVertex].m_triangles.Count();
	for ( int i = 0; i < nTriCount; i++ )
	{
		const vertex_triangle_t &tri = m_vertList[nVertex].m_triangles[i];
		list.AddIfUnique( tri.nV1 );
		list.AddIfUnique( tri.nV2 );
	}
}

// Counts the number of adjacent verts shared by a pair of verts
// This is used to prevent creating shark fin topologies
int CMeshVisit::CountSharedVerts( int nVert0, int nVert1 )
{
	CUniqueVertexList vertIndex0;
	CUniqueVertexList vertIndex1;

	Get1Ring( vertIndex0, nVert0 );
	Get1Ring( vertIndex1, nVert1 );
	int nSharedCount = 0;
	for ( int i = 0; i < vertIndex1.Count(); i++ )
	{
		if ( vertIndex0.Find(vertIndex1[i]) != -1 )
		{
			nSharedCount++;
		}
	}
	return nSharedCount;
}

// Heuristics for avoiding collapsing edges that will generate bad topology
bool CMeshVisit::IsValidCollapseVertex( int nVertCheck, int nVertOpposite, const Vector &vReplacePos )
{
	Vector vOld = GetVertexPosition(nVertCheck);

	int nTriCount = m_vertList[nVertCheck].m_triangles.Count();

	for ( int i = 0; i < nTriCount; i++ )
	{
		const vertex_triangle_t &tri = m_vertList[nVertCheck].m_triangles[i];
		int nV1 = tri.nV1;
		int nV2 = tri.nV2;
		// this triangle has the collapsing edge, skip it
		if ( nV1 == nVertOpposite || nV2 == nVertOpposite )
			continue;

		Vector v1 = GetVertexPosition(nV1);
		Vector v2 = GetVertexPosition(nV2);
		Vector vEdge0 = v1 - vOld;
		Vector vEdge1 = v2 - vOld;
		Vector vNormal = CrossProduct( vEdge1, vEdge0 );
		Vector vNewEdge0 = v1 - vReplacePos;
		Vector vNewEdge1 = v2 - vReplacePos;
		Vector vNewNormal = CrossProduct( vNewEdge1, vNewEdge0 );
		float flDot = DotProduct( vNewNormal, vNormal );

		// If the collapse will flip the face, avoid it
		if ( flDot < 0.0f )
			return false;
	}

	return true;
}

bool CMeshVisit::IsValidCollapse( int nMinEdge )
{
	VPROF("IsValidcollapse");
	if ( nMinEdge < 0 )
		return true;

	if ( m_edgeList[nMinEdge].m_bNonManifold )
		return false;

	int nVert0 = m_edgeList[nMinEdge].m_nVert[0];
	int nVert1 = m_edgeList[nMinEdge].m_nVert[1];
	// This constraint should keep shark-fin like geometries from forming
	if ( CountSharedVerts( nVert0, nVert1 ) > 2 )
		return false;

	Vector vOptimal = m_edgeList[nMinEdge].m_vOptimal;
	return IsValidCollapseVertex( nVert0, nVert1, vOptimal ) && IsValidCollapseVertex( nVert1, nVert0, vOptimal );
}


void CMeshVisit::CollapseEdge( int nCollapse )
{
	VPROF("CollapseEdge");
	m_edgeList[nCollapse].MarkCollapsed();
	Vector vOptimal = m_edgeList[nCollapse].m_vOptimal;
	// get the vert being removed
	Assert(nCollapse < m_edgeList.Count() );
	int nV0 = m_edgeList[nCollapse].m_nVert[0];
	int nV1 = m_edgeList[nCollapse].m_nVert[1];
	Assert( nV0 < m_vertList.Count() );
	Assert( nV1 < m_vertList.Count() );
	float flInterp = m_edgeList[nCollapse].m_flInterp;
	uint32 nVertexKeep = nV0;
	uint32 nVertexRemove = nV1;
	if ( m_edgeList[nCollapse].m_nBestIndex == 1 )
	{
		nVertexKeep = m_edgeList[nCollapse].m_nVert[1];
		nVertexRemove = m_edgeList[nCollapse].m_nVert[0];
	}

	// propagate the error
	m_errorVert[nVertexKeep] += m_errorVert[nVertexRemove];
	m_errorVert[nVertexKeep] *= m_flIntegrationPenalty;

	CUniqueVertexList vertIndex;
	Get1Ring( vertIndex, nVertexRemove );
	// @TODO: Need to copy triangles over if we allow merging unconnected pairs
	// Assert that this pair is connected
	// Assert( vertIndex.Find(nVertexKeep) != -1 );

	int nTrianglesRemoved = 0;
	CUtlVectorFixedGrowable<uint32, 32> removeList;

	for ( int i = 0; i < vertIndex.Count(); i++ )
	{
		uint32 nVertex = vertIndex[i];
		for ( int j = 0; j < m_vertList[nVertex].m_triangles.Count(); j++ )
		{
			vertex_triangle_t &tri = m_vertList[nVertex].m_triangles[j];
			if ( tri.nV1 != nVertexRemove && tri.nV2 != nVertexRemove )
				continue;

			if ( tri.nV1 == nVertexRemove )
			{
				tri.nV1 = nVertexKeep;
			}
			if ( tri.nV2 == nVertexRemove )
			{
				tri.nV2 = nVertexKeep;
			}
			if ( tri.nV1 == tri.nV2 || tri.nV1 == nVertex || tri.nV2 == nVertex )
			{
				removeList.AddToTail(j);
			}
		}
		nTrianglesRemoved += removeList.Count();
		for ( int j = removeList.Count(); --j >= 0; )
		{
			m_vertList[nVertex].m_triangles.FastRemove(removeList[j]);
		}
		removeList.RemoveAll();
	}

	for ( int i = 0; i < vertIndex.Count(); i++ )
	{
		RemapEdge( nVertexRemove, vertIndex[i], nVertexKeep );
	}

	int nRemoveCount = 0;
	for ( int j = 0; j < m_vertList[nVertexRemove].m_triangles.Count(); j++ )
	{
		vertex_triangle_t &tri = m_vertList[nVertexRemove].m_triangles[j];
		if ( tri.nV1 == nVertexKeep || tri.nV2 == nVertexKeep )
		{
			nRemoveCount++;
			continue;
		}
		m_vertList[nVertexKeep].m_triangles.AddToTail( tri );
	}
	nTrianglesRemoved += nRemoveCount;
	// These triangles are all invalid now
	m_vertList[nVertexRemove].m_triangles.RemoveAll();

	Assert( (nTrianglesRemoved % 3) == 0 );
	// each triangle has 3 copies in the list (one at each vert) so the number of real triangles removed
	// is 1/3rd of the number removed from the vertex lists
	m_nTriangleCount -= (nTrianglesRemoved / 3);

	LerpVertex( GetVertex(nVertexKeep), GetVertex(nV0), GetVertex(nV1), flInterp, m_nVertexStrideFloats );
	m_nCollapseIndex++;
}


int CMeshVisit::CountTotalUsedVerts()
{
	int nVertCount = m_vertList.Count();
	int nMaxVertexIndex = m_vertList.Count();

	CUtlVector<int> used;
	used.SetCount( nMaxVertexIndex + 1 );
	used.FillWithValue( 0 );
	int nUsedCount = 0;

	for ( int i = 0; i < nVertCount; i++ )
	{
		int nTriCount = m_vertList[i].m_triangles.Count();
		if ( nTriCount )
		{
			if ( !used[i] )
			{
				used[i] = 1;
				nUsedCount++;
			}
		}
		for ( int j = 0; j < nTriCount; j++ )
		{
			if ( !used[m_vertList[i].m_triangles[j].nV1] )
			{
				used[m_vertList[i].m_triangles[j].nV1] = 1;
				nUsedCount++;
			}
			if ( !used[m_vertList[i].m_triangles[j].nV2] )
			{
				used[m_vertList[i].m_triangles[j].nV2] = 1;
				nUsedCount++;
			}
		}
	}
	return nUsedCount;
}

int CountUsedVerts( const uint32 *pIndexList, int nIndexCount, int nVertexCount )
{
	CUtlVector<uint8> used;
	used.SetCount(nVertexCount);
	for ( int i = 0; i < nVertexCount; i++ )
	{
		used[i] = 0;
	}
	int nUsedCount = 0;
	for ( int i = 0; i < nIndexCount; i++ )
	{
		int nIndex = pIndexList[i];
		if ( !used[nIndex] )
		{
			used[nIndex] = 1;
			nUsedCount++;
		}
	}
	return nUsedCount;
}

void CMeshVisit::BuildFromMesh( const CMesh &input )
{
	VPROF("InitFromSimpleMesh");

	// NOTE: This assumes that position is the FIRST float3 in the buffer
	int nPosOffset = input.FindFirstAttributeOffset( VERTEX_ELEMENT_POSITION );
	if ( nPosOffset != 0 )
	{
		return;
	}

	int nInputVertCount = input.m_nVertexCount;
	int nInputIndexCount = input.m_nIndexCount;
	int nInputTriangleCount = nInputIndexCount / 3;
	m_nTriangleCount = nInputTriangleCount;
	m_edgeHash.Init( nInputIndexCount * 2 );

	// reserve space for vertex tables
	m_vertList.SetCount( nInputVertCount );
	for ( int i = 0; i < nInputVertCount; i++ )
	{
		m_vertList[i].m_triangles.EnsureCapacity( 8 );
	}

	m_edgeList.EnsureCapacity( nInputTriangleCount * 3 * 2 );

	// check for degenerate triangles in debug builds
#if _DEBUG
	for ( int i = 0; i < nInputIndexCount; i += 3 )
	{
		if ( input.m_pIndices[i+0] == input.m_pIndices[i+1] || input.m_pIndices[i+0] == input.m_pIndices[i+2] || input.m_pIndices[i+1] == input.m_pIndices[i+2] )
		{
			// Found a degenerate triangle
			// use CleanMesh() to remove degenerates if you aren't sure if the mesh contains degenerates
			// The simplification code does not tolerate degenerate triangles
			Assert(0);
		}
	}
#endif

	for ( int i = 0; i < nInputIndexCount; i += 3 )
	{
		for ( int j = 0; j < 3; j++ )
		{
			int nV0 = input.m_pIndices[i+j];
			int nNext = (j+1)%3;
			int nV1 = input.m_pIndices[i+nNext];
			edge_hash_t tmp;
			tmp.m_nV0 = MIN(nV0, nV1);
			tmp.m_nV1 = MAX(nV0, nV1);
			uint nHashKey = VertHashKey(MIN(nV0, nV1), MAX(nV0, nV1));
			tmp.m_pSharedEdge = NULL;
			UtlHashFastHandle_t edgeHashIndex = m_edgeHash.Find( nHashKey, tmp );
			// new edge, initialize it
			if ( m_edgeHash.InvalidHandle() == edgeHashIndex )
			{
				int nEdgeIndex = m_edgeList.AddToTail();
				tmp.m_pSharedEdge = &m_edgeList[nEdgeIndex];
				tmp.m_pSharedEdge->InitVerts( tmp.m_nV0, tmp.m_nV1 );
				edgeHashIndex = m_edgeHash.Insert( nHashKey, tmp );
			}
			edge_hash_t &edgeHash = m_edgeHash.Element( edgeHashIndex );
			edgeHash.m_pSharedEdge->m_nReferences++;

			vertex_triangle_t tri;
			tri.nV1 = nV1;
			tri.nV2 = input.m_pIndices[ i + ((j+2)%3) ];
			m_vertList[nV0].m_triangles.AddToTail( tri );
		}
	}


	m_nInputVertCount = input.m_nVertexCount;
	m_vertData.SetCount( input.m_nVertexCount * input.m_nVertexStrideFloats );
	m_pVertexBase = m_vertData.Base();
	m_nVertexStrideFloats = input.m_nVertexStrideFloats;
	V_memcpy( m_pVertexBase, input.GetVertex(0), input.GetTotalVertexSizeInBytes() );
	m_nCollapseIndex = 0;
}

// Removes elements from the queue until a valid one is found, returns that index or -1 indicating there are no valid edges
int CMeshVisit::FindMinErrorEdge()
{
	VPROF("FindMinErrorEdge");
	int nBest = -1;

	while ( true )
	{
		if ( !m_edgeQueue.Count() )
			return -1;

		edge_queue_entry_t entry = m_edgeQueue.ElementAtHead();
		m_edgeQueue.RemoveAtHead();

		CQEMEdge &edge = m_edgeList[entry.m_nEdgeIndex];
		if ( edge.m_flCurrentError == entry.m_flError && !edge.IsCollapsed() )
		{
			if ( IsValidCollapse( entry.m_nEdgeIndex ) )
			{
				nBest = entry.m_nEdgeIndex;
				break;
			}
		}
	}

	return nBest;
}

void CMeshVisit::ComputeVertListError( float flOpenEdgePenalty, float flMinArea, float flMaxArea, const mesh_simplifyweights_t *pWeights )
{
	m_errorVert.SetCount( m_nInputVertCount );
	if ( pWeights )
	{
		// can we use all of the weights?  If not, don't use any of them
		if ( pWeights->m_nVertexCount != m_nInputVertCount )
		{
			pWeights = NULL;
		}
	}
	for ( int i = 0; i < m_nInputVertCount; i++ )
	{
		m_errorVert[i].SetToZero();
		for ( int j = 0; j < m_vertList[i].m_triangles.Count(); j++ )
		{
			int nV0 = i;
			int nV1 = m_vertList[i].m_triangles[j].nV1;
			int nV2 = m_vertList[i].m_triangles[j].nV2;
			if ( IsOpenEdge( nV0, nV1 ) )
			{
				Vector v0 = GetVertexPosition(nV0);
				Vector v1 = GetVertexPosition(nV1);
				Vector v2 = GetVertexPosition(nV2);
				Vector vNormal = CrossProduct( v2 - v0, v1 - v0 );
				Vector vPlaneNormal = CrossProduct( v1 - v0, vNormal );
				float flArea = 0.5f * vPlaneNormal.NormalizeInPlace() * flOpenEdgePenalty;
				CQuadricError errorThisEdge;
				errorThisEdge.InitFromPlane( vPlaneNormal, -DotProduct(vPlaneNormal, v0), flArea );
				m_errorVert[i] += errorThisEdge;
				continue;
			}
			CQuadricError errorThisTri;
			errorThisTri.InitFromTriangle( GetVertexPosition(nV0), GetVertexPosition(nV1), GetVertexPosition(nV2), flMinArea );
			m_errorVert[i] += errorThisTri;
		}
		if ( pWeights && pWeights->m_pVertexWeights )
		{
			m_errorVert[i] *= pWeights->m_pVertexWeights[i];
		}
	}

	int nInputEdgeCount = m_edgeList.Count();
	for ( int i = 0; i < nInputEdgeCount; i++ )
	{
		UpdateEdgeError( &m_edgeList[i] );
	}
}

void GetOpenEdges( CUtlVector<Vector> &list, const CMesh &input )
{
	CMeshVisit visit;
	visit.BuildFromMesh( input );

	for ( int i = 0; i < visit.m_edgeList.Count(); i++ )
	{
		if ( visit.m_edgeList[i].m_nReferences < 2 )
		{
			list.AddToTail( visit.GetVertexPosition( visit.m_edgeList[i].m_nVert[0] ) );
			list.AddToTail( visit.GetVertexPosition( visit.m_edgeList[i].m_nVert[1] ) );
		}
	}
}

void SimplifyMeshQEM2( CMesh &meshOut, const CMesh &input, const mesh_simplifyparams_t &params, const mesh_simplifyweights_t *pWeights )
{
	VPROF("Simplify");
	CMeshVisit visit;
	visit.BuildFromMesh( input );
	CUtlVector<CQuadricError> errorEdge;

	int nInputVertCount = input.m_nVertexCount;

	int nInputEdgeCount = visit.m_edgeList.Count();
	errorEdge.SetCount( nInputEdgeCount );
	visit.m_flIntegrationPenalty = params.m_flIntegrationPenalty;
	visit.ComputeVertListError( params.m_flOpenEdgePenalty, 0.0f, 1.0f, pWeights );

	int nMinEdge = visit.FindMinErrorEdge();
	int nVertexCurrent = CountUsedVerts( input.m_pIndices, input.m_nIndexCount, input.m_nVertexCount );

	if ( nMinEdge >= 0 )
	{
		float flMinError = visit.m_edgeList[nMinEdge].m_flCurrentError;
		while ( flMinError < params.m_flMaxError || nVertexCurrent > params.m_nMaxVertexCount || visit.m_nTriangleCount > params.m_nMaxTriangleCount )
		{
			visit.CollapseEdge( nMinEdge );
			nVertexCurrent--;
			// don't collapse to anything two dimensional
			if ( nVertexCurrent < 5 )
				return;

			nMinEdge = visit.FindMinErrorEdge();
			if ( nMinEdge < 0 )
				break;
			flMinError = visit.m_edgeList[nMinEdge].m_flCurrentError;
		}
	}

	Vprof_MarkFrame_IfEnabled();

	CUtlVector<uint32> indexOut;
	visit.WriteMeshIndexList( indexOut );

	int nOutputIndexCount = indexOut.Count();
	const uint32 nInvalidIndex = uint32(-1);
	CUtlVector<uint32> nIndexMap;
	nIndexMap.SetCount( nInputVertCount );
	nIndexMap.FillWithValue( nInvalidIndex );

	int nOutputVertexCount = 0;
	for ( int i = 0; i < nOutputIndexCount; i++ )
	{
		int nIndex = indexOut[i];
		if ( nIndexMap[nIndex] == nInvalidIndex )
		{
			nIndexMap[nIndex] = nOutputVertexCount;
			nOutputVertexCount++;
		}
		indexOut[i] = nIndexMap[nIndex];
	}

	meshOut.AllocateMesh( nOutputVertexCount, nOutputIndexCount, input.m_nVertexStrideFloats, input.m_pAttributes, input.m_nAttributeCount );
	for ( int i = 0; i < nOutputIndexCount; i++ )
	{
		meshOut.m_pIndices[i] = indexOut[i];
	}
	for ( int i = 0; i < nInputVertCount; i++ )
	{
		if ( nIndexMap[i] != nInvalidIndex )
		{
			V_memcpy( meshOut.GetVertex(nIndexMap[i]), visit.GetVertex(i), meshOut.m_nVertexStrideFloats * sizeof(float) );
		}
	}
#if _DEBUG
	int nDeltaIndex = input.m_nIndexCount - indexOut.Count();
	Msg("Simplified.  Removed %d triangles (now %d was %d) (now %d verts, was %d)\n", nDeltaIndex / 3, indexOut.Count() / 3, input.m_nIndexCount / 3, nOutputVertexCount, nInputVertCount );
#endif
}

void SimplifyMesh( CMesh &meshOut, const CMesh &input, const mesh_simplifyparams_t &params, const mesh_simplifyweights_t *pWeights )
{
	Vprof_Start_IfEnabled();
	SimplifyMeshQEM2( meshOut, input, params, pWeights);

	Vprof_Report_IfEnabled();
}

