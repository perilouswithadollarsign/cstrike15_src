//--------------------------------------------------------------------------------------------------
/**
	@file		qhConvex.h

	@author		Dirk Gregorius
	@version	0.1
	@date		30/11/2011

	Copyright(C) 2011 by D. Gregorius. All rights reserved.
*/
//--------------------------------------------------------------------------------------------------
#pragma once

#include "qhTypes.h"
#include "qhMath.h"
#include "qhArray.h"
#include "qhList.h"
#include "qhHalfEdge.h"
#include "qhMass.h"


//--------------------------------------------------------------------------------------------------
// qhMesh
//--------------------------------------------------------------------------------------------------
struct qhMesh
{
	qhArray< qhVector3 > Vertices;  // Vertices
	qhArray< qhVector3 > Normals;   // *Face* normals
	qhArray< int > Faces;			// Index count for each face
	qhArray< int > Indices;			// Face indices into vertex array
};


//--------------------------------------------------------------------------------------------------
// qhIteration
//--------------------------------------------------------------------------------------------------
struct qhIteration
	{
	qhVector3 Apex;
	qhArray< qhVector3 > Horizon;
	qhArray< qhVector3 > Vertices;
	qhArray< int > Faces;
	};


//--------------------------------------------------------------------------------------------------
// qhConvex
//--------------------------------------------------------------------------------------------------
class qhConvex
	{
	public:
		// Construction / Destruction
		qhConvex( void );
		~qhConvex( void );
		
		void Construct( int VertexCount, const qhVector3* VertexBase, qhReal RelativeWeldTolerance );
		void Construct( int PlaneCount, const qhPlane* PlaneBase, qhReal RelativeWeldTolerance, const qhVector3& InternalPoint = QH_VEC3_ZERO );
		bool IsConsistent( void ) const;

		void Simplify( qhConvex& Convex, qhReal MaxAngle ) const;
		
		// Accessors / Mutators
		qhVector3 GetCentroid( void ) const;

		int GetVertexCount( void ) const;
		int GetEdgeCount( void ) const;
		int GetFaceCount( void ) const;

		const qhList< qhVertex >& GetVertexList( void ) const;
		const qhList< qhFace >& GetFaceList( void ) const;

		// Polygonal mesh for rendering
		void GetMesh( qhMesh& Mesh ) const;

		// Mass properties (relative to origin)
		qhMass ComputeMass( qhReal Density = qhReal( 1 ) ) const;

		// Debug information
		int GetIterationCount( void ) const;
		const qhIteration& GetIteration( int Index ) const;

	private:
		// Memory management
		qhVertex* CreateVertex( const qhVector3& Position );
		void DestroyVertex( qhVertex* Vertex );
		qhFace* CreateFace( qhVertex* Vertex1, qhVertex* Vertex2, qhVertex* Vertex3 );
		void DestroyFace( qhFace* Face );

		// Implementation
		void ComputeTolerance( qhArray< qhVector3 >& Vertices );
		bool BuildInitialHull( int VertexCount, const qhVector3* VertexBase );
		qhVertex* NextConflictVertex( void );
		void AddVertexToHull( qhVertex* Vertex );
		void AddIteration( qhVertex* Apex, const qhArray< qhHalfEdge* >& Horizon, const qhList< qhFace >& FaceList );
		
		void CleanHull( void );
		void ShiftHull( const qhVector3& Translation );
		
		void BuildHorizon( qhArray< qhHalfEdge* >& Horizon, qhVertex* Apex, qhFace* Seed, qhHalfEdge* Edge1 = NULL );
		void BuildCone( qhArray< qhFace* >& Cone, const qhArray< qhHalfEdge* >& Horizon, qhVertex* Apex );
		void MergeFaces( qhArray< qhFace* >& Cone );
		void ResolveVertices( qhArray< qhFace* >& Cone );
		void ResolveFaces( qhArray< qhFace* >& Cone );
		
		bool FirstPass( qhFace* Face );
		bool SecondPass( qhFace* Face  );
		void ConnectFaces( qhHalfEdge* Edge );
		void ConnectEdges( qhHalfEdge* Prev, qhHalfEdge* Next, qhArray< qhFace* >& MergedFaces );
		void DestroyEdges( qhHalfEdge* Begin, qhHalfEdge* End );
		void AbsorbFaces( qhFace* Face, qhArray< qhFace* >& MergedFaces );

		// Data members
		qhReal mTolerance;
		qhReal mMinRadius;
		qhReal mMinOutside;	
		
		qhVector3 mInteriorPoint;
		qhList< qhVertex > mOrphanedList;
		qhList< qhVertex > mVertexList;
		qhList< qhFace > mFaceList;

		qhArray< qhIteration > mIterations;

		// Non-copyable
		qhConvex( const qhConvex& );
		qhConvex& operator=( const qhConvex& );
	};


#include "qhConvex.inl"
