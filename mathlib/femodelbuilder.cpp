//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
#include "mathlib/femodelbuilder.h"
#include "tier1/utlhashtable.h"
#include "tier1/utlsortvector.h"
#include "tier1/heapsort.h"
#include "bitvec.h"
#include "tier1/utlpair.h"
#include "modellib/clothhelpers.h"
#include "tier1/utlstringtoken.h"
#include "mathlib/feagglomerator.h"
#include <algorithm>
#include "femodel.inl"

// this function is exclusively for use with invM0/(invM0+invM1) type of formulae
// if one of the particles is almost infinite mass, I don't want to see any creep in its position (mostly for debugging)
inline float SnapWeight01( float w )
{
	Assert( w >= 0.0f && w <= 1.0f );
	return w < 1e-6f ? 0 : w > 0.99999f ? 1.0f : w;
}

inline float SnapWeight0( float w )
{
	Assert( w >= 0.0f && w <= 1.0f );
	return w < 1e-12f ? 0 : w ;
}

inline bool Is0To1( float w )
{
	return w >= 0 && w <= 1.0f;
}


inline void KeepIn01( float &ref )
{
	if ( ref > 1.0f )
	{
		ref = 1.0f;
	}
	else if ( !( ref >= 0.0f ) ) // the "!" should catch inf or nan
	{
		ref = 0.0f;
	}
}

void CFeModelBuilder::BuildKelagerBends( )
{
	CUtlVector< CUtlVector< uint16 > * > neighbors;
	neighbors.SetCount( m_Nodes.Count( ) );
	for ( int i = 0; i < m_Nodes.Count( ); ++i )
	{
		( neighbors[ i ] = new CUtlVector< uint16 > )->EnsureCapacity( 8 );
	}
	CVarBitVec staticNodes( m_Nodes.Count( ) );

	int nMaxValency = 0;
	for ( int i = 0; i < m_Elems.Count( ); ++i )
	{
		BuildElem_t elem = m_Elems[ i ];
		uint nElemEdges = elem.nNode[ 3 ] == elem.nNode[ 2 ] ? 3 : 4;
		for ( uint j = 0; j < nElemEdges; ++j )
		{
			uint j1 = ( j + 1 ) % nElemEdges;
			uint v0 = elem.nNode[ j ], v1 = elem.nNode[ j1 ];
			if ( j < elem.nStaticNodes )
			{
				staticNodes.Set( v0 );
			}
			if ( neighbors[ v0 ]->Find( v1 ) < 0 )
			{
				int v0elem = neighbors[ v0 ]->AddToTail( v1 );
				int v1elem = neighbors[ v1 ]->AddToTail( v0 );
				nMaxValency = Max( nMaxValency, Max( v0elem, v1elem ) + 1 );
			}
		}
	}

	// now we know which vertex is connected to which, and which is static
	CVarBitVec paired( nMaxValency );

	for ( int nNode = 0; nNode < m_Nodes.Count( ); ++nNode )
	{
		const CUtlVector< uint16 > &ring = *( neighbors[ nNode ] );
		paired.ClearAll( );

		Vector v = m_Nodes[ nNode ].transform.m_vPosition;

		for ( int b0idx = 0; b0idx < ring.Count( ); ++b0idx )
		{
			if ( paired[ b0idx ] )
				continue;
			float flBestCos = -0.87f, flBestH0 = 0;
			int nBestB1idx = -1;
			int b0 = ring[ b0idx ];
			int isB0static = staticNodes[ b0 ];
			Vector vB0 = m_Nodes[ b0 ].transform.m_vPosition - v;
			for ( int b1idx = 0; b1idx < ring.Count( ); ++b1idx )
			{
				if ( b1idx == b0idx )
					continue;
				int b1 = ring[ b1idx ];
				if ( isB0static && staticNodes[ b1 ] )
					continue; // no need to check static-static
				Vector vB1 = m_Nodes[ b1 ].transform.m_vPosition - v;
				float flCos = DotProduct( vB1.Normalized( ), vB0.Normalized( ) );
				if ( flCos < flBestCos )
				{
					flBestCos = flCos;
					nBestB1idx = b1idx;
					flBestH0 = ( vB1 + vB0 ).Length( ) / 3.0f;
				}
			}

			if ( nBestB1idx >= 0 )
			{
				FeKelagerBend_t kbend;
				kbend.flHeight0 = flBestH0;
				kbend.m_nNode[ 0 ] = nNode;
				kbend.m_nNode[ 1 ] = b0;
				kbend.m_nNode[ 2 ] = ring[ nBestB1idx ];
				kbend.m_nFlags = 7;
				for ( int j = 0; j < 3; ++j )
				{
					if ( staticNodes[ kbend.m_nNode[ j ] ] )
						kbend.m_nFlags ^= 1 << j;
				}
				Assert( kbend.m_nFlags );
				m_KelagerBends.AddToTail( kbend );
				paired.Set( nBestB1idx );
			}
		}
	}

	neighbors.PurgeAndDeleteElements( );
}



void CFeModelBuilder::BuildAndSortRods( float flCurvatureAngle, bool bTriangulate ) // flCurvatureAngle is additional angle we can bend each side
{
	CUtlHashtable< uint32, uint32 > edgeToRod;

	// TODO: filter out rods that connect invalid here?

	if ( m_bAddStiffnessRods )
	{
		if ( bTriangulate )
		{
			for ( int nElem = 0; nElem < m_Elems.Count(); ++nElem )
			{
				const BuildElem_t &elem = m_Elems[ nElem ];
				if ( elem.nNode[ 2 ] != elem.nNode[ 3 ] )
				{
					uint v0 = elem.nNode[ 1 ], v1 = elem.nNode[ 3 ];
					BuildRod( flCurvatureAngle, v0, v1, nElem, nElem, elem.nNode[ 0 ], elem.nNode[ 2 ], edgeToRod );
				}
			}
		}


		for ( int nEdge = 0; nEdge < m_FeEdgeDesc.Count(); ++nEdge )
		{
			FeEdgeDesc_t edge = m_FeEdgeDesc[ nEdge ];

			for ( uint nLong = 0; nLong < 2; ++nLong )
			{
				uint v0 = edge.nSide[ 0 ][ nLong ], v1 = edge.nSide[ 1 ][ nLong ];
				BuildRod( flCurvatureAngle, v0, v1, edge.nElem[ 0 ], edge.nElem[ 1 ], edge.nEdge[ 0 ], edge.nEdge[ 1 ], edgeToRod );
			}
		}
	}
	for ( int i = 0; i < m_Rods.Count( ); ++i )
	{
		FeRodConstraint_t &rod = m_Rods[ i ];
		if ( m_Nodes[ rod.nNode[ 0 ] ].nRank > m_Nodes[ rod.nNode[ 1 ] ].nRank )
		{
			rod.InvariantReverse( );
		}
	}

	HeapSort( m_Rods, [&] ( const FeRodConstraint_t &left, const FeRodConstraint_t &right ) {
		int nRankLeft = Min( m_Nodes[ left.nNode[ 0 ] ].nRank, m_Nodes[ left.nNode[ 1 ] ].nRank );
		int nRankRight = Min( m_Nodes[ right.nNode[ 0 ] ].nRank, m_Nodes[ right.nNode[ 1 ] ].nRank );
		if ( nRankLeft == nRankRight )
		{
			float flLeft = Min( left.flWeight0, 1 - left.flWeight0 );
			float flRight = Min( right.flWeight0, 1 - right.flWeight0 );
			return flLeft < flRight;
		}
		else
		{
			return nRankLeft < nRankRight;
		}
	} );
}


void CFeModelBuilder::BuildRod( float flCurvatureAngle, uint v0, uint v1, uint nElem0, uint nElem1, uint nEdgeV0, uint nEdgeV1, CUtlHashtable< uint32, uint32 > &edgeToRod )
{
	if ( !m_Nodes[ v0 ].bSimulated  && m_Nodes[ v1 ].bSimulated )
		return; // rods between static nodes don't work
	uint nRodV0 = v0 | ( v1 << 16 ), nRodV1 = v1 | ( v0 << 16 );
	float invM0 = m_Nodes[ v0 ].invMass, invM1 = m_Nodes[ v1 ].invMass;
	if ( invM1 + invM0 < 1e-12f )
		return; // doesn't matter, this rod is between two infinitely massive nodes

	Vector vEdge = ( m_Nodes[ nEdgeV1 ].transform.m_vPosition - m_Nodes[ nEdgeV0 ].transform.m_vPosition ).NormalizedSafe( Vector( 0, 0, 1 ) );
	float flRelaxedRodLength = ( m_Nodes[ v1 ].transform.m_vPosition - m_Nodes[ v0 ].transform.m_vPosition ).Length();
	float flSumSlack = m_Elems[ nElem0 ].flSlack + m_Elems[ nElem1 ].flSlack;

	// max  distance happens when the two polygons are rotated around edge.nEdge[] so that v0, v1 and edge.nEdge[] are in one plane
	// then, the angle v0-edge.nEdge[1-nLong]-v1 is at its maximum, and is the sum of angles in each quad's plane
	float flMaxDist, flMinDist;
	if ( m_bRigidEdgeHinges )
	{
		flMaxDist = flMinDist = flRelaxedRodLength;
	}
	else
	{
		Vector vCenter = m_Nodes[ nEdgeV0 ].transform.m_vPosition;
		Vector vSide[ 2 ] = { m_Nodes[ v0 ].transform.m_vPosition - vCenter, m_Nodes[ v1 ].transform.m_vPosition - vCenter };
		float flSideProj[ 2 ] = { DotProduct( vSide[ 0 ], vEdge ), DotProduct( vSide[ 1 ], vEdge ) }; // projection of side vectors onto the hinge edge
		float flSideProjDelta = flSideProj[ 0 ] - flSideProj[ 1 ];
		Vector vSideHeight[ 2 ] = { vSide[ 0 ] - vEdge * flSideProj[ 0 ], vSide[ 1 ] - vEdge * flSideProj[ 1 ] };// orthogonal to the hinge components of the sides
		float h[ 2 ] = { vSideHeight[ 0 ].Length( ), vSideHeight[ 1 ].Length( ) };
		flMaxDist = sqrtf( Sqr( flSideProjDelta ) + Sqr( h[ 0 ] + h[ 1 ] ) ) + flSumSlack;
		// min distance happens when triangles are closed like a book at angle flCurvatureAngle. THe points on triangles are running in circles, so it's a distance between points on 2 circles in 3D
		Vector deltaMin( h[ 1 ] * cosf( flCurvatureAngle ) - h[ 0 ], h[1] * sinf( flCurvatureAngle ), flSideProjDelta );
		flMinDist = Min( deltaMin.Length() - flSumSlack, flRelaxedRodLength ); // the mindist cannot be higher than relaxed pose mindist
	}
	Assert( flMinDist <= flMaxDist * 1.0001f + FLT_EPSILON );
	UtlHashHandle_t hFind = edgeToRod.Find( nRodV0 );
	uint nRodVidx = 0;
	if ( hFind == edgeToRod.InvalidHandle( ) )
	{
		hFind = edgeToRod.Find( nRodV1 );
		nRodVidx = 1;
	}
	if ( hFind == edgeToRod.InvalidHandle( ) )
	{
		nRodVidx = 0;

		FeRodConstraint_t rod;
		rod.nNode[ 0 ] = v0;
		rod.nNode[ 1 ] = v1;
		rod.flRelaxationFactor = 1.0f;
		rod.flWeight0 = SnapWeight01( invM0 / ( invM1 + invM0 ) );
		rod.flMinDist = flMinDist;
		rod.flMaxDist = flMaxDist;

		int nRod = m_Rods.AddToTail( rod );
		edgeToRod.Insert( nRodV0, ( uint ) nRod );
	}
	else
	{
		FeRodConstraint_t &rod = m_Rods[ edgeToRod[ hFind ] ];
		Assert( nRodVidx == 0 ? ( rod.nNode[ 0 ] == v0 && rod.nNode[ 1 ] == v1 ) : ( rod.nNode[ 0 ] == v1 && rod.nNode[ 1 ] == v0 ) );
		rod.flMinDist = Min( rod.flMinDist, flMinDist );
		rod.flMaxDist = Max( rod.flMaxDist, flMaxDist );
	}
}



void CFeModelBuilder::BuildSprings( CUtlVector< FeSpringIntegrator_t > &springs )
{
	CUtlHashtable< uint32 > created;

	springs.EnsureCapacity( m_Springs.Count() );
	for ( int i = 0; i < m_Springs.Count( ); ++i )
	{
		FeSpringIntegrator_t spring;
		spring.nNode[ 0 ] = m_Springs[ i ].nNode[ 0 ];
		spring.nNode[ 1 ] = m_Springs[ i ].nNode[ 1 ];

		if ( created.HasElement( spring.nNode[ 0 ] + ( uint( spring.nNode[ 1 ] ) << 16 ) ) )
		{
			continue;
		}

		const BuildNode_t &node0 = m_Nodes[ spring.nNode[ 0 ] ], &node1 = m_Nodes[ spring.nNode[ 1 ] ];
		if ( node1.invMass + node0.invMass < 1e-9f )
		{
			continue; // skip springs between pinned points
		}

		//float flAveDamping = ( node0.integrator.flPointDamping + node1.integrator.flPointDamping ) * 0.5f;

		float expd0 = expf( -node0.integrator.flPointDamping ), expd1 = expf( -node1.integrator.flPointDamping );
		// if particles are severely damped, or are massive, the spring should feel proportionally weaker: 
		// the original Dota spring constants had dimentionality to compute force (not acceleration), so they 
		// effectively (implicitly) were divided by mass to compute acceleration later
		spring.flSpringConstant   = m_Springs[ i ].flSpringConstant * ( node1.invMass * expd1 + node0.invMass * expd0 );
		spring.flSpringDamping    = m_Springs[ i ].flSpringDamping * ( node1.invMass * expd1 + node0.invMass * expd0 );

		if ( spring.flSpringConstant == 0 && spring.flSpringDamping == 0 )
		{
			continue; // this spring will never generate any force...
		}

		spring.flSpringRestLength = ( m_Nodes[ spring.nNode[ 0 ] ].transform.m_vPosition - m_Nodes[ spring.nNode[ 1 ] ].transform.m_vPosition ).Length();

		// make well-damped node to have effectively higher mass (lower invMass)
		// so, node0 damping greater => node0 invMass lower
		// because damping is an exponential function, this should figure out fine
		// also, node0 fixed => weight0 == 0, and vice versa
		spring.flNodeWeight0      = SnapWeight01( node0.invMass * expd0 / ( node1.invMass * expd1 + node0.invMass * expd0 ) );
		created.Insert( spring.nNode[ 0 ] + ( uint( spring.nNode[ 1 ] ) << 16 ) );
		created.Insert( spring.nNode[ 1 ] + ( uint( spring.nNode[ 0 ] ) << 16 ) );

		springs.AddToTail( spring );
	}
}




void CFeModelBuilder::BuildQuads( CUtlVector< FeQuad_t > &quads, bool bSkipTris )
{
	quads.EnsureCapacity( m_Elems.Count( ) );
	uint nQuadCount[ 3 ] = { 0, 0, 0 };
	uint nLastStaticNodes = 2;
	for ( int nElem = 0; nElem < m_Elems.Count( ); ++nElem )
	{
		const BuildElem_t &buildElem = m_Elems[ nElem ];

		bool bIsQuad = buildElem.nNode[ 2 ] != buildElem.nNode[ 3 ];
		if ( bSkipTris && !bIsQuad )
		{
			continue; // skip this quad: it's really a triangle and we skip triangles
		}

		uint nStaticNodes = buildElem.nStaticNodes;
		Assert( nStaticNodes <= nLastStaticNodes );	// quads should already be sorted
		nLastStaticNodes = nStaticNodes;
		FeQuad_t quad;
		quad.flSlack = buildElem.flSlack;

		BuildNode_t node[ 4 ];
		for ( uint j = 0; j < 4; ++j )
		{
			node[ j ] = m_Nodes[ buildElem.nNode[ j ] ];
		}

		float flSumMass = 0;
		float flMass[ 4 ];
		for ( uint j = nStaticNodes; j < 4; ++j )
		{
			flSumMass += ( flMass[ j ] = 1.0f / Max( 1e-6f, node[ j ].invMass ) );
		}

		Vector vCoM = vec3_origin;
		float flWeight[ 4 ] = { 0,0,0,0}, flSumWeights = 0;
		for ( uint j = nStaticNodes; j < 4; ++j )
		{
			flWeight[ j ] = flMass[ j ] / flSumMass;
			vCoM += flWeight[ j ] * node[ j ].transform.m_vPosition;
			flSumWeights += flWeight[ j ];
		}

		const VectorAligned &p0 = node[ 0 ].transform.m_vPosition;
		const VectorAligned &p1 = node[ 1 ].transform.m_vPosition;
		const VectorAligned &p2 = node[ 2 ].transform.m_vPosition;
		const VectorAligned &p3 = node[ 3 ].transform.m_vPosition;

		// special case: the center of mass is assumed to be at or between the infinite-mass nodes
		switch ( nStaticNodes )
		{
		case 1:
			vCoM = p0;
			break;
		case 2:
			vCoM = ( p0 + p1 ) * 0.5f;
			break;
		}

		CFeBasis basis;
		if ( nStaticNodes == 2 )
		{
			basis.Init( p1 - p0, p2 + p3 - 2 * p0 );
		}
		else
		{
			basis.Init( p2 - p0, p3 - p1 );
		}

		Vector vShapeWorld[ 4 ];
		for ( uint j = 0; j < 4; ++j )
		{
			vShapeWorld[ j ] = node[ j ].transform.m_vPosition - vCoM;
			quad.nNode[ j ] = buildElem.nNode[ j ];
			quad.vShape[ j ].Init( basis.WorldToLocal( vShapeWorld[ j ] ), j < nStaticNodes ? 0.0f : SnapWeight0( flWeight[ j ] / flSumWeights ) );
		}

		quads.AddToTail( quad );
		nQuadCount[ nStaticNodes ] = quads.Count( );
	}

	m_nQuadCount[ 0 ] = quads.Count( );
	m_nQuadCount[ 1 ] = Max( nQuadCount[ 1 ], nQuadCount[ 2 ] );
	m_nQuadCount[ 2 ] = nQuadCount[ 2 ];

}


