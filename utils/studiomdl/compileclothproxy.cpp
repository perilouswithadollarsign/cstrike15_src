#include "movieobjects/dmemodel.h"
#include "movieobjects/dmemesh.h"
#include "compileclothproxy.h"
#include "mdlobjects/clothproxymesh.h"
#include "mathlib/femodelbuilder.h"


const char *g_pDefaultClothRootBoneName = "$cloth_root";
CClothProxyCompiler *g_pClothProxyCompiler = NULL;

CClothProxyCompiler::CClothProxyCompiler( CAuthPhysFx *pAuthFx )
	: m_pAuthFx( pAuthFx )
	, m_nProxyMeshes( 0 )
{
	m_nRootFxNode = pAuthFx->FindNodeIndex( g_pDefaultClothRootBoneName );
}


void CClothProxyCompiler::Init( const CVClothProxyMeshOptions &clothProxyMeshList )
{
	Assert( IsEmpty() ); // calling Init() twice would not be too bad now, but it wouldn't be expected
	m_Options = clothProxyMeshList;
	if ( m_Options.m_bCreateStaticBone )
	{
		GetOrCreateClothRootBone();
	}
	m_nIslandCount = 0;
	m_NodeToIslandMap.Purge();
}



void CClothProxyCompiler::Append( CDmeModel *pModel, float flClothEnableThreshold, const CVClothProxyMesh &proxy )
{
	Assert( m_NodeToIslandMap.Count() == 0 ); // we didn't finish yet

	pModel->ZUp( true );
	CDmeModel *pDmeModelModel = pModel->GetValueElement< CDmeModel >( "model" );
	if ( pDmeModelModel && pDmeModelModel != pModel )
	{
		pDmeModelModel->ZUp( true );
	}

	CDmeModel *pDmeModelSkeleton = pModel->GetValueElement< CDmeModel >( "skeleton" );
	if ( pDmeModelSkeleton && pDmeModelSkeleton != pModel )
	{
		pDmeModelSkeleton->ZUp( true );
	}


	CModelContext context( this, pModel, flClothEnableThreshold, proxy );
	//CDmeTransformList *pBindPose = pModel->FindBaseState( "bind" );
	int nBaseQuad = m_pAuthFx->m_Quads.Count();

	int nChildCount = pModel->GetChildCount();
	for ( int nChild = 0; nChild < nChildCount; ++nChild )
	{
		if ( CDmeDag *pChild = pModel->GetChild( nChild ) )
		{
			AppendDag( context, pChild );
		}
	}

	if ( proxy.m_bFlexClothBorders )
	{
		for ( int nQuad = nBaseQuad; nQuad < m_pAuthFx->m_Quads.Count(); ++nQuad )
		{
			MarkFreeRotatingNodes( m_pAuthFx->m_Quads[ nQuad ] );
		}
	}
}


// look through all planes in all the meshes (supposedly everything's convex..)
// find the plane farthest from each node (should we add "cloth_plane_collision_enable" attribute ?) and the most-bound bone
// that's going to be the plane and parent bone for collision
void CClothProxyCompiler::AppendPlaneCollision( CDmeModel *pModel )
{
	pModel->ZUp( true );
	CDmeModel *pDmeModelModel = pModel->GetValueElement< CDmeModel >( "model" );
	if ( pDmeModelModel && pDmeModelModel != pModel )
	{
		pDmeModelModel->ZUp( true );
	}

	CVClothProxyMesh proxy;
	CModelContext context( this, pModel, 0, proxy );
	CDmeModel *pDmeModelSkeleton = pModel->GetValueElement< CDmeModel >( "skeleton" );
	if ( pDmeModelSkeleton && pDmeModelSkeleton != pModel )
	{
		pDmeModelSkeleton->ZUp( true );
	}


	int nChildCount = pModel->GetChildCount();
	for ( int nChild = 0; nChild < nChildCount; ++nChild )
	{
		if ( CDmeDag *pChild = pModel->GetChild( nChild ) )
		{
			AppendPlaneCollisionDag( context, pChild );
		}
	}
}

void CClothProxyCompiler::AppendPlaneCollisionDag( CModelContext &modelContext, CDmeDag *pDmeDag )
{
	if ( CDmeMesh *pMesh = CastElement< CDmeMesh >( pDmeDag->GetShape() ) )
	{
		matrix3x4_t tm;
		pDmeDag->GetAbsTransform( tm );
		AppendPlaneCollisionMesh( modelContext, pMesh, tm );
	}

	int nChildCount = pDmeDag->GetChildCount();
	for ( int nChild = 0; nChild < nChildCount; ++nChild )
	{
		CDmeDag *pChild = pDmeDag->GetChild( nChild );
		AppendPlaneCollisionDag( modelContext, pChild );
	}

}

