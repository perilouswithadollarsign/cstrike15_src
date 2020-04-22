//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "bone_merge_cache.h"
#include "bone_setup.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// CBoneMergeCache
//-----------------------------------------------------------------------------

CBoneMergeCache::CBoneMergeCache()
{
	Init( NULL );
}

void CBoneMergeCache::Init( CBaseAnimating *pOwner )
{
	m_pOwner = pOwner;
	m_pFollow = NULL;
	m_pFollowHdr = NULL;
	m_pFollowRenderHdr = NULL;
	m_pOwnerHdr = NULL;
	m_pFollowRenderHdr = NULL;
	m_nFollowBoneSetupMask = 0;
	m_bForceCacheClear = false;
	m_MergedBones.Purge();
	V_memset( m_iRawIndexMapping, 0xFF, sizeof( m_iRawIndexMapping ) );	// initialize to -1s
}

void CBoneMergeCache::UpdateCache()
{
	if ( !m_pOwner )
		return;

	CStudioHdr *pOwnerHdr = m_pOwner->GetModelPtr();
	if ( !pOwnerHdr )
		return;
	const studiohdr_t *pOwnerRenderHdr = pOwnerHdr->GetRenderHdr();

	CBaseAnimating *pFollow = m_pOwner->FindFollowedEntity();
	CStudioHdr *pFollowHdr = (pFollow ? pFollow->GetModelPtr() : NULL);
	const studiohdr_t *pFollowRenderHdr = (pFollowHdr ? pFollowHdr->GetRenderHdr() : NULL );

	// if the follow parent has changed, or any of the underlying models has changed, reset the MergedBones list
	if ( pFollow != m_pFollow || pFollowRenderHdr != m_pFollowRenderHdr || pOwnerRenderHdr != m_pOwnerRenderHdr || m_bForceCacheClear )
	{
		m_MergedBones.Purge();

		m_bForceCacheClear = false;
	
		// Update the cache.
		if ( pFollow && pFollowHdr && pOwnerHdr )
		{
			m_pFollow = pFollow;
			m_pFollowHdr = pFollowHdr;
			m_pFollowRenderHdr = pFollowRenderHdr;
			m_pOwnerHdr = pOwnerHdr;
			m_pOwnerRenderHdr = pOwnerRenderHdr;

			m_BoneMergeBits.Resize( pOwnerHdr->numbones() );
			m_BoneMergeBits.ClearAll();

			const mstudiobone_t *pOwnerBones = m_pOwnerHdr->pBone( 0 );
			
			m_nFollowBoneSetupMask = BONE_USED_BY_BONE_MERGE;
			for ( int i = 0; i < m_pOwnerHdr->numbones(); i++ )
			{
				int parentBoneIndex = Studio_BoneIndexByName( m_pFollowHdr, pOwnerBones[i].pszName() );
				if ( parentBoneIndex < 0 )
					continue;

				m_iRawIndexMapping[i] = parentBoneIndex;

				// Add a merged bone here.
				CMergedBone mergedBone;
				mergedBone.m_iMyBone = i;
				mergedBone.m_iParentBone = parentBoneIndex;
				m_MergedBones.AddToTail( mergedBone );
				m_BoneMergeBits.Set( i );

				// flag bones used in merge so that they'll always be setup
				if ( ( m_pFollowHdr->boneFlags( parentBoneIndex ) & BONE_USED_BY_BONE_MERGE ) == 0 )
				{
					// go ahead and mark the bone and its parents
					int n = parentBoneIndex;
					while (n != -1)
					{
						m_pFollowHdr->setBoneFlags( n, BONE_USED_BY_BONE_MERGE );
						n = m_pFollowHdr->boneParent( n );
					}
				}

				// FIXME: only do this if it's for a "reverse" merge
				if ( ( m_pOwnerHdr->boneFlags( i ) & BONE_USED_BY_BONE_MERGE ) == 0 )
				{
					// go ahead and mark the bone and its parents
					int n = i;
					while (n != -1)
					{
						m_pOwnerHdr->setBoneFlags( n, BONE_USED_BY_BONE_MERGE );
						n = m_pOwnerHdr->boneParent( n );
					}
				}
			}

			// No merged bones found? Slam the mask to 0
			if ( !m_MergedBones.Count() )
			{
				m_nFollowBoneSetupMask = 0;
			}

			// find and record pose params that match by name
			for ( int i = 0; i < MAXSTUDIOPOSEPARAM; i++ )
			{
				// init mapping as invalid
				m_nOwnerToFollowPoseParamMapping[i] = -1;
				
				if ( i < m_pFollowHdr->GetNumPoseParameters() )
				{
					// get the follower's pose param name for this index
					const char * szFollowerPoseParamName = m_pFollowHdr->pPoseParameter( i ).pszName();

					// find it on the owner
					for ( int n = 0; n < MAXSTUDIOPOSEPARAM && n < m_pOwnerHdr->GetNumPoseParameters(); n++ )
					{
						const char * szOwnerPoseParamName = m_pOwnerHdr->pPoseParameter( n ).pszName();
						if ( !Q_strcmp( szFollowerPoseParamName, szOwnerPoseParamName ) )
						{
							//match
							m_nOwnerToFollowPoseParamMapping[i] = n;
							break;
						}
					}
				}
			}

		}
		else
		{
			Init( m_pOwner );
		}
	}
}

