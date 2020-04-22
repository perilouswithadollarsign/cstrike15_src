//========= Copyright © Valve Corporation, All rights reserved. ============//
#ifndef SERIALIZE_HDR
#define SERIALIZE_HDR	

#include "resourcefile/resourcestream.h"
#include "mathlib/aabb.h"
#include "bitvec.h"
#include "rubikon/serializehelpers.h"
#include "rubikon/constants.h"
#include "tier1/checksum_crc.h"
#include "mathlib/transform.h"


//---------------------------------------------------------------------------------------
// Sphere serialization 
//---------------------------------------------------------------------------------------
schema struct RnSphere_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( RnSphere_t );

	AABB_t GetBbox() const;
	AABB_t GetBbox( const CTransform& xform ) const;

	int GetTriangulation( Vector *pVerts = NULL ) const;
	void GetTriangulation( CUtlVector<Vector> &vertices, CUtlVector<uint32> &indices ) const;
	void GetTriangulation( CUtlVector<Vector> &vertices, CUtlVector<uint32> &indices, int nSides, int nSlices ) const;
	
	float GetVolume() const;
	Vector ComputeOrthographicAreas() const;

	Vector m_vCenter;
	float32 m_flRadius;
};

inline RnSphere_t operator*( const RnSphere_t& sphere, float flScale )
{
	RnSphere_t out;
	out.m_vCenter = sphere.m_vCenter * flScale;
	out.m_flRadius = sphere.m_flRadius * flScale;

	return out;
}


inline RnSphere_t operator*( float flScale, const RnSphere_t& sphere )
{
	RnSphere_t out;
	out.m_vCenter = sphere.m_vCenter * flScale;
	out.m_flRadius = sphere.m_flRadius * flScale;

	return out;
}


//---------------------------------------------------------------------------------------
// Capsule serialization 
//---------------------------------------------------------------------------------------
schema struct RnCapsule_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( RnCapsule_t );

	AABB_t GetBbox() const;
	AABB_t GetBbox( const CTransform& xform ) const;

	int GetTriangulation( Vector *pVerts = NULL ) const;
	void GetTriangulation( CUtlVector<Vector> &vertices, CUtlVector<uint32> &indices ) const;
	void GetTriangulation( CUtlVector<Vector> &vertices, CUtlVector<uint32> &indices, int nSides, int nSlices ) const;
	
	float GetVolume() const;
	Vector ComputeOrthographicAreas() const;

	Vector m_vCenter[ 2 ];
	float32 m_flRadius;
};


inline RnCapsule_t operator*( const RnCapsule_t& capsule, float flScale )
{
	RnCapsule_t out;
	out.m_vCenter[ 0 ] = capsule.m_vCenter[ 0 ] * flScale;
	out.m_vCenter[ 1 ] = capsule.m_vCenter[ 1 ] * flScale;
	out.m_flRadius = capsule.m_flRadius * flScale;

	return out;
}


inline RnCapsule_t operator*( float flScale, const RnCapsule_t& capsule )
{
	RnCapsule_t out;
	out.m_vCenter[ 0 ] = capsule.m_vCenter[ 0 ] * flScale;
	out.m_vCenter[ 1 ] = capsule.m_vCenter[ 1 ] * flScale;
	out.m_flRadius = capsule.m_flRadius * flScale;

	return out;
}


//--------------------------------------------------------------------------------------------------
// Ray 
//--------------------------------------------------------------------------------------------------
struct RnRay_t
{
	RnRay_t( void ) { }

	RnRay_t( const Vector& vStart, const Vector& vEnd ) 
	{
		vOrigin = vStart;
		vDelta = vEnd - vStart;

		// Pre-compute inverse
		vDeltaInv.x = vDelta.x != 0.0f ? 1.0f / vDelta.x : FLT_MAX;
		vDeltaInv.y = vDelta.y != 0.0f ? 1.0f / vDelta.y : FLT_MAX;
		vDeltaInv.z = vDelta.z != 0.0f ? 1.0f / vDelta.z : FLT_MAX;
	}

	VectorAligned vOrigin;
	VectorAligned vDelta;
	VectorAligned vDeltaInv;
};



//---------------------------------------------------------------------------------------
// Hull serialization 
//---------------------------------------------------------------------------------------
schema struct RnPlane_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( RnPlane_t );

	Vector m_vNormal;														// The plane normal
	float32 m_flOffset;														// The plane offset such that P: n*x - d = 0

	// Construction
	FORCEINLINE RnPlane_t( void )											{ }
	FORCEINLINE RnPlane_t( const Vector& n, float d )						{ m_vNormal = n; m_flOffset = d; }
	FORCEINLINE RnPlane_t( const Vector& n, const Vector& p )				{ m_vNormal = n; m_flOffset = DotProduct( n, p ); }
	
	// Utilities
	FORCEINLINE float Distance( const Vector &vPoint ) const				{ return DotProduct( m_vNormal, vPoint ) - m_flOffset; }
	FORCEINLINE bool IsValid( void ) const									{ return m_vNormal != vec3_origin; }

	FORCEINLINE bool operator == ( const RnPlane_t &other )const			{ return m_vNormal == other.m_vNormal && m_flOffset == other.m_flOffset; }
};

schema struct RnHalfEdge_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( RnHalfEdge_t );

	uint8 m_nNext;															// Next edge index in CCW circular list around face
	uint8 m_nTwin;															// Twin edge 
	uint8 m_nOrigin;														// Origin vertex index of edge
	uint8 m_nFace;															// Face index 
};

schema struct RnFace_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( RnFace_t );

	uint8 m_nEdge;															// Start edge index for CCW circular list around face 
};

schema struct RnHull_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( RnHull_t );

	Vector m_vCentroid;														// Hull centroid
	float m_flMaxAngularRadius;												// Angular radius for CCD
	CResourceArray< Vector > m_Vertices;									// Hull vertices (x1, y1, z1, x2, y2, z2, ...)
	CResourceArray< RnPlane_t > m_Planes;									// Hull face planes with outward pointing normals (n1, -d1, n2, -d2, ...)
	CResourceArray< RnHalfEdge_t > m_Edges;									// Hull half edges order such that each edge e is followed by its twin e' (e1, e1', e2, e2', ...)
	CResourceArray< RnFace_t > m_Faces;										// Hull faces
	Vector m_vOrthographicAreas; // fraction 0..1 of coverage along YZ,ZX,XY sides of AABB
	matrix3x4_t m_MassProperties; // inertia tensor (in 3x3 part, always PSD) and center of mass (translation)
	float m_flVolume;
	float m_flMaxMotionRadius;
	float m_flMinMotionThickness;
	AABB_t m_Bounds;

	FORCEINLINE int GetVertexCount( void ) const							{ return m_Vertices.Count();	}
	FORCEINLINE const Vector GetVertex( int nVertex ) const					{ return m_Vertices[ nVertex ]; }
	FORCEINLINE int GetPlaneCount( void ) const								{ return m_Planes.Count();		}
	FORCEINLINE const RnPlane_t& GetPlane( int nFace ) const				{ return m_Planes[ nFace ];		}
	FORCEINLINE const Vector& GetPlaneNormal( int nFace ) const				{ return m_Planes[ nFace ].m_vNormal;		}
	FORCEINLINE int GetEdgeCount( void ) const								{ return m_Edges.Count();		}
	FORCEINLINE const RnHalfEdge_t* GetEdge( int nEdge ) const				{ return &m_Edges[ nEdge ];		}
	FORCEINLINE int GetFaceCount( void ) const								{ return m_Faces.Count();		}
	FORCEINLINE const RnFace_t* GetFace( int nFace ) const					{ return &m_Faces[ nFace ];		}
	FORCEINLINE const Vector ComputeFaceCentroid( int nFace )const;
	int GetMemory( void ) const;

	void Transform( const matrix3x4_t& transform );
	
	AABB_t GetBbox( void ) const;
	AABB_t GetBbox( const CTransform& xform ) const;

	int GetTriangulation( Vector *pVerts = NULL ) const;
	void GetTriangulation( CUtlVector<Vector> &vertices, CUtlVector<uint32> &indices, float flScale = 1.0f ) const;
	
	float GetVolume( void ) const;
	FORCEINLINE const Vector &GetCentroid( )const							{ return m_vCentroid; }

	uintp GetRuntimeSize( void ) const;
	
	void Validate( void ) const;
};

inline void ShallowCopy( RnHull_t &dest, const RnHull_t &source )
{
	V_memcpy( &dest, &source, sizeof( RnHull_t ) );
	dest.m_Vertices = source.m_Vertices;
	dest.m_Planes   = source.m_Planes;
	dest.m_Edges    = source.m_Edges;
	dest.m_Faces    = source.m_Faces;
}


