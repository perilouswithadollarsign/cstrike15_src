//========= Copyright © Valve Corporation, All rights reserved. ============//
#ifndef VPHYSICS2_PARAM_TYPES
#define VPHYSICS2_PARAM_TYPES

#include "mathlib/mathlib.h"
#include "mathlib/eigen.h"
#include "tier1/utlvector.h"
#include "rubikon/serializehelpers.h"
#include "rubikon/constants.h"
#include "mathlib/transform.h"

struct RnMaterial_t;


#if _MSC_VER >= 1300 // msvc 7.1
#define RN_DEPRECATED __declspec( deprecated )
#pragma warning(1 : 4996) // deprecated warning
#else
#define RN_DEPRECATED
#endif


//-------------------------------------------------------------------------------------------------
// Angular velocity type 
//-------------------------------------------------------------------------------------------------
enum AngularVelocityUnits_t
{
	RAD_PER_SECOND,
	DEG_PER_SECOND
};


//-------------------------------------------------------------------------------------------------
// Simulation type 
//-------------------------------------------------------------------------------------------------
enum PhysicsSimulation_t
{
	DISCRETE_SIM,
	CONTINUOUS_SIM
};

//-------------------------------------------------------------------------------------------------
// Softbody simulation type 
//-------------------------------------------------------------------------------------------------
enum PhysicsSoftbodySimulation_t
{
	SOFTBODY_SIM_RELAXATION				= 1 << 0,
	SOFTBODY_SIM_CONJUGATE_GRADIENT		= 1 << 1,
	SOFTBODY_SIM_MULTIGRID				= 1 << 2,
	SOFTBODY_SIM_DIAGONAL_PRECONDITIONER	= 1 << 3,
	SOFTBODY_SIM_LINEARIZATION_GUARD	= 1 << 4,
	SOFTBODY_SIM_WARMSTART				= 1 << 5,
	SOFTBODY_SIM_TRIDIAGONAL_PRECONDITIONER = 1 << 6,
	SOFTBODY_SIM_RELAXATION_PRECONDITIONER = 1 << 7,
	SOFTBODY_SIM_BOXERMAN_DAMPING		= 1 << 8,
	SOFTBODY_SIM_EXPERIMENTAL_1 = 1 << 24,
	SOFTBODY_SIM_EXPERIMENTAL_2 = 1 << 25,
	SOFTBODY_SIM_EXPERIMENTAL_3 = 1 << 26,
	SOFTBODY_SIM_EXPERIMENTAL_4 = 1 << 27
};

enum PhysicsSoftbodyDebugDrawFlags_t
{
	SOFTBODY_DEBUG_DRAW_TREE_BOTTOM_UP = 1 << 0,
	SOFTBODY_DEBUG_DRAW_TREE_TOP_DOWN  = 1 << 1,
	SOFTBODY_DEBUG_DRAW_TREE_CLUSTERS  = 1 << 2
};

enum PhysicsSoftbodyAnimSpace_t
{
	SOFTBODY_ANIM_SPACE_WORLD,
	SOFTBODY_ANIM_SPACE_LOCAL
};

enum PhysicsSoftbodySimTransformsFlags_t
{
	SOFTBODY_SIM_TRANSFORMS_INCLUDE_STATIC = 1
};

enum PhysicsSoftbodyEnergyTypeEnum_t
{
	SOFTBODY_ENERGY_ELASTIC,
	SOFTBODY_ENERGY_KINEMATIC,
	SOFTBODY_ENERGY_POTENTIAL,
	SOFTBODY_ENERGY_TOTAL,
	SOFTBODY_ENERGY_COUNT
};

//-------------------------------------------------------------------------------------------------
// Stabilization type 
//-------------------------------------------------------------------------------------------------
enum PhysicsStabilization_t
{
	BAUMGARTE_STAB,
	PROJECTION_STAB
};


