//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "studio.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "convar.h"
#include "tier1/utlmap.h"
#include "tier1/utlbufferstrider.h"
#include "tier0/vprof.h"
#include "mathlib/femodel.h"
#include "mathlib/femodeldesc.h"
#include "mathlib/softbody.h"
#include "mathlib/softbody.inl"
#include "bone_setup.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// preload up to 1 second worth of blocks ahead.
ConVar mod_load_preload( "mod_load_preload", IsGameConsole() ? "1.0" : "1.0", 0, "Indicates how far ahead in seconds to preload animations." );
#ifdef _DEBUG
ConVar softbody_debug( "softbody_debug", "0", FCVAR_CHEAT );
ConVar softbody_debug_substr( "softbody_debug_substr", "", FCVAR_CHEAT );
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

mstudioanimdesc_t &studiohdr_t::pAnimdesc_Internal( int i ) const
{ 
	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	virtualgroup_t *pGroup = &pVModel->m_group[ pVModel->m_anim[i].group ];
	const studiohdr_t *pStudioHdr = pGroup->GetStudioHdr();
	Assert( pStudioHdr );

	return *pStudioHdr->pLocalAnimdesc( pVModel->m_anim[i].index );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

byte *mstudioanimdesc_t::pAnimBlock( int block, int index, bool preloadIfMissing ) const
{
	if (block == -1)
	{
		return (byte *)NULL;
	}
	if (block == 0)
	{
		return (((byte *)this) + index);
	}

	byte *pAnimBlock = pStudiohdr()->GetAnimBlock( block, preloadIfMissing );
	if ( pAnimBlock )
	{
		return pAnimBlock + index;
	}

	return (byte *)NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Indicates if the block has been preloaded already.
//			Returns true if the block is in memory, or is asynchronously loading.
//-----------------------------------------------------------------------------
bool mstudioanimdesc_t::hasAnimBlockBeenPreloaded( int block ) const
{
	return pStudiohdr()->hasAnimBlockBeenPreloaded( block );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

static ConVar mod_load_showstall( "mod_load_showstall", "0", 0, "1 - show hitches , 2 - show stalls" );
byte *mstudioanimdesc_t::pAnim( int *piFrame ) const
{
	float flStall = 0;
	return pAnim( piFrame, flStall );
}

byte *mstudioanimdesc_t::pAnim( int *piFrame, float &flStall ) const
{
	byte *panim = NULL;

	int block = animblock;
	int index = animindex;
	int section = 0;

	if (sectionframes != 0)
	{
		if (numframes > sectionframes && *piFrame == numframes - 1)
		{
			// last frame on long anims is stored separately
			*piFrame = 0;
			section = (numframes / sectionframes) + 1;
		}
		else
		{
			section = *piFrame / sectionframes;
			*piFrame -= section * sectionframes;
		}

		block = pSection( section )->animblock;
		index = pSection( section )->animindex;
	}

	if (block == -1)
	{
		// model needs to be recompiled
		return NULL;
	}

	panim = pAnimBlock( block, index );

	// force a preload of future animations
	if ( sectionframes != 0 )
	{
		// calc how many sections ahead to try looking
		int maxSection = MIN( section + (mod_load_preload.GetFloat() * fps / sectionframes), (numframes / sectionframes) ) + 1;
		int prevBlock = block;

		for ( int i = section + 1; i <= maxSection; i++ )
		{
			// if this is a new block, preload it
			if ( pSection( i )->animblock != prevBlock )
			{
				bool preloaded = hasAnimBlockBeenPreloaded( pSection( i )->animblock );
				if ( preloaded == false )
				{
					// This will preload the block
					pAnimBlock( pSection( i )->animblock, pSection( i )->animindex, true );
					// Msg( "[%8.3f] precaching %s:%s:%d:%d.\n",  Plat_FloatTime(), pStudiohdr()->pszName(), pszName(), i, pSection( i )->animblock );
				}

				prevBlock = pSection( i )->animblock;
			}
		}
	}

	if (panim == NULL)
	{
		if (section > 0 && mod_load_showstall.GetInt() > 0)
		{
			Msg("[%8.3f] hitch on %s:%s:%d:%d\n", Plat_FloatTime(), pStudiohdr()->pszName(), pszName(), section, block );
		}

		// back up until a previously loaded block is found
		while (--section >= 0)
		{
			block = pSection( section )->animblock;
			index = pSection( section )->animindex;
			panim = pAnimBlock( block, index, false );
			if (panim)
			{
				// set it to the last frame in the last valid section
				*piFrame = sectionframes - 1;
				break;
			}
		}
	}

	// try to guess a valid stall time interval (tuned for the X360)
	flStall = 0.0f;
	if (panim == NULL && section <= 0)
	{
		zeroframestalltime = Plat_FloatTime();
		flStall = 1.0f;
	}
	else if (panim != NULL && zeroframestalltime != 0.0f)
	{
		float dt = Plat_FloatTime() - zeroframestalltime;
		if (dt >= 0.0)
		{
			flStall = SimpleSpline( clamp( (0.200f - dt) * 5.0, 0.0f, 1.0f ) );
		}

		if (flStall == 0.0f)
		{
			// disable stalltime
			zeroframestalltime = 0.0f;
		}
		else if (mod_load_showstall.GetInt() > 1)
		{
			Msg("[%8.3f] stall blend %.2f on %s:%s:%d:%d\n", Plat_FloatTime(), flStall, pStudiohdr()->pszName(), pszName(), section, block );
		}
	}

	if (panim == NULL && mod_load_showstall.GetInt() > 1)
	{
		Msg("[%8.3f] stall on %s:%s:%d:%d\n", Plat_FloatTime(), pStudiohdr()->pszName(), pszName(), section, block );
	}

	return panim;
}

mstudioikrule_t *mstudioanimdesc_t::pIKRule( int i ) const
{
	if (numikrules)
	{
		if (ikruleindex)
		{
			return (mstudioikrule_t *)(((byte *)this) + ikruleindex) + i;
		}
		else 
		{
			if (animblock == 0)
			{
				AssertOnce(0); // Should never happen
				return  (mstudioikrule_t *)(((byte *)this) + animblockikruleindex) + i;
			}
			else
			{
				byte *pAnimBlock = pStudiohdr()->GetAnimBlock( animblock );
				
				if ( pAnimBlock )
				{
					return (mstudioikrule_t *)(pAnimBlock + animblockikruleindex) + i;
				}
			}
		}
	}

	return NULL;
}


mstudiolocalhierarchy_t *mstudioanimdesc_t::pHierarchy( int i ) const
{
	if (localhierarchyindex)
	{
		if (animblock == 0)
		{
			return  (mstudiolocalhierarchy_t *)(((byte *)this) + localhierarchyindex) + i;
		}
		else
		{
			byte *pAnimBlock = pStudiohdr()->GetAnimBlock( animblock );
			
			if ( pAnimBlock )
			{
				return (mstudiolocalhierarchy_t *)(pAnimBlock + localhierarchyindex) + i;
			}
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

bool studiohdr_t::SequencesAvailable() const
{
	if (numincludemodels == 0)
	{
		return true;
	}

	return ( GetVirtualModel() != NULL );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int studiohdr_t::GetNumSeq_Internal( void ) const
{
	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );
	return pVModel->m_seq.Count();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

mstudioseqdesc_t &studiohdr_t::pSeqdesc_Internal( int i ) const
{
	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	if ( !pVModel )
	{
		return *pLocalSeqdesc( i );
	}

	virtualgroup_t *pGroup = &pVModel->m_group[ pVModel->m_seq[i].group ];
	const studiohdr_t *pStudioHdr = pGroup->GetStudioHdr();
	Assert( pStudioHdr );

	return *pStudioHdr->pLocalSeqdesc( pVModel->m_seq[i].index );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int studiohdr_t::iRelativeAnim_Internal( int baseseq, int relanim ) const
{
	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	virtualgroup_t *pGroup = &pVModel->m_group[ pVModel->m_seq[baseseq].group ];

	return pGroup->masterAnim[ relanim ];
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int studiohdr_t::iRelativeSeq_Internal( int baseseq, int relseq ) const
{
	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	virtualgroup_t *pGroup = &pVModel->m_group[ pVModel->m_seq[baseseq].group ];

	return pGroup->masterSeq[ relseq ];
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int	studiohdr_t::GetNumPoseParameters( void ) const
{
	if (numincludemodels == 0)
	{
		return numlocalposeparameters;
	}

	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	return pVModel->m_pose.Count();
}



//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

const mstudioposeparamdesc_t &studiohdr_t::pPoseParameter( int i )
{
	if (numincludemodels == 0)
	{
		return *pLocalPoseParameter( i );
	}

	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	if ( pVModel->m_pose[i].group == 0)
		return *pLocalPoseParameter( pVModel->m_pose[i].index );

	virtualgroup_t *pGroup = &pVModel->m_group[ pVModel->m_pose[i].group ];

	const studiohdr_t *pStudioHdr = pGroup->GetStudioHdr();
	Assert( pStudioHdr );

	return *pStudioHdr->pLocalPoseParameter( pVModel->m_pose[i].index );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int studiohdr_t::GetSharedPoseParameter( int iSequence, int iLocalPose ) const
{
	if (numincludemodels == 0)
	{
		return iLocalPose;
	}

	if (iLocalPose == -1)
		return iLocalPose;

	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	virtualgroup_t *pGroup = &pVModel->m_group[ pVModel->m_seq[iSequence].group ];

	return pGroup->masterPose[iLocalPose];
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int studiohdr_t::EntryNode( int iSequence )
{
	mstudioseqdesc_t &seqdesc = pSeqdesc( iSequence );

	if (numincludemodels == 0 || seqdesc.localentrynode == 0)
	{
		return seqdesc.localentrynode;
	}

	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	virtualgroup_t *pGroup = &pVModel->m_group[ pVModel->m_seq[iSequence].group ];

	return pGroup->masterNode[seqdesc.localentrynode-1]+1;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------


int studiohdr_t::ExitNode( int iSequence )
{
	mstudioseqdesc_t &seqdesc = pSeqdesc( iSequence );

	if (numincludemodels == 0 || seqdesc.localexitnode == 0)
	{
		return seqdesc.localexitnode;
	}

	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	virtualgroup_t *pGroup = &pVModel->m_group[ pVModel->m_seq[iSequence].group ];

	return pGroup->masterNode[seqdesc.localexitnode-1]+1;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int	studiohdr_t::GetNumAttachments( void ) const
{
	if (numincludemodels == 0)
	{
		return numlocalattachments;
	}

	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	return pVModel->m_attachment.Count();
}



//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

const mstudioattachment_t &studiohdr_t::pAttachment( int i ) const
{
	if (numincludemodels == 0)
	{
		return *pLocalAttachment( i );
	}

	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	virtualgroup_t *pGroup = &pVModel->m_group[ pVModel->m_attachment[i].group ];
	const studiohdr_t *pStudioHdr = pGroup->GetStudioHdr();
	Assert( pStudioHdr );

	return *pStudioHdr->pLocalAttachment( pVModel->m_attachment[i].index );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int	studiohdr_t::GetAttachmentBone( int i )
{
	const mstudioattachment_t &attachment = pAttachment( i );

	// remap bone
	virtualmodel_t *pVModel = GetVirtualModel();
	if (pVModel)
	{
		virtualgroup_t *pGroup = &pVModel->m_group[ pVModel->m_attachment[i].group ];
		int iBone = pGroup->masterBone[attachment.localbone];
		if (iBone == -1)
			return 0;
		return iBone;
	}
	return attachment.localbone;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void studiohdr_t::SetAttachmentBone( int iAttachment, int iBone )
{
	mstudioattachment_t &attachment = (mstudioattachment_t &)pAttachment( iAttachment );

	// remap bone
	virtualmodel_t *pVModel = GetVirtualModel();
	if (pVModel)
	{
		virtualgroup_t *pGroup = &pVModel->m_group[ pVModel->m_attachment[iAttachment].group ];
		iBone = pGroup->boneMap[iBone];
	}
	attachment.localbone = iBone;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

char *studiohdr_t::pszNodeName( int iNode )
{
	if (numincludemodels == 0)
	{
		return pszLocalNodeName( iNode );
	}

	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	if ( pVModel->m_node.Count() <= iNode-1 )
		return "Invalid node";

	return pVModel->m_group[ pVModel->m_node[iNode-1].group ].GetStudioHdr()->pszLocalNodeName( pVModel->m_node[iNode-1].index );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int studiohdr_t::GetTransition( int iFrom, int iTo ) const
{
	if (numincludemodels == 0)
	{
		return *pLocalTransition( (iFrom-1)*numlocalnodes + (iTo - 1) );
	}

	return iTo;
	/*
	FIXME: not connected
	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	return pVModel->m_transition.Element( iFrom ).Element( iTo );
	*/
}


int	studiohdr_t::GetActivityListVersion( void )
{
	if (numincludemodels == 0)
	{
		return activitylistversion;
	}

	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	int version = activitylistversion;

	int i;
	for (i = 1; i < pVModel->m_group.Count(); i++)
	{
		virtualgroup_t *pGroup = &pVModel->m_group[ i ];
		const studiohdr_t *pStudioHdr = pGroup->GetStudioHdr();

		Assert( pStudioHdr );

		version = MIN( version, pStudioHdr->activitylistversion );
	}

	return version;
}

void studiohdr_t::SetActivityListVersion( int version ) const
{
	activitylistversion = version;

	if (numincludemodels == 0)
	{
		return;
	}

	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	int i;
	for (i = 1; i < pVModel->m_group.Count(); i++)
	{
		virtualgroup_t *pGroup = &pVModel->m_group[ i ];
		const studiohdr_t *pStudioHdr = pGroup->GetStudioHdr();

		Assert( pStudioHdr );

		pStudioHdr->SetActivityListVersion( version );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------


int studiohdr_t::GetNumIKAutoplayLocks( void ) const
{
	if (numincludemodels == 0)
	{
		return numlocalikautoplaylocks;
	}

	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	return pVModel->m_iklock.Count();
}

const mstudioiklock_t &studiohdr_t::pIKAutoplayLock( int i )
{
	if (numincludemodels == 0)
	{
		return *pLocalIKAutoplayLock( i );
	}

	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	virtualgroup_t *pGroup = &pVModel->m_group[ pVModel->m_iklock[i].group ];
	const studiohdr_t *pStudioHdr = pGroup->GetStudioHdr();
	Assert( pStudioHdr );

	return *pStudioHdr->pLocalIKAutoplayLock( pVModel->m_iklock[i].index );
}

int	studiohdr_t::CountAutoplaySequences() const
{
	int count = 0;
	for (int i = 0; i < GetNumSeq(); i++)
	{
		mstudioseqdesc_t &seqdesc = pSeqdesc( i );
		if (seqdesc.flags & STUDIO_AUTOPLAY)
		{
			count++;
		}
	}
	return count;
}

int	studiohdr_t::CopyAutoplaySequences( unsigned short *pOut, int outCount ) const
{
	int outIndex = 0;
	for (int i = 0; i < GetNumSeq() && outIndex < outCount; i++)
	{
		mstudioseqdesc_t &seqdesc = pSeqdesc( i );
		if (seqdesc.flags & STUDIO_AUTOPLAY)
		{
			pOut[outIndex] = i;
			outIndex++;
		}
	}
	return outIndex;
}

//-----------------------------------------------------------------------------
// Purpose:	maps local sequence bone to global bone
//-----------------------------------------------------------------------------

int	studiohdr_t::RemapSeqBone( int iSequence, int iLocalBone ) const	
{
	// remap bone
	virtualmodel_t *pVModel = GetVirtualModel();
	if (pVModel)
	{
		const virtualgroup_t *pSeqGroup = pVModel->pSeqGroup( iSequence );
		return pSeqGroup->masterBone[iLocalBone];
	}
	return iLocalBone;
}

int	studiohdr_t::RemapAnimBone( int iAnim, int iLocalBone ) const
{
	// remap bone
	virtualmodel_t *pVModel = GetVirtualModel();
	if (pVModel)
	{
		const virtualgroup_t *pAnimGroup = pVModel->pAnimGroup( iAnim );
		return pAnimGroup->masterBone[iLocalBone];
	}
	return iLocalBone;
}






//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

CStudioHdr::CStudioHdr( void ) 
{
	// set pointer to bogus value
	m_nFrameUnlockCounter = 0;
	m_pFrameUnlockCounter = &m_nFrameUnlockCounter;
	Init( NULL );
}

CStudioHdr::CStudioHdr( const studiohdr_t *pStudioHdr, IMDLCache *mdlcache ) 
{
	// preset pointer to bogus value (it may be overwritten with legitimate data later)
	m_nFrameUnlockCounter = 0;
	m_pFrameUnlockCounter = &m_nFrameUnlockCounter;
	Init( pStudioHdr, mdlcache );
}


// extern IDataCache *g_pDataCache;

void CStudioHdr::Init( const studiohdr_t *pStudioHdr, IMDLCache *mdlcache )
{
	m_pStudioHdr = pStudioHdr;

	m_pVModel = NULL;
	m_pSoftbody = NULL;
	m_pStudioHdrCache.RemoveAll();

	if (m_pStudioHdr == NULL)
	{
		return;
	}

	if ( mdlcache )
	{
		m_pFrameUnlockCounter = mdlcache->GetFrameUnlockCounterPtr( MDLCACHE_STUDIOHDR );
		m_nFrameUnlockCounter = *m_pFrameUnlockCounter - 1;
	}

	if (m_pStudioHdr->numincludemodels != 0)
	{
		ResetVModel( m_pStudioHdr->GetVirtualModel() );
	}

	m_boneFlags.EnsureCount( numbones() );
	m_boneParent.EnsureCount( numbones() );
	for (int i = 0; i < numbones(); i++)
	{
		m_boneFlags[i] = pBone( i )->flags;
		m_boneParent[i] = pBone( i )->parent;
	}



	m_pActivityToSequence = NULL;
}

void CStudioHdr::Term()
{
	if ( m_pSoftbody )
	{
		CFeModel *pFeModel = m_pSoftbody->GetFeModel();
		m_pSoftbody->Shutdown();
		Assert( pFeModel );
		MemAlloc_FreeAligned( pFeModel );
		m_pSoftbody = NULL;
	}
	CActivityToSequenceMapping::ReleaseMapping( m_pActivityToSequence );
	m_pActivityToSequence = NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

bool CStudioHdr::SequencesAvailable() const
{
	if (m_pStudioHdr->numincludemodels == 0)
	{
		return true;
	}

	if (m_pVModel == NULL)
	{
		// repoll m_pVModel
		return (ResetVModel( m_pStudioHdr->GetVirtualModel() ) != NULL);
	}
	else
		return true;
}


const virtualmodel_t * CStudioHdr::ResetVModel( const virtualmodel_t *pVModel ) const
{
	if (pVModel != NULL)
	{
		m_pVModel = (virtualmodel_t *)pVModel;
#if !defined( POSIX )
		Assert( !pVModel->m_Lock.GetOwnerId() );
#endif
		m_pStudioHdrCache.SetCount( m_pVModel->m_group.Count() );

		int i;
		for (i = 0; i < m_pStudioHdrCache.Count(); i++)
		{
			m_pStudioHdrCache[ i ] = NULL;
		}
		
		return const_cast<virtualmodel_t *>(pVModel);
	}
	else
	{
		m_pVModel = NULL;
		return NULL;
	}
}

const studiohdr_t *CStudioHdr::GroupStudioHdr( int i )
{
	if ( !this )
	{
		ExecuteNTimes( 5, Warning( "Call to NULL CStudioHdr::GroupStudioHdr()\n" ) );
	}

	if ( m_nFrameUnlockCounter != *m_pFrameUnlockCounter )
	{
		m_FrameUnlockCounterMutex.Lock();
		if ( *m_pFrameUnlockCounter != m_nFrameUnlockCounter ) // i.e., this thread got the mutex
		{
			memset( m_pStudioHdrCache.Base(), 0, m_pStudioHdrCache.Count() * sizeof(studiohdr_t *) );
			m_nFrameUnlockCounter = *m_pFrameUnlockCounter;
		}
		m_FrameUnlockCounterMutex.Unlock();
	}

	if ( !m_pStudioHdrCache.IsValidIndex( i ) )
	{
		const char *pszName;
		pszName = ( m_pStudioHdr ) ? m_pStudioHdr->pszName() : "<<null>>";
		ExecuteNTimes( 5, Warning( "Invalid index passed to CStudioHdr(%s)::GroupStudioHdr(): %d [%d]\n", pszName, i, m_pStudioHdrCache.Count() ) );
		DebuggerBreakIfDebugging();
		return m_pStudioHdr; // return something known to probably exist, certainly things will be messed up, but hopefully not crash before the warning is noticed
	}

	const studiohdr_t *pStudioHdr = m_pStudioHdrCache[ i ];

	if (pStudioHdr == NULL)
	{
#if !defined( POSIX )
		Assert( !m_pVModel->m_Lock.GetOwnerId() );
#endif
		virtualgroup_t *pGroup = &m_pVModel->m_group[ i ];
		pStudioHdr = pGroup->GetStudioHdr();
		m_pStudioHdrCache[ i ] = pStudioHdr;
	}

	Assert( pStudioHdr );
	return pStudioHdr;
}


const studiohdr_t *CStudioHdr::pSeqStudioHdr( int sequence )
{
	if (m_pVModel == NULL)
	{
		return m_pStudioHdr;
	}

	const studiohdr_t *pStudioHdr = GroupStudioHdr( m_pVModel->m_seq[sequence].group );

	return pStudioHdr;
}


const studiohdr_t *CStudioHdr::pAnimStudioHdr( int animation )
{
	if (m_pVModel == NULL)
	{
		return m_pStudioHdr;
	}

	const studiohdr_t *pStudioHdr = GroupStudioHdr( m_pVModel->m_anim[animation].group );

	return pStudioHdr;
}



mstudioanimdesc_t &CStudioHdr::pAnimdesc_Internal( int i )
{ 
	const studiohdr_t *pStudioHdr = GroupStudioHdr( m_pVModel->m_anim[i].group );

	return *pStudioHdr->pLocalAnimdesc( m_pVModel->m_anim[i].index );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int CStudioHdr::GetNumSeq_Internal( void ) const
{
	return m_pVModel->m_seq.Count();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

mstudioseqdesc_t &CStudioHdr::pSeqdesc_Internal( int i )
{
	Assert( i >= 0 && i < GetNumSeq() );
	if ( i < 0 || i >= GetNumSeq() )
	{
		// Avoid reading random memory.
		i = 0;
	}
	
	const studiohdr_t *pStudioHdr = GroupStudioHdr( m_pVModel->m_seq[i].group );

	return *pStudioHdr->pLocalSeqdesc( m_pVModel->m_seq[i].index );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int CStudioHdr::iRelativeAnim_Internal( int baseseq, int relanim ) const
{
	virtualgroup_t *pGroup = &m_pVModel->m_group[ m_pVModel->m_seq[baseseq].group ];

	return pGroup->masterAnim[ relanim ];
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int CStudioHdr::iRelativeSeq( int baseseq, int relseq ) const
{
	if (m_pVModel == NULL)
	{
		return relseq;
	}

	Assert( m_pVModel );

	virtualgroup_t *pGroup = &m_pVModel->m_group[ m_pVModel->m_seq[baseseq].group ];

	return pGroup->masterSeq[ relseq ];
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int	CStudioHdr::GetNumPoseParameters( void ) const
{
	if (m_pVModel == NULL)
	{
		if ( m_pStudioHdr )
			return m_pStudioHdr->numlocalposeparameters;
		else
			return 0;
	}

	Assert( m_pVModel );

	return m_pVModel->m_pose.Count();
}



//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

const mstudioposeparamdesc_t &CStudioHdr::pPoseParameter( int i )
{
	if (m_pVModel == NULL)
	{
		return *m_pStudioHdr->pLocalPoseParameter( i );
	}

	if ( m_pVModel->m_pose[i].group == 0)
		return *m_pStudioHdr->pLocalPoseParameter( m_pVModel->m_pose[i].index );

	const studiohdr_t *pStudioHdr = GroupStudioHdr( m_pVModel->m_pose[i].group );

	return *pStudioHdr->pLocalPoseParameter( m_pVModel->m_pose[i].index );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int CStudioHdr::GetSharedPoseParameter( int iSequence, int iLocalPose ) const
{
	if (m_pVModel == NULL)
	{
		return iLocalPose;
	}

	if (iLocalPose == -1)
		return iLocalPose;

	Assert( m_pVModel );

	virtualgroup_t *pGroup = &m_pVModel->m_group[ m_pVModel->m_seq[iSequence].group ];

	return pGroup->masterPose[iLocalPose];
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int CStudioHdr::EntryNode( int iSequence )
{
	mstudioseqdesc_t &seqdesc = pSeqdesc( iSequence );

	if (m_pVModel == NULL || seqdesc.localentrynode == 0)
	{
		return seqdesc.localentrynode;
	}

	Assert( m_pVModel );

	virtualgroup_t *pGroup = &m_pVModel->m_group[ m_pVModel->m_seq[iSequence].group ];

	return pGroup->masterNode[seqdesc.localentrynode-1]+1;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------


int CStudioHdr::ExitNode( int iSequence )
{
	mstudioseqdesc_t &seqdesc = pSeqdesc( iSequence );

	if (m_pVModel == NULL || seqdesc.localexitnode == 0)
	{
		return seqdesc.localexitnode;
	}

	Assert( m_pVModel );

	virtualgroup_t *pGroup = &m_pVModel->m_group[ m_pVModel->m_seq[iSequence].group ];

	return pGroup->masterNode[seqdesc.localexitnode-1]+1;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int	CStudioHdr::GetNumAttachments( void ) const
{
	if (m_pVModel == NULL)
	{
		return m_pStudioHdr->numlocalattachments;
	}

	Assert( m_pVModel );

	return m_pVModel->m_attachment.Count();
}



//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

const mstudioattachment_t &CStudioHdr::pAttachment( int i )
{
	if (m_pVModel == NULL)
	{
		return *m_pStudioHdr->pLocalAttachment( i );
	}

	Assert( m_pVModel );

	const studiohdr_t *pStudioHdr = GroupStudioHdr( m_pVModel->m_attachment[i].group );

	return *pStudioHdr->pLocalAttachment( m_pVModel->m_attachment[i].index );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int	CStudioHdr::GetAttachmentBone( int i )
{
	if (m_pVModel == 0)
	{
		return m_pStudioHdr->pLocalAttachment( i )->localbone;
	}

	virtualgroup_t *pGroup = &m_pVModel->m_group[ m_pVModel->m_attachment[i].group ];
	const mstudioattachment_t &attachment = pAttachment( i );
	int iBone = pGroup->masterBone[attachment.localbone];
	if (iBone == -1)
		return 0;
	return iBone;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void CStudioHdr::SetAttachmentBone( int iAttachment, int iBone )
{
	mstudioattachment_t &attachment = (mstudioattachment_t &)m_pStudioHdr->pAttachment( iAttachment );

	// remap bone
	if (m_pVModel)
	{
		virtualgroup_t *pGroup = &m_pVModel->m_group[ m_pVModel->m_attachment[iAttachment].group ];
		iBone = pGroup->boneMap[iBone];
	}
	attachment.localbone = iBone;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

char *CStudioHdr::pszNodeName( int iNode )
{
	if (m_pVModel == NULL)
	{
		return m_pStudioHdr->pszLocalNodeName( iNode );
	}

	if ( m_pVModel->m_node.Count() <= iNode-1 )
		return "Invalid node";

	const studiohdr_t *pStudioHdr = GroupStudioHdr( m_pVModel->m_node[iNode-1].group );
	
	return pStudioHdr->pszLocalNodeName( m_pVModel->m_node[iNode-1].index );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int CStudioHdr::GetTransition( int iFrom, int iTo ) const
{
	if (m_pVModel == NULL)
	{
		return *m_pStudioHdr->pLocalTransition( (iFrom-1)*m_pStudioHdr->numlocalnodes + (iTo - 1) );
	}

	return iTo;
	/*
	FIXME: not connected
	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	return pVModel->m_transition.Element( iFrom ).Element( iTo );
	*/
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int	CStudioHdr::GetActivityListVersion( void )
{
	if (m_pVModel == NULL)
	{
		return m_pStudioHdr->activitylistversion;
	}

	int version = m_pStudioHdr->activitylistversion;

	int i;
	for (i = 1; i < m_pVModel->m_group.Count(); i++)
	{
		const studiohdr_t *pStudioHdr = GroupStudioHdr( i );
		Assert( pStudioHdr );
		version = MIN( version, pStudioHdr->activitylistversion );
	}

	return version;
}

void CStudioHdr::SetActivityListVersion( int version )
{
	m_pStudioHdr->activitylistversion = version;

	if (m_pVModel == NULL)
	{
		return;
	}

	int i;
	for (i = 1; i < m_pVModel->m_group.Count(); i++)
	{
		const studiohdr_t *pStudioHdr = GroupStudioHdr( i );
		Assert( pStudioHdr );
		pStudioHdr->SetActivityListVersion( version );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int	CStudioHdr::GetEventListVersion( void )
{
	if (m_pVModel == NULL)
	{
		return m_pStudioHdr->eventsindexed;
	}

	int version = m_pStudioHdr->eventsindexed;

	int i;
	for (i = 1; i < m_pVModel->m_group.Count(); i++)
	{
		const studiohdr_t *pStudioHdr = GroupStudioHdr( i );
		Assert( pStudioHdr );
		version = MIN( version, pStudioHdr->eventsindexed );
	}

	return version;
}

void CStudioHdr::SetEventListVersion( int version )
{
	m_pStudioHdr->eventsindexed = version;

	if (m_pVModel == NULL)
	{
		return;
	}

	int i;
	for (i = 1; i < m_pVModel->m_group.Count(); i++)
	{
		const studiohdr_t *pStudioHdr = GroupStudioHdr( i );
		Assert( pStudioHdr );
		pStudioHdr->eventsindexed = version;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------


int CStudioHdr::GetNumIKAutoplayLocks( void ) const
{
	if (m_pVModel == NULL)
	{
		return m_pStudioHdr->numlocalikautoplaylocks;
	}

	return m_pVModel->m_iklock.Count();
}

const mstudioiklock_t &CStudioHdr::pIKAutoplayLock( int i )
{
	if (m_pVModel == NULL)
	{
		return *m_pStudioHdr->pLocalIKAutoplayLock( i );
	}

	const studiohdr_t *pStudioHdr = GroupStudioHdr( m_pVModel->m_iklock[i].group );
	Assert( pStudioHdr );
	return *pStudioHdr->pLocalIKAutoplayLock( m_pVModel->m_iklock[i].index );
}

#if 0
int	CStudioHdr::CountAutoplaySequences() const
{
	int count = 0;
	for (int i = 0; i < GetNumSeq(); i++)
	{
		mstudioseqdesc_t &seqdesc = pSeqdesc( i );
		if (seqdesc.flags & STUDIO_AUTOPLAY)
		{
			count++;
		}
	}
	return count;
}

int	CStudioHdr::CopyAutoplaySequences( unsigned short *pOut, int outCount ) const
{
	int outIndex = 0;
	for (int i = 0; i < GetNumSeq() && outIndex < outCount; i++)
	{
		mstudioseqdesc_t &seqdesc = pSeqdesc( i );
		if (seqdesc.flags & STUDIO_AUTOPLAY)
		{
			pOut[outIndex] = i;
			outIndex++;
		}
	}
	return outIndex;
}

#endif

//-----------------------------------------------------------------------------
// Purpose:	maps local sequence bone to global bone
//-----------------------------------------------------------------------------

int	CStudioHdr::RemapSeqBone( int iSequence, int iLocalBone ) const	
{
	// remap bone
	if (m_pVModel)
	{
		const virtualgroup_t *pSeqGroup = m_pVModel->pSeqGroup( iSequence );
		return pSeqGroup->masterBone[iLocalBone];
	}
	return iLocalBone;
}

int	CStudioHdr::RemapAnimBone( int iAnim, int iLocalBone ) const
{
	// remap bone
	if (m_pVModel)
	{
		const virtualgroup_t *pAnimGroup = m_pVModel->pAnimGroup( iAnim );
		return pAnimGroup->masterBone[iLocalBone];
	}
	return iLocalBone;
}

//-----------------------------------------------------------------------------
// Purpose: run the interpreted FAC's expressions, converting flex_controller 
//			values into FAC weights
//-----------------------------------------------------------------------------
void CStudioHdr::RunFlexRulesOld( const float *src, float *dest )
{
	int i, j;

	// FIXME: this shouldn't be needed, flex without rules should be stripped in studiomdl
	for (i = 0; i < numflexdesc(); i++)
	{
		dest[i] = 0;
	}

	for (i = 0; i < numflexrules(); i++)
	{
		float stack[32] = { 0.0 };
		int k = 0;
		mstudioflexrule_t *prule = pFlexRule( i );

		mstudioflexop_t *pops = prule->iFlexOp( 0 );

		// debugoverlay->AddTextOverlay( GetAbsOrigin() + Vector( 0, 0, 64 ), i + 1, 0, "%2d:%d\n", i, prule->flex );

		for (j = 0; j < prule->numops; j++)
		{
			switch (pops->op)
			{
			case STUDIO_ADD: stack[k-2] = stack[k-2] + stack[k-1]; k--; break;
			case STUDIO_SUB: stack[k-2] = stack[k-2] - stack[k-1]; k--; break;
			case STUDIO_MUL: stack[k-2] = stack[k-2] * stack[k-1]; k--; break;
			case STUDIO_DIV:
				if (stack[k-1] > 0.0001)
				{
					stack[k-2] = stack[k-2] / stack[k-1];
				}
				else
				{
					stack[k-2] = 0;
				}
				k--; 
				break;
			case STUDIO_NEG: stack[k-1] = -stack[k-1]; break;
			case STUDIO_MAX: stack[k-2] = MAX( stack[k-2], stack[k-1] ); k--; break;
			case STUDIO_MIN: stack[k-2] = MIN( stack[k-2], stack[k-1] ); k--; break;
			case STUDIO_CONST: stack[k] = pops->d.value; k++; break;
			case STUDIO_FETCH1: 
				{ 
				int m = pFlexcontroller( (LocalFlexController_t)pops->d.index)->localToGlobal;
				stack[k] = src[m];
				k++; 
				break;
				}
			case STUDIO_FETCH2:
				{
					stack[k] = dest[pops->d.index]; k++; break;
				}
			case STUDIO_COMBO:
				{
					int m = pops->d.index;
					int km = k - m;
					for ( int i = km + 1; i < k; ++i )
					{
						stack[ km ] *= stack[ i ];
					}
					k = k - m + 1;
				}
				break;
			case STUDIO_DOMINATE:
				{
					int m = pops->d.index;
					int km = k - m;
					float dv = stack[ km ];
					for ( int i = km + 1; i < k; ++i )
					{
						dv *= stack[ i ];
					}
					stack[ km - 1 ] *= 1.0f - dv;
					k -= m;
				}
				break;
			case STUDIO_2WAY_0:
				{ 
					int m = pFlexcontroller( (LocalFlexController_t)pops->d.index )->localToGlobal;
					stack[ k ] = RemapValClamped( src[m], -1.0f, 0.0f, 1.0f, 0.0f );
					k++; 
				}
				break;
			case STUDIO_2WAY_1:
				{ 
					int m = pFlexcontroller( (LocalFlexController_t)pops->d.index )->localToGlobal;
					stack[ k ] = RemapValClamped( src[m], 0.0f, 1.0f, 0.0f, 1.0f );
					k++; 
				}
				break;
			case STUDIO_NWAY:
				{
					LocalFlexController_t valueControllerIndex = static_cast< LocalFlexController_t >( (int)stack[ k - 1 ] );
					int m = pFlexcontroller( valueControllerIndex )->localToGlobal;
					float flValue = src[ m ];
					int v = pFlexcontroller( (LocalFlexController_t)pops->d.index )->localToGlobal;

					const Vector4D filterRamp( stack[ k - 5 ], stack[ k - 4 ], stack[ k - 3 ], stack[ k - 2 ] );

					// Apply multicontrol remapping
					if ( flValue <= filterRamp.x || flValue >= filterRamp.w )
					{
						flValue = 0.0f;
					}
					else if ( flValue < filterRamp.y )
					{
						flValue = RemapValClamped( flValue, filterRamp.x, filterRamp.y, 0.0f, 1.0f );
					}
					else if ( flValue > filterRamp.z )
					{
						flValue = RemapValClamped( flValue, filterRamp.z, filterRamp.w, 1.0f, 0.0f );
					}
					else
					{
						flValue = 1.0f;
					}

					stack[ k - 5 ] = flValue * src[ v ];

					k -= 4; 
				}
				break;
			case STUDIO_DME_LOWER_EYELID:
				{ 
					const mstudioflexcontroller_t *const pCloseLidV = pFlexcontroller( (LocalFlexController_t)pops->d.index );
					const float flCloseLidV = RemapValClamped( src[ pCloseLidV->localToGlobal ], pCloseLidV->min, pCloseLidV->max, 0.0f, 1.0f );

					const mstudioflexcontroller_t *const pCloseLid = pFlexcontroller( static_cast< LocalFlexController_t >( (int)stack[ k - 1 ] ) );
					const float flCloseLid = RemapValClamped( src[ pCloseLid->localToGlobal ], pCloseLid->min, pCloseLid->max, 0.0f, 1.0f );

					int nBlinkIndex = static_cast< int >( stack[ k - 2 ] );
					float flBlink = 0.0f;
					if ( nBlinkIndex >= 0 )
					{
						const mstudioflexcontroller_t *const pBlink = pFlexcontroller( static_cast< LocalFlexController_t >( (int)stack[ k - 2 ] ) );
						flBlink = RemapValClamped( src[ pBlink->localToGlobal ], pBlink->min, pBlink->max, 0.0f, 1.0f );
					}

					int nEyeUpDownIndex = static_cast< int >( stack[ k - 3 ] );
					float flEyeUpDown = 0.0f;
					if ( nEyeUpDownIndex >= 0 )
					{
						const mstudioflexcontroller_t *const pEyeUpDown = pFlexcontroller( static_cast< LocalFlexController_t >( (int)stack[ k - 3 ] ) );
						flEyeUpDown = RemapValClamped( src[ pEyeUpDown->localToGlobal ], pEyeUpDown->min, pEyeUpDown->max, -1.0f, 1.0f );
					}

					if ( flEyeUpDown > 0.0 )
					{
						stack [ k - 3 ] = ( 1.0f - flEyeUpDown ) * ( 1.0f - flCloseLidV ) * flCloseLid;
					}
					else
					{
						stack [ k - 3 ] = ( 1.0f - flCloseLidV ) * flCloseLid;
					}
					k -= 2;
				}
				break;
			case STUDIO_DME_UPPER_EYELID:
				{ 
					const mstudioflexcontroller_t *const pCloseLidV = pFlexcontroller( (LocalFlexController_t)pops->d.index );
					const float flCloseLidV = RemapValClamped( src[ pCloseLidV->localToGlobal ], pCloseLidV->min, pCloseLidV->max, 0.0f, 1.0f );

					const mstudioflexcontroller_t *const pCloseLid = pFlexcontroller( static_cast< LocalFlexController_t >( (int)stack[ k - 1 ] ) );
					const float flCloseLid = RemapValClamped( src[ pCloseLid->localToGlobal ], pCloseLid->min, pCloseLid->max, 0.0f, 1.0f );

					int nBlinkIndex = static_cast< int >( stack[ k - 2 ] );
					float flBlink = 0.0f;
					if ( nBlinkIndex >= 0 )
					{
						const mstudioflexcontroller_t *const pBlink = pFlexcontroller( static_cast< LocalFlexController_t >( (int)stack[ k - 2 ] ) );
						flBlink = RemapValClamped( src[ pBlink->localToGlobal ], pBlink->min, pBlink->max, 0.0f, 1.0f );
					}

					int nEyeUpDownIndex = static_cast< int >( stack[ k - 3 ] );
					float flEyeUpDown = 0.0f;
					if ( nEyeUpDownIndex >= 0 )
					{
						const mstudioflexcontroller_t *const pEyeUpDown = pFlexcontroller( static_cast< LocalFlexController_t >( (int)stack[ k - 3 ] ) );
						flEyeUpDown = RemapValClamped( src[ pEyeUpDown->localToGlobal ], pEyeUpDown->min, pEyeUpDown->max, -1.0f, 1.0f );
					}

					if ( flEyeUpDown < 0.0f )
					{
						stack [ k - 3 ] = ( 1.0f + flEyeUpDown ) * flCloseLidV * flCloseLid;
					}
					else
					{
						stack [ k - 3 ] = flCloseLidV * flCloseLid;
					}
					k -= 2;
				}
				break;
			}

			pops++;
		}

		dest[prule->flex] = stack[0];
	}
}

void CStudioHdr::RunFlexRulesNew( const float *src, float *dest )
{
	// FIXME: this shouldn't be needed, flex without rules should be stripped in studiomdl
	memset( dest, 0, sizeof( dest[0] ) * numflexdesc() );

	for (int i = 0; i < numflexrules(); i++)
	{
		float stack[32];
		float *pSP = stack + ARRAYSIZE( stack );
		mstudioflexrule_t *prule = pFlexRule( i );

		mstudioflexop_t *pops = prule->iFlexOp( 0 );

		int nOps = prule->numops;
		float flTOS = 0.;
		if ( nOps )
			do
			{
				switch (pops->op)
				{
					case STUDIO_ADD:
						flTOS += *(pSP++);
						break;

					case STUDIO_SUB:
						flTOS = *(pSP++) - flTOS;
						break;

					case STUDIO_MUL:
						flTOS *= *(pSP++);
						break;
					case STUDIO_DIV:
						if (flTOS > 0.0001)
						{
							flTOS = *(pSP) / flTOS;
						}
						else
						{
							flTOS = 0.;
						}
						pSP++;
						break;

					case STUDIO_NEG:
						flTOS = -flTOS;
						break;

					case STUDIO_MAX:
					{
						float flNos = *(pSP++);
						flTOS = MAX( flTOS, flNos );
						break;
					}

					case STUDIO_MIN:
					{
						float flNos = *(pSP++);
						flTOS = MIN( flTOS, flNos);
						break;
					}
					case STUDIO_CONST:
						*(--pSP) = flTOS;
						flTOS = pops->d.value; 
						break;

					case STUDIO_FETCH1: 
					{ 
						*(--pSP ) = flTOS;
						int m = pFlexcontroller( (LocalFlexController_t)pops->d.index)->localToGlobal;
						flTOS = src[m];
						break;
					}

					case STUDIO_FETCH2:
					{
						*(--pSP) = flTOS;
						flTOS = dest[pops->d.index];
						break;
					}
					case STUDIO_COMBO:
					{
 						// tos = prod( top m elements on stack)
						int m = pops->d.index;
						while( --m )
						{
							flTOS *= *(pSP++);
						}
						break;
					}
					break;

					case STUDIO_DOMINATE:
					{
						// tos *= 1-prod( next top m elements on stack)
						int m = pops->d.index;
						float dv = *(pSP++);
						while( --m )
						{
							dv *= *(pSP++);
						}
						flTOS *= 1.0 - dv;
						break;
					}
					break;
					case STUDIO_2WAY_0:
					{ 
						int m = pFlexcontroller( (LocalFlexController_t)pops->d.index )->localToGlobal;
						*(--pSP) = flTOS;
						flTOS = RemapValClamped( src[m], -1.0f, 0.0f, 1.0f, 0.0f );
					}
					break;

					case STUDIO_2WAY_1:
					{ 
						int m = pFlexcontroller( (LocalFlexController_t)pops->d.index )->localToGlobal;
						*(--pSP) = flTOS;
						flTOS = RemapValClamped( src[m], 0.0f, 1.0f, 0.0f, 1.0f );
					}
					break;

					case STUDIO_NWAY:
					{
						LocalFlexController_t valueControllerIndex = static_cast< LocalFlexController_t >( (int) flTOS );
						int m = pFlexcontroller( valueControllerIndex )->localToGlobal;
						float flValue = src[ m ];
						int v = pFlexcontroller( (LocalFlexController_t)pops->d.index )->localToGlobal;

						const Vector4D filterRamp( pSP[3], pSP[2], pSP[1], pSP[0] );

						// Apply multicontrol remapping
						if ( flValue <= filterRamp.x || flValue >= filterRamp.w )
						{
							flValue = 0.0f;
						}
						else if ( flValue < filterRamp.y )
						{
							flValue = RemapValClamped( flValue, filterRamp.x, filterRamp.y, 0.0f, 1.0f );
						}
						else if ( flValue > filterRamp.z )
						{
							flValue = RemapValClamped( flValue, filterRamp.z, filterRamp.w, 1.0f, 0.0f );
						}
						else
						{
							flValue = 1.0f;
						}

						pSP+= 4;
						flTOS  = flValue * src[ v ];
					}
					break;

					case STUDIO_DME_LOWER_EYELID:
					{ 
						const mstudioflexcontroller_t *const pCloseLidV = 
							pFlexcontroller( (LocalFlexController_t)pops->d.index );
						const float flCloseLidV = 
							RemapValClamped( src[ pCloseLidV->localToGlobal ], pCloseLidV->min, pCloseLidV->max, 0.0f, 1.0f );
						
						const mstudioflexcontroller_t *const pCloseLid = 
							pFlexcontroller( static_cast< LocalFlexController_t >( (int)flTOS ) );
						const float flCloseLid = 
							RemapValClamped( src[ pCloseLid->localToGlobal ], pCloseLid->min, pCloseLid->max, 0.0f, 1.0f );

						int nBlinkIndex = static_cast< int >( pSP[0] );
						float flBlink = 0.0f;
						if ( nBlinkIndex >= 0 )
						{
							const mstudioflexcontroller_t *const pBlink = 
								pFlexcontroller( static_cast< LocalFlexController_t >( nBlinkIndex ) );
							flBlink = RemapValClamped( src[ pBlink->localToGlobal ], pBlink->min, pBlink->max, 0.0f, 1.0f );
						}

						int nEyeUpDownIndex = static_cast< int >( pSP[1] );
						float flEyeUpDown = 0.0f;
						if ( nEyeUpDownIndex >= 0 )
						{
							const mstudioflexcontroller_t *const pEyeUpDown =
								pFlexcontroller( static_cast< LocalFlexController_t >( nEyeUpDownIndex ) );
							flEyeUpDown = RemapValClamped( src[ pEyeUpDown->localToGlobal ], pEyeUpDown->min, pEyeUpDown->max, -1.0f, 1.0f );
						}

						if ( flEyeUpDown > 0.0 )
						{
							flTOS = ( 1.0f - flEyeUpDown ) * ( 1.0f - flCloseLidV ) * flCloseLid;
						}
						else
						{
							flTOS = ( 1.0f - flCloseLidV ) * flCloseLid;
						}
						pSP += 2;
					}
					break;

					case STUDIO_DME_UPPER_EYELID:
					{ 
						const mstudioflexcontroller_t *const pCloseLidV = pFlexcontroller( (LocalFlexController_t)pops->d.index );
						const float flCloseLidV = RemapValClamped( src[ pCloseLidV->localToGlobal ], pCloseLidV->min, pCloseLidV->max, 0.0f, 1.0f );
						
						const mstudioflexcontroller_t *const pCloseLid = pFlexcontroller( static_cast< LocalFlexController_t >( (int)flTOS ) );
						const float flCloseLid = RemapValClamped( src[ pCloseLid->localToGlobal ], pCloseLid->min, pCloseLid->max, 0.0f, 1.0f );
						
						int nBlinkIndex = static_cast< int >( pSP[0] );
						float flBlink = 0.0f;
						if ( nBlinkIndex >= 0 )
						{
							const mstudioflexcontroller_t *const pBlink = pFlexcontroller( static_cast< LocalFlexController_t >( nBlinkIndex ) );
							flBlink = RemapValClamped( src[ pBlink->localToGlobal ], pBlink->min, pBlink->max, 0.0f, 1.0f );
						}
						
						int nEyeUpDownIndex = static_cast< int >( pSP[1] );
						float flEyeUpDown = 0.0f;
						if ( nEyeUpDownIndex >= 0 )
						{
							const mstudioflexcontroller_t *const pEyeUpDown = pFlexcontroller( static_cast< LocalFlexController_t >( nEyeUpDownIndex ) );
							flEyeUpDown = RemapValClamped( src[ pEyeUpDown->localToGlobal ], pEyeUpDown->min, pEyeUpDown->max, -1.0f, 1.0f );
						}
						
						if ( flEyeUpDown < 0.0f )
						{
							flTOS = ( 1.0f + flEyeUpDown ) * flCloseLidV * flCloseLid;
						}
						else
						{
							flTOS = flCloseLidV * flCloseLid;
						}
						pSP += 2;
					}
					break;
				}
				
				pops++;
			} while( --nOps );
		dest[prule->flex] = flTOS;
	}
}

#define USE_OLD_FLEX_RULES_INTERPRETER

void CStudioHdr::RunFlexRules( const float *src, float *dest )
{
#ifndef USE_OLD_FLEX_RULES_INTERPRETER
	RunFlexRulesNew( src, dest );
#else
	RunFlexRulesOld( src, dest );
#endif

#if defined(_DEBUG) && !defined(USE_OLD_FLEX_RULES_INTERPRETER)
	float d1[ MAXSTUDIOFLEXDESC ];
	RunFlexRulesOld( src, d1 );

	for ( int i =0; i < numflexdesc(); i++)
	{
		if ( fabs( d1[i] - dest[i] ) > 0.001 )
		{
			Warning("bad %d old =%f new=%f\n", i, dest[i], d1[i] );
		}
	}
#endif // _DEBUG
}


//-----------------------------------------------------------------------------
//	propagate flags all the way down
//-----------------------------------------------------------------------------

void CStudioHdr::setBoneFlags( int iBone, int flags )
{
	((mstudiobone_t *)pBone( iBone ))->flags |= flags; 
	mstudiolinearbone_t *pLinear = pLinearBones();
	if ( pLinear )
	{
		*(pLinear->pflags( iBone )) |= flags;
	}

	m_boneFlags[ iBone ] |= flags; 
}

void CStudioHdr::clearBoneFlags( int iBone, int flags )
{ 
	((mstudiobone_t *)pBone( iBone ))->flags &= ~flags; 
	mstudiolinearbone_t *pLinear = pLinearBones();
	if ( pLinear )
	{
		*(pLinear->pflags( iBone )) &= ~flags;
	}

	m_boneFlags[ iBone ] &= ~flags; 
}


//-----------------------------------------------------------------------------
//	CODE PERTAINING TO ACTIVITY->SEQUENCE MAPPING SUBCLASS
//-----------------------------------------------------------------------------
#define iabs(i) (( (i) >= 0 ) ? (i) : -(i) )

CUtlSymbolTable g_ActivityModifiersTable;

extern void SetActivityForSequence( CStudioHdr *pstudiohdr, int i );
void CStudioHdr::CActivityToSequenceMapping::Initialize( const CStudioHdr * __restrict pstudiohdr )
{
	VPROF( "CStudioHdr::CActivityToSequenceMapping::Initialize" );
	// Algorithm: walk through every sequence in the model, determine to which activity
	// it corresponds, and keep a count of sequences per activity. Once the total count
	// is available, allocate an array large enough to contain them all, update the 
	// starting indices for every activity's section in the array, and go back through,
	// populating the array with its data.

	m_pStudioHdr = pstudiohdr->m_pStudioHdr;

	AssertMsg1( m_pSequenceTuples == NULL, "Tried to double-initialize sequence mapping for %s", pstudiohdr->pszName() );
	if ( m_pSequenceTuples != NULL )
		return; // don't double initialize.

	SetValidation(pstudiohdr);

	if ( ! pstudiohdr->SequencesAvailable() )
		return; // nothing to do.

	// Some studio headers have no activities at all. In those
	// cases we can avoid a lot of this effort.
	bool bFoundOne = false;	

	// for each sequence in the header...
	const int NumSeq = pstudiohdr->GetNumSeq();
	for ( int i = 0 ; i < NumSeq ; ++i )
	{
		const mstudioseqdesc_t &seqdesc = ((CStudioHdr *)pstudiohdr)->pSeqdesc( i );
#if defined(SERVER_DLL) || defined(CLIENT_DLL) || defined(GAME_DLL)
		if (!(seqdesc.flags & STUDIO_ACTIVITY))
		{
			// AssertMsg2( false, "Sequence %d on studiohdr %s didn't have its activity initialized!", i, pstudiohdr->pszName() );
			SetActivityForSequence( (CStudioHdr *)pstudiohdr, i );
		}
#endif

		// is there an activity associated with this sequence?
		if (seqdesc.activity >= 0)
		{
			bFoundOne = true;

			// look up if we already have an entry. First we need to make a speculative one --
			HashValueType entry(seqdesc.activity, 0, 1, iabs(seqdesc.actweight));
			UtlHashHandle_t handle = m_ActToSeqHash.Find(entry);
			if ( m_ActToSeqHash.IsValidHandle(handle) )
			{	
				// we already have an entry and must update it by incrementing count
				HashValueType * __restrict toUpdate = &m_ActToSeqHash.Element(handle);
				toUpdate->count += 1;
				toUpdate->totalWeight += iabs(seqdesc.actweight);
				Assert( toUpdate->totalWeight > 0 );
			}
			else
			{
				// we do not have an entry yet; create one.
				m_ActToSeqHash.Insert(entry);
			}
		}
	}

	// if we found nothing, don't bother with any other initialization!
	if (!bFoundOne)
		return;

	// Now, create starting indices for each activity. For an activity n, 
	// the starting index is of course the sum of counts [0..n-1]. 
	register int sequenceCount = 0;
	int topActivity = 0; // this will store the highest seen activity number (used later to make an ad hoc map on the stack)
	for ( UtlHashHandle_t handle = m_ActToSeqHash.GetFirstHandle() ; 
		  m_ActToSeqHash.IsValidHandle(handle) ;
		  handle = m_ActToSeqHash.GetNextHandle(handle) )
	{
		HashValueType &element = m_ActToSeqHash[handle];
		element.startingIdx = sequenceCount;
		sequenceCount += element.count;
		topActivity = MAX(topActivity, element.activityIdx);
	}
	

	// Allocate the actual array of sequence information. Note the use of restrict;
	// this is an important optimization, but means that you must never refer to this
	// array through m_pSequenceTuples in the scope of this function.
	SequenceTuple * __restrict tupleList = new SequenceTuple[sequenceCount];
	m_pSequenceTuples = tupleList; // save it off -- NEVER USE m_pSequenceTuples in this function!
	m_iSequenceTuplesCount = sequenceCount;



	// Now we're going to actually populate that list with the relevant data. 
	// First, create an array on the stack to store how many sequences we've written
	// so far for each activity. (This is basically a very simple way of doing a map.)
	// This stack may potentially grow very large; so if you have problems with it, 
	// go to a utlmap or similar structure.
	unsigned int allocsize = (topActivity + 1) * sizeof(int);
#define ALIGN_VALUE( val, alignment ) ( ( val + alignment - 1 ) & ~( alignment - 1 ) ) //  need macro for constant expression
	allocsize = ALIGN_VALUE(allocsize,16);
	int * __restrict seqsPerAct = static_cast<int *>(stackalloc(allocsize));
	memset(seqsPerAct, 0, allocsize);

	// okay, walk through all the sequences again, and write the relevant data into 
	// our little table.
	for ( int i = 0 ; i < NumSeq ; ++i )
	{
		const mstudioseqdesc_t &seqdesc = ((CStudioHdr *)pstudiohdr)->pSeqdesc( i );
		if (seqdesc.activity >= 0)
		{
			const HashValueType &element = m_ActToSeqHash[m_ActToSeqHash.Find(HashValueType(seqdesc.activity, 0, 0, 0))];
			
			// If this assert trips, we've written more sequences per activity than we allocated 
			// (therefore there must have been a miscount in the first for loop above).
			int tupleOffset = seqsPerAct[seqdesc.activity];
			Assert( tupleOffset < element.count );

			if ( seqdesc.numactivitymodifiers > 0 )
			{
				// add entries for this model's activity modifiers
				(tupleList + element.startingIdx + tupleOffset)->pActivityModifiers = new CUtlSymbol[ seqdesc.numactivitymodifiers ];
				(tupleList + element.startingIdx + tupleOffset)->iNumActivityModifiers = seqdesc.numactivitymodifiers;

				for ( int k = 0; k < seqdesc.numactivitymodifiers; k++ )
				{
					(tupleList + element.startingIdx + tupleOffset)->pActivityModifiers[ k ] = g_ActivityModifiersTable.AddString( seqdesc.pActivityModifier( k )->pszName() );
				}
			}
			else
			{
				(tupleList + element.startingIdx + tupleOffset)->pActivityModifiers = NULL;
				(tupleList + element.startingIdx + tupleOffset)->iNumActivityModifiers = 0;
			}

			// You might be tempted to collapse this pointer math into a single pointer --
			// don't! the tuple list is marked __restrict above.
			(tupleList + element.startingIdx + tupleOffset)->seqnum = i; // store sequence number
			(tupleList + element.startingIdx + tupleOffset)->weight = iabs(seqdesc.actweight);

			// We can't have weights of 0
			// Assert( (tupleList + element.startingIdx + tupleOffset)->weight > 0 );
			if ( (tupleList + element.startingIdx + tupleOffset)->weight == 0 )
			{
				(tupleList + element.startingIdx + tupleOffset)->weight = 1;
			}

			seqsPerAct[seqdesc.activity] += 1;
		}
	}

#ifdef DBGFLAG_ASSERT
	// double check that we wrote exactly the right number of sequences.
	unsigned int chkSequenceCount = 0;
	for (int j = 0 ; j <= topActivity ; ++j)
	{
		chkSequenceCount += seqsPerAct[j];
	}
	Assert(chkSequenceCount == m_iSequenceTuplesCount);
#endif

}

/// Force Initialize() to occur again, even if it has already occured.
void CStudioHdr::CActivityToSequenceMapping::Reinitialize( CStudioHdr *pstudiohdr )
{
	if (m_pSequenceTuples)
	{
		delete m_pSequenceTuples;
		m_pSequenceTuples = NULL;
	}
	m_ActToSeqHash.RemoveAll();

	Initialize(pstudiohdr);
}

// Look up relevant data for an activity's sequences. This isn't terribly efficient, due to the
// load-hit-store on the output parameters, so the most common case -- SelectWeightedSequence --
// is specially implemented.
const CStudioHdr::CActivityToSequenceMapping::SequenceTuple *CStudioHdr::CActivityToSequenceMapping::GetSequences( int forActivity, int * __restrict outSequenceCount, int * __restrict outTotalWeight )
{
	// Construct a dummy entry so we can do a hash lookup (the UtlHash does not divorce keys from values)

	HashValueType entry(forActivity, 0, 0, 0);
	UtlHashHandle_t handle = m_ActToSeqHash.Find(entry);
	
	if (m_ActToSeqHash.IsValidHandle(handle))
	{
		const HashValueType &element = m_ActToSeqHash[handle];
		const SequenceTuple *retval = m_pSequenceTuples + element.startingIdx;
		*outSequenceCount = element.count;
		*outTotalWeight = element.totalWeight;

		return retval;
	}
	else
	{
		// invalid handle; return NULL.
		// this is actually a legit use case, so no need to assert.
		return NULL;
	}
}

int CStudioHdr::CActivityToSequenceMapping::NumSequencesForActivity( int forActivity )
{
	// If this trips, you've called this function on something that doesn't 
	// have activities.
	//Assert(m_pSequenceTuples != NULL);
	if ( m_pSequenceTuples == NULL )
		return 0;

	HashValueType entry(forActivity, 0, 0, 0);
	UtlHashHandle_t handle = m_ActToSeqHash.Find(entry);
	if (m_ActToSeqHash.IsValidHandle(handle))
	{
		return m_ActToSeqHash[handle].count;
	}
	else
	{
		return 0;
	}
}

static CStudioHdr::CActivityToSequenceMapping emptyMapping;

// double-check that the data I point to hasn't changed
bool CStudioHdr::CActivityToSequenceMapping::ValidateAgainst( const CStudioHdr * RESTRICT pstudiohdr ) RESTRICT
{
	return ( this == &emptyMapping || 
			 ( m_pStudioHdr == pstudiohdr->m_pStudioHdr && m_expectedVModel == pstudiohdr->GetVirtualModel() ) );
}

void CStudioHdr::CActivityToSequenceMapping::SetValidation( const CStudioHdr *RESTRICT pstudiohdr ) RESTRICT
{
	m_expectedVModel = pstudiohdr->GetVirtualModel();
}

struct StudioHdrToActivityMapEntry_t
{
	long checksum;
	char name[64];
	int	nRefs;
	CStudioHdr::CActivityToSequenceMapping *pMap;
};

CUtlMap<const studiohdr_t *, StudioHdrToActivityMapEntry_t> g_StudioHdrToActivityMaps( DefLessFunc( const studiohdr_t * ) );
CThreadFastMutex g_StudioHdrToActivityMapsLock;

CStudioHdr::CActivityToSequenceMapping *CStudioHdr::CActivityToSequenceMapping::FindMapping( const CStudioHdr *pHdr )
{
	VPROF( "CStudioHdr::CActivityToSequenceMapping::FindMapping" );

	if ( !pHdr->SequencesAvailable() || pHdr->GetNumSeq() <= 1 )
	{
		return &emptyMapping;
	}

	Assert( !pHdr->m_pActivityToSequence );

	AUTO_LOCK( g_StudioHdrToActivityMapsLock );
	const studiohdr_t *pRealHdr = pHdr->m_pStudioHdr;
	int i = g_StudioHdrToActivityMaps.Find( pRealHdr );
	if ( i != g_StudioHdrToActivityMaps.InvalidIndex() )
	{
		if ( !IsX360() && ( g_StudioHdrToActivityMaps[i].checksum != pRealHdr->checksum || Q_strcmp( g_StudioHdrToActivityMaps[i].name, pRealHdr->name ) != 0 ) )
		{
			AssertFatal( g_StudioHdrToActivityMaps[i].nRefs == 0 );
			delete g_StudioHdrToActivityMaps[i].pMap;
			g_StudioHdrToActivityMaps.RemoveAt( i );
		}
		else
		{
			Assert( g_StudioHdrToActivityMaps[i].checksum == pRealHdr->checksum && Q_strcmp( g_StudioHdrToActivityMaps[i].name, pRealHdr->name ) == 0 );
			g_StudioHdrToActivityMaps[i].nRefs++;
			return g_StudioHdrToActivityMaps[i].pMap;
		}
	}

	i = g_StudioHdrToActivityMaps.Insert( pRealHdr );

	g_StudioHdrToActivityMaps[i].checksum = pRealHdr->checksum;
	Q_strncpy( g_StudioHdrToActivityMaps[i].name, pRealHdr->name, 64 );
	g_StudioHdrToActivityMaps[i].nRefs = 1;
	g_StudioHdrToActivityMaps[i].pMap = new CStudioHdr::CActivityToSequenceMapping;
	g_StudioHdrToActivityMaps[i].pMap->Initialize( pHdr );

	return g_StudioHdrToActivityMaps[i].pMap;
}

void CStudioHdr::CActivityToSequenceMapping::ReleaseMapping( CActivityToSequenceMapping *pMap )
{
	if ( pMap && pMap != &emptyMapping)
	{
		VPROF( "CStudioHdr::CActivityToSequenceMapping::ReleaseMapping" );
		AUTO_LOCK( g_StudioHdrToActivityMapsLock );
		int i = g_StudioHdrToActivityMaps.Find( pMap->m_pStudioHdr );
		if ( i != g_StudioHdrToActivityMaps.InvalidIndex() )
		{
			Assert( g_StudioHdrToActivityMaps[i].nRefs > 0 );
			g_StudioHdrToActivityMaps[i].nRefs--;
		}
		else
		{
			Assert( 0 );
		}
	}
}

void CStudioHdr::CActivityToSequenceMapping::ResetMappings()
{
	for ( int i = g_StudioHdrToActivityMaps.FirstInorder(); i != g_StudioHdrToActivityMaps.InvalidIndex(); i = g_StudioHdrToActivityMaps.NextInorder( i ) )
	{
		if ( g_StudioHdrToActivityMaps[i].nRefs == 0 )
		{
			delete g_StudioHdrToActivityMaps[i].pMap;
		}
		else
		{
			Msg( "****************************************************************\n" );
			Msg( "****************************************************************\n" );
			Msg( "*************  DO NOT IGNORE ME  *******************************\n" );
			Msg( "****************************************************************\n" );
			Msg( "****************************************************************\n" );
			Warning( "Studio activity sequence mapping leak! (%s, %d)\n", g_StudioHdrToActivityMaps[i].name, g_StudioHdrToActivityMaps[i].nRefs );
			Msg( "****************************************************************\n" );
			Msg( "****************************************************************\n" );
			Msg( "****************************************************************\n" );
			Msg( "****************************************************************\n" );
			Msg( "****************************************************************\n" );
		}
	}
	g_StudioHdrToActivityMaps.RemoveAll();
}





//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

int CStudioHdr::LookupSequence( const char *pszName )
{
	int iSequence = m_namedSequence.Find( pszName );

	if ( iSequence == m_namedSequence.InvalidIndex() )
	{
		for (iSequence = 0; iSequence < GetNumSeq(); iSequence++)
		{
			if ( V_stricmp( pSeqdesc( iSequence ).pszLabel(), pszName ) == 0)
				break;
		}
		if ( iSequence == GetNumSeq() )
		{
			m_namedSequence.Insert( pszName, -1 );
			return -1;
		}
		else
		{
			m_namedSequence.Insert( pszName, iSequence );
			return iSequence;
		}
	}
	else
	{
		return m_namedSequence[iSequence];
	}
}

CSoftbody * CStudioHdr::InitSoftbody( CSoftbodyEnvironment *pSoftbodyEnvironment )
{
#if defined( CLIENT_DLL ) || defined( ENABLE_STUDIO_SOFTBODY )

	if ( m_pSoftbody )
	{
		ExecuteOnce( Warning( "InitSoftbody is called with Softbody already existing. This pattern is suspicious and will not work if skeleton has changed, please show Sergiy a repro.\n" ) );
		return m_pSoftbody;
	}

	if ( m_pStudioHdr->studiohdr2index )
	{
		const studiohdr2_t *pHdr2 = m_pStudioHdr->pStudioHdr2();
		if ( const PhysFeModelDesc_t *pFeModelDesc = pHdr2->m_pFeModel.GetPtr() )
		{
			uint numCtrlNames = pFeModelDesc->m_CtrlName.Count();
			CBufferStrider feModelData( MemAlloc_AllocAligned( sizeof( CFeModel ) + sizeof( CSoftbody ) + sizeof( char* ) * numCtrlNames, 16 ) );
			CFeModel *pFeModel = feModelData.Stride<CFeModel>();
			m_pSoftbody = feModelData.Stride< CSoftbody >();
			Construct( m_pSoftbody );
#ifdef _DEBUG
			if ( softbody_debug.GetInt() )
			{
				const char *pSubstr = softbody_debug_substr.GetString();
				Msg( "InitSoftbody%s %s %p\n", ( *pSubstr && V_stristr( name(), pSubstr ) ) ? "!":"", name(), m_pSoftbody );
			}
#endif
			char **pCtrlNames = feModelData.Stride< char* >( numCtrlNames );
			Clone( pFeModelDesc, 0, pCtrlNames, pFeModel );
			int numModelBones = numbones();

			m_pSoftbody->Init( pSoftbodyEnvironment, pFeModel, numModelBones );
			for ( uint nCtrl = 0; nCtrl < pFeModel->m_nCtrlCount; ++nCtrl )
			{
				int nModelBone = Studio_BoneIndexByName( this, pFeModel->m_pCtrlName[ nCtrl ] );
				m_pSoftbody->BindModelBoneToCtrl( nModelBone, nCtrl );
				//m_pStudioHdr->setBoneFlags( nModelBone, BONE_ALWAYS_PROCEDURAL );
				//const_cast< mstudiobone_t* >( m_pStudioHdr->pBone( nModelBone ) )->proctype = STUDIO_PROC_SOFTBODY;						
			}
		}
	}
#endif
	return m_pSoftbody;
}

void CStudioHdr::FreeSoftbody()
{
#if defined( CLIENT_DLL ) || defined( ENABLE_STUDIO_SOFTBODY )
	if ( m_pSoftbody )
	{
#ifdef _DEBUG
		if ( softbody_debug.GetInt() )
			Msg("FreeSoftbody %s %p\n", name(), m_pSoftbody);
#endif
		CFeModel *pFeModel = m_pSoftbody->GetFeModel();
		m_pSoftbody->Shutdown();
		Assert( pFeModel );
		MemAlloc_FreeAligned( pFeModel );
		m_pSoftbody = NULL;
	}
#endif
}
