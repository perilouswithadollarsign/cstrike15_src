//========= Copyright © Valve Corporation, All rights reserved. ============//
#ifndef RUBIKON_FEM_HIERARCHY_HDR
#define RUBIKON_FEM_HIERARCHY_HDR


#include "ssemath.h"
#include "vector4d.h"
#include "rubikon/serialize.h"
#include "mathlib/ssequaternion.h"
#include "rubikon/param_types.h"
#include "tier1/utlvector.h"

template <typename TInt >
inline void RemapNode( TInt &refNode, const CUtlVector< int > &remap )
{
	Assert( refNode < TInt( remap.Count() )) ;
	if ( uint( refNode ) < uint( remap.Count() ) )
	{
		refNode = remap[ refNode ];
	}
}

template< class T >
class CUtlVectorAligned : public CUtlVector < T, CUtlMemoryAligned<T, VALIGNOF( T ) > >
{};

//////////////////////////////////////////////////////////////////////////
// Finite element description structures


enum FeFlagEnum_t
{
	FE_FLAG_UNINERTIAL_CONSTRAINTS = 1 << 4,
	FE_FLAG_ENABLE_FTL = 1 << 5,
	FE_FLAG_HAS_NODE_DAMPING = 1 << 8,
	FE_FLAG_HAS_ANIMATION_FORCE_ATTRACTION = 1 << 9,
	FE_FLAG_HAS_ANIMATION_VERTEX_ATTRACTION = 1 << 10,
	FE_FLAG_HAS_CUSTOM_GRAVITY = 1 << 11,
	FE_FLAG_HAS_STRETCH_VELOCITY_DAMPING = 1 << 12,

	FE_FLAG_ENABLE_WORLD_SHAPE_COLLISION_SHIFT = 16,
	FE_FLAG_ENABLE_WORLD_SHAPE_COLLISION_BASE = 1 << FE_FLAG_ENABLE_WORLD_SHAPE_COLLISION_SHIFT,
	FE_FLAG_ENABLE_WORLD_SPHERE_COLLISION = FE_FLAG_ENABLE_WORLD_SHAPE_COLLISION_BASE << SHAPE_SPHERE,
	FE_FLAG_ENABLE_WORLD_CAPSULE_COLLISION = FE_FLAG_ENABLE_WORLD_SHAPE_COLLISION_BASE << SHAPE_CAPSULE,
	FE_FLAG_ENABLE_WORLD_HULL_COLLISION = FE_FLAG_ENABLE_WORLD_SHAPE_COLLISION_BASE << SHAPE_HULL,
	FE_FLAG_ENABLE_WORLD_MESH_COLLISION = FE_FLAG_ENABLE_WORLD_SHAPE_COLLISION_BASE << SHAPE_MESH,
	FE_FLAG_ENABLE_WORLD_SHAPE_COLLISION_MASK = ( ( 1 << SHAPE_COUNT ) - 1 ) << FE_FLAG_ENABLE_WORLD_SHAPE_COLLISION_SHIFT
};


class CResourceString;
schema class FourVectors2D
{
public:
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS( FourVectors2D );
	FourVectors2D( ){}
	FourVectors2D( const fltx4 &_x, const fltx4 &_y ):
		x( _x ),
		y( _y )
	{
	}

	fltx4 Length( ) const
	{
		return SqrtSIMD( x * x + y * y );
	}

	const FourVectors2D operator - ( const FourVectors2D &right ) const
	{
		return FourVectors2D( x - right.x, y - right.y );
	}

public:
	fltx4 x;
	fltx4 y;
};



inline FourVectors2D operator * ( const fltx4 &f4Scalar, const FourVectors2D & v )
{
	FourVectors2D res;
	res.x = v.x * f4Scalar;
	res.y = v.y * f4Scalar;
	return res;
}






schema struct FeEdgeDesc_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( FeEdgeDesc_t );
	// the adjacent quads go : { nEdge[0], nEdge[1], nSide[0][0], nSide[0][1]} and { nEdge[0], nEdge[1], nSide[1][0], nSide[1][1] };
	uint16 nEdge[ 2 ];
	uint16 nSide[ 2 ][ 2 ];
	uint16 nElem[ 2 ];
};


schema struct OldFeEdge_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( OldFeEdge_t );
	// Indexing is following conventions in Bergou "Discrete Quadratic Curvature Energies" and "A Quadratic Bending Model for Inextensible Surfaces"
	float m_flK[ 3 ]; // c01+c04, c01+c02, -c01-c03. THe last element, -c02-c04 can be computed and negative sum of these three
	float32 invA;
	float32 t;
	float32 flThetaRelaxed;
	float32 flThetaFactor;
	float32 c01;
	float32 c02;
	float32 c03;
	float32 c04;
	//float flKelagerH0;
	float flAxialModelDist; // distance between edges 01 and 23
	float flAxialModelWeights[ 4 ]; // if dp is the error of axial model , these are the weights of deltas to apply to each vertex; infinite masses are factored in, so these may not sum up to 0, but should sum up to 0 when all particle masses are equal
	uint16 m_nNode[ 4 ];
};


schema struct FeKelagerBend_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( FeKelagerBend_t );
	float flHeight0; // relaxed distance from tip (v) to centroid (c)
	uint16 m_nNode[ 3 ]; // v, b0, b1: tip and two base ends
	uint16 m_nFlags; // 3 lower bits are the inverse masses of the nodes
};



// the triangle has vertices (0,0), (x1,0), (x2,y2) and weights 1-w1-w2, w1, w2
schema struct FeTri_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( FeTri_t );
	uint16 nNode[ 3 ];
	float32 w1;
	float32 w2; // these weights double as mass for rotation weighting and for computing center of mass
	float32 v1x;	  // vertex 1 is at ( v1x, 0 ) by constructino
	Vector2D v2;
};

schema struct FeSimdTri_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( FeSimdTri_t );
	uint nNode[ 3 ][ 4 ];
	fltx4 w1;
	fltx4 w2;
	fltx4 v1x;
	FourVectors2D v2;
	void Init( const FeTri_t *pTris );
};

schema struct FeQuad_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( FeQuad_t );
	// element has 3-4 nodes in case of thin film or cloth (tris or quads), and 4 nodes in case of softbody (tetrahedrons);
	// there are other discretizations possible, but 4 nodes per element align nincely
	// the last 2 nodes are the same (duplication is taken care of in their weights)
	uint16 nNode[ 4 ];

	float flSlack; // now much the node can deform plastically, either along local Z axis or in a small sphere around relaxed position (depends on the solver)

	// relaxed pose, with center of mass at identity (w is node weight), weights sum up to 1.
	// It'd be convenient if inertia tensor aligned with x,y,z and its eigenvalues sorted in descending order (Ixx >= Iyy >= Izz), BUT
	// for ease of coding I'_ assuming convention: axis X = edge 0-1, axis Y = edge 0-2 orthogonalized with edge 0-1
	// also, center of mass is at vertex[0] if there's any static node, also for the ease of coding
	// The weights are for summing up weighted covariance matrix when solving Wahba problem
	// If 1 or 2 nodes are static, this is implicit and weights do not reflect this (i.e. they are NOT 1,0,0,0 or .5,.5,0,0 )
	// because for Quads with static nodes we need to run different shape fitting codes

	Vector4D vShape[ 4 ];
};

schema struct FeNodeBase_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( FeNodeBase_t );
	uint16 nNode; // target node to compute the basis
	uint16 nDummy[ 3 ];
	uint16 nNodeX0; //
	uint16 nNodeX1; // x = nodeX1 - nodeX0
	uint16 nNodeY0;
	uint16 nNodeY1; // y = nodeY1 - nodeY0, then orthogonalized
	Quaternion qAdjust;

	void RebaseNodes( int nBaseNode )
	{
		nNode += nBaseNode;
		nNodeX0 += nBaseNode;
		nNodeX1 += nBaseNode;
		nNodeY0 += nBaseNode;
		nNodeY1 += nBaseNode;
	}
	void RemapNodes( const CUtlVector< int > &remap )
	{
		RemapNode( nNode, remap );
		RemapNode( nNodeX0, remap );
		RemapNode( nNodeX1, remap );
		RemapNode( nNodeY0, remap );
		RemapNode( nNodeY1, remap );
	}

};

// after computing node bases (orientations), take node
schema struct FeNodeReverseOffset_t
{
	uint16 nBoneCtrl;   // the node that has orientation, but doesn't have position
	uint16 nTargetNode; // the node whose position is known in the frame of nNodeBone
	Vector vOffset;     // the position of nNodeTarget in the frame of nNodeBone
};

