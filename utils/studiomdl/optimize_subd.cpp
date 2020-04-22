//========= Copyright � 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "optimize_subd.h"

#define PI 3.14159265

#define VTXIDX(vID) m_vtxList[vID].origMeshVertID
#define	VTXPOS(vID) *m_vtxData->Position( m_vtxList[vID].origMeshVertID )
#define VTXNOR(vID) *m_vtxData->Normal( m_vtxList[vID].origMeshVertID )

static Vector project_and_normalize( Vector v, Vector n )
{
	v = v - DotProduct(v, n)*n;
	VectorNormalize(v);

	return v;
}

static int MOD4[8] = {0,1,2,3,0,1,2,3};

namespace OptimizedModel
{
	class NeighborCornerBitfield 
	{
	public:
		unsigned short *bitfield;

		NeighborCornerBitfield(unsigned short *field): index(0), bitfield(field) { *bitfield = 0; }

		void pushBit( bool bit )
		{
			*bitfield |= bit<<index; index++;
		}

		void popBit()
		{
			index--; *bitfield &= ~(1<<index);
		}

		bool getBitAt( unsigned short i )	
		{
			return ((*bitfield & (1<<i))>>i) == 1;
		}

		void insertBitAt( unsigned short i, bool bit )
		{
			unsigned short preMask = (1<<i)-1;
			*bitfield = (*bitfield & preMask) + ((*bitfield & (~preMask))<<1) + (bit<<i);
			index++;
		}

		void removeBitAt( unsigned short i )
		{
			unsigned short preMask  = (1<<i)-1;
			unsigned short postMask = ~((1<<(i+1))-1);
			*bitfield = ((*bitfield & postMask)>>1) + (*bitfield & preMask);
			index--;
		}


		void clearBits()
		{
			*bitfield = 0;
		}

	private:
		unsigned short index;	
	};



	COptimizeSubDBuilder::COptimizeSubDBuilder(SubD_FaceList_t& subDFaceList, const SubD_VertexList_t& vertexList, const SubD_VertexData_t &vertexData, bool bIsTagged, bool bMendVertices)
		: m_faceList(subDFaceList), m_vtxList(vertexList), m_vtxData(vertexData)
	{
		m_numPatches = (int) subDFaceList.Count();

		ProcessPatches(bIsTagged,bMendVertices);
	}

	void dumpPatch(SubD_Face_t *patch)
	{
		Msg( "Patch: %d\n", patch->patchID );
		Msg( "                    vtxIDs: %d %d %d %d\n", patch->vtxIDs[0], patch->vtxIDs[1], patch->vtxIDs[2], patch->vtxIDs[3] );
		Msg( "                  valences: %d %d %d %d\n", patch->valences[0], patch->valences[1], patch->valences[2], patch->valences[3] );
		Msg( "              vtx1RingSize: %d %d %d %d\n", patch->vtx1RingSize[0], patch->vtx1RingSize[1], patch->vtx1RingSize[2], patch->vtx1RingSize[3] );
		Msg( "  vtx1RingCenterQuadOffset: %d %d %d %d\n", patch->vtx1RingCenterQuadOffset[0], patch->vtx1RingCenterQuadOffset[1], patch->vtx1RingCenterQuadOffset[2], patch->vtx1RingCenterQuadOffset[3] );
		Msg( "                    bndVtx: %d %d %d %d\n", patch->bndVtx[0], patch->bndVtx[1], patch->bndVtx[2], patch->bndVtx[3] );
		Msg( "                 cornerVtx: %d %d %d %d\n", patch->cornerVtx[0], patch->cornerVtx[1], patch->cornerVtx[2], patch->cornerVtx[3] );
		Msg( "                   BndEdge: %d %d %d %d\n", patch->bndEdge[0], patch->bndEdge[1], patch->bndEdge[2], patch->bndEdge[3] );
		Msg( "            halfEdges.twin: %d/%d %d/%d %d/%d %d/%d\n", 
			patch->halfEdges[0].twin ? patch->halfEdges[0].twin->patch->patchID : -1, patch->halfEdges[0].twin ? patch->halfEdges[0].twin->localID: -1,
			patch->halfEdges[1].twin ? patch->halfEdges[1].twin->patch->patchID : -1, patch->halfEdges[1].twin ? patch->halfEdges[1].twin->localID: -1,
			patch->halfEdges[2].twin ? patch->halfEdges[2].twin->patch->patchID : -1, patch->halfEdges[2].twin ? patch->halfEdges[2].twin->localID: -1,
			patch->halfEdges[3].twin ? patch->halfEdges[3].twin->patch->patchID : -1, patch->halfEdges[3].twin ? patch->halfEdges[3].twin->localID: -1 );
		Msg( "     halfEdges.sectorStart: %d/%d %d/%d %d/%d %d/%d\n", 
			patch->halfEdges[0].sectorStart ? patch->halfEdges[0].sectorStart->patch->patchID : -1, patch->halfEdges[0].sectorStart ? patch->halfEdges[0].sectorStart->localID: -1,
			patch->halfEdges[1].sectorStart ? patch->halfEdges[1].sectorStart->patch->patchID : -1, patch->halfEdges[1].sectorStart ? patch->halfEdges[1].sectorStart->localID: -1,
			patch->halfEdges[2].sectorStart ? patch->halfEdges[2].sectorStart->patch->patchID : -1, patch->halfEdges[2].sectorStart ? patch->halfEdges[2].sectorStart->localID: -1,
			patch->halfEdges[3].sectorStart ? patch->halfEdges[3].sectorStart->patch->patchID : -1, patch->halfEdges[3].sectorStart ? patch->halfEdges[3].sectorStart->localID: -1 );
		Msg( "               nbCornerVtx: %x %x %x %x\n", patch->nbCornerVtx[0], patch->nbCornerVtx[1], patch->nbCornerVtx[2], patch->nbCornerVtx[3] );
		Msg( "              loopGapAngle: %d %d %d %d\n", patch->loopGapAngle[0], patch->loopGapAngle[1], patch->loopGapAngle[2], patch->loopGapAngle[3] );
	}

