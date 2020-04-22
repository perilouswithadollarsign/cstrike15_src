//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "mathlib/mathlib.h"

#include "bone_setup_PS3.h"
#include "bone_utils_PS3.h"



//--------------------------------------------------------------------------------------------------------------------
//
// local data for a bone job
//
//--------------------------------------------------------------------------------------------------------------------
#define CPSPATH_FALSE						0
#define CPSPATH_CALCANIM1					1
#define CPSPATH_CALCANIM1_SCALE1MINUSS0		2
#define CPSPATH_CALCANIM1_SCALES0			3
#define CPSPATH_CALCANIM2_S0				4
#define CPSPATH_CALCANIM2_S1				5
#define CPSPATH_CALCANIM3					6
#define CPSPATH_CALCANIM4					7



ALIGN16 byte		g_ls_Stack[ (LS_FLOATSTACK_SIZE * sizeof(float)) + (LS_INTSTACK_SIZE * sizeof(int)) ];
byte				*g_ls_StackPtr;
byte				*g_ls_StackPtr_MAX;


#if defined(__SPU__)

BoneVector				*g_posInit;
BoneQuaternion			*g_qInit;
ikcontextikrule_t_PS3	*g_addDep_IKRules;
int						g_addDep_numIKRules;

#else

BoneVector				g_posInit[ MAXSTUDIOBONES_PS3 ];			// 2k
BoneQuaternion			g_qInit[ MAXSTUDIOBONES_PS3 ];				// 2k
ikcontextikrule_t_PS3	g_addDep_IKRules[ MAX_IKRULES ]	ALIGN16;
int						g_addDep_numIKRules;

#endif



//static int				g_accumPoseCount;




#if !defined(__SPU__)
#include <string.h>
#include "tier0/vprof.h"

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

extern IVJobs * g_pVJobs;

CBoneJobs g_BoneJobs;
CBoneJobs* g_pBoneJobs = &g_BoneJobs;

job_accumpose::JobDescriptor_t g_AccumPoseJobDescriptor ALIGN128;


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

void CBoneJobs::Init( void )
{
	m_bEnabled		= false;

	m_boneJobCount	= 0;
	m_boneJobData.EnsureCapacity( 96 );

	// requires a SPURS instance, so register with VJobs
	if( g_pVJobs )
	{
		g_pVJobs->Register( this );
	}
}

void CBoneJobs::Shutdown()
{
	g_pVJobs->Unregister( this ); 
}

void CBoneJobs::OnVjobsInit()
{
	m_bEnabled = true;

	g_AccumPoseJobDescriptor.header = *m_pRoot->m_pJobAccumPose;

	g_AccumPoseJobDescriptor.header.useInOutBuffer	= 1;
	g_AccumPoseJobDescriptor.header.sizeStack		= (12*1024)/8;
	g_AccumPoseJobDescriptor.header.sizeInOrInOut	= 0;
}

void CBoneJobs::OnVjobsShutdown()
{
	m_bEnabled = false;
}


void CBoneJobs::StartFrame( int maxBoneJobs )
{
	m_boneJobData.EnsureCapacity( maxBoneJobs );
}

void CBoneJobs::EndFrame( void )
{
	m_boneJobData.RemoveAll();
}


void CBoneJobs::ResetBoneJobs( void )
{
	m_boneJobCount			= 0;
	m_boneJobNextSPURSPort	= 0;
}

int CBoneJobs::AddBoneJob( void )
{
	return m_boneJobCount++;
}

int CBoneJobs::GetNumBoneJobs( void )
{
	return m_boneJobCount;
}

PS3BoneJobData *CBoneJobs::GetJobData( int job )
{
	return &m_boneJobData[ job ];
}

int	CBoneJobs::GetNextFreeSPURSPort( void )
{
	m_boneJobNextSPURSPort = (m_boneJobNextSPURSPort + 1) % VJobsRoot::MAXPORTS_ANIM;

	return m_boneJobNextSPURSPort;
}


//-----------------------------------------------------------------------------
// Contains support for running BoneJobs on SPU
// 
// each bonejob contains a number of AccumulatePose calls that were 'discovered'
// in the 1st pass on PPU - this pass filled the bonejob packet with data for all
// AccumulatePose calls that take place on C_BaseAnimating
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: calculate a pose for a single sequence
//-----------------------------------------------------------------------------
void InitPose_PS3(
	const CStudioHdr *pStudioHdr,
	BoneVector pos[], 
	BoneQuaternion q[],
	int boneMask 
	)
{
	// const fltx4 zeroQ = Four_Origin;
	SNPROF_ANIM("InitPose_PS3");

	if ( mstudiolinearbone_t *pLinearBones = pStudioHdr->pLinearBones() )
	{
		int numBones = pStudioHdr->numbones();
		
		AssertFatal( sizeof(BoneQuaternion) == sizeof(Quaternion) );
		memcpy( q, (((byte *)pLinearBones) + pLinearBones->quatindex), sizeof( Quaternion ) * numBones );

		// want to align pos entries to 16B
//		memcpy( pos, (((byte *)pLinearBones) + pLinearBones->posindex), sizeof( Vector ) * numBones );
		Vector *pSrcBones = (Vector *)(((byte *)pLinearBones) + pLinearBones->posindex);
		for( int i = 0; i < pStudioHdr->numbones(); i++ )
		{
			pos[i] = pSrcBones[i];
		}

	}
	else
	{
		for( int i = 0; i < pStudioHdr->numbones(); i++ )
		{
			if( pStudioHdr->boneFlags(  i ) & boneMask ) 
			{
				const mstudiobone_t *pbone = pStudioHdr->pBone( i );
				pos[i] = pbone->pos;
				q[i] = pbone->quat;
			}
			/* // unnecessary to initialize unused bones since they are ignored downstream.
			else
			{
				pos[i].Zero();
				// q[i] = zeroQ;
				StoreAlignedSIMD(q[i].Base(), zeroQ);
			}
			*/
		}
	}
}
	
//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
inline bool PoseIsAllZeros_PS3( 
						   const CStudioHdr *pStudioHdr,
						   int sequence, 
						   mstudioseqdesc_t	&seqdesc,
						   int i0,
						   int i1
						   )
{
	int baseanim;

	// remove "zero" positional blends
	baseanim = pStudioHdr->iRelativeAnim( sequence, seqdesc.anim(i0  ,i1 ) );
	mstudioanimdesc_t		&anim = ((CStudioHdr *)pStudioHdr)->pAnimdesc( baseanim );
	return (anim.flags & STUDIO_ALLZEROS) != 0;
}


//-----------------------------------------------------------------------------
// fill animdata_SPU struct
//-----------------------------------------------------------------------------
static void SetAnimData_AnimDesc( animData_SPU *pAnim, accumposeentry_SPU *pPoseEntry, mstudioanimdesc_t &animdesc, int animIndex )
{
	SNPROF_ANIM("SetAnimData_AnimDesc");

	int	  iFrame;
	float s;
	float flStall;

	pAnim->pEA_animdesc							= &animdesc;
			
	float fFrame								= pPoseEntry->cycle * (animdesc.numframes - 1);

	iFrame										= (int)fFrame;
	s											= (fFrame - iFrame);

	int iLocalFrame								= iFrame;

	pAnim->pEA_animdesc_pFrameanim				= NULL;
	pAnim->pEA_animdesc_panim					= NULL;

	if( animdesc.flags & STUDIO_FRAMEANIM )
	{
		const mstudio_frame_anim_t *pFrameanim	= (mstudio_frame_anim_t *)animdesc.pAnim( &iLocalFrame, flStall );

		pAnim->animdesc_iLocalFrame				= iLocalFrame;

		pAnim->pEA_animdesc_pFrameanim			= (void *)pFrameanim;

	}
	else
	{
		pAnim->pEA_animdesc_panim				= (void *)animdesc.pAnim( &iLocalFrame, flStall );

		pAnim->animdesc_iLocalFrame				= iLocalFrame;

	}


	pAnim->pEA_animdesc_ikrule					= NULL;
	pAnim->pEA_animdesc_ikrulezeroframe			= NULL;

	mstudioikrule_t *pIKRule					= animdesc.pIKRule( 0 );
	if( pIKRule )
	{
		pAnim->pEA_animdesc_ikrule				= pIKRule;
	}
	else
	{
		mstudioikrulezeroframe_t *pZeroFrameRule = animdesc.pIKRuleZeroFrame( 0 );
		pAnim->pEA_animdesc_ikrulezeroframe		 = pZeroFrameRule;
	}

	pAnim->flStall = flStall;
}