//-------------------------------------------------------------------------------------------------
// Body type 
//-------------------------------------------------------------------------------------------------
enum PhysicsBodyType_t
{
	BODY_STATIC,
	BODY_KEYFRAMED,
	BODY_DYNAMIC,
	BODY_TYPE_COUNT
};

enum RnContactType_t
{
	CONTACT_CONVEX,
	CONTACT_MESH,

	CONTACT_TYPE_COUNT
};

//-------------------------------------------------------------------------------------------------
// Joint type 
//-------------------------------------------------------------------------------------------------
enum PhysicsJointType_t
{
	NULL_JOINT,
	SPHERICAL_JOINT,
	PRISMATIC_JOINT,
	REVOLUTE_JOINT,
	QUAT_ORTHOTWIST_JOINT,
	MOUSE_JOINT,
	WELD_JOINT,

	SPRING,
	PULLEY,
	GEAR,

	CONICAL_JOINT,

	JOINT_COUNT,

	INVALID_JOINT_TYPE = JOINT_COUNT
};


//-------------------------------------------------------------------------------------------------
// Joint state
//-------------------------------------------------------------------------------------------------
enum JointStateFlagsEnum_t
{
	JOINT_STATE_DEACTIVATED	= 1 << 0,
	JOINT_STATE_COLLIDE = 1 << 1,						// the joint does NOT collide if it's disabled, but it keeps this state in case it's re-enabled later
	JOINT_STATE_LINEAR_CONSTRAINT_DISABLED = 1 << 2,	// only angular constraint: linear constraint is not active and objects are free to translate (but not rotate) independently
	JOINT_STATE_ANGULAR_CONSTRAINT_DISABLED = 1 << 3,	// only linear constraint: angular constraints are not active and objects are free to rotate (but not translate) independently
	JOINT_STATE_MOTOR_ENABLED = 1 << 4,					// enable/disable motor
	JOINT_STATE_LIMIT_ENABLED = 1 << 5					// enable/disable limit (we might need to distinguish between linear and angular limit if we add a cylindrical joint)
};


//-------------------------------------------------------------------------------------------------
// Limit state
//-------------------------------------------------------------------------------------------------
enum PhysicsLimitState_t
{
	LIMIT_FREE,
	LIMIT_AT_LOWER_LIMIT,
	LIMIT_AT_UPPER_LIMIT,
	LIMIT_AT_LIMIT
};


//-------------------------------------------------------------------------------------------------
// Range (e.g. used for defining limits)
//-------------------------------------------------------------------------------------------------
struct Range_t
{
	float m_flMin;
	float m_flMax;

	Range_t( void ) { m_flMin = FLT_MAX; m_flMax = -FLT_MAX; }
	Range_t( float flMin, float flMax ) { m_flMin = flMin; m_flMax = flMax; }
};

inline bool operator==( const Range_t& lhs, const Range_t& rhs )
{
	return lhs.m_flMin == rhs.m_flMin && lhs.m_flMax == rhs.m_flMax;
}

inline bool operator!=( const Range_t& lhs, const Range_t& rhs )
{
	return lhs.m_flMin != rhs.m_flMin || lhs.m_flMax != rhs.m_flMax;
}


//-------------------------------------------------------------------------------------------------
// Shape type 
//-------------------------------------------------------------------------------------------------
enum PhysicsShapeType_t
{
	SHAPE_SPHERE,
	SHAPE_CAPSULE,
	SHAPE_HULL,
	SHAPE_MESH,
	SHAPE_COUNT,
	SHAPE_UNKNOWN
};


enum PhysicsPseudoShapeEnum_t
{
	PSEUDO_SHAPE_BOX = SHAPE_COUNT,
	PSEUDO_SHAPE_COMPOUND,
	PSEUDO_SHAPE_NONE,
	PSEUDO_SHAPE_COUNT
};



