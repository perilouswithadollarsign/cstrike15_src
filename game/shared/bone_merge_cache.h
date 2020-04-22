//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef BONE_MERGE_CACHE_H
#define BONE_MERGE_CACHE_H
#ifdef _WIN32
#pragma once
#endif

#if defined( CLIENT_DLL )
#undef CBaseAnimating
#define CBaseAnimating C_BaseAnimating
#endif

class CBaseAnimating;
class CStudioHdr;
struct studiohdr_t;

#ifndef CLIENT_DLL
#include "bone_setup.h"
#endif

#include "mathlib/vector.h"


class CBoneMergeCache
{
public:

	CBoneMergeCache();
	
	void Init( CBaseAnimating *pOwner );

	// Updates the lookups that let it merge bones quickly.
	void UpdateCache();
	
	// This copies the transform from all bones in the followed entity that have 
	// names that match our bones.
#ifdef CLIENT_DLL
	void MergeMatchingBones( int boneMask );
#else
	void BuildMatricesWithBoneMerge( const CStudioHdr *pStudioHdr, const QAngle& angles, 
		const Vector& origin, const Vector pos[MAXSTUDIOBONES],
		const Quaternion q[MAXSTUDIOBONES], matrix3x4_t bonetoworld[MAXSTUDIOBONES],
		CBaseAnimating *pParent, CBoneCache *pParentCache, int boneMask );
#endif
	

	void MergeMatchingPoseParams( void );

	void CopyFromFollow( const BoneVector followPos[], const BoneQuaternion followQ[], int boneMask, BoneVector myPos[], BoneQuaternion myQ[] );
	void CopyToFollow( const BoneVector myPos[], const BoneQuaternion myQ[], int boneMask, BoneVector followPos[], BoneQuaternion followQ[] );
	bool IsCopied( );

	// Returns true if the specified bone is one that gets merged in MergeMatchingBones.
	int IsBoneMerged( int iBone ) const;

	// Gets the origin for the first merge bone on the parent.
	bool GetAimEntOrigin( Vector *pAbsOrigin, QAngle *pAbsAngles );
	bool GetRootBone( matrix3x4_t &rootBone );

	void ForceCacheClear( void ) { m_bForceCacheClear = true; UpdateCache(); }

	const unsigned short *GetRawIndexMapping( void ) { return &m_iRawIndexMapping[0]; }

protected:

	// This is the entity that we're keeping the cache updated for.
	CBaseAnimating *m_pOwner;

	// All the cache data is based off these. When they change, the cache data is regenerated.
	// These are either all valid pointers or all NULL.
	CBaseAnimating *m_pFollow;
	CStudioHdr		*m_pFollowHdr;
	const studiohdr_t *m_pFollowRenderHdr;
	CStudioHdr		*m_pOwnerHdr;
	const studiohdr_t *m_pOwnerRenderHdr;

	// keeps track if this entity is part of a reverse bonemerge
	int				m_nCopiedFramecount;

	// This is the mask we need to use to set up bones on the followed entity to do the bone merge
	int				m_nFollowBoneSetupMask;

	// Cache data.
	class CMergedBone
	{
	public:
		unsigned short m_iMyBone;		// index of MergeCache's owner's bone
		unsigned short m_iParentBone;	// index of m_pFollow's matching bone
	};

	// This is an array of pose param indices on the follower to pose param indices on the owner
	int				m_nOwnerToFollowPoseParamMapping[MAXSTUDIOPOSEPARAM];

	CUtlVector<CMergedBone> m_MergedBones;
	CVarBitVec m_BoneMergeBits;	// One bit for each bone. The bit is set if the bone gets merged.

	unsigned short m_iRawIndexMapping[ MAXSTUDIOBONES ];

	bool m_bForceCacheClear;
};


inline int CBoneMergeCache::IsBoneMerged( int iBone ) const
{
	if ( m_pOwnerHdr )
		return m_BoneMergeBits.Get( iBone );
	else
		return 0;
}


#endif // BONE_MERGE_CACHE_H
