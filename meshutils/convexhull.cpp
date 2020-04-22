//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================
#include "mesh.h"
#include "tier1/mempool.h"
#include "utlintrusivelist.h"
#include "mathlib/ssemath.h"

class CHullTri;
static int g_Mod3Table[] = { 0, 1, 2, 0, 1, 2 };

int SupportPoint( const Vector &vDirection, const float *pVerts, int nVertCount, uint32 nVertexStrideFloats )
{
	if ( nVertCount < 2 )
		return 0;
	const float *pDirection = vDirection.Base();
	float flMax = DotProduct( pDirection, pVerts );
	int nMax = 0;
	pVerts += nVertexStrideFloats;
	for ( int i = 1; i < nVertCount; i++ )
	{
		float flDot = DotProduct( pDirection, pVerts );
		if ( flDot > flMax )
		{
			flMax = flDot;
			nMax = i;
		}
		pVerts += nVertexStrideFloats;
	}

	return nMax;
}


// Class half-edge data structure.  Stores neighboring information per triangle edge
// so it is possible to walk adjacent parts of the mesh
class CHullHalfEdge
{
public:
	CHullHalfEdge()
	{
		m_pTri = NULL;
		m_pNext = NULL;
		m_pPrev = NULL;
		m_pOpposite = NULL;
	}
	void SetNext( CHullHalfEdge *pNext )
	{
		pNext->m_pPrev = this;
		m_pNext = pNext;
	}
	void GetVerts( int nVerts[2] );
	int GetStartVert();
	int GetIndex();

	CHullTri *m_pTri;			// triangle containing these edges
	CHullHalfEdge *m_pNext;		// next edge on this tri in clockwise order
	CHullHalfEdge *m_pPrev;		// previous edge
	CHullHalfEdge *m_pOpposite;	// the other half edge on the other triangle that shares this edge
};

// each triangle on the hull or potentially on the hull is one of these
class CHullTri
{
public:
	// compute and store the plane of this triangle for later computation
	void Init( int nV0, int nV1, int nV2, const float *pVerts, int nVertexStrideFloats ) 
	{ 
		// save vertex indices
		m_nIndex[0] = nV0; 
		m_nIndex[1] = nV1; 
		m_nIndex[2] = nV2;

		// compute plane normal and plane constant
		Vector vEdge0 = *(Vector *)(pVerts + nV1 * nVertexStrideFloats ) - *(Vector *)(pVerts + nV0 * nVertexStrideFloats );
		Vector vEdge1 = *(Vector *)(pVerts + nV2 * nVertexStrideFloats ) - *(Vector *)(pVerts + nV0 * nVertexStrideFloats );
		m_vNormal = CrossProduct( vEdge1, vEdge0 );
		VectorNormalize( m_vNormal );
		m_flDist = DotProduct( pVerts + nV0 * nVertexStrideFloats, m_vNormal.Base() );

		// Part of the list of vertices in front of the plane
		m_flMaxDist = 0;
		m_pMaxVert = NULL;
		
		// mailbox for mesh walking
		m_nVisitCount = 0;

		// setup half edges
		for ( int i = 0; i < 3; i++ )
		{
			m_edges[i].m_pTri = this;
			m_edges[i].SetNext( &m_edges[g_Mod3Table[(i+1)]] );
		}
	}

	// distance from a vert to the triangle's plane
	float VertDist( const float *pVert )
	{
		float flDist = DotProduct( pVert, m_vNormal.Base() ) - m_flDist;
		return flDist;
	}

	// link half edges to neighboring triangles
	// NOTE: does the backlinks too, which results in doing that work twice, optimize?
	void SetNeighbors( CHullHalfEdge *p0, CHullHalfEdge *p1, CHullHalfEdge *p2 )
	{
		m_edges[0].m_pOpposite = p0;
		p0->m_pOpposite = &m_edges[0];

		m_edges[1].m_pOpposite = p1;
		p1->m_pOpposite = &m_edges[1];

		m_edges[2].m_pOpposite = p2;
		p2->m_pOpposite = &m_edges[2];
	}

	// debug, check validity / consistency.  I used this to find bugs when relinking the mesh
	bool IsValid()
	{
		if ( m_edges[0].m_pNext != &m_edges[1] || m_edges[0].m_pPrev != &m_edges[2] || 
			m_edges[1].m_pNext != &m_edges[2] || m_edges[1].m_pPrev != &m_edges[0] ||
			m_edges[2].m_pNext != &m_edges[0] || m_edges[2].m_pPrev != &m_edges[1] )
			return false;
		if ( m_edges[0].m_pTri != this || m_edges[1].m_pTri != this || m_edges[2].m_pTri != this )
			return false;
		if ( m_edges[0].m_pOpposite->m_pOpposite != &m_edges[0] ||
			m_edges[1].m_pOpposite->m_pOpposite != &m_edges[1] ||
			m_edges[2].m_pOpposite->m_pOpposite != &m_edges[2] )
			return false;
		return true;
	}

	// simple accessor
	inline CHullHalfEdge *GetEdge( int nIndex ) { return &m_edges[nIndex]; }

	CUtlVector<const float *> m_pVerts;
	CHullHalfEdge m_edges[3];
	Vector m_vNormal;
	float m_flDist;
	int m_nIndex[3];
	int m_nVisitCount;
	const float *m_pMaxVert;
	float m_flMaxDist;
	CHullTri *m_pNext;
	CHullTri *m_pPrev;
};

// gets the verts from an edge
void CHullHalfEdge::GetVerts( int nVerts[2] )
{
	int nIndex = this - m_pTri->m_edges;
	Assert(nIndex>=0 && nIndex<ARRAYSIZE(m_pTri->m_edges));
	nVerts[0] = m_pTri->m_nIndex[nIndex];
	nVerts[1] = m_pTri->m_nIndex[ g_Mod3Table[nIndex+1] ];
}

// edge to edge index (0,1,2)
int CHullHalfEdge::GetIndex()
{
	int nIndex = this - m_pTri->m_edges;
	Assert(nIndex>=0 && nIndex<ARRAYSIZE(m_pTri->m_edges));
	return nIndex;
}

int CHullHalfEdge::GetStartVert()
{
	return m_pTri->m_nIndex[GetIndex()];
}