schema struct FeSimdQuad_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( FeSimdQuad_t );
	uint16 nNode[ 4 ][ 4 ];
	fltx4 f4Slack;
	FourVectors vShape[ 4 ];
	fltx4 f4Weights[ 4 ]; // careful: weights are transposed
	void Init( const FeQuad_t *pScalar );
};

// axial edge bend model: trying to maintain the distance between two points, one lying on the edge (with coordinate te in 0..1 interval) , another on the "virtual edge" (with coordinate tv in 0..1 interval)
// The edge has 2 adjacent quads. The virtual edge connects centers of the opposite edges (that become points in case of triangles). I.e.:
//
// 2----1----4
// |    ^    |
// +<---+----+  <- this is the virtual edge
// |    |    |
// 3----0----5
//
//      ^
//      \
//       --- this is the edge
// 
// Sometimes node[2]== node[3], and/or node[4]==node[5], all the math works the same except the 4-5 and/or 2-3 edge is degenerate and the corresponding quads become triangles.
// The signed distance is maintained by crossing edge and virtual edge and making sure the projection of the vector beween the points on the edge and virtual edge is positive. If not, we must flip the angle of edge bend.
//
schema struct FeAxialEdgeBend_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( FeAxialEdgeBend_t );
	float32 te; // barycentric coordinates of connection points on the main edge  n0-n1
	float32 tv; // barycentric coordinates of connection points on the virtual edge (n2+n3)/2 - (n4+n5)/2
	// Note: connection points form a line orthogonal (in rest state) to edge 0-1 and 2-3
	float32 flDist; // max distance between the connection points in rest configuration
	float32 flWeight[ 4 ]; // given a delta vector, multiply it by each weight and add to the node. It's guaranteed to conserve momentum (taking masses of particles into account). Note: w[4] = { node 0, node 1, nodes 2 and 3, nodes 4 and 5 }
	uint16 nNode[ 6 ];
};


// double-quad bend limit
schema struct FeBandBendLimit_t 
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( FeBandBendLimit_t );
	float32 flDistMin;
	float32 flDistMax;
	uint16 nNode[ 6 ]; // 0-1 are the central edge; 2-3 and 4-5 are the opposite sides of the quads (or tris if 2==3 and/or 4==5)
};


// a very simple distance limit
schema struct FeRodConstraint_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( FeRodConstraint_t );
	uint16 nNode[ 2 ];
	float32 flMaxDist;
	float32 flMinDist; // max/min distance
	float32 flWeight0; // relative weight of node[0], node[1] weight is 1-flWeight0
	float32 flRelaxationFactor; // the same as m_flStretchiness in Source1 cloth

	void InvariantReverse( )
	{
		Swap( nNode[ 0 ], nNode[ 1 ] );
		this->flWeight0 = 1.0f - this->flWeight0;
	}
};

schema struct FeSimdRodConstraint_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( FeSimdRodConstraint_t );
	uint16 nNode[ 2 ][ 4 ];
	fltx4 f4MaxDist;
	fltx4 f4MinDist; // max/min distance
	fltx4 f4Weight0; // relative weight of node[0], node[1] weight is 1-flWeight0
	fltx4 f4RelaxationFactor;
	void Init( const FeRodConstraint_t *pScalar );
};


schema struct FeSimdNodeBase_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( FeSimdNodeBase_t );
	uint16 nNode[ 4 ]; // target node to compute the basis
	uint16 nNodeX0[ 4 ]; //
	uint16 nNodeX1[ 4 ]; // x = nodeX1 - nodeX0
	uint16 nNodeY0[ 4 ] ;
	uint16 nNodeY1[ 4 ]; // y = nodeY1 - nodeY0, then orthogonalized
	uint16 nDummy[ 4 ]; // necessary for alignment, to avoid collecting garbage in the resources
	FourQuaternions qAdjust;
	void Init( const FeNodeBase_t *pScalar );
};

schema struct FeNodeIntegrator_t
{
	TYPEMETA( MNoScatter )
	DECLARE_SCHEMA_DATA_CLASS( FeNodeIntegrator_t );
	float flPointDamping; // FixedPointDamping from Dota - only non-0 on static nodes; generic damping on dynamic nodes for future extensibility, but normally 0 on dynamic nodes; multiplied by dt
	float flAnimationForceAttraction;
	float flAnimationVertexAttraction;
	float flGravity;

	void Init( )
	{
		flPointDamping = 0; // FixedPointDamping from Dota - only non-0 on static nodes; generic damping on dynamic nodes for future extensibility, but normally 0 on dynamic nodes; multiplied by dt
		flAnimationForceAttraction = 0 ;
		flAnimationVertexAttraction = 0;
		flGravity = 360.0f;
	}
};

schema struct FeSpringIntegrator_t
{
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS( FeSpringIntegrator_t );
	uint16 nNode[ 2 ];
	float32 flSpringRestLength;
	float32 flSpringConstant;
	float32 flSpringDamping; 
	float32 flNodeWeight0; // relative weight of node[0], node[1] weight is 1-flWeight0
};


/// 
// Per-node effects that change velocity (and not position directly) are parametrized here 
schema struct FeSimdSpringIntegrator_t
{
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS( FeSimdSpringIntegrator_t );
	uint16 nNode[ 2 ][ 4 ];
	fltx4 flSpringRestLength;
	fltx4 flSpringConstant;
	fltx4 flSpringDamping;
	fltx4 flNodeWeight0; // relative weight of node[0], node[1] weight is 1-flWeight0

	void Init( const FeSpringIntegrator_t *pScalar );
};



//////////////////////////////////////////////////////////////////////////
// Some Ctrl points do not correspond to any bone, but to other control points
// vOffset is the child Ctrl in the coordinate system of the parent ctrl
//
schema struct FeCtrlOffset_t
{
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS( FeCtrlOffset_t );
	uint16 nCtrlParent;
	uint16 nCtrlChild;
	Vector vOffset;
};


schema struct ALIGN4 FeCtrlOsOffset_t // the rope offset from S1, a horrible hack in S1 I need to replicate in the new cloth so that some ropes behave the way they do in S1
{
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS( FeCtrlOsOffset_t );
	uint16 nCtrlParent;
	uint16 nCtrlChild;
} ALIGN4_POST;

//////////////////////////////////////////////////////////////////////////
// Define how much the child Ctrl follows the parent Ctrl movements.
// Experimental, to try to remove lots of dynamic swing from jerky animations:
// if children of an animated node blindly derive the parent's animation,
// there will be less random swing
schema struct FeFollowNode_t
{
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS( FeFollowNode_t );
	uint16 nParentNode;
	uint16 nChildNode;
	float flWeight;
};


schema struct FeCollisionSphere_t
{
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS( FeCollisionSphere_t );
	uint16 nCtrlParent;
	uint16 nChildNode;
	float m_flRFactor; // has different meaning in different contexts
	Vector m_vOrigin;
	float flStickiness;
};

schema struct FeCollisionPlane_t
{
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS( FeCollisionPlane_t );
	uint16 nCtrlParent;
	uint16 nChildNode;
	RnPlane_t m_Plane;
	float flStickiness;
};



schema struct FeWorldCollisionParams_t
{
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS( FeWorldCollisionParams_t );
	float flWorldFriction; // this is the WorldFriction from source1. It's not exactly friction, it's just a multiplier
	float flGroundFriction;
	uint16 nListBegin; // begin in the WorldCollisionNodes array
	uint16 nListEnd;   // end in the WorldCollisionNodes array
};


schema struct FeTreeChildren_t
{
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS( FeTreeChildren_t );
	uint16 nChild[ 2 ]; // 0 = the first dynamic node; N = the first cluster
};


schema struct FeTaperedCapsuleRigid_t
{
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS( FeTaperedCapsuleRigid_t );

	uint16 nNode;
	uint16 nCollisionMask;
	Vector vCenter[ 2 ];
	float flRadius[ 2 ];
	float flStickiness;

	FeTaperedCapsuleRigid_t()
	: nNode( 0 )
	, nCollisionMask( 0 )
	{
		vCenter[ 0 ] = vec3_origin;
		vCenter[ 1 ] = vec3_origin;
		flRadius[ 0 ] = 0.0f;
		flRadius[ 1 ] = 0.0f;
		flStickiness = 0.0f;
	}

	bool UsesNode( uint nOtherNode )const
	{
		return nNode == nOtherNode;
	}
	void ChangeNode( uint nOldNode, uint nNewNode )
	{
		if ( nNode == nOldNode )
			nNode = nNewNode;
	}
	void RebaseNodes( int nBaseNode )
	{
		nNode += nBaseNode;
	}
	void RemapNodes( const CUtlVector< int > &remap )
	{
		RemapNode( nNode, remap );
	}
	bool IsValid( uint nNodeCount )const
	{
		return nNode < nNodeCount;
	}
};

