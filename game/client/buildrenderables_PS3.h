//========= Copyright © Valve Corporation, All rights reserved. ============//
#ifndef BUILDRENDERABLES_PS3_HDR
#define BUILDRENDERABLES_PS3_HDR

#if defined( _PS3 )

#if defined(__SPU__)

#include "ps3/spu_job_shared.h"

#else

#include "utlvector.h"
#include "vjobs/root.h"
#include <vjobs_interface.h>
#include <ps3/vjobutils.h>
#include <ps3/vjobutils_shared.h>
#include "vjobs/jobparams_shared.h"

#include "ClientLeafSystem.h"

#endif // __SPU__


// copied from bspfile.h, do we need the rest of that file?
#define MAX_MAP_AREAS 256

// round up to next 16B boundary, then add 16B
#define ROUNDUPTONEXT16B( a ) (0x10 + ((a) + (0x10 - ((a)%0x10))))


struct ALIGN128 buildRenderablesJob_SPU
{
	int					debugJob;					// for dynamic switching of DebuggerBreak
	uint32				pEA_debugRenderable;

	SetupRenderInfo_t	info;
	void*				pEA_worldbrush_leafs;		// mleaf_t *

	void*				pEA_detailObjects;			// CDetailModel *, g_detailObjectSystem->m_DetailObjects.Base()
	void*				pEA_detailObjects_origin;	// CDetailModel *, g_detailObjectSystem->m_DetailObjects.Base()
													// this is ptr to detailmodel.m_origin, so we don't have to fetch the whole thing, stride can this still be sizeof(CDetailModel) to access elements
	int					detailObjects_count;		// g_detailObjectSystem->m_DetailObjects.count()
	Vector				viewOrigin;					// g_vecCurrentRenderOrigin, CurrentViewOrigin()
	int					strideCDetailModel;

	void*				pEA_clientRenderablesList_RenderGroups;	// ptr to CEntry m_REnderGroups[RENDER_GROUP_COUNT][MAX_GROUP_ENTITIRES]
	void*				pEA_clientRenderablesList_RenderGroupCounts;	// ptr to int m_REnderGroupCounts[RENDER_GROUP_COUNT]


	DistanceFadeInfo_t	info_mpRenderList_mDetailFade;

	int					maxCount;
	int					buildFastReflectionRenderables;	// r_fastreflectionfastpath

//
	void*				pEA_frustums[MAX_MAP_AREAS];	// Frustum_t *
	int					numAreaFrustums;

// detailobjectsystem info
	float				flFactor;
	float				flDetailFadeStart;
	float				flDetailFadeEnd;

// translucency
	float				flMinLevelFadeArea;
	float				flMaxLevelFadeArea;
	float				flMinGlobalFadeArea;
	float				flMaxGlobalFadeArea;
	float				flGlobalDistFadeScale;
	int					bComputeScreenFade;
	ScreenSizeComputeInfo_t screensizecomputeinfo;

	int					shouldDrawDetailObjects;


// utl
	void*				pEA_clientleafsystem_mleaf;
	void*				pEA_renderablesLIST;		// renderable_LIST_t *
	void*				pEA_renderablesInLeafLIST;	// renderableInLeaf_LIST_t *
	int					renderablesHeadIdx;

// extract 
	int					clientleafsystem_alternateSortCount; //
	int					clientleafsystem_drawStaticProps; //
	int					clientleafsystem_disableShadowDepthCount;

// convar copies
	int					r_drawallrenderables;	
	int					r_portaltestents_AND_NOTr_portalsopenall;
	int					r_occlusion;

// gpGlobals
	int					frameCount;
	float				curTime;

// CSM View
	bool				bCSMView;
	bool				bDisableCSMCulling;
	bool				bShadowEntities;
	bool				bShadowStaticProps;
	bool				bShadowSprites;
	bool				bIgnoreDisableShadowDepthRendering;
	int					cascadeID;
	int					debugViewID;
	int					debugViewID_DEBUG;

// OUT
	void*				ppEA_Renderables;			// RenderableInfo **
	void*				pEA_RenderablesCount;		// int *
	void*				pEA_RLInfo;					// BuildRenderListInfo_t *

	void*				pEA_DetailRenderables;		//  DetailRenderableInfo_t *
	void*				pEA_DetailRenderablesCount;	// int *

	enum
	{
		MAX_GROUP_ENTITIES = 4096,
		MAX_BONE_SETUP_DEPENDENCY = 64,
	};



} ALIGN128_POST;


#if !defined( __SPU__ )
struct ALIGN128 PS3BuildRenderablesJobData
{
public:

	job_buildrenderables::JobDescriptor_t	jobDescriptor			ALIGN128;

	// src, SPU in only, going to SPU at start of job
	buildRenderablesJob_SPU					buildRenderablesJobSPU;

} ALIGN128_POST;


class CPS3BuildRenderablesJob : public VJobInstance
{
public:
	CPS3BuildRenderablesJob() 
	{
	}

	~CPS3BuildRenderablesJob() 
	{
		Shutdown();
	}


	void	OnVjobsInit( void );		// gets called after m_pRoot was created and assigned
	void	OnVjobsShutdown( void );	// gets called before m_pRoot is about to be destructed and NULL'ed

	void	Init( void );
	void	Shutdown( void );

 	void	ResetBoneJobs( void );
 
 	PS3BuildRenderablesJobData *GetJobData( int job );

	CUtlVector<PS3BuildRenderablesJobData>	m_buildRenderablesJobData;

private:

 	int									m_buildRenderablesJobCount;
 	int									m_buildRenderablesJobNextSPURSPort;

	bool								m_bEnabled;
};



extern IVJobs * g_pVJobs;
extern CPS3BuildRenderablesJob* g_pBuildRenderablesJob;
extern job_buildrenderables::JobDescriptor_t g_buildRenderablesJobDescriptor ALIGN128;




#endif // #if !defined(__SPU__)


#endif	// if !defined(_PS3)


#endif