//--------------------------------------------------------------------------------------------------
// Helpers (for stack allocation)
//--------------------------------------------------------------------------------------------------
inline RnHalfEdge_t MakeEdge( uint8 nNext, uint8 nTwin, uint8 nOrigin, uint8 nFace )
{
	RnHalfEdge_t e;
	e.m_nNext = nNext;
	e.m_nTwin = nTwin;
	e.m_nOrigin = nOrigin;
	e.m_nFace = nFace;

	return e;
}


struct RnHullTriangle_t : public RnHull_t
{
	void Init( const Vector& v1, const Vector& v2, const Vector& v3 )
	{
		// Centroid 
		m_vCentroid = ( v1 + v2 + v3 ) / 3.0f;

		// Vertices
		Vector TriangleVertices;
		m_TriangleVertices[ 0 ] = v1;
		m_TriangleVertices[ 1 ] = v2;
		m_TriangleVertices[ 2 ] = v3;
		m_Vertices.WriteDirect( 3, m_TriangleVertices );

		// Planes 
		Vector n = CrossProduct( v2 - v1, v3 - v1 );
		VectorNormalize( n );

		m_TrianglePlanes[ 0 ] = RnPlane_t( n, v1 );
		m_TrianglePlanes[ 1 ] = RnPlane_t( -n, v1 );
		m_Planes.WriteDirect( 2, m_TrianglePlanes );

		// Edges (remember that each edge *must* be followed by its twin!)
		m_TriangleEdges[ 0 ] = MakeEdge( 2, 1, 0, 0 ); // Face 0 - Edge 0
		m_TriangleEdges[ 1 ] = MakeEdge( 5, 0, 1, 1 ); // Face 1 - Edge 0
		m_TriangleEdges[ 2 ] = MakeEdge( 4, 3, 1, 0 ); // Face 0 - Edge 1
		m_TriangleEdges[ 3 ] = MakeEdge( 1, 2, 2, 1 ); // Face 1 - Edge 1
		m_TriangleEdges[ 4 ] = MakeEdge( 0, 5, 2, 0 ); // Face 0 - Edge 2
		m_TriangleEdges[ 5 ] = MakeEdge( 3, 4, 0, 1 ); // Face 1 - Edge 2
		m_Edges.WriteDirect( 6, m_TriangleEdges );

		// Faces
		m_TriangleFaces[ 0 ].m_nEdge = 0;
		m_TriangleFaces[ 1 ].m_nEdge = 1;
		m_Faces.WriteDirect( 2, m_TriangleFaces );

		// Bounds
		m_Bounds.m_vMinBounds = VectorMin( v1, VectorMin( v2, v3 ) );
		m_Bounds.m_vMaxBounds = VectorMax( v1, VectorMax( v2, v3 ) );
	}

	Vector m_TriangleVertices[ 3 ];
	RnPlane_t m_TrianglePlanes[ 2 ];
	RnHalfEdge_t m_TriangleEdges[ 6 ];
	RnFace_t m_TriangleFaces[ 2 ];
};


struct RnHullBox_t : public RnHull_t
{
	void Init( const Vector& vMin, const Vector& vMax )
	{	
		// Centroid
		m_vCentroid = 0.5f * ( vMin + vMax );

		// Vertices
		Vector vExtent = vMax - m_vCentroid;
		float ex = vExtent.x;
		float ey = vExtent.y;
		float ez = vExtent.z;

		m_BoxVertices[ 0 ] = m_vCentroid + Vector(  ex,  ey,  ez );
		m_BoxVertices[ 1 ] = m_vCentroid + Vector( -ex,  ey,  ez );
		m_BoxVertices[ 2 ] = m_vCentroid + Vector( -ex, -ey,  ez );
		m_BoxVertices[ 3 ] = m_vCentroid + Vector(  ex, -ey,  ez );
		m_BoxVertices[ 4 ] = m_vCentroid + Vector(  ex,  ey, -ez );
		m_BoxVertices[ 5 ] = m_vCentroid + Vector( -ex,  ey, -ez );
		m_BoxVertices[ 6 ] = m_vCentroid + Vector( -ex, -ey, -ez );
		m_BoxVertices[ 7 ] = m_vCentroid + Vector(  ex, -ey, -ez );
		m_Vertices.WriteDirect( 8, m_BoxVertices );

		// Planes
		Vector vAxisX( 1, 0, 0 );
		Vector vAxisY( 0, 1, 0 );
		Vector vAxisZ( 0, 0, 1 );
		
		m_BoxPlanes[ 0 ] = RnPlane_t( -vAxisX, vMin );
		m_BoxPlanes[ 1 ] = RnPlane_t( vAxisX, vMax );
		m_BoxPlanes[ 2 ] = RnPlane_t( -vAxisY, vMin );
		m_BoxPlanes[ 3 ] = RnPlane_t( vAxisY, vMax );
		m_BoxPlanes[ 4 ] = RnPlane_t( -vAxisZ, vMin );
		m_BoxPlanes[ 5 ] = RnPlane_t( vAxisZ, vMax );
		m_Planes.WriteDirect( 6, m_BoxPlanes );

		// Edges (remember that each edge *must* be followed by its twin!) 
		m_BoxEdges[  0 ] = MakeEdge(  2,  1, 2, 0 );
		m_BoxEdges[  1 ] = MakeEdge( 17,  0, 1, 5 );
		m_BoxEdges[  2 ] = MakeEdge(  4,  3, 1, 0 );
		m_BoxEdges[  3 ] = MakeEdge( 20,  2, 5, 3 );
		m_BoxEdges[  4 ] = MakeEdge(  6,  5, 5, 0 );
		m_BoxEdges[  5 ] = MakeEdge( 23,  4, 6, 4 );
		m_BoxEdges[  6 ] = MakeEdge(  0,  7, 6, 0 );
		m_BoxEdges[  7 ] = MakeEdge( 18,  6, 2, 2 );
		m_BoxEdges[  8 ] = MakeEdge( 10,  9, 0, 1 );
		m_BoxEdges[  9 ] = MakeEdge( 21,  8, 3, 5 );
		m_BoxEdges[ 10 ] = MakeEdge( 12, 11, 3, 1 );
		m_BoxEdges[ 11 ] = MakeEdge( 16, 10, 7, 2 );
		m_BoxEdges[ 12 ] = MakeEdge( 14, 13, 7, 1 );
		m_BoxEdges[ 13 ] = MakeEdge( 19, 12, 4, 4 );
		m_BoxEdges[ 14 ] = MakeEdge(  8, 15, 4, 1 );
		m_BoxEdges[ 15 ] = MakeEdge( 22, 14, 0, 3 );
		m_BoxEdges[ 16 ] = MakeEdge(  7, 17, 3, 2 );
		m_BoxEdges[ 17 ] = MakeEdge(  9, 16, 2, 5 );
		m_BoxEdges[ 18 ] = MakeEdge( 11, 19, 6, 2 );
		m_BoxEdges[ 19 ] = MakeEdge(  5, 18, 7, 4 );
		m_BoxEdges[ 20 ] = MakeEdge( 15, 21, 1, 3 );
		m_BoxEdges[ 21 ] = MakeEdge(  1, 20, 0, 5 );
		m_BoxEdges[ 22 ] = MakeEdge(  3, 23, 4, 3 );
		m_BoxEdges[ 23 ] = MakeEdge( 13, 22, 5, 4 );
 		m_Edges.WriteDirect( 24, m_BoxEdges );

		// Faces
		m_BoxFaces[ 0 ].m_nEdge = 0;
		m_BoxFaces[ 1 ].m_nEdge = 8;
		m_BoxFaces[ 2 ].m_nEdge = 16;
		m_BoxFaces[ 3 ].m_nEdge = 20;
		m_BoxFaces[ 4 ].m_nEdge = 19;
		m_BoxFaces[ 5 ].m_nEdge = 21;
		m_Faces.WriteDirect( 6, m_BoxFaces );

		// Bounds
		m_Bounds.m_vMinBounds = vMin;
		m_Bounds.m_vMaxBounds = vMax;
	}

	Vector m_BoxVertices[ 8 ];
	RnPlane_t m_BoxPlanes[ 6 ];
	RnHalfEdge_t m_BoxEdges[ 24 ];
	RnFace_t m_BoxFaces[ 6 ];
};


//--------------------------------------------------------------------------------------------------
// Mesh serialization
//--------------------------------------------------------------------------------------------------
#define RN_TYPE_SPLIT_X		0
#define RN_TYPE_SPLIT_Y		1
#define RN_TYPE_SPLIT_Z		2
#define RN_TYPE_LEAF		3

schema struct RnTriangle_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( RnTriangle_t );

	int32 m_nIndex[ 3 ];
};