// instantiate one of these to build convex hulls and fit OBBs
class CConvexHullBuilder
{
public:
	CConvexHullBuilder( const float *pVerts, int nVertexCount, uint32 nVertexStrideFloats, float flFrontEpsilon ) 
		: m_triPool(128), m_nCurrentVisit(0), m_pVerts(pVerts), m_nVertexStrideFloats(nVertexStrideFloats), m_nVertexCount(nVertexCount), m_flCoplanarEpsilon(flFrontEpsilon) {}

	void BuildHull();
	// tries all faces
	void FitOBB( matrix3x4_t &xform, Vector &vExtents );
	// tries only the face with minimum extents of hull along normal
	void FitOBBFast( matrix3x4_t &xform, Vector &vExtents );
	void GenerateOutputMesh( CMesh *pOutputMesh );

	bool IsValid()
	{
		for ( CHullTri *pTri = m_faceListVerts.Head(); pTri != NULL; pTri = pTri->m_pNext )
		{
			if ( !pTri->IsValid() )
				return false;
			if ( pTri->m_pNext && pTri->m_pNext->m_pPrev != pTri )
				return false;
			if ( pTri->m_pPrev && pTri->m_pPrev->m_pNext != pTri )
				return false;
			if ( pTri->m_pMaxVert == NULL || pTri->m_pVerts.Count() <= 0 )
				return false;
		}
		for ( CHullTri *pTri = m_faceListNoVerts.Head(); pTri != NULL; pTri = pTri->m_pNext )
		{
			if ( !pTri->IsValid() )
				return false;
			if ( pTri->m_pNext && pTri->m_pNext->m_pPrev != pTri )
				return false;
			if ( pTri->m_pPrev && pTri->m_pPrev->m_pNext != pTri )
				return false;
			if ( pTri->m_pMaxVert != NULL || pTri->m_pVerts.Count() > 0 )
				return false;
		}
		return true;
	}

private:
	void BuildInitialTetrahedron();
	void BuildHorizonList( CHullTri *pTri, const float *pVert );
	void BuildHorizonList_r( CHullTri *pTri, const float *pVert, int nEdgeStart );
	void BuildSilhouette_r( CHullTri *pTri, const Vector &vNormal, int nEdgeStart );
	void BuildSilhouette( CHullTri *pTri, const Vector &vNormal );
	void TransferVerts( CHullTri *pRemove, CHullTri *pNewTri );
	float SupportExtents( const Vector &vDirection, float *pMin, float *pMax ) const;
	float SupportExtents_Silhouette( const Vector &vDirection, float *pMin = NULL, float *pMax = NULL );
	void FitOBBToFace( CHullTri *pTri, matrix3x4_t &xform, Vector &vExtents );

	inline const float *GetVertex( int nIndex ) { return m_pVerts + nIndex * m_nVertexStrideFloats; }
	inline const Vector *GetVertexPosition( int nIndex ) { return (const Vector *)(m_pVerts + nIndex * m_nVertexStrideFloats); }
	void NextVisit()
	{
		m_nCurrentVisit++;
	}

	void AddTriangle( CHullTri *pTri )
	{
		if ( pTri->m_pMaxVert )
		{
			// add the new faces to the tail since they are likely to have fewer verts in front than faces already in the list
			m_faceListVerts.AddToTail( pTri );
		}
		else
		{
			m_faceListNoVerts.AddToHead( pTri );
		}
	}

	void RemoveTriangle( CHullTri *pTri )
	{
		if ( pTri->m_pMaxVert )
		{
			m_faceListVerts.RemoveNode( pTri );
		}
		else
		{
			m_faceListNoVerts.RemoveNode( pTri );
		}
	}

	bool Visit( CHullTri *pTri )
	{
		if ( pTri->m_nVisitCount != m_nCurrentVisit )
		{
			pTri->m_nVisitCount = m_nCurrentVisit;
			return true;
		}
		return false;
	}

	CUtlIntrusiveDListWithTailPtr<CHullTri> m_faceListVerts;
	CUtlIntrusiveDList<CHullTri> m_faceListNoVerts;

	CClassMemoryPool<CHullTri> m_triPool;
	CUtlVectorFixedGrowable<CHullHalfEdge *, 256> m_halfEdgeList;
	CUtlVectorFixedGrowable<CHullTri *, 256> m_removeList;
	CUtlVectorFixedGrowable<const float *, 256> m_silhouetteVertexList;
	const float *m_pVerts;
	float m_flCoplanarEpsilon;
	uint32 m_nVertexStrideFloats;
	int m_nVertexCount;
	int m_nCurrentVisit;
};

