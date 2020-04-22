//========== Copyright (c) Valve Corporation. All Rights Reserved. ============
#ifndef AUTHPHYSMODEL_HDR
#define AUTHPHYSMODEL_HDR

#ifdef COMPILER_MSVC
#pragma once
#endif

#include "tier1/utlincrementalvector.h"
#include "tier1/utlstringtoken.h"
#include "resourcefile/resourcestream.h"
#include "mathlib/aabb.h"
#include "mathlib/vertexcolor.h"
#include "rubikon/param_types.h"
#include "mathlib/femodeldesc.h"
#include "mathlib/femodelbuilder.h"
#include "tier1/utlhashtable.h"
#include "bitvec.h"
#include "tier1/utlstringmap.h"
#include "tier1/utlsortvector.h"
#include "mdlobjects/vpropbreakabledata.h"

struct VPhysXBodyPart_t;
struct RnSphereDesc_t;
struct RnCapsuleDesc_t;
struct RnHullDesc_t;
struct RnMeshDesc_t;
struct RnShapeDesc_t;
struct AABB_t;
struct RnHull_t;
class CToolSceneTraceEnvironment;
class CMesh;
struct GizmoTransform_t;
struct RnHullSimplificationParams_t;
struct PhysSoftbodyDesc_t;
class CUtlSymbolTable;
template <class T >class CUtlStringMap;
class CPhysModelSource;
/*schema*/ class CAuthPhysBody;
class KeyValues;
struct VPhysXAggregateData_t;
struct VPhysXJoint_t;
class CResourceStream;
struct VertexPositionNormal_t;
struct VertexPositionColor_t;
struct AuthHullSimplificationParams_t;
class CVClothProxyMeshOptions;


/*schema*/ enum AuthPhysCollisionAttributesModeEnum_t
{
	AUTH_PHYS_COLL_ATTR_IGNORE,						META( MPropertyFriendlyName = "Default/Derived" )
	AUTH_PHYS_COLL_ATTR_OVERRIDE,					META( MPropertyFriendlyName = "Override" )
	AUTH_PHYS_COLL_ATTR_APPEND						META( MPropertyFriendlyName = "Append" )
};
DECLARE_SCHEMA_ENUM( AuthPhysCollisionAttributesModeEnum_t );


enum AuthPhysFxCollisionTypeEnum_t
{
	AUTH_PHYS_FX_SPHERE,
	AUTH_PHYS_FX_PLANE
};



/*schema*/ class CAuthPhysCollisionAttributesOverride;
/*schema*/ struct VPhysXCollisionAttributes_t;
class CAuthPhysCompileContext;



/*schema*/ class CAuthPhysCollisionAttributes
{
public:
	CAuthPhysCollisionAttributes()
	{
		m_CollisionGroup = "default";
	}
	//DECLARE_SCHEMA_DATA_CLASS( CAuthPhysCollisionAttributes )
	void ApplyOverride( const CAuthPhysCollisionAttributesOverride &collOverride );
	void Compile( CAuthPhysCompileContext &context, VPhysXCollisionAttributes_t &entry )const;
	
	CUtlString m_CollisionGroup; META( MPropertyFriendlyName = "Collision Group"; MPropertyChoiceProviderFn = GetCollisionGroupList );
	CUtlVector< CUtlString > m_InteractAs; META( MPropertyFriendlyName = "Interact As"; MPropertyChoiceProviderFn = GetInteractionLayerList );
	CUtlVector< CUtlString > m_InteractWith; META( MPropertyFriendlyName = "Interact With"; MPropertyChoiceProviderFn = GetInteractionLayerList );

	bool operator == ( const CAuthPhysCollisionAttributes & other )const;
};


/*schema*/ class CAuthPhysCollisionAttributesOverride: public CAuthPhysCollisionAttributes
{
public:
	CAuthPhysCollisionAttributesOverride()
	{
		m_nMode = AUTH_PHYS_COLL_ATTR_IGNORE; // ignore by default
	}
	//DECLARE_SCHEMA_DATA_CLASS( CAuthPhysCollisionAttributesOverride )
	AuthPhysCollisionAttributesModeEnum_t m_nMode; META( MPropertyFriendlyName = "Mode" );
};



class CLockecResourceHashFunctor
{
public:
	uint operator() ( const CLockedResource<char> &res )const
	{
		CRC32_t nCrc;
		CRC32_Init( &nCrc );
		CRC32_ProcessBuffer( &nCrc, res, res.Count( ) );
		CRC32_Final( &nCrc );
		return nCrc;
	}
};


class CLockedResourceEqualFunctor
{
public:
	bool operator() ( const CLockedResource<char> &a, const CLockedResource<char> & b )const
	{
		return a.Count( ) == b.Count( ) && !V_memcmp( a, b, a.Count( ) );
	}
};

typedef CUtlHashtable< CLockedResource< char >, empty_t, CLockecResourceHashFunctor, CLockedResourceEqualFunctor > CLockedResourceHashtable;
																	 



class CAuthPhysCompileContext
{
public:
	CAuthPhysCompileContext( CResourceStream *pStream )
		: m_pStream( pStream ), m_DefaultSurfaceProperty( "default" )
	{}

	int ResolveCollisionAttributesIndex();

	const CAuthPhysCollisionAttributes &GetDefaultCollisionAttributes() { return m_DefaultCollisionAttributes; }
	void ApplyOverrideToDefault( const CAuthPhysCollisionAttributesOverride &collAttr ) { m_DefaultCollisionAttributes.ApplyOverride( collAttr ); }
	void SetDefaultCollisionAttributes( const CAuthPhysCollisionAttributes &attr ) { m_DefaultCollisionAttributes = attr; }

	int GetCollAttrPaletteSize( )const { return m_CollAttrPalette.Count();}
	const CAuthPhysCollisionAttributes &GetCollAttrPaletteEntry( int i ) { return m_CollAttrPalette[i]; }
	CResourceStream *GetStream() { return m_pStream; }

	CLockedResource<char> WriteString( const char *pString, uint32 *pHashOut = NULL ); // writes a non-unique string (saving space) , returns its hash
	template <typename T> CLockedResource< T > FindOrWrite( const T *pData, uint nElements )
	{
		CLockedResource< char > res( ( char* ) pData, nElements * sizeof( T ) );
		UtlHashHandle_t hFind = m_WrittenResources.Find( res );
		if ( hFind == m_WrittenResources.InvalidHandle( ) )
		{
			CLockedResource<T> written = m_pStream->Allocate< T>( nElements );
			V_memcpy( written, pData, sizeof ( T ) *nElements );

			m_WrittenResources.Insert( CLockedResource<char>( ( char* ) ( T* ) written, nElements * sizeof( T ) ));
			return written;
		}
		else
		{
			CLockedResource<char> found = m_WrittenResources[ hFind ];
			return CLockedResource< T >( ( T* ) ( char* ) found, nElements ); // found a copy, no need to write it out again
		}
	}

	const CUtlString &GetDefaultSurfaceProperty()const { return m_DefaultSurfaceProperty; }
	void SetDefaultSurfaceProperty( const char *pSurfacePropertyOverride ){ m_DefaultSurfaceProperty = pSurfacePropertyOverride ; }
	int ResolveSurfacePropertyIndex();

	int GetSurfacePropertyPaletteSize() { return m_SurfacePropPalette.Count(); }
	const CUtlString &GetSurfacePropertyPaletteEntry( int i ) { return m_SurfacePropPalette[i]; }
protected:
	CAuthPhysCollisionAttributes m_DefaultCollisionAttributes;
	CResourceStream *m_pStream;
	CUtlVector< CAuthPhysCollisionAttributes > m_CollAttrPalette;