	void dumpPatches( SubD_FaceList_t &quads )
	{
		size_t nQuads = quads.Count();
		for (size_t k=0; k<nQuads; k++)
		{
			dumpPatch(&quads[k]);
		}
	}

	void COptimizeSubDBuilder::ProcessPatches( bool bIsTagged, bool bMendVertices )
	{
		// Init attributes
		for ( int i=0; i<m_numPatches; ++i )
		{
			SubD_Face_t *pPatch = &m_faceList[i];

			for (int k=0; k<4; ++k)
			{
				pPatch->patchID = i;
				if ( !bIsTagged )
				{
					pPatch->bndVtx[k]   	= false;
					pPatch->bndEdge[k]  	= false;
					pPatch->cornerVtx[k]	= false;
				}
				pPatch->nbCornerVtx[k] 		= 0;
				pPatch->valences[k]    		= 0;
				pPatch->minOneRingIndex[k]	= 0;
				pPatch->loopGapAngle[k]		= 65535;
				pPatch->edgeBias[2*k]  		= 16384;
				pPatch->edgeBias[2*k+1]		= 16384;

				pPatch->halfEdges[k].twin = NULL;
				pPatch->halfEdges[k].sectorStart = &pPatch->halfEdges[k]; // start one-ring with this halfedge
				pPatch->halfEdges[k].localID = k;
				pPatch->halfEdges[k].patch = pPatch;
			}
		}

		RemapIndices();

		BuildNeighborhoodInfo();

		CheckForManifoldMesh();

		ConsistentPatchOrientation();

		if ( !bIsTagged )
		{
			TagCreases();
		}

		//	dumpPatches(m_QuadArray);

		// first pass --------------------------------------------------------------------
		for ( int i=0; i<m_numPatches; i++ )
		{
			SubD_Face_t *pPatch = &m_faceList[i];

			for ( int k=0; k<4; k++ )
			{
				ComputeSectorStart( pPatch, k );
				ComputePerVertexInfo( pPatch, k );
				ComputeSectorOneRing( pPatch, k );
				ComputeSectorAngle( pPatch, k );
			}
		}
//		dumpPatches(m_faceList);

		// second pass requires all per-vertex-per-face variables to be computed ----------

		for ( int i=0; i<m_numPatches; i++ )
		{
			SubD_Face_t *pPatch = &m_faceList[i];

			for ( int k=0; k<4; k++ )
			{
				ComputeNbCorners( pPatch, k );
			}
		}

		// third pass computes neighboring texcoords for watertight displacement mapping ----------
		for ( int i=0; i<m_numPatches; i++ )
		{
			SubD_Face_t *pPatch = &m_faceList[i];
			ComputeNeighborTexcoords( pPatch );
		}

		// Compute offsets into one-rings, necessary for subsequent evaluation consistency
		SetMinOneRingIndices();

		// Sort patches by regular vs extraordinary
		SubD_FaceList_t regFaceList;
		SubD_FaceList_t extraFaceList;
		for ( int i=0; i<m_numPatches; i++ )
		{
			SubD_Face_t *pPatch = &m_faceList[i];

			if ( FaceIsRegular( pPatch ) )
			{
				regFaceList.AddToTail( *pPatch );
			}
			else
			{
				extraFaceList.AddToTail( *pPatch );
			}
		}

		// recombine
		int nRegFaces = regFaceList.Count();
		for ( int i=0; i<nRegFaces; i++ )
		{
			m_faceList[i] = regFaceList[i];
		}

		int nExtraFaces = extraFaceList.Count();
		for ( int i=0; i<nExtraFaces; i++ )
		{
			m_faceList[i+nRegFaces] = extraFaceList[i];
		}

		// mend vertices at the end ----------
		/*if ( bMendVertices )
		{
			for ( int i=0; i<m_numPatches; i++ )
			{
				SubD_Face_t *pPatch = &m_faceList[i];

				for (int k=0; k<4; k++)
				{
					mendVertices( pPatch, k );
				}
			}
		}*/

	}

