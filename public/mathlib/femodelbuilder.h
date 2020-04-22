//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
#ifndef FINITE_ELEMENT_MODEL_BUILDER_HDR
#define FINITE_ELEMENT_MODEL_BUILDER_HDR

#include "tier1/utlvector.h"
#include "tier1/utlhashtable.h"
#include "tier1/utlsortvector.h"
#include "mathlib/femodel.h"
#include "tier1/utlbufferstrider.h"


template <typename T>
class CUtlVectorOfPointers: public CUtlVector< T * >
{
public:
	~CUtlVectorOfPointers( )
	{
		Purge( );
	}
	template <typename Functor>
	void SetCountAndInit( int nCount, Functor fn )
	{
		CUtlVector< T * >::SetCount( nCount );
		for ( int i = 0; i < nCount; ++i )
		{
			CUtlVector< T * >::Element( i ) = fn( i );
		}
	}
	void Purge( )
	{
		for ( int i = 0; i < CUtlVector< T * >::Count( ); ++i )
		{
			delete ( *this )[ i ];
		}
		CUtlVector< T * >::Purge( );
	}
};



class CFeModelBuilder: public CFeModel, public CMultiBufferHelper< CFeModelBuilder >
{
public:
	struct BuildElem_t;

	CFeModelBuilder( )
	{
		V_memset( static_cast< CFeModel* >( this ), 0, sizeof( CFeModel ) );
		m_bIdentityCtrlOrder = false;
		m_bEnableExplicitNodeMasses = false;
		m_bUnitlessDamping = false;
		m_bAddStiffnessRods = true;
		m_bUsePerNodeLocalForceAndRotation = false;
		m_flQuadBendTolerance = 0.05f;
		m_bRigidEdgeHinges = false;
		m_nFitMatrixMinInfluences = 8;
		m_bNeedBacksolvedBasesOnly = false;
	}

	typedef CUtlVectorFixedGrowable< FeFitWeight_t, 8 > FitWeightArray_t;

	void EnableUnitlessDamping( bool bEnableUnitlessDamping ) { m_bUnitlessDamping = bEnableUnitlessDamping;  }
	void EnableIdentityCtrlOrder( ) { m_bIdentityCtrlOrder = true; }
	void EnableExplicitNodeMasses( bool bExplicit ) { m_bEnableExplicitNodeMasses = bExplicit; }
	void EnableRigidStiffnessRods( bool bRigidStiffnessRods ) { m_bRigidEdgeHinges = bRigidStiffnessRods; }
	void SetQuadBendTolerance( float flQuadBendTolerance ) { m_flQuadBendTolerance = flQuadBendTolerance; }

	bool Finish( bool bTriangulate, float flAddCurvature, float flAddSlack );
	void AdjustQuads();
	float ElemNormalLength( const uint nNode[4] );
	float NodeDist( uint nNode0, uint nNode1 );
	Vector TriNormal( uint nNode0, uint nNode1, uint nNode2 );
	void AddBendCurvature( float k )
	{
		for ( int i = 0; i < m_KelagerBends.Count( ); ++i )
		{
			FeKelagerBend_t &kbend = m_KelagerBends[ i ];
			float flMinSide = FLT_MAX;
			for ( int j = 0; j < 4; ++j )
			{
				uint n0 = kbend.m_nNode[ j ], n1 = kbend.m_nNode[ ( j + 1 ) % 4 ];
				if ( n1 != n0 )
				{
					float flSide = ( m_Nodes[ n0 ].transform.m_vPosition - m_Nodes[ n1 ].transform.m_vPosition ).Length( );
					if ( flSide < flMinSide )
						flMinSide = flSide;
				}
			}
			kbend.flHeight0 += flMinSide * k;
		}

		for ( int i = 0; i < m_AxialEdges.Count( ); ++i )
		{
			FeAxialEdgeBend_t &edge = m_AxialEdges[ i ];
			Vector f01 = m_Nodes[ edge.nNode[ 0 ] ].transform.m_vPosition * edge.te + m_Nodes[ edge.nNode[ 1 ] ].transform.m_vPosition * ( 1 - edge.te );
			float h = ( m_Nodes[ edge.nNode[ 2 ] ].transform.m_vPosition - f01 ).Length( ) + ( m_Nodes[ edge.nNode[ 3 ] ].transform.m_vPosition - f01 ).Length( );
			edge.flDist += h * k;
		}
	}
	
