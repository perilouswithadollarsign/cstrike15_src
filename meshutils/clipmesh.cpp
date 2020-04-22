//=========== Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Mesh clipping operations.
//
//===========================================================================//

#include "mesh.h"
#include "tier1/utlbuffer.h"

void ClipTriangle( float *pBackOut, float *pFrontOut, int *pNumBackOut, int *pNumFrontOut, int nStrideFloats, 
				   float **ppVertsIn, Vector4D &vClipPlane )
{
	int nBack = 0;
	int nFront = 0;

	Vector vPlaneNormal = vClipPlane.AsVector3D();

	const int nVerts = 3;
	for( int v=0; v<nVerts; ++v )
	{
		int nxtVert = ( v + 1 ) % nVerts;
		
		Vector vPos1 = *(Vector*)ppVertsIn[ v ];
		Vector vPos2 = *(Vector*)ppVertsIn[ nxtVert ];

		float flDot1 = DotProduct( vPos1, vPlaneNormal ) + vClipPlane.w;
		float flDot2 = DotProduct( vPos2, vPlaneNormal ) + vClipPlane.w;

		// Enforce that points that lie perfectly on the plane always go to the front
		if ( flDot1 == 0.0f )
		{
			flDot1 = 0.01f;
		}
		if ( flDot2 == 0.0f )
		{
			flDot2 = 0.01f;
		}

		if ( flDot1 < 0 )
		{
			CopyVertex( pBackOut + nBack * nStrideFloats, ppVertsIn[ v ], nStrideFloats );
			nBack ++;
		}
		else
		{
			CopyVertex( pFrontOut + nFront * nStrideFloats, ppVertsIn[ v ], nStrideFloats );
			nFront ++;
		}

		if ( flDot1 * flDot2 < 0 )
		{
			// Lerp verts
			float flLerp = -flDot1 / ( flDot2 - flDot1 );

			LerpVertex( pBackOut + nBack * nStrideFloats, ppVertsIn[ v ], ppVertsIn[ nxtVert ], flLerp, nStrideFloats );
			CopyVertex( pFrontOut + nFront * nStrideFloats, pBackOut + nBack * nStrideFloats, nStrideFloats );
			nBack ++;
			nFront++;
		}
	}

	*pNumBackOut = nBack;
	*pNumFrontOut = nFront;
}

