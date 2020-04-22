//===== Copyright © Valve Corporation, All rights reserved. ======//
//
// This is a declaration of an abstraction of data used to generate collision mesh and potentially other physics data
//
#ifndef PHYS_MODEL_SOURCE_HDR
#define PHYS_MODEL_SOURCE_HDR

#include "mathlib/aabb.h"
#include "tier1/utlvector.h"
#include "tier1/utlstringtoken.h"
#include "bitvec.h"
#include "meshutils/mesh.h"
#include "movieobjects/dmemodel.h"
#include "mathlib/transform.h"

class CMesh;

enum VertEnumFlagEnum_t
{
	FLAG_ENUMERATE_VERTICES_WITH_SUBTREE = 1 << 0, // enumerate vertices belonging to the given bone and its children
	FLAG_ENUMERATE_VERTICES_ALL = 1 << 1
};

void ComputeSubtree( const CDmeModel *pDmeModel, int nSubtreeTipBone, CVarBitVec *pSubtree );

// this is an adaptor class so that we can use render mesh or 
class CPhysModelSource
{
	class CModel; // Source2 model; not supported in Source1
public:
	CPhysModelSource():
		m_pRenderModel( NULL ),
		m_pDmeModel( NULL )
	{

	}
	CPhysModelSource( const CModel *pModel ):
		m_pRenderModel( pModel ),
		m_pDmeModel( NULL )
	{
	}

	CPhysModelSource( CDmeModel *pModel ):
		m_pRenderModel( NULL ),
		m_pDmeModel( pModel )
	{
	}

	CPhysModelSource( const CPhysModelSource &source ):
		m_pRenderModel( source.m_pRenderModel ),
		m_pDmeModel( source.m_pDmeModel )
	{

	}

	~CPhysModelSource()
	{
		Purge();
	}

	void SetRenderModel ( const CModel *pModel )
	{
		Purge();
		m_pRenderModel = pModel;
	}

	void SetDmeModel( CDmeModel *pModel )
	{
		Purge();
		m_pDmeModel = pModel;
	}

	const CModel *const GetRenderModel() const { return m_pRenderModel; }
	CDmeModel *GetDmeModel() const { return m_pDmeModel; }

	void Purge()
	{
		m_DmeMeshCache.PurgeAndDeleteElements();
		m_pDmeModel    = NULL;
		m_pRenderModel = NULL;
	}

	bool IsValid()const
	{
		return m_pRenderModel || m_pDmeModel;
	}

	int GetBoneCount()const;
	const char *GetBoneNameByIndex( int nIndex )const;

	int FindBoneByName( const char *pName )const
	{
		for ( int i = 0; i < GetBoneCount(); ++i )
		{
			if ( !V_stricmp( GetBoneNameByIndex( i ), pName ) )
				return i;
		}
		return -1;
	}

	void GetBoneTriangles( CUtlStringToken joint, uint nFlags, float flMinWeight, CUtlVector<Vector> &arrVertices, CUtlVector<uint> &arrIndices )const;
	bool BoneHasMeat( CUtlStringToken joint, uint nFlags, const CTransform &bindPose ) const;
	AABB_t GetBoneInfluenceBbox( CUtlStringToken joint, uint nFlags, const CTransform &bindPose, float flMinWeight = 0.5f )const;

	static bool IsBitInSet( int nBit, const CVarBitVec &bonesInSubtree )
	{
		return uint( nBit ) < uint( bonesInSubtree.GetNumBits() ) && bonesInSubtree.IsBitSet( nBit );
	}