	HalfEdge *COptimizeSubDBuilder::FindTwin( HalfEdge &he )
	{
		Vector p0 = VTXPOS( he.patch->vtxIDs[ MOD4[he.localID+0] ] ); 
		Vector p1 = VTXPOS( he.patch->vtxIDs[ MOD4[he.localID+1] ] ); // twin face will have edge p1->p0

		for (int i=0; i<m_numPatches; i++)
		{
			SubD_Face_t *patch = &m_faceList[i];
			for ( unsigned short k=0; k<4; k++ )
			{
				if ( ( VTXPOS( patch->vtxIDs[ MOD4[k + 0] ] ) == p1 ) &&
					 ( VTXPOS( patch->vtxIDs[ MOD4[k + 1] ] ) == p0 ) )
				{
					return &patch->halfEdges[k];
				}
			}
		}

		return NULL;
	}

	// Set the minimum one-ring index for each of the four vertices of a patch.
	// This value is used during the mapping from vertices to Bezier control
	// points in order to ensure consistent evaluation order and avoid cracks
	void COptimizeSubDBuilder::SetMinOneRingIndices()
	{
		for ( int i=0; i<m_numPatches; i++ )				// Walk patches
		{
			SubD_Face_t* pPatch = &m_faceList[i];
			int nFirstNeighbor = 0;							// First neighbor in a given vertex's one-ring

			for ( int k=0; k<4; k++ )						// For each vertex of the patch
			{
				int nMinNeighborIdx = m_IndexRemapTable[pPatch->oneRing[nFirstNeighbor]];	// Remapped Index
				int nMinNeighborOffset = 0;													// Neighbor zero is the current min

				int nLastNeighbor = nFirstNeighbor + pPatch->vtx1RingSize[k] - 1;	// Last neighbor

				for ( int j=nFirstNeighbor; j<=nLastNeighbor; j++ )					// First neighbor to the last neighbor, inclusive
				{
					int nNeighborIdx = m_IndexRemapTable[pPatch->oneRing[j]];		// Use only remapped indices

					if ( nNeighborIdx < nMinNeighborIdx )							// If we have a smaller remapped index
					{
						nMinNeighborIdx = nNeighborIdx;								// Set as new min index
						nMinNeighborOffset = j - nFirstNeighbor;					// Offset into THIS vertex's one-ring
					}
				}

				pPatch->minOneRingIndex[k] = nMinNeighborOffset;		// Set the offset into THIS vertex's one-ring

				nFirstNeighbor = nLastNeighbor + 1;						// Go to next range of indices in the one-ring array
			}
		}
	}

	// Positions appear redundantly in vertex data, so we need a mapping so that SetMinOneRingIndices() can do the right thing
	void COptimizeSubDBuilder::RemapIndices()
	{
		for ( int i=0; i<m_vtxList.Count(); i++ )
		{
			m_IndexRemapTable.AddToTail(i);					// Set identity mapping
		}

		for ( int i=0; i<m_vtxList.Count(); i++ )			// Walk indices again
		{
			for ( int j=i+1; j<m_vtxList.Count(); j++ )		// Look at later indices
			{
				Vector vPosi = VTXPOS( i ); 
				Vector vPosj = VTXPOS( j ); 
				if ( vPosi == vPosj )						// If the positions are equivalent, set index remapping
				{
					m_IndexRemapTable[j] = MIN( i, j );
					m_IndexRemapTable[i] = MIN( i, j );
				}
			}
		}
/*
		for ( int i=0; i<m_vtxList.Count(); i++ )
		{
			if ( i != m_IndexRemapTable[i] )
			{
				Msg( "(%d, %d)  ***\n", i, m_IndexRemapTable[i] );
			}
			else
			{
				Msg( "(%d, %d)\n", i, m_IndexRemapTable[i] );
			}
		}
*/
	}

	void COptimizeSubDBuilder::BuildNeighborhoodInfo( )
	{
		for ( int i=0; i<m_numPatches; i++ )
		{
			SubD_Face_t* pPatch = &m_faceList[i];

			for ( int k=0; k<4; k++ )
			{
				if ( !pPatch->halfEdges[k].twin )
				{
					HalfEdge *pTwin = FindTwin(pPatch->halfEdges[k]);
					pPatch->halfEdges[k].twin = pTwin;									// record twin

					if ( pTwin )
					{
						pPatch->halfEdges[k].twin->twin = &pPatch->halfEdges[k];		// record twin's twin
					} 
					else 
					{
						pPatch->bndEdge[k] = true;
						pPatch->bndVtx[MOD4[k+0]] = true;
						pPatch->bndVtx[MOD4[k+1]] = true;
					}
				}
			}
		}
	}

