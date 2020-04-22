//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =====//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#include "engine/IEngineTrace.h"
#include "icliententitylist.h"
#include "ispatialpartitioninternal.h"
#include "icliententity.h"
#include "cmodel_engine.h"
#include "dispcoll_common.h"
#include "staticpropmgr.h"
#include "server.h"
#include "edict.h"
#include "gl_model_private.h"
#include "world.h"
#include "vphysics_interface.h"
#include "client_class.h"
#include "server_class.h"
#include "debugoverlay.h"
#include "collisionutils.h"
#include "tier0/vprof.h"
#include "convar.h"
#include "mathlib/polyhedron.h"
#include "sys_dll.h"
#include "vphysics/virtualmesh.h"
#include "tier1/utlhashtable.h"
#include "tier1/refcount.h"
#include "vstdlib/jobthread.h"
#include "tier0/microprofiler.h"
#if !COMPILER_GCC
#include <atomic>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Various statistics to gather
//-----------------------------------------------------------------------------
enum
{
	TRACE_STAT_COUNTER_TRACERAY = 0,
	TRACE_STAT_COUNTER_POINTCONTENTS,
	TRACE_STAT_COUNTER_ENUMERATE,
	NUM_TRACE_STAT_COUNTER
};


//-----------------------------------------------------------------------------
// Used to visualize raycasts going on
//-----------------------------------------------------------------------------
#ifdef _DEBUG

ConVar debugrayenable( "debugrayenable", "0", NULL, "Use this to enable ray testing.  To reset: bind \"F1\" \"clearalloverlays; debugrayreset  0; host_framerate 66.66666667\"" );
ConVar debugrayreset( "debugrayreset", "0" );
ConVar debugraylimit( "debugraylimit", "500", NULL, "number of rays per frame that you have to hit before displaying them all" );
static CUtlVector<Ray_t> s_FrameRays;
#endif

#define BENCHMARK_RAY_TEST 0

#if BENCHMARK_RAY_TEST
static CUtlVector<Ray_t> s_BenchmarkRays;
#endif

class CAsyncOcclusionQuery;


//-----------------------------------------------------------------------------
// Implementation of IEngineTrace
//-----------------------------------------------------------------------------
abstract_class CEngineTrace : public IEngineTrace
{
public:
	CEngineTrace()
	{
		m_nOcclusionTestsSuspended = 0;
	}
	// Returns the contents mask at a particular world-space position
	virtual int		GetPointContents( const Vector &vecAbsPosition, int contentsMask, IHandleEntity** ppEntity );
	virtual int		GetPointContents_WorldOnly( const Vector &vecAbsPosition, int contentsMask );
	virtual int		GetPointContents_Collideable( ICollideable *pCollide, const Vector &vecAbsPosition );

	// Traces a ray against a particular edict
	virtual void	ClipRayToEntity( const Ray_t &ray, unsigned int fMask, IHandleEntity *pEntity, trace_t *pTrace );

	// A version that simply accepts a ray (can work as a traceline or tracehull)
	virtual void	TraceRay( const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace );

	// A version that sets up the leaf and entity lists and allows you to pass those in for collision.
	virtual void	SetupLeafAndEntityListRay( const Ray_t &ray, ITraceListData *pTraceData );
	virtual void    SetupLeafAndEntityListBox( const Vector &vecBoxMin, const Vector &vecBoxMax, ITraceListData *pTraceData );
	virtual void	TraceRayAgainstLeafAndEntityList( const Ray_t &ray, ITraceListData *pTraceData, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace );

	// A version that sweeps a collideable through the world
	// abs start + abs end represents the collision origins you want to sweep the collideable through
	// vecAngles represents the collision angles of the collideable during the sweep
	virtual void	SweepCollideable( ICollideable *pCollide, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
		const QAngle &vecAngles, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace );

	// Enumerates over all entities along a ray
	// If triggers == true, it enumerates all triggers along a ray
	virtual void	EnumerateEntities( const Ray_t &ray, bool triggers, IEntityEnumerator *pEnumerator );

	// Same thing, but enumerate entitys within a box
	virtual void	EnumerateEntities( const Vector &vecAbsMins, const Vector &vecAbsMaxs, IEntityEnumerator *pEnumerator );

	// FIXME: Different versions for client + server. Eventually we need to make these go away
	virtual ICollideable *HandleEntityToCollideable( IHandleEntity *pHandleEntity ) = 0;
	virtual ICollideable *GetWorldCollideable() = 0;

	// Traces a ray against a particular edict
	virtual void ClipRayToCollideable( const Ray_t &ray, unsigned int fMask, ICollideable *pEntity, trace_t *pTrace );

	// HACKHACK: Temp
	virtual int GetStatByIndex( int index, bool bClear );

	//finds brushes in an AABB, prone to some false positives
	virtual void GetBrushesInAABB( const Vector &vMins, const Vector &vMaxs, CBrushQuery &BrushQuery, int iContentsMask, int cmodelIndex );
	virtual void GetBrushesInCollideable( ICollideable *pCollideable, CBrushQuery &BrushQuery );

	//Creates a CPhysCollide out of all displacements wholly or partially contained in the specified AABB
	virtual CPhysCollide* GetCollidableFromDisplacementsInAABB( const Vector& vMins, const Vector& vMaxs );
	virtual int GetMeshesFromDisplacementsInAABB( const Vector& vMins, const Vector& vMaxs, virtualmeshlist_t *pOutputMeshes, int iMaxOutputMeshes );

	// gets the number of displacements in the world
	virtual int GetNumDisplacements( );

	// gets a specific diplacement mesh
	virtual void GetDisplacementMesh( int nIndex, virtualmeshlist_t *pMeshTriList );

	//retrieve brush planes and contents, returns zero if the brush doesn't exist, 
	//returns positive number of sides filled out if the array can hold them all, negative number of slots needed to hold info if the array is too small
	virtual int GetBrushInfo( int iBrush, int &ContentsOut, BrushSideInfo_t *pBrushSideInfoOut, int iBrushSideInfoArraySize );

	virtual bool PointOutsideWorld( const Vector &ptTest ); //Tests a point to see if it's outside any playable area


	// Walks bsp to find the leaf containing the specified point
	virtual int GetLeafContainingPoint( const Vector &ptTest );

	virtual ITraceListData *AllocTraceListData() { return new CTraceListData; }
	virtual void FreeTraceListData(ITraceListData *pTraceListData) { delete pTraceListData; }

	/// Used only in debugging: get/set/clear/increment the trace debug counter. See comment below for details.
	virtual int GetSetDebugTraceCounter( int value, DebugTraceCounterBehavior_t behavior );
	virtual const char *GetDebugName( IHandleEntity *pHandleEntity ) = 0;

	virtual bool IsFullyOccluded( int nOcclusionKey, const AABB_t &aabb1, const AABB_t &aabb2, const Vector &vShadow ) OVERRIDE;
	virtual void SuspendOcclusionTests() OVERRIDE{ m_nOcclusionTestsSuspended++; }
	virtual void ResumeOcclusionTests()OVERRIDE;
	virtual void FlushOcclusionQueries() OVERRIDE;
private:
	// FIXME: Different versions for client + server. Eventually we need to make these go away
	virtual void SetTraceEntity( ICollideable *pCollideable, trace_t *pTrace ) = 0;
	virtual ICollideable *GetCollideable( IHandleEntity *pEntity ) = 0;
	virtual int SpatialPartitionMask() const = 0;
	virtual int SpatialPartitionTriggerMask() const = 0;

	// Figure out point contents for entities at a particular position
	int EntityContents( const Vector &vecAbsPosition );

	// Should we perform the custom raytest?
	bool ShouldPerformCustomRayTest( const Ray_t& ray, ICollideable *pCollideable ) const;

	// Performs the custom raycast
	bool ClipRayToCustom( const Ray_t& ray, unsigned int fMask, ICollideable *pCollideable, trace_t* pTrace );

	// Perform vphysics trace
	bool ClipRayToVPhysics( const Ray_t &ray, unsigned int fMask, ICollideable *pCollideable, studiohdr_t *pStudioHdr, trace_t *pTrace );

	// Perform hitbox trace
	bool ClipRayToHitboxes( const Ray_t& ray, unsigned int fMask, ICollideable *pCollideable, trace_t* pTrace );

	// Perform bsp trace
	bool ClipRayToBSP( const Ray_t &ray, unsigned int fMask, ICollideable *pCollideable, trace_t *pTrace );

	// bbox
	bool ClipRayToBBox( const Ray_t &ray, unsigned int fMask, ICollideable *pCollideable, trace_t *pTrace );

	// OBB
	bool ClipRayToOBB( const Ray_t &ray, unsigned int fMask, ICollideable *pEntity, trace_t *pTrace );

	// Clips a trace to another trace
	bool ClipTraceToTrace( trace_t &clipTrace, trace_t *pFinalTrace );
private:
	int m_traceStatCounters[NUM_TRACE_STAT_COUNTER];
	int m_nOcclusionTestsSuspended;
	// Note: occlusion key MUST be evenly distributed for this to work well. Fortunately, it's just player ids, they're distributed perfectly well
	CUtlHashtable < int, CAsyncOcclusionQuery*, IdentityHashFunctor > m_OcclusionQueryMap;

	friend void RayBench( const CCommand &args );
	friend void RayBatchBench( const CCommand &args );
};

extern void FlushOcclusionQueries();

class CEngineTraceServer : public CEngineTrace
{
private:
	virtual ICollideable *HandleEntityToCollideable( IHandleEntity *pEnt );
	virtual const char *GetDebugName( IHandleEntity *pHandleEntity );
	virtual void SetTraceEntity( ICollideable *pCollideable, trace_t *pTrace );
	virtual int SpatialPartitionMask() const;
	virtual int SpatialPartitionTriggerMask() const;
	virtual ICollideable *GetWorldCollideable();
	friend void RayBench( const CCommand &args );
	friend void RayBatchBench( const CCommand &args );
public:
	// IEngineTrace
	virtual ICollideable *GetCollideable( IHandleEntity *pEntity );
};

#ifndef DEDICATED
class CEngineTraceClient : public CEngineTrace
{
private:
	virtual ICollideable *HandleEntityToCollideable( IHandleEntity *pEnt );
	virtual const char *GetDebugName( IHandleEntity *pHandleEntity );
	virtual void SetTraceEntity( ICollideable *pCollideable, trace_t *pTrace );
	virtual int SpatialPartitionMask() const;
	virtual int SpatialPartitionTriggerMask() const;
	virtual ICollideable *GetWorldCollideable();
public:
	// IEngineTrace
	virtual ICollideable *GetCollideable( IHandleEntity *pEntity );
};
#endif

//-----------------------------------------------------------------------------
// Expose CVEngineServer to the game + client DLLs
//-----------------------------------------------------------------------------
static CEngineTraceServer	s_EngineTraceServer;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CEngineTraceServer, IEngineTrace, INTERFACEVERSION_ENGINETRACE_SERVER, s_EngineTraceServer);

#ifndef DEDICATED
static CEngineTraceClient	s_EngineTraceClient;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CEngineTraceClient, IEngineTrace, INTERFACEVERSION_ENGINETRACE_CLIENT, s_EngineTraceClient);
#endif

//-----------------------------------------------------------------------------
// Expose CVEngineServer to the engine.
//-----------------------------------------------------------------------------
IEngineTrace *g_pEngineTraceServer = &s_EngineTraceServer;
#ifndef DEDICATED
IEngineTrace *g_pEngineTraceClient = &s_EngineTraceClient;
#endif

//-----------------------------------------------------------------------------
// Client-server neutral method of getting at collideables
//-----------------------------------------------------------------------------
#ifndef DEDICATED
ICollideable *CEngineTraceClient::GetCollideable( IHandleEntity *pEntity )
{
	Assert( pEntity );

	ICollideable *pProp = StaticPropMgr()->GetStaticProp( pEntity );
	if ( pProp )
		return pProp;

	IClientUnknown *pUnk = entitylist->GetClientUnknownFromHandle( pEntity->GetRefEHandle() );
	return pUnk->GetCollideable(); 
}
#endif

ICollideable *CEngineTraceServer::GetCollideable( IHandleEntity *pEntity )
{
	Assert( pEntity );

	ICollideable *pProp = StaticPropMgr()->GetStaticProp( pEntity );
	if ( pProp )
		return pProp;

	IServerUnknown *pNetUnknown = static_cast<IServerUnknown*>(pEntity);
	return pNetUnknown->GetCollideable();
}


//-----------------------------------------------------------------------------
// Spatial partition masks for iteration
//-----------------------------------------------------------------------------
#ifndef DEDICATED
int CEngineTraceClient::SpatialPartitionMask() const
{
	return PARTITION_CLIENT_SOLID_EDICTS;
}
#endif

int CEngineTraceServer::SpatialPartitionMask() const
{
	return PARTITION_ENGINE_SOLID_EDICTS;
}

#ifndef DEDICATED
int CEngineTraceClient::SpatialPartitionTriggerMask() const
{
	return PARTITION_CLIENT_TRIGGER_ENTITIES;
}
#endif

int CEngineTraceServer::SpatialPartitionTriggerMask() const
{
	return PARTITION_ENGINE_TRIGGER_EDICTS;
}


//-----------------------------------------------------------------------------
// Spatial partition enumerator looking for entities that we may lie within
//-----------------------------------------------------------------------------
class CPointContentsEnum : public IPartitionEnumerator
{
public:
	CPointContentsEnum( CEngineTrace *pEngineTrace, const Vector &pos, int contentsMask ) : m_Contents(CONTENTS_EMPTY), m_validMask(contentsMask)
	{
		m_pEngineTrace = pEngineTrace;
		m_Pos = pos; 
		m_pCollide = NULL;
	}

	static inline bool TestEntity( 
		CEngineTrace *pEngineTrace,
		ICollideable *pCollide, 
		const Vector &vPos, 
		int validMask,
		int *pContents, 
		ICollideable **pWorldCollideable )
	{
		// Deal with static props
		// NOTE: I could have added static props to a different list and
		// enumerated them separately, but that would have been less efficient

		if ( (validMask & CONTENTS_SOLID) && StaticPropMgr()->IsStaticProp( pCollide->GetEntityHandle() ) )
		{
			Ray_t ray;
			trace_t trace;
			ray.Init( vPos, vPos );
			pEngineTrace->ClipRayToCollideable( ray, MASK_ALL, pCollide, &trace );
			if (trace.startsolid)
			{
				// We're in a static prop; that's solid baby
				// Pretend we hit the world
				*pContents = CONTENTS_SOLID;
				*pWorldCollideable = pEngineTrace->GetWorldCollideable();
				return true;
			}
			return false;
		}
		
		// We only care about solid volumes
		if ((pCollide->GetSolidFlags() & FSOLID_VOLUME_CONTENTS) == 0)
			return false;

		model_t* pModel = (model_t*)pCollide->GetCollisionModel();
		if ( pModel && pModel->type == mod_brush )
		{
			Assert( pCollide->GetCollisionModelIndex() < MAX_MODELS && pCollide->GetCollisionModelIndex() >= 0 );
			int nHeadNode = GetModelHeadNode( pCollide );
			int contents = CM_TransformedPointContents( vPos, nHeadNode, 
				pCollide->GetCollisionOrigin(), pCollide->GetCollisionAngles() );

			if (contents & validMask)
			{
				// Return the contents of the first thing we hit
				*pContents = contents;
				*pWorldCollideable = pCollide;
				return true;
			}
		}

		return false;
	}

	IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
		ICollideable *pCollide = m_pEngineTrace->HandleEntityToCollideable( pHandleEntity );
		if (!pCollide)
			return ITERATION_CONTINUE;

		if ( CPointContentsEnum::TestEntity( m_pEngineTrace, pCollide, m_Pos, m_validMask, &m_Contents, &m_pCollide ) )
			return ITERATION_STOP;
		else
			return ITERATION_CONTINUE;
	}

