//============ Copyright (c) Valve Corporation, All rights reserved. ==========
#ifndef CLOTHPROXYCOMPILER_HDR
#define CLOTHPROXYCOMPILER_HDR

#include "movieobjects/dmevertexdata.h"
#include "bitvec.h"
#include "tier1/utlstringmap.h"
#include "mdlobjects/authphysfx.h"
#include "mdlobjects/clothproxymesh.h"
#include "movieobjects/dmefaceset.h"
#include "meshutils/mesh.h"
#include "movieobjects/dmemeshtypes.h"

class CDmeModel;
class CAuthPhysFx;
class CVClothProxyMesh;
class CDmeDag;

class CClothProxyCompiler 
{
public:
	CVClothProxyMeshOptions m_Options;

	CClothProxyCompiler( CAuthPhysFx *pAuthFx );
	~CClothProxyCompiler(){}
	bool IsEmpty() const
	{
		return m_pAuthFx->m_Nodes.Count() == 0; 
	}
	CAuthPhysFx *GetFx() { return m_pAuthFx; }
	void Init( const CVClothProxyMeshOptions &clothProxyMeshList );

	int GetOrCreateClothRootBone();
	void Append( CDmeModel *pModel, float flClothEnableThreshold, const CVClothProxyMesh &proxy);
	void AppendPlaneCollision( CDmeModel *pModel );
	void Cook( );
	CLockedResource< PhysFeModelDesc_t > Compile( CResourceStream *pStream )const;
	void MarkFreeRotatingNodes( const CAuthPhysFx::CQuad &quad );
	void AlignNodes();
	int GetAuthFxBone( const char *pName );

	template <typename T>
	class CIndexedAttr
	{
	public:
		CDmrArrayConst< T > m_Data;
		CDmrArrayConst< int > m_IndexData;
	public:
		CIndexedAttr() {}
		CIndexedAttr( CDmeVertexData *pBindState, FieldIndex_t nField )
		{
			Init( pBindState, nField );
		}
		CIndexedAttr( CDmeVertexData *pBindState, CDmeVertexDataBase::StandardFields_t nField )
		{
			Init( pBindState, nField );
		}
		void Init( CDmeVertexData *pBindState, CDmeVertexDataBase::StandardFields_t nField )
		{
			Init( pBindState, pBindState->FindFieldIndex( nField ) );
		}
		CIndexedAttr( CDmeVertexData *pBindState, const char *pField )
		{
			Init( pBindState, pBindState->FindFieldIndex( pField ) );
		}

		void Init( CDmeVertexData *pBindState, FieldIndex_t nField )
		{
			if ( nField >= 0 )
			{
				m_Data = pBindState->GetVertexData( nField );
				m_IndexData = pBindState->GetIndexData( nField );
			}
		}

		operator bool() const { return m_IndexData.IsValid() && m_Data.IsValid() && m_Data.Count() > 0 && m_IndexData.Count() > 0; }
		const T& operator []( int i ) const { return m_Data[ m_IndexData[ i ] ]; }

		int GetDataCount() const{ return m_Data.Count(); }
		int GetElementCount()const { return m_IndexData.Count(); } // not really vertex count
		int GetAttrCount()const { return 1; } // TODO: find the number of attributes per vertex

		void Reset()
		{
			m_Data = CDmrArrayConst< T >();
			m_IndexData = CDmrArrayConst< int >();
		}
	};

	struct Binding_t
	{
		int nAuthFxBone;
		float flWeight;
		Binding_t() : nAuthFxBone( -1 ), flWeight( 0 ) {}
	};

	struct ProjItem_t
	{
		ProjItem_t( int i = -1, float f = 0 ) : nIndex( i ), flEnvelope( f ){}
		ProjItem_t( const ProjItem_t &other ) : nIndex( other.nIndex ), flEnvelope( other.flEnvelope ){}
		int nIndex;
		float flEnvelope;
	};

	struct QuadProjection_t 
	{
		Vector m_vContact; // projected point on the quad
		Vector m_vNormal;
		float m_flDistance; // negative if behind the plane
		int m_nBindings;
		Binding_t m_Binding[ 4 ];

		bool IsEmpty() const { return m_nBindings == 0; }
		Vector GetOriginalPoint()const { return m_vContact + m_vNormal * m_flDistance; }
		void AddBinding( uint nIndex, float flWeight )
		{
			Binding_t &b = m_Binding[ m_nBindings++ ];
			b.nAuthFxBone = nIndex;
			b.flWeight = flWeight;
		}
		bool operator < ( const QuadProjection_t &other )const
		{
			return m_flDistance < other.m_flDistance;
		}
	};

