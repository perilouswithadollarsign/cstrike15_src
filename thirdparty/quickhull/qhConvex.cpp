//--------------------------------------------------------------------------------------------------
// qhConvex.cpp
//
// Copyright(C) 2011 by D. Gregorius. All rights reserved.
//--------------------------------------------------------------------------------------------------
#include "qhConvex.h"

#include <algorithm>
#include <limits>

#define QH_AXIS_X	0
#define QH_AXIS_Y	1
#define QH_AXIS_Z	2


//--------------------------------------------------------------------------------------------------
// Local construction utilities
//--------------------------------------------------------------------------------------------------
struct qhHalfSpace
{
	qhVector3 Vertex;
	qhReal Area;
};


//--------------------------------------------------------------------------------------------------
static inline qhReal qhAngle( const qhHalfSpace& H1, const qhHalfSpace& H2 )
{
	qhReal Cos = qhDot( H1.Vertex, H2.Vertex ) / ( qhLength( H1.Vertex ) * qhLength( H2.Vertex ) ); 
	return qhArcCos( qhClamp( Cos, qhReal( -1 ), qhReal( 1 ) ) );
}


//--------------------------------------------------------------------------------------------------
static inline qhHalfSpace qhMerge( const qhHalfSpace& H1, const qhHalfSpace& H2 )
{
	qhHalfSpace Out;
	Out.Vertex = ( H1.Area * H1.Vertex + H2.Area * H2.Vertex ) / ( H1.Area + H2.Area );
	Out.Area = H1.Area + H2.Area;

	return Out;
}


//--------------------------------------------------------------------------------------------------
static inline void qhSwap( qhIteration& Lhs, qhIteration& Rhs )
	{
	qhSwap( Lhs.Apex, Rhs.Apex );
	qhSwap( Lhs.Horizon, Rhs.Horizon );
	qhSwap( Lhs.Vertices, Rhs.Vertices );
	qhSwap( Lhs.Faces, Rhs.Faces );
	}


//--------------------------------------------------------------------------------------------------
static inline qhVector3 qhBuildCentroid( int VertexCount, const qhVector3* Vertices )
{
	qhVector3 Centroid = QH_VEC3_ZERO;
	for ( int i = 0; i < VertexCount; ++i )
		{
		Centroid += Vertices[ i ];
		}

	QH_ASSERT( VertexCount > 3 );
	return Centroid / qhReal( VertexCount );
}


//--------------------------------------------------------------------------------------------------
static void qhShiftVertices( qhArray< qhVector3 >& Vertices, int VertexCount, const qhVector3* VertexBase, const qhVector3& Translation )
{
	Vertices.Resize( VertexCount );
	for ( int i = 0; i < VertexCount; ++i )
	{
		Vertices[ i ] = VertexBase[ i ] + Translation;
	}
}


//--------------------------------------------------------------------------------------------------
static inline qhBounds3 qhBuildBounds( qhArray< qhVector3 >& Vertices )
{
	qhBounds3 Bounds = QH_BOUNDS3_EMPTY;
	for ( int i = 0; i < Vertices.Size(); ++i )
	{
		Bounds += Vertices[ i ];
	}

	return Bounds;
}


//--------------------------------------------------------------------------------------------------
static void qhWeldVertices( qhArray< qhVector3 >& Vertices, const qhVector3& Tolerance )
	{
	// DIRK_TODO: This is O(n^2). If this becomes a performance bottleneck
	// since we feed large vertex buffers we should use a grid. 
	for ( int i = 0; i < Vertices.Size(); ++i )
		{
		for ( int k = Vertices.Size() - 1; k > i; --k )
			{
			QH_ASSERT( k > i );
			qhVector3 Offset = Vertices[ i ] - Vertices[ k ];
			if ( qhAbs( Offset.X ) < Tolerance.X && qhAbs( Offset.Y ) < Tolerance.Y && qhAbs( Offset.Z ) < Tolerance.Z )
				{
				Vertices[ k ] = Vertices.Back();
				Vertices.PopBack();
				}
			}
		}	
	}


//--------------------------------------------------------------------------------------------------
static void qhFindFarthestPointsAlongCardinalAxes( int& Index1, int& Index2, qhReal Tolerance, int VertexCount, const qhVector3* VertexBase ) 
	{
	Index1 = Index2 = -1;
	qhVector3 V0 = VertexBase[ 0 ];
	qhVector3 Min[ 3 ] = { V0, V0, V0 };
	qhVector3 Max[ 3 ] = { V0, V0, V0 };
	int MinIndex[ 3 ] = { 0, 0, 0 };
	int MaxIndex[ 3 ] = { 0, 0, 0 };

	for ( int i = 1; i < VertexCount; ++i )
		{
		const qhVector3& V = VertexBase[ i ];

		// X-Axis
		if ( V.X < Min[ QH_AXIS_X ].X  )
			{
			Min[ QH_AXIS_X ] = V;
			MinIndex[ QH_AXIS_X ] = i;
			}
		else if ( V.X > Max[ QH_AXIS_X ].X )
			{
			Max[ QH_AXIS_X ] = V;
			MaxIndex[ QH_AXIS_X ] = i;
			}

		// Y-Axis
		if ( V.Y < Min[ QH_AXIS_Y ].Y )
			{
			Min[ QH_AXIS_Y ] = V;
			MinIndex[ QH_AXIS_Y ] = i;
			}
		else if ( V.Y > Max[ QH_AXIS_Y ].Y )
			{
			Max[ QH_AXIS_Y ] = V;
			MaxIndex[ QH_AXIS_Y ] = i;
			}

		// Z-Axis
		if ( V.Z < Min[ QH_AXIS_Z ].Z )
			{
			Min[ QH_AXIS_Z ] = V;
			MinIndex[ QH_AXIS_Z ] = i;
			}
		else if ( V.Z > Max[ QH_AXIS_Z ].Z )
			{
			Max[ QH_AXIS_Z ] = V;
			MaxIndex[ QH_AXIS_Z ] = i;
			}
		}

	qhVector3 Distance;
	Distance[ QH_AXIS_X ] = Max[ QH_AXIS_X ].X - Min[ QH_AXIS_X ].X; 
	Distance[ QH_AXIS_Y ] = Max[ QH_AXIS_Y ].Y - Min[ QH_AXIS_Y ].Y; 
	Distance[ QH_AXIS_Z ] = Max[ QH_AXIS_Z ].Z - Min[ QH_AXIS_Z ].Z; 

	int MaxElement = qhMaxElement( Distance );
	if ( Distance[ MaxElement ] > qhReal( 100 ) * Tolerance )
		{
		Index1 = MinIndex[ MaxElement ];
		Index2 = MaxIndex[ MaxElement ];
		}
	}