void CClothProxyCompiler::AppendPlaneCollisionMesh( CModelContext &modelContext, CDmeMesh *pMesh, const matrix3x4_t &tm )
{
	CMeshContext context( this, &modelContext, pMesh, tm, m_nProxyMeshes );
	if ( !context.m_AttrPos )
	{
		Warning( "No position data found in mesh %s\n", pMesh->GetName() );
		return; // something's wrong if we have no positions
	}

	// for each existing physical cloth vertex (CAuthPhysFx::Node), find the best (farthest) plane
	struct BestPlane_t
	{
		RnPlane_t m_WorldPlane; // the plane is in joint space
		Vector m_vPosition;
		float m_flDistance;
		int m_nDmeJointParent; // this is the Dme Joint Index of the 
	};

	CUtlVector< BestPlane_t > bestPlanes;
	bestPlanes.SetCount( m_pAuthFx->m_Nodes.Count() );

	for ( int nNode = 0; nNode < m_pAuthFx->m_Nodes.Count(); ++nNode )
	{
		bestPlanes[ nNode ].m_nDmeJointParent = -1;
		bestPlanes[ nNode ].m_flDistance = -1; // find some sane distance
		bestPlanes[ nNode ].m_vPosition = m_pAuthFx->m_Nodes[ nNode ].m_Transform.m_vPosition;
	}

	int nNonRigidPolygons = 0, nRejectedTests = 0, nTotalTests = 0;
	EnumerateFaces( pMesh, context.m_pBindState, NULL,
		[ &bestPlanes, &context, &nNonRigidPolygons, &modelContext, &nRejectedTests, &nTotalTests ]( int nPos[], int nCount )
	{
		// find this plane's parent joint
		int nParentJoint = context.FindMostBoundJoint( nPos[ 0 ], 1.0f );
		if ( nParentJoint < 0 )
		{
			++nNonRigidPolygons;
			return;
		}

		for ( int i = 1; i < nCount; ++i )
		{
			int nOtherParentJoint = context.FindMostBoundJoint( nPos[ i ], 1.0f );
			if ( nOtherParentJoint != nParentJoint )
			{
				++nNonRigidPolygons;
				return;
			}
		};

		Vector vCenter = vec3_origin, vNormal = vec3_origin, vPrev = context.m_AttrPos.m_Data[ nPos[ nCount - 1 ] ];

		for ( int i = 0; i < nCount; i++ )
		{
			Vector vNext = context.m_AttrPos.m_Data[ nPos[ i ] ];
			vCenter += vNext;
			// this is Newell normal algorithm, it's general and robust 
			vNormal.x += ( vPrev.y - vNext.y ) * ( vNext.z + vPrev.z );
			vNormal.y += ( vPrev.z - vNext.z ) * ( vNext.x + vPrev.x );
			vNormal.z += ( vPrev.x - vNext.x ) * ( vNext.y + vPrev.y );

			vPrev = vNext;
		}
		vCenter /= nCount;
		Vector vWorldCenter = VectorTransform( vCenter, context.m_MeshTransform );
		Vector vWorldNormal = VectorRotate( vNormal, context.m_MeshTransform ).Normalized();

		RnPlane_t worldPlane;
		worldPlane.m_vNormal = vWorldNormal;
		worldPlane.m_flOffset = DotProduct( vWorldNormal, vWorldCenter );

		nTotalTests += bestPlanes.Count();
		for ( int i = 0; i < bestPlanes.Count(); ++i )
		{
			BestPlane_t &bp = bestPlanes[ i ];
			float flDist = worldPlane.Distance( bp.m_vPosition );
			if ( flDist < 0 )
			{
				nRejectedTests++;
			}
			else if ( flDist >= bp.m_flDistance )
			{
				bp.m_flDistance = flDist;
				bp.m_nDmeJointParent = nParentJoint;
				bp.m_WorldPlane = worldPlane;
			}
		}
	} );
	if ( nNonRigidPolygons )
	{
		Msg( "Plane collision for cloth %s: Ignoring %d polygons because they aren't rigidly bound to a bone", pMesh->GetName(), nNonRigidPolygons );
	}

	if ( nRejectedTests > nTotalTests / 2 )
	{
		Msg( "Plane collision for cloth %s: Rejected %d/%d plane-node combos; are the normals wrong on the collision?", pMesh->GetName(), nRejectedTests, nTotalTests );
	}

	CVarBitVec fitNodes( m_pAuthFx->m_Nodes.Count() );
	for ( int i = 0; i < m_pAuthFx->m_FitInfluences.Count(); ++i )
	{
		fitNodes.Set( m_pAuthFx->m_FitInfluences[ i ].nMatrixNode );
	}
	for ( int nNode = 0; nNode < m_pAuthFx->m_Nodes.Count(); ++nNode )
	{
		const BestPlane_t &bestPlane = bestPlanes[ nNode ];
		if ( m_pAuthFx->m_Nodes[ nNode ].m_bSimulated
			&& bestPlane.m_nDmeJointParent >= 0
			&& !fitNodes.IsBitSet( nNode )
			//&& bestPlane.m_flDistance > 0
			)
		{
			CAuthPhysFx::CCollisionPlane plane;
			plane.m_nChildBone = nNode;
			plane.m_nParentBone = modelContext.MapJointToFxBone( bestPlane.m_nDmeJointParent, false );
			plane.m_flStickiness = 0;
			const CTransform &tmParent = m_pAuthFx->m_Nodes[ plane.m_nParentBone ].m_Transform;
			plane.m_Plane.m_vNormal = VectorRotate( bestPlane.m_WorldPlane.m_vNormal, Conjugate( tmParent.m_orientation ) );
			plane.m_Plane.m_flOffset = bestPlane.m_WorldPlane.m_flOffset - DotProduct( tmParent.m_vPosition, bestPlane.m_WorldPlane.m_vNormal );
			m_pAuthFx->m_CollisionPlanes.AddToTail( plane );
		}
	}
}


void CClothProxyCompiler::MarkFreeRotatingNodes( const CAuthPhysFx::CQuad &quad )
{
	// make sure the appropriate static nodes are marked as FreeRotating
	int nSimNodes = 0, nStaticNodes = 0;
	int nNodes = ( quad.m_nNodes[ 3 ] == quad.m_nNodes[ 2 ] ) ? 3 : 4;
	for ( int j = 0; j < nNodes; ++j )
	{
		if ( m_pAuthFx->m_Nodes[ quad.m_nNodes[ j ] ].m_bSimulated )
		{
			nSimNodes++;
		}
		else
		{
			nStaticNodes++;
		}
	}
	if ( nSimNodes >= 2 && nStaticNodes > 0 )
	{
		// we have a freely moving quad
		for ( int j = 0; j < nNodes; ++j )
		{
			CAuthPhysFx::CBone &node = m_pAuthFx->m_Nodes[ quad.m_nNodes[ j ] ];
			if ( !node.m_bSimulated )
			{
				node.m_bFreeRotation = true;
				node.m_bNeedNodeBase = true;
			}
		}
	}
}


void CClothProxyCompiler::AlignNodes()
{
	// align nodes that need node base : they can be arbitrary orientation, and we can choose the orientation that makes it cheaper to compute them for animation
	CVarBitVec needBase( m_pAuthFx->m_Nodes.Count() );
	for ( int nNode = 0; nNode < m_pAuthFx->m_Nodes.Count(); ++nNode )
	{
		if ( m_pAuthFx->m_Nodes[ nNode ].m_bNeedNodeBase )
		{
			needBase.Set( nNode );
		}
	}
	m_pAuthFx->AlignNodes( needBase );
}


void CClothProxyCompiler::Cook( )
{
	AlignNodes();
}



void CClothProxyCompiler::AppendDag( CModelContext &context, CDmeDag *pDag )
{
	if( CDmeMesh *pMesh = CastElement< CDmeMesh >( pDag->GetShape() ) )
	{
		matrix3x4_t tm;
		pDag->GetAbsTransform( tm );
		AppendMesh( context, pMesh, tm );
	}

	int nChildCount = pDag->GetChildCount();
	for ( int nChild = 0; nChild < nChildCount; ++nChild )
	{
		CDmeDag *pChild = pDag->GetChild( nChild );
		AppendDag( context, pChild );
	}
}



void Aggregate( Quaternion &q, const Quaternion &p )
{
	if ( QuaternionDotProduct( p, q ) > 0 )
	{
		q = q + p;
	
	}
	else
	{
		q = q - p;
	}
}




// each vertex potentially has multiple normals and tangents associated with it. Average those out to make a reasonable orientations

void ClothBoneOrientFromNormals( CClothProxyCompiler::CMeshContext &context )
{
	// we only have normals
	for ( int nPosIndex = 0; nPosIndex < context.m_AttrPos.m_IndexData.Count(); ++nPosIndex )
	{
		int nDmePos = context.m_AttrPos.m_IndexData[ nPosIndex ];
		if ( CAuthPhysFx::CBone *pFxBone = context.GetClothBone( nDmePos ) )
		{
			Aggregate( pFxBone->m_Transform.m_orientation, RotateBetween( Vector( 0, 0, 1 ), context.m_AttrNormal[ nPosIndex ] ) );
		}
	}
}