// TODO: this wants to be ALIGN32, but it's currently stored in CUtlVector and CResourceArray, which do not support this.
schema struct ALIGN16 RnNode_t // node needs to not stride over cache line boundary and min/max vectors need to be aligned for easy SIMD loading
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( RnNode_t );	

	Vector m_vMin;															// The node AABB 
	uint32 m_nChildren;														// The 2nd child offset and the node type/split axis
	Vector m_vMax;															// The node AABB
	uint32 m_nTriangleOffset;												// If leaf node this is the offset into the associated triangle array
	
	// Traversal
	FORCEINLINE bool IsLeaf( void ) const									{ return GetType() == RN_TYPE_LEAF;	}
	FORCEINLINE RnNode_t* GetLeftChild( void ) 								{ return this + 1; }
	FORCEINLINE const RnNode_t* GetLeftChild( void ) const					{ return this + 1; }
	FORCEINLINE RnNode_t* GetRightChild( void ) 							{ return this + GetChildOffset(); }
	FORCEINLINE const RnNode_t* GetRightChild( void ) const					{ return this + GetChildOffset(); }
	FORCEINLINE uint GetAxis( void ) const									{ AssertDbg( !IsLeaf() ); return m_nChildren >> 30; }
	FORCEINLINE uint GetChildOffset( void) const							{ AssertDbg( !IsLeaf() ); return m_nChildren & 0x3FFFFFFF; }
	FORCEINLINE uint GetType( void ) const									{ return m_nChildren >> 30; }
	FORCEINLINE uint GetTriangleCount( void ) const							{ AssertDbg( IsLeaf() ); return m_nChildren & 0x3FFFFFFF; }
	FORCEINLINE uint GetTriangleOffset( void ) const						{ AssertDbg( IsLeaf() ); return m_nTriangleOffset; }
	FORCEINLINE void SetTriangleOffset( uint32 nTriangleOffset )			{ AssertDbg( IsLeaf( ) ); m_nTriangleOffset = nTriangleOffset; }

	// Construction
	void SetLeaf( uint nOffset, uint nCount )
	{
		m_nTriangleOffset = nOffset;
		m_nChildren = ( RN_TYPE_LEAF << 30 ) | nCount;
	}

	void SetNode( uint nAxis, uint nChildOffset )
	{
		AssertDbg( nAxis < 3 );
		m_nChildren = ( nAxis << 30 ) | nChildOffset;
	}

	// Statistics
	int GetHeight( void ) const;

} ALIGN16_POST;

schema struct RnMesh_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( RnMesh_t );

	Vector m_vMin;															// The mesh AABB 
	Vector m_vMax;															// The mesh AABB
	CResourceArray< RnNode_t > m_Nodes;										// The nodes of the loose kd-tree to accelerate ray casts and volume queries against this mesh.
	CResourceArray< Vector > m_Vertices;									// The mesh vertices in the space of the parent shape.
	CResourceArray< RnTriangle_t > m_Triangles;								// The mesh triangles with additional topology information similar to the half-edge data structure. 
	CResourceArray< uint8 > m_Materials;									// The per-triangle material indices for this mesh. Can be empty if all triangles share the same material.
	Vector m_vOrthographicAreas; // fraction 0..1 of coverage along YZ,ZX,XY sides of AABB
	
	FORCEINLINE RnNode_t* GetRoot( void ) 									{ return &m_Nodes[ 0 ]; }
	FORCEINLINE const RnNode_t* GetRoot( void ) const						{ return &m_Nodes[ 0 ]; }
	FORCEINLINE int GetVertexCount( void ) const							{ return m_Vertices.Count();		}
	FORCEINLINE const Vector &GetVertex( int nVertex ) const				{ return m_Vertices[ nVertex ];		}
	FORCEINLINE int GetTriangleCount( void ) const							{ return m_Triangles.Count();		}
	FORCEINLINE const RnTriangle_t* GetTriangle( int nTriangle ) const		{ return &m_Triangles[ nTriangle ];	}
	FORCEINLINE int GetMaterialCount( void ) const							{ return m_Materials.Count();		}
	FORCEINLINE uint8 GetMaterial( int nMaterial ) const					{ return m_Materials[ nMaterial ];  }									
	FORCEINLINE const Vector ComputeTriangleUnitNormal( const Vector &vScale, int nIndex )const;
	FORCEINLINE const Vector ComputeTriangleCentroid( int nIndex )const;
	FORCEINLINE const Vector ComputeTriangleIncenter( int nIndex )const; // the center of inscribed circle

	// Statistics
	int GetHeight( void ) const;
	int GetMemory( void ) const;	

	AABB_t GetBbox( void ) const;
	int GetTriangulation( Vector *pVerts = NULL ) const;
	void GetTriangulation( CUtlVector<Vector> &vertices, CUtlVector<uint32> &indices, const Vector &vScale = Vector( 1, 1, 1 ) ) const;
	float GetVolume( void ) const { return 0; }

	uintp GetRuntimeSize( void ) const ;

	void Validate( void ) const;

	template < typename Functor >
	void CastBox( const Functor &callback, const RnRay_t& localRay, const Vector& vExtent, float flMaxFraction ) const;
};


//--------------------------------------------------------------------------------------------------
// Shape serialization
//--------------------------------------------------------------------------------------------------
schema struct RnShapeDesc_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( RnShapeDesc_t );

	uint32 m_nCollisionAttributeIndex;
	uint32 m_nSurfacePropertyIndex;
};

schema struct RnSphereDesc_t : public RnShapeDesc_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( RnSphereDesc_t );

	RnSphere_t m_Sphere;
};

schema struct RnCapsuleDesc_t : public RnShapeDesc_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( RnCapsuleDesc_t );

	RnCapsule_t m_Capsule;
};

schema struct RnHullDesc_t : public RnShapeDesc_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( RnHullDesc_t );

	RnHull_t m_Hull;
};

schema struct RnMeshDesc_t : public RnShapeDesc_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( RnMeshDesc_t );

	RnMesh_t m_Mesh;
};


//--------------------------------------------------------------------------------------------------
template <typename Array>
uintp GetRuntimeSizeOf( const Array & arr )
{
	return sizeof( arr[0] ) * uintp( arr.Count() );
}	


//--------------------------------------------------------------------------------------------------
inline uintp RnHull_t::GetRuntimeSize( void ) const
{
	return sizeof( *this ) +
		GetRuntimeSizeOf( m_Vertices ) +
		GetRuntimeSizeOf( m_Planes ) +
		GetRuntimeSizeOf( m_Edges ) +
		GetRuntimeSizeOf( m_Faces );
}


//--------------------------------------------------------------------------------------------------
inline uintp RnMesh_t::GetRuntimeSize( void ) const 
{
	return ( ( sizeof( *this ) + 15 ) & ~15 ) +
		GetRuntimeSizeOf( m_Nodes ) +
		GetRuntimeSizeOf( m_Vertices ) +
		GetRuntimeSizeOf( m_Triangles ) +
		GetRuntimeSizeOf( m_Materials );
}


//--------------------------------------------------------------------------------------------------
inline void RnHull_t::Validate( void ) const
{
#ifdef DBGFLAG_ASSERT
	Assert( m_flMaxAngularRadius > 0 && m_flMaxAngularRadius < 1e5f && m_vCentroid.Length() < 1e4f && uint( m_Vertices.Count() ) < 256 && uint( m_Planes.Count() ) < 256 && uint( m_Edges.Count() ) < 256 && uint( m_Faces.Count() ) < 256 );
	for( int i = 0; i < m_Faces.Count(); ++i )
	{
		Assert( m_Faces[i].m_nEdge < ( uint )m_Edges.Count() );
	}
	for( int i = 0; i < m_Edges.Count(); ++i )
	{
		Assert( m_Edges[i].m_nNext < ( uint )m_Edges.Count() );
		Assert( m_Edges[i].m_nTwin < ( uint )m_Edges.Count() );
		Assert( m_Edges[i].m_nOrigin < ( uint )m_Vertices.Count() );
		Assert( m_Edges[i].m_nFace < ( uint )m_Faces.Count() );
	}
#endif
}



//--------------------------------------------------------------------------------------------------
inline void RnMesh_t::Validate( void ) const
{
#ifdef DBGFLAG_ASSERT
	Assert( m_vMin.Length() < 1e5f && m_vMax.Length() < 1e5f ); // check saneness
	//for( int i = 0; i < m_Nodes.Count(); ++i )
	{
		
	}
	for( int i = 0; i < m_Vertices.Count(); ++i )
	{
		Assert( m_Vertices[i].Length() < 1e5f );
	}
#endif
}


//--------------------------------------------------------------------------------------------------
// Joint serialization
//--------------------------------------------------------------------------------------------------
struct RnJointDesc_t
{
	// Bodies
	uint32 m_nBody1;
	uint32 m_nBody2;