//-------------------------------------------------------------------------------------------------
// ShapeCastResult
//-------------------------------------------------------------------------------------------------
enum RnSelectStaticDynamic
{
	SELECT_STATIC = 1,
	SELECT_DYNAMIC = 2, // <sergiy> the "2" is for binary backwards-compatibility with PhysX, but I hope we'll switch to keyframed_only |dynamic_only combo later, it'd be more convenient for physics testbed
	
	SELECT_KEYFRAMED_ONLY = 0x104, // select kinematic, but not dynamic
	SELECT_DYNAMIC_ONLY   = 0x108, // select dynamic, but not kinematic

	SELECT_ALL = SELECT_STATIC | SELECT_DYNAMIC,
};


enum RnDebugLayers_t 
{
	RN_DRAW_STATIC_BODIES					= 1 << 0,	// Draw all shapes associated with a rigid body 
	RN_DRAW_KEYFRAMED_BODIES				= 1 << 1,	// Draw all shapes associated with a rigid body 
	RN_DRAW_DYNAMIC_BODIES					= 1 << 2,	// Draw all shapes associated with a rigid body 
	RN_DRAW_CONTACT_POINTS					= 1 << 3,	// Draw contact points and normals
	RN_DRAW_CONTACT_MANIFOLDS				= 1 << 4,
	RN_DRAW_CONTACT_INDICES					= 1 << 5,
	RN_DRAW_CONTACTS						= RN_DRAW_CONTACT_POINTS | RN_DRAW_CONTACT_MANIFOLDS | RN_DRAW_CONTACT_INDICES,	
	RN_DRAW_JOINTS							= 1 << 6,	// Draw joints and limits
	RN_DRAW_PROXIES							= 1 << 7,	// Draw broadphase proxies associated with each shape
	RN_DRAW_RECORDED_RAYCASTS				= 1 << 8,
	RN_DRAW_RECORDED_FORCES					= 1 << 9,
	RN_DRAW_INERTIA_BOXES					= 1 << 10,
	RN_DRAW_MESH_FACE_INDICES				= 1 << 11,
	RN_DRAW_MESH_VERTEX_INDICES				= 1 << 12,
	RN_DRAW_MESH_EDGE_INDICES				= 1 << 13,
	RN_DRAW_SOFTBODIES						= 1 << 14,
	RN_DRAW_SOFTBODY_INDICES				= 1 << 15,
	RN_DRAW_SOFTBODY_FIELDS					= 1 << 16,
	RN_DRAW_AWAKE_BODIES					= 1 << 17,
	RN_DRAW_RECORDED_CAPSULE_SWEEPS			= 1 << 18,	// Draw recorded capsule sweeps for character controller
	RN_DRAW_RECORDED_COLLISION_RESULTS		= 1 << 19,   // Draw recorded contact planes for character controller 
	RN_DRAW_SOFTBODY_BASES                  = 1 << 20
};

enum RnSoftbodyDrawLayers_t
{
	RN_SOFTBODY_DRAW_EDGES = 1 << 0,
	RN_SOFTBODY_DRAW_POLYGONS = 1 << 1,
	RN_SOFTBODY_DRAW_INDICES = 1 << 2,
	RN_SOFTBODY_DRAW_FIELDS = 1 << 3,
	RN_SOFTBODY_DRAW_BASES = 1 << 4,
	RN_SOFTBODY_DRAW_WIND = 1 << 5
};

struct RnTreeStats_t
{
	RnTreeStats_t( )
	{
		m_nNodeCount = 0;
		m_nLeafCount = 0;
		m_nTreeHeight = 0;
		m_flLeafMetric = 0;
		m_flNodeMetric = 0;
		m_flRootMetric = 0;
	}

	float GetLeafFactor( ) const 
	{
		return m_flRootMetric > 0 ? m_flLeafMetric / m_flRootMetric : 0;
	}

	float GetNodeFactor( ) const
	{
		return m_flRootMetric > 0 ? m_flNodeMetric / m_flRootMetric : 0;
	}