void ClothBoneOrientFromNormalsAndTangents( CClothProxyCompiler::CMeshContext &context )
{
	// we only have normals
	for ( int nPosIndex = 0; nPosIndex < context.m_AttrPos.m_IndexData.Count(); ++nPosIndex )
	{
		int nDmePos = context.m_AttrPos.m_IndexData[ nPosIndex ];
		if ( CAuthPhysFx::CBone *pFxBone = context.GetClothBone( nDmePos ) )
		{
			Vector vNormal = context.m_AttrNormal[ nPosIndex ];
			vNormal.NormalizeInPlace();
			Vector vTangent = context.m_AttrTangent[ nPosIndex ];
			vTangent = vTangent - DotProduct( vTangent, vNormal ) * vNormal;
			vTangent.NormalizeInPlace();
			Vector vCotangent = CrossProduct( vTangent, vNormal );
			matrix3x4_t tm;
			tm.InitXYZ( vCotangent, vTangent, vNormal, vec3_origin );
			Aggregate( pFxBone->m_Transform.m_orientation, MatrixQuaternion( tm ) );
		}
	}
}



void CClothProxyCompiler::OrientClothBones( CMeshContext &context )
{
	if ( !context.m_AttrNormal )
	{
		// TODO: implement computing a nice orientation from positions only
		return;
	}

	if ( !context.m_AttrTangent )
	{
		ClothBoneOrientFromNormals( context );
	}
	else
	{
		ClothBoneOrientFromNormalsAndTangents( context );
	}

	for ( int nDmePos = 0; nDmePos < context.m_DmePosToFxBone.Count(); ++nDmePos )
	{
		int nFxBone = context.m_DmePosToFxBone[ nDmePos ];
		if ( nFxBone >= 0 )
		{
			Quaternion &q = m_pAuthFx->m_Nodes[ nFxBone ].m_Transform.m_orientation;
			q = GetNormalized( q );
		}
	}
}




int CClothProxyCompiler::GetOrCreateClothRootBone()
{
	if ( m_nRootFxNode < 0 )
	{
		Assert( m_pAuthFx->FindNodeIndex( g_pDefaultClothRootBoneName ) < 0 );
		CAuthPhysFx::CBone root;
		root.m_Name = g_pDefaultClothRootBoneName;
		root.m_Transform = g_TransformIdentity;
		root.m_bSimulated = false;
		root.m_bFreeRotation = false;
		m_nRootFxNode = m_pAuthFx->m_Nodes.AddToTail( root );
	}
	return m_nRootFxNode;
}

CAuthPhysFx::CBone *CClothProxyCompiler::CMeshContext::GetClothBone( int nDmePos )
{
	int nFxBone = m_DmePosToFxBone[ nDmePos ];
	return nFxBone < 0 ? NULL : &m_pAuthFx->m_Nodes[ nFxBone ];
}


CAuthPhysFx::CBone *CClothProxyCompiler::CMeshContext::GetOrCreateClothBone( int nDmePos, bool bSimulated )
{
	return &m_pAuthFx->m_Nodes[ GetOrCreateClothBoneIndex( nDmePos, bSimulated ) ];
}

int CClothProxyCompiler::CMeshContext::GetClothBoneIndex( int nDmePos )
{
	return m_DmePosToFxBone[ nDmePos ];
}

int CClothProxyCompiler::CMeshContext::GetOrCreateClothBoneIndex( int nDmePos, bool bSimulated )
{
	int &refFxBone = m_DmePosToFxBone[ nDmePos ];
	if ( refFxBone < 0 )
	{
		CAuthPhysFx::CBone bone;
		bone.m_Name.Format( "$cloth_m%dp%d", m_nDmeMesh, nDmePos );
		bone.m_Transform.m_vPosition = VectorTransform( m_AttrPos.m_Data[ nDmePos ], m_MeshTransform );
		bone.m_Transform.m_orientation = quat_identity;
		bone.m_bSimulated = bSimulated; // by default, we don't want to make anything dynamic; only the nodes explicitly designated as dynamic should turn dynamic
		bone.m_bFreeRotation = bSimulated;
		bone.m_bVirtual = true; // all these nodes are virtual, they need a parent bone to drive them for animation attraction or rigid animation (or setting to animated pose) to work
		bone.m_bNeedNodeBase = bSimulated; // potentially, we need every simulated node to recompute the transform to drive vertex angents and normals. Unless we later find out this bone doesn't have any vertices bound, in which case we can reset it to false
		refFxBone = m_pAuthFx->m_Nodes.AddToTail( bone );
	}
	return refFxBone;
}

void CClothProxyCompiler::CreateClothBones( CMeshContext &context )
{
	context.m_DmePosToFxBone.SetCount( context.m_AttrPos.m_Data.Count() );
	context.m_DmePosToFxBone.FillWithValue( -1 );

	CIndexedAttr< float > attrCloth( context.m_pBindState, CDmeVertexDataBase::FIELD_CLOTH_ENABLE );

	if ( attrCloth && attrCloth.GetElementCount() == context.m_AttrPos.GetElementCount() )
	{
		int nDataCount = context.m_AttrPos.GetDataCount();
		CVarBitVec dmePosEnable( nDataCount );
		for ( int nIndex = 0; nIndex < attrCloth.GetElementCount(); ++nIndex )
		{
			float flEnable = attrCloth[ nIndex ];
			int nDmePos = context.m_AttrPos.m_IndexData[ nIndex ];
			dmePosEnable.Set( nDmePos );
			if ( flEnable >= context.m_pModelContext->m_flClothEnableThreshold ) // only create dynamic nodes at this time, we may or may not need the static nodes
			{
				context.GetOrCreateClothBone( nDmePos, true );
			}
		}
		int nPosEnableCount = dmePosEnable.PopulationCount();
		if ( nPosEnableCount != nDataCount )
		{
			Warning( "Mesh %s has cloth_enable data on %d of %d vertices only. Delete history and reexport?\n", context.m_pDmeMesh->GetName(), nPosEnableCount, nDataCount );
		}
	}
	else
	{
		Warning( "Mesh %s has no cloth_enable attribute, pinning one vertex\n", context.m_pDmeMesh->GetName() );
		// in a proxy mesh, we use the whole thing. We might as well just go ahead and create the bones
		for ( int nPos = 0; nPos < context.m_AttrPos.m_Data.Count(); ++nPos )
		{
			// cloth_enable may or may not be present. If it's not present, we assume it's all enabled but a single vertex (so that it doesn't fall down immediately, but shows to the artist that s|he forgot to create cloth_enable map)
			context.GetOrCreateClothBone( nPos, true );
		}

		int nPin = 0;
		// find the top vertex and mark it as static
		for ( int nPos = 1; nPos < context.m_AttrPos.GetDataCount(); ++nPos )
		{
			if ( context.m_AttrPos.m_Data[ nPos ].z > context.m_AttrPos.m_Data[ nPin ].z )
			{
				nPin = nPos;
			}
		}
		context.GetOrCreateClothBone( nPin, false ); // pin one vertex for the artist to see a piece of cloth hanging to remind that cloth_enable needs to be done
	}
}