	// Joint frames
	Vector m_vOrigin1;
	Quaternion m_qBasis1;
	Vector m_vOrigin2;
	Quaternion m_qBasis2;

	// Breakable
	float m_flMaxForce;
	float m_flMaxTorque;
};

struct RnSphericalDesc_t : public RnJointDesc_t
{
	// Angular motor (3D)
	Vector m_vTargetVelocity;
	float m_flMaxTorque;
};

struct RnUniversalDesc_t : public RnJointDesc_t
{
	// Limit
	float m_flConeAngle;
};

struct RnRevoluteDesc_t : public RnJointDesc_t
{
	// Limit
	float m_flMinAngle;
	float m_flMaxAngle;

	// Angular motor (1D)
	float m_flTargetVelocity;
	float m_flMaxTorque;
};

struct RnPrismaticDesc_t : public RnJointDesc_t
{
	// Limit
	float m_flMinDistance;
	float m_flMaxDistance;

	// Linear motor (1D)
	float m_flTargetVelocity;
	float m_flMaxForce;
};

struct RnRagdollDesc_t : public RnJointDesc_t
{
	// Conical limit with elliptical base
	float32 m_flRadiusY;
	float32 m_flRadiusZ;

	// Angular motor (3D)
	Vector m_vTargetVelocity;
	float32 m_flMaxTorque;
};

struct rnWeldDesc_t : public RnJointDesc_t
{
	// Spring parameters
	float m_flFrequency;
	float m_flDampingRatio;
};


struct rnPulleyDesc_t : public RnJointDesc_t
{

};

struct rnSpringDesc_t : public RnJointDesc_t
{
	// Spring parameters
	float32 m_flFrequency;
	float32 m_flDampingRatio;
};

schema struct RnSoftbodyParticle_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( RnSoftbodyParticle_t );
	float32 m_flMassInv;
};

schema struct RnSoftbodySpring_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( RnSoftbodySpring_t );
	uint16 m_nParticle[2];
	float32 m_flLength;
};

schema struct RnSoftbodyCapsule_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( RnSoftbodyCapsule_t );
	Vector m_vCenter[2];
	float32 m_flRadius;
	uint16 m_nParticle[2];
};


schema struct ALIGN16 RnBlendVertex_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( RnBlendVertex_t );
	uint16 m_nWeight0;
	uint16 m_nIndex0;
	uint16 m_nWeight1;
	uint16 m_nIndex1;
	uint16 m_nWeight2;
	uint16 m_nIndex2;
	uint16 m_nFlags;
	uint16 m_nTargetIndex;
};



inline Vector RnSphere_t::ComputeOrthographicAreas()const
{
	return Vector( M_PI/4, M_PI/4, M_PI/4 );
}


// compute the area of 2D capsule with the given 2 centers and the radius
inline float Compute2DCapsuleArea( float x0, float y0, float x1, float y1, float flRadius )
{
	// it's really simple: the 2D pill, if you don't think of it as 2 circles and a rectangle, but think of it as 2 half-circles and a rectangle, 
	// has no overlapping parts and is easy to compute the area of
	float flAxisLength  = sqrtf( Sqr( x0 - x1 ) + Sqr( y0 - y1 ) );
	return M_PI * flRadius * flRadius + flAxisLength * 2 * flRadius;
}

inline float Compute2DCapsuleOrthographicArea( float x0, float y0, float x1, float y1, float flRadius )
{
	float flBboxArea = ( fabsf( x0 - x1 ) + 2 * flRadius ) * ( fabsf( y0 - y1 ) + 2 * flRadius ); // the area of bounding rectangle
	return Compute2DCapsuleArea( x0, y0, x1, y1, flRadius ) / flBboxArea;
}


inline Vector RnCapsule_t::ComputeOrthographicAreas()const
{
	return Vector( 
		Compute2DCapsuleOrthographicArea( m_vCenter[0].x, m_vCenter[0].y, m_vCenter[1].x, m_vCenter[1].y, m_flRadius ),
		Compute2DCapsuleOrthographicArea( m_vCenter[0].y, m_vCenter[0].z, m_vCenter[1].y, m_vCenter[1].z, m_flRadius ),
		Compute2DCapsuleOrthographicArea( m_vCenter[0].z, m_vCenter[0].x, m_vCenter[1].z, m_vCenter[1].x, m_flRadius )
	);
}



inline bool IsTriangulationValid( int nVertexCount, const CUtlVector<uint32> &indices )
{
	if( nVertexCount == 0 )
	{
		return indices.IsEmpty(); // triangulation is valid iff both indices and vertices are empty
	}
	// check that indices are referring to all the vertices, and no more
	CVarBitVecT<uint32> used( nVertexCount );
	for( int i = 0; i < indices.Count(); ++i )
	{
		if( indices[i] < ( uint )nVertexCount )
		{
			used.Set( indices[i] );
		}
		else
		{
			return false;
		}
	}

	uint nUnusedVerts = 0;
	for( int i = 0; i < nVertexCount; ++i )
	{
		if( !used[ i ] )
		{
			nUnusedVerts++;
			Warning( "%d,", i );
		}
	}
	if ( nUnusedVerts )
	{
		Warning( " fully %u verts are unused\n", nUnusedVerts );
	}

	//bool bIsAllSet = used.IsAllSet();
	return true; // even if we don't use some verts, the triangulation is still valid; it's just not optimal
}


inline bool IsTriangulationValid( const CUtlVector<Vector> &vertices, const CUtlVector<uint32> &indices )
{
	return IsTriangulationValid( vertices.Count(), indices );
}

//--------------------------------------------------------------------------------------------------
// RnHull_t
//--------------------------------------------------------------------------------------------------
inline void RnHull_t::Transform( const matrix3x4_t& transform )
{
	m_vCentroid = VectorTransform( m_vCentroid, transform );

	for ( int nVertex = 0; nVertex < m_Vertices.Count(); ++nVertex )
	{
		m_Vertices[ nVertex ] = VectorTransform( m_Vertices[ nVertex ], transform );
	}

	Vector vOrigin = transform.GetOrigin();
	for ( int nPlane = 0; nPlane < m_Planes.Count(); ++nPlane )
	{
		Vector vNormal = VectorRotate( m_Planes[ nPlane ].m_vNormal, transform );
		float flOffset = m_Planes[ nPlane ].m_flOffset + DotProduct( vNormal, vOrigin );

		m_Planes[ nPlane ].m_vNormal = vNormal;
		m_Planes[ nPlane ].m_flOffset = flOffset;
	}
}


//--------------------------------------------------------------------------------------------------
// RnNode_t
//--------------------------------------------------------------------------------------------------
inline int RnNode_t::GetHeight( void ) const
{
	if ( IsLeaf() )
	{
		return 0;
	}

	const RnNode_t* pLeftChild = GetLeftChild();
	int LeftHeight = pLeftChild->GetHeight();
	const RnNode_t* pRightChild = GetRightChild();
	int RightHeight = pRightChild->GetHeight();

	return 1 + MAX( LeftHeight, RightHeight );
}


//--------------------------------------------------------------------------------------------------
// RnMesh_t
//--------------------------------------------------------------------------------------------------
inline int RnMesh_t::GetHeight( void ) const 
{
	const RnNode_t* Root = GetRoot();
	if ( Root == NULL )
	{
		return 0;
	}

	return Root->GetHeight();
}

//--------------------------------------------------------------------------------------------------
inline int RnHull_t::GetMemory( void ) const 
{
	int nMemory = 0;
	nMemory += sizeof( RnHull_t );
	nMemory += m_Vertices.Count() * sizeof( m_Vertices[0] );
	nMemory += m_Planes.Count() * sizeof( m_Planes[0] );
	nMemory += m_Edges.Count() * sizeof( m_Edges[0] );
	nMemory += m_Faces.Count() * sizeof( m_Faces[0] );
	return nMemory;
}

//--------------------------------------------------------------------------------------------------
inline int RnMesh_t::GetMemory( void ) const 
{
	int nMemory = 0;
	nMemory += sizeof( RnMesh_t );
	nMemory += m_Nodes.Count() * sizeof( RnNode_t );
	nMemory += m_Vertices.Count() * sizeof( Vector );
	nMemory += m_Triangles.Count() * sizeof( RnTriangle_t );
	nMemory += m_Materials.Count() * sizeof( uint8 );

	return nMemory;
}


//--------------------------------------------------------------------------------------------------
inline AABB_t RnSphere_t::GetBbox()const
{
	AABB_t b;
	b.m_vMinBounds.Init( m_vCenter.x - m_flRadius, m_vCenter.y - m_flRadius, m_vCenter.z - m_flRadius );
	b.m_vMaxBounds.Init( m_vCenter.x + m_flRadius, m_vCenter.y + m_flRadius, m_vCenter.z + m_flRadius );
	return b;
}