FeTri_t CFeModelBuilder::BuildTri( const BuildElem_t &buildElem, int nTriStaticNodes, int nSubTri )
{
	FeTri_t tri;
	BuildNode_t node[ 3 ];
	for ( uint j = 0; j < 3; ++j )
	{
		tri.nNode[ j ] = buildElem.nNode[ j > 0 ? ( j + nSubTri ) : 0 ];
		node[ j ] = m_Nodes[ tri.nNode[ j ] ];
	}

	float flSumMass = 0;
	float flMass[ 3 ] = { 0, 0, 0 };
	for ( uint j = nTriStaticNodes; j < 3; ++j )
	{
		flSumMass += ( flMass[ j ] = 1.0f / Max( 1e-6f, node[ j ].invMass ) );
	}

	Vector vEdge1 = node[ 1 ].transform.m_vPosition - node[ 0 ].transform.m_vPosition, vEdge2 = node[ 2 ].transform.m_vPosition - node[ 0 ].transform.m_vPosition;
	CFeTriBasis basis( vEdge1, vEdge2 );
	AssertDbg( fabsf( DotProduct( basis.vAxisY, vEdge1 ) ) < 1e-4f * ( 1 + vEdge1.Length() ) );
	AssertDbg( CrossProduct( basis.vAxisX, vEdge1 ).Length( ) < 1e-4f * ( 1 + vEdge1.Length() ) );

	tri.v1x = vEdge1.Length( );
	tri.v2.x = DotProduct( vEdge2, basis.vAxisX );
	tri.v2.y = DotProduct( vEdge2, basis.vAxisY );
	tri.w1 = flMass[ 1 ] / flSumMass;
	tri.w2 = flMass[ 2 ] / flSumMass;

	return tri;
}



void CFeModelBuilder::BuildTris( CUtlVector< FeTri_t > &trisOut, bool bTriangulate )
{
	CUtlVector< FeTri_t > tris[ 3 ];
	tris[ 0 ].EnsureCapacity( m_Elems.Count( ) * 2 );
	for ( int nElem = 0; nElem < m_Elems.Count( ); ++nElem )
	{
		const BuildElem_t &buildElem = m_Elems[ nElem ];
		bool bIsQuad = buildElem.nNode[ 2 ] != buildElem.nNode[ 3 ];
		if ( !bTriangulate && bIsQuad )
		{
			// this is a quad, skip
			continue;
		}

		tris[ buildElem.nStaticNodes ].AddToTail( BuildTri( buildElem, buildElem.nStaticNodes, 0 ) );

		if ( bIsQuad )
		{
			// add the second triangle of the quad ; it's 1 static node if the quad had 2 static nodes
			uint nTri2static = Min< uint >( 1u, buildElem.nStaticNodes );
			tris[ nTri2static ].AddToTail( BuildTri( buildElem, nTri2static, 1 ) );
		}
	}

	m_nTriCount[ 0 ] = tris[ 0 ].Count( ) + tris[ 1 ].Count( ) + tris[ 2 ].Count( );
	m_nTriCount[ 1 ] = tris[ 1 ].Count( ) + tris[ 2 ].Count( );
	m_nTriCount[ 2 ] = tris[ 2 ].Count( );

	trisOut.EnsureCapacity( m_nTriCount[ 0 ] );
	trisOut.AddVectorToTail( tris[ 2 ] );
	trisOut.AddVectorToTail( tris[ 1 ] );
	trisOut.AddVectorToTail( tris[ 0 ] );
}


void CFeModelBuilder::BuildFeEdgeDesc( )
{
	struct Side_t
	{
		uint16 nNode[ 2 ];
		uint16 nElem;
	};
	CUtlHashtable< uint32, Side_t > edgeToVert;
	uint nNodeCount = ( uint )m_Nodes.Count(); NOTE_UNUSED( nNodeCount );
	for ( int nElement = 0; nElement < m_Elems.Count( ); ++nElement )
	{
		BuildElem_t elem = m_Elems[ nElement ];
		const uint nElemEdgeCount = 4;

		for ( uint nElemEdge = 0; nElemEdge < nElemEdgeCount; ++nElemEdge )
		{
			uint j1 = ( nElemEdge + 1 ) % nElemEdgeCount;
			if ( nElemEdge == j1 )
				continue; // skip degenerate edges
			uint32 n0 = elem.nNode[ nElemEdge ], n1 = elem.nNode[ j1 ];
			uint nEdgeV0 = n0 | ( n1 << 16 ), nEdgeV1 = n1 | ( n0 << 16 );
			int j2 = ( nElemEdge + 2 ) % nElemEdgeCount, j3 = ( nElemEdge + 3 ) % nElemEdgeCount;
			uint n2 = elem.nNode[ j2 ], n3 = elem.nNode[ j3 ];
			Assert( n3 < nNodeCount && n2 < nNodeCount && n1 < nNodeCount && n0 < nNodeCount );
			UtlHashHandle_t hFind = edgeToVert.Find( nEdgeV0 );

			uint nEdgeVidx = 0;
			if ( hFind == edgeToVert.InvalidHandle( ) )
			{
				hFind = edgeToVert.Find( nEdgeV1 );
				nEdgeVidx = 1;
			}
			if ( hFind == edgeToVert.InvalidHandle( ) )
			{
				Side_t side = { { (uint16)n2, (uint16)n3 }, (uint16)nElement };
				edgeToVert.Insert( nEdgeV0, side );
			}
			else
			{
				// found a matching edge!
				FeEdgeDesc_t edge;
				edge.nEdge[ 0 ] = n0;
				edge.nEdge[ 1 ] = n1;
				edge.nSide[ 0 ][ 0 ] = n2;
				edge.nSide[ 0 ][ 1 ] = n3;
				edge.nSide[ 1 ][ 0 ] = edgeToVert[ hFind ].nNode[ nEdgeVidx ];
				edge.nSide[ 1 ][ 1 ] = edgeToVert[ hFind ].nNode[ 1 - nEdgeVidx ];
				edge.nElem[ 0 ] = nElement;
				edge.nElem[ 1 ] = edgeToVert[ hFind ].nElem;

				m_FeEdgeDesc.AddToTail( edge );

				edgeToVert.RemoveByHandle( hFind );
			}
		}
	}
}


void CFeModelBuilder::BuildNodeSlack( float flSlackMultiplier )
{
	for ( int nNode = 0; nNode < m_Nodes.Count( ); ++nNode )
	{
		m_Nodes[ nNode ].flSlack = FLT_MAX;
	}
	for ( int nElem = 0; nElem < m_Elems.Count( ); ++nElem )
	{
		BuildElem_t elem = m_Elems[ nElem ];
		for ( int i = 0; i < 4; ++i )
		{
			int i1 = ( i + 1 ) % 4;
			uint n0 = elem.nNode[ i ], n1 = elem.nNode[ i1 ];
			if ( n0 != n1 )
			{
				float flSlack = flSlackMultiplier * ( m_Nodes[ n0 ].transform.m_vPosition - m_Nodes[ n1 ].transform.m_vPosition ).Length( );
				m_Nodes[ n0 ].flSlack = Min( m_Nodes[ n0 ].flSlack, flSlack );
				m_Nodes[ n1 ].flSlack = Min( m_Nodes[ n0 ].flSlack, flSlack );
			}
		}
	}
	for ( int nElem = 0; nElem < m_Elems.Count( ); ++nElem )
	{
		BuildElem_t &elem = m_Elems[ nElem ];
		elem.flSlack = FLT_MAX;
		for ( int i = 0; i < 4; ++i )
		{
			elem.flSlack = Min( elem.flSlack, m_Nodes[ elem.nNode[ i ] ].flSlack );
		}
	}
}







void CFeModelBuilder::BuildOldFeEdges( )
{
	for ( int ei = 0; ei < m_FeEdgeDesc.Count( ); ++ei )
	{
		// found a matching edge!
		FeEdgeDesc_t edgeIn = m_FeEdgeDesc[ ei ];
		OldFeEdge_t edge;
		edge.m_nNode[ 0 ] = edgeIn.nEdge[ 0 ];
		edge.m_nNode[ 1 ] = edgeIn.nEdge[ 1 ];
		edge.m_nNode[ 2 ] = edgeIn.nSide[ 0 ][ 0 ];
		edge.m_nNode[ 3 ] = edgeIn.nSide[ 1 ][ 0 ];

		Vector x[ 4 ];
		for ( int k = 0; k < 4; ++k )
			x[ k ] = m_Nodes[ edge.m_nNode[ k ] ].transform.m_vPosition;
		Vector e[ 5 ] = { x[ 1 ] - x[ 0 ], x[ 2 ] - x[ 0 ], x[ 3 ] - x[ 0 ], x[ 2 ] - x[ 1 ], x[ 3 ] - x[ 1 ] };
		float cot0[ 5 ];

		// this is the factor f to make Q = fK^t * fK, see "Discrete IBM: Implementation", page 2 of "A Quadratic Bending Model for Inextensible Surfaces"
		float flInvAreasSqrt = sqrtf( 6.0f / ( CrossProduct( e[ 0 ], e[ 1 ] ).Length( ) + CrossProduct( e[ 0 ], e[ 2 ] ).Length( ) ) );

		for ( int k = 0; k < 5; ++k )
			cot0[ k ] = DotProduct( e[ 0 ], e[ k ] ) / CrossProduct( e[ 0 ], e[ k ] ).Length( ); // ( cosine / sine ), coincidentally a stable way to find a non-zero (non-degenerate) angle..

		edge.m_flK[ 0 ] = flInvAreasSqrt * ( cot0[ 3 ] + cot0[ 4 ] );
		edge.m_flK[ 1 ] = flInvAreasSqrt * ( cot0[ 1 ] + cot0[ 2 ] );
		edge.m_flK[ 2 ] = flInvAreasSqrt * ( -cot0[ 1 ] - cot0[ 3 ] );

		Vector n1 = CrossProduct( e[ 0 ], e[ 1 ] ), n2 = CrossProduct( e[ 0 ], e[ 2 ] );
		float e_sqr = e[ 0 ].LengthSqr( ), n1_len = n1.Length( ), n2_len = n2.Length( );
		float flTheta0 = atan2f( CrossProduct( n1, n2 ).Length( ), DotProduct( n1, n2 ) );
		edge.flThetaRelaxed = sinf( 0.5f * flTheta0 );
		edge.flThetaFactor = e_sqr / ( n1_len + n2_len );
		edge.t = 0.5f * DotProduct( e[ 2 ] + e[ 1 ], e[ 0 ] ) / e[ 0 ].LengthSqr( );
		edge.invA = flInvAreasSqrt;
		edge.c01 = cot0[ 1 ];
		edge.c02 = cot0[ 2 ];
		edge.c03 = cot0[ 3 ];
		edge.c04 = cot0[ 4 ];

		// virtual edge and axial normal
		m_OldFeEdges.AddToTail( edge );
	}
}


// return 1/(x0^2/invM0+x1^2/invM1)
static inline float Compute1DInvInertiaTensor( float invM0, float x0, float invM1, float x1 )
{
	if ( invM0 <= 1e-8f )
	{
		return invM1 <= 1e-8f || fabsf( x0 ) > 1e-6f ? 0.0f : invM1 / Sqr( x1 );
	}
	else
	{
		if ( invM1 <= 1e-8f )
		{
			return fabsf( x1 ) > 1e-6f ? 0.0f : invM0 / Sqr( x0 );
		}
		else
		{
			return 1.0f / ( Sqr( x0 ) / invM0 + Sqr( x1 ) / invM1 );
		}
	}
}

static inline float ComputeEdgeCenterOfMass( float a, float b )
{
	return fabsf( b ) >= 1e-8f ? a / b : 0.5f; 
}

static inline float CombineInvMasses( float invM0, float invM1 )
{
	if ( invM0 <= 0 || invM1 <= 0 )
		return 0;
	return ( invM0 * invM1 ) / ( invM0 + invM1 ); // ( m0 + m1 ) ^ -1
}

void CFeModelBuilder::BuildAxialEdges( )
{
	for ( int ei = 0; ei < m_FeEdgeDesc.Count( ); ++ei )
	{
		// found a matching edge!
		FeEdgeDesc_t edgeIn = m_FeEdgeDesc[ ei ];
		FeAxialEdgeBend_t edge;
		edge.nNode[ 0 ] = edgeIn.nEdge[ 0 ];
		edge.nNode[ 1 ] = edgeIn.nEdge[ 1 ];
		edge.nNode[ 2 ] = edgeIn.nSide[ 0 ][ 0 ];
		edge.nNode[ 3 ] = edgeIn.nSide[ 0 ][ 1 ];
		edge.nNode[ 4 ] = edgeIn.nSide[ 1 ][ 0 ];
		edge.nNode[ 5 ] = edgeIn.nSide[ 1 ][ 1 ];

		Vector x[ 4 ] = {
			m_Nodes[ edge.nNode[ 0 ] ].transform.m_vPosition,
			m_Nodes[ edge.nNode[ 1 ] ].transform.m_vPosition,
			( m_Nodes[ edge.nNode[ 2 ] ].transform.m_vPosition + m_Nodes[ edge.nNode[ 3 ] ].transform.m_vPosition )* 0.5f,
			( m_Nodes[ edge.nNode[ 4 ] ].transform.m_vPosition + m_Nodes[ edge.nNode[ 5 ] ].transform.m_vPosition )* 0.5f
		};

		// do we need to flip it? The direction of the bend depends on the ordering of nodes
		{
			Vector vApproximateAxis = ( x[ 1 ] + x[ 0 ] ) - ( x[ 2 ] + x[ 3 ] );
			Vector vEdge = x[ 1 ] - x[ 0 ], vVirtualEdge = x[ 2 ] - x[ 3 ];
			Vector vCrossEdges = CrossProduct( vEdge, vVirtualEdge );
			if ( DotProduct( vApproximateAxis, vCrossEdges ) < 0 )
			{
				// flip one of the edges around
				Swap( x[ 0 ], x[ 1 ] );
				Swap( edge.nNode[ 0 ], edge.nNode[ 1 ] );
			}
		}

		float invM[ 4 ] = {
			m_Nodes[ edge.nNode[ 0 ] ].invMass,
			m_Nodes[ edge.nNode[ 1 ] ].invMass,
			edge.nNode[ 2 ] == edge.nNode[ 3 ] ? m_Nodes[ edge.nNode[ 2 ] ].invMass : CombineInvMasses( m_Nodes[ edge.nNode[ 2 ] ].invMass, m_Nodes[ edge.nNode[ 3 ] ].invMass ),
			edge.nNode[ 4 ] == edge.nNode[ 5 ] ? m_Nodes[ edge.nNode[ 4 ] ].invMass : CombineInvMasses( m_Nodes[ edge.nNode[ 4 ] ].invMass, m_Nodes[ edge.nNode[ 5 ] ].invMass )
		};
		if ( invM[ 0 ] + invM[ 1 ] + invM[ 2 ] + invM[ 3 ] == 0 )
		{
			// nothing to keep bent, the static nodes at the fringes will take care of it
			continue;
		}

		Vector e[ 5 ] = { x[ 1 ] - x[ 0 ], x[ 2 ] - x[ 0 ], x[ 3 ] - x[ 0 ], x[ 2 ] - x[ 1 ], x[ 3 ] - x[ 1 ] };
		float cot0[ 5 ] = { 0 };

		// this is the factor f to make Q = fK^t * fK, see "Discrete IBM: Implementation", page 2 of "A Quadratic Bending Model for Inextensible Surfaces"
		//float flInvAreasSqrt = sqrtf( 6.0f / ( CrossProduct( e[ 0 ], e[ 1 ] ).Length() + CrossProduct( e[ 0 ], e[ 2 ] ).Length() ) );

		for ( int k = 1; k < 5; ++k )
		{
			cot0[ k ] = DotProduct( e[ 0 ], e[ k ] ) / CrossProduct( e[ 0 ], e[ k ] ).Length(); // ( cosine / sine ), coincidentally a stable way to find a non-zero (non-degenerate) angle..
		}

		Vector n1 = CrossProduct( e[ 0 ], e[ 1 ] ), n2 = CrossProduct( e[ 0 ], e[ 2 ] );

		// virtual edge and axial normal
		Vector eV = x[ 3 ] - x[ 2 ], an = CrossProduct( e[ 0 ], eV );
		float flAxialNormalLength = an.Length( );
		if ( flAxialNormalLength < 0.001f )
			continue;

		an /= flAxialNormalLength;
		float e0len = e[ 0 ].Length( ), eVlen = eV.Length( );
		Vector e0dir = e[ 0 ] / e0len;
		Vector side = CrossProduct( an, e0dir );
		float t2 = DotProduct( side, e[ 1 ] ), t3 = DotProduct( side, e[ 2 ] );
		// the indices are a misnomer; t correspond to x, e to e in "Discrete Quadratic Curvature Energies" by Wardetzky et al, page 28
		edge.tv = -t2 / ( t3 - t2 );
		Vector v23 = e[ 1 ] + eV * edge.tv;
		float f0len = DotProduct( v23, e0dir );
		edge.te = f0len / e0len;
		Vector v01 = e0dir * f0len;

		Assert( fabs( DotProduct( v01 - v23, e[ 0 ] ) ) < 0.001f && fabs( DotProduct( v01 - v23, eV ) ) < 0.001f );
		edge.flDist = ( v01 - v23 ).Length( );

		float c0len = ComputeEdgeCenterOfMass( e0len * invM[ 0 ], invM[ 0 ] + invM[ 1 ] ); // pretend x0 = 0, x1 = e0len, find the center of mass along that line
		float cVlen = ComputeEdgeCenterOfMass( eVlen * invM[ 2 ], invM[ 2 ] + invM[ 3 ] ); // pretend x2 = 0, x3 = eVlen, find the center of mass of the virtual edge

		// Inv Inertia Tensor is ( m0 * x0^2 + m1 * x1^2 )^-1 == m0^-1 * m1^-1 / ( m1^-1 * x0^2 + m0^-1 * x1^2 ) , we just compute it here
		float flI0inv = Compute1DInvInertiaTensor( invM[ 0 ], ( c0len ), invM[ 1 ], ( e0len - c0len ) );	
		float flIVinv = Compute1DInvInertiaTensor( invM[ 2 ], ( cVlen ), invM[ 3 ], ( eVlen - cVlen ) );

		float /*f0len = edge.t01 * e0len,*/ fVlen = edge.tv * eVlen;
		float f0 = f0len - c0len, fV = fVlen - cVlen;

		if ( 1 )
		{
			// when applying impulse 1.0 to edge 0-1, and -1.0 to virtual edge 2-3, this is how much the x[.] vertices move 
			float flReact[ 4 ] = {
				invM[ 0 ] - flI0inv * f0 * c0len, invM[ 1 ] + flI0inv * f0 * ( e0len - c0len ),
				-( invM[ 2 ] - flIVinv * fV * cVlen ), -( invM[ 3 ] + flIVinv * fV * ( eVlen - cVlen ) )
			};

			// reactions at the fulcrum points
			float flReact01 = edge.te * flReact[ 0 ] + ( 1 - edge.te ) * flReact[ 1 ], flReact23 = edge.tv * flReact[ 2 ] + ( 1 - edge.tv ) * flReact[ 3 ];
			float flReactSumAtFulcrum = flReact01 - flReact23; // applying impulse 1.0 at the connection between fulcrums, this is how much the gap closes
			if ( flReactSumAtFulcrum > 1e-8f )
			{
				edge.flWeight[ 0 ] = ( flReact[ 0 ] / flReactSumAtFulcrum );
				edge.flWeight[ 1 ] = ( flReact[ 1 ] / flReactSumAtFulcrum );
				edge.flWeight[ 2 ] = ( flReact[ 2 ] / flReactSumAtFulcrum );
				//if ( edge.nNode[ 2 ] == edge.nNode[ 3 ] )
				//	edge.flWeight[ 2 ] *= 0.5f; // the same weight will be applied to the same node twice.. Careful! SIMD implementation doesn't need *=0.5! and scalar can easily avoid this, too
				edge.flWeight[ 3 ] = ( flReact[ 3 ] / flReactSumAtFulcrum );
				//if ( edge.nNode[ 4 ] == edge.nNode[ 5 ] )
				//	edge.flWeight[ 3 ] *= 0.5f; // the same weight will be applied to the same node twice.. Careful! SIMD implementation doesn't need *=0.5! and scalar can easily avoid this, too


				Assert( invM[ 0 ] == 0 || invM[ 1 ] == 0 || invM[ 2 ] == 0 || invM[ 3 ] == 0 || fabsf( edge.flWeight[ 0 ] / invM[ 0 ] + edge.flWeight[ 1 ] / invM[ 1 ] + edge.flWeight[ 2 ] / invM[ 2 ] + edge.flWeight[ 3 ] / invM[ 3 ] ) < 1e-5f / ( invM[ 0 ] + invM[ 1 ] + invM[ 2 ] + invM[ 3 ] ) );

				m_AxialEdges.AddToTail( edge );
			}
		}
		else
		{
			// just apply impulse along the line between the edge and virtual edge. 
			float flEdgeInvMass = CombineInvMasses( invM[ 0 ], invM[ 1 ] );
			// virtual edge will move against the direction, main edge will move along
			float flVirtEdgeReact = invM[ 2 ] * ( 1 - edge.tv ) + invM[ 3 ] * edge.tv;
			float flTotalReaction = flEdgeInvMass + flVirtEdgeReact; // this is how much the edge and virt edge close when applying impulse 1.0
			if ( flTotalReaction < FLT_EPSILON )
				continue; // we have nothing to move except one of the vertices on the real edge, and that'll be taken care of by quad contraints
			edge.flWeight[ 0 ] = edge.flWeight[ 1 ] = flEdgeInvMass / flTotalReaction;
			edge.flWeight[ 2 ] = - invM[ 2 ] / flTotalReaction;
			edge.flWeight[ 3 ] = -invM[ 3 ] / flTotalReaction;
		}
	}

	CFeModelBuilder *pThis = this;
	HeapSort( m_AxialEdges, [ pThis ]( const FeAxialEdgeBend_t &left, const FeAxialEdgeBend_t &right )
		{
			return pThis->GetRank( left ) < pThis->GetRank( right );
		}
	);
}