schema struct FeSphereRigid_t
{
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS( FeSphereRigid_t );

	FeSphereRigid_t()
	{
		nNode = 0;
		nCollisionMask = 0;
		vCenter = vec3_origin;
		flRadius = 1;
		flStickiness = 0;
	}

	uint16 nNode;
	uint16 nCollisionMask;
	Vector vCenter;
	float flRadius;
	float flStickiness;

	bool UsesNode( uint nOtherNode )const
	{
		return nNode == nOtherNode;
	}
	void ChangeNode( uint nOldNode, uint nNewNode )
	{
		if ( nNode == nOldNode )
			nNode = nNewNode;
	}
	void RebaseNodes( int nBaseNode )
	{
		nNode += nBaseNode;
	}
	void RemapNodes( const CUtlVector< int > &remap )
	{
		RemapNode( nNode, remap );
	}
	bool IsValid( uint nNodeCount )const
	{
		return nNode < nNodeCount;
	}
};

schema struct FeTaperedCapsuleStretch_t
{
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS( FeTaperedCapsuleStretch_t );
	uint16 nNode[ 2 ];
	uint16 nCollisionMask;
	uint16 nDummy; META( MPropertySuppressField );
	float flRadius[ 2 ];
	float flStickiness;

	bool operator ==( const FeTaperedCapsuleStretch_t &right )const
	{
		return nNode[ 0 ] == right.nNode[ 0 ] && nNode[ 1 ] == right.nNode[ 1 ] && flRadius[ 0 ] == right.flRadius[ 0 ] && flRadius[ 1 ] == right.flRadius[ 1 ];
	}
	bool UsesNode( uint nOtherNode )const
	{
		return nNode[ 0 ] == nOtherNode || nNode[ 1 ] == nOtherNode;
	}
	void ChangeNode( uint nOldNode, uint nNewNode )
	{
		if ( nNode[ 0 ] == nOldNode )
			nNode[ 0 ] = nNewNode;
		if ( nNode[ 1 ] == nOldNode )
			nNode[ 1 ] = nNewNode;
	}
	void RebaseNodes( int nBaseNode )
	{
		nNode[ 0 ] += nBaseNode;
		nNode[ 1 ] += nBaseNode;
	}
	void RemapNodes( const CUtlVector< int > &remap )
	{
		RemapNode( nNode[ 0 ], remap );
		RemapNode( nNode[ 1 ], remap );
	}
	bool IsValid( uint nNodeCount )const
	{
		return nNode[ 0 ] < nNodeCount && nNode[ 1 ] < nNodeCount;
	}
};



schema class CovMatrix3
{
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS( CovMatrix3 );
public:
	void InitForWahba( float m, const Vector &x );
	void Reset();

	void AddCov( const Vector &d ); // d is supposedly a vector relatively to the mean of the set; i.e. we assume here that we're actually summing up voth d and -d 
	void AddCov( const Vector &d, float m );
	CovMatrix3 GetPseudoInverse();

	// the element of the sum on the left side of the approximate solution of Wahba's problem (see wahba.nb for details)
	// thi sis essentially Sum[Mi Xi * w * Xi], Mi = weights, "*" means cross product, Xi is a deformed polygon vertex relative to center of mass, 
	// 21 flops, with madd
	void AddForWahba( float m, const Vector &x );
	void NormalizeEigenvalues();
	void RegularizeEigenvalues();
	Vector operator * ( const Vector &d );

public:
	Vector m_vDiag;
	float m_flXY;
	float m_flXZ;
	float m_flYZ;
};


schema class FourCovMatrices3
{
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS( FourCovMatrices3 );
public:
	void InitForWahba( const fltx4 &m, const FourVectors &x );

	// the element of the sum on the left side of the approximate solution of Wahba's problem (see wahba.nb for details)
	// thi sis essentially Sum[Mi Xi * w * Xi], Mi = weights, "*" means cross product, Xi is a deformed polygon vertex relative to center of mass, 
	// 21 flops, with madd
	void AddForWahba( const fltx4 &m, const FourVectors &x );
	FourVectors operator * ( const FourVectors &d );

public:
	FourVectors m_vDiag;
	fltx4 m_flXY;
	fltx4 m_flXZ;
	fltx4 m_flYZ;
};


schema struct FeFitWeight_t
{
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS( FeFitWeight_t );
	float flWeight;
	uint16 nNode;
	uint16 nDummy;
};

schema struct FeFitInfluence_t // helper struct
{
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS( FeFitInfluence_t );
	uint nVertexNode;
	float flWeight;
	uint nMatrixNode;

	void RebaseNodes( int nBaseNode )
	{
		nVertexNode += nBaseNode;
		nMatrixNode += nBaseNode;
	}
	void RemapNodes( const CUtlVector< int > &remap )
	{
		RemapNode( nVertexNode, remap );
		RemapNode( nMatrixNode, remap );
	}
};


schema struct FeFitMatrix_t
{
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS( FeFitMatrix_t );
	CTransform bone; // the bone transform relative to the computed Apq*Aqq^-1
/*
	CovMatrix3 AqqInv; // see MeshlessDeformations_SIG05.pdf for explanation
	float flStretchMin;
	float flStretchMax;
*/
	Vector vCenter; // center of mass in the init pose; may be different from the init pose of the ctrl
	uint16 nEnd; // subarray end index in the index and weight arrays 
	uint16 nNode; // the node to backsolve
	uint16 nCtrl; // the ctrl this backsolve will solve for
	uint16 nBeginDynamic; // the subarray begin of influences of dynamic nodes, so that we can feed the fit transforms back into simulation
	//uint16 nEndDominant; // the same as nEnd when there are no static nodes; otherwise, the end of subarray of static nodes - used for computing the center of mass of the cluster of nodes, where static nodes must dominate
	//uint16 nBegin; // not really needed, here for padding only
};

schema struct FeSimdFitMatrices_t
{
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS( FeSimdFitMatrices_t );
	FourVectors vCenter; // center of mass in the init pose; may be different from the init pose of the ctrl
	uint16 nEnd[4]; // subarray end index in the index and weight arrays 
	uint16 nCtrl[4]; // the ctrl this backsolve will solve for
	FourCovMatrices3 AqqInv; // see MeshlessDeformations_SIG05.pdf for explanation
	fltx4 flStretchMin;
	fltx4 flStretchMax;
};


class FeAabb_t
{
public:
	FeAabb_t(){}
	explicit FeAabb_t( const AABB_t &aabb )
	{
		m_vMinBounds = LoadUnaligned3SIMD( &aabb.m_vMinBounds );
		m_vMaxBounds = LoadUnaligned3SIMD( &aabb.m_vMaxBounds );
	}

	fltx4 m_vMinBounds;
	fltx4 m_vMaxBounds;

	const Vector &GetMinAsVector()const { return *( const Vector* )( char* )&m_vMinBounds; }
	const Vector &GetMaxAsVector()const { return *( const Vector* )( char* )&m_vMaxBounds; }
	float GetCost( float flExpand )const
	{
		fltx4 f4Size = m_vMaxBounds - m_vMinBounds + ReplicateX4( flExpand );
		Vector vSize = *( const Vector* )( char* )&f4Size;
		return vSize.x * vSize.y * vSize.z;
	}
	fltx4 GetDistVector( fltx4 p )const
	{
		return MaxSIMD( Four_Zeros, MaxSIMD( m_vMinBounds - p, p - m_vMaxBounds ) );
	}