// Builds a tetradhedron to start the convex hull
// This does an iteration like GJK - could use bounding box or some other method
// BUGBUG: Test this with 2D mesh for failure cases
void CConvexHullBuilder::BuildInitialTetrahedron()
{
	Vector vDir(0,0,1);
	int i0 = SupportPoint( vDir, m_pVerts, m_nVertexCount, m_nVertexStrideFloats );
	int i1 = SupportPoint( -vDir, m_pVerts, m_nVertexCount, m_nVertexStrideFloats );
	Vector vEdge0 = *GetVertexPosition(i1) - *GetVertexPosition(i0);
	Vector vNormal = CrossProduct( *GetVertexPosition(i0), vEdge0 );
	if ( vNormal.LengthSqr() < 1e-3f )
	{
		vNormal.x = 1.0f;
	}
	int i2 = SupportPoint( vNormal, m_pVerts, m_nVertexCount, m_nVertexStrideFloats );
	if ( i2 == i0 || i2 == i1 )
	{
		i2 = SupportPoint( -vNormal, m_pVerts, m_nVertexCount, m_nVertexStrideFloats );
	}

	Vector vEdge1 = *GetVertexPosition(i2) - *GetVertexPosition(i0);
	vNormal = CrossProduct( vEdge1, vEdge0 );
	int i3 = SupportPoint( -vNormal, m_pVerts, m_nVertexCount, m_nVertexStrideFloats );


	CHullTri *pTri0 = m_triPool.Alloc();
	pTri0->Init( i0, i1, i2, m_pVerts, m_nVertexStrideFloats );

	CHullTri *pTri1 = m_triPool.Alloc();
	pTri1->Init( i0, i3, i1, m_pVerts, m_nVertexStrideFloats );

	CHullTri *pTri2 = m_triPool.Alloc();
	pTri2->Init( i1, i3, i2, m_pVerts, m_nVertexStrideFloats );

	CHullTri *pTri3 = m_triPool.Alloc();
	pTri3->Init( i2, i3, i0, m_pVerts, m_nVertexStrideFloats );

	m_faceListVerts.AddToTail(pTri0);
	m_faceListVerts.AddToTail(pTri1);
	m_faceListVerts.AddToTail(pTri2);
	m_faceListVerts.AddToTail(pTri3);

	pTri0->SetNeighbors( pTri1->GetEdge(2), pTri2->GetEdge(2), pTri3->GetEdge(2) );
	pTri1->SetNeighbors( pTri3->GetEdge(1), pTri2->GetEdge(0), pTri0->GetEdge(0) );
	pTri2->SetNeighbors( pTri1->GetEdge(1), pTri3->GetEdge(0), pTri0->GetEdge(1) );
	pTri3->SetNeighbors( pTri2->GetEdge(1), pTri1->GetEdge(0), pTri0->GetEdge(2) );
	const float flFrontDist = m_flCoplanarEpsilon;
	for ( int i = 0; i < m_nVertexCount; i++ )
	{
		for ( CHullTri *pTri = m_faceListVerts.Head(); pTri != NULL; pTri = pTri->m_pNext )
		{
			const float *pVert = GetVertex(i);
			float flDist = pTri->VertDist( pVert );

			if ( flDist > flFrontDist )
			{
				pTri->m_pVerts.AddToTail( pVert );
				if ( flDist > pTri->m_flMaxDist )
				{
					pTri->m_flMaxDist = flDist;
					pTri->m_pMaxVert = pVert;
				}
				break;
			}
		}
	}

	for ( CHullTri *pNext = NULL, *pTri = m_faceListVerts.Head(); pTri != NULL; pTri = pNext )
	{
		pNext = pTri->m_pNext;
		if ( !pTri->m_pMaxVert )
		{
			m_faceListVerts.RemoveNode( pTri );
			m_faceListNoVerts.AddToTail( pTri );
		}
	}
}

// Recursively visit neighboring triangles to produce the list of edges that are silhouettes with respect
// to the new vertex.
// IMPORTANT NOTE: This recursion is carefully designed to visit the boundary edge list exactly in order
// Once it reaches the boundary it will form an edge loop in order in the m_halfEdgeList array
// If this array is not in order the half-edges will not be linked up properly in BuildHull
void CConvexHullBuilder::BuildHorizonList_r( CHullTri *pTri, const float *pVert, int nEdgeStart )
{
	RemoveTriangle( pTri );
	for ( int i = 0; i < 3; i++ )
	{
		// this visits the neighbors in clockwise order starting with the edge you came in on
		CHullHalfEdge *pNeighborEdge = pTri->GetEdge( g_Mod3Table[( i + nEdgeStart)])->m_pOpposite;
		CHullTri *pNeighbor = pNeighborEdge->m_pTri;
		bool bRecurse = Visit( pNeighbor );
		if ( pNeighbor->VertDist(pVert) > 0 )
		{
			if ( bRecurse )
			{
				BuildHorizonList_r( pNeighbor, pVert, pNeighborEdge->GetIndex() );
			}
		}
		else
		{
			// This edge has a neighbor that has the new vert in front (so removed) but it's triangle
			// is not in front.  This is a silhouette edge that will remain on the hull, but needs to 
			// be relinked to a new triangle
			m_halfEdgeList.AddToTail( pNeighborEdge );
		}
	}
	m_removeList.AddToTail( pTri );
}

// Starts a new recursion to found the silhouette edge loop
void CConvexHullBuilder::BuildHorizonList( CHullTri *pTri, const float *pVert )
{
	// clear out any list - we're going to write this
	m_halfEdgeList.RemoveAll();
	// advance the token we use to avoid visiting triangles twice
	NextVisit();
	// mark the starting triangle as visited so we don't re-enter it
	Visit( pTri );
	BuildHorizonList_r( pTri, pVert, 0 );
}

void CConvexHullBuilder::BuildSilhouette_r( CHullTri *pTri, const Vector &vNormal, int nEdgeStart )
{
	for ( int i = 0; i < 3; i++ )
	{
		// this visits the neighbors in clockwise order starting with the edge you came in on
		CHullHalfEdge *pNeighborEdge = pTri->GetEdge( g_Mod3Table[( i + nEdgeStart)])->m_pOpposite;
		CHullTri *pNeighbor = pNeighborEdge->m_pTri;
		bool bRecurse = Visit( pNeighbor );
		float flDot = DotProduct( vNormal, pNeighbor->m_vNormal );
		if ( flDot > 0 )
		{
			if ( bRecurse )
			{
				BuildSilhouette_r( pNeighbor, vNormal, pNeighborEdge->GetIndex() );
			}
		}
		else
		{
			// add edge to sil
			m_silhouetteVertexList.AddToTail( GetVertex( pNeighborEdge->m_pNext->GetStartVert() ) );
		}
	}
}

// Starts a new recursion to found the silhouette edge loop
void CConvexHullBuilder::BuildSilhouette( CHullTri *pTri, const Vector &vNormal )
{
	// clear out any list - we're going to write this
	m_silhouetteVertexList.RemoveAll();
	// advance the token we use to avoid visiting triangles twice
	NextVisit();
	// mark the starting triangle as visited so we don't re-enter it
	Visit( pTri );
	BuildSilhouette_r( pTri, vNormal, 0 );
}


// transfer any verts in the front list of pRemove to the front list of pNewTri if they
// are in front of pNewTri
// Remove them from the original list so they won't get transferred to subsequent triangles
void CConvexHullBuilder::TransferVerts( CHullTri *pRemove, CHullTri *pNewTri )
{
	int nVertex = 0;
	const float flFrontDist = m_flCoplanarEpsilon;
	while ( nVertex < pRemove->m_pVerts.Count() )
	{
		const float *pCheckVert = pRemove->m_pVerts[nVertex];
		if ( pCheckVert == pRemove->m_pMaxVert )
		{
			pRemove->m_pVerts.FastRemove(nVertex);
			continue;
		}
		float flDist = pNewTri->VertDist(pCheckVert);
		if ( flDist > flFrontDist )
		{
			pNewTri->m_pVerts.AddToTail( pCheckVert );
			if ( flDist > pNewTri->m_flMaxDist )
			{
				pNewTri->m_flMaxDist = flDist;
				pNewTri->m_pMaxVert = pCheckVert;
			}
			pRemove->m_pVerts.FastRemove(nVertex);
		}
		else
		{
			nVertex++;
		}
	}
}