	uint m_nNodeCount;
	uint m_nLeafCount;
	uint m_nTreeHeight;
	float m_flRootMetric;
	float m_flLeafMetric;
	float m_flNodeMetric;
};


//-------------------------------------------------------------------------------------------------
// Shape cast result
//-------------------------------------------------------------------------------------------------
struct CShapeCastResult
{
	float m_flHitTime; 
	Vector m_vHitPoint;
	Vector m_vHitNormal;
	const RnMaterial_t *m_pMaterial;
	bool m_bStartInSolid;

	CShapeCastResult( void )
	{
		m_flHitTime = 1.0f; 
		m_vHitPoint = Vector( 0, 0, 0 );
		m_vHitNormal = Vector( 0, 0, 0 );
		m_bStartInSolid = false;
		m_pMaterial = NULL;
	}

	bool DidHit( void )
	{
		return m_flHitTime < 1.0f;
	}
};


//-------------------------------------------------------------------------------------------------
// Distance query
//-------------------------------------------------------------------------------------------------
struct RnDistanceQueryResult_t
{
	float m_flDistance;			//!< Distance between shapes (distance of zero means shapes are touching or penetrating)
	Vector m_vPoint1;			//!< Closest point on first shape
	Vector m_vPoint2;			//!< Closest point on second shape

	bool Overlap() const
	{
		return m_flDistance == 0.0f;
	}

	Vector GetNormal() const
	{
		Vector vNormal = m_vPoint2 - m_vPoint1;
		VectorNormalize( vNormal );

		return vNormal;
	}
};

#define MAX_SIMPLEX_VERTICES	4

struct RnSimplexCache_t
{
	float m_flMetric;
	int m_nVertexCount;
	uint8 m_Vertices1[ MAX_SIMPLEX_VERTICES ];
	uint8 m_Vertices2[ MAX_SIMPLEX_VERTICES ];
	float m_Lambdas[ MAX_SIMPLEX_VERTICES ];

	void Clear( void ) { m_nVertexCount = 0; }
	bool IsEmpty( void ) const	{ return m_nVertexCount == 0; }
};

struct RnIteration_t
{
	float m_flAlpha;
	RnDistanceQueryResult_t m_Query;
	RnSimplexCache_t m_Cache;

	Vector m_vLocalVertexA1;
	Vector m_vLocalVertexA2;
	Vector m_vLocalVertexB1;
	Vector m_vLocalVertexB2;
	Vector m_vLocalAxis;

	int m_nWitnessA;
	int m_nWitnessB;
};


//-------------------------------------------------------------------------------------------------
// Shape material
//-------------------------------------------------------------------------------------------------
struct RnMaterial_t
{
	RnMaterial_t()
	{

	}

	// Default material	(density of water in kg/inch^3)
	RnMaterial_t( float flFriction, float flRestitution )
	: m_flDensity( 0.015625f )
	, m_flFriction( flFriction )
	, m_flRestitution( flRestitution )
	{

	}

	float m_flDensity;
	float m_flFriction;
	float m_flRestitution;
	uintp m_pUserData AUTO_SERIALIZE_AS( CPhysSurfacePropertiesHandle );
};


//-------------------------------------------------------------------------------------------------
// This describes definitively the box position, orientation and size. 
// Useful for testing, to pass around the box with inertia equivalent to a given non-box body
//-------------------------------------------------------------------------------------------------
struct PhysicsInertiaBox_t
{
	Quaternion m_qOrientation; // global orientation (it's the principal axes orientation for an equivalent body)
	Vector m_vCenter; // (center of mass of equivalent body)
	Vector m_vSize; // the dimensions of the box is -m_vSize ... +m_vSize
	Vector m_vInertiaTensor; // eigenvalues of inertia tensor

