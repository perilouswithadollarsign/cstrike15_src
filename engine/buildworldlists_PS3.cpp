//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#include "buildworldlists_PS3.h"

#include "gl_model_private.h"
#include "host.h"
#include "disp.h"

#include "vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

CPS3BuildWorldListsJob g_BuildWorldListsJob;
CPS3BuildWorldListsJob* g_pBuildWorldListsJob = &g_BuildWorldListsJob;

job_buildworldlists::JobDescriptor_t g_buildWorldListsJobDescriptor[MAX_CONCURRENT_BUILDVIEWS] ALIGN128;
job_buildworldlists::buildWorldListsDMAOut g_buildWorldListsDMAOutData[MAX_CONCURRENT_BUILDVIEWS] ALIGN128;

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CPS3BuildWorldListsJob::Init( void )
{
	// Register with VJobs
	if( g_pVJobs )
	{
		g_pVJobs->Register( this );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CPS3BuildWorldListsJob::Shutdown()
{
	g_pVJobs->Unregister( this ); 
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CPS3BuildWorldListsJob::OnVjobsInit()
{
	for( int i = 0; i < MAX_CONCURRENT_BUILDVIEWS; i++ )
	{
		g_buildWorldListsJobDescriptor[i].header = *m_pRoot->m_pJobBuildWorldLists;

		g_buildWorldListsJobDescriptor[i].header.useInOutBuffer	= 0;
		g_buildWorldListsJobDescriptor[i].header.sizeStack		= (48*1024)/16;
		g_buildWorldListsJobDescriptor[i].header.sizeInOrInOut	= 0;
	}

}

//-----------------------------------------------------------------------------
//  
//-----------------------------------------------------------------------------
void CPS3BuildWorldListsJob::OnVjobsShutdown()
{
}

//--------------------------------------------------------------------------------------------------
// PPU calls in here to build on SPU
//--------------------------------------------------------------------------------------------------


int gSPUBreak = 0;

extern Vector modelorg;
extern bool g_bViewerInSolidSpace;
extern int r_visframecount;
extern CUtlVector< Frustum_t, CUtlMemoryAligned< Frustum_t,16 > > g_AreaFrustum;
extern unsigned char g_RenderAreaBits[32];
extern Frustum_t g_Frustum;

#define GET_OFFSET(type, field)    ((uint32)&(((type *)0)->field))

void CPS3BuildWorldListsJob::BuildWorldLists_SPU( job_buildworldlists::JobParams_t* pParam, job_buildworldlists::buildWorldListsDMAOut *pDMAOut, 
												   bool bDrawtopview, void *pEA_VolumeCuller, float *pOrthoCenter, float *pOrthoHalfDi, bool bTopViewNoBackfaceCulling, bool bTopViewNoVisCheck, 
												   bool bShadowDepth, void* pEA_Info, void *pEA_RenderListLeaves, int drawFlags, 
												   Frustum_t *pFrustum, Frustum_t *pAreaFrustum, unsigned char *pRenderAreaBits, 
												   int buildViewID )
{
	SNPROF("BuildWorldLists_SPU");

	// Fill in remaining src Params, assume dma dst params already filled

	pParam->m_nDebugBreak				= gSPUBreak;

	// which path
	pParam->m_bShadowDepth				= bShadowDepth;
	pParam->m_bDrawTopView				= bDrawtopview;
	pParam->m_bTopViewNoBackfaceCulling = bTopViewNoBackfaceCulling;
	pParam->m_bTopViewNoVisCheck        = bTopViewNoVisCheck;
	pParam->m_eaVolumeCuller			= uintp(pEA_VolumeCuller);

	if( !pEA_VolumeCuller )
	{
		pParam->m_orthoCenter[0] = pOrthoCenter[0];
		pParam->m_orthoCenter[1] = pOrthoCenter[1];
		pParam->m_orthoHalfDi[0] = pOrthoHalfDi[0];
		pParam->m_orthoHalfDi[1] = pOrthoHalfDi[1];
	}

	pParam->m_eaWorldNodes  = uintp(host_state.worldbrush->nodes);
	pParam->m_visframecount = r_visframecount;
	
	pParam->m_pSurfaces2    = uintp(host_state.worldbrush->surfaces2);
	pParam->m_pmarksurfaces = uintp(host_state.worldbrush->marksurfaces);
	pParam->m_pLeafs        = uintp(host_state.worldbrush->leafs);
	pParam->m_pDispInfos	= uintp(((CDispArray*)host_state.worldbrush->hDispInfos)->m_pDispInfos);
	pParam->m_eaDispInfoReferences = uintp(host_state.worldbrush->m_pDispInfoReferences);

	memcpy((void*)pParam->m_ModelOrg,	&modelorg, sizeof(modelorg));
	pParam->m_bViewerInSolidSpace	= g_bViewerInSolidSpace;
	
	pParam->m_Disp_ParentSurfID_offset = GET_OFFSET(CDispInfo, m_ParentSurfID);
	pParam->m_Disp_BB_offset = GET_OFFSET(CDispInfo, m_BBoxMin);
	pParam->m_Disp_Info_Size = sizeof(CDispInfo);

	pParam->m_eaFrustum		   = uintp(pFrustum);
	pParam->m_eaAreaFrustum    = uintp(pAreaFrustum);
	pParam->m_eaRenderAreaBits = uintp(pRenderAreaBits);

	pParam->m_nNumSortID	= materials->GetNumSortIDs();

	pParam->m_nMaxVisitSurfaces = host_state.worldbrush->numsurfaces;
	pParam->m_nAreaFrustum	= g_AreaFrustum.Count();

	pParam->m_eaInfo		= uintp(pEA_Info); 
	pParam->m_eaRenderListLeaves = uintp(pEA_RenderListLeaves);

	pParam->m_DrawFlags     = drawFlags;
	pParam->m_buildViewID   = buildViewID;

	pParam->m_eaDMAOut		= uintp(pDMAOut);

	// hacky - careful - byte offset to size/Count() member of CUtlVector
	pParam->m_nUtlCountOffset = 12; //GET_OFFSET(CUtlVector<int>, m_Size);


	// Push Job

	ConVarRef r_PS3_SPU_BuildWRLists_ImmediateSync("r_PS3_SPU_BuildWRLists_ImmediateSync");


	if( r_PS3_SPU_BuildWRLists_ImmediateSync.GetInt() )
	{
		CELL_VERIFY( m_pRoot->m_queuePortBuildWorld[ buildViewID ].pushJob( &g_buildWorldListsJobDescriptor[ buildViewID ].header, 
			sizeof(g_buildWorldListsJobDescriptor[0]), 0, CELL_SPURS_JOBQUEUE_FLAG_SYNC_JOB ) );
		CELL_VERIFY( m_pRoot->m_queuePortBuildWorld[ buildViewID ].sync(0) );
	}
	else
	{
		int syncTag = 1 + buildViewID;
		CELL_VERIFY( m_pRoot->m_queuePortBuildWorld[ buildViewID ].pushJob( &g_buildWorldListsJobDescriptor[ buildViewID ].header, 
			sizeof(g_buildWorldListsJobDescriptor[0]), syncTag, CELL_SPURS_JOBQUEUE_FLAG_SYNC_JOB ) );
		//Msg("pushJob World(%d) SyncTag(%d)\n", buildViewID, syncTag );
	}

}


