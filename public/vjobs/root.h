//========== Copyright © Valve Corporation, All rights reserved. ========
#ifndef JOB_VJOBS_ROOT_HDR
#define JOB_VJOBS_ROOT_HDR

#ifdef _PS3
#include <cell/spurs.h>
#include "vjobs/edgegeomparams_shared.h"
#include "const.h"
#endif


// this structure gives the client a kind of "root access" to SPURS and all VJobs functionality
struct ALIGN128 VJobsRoot
{
	enum AlignmentEnum_t {ALIGNMENT = 128};
	enum {MAXPORTS_ANIM = 32};

#ifdef _PS3
	cell::Spurs::Spurs m_spurs;

	// the job queue processes a lot of Edge jobs, and edge jobs have the largest descriptors of all
	cell::Spurs::JobQueue::JobQueue< 512, 256 > m_largeJobQueue;
	cell::Spurs::JobQueue::JobQueue< 512, sizeof( job_edgegeom::JobDescriptor_t ) > m_smallJobQueue;

	cell::Spurs::JobQueue::JobQueue< 512, sizeof( job_edgegeom::JobDescriptor_t ) > m_buildWorldRenderableJobQueue;

	JobQueue::Port2 m_queuePortBlobulator, m_queuePortSound;//DECL_ALIGN( CELL_SPURS_JOBQUEUE_PORT2_ALIGN );

	JobQueue::Port2 m_queuePortAnim[ MAXPORTS_ANIM ];
	JobQueue::Port2 m_queuePortBuildIndices;
	JobQueue::Port2 m_queuePortBuildWorldAndRenderables;
	JobQueue::Port2 m_queuePortBuildWorld[ MAX_CONCURRENT_BUILDVIEWS ];
	JobQueue::Port2 m_queuePortBuildRenderables[ MAX_CONCURRENT_BUILDVIEWS ];

	uint64 m_nSpugcmChainPriority;
	uint64 m_nEdgeChainPriority;
	uint64 m_nFpcpChainPriority;
	uint64 m_nSystemWorkloadPriority;
	uint64 m_nSlimJobQueuePriority;
	uint64 m_nBulkJobQueuePriority;
	uint64 m_nEdgePostWorkloadPriority;
	uint64 m_nGemWorkloadPriority;
										 
	const CellSpursJobHeader *m_pFpcPatch2;
	const CellSpursJobHeader *m_pJobNotify;
	const CellSpursJobHeader *m_pJobZPass;
	const CellSpursJobHeader *m_pCtxFlush;
	const CellSpursJobHeader *m_pGcmStateFlush;
	const CellSpursJobHeader *m_pEdgeGeom;
	const CellSpursJobHeader *m_pDrawIndexedPrimitive;
	const CellSpursJobHeader *m_pJobBlobulator;
	const CellSpursJobHeader *m_pJobSndUpsampler;
	const CellSpursJobHeader *m_pJobMp3Dec;
	const CellSpursJobHeader *m_pJobZlibInflate;
	const CellSpursJobHeader *m_pJobZlibDeflate;
	const CellSpursJobHeader *m_pJobAccumPose;
	const CellSpursJobHeader *m_pJobBuildIndices;
	const CellSpursJobHeader *m_pJobBuildRenderables;
	const CellSpursJobHeader *m_pJobBuildWorldLists;

#endif
}
ALIGN128_POST;


#endif