	void COptimizeSubDBuilder::CheckForManifoldMesh( )
	{
		for ( int i=0; i<m_numPatches; i++ )
		{
			SubD_Face_t* pPatch = &m_faceList[i];

			for (unsigned short k=0; k<4; ++k)
			{
				if (( pPatch->halfEdges[k].twin != NULL ) && (pPatch->halfEdges[k].twin->twin != &pPatch->halfEdges[k]) )
				{

					Msg( "Topology error at vertices %d, %d, %d\n", pPatch->vtxIDs[MOD4[k+3]], pPatch->vtxIDs[MOD4[k+0]], pPatch->vtxIDs[MOD4[k+1]] );

					Vector vA = VTXPOS( pPatch->vtxIDs[MOD4[k+3]] );
					Vector vB = VTXPOS( pPatch->vtxIDs[MOD4[k+0]] );
					Vector vC = VTXPOS( pPatch->vtxIDs[MOD4[k+1]] );

					Msg( "spaceLocator -p %.4f %.4f %.4f;\n", vA.x, vA.y, vA.z );
					Msg( "spaceLocator -p %.4f %.4f %.4f;\n", vB.x, vB.y, vB.z );
					Msg( "spaceLocator -p %.4f %.4f %.4f;\n", vC.x, vC.y, vC.z );

				}
			}
		}
	}

	void COptimizeSubDBuilder::ComputeSectorStart(SubD_Face_t *pPatch, unsigned short k)
	{
		HalfEdge *sectorStart, *next = &pPatch->halfEdges[k];

		do
		{
			sectorStart = next;
			if ( next->BndEdge() )
			{
				next = NULL;
			}
			else
			{
				next = next->PrevByTail();
			}
		}
		while ( ( next != NULL ) && ( next != &(pPatch->halfEdges[k]) ) );

		if ( next == NULL )
		{
			pPatch->halfEdges[k].sectorStart = sectorStart; // only update sectorStart if we actually hit a boundary
		}
	}

	// Propagates bndVtx to faces that do not have a BndEdge to this vertex, sets cornerVtx, 
	// Requires sectorStart, corrects sectorStart and bndVtx for dangling crease edges.
	void COptimizeSubDBuilder::ComputePerVertexInfo(SubD_Face_t *baseQuad, unsigned short baseLocalID) 
	{
		unsigned short nBndEdges = 0;

		HalfEdge *sectorStart = baseQuad->halfEdges[ MOD4[baseLocalID] ].sectorStart, *he = sectorStart;

		// Find first sector
		HalfEdge *next = he->PrevByTail();
		while ( ( next!=NULL ) && ( next!=sectorStart ) )
		{
			he = next;
			next = next->PrevByTail();
		}

		if ( next != NULL )
		{
			he = sectorStart;
		}

		if ( he->BndEdge() )
		{
			nBndEdges++;
		}

		HalfEdge *heEnd = he->twin;
		he = he->PrevInFace();

		do
		{
			if ( he->BndEdge() )
			{
				nBndEdges++;
			}
			he = he->NextByHead();

		} while (( he != NULL ) && (he != heEnd));

		// Set flags
		if ( nBndEdges == 1 ) // dangling BndEdge -> correct sectorStart
		{
			baseQuad->halfEdges[ baseLocalID ].sectorStart = &baseQuad->halfEdges[ baseLocalID ];
			baseQuad->bndVtx[baseLocalID] = false;
		}
		else if ( nBndEdges >= 2 )
		{
			baseQuad->bndVtx[baseLocalID] = true;
			if ( nBndEdges > 2 )
			{
				baseQuad->cornerVtx[baseLocalID] = true;     // more than 2 BndEdges -> cornerVtx
			}
		}
	}

	//
	// Writes oneRing, vtx1RingSize, vtx1RingCenterQuadOffset, valence
	//
	void COptimizeSubDBuilder::ComputeSectorOneRing( SubD_Face_t *baseQuad, unsigned short baseLocalID )	
	{
		unsigned short *oneRing = baseQuad->oneRing; 
		for ( unsigned short k=0; k < baseLocalID; k++ )
		{
			oneRing += baseQuad->vtx1RingSize[k];
		}

		unsigned short &centerOffset = baseQuad->vtx1RingCenterQuadOffset[baseLocalID] = 1;
		unsigned short &valence = baseQuad->valences[baseLocalID] = 0;
		unsigned short &oneRingSize = baseQuad->vtx1RingSize[baseLocalID] = 0;

		HalfEdge *heBase = &baseQuad->halfEdges[ MOD4[baseLocalID] ];
		HalfEdge *he = heBase->sectorStart;

		oneRing[oneRingSize++] = he->patch->vtxIDs[ MOD4[he->localID+0] ];
		valence++;
		oneRing[oneRingSize++] = he->patch->vtxIDs[ MOD4[he->localID+1] ];

		HalfEdge *heEnd = he->twin;
		he = he->PrevInFace();

		do
		{
			oneRing[oneRingSize++] = he->patch->vtxIDs[ MOD4[he->localID+3] ];
			valence++;
			oneRing[oneRingSize++] = he->patch->vtxIDs[ MOD4[he->localID+0] ];

			if ( he->twin == heBase )
			{
				centerOffset = oneRingSize - 1;
			}

			he = (he->BndEdge() && baseQuad->bndVtx[baseLocalID]) ? NULL : he->NextByHead(); // make sure we only step over BndEdge if it is dangling.

		} while ( ( he != NULL ) && ( he != heEnd ) );

		if ( ( he != NULL) && ( he == heEnd ) ) // if we closed the loop, add off-edge vertex from last quad.
		{
			oneRing[oneRingSize++] = he->patch->vtxIDs[ MOD4[ he->localID+3 ]];
		}
	}	
	