	void operator |= ( const FeAabb_t &other )
	{
		m_vMinBounds = MinSIMD( m_vMinBounds, other.m_vMinBounds );
		m_vMaxBounds = MaxSIMD( m_vMaxBounds, other.m_vMaxBounds );
	}
	void operator |= ( const fltx4 &other )
	{
		m_vMinBounds = MinSIMD( m_vMinBounds, other );
		m_vMaxBounds = MaxSIMD( m_vMaxBounds, other );
	}
	void AddCenterAndExtents( const fltx4 &f4Center, const fltx4 &f4Extents )
	{
		m_vMinBounds = MinSIMD( m_vMinBounds, f4Center - f4Extents );
		m_vMaxBounds = MaxSIMD( m_vMaxBounds, f4Center + f4Extents );
	}
	void AddExtents( const fltx4 &f4Extents )
	{
		m_vMinBounds = m_vMinBounds - f4Extents;
		m_vMaxBounds = m_vMaxBounds + f4Extents;
	}
	int HasSeparatingAxis( const FeAabb_t &other )const
	{
		fltx4 f4Separation = OrSIMD( CmpLtSIMD( m_vMaxBounds, other.m_vMinBounds ), CmpLtSIMD( other.m_vMaxBounds, m_vMinBounds ) );
		int nSeparation = TestSignSIMD( f4Separation );
		return nSeparation & 7;
	}
	bool IsValid()const
	{
		return IsAllGreaterThanOrEq( SetWToZeroSIMD( m_vMaxBounds ), SetWToZeroSIMD( m_vMinBounds ) );
	}
};



class ALIGN16 CFeModel
{
public:

	CFeModel( )
	{
		V_memset( this, 0, sizeof( *this ) );
	}

	// make a shallow copy of the other FeModel; the FeModel class is NOT for editing Finite Element Models,
	// it's only for storing pointers to immutable resource data, so this copy constructor should not become deep-copy. 
	CFeModel( const CFeModel &other )
	{
		V_memcpy( this, &other, sizeof( *this ) );
	}

	static float SafeRcp( float x )
	{
		if ( fabsf( x ) < 1e-37f )
		{
			return FLT_MAX;
		}
		else
		{
			return 1.0f / x;
		}
	}

	uint NodeToCtrl( uint nNode ) const { return m_pNodeToCtrl ? m_pNodeToCtrl[ nNode ] : nNode; }
	uint CtrlToNode( uint nCtrl ) const { return m_pCtrlToNode ? m_pCtrlToNode[ nCtrl ] : nCtrl; }

	const char *GetNodeName( uint nNode )const { return GetCtrlName( NodeToCtrl( nNode ) ); }
	const char *GetCtrlName( uint nCtrl )const { return m_pCtrlName ? m_pCtrlName[ nCtrl ] : NULL;  }

	FeCtrlOffset_t *FindCtrlOffsetByChild( uint nChild );
	FeCtrlOsOffset_t *FindCtrlOsOffsetByChild( uint nChild );

	uint GetInclusiveCollisionSphereCount( ) const { return m_nCollisionSpheres[ 1 ]; }
	uint GetExclusiveCollisionSphereCount( ) const { return m_nCollisionSpheres[ 0 ]  - m_nCollisionSpheres[ 1 ]; }

	int GetComplexity( )const;

	uint GetDynamicNodeCount() const { return m_nNodeCount - m_nStaticNodes; }
	uint GetTreeRootAabbIndex() const { return m_nNodeCount - m_nStaticNodes - 2;  } // we have dynNodes - 1 tree nodes. The very last one is the root

	uint ComputeCollisionTreeDepthTopDown() const;
	uint ComputeCollisionTreeDepthTopDown( CUtlVector< uint16 > &levels ) const;
	uint ComputeCollisionTreeDepthTopDown( uint16 *pLevels ) const;
	uint ComputeCollisionTreeHeightBottomUp() const;
	uint ComputeCollisionTreeHeightBottomUp( CUtlVector< uint16 > &levels ) const;
	uint ComputeCollisionTreeHeightBottomUp( uint16 *pLevels ) const;
	uint ComputeCollisionTreeNodeCount() const { CUtlVector< uint16 > counts;  return ComputeCollisionTreeNodeCount( counts ); }
	uint ComputeCollisionTreeNodeCount( CUtlVector< uint16 > &counts ) const { counts.SetCount( GetDynamicNodeCount() - 1 ); return ComputeCollisionTreeNodeCount( counts.Base() ); }
	uint ComputeCollisionTreeNodeCount( uint16 *pCounts ) const;

	void ComputeCollisionTreeBounds( const fltx4 *pDynPos, FeAabb_t *pClusters )const;
	inline void ComputeCollisionTreeBounds( const VectorAligned *pDynPos, FeAabb_t *pClusters ) const { ComputeCollisionTreeBounds( ( const fltx4* )pDynPos, pClusters ); }
	float ComputeCollisionTreeBoundsError( const fltx4 *pDynPos, const FeAabb_t *pClusters )const;
	inline float ComputeCollisionTreeBoundsError( const VectorAligned *pDynPos, FeAabb_t *pClusters ) const { return ComputeCollisionTreeBoundsError( ( const fltx4* )pDynPos, pClusters ); }

	template < typename Functor, bool bIgnoreMask = false >
	void CastBox( uint16 nCollisionMask, const FeAabb_t &box, const FeAabb_t *pClusters, const Functor &fn )const
	{
		uint nStackDepth = m_nTreeDepth + 2;
		uint16 *pStack = StackAlloc( uint16, nStackDepth );
		uint nStackCount = 0;
		uint nDynCount = GetDynamicNodeCount(), nLastCluster = nDynCount - 2;
		for ( ;; )
		{
			FeTreeChildren_t tc = m_pTreeChildren[ nLastCluster ];
			AssertDbg( m_pTreeParents[ tc.nChild[ 0 ] ] == nLastCluster + nDynCount && m_pTreeParents[ tc.nChild[ 1 ] ] == nLastCluster + nDynCount );
			if ( ( bIgnoreMask || ( m_pTreeCollisionMasks[ nLastCluster + nDynCount ] & nCollisionMask ) ) && !pClusters[ nLastCluster ].HasSeparatingAxis( box ) )
			{
				// descent further down the tree
				if ( tc.nChild[ 0 ] < nDynCount )
				{
					if ( bIgnoreMask || ( m_pTreeCollisionMasks[ tc.nChild[ 0 ] ] & nCollisionMask ) )
					{
						fn( tc.nChild[ 0 ] );
					}
				}
				else
				{
					Assert( nStackCount < nStackDepth );
					pStack[ nStackCount++ ] = tc.nChild[ 0 ] - nDynCount;
				}

				if ( tc.nChild[ 1 ] < nDynCount )
				{
					if ( bIgnoreMask || ( m_pTreeCollisionMasks[ tc.nChild[ 1 ] ] & nCollisionMask ) )
					{
						fn( tc.nChild[ 1 ] );
					}
				}
				else
				{
					Assert( nStackCount < nStackDepth );
					pStack[ nStackCount++ ] = tc.nChild[ 1 ] - nDynCount;
				}
			}
			if ( nStackCount == 0 )
				break;
			nLastCluster = pStack[ --nStackCount ];
		};
	}

public:
	void RelaxRods( VectorAligned *pPos, float flStiffness = 1.0f, float flModelScale = 1.0f ) const;
	void RelaxRods2( VectorAligned *pPos, float flStiffness = 1.0f, float flModelScale = 1.0f ) const;
	void RelaxRods2Ftl( VectorAligned *pPos, float flStiffness = 1.0f, float flModelScale = 1.0f ) const;
	void RelaxRodsUninertial( VectorAligned *pPos1, VectorAligned *pPos0, float flStiffness = 1.0f, float flModelScale = 1.0f ) const;
	void RelaxSimdRods( fltx4 *pPos, float flStiffness = 1.0f, float flModelScale = 1.0f ) const;
	void RelaxSimdRodsFtl( fltx4 *pPos, float flStiffness = 1.0f, float flModelScale = 1.0f ) const;
	float RelaxQuads( VectorAligned *pPos, float flStiffness = 1.0f, float flModelScale = 1.0f, int nExperimental = 0 ) const;
	float RelaxTris( VectorAligned *pPos, float flStiffness = 1.0f, float flModelScale = 1.0f ) const;
	fltx4 RelaxSimdTris( VectorAligned *pPos, float flStiffness = 1.0f, float flModelScale = 1.0f )const;
	fltx4 RelaxSimdQuads( VectorAligned *pPos, float flStiffness = 1.0f, float flModelScale = 1.0f, int nExperimental = 0 ) const;

	void RelaxBend( VectorAligned *pPos, float flStiffness = 1.0f ) const;
	void RelaxBendSIMD( VectorAligned *pPos, float flStiffness = 1.0f ) const;
	void RelaxBendRigid( VectorAligned *pPos, float flStiffness = 1.0f ) const;
	void RelaxBendRigidSIMD( VectorAligned *pPos, float flStiffness = 1.0f ) const;

