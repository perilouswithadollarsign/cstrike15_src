#ifndef INCLUDED_BUILDWORLDLISTS_PS3_H
#define INCLUDED_BUILDWORLDLISTS_PS3_H

#if defined( _PS3 )

#include "utlvector.h"
#include "vjobs/root.h"
#include <vjobs_interface.h>
#include <ps3/vjobutils.h>
#include <ps3/vjobutils_shared.h>
#include "vjobs/jobparams_shared.h"

#include "gl_matsysiface.h"


//--------------------------------------------------------------------------------------------------
// Job structure
//--------------------------------------------------------------------------------------------------

class CPS3BuildWorldListsJob : public VJobInstance
{
public:
	CPS3BuildWorldListsJob() 
	{
	}

	~CPS3BuildWorldListsJob() 
	{
	//	Shutdown();
	}


	void	OnVjobsInit( void );		// gets called after m_pRoot was created and assigned
	void	OnVjobsShutdown( void );	// gets called before m_pRoot is about to be destructed and NULL'ed

	void	Init( void );
	void	Shutdown( void );

	void CPS3BuildWorldListsJob::BuildWorldLists_SPU( job_buildworldlists::JobParams_t* pParam, job_buildworldlists::buildWorldListsDMAOut *pDMAOut, 
													  bool bDrawtopview, void *pEA_VolumeCuller, float *pOrthoCenter, float *pOrthoHalfDi, bool bTopViewNoBackfaceCulling, bool bTopViewNoVisCheck, 
													  bool bShadowDepth, void* pInfo, void *pEA_RenderListLeaves, int drawFlags, 
													  Frustum_t *pFrustum, Frustum_t *pAreaFrustum, unsigned char *pRenderAreaBits,
													  int buildViewID );
};

// output dma params

extern IVJobs * g_pVJobs;
extern CPS3BuildWorldListsJob* g_pBuildWorldListsJob;

extern job_buildworldlists::JobDescriptor_t g_buildWorldListsJobDescriptor[MAX_CONCURRENT_BUILDVIEWS] ALIGN128;

extern job_buildworldlists::buildWorldListsDMAOut g_buildWorldListsDMAOutData[MAX_CONCURRENT_BUILDVIEWS] ALIGN128;



#endif	// if !defined(_PS3)

#endif // INCLUDED_BUILDWORLDLISTS_PS3_H