	// Depends on bndVtx, cornerVtx, valence
	void COptimizeSubDBuilder::ComputeSectorAngle( SubD_Face_t *baseQuad, unsigned short baseLocalID )
	{
		if ( !baseQuad->bndVtx[baseLocalID] )		// If no boundary vertex, nothing needs to be done (includes dangling crease)
			return;

		if ( !baseQuad->cornerVtx[baseLocalID] )	// If no corner, set loopGapAngle = PI (or PI/2 for valence==2)
		{
			baseQuad->loopGapAngle[baseLocalID] = 65535 / ( baseQuad->valences[baseLocalID] == 2 ? 4 : 2 );
			return;
		}

		HalfEdge *he = baseQuad->halfEdges[ MOD4[baseLocalID] ].sectorStart; 

		Vector center_pos = VTXPOS( he->patch->vtxIDs[ he->localID ] );
		Vector center_nor = VTXNOR( he->patch->vtxIDs[ he->localID ] );
		VectorNormalize(center_nor);

		int debugVtxID = he->patch->vtxIDs[ MOD4[ he->localID+1 ] ];

		Vector eVec1 = VTXPOS( he->patch->vtxIDs[ MOD4[ he->localID+1 ] ] ) - center_pos, eVec2; 
		Vector npVec1 = project_and_normalize( eVec1, center_nor ), npVec2; 

		float sector_angle = 0;

		he = he->PrevInFace();

		do 
		{
		    debugVtxID = he->patch->vtxIDs[ MOD4[ he->localID ] ];
										   
			eVec2 = VTXPOS( he->patch->vtxIDs[ MOD4[ he->localID ] ] ) - center_pos;
			npVec2 = project_and_normalize( eVec2, center_nor ); 
			sector_angle += acosf( DotProduct( npVec1, npVec2 ) );

			he = he->BndEdge() ? NULL : he->NextByHead(); // make sure we only step over BndEdge if it is dangling.
			npVec1 = npVec2;
		} while ( he != NULL ); // only way to terminate is to hit BndEdge

		VectorNormalize( eVec1 );
		VectorNormalize( eVec2 );
		float loopGapAngleF = acosf( DotProduct(eVec1, eVec2) );  // measure overall gap
		baseQuad->loopGapAngle[baseLocalID] = (unsigned int) ( ( 65535.0 * loopGapAngleF ) / ( 2 * PI ) );
	}

	void COptimizeSubDBuilder::MendVertices(SubD_Face_t *baseQuad, unsigned short baseLocalID)
	{
		HalfEdge *he = baseQuad->halfEdges[ baseLocalID ].sectorStart; 
		unsigned short vtxID = baseQuad->vtxIDs[ baseLocalID ];
		
		Vector p = VTXPOS( vtxID );
		Vector n = VTXNOR( vtxID );

		HalfEdge *heEnd = he->twin;
		he = he->PrevInFace();

		do
		{
			if (( VTXPOS( he->patch->vtxIDs[ MOD4[he->localID+1] ]) == p ) &&
				( VTXNOR( he->patch->vtxIDs[ MOD4[he->localID+1] ]) == n ))
			{
				he->patch->vtxIDs[ MOD4[he->localID+1] ] = vtxID;
		
			}

			if ( (he->twin) && 
				( VTXPOS( he->twin->patch->vtxIDs[ MOD4[he->twin->localID] ] ) == p) &&
				( VTXNOR( he->twin->patch->vtxIDs[ MOD4[he->twin->localID] ] ) == n) )
			{
				he->twin->patch->vtxIDs[ MOD4[he->twin->localID] ] = vtxID;
			}

			he = he->NextByHead();
		} while (( he != NULL ) && (he != heEnd));
		
	}

	// Computes a bitfield with bits set if the corresponding neighbor-vertex is a concave corner
	// this has to go in a second pass as all per-face-per-vertex flags from the first pass to be computed beforehand
	void COptimizeSubDBuilder::ComputeNbCorners( SubD_Face_t *baseQuad, unsigned short baseLocalID )
	{
		NeighborCornerBitfield nbCorners( &baseQuad->nbCornerVtx[baseLocalID] );

		HalfEdge *he = baseQuad->halfEdges[ MOD4[baseLocalID] ].sectorStart; 
		nbCorners.pushBit( he->patch->cornerVtx[ MOD4[he->localID+1] ] == 2 );

		HalfEdge *heEnd = he->twin;
		he = he->PrevInFace();

		do
		{
			nbCorners.pushBit( he->patch->cornerVtx[ he->localID ] == 2 );

			he = ( he->BndEdge() && baseQuad->bndVtx[baseLocalID] ) ? NULL : he->NextByHead(); // make sure we only step over BndEdge if it is dangling.
		} while (( he != NULL ) && (he != heEnd));
	}

