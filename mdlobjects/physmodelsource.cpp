//===== Copyright © Valve Corporation, All rights reserved. ======//
#include "mdlobjects/physmodelsource.h"
#include "meshutils/mesh.h"
#include "movieobjects/dmemodel.h"
#include "dmeutils/dmmeshutils.h"
#include "meshsystem/imeshsystem.h"
#include "mathlib/disjoint_set_forest.h"
#include "meshutils/mesh.h"
#include "bone_setup.h"

void CPhysModelSource::GetBoneTriangles( CUtlStringToken joint, uint nFlags, float flMinWeight, CUtlVector<Vector> &arrVertices, CUtlVector<uint> &arrIndices )const
{
/*
	if( m_pRenderModel )
	{
		::GetBoneTriangles( m_pRenderModel, joint, nFlags, flMinWeight, arrVertices, arrIndices );
	}
	else */if( m_pDmeModel )
	{
		int nMeshBoneIndex = m_pDmeModel->GetJointIndex( joint );

		uint isEnumAll = nFlags & FLAG_ENUMERATE_VERTICES_ALL;

		int nJointCount = m_pDmeModel->GetJointCount();
		CVarBitVec bonesInSubtree( nJointCount );
		if( uint( nMeshBoneIndex ) < uint( nJointCount ) )
		{
			bonesInSubtree.Set( nMeshBoneIndex );
			if( nFlags & FLAG_ENUMERATE_VERTICES_WITH_SUBTREE )
			{
				ComputeSubtree( m_pDmeModel, nMeshBoneIndex, &bonesInSubtree );
			}
		}

		CreateDmeModelCache();
		for( int nMesh = 0; nMesh < m_DmeMeshCache.Count() ; ++nMesh )
		{
			CMesh *pMesh = m_DmeMeshCache[nMesh];
						
			CMesh::SkinningDataFields_t skinData = pMesh->GetSkinningDataFields();
			if( isEnumAll || skinData.HasSkinningData() )
			{
				CUtlVector<int> mapVerts;
				mapVerts.SetCount( pMesh->VertexCount() );
				mapVerts.FillWithValue( -1 );

				for( int nVertex = 0; nVertex < pMesh->VertexCount(); ++nVertex )
				{
					if( isEnumAll || pMesh->GetVertexJointSumWeight( skinData, nVertex, bonesInSubtree ) > flMinWeight )
					{
						// this vertex belongs to this joint
						mapVerts[nVertex] = arrVertices.AddToTail( pMesh->GetVertexPosition( nVertex ) );
					}
				}

				for( int nTri = 0, nTriCount = pMesh->TriangleCount(); nTri < nTriCount; ++nTri )
				{
					int v[3];
					for( int nTriVertIndex = 0; nTriVertIndex < 3; ++nTriVertIndex )
					{
						int nIndexInMesh = pMesh->m_pIndices[nTri * 3 + nTriVertIndex ];
						v[ nTriVertIndex ] = mapVerts[ nIndexInMesh ];
					}
					if( v[0] >= 0 && v[1] >= 0 && v[2] >= 0 )
					{
						// all 3 vertices of this triangle are mapped (i.e. belong to this bone)
						arrIndices.AddToTail( v[0] );
						arrIndices.AddToTail( v[1] );
						arrIndices.AddToTail( v[2] );
					}
				}
			}
		}
	}
}


void CPhysModelSource::CreateDmeModelCache() const
{
	if( m_pDmeModel && m_DmeMeshCache.Count() == 0 )
	{
		CUtlVector< CDmeDag* > dags;
		LoadCollisionMeshes( m_DmeMeshCache, dags, m_pDmeModel, 1.0f );
		if( dags.Count() == m_DmeMeshCache.Count() )
		{
			m_DmeDagIndexCache.SetCount( dags.Count() );
			for( int i = 0; i < dags.Count(); ++i )
			{
				m_DmeDagIndexCache[i] = m_pDmeModel->GetJointIndex( dags[i] );
			}
		}
	}
}


int CPhysModelSource::GetDmeDagIndex( int nMesh )const
{
	if( uint( nMesh ) < uint( m_DmeDagIndexCache.Count() ) )
	{
		return m_DmeDagIndexCache[ nMesh ];
	}
	return -1;
}


int CPhysModelSource::GetBoneCount()const
{
/*
	if( m_pRenderModel )
	{
		return m_pRenderModel->NumBones();
	}
*/

	if( m_pDmeModel )
	{
		return m_pDmeModel->GetJointCount();
	}

	return 0;
}

const char *CPhysModelSource::GetBoneNameByIndex( int nIndex )const
{
/*
	if( m_pRenderModel )
	{
		return m_pRenderModel->MasterSkeleton().GetBoneNameByIndex( nIndex );
	}
*/
	if( m_pDmeModel )
	{
		return m_pDmeModel->GetJoint( nIndex )->GetName();
	}
	return NULL;
}







AABB_t CPhysModelSource::GetBoneInfluenceBbox( CUtlStringToken joint, uint nFlags, const CTransform &bindPose, float flMinWeight )const
{
	AABB_t bbox;
	bbox.MakeInvalid();
	EnumerateBoneVerts( joint, nFlags, [&]( const Vector &vGlobal, float w ){
		if( ( nFlags & FLAG_ENUMERATE_VERTICES_ALL ) || ( w >= flMinWeight ) )
		{
			Vector vLocal = bindPose.TransformVectorByInverse( vGlobal );
			bbox |= vLocal;
		}
	});
	return bbox;
}