//--------------------------------------------------------------------------------------------------
inline AABB_t RnSphere_t::GetBbox( const CTransform& xform ) const
{
	Vector vCenter = TransformPoint( xform, m_vCenter );
	Vector vExtent( m_flRadius, m_flRadius, m_flRadius );

	Vector vMin = vCenter - vExtent; 
	Vector vMax = vCenter + vExtent;
	return AABB_t( vMin, vMax );
}


//--------------------------------------------------------------------------------------------------
inline int RnSphere_t::GetTriangulation( Vector *pVerts ) const
{
	if( pVerts )
	{
		float flSin[5] = {0,1,0,-1,0};
		float flCos[5] = {1,0,-1,0,1};
		Vector *pOut = pVerts;

		for( int i = 0; i < 4; ++i )
		{
			float s0 = flSin[i] * m_flRadius, c0 = flCos[i] * m_flRadius, s1 = flSin[i+1] * m_flRadius, c1 = flCos[i+1] * m_flRadius;

			*pOut++ = m_vCenter + Vector( -m_flRadius, 0, 0 );
			*pOut++ = m_vCenter + Vector( 0, c0, s0 );
			*pOut++ = m_vCenter + Vector( 0, c1, s1 );

			*pOut++ = m_vCenter + Vector( m_flRadius, 0, 0 );
			*pOut++ = m_vCenter + Vector( 0, c1, s1 );
			*pOut++ = m_vCenter + Vector( 0, c0, s0 );
		}
		Assert( 24 == pOut - pVerts );
	}
	return 24; // approximation with octahedron
}



//--------------------------------------------------------------------------------------------------
inline void RnSphere_t::GetTriangulation( CUtlVector<Vector> &vertices, CUtlVector<uint32> &indices )const
{
	GetTriangulation( vertices, indices, 12, 5 );
}


//--------------------------------------------------------------------------------------------------
inline void RnSphere_t::GetTriangulation( CUtlVector<Vector> &vertices, CUtlVector<uint32> &indices, int nSides, int nSlices )const
{
	int nIndexBase = indices.AddMultipleToTail( nSides * 2 * nSlices * 3 ); 
	uint32 *pOut = indices.Base() + nIndexBase;

	int nVertBase = vertices.AddMultipleToTail( nSides * nSlices + 2 );
	Vector *pVerts = vertices.Base() + nVertBase;
	V_memset( pVerts, 0, ( nSides * nSlices + 2 ) * sizeof( Vector ) );

	pVerts[ nSides * nSlices + 0 ] = Vector( -m_flRadius, 0, 0 );
	pVerts[ nSides * nSlices + 1 ] = Vector(  m_flRadius, 0, 0 );

	for( int i = 0; i < nSides; ++i )
	{
		float theta = ( -2 * M_PI * i ) / nSides;
		float s0 = m_flRadius * sinf( theta ), c0 = m_flRadius * cosf( theta );

		int i1 = ( i + 1 ) % nSides;

		// sides
		for( int j = 0; ; ++j )
		{
			float psi = ( j + 1 ) * M_PI / ( nSlices + 1 ) - M_PI / 2, x0 = sinf( psi ), y0 = cosf( psi );
			pVerts[ nSlices * i + j ] = Vector( x0 * m_flRadius, c0 * y0, s0 * y0 );

			if( j + 1 < nSlices )
			{
				*pOut++ = nVertBase + nSlices * i1 + j ;
				*pOut++ = nVertBase + nSlices * i  + j ;
				*pOut++ = nVertBase + nSlices * i1 + j + 1;

				*pOut++ = nVertBase + nSlices * i1 + j + 1;
				*pOut++ = nVertBase + nSlices * i  + j ;
				*pOut++ = nVertBase + nSlices * i  + j + 1;

				Assert( nVertBase + nSlices * i  + j + 1 < vertices.Count() );
			}
			else
			{
				break;
			}
		}

		// the last slice - only triangles

		// the end of caps
		if( 1 )
		{
			*pOut++ = nVertBase + nSlices * nSides + 0;
			*pOut++ = nVertBase + nSlices * i  ;
			*pOut++ = nVertBase + nSlices * i1 ;
		}

		if( 1 )
		{
			*pOut++ = nVertBase + nSlices * nSides + 1;
			*pOut++ = nVertBase + nSlices * i1 + nSlices - 1;
			*pOut++ = nVertBase + nSlices * i  + nSlices - 1;
		}
	}

	for( int i = 0; i < nSides * nSlices + 2; ++i )
	{
		pVerts[i] += m_vCenter;
	}

	//indices.SetCount( pOut - indices.Base() );

	Assert( pOut == indices.end() );
	AssertDbg( IsTriangulationValid( vertices, indices ) );
}


//--------------------------------------------------------------------------------------------------
inline AABB_t RnCapsule_t::GetBbox() const
{
	AABB_t b;
	b.m_vMinBounds.Init( Min( m_vCenter[0].x, m_vCenter[1].x ) - m_flRadius, Min( m_vCenter[0].y, m_vCenter[1].y ) - m_flRadius, Min( m_vCenter[0].z, m_vCenter[1].z ) - m_flRadius );
	b.m_vMaxBounds.Init( Max( m_vCenter[0].x, m_vCenter[1].x ) + m_flRadius, Max( m_vCenter[0].y, m_vCenter[1].y ) + m_flRadius, Max( m_vCenter[0].z, m_vCenter[1].z ) + m_flRadius );
	return b;
}


//--------------------------------------------------------------------------------------------------
inline AABB_t RnCapsule_t::GetBbox( const CTransform& xform ) const
{
	Vector vCenter1 = TransformPoint( xform, m_vCenter[ 0 ] );
	Vector vCenter2 = TransformPoint( xform, m_vCenter[ 1 ] );
	Vector vExtent( m_flRadius, m_flRadius, m_flRadius );

	AABB_t aabb1( vCenter1 - vExtent, vCenter1 + vExtent );
	AABB_t aabb2( vCenter2 - vExtent, vCenter2 + vExtent );

	return aabb1 + aabb2;
}


//--------------------------------------------------------------------------------------------------
inline int RnCapsule_t::GetTriangulation( Vector *pVerts ) const
{
	int nSides = 12, nSlices = 2;
	if( pVerts )
	{
		Vector vHeight = m_vCenter[1] - m_vCenter[0];
		float flHeight = vHeight.Length();
		Vector vAxisX;

		if( flHeight > 1e-5f )
		{
			vAxisX = vHeight / flHeight;
		}
		else
		{
			vAxisX = vHeight = Vector( 1,0,0 );
			flHeight = 1;
		}
		Vector vAxisY = VectorPerpendicularToVector( vAxisX ), vAxisZ = CrossProduct( vAxisX, vAxisY );

		Vector *pOut = pVerts;
		float s0 = 0, c0 = m_flRadius;
		for( int i = 0; i < nSides; ++i )
		{
			float theta = (2 * M_PI * ( i + 1 ) ) / nSides;
			float s1 = m_flRadius * sinf( theta ), c1 = m_flRadius * cosf( theta );

			// cylinder sides
			*pOut++ = Vector( 0, c1, s1 );
			*pOut++ = Vector( 0, c0, s0 );
			*pOut++ = Vector( flHeight, c1, s1 );

			*pOut++ = Vector( flHeight, c1, s1 );
			*pOut++ = Vector( 0, c0, s0 );
			*pOut++ = Vector( flHeight, c0, s0 );

			// caps - quads
			float x0 = 0, x1 = 0, y0 = 1, y1 = 0;
			for( int j = 0; j < nSlices; ++j )
			{
				float psi = ( ( j + 1 ) * M_PI / 2 ) / ( nSlices + 1 );
				x1 = sinf( psi );
				y1 = cosf( psi );
				// top cap
				*pOut++ = Vector( flHeight + x0 * m_flRadius, c1 * y0, s1 * y0 );
				*pOut++ = Vector( flHeight + x0 * m_flRadius, c0 * y0, s0 * y0 );
				*pOut++ = Vector( flHeight + x1 * m_flRadius, c1 * y1, s1 * y1 );

				*pOut++ = Vector( flHeight + x1 * m_flRadius, c1 * y1, s1 * y1 );
				*pOut++ = Vector( flHeight + x0 * m_flRadius, c0 * y0, s0 * y0 );
				*pOut++ = Vector( flHeight + x1 * m_flRadius, c0 * y1, s0 * y1 );

				// bottom cap
				*pOut++ = Vector( -x0 * m_flRadius, c1 * y0, s1 * y0 );
				*pOut++ = Vector( -x1 * m_flRadius, c1 * y1, s1 * y1 );
				*pOut++ = Vector( -x0 * m_flRadius, c0 * y0, s0 * y0 );

				*pOut++ = Vector( -x1 * m_flRadius, c1 * y1, s1 * y1 );
				*pOut++ = Vector( -x1 * m_flRadius, c0 * y1, s0 * y1 );
				*pOut++ = Vector( -x0 * m_flRadius, c0 * y0, s0 * y0 );

				x0 = x1;
				y0 = y1;
			}

			// the end of caps
			*pOut++ = Vector( -m_flRadius, 0, 0 );
			*pOut++ = Vector( -x1 * m_flRadius, c0 * y1, s0 * y1 );
			*pOut++ = Vector( -x1 * m_flRadius, c1 * y1, s1 * y1 );

			*pOut++ = Vector( flHeight + m_flRadius, 0, 0 );
			*pOut++ = Vector( flHeight + x1 * m_flRadius, c1 * y1, s1 * y1 );
			*pOut++ = Vector( flHeight + x1 * m_flRadius, c0 * y1, s0 * y1 );

			s0 = s1;
			c0 = c1;
		}

		for( Vector *p = pVerts; p < pOut; ++p )
		{
			*p = m_vCenter[0] + vAxisX * p->x + vAxisY * p->y + vAxisZ * p->z;
		}
		Assert( nSides * ( 2 + 2 * ( 2 * nSlices + 1 ) ) * 3 == pOut - pVerts );
	}

	return nSides * ( 2 + 2 * ( 2 * nSlices + 1 ) ) * 3; // each side is 4 tris, each 3 verts
}