private:
	static int GetModelHeadNode( ICollideable *pCollide )
	{
		int modelindex = pCollide->GetCollisionModelIndex();
		if(modelindex >= MAX_MODELS || modelindex < 0)
			return -1;

		model_t *pModel = (model_t*)pCollide->GetCollisionModel();
		if(!pModel)
			return -1;

		if(cmodel_t *pCModel = CM_InlineModelNumber(modelindex-1))
			return pCModel->headnode;
		else
			return -1;
	}

public:
	int m_Contents;
	ICollideable *m_pCollide;

private:
	CEngineTrace *m_pEngineTrace;
	Vector m_Pos;
	int m_validMask;
};


//-----------------------------------------------------------------------------
// Returns the world contents
//-----------------------------------------------------------------------------
int	CEngineTrace::GetPointContents_WorldOnly( const Vector &vecAbsPosition, int contentsMask )
{
	int nContents = CM_PointContents( vecAbsPosition, 0, contentsMask );

	return nContents;
}


//-----------------------------------------------------------------------------
// Returns the contents mask at a particular world-space position
//-----------------------------------------------------------------------------
int	CEngineTrace::GetPointContents( const Vector &vecAbsPosition, int contentsMask, IHandleEntity** ppEntity )
{
	VPROF( "CEngineTrace_GetPointContents" );
//	VPROF_BUDGET( "CEngineTrace_GetPointContents", "CEngineTrace_GetPointContents" );
	
	m_traceStatCounters[TRACE_STAT_COUNTER_POINTCONTENTS]++;
	// First check the collision model
	int nContents = CM_PointContents( vecAbsPosition, 0, contentsMask ) & contentsMask;
	
	if ( nContents != CONTENTS_SOLID )
	{
		CPointContentsEnum contentsEnum(this, vecAbsPosition, contentsMask);
		SpatialPartition()->EnumerateElementsAtPoint( SpatialPartitionMask(),
			vecAbsPosition, false, &contentsEnum );

		int nEntityContents = contentsEnum.m_Contents;
		if ( nEntityContents != CONTENTS_EMPTY )
		{
			if (ppEntity)
			{
				*ppEntity = contentsEnum.m_pCollide->GetEntityHandle();
			}

			return nEntityContents;
		}
	}

	if (ppEntity)
	{
		*ppEntity = GetWorldCollideable()->GetEntityHandle();
	}

	return nContents;
}


int CEngineTrace::GetPointContents_Collideable( ICollideable *pCollide, const Vector &vecAbsPosition )
{
	int contents = CONTENTS_EMPTY;
	ICollideable *pDummy = NULL;
	CPointContentsEnum::TestEntity( this, pCollide, vecAbsPosition, MASK_ALL, &contents, &pDummy );
	return contents;
}


//-----------------------------------------------------------------------------
// Should we perform the custom raytest?
//-----------------------------------------------------------------------------
inline bool CEngineTrace::ShouldPerformCustomRayTest( const Ray_t& ray, ICollideable *pCollideable ) const
{
	// No model? The entity's got its own collision detector maybe
	// Does the entity force box or ray tests to go through its code?
	return( (pCollideable->GetSolid() == SOLID_CUSTOM) ||
			(ray.m_IsRay && (pCollideable->GetSolidFlags() & FSOLID_CUSTOMRAYTEST )) || 
			(!ray.m_IsRay && (pCollideable->GetSolidFlags() & FSOLID_CUSTOMBOXTEST )) );
}