	CLockedResourceHashtable m_WrittenResources;
	CUtlVector< CUtlString > m_SurfacePropPalette;
	CUtlString m_DefaultSurfaceProperty;
};


class CAuthPhysCollisionAttributeScope
{
public:
	CAuthPhysCollisionAttributeScope( CAuthPhysCompileContext* pContext, CAuthPhysCollisionAttributesOverride &collOverride )
	{
		m_pContext = pContext;
		m_SavedDefaults = pContext->GetDefaultCollisionAttributes();
		pContext->ApplyOverrideToDefault( collOverride );
	}
	~CAuthPhysCollisionAttributeScope()
	{
		m_pContext->SetDefaultCollisionAttributes( m_SavedDefaults );
	}
protected:
	CAuthPhysCompileContext* m_pContext;
	CAuthPhysCollisionAttributes m_SavedDefaults;
};

class CAuthPhysSurfacePropertyScope
{
public:
	CAuthPhysSurfacePropertyScope( CAuthPhysCompileContext* pContext, const CUtlString &surfacePropOverride )
	{
		m_pContext = pContext;
		m_SavedDefault = pContext->GetDefaultSurfaceProperty();
		if( !surfacePropOverride.IsEmpty() )
		{
			pContext->SetDefaultSurfaceProperty( surfacePropOverride );
		}
	}
	~CAuthPhysSurfacePropertyScope()
	{
		m_pContext->SetDefaultSurfaceProperty( m_SavedDefault );
	}
protected:
	CAuthPhysCompileContext* m_pContext;
	CUtlString m_SavedDefault;
};


enum PhysicsShapeType_t;


/*schema*/ enum AuthPhysShapeTypeEnum_t
{
	AUTH_PHYS_SHAPE_SPHERE = SHAPE_SPHERE,					META( MPropertyFriendlyName = "Sphere" )
	AUTH_PHYS_SHAPE_CAPSULE = SHAPE_CAPSULE,				META( MPropertyFriendlyName = "Capsule" )
	AUTH_PHYS_SHAPE_HULL = SHAPE_HULL,						META( MPropertyFriendlyName = "Convex Hull" )
	AUTH_PHYS_SHAPE_MESH = SHAPE_MESH,						META( MPropertyFriendlyName = "Triangle Mesh" )
};
DECLARE_SCHEMA_ENUM( AuthPhysShapeTypeEnum_t );








/*schema*/ class CPhysPartBreakableData
{
	//DECLARE_SCHEMA_DATA_CLASS( CPhysPartBreakableData )
	CPhysPartBreakableData()
	{
		m_bMotionDisabled = false;
		m_nHealth = 1;
		m_flBurstScale = 1.0;
		m_flBurstRandomize = 0.0f;
		m_nFadeTime = 20.0; 
		m_nFadeMin = 0;
		m_nFadeMax = 0.0;
		m_bNoShadows = false;
	}

	CUtlString m_CollisionGroup; META( MPropertyFriendlyName = "Collision Group"; MPropertyChoiceProviderFn = GetCollisionGroupList; );
	bool m_bMotionDisabled; META( MPropertyFriendlyName = "Motion Disabled" );
	int m_nHealth;META( MPropertyFriendlyName = "Health" );
	int m_nFadeTime;META( MPropertyFriendlyName = "Fade Time" );
	int m_nFadeMin;META( MPropertyFriendlyName = "Fade Min Distance" );
	int m_nFadeMax;META( MPropertyFriendlyName = "Fade Max Distance" );
	float m_flBurstScale; META( MPropertyFriendlyName = "Burst Scale" );
	float m_flBurstRandomize; META( MPropertyFriendlyName = "Burst Randomize" );
	bool  m_bNoShadows;		  META( MPropertyFriendlyName = "Do Not Cast Shadows" );
	CUtlString m_SurfaceProp; META( MPropertyFriendlyName = "Surface Prop"; MPropertyChoiceProviderFn = GetSurfacePropertyList; );

	CLockedResource< VpropBreakablePartData_t > Compile( CResourceStream *pStream )
	{
		CLockedResource< VpropBreakablePartData_t > pData = pStream->Allocate< VpropBreakablePartData_t >();
		pData->m_nCollisionGroupHash = m_CollisionGroup.IsEmpty() ? 0 : MakeStringToken( m_CollisionGroup.Get() ).GetHashCode();
		pData->m_bMotionDisabled = m_bMotionDisabled;
		pData->m_nHealth = m_nHealth;
		pData->m_nFadeTime = m_nFadeTime;
		pData->m_nFadeMin = m_nFadeMin;
		pData->m_nFadeMax = m_nFadeMax;
		pData->m_flBurstScale = m_flBurstScale;
		pData->m_flBurstRandomize = m_flBurstRandomize;
		pData->m_bNoShadows = m_bNoShadows;
		pData->m_nSurfaceProp = m_SurfaceProp.IsEmpty() ? 0 : MakeStringToken( m_SurfaceProp ).GetHashCode();

		return pData;
	}
};




/*schema*/ enum AuthPhysJointTypeEnum_t
{
	AUTH_PHYS_SPHERICAL_JOINT = SPHERICAL_JOINT,			META( MPropertyFriendlyName = "Spherical Joint" )
	AUTH_PHYS_REVOLUTE_JOINT = REVOLUTE_JOINT,				META( MPropertyFriendlyName = "Revolute Joint" )
	AUTH_PHYS_PRISMATIC_JOINT = PRISMATIC_JOINT,			META( MPropertyFriendlyName = "Prismatic Joint" )
	AUTH_PHYS_ORTHOTWIST_JOINT = QUAT_ORTHOTWIST_JOINT,		META( MPropertyFriendlyName = "Ragdoll Joint" )
	AUTH_PHYS_WELD_JOINT = WELD_JOINT,						META( MPropertyFriendlyName = "Weld Joint" )
	AUTH_PHYS_NULL_JOINT = NULL_JOINT 						META( MPropertyFriendlyName = "Null Joint" )
};
DECLARE_SCHEMA_ENUM( AuthPhysJointTypeEnum_t );


struct AuthPhysRange_t
{
	TYPEMETA( MNoScatter )
	//DECLARE_SCHEMA_DATA_CLASS( AuthPhysRange_t );
	float32 m_flMin;
	float32 m_flMax;

	AuthPhysRange_t() {}
	AuthPhysRange_t( float flMin, float flMax )
		: m_flMin( flMin )
		, m_flMax( flMax )
	{}
};






class CBoneParseParams;


/*schema*/ struct AuthPhysSphereCollision_t
{
	//DECLARE_SCHEMA_DATA_CLASS( AuthPhysSphereCollision_t );
public:
	AuthPhysSphereCollision_t()
	{
		m_bInclusive = false;
		m_flRadius = 16;
		m_vOrigin = vec3_origin;
		m_flStickiness = 0;
	}

	bool m_bInclusive;
	float m_flRadius;
	Vector m_vOrigin;
	float m_flStickiness;
};


class CAuthPhysFxRodMap : public CUtlVector < CUtlSortVector< int > * >
{
public:
	CAuthPhysFxRodMap(){ }
	~CAuthPhysFxRodMap() { PurgeAndDeleteElements(); }

	void Init( int nNodes )
	{
		PurgeAndDeleteElements();
		SetCount( nNodes );
		FillWithValue( NULL );
	}

	void Append( int nNodeA, int nNodeB )
	{
		CUtlSortVector< int > * &list = ( *this )[ nNodeA ];
		if ( !list )
		{
			list = new CUtlSortVector < int > ;
			list->Insert( nNodeB );
		}
		else
		{
			list->InsertIfNotFound( nNodeB );
		}
	}
};