	float RelaxQuad2( float flStiffness, float flModelScale, const FeQuad_t &quad, const VectorAligned &p0, const VectorAligned &p1, VectorAligned &p2, VectorAligned &p3 )const;
	float RelaxQuad1( float flStiffness, float flModelScale, const FeQuad_t &quad, const VectorAligned &p0, VectorAligned &p1, VectorAligned &p2, VectorAligned &p3 )const;
	float RelaxQuad0( float flStiffness, float flModelScale, const FeQuad_t &quad, VectorAligned &p0, VectorAligned &p1, VectorAligned &p2, VectorAligned &p3 )const;
	float RelaxQuad0flat( float flStiffness, float flModelScale, const FeQuad_t &quad, VectorAligned &p0, VectorAligned &p1, VectorAligned &p2, VectorAligned &p3 )const;

	float RelaxTri2( float flStiffness, float flModelScale, const FeTri_t &tri, const VectorAligned &p0, const VectorAligned &p1, VectorAligned &p2 )const;
	float RelaxTri1( float flStiffness, float flModelScale, const FeTri_t &tri, const VectorAligned &p0, VectorAligned &p1, VectorAligned &p2 )const;
	float RelaxTri0( float flStiffness, float flModelScale, const FeTri_t &tri, VectorAligned &p0, VectorAligned &p1, VectorAligned &p2 )const;

	fltx4 RelaxSimdTri2( const fltx4& flStiffness, const fltx4 &fl4ModelScale, const FeSimdTri_t &tri, fltx4 *pPos )const;
	fltx4 RelaxSimdTri1( const fltx4& flStiffness, const fltx4 &fl4ModelScale, const FeSimdTri_t &tri, fltx4 *pPos )const;
	fltx4 RelaxSimdTri0( const fltx4& flStiffness, const fltx4 &fl4ModelScale, const FeSimdTri_t &tri, fltx4 *pPos )const;


	fltx4 RelaxSimdQuad2( const fltx4& flStiffness, const fltx4 &fl4ModelScale, const FeSimdQuad_t &quad, fltx4 *pPos )const;
	fltx4 RelaxSimdQuad1( const fltx4& flStiffness, const fltx4 &fl4ModelScale, const FeSimdQuad_t &quad, fltx4 *pPos )const;
	fltx4 RelaxSimdQuad0( const fltx4& flStiffness, const fltx4 &fl4ModelScale, const FeSimdQuad_t &quad, fltx4 *pPos )const;
	fltx4 RelaxSimdQuad0flat( const fltx4& flStiffness, const fltx4 &fl4ModelScale, const FeSimdQuad_t &quad, fltx4 *pPos )const;

	void FitCenters( fltx4 *pPos )const;
	void FitTransforms( const VectorAligned *pPos, matrix3x4a_t *pOut )const;
	void FitTransform( matrix3x4a_t *pOut, const FeFitMatrix_t &fm, const VectorAligned *pPos, const FeFitWeight_t *pWeights, const FeFitWeight_t *pWeightsEnd )const;
	void FitTransform2D( matrix3x4a_t *pOut, const FeFitMatrix_t &fm, const Vector &vAxis, const VectorAligned *pPos, const FeFitWeight_t *pWeights, const FeFitWeight_t *pWeightsEnd )const;

	void FeedbackFitTransforms( VectorAligned *pPos, float flStiffness )const;
	void FeedbackFitTransform( const matrix3x4a_t &tm, const FeFitMatrix_t &fm, VectorAligned *pPos, const FeFitWeight_t *pWeights, const FeFitWeight_t *pWeightsEnd, float flStiffness )const;

	void IntegrateSprings( VectorAligned *pPos0, VectorAligned *pPos1, float flTimeStep/*flHalfSqrDt*/, float flModelScale )const; // for integrating accelerations into positions, pass dt*dt/2 for subsequent verlet integration; for integrating accelerations into velocities or velocities into positions, pass dt;

	float ComputeElasticEnergyQuads( const VectorAligned *pPos, float flModelScale )const;
	float ComputeElasticEnergyRods( const VectorAligned *pPos, float flModelScale )const;
	float ComputeElasticEnergySprings( const VectorAligned *pPos0, const VectorAligned *pPos1, float flTimeStep/*flHalfSqrDt*/, float flModelScale )const;

	// flVelDrag is the drag from the classic lift equation: drag proportional to A v^2 
	// expected Identity values: flExpDrag= 0, flVelDrag = 0
	void ApplyAirDrag( VectorAligned *pPos0, const VectorAligned *pPos1, float flExpDrag, float flVelDrag );
	void ApplyQuadAirDrag( fltx4*pPos0, const fltx4*pPos1, float flExpDrag, float flVelDrag );
	void ApplyRodAirDrag( fltx4 *pPos0, const fltx4 *pPos1, float flExpDrag, float flVelDrag );
	void ApplyQuadWind( VectorAligned*pPos1, const Vector &vWind, float flAirDrag );

	void SmoothQuadVelocityField( fltx4 *pPos0, const fltx4 *pPos1, float flBlendFactor );
	void SmoothRodVelocityField( fltx4 *pPos0, const fltx4 *pPos1, float flBlendFactor );
public:
	uint32 m_nStaticNodeFlags;
	uint32 m_nDynamicNodeFlags;

	float32 m_flLocalForce; // 0.0 means fully local frame force; 1.0 means fully global force, i.e. only teleport when moving really far in one frame, otherwise capes lag behind. Legacy definition.
	float32 m_flLocalRotation; // 0.0 means fully global frame rotation; 1.0 means cloth rotates rigidly (not inertially) together with rotating entity. Legacy definition.

	uint16 m_nNodeCount; // total nodes
	uint16 m_nCtrlCount; // number of control bones or vertices outside the softbody (some may be skipped)
	uint16 m_nStaticNodes; // the first N nodes are static, the rest are moveable
	uint16 m_nRotLockStaticNodes; // of the first N static nodes, some or all have their rotation locked. The rest have only position locked.
	uint16 m_nQuadCount[ 3 ]; // [i] has total elements with AT LEAST i static nodes
	uint16 m_nTriCount[ 3 ]; // [i] has total elements with AT LEAST i static nodes
	uint16 m_nSimdQuadCount[ 3 ];
	uint16 m_nSimdTriCount[ 3 ];
	uint16 m_nSimdSpringIntegratorCount;
	uint16 m_nSimdRodCount;
	uint16 m_nSimdNodeBaseCount;
	uint16 m_nRodCount; // edges are used for bending energy computation / un-bend integration
	uint16 m_nAxialEdgeCount;
	uint16 m_nRopeCount;
	uint16 m_nRopeIndexCount;
	uint16 m_nNodeBaseCount;
	uint16 m_nSpringIntegratorCount;
	uint16 m_nCtrlOffsets;
	uint16 m_nCtrlOsOffsets;
	uint16 m_nFollowNodeCount;
	uint16 m_nCollisionSpheres[ 2 ]; // [0] is total spheres, [1] is Inclusive spheres only (the rest are exclusive)
	uint16 m_nCollisionPlanes;
	uint16 m_nWorldCollisionParamCount;
	uint16 m_nWorldCollisionNodeCount;
	uint16 m_nTaperedCapsuleStretchCount;
	uint16 m_nTaperedCapsuleRigidCount;
	uint16 m_nSphereRigidCount;
	uint16 m_nTreeDepth;
	uint16 m_nQuadVelocitySmoothIterations;
	uint16 m_nRodVelocitySmoothIterations;
	uint16 m_nFreeNodeCount;
	uint16 m_nFitMatrixCount[ 3 ];
	uint16 m_nSimdFitMatrixCount[ 3 ];
	uint32 m_nFitWeightCount;
	uint32 m_nReverseOffsetCount;

	float32 m_flDefaultThreadStretch;
	float32 m_flDefaultSurfaceStretch;
	float32 m_flDefaultGravityScale;
	float32 m_flDefaultVelAirDrag;
	float32 m_flDefaultExpAirDrag;
	float32 m_flDefaultVelQuadAirDrag;
	float32 m_flDefaultExpQuadAirDrag;
	float32 m_flDefaultVelRodAirDrag;
	float32 m_flDefaultExpRodAirDrag;
	float32 m_flQuadVelocitySmoothRate;
	float32 m_flRodVelocitySmoothRate;
	float32 m_flAddWorldCollisionRadius;
	float32 m_flDefaultVolumetricSolveAmount;

	float32 m_flWindage;
	float32 m_flWindDrag;


