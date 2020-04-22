//========== Copyright © Valve Corporation, All rights reserved. ========
#ifndef VJOBSINTERFACE_HDR
#define VJOBSINTERFACE_HDR

#include "appframework/iappsystem.h"

#define VJOBS_INTERFACE_VERSION "VJobs01"

#ifdef _PS3
#define VJOBS_ON_SPURS 1
#endif

enum RunTargetEnum
{
	RUN_TARGET_MAIN_CPU,
	RUN_TARGET_SATELLITE_CPU,
	RUN_TARGET_GPU
};

class VjobChain3;
class VjobChain4;

namespace job_fpcpatch
{
	struct FpcPatchState_t;
}

namespace job_fpcpatch2
{
	struct FpHeader_t;
}

struct VJobsRoot;
struct VJobInstance
{
	VJobInstance(): m_pRoot( NULL ){}
	VJobsRoot* m_pRoot;
	int m_nIncrementalVectorIndex; // this instance will be part of incremental vector
	virtual void OnVjobsInit() {} // gets called after m_pRoot was created and assigned
	virtual void OnVjobsShutdown() {} // gets called before m_pRoot is about to be destructed and NULL'ed
};


struct IVJobs: public IAppSystem
{
	// this must be called right before unloading PRX for the purpose of immediately reloading it
	virtual void BeforeReload() = 0;
	virtual void AfterReload() = 0;
	
	virtual void SetRunTarget( RunTargetEnum nRunTarget ) = 0;
	
//	virtual void StartTest() = 0;
	
	virtual void Register( VJobInstance * pInstance ) = 0;
	virtual void Unregister( VJobInstance * pInstance ) = 0;
	
	virtual void DoNothing() = 0;
};


#endif