//--------------------------------------------------------------------------------------------------
static int qhFindFarthestPointFromLine( int Index1, int Index2, qhReal Tolerance, int VertexCount, const qhVector3* VertexBase ) 
	{
	const qhVector3& A = VertexBase[ Index1 ];
	const qhVector3& B = VertexBase[ Index2 ];

	qhVector3 AB = B - A;
	qhReal MaxDistance = qhReal( 100 ) * Tolerance;
	int MaxIndex = -1;

	for ( int i = 0; i < VertexCount; ++i )
		{
		if ( i == Index1 || i == Index2 )
			{
			continue;
			}

		const qhVector3& P = VertexBase[ i ];

		qhVector3 AP = P - A;
		qhReal s = qhDot( AP, AB ) / qhDot( AB, AB );
		qhVector3 Q = A + s * AB;

		qhReal Distance = qhDistance( P, Q );
		if ( Distance > MaxDistance )
			{
			MaxDistance = Distance;
			MaxIndex = i;
			}
		}

	return MaxIndex;
	}


//--------------------------------------------------------------------------------------------------
static int qhFindFarthestPointFromPlane( int Index1, int Index2, int Index3, qhReal Tolerance, int VertexCount, const qhVector3* VertexBase ) 
	{
	const qhVector3& A = VertexBase[ Index1 ];
	const qhVector3& B = VertexBase[ Index2 ];
	const qhVector3& C = VertexBase[ Index3 ];
	qhPlane Plane = qhPlane( A, B, C );
	Plane.Normalize();
	
	qhReal MaxDistance = qhReal( 100 ) * Tolerance;
	int MaxIndex = -1;

	for ( int i = 0; i < VertexCount; ++i )
		{
		if ( i == Index1 || i == Index2 || i == Index3 )
			{
			continue;
			}

		qhReal Distance = qhAbs( Plane.Distance( VertexBase[ i ] ) );
		if ( Distance > MaxDistance )
			{
			MaxDistance = Distance;
			MaxIndex = i;
			}
		}

	return MaxIndex;
	}


//--------------------------------------------------------------------------------------------------
// qhConvex
//--------------------------------------------------------------------------------------------------
qhConvex::qhConvex( void )
	: mTolerance( 0 )
	, mMinRadius( 0 )
	, mMinOutside( 0 )
	, mInteriorPoint( QH_VEC3_ZERO )
	{

	}


//--------------------------------------------------------------------------------------------------
qhConvex::~qhConvex( void )
	{
	// Destroy faces
	qhFace* Face = mFaceList.Begin();
	while ( Face != mFaceList.End() )
		{
		qhFace* Nuke = Face;
		Face = Face->Next;

		qhRemove( Nuke );
		DestroyFace( Nuke );
		}

	// Destroy vertices
	qhVertex* Vertex = mVertexList.Begin();
	while ( Vertex != mVertexList.End() )
		{
		qhVertex* Nuke = Vertex;
		Vertex = Vertex->Next;

		qhRemove( Nuke );
		DestroyVertex( Nuke );
		}
	}


//--------------------------------------------------------------------------------------------------
void qhConvex::Construct( int VertexCount, const qhVector3* VertexBase, qhReal RelativeWeldTolerance )
	{
	QH_ASSERT( mIterations.Size() == 0 );

	// Validate passed arguments
	if ( VertexCount < 4 || VertexBase == NULL )
	{
		return;
	}

	// Pre-process: Shift to origin and remove duplicates
	qhVector3 Centroid = qhBuildCentroid( VertexCount, VertexBase );

	qhArray< qhVector3 > Vertices;
	qhShiftVertices( Vertices, VertexCount, VertexBase, -Centroid );
	qhBounds3 Bounds = qhBuildBounds( Vertices );
	qhWeldVertices( Vertices, RelativeWeldTolerance * ( Bounds.Max - Bounds.Min ) );

	// Try to build an initial hull 
	ComputeTolerance( Vertices );

	if ( !BuildInitialHull( Vertices.Size(), Vertices.Begin() ) )
		{
		return;
		}

	// Construct hull
	qhVertex* Vertex = NextConflictVertex();
	while ( Vertex != NULL )
		{
		AddVertexToHull( Vertex );
		Vertex = NextConflictVertex();
		}

	// Post-process: Clean and shift back to center
	CleanHull();

	// Shift hull back to original centroid
	ShiftHull( Centroid );
	}


//--------------------------------------------------------------------------------------------------
void qhConvex::Construct( int PlaneCount, const qhPlane* PlaneBase, qhReal RelativeWeldTolerance, const qhVector3& InternalPoint )
	{
	QH_ASSERT( mIterations.Size() == 0 );

	// Validate passed arguments
	if ( PlaneCount < 4 || PlaneBase == NULL )
		{
		return;
		}

	// Try to build dual
	qhArray< qhVector3 > DualVertices;
	DualVertices.Resize( PlaneCount );

	for ( int Index = 0; Index < PlaneCount; ++Index )
		{
		// Shift planes so we contain the origin
		qhPlane Plane = PlaneBase[ Index ];
		Plane.Translate( -InternalPoint );
		if ( Plane.Offset <= 0.0f )
			{
			return;
			}

		DualVertices[ Index ] = Plane.Normal / Plane.Offset;
		}

	qhConvex Dual;
	Dual.Construct( DualVertices.Size(), DualVertices.Begin(), 0.0f );
	if ( !Dual.IsConsistent() )
		{
		return;
		}

	// Build primal (dual of dual -> this is the convex hull defined by the planes)
	const qhList< qhFace >& FaceList = Dual.GetFaceList();

	qhArray< qhVector3 > PrimalVertices;
	PrimalVertices.Reserve( FaceList.Size() );

	for ( const qhFace* Face = FaceList.Begin(); Face != FaceList.End(); Face = Face->Next )
		{
		qhPlane Plane = Face->Plane;
		QH_ASSERT( Plane.Offset > 0.0f );

		// Shift vertices back
		qhVector3 Vertex = Plane.Normal / Plane.Offset + InternalPoint;
		PrimalVertices.PushBack( Vertex );
		}

	Construct( PrimalVertices.Size(), PrimalVertices.Begin(), RelativeWeldTolerance );
	}