/*schema*/ class CAuthPhysFx
{
	//DECLARE_SCHEMA_DATA_CLASS( CAuthPhysFx );
public:
	CAuthPhysFx(  )
	{
		InitDefaults( );
	}
	~CAuthPhysFx(){}

	int Cleanup();
	template <typename TBitVec>
	void AlignNodes( const TBitVec &nodes );
	void AlignNodes()
	{
		struct IdentityMap_t { bool operator[]( int i ) const { return true; } } identityMap;
		AlignNodes( identityMap );
	}
public:

	/*schema*/ class CDagEdge
	{

		//DECLARE_SCHEMA_DATA_CLASS( CAuthPhysFx::CDagEdge );
	public:
		CDagEdge( )
		{
			m_nParentBone = -1;
			m_nChildBone = -1;
		}

		bool UsesNode( int nNode ) const
		{
			return m_nParentBone == nNode || m_nChildBone == nNode;
		}

		void ChangeNode( int nOldNode, int nNewNode )
		{
			if ( m_nParentBone == nOldNode )
				m_nParentBone = nNewNode;
			if ( m_nChildBone == nOldNode )
				m_nChildBone = nNewNode;
		}
		void RebaseNodes( int nBaseNode )
		{
			m_nParentBone += nBaseNode;
			m_nChildBone += nBaseNode;
		}

		void RemapNodes( const CUtlVector< int > &remap )
		{
			RemapNode( m_nParentBone, remap );
			RemapNode( m_nChildBone, remap );
		}

		bool IsValid( int nNodeCount )const
		{
			return m_nParentBone < nNodeCount && m_nChildBone < nNodeCount;
		}

		bool operator == ( const CDagEdge &other ) const { return m_nParentBone == other.m_nParentBone && m_nChildBone == other.m_nChildBone; }

		int m_nParentBone;
		int m_nChildBone;
	};

	/*schema*/ class CCollisionSphere: public CDagEdge
	{
		//DECLARE_SCHEMA_DATA_CLASS( CAuthPhysFx::CCollisionSphere );
	public:
		CCollisionSphere( )
		{
			m_bInclusive = false;
			m_flRadius = 0;
			m_vOrigin = vec3_origin;
			m_flStickiness = 0;
		}

		void Assign( const AuthPhysSphereCollision_t &that )
		{
			m_bInclusive = that.m_bInclusive;
			m_flRadius = that.m_flRadius;
			m_vOrigin = that.m_vOrigin;
			m_flStickiness = that.m_flStickiness;
		}
	public:
		bool m_bInclusive;
		float m_flRadius;
		Vector m_vOrigin;
		float m_flStickiness;
	};

	/*schema*/ class CCollisionPlane: public CDagEdge
	{
		//DECLARE_SCHEMA_DATA_CLASS( CAuthPhysFx::CCollisionPlane );
	public:
		CCollisionPlane( )
		{
			m_Plane.m_vNormal = Vector( 0, 0, 1 );
			m_Plane.m_flOffset = 0;
			m_flStickiness = 0;
		}

		bool operator == ( const CCollisionPlane &other )const
		{
			return static_cast< const CDagEdge& >( *this ) == static_cast< const CDagEdge& >( other )
				&& m_Plane == other.m_Plane;

		}

		RnPlane_t m_Plane;
		float m_flStickiness;
	};

	/*schema*/ class CCtrlOffset : public CDagEdge
	{
		//DECLARE_SCHEMA_DATA_CLASS( CAuthPhysFx::CCtrlOffset );
	public:
		CCtrlOffset()
		{
			m_vOffset = Vector( 0, 8, 0 ); // zero offset isn't really useful
		}
		bool operator == ( const CCtrlOffset &other )const
		{
			return static_cast< const CDagEdge& >( *this ) == static_cast< const CDagEdge& >( other ) && m_vOffset == other.m_vOffset;
		}
		operator const FeCtrlOffset_t()const
		{
			FeCtrlOffset_t offset;
			offset.nCtrlChild = m_nChildBone;
			offset.nCtrlParent = m_nParentBone;
			offset.vOffset = m_vOffset;
			return offset;
		}

		Vector m_vOffset;
	};


	/*schema*/ class CBone
	{
		//DECLARE_SCHEMA_DATA_CLASS( CAuthPhysFx::CBone )
	public:
		CBone( )
		{
			m_nParent = -1;
			m_bVirtual = false;
			m_bNeedNodeBase = false;
			m_bHasMassOverride = false;
			m_bSimulated = true;
			m_bForceSimulated = false;
			m_bFreeRotation = true;
			m_bAnimRotation = false;
			m_bOsOffset = false;
			m_Transform = g_TransformIdentity;
			m_flMass = 1.0f;
			m_flMassBias = 0.0f;
			m_bNeedsWorldCollision = false;
			m_Integrator.Init( );
			m_nFollowParent = -1;
			m_flFollowWeight = 0;
			m_flWorldFriction = 0;
			m_flGroundFriction = 0;
			m_flLegacyStretchForce = 0;
			m_bUseRods = false;
			m_flCollisionRadius = 0;
			m_nCollisionMask = 0xFFFF;
			m_flLocalForce = 1.0f;
			m_flLocalRotation = 0.0f;
			m_flVolumetricSolveAmount = 0.0f;
		}

		CFeModelBuilder::BuildNode_t AsBuildNode()const;

		uint32 GetNameHash() { return MakeStringToken( m_Name.Get() ).GetHashCode(); }
		bool ApplyDotaFlags( const char *pFlags, CBoneParseParams *pParamsOut );
		bool ChangeNode( int nOldNode, int nNewNode )
		{
			bool bChanged = false;
			if ( m_nParent == nOldNode )
			{
				m_nParent = nNewNode;
				bChanged = true;
			}
			if ( m_nFollowParent == nOldNode )
			{
				m_nFollowParent = nNewNode;
				bChanged = true;
			}
			return bChanged;
		}
		CTransform GetOffsetTransform( const Vector &vOffset )
		{
			return CTransform( m_Transform.TransformVector( vOffset ), m_Transform.m_orientation );
		}
		void RebaseNodes( int nBaseNode )
		{
			if( m_nFollowParent >= 0 )
				m_nFollowParent += nBaseNode;
			if ( m_nParent >= 0 )
				m_nParent += nBaseNode;
		}
		void RemapNodes( const CUtlVector< int > &remap )
		{
			RemapNode( m_nFollowParent, remap );
			RemapNode( m_nParent, remap );
		}
	public:
		bool m_bSimulated;				// this is an fx particle comprising jiggle bones, chains, cloths, or any other soft body
		bool m_bForceSimulated;			// if true, this node may not be deduced to be non-simulated
		bool m_bFreeRotation;
		bool m_bAnimRotation;
		bool m_bVirtual;
		bool m_bNeedNodeBase;
		bool m_bNeedsWorldCollision;
		bool m_bOsOffset; // object-space offset, used for compatibility with S1 cloth
		float m_flWorldFriction; META( MPropertyFriendlyName = "Ground NOT-Collide" );// this is not a friction coefficient! this is the "WorldFriction" parameter from source1 cloth
		float m_flGroundFriction;
		float m_flLegacyStretchForce; // premultiplied by invMass here
		float m_flMass;
		float m_flMassBias;
		int m_nFollowParent;
		float m_flFollowWeight;
		float m_flCollisionRadius;
		FeNodeIntegrator_t m_Integrator; 
		bool m_bHasMassOverride;
		uint32 m_nCollisionMask;
		int m_nParent;			// parent bone index
		CUtlString m_Name;		// bone name, the same as in model, the same as used in animation
		CTransform m_Transform; META( MPropertySuppressField );// bind pose, relaxed pose
		bool m_bUseRods; // UI convenience: when checked, affects generation of quads or rods around this node

		float m_flLocalForce;
		float m_flLocalRotation;
		float m_flVolumetricSolveAmount;
	};


	/*schema*/ class CConstraint
	{
		//DECLARE_SCHEMA_DATA_CLASS( CAuthPhysFx::CConstraint )
	public:
		CConstraint( int nBone0 = -1, int nBone1 = -1 ) { m_nBones[0] = nBone0; m_nBones[1] = nBone1; }

		bool Equals( int nBone0, int nBone1 ) const { return ( m_nBones[0] == nBone0 && m_nBones[1] == nBone1 ) || ( m_nBones[1] == nBone0 && m_nBones[0] == nBone1 ); }

		int m_nBones[2]; // bone indices of the connected particles

	public:
		bool UsesNode( int nNode ) const
		{
			return m_nBones[ 0 ] == nNode || m_nBones[ 1 ] == nNode;
		}
		void ChangeNode( int nOldNode, int nNewNode )
		{
			if ( m_nBones[ 0 ] == nOldNode )
				m_nBones[ 0 ] = nNewNode;
			if ( m_nBones[ 1 ] == nOldNode )
				m_nBones[ 1 ] = nNewNode;
		}					
		void RebaseNodes( int nBaseNode )
		{
			m_nBones[ 0 ] += nBaseNode;
			m_nBones[ 1 ] += nBaseNode;
		}
		void RemapNodes( const CUtlVector< int > &remap )
		{
			RemapNode( m_nBones[ 0 ], remap );
			RemapNode( m_nBones[ 1 ], remap );
		}
	};

	/*schema*/ class CCapsule
	{
		//DECLARE_SCHEMA_DATA_CLASS( CAuthPhysFx::CCapsule )
	public:
		int m_nBone[2];
		Vector m_vCenter[2]; // each capsule center in the corresponding bone's local coordinates
		float m_flRadius;
	public:
		bool UsesNode( int nNode ) const
		{
			return m_nBone[ 0 ] == nNode || m_nBone[ 1 ] == nNode;
		}
		void ChangeNode( int nOldNode, int nNewNode )
		{
			if ( m_nBone[ 0 ] == nOldNode )
				m_nBone[ 0 ] = nNewNode;
			if ( m_nBone[ 1 ] == nOldNode )
				m_nBone[ 1 ] = nNewNode;
		}
		void RebaseNodes( int nBaseNode )
		{
			m_nBone[ 0 ] += nBaseNode;
			m_nBone[ 1 ] += nBaseNode;
		}
		void RemapNodes( const CUtlVector< int > &remap )
		{
			RemapNode( m_nBone[ 0 ], remap );
			RemapNode( m_nBone[ 1 ], remap );
		}
	};

	/*schema*/ class CQuad
	{
		//DECLARE_SCHEMA_DATA_CLASS( CAuthPhysFx::CQuad )
	public:
		uint m_nNodes[ 4 ];
		bool m_bUseRods;
		CQuad() { m_bUseRods = false; }

		CQuad( uint nNode0, uint nNode1, uint nNode2, uint nNode3 )
		{
			m_nNodes[ 0 ] = nNode0;
			m_nNodes[ 1 ] = nNode1;
			m_nNodes[ 2 ] = nNode2;
			m_nNodes[ 3 ] = nNode3;
			m_bUseRods = false;
		}

		CFeModelBuilder::BuildElem_t AsBuildElem()const;

		bool operator == ( const CQuad &right ) const 
		{
			return m_nNodes[ 0 ] == right.m_nNodes[ 0 ] && m_nNodes[ 1 ] == right.m_nNodes[ 1 ] && m_nNodes[ 2 ] == right.m_nNodes[ 2 ] && m_nNodes[ 3 ] == right.m_nNodes[ 3 ];
		}
		bool UsesNode( uint nNode ) const
		{
			return m_nNodes[ 0 ] == nNode || m_nNodes[ 1 ] == nNode || m_nNodes[ 2 ] == nNode || m_nNodes[ 3 ] == nNode; 
		}
		template <typename TBitVec >
		bool UsesAnyNode( const TBitVec &nodes )
		{
			return nodes[ m_nNodes[ 0 ] ] || nodes[ m_nNodes[ 1 ] ] || nodes[ m_nNodes[ 2 ] ] || nodes[ m_nNodes[ 3 ] ] ;
		}
		void ChangeNode( uint nOldNode, uint nNewNode )
		{
			if ( m_nNodes[ 0 ] == nOldNode )
				m_nNodes[ 0 ] = nNewNode;
			if ( m_nNodes[ 1 ] == nOldNode )
				m_nNodes[ 1 ] = nNewNode;
			if ( m_nNodes[ 2 ] == nOldNode )
				m_nNodes[ 2 ] = nNewNode;
			if ( m_nNodes[ 3 ] == nOldNode )
				m_nNodes[ 3 ] = nNewNode;
		}
		bool IsValid( uint nNodeCount )const
		{
			return m_nNodes[ 0 ] < nNodeCount && m_nNodes[ 1 ] < nNodeCount && m_nNodes[ 2 ] < nNodeCount && m_nNodes[ 3 ] < nNodeCount;
		}
		void RebaseNodes( int nBaseNode )
		{
			m_nNodes[ 0 ] += nBaseNode;
			m_nNodes[ 1 ] += nBaseNode;
			m_nNodes[ 2 ] += nBaseNode;
			m_nNodes[ 3 ] += nBaseNode;
		}
		void RemapNodes( const CUtlVector< int > &remap )
		{
			RemapNode( m_nNodes[ 0 ], remap );
			RemapNode( m_nNodes[ 1 ], remap );
			RemapNode( m_nNodes[ 2 ], remap );
			RemapNode( m_nNodes[ 3 ], remap );
		}
	};

	/*schema*/ class CRod
	{
		//DECLARE_SCHEMA_DATA_CLASS( CAuthPhysFx::CRod )
	public:
		uint m_nNodes[ 2 ];
		float32 m_flMotionBias[ 2 ]; // 
		float32 m_flRelaxationFactor;

		bool m_bExplicitLength;
		float32 m_flLength;
		float32 m_flContractionFactor;

		CRod( ) 
		{
			m_bExplicitLength = false;
			m_flContractionFactor = 0.05f; // in Source1, rods may contract to 0
			m_flLength = 0;
			m_nNodes[ 0 ] = m_nNodes[ 1 ] = 0; 
			m_flRelaxationFactor = 1.0f; 
			m_flMotionBias[ 0 ] = m_flMotionBias[ 1 ] = 1.0f;
		}
		CRod( uint nNode0, uint nNode1, float flRelaxationFactor = 1.0f )
		{ 
			m_nNodes[ 0 ] = nNode0;
			m_nNodes[ 1 ] = nNode1;
			m_flRelaxationFactor = flRelaxationFactor;
			m_flMotionBias[ 0 ] = m_flMotionBias[ 1 ] = 1.0f;
		}
		bool operator < ( const CRod &other )const
		{
			return m_nNodes[ 0 ] == other.m_nNodes[ 0 ] ? m_nNodes[ 1 ] < other.m_nNodes[ 1 ] : m_nNodes[ 0 ] < other.m_nNodes[ 0 ];
		}

		bool operator == ( const CRod &other )const
		{
			return ( m_nNodes[ 0 ] == other.m_nNodes[ 0 ] && m_nNodes[ 1 ] == other.m_nNodes[ 1 ] ) || ( m_nNodes[ 0 ] == other.m_nNodes[ 1 ] && m_nNodes[ 1 ] == other.m_nNodes[ 0 ] );
		}
		bool Equals( uint nNode0, uint nNode1 )const
		{
			return ( m_nNodes[ 0 ] == nNode0 && m_nNodes[ 1 ] == nNode1 ) || ( m_nNodes[ 1 ] == nNode0 && m_nNodes[ 0 ] == nNode1 );
		}
		bool UsesNode( uint nNode ) const
		{
			return m_nNodes[ 0 ] == nNode || m_nNodes[ 1 ] == nNode; 
		}
		void ChangeNode( uint nOldNode, uint nNewNode )
		{
			if ( m_nNodes[ 0 ] == nOldNode )
				m_nNodes[ 0 ] = nNewNode;
			if ( m_nNodes[ 1 ] == nOldNode )
				m_nNodes[ 1 ] = nNewNode;
		}
		void RebaseNodes( int nBaseNode )
		{
			m_nNodes[ 0 ] += nBaseNode;
			m_nNodes[ 1 ] += nBaseNode;
		}
		void RemapNodes( const CUtlVector< int > &remap )
		{
			RemapNode( m_nNodes[ 0 ], remap );
			RemapNode( m_nNodes[ 1 ], remap );
		}
		void Reverse()
		{
			::Swap( m_nNodes[ 0 ], m_nNodes[ 1 ] );
			::Swap( m_flMotionBias[ 0 ], m_flMotionBias[ 1 ] );
		}
		uint GetOtherNode( uint nNode )const
		{
			if ( m_nNodes[ 1 ] == nNode )
			{
				return m_nNodes[ 0 ];
			}
			else
			{
				Assert( m_nNodes[ 0 ] == nNode );
				return m_nNodes[ 1 ];
			}
		}
		bool IsValid( uint nNodeCount )const
		{
			return m_nNodes[ 0 ] < nNodeCount && m_nNodes[ 1 ] < nNodeCount;
		}

	};


	/*schema*/ class CSpring
	{
	public:
		//DECLARE_SCHEMA_DATA_CLASS( CAuthPhysFx::CSpring )
		uint m_nNodes[ 2 ];
		float32 m_flSpringConstant;
		float32 m_flSpringDamping;
		float32 m_flStretchiness; // Not Implemented
		CSpring( ){}
		CSpring( uint nNode0, uint nNode1, float flSpringConstant, float flSpringDamping, float flStretchForce, float flStretchiness )
		{
			m_nNodes[ 0 ] = nNode0;
			m_nNodes[ 1 ] = nNode1;
			m_flSpringConstant = flSpringConstant + 60 * flStretchForce * flStretchiness; // I think this may be approximately how it effectively works in Source1;;
			m_flSpringDamping = flSpringDamping;
			m_flStretchiness   = flStretchiness; // Note: so far this is not used, saving mainly for documentation
		}
		bool operator < ( const CRod &other )
		{
			return m_nNodes[ 0 ] == other.m_nNodes[ 0 ] ? m_nNodes[ 1 ] < other.m_nNodes[ 1 ] : m_nNodes[ 0 ] < other.m_nNodes[ 0 ];
		}
		bool UsesNode( uint nNode ) const { return m_nNodes[ 0 ] == nNode || m_nNodes[ 1 ] == nNode; }
		void ChangeNode( uint nOldNode, uint nNewNode )
		{
			if ( m_nNodes[ 0 ] == nOldNode )
				m_nNodes[ 0 ] = nNewNode;
			if ( m_nNodes[ 1 ] == nOldNode )
				m_nNodes[ 1 ] = nNewNode;
		}
		void RebaseNodes( int nBaseNode )
		{
			m_nNodes[ 0 ] += nBaseNode;
			m_nNodes[ 1 ] += nBaseNode;
		}
		void RemapNodes( const CUtlVector< int > &remap )
		{
			RemapNode( m_nNodes[ 0 ], remap );
			RemapNode( m_nNodes[ 1 ], remap );
		}
	};

public:
	bool IsEmpty() const { return m_Quads.IsEmpty() && m_Rods.IsEmpty() && m_Nodes.IsEmpty() && m_CollisionSpheres.IsEmpty() && m_CollisionPlanes.IsEmpty() && m_SphereRigids.IsEmpty() && m_TaperedCapsuleStretches.IsEmpty() && m_TaperedCapsuleRigids.IsEmpty() && m_Capsules.IsEmpty() && m_Constraints.IsEmpty() && m_Springs.IsEmpty(); }
	bool ImportDotaCloth( const char *pClothFile, CPhysModelSource &physicsModel/*CUtlStringMap< int, CUtlSymbolTable > *pBoneToIndex*/ );
	void Load( const CFeModel *pFeModel );
	void AddRod( const CUtlVector< CBone > &nodes, uint nNode0, uint nNode1, float flRelaxationFactor );

	void SetBones( const CUtlVector< CBone > &bones );
	int AddConstraint( int nBone0, int nBone1 );
	const CBone *GetBone( int nBone ) const { return &m_Nodes[nBone]; }
	CBone *GetBone( int nBone ) { return &m_Nodes[nBone]; }
	int FindNodeIndex( const char *pName );
	CBone *GetOrCreateBone( const char *pName );
	int GetBoneCount() const { return m_Nodes.Count(); }
	int GetSimParticleCount()const;
	int GetStaticParticleCount( )const { return m_Nodes.Count( ) - GetSimParticleCount( ); }
	CLockedResource< PhysFeModelDesc_t > Compile( CResourceStream *pStream, const CVClothProxyMeshOptions *pOptions = NULL )const;
	bool IsSimilarTo( const CFeModel *pFeModel )const;
	bool IsConstraintSimulated( int nConstraint ) const ;
	bool IsSpringSimulated( int nSpring ) const;

	int GetRodCount( ) const { return m_Rods.Count( ); }
	CRod *GetRod( int i ) { return &m_Rods[ i ]; }
	float GetRodLength( int nRod )const;
	int GetQuadCount( ) const { return m_Quads.Count( ); }
	CQuad *GetQuad( int i ) { return &m_Quads[ i ]; }

	int GetConstraintCount() const { return m_Constraints.Count(); }
	CConstraint* GetConstraint( int i ) { return &m_Constraints[i]; }

	int GetSpringCount( ) const { return m_Springs.Count( ); }
	CSpring* GetSpring( int i ) { return &m_Springs[ i ]; }

	int GetCapsuleCount() const { return m_Capsules.Count(); }
	CCapsule* GetCapsule( int i ) { return &m_Capsules[i]; }

	bool IsNewSpringAllowed( int nBone0, int nBone1 );
	bool IsNewRodAllowed( int nBone0, int nBone1 );

	void SortAndRemoveDuplicates( );
	void RemoveRodsConnecting( const CVarBitVec &nodes );
	void RemoveQuadsConnecting( const CVarBitVec &nodes );

	int BuildIslandMap( CUtlVector< int > &nodeToIsland )const; 

	void Swap( CAuthPhysFx &other )
	{
		m_Quads.Swap( other.m_Quads );
		m_Rods.Swap( other.m_Rods );
		m_Springs.Swap( other.m_Springs );
		m_Constraints.Swap( other.m_Constraints );    // springs and constraints between fx particles
		m_Capsules.Swap( other.m_Capsules );			// capsules repelling the particles
		m_Nodes.Swap( other.m_Nodes );				// bones, including dynamic particles
		m_TaperedCapsuleRigids.Swap( other.m_TaperedCapsuleRigids );
		m_TaperedCapsuleStretches.Swap( other.m_TaperedCapsuleStretches );
		m_SphereRigids.Swap( other.m_SphereRigids );
		m_CollisionSpheres.Swap( other.m_CollisionSpheres );
		m_CollisionPlanes.Swap( other.m_CollisionPlanes );
		m_PresetNodeBases.Swap( other.m_PresetNodeBases );
		m_FitInfluences.Swap( other.m_FitInfluences );

		::Swap( m_flDefaultSurfaceStretch, other.m_flDefaultSurfaceStretch );
		::Swap( m_flDefaultThreadStretch, other.m_flDefaultThreadStretch );

		::Swap( m_flDefaultGravityScale, other.m_flDefaultGravityScale );
		::Swap( m_flDefaultVelAirDrag, other.m_flDefaultVelAirDrag );
		::Swap( m_flDefaultExpAirDrag, other.m_flDefaultExpAirDrag );
		::Swap( m_flDefaultVelQuadAirDrag, other.m_flDefaultVelQuadAirDrag );
		::Swap( m_flDefaultExpQuadAirDrag, other.m_flDefaultExpQuadAirDrag );
		::Swap( m_flDefaultVelRodAirDrag, other.m_flDefaultVelRodAirDrag );
		::Swap( m_flDefaultExpRodAirDrag, other.m_flDefaultExpRodAirDrag );
		::Swap( m_flQuadVelocitySmoothRate, other.m_flQuadVelocitySmoothRate );
		::Swap( m_flRodVelocitySmoothRate, other.m_flRodVelocitySmoothRate );
		::Swap( m_flWindage, other.m_flWindage );
		::Swap( m_flWindDrag, other.m_flWindDrag );
		::Swap( m_nQuadVelocitySmoothIterations, other.m_nQuadVelocitySmoothIterations );
		::Swap( m_nRodVelocitySmoothIterations, other.m_nRodVelocitySmoothIterations );
		::Swap( m_flDefaultGroundFriction, other.m_flDefaultGroundFriction );
		::Swap( m_flDefaultWorldCollisionPenetration, other.m_flDefaultWorldCollisionPenetration );
		::Swap( m_bForceWorldCollisionOnAllNodes, other.m_bForceWorldCollisionOnAllNodes );
		::Swap( m_flAddWorldCollisionRadius, other.m_flAddWorldCollisionRadius );

		::Swap( m_flAddCurvature, other.m_flAddCurvature );
		::Swap( m_flQuadBendTolerance, other.m_flQuadBendTolerance );
		::Swap( m_flLocalForce, other.m_flLocalForce );
		::Swap( m_flLocalRotation, other.m_flLocalRotation );
		::Swap( m_flVolumetricSolveAmount, other.m_flVolumetricSolveAmount );
		::Swap( m_bFollowTheLead, other.m_bFollowTheLead );
		::Swap( m_bUsePerNodeLocalForceAndRotation, other.m_bUsePerNodeLocalForceAndRotation );

		::Swap( m_bUninertialRods, other.m_bUninertialRods );
		::Swap( m_bExplicitMasses, other.m_bExplicitMasses );
		::Swap( m_bCanCollideWithWorldCapsulesAndSpheres, other.m_bCanCollideWithWorldCapsulesAndSpheres );
		::Swap( m_bCanCollideWithWorldHulls, other.m_bCanCollideWithWorldHulls );
		::Swap( m_bCanCollideWithWorldMeshes, other.m_bCanCollideWithWorldMeshes );
		::Swap( m_bUnitlessDamping, other.m_bUnitlessDamping );
		::Swap( m_nMergePriority, other.m_nMergePriority );
		::Swap( m_bNewStyle, other.m_bNewStyle );
		::Swap( m_bAddStiffnessRods, other.m_bAddStiffnessRods );
		::Swap( m_bRigidEdgeHinges, other.m_bRigidEdgeHinges );
	}

	template <typename T >
	static void AppendAndRebaseNodes( CUtlVector< T > &arrThis, const CUtlVector< T > &arrThat, int nNodeBase )
	{
		int nScan = arrThis.Count();
		arrThis.AddMultipleToTail( arrThat.Count(), arrThat.Base() );
		for ( ; nScan < arrThis.Count(); ++nScan )
		{
			arrThis[ nScan ].RebaseNodes( nNodeBase );
		}
	}
	template <typename T >
	static void AppendAndRemapNodes( CUtlVector< T > &arrThis, const CUtlVector< T > &arrThat, const CUtlVector< int > &remap )
	{
		int nScan = arrThis.Count();
		arrThis.AddMultipleToTail( arrThat.Count(), arrThat.Base() );
		for ( ; nScan < arrThis.Count(); ++nScan )
		{
			arrThis[ nScan ].RemapNodes( remap );
		}
	}


	void Append( const CAuthPhysFx &other )
	{
		int nBaseNodes = m_Nodes.Count();

		if ( m_nMergePriority < other.m_nMergePriority )
		{
			m_flDefaultSurfaceStretch = other.m_flDefaultSurfaceStretch;
			m_flDefaultThreadStretch = other.m_flDefaultThreadStretch;

			m_flDefaultGravityScale = other.m_flDefaultGravityScale;
			m_flDefaultVelAirDrag = other.m_flDefaultVelAirDrag;
			m_flDefaultExpAirDrag = other.m_flDefaultExpAirDrag;
			m_flDefaultVelQuadAirDrag = other.m_flDefaultVelQuadAirDrag;
			m_flDefaultExpQuadAirDrag = other.m_flDefaultExpQuadAirDrag;
			m_flDefaultVelRodAirDrag = other.m_flDefaultVelRodAirDrag;
			m_flDefaultExpRodAirDrag = other.m_flDefaultExpRodAirDrag;
			m_flQuadVelocitySmoothRate = other.m_flQuadVelocitySmoothRate;
			m_flRodVelocitySmoothRate = other.m_flRodVelocitySmoothRate;
			m_flWindage = other.m_flWindage;
			m_flWindDrag = other.m_flWindage;

			m_nQuadVelocitySmoothIterations = other.m_nQuadVelocitySmoothIterations;
			m_nRodVelocitySmoothIterations = other.m_nRodVelocitySmoothIterations;
			m_flDefaultGroundFriction = other.m_flDefaultGroundFriction;
			m_flDefaultWorldCollisionPenetration = other.m_flDefaultWorldCollisionPenetration;
			m_bForceWorldCollisionOnAllNodes = other.m_bForceWorldCollisionOnAllNodes;
			m_flAddWorldCollisionRadius = other.m_flAddWorldCollisionRadius;

			m_flAddCurvature = other.m_flAddCurvature;
			m_flQuadBendTolerance = other.m_flQuadBendTolerance;
			m_flLocalForce = other.m_flLocalForce;
			m_flLocalRotation = other.m_flLocalRotation;
			m_flVolumetricSolveAmount = other.m_flVolumetricSolveAmount;
			m_bFollowTheLead = other.m_bFollowTheLead;
			m_bUsePerNodeLocalForceAndRotation = other.m_bUsePerNodeLocalForceAndRotation;
			m_bUninertialRods = other.m_bUninertialRods;
			m_bExplicitMasses = other.m_bExplicitMasses;
			m_bCanCollideWithWorldHulls = other.m_bCanCollideWithWorldHulls;
			m_bCanCollideWithWorldMeshes = other.m_bCanCollideWithWorldMeshes;
			m_bCanCollideWithWorldCapsulesAndSpheres = other.m_bCanCollideWithWorldCapsulesAndSpheres;
			m_bUnitlessDamping = other.m_bUnitlessDamping;
			m_nMergePriority = other.m_nMergePriority;
			m_bNewStyle = other.m_bNewStyle;
			m_bAddStiffnessRods = other.m_bAddStiffnessRods;
			m_bRigidEdgeHinges = other.m_bRigidEdgeHinges;
		}

		CUtlVector< int > remap;
		remap.SetCount( other.m_Nodes.Count() );
		// merge the Nodes
		m_Nodes.EnsureCapacity( m_Nodes.Count() + other.m_Nodes.Count() );
		for ( int nOtherNode = 0; nOtherNode < other.m_Nodes.Count(); ++nOtherNode )
		{
			const CBone &otherNode = other.m_Nodes[ nOtherNode ];
			int nMapNode = -1;
			if ( !otherNode.m_Name.IsEmpty() )
			{
				nMapNode = FindNodeIndex( otherNode.m_Name );
			}
			if ( nMapNode < 0 )
			{
				// couldn't find node to map, create a new one
				remap[ nOtherNode ] = m_Nodes.AddToTail( otherNode );
			}
			else
			{
				remap[ nOtherNode ] = nMapNode;
			}
		}
		// the remap array is ready; use it to remap all the node indices
		for ( int nNewNode = nBaseNodes; nNewNode < m_Nodes.Count(); ++nNewNode )
		{
			m_Nodes[ nNewNode ].RemapNodes( remap );
		}
		AppendAndRemapNodes( m_Quads, other.m_Quads, remap );
		AppendAndRemapNodes( m_Rods, other.m_Rods, remap );
		AppendAndRemapNodes( m_Springs, other.m_Springs, remap );
		AppendAndRemapNodes( m_Constraints, other.m_Constraints, remap );    // springs and constraints between fx particles
		AppendAndRemapNodes( m_Capsules, other.m_Capsules, remap );			// capsules repelling the particles
		AppendAndRemapNodes( m_TaperedCapsuleRigids, other.m_TaperedCapsuleRigids, remap );
		AppendAndRemapNodes( m_TaperedCapsuleStretches, other.m_TaperedCapsuleStretches, remap );
		AppendAndRemapNodes( m_SphereRigids, other.m_SphereRigids, remap );
		AppendAndRemapNodes( m_CollisionSpheres, other.m_CollisionSpheres, remap );
		AppendAndRemapNodes( m_CollisionPlanes, other.m_CollisionPlanes, remap );
		AppendAndRemapNodes( m_PresetNodeBases, other.m_PresetNodeBases, remap );
		AppendAndRemapNodes( m_FitInfluences, other.m_FitInfluences, remap );
	}


	void Purge( )
	{
		m_Quads.Purge();
		m_Rods.Purge();
		m_Springs.Purge();
		m_Constraints.Purge();    // springs and constraints between fx particles
		m_Capsules.Purge();			// capsules repelling the particles
		m_Nodes.Purge();				// bones, including dynamic particles
		m_TaperedCapsuleRigids.Purge();
		m_TaperedCapsuleStretches.Purge();
		m_SphereRigids.Purge();
		m_CollisionSpheres.Purge();
		m_CollisionPlanes.Purge();
		m_PresetNodeBases.Purge();
		m_FitInfluences.Purge();
		InitDefaults();
	}

	void InitDefaults()
	{
		m_flDefaultSurfaceStretch = 0;
		m_flDefaultThreadStretch = 0;
		m_flLocalForce = 1.0f;
		m_flLocalRotation = 0.0f;
		m_flVolumetricSolveAmount = 0.0f;
		m_bUninertialRods = false;
		m_bExplicitMasses = false;
		m_bCanCollideWithWorldHulls = false;
		m_bCanCollideWithWorldMeshes = false;
		m_bCanCollideWithWorldCapsulesAndSpheres = false;
		m_bUnitlessDamping = true;
		m_bFollowTheLead = false;
		m_bUsePerNodeLocalForceAndRotation = false;
		m_flAddCurvature = 0;
		m_flQuadBendTolerance = 0.05f;

		m_flDefaultGravityScale = 1.0f;
		m_flDefaultVelAirDrag = 0;
		m_flDefaultExpAirDrag = 0;
		m_flDefaultVelQuadAirDrag = 0;
		m_flDefaultExpQuadAirDrag = 0;
		m_flDefaultVelRodAirDrag = 0;
		m_flDefaultExpRodAirDrag = 0;
		m_flQuadVelocitySmoothRate = 0;
		m_flRodVelocitySmoothRate = 0;
		m_flWindage = 1.0f;
		m_flWindDrag = 0;

		m_nQuadVelocitySmoothIterations = 0;
		m_nRodVelocitySmoothIterations = 0;
		m_bForceWorldCollisionOnAllNodes = false;
		m_flDefaultGroundFriction = 0;
		m_flDefaultWorldCollisionPenetration = 0;
		m_flAddWorldCollisionRadius = 2.0f;

		m_nMergePriority = 0;
		m_bNewStyle = true;
		m_bAddStiffnessRods = true;
		m_bRigidEdgeHinges = false;
	}

	void CreateRodMap( CAuthPhysFxRodMap &rodMap )
	{
		rodMap.Init( GetBoneCount() );
		for ( int nRod = 0; nRod < m_Rods.Count(); ++nRod )
		{
			rodMap.Append( m_Rods[ nRod ].m_nNodes[ 0 ], nRod );
			rodMap.Append( m_Rods[ nRod ].m_nNodes[ 1 ], nRod );
		}
	}

	void CreateConnMap( CAuthPhysFxRodMap &connMap )
	{
		connMap.Init( GetBoneCount() );
		for ( int nRod = 0; nRod < m_Rods.Count(); ++nRod )
		{
			const uint *n = m_Rods[ nRod ].m_nNodes;
			//if ( filter( n[ 0 ] ) && filter( n[ 1 ] ) )
			{
				connMap.Append( n[ 0 ], n[ 1 ] );
				connMap.Append( n[ 1 ], n[ 0 ] );
			}
		}
		for ( const CQuad &quad : m_Quads )
		{
			for ( int i = 0; i < 3; ++i )
			{
				uint n0 = quad.m_nNodes[ i ];
				for ( int j = i + 1; j < 4; ++j )
				{
					uint n1 = quad.m_nNodes[ j ];
					if ( n0 != n1 )
					{
						connMap.Append( n0, n1 );
						connMap.Append( n1, n0 );
					}
				}
			}
		}
	}


	CBone *GetDriverBone( int fxBone )
	{
		CAuthPhysFx::CBone *pFxBone;
		for ( ;; )
		{
			AssertDbg( fxBone >= 0 && fxBone < GetBoneCount() );
			pFxBone = GetBone( fxBone );
			
			// bones that move by themselves are driving themselves
			if ( pFxBone->m_bSimulated )
				break;
			if ( pFxBone->m_bFreeRotation )
				break;

			// try to find the true parent that controls all movements of this bone
			if ( pFxBone->m_nParent >= 0 )
			{
				fxBone = pFxBone->m_nParent;
			}
			else if ( pFxBone->m_nFollowParent >= 0 && pFxBone->m_flFollowWeight >= 1.0f )
			{
				fxBone = pFxBone->m_nFollowParent;
			}
			else
				break; // didn't find true parent
		}
		return pFxBone;
	}


public:
	CUtlVector< CQuad > m_Quads;
	CUtlVector< CRod > m_Rods;
	CUtlVector< CSpring > m_Springs;
	CUtlVector< CConstraint > m_Constraints;                        // springs and constraints between fx particles
	CUtlVector< CCapsule > m_Capsules;			                    // capsules repelling the particles
	CUtlVector< CBone > m_Nodes;				                    // bones, including dynamic particles
	CUtlVector< CCollisionSphere > m_CollisionSpheres;
	CUtlVector< CCollisionPlane > m_CollisionPlanes;
	CUtlVector< FeNodeBase_t > m_PresetNodeBases;
	CUtlVector< FeTaperedCapsuleStretch_t > m_TaperedCapsuleStretches;
	CUtlVector< FeTaperedCapsuleRigid_t > m_TaperedCapsuleRigids;
	CUtlVector< FeSphereRigid_t > m_SphereRigids;
	CUtlVector< CCtrlOffset > m_CtrlOffsets;
	CUtlVector< FeFitInfluence_t > m_FitInfluences;
	float m_flDefaultSurfaceStretch;
	float m_flDefaultThreadStretch;

	float m_flDefaultGravityScale;
	float m_flDefaultVelAirDrag;
	float m_flDefaultExpAirDrag;
	float m_flDefaultVelQuadAirDrag;
	float m_flDefaultExpQuadAirDrag;
	float m_flDefaultVelRodAirDrag;
	float m_flDefaultExpRodAirDrag;
	float m_flQuadVelocitySmoothRate;
	float m_flRodVelocitySmoothRate;
	float m_flWindage;
	float m_flWindDrag;
	int m_nQuadVelocitySmoothIterations;
	int m_nRodVelocitySmoothIterations;
	float m_flDefaultGroundFriction;
	float m_flDefaultWorldCollisionPenetration; META( MPropertyFriendlyName = "World Velocity Penetration (Source1) 0.0 = velocity goes to 0 on contact" );
	float m_flAddWorldCollisionRadius;

	float m_flLocalForce;
	float m_flLocalRotation;
	float m_flVolumetricSolveAmount;
	float m_flAddCurvature;
	float m_flQuadBendTolerance; META( MPropertyFriendlyName = "Triangulate Quads bent more than" );
	bool m_bFollowTheLead;
	bool m_bUsePerNodeLocalForceAndRotation;
	bool m_bUninertialRods;
	bool m_bExplicitMasses;
	bool m_bUnitlessDamping;
	bool m_bForceWorldCollisionOnAllNodes; // convenience function: just slam all nodes' world collision flag, useful for quick testing
	int m_nMergePriority;
	bool m_bNewStyle;

	bool m_bCanCollideWithWorldMeshes;
	bool m_bCanCollideWithWorldCapsulesAndSpheres;
	bool m_bCanCollideWithWorldHulls;

	bool m_bAddStiffnessRods;
	bool m_bRigidEdgeHinges;
};