	FeSimdQuad_t *m_pSimdQuads SERIALIZE_ARRAY_SIZE( m_nSimdQuadCount[ 0 ] );
	FeQuad_t *m_pQuads SERIALIZE_ARRAY_SIZE( m_nQuadCount[0] );
	FeSimdTri_t *m_pSimdTris SERIALIZE_ARRAY_SIZE( m_nSimdTriCount[ 0 ] );
	FeTri_t *m_pTris SERIALIZE_ARRAY_SIZE( m_nTriCount[ 0 ] );
	FeRodConstraint_t *m_pRods SERIALIZE_ARRAY_SIZE( m_nRodCount );
	FeSimdRodConstraint_t *m_pSimdRods SERIALIZE_ARRAY_SIZE( m_nSimdRodCount );
	FeAxialEdgeBend_t *m_pAxialEdges SERIALIZE_ARRAY_SIZE( m_nAxialEdgeCount );
	uint16 *m_pNodeToCtrl SERIALIZE_ARRAY_SIZE( m_nNodeCount );
	uint16 *m_pCtrlToNode SERIALIZE_ARRAY_SIZE( m_nCtrlCount );
	uint16 *m_pRopes SERIALIZE_ARRAY_SIZE( m_nRopeIndexCount ); // the first m_nRopeCount elements are the End indices of each rope's subarray (indices are all starting at m_pRopes[0]). Each subarray starts with a static (or otherwise untouchable) element and runs through dynamic elements.
	uint32 *m_pCtrlHash SERIALIZE_ARRAY_SIZE( m_nCtrlCount );
	FeNodeBase_t *m_pNodeBases SERIALIZE_ARRAY_SIZE( m_nNodeBaseCount );
	FeSimdNodeBase_t *m_pSimdNodeBases SERIALIZE_ARRAY_SIZE( m_nSimdNodeBaseCount );
	CTransform *m_pInitPose SERIALIZE_ARRAY_SIZE( m_nCtrlCount );
	FeCtrlOffset_t *m_pCtrlOffsets SERIALIZE_ARRAY_SIZE( m_nCtrlOffsets );
	FeCtrlOsOffset_t *m_pCtrlOsOffsets SERIALIZE_ARRAY_SIZE( m_nCtrlOsOffsets );
	FeFollowNode_t *m_pFollowNodes SERIALIZE_ARRAY_SIZE( m_nFollowNodeCount );
	FeCollisionSphere_t *m_pCollisionSpheres SERIALIZE_ARRAY_SIZE( m_nCollisionSpheres[ 0 ] );
	FeCollisionPlane_t *m_pCollisionPlanes SERIALIZE_ARRAY_SIZE( m_nCollisionPlanes );
	FeWorldCollisionParams_t *m_pWorldCollisionParams SERIALIZE_ARRAY_SIZE( m_nWorldCollisionParamCount ); 
	FeTaperedCapsuleStretch_t *m_pTaperedCapsuleStretches SERIALIZE_ARRAY_SIZE( m_nTaperedCapsuleStretchCount );
	FeTaperedCapsuleRigid_t *m_pTaperedCapsuleRigids SERIALIZE_ARRAY_SIZE( m_nTaperedCapsuleRigidCount );
	FeSphereRigid_t *m_pSphereRigids SERIALIZE_ARRAY_SIZE( m_nSphereRigidCount );
	float *m_pLegacyStretchForce SERIALIZE_ARRAY_SIZE( m_nNodeCount );
	float *m_pNodeCollisionRadii SERIALIZE_ARRAY_SIZE( GetDynamicNodeCount() ); // spans all dynamic nodes ( m_nNodeCount - m_nStaticNodes  )
	float *m_pLocalRotation SERIALIZE_ARRAY_SIZE( GetDynamicNodeCount() );
	float *m_pLocalForce SERIALIZE_ARRAY_SIZE( GetDynamicNodeCount() );
	FeTreeChildren_t *m_pTreeChildren SERIALIZE_ARRAY_SIZE( GetDynamicNodeCount() - 1 );// spans all clusters (m_nNodeCount - m_nStaticNodes - 1 )
	uint16 *m_pTreeParents SERIALIZE_ARRAY_SIZE( GetDynamicNodeCount() * 2 - 1 );// spans from the first dynamic node (m_nStaticNodes) to the first cluster (m_nNodeCount - m_nStaticNodes) and then all clusters (m_nNodeCount - m_nStaticNodes - 1 )
	uint16 *m_pTreeCollisionMasks SERIALIZE_ARRAY_SIZE( GetDynamicNodeCount() * 2 - 1 ); // spans from the first dynamic node (m_nStaticNodes) to the first cluster (m_nNodeCount - m_nStaticNodes) and then all clusters (m_nNodeCount - m_nStaticNodes - 1 )
	uint16 *m_pWorldCollisionNodes SERIALIZE_ARRAY_SIZE( m_nWorldCollisionNodeCount ); // list of nodes that collide with the world
	uint16 *m_pFreeNodes SERIALIZE_ARRAY_SIZE( m_nFreeNodeCount );
	FeFitWeight_t *m_pFitWeights SERIALIZE_ARRAY_SIZE( m_nFitWeightCount );
	FeNodeReverseOffset_t *m_pReverseOffsets SERIALIZE_ARRAY_SIZE( m_nReverseOffsetCount );

	// either 0 elements (implying 0 damping for all nodes), or m_nNodeCount elements;
	// static nodes are damped as they come in from animation,
	// dynamic nodes as they simulate; 
	// damping is multiplied by the time step (in Dota, they are not)
	FeNodeIntegrator_t *m_pNodeIntegrator SERIALIZE_ARRAY_SIZE( m_nNodeCount );

	// this is to simulate spring forces (acceleration level) with the verlet integrator: it gets applied as a separte step, just adding a*t^2 to the corresponding nodes
	// if nodes have different damping, it needs to be figured out in the weight here. If damping is not 1.0, it needs to be premultiplied in both the constant and damping
	FeSpringIntegrator_t *m_pSpringIntegrator SERIALIZE_ARRAY_SIZE( m_nSpringIntegratorCount );
	FeSimdSpringIntegrator_t *m_pSimdSpringIntegrator SERIALIZE_ARRAY_SIZE( m_nSimdSpringIntegratorCount );

	FeFitMatrix_t *m_pFitMatrices SERIALIZE_ARRAY_SIZE( m_nFitMatrixCount[0] );
	FeSimdFitMatrices_t *m_pSimdFitMatrices SERIALIZE_ARRAY_SIZE( m_nSimdFitMatrixCount[ 0 ] );

	float *m_pNodeInvMasses SERIALIZE_ARRAY_SIZE( m_nNodeCount );
	const char **m_pCtrlName SERIALIZE_ARRAY_SIZE( m_nCtrlCount );

	AUTO_SERIALIZE;
} ALIGN16_POST;


inline bool EqualControls( const CFeModel *p, const CFeModel *q )
{
	if ( !p || !q || p->m_nCtrlCount != q->m_nCtrlCount )
	{
		return false;
	}

	for ( uint i = p->m_nCtrlCount; i-- > 0; )
	{
		if ( p->m_pCtrlHash[ i ] != q->m_pCtrlHash[ i ] )
		{
			return false;
		}
	}
	return true;
}






inline void FeSimdRodConstraint_t::Init( const FeRodConstraint_t *pScalar )
{
	for ( int i = 0; i < 4; ++i )
	{
		this->nNode[ 0 ][ i ] = pScalar[ i ].nNode[ 0 ];
		this->nNode[ 1 ][ i ] = pScalar[ i ].nNode[ 1 ];
		SubFloat( this->f4MinDist, i ) = pScalar[ i ].flMinDist;
		SubFloat( this->f4MaxDist, i ) = pScalar[ i ].flMaxDist;
		SubFloat( this->f4Weight0, i ) = pScalar[ i ].flWeight0;
		SubFloat( this->f4RelaxationFactor, i ) = pScalar[ i ].flRelaxationFactor;
	}
}


inline void FeSimdQuad_t::Init( const FeQuad_t *pScalar )
{
	for ( int i = 0; i < 4; ++i )
	{
		for ( int j = 0; j < 4; ++j )
		{
			this->nNode[ j ][ i ] = pScalar[ i ].nNode[ j ];
			SubFloat( this->vShape[ j ].x, i ) = pScalar[ i ].vShape[ j ].x;
			SubFloat( this->vShape[ j ].y, i ) = pScalar[ i ].vShape[ j ].y;
			SubFloat( this->vShape[ j ].z, i ) = pScalar[ i ].vShape[ j ].z;
			SubFloat( this->f4Weights[ j ], i ) = pScalar[ i ].vShape[ j ].w; // careful: weights are transposed
		}
		SubFloat( this->f4Slack, i ) = pScalar[ i ].flSlack;
	}
}