int CFeModelBuilder::ReconcileElemStaticNodes()
{
	int nExtraStaticNodesFound = 0;
	// find out if any elements have 3+ static nodes and remove them; sort nodes in elements that have more static nodes than declared
	// we may FastRemove elements during this loop, so counting downwards is easiest
	for ( int nElem = m_Elems.Count(); nElem-- > 0; )
	{
		BuildElem_t &elem = m_Elems[ nElem ];
		int nExtraStatics = 0;
		{
			uint numNodes = elem.NumNodes();
			for ( uint j = elem.nStaticNodes; j < numNodes; ++j )
			{
				uint nNode = elem.nNode[ j ];
				if ( !m_Nodes[ nNode ].bSimulated )
				{
					++nExtraStatics;
				}
			}
		}

		nExtraStaticNodesFound += nExtraStatics;
		elem.nStaticNodes += nExtraStatics;
		if ( nExtraStatics )
		{
			CUtlVectorAligned< BuildNode_t > &nodes = m_Nodes;
			// element is still dynamic; sort it and go on
			uint numNodes = elem.NumNodes();
			BubbleSort( elem.nNode, numNodes, [ &nodes ]( uint nLeftNode, uint nRightNode ) {
				return int( nodes[ nLeftNode ].bSimulated ) < int( nodes[ nRightNode ].bSimulated ); // sort: non-simulated, then simulated
			} );
			elem.nNode[ 3 ] = elem.nNode[ numNodes - 1 ];

			for ( elem.nStaticNodes = 0; elem.nStaticNodes < numNodes && !nodes[ elem.nNode[ elem.nStaticNodes ] ].bSimulated ; ++elem.nStaticNodes )
				continue;
		}

		if ( elem.nStaticNodes >= 3 )
		{
			// the whole element is static; remove it, mark all as static
			for ( uint j = elem.nStaticNodes; j < BuildElem_t::MAX_NODES; ++j )
			{
				BuildNode_t &refNode = m_Nodes[ elem.nNode[ j ] ];
				if ( refNode.bSimulated && !refNode.bForceSimulated )
				{
					refNode.bSimulated = false;
					nExtraStaticNodesFound++;
					elem.nStaticNodes++;
				}
			}
			// Note: we still need fully static elements to construct stiffness rods, perhaps some face angle constraints, and the analogs of 3-static-node quads
			//m_Elems.FastRemove( nElem );
		}
	}
	return nExtraStaticNodesFound;
}

int CFeModelBuilder::RemoveFullyStaticElems()
{
	int nRemoved = 0;
	for ( int nElem = m_Elems.Count(); nElem-- > 0; )
	{
		BuildElem_t &elem = m_Elems[ nElem ];
		if ( elem.nStaticNodes >= 3 )
		{
			++nRemoved;
			m_Elems.Remove( nElem ); // keep the order
		}
	}
	return nRemoved;
}


void CFeModelBuilder::BuildInvMassesAndSortNodes( )
{
	// we don't remap anything, because we assume everything's downstream of this call
	Assert( m_AxialEdges.IsEmpty( ) && m_OldFeEdges.IsEmpty( ) && m_KelagerBends.IsEmpty( ) &&  m_FeEdgeDesc.IsEmpty( ) );

	CleanupElements();

	const int nInvalidRank = INT_MAX;
	CUtlVector< float > nodeMass;
	if ( m_bEnableExplicitNodeMasses )
	{
		nodeMass.SetCount( m_Nodes.Count( ) );
		for ( int i = 0; i < m_Nodes.Count( ); ++i )
		{
			if ( m_Nodes[ i ].invMass > FLT_EPSILON )
			{
				nodeMass[ i ] = 1.0f / m_Nodes[ i ].invMass;
			}
			else
			{
				nodeMass[ i ] = 0;
				m_Nodes[ i ].bSimulated = false;
			}
		}
	}
	else
	{
		RecomputeMasses( nodeMass );
		BalanceGlobalMassMultipliers( nodeMass );
	}

	CUtlVector< CUtlSortVector< int >* >nodeConn; // nodes this node is connected to
	nodeConn.SetCount( m_Nodes.Count( ) );
	for ( int nNode = 0; nNode < m_Nodes.Count( ); ++nNode )
	{
		m_Nodes[ nNode ].nRank = nInvalidRank;
		nodeConn[ nNode ] = new CUtlSortVector< int >( ); // this shouldn't normally be more than 8 elements, so even sorting this vector isn't a win, but the utility functions on Sortvector are convenient
	}

	// propagate static nodes
	do
	{
		// mark all nodes declared as static in Finite ELements, as static
		for ( int nElem = 0; nElem < m_Elems.Count( ); ++nElem )
		{
			const BuildElem_t &elem = m_Elems[ nElem ];
			for ( uint j = 0; j < elem.nStaticNodes; ++j )
			{
				uint nNode = elem.nNode[ j ];
				m_Nodes[ nNode ].bSimulated = false;
			}
		}
	}
	while ( ReconcileElemStaticNodes() );

	// slam nodes that are not simulated
	for ( int i = 0; i < m_Nodes.Count( ); ++i )
	{
		if ( !m_Nodes[ i ].bSimulated )
		{
			nodeMass[ i ] = 0.0f;
			m_Nodes[ i ].invMass = 0.0f;
		}
	}

	// add node connectivity from elements
	for ( int nElem = 0; nElem < m_Elems.Count(); ++nElem)
	{
		const BuildElem_t &elem = m_Elems[ nElem ];

		if ( CountSimulatedNodesIn( elem ) >= 1 )
		{
			AssertDbg( elem.nNode[ 1 ] != elem.nNode[ 0 ] && elem.nNode[ 2 ] != elem.nNode[ 1 ] && elem.nNode[ 0 ] != elem.nNode[ 2 ] && elem.nNode[ 0 ] != elem.nNode[ 3 ] );

			for ( int j = 0; j < 3; ++j )
			{
				for ( int k = j + 1; k < 4; ++k )
				{
					nodeConn[ elem.nNode[ j ] ]->InsertIfNotFound( elem.nNode[ k ] );
					nodeConn[ elem.nNode[ k ] ]->InsertIfNotFound( elem.nNode[ j ] );
				}
			}
		}
	}

	// add node connectivity from rods
	for ( int nRod = 0; nRod < m_Rods.Count( );  )
	{
		const FeRodConstraint_t &rod = m_Rods[ nRod ];
		if ( CountSimulatedNodesIn( rod ) >= 1 )
		{
			nodeConn[ rod.nNode[ 0 ] ]->InsertIfNotFound( rod.nNode[ 1 ] );
			nodeConn[ rod.nNode[ 1 ] ]->InsertIfNotFound( rod.nNode[ 0 ] );
			++nRod;
		}
		else
		{
			m_Rods.FastRemove( nRod );
		}
	}

	// invert accumulated masses
	if ( !m_bEnableExplicitNodeMasses )
	{
		bool bShouldReconcileElems = false;
		for ( int nNode = 0; nNode < m_Nodes.Count( ); ++nNode )
		{
			BuildNode_t &refNode = m_Nodes[ nNode ];
			float flMassMultiplier = refNode.flMassMultiplier;
			float flMass = nodeMass[ nNode ] * ( flMassMultiplier > FLT_EPSILON ? flMassMultiplier : 1.0f ) + refNode.flMassBias;

			if ( flMass > 1e-8f )
			{
				refNode.invMass = 1.0f / flMass;
			}
			else if ( refNode.bForceSimulated && refNode.bSimulated )
			{
				refNode.invMass = 1.0f;
			}
			else
			{
				if ( refNode.bSimulated )
				{
					bShouldReconcileElems = true;
				}

				refNode.invMass = 0;
				refNode.bSimulated = false;
			}
		}
		if ( bShouldReconcileElems )
		{
			ReconcileElemStaticNodes();
		}
	}

	// set static nodes to invM = 0 (infinite mass)
	m_CtrlToNode.SetCount( m_Nodes.Count( ) );
	m_NodeToCtrl.Purge( );
	int nInvalidNode = -1; // m_Nodes.Count( );
	m_CtrlToNode.FillWithValue( nInvalidNode );

	// start ranking - building static part of Node->Ctrl map
	for ( int nNode = 0; nNode < m_Nodes.Count( ); ++nNode )
	{
		if ( !m_Nodes[ nNode ].bSimulated )
		{
			m_Nodes[ nNode ].nRank = 0;
			m_CtrlToNode[ nNode ] = m_NodeToCtrl.AddToTail( nNode );
		}
	}

	int nRankBegin = 0, nRankEnd = m_NodeToCtrl.Count( ), nNextRank = 1;
	do
	{
		for ( int nParentIdx = nRankBegin; nParentIdx < nRankEnd; ++nParentIdx )
		{
			uint nParentNode = m_NodeToCtrl[ nParentIdx ];
			CUtlSortVector< int > *pConnNodes = nodeConn[ nParentNode ];
			for ( int e = 0; e < pConnNodes->Count( ); ++e )
			{
				uint nChildNode = ( *pConnNodes )[ e ];
				if( nChildNode != nParentNode && m_CtrlToNode[ nChildNode ] == nInvalidNode )
				{
					// this child node isn't mapped yet
					m_CtrlToNode[ nChildNode ] = m_NodeToCtrl.AddToTail( nChildNode );
					m_Nodes[ nChildNode ].nRank = nNextRank;
				}
			}
		}

		nRankBegin = nRankEnd;
		nRankEnd = m_NodeToCtrl.Count( );
		++nNextRank;

		// find all nodes connected to this node
	}
	while ( nRankEnd > nRankBegin );


	// now add isolated islands of elements
	for ( int nElem = 0; nElem < m_Elems.Count( ); ++nElem )
	{
		const BuildElem_t &elem = m_Elems[ nElem ];
		for ( int j = 0; j < 4; ++j )
		{
			uint nNode = elem.nNode[ j ];

			if ( m_CtrlToNode[ nNode ] == nInvalidNode )
			{
				// isolated node
				m_Nodes[ nNode ].nRank = nNextRank;
				m_CtrlToNode[ nNode ] = m_NodeToCtrl.AddToTail( nNode );
			}
		}
	}

	for ( int nRod = 0; nRod < m_Rods.Count( ); ++nRod )
	{
		const FeRodConstraint_t &rod = m_Rods[ nRod ];

		for ( uint j = 0; j < 2; ++j )
		{
			uint nNode = rod.nNode[ j ];
			if ( m_CtrlToNode[ nNode ] == nInvalidNode )
			{
				// isolated rod
				m_Nodes[ nNode ].nRank = nNextRank;
				m_CtrlToNode[ nNode ] = m_NodeToCtrl.AddToTail( nNode );
			}
		}
	}

	for ( int nNode = 0; nNode < m_Nodes.Count( ); ++nNode )
	{
		if ( m_CtrlToNode[ nNode ] == nInvalidNode )
		{
			m_CtrlToNode[ nNode ] = m_NodeToCtrl.AddToTail( nNode );
		}
	}


	HeapSort( m_NodeToCtrl, [&] ( int nLeft, int nRight ) {
		const BuildNode_t &left  = m_Nodes[ nLeft ];
		const BuildNode_t &right = m_Nodes[ nRight ];
		int nLeftDynamic = left.invMass == 0.0f ? 0 : 1;
		int nRightDynamic = right.invMass == 0.0f ? 0 : 1;

		if ( nLeftDynamic == nRightDynamic )
		{
			if ( !nLeftDynamic )
			{
				// the first static nodes: we need "Locked" then "Free" rotation nodes.
				if ( left.bFreeRotation != right.bFreeRotation )
				{
					return ( left.bFreeRotation ? 1 : 0 ) < ( right.bFreeRotation ? 1 : 0 );
				}
			}

			if ( left.nRank == right.nRank )
			{
				return nLeft < nRight;
			}
			return left.nRank < right.nRank;
		}
		return nLeftDynamic < nRightDynamic;
	} );

	for ( int nNode = 0; nNode < m_NodeToCtrl.Count( ); ++nNode )
	{
		m_CtrlToNode[ m_NodeToCtrl[ nNode ] ] = nNode;
	}


	// permutation of nodes
	CUtlVectorAligned< BuildNode_t > newNodes;
	newNodes.SetCount( m_NodeToCtrl.Count( ) );
	for ( int i = 0; i < m_NodeToCtrl.Count( ); ++i )
	{
		newNodes[ i ] = m_Nodes[ m_NodeToCtrl[ i ] ];
		ConvertCtrlToNode( newNodes[ i ].nParent );
		ConvertCtrlToNode( newNodes[ i ].nFollowParent );
	}
	for ( int i = 0; i < m_CollisionSpheres.Count( ); ++i )
	{
		ConvertCtrlToNode( m_CollisionSpheres[ i ].m_nChild );
		ConvertCtrlToNode( m_CollisionSpheres[ i ].m_nParent );
	}
	for ( int i = 0; i < m_CollisionPlanes.Count(); ++i )
	{
		BuildCollisionPlane_t &plane = m_CollisionPlanes[ i ];
		ConvertCtrlToNode( plane.m_nChild );
		ConvertCtrlToNode( plane.m_nParent );
	}
	for ( int i = 0; i < m_PresetNodeBases.Count(); ++i )
	{
		FeNodeBase_t &nb = m_PresetNodeBases[ i ];
		ConvertCtrlToNode( nb.nNode );
		ConvertCtrlToNode( nb.nNodeX0 );
		ConvertCtrlToNode( nb.nNodeX1 );
		ConvertCtrlToNode( nb.nNodeY0 );
		ConvertCtrlToNode( nb.nNodeY1 );
	}
	for ( int i = 0; i < m_TaperedCapsuleStretches.Count(); ++i )
	{
		FeTaperedCapsuleStretch_t &tc = m_TaperedCapsuleStretches[ i ];
		ConvertCtrlToNode( tc.nNode[ 0 ] );
		ConvertCtrlToNode( tc.nNode[ 1 ] );
		KeepIn01( tc.flStickiness );
		if ( tc.flRadius[ 0 ] > tc.flRadius[ 1 ] )
		{
			Swap( tc.flRadius[ 0 ], tc.flRadius[ 1 ] );
			Swap( tc.nNode[ 0 ], tc.nNode[ 1 ] );
		}
	}
	for ( int i = m_SphereRigids.Count(); i-- > 0; )
	{
		FeSphereRigid_t &sr = m_SphereRigids[ i ];
		ConvertCtrlToNode( sr.nNode );
		KeepIn01( sr.flStickiness );
		if ( sr.flRadius < 0.05f )
		{
			// this ball is too small to collide with anything meaningfully
			m_SphereRigids.FastRemove( i );
		}
	}
	for ( int i = m_TaperedCapsuleRigids.Count(); i-- > 0; )
	{
		FeTaperedCapsuleRigid_t &tc = m_TaperedCapsuleRigids[ i ];
		ConvertCtrlToNode( tc.nNode );
		KeepIn01( tc.flStickiness );
		if ( tc.flRadius[ 0 ] > tc.flRadius[ 1 ] )
		{
			Swap( tc.flRadius[ 0 ], tc.flRadius[ 1 ] );
			Swap( tc.vCenter[ 0 ], tc.vCenter[ 1 ] );
		}

		// is this a sphere?
		if ( tc.flRadius[ 1 ] < 0.05f )
		{
			// this rod is too thin to collide meaningfully
			m_TaperedCapsuleRigids.FastRemove( i );
		}
		else if ( ( tc.vCenter[ 0 ] - tc.vCenter[ 1 ] ).Length() <= tc.flRadius[ 1 ] - tc.flRadius[ 0 ] + 0.05f )
		{
			// this cone's tip hides in the base sphere; convert to a sphere
			FeSphereRigid_t sr;
			sr.flRadius = tc.flRadius[ 1 ];
			sr.nNode = tc.nNode;
			sr.nCollisionMask = tc.nCollisionMask;
			m_SphereRigids.AddToTail( sr );
			m_TaperedCapsuleRigids.FastRemove( i );
		}
	}
	for ( int i = 0; i < m_FitInfluences.Count(); ++i )
	{
		FeFitInfluence_t &influence = m_FitInfluences[ i ];
		ConvertCtrlToNode( influence.nMatrixNode );
		ConvertCtrlToNode( influence.nVertexNode );
	}
	Assert( m_ReverseOffsets.Count() == 0 ); // Code path not tested: do we need to convert nBoneCtrl (ctrl->nodes)?
	for ( int i = 0; i < m_ReverseOffsets.Count(); ++i )
	{
		FeNodeReverseOffset_t &revOffset = m_ReverseOffsets[ i ];
		//ConvertCtrlToNode( revOffset.nBoneCtrl );
		ConvertCtrlToNode( revOffset.nTargetNode );
	}
	m_Nodes.Swap( newNodes );

	for ( m_nStaticNodes = 0, m_nRotLockStaticNodes = 0; m_nStaticNodes < m_Nodes.Count( ); m_nStaticNodes++ )
	{
		if ( m_Nodes[ m_nStaticNodes ].invMass > 0 )
			break;

		if ( !m_Nodes[ m_nStaticNodes ].bFreeRotation )
		{
			AssertDbg( m_nRotLockStaticNodes == m_nStaticNodes ); // if this assert fires, it means there are holes in the subarray of static non-rotating nodes
			m_nRotLockStaticNodes = m_nStaticNodes + 1; // this node is locked-rotation
		}
	}

	for ( uint i = 0; i < m_nRotLockStaticNodes; ++i )
	{
		AssertDbg( !m_Nodes[ i ].bFreeRotation );
	}
	
	// now that we have permuted the nodes, reindex all elements
	for ( int i = 0; i < m_Elems.Count( ); ++i )
	{
		BuildElem_t &elem = m_Elems[ i ]; 
		elem.nRank = INT_MAX;
		for ( int j = 0; j < 4; ++j )
		{
			uint &refNodeIndex = elem.nNode[j];
			refNodeIndex = m_CtrlToNode[ refNodeIndex ];
			Assert( refNodeIndex  < uint( m_Nodes.Count( ) ) );
			Assert( uint( j ) < ( elem.nStaticNodes == 3 && elem.nNode[2] == elem.nNode[3] ? 4 : elem.nStaticNodes ) ? ( refNodeIndex < m_nStaticNodes ) : ( refNodeIndex >= m_nStaticNodes ) );
			int nNodeRank = m_Nodes[ refNodeIndex ].nRank;
			if ( nNodeRank < elem.nRank )
				elem.nRank = nNodeRank;
		}
	}
	for ( int i = 0; i < m_Rods.Count( ); ++i )
	{
		for ( int j = 0; j < 2; ++j )
		{
			m_Rods[ i ].nNode[ j ] = m_CtrlToNode[ m_Rods[ i ].nNode[ j ] ];
		}
	}
	for ( int i = 0; i < m_Springs.Count( ); ++i )
	{
		for ( int j = 0; j < 2; ++j )
		{
			m_Springs[ i ].nNode[ j ] = m_CtrlToNode[ m_Springs[ i ].nNode[ j ] ];
		}
	}



	std::stable_sort( m_Elems.begin(), m_Elems.end(), BuildElem_t::Order );
	//HeapSort( m_Elems, BuildElem_t::Order );

	// note: later we sort rods anyway
/*
	HeapSort( m_Rods, [&] ( const FeRodConstraint_t &left, const FeRodConstraint_t  &right ){
		int nLeftRank = Min( m_Nodes[ left.nNode[ 0 ] ].nRank, m_Nodes[ left.nNode[ 1 ] ].nRank );
		int nRightRank = Min( m_Nodes[ right.nNode[ 0 ] ].nRank, m_Nodes[ right.nNode[ 1 ] ].nRank );
		return nLeftRank < nRightRank;
	} );
*/
}