// typedef CAuthPhysFx::CQuad CAuthPhysFxQuad;
// typedef CAuthPhysFx::CRod  CAuthPhysFxRod;
// typedef CAuthPhysFx::CSpring  CAuthPhysFxSpring ;
// typedef CAuthPhysFx::CConstraint  CAuthPhysFxConstraint ;
// typedef CAuthPhysFx::CCapsule  CAuthPhysFxCapsule ;
// typedef CAuthPhysFx::CBone  CAuthPhysFxBone ;
// typedef CAuthPhysFx::CCollisionSphere  CAuthPhysFxCollisionSphere ;
// typedef CAuthPhysFx::CCollisionPlane  CAuthPhysFxCollisionPlane ;


class CBoneParseParams
{
public:
	CBoneParseParams( KeyValues *pSubKey, int nCompatibilityMode );
	typedef CAuthPhysFx::CBone CBone;
	void ApplyDefaultParams( CBone &node );
	bool ApplyDotaFlags( CAuthPhysFx::CBone &bone, const char *pszParms );
public:
	float m_flWorldFriction;
	float m_flAnimationForceAttraction;
	float m_flAnimationVertexAttraction;
	float m_flDamping;
	float m_flFixedPointDamping;
	float m_flSpringStretchiness;
	float m_flRelaxationFactor;
	float m_flStretchForce;
	float m_flStructSpringConstant;
	float m_flStructSpringDamping;
	float m_flGravityScale;
public:
	bool m_bPrevColumnParent;
	Vector m_vOffset; // offset relative to the animation transform