	struct MbaContext_t;

	int FindBuildNodeIndex( const char *pName );

	void BuildAxialEdges( );
	void BuildOldFeEdges( );
	void BuildKelagerBends( );
	void BuildAndSortRods( float flCurvatureAngle, bool bTriangulate );
	void BuildRod( float flCurvatureAngle, uint v0, uint v1, uint nElem0, uint nElem1, uint nEdge0, uint nEdge1, CUtlHashtable< uint32, uint32 > &edgeToRod );
	void BuildQuads( CUtlVector< FeQuad_t > &quads, bool bSkipTris );
	void BuildTris( CUtlVector< FeTri_t > &quads, bool bTriangulate );
	void BuildNodeSlack( float flSlackMultiplier );
	void BuildFeEdgeDesc( );
	void BuildInvMassesAndSortNodes( );
	int ReconcileElemStaticNodes();
	int RemoveFullyStaticElems();
	void BuildBaseRecovery( );
	void BuildRopes( );
	void BuildFreeNodes( MbaContext_t &context );
	void BuildCtrlOffsets( );
	void BuildNodeFollowers( CUtlVector< FeFollowNode_t > &nodeFollowers );
	uint BuildCollisionSpheres( CUtlVector< FeCollisionSphere_t > &collisionSpheres );
	void BuildCollisionPlanes( CUtlVector< FeCollisionPlane_t > &collisionPlanes );
	void BuildWorldCollisionNodes( CUtlVector< FeWorldCollisionParams_t > &worldCollisionParams, CUtlVector< uint16 > &worldCollisionNodes );
	void BuildFitMatrices( MbaContext_t &context );
	void RemoveStandaloneNodeBases( MbaContext_t &context );
	Vector ComputeCenter( const FitWeightArray_t &weights );
	void CheckIdentityCtrlOrder( );
	void BuildSprings( CUtlVector< FeSpringIntegrator_t > &springs );
	void ValidateBases( );
	void PrintNodeTree( uint nNode, const CUtlString &prefix );

	void CleanupElements();
	void RecomputeMasses( CUtlVector< float >& nodeMass );
	void BalanceGlobalMassMultipliers( CUtlVector< float >& nodeMass );

	int CountSimulatedNodesIn( const FeRodConstraint_t & rod );
	int CountSimulatedNodesIn( const BuildElem_t& elem );
	void BuildTree();

	FeTri_t BuildTri( const BuildElem_t &buildElem, int nTriStaticNodes, int nSubTri );
	uint GetDampingFlags( )const
	{
		uint nFlags = 0;
		for ( int i = 0; i < m_Nodes.Count( ); ++i )
		{
			nFlags |= m_Nodes[ i ].GetDampingFlags( );
		}
		return nFlags;
	}

	bool HasLegacyStretchForce( ) const;

