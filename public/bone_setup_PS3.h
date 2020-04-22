//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BONE_SETUP_PS3_H
#define BONE_SETUP_PS3_H

#ifndef __SPU__
#include "studio.h"
#endif

#include "studio_PS3.h"

#ifndef __SPU__
#include "bone_setup.h"
#include "vjobs/root.h"
#include "vjobs/accumpose_shared.h"
#include <vjobs_interface.h>
#include <ps3/vjobutils.h>
#include <ps3/vjobutils_shared.h>
#endif // __SPU__


#if defined( _PS3 )


#if defined(__SPU__)
#include "ps3/spu_job_shared.h"

#define SNPROF(name)		((void)0)
#define SNPROF_ANIM(name)	((void)0)
#endif


//#define _DEBUG_SPUvPPU_ANIMATION 1
#define ANIM_EPS	0.3f


//-----------------------------------------------------------------------------
// Purpose: SPU version of data collected and required by AccumulatePose 
// this data gets sent to SPU
//-----------------------------------------------------------------------------

#define MAX_ACCUMPOSECALLS	(20) //(MAX_OVERLAYS)
#define MAX_LAYERS_SPU		MAX_ACCUMPOSECALLS
#define MAX_PQSTACKLEVEL	4
#define MAX_IKCHAINELEMENTS	32
#define MAX_IKCHAINS		16
#define MAX_IKLOCKS			16
#define MAX_IKRULES			48
#define MAX_IKTARGETS		12
#define MAX_BLENDANIMS		4


class CBoneBitList_PS3
{
public:
	CBoneBitList_PS3() 
	{
		memset( &m_markedBones, 0, MAXSTUDIOBONES_PS3 );
	}

	inline void MarkBone( int iBone )
	{
		m_markedBones[ iBone ] = 1;
	}
	inline bool IsBoneMarked( int iBone )
	{
		return m_markedBones[ iBone ];
	}
	inline void ResetMarkedBones( int numbones )
	{
		memset( &m_markedBones, 0, numbones );
	}

	byte m_markedBones[ MAXSTUDIOBONES_PS3 ];
};


struct animData_SPU
{
	void					*pEA_animdesc;					// mstudioanimdesc_t *

	int						animstudiohdr_numbones;		
	int						seqdesc_anim;

	float					flStall;
	float					seqdesc_weight;

	int						animdesc_iLocalFrame;

	void					*pEA_animdesc_panim;			// mstudio_rle_anim_t *
	void					*pEA_animdesc_pFrameanim;		// mstudio_frame_anim_t_PS3 *
	void					*pEA_animdesc_ikrule;			// mstudioikrule_t_PS3 *
	void					*pEA_animdesc_ikrulezeroframe;	// zero frame rule of above
	void					*pEA_animgroup_masterbone;		// int *

	void					*pEA_anim_bones_pos;			// Vector *, start of Vector pos, Quaternion quat, RadianEuler rot, Vector posscale, Vector rotscale, matrix3x4_t poseToBone, Quaternion qAlignment, int flags
	void					*pEA_anim_bones_flags;			// int *, start of flags
	void					*pEA_anim_linearBones;			// mstudiolinearbone_t_PS3 *
};

struct ALIGN128 accumposeentry_SPU
{
	void					*pEA_seqdesc;					// mstudioseqdesc_t_PS3 *

	void 					*pEA_seqgroup_boneMap;			// const int *
	void 					*pEA_seqgroup_masterBone;		// const int *
	void					*pEA_seqdesc_boneWeight;		// float *
	void					*pEA_seqdesc_iklocks;			// mstudioiklock_t *

	// from seqstudiohdr
	void					*pEA_seq_linearBones;			// mstudiolinearbone_t_PS3 *
	void					*pEA_seq_linearbones_pos;		// Vector *
	void					*pEA_seq_linearbones_quat;		// Quaternion *
	void					*pEA_seq_bones_pos;				// mstudiobone_t_PS3 * offset to .pos

	int						seqdesc_flags;

	int						cpsPath;						// CalcPoseSingle path
	animData_SPU			anims[ MAX_BLENDANIMS ];
	int8					animIndices[ MAX_BLENDANIMS ];	// index into anims[]

	int						pqStackLevel;

	int						iSeq; 
	float					cycle;
	float					weight;

	int						i0;
	int						i1;

	int						seqdesc_numiklocks;
	int						seqdesc_numikrules;

	float					s0;
	float					s1;
	float					cyclePoseSingle;