//-----------------------------------------------------------------------------
// Performs the custom raycast
//-----------------------------------------------------------------------------
bool CEngineTrace::ClipRayToCustom( const Ray_t& ray, unsigned int fMask, ICollideable *pCollideable, trace_t* pTrace )
{
	if ( pCollideable->TestCollision( ray, fMask, *pTrace ))
	{
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Performs the hitbox raycast, returns true if the hitbox test was made
//-----------------------------------------------------------------------------
bool CEngineTrace::ClipRayToHitboxes( const Ray_t& ray, unsigned int fMask, ICollideable *pCollideable, trace_t* pTrace )
{
	trace_t hitboxTrace;
	CM_ClearTrace( &hitboxTrace );

	// Keep track of the contents of what was hit initially
	hitboxTrace.contents = pTrace->contents;
	VectorAdd( ray.m_Start, ray.m_StartOffset, hitboxTrace.startpos );
	VectorAdd( hitboxTrace.startpos, ray.m_Delta, hitboxTrace.endpos );

	// At the moment, it has to be a true ray to work with hitboxes
	if ( !ray.m_IsRay )
		return false;

	// If the hitboxes weren't even tested, then just use the original trace
	if (!pCollideable->TestHitboxes( ray, fMask, hitboxTrace ))
		return false;

	// If they *were* tested and missed, clear the original trace
	if (!hitboxTrace.DidHit())
	{
		CM_ClearTrace( pTrace );
		pTrace->startpos = hitboxTrace.startpos;
		pTrace->endpos = hitboxTrace.endpos;
	}
	else if ( pCollideable->GetSolid() != SOLID_VPHYSICS )
	{
		// If we also hit the hitboxes, maintain fractionleftsolid +
		// startpos because those are reasonable enough values and the
		// hitbox code doesn't set those itself.
		Vector vecStartPos = pTrace->startpos;
		float flFractionLeftSolid = pTrace->fractionleftsolid;

		*pTrace = hitboxTrace;

		if (hitboxTrace.startsolid)
		{
			pTrace->startpos = vecStartPos;
			pTrace->fractionleftsolid = flFractionLeftSolid;
		}
	}
	else
	{
		// Fill out the trace hitbox details
		pTrace->contents = hitboxTrace.contents;
		pTrace->hitgroup = hitboxTrace.hitgroup;
		pTrace->hitbox = hitboxTrace.hitbox;
		pTrace->physicsbone = hitboxTrace.physicsbone;
		pTrace->surface = hitboxTrace.surface;
		Assert( pTrace->physicsbone >= 0 );
		// Fill out the surfaceprop details from the hitbox. Use the physics bone instead of the hitbox bone
		Assert(pTrace->surface.flags == SURF_HITBOX);
	}

	return true;
}

int CEngineTrace::GetStatByIndex( int index, bool bClear )
{
	if ( index >= NUM_TRACE_STAT_COUNTER )
		return 0;
	int out = m_traceStatCounters[index];
	if ( bClear )
	{
		m_traceStatCounters[index] = 0;
	}
	return out;
}




class CSetupBrushQuery : public CBrushQuery
{
public:
	void Setup( int iCount, uint32 *pBrushes, int iMaxBrushSides, TraceInfo_t *pTraceInfo )
	{
		m_iCount = iCount;
		m_pBrushes = pBrushes;
		m_iMaxBrushSides = iMaxBrushSides;
		m_pData = pTraceInfo;
		m_pReleaseFunc = CSetupBrushQuery::BrushQueryReleaseFunc;
	}

	static void BrushQueryReleaseFunc( CBrushQuery *pBrushQuery )
	{
		TraceInfo_t *pTraceInfo = reinterpret_cast<TraceInfo_t *>(reinterpret_cast<CSetupBrushQuery *>(pBrushQuery)->m_pData);
		EndTrace( pTraceInfo );
	}
};

void CEngineTrace::GetBrushesInAABB( const Vector &vMins, const Vector &vMaxs, CBrushQuery &BrushQuery, int nContentsMask, int nCModelIndex )
{
	BrushQuery.ReleasePrivateData();

	//similar to CM_BoxTraceAgainstLeafList() but tracking every brush we intersect
	TraceInfo_t *pTraceInfo = BeginTrace();
	if ( nContentsMask == CONTENTS_BRUSH_PAINT && !host_state.worldbrush->m_pSurfaceBrushList )
	{
		nContentsMask = MASK_ALL;
	}

	Vector vCenter = (vMins + vMaxs) * 0.5f;
	Vector vExtents = vMaxs - vCenter;

	CM_ClearTrace(&pTraceInfo->m_trace);
	// Setup global trace data. (This is nasty! I hate this.)
	pTraceInfo->m_bDispHit = false;
	pTraceInfo->m_DispStabDir.Init();
	pTraceInfo->m_contents = nContentsMask;
	VectorCopy( vCenter, pTraceInfo->m_start );
	VectorCopy( vCenter, pTraceInfo->m_end );
	VectorMultiply( vExtents, -1.0f, pTraceInfo->m_mins );
	VectorCopy( vExtents, pTraceInfo->m_maxs );
	VectorCopy( vExtents, pTraceInfo->m_extents );
	pTraceInfo->m_delta = vec3_origin;
	pTraceInfo->m_invDelta = vec3_origin;
	pTraceInfo->m_ispoint = false;
	pTraceInfo->m_isswept = false;

	int *pLeafList = (int *)stackalloc( pTraceInfo->m_pBSPData->numleafs * sizeof( int ) );
	int iNumLeafs = CM_BoxLeafnums( vMins, vMaxs, pLeafList, pTraceInfo->m_pBSPData->numleafs, NULL, nCModelIndex );

	TraceCounter_t *pVisitedBrushes = pTraceInfo->m_BrushCounters[0].Base();
	Plat_FastMemset( pVisitedBrushes, 0, pTraceInfo->m_BrushCounters[0].Count() * sizeof(TraceCounter_t) );

	TraceCounter_t *pKeepBrushes = pTraceInfo->m_BrushCounters[1].Base();
	int iKeepBrushCount = 0;
	int iMaxBrushSides = 0;

	for( int iLeaf = 0; iLeaf != iNumLeafs; ++iLeaf )
	{
		cleaf_t *pLeaf = &pTraceInfo->m_pBSPData->map_leafs[pLeafList[iLeaf]];
		for( int iBrushCounter = 0; iBrushCounter != pLeaf->numleafbrushes; ++iBrushCounter )
		{
			int iBrushNumber = pTraceInfo->m_pBSPData->map_leafbrushes[pLeaf->firstleafbrush + iBrushCounter];

			if( pVisitedBrushes[iBrushNumber] > 0 )
				continue;

			pVisitedBrushes[iBrushNumber] = 1;

			cbrush_t *pBrush = &pTraceInfo->m_pBSPData->map_brushes[iBrushNumber];

			// only collide with objects you are interested in
			if( !( pBrush->contents & nContentsMask ) )
				continue;

			CM_TestBoxInBrush( pTraceInfo, pBrush );

			if ( pTraceInfo->m_trace.allsolid )
			{
				//store the brush
				Assert( iKeepBrushCount < pTraceInfo->m_BrushCounters[0].Count() );

				pKeepBrushes[iKeepBrushCount] = iBrushNumber;
				++iKeepBrushCount;

				int iSideCount = pBrush->IsBox() ? 6 : pBrush->numsides;

				if( iSideCount > iMaxBrushSides )
				{
					iMaxBrushSides = iSideCount;
				}

				pTraceInfo->m_trace.allsolid = false; //clear the flag for re-use
			}
		}
	}

	
	//Purposefully not ending the trace here!
	//The CBrushQuery type holds onto the TraceInfo_t until it's destructed by whoever called us
	//EndTrace( pTraceInfo );
	((CSetupBrushQuery *)&BrushQuery)->Setup( iKeepBrushCount, pKeepBrushes, iMaxBrushSides, pTraceInfo );
}


static void GetBrushesInCollideable_r( CCollisionBSPData *pBSPData, TraceCounter_t *pVisitedBrushes, TraceCounter_t **pKeepBrushes, int node )
{
	if ( node < 0 )
	{
		int leafIndex = -1 - node;			

		// Add the solids in the "empty" leaf
		for ( int i = 0; i < pBSPData->map_leafs[leafIndex].numleafbrushes; i++ )
		{
			int brushIndex = pBSPData->map_leafbrushes[pBSPData->map_leafs[leafIndex].firstleafbrush + i];
			if( pVisitedBrushes[brushIndex] == 0 )
			{
				pVisitedBrushes[brushIndex] = 1;
				**pKeepBrushes = brushIndex;
				++(*pKeepBrushes);
			}
		}
	}
	else
	{
		cnode_t *pnode = &pBSPData->map_nodes[node];

		GetBrushesInCollideable_r( pBSPData, pVisitedBrushes, pKeepBrushes, pnode->children[0] );
		GetBrushesInCollideable_r( pBSPData, pVisitedBrushes, pKeepBrushes, pnode->children[1] );
	}
}

void CEngineTrace::GetBrushesInCollideable( ICollideable *pCollideable, CBrushQuery &BrushQuery )
{
	BrushQuery.ReleasePrivateData();

	//if( pCollideable->GetSolid() != SOLID_BSP )
	//	return; //should anything other than SOLID_BSP be valid?

	int nModelIndex = pCollideable->GetCollisionModelIndex();
	cmodel_t *pCModel = CM_InlineModelNumber( nModelIndex - 1 );
	if( pCModel == NULL )
		return;

	int nHeadNode = pCModel->headnode;

	TraceInfo_t *pTraceInfo = BeginTrace();

	CM_ClearTrace(&pTraceInfo->m_trace);
	// Setup global trace data. (This is nasty! I hate this.)
	pTraceInfo->m_bDispHit = false;
	pTraceInfo->m_DispStabDir.Init();
	pTraceInfo->m_contents = CONTENTS_EMPTY;
	VectorCopy( vec3_origin, pTraceInfo->m_start );
	VectorCopy( vec3_origin, pTraceInfo->m_end );
	VectorCopy( vec3_origin, pTraceInfo->m_mins );
	VectorCopy( vec3_origin, pTraceInfo->m_maxs );
	VectorCopy( vec3_origin, pTraceInfo->m_extents );
	pTraceInfo->m_delta = vec3_origin;
	pTraceInfo->m_invDelta = vec3_origin;
	pTraceInfo->m_ispoint = false;
	pTraceInfo->m_isswept = false;

	for( int i = 0; i != 2; ++i )
	{
		memset( pTraceInfo->m_BrushCounters[i].Base(), 0, pTraceInfo->m_BrushCounters[i].Count() * sizeof(TraceCounter_t) );
	}
	
	TraceCounter_t *pKeepBrushes = pTraceInfo->m_BrushCounters[1].Base(); //will get modified by GetBrushesInCollideable_r
	GetBrushesInCollideable_r( pTraceInfo->m_pBSPData, pTraceInfo->m_BrushCounters[0].Base(), &pKeepBrushes, nHeadNode );
	int iKeepBrushCount = pKeepBrushes - pTraceInfo->m_BrushCounters[1].Base();
	pKeepBrushes = pTraceInfo->m_BrushCounters[1].Base();

	int iMaxBrushSides = 0;
	for( int i = 0; i != iKeepBrushCount; ++i )
	{
		cbrush_t *pBrush = &pTraceInfo->m_pBSPData->map_brushes[pKeepBrushes[i]];
		int iSideCount = pBrush->IsBox() ? 6 : pBrush->numsides;

		if( iSideCount > iMaxBrushSides )
		{
			iMaxBrushSides = iSideCount;
		}
	}

	//Purposefully not ending the trace here!
	//The CBrushQuery type holds onto the TraceInfo_t until it's destructed by whoever called us
	//EndTrace( pTraceInfo );
	((CSetupBrushQuery *)&BrushQuery)->Setup( iKeepBrushCount, pKeepBrushes, iMaxBrushSides, pTraceInfo );
}



//-----------------------------------------------------------------------------
// Purpose: Used to copy the collision information of all displacement surfaces in a specified box
// Input  : vMins - min vector of the AABB
//			vMaxs - max vector of the AABB
// Output : CPhysCollide* the collision mesh created from all the displacements partially contained in the specified box
//					Note: We're not clipping to the box. Collidable may be larger than the box provided.
//-----------------------------------------------------------------------------
CPhysCollide* CEngineTrace::GetCollidableFromDisplacementsInAABB( const Vector& vMins, const Vector& vMaxs )
{
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	int *pLeafList = (int *)stackalloc( pBSPData->numleafs * sizeof( int ) ); 
	int iLeafCount = CM_BoxLeafnums( vMins, vMaxs, pLeafList, pBSPData->numleafs, NULL );

	// Get all the triangles for displacement surfaces in this box, add them to a polysoup
	CPhysPolysoup *pDispCollideSoup = physcollision->PolysoupCreate();

	// Count total triangles added to this poly soup- Can't support more than 65535.
	int iTriCount = 0;

	TraceInfo_t *pTraceInfo = BeginTrace();

	TraceCounter_t *pCounters = pTraceInfo->GetDispCounters();
	int count = pTraceInfo->GetCount();

	// For each leaf in which the box lies, Get all displacements in that leaf and use their triangles to create the mesh
	for ( int i = 0; i < iLeafCount; ++i )
	{
		// Current leaf
		cleaf_t curLeaf = pBSPData->map_leafs[ pLeafList[i] ];

		// Test box against all displacements in the leaf.
		for( int i = 0; i < curLeaf.dispCount; i++ )
		{
			int dispIndex = pBSPData->map_dispList[curLeaf.dispListStart + i];
			CDispCollTree *pDispTree = &g_pDispCollTrees[dispIndex];
		
			// make sure we only check this brush once per trace/stab
			if ( !pTraceInfo->Visit( pDispTree->m_iCounter, count, pCounters ) )
				continue;

			// If this displacement doesn't touch our test box, don't add it to the list.
			if ( !IsBoxIntersectingBox( vMins, vMaxs, pDispTree->m_mins, pDispTree->m_maxs) )
				continue;

			// The the triangle mesh for this displacement surface
			virtualmeshlist_t meshTriList;
			pDispTree->GetVirtualMeshList( &meshTriList );

			Assert ( meshTriList.indexCount%3 == 0 );
			Assert ( meshTriList.indexCount != 0 );
			Assert ( meshTriList.indexCount/3 == meshTriList.triangleCount );

			// Don't allow more than 64k triangles in a collision model
			// TODO: Return a list of collidables? How often do we break 64k triangles?
			iTriCount += meshTriList.triangleCount;
			if ( iTriCount > 65535 )
			{
				AssertMsg ( 0, "Displacement surfaces have too many triangles to duplicate in GetCollidableFromDisplacementsInBox." );
				EndTrace( pTraceInfo );
				return NULL;
			}

			for ( int j = 0; j < meshTriList.indexCount; j+=3 )
			{
				// Don't index past the index list
				Assert( j+2 < meshTriList.indexCount );

				if ( j+2 >= meshTriList.indexCount )
				{
					EndTrace( pTraceInfo );
					physcollision->PolysoupDestroy( pDispCollideSoup );
					return NULL;
				}

				unsigned short i0 = meshTriList.indices[j+0];
				unsigned short i1 = meshTriList.indices[j+1];
				unsigned short i2 = meshTriList.indices[j+2];

				// Don't index past the end of the vert list
				Assert ( i0 < meshTriList.vertexCount && i1 < meshTriList.vertexCount && i2 < meshTriList.vertexCount );

				if ( i0 >= meshTriList.vertexCount || i1 >= meshTriList.vertexCount || i2 >= meshTriList.vertexCount )
				{
					EndTrace( pTraceInfo );
					physcollision->PolysoupDestroy( pDispCollideSoup );
					return NULL;
				}

				Vector &v0 = meshTriList.pVerts[ i0 ];
				Vector &v1 = meshTriList.pVerts[ i1 ];
				Vector &v2 = meshTriList.pVerts[ i2 ];

				Assert ( v0.IsValid() && v1.IsValid() && v2.IsValid() );

				// We don't need exact clipping to the box... Include any triangle that has at least one vert on the inside
				if ( IsPointInBox( v0, vMins, vMaxs ) || IsPointInBox( v1, vMins, vMaxs ) || IsPointInBox( v2, vMins, vMaxs ) )
				{
					// This is for collision only, so we don't need to worry about blending-- Use the first surface prop.
					int nProp = pDispTree->GetSurfaceProps(0);
					physcollision->PolysoupAddTriangle( pDispCollideSoup, v0, v1, v2, nProp );
				}
 
			}// triangle loop
		}// for each displacement in leaf		
	}// for each leaf

	EndTrace( pTraceInfo );

	CPhysCollide* pCollide = physcollision->ConvertPolysoupToCollide ( pDispCollideSoup, false );

	// clean up poly soup
	physcollision->PolysoupDestroy( pDispCollideSoup );

	return pCollide;
}


//-----------------------------------------------------------------------------
// Purpose: Used to copy the mesh information of all displacement surfaces in a specified box
// Input  : vMins - min vector of the AABB
//			vMaxs - max vector of the AABB
//			pOutputMeshes - A preallocated array to store results
//			iMaxOutputMeshes - The array size of pOutputMeshes
// Output : Number of meshes written to pOutputMeshes
//-----------------------------------------------------------------------------
int CEngineTrace::GetMeshesFromDisplacementsInAABB( const Vector& vMins, const Vector& vMaxs, virtualmeshlist_t *pOutputMeshes, int iMaxOutputMeshes )
{
	int iMeshesWritten = 0;
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	int *pLeafList = (int *)stackalloc( pBSPData->numleafs * sizeof( int ) ); 
	int iLeafCount = CM_BoxLeafnums( vMins, vMaxs, pLeafList, pBSPData->numleafs, NULL );

	TraceInfo_t *pTraceInfo = BeginTrace();

	TraceCounter_t *pCounters = pTraceInfo->GetDispCounters();
	int count = pTraceInfo->GetCount();

	// For each leaf in which the box lies, Get all displacements in that leaf and use their triangles to create the mesh
	for ( int i = 0; i < iLeafCount; ++i )
	{
		// Current leaf
		cleaf_t curLeaf = pBSPData->map_leafs[ pLeafList[i] ];

		// Test box against all displacements in the leaf.
		for( int i = 0; i < curLeaf.dispCount; i++ )
		{
			int dispIndex = pBSPData->map_dispList[curLeaf.dispListStart + i];
			CDispCollTree *pDispTree = &g_pDispCollTrees[dispIndex];

			// make sure we only check this brush once per trace/stab
			if ( !pTraceInfo->Visit( pDispTree->m_iCounter, count, pCounters ) )
				continue;

			// If this displacement doesn't touch our test box, don't add it to the list.
			if ( !IsBoxIntersectingBox( vMins, vMaxs, pDispTree->m_mins, pDispTree->m_maxs) )
				continue;

			// Get the triangle mesh for this displacement surface
			pDispTree->GetVirtualMeshList( &pOutputMeshes[iMeshesWritten] );
			++iMeshesWritten;
			if( iMeshesWritten == iMaxOutputMeshes )
			{
				EndTrace( pTraceInfo );
				return iMeshesWritten;
			}
		}// for each displacement in leaf
	}// for each leaf

	EndTrace( pTraceInfo );

	return iMeshesWritten;
}

CON_COMMAND( disp_list_all_collideable, "List all collideable displacements" )
{
	int nPhysicsCollide = 0, nHullCollide = 0, nRayCollide = 0;
	ConMsg( "Displacement list:\n" );
	for ( int i = 0; i < g_DispCollTreeCount; ++ i )
	{
		CDispCollTree *pDispCollisionTree = &g_pDispCollTrees[ i ];
		int nFlags = pDispCollisionTree->GetFlags();
		Vector vMin, vMax;
		pDispCollisionTree->GetBounds( vMin, vMax );
		Vector vCenter = ( vMin + vMax ) * 0.5f;
		ConMsg( "Displacement %3d, location ( % 10.2f % 10.2f % 10.2f ), collision flags: %s %s %s\n", 
			i, vCenter.x, vCenter.y, vCenter.z,
			( nFlags & CCoreDispInfo::SURF_NOPHYSICS_COLL ) ? "   Physics" : "NO Physics",
			( nFlags & CCoreDispInfo::SURF_NOHULL_COLL ) ? "   Hull" : "NO Hull",
			( nFlags & CCoreDispInfo::SURF_NORAY_COLL ) ? "   Ray" : "NO Ray" );

		nPhysicsCollide += ( nFlags & CCoreDispInfo::SURF_NOPHYSICS_COLL ) ? 1 : 0;
		nHullCollide += ( nFlags & CCoreDispInfo::SURF_NOHULL_COLL ) ? 1 : 0;
		nRayCollide += ( nFlags & CCoreDispInfo::SURF_NORAY_COLL ) ? 1 : 0;
	}
	ConMsg( "Total displacements: %d\nCollision stats: %d with physics, %d with hull, %d with ray.\n", g_DispCollTreeCount, nPhysicsCollide, nHullCollide, nRayCollide );
}



int CEngineTrace::GetNumDisplacements( )
{
	return g_DispCollTreeCount;
}


void CEngineTrace::GetDisplacementMesh( int nIndex, virtualmeshlist_t *pMeshTriList )
{
	g_pDispCollTrees[ nIndex ].GetVirtualMeshList( pMeshTriList );
}


int CEngineTrace::GetBrushInfo( int iBrush, int &ContentsOut, BrushSideInfo_t *pBrushSideInfoOut, int iBrushSideInfoArraySize )
{
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	if( iBrush < 0 || iBrush >= pBSPData->numbrushes )
		return 0;

	cbrush_t *pBrush = &pBSPData->map_brushes[iBrush];
	ContentsOut = pBrush->contents;
	
	if ( pBrush->IsBox() )
	{
		if( !pBrushSideInfoOut || (iBrushSideInfoArraySize < 6) )
			return -6;

		cboxbrush_t *pBox = &pBSPData->map_boxbrushes[pBrush->GetBox()];
		for ( int i = 0; i < 6; i++ )
		{
			V_memset( &pBrushSideInfoOut[i].plane, 0, sizeof( pBrushSideInfoOut[i].plane ) );
			int maskIndex = i;
			if ( i < 3 )
			{
				pBrushSideInfoOut[i].plane.normal[i] = 1.0f;
				pBrushSideInfoOut[i].plane.dist = pBox->maxs[i];
				maskIndex += 3;
			}
			else
			{
				pBrushSideInfoOut[i].plane.normal[i-3] = -1.0f;
				pBrushSideInfoOut[i].plane.dist = -pBox->mins[i-3];
			}
			pBrushSideInfoOut[i].bevel = 0;
			pBrushSideInfoOut[i].thin = ( pBox->thinMask & (1 << maskIndex) ) ? 1 : 0;
		}
		return 6;
	}
	else
	{
		if( !pBrushSideInfoOut || (iBrushSideInfoArraySize < pBrush->numsides) )
			return -pBrush->numsides;

		cbrushside_t *stopside = &pBSPData->map_brushsides[pBrush->firstbrushside];
		// Note:  Don't do this in the [] since the final one on the last brushside will be past the end of the array end by one index
		stopside += pBrush->numsides;
		for( cbrushside_t *side = &pBSPData->map_brushsides[pBrush->firstbrushside]; side != stopside; ++side )
		{
			pBrushSideInfoOut->plane = *side->plane;
			pBrushSideInfoOut->bevel = side->bBevel;
			pBrushSideInfoOut->thin = side->bThin;
			++pBrushSideInfoOut;
		}
		return pBrush->numsides;
	}
}

//Tests a point to see if it's outside any playable area
bool CEngineTrace::PointOutsideWorld( const Vector &ptTest )
{
	int iLeaf = CM_PointLeafnum( ptTest );
	Assert( iLeaf >= 0 );

	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	if( pBSPData->map_leafs[iLeaf].cluster == -1 )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Expose to the game dll a method for finding the leaf which contains a given point
// Input  : &vPos - Returns the leaf which contains this point
// Output : int - The handle to the leaf
//-----------------------------------------------------------------------------
int CEngineTrace::GetLeafContainingPoint( const Vector &vPos )
{
	return CM_PointLeafnum( vPos );
}



//-----------------------------------------------------------------------------
// Convex info for studio + brush models
//-----------------------------------------------------------------------------
class CBrushConvexInfo : public IConvexInfo
{
public:
	CBrushConvexInfo()
	{
		m_pBSPData = GetCollisionBSPData();
	}
	virtual unsigned int GetContents( int convexGameData )
	{
		return m_pBSPData->map_brushes[convexGameData].contents;
	}

private:
	CCollisionBSPData *m_pBSPData;
};

class CStudioConvexInfo : public IConvexInfo
{
public:
	CStudioConvexInfo( studiohdr_t *pStudioHdr )
	{
		m_pStudioHdr = pStudioHdr;
	}

	virtual unsigned int GetContents( int convexGameData )
	{
		if ( convexGameData == 0 )
		{
			return m_pStudioHdr->contents;
		}

		Assert( convexGameData <= m_pStudioHdr->numbones );
		const mstudiobone_t *pBone = m_pStudioHdr->pBone(convexGameData - 1);
		return pBone->contents;
	}

private:
	studiohdr_t *m_pStudioHdr;
};


//-----------------------------------------------------------------------------
// Perform vphysics trace
//-----------------------------------------------------------------------------
bool CEngineTrace::ClipRayToVPhysics( const Ray_t &ray, unsigned int fMask, ICollideable *pEntity, studiohdr_t *pStudioHdr, trace_t *pTrace )
{
	if ( pEntity->GetSolid() != SOLID_VPHYSICS )
		return false;

	bool bTraced = false;

	// use the vphysics model for rotated brushes and vphysics simulated objects
	const model_t *pModel = pEntity->GetCollisionModel();
	if( !pModel )
	{
		IPhysicsObject *pPhysics = pEntity->GetVPhysicsObject();
		if ( pPhysics )
		{
			const CPhysCollide *pSolid = pPhysics->GetCollide();
			if ( pSolid )
			{
				physcollision->TraceBox( 
					ray,
					fMask,
					NULL,
					pSolid,
					pEntity->GetCollisionOrigin(), 
					pEntity->GetCollisionAngles(), 
					pTrace );
				return true;
			}
		}
		Vector vecMins = pEntity->OBBMins( ), vecMaxs = pEntity->OBBMaxs();
		Warning("CEngineTrace::ClipRayToVPhysics : no model; bbox {%g,%g,%g}-{%g,%g,%g}\n", vecMins.x,vecMins.y,vecMins.z, vecMaxs.x,vecMaxs.y,vecMaxs.z) ;
		return false;
	}						  


	if ( pStudioHdr )
	{
		CStudioConvexInfo studioConvex( pStudioHdr );
		vcollide_t *pCollide = g_pMDLCache->GetVCollide( pModel->studio );
		if ( pCollide && pCollide->solidCount )
		{
			physcollision->TraceBox( 
				ray,
				fMask,
				&studioConvex,
				pCollide->solids[0], // UNDONE: Support other solid indices?!?!?!? (forced zero)
				pEntity->GetCollisionOrigin(), 
				pEntity->GetCollisionAngles(), 
				pTrace );
			bTraced = true;
		}
	}
	else
	{
		Assert(pModel->type != mod_studio);
		// use the regular code for raytraces against brushes
		// do ray traces with normal code, but use vphysics to do box traces
		if ( !ray.m_IsRay || pModel->type != mod_brush )
		{
			int nModelIndex = pEntity->GetCollisionModelIndex();

			// BUGBUG: This only works when the vcollide in question is the first solid in the model
			vcollide_t *pCollide = CM_VCollideForModel( nModelIndex, (model_t*)pModel );

			if ( pCollide && pCollide->solidCount )
			{
				CBrushConvexInfo brushConvex;

				IConvexInfo *pConvexInfo = (pModel->type) == mod_brush ? &brushConvex : NULL;
				physcollision->TraceBox( 
					ray,
					fMask,
					pConvexInfo,
					pCollide->solids[0], // UNDONE: Support other solid indices?!?!?!? (forced zero)
					pEntity->GetCollisionOrigin(), 
					pEntity->GetCollisionAngles(), 
					pTrace );
				bTraced = true;
			}
		}
	}

	return bTraced;
}


//-----------------------------------------------------------------------------
// Perform bsp trace
//-----------------------------------------------------------------------------
bool CEngineTrace::ClipRayToBSP( const Ray_t &ray, unsigned int fMask, ICollideable *pEntity, trace_t *pTrace )
{
	int nModelIndex = pEntity->GetCollisionModelIndex();
	cmodel_t *pCModel = CM_InlineModelNumber( nModelIndex - 1 );
	int nHeadNode = pCModel->headnode;

	CM_TransformedBoxTrace( ray, nHeadNode, fMask, pEntity->GetCollisionOrigin(), pEntity->GetCollisionAngles(), *pTrace );
	return true;
}


// NOTE: Switched over to SIMD ray/box test since there is a bug we haven't hunted down yet in the scalar version
bool CEngineTrace::ClipRayToBBox( const Ray_t &ray, unsigned int fMask, ICollideable *pEntity, trace_t *pTrace )
{
	extern bool IntersectRayWithBox( const Ray_t &ray, const VectorAligned &inInvDelta, const VectorAligned &inBoxMins, const VectorAligned &inBoxMaxs, trace_t *RESTRICT pTrace );

	if ( pEntity->GetSolid() != SOLID_BBOX )
		return false;

	// We can't use the OBBMins/Maxs unless the collision angles are world-aligned
	Assert( pEntity->GetCollisionAngles() == vec3_angle );

	VectorAligned vecAbsMins, vecAbsMaxs;
	VectorAligned vecInvDelta;
	// NOTE: If ray.m_pWorldAxisTransform is set, then the boxes should be rotated into the root parent's space
	if ( !ray.m_IsRay && ray.m_pWorldAxisTransform )
	{
		Ray_t ray_l;

		ray_l.m_Extents = ray.m_Extents;

		VectorIRotate( ray.m_Delta, *ray.m_pWorldAxisTransform, ray_l.m_Delta );
		ray_l.m_StartOffset.Init();
		VectorITransform( ray.m_Start, *ray.m_pWorldAxisTransform, ray_l.m_Start );

		vecInvDelta = ray_l.InvDelta();
		Vector localEntityOrigin;
		VectorITransform( pEntity->GetCollisionOrigin(), *ray.m_pWorldAxisTransform, localEntityOrigin );
		ray_l.m_IsRay = ray.m_IsRay;
		ray_l.m_IsSwept = ray.m_IsSwept;

		VectorAdd( localEntityOrigin,  pEntity->OBBMins(), vecAbsMins );
		VectorAdd( localEntityOrigin, pEntity->OBBMaxs(), vecAbsMaxs );
		IntersectRayWithBox( ray_l, vecInvDelta, vecAbsMins, vecAbsMaxs, pTrace );

		if ( pTrace->DidHit() )
		{
			Vector temp;
			VectorCopy (pTrace->plane.normal, temp);
			VectorRotate( temp, *ray.m_pWorldAxisTransform, pTrace->plane.normal );
			VectorAdd( ray.m_Start, ray.m_StartOffset, pTrace->startpos );

			if (pTrace->fraction == 1)
			{
				VectorAdd( pTrace->startpos, ray.m_Delta, pTrace->endpos);
			}
			else
			{
				VectorMA( pTrace->startpos, pTrace->fraction, ray.m_Delta, pTrace->endpos );
			}
			pTrace->plane.dist = DotProduct( pTrace->endpos, pTrace->plane.normal );
			if ( pTrace->fractionleftsolid < 1 )
			{
				pTrace->startpos += ray.m_Delta * pTrace->fractionleftsolid;
			}
		}
		else
		{
			VectorAdd( ray.m_Start, ray.m_StartOffset, pTrace->startpos );
		}

		return true;
	}

	vecInvDelta = ray.InvDelta();
	VectorAdd( pEntity->GetCollisionOrigin(), pEntity->OBBMins(), vecAbsMins );
	VectorAdd( pEntity->GetCollisionOrigin(), pEntity->OBBMaxs(), vecAbsMaxs );
	IntersectRayWithBox( ray, vecInvDelta, vecAbsMins, vecAbsMaxs, pTrace); 
	return true;
}

bool CEngineTrace::ClipRayToOBB( const Ray_t &ray, unsigned int fMask, ICollideable *pEntity, trace_t *pTrace )
{
	if ( pEntity->GetSolid() != SOLID_OBB )
		return false;

	// NOTE: This is busted because it doesn't compute fractionleftsolid, which at the
	// moment is required for the engine trace system.
	IntersectRayWithOBB( ray, pEntity->GetCollisionOrigin(), pEntity->GetCollisionAngles(), 
		pEntity->OBBMins(), pEntity->OBBMaxs(), DIST_EPSILON, pTrace );
	return true;
}


//-----------------------------------------------------------------------------
// Main entry point for clipping rays to entities
//-----------------------------------------------------------------------------
#ifndef DEDICATED
void CEngineTraceClient::SetTraceEntity( ICollideable *pCollideable, trace_t *pTrace )
{
	if ( !pTrace->DidHit() )
		return;

	// FIXME: This is only necessary because of traces occurring during
	// LevelInit (a suspect time to be tracing)
	if (!pCollideable)
	{
		pTrace->m_pEnt = NULL;
		return;
	}

	IClientUnknown *pUnk = (IClientUnknown*)pCollideable->GetEntityHandle();
	if ( !StaticPropMgr()->IsStaticProp( pUnk ) )
	{
		pTrace->m_pEnt = (CBaseEntity*)(pUnk->GetIClientEntity());
	}
	else
	{
		// For static props, point to the world, hitbox is the prop index
		pTrace->m_pEnt = (CBaseEntity*)(entitylist->GetClientEntity(0));
		pTrace->hitbox = StaticPropMgr()->GetStaticPropIndex( pUnk ) + 1;
	}
}
#endif

void CEngineTraceServer::SetTraceEntity( ICollideable *pCollideable, trace_t *pTrace )
{
	if ( !pTrace->DidHit() )
		return;

	IHandleEntity *pHandleEntity = pCollideable->GetEntityHandle();
	if ( !StaticPropMgr()->IsStaticProp( pHandleEntity ) )
	{
		pTrace->m_pEnt = (CBaseEntity*)(pHandleEntity);
	}
	else
	{
		// For static props, point to the world, hitbox is the prop index
		pTrace->m_pEnt = (CBaseEntity*)(sv.edicts->GetIServerEntity());
		pTrace->hitbox = StaticPropMgr()->GetStaticPropIndex( pHandleEntity ) + 1;
	}
}


//-----------------------------------------------------------------------------
// Traces a ray against a particular edict
//-----------------------------------------------------------------------------
void CEngineTrace::ClipRayToCollideable( const Ray_t &ray, unsigned int fMask, ICollideable *pEntity, trace_t *pTrace )
{
	CM_ClearTrace( pTrace );
	VectorAdd( ray.m_Start, ray.m_StartOffset, pTrace->startpos );
	VectorAdd( pTrace->startpos, ray.m_Delta, pTrace->endpos );

	const model_t *pModel = pEntity->GetCollisionModel();
	bool bIsStudioModel = false;
	studiohdr_t *pStudioHdr = NULL;
	if ( pModel && pModel->type == mod_studio )
	{
		bIsStudioModel = true;
		pStudioHdr = (studiohdr_t *)modelloader->GetExtraData( (model_t*)pModel );
		// Cull if the collision mask isn't set + we're not testing hitboxes.
		if ( (( fMask & CONTENTS_HITBOX ) == 0) )
		{
			if ( ( fMask & pStudioHdr->contents ) == 0)
				return;
		}
	}

	const matrix3x4_t *pOldTransform = ray.m_pWorldAxisTransform;
	if ( pEntity->GetSolidFlags() & FSOLID_ROOT_PARENT_ALIGNED )
	{
		const_cast<Ray_t &>(ray).m_pWorldAxisTransform = pEntity->GetRootParentToWorldTransform();
	}
	bool bTraced = false;
	bool bCustomPerformed = false;
	if ( ShouldPerformCustomRayTest( ray, pEntity ) )
	{
		ClipRayToCustom( ray, fMask, pEntity, pTrace );
		bTraced = true;
		bCustomPerformed = true;
	}
	else
	{
		bTraced = ClipRayToVPhysics( ray, fMask, pEntity, pStudioHdr, pTrace );	
	}

	// FIXME: Why aren't we using solid type to check what kind of collisions to test against?!?!
	if ( !bTraced && pModel && pModel->type == mod_brush )
	{
		bTraced = ClipRayToBSP( ray, fMask, pEntity, pTrace );
	}

	if ( !bTraced )
	{
		bTraced = ClipRayToOBB( ray, fMask, pEntity, pTrace );
	}

	// Hitboxes..
	bool bTracedHitboxes = false;
	if ( bIsStudioModel && (fMask & CONTENTS_HITBOX) )
	{
		// Until hitboxes are no longer implemented as custom raytests,
		// don't bother to do the work twice
		if (!bCustomPerformed)
		{
			bTraced = ClipRayToHitboxes( ray, fMask, pEntity, pTrace );
			if ( bTraced )
			{
				// Hitboxes will set the surface properties
				bTracedHitboxes = true;
			}
		}
	}

	if ( !bTraced )
	{
		ClipRayToBBox( ray, fMask, pEntity, pTrace );
	}

	if ( bIsStudioModel && !bTracedHitboxes && pTrace->DidHit() && (!bCustomPerformed || pTrace->surface.surfaceProps == 0) )
	{
		pTrace->contents = pStudioHdr->contents;
		// use the default surface properties
		pTrace->surface.name = "**studio**";
		pTrace->surface.flags = 0;
		pTrace->surface.surfaceProps = pStudioHdr->GetSurfaceProp();
	}

	if (!pTrace->m_pEnt && pTrace->DidHit())
	{
		SetTraceEntity( pEntity, pTrace );
	}

#ifdef _DEBUG
	Vector vecOffset, vecEndTest;
	VectorAdd( ray.m_Start, ray.m_StartOffset, vecOffset );
	VectorMA( vecOffset, pTrace->fractionleftsolid, ray.m_Delta, vecEndTest );
	// <sergiy> changing this from absolute to relative error, because the vector lengths are often over 1000
	Assert( ( vecEndTest - pTrace->startpos ).Length() < 0.01f + 0.001f * vecEndTest.Length() + pTrace->startpos.Length( ) ) ;
	VectorMA( vecOffset, pTrace->fraction, ray.m_Delta, vecEndTest );
	Assert( ( vecEndTest - pTrace->endpos ).Length() < 0.01f + 0.001f * vecEndTest.Length() + pTrace->endpos.Length( ) ) ;
#endif
	const_cast<Ray_t &>(ray).m_pWorldAxisTransform = pOldTransform;
}


//-----------------------------------------------------------------------------
// Main entry point for clipping rays to entities
//-----------------------------------------------------------------------------
void CEngineTrace::ClipRayToEntity( const Ray_t &ray, unsigned int fMask, IHandleEntity *pEntity, trace_t *pTrace )
{
	ClipRayToCollideable( ray, fMask, GetCollideable(pEntity), pTrace );
}


//-----------------------------------------------------------------------------
// Grabs all entities along a ray
//-----------------------------------------------------------------------------
class CEntitiesAlongRay : public IPartitionEnumerator
{
public:
	CEntitiesAlongRay( ) : m_EntityHandles(0, 32) {}

	void Reset()
	{
		m_EntityHandles.RemoveAll();
	}

	IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
		m_EntityHandles.AddToTail( pHandleEntity );
		return ITERATION_CONTINUE;
	}

	CUtlVector< IHandleEntity * >	m_EntityHandles;
};

class CEntityListAlongRay : public IPartitionEnumerator
{
public:

	enum { MAX_ENTITIES_ALONGRAY = 1024 };

	CEntityListAlongRay() 
	{
		m_nCount = 0;
	}

	void Reset()
	{
		m_nCount = 0;
	}

	int Count()
	{
		return m_nCount;
	}

	IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
		if ( m_nCount < MAX_ENTITIES_ALONGRAY )
		{
			m_EntityHandles[m_nCount] = pHandleEntity;
			m_nCount++;
		}
		else
		{
			DevMsg( 1, "Max entity count along ray exceeded!\n" );
		}

		return ITERATION_CONTINUE;
	}

	int m_nCount;
	IHandleEntity	*m_EntityHandles[MAX_ENTITIES_ALONGRAY];
};

//-----------------------------------------------------------------------------
// Makes sure the final trace is clipped to the clip trace
// Returns true if clipping occurred
//-----------------------------------------------------------------------------
bool CEngineTrace::ClipTraceToTrace( trace_t &clipTrace, trace_t *pFinalTrace )
{
	if (clipTrace.allsolid || clipTrace.startsolid || (clipTrace.fraction < pFinalTrace->fraction))
	{
		if (pFinalTrace->startsolid)
		{
			float flFractionLeftSolid = pFinalTrace->fractionleftsolid;
			Vector vecStartPos = pFinalTrace->startpos;

			*pFinalTrace = clipTrace;
			pFinalTrace->startsolid = true;

			if ( flFractionLeftSolid > clipTrace.fractionleftsolid )
			{
				pFinalTrace->fractionleftsolid = flFractionLeftSolid;
				pFinalTrace->startpos = vecStartPos;
			}
		}
		else
		{
			*pFinalTrace = clipTrace;
		}
		return true;
	}

	if (clipTrace.startsolid)
	{
		pFinalTrace->startsolid = true;

		if ( clipTrace.fractionleftsolid > pFinalTrace->fractionleftsolid )
		{
			pFinalTrace->fractionleftsolid = clipTrace.fractionleftsolid;
			pFinalTrace->startpos = clipTrace.startpos;
		}
	}
	return false;
}

inline bool ShouldTestStaticProp( IHandleEntity *pHandleEntity )
{
#if defined( _GAMECONSOLE )
	return pHandleEntity->m_bIsStaticProp;
#else
	return true;
#endif
}

//-----------------------------------------------------------------------------
// Converts a user id to a collideable + username
//-----------------------------------------------------------------------------
ICollideable *CEngineTraceServer::HandleEntityToCollideable( IHandleEntity *pHandleEntity )
{
	ICollideable *pCollideable = NULL;
	if ( ShouldTestStaticProp( pHandleEntity ) )
	{
		pCollideable = StaticPropMgr()->GetStaticProp( pHandleEntity );
		if ( pCollideable )
			return pCollideable;
	}

	IServerUnknown *pServerUnknown = static_cast<IServerUnknown*>(pHandleEntity);
	if ( pServerUnknown )
	{
		pCollideable = pServerUnknown->GetCollideable();
	}
	return pCollideable;
}

const char *CEngineTraceServer::GetDebugName( IHandleEntity *pHandleEntity )
{
	if ( ShouldTestStaticProp( pHandleEntity ) && StaticPropMgr()->IsStaticProp(pHandleEntity) )
		return "static prop";

	IServerUnknown *pServerUnknown = static_cast<IServerUnknown*>(pHandleEntity);
	if ( !pServerUnknown || !pServerUnknown->GetNetworkable())
		return "<null>";

	return pServerUnknown->GetNetworkable()->GetClassName();
}

#ifndef DEDICATED
ICollideable *CEngineTraceClient::HandleEntityToCollideable( IHandleEntity *pHandleEntity )
{
	ICollideable *pCollideable = NULL;
	if ( ShouldTestStaticProp( pHandleEntity ) )
	{
		pCollideable = StaticPropMgr()->GetStaticProp( pHandleEntity );
		if ( pCollideable )
			return pCollideable;
	}
	IClientUnknown *pUnk = static_cast<IClientUnknown*>(pHandleEntity);
	if ( pUnk )
	{
		pCollideable = pUnk->GetCollideable();
	}

	return pCollideable;
}

const char *CEngineTraceClient::GetDebugName( IHandleEntity *pHandleEntity )
{
	if ( ShouldTestStaticProp( pHandleEntity ) && StaticPropMgr()->IsStaticProp(pHandleEntity) )
		return "static prop";

	IClientUnknown *pUnk = static_cast<IClientUnknown*>(pHandleEntity);
	if ( !pUnk )
		return "<null>";

	IClientNetworkable *pNetwork = pUnk->GetClientNetworkable();
	if (pNetwork && pNetwork->GetClientClass() )
		return pNetwork->GetClientClass()->m_pNetworkName;
	return "client entity";
}
#endif

//-----------------------------------------------------------------------------
// Returns the world collideable for trace setting
//-----------------------------------------------------------------------------
#ifndef DEDICATED
ICollideable *CEngineTraceClient::GetWorldCollideable()
{
	IClientEntity *pUnk = entitylist->GetClientEntity( 0 );
	AssertOnce( pUnk );
	return pUnk ? pUnk->GetCollideable() : NULL;
}
#endif

ICollideable *CEngineTraceServer::GetWorldCollideable()
{
	if (!sv.edicts)
		return NULL;
	return sv.edicts->GetCollideable();
}


//-----------------------------------------------------------------------------
// Debugging code to render all ray casts since the last time this call was made
//-----------------------------------------------------------------------------
void EngineTraceRenderRayCasts()
{
#if defined _DEBUG && !defined DEDICATED
	if( debugrayenable.GetBool() && s_FrameRays.Count() > debugraylimit.GetInt() && !debugrayreset.GetInt() )
	{
		Warning( "m_FrameRays.Count() == %d\n", s_FrameRays.Count() );
		debugrayreset.SetValue( 1 );
		int i;
		for( i = 0; i < s_FrameRays.Count(); i++ )
		{
			Ray_t &ray = s_FrameRays[i];
			if( ray.m_Extents.x != 0.0f || ray.m_Extents.y != 0.0f || ray.m_Extents.z != 0.0f )
			{
				CDebugOverlay::AddLineOverlay( ray.m_Start, ray.m_Start + ray.m_Delta, 255, 0, 0, 255, true, 3600.0f );
			}
			else
			{
				CDebugOverlay::AddLineOverlay( ray.m_Start, ray.m_Start + ray.m_Delta, 255, 255, 0, 255, true, 3600.0f );
			}
		}
	}

	s_FrameRays.RemoveAll( );
#endif
}

static void ComputeRayBounds( const Ray_t &ray, Vector &mins, Vector &maxs )
{
	if ( ray.m_IsRay )
	{
		Vector start = ray.m_Start;
		for ( int i = 0; i < 3; i++ )
		{
			if ( ray.m_Delta[i] > 0 )
			{
				maxs[i] = start[i] + ray.m_Delta[i];
				mins[i] = start[i];
			}
			else
			{
				maxs[i] = start[i];
				mins[i] = start[i] + ray.m_Delta[i];
			}
		}
	}
	else
	{
		Vector start = ray.m_Start;
		for ( int i = 0; i < 3; i++ )
		{
			if ( ray.m_Delta[i] > 0 )
			{
				maxs[i] = start[i] + ray.m_Delta[i] + ray.m_Extents[i];
				mins[i] = start[i] - ray.m_Extents[i];
			}
			else
			{
				maxs[i] = start[i] + ray.m_Extents[i];
				mins[i] = start[i] + ray.m_Delta[i] - ray.m_Extents[i];
			}
		}
	}
}

static bool IsBoxWithinBounds( const Vector &boxMins, const Vector &boxMaxs, const Vector &boundsMins, const Vector &bounsMaxs )
{
	if ( boxMaxs.x <= bounsMaxs.x && boxMins.x >= boundsMins.x &&
		boxMaxs.y <= bounsMaxs.y && boxMins.y >= boundsMins.y &&
		boxMaxs.z <= bounsMaxs.z && boxMins.z >= boundsMins.z )
		return true;
	return false;
}


bool CTraceListData::CanTraceRay( const Ray_t &ray )
{
	Vector rayMins, rayMaxs;
	ComputeRayBounds( ray, rayMins, rayMaxs );
	return IsBoxWithinBounds( rayMins, rayMaxs, m_mins, m_maxs );
}

// implementing members of CTraceListData
IterationRetval_t CTraceListData::EnumElement( IHandleEntity *pHandleEntity )
{
	ICollideable *pCollideable = m_pEngineTrace->HandleEntityToCollideable( pHandleEntity );
	// Check for error condition.
	if ( !IsSolid( pCollideable->GetSolid(), pCollideable->GetSolidFlags() ) )
	{
		Assert( 0 );
		if ( pCollideable->GetCollisionModel() )
		{
			Msg("%s in solid list (not solid) (%d, %04X) %.*s\n", m_pEngineTrace->GetDebugName(pHandleEntity), pCollideable->GetSolid(), pCollideable->GetSolidFlags(),
				sizeof( pCollideable->GetCollisionModel()->szPathName ), pCollideable->GetCollisionModel()->szPathName );
		}
		else
		{
			Msg("%s in solid list (not solid) (%d, %04X)\n", m_pEngineTrace->GetDebugName(pHandleEntity), pCollideable->GetSolid(), pCollideable->GetSolidFlags() );
		}
	}
	else
	{
		if ( StaticPropMgr()->IsStaticProp( pHandleEntity ) )
		{
			int index = m_staticPropList.AddToTail();
			m_staticPropList[index].pCollideable = pCollideable;
			m_staticPropList[index].pEntity = pHandleEntity;
		}
		else
		{
			int index = m_entityList.AddToTail();
			m_entityList[index].pCollideable = pCollideable;
			m_entityList[index].pEntity = pHandleEntity;
		}
	}

	return ITERATION_CONTINUE;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEngineTrace::SetupLeafAndEntityListRay( const Ray_t &ray, ITraceListData *pTraceData )
{
	Vector mins, maxs;
	ComputeRayBounds( ray, mins, maxs );
	SetupLeafAndEntityListBox( mins, maxs, pTraceData );
}

//-----------------------------------------------------------------------------
// Purpose: Gives an AABB and returns a leaf and entity list.
//-----------------------------------------------------------------------------
void CEngineTrace::SetupLeafAndEntityListBox( const Vector &vecBoxMin, const Vector &vecBoxMax, ITraceListData *pTraceData )
{
	VPROF("SetupLeafAndEntityListBox");
	CTraceListData &traceData = *static_cast<CTraceListData *>(pTraceData);
	traceData.Reset();
	traceData.m_pEngineTrace = this;
	// increase bounds slightly to catch exact cases
	for ( int i = 0; i < 3; i++ )
	{
		traceData.m_mins[i] = vecBoxMin[i] - 1;
		traceData.m_maxs[i] = vecBoxMax[i] + 1;
	}
	// Get the leaves that intersect this box.
	CM_GetTraceDataForBSP( traceData.m_mins, traceData.m_maxs, traceData );
	// Find all entities in the voxels that intersect this box.
	SpatialPartition()->EnumerateElementsInBox( SpatialPartitionMask(), traceData.m_mins, traceData.m_maxs, false, &traceData );
}



//-----------------------------------------------------------------------------
// Purpose:
// NOTE: the fMask is redundant with the stuff below, what do I want to do???
//-----------------------------------------------------------------------------
void CEngineTrace::TraceRayAgainstLeafAndEntityList( const Ray_t &ray, ITraceListData *pTraceData,
										             unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace )
{
	VPROF("TraceRayAgainstLeafAndEntityList");
	CTraceListData &traceData = *static_cast<CTraceListData *>(pTraceData);
	Vector rayMins, rayMaxs;
	ComputeRayBounds( ray, rayMins, rayMaxs );
	if ( !IsBoxWithinBounds( rayMins, rayMaxs, traceData.m_mins, traceData.m_maxs ) )
	{
		TraceRay( ray, fMask, pTraceFilter, pTrace );
		return;
	}
	// Make sure we have some kind of trace filter.
	CTraceFilterHitAll traceFilter;
	if ( !pTraceFilter )
	{
		pTraceFilter = &traceFilter;
	}

	// Collide with the world.
	if ( pTraceFilter->GetTraceType() != TRACE_ENTITIES_ONLY )
	{
		ICollideable *pCollide = GetWorldCollideable();

		CM_BoxTraceAgainstLeafList( ray, traceData, fMask, *pTrace );
		SetTraceEntity( pCollide, pTrace );

		// Blocked by the world or early out because we only are tracing against the world.
		if ( ( pTrace->fraction == 0 ) || ( pTraceFilter->GetTraceType() == TRACE_WORLD_ONLY ) )
			return;
	}
	else
	{
		// Setup the trace data.
		CM_ClearTrace ( pTrace );

		// Set initial start and endpos.  This is necessary if the world isn't traced against,
		// because we may not trace against anything below.
		VectorAdd( ray.m_Start, ray.m_StartOffset, pTrace->startpos );
		VectorAdd( pTrace->startpos, ray.m_Delta, pTrace->endpos );
	}
	// Save the world collision fraction.
	float flWorldFraction = pTrace->fraction;
	float flWorldFractionLeftSolidScale = flWorldFraction;

	// Create a ray that extends only until we hit the world
	// and adjust the trace accordingly
	Ray_t entityRay = ray;

	if ( pTrace->fraction == 0 )
	{
		entityRay.m_Delta.Init();
		flWorldFractionLeftSolidScale = pTrace->fractionleftsolid;
		pTrace->fractionleftsolid = 1.0f;
		pTrace->fraction = 1.0f;
	}
	else
	{
		// Explicitly compute end so that this computation happens at the quantization of
		// the output (endpos).  That way we won't miss any intersections we would get
		// by feeding these results back in to the tracer
		// This is not the same as entityRay.m_Delta *= pTrace->fraction which happens 
		// at a quantization that is more precise as m_Start moves away from the origin
		Vector end;
		VectorMA( entityRay.m_Start, pTrace->fraction, entityRay.m_Delta, end );
		VectorSubtract(end, entityRay.m_Start, entityRay.m_Delta);
		// We know this is safe because pTrace->fraction != 0
		pTrace->fractionleftsolid /= pTrace->fraction;
		pTrace->fraction = 1.0;
	}

	// Collide with entities.
	bool bNoStaticProps = pTraceFilter->GetTraceType() == TRACE_ENTITIES_ONLY;
	bool bFilterStaticProps = pTraceFilter->GetTraceType() == TRACE_EVERYTHING_FILTER_PROPS;

	trace_t trace;
	Vector mins, maxs;
	if ( !bNoStaticProps )
	{
		int propCount = traceData.m_staticPropList.Count();
		for ( int iProp = 0; iProp < propCount && !pTrace->allsolid; iProp++ )
		{
			IHandleEntity *pHandleEntity = traceData.m_staticPropList[iProp].pEntity;
			ICollideable *pCollideable = traceData.m_staticPropList[iProp].pCollideable;
			if ( bFilterStaticProps )
			{
				if ( !pTraceFilter->ShouldHitEntity( pHandleEntity, fMask ) )
					continue;
			}
			pCollideable->WorldSpaceSurroundingBounds( &mins, &maxs );
			if ( !IsBoxIntersectingRay( mins, maxs, entityRay, DIST_EPSILON ) )
				continue;
			ClipRayToCollideable( entityRay, fMask, pCollideable, &trace );

			// Make sure the ray is always shorter than it currently is
			ClipTraceToTrace( trace, pTrace );
		}
	}
	int entityCount = traceData.m_entityList.Count();
	for ( int iEntity = 0; iEntity < entityCount && !pTrace->allsolid; ++iEntity )
	{
		IHandleEntity *pHandleEntity = traceData.m_entityList[iEntity].pEntity;
		ICollideable *pCollideable = traceData.m_entityList[iEntity].pCollideable;
		if ( !pTraceFilter->ShouldHitEntity( pHandleEntity, fMask ) )
			continue;

		pCollideable->WorldSpaceSurroundingBounds( &mins, &maxs );
		if ( !IsBoxIntersectingRay( mins, maxs, entityRay, DIST_EPSILON ) )
			continue;
		ClipRayToCollideable( entityRay, fMask, pCollideable, &trace );

		// Make sure the ray is always shorter than it currently is
		ClipTraceToTrace( trace, pTrace );
	}

	// Fix up the fractions so they are appropriate given the original unclipped-to-world ray.
	pTrace->fraction *= flWorldFraction;
	pTrace->fractionleftsolid *= flWorldFraction;

	if ( !ray.m_IsRay )
	{
		// Make sure no fractionleftsolid can be used with box sweeps
		VectorAdd( ray.m_Start, ray.m_StartOffset, pTrace->startpos );
		pTrace->fractionleftsolid = 0;

#ifdef _DEBUG
		pTrace->fractionleftsolid = VEC_T_NAN;
#endif
	}
}

#if BENCHMARK_RAY_TEST

ConVar ray_count_max("ray_count_max","8");
ConVar ray_batch_extents("ray_batch_extents","96");
ConVar ray_batch_iterations("ray_batch_iterations","20");
CON_COMMAND( ray_save, "Save the rays" )
{
	int count = s_BenchmarkRays.Count();
	if ( count )
	{
		FileHandle_t hFile = g_pFileSystem->Open("rays.bin", "wb");
		if ( hFile )
		{
			g_pFileSystem->Write( &count, sizeof(count), hFile );
			g_pFileSystem->Write( s_BenchmarkRays.Base(), sizeof(s_BenchmarkRays[0])*count, hFile );
			g_pFileSystem->Close( hFile );
		}
	}

	Msg("Saved %d rays\n", count );
}

CON_COMMAND( ray_load, "Load the rays" )
{
	s_BenchmarkRays.RemoveAll();
	FileHandle_t hFile = g_pFileSystem->Open("rays.bin", "rb");
	if ( hFile )
	{
		int count = 0;
		g_pFileSystem->Read( &count, sizeof(count), hFile );
		if ( count )
		{
			s_BenchmarkRays.EnsureCount( count );
			g_pFileSystem->Read( s_BenchmarkRays.Base(), sizeof(s_BenchmarkRays[0])*count, hFile );
		}
		g_pFileSystem->Close( hFile );
	}

	Msg("Loaded %d rays\n", s_BenchmarkRays.Count() );
}

CON_COMMAND( ray_clear, "Clear the current rays" )
{
	s_BenchmarkRays.RemoveAll();
	Msg("Reset rays!\n");
}


struct ray_batch_t
{
	Vector mins;
	Vector maxs;
	int start;
	int count;
};

CON_COMMAND_EXTERN( ray_batch_bench, RayBatchBench, "Time batches of rays" )
{
	const int MAX_RAY_BATCHES = 1024;
	ray_batch_t batches[MAX_RAY_BATCHES];
	int batchCount = 0;
	for ( int i = 0; i < s_BenchmarkRays.Count(); i++ )
	{
		if ( !s_BenchmarkRays[i].m_IsRay )
		{
			int count = 0;
			Vector mins, maxs;
			ClearBounds(mins, maxs);
			for ( int j = i; j < s_BenchmarkRays.Count(); j++ )
			{
				if ( s_BenchmarkRays[j].m_IsRay )
					break;
				Vector tmpMins, tmpMaxs;
				ComputeRayBounds( s_BenchmarkRays[j], tmpMins, tmpMaxs );
				AddPointToBounds( tmpMins, mins, maxs );
				AddPointToBounds( tmpMaxs, mins, maxs );
				Vector ext = maxs - mins;
				float maxSize = MAX(ext[0], ext[1]);
				maxSize = MAX(maxSize, ext[2]);
				if ( maxSize > ray_batch_extents.GetFloat() )
					break;
				count++;
				if ( count >= ray_count_max.GetInt() )
					break;
			}
			if ( count >= ray_count_max.GetInt() && batchCount < MAX_RAY_BATCHES )
			{
				batches[batchCount].count = count;
				batches[batchCount].start = i;
				batches[batchCount].mins = mins;
				batches[batchCount].maxs = maxs;
				batchCount++;
			}
		}
	}

	Msg("Testing %d batches of %d\n", batchCount, ray_count_max.GetInt() );

	const int ITERATION_COUNT = ray_batch_iterations.GetInt();
	float normalTime = 1;
	// normal trace test
	if ( 1 )
	{
#if VPROF_LEVEL > 0 
		g_VProfCurrentProfile.Start();
		g_VProfCurrentProfile.Reset();
		g_VProfCurrentProfile.ResetPeaks();
#endif
		double tStart = Plat_FloatTime();
		trace_t trace;
		for ( int jj = 0; jj < ITERATION_COUNT; jj++ )
		{
			for ( int kk = 0; kk < batchCount; kk++)
			{
				int batchEnd = batches[kk].start + batches[kk].count;

				for ( int i = batches[kk].start; i < batchEnd; i++ )
				{
					CM_BoxTrace( s_BenchmarkRays[i], 0, MASK_SOLID, true, trace );
					if ( 1 )
					{
						// Create a ray that extends only until we hit the world and adjust the trace accordingly
						Ray_t entityRay = s_BenchmarkRays[i];
						VectorScale( entityRay.m_Delta, trace.fraction, entityRay.m_Delta );
						CEntityListAlongRay enumerator;
						enumerator.Reset();
						SpatialPartition()->EnumerateElementsAlongRay( PARTITION_ENGINE_SOLID_EDICTS, entityRay, false, &enumerator );
						trace_t tr;
						ICollideable *pCollideable;
						int nCount = enumerator.Count();
						//float flWorldFraction = trace.fraction;
						if ( 1 )
						{

							VPROF("IntersectStaticProps");
							for ( int i = 0; i < nCount; ++i )
							{
								// Generate a collideable
								IHandleEntity *pHandleEntity = enumerator.m_EntityHandles[i];

								if ( !StaticPropMgr()->IsStaticProp( pHandleEntity ) )
									continue;
								pCollideable = s_EngineTraceServer.HandleEntityToCollideable( pHandleEntity );
								s_EngineTraceServer.ClipRayToCollideable( entityRay, MASK_SOLID, pCollideable, &tr );

								// Make sure the ray is always shorter than it currently is
								s_EngineTraceServer.ClipTraceToTrace( tr, &trace );
							}
						}
					}
				}
#if VPROF_LEVEL > 0 
				g_VProfCurrentProfile.MarkFrame();
#endif
			}
		}
		double tEnd = Plat_FloatTime();
		float ms = (tEnd - tStart) * 1000.0f;
		if ( ms > 0 )
			normalTime = ms;
#if VPROF_LEVEL > 0 
		g_VProfCurrentProfile.MarkFrame();
		g_VProfCurrentProfile.Stop();
		g_VProfCurrentProfile.OutputReport( VPRT_FULL & ~VPRT_HIERARCHY, NULL );
#endif
		Msg("NORMAL RAY TEST: %.2fms\n", ms );
	}

	float batchedTime = 1;
	// batched trace test
	if ( 1 )
	{
#if VPROF_LEVEL > 0 
		g_VProfCurrentProfile.Start();
		g_VProfCurrentProfile.Reset();
		g_VProfCurrentProfile.ResetPeaks();
#endif
		double tStart = Plat_FloatTime();
		trace_t trace;
		CTraceFilterHitAll traceFilter;
		CTraceListData traceData;
		for ( int jj = 0; jj < ITERATION_COUNT; jj++ )
		{
			for ( int kk = 0; kk < batchCount; kk++)
			{

				int batchEnd = batches[kk].start + batches[kk].count;
				s_EngineTraceServer.SetupLeafAndEntityListBox( batches[kk].mins, batches[kk].maxs, &traceData );
				traceData.m_entityList.RemoveAll();	// normal list skips all but static props, so skip them here too for comparison
				for ( int i = batches[kk].start; i < batchEnd; i++ )
				{
					s_EngineTraceServer.TraceRayAgainstLeafAndEntityList( s_BenchmarkRays[i], &traceData, MASK_SOLID, &traceFilter, &trace );
				}
#if VPROF_LEVEL > 0 
				g_VProfCurrentProfile.MarkFrame();
#endif
			}
		}

		double tEnd = Plat_FloatTime();
		float ms = (tEnd - tStart) * 1000.0f;
		if ( ms > 0 )
			batchedTime = ms;

#if VPROF_LEVEL > 0 
		g_VProfCurrentProfile.MarkFrame();
		g_VProfCurrentProfile.Stop();
		g_VProfCurrentProfile.OutputReport( VPRT_FULL & ~VPRT_HIERARCHY, NULL );
#endif
		Msg("LEAFLIST RAY TEST: %.2fms\n", ms );
	}
	float improvement = (normalTime - batchedTime) / normalTime;
	Msg("%.1f%% improvement due to batching at %d\n", improvement*100.0f, ray_count_max.GetInt());
}

CON_COMMAND_EXTERN( ray_bench, RayBench, "Time the rays" )
{
#if VPROF_LEVEL > 0 
	g_VProfCurrentProfile.Start();
	g_VProfCurrentProfile.Reset();
	g_VProfCurrentProfile.ResetPeaks();
#endif
	{
		double tStart = Plat_FloatTime();
		trace_t trace;
		int hit = 0;
		int miss = 0;
		int rayVsProp = 0;
		int boxVsProp = 0;
		for ( int i = 0; i < s_BenchmarkRays.Count(); i++ )
		{
			CM_BoxTrace( s_BenchmarkRays[i], 0, MASK_SOLID, true, trace );
			if ( 0 )
			{
				VPROF("QueryStaticProps");
				// Create a ray that extends only until we hit the world and adjust the trace accordingly
				Ray_t entityRay = s_BenchmarkRays[i];
				VectorScale( entityRay.m_Delta, trace.fraction, entityRay.m_Delta );
				CEntityListAlongRay enumerator;
				enumerator.Reset();
				SpatialPartition()->EnumerateElementsAlongRay( PARTITION_ENGINE_SOLID_EDICTS, entityRay, false, &enumerator );
				trace_t tr;
				ICollideable *pCollideable;
				int nCount = enumerator.Count();
				//float flWorldFraction = trace.fraction;
				if ( 0 )
				{

					VPROF("IntersectStaticProps");
				for ( int i = 0; i < nCount; ++i )
				{
					// Generate a collideable
					IHandleEntity *pHandleEntity = enumerator.m_EntityHandles[i];

					if ( !StaticPropMgr()->IsStaticProp( pHandleEntity ) )
						continue;
					if ( entityRay.m_IsRay )
						rayVsProp++;
					else
						boxVsProp++;
					pCollideable = s_EngineTraceServer.HandleEntityToCollideable( pHandleEntity );
					s_EngineTraceServer.ClipRayToCollideable( entityRay, MASK_SOLID, pCollideable, &tr );

					// Make sure the ray is always shorter than it currently is
					s_EngineTraceServer.ClipTraceToTrace( tr, &trace );
				}
				}
			}
			if ( trace.DidHit() )
				hit++;
			else
				miss++;
#if VPROF_LEVEL > 0 
			g_VProfCurrentProfile.MarkFrame();
#endif
		}
		double tEnd = Plat_FloatTime();
		float ms = (tEnd - tStart) * 1000.0f;
		int swept = 0;
		int point = 0;
		for ( int i = 0; i < s_BenchmarkRays.Count(); i++ )
		{
			swept += s_BenchmarkRays[i].m_IsSwept ? 1 : 0;
			point += s_BenchmarkRays[i].m_IsRay ? 1 : 0;
		}
		Msg("RAY TEST: %d hits, %d misses, %.2fms   (%d rays, %d sweeps) (%d ray/prop, %d box/prop)\n", hit, miss, ms, point, swept, rayVsProp, boxVsProp );
	}
#if VPROF_LEVEL > 0 
	g_VProfCurrentProfile.MarkFrame();
	g_VProfCurrentProfile.Stop();
	g_VProfCurrentProfile.OutputReport( VPRT_FULL & ~VPRT_HIERARCHY, NULL );
#endif
}
#endif

const int32 ALIGN16 g_ClearXYZSign[ 4 ] ALIGN16_POST = { 0x7fffffff, 0x7fffffff, 0x7fffffff, 0 };

fltx4 TestBoxAinB( const VectorAligned &ptA, const VectorAligned &extA, const VectorAligned &ptB, const VectorAligned &extB )
{
	fltx4 f4ptA = LoadAlignedSIMD( &ptA ), f4extA = LoadAlignedSIMD( &extA ), f4ptB = LoadAlignedSIMD( &ptB ), f4extB = LoadAlignedSIMD( &extB );
	return AndSIMD( CmpGeSIMD( f4ptA - f4extA, f4ptB - f4extB ), CmpLeSIMD( f4ptA + f4extA, f4ptB + f4extB ) );
}

// returns positive distances when box A protruding out of B; negative if A is contained inside of B
fltx4 ProtrusionBoxAoutB( const AABB_t &aabb0, const AABB_t &aabb1 )
{
	fltx4 f4Min0 = LoadUnaligned3SIMD( &aabb0.m_vMinBounds ), f4Min1 = LoadUnaligned3SIMD( &aabb1.m_vMinBounds );
	fltx4 f4Max0 = LoadUnaligned3SIMD( &aabb0.m_vMaxBounds ), f4Max1 = LoadUnaligned3SIMD( &aabb1.m_vMaxBounds );

	return MaxSIMD( f4Min1 - f4Min0, f4Max0 - f4Max1 );

	// equivalent expression:
	// MaxSIMD( ( f4ptB - f4extB ) - ( f4ptA - f4extA ), ( f4ptA + f4extA ) - ( f4ptB + f4extB ) );
}

struct OcclusionStats_t
{
	uint64 nTotalCalls;
	uint64 nTotalOcclusions;
	uint64 nNormalReuse;
	uint64 nQueriesCancelled;
	uint64 nWithinJitter;
	uint64 nKeyNotFound;
	uint64 nMovedMoreThanTolerance;
	uint64 nNotCompletedInTime;
	uint64 nTotalLatencyTicks;
	uint64 nTotalRcpThroughputTicks;
	uint64 nJobRestarts;
	uint64 nJobRestartMainThreadTicks;
	uint64 nVisLeavesCollected;
	uint64 nVisLeavesChecked;
	uint64 nVisShadowCullCalls;
	uint64 nVisShadowCullsSucceeded;

	CInterlockedUInt nQueries;
	CInterlockedUInt nQueriesInFlight;
	CInterlockedUInt nJobsInFlight;
	CInterlockedUInt nJobs;

	bool RegisterOcclusion( bool bOcclusion )
	{
		if ( bOcclusion )
			nTotalOcclusions++;
		return bOcclusion;
	}

	void Reset()
	{
		nTotalCalls = 0;
		nTotalOcclusions = 0;
		nNormalReuse = 0;
		nQueriesCancelled = 0;
		nWithinJitter = 0;
		nKeyNotFound = 0;
		nMovedMoreThanTolerance = 0;
		nNotCompletedInTime = 0;
		nTotalLatencyTicks = 0;
		nJobRestarts = 0;
		nJobRestartMainThreadTicks = 0;
		nVisLeavesCollected = 0;
		nVisLeavesChecked = 0;
		nVisShadowCullCalls = 0;
		nVisShadowCullsSucceeded = 0;
	}

	void Dump( bool bJitter )
	{
		uint64 nSubJitter = bJitter ? 0 : nWithinJitter;
		uint64 nTotalCallsAdj = nTotalCalls - nSubJitter;
		Msg( "%s Occlusion calls. %s (%.1f%%) calls within jitter. %u/%u queries, %u/%u jobs in flight. %d threads in pool\n", 
			V_pretifynum( nTotalCalls ), V_pretifynum( nWithinJitter ), ( nTotalCalls ? double( nWithinJitter ) * 100. / double( nTotalCalls ) : 100. ),
			( uint )nQueriesInFlight, ( uint )nQueries,
			( uint )nJobsInFlight, ( uint )nJobs,
			g_pThreadPool->NumThreads() );
		if ( nTotalCalls )
		{
			Msg( "Rates:  %12s (%4.1f%% of %s) Occlusions\n", V_pretifynum( nTotalOcclusions ), double( nTotalOcclusions ) * 100. / double( nTotalCalls ), V_pretifynum( nTotalCalls ) );
		}
		if ( nTotalCallsAdj )
		{
			Msg( "%20s (%4.1f%% of %s) Normal Query Reuses\n", V_pretifynum( nNormalReuse ), double( nNormalReuse ) * 100. / double( nTotalCallsAdj ), V_pretifynum( nTotalCallsAdj ) );
			if ( nVisLeavesCollected )
				Msg( "%20s (%4.1f per call) Vis Leaves collected\n", V_pretifynum( nVisLeavesCollected ), double( nVisLeavesCollected ) / double( nTotalCallsAdj ) );
			else
				Msg( "No Vis Leaves collected\n" );
			if ( nVisLeavesChecked )
				Msg("%20s (%4.1f per call) Vis Leaves checked\n", V_pretifynum( nVisLeavesChecked ), double( nVisLeavesChecked ) / double( nTotalCallsAdj ) );
			else
				Msg( "No Vis Leaves checked\n" );
			if ( nVisShadowCullCalls )
				Msg( "%20s (%4.1f%% of %s) Vis shadows culled\n", V_pretifynum( nVisShadowCullsSucceeded ), double( nVisShadowCullsSucceeded ) * 100. / double( nVisShadowCullCalls ), V_pretifynum( nVisShadowCullCalls ) );
			else
				Msg( "No Vis shadows culled\n" );
			if ( nWithinJitter )
				Msg( "%20s (%4.1f%% of %s) Within-Jitter Reuses\n", V_pretifynum( nWithinJitter ), double( nWithinJitter ) * 100. / double( nTotalCalls ), V_pretifynum( nTotalCalls ) );
			else
				Msg( "No Within-Jitter Reuses\n" );
			uint64 nQueuePoints = nNormalReuse + nKeyNotFound + nMovedMoreThanTolerance + nNotCompletedInTime;
			if ( nQueuePoints >= nJobRestarts && nJobRestarts )
				Msg( "%20s (%.1f queued queries per) Job Restarts\n", V_pretifynum( nJobRestarts ), ( double( nQueuePoints ) / double( nJobRestarts ) ) );
			else
				Msg( "No Jobs Restarted\n" );
		}
		else
		{
			Msg( "No untrivial occlusion calls registered\n" );
		}
		if ( nKeyNotFound | nMovedMoreThanTolerance | nNotCompletedInTime )
		{
			Msg( "Events: %12llu key not found.\n", nKeyNotFound );
			if ( nMovedMoreThanTolerance )
				Msg( "%20s (%4.1f%%) moved more than tolerance\n", V_pretifynum( nMovedMoreThanTolerance ), double( nMovedMoreThanTolerance ) * 100. / double( nWithinJitter + nMovedMoreThanTolerance + nNormalReuse + nNotCompletedInTime ) );
			else
				Msg( "None moved more than tolerance\n" );

			if ( nNotCompletedInTime )
				Msg( "%20s not completed on time\n", V_pretifynum( nNotCompletedInTime ) );
			else
				Msg( "All queries completed on time\n" );
		}
		else
		{
			Msg( "No events registered\n" );
		}
		if ( nNormalReuse )
		{
			Msg( "Ticks: %13s Query latency\n", V_pretifynum( nTotalLatencyTicks / nNormalReuse ) );
		}
		else
		{ 
			Msg( "Ticks: No Query Latency data\n" );
		}
		if ( nNormalReuse > nQueriesCancelled )
		{
			Msg( "%20s Query Reciprocal Throughput (%s cancels)\n", V_pretifynum( nTotalRcpThroughputTicks / ( nNormalReuse - nQueriesCancelled ) ), V_pretifynum( nQueriesCancelled ) );
		}
		if ( nJobRestarts && nJobRestartMainThreadTicks )
		{
			Msg( "%20s Job Restart\n", V_pretifynum( nJobRestartMainThreadTicks / nJobRestarts ) );
		}
	}
};
static OcclusionStats_t s_occlusionStats = { 0 };


class CAsyncOcclusionQuery;
CThreadFastMutex s_occlusionQueryMutex;
typedef CUtlLinkedList< CAsyncOcclusionQuery* > OcclusionQueryList_t;
OcclusionQueryList_t s_occlusionQueries; // these are the real queries in flight awaiting a job to pick them up

class COcclusionQueryJob : public CJob
{
public:
	bool m_bFinished; // true when this 
public:
	COcclusionQueryJob() : m_bFinished( false )
	{
		s_occlusionStats.nJobs++;
		s_occlusionStats.nJobsInFlight++;
	}
	virtual ~COcclusionQueryJob() OVERRIDE
	{
		s_occlusionStats.nJobs--;
	}
	virtual JobStatus_t	DoExecute() OVERRIDE;
};

static COcclusionQueryJob *s_pOcclusionQueryJob = NULL;  // this is the job that was last queued to consume the s_occlusionQueries queue

void SpinUpOcclusionJob()
{
	if ( s_pOcclusionQueryJob )
		s_pOcclusionQueryJob->Release();
	s_pOcclusionQueryJob = new COcclusionQueryJob;
	//s_pOcclusionQueryJob->AddRef();
	uint64 nSpinUpBegin = GetTimebaseRegister();
	g_pThreadPool->AddJob( s_pOcclusionQueryJob );
	s_occlusionStats.nJobRestartMainThreadTicks += GetTimebaseRegister() - nSpinUpBegin;
	s_occlusionStats.nJobRestarts++;
}

CON_COMMAND_F( occlusion_stats, "Occlusion statistics; [-jitter] [-reset]", FCVAR_RELEASE )
{
	bool bJitter = false, bReset = false, bFlush = false;
	for ( int i = 1; i < args.ArgC(); ++i )
	{
		if ( !V_stricmp( args[ i ], "-jitter" ) )
			bJitter = true;
		else if ( !V_stricmp( args[ i ], "-reset" ) )
			bReset = true;
		else if ( !V_stricmp( args[ i ], "-flush" ) )
			bFlush = true;
	}
	s_occlusionStats.Dump( bJitter);
	if ( bReset )
	{
		s_occlusionStats.Reset();
	}
	if ( bFlush )
	{
		FlushOcclusionQueries();
	}
}

void OnOcclusionTestAsyncChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	extern void AdjustThreadPoolThreadCount();
	AdjustThreadPoolThreadCount();
}

ConVar occlusion_test_margins( "occlusion_test_margins", "36", FCVAR_RELEASE, "Amount by which the player bounding box is expanded for occlusion test. This margin should be large enough to accommodate player movement within a frame or two, and the longest weapon they might hold. Shadow does not take this into account." ); // default: 360 (max speed) / 30 ( give it a couple of frames) + however much the biggest weapon can stick out
ConVar occlusion_test_jump_margin( "occlusion_test_jump_margin", "12", FCVAR_RELEASE, "Amount by which the player bounding box is expanded up for occlusion test to account for jumping. This margin should be large enough to accommodate player movement within a frame or two. Affects both camera box and player box." ); // default: 360 (max speed) / 30 ( give it a couple of frames) + however much the biggest weapon can stick out
ConVar occlusion_test_shadow_max_distance( "occlusion_test_shadow_max_distance", "1500", FCVAR_RELEASE, "Max distance at which to consider shadows for occlusion computations" );
ConVar occlusion_test_async( "occlusion_test_async", "0", FCVAR_RELEASE, "Enable asynchronous occlusion test in another thread; may save some server tick time at the cost of synchronization overhead with the async occlusion query thread", OnOcclusionTestAsyncChanged );
ConVar occlusion_test_async_move_tolerance( "occlusion_test_async_move_tolerance", "8.25", FCVAR_CHEAT );
ConVar occlusion_test_async_jitter( "occlusion_test_async_jitter", "2", FCVAR_CHEAT );



bool IsCastingShadow( const AABB_t &aabb )
{
	int nLeafArray[ 1024 ];
	int nLeafCount = CM_BoxLeafnums( aabb.m_vMinBounds, aabb.m_vMaxBounds, nLeafArray, ARRAYSIZE( nLeafArray ), NULL );
	s_occlusionStats.nVisLeavesCollected+=nLeafCount;
	s_occlusionStats.nVisShadowCullCalls++;

	for ( int n = 0; n < nLeafCount; ++n )
	{
		int nLeaf = nLeafArray[ n ];
		mleaf_t *pLeaf = &host_state.worldbrush->leafs[ nLeaf ];
		if ( pLeaf && ( pLeaf->flags & ( LEAF_FLAGS_SKY | LEAF_FLAGS_SKY2D ) ) )
		{
			s_occlusionStats.nVisLeavesChecked += n+1;
			return true;
		}
	}
	s_occlusionStats.nVisShadowCullsSucceeded++;
	s_occlusionStats.nVisLeavesChecked += nLeafCount;
	return false;
}

bool IsFullyOccluded_WithShadow( const AABB_t &aabb1, const AABB_t &aabb2, const Vector &vShadow, float flExtraMoveTolerance = 0.0f )
{
	VectorAligned vCenter1( aabb1.GetCenter() );
	VectorAligned vHullExtents1( aabb1.GetSize() * 0.5f );
	VectorAligned vCenter2( aabb2.GetCenter() );
	VectorAligned vHullExtents2( aabb2.GetSize() * 0.5f );
	float flHorzMargin = occlusion_test_margins.GetFloat();
	float flJumpMargin = occlusion_test_jump_margin.GetFloat();

	flHorzMargin += flExtraMoveTolerance;
	flJumpMargin += flExtraMoveTolerance;

	if ( vShadow != vec3_origin )
	{
		Vector vHullDist = VectorMax( vec3_origin, VectorAbs( vCenter1 - vCenter2 ) - ( vHullExtents1 + vHullExtents2 ) ); // distance between hulls..
		if ( vHullDist.LengthSqr() < Sqr( occlusion_test_shadow_max_distance.GetFloat() ) 
		  && IsCastingShadow( aabb1 ) )
		{
			VectorAligned vShadowEnd( vCenter1 + vShadow );
			OcclusionTestResults_t tr;
			bool bShadowIsClose = CM_IsFullyOccluded( vCenter1, vHullExtents1, vShadowEnd, vHullExtents1, &tr );
			if ( bShadowIsClose )
			{
				AABB_t aabbEx;
				aabbEx.m_vMinBounds = VectorMin( aabb1.m_vMinBounds - Vector( flHorzMargin, flHorzMargin, 0 ), tr.vEndMin );
				aabbEx.m_vMaxBounds = VectorMax( aabb1.m_vMaxBounds + Vector( flHorzMargin, flHorzMargin, flJumpMargin ), tr.vEndMax );
				return CM_IsFullyOccluded( aabbEx, aabb2 ); // trace extended box
			}
			else
			{
				return false; // shadow goes too far, don't try to trace :(
			}
		}
	}

	// trace extended box, no shadow
	return CM_IsFullyOccluded(
		VectorAligned( vCenter1 + Vector( 0, 0, flJumpMargin * 0.5f ) ),
		VectorAligned( vHullExtents1 + Vector( flHorzMargin, flHorzMargin, flJumpMargin * 0.5f ) ),
		vCenter2, vHullExtents2
		);
}

class ALIGN16 CAsyncOcclusionQuery : public CAlignedNewDelete< 16, CRefCounted< CRefCountServiceMT > >
{
public:
	Vector m_vShadow;
	AABB_t m_aabb0;
	AABB_t m_aabb1;
	uint64 m_nTicksLatency;	// this is garbage on architectures that don't have coherent rdtsc on multiple threads. Otherwise, it's the start tick when !m_bCompleted and latency of this query in tick if m_bCompleted
	uint64 m_nTicksRcpThroughput;
	bool m_bCancel;
	bool m_bResult;
	bool m_bCompleted;
public:
	CAsyncOcclusionQuery( const AABB_t &aabb0, const AABB_t &aabb1, const Vector &vShadow )
	{
		s_occlusionStats.nQueriesInFlight++;
		s_occlusionStats.nQueries++;
		Init( aabb0, aabb1, vShadow );
	}
	virtual ~CAsyncOcclusionQuery() OVERRIDE
	{
		if ( !m_bCompleted )
			s_occlusionStats.nQueriesInFlight--;
		s_occlusionStats.nQueries--;
	}

	void Init( const AABB_t &aabb0, const AABB_t &aabb1, const Vector &vShadow )
	{
		m_aabb0 = aabb0;
		m_aabb1 = aabb1;
		m_vShadow = vShadow;
		m_bCancel= false;
		m_bResult =false;
		m_bCompleted = false;
		m_nTicksLatency = GetTimebaseRegister();
		m_nTicksRcpThroughput = 0;
	}

	void DoExecute()
	{
		uint64 nTicksStarted = GetTimebaseRegister();
		if ( !m_bCancel )
		{
#if COMPILER_GCC
			__sync_synchronize();
#else
			std::atomic_thread_fence( std::memory_order_acquire );
#endif
			m_bResult = IsFullyOccluded_WithShadow( m_aabb0, m_aabb1, m_vShadow, occlusion_test_async_move_tolerance.GetFloat() );
		}
		uint64 nTicksEnded = GetTimebaseRegister();
		m_nTicksRcpThroughput = m_bCancel ? 0 : nTicksEnded - nTicksStarted;
		m_nTicksLatency = nTicksEnded - m_nTicksLatency;
#if COMPILER_GCC 
		__sync_synchronize();
#else
		std::atomic_thread_fence( std::memory_order_release );
#endif
		s_occlusionStats.nQueriesInFlight--;
		m_bCompleted = true;
	}

	fltx4 GetManhattanDistance( const AABB_t &aabb0, const AABB_t &aabb1 )
	{
		return SetWToZeroSIMD( MaxSIMD( ProtrusionBoxAoutB( aabb0, m_aabb0 ), ProtrusionBoxAoutB( aabb1, m_aabb1 ) ) );
	}

	void Cancel()
	{
		m_bCancel = true;
	}
	void Queue( int nOcclusionTestsSuspended )
	{
		Assert( !m_bCancel );
		Assert( !m_bCompleted );
		AddRef();
		{
			CAutoLockT< CThreadFastMutex > autoLock( s_occlusionQueryMutex );
			s_occlusionQueries.AddToTail( this );
			if ( s_pOcclusionQueryJob )
			{
				if ( !s_pOcclusionQueryJob->m_bFinished )
					return;
			}
		}
		if ( !nOcclusionTestsSuspended || occlusion_test_async.GetInt() >= 2 )
			SpinUpOcclusionJob();
	}
} ALIGN16_POST;


JobStatus_t	COcclusionQueryJob::DoExecute()
{
	for ( ;; )
	{
		CAsyncOcclusionQuery *pQuery;
		{
			CAutoLockT< CThreadFastMutex > autoLock( s_occlusionQueryMutex );
			OcclusionQueryList_t::IndexLocalType_t nHead = s_occlusionQueries.Head( );
			if ( nHead == s_occlusionQueries.InvalidIndex() )
			{
				m_bFinished = true;
				break;
			}
			else
			{
				pQuery = s_occlusionQueries.Element( nHead );
				s_occlusionQueries.Remove( nHead );
			}
		}

		pQuery->DoExecute();
		pQuery->Release();
	}
	s_occlusionStats.nJobsInFlight--;
	return JOB_OK;
}



void CEngineTrace::FlushOcclusionQueries()
{
	// cancel all queries in flight: take them away from the jobs consuming them
	{
		CAutoLockT< CThreadFastMutex > autoLock( s_occlusionQueryMutex );
		for ( ;;) 
		{
			OcclusionQueryList_t::IndexLocalType_t nHead = s_occlusionQueries.Head( );
			if ( nHead == s_occlusionQueries.InvalidIndex() )
			{
				break;
			}
			else
			{
				CAsyncOcclusionQuery *pQuery = s_occlusionQueries.Element( nHead );
				s_occlusionQueries.Remove( nHead );
				pQuery->Release();
			}
		}
	}
	// also, release all jobs currently locked by 
	for ( UtlHashHandle_t it = m_OcclusionQueryMap.FirstHandle(); it != m_OcclusionQueryMap.InvalidHandle(); it = m_OcclusionQueryMap.RemoveAndAdvance( it ) )
	{
		m_OcclusionQueryMap.Element( it )->Release();
	}
	m_OcclusionQueryMap.Purge();
	if ( s_pOcclusionQueryJob )
	{
		s_pOcclusionQueryJob->Release();
		s_pOcclusionQueryJob = NULL;
	}
}

void FlushOcclusionQueries()
{
	s_EngineTraceServer.FlushOcclusionQueries();
#ifndef DEDICATED
	s_EngineTraceClient.FlushOcclusionQueries();
#endif
}


void CEngineTrace::ResumeOcclusionTests()
{
	if ( !--m_nOcclusionTestsSuspended && s_occlusionQueries.Head() != s_occlusionQueries.InvalidIndex() )
	{
		// We're out of suspension and we have some jobs queued up. Execute them.
		SpinUpOcclusionJob();
	}
}




bool CEngineTrace::IsFullyOccluded( int nOcclusionKey, const AABB_t &aabb0, const AABB_t &aabb1, const Vector &vShadow )
{
	s_occlusionStats.nTotalCalls++;
	if ( !occlusion_test_async.GetInt() || nOcclusionKey < 0 )
		return s_occlusionStats.RegisterOcclusion( IsFullyOccluded_WithShadow( aabb0, aabb1, vShadow ) );

	// first, try to find the previous frame version of this job
	UtlHashHandle_t hFind = m_OcclusionQueryMap.Find( nOcclusionKey );
	if ( hFind != m_OcclusionQueryMap.InvalidHandle() )
	{
		CAsyncOcclusionQuery* pQuery = m_OcclusionQueryMap[ hFind ];
		fltx4 f4ManhattanError = pQuery->GetManhattanDistance( aabb0, aabb1 );
		if ( IsAllGreaterThanOrEq( ReplicateX4( occlusion_test_async_move_tolerance.GetFloat() ), f4ManhattanError ) )
		{
			if ( pQuery->m_bCompleted )
			{
#if COMPILER_GCC 
				__sync_synchronize();
#else
				std::atomic_thread_fence( std::memory_order_acquire );
#endif
				bool bIsOccluded = pQuery->m_bResult;
				s_occlusionStats.RegisterOcclusion( bIsOccluded );
				// Optimal case: we can use the results of this job because it's a strict superset of this query and it's completed
				if ( IsAllGreaterThanOrEq( ReplicateX4( occlusion_test_async_jitter.GetFloat() ), f4ManhattanError ) )
				{
					s_occlusionStats.nWithinJitter++;
					// we don't need to restart this query, it's perfectly fine within the jitter margin
				}
				else
				{
					s_occlusionStats.nNormalReuse++;
					s_occlusionStats.nTotalLatencyTicks += pQuery->m_nTicksLatency;
					if ( pQuery->m_nTicksRcpThroughput )
						s_occlusionStats.nTotalRcpThroughputTicks += pQuery->m_nTicksRcpThroughput;
					else
						s_occlusionStats.nQueriesCancelled++;
					// the query was within the margins, but we need to restart it. This will hopefully be much more common than any of the error modes below
					s_occlusionStats.nQueriesInFlight++; // reusing the same queue
					pQuery->Init( aabb0, aabb1, vShadow );
					pQuery->Queue( m_nOcclusionTestsSuspended);
				}
				return bIsOccluded;
			}
			else
			{
				s_occlusionStats.nNotCompletedInTime++;
			}
		}
		else
		{
			s_occlusionStats.nMovedMoreThanTolerance++;
		}

		// for whatever reason, the query didn't work out... try to cancel it, and queue a new one
		pQuery->Cancel();
		pQuery->Release();

		CAsyncOcclusionQuery* pNewQuery = new CAsyncOcclusionQuery( aabb0, aabb1, vShadow );
		pNewQuery->Queue( m_nOcclusionTestsSuspended );
		m_OcclusionQueryMap[ hFind ] = pNewQuery;
	}
	else
	{
		s_occlusionStats.nKeyNotFound++;
		CAsyncOcclusionQuery* pNewQuery = new CAsyncOcclusionQuery( aabb0, aabb1, vShadow );
		pNewQuery->Queue( m_nOcclusionTestsSuspended );
		m_OcclusionQueryMap.Insert( nOcclusionKey, pNewQuery );
	}

	// we queued the new query, but we still don't know whether the boxes are occlude
	// we may return false here to be safe and save some CPU, or we could run the query synchronously

	return s_occlusionStats.RegisterOcclusion( IsFullyOccluded_WithShadow( aabb0, aabb1, vShadow ) );
}



//-----------------------------------------------------------------------------
// A version that simply accepts a ray (can work as a traceline or tracehull)
//-----------------------------------------------------------------------------
void CEngineTrace::TraceRay( const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace )
{	
	// check ray extents for bugs
	Assert(ray.m_Extents.x >=0 && ray.m_Extents.y >= 0 && ray.m_Extents.z >= 0);

#if defined _DEBUG && !defined DEDICATED
	if( debugrayenable.GetBool() )
	{
		s_FrameRays.AddToTail( ray );
	}
#endif

#if BENCHMARK_RAY_TEST
	if( s_BenchmarkRays.Count() < 15000 )
	{
		s_BenchmarkRays.EnsureCapacity(15000);
		s_BenchmarkRays.AddToTail( ray );
	}
#endif

	VPROF_INCREMENT_COUNTER( "TraceRay", 1 );
	m_traceStatCounters[TRACE_STAT_COUNTER_TRACERAY]++;
//	VPROF_BUDGET( "CEngineTrace::TraceRay", "Ray/Hull Trace" );
	
	CTraceFilterHitAll traceFilter;
	if ( !pTraceFilter )
	{
		pTraceFilter = &traceFilter;
	}

	CM_ClearTrace( pTrace );

	// Collide with the world.
	if ( pTraceFilter->GetTraceType() != TRACE_ENTITIES_ONLY )
	{
		ICollideable *pCollide = GetWorldCollideable();
		Assert( pCollide );

		// Make sure the world entity is unrotated
		// FIXME: BAH! The !pCollide test here is because of
		// CStaticProp::PrecacheLighting.. it's occurring too early
		// need to fix that later

		// Commenting this check out because Abs queries are not valid at the moment and we can't easily set them valid from Engine.dll,
		// So having this assert enabled causes another assert to fire just for checking the origin / angles when abs queries are not valid.
		//Assert(!pCollide || pCollide->GetCollisionOrigin() == vec3_origin );
		//Assert(!pCollide || pCollide->GetCollisionAngles() == vec3_angle );

		CM_BoxTrace( ray, 0, fMask, true, *pTrace );
		SetTraceEntity( pCollide, pTrace );

		// inside world, no need to check being inside anything else
		if ( pTrace->startsolid )
			return;

		// Early out if we only trace against the world
		if ( pTraceFilter->GetTraceType() == TRACE_WORLD_ONLY )
			return;
	}
	else
	{
		// Set initial start + endpos, necessary if the world isn't traced against 
		// because we may not trace against *anything* below.
		VectorAdd( ray.m_Start, ray.m_StartOffset, pTrace->startpos );
		VectorAdd( pTrace->startpos, ray.m_Delta, pTrace->endpos );
	}

	// Save the world collision fraction.
	float flWorldFraction = pTrace->fraction;
	float flWorldFractionLeftSolidScale = flWorldFraction;

	// Create a ray that extends only until we hit the world
	// and adjust the trace accordingly
	Ray_t entityRay = ray;

	if ( pTrace->fraction == 0 )
	{
		entityRay.m_Delta.Init();
		flWorldFractionLeftSolidScale = pTrace->fractionleftsolid;
		pTrace->fractionleftsolid = 1.0f;
		pTrace->fraction = 1.0f;
	}
	else
	{
		// Explicitly compute end so that this computation happens at the quantization of
		// the output (endpos).  That way we won't miss any intersections we would get
		// by feeding these results back in to the tracer
		// This is not the same as entityRay.m_Delta *= pTrace->fraction which happens 
		// at a quantization that is more precise as m_Start moves away from the origin
		Vector end;
		VectorMA( entityRay.m_Start, pTrace->fraction, entityRay.m_Delta, end );
		VectorSubtract(end, entityRay.m_Start, entityRay.m_Delta);
		// We know this is safe because pTrace->fraction != 0
		pTrace->fractionleftsolid /= pTrace->fraction;
		pTrace->fraction = 1.0;
	}

	// Collide with entities along the ray
	// FIXME: Hitbox code causes this to be re-entrant for the IK stuff.
	// If we could eliminate that, this could be static and therefore
	// not have to reallocate memory all the time
	CEntityListAlongRay enumerator;
	enumerator.Reset();
	SpatialPartition()->EnumerateElementsAlongRay( SpatialPartitionMask(), entityRay, false, &enumerator );

	bool bNoStaticProps = pTraceFilter->GetTraceType() == TRACE_ENTITIES_ONLY;
	bool bFilterStaticProps = pTraceFilter->GetTraceType() == TRACE_EVERYTHING_FILTER_PROPS;

	trace_t tr;
	ICollideable *pCollideable;
	int nCount = enumerator.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		// Generate a collideable
		IHandleEntity *pHandleEntity = enumerator.m_EntityHandles[i];
		pCollideable = HandleEntityToCollideable( pHandleEntity );

		// Check for error condition
		if ( IsPC() && IsDebug() && !IsSolid( pCollideable->GetSolid(), pCollideable->GetSolidFlags() ) )
		{
			Assert( 0 );
			Msg( "%s in solid list (not solid)\n", GetDebugName(pHandleEntity) );
			continue;
		}

		if ( !StaticPropMgr()->IsStaticProp( pHandleEntity ) )
		{
			if ( !pTraceFilter->ShouldHitEntity( pHandleEntity, fMask ) )
				continue;
		}
		else
		{
			// FIXME: Could remove this check here by
			// using a different spatial partition mask. Look into it
			// if we want more speedups here.
			if ( bNoStaticProps )
				continue;

			if ( bFilterStaticProps )
			{
				if ( !pTraceFilter->ShouldHitEntity( pHandleEntity, fMask ) )
					continue;
			}
		}

		ClipRayToCollideable( entityRay, fMask, pCollideable, &tr );

		// Make sure the ray is always shorter than it currently is
		ClipTraceToTrace( tr, pTrace );

		// Stop if we're in allsolid
		if (pTrace->allsolid)
			break;
	}

	// Fix up the fractions so they are appropriate given the original
	// unclipped-to-world ray
	pTrace->fraction *= flWorldFraction;
	pTrace->fractionleftsolid *= flWorldFractionLeftSolidScale;

#ifdef _DEBUG
	Vector vecOffset, vecEndTest;
	VectorAdd( ray.m_Start, ray.m_StartOffset, vecOffset );
	VectorMA( vecOffset, pTrace->fractionleftsolid, ray.m_Delta, vecEndTest );
	Assert( VectorsAreEqual( vecEndTest, pTrace->startpos, 0.1f ) );
	VectorMA( vecOffset, pTrace->fraction, ray.m_Delta, vecEndTest );
	Assert( VectorsAreEqual( vecEndTest, pTrace->endpos, 0.1f ) );
//	Assert( !ray.m_IsRay || pTrace->allsolid || pTrace->fraction >= pTrace->fractionleftsolid );
#endif

	if ( !ray.m_IsRay )
	{
		// Make sure no fractionleftsolid can be used with box sweeps
		VectorAdd( ray.m_Start, ray.m_StartOffset, pTrace->startpos );
		pTrace->fractionleftsolid = 0;

#ifdef _DEBUG
		pTrace->fractionleftsolid = VEC_T_NAN;
#endif
	}
}