	static PhysicsInertiaBox_t GetDefault( )
	{
		PhysicsInertiaBox_t ib;
		ib.m_qOrientation = quat_identity;
		ib.m_vCenter = vec3_origin;
		ib.m_vSize = Vector( 1,1,1 );
		ib.m_vInertiaTensor = Vector( 1,1,1 );
		return ib;
	}
};

//---------------------------------------------------------------------------------------
// Manifold
//---------------------------------------------------------------------------------------
struct ManifoldPoint_t
{
	Vector m_vPosition;									//! The contact point in world coordinates on the surface of shape1
	Vector m_vLocalP1, m_vLocalP2;						//! The contact point in local coordinates of shape1 and shape2
	float m_flSeparation;								//! The separation at the contact point. A negative value means that the shapes are penetrating.
	float m_flImpulse;									//! The non-penetration impulse applied by the solver at this contact point (must be greater or equal zero)
	uint32 m_nID;										//! A unique ID for this contact point to match it to the old manifold and find the last applied non-penetration impulse 
};

struct Manifold_t
{
	int m_nPointCount;									//! The number of contact points in the manifold (can be zero to four)
	ManifoldPoint_t m_Points[ MAX_CONTACT_POINTS ];		//! The current contact patch. All points lie in the same plane

	Vector m_vCenter;									//! The center of the current contact patch
	Vector m_vNormal;									//! The plane normal of current contact patch (points from shape1 towards shape2)
	Vector m_vTangent1, m_vTangent2;						

	float m_flTangentImpulse1;							//! The friction impulse applied by the solver in the direction of tangent1
	float m_flTangentImpulse2;							//! The friction impulse applied by the solver in the direction of tangent2
	float m_flTwistImpulse;								//! The torsional friction impulse applied by the solver in the direction of the normal
};


//---------------------------------------------------------------------------------------
// Swept transform
//---------------------------------------------------------------------------------------
struct RnSweptTransform_t
{
	RnSweptTransform_t() {}

	RnSweptTransform_t( const CTransform& xform1, const CTransform& xform2 )
	: m_vPosition1( xform1.m_vPosition )
	, m_vPosition2( xform2.m_vPosition )
	, m_qOrientation1( xform1.m_orientation )
	, m_qOrientation2( xform2.m_orientation )
	{
		// Normalize sweep (e.g. check polarity)
		Normalize();
	}

	RnSweptTransform_t( matrix3x4_t xform1, matrix3x4_t xform2 )
	{
		// Normalize the matrix to avoid issues when 
		// converting the rotation to a quaternion!
		MatrixNormalize( xform1, xform1 );
		MatrixNormalize( xform2, xform2 );

		m_vPosition1 = xform1.GetOrigin();
		m_vPosition2 = xform2.GetOrigin();
		m_qOrientation1 = xform1.ToQuaternion();
		m_qOrientation2 = xform2.ToQuaternion();

		// Normalize sweep (e.g. check polarity)
		Normalize();
	}

	CTransform Interpolate( float flAlpha ) const
	{
		AssertDbg( IsNormalized() );

		CTransform out;
		out.m_vPosition = ( 1.0f - flAlpha ) * m_vPosition1 + flAlpha * m_vPosition2;
		out.m_orientation = ( 1.0f - flAlpha ) * m_qOrientation1 + flAlpha * m_qOrientation2;
		QuaternionNormalize( out.m_orientation );

		return out;
	}

	void Normalize()
	{
		if ( QuaternionDotProduct( m_qOrientation1, m_qOrientation2 ) < 0.0f )
		{
			m_qOrientation2 = -m_qOrientation2;
		}
	}

	bool IsNormalized() const
	{
		return QuaternionDotProduct( m_qOrientation1, m_qOrientation2 ) >= 0.0f;
	}

	Vector m_vPosition1, m_vPosition2;
	Quaternion m_qOrientation1, m_qOrientation2;
};


//---------------------------------------------------------------------------------------
// Snooping
//---------------------------------------------------------------------------------------
struct RnSnooperAdvertisement_t
{
	int64 m_nProcessId;
	int64 m_nRnWorldAddr;
	char m_Name[48];
};