CClothProxyCompiler::CModelContext::CModelContext( CClothProxyCompiler *pCompiler, CDmeModel *pModel, float flClothEnableThreshold, const CVClothProxyMesh &proxy ) : m_Proxy( proxy ), m_pModel( pModel )
{
	m_pAuthFx = pCompiler->GetFx();
	m_flClothEnableThreshold = flClothEnableThreshold;
	m_JointToBoneSubset.SetCount( pModel->GetJointCount() );
	int nModelJointCount = pModel->GetJointCount();
	m_JointToFxBone.SetCount( nModelJointCount );
	for ( int nJoint = 0; nJoint < nModelJointCount; ++nJoint )
	{
		const char *pJointName = pModel->GetJoint( nJoint )->GetName();
		m_JointToFxBone[ nJoint ] = m_pAuthFx->FindNodeIndex( pJointName );
	}
	

	for ( int nJoint = 0; nJoint < nModelJointCount; ++nJoint )
	{
		CDmeDag *pJoint = pModel->GetJoint( nJoint );
		UtlSymId_t &refSubset = m_JointToBoneSubset[ nJoint ];
		refSubset = pCompiler->m_BoneSubsets.Find( pJoint->GetName() );
		if ( refSubset == UTL_INVAL_SYMBOL )
		{
			refSubset = pCompiler->m_BoneSubsets.Insert( pJoint->GetName(), new CAuthFxBoneSubset( nJoint ) );
		}
	}
}


int CClothProxyCompiler::CModelContext::MapJointToFxBone( int nJoint, bool bSimulated )
{
	if ( nJoint < 0 )
		return -1;
	int &refFxBoneIndex = m_JointToFxBone[ nJoint ];
	if ( refFxBoneIndex < 0 )
	{
		CDmeDag *pJoint = m_pModel->GetJoint( nJoint );
		//backSolvedJoints.Set( nJoint ); // we are back-solving this one, if only with one single dynamic simulated vertex so far
		const char *pJointName = pJoint->GetName();
		if ( IsDebug() && bSimulated && !V_stricmp( pJointName, "pelvis" ) )
		{
			ExecuteOnce( Warning( "Including pelvis into simulated cloth bones; this is probably wrong, as pelvis will tilt depending on cloth movement\n" ) );
		}
		refFxBoneIndex = m_pAuthFx->FindNodeIndex( pJointName );
		if ( refFxBoneIndex < 0 )
		{
			// make a driven node
			CAuthPhysFx::CBone fxBone;
			fxBone.m_Name = pJoint->GetName();
			fxBone.m_bSimulated = bSimulated;
			// dynamic nodes are naturally free rotating
			// static nodes are not, by default. Unless we want to back-solve something and make it into a static joint. That's not implemented yet, but may be in the future for some reason.
			// in that case, we need to make those static nodes free-rotating here
			fxBone.m_bFreeRotation = bSimulated;
			fxBone.m_bForceSimulated = bSimulated;
			matrix3x4a_t tm;
			pJoint->GetAbsTransform( tm );
			fxBone.m_Transform = MatrixTransform( tm );
			refFxBoneIndex = m_pAuthFx->m_Nodes.AddToTail( fxBone );
		}
	}
	return refFxBoneIndex;
}



int CClothProxyCompiler::CMeshContext::FindMostBoundJoint( int nDmePos, float flBonusForExisting )
{
	int nBindCount = GetSkinningJointCount();
	if ( !nBindCount )
		return -1;
	// a static node. Find the joint that will drive it and add it as a static node
	const int *pIndices = m_pBindState->GetJointIndexData( nDmePos );
	const float *pWeight = m_pBindState->GetJointWeightData( nDmePos );

	int nBestIndex = -1;
	float flBestWeight = 0;

	for ( int i = 0; i < nBindCount; ++i )
	{
		float flWeight = pWeight[ i ];
		if ( m_pModelContext->m_JointToFxBone[ pIndices[ i ] ] >= 0 )
		{
			// the joint already exists, add the bonus points
			flWeight += flBonusForExisting;
		}
		if ( flWeight > flBestWeight )
		{
			nBestIndex = pIndices[ i ];
			flBestWeight = flWeight;
		}
	}
	return nBestIndex;
}