//-----------------------------------------------------------------------------
// A version that sweeps a collideable through the world
//-----------------------------------------------------------------------------
void CEngineTrace::SweepCollideable( ICollideable *pCollide, 
		const Vector &vecAbsStart, const Vector &vecAbsEnd, const QAngle &vecAngles,
		unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace )
{
	Ray_t ray;
	Assert( vecAngles == vec3_angle );
	ray.Init( vecAbsStart, vecAbsEnd, pCollide->OBBMins(), pCollide->OBBMaxs() );
	if ( pCollide->GetSolidFlags() & FSOLID_ROOT_PARENT_ALIGNED )
	{
		ray.m_pWorldAxisTransform = pCollide->GetRootParentToWorldTransform();
	}
	TraceRay( ray, fMask, pTraceFilter, pTrace );
}


//-----------------------------------------------------------------------------
// Lets clients know about all edicts along a ray
//-----------------------------------------------------------------------------
class CEnumerationFilter : public IPartitionEnumerator
{
public:
	CEnumerationFilter( CEngineTrace *pEngineTrace, IEntityEnumerator* pEnumerator ) : 
		m_pEngineTrace(pEngineTrace), m_pEnumerator(pEnumerator) {}

	IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
		// Don't enumerate static props
		if ( StaticPropMgr()->IsStaticProp( pHandleEntity ) )
			return ITERATION_CONTINUE;