float CConvexHullBuilder::SupportExtents( const Vector &vDirection, float *pMin, float *pMax ) const
{
	const float *pVerts = m_pVerts;
	const float *pDirection = vDirection.Base();
	float flMax = DotProduct( pDirection, pVerts );
	float flMin = flMax;
	const uint32 nStride = m_nVertexStrideFloats;
	pVerts += nStride;
	for ( int i = 1; i < m_nVertexCount; i++ )
	{
		float flDot = DotProduct( pDirection, pVerts );
		flMax = MAX(flMax, flDot);
		flMin = MIN(flMin, flDot);
		pVerts += nStride;
	}

	if ( pMin )
	{
		*pMin = flMin;
	}
	if ( pMax )
	{
		*pMax = flMax;
	}
	return flMax - flMin;
}

// UNDONE: Only look at half edge verts
float CConvexHullBuilder::SupportExtents_Silhouette( const Vector &vDirection, float *pMin, float *pMax )
{
	if ( !m_silhouetteVertexList.Count() )
		return 0;

	const float *pDirection = vDirection.Base();
	float flMax = DotProduct( pDirection, m_silhouetteVertexList[0] );
	float flMin = flMax;
	int nVertCount = m_silhouetteVertexList.Count();
	for ( int i = 1; i < nVertCount; i++ )
	{
		float flDot = DotProduct( pDirection, m_silhouetteVertexList[i] );
		flMax = MAX(flMax, flDot);
		flMin = MIN(flMin, flDot);
	}
	if ( pMin )
	{
		*pMin = flMin;
	}
	if ( pMax )
	{
		*pMax = flMax;
	}

	return flMax - flMin;
}

// Construct the triangulated convex hull from the point set
void CConvexHullBuilder::BuildHull()
{
	// get a tetrahedron for this mesh and build a list of verts for each face
	BuildInitialTetrahedron();

	// process each face that has vertices in front of it until all are done
	while ( m_faceListVerts.Head() != NULL )
	{
		CHullTri *pTri = m_faceListVerts.Head();

		const float *pNewVert = pTri->m_pMaxVert;
		int nNewVertIndex = (pNewVert - m_pVerts) / m_nVertexStrideFloats;
		// Get the horizon edge loop of the new vert
		BuildHorizonList( pTri, pNewVert );

		// now build a new triangle along each horizon edge and the new vert
		int nEdgeVerts[2];
		CUtlVectorFixedGrowable<CHullTri *, 64> addedList;
		int nLastEdgeVert = -1;
		for ( int i = 0; i < m_halfEdgeList.Count(); i++ )
		{
			CHullHalfEdge *pEdge = m_halfEdgeList[i];
			pEdge->GetVerts( nEdgeVerts );
			CHullTri *pNewTri = m_triPool.Alloc();
			if ( nLastEdgeVert >= 0 )
			{
				Assert(nEdgeVerts[1]==nLastEdgeVert);
			}
			nLastEdgeVert = nEdgeVerts[0];
			pNewTri->Init( nNewVertIndex, nEdgeVerts[1], nEdgeVerts[0], m_pVerts, m_nVertexStrideFloats );

			// now transfer any verts in front of the new triangle 
			for ( int j = 0; j < m_removeList.Count(); j++ )
			{
				TransferVerts( m_removeList[j], pNewTri );
			}
			addedList.AddToTail( pNewTri );

			// put this triangle in the list of hull triangles or the list of triangles to be expanded
			AddTriangle(pNewTri);
		}

		// now free the removed triangles (we had to keep them until their verts were transferred)
		for ( int j = 0; j < m_removeList.Count(); j++ )
		{
			m_triPool.Free( m_removeList[j] );
		}
		m_removeList.RemoveAll();

		// now link up the halfEdges between the new triangles and the hull mesh
		// NOTE: This depends on m_halfEdgeList being an in-order loop which is guaranteed by
		// the recursion order in BuildHorizon
		int nNewTriangleCount = addedList.Count();
		CHullTri *pPrev = addedList[ nNewTriangleCount-1 ];
		for ( int i = 0; i < nNewTriangleCount; i++ )
		{
			CHullTri *pNext = addedList[ (i + 1) % nNewTriangleCount ];
			addedList[i]->SetNeighbors( pPrev->GetEdge(2), m_halfEdgeList[i], pNext->GetEdge(0) );
			pPrev = addedList[i];
		}
	}

	Assert(m_faceListVerts.Head()==NULL);
}

// write all of the faces on the hull to an output mesh 
void CConvexHullBuilder::GenerateOutputMesh( CMesh *pOutMesh )
{
	CUtlVector<int> vertexMap;
	vertexMap.SetCount( m_nVertexCount );
	vertexMap.FillWithValue( -1 );
	int nOutVert = 0;
	int nOutTri = 0;
	for ( CHullTri *pTri = m_faceListNoVerts.Head(); pTri != NULL; pTri = pTri->m_pNext )
	{
		for ( int j = 0; j < 3; j++ )
		{
			int nIndex = pTri->m_nIndex[j];
			if ( vertexMap[nIndex] < 0 )
			{
				vertexMap[nIndex] = nOutVert++;
			}
		}
		nOutTri++;
	}
	vertexMap.FillWithValue(-1);
	pOutMesh->AllocateMesh( nOutVert, nOutTri * 3, m_nVertexStrideFloats, NULL, 0 );
	nOutVert = 0;
	nOutTri = 0;
	for ( CHullTri *pTri = m_faceListNoVerts.Head(); pTri != NULL; pTri = pTri->m_pNext )
	{
		for ( int j = 0; j < 3; j++ )
		{
			int nIndex = pTri->m_nIndex[j];
			if ( vertexMap[nIndex] < 0 )
			{
				CopyVertex( pOutMesh->GetVertex(nOutVert), GetVertex(nIndex), m_nVertexStrideFloats );
				vertexMap[nIndex] = nOutVert++;
			}
			pOutMesh->m_pIndices[nOutTri*3+j] = vertexMap[nIndex];
		}
		nOutTri++;
	}
}