void CClothProxyCompiler::AddFitWeights( CMeshContext &context )
{
	int nBindCount = context.GetSkinningJointCount();
	if ( !nBindCount )
	{
		// cannot backsolve joints if vertices aren't bound to them
		return;
	}
	float flWeightThreshold = context.m_pModelContext->m_Proxy.m_flBackSolveInfluenceThreshold;

	// first pass: just find the dynamic nodes that may contribute to back-solve
	// jointToFxBone will be non--1  only for those joints that have dynamic influences (and never locked)

	CVarBitVec lockedJoints( context.m_pModelContext->GetJointCount() );
	bool bBackSolveNonClothJoints = context.m_pModelContext->m_Proxy.m_bFlexClothBorders;
	if ( !bBackSolveNonClothJoints )
	{
		CIndexedAttr< float > attrCloth( context.m_pBindState, CDmeVertexDataBase::FIELD_CLOTH_ENABLE );
		// don't try to back-solve joints that have ANY static influences, so find those first
		if ( attrCloth )
		{
			for ( int nIndex = 0; nIndex < attrCloth.GetElementCount(); ++nIndex )
			{
				float flEnable = attrCloth[ nIndex ];
				if ( flEnable < context.m_pModelContext->m_flClothEnableThreshold ) // only create dynamic nodes at this time, we may or may not need the static nodes
				{
					int nDmePos = context.m_AttrPos.m_IndexData[ nIndex ];
					// static node, skip all attached joints 
					// a static node. add to dynamic back-solve (no need to back-solve static-only nodes)

					const int *pIndices = context.m_pBindState->GetJointIndexData( nDmePos );
					const float *pWeight = context.m_pBindState->GetJointWeightData( nDmePos );
					for ( int nBinding = 0; nBinding < nBindCount; ++nBinding )
					{
						if ( pWeight[ nBinding ] >= flWeightThreshold )
						{
							lockedJoints.Set( pIndices[ nBinding ] );
						}
					}
				}
			}
		}
	}

	//CVarBitVec backSolvedJoints( nModelJointCount ); // those Dme Joints that will be back-solved (rotated and translated) by cloth movement

	for ( int nPos = 0; nPos < context.m_AttrPos.m_Data.Count(); ++nPos )
	{
		int nFxBone = context.m_DmePosToFxBone[ nPos ];
		if ( nFxBone < 0 )
			continue;
		CAuthPhysFx::CBone &bone = m_pAuthFx->m_Nodes[ nFxBone ];  // note: to find this as vertex in Maya, maya.xyz = valve.yzx; conversely, valve.xyz = maya.zxy
		if ( !bone.m_bSimulated )
			continue;
		// a simulated position. we may back-solve from it. 
		const int *pIndices = context.m_pBindState->GetJointIndexData( nPos );
		const float *pWeight = context.m_pBindState->GetJointWeightData( nPos );
		for ( int nBinding = 0; nBinding < nBindCount; ++nBinding )
		{
			FeFitInfluence_t inf;
			inf.flWeight = pWeight[ nBinding ];
			inf.nVertexNode = nFxBone;
			if ( inf.flWeight >= flWeightThreshold )
			{
				// non-negligible influence, must add for back-solving
				int nJoint = pIndices[ nBinding ];
				if ( lockedJoints[ nJoint ] )
					continue; // just don't add any influences here, we aren't trying to affect this joint

				// we have a fxBone. add influence on it
				inf.nMatrixNode = context.m_pModelContext->MapJointToFxBone( nJoint, true );
				m_pAuthFx->m_FitInfluences.AddToTail( inf );
			}
		}
	}

	if ( bBackSolveNonClothJoints )
	{
		// add the static node influences for those joints that already have dynamic influences.
		// if a joint has no dynamic influences at all we should just ignore it
		CVarBitVec pinnedFxBones( Max( m_pAuthFx->GetBoneCount(), 1 ) );
		for ( int nPos = 0; nPos < context.m_AttrPos.m_Data.Count(); ++nPos )
		{
			CAuthPhysFx::CBone *pFxBone = context.GetClothBone( nPos );
			if ( pFxBone && pFxBone->m_bSimulated )
				continue; // already went through dynamic nodes, skip all of them. We only need to look at the static nodes here

			// a static node. add to dynamic back-solve (no need to back-solve static-only nodes). We'll skip joints that don't get affected by any dynamic node

			const int *pIndices = context.m_pBindState->GetJointIndexData( nPos );
			const float *pWeight = context.m_pBindState->GetJointWeightData( nPos );
			for ( int nBinding = 0; nBinding < nBindCount; ++nBinding )
			{
				float flWeight = pWeight[ nBinding ];
				if ( flWeight < flWeightThreshold )
					continue;
				int nJoint = pIndices[ nBinding ];
				int nFxBoneIndex = context.m_pModelContext->m_JointToFxBone[ nJoint ];
				if ( nFxBoneIndex < 0 ) // is this Dme joint one of the Bones that have at least some dynamic cloth influence? if not, we don't want to build any influences on it
					continue;
				// ok , this STATIC vertex is bound to a bone that is also bound to some dynamic verts. It's useful to bind it to static verts, too, and let the solver decide how to fit the matrix to one set or the other
				FeFitInfluence_t inf;
				inf.flWeight = flWeight;
				int nVertexNode = context.GetOrCreateClothBoneIndex( nPos, false ); // 
				// this is a static node vertex, otherwise we would skip it at the start
				AssertDbg( !m_pAuthFx->m_Nodes[ nVertexNode ].m_bSimulated );
				inf.nVertexNode = nVertexNode;

				inf.nMatrixNode = nFxBoneIndex;
				if ( 0 )
				{
					// we have a fxBone. add the static influence on it. One way to do it is to effectively pin it with this single vert: if a bone has ANY non-cloth-enabled verts, moving it will move those verts, which is not an expected result from an artist perspective
					// so we may pin it with itself - so that it rotates, but doesn't move from its animated position
					// .. don't add anything extra
					if ( !pinnedFxBones[ nFxBoneIndex ] )
					{
						pinnedFxBones.Set( nFxBoneIndex );
						CAuthPhysFx::CBone& fxBone = m_pAuthFx->m_Nodes[ nFxBoneIndex ];
						fxBone.m_bSimulated = false;
						fxBone.m_bForceSimulated = false;
						fxBone.m_bFreeRotation = true;
						inf.flWeight = 1.0f;
						inf.nVertexNode = nFxBoneIndex;
						m_pAuthFx->m_FitInfluences.AddToTail( inf );
					}
				}
				else
				{
					// an alternative is to just add all influences and let the FeModelBuilder deal with it
					m_pAuthFx->m_FitInfluences.AddToTail( inf ); // influence this semi-dynamic bone, because we don't want totally free movement of that joint
				}
			}
		}
	}
	// also, we have to take into account connected static nodes: if a bone moves a dynamic vertex, which is connected to a static vertex, that static vertex should be in the set to influence the bone
	// the FeModelBuilder may choose the 3 most appropriate static influences to compute center of mass and orientation
}



void CClothProxyCompiler::BindNodeOffsetParents( CMeshContext &context )
{
	int nSkinningJointCount = context.GetSkinningJointCount() > 0;
	for ( int nDmePos = 0; nDmePos < context.m_AttrPos.m_Data.Count(); ++nDmePos )
	{
		CAuthPhysFx::CBone *pFxBone = context.GetClothBone( nDmePos );
		if ( !pFxBone )
			continue;
		// to always have an option to turn on animation attraction for dynamic nodes, run this for dynamic nodes, too
		//if ( pFxBone->m_bSimulated )
		//	continue; 

		if ( pFxBone->m_nParent >= 0 )
			continue; // we already bound this node to a parent, makes no sense to override it now

		// bind this vertex node to a real bone
		if ( nSkinningJointCount )
		{
			int nParentJoint = context.FindMostBoundJoint( nDmePos, 0.05f );
			if ( nParentJoint < 0 )
			{
				Vector p = VectorTransform( context.m_AttrPos.m_Data[ nDmePos ], context.m_MeshTransform );
				Warning( "Cannot find most-bound-joint for position %d {%.2f,%.2f,%.2f} in mesh %s\n", nDmePos, p.x, p.y, p.z, context.m_pDmeMesh->GetName() );
			}
			else
			{
				pFxBone->m_nParent = context.m_pModelContext->MapJointToFxBone( nParentJoint, false );
			}
		}
		else
		{
			pFxBone->m_nParent = GetOrCreateClothRootBone();
		}
	}
}


int CClothProxyCompiler::CMeshContext::GetSkinningJointCount()
{
	return this->m_pBindState->HasSkinningData() ? this->m_pBindState->JointCount() : 0;
}

