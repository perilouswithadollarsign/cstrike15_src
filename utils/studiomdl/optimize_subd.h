#ifndef OPTIMIZE_SUBD_H
#define OPTIMIZE_SUBD_H
#pragma once

#include "optimize.h"
#include "studio.h"

// Maximum number of points that can be part of a subd quad.
// This includes the 4 interior points of the quad, plus the 1-ring neighborhood
#define MAX_SUBD_POINTS 32

#define MAX_SUBD_ONERING_POINTS (MAX_SUBD_POINTS + 4*5)

#define CORNER_WITH_SMOOTHBNDTANGENTS 2

namespace OptimizedModel 
{

	struct SubD_Face_t;

	// minimal HalfEdge structure, embedded in a face (#halfedges = #vertexperface)
	struct HalfEdge 
	{
		HalfEdge *twin;
		HalfEdge *sectorStart;
		unsigned char  localID; // local halfedge/vertex ID
		SubD_Face_t *patch;

		inline HalfEdge *NextInFace();
		inline HalfEdge *PrevInFace();
		inline HalfEdge *NextByHead();
		inline HalfEdge *PrevByHead();
		inline HalfEdge *NextByTail();
		inline HalfEdge *PrevByTail();

		inline unsigned short &BndEdge();
	};

	struct Orientation
	{
		uint8 u : 1;
		uint8 v : 1;
		uint8 uSet : 1;
		uint8 vSet : 1;

		void SetU( bool b )
		{
			Assert( !uSet );
			u = b;
			uSet = true;
		}

		void SetV( bool b )
		{
			Assert( !vSet );
			v = b;
			vSet = true;
		}

		Orientation() { uSet = vSet = false; }
	};

	struct SubD_Face_t
	{
		unsigned short patchID;								// for building our 4 sets of watertight UVs
		unsigned short vtxIDs[4];

		unsigned short oneRing[MAX_SUBD_ONERING_POINTS];
		unsigned short vtx1RingSize[4];						// Pre-calculated prefixes for the first 4 points
		unsigned short vtx1RingCenterQuadOffset[4];			// start of inner quad vertices in vertex 1-ring

		unsigned short valences[4];	    					// Valences for the first 4 points in current sector
		unsigned short minOneRingIndex[4]; 					// Location in oneRing array to start applying stencil (determined by lowest position index)

		unsigned short bndVtx[4];							// is vertex on the boundary?
		unsigned short bndEdge[4];							// is associated edge on the boundary?
		unsigned short cornerVtx[4];						// should a boundary-vertex be treated as a corner?

		unsigned short nbCornerVtx[4];						// bitfield, for all on-edge neighbors record if corner vertices

		unsigned short loopGapAngle[4];
		
		unsigned short edgeBias[8];
		
		unsigned short vUV0[4];								// Vert index for Interior TexCoord (for vtxIDs[0-3])
		unsigned short vUV1[4];								// Vert index for Parametric V TexCoord (for vtxIDs[0-3])
		unsigned short vUV2[4];								// Vert index for Parametric U TexCoord (for vtxIDs[0-3])
		unsigned short vUV3[4];								// Vert index for Corner TexCoord (for vtxIDs[0-3])

		HalfEdge	   halfEdges[4];
		
		void SetEdgeBias(int localID, float f0, float f1)
		{
			if (halfEdges[localID].twin==NULL) return;
			edgeBias[2*localID]   = f0 * 32768.0f;
			edgeBias[2*localID+1] = f1 * 32768.0f;
			halfEdges[localID].twin->patch->edgeBias[ 2*halfEdges[localID].twin->localID+1 ] = (1.0f - f0) * 32768.0f;
			halfEdges[localID].twin->patch->edgeBias[ 2*halfEdges[localID].twin->localID   ] = (1.0f - f1) * 32768.0f;
		}
	};