//-----------------------------------------------------------------------------
// Fill PoseEntry anim data
//-----------------------------------------------------------------------------
bool CBoneSetup_PS3::SetAnimData( accumposeentry_SPU *pPoseEntry, const CStudioHdr *pStudioHdr, mstudioseqdesc_t &seqdesc, int sequence, int x, int y, int animIndex, float weight )
{
	SNPROF_ANIM("CBoneSetup_PS3::SetAnimData");

	animData_SPU *pAnim		= &pPoseEntry->anims[ animIndex ];

	pAnim->seqdesc_anim		= seqdesc.anim( x, y );
	pAnim->seqdesc_weight	= weight;


	virtualmodel_t *pVModel	= pStudioHdr->GetVirtualModel();
	if( pVModel )
	{
		int baseanimation					= pStudioHdr->iRelativeAnim( sequence, pAnim->seqdesc_anim );
		mstudioanimdesc_t &animdesc			= ((CStudioHdr *)pStudioHdr)->pAnimdesc( baseanimation );

		if( animdesc.numlocalhierarchy )
		{
			// don't take SPU path
			m_errorFlags = BONEJOB_ERROR_LOCALHIER;
			// TODO: follow this path out quickly
			return false;
		}

		SetAnimData_AnimDesc( pAnim, pPoseEntry, animdesc, animIndex );

		pAnim->pEA_animgroup_masterbone		= pVModel->pAnimGroup( baseanimation )->masterBone.Base();

		const studiohdr_t *pAnimStudioHdr;

		pAnimStudioHdr						= ((CStudioHdr *)pStudioHdr)->pAnimStudioHdr( baseanimation );

		pAnim->animstudiohdr_numbones		= pAnimStudioHdr->numbones;

		mstudiolinearbone_t *pLinearBones	= pAnimStudioHdr->pLinearBones();
		pAnim->pEA_anim_linearBones         = pLinearBones;
		pAnim->pEA_anim_bones_pos			= &pAnimStudioHdr->pBone(0)->pos;
		pAnim->pEA_anim_bones_flags			= &pAnimStudioHdr->pBone(0)->flags;
	}
	else
	{
		mstudioanimdesc_t &animdesc	= ((CStudioHdr *)pStudioHdr)->pAnimdesc( pAnim->seqdesc_anim );

		if( animdesc.numlocalhierarchy )
		{
			// don't take SPU path
			m_errorFlags = BONEJOB_ERROR_LOCALHIER;
			// TODO: follow this path out quickly
			return false;
		}

		SetAnimData_AnimDesc( pAnim, pPoseEntry, animdesc, animIndex );

		pAnim->animstudiohdr_numbones		= pStudioHdr->GetRenderHdr()->numbones;
	}


	return true;
}