void CBoneMergeCache::MergeMatchingPoseParams( void )
{
	UpdateCache();

	// If this is set, then all the other cache data is set.
	if ( !m_pOwnerHdr || m_MergedBones.Count() == 0 )
		return;

	// set follower pose params using mapped indices from owner
	for ( int i = 0; i < MAXSTUDIOPOSEPARAM; i++ )
	{
		if ( m_nOwnerToFollowPoseParamMapping[i] != -1 )
		{
			m_pOwner->SetPoseParameter( m_nOwnerToFollowPoseParamMapping[i], m_pFollow->GetPoseParameter( i ) );
		}
	}
}

#ifdef CLIENT_DLL
void CBoneMergeCache::MergeMatchingBones( int boneMask )
{
	UpdateCache();

	// If this is set, then all the other cache data is set.
	if ( !m_pOwnerHdr || m_MergedBones.Count() == 0 )
		return;

	// Have the entity we're following setup its bones.

	int nTempMask = m_nFollowBoneSetupMask;
	if ( m_pFollow->IsPlayer() )
	{
		// if the parent is a player, respect the incoming bone mask plus attachments,
		nTempMask = ( boneMask | BONE_USED_BY_ATTACHMENT );
		
		// but only use the custom blending rule portion
		if ( m_pFollow->m_nCustomBlendingRuleMask != -1 )
			nTempMask &= m_pFollow->m_nCustomBlendingRuleMask;
	}
	
	m_pFollow->SetupBones( NULL, -1, nTempMask, gpGlobals->curtime );

	// Now copy the bone matrices.
	for ( int i=0; i < m_MergedBones.Count(); i++ )
	{
		int iOwnerBone = m_MergedBones[i].m_iMyBone;
		int iParentBone = m_MergedBones[i].m_iParentBone;
		
		// Only update bones reference by the bone mask.
		if ( !( m_pOwnerHdr->boneFlags( iOwnerBone ) & boneMask ) )
			continue;

		MatrixCopy( m_pFollow->GetBone( iParentBone ), m_pOwner->GetBoneForWrite( iOwnerBone ) );
	}
}
#endif