	inline HalfEdge *HalfEdge::NextInFace()
	{
		static int MOD4[8] = {0,1,2,3,0,1,2,3};
		return &patch->halfEdges[MOD4[localID+1]];
	}
	inline HalfEdge *HalfEdge::PrevInFace()
	{
		static int MOD4[8] = {0,1,2,3,0,1,2,3};
		return &patch->halfEdges[MOD4[localID+3]];
	}
	inline HalfEdge *HalfEdge::NextByHead() { return (twin==NULL)? NULL : twin->PrevInFace(); }
	inline HalfEdge *HalfEdge::PrevByHead() { return NextInFace()->twin; }
	inline HalfEdge *HalfEdge::NextByTail() { return PrevInFace()->twin; }
	inline HalfEdge *HalfEdge::PrevByTail() { return (twin==NULL)? NULL : twin->NextInFace(); }

	inline bool FaceIsRegular( SubD_Face_t *patch )
	{
		return ( patch->valences[0] == 4 && patch->valences[1] == 4 && patch->valences[2] == 4 && patch->valences[3] == 4 ) &&
			   ( patch->bndVtx[0] == false && patch->bndVtx[1] == false && patch->bndVtx[2] == false && patch->bndVtx[3] == false ) &&
			   ( patch->bndEdge[0] == false && patch->bndEdge[1] == false && patch->bndEdge[2] == false && patch->bndEdge[3] == false );
	}

	inline unsigned short &HalfEdge::BndEdge() { return patch->bndEdge[localID]; }

	typedef CUtlVector<SubD_Face_t>		    SubD_FaceList_t;
	typedef CUtlVector<Vertex_t>		    SubD_VertexList_t;
	typedef const mstudio_meshvertexdata_t *SubD_VertexData_t;

	class COptimizeSubDBuilder
	{
	public:
		COptimizeSubDBuilder(SubD_FaceList_t& subDFaceList, const SubD_VertexList_t& vertexList, const SubD_VertexData_t &vertexData, bool bIsTagged, bool bMendVertices=true );
		void ProcessPatches( bool bIsTagged, bool bMendVertices );

		HalfEdge *FindTwin(HalfEdge &he);
		void CheckForManifoldMesh( );
		void BuildNeighborhoodInfo( );

		void ComputeSectorStart( SubD_Face_t *quad, unsigned short k );
		void ComputePerVertexInfo( SubD_Face_t *baseQuad, unsigned short baseLocalID );
		void ComputeSectorAngle( SubD_Face_t *baseQuad, unsigned short baseLocalID );
		void ComputeNbCorners( SubD_Face_t *baseQuad, unsigned short baseLocalID );
		void ComputeSectorOneRing( SubD_Face_t *baseQuad, unsigned short baseLocalID );
		unsigned short FindNeighborVertex( HalfEdge** ppOutMirrorEdge, const HalfEdge *pHalfEdge, int indexAlongEdge );
		void ComputeNeighborTexcoords( SubD_Face_t *baseQuad );
		void MendVertices( SubD_Face_t *quad, unsigned short baseLocalID );
		void TagCreases();

	private:

		// Routines used for orienting faces for edge consistency
		void RotateOnce( SubD_Face_t *pFace );
		void RotateFace( SubD_Face_t *pFace, int nTimesToRotate );
		int FaceEdgeIndex( SubD_Face_t *pFace, HalfEdge *pEdge );
		void Propagate( CUtlVector<Orientation> & orientationArray, HalfEdge *pEdge, bool dir );
		void ConsistentPatchOrientation();
		void RemapIndices();
		void SetMinOneRingIndices();

		SubD_FaceList_t         &m_faceList;
		const SubD_VertexList_t &m_vtxList;
		const SubD_VertexData_t &m_vtxData;
		int						 m_numPatches;
		CUtlVector<int>			 m_IndexRemapTable;
	};


}; // namespace OptimizedModel

#endif // OPTIMIZE_SUBD_H