	class CModelContext
	{
	public:
		CModelContext( CClothProxyCompiler *pCompiler, CDmeModel *pModel, float flClothEnableThreshold, const CVClothProxyMesh &proxy );
		int MapJointToFxBone( int nJoint, bool bSimulated );
		int GetJointCount()const { return m_JointToFxBone.Count(); }
	public:
		CDmeModel *m_pModel;
		CUtlVector< UtlSymId_t > m_JointToBoneSubset;
		CUtlVector< int > m_JointToFxBone;
		CAuthPhysFx * m_pAuthFx;
		float m_flClothEnableThreshold;
		const CVClothProxyMesh &m_Proxy;
	};
	friend class CModelContext;

	class CMeshContext
	{
	public:
		CMeshContext( CClothProxyCompiler *pCompiler, CModelContext *pModelContext, CDmeMesh *pMesh, const matrix3x4_t &tm, int nDmeMesh );
	public:
		
		int m_nDmeMesh;
		CDmeMesh *m_pDmeMesh;
		matrix3x4_t m_MeshTransform;
		CDmeVertexData *m_pBindState;
		CIndexedAttr< Vector > m_AttrPos, m_AttrNormal, m_AttrTangent;
		CAuthPhysFx *m_pAuthFx;
		CModelContext *m_pModelContext;
		CUtlVector< int > m_DmePosToFxBone; // position (index in the position array in DmeVertex) to FxBone (index of CBone in AuthFx) map
		CClothProxyCompiler *m_pCompiler;
	public:
		int FindMostBoundJoint( int nDmePos, float flBonusForExisting );
		int GetSkinningJointCount();
		int GetOrCreateClothBoneIndex( int nDmePos, bool bSimulated );
		int GetClothBoneIndex( int nDmePos );
		CAuthPhysFx::CBone *GetOrCreateClothBone( int nDmePos, bool bSimulated );
		CAuthPhysFx::CBone *GetClothBone( int nDmePos );
	};
	friend class CMeshContext;
	int GetMaxBonesPerVertex()const { return Max( 1, Min( 4, m_Options.m_nMaxBonesPerVertex ) ); }

	bool Project( const Vector &vPos, UtlSymId_t*pFindSubset, int nFindSubsetCount, CUtlVector<Binding_t> &outBindings, int nIslandFilter );

	void ProjectAndAddToQueue( const ProjItem_t &q, const Vector & vPos, CUtlSortVector< QuadProjection_t > &bestProj, int nIslandFilter );

	int NodeToIsland( int nNode );
	int GetIslandCount() const { return m_nIslandCount; }

protected:
	void AppendPlaneCollisionDag( CModelContext &modelContext, CDmeDag *pDmeDag );
	void AppendPlaneCollisionMesh( CModelContext &modelContext, CDmeMesh *pMesh, const matrix3x4_t &tm );
	void AppendDag( CModelContext &context, CDmeDag *pDmeDag );
	void AppendMesh( CModelContext &modelContext, CDmeMesh *pMesh, const matrix3x4_t &tm );
	void CreateClothBones( CMeshContext &context );
	void AddFitWeights( CMeshContext &context );
	void BindNodeOffsetParents( CMeshContext &context );
	void OrientClothBones( CMeshContext &context );
	void CreateClothQuads( CMeshContext &context );
	void ApplyClothBoneAttributes( CMeshContext &context );
	QuadProjection_t ProjectOnQuad( const Vector &vPos, const CAuthPhysFx::CQuad &quad );
	QuadProjection_t ProjectOnTri( const Vector &vPos, const CAuthPhysFx::CQuad &quad );
public:
	class CVertex
	{
	public:
		Vector m_vPos; // world position
	};

	class CPolygon
	{
	public:
		CUtlVectorFixedGrowable< CVertex*, 4 > m_Verts;
		Vector m_vNormal;
	};


	class CAuthFxSubset
	{
	public:
		CUtlVectorFixedGrowable< ProjItem_t, 4 > m_Quads;
		CUtlVectorFixedGrowable< ProjItem_t, 4 > m_Rods;

		void Append( const CAuthFxSubset &other );
	};

	class CAuthFxBoneSubset : public CAuthFxSubset
	{
	public:
		CAuthFxBoneSubset( int nModelJoint ) : m_nModelJoint( nModelJoint ){}
		int m_nModelJoint;
	};

	UtlSymId_t FindBoneSym( const char *pName )
	{
		return m_BoneSubsets.Find( pName );
	}