// Clips a mesh against a plane and returns 2 meshes, one on each side of the plane.  The caller must
// check pMeshBack and pMeshFront for empty vertex or index sets implying that the mesh was entirely
// on one side of the plane or the other.
bool ClipMeshToHalfSpace( CMesh *pMeshBack, CMesh *pMeshFront, const CMesh &inputMesh, Vector4D &vClipPlane )
{
	Assert( pMeshBack || pMeshFront );

	// NOTE: This assumes that position is the FIRST float3 in the buffer
	int nPosOffset = inputMesh.FindFirstAttributeOffset( VERTEX_ELEMENT_POSITION );
	if ( nPosOffset != 0 )
		return false;

	int nVertexStrideFloats = inputMesh.m_nVertexStrideFloats;
	int nIndexCount = inputMesh.m_nIndexCount;
	uint32 *pIndices = inputMesh.m_pIndices;
	float *pVertices = inputMesh.m_pVerts;

	// Allocate working space for the number of vertices out on each side of the plane.
	// This is a maximum of 4 vertices out when clipping a triangle to a plane.
	float *pBackOut = new float[ nVertexStrideFloats * 4 ];
	float *pFrontOut = new float[ nVertexStrideFloats * 4 ];

	CUtlBuffer backMeshVerts;
	CUtlBuffer frontMeshVerts;

	for ( int i=0; i<nIndexCount; i += 3 )
	{
		float *ppVerts[3];
		int nIndex0 = pIndices[ i     ];
		int nIndex1 = pIndices[ i + 1 ];
		int nIndex2 = pIndices[ i + 2 ];

		ppVerts[0] = pVertices + nIndex0 * nVertexStrideFloats;
		ppVerts[1] = pVertices + nIndex1 * nVertexStrideFloats;
		ppVerts[2] = pVertices + nIndex2 * nVertexStrideFloats;

		int nBackOut = 0;
		int nFrontOut = 0;
		ClipTriangle( pBackOut, pFrontOut, &nBackOut, &nFrontOut, nVertexStrideFloats, ppVerts, vClipPlane );

		// reconstruct triangles out of the polygon
		int numBackTris = nBackOut - 2;
		for ( int t=0; t<numBackTris; ++t )
		{
			backMeshVerts.Put( pBackOut,								   nVertexStrideFloats * sizeof( float ) );
			backMeshVerts.Put( pBackOut + nVertexStrideFloats * ( t + 1 ), nVertexStrideFloats * sizeof( float ) );
			backMeshVerts.Put( pBackOut + nVertexStrideFloats * ( t + 2 ), nVertexStrideFloats * sizeof( float ) );
		}

		int numFrontTris = nFrontOut - 2;
		for ( int t=0; t<numFrontTris; ++t )
		{
			frontMeshVerts.Put( pFrontOut,									 nVertexStrideFloats * sizeof( float ) );
			frontMeshVerts.Put( pFrontOut + nVertexStrideFloats * ( t + 1 ), nVertexStrideFloats * sizeof( float ) );
			frontMeshVerts.Put( pFrontOut + nVertexStrideFloats * ( t + 2 ), nVertexStrideFloats * sizeof( float ) );
		}
	}

	delete []pBackOut;
	delete []pFrontOut;

	// Turn the utlbuffers into actual meshes
	if ( pMeshBack )
	{
		pMeshBack->m_nVertexCount = backMeshVerts.TellPut() / ( nVertexStrideFloats * sizeof( float ) );
		if ( pMeshBack->m_nVertexCount )
		{
			pMeshBack->AllocateMesh( pMeshBack->m_nVertexCount, pMeshBack->m_nVertexCount, nVertexStrideFloats, inputMesh.m_pAttributes, inputMesh.m_nAttributeCount );

			Q_memcpy( pMeshBack->m_pVerts, backMeshVerts.Base(), backMeshVerts.TellPut() );
			for ( int i=0; i<pMeshBack->m_nIndexCount; ++i )
			{
				pMeshBack->m_pIndices[i] = i;
			}
		}
	}

	if ( pMeshFront )
	{
		pMeshFront->m_nVertexCount = frontMeshVerts.TellPut() / ( nVertexStrideFloats * sizeof( float ) );
		if ( pMeshFront->m_nVertexCount )
		{
			pMeshFront->AllocateMesh( pMeshFront->m_nVertexCount, pMeshFront->m_nVertexCount, nVertexStrideFloats, inputMesh.m_pAttributes, inputMesh.m_nAttributeCount );

			Q_memcpy( pMeshFront->m_pVerts, frontMeshVerts.Base(), frontMeshVerts.TellPut() );
			for ( int i=0; i<pMeshFront->m_nIndexCount; ++i )
			{
				pMeshFront->m_pIndices[i] = i;
			}
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------
void CreateGridCellsForVolume( CUtlVector< GridVolume_t > &outputVolumes, const Vector &vTotalMinBounds, const Vector &vTotalMaxBounds, const Vector &vGridSize )
{
	// First, determine if we need to be split
	Vector vDelta = vTotalMaxBounds - vTotalMinBounds;
	int nX = vDelta.x / vGridSize.x;
	int nY = vDelta.y / vGridSize.y;
	int nZ = vDelta.z / vGridSize.z;

	Vector vEpsilon( 0.1f, 0.1f, 0.1f );
	Vector vMinBounds = vTotalMinBounds - vEpsilon;
	Vector vMaxBounds = vTotalMaxBounds + vEpsilon;

	if ( nX * nY * nZ < 2 )
	{
		GridVolume_t newbounds;
		newbounds.m_vMinBounds = vMinBounds;
		newbounds.m_vMaxBounds = vMaxBounds;
		outputVolumes.AddToTail( newbounds );
	}
	else
	{
		Vector vStep;
		vStep.z = vDelta.z / nZ;
		vStep.y = vDelta.y / nY;
		vStep.x = vDelta.x / nX;

		// Create the split volumes
		outputVolumes.EnsureCount( nX * nY * nZ );

		int nVolumes = 0;
		Vector vStart = vMinBounds;
		for ( int z=0; z<nZ; ++z )
		{
			vStart.y = vMinBounds.y;
			for ( int y=0; y<nY; ++y )
			{
				vStart.x = vMinBounds.x;
				for ( int x=0; x<nX; ++x )
				{
					GridVolume_t newbounds;
					newbounds.m_vMinBounds = vStart;
					newbounds.m_vMaxBounds = vStart + vStep;

					if ( x == nX - 1 )
						newbounds.m_vMaxBounds.x = vMaxBounds.x;
					if ( y == nY - 1 )
						newbounds.m_vMaxBounds.y = vMaxBounds.y;
					if ( z == nZ - 1 )
						newbounds.m_vMaxBounds.z = vMaxBounds.z;

					outputVolumes[ nVolumes ] = newbounds;
					nVolumes ++;

					vStart.x += vStep.x;
				}
				vStart.y += vStep.y;
			}
			vStart.z += vStep.z;
		}
	}
}

//--------------------------------------------------------------------------------------
// For a list of AABBs, find all indices in the mesh that belong to each AABB.  Then
// coalesce those indices into a single buffer in order of the input AABBs and return a
// list of ranges of the indices in the output mesh that are needed in each input volume
//--------------------------------------------------------------------------------------
void CreatedGriddedIndexRangesFromMesh( CMesh *pOutputMesh, CUtlVector< IndexRange_t > &outputRanges, const CMesh &inputMesh, CUtlVector< GridVolume_t > &inputVolumes )
{
	DuplicateMesh( pOutputMesh, inputMesh );

	int nVolumes = inputVolumes.Count();
	outputRanges.EnsureCount( nVolumes );

	int nFaces = inputMesh.m_nIndexCount / 3;
	uint32 *pInputIndices = inputMesh.m_pIndices;
	uint32 *pOutputIndices = pOutputMesh->m_pIndices;
	int nMeshIndices = 0;

	// Go though each volume and assign indices to its respective range
	for ( int v=0; v<nVolumes; ++v )
	{
		GridVolume_t &volume = inputVolumes[ v ];
		IndexRange_t &range = outputRanges[ v ];
		range.m_nStartIndex = nMeshIndices;

		for ( int f=0; f<nFaces; ++f )
		{
			uint32 i0 = pInputIndices[ f * 3     ];
			uint32 i1 = pInputIndices[ f * 3 + 1 ];
			uint32 i2 = pInputIndices[ f * 3 + 2 ];

			Vector vCenter = *(Vector*)inputMesh.GetVertex( i0 );
			vCenter += *(Vector*)inputMesh.GetVertex( i1 );
			vCenter += *(Vector*)inputMesh.GetVertex( i2 );
			vCenter /= 3.0f;
			
			if ( vCenter.x > volume.m_vMinBounds.x && vCenter.x <= volume.m_vMaxBounds.x &&
				 vCenter.y > volume.m_vMinBounds.y && vCenter.y <= volume.m_vMaxBounds.y &&
				 vCenter.z > volume.m_vMinBounds.z && vCenter.z <= volume.m_vMaxBounds.z )
			{
				// Add the whole triangle
				pOutputIndices[ nMeshIndices++ ] = i0;
				pOutputIndices[ nMeshIndices++ ] = i1;
				pOutputIndices[ nMeshIndices++ ] = i2;
			}
		}

		range.m_nIndexCount = nMeshIndices - range.m_nStartIndex;
	}

	Assert( nMeshIndices == inputMesh.m_nIndexCount );
}