void CConvexHullBuilder::FitOBBToFace( CHullTri *pTri, matrix3x4_t &xform, Vector &vExtents )
{
	BuildSilhouette( pTri, pTri->m_vNormal );
	int nEdgeCount = m_silhouetteVertexList.Count();
	int nMinEdge = 0;
	float flMinPerimeter = 1e24f;
	for ( int i = 0; i < nEdgeCount; i++ )
	{
		int nNext = (i+1)%nEdgeCount;
		Vector v0 = *(Vector *)m_silhouetteVertexList[i];
		Vector v1 = *(Vector *)m_silhouetteVertexList[nNext];
		Vector vEdge = v1 - v0;
		Vector vNormalY = CrossProduct( pTri->m_vNormal, vEdge );
		VectorNormalize( vNormalY );
		float flExtentTestY = SupportExtents_Silhouette( vNormalY );
		Vector vNormalZ = CrossProduct( pTri->m_vNormal, vNormalY );
		float flExtentTestZ = SupportExtents_Silhouette( vNormalZ );
		float flPerim = flExtentTestZ + flExtentTestY;
		if ( flPerim < flMinPerimeter )
		{
			flMinPerimeter = flPerim;
			nMinEdge = i;
		}
	}

	Vector vNormalX = pTri->m_vNormal;
	int nNext = (nMinEdge+1)%nEdgeCount;
	Vector vEdge = *(Vector *)m_silhouetteVertexList[nNext] - *(Vector *)m_silhouetteVertexList[nMinEdge];
	Vector vNormalY = CrossProduct( pTri->m_vNormal, vEdge );
	VectorNormalize( vNormalY );
	Vector vNormalZ = CrossProduct( pTri->m_vNormal, vNormalY );
	VectorNormalize( vNormalZ );
	MatrixSetColumn( vNormalX, 0, xform );
	MatrixSetColumn( vNormalY, 1, xform );
	MatrixSetColumn( vNormalZ, 2, xform );

	Vector vMins, vMaxs, vCenter;
	SupportExtents( pTri->m_vNormal, &vMins.x, &vMaxs.x );
	SupportExtents_Silhouette( vNormalY, &vMins.y, &vMaxs.y );
	SupportExtents_Silhouette( vNormalZ, &vMins.z, &vMaxs.z );
	vCenter = 0.5f * (vMins + vMaxs);
	vExtents = vMaxs - vCenter;
	vCenter = vNormalX * vCenter.x + vNormalY * vCenter.y + vNormalZ * vCenter.z;
	MatrixSetColumn( vCenter, 3, xform );

}


void CConvexHullBuilder::FitOBBFast( matrix3x4_t &xform, Vector &vExtents )
{
	CHullTri *pTri = m_faceListNoVerts.Head();

	if ( !pTri )
		return;

	CHullTri *pMinTri = pTri;
	float flMinX = 0, flMaxX = 0;
	float flExtentX = SupportExtents( pTri->m_vNormal, &flMinX, &flMaxX );
	pTri = pTri->m_pNext;

	for ( ; pTri != NULL; pTri = pTri->m_pNext )
	{
		float flMinTest = 0, flMaxTest = 0;
		float flExtentTest = SupportExtents( pTri->m_vNormal, &flMinTest, &flMaxTest );
		if ( flExtentTest < flExtentX )
		{
			flExtentX = flExtentTest;
			flMinX = flMinTest;
			flMaxX = flMaxTest;
			pMinTri = pTri;
		}
	}
	FitOBBToFace( pMinTri, xform, vExtents );
}

void CConvexHullBuilder::FitOBB( matrix3x4_t &xform, Vector &vExtents )
{
	// try all faces, return minimum surface area box
	CHullTri *pMinTri = NULL;
	Vector vTmpExtents;
	matrix3x4_t tmpXform;
	float flMinSurfacearea = 1e24;
	for ( CHullTri *pTri = m_faceListNoVerts.Head(); pTri != NULL; pTri = pTri->m_pNext )
	{
		FitOBBToFace( pTri, tmpXform, vTmpExtents );
		float flSurfaceArea = vTmpExtents.x * vTmpExtents.y + vTmpExtents.x * vTmpExtents.z + vTmpExtents.y * vTmpExtents.z;
		if ( flSurfaceArea < flMinSurfacearea )
		{
			flMinSurfacearea = flSurfaceArea;
			pMinTri = pTri;
		}
	}
	FitOBBToFace( pMinTri, xform, vExtents );
}

// returns the mesh containing the convex hull of the input mesh
void ConvexHull3D( CMesh *pOutMesh, const CMesh &inputMesh, float flCoplanarEpsilon )
{
	CConvexHullBuilder builder( inputMesh.m_pVerts, inputMesh.m_nVertexCount, inputMesh.m_nVertexStrideFloats, flCoplanarEpsilon );
	builder.BuildHull();
	builder.GenerateOutputMesh( pOutMesh );
}

void FitOBBToMesh( matrix3x4_t *pCenter, Vector *pExtents, const CMesh &inputMesh, float flCoplanarEpsilon )
{
	CConvexHullBuilder builder( inputMesh.m_pVerts, inputMesh.m_nVertexCount, inputMesh.m_nVertexStrideFloats, flCoplanarEpsilon );
	builder.BuildHull();
	matrix3x4_t xform;
	Vector vExtents;
	builder.FitOBB( xform, vExtents );
	if ( pCenter && pExtents )
	{
		*pCenter = xform;
		*pExtents = vExtents;
	}
}


Vector CSGInsidePoint( VPlane *pPlanes, int planeCount )
{
	Vector point = vec3_origin;

	for ( int i = 0; i < planeCount; i++ )
	{
		float d = DotProduct( pPlanes[i].m_Normal, point ) - pPlanes[i].m_Dist;
		if ( d < 0 )
		{
			point -= d * pPlanes[i].m_Normal;
		}
	}
	return point;
}