		if ( !m_pEnumerator->EnumEntity( pHandleEntity ) )
		{
			return ITERATION_STOP;
		}

		return ITERATION_CONTINUE;
	}

private:
	IEntityEnumerator* m_pEnumerator;
	CEngineTrace *m_pEngineTrace;
};


//-----------------------------------------------------------------------------
// Enumerates over all entities along a ray
// If triggers == true, it enumerates all triggers along a ray
//-----------------------------------------------------------------------------
void CEngineTrace::EnumerateEntities( const Ray_t &ray, bool bTriggers, IEntityEnumerator *pEnumerator )
{
	m_traceStatCounters[TRACE_STAT_COUNTER_ENUMERATE]++;
	// FIXME: If we store CBaseHandles directly in the spatial partition, this method
	// basically becomes obsolete. The spatial partition can be queried directly.
	CEnumerationFilter enumerator( this, pEnumerator );

	int fMask = !bTriggers ? SpatialPartitionMask() : SpatialPartitionTriggerMask();

	// NOTE: Triggers currently don't exist on the client
	if (fMask)
	{
		SpatialPartition()->EnumerateElementsAlongRay( fMask, ray, false, &enumerator );
	}
}


//-----------------------------------------------------------------------------
// Lets clients know about all entities in a box
//-----------------------------------------------------------------------------
void CEngineTrace::EnumerateEntities( const Vector &vecAbsMins, const Vector &vecAbsMaxs, IEntityEnumerator *pEnumerator )
{
	m_traceStatCounters[TRACE_STAT_COUNTER_ENUMERATE]++;
	// FIXME: If we store CBaseHandles directly in the spatial partition, this method
	// basically becomes obsolete. The spatial partition can be queried directly.
	CEnumerationFilter enumerator( this, pEnumerator );
	SpatialPartition()->EnumerateElementsInBox( SpatialPartitionMask(),
		vecAbsMins, vecAbsMaxs, false, &enumerator );
}