	unsigned short COptimizeSubDBuilder::FindNeighborVertex( HalfEdge** ppOutMirrorEdge, const HalfEdge *pHalfEdge, int indexAlongEdge )
		// Finds neighboring vertex along the mirror edge of pHalfEdge.
		// Returns the index of the vertex.
		// pOutMirrorEdge is the mirror edge we took this vertex from.
		// pHalfEdge is the shared edge who's mirror we want to find.
		// indexAlongEdge is the index of the vertex along the edge. ( 0 or 1 only )
	{
		HalfEdge* pMirrorEdge = pHalfEdge->twin;

		unsigned short vertexID = (unsigned short)-1;
		if ( pMirrorEdge )
		{
			vertexID = pMirrorEdge->patch->vtxIDs[ ( pMirrorEdge->localID + indexAlongEdge ) % 4 ] ;
		}

		if ( ppOutMirrorEdge )
		{
			*ppOutMirrorEdge = pMirrorEdge;
		}

		return vertexID;
	}

	// Computes the neighboring texcoords ( Interior, EdgeV, EdgeU, Corner ) for each vertex
	// texcoords are computed in such a way that every shared edge or corner computes the same values
	// this is used as a tie-breaking scheme for creating consistent texture sampling for displacement maps
	void COptimizeSubDBuilder::ComputeNeighborTexcoords( SubD_Face_t *baseQuad )
	{
		unsigned short p = baseQuad->patchID;
		unsigned short invalidNeighborValue = (unsigned short)-1;

		// Loop over all 4 verts of the quad
		for ( int i=0; i<4; ++i )
		{
			unsigned short index = baseQuad->vtxIDs[i];

			// Interior point is alway the current corner
			baseQuad->vUV0[i] = VTXIDX( index );
//			Assert( index == baseQuad->vUV0[i] );

			// Default to original texcoord values for 1 and 2
			baseQuad->vUV1[i] = baseQuad->vUV0[i];
			baseQuad->vUV2[i] = baseQuad->vUV0[i];

			// Find the texture coordinates of our neighbors
			// Only keep the texture coordinates of the neighbor with the greatest quad index
			HalfEdge* pMirrorEdgeV = NULL;

			// V edge ( store the UVs of the patch with the greatest ID )
			unsigned short  iNeighborPatchV = invalidNeighborValue;
			unsigned short iNeighborV = FindNeighborVertex( &pMirrorEdgeV, &baseQuad->halfEdges[ i ], 1 );
			if ( iNeighborV != invalidNeighborValue )	// hard edge test
			{
				iNeighborPatchV = pMirrorEdgeV->patch->patchID;
				if ( iNeighborPatchV > p )
				{
					baseQuad->vUV1[i] = VTXIDX( iNeighborV );
				}
			}

			HalfEdge* pMirrorEdgeU = NULL;

			// U edge ( store the UVs of the patch with the greatest ID )
			unsigned short iNeighborPatchU = invalidNeighborValue;
			unsigned short iNeighborU = FindNeighborVertex( &pMirrorEdgeU, &baseQuad->halfEdges[ (i+3)%4 ], 0 );
			if ( iNeighborU != invalidNeighborValue )	// hard edge test
			{
				iNeighborPatchU = pMirrorEdgeU->patch->patchID;
				if ( iNeighborPatchU > p )
				{
					baseQuad->vUV2[i] = VTXIDX( iNeighborU );
				}
			}

			// Corner ( store the UVs of the patch with the greatest ID ).
			// Walk from NeighborV to NeighborU and store data for the largest patch ID.
			// We may redundantly check NeighborPatchU here if this is a valence 3 vertex.
			HalfEdge* pMirrorEdgeCorner = pMirrorEdgeV;
			unsigned short iNeighborPatch = invalidNeighborValue;
			unsigned short iMaxNeighborCorner = index;
			unsigned short iMaxPatch = baseQuad->patchID;
			if ( pMirrorEdgeCorner )
			{
				do
				{
					HalfEdge* pNextEdge = pMirrorEdgeCorner->NextInFace();
					unsigned short iNeighborCorner = FindNeighborVertex( &pMirrorEdgeCorner, pNextEdge, 1 );
					if ( iNeighborCorner != invalidNeighborValue )	// hard edge test
					{
						iNeighborPatch = pMirrorEdgeCorner->patch->patchID;
						if ( pMirrorEdgeCorner->patch->patchID > iMaxPatch )
						{
							iMaxPatch = pMirrorEdgeCorner->patch->patchID;
							iMaxNeighborCorner = iNeighborCorner;
						}
					}
				} while( iNeighborPatch != iNeighborPatchU && pMirrorEdgeCorner );
			}

			// Determine whether We still need to check against U and V adjacent patches
			if ( pMirrorEdgeU && ( pMirrorEdgeU->patch->patchID > iMaxPatch ) )
			{
				iMaxPatch = pMirrorEdgeU->patch->patchID;
				iMaxNeighborCorner = iNeighborU;
			}

			if ( pMirrorEdgeV && ( pMirrorEdgeV->patch->patchID > iMaxPatch ) )
			{
				iMaxPatch = pMirrorEdgeV->patch->patchID;
				iMaxNeighborCorner = iNeighborV;
			}

			baseQuad->vUV3[i] = VTXIDX( iMaxNeighborCorner );
		}
	}

