//========= Copyright © Valve Corporation, All rights reserved. ============//
#ifndef BUILDINDICES_PS3_HDR
#define BUILDINDICES_PS3_HDR

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

#endif // __SPU__


#define MAX_BUILDINDICES_JOBS 32

// round up to next 16B boundary, then add 16B
#define ROUNDUPTONEXT16B( a ) (0x10 + ((a) + (0x10 - ((a)%0x10))))


struct ALIGN128 buildIndicesJob_SPU
{
	int						debugJob;						// for dynamic switching of DebuggerBreak

	int						count;
	int						maxIndices;						//
	int						worldStaticMeshesCount;			//

	int						listIndex;
	int						indexCount;
	int						meshListCount;
	int						batchListCount;

	void*					pEA_groupList_listIndexStart;	// surfacesortgroup *
	void*					pEA_worldStaticMeshes;			// IMesh **, g_WorldStaticMeshes[]
	void*					pEA_lastMesh;					// IMesh *

	void*					pEA_sortList_materiallist;		// CMSurfaceSortList.m_list materiallist_t * via CUtlVector
	int32					group_listHead;					// surfacesortgroup_t.listHead

// worldbrushdata_t
	void*					pEA_worldbrush_surfaces1;		// msurface1_t * (host_state.worldbrush
	void*					pEA_worldbrush_surfaces2;		// msurface2_t *
	void*					pEA_worldbrush_primitives;		// mprimitive_t *
	void*					pEA_worldbrush_primindices;		// unsigned short *
	int						worldbrush_numsurfaces;			//

// CIndexBuilder
	void*					pEA_indexbuilder_indices;		// unsigned short *
	int						indexbuilder_indexOffset;		//
	unsigned int			indexbuilder_indexSize;			//
	int						indexbuilder_indexCount;		//
	int						indexbuilder_currentIndex;		//

	int						pad[1];							// to 16B boundary, shouldn't need this

} ALIGN128_POST;



#if !defined( __SPU__ )
struct ALIGN128 PS3BuildIndicesJobData
{
public:

	job_buildindices::JobDescriptor_t	jobDescriptor			ALIGN128;

	// src, SPU in only, going to SPU at start of job
	buildIndicesJob_SPU					buildIndicesJobSPU;

} ALIGN128_POST;


class CPS3BuildIndicesJob : public VJobInstance
{
public:
	CPS3BuildIndicesJob() 
	{
	}

	~CPS3BuildIndicesJob() 
	{
		//Shutdown();
	}


	void	OnVjobsInit( void );		// gets called after m_pRoot was created and assigned
	void	OnVjobsShutdown( void );	// gets called before m_pRoot is about to be destructed and NULL'ed

	void	Init( void );
	void	Shutdown( void );

	void    Sync( void );
	void    Push( job_buildindices::JobDescriptor_t *pJobDescriptor );

 	void	ResetBoneJobs( void );
 
 	PS3BuildIndicesJobData *GetJobData( void );

	CUtlVector<PS3BuildIndicesJobData>	m_buildIndicesJobData;

private:

 	int									m_buildIndicesJobCount;
 	int									m_buildIndicesJobNextSPURSPort;

	bool								m_bEnabled;
};



extern IVJobs * g_pVJobs;
extern CPS3BuildIndicesJob* g_pBuildIndicesJob;
extern job_buildindices::JobDescriptor_t g_buildIndicesJobDescriptor ALIGN128;




#endif // #if !defined(__SPU__)


#endif	// if !defined(_PS3)


#endif
