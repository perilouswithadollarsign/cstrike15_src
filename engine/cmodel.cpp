//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: BSP collision!
//
// $NoKeywords: $
//=============================================================================//

#include "cmodel_engine.h"
#include "cmodel_private.h"
#include "dispcoll_common.h"
#include "coordsize.h"

#include "quakedef.h"
#include <string.h>
#include <stdlib.h>
#include "mathlib/mathlib.h"
#include "common.h"
#include "sysexternal.h"
#include "zone.h"
#include "utlvector.h"
#include "const.h"
#include "gl_model_private.h"
#include "vphysics_interface.h"
#include "icliententity.h"
#include "engine/ICollideable.h"
#include "enginethreads.h"
#include "sys_dll.h"
#include "collisionutils.h"
#include "tier0/tslist.h"
#include "tier0/vprof.h"
#include "tier0/microprofiler.h"
#include "keyvalues.h"
#include "paint.h"
#include "tier1/fmtstr.h"
#include "bsplog.h"
#include "edict.h"
#include "debugoverlay.h"
#include "engine/IEngineTrace.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CCollisionBSPData g_BSPData;								// the global collision bsp
#define g_BSPData dont_use_g_BSPData_directly

#ifdef COUNT_COLLISIONS
CCollisionCounts  g_CollisionCounts;						// collision test counters
#endif

static const float kBoxCheckFloatEpsilon = 0.01f; // Used for box trace assert checks below.


csurface_t CCollisionBSPData::nullsurface = { "**empty**", 0, 0 };				// generic null collision model surface

csurface_t *CCollisionBSPData::GetSurfaceAtIndex( unsigned short surfaceIndex )
{
	if ( surfaceIndex == SURFACE_INDEX_INVALID )
	{
		return &nullsurface;
	}
	return &map_surfaces[surfaceIndex];
}

CTSPool<TraceInfo_t> g_TraceInfoPool;

TraceInfo_t *BeginTrace()
{
	TraceInfo_t *pTraceInfo = g_TraceInfoPool.GetObject();
	if ( pTraceInfo->m_BrushCounters[0].Count() != GetCollisionBSPData()->numbrushes + 1 )
	{
		memset( pTraceInfo->m_Count, 0, sizeof( pTraceInfo->m_Count ) );
		pTraceInfo->m_nCheckDepth = -1;

		for ( int i = 0; i < MAX_CHECK_COUNT_DEPTH; i++ )
		{
			pTraceInfo->m_BrushCounters[i].SetCount( GetCollisionBSPData()->numbrushes + 1 );
			pTraceInfo->m_DispCounters[i].SetCount( g_DispCollTreeCount );

			memset( pTraceInfo->m_BrushCounters[i].Base(), 0, pTraceInfo->m_BrushCounters[i].Count() * sizeof(TraceCounter_t) );
			memset( pTraceInfo->m_DispCounters[i].Base(), 0, pTraceInfo->m_DispCounters[i].Count() * sizeof(TraceCounter_t) );
		}
	}

	PushTraceVisits( pTraceInfo );
	pTraceInfo->m_pBSPData = GetCollisionBSPData();

	return pTraceInfo;
}

void PushTraceVisits( TraceInfo_t *pTraceInfo )
{
	++pTraceInfo->m_nCheckDepth;
	Assert( (pTraceInfo->m_nCheckDepth >= 0) && (pTraceInfo->m_nCheckDepth < MAX_CHECK_COUNT_DEPTH) );

	int i = pTraceInfo->m_nCheckDepth;
	pTraceInfo->m_Count[i]++;
	if ( pTraceInfo->m_Count[i] == 0 )
	{
		pTraceInfo->m_Count[i]++;
		memset( pTraceInfo->m_BrushCounters[i].Base(), 0, pTraceInfo->m_BrushCounters[i].Count() * sizeof(TraceCounter_t) );
		memset( pTraceInfo->m_DispCounters[i].Base(), 0, pTraceInfo->m_DispCounters[i].Count() * sizeof(TraceCounter_t) );
	}
}

void PopTraceVisits( TraceInfo_t *pTraceInfo )
{
	--pTraceInfo->m_nCheckDepth;
	Assert( pTraceInfo->m_nCheckDepth >= -1 );
}

void EndTrace( TraceInfo_t *&pTraceInfo )
{
	PopTraceVisits( pTraceInfo );
	Assert( pTraceInfo->m_nCheckDepth == -1 );
	g_TraceInfoPool.PutObject( pTraceInfo );
	pTraceInfo = NULL;
}

static ConVar map_noareas( "map_noareas", "0", 0, "Disable area to area connection testing." );