fltx4 CSGInsidePoint_SIMD( fltx4 *pPlanes, int nPlaneCount )
{
	fltx4 point = Four_Zeros;

	// TODO: Depending of the number of planes that we have, it may be possible to actually calculate several planes at the same time.
	//	Have to see if the calculation is still correct when done in parallel (the offset may still be similar).
	//  Even with the current code complexity, it would be good to do 4 by 4 if we have at least 8 to 12 planes every time.
	for ( int i = 0; i < nPlaneCount; i++ )
	{
		fltx4 planeValue = pPlanes[i];

		// float d = DotProduct( pPlanes[i].m_Normal, point ) - pPlanes[i].m_Dist;
		fltx4 result = Dot3SIMD( planeValue, point );				// Fast on X360, not so fast on PC and PS3 - Still faster than FPU operation though
		result = SubSIMD( result, SplatWSIMD( planeValue ) );		// XYZW has the result of the dot product
		// if ( d < 0 )
		//	point -= d * pPlanes[i].m_Normal;
		fltx4 offset = MulSIMD( result, planeValue );				// XYZ has d * normal, and W as garbage
		bi32x4 mask = CmpLtSIMD( result, Four_Zeros );				// (d < 0) ? 0xffffffff : 0
		point = SubSIMD( point, MaskedAssign( mask, offset, Four_Zeros ) );	// point will have garbage W, but this has no impact
																			// (as we use Dot3 product).
	}
	point = SetWToZeroSIMD( point );								// Just in case
	return point;
}

void TranslatePlaneList( VPlane *pPlanes, int nPlaneCount, const Vector &offset )
{
	for ( int i = 0; i < nPlaneCount; i++ )
	{
		pPlanes[i].m_Dist += DotProduct( offset, pPlanes[i].m_Normal );
	}
}

void TranslatePlaneList_SIMD( fltx4 *pPlanes, int nPlaneCount, const fltx4 &offset )
{
	int i = 0;
	fltx4 f4WMask = LoadAlignedSIMD( g_SIMD_ComponentMask[3] );
	while ( nPlaneCount >= 4 )
	{
		fltx4 plane0 = pPlanes[i];
		fltx4 plane1 = pPlanes[i + 1];
		fltx4 plane2 = pPlanes[i + 2];
		fltx4 plane3 = pPlanes[i + 3];

		fltx4 dot0 = Dot3SIMD( offset, plane0 );
		fltx4 dot1 = Dot3SIMD( offset, plane1 );
		fltx4 dot2 = Dot3SIMD( offset, plane2 );
		fltx4 dot3 = Dot3SIMD( offset, plane3 );

		dot0 = AndSIMD( dot0, f4WMask );		// 0 0 0 Dot
		dot1 = AndSIMD( dot1, f4WMask );		// W contains Dist
		dot2 = AndSIMD( dot2, f4WMask );
		dot3 = AndSIMD( dot3, f4WMask );

		pPlanes[i] = AddSIMD( plane0, dot0 );
		pPlanes[i + 1] = AddSIMD( plane1, dot1 );
		pPlanes[i + 2] = AddSIMD( plane2, dot2 );
		pPlanes[i + 3] = AddSIMD( plane3, dot3 );

		nPlaneCount -= 4;
		i += 4;
	}
	while ( nPlaneCount > 0 )
	{
		fltx4 plane0 = pPlanes[i];
		fltx4 dot0 = Dot3SIMD( offset, plane0 );
		dot0 = AndSIMD( dot0, f4WMask );		// 0 0 0 Dot
		pPlanes[i] = AddSIMD( plane0, dot0 );
		++i;
		--nPlaneCount;
	}
}

void InvertPlanes( VPlane *pPlaneList, int nPlaneCount )
{
	for ( int i = 0; i < nPlaneCount; i++ )
	{
		pPlaneList[i].m_Normal *= -1;
		pPlaneList[i].m_Dist *= -1;
	}
}

void InvertPlanes_SIMD( fltx4 * pPlanes, int nPlaneCount )
{
	int i = 0;
	while ( nPlaneCount >= 4)
	{
		pPlanes[i] = -pPlanes[i];
		pPlanes[i + 1] = -pPlanes[i + 1];
		pPlanes[i + 2] = -pPlanes[i + 2];
		pPlanes[i + 3] = -pPlanes[i + 3];
		i += 4;
		nPlaneCount -= 4;
	}

	while ( nPlaneCount > 0 )
	{
		pPlanes[i] = -pPlanes[i];
		++i;
		--nPlaneCount;
	}
}

void CSGPlaneList( CUtlVector<Vector> &verts, CUtlVector<uint32> &index, CUtlVector<uint32> &trianglePlaneIndices, VPlane *pPlaneList, int nPlaneCount, float flCoplanarEpsilon )
{
	const int MAX_VERTS = 128;
	Vector	vertsIn[MAX_VERTS], vertsOut[MAX_VERTS];

	// compute a point inside the volume defined by these planes
	Vector insidePoint = CSGInsidePoint( pPlaneList, nPlaneCount );
	// move the planes so that the inside point is at the origin 
	// NOTE: This is to maximize precision for the CSG operations
	TranslatePlaneList( pPlaneList, nPlaneCount, -insidePoint );

	// Build the CSG solid of this leaf given that the planes in the list define a convex solid
	for ( int i = 0; i < nPlaneCount; ++i )
	{
		// Build a big-ass poly in this plane
		int vertCount = PolyFromPlane( vertsIn, pPlaneList[i].m_Normal, pPlaneList[i].m_Dist );

		// Now chop it by every other plane
		int j;
		for ( j = 0; j < nPlaneCount; ++j )
		{
			// don't clip planes with themselves
			if ( i == j )
				continue;

			// Less than a poly left, something is wrong, don't bother with this polygon
			if ( vertCount < 3 )
				continue;

			// Chop the polygon against this plane
			vertCount = ClipPolyToPlane( vertsIn, vertCount, vertsOut, pPlaneList[j].m_Normal, pPlaneList[j].m_Dist, flCoplanarEpsilon );

			// Just copy the verts each time, don't bother swapping pointers (efficiency is not a goal here)
			for ( int k = 0; k < vertCount; ++k )
			{
				VectorCopy( vertsOut[k], vertsIn[k] );
			}
		}

		// We've got a polygon here
		if ( vertCount >= 3 )
		{
			// Since this is a convex polygon, use the fan algorithm to create the triangles
			int baseVertex = verts.Count();
			for ( int j = 0; j < vertCount - 2; ++j )
			{
				trianglePlaneIndices.AddToTail(i);
				index.AddToTail( baseVertex );
				index.AddToTail( baseVertex + j + 1 );
				index.AddToTail( baseVertex + j + 2 );
			}

			// Copy verts
			for ( int j = 0; j < vertCount; ++j )
			{
				verts.AddToTail( vertsIn[j] + insidePoint );
			}
		}
	}
}

