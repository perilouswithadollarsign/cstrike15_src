//=========== Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Mesh class UV parameterization operations.
//
//===========================================================================//
#include "mesh.h"
#include "tier1/utlbuffer.h"

Vector4D PlaneFromTriangle( Vector &A, Vector &B, Vector &C )
{
	// Calculate normal
	Vector vAB = B - A;
	Vector vAC = C - A;
	Vector vNorm = -CrossProduct( vAC, vAB );
	vNorm.NormalizeInPlace();

	float d = DotProduct( A, vNorm );
	return Vector4D( vNorm.x, vNorm.y, vNorm.z, d );
}

Vector4D CMesh::PlaneFromTriangle( int nTriangle ) const
{
	Vector A = *(Vector*)GetVertex( m_pIndices[ nTriangle * 3     ] );
	Vector B = *(Vector*)GetVertex( m_pIndices[ nTriangle * 3 + 1 ] );
	Vector C = *(Vector*)GetVertex( m_pIndices[ nTriangle * 3 + 2 ] );

	return ::PlaneFromTriangle( A, B, C );
}

int AddTriangleToChart( const CMesh &inputMesh, int nTriangle, UVChart_t *pChart, float flThreshold, CUtlVector<bool> &usedTriangles, int* pAdjacency )
{
	if ( usedTriangles[ nTriangle ] == true )
		return 0;

	Vector4D vTriPlane = inputMesh.PlaneFromTriangle( nTriangle );
	float flDot = DotProduct( vTriPlane.AsVector3D(), pChart->m_vPlane.AsVector3D() );
	if ( flDot < flThreshold )
	{
		return 0;
	}

	// Add this triangle to the chart
	pChart->m_TriangleList.AddToTail( nTriangle );
	usedTriangles[ nTriangle ] = true;

	int nAdded = 1;

	// Add any adjacent triangles to the chart
	for ( int i=0; i<3; ++i )
	{
		int nAdj = pAdjacency[ nTriangle * 3 + i ];
		if ( nAdj != -1)
		{
			nAdded += AddTriangleToChart( inputMesh, nAdj, pChart, flThreshold, usedTriangles, pAdjacency );
		}
	}

	return nAdded;
}