#ifndef CLIENT_DLL
void CBoneMergeCache::BuildMatricesWithBoneMerge( 
	const CStudioHdr *pStudioHdr,
	const QAngle& angles, 
	const Vector& origin, 
	const Vector pos[MAXSTUDIOBONES],
	const Quaternion q[MAXSTUDIOBONES],
	matrix3x4_t bonetoworld[MAXSTUDIOBONES],
	CBaseAnimating *pParent,
	CBoneCache *pParentCache,
	int boneMask
	)
{

	UpdateCache();


	bool bMergedBone[ MAXSTUDIOBONES ];
	memset( &bMergedBone, 0, sizeof(bool) * MAXSTUDIOBONES );


	for ( int i=0; i < m_MergedBones.Count(); i++ )
	{
		int iOwnerBone = m_MergedBones[i].m_iMyBone;
		int iParentBone = m_MergedBones[i].m_iParentBone;

		if ( iParentBone >= 0 )
		{
			// Only update bones reference by the bone mask.
			if ( !( pStudioHdr->boneFlags( iOwnerBone ) & boneMask ) )
				continue;

			matrix3x4_t *pMat = pParentCache->GetCachedBone( iParentBone );
			if ( pMat )
			{
				MatrixCopy( *pMat, bonetoworld[ iOwnerBone ] );
				bMergedBone[iOwnerBone] = true;
			}
		}

	}


	const mstudiobone_t *pbones = pStudioHdr->pBone( 0 );
	
	matrix3x4_t rotationmatrix; // model to world transformation
	AngleMatrix( angles, origin, rotationmatrix);

	for ( int i=0; i < pStudioHdr->numbones(); i++ )
	{
		if ( !bMergedBone[i] )
		{
			// If we get down here, then the bone wasn't merged.
			matrix3x4_t bonematrix;
			QuaternionMatrix( q[i], pos[i], bonematrix );
	
			if (pbones[i].parent == -1) 
			{
				ConcatTransforms (rotationmatrix, bonematrix, bonetoworld[i]);
			} 
			else 
			{
				ConcatTransforms (bonetoworld[pbones[i].parent], bonematrix, bonetoworld[i]);
			}
		}
	}

	//CStudioHdr *fhdr = pParent->GetModelPtr();
	//const mstudiobone_t *pbones = pStudioHdr->pBone( 0 );
	//
	//matrix3x4_t rotationmatrix; // model to world transformation
	//AngleMatrix( angles, origin, rotationmatrix);
	//
	//for ( int i=0; i < pStudioHdr->numbones(); i++ )
	//{
	//	// Now find the bone in the parent entity.
	//	bool merged = false;
	//	int parentBoneIndex = Studio_BoneIndexByName( fhdr, pbones[i].pszName() );
	//	if ( parentBoneIndex >= 0 )
	//	{
	//		matrix3x4_t *pMat = pParentCache->GetCachedBone( parentBoneIndex );
	//		if ( pMat )
	//		{
	//			MatrixCopy( *pMat, bonetoworld[ i ] );
	//			merged = true;
	//		}
	//	}
	//
	//	if ( !merged )
	//	{
	//		// If we get down here, then the bone wasn't merged.
	//		matrix3x4_t bonematrix;
	//		QuaternionMatrix( q[i], pos[i], bonematrix );
	//
	//		if (pbones[i].parent == -1) 
	//		{
	//			ConcatTransforms (rotationmatrix, bonematrix, bonetoworld[i]);
	//		} 
	//		else 
	//		{
	//			ConcatTransforms (bonetoworld[pbones[i].parent], bonematrix, bonetoworld[i]);
	//		}
	//	}
	//}


}
#endif

void CBoneMergeCache::CopyFromFollow( const BoneVector followPos[], const BoneQuaternion followQ[], int boneMask, BoneVector myPos[], BoneQuaternion myQ[] )
{
	UpdateCache();

	// If this is set, then all the other cache data is set.
	if ( !m_pOwnerHdr || m_MergedBones.Count() == 0 )
		return;

	// Now copy the bone matrices.
	for ( int i=0; i < m_MergedBones.Count(); i++ )
	{
		int iOwnerBone = m_MergedBones[i].m_iMyBone;
		int iParentBone = m_MergedBones[i].m_iParentBone;
		
		// Only update bones reference by the bone mask.
		if ( !( m_pOwnerHdr->boneFlags( iOwnerBone ) & boneMask ) )
			continue;

		myPos[ iOwnerBone ] = followPos[ iParentBone ];
		myQ[ iOwnerBone ] = followQ[ iParentBone ];
	}
}