inline void FeSimdTri_t::Init( const FeTri_t *pTris )
{
	for ( int i = 0; i < 4; ++i )
	{
		this->nNode[ 0 ][ i ] = pTris[ i ].nNode[ 0 ];
		this->nNode[ 1 ][ i ] = pTris[ i ].nNode[ 1 ];
		this->nNode[ 2 ][ i ] = pTris[ i ].nNode[ 2 ];
		SubFloat( this->w1, i ) = pTris[ i ].w1;
		SubFloat( this->w2, i ) = pTris[ i ].w2;
		SubFloat( this->v1x, i ) = pTris[ i ].v1x;
		SubFloat( this->v2.x, i ) = pTris[ i ].v2.x;
		SubFloat( this->v2.y, i ) = pTris[ i ].v2.y;
	}
}


inline void FeSimdNodeBase_t::Init( const FeNodeBase_t *pBases )
{
	for ( int i = 0; i < 4; ++i )
	{
		this->nNode[ i ] = pBases[ i ].nNode;
		this->nNodeX0[ i ] = pBases[ i ].nNodeX0;
		this->nNodeX1[ i ] = pBases[ i ].nNodeX1;
		this->nNodeY0[ i ] = pBases[ i ].nNodeY0;
		this->nNodeY1[ i ] = pBases[ i ].nNodeY1;
		this->nDummy[ i ] = 0;
		SubFloat( this->qAdjust.x, i ) = pBases[ i ].qAdjust.x;
		SubFloat( this->qAdjust.y, i ) = pBases[ i ].qAdjust.y;
		SubFloat( this->qAdjust.z, i ) = pBases[ i ].qAdjust.z;
		SubFloat( this->qAdjust.w, i ) = pBases[ i ].qAdjust.w;
	}
}



inline void FeSimdSpringIntegrator_t::Init( const FeSpringIntegrator_t *pSprings )
{
	for ( int i = 0; i < 4; ++i )
	{
		this->nNode[ 0 ][ i ] = pSprings[ i ].nNode[ 0 ];
		this->nNode[ 1 ][ i ] = pSprings[ i ].nNode[ 1 ];
		SubFloat( this->flSpringRestLength , i ) = pSprings[ i ].flSpringRestLength ;
		SubFloat( this->flSpringConstant   , i ) = pSprings[ i ].flSpringConstant   ;
		SubFloat( this->flSpringDamping	   , i ) = pSprings[ i ].flSpringDamping	 ;
		SubFloat( this->flNodeWeight0	   , i ) = pSprings[ i ].flNodeWeight0	     ;
	}
}





// 7 flops, with madd
class CSinCosRotation
{
public:
	explicit CSinCosRotation( const Vector &vSinOmega )
	{
		// ~10flops
		m_vSinOmega = vSinOmega;
// 		m_flCosOmega = 1.0f / sqrtf( 1 + vSinOmega.LengthSqr( ) ); 
// 		m_vSinOmega *= m_flCosOmega;
	}

	const Vector operator *( const Vector &d )
	{
		return d /** m_flCosOmega*/ + CrossProduct( m_vSinOmega, d ); // ~6flops if really using madd
	}

	//float GetCosine( )const { return m_flCosOmega; }
protected:
	Vector m_vSinOmega;
//	float m_flCosOmega;
};


class CSinCosRotation2D
{
public:
	CSinCosRotation2D( float flWeightedCos, float flWeightedSin )
	{
		// ~5flops without extra checks, ~8flops with
		float flWeight = sqrtf( flWeightedCos * flWeightedCos + flWeightedSin * flWeightedSin );
		if ( flWeight > FLT_EPSILON )
		{
			float flWeightInv = 1.0f / flWeight;
			m_flSin = flWeightedSin * flWeightInv;
			m_flCos = flWeightedCos * flWeightInv;
		}
		else
		{
			m_flSin = 0.0f;
			m_flCos = 1.0f;
		}
	}

	float GetX( const Vector2D &d )const
	{
		return d.x * m_flCos - d.y * m_flSin;
	}
	
	float GetY( const Vector2D &d )const
	{
		return d.y * m_flCos + d.x * m_flSin;
	}

	const Vector2D operator *( const Vector2D &d )const 
	{
		return Vector2D( d.x * m_flCos - d.y * m_flSin, d.y * m_flCos + d.x * m_flSin ); // ~4flops if really using madd
	}

	// call this, and rotation will return a "delta", not rotated vector
// 	void Differentiate( ) { m_flCos -= 1.0f; } 
// 	const Vector2D RotateX0( float dx )
// 	{
// 		return Vector2D( dx * m_flCos, dx * m_flSin );
// 	}

	float GetCosine( )const { return m_flCos; }
	float GetSine( )const { return m_flSin;  }
public:
	float m_flSin;
	float m_flCos;
};

inline float Diff( const CSinCosRotation2D &left, const CSinCosRotation2D &right )
{
	return fabsf( left.m_flSin - right.m_flSin ) + fabsf( left.m_flCos - right.m_flCos );
}



class CFeTriBasis
{
public:
	CFeTriBasis( )
	{}

	CFeTriBasis( const Vector &vTentativeX, const Vector &vTentativeY )
	{
		Init( vTentativeX, vTentativeY );
	}

	void Init( const Vector &vTentativeX, const Vector &vTentativeY )
	{
		v1x = vTentativeX.Length( );
		vAxisX = vTentativeX / v1x; // ~9 flops
		v2.x = DotProduct( vTentativeY, vAxisX );
		vAxisY = vTentativeY - vAxisX * v2.x; // ~15 flops
		v2.y = vAxisY.Length( );
		vAxisY /= v2.y;
	}

	const Vector2D WorldToLocalXY( const Vector &p )const // ~6 flops
	{
		return Vector2D( DotProduct( p, vAxisX ), DotProduct( p, vAxisY ) );
	}

	const Vector LocalXYToWorld( const Vector2D &vLocal )const
	{
		return vLocal.x * vAxisX + vLocal.y * vAxisY;
	}

	const Vector LocalXYToWorld( float vLocalX, float vLocalY )const
	{
		return vLocalX * vAxisX + vLocalY * vAxisY;
	}

	void UnrotateXY( const CSinCosRotation2D& rotation ) // ~12 flops
	{
		Vector vTempX = vAxisX * rotation.m_flCos + vAxisY * rotation.m_flSin;
		Vector vTempY = vAxisY * rotation.m_flCos - vAxisX * rotation.m_flSin;
		this->vAxisX = vTempX;
		this->vAxisY = vTempY;
	}

public:
	Vector vAxisX, vAxisY;
	float v1x;
	Vector2D v2;
};





class CFeBasis
{
public:
	// ~30 flops
	CFeBasis()
	{}

	CFeBasis( const Vector &vTentativeX, const Vector &vTentativeY )
	{
		Init( vTentativeX, vTentativeY );
	}

	void Init( const Vector &vTentativeX, const Vector &vTentativeY )
	{
		vAxisX = vTentativeX.NormalizedSafe( Vector( 1,0,0 ) ); // ~9 flops
		vAxisY = ( vTentativeY - vAxisX * DotProduct( vTentativeY, vAxisX ) ).NormalizedSafe( Vector( 0,1,0) ); // ~15 flops
		vAxisZ = CrossProduct( vAxisX, vAxisY ); // ~6flops
	}

	const Vector2D WorldToLocalYZ( const Vector &p )const
	{
		return Vector2D( DotProduct( p, vAxisY ), DotProduct( p, vAxisZ ) );
	}

	const Vector2D WorldToLocalXY( const Vector &p )const // ~6 flops
	{
		return Vector2D( DotProduct( p, vAxisX ), DotProduct( p, vAxisY ) );
	}

	const Vector WorldToLocal( const Vector &p )const
	{
		return Vector( DotProduct( p, vAxisX ), DotProduct( p, vAxisY ), DotProduct( p, vAxisZ ) );
	}

	const Vector LocalToWorld( const Vector &vLocal )const
	{
		return vLocal.x * vAxisX + vLocal.y * vAxisY + vLocal.z * vAxisZ;
	}

	const Vector LocalToWorld( float x, float y, float z )const
	{
		return x * vAxisX + y * vAxisY + z * vAxisZ;
	}