	void DumpPatchLite( SubD_Face_t *patch )
	{
		Msg( "Patch: %d\n", patch->patchID );
		Msg( "                    vtxIDs: %d %d %d %d\n", patch->vtxIDs[0], patch->vtxIDs[1], patch->vtxIDs[2], patch->vtxIDs[3] );
		Msg( "            halfEdges.twin: %d/%d %d/%d %d/%d %d/%d\n", 
			patch->halfEdges[0].twin ? patch->halfEdges[0].twin->patch->patchID : -1, patch->halfEdges[0].twin ? patch->halfEdges[0].twin->localID: -1,
			patch->halfEdges[1].twin ? patch->halfEdges[1].twin->patch->patchID : -1, patch->halfEdges[1].twin ? patch->halfEdges[1].twin->localID: -1,
			patch->halfEdges[2].twin ? patch->halfEdges[2].twin->patch->patchID : -1, patch->halfEdges[2].twin ? patch->halfEdges[2].twin->localID: -1,
			patch->halfEdges[3].twin ? patch->halfEdges[3].twin->patch->patchID : -1, patch->halfEdges[3].twin ? patch->halfEdges[3].twin->localID: -1 );
		Msg( "     halfEdges.sectorStart: %d/%d %d/%d %d/%d %d/%d\n", 
			patch->halfEdges[0].sectorStart ? patch->halfEdges[0].sectorStart->patch->patchID : -1, patch->halfEdges[0].sectorStart ? patch->halfEdges[0].sectorStart->localID: -1,
			patch->halfEdges[1].sectorStart ? patch->halfEdges[1].sectorStart->patch->patchID : -1, patch->halfEdges[1].sectorStart ? patch->halfEdges[1].sectorStart->localID: -1,
			patch->halfEdges[2].sectorStart ? patch->halfEdges[2].sectorStart->patch->patchID : -1, patch->halfEdges[2].sectorStart ? patch->halfEdges[2].sectorStart->localID: -1,
			patch->halfEdges[3].sectorStart ? patch->halfEdges[3].sectorStart->patch->patchID : -1, patch->halfEdges[3].sectorStart ? patch->halfEdges[3].sectorStart->localID: -1 );
	}

	// Rotate a particular face one step (element N grabs from element N-1)
	void COptimizeSubDBuilder::RotateOnce( SubD_Face_t *pPatch )
	{
//		Msg( "- Before ------------------------------------------------------------------------------------------\n" );
//		DumpPatchLite( pPatch );

		SubD_Face_t tmpFace;
		memcpy( &tmpFace, pPatch, sizeof( SubD_Face_t ) );

		HalfEdge *pTwins[4] = { NULL, NULL, NULL, NULL };
		for ( int i=0; i<4; i++ )
		{
			pTwins[i] = pPatch->halfEdges[i].twin;					// Point to each HalfEdge's twin
			if ( pTwins[i] )
			{
				Assert( pTwins[i]->twin == &(pPatch->halfEdges[i]) );	// ith twin should be pointing back to ith HalfEdge
			}
		}

		for ( int i=0; i<4; i++ )
		{
			pPatch->vtxIDs[i] = tmpFace.vtxIDs[(i+3)%4];			// Grab from n-1
			pPatch->bndEdge[i] = tmpFace.bndEdge[(i+3)%4];
			pPatch->bndVtx[i] = tmpFace.bndVtx[(i+3)%4];

			memcpy( &(pPatch->halfEdges[i]), &(tmpFace.halfEdges[(i+3)%4]), sizeof(HalfEdge) );
			pPatch->halfEdges[i].localID = i;
			pPatch->halfEdges[i].sectorStart = &pPatch->halfEdges[i];
		}

		for ( int i=0; i<4; i++ )
		{
			if ( pTwins[i] )
			{
				pTwins[i]->twin = &(pPatch->halfEdges[(i+1)%4]);		// Record twin's twin after we've rotated the local patch data
			}
		}

//		Msg( "- After ------------------------------------------------------------------------------------------\n" );
//		DumpPatchLite( pPatch );
//		Msg( "---------------------------------------------------------------------------------------------------\n" );
//		Msg( "---------------------------------------------------------------------------------------------------\n\n" );
	}