	const char *m_pBonePrefix;
	int m_nNominalColumnCount;
	int m_nRowCount;
	int m_nVirtColumnCount;

	float m_flFollowRootBegin;
	float m_flFollowRootEnd;
	bool m_bIsRopeS1;
	int m_nBoneIndex;

	typedef CAuthPhysFx::CCollisionPlane CCollisionPlane;
	typedef CAuthPhysFx::CCollisionSphere CCollisionSphere;
	CUtlVector< CCollisionSphere > *m_pCollisionSpheres;
	CUtlVector< CCollisionPlane > *m_pCollisionPlanes;
};


class CNodeIdx
{
public:
	CNodeIdx( uint nBase, uint nRows, uint nColumns ): m_nBase( nBase ), m_nRows( nRows ), m_nColumns( nColumns ) {}
	uint operator() ( uint nRow, uint nColumn )const
	{
		AssertDbg( nRow < m_nRows && nColumn < m_nColumns );
		return m_nBase + nRow * m_nColumns + nColumn;
	}
protected:
	uint m_nBase;
	uint m_nRows;
	uint m_nColumns;
};



class CAuthClothParser: public CAuthPhysFx
{
public:
	void SetBones( CPhysModelSource &physicsModel );
	bool Parse( KeyValues *kv );

	bool ParseDefaults( KeyValues *pSubkey );
	bool ParsePiece( KeyValues *pSubkey );
	void ParseExplicitDefinitions( KeyValues *pSubkey, CBoneParseParams &parseParm );
	bool ParseLegacyDotaNodeGrid( KeyValues *pSubKey, CBoneParseParams &parseParms, const CNodeIdx &nodeIdx );
	bool CreateLegacyDotaRodGrid( CBoneParseParams &parseParms, const CNodeIdx &nodeIdx, int );
	bool TieModelToNewNode( int nNewNode );