	struct MbaContext_t
	{
		CUtlVector< FeQuad_t > quads;
		CUtlVector< FeTri_t > tris;
		CUtlVectorAligned< FeSimdRodConstraint_t > simdRods;
		CUtlVectorAligned< FeSimdQuad_t > simdQuads[ 3 ];
		CUtlVectorAligned< FeSimdTri_t > simdTris[ 3 ];
		CUtlVectorAligned< FeSimdNodeBase_t > simdBases;
		CUtlVector< FeSpringIntegrator_t > springs;
		CUtlVectorAligned< FeSimdSpringIntegrator_t > simdSprings;
		CUtlVector< FeFollowNode_t > nodeFollowers;;
		CUtlVector< FeCollisionSphere_t > collisionSpheres;
		CUtlVector< FeCollisionPlane_t > collisionPlanes;
		CUtlVector< FeWorldCollisionParams_t > worldCollisionParams;
		CUtlVector< uint16 > worldCollisionNodes;
		CUtlVectorAligned< FeFitMatrix_t > fitMatrices;
		FitWeightArray_t fitWeights;
		uint m_nFitMatrices1;
		uint m_nFitMatrices2;
		uint nLegacyStretchForceCount;
		uint nCollisionEllipsoidsInclusive;
		uint nNodeIntegratorCount;
		uint nStringsMemSize;
		uint nCtrlNameCount;
		bool m_bHasNodeCollisionRadii;
		bool m_bUsePerNodeLocalRotation;
		bool m_bUsePerNodeLocalForce;
	};

	template <typename Allocator > void OnAllocateMultiBuffer( Allocator &a, MbaContext_t &context );

protected:
	void ConvertCtrlToNode( int &refCtrl )
	{
		if ( refCtrl >= 0 )
		{
			Assert( refCtrl < m_CtrlToNode.Count() );
			refCtrl = m_CtrlToNode[ refCtrl ];
		}
	}
	void ConvertCtrlToNode( uint16 &refCtrl )
	{
		Assert( int( refCtrl ) < m_CtrlToNode.Count() );
		refCtrl = m_CtrlToNode[ refCtrl ];
	}
	void ConvertCtrlToNode( uint32 &refCtrl )
	{
		Assert( int( refCtrl ) < m_CtrlToNode.Count() );
		refCtrl = m_CtrlToNode[ refCtrl ];
	}
	float GetRank( const FeAxialEdgeBend_t &bend )const
	{
		float flRank = 0;

		// ad-hoc ranking of bends; if a bend affects some node a lot, then it's ranked closer to that node.
		// Ranks of nodes within one bend shouldn't differ by more than 2, because the bends are comprised
		// of adjacent quads' nodes, so this won't deviate much from strict ranking by the lowest-rank node in the bend
		flRank += bend.flWeight[ 0 ] * m_Nodes[ bend.nNode[ 0 ] ].nRank;
		flRank += bend.flWeight[ 1 ] * m_Nodes[ bend.nNode[ 1 ] ].nRank;
		flRank += bend.flWeight[ 2 ] * m_Nodes[ bend.nNode[ 2 ] ].nRank;
		flRank += bend.flWeight[ 2 ] * m_Nodes[ bend.nNode[ 3 ] ].nRank;
		flRank += bend.flWeight[ 3 ] * m_Nodes[ bend.nNode[ 4 ] ].nRank;
		flRank += bend.flWeight[ 3 ] * m_Nodes[ bend.nNode[ 5 ] ].nRank;
		return flRank;
	}
public:
	struct BuildElem_t
	{
		enum { MAX_NODES = 4 };
		BuildElem_t()
		{
			nRank = 0;
			flSlack = 0;
			nStaticNodes = 0;
			for ( int i = 0; i < MAX_NODES; ++i )
				nNode[ i ] = 0;
		}

		uint nNode[ MAX_NODES ];
		uint NumNodes( )const { return nNode[ 3 ] == nNode[ 2 ] ? 3 : 4; }
		uint nStaticNodes; // 0..2
		float flSlack;
		int nRank; // 0 means static, then it means "how many elements removed from the closest static"

		static bool Order( const BuildElem_t &left, const BuildElem_t &right )
		{
			int nDelta = int( right.nStaticNodes ) - int( left.nStaticNodes );
			if ( nDelta == 0 )
			{
				return left.nRank < right.nRank;
			}
			return nDelta < 0;
		}
	};
	struct BuildSpring_t
	{
		uint16 nNode[ 2 ];
		float32 flSpringConstant;
		float32 flSpringDamping;
		float32 flStretchiness; // Not Implemented!
	};