struct RnWorldSnoopStats_t: public RnSnooperAdvertisement_t
{
	uint m_nBodies;
	uint m_nSoftbodies;
	uint m_nJoints;
	uint m_nContacts;
	uint m_nSimulationFrame;
	float m_flSimulationTimeElapsed;
};

struct RnWorldSnoopParams_t
{
	RnWorldSnoopParams_t()
	{
		V_memset( this, 0, sizeof( *this ) );
	}
	int m_nProcessId;
	uint64 m_nRemoteAddr;
	uint m_nTimeout;
	bool m_bOnlyIfFrameChanged;
	bool m_bPurgeResourceCaches;
	bool m_bFailOnTimeout; // when this is set to true, it make snooping safe: when snooper cannot lock the snoop mutex, it won't try to snoop
};

enum RnWorldSnoopEnum_t
{
	RN_WORLD_SNOOP_OK,
	RN_WORLD_SNOOP_UNCHANGED,
	RN_WORLD_SNOOP_ERROR,
	RN_WORLD_SNOOP_CRASH,
	RN_WORLD_SNOOP_TIMEOUT
};

class CRnBody;
class CRnShape;
class CPhysSurfaceProperties;

struct RecordedTraceResult_t
{
	const CRnBody *m_pHitObject;
	const CRnShape *m_pHitShape;

	Vector m_vHitPoint;
	Vector m_vHitNormal;
	float m_flHitOffset;
	float m_flFraction; 
	bool m_bStartInSolid;
	const CPhysSurfaceProperties *m_pSurfaceProperties;

	Vector m_vRayStart;
	Vector m_vRayDelta;
	Vector m_vExtents;
	int m_nSelect;
};

struct RecordedForce_t
{
	const CRnBody *m_pHitObject;
	const CRnShape *m_pHitShape;
	Vector m_vForcePoint;
	Vector m_vForce;
};

struct RnInertiaProperties_t
{
	matrix3x4_t m_Inertia;
	float m_flMass;

	const Vector GetMassCenter( ) const { return m_Inertia.GetOrigin(); }
	float GetMass() const { return m_flMass; }
	Quaternion GetOrientation( Vector &inertia )const 
	{
		return Diagonalizer( m_Inertia, inertia );
	}
	Quaternion GetOrientation( )const
	{
		Vector inertia;
		NOTE_UNUSED( inertia );
		return Diagonalizer( m_Inertia, inertia );
	}
	matrix3x4_t GetPrincipalTransform()const
	{
		return QuaternionMatrix( GetOrientation(), m_Inertia.GetOrigin() );
	}
	void SetMassCenter( const Vector &v )
	{
		m_Inertia.SetOrigin( v );
	}

	static const RnInertiaProperties_t GetDefault()
	{
		RnInertiaProperties_t out;
		V_memset( &out, 0, sizeof( out ) );
		return out;
	}
};


inline const RnInertiaProperties_t TransformInertiaProperties( const matrix3x4_t &tm, const RnInertiaProperties_t &ip )
{
	RnInertiaProperties_t out;
	out.m_flMass = ip.m_flMass;
	// Multiply R * I * R^-1
	out.m_Inertia = ConcatTransforms( ConcatTransforms( tm, ip.m_Inertia ), MatrixInvert( tm ) );
	out.SetMassCenter( tm.TransformVector( ip.GetMassCenter() ) );
	return out;
}


struct RnSupport_t
{
	Vector m_vPoint;     // support point - farthest in the given direction
	float m_flDistance;  // support distance - max in the given direction
public:
	const Vector &GetPoint()const { return m_vPoint; }
	float GetDistance() const { return m_flDistance; }

	static RnSupport_t GetDefault() 
	{
		RnSupport_t out;
		out.m_flDistance = -FLT_MAX;
		out.m_vPoint.Init(0,0,0);
		return out;
	}
};