void	FloodAreaConnections (CCollisionBSPData *pBSPData);



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
vcollide_t *CM_GetVCollide( int modelIndex )
{
	cmodel_t *pModel = CM_InlineModelNumber( modelIndex );
	if( !pModel )
		return NULL;

	// return the model's collision data
	return &pModel->vcollisionData;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
cmodel_t *CM_InlineModel( const char *name )
{
	// error checking!
	if( !name )
		return NULL;

	// JAYHL2: HACKHACK Get rid of this
	if( StringHasPrefix( name, "maps/" ) )
		return CM_InlineModelNumber( 0 );

	// check for valid name
	if( name[0] != '*' )
		Sys_Error( "CM_InlineModel: bad model name!" );

	// check for valid model
	int ndxModel = atoi( name + 1 );
	if( ( ndxModel < 1 ) || ( ndxModel >= GetCollisionBSPData()->numcmodels ) )
		Sys_Error( "CM_InlineModel: bad model number!" );

	return CM_InlineModelNumber( ndxModel );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
cmodel_t *CM_InlineModelNumber( int index )
{
	CCollisionBSPData *pBSPDataData = GetCollisionBSPData();

	if( ( index < 0 ) || ( index > pBSPDataData->numcmodels ) )
		return NULL;

	return ( &pBSPDataData->map_cmodels[ index ] );
}


int CM_BrushContents_r( CCollisionBSPData *pBSPData, int nodenum )
{
	int contents = 0;

	while (1)
	{
		if (nodenum < 0)
		{
			int leafIndex = -1 - nodenum;
			cleaf_t &leaf = pBSPData->map_leafs[leafIndex];

			for ( int i = 0; i < leaf.numleafbrushes; i++ )
			{
				unsigned short brushIndex = pBSPData->map_leafbrushes[ leaf.firstleafbrush + i ];
				contents |= pBSPData->map_brushes[brushIndex].contents;
			}

			return contents;
		}

		cnode_t &node = pBSPData->map_rootnode[nodenum];
		contents |= CM_BrushContents_r( pBSPData, node.children[0] );
		nodenum = node.children[1];
	}

	return contents;
}


int CM_InlineModelContents( int index )
{
	cmodel_t *pModel = CM_InlineModelNumber( index );
	if ( !pModel )
		return 0;

	return CM_BrushContents_r( GetCollisionBSPData(), pModel->headnode );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int	CM_NumClusters( void )
{
	return GetCollisionBSPData()->numclusters;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
char *CM_EntityString( void )
{
	return GetCollisionBSPData()->map_entitystring.Get();
}

void CM_DiscardEntityString( void )
{
	GetCollisionBSPData()->map_entitystring.Discard();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int	CM_LeafContents( int leafnum )
{
	const CCollisionBSPData *pBSPData = GetCollisionBSPData();

	Assert( leafnum >= 0 );
	Assert( leafnum < pBSPData->numleafs );

	return pBSPData->map_leafs[leafnum].contents;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int	CM_LeafCluster( int leafnum )
{
	const CCollisionBSPData *pBSPData = GetCollisionBSPData();

	Assert( leafnum >= 0 );
	Assert( leafnum < pBSPData->numleafs );

	return pBSPData->map_leafs[leafnum].cluster;
}


int	CM_LeafFlags( int leafnum )
{
	const CCollisionBSPData *pBSPData = GetCollisionBSPData();

	Assert( leafnum >= 0 );
	Assert( leafnum < pBSPData->numleafs );

	return pBSPData->map_leafs[leafnum].flags;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int	CM_LeafArea( int leafnum )
{
	const CCollisionBSPData *pBSPData = GetCollisionBSPData();

	Assert( leafnum >= 0 );
	Assert( leafnum < pBSPData->numleafs );

	return pBSPData->map_leafs[leafnum].area;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CM_FreeMap(void)
{
	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	// free the collision bsp data
	CollisionBSPData_Destroy( pBSPData );
}


// This turns on all the area portals that are "always on" in the map.
void CM_InitPortalOpenState( CCollisionBSPData *pBSPData )
{
	for ( int i=0; i < pBSPData->numportalopen; i++ )
	{
		pBSPData->portalopen[i] = false;
	}
}


void CM_RegisterPaintMap( CCollisionBSPData *pBSPData )
{
	char* paintStr = V_stristr( pBSPData->map_entitystring.Get(), "paintinmap" );
	bool bMapHasPaint = false;
	if ( paintStr )
	{
		KeyValues *kv = KeyValues::FromString( "", paintStr );
		KeyValues * firstVal =  kv->GetFirstValue();

		const char* val = firstVal->GetString();

		bMapHasPaint = val[1] == '1';

		kv->deleteThis();
	}

	g_PaintManager.m_bShouldRegister = bMapHasPaint;
}


/*
==================
CM_LoadMap

Loads in the map and all submodels
==================
*/
cmodel_t *CM_LoadMap( const char *pPathName, bool allowReusePrevious, texinfo_t *pTexinfo, int texinfoCount, unsigned *checksum )
{
	static unsigned	int last_checksum = 0xFFFFFFFF;

	// get the current bsp -- there is currently only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	Assert( physcollision );

	if( !strcmp( pBSPData->mapPathName, pPathName ) && allowReusePrevious )
	{
		*checksum = last_checksum;
		return &pBSPData->map_cmodels[0];		// still have the right version
	}

	// only pre-load if the map doesn't already exist
	CollisionBSPData_PreLoad( pBSPData );

	if ( !pPathName || !pPathName[0] )
	{
		*checksum = 0;
		return &pBSPData->map_cmodels[0];			// cinematic servers won't have anything at all
	}

	// read in the collision model data
	CMapLoadHelper::Init( 0, pPathName );
	CollisionBSPData_Load( pPathName, pBSPData, pTexinfo, texinfoCount );
	CMapLoadHelper::Shutdown( );

    // Push the displacement bounding boxes down the tree and set leaf data.
    CM_DispTreeLeafnum( pBSPData );

	CM_InitPortalOpenState( pBSPData );
	FloodAreaConnections( pBSPData );
	CM_RegisterPaintMap( pBSPData );

#ifdef COUNT_COLLISIONS
	// initialize counters
	CollisionCounts_Init( &g_CollisionCounts );
#endif

	return &pBSPData->map_cmodels[0];
}


//-----------------------------------------------------------------------------
//
// Methods associated with colliding against the world + models
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// returns a vcollide that can be used to collide against this model
//-----------------------------------------------------------------------------
vcollide_t* CM_VCollideForModel( int modelindex, const model_t* pModel )
{
	switch( pModel->type )
	{
	case mod_brush:
		return CM_GetVCollide( modelindex-1 );
	case mod_studio:
		Assert( modelloader->IsLoaded( pModel ) );
		return g_pMDLCache->GetVCollide( pModel->studio );
	}

	return 0;
}




//=======================================================================

/*
==================
CM_PointLeafnum_r

==================
*/
int CM_PointLeafnumMinDistSqr_r( CCollisionBSPData *pBSPData, const Vector& p, int num, float &minDistSqr )
{
	float		d;
	cnode_t		*node;
	cplane_t	*plane;

	while (num >= 0)
	{
		node = pBSPData->map_rootnode + num;
		plane = node->plane;

		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct (plane->normal, p) - plane->dist;

		minDistSqr = fpmin( d*d, minDistSqr );
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

#ifdef COUNT_COLLISIONS
	g_CollisionCounts.m_PointContents++;			// optimize counter
#endif

	return -1 - num;
}

int CM_PointLeafnum_r( CCollisionBSPData *pBSPData, const Vector& p, int num)
{
	float		d;
	cnode_t		*node;
	cplane_t	*plane;

	while (num >= 0)
	{
		node = pBSPData->map_rootnode + num;
		plane = node->plane;

		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct (plane->normal, p) - plane->dist;
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

#ifdef COUNT_COLLISIONS
	g_CollisionCounts.m_PointContents++;			// optimize counter
#endif

	return -1 - num;
}

int CM_PointLeafnum (const Vector& p)
{
	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	if (!pBSPData->numplanes)
		return 0;		// sound may call this without map loaded
	return CM_PointLeafnum_r (pBSPData, p, 0);
}

void CM_SnapPointToReferenceLeaf_r( CCollisionBSPData *pBSPData, const Vector& p, int num, float tolerance, Vector *pSnapPoint )
{
	float		d, snapDist;
	cnode_t		*node;
	cplane_t	*plane;

	while (num >= 0)
	{
		node = pBSPData->map_rootnode + num;
		plane = node->plane;

		if (plane->type < 3)
		{
			d = p[plane->type] - plane->dist;
			snapDist = (*pSnapPoint)[plane->type] - plane->dist;
		}
		else
		{
			d = DotProduct (plane->normal, p) - plane->dist;
			snapDist = DotProduct (plane->normal, *pSnapPoint) - plane->dist;
		}

		if (d < 0)
		{
			num = node->children[1];
			if ( snapDist > 0 )
			{
				*pSnapPoint -= plane->normal * (snapDist + tolerance);
			}
		}
		else
		{
			num = node->children[0];
			if ( snapDist < 0 )
			{
				*pSnapPoint += plane->normal * (-snapDist + tolerance);
			}
		}
	}
}

void CM_SnapPointToReferenceLeaf(const Vector &referenceLeafPoint, float tolerance, Vector *pSnapPoint)
{
	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	if (pBSPData->numplanes)
	{
		CM_SnapPointToReferenceLeaf_r(pBSPData, referenceLeafPoint, 0, tolerance, pSnapPoint);
	}
}


/*
=============
CM_BoxLeafnums

Fills in a list of all the leafs touched
=============
*/
struct leafnums_t
{
	int		leafTopNode;
	int		leafMaxCount;
	int		*pLeafList;
	CCollisionBSPData *pBSPData;
};



int CM_BoxLeafnums( leafnums_t &context, const Vector &center, const Vector &extents, int nodenum )
{
	int leafCount = 0;
	const int NODELIST_MAX = 1024;
	int nodeList[NODELIST_MAX];
	int nodeReadIndex = 0;
	int nodeWriteIndex = 0;
	cplane_t	*plane;
	cnode_t		*node;
	int prev_topnode = -1;

	while (1)
	{
		if (nodenum < 0)
		{
			// This handles the case when the box lies completely
			// within a single node. In that case, the top node should be
			// the parent of the leaf
			if (context.leafTopNode == -1)
				context.leafTopNode = prev_topnode;

			if (leafCount < context.leafMaxCount)
			{
				context.pLeafList[leafCount] = -1 - nodenum;
				leafCount++;
			}
			if ( nodeReadIndex == nodeWriteIndex )
				return leafCount;
			nodenum = nodeList[nodeReadIndex];
			nodeReadIndex = (nodeReadIndex+1) & (NODELIST_MAX-1);
		}
		else
		{
			node = &context.pBSPData->map_rootnode[nodenum];
			plane = node->plane;
			//		s = BoxOnPlaneSide (leaf_mins, leaf_maxs, plane);
			//		s = BOX_ON_PLANE_SIDE(*leaf_mins, *leaf_maxs, plane);
			float d0 = DotProduct( plane->normal, center ) - plane->dist;
			float d1 = DotProductAbs( plane->normal, extents );
			prev_topnode = nodenum;
			if (d0 >= d1)
				nodenum = node->children[0];
			else if (d0 < -d1)
				nodenum = node->children[1];
			else
			{	// go down both
				if (context.leafTopNode == -1)
					context.leafTopNode = nodenum;
				nodeList[nodeWriteIndex] = node->children[0];
				nodeWriteIndex = (nodeWriteIndex+1) & (NODELIST_MAX-1);
				// check for overflow of the ring buffer
				Assert(nodeWriteIndex != nodeReadIndex);
				nodenum = node->children[1];
			}
		}
	}
}

int	CM_BoxLeafnums ( const Vector& mins, const Vector& maxs, int *list, int listsize, int *topnode, int cmodelIndex )
{
	leafnums_t context;
	context.pLeafList = list;
	context.leafTopNode = -1;
	context.leafMaxCount = listsize;
	// get the current collision bsp -- there is only one!
	context.pBSPData = GetCollisionBSPData();
	Vector center = (mins+maxs)*0.5f;
	Vector extents = maxs - center;
	AssertMsg( cmodelIndex >= 0 && cmodelIndex < context.pBSPData->numcmodels, "Collision model index out of bounds." );
	int leafCount = 0;
	if( cmodelIndex >= 0 && cmodelIndex < context.pBSPData->numcmodels )
		leafCount = CM_BoxLeafnums(context, center, extents, context.pBSPData->map_cmodels[cmodelIndex].headnode );

	if( topnode )
		*topnode = context.leafTopNode;

	return leafCount;
}

// UNDONE: This is a version that returns only leaves with valid clusters
// UNDONE: Use this in the PVS calcs for networking
#if 0
int CM_BoxClusters( leafnums_t * RESTRICT pContext, const Vector &center, const Vector &extents, int nodenum )
{
	const int NODELIST_MAX = 1024;
	int nodeList[NODELIST_MAX];
	int nodeReadIndex = 0;
	int nodeWriteIndex = 0;
	cplane_t *RESTRICT plane;
	cnode_t	 *RESTRICT node;
	int prev_topnode = -1;
	int leafCount = 0;
	while (1)
	{
		if (nodenum < 0)
		{
			int leafIndex = -1 - nodenum;
			// This handles the case when the box lies completely
			// within a single node. In that case, the top node should be
			// the parent of the leaf
			if (pContext->leafTopNode == -1)
				pContext->leafTopNode = prev_topnode;

			if (leafCount < pContext->leafMaxCount)
			{
				cleaf_t *RESTRICT pLeaf = &pContext->pBSPData->map_leafs[leafIndex];
				if ( pLeaf->cluster >= 0 )
				{
					pContext->pLeafList[leafCount] = leafIndex;
					leafCount++;
				}
			}
			if ( nodeReadIndex == nodeWriteIndex )
				return leafCount;
			nodenum = nodeList[nodeReadIndex];
			nodeReadIndex = (nodeReadIndex+1) & (NODELIST_MAX-1);
		}
		else
		{
			node = &pContext->pBSPData->map_rootnode[nodenum];
			plane = node->plane;
			float d0 = DotProduct( plane->normal, center ) - plane->dist;
			float d1 = DotProductAbs( plane->normal, extents );
			prev_topnode = nodenum;
			if (d0 >= d1)
				nodenum = node->children[0];
			else if (d0 < -d1)
				nodenum = node->children[1];
			else
			{	// go down both
				if (pContext->leafTopNode == -1)
					pContext->leafTopNode = nodenum;
				nodenum = node->children[0];
				nodeList[nodeWriteIndex] = node->children[1];
				nodeWriteIndex = (nodeWriteIndex+1) & (NODELIST_MAX-1);
				// check for overflow of the ring buffer
				Assert(nodeWriteIndex != nodeReadIndex);
			}
		}
	}
}

int	CM_BoxClusters_headnode ( CCollisionBSPData *pBSPData, const Vector& mins, const Vector& maxs, int *list, int listsize, int nodenum, int *topnode)
{
	leafnums_t context;
	context.pLeafList = list;
	context.leafTopNode = -1;
	context.leafMaxCount = listsize;
	Vector center = 0.5f * (mins + maxs);
	Vector extents = maxs - center;
	context.pBSPData = pBSPData;

	int leafCount = CM_BoxClusters( &context, center, extents, nodenum );
	if (topnode)
		*topnode = context.leafTopNode;

	return leafCount;
}
#endif

/*
==================
CM_PointContents

==================
*/

int CM_PointContents ( const Vector &p, int headnode, int contentsMask )
{
	int		l;

	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	if (!pBSPData->numnodes )	// map not loaded
		return 0;

	if ( !(pBSPData->allcontents & contentsMask) )
	{
		return 0;
	}

	l = CM_PointLeafnum_r (pBSPData, p, headnode);

	// iterate the leaf brushes and check for intersection with each one
	cleaf_t &leaf = pBSPData->map_leafs[l];
	if ( leaf.cluster < 0 )
		return leaf.contents;

	int nContents = 0;
	const unsigned short *pBrushList = &pBSPData->map_leafbrushes[leaf.firstleafbrush];

	for ( int i = 0; i < leaf.numleafbrushes; i++ )
	{
		cbrush_t *pBrush = &pBSPData->map_brushes[ pBrushList[i] ];
		// only consider brushes that have contents
		if ( !pBrush->contents )
			continue;

		if ( pBrush->IsBox() )
		{
			// special case for box brush
			cboxbrush_t *pBox = &pBSPData->map_boxbrushes[pBrush->GetBox()];
			if ( IsPointInBox( p, pBox->mins, pBox->maxs ) )
			{
				nContents |= pBrush->contents;
			}
		}
		else
		{
			// must be on the back of each brush side to be inside, skip bevels because they aren't necessary for testing points
			cbrushside_t *  RESTRICT pSide = &pBSPData->map_brushsides[pBrush->firstbrushside];
			bool bInside = true;
			for ( const cbrushside_t * const pSideLimit = pSide + pBrush->numsides; pSide < pSideLimit; pSide++ )
			{
				if ( pSide->bBevel )
					continue;
				float flDist = DotProduct( pSide->plane->normal, p ) - pSide->plane->dist;
				// outside plane, no intersection
				if ( flDist > 0.0f )
				{
					bInside = false;
					break;
				}
			}

			if ( bInside )
			{
				nContents |= pBrush->contents;
			}
		}
	}
	// point wasn't inside any brushes so return empty
	return nContents;
}


/*
==================
CM_TransformedPointContents

Handles offseting and rotation of the end points for moving and
rotating entities
==================
*/
int	CM_TransformedPointContents ( const Vector& p, int headnode, const Vector& origin, QAngle const& angles)
{
	Vector		p_l;
	Vector		temp;
	Vector		forward, right, up;
	int			l;

	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	// subtract origin offset
	VectorSubtract (p, origin, p_l);

	// rotate start and end into the models frame of reference
	if ( angles[0] || angles[1] || angles[2] )
	{
		AngleVectors (angles, &forward, &right, &up);

		VectorCopy (p_l, temp);
		p_l[0] = DotProduct (temp, forward);
		p_l[1] = -DotProduct (temp, right);
		p_l[2] = DotProduct (temp, up);
	}

	l = CM_PointLeafnum_r (pBSPData, p_l, headnode);

	return pBSPData->map_leafs[l].contents;
}


// does this box brush occlude the remaining polygon completely?
bool OccludeWithBoxBrush( COcclusionInfo &oi, const cbrush_t *pBrush, cboxbrush_t *pBox, const char * &pCodePath)
{
	// This version works with expanded box. TODO: Implement checking 8 rays


	// Constrain the brush to the local region of the cast. To be more conservative, it should be extents...deltaPos-extents, but 0..deltaPos should be good enough, since players shouldn't penetrate walls
	if ( pBox->mins.x < oi.m_traceMins.x
	  && pBox->mins.y < oi.m_traceMins.y
	  && pBox->mins.z < oi.m_traceMins.z )
	{
		pCodePath = "before_start";
		return false;  // brush is somewhere outside of our region of interest: if it occludes the view, it would mean we're partially embedded into this brush, so we'll just say we ignore brushes we're embedded into
	}

	if ( pBox->maxs.x > oi.m_traceMaxs.x
	  && pBox->maxs.y > oi.m_traceMaxs.y
	  && pBox->maxs.z > oi.m_traceMaxs.z )
	{
		pCodePath = "after_end";
		return false;  // brush is somewhere outside of our region of interest: if it occludes the view, it would mean we're partially embedded into this brush, so we'll just say we ignore brushes we're embedded into
	}

	Vector vDeltaBrush = ( pBox->maxs + pBox->mins ) * 0.5f - oi.m_start;
	// we operate in the +++ octant
	Vector vDeltaBrushPos = ScaleVector( vDeltaBrush, oi.m_deltaSigns );
	Vector vBrushExtent = ( pBox->maxs - pBox->mins ) * 0.5f;

	//float xy = fabsf( oi.m_delta.x * oi.m_delta.y ) , yz = fabsf( oi.m_delta.y * oi.m_delta.z ), zx = fabsf( oi.m_delta.z * oi.m_delta.x ); // +++ octant values
	Vector uvwBrushDist = CrossProduct( oi.m_deltaPos, vDeltaBrushPos ); // this is {(x^d)*b, (y^d)*b, (z^d)*b} = {x*(d^b),y*(d^b),z*(d^b)} where b = delta brush, d = delta (in positive octant)
	Vector uvwBrushExtents( oi.m_deltaPos.y * vBrushExtent.z + oi.m_deltaPos.z * vBrushExtent.y, oi.m_deltaPos.z * vBrushExtent.x + oi.m_deltaPos.x * vBrushExtent.z, oi.m_deltaPos.x * vBrushExtent.y + oi.m_deltaPos.y * vBrushExtent.x ); // all extents and their abs projections should be positive or zero

/*
	// does the brush completely occlude oi.m_extents?
	if ( fabsf( uvwBrushDist.x ) > uvwBrushExtents.x - oi.m_uvwExtents.x
	  || fabsf( uvwBrushDist.y ) > uvwBrushExtents.y - oi.m_uvwExtents.y
	  || fabsf( uvwBrushDist.z ) > uvwBrushExtents.z - oi.m_uvwExtents.z
		)
	{
		return false;
	}
*/

	Vector uvwBrushMins = uvwBrushDist - uvwBrushExtents, uvwBrushMaxs = uvwBrushDist + uvwBrushExtents;
	// boolean subtraction: oi.uvw - uvwBrush. If we have nothing left in U,V and W, then we've got a full occluder - return true!
	// if we have something left in only one dimension, cut that dimension and continue - we have a simple partial occluder that can be considered an infinite stripe
	// if we have something left in more than one dimension, then we can't cut anything because the result is a concave polygon, and it's expensive to deal with concave polygons
	Vector uvwNewMins = oi.m_uvwMins;
	Vector uvwNewMaxs = oi.m_uvwMaxs;
	int nEmpties = 0;
	for ( int i = 0; i < 3; ++i )
	{
		if ( uvwNewMins[ i ] >= uvwBrushMins[ i ] )
			uvwNewMins[ i ] = Max( uvwNewMins[ i ], uvwBrushMaxs[ i ] );
		else if ( uvwNewMaxs[ i ] <= uvwBrushMaxs[ i ] )
			uvwNewMaxs[ i ] = Min( uvwNewMaxs[ i ], uvwBrushMins[ i ] );
		else
			continue;
		if ( uvwNewMins[ i ] >= uvwNewMaxs[ i ] )
			nEmpties |= 1 << i;
	}
	if ( oi.m_pResults )
	{
		// The shadow is ended by this box brush, so the end is enclosed in it... very conservative, horrible approximation! this will crash and burn because the boxes are so big, so I need to write another code path for this case
		oi.m_pResults->vEndMin = pBox->mins;
		oi.m_pResults->vEndMax = pBox->maxs;
	}

	switch ( nEmpties )
	{
	case 7:
		pCodePath = "full_occluder";
		return true;	// Success - we found our full occluder!
	case 6:
		pCodePath = "partial_occluder_Uyz";
		oi.m_uvwMins.x = uvwNewMins.x;
		oi.m_uvwMaxs.x = uvwNewMaxs.x;
		break;
	case 5:
		pCodePath = "partial_occluder_Vzx";
		oi.m_uvwMins.y = uvwNewMins.y;
		oi.m_uvwMaxs.y = uvwNewMaxs.y;
		break;
	case 3:
		pCodePath = "partial_occluder_Wxy";
		oi.m_uvwMins.z = uvwNewMins.z;
		oi.m_uvwMaxs.z = uvwNewMaxs.z;
		break;
	default:
		pCodePath = "skipped";
		return false;
	}
	return false;
}

/*
===============================================================================

BOX TRACING

===============================================================================
*/

// Custom SIMD implementation for box brushes

const fltx4 Four_DistEpsilons={DIST_EPSILON,DIST_EPSILON,DIST_EPSILON,DIST_EPSILON};
const int32 ALIGN16 g_CubeFaceIndex0[4] ALIGN16_POST = {0,1,2,-1};
const int32 ALIGN16 g_CubeFaceIndex1[4] ALIGN16_POST = {3,4,5,-1};
bool IntersectRayWithBoxBrush( TraceInfo_t *pTraceInfo, const cbrush_t *pBrush, cboxbrush_t *pBox )
{
	// Load the unaligned ray/box parameters into SIMD registers
	fltx4 start = LoadUnaligned3SIMD(pTraceInfo->m_start.Base());
	fltx4 extents = LoadUnaligned3SIMD(pTraceInfo->m_extents.Base());
	fltx4 delta = LoadUnaligned3SIMD(pTraceInfo->m_delta.Base());
	fltx4 boxMins = LoadAlignedSIMD( pBox->mins.Base() );
	fltx4 boxMaxs = LoadAlignedSIMD( pBox->maxs.Base() );

	// compute the mins/maxs of the box expanded by the ray extents
	// relocate the problem so that the ray start is at the origin.
	fltx4 offsetMins = SubSIMD(boxMins, start);
	fltx4 offsetMaxs = SubSIMD(boxMaxs, start);
	fltx4 offsetMinsExpanded = SubSIMD( offsetMins, extents );
	fltx4 offsetMaxsExpanded = AddSIMD( offsetMaxs, extents );

	// Check to see if both the origin (start point) and the end point (delta) are on the front side
	// of any of the box sides - if so there can be no intersection
	bi32x4 startOutMins = CmpLtSIMD(Four_Zeros, offsetMinsExpanded);
	bi32x4 endOutMins = CmpLtSIMD(delta,offsetMinsExpanded);
	bi32x4 minsMask = AndSIMD( startOutMins, endOutMins );
	bi32x4 startOutMaxs = CmpGtSIMD(Four_Zeros, offsetMaxsExpanded);
	bi32x4 endOutMaxs = CmpGtSIMD(delta,offsetMaxsExpanded);
	bi32x4 maxsMask = AndSIMD( startOutMaxs, endOutMaxs );
	if ( IsAnyTrue(SetWToZeroSIMD(OrSIMD(minsMask,maxsMask))))
		return false;

	bi32x4 crossPlane = OrSIMD(XorSIMD(startOutMins,endOutMins), XorSIMD(startOutMaxs,endOutMaxs));
	// now build the per-axis interval of t for intersections
	fltx4 invDelta = LoadUnaligned3SIMD(pTraceInfo->m_invDelta.Base());
	fltx4 tmins = MulSIMD( offsetMinsExpanded, invDelta );
	fltx4 tmaxs = MulSIMD( offsetMaxsExpanded, invDelta );
	// now sort the interval per axis
	fltx4 mint = MinSIMD( tmins, tmaxs );
	fltx4 maxt = MaxSIMD( tmins, tmaxs );
	// only axes where we cross a plane are relevant
	mint = MaskedAssign( crossPlane, mint, Four_Negative_FLT_MAX );
	maxt = MaskedAssign( crossPlane, maxt, Four_FLT_MAX );

	// now find the intersection of the intervals on all axes
	fltx4 firstOut = FindLowestSIMD3(maxt);
	fltx4 lastIn = FindHighestSIMD3(mint);
	// NOTE: This is really a scalar quantity now [t0,t1] == [lastIn,firstOut]
	firstOut = MinSIMD(firstOut, Four_Ones);
	lastIn = MaxSIMD(lastIn, Four_Zeros);

	// If the final interval is valid lastIn<firstOut, check for separation
	bi32x4 separation = CmpGtSIMD(lastIn, firstOut);

	if ( IsAllZeros(separation) )
	{
		bool bStartOut = IsAnyTrue(SetWToZeroSIMD(OrSIMD(startOutMins,startOutMaxs)));
		offsetMinsExpanded = SubSIMD(offsetMinsExpanded, Four_DistEpsilons);
		offsetMaxsExpanded = AddSIMD(offsetMaxsExpanded, Four_DistEpsilons);

		fltx4 tmins = MulSIMD( offsetMinsExpanded, invDelta );
		fltx4 tmaxs = MulSIMD( offsetMaxsExpanded, invDelta );

		fltx4 minface0 = LoadAlignedSIMD( (float *) g_CubeFaceIndex0 );
		fltx4 minface1 = LoadAlignedSIMD( (float *) g_CubeFaceIndex1 );
		bi32x4 faceMask = CmpLeSIMD( tmins, tmaxs );
		fltx4 mint = MinSIMD( tmins, tmaxs );
		fltx4 maxt = MaxSIMD( tmins, tmaxs );
		fltx4 faceId = MaskedAssign( faceMask, minface0, minface1 );
		// only axes where we cross a plane are relevant
		mint = MaskedAssign( crossPlane, mint, Four_Negative_FLT_MAX );
		maxt = MaskedAssign( crossPlane, maxt, Four_FLT_MAX );

		fltx4 firstOutTmp = FindLowestSIMD3(maxt);

		// implement FindHighest of 3, but use intermediate masks to find the
		// corresponding index in faceId to the highest at the same time
		fltx4 compareOne = RotateLeft( mint );
		faceMask = CmpGtSIMD(mint, compareOne);
		// compareOne is [y,z,G,x]
		fltx4 max_xy = MaxSIMD( mint, compareOne );
		fltx4 faceRot = RotateLeft(faceId);
		fltx4 faceId_xy = MaskedAssign(faceMask, faceId, faceRot);
		// max_xy is [max(x,y), ... ]
		compareOne = RotateLeft2( mint );
		faceRot = RotateLeft2(faceId);
		// compareOne is [z, G, x, y]
		faceMask = CmpGtSIMD( max_xy, compareOne );
		fltx4 max_xyz = MaxSIMD( max_xy, compareOne );
		faceId = MaskedAssign( faceMask, faceId_xy, faceRot );
		fltx4 lastInTmp = SplatXSIMD( max_xyz );

		fltx4 firstOut = MinSIMD(firstOutTmp, Four_Ones);
		fltx4 lastIn = MaxSIMD(lastInTmp, Four_Zeros);
		bi32x4 separation = CmpGtSIMD(lastIn, firstOut);
		Assert(IsAllZeros(separation));
		if ( IsAllZeros(separation) )
		{
			uint32 faceIndex = SubInt(faceId, 0);
			Assert(faceIndex<6);
			float t1 = SubFloat(lastIn,0);
			trace_t * RESTRICT pTrace = &pTraceInfo->m_trace;

			// this condition is copied from the brush case to avoid hitting an assert and
			// overwriting a previous start solid with a new shorter fraction
			if ( bStartOut && pTraceInfo->m_ispoint && pTrace->fractionleftsolid > t1 )
			{
				bStartOut = false;
			}

			if ( !bStartOut )
			{
				float t2 = SubFloat(firstOut,0);
				pTrace->startsolid = true;
				pTrace->contents = pBrush->contents;
				if ( t2 >= 1.0f )
				{
					pTrace->allsolid = true;
					pTrace->fraction = 0.0f;
				}
				else if ( t2 > pTrace->fractionleftsolid )
				{
					pTrace->fractionleftsolid = t2;
					if (pTrace->fraction <= t2)
					{
						pTrace->fraction = 1.0f;
						pTrace->surface = pTraceInfo->m_pBSPData->nullsurface;
					}
				}
			}
			else
			{
				static const int signbits[3]={1,2,4};
				if ( t1 < pTrace->fraction )
				{
					pTraceInfo->m_bDispHit = false;
					pTrace->fraction = t1;
					pTrace->plane.normal = vec3_origin;
					pTrace->surface = *pTraceInfo->m_pBSPData->GetSurfaceAtIndex( pBox->surfaceIndex[faceIndex] );
					pTrace->worldSurfaceIndex = pBox->surfaceIndex[ faceIndex ];
					if ( faceIndex >= 3 )
					{
						faceIndex -= 3;
						pTrace->plane.dist = pBox->maxs[faceIndex];
						pTrace->plane.normal[faceIndex] = 1.0f;
						pTrace->plane.signbits = 0;
					}
					else
					{
						pTrace->plane.dist = -pBox->mins[faceIndex];
						pTrace->plane.normal[faceIndex] = -1.0f;
						pTrace->plane.signbits = signbits[faceIndex];
					}
					pTrace->plane.type = faceIndex;
					pTrace->contents = pBrush->contents;
					return true;
				}
			}
		}
	}
	return false;
}

// slightly different version of the above.  This folds in more of the trace_t output because CM_ComputeTraceEndpts() isn't called after this
// so this routine needs to properly compute start/end points and fractions in all cases
bool IntersectRayWithBox( const Ray_t &ray, const VectorAligned &inInvDelta, const VectorAligned &inBoxMins, const VectorAligned &inBoxMaxs, trace_t *RESTRICT pTrace )
{
	// mark trace as not hitting
	pTrace->startsolid = false;
	pTrace->allsolid = false;
	pTrace->fraction = 1.0f;

	// Load the unaligned ray/box parameters into SIMD registers
	fltx4 start = LoadUnaligned3SIMD(ray.m_Start.Base());
	fltx4 extents = LoadUnaligned3SIMD(ray.m_Extents.Base());
	fltx4 delta = LoadUnaligned3SIMD(ray.m_Delta.Base());
	fltx4 boxMins = LoadAlignedSIMD( inBoxMins.Base() );
	fltx4 boxMaxs = LoadAlignedSIMD( inBoxMaxs.Base() );

	// compute the mins/maxs of the box expanded by the ray extents
	// relocate the problem so that the ray start is at the origin.
	fltx4 offsetMins = SubSIMD(boxMins, start);
	fltx4 offsetMaxs = SubSIMD(boxMaxs, start);
	fltx4 offsetMinsExpanded = SubSIMD(offsetMins, extents);
	fltx4 offsetMaxsExpanded = AddSIMD(offsetMaxs, extents);

	// Check to see if both the origin (start point) and the end point (delta) are on the front side
	// of any of the box sides - if so there can be no intersection
	bi32x4 startOutMins = CmpLtSIMD(Four_Zeros, offsetMinsExpanded);
	bi32x4 endOutMins = CmpLtSIMD(delta,offsetMinsExpanded);
	bi32x4 minsMask = AndSIMD( startOutMins, endOutMins );
	bi32x4 startOutMaxs = CmpGtSIMD(Four_Zeros, offsetMaxsExpanded);
	bi32x4 endOutMaxs = CmpGtSIMD(delta,offsetMaxsExpanded);
	bi32x4 maxsMask = AndSIMD( startOutMaxs, endOutMaxs );
	if ( IsAnyTrue(SetWToZeroSIMD(OrSIMD(minsMask,maxsMask))))
		return false;

	bi32x4 crossPlane = OrSIMD(XorSIMD(startOutMins,endOutMins), XorSIMD(startOutMaxs,endOutMaxs));
	// now build the per-axis interval of t for intersections
	fltx4 invDelta = LoadAlignedSIMD(inInvDelta.Base());
	fltx4 tmins = MulSIMD( offsetMinsExpanded, invDelta );
	fltx4 tmaxs = MulSIMD( offsetMaxsExpanded, invDelta );
	// now sort the interval per axis
	fltx4 mint = MinSIMD( tmins, tmaxs );
	fltx4 maxt = MaxSIMD( tmins, tmaxs );
	// only axes where we cross a plane are relevant
	mint = MaskedAssign( crossPlane, mint, Four_Negative_FLT_MAX );
	maxt = MaskedAssign( crossPlane, maxt, Four_FLT_MAX );

	// now find the intersection of the intervals on all axes
	fltx4 firstOut = FindLowestSIMD3(maxt);
	fltx4 lastIn = FindHighestSIMD3(mint);
	// NOTE: This is really a scalar quantity now [t0,t1] == [lastIn,firstOut]
	firstOut = MinSIMD(firstOut, Four_Ones);
	lastIn = MaxSIMD(lastIn, Four_Zeros);

	// If the final interval is valid lastIn<firstOut, check for separation
	bi32x4 separation = CmpGtSIMD(lastIn, firstOut);

	if ( IsAllZeros(separation) )
	{
		bool bStartOut = IsAnyTrue(SetWToZeroSIMD(OrSIMD(startOutMins,startOutMaxs)));
		offsetMinsExpanded = SubSIMD(offsetMinsExpanded, Four_DistEpsilons);
		offsetMaxsExpanded = AddSIMD(offsetMaxsExpanded, Four_DistEpsilons);

		fltx4 tmins = MulSIMD( offsetMinsExpanded, invDelta );
		fltx4 tmaxs = MulSIMD( offsetMaxsExpanded, invDelta );

		fltx4 minface0 = LoadAlignedSIMD( (float *) g_CubeFaceIndex0 );
		fltx4 minface1 = LoadAlignedSIMD( (float *) g_CubeFaceIndex1 );
		bi32x4 faceMask = CmpLeSIMD( tmins, tmaxs );
		fltx4 mint = MinSIMD( tmins, tmaxs );
		fltx4 maxt = MaxSIMD( tmins, tmaxs );
		fltx4 faceId = MaskedAssign( faceMask, minface0, minface1 );
		// only axes where we cross a plane are relevant
		mint = MaskedAssign( crossPlane, mint, Four_Negative_FLT_MAX );
		maxt = MaskedAssign( crossPlane, maxt, Four_FLT_MAX );

		fltx4 firstOutTmp = FindLowestSIMD3(maxt);

		//fltx4 lastInTmp = FindHighestSIMD3(mint);
		// implement FindHighest of 3, but use intermediate masks to find the
		// corresponding index in faceId to the highest at the same time
		fltx4 compareOne = RotateLeft( mint );
		faceMask = CmpGtSIMD(mint, compareOne);
		// compareOne is [y,z,G,x]
		fltx4 max_xy = MaxSIMD( mint, compareOne );
		fltx4 faceRot = RotateLeft(faceId);
		fltx4 faceId_xy = MaskedAssign(faceMask, faceId, faceRot);
		// max_xy is [max(x,y), ... ]
		compareOne = RotateLeft2( mint );
		faceRot = RotateLeft2(faceId);
		// compareOne is [z, G, x, y]
		faceMask = CmpGtSIMD( max_xy, compareOne );
		fltx4 max_xyz = MaxSIMD( max_xy, compareOne );
		faceId = MaskedAssign( faceMask, faceId_xy, faceRot );
		fltx4 lastInTmp = SplatXSIMD( max_xyz );

		fltx4 firstOut = MinSIMD(firstOutTmp, Four_Ones);
		fltx4 lastIn = MaxSIMD(lastInTmp, Four_Zeros);
		bi32x4 separation = CmpGtSIMD(lastIn, firstOut);
		Assert(IsAllZeros(separation));
		if ( IsAllZeros(separation) )
		{
			uint32 faceIndex = SubInt(faceId, 0);
			Assert(faceIndex<6);
			float t1 = SubFloat(lastIn,0);

			// this condition is copied from the brush case to avoid hitting an assert and
			// overwriting a previous start solid with a new shorter fraction
			if ( bStartOut && ray.m_IsRay && pTrace->fractionleftsolid > t1 )
			{
				bStartOut = false;
			}

			if ( !bStartOut )
			{
				float t2 = SubFloat(firstOut,0);
				pTrace->startsolid = true;
				pTrace->contents = CONTENTS_SOLID;
				pTrace->fraction = 0.0f;
				pTrace->startpos = ray.m_Start + ray.m_StartOffset;
				pTrace->endpos = pTrace->startpos;
				if ( t2 >= 1.0f )
				{
					pTrace->allsolid = true;
				}
				else if ( t2 > pTrace->fractionleftsolid )
				{
					pTrace->fractionleftsolid = t2;
					pTrace->startpos += ray.m_Delta * pTrace->fractionleftsolid;
				}
				return true;
			}
			else
			{
				static const int signbits[3]={1,2,4};
				if ( t1 <= 1.0f )
				{
					pTrace->fraction = t1;
					pTrace->plane.normal = vec3_origin;
					if ( faceIndex >= 3 )
					{
						faceIndex -= 3;
						pTrace->plane.dist = inBoxMaxs[faceIndex];
						pTrace->plane.normal[faceIndex] = 1.0f;
						pTrace->plane.signbits = 0;
					}
					else
					{
						pTrace->plane.dist = -inBoxMins[faceIndex];
						pTrace->plane.normal[faceIndex] = -1.0f;
						pTrace->plane.signbits = signbits[faceIndex];
					}
					pTrace->plane.type = faceIndex;
					pTrace->contents = CONTENTS_SOLID;
					Vector start;
					VectorAdd( ray.m_Start, ray.m_StartOffset, start );

					if (pTrace->fraction == 1)
					{
						VectorAdd(start, ray.m_Delta, pTrace->endpos);
					}
					else
					{
						VectorMA( start, pTrace->fraction, ray.m_Delta, pTrace->endpos );
					}
					return true;
				}
			}
		}
	}
	return false;
}


//const fltx4 Four_DistEpsilons = { DIST_EPSILON, DIST_EPSILON, DIST_EPSILON, DIST_EPSILON };
const fltx4 Four_NegDistEpsilons = { -DIST_EPSILON, -DIST_EPSILON, -DIST_EPSILON, -DIST_EPSILON };
const fltx4 Four_NegEpsilons = { -FLT_EPSILON, -FLT_EPSILON, -FLT_EPSILON, -FLT_EPSILON };
const fltx4 Four_NegOnes = { -1.0, -1.0, -1.0, -1.0 };

FORCEINLINE fltx4 ShuffleABXY( const fltx4 &abcd, const fltx4 &xyzw )
{
	return _mm_shuffle_ps( abcd, xyzw, MM_SHUFFLE_REV( 0, 1, 0, 1 ) );
}

FORCEINLINE fltx4 ShuffleCDZW( const fltx4 &abcd, const fltx4 &xyzw )
{
	return _mm_shuffle_ps( abcd, xyzw, MM_SHUFFLE_REV( 2,3,2,3 ) );
}


FORCEINLINE fltx4 ShuffleXZYW( const fltx4 &xyzw )
{
	return _mm_shuffle_ps( xyzw, xyzw, MM_SHUFFLE_REV( 0,2,1,3 ) );
}
FORCEINLINE fltx4 ShuffleYWXZ( const fltx4 &xyzw )
{
	return _mm_shuffle_ps( xyzw, xyzw, MM_SHUFFLE_REV( 1, 3, 0, 2 ) );
}
FORCEINLINE fltx4 ShuffleZWXY( const fltx4 &xyzw )
{
	return _mm_shuffle_ps( xyzw, xyzw, MM_SHUFFLE_REV( 2, 3, 0, 1 ) );
}


FORCEINLINE fltx4 ShuffleYZWX( const fltx4 &xyzw )
{
	return _mm_shuffle_ps( xyzw, xyzw, MM_SHUFFLE_REV( 1, 2, 3, 0 ) );
}

FORCEINLINE fltx4 ShuffleAAXX( const fltx4 &abcd, const fltx4 &xyzw )
{
	return _mm_shuffle_ps( abcd, xyzw, MM_SHUFFLE_REV( 0,0, 0,0 ) );
}
FORCEINLINE fltx4 ShuffleCCZZ( const fltx4 &abcd, const fltx4 &xyzw )
{
	return _mm_shuffle_ps( abcd, xyzw, MM_SHUFFLE_REV( 2,2,2,2 ) );
}


FORCEINLINE int IntersectOcclusionInterval( fltx4 d0[ 2 ], fltx4 d1[ 2 ], fltx4 f4EnterNum[ 2 ], fltx4 f4EnterDenum[ 2 ], fltx4 f4LeaveNum[ 2 ], fltx4 f4LeaveDenum[ 2 ] )
{
	// We enter the polytope if d0-d1 >= 0, and leave if d0-d1 < 0. Note that the math works when d0-d1==0: we enter at -inf or +inf correctly. -inf is ignored, +inf means we never enter, the line is outside of the polytope and we can terminate. The same line of reasoning works with the Leave side
	fltx4 delta[ 2 ] = { d0[ 0 ] - d1[ 0 ], d0[ 1 ] - d1[ 1 ] };
	fltx4 isLeaving[ 2 ] = { CmpLtSIMD( delta[ 0 ], Four_Zeros ), CmpLtSIMD( delta[ 1 ], Four_Zeros ) };  // is the ray leaving the hemispace (moving outside the plane, moving along the normal; movement is from d0 to d1)?
	fltx4 gtEnter[ 2 ] = { CmpLtSIMD( f4EnterNum[ 0 ] * delta[ 0 ], d0[ 0 ] * f4EnterDenum[ 0 ] ), CmpLtSIMD( f4EnterNum[ 1 ] * delta[ 1 ], d0[ 1 ] * f4EnterDenum[ 1 ] ) }; // valid when is Entering (!isLeaving) only
	fltx4 ltLeave[ 2 ] = { CmpGtSIMD( f4LeaveNum[ 0 ] * delta[ 0 ], d0[ 0 ] * f4LeaveDenum[ 0 ] ), CmpGtSIMD( f4LeaveNum[ 1 ] * delta[ 1 ], d0[ 1 ] * f4LeaveDenum[ 1 ] ) }; // valid when isLeaving only : is the d0/delta < Leave? then we'll update the Leave value

	fltx4 maskNewLeave[ 2 ] = { AndSIMD( isLeaving[ 0 ], ltLeave[ 0 ] ), AndSIMD( isLeaving[ 1 ], ltLeave[ 1 ] ) };
	f4LeaveNum[ 0 ] = MaskedAssign( maskNewLeave[ 0 ], d0[ 0 ], f4LeaveNum[ 0 ] );
	f4LeaveNum[ 1 ] = MaskedAssign( maskNewLeave[ 1 ], d0[ 1 ], f4LeaveNum[ 1 ] );
	f4LeaveDenum[ 0 ] = MaskedAssign( maskNewLeave[ 0 ], delta[ 0 ], f4LeaveDenum[ 0 ] );
	f4LeaveDenum[ 1 ] = MaskedAssign( maskNewLeave[ 1 ], delta[ 1 ], f4LeaveDenum[ 1 ] );

	fltx4 maskNewEnter[ 2 ] = { AndNotSIMD( isLeaving[ 0 ], gtEnter[ 0 ] ), AndNotSIMD( isLeaving[ 1 ], gtEnter[ 1 ] ) };
	f4EnterNum[ 0 ] = MaskedAssign( maskNewEnter[ 0 ], d0[ 0 ], f4EnterNum[ 0 ] );
	f4EnterNum[ 1 ] = MaskedAssign( maskNewEnter[ 1 ], d0[ 1 ], f4EnterNum[ 1 ] );
	f4EnterDenum[ 0 ] = MaskedAssign( maskNewEnter[ 0 ], delta[ 0 ], f4EnterDenum[ 0 ] );
	f4EnterDenum[ 1 ] = MaskedAssign( maskNewEnter[ 1 ], delta[ 1 ], f4EnterDenum[ 1 ] );

	fltx4 maskEnterAfterLeave[ 2 ] = { CmpLtSIMD( f4EnterNum[ 0 ] * f4LeaveDenum[ 0 ], f4EnterDenum[ 0 ] * f4LeaveNum[ 0 ] ), CmpLtSIMD( f4EnterNum[ 1 ] * f4LeaveDenum[ 1 ], f4EnterDenum[ 1 ] * f4LeaveNum[ 1 ] ) };
	return TestSignSIMD( OrSIMD( maskEnterAfterLeave[ 0 ], maskEnterAfterLeave[ 1 ] ) );
	// non-0 means one of the rays enters after it leaves the polytope, i.e. it's not intersected by polytope, so exit early (ish)
	// non-0 means "not occluded"
}

static fltx4 Four_OneAndRcpMargin = { 1.001f, 1.001f, 1.001f, 1.001f };
static fltx4 Four_NegRcpMargin = { -0.001f, -0.001f, -0.001f, -0.001f };

bool CM_BrushOcclusionPass( COcclusionInfo &oi, const cbrush_t * RESTRICT brush )
{
	fltx4 fracHit[ 2 ] = { Four_Ones, Four_Ones };
	if ( brush->IsBox() )
	{
		cboxbrush_t *pBox = &oi.m_pBSPData->map_boxbrushes[ brush->GetBox() ];
		if ( oi.m_pDebugLog )
		{
			oi.m_pDebugLog->AddBox( CFmtStr( "hit%04d_box_brush%d", oi.m_pDebugLog->GetPrimCount(), brush->GetBox() ).Get(), "relevant", pBox->mins, pBox->maxs );
		}

/*
		if ( !oi.m_pResults )
		{
			const char *pCodePath;
			bool bFullyOccluded = OccludeWithBoxBrush( oi, brush, pBox, pCodePath );
			return bFullyOccluded;
		}
*/

		fltx4 f4Mins = LoadUnaligned3SIMD( &pBox->mins );
		fltx4 f4Maxs = LoadUnaligned3SIMD( &pBox->maxs );

		// d12_XnXYnY means d1 (the height of the end point), above the plane 2 (+Y: {+X,-X,+Y,-Y,+Z,-Z}[2]), kept in SIMD in the order {+X,-X,+Y,-Y}

		// for +X plane: (+Y is the same) 
		// d0 = start.x - max.x, d1  = end.x - max.x
		fltx4 maxXXYY = ShuffleXXYY( f4Maxs );
		fltx4 d0U = oi.m_StartXnXYnY - maxXXYY, d1U = oi.m_EndXnXYnY - maxXXYY; // U = 0_XnXYnY, plane {+X,+X,+Y,+Y} dot point {posX, negX, posY, negY}
		// for -X plane: (-Y is the same)
		// d0 = min.x - start.x, d1 = min.x - end.x
		fltx4 minXXYY = ShuffleXXYY( f4Mins );
		fltx4 d0V = minXXYY - oi.m_StartXnXYnY, d1V = minXXYY - oi.m_EndXnXYnY; // V = 1_XnXYnY, plane {-X,-X,-Y,-Y} dot point {posX, negX, posY, negY}
		// for +Z plane:
		// d01 = {start.Z - max.z, start.nZ - max.z, end.Z - max.z, end.nZ - max.z }
		fltx4 d01Z = oi.m_StartEndZnZ - SplatZSIMD( f4Maxs ), d01nZ = SplatZSIMD( f4Mins ) - oi.m_StartEndZnZ;
		fltx4 d0ZnZ = ShuffleABXY(d01Z, d01nZ), d1ZnZ = ShuffleCDZW(d01Z, d01nZ); // ZnZZnZ, plane { +Z, +Z, -Z, -Z } dot point {posZ, negZ, posZ, negZ}
		
		// every single ray/interval must lie, at least partially, behind each plane (negatively). At least one of d0 and d1, for both X and nX shifts, must be negative. 
		// If there's a single one interval with both d0 >= 0 and d1 >= 0 it means there's a ray that misses this box completely, and we don't have full occlusion
		if ( TestSignSIMD( AndSIMD( AndSIMD( OrSIMD( d0U, d1U ), OrSIMD( d0V, d1V ) ), OrSIMD( d0ZnZ, d1ZnZ ) ) ) != 15 )
		{
			// there's a separating axis along one of the orts, both d0 and d1 lie on one side of it
			return false;
		}

		// PERF: Even though the following code is gigantic, it's not executed for many boxes

		// We enter the polytope if d1-d2 >= 0, and leave if d1-d2 < 0. Note that the math works when d1-d2==0: we enter at -inf or +inf correctly. -inf is ignored, +inf means we never enter, the line is outside of the polytope and we can terminate. The same line of reasoning works with the Leave side
		{
			fltx4 deltaU = d0U - d1U; // this should be the same as deltaV, with flipped signs
			Assert( IsAllGreaterThanOrEq( Four_DistEpsilons, AbsSIMD( deltaU + ( d0V - d1V ) ) ) );
			fltx4 deltaZ = d0ZnZ - d1ZnZ; // same with +Z and -Z planes
			Assert( IsAllGreaterThanOrEq( Four_DistEpsilons, AbsSIMD( deltaZ + ShuffleZWXY( deltaZ ) ) ) );

			//fltx4 isDirPosU = CmpGtSIMD( deltaU, Four_Zeros );// , isDirNegU = CmpLtSIMD( deltaU, Four_NegEpsilons ); // Pos => use U over V; Neg => use V over U
			//fltx4 isDirPosZ = CmpGtSIMD( deltaZ, Four_Zeros );// , isDirNegZ = CmpLtSIMD( deltaZ, Four_NegEpsilons ); // note: this mask is valid in the XY and inverted in ZW
			//fltx4 deltaUabs = AbsSIMD( deltaU ), deltaZabs = AbsSIMD( deltaZ );
			//fltx4 isNonParallelU = CmpGtSIMD( deltaUabs, Four_Epsilons ), isNonParallelZ = CmpGtSIMD( deltaZabs, Four_Epsilons );
			fltx4 deltaUrcp = ReciprocalEstSIMD( deltaU ), deltaZrcp = ReciprocalEstSIMD( deltaZ );
			//fltx4 fracU = AndSIMD( isNonParallelU, MaskedAssign( isDirPosU, d0U, d0V ) * deltaUrcp ), fracZ = AndSIMD( isNonParallelZ, MaskedAssign( isDirPosZ, d0ZnZ, ShuffleZWXY( d0ZnZ ) ) * deltaZrcp );
			fltx4 f4Size = f4Maxs - f4Mins;

			fltx4 fracUa = d0U * deltaUrcp, fracUb = ( d0U + ShuffleXXYY( f4Size ) ) * deltaUrcp;
			fltx4 fracUmin = MinSIMD( fracUa, fracUb ), fracUmax = MaxSIMD( fracUa, fracUb );
			fltx4 fracZa = d01Z * deltaZrcp, fracZb = ( d01Z + SplatZSIMD( f4Size ) ) * deltaZrcp;
			fltx4 fracZmin = MinSIMD( fracZa, fracZb ), fracZmax = MaxSIMD( fracZa, fracZb );

			Assert( IsAllGreaterThanOrEq( Four_OneAndRcpMargin, MaxSIMD( fracUmin, fracZmin ) ) ); // at least one ray starts further than 1 along at least one dimension? This shouldn't happen
			// There may be large-ish round-off errors due to using reciprocal estimates (deltaUrcp and deltaZrcp)
			// There are 8 rays, in all combinations of +- with X, Y, Z - those are the intervals we'll need to intersect: 
			// +-,+-,+-: fracU01, fracU23, fracZ01
			// so, fracMin[2] = { { max(u0,u2,z0),max(u1,u2,z0),max(u0,u3,z0), max(u1, u3, z0) }, { max(u0,u2,z1),max(u1,u2,z1),max(u0,u3,z1), max(u1, u3, z1) } }
			fltx4 fracUminTmp = MaxSIMD( ShuffleXYXY( fracUmin ), ShuffleZZWW( fracUmin ) ), fracUmaxTmp = MinSIMD( ShuffleXYXY( fracUmax ), ShuffleZZWW( fracUmax ) );
			fracHit[ 0 ] = MaxSIMD( Four_Zeros, MaxSIMD( fracUminTmp, SplatXSIMD( fracZmin ) ) ); fracHit[ 1 ] = MaxSIMD( Four_Zeros, MaxSIMD( fracUminTmp, SplatYSIMD( fracZmin ) ) );
			fltx4 fracHitMax[ 2 ] = { MinSIMD( fracUmaxTmp, SplatXSIMD( fracZmax ) ), MinSIMD( fracUmaxTmp, SplatYSIMD( fracZmax ) ) };

			// now we need to intersect all found intervals, and if we end up with empty intersection or starting beyond +1, then there's at least one ray that doesn't get blocked and we have no full occlusion
			if ( TestSignSIMD( OrSIMD( fracHitMax[ 0 ] - fracHit[ 0 ], fracHitMax[ 1 ] - fracHit[ 1 ] ) ) )
			{
				// one of the intervals is empty, ray hasn't hit the box, exit
				return false;
			}
		}
	}
	else// support for non-box brushes
	{
		cbrushside_t *  RESTRICT pSidesBegin = &oi.m_pBSPData->map_brushsides[ brush->firstbrushside ], *pSide = pSidesBegin;
		if ( oi.m_pDebugLog )
		{
			oi.m_pDebugLog->AddBrush( CFmtStr( "hit%04d_brush%d_%dside", oi.m_pDebugLog->GetPrimCount(), brush->firstbrushside, brush->numsides ).Get(), "relevant", pSidesBegin, brush->numsides );
		}

		fltx4 f4EnterNum[ 2 ] = { Four_Zeros, Four_Zeros }, f4EnterDenum[ 2 ] = { Four_Ones, Four_Ones };
		fltx4 f4LeaveNum[ 2 ] = { Four_NegOnes, Four_NegOnes }, f4LeaveDenum[ 2 ] = { Four_NegOnes, Four_NegOnes };

		for ( const cbrushside_t * const pSidesEnd = pSide + brush->numsides; pSide < pSidesEnd; pSide++ )
		{
			// don't trace rays against bevel planes
			if ( pSide->bBevel )
				continue;
			DbgAssert( uintp( &pSide->plane->dist ) - uintp( &pSide->plane->normal ) == 12 ); // dist must follow the normal exactly
			fltx4 plane = LoadUnalignedSIMD( &pSide->plane->normal );
			//fltx4 dist = SplatWSIMD( plane );
			//fltx4 planeNormalX = SplatXSIMD( plane ), planeNormalY = SplatYSIMD( plane ), planeNormalZ = SplatZSIMD( plane );
			// fltx4 d0[ 2 ] = { oi.GetStartX() * planeNormalX + oi.GetStartY() * planeNormalY + oi.GetStartZ0() * planeNormalZ - dist, oi.GetStartX() * planeNormalX + oi.GetStartY() * planeNormalY + oi.GetStartZ1() * planeNormalZ - dist };
			fltx4 planeXXYY = ShuffleXXYY( plane );
			fltx4 startPlaneXnXYnY = oi.m_StartXnXYnY * planeXXYY, endPlaneXnXYnY = oi.m_EndXnXYnY * planeXXYY, startEndPlaneZnZ_dist = oi.m_StartEndZnZ * SplatZSIMD( plane ) - SplatWSIMD( plane );
			fltx4 d0xy = ShuffleXYXY( startPlaneXnXYnY ) + ShuffleZZWW( startPlaneXnXYnY );
			fltx4 d0[ 2 ] = { d0xy + SplatXSIMD( startEndPlaneZnZ_dist ), d0xy + SplatYSIMD( startEndPlaneZnZ_dist ) };
			fltx4 d1xy = ShuffleXYXY( endPlaneXnXYnY ) + ShuffleZZWW( endPlaneXnXYnY );
			fltx4 d1[ 2 ] = { d1xy + SplatZSIMD( startEndPlaneZnZ_dist ), d1xy + SplatWSIMD( startEndPlaneZnZ_dist ) };

			if ( ( TestSignSIMD( OrSIMD( d0[ 0 ], d1[ 0 ] ) ) & TestSignSIMD( OrSIMD( d0[ 1 ], d1[ 1 ] ) ) ) != 15 )
			{
				// we found the separating plane for one of the 8 rays.
				// That ray's d1 >= 0 && d2 >= 0, it lies completely outside the brush side, so it doesn't intersect the brush, guaranteed.
				// Therefore the whole probe is not occluded.
				// Therefore we can return false ("not occluded")
				return false;
			}

			if ( IntersectOcclusionInterval( d0, d1, f4EnterNum, f4EnterDenum, f4LeaveNum, f4LeaveDenum ) )
			{
				// non-0 means one of the rays enters after it leaves the polytope, i.e. it's not intersected by polytope, so exit early (ish)
				// non-0 means "not occluded"
				return false;
			}
		}

		fracHit[ 0 ] = f4EnterNum[ 0 ] * ReciprocalEstSIMD( f4EnterDenum[ 0 ] );
		fracHit[ 1 ] = f4EnterNum[ 1 ] * ReciprocalEstSIMD( f4EnterDenum[ 1 ] );
	};
	if ( oi.m_pResults )
	{
		// compute the bbox of the ends of the rays. This is only executed at most once per occlusion query, so it's not a critical path
		fltx4 fracAft[ 2 ] = { Four_Ones - fracHit[ 0 ], Four_Ones - fracHit[ 1 ] };
		Assert( IsAllGreaterThanOrEq( fracHit[ 0 ], Four_Zeros ) && IsAllGreaterThanOrEq( fracHit[ 1 ], Four_Zeros ) && IsAllGreaterThanOrEq( Four_OneAndRcpMargin, fracHit[ 0 ] ) && IsAllGreaterThanOrEq( Four_OneAndRcpMargin, fracHit[ 1 ] ) );
		Assert( IsAllGreaterThanOrEq( fracAft[ 0 ], Four_NegRcpMargin ) && IsAllGreaterThanOrEq( fracAft[ 1 ], Four_NegRcpMargin ) && IsAllGreaterThanOrEq( Four_Ones, fracAft[ 0 ] ) && IsAllGreaterThanOrEq( Four_Ones, fracAft[ 1 ] ) );
		// the 8 start points' coordinates will need to multiply by enter; the end points' coordinates will need to multiply by 1-enter
		fltx4 startX = oi.GetStartX(), endX = oi.GetEndX();
		fltx4 hitX[ 2 ] = { fracHit[ 0 ] * endX + fracAft[ 0 ] * startX, fracHit[ 1 ] * endX + fracAft[ 1 ] * startX };
		fltx4 minXXXX = MinSIMD( hitX[ 0 ], hitX[ 1 ] ), maxXXXX = MaxSIMD( hitX[ 0 ], hitX[ 1 ] ); // 4 values will need to be collapsed to the horizontal min/max in each register
		fltx4 startY = oi.GetStartY(), endY = oi.GetEndY();
		fltx4 hitY[ 2 ] = { fracHit[ 0 ] * endY + fracAft[ 0 ] * startY, fracHit[ 1 ] * endY + fracAft[ 1 ] * startY };
		fltx4 minYYYY = MinSIMD( hitY[ 0 ], hitY[ 1 ] ), maxYYYY = MaxSIMD( hitY[ 0 ], hitY[ 1 ] ); // 4 values will need to be collapsed to the horizontal min/max in each register
			
		fltx4 minXXYY = MinSIMD( ShuffleABXY( minXXXX, minYYYY ), ShuffleCDZW( minXXXX, minYYYY ) ), maxXXYY = MaxSIMD( ShuffleABXY( maxXXXX, maxYYYY ), ShuffleCDZW( maxXXXX, maxYYYY ) );
		fltx4 minXYXY = MinSIMD( ShuffleXZYW( minXXYY ), ShuffleYWXZ( minXXYY ) ), maxXYXY = MaxSIMD( ShuffleXZYW( maxXXYY ), ShuffleYWXZ( maxXXYY ) );

		fltx4 hitZ[ 2 ] = { fracHit[ 0 ] * oi.GetEndZ0() + fracAft[ 0 ] * oi.GetStartZ0(), fracHit[ 1 ] * oi.GetEndZ1() + fracAft[ 1 ] * oi.GetStartZ1() };
		fltx4 minZZZZ = MinSIMD( hitZ[ 0 ], hitZ[ 1 ] ), maxZZZZ = MaxSIMD( hitZ[ 0 ], hitZ[ 1 ] ); // 4 values will need to be collapsed to the horizontal min/max in each register
		fltx4 minZZ = MinSIMD( minZZZZ, ShuffleZWXY( minZZZZ ) ), maxZZ = MaxSIMD( maxZZZZ, ShuffleZWXY( maxZZZZ ) );
		fltx4 minZ = MinSIMD( minZZ, ShuffleYZWX( minZZ ) ), maxZ = MaxSIMD( maxZZ, ShuffleYZWX( maxZZ ) );
		fltx4 minXYZZ = ShuffleABXY( minXYXY, minZ );
		StoreAligned3SIMD( &oi.m_pResults->vEndMin, minXYZZ );
		fltx4 maxXYZZ = ShuffleABXY( maxXYXY, maxZ );
		StoreAligned3SIMD( &oi.m_pResults->vEndMax, maxXYZZ );
		Assert( IsAllGreaterThanOrEq( maxXYZZ, minXYZZ ) );
	}

	return true; // yes, we didn't exit early, so it means all rays are intersected by the polytope, which means we're completely occluded
}


/*
================
CM_ClipBoxToBrush
================
*/
template <bool IS_POINT>
void FASTCALL CM_ClipBoxToBrush( TraceInfo_t * RESTRICT pTraceInfo, const cbrush_t * RESTRICT brush )
{
	if ( brush->IsBox() )
	{
		cboxbrush_t *pBox = &pTraceInfo->m_pBSPData->map_boxbrushes[brush->GetBox()];
		IntersectRayWithBoxBrush( pTraceInfo, brush, pBox );
		return;
	}
	if (!brush->numsides)
		return;

	trace_t * RESTRICT trace = &pTraceInfo->m_trace;
	const Vector& p1 = pTraceInfo->m_start;
	const Vector& p2= pTraceInfo->m_end;
	int brushContents = brush->contents;

#ifdef COUNT_COLLISIONS
	g_CollisionCounts.m_BrushTraces++;
#endif

	float enterfrac = NEVER_UPDATED;
	float leavefrac = 1.f;

	bool getout = false;
	bool startout = false;
	cbrushside_t* leadside = NULL;

	float dist;

	cbrushside_t *  RESTRICT side = &pTraceInfo->m_pBSPData->map_brushsides[brush->firstbrushside];
	for ( const cbrushside_t * const sidelimit = side + brush->numsides; side < sidelimit; side++ )
	{
		cplane_t *plane = side->plane;
		const Vector &planeNormal = plane->normal;

		if (!IS_POINT)
		{
			// general box case
			// push the plane out apropriately for mins/maxs

			dist = DotProductAbs( planeNormal, pTraceInfo->m_extents );
			dist = plane->dist + dist;
		}
		else
		{
			// special point case
			dist = plane->dist;
			// don't trace rays against bevel planes
			if ( side->bBevel )
				continue;
		}

		float d1 = DotProduct (p1, planeNormal) - dist;
		float d2 = DotProduct (p2, planeNormal) - dist;

		// if completely in front of face, no intersection
		if( d1 > 0.f )
		{
			startout = true;

			// d1 > 0.f && d2 > 0.f
			if( d2 > 0.f )
				return;

		}
		else
		{
			// d1 <= 0.f && d2 <= 0.f
			if( d2 <= 0.f )
				continue;

			// d2 > 0.f
			getout = true;
		}

		// crosses face
		if (d1 > d2)
		{	// enter
			// NOTE: This could be negative if d1 is less than the epsilon.
			// If the trace is short (d1-d2 is small) then it could produce a large
			// negative fraction.
			float f = (d1-DIST_EPSILON);
			if ( f < 0.f )
				f = 0.f;
			f = f / (d1-d2);
			if (f > enterfrac)
			{
				enterfrac = f;
				leadside = side;
			}
		}
		else
		{	// leave
			float f = (d1+DIST_EPSILON) / (d1-d2);
			if (f < leavefrac)
				leavefrac = f;
		}
	}

	// when this happens, we entered the brush *after* leaving the previous brush.
	// Therefore, we're still outside!

	// NOTE: We only do this test against points because fractionleftsolid is
	// not possible to compute for brush sweeps without a *lot* more computation
	// So, client code will never get fractionleftsolid for box sweeps
	if (IS_POINT && startout)
	{
		// Add a little sludge.  The sludge should already be in the fractionleftsolid
		// (for all intents and purposes is a leavefrac value) and enterfrac values.
		// Both of these values have +/- DIST_EPSILON values calculated in.  Thus, I
		// think the test should be against "0.0."  If we experience new "left solid"
		// problems you may want to take a closer look here!
//		if ((trace->fractionleftsolid - enterfrac) > -1e-6)
		if ((trace->fractionleftsolid - enterfrac) > 0.0f )
			startout = false;
	}

	if (!startout)
	{	// original point was inside brush
		trace->startsolid = true;
		// return starting contents
		trace->contents = brushContents;

		if (!getout)
		{
			trace->allsolid = true;
			trace->fraction = 0.0f;
			trace->fractionleftsolid = 1.0f;
		}
		else
		{
			// if leavefrac == 1, this means it's never been updated or we're in allsolid
			// the allsolid case was handled above
			if ((leavefrac != 1) && (leavefrac > trace->fractionleftsolid))
			{
				trace->fractionleftsolid = leavefrac;

				// This could occur if a previous trace didn't start us in solid
				if (trace->fraction <= leavefrac)
				{
					trace->fraction = 1.0f;
					trace->surface = pTraceInfo->m_pBSPData->nullsurface;
				}
			}
		}
		return;
	}

	// We haven't hit anything at all until we've left...
	if (enterfrac < leavefrac)
	{
		if (enterfrac > NEVER_UPDATED && enterfrac < trace->fraction)
		{
			// WE HIT SOMETHING!!!!!
			if (enterfrac < 0)
				enterfrac = 0;
			trace->fraction = enterfrac;
			pTraceInfo->m_bDispHit = false;
			trace->plane = *(leadside->plane);
			trace->surface = *pTraceInfo->m_pBSPData->GetSurfaceAtIndex( leadside->surfaceIndex );
			trace->worldSurfaceIndex = leadside->surfaceIndex;
			trace->contents = brushContents;
		}
	}
}

inline bool IsTraceBoxIntersectingBoxBrush( TraceInfo_t *pTraceInfo, cboxbrush_t *pBox )
{
	fltx4 start = LoadUnaligned3SIMD(pTraceInfo->m_start.Base());
	fltx4 mins = LoadUnaligned3SIMD(pTraceInfo->m_mins.Base());
	fltx4 maxs = LoadUnaligned3SIMD(pTraceInfo->m_maxs.Base());

	fltx4 boxMins = LoadAlignedSIMD( pBox->mins.Base() );
	fltx4 boxMaxs = LoadAlignedSIMD( pBox->maxs.Base() );
	fltx4 offsetMins = AddSIMD(mins, start);
	fltx4 offsetMaxs = AddSIMD(maxs,start);
	fltx4 minsOut = MaxSIMD(boxMins, offsetMins);
	fltx4 maxsOut = MinSIMD(boxMaxs, offsetMaxs);
	bi32x4 separated = CmpGtSIMD(minsOut, maxsOut);
	bi32x4 sep3 = SetWToZeroSIMD(separated);
	return IsAllZeros(sep3);
}
/*
================
CM_TestBoxInBrush
================
*/
void FASTCALL CM_TestBoxInBrush( TraceInfo_t *pTraceInfo, const cbrush_t *brush )
{
	if ( brush->IsBox())
	{
		cboxbrush_t *pBox = &pTraceInfo->m_pBSPData->map_boxbrushes[brush->GetBox()];
		if ( !IsTraceBoxIntersectingBoxBrush( pTraceInfo, pBox ) )
			return;
	}
	else
	{
		if (!brush->numsides)
			return;
		const Vector& mins = pTraceInfo->m_mins;
		const Vector& maxs = pTraceInfo->m_maxs;
		const Vector& p1 = pTraceInfo->m_start;
		int			i, j;

		cplane_t	*plane;
		float		dist;
		Vector		ofs(0,0,0);
		float		d1;
		cbrushside_t	*side;

		for (i=0 ; i<brush->numsides ; i++)
		{
			side = &pTraceInfo->m_pBSPData->map_brushsides[brush->firstbrushside+i];
			plane = side->plane;

			// FIXME: special case for axial

			// general box case

			// push the plane out appropriately for mins/maxs

			// FIXME: use signbits into 8 way lookup for each mins/maxs
			for (j=0 ; j<3 ; j++)
			{
				if (plane->normal[j] < 0)
					ofs[j] = maxs[j];
				else
					ofs[j] = mins[j];
			}
			dist = DotProduct (ofs, plane->normal);
			dist = plane->dist - dist;

			d1 = DotProduct (p1, plane->normal) - dist;

			// if completely in front of face, no intersection
			if (d1 > 0)
				return;

		}
	}

	// inside this brush
	trace_t *trace = &pTraceInfo->m_trace;
	trace->startsolid = trace->allsolid = true;
	trace->fraction = 0;
	trace->fractionleftsolid = 1.0f;
	trace->contents = brush->contents;
}

template<bool IS_POINT, bool CHECK_COUNTERS>
FORCEINLINE_TEMPLATE void CM_TraceToDispList( TraceInfo_t * RESTRICT pTraceInfo, const unsigned short *pDispList, int dispListCount, float startFrac, float endFrac )
{
	VPROF("CM_TraceToDispList");
	//
	// trace ray/swept box against all displacement surfaces in this leaf
	//
	TraceCounter_t * RESTRICT pCounters = 0;
	TraceCounter_t count = 0;

	if ( CHECK_COUNTERS )
	{
		pCounters = pTraceInfo->GetDispCounters();
		count = pTraceInfo->GetCount();
	}

	if ( IsX360() || IsPS3() )
	{
		// set up some relatively constant variables we'll use in the loop below
		fltx4 traceStart = LoadUnaligned3SIMD(pTraceInfo->m_start.Base());
		fltx4 traceDelta = LoadUnaligned3SIMD(pTraceInfo->m_delta.Base());
		fltx4 traceInvDelta = LoadUnaligned3SIMD(pTraceInfo->m_invDelta.Base());
		static const fltx4 vecEpsilon = {DISPCOLL_DIST_EPSILON,DISPCOLL_DIST_EPSILON,DISPCOLL_DIST_EPSILON,DISPCOLL_DIST_EPSILON};
		// only used in !IS_POINT version:
		fltx4 extents;
		if (!IS_POINT)
		{
			extents = LoadUnaligned3SIMD(pTraceInfo->m_extents.Base());
		}

		// TODO: this loop probably ought to be unrolled so that we can make a more efficient
		// job of intersecting rays against boxes. The simple SIMD version used here,
		// though about 6x faster than the fpu version, is slower still than intersecting
		// against four boxes simultaneously.
		for( int i = 0; i < dispListCount; i++ )
		{
			int dispIndex = pDispList[i];
			alignedbbox_t * RESTRICT pDispBounds = &g_pDispBounds[dispIndex];

			// only collide with objects you are interested in
			if( !( pDispBounds->GetContents() & pTraceInfo->m_contents ) )
				continue;

			if( CHECK_COUNTERS && pTraceInfo->m_isswept )
			{
				// make sure we only check this brush once per trace/stab
				if ( !pTraceInfo->Visit( pDispBounds->GetCounter(), count, pCounters ) )
					continue;
			}

			if ( IS_POINT )
			{
				if (!IsBoxIntersectingRay( LoadAlignedSIMD(pDispBounds->mins.Base()), LoadAlignedSIMD(pDispBounds->maxs.Base()),
					traceStart, traceDelta, traceInvDelta, vecEpsilon ))
					continue;
			}
			else
			{
				fltx4 mins = SubSIMD(LoadAlignedSIMD(pDispBounds->mins.Base()),extents);
				fltx4 maxs = AddSIMD(LoadAlignedSIMD(pDispBounds->maxs.Base()),extents);
				if (!IsBoxIntersectingRay( mins, maxs,
					traceStart, traceDelta, traceInvDelta, vecEpsilon ))
					continue;
			}

			CDispCollTree * RESTRICT pDispTree = &g_pDispCollTrees[dispIndex];
			CM_TraceToDispTree<IS_POINT>( pTraceInfo, pDispTree, startFrac, endFrac );
			if( !pTraceInfo->m_trace.fraction )
				break;
		}
	}
	else
	{
		// utterly nonoptimal FPU pathway
		for( int i = 0; i < dispListCount; i++ )
		{
			int dispIndex = pDispList[i];
			alignedbbox_t * RESTRICT pDispBounds = &g_pDispBounds[dispIndex];

			// only collide with objects you are interested in
			if( !( pDispBounds->GetContents() & pTraceInfo->m_contents ) )
				continue;

			if( CHECK_COUNTERS && pTraceInfo->m_isswept )
			{
				// make sure we only check this brush once per trace/stab
				if ( !pTraceInfo->Visit( pDispBounds->GetCounter(), count, pCounters ) )
					continue;
			}

			if ( IS_POINT && !IsBoxIntersectingRay( pDispBounds->mins, pDispBounds->maxs, pTraceInfo->m_start, pTraceInfo->m_delta, pTraceInfo->m_invDelta, DISPCOLL_DIST_EPSILON ) )
			{
				continue;
			}

			if ( !IS_POINT && !IsBoxIntersectingRay( pDispBounds->mins - pTraceInfo->m_extents, pDispBounds->maxs + pTraceInfo->m_extents,
				pTraceInfo->m_start, pTraceInfo->m_delta, pTraceInfo->m_invDelta, DISPCOLL_DIST_EPSILON ) )
			{
				continue;
			}

			CDispCollTree * RESTRICT pDispTree = &g_pDispCollTrees[dispIndex];
			CM_TraceToDispTree<IS_POINT>( pTraceInfo, pDispTree, startFrac, endFrac );
			if( !pTraceInfo->m_trace.fraction )
				break;
		}
	}

	CM_PostTraceToDispTree( pTraceInfo );
}


bool IsNoDrawBrush( CCollisionBSPData *pBSPData, const int relevantContents, const int traceContents, const cbrush_t * RESTRICT pBrush )
{
	// Many traces rely on CONTENTS_OPAQUE always being hit, even if it is nodraw.  AI blocklos brushes
	// need this, for instance.  CS and Terror visibility checks don't want this behavior, since
	// blocklight brushes also are CONTENTS_OPAQUE and SURF_NODRAW, and are actually in the playable
	// area in several maps.
	// NOTE: This is no longer true - no traces should rely on hitting CONTENTS_OPAQUE unless they
	// actually want to hit blocklight brushes.  No other brushes are marked with those bits
	// so it should be renamed CONTENTS_BLOCKLIGHT.  CONTENTS_BLOCKLOS has its own field now
	// so there is no reason to ignore nodraw opaques since you can merely remove CONTENTS_OPAQUE to
	// get that behavior
	if ( relevantContents == CONTENTS_OPAQUE && ( traceContents & CONTENTS_IGNORE_NODRAW_OPAQUE ) )
	{
		// if the only reason we hit this brush is because it is opaque, make sure it isn't nodraw
		if ( pBrush->IsBox() )
		{
			cboxbrush_t *pBox = &pBSPData->map_boxbrushes[ pBrush->GetBox() ];
			for ( int i = 0; i < 6; i++ )
			{
				csurface_t *surface = pBSPData->GetSurfaceAtIndex( pBox->surfaceIndex[ i ] );
				if ( surface->flags & SURF_NODRAW )
				{
					return true;
				}
			}
		}
		else
		{
			cbrushside_t *side = &pBSPData->map_brushsides[ pBrush->firstbrushside ];
			for ( int i = 0; i < pBrush->numsides; i++, side++ )
			{
				csurface_t *surface = pBSPData->GetSurfaceAtIndex( side->surfaceIndex );
				if ( surface->flags & SURF_NODRAW )
				{
					return true;
				}
			}
		}
	}
	return false;
}



FORCEINLINE_TEMPLATE bool CM_BrushListOcclusionPass( COcclusionInfo &oi, const unsigned short *pBrushList, int brushListCount )
{
	//
	// trace ray/box sweep against all brushes in this leaf
	//
	CRangeValidatedArray<cbrush_t> & 			map_brushes = oi.m_pBSPData->map_brushes;

	for ( int ndxLeafBrush = 0; ndxLeafBrush < brushListCount; ndxLeafBrush++ )
	{
		// get the current brush
		int ndxBrush = pBrushList[ ndxLeafBrush ];

		cbrush_t * RESTRICT pBrush = &map_brushes[ ndxBrush ];

		const int traceContents = oi.m_contents;
		const int relevantContents = ( pBrush->contents & traceContents );

		// only collide with objects you are interested in
		if ( !relevantContents )
			continue;

		if ( !oi.PrepareCheckBrush( ndxBrush ) )
			continue; // already checked this brush

		if ( IsNoDrawBrush( oi.m_pBSPData, relevantContents, traceContents, pBrush ) )
			continue;

		// trace against the brush and find impact point -- if any?
		// NOTE: pTraceInfo->m_trace.fraction == 0.0f only when trace starts inside of a brush!
		if( CM_BrushOcclusionPass( oi, pBrush ) )
			return true;
	}
	return false; // nothing fully occludes the path
}

template <bool IS_POINT, bool CHECK_COUNTERS>
FORCEINLINE_TEMPLATE void CM_TraceToBrushList( TraceInfo_t * RESTRICT pTraceInfo, const unsigned short *pBrushList, int brushListCount )
{
	//
	// trace ray/box sweep against all brushes in this leaf
	//
	CRangeValidatedArray<cbrush_t> & 			map_brushes = pTraceInfo->m_pBSPData->map_brushes;
	TraceCounter_t * RESTRICT pCounters = NULL;
	TraceCounter_t count = 0;

	if ( CHECK_COUNTERS )
	{
		pCounters = pTraceInfo->GetBrushCounters();
		count = pTraceInfo->GetCount();
	}

	for( int ndxLeafBrush = 0; ndxLeafBrush < brushListCount; ndxLeafBrush++ )
	{
		// get the current brush
		int ndxBrush = pBrushList[ndxLeafBrush];

		cbrush_t * RESTRICT pBrush = &map_brushes[ndxBrush];

		// make sure we only check this brush once per trace/stab
		if ( CHECK_COUNTERS && !pTraceInfo->Visit( pBrush, ndxBrush, count, pCounters ) )
			continue;

		const int traceContents = pTraceInfo->m_contents;
		const int releventContents = ( pBrush->contents & traceContents );

		// only collide with objects you are interested in
		if( !releventContents )
			continue;

		// Many traces rely on CONTENTS_OPAQUE always being hit, even if it is nodraw.  AI blocklos brushes
		// need this, for instance.  CS and Terror visibility checks don't want this behavior, since
		// blocklight brushes also are CONTENTS_OPAQUE and SURF_NODRAW, and are actually in the playable
		// area in several maps.
		// NOTE: This is no longer true - no traces should rely on hitting CONTENTS_OPAQUE unless they
		// actually want to hit blocklight brushes.  No other brushes are marked with those bits
		// so it should be renamed CONTENTS_BLOCKLIGHT.  CONTENTS_BLOCKLOS has its own field now
		// so there is no reason to ignore nodraw opaques since you can merely remove CONTENTS_OPAQUE to
		// get that behavior
		if ( releventContents == CONTENTS_OPAQUE && (traceContents & CONTENTS_IGNORE_NODRAW_OPAQUE) )
		{
			// if the only reason we hit this brush is because it is opaque, make sure it isn't nodraw
			bool isNoDraw = false;

			if ( pBrush->IsBox())
			{
				cboxbrush_t *pBox = &pTraceInfo->m_pBSPData->map_boxbrushes[pBrush->GetBox()];
				for (int i=0 ; i<6 && !isNoDraw ;i++)
				{
					csurface_t *surface = pTraceInfo->m_pBSPData->GetSurfaceAtIndex( pBox->surfaceIndex[i] );
					if ( surface->flags & SURF_NODRAW )
					{
						isNoDraw = true;
						break;
					}
				}
			}
			else
			{
				cbrushside_t *side = &pTraceInfo->m_pBSPData->map_brushsides[pBrush->firstbrushside];
				for (int i=0 ; i<pBrush->numsides && !isNoDraw ;i++, side++)
				{
					csurface_t *surface = pTraceInfo->m_pBSPData->GetSurfaceAtIndex( side->surfaceIndex );
					if ( surface->flags & SURF_NODRAW )
					{
						isNoDraw = true;
						break;
					}
				}
			}

			if ( isNoDraw )
			{
				continue;
			}
		}

		// trace against the brush and find impact point -- if any?
		// NOTE: pTraceInfo->m_trace.fraction == 0.0f only when trace starts inside of a brush!
		CM_ClipBoxToBrush<IS_POINT>( pTraceInfo, pBrush );
		if( !pTraceInfo->m_trace.fraction )
			return;
	}
}


bool FASTCALL CM_LeafOcclusionPass( COcclusionInfo &oi, int ndxLeaf, float startFrac, float endFrac )
{
	// get the leaf
	cleaf_t * RESTRICT pLeaf = &oi.m_pBSPData->map_leafs[ ndxLeaf ];

	if ( pLeaf->numleafbrushes )
	{
		const unsigned short *pBrushList = &oi.m_pBSPData->map_leafbrushes[pLeaf->firstleafbrush];
		return CM_BrushListOcclusionPass( oi, pBrushList, pLeaf->numleafbrushes );
	}

	// ToDo: A pass over displacement surfaces in this leaf.
	return false;
}


/*
================
CM_TraceToLeaf
================
*/

template <bool IS_POINT>
void FASTCALL CM_TraceToLeaf( TraceInfo_t * RESTRICT pTraceInfo, int ndxLeaf, float startFrac, float endFrac )
{
	VPROF("CM_TraceToLeaf");
	// get the leaf
	cleaf_t * RESTRICT pLeaf = &pTraceInfo->m_pBSPData->map_leafs[ndxLeaf];

	if ( pLeaf->numleafbrushes )
	{
		const unsigned short *pBrushList = &pTraceInfo->m_pBSPData->map_leafbrushes[pLeaf->firstleafbrush];
		CM_TraceToBrushList<IS_POINT, true>( pTraceInfo, pBrushList, pLeaf->numleafbrushes );
		// TODO: this may be redundant
		if( pTraceInfo->m_trace.startsolid )
			return;
	}

	// Collide (test) against displacement surfaces in this leaf.
	if( pLeaf->dispCount )
	{
		unsigned short *pDispList = &pTraceInfo->m_pBSPData->map_dispList[pLeaf->dispListStart];
		CM_TraceToDispList<IS_POINT, true>( pTraceInfo, pDispList, pLeaf->dispCount, startFrac, endFrac );
	}
}

void FASTCALL CM_GetTraceDataForLeaf( TraceInfo_t * RESTRICT pTraceInfo, int ndxLeaf, CTraceListData &traceData )
{
	// get the leaf
	cleaf_t * RESTRICT pLeaf = &pTraceInfo->m_pBSPData->map_leafs[ndxLeaf];

	if ((pLeaf->contents & CONTENTS_SOLID) == 0)
	{
		traceData.m_bFoundNonSolidLeaf = true;
	}

	//

	// add brushes to list
	if ( 1 )
	{
		const int numleafbrushes = pLeaf->numleafbrushes;
		const int lastleafbrush = pLeaf->firstleafbrush + numleafbrushes;
		const CRangeValidatedArray<unsigned short> &map_leafbrushes = pTraceInfo->m_pBSPData->map_leafbrushes;
		CRangeValidatedArray<cbrush_t> & 			map_brushes = pTraceInfo->m_pBSPData->map_brushes;
		TraceCounter_t * RESTRICT pCounters = pTraceInfo->GetBrushCounters();
		TraceCounter_t count = pTraceInfo->GetCount();
		for( int ndxLeafBrush = pLeaf->firstleafbrush; ndxLeafBrush < lastleafbrush; ndxLeafBrush++ )
		{
			// get the current brush
			int ndxBrush = map_leafbrushes[ndxLeafBrush];

			cbrush_t * RESTRICT pBrush = &map_brushes[ndxBrush];

			// make sure we only add this brush once
			if ( !pTraceInfo->Visit( pBrush, ndxBrush, count, pCounters ) )
				continue;
			traceData.m_brushList.AddToTail(ndxBrush);
		}
	}

	// add displacements to list
	if ( 1 )
	{
		TraceCounter_t *pCounters = pTraceInfo->GetDispCounters();
		TraceCounter_t count = pTraceInfo->GetCount();

		// Collide (test) against displacement surfaces in this leaf.
		for( int i = 0; i < pLeaf->dispCount; i++ )
		{
			int dispIndex = pTraceInfo->m_pBSPData->map_dispList[pLeaf->dispListStart + i];
			alignedbbox_t * RESTRICT pDispBounds = &g_pDispBounds[dispIndex];
			// make sure we only add this disp once
			if ( !pTraceInfo->Visit( pDispBounds->GetCounter(), count, pCounters ) )
				continue;
			if ( !IsBoxIntersectingBox( pDispBounds->mins, pDispBounds->maxs, traceData.m_mins, traceData.m_maxs ) )
				continue;

			traceData.m_dispList.AddToTail(dispIndex);
		}
	}
}

void CM_GetTraceDataForBSP( const Vector &mins, const Vector &maxs, CTraceListData &traceData )
{
	CCollisionBSPData *pBSPData = GetCollisionBSPData();
	Vector center = (mins+maxs)*0.5f;
	Vector extents = maxs - center;
	int nodenum = 0;

	TraceInfo_t *pTraceInfo = BeginTrace();
	const int NODELIST_MAX = 1024;
	int nodeList[NODELIST_MAX];
	int nodeReadIndex = 0;
	int nodeWriteIndex = 0;
	cplane_t	*plane;
	cnode_t		*node;

	while (1)
	{
		if (nodenum < 0)
		{
			int leafIndex = -1 - nodenum;
			CM_GetTraceDataForLeaf( pTraceInfo, leafIndex, traceData );
			if ( nodeReadIndex == nodeWriteIndex )
				break;
			nodenum = nodeList[nodeReadIndex];
			nodeReadIndex = (nodeReadIndex+1) & (NODELIST_MAX-1);
		}
		else
		{
			node = &pBSPData->map_rootnode[nodenum];
			plane = node->plane;
			//		s = BoxOnPlaneSide (leaf_mins, leaf_maxs, plane);
			//		s = BOX_ON_PLANE_SIDE(*leaf_mins, *leaf_maxs, plane);
			float d0 = DotProduct( plane->normal, center ) - plane->dist;
			float d1 = DotProductAbs( plane->normal, extents );
			if (d0 >= d1)
				nodenum = node->children[0];
			else if (d0 < -d1)
				nodenum = node->children[1];
			else
			{	// go down both
				nodeList[nodeWriteIndex] = node->children[0];
				nodeWriteIndex = (nodeWriteIndex+1) & (NODELIST_MAX-1);
				// check for overflow of the ring buffer
				Assert(nodeWriteIndex != nodeReadIndex);
				nodenum = node->children[1];
			}
		}
	}

	EndTrace(pTraceInfo);
}


/*
================
CM_TestInLeaf
================
*/
static void FASTCALL CM_TestInLeaf( TraceInfo_t *pTraceInfo, int ndxLeaf )
{
	// get the leaf
	cleaf_t *pLeaf = &pTraceInfo->m_pBSPData->map_leafs[ndxLeaf];

	//
	// trace ray/box sweep against all brushes in this leaf
	//
	TraceCounter_t *pCounters = pTraceInfo->GetBrushCounters();
	TraceCounter_t count = pTraceInfo->GetCount();
	for( int ndxLeafBrush = 0; ndxLeafBrush < pLeaf->numleafbrushes; ndxLeafBrush++ )
	{
		// get the current brush
		int ndxBrush = pTraceInfo->m_pBSPData->map_leafbrushes[pLeaf->firstleafbrush+ndxLeafBrush];

		cbrush_t *pBrush = &pTraceInfo->m_pBSPData->map_brushes[ndxBrush];

		// make sure we only check this brush once per trace/stab
		if ( !pTraceInfo->Visit( pBrush, ndxBrush, count, pCounters ) )
			continue;

		// only collide with objects you are interested in
		if( !( pBrush->contents & pTraceInfo->m_contents ) )
			continue;

		//
		// test to see if the point/box is inside of any solid
		// NOTE: pTraceInfo->m_trace.fraction == 0.0f only when trace starts inside of a brush!
		//
		CM_TestBoxInBrush( pTraceInfo, pBrush );
		if( !pTraceInfo->m_trace.fraction )
			return;
	}

	// TODO: this may be redundant
	if( pTraceInfo->m_trace.startsolid )
		return;

	// if there are no displacement surfaces in this leaf -- we are done testing
	if( pLeaf->dispCount )
	{
		// test to see if the point/box is inside of any of the displacement surface
		unsigned short *pDispList = &pTraceInfo->m_pBSPData->map_dispList[pLeaf->dispListStart];
		CM_TestInDispTree( pTraceInfo, pDispList, pLeaf->dispCount, pTraceInfo->m_start, pTraceInfo->m_mins, pTraceInfo->m_maxs, pTraceInfo->m_contents, &pTraceInfo->m_trace );
	}
}

//-----------------------------------------------------------------------------
// Computes the ray endpoints given a trace.
//-----------------------------------------------------------------------------
static inline void CM_ComputeTraceEndpoints( const Ray_t& ray, trace_t& tr )
{
	// The ray start is the center of the extents; compute the actual start
	Vector start;
	VectorAdd( ray.m_Start, ray.m_StartOffset, start );

	if (tr.fraction == 1)
		VectorAdd(start, ray.m_Delta, tr.endpos);
	else
		VectorMA( start, tr.fraction, ray.m_Delta, tr.endpos );

	if (tr.fractionleftsolid == 0)
	{
		VectorCopy (start, tr.startpos);
	}
	else
	{
		if (tr.fractionleftsolid == 1.0f)
		{
			tr.startsolid = tr.allsolid = 1;
			tr.fraction = 0.0f;
			VectorCopy( start, tr.endpos );
		}

		VectorMA( start, tr.fractionleftsolid, ray.m_Delta, tr.startpos );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get a list of leaves for a trace.
//-----------------------------------------------------------------------------
void CM_RayLeafnums_r( const Ray_t &ray, CCollisionBSPData *pBSPData, int iNode,
					  float p1f, float p2f, const Vector &vecPoint1, const Vector &vecPoint2,
					  int *pLeafList, int nMaxLeafCount, int &nLeafCount )
{
	cnode_t		*pNode = NULL;
	cplane_t	*pPlane = NULL;
	float		flDist1 = 0.0f, flDist2 = 0.0f;
	float		flOffset = 0.0f;
	float		flDist;
	float		flFrac1, flFrac2;
	int			nSide;
	float		flMid;
	Vector		vecMid;

	// A quick check so we don't flood the message on overflow - or keep testing beyond our means!
	if ( nLeafCount >= nMaxLeafCount )
		return;

	// Find the point distances to the separating plane and the offset for the size of the box.
	// NJS: Hoisted loop invariant comparison to pTraceInfo->m_ispoint
	if( ray.m_IsRay )
	{
		while( iNode >= 0 )
		{
			pNode = pBSPData->map_rootnode + iNode;
			pPlane = pNode->plane;

			if ( pPlane->type < 3 )
			{
				flDist1 = vecPoint1[pPlane->type] - pPlane->dist;
				flDist2 = vecPoint2[pPlane->type] - pPlane->dist;
				flOffset = ray.m_Extents[pPlane->type];
			}
			else
			{
				flDist1 = DotProduct( pPlane->normal, vecPoint1 ) - pPlane->dist;
				flDist2 = DotProduct( pPlane->normal, vecPoint2 ) - pPlane->dist;
				flOffset = 0.0f;
			}

			// See which sides we need to consider
			if ( flDist1 > flOffset && flDist2 > flOffset )
			{
				iNode = pNode->children[0];
				continue;
			}

			if ( flDist1 < -flOffset && flDist2 < -flOffset )
			{
				iNode = pNode->children[1];
				continue;
			}

			break;
		}
	}
	else
	{
		while( iNode >= 0 )
		{
			pNode = pBSPData->map_rootnode + iNode;
			pPlane = pNode->plane;

			if ( pPlane->type < 3 )
			{
				flDist1 = vecPoint1[pPlane->type] - pPlane->dist;
				flDist2 = vecPoint2[pPlane->type] - pPlane->dist;
				flOffset = ray.m_Extents[pPlane->type];
			}
			else
			{
				flDist1 = DotProduct( pPlane->normal, vecPoint1 ) - pPlane->dist;
				flDist2 = DotProduct( pPlane->normal, vecPoint2 ) - pPlane->dist;
				flOffset = fabs( ray.m_Extents[0] * pPlane->normal[0] ) +
					fabs( ray.m_Extents[1] * pPlane->normal[1] ) +
					fabs( ray.m_Extents[2] * pPlane->normal[2] );
			}

			// See which sides we need to consider
			if ( flDist1 > flOffset && flDist2 > flOffset )
			{
				iNode = pNode->children[0];
				continue;
			}

			if ( flDist1 < -flOffset && flDist2 < -flOffset )
			{
				iNode = pNode->children[1];
				continue;
			}

			break;
		}
	}

	// If < 0, we are in a leaf node.
	if ( iNode < 0 )
	{
		if ( nLeafCount < nMaxLeafCount )
		{
			pLeafList[nLeafCount] = -1 - iNode;
			nLeafCount++;
		}
		else
		{
			DevMsg( 1, "CM_RayLeafnums_r: Max leaf count along ray exceeded!\n" );
		}

		return;
	}

	// Put the crosspoint DIST_EPSILON pixels on the near side.
	if ( flDist1 < flDist2 )
	{
		flDist = 1.0 / ( flDist1 - flDist2 );
		nSide = 1;
		flFrac2 = ( flDist1 + flOffset + DIST_EPSILON ) * flDist;
		flFrac1 = ( flDist1 - flOffset - DIST_EPSILON ) * flDist;
	}
	else if ( flDist1 > flDist2 )
	{
		flDist = 1.0 / ( flDist1-flDist2 );
		nSide = 0;
		flFrac2 = ( flDist1 - flOffset - DIST_EPSILON ) * flDist;
		flFrac1 = ( flDist1 + flOffset + DIST_EPSILON ) * flDist;
	}
	else
	{
		nSide = 0;
		flFrac1 = 1.0f;
		flFrac2 = 0.0f;
	}

	// Move up to the node
	flFrac1 = clamp( flFrac1, 0.0f, 1.0f );
	flMid = p1f + ( p2f - p1f ) * flFrac1;
	VectorLerp( vecPoint1, vecPoint2, flFrac1, vecMid );
	CM_RayLeafnums_r( ray, pBSPData, pNode->children[nSide], p1f, flMid, vecPoint1, vecMid, pLeafList, nMaxLeafCount, nLeafCount );

	// Go past the node
	flFrac2 = clamp( flFrac2, 0.0f, 1.0f );
	flMid = p1f + ( p2f - p1f ) * flFrac2;
	VectorLerp( vecPoint1, vecPoint2, flFrac2, vecMid );
	CM_RayLeafnums_r( ray, pBSPData, pNode->children[nSide^1], flMid, p2f, vecMid, vecPoint2, pLeafList, nMaxLeafCount, nLeafCount );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CM_RayLeafnums( const Ray_t &ray, int *pLeafList, int nMaxLeafCount, int &nLeafCount  )
{
	CCollisionBSPData *pBSPData = GetCollisionBSPData();
	if ( !pBSPData->numnodes )
		return;

	Vector vecEnd;
	VectorAdd( ray.m_Start, ray.m_Delta, vecEnd );
	CM_RayLeafnums_r( ray, pBSPData, 0/*headnode*/, 0.0f, 1.0f, ray.m_Start, vecEnd, pLeafList, nMaxLeafCount, nLeafCount );
}


bool FASTCALL CM_RecursiveOcclusionPass( COcclusionInfo &oi, int num, const float p1f, const float p2f, const Vector& p1, const Vector& p2 )
{
	cnode_t		*node = NULL;
	cplane_t	*plane;
	float		t1 = 0, t2 = 0, offset = 0;
	float		frac, frac2;
	float		idist;
	Vector		mid;
	int			side;
	float		midf;


	// find the point distances to the separating plane
	// and the offset for the size of the box

	while ( num >= 0 )
	{
		node = oi.m_pBSPData->map_rootnode + num;
		plane = node->plane;
		byte type = plane->type;
		float dist = plane->dist;

		if ( type < 3 )
		{
			t1 = p1[ type ] - dist;
			t2 = p2[ type ] - dist;
			offset = oi.m_extents[ type ];
		}
		else
		{
			t1 = DotProduct( plane->normal, p1 ) - dist;
			t2 = DotProduct( plane->normal, p2 ) - dist;
			offset = fabsf( oi.m_extents[ 0 ] * plane->normal[ 0 ] ) +
				fabsf( oi.m_extents[ 1 ] * plane->normal[ 1 ] ) +
				fabsf( oi.m_extents[ 2 ] * plane->normal[ 2 ] );
		}

		// see which sides we need to consider
		if ( t1 > offset && t2 > offset )
			//		if (t1 >= offset && t2 >= offset)
		{
			num = node->children[ 0 ];
			continue;
		}
		if ( t1 < -offset && t2 < -offset )
		{
			num = node->children[ 1 ];
			continue;
		}
		break;
	}

	// if < 0, we are in a leaf node
	if ( num < 0 )
	{
		return CM_LeafOcclusionPass( oi, -1 - num, p1f, p2f );
	}

	// put the crosspoint DIST_EPSILON pixels on the near side
	if ( t1 < t2 )
	{
		idist = 1.0 / ( t1 - t2 );
		side = 1;
		frac2 = ( t1 + offset + DIST_EPSILON )*idist;
		frac = ( t1 - offset - DIST_EPSILON )*idist;
	}
	else if ( t1 > t2 )
	{
		idist = 1.0 / ( t1 - t2 );
		side = 0;
		frac2 = ( t1 - offset - DIST_EPSILON )*idist;
		frac = ( t1 + offset + DIST_EPSILON )*idist;
	}
	else
	{
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// move up to the node
	frac = clamp( frac, 0, 1 );
	midf = p1f + ( p2f - p1f )*frac;
	VectorLerp( p1, p2, frac, mid );

	if ( CM_RecursiveOcclusionPass( oi, node->children[ side ], p1f, midf, p1, mid ) )
	{
		return true; // found full occlusion
	}

	// go past the node
	frac2 = clamp( frac2, 0, 1 );
	midf = p1f + ( p2f - p1f )*frac2;
	VectorLerp( p1, p2, frac2, mid );

	if ( CM_RecursiveOcclusionPass( oi, node->children[ side ^ 1 ], midf, p2f, mid, p2 ) )
	{
		return true; // found full occlusion
	}
	return false;// didn't find full occlusion yet
}


/*
==================
CM_RecursiveHullCheck

==================
Attempt to do whatever is nessecary to get this function to unroll at least once
*/
template <bool IS_POINT>
static void FASTCALL CM_RecursiveHullCheckImpl( TraceInfo_t *pTraceInfo, int num, const float p1f, const float p2f, const Vector& p1, const Vector& p2)
{
	if (pTraceInfo->m_trace.fraction <= p1f)
		return;		// already hit something nearer

	cnode_t		*node = NULL;
	cplane_t	*plane;
	float		t1 = 0, t2 = 0, offset = 0;
	float		frac, frac2;
	float		idist;
	Vector		mid;
	int			side;
	float		midf;


	// find the point distances to the separating plane
	// and the offset for the size of the box

	while( num >= 0 )
	{
		node = pTraceInfo->m_pBSPData->map_rootnode + num;
		plane = node->plane;
		byte type = plane->type;
		float dist = plane->dist;

		if (type < 3)
		{
			t1 = p1[type] - dist;
			t2 = p2[type] - dist;
			offset = pTraceInfo->m_extents[type];
		}
		else
		{
			t1 = DotProduct (plane->normal, p1) - dist;
			t2 = DotProduct (plane->normal, p2) - dist;
			if( IS_POINT )
			{
				offset = 0;
			}
			else
			{
				offset = fabsf(pTraceInfo->m_extents[0]*plane->normal[0]) +
					fabsf(pTraceInfo->m_extents[1]*plane->normal[1]) +
					fabsf(pTraceInfo->m_extents[2]*plane->normal[2]);
			}
		}

		// see which sides we need to consider
		if (t1 > offset && t2 > offset )
//		if (t1 >= offset && t2 >= offset)
		{
			num = node->children[0];
			continue;
		}
		if (t1 < -offset && t2 < -offset)
		{
			num = node->children[1];
			continue;
		}
		break;
	}

	// if < 0, we are in a leaf node
	if (num < 0)
	{
		CM_TraceToLeaf<IS_POINT>(pTraceInfo, -1-num, p1f, p2f);
		return;
	}

	// put the crosspoint DIST_EPSILON pixels on the near side
	if (t1 < t2)
	{
		idist = 1.0/(t1-t2);
		side = 1;
		frac2 = (t1 + offset + DIST_EPSILON)*idist;
		frac = (t1 - offset - DIST_EPSILON)*idist;
	}
	else if (t1 > t2)
	{
		idist = 1.0/(t1-t2);
		side = 0;
		frac2 = (t1 - offset - DIST_EPSILON)*idist;
		frac = (t1 + offset + DIST_EPSILON)*idist;
	}
	else
	{
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// move up to the node
	frac = clamp( frac, 0, 1 );
	midf = p1f + (p2f - p1f)*frac;
	VectorLerp( p1, p2, frac, mid );

	CM_RecursiveHullCheckImpl<IS_POINT>(pTraceInfo, node->children[side], p1f, midf, p1, mid);

	// go past the node
	frac2 = clamp( frac2, 0, 1 );
	midf = p1f + (p2f - p1f)*frac2;
	VectorLerp( p1, p2, frac2, mid );

	CM_RecursiveHullCheckImpl<IS_POINT>(pTraceInfo, node->children[side^1], midf, p2f, mid, p2);
}

void FASTCALL CM_RecursiveHullCheck ( TraceInfo_t *pTraceInfo, int num, const float p1f, const float p2f )
{
	const Vector& p1 = pTraceInfo->m_start;
	const Vector& p2 =  pTraceInfo->m_end;

	if( pTraceInfo->m_ispoint )
	{
		CM_RecursiveHullCheckImpl<true>( pTraceInfo, num, p1f, p2f, p1, p2);
	}
	else
	{
		CM_RecursiveHullCheckImpl<false>( pTraceInfo, num, p1f, p2f, p1, p2);
	}
}

void CM_ClearTrace( trace_t *trace )
{
	memset( trace, 0, sizeof(*trace));
	trace->fraction = 1.f;
	trace->fractionleftsolid = 0;
	trace->surface = CCollisionBSPData::nullsurface;
}


//-----------------------------------------------------------------------------
//
// The following versions use ray... gradually I'm gonna remove other versions
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Test an unswept box
//-----------------------------------------------------------------------------

static inline void CM_UnsweptBoxTrace( TraceInfo_t *pTraceInfo, const Ray_t& ray, int headnode, int brushmask )
{
	int		leafs[1024];
	int		i, numleafs;

	leafnums_t context;
	context.pLeafList = leafs;
	context.leafTopNode = -1;
	context.leafMaxCount = ARRAYSIZE(leafs);
	context.pBSPData = pTraceInfo->m_pBSPData;

	bool bFoundNonSolidLeaf = false;
	numleafs = CM_BoxLeafnums ( context, ray.m_Start, ray.m_Extents+Vector(1,1,1), headnode);
	for (i=0 ; i<numleafs ; i++)
	{
		if ((pTraceInfo->m_pBSPData->map_leafs[leafs[i]].contents & CONTENTS_SOLID) == 0)
		{
			bFoundNonSolidLeaf = true;
		}

		CM_TestInLeaf ( pTraceInfo, leafs[i] );
		if (pTraceInfo->m_trace.allsolid)
			break;
	}

	if (!bFoundNonSolidLeaf)
	{
		pTraceInfo->m_trace.allsolid = pTraceInfo->m_trace.startsolid = 1;
		pTraceInfo->m_trace.fraction = 0.0f;
		pTraceInfo->m_trace.fractionleftsolid = 1.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Ray/Hull trace against the world without the RecursiveHullTrace
//-----------------------------------------------------------------------------
void CM_BoxTraceAgainstLeafList( const Ray_t &ray, const CTraceListData &traceData, int nBrushMask, trace_t &trace )
{
	VPROF("CM_BoxTraceAgainstLeafList");
	TraceInfo_t *pTraceInfo = BeginTrace();

	CM_ClearTrace(&pTraceInfo->m_trace);
	// Setup global trace data. (This is nasty! I hate this.)
	pTraceInfo->m_bDispHit = false;
	pTraceInfo->m_DispStabDir.Init();
	pTraceInfo->m_contents = nBrushMask;
	VectorCopy( ray.m_Start, pTraceInfo->m_start );
	VectorAdd( ray.m_Start, ray.m_Delta, pTraceInfo->m_end );
	VectorMultiply( ray.m_Extents, -1.0f, pTraceInfo->m_mins );
	VectorCopy( ray.m_Extents, pTraceInfo->m_maxs );
	VectorCopy( ray.m_Extents, pTraceInfo->m_extents );
	pTraceInfo->m_delta = ray.m_Delta;
	pTraceInfo->m_invDelta = ray.InvDelta();
	pTraceInfo->m_ispoint = ray.m_IsRay;
	pTraceInfo->m_isswept = ray.m_IsSwept;

	if ( !ray.m_IsSwept )
	{
		for ( int i = 0; i < traceData.m_brushList.Count(); i++ )
		{
			int brushIndex = traceData.m_brushList[i];
			cbrush_t *pBrush = &pTraceInfo->m_pBSPData->map_brushes[brushIndex];

			// only collide with objects you are interested in
			if( !( pBrush->contents & pTraceInfo->m_contents ) )
				continue;

			//
			// test to see if the point/box is inside of any solid
			// NOTE: pTraceInfo->m_trace.fraction == 0.0f only when trace starts inside of a brush!
			//
			CM_TestBoxInBrush( pTraceInfo, pBrush );

			if ( pTraceInfo->m_trace.allsolid )
				break;
		}
		if( !pTraceInfo->m_trace.startsolid )
		{
			CM_TestInDispTree( pTraceInfo, traceData.m_dispList.Base(), traceData.m_dispList.Count(), pTraceInfo->m_start, pTraceInfo->m_mins, pTraceInfo->m_maxs, pTraceInfo->m_contents, &pTraceInfo->m_trace );
		}

		if (!traceData.m_bFoundNonSolidLeaf)
		{
			pTraceInfo->m_trace.allsolid = pTraceInfo->m_trace.startsolid = 1;
			pTraceInfo->m_trace.fraction = 0.0f;
			pTraceInfo->m_trace.fractionleftsolid = 1.0f;
		}
	}
	else
	{
		if ( ray.m_IsRay )
		{
			CM_TraceToBrushList<true, false>( pTraceInfo, traceData.m_brushList.Base(), traceData.m_brushList.Count() );
		}
		else
		{
			CM_TraceToBrushList<false, false>( pTraceInfo, traceData.m_brushList.Base(), traceData.m_brushList.Count() );
		}
		if ( pTraceInfo->m_trace.fraction > 0 && !pTraceInfo->m_trace.startsolid )
		{
			if ( ray.m_IsRay )
			{
				CM_TraceToDispList<true, false>( pTraceInfo, traceData.m_dispList.Base(), traceData.m_dispList.Count(), 0, 1 );
			}
			else
			{
				CM_TraceToDispList<false, false>( pTraceInfo, traceData.m_dispList.Base(), traceData.m_dispList.Count(), 0, 1 );
			}
		}
	}

	// Compute the trace start and end points.
	CM_ComputeTraceEndpoints( ray, pTraceInfo->m_trace );

	// Copy off the results
	trace = pTraceInfo->m_trace;
	EndTrace( pTraceInfo );

	Assert( !ray.m_IsRay || trace.allsolid || ((trace.fraction + kBoxCheckFloatEpsilon) >= trace.fractionleftsolid) );
}

#ifdef _DEBUG
CON_COMMAND( dump_occlusion_map, "Dump the data used for occlusion testing" )
{
	CBspDebugLog log("dump_occlusion_map.obj");
	CCollisionBSPData *pBSPData = GetCollisionBSPData();
	int nNotBoxes = 0;
	int nRelevantBrushes = 0;
	int traceContents = CONTENTS_SOLID | CONTENTS_MOVEABLE;
	for ( int nBrush = 0; nBrush < pBSPData->numbrushes; ++nBrush )
	{
		const cbrush_t &brush = pBSPData->map_brushes[ nBrush ];
		const int relevantContents = brush.contents & traceContents;
		if ( !relevantContents )
			continue;
		if ( IsNoDrawBrush( pBSPData, relevantContents, traceContents, &brush ) )
			continue;
		++nRelevantBrushes;

		if ( brush.IsBox() )
		{
			const cboxbrush_t &box = pBSPData->map_boxbrushes[ brush.GetBox() ];
			log.AddBox( CFmtStr( "box%d", brush.GetBox() ).Get(), "relevant", box.mins, box.maxs );
		}
		else
		{
			cbrushside_t * RESTRICT pSidesBegin = &pBSPData->map_brushsides[ brush.firstbrushside ];
			log.AddBrush( CFmtStr( "brush%d_%dside", brush.firstbrushside, brush.numsides ).Get(), "relevant_brush", pSidesBegin, brush.numsides );
			++nNotBoxes;
		}
	}
	Msg( "%d/%d relevant brushes, %d are not boxes\n", nRelevantBrushes, pBSPData->numbrushes, nNotBoxes );
}
#endif


uint64 COcclusionInfo::s_nAssocArrayCollisions = 0;
uint64 COcclusionInfo::s_nAssocArrayHits = 0;
uint64 COcclusionInfo::s_nAssocArrayMisses = 0;
//uint64 s_nOccluded = 0, s_nUnoccluded = 0;


const int32 ALIGN16 g_SIMD_0101_signmask[ 4 ] ALIGN16_POST = { 0, 0x80000000, 0, 0x80000000 };
const int32 ALIGN16 g_SIMD_0011_signmask[ 4 ] ALIGN16_POST = { 0, 0, 0x80000000, 0x80000000 };

struct OcclusionTestRec_t
{
	VectorAligned p0;
	VectorAligned vExtents0;
	VectorAligned p1;
	VectorAligned vExtents1;
};


ConVar occlusion_test_rays( "occlusion_test_rays", "0", FCVAR_DEVELOPMENTONLY );
static CUtlVector< OcclusionTestRec_t > s_OcclusionTestRecords;

static int s_nOcclusionTestsToCollect = 0, s_nOcclusionTestsCollectionsToRun = 0, s_nOcclusionTestCollectionLock = 0;
CON_COMMAND_F( occlusion_test_record, "dump occlusion tests - useful on server only", FCVAR_CHEAT )
{
	if ( args.ArgC() < 2 )
	{
		if ( s_nOcclusionTestsCollectionsToRun == 0 )
			Msg( "Not recording occlusion tests now, %d in record array\n", s_OcclusionTestRecords.Count() );
		else
			Msg( "Currently recording %d tests in %d passes, %d records collected in this pass so far\n", s_nOcclusionTestsToCollect, s_nOcclusionTestsCollectionsToRun, s_OcclusionTestRecords.Count() );
		return;
	}
	int nCount = V_atoi( args.Arg( 1 ) );
	if ( nCount > 0 )
	{
		s_nOcclusionTestsToCollect = nCount;
		if ( args.ArgC() >= 3 )
		{
			s_nOcclusionTestsCollectionsToRun = V_atoi( args.Arg( 2 ) );
			if ( s_nOcclusionTestsCollectionsToRun < 1 )
				s_nOcclusionTestsCollectionsToRun = 1;
		}
		else
		{
			s_nOcclusionTestsCollectionsToRun = 1;
		}
/*
		if ( s_OcclusionTestRecords.Count() > 0 )
		{
			Msg( "Abandoning %d tests already collected. ", s_OcclusionTestRecords.Count() );
			s_OcclusionTestRecords.Purge();
		}
*/
		Msg( "Preparing to collect %d occlusion tests. %d collected so far.\n", nCount, s_OcclusionTestRecords.Count() );
		s_OcclusionTestRecords.EnsureCapacity( nCount );
	}	
	else if ( nCount == 0 )
	{
		Msg( "Stopping all occlusion tests: %dx%d, purging %d\n", s_nOcclusionTestsToCollect, s_nOcclusionTestsCollectionsToRun, s_OcclusionTestRecords.Count() );
		s_nOcclusionTestsToCollect = 0;
		s_nOcclusionTestsCollectionsToRun = 0;
		s_OcclusionTestRecords.Purge();
	}
	else
	{
		Msg( "Need to collect positive number of tests\n" );
	}
}


const char *GetMapName( void );


bool DebugCheckOcclusion( const Vector &p0, const Vector &vExtents0, const Vector &p1, const Vector &vExtents1, int nOcclusionRays )
{
	for ( int i = 0; i < nOcclusionRays; ++i )
	{
		// we shouldn't find a single ray between the two boxes that is not occluded
		Vector rnd0( RandomFloat() * vExtents0.x, RandomFloat() * vExtents0.y, RandomFloat() * vExtents0.z );
		Vector rnd1( RandomFloat() * vExtents1.x, RandomFloat() * vExtents1.y, RandomFloat() * vExtents1.z );
		Ray_t ray;
		ray.Init( p0 + rnd0, p1 + rnd1 );

		trace_t tr;
		V_memset( &tr, 0, sizeof( tr ) );

		CM_BoxTrace( ray, 0, CONTENTS_SOLID | CONTENTS_MOVEABLE, false, tr );
		if ( !tr.DidHit() )
		{
			return false;
		}
	}
	return true;
}


bool CM_IsFullyOccluded( const VectorAligned &p0, const VectorAligned &vExtents0, const VectorAligned &p1, const VectorAligned &vExtents1, OcclusionTestResults_t *pResults )
{
	if ( s_nOcclusionTestsToCollect > 0 && s_nOcclusionTestCollectionLock == 0 )
	{
		OcclusionTestRec_t &rec = s_OcclusionTestRecords[ s_OcclusionTestRecords.AddToTail() ];
		rec.p0 = p0;
		rec.vExtents0 = vExtents0;
		rec.p1 = p1;
		rec.vExtents1 = vExtents1;

		if ( s_OcclusionTestRecords.Count() >= s_nOcclusionTestsToCollect )
		{
			// record the file
			for ( int nAttempt = 0; nAttempt < 10000; ++nAttempt )
			{
				const char *pMapName = GetMapName();
				CFmtStr fileName( "occlusion_records.%s.%04d.ocr", pMapName, nAttempt );
				if ( g_pFullFileSystem->FileExists( fileName.Get() ) )
					continue;
				Msg( "Saving %d occlusion records to %s (#%d)\n", s_OcclusionTestRecords.Count(), fileName.Get(), nAttempt );
				FileHandle_t hDump = g_pFullFileSystem->Open( fileName.Get(), "wb" );
				g_pFullFileSystem->Write( s_OcclusionTestRecords.Base(), s_OcclusionTestRecords.Count() * sizeof( s_OcclusionTestRecords[ 0 ] ), hDump );
				g_pFullFileSystem->Close( hDump );
				s_OcclusionTestRecords.Purge();
				break;
			}
			if ( --s_nOcclusionTestsCollectionsToRun <= 0 )
			{
				s_nOcclusionTestsToCollect = 0;
				s_nOcclusionTestsCollectionsToRun = 0;
			}
		}
	}

	VPROF_BUDGET( "CM_IsFullyOccluded", VPROF_BUDGETGROUP_OTHER_UNACCOUNTED );
	Vector vExtents = VectorMax( vExtents0, vExtents1 );
	COcclusionInfo oi;
	oi.m_pBSPData = GetCollisionBSPData();
	oi.m_pResults = pResults;
	// check if the map is not loaded
	if ( !oi.m_pBSPData->numnodes )
	{
		return false;
	}

	// axis-aligned boxes are a very special case: we can just cast 6 rays and if we don't care about their order (i.e. if we aren't looking for partial occlusions), and have 2x4 or 8 lane SIMD, we can just cast each corner to a corresponding other box'es corner and we'll be just fine
	fltx4 f4ExtentsStart = LoadAlignedSIMD( &vExtents0 ), f4ExtentsEnd = LoadAlignedSIMD( &vExtents1 ), f4Start = LoadAlignedSIMD( &p0 ), f4End = LoadAlignedSIMD( &p1 );
	fltx4 f4Delta = f4End - f4Start;
	fltx4 f4DeltaPos = AbsSIMD( f4Delta );
	if ( !( TestSignSIMD( ( f4ExtentsStart + f4ExtentsEnd ) - f4DeltaPos ) & 7 ) )
	{
		return false; // we can't get full occlusion if the two boxes intersect
	}
	StoreAligned3SIMD( &oi.m_delta, f4Delta );
	StoreAligned3SIMD( &oi.m_deltaPos, f4DeltaPos );

	fltx4 f40101signmask = *( fltx4* )g_SIMD_0101_signmask;
	oi.m_StartXnXYnY = ShuffleXXYY( f4Start ) + XorSIMD( f40101signmask, ShuffleXXYY( f4ExtentsStart ) );
	oi.m_EndXnXYnY = ShuffleXXYY( f4End ) + XorSIMD( f40101signmask, ShuffleXXYY( f4ExtentsEnd ) );
	oi.m_StartEndZnZ = _mm_shuffle_ps( f4Start, f4End, MM_SHUFFLE_REV( 2, 2, 2, 2 ) ) + XorSIMD( f40101signmask, _mm_shuffle_ps( f4ExtentsStart, f4ExtentsEnd, MM_SHUFFLE_REV( 2, 2, 2, 2 ) ) );

	oi.m_start = p0;
	oi.m_end = p1;
	Vector vExtentsScaled = vExtents;
	oi.m_extents = vExtentsScaled;
	oi.m_extents.z = Max( 0.0f, oi.m_extents.z - 0.0625f /*MOVE_HEIGHT_EPSILON*/ );
	oi.m_delta = p1 - p0;
	VectorAbs( oi.m_delta, oi.m_deltaPos );

	oi.m_uvwExtents.Init( oi.m_deltaPos.y * oi.m_extents.z + oi.m_deltaPos.z * oi.m_extents.y, oi.m_deltaPos.z * oi.m_extents.x + oi.m_deltaPos.x * oi.m_extents.z, oi.m_deltaPos.x * oi.m_extents.y + oi.m_deltaPos.y * oi.m_extents.x ); // all extents and their abs projections should be positive or zero
	oi.m_uvwMins = -oi.m_uvwExtents;
	oi.m_uvwMaxs = oi.m_uvwExtents;
	oi.m_deltaSigns.Init( Sign( oi.m_delta.x ), Sign( oi.m_delta.y ), Sign( oi.m_delta.z ) );

// 	oi.m_minsPos.Init( oi.m_delta.x >= 0 ? p0.x : -p1.x, oi.m_delta.y >= 0 ? p0.y : -p1.y, oi.m_delta.z >= 0 ? p0.z : -p1.z );
// 	oi.m_maxsPos.Init( oi.m_delta.x >= 0 ? p1.x : -p0.x, oi.m_delta.y >= 0 ? p1.y : -p0.y, oi.m_delta.z >= 0 ? p1.z : -p0.z );
//	AssertDbg( oi.m_minsPos.x <= oi.m_maxsPos.x && oi.m_minsPos.y <= oi.m_maxsPos.y && oi.m_minsPos.z <= oi.m_maxsPos.z );
	oi.m_traceMins = VectorMin( p0, p1 );
	oi.m_traceMins.z -= vExtentsScaled.z;
	oi.m_traceMaxs = VectorMax( p0, p1 );
	oi.m_traceMaxs.z += vExtentsScaled.z;
	oi.m_contents = CONTENTS_SOLID | CONTENTS_MOVEABLE; // can solid or moveable be semitransparent?
	oi.m_pDebugLog = NULL;
#ifdef _DEBUG
	static bool s_bDumpOcclusionPass = false;
	if ( s_bDumpOcclusionPass )
	{
		oi.m_pDebugLog = new CBspDebugLog( "bsp_debug_log.obj");
		oi.m_pDebugLog->AddBox( "start", "start", p0 - vExtents0, p0 + vExtents0 );
		oi.m_pDebugLog->AddBox( "end", "end", p1 - vExtents1, p1 + vExtents1 );
		oi.m_pDebugLog->ResetPrimCount();
	}
#endif
	return CM_RecursiveOcclusionPass( oi, 0, 0.0f, 1.0f, p0, p1 );

}





bool CM_IsFullyOccluded( const AABB_t &aabb1, const AABB_t &aabb2 )
{
	VectorAligned vCenter1( aabb1.GetCenter() );
	VectorAligned vCenter2( aabb2.GetCenter() );
	VectorAligned vHullExtents1( aabb1.GetSize() * .5f );
	VectorAligned vHullExtents2( aabb2.GetSize() * .5f ); // include both hulls extents

	bool bOccluded = CM_IsFullyOccluded(
		vCenter1,
		vHullExtents1,
		vCenter2,
		vHullExtents2
	);
	return bOccluded;
}





CON_COMMAND_F( occlusion_test_run, "run occlusion test", FCVAR_CHEAT )
{
	if ( s_nOcclusionTestCollectionLock )
	{
		Msg( "Occlusion test collection lock is on (%d). This is unexpected. Resetting.\n", s_nOcclusionTestCollectionLock );
		s_nOcclusionTestCollectionLock = 0;
	}

	if ( args.ArgC() < 2 )
	{
		Msg( "Usage: occlusion_test_run <index> [-cold] [-check [<n_rays>]]\n" );
		return;
	}

	const char *pMapName = GetMapName();
	CFmtStr fileName( "occlusion_records.%s.%04d.ocr", pMapName, V_atoi( args.Arg( 1 ) ) );
	CUtlBuffer buf;
	if ( !g_pFullFileSystem->ReadFile( fileName.Get(), NULL, buf ) )
	{
		Msg( "Cannot read %s\n", fileName.Get() );
		return;
	}
	s_nOcclusionTestCollectionLock++;
	bool bColdCache = false;
	int nDoubleCheck = 0;
	const CPUInformation &cpuInfo = GetCPUInformation();
	uint32 nCacheSizeKb = Max( Max( cpuInfo.m_nL1CacheSizeKb, cpuInfo.m_nL2CacheSizeKb ), cpuInfo.m_nL3CacheSizeKb );
	for ( int i = 2; i < args.ArgC(); ++i )
	{
		const char *pArg = args.Arg( i );
		if ( !V_stricmp( pArg, "-cold" ) )
		{
			bColdCache = true;
			Msg( "Will test with cold cache; flush array size %dkb; cache sizes: L1 %dKb, L2 %dKb, L3 %dKb\n", nCacheSizeKb, cpuInfo.m_nL1CacheSizeKb, cpuInfo.m_nL2CacheSizeKb , cpuInfo.m_nL3CacheSizeKb );
		}
		else if ( !V_stricmp( pArg, "-check" ) )
		{
			nDoubleCheck = 100;
			if ( i + 1 < args.ArgC() )
			{
				nDoubleCheck = V_atoi( args[ i + 1 ] );
				if ( nDoubleCheck > 0 )
				{
					i++;
				}
				else
				{
					nDoubleCheck = 100;
				}
			}
			Msg( "Will double-check occlusion with %d rays\n", nDoubleCheck );
		}
	}
	int nRecCount = buf.TellPut() / sizeof( OcclusionTestRec_t );
	OcclusionTestRec_t *pRec = ( OcclusionTestRec_t * )buf.Base();
	Msg( "Loaded %d occlusion records from %s\n", nRecCount, fileName.Get() );
	int nRecOccluded = 0, nRecFalseOcclusion = 0, nRecFalseNonOcclusion = 0;
	int64 nCpuSpeed = cpuInfo.m_Speed;
	int64 nBestCase = nCpuSpeed, nWorstCase = 0, nTotalTime = 0;
	int nBestIndex = -1,  nWorstIndex = -1;

	CUtlVector< int8 > flushCache;
	if ( bColdCache )
		flushCache.SetCount( nCacheSizeKb * 1024 );

	for ( int i = 0; i < nRecCount; ++i )
	{
		if ( bColdCache )
		{
			// flush the cache
			flushCache.FillWithValue( i );
		}

		int64 nTimeStart = GetTimebaseRegister();
		bool bOccluded = CM_IsFullyOccluded( pRec[ i ].p0, pRec[ i ].vExtents0, pRec[ i ].p1, pRec[ i ].vExtents1 );
		int64 nTimeOcclusion = GetTimebaseRegister() - nTimeStart;

		if ( nWorstCase < nTimeOcclusion )
		{
			nWorstCase = nTimeOcclusion;
			nWorstIndex = i;
		}

		if ( nBestCase > nTimeOcclusion )
		{
			nBestCase = nTimeOcclusion ;
			nBestIndex = i;
		}

		nTotalTime += nTimeOcclusion;

		if ( bOccluded )
			++nRecOccluded;

		if ( nDoubleCheck )
		{
			if ( bOccluded != DebugCheckOcclusion( pRec[ i ].p0, pRec[ i ].vExtents0, pRec[ i ].p1, pRec[ i ].vExtents1, nDoubleCheck ) )
			{
				if ( bOccluded )
				{
					++nRecFalseOcclusion;
					Warning( "FALSE OCCLUSION FOUND: #%d\n", i );
				}
				else
					++nRecFalseNonOcclusion;
			}
		}
	}
	double flMicrosecondsPerTick = 1e6 / nCpuSpeed;
	Msg( "%d/%d (%.1f%%) occluded, occlusion tests run %.1f us ave ([%d]=%.1fus, [%d]=%.1fus) on a %.2fGHz CPU\n",
		nRecOccluded, nRecCount, ( nRecOccluded * 100.0f ) / nRecCount,
		( nTotalTime * flMicrosecondsPerTick ) / nRecCount,
		nBestIndex, nBestCase * flMicrosecondsPerTick,
		nWorstIndex, nWorstCase * flMicrosecondsPerTick,
		nCpuSpeed * 1e-9
	);
	if ( nDoubleCheck )
	{
		Msg( "%d false non-occlusions\n", nRecFalseNonOcclusion );
	}
	s_nOcclusionTestCollectionLock--;
}



void CM_BoxTrace( const Ray_t& ray, int headnode, int brushmask, bool computeEndpt, trace_t& tr )
{
	VPROF("BoxTrace");
	// for multi-check avoidance
	TraceInfo_t *pTraceInfo = BeginTrace();

#ifdef COUNT_COLLISIONS
	// for statistics, may be zeroed
	g_CollisionCounts.m_Traces++;
#endif

	// fill in a default trace
	CM_ClearTrace( &pTraceInfo->m_trace );

	// check if the map is not loaded
	if (!pTraceInfo->m_pBSPData->numnodes)
	{
		tr = pTraceInfo->m_trace;
		EndTrace( pTraceInfo );
		return;
	}

	pTraceInfo->m_bDispHit = false;
	pTraceInfo->m_DispStabDir.Init();
	pTraceInfo->m_contents = brushmask;
	VectorCopy (ray.m_Start, pTraceInfo->m_start);
	VectorAdd  (ray.m_Start, ray.m_Delta, pTraceInfo->m_end);
	VectorMultiply (ray.m_Extents, -1.0f, pTraceInfo->m_mins);
	VectorCopy (ray.m_Extents, pTraceInfo->m_maxs);
	VectorCopy (ray.m_Extents, pTraceInfo->m_extents);
	pTraceInfo->m_delta = ray.m_Delta;
	pTraceInfo->m_invDelta = ray.InvDelta();
	pTraceInfo->m_ispoint = ray.m_IsRay;
	pTraceInfo->m_isswept = ray.m_IsSwept;

	if (!ray.m_IsSwept)
	{
		// check for position test special case
		CM_UnsweptBoxTrace( pTraceInfo, ray, headnode, brushmask );
	}
	else
	{
		// general sweeping through world
		CM_RecursiveHullCheck( pTraceInfo, headnode, 0, 1 );
	}
	// Compute the trace start + end points
	if (computeEndpt)
	{
		CM_ComputeTraceEndpoints( ray, pTraceInfo->m_trace );
	}

	// Copy off the results
	tr = pTraceInfo->m_trace;
	EndTrace( pTraceInfo );

	Assert( !ray.m_IsRay || tr.allsolid || ((tr.fraction + kBoxCheckFloatEpsilon) >= tr.fractionleftsolid) );
}


void CM_TransformedBoxTrace( const Ray_t& ray, int headnode, int brushmask,
							const Vector& origin, QAngle const& angles, trace_t& tr )
{
	matrix3x4_t	localToWorld;
	Ray_t ray_l;

	// subtract origin offset
	VectorCopy( ray.m_StartOffset, ray_l.m_StartOffset );
	VectorCopy( ray.m_Extents, ray_l.m_Extents );

	// Are we rotated?
	bool rotated = (angles[0] || angles[1] || angles[2]);

	// rotate start and end into the models frame of reference
	if (rotated)
	{
		// NOTE: In this case, the bbox is rotated into the space of the BSP as well
		// to insure consistency at all orientations, we must rotate the origin of the ray
		// and reapply the offset to the center of the box.  That way all traces with the
		// same box centering will have the same transformation into local space
		Vector worldOrigin = ray.m_Start + ray.m_StartOffset;
		AngleMatrix( angles, origin, localToWorld );
		VectorIRotate( ray.m_Delta, localToWorld, ray_l.m_Delta );
		VectorITransform( worldOrigin, localToWorld, ray_l.m_Start );
		ray_l.m_Start -= ray.m_StartOffset;
	}
	else
	{
		VectorSubtract( ray.m_Start, origin, ray_l.m_Start );
		VectorCopy( ray.m_Delta, ray_l.m_Delta );
	}

	ray_l.m_IsRay = ray.m_IsRay;
	ray_l.m_IsSwept = ray.m_IsSwept;

	// sweep the box through the model, don't compute endpoints
	CM_BoxTrace( ray_l, headnode, brushmask, false, tr );

	// If we hit, gotta fix up the normal...
	if (( tr.fraction != 1 ) && rotated )
	{
		// transform the normal from the local space of this entity to world space
		Vector temp;
		VectorCopy (tr.plane.normal, temp);
		VectorRotate( temp, localToWorld, tr.plane.normal );
	}

	CM_ComputeTraceEndpoints( ray, tr );
}



/*
===============================================================================

PVS / PAS

===============================================================================
*/

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pBSPData -
//			*out -
//-----------------------------------------------------------------------------
void CM_NullVis( CCollisionBSPData *pBSPData, byte *out )
{
	int numClusterBytes = (pBSPData->numclusters+7)>>3;
	byte *out_p = out;

	while (numClusterBytes)
	{
		*out_p++ = 0xff;
		numClusterBytes--;
	}
}

/*
===================
CM_DecompressVis
===================
*/
void CM_DecompressVis( CCollisionBSPData *pBSPData, int cluster, int visType, byte *out )
{
	int		c;
	byte	*out_p;
	int		numClusterBytes;

	if ( !pBSPData )
	{
		Assert( false ); // Shouldn't ever happen.
	}

	if ( cluster > pBSPData->numclusters || cluster < 0 )
	{
		// This can happen if this is called while the level is loading. See Map_VisCurrentCluster.
		CM_NullVis( pBSPData, out );
		return;
	}

	// no vis info, so make all visible
	if ( !pBSPData->numvisibility || !pBSPData->map_vis )
	{
		CM_NullVis( pBSPData, out );
		return;
	}

	byte *in = ((byte *)pBSPData->map_vis) + pBSPData->map_vis->bitofs[cluster][visType];
	numClusterBytes = (pBSPData->numclusters+7)>>3;
	out_p = out;

	// no vis info, so make all visible
	if ( !in )
	{
		CM_NullVis( pBSPData, out );
		return;
	}

	do
	{
		if (*in)
		{
			*out_p++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		if ((out_p - out) + c > numClusterBytes)
		{
			c = numClusterBytes - (out_p - out);
			ConMsg( "warning: Vis decompression overrun\n" );
		}
		while (c)
		{
			*out_p++ = 0;
			c--;
		}
	} while (out_p - out < numClusterBytes);
}

//-----------------------------------------------------------------------------
// Purpose: Decompress the RLE bitstring for PVS or PAS of one cluster
// Input  : *dest - buffer to store the decompressed data
//			cluster - index of cluster of interest
//			visType - DVIS_PAS or DVIS_PAS
// Output : byte * - pointer to the filled buffer
//-----------------------------------------------------------------------------
const byte *CM_Vis( byte *dest, int destlen, int cluster, int visType )
{
	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	if ( !dest || visType > 2 || visType < 0 )
	{
		Sys_Error( "CM_Vis: error");
		return NULL;
	}

	if ( cluster == -1 )
	{
		int len = (pBSPData->numclusters+7)>>3;
		if ( len > destlen )
		{
			Sys_Error( "CM_Vis:  buffer not big enough (%i but need %i)\n",
				destlen, len );
		}
		memset( dest, 0, (pBSPData->numclusters+7)>>3 );
	}
	else
	{
		CM_DecompressVis( pBSPData, cluster, visType, dest );
	}

	return dest;
}

static byte	pvsrow[MAX_MAP_LEAFS/8];

int CM_ClusterPVSSize()
{
	return sizeof( pvsrow );
}

const byte	*CM_ClusterPVS (int cluster)
{
	return CM_Vis( pvsrow, CM_ClusterPVSSize(), cluster, DVIS_PVS );
}

/*
===============================================================================

AREAPORTALS

===============================================================================
*/

void FloodArea_r (CCollisionBSPData *pBSPData, carea_t *area, int floodnum)
{
	int		i;
	dareaportal_t	*p;

	if (area->floodvalid == pBSPData->floodvalid)
	{
		if (area->floodnum == floodnum)
			return;
		Sys_Error( "FloodArea_r: reflooded");
	}

	area->floodnum = floodnum;
	area->floodvalid = pBSPData->floodvalid;
	p = &pBSPData->map_areaportals[area->firstareaportal];
	for (i=0 ; i<area->numareaportals ; i++, p++)
	{
		if (pBSPData->portalopen[p->m_PortalKey])
		{
			FloodArea_r (pBSPData, &pBSPData->map_areas[p->otherarea], floodnum);
		}
	}
}

/*
====================
FloodAreaConnections


====================
*/
void	FloodAreaConnections ( CCollisionBSPData *pBSPData )
{
	int		i;
	carea_t	*area;
	int		floodnum;

	// all current floods are now invalid
	pBSPData->floodvalid++;
	floodnum = 0;

	// area 0 is not used
	for (i=1 ; i<pBSPData->numareas ; i++)
	{
		area = &pBSPData->map_areas[i];
		if (area->floodvalid == pBSPData->floodvalid)
			continue;		// already flooded into
		floodnum++;
		FloodArea_r (pBSPData, area, floodnum);
	}

}

void CM_SetAreaPortalState( int portalnum, int isOpen )
{
	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	// Portalnums in the BSP file are 1-based instead of 0-based
	if (portalnum > pBSPData->numareaportals)
	{
		Sys_Error( "portalnum > numareaportals");
	}

	pBSPData->portalopen[portalnum] = (isOpen != 0);
	FloodAreaConnections (pBSPData);
}

void CM_SetAreaPortalStates( const int *portalnums, const int *isOpen, int nPortals )
{
	if ( nPortals == 0 )
		return;

	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	// get the current collision bsp -- there is only one!
	for ( int i=0; i < nPortals; i++ )
	{
		// Portalnums in the BSP file are 1-based instead of 0-based
		if (portalnums[i] > pBSPData->numareaportals)
			Sys_Error( "portalnum > numareaportals");

		pBSPData->portalopen[portalnums[i]] = (isOpen[i] != 0);
	}

	FloodAreaConnections( pBSPData );
}

bool CM_AreasConnected( int area1, int area2 )
{
	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	if (map_noareas.GetInt())
		return true;

	if (area1 >= pBSPData->numareas || area2 >= pBSPData->numareas)
	{
		Sys_Error( "area(1==%i, 2==%i) >= numareas (%i):  Check if engine->ResetPVS() was called from ClientSetupVisibility", area1, area2,  pBSPData->numareas );
	}

	return (pBSPData->map_areas[area1].floodnum == pBSPData->map_areas[area2].floodnum);
}

void CM_LeavesConnected( const Vector &vecOrigin, int nCount, const int *pLeaves, bool *pIsConnected )
{
	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	if ( map_noareas.GetInt() )
	{
		memset( pIsConnected, 1, nCount * sizeof(bool) );
		return;
	}

	int nArea = CM_LeafArea( CM_PointLeafnum( vecOrigin ) );
	Assert( nArea < pBSPData->numareas && nArea >= 0 );
	for ( int i = 0; i < nCount; ++i )
	{
		int nLeafArea = pBSPData->map_leafs[ pLeaves[i] ].area;
		Assert( nLeafArea < pBSPData->numareas && nLeafArea >= 0 );
		pIsConnected[i] = ( pBSPData->map_areas[nArea].floodnum == pBSPData->map_areas[ nLeafArea ].floodnum );
	}
}

//-----------------------------------------------------------------------------
// Purpose: CM_WriteAreaBits
//  Writes a bit vector of all the areas in the same flood as the area parameter
// Input  : *buffer -
//			buflen -
//			area -
// Output : int - number of bytes of collision data (based on area count)
//-----------------------------------------------------------------------------
int CM_WriteAreaBits( byte *buffer, int buflen, int area )
{
	int		i;
	int		floodnum;
	int		bytes;

	if ( buflen < 32 )
	{
		Sys_Error( "CM_WriteAreaBits with buffer size %d < 32\n", buflen );
	}

	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();
	Assert( pBSPData );
	bytes = ( pBSPData->numareas + 7 ) >> 3;
	Assert( buflen >= bytes );

	if ( map_noareas.GetInt() )
	{
		// for debugging, send everything (or nothing if map_noareas == 2 )
		byte fill = ( map_noareas.GetInt() == 2 ) ? 0x00 : 0xff;

		Q_memset( buffer, fill, buflen );
	}
	else
	{
		Q_memset( buffer, 0x00, buflen );

		floodnum = pBSPData->map_areas[area].floodnum;
		for ( i = 0 ; i < pBSPData->numareas; ++i )
		{
			if ( pBSPData->map_areas[i].floodnum == floodnum || !area )
			{
				buffer[ i >> 3 ] |= ( 1 << ( i & 7 ) );
			}
		}
	}

	return bytes;
}

bool CM_GetAreaPortalPlane( const Vector &vViewOrigin, int portalKey, VPlane *pPlane )
{
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	// First, find the leaf and area the viewer is in.
	int iLeaf = CM_PointLeafnum( vViewOrigin );
	if( iLeaf < 0 || iLeaf >= pBSPData->numleafs )
		return false;

	int iArea = pBSPData->map_leafs[iLeaf].area;
	if( iArea < 0 || iArea >= pBSPData->numareas )
		return false;

	carea_t *pArea = &pBSPData->map_areas[iArea];
	for( int i=0; i < pArea->numareaportals; i++ )
	{
		dareaportal_t *pPortal = &pBSPData->map_areaportals[pArea->firstareaportal + i];

		if( pPortal->m_PortalKey == portalKey )
		{
			cplane_t *pMapPlane = &pBSPData->map_planes[pPortal->planenum];
			pPlane->m_Normal = pMapPlane->normal;
			pPlane->m_Dist = pMapPlane->dist;
			return true;
		}
	}

	return false;
}


/*
=============
CM_HeadnodeVisible

Returns true if any leaf under headnode has a cluster that
is potentially visible
=============
*/
bool CM_HeadnodeVisible (int nodenum, const byte *visbits, int vissize )
{
	int		leafnum;
	int		cluster;
	cnode_t	*node;

	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	if (nodenum < 0)
	{
		leafnum = -1-nodenum;
		cluster = pBSPData->map_leafs[leafnum].cluster;
		if (cluster == -1)
			return false;
		if (visbits[cluster>>3] & (1<<(cluster&7)))
			return true;
		return false;
	}

	node = &pBSPData->map_rootnode[nodenum];
	if (CM_HeadnodeVisible(node->children[0], visbits, vissize ))
		return true;
	return CM_HeadnodeVisible(node->children[1], visbits, vissize );
}



//-----------------------------------------------------------------------------
// Purpose: returns true if the box is in a cluster that is visible in the visbits
// Input  : mins - box extents
//			maxs -
//			*visbits - pvs or pas of some cluster
// Output : true if visible, false if not
//-----------------------------------------------------------------------------
#define MAX_BOX_LEAVES	256
int	CM_BoxVisible( const Vector& mins, const Vector& maxs, const byte *visbits, int vissize )
{
	int leafList[MAX_BOX_LEAVES];
	int topnode;

	// FIXME: Could save a loop here by traversing the tree in this routine like the code above
	int count = CM_BoxLeafnums( mins, maxs, leafList, MAX_BOX_LEAVES, &topnode );
	for ( int i = 0; i < count; i++ )
	{
		int cluster = CM_LeafCluster( leafList[i] );
		int offset = cluster>>3;

		if ( offset > vissize )
		{
			Sys_Error( "CM_BoxVisible:  cluster %i, offset %i out of bounds %i\n", cluster, offset, vissize );
		}

		if (visbits[cluster>>3] & (1<<(cluster&7)))
		{
			return true;
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Returns the world-space center of an entity
//-----------------------------------------------------------------------------
void CM_WorldSpaceCenter( ICollideable *pCollideable, Vector *pCenter )
{
	Vector vecLocalCenter;
	VectorAdd( pCollideable->OBBMins(), pCollideable->OBBMaxs(), vecLocalCenter );
	vecLocalCenter *= 0.5f;

	if ( ( pCollideable->GetCollisionAngles() == vec3_angle ) || ( vecLocalCenter == vec3_origin ) )
	{
		VectorAdd( vecLocalCenter, pCollideable->GetCollisionOrigin(), *pCenter );
	}
	else
	{
		VectorTransform( vecLocalCenter, pCollideable->CollisionToWorldTransform(), *pCenter );
	}
}


//-----------------------------------------------------------------------------
// Returns the world-align bounds of an entity
//-----------------------------------------------------------------------------
void CM_WorldAlignBounds( ICollideable *pCollideable, Vector *pMins, Vector *pMaxs )
{
	if ( pCollideable->GetCollisionAngles() == vec3_angle )
	{
		*pMins = pCollideable->OBBMins();
		*pMaxs = pCollideable->OBBMaxs();
	}
	else
	{
		ITransformAABB( pCollideable->CollisionToWorldTransform(), pCollideable->OBBMins(), pCollideable->OBBMaxs(), *pMins, *pMaxs );
		*pMins -= pCollideable->GetCollisionOrigin();
		*pMaxs -= pCollideable->GetCollisionOrigin();
	}
}


//-----------------------------------------------------------------------------
// Returns the world-space bounds of an entity
//-----------------------------------------------------------------------------
void CM_WorldSpaceBounds( ICollideable *pCollideable, Vector *pMins, Vector *pMaxs )
{
	if ( pCollideable->GetCollisionAngles() == vec3_angle )
	{
		VectorAdd( pCollideable->GetCollisionOrigin(), pCollideable->OBBMins(), *pMins );
		VectorAdd( pCollideable->GetCollisionOrigin(), pCollideable->OBBMaxs(), *pMaxs );
	}
	else
	{
		TransformAABB( pCollideable->CollisionToWorldTransform(), pCollideable->OBBMins(), pCollideable->OBBMaxs(), *pMins, *pMaxs );
	}
}


void CM_SetupAreaFloodNums( byte areaFloodNums[MAX_MAP_AREAS], int *pNumAreas )
{
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	*pNumAreas = pBSPData->numareas;
	if ( pBSPData->numareas > MAX_MAP_AREAS )
		Error( "pBSPData->numareas > MAX_MAP_AREAS" );

	for ( int i=0; i < pBSPData->numareas; i++ )
	{
		Assert( pBSPData->map_areas[i].floodnum < MAX_MAP_AREAS );
		areaFloodNums[i] = (byte)pBSPData->map_areas[i].floodnum;
	}
}


// -----------------------------------------------------------------------------
// CFastLeafAccessor implementation.
// -----------------------------------------------------------------------------

CFastPointLeafNum::CFastPointLeafNum()
{
	Reset();
}


int CFastPointLeafNum::GetLeaf( const Vector &vPos )
{
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	if ( m_iCachedLeaf < 0 || m_iCachedLeaf >= pBSPData->numleafs ||
		vPos.DistToSqr( m_vCachedPos ) > m_flDistToExitLeafSqr )
	{
		m_vCachedPos = vPos;

		m_flDistToExitLeafSqr = 1e24;
		m_iCachedLeaf = CM_PointLeafnumMinDistSqr_r( pBSPData, vPos, 0, m_flDistToExitLeafSqr );
	}

	Assert( m_iCachedLeaf >= 0 );
	Assert( m_iCachedLeaf < pBSPData->numleafs );
	return m_iCachedLeaf;
}

void CFastPointLeafNum::Reset( void )
{
	m_iCachedLeaf = -1;
	m_flDistToExitLeafSqr = -1;
	m_vCachedPos.Init();
}

bool FASTCALL IsBoxIntersectingRayNoLowest( fltx4 boxMin, fltx4  boxMax,
								   const fltx4 & origin, const fltx4 & delta, const fltx4 & invDelta, // ray parameters
								   const fltx4 & vTolerance ///< eg from ReplicateX4(flTolerance)
								   )
{
	/*
	Assert( boxMin[0] <= boxMax[0] );
	Assert( boxMin[1] <= boxMax[1] );
	Assert( boxMin[2] <= boxMax[2] );
	*/
#if defined(_X360) && defined(DBGFLAG_ASSERT)
	unsigned int r;
	AssertMsg( (XMVectorGreaterOrEqualR(&r, SetWToZeroSIMD(boxMax),SetWToZeroSIMD(boxMin)), XMComparisonAllTrue(r)), "IsBoxIntersectingRay : boxmax < boxmin" );
#endif

	// test if delta is tiny along any dimension
	bi32x4 bvDeltaTinyComponents = CmpInBoundsSIMD( delta, Four_Epsilons );

	// push box extents out by tolerance (safe to do because pass by copy, not ref)
	boxMin = SubSIMD(boxMin, vTolerance);
	boxMax = AddSIMD(boxMax, vTolerance);


	// for the very  short components of the ray, check if the origin is inside the box;
	// if not, then it doesn't intersect.
	bi32x4 bvOriginOutsideBox = OrSIMD( CmpLtSIMD(origin,boxMin), CmpGtSIMD(origin,boxMax) );
	bvDeltaTinyComponents = SetWToZeroSIMD(bvDeltaTinyComponents);

	// work out entry and exit points for the ray. This may produce strange results for
	// very short delta components, but those will be masked out by bvDeltaTinyComponents
	// anyway. We could early-out on bvOriginOutsideBox, but it won't be ready to branch
	// on for fourteen cycles.
	fltx4 vt1 = SubSIMD( boxMin, origin );
	fltx4 vt2 = SubSIMD( boxMax, origin );
	vt1 = MulSIMD( vt1, invDelta );
	vt2 = MulSIMD( vt2, invDelta );

	// ensure that vt1<vt2
	{
		fltx4 temp = MaxSIMD( vt1, vt2 );
		vt1 = MinSIMD( vt1, vt2 );
		vt2 = temp;
	}

	// Non-parallel case
	// Find the t's corresponding to the entry and exit of
	// the ray along x, y, and z. The find the furthest entry
	// point, and the closest exit point. Once that is done,
	// we know we don't collide if the closest exit point
	// is behind the starting location. We also don't collide if
	// the closest exit point is in front of the furthest entry point
	fltx4 closestExit,furthestEntry;
	{
		VectorAligned temp;
		StoreAlignedSIMD(temp.Base(),vt2);
		closestExit = ReplicateX4( MIN( MIN(temp.x,temp.y), temp.z) );

		StoreAlignedSIMD(temp.Base(),vt1);
		furthestEntry = ReplicateX4( MAX( MAX(temp.x,temp.y), temp.z) );
	}


	// now start testing. We bail out if:
	// any component with tiny delta has origin outside the box
	if (!IsAllZeros(AndSIMD(bvOriginOutsideBox, bvDeltaTinyComponents)))
		return false;
	else
	{
		// however if there are tiny components inside the box, we
		// know that they are good. (we didn't really need to run
		// the other computations on them, but it was faster to do
		// so than branching around them).

		// now it's the origin INSIDE box (eg, tiny components & ~outside box)
		bvOriginOutsideBox = AndNotSIMD(bvOriginOutsideBox,bvDeltaTinyComponents);
	}

	// closest exit is in front of furthest entry
	bi32x4 tminOverTmax = CmpGtSIMD( furthestEntry, closestExit );
	// closest exit is behind start, or furthest entry after end
	bi32x4 outOfBounds = OrSIMD( CmpGtSIMD(furthestEntry, LoadOneSIMD()), CmpGtSIMD( LoadZeroSIMD(), closestExit ) );
	bi32x4 failedComponents = OrSIMD(tminOverTmax, outOfBounds); // any 1's here mean return false
	// but, if a component is tiny and has its origin inside the box, ignore the computation against bogus invDelta.
	failedComponents = AndNotSIMD(bvOriginOutsideBox,failedComponents);
	return ( IsAllZeros( SetWToZeroSIMD( failedComponents ) ) );
}


#if 0  // example code for testing Quaternion48S

#include <vectormath/cpp/vectormath_aos.h>
#include "pixelwriter.h"

struct inpix_t
{
	float channels[4];
};
CON_COMMAND( ps3_testf16, "test float116" )
{
	volatile static bool trapper = true;

	const int nPix = atoi(args[1]);

	CUtlMemory<float16> memOld; memOld.EnsureCapacity( nPix * 4 );
	CUtlMemory<float16> memNu; memNu.EnsureCapacity( nPix * 4 );

	// set to be one huge row of pixels
	CPixelWriter vOldWay;
	vOldWay.SetPixelMemory( IMAGE_FORMAT_RGBA16161616F, memOld.Base(), nPix * 4 * sizeof(float16) );
	vOldWay.bIgnorePS3NOCHECKIN = true;
	CPixelWriter vNewWay;
	vNewWay.bIgnorePS3NOCHECKIN = false;
	vNewWay.SetPixelMemory( IMAGE_FORMAT_RGBA16161616F, memOld.Base(), nPix * 4 * sizeof(float16) );



	CUtlVector<inpix_t> indata; indata.EnsureCount( nPix );
	static const float mint = exp2f(-14);
	for ( int i = 0 ; i < nPix ; ++i )
	{
		for ( int j = 0 ; j < 4 ; ++j )
		{
			indata[i].channels[j] = RandomFloat( mint, maxfloat16bits * 2 );
		}
	}

	for ( int i = 0 ; i < nPix ; ++i )
	{
		vOldWay.WritePixelF( indata[i].channels[0], indata[i].channels[1], indata[i].channels[2], indata[i].channels[3] );
	}

	vNewWay.WriteManyPixelTo16BitF( indata[0].channels, nPix );

	const float16 *pOldPixels = (const float16 *)vOldWay.GetPixelMemory();
	const float16 *pNewPixels = (const float16 *)vNewWay.GetPixelMemory();

	for ( int i = 0 ; i < nPix * 4  ; ++i )
	{
		if ( pOldPixels[i] != pNewPixels[i] )
		{
			Msg( "%f -> %f  %f\t%x  %x\n", indata[i], pOldPixels[i], pNewPixels[i], pOldPixels[i].GetBits(),  pNewPixels[i].GetBits() );
			Assert(false);
		}
	}

#if 0
	static float fins[65536];

	// build an array of every float that can be represented
	// in a float16
	for ( uint i = 0 ; i <= 65535 ; ++i )
	{
		short s = i;
		float16 foo;
		memcpy( &foo, &s, sizeof(short) );
		fins[i] = foo.GetFloat();
	}

	int bottom; int top;
	{
		float16 fbot; fbot.SetFloat( exp2f(-14) );
		float16 ftop; ftop.SetFloat( maxfloat16bits );
		bottom = fbot.GetBits();
		top = ftop.GetBits();
	}

	// now test conversion back
	for ( uint i = bottom ; i <= top ; i+=4 )
	{
		float16 oldway[4];
		float16 neway[4];

		oldway[0].SetFloat( fins[i+0] );
		oldway[1].SetFloat( fins[i+1] );
		oldway[2].SetFloat( fins[i+2] );
		oldway[3].SetFloat( fins[i+3] );

		float16::ConvertFourFloatsTo16BitsAtOnce( neway, fins+i+0, fins+i+1, fins+i+2, fins+i+3 );

		for ( int q = 0 ; q < 4 ; ++q )
		{

			if ( oldway[q] != neway[q] && neway[q].GetBits() > 0 )
			{
				if ( trapper ) DebuggerBreak();
				Msg( "%f -> %f %f\t%x %x\n",
					fins[i+q], oldway[q].GetFloat(), neway[q].GetFloat(), oldway[q].GetBits(), neway[q].GetBits() );
			}

			/*
			if ( neway[q].GetFloat() > 0 )
			{

				if ( trapper ) DebuggerBreak();
				Msg( "%f -> %f %f\t%x %x\n",
					fins[i+q], oldway[q].GetFloat(), neway[q].GetFloat(), oldway[q].GetBits(), neway[q].GetBits() );
			}
			*/
		}
	}
#endif

#if 0
	float16 a,b;
	a.SetFloat(1.0f);
	b = 1.0f;
	short c = float16::ConvertFloatTo16bits(1.0f);
	short d = float16::ConvertFloatTo16bitsNonDefault<false>(1.0f);

	Msg("%f %f %x %x\n", a,b ,c ,d );

	float foo[128];
	float16 bar[128];
	short quux[128];
	for ( int i = 0 ; i < 128 ; ++i )
	{
		foo[i]  = RandomFloat( -65535, 65535 );
		bar[i]  = foo[i];
		quux[i] = float16::ConvertFloatTo16bitsNonDefault<false>(foo[i]);
	}

	for ( int i = 0 ; i < 128 ; ++i )
	{
		if ( bar[i].GetBits() != quux[i] )
		{
			Msg( "%f -> %f  %x %x \n", foo[i], bar[i].GetFloat(), bar[i].GetBits(), quux[i] );
		}
	}
#endif
}




#endif