	int						numLocalLayers;
	int						numSequenceLayers;

#if !defined(__SPU__)
	bool SetAnimData( const CStudioHdr *pStudioHdr, mstudioseqdesc_t &seqdesc, int x, int y, int animIndex );
#endif

} ALIGN128_POST;

struct ALIGN128 bonejob_SPU
{
	accumposeentry_SPU		accumPoseEntry[ MAX_LAYERS_SPU ];

	int						numTotalPoses;
	int						numPoses_PreAutoSeq;
	int						numPoses_AutoSeq;

	// same for all calls
	int						boneFlags[ MAXSTUDIOBONES_PS3 ];	// 1k
	int						boneParent[ MAXSTUDIOBONES_PS3 ];	// 1k
	float					poseparam[ MAXSTUDIOPOSEPARAM_PS3 ];	// 96B

	void					*pEA_IKAutoplayLocks[ MAX_IKLOCKS ];	// mstudioiklock_t

	float					currentTime;

	int						numBones;
	int						maxBones;							// takes into account virtual bonemapping, used for SPU mem worst-case allocations
	int						numikchains;
	int						boneMask;
	int						studiobone_posoffset;				// mstudiobone_t offset in bytes to pos field

	BoneVector				autoikOrigin;
	QAngle					autoikAngles;
	int						autoikFramecount;
	int						numikAutoplayLocks;

	void					*pEA_hdr;							// CStudioHdr*
	void					*pEA_IKContext;						// CIKContext *
	void					*pEA_studiohdr_ikchains;			// mstudioikchain_t *
	void					*pEA_studiohdr_ikautoplaylocks;		// mstudioiklock_t *
	void					*pEA_studiohdr_vmodel;				// virtualmodel_t *
	void					*pEA_studiohdr_bones;				// mstudiobone_t_PS3 *
	void					*pEA_studiohdr_bones_pos;			// Vector *, start of Vector pos, Quaternion quat, RadianEuler rot, Vector posscale, Vector rotscale, matrix3x4_t poseToBone, Quaternion qAlignment, int flags
	void					*pEA_studiohdr_linearBones;			// mstudiolinearbone_t_PS3 *
	void					*pEA_linearbones_posscale;			// Vector *
	void					*pEA_linearbones_pos;				// Vector *
	void					*pEA_linearbones_rot;				// RadianEuler *
	void					*pEA_linearbones_rotscale;			// Vector *
	void					*pEA_linearbones_flags;				// int *
	void					*pEA_linearbones_quat;				// Quaternion *
	void					*pEA_linearbones_qalignment;		// Quaternion *

	// dst data ea ptrs
	void					*pEA_pos;							// BoneVector *
	void					*pEA_q;								// BoneQuaternion *
	void					*pEA_addDep_IKRules;				// ikcontextikrule_t *
	void					*pEA_addDep_numIKRules;				// int *
	void					*pEA_flags;							// int *

	int						debugJob;							// for dynamic switching of DebuggerBreak

} ALIGN128_POST;

#if !defined(__SPU__)
struct bonejob_PPU
{

public:

	matrix3x4_t			parentTransform;

	float				cycle;
	int					maxBones;
	int					boneMask;
	int					bonesMaskNeedsRecalc;
	int					oldReadableBones;

	void				*pBaseAnimating;	// C_BaseAnimating*
	CStudioHdr			*pStudioHdr;
	matrix3x4a_t		*pBoneToWorldOut;

};


struct ALIGN128 PS3BoneJobData
{
public:

	job_accumpose::JobDescriptor_t	jobDescriptor			ALIGN128;

	// dst. SPU in/out - could be init'd entirely from SPU, in which case it's out only
	BoneVector				pos[ MAXSTUDIOBONES_PS3 ]		ALIGN16;		// 2k
	BoneQuaternion			q[ MAXSTUDIOBONES_PS3 ]			ALIGN16;		// 2k	

	// dst, SPU out only
	ikcontextikrule_t		addDep_IKRules[ MAX_IKRULES ]	ALIGN16;		// 4k
	int						addDep_numIKRules				ALIGN16;		// [4] to pad to 16B
	int						pad[3];

	// src, SPU in only, going to SPU at start of job
	bonejob_SPU				bonejobSPU;

	// src, PPU only, work data, and data required if job needs to be executed from scratch on PPU if SPU fails
	bonejob_PPU				bonejobPPU;

} ALIGN128_POST;