int CFeModelBuilder::CountSimulatedNodesIn( const FeRodConstraint_t & rod )
{
	int nResult = 0;
	for ( int i = 0; i < 2; i++ )
	{
		if ( m_Nodes[ rod.nNode[ i ] ].bSimulated )
		{
			++nResult;
		}
	}
	return nResult;
}



int CFeModelBuilder::CountSimulatedNodesIn( const BuildElem_t& elem )
{
	int nResult = 0;
	for ( int i = 0; i < 4; i++ )
	{
		if ( i > 0 && elem.nNode[ i ] == elem.nNode[ i - 1 ] )
			continue;
		if ( m_Nodes[ elem.nNode[ i ] ].bSimulated )
		{
			++nResult;
		}
	}
	return nResult;
}



void CFeModelBuilder::BuildCtrlOffsets( )
{
	int nBenignlyParentlessVirtualNodes = 0;
	for ( int nNode = 0; nNode < m_Nodes.Count( ); ++nNode )
	{
		const BuildNode_t &node = m_Nodes[ nNode ];
		if ( !node.bVirtual )
		{
			continue;
		}
		if ( node.bSimulated && node.integrator.flAnimationForceAttraction == 0 && node.integrator.flAnimationVertexAttraction == 0 )
		{
			if ( !( m_nDynamicNodeFlags & FE_FLAG_ENABLE_FTL ) )
			{
				// this is a new-style cloth (not source1 compatible), no need to be ultra-conservative, so let's just skip the offsets for nodes that don't need it?
				// unless we want to be able to dynamically tweak animation attraction in softbody
				++nBenignlyParentlessVirtualNodes;
				continue;
			}
		}
		int nParentNode = node.nParent;
		if ( nParentNode < 0 )
			continue; // there's no parent for this virtual node, that's fine, we'll just have no animation attraction because we'll have no animation on it

		while ( nParentNode >= 0 && m_Nodes[ nParentNode ].bVirtual )
		{
			nParentNode = m_Nodes[ nParentNode ].nParent;
			if ( nParentNode == node.nParent )
			{
				// parent node loop detected!
				nParentNode = -1;
				break;
			}
		}
		if ( nParentNode < 0 )
		{
			Warning( "Cannot find real parent of virtual node %d:%s\n", nNode, node.pName );
			continue;
		}

		if ( node.bOsOffset )
		{
			FeCtrlOsOffset_t offset;
			offset.nCtrlChild   = m_NodeToCtrl[ nNode ];
			offset.nCtrlParent  = m_NodeToCtrl[ nParentNode ];
			m_CtrlOsOffsets.AddToTail( offset );
		}
		else
		{
			FeCtrlOffset_t offset;
			offset.nCtrlChild   = m_NodeToCtrl[ nNode ];
			offset.nCtrlParent  = m_NodeToCtrl[ nParentNode ];
			offset.vOffset = m_Nodes[ nParentNode ].transform.TransformVectorByInverse( node.transform.m_vPosition );
			m_CtrlOffsets.AddToTail( offset );
		}
	}
	if ( nBenignlyParentlessVirtualNodes )
	{
		Msg( "%d/%d cloth nodes(verts) will simulate without any animation influence.\n", nBenignlyParentlessVirtualNodes, m_Nodes.Count() );
	}
	// sort just to have more a cache-friendly loop and more eye-friendly list; it's not necessary per se
	HeapSort( m_CtrlOffsets, [] ( const FeCtrlOffset_t &left, const FeCtrlOffset_t &right ) {
		return left.nCtrlChild < right.nCtrlChild;
	} );
	HeapSort( m_CtrlOsOffsets, [] ( const FeCtrlOsOffset_t &left, const FeCtrlOsOffset_t &right ) {
		return left.nCtrlChild < right.nCtrlChild;
	} );
}


// returns the number of inclusive collision ellipsoids
uint CFeModelBuilder::BuildCollisionSpheres( CUtlVector< FeCollisionSphere_t > &collisionSpheres )
{
	for ( int i = m_CollisionSpheres.Count(); i-- > 0; )
	{
		BuildCollisionSphere_t &sphere = m_CollisionSpheres[ i ];
		if ( sphere.IsDegenerate( ) || !m_Nodes[ sphere.m_nChild ].bSimulated )
		{
			m_CollisionSpheres.FastRemove( i );
		}
		else
		{
			KeepIn01( sphere.m_flStickiness );
		}
	}


	HeapSort( m_CollisionSpheres, [] ( const BuildCollisionSphere_t &left, const BuildCollisionSphere_t&right ) {
		if ( left.m_bInclusive != right.m_bInclusive )
			return left.m_bInclusive > right.m_bInclusive;
		if ( left.m_nChild != right.m_nChild )
			return left.m_nChild < right.m_nChild;
		return left.m_nParent < right.m_nParent;
	} );
	uint nInclusive = 0;
// 	while ( nInclusive < uint( m_CollisionEllipsoids.Count( ) ) && m_CollisionEllipsoids[ nInclusive ].m_bInclusive )
// 	{
// 		nInclusive++;
// 	}
	collisionSpheres.SetCount( m_CollisionSpheres.Count( ) );
	for ( int i = 0; i < m_CollisionSpheres.Count( ); ++i )
	{
		const BuildCollisionSphere_t &ce = m_CollisionSpheres[ i ];
		FeCollisionSphere_t &fce = collisionSpheres[ i ];
		if ( ce.m_bInclusive )
		{
			nInclusive = i + 1;
			fce.m_flRFactor = 1.0f / Sqr( ce.m_flRadius );
		}
		else
		{
			fce.m_flRFactor = ce.m_flRadius;
		}

		fce.nChildNode = ce.m_nChild;
		fce.nCtrlParent = m_NodeToCtrl[ ce.m_nParent ];
		fce.m_vOrigin = ce.m_vOrigin;
	}

	return nInclusive;
}



// returns the number of inclusive collision ellipsoids
void CFeModelBuilder::BuildCollisionPlanes( CUtlVector< FeCollisionPlane_t > &collisionPlanes )
{
	for ( int i = m_CollisionPlanes.Count(); i-- > 0; )
	{
		BuildCollisionPlane_t &plane = m_CollisionPlanes[ i ];
		if ( plane.IsDegenerate( ) || !m_Nodes[ plane.m_nChild ].bSimulated )
		{
			m_CollisionPlanes.FastRemove( i );
		}
		else
		{
			KeepIn01( plane.m_flStickiness );
		}
	}


	HeapSort( m_CollisionPlanes, [] ( const BuildCollisionPlane_t &left, const BuildCollisionPlane_t&right ) {
		if ( left.m_nChild != right.m_nChild )
			return left.m_nChild < right.m_nChild;
		return left.m_nParent < right.m_nParent;
	} );
	// 	while ( nInclusive < uint( m_CollisionEllipsoids.Count( ) ) && m_CollisionEllipsoids[ nInclusive ].m_bInclusive )
	// 	{
	// 		nInclusive++;
	// 	}
	collisionPlanes.SetCount( m_CollisionPlanes.Count( ) );
	for ( int i = 0; i < m_CollisionPlanes.Count( ); ++i )
	{
		const BuildCollisionPlane_t &source = m_CollisionPlanes[ i ];
		FeCollisionPlane_t &fce = collisionPlanes[ i ];
		fce.nChildNode = source.m_nChild;
		fce.nCtrlParent = m_NodeToCtrl[ source.m_nParent ];
		fce.m_Plane = source.m_Plane;
		fce.m_Plane.m_vNormal.NormalizeInPlace( );
	}
}




void CFeModelBuilder::BuildWorldCollisionNodes( CUtlVector< FeWorldCollisionParams_t > &worldCollisionParams, CUtlVector< uint16 > &worldCollisionNodes )
{
	CUtlVectorAligned< BuildNode_t > &nodes = m_Nodes;
	int nAdded = 0;
	worldCollisionNodes.EnsureCapacity( nodes.Count( ) );
	for ( int i = 0; i < m_Nodes.Count( ); ++i )
	{
		if ( nodes[ i ].bWorldCollision && nodes[ i ].bSimulated )
		{
			nodes[ i ].flWorldFriction = Min( nodes[ i ].flWorldFriction, 1.0f );
			nAdded++;
			worldCollisionNodes.AddToTail( i );
		}
	}

	if ( !nAdded )
		return;

	// sort by ascending "WorldFriction"; this lends itself to LOD-ing, as we can clamp out "world friction" close to "1" ("world friction" is a parameter from Source1; it's not a physical friction! )
	HeapSort( worldCollisionNodes, [&nodes]( uint16 left, uint16 right ) {
		if ( nodes[ left ].flWorldFriction != nodes[ right ].flWorldFriction )
		{
			return nodes[ left ].flWorldFriction < nodes[ right ].flWorldFriction;
		}
		return nodes[ left ].flGroundFriction > nodes[ right ].flGroundFriction;
	} );

	uint nBegin = 0;
	do
	{
		// start the new block of parameters
		FeWorldCollisionParams_t &params = worldCollisionParams[ worldCollisionParams.AddToTail( ) ];
		BuildNode_t &beginNode = m_Nodes[ worldCollisionNodes[ nBegin ] ];
		params.flWorldFriction = beginNode.flWorldFriction;
		params.flGroundFriction = beginNode.flGroundFriction;
		//uint nFirstNode = worldCollisionNodes[ nBegin ];
		for ( params.nListBegin = nBegin, params.nListEnd = nBegin + 1; int( params.nListEnd ) < worldCollisionNodes.Count(); ++params.nListEnd )
		{
			uint nNextNode = worldCollisionNodes[ params.nListEnd ];
			if ( m_Nodes[ nNextNode ].flWorldFriction - params.flWorldFriction > 0.1f 
				|| m_Nodes[ nNextNode ].flGroundFriction - params.flGroundFriction >0.1f )
			{											  
				break; // time to start a new block of parameters
			}
		}
		nBegin = params.nListEnd; // start the next batch of parameters
	}
	while ( int( nBegin ) < worldCollisionNodes.Count( ) );
}

FeFitWeight_t GetBestWeight( const CFeModelBuilder::FitWeightArray_t &array )
{
	FeFitWeight_t fw = array[ 0 ];
	for ( int i = 1; i < array.Count(); ++i )
	{
		if ( array[ i ].flWeight > fw.flWeight )
			fw = array[ i ];
	}
	return fw;
}


struct CFitMatrixBuildInfo
{
public:
	int m_nNodeIndex;
	CFeModelBuilder::FitWeightArray_t m_StaticNodes;
	CFeModelBuilder::FitWeightArray_t m_DynamicNodes;
	CFitMatrixBuildInfo() : m_nNodeIndex( -1 ) { }
	void AddNode( const FeFitWeight_t &w, bool bSimulated )
	{
		if ( bSimulated )
			m_DynamicNodes.AddToTail( w );
		else
			m_StaticNodes.AddToTail( w );
	}
	static void Normalize( CUtlVector< FeFitWeight_t > &nodes )
	{
		if ( nodes.Count() )
		{
			float flSumWeights = 0;
			for ( int nNode = 0; nNode < nodes.Count(); ++nNode )
			{
				FeFitWeight_t &w = nodes[ nNode ];
				w.nDummy = 0;
				flSumWeights += w.flWeight;
			}
			if ( flSumWeights <= FLT_EPSILON )
			{
				for ( int nNode = 0; nNode < nodes.Count(); ++nNode )
				{
					FeFitWeight_t &w = nodes[ nNode ];
					w.flWeight = 1.0f / float( nodes.Count() );
				}
			}
			else
			{
				for ( int nNode = 0; nNode < nodes.Count(); ++nNode )
				{
					FeFitWeight_t &w = nodes[ nNode ];
					w.flWeight /= flSumWeights;
				}
			}
		}
	}
	int Finish()
	{
		if ( m_nNodeIndex < 0 )
		{
			return 0; // we need a node to influence
		}
		// make sure our dummies are all zeroed out and weights are all summing up to 1.0 - not necessary if we don't use stretch min/max and (bracketing of singular values of Apq*Aqq^-1 matrix)
		// Warning: These normalized weights don't work with FeedbackFitTransforms()
		//Normalize( m_StaticNodes );
		//Normalize( m_DynamicNodes );

		// if we have no influences, we have no weight node
		return m_StaticNodes.Count() + m_DynamicNodes.Count( );
	}

	uint GetMostInfluentialNode()const
	{
		if ( !m_StaticNodes.IsEmpty() )
			return GetBestWeight( m_StaticNodes ).nNode;
		if ( !m_DynamicNodes.IsEmpty() )
			return GetBestWeight( m_DynamicNodes ).nNode;
		return m_nNodeIndex;
	}
};



Vector CFeModelBuilder::ComputeCenter( const FitWeightArray_t &weights )
{
	Vector vVertSum = vec3_origin;
	float flWeightSum = 0;
	for ( int i = 0; i < weights.Count(); ++i )
	{
		const FeFitWeight_t &w = weights[ i ];
		vVertSum += w.flWeight * m_Nodes[ w.nNode ].transform.m_vPosition;
		flWeightSum += w.flWeight;
	}
	if ( flWeightSum > FLT_EPSILON )
		return vVertSum / flWeightSum;
	if ( weights.Count() > 0 )
		return m_Nodes[ weights[ 0 ].nNode ].transform.m_vPosition;
	return vec3_origin;
}