class CEntList : public IEntityEnumerator
{
public:
	virtual bool EnumEntity( IHandleEntity *pHandleEntity )
	{
		IServerUnknown *pNetEntity = static_cast<IServerUnknown*>(pHandleEntity);
		ICollideable *pCollide = pNetEntity->GetCollideable();
		if ( !pCollide )
			return true;

		Vector vecCenter;
		VectorMA( MainViewOrigin(), 100.0f, MainViewForward(), vecCenter );
		float flDist = (vecCenter - pCollide->GetCollisionOrigin()).LengthSqr();
		if (flDist < m_flClosestDist)
		{
			m_flClosestDist = flDist;
			m_pClosest = pCollide;
		}

		return true;
	}

	ICollideable *m_pClosest;
	float m_flClosestDist;


};


// create a macro that is true if we are allowed to debug traces during thinks, and compiles out to nothing otherwise.
#ifndef _PS3
#include "engine/thinktracecounter.h"
#endif

/// Used only in debugging: get/set/clear/increment the trace debug counter. See comment below for details.
int CEngineTrace::GetSetDebugTraceCounter( int value, DebugTraceCounterBehavior_t behavior )
{
#ifdef THINK_TRACE_COUNTER_COMPILED
	extern CTHREADLOCALINT g_DebugTracesRemainingBeforeTrap;
	if ( DEBUG_THINK_TRACE_COUNTER_ALLOWED() )
	{
		const int retval = g_DebugTracesRemainingBeforeTrap;

		switch ( behavior ) 
		{
		case kTRACE_COUNTER_SET: 
			{
				g_DebugTracesRemainingBeforeTrap = value;
				break;
			}
		case kTRACE_COUNTER_INC:
			{
				g_DebugTracesRemainingBeforeTrap = value + g_DebugTracesRemainingBeforeTrap;
				break;
			}
		}

		return retval;
	}
	else
	{
		return 0;
	}
#else
	return 0;
#endif
}