bool CPhysModelSource::BoneHasMeat( CUtlStringToken joint, uint nFlags, const CTransform &bindPose )const
{
	AABB_t bbox = GetBoneInfluenceBbox( joint, nFlags, bindPose );
	if( bbox.IsEmpty() )
		return false;

	Vector vDelta = bbox.m_vMaxBounds - bbox.m_vMinBounds;
	float flDim = vDelta.SmallestComponentValue();

	//float flDim = bbox.LengthOfSmallestDimension( );

	// don't accept too flat or degenerate geometries
	return flDim > 1.0f;
}


class CDmeModelSubtreeAdaptor
{
public: 
	CDmeModelSubtreeAdaptor( const CDmeModel *pModel ) : m_pDmeModel ( pModel ){}
	int GetParent( int nJoint )const
	{
		CDmeDag *pParent = m_pDmeModel->GetJoint( nJoint )->GetParent();
		if( pParent )
			return m_pDmeModel->GetJointIndex( pParent );
		return -1;
	}
protected:
	const CDmeModel *m_pDmeModel;
};


void ComputeSubtree( const CDmeModel *pDmeModel, int nSubtreeTipBone, CVarBitVec *pSubtree )
{
	CDmeModelSubtreeAdaptor adaptor( pDmeModel );
	ComputeSubtree( &adaptor, nSubtreeTipBone, pSubtree );
}


int CPhysModelSource::GetParentJoint( int nJoint )const
{
	if( m_pDmeModel )
	{
		CDmeDag *pParent = m_pDmeModel->GetJoint( nJoint )->GetParent();
		if( pParent )
			return m_pDmeModel->GetJointIndex( pParent );
	}

/*
	if( m_pRenderModel )
	{
		return m_pRenderModel->MasterSkeleton().GetParent( nJoint );
	}
*/
	return -1;
}


void CPhysModelSource::GetBoneSubtree( int nBone, CVarBitVec *pSubtree ) const
{
/*
	if( m_pRenderModel )
	{
		m_pRenderModel->MasterSkeleton().GetBoneSubtree( nBone, pSubtree );
	}
*/
	if( m_pDmeModel )
	{
		ComputeSubtree( m_pDmeModel, nBone, pSubtree );
	}
}


CTransform CPhysModelSource::GetBindPoseParentTransform( int nJointIndex )const
{
	CTransform tm = g_TransformIdentity;
/*
	if( m_pRenderModel )
	{
		m_pRenderModel->MasterSkeleton().GetBindPoseParentTransform( nJointIndex, tm );
	}
*/
	if( m_pDmeModel )
	{
		matrix3x4_t mat;
		m_pDmeModel->GetJoint( nJointIndex )->GetLocalMatrix( mat );
		tm = MatrixTransform( mat );
	}
	return tm;
}




void CPhysModelSource::GetBindPoseWorldTransforms( CUtlVector< CTransform > &transforms )const
{
/*
	if( m_pRenderModel )
	{
		int nBones = m_pRenderModel->NumBones( );
		transforms.SetCount( nBones );
		m_pRenderModel->MasterSkeleton().GetBindPoseWorldTransforms( g_TransformIdentity, 1.0f, nBones, transforms.Base() );
	}
*/
	if( m_pDmeModel )
	{
		int nBones = m_pDmeModel->GetJointCount();
		transforms.SetCount( nBones );
		for( int i = 0; i < nBones; ++i )
		{
			matrix3x4_t mat;
			m_pDmeModel->GetJoint( i )->GetAbsTransform( mat );
			transforms[i] = MatrixTransform( mat );
		}
	}
}

CPhysModelSource::Stats_t CPhysModelSource::GetStats( )const
{
	Stats_t stats;
/*
	if( GetRenderModel() )
	{
		for( int nMesh = 0; nMesh < GetRenderModel()->GetNumMeshes(); ++nMesh )
		{
			HRenderMesh hMesh = GetRenderModel()->GetMesh( nMesh );
			const CUtlVector<TraceDataForDraw_t> *pDrawData;
			g_pMeshSystem->GetToolsGeometryInfo( hMesh, &pDrawData );
			const CRenderMesh *pPermMesh = ResourceHandleToData( hMesh );
			if( pDrawData && pPermMesh )
			{
				stats.m_nMeshCount += pDrawData->Count();
				for( int nDrawData = 0; nDrawData < pDrawData->Count(); ++nDrawData )
				{
					const TraceDataForDraw_t &data = pDrawData->Element( nDrawData );
					stats.m_nVertCount += data.m_nTraceVertices;
					stats.m_nTriCount += data.m_nTraceTriangles;
				}
			}
		}
	}
	else
*/
	{
		CreateDmeModelCache();
		stats.m_nMeshCount = m_DmeMeshCache.Count();
		for( int nMesh = 0; nMesh < stats.m_nMeshCount; ++nMesh )
		{
			CMesh *pMesh = m_DmeMeshCache[nMesh];
			stats.m_nTriCount += pMesh->TriangleCount();
			stats.m_nVertCount += pMesh->VertexCount();
		}
	}
	return stats;
}