void CFeModelBuilder::BuildFitMatrices( MbaContext_t &context )
{
	CUtlVector< CFitMatrixBuildInfo* > nodeFitMatrices;
	nodeFitMatrices.SetCount( m_Nodes.Count() );
	nodeFitMatrices.FillWithValue( NULL );
	for ( int i = 0; i < m_FitInfluences.Count(); ++i )
	{
		FeFitInfluence_t &inf = m_FitInfluences[ i ];
		if ( inf.flWeight <= 0 )
			continue;

		CFitMatrixBuildInfo*&info = nodeFitMatrices[ inf.nMatrixNode ];
		if ( !info )
			info = new CFitMatrixBuildInfo;
		info->m_nNodeIndex = inf.nMatrixNode;
		FeFitWeight_t fw;
		fw.flWeight = inf.flWeight;
		fw.nNode = inf.nVertexNode;
		fw.nDummy = 0;
		info->AddNode( fw, m_Nodes[ inf.nVertexNode ].bSimulated );
	}

	int nPrecomputeWeightCount = 0;
	for ( int i = nodeFitMatrices.Count(); i-- > 0; )
	{
		if ( !nodeFitMatrices[ i ] )
			nodeFitMatrices.FastRemove( i );
		else if ( int nWeights = nodeFitMatrices[ i ]->Finish() )
		{
			// we keep this one
			nPrecomputeWeightCount += nWeights;
		}
		else
		{
			delete nodeFitMatrices[ i ];
			nodeFitMatrices.FastRemove( i );
		}
	}

	// sort by the count of statics
	HeapSort( nodeFitMatrices, []( CFitMatrixBuildInfo*pLeft, CFitMatrixBuildInfo*pRight ){ return pLeft->m_StaticNodes.Count() > pRight->m_StaticNodes.Count(); } );
	context.fitMatrices.RemoveAll();
	context.fitMatrices.EnsureCapacity( nodeFitMatrices.Count() );
	context.fitWeights.RemoveAll();
	context.fitWeights.EnsureCapacity( nPrecomputeWeightCount );

	int nTooFewInfluences = 0;
	CUtlString simpleFitList, complexFitList;
	int nMatricesByStaticCount[ 3 ] = { 0, 0, 0 };
	for ( int i = 0; i < nodeFitMatrices.Count(); ++i )
	{
		CFitMatrixBuildInfo *pInfo = nodeFitMatrices[ i ];

		FeFitMatrix_t fm;
		fm.nNode = pInfo->m_nNodeIndex;
		fm.nCtrl = m_NodeToCtrl[ fm.nNode ];
		Vector vFitCenter = ComputeCenter( pInfo->m_StaticNodes.Count() ? pInfo->m_StaticNodes : pInfo->m_DynamicNodes );
		fm.vCenter = vFitCenter;
		/*CovMatrix3 Aqq;
		Aqq.Reset();
		for ( FeFitWeight_t &w : pInfo->m_DynamicNodes )
		{
			Aqq.AddCov( m_Nodes[ w.nNode ].transform.m_vPosition - fm.vCenter, w.flWeight );
		}
		fm.AqqInv = Aqq.GetPseudoInverse();*/
		fm.bone.m_vPosition = m_Nodes[ fm.nNode ].transform.m_vPosition - vFitCenter;
		fm.bone.m_orientation = m_Nodes[ fm.nNode ].transform.m_orientation;
		//fm.flStretchMax = fm.flStretchMin = 1.0f;

		if ( pInfo->m_StaticNodes.Count() + pInfo->m_DynamicNodes.Count() < m_nFitMatrixMinInfluences )
		{
			// need to add a few more verts
			nTooFewInfluences++;
			uint nBestInfluence = pInfo->GetMostInfluentialNode();
			if ( nBestInfluence < uint( m_Nodes.Count() ) )
			{
				// add the verts from this element to control this 

				// compute the orientation of this node, 
				// except we'll just stomp that orientation over our target node:
				// the node that doesn't have enough influences to shape-fit it reliably
				FeNodeBase_t nodeBase = BuildNodeBasisFast( m_Nodes, fm.nNode, *( m_NodeNeighbors[ nBestInfluence ] )); 
				m_NodeBases.AddToTail( nodeBase );

				FeNodeReverseOffset_t revOffset;
				revOffset.nBoneCtrl = fm.nCtrl;
				revOffset.nTargetNode = nBestInfluence;
				revOffset.vOffset = m_Nodes[ fm.nNode ].transform.TransformVectorByInverse( m_Nodes[ nBestInfluence ].transform.m_vPosition );
				m_ReverseOffsets.AddToTail( revOffset );
				simpleFitList.Append( " " ); simpleFitList.Append( m_Nodes[ fm.nNode ].pName );
				continue;
			}
			else
			{
				Warning( "Too few simulated vertices bound to matrix %s\n", m_Nodes[ fm.nNode ].pName );
			}
		}
		context.fitWeights.AddVectorToTail( pInfo->m_StaticNodes );
		fm.nBeginDynamic = context.fitWeights.Count();
		context.fitWeights.AddVectorToTail( pInfo->m_DynamicNodes );
		fm.nEnd = context.fitWeights.Count();
		context.fitMatrices.AddToTail( fm );
		nMatricesByStaticCount[ Min( pInfo->m_StaticNodes.Count(), 2 ) ] = context.fitMatrices.Count();
		complexFitList.Append( " " ); complexFitList.Append( m_Nodes[ fm.nNode ].pName );
	}
	if ( nTooFewInfluences )
	{
		Msg( "%d of %d driving bones have too few influences and were switched to simplified single-polygon shape-fit mode: {%s}\n%d matrices will shape-fit complex deformations: {%s}\n", 
			nTooFewInfluences,
			nodeFitMatrices.Count(),
			simpleFitList.Get(),
			context.fitMatrices.Count(),
			complexFitList.Get()
		);
	}

	// make sure [0] >= [1] >= [2]
	for ( int i = 3; i-- > 1; )
		nMatricesByStaticCount[ i - 1 ] = Max( nMatricesByStaticCount[ i - 1 ], nMatricesByStaticCount[ i ] );
	context.m_nFitMatrices1 = nMatricesByStaticCount[ 1 ];
	context.m_nFitMatrices2 = nMatricesByStaticCount[ 2 ];

	nodeFitMatrices.PurgeAndDeleteElements();
}


void CFeModelBuilder::RemoveStandaloneNodeBases( MbaContext_t &context )
{
	CVarBitVec needBase( m_Nodes.Count() );
	for ( int i = 0; i < m_ReverseOffsets.Count(); ++i )
	{
		needBase.Set( CtrlToNode( m_ReverseOffsets[ i ].nBoneCtrl ) );
	}

	// only leave those bases that are necessary to compute before revOffsets
	int nRunningCount = 0;
	for ( int i = 0; i < m_NodeBases.Count(); ++i )
	{
		if ( needBase.IsBitSet( i ) )
		{
			if ( i != nRunningCount )
			{
				m_NodeBases[ nRunningCount ] = m_NodeBases[ i ];
			}
			nRunningCount++;
		}
	}
	m_NodeBases.SetCountNonDestructively( nRunningCount );
}


void CFeModelBuilder::BuildNodeFollowers( CUtlVector< FeFollowNode_t > &nodeFollowers )
{
	nodeFollowers.Purge( );
	nodeFollowers.EnsureCapacity( m_Nodes.Count( ) );
	for ( int i = 0; i < m_Nodes.Count( ); ++i )
	{
		FeFollowNode_t follower;
		follower.flWeight = m_Nodes[ i ].flFollowWeight;
		if ( follower.flWeight > 0.001f && m_Nodes[ i ].nFollowParent >= 0 && m_Nodes[ i ].bSimulated )
		{
			follower.nChildNode = i;
			follower.nParentNode = m_Nodes[ i ].nFollowParent;
			nodeFollowers.AddToTail( follower );
		}
	}
	// note: we actually need to sort the followers, because following is generally order-dependent
	// it shouldn't make a difference for Dota imported cloth, as there cohorts of nodes follow a single parent
	// Just scan the node followers backwards here, to find if there are parents after children
	CVarBitVec followers( m_Nodes.Count() );
	for ( int i = nodeFollowers.Count(); i-- > 0; )
	{
		const FeFollowNode_t &fn = nodeFollowers[ i ];
		if ( followers.IsBitSet( fn.nParentNode ) )
		{
			Warning( "FeModelBuilder: Follower #%d %s is following parent #%d %s, which is also a follower but is setup down the chain. The order should be parent-to-child. The follower chain may produce overly-soft or 1-tick-lagging results\n",
				fn.nChildNode, m_Nodes[ fn.nChildNode ].pName, fn.nParentNode, m_Nodes[ fn.nParentNode ].pName );
		}
		followers.Set( fn.nChildNode );
	}
}

void CFeModelBuilder::CleanupElements()
{
	int nRemoved = 0;
	for ( int nElem = m_Elems.Count(); nElem-- > 0; )
	{
		BuildElem_t &elem = m_Elems[ nElem ];

		uint n0 = elem.nNode[ 0 ], n1 = elem.nNode[ 1 ], n2 = elem.nNode[ 2 ], n3 = elem.nNode[ 3 ];
		Vector p0 = m_Nodes[ n0 ].transform.m_vPosition, p1 = m_Nodes[ n1 ].transform.m_vPosition, p2 = m_Nodes[ n2 ].transform.m_vPosition;
		Vector vCross = CrossProduct( p1 - p0, p2 - p0 );
		if ( vCross.Length() <= 1e-6f )
		{
			++nRemoved;
			if ( n2 != n3 )
			{
				++nRemoved; // count quad as 2 tris for stats
			}
			m_Elems.FastRemove( nElem );
			continue;
		}

		if ( n2 != n3 )
		{
			Vector p3 = m_Nodes[ n3 ].transform.m_vPosition;
			if ( CrossProduct( p2 - p0, p3 - p0 ).Length() <= 1e-6f )
			{
				// make this a triangle, but save the rest of the element
				++nRemoved;
				elem.nNode[ 3 ] = n2;
				continue;
			}
		}
	}
	if ( nRemoved )
	{
		Warning( "Cloth removed %d degenerate triangles, %d elements remaining. Please clean up the mesh.\n", nRemoved, m_Elems.Count() );
	}
}


void CFeModelBuilder::RecomputeMasses( CUtlVector< float >& nodeMass )
{
	nodeMass.SetCount( m_Nodes.Count( ) );
	nodeMass.FillWithValue( 0 ); // all unaccounted nodes are going to stay static

	// accumulate the automatic node masses
	for ( int nElem = 0; nElem < m_Elems.Count( ); ++nElem )
	{
		const BuildElem_t &elem = m_Elems[ nElem ];

		for ( int nTriangulate = 0; nTriangulate < 2; ++nTriangulate )
		{
			uint n0 = elem.nNode[ 0 ], n1 = elem.nNode[ 1 + nTriangulate ], n2 = elem.nNode[ 2 + nTriangulate ];
			if ( n1 == n2 )
				continue;
			Assert( n0 != n1 && n1 != n2 && n2 != n0 );
			Vector p0 = m_Nodes[ n0 ].transform.m_vPosition, p1 = m_Nodes[ n1 ].transform.m_vPosition, p2 = m_Nodes[ n2 ].transform.m_vPosition;
			Vector vCross = CrossProduct( p1 - p0, p2 - p0 );
			float flSinArea = vCross.Length( );
			Assert( flSinArea >= 1e-6f );
			float cot0 = DotProduct( p1 - p0, p2 - p0 ) / flSinArea;
			float cot1 = DotProduct( p2 - p1, p0 - p1 ) / flSinArea;
			float cot2 = DotProduct( p0 - p2, p1 - p2 ) / flSinArea;
			float x01sqr = ( p1 - p0 ).LengthSqr( );
			float x12sqr = ( p2 - p1 ).LengthSqr( );
			float x20sqr = ( p0 - p2 ).LengthSqr( );

			float m0 = 0, m1 = 0, m2 = 0;
			if ( cot0 > 0 && cot1 > 0 && cot2 > 0 )
			{
				// a good, unobtuse triangle
				m0 = .125f * ( x01sqr * cot2 + x20sqr * cot1 );
				m1 = .125f * ( x01sqr * cot2 + x12sqr * cot0 );
				m2 = .125f * ( x20sqr * cot1 + x12sqr * cot0 );
			}
			else
			{
				// an obtuse triangle: when it becomes right, we cross over to split 50/25/25 between obtuse/sharp/sharp 
				float flAqtr = .125f * flSinArea; // quarter area of triangle
				m0 = m1 = m2 = flAqtr;
				// the obtuse angle should get 1/2 area
				if ( cot0 < cot1 )
				{
					if ( cot0 < cot2 )
					{
						Assert( cot0 <= 0 );
						m0 *= 2;
					}
					else
					{
						Assert( cot2 <= 0 );
						m2 *= 2;
					}
				}
				else
				{
					if ( cot1 < cot2 )
					{
						Assert( cot1 <= 0 );
						m1 *= 2;
					}
					else
					{
						Assert( cot2 <= 0 );
						m2 *= 2;
					}
				}
			}
			nodeMass[ n0 ] += m0;
			nodeMass[ n1 ] += m1;
			nodeMass[ n2 ] += m2;
		}
	}

	// we're assuming linear density of 8 kg/in and area density of 1 kg/in^2, and they aren't compatible,
	// but it doesn't matter if we don't have the same nodes participating in both rod and quad constraints.
	// if we do, the auto-mass computation will be skewed, most likely, towards the ropes rather than cloth
	for ( int nRod = 0; nRod < m_Rods.Count( ); ++nRod )
	{
		uint n0 = m_Rods[ nRod ].nNode[ 0 ], n1 = m_Rods[ nRod ].nNode[ 1 ];
		float flLength = ( m_Nodes[ n0 ].transform.m_vPosition - m_Nodes[ n1 ].transform.m_vPosition ).Length( );
		nodeMass[ n0 ] += 8 * flLength;
		nodeMass[ n1 ] += 8 * flLength;
	}
}

// make sure that for nodes that have "global" mass multipliers, we redistributes masses so that
// the mass ratios are exactly what the user specified. The absolute values of the masses are presumed
// to not be very important and are computed automatically to balance against the distances between nodes
// note: connectivity may make this hard to tune. If we have a couple of rags of very different scales,
// the mass ratios between the "global" and "local" nodes are going to be out of whack
void CFeModelBuilder::BalanceGlobalMassMultipliers( CUtlVector< float >& nodeMass )
{
	float flComputedMassSum = 0, flOverrideMassSum = 0;
	for ( int i = 0; i < m_Nodes.Count( ); ++i )
	{
		if ( m_Nodes[ i ].bMassMultiplierGlobal && nodeMass[ i ] > 0 )
		{
			flComputedMassSum += nodeMass[ i ];
			flOverrideMassSum += m_Nodes[ i ].flMassMultiplier;
		}
	}
	
	if ( flOverrideMassSum > 0 && flComputedMassSum > 0 )
	{
		float flScale = flComputedMassSum / flOverrideMassSum;
		for ( int i = 0; i < m_Nodes.Count( ); ++i )
		{
			if ( m_Nodes[ i ].bMassMultiplierGlobal && nodeMass[ i ] > 0 )
			{
				nodeMass[ i ] = flScale * m_Nodes[ i ].flMassMultiplier;
			}
		}
	}
}


bool Conflict( const FeRodConstraint_t &a, const FeRodConstraint_t &b )
{
	return a.nNode[ 0 ] == b.nNode[ 0 ] || a.nNode[ 0 ] == b.nNode[ 1 ] || a.nNode[ 1 ] == b.nNode[ 0 ] || a.nNode[ 1 ] == b.nNode[ 1 ];
}

bool Conflict( const CFeModelBuilder::BuildElem_t &a, const CFeModelBuilder::BuildElem_t &b )
{
	for ( int i = 0; i < 4; ++i )
	{
		for ( int j = 0; j < 4; ++j )
		{
			if ( a.nNode[ i ] == b.nNode[ j ] )
				return true;
		}
	}
	return false;
}

bool Conflict( const FeQuad_t &a, const FeQuad_t &b )
{
	for ( int i = 0; i < 4; ++i )
	{
		for ( int j = 0; j < 4; ++j )
		{
			if ( a.nNode[ i ] == b.nNode[ j ] )
				return true;
		}
	}
	return false;
}


bool Conflict( const FeTri_t &a, const FeTri_t &b )
{
	for ( int i = 0; i < 3; ++i )
	{
		for ( int j = 0; j < 3; ++j )
		{
			if ( a.nNode[ i ] == b.nNode[ j ] )
				return true;
		}
	}
	return false;
}


bool Conflict( const FeSpringIntegrator_t &a, const FeSpringIntegrator_t &b )
{
	return a.nNode[ 0 ] == b.nNode[ 0 ] || a.nNode[ 0 ] == b.nNode[ 1 ] || a.nNode[ 1 ] == b.nNode[ 0 ] || a.nNode[ 1 ] == b.nNode[ 1 ];
}


void ListUsedNodes( const FeNodeBase_t &a, uint16 nNodes[ 5 ] )
{
	nNodes[ 0 ] = a.nNode;
	nNodes[ 1 ] = a.nNodeX0;
	nNodes[ 2 ] = a.nNodeX1;
	nNodes[ 3 ] = a.nNodeY0;
	nNodes[ 4 ] = a.nNodeY1;
}

bool Conflict( const FeNodeBase_t &a, const FeNodeBase_t &b )
{
	uint16 m[ 5 ], n[ 5 ];
	ListUsedNodes( a, m );
	ListUsedNodes( b, n );
	for ( int i = 0; i < 5; ++i )
	{
		for ( int j = 0; j < 5; ++j )
		{
			if ( m[ i ] == n[ j ] )
				return true;
		}
	}
	return false;
}



template <typename T>
bool CanAddToSchedule( const CUtlVector< T > &schedule, const T &add, int nLanes )
{
	for ( int j = ( schedule.Count( ) & ~( nLanes - 1 ) ); j < schedule.Count( ); ++j )
	{
		if ( Conflict( schedule[ j ], add ) )
			return false;
	}
	return true;
}