void CClothProxyCompiler::CreateClothQuads( CMeshContext &context )
{
	int nBindCount = context.GetSkinningJointCount();
	CClothProxyCompiler *pCompiler = this;
	int nSkippedQuads = 0;

	CVarBitVec dynamicDmePos( context.m_DmePosToFxBone.Count() );
	Assert( context.m_AttrPos.GetDataCount() == context.m_DmePosToFxBone.Count() );
	for ( int nDmePos = 0; nDmePos < context.m_DmePosToFxBone.Count(); ++nDmePos )
	{
		CAuthPhysFx::CBone *pFxBone = context.GetClothBone( nDmePos );
		if ( pFxBone && pFxBone->m_bSimulated )
		{
			dynamicDmePos.Set( nDmePos );
		}
	}

	EnumerateFaces( context.m_pDmeMesh, context.m_pBindState, &dynamicDmePos,
		[ pCompiler, &context, nBindCount, &nSkippedQuads ]( int nPos[], int nCount )
	{
		Assert( nCount >= 3 && nCount <= 4 );
		CAuthPhysFx::CQuad quad;
		Vector v[ 4 ];
		int nDynamic = 0;
		for ( int j = 0; j < nCount; ++j )
		{
			int nPosVert = nPos[ j ];
			int nFxBone = context.GetOrCreateClothBoneIndex( nPosVert, false );
			if ( context.m_pAuthFx->m_Nodes[ nFxBone ].m_bSimulated )
				++nDynamic;
			quad.m_nNodes[ j ] = nFxBone;
			v[ j ] = context.m_pAuthFx->GetBone( nFxBone )->m_Transform.m_vPosition;
		}
		Assert( nDynamic > 0 ); NOTE_UNUSED( nDynamic );
		for ( int j = nCount; j < 4; ++j )
		{
			quad.m_nNodes[ j ] = quad.m_nNodes[ nCount - 1 ];
			v[ j ] = v[ nCount - 1 ];
		}
		Vector vNormal = CrossProduct( v[ 2 ] - v[ 0 ], v[ 3 ] - v[ 1 ] );
		if ( vNormal.LengthSqr() < 1e-10f )
		{
			nSkippedQuads++; // this quad is too small, let the artist fix the content
		}
		else
		{
			int nQuad = context.m_pAuthFx->m_Quads.AddToTail( quad );
			CUtlSortVector< int > arrBones;
			for ( int j = 0; j < nCount; ++j )
			{
				if ( nBindCount > 0 )
				{
					const int *pIndices = context.m_pBindState->GetJointIndexData( nPos[j] );
					const float *pWeight = context.m_pBindState->GetJointWeightData( nPos[j] );
					// this vertex is bound to some bones, attach the quad there
					for ( int nBinding = 0; nBinding < nBindCount; ++nBinding )
					{
						if ( pWeight[ nBinding ] > FLT_EPSILON )
						{
							int nIndex = pIndices[ nBinding ];
							Assert( nIndex >= 0 && nIndex < context.m_pModelContext->m_JointToBoneSubset.Count() );
							arrBones.InsertIfNotFound( nIndex );
						}
					}
				}
			}
			if ( arrBones.IsEmpty() )
			{
				// this isn't bound to bones. Attach to the default list
				pCompiler->m_DefaultSubset.m_Quads.AddToTail( ProjItem_t( nQuad, context.m_pModelContext->m_Proxy.m_flEnvelope ) );
			}
			else
			{
				for ( int nIndex : arrBones )
				{
					UtlSymId_t nSymBone = context.m_pModelContext->m_JointToBoneSubset[ nIndex ];
					CAuthFxBoneSubset *pSubset = pCompiler->m_BoneSubsets[ nSymBone ];
					pSubset->m_Quads.AddToTail( ProjItem_t( nQuad, context.m_pModelContext->m_Proxy.m_flEnvelope ) );
				}
			}
		}
	} );

	if ( nSkippedQuads )
	{
		Warning( "Skipping %d degenerate quads, %s\n", nSkippedQuads, context.m_pDmeMesh->GetName() );
	}
}



void CClothProxyCompiler::ApplyClothBoneAttributes( CMeshContext &context )
{
	CDmeVertexData *pBindState = context.m_pBindState;
	ClothAttributes < CIndexedAttr < float > > attr( [ pBindState ]( const char *pAttrName ) {  return CIndexedAttr< float >( pBindState, pAttrName ); } );
	Assert( context.m_AttrPos.GetDataCount() == pBindState->GetPositionData().Count() );
	CVarBitVec walkedPos( context.m_AttrPos.GetDataCount() );
	for ( int nIndex = 0; nIndex < context.m_AttrPos.GetElementCount(); ++nIndex )
	{
		int nDmePos = context.m_AttrPos.m_IndexData[ nIndex ];
		CAuthPhysFx::CBone *pFxBone = context.GetClothBone( nDmePos );
		if ( pFxBone && !walkedPos[ nDmePos ] )
		{
			attr.Apply( nIndex, *pFxBone );
			walkedPos.Set( nDmePos );
		}
	}
}



CClothProxyCompiler::CMeshContext::CMeshContext( CClothProxyCompiler *pCompiler, CModelContext *pModelContext, CDmeMesh *pMesh, const matrix3x4_t &tm, int nDmeMesh )
{
	m_pModelContext = pModelContext;
	m_nDmeMesh = nDmeMesh;
	m_pDmeMesh = pMesh;
	m_MeshTransform = tm;
	m_pCompiler = pCompiler;
	m_pAuthFx = pCompiler ? pCompiler->m_pAuthFx : NULL;
	m_pBindState = pMesh->GetBindBaseState();
	m_pBindState->Resolve(); // not sure what this does, but Get...Data functions seem to do this
	m_AttrPos.Init( m_pBindState, CDmeVertexData::FIELD_POSITION );
	m_AttrNormal.Init( m_pBindState, CDmeVertexData::FIELD_NORMAL );
	if ( m_AttrNormal && m_AttrNormal.GetElementCount() != m_AttrPos.GetElementCount() )
	{
		Warning( "Mesh %s unexpected normal element count %d, expected %d\n", pMesh->GetName(), m_AttrNormal.GetElementCount(), m_AttrPos.GetElementCount() );
		m_AttrNormal.Reset();
	}
	else
	{
		m_AttrTangent.Init( m_pBindState, CDmeVertexData::FIELD_TANGENT );
		if ( m_AttrTangent && m_AttrTangent.GetElementCount() != m_AttrPos.GetElementCount() )
		{
			Warning( "Mesh %s unexpected tangent element count %d, expected %d\n", pMesh->GetName(), m_AttrTangent.GetElementCount(), m_AttrPos.GetElementCount() );
			m_AttrTangent.Reset();
		}
	}

	
}