	struct BuildCollisionSphere_t
	{
		int m_nParent;
		int m_nChild;
		float m_flRadius;
		Vector m_vOrigin;
		bool m_bInclusive;
		float m_flStickiness;

		BuildCollisionSphere_t( )
		{
			m_nParent = -1;
			m_nChild = -1;
			m_flRadius = 0;
			m_vOrigin = vec3_origin;
			m_bInclusive = true;
			m_flStickiness = 0;
		}

		bool IsDegenerate( )const
		{
			return m_nParent < 0 || m_nChild < 0 || ( !m_bInclusive && m_flRadius < 1e-3f );
		}
	};

	struct BuildCollisionPlane_t
	{
		int m_nParent;
		int m_nChild;
		RnPlane_t m_Plane;
		float m_flStickiness;
		BuildCollisionPlane_t( )
		{
			m_nParent = -1;
			m_nChild = -1;
			m_Plane.m_flOffset = 0;
			m_flStickiness = 0;
			m_Plane.m_vNormal = vec3_origin;
		}
		bool IsDegenerate( )const
		{
			return m_nParent < 0 || m_nChild < 0 || m_Plane.m_vNormal.LengthSqr( ) < 1e-12f;
		}
	};


	struct BuildNode_t
	{
		CTransform transform; // relaxed position
		float flMassMultiplier;
		float flMassBias;
		float invMass;
		float flSlack;
		float flGravityZ;
		float flCollisionRadius;
		FeNodeIntegrator_t integrator;
		const char *pName;
		int nParent;
		int nRank; // 0 means static, then it means "how many elements removed from the closest static"
		uint nCollisionMask;

		int nFollowParent;
		float flFollowWeight;
		float flWorldFriction; // not really a friction coefficient, this corresponds to WorldFriction coefficient from Source1 cloth
		float flGroundFriction;

		float flLegacyStretchForce;
		float flLocalForce;
		float flLocalRotation;
		
		bool bSimulated : 1;
		bool bForceSimulated : 1;
		bool bFreeRotation : 1;
		bool bAnimRotation : 1;
		bool bMassMultiplierGlobal : 1; // if true, the mass multipliers are gathered and distributed so that all nodes with "global" multipliers keep the mass ratio = multiplier ratio
		bool bVirtual : 1;
		bool bNeedNodeBase : 1;
		bool bWorldCollision : 1;
		bool bOsOffset : 1;

		BuildNode_t( )
		{
			bSimulated = false;
			bForceSimulated = false;
			bFreeRotation = false; // true only makes sense for non-simulated
			bAnimRotation = false;
			bMassMultiplierGlobal = false;
			bVirtual = false;
			bNeedNodeBase = false;
			bWorldCollision = false;
			bOsOffset = false;
			flWorldFriction = 1.0f;
			flGroundFriction = 0.0f;
			flLegacyStretchForce = 0;
			transform = g_TransformIdentity;
			flMassMultiplier = 1.0f; // can be 0
			flMassBias = 0.0f;
			invMass = 0.0f; // can be arbitrary
			flSlack = 0.0f;
			flCollisionRadius = 0;
			flGravityZ = 360;
			pName = NULL;
			nRank = 0;
			nParent = -1;
			nFollowParent = -1;
			flFollowWeight = 0;
			nCollisionMask = 0;
			flLocalForce = 1.0f;
			flLocalRotation = 0.0f;
			integrator.Init();
		}