class CBoneJobs : public VJobInstance
{
public:
	CBoneJobs() 
	{
	}

	~CBoneJobs() 
	{
		Shutdown();
	}

	void	Init( void );
	void	Shutdown( void );

	void	OnVjobsInit( void );		// gets called after m_pRoot was created and assigned
	void	OnVjobsShutdown( void );	// gets called before m_pRoot is about to be destructed and NULL'ed

	void	StartFrame( int maxBoneJobs );
	void	EndFrame( void );

	void	ResetBoneJobs( void );
	int		AddBoneJob( void );
	int		GetNumBoneJobs( void );
	int		GetNextFreeSPURSPort( void );

	PS3BoneJobData *GetJobData( int job );

private:

	CUtlVector<PS3BoneJobData>	m_boneJobData;
	int							m_boneJobCount;
	int							m_boneJobNextSPURSPort;
	bool						m_bEnabled;
};

extern CBoneJobs* g_pBoneJobs;
#endif  // #if !defined SPU



//-----------------------------------------------------------------------------
//
// Main spu job class
//
//-----------------------------------------------------------------------------

class C_AccumulatePose_SPU
{
public:
	C_AccumulatePose_SPU( bonejob_SPU *pBoneJob ) { m_pBoneJob = pBoneJob; };

	inline void	ResetCount() { m_iCount = 0; };
	inline int GetCount() { return m_iCount; };

	void AccumulatePose( BoneVector pos[], BoneQuaternion q[] );
	void AddLocalLayers( accumposeentry_SPU *pAccPoseEntry, BoneVector pos[], BoneQuaternion q[] );
	void AddSequenceLayers( accumposeentry_SPU *pAccPoseEntry, BoneVector pos[], BoneQuaternion q[] );

	void SetIKContext( void *pIKContext ) { m_pIKContext = pIKContext; };
	void *GetIKContext( void ) { return m_pIKContext; };

private:

	bool CalcPoseSingle(
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
		);

	void		*m_pIKContext;		// CIKContext *
	bonejob_SPU	*m_pBoneJob;

	// next index into bonejob accumposeentry to execute
	int			m_iCount;
};





//-----------------------------------------------------------------------------
// Purpose: blends together all the bones from two p:q lists
//
// p1 = p1 * (1 - s) + p2 * s
// q1 = q1 * (1 - s) + q2 * s
//-----------------------------------------------------------------------------
void SlerpBones_SPU( 
					bonejob_SPU* pSPUJob,
					accumposeentry_SPU *pPoseEntry,
					BoneQuaternion *q1, 
					BoneVector *pos1, 
					const BoneQuaternion *q2, 
					const BoneVector *pos2, 
					const int *boneMap,
					const float *boneWeight,
					float s,
					int boneMask );

#if !defined(__SPU__)
class CBoneSetup_PS3;
class IBoneSetup_PS3
{
public:
	IBoneSetup_PS3( const CStudioHdr *pStudioHdr, int boneMask, const float poseParameter[], bonejob_SPU *pBoneJobSPU );
	~IBoneSetup_PS3( void );

	void InitPose_PS3( BoneVector pos[], BoneQuaternion q[] );
	void CalcAutoplaySequences_AddPoseCalls( float flRealTime );
	
	void AccumulatePose_AddToBoneJob( bonejob_SPU* pBonejobSPU, int sequence, float cycle, float flWeight, CIKContext *pIKContext, int pqStackLevel );
	int RunAccumulatePoseJobs_PPU( bonejob_SPU *pBoneJobSPU );
	int RunAccumulatePoseJobs_SPU( bonejob_SPU *pBoneJobSPU, job_accumpose::JobDescriptor_t *pJobDescriptor );

	CStudioHdr  *GetStudioHdr();

	void		ResetErrorFlags();
	int			ErrorFlags();

	bonejob_SPU *GetBoneJobSPU();

private:
	CBoneSetup_PS3 *m_pBoneSetup;
};
#endif

// Given two samples of a bone separated in time by dt, 
// compute the velocity and angular velocity of that bone
//void CalcBoneDerivatives( Vector &velocity, AngularImpulse &angVel, const matrix3x4_t &prev, const matrix3x4_t &current, float dt );
// Give a derivative of a bone, compute the velocity & angular velocity of that bone
//void CalcBoneVelocityFromDerivative( const QAngle &vecAngles, Vector &velocity, AngularImpulse &angVel, const matrix3x4_t &current );