void CBoneMergeCache::CopyToFollow( const BoneVector myPos[], const BoneQuaternion myQ[], int boneMask, BoneVector followPos[], BoneQuaternion followQ[] )
{
	UpdateCache();

	// If this is set, then all the other cache data is set.
	if ( !m_pOwnerHdr || m_MergedBones.Count() == 0 )
		return;

	// Now copy the bone matrices.
	for ( int i=0; i < m_MergedBones.Count(); i++ )
	{
		int iOwnerBone = m_MergedBones[i].m_iMyBone;
		int iParentBone = m_MergedBones[i].m_iParentBone;
		
		// Only update bones reference by the bone mask.
		if ( !( m_pOwnerHdr->boneFlags( iOwnerBone ) & boneMask ) )
			continue;

		followPos[ iParentBone ] = myPos[ iOwnerBone ];
		followQ[ iParentBone ] = myQ[ iOwnerBone ];
	}
	m_nCopiedFramecount = gpGlobals->framecount;
}

bool CBoneMergeCache::IsCopied( )
{
	return (m_nCopiedFramecount == gpGlobals->framecount);
}


bool CBoneMergeCache::GetAimEntOrigin( Vector *pAbsOrigin, QAngle *pAbsAngles )
{
	UpdateCache();

	// If this is set, then all the other cache data is set.
	if ( !m_pOwnerHdr || m_MergedBones.Count() == 0 )
		return false;

	// We want the abs origin such that if we put the entity there, the first merged bone
	// will be aligned. This way the entity will be culled in the correct position.
	//
	// ie: mEntity * mBoneLocal = mFollowBone
	// so: mEntity = mFollowBone * Inverse( mBoneLocal )
	//
	// Note: the code below doesn't take animation into account. If the attached entity animates
	// all over the place, then this won't get the right results.
	
	// FIXME: we're merged onto a dead player that's likely ragdolled all over the place.
	// The parent's cached position and angles are dirty, but the absorigin and absangles are good enough.
	// Returning false here means the uncached parent origin/angles will be used.
	if ( m_pFollow->IsPlayer() && !m_pFollow->IsAlive() )
		return false;

	// Get mFollowBone.
#ifdef CLIENT_DLL
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
	m_pFollow->SetupBones( NULL, -1, m_nFollowBoneSetupMask, gpGlobals->curtime );
	const matrix3x4_t &mFollowBone = m_pFollow->GetBone( m_MergedBones[0].m_iParentBone );
#else
	m_pFollow->SetupBones( NULL, m_nFollowBoneSetupMask );

	matrix3x4_t mFollowBone;
	m_pFollow->GetBoneTransform( m_MergedBones[0].m_iParentBone, mFollowBone );
#endif

	// Get Inverse( mBoneLocal )
	matrix3x4_t mBoneLocal, mBoneLocalInv;
	SetupSingleBoneMatrix( m_pOwnerHdr, m_pOwner->GetSequence(), 0, m_MergedBones[0].m_iMyBone, mBoneLocal );
	MatrixInvert( mBoneLocal, mBoneLocalInv );

	// Now calculate mEntity = mFollowBone * Inverse( mBoneLocal )
	matrix3x4_t mEntity;
	ConcatTransforms( mFollowBone, mBoneLocalInv, mEntity );
	MatrixAngles( mEntity, *pAbsAngles, *pAbsOrigin );

	return true;
}

bool CBoneMergeCache::GetRootBone( matrix3x4_t &rootBone )
{
	UpdateCache();

	// If this is set, then all the other cache data is set.
	if ( !m_pOwnerHdr || m_MergedBones.Count() == 0 )
		return false;

	// Get mFollowBone.
#ifdef CLIENT_DLL
	m_pFollow->SetupBones( NULL, -1, m_nFollowBoneSetupMask, gpGlobals->curtime );
	rootBone = m_pFollow->GetBone( m_MergedBones[0].m_iParentBone );
#else
	m_pFollow->SetupBones( NULL, m_nFollowBoneSetupMask );
	m_pFollow->GetBoneTransform( m_MergedBones[0].m_iParentBone, rootBone );
#endif

	return true;
}