	void COptimizeSubDBuilder::RotateFace( SubD_Face_t *pPatch, int nTimesToRotate )
	{
		for ( int i=0; i<nTimesToRotate; i++ )
		{
			RotateOnce( pPatch );
		}
	}

	int COptimizeSubDBuilder::FaceEdgeIndex( SubD_Face_t *pFace, HalfEdge *pEdge )
	{
		int i = 0;
		while ( &(pFace->halfEdges[i]) != pEdge )
		{
			i++;
		}
		return i;
	}

	void COptimizeSubDBuilder::Propagate( CUtlVector<Orientation> & orientationArray, HalfEdge *pEdge, bool dir )
	{
		Assert( pEdge );

		while( true )
		{
			HalfEdge *pNeighborEdge = pEdge->twin;
			if ( !pNeighborEdge )
				break;	// Stop at mesh boundaries.

			SubD_Face_t *pFace = pNeighborEdge->patch;
			if ( !pFace )
				break;	// Stop at mesh boundaries.

			int nEdgeIndex = FaceEdgeIndex( pFace, pNeighborEdge );

			Orientation & faceOrientation = orientationArray[pFace->patchID];

			if ( nEdgeIndex == 1 || nEdgeIndex == 3 )
			{
				if ( faceOrientation.uSet )
				{
					Assert( faceOrientation.u == ( ( nEdgeIndex == 1 ) ^ dir ) );
					break;
				}
				faceOrientation.SetU( ( nEdgeIndex == 1 ) ^ dir );
			}
			else // if ( nEdgeIndex == 0 || nEdgeIndex == 2 )
			{
				if ( faceOrientation.vSet )
				{
					Assert( faceOrientation.v == ( ( nEdgeIndex == 0 ) ^ dir ) );
					break;
				}
				faceOrientation.SetV( ( nEdgeIndex == 0 ) ^ dir );
			}

			pEdge = pNeighborEdge->NextInFace()->NextInFace();
		}
	}


	static HalfEdge *FaceEdge( SubD_Face_t *pPatch, int idx )
	{
		int i = 0;
		HalfEdge *pEdge = &pPatch->halfEdges[0];

		while ( i != idx )
		{
			i++;
			pEdge = pEdge->NextInFace();
		}

		return pEdge;
	}

	// Reorient faces in order to avoid parametric discontinuities.
	void COptimizeSubDBuilder::ConsistentPatchOrientation()
	{
		CUtlVector<Orientation> orientationArray;
		orientationArray.AddMultipleToTail( m_numPatches ); 

		for( int f = 0; f < m_numPatches; f++ )
		{
			SubD_Face_t *pPatch = &m_faceList[f];
			HalfEdge *pEdges = &pPatch->halfEdges[0];

			if ( !orientationArray[f].uSet )
			{
				orientationArray[f].SetU( false );
				Propagate( orientationArray, pEdges+1, false );
				Propagate( orientationArray, pEdges+3, true );
			}

			if ( !orientationArray[f].vSet )
			{
				orientationArray[f].SetV( false );
				Propagate( orientationArray, pEdges+0, false );
				Propagate( orientationArray, pEdges+2, true );
			}
		}

		for( int f = 0; f < m_numPatches; f++ )
		{
			SubD_Face_t *pPatch = &m_faceList[f];

			const Orientation &o = orientationArray[f];	// Determine edge from orientation flags.
			static const int nTimesToRotate[4] = {0, 1, 3, 2};
			const int idx = nTimesToRotate[(o.v << 1) + o.u];

			RotateFace( pPatch, idx );
		}
	}

	void COptimizeSubDBuilder::TagCreases()
	{
		static int MOD4[] = {0,1,2,3,0,1,2,3};

		for (unsigned short i=0; i<m_numPatches; i++)
		{
			SubD_Face_t *pPatch = &m_faceList[i];

			for ( int k=0; k<4; k++ ) // for all vertices
			{
				if ( pPatch->halfEdges[k].twin != NULL )
				{
					HalfEdge *twin = pPatch->halfEdges[k].twin;
					SubD_Face_t *nbQuad = twin->patch;
					int quad0vtx0ID =   pPatch->vtxIDs[ MOD4[k+0] ];
					int quad1vtx0ID = nbQuad->vtxIDs[ MOD4[twin->localID+1] ];

					int quad0vtx1ID =   pPatch->vtxIDs[ MOD4[k+1] ];
					int quad1vtx1ID = nbQuad->vtxIDs[ MOD4[twin->localID+0] ];

					if ( ( VTXNOR( quad0vtx0ID ) != VTXNOR( quad1vtx0ID ) ) ||
						 ( VTXNOR( quad0vtx1ID ) != VTXNOR( quad1vtx1ID ) ) )
					{
						pPatch->bndEdge[k] = true;
						pPatch->bndVtx[MOD4[k+0]] = true;
						pPatch->bndVtx[MOD4[k+1]] = true;
					}
				}

			}
		}
	}

}; // namespace 