//-----------------------------------------------------------------------------
// CalcPoseSingle PPU pass - fill PoseEntry
// Get this working first before moving more to SPU
//-----------------------------------------------------------------------------
bool CBoneSetup_PS3::CalcPoseSingle( 
						 accumposeentry_SPU *pPoseEntry,						
						 const CStudioHdr *pStudioHdr,
						 mstudioseqdesc_t &seqdesc,
						 int sequence, 
						 float cycle,
						 const float poseParameter[],
						 float flTime )
{
	SNPROF_ANIM( "CBoneSetup_PS3::CalcPoseSingle" );

	ConVarRef anim_3wayblend( "anim_3wayblend" );

	bool bResult = true;

	float s0, s1;
	int   i0, i1;


	s0 = 0.0f; s1 = 0.0f;

	i0 = Studio_LocalPoseParameter( pStudioHdr, poseParameter, seqdesc, sequence, 0, s0 );
	i1 = Studio_LocalPoseParameter( pStudioHdr, poseParameter, seqdesc, sequence, 1, s1 );

	//pPoseEntry->cps = Studio_CPS( pStudioHdr, seqdesc, sequence, poseParameter );

	if( seqdesc.flags & STUDIO_REALTIME )
	{
		float cps = Studio_CPS( pStudioHdr, seqdesc, sequence, poseParameter );
		cycle = flTime * cps;
		cycle = cycle - (int)cycle;
	}
	else if( seqdesc.flags & STUDIO_CYCLEPOSE )
	{
		int iPose = pStudioHdr->GetSharedPoseParameter( sequence, seqdesc.cycleposeindex );
		if( iPose != -1 )
		{
			/*
			const mstudioposeparamdesc_t &Pose = pStudioHdr->pPoseParameter( iPose );
			cycle = poseParameter[ iPose ] * (Pose.end - Pose.start) + Pose.start;
			*/
			cycle = poseParameter[ iPose ];
		}
		else
		{
			cycle = 0.0f;
		}
	}
	else if( cycle < 0.0f || cycle >= 1.0f )
	{
		if( seqdesc.flags & STUDIO_LOOPING )
		{
			cycle = cycle - (int)cycle;
			if( cycle < 0 ) cycle += 1.0f;
		}
		else
		{
			cycle = clamp( cycle, 0.0f, 1.0f );
		}
	}

	pPoseEntry->cyclePoseSingle	 = cycle;

	pPoseEntry->i0 = i0;
	pPoseEntry->i1 = i1;
	pPoseEntry->s0 = s0;
	pPoseEntry->s1 = s1;

	pPoseEntry->animIndices[0] = -1;
	pPoseEntry->animIndices[1] = -1;
	pPoseEntry->animIndices[2] = -1;
	pPoseEntry->animIndices[3] = -1;

	pPoseEntry->anims[0].seqdesc_weight = 0.0f;
	pPoseEntry->anims[1].seqdesc_weight = 0.0f;
	pPoseEntry->anims[2].seqdesc_weight = 0.0f;
	pPoseEntry->anims[3].seqdesc_weight = 0.0f;

	pPoseEntry->anims[0].animstudiohdr_numbones = 0;
	pPoseEntry->anims[1].animstudiohdr_numbones = 0;
	pPoseEntry->anims[2].animstudiohdr_numbones = 0;
	pPoseEntry->anims[3].animstudiohdr_numbones = 0;

	if( s0 < 0.001f )
	{
		if( s1 < 0.001f )
		{
			if( PoseIsAllZeros_PS3( pStudioHdr, sequence, seqdesc, i0, i1 ) )
			{
				pPoseEntry->cpsPath = CPSPATH_FALSE;

				bResult = false;
			}
			else
			{
				pPoseEntry->cpsPath = CPSPATH_CALCANIM1;

				pPoseEntry->animIndices[0] = 0;
				SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0, i1, 0, 1.0f );
			}
		}
		else if( s1 > 0.999f )
		{
			pPoseEntry->cpsPath = CPSPATH_CALCANIM1;

			pPoseEntry->animIndices[0] = 2;
			SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0, i1+1, 2, 1.0f );
		}
		else
		{
			pPoseEntry->cpsPath = CPSPATH_CALCANIM2_S1;

			pPoseEntry->animIndices[0] = 0;
			pPoseEntry->animIndices[1] = 2;
			SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0, i1, 0, 1.0f - s1 );
			SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0, i1+1, 2, s1 );
		}
	}
	else if( s0 > 0.999f )
	{
		if( s1 < 0.001f )
		{
			if( PoseIsAllZeros_PS3( pStudioHdr, sequence, seqdesc, i0+1, i1 ) )
			{
				pPoseEntry->cpsPath		= CPSPATH_FALSE;

				bResult = false;
			}
			else
			{
				pPoseEntry->cpsPath		= CPSPATH_CALCANIM1;

				pPoseEntry->animIndices[0]	= 1;
				SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0+1, i1, 1, 1.0f );
			}
		}
		else if( s1 > 0.999f )
		{
			pPoseEntry->cpsPath = CPSPATH_CALCANIM1;

			pPoseEntry->animIndices[0]	= 3;
			SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0+1, i1+1, 3, 1.0f );
		}
		else
		{
			pPoseEntry->cpsPath = CPSPATH_CALCANIM2_S1;

			pPoseEntry->animIndices[0]	= 1;
			pPoseEntry->animIndices[1]	= 3;
			SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0+1, i1, 1, 1.0f - s1 );
			SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0+1, i1+1, 3, s1 );
		}
	}
	else
	{
		if( s1 < 0.001f )
		{
			pPoseEntry->animIndices[0]	= 0;
			pPoseEntry->animIndices[1]	= 1;
			SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0, i1, 0, 1.0f - s0 );
			SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0+1, i1, 1, s0 );

			if( PoseIsAllZeros_PS3( pStudioHdr, sequence, seqdesc, i0+1, i1 ) )
			{
				pPoseEntry->cpsPath			= CPSPATH_CALCANIM1_SCALE1MINUSS0;
			}
			else if( PoseIsAllZeros_PS3( pStudioHdr, sequence, seqdesc, i0, i1 ) )
			{
				pPoseEntry->cpsPath			= CPSPATH_CALCANIM1_SCALES0;
			}
			else
			{
				pPoseEntry->cpsPath			= CPSPATH_CALCANIM2_S0;
			}
		}
		else if( s1 > 0.999f )
		{
			pPoseEntry->cpsPath = CPSPATH_CALCANIM2_S0;

			pPoseEntry->animIndices[0]		= 2;
			pPoseEntry->animIndices[1]		= 3;
			SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0, i1+1, 2, 1.0f - s0 );
			SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0+1, i1+1, 3, s0 );
		}
		else if( !anim_3wayblend.GetBool() )
		{
			pPoseEntry->cpsPath = CPSPATH_CALCANIM4;

			pPoseEntry->animIndices[0]		= 0;
			pPoseEntry->animIndices[1]		= 1;
			pPoseEntry->animIndices[2]		= 2;
			pPoseEntry->animIndices[3]		= 3;
			SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0, i1, 0, (1.0f - s0) * (1.0f - s1) );
			SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0+1, i1, 1, (s0) * (1.0f - s1) );
			SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0, i1+1, 2, (1.0f - s0) * (s1) );
			SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0+1, i1+1, 3, (s0) * (s1) );
		}
		else
		{
			pPoseEntry->cpsPath = CPSPATH_CALCANIM3;

			pPoseEntry->animIndices[0]		= 0;
			pPoseEntry->animIndices[1]		= 1;
			pPoseEntry->animIndices[2]		= 2;
			pPoseEntry->animIndices[3]		= 3;
			// weights get fixed up later
			SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0, i1, 0, 0.0f );		
			SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0+1, i1, 1, 0.0f );
			SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0, i1+1, 2, 0.0f );
			SetAnimData( pPoseEntry, pStudioHdr, seqdesc, sequence, i0+1, i1+1, 3, 0.0f );
		}
	}


	if( pPoseEntry->animIndices[0] == -1 )
	{
		// ik may use the zeroth entry - why?
		virtualmodel_t *pVModel	= pStudioHdr->GetVirtualModel();

		if( pVModel )
		{
			int baseanimation					= pStudioHdr->iRelativeAnim( sequence, seqdesc.anim( i0, i1 ) );
			mstudioanimdesc_t &animdesc			= ((CStudioHdr *)pStudioHdr)->pAnimdesc( baseanimation );
			pPoseEntry->anims[ 0 ].pEA_animdesc	= &animdesc;
		}
		else
		{
			mstudioanimdesc_t &animdesc			= ((CStudioHdr *)pStudioHdr)->pAnimdesc( seqdesc.anim( i0, i1 ) );
			pPoseEntry->anims[ 0 ].pEA_animdesc	= &animdesc;
		}
	}

	// Real IK weights