void CClothProxyCompiler::AppendMesh( CModelContext &modelContext, CDmeMesh *pMesh, const matrix3x4_t &tm )
{
	CMeshContext context( this, &modelContext, pMesh, tm, m_nProxyMeshes++ );
	if ( !context.m_AttrPos )
	{
		Warning( "No position data found in mesh %s\n", pMesh->GetName() );
		return; // something's wrong if we have no positions
	}
	m_MeshNames.AddToTail( pMesh->GetName() );

	CreateClothBones( context );
	if ( modelContext.m_Proxy.m_bBackSolveJoints || m_Options.m_bDriveMeshesWithBacksolvedJointsOnly )
	{
		// back-solve joints from the created simulated bones
		AddFitWeights( context );
	}		  

	// this may create the last static nodes for those quads that connect static nodes to dynamic nodes
	CreateClothQuads( context );

	// apply attributes and orientations must be last, after all the Fx nodes have been created as necessary
	// they only make sense for skinned cloth, though
	BindNodeOffsetParents( context );
	// OrientClothBones( context ); <-- cloth bones that can be oriented, will be oriented according to easy computation with FeNodeBase
	ApplyClothBoneAttributes( context ); 
}

/*
void CClothProxyCompiler::Bake( CAuthPhysFx *pFx )
{
	// go through collected bones, find the used ones and add them and their corresponding polygons to AuthFx
}

void CClothProxyCompiler::CAuthFxSubset::Append( const CAuthFxSubset &other )
{
	
}
*/

int CClothProxyCompiler::GetAuthFxBone( const char *pName )
{
	return m_pAuthFx->FindNodeIndex( pName ); // TODO: maybe replace this with a faster lookup
}



bool CClothProxyCompiler::Project( const Vector &vPos, UtlSymId_t *pFindSubset, int nFindSubsetCount, CUtlVector<Binding_t> &outBindings, int nIslandFilter )
{
	CUtlSortVector< QuadProjection_t > bestProj;

	CVarBitVec visitedQuads( m_pAuthFx->GetQuadCount() );
	for ( int nSubsetIndex = 0; nSubsetIndex < nFindSubsetCount; ++nSubsetIndex )
	{
		UtlSymId_t nFxBone = pFindSubset[ nSubsetIndex ];
		CAuthFxBoneSubset *pSubset = m_BoneSubsets[ nFxBone ];
		for ( int nPrIt = 0; nPrIt < pSubset->m_Quads.Count(); ++nPrIt )
		{
			ProjItem_t q = pSubset->m_Quads[ nPrIt ];
			if ( visitedQuads.IsBitSet( q.nIndex ) )
				continue;
			visitedQuads.Set( q.nIndex );
			ProjectAndAddToQueue( q, vPos, bestProj, nIslandFilter );
		}
	}

	for ( int nPrIt = 0; nPrIt < m_DefaultSubset.m_Quads.Count(); ++nPrIt )
	{
		ProjItem_t q = m_DefaultSubset.m_Quads[ nPrIt ];
		if ( visitedQuads.IsBitSet( q.nIndex ) )
			continue;
		visitedQuads.Set( q.nIndex );
		ProjectAndAddToQueue( q, vPos, bestProj, nIslandFilter );
	}


	if ( bestProj.Count() >= 2 && bestProj[ 0 ].m_flDistance < 0 && bestProj[ 1 ].m_flDistance < 0 && DotProduct( bestProj[ 0 ].m_vNormal, bestProj[ 1 ].m_vNormal ) < 0 )
	{
		// we're smack inside some volume
		bestProj.SetCountNonDestructively( 2 );
	}
	else if ( bestProj.Count() > 1 )
	{
		bestProj.SetCountNonDestructively( 1 );
	}

	int nAddedBindings = 0;
	for ( int i = 0; i < bestProj.Count(); ++i )
	{
		for ( int j = 0; j < bestProj[ i ].m_nBindings; ++j )
		{
			outBindings.AddToTail( bestProj[ i ].m_Binding[ j ] );
			++nAddedBindings;
		}
	}

	return nAddedBindings > 0;
}

CClothProxyCompiler::QuadProjection_t CClothProxyCompiler::ProjectOnQuad( const Vector &vPos, const CAuthPhysFx::CQuad &quad )
{
	if ( quad.m_nNodes[ 3 ] == quad.m_nNodes[ 2 ] )
		return ProjectOnTri( vPos, quad );

	QuadProjection_t out;
	Vector v[ 4 ], e[4];
	for ( int i = 0; i < 4; ++i )
	{
		v[ i ] = m_pAuthFx->GetBone( quad.m_nNodes[ i ] )->m_Transform.m_vPosition;
	}
	float flEdgeLen[ 4 ];
	for ( int i = 0; i < 4; ++i )
	{
		e[ i ] = v[ ( i + 1 ) % 4 ] - v[ i ];
		flEdgeLen[ i ] = e[ i ].Length();
		e[ i ] /= flEdgeLen[ i ];
	}

	Vector vNormal = CrossProduct( v[ 2 ] - v[ 0 ], v[ 3 ] - v[ 1 ] ).Normalized();
	float flSide[ 4 ];
	for ( int i = 0; i < 4; ++i )
	{
		flSide[ i ] = DotProduct( vPos - v[ i ], CrossProduct( vNormal, e[i] ).Normalized() );
	}

	Assert( !( flSide[ 0 ] < 0 && flSide[ 1 ] < 0 && flSide[ 2 ] < 0 && flSide[ 3 ] < 0 ) );
	if ( ( flSide[ 0 ] > 0 && flSide[ 1 ] > 0 && flSide[ 2 ] > 0 && flSide[ 3 ] > 0 ) )
	{
		// we project onto the interior of the quad, more or less. Form the approximate weights, find approximate distance
		float flEven = flSide[ 0 ] / ( flSide[ 0 ] + flSide[ 2 ] ), flOdd = flSide[ 1 ] / ( flSide[ 1 ] + flSide[ 3 ] );
		out.m_nBindings = 4;
		out.m_Binding[ 0 ].nAuthFxBone = quad.m_nNodes[ 0 ];
		out.m_Binding[ 0 ].flWeight = flOdd * ( 1 - flEven );
		out.m_Binding[ 1 ].nAuthFxBone = quad.m_nNodes[ 1 ];
		out.m_Binding[ 1 ].flWeight = ( 1 - flOdd ) * ( 1 - flEven );
		out.m_Binding[ 2 ].nAuthFxBone = quad.m_nNodes[ 2 ];
		out.m_Binding[ 2 ].flWeight = ( 1 - flOdd ) * flEven;
		out.m_Binding[ 3 ].nAuthFxBone = quad.m_nNodes[ 3 ];
		out.m_Binding[ 3 ].flWeight = flOdd * flEven;
		out.m_vNormal = vNormal;
		out.m_vContact = v[ 0 ] * out.m_Binding[ 0 ].flWeight + v[ 1 ] * out.m_Binding[ 1 ].flWeight + v[ 2 ] * out.m_Binding[ 2 ].flWeight + v[ 3 ] * out.m_Binding[ 3 ].flWeight;//vPos - vNormal * out.m_flDistance;
		out.m_flDistance = ( vPos - out.m_vContact ).Length();
		return out;
	}

	// project onto each edge interior. The closest one wins
	out.m_nBindings = 0;
	out.m_flDistance = FLT_MAX;
	for ( int i = 0; i < 4; ++i )
	{
		float flEdgeProj = DotProduct( vPos - v[ i ], e[ i ] );
		flEdgeProj = Clamp( flEdgeProj, 0.0f, flEdgeLen[ i ] );
		Vector vContact = v[ i ] + flEdgeProj * e[ i ];
		Vector vContactToPos = vPos - vContact;
		float flDistance = vContactToPos.Length();
		if ( !out.m_nBindings || flDistance < out.m_flDistance )
		{
			float w = flEdgeProj / flEdgeLen[ i ];
			out.m_flDistance = flDistance;
			out.m_vContact = vContact;
			out.m_vNormal = flDistance > FLT_EPSILON ? vContactToPos / flDistance : m_pAuthFx->GetBone(quad.m_nNodes[ i ] )->m_Transform.m_orientation.GetUp();
			out.m_nBindings = 0;

			if ( w < 0.999f )
			{
				out.AddBinding( quad.m_nNodes[ i ], 1.0f - w );
			}
			if ( w > 0.001f )
			{
				out.AddBinding( quad.m_nNodes[ ( i + 1 ) % 4 ], w );
			}
		}
	}
	return out;
}