template < typename T, typename TSimd >
void Schedule( CUtlVectorAligned< TSimd > *pOutput, const T *pInput, int nInputCount, int nLanes )
{
	Assert( !( nLanes & ( nLanes - 1 ) ) );
	CUtlVector< T > schedule, delayed;
	delayed.CopyArray( pInput, nInputCount );
	schedule.EnsureCapacity( delayed.Count( ) );

	int nScheduleRoll = 0;

	while ( delayed.Count( ) > 0 || ( schedule.Count( ) & ( nLanes - 1 ) ) )
	{
		bool bAdded = false;
		//if ( !( i & ( nLanes - 1 ) ) )
		for ( int j = delayed.Count( ); j-- > 0; )
		{
			if ( CanAddToSchedule( schedule, delayed[ j ], nLanes ) )
			{
				schedule.AddToTail( delayed[ j ] );
				delayed.Remove( j );
				bAdded = true;
				break;
			}
		}
		if ( !bAdded )
		{
			//try to find something else that we can solve twice per iteration
			for ( int j = 0; j < schedule.Count( ); ++j )
			{
				int k = ( nScheduleRoll + j ) % schedule.Count( );
				++nScheduleRoll;
				if ( CanAddToSchedule( schedule, schedule[ k ], nLanes ) )
				{
					T duplicate = schedule[ k ];
					schedule.AddToTail( duplicate );
					bAdded = true;
					break;
				}
			}
			if ( !bAdded )
			{
				// found nothing, just duplicate the last element, it's always safe
				T safeDup = schedule.Tail( );
				schedule.AddToTail( safeDup );
			}
		}
	}

	Assert( !( schedule.Count( ) & ( nLanes - 1 ) ) );
	pOutput->SetCount( schedule.Count( ) / nLanes );
	for ( int nScalar = 0, nSimd = schedule.Count( ) / nLanes; nSimd-- > 0; nScalar += nLanes )
	{
		( *pOutput )[ nSimd ].Init( &schedule[ nScalar ] );
	}
}



// build the sequences of node indices from parent to child  that are used in reconstruction of node rotations
void CFeModelBuilder::BuildRopes( )
{
	// mark "1" for nodes that are NOT ends of ropes, e.g. they:
	// a) have rope (sub)chains hanging off of them
	// b) have quads attached to them, so they can form their bases by looking at those quads
	// c) have preset bases for any other reason
	CVarBitVec skip( m_Nodes.Count( ) ); 
	CVarBitVec hasBase( m_Nodes.Count() );

	// skip already preset base
	for ( int i = 0; i < m_NodeBases.Count(); ++i )
	{
		uint nNode = m_NodeBases[ i ].nNode;
		skip.Set( nNode );
		hasBase.Set( nNode );
	}
	// skip free nodes
	for ( int i = 0; i < m_Nodes.Count(); ++i )
	{
		if ( m_Nodes[ i ].bSimulated && m_Nodes[ i ].bAnimRotation )
		{
			skip.Set( i );
		}
	}

	if ( IsDebug() )
	{
		for ( int nElem = 0; nElem < m_Elems.Count(); ++nElem )
		{
			const BuildElem_t &elem = m_Elems[ nElem ];
			AssertDbg( elem.nNode[ 1 ] != elem.nNode[ 0 ] && elem.nNode[ 2 ] != elem.nNode[ 1 ] && elem.nNode[ 0 ] != elem.nNode[ 2 ] );
			// the first 3 nodes are unique, so there's at least a triangle here (hopefully non-degenerate), so we'll try to rely on that for base generation
			for ( int c = 0; c < ARRAYSIZE( elem.nNode ); ++c )
			{
				BuildNode_t &node = m_Nodes[ elem.nNode[ c ] ]; NOTE_UNUSED( node );
				Assert( skip.IsBitSet( elem.nNode[ c ] ) || !node.bSimulated || node.bVirtual ); // should've been set when computing node bases
			}
		}
	}

	for ( int nNode = 0; nNode < m_Nodes.Count( ); ++nNode )
	{
		BuildNode_t &node = m_Nodes[ nNode ];
		if ( !node.bSimulated || node.bVirtual )
		{
			// this can't be the tail of a rope
			skip.Set( nNode );
			hasBase.Set( nNode );
			continue;
		}
		
		if ( node.nParent >= 0 )
		{
			// the parent can't be the tail because this node will be the tail
			skip.Set( node.nParent );
		}
	}

	// now we know which nodes are part of the chain
	for ( int nTail = 0; nTail < m_Nodes.Count( ); ++nTail )
	{
		if ( m_Nodes[ nTail ].bSimulated && !m_Nodes[ nTail ].bVirtual && m_Nodes[ nTail ].nParent >= 0 && !skip.IsBitSet( nTail ) )
		{
			// starting from the tip of the chain, walk up to form a chain
			CUtlVector< int > *pChain = new CUtlVector< int >;
			for ( int nLink = nTail; ; nLink = m_Nodes[ nLink ].nParent )
			{
				if ( nLink < 0 )
				{
					// the chain unexpectedly terminated with a (previous) dynamic: ambiguous case, because where do we start the default transform?
					AssertDbg( !pChain->IsEmpty( ) );
					int nRoot = pChain->Tail( );
					pChain->AddToTail( nRoot ); // just pretend the start of the chain will prime itself w.r.t. orientation.
					break;
				}
				else
				{
					pChain->AddToTail( nLink );
					if ( hasBase[ nLink ] )
					{
						// the chain is terminated with a static: the easiest case
						break;
					}
					skip.Set( nLink );
				}
			}
			pChain->Reverse();
			m_Ropes.AddToTail( pChain );
		}
	}
}

void CFeModelBuilder::BuildFreeNodes( MbaContext_t &context )
{
	CVarBitVec hasBase( m_Nodes.Count() );
	for ( int i = 0; i < m_Ropes.Count(); ++i )
	{
		for ( int j = 1; j < m_Ropes[ i ]->Count(); ++j )
		{
			int nNode = m_Ropes[ i ]->Element( j );
			AssertDbg( !hasBase[ nNode ] );
			hasBase.Set( nNode );
		}
	}
	for ( int i = 0; i < m_NodeBases.Count(); ++i )
	{
		int nNode = m_NodeBases[ i ].nNode;
		AssertDbg( !hasBase[ nNode ] ); //
		hasBase.Set( nNode );
	}
	for ( int i = 0; i < context.fitMatrices.Count(); ++i )
	{
		const FeFitMatrix_t &fm = context.fitMatrices[ i ];
		hasBase.Set( fm.nNode );
	}
	for ( int nNode = 0; nNode < m_Nodes.Count(); ++nNode )
	{
		if ( m_Nodes[ nNode ].bSimulated )
		{
			if ( !hasBase.IsBitSet( nNode ) || m_Nodes[ nNode ].bAnimRotation )
			{
				AssertDbg( !hasBase.IsBitSet( nNode ) ); // we should have node that takes rotation from animation, and also has the rotation computed by ropes or by node bases - that'd be wasteful. but otherwise, this assert is harmless.
				m_FreeNodes.AddToTail( nNode );
			}
		}
	}
	
}


template <typename Functor >
CUtlPair< int, int > FindBestPair( const CUtlSortVector< int > &neighbors, Functor fn )
{
	CUtlPair< int, int > best( 0, 0 );
	float flBest = -FLT_MAX;
	for ( int i = 0; i < neighbors.Count( ); ++i )
	{
		for ( int j = i + 1; j < neighbors.Count( ); ++j )
		{
			float flGrade = fn( neighbors[ i ], neighbors[ j ] );
			if ( flGrade >= flBest )
			{
				flBest = flGrade;
				best.first = neighbors[ i ];
				best.second = neighbors[ j ];
			}
		}
	}
	return best;
}


bool IsIdentity( const Quaternion &q, float flBiasSqr = 1e-8f )
{
	return q.x * q.x + q.y * q.y + q.z * q.z < flBiasSqr;
}


FeNodeBase_t CFeModelBuilder::BuildNodeBasisFast( uint nNode )
{
	return BuildNodeBasisFast( m_Nodes, nNode, *( m_NodeNeighbors[ nNode ] ) );
}


FeNodeBase_t CFeModelBuilder::BuildNodeBasisFast( const CUtlVectorAligned< BuildNode_t > &nodes, uint nNode, const CUtlSortVector< int > &neighbors )
{
	FeNodeBase_t basis;
	V_memset( &basis, 0, sizeof( basis ) );
	basis.nNode = nNode;
	
	// find max-length axis
	CUtlPair< int, int > axisXIndices = FindBestPair( neighbors,
		[&nodes] ( uint n0, uint n1 )
		{
			return ( nodes[ n0 ].transform.m_vPosition - nodes[ n1 ].transform.m_vPosition ).Length( );
		}
	);
	basis.nNodeX0 = axisXIndices.second;
	basis.nNodeX1 = axisXIndices.first;

	Vector vAxisX = ( nodes[ basis.nNodeX1 ].transform.m_vPosition - nodes[ basis.nNodeX0 ].transform.m_vPosition );
	// find max-area cross-axis
	CUtlPair< int, int > axisYIndices = FindBestPair( neighbors,
		[&nodes, &vAxisX] ( uint n0, uint n1 )
		{
			return CrossProduct( nodes[ n0 ].transform.m_vPosition - nodes[ n1 ].transform.m_vPosition, vAxisX ).Length( );
		}
	);
	basis.nNodeY0 = axisYIndices.second;
	basis.nNodeY1 = axisYIndices.first;

	Vector vAxisY = ( nodes[ basis.nNodeY1 ].transform.m_vPosition - nodes[ basis.nNodeY0 ].transform.m_vPosition ).NormalizedSafe( Vector( 0,0,-1 ) );
	vAxisX = ( vAxisX - DotProduct( vAxisY, vAxisX ) * vAxisY );
	float flAxisXlen = vAxisX.Length( );
	if ( flAxisXlen <= 0.05f )
	{
		CUtlString list;
		for ( int j = 0; j < neighbors.Count( ); ++j )
		{
			if ( j )
				list += ", ";
			list += nodes[ neighbors[ j ] ].pName;
		}
		Warning( "Degenerate finite element (%s), won't be able to compute good normals for node %d bone %s\n", list.Get( ), nNode, nodes[ nNode ].pName );
		Vector vAxisXAlt = CrossProduct( nodes[ nNode ].transform.m_orientation.GetUp(), vAxisY );
		float flAxisXAltLen = vAxisXAlt.Length( );
		if ( flAxisXAltLen < 0.001f )
		{
			vAxisX = VectorPerpendicularToVector( vAxisY );
		}
		else
		{
			vAxisX = vAxisXAlt / flAxisXAltLen;
		}
	}
	else
	{
		vAxisX /= flAxisXlen;
	}
	matrix3x4_t tmPredicted;
	tmPredicted.InitXYZ( vAxisX, vAxisY, CrossProduct( vAxisX, vAxisY ), vec3_origin );
	Assert( IsGoodWorldTransform( tmPredicted, 1.0f ) );
	Quaternion qPredicted = MatrixQuaternion( tmPredicted );
	basis.qAdjust = Conjugate( qPredicted ) * nodes[ nNode ].transform.m_orientation;
	QuaternionNormalize( basis.qAdjust );

	// there are a few special cases, when we can rearrange the axis order to get qAdjust to become identity
	// that is more efficent to compute, we'll save a matrix multiplication per node
	if ( IsIdentity( basis.qAdjust ) )
	{
		basis.qAdjust = quat_identity;
	}
	else if ( IsIdentity( basis.qAdjust * Quaternion( 0, 0, 1, 0 ) ) ) // 180 degree rotation around Z
	{
		// just flip X and Y axes to give adjust identity
		Swap( basis.nNodeX0, basis.nNodeX1 );
		Swap( basis.nNodeY0, basis.nNodeY1 );
		basis.qAdjust = quat_identity;
	}
	else if ( IsIdentity( basis.qAdjust * Quaternion( 0, 1, 0, 0 ) ) ) // 180 degree rotation around Y
	{
		// just flip X axis to give adjust identity
		Swap( basis.nNodeX0, basis.nNodeX1 );
		basis.qAdjust = quat_identity;
	}
	else if ( IsIdentity( basis.qAdjust * Quaternion( 1, 0, 0, 0 ) ) ) // 180 degree rotation around X
	{
		// just flip X axis to give adjust identity
		Swap( basis.nNodeY0, basis.nNodeY1 );
		basis.qAdjust = quat_identity;
	}
/*	else
	{ // test, debug the clauses above with this
		Swap( basis.nNodeX0, basis.nNodeX1 );
		Swap( basis.nNodeY0, basis.nNodeY1 );
		basis.qAdjust *= Quaternion( 0, 0, 1, 0 );
	}
*/

	return basis;
}


void CFeModelBuilder::BuildNodeBases( )
{
	BuildNodeBases( m_Nodes, m_Elems, m_PresetNodeBases, m_NodeBases, m_NodeNeighbors );
}


void CFeModelBuilder::BuildNodeBases( const CUtlVectorAligned< BuildNode_t > &nodes, const CUtlVector< BuildElem_t > &elems, const CUtlVector< FeNodeBase_t > &presetNodeBases, CUtlVector< FeNodeBase_t > &nodeBases, CUtlVectorOfPointers< CUtlSortVector< int > > &neighbors )
{
	neighbors.SetCountAndInit( nodes.Count( ),
		[] ( int nNode )
		{ 
			CUtlSortVector< int > *pResult = new CUtlSortVector< int >;
			pResult->Insert( nNode ); //  node is always a neighbor of itself..
			return pResult;
		}
	);

	for ( int nElem = 0; nElem < elems.Count( ); ++nElem )
	{
		const BuildElem_t &elem = elems[ nElem ];
		AssertDbg( elem.nNode[ 1 ] != elem.nNode[ 0 ] && elem.nNode[ 2 ] != elem.nNode[ 1 ] && elem.nNode[ 0 ] != elem.nNode[ 2 ] );
		int numNodes = elem.nNode[ 3 ] == elem.nNode[ 2 ] ? 3 : 4;
		for ( int c = 0; c < numNodes; ++c )
		{
			for ( int d = 0; d < numNodes; ++d )
			{
				// add the previous and the next node
				neighbors[ elem.nNode[ c ] ]->InsertIfNotFound( elem.nNode[ d ] );
			}
		}
	}

	// copy the preset node bases
	nodeBases.EnsureCapacity( presetNodeBases.Count() );
	CVarBitVec nodeWithBase( nodes.Count() );
	for ( int i = 0; i < presetNodeBases.Count(); ++i )
	{
		const FeNodeBase_t &nb = presetNodeBases[ i ];
		if ( !nodeWithBase.IsBitSet( nb.nNode ) )
		{
			nodeBases.AddToTail( nb );
			nodeWithBase.Set( nb.nNode );
		}
	}


	// compute additional bases
	for ( int nNode = 0; nNode < nodes.Count( ); ++nNode )
	{
		if ( !nodeWithBase.IsBitSet( nNode ) && !( nodes[ nNode ].bSimulated && nodes[ nNode ].bAnimRotation ) )
		{
			CUtlSortVector< int > *pCheck = neighbors[ nNode ];
			const BuildNode_t &node = nodes[ nNode ];
			if ( pCheck->Count() >= 3  // we need at least 3 nodes to compute the basis
				 && ( !node.bVirtual || node.bNeedNodeBase ) // virtual nodes have no effect on animation, there's no need to do a complex computation of basis for them
				 && ( node.bSimulated || node.bFreeRotation ) // static nodes need not recompute their basis, they get it from animation; unless we explicitly specified we need a free-rotating locked/static/keyframed node. Then compute the rotation.
				 )
			{
				nodeBases.AddToTail( BuildNodeBasisFast( nodes, nNode, *( neighbors[ nNode ] ) ) );
				nodeWithBase.Set( nNode );
			}
		}
	}
}



int RopeIndexCount( const CUtlVectorOfPointers< CUtlVector< int > > &ropes )
{
	int nCount = ropes.Count( );
	for ( int i = 0; i < ropes.Count( ); ++i )
	{
		nCount += ropes[ i ]->Count( );
	}
	return nCount;
}


void CFeModelBuilder::ValidateBases( )
{
#ifdef DBGFLAG_ASSERT
	int nLog = CommandLine( )->FindParm( "-log" );

	CVarBitVec hasBase( m_Nodes.Count( ) );
	for ( int i = 0; i < m_Ropes.Count( ); ++i )
	{
		for ( int j = 1; j < m_Ropes[ i ]->Count( ); ++j )
		{
			int nNode = m_Ropes[ i ]->Element( j );
			AssertDbg( !hasBase[ nNode ] );
			hasBase.Set( nNode );
		}
	}
	for ( int i = 0; i < m_NodeBases.Count( ); ++i )
	{
		int nNode = m_NodeBases[ i ].nNode;
		AssertDbg( !hasBase[ nNode ] ); //
		hasBase.Set( nNode );
	}
	for ( int i = 0; i < m_FreeNodes.Count(); ++i )
	{
		hasBase.Set( m_FreeNodes[ i ] );
	}

	//--
	for ( int nNode = 0; nNode < m_Nodes.Count( ); ++nNode )
	{
		int nHasBase = hasBase[ nNode ] ? 1 : 0; NOTE_UNUSED( nHasBase );
		int nSimulated = m_Nodes[ nNode ].bSimulated ? 1 : 0;		NOTE_UNUSED( nSimulated );
		//const char *pBoneName = m_Nodes[ nNode ].pName; NOTE_UNUSED( pBoneName );
		Assert( nHasBase == nSimulated || m_Nodes[ nNode ].bVirtual || m_Nodes[ nNode ].bFreeRotation ); // we don't really care if virtual nodes have bases or not. In fact, it's probably better not to compute bases for them, but it may be necessary if real nodes' bases depend on them..
		if ( m_Nodes[ nNode ].nParent < 0 )
		{
			if ( nLog )
			{
				PrintNodeTree( nNode, "" );
			}
		}
	}

	//
	// Just print out the elements for eyeballing
	// 
	for ( int nElem = 0; nElem < m_Elems.Count( ); ++nElem )
	{
		CUtlVectorFixedGrowable< const char *, 4 > names;
		const BuildElem_t &elem = m_Elems[ nElem ];
		int nPrefixLen = 1000;
		for ( uint n = 0; n < elem.NumNodes( ); ++n )
		{
			const char *pName = m_Nodes[ elem.nNode[ n ] ].pName;
			names.AddToTail( pName );
			int nNewLen = 0;
			for ( ; nNewLen < nPrefixLen && pName && pName[ nNewLen ] && names[ 0 ][ nNewLen ] == pName[ nNewLen ]; ++nNewLen )
				continue;
			nPrefixLen = nNewLen;
		}
		CUtlString list;
		list.SetDirect( names[ 0 ], nPrefixLen );
		list += "( ";
		for ( int n = 0; n < names.Count( ); ++n )
		{
			if ( n )
				list += ", ";
			list += names[ n ] ? names[ n ] + nPrefixLen : "null";
		}
		list += " )";
		if ( nLog )
		{
			Msg( "%s\n", list.Get( ) );
		}
	}
#endif
}