#ifdef _DEBUG

	//-----------------------------------------------------------------------------
	// A method to test out sweeps
	//-----------------------------------------------------------------------------
	CON_COMMAND( test_sweepaabb, "method to test out sweeps" )
	{
		Vector vecStartPoint;
		VectorMA( MainViewOrigin(), 50.0f, MainViewForward(), vecStartPoint );

		Vector endPoint;
		VectorMA( MainViewOrigin(), COORD_EXTENT * 1.74f, MainViewForward(), endPoint );

		Ray_t ray;
		ray.Init( vecStartPoint, endPoint );

		trace_t tr;
	//	CTraceFilterHitAll traceFilter;
	//	g_pEngineTraceServer->TraceRay( ray, MASK_ALL, &traceFilter, &tr );

		CEntList list;
		list.m_pClosest = NULL;
		list.m_flClosestDist = FLT_MAX;
		g_pEngineTraceServer->EnumerateEntities( MainViewOrigin() - Vector( 200, 200, 200 ), MainViewOrigin() + Vector( 200, 200, 200 ), &list );

		if ( !list.m_pClosest )
			return;

		// Visualize the intersection test
		ICollideable *pCollide = list.m_pClosest;
		if ( pCollide->GetCollisionOrigin() == vec3_origin )
			return;

		QAngle test( 0, 45, 0 );
	#ifndef DEDICATED
		CDebugOverlay::AddBoxOverlay( pCollide->GetCollisionOrigin(),
			pCollide->OBBMins(), pCollide->OBBMaxs(),
			test /*pCollide->GetCollisionAngles()*/, 0, 0, 255, 128, 5.0f );
	#endif

		VectorMA( MainViewOrigin(), 200.0f, MainViewForward(), endPoint );
		ray.Init( vecStartPoint, endPoint, Vector( -10, -20, -10 ), Vector( 30, 30, 20 ) );

		bool bIntersect = IntersectRayWithOBB( ray, pCollide->GetCollisionOrigin(), test, pCollide->OBBMins(),
			pCollide->OBBMaxs(), 0.0f, &tr );
		unsigned char r, g, b, a;
		b = 0;
		a = 255;
		r = bIntersect ? 255 : 0;
		g = bIntersect ? 0 : 255;

	#ifndef DEDICATED
		CDebugOverlay::AddSweptBoxOverlay( tr.startpos, tr.endpos,
			Vector( -10, -20, -10 ), Vector( 30, 30, 20 ), vec3_angle, r, g, b, a, 5.0 );
	#endif
	}


#endif