CClothProxyCompiler::QuadProjection_t CClothProxyCompiler::ProjectOnTri( const Vector &vPos, const CAuthPhysFx::CQuad &quad )
{
	Assert( quad.m_nNodes[ 3 ] == quad.m_nNodes[ 2 ] );

	QuadProjection_t out;
	Vector v[ 3 ], e[ 3 ];
	for ( int i = 0; i < 3; ++i )
	{
		v[ i ] = m_pAuthFx->GetBone( quad.m_nNodes[ i ] )->m_Transform.m_vPosition;
	}
	float flEdgeLen[ 3 ];
	for ( int i = 0; i < 3; ++i )
	{
		e[ i ] = v[ ( i + 1 ) % 3 ] - v[ i ];
		flEdgeLen[ i ] = e[ i ].Length();
		e[ i ] /= flEdgeLen[ i ];
	}

	Vector vNormal = CrossProduct( v[ 1 ] - v[ 0 ], v[ 2 ] - v[ 0 ] ).Normalized();
	float flSide[ 3 ], flOpposing[ 3 ];
	for ( int i = 0; i < 3; ++i )
	{
		Vector vInSide = CrossProduct( vNormal, e[ i ] ).Normalized();
		flSide[ i ] = DotProduct( vPos - v[ i ], vInSide );
		flOpposing[ i ] = DotProduct( v[ ( i + 2 ) % 3 ] - v[ i ], vInSide );
	}

	Assert( !( flSide[ 0 ] < 0 && flSide[ 1 ] < 0 && flSide[ 2 ] < 0 ) );
	if ( ( flSide[ 0 ] > 0 && flSide[ 1 ] > 0 && flSide[ 2 ] > 0 ) )
	{
		// we project onto the interior of the quad, more or less. Form the approximate weights, find approximate distance
		out.m_nBindings = 3;
		out.m_Binding[ 0 ].nAuthFxBone = quad.m_nNodes[ 0 ];
		out.m_Binding[ 0 ].flWeight = Clamp( flSide[ 1 ] / flOpposing[ 1 ], 0.0f, 1.0f );
		out.m_Binding[ 1 ].nAuthFxBone = quad.m_nNodes[ 1 ];
		out.m_Binding[ 1 ].flWeight = Clamp( flSide[ 2 ] / flOpposing[ 2 ], 0.0f, 1.0f );
		out.m_Binding[ 2 ].nAuthFxBone = quad.m_nNodes[ 2 ];
		out.m_Binding[ 2 ].flWeight = Clamp( flSide[ 0 ] / flOpposing[ 0 ], 0.0f, 1.0f );
		out.m_vNormal = vNormal;
		out.m_vContact = v[ 0 ] * out.m_Binding[ 0 ].flWeight + v[ 1 ] * out.m_Binding[ 1 ].flWeight + v[ 2 ] * out.m_Binding[ 2 ].flWeight;
		out.m_flDistance = ( vPos - out.m_vContact ).Length();
		return out;
	}

	// project onto each edge interior. The closest one wins
	out.m_nBindings = 0;
	out.m_flDistance = FLT_MAX;
	for ( int i = 0; i < 3; ++i )
	{
		float flEdgeProj = DotProduct( vPos - v[ i ], e[ i ] );
		flEdgeProj = Clamp( flEdgeProj, 0.0f, flEdgeLen[ i ] );
		Vector vContact = v[ i ] + flEdgeProj * e[ i ];
		Vector vContactToPos = vPos - vContact;
		float flDistance = vContactToPos.Length();
		if ( flDistance < out.m_flDistance )
		{
			float w = flEdgeProj / flEdgeLen[ i ];
			out.m_flDistance = flDistance;
			out.m_vContact = vContact;
			out.m_vNormal = flDistance > FLT_EPSILON ? vContactToPos / flDistance : vNormal;
			out.m_nBindings = 0;

			if ( w < 0.999f )
			{
				out.AddBinding( quad.m_nNodes[ i ], 1.0f - w );
			}
			if ( w > 0.001f )
			{
				out.AddBinding( quad.m_nNodes[ ( i + 1 ) % 3 ], w );
			}
		}
	}
	return out;
}

void CClothProxyCompiler::ProjectAndAddToQueue( const ProjItem_t &q, const Vector & vPos, CUtlSortVector< QuadProjection_t > &bestProj, int nIslandFilter )
{
	CAuthPhysFx::CQuad &quad = m_pAuthFx->m_Quads[ q.nIndex ];
	
	if ( nIslandFilter >= 0 && NodeToIsland( quad.m_nNodes[ 0 ] ) != nIslandFilter )
		return;

	// project onto this quad, find the best projection
	QuadProjection_t proj = ProjectOnQuad( vPos, quad );
	if ( !proj.IsEmpty() && proj.m_flDistance <= q.flEnvelope )
	{
		bestProj.Insert( proj );
		if ( bestProj.Count() > 2 )
		{
			bestProj.SetCountNonDestructively( 2 ); // we only need to keep 2 entries
		}
	}
}


int CClothProxyCompiler::NodeToIsland( int nNode )
{
	AssertDbg( nNode >= 0 && nNode < m_pAuthFx->GetBoneCount() );
	if ( m_Options.m_flMatchProxiesToMeshes <= 0 )
	{
		if ( m_NodeToIslandMap.Count() != m_pAuthFx->GetBoneCount() )
		{
			m_nIslandCount = m_pAuthFx->BuildIslandMap( m_NodeToIslandMap );
		}
		return m_NodeToIslandMap[ nNode ];
	}
	else
	{
		return 0;
	}
}


CLockedResource< PhysFeModelDesc_t > CClothProxyCompiler::Compile( CResourceStream *pStream )const
{
	return m_pAuthFx->Compile( pStream, &m_Options );
}