// 	0 :  (1.0f - s0) * (1.0f - s1);
// 	1 :  (s0) * (1.0f - s1);
// 	2 :  (1.0f - s0) * (s1);
// 	3 :  (s0) * (s1);

	return bResult;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void AddSequenceLocks_PS3( accumposeentry_SPU* pPoseEntry, const CStudioHdr* pStudioHdr, mstudioseqdesc_t &seqdesc )
{
	pPoseEntry->pEA_seqdesc_iklocks = NULL;

	if( pStudioHdr->numikchains() == 0)
	{
		return;
	}

	if( seqdesc.numiklocks == 0 )
	{
		return;
	}

	pPoseEntry->pEA_seqdesc_iklocks = seqdesc.pIKLock( 0 );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void AddAutoLocks_PS3( bonejob_SPU *pBonejob, const CStudioHdr* pStudioHdr )
{
	SNPROF_ANIM("AddAutoLocks_PS3");

	if( pStudioHdr->GetNumIKAutoplayLocks() == 0 )
	{
		return;
	}

 	AssertFatal( pStudioHdr->GetNumIKAutoplayLocks() < MAX_IKLOCKS );
 
 	for( int i = 0; i < pStudioHdr->GetNumIKAutoplayLocks(); i++ )
 	{
 		const mstudioiklock_t &lock = ((CStudioHdr *)pStudioHdr)->pIKAutoplayLock( i );

		pBonejob->pEA_IKAutoplayLocks[ i ] = (void *)&lock;
 	}
}



//-----------------------------------------------------------------------------
// Add an AccumulatePose call to the bonejob
//-----------------------------------------------------------------------------
void CBoneSetup_PS3::AccumulatePose_AddToBoneJob( bonejob_SPU* pSPUJob,
												  int sequence, 
												  float cycle,
												  float flWeight,
												  CIKContext *pIKContext,
												  int pqStackLevel  )
{
	SNPROF_ANIM( "CBoneSetup_PS3::AccumulatePose_AddToBoneJob" );

	if( pSPUJob->numTotalPoses >= MAX_ACCUMPOSECALLS )
	{
		m_errorFlags = BONEJOB_ERROR_EXCEEDEDMAXCALLS;
		return;
	}

	if( ( pqStackLevel+1 ) >= MAX_PQSTACKLEVEL )
	{
		// pq stack depth exceeded, early out of job, run from scratch on PPU
		m_errorFlags = BONEJOB_ERROR_EXCEEDEDPQSTACK;
		return;
	}

	flWeight = clamp( flWeight, 0.0f, 1.0f );

	if( sequence < 0 )
	{
		// just skip, no error flags?
		return;
	}

	if( sequence >= m_pStudioHdr->GetNumSeq() ) 
	{
		sequence = 0;
	}

	//-------------------------------------------------------------------------------------
	// get stack entry
	accumposeentry_SPU	*pPoseEntry = &pSPUJob->accumPoseEntry[ pSPUJob->numTotalPoses++ ];

	//-------------------------------------------------------------------------------------

	//-------------------------------------------------------------------------------------
	// copy call params and grab common data and ptrs for SPU path
	mstudioseqdesc_t	   &seqdesc		= ((CStudioHdr *)m_pStudioHdr)->pSeqdesc( sequence );

	pPoseEntry->pEA_seqdesc				= &seqdesc;
	pPoseEntry->iSeq					= sequence;
	pPoseEntry->cycle					= cycle;
	pPoseEntry->weight					= flWeight;
	pPoseEntry->pqStackLevel			= pqStackLevel;

	pPoseEntry->seqdesc_numiklocks		= seqdesc.numiklocks;
	pPoseEntry->seqdesc_numikrules		= seqdesc.numikrules;
	pPoseEntry->seqdesc_flags			= seqdesc.flags;

	virtualmodel_t *pVModel				= m_pStudioHdr->GetVirtualModel();
	const virtualgroup_t *pSeqGroup		= NULL;
	pPoseEntry->pEA_seqgroup_boneMap	= NULL;
	pPoseEntry->pEA_seqgroup_masterBone	= NULL;

	if( pVModel )
	{
		const studiohdr_t *pSeqStudioHdr;				

		pSeqGroup = pVModel->pSeqGroup( sequence );

		if( pSeqGroup )
		{
			pPoseEntry->pEA_seqgroup_boneMap		= (void *)pSeqGroup->boneMap.Base();
			pPoseEntry->pEA_seqgroup_masterBone		= (void *)pSeqGroup->masterBone.Base();
		}

		pSeqStudioHdr = ((CStudioHdr *)m_pStudioHdr)->pSeqStudioHdr( sequence );

		mstudiolinearbone_t *pLinearBones			= pSeqStudioHdr->pLinearBones();
		pPoseEntry->pEA_seq_linearBones				= pLinearBones;

		if( pLinearBones )
		{
			pPoseEntry->pEA_seq_linearbones_pos		= (void *)(((byte *)pLinearBones) + pLinearBones->posindex);
			pPoseEntry->pEA_seq_linearbones_quat	= (void *)(((byte *)pLinearBones) + pLinearBones->quatindex);
		}

		pPoseEntry->pEA_seq_bones_pos				= &pSeqStudioHdr->pBone(0)->pos;

	}
	pPoseEntry->pEA_seqdesc_boneWeight				= (void *)seqdesc.pBoneweight( 0 );

	//-------------------------------------------------------------------------------------


	if( seqdesc.numiklocks )
	{
		AddSequenceLocks_PS3( pPoseEntry, m_pStudioHdr, seqdesc );
	}

	// potentially recurse
	if( CalcPoseSingle( pPoseEntry, m_pStudioHdr, seqdesc, sequence, cycle, m_flPoseParameter, pSPUJob->currentTime ) )
	{
		pPoseEntry->numLocalLayers = AddLocalLayers( pSPUJob, seqdesc, sequence, cycle, 1.0f, pIKContext, pqStackLevel+1 );
	}

	pPoseEntry->numSequenceLayers = AddSequenceLayers( pSPUJob, seqdesc, sequence, cycle, flWeight, pIKContext, pqStackLevel+1 );


	// update bonejob max bones
	for( int lp = 0; lp < MAX_BLENDANIMS; lp++ )
	{
		pSPUJob->maxBones = MAX( pPoseEntry->anims[lp].animstudiohdr_numbones, pSPUJob->maxBones );
	}
}



//-----------------------------------------------------------------------------
// Purpose: calculate a pose for a single sequence
//			adds autolayers, runs local ik rukes
//-----------------------------------------------------------------------------
int CBoneSetup_PS3::AddSequenceLayers(
								   bonejob_SPU *pSPUJob,
								   mstudioseqdesc_t &seqdesc,
								   int sequence, 
								   float cycle,
								   float flWeight,
								   CIKContext *pIKContext,
								   int pqStackLevel
								   )
{
	SNPROF_ANIM("CBoneSetup_PS3::AddSequenceLayers");

	int ret = 0;

	for (int i = 0; i < seqdesc.numautolayers; i++)
	{
		mstudioautolayer_t *pLayer = seqdesc.pAutolayer( i );

		if (pLayer->flags & STUDIO_AL_LOCAL)
			continue;

		float layerCycle = cycle;
		float layerWeight = flWeight;

		if (pLayer->start != pLayer->end)
		{
			float s = 1.0f;
			float index;

			if (!(pLayer->flags & STUDIO_AL_POSE))
			{
				index = cycle;
			}
			else
			{
				int iSequence = m_pStudioHdr->iRelativeSeq( sequence, pLayer->iSequence );
				int iPose = m_pStudioHdr->GetSharedPoseParameter( iSequence, pLayer->iPose );
				if (iPose != -1)
				{
					const mstudioposeparamdesc_t &Pose = ((CStudioHdr *)m_pStudioHdr)->pPoseParameter( iPose );
					index = m_flPoseParameter[ iPose ] * (Pose.end - Pose.start) + Pose.start;
				}
				else
				{
					index = 0;
				}
			}

			if (index < pLayer->start)
				continue;
			if (index >= pLayer->end)
				continue;

			if (index < pLayer->peak && pLayer->start != pLayer->peak)
			{
				s = (index - pLayer->start) / (pLayer->peak - pLayer->start);
			}
			else if (index > pLayer->tail && pLayer->end != pLayer->tail)
			{
				s = (pLayer->end - index) / (pLayer->end - pLayer->tail);
			}

			if (pLayer->flags & STUDIO_AL_SPLINE)
			{
				s = SimpleSpline( s );
			}

			if ((pLayer->flags & STUDIO_AL_XFADE) && (index > pLayer->tail))
			{
				layerWeight = ( s * flWeight ) / ( 1.0f - flWeight + s * flWeight );
			}
			else if (pLayer->flags & STUDIO_AL_NOBLEND)
			{
				layerWeight = s;
			}
			else
			{
				layerWeight = flWeight * s;
			}

			if (!(pLayer->flags & STUDIO_AL_POSE))
			{
				layerCycle = (cycle - pLayer->start) / (pLayer->end - pLayer->start);
			}
		}

		ret++;
		int iSequence = m_pStudioHdr->iRelativeSeq( sequence, pLayer->iSequence );
		AccumulatePose_AddToBoneJob( pSPUJob, iSequence, layerCycle, layerWeight, pIKContext, pqStackLevel );
	}

	return ret;
}


//-----------------------------------------------------------------------------
// Purpose: calculate a pose for a single sequence
//			adds autolayers, runs local ik rukes
//-----------------------------------------------------------------------------
int CBoneSetup_PS3::AddLocalLayers(
								bonejob_SPU *pSPUJob,
								mstudioseqdesc_t &seqdesc,
								int sequence, 
								float cycle,
								float flWeight,
								CIKContext *pIKContext,
								int pqStackLevel
								)
{
	SNPROF_ANIM("CBoneSetup_PS3::AddLocalLayers");

	int ret = 0;

	if (!(seqdesc.flags & STUDIO_LOCAL))
	{
		return ret;
	}

	for (int i = 0; i < seqdesc.numautolayers; i++)
	{
		mstudioautolayer_t *pLayer = seqdesc.pAutolayer( i );

		if (!(pLayer->flags & STUDIO_AL_LOCAL))
			continue;

		float layerCycle = cycle;
		float layerWeight = flWeight;

		if (pLayer->start != pLayer->end)
		{
			float s = 1.0f;

			if (cycle < pLayer->start)
				continue;
			if (cycle >= pLayer->end)
				continue;

			if (cycle < pLayer->peak && pLayer->start != pLayer->peak)
			{
				s = (cycle - pLayer->start) / (pLayer->peak - pLayer->start);
			}
			else if (cycle > pLayer->tail && pLayer->end != pLayer->tail)
			{
				s = (pLayer->end - cycle) / (pLayer->end - pLayer->tail);
			}

			if (pLayer->flags & STUDIO_AL_SPLINE)
			{
				s = SimpleSpline( s );
			}

			if ((pLayer->flags & STUDIO_AL_XFADE) && (cycle > pLayer->tail))
			{
				layerWeight = ( s * flWeight ) / ( 1.0f - flWeight + s * flWeight );
			}
			else if (pLayer->flags & STUDIO_AL_NOBLEND)
			{
				layerWeight = s;
			}
			else
			{
				layerWeight = flWeight * s;
			}

			layerCycle = (cycle - pLayer->start) / (pLayer->end - pLayer->start);
		}

		ret++;
		int iSequence = m_pStudioHdr->iRelativeSeq( sequence, pLayer->iSequence );
		AccumulatePose_AddToBoneJob( pSPUJob, iSequence, layerCycle, layerWeight, pIKContext, pqStackLevel );
	}

	return ret;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IBoneSetup_PS3::IBoneSetup_PS3( const CStudioHdr *pStudioHdr, int boneMask, const float poseParameter[], bonejob_SPU *pBoneJobSPU )
{
	m_pBoneSetup = new CBoneSetup_PS3( pStudioHdr, boneMask, poseParameter, pBoneJobSPU );
}

IBoneSetup_PS3::~IBoneSetup_PS3( void )
{
	if ( m_pBoneSetup )
	{
		delete m_pBoneSetup;
	}
}

void IBoneSetup_PS3::InitPose_PS3( BoneVector pos[], BoneQuaternion q[] )
{
	::InitPose_PS3( m_pBoneSetup->m_pStudioHdr, pos, q, m_pBoneSetup->m_boneMask );
}

void IBoneSetup_PS3::CalcAutoplaySequences_AddPoseCalls( float flRealTime )
{
	m_pBoneSetup->CalcAutoplaySequences_AddPoseCalls( flRealTime );
}

void IBoneSetup_PS3::AccumulatePose_AddToBoneJob( bonejob_SPU* pBonejobSPU, int sequence, float cycle, float flWeight, CIKContext *pIKContext, int pqStackLevel )
{
	m_pBoneSetup->AccumulatePose_AddToBoneJob( pBonejobSPU, sequence, cycle, flWeight, pIKContext, pqStackLevel );
}


CStudioHdr *IBoneSetup_PS3::GetStudioHdr()
{
	return (CStudioHdr *)m_pBoneSetup->m_pStudioHdr;
}

void IBoneSetup_PS3::ResetErrorFlags()
{
	m_pBoneSetup->m_errorFlags = 0;
}

int IBoneSetup_PS3::ErrorFlags()
{
	return m_pBoneSetup->m_errorFlags;
}

bonejob_SPU *IBoneSetup_PS3::GetBoneJobSPU()
{
	return m_pBoneSetup->m_pBoneJobSPU;
}

int IBoneSetup_PS3::RunAccumulatePoseJobs_PPU( bonejob_SPU *pBonejobSPU )
{
	return m_pBoneSetup->RunAccumulatePoseJobs_PPU( pBonejobSPU );
}

int IBoneSetup_PS3::RunAccumulatePoseJobs_SPU( bonejob_SPU *pBoneJobSPU, job_accumpose::JobDescriptor_t *pJobDescriptor )
{
	return m_pBoneSetup->RunAccumulatePoseJobs_SPU( pBoneJobSPU, pJobDescriptor );
}


CBoneSetup_PS3::CBoneSetup_PS3( const CStudioHdr *pStudioHdr, int boneMask, const float poseParameter[], bonejob_SPU *pBoneJobSPU )
{
	m_pStudioHdr		= pStudioHdr;
	m_boneMask			= boneMask;
	m_flPoseParameter	= poseParameter;
	m_pBoneJobSPU		= pBoneJobSPU;

	m_errorFlags		= 0;
}


//-----------------------------------------------------------------------------
// Purpose: run all animations that automatically play and are driven off of poseParameters
// This version adds AccumulatePose calls to the current bonejob
//-----------------------------------------------------------------------------
void CBoneSetup_PS3::CalcAutoplaySequences_AddPoseCalls( float flRealTime )
{
	SNPROF_ANIM( "CBoneSetup_PS3::CalcAutoplaySequences_AddPoseCalls" );

	int			i;


	AddAutoLocks_PS3( m_pBoneJobSPU, m_pStudioHdr );


	unsigned short *pList = NULL;
	int count = m_pStudioHdr->GetAutoplayList( &pList );

	for( i = 0; i < count; i++ )
	{
		int sequenceIndex = pList[i];
		mstudioseqdesc_t &seqdesc = ((CStudioHdr *)m_pStudioHdr)->pSeqdesc( sequenceIndex );

		if( seqdesc.flags & STUDIO_AUTOPLAY )
		{
			float cycle = 0.0f;
			float cps = Studio_CPS( m_pStudioHdr, seqdesc, sequenceIndex, m_flPoseParameter );
			cycle = flRealTime * cps;
			cycle = cycle - (int)cycle;

//			AccumulatePose( pos, q, sequenceIndex, cycle, 1.0, flRealTime, pIKContext );
			// null ik context since dependencies are added but not used
			// still need autoplaylocks added/solved
			AccumulatePose_AddToBoneJob( m_pBoneJobSPU, sequenceIndex, cycle, 1.0f, NULL, 0 );
		}
	}
}


void MsgJobData( bonejob_SPU *pBoneJob )
{
	int lp;

	Msg("**********************************************************\n");
	Msg(" BoneJob Data Start\n");
	Msg("**********************************************************\n");

	Msg("numikAutoplayLocks: %d\n", pBoneJob->numikAutoplayLocks);
	for(lp = 0; lp< pBoneJob->numikAutoplayLocks; lp++)
	{
		Msg("pEA_IKAutoplayLocks[%d] = 0x%x\n", lp, (unsigned int)pBoneJob->pEA_IKAutoplayLocks[lp]);
	}


	Msg("numTotalPoses: %d\n", pBoneJob->numTotalPoses );
	for(lp = 0; lp< pBoneJob->numTotalPoses; lp++)
	{
		accumposeentry_SPU *pPoseEntry = &pBoneJob->accumPoseEntry[lp];

		Msg("PoseEntry[%d]\n",lp);
		Msg("--------------\n");
		Msg("pEA_seqdesc = 0x%x\n", (unsigned int)pPoseEntry->pEA_seqdesc);

		Msg("pEA_seqgroup_boneMap = 0x%x\n", (unsigned int)pPoseEntry->pEA_seqgroup_boneMap);
		Msg("pEA_seqgroup_masterBone = 0x%x\n", (unsigned int)pPoseEntry->pEA_seqgroup_masterBone);
		Msg("pEA_seqdesc_boneWeight = 0x%x\n", (unsigned int)pPoseEntry->pEA_seqdesc_boneWeight);
		Msg("pEA_seqdesc_iklocks = 0x%x\n", (unsigned int)pPoseEntry->pEA_seqdesc_iklocks);
		Msg("pEA_seq_linearBones = 0x%x\n", (unsigned int)pPoseEntry->pEA_seq_linearBones);
		Msg("pEA_seq_linearbones_pos = 0x%x\n", (unsigned int)pPoseEntry->pEA_seq_linearbones_pos);
		Msg("pEA_seq_linearbones_quat = 0x%x\n", (unsigned int)pPoseEntry->pEA_seq_linearbones_quat);

		Msg("pEA_seq_bones_pos = 0x%x\n", (unsigned int)pPoseEntry->pEA_seq_bones_pos);

		switch( pPoseEntry->cpsPath )
		{
		case CPSPATH_FALSE:
			break;
		case CPSPATH_CALCANIM1:
		case CPSPATH_CALCANIM1_SCALE1MINUSS0:
		case CPSPATH_CALCANIM1_SCALES0:
			break;
		case CPSPATH_CALCANIM2_S0:
		case CPSPATH_CALCANIM2_S1:
			break;
		case CPSPATH_CALCANIM3:
		case CPSPATH_CALCANIM4:
			break;
		default:
			break;
		}

	}

	Msg("**********************************************************\n");
	Msg(" BoneJob Data End\n");
	Msg("**********************************************************\n");
}


//--------------------------------------------------------------------------------------------------------------------
// Entry point for running bone jobs, spu jobs pushed from here
// 
//--------------------------------------------------------------------------------------------------------------------
int CBoneSetup_PS3::RunAccumulatePoseJobs_SPU( bonejob_SPU *pBoneJob, job_accumpose::JobDescriptor_t *pJobDescriptor )
{
	SNPROF_ANIM("CBoneSetup_PS3::RunAccumulatePoseJobs_SPU");

	ConVarRef cl_PS3_SPU_bones_debug( "cl_PS3_SPU_bones_debug" );
	ConVarRef cl_PS3_SPU_bones_minbonecount( "cl_PS3_SPU_bones_minbonecount" );



	int				numBones	= pBoneJob->numBones;


	// don't push jobs with less than minbonecount bones to SPU, just perform the job inline on PPU, need to find a sweet spot for minbonecount
	if( numBones < cl_PS3_SPU_bones_minbonecount.GetInt() )
	{
		return RunAccumulatePoseJobs_PPU( pBoneJob );
	}


	BoneVector		*pos		= (BoneVector *)pBoneJob->pEA_pos;
	BoneQuaternion	*q			= (BoneQuaternion *)pBoneJob->pEA_q;


	if( cl_PS3_SPU_bones_debug.GetInt() == 2 )
	{
		memcpy( g_posInit, pos, sizeof(BoneVector) * numBones );
		memcpy( g_qInit, q, sizeof(BoneQuaternion) * numBones );
	}

	// initialise header
	pJobDescriptor->header = g_AccumPoseJobDescriptor.header;

	pJobDescriptor->header.useInOutBuffer	= 1;
	pJobDescriptor->header.sizeStack		= (16*1024)/8;
	pJobDescriptor->header.sizeInOrInOut	= 0;
	pJobDescriptor->header.sizeDmaList		= 0;

	// add dma's

	AddInputDma( pJobDescriptor, sizeof(bonejob_SPU), pBoneJob );
	AddInputDma( pJobDescriptor, sizeof(BoneVector) * pBoneJob->numBones, pBoneJob->pEA_pos );
	AddInputDma( pJobDescriptor, sizeof(BoneQuaternion) * pBoneJob->numBones, pBoneJob->pEA_q );

	pJobDescriptor->header.sizeInOrInOut	+= (sizeof(ikcontextikrule_t) * MAX_IKRULES) + (sizeof(int) * 4);


	int jobPort = g_pBoneJobs->GetNextFreeSPURSPort(); 


	// kick off job
	//Msg("push job port %d, numbones %d\n", jobPort, pBoneJob->numBones );
	CELL_VERIFY( g_pBoneJobs->m_pRoot->m_queuePortAnim[ jobPort ].pushJob( &pJobDescriptor->header, sizeof(*pJobDescriptor), 0, CELL_SPURS_JOBQUEUE_FLAG_SYNC_JOB ) );


	if( cl_PS3_SPU_bones_debug.GetInt() )
	{
		//Msg("_DEBUG_ACCUMPOSE sync job %d\n", jobPort);
		CELL_VERIFY( g_pBoneJobs->m_pRoot->m_queuePortAnim[ jobPort ].sync( 0 ) );

		if( cl_PS3_SPU_bones_debug.GetInt() > 1 )
		{
			// reset pos, q
			memcpy( pos, g_posInit, sizeof(BoneVector) * numBones );
			memcpy( q, g_qInit, sizeof(BoneQuaternion) * numBones );

			RunAccumulatePoseJobs_PPU( pBoneJob );
		}
	}

	return jobPort;
}

//--------------------------------------------------------------------------------------------------------------------
// Entry point for running bone jobs
// This is the PPU version
//
//--------------------------------------------------------------------------------------------------------------------
int CBoneSetup_PS3::RunAccumulatePoseJobs_PPU( bonejob_SPU *pBoneJob )
{
	SNPROF_ANIM("CBoneSetup_PS3::RunAccumulatePoseJobs_PPU");

	InitLSStack();

	BoneVector		*pos		= (BoneVector *)pBoneJob->pEA_pos;
	BoneQuaternion	*q			= (BoneQuaternion *)pBoneJob->pEA_q;

	g_addDep_numIKRules			= 0;

	// pos and q already contain result of initial ::init, which all subsequent calls of ::init take a copy of
	// don't write over until the end when it should contain the final pos, q data

	C_AccumulatePose_SPU cAccumPose( pBoneJob );

	int numBones				= pBoneJob->numBones;

	// backup
	memcpy( g_posInit, pos, sizeof(BoneVector) * numBones );
	memcpy( g_qInit, q, sizeof(BoneQuaternion) * numBones );

	// into pq stack which we work on 
	BoneVector		*pos0 = (BoneVector *)PushLSStack( sizeof(BoneVector) * numBones );
	BoneQuaternion	*q0	 = (BoneQuaternion *)PushLSStack( sizeof(BoneQuaternion) * numBones );

	memcpy( pos0, pos, sizeof(BoneVector) * numBones );
	memcpy( q0, q, sizeof(BoneQuaternion) * numBones );

	cAccumPose.ResetCount();

	cAccumPose.SetIKContext( pBoneJob->pEA_IKContext );

	while( cAccumPose.GetCount() < pBoneJob->numPoses_PreAutoSeq )
	{
		cAccumPose.AccumulatePose( pos0, q0 );
	}

	CIKContext_PS3 auto_ik;
	auto_ik.Init( pBoneJob, pBoneJob->autoikAngles, pBoneJob->autoikOrigin, pBoneJob->currentTime, pBoneJob->autoikFramecount, pBoneJob->boneMask );

	auto_ik.AddAutoplayLocks( pBoneJob, pos0, q0 );

	while( cAccumPose.GetCount() < pBoneJob->numTotalPoses )
	{
		cAccumPose.AccumulatePose( pos0, q0 );
	}

	auto_ik.SolveAutoplayLocks( pBoneJob, pos0, q0 );

	// write results to pos, q
	memcpy( pos, pos0, sizeof( BoneVector ) * numBones );
	memcpy( q, q0, sizeof( BoneQuaternion ) * numBones );

	// write AddDependencies() potential IK rules
	if( g_addDep_numIKRules )
	{
		memcpy( pBoneJob->pEA_addDep_numIKRules, &g_addDep_numIKRules, sizeof(int) );
		memcpy( pBoneJob->pEA_addDep_IKRules, g_addDep_IKRules, g_addDep_numIKRules * sizeof(ikcontextikrule_t_PS3) );
	}

	PopLSStack( sizeof(BoneQuaternion) * numBones );
	PopLSStack( sizeof(BoneVector) * numBones );

	TermLSStack();

	return VJobsRoot::MAXPORTS_ANIM;	// => can only be a ppu job
}




#endif // #if !defined(__SPU__)

//--------------------------------------------------------------------------------------------------------------------
//
// Below this point needs to go to SPU, above this point PPU only (prep pass for SPU)
//
//--------------------------------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: turn a 2x2 blend into a 3 way triangle blend
// Returns: returns the animation indices and barycentric coordinates of a triangle
//			the triangle is a right triangle, and the diagonal is between elements [0] and [2]
//-----------------------------------------------------------------------------
void Calc3WayBlendIndices_SPU( accumposeentry_SPU *pPoseEntry, int i0, int i1, float s0, float s1, int *pAnimIndices, float *pWeight )
{
	// Figure out which bi-section direction we are using to make triangles.
	bool bEven = ( ( ( i0 + i1 ) & 0x1 ) == 0 );

	// diagonal is between elements 1 & 3
	// TL to BR
	if ( bEven )
	{
		if ( s0 > s1 )
		{
			// B
			pAnimIndices[0] = pPoseEntry->animIndices[0];//0;
			pAnimIndices[1] = pPoseEntry->animIndices[1];//1;
			pAnimIndices[2] = pPoseEntry->animIndices[3];//3;
			pWeight[0] = (1.0f - s0);
			pWeight[1] = s0 - s1;
		}
		else
		{
			// C
			pAnimIndices[0] = pPoseEntry->animIndices[3];//3;
			pAnimIndices[1] = pPoseEntry->animIndices[2];//2;
			pAnimIndices[2] = pPoseEntry->animIndices[0];//0;
			pWeight[0] = s0;
			pWeight[1] = s1 - s0;
		}
	}
	// BL to TR
	else
	{
		float flTotal = s0 + s1;

		if( flTotal > 1.0f )
		{
			// D
			pAnimIndices[0] = pPoseEntry->animIndices[1];//1;
			pAnimIndices[1] = pPoseEntry->animIndices[3];//3;
			pAnimIndices[2] = pPoseEntry->animIndices[2];//2;
			pWeight[0] = (1.0f - s1);
			pWeight[1] = s0 - 1.0f + s1;
		}
		else
		{
			// A
			pAnimIndices[0] = pPoseEntry->animIndices[2];//2;
			pAnimIndices[1] = pPoseEntry->animIndices[0];//0;
			pAnimIndices[2] = pPoseEntry->animIndices[1];//1;
			pWeight[0] = s1;
			pWeight[1] = 1.0f - s0 - s1;
		}
	}

	// clamp the diagonal
	if( pWeight[1] < 0.001f )
		pWeight[1] = 0.0f;

	pWeight[2] = 1.0f - pWeight[0] - pWeight[1];

	Assert( pWeight[0] >= 0.0f && pWeight[0] <= 1.0f );
	Assert( pWeight[1] >= 0.0f && pWeight[1] <= 1.0f );
	Assert( pWeight[2] >= 0.0f && pWeight[2] <= 1.0f );

	// fix up pose entry weights so that IK rules are blended correctly (assume they all come in as zero for the 3 way blend case)
	pPoseEntry->anims[ pAnimIndices[0] ].seqdesc_weight = pWeight[0];
	pPoseEntry->anims[ pAnimIndices[1] ].seqdesc_weight = pWeight[1];
	pPoseEntry->anims[ pAnimIndices[2] ].seqdesc_weight = pWeight[2];
}




void C_AccumulatePose_SPU::AddLocalLayers( accumposeentry_SPU *pAccPoseEntry, BoneVector pos[], BoneQuaternion q[] )
{
	for( int lp = 0; lp < pAccPoseEntry->numLocalLayers; lp++ )
	{
		AccumulatePose( pos, q );
	}
}

void C_AccumulatePose_SPU::AddSequenceLayers( accumposeentry_SPU *pAccPoseEntry, BoneVector pos[], BoneQuaternion q[] )
{
	for( int lp = 0; lp < pAccPoseEntry->numSequenceLayers; lp++ )
	{
		AccumulatePose( pos, q );
	}
}

//-----------------------------------------------------------------------------
// Purpose: calculate a pose for a single sequence
// SPU pass
//-----------------------------------------------------------------------------
bool C_AccumulatePose_SPU::CalcPoseSingle(
	const bonejob_SPU *pBonejob,
	accumposeentry_SPU *pPoseEntry,
	BoneVector *pos, 
	BoneQuaternion *q, 
	int *boneMap,
	float *boneWeight, 
	int sequence, 
	const float poseParameter[],
	int boneMask,
	float flTime
	)
{
	SNPROF_ANIM("C_AccumulatePose_SPU::CalcPoseSingle");

	bool bResult = true;
	
	BoneVector		*pos2	= (BoneVector *)PushLSStack( sizeof(BoneVector) * pBonejob->numBones );
	BoneQuaternion	*q2		= (BoneQuaternion *)PushLSStack( sizeof(BoneQuaternion) * pBonejob->numBones );
	BoneVector		*pos3	= (BoneVector *)PushLSStack( sizeof(BoneVector) * pBonejob->numBones );
	BoneQuaternion	*q3		= (BoneQuaternion *)PushLSStack( sizeof(BoneQuaternion) * pBonejob->numBones );

	
	float	s0 = pPoseEntry->s0;
	float	s1 = pPoseEntry->s1;
	int		i0 = pPoseEntry->i0;
	int		i1 = pPoseEntry->i1;

	float	cycle = pPoseEntry->cyclePoseSingle;

	switch( pPoseEntry->cpsPath )
	{
	case CPSPATH_FALSE:
		bResult = false;
		break;
	case CPSPATH_CALCANIM1:
		CalcAnimation_PS3( pBonejob, pPoseEntry, pos,  q, boneMap, boneWeight, pPoseEntry->animIndices[0], cycle, boneMask );
		break;
	case CPSPATH_CALCANIM1_SCALE1MINUSS0:
		CalcAnimation_PS3( pBonejob, pPoseEntry, pos,  q, boneMap, boneWeight,   pPoseEntry->animIndices[0], cycle, boneMask );
		ScaleBones_PS3( pBonejob, pPoseEntry, q, pos, boneMap, boneWeight, 1.0f - s0, boneMask );
		break;
	case CPSPATH_CALCANIM1_SCALES0:
		CalcAnimation_PS3( pBonejob, pPoseEntry, pos,  q, boneMap, boneWeight,  pPoseEntry->animIndices[0], cycle, boneMask );
		ScaleBones_PS3( pBonejob, pPoseEntry, q, pos, boneMap, boneWeight, s0, boneMask );
		break;
	case CPSPATH_CALCANIM2_S0:
		CalcAnimation_PS3( pBonejob, pPoseEntry, pos,  q,  boneMap, boneWeight,  pPoseEntry->animIndices[0], cycle, boneMask );
		CalcAnimation_PS3( pBonejob, pPoseEntry, pos2, q2, boneMap, boneWeight,  pPoseEntry->animIndices[1], cycle, boneMask );
		BlendBones_PS3( pBonejob, pPoseEntry, q, pos, boneMap, boneWeight,  q2, pos2, s0, boneMask );
		break;
	case CPSPATH_CALCANIM2_S1:
		CalcAnimation_PS3( pBonejob, pPoseEntry, pos,  q,  boneMap, boneWeight,  pPoseEntry->animIndices[0], cycle, boneMask );
		CalcAnimation_PS3( pBonejob, pPoseEntry, pos2, q2, boneMap, boneWeight,  pPoseEntry->animIndices[1], cycle, boneMask );
		BlendBones_PS3( pBonejob, pPoseEntry, q, pos, boneMap, boneWeight,  q2, pos2, s1, boneMask );
		break;
	case CPSPATH_CALCANIM3:
		int		iAnimIndices[3];
		float	weight[3];

		Calc3WayBlendIndices_SPU( pPoseEntry, i0, i1, s0, s1, iAnimIndices, weight );

		if( weight[1] < 0.001f )
		{
			// on diagonal
			CalcAnimation_PS3( pBonejob, pPoseEntry, pos,  q,  boneMap, boneWeight, iAnimIndices[0], cycle, boneMask );
			CalcAnimation_PS3( pBonejob, pPoseEntry, pos2, q2, boneMap, boneWeight, iAnimIndices[2], cycle, boneMask );
			BlendBones_PS3( pBonejob, pPoseEntry, q, pos, boneMap, boneWeight,  q2, pos2, weight[2] / (weight[0] + weight[2]), boneMask );
		}
		else
		{
			CalcAnimation_PS3( pBonejob, pPoseEntry, pos,  q,  boneMap, boneWeight, iAnimIndices[0], cycle, boneMask );
			CalcAnimation_PS3( pBonejob, pPoseEntry, pos2, q2, boneMap, boneWeight, iAnimIndices[1], cycle, boneMask );
			BlendBones_PS3( pBonejob, pPoseEntry, q, pos, boneMap, boneWeight,  q2, pos2, weight[1] / (weight[0] + weight[1]), boneMask );

			CalcAnimation_PS3( pBonejob, pPoseEntry, pos3, q3, boneMap, boneWeight, iAnimIndices[2], cycle, boneMask );
			BlendBones_PS3( pBonejob, pPoseEntry, q, pos, boneMap, boneWeight,  q3, pos3, weight[2], boneMask );
		}

		break;
	case CPSPATH_CALCANIM4:
		CalcAnimation_PS3( pBonejob, pPoseEntry, pos,  q,  boneMap, boneWeight,  pPoseEntry->animIndices[0], cycle, boneMask );
		CalcAnimation_PS3( pBonejob, pPoseEntry, pos2, q2, boneMap, boneWeight,  pPoseEntry->animIndices[1], cycle, boneMask );
		BlendBones_PS3( pBonejob, pPoseEntry, q, pos, boneMap, boneWeight,  q2, pos2, s0, boneMask );

		CalcAnimation_PS3( pBonejob, pPoseEntry, pos2, q2, boneMap, boneWeight,  pPoseEntry->animIndices[2], cycle, boneMask );
		CalcAnimation_PS3( pBonejob, pPoseEntry, pos3, q3, boneMap, boneWeight,  pPoseEntry->animIndices[3], cycle, boneMask );
		BlendBones_PS3( pBonejob, pPoseEntry, q2, pos2, boneMap, boneWeight,  q3, pos3, s0, boneMask );

		BlendBones_PS3( pBonejob, pPoseEntry, q, pos, boneMap, boneWeight,  q2, pos2, s1, boneMask );
		break;
	default:
		bResult = false;
		break;
	}

	PopLSStack( sizeof(BoneQuaternion) * pBonejob->numBones );
	PopLSStack( sizeof(BoneVector) * pBonejob->numBones );
	PopLSStack( sizeof(BoneQuaternion) * pBonejob->numBones );
	PopLSStack( sizeof(BoneVector) * pBonejob->numBones );

	return bResult;
}

//-----------------------------------------------------------------------------
// SPU/bonejob version of AccumulatePose
// Assumes bonejob calls to this happen in the correct order (recursion has happened during the 1st/prep pass on PPU)
// individual AccumulatePose call data is stored in bonejob->accumPoseEntry
//-----------------------------------------------------------------------------
void C_AccumulatePose_SPU::AccumulatePose( BoneVector pos[], BoneQuaternion q[] )
{
	SNPROF_ANIM("C_AccumulatePose_SPU::AccumulatePose");

	accumposeentry_SPU *pPoseEntry	= &m_pBoneJob->accumPoseEntry[ m_iCount++ ];

	int		boneMask				= m_pBoneJob->boneMask;
	int		numBones				= m_pBoneJob->numBones;
	float	time					= m_pBoneJob->currentTime;
	float	*pPoseParam				= m_pBoneJob->poseparam;

	float	weight					= pPoseEntry->weight;
	int		sequence				= pPoseEntry->iSeq;

	void	*pIKContext				= GetIKContext();


	CIKContext_PS3 seq_ik;
	if( pPoseEntry->seqdesc_numiklocks )
	{
//IKOFF
		seq_ik.Init( m_pBoneJob, vec3_angle, vec3_origin, 0.0f, 0, boneMask );  
//IKOFF
		seq_ik.AddSequenceLocks( m_pBoneJob, pPoseEntry, pos, q );
	}

	{
		BoneVector		*pos2		 = (BoneVector *)PushLSStack( sizeof(BoneVector) * m_pBoneJob->maxBones ); 
		BoneQuaternion	*q2			 = (BoneQuaternion *)PushLSStack( sizeof(BoneQuaternion) * m_pBoneJob->maxBones );
		int				*pBoneMap	 = (int *)PushLSStack( sizeof(int) * m_pBoneJob->maxBones );
		float			*pBoneWeight = (float *)PushLSStack( sizeof(float) * m_pBoneJob->maxBones );


		// used in calcposesingle (calcanim/calcvirtualanim) & SlerpBones
		// needs to be stack based, as AddLocalLayers may recurse AccumulatePose...
		GetBoneMapBoneWeight_SPU( m_pBoneJob, pPoseEntry, pBoneMap, pBoneWeight );

		if( pPoseEntry->seqdesc_flags & STUDIO_LOCAL )
		{
			// ::init
			memcpy( pos2, g_posInit, sizeof( BoneVector ) * numBones );
			memcpy( q2, g_qInit, sizeof( BoneQuaternion ) * numBones );
		}

		if( CalcPoseSingle( m_pBoneJob, pPoseEntry, pos2, q2, pBoneMap, pBoneWeight, sequence, pPoseParam, boneMask, time ) )
		{
			AddLocalLayers( pPoseEntry, pos2, q2 );

			SlerpBones_SPU( m_pBoneJob, pPoseEntry, q, pos, q2, pos2, pBoneMap, pBoneWeight, weight, boneMask );
		}

		PopLSStack( sizeof(float) * m_pBoneJob->maxBones );
		PopLSStack( sizeof(int) * m_pBoneJob->maxBones );
		PopLSStack( sizeof(BoneQuaternion) * m_pBoneJob->maxBones );
		PopLSStack( sizeof(BoneVector) * m_pBoneJob->maxBones ); 
	}

 	if( pIKContext )
 	{
//IKOFF
		AddDependencies_SPU( m_pBoneJob, pPoseEntry, weight );
 	}

	AddSequenceLayers( pPoseEntry, pos, q );

	if( pPoseEntry->seqdesc_numiklocks )
	{
//IKOFF
		seq_ik.SolveSequenceLocks( m_pBoneJob, pPoseEntry, pos, q );
	}
}