	CUtlVector< CUtlString > m_MeshNames; // names of meshes that comprise this proxy mesh, for debugging

protected:
	CAuthPhysFx *m_pAuthFx;
	CUtlStringMapAutoPurge< CAuthFxBoneSubset* > m_BoneSubsets; // for each AuthFx bone, a subset of quads and rods to project to
	CAuthFxSubset m_DefaultSubset;
	int m_nProxyMeshes;
	int m_nRootFxNode;
private:
	int m_nIslandCount;
	CUtlVector< int > m_NodeToIslandMap;
};



template <typename Functor >
inline void EnumerateFaces( CDmeMesh *pDmeMesh, CDmeVertexData * pBindState, const CVarBitVec *pUsefulDmeVerts, Functor &fn )
{
	//const CUtlVector< Vector > & arrDmeVert = pBindState->GetPositionData();// the most original and un-split position array
	const CUtlVector< int > & arrVertIndex = pBindState->GetVertexIndexData( CDmeVertexDataBase::FIELD_POSITION );

	// find connecting (static) bones, and the bones that will drive them (add those, too, taking care not to add them twice)
	//pBindState->FindFieldIndex( CDmeVertexDataBase::FIELD_POSITION )

	int nFaceSetCount = pDmeMesh->FaceSetCount();
	int nSkippedDegenerate = 0, nSkippedManygons = 0;
	for ( int nFaceSet = 0; nFaceSet < nFaceSetCount; ++nFaceSet )
	{
		CDmeFaceSet *pFaceSet = pDmeMesh->GetFaceSet( nFaceSet );
		int nIndexCount = pFaceSet->NumIndices(); // for each face (N-gon), there are N indices and -1 in this array
		int nFirstIndex = 0;
		while ( nFirstIndex < nIndexCount )
		{
			int nVertexCount = pFaceSet->GetNextPolygonVertexCount( nFirstIndex );
			if ( nVertexCount < 2 )
			{
				nSkippedDegenerate++;
				continue;
			}

			int nUsefulVertexCount = nVertexCount;
			if ( nVertexCount > 4 )
			{
				nUsefulVertexCount = 4;
				nSkippedManygons++;
			}

			int nPosVerts[ 4 ] = { -1,-1,-1,-1};
			//DmeVertexIndex_t nFaceVerts[ 4 ];
			CAuthPhysFx::CQuad quad;
			bool bUseful = false;
			for ( int nV = 0; nV < nUsefulVertexCount; ++nV )
			{
				DmeVertexIndex_t nFaceVertIndex = pFaceSet->GetIndex( nFirstIndex + nV );
				//nFaceVerts[ nV ] = nFaceVertIndex;
				int nDmeVert = arrVertIndex[ nFaceVertIndex ];
				nPosVerts[ nV ] = nDmeVert;
				if ( !pUsefulDmeVerts || ( nDmeVert < pUsefulDmeVerts->GetNumBits() && pUsefulDmeVerts->IsBitSet( nDmeVert ) ) )
				{// it's all useful if we don't have a bitmap; otherwise, consult the bitmap - at least one vertex of the polygon must be useful
					bUseful = true;
				}
			}
			if ( bUseful ) // skip irrelevant polygons
			{
				for ( int nV = nUsefulVertexCount; nV < 4; ++nV )
				{
					nPosVerts[ nV ] = nPosVerts[ nUsefulVertexCount - 1 ];
					//nFaceVerts[ nV ] = nFaceVerts[ nUsefulVertexCount - 1 ];
				}

				fn( nPosVerts, nUsefulVertexCount );
			}

			nFirstIndex += nVertexCount + 1; // skip N-gon indices and the -1 terminator
		}
	}
	if ( nSkippedDegenerate || nSkippedManygons )
	{
		Warning( "Cloth: %d degenerate and %d 5+gons\n", nSkippedDegenerate, nSkippedManygons );
	}
}