	void ReconstructHierarchy( );

	void ParseExplicitNode( KeyValues *kv, CBoneParseParams &parseParm );
	void ParseExplicitElem( KeyValues *kv, CBoneParseParams &parseParm );
	template <typename T>
	void ParseExplicitColl( KeyValues *kv, CBoneParseParams &parseParm, CUtlVector< T > &collArray );

	int FindNodeByName( const char *pName );
	void AddDotaRod( uint nNode0, uint nNode1, CBoneParseParams &parseParm );
protected:
	int m_nDefaultCompatibilityMode;
	int m_nCompatibilityMode;

	CUtlStringMap< int > m_NodeNameMap;
	int m_nNodeNameMapNodes;

	CUtlStringMap< int > m_BoneToIndex;
	CUtlVector< int > m_BoneToParent;
	CUtlVector< int > m_ModelBoneToNode;
	CUtlVector< CTransform >m_BoneTransforms;
};




struct AuthHullSimplificationParams_t: public RnHullSimplificationParams_t
{
	float m_flMinSkinWeight;
	bool m_bIncludSubtree;

	AuthHullSimplificationParams_t( )
	{
		m_bIncludSubtree = false;
		m_flMinSkinWeight = 0.5f;
	}
};


template <typename TBitVec>
void CAuthPhysFx::AlignNodes( const TBitVec &useNodes )
{
	CUtlVectorAligned< CFeModelBuilder::BuildNode_t > nodes;
	nodes.SetCount( m_Nodes.Count() );
	for ( int nNode = 0; nNode < nodes.Count(); ++nNode )
	{
		nodes[ nNode ] = m_Nodes[ nNode ].AsBuildNode();
	}
	CUtlVector< CFeModelBuilder::BuildElem_t > elems;
	elems.EnsureCapacity( m_Quads.Count() );
	for ( int nQuad = 0; nQuad < m_Quads.Count(); ++nQuad )
	{
		if ( m_Quads[ nQuad ].UsesAnyNode( useNodes ) )
		{
			elems.AddToTail( m_Quads[ nQuad ].AsBuildElem() );
		}
	}
	if ( !elems.IsEmpty() )
	{
		CUtlVector < FeNodeBase_t > presets, bases;
		CUtlVectorOfPointers< CUtlSortVector< int > > neighbors;
		CFeModelBuilder::BuildNodeBases( nodes, elems, presets, bases, neighbors );
		for ( const FeNodeBase_t &base : bases )
		{
			if ( useNodes[ base.nNode ] )
			{
				CBone &bone = m_Nodes[ base.nNode ];
				bone.m_Transform.m_orientation = bone.m_Transform.m_orientation * Conjugate( base.qAdjust ); // qAdjust = ~predict * old_orient; so, to make qAdjust = idenity we need to make new_orient = predict = old_orient * ~(~predict * old_orient )
			}
		}
	}
}

#endif