void CFeModelBuilder::PrintNodeTree( uint nRoot, const CUtlString &prefix )
{
	const BuildNode_t &root = m_Nodes[ nRoot ];
	Msg( "%s%s%s #%d", prefix.Get( ), root.pName, root.bSimulated ? " sim" : "", nRoot );
	if ( root.flFollowWeight > 0 && root.nFollowParent >= 0 )
	{
		Msg( " - follow %s w%.3f", m_Nodes[ root.nFollowParent ].pName, root.flFollowWeight );
	}
	Msg( "\n" );
	for ( int nChild = 0; nChild < m_Nodes.Count( ); ++nChild )
	{
		if ( m_Nodes[ nChild ].nParent == int( nRoot ) )
		{
			PrintNodeTree( nChild, prefix + "  " );
		}
	}
}


void CFeModelBuilder::CheckIdentityCtrlOrder( )
{
	if ( m_bIdentityCtrlOrder )
	{
		// convert all Ctrl-based indices to node-based indices
		for ( int i = 0; i < m_CtrlOffsets.Count(); ++i )
		{
			FeCtrlOffset_t &offset = m_CtrlOffsets[ i ];
			ConvertCtrlToNode( offset.nCtrlChild );
			ConvertCtrlToNode( offset.nCtrlParent );
		}

		for ( int i = 0; i < m_CtrlOsOffsets.Count(); ++i )
		{
			FeCtrlOsOffset_t &offset = m_CtrlOsOffsets[ i ];
			ConvertCtrlToNode( offset.nCtrlChild );
			ConvertCtrlToNode( offset.nCtrlParent );
		}

		for ( int i = 0; i < m_ReverseOffsets.Count(); ++i )
		{
			FeNodeReverseOffset_t &revOffset = m_ReverseOffsets[ i ];
			ConvertCtrlToNode( revOffset.nBoneCtrl );
		}

		m_NodeToCtrl.SetCount( m_Nodes.Count( ) );
		m_CtrlToNode.SetCount( m_Nodes.Count( ) );
		for ( int i = 0; i < m_Nodes.Count( ); ++i )
		{
			m_NodeToCtrl[ i ] = i;
			m_CtrlToNode[ i ] = i;
		}
	}
	else
	{
		if ( m_NodeToCtrl.Count( ) == m_Nodes.Count( ) && m_CtrlToNode.Count( ) == m_Nodes.Count( ) )
		{
			for ( int i = 0; i < m_Nodes.Count( ); ++i )
			{
				if ( m_NodeToCtrl[ i ] != i || m_CtrlToNode[ i ] != i )
					return;
			}
			// we already have de facto identity order
			m_bIdentityCtrlOrder = true;
		}
	}
}


bool CFeModelBuilder::HasLegacyStretchForce( ) const
{
	for ( int i = 0; i < m_Nodes.Count( ); ++i )
	{
		if ( m_Nodes[ i ].bSimulated && m_Nodes[ i ].flLegacyStretchForce != 0 )
		{
			return true;
		}
	}
	return false;
}



template <typename Allocator >
void CFeModelBuilder::OnAllocateMultiBuffer( Allocator &subAlloc, MbaContext_t &context )
{
	uint nDynCount = GetDynamicNodeCount();
	subAlloc( m_pSimdRods, context.simdRods );
	if ( subAlloc( m_pSimdQuads, m_nSimdQuadCount[ 0 ] ) )
	{
		for ( uint nSimdQuad = 0; nSimdQuad < m_nSimdQuadCount[ 2 ]; ++nSimdQuad )
		{
			m_pSimdQuads[ nSimdQuad ] = context.simdQuads[ 2 ][ nSimdQuad ];
		}
		for ( uint nSimdQuad = m_nSimdQuadCount[ 2 ]; nSimdQuad < m_nSimdQuadCount[ 1 ]; ++nSimdQuad )
		{
			m_pSimdQuads[ nSimdQuad ] = context.simdQuads[ 1 ][ nSimdQuad - m_nSimdQuadCount[ 2 ] ];
		}
		for ( uint nSimdQuad = m_nSimdQuadCount[ 1 ]; nSimdQuad < m_nSimdQuadCount[ 0 ]; ++nSimdQuad )
		{
			m_pSimdQuads[ nSimdQuad ] = context.simdQuads[ 0 ][ nSimdQuad - m_nSimdQuadCount[ 1 ] ];
		}
	}
	if ( subAlloc( m_pSimdTris, m_nSimdTriCount[ 0 ] ) )
	{
		for ( uint nSimdTri = 0; nSimdTri < m_nSimdTriCount[ 2 ]; ++nSimdTri )
		{
			m_pSimdTris[ nSimdTri ] = context.simdTris[ 2 ][ nSimdTri ];
		}
		for ( uint nSimdTri = m_nSimdTriCount[ 2 ]; nSimdTri < m_nSimdTriCount[ 1 ]; ++nSimdTri )
		{
			m_pSimdTris[ nSimdTri ] = context.simdTris[ 1 ][ nSimdTri - m_nSimdTriCount[ 2 ] ];
		}
		for ( uint nSimdTri = m_nSimdTriCount[ 1 ]; nSimdTri < m_nSimdTriCount[ 0 ]; ++nSimdTri )
		{
			m_pSimdTris[ nSimdTri ] = context.simdTris[ 0 ][ nSimdTri - m_nSimdTriCount[ 1 ] ];
		}
	}
	subAlloc( m_pSimdNodeBases, context.simdBases ); // must be aligned
	subAlloc( m_pSimdSpringIntegrator, context.simdSprings );
	subAlloc( m_pFitMatrices, context.fitMatrices );
	if ( subAlloc( m_pInitPose, m_nCtrlCount ) )
	{
		for ( uint nCtrl = 0; nCtrl < m_nCtrlCount; ++nCtrl )
		{
			m_pInitPose[ nCtrl ] = m_CtrlToNode[ nCtrl ] >= 0 ? m_Nodes[ m_CtrlToNode[ nCtrl ] ].transform : g_TransformIdentity;
		}
	}
	subAlloc( m_pCtrlName, context.nCtrlNameCount );
	subAlloc( m_pNodeBases, m_NodeBases );
	subAlloc( m_pReverseOffsets, m_ReverseOffsets );
	subAlloc( m_pCtrlOffsets, m_CtrlOffsets );
	subAlloc( m_pCtrlOsOffsets, m_CtrlOsOffsets );
	subAlloc( m_pFollowNodes, context.nodeFollowers );
	uint32 *pCtrlHash = subAlloc( m_pCtrlHash, context.nCtrlNameCount );
	subAlloc( m_pQuads, context.quads );
	subAlloc( m_pRods, m_Rods );
	subAlloc( m_pTris, context.tris );

	subAlloc( m_pAxialEdges, m_AxialEdges );
	if ( subAlloc( m_pNodeIntegrator, context.nNodeIntegratorCount ) )
	{
		for ( uint nNode = 0; nNode < context.nNodeIntegratorCount; ++nNode )
		{
			m_pNodeIntegrator[ nNode ] = m_Nodes[ nNode ].integrator;
			if ( nNode < m_nStaticNodes )
			{
				m_nStaticNodeFlags |= m_Nodes[ nNode ].GetDampingFlags( );
				// note: flAnimationForceAttraction seemingly isn't used in source1 on static nodes, no need to premultiply
			}
			else
			{
				// <sergiy> Important fix: the springs (particularly animation force attraction) in Source1 were tuned to the "mass" of cloth particles
				//          Source2 computes accelerations directly, so we need to pre-divide by mass here. I forgot to do that in CL 2322760, fixed in 2532503.
				// <sergiy> turns out I do predivide flAnimationForceAttraction by mass when importing CAuthClothParser::ParseLegacyDotaNodeGrid, so no need to do that here again.
				// m_pNodeIntegrator[ nNode ].flAnimationForceAttraction *= m_Nodes[ nNode ].invMass;
				m_nDynamicNodeFlags |= m_Nodes[ nNode ].GetDampingFlags();
			}
		}
	}
	subAlloc( m_pSpringIntegrator, context.springs );
	if ( subAlloc( m_pNodeInvMasses, m_nNodeCount ) )
	{
		for ( int nNode = 0; nNode < m_Nodes.Count( ); ++nNode )
		{
			m_pNodeInvMasses[ nNode ] = m_Nodes[ nNode ].invMass;
		}
	}

	subAlloc( m_pCollisionSpheres, context.collisionSpheres );
	subAlloc( m_pCollisionPlanes, context.collisionPlanes );

	subAlloc( m_pFitWeights, context.fitWeights );
	
	if ( context.m_bHasNodeCollisionRadii )
	{
		if ( subAlloc( m_pNodeCollisionRadii, GetDynamicNodeCount() ) )
		{
			AssertDbg( m_Nodes.Count() == int( m_nNodeCount ) );
			for ( uint nNode = m_nStaticNodes; nNode < m_nNodeCount; ++nNode )
			{
				m_pNodeCollisionRadii[ nNode - m_nStaticNodes ] = m_Nodes[ nNode ].flCollisionRadius;
			}
		}
	}
	else
	{
		m_pNodeCollisionRadii = NULL;
	}

	if ( context.m_bUsePerNodeLocalForce )
	{
		if ( subAlloc( m_pLocalForce, GetDynamicNodeCount() ) )
		{
			AssertDbg( m_Nodes.Count() == int( m_nNodeCount ) );
			for ( uint nNode = m_nStaticNodes; nNode < m_nNodeCount; ++nNode )
			{
				m_pLocalForce[ nNode - m_nStaticNodes ] = m_Nodes[ nNode ].flLocalForce;
			}
		}
	}
	if ( context.m_bUsePerNodeLocalRotation )
	{
		if ( subAlloc( m_pLocalRotation, GetDynamicNodeCount() ) )
		{
			AssertDbg( m_Nodes.Count() == int( m_nNodeCount ) );
			for ( uint nNode = m_nStaticNodes; nNode < m_nNodeCount; ++nNode )
			{
				m_pLocalRotation[ nNode - m_nStaticNodes ] = m_Nodes[ nNode ].flLocalRotation;
			}
		}
	}

	subAlloc( m_pWorldCollisionParams, context.worldCollisionParams );
	subAlloc( m_pTaperedCapsuleStretches, m_TaperedCapsuleStretches );
	subAlloc( m_pTaperedCapsuleRigids, m_TaperedCapsuleRigids );
	subAlloc( m_pSphereRigids, m_SphereRigids );

	if ( subAlloc( m_pLegacyStretchForce, context.nLegacyStretchForceCount ) )
	{
		for ( uint i = 0; i < context.nLegacyStretchForceCount; ++i )
		{
			m_pLegacyStretchForce[ i ] = m_Nodes[ i ].bSimulated ? m_Nodes[ i ].flLegacyStretchForce/* * m_Nodes[ i ].invMass*/ : 0.0f;
		}
	}

	if ( !m_bIdentityCtrlOrder )
	{
		if ( subAlloc( m_pCtrlToNode, m_CtrlToNode.Count( ) ) )
		{
			for ( int i = 0; i < m_CtrlToNode.Count( ); ++i )
			{
				m_pCtrlToNode[ i ] = m_CtrlToNode[ i ];
			}
		}
		if ( subAlloc( m_pNodeToCtrl, m_NodeToCtrl.Count( ) ) )
		{
			for ( int i = 0; i < m_NodeToCtrl.Count( ); ++i )
			{
				m_pNodeToCtrl[ i ] = m_NodeToCtrl[ i ];
			}
		}
	}
	else
	{
		// these get assigned NULL twice..
		m_pCtrlToNode = NULL;
		m_pNodeToCtrl = NULL;
	}

	subAlloc( m_pTreeChildren, m_TreeChildren );
	subAlloc( m_pTreeParents, m_TreeParents );
	if ( subAlloc( m_pTreeCollisionMasks, m_TreeParents.Count() ) )
	{
		V_memset( m_pTreeCollisionMasks, 0, sizeof( *m_pTreeCollisionMasks ) * ( 2 * nDynCount - 1 ) );
		for ( uint i = 0; i < nDynCount; ++i )
		{
			m_pTreeCollisionMasks[ i ] = m_Nodes[ i + m_nStaticNodes ].nCollisionMask;
		}
		// propagate the mask to parents
		for ( uint i = 0; i < 2 * nDynCount - 2; ++i )
		{
			m_pTreeCollisionMasks[ m_TreeParents[ i ] ] |= m_pTreeCollisionMasks[ i ];
		}
	}

	subAlloc( m_pWorldCollisionNodes, context.worldCollisionNodes );
	subAlloc( m_pFreeNodes, m_FreeNodes );

	if ( subAlloc( m_pRopes, m_nRopeIndexCount ) )
	{
		uint nRopeRunningIndex = m_Ropes.Count( );
		for ( int nRope = 0; nRope < m_Ropes.Count( ); ++nRope )
		{
			for ( int nLink = 0; nLink < m_Ropes[ nRope ]->Count( ); ++nLink )
			{
				m_pRopes[ nRopeRunningIndex++ ] = m_Ropes[ nRope ]->Element( nLink );
			}
			m_pRopes[ nRope ] = nRopeRunningIndex;
		}
		Assert( nRopeRunningIndex == m_nRopeIndexCount );
	}

	for ( uint nCtrl = 0; nCtrl < context.nCtrlNameCount; ++nCtrl )
	{
		int nNode = m_CtrlToNode[ nCtrl ];
		subAlloc.String( m_pCtrlName[ nCtrl ], nNode >= 0 ? m_Nodes[ nNode ].pName : NULL );
	}

	if ( pCtrlHash )
	{
		for ( uint nCtrl = 0; nCtrl < context.nCtrlNameCount; ++nCtrl )
		{
			const char *pName = m_pCtrlName[ nCtrl ];
			pCtrlHash[ nCtrl ] = pName ? MakeStringToken( pName ).m_nHashCode : 0;
		}
	}
}

float CFeModelBuilder::ElemNormalLength( const uint nNode[ 4 ] )
{
	Vector p0 = m_Nodes[ nNode[ 0 ] ].transform.m_vPosition;
	Vector p1 = m_Nodes[ nNode[ 1 ] ].transform.m_vPosition;
	Vector p2 = m_Nodes[ nNode[ 2 ] ].transform.m_vPosition;
	Vector p3 = m_Nodes[ nNode[ 3 ] ].transform.m_vPosition;
	Vector vNormal = CrossProduct( p2 - p0, p3 - p1 );
	return vNormal.Length();
}

Vector CFeModelBuilder::TriNormal( uint nNode0, uint nNode1, uint nNode2 )
{
	Vector p0 = m_Nodes[ nNode0 ].transform.m_vPosition;
	Vector p1 = m_Nodes[ nNode1 ].transform.m_vPosition;
	Vector p2 = m_Nodes[ nNode2 ].transform.m_vPosition;

	Vector vNormal = CrossProduct( p1 - p0, p2 - p0 );
	return vNormal;
}

float CFeModelBuilder::NodeDist( uint nNode0, uint nNode1 )
{
	const Vector &p0 = m_Nodes[ nNode0 ].transform.m_vPosition;
	const Vector &p1 = m_Nodes[ nNode1 ].transform.m_vPosition;
	return ( p0 - p1 ).Length();
}


void CFeModelBuilder::AdjustQuads()
{
	CUtlVector< BuildElem_t > append;
	// convert self-intersecting quads to nice-looking quads
	for ( int i = 0; i < m_Elems.Count(); ++i )
	{
		BuildElem_t &elem = m_Elems[ i ];
		if ( elem.NumNodes() < 4 )
		{
			// nothing to adjust
			continue;
		}

		switch( elem.nStaticNodes )
		{
		case 2:
			{ // just a nicety: if we can, let's make this into a convex quad. it's not necessary (and impossible in case of diagonal statics)
				Vector e0 = m_Nodes[ elem.nNode[ 1 ] ].transform.m_vPosition - m_Nodes[ elem.nNode[ 0 ] ].transform.m_vPosition;
				Vector e2 = m_Nodes[ elem.nNode[ 2 ] ].transform.m_vPosition - m_Nodes[ elem.nNode[ 3 ] ].transform.m_vPosition;
				if ( DotProduct( e2, e0 ) < 0 )
				{
					Swap( elem.nNode[ 3 ], elem.nNode[ 2 ] );
				}
			}
			break;

		case 1:
		case 0:
			{
				float flNoPermValue = ElemNormalLength( elem.nNode );
				uint nPerm1[ 4 ] = { elem.nNode[ 0 ], elem.nNode[ 2 ], elem.nNode[ 3 ], elem.nNode[ 1 ] };
				uint nPerm2[ 4 ] = { elem.nNode[ 0 ], elem.nNode[ 3 ], elem.nNode[ 1 ], elem.nNode[ 2 ] };
				float flPerm1Value = ElemNormalLength( nPerm1 );
				float flPerm2Value = ElemNormalLength( nPerm2 );
				uint *pBestPerm = NULL;
				if ( flPerm1Value > flPerm2Value )
				{
					if ( flPerm1Value > flNoPermValue )
					{
						pBestPerm = nPerm1;
					}
				}
				else
				{
					if ( flPerm2Value > flNoPermValue )
					{
						pBestPerm = nPerm2;
					}
				}
				if ( pBestPerm )
				{
					elem.nNode[ 0 ] = pBestPerm[ 0 ];
					elem.nNode[ 1 ] = pBestPerm[ 1 ];
					elem.nNode[ 2 ] = pBestPerm[ 2 ];
					elem.nNode[ 3 ] = pBestPerm[ 3 ];
				}
			}
			break;
		}

		if ( m_flQuadBendTolerance < 1.0f && elem.nStaticNodes == 0 && elem.nNode[ 2 ] != elem.nNode[ 3 ] )
		{
			// check if the quad is bent too much, and triangulate if so
			if ( NodeDist( elem.nNode[ 0 ], elem.nNode[ 2 ] ) > NodeDist( elem.nNode[ 1 ], elem.nNode[ 3 ] ) )
			{
				// try to make it so that triangulation (0,1,2) (2,3,0) produces the shortest diagonal (0,2)
				uint nWasNode0 = elem.nNode[ 0 ];
				elem.nNode[ 0 ] = elem.nNode[ 1 ];
				elem.nNode[ 1 ] = elem.nNode[ 2 ];
				elem.nNode[ 2 ] = elem.nNode[ 3 ];
				elem.nNode[ 3 ] = nWasNode0;
			}

			// how much is it bent? compare sine of the bend angle with the limit
			Vector n0 = TriNormal( elem.nNode[ 0 ], elem.nNode[ 1 ], elem.nNode[ 2 ] );
			Vector n1 = TriNormal( elem.nNode[ 2 ], elem.nNode[ 3 ], elem.nNode[ 0 ] );
			float flSinBendF = CrossProduct( n0, n1 ).Length(), flF = n0.Length() * n1.Length();
			if ( flSinBendF > m_flQuadBendTolerance * flF )
			{
				// the bend is too great. Split the quad
				BuildElem_t newElem = elem; // new element is (0,2,3,3)
				newElem.nNode[ 1 ] = newElem.nNode[ 2 ];
				newElem.nNode[ 2 ] = newElem.nNode[ 3 ];
				append.AddToTail( newElem );

				// convert old elem to (0,1,2,2) triangle
				elem.nNode[ 3 ] = elem.nNode[ 2 ];
			}
		}
	}

	m_Elems.AddMultipleToTail( append.Count(), append.Base() );
}