/*
template <typename Functor >
inline void EnumerateFaces( CDmeMesh *pDmeMesh, CDmeVertexData * pBindState,  Functor &fn )
{
	//const CUtlVector< Vector > & arrDmeVert = pBindState->GetPositionData();// the most original and un-split position array
	const CUtlVector< int > & arrVertIndex = pBindState->GetVertexIndexData( CDmeVertexDataBase::FIELD_POSITION );

	int nFaceSetCount = pDmeMesh->FaceSetCount();
	for ( int nFaceCount = 0; nFaceCount < nFaceSetCount; ++nFaceCount )
	{
		CDmeFaceSet *pFaceSet = pDmeMesh->GetFaceSet( nFaceCount );
		int nIndexCount = pFaceSet->NumIndices(); // for each face (N-gon), there are N indices and -1 in this array
		int nFirstIndex = 0;
		while ( nFirstIndex < nIndexCount )
		{
			int nTotalVertexCount = pFaceSet->GetNextPolygonVertexCount( nFirstIndex );
			int nPolyIndex0 = arrVertIndex[ pFaceSet->GetIndex( nFirstIndex ) ];
			//Vector vApex = arrDmeVert[ nPolyIndex0 ]; // we're going around the first vertex and split the poly into quads and tris
			for ( int nBase = 1; nBase < nTotalVertexCount; nBase += 2 )
			{
				int nPosVerts[ 4 ] = { nPolyIndex0 };
				int nFoundVerts = 1;
				//Vector vPrevVert = vApex;
				for ( int m = Min( nTotalVertexCount - 1, nBase + 2 ); m >= nBase; m-- )
				{
					int nPolyIndexM = arrVertIndex[ pFaceSet->GetIndex( nFirstIndex + m ) ];
					Vector vVertM = arrVertIndex[ nPolyIndexM ];
					//if ( ( vPrevVert - vVertM ).Length() > flCollapseEdgesThreshold )
					{
						nPosVerts[ nFoundVerts++ ] = nPolyIndexM;
						//vPrevVert = vVertM;
					}
				}

				if ( nFoundVerts > 1 )
				{
					for ( int m = nFoundVerts; m < 4; ++nFoundVerts )
						nPosVerts[ m ] = nPosVerts[ nFoundVerts - 1 ];
					fn( nPosVerts, nFoundVerts );
				}
			}

			nFirstIndex += nVertexCount + 1; // skip N-gon indices and the -1 terminator
		}
	}
}

*/

template < typename Attr >
class ClothAttributes
{
public:
	Attr m_animation_attraction ;
	Attr m_animation_force_attraction ;
	Attr m_drag ;
	Attr m_mass ;
	Attr m_gravity ;
	Attr m_collision_radius ;
	Attr m_ground_collision ;
	Attr m_ground_friction ;
	Attr m_use_rods;
	Attr m_anchor_free_rotate;
protected:
	float Get( const CMesh::CSingleVertexFieldAccessor< float > &accessor )
	{
		return *accessor;
	}
	float Get( float x )
	{
		return x;
	}
public:
	template < typename Map >
	ClothAttributes( Map map )
	{
		m_animation_attraction = map( "cloth_animation_attract" );
		m_animation_force_attraction = map( "cloth_animation_force_attract" );
		m_drag = map( "cloth_drag" );
		m_mass = map( "cloth_mass" );
		m_gravity = map( "cloth_gravity" );
		m_collision_radius = map( "cloth_collision_radius" );
		m_ground_collision = map( "cloth_ground_collision" );
		m_ground_friction = map( "cloth_ground_friction" );
		m_use_rods = map( "cloth_use_rods" );
		m_anchor_free_rotate = map( "cloth_anchor_free_rotate" );
	}

	void Apply( int nVert, CAuthPhysFx::CBone &authFxBone )
	{ 
		if ( m_animation_attraction )
			authFxBone.m_Integrator.flAnimationVertexAttraction = 30 * Get( m_animation_attraction[ nVert ] );
		if ( m_animation_force_attraction )
			authFxBone.m_Integrator.flAnimationForceAttraction = 30 * Get( m_animation_force_attraction[ nVert ] );
		if ( m_drag )
			authFxBone.m_Integrator.flPointDamping = 30 * Get( m_drag[ nVert ] );
		if ( m_mass )
			authFxBone.m_flMassBias = expf( Get( m_mass[ nVert ] ) );
		if ( m_gravity )
			authFxBone.m_Integrator.flGravity = Get( m_gravity[ nVert ] );
		if ( m_collision_radius )
			authFxBone.m_flCollisionRadius = Get( m_collision_radius[ nVert ] );
		if ( m_ground_collision )
		{
			authFxBone.m_flWorldFriction = 1.0f - Get( m_ground_collision[ nVert ] ); // ground_collision 0 maps to "worldFriction(source1 misnomer)" 1
			if ( authFxBone.m_flWorldFriction < 0.999f )
			{
				authFxBone.m_bNeedsWorldCollision = true;
			}
		}
		if ( m_ground_friction )
		{
			authFxBone.m_flGroundFriction = Get( m_ground_friction[ nVert ] );
		}
		if ( m_use_rods )
		{
			authFxBone.m_bUseRods = Get( m_use_rods[ nVert ] )> 0.5f;
		}
		if ( m_anchor_free_rotate )
		{
			authFxBone.m_bFreeRotation = Get( m_anchor_free_rotate[ nVert ] )> 0.5f;
		}
	}
};


extern const char *g_pDefaultClothRootBoneName;
extern CClothProxyCompiler *g_pClothProxyCompiler ;


#endif // CLOTHPROXYCOMPILER_HDR