// This function sets up the local transform for a single frame of animation. It doesn't handle
// pose parameters or interpolation between frames.
// void SetupSingleBoneMatrix_PS3( 
// 	CStudioHdr_PS3 *pOwnerHdr, 
// 	int nSequence, 
// 	int iFrame,
// 	int iBone, 
// 	matrix3x4_t &mBoneLocal );


// Purpose: build boneToWorld transforms for a specific bone
void BuildBoneChain_PS3(
						const int *pBoneParent,
						const matrix3x4a_t &rootxform,
						const BoneVector pos[], 
						const BoneQuaternion q[], 
						int	iBone,
						matrix3x4a_t *pBoneToWorld
						);

void BuildBoneChain_PS3(
						const int *pBoneParent,
						const matrix3x4a_t &rootxform,
						const BoneVector pos[], 
						const BoneQuaternion q[], 
						int	iBone,
						matrix3x4a_t *pBoneToWorld,
						CBoneBitList_PS3 &boneComputed );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

// ik info
class CIKTarget_PS3
{
public:
// 	void SetOwner( int entindex, const Vector &pos, const QAngle &angles );
// 	void ClearOwner( void );
// 	int GetOwner( void );
// 	void UpdateOwner( int entindex, const Vector &pos, const QAngle &angles );
// 	void SetPos( const Vector &pos );
// 	void SetAngles( const QAngle &angles );
// 	void SetQuaternion( const Quaternion &q );
// 	void SetNormal( const Vector &normal );
// 	void SetPosWithNormalOffset( const Vector &pos, const Vector &normal );
// 	void SetOnWorld( bool bOnWorld = true );

// 	bool IsActive( void );
// 	void IKFailed( void );
	int						chain;
	int						type;
//	void MoveReferenceFrame( Vector &deltaPos, QAngle &deltaAngles );
	// accumulated offset from ideal footplant location
public:
	struct x2 {
		char				*pAttachmentName;
		Vector				pos;
		Quaternion			q;
	} offset;
//private:
	struct x3 {
		Vector				pos;
		Quaternion			q;
	} ideal;
public:
	struct x4 {
		float				latched;
		float				release;
		float				height;
		float				floor;
		float				radius;
		float				flTime;
		float				flWeight;
		Vector				pos;
		Quaternion			q;
		bool				onWorld;
	} est; // estimate contact position
	struct x5 {
		float				hipToFoot;	// distance from hip
		float				hipToKnee;	// distance from hip to knee
		float				kneeToFoot;	// distance from knee to foot
		Vector				hip;		// location of hip
		Vector				closest;	// closest valid location from hip to foot that the foot can move to
		Vector				knee;		// pre-ik location of knee
		Vector				farthest;	// farthest valid location from hip to foot that the foot can move to
		Vector				lowest;		// lowest position directly below hip that the foot can drop to
	} trace;
//private:
	// internally latched footset, position
	struct x1 {
		// matrix3x4a_t		worldTarget;
		bool				bNeedsLatch;
		bool				bHasLatch;
		float				influence;
		int					iFramecounter;
		int					owner;
		Vector				absOrigin;
		QAngle				absAngles;
		Vector				pos;
		Quaternion			q;
		Vector				deltaPos;	// accumulated error
		Quaternion			deltaQ;
		Vector				debouncePos;
		Quaternion			debounceQ;
	} latched;
	struct x6 {
		float				flTime; // time last error was detected
		float				flErrorTime;
		float				ramp;
		bool				bInError;
	} error;

//	friend class CIKContext_PS3;
};


struct ikchainresult_t_PS3
{
	// accumulated offset from ideal footplant location
	int					target;
	Vector				pos;
	Quaternion			q;
	float				flWeight;
};



struct ikcontextikrule_t_PS3
{
	int					index;

	int					type;
	int					chain;

	int					bone;

	int					slot;			// iktarget slot.  Usually same as chain.

	float				height;
	float				radius;
	float				floor;
	Vector				pos;
	float				pad1;
	Quaternion			q;

	float				start;			// beginning of influence
	float				peak;			// start of full influence
	float				tail;			// end of full influence
	float				end;			// end of all influence

	float				top;
	float				drop;

	float				commit;			// frame footstep target should be committed
	float				release;		// frame ankle should end rotation from latched orientation

	float				flWeight;		// processed version of start-end cycle
	float				flRuleWeight;	// blending weight
	float				latched;		// does the IK rule use a latched value?
	char				*szLabel;