void CSGPlaneList_SIMD( CUtlVector<fltx4> &verts, CUtlVector<uint32> &index, CUtlVector<uint16> &trianglePlaneIndices, fltx4 *pPlaneList, int nPlaneCount, float flCoplanarEpsilon )
{
	const int MAX_VERTS = 128;
	fltx4	vertsIn[MAX_VERTS], vertsOut[MAX_VERTS];
#if _DEBUG
	Vector tempVertsIn[MAX_VERTS], tempVertOuts[MAX_VERTS];
#endif

	// compute a point inside the volume defined by these planes
	fltx4 insidePoint = CSGInsidePoint_SIMD( pPlaneList, nPlaneCount );

#if _DEBUG
	Vector vInsidePoint = CSGInsidePoint( (VPlane *)pPlaneList, nPlaneCount );
	Assert( vInsidePoint.x == SubFloat( insidePoint, 0 ) );
	Assert( vInsidePoint.y == SubFloat( insidePoint, 1 ) );
	Assert( vInsidePoint.z == SubFloat( insidePoint, 2 ) );
#endif

	// move the planes so that the inside point is at the origin 
	// NOTE: This is to maximize precision for the CSG operations
	TranslatePlaneList_SIMD( pPlaneList, nPlaneCount, -insidePoint );

	// Build the CSG solid of this leaf given that the planes in the list define a convex solid
	for ( int i = 0; i < nPlaneCount; ++i )
	{
		// Build a big-ass poly in this plane
		PolyFromPlane_SIMD( vertsIn, pPlaneList[i] );
		int nVertCount = 4;		// PolyFromPlane actually always return 4. Bake the result.

#if _DEBUG
		Vector normalI( SubFloat( pPlaneList[i], 0 ), SubFloat( pPlaneList[i], 1 ), SubFloat( pPlaneList[i], 2 ) );
		int nResult = PolyFromPlane( tempVertsIn, normalI, SubFloat( pPlaneList[i], 3 ) );
		Assert( nVertCount == nResult );
		for (int n = 0 ; n < nVertCount ; ++n )
		{
			Assert( tempVertsIn[n].x == SubFloat( vertsIn[n], 0 ) );
			Assert( tempVertsIn[n].y == SubFloat( vertsIn[n], 1 ) );
			Assert( tempVertsIn[n].z == SubFloat( vertsIn[n], 2 ) );
		}
#endif

		// Now chop it by every other plane
		int j;
		for ( j = 0; j < nPlaneCount; ++j )
		{
			// don't clip planes with themselves
			if ( i == j )
			{
				continue;
			}

			// Less than a poly left, something is wrong, don't bother with this polygon
			if ( nVertCount < 3 )
			{
				break;					// Or with any other polygon (as nVertCount would not be set in the loop).
			}

			// Chop the polygon against this plane
#if _DEBUG
			int nOldVertCount = nVertCount;
#endif
			nVertCount = ClipPolyToPlane_SIMD( vertsIn, nVertCount, vertsOut, pPlaneList[j], flCoplanarEpsilon );

#if _DEBUG
			Vector normal( SubFloat( pPlaneList[j], 0 ), SubFloat( pPlaneList[j], 1 ), SubFloat( pPlaneList[j], 2 ) );
			int nClipResult = ClipPolyToPlane( tempVertsIn, nOldVertCount, tempVertOuts, normal, SubFloat( pPlaneList[j], 3 ), flCoplanarEpsilon );
			Assert( nClipResult == nVertCount );
			for ( int n = 0 ; n < nVertCount ; ++n )
			{
				// In some cases, the SIMD algorithm does not return the exact same result as the fpu version.
				Assert( fabs( tempVertOuts[n].x - SubFloat( vertsOut[n], 0 ) ) < 0.001f );
				Assert( fabs( tempVertOuts[n].y - SubFloat( vertsOut[n], 1 ) ) < 0.001f );
				Assert( fabs( tempVertOuts[n].z - SubFloat( vertsOut[n], 2 ) ) < 0.001f );
			}
#endif

			// Just copy the verts each time, don't bother swapping pointers (efficiency is not a goal here) - Not anymore :)
			int nNumberOfVertices = nVertCount;
			int k = 0;
			while ( nNumberOfVertices >= 4)
			{
				vertsIn[k] = vertsOut[k];
				vertsIn[k + 1] = vertsOut[k + 1];
				vertsIn[k + 2] = vertsOut[k + 2];
				vertsIn[k + 3] = vertsOut[k + 3];
#if _DEBUG
				tempVertsIn[k] = tempVertOuts[k];
				tempVertsIn[k + 1] = tempVertOuts[k + 1];
				tempVertsIn[k + 2] = tempVertOuts[k + 2];
				tempVertsIn[k + 3] = tempVertOuts[k + 3];
#endif
				nNumberOfVertices -= 4;
				k += 4;
			}

			while ( nNumberOfVertices > 0 )
			{
				vertsIn[k] = vertsOut[k];
#if _DEBUG
				tempVertsIn[k] = tempVertOuts[k];
#endif
				--nNumberOfVertices;
				++k;
			}
		}

		// We've got a polygon here
		if ( nVertCount >= 3 )
		{
			// Since this is a convex polygon, use the fan algorithm to create the triangles
			const int NUMBER_OF_TRIANGLES = nVertCount - 2;
			int nDestTrianglePlaneIndex = trianglePlaneIndices.Count();
			int nDestIndex = index.Count();
			// Set the count once (will grow as needed), so we don't call AddTail() several times.
			trianglePlaneIndices.SetCountNonDestructively( nDestTrianglePlaneIndex + NUMBER_OF_TRIANGLES );
			index.SetCountNonDestructively( nDestIndex + ( 3 * NUMBER_OF_TRIANGLES ) );

			int nBaseVertex = verts.Count();
			for ( int j = 0; j < NUMBER_OF_TRIANGLES; ++j )
			{
				trianglePlaneIndices[nDestTrianglePlaneIndex] = i;
				index[nDestIndex] = nBaseVertex;
				index[nDestIndex + 1] = nBaseVertex + j + 1;
				index[nDestIndex + 2] = nBaseVertex + j + 2;
				++nDestTrianglePlaneIndex;
				nDestIndex += 3;
			}

			// Copy verts
			int nNumberOfVertices = nVertCount;
			int nDestVertIndex = verts.Count();
			// Set the count once (will grow as needed), so we don't call AddTail() several times.
			verts.SetCountNonDestructively( nDestVertIndex + nNumberOfVertices );
			int k = 0;
			while ( nNumberOfVertices >= 4 )
			{
				fltx4 result0 = AddSIMD( vertsIn[k], insidePoint );
				fltx4 result1 = AddSIMD( vertsIn[k + 1], insidePoint );
				fltx4 result2 = AddSIMD( vertsIn[k + 2], insidePoint );
				fltx4 result3 = AddSIMD( vertsIn[k + 3], insidePoint );
				verts[nDestVertIndex] = result0;
				verts[nDestVertIndex + 1] = result1;
				verts[nDestVertIndex + 2] = result2;
				verts[nDestVertIndex + 3] = result3;
				nNumberOfVertices -= 4;
				k += 4;
				nDestVertIndex += 4;
			}
			while ( nNumberOfVertices > 0 )
			{
				fltx4 result = AddSIMD( vertsIn[k], insidePoint );
				verts[nDestVertIndex] = result;
				--nNumberOfVertices;
				++k;
				++nDestVertIndex;
			}
		}
	}
}