//--------------------------------------------------------------------------------------------------
inline void RnCapsule_t::GetTriangulation( CUtlVector<Vector> &vertices, CUtlVector<uint32> &indices )const
{
	GetTriangulation( vertices, indices, 12, 2 );
}


//--------------------------------------------------------------------------------------------------
inline void RnCapsule_t::GetTriangulation( CUtlVector<Vector> &vertices, CUtlVector<uint32> &indices, int nSides, int nSlices )const
{
	int nVertBase = vertices.Count();
	int nIndexBase = indices.AddMultipleToTail( nSides * ( 2 + 2 * ( 2 * nSlices + 1 ) ) * 3 ); // each side is 4 tris, each 3 verts 
	uint32 *pOut = &indices[ nIndexBase ];

	int nSideVerts = 2 + 2 * nSlices;
	vertices.AddMultipleToTail( nSides * nSideVerts + 2 );
	Vector *pVerts = &vertices[nVertBase];
	V_memset( pVerts, 0, nSides * nSideVerts * sizeof( Vector ) );

	Vector vHeight = m_vCenter[1] - m_vCenter[0];
	float flHeight = vHeight.Length();
	Vector vAxisX;

	if( flHeight > 1e-5f )
	{
		vAxisX = vHeight / flHeight;
	}
	else
	{
		vAxisX = vHeight = Vector( 1,0,0 );
		flHeight = 1;
	}
	Vector vAxisY = VectorPerpendicularToVector( vAxisX ), vAxisZ = CrossProduct( vAxisX, vAxisY );

	pVerts[ nSides * nSideVerts + 0 ] = Vector( -m_flRadius, 0, 0 );
	pVerts[ nSides * nSideVerts + 1 ] = Vector( flHeight + m_flRadius, 0, 0 );

	for( int i = 0; i < nSides; ++i )
	{
		float theta = ( -2 * M_PI * i ) / nSides;
		float s0 = m_flRadius * sinf( theta ), c0 = m_flRadius * cosf( theta );

		int i1 = ( i + 1 ) % nSides;
		// cylinder sides
		*pOut++ = nVertBase + nSideVerts * i1;
		*pOut++ = nVertBase + nSideVerts * i;
		*pOut++ = nVertBase + nSideVerts * i1 + 1;

		*pOut++ = nVertBase + nSideVerts * i1 + 1;
		*pOut++ = nVertBase + nSideVerts * i;
		*pOut++ = nVertBase + nSideVerts * i  + 1;

		// caps - quads
		for( int j = 0; ; ++j )
		{
			int j1 = j + 1;
			float psi = ( j * M_PI / 2 ) / ( nSlices + 1 ), x0 = sinf( psi ), y0 = cosf( psi );
			pVerts[nSideVerts * i + j * 2 + 0 ] = Vector(          - x0 * m_flRadius, c0 * y0, s0 * y0 );
			pVerts[nSideVerts * i + j * 2 + 1 ] = Vector( flHeight + x0 * m_flRadius, c0 * y0, s0 * y0 );

			if( j < nSlices)
			{
				// top cap
				*pOut++ = nVertBase + nSideVerts * i1 + j  * 2 + 1;
				*pOut++ = nVertBase + nSideVerts * i  + j  * 2 + 1;
				*pOut++ = nVertBase + nSideVerts * i1 + j1 * 2 + 1;

				*pOut++ = nVertBase + nSideVerts * i1 + j1 * 2 + 1;
				*pOut++ = nVertBase + nSideVerts * i  + j  * 2 + 1;
				*pOut++ = nVertBase + nSideVerts * i  + j1 * 2 + 1;

				// bottom cap
				*pOut++ = nVertBase + nSideVerts * i1 + j  * 2 + 0;
				*pOut++ = nVertBase + nSideVerts * i1 + j1 * 2 + 0;
				*pOut++ = nVertBase + nSideVerts * i  + j  * 2 + 0;

				*pOut++ = nVertBase + nSideVerts * i1 + j1 * 2 + 0;
				*pOut++ = nVertBase + nSideVerts * i  + j1 * 2 + 0;
				*pOut++ = nVertBase + nSideVerts * i  + j  * 2 + 0;
			}
			else
			{
				// the last slice - only triangles

				// the end of caps
				*pOut++ = nVertBase + nSides * nSideVerts + 0;
				*pOut++ = nVertBase + nSideVerts * i  + j  * 2 + 0;
				*pOut++ = nVertBase + nSideVerts * i1 + j  * 2 + 0;

				*pOut++ = nVertBase + nSides * nSideVerts + 1;
				*pOut++ = nVertBase + nSideVerts * i1 + j  * 2 + 1;
				*pOut++ = nVertBase + nSideVerts * i  + j  * 2 + 1;
				break;
			}
		}
	}

	for( int i = 0; i < nSides * nSideVerts + 2; ++i )
	{
		pVerts[i] = m_vCenter[0] + vAxisX * pVerts[i].x + vAxisY * pVerts[i].y + vAxisZ * pVerts[i].z;
	}
	Assert( pOut == indices.end() );
	AssertDbg( IsTriangulationValid( vertices, indices ) );
}

//--------------------------------------------------------------------------------------------------
inline AABB_t RnHull_t::GetBbox() const
{
	// note: we should store the bbox in RnHull if it's a frequent operation to compute its bbox
	AABB_t b;
	b.MakeInvalid();
	
	for( int nVert = 0; nVert < m_Vertices.Count(); ++nVert )
	{
		b |= m_Vertices[nVert];
	}
	
	return b;
}


//--------------------------------------------------------------------------------------------------
inline AABB_t RnHull_t::GetBbox( const CTransform& xform ) const
{
	// note: if we store the bbox in RnHull we could just transform it (which potentially might grow it)
	AABB_t b;
	b.MakeInvalid();
	
	for ( int nVert = 0; nVert < m_Vertices.Count(); ++nVert )
	{
		b |= TransformPoint( xform, m_Vertices[ nVert ] );
	}

	return b;
}


//--------------------------------------------------------------------------------------------------
inline int RnHull_t::GetTriangulation( Vector *pVerts ) const
{
	int nVertsOut = 0;
	for( int nFace = 0; nFace < m_Faces.Count(); ++nFace )
	{
		uint nStartEdge = m_Faces[nFace].m_nEdge, nStartVert = m_Edges[nStartEdge].m_nOrigin;
		const Vector &vStartEdge = m_Vertices[ nStartVert ];
		for( uint nEdge = m_Edges[nStartEdge].m_nNext; nEdge != nStartEdge; )
		{
			uint nNextEdge = m_Edges[nEdge].m_nNext; // go to next edge
			if( nNextEdge == nStartEdge )
			{
				break; // if next edge is the starting edge, we're done
			}

			if( pVerts )
			{
				pVerts[ nVertsOut + 0 ] = vStartEdge;
				pVerts[ nVertsOut + 1 ] = m_Vertices[ m_Edges[nEdge].m_nOrigin ];
				pVerts[ nVertsOut + 2 ] = m_Vertices[ m_Edges[nNextEdge].m_nOrigin ];
			}
			nVertsOut += 3;

			nEdge = nNextEdge;
		}
	}

	return nVertsOut;
}