//--------------------------------------------------------------------------------------
// CreateUniqueUVParameterization
// 
// Creates a unique parameterization for the mesh in 0..1 UV space.  The mesh is assumed
// to be welded and clean before this is called.
//--------------------------------------------------------------------------------------
bool CreateUniqueUVParameterization( CMesh *pMeshOut, const CMesh &inputMesh, float flThreshold, int nAtlasTextureSizeX, int nAtlasTextureSizeY, float flGutterSize )
{
	int nMaxCharts = 10000;
	
	// Generate adjacency
	int *pAdjacencyBuffer = new int[ inputMesh.m_nIndexCount ];
	if ( !inputMesh.CalculateAdjacency( pAdjacencyBuffer, inputMesh.m_nIndexCount ) )
	{
		return false;
	}

	int nPosOffset = inputMesh.FindFirstAttributeOffset( VERTEX_ELEMENT_POSITION );
	int nTexOffset = inputMesh.FindFirstAttributeOffset( VERTEX_ELEMENT_TEXCOORD2D_0 );
	if ( nTexOffset == -1 )
	{
		nTexOffset = inputMesh.FindFirstAttributeOffset( VERTEX_ELEMENT_TEXCOORD3D_0 );
	}
	if ( nTexOffset == -1 || nPosOffset == -1)
	{
		Warning( "Cannot create UV parameterization without position or texcoords!\n" );
		return false;
	}

	// Now go through the triangles and add them to charts based upon minimum angle between charts
	CUtlVector<int> triangleIndices;
	CUtlVector<bool> usedTriangles;
	CUtlVector<UVChart_t*> chartList;
	int nTris = inputMesh.m_nIndexCount / 3;
	triangleIndices.EnsureCount( nTris );
	usedTriangles.EnsureCount( nTris );
	for( int t=0; t<nTris; ++t )
	{
		triangleIndices[t] = t;
		usedTriangles[t] = false;
	}

	bool *pUsedTriangles = usedTriangles.Base();

	int nRemaining = nTris;
	while( nRemaining > 0 )
	{
		// Select a random triangle
		int nTriangle = -1;
		for( int t=0; t<nTris; ++t )
		{
			if ( pUsedTriangles[t] == false )
			{
				nTriangle = t;
				break;
			}
		}

		if ( nTriangle > -1 )
		{
			Vector4D vTriPlane = inputMesh.PlaneFromTriangle( nTriangle );

			if ( vTriPlane.AsVector3D().LengthSqr() < 0.9f )
			{
				// degenerate tri, just get rid of it
				pUsedTriangles[ nTriangle ] = true;
				nRemaining --;
			}
			else
			{
				// create a new chart
				UVChart_t *pNewChart = new UVChart_t;
				pNewChart->m_vPlane = vTriPlane;
				pNewChart->m_vMinUV = Vector2D( FLT_MAX, FLT_MAX );
				pNewChart->m_vMaxUV = Vector2D( -FLT_MAX, -FLT_MAX );

				int nAdded = AddTriangleToChart( inputMesh, nTriangle, pNewChart, flThreshold, usedTriangles, pAdjacencyBuffer );
				if ( nAdded < 1 )
				{
					Msg( "Error: didn't add any triangles to chart: %d\n", nTriangle );
				}
				nRemaining -= nAdded;

				// Add the chart to the list
				chartList.AddToTail( pNewChart );

				if ( chartList.Count() > nMaxCharts )
				{
					nRemaining = 0;
					break;
				}
			}
		}
		else
		{
			Assert( nRemaining == 0 );
			nRemaining = 0;
		}
	}


	delete []pAdjacencyBuffer;
	pAdjacencyBuffer = NULL;

	// create a local texture vector
	CUtlVector<AtlasChart_t> atlasChartVector;
	
	CMesh tempMesh;
	int nTotalChartTriangles = 0;
	int nCharts = chartList.Count();
	float flTotalArea = 0;
	if ( nCharts < nMaxCharts )
	{
		// Average each chart's plane and create a new texture for each chart
		int nCharts = chartList.Count();
		atlasChartVector.EnsureCount( nCharts );

		for ( int c=0; c<nCharts; ++c )
		{
			UVChart_t *pChart = chartList[c];

			int nTris = pChart->m_TriangleList.Count();
			Vector4D vPlane(0,0,0,0);
			for ( int p=0; p<nTris; ++p )
			{
				int iTri = pChart->m_TriangleList[p];
				vPlane += inputMesh.PlaneFromTriangle( iTri );
			}
			
			nTotalChartTriangles += nTris;

			Vector vNorm = vPlane.AsVector3D().Normalized();
			pChart->m_vPlane = Vector4D( vNorm.x, vNorm.y, vNorm.z, 0 );

			AtlasChart_t AtlasChart;
			AtlasChart.m_bAtlased = false;
			AtlasChart.m_vAtlasMin.Init( 0, 0 );
			AtlasChart.m_vAtlasMax.Init( 0, 0 );
			AtlasChart.m_vMaxTextureSize.Init( 0, 0 );
			atlasChartVector[ c ] = AtlasChart;
		}

		// Create a new temporary mesh
		tempMesh.AllocateMesh( nTotalChartTriangles * 3, nTotalChartTriangles * 3, inputMesh.m_nVertexStrideFloats, inputMesh.m_pAttributes, inputMesh.m_nAttributeCount );

		// loop through all charts and add the vertices and indices to the new mesh
		int nNewVertex = 0;
		flTotalArea = 0.0f;
		for ( int c=0; c<nCharts; ++c )
		{
			UVChart_t *pChart = chartList[c];

			Vector vNorm = pChart->m_vPlane.AsVector3D();
			Vector vUp(0,1,0);
			if ( DotProduct( vNorm, vUp ) > 0.95f )
				vUp = Vector(0,0,1);

			Vector vRight = CrossProduct( vNorm, vUp );
			vRight.NormalizeInPlace();
			vUp = CrossProduct( vRight, vNorm );
			vUp.NormalizeInPlace();

			pChart->m_nVertexStart = nNewVertex;

			// project the vertices onto the chart plane
			// and find the min and max plane boundaries
			int nTris = pChart->m_TriangleList.Count();
			for ( int t=0; t<nTris; ++t )
			{
				int iTri = pChart->m_TriangleList[t];

				for ( int i=0; i<3; ++i )
				{
					int nIndex = inputMesh.m_pIndices[ iTri * 3 + i ];
					float *pVert = (float*)inputMesh.GetVertex( nIndex );
					Vector &vPos = *( ( Vector* )( pVert + nPosOffset ) );

					Vector2D vTexcoord;
					vTexcoord.x = DotProduct( vPos, vRight );
					vTexcoord.y = DotProduct( vPos, vUp );

					pChart->m_vMinUV.x = MIN( vTexcoord.x, pChart->m_vMinUV.x );
					pChart->m_vMinUV.y = MIN( vTexcoord.y, pChart->m_vMinUV.y );
					pChart->m_vMaxUV.x = MAX( vTexcoord.x, pChart->m_vMaxUV.x );
					pChart->m_vMaxUV.y = MAX( vTexcoord.y, pChart->m_vMaxUV.y );

					float *pNewVert = tempMesh.GetVertex( nNewVertex );

					// New vertex
					CopyVertex( pNewVert, pVert, inputMesh.m_nVertexStrideFloats );
					Vector2D *pNewTex = (Vector2D*)( pNewVert + nTexOffset );
					*pNewTex = vTexcoord;

					// New index
					tempMesh.m_pIndices[ nNewVertex ] = nNewVertex;

					nNewVertex++;
					
				}
			}

			pChart->m_nVertexCount = nNewVertex - pChart->m_nVertexStart;

			// update size of the texture
			AtlasChart_t &chart = atlasChartVector[ c ];
			chart.m_vMaxTextureSize.x = pChart->m_vMaxUV.x - pChart->m_vMinUV.x;
			chart.m_vMaxTextureSize.y = pChart->m_vMaxUV.y - pChart->m_vMinUV.y;
			flTotalArea += chart.m_vMaxTextureSize.x * chart.m_vMaxTextureSize.y;

			Vector2D vChartUVDelta = pChart->m_vMaxUV - pChart->m_vMinUV;

			// Normalize texture coordinates within the chart plane boundaries
			if ( vChartUVDelta.x == 0 || vChartUVDelta.y == 0 )
			{
				// Zero texcoords if our chart is infintesimally small
				for ( int v=pChart->m_nVertexStart; v<pChart->m_nVertexStart + pChart->m_nVertexCount; ++v )
				{
					float *pNewVert = tempMesh.GetVertex( v );
					Vector2D *pNewTex = (Vector2D*)( pNewVert + nTexOffset );
					*pNewTex = Vector2D(0,0);
				}
			}
			else
			{
				for ( int v=pChart->m_nVertexStart; v<pChart->m_nVertexStart + pChart->m_nVertexCount; ++v )
				{
					float *pNewVert = tempMesh.GetVertex( v );
					Vector2D *pNewTex = (Vector2D*)( pNewVert + nTexOffset );

					Vector2D vNewTex = ( *pNewTex - pChart->m_vMinUV ) / vChartUVDelta;

					*pNewTex = vNewTex;
				}
			}

		}
	}

	bool bMadeAtlas = false;

	if ( nCharts < nMaxCharts )
	{
		// We made a chart
		bMadeAtlas = true;

		// Create an atlas
		Msg( "Attempting to atlas %d charts\n", nCharts );

		int nAtlasSideSize = (int)(sqrtf( flTotalArea ));
		int nGrowAmount = MAX( 8, nAtlasSideSize / 64 );
		nAtlasSideSize -= nGrowAmount * 2;

		PackChartsIntoAtlas( atlasChartVector.Base(), atlasChartVector.Count(), nAtlasSideSize, nAtlasSideSize, nGrowAmount );

		Vector2D vTextureSize( nAtlasTextureSizeX, nAtlasTextureSizeY );
		Vector2D vGutterOffset( flGutterSize / vTextureSize.x, flGutterSize / vTextureSize.y );

		// Update triangle coordinates to fit into this atlas
		for ( int c=0; c<nCharts; ++c )
		{
			UVChart_t *pChart = chartList[ c ];
			AtlasChart_t &atlasData = atlasChartVector[ c ];
			Vector2D vAtlasMin = ( atlasData.m_vAtlasMin ) + vGutterOffset;
			Vector2D vAtlasMax = ( atlasData.m_vAtlasMax ) - vGutterOffset;
			Vector2D vDeltaAtlas = vAtlasMax - vAtlasMin;
			Vector2D vDeltaBounds(1,1);
			Vector2D vAtlasUVSize(0,0);
			if ( vDeltaBounds.x != 0.0f && vDeltaBounds.y != 0.0f )
				vAtlasUVSize = vDeltaAtlas / vDeltaBounds;
			Vector2D vShift = vAtlasMin - Vector2D(0,0) * vAtlasUVSize;

			for ( int v=pChart->m_nVertexStart; v<pChart->m_nVertexStart + pChart->m_nVertexCount; ++v )
			{
				float *pNewVert = tempMesh.GetVertex( v );
				Vector2D *pNewTex = (Vector2D*)( pNewVert + nTexOffset );

				pNewTex->x = pNewTex->x * vAtlasUVSize.x + vShift.x;
				pNewTex->y = pNewTex->y * vAtlasUVSize.y + vShift.y;
			}
		}
	
		// Clean and weld the mesh
		float flEpsilon = 1e-6;
		float *pEpsilons = new float[ tempMesh.m_nVertexStrideFloats ];
		for ( int e=0; e<tempMesh.m_nVertexStrideFloats; ++e )
		{
			pEpsilons[ e ] = flEpsilon;
		}
		WeldVertices( pMeshOut, tempMesh, pEpsilons, tempMesh.m_nVertexStrideFloats );

		delete []pEpsilons;
	}
	else
	{
		// We didn't make a chart
		bMadeAtlas = false;

		// Create an atlas
		Msg( "Too many charts (%d), creating planar mapping\n", nCharts );

		// Create a planar projection
		Vector vMinBounds;
		Vector vMaxBounds;
		inputMesh.CalculateBounds( &vMinBounds, &vMaxBounds );

		// BBox delta
		Vector vBoundsDelta = vMaxBounds - vMinBounds;

		DuplicateMesh( pMeshOut, inputMesh );

		// Update UVs based on... shakes magic 8 ball... XZ projection, for now
		for ( int v=0; v<pMeshOut->m_nVertexCount; ++v )
		{
			float *pNewVert = pMeshOut->GetVertex( v );
			Vector *pPos = ( Vector* )( pNewVert + nPosOffset );
			Vector2D *pNewTex = ( Vector2D* )( pNewVert + nTexOffset );

			pNewTex->x = ( pPos->x - vMinBounds.x ) / vBoundsDelta.x;
			pNewTex->y = ( pPos->z - vMinBounds.z ) / vBoundsDelta.z;
		}
	}

	// Delete the charts
	nCharts = chartList.Count();
	for ( int c=0; c<nCharts; ++c )
	{
		UVChart_t *pChart = chartList[c];
		delete pChart;
	}
	chartList.Purge();

	return bMadeAtlas;
}