	template <typename Fn>
	void EnumerateBoneVerts( CUtlStringToken joint, uint nFlags, Fn functor )const
	{
/*
		if( GetRenderModel() )
		{
			::EnumerateBoneVerts( GetRenderModel(), joint, nFlags, functor );
		}
		else */if( m_pDmeModel )
		{
			CreateDmeModelCache();
			int nMeshBoneIndex = m_pDmeModel->GetJointIndex( joint );

			for( int nMesh = 0; nMesh < m_DmeMeshCache.Count(); ++nMesh )
			{
				CMesh *pMesh = m_DmeMeshCache[nMesh];
				CMesh::SkinningDataFields_t skinData;
				CVarBitVec bonesInSubtree( m_pDmeModel->GetJointCount() );
				if( nMeshBoneIndex >= 0 )
				{
					bonesInSubtree.Set( nMeshBoneIndex );
					skinData = pMesh->GetSkinningDataFields();
					if( nFlags & FLAG_ENUMERATE_VERTICES_WITH_SUBTREE )
					{
						ComputeSubtree( m_pDmeModel, nMeshBoneIndex, &bonesInSubtree );
					}
				}

				if( ( nFlags & FLAG_ENUMERATE_VERTICES_ALL ) || ( !skinData.HasSkinningData() && IsBitInSet( GetDmeDagIndex( nMesh ), bonesInSubtree ) ) )
				{
					for( int nVert = 0; nVert < pMesh->VertexCount(); ++nVert )
					{
						// this vertex belongs to this joint
						functor( pMesh->GetVertexPosition( nVert ), 1.0f );
					}
				}
				else if( skinData.HasSkinningData() )
				{
					for( int nVert = 0; nVert < pMesh->VertexCount(); ++nVert )
					{
						// this vertex belongs to this joint
						float flWeight = pMesh->GetVertexJointSumWeight( skinData, nVert, bonesInSubtree );
						functor( pMesh->GetVertexPosition( nVert ), flWeight );
					}
				}
			}
		}
	}

	struct Stats_t
	{
		int m_nVertCount;
		int m_nTriCount; 
		int m_nMeshCount;

		Stats_t()
		{
			m_nVertCount = 0;
			m_nTriCount = 0;
			m_nMeshCount = 0;
		}

		void operator += ( const Stats_t &that )
		{
			m_nTriCount += that.m_nTriCount;
			m_nVertCount += that.m_nVertCount;
			m_nMeshCount += that.m_nMeshCount;
		}
	};
	Stats_t GetStats( )const;

	int GetParentJoint( int nJoint )const;
	void GetBoneSubtree( int nBone, CVarBitVec *pSubtree ) const; // this is O(N) algorithm : starting from the given bone, it finds all (grand)children of that bone and sets corresponding bits in pSubtree; pSubtree must be pre-allocated with the desired number of bones
	CTransform GetBindPoseParentTransform( int nJointIndex )const;
	void GetBindPoseWorldTransforms( CUtlVector< CTransform > &transforms )const;
	bool GetAnimFrame( const char *pAnimName, float flCycle, CUtlVector< CTransform > *pTransformsOut )const;

	int GetDmeDagIndex( int nMesh )const;
	
protected:
	void CreateDmeModelCache()const;
protected:
	const CModel *m_pRenderModel;
	CDmeModel *m_pDmeModel;
	mutable CUtlVector< CMesh* > m_DmeMeshCache;
	mutable CUtlVector< int > m_DmeDagIndexCache; // for each DmeMesh, this is the index of DmeDag in the Dme Model
};


inline void AdjustLegacyDotaOrientation( CUtlVector< CTransform > &transforms )
{
	float sin45 = sqrtf( .5f );
	CTransform root( vec3_origin, Quaternion( 0, 0, sin45, sin45 ) * Quaternion( sin45, 0, 0, sin45 ) );
	for ( int nBone = 0; nBone < transforms.Count( ); ++nBone )
	{
		transforms[ nBone ] = ConcatTransforms( root, transforms[ nBone ] );
	}
}



//void GetBoneTriangles( const CModel *pModel, CUtlStringToken joint, uint nFlags, float flMinWeight, CUtlVector<Vector> &arrVertices, CUtlVector<uint> &arrIndices );


#endif