//------------------------------------------------------------------------------------------------------------------------------------------------------
inline void RnHull_t::GetTriangulation( CUtlVector<Vector> &vertices, CUtlVector<uint32> &indices, float flScale )const
{
	int nVertBase = vertices.Count();
	vertices.AddMultipleToTail( GetVertexCount() );
	for( int i = 0; i < GetVertexCount(); ++i )
	{
		vertices[ i + nVertBase ] = GetVertex( i ) * flScale;
	}

	for ( int i = 0; i < GetFaceCount(); ++i )
	{
		const RnFace_t* pFace = GetFace( i );

		const RnHalfEdge_t* pEdge1 = GetEdge( pFace->m_nEdge );
		const RnHalfEdge_t* pEdge2 = GetEdge( pEdge1->m_nNext );
		const RnHalfEdge_t* pEdge3 = GetEdge( pEdge2->m_nNext );
		AssertDbg( pEdge1 != pEdge3 );

		int v1 = nVertBase + pEdge1->m_nOrigin;

		do 
		{
			int v2 = nVertBase + pEdge2->m_nOrigin;
			int v3 = nVertBase + pEdge3->m_nOrigin;

			indices.AddToTail( v1 );
			indices.AddToTail( v2 );
			indices.AddToTail( v3 );

			pEdge2 = pEdge3;
			pEdge3 = GetEdge( pEdge3->m_nNext );
		} 
		while ( pEdge1 != pEdge3 );
	}

	AssertDbg( IsTriangulationValid( vertices, indices ) );
}



//--------------------------------------------------------------------------------------------------
inline AABB_t RnMesh_t::GetBbox()const
{
	AABB_t b;
	b.m_vMinBounds = m_vMin;
	b.m_vMaxBounds = m_vMax;
	return b;
}


//------------------------------------------------------------------------------------------------------------------------------------------------------
inline int RnMesh_t::GetTriangulation( Vector *pVerts ) const
{
	if( pVerts )
	{
		for( int nTriangleIndex = 0; nTriangleIndex < m_Triangles.Count(); ++nTriangleIndex )
		{
			const RnTriangle_t &tri = m_Triangles[nTriangleIndex];
			for( int nVertInTri = 0; nVertInTri < 3; ++nVertInTri )
			{
				pVerts[ nTriangleIndex * 3 + nVertInTri ] = m_Vertices[ tri.m_nIndex[ nVertInTri ] ];
			}
		}
	}
	return m_Triangles.Count() * 3;
}


//------------------------------------------------------------------------------------------------------------------------------------------------------
inline void RnMesh_t::GetTriangulation( CUtlVector<Vector> &vertices, CUtlVector<uint32> &indices, const Vector &vScale ) const
{
	int nVertBase = vertices.Count();
	vertices.AddMultipleToTail( GetVertexCount() );
	for ( int i = 0; i < GetVertexCount(); ++i )
	{
		Vector vertex = vScale * GetVertex( i );
		vertices[ nVertBase + i ] = vertex;
	}

	indices.EnsureCapacity( 3 * GetTriangleCount() );
	for ( int i = 0; i < GetTriangleCount(); ++i )
	{
		const RnTriangle_t* pTriangle = GetTriangle( i );
		indices.AddToTail( nVertBase + pTriangle->m_nIndex[ 0 ] );
		indices.AddToTail( nVertBase + pTriangle->m_nIndex[ 1 ] );
		indices.AddToTail( nVertBase + pTriangle->m_nIndex[ 2 ] );
	}

	AssertDbg( IsTriangulationValid( vertices, indices ) );
}


//------------------------------------------------------------------------------------------------------------------------------------------------------
inline float RnSphere_t::GetVolume()const
{
	return m_flRadius * m_flRadius * m_flRadius * M_PI * ( 4.0f / 3.0f ); // http://en.wikipedia.org/wiki/Sphere#Volume_of_a_sphere
}


//------------------------------------------------------------------------------------------------------------------------------------------------------
inline float RnCapsule_t::GetVolume()const
{
	return m_flRadius * m_flRadius * M_PI * ( m_flRadius * ( 4.0f / 3.0f ) + ( m_vCenter[0] - m_vCenter[1] ).Length() ); // http://en.wikipedia.org/wiki/Sphere#Volume_of_a_sphere
}



//------------------------------------------------------------------------------------------------------------------------------------------------------
inline float RnHull_t::GetVolume()const
{
	float flVolume = 0;
	for( int nFace = 0; nFace < m_Faces.Count(); ++nFace )
	{
		uint nStartEdge = m_Faces[nFace].m_nEdge, nStartVert = m_Edges[nStartEdge].m_nOrigin;
		const Vector &vStartEdge = m_Vertices[ nStartVert ];
		for( uint nEdge = m_Edges[nStartEdge].m_nNext; nEdge != nStartEdge; )
		{
			uint nNextEdge = m_Edges[nEdge].m_nNext; // go to next edge
			if( nNextEdge == nStartEdge )
			{
				break; // if next edge is the starting edge, we're done
			}

			Vector v1 = m_Vertices[ m_Edges[nEdge].m_nOrigin ];
			Vector v2 = m_Vertices[ m_Edges[nNextEdge].m_nOrigin ];
			float flCenterZHalf = ( vStartEdge.z + v1.z + v2.z ) * ( 1.0f / 6.0f ); // half-height of the prism, to account for cross product being double the area of base

			float flSignedDoubleBaseArea = CrossProductZ( v1 - vStartEdge, v2 - vStartEdge );
			flVolume += flCenterZHalf * flSignedDoubleBaseArea;

			nEdge = nNextEdge;
		}
	}
	Assert( flVolume > 0 );
	return flVolume;
}

FORCEINLINE const Vector RnMesh_t::ComputeTriangleUnitNormal( const Vector &vScale, int nTriIndex )const
{
	const RnTriangle_t* pTriangle = GetTriangle( nTriIndex );

	const Vector& vVertex1 = GetVertex( pTriangle->m_nIndex[ 0 ] );
	const Vector& vVertex2 = GetVertex( pTriangle->m_nIndex[ 1 ] );
	const Vector& vVertex3 = GetVertex( pTriangle->m_nIndex[ 2 ] );

	Vector vEdge1 = ScaleVector( vScale, vVertex2 - vVertex1 );
	Vector vEdge2 = ScaleVector( vScale, vVertex3 - vVertex1 );
	Vector vNormal = CrossProduct( vEdge1, vEdge2 );
	return vNormal / vNormal.Length(); // valid RnMesh should never have degenerate triangles
}


FORCEINLINE const Vector RnMesh_t::ComputeTriangleCentroid( int nTriIndex )const
{
	const RnTriangle_t* pTriangle = GetTriangle( nTriIndex );

	const Vector& vVertex1 = GetVertex( pTriangle->m_nIndex[ 0 ] );
	const Vector& vVertex2 = GetVertex( pTriangle->m_nIndex[ 1 ] );
	const Vector& vVertex3 = GetVertex( pTriangle->m_nIndex[ 2 ] );

	return ( vVertex1 + vVertex2 + vVertex3 ) * ( 1.0f / 3.0f );
}


FORCEINLINE const Vector RnMesh_t::ComputeTriangleIncenter( int nTriIndex )const
{
	const RnTriangle_t* pTriangle = GetTriangle( nTriIndex );

	const Vector& a = GetVertex( pTriangle->m_nIndex[ 0 ] );
	const Vector& b = GetVertex( pTriangle->m_nIndex[ 1 ] );
	const Vector& c = GetVertex( pTriangle->m_nIndex[ 2 ] );

	float la = ( b - c ).Length(), lb = ( a - c ).Length(), lc = ( b - a ).Length();
	return ( a * la + b * lb + c * lc ) / ( la + lb + lc );
}

FORCEINLINE const Vector RnHull_t::ComputeFaceCentroid( int nFace )const
{
	Vector vSum( 0,0,0 );
	uint8 nFirstEdge = m_Faces[nFace].m_nEdge, nEdge = nFirstEdge;
	float flNumEdges = 0;
	do 
	{
		AssertDbg( m_Edges[nEdge].m_nFace == uint( nFace ) );
		vSum += m_Vertices[ m_Edges[nEdge].m_nOrigin ];
		nEdge = m_Edges[ nEdge ].m_nNext;
		
		flNumEdges += 1.0f;

	} while ( nEdge != nFirstEdge );
	
	return vSum / flNumEdges;
}




//--------------------------------------------------------------------------------------------------
// SIMD clipping
//--------------------------------------------------------------------------------------------------
FORCEINLINE fltx4 HMaxSIMD( fltx4 a )
{
	fltx4 b = RotateLeft( a );
	fltx4 c = MaxSIMD( a, b );
	fltx4 d = RotateLeft2( c );
	return MaxSIMD( c, d );
}


