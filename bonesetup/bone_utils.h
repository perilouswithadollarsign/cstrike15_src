

DLL_IMPORT CLinkedMiniProfiler *g_pPhysicsMiniProfilers;
#if 0
#define BONE_PROFILE(ID)   static CLinkedMiniProfiler s_miniprofiler_##ID(#ID, &g_pPhysicsMiniProfilers);		\
	CMiniProfilerGuard mpguard_##ID(&s_miniprofiler_##ID);
#define BONE_PROFILE_LOOP(ID,COUNT)   static CLinkedMiniProfiler s_miniprofiler_##ID(#ID, &g_pPhysicsMiniProfilers);		\
	CMiniProfilerGuard mpguard_##ID(&s_miniprofiler_##ID,(COUNT));
#define BONE_PROFILE_FUNC()   static CLinkedMiniProfiler s_miniprofiler_FUNC(__FUNCTION__, &g_pPhysicsMiniProfilers);		\
	CMiniProfilerGuard mpguard_##ID(&s_miniprofiler_FUNC);
#else
#define BONE_PROFILE(ID)   
#define BONE_PROFILE_LOOP(ID,COUNT)
#define BONE_PROFILE_FUNC()  
#endif



template <typename T>
class CBoneSetupMemoryPool
{
public:
	T *Alloc()
	{
		T *p = (T *)m_FreeBlocks.Pop();
		if ( !p )
		{
			p = (T *)MemAlloc_AllocAligned( sizeof( T ) * MAXSTUDIOBONES, 16 );
			if ( ((size_t)p) % MAX(TSLIST_NODE_ALIGNMENT,16) != 0 )
				DebuggerBreak();
		}

		return p;
	}

	void Free( T *p )
	{
		m_FreeBlocks.Push( (TSLNodeBase_t *)p );
	}

private:
	CTSListBase m_FreeBlocks;
};

extern CBoneSetupMemoryPool<BoneQuaternionAligned> g_QuaternionPool;
extern CBoneSetupMemoryPool<BoneVector> g_VectorPool;
extern CBoneSetupMemoryPool<matrix3x4a_t> g_MatrixPool;



void CalcDecompressedAnimation( const mstudiocompressedikerror_t *pCompressed, int iFrame, float fraq, BoneVector &pos, BoneQuaternion &q );
void QuaternionAccumulate( const Quaternion &p, float s, const Quaternion &q, Quaternion &qt );
void CalcAnimation( const CStudioHdr *pStudioHdr, BoneVector *pos, BoneQuaternion *q, mstudioseqdesc_t &seqdesc, int sequence, int animation, float cycle, int boneMask );
void BlendBones( const CStudioHdr *pStudioHdr, BoneQuaternionAligned q1[MAXSTUDIOBONES], BoneVector pos1[MAXSTUDIOBONES], mstudioseqdesc_t &seqdesc, int sequence, const BoneQuaternionAligned q2[MAXSTUDIOBONES], const BoneVector pos2[MAXSTUDIOBONES], float s, int boneMask );
void ScaleBones( const CStudioHdr *pStudioHdr, BoneQuaternion q1[MAXSTUDIOBONES], BoneVector pos1[MAXSTUDIOBONES], int sequence, float s, int boneMask );

void CalcPose( const CStudioHdr *pStudioHdr, CIKContext *pIKContext, BoneVector pos[], BoneQuaternionAligned q[], int sequence, float cycle, const float poseParameter[], int boneMask, float flWeight = 1.0f, float flTime = 0.0f );
bool CalcPoseSingle( const CStudioHdr *pStudioHdr, BoneVector pos[], BoneQuaternionAligned q[], mstudioseqdesc_t &seqdesc, int sequence, float cycle, const float poseParameter[], int boneMask, float flTime );

void CalcBoneAdj( const CStudioHdr *pStudioHdr, BoneVector pos[], BoneQuaternion q[], const float controllers[], int boneMask );

void BuildBoneChainPartial(
	const CStudioHdr *pStudioHdr,
	const matrix3x4_t &rootxform,
	const BoneVector pos[], 
	const BoneQuaternion q[], 
	int	iBone,
	matrix3x4_t *pBoneToWorld,
	CBoneBitList &boneComputed,
	int iRoot );


class CBoneSetup
{
public:
	CBoneSetup( const CStudioHdr *pStudioHdr, int boneMask, const float poseParameter[], IPoseDebugger *pPoseDebugger = NULL );
	void InitPose( BoneVector pos[], BoneQuaternionAligned q[] );
	void AccumulatePose( BoneVector pos[], BoneQuaternion q[], int sequence, float cycle, float flWeight, float flTime, CIKContext *pIKContext );
	void CalcAutoplaySequences(	BoneVector pos[], BoneQuaternion q[], float flRealTime, CIKContext *pIKContext );
private:
	void AddSequenceLayers( BoneVector pos[], BoneQuaternion q[], mstudioseqdesc_t &seqdesc, int sequence, float cycle, float flWeight, float flTime, CIKContext *pIKContext );
	void AddLocalLayers( BoneVector pos[], BoneQuaternion q[], mstudioseqdesc_t &seqdesc, int sequence, float cycle, float flWeight, float flTime, CIKContext *pIKContext );
public:
	const CStudioHdr *m_pStudioHdr;
	int m_boneMask;
	const float *m_flPoseParameter;
	IPoseDebugger *m_pPoseDebugger;
};