void HullFromPlanes( CMesh *pOutMesh, CUtlVector<uint32> *pTrianglePlaneIndices,  const float *pPlanesInput, int nPlaneCount, int nPlaneStrideFloats, float flCoplanarEpsilon )
{
	CUtlVector<VPlane> planes;
	planes.SetCount( nPlaneCount );
	const float *pPlane = pPlanesInput;
	for ( int i = 0; i < nPlaneCount; i++, pPlane += nPlaneStrideFloats )
	{
		planes[i].m_Normal.Init( pPlane[0], pPlane[1], pPlane[2] );
		planes[i].m_Dist = pPlane[3];
	}
	// the clipping code returns polys IN FRONT of the plane, whereas our interface has planes pointing out of the solid
	InvertPlanes( planes.Base(), planes.Count() );
	CUtlVector<Vector> verts;
	CUtlVector<uint32> index;
	CUtlVector<uint32> trianglePlaneIndices;
	CSGPlaneList( verts, index, trianglePlaneIndices, planes.Base(), planes.Count(), flCoplanarEpsilon );
	if ( verts.Count() )
	{
		pOutMesh->AllocateAndCopyMesh( verts.Count(), (float *)verts.Base(), index.Count(), index.Base(), 3, 0, 0 );
	}
	
	// If the caller wants the index of the originating plane of each triangle, put it in the output.
	if( pTrianglePlaneIndices )
		pTrianglePlaneIndices->Swap( trianglePlaneIndices );
}

// Optimized version where there is no need to copy, planes are modified in place (assuming that the list has been created dynamically), and we use VMX intensively
void HullFromPlanes_SIMD( CMesh *pOutMesh, CUtlVector<uint16> *pTrianglePlaneIndices,  fltx4 *pPlanesInput, int nPlaneCount, float flCoplanarEpsilon )
{
#if _DEBUG
	CUtlVector<VPlane> planes;
	planes.SetCount( nPlaneCount );
	const float *pPlane = (const float *)pPlanesInput;
	for ( int i = 0; i < nPlaneCount; i++, pPlane += 4 )
	{
		planes[i].m_Normal.Init( pPlane[0], pPlane[1], pPlane[2] );
		planes[i].m_Dist = pPlane[3];
	}
	// the clipping code returns polys IN FRONT of the plane, whereas our interface has planes pointing out of the solid
	InvertPlanes( planes.Base(), planes.Count() );
#endif

	// the clipping code returns polys IN FRONT of the plane, whereas our interface has planes pointing out of the solid
	InvertPlanes_SIMD( pPlanesInput, nPlaneCount );			// Too bad we are doing this here instead of in the caller code (when creating the array).
														// At the same time, this is implementation specific that the caller should not be aware of
	CUtlVector<fltx4> verts;
	CUtlVector<uint32> index;
	CUtlVector<uint16> trianglePlaneIndices;
	CSGPlaneList_SIMD( verts, index, trianglePlaneIndices, pPlanesInput, nPlaneCount, flCoplanarEpsilon );

#if _DEBUG
	CUtlVector<Vector> vertsSlow;
	CUtlVector<uint32> indexSlow;
	CUtlVector<uint32> trianglePlaneIndicesSlow;
	CSGPlaneList( vertsSlow, indexSlow, trianglePlaneIndicesSlow, planes.Base(), planes.Count(), flCoplanarEpsilon );

	Assert( verts.Count() == vertsSlow.Count() );
	Assert( index.Count() == indexSlow.Count() );
	Assert( trianglePlaneIndices.Count() == trianglePlaneIndicesSlow.Count() );

	for (int i = 0 ; i < verts.Count() ; ++i )
	{
		Assert( SubFloat( verts[i], 0 ) == vertsSlow[i].x );
		Assert( SubFloat( verts[i], 1 ) == vertsSlow[i].y );
		Assert( SubFloat( verts[i], 2 ) == vertsSlow[i].z );
	}
	for (int i = 0 ; i < index.Count() ; ++i )
	{
		Assert( index[i] == indexSlow[i] );
	}
	for (int i = 0 ; i < trianglePlaneIndices.Count() ; ++i )
	{
		Assert( trianglePlaneIndices[i] == trianglePlaneIndicesSlow[i] );
	}
#endif

	// Do we really need a mesh in our use case?
	if ( verts.Count() )
	{
		pOutMesh->AllocateAndCopyMesh( verts.Count(), (float *)verts.Base(), index.Count(), index.Base(), 4, 0, 0 );
	}

	// If the caller wants the index of the originating plane of each triangle, put it in the output.
	if( pTrianglePlaneIndices )
	{
		pTrianglePlaneIndices->Swap( trianglePlaneIndices );
	}
}