	const Vector LocalToWorld( const Vector4D &vLocal )const // ~9flops
	{
		return vLocal.x * vAxisX + vLocal.y * vAxisY + vLocal.z * vAxisZ;
	}
	void UnrotateXY( const CSinCosRotation2D& rotation ) // ~12 flops
	{
		Vector vTempX = vAxisX * rotation.m_flCos + vAxisY * rotation.m_flSin;
		Vector vTempY = vAxisY * rotation.m_flCos - vAxisX * rotation.m_flSin;
		this->vAxisX = vTempX;
		this->vAxisY = vTempY;
	}

public:
	Vector vAxisX, vAxisY, vAxisZ;
};




class FourVectorsYZ
{
public:
	FourVectorsYZ( ){}
	FourVectorsYZ( const fltx4 &_y, const fltx4 &_z ):
		y( _y ),
		z( _z )
	{
	}

public:
	fltx4 y;
	fltx4 z;
};


extern const fltx4 Four_SinCosRotation2DMinWeights;

class CSimdSinCosRotation2D
{
public:
	CSimdSinCosRotation2D( fltx4 fl4WeightedCos, fltx4 fl4WeightedSin )
	{
		fltx4 fl4Weight = fl4WeightedCos * fl4WeightedCos + fl4WeightedSin * fl4WeightedSin;
		fltx4 fl4WeightInv = ReciprocalSqrtSIMD( fl4Weight );
		fltx4 isValid = CmpGtSIMD( fl4Weight, Four_SinCosRotation2DMinWeights );
		m_fl4Sin = AndSIMD( isValid, fl4WeightedSin * fl4WeightInv );
		m_fl4Cos = MaskedAssign( isValid, fl4WeightedCos * fl4WeightInv, Four_Ones );
	}

	const FourVectors2D operator *( const FourVectors2D &d )
	{
		return FourVectors2D( d.x * m_fl4Cos - d.y * m_fl4Sin, d.y * m_fl4Cos + d.x * m_fl4Sin ); // ~4flops if really using madd
	}

	fltx4 GetCosine( )const { return m_fl4Cos; }
	fltx4 GetSine( )const { return m_fl4Sin; }
public:
	fltx4 m_fl4Sin;
	fltx4 m_fl4Cos;
};


class CFeSimdBasis
{
public:
	// ~30 flops
	CFeSimdBasis( const FourVectors &vTentativeX, const FourVectors &vTentativeY )
	{
		vAxisX = vTentativeX.NormalizedSafeX( ); // ~9 flops
		vAxisY = ( vTentativeY - vAxisX * DotProduct( vTentativeY, vAxisX ) ).NormalizedSafeY( ); // ~15 flops
		vAxisZ = CrossProduct( vAxisX, vAxisY ); // ~6flops
	}

	const FourVectorsYZ WorldToLocalYZ( const FourVectors &p )const
	{
		return FourVectorsYZ( DotProduct( p, vAxisY ), DotProduct( p, vAxisZ ) );
	}

	const FourVectors2D WorldToLocalXY( const FourVectors &p )const
	{
		return FourVectors2D( DotProduct( p, vAxisX ), DotProduct( p, vAxisY ) );
	}

	const FourVectors WorldToLocal( const FourVectors &p )const
	{
		return FourVectors( DotProduct( p, vAxisX ), DotProduct( p, vAxisY ), DotProduct( p, vAxisZ ) );
	}

	const FourVectors LocalToWorld( const fltx4& x, const fltx4& y, const fltx4& z )const
	{
		return vAxisX * x + vAxisY * y + vAxisZ * z;
	}

	const FourVectors LocalToWorld( const FourVectors &vLocal )const
	{
		return vAxisX * vLocal.x + vAxisY * vLocal.y + vAxisZ * vLocal.z;
	}

	void UnrotateXY( const CSimdSinCosRotation2D& rotation ) // ~12 flops
	{
		FourVectors vTempX = vAxisX * rotation.m_fl4Cos + vAxisY * rotation.m_fl4Sin;
		FourVectors vTempY = vAxisY * rotation.m_fl4Cos - vAxisX * rotation.m_fl4Sin;
		this->vAxisX = vTempX;
		this->vAxisY = vTempY;
	}
public:
	FourVectors vAxisX, vAxisY, vAxisZ;
};



class CFeSimdTriBasis
{
public:
	CFeSimdTriBasis( )
	{}

	CFeSimdTriBasis( const FourVectors &vTentativeX, const FourVectors &vTentativeY )
	{
		Init( vTentativeX, vTentativeY );
	}

	void Init( const FourVectors &vTentativeX, const FourVectors &vTentativeY )
	{
		fltx4 v1xLenSqr = vTentativeX.LengthSqr( ), v1xLenInv = ReciprocalSqrtSIMD( v1xLenSqr );
		this->v1x = v1xLenSqr * v1xLenInv;
		this->vAxisX = vTentativeX * v1xLenInv;
		this->v2.x = DotProduct( vTentativeY, this->vAxisX );
		this->vAxisY = vTentativeY - this->vAxisX * this->v2.x; 
		fltx4 v2yLenSqr = this->vAxisY.LengthSqr( ), v2yLenInv = ReciprocalSqrtSIMD( v2yLenSqr );
		this->v2.y = v2yLenSqr * v2yLenInv;
		this->vAxisY *= v2yLenInv;
	}

	const FourVectors2D WorldToLocalXY( const FourVectors &p )const // ~6 flops
	{
		return FourVectors2D( DotProduct( p, vAxisX ), DotProduct( p, vAxisY ) );
	}

	const FourVectors LocalXYToWorld( const FourVectors2D &vLocal )const
	{
		return vLocal.x * vAxisX + vLocal.y * vAxisY;
	}

	const FourVectors LocalXYToWorld( fltx4 vLocalX, fltx4 vLocalY )const
	{
		return vLocalX * vAxisX + vLocalY * vAxisY;
	}

	void UnrotateXY( const CSimdSinCosRotation2D& rotation ) // ~12 flops
	{
		FourVectors vTempX = vAxisX * rotation.m_fl4Cos + vAxisY * rotation.m_fl4Sin;
		FourVectors vTempY = vAxisY * rotation.m_fl4Cos - vAxisX * rotation.m_fl4Sin;
		this->vAxisX = vTempX;
		this->vAxisY = vTempY;
	}

public:
	FourVectors vAxisX, vAxisY;
	fltx4 v1x;
	FourVectors2D v2;
};


// 7 flops, with madd
class CSimdSinCosRotation
{
public:
	explicit CSimdSinCosRotation( const FourVectors &vSinOmega )
	{
		// ~10flops
		m_v4SinOmega = vSinOmega;
// 		m_fl4CosOmega = ReciprocalSqrtSIMD( Four_Ones + vSinOmega.LengthSqr( ) );
// 		m_v4SinOmega *= m_fl4CosOmega;
	}

	const FourVectors operator *( const FourVectors &d )
	{
		return d /** m_fl4CosOmega */+ CrossProduct( m_v4SinOmega, d ); // ~6flops if really using madd
	}

	const FourVectors Unrotate( const FourVectors &d )
	{
		return d /** m_fl4CosOmega */- CrossProduct( m_v4SinOmega, d ); // ~6flops if really using madd
	}


	//fltx4 GetCosine( )const { return m_fl4CosOmega; }
protected:
	FourVectors m_v4SinOmega;
	//fltx4 m_fl4CosOmega;
};



inline FeCtrlOffset_t *CFeModel::FindCtrlOffsetByChild( uint nChild )
{
	for ( uint i = 0; i < m_nCtrlOffsets; ++i )
	{
		if ( m_pCtrlOffsets[ i ].nCtrlChild == nChild )
		{
			return &m_pCtrlOffsets[ i ];
		}
	}
	return NULL;
}

inline FeCtrlOsOffset_t *CFeModel::FindCtrlOsOffsetByChild( uint nChild )
{
	for ( uint i = 0; i < m_nCtrlOsOffsets; ++i )
	{
		if ( m_pCtrlOsOffsets[ i ].nCtrlChild == nChild )
		{
			return &m_pCtrlOsOffsets[ i ];
		}
	}
	return NULL;
}

FORCEINLINE const Vector AsVector( const fltx4& f4 )
{
	return *( const Vector* )( char* )&f4; // casting through char* because that's the only way to avoid pointer aliasing issues
}

#endif // RUBIKON_FEM_CLOTH_HDR