	Vector				kneeDir;
	float				pad2;
	Vector				kneePos;
	float				pad3;

	ikcontextikrule_t_PS3() {}

private:
	// No copy constructors allowed
	ikcontextikrule_t_PS3(const ikcontextikrule_t_PS3& vOther);
};

void Studio_AlignIKMatrix_PS3( matrix3x4a_t &mMat, const Vector &vAlignTo );

bool Studio_SolveIK_PS3( int8 iThigh, int8 iKnee, int8 iFoot, Vector &targetFoot, matrix3x4a_t* pBoneToWorld );
bool Studio_SolveIK_PS3( int8 iThigh, int8 iKnee, int8 iFoot, Vector &targetFoot, Vector &targetKneePos, Vector &targetKneeDir, matrix3x4a_t* pBoneToWorld );


class CIKContext_PS3
{
public:
	CIKContext_PS3( );
//	void Init( const CStudioHdr *pStudioHdr, const QAngle &angles, const BoneVector &pos, float flTime, int iFramecounter, int boneMask );
	void Init( bonejob_SPU *pBonejobSPU, const QAngle &angles, const Vector &pos, float flTime, int iFramecounter, int boneMask );

	//	void AddDependencies(  mstudioseqdesc_t_PS3 &seqdesc, int iSequence, float flCycle, const float poseParameters[], float flWeight = 1.0f );

	void ClearTargets( void );
// 	void UpdateTargets( Vector pos[], Quaternion q[], matrix3x4a_t boneToWorld[], CBoneBitList &boneComputed );
// 	void AutoIKRelease( void );

	void AddAutoplayLocks( bonejob_SPU *pBonejob, BoneVector pos[], BoneQuaternion q[] );
	void SolveAutoplayLocks( bonejob_SPU *pBonejob, BoneVector pos[], BoneQuaternion q[] );

//	void AddSequenceLocks( mstudioseqdesc_t &SeqDesc, BoneVector pos[], BoneQuaternion q[] );
	void AddSequenceLocks( bonejob_SPU *pBonejob, accumposeentry_SPU* pPoseEntry, BoneVector pos[], BoneQuaternion q[] );
	void SolveSequenceLocks( bonejob_SPU *pBonejob, accumposeentry_SPU* pPoseEntry, BoneVector pos[], BoneQuaternion q[] );
	
//	void AddAllLocks( BoneVector pos[], BoneQuaternion q[] );
//	void SolveAllLocks( BoneVector pos[], BoneQuaternion q[] );

	void SolveLock( bonejob_SPU *pBonejob, const mstudioiklock_t_PS3 *plock, int i, BoneVector pos[], BoneQuaternion q[], matrix3x4a_t boneToWorld[], CBoneBitList_PS3 &boneComputed );

// 	CUtlVectorFixed< CIKTarget, 12 >	m_target;
	CIKTarget_PS3	m_target[ 12 ];
	int				m_targetCount;
	int				m_numTarget;

private:

// 	CStudioHdr const *m_pStudioHdr;
// 
// 	bool Estimate( int iSequence, float flCycle, int iTarget, const float poseParameter[], float flWeight = 1.0f ); 
 	void BuildBoneChain( const bonejob_SPU *pBonejob, const BoneVector pos[], const BoneQuaternion q[], int iBone, matrix3x4a_t *pBoneToWorld, CBoneBitList_PS3 &boneComputed );
// 
// 	// virtual IK rules, filtered and combined from each sequence
// 	CUtlVector< CUtlVector< ikcontextikrule_t_PS3 > > m_ikChainRule;
// 	CUtlVector< ikcontextikrule_t_PS3 > m_ikLock;

	ikcontextikrule_t_PS3 m_ikLock[ MAX_IKLOCKS ];

	matrix3x4a_t m_rootxform;
 