bool CFeModelBuilder::Finish( bool bTriangulate, float flAddCurvature, float flSlackMultiplier )
{
	//Schedule( , m_Elems.Base(), 4 ); NOTE_UNUSED( nElemSlack );
	MbaContext_t context;

	BuildInvMassesAndSortNodes( );
	m_nNodeCount = m_Nodes.Count();
	m_nCtrlCount = m_CtrlToNode.Count();
	
	AdjustQuads();

	BuildTree();
	CheckIdentityCtrlOrder( );
	BuildNodeBases( ); // all node bases are built; even those we won't need later
	
	BuildRopes();
	ValidateBases( );

	BuildFeEdgeDesc( );
	BuildNodeSlack( flSlackMultiplier );
	BuildAndSortRods( clamp( flAddCurvature, 0.0f, 1.0f ) * M_PI, bTriangulate );

	//BuildKelagerBends();
	if ( m_bRigidEdgeHinges )
	{
		BuildAxialEdges();
	}

	RemoveFullyStaticElems(); // we don't need fully static quads anymore

	if ( bTriangulate )
	{
		BuildTris( context.tris, true );
	}
	else
	{
		BuildTris( context.tris, false );
		BuildQuads( context.quads, true );
	}

	Schedule( &context.simdRods, m_Rods.Base( ), m_Rods.Count( ), 4 );

	Schedule( &context.simdQuads[ 0 ], context.quads.Base( ) + m_nQuadCount[ 1 ], m_nQuadCount[ 0 ] - m_nQuadCount[ 1 ], 4 );
	Schedule( &context.simdQuads[ 1 ], context.quads.Base( ) + m_nQuadCount[ 2 ], m_nQuadCount[ 1 ] - m_nQuadCount[ 2 ], 4 );
	Schedule( &context.simdQuads[ 2 ], context.quads.Base( ), m_nQuadCount[ 2 ], 4 );
	m_nSimdQuadCount[ 0 ] = context.simdQuads[ 2 ].Count( ) + context.simdQuads[ 1 ].Count( ) + context.simdQuads[ 0 ].Count( );
	m_nSimdQuadCount[ 1 ] = context.simdQuads[ 2 ].Count( ) + context.simdQuads[ 1 ].Count( );
	m_nSimdQuadCount[ 2 ] = context.simdQuads[ 2 ].Count( );

	Schedule( &context.simdTris[ 0 ], context.tris.Base( ) + m_nTriCount[ 1 ], m_nTriCount[ 0 ] - m_nTriCount[ 1 ], 4 );
	Schedule( &context.simdTris[ 1 ], context.tris.Base( ) + m_nTriCount[ 2 ], m_nTriCount[ 1 ] - m_nTriCount[ 2 ], 4 );
	Schedule( &context.simdTris[ 2 ], context.tris.Base( ), m_nTriCount[ 2 ], 4 );
	m_nSimdTriCount[ 0 ] = context.simdTris[ 2 ].Count( ) + context.simdTris[ 1 ].Count( ) + context.simdTris[ 0 ].Count( );
	m_nSimdTriCount[ 1 ] = context.simdTris[ 2 ].Count( ) + context.simdTris[ 1 ].Count( );
	m_nSimdTriCount[ 2 ] = context.simdTris[ 2 ].Count( );

	Schedule( &context.simdBases, m_NodeBases.Base( ), m_NodeBases.Count( ), 4 );

	BuildSprings( context.springs );
	Schedule( &context.simdSprings, context.springs.Base( ), context.springs.Count( ), 4 );

	BuildCtrlOffsets( );

	BuildNodeFollowers( context.nodeFollowers );

	context.nCollisionEllipsoidsInclusive = BuildCollisionSpheres( context.collisionSpheres );
	BuildCollisionPlanes( context.collisionPlanes );

	BuildWorldCollisionNodes( context.worldCollisionParams, context.worldCollisionNodes );
	BuildFitMatrices( context );
	if ( m_bNeedBacksolvedBasesOnly )
		RemoveStandaloneNodeBases( context );
	BuildFreeNodes( context );

	if ( HasLegacyStretchForce( ) )
	{
		context.nLegacyStretchForceCount = m_Nodes.Count();
	}
	else
	{
		context.nLegacyStretchForceCount = 0;
	}

	context.m_bHasNodeCollisionRadii = false;
	for ( uint i = m_nStaticNodes; i < m_nNodeCount; ++i )
	{
		const BuildNode_t &node = m_Nodes[ i ];
		if ( node.flCollisionRadius != 0 )
		{
			context.m_bHasNodeCollisionRadii = true;
			break;
		}
	}

	context.m_bUsePerNodeLocalRotation = false;
	context.m_bUsePerNodeLocalForce = false;
	if ( m_bUsePerNodeLocalForceAndRotation )
	{
		if ( m_nNodeCount > m_nStaticNodes )
		{
			m_flLocalForce = m_Nodes[ m_nStaticNodes ].flLocalForce; // find the max local force/rotation values and stick them into defaults
			m_flLocalRotation = m_Nodes[ m_nStaticNodes ].flLocalRotation;
			for ( uint i = m_nStaticNodes + 1; i < m_nNodeCount; ++i )
			{
				const BuildNode_t &node = m_Nodes[ i ];
				if ( node.flLocalRotation != m_flLocalRotation )
				{
					context.m_bUsePerNodeLocalRotation = true;
					m_flLocalRotation = Max( m_flLocalRotation, node.flLocalRotation );
				}																	
				if ( node.flLocalForce != m_flLocalForce )
				{
					context.m_bUsePerNodeLocalForce = true;
					m_flLocalForce = Max( m_flLocalForce, node.flLocalForce );
				}
			}

			if ( !( m_flLocalForce > 0 ) )
			{
				m_flLocalForce = 0;
				context.m_bUsePerNodeLocalForce = false;
			}
			if ( !( m_flLocalRotation > 0 ) )
			{
				m_flLocalRotation = 0;
				context.m_bUsePerNodeLocalRotation = false;
			}
		}
	}

	// m_nQuadCount[0] = m_Elems.Count( ); // already assigned in BuildQuads
	m_nRodCount = m_Rods.Count( );
	m_nSimdRodCount = context.simdRods.Count( );
	m_nAxialEdgeCount = m_AxialEdges.Count( );
	m_nRopeCount = m_Ropes.Count( );
	m_nRopeIndexCount = RopeIndexCount( m_Ropes );
	m_nNodeBaseCount = m_NodeBases.Count( );
	m_nSimdNodeBaseCount = context.simdBases.Count( );
	m_nSpringIntegratorCount = context.springs.Count( );
	m_nSimdSpringIntegratorCount = context.simdSprings.Count( );
	m_nCtrlOffsets = m_CtrlOffsets.Count();
	m_nCtrlOsOffsets = m_CtrlOsOffsets.Count( );
	m_nFollowNodeCount = context.nodeFollowers.Count( );
	m_nCollisionPlanes = context.collisionPlanes.Count( );
	m_nCollisionSpheres[ 0 ] = context.collisionSpheres.Count( );
	m_nCollisionSpheres[ 1 ] = context.nCollisionEllipsoidsInclusive;
	m_nWorldCollisionNodeCount = context.worldCollisionNodes.Count( );
	m_nWorldCollisionParamCount = context.worldCollisionParams.Count( );
	m_nFitWeightCount = context.fitWeights.Count();
	m_nFitMatrixCount[ 0 ] = context.fitMatrices.Count();
	m_nFitMatrixCount[ 1 ] = context.m_nFitMatrices1;
	m_nFitMatrixCount[ 2 ] = context.m_nFitMatrices2;
	m_nReverseOffsetCount = m_ReverseOffsets.Count();

	m_nTaperedCapsuleStretchCount = m_TaperedCapsuleStretches.Count();
	m_nTaperedCapsuleRigidCount = m_TaperedCapsuleRigids.Count();
	m_nSphereRigidCount = m_SphereRigids.Count();
	m_nFreeNodeCount = m_FreeNodes.Count();
	context.nNodeIntegratorCount = GetDampingFlags( ) ? m_nNodeCount : 0;
	
	context.nStringsMemSize = 0;
	context.nCtrlNameCount = 0; // if there are no names given, there's no reason to create the array of ctrl names (or their hashes)
	for ( int nCtrl = 0; nCtrl < m_CtrlToNode.Count( ); ++nCtrl )
	{
		int nNode = m_CtrlToNode[ nCtrl ];
		if ( nNode >= 0 )
		{
			const char *pName = m_Nodes[ nNode ].pName;
			if ( pName && *pName )
			{
				// there's a non-empty name
				context.nStringsMemSize += V_strlen( pName ) + 1;
				context.nCtrlNameCount = m_nCtrlCount;
			}
		}
	}

	if ( false ) // sorting is not strictly necessary and it obscures debugging
	{
		HeapSort( m_TaperedCapsuleStretches, []( const FeTaperedCapsuleStretch_t &left, const FeTaperedCapsuleStretch_t &right ) {
			if ( left.nNode[ 1 ] != right.nNode[ 1 ] )
				return left.nNode[ 1 ] < right.nNode[ 1 ];
			if ( left.nNode[ 0 ] != right.nNode[ 0 ] )
				return left.nNode[ 0 ] < right.nNode[ 0 ];
			if ( left.flRadius[ 1 ] != right.flRadius[ 1 ] )
				return left.flRadius[ 1 ] > right.flRadius[ 1 ];
			if ( left.flRadius[ 0 ] != right.flRadius[ 0 ] )
				return left.flRadius[ 0 ] > right.flRadius[ 0 ];
			return false;
		} );
		RemoveDuplicates( m_TaperedCapsuleStretches );
		HeapSort( m_TaperedCapsuleRigids, []( const FeTaperedCapsuleRigid_t &left, const FeTaperedCapsuleRigid_t &right ) {
			return left.nNode < right.nNode;
		} );
		HeapSort( m_SphereRigids, []( const FeSphereRigid_t & left, const FeSphereRigid_t &right ) {
			return left.nNode < right.nNode;
		} );
	}
	ReallocateMultiBuffer( context );

	if ( !m_bUnitlessDamping )
	{
		// make a pass over integrators - the dynamic node damping is defined w.r.t. mass outside of the FE model builder. But inside Fe model simulator, nodes are massless (only mass ratios are used)
		// so we need to premultiply some entities by invMass
		for ( uint i = m_nStaticNodes; i < context.nNodeIntegratorCount; ++i )
		{
			m_pNodeIntegrator[ i ].flPointDamping *= m_Nodes[ i ].invMass;
		}
	}
	m_nTreeDepth = ComputeCollisionTreeDepthTopDown();

#ifdef DBGFLAG_ASSERTDEBUG
	if ( m_nQuadCount[ 0 ] == m_Elems.Count() )
	{
		for ( uint nCtrl = 0; nCtrl < m_nQuadCount[ 2 ]; ++nCtrl )
		{
			Assert( m_Elems[ nCtrl ].nStaticNodes == 2 );
		}

		for ( uint nCtrl = m_nQuadCount[ 2 ]; nCtrl < m_nQuadCount[ 1 ]; ++nCtrl )
		{
			Assert( m_Elems[ nCtrl ].nStaticNodes == 1 );
		}
		for ( uint nCtrl = m_nQuadCount[ 1 ]; nCtrl < m_nQuadCount[ 0 ]; ++nCtrl )
		{
			Assert( m_Elems[ nCtrl ].nStaticNodes == 0 );
		}
	}

	for ( uint nQuad = 0; nQuad < m_nQuadCount[ 2 ]; ++nQuad )
	{
		Assert( m_pQuads[ nQuad ].nNode[ 0 ] < m_nStaticNodes && m_pQuads[ nQuad ].nNode[ 1 ] < m_nStaticNodes && m_pQuads[ nQuad ].nNode[ 2 ] >= m_nStaticNodes && m_pQuads[ nQuad ].nNode[ 3 ] >= m_nStaticNodes );
	}

	for ( uint nQuad = m_nQuadCount[ 2 ]; nQuad < m_nQuadCount[ 1 ]; ++nQuad )
	{
		Assert( m_pQuads[ nQuad ].nNode[ 0 ] < m_nStaticNodes && m_pQuads[ nQuad ].nNode[ 1 ] >= m_nStaticNodes && m_pQuads[ nQuad ].nNode[ 2 ] >= m_nStaticNodes && m_pQuads[ nQuad ].nNode[ 3 ] >= m_nStaticNodes );
	}
	for ( uint nQuad = m_nQuadCount[ 1 ]; nQuad < m_nQuadCount[ 0 ]; ++nQuad )
	{
		Assert( m_pQuads[ nQuad ].nNode[ 0 ] >= m_nStaticNodes && m_pQuads[ nQuad ].nNode[ 1 ] >= m_nStaticNodes && m_pQuads[ nQuad ].nNode[ 2 ] >= m_nStaticNodes && m_pQuads[ nQuad ].nNode[ 3 ] >= m_nStaticNodes );
	}


	Assert( m_nQuadCount[ 0 ] >= m_nQuadCount[ 1 ] && m_nQuadCount[ 1 ] >= m_nQuadCount[ 2 ] );

	for ( uint nRod = 0; nRod < m_nRodCount; ++nRod )
	{
		AssertDbg( Is0To1( m_pRods[ nRod ].flWeight0 ) && Is0To1( m_pRods[ nRod ].flRelaxationFactor ) );
	}
#endif

	return true;
}

void CFeModelBuilder::BuildTree()
{
	uint nDynNodeCount = GetDynamicNodeCount();
	if ( nDynNodeCount < 2 )
		return;
	uint nStaticBaseIndex = m_nStaticNodes;

	CFeAgglomerator agg( nDynNodeCount );
	for ( uint nDynNode = 0; nDynNode < nDynNodeCount; ++nDynNode )
	{	
		agg.SetNode( nDynNode, m_Nodes[ nDynNode + nStaticBaseIndex ].transform.m_vPosition );
	}
	for ( int nRod = 0; nRod < m_Rods.Count(); ++nRod )
	{
		const FeRodConstraint_t& rod = m_Rods[ nRod ];
		if ( rod.nNode[ 0 ] >= nStaticBaseIndex && rod.nNode[ 1 ] >= nStaticBaseIndex )
		{
			agg.LinkNodes( rod.nNode[ 0 ] - nStaticBaseIndex, rod.nNode[ 1 ] - nStaticBaseIndex );
		}
	}
	for ( int nElemIdx = 0; nElemIdx < m_Elems.Count(); ++nElemIdx )
	{
		const BuildElem_t& elem = m_Elems[ nElemIdx ];
		if ( elem.nStaticNodes < 3 )
		{
			for ( int nElemIndex = elem.nStaticNodes + 1; nElemIndex < ARRAYSIZE( elem.nNode ); ++nElemIndex )
			{
				agg.LinkNodes( elem.nNode[ nElemIndex ] - nStaticBaseIndex, elem.nNode[ nElemIndex - 1 ] - nStaticBaseIndex );
			}
			if ( elem.nStaticNodes == 0 )
			{
				agg.LinkNodes( elem.nNode[ 0 ] - nStaticBaseIndex, elem.nNode[ ARRAYSIZE( elem.nNode ) - 1 ] - nStaticBaseIndex );
			}
		}
	}

	agg.Build( true );

	Assert( agg.GetClusterCount() == int( nDynNodeCount * 2 - 1 ) );
	
	m_TreeParents.SetCount( agg.GetClusterCount() );
	for ( int nDynNode = 0; nDynNode < agg.GetClusterCount(); ++nDynNode )
	{
		m_TreeParents[ nDynNode ] = agg.GetCluster( nDynNode )->m_nParent;
	}

	m_TreeChildren.SetCount( agg.GetClusterCount() - nDynNodeCount );
	for ( int nDynNode = nDynNodeCount; nDynNode < agg.GetClusterCount(); ++nDynNode )
	{
		for ( int nChildIndex = 0; nChildIndex < 2; ++nChildIndex )
		{
			uint nChild = agg.GetCluster( nDynNode )->m_nChild[ nChildIndex ];
			m_TreeChildren[ nDynNode - nDynNodeCount ].nChild[ nChildIndex ] = nChild;
			Assert( agg.GetCluster( nChild )->m_nParent == nDynNode );
		}
	}

}

int CFeModelBuilder::FindBuildNodeIndex( const char *pName )
{
	for ( int i = 0; i < m_Nodes.Count(); ++i )
	{
		if ( !V_stricmp( m_Nodes[ i ].pName, pName ) )
		{
			return i;
		}
	}
	return -1;
}