//--------------------------------------------------------------------------------------------------
bool qhConvex::IsConsistent( void ) const
	{
	// Convex polyhedron invariants
	int V = GetVertexCount();
	int E = GetEdgeCount() / 2;
	int F = GetFaceCount();

	// Euler's identity 
	if ( V - E + F != 2 )
		{
		return false;
		}

	// Edge and face invariants
	for ( const qhFace* Face = mFaceList.Begin(); Face != mFaceList.End(); Face = Face->Next )
		{
		// Face invariants (Topology)
		if ( Face->Edge->Face != Face )
			{
			return false;
			}

		// Face invariants (Geometry)
		if ( Face->Plane.Distance( mInteriorPoint ) > 0 )
			{
			return false;
			}

		if ( !qhCheckConsistency( Face ) )
			{
			return false;
			}

		if ( Face->Mark != QH_MARK_VISIBLE )
			{
			return false;
			}

		const qhHalfEdge* Edge = Face->Edge;

		do 
			{
			// Edge invariants (Topology)
			if ( Edge->Next->Origin != Edge->Twin->Origin )
				{
				return false;
				}

			if ( Edge->Prev->Next != Edge )
				{
				return false;
				}

			if ( Edge->Next->Prev != Edge )
				{
				return false;
				}

			if ( Edge->Twin->Twin != Edge )
				{
				return false;
				}

			if ( Edge->Face != Face )
				{
				return false;
				}

			// Edge invariants (Geometry)
			if ( qhDistance( Edge->Origin->Position, Edge->Twin->Origin->Position ) < qhReal( 1000 ) * QH_REAL_MIN )
				{
				return false;
				}

			Edge = Edge->Next;
			} 
		while ( Edge != Face->Edge );
		}

	return true;
	}


//--------------------------------------------------------------------------------------------------
void qhConvex::Simplify( qhConvex& Convex, qhReal MaxAngle ) const
	{
	// Cluster all normals within the face tolerance 
	typedef qhArray< const qhFace* > qhCluster;
	qhArray< qhCluster > Clusters;

	for ( const qhFace* Face = mFaceList.Begin(); Face != mFaceList.End(); Face = Face->Next )
		{
		int BestIndex = -1;
		qhReal BestDot = qhCos( MaxAngle );

		for ( int Index = 0; Index < Clusters.Size(); ++Index )
			{
			const qhCluster& Cluster = Clusters[ Index ];
			QH_ASSERT( !Cluster.Empty() );

			qhReal Dot = qhDot( Face->Plane.Normal, Cluster[ 0 ]->Plane.Normal );
			if ( Dot > BestDot )
				{
				BestIndex = Index;
				BestDot = Dot;
				}
			}

		qhCluster& Cluster = BestIndex < 0 ? Clusters.Expand() : Clusters[ BestIndex ];
		Cluster.PushBack( Face );
		}

	// Build dual
	qhArray< qhVector3 > DualVertices;
	qhBounds3 DualBounds = QH_BOUNDS3_EMPTY;
	qhVector3 Centroid = GetCentroid();

	for ( int I = 0; I < Clusters.Size(); ++I )
		{
		const qhCluster& Cluster = Clusters[ I ];
		QH_ASSERT( !Cluster.Empty() );

		qhReal Area = 0;
		qhVector3 Vertex = QH_VEC3_ZERO;
		for ( int K = 0; K < Cluster.Size(); ++K )
			{
			const qhFace* Face = Cluster[ K ];
			qhPlane Plane = Face->Plane;
			Plane.Translate( -Centroid );
			QH_ASSERT( Plane.Offset > qhReal( 0 ) );

			Area += Face->Area;
			Vertex += Face->Area * ( Plane.Normal / Plane.Offset );
			}
		QH_ASSERT( Area > qhReal( 0 ) );
		Vertex /= Area;

		DualVertices.PushBack( Vertex );
		DualBounds += Vertex;
		}

	qhConvex Dual;
	Dual.Construct( DualVertices.Size(), DualVertices.Begin(), 0.0f );
	if ( !Dual.IsConsistent() )
		{
		return;
		}

	// Build the final hull
	qhArray< qhVector3 > Vertices;
	for ( const qhFace* Face = Dual.GetFaceList().Begin(); Face != Dual.GetFaceList().End(); Face = Face->Next )
		{
		const qhPlane& Plane = Face->Plane;
		QH_ASSERT( Plane.Offset > qhReal( 0 ) );

		qhVector3 Vertex = ( Plane.Normal / Plane.Offset ) + Centroid;
		Vertices.PushBack( Vertex );
		}

	Convex.Construct( Vertices.Size(), Vertices.Begin(), 0.0f );


	// Build half-spaces
// 	qhVector3 Centroid = GetCentroid();
// 
// 	qhArray< qhHalfSpace > HalfSpaces;
// 	HalfSpaces.Reserve( GetFaceCount() );
// 
// 	for ( const qhFace* Face = mFaceList.Begin(); Face != mFaceList.End(); Face = Face->Next )
// 		{
// 		qhPlane Plane = Face->Plane;
// 		Plane.Translate( -Centroid );
// 		QH_ASSERT( Plane.Offset > qhReal( 0 ) );
// 
// 		qhHalfSpace HalfSpace;
// 		HalfSpace.Vertex = Plane.Normal / Plane.Offset;
// 		HalfSpace.Area = Face->Area;
// 
// 		HalfSpaces.PushBack( HalfSpace );
// 		}
// 	
// 	// Merge faces within specified range
// 	while ( true )
// 		{
// 		// Find global minimum
// 		qhReal BestAngle = QH_PI;
// 		int BestIndex1 = -1;
// 		int BestIndex2 = -1;
// 
// 		for ( int Index1 = 0; Index1 < HalfSpaces.Size(); ++Index1 )
// 			{
// 			const qhHalfSpace& HalfSpace1 = HalfSpaces[ Index1 ];
// 			for ( int Index2 = Index1 + 1; Index2 < HalfSpaces.Size(); ++Index2 )
// 				{
// 				qhHalfSpace HalfSpace2 = HalfSpaces[ Index2 ];
// 
// 				qhReal Angle = qhAngle( HalfSpace1, HalfSpace2 );
// 				if ( Angle < BestAngle )
// 					{
// 					BestAngle = Angle;
// 					BestIndex1 = Index1;
// 					BestIndex2 = Index2;
// 					}
// 				}
// 			}
// 
// 		// Exit if there are no more faces to merge
// 		if ( BestAngle > MaxAngle )
// 			{
// 			break;
// 			}
// 
// 		// Merge minimizing faces
// 		const qhHalfSpace& HalfSpace1 = HalfSpaces[ BestIndex1 ];
// 		const qhHalfSpace& HalfSpace2 = HalfSpaces[ BestIndex2 ];
// 		qhHalfSpace HalfSpace = qhMerge( HalfSpace1, HalfSpace2 );
// 
// 		// Remove merged half-spaces and add the new one. We need to remove
// 		// the second half-space first to not invalidate the first index!
// 		QH_ASSERT( BestIndex1 < BestIndex2 );
// 		HalfSpaces[ BestIndex2 ] = HalfSpaces.Back();
// 		HalfSpaces.PopBack();
// 		HalfSpaces[ BestIndex1 ] = HalfSpaces.Back();
// 		HalfSpaces.PopBack();
// 
// 		HalfSpaces.PushBack( HalfSpace );
// 		}
// 
// 	// Build the dual
// 	qhArray< qhVector3 > DualVertices;
// 	DualVertices.Resize( HalfSpaces.Size() );
// 	for ( int Index = 0; Index < HalfSpaces.Size(); ++Index )
// 		{
// 		DualVertices[ Index ] = HalfSpaces[ Index ].Vertex;
// 		}
// 
// 	qhConvex Dual;
// 	Dual.Construct( DualVertices.Size(), DualVertices.Begin(), qhReal( 0 ) );
// 	if ( !Dual.IsConsistent() )
// 		{
// 		return;
// 		}
// 
// 	// Build the merged hull
// 	qhArray< qhVector3 > Vertices;
// 	for ( const qhFace* Face = Dual.GetFaceList().Begin(); Face != Dual.GetFaceList().End(); Face = Face->Next )
// 		{
// 		const qhPlane& Plane = Face->Plane;
// 		QH_ASSERT( Plane.Offset > qhReal( 0 ) );
// 		
// 		qhVector3 Vertex = ( Plane.Normal / Plane.Offset ) + Centroid;
// 		Vertices.PushBack( Vertex );
// 		}
// 	
// 	Convex.Construct( Vertices.Size(), Vertices.Begin(), 0.0f );
	}


//--------------------------------------------------------------------------------------------------
qhVertex* qhConvex::CreateVertex( const qhVector3& Position )
	{
	qhVertex* Vertex = (qhVertex*)qhAlloc( sizeof( qhVertex ) );
	new ( Vertex ) qhVertex;

	Vertex->Prev = NULL;
	Vertex->Next = NULL;

	Vertex->Mark = QH_MARK_CONFIRM;
	Vertex->Position = Position;
	Vertex->Edge = NULL;
	Vertex->ConflictFace = NULL;
	
	return Vertex;
	}


//--------------------------------------------------------------------------------------------------
void qhConvex::DestroyVertex( qhVertex* Vertex )
	{
	QH_ASSERT( !qhInList( Vertex ) );

	Vertex->~qhVertex();
	qhFree( Vertex );
	}


//--------------------------------------------------------------------------------------------------
qhFace* qhConvex::CreateFace( qhVertex* Vertex1, qhVertex* Vertex2, qhVertex* Vertex3 )
	{
	qhFace* Face = (qhFace*)qhAlloc( sizeof( qhFace ) );
	new ( Face ) qhFace;

	qhHalfEdge* Edge1 = (qhHalfEdge*)qhAlloc( sizeof( qhHalfEdge ) );
	qhHalfEdge* Edge2 = (qhHalfEdge*)qhAlloc( sizeof( qhHalfEdge ) );
	qhHalfEdge* Edge3 = (qhHalfEdge*)qhAlloc( sizeof( qhHalfEdge ) );

	qhPlane Plane = qhPlane( Vertex1->Position, Vertex2->Position, Vertex3->Position );
	qhReal Area = qhLength( Plane.Normal ) / qhReal( 2 );
	Plane.Normalize();

	// Initialize face
	Face->Prev = NULL;
	Face->Next = NULL;

	Face->Edge = Edge1;

	Face->Mark = QH_MARK_VISIBLE;
	Face->Area = Area;
	Face->Centroid = ( Vertex1->Position + Vertex2->Position + Vertex3->Position ) / qhReal( 3.0 ); 
	Face->Plane = Plane;
	Face->Flipped = Plane.Distance( mInteriorPoint ) > 0.0f;

	// Initialize edges
	Edge1->Prev = Edge3;
	Edge1->Next = Edge2;
	Edge1->Origin = Vertex1;
	Edge1->Face = Face;
	Edge1->Twin = NULL;

	Edge2->Prev = Edge1;
	Edge2->Next = Edge3;
	Edge2->Origin = Vertex2;
	Edge2->Face = Face;
	Edge2->Twin = NULL;

	Edge3->Prev = Edge2;
	Edge3->Next = Edge1;
	Edge3->Origin = Vertex3;
	Edge3->Face = Face;
	Edge3->Twin = NULL;

	return Face;
	}


//--------------------------------------------------------------------------------------------------
void qhConvex::DestroyFace( qhFace* Face )
	{
	QH_ASSERT( !qhInList( Face ) );
	
	// Edge can be null if face was merged
	qhHalfEdge* Edge = Face->Edge;
	if ( Edge != NULL )
		{
		do 
			{
			qhHalfEdge* Nuke = Edge;
			Edge = Edge->Next;

			qhFree( Nuke );
			} 
		while ( Edge != Face->Edge );
		}

	Face->~qhFace();
	qhFree( Face );
	}


//--------------------------------------------------------------------------------------------------
void qhConvex::ComputeTolerance( qhArray< qhVector3 >& Vertices )
	{
	qhBounds3 Bounds = qhBuildBounds( Vertices );
	qhVector3 Max = qhMax( qhAbs( Bounds.Min ), qhAbs( Bounds.Max ) );

	qhReal MaxSum = Max.X + Max.Y + Max.Z;
	qhReal MaxCoord = qhMax( Max.X, qhMax( Max.Y, Max.Z ) );
	qhReal MaxDistance = qhMin( QH_SQRT3 * MaxCoord, MaxSum );

	qhReal Tolerance = ( qhReal( 3 ) * MaxDistance * qhReal( 1.01 ) + MaxCoord ) * QH_REAL_EPSILON;

	mTolerance = Tolerance;
	mMinRadius = qhReal( 4 ) * Tolerance; 
	mMinOutside = qhReal( 2 ) * mMinRadius;
	}


//--------------------------------------------------------------------------------------------------
bool qhConvex::BuildInitialHull( int VertexCount, const qhVector3* VertexBase )
	{
	int Index1, Index2;
	qhFindFarthestPointsAlongCardinalAxes( Index1, Index2, mTolerance, VertexCount, VertexBase );
	if ( Index1 < 0 || Index2 < 0 )
		{
		return false;
		}

	int Index3 = qhFindFarthestPointFromLine( Index1, Index2, mTolerance, VertexCount, VertexBase );
	if ( Index3 < 0 )
		{
		return false;
		}

	int Index4 = qhFindFarthestPointFromPlane( Index1, Index2, Index3, mTolerance, VertexCount, VertexBase );
	if ( Index4 < 0 )
		{
		return false;
		}

	// Compute an interior point to detect flipped faces
	mInteriorPoint = QH_VEC3_ZERO;
	mInteriorPoint += VertexBase[ Index1 ];
	mInteriorPoint += VertexBase[ Index2 ];
	mInteriorPoint += VertexBase[ Index3 ];
	mInteriorPoint += VertexBase[ Index4 ];
	mInteriorPoint /= qhReal( 4 );

	// Check winding order 
	qhVector3 V1 = VertexBase[ Index1 ] - VertexBase[ Index4 ];
	qhVector3 V2 = VertexBase[ Index2 ] - VertexBase[ Index4 ];
	qhVector3 V3 = VertexBase[ Index3 ] - VertexBase[ Index4 ];

	if ( qhDet( V1, V2, V3 ) < qhReal( 0.0 ) )
		{
		std::swap( Index2, Index3 );
		}

	// Allocate initial vertices and save them in the vertex list
	qhVertex* Vertex1 = CreateVertex( VertexBase[ Index1 ] );
	mVertexList.PushBack( Vertex1 );
	qhVertex* Vertex2 = CreateVertex( VertexBase[ Index2 ] );
	mVertexList.PushBack( Vertex2 );
	qhVertex* Vertex3 = CreateVertex( VertexBase[ Index3 ] );
	mVertexList.PushBack( Vertex3 );
	qhVertex* Vertex4 = CreateVertex( VertexBase[ Index4 ] );
	mVertexList.PushBack( Vertex4 );

	// Allocate initial faces and save them in the face list
	qhFace* Face1 = CreateFace( Vertex1, Vertex2, Vertex3 );
	mFaceList.PushBack( Face1 );
	qhFace* Face2 = CreateFace( Vertex4, Vertex2, Vertex1 );
	mFaceList.PushBack( Face2 );
	qhFace* Face3 = CreateFace( Vertex4, Vertex3, Vertex2 );
	mFaceList.PushBack( Face3 );
	qhFace* Face4 = CreateFace( Vertex4, Vertex1, Vertex3 );
	mFaceList.PushBack( Face4 );

	// Link faces
	qhLinkFaces( Face1, 0, Face2, 1 );
	qhLinkFaces( Face1, 1, Face3, 1 );
	qhLinkFaces( Face1, 2, Face4, 1 );
	
	qhLinkFaces( Face2, 0, Face3, 2 );
	qhLinkFaces( Face3, 0, Face4, 2 );
	qhLinkFaces( Face4, 0, Face2, 2 );

	QH_ASSERT( qhCheckConsistency( Face1 ) );
	QH_ASSERT( qhCheckConsistency( Face2 ) );
	QH_ASSERT( qhCheckConsistency( Face3 ) );
	QH_ASSERT( qhCheckConsistency( Face4 ) );

	// Fill initial conflict lists
	for ( int i = 0; i < VertexCount; ++i )
		{
		if ( i == Index1 || i == Index2 || i == Index3 || i == Index4 )
			{
			continue;
			}

		const qhVector3& Point = VertexBase[ i ];

		qhReal MaxDistance = mMinOutside;
		qhFace* MaxFace = NULL;

		for ( qhFace* Face = mFaceList.Begin(); Face != mFaceList.End(); Face = Face->Next )
			{
			qhReal Distance = Face->Plane.Distance( Point );
			if ( Distance > MaxDistance )
				{
				MaxDistance = Distance;
				MaxFace = Face;
				}
			}

		if ( MaxFace != NULL )
			{
			qhVertex* Vertex = CreateVertex( Point );

			Vertex->ConflictFace = MaxFace;
			MaxFace->ConflictList.PushBack( Vertex );
			}
		}

	return true;
	}


//--------------------------------------------------------------------------------------------------
qhVertex* qhConvex::NextConflictVertex( void )
	{
	qhVertex* MaxVertex = NULL;
	qhReal MaxDistance = mMinOutside;

	for ( qhFace* Face = mFaceList.Begin(); Face != mFaceList.End(); Face = Face->Next )
	{
		if ( !Face->ConflictList.Empty() )
		{
			for ( qhVertex* Vertex = Face->ConflictList.Begin(); Vertex != Face->ConflictList.End(); Vertex = Vertex->Next )
			{
				QH_ASSERT( Vertex->ConflictFace == Face );
				qhReal Distance = Face->Plane.Distance( Vertex->Position );

				if ( Distance > MaxDistance )
				{
					MaxDistance = Distance;
					MaxVertex = Vertex;
				}
			}
		}
	}

	return MaxVertex;
	}


//--------------------------------------------------------------------------------------------------
void qhConvex::AddVertexToHull( qhVertex* Vertex )
	{
	// Remove vertex from conflict face
	qhFace* Face = Vertex->ConflictFace;
	Vertex->ConflictFace = NULL;
	Face->ConflictList.Remove( Vertex );
	mVertexList.PushBack( Vertex );

	// Find the horizon edges
	qhArray< qhHalfEdge* > Horizon;
	BuildHorizon( Horizon, Vertex, Face );
	QH_ASSERT( Horizon.Size() >= 3 );

	// Create new cone faces
	qhArray< qhFace* > Cone;
	BuildCone( Cone, Horizon, Vertex );
	QH_ASSERT( Cone.Size() >= 3 );

#ifdef QH_DEBUG
	// Push iteration before merging faces
	AddIteration( Vertex, Horizon, mFaceList );
	int Iteration = mIterations.Size() - 1;
#endif

	// Merge coplanar faces
	MergeFaces( Cone );
	
	// Resolve orphaned vertices
	ResolveVertices( Cone );

	// Remove hidden faces and add new ones
	ResolveFaces( Cone );
	}


//--------------------------------------------------------------------------------------------------
void qhConvex::AddIteration( qhVertex* Apex, const qhArray< qhHalfEdge* >& Horizon, const qhList< qhFace >& FaceList )
{
	qhIteration& Iteration = mIterations.Expand();

	// Save apex
	Iteration.Apex = Apex->Position;

	// Save horizon
	for ( int i = 0; i < Horizon.Size(); ++i )
	{
		const qhHalfEdge* Edge = Horizon[ i ];
		Iteration.Horizon.PushBack( Edge->Origin->Position );
	}

	// Save current hull faces
	for ( const qhFace* Face = FaceList.Begin(); Face != FaceList.End(); Face = Face->Next )
	{
		int VertexCount = 0;
		const qhHalfEdge* Edge = Face->Edge;

		do 
		{
			VertexCount++;
			Iteration.Vertices.PushBack( Edge->Origin->Position );
			Edge = Edge->Next;
		} 
		while ( Edge != Face->Edge );

		Iteration.Faces.PushBack( VertexCount );
	}
}


//--------------------------------------------------------------------------------------------------
void qhConvex::CleanHull( void )
	{
	// Mark all vertices on the hull as visible and set leaving edge
	for ( qhFace* Face = mFaceList.Begin(); Face != mFaceList.End(); Face = Face->Next )
		{
		qhHalfEdge* Edge = Face->Edge;

		do 
			{
			Edge->Origin->Mark = QH_MARK_VISIBLE;

			if ( Edge->Origin->Edge == NULL )
				{
				Edge->Origin->Edge = Edge;
				}

			Edge = Edge->Next;
			}
			while ( Edge != Face->Edge );
		}

	// Remove unconfirmed vertices
	qhVertex* Vertex = mVertexList.Begin();
	while ( Vertex != mVertexList.End() )
		{
		qhVertex* Next = Vertex->Next;
		if ( Vertex->Mark != QH_MARK_VISIBLE )
			{
			mVertexList.Remove( Vertex );
			DestroyVertex( Vertex );
			}

		Vertex = Next;
		}
	}


//--------------------------------------------------------------------------------------------------
void qhConvex::ShiftHull( const qhVector3& Translation )
	{
	// Transform vertices
	for ( qhVertex* Vertex = mVertexList.Begin(); Vertex != mVertexList.End(); Vertex = Vertex->Next )
		{
		Vertex->Position += Translation;
		}

	// Transform planes
	for ( qhFace* Face = mFaceList.Begin(); Face != mFaceList.End(); Face = Face->Next )
		{
		Face->Plane.Translate( Translation );
		}

	// Shift interior point
	mInteriorPoint += Translation;
	}


//--------------------------------------------------------------------------------------------------
void qhConvex::BuildHorizon( qhArray< qhHalfEdge* >& Horizon, qhVertex* Apex, qhFace* Seed, qhHalfEdge* Edge1 )
	{
	// Move vertices to orphaned list
	Seed->Mark = QH_MARK_DELETE;

	qhVertex* Vertex = Seed->ConflictList.Begin();
	while ( Vertex != Seed->ConflictList.End() )
		{
		qhVertex* Orphan = Vertex;
		Vertex = Vertex->Next;

		Orphan->ConflictFace = NULL;
		Seed->ConflictList.Remove( Orphan );

		mOrphanedList.PushBack( Orphan );
		}
	QH_ASSERT( Seed->ConflictList.Empty() );

	qhHalfEdge* Edge;
	if ( Edge1 != NULL )
		{
		Edge = Edge1->Next;
		}
	else
		{
		Edge1 = Seed->Edge;
		Edge = Edge1;
		}

	do 
		{
		qhHalfEdge* Twin = Edge->Twin;
		if ( Twin->Face->Mark == QH_MARK_VISIBLE )
			{
			if ( Twin->Face->Plane.Distance( Apex->Position ) > mMinRadius )
				{
				BuildHorizon( Horizon, Apex, Twin->Face, Twin );
				}
			else
				{
				Horizon.PushBack( Edge );
				}
			}
		
		Edge = Edge->Next;
		}
	while ( Edge != Edge1 );
	}


//--------------------------------------------------------------------------------------------------
void qhConvex::BuildCone( qhArray< qhFace* >& Cone, const qhArray< qhHalfEdge* >& Horizon, qhVertex* Apex )
	{
	// Create cone faces and link bottom edges to horizon
	for ( int i = 0; i < Horizon.Size(); ++i )
		{
		qhHalfEdge* Edge = Horizon[ i ];
		QH_ASSERT( Edge->Twin->Twin == Edge );

		qhFace* Face = CreateFace( Apex, Edge->Origin, Edge->Twin->Origin );
		Cone.PushBack( Face );
		
		// Link face to bottom edge
		qhLinkFace( Face, 1, Edge->Twin );
		}

	// Link new cone faces with each other
	qhFace* Face1 = Cone.Back();
	for ( int i = 0; i < Cone.Size(); ++i )
		{
		qhFace* Face2 = Cone[ i ];
		qhLinkFaces( Face1, 2, Face2, 0 );
		Face1 = Face2;
		}
	}


//--------------------------------------------------------------------------------------------------
void qhConvex::MergeFaces( qhArray< qhFace* >& Cone )
	{
	// Merge flipped faces
	for ( int i = 0; i < Cone.Size(); ++i )
		{
		qhFace* Face = Cone[ i ];
		if ( Face->Mark == QH_MARK_VISIBLE )
			{
			if ( Face->Flipped )
				{
				qhReal BestArea = 0;
				qhHalfEdge* BestEdge = NULL;

				qhHalfEdge* Edge = Face->Edge;

				do 
					{
					qhHalfEdge* Twin = Edge->Twin;

					qhReal Area = Twin->Face->Area;
					if ( Area > BestArea )
						{
						BestArea = Area;
						BestEdge = Edge; 
						}

					Edge = Edge->Next;
					} 
				while ( Edge != Face->Edge );

				QH_ASSERT( BestEdge != NULL );
				ConnectFaces( BestEdge );

				QH_ASSERT( Face->Mark == QH_MARK_VISIBLE );
				QH_ASSERT( Face->Flipped );
				Face->Flipped = false;
				}
			}
		}

	// First merge pass
	for ( int i = 0; i < Cone.Size(); ++i )
		{
		qhFace* Face = Cone[ i ];
		if ( Face->Mark == QH_MARK_VISIBLE )
			{
			// Merge faces which are non-convex as determined by the larger face
			while ( FirstPass( Face ) ) {}
			}
		}

	// Second merge pass
	for ( int i = 0; i < Cone.Size(); ++i )
		{
		qhFace* Face = Cone[ i ];
		if ( Face->Mark == QH_MARK_CONCAVE )
			{
			Face->Mark = QH_MARK_VISIBLE;
			while ( SecondPass( Face ) ) {}
			}
		}
	}


//--------------------------------------------------------------------------------------------------
void qhConvex::ResolveVertices( qhArray< qhFace* >& Cone )
	{
	// Resolve orphaned vertices
	qhVertex* Vertex = mOrphanedList.Begin();
	while ( Vertex != mOrphanedList.End() )
		{
		qhVertex* Next = Vertex->Next;
		mOrphanedList.Remove( Vertex );
	
		qhReal MaxDistance = mMinOutside;
		qhFace* MaxFace = NULL;
	
		for ( int i = 0; i < Cone.Size(); ++i )
			{
			// Skip faces that got merged
			if ( Cone[ i ]->Mark == QH_MARK_VISIBLE )
				{
				qhReal Distance = Cone[ i ]->Plane.Distance( Vertex->Position );
				if ( Distance > MaxDistance )
					{
					MaxDistance = Distance;
					MaxFace = Cone[ i ];
					}
				}
			}
	
		if ( MaxFace != NULL )
			{
			QH_ASSERT( MaxFace->Mark == QH_MARK_VISIBLE );
			MaxFace->ConflictList.PushBack( Vertex );
			Vertex->ConflictFace = MaxFace;
			}
		else
			{
			// Vertex has been already removed from the orphaned list 
			// and can be destroyed
			DestroyVertex( Vertex );
			Vertex = NULL;
			}
	
		Vertex = Next;
		}

	QH_ASSERT( mOrphanedList.Empty() );
	}


//--------------------------------------------------------------------------------------------------
void qhConvex::ResolveFaces( qhArray< qhFace* >& Cone )
	{
	// Delete hidden faces
	qhFace* Face = mFaceList.Begin();
	while ( Face != mFaceList.End() )
		{
		qhFace* Nuke = Face;
		Face = Face->Next;

		if ( Nuke->Mark == QH_MARK_DELETE )
			{
			QH_ASSERT( Nuke->ConflictList.Empty() );

			mFaceList.Remove( Nuke );
			DestroyFace( Nuke );
			}
		}

	// Add new faces
	for ( int i = 0; i < Cone.Size(); ++i )
		{
		if ( Cone[ i ]->Mark == QH_MARK_DELETE )
			{
			DestroyFace( Cone[ i ] );
			continue;
			}

		mFaceList.PushBack( Cone[ i ] );
		}
	}


//--------------------------------------------------------------------------------------------------
bool qhConvex::FirstPass( qhFace* Face )
	{
	bool Concave = false;

	qhHalfEdge* Edge = Face->Edge;

	do 
		{
		qhHalfEdge* Twin = Edge->Twin;

		if ( Face->Area > Twin->Face->Area )
			{
			if ( !Edge->IsConvex( mMinRadius ) )
				{
				// Merge 
				ConnectFaces( Edge );
				return true;
				}
			else if ( !Twin->IsConvex( mMinRadius ) )
				{
				// Mark as concave and handle in second pass
				Concave = true;
				}
			}
		else
			{
			if ( !Twin->IsConvex( mMinRadius ) )
				{
				// Merge 
				ConnectFaces( Edge );
				return true;
				}
			else if ( !Edge->IsConvex( mMinRadius ) )
				{
				// Mark as concave and handle in second pass
				Concave = true;
				}
			}

		Edge = Edge->Next;
		} 
	while ( Edge != Face->Edge );

	if ( Concave )
		{
		Face->Mark = QH_MARK_CONCAVE;
		}

	return false;
	}


//--------------------------------------------------------------------------------------------------
bool qhConvex::SecondPass( qhFace* Face )
	{
	qhHalfEdge* Edge = Face->Edge;

	do 
		{
		qhHalfEdge* Twin = Edge->Twin;

		if ( !Edge->IsConvex( mMinRadius ) || !Twin->IsConvex( mMinRadius ) )
			{
			ConnectFaces( Edge );
			return true;
			}

		Edge = Edge->Next;
		} 
	while ( Edge != Face->Edge );

	return false;
	}


//--------------------------------------------------------------------------------------------------
void qhConvex::ConnectFaces( qhHalfEdge* Edge )
	{
	// The absorbing face
	qhFace* Face = Edge->Face;
	QH_ASSERT( qhCheckConsistency( Face ) );

	// Find the strip of shared edges
	qhHalfEdge* Twin = Edge->Twin;
	QH_ASSERT( qhCheckConsistency( Twin->Face ) );

	qhHalfEdge* EdgePrev = Edge->Prev;
	qhHalfEdge* EdgeNext = Edge->Next;
	qhHalfEdge* TwinPrev = Twin->Prev;
	qhHalfEdge* TwinNext = Twin->Next;
	
	while ( EdgePrev->Twin->Face == Twin->Face )
		{
		QH_ASSERT( EdgePrev->Twin == TwinNext );
		QH_ASSERT( TwinNext->Twin == EdgePrev );

		EdgePrev = EdgePrev->Prev;
		TwinNext = TwinNext->Next;
		}
	QH_ASSERT( EdgePrev->Face != TwinNext->Face );

	while ( EdgeNext->Twin->Face == Twin->Face )
		{
		QH_ASSERT( EdgeNext->Twin == TwinPrev );
		QH_ASSERT( TwinPrev->Twin == EdgeNext );

		EdgeNext = EdgeNext->Next;
		TwinPrev = TwinPrev->Prev;
		}
	QH_ASSERT( EdgeNext->Face != TwinPrev->Face );

	// Make sure we don't reference a shared edge
	Face->Edge = EdgePrev;
		
	// Discard opposing face and absorb non-shared edges
	qhArray< qhFace* > MergedFaces;
	MergedFaces.PushBack( Twin->Face );
	Twin->Face->Mark = QH_MARK_DELETE;
	Twin->Face->Edge = NULL;

	for ( qhHalfEdge* Absorbed = TwinNext; Absorbed != TwinPrev->Next; Absorbed = Absorbed->Next )
		{
		Absorbed->Face = Face;
		}

	// Delete shared edges (before connection)
	DestroyEdges( EdgePrev->Next, EdgeNext );
	DestroyEdges( TwinPrev->Next, TwinNext );

	// Connect half edges (this can have side effects)
	ConnectEdges( EdgePrev, TwinNext, MergedFaces );
	ConnectEdges( TwinPrev, EdgeNext, MergedFaces );

	// Rebuild geometry for the merges face
	qhNewellPlane( Face );
	QH_ASSERT( qhCheckConsistency( Face ) );

	// Absorb conflict vertices
	AbsorbFaces( Face, MergedFaces );
	}


//--------------------------------------------------------------------------------------------------
void qhConvex::ConnectEdges( qhHalfEdge* Prev, qhHalfEdge* Next, qhArray< qhFace* >& MergedFaces )
	{
	QH_ASSERT( Prev != Next );
	QH_ASSERT( Prev->Face == Next->Face );
	
	// Check for redundant edges (this has side effects)
	// If this condition holds true both faces are in the same 
	// plane since the share three vertices.
	if ( Prev->Twin->Face == Next->Twin->Face )
		{
		// Next is redundant and will be removed. 
		// It should not be referenced by its associated face!
		if ( Next->Face->Edge == Next )
			{
			Next->Face->Edge = Prev;
			}

		qhHalfEdge* Twin;
		if ( qhVertexCount( Prev->Twin->Face ) == 3 )
			{
			Twin = Next->Twin->Prev->Twin;
			QH_ASSERT( Twin->Face->Mark != QH_MARK_DELETE );

			// If the opposing face is a triangle. We will    
			// get rid of it *and* its associated edges 
			// (Don't set OpposingFace->Edge = NULL!)
			qhFace* OpposingFace = Prev->Twin->Face;
			OpposingFace->Mark = QH_MARK_DELETE;
			MergedFaces.PushBack( OpposingFace );
			}
		else
			{
			Twin = Next->Twin;
			
			// Prev->Twin is redundant and will be removed.
			// It should not be referenced by its associated face!
			if ( Twin->Face->Edge == Prev->Twin )
				{
				Twin->Face->Edge = Twin;
				}

			Twin->Next = Prev->Twin->Next;
			Twin->Next->Prev = Twin;  

			qhFree( Prev->Twin );
			}
		
		Prev->Next = Next->Next;
		Prev->Next->Prev = Prev;

		Prev->Twin = Twin;
		Twin->Twin = Prev;

		// Destroy redundant edge and its associated vertex
		mVertexList.Remove( Next->Origin );
		DestroyVertex( Next->Origin );

		qhFree( Next );

		// Twin->Face was modified, so recompute its plane
		qhNewellPlane( Twin->Face );	
		QH_ASSERT( qhCheckConsistency( Twin->Face ) );
		}
	else
		{
		Prev->Next = Next;
		Next->Prev = Prev;
		}
	}


//--------------------------------------------------------------------------------------------------
void qhConvex::DestroyEdges( qhHalfEdge* Begin, qhHalfEdge* End )
	{
	qhHalfEdge* Edge = Begin;
	while ( Edge != End )
		{
		qhHalfEdge* Nuke = Edge;
		Edge = Edge->Next;

		// Delete vertex if there is more than one shared edge
		// DIRK_TODO: Since we run over the twin edges as well this would delete the vertex twice!
// 		if ( Nuke != Begin )
// 			{
// 			mVertexList.Remove( Nuke->Origin );
// 			DestroyVertex( Nuke->Origin );
// 			}
		
		qhFree( Nuke );
		Nuke = NULL;
		}
	}


//--------------------------------------------------------------------------------------------------
void qhConvex::AbsorbFaces( qhFace* Face, qhArray< qhFace* >& MergedFaces )
	{
	for ( int i = 0; i < MergedFaces.Size(); ++i )
		{
		QH_ASSERT( MergedFaces[ i ]->Mark == QH_MARK_DELETE );
		qhList< qhVertex >& ConflictList = MergedFaces[ i ]->ConflictList;

		qhVertex* Vertex = ConflictList.Begin();
		while ( Vertex != ConflictList.End() )
			{
			qhVertex* Next = Vertex->Next;
			ConflictList.Remove( Vertex );

			if ( Face->Plane.Distance( Vertex->Position ) > mMinOutside )
				{
				Face->ConflictList.PushBack( Vertex );
				Vertex->ConflictFace = Face;
				}
			else
				{
				mOrphanedList.PushBack( Vertex );
				}

			Vertex = Next;
			}

		QH_ASSERT( ConflictList.Empty() );
		}
	}


//-------------------------------------------------------------------------------------------------
void qhConvex::GetMesh( qhMesh& Mesh ) const
	{
	Mesh.Vertices.Clear();
	Mesh.Normals.Clear();
	Mesh.Faces.Clear();
	Mesh.Indices.Clear();

	// Save vertices
	for ( const qhVertex* Vertex = mVertexList.Begin(); Vertex != mVertexList.End(); Vertex = Vertex->Next )
		{
		Mesh.Vertices.PushBack( Vertex->Position );
		}

	// Save faces
	for ( const qhFace* Face = mFaceList.Begin(); Face != mFaceList.End(); Face = Face->Next )
		{
		Mesh.Normals.PushBack( Face->Plane.Normal );

		int IndexStart = Mesh.Indices.Size();
		const qhHalfEdge* Edge = Face->Edge;

		do 
			{
			int Index = mVertexList.IndexOf( Edge->Origin );
			Mesh.Indices.PushBack( Index );

			Edge = Edge->Next;
			}
		while ( Edge != Face->Edge );

		int IndexEnd = Mesh.Indices.Size();
		Mesh.Faces.PushBack( IndexEnd - IndexStart );
		}
	}


//-------------------------------------------------------------------------------------------------
qhMass qhConvex::ComputeMass( qhReal Density ) const
	{
	// M. Kallay - "Computing the Moment of Inertia of a Solid Defined by a Triangle Mesh"
	qhReal Volume = qhReal( 0 );
	qhVector3 Center = QH_VEC3_ZERO;

	qhReal XX = qhReal( 0 );  qhReal XY = qhReal( 0 );
	qhReal YY = qhReal( 0 );  qhReal XZ = qhReal( 0 );
	qhReal ZZ = qhReal( 0 );  qhReal YZ = qhReal( 0 );

	// Iterate over faces and triangulate in-place
	for ( const qhFace* Face = mFaceList.Begin(); Face != mFaceList.End(); Face = Face->Next )
		{
		const qhHalfEdge* Edge1 = Face->Edge;
		const qhHalfEdge* Edge2 = Edge1->Next;
		const qhHalfEdge* Edge3 = Edge2->Next;
		QH_ASSERT( Edge3 != Edge1 );

		qhVector3 V1 = Edge1->Origin->Position;

		do 
			{
			qhVector3 V2 = Edge2->Origin->Position;
			qhVector3 V3 = Edge3->Origin->Position;

			// Signed volume of this tetrahedron
			qhReal Det = qhDet( V1, V2, V3 );

			// Contribution to mass
			Volume += Det;

			// Contribution to centroid
			qhVector3 v4 = V1 + V2 + V3;
			Center += Det * v4;

			// Contribution to inertia monomials
			XX += Det * ( V1.X*V1.X + V2.X*V2.X + V3.X*V3.X + v4.X*v4.X );
			YY += Det * ( V1.Y*V1.Y + V2.Y*V2.Y + V3.Y*V3.Y + v4.Y*v4.Y );
			ZZ += Det * ( V1.Z*V1.Z + V2.Z*V2.Z + V3.Z*V3.Z + v4.Z*v4.Z );
			XY += Det * ( V1.X*V1.Y + V2.X*V2.Y + V3.X*V3.Y + v4.X*v4.Y );
			XZ += Det * ( V1.X*V1.Z + V2.X*V2.Z + V3.X*V3.Z + v4.X*v4.Z );
			YZ += Det * ( V1.Y*V1.Z + V2.Y*V2.Z + V3.Y*V3.Z + v4.Y*v4.Z );

			Edge2 = Edge3;
			Edge3 = Edge3->Next;
			} 
		while ( Edge3 != Face->Edge );
		}
	QH_ASSERT( Volume > 0.0f );

	// Fetch result
	qhMatrix3 Inertia;
	Inertia.C1.X = YY + ZZ;  Inertia.C2.X =     -XY;  Inertia.C3.X =     -XZ;
	Inertia.C1.Y =     -XY;  Inertia.C2.Y = XX + ZZ;  Inertia.C3.Y =     -YZ;
	Inertia.C1.Z =     -XZ;  Inertia.C2.Z =     -YZ;  Inertia.C3.Z = XX + YY;

	qhMass Mass;
	Mass.Weight = Density * Volume / qhReal( 6.0 );
	Mass.Center = Center / ( qhReal( 4 ) * Volume );
	Mass.Inertia = ( Density / qhReal( 120 ) ) * Inertia;

	return Mass;
	}