 	int			 m_iFramecounter;
 	float		 m_flTime;
 	int			 m_boneMask;
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------


// replaces the bonetoworld transforms for all bones that are procedural
// bool CalcProceduralBone_PS3(
// 	const CStudioHdr_PS3 *pStudioHdr,
// 	int iBone,
// 	CBoneAccessor_PS3 &bonetoworld
// 	);

// void Studio_BuildMatrices(
// 	const CStudioHdr_PS3 *pStudioHdr,
// 	const QAngle& angles, 
// 	const BoneVector& origin, 
// 	const BoneVector pos[],
// 	const BoneQuaternion q[],
// 	int iBone,
// 	float flScale,
// 	matrix3x4a_t bonetoworld[MAXSTUDIOBONES],
// 	int boneMask
// 	);
 

// Get a bone->bone relative transform
//void Studio_CalcBoneToBoneTransform( const CStudioHdr *pStudioHdr, int inputBoneIndex, int outputBoneIndex, matrix3x4_t &matrixOut );

// Given a bone rotation value, figures out the value you need to give to the controller
// to have the bone at that value.
// [in]  flValue  = the desired bone rotation value
// [out] ctlValue = the (0-1) value to set the controller t.
// return value   = flValue, unwrapped to lie between the controller's start and end.
//float Studio_SetController( const CStudioHdr *pStudioHdr, int iController, float flValue, float &ctlValue );


// Given a 0-1 controller value, maps it into the controller's start and end and returns the bone rotation angle.
// [in] ctlValue  = value in controller space (0-1).
// return value   = value in bone space
//float Studio_GetController( const CStudioHdr *pStudioHdr, int iController, float ctlValue );

//void Studio_CalcDefaultPoseParameters( const CStudioHdr *pStudioHdr, float flPoseParameter[MAXSTUDIOPOSEPARAM], int nCount );
//float Studio_GetPoseParameter( const CStudioHdr *pStudioHdr, int iParameter, float ctlValue );
//float Studio_SetPoseParameter( const CStudioHdr *pStudioHdr, int iParameter, float flValue, float &ctlValue );

// converts a global 0..1 pose parameter into the local sequences blending value
//int Studio_LocalPoseParameter( const CStudioHdr *pStudioHdr, const float poseParameter[], mstudioseqdesc_t &seqdesc, int iSequence, int iLocalIndex, float &flSetting );

//void Studio_SeqAnims( const CStudioHdr *pStudioHdr, mstudioseqdesc_t &seqdesc, int iSequence, const float poseParameter[], mstudioanimdesc_t *panim[4], float *weight );
//int Studio_MaxFrame( const CStudioHdr *pStudioHdr, int iSequence, const float poseParameter[] );
//float Studio_FPS( const CStudioHdr *pStudioHdr, int iSequence, const float poseParameter[] );
//float Studio_CPS( const CStudioHdr *pStudioHdr, mstudioseqdesc_t &seqdesc, int iSequence, const float poseParameter[] );
//float Studio_Duration( const CStudioHdr *pStudioHdr, int iSequence, const float poseParameter[] );
//void Studio_MovementRate( const CStudioHdr *pStudioHdr, int iSequence, const float poseParameter[], Vector *pVec );
//float Studio_SeqMovementAndDuration( const CStudioHdr *pStudioHdr, int iSequence, float flCycleFrom, float flCycleTo, const float poseParameter[], Vector &deltaPos );

// void Studio_Movement( const CStudioHdr *pStudioHdr, int iSequence, const float poseParameter[], Vector *pVec );

//void Studio_AnimPosition( mstudioanimdesc_t *panim, float flCycle, Vector &vecPos, Vector &vecAngle );
//void Studio_AnimVelocity( mstudioanimdesc_t *panim, float flCycle, Vector &vecVelocity );
//float Studio_FindAnimDistance( mstudioanimdesc_t *panim, float flDist );
// bool Studio_AnimMovement( mstudioanimdesc_t *panim, float flCycleFrom, float flCycleTo, Vector &deltaPos, QAngle &deltaAngle );
// bool Studio_SeqMovement( const CStudioHdr *pStudioHdr, int iSequence, float flCycleFrom, float flCycleTo, const float poseParameter[], Vector &deltaMovement, QAngle &deltaAngle );
// bool Studio_SeqVelocity( const CStudioHdr *pStudioHdr, int iSequence, float flCycle, const float poseParameter[], Vector &vecVelocity );
// float Studio_FindSeqDistance( const CStudioHdr *pStudioHdr, int iSequence, const float poseParameter[], float flDist );
// float Studio_FindSeqVelocity( const CStudioHdr *pStudioHdr, int iSequence, const float poseParameter[], float flVelocity );
// int Studio_FindAttachment( const CStudioHdr *pStudioHdr, const char *pAttachmentName );
// int Studio_FindRandomAttachment( const CStudioHdr *pStudioHdr, const char *pAttachmentName );
// int Studio_BoneIndexByName( const CStudioHdr *pStudioHdr, const char *pName );
// const char *Studio_GetDefaultSurfaceProps( CStudioHdr *pstudiohdr );
// float Studio_GetMass( CStudioHdr *pstudiohdr );
// const char *Studio_GetKeyValueText( const CStudioHdr *pStudioHdr, int iSequence );

// FORWARD_DECLARE_HANDLE( memhandle_t );
// struct bonecacheparams_t
// {
// 	CStudioHdr		*pStudioHdr;
// 	matrix3x4a_t	*pBoneToWorld;
// 	float			curtime;
// 	int				boneMask;
// };
// 
// class CBoneCache
// {
// public:
// 
// 	// you must implement these static functions for the ResourceManager
// 	// -----------------------------------------------------------
// 	static CBoneCache *CreateResource( const bonecacheparams_t &params );
// 	static unsigned int EstimatedSize( const bonecacheparams_t &params );
// 	// -----------------------------------------------------------
// 	// member functions that must be present for the ResourceManager
// 	void			DestroyResource();
// 	CBoneCache		*GetData() { return this; }
// 	unsigned int	Size() { return m_size; }
// 	// -----------------------------------------------------------
// 
// 					CBoneCache();
// 
// 	// was constructor, but placement new is messy wrt memdebug - so cast & init instead
// 	void			Init( const bonecacheparams_t &params, unsigned int size, short *pStudioToCached, short *pCachedToStudio, int cachedBoneCount );
// 	
// 	void			UpdateBones( const matrix3x4a_t *pBoneToWorld, int numbones, float curtime );
// 	matrix3x4a_t	*GetCachedBone( int studioIndex );
// 	void			ReadCachedBones( matrix3x4a_t *pBoneToWorld );
// 	void			ReadCachedBonePointers( matrix3x4_t **bones, int numbones );
// 
// 	bool			IsValid( float curtime, float dt = 0.1f );
// 
// public:
// 	float			m_timeValid;
// 	int				m_boneMask;
// 
// private:
// 	matrix3x4a_t	*BoneArray();
// 	short			*StudioToCached();
// 	short			*CachedToStudio();
// 
// 	unsigned int	m_size;
// 	unsigned short	m_cachedBoneCount;
// 	unsigned short	m_matrixOffset;
// 	unsigned short	m_cachedToStudioOffset;
// 	unsigned short	m_boneOutOffset;
// };
// 
// void Studio_LockBoneCache();
// void Studio_UnlockBoneCache();
// 
// CBoneCache *Studio_GetBoneCache( memhandle_t cacheHandle, bool bLock = false );
// void Studio_ReleaseBoneCache( memhandle_t cacheHandle );
// memhandle_t Studio_CreateBoneCache( bonecacheparams_t &params );
// void Studio_DestroyBoneCache( memhandle_t cacheHandle );
// void Studio_InvalidateBoneCacheIfNotMatching( memhandle_t cacheHandle, float flTimeValid );

// Given a ray, trace for an intersection with this studiomodel.  Get the array of bones from StudioSetupHitboxBones
// bool TraceToStudio( class IPhysicsSurfaceProps *pProps, const Ray_t& ray, CStudioHdr *pStudioHdr, mstudiohitboxset_t *set, matrix3x4_t **hitboxbones, int fContentsMask, const Vector &vecOrigin, float flScale, trace_t &trace );
// TERROR: TraceToStudio variant that prioritizes hitgroups, so bullets can pass through arms and chest to hit the head, for instance
// bool TraceToStudioGrouped( IPhysicsSurfaceProps *pProps, const Ray_t& ray, CStudioHdr *pStudioHdr, mstudiohitboxset_t *set, 
// 						  matrix3x4_t **hitboxbones, int fContentsMask, trace_t &tr, const CUtlVector< int > &sortedHitgroups );

void QuaternionSM_PS3( float s, const Quaternion &p, const Quaternion &q, Quaternion &qt );
void QuaternionMA_PS3( const Quaternion &p, float s, const Quaternion &q, Quaternion &qt );
// 
// bool Studio_PrefetchSequence( const CStudioHdr *pStudioHdr, int iSequence );
// 
// void Studio_RunBoneFlexDrivers( float *pFlexController, const CStudioHdr *pStudioHdr, const Vector *pPositions, const matrix3x4_t *pBoneToWorld, const matrix3x4_t &mRootToWorld );

//-----------------------------------------------------------------------------
// Computes a number of twist bones given a parent/child pair
// pqTwists, pflWeights, pqTwistBinds must all have at least nCount elements 
//-----------------------------------------------------------------------------
// void ComputeTwistBones(
// 	Quaternion *pqTwists,
// 	int nCount,
// 	bool bInverse,
// 	const Vector &vUp,
// 	const Quaternion &qParent,
// 	const matrix3x4_t &mChild,
// 	const Quaternion &qBaseInv,
// 	const float *pflWeights,
// 	const Quaternion *pqTwistBinds );

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

// round up to next 16B boundary, then add 16B
#define ROUNDUPTONEXT16B( a ) (0x10 + (a + (0x10 - (a%0x10))))


//-----------------------------------------------------------------------------
// Squeezing memory for SPU - manual float and int allocs
//-----------------------------------------------------------------------------

#define LS_FLOATSTACK_SIZE		((MAXSTUDIOBONES_PS3 + 32) * 4 * 3 * 3)	// 24k
#define LS_INTSTACK_SIZE		((MAXSTUDIOBONES_PS3 * 2) + 16)		// 1k


extern ALIGN16 byte		g_ls_Stack[ (LS_FLOATSTACK_SIZE * sizeof(float)) + (LS_INTSTACK_SIZE * sizeof(int)) ];
extern byte				*g_ls_StackPtr;
extern byte				*g_ls_StackPtr_MAX;


// init when starting Job
FORCEINLINE void InitLSStack()
{
	g_ls_StackPtr		= g_ls_Stack;
}

// term when exiting Job
FORCEINLINE void TermLSStack()
{
#if !defined( _CERT )
	// ensure pop'd to start
	if( g_ls_StackPtr != g_ls_Stack )
	{
		Msg("*** IMPROPER LS STACK ON TERM ***\n");
		Msg("*** IMPROPER LS STACK ON TERM ***\n");
		Msg("*** IMPROPER LS STACK ON TERM ***\n");
		Msg("*** IMPROPER LS STACK ON TERM ***\n");
		DebuggerBreak();
	}
#endif

	// float watermark
//	Msg("LS stack - high: %d, max: %d\n", (int)(g_ls_StackPtr_MAX - g_ls_Stack), (int)sizeof(g_ls_Stack) );
}

// keep it simple, ensure allocation is multiple of 16B to keep vectors aligned
// size is number of bytes
FORCEINLINE byte *PushLSStack( int size )
{
	byte *pStack = g_ls_StackPtr;

	g_ls_StackPtr += ROUNDUPTONEXT16B( size );

#if !defined( _CERT )
	if( g_ls_StackPtr > (g_ls_Stack + sizeof(g_ls_Stack)) )
	{
		Msg("*** EXCEEDED LS STACK ***\n");
		Msg("*** EXCEEDED LS STACK ***\n");
		Msg("*** EXCEEDED LS STACK ***\n");
		Msg("*** EXCEEDED LS STACK ***\n");
		DebuggerBreak();
	}
#endif

	// maintain watermark
	if( g_ls_StackPtr > g_ls_StackPtr_MAX )
	{
		g_ls_StackPtr_MAX = g_ls_StackPtr;
	}

	return pStack;
}

FORCEINLINE void PopLSStack( int size )
{
	g_ls_StackPtr -= ROUNDUPTONEXT16B( size );

#if !defined( _CERT )
	if( g_ls_StackPtr < g_ls_Stack )
	{
		Msg("*** EXCEEDED LS STACK - POP ***\n");
		Msg("*** EXCEEDED LS STACK - POP ***\n");
		Msg("*** EXCEEDED LS STACK - POP ***\n");
		Msg("*** EXCEEDED LS STACK - POP ***\n");
		DebuggerBreak();
	}
#endif
}


// class CLSStack
// {
// public:
// 	CLSStack( int size )
// 	{
// 		;
// 	}
// 	~CLSStack()
// 	{
// 		;
// 	}
// };
// 
// #define LSSTACK(size) CLSStack



//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------


#if defined(__SPU__)

extern BoneVector				*g_posInit;
extern BoneQuaternion			*g_qInit;
extern ikcontextikrule_t_PS3	*g_addDep_IKRules;
extern int						g_addDep_numIKRules;

#else

extern BoneVector				g_posInit[ MAXSTUDIOBONES_PS3 ];
extern BoneQuaternion			g_qInit[ MAXSTUDIOBONES_PS3 ];

extern ikcontextikrule_t_PS3	g_addDep_IKRules[ MAX_IKRULES ]	ALIGN16;
extern int						g_addDep_numIKRules;

#endif

#endif // _PS3


#endif // BONE_SETUP_H