struct CRnContactStats
{
	uint m_nContacts;
	uint m_nPoints;
	uint m_nTouchingPoints;
	uint m_nManifolds;

	CRnContactStats( uint nContacts = 0 )
	{
		m_nContacts = nContacts;
		m_nPoints = 0;
		m_nTouchingPoints = 0;
		m_nManifolds = 0;
	}

	CRnContactStats& operator += ( const CRnContactStats&that )
	{
		m_nContacts += that.m_nContacts;
		m_nPoints += that.m_nPoints;
		m_nTouchingPoints += that.m_nTouchingPoints;
		m_nManifolds += that.m_nManifolds;
		return *this;
	}
};

struct RnPlane_t;
typedef CUtlVectorFixedGrowable< RnPlane_t, 32 > PlaneBuffer;

struct RnDebugDrawOptions_t
{
	RnDebugDrawOptions_t( uint nLayers = 0 ) { m_nLayers = nLayers; m_nSoftbodyLayers = 1; }
	uint32 m_nLayers;
	uint32 m_nSoftbodyLayers;

	void EnableLayers( uint nLayersMask, bool bEnable )
	{
		if ( bEnable )
			m_nLayers |= nLayersMask;
		else
			m_nLayers &= ~nLayersMask;
	}
};

struct PhysGenericCallback_t
{
	void( *m_pCallbackFn )( void * );
	void *m_pData;
	PhysGenericCallback_t( ) : m_pCallbackFn( NULL ), m_pData( NULL ) {}
	PhysGenericCallback_t( void( *pCallbackFn )( void * ), void *pData = NULL ): m_pCallbackFn( pCallbackFn ), m_pData( pData ) { }

	void Call()	const
	{
		m_pCallbackFn( m_pData );
	}
	bool operator == ( const PhysGenericCallback_t &that )const
	{
		return m_pCallbackFn == that.m_pCallbackFn && m_pData == that.m_pData;
	}
};

enum PhysCallbackStage_t
{
	PHYS_CALLBACK_END_OF_STEP,
	PHYS_CALLBACK_COUNT
};


class CDebugHighlightCone
{
public:
	Vector m_vApex;
	Vector m_vAxis;
	float m_flSlope; // 0 means 0-width cone; 1 means cone surface goes 45 degrees from the axis
public:
	CDebugHighlightCone():
		m_vAxis(0, 0, 1), m_vApex(0, 0, 0), m_flSlope( 0)
	{
	}

	bool ContainsPoint( const Vector &v )
	{
		return Highlight( v ) > 0.0f;
	}
	
	float Highlight( const Vector &v )
	{
		if ( m_flSlope <= 0 )
			return 0.0f;
		
		Vector d = v - m_vApex;
		float flDistAlongAxis = DotProduct( d, m_vAxis );
		if ( flDistAlongAxis <= 0 )
			return 0.0f;
		
		float flDistToAxis = ( d - m_vAxis * flDistAlongAxis ).Length();
		if ( flDistToAxis >= flDistAlongAxis * m_flSlope )
			return 0.0f;
		else
		{
			return 1.0f - flDistToAxis / flDistAlongAxis * m_flSlope;
		}
	}
};

struct RnHullSimplificationParams_t
{
	float m_flPrecisionDegrees;
	float m_flPrecisionInches;
	int m_nMaxFaces;
	int m_nMaxEdges;
	int m_nMaxVerts;
	
	RnHullSimplificationParams_t()
	{
		m_flPrecisionDegrees = 0.0f;
		m_flPrecisionInches = 0.0f;
		m_nMaxFaces = 0;
		m_nMaxEdges = 0;
		m_nMaxVerts = 0;
	}

	bool HasLimits()const
	{
		return m_flPrecisionDegrees > 0 || m_nMaxVerts > 0 || m_nMaxFaces > 0 || m_nMaxEdges > 0;
	}
};

#endif