		uint GetDampingFlags( )const
		{
			uint nFlags = 0;
			if ( integrator.flPointDamping != 0 )
				nFlags |= FE_FLAG_HAS_NODE_DAMPING;
			if ( integrator.flAnimationVertexAttraction != 0 )
				nFlags |= FE_FLAG_HAS_ANIMATION_VERTEX_ATTRACTION;
			if ( integrator.flAnimationForceAttraction != 0 )
				nFlags |= FE_FLAG_HAS_ANIMATION_FORCE_ATTRACTION;
			if ( integrator.flGravity != 360 )
				nFlags |= FE_FLAG_HAS_CUSTOM_GRAVITY;
			if ( bSimulated && flLegacyStretchForce != 0 )
				nFlags |= FE_FLAG_HAS_STRETCH_VELOCITY_DAMPING;
			return nFlags;
		}
	};


public:
	// this is to precompute orientations of bones that we have the freedom to orient
	static FeNodeBase_t BuildNodeBasisFast( const CUtlVectorAligned< BuildNode_t > &nodes, uint nNode, const CUtlSortVector< int > &neighbors );
	static void BuildNodeBases( const CUtlVectorAligned< BuildNode_t > &nodes, const CUtlVector< BuildElem_t > &elems, const CUtlVector< FeNodeBase_t > &presetNodeBases, CUtlVector< FeNodeBase_t > &nodeBases, CUtlVectorOfPointers< CUtlSortVector< int > > &neighbors );
	void BuildNodeBases();
	FeNodeBase_t BuildNodeBasisFast( uint nNode );
public:

// 	class CFitMatrix
// 	{
// 	public:
// 		int m_nStaticWeights; // should normally be 0,1 or 2. If 3+ static nodes influence this fit matrix, maybe we should only keep those static node influences and say we have 0 static nodes, so that it back-soves to static nodes only
// 		CUtlVector< InfluenceWeight_t > m_Weights;
// 	};



	// generated
	CUtlVectorOfPointers< CUtlSortVector< int > > m_NodeNeighbors;
	CUtlVector< FeCtrlOffset_t > m_CtrlOffsets;
	CUtlVector< FeCtrlOsOffset_t > m_CtrlOsOffsets;
	CUtlVector< FeAxialEdgeBend_t > m_AxialEdges;
	CUtlVector< OldFeEdge_t > m_OldFeEdges;
	CUtlVector< FeKelagerBend_t > m_KelagerBends;

	CUtlVector< FeEdgeDesc_t > m_FeEdgeDesc;

	CUtlVector< int > m_NodeToCtrl;
	CUtlVector< int > m_CtrlToNode;
	CUtlVectorOfPointers< CUtlVector< int > > m_Ropes;

	CUtlVector< FeNodeBase_t > m_NodeBases;
	CUtlVector< FeNodeReverseOffset_t > m_ReverseOffsets;
	CUtlVector< uint16 > m_TreeParents; // dynamic nodes (N) + clusters (N-1)
	CUtlVector< FeTreeChildren_t > m_TreeChildren; // clusters (N-1) * 2
	CUtlVector< uint16 > m_FreeNodes;

	// input
	CUtlVector< FeNodeBase_t > m_PresetNodeBases;
	CUtlVector< FeTaperedCapsuleStretch_t > m_TaperedCapsuleStretches;
	CUtlVector< FeTaperedCapsuleRigid_t > m_TaperedCapsuleRigids;
	CUtlVector< FeSphereRigid_t > m_SphereRigids;
	CUtlVector< BuildCollisionSphere_t > m_CollisionSpheres;
	CUtlVector< BuildCollisionPlane_t > m_CollisionPlanes;
	CUtlVector< BuildSpring_t > m_Springs;
	CUtlVector< FeRodConstraint_t > m_Rods;
	CUtlVector< BuildElem_t > m_Elems;
	CUtlVectorAligned< BuildNode_t > m_Nodes; // in-out
	CUtlVector< FeFitInfluence_t > m_FitInfluences;
	CUtlVectorOfPointers< CUtlSortVector< int > > m_Neighbors;

	bool m_bIdentityCtrlOrder;
	bool m_bEnableExplicitNodeMasses;
	bool m_bUnitlessDamping;
	bool m_bAddStiffnessRods;
	bool m_bUsePerNodeLocalForceAndRotation;
	bool m_bRigidEdgeHinges;
	float m_flQuadBendTolerance;
	int m_nFitMatrixMinInfluences;
	bool m_bNeedBacksolvedBasesOnly;
};


#endif