//-------------------------------------------------------------------------------------------------
FORCEINLINE fltx4 HMinSIMD( fltx4 a )
{
	fltx4 b = RotateLeft( a );
	fltx4 c = MinSIMD( a, b );
	fltx4 d = RotateLeft2( c );
	return MinSIMD( c, d );
}


//-------------------------------------------------------------------------------------------------
FORCEINLINE int ClipRaySIMD( const RnRay_t& ray, const Vector& vMin, const Vector& vMax, float &flBestTime )
{
	fltx4 f4Min = LoadAlignedSIMD( &vMin.x ), f4Max = LoadAlignedSIMD( &vMax.x );
	fltx4 f4Origin = LoadAlignedSIMD( &ray.vOrigin ), f4DeltaInv = LoadAlignedSIMD( &ray.vDeltaInv );
	fltx4 t1 = ( f4Min - f4Origin ) * f4DeltaInv; // can be MADD
	fltx4 t2 = ( f4Max - f4Origin ) * f4DeltaInv; // can be MADD
	fltx4 f4TMin = MinSIMD( t1, t2 ), f4TMax = MaxSIMD( t1, t2 );
	fltx4 f4MinT = HMaxSIMD( SetWSIMD( f4TMin, Four_Zeros ) );
	fltx4 f4MaxT = HMinSIMD( SetWSIMD( f4TMax, ReplicateX4( flBestTime ) ) );

	// flMinT = max( 0, f4TMin.xyz )
	// flMaxT = min( 1, f4TMin.xyz )
	// return ( flMinT > flMaxT || flMinT > flBestTime )
	return _mm_comigt_ss( f4MinT, f4MaxT );
}


//-------------------------------------------------------------------------------------------------
FORCEINLINE int ClipRaySIMD( const RnRay_t& ray, const fltx4 &f4Min , const fltx4& f4Max, float &flBestTime )
{
	fltx4 f4Origin = LoadAlignedSIMD( &ray.vOrigin ), f4DeltaInv = LoadAlignedSIMD( &ray.vDeltaInv );
	fltx4 t1 = ( f4Min - f4Origin ) * f4DeltaInv; // can be MADD
	fltx4 t2 = ( f4Max - f4Origin ) * f4DeltaInv; // can be MADD
	fltx4 f4TMin = MinSIMD( t1, t2 ), f4TMax = MaxSIMD( t1, t2 );
	fltx4 f4MinT = HMaxSIMD( SetWSIMD( f4TMin, Four_Zeros ) );
	fltx4 f4MaxT = HMinSIMD( SetWSIMD( f4TMax, ReplicateX4( flBestTime ) ) );

	// flMinT = max( 0, f4TMin.xyz )
	// flMaxT = min( 1, f4TMin.xyz )
	// return ( flMinT > flMaxT || flMinT > flBestTime )
	return _mm_comigt_ss( f4MinT, f4MaxT );
}

//-------------------------------------------------------------------------------------------------
FORCEINLINE int ClipRaySIMD( const RnRay_t& ray, const fltx4& f4Min, const fltx4& f4Max )
{
	fltx4 f4Origin = LoadAlignedSIMD( &ray.vOrigin ), f4DeltaInv = LoadAlignedSIMD( &ray.vDeltaInv );
	fltx4 t1 = ( f4Min - f4Origin ) * f4DeltaInv; // can be MADD
	fltx4 t2 = ( f4Max - f4Origin ) * f4DeltaInv; // can be MADD
	fltx4 f4TMin = MinSIMD( t1, t2 ), f4TMax = MaxSIMD( t1, t2 );
	fltx4 f4MinT = HMaxSIMD( SetWSIMD( f4TMin, Four_Zeros ) );
	fltx4 f4MaxT = HMinSIMD( SetWSIMD( f4TMax, Four_Ones ) );

	// flMinT = max( 0, f4TMin.xyz )
	// flMaxT = min( 1, f4TMin.xyz )
	// return ( flMinT > flMaxT || flMinT > flBestTime )
	return _mm_comigt_ss( f4MinT, f4MaxT );
}


//-------------------------------------------------------------------------------------------------
template < typename Functor >
void RnMesh_t::CastBox( const Functor &callback, const RnRay_t &ray, const Vector& vLocalExtent, float flMaxFraction ) const
{
	int nCount = 0;
	const RnNode_t* stack[ STACK_SIZE ];
	stack[ nCount++ ] = GetRoot();

	fltx4 f4LocalExtent = LoadUnalignedSIMD( &vLocalExtent );

	while ( nCount > 0 )
	{
		const RnNode_t* pNode = stack[ --nCount ];

		if( ClipRaySIMD( ray, LoadAlignedSIMD( &pNode->m_vMin ) - f4LocalExtent, LoadAlignedSIMD( &pNode->m_vMax ) + f4LocalExtent ) )
		{
			continue;
		}
		if ( !pNode->IsLeaf() )
		{
			// Determine traversal order (front -> back)
			// The near and far child are determined by the direction of 
			// the ray with respect to the dimension of the split plane.
			const RnNode_t *pLeft = pNode->GetLeftChild(), *pRight = pNode->GetRightChild();
			if ( ray.vDelta[ pNode->GetAxis() ] > 0.0f )
			{
				stack[ nCount++ ] = pRight;
				stack[ nCount++ ] = pLeft;
			}
			else
			{
				stack[ nCount++ ] = pLeft;
				stack[ nCount++ ] = pRight;
			}
			AssertDbg( nCount < STACK_SIZE - 1 ); // there should always be space for at least 1 more entry in the stack
		}
		else
		{
			uint32 nTriangleCount = pNode->GetTriangleCount();
			uint32 nTriangleOffset = pNode->GetTriangleOffset();

			for ( uint32 nTriangle = 0; nTriangle < nTriangleCount; ++nTriangle )
			{
				// Get the triangle in world coordinates
				const RnTriangle_t* pTriangle = GetTriangle( nTriangleOffset + nTriangle );
				const Vector &vVertex1 = GetVertex( pTriangle->m_nIndex[ 0 ] );
				const Vector &vVertex2 = GetVertex( pTriangle->m_nIndex[ 1 ] );
				const Vector &vVertex3 = GetVertex( pTriangle->m_nIndex[ 2 ] );
				callback( vVertex1, vVertex2, vVertex3 );
			}
		}
	}
}


template <typename Array>
inline bool ArePlainArraysEqual( const Array &a, const Array &b )
{
	return a.Count( ) == b.Count( ) && 0 == V_memcmp( a.Base( ), b.Base( ), a.Count( ) * sizeof( a[ 0 ] ) );
}

template <typename Array>
inline void CRC64_ProcessArray( CRC64_t *pCrc, const Array &array )
{
	CRC64_ProcessBuffer( pCrc, array.Base( ), array.Count( ) * sizeof( array[ 0 ] ) );
}


class CRnEqualFunctor
{
public:
	bool operator() ( const RnHull_t *pLeft, const RnHull_t *pRight )const
	{
		return ArePlainArraysEqual( pLeft->m_Vertices, pRight->m_Vertices )
			&& ArePlainArraysEqual( pLeft->m_Planes, pRight->m_Planes )
			&& ArePlainArraysEqual( pLeft->m_Edges, pRight->m_Edges )
			&& ArePlainArraysEqual( pLeft->m_Faces, pRight->m_Faces );
	}

	bool operator() ( const RnMesh_t *pLeft, const RnMesh_t *pRight )const
	{
		return ArePlainArraysEqual( pLeft->m_Vertices, pRight->m_Vertices )
			&& ArePlainArraysEqual( pLeft->m_Nodes, pRight->m_Nodes )
			&& ArePlainArraysEqual( pLeft->m_Triangles, pRight->m_Triangles )
			&& ArePlainArraysEqual( pLeft->m_Materials, pRight->m_Materials );
	}
};

class CRnHashFunctor
{
public:
	uint operator()( const RnHull_t *pHull )const
	{
		CRC64_t nCrc;
		CRC64_Init( &nCrc );
		CRC64_ProcessArray( &nCrc, pHull->m_Vertices );
		CRC64_ProcessArray( &nCrc, pHull->m_Planes );
		CRC64_ProcessArray( &nCrc, pHull->m_Edges );
		CRC64_ProcessArray( &nCrc, pHull->m_Faces );
		CRC64_Final( &nCrc );
		return nCrc;
	}
	uint operator()( const RnMesh_t *pHull )const
	{
		CRC64_t nCrc;
		CRC64_Init( &nCrc );
		CRC64_ProcessArray( &nCrc, pHull->m_Vertices );
		CRC64_ProcessArray( &nCrc, pHull->m_Nodes );
		CRC64_ProcessArray( &nCrc, pHull->m_Triangles );
		CRC64_ProcessArray( &nCrc, pHull->m_Materials );
		CRC64_Final( &nCrc );
		return nCrc;
	}
};

#endif

