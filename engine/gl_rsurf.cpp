//===== Copyright (c) 1996-2007, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//


#include "render_pch.h"
#include "client.h"
#include "gl_model_private.h"
#include "gl_water.h"
#include "gl_cvars.h"
#include "zone.h"
#include "decal.h"
#include "decal_private.h"
#include "gl_lightmap.h"
#include "r_local.h"
#include "gl_matsysiface.h"
#include "gl_rsurf.h"
#include "materialsystem/imesh.h"
#include "materialsystem/ivballoctracker.h"
#include "tier2/tier2.h"
#include "collisionutils.h"
#include "cdll_int.h"
#include "utllinkedlist.h"
#include "r_areaportal.h"
#include "brushbatchrender.h"
#include "bsptreedata.h"
#include "cmodel_private.h"
#include "tier0/dbg.h"
#include "crtmemdebug.h"
#include "iclientrenderable.h"
#include "icliententitylist.h"
#include "icliententity.h"
#include "gl_rmain.h"
#include "tier0/vprof.h"
#include "bitvec.h"
#include "debugoverlay.h"
#include "host.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "cl_main.h"
#include "cmodel_engine.h"
#include "r_decal.h"
#include "materialsystem/materialsystem_config.h"
#include "materialsystem/imaterialproxy.h"
#include "materialsystem/imaterialvar.h"
#include "coordsize.h"
#include "mempool.h"
#include "tier0/cache_hints.h"
#ifndef DEDICATED
#include "Overlay.h"
#endif
#include "paint.h"
#include "disp.h"
#include "mathlib/volumeculler.h"
#include "vstdlib/jobthread.h"

#if defined(_PS3)
#include "buildindices_PS3.h"
#include "buildworldlists_PS3.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------

class IClientEntity;

// interface to shader drawing
void Shader_BrushBegin( model_t *model, IClientEntity *baseentity = NULL );
void Shader_BrushSurfaceOverride( IMatRenderContext *pRenderContext, SurfaceHandle_t surfID, model_t *model, IClientEntity *baseentity = NULL );
void Shader_BrushEnd( IMatRenderContext *pRenderContext, VMatrix const* brushToWorld, model_t *model, bool bShadowDepth, IClientEntity *baseentity = NULL );
void BuildMSurfaceVertexArrays( worldbrushdata_t *pBrushData, SurfaceHandle_t surfID, CMeshBuilder &builder );

ConVar r_hidepaintedsurfaces( "r_hidepaintedsurfaces", "0", 0, "If enabled, hides all surfaces which have been painted." );

//-----------------------------------------------------------------------------
// Information about the fog volumes for this pass of rendering
//-----------------------------------------------------------------------------

struct FogState_t
{
	MaterialFogMode_t m_FogMode;
	float m_FogStart;
	float m_FogEnd;
	float m_FogColor[3];
	bool m_FogEnabled;
};

struct FogVolumeInfo_t : public FogState_t
{
	bool	m_InFogVolume;
	float	m_FogSurfaceZ;
	float	m_FogMinZ;
	int		m_FogVolumeID;
};

//-----------------------------------------------------------------------------
// Cached convars...
//-----------------------------------------------------------------------------
struct CachedConvars_t
{
	bool	m_bDrawWorld;
	int		m_nDrawLeaf;
	bool	m_bDrawFuncDetail;
};


static CachedConvars_t s_ShaderConvars;

// AR - moved so DEDICATED can access these vars
Frustum_t g_Frustum;

//-----------------------------------------------------------------------------
// Convars
//-----------------------------------------------------------------------------
static ConVar r_drawtranslucentworld( "r_drawtranslucentworld", "1", FCVAR_CHEAT );
static ConVar mat_forcedynamic( "mat_forcedynamic", "0", FCVAR_CHEAT );
static ConVar r_drawleaf( "r_drawleaf", "-1", FCVAR_CHEAT, "Draw the specified leaf." );
static ConVar r_drawworld( "r_drawworld", "1", FCVAR_CHEAT, "Render the world." );
static ConVar r_drawfuncdetail( "r_drawfuncdetail", "1", FCVAR_CHEAT, "Render func_detail" );
static ConVar fog_enable_water_fog( "fog_enable_water_fog", "1", FCVAR_CHEAT );
ConVar r_fastzreject( "r_fastzreject", "0", 0, "Activate/deactivates a fast z-setting algorithm to take advantage of hardware with fast z reject. Use -1 to default to hardware settings" );
static ConVar r_fastzrejectdisp( "r_fastzrejectdisp", "0", 0, "Activates/deactivates fast z rejection on displacements (360 only). Only active when r_fastzreject is on." );

ConVar r_skybox_draw_last( "r_skybox_draw_last", IsPS3()? "1" : "0", 0, "Draws skybox after world brush geometry, rather than before." );


#if defined(_PS3)
ConVar r_PS3_SPU_buildindices( "r_PS3_SPU_buildindices", "1", 0, "0: PPU, 1: SPU, 2: SPU with debug stop on job entry" );
ConVar r_PS3_SPU_buildworldlists( "r_PS3_SPU_buildworldlists", "1", 0, "0: PPU, 1: SPU" );
#endif

ConVar r_csm_static_vb("r_csm_static_vb","1", 0, "Use a precomputed static VB for CSM rendering");
ConVar r_csm_fast_path("r_csm_fast_path","1", FCVAR_DEVELOPMENTONLY, "Use shadow fast path for CSM rendering - minimize number of draw call");

//-----------------------------------------------------------------------------
// Installs a client-side renderer for brush models
//-----------------------------------------------------------------------------
static IBrushRenderer* s_pBrushRenderOverride = 0;

//-----------------------------------------------------------------------------
// Make sure we don't render the same surfaces twice
//-----------------------------------------------------------------------------
int	r_surfacevisframe = 0;
#define r_surfacevisframe dont_use_r_surfacevisframe_here


//-----------------------------------------------------------------------------
// Fast z reject displacements?
//-----------------------------------------------------------------------------
static bool s_bFastZRejectDisplacements = false;

//-----------------------------------------------------------------------------
// Top view bounds
//-----------------------------------------------------------------------------

static bool r_drawtopview = false;
static bool r_bTopViewNoBackfaceCulling = false;
static bool r_bTopViewNoVisCheck = false;
// These have to be explicitly initialized because in debug builds Vector2D's default
// constructor initializes the components to VEC_T_NAN, leading to asserts and errors.
static Vector2D s_OrthographicCenter(0.0f, 0.0f);
static Vector2D s_OrthographicHalfDiagonal(0.0f, 0.0f);
static const CVolumeCuller *s_pTopViewVolumeCuller = NULL;

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CVisitedSurfs 
{
public:
	FORCEINLINE bool VisitSurface( SurfaceHandle_t surfID )
	{
		return !m_bits.TestAndSet( MSurf_Index( surfID ) );
	}

	FORCEINLINE void MarkSurfaceVisited( SurfaceHandle_t surfID )
	{
		m_bits.Set( MSurf_Index( surfID ) );
	}

	FORCEINLINE bool VisitedSurface( SurfaceHandle_t surfID )
	{
		return m_bits.IsBitSet( MSurf_Index( surfID ) );
	}

	FORCEINLINE bool VisitedSurface( int index )
	{
		return m_bits.IsBitSet( index );
	}

	FORCEINLINE int GetSize() { return m_bits.GetNumBits(); }

	void Resize( int nSurfaces )
	{
		m_bits.Resize(nSurfaces);
	}
	void ClearAll()
	{
		m_bits.ClearAll();
	}

	CVarBitVec m_bits;
};


//-----------------------------------------------------------------------------
// Returns planes in brush models
//-----------------------------------------------------------------------------
int R_GetBrushModelPlaneCount( const model_t *model )
{
	return model->brush.nummodelsurfaces;
}

const cplane_t &R_GetBrushModelPlane( const model_t *model, int nIndex, Vector *pOrigin )
{
	SurfaceHandle_t surfID = SurfaceHandleFromIndex( model->brush.firstmodelsurface, model->brush.pShared );
	surfID += nIndex;
	Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );

	if ( pOrigin )
	{
		int vertCount = MSurf_VertCount( surfID );
		if ( vertCount > 0 )
		{
			int nFirstVertex = model->brush.pShared->vertindices[MSurf_FirstVertIndex( surfID )];
			*pOrigin = model->brush.pShared->vertexes[nFirstVertex].position;
		}
		else
		{
			const cplane_t &plane = MSurf_Plane( surfID );
			VectorMultiply( plane.normal, plane.dist, *pOrigin );
		}
	}

	return MSurf_Plane( surfID );
}

//-----------------------------------------------------------------------------
// Computes the centroid of a surface
//-----------------------------------------------------------------------------
void Surf_ComputeCentroid( SurfaceHandle_t surfID, Vector *pVecCentroid )
{
	int nCount = MSurf_VertCount( surfID );
	int nFirstVertIndex = MSurf_FirstVertIndex( surfID );

	float flTotalArea = 0.0f;
	Vector vecNormal;
	pVecCentroid->Init(0,0,0);
	int vertIndex = host_state.worldbrush->vertindices[nFirstVertIndex];
	Vector vecApex = host_state.worldbrush->vertexes[vertIndex].position;
	for (int v = 1; v < nCount - 1; ++v )
	{
		vertIndex = host_state.worldbrush->vertindices[nFirstVertIndex+v];
		Vector v1 = host_state.worldbrush->vertexes[vertIndex].position;
		vertIndex = host_state.worldbrush->vertindices[nFirstVertIndex+v+1];
		Vector v2 = host_state.worldbrush->vertexes[vertIndex].position;
		CrossProduct( v2 - v1, v1 - vecApex, vecNormal );
		float flArea = vecNormal.Length();
		flTotalArea += flArea;
		*pVecCentroid += (vecApex + v1 + v2) * flArea / 3.0f;
	}

	if (flTotalArea)
	{
		*pVecCentroid /= flTotalArea;
	}
}

//-----------------------------------------------------------------------------
// Converts sort infos to lightmap pages
//-----------------------------------------------------------------------------
int SortInfoToLightmapPage( int sortID )
{
	return materialSortInfoArray[sortID].lightmapPageID;
}

#ifndef DEDICATED

class CWorldRenderList : public CRefCounted1<IWorldRenderList>
{
public:
	CWorldRenderList()
	{
	}

	~CWorldRenderList()
	{
		Purge();
	}

#if defined(_PS3)
	static CWorldRenderList *FindOrCreateList_PS3( int nSurfaces, int viewID );
#endif

	static CWorldRenderList *FindOrCreateList( int nSurfaces )
	{
		CWorldRenderList *p = g_Pool.GetObject();
		if ( p->m_VisitedSurfs.GetSize() != nSurfaces )
		{
			p->Init(nSurfaces);
		}
		else
		{
			p->AddRef();
			AssertMsg( p->m_VisitedSurfs.GetSize() == nSurfaces, "World render list pool not cleared between maps" );
		}

		return p;
	}

	static void PurgeAll()
	{
		CWorldRenderList *p;
		while ( ( p = g_Pool.GetObject( false ) ) != NULL )
		{
			p->Purge();
			delete p;
		}
	}

	virtual bool OnFinalRelease()
	{
		Reset();
		g_Pool.PutObject( this );
		return false;
	}

	void Init( int nSurfaces )
	{
		m_SortList.Init(materials->GetNumSortIDs(), 512);
		m_DispSortList.Init(materials->GetNumSortIDs(), 32);
		m_VisitedSurfs.Resize( nSurfaces );
		m_leaves.EnsureCapacity(1024);
#if defined(_PS3)
		m_SPUDecalSurfsToAdd.EnsureCapacity(0x180);
#endif
		m_DecalSurfsToAdd.EnsureCapacity( 512 );
		m_bSkyVisible = false;
		m_bWaterVisible = false;
	}

	void Purge()
	{
		m_leaves.Purge();
		for ( int i = 0; i < MAX_MAT_SORT_GROUPS; i++ )
		{
			m_ShadowHandles[i].Purge();
			m_DlightSurfaces[i].Purge();
			m_PaintedSurfaces[i].Purge();
		}
		m_SortList.Shutdown();
		m_DispSortList.Shutdown();
		m_AlphaSurfaces.Purge();
#if defined(_PS3)
		m_SPUDecalSurfsToAdd.Purge();
#endif
		m_DecalSurfsToAdd.Purge();
	}

	void Reset()
	{
		m_SortList.Reset();
		m_AlphaSurfaces.RemoveAll();
		m_DispSortList.Reset();

		m_bSkyVisible = false;
		m_bWaterVisible = false;
		for (int j = 0; j < MAX_MAT_SORT_GROUPS; ++j)
		{
			//Assert(pRenderList->m_ShadowHandles[j].Count() == 0 );
			m_ShadowHandles[j].RemoveAll();
			m_DlightSurfaces[j].RemoveAll();
			m_PaintedSurfaces[j].RemoveAll();
		}

		// We haven't found any visible leafs this frame
		m_leaves.RemoveAll();

#if defined(_PS3)
		m_SPUDecalSurfsToAdd.RemoveAll();
#endif
		m_DecalSurfsToAdd.RemoveAll();

		m_VisitedSurfs.ClearAll();
	}

	void CountTranslucentSurfaces()
	{
		int count = m_leaves.Count();
		if ( count > 0 )
		{
			int test = m_leaves[0].firstTranslucentSurface;
			for ( int i = 1; i < count; i++ )
			{
				int transCount = m_leaves[i].firstTranslucentSurface - test;
				if ( transCount )
				{
					m_leaves[i-1].translucentSurfaceCount = transCount;
					test = m_leaves[i].firstTranslucentSurface;
				}
			}
			if ( m_leaves[count-1].firstTranslucentSurface != m_AlphaSurfaces.Count() )
			{
				m_leaves[count-1].translucentSurfaceCount = m_AlphaSurfaces.Count() - m_leaves[count-1].firstTranslucentSurface;
			}
		}
	}

	void QueueDecalSurf( SurfaceHandle_t surfID, int renderGroup )
	{
		DecalSurfPair_t decal;
		decal.m_surfID		= surfID;
		decal.m_renderGroup = renderGroup;

		m_DecalSurfsToAdd.AddToTail( decal );
	}

	void AddDecalSurfs( void )
	{
		int count = m_DecalSurfsToAdd.Count();

		for( int i = 0; i < count ; i++ )
		{
			const DecalSurfPair_t& surfPair = m_DecalSurfsToAdd[i];

			DecalSurfaceAdd( surfPair.m_surfID, surfPair.m_renderGroup  );
		}
	}

#if defined(_PS3)
	// kludgy, but get theory of this working first TODO:tidy...

	// these must match the same definitions in job_worldlists.cpp
#define MAX_DRAWN_SURF	0x260
#define MAX_LEAVES		0x520 
#define MAX_DECAL_SURF	0x180

	void EnsureCapacityForSPU( int maxSortID, int surfVisited )
	{
		m_SortList.EnsureCapacityForSPU(maxSortID, MAX_DRAWN_SURF);
		m_DispSortList.EnsureCapacityForSPU(maxSortID, MAX_DRAWN_SURF/8); // was 32
		m_VisitedSurfs.Resize( surfVisited );
		m_leaves.EnsureCapacity(MAX_LEAVES);
		m_AlphaSurfaces.EnsureCapacity(MAX_DRAWN_SURF/2);  // was 512

		for ( int i = 0; i < MAX_MAT_SORT_GROUPS; i++ )
		{
			m_DlightSurfaces[i].EnsureCapacity(MAX_DRAWN_SURF/4);
			m_PaintedSurfaces[i].EnsureCapacity(MAX_DRAWN_SURF/4);
		}

		m_bSkyVisible = false;
		m_bWaterVisible = false;

		m_VisitedSurfs.ClearAll();

		m_SPUDecalSurfsToAdd.EnsureCapacity(MAX_DECAL_SURF);
	}


	void FillOutputParamsForSPU( job_buildworldlists::buildWorldListsDMAOut *pDMAOut )
	{
		// renderlist destination dma data

		// m_SortList
		pDMAOut->m_pSortList_m_list					= uintp(m_SortList.GetMaterialList());
		pDMAOut->m_pSortList_m_listUtlPtr			= uintp(m_SortList.GetMaterialListUtlPtr());
		pDMAOut->m_pSortList_m_groupsShared			= uintp(m_SortList.GetGroupsShared());
		pDMAOut->m_pSortList_m_groupsSharedUtlPtr	= uintp(m_SortList.GetGroupsSharedUtlPtr());
		pDMAOut->m_pSortList_m_groupIndices			= uintp(m_SortList.GetGroupIndices());
		pDMAOut->m_pSortList_m_groupIndicesUtlPtr	= uintp(m_SortList.GetGroupIndicesUtlPtr());
		for( int lp = 0; lp < MAX_MAT_SORT_GROUPS; lp++ )
		{
			pDMAOut->m_pSortList_m_sortGroupLists[lp]		= uintp(m_SortList.GetSortGroupLists( lp ));
			pDMAOut->m_pSortList_m_sortGroupListsUtlPtr[lp]	= uintp(m_SortList.GetSortGroupListsUtlPtr( lp ));
		}

		// m_DispSortList
		pDMAOut->m_pDispSortList_m_list					= uintp(m_DispSortList.GetMaterialList());
		pDMAOut->m_pDispSortList_m_listUtlPtr			= uintp(m_DispSortList.GetMaterialListUtlPtr());
		pDMAOut->m_pDispSortList_m_groupsShared			= uintp(m_DispSortList.GetGroupsShared());
		pDMAOut->m_pDispSortList_m_groupsSharedUtlPtr	= uintp(m_DispSortList.GetGroupsSharedUtlPtr());
		pDMAOut->m_pDispSortList_m_groupIndices			= uintp(m_DispSortList.GetGroupIndices());
		pDMAOut->m_pDispSortList_m_groupIndicesUtlPtr	= uintp(m_DispSortList.GetGroupIndicesUtlPtr());
		for( int lp = 0; lp < MAX_MAT_SORT_GROUPS; lp++ )
		{
			pDMAOut->m_pDispSortList_m_sortGroupLists[lp] = uintp(m_DispSortList.GetSortGroupLists( lp ));
			pDMAOut->m_pDispSortList_m_sortGroupListsUtlPtr[lp] = uintp(m_DispSortList.GetSortGroupListsUtlPtr( lp ));
		}

		// m_AlphaSurfaces
		pDMAOut->m_pAlphaSurfaces					= uintp(m_AlphaSurfaces.Base());
		pDMAOut->m_pAlphaSurfacesUtlPtr				= uintp(&m_AlphaSurfaces);

		// m_DlightSurfaces
		for( int lp = 0; lp < MAX_MAT_SORT_GROUPS; lp++ )
		{
			pDMAOut->m_pDlightSurfaces[lp]			= uintp(m_DlightSurfaces[lp].Base());
			pDMAOut->m_pDlightSurfacesUtlPtr[lp]	= uintp(&m_DlightSurfaces[lp]);
		}

		// m_PaintedSurfaces
		for( int lp = 0; lp < MAX_MAT_SORT_GROUPS; lp++ )
		{
			pDMAOut->m_pPaintedSurfaces[lp]			= uintp(m_PaintedSurfaces[lp].Base());
			pDMAOut->m_pPaintedSurfacesUtlPtr[lp]	= uintp(&m_PaintedSurfaces[lp]);
		}

		// m_leaves
		pDMAOut->m_pLeaves							= uintp(m_leaves.Base());
		pDMAOut->m_pLeavesUtlPtr					= uintp(&m_leaves);

		// m_VisitedSurfs
		pDMAOut->m_pVisitedSurfs					= uintp(m_VisitedSurfs.m_bits.Base());

		// decal surfs to add
		pDMAOut->m_pDecalSurfsToAdd					= uintp(m_SPUDecalSurfsToAdd.Base());
		pDMAOut->m_pDecalSurfsToAddUtlPtr			= uintp(&m_SPUDecalSurfsToAdd);

		// m_bSkyVisible
		pDMAOut->m_pSkyVisible						= uintp(&m_bSkyVisible);

		// m_bWaterVisible
		pDMAOut->m_pWaterVisible					= uintp(&m_bWaterVisible);
	}

	void AddSPUDecalSurfs( void )
	{
		int count = m_SPUDecalSurfsToAdd.Count();

		for( int i = 0; i < count ; i++ )
		{
			job_buildworldlists::decalSurfPair &surfPair = m_SPUDecalSurfsToAdd[i];

			DecalSurfaceAdd( (SurfaceHandle_t)surfPair.m_surfID, surfPair.m_renderGroup  );
		}
	}


	CUtlVector<job_buildworldlists::decalSurfPair> m_SPUDecalSurfsToAdd;

#endif

	struct DecalSurfPair_t
	{
		SurfaceHandle_t m_surfID;
		int				m_renderGroup;
	};

	CMSurfaceSortList m_SortList;
	CMSurfaceSortList m_DispSortList;
	CUtlVector<SurfaceHandle_t> m_AlphaSurfaces;

	//-------------------------------------------------------------------------
	// List of decals to render this frame (need an extra one for brush models)
	//-------------------------------------------------------------------------
	CUtlVector<ShadowDecalHandle_t> m_ShadowHandles[MAX_MAT_SORT_GROUPS];
	
	// list of surfaces with dynamic lightmaps
	CUtlVector<SurfaceHandle_t>	m_DlightSurfaces[MAX_MAT_SORT_GROUPS];

	// PORTAL 2 HACK
	// list of surfaces with paint applied
	CUtlVector<SurfaceHandle_t>	m_PaintedSurfaces[MAX_MAT_SORT_GROUPS];

	//-------------------------------------------------------------------------
	// Used to generate a list of the leaves visited, and in back-to-front order
	// for this frame of rendering
	//-------------------------------------------------------------------------
	CUtlVector<WorldListLeafData_t>	m_leaves;

	//-------------------------------------------------------------------------
	// List of decals queued when building world list as DecalSurfaceAdd is not
	// threadsafe. Decals are then added in R_BuildWorldLists_Epilogue
	//-------------------------------------------------------------------------
	CUtlVector<DecalSurfPair_t> m_DecalSurfsToAdd;

	CVisitedSurfs				m_VisitedSurfs;
	bool						m_bSkyVisible;
	bool						m_bWaterVisible;

	static CObjectPool<CWorldRenderList> g_Pool;
};

CObjectPool<CWorldRenderList> CWorldRenderList::g_Pool;

IWorldRenderList *AllocWorldRenderList()
{
	return CWorldRenderList::FindOrCreateList( host_state.worldbrush->numsurfaces );
}

#if defined(_PS3)
static CWorldRenderList g_Pool_PS3[ MAX_CONCURRENT_BUILDVIEWS ];

// use viewID to ensure a unique list is grabbed from the pool every time
IWorldRenderList *AllocWorldRenderList_PS3( int viewID )
{
	return CWorldRenderList::FindOrCreateList_PS3( host_state.worldbrush->numsurfaces, viewID );
}

CWorldRenderList *CWorldRenderList::FindOrCreateList_PS3( int nSurfaces, int viewID )
{
	if( viewID >= MAX_CONCURRENT_BUILDVIEWS )
	{
		Error("*** Exceeded max concurrent buildviews, FindOrCreateList_PS3 ***\n");
	}

	CWorldRenderList *p = &g_Pool_PS3[ viewID ];

	if ( p->m_VisitedSurfs.GetSize() != nSurfaces )
	{
		p->Init(nSurfaces);
	}
// 	else
// 	{
// 		p->AddRef();
// 		AssertMsg( p->m_VisitedSurfs.GetSize() == nSurfaces, "World render list pool not cleared between maps" );
// 	}

	return p;
}

#endif


//-----------------------------------------------------------------------------
// Activates top view
//-----------------------------------------------------------------------------

void R_DrawTopView( bool enable )
{
	r_drawtopview = enable;
}

void R_TopViewNoBackfaceCulling( bool bDisable )
{
	r_bTopViewNoBackfaceCulling = bDisable;
}

void R_TopViewNoVisCheck( bool bDisable )
{
	r_bTopViewNoVisCheck = bDisable;
}

void R_TopViewBounds( Vector2D const& mins, Vector2D const& maxs )
{
	Vector2DAdd( maxs, mins, s_OrthographicCenter );
	s_OrthographicCenter *= 0.5f;
	Vector2DSubtract( maxs, s_OrthographicCenter, s_OrthographicHalfDiagonal );
}

void R_SetTopViewVolumeCuller( const CVolumeCuller *pTopViewVolumeCuller )
{
	s_pTopViewVolumeCuller = pTopViewVolumeCuller;
}

//-----------------------------------------------------------------------------
// Adds surfaces to list of things to render
//-----------------------------------------------------------------------------
void Shader_TranslucentWorldSurface( CWorldRenderList *pRenderList, SurfaceHandle_t surfID )
{
	Assert( !SurfaceHasDispInfo( surfID ) && (pRenderList->m_leaves.Count() > 0) );

	// Hook into the chain of translucent objects for this leaf
	int sortGroup = MSurf_SortGroup( surfID );
	pRenderList->m_AlphaSurfaces.AddToTail( surfID );
	if ( MSurf_Flags( surfID ) & (SURFDRAW_HASLIGHTSYTLES|SURFDRAW_HASDLIGHT) )
	{
		pRenderList->m_DlightSurfaces[sortGroup].AddToTail( surfID );
	}
}

static inline void Shader_WorldSurface( CWorldRenderList *pRenderList, SurfaceHandle_t surfID )
{
	// Hook it into the list of surfaces to render with this material
	// Do it in a way that generates a front-to-back ordering for fast z reject
	Assert( !SurfaceHasDispInfo( surfID ) );

	// Each surface is in exactly one group
	int nSortGroup = MSurf_SortGroup( surfID );

	// Add decals on non-displacement surfaces
	if( SurfaceHasDecals( surfID ) )
	{
		DecalSurfaceAdd( surfID, nSortGroup );
	}

	int nMaterialSortID = MSurf_MaterialSortID( surfID );

	if ( MSurf_Flags( surfID ) & (SURFDRAW_HASLIGHTSYTLES|SURFDRAW_HASDLIGHT) )
	{
		pRenderList->m_DlightSurfaces[nSortGroup].AddToTail( surfID );
	}

	if ( MSurf_Flags( surfID ) & SURFDRAW_PAINTED )
	{
		pRenderList->m_PaintedSurfaces[nSortGroup].AddToTail( surfID );
	}

	pRenderList->m_SortList.AddSurfaceToTail( surfID, nSortGroup, nMaterialSortID );
}


//-----------------------------------------------------------------------------
// Adds displacement surfaces to list of things to render
//-----------------------------------------------------------------------------
void Shader_TranslucentDisplacementSurface( CWorldRenderList *pRenderList, SurfaceHandle_t surfID )
{
	Assert( SurfaceHasDispInfo( surfID ) && (pRenderList->m_leaves.Count() > 0));

	// For translucent displacement surfaces, they can exist in many
	// leaves. We want to choose the leaf that's closest to the camera
	// to render it in. Thankfully, we're iterating the tree in front-to-back
	// order, so this is very simple.

	// NOTE: You might expect some problems here when displacements cross fog volume 
	// planes. However, these problems go away (I hope!) because the first planes
	// that split a scene are the fog volume planes. That means that if we're
	// in a fog volume, the closest leaf that the displacement will be in will
	// also be in the fog volume. If we're not in a fog volume, the closest
	// leaf that the displacement will be in will not be a fog volume. That should
	// hopefully hide any discontinuities between fog state that occur when 
	// rendering displacements that straddle fog volume boundaries.

	// Each surface is in exactly one group
	int sortGroup = MSurf_SortGroup( surfID );
	if ( MSurf_Flags( surfID ) & (SURFDRAW_HASLIGHTSYTLES|SURFDRAW_HASDLIGHT) )
	{
		pRenderList->m_DlightSurfaces[sortGroup].AddToTail( surfID );
	}
	pRenderList->m_AlphaSurfaces.AddToTail(surfID);
}

static void Shader_DisplacementSurface( CWorldRenderList *pRenderList, SurfaceHandle_t surfID )
{
	Assert( SurfaceHasDispInfo( surfID ) );

	// For opaque displacement surfaces, we're going to build a temporary list of 
	// displacement surfaces in each material bucket, and then add those to
	// the actual displacement lists in a separate pass.
	// We do this to sort the displacement surfaces by material
	// Each surface is in exactly one group
	int nSortGroup = MSurf_SortGroup( surfID );
	int nMaterialSortID = MSurf_MaterialSortID( surfID );
	if ( MSurf_Flags( surfID ) & (SURFDRAW_HASLIGHTSYTLES|SURFDRAW_HASDLIGHT) )
	{
		pRenderList->m_DlightSurfaces[nSortGroup].AddToTail( surfID );
	}


	pRenderList->m_DispSortList.AddSurfaceToTail( surfID, nSortGroup, nMaterialSortID );
}
 
//-----------------------------------------------------------------------------
// Purpose: This draws a single surface using the dynamic mesh
//-----------------------------------------------------------------------------
void Shader_DrawSurfaceDynamic( IMatRenderContext *pRenderContext, SurfaceHandle_t surfID )
{
	if( !SurfaceHasPrims( surfID ) )
	{
		IMesh *pMesh = pRenderContext->GetDynamicMesh( );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_POLYGON, MSurf_VertCount( surfID ) );
		BuildMSurfaceVertexArrays( host_state.worldbrush, surfID, meshBuilder );
		meshBuilder.End();
		pMesh->Draw();
		return;
	}

	mprimitive_t *pPrim = &host_state.worldbrush->primitives[MSurf_FirstPrimID( surfID )];

	if ( pPrim->vertCount )
	{
#ifdef DBGFLAG_ASSERT
		int primType = pPrim->type;
#endif
		IMesh *pMesh = pRenderContext->GetDynamicMesh( false );
		CMeshBuilder meshBuilder;
		for( int i = 0; i < MSurf_NumPrims( surfID ); i++, pPrim++ )
		{
			// Can't have heterogeneous primitive lists
			Assert( primType == pPrim->type );
			switch( pPrim->type )
			{
			case PRIM_TRILIST:
				meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, pPrim->vertCount, pPrim->indexCount );
				break;
			case PRIM_TRISTRIP:
				meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, pPrim->vertCount, pPrim->indexCount );
				break;
			default:
				Assert( 0 );
				return;
			}
			Assert( pPrim->indexCount );
			BuildMSurfacePrimVerts( host_state.worldbrush, pPrim, meshBuilder, surfID );
			BuildMSurfacePrimIndices( host_state.worldbrush, pPrim, meshBuilder );
			meshBuilder.End();
			pMesh->Draw();
		}
	}
	else
	{
		// prims are just a tessellation
		IMesh *pMesh = pRenderContext->GetDynamicMesh( );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, MSurf_VertCount( surfID ), pPrim->indexCount );
		BuildMSurfaceVertexArrays( host_state.worldbrush, surfID, meshBuilder );
		for ( int primIndex = 0; primIndex < pPrim->indexCount; primIndex++ )
		{
			meshBuilder.FastIndex( host_state.worldbrush->primindices[pPrim->firstIndex + primIndex] );
		}

		meshBuilder.End();
		pMesh->Draw();
	}
}


//-----------------------------------------------------------------------------
// Purpose: This draws a single surface using its static mesh
//-----------------------------------------------------------------------------

// NOTE: Since a static vb/dynamic ib IMesh doesn't buffer, we shouldn't use this
// since it causes a lock and drawindexedprimitive per surface! (gary)
void Shader_DrawSurfaceListStatic( IMatRenderContext *pRenderContext, SurfaceHandle_t *pList, int listCount, int triangleCount )
{
	VPROF( "Shader_DrawSurfaceListStatic" );
	if ( 
#ifdef USE_CONVARS
		mat_forcedynamic.GetInt() || 
#endif
		(MSurf_Flags( pList[0] ) & SURFDRAW_WATERSURFACE) )
	{
		for ( int i = 0; i < listCount; i++ )
			Shader_DrawSurfaceDynamic( pRenderContext, pList[i] );
		return;
	}

	if ( triangleCount )
	{
		int indexCount = triangleCount * 3;
		IMesh *pMesh = pRenderContext->GetDynamicMesh( true, g_WorldStaticMeshes[MSurf_MaterialSortID( pList[0] )] );
		CMeshBuilder meshBuilder;

		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 0, indexCount );

		for ( int i = 0; i < listCount; i++ )
		{
			BuildIndicesForWorldSurface( meshBuilder, pList[i], host_state.worldbrush );
		}

		meshBuilder.End();
		pMesh->Draw();
	}
}

//-----------------------------------------------------------------------------
// Sets the lightmapping state
//-----------------------------------------------------------------------------
static inline void Shader_SetChainLightmapState( IMatRenderContext *pRenderContext, SurfaceHandle_t surfID )
{
	if ( g_pMaterialSystemConfig->nFullbright == 1 )
	{
		if( MSurf_Flags( surfID ) & SURFDRAW_BUMPLIGHT )
		{
			pRenderContext->BindLightmapPage( MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP );
		}
		else
		{
			pRenderContext->BindLightmapPage( MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE );
		}
	}
	else
	{
		Assert( MSurf_MaterialSortID( surfID ) >= 0 && MSurf_MaterialSortID( surfID ) < g_WorldStaticMeshes.Count() );
		pRenderContext->BindLightmapPage( materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID );
	}
}


//-----------------------------------------------------------------------------
// Sets the lightmap + texture to render with
//-----------------------------------------------------------------------------
IMaterial *Shader_SetChainTextureState( IMatRenderContext *pRenderContext, SurfaceHandle_t surfID, IClientEntity* pBaseEntity, ERenderDepthMode_t DepthMode )
{
	IMaterial *pSurfaceMaterial = MSurf_TexInfo( surfID )->material;
	if ( DepthMode )
	{
		// Select proper override material
		int nAlphaTest = (int) pSurfaceMaterial->IsAlphaTested();
		int nNoCull = (int) pSurfaceMaterial->IsTwoSided();
		
		IMaterial *pDepthWriteMaterial;
		if ( DepthMode == DEPTH_MODE_SHADOW )
		{
			pDepthWriteMaterial = g_pMaterialDepthWrite[ nAlphaTest ][ nNoCull ];
		}
		else
		{
			pDepthWriteMaterial = g_pMaterialSSAODepthWrite[ nAlphaTest ][ nNoCull ];
		}

		if ( nAlphaTest == 1 )
		{
			static unsigned int originalTextureVarCache = 0;
			IMaterialVar *pOriginalTextureVar = pSurfaceMaterial->FindVarFast( "$basetexture", &originalTextureVarCache );
			static unsigned int originalTextureFrameVarCache = 0;
			IMaterialVar *pOriginalTextureFrameVar = pSurfaceMaterial->FindVarFast( "$frame", &originalTextureFrameVarCache );
			static unsigned int originalAlphaRefCache = 0;
			IMaterialVar *pOriginalAlphaRefVar = pSurfaceMaterial->FindVarFast( "$AlphaTestReference", &originalAlphaRefCache );

			static unsigned int textureVarCache = 0;
			IMaterialVar *pTextureVar = pDepthWriteMaterial->FindVarFast( "$basetexture", &textureVarCache );
			static unsigned int textureFrameVarCache = 0;
			IMaterialVar *pTextureFrameVar = pDepthWriteMaterial->FindVarFast( "$frame", &textureFrameVarCache );
			static unsigned int alphaRefCache = 0;
			IMaterialVar *pAlphaRefVar = pDepthWriteMaterial->FindVarFast( "$AlphaTestReference", &alphaRefCache );

			if( pTextureVar && pOriginalTextureVar )
			{
				pTextureVar->SetTextureValue( pOriginalTextureVar->GetTextureValue() );
			}

			if( pTextureFrameVar && pOriginalTextureFrameVar )
			{
				pTextureFrameVar->SetIntValue( pOriginalTextureFrameVar->GetIntValue() );
			}

			if( pAlphaRefVar && pOriginalAlphaRefVar )
			{
				pAlphaRefVar->SetFloatValue( pOriginalAlphaRefVar->GetFloatValue() );
			}
		}

		pRenderContext->Bind( pDepthWriteMaterial );
		pSurfaceMaterial = pDepthWriteMaterial;
	}
	else
	{
		pRenderContext->Bind( pSurfaceMaterial, pBaseEntity ? pBaseEntity->GetClientRenderable() : NULL );
		Shader_SetChainLightmapState( pRenderContext, surfID );
	}
	return pSurfaceMaterial;
}

static byte flatColor[4] = { 255, 255, 255, 255 };
static byte flatColorNoAlpha[4] = { 255, 255, 255, 0 };
// simple helper class to cache off material properties to avoid conversions, calls, etc in loops
struct texturegen_t 
{
	Vector uAxis;
	float uOffset;
	Vector vAxis;
	float vOffset;
	float invU;
	float invV;
	byte *pColor;
	void Init( SurfaceHandle_t surfID )
	{
		mtexinfo_t* pTexInfo = MSurf_TexInfo( surfID );
		uAxis = pTexInfo->textureVecsTexelsPerWorldUnits[0].AsVector3D();
		uOffset = pTexInfo->textureVecsTexelsPerWorldUnits[0][3];
		vAxis = pTexInfo->textureVecsTexelsPerWorldUnits[1].AsVector3D();
		vOffset = pTexInfo->textureVecsTexelsPerWorldUnits[1][3];
		invU = 1.0f / pTexInfo->material->GetMappingWidth();
		invV = 1.0f / pTexInfo->material->GetMappingHeight();
		// The amount to blend between basetexture and basetexture2 used to sit in lightmap
		// alpha, so we didn't care about the vertex color or vertex alpha. But now if they're
		// using it, we have to make sure the vertex has the color and alpha specified correctly
		// or it will look weird.
		if ( (pTexInfo->texinfoFlags & TEXINFO_USING_BASETEXTURE2) )
		{
			pColor = flatColorNoAlpha;
		}
		else
		{
			pColor = flatColor;
		}
	}
	inline float ComputeU( const Vector &pos ) const
	{
		return invU * (DotProduct(pos, uAxis) + uOffset);
	}
	inline float ComputeV( const Vector &pos ) const
	{
		return invV * (DotProduct(pos, vAxis) + vOffset);
	}
};

void BuildMSurfaceVertexArraysTextureOnly( worldbrushdata_t *pBrushData, SurfaceHandle_t surfID, CMeshBuilder &builder )
{
	int vertCount = MSurf_VertCount(surfID);
	unsigned short *pVertIndex = &pBrushData->vertindices[MSurf_FirstVertIndex( surfID )];
	for ( int i = 0; i < vertCount; i++ )
	{
		// world-space vertex
		// output to mesh
		Vector &vec = pBrushData->vertexes[pVertIndex[i]].position;
		builder.Position3fv( vec.Base() );

		Vector2D uv;
		SurfComputeTextureCoordinate( surfID, vec, uv.Base() );
		builder.TexCoord2fv( 0, uv.Base() );

		builder.Color4ubv( flatColor );
		builder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();
	}
}


void Shader_AddSurfaceDynamicTextureOnly( CMeshBuilder &meshBuilder, SurfaceHandle_t surfID )
{
	int startVert = meshBuilder.VertexCount();
	worldbrushdata_t *pData = host_state.worldbrush;
	BuildMSurfaceVertexArraysTextureOnly( pData, surfID, meshBuilder );
	CIndexBuilder &indexBuilder = meshBuilder;
	if ( SurfaceHasPrims(surfID) )
	{
		mprimitive_t *pPrim = &pData->primitives[MSurf_FirstPrimID( surfID, pData )];
		Assert(pPrim->vertCount==0);
		Assert( pPrim->indexCount == ((MSurf_VertCount( surfID ) - 2)*3));

		indexBuilder.FastIndexList( &pData->primindices[pPrim->firstIndex], startVert, pPrim->indexCount );
	}
	else
	{
		int triangleCount = MSurf_VertCount(surfID)-2;
		indexBuilder.FastPolygon( startVert, triangleCount );
	}
}

void Shader_AddSurfaceDynamic( CMeshBuilder &meshBuilder, SurfaceHandle_t surfID )
{
	int startVert = meshBuilder.VertexCount();
	worldbrushdata_t *pData = host_state.worldbrush;
	BuildMSurfaceVertexArrays( pData, surfID, meshBuilder );
	CIndexBuilder &indexBuilder = meshBuilder;
	if ( SurfaceHasPrims(surfID) )
	{
		mprimitive_t *pPrim = &pData->primitives[MSurf_FirstPrimID( surfID, pData )];
		Assert(pPrim->vertCount==0);
		Assert( pPrim->indexCount == ((MSurf_VertCount( surfID ) - 2)*3));

		indexBuilder.FastIndexList( &pData->primindices[pPrim->firstIndex], startVert, pPrim->indexCount );
	}
	else
	{
		int triangleCount = MSurf_VertCount(surfID)-2;
		indexBuilder.FastPolygon( startVert, triangleCount );
	}
}

static void Shader_DrawDynamicChain( IMatRenderContext *pRenderContext, const CMSurfaceSortList &sortList, const surfacesortgroup_t &group, ERenderDepthMode_t DepthMode )
{
	SurfaceHandle_t surfID = sortList.GetSurfaceAtHead(group);
	if ( !IS_SURF_VALID(surfID))
		return;
	IMaterial *pDrawMaterial = Shader_SetChainTextureState( pRenderContext, surfID, 0, DepthMode );

	int nMaxIndices  = pRenderContext->GetMaxIndicesToRender();
	int nMaxVertices = pRenderContext->GetMaxVerticesToRender( pDrawMaterial );
	int nCurrIndexCount = group.triangleCount*3;
	int nCurrVertexCount = group.vertexCount;
	if ( nCurrIndexCount < nMaxIndices && nCurrVertexCount < nMaxVertices )
	{
		IMesh *pMesh = pRenderContext->GetDynamicMesh( false );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nCurrVertexCount, nCurrIndexCount );
		if ( DepthMode != DEPTH_MODE_NORMAL )
		{
			MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(sortList, group, surfID)
			{
				Shader_AddSurfaceDynamicTextureOnly( meshBuilder, surfID );
			}
			MSL_FOREACH_SURFACE_IN_GROUP_END()
		}
		else
		{
			MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(sortList, group, surfID)
			{
				Shader_AddSurfaceDynamic( meshBuilder, surfID );
			}
			MSL_FOREACH_SURFACE_IN_GROUP_END()
		}
		meshBuilder.End();
		pMesh->Draw();
	}
	else
	{
		// UNDONE: This will have really bad perf on 360.  There's a simple mod to the above code to fix it 
		// but it never happens in l4d so I didn't bother writing/testing that code.
		Assert(0);
		MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(sortList, group, surfID)
		{
			Shader_DrawSurfaceDynamic( pRenderContext, surfID );
		}
		MSL_FOREACH_SURFACE_IN_GROUP_END()
	}
}


void Shader_DrawChainsDynamic( IMatRenderContext *pRenderContext, const CMSurfaceSortList &sortList, int nSortGroup, ERenderDepthMode_t DepthMode )
{
	MSL_FOREACH_GROUP_BEGIN(sortList, nSortGroup, group )
	{
		Shader_DrawDynamicChain( pRenderContext, sortList, group, DepthMode );
	}
	MSL_FOREACH_GROUP_END()
}

struct vertexformatlist_t
{
	unsigned short numbatches;
	unsigned short firstbatch;
	IMesh	*pMesh;
};

struct batchlist_t
{
	SurfaceHandle_t	surfID;		// material and lightmap info
	unsigned short firstIndex;
	unsigned short numIndex;
};


static void Shader_DrawChainsStatic( IMatRenderContext *pRenderContext, const CMSurfaceSortList &sortList, int nSortGroup, ERenderDepthMode_t DepthMode )
{
	//VPROF("DrawChainsStatic");
	CUtlVectorFixed<vertexformatlist_t, MAX_VERTEX_FORMAT_CHANGES> meshList;
	int meshMap[MAX_VERTEX_FORMAT_CHANGES];
	CUtlVectorFixedGrowable<batchlist_t, 512> batchList;
	CUtlVectorFixedGrowable<const surfacesortgroup_t *, 8> dynamicGroups;
	bool bWarn = true;
	CMeshBuilder meshBuilder;

	bool skipBind = false;
	if ( g_pMaterialSystemConfig->nFullbright == 1  )
	{
		skipBind = true;
	}

	const CUtlVector<surfacesortgroup_t *> &groupList = sortList.GetSortList(nSortGroup);
	int count = groupList.Count();

	int i, listIndex = 0;

#if defined(_PS3)
	g_pBuildIndicesJob->m_buildIndicesJobData.EnsureCapacity( count );
#endif

	//PIXEVENT( pRenderContext, "Shader_DrawChainsStatic" );

	int nMaxIndices = pRenderContext->GetMaxIndicesToRender();
	while ( listIndex < count )
	{
		const surfacesortgroup_t &group = *groupList[listIndex];
		SurfaceHandle_t surfID = sortList.GetSurfaceAtHead(group);
		int sortID = MSurf_MaterialSortID( surfID );
		IMesh *pBuildMesh = pRenderContext->GetDynamicMesh( false, g_WorldStaticMeshes[sortID] );
		meshBuilder.Begin( pBuildMesh, MATERIAL_TRIANGLES, 0, nMaxIndices );
		IMesh *pLastMesh = NULL;
		int indexCount = 0;
		int meshIndex  = -1;

		// start SPU job here

#if defined(_PS3)

		if( r_PS3_SPU_buildindices.GetInt() ) //&& ( count < g_pBuildIndicesJob->m_buildIndicesJobData.Count() ) )
		{
			PS3BuildIndicesJobData *pJobData	 = g_pBuildIndicesJob->GetJobData();

			// fill SPU job struct
			buildIndicesJob_SPU *pjob_SPU		 = &pJobData->buildIndicesJobSPU;

			pjob_SPU->debugJob					 = r_PS3_SPU_buildindices.GetInt() == 2;

			pjob_SPU->count						 = count;
			pjob_SPU->maxIndices				 = nMaxIndices;
			pjob_SPU->worldStaticMeshesCount	 = g_WorldStaticMeshes.Count();

			pjob_SPU->listIndex					 = listIndex;
			pjob_SPU->indexCount				 = indexCount;
			pjob_SPU->meshListCount				 = meshList.Count();
			pjob_SPU->batchListCount			 = batchList.Count();
	
			pjob_SPU->pEA_sortList_materiallist  = (void *)sortList.GetMaterialList();

			pjob_SPU->group_listHead			 = (group).listHead;

			pjob_SPU->pEA_worldbrush_surfaces1   = host_state.worldbrush->surfaces1;
			pjob_SPU->pEA_worldbrush_surfaces2   = host_state.worldbrush->surfaces2;
			pjob_SPU->pEA_worldbrush_primitives  = host_state.worldbrush->primitives;
			pjob_SPU->pEA_worldbrush_primindices = host_state.worldbrush->primindices;
			pjob_SPU->worldbrush_numsurfaces     = host_state.worldbrush->numsurfaces;

			pjob_SPU->pEA_indexbuilder_indices   = meshBuilder.m_pIndices;
			pjob_SPU->indexbuilder_indexSize     = meshBuilder.m_nIndexSize;

			// push buildindices job
			job_buildindices::JobDescriptor_t *pJobDescriptor = &pJobData->jobDescriptor;

			pJobDescriptor->header = g_buildIndicesJobDescriptor.header;

			pJobDescriptor->header.useInOutBuffer	= 1;
			pJobDescriptor->header.sizeStack		= (40*1024)/8;
			pJobDescriptor->header.sizeInOrInOut	= 0;
			pJobDescriptor->header.sizeDmaList		= 0;

			AddInputDma( pJobDescriptor, sizeof(buildIndicesJob_SPU), pjob_SPU );
			AddInputDma( pJobDescriptor, ROUNDUPTONEXT16B( sizeof(IMesh *) * g_WorldStaticMeshes.Count() ), g_WorldStaticMeshes.Base() );
			AddInputDma( pJobDescriptor, ROUNDUPTONEXT16B( sizeof(surfacesortgroup_t *) * (count) ), groupList.Base() );

			// push
			g_pBuildIndicesJob->Push( pJobDescriptor );

			// debug
			if( pjob_SPU->debugJob )
			{
				g_pBuildIndicesJob->Sync();
			}
		}
#endif
		for ( ; listIndex < count; listIndex++ )
		{

			const surfacesortgroup_t &group = *groupList[listIndex];
			surfID = sortList.GetSurfaceAtHead(group);
			Assert( IS_SURF_VALID( surfID ) );
			if ( MSurf_Flags(surfID) & SURFDRAW_DYNAMIC )
			{
				dynamicGroups.AddToTail( &group );
				continue;
			}

			Assert( group.triangleCount > 0 );
			int numIndex = group.triangleCount * 3;
			if ( indexCount + numIndex > nMaxIndices )
			{
				if ( numIndex > nMaxIndices )
				{
					IMaterial *pDrawMaterial = materialSortInfoArray[MSurf_MaterialSortID( surfID )].material;
					DevMsg("Too many faces with the same material in scene! Material: %s, num indices %d (max: %d)\n", pDrawMaterial ? pDrawMaterial->GetName() : "null", numIndex, nMaxIndices );
					break;
				}

				pLastMesh = NULL;
				break;
			}

			sortID = MSurf_MaterialSortID( surfID );

			if ( g_WorldStaticMeshes[sortID] != pLastMesh )
			{
				if( meshList.Count() < MAX_VERTEX_FORMAT_CHANGES - 1 )
				{
					meshIndex = meshList.AddToTail();
					meshList[meshIndex].numbatches = 0;
					meshList[meshIndex].firstbatch = batchList.Count();
					pLastMesh = g_WorldStaticMeshes[sortID];
					Assert( pLastMesh );
					meshList[meshIndex].pMesh = pLastMesh;
				}
				else
				{
					if ( bWarn )
					{
						DevWarning( 2, "Too many vertex format changes in frame, whole world not rendered\n" );
						bWarn = false;
					}
					continue;
				}
			}


			int batchIndex = batchList.AddToTail();
			batchlist_t &batch = batchList[batchIndex];
			batch.firstIndex = indexCount;
			batch.surfID = surfID;
			batch.numIndex = numIndex;
			Assert( indexCount + batch.numIndex < nMaxIndices );
			indexCount += batch.numIndex;

			meshList[meshIndex].numbatches++;


#if defined(_PS3)
			if( r_PS3_SPU_buildindices.GetInt() == 0 )
			{
#endif
			MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(sortList, group, surfID)
			{
				Assert( meshBuilder.m_nFirstVertex == 0 );
				//Msg("surfID %d\n", (uint32)surfID );
				BuildIndicesForWorldSurface( meshBuilder, surfID, host_state.worldbrush );
			}
			MSL_FOREACH_SURFACE_IN_GROUP_END()
#if defined(_PS3)
			}
#endif

		}

#if defined(_PS3)
		if( r_PS3_SPU_buildindices.GetInt() )
		{
 			meshBuilder.AdvanceIndices( indexCount );
		}
#endif

  		// close out the index buffer
 		meshBuilder.End( false, false );


		int meshTotal = meshList.Count();
		VPROF_INCREMENT_COUNTER( "vertex format changes", meshTotal );

		// HACKHACK: Crappy little bubble sort
		// UNDONE: Make the traversal happen so that they are already sorted when you get here.
		// NOTE: Profiled in a fairly complex map.  This is not even costing 0.01ms / frame!
		for ( i = 0; i < meshTotal; i++ )
		{
			meshMap[i] = i;
		}

		bool swapped = true;
		while ( swapped )
		{
			swapped = false;
			for ( i = 1; i < meshTotal; i++ )
			{
				if ( meshList[meshMap[i]].pMesh < meshList[meshMap[i-1]].pMesh )
				{
					int tmp = meshMap[i-1];
					meshMap[i-1] = meshMap[i];
					meshMap[i] = tmp;
					swapped = true;
				}
			}
		}

		pRenderContext->BeginBatch( pBuildMesh );

		for ( int m = 0; m < meshTotal; m++ )
		{
			vertexformatlist_t &mesh = meshList[meshMap[m]];
			IMaterial *pBindMaterial = materialSortInfoArray[MSurf_MaterialSortID( batchList[mesh.firstbatch].surfID )].material;
			Assert( mesh.pMesh && pBuildMesh );
//			IMesh *pMesh = pRenderContext->GetDynamicMesh( false, mesh.pMesh, pBuildMesh, pBindMaterial );
			pRenderContext->BindBatch( mesh.pMesh, pBindMaterial );

			for ( int b = 0; b < mesh.numbatches; b++ )
			{
				batchlist_t &batch = batchList[b+mesh.firstbatch];
				IMaterial *pDrawMaterial = materialSortInfoArray[MSurf_MaterialSortID( batch.surfID )].material;

				if ( DepthMode != DEPTH_MODE_NORMAL )
				{
					// Select proper override material
					int nAlphaTest = (int) pDrawMaterial->IsAlphaTested();
					int nNoCull = (int) pDrawMaterial->IsTwoSided();
					
					IMaterial *pDepthWriteMaterial;

					if ( DepthMode == DEPTH_MODE_SSA0 )
					{
						pDepthWriteMaterial = g_pMaterialSSAODepthWrite[ nAlphaTest ][ nNoCull ];
					}
					else
					{
						pDepthWriteMaterial = g_pMaterialDepthWrite[ nAlphaTest ][ nNoCull ];
					}

					if ( nAlphaTest == 1 )
					{
						static unsigned int originalTextureVarCache = 0;
						IMaterialVar *pOriginalTextureVar = pDrawMaterial->FindVarFast( "$basetexture", &originalTextureVarCache );
						static unsigned int originalTextureFrameVarCache = 0;
						IMaterialVar *pOriginalTextureFrameVar = pDrawMaterial->FindVarFast( "$frame", &originalTextureFrameVarCache );
						static unsigned int originalAlphaRefCache = 0;
						IMaterialVar *pOriginalAlphaRefVar = pDrawMaterial->FindVarFast( "$AlphaTestReference", &originalAlphaRefCache );

						static unsigned int textureVarCache = 0;
						IMaterialVar *pTextureVar = pDepthWriteMaterial->FindVarFast( "$basetexture", &textureVarCache );
						static unsigned int textureFrameVarCache = 0;
						IMaterialVar *pTextureFrameVar = pDepthWriteMaterial->FindVarFast( "$frame", &textureFrameVarCache );
						static unsigned int alphaRefCache = 0;
						IMaterialVar *pAlphaRefVar = pDepthWriteMaterial->FindVarFast( "$AlphaTestReference", &alphaRefCache );

						if( pTextureVar && pOriginalTextureVar )
						{
							pTextureVar->SetTextureValue( pOriginalTextureVar->GetTextureValue() );
						}

						if( pTextureFrameVar && pOriginalTextureFrameVar )
						{
							pTextureFrameVar->SetIntValue( pOriginalTextureFrameVar->GetIntValue() );
						}

						if( pAlphaRefVar && pOriginalAlphaRefVar )
						{
							pAlphaRefVar->SetFloatValue( pOriginalAlphaRefVar->GetFloatValue() );
						}
					}

					pRenderContext->Bind( pDepthWriteMaterial );
				}
				else
				{
					pRenderContext->Bind( pDrawMaterial, NULL );
	
					if ( skipBind )
					{
						if( MSurf_Flags( batch.surfID ) & SURFDRAW_BUMPLIGHT )
						{
							pRenderContext->BindLightmapPage( MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP );
						}
						else
						{
							pRenderContext->BindLightmapPage( MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE );
						}
					}
					else
					{
						int nLightmapPageId = materialSortInfoArray[MSurf_MaterialSortID( batch.surfID )].lightmapPageID;
						pRenderContext->BindLightmapPage( nLightmapPageId );
					}
				}
//				pMesh->Draw( batch.firstIndex, batch.numIndex );
				pRenderContext->DrawBatch( MATERIAL_TRIANGLES, batch.firstIndex, batch.numIndex );
			}
		}
		pRenderContext->EndBatch();


		// if we get here and pLast mesh is NULL and we rendered somthing, we need to loop
		if ( pLastMesh || !meshTotal )
			break;

		meshList.RemoveAll();
		batchList.RemoveAll();
	}
	for ( i = 0; i < dynamicGroups.Count(); i++ )
	{
		Shader_DrawDynamicChain( pRenderContext, sortList, *dynamicGroups[i], DepthMode );
	}

}


#if defined(_PS3)

//-----------------------------------------------------------------------------
// End of frame sync point for SPURS jobs that require it
//-----------------------------------------------------------------------------
void R_FrameEndSPURSSync( int flags )
{
	if( r_PS3_SPU_buildindices.GetInt() )
	{
		g_pBuildIndicesJob->Sync();
	}
}

#endif



//-----------------------------------------------------------------------------
// The following methods will display debugging info in the middle of each surface
//-----------------------------------------------------------------------------
typedef void (*SurfaceDebugFunc_t)( IMatRenderContext *pRenderContext, SurfaceHandle_t surfID, const Vector &vecCentroid );

static void DrawSurfaceID( IMatRenderContext *pRenderContext, SurfaceHandle_t surfID, const Vector &vecCentroid )
{
	char buf[32];
	Q_snprintf(buf, sizeof( buf ), "0x%p", surfID );
	CDebugOverlay::AddTextOverlay( vecCentroid, 0, buf );
}

static void DrawSurfaceIDAsInt( IMatRenderContext *pRenderContext, SurfaceHandle_t surfID, const Vector &vecCentroid )
{
	int nInt = (msurface2_t*)surfID - host_state.worldbrush->surfaces2;
	char buf[32];
	Q_snprintf( buf, sizeof( buf ), "%d", nInt );
	CDebugOverlay::AddTextOverlay( vecCentroid, 0, buf );
}

static void DrawSurfaceMaterial( IMatRenderContext *pRenderContext, SurfaceHandle_t surfID, const Vector &vecCentroid )
{
	mtexinfo_t * pTexInfo = MSurf_TexInfo(surfID);

	const char *pFullMaterialName = pTexInfo->material ? pTexInfo->material->GetName() : "no material";
	const char *pSlash = strrchr( pFullMaterialName, '/' );
	const char *pMaterialName = strrchr( pFullMaterialName, '\\' );
	if (pSlash > pMaterialName)
		pMaterialName = pSlash;
	if (pMaterialName)
		++pMaterialName;
	else
		pMaterialName = pFullMaterialName;

	CDebugOverlay::AddTextOverlay( vecCentroid, 0, pMaterialName );
}


//-----------------------------------------------------------------------------
// Displays the surface id # in the center of the surface.
//-----------------------------------------------------------------------------
void Shader_DrawSurfaceDebuggingInfo( IMatRenderContext *pRenderContext, SurfaceHandle_t *pList, int listCount, SurfaceDebugFunc_t func )
{
	for ( int i = 0; i < listCount; i++ )
	{
		SurfaceHandle_t surfID = pList[i];
		Assert( !SurfaceHasDispInfo( surfID ) );

		// Compute the centroid of the surface
		int nCount = MSurf_VertCount( surfID );
		if (nCount >= 3)
		{
			Vector vecCentroid;
			Surf_ComputeCentroid( surfID, &vecCentroid );
			VectorTransform( vecCentroid, g_BrushToWorldMatrix.As3x4(), vecCentroid );
			func( pRenderContext, surfID, vecCentroid );
		}
	}
}



//-----------------------------------------------------------------------------
// Doesn't draw internal triangles
//-----------------------------------------------------------------------------
void Shader_DrawWireframePolygons( IMatRenderContext *pRenderContext, SurfaceHandle_t *pList, int listCount )
{
	int nLineCount = 0;
	for ( int i = 0; i < listCount; i++ )
	{
		int nCount = MSurf_VertCount( pList[i] );
		if (nCount >= 3)
		{
			nLineCount += nCount;
		}
	}

	if (nLineCount == 0)
		return;

	pRenderContext->Bind( g_materialWorldWireframe );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( false );
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_LINES, nLineCount );

	for ( int i = 0; i < listCount; i++ )
	{
		SurfaceHandle_t surfID = pList[i];
		Assert( !SurfaceHasDispInfo( surfID ) );

		// Compute the centroid of the surface
		int nCount = MSurf_VertCount( surfID );
		if (nCount >= 3)
		{
			int nFirstVertIndex = MSurf_FirstVertIndex( surfID );
			int nVertIndex = host_state.worldbrush->vertindices[nFirstVertIndex + nCount - 1];
			Vector vecPrevPos = host_state.worldbrush->vertexes[nVertIndex].position;
			for (int v = 0; v < nCount; ++v )
			{
				// world-space vertex
				nVertIndex = host_state.worldbrush->vertindices[nFirstVertIndex + v];
				Vector& vec = host_state.worldbrush->vertexes[nVertIndex].position;

				// output to mesh
				meshBuilder.Position3fv( vecPrevPos.Base() );
				meshBuilder.AdvanceVertex();
				meshBuilder.Position3fv( vec.Base() );
				meshBuilder.AdvanceVertex();

				vecPrevPos = vec;
			}
		}
	}

	meshBuilder.End();
	pMesh->Draw();
}


//-----------------------------------------------------------------------------
// Debugging mode, renders the wireframe.
//-----------------------------------------------------------------------------
static void Shader_DrawChainsWireframe(	IMatRenderContext *pRenderContext, SurfaceHandle_t *pList, int listCount )
{
	int nWireFrameMode = WireFrameMode();

	switch( nWireFrameMode )
	{
	case 3:
		// Doesn't draw internal triangles
		Shader_DrawWireframePolygons( pRenderContext, pList, listCount );
		break;

	default:
		{
			if( nWireFrameMode == 2 )
			{
				pRenderContext->Bind( g_materialWorldWireframeZBuffer );
			}
			else
			{
				pRenderContext->Bind( g_materialWorldWireframe );
			}
			for ( int i = 0; i < listCount; i++ )
			{
				SurfaceHandle_t surfID = pList[i];
				Assert( !SurfaceHasDispInfo( surfID ) );
				Shader_DrawSurfaceDynamic( pRenderContext, surfID );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Debugging mode, renders the normals
//-----------------------------------------------------------------------------
static void Shader_DrawChainNormals( IMatRenderContext *pRenderContext, SurfaceHandle_t *pList, int listCount )
{
	Vector p, tVect, tangentS, tangentT;

	worldbrushdata_t *pBrushData = host_state.worldbrush;
	pRenderContext->Bind( g_pMaterialWireframeVertexColor );

	for ( int i = 0; i < listCount; i++ )
	{
		SurfaceHandle_t surfID = pList[i];
		IMesh *pMesh = pRenderContext->GetDynamicMesh( );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_LINES, MSurf_VertCount( surfID ) * 3 );
	
		bool negate = TangentSpaceSurfaceSetup( surfID, tVect );

		int vertID;
		for( vertID = 0; vertID < MSurf_VertCount( surfID ); ++vertID )
		{
			int vertIndex = pBrushData->vertindices[MSurf_FirstVertIndex( surfID )+vertID];
			Vector& pos = pBrushData->vertexes[vertIndex].position;
			Vector& norm = pBrushData->vertnormals[ pBrushData->vertnormalindices[MSurf_FirstVertNormal( surfID )+vertID] ];

			TangentSpaceComputeBasis( tangentS, tangentT, norm, tVect, negate );

			meshBuilder.Position3fv( pos.Base() );
			meshBuilder.Color3ub( 0, 0, 255 );
			meshBuilder.AdvanceVertex();

			VectorMA( pos, 5.0f, norm, p );
			meshBuilder.Position3fv( p.Base() );
			meshBuilder.Color3ub( 0, 0, 255 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv( pos.Base() );
			meshBuilder.Color3ub( 0, 255, 0 );
			meshBuilder.AdvanceVertex();

			VectorMA( pos, 5.0f, tangentT, p );
			meshBuilder.Position3fv( p.Base() );
			meshBuilder.Color3ub( 0, 255, 0 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv( pos.Base() );
			meshBuilder.Color3ub( 255, 0, 0 );
			meshBuilder.AdvanceVertex();

			VectorMA( pos, 5.0f, tangentS, p );
			meshBuilder.Position3fv( p.Base() );
			meshBuilder.Color3ub( 255, 0, 0 );
			meshBuilder.AdvanceVertex();
		}
		
		meshBuilder.End();
		pMesh->Draw();
	}
}

static void Shader_DrawChainBumpBasis( IMatRenderContext *pRenderContext, SurfaceHandle_t *pList, int listCount )
{
	Vector p, tVect, tangentS, tangentT;

	worldbrushdata_t *pBrushData = host_state.worldbrush;
	pRenderContext->Bind( g_pMaterialWireframeVertexColor );

	for ( int i = 0; i < listCount; i++ )
	{
		SurfaceHandle_t surfID = pList[i];
		IMesh *pMesh = pRenderContext->GetDynamicMesh( );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_LINES, MSurf_VertCount( surfID ) * 3 );
	
		bool negate = TangentSpaceSurfaceSetup( surfID, tVect );

		int vertID;
		for( vertID = 0; vertID < MSurf_VertCount( surfID ); ++vertID )
		{
			int vertIndex = pBrushData->vertindices[MSurf_FirstVertIndex( surfID )+vertID];
			Vector& pos = pBrushData->vertexes[vertIndex].position;
			Vector& norm = pBrushData->vertnormals[ pBrushData->vertnormalindices[MSurf_FirstVertNormal( surfID )+vertID] ];

			TangentSpaceComputeBasis( tangentS, tangentT, norm, tVect, negate );

			Vector worldSpaceBumpBasis[3];

			int i;
			for( i = 0; i < 3; i++ )
			{
				worldSpaceBumpBasis[i][0] = 
					g_localBumpBasis[i][0] * tangentS[0] +
					g_localBumpBasis[i][1] * tangentS[1] + 
					g_localBumpBasis[i][2] * tangentS[2];
				worldSpaceBumpBasis[i][1] = 
					g_localBumpBasis[i][0] * tangentT[0] +
					g_localBumpBasis[i][1] * tangentT[1] + 
					g_localBumpBasis[i][2] * tangentT[2];
				worldSpaceBumpBasis[i][2] = 
					g_localBumpBasis[i][0] * norm[0] +
					g_localBumpBasis[i][1] * norm[1] + 
					g_localBumpBasis[i][2] * norm[2];
			}

			meshBuilder.Position3fv( pos.Base() );
			meshBuilder.Color3ub( 255, 0, 0 );
			meshBuilder.AdvanceVertex();

			VectorMA( pos, 5.0f, worldSpaceBumpBasis[0], p );
			meshBuilder.Position3fv( p.Base() );
			meshBuilder.Color3ub( 255, 0, 0 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv( pos.Base() );
			meshBuilder.Color3ub( 0, 255, 0 );
			meshBuilder.AdvanceVertex();

			VectorMA( pos, 5.0f, worldSpaceBumpBasis[1], p );
			meshBuilder.Position3fv( p.Base() );
			meshBuilder.Color3ub( 0, 255, 0 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv( pos.Base() );
			meshBuilder.Color3ub( 0, 0, 255 );
			meshBuilder.AdvanceVertex();

			VectorMA( pos, 5.0f, worldSpaceBumpBasis[2], p );
			meshBuilder.Position3fv( p.Base() );
			meshBuilder.Color3ub( 0, 0, 255 );
			meshBuilder.AdvanceVertex();
		}
		
		meshBuilder.End();
		pMesh->Draw();
	}
}


//-----------------------------------------------------------------------------
// Debugging mode, renders the luxel grid.
//-----------------------------------------------------------------------------
static void Shader_DrawLuxels( IMatRenderContext *pRenderContext, SurfaceHandle_t *pList, int listCount )
{
	pRenderContext->Bind( g_materialDebugLuxels );

	for ( int i = 0; i < listCount; i++ )
	{
		SurfaceHandle_t surfID = pList[i];
		Assert( !SurfaceHasDispInfo( surfID ) );

		// Gotta bind the lightmap page so the rendering knows the lightmap scale
		pRenderContext->BindLightmapPage( materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID );
		Shader_DrawSurfaceDynamic( pRenderContext, surfID );
	}
}

CShaderDebug g_ShaderDebug;


ConVar mat_surfaceid("mat_surfaceid", "0", FCVAR_CHEAT);
ConVar mat_surfacemat("mat_surfacemat", "0", FCVAR_CHEAT);


//-----------------------------------------------------------------------------
// Purpose: 
// Output : static void
//-----------------------------------------------------------------------------
static void ComputeDebugSettings( void )
{
	g_ShaderDebug.wireframe = ShouldDrawInWireFrameMode() || (r_drawworld.GetInt() == 2);
	g_ShaderDebug.normals = mat_normals.GetBool();
	g_ShaderDebug.luxels = mat_luxels.GetBool();
	g_ShaderDebug.bumpBasis = mat_bumpbasis.GetBool();
	g_ShaderDebug.surfaceid = mat_surfaceid.GetInt();
	g_ShaderDebug.surfacematerials = mat_surfacemat.GetBool();
	g_ShaderDebug.TestAnyDebug();
}

//-----------------------------------------------------------------------------
// Draw debugging information
//-----------------------------------------------------------------------------
void DrawDebugInformation( IMatRenderContext *pRenderContext, SurfaceHandle_t *pList, int listCount )
{
	// Overlay with wireframe if we're in that mode
	if( g_ShaderDebug.wireframe )
	{
		Shader_DrawChainsWireframe(pRenderContext, pList, listCount);
	}

	// Overlay with normals if we're in that mode
	if( g_ShaderDebug.normals )
	{
		Shader_DrawChainNormals(pRenderContext, pList, listCount);
	}

	if( g_ShaderDebug.bumpBasis )
	{
		Shader_DrawChainBumpBasis(pRenderContext, pList, listCount);
	}
	
	// Overlay with luxel grid if we're in that mode
	if( g_ShaderDebug.luxels )
	{
		Shader_DrawLuxels(pRenderContext, pList, listCount);
	}

	if ( g_ShaderDebug.surfaceid )
	{
		// Draw the surface id in the middle of the surfaces
		Shader_DrawSurfaceDebuggingInfo( pRenderContext, pList, listCount, (g_ShaderDebug.surfaceid != 2 ) ? DrawSurfaceID : DrawSurfaceIDAsInt );
	}
	else if ( g_ShaderDebug.surfacematerials )
	{
		// Draw the material name in the middle of the surfaces
		Shader_DrawSurfaceDebuggingInfo( pRenderContext, pList, listCount, DrawSurfaceMaterial );
	}
}

static void AddProjectedTextureDecalsToList( CWorldRenderList *pRenderList, int nSortGroup )
{
	const CMSurfaceSortList &sortList = pRenderList->m_SortList;
	MSL_FOREACH_GROUP_BEGIN( sortList, nSortGroup, group )
	{
		MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(sortList, group, surfID)
		{
			Assert( !SurfaceHasDispInfo( surfID ) );
			if ( SHADOW_DECAL_HANDLE_INVALID != MSurf_ShadowDecals( surfID ) )
			{
				// No shadows on water surfaces
				if ((MSurf_Flags( surfID ) & SURFDRAW_NOSHADOWS) == 0)
				{
					MEM_ALLOC_CREDIT();
					pRenderList->m_ShadowHandles[nSortGroup].AddToTail( MSurf_ShadowDecals( surfID ) );
				}
			}
			// Add overlay fragments to list.
			if ( OVERLAY_FRAGMENT_INVALID != MSurf_OverlayFragmentList( surfID ) )
			{
				OverlayMgr()->AddFragmentListToRenderList( nSortGroup, MSurf_OverlayFragmentList( surfID ), false );
			}
		}
		MSL_FOREACH_SURFACE_IN_GROUP_END();
	}
	MSL_FOREACH_GROUP_END()
}

//-----------------------------------------------------------------------------
// Draws all of the opaque non-displacement surfaces queued up previously
//-----------------------------------------------------------------------------
static void Shader_DrawChains( IMatRenderContext *pRenderContext, const CWorldRenderList *pRenderList, int nSortGroup, ERenderDepthMode_t DepthMode )
{
	Assert( !g_EngineRenderer->InLightmapUpdate() );
	VPROF("Shader_DrawChains");
	// Draw chains...
#ifdef USE_CONVARS
	if ( !mat_forcedynamic.GetInt() && !g_pMaterialSystemConfig->bDrawFlat )
#else
	if( 1 )
#endif
	{
		if ( g_VBAllocTracker )
			g_VBAllocTracker->TrackMeshAllocations( "Shader_DrawChainsStatic" );
		Shader_DrawChainsStatic( pRenderContext, pRenderList->m_SortList, nSortGroup, DepthMode );
	}
	else
	{
		if ( g_VBAllocTracker )
			g_VBAllocTracker->TrackMeshAllocations( "Shader_DrawChainsDynamic" );
		Shader_DrawChainsDynamic( pRenderContext, pRenderList->m_SortList, nSortGroup, DepthMode );
	}
	if ( g_VBAllocTracker )
		g_VBAllocTracker->TrackMeshAllocations( NULL );

	if ( !r_hidepaintedsurfaces.GetBool() )
	{
		pRenderContext->SetRenderingPaint( true );

		PIXEVENT( pRenderContext, "Paint" );
		for ( int i = 0; i < pRenderList->m_PaintedSurfaces[ nSortGroup ].Count(); i++ )
		{
			SurfaceHandle_t surfID = pRenderList->m_PaintedSurfaces[ nSortGroup ][i];
#ifdef DBGFLAG_ASSERT
 			bool bSurfacePainted = ( MSurf_Flags( surfID ) & SURFDRAW_PAINTED ) != 0;
 			Assert( bSurfacePainted );
#endif
 
 			IMaterial *pMaterial = MSurf_TexInfo( surfID )->material;
 
			pRenderContext->Bind( pMaterial, NULL );
			Shader_SetChainLightmapState( pRenderContext, surfID );
			Shader_DrawSurfaceDynamic( pRenderContext, surfID );
		}

		pRenderContext->SetRenderingPaint( false );
	}


	if ( DepthMode != DEPTH_MODE_NORMAL )	// Skip debug stuff in shadow depth map
		return;

#ifndef _PS3
#ifdef USE_CONVARS
	if ( g_ShaderDebug.anydebug )
	{
		const CMSurfaceSortList &sortList = pRenderList->m_SortList;
		// Debugging information
		MSL_FOREACH_GROUP_BEGIN(sortList, nSortGroup, group )
		{
			CUtlVector<msurface2_t *> surfList;
			sortList.GetSurfaceListForGroup( surfList, group );
			DrawDebugInformation( pRenderContext, surfList.Base(), surfList.Count() );
		}
		MSL_FOREACH_GROUP_END()

		// displacements
		const CMSurfaceSortList &dispSortList = pRenderList->m_DispSortList;

		MSL_FOREACH_GROUP_BEGIN(dispSortList, nSortGroup, group )
		{
			CUtlVector<msurface2_t *> surfList;
			dispSortList.GetSurfaceListForGroup( surfList, group );
			DispInfo_RenderListDebug( pRenderContext, surfList.Base(), surfList.Count() );
		}
		MSL_FOREACH_GROUP_END()
	}
#endif
#endif
}

//-----------------------------------------------------------------------------
// Draws all of the opaque displacement surfaces queued up previously
//-----------------------------------------------------------------------------
static void Shader_DrawDispChain( IMatRenderContext *pRenderContext, int nSortGroup, const CMSurfaceSortList &list, unsigned long flags, ERenderDepthMode_t DepthMode )
{
	VPROF_BUDGET( "Shader_DrawDispChain", VPROF_BUDGETGROUP_DISPLACEMENT_RENDERING );
	int count = 0;
	msurface2_t **pList;
	MSL_FOREACH_GROUP_BEGIN( list, nSortGroup, group )
	{
		count += group.surfaceCount;
	}
	MSL_FOREACH_GROUP_END()

	if (count)
	{
		pList = (msurface2_t **)stackalloc( count * sizeof(msurface2_t *));
		int i = 0;
		MSL_FOREACH_GROUP_BEGIN( list, nSortGroup, group )
		{
			MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(list,group,surfID)
			{
				pList[i] = surfID;
				++i;
			}
			MSL_FOREACH_SURFACE_IN_GROUP_END()
		}
		MSL_FOREACH_GROUP_END()
		Assert(i==count);

		// draw displacments, batch decals
		DispInfo_RenderListWorld( pRenderContext, nSortGroup, pList, count, g_EngineRenderer->ViewGetCurrent().m_bOrtho, flags, DepthMode );
		stackfree(pList);
	}
}

//-----------------------------------------------------------------------------
// Draws all decals of the opaque displacement surfaces queued up previously
//-----------------------------------------------------------------------------
void Shader_DrawDispChainDecalsAndOverlays( IMatRenderContext *pRenderContext, int nSortGroup, const CMSurfaceSortList &list, unsigned long flags )
{
	VPROF_BUDGET( "Shader_DrawDispChain", VPROF_BUDGETGROUP_DISPLACEMENT_RENDERING );
	int count = 0;
	msurface2_t **pList;
	MSL_FOREACH_GROUP_BEGIN( list, nSortGroup, group )
	{
		count += group.surfaceCount;
	}
	MSL_FOREACH_GROUP_END()

		if (count)
		{
			pList = (msurface2_t **)stackalloc( count * sizeof(msurface2_t *));
			int i = 0;
			MSL_FOREACH_GROUP_BEGIN( list, nSortGroup, group )
			{
				MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(list,group,surfID)
				{
					pList[i] = surfID;
					++i;
				}
				MSL_FOREACH_SURFACE_IN_GROUP_END()
			}
			MSL_FOREACH_GROUP_END()
				Assert(i==count);

			// draw displacments, batch decals
			DispInfo_RenderListDecalsAndOverlays( pRenderContext, nSortGroup, pList, count, g_EngineRenderer->ViewGetCurrent().m_bOrtho, flags );
			stackfree(pList);
		}
}

static void Shader_BuildDynamicLightmaps( CWorldRenderList *pRenderList )
{
	VPROF( "Shader_BuildDynamicLightmaps" );

	R_DLightStartView();

	// Build all lightmaps for opaque surfaces
	for ( int nSortGroup = 0; nSortGroup < MAX_MAT_SORT_GROUPS; ++nSortGroup)
	{
		for ( int i = pRenderList->m_DlightSurfaces[nSortGroup].Count()-1; i >= 0; --i )
		{
			R_CheckForLightmapUpdates( pRenderList->m_DlightSurfaces[nSortGroup].Element(i), 0 );
		}
	}

	R_DLightEndView();
}


//-----------------------------------------------------------------------------
// Compute if we're in or out of a fog volume
//-----------------------------------------------------------------------------
static void ComputeFogVolumeInfo( FogVolumeInfo_t *pFogVolume, const Vector& currentViewOrigin )
{
	pFogVolume->m_InFogVolume = false;
	int leafID = CM_PointLeafnum( currentViewOrigin );
	if( leafID < 0 || leafID >= host_state.worldbrush->numleafs )
		return;

	mleaf_t* pLeaf = &host_state.worldbrush->leafs[leafID];
	pFogVolume->m_FogVolumeID = pLeaf->leafWaterDataID;
	if( pFogVolume->m_FogVolumeID == -1 )
		return;

	pFogVolume->m_InFogVolume = true;

	mleafwaterdata_t* pLeafWaterData = &host_state.worldbrush->leafwaterdata[pLeaf->leafWaterDataID];
	if( pLeafWaterData->surfaceTexInfoID == -1 )
	{
		// Should this ever happen?????
		pFogVolume->m_FogEnabled = false;
		return;
	}
	mtexinfo_t* pTexInfo = &host_state.worldbrush->texinfo[pLeafWaterData->surfaceTexInfoID];

	IMaterial* pMaterial = pTexInfo->material;
	if( pMaterial )
	{
		IMaterialVar* pFogColorVar	= pMaterial->FindVar( "$fogcolor", NULL );
		IMaterialVar* pFogEnableVar = pMaterial->FindVar( "$fogenable", NULL );
		IMaterialVar* pFogStartVar	= pMaterial->FindVar( "$fogstart", NULL );
		IMaterialVar* pFogEndVar	= pMaterial->FindVar( "$fogend", NULL );

		pFogVolume->m_FogEnabled = pFogEnableVar->GetIntValue() ? true : false;
		pFogColorVar->GetVecValue( pFogVolume->m_FogColor, 3 );
		pFogVolume->m_FogStart		= -pFogStartVar->GetFloatValue();
		pFogVolume->m_FogEnd		= -pFogEndVar->GetFloatValue();
		pFogVolume->m_FogSurfaceZ	= pLeafWaterData->surfaceZ;
		pFogVolume->m_FogMinZ		= pLeafWaterData->minZ;
		pFogVolume->m_FogMode		= MATERIAL_FOG_LINEAR;
	}
	else
	{
		static bool bComplained = false;
		if( !bComplained )
		{
			Warning( "***Water vmt missing . . check console for missing materials!***\n" );
			bComplained = true;
		}
		pFogVolume->m_FogEnabled = false;
	}
}


//-----------------------------------------------------------------------------
// Resets a world render list
//-----------------------------------------------------------------------------
void ResetWorldRenderList( CWorldRenderList *pRenderList )
{
	if ( pRenderList )
	{
		pRenderList->Reset();
	}
}


//-----------------------------------------------------------------------------
// Call this before rendering; it clears out the lists of stuff to render
//-----------------------------------------------------------------------------
void Shader_WorldBegin( CWorldRenderList *pRenderList )
{
	// Cache the convars so we don't keep accessing them...
	s_ShaderConvars.m_bDrawWorld = r_drawworld.GetBool();
	s_ShaderConvars.m_nDrawLeaf = r_drawleaf.GetInt();
	s_ShaderConvars.m_bDrawFuncDetail = r_drawfuncdetail.GetBool();

	ResetWorldRenderList( pRenderList );

	// Clear out the decal list
	DecalSurfacesInit( false );

	// Clear out the render lists of overlays
	OverlayMgr()->ClearRenderLists();

	// Clear out the render lists of shadows
	g_pShadowMgr->ClearShadowRenderList( );
}

void Shader_GetSurfVertexAndIndexCount( SurfaceHandle_t surfaceHandle, int *pVertexCount, int *pIndexCount )
{
	*pVertexCount = *pIndexCount = 0;

	if ( SurfaceHasPrims( surfaceHandle ) )
	{
		mprimitive_t *pPrim = &host_state.worldbrush->primitives[MSurf_FirstPrimID( surfaceHandle )];
		// I don't understand why the vertCount would be 0 here, but that's what the old code says
		if ( pPrim->vertCount == 0 )
		{
			*pVertexCount = MSurf_VertCount( surfaceHandle );
			*pIndexCount = pPrim->indexCount;
		}
	}
	else
	{
		// Triangle strip
		*pVertexCount = MSurf_VertCount( surfaceHandle );
		*pIndexCount = ( *pVertexCount - 2 ) * 3;
	}
}

// This was moved out to a separate function to work around a VS2010 PC-only code-gen bug
static void Shader_WorldZFillSurfChain_SinglePrimitive( SurfaceHandle_t surfaceHandle, CMeshBuilder &meshBuilder, int &nStartVert )
{
	mvertex_t *pWorldVerts = host_state.worldbrush->vertexes;
	mprimitive_t *pPrim = &host_state.worldbrush->primitives[MSurf_FirstPrimID( surfaceHandle )];
	if ( pPrim->vertCount == 0 )
	{
		int firstVert = MSurf_FirstVertIndex( surfaceHandle );
		for ( int i = 0; i < MSurf_VertCount(surfaceHandle); i++ )
		{
			int vertIndex = host_state.worldbrush->vertindices[firstVert + i];
			meshBuilder.Position3fv( pWorldVerts[vertIndex].position.Base() );
			meshBuilder.AdvanceVertex();
		}
		for ( int primIndex = 0; primIndex < pPrim->indexCount; primIndex++ )
		{
			meshBuilder.FastIndex( host_state.worldbrush->primindices[pPrim->firstIndex + primIndex] + nStartVert );
		}
	}
}

//-----------------------------------------------------------------------------
// Performs the z-fill
//-----------------------------------------------------------------------------
static void Shader_WorldZFillSurfChain_Single( SurfaceHandle_t surfaceHandle, CMeshBuilder &meshBuilder, int &nStartVert )
{
	mvertex_t *pWorldVerts = host_state.worldbrush->vertexes;
	
	int nSurfTriangleCount = MSurf_VertCount( surfaceHandle ) - 2;

	unsigned short *pVertIndex = &(host_state.worldbrush->vertindices[MSurf_FirstVertIndex( surfaceHandle )]);

	// add surface to this batch
	if ( SurfaceHasPrims(surfaceHandle) )
	{
		Shader_WorldZFillSurfChain_SinglePrimitive( surfaceHandle, meshBuilder, nStartVert );
	}
	else
	{
		switch (nSurfTriangleCount)
		{
		case 1:
			meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
			meshBuilder.AdvanceVertex();
			meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
			meshBuilder.AdvanceVertex();
			meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
			meshBuilder.AdvanceVertex();

			meshBuilder.FastIndex( nStartVert );
			meshBuilder.FastIndex( nStartVert + 1 );
			meshBuilder.FastIndex( nStartVert + 2 );

			break;

		case 2:
			meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
			meshBuilder.AdvanceVertex();
			meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
			meshBuilder.AdvanceVertex();
			meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
			meshBuilder.AdvanceVertex();
			meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
			meshBuilder.AdvanceVertex();
			meshBuilder.FastIndex( nStartVert );
			meshBuilder.FastIndex( nStartVert + 1 );
			meshBuilder.FastIndex( nStartVert + 2 );
			meshBuilder.FastIndex( nStartVert );
			meshBuilder.FastIndex( nStartVert + 2 );
			meshBuilder.FastIndex( nStartVert + 3 );
			break;

		default:
			{
				for ( unsigned short v = 0; v < nSurfTriangleCount; ++v )
				{
					meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
					meshBuilder.AdvanceVertex();

					meshBuilder.FastIndex( nStartVert );
					meshBuilder.FastIndex( nStartVert + v + 1 );
					meshBuilder.FastIndex( nStartVert + v + 2 );
				}

				meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
				meshBuilder.AdvanceVertex();
				meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
				meshBuilder.AdvanceVertex();
			}
			break;
		}
	}
	nStartVert += nSurfTriangleCount + 2;
}

static const int s_DrawWorldListsToSortGroup[MAX_MAT_SORT_GROUPS] = 
{
	MAT_SORT_GROUP_STRICTLY_ABOVEWATER,
	MAT_SORT_GROUP_STRICTLY_UNDERWATER,
	MAT_SORT_GROUP_INTERSECTS_WATER_SURFACE,
	MAT_SORT_GROUP_WATERSURFACE,
};

static ConVar r_flashlightrendermodels(  "r_flashlightrendermodels", "1" );

// NOTE: This is a modified copy of Shader_DrawChainsStatic()
static void Shader_DrawDepthFillChainsStatic( IMatRenderContext *pRenderContext, const CMSurfaceSortList &sortList, int nSortGroup, unsigned long flags )
{
	CUtlVectorFixed<vertexformatlist_t, MAX_VERTEX_FORMAT_CHANGES> meshList;
	int meshMap[MAX_VERTEX_FORMAT_CHANGES];
	CUtlVectorFixedGrowable<batchlist_t, 512> batchList;
	bool bWarn = true;
	CMeshBuilder meshBuilder;
	CUtlVector<const surfacesortgroup_t *> alphatestedGroups;

	const CUtlVector<surfacesortgroup_t *> &groupList = sortList.GetSortList(nSortGroup);
	int count = groupList.Count();

	int i, listIndex = 0;


	IMaterial *pDrawMaterial;
	if ( flags & DRAWWORLDLISTS_DRAW_SSAO )
	{
		pDrawMaterial = g_pMaterialSSAODepthWrite[ 0 ][ 1 ];
	}
	else
	{
		pDrawMaterial = g_pMaterialDepthWrite[ 0 ][ 1 ];
	}

	int nMaxIndices = pRenderContext->GetMaxIndicesToRender();
	while ( listIndex < count )
	{
		const surfacesortgroup_t &group = *groupList[listIndex];
		SurfaceHandle_t surfID = sortList.GetSurfaceAtHead(group);
		int sortID = MSurf_MaterialSortID( surfID );
		IMaterial *pMaterial = MSurf_TexInfo( surfID )->material;
		if ( (MSurf_Flags(surfID) & SURFDRAW_WATERSURFACE) || !g_DepthMeshForSortID[sortID] || pMaterial->IsTranslucent() )
		{
			listIndex++;
			continue;
		}

		if ( pMaterial->IsAlphaTested() )
		{
			listIndex++;
			alphatestedGroups.AddToTail( &group );
			continue;
		}

		IMesh *pBuildMesh = pRenderContext->GetDynamicMesh( false, g_DepthMeshForSortID[sortID] );
		meshBuilder.Begin( pBuildMesh, MATERIAL_TRIANGLES, 0, nMaxIndices );
		IMesh *pLastMesh = NULL;
		int indexCount = 0;
		int meshIndex  = -1;

		for ( ; listIndex < count; listIndex++ )
		{

			const surfacesortgroup_t &group = *groupList[listIndex];
			surfID = sortList.GetSurfaceAtHead(group);
			Assert( IS_SURF_VALID( surfID ) );

			Assert( group.triangleCount > 0 );
			int numIndex = group.triangleCount * 3;
			if ( indexCount + numIndex > nMaxIndices )
			{
				if ( numIndex > nMaxIndices )
				{
					DevMsg("Too many faces with the same material in scene! Material: %s, num indices %d (max: %d)\n", pDrawMaterial ? pDrawMaterial->GetName() : "null", numIndex, nMaxIndices );
					break;
				}

				pLastMesh = NULL;
				break;
			}

			sortID = MSurf_MaterialSortID( surfID );

			if ( g_DepthMeshForSortID[sortID] != pLastMesh )
			{
				if( meshList.Count() < MAX_VERTEX_FORMAT_CHANGES - 1 )
				{
					meshIndex = meshList.AddToTail();
					meshList[meshIndex].numbatches = 0;
					meshList[meshIndex].firstbatch = batchList.Count();
					pLastMesh = g_DepthMeshForSortID[sortID];
					Assert( pLastMesh );
					meshList[meshIndex].pMesh = pLastMesh;
				}
				else
				{
					if ( bWarn )
					{
						DevWarning( 2, "Too many vertex format changes in frame, whole world not rendered\n" );
						bWarn = false;
					}
					continue;
				}
			}


			int batchIndex = batchList.AddToTail();
			batchlist_t &batch = batchList[batchIndex];
			batch.firstIndex = indexCount;
			batch.surfID = surfID;
			batch.numIndex = numIndex;
			Assert( indexCount + batch.numIndex < nMaxIndices );
			indexCount += batch.numIndex;

			meshList[meshIndex].numbatches++;


			MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(sortList, group, surfID)
			{
				if ( MSurf_Flags( surfID ) == 0 )
					continue;
				BuildDepthFillIndicesForWorldSurface( meshBuilder, surfID, host_state.worldbrush );
			}
			MSL_FOREACH_SURFACE_IN_GROUP_END()
		}

		// close out the index buffer
		meshBuilder.End( false, false );

		int meshTotal = meshList.Count();
		// HACKHACK: Crappy little bubble sort
		// UNDONE: Make the traversal happen so that they are already sorted when you get here.
		// NOTE: Profiled in a fairly complex map.  This is not even costing 0.01ms / frame!
		for ( i = 0; i < meshTotal; i++ )
		{
			meshMap[i] = i;
		}

		bool swapped = true;
		while ( swapped )
		{
			swapped = false;
			for ( i = 1; i < meshTotal; i++ )
			{
				if ( meshList[meshMap[i]].pMesh < meshList[meshMap[i-1]].pMesh )
				{
					int tmp = meshMap[i-1];
					meshMap[i-1] = meshMap[i];
					meshMap[i] = tmp;
					swapped = true;
				}
			}
		}

		pRenderContext->BeginBatch( pBuildMesh );

		for ( int m = 0; m < meshTotal; m++ )
		{
			vertexformatlist_t &mesh = meshList[meshMap[m]];
			Assert( mesh.pMesh && pBuildMesh );
			pRenderContext->BindBatch( mesh.pMesh, pDrawMaterial );

			for ( int b = 0; b < mesh.numbatches; b++ )
			{
				batchlist_t &batch = batchList[b+mesh.firstbatch];

				pRenderContext->Bind( pDrawMaterial, NULL );
				pRenderContext->DrawBatch( MATERIAL_TRIANGLES, batch.firstIndex, batch.numIndex );
			}
		}
		pRenderContext->EndBatch();


		// if we get here and pLast mesh is NULL and we rendered somthing, we need to loop
		if ( pLastMesh || !meshTotal )
			break;

		meshList.RemoveAll();
		batchList.RemoveAll();
	}
	// Now draw the alpha-tested groups we stored away earlier


	ERenderDepthMode_t DepthMode;

	if ( flags & DRAWWORLDLISTS_DRAW_SSAO )
	{
		DepthMode = DEPTH_MODE_SSA0;
	}
	else
	{
		DepthMode = DEPTH_MODE_SHADOW;
	}


	for ( int i = 0; i < alphatestedGroups.Count(); i++ )
	{
		Shader_DrawDynamicChain( pRenderContext, sortList, *alphatestedGroups[i], DepthMode );
	}
}

static void Shader_WorldShadowDepthFillStaticVB( IMatRenderContext *pRenderContext, CWorldRenderList *pRenderList, unsigned long flags )
{
	int g;
	const CMSurfaceSortList &sortList = pRenderList->m_SortList;
	for ( g = 0; g < MAX_MAT_SORT_GROUPS; ++g )
	{
		if ( ( flags & ( 1 << g ) ) == 0 )
			continue;

		int nSortGroup = s_DrawWorldListsToSortGroup[g];

		Shader_DrawDepthFillChainsStatic( pRenderContext, sortList, nSortGroup, flags );
		if ( ( flags & DRAWWORLDLISTS_DRAW_SKIP_DISPLACEMENTS ) == 0 )
		{
			// Draws opaque displacement surfaces along with shadows, overlays, flashlights, etc.
			Shader_DrawDispChain( pRenderContext, nSortGroup, pRenderList->m_DispSortList, flags, DEPTH_MODE_SHADOW );
		}
	}
}

//-----------------------------------------------------------------------------
// Performs the shadow depth texture fill
//-----------------------------------------------------------------------------
static void Shader_WorldShadowDepthFill( IMatRenderContext *pRenderContext, CWorldRenderList *pRenderList, unsigned long flags )
{
	if ( r_csm_static_vb.GetBool() )
	{
		Shader_WorldShadowDepthFillStaticVB( pRenderContext, pRenderList, flags );
		return;
	}
	// First, count the number of vertices + indices
	int nVertexCount = 0;
	int nIndexCount = 0;
	ERenderDepthMode_t DepthMode = DEPTH_MODE_SHADOW;
	if ( flags & DRAWWORLDLISTS_DRAW_SSAO )
	{
		DepthMode = DEPTH_MODE_SSA0;
	}


	int g;
	CUtlVector<const surfacesortgroup_t *> alphatestedGroups;

	const CMSurfaceSortList &sortList = pRenderList->m_SortList;
	for ( g = 0; g < MAX_MAT_SORT_GROUPS; ++g )
	{
		if ( ( flags & ( 1 << g ) ) == 0 )
			continue;

		int nSortGroup = s_DrawWorldListsToSortGroup[g];
		MSL_FOREACH_GROUP_BEGIN(sortList, nSortGroup, group )
		{
			SurfaceHandle_t surfID = sortList.GetSurfaceAtHead(group);
			if ( MSurf_Flags( surfID ) & SURFDRAW_WATERSURFACE )
				continue;
			IMaterial *pMaterial = MSurf_TexInfo( surfID )->material;

			if( pMaterial->IsTranslucent() )
				continue;

			if ( pMaterial->IsAlphaTested() )
			{
				alphatestedGroups.AddToTail( &group );
				continue;
			}

			nVertexCount += group.vertexCount;
			nIndexCount += group.triangleCount*3;
		}
		MSL_FOREACH_GROUP_END()

		if ( ( flags & DRAWWORLDLISTS_DRAW_SKIP_DISPLACEMENTS ) == 0 )
		{
			// Draws opaque displacement surfaces along with shadows, overlays, flashlights, etc.
			Shader_DrawDispChain( pRenderContext, nSortGroup, pRenderList->m_DispSortList, flags, DepthMode );
		}
	}
	if ( nVertexCount == 0 )
		return;
 
	//this bind needs to be before the GetDynamic Mesh call, changes the vertex size. tmauer.
	IMaterial *pDrawMaterial;

	if ( DepthMode == DEPTH_MODE_SHADOW )
	{
		pDrawMaterial = g_pMaterialDepthWrite[ 0 ][ 1 ];
	}
	else
	{
		pDrawMaterial = g_pMaterialSSAODepthWrite[ 0 ][ 1 ];
	}
	pRenderContext->Bind( pDrawMaterial );

	IMesh *pMesh = pRenderContext->GetDynamicMesh( false );

	int nMaxIndices  = pRenderContext->GetMaxIndicesToRender();
	int nMaxVertices = pRenderContext->GetMaxVerticesToRender( pDrawMaterial );	// opaque, nocull

	// nBatchIndexCount and nBatchVertexCount are the number of indices and vertices we can fit in this batch
	// Each batch must have fewer than nMaxIndices and nMaxVertices or the material system will fail
	int nBatchIndexCount  = MIN( nIndexCount,  nMaxIndices  );
	int nBatchVertexCount = MIN( nVertexCount, nMaxVertices );


	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nBatchVertexCount, nBatchIndexCount );

	int nStartVert = 0;
	for ( g = 0; g < MAX_MAT_SORT_GROUPS; ++g )
	{
		if ( ( flags & ( 1 << g ) ) == 0 )
			continue;

		int nSortGroup = s_DrawWorldListsToSortGroup[g];
		MSL_FOREACH_GROUP_BEGIN(sortList, nSortGroup, group )
		{
			SurfaceHandle_t surfID = sortList.GetSurfaceAtHead(group);
			// Check to see if we can add this list to the current batch...
			if ( ( group.triangleCount == 0 ) || ( group.vertexCount == 0 ) )
				continue;

			IMaterial *pMaterial = MSurf_TexInfo( surfID )->material;

			// Opaque only on this loop
			if( pMaterial->IsTranslucent() ||  pMaterial->IsAlphaTested() )
				continue;

			MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(sortList, group, nSurfID)
			{
				// Draw all surfaces except for water surfaces since it may move up or down to fixup water transitions.
				if ( MSurf_Flags( nSurfID ) == 0 || ( MSurf_Flags( nSurfID ) & SURFDRAW_WATERSURFACE ) != 0 )
					continue;

				int nSurfaceVertexCount, nSurfaceIndexCount;
				Shader_GetSurfVertexAndIndexCount( nSurfID, &nSurfaceVertexCount, &nSurfaceIndexCount );

				if ( nSurfaceVertexCount > nMaxVertices || nSurfaceIndexCount > nMaxIndices )
				{
					// Too many vertices/indices in a batch, no simple way to split the batch
					Error( "Too many vertices (%d, max: %d) or indices (%d, max: %d) in surface.\n", nSurfaceVertexCount, nMaxVertices, nSurfaceIndexCount, nMaxIndices );
					continue;
				}

				if ( nBatchIndexCount < nSurfaceIndexCount || nBatchVertexCount < nSurfaceVertexCount )
				{
					// Surface doesn't fit, flush the current batch.
					meshBuilder.End();
					pMesh->Draw();
					nBatchIndexCount  = MIN( nIndexCount,  nMaxIndices  );
					nBatchVertexCount = MIN( nVertexCount, nMaxVertices );
					pMesh = pRenderContext->GetDynamicMesh( false );
					meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nBatchVertexCount, nBatchIndexCount );
					nStartVert = 0;
				}

				Shader_WorldZFillSurfChain_Single( nSurfID, meshBuilder, nStartVert );

				nBatchIndexCount -= nSurfaceIndexCount;
				nBatchVertexCount -= nSurfaceVertexCount;
				nIndexCount -= nSurfaceIndexCount;
				nVertexCount -= nSurfaceVertexCount;
			}
			MSL_FOREACH_SURFACE_IN_GROUP_END()
		}
		MSL_FOREACH_GROUP_END()
	}

	meshBuilder.End();
	pMesh->Draw();

	// Now draw the alpha-tested groups we stored away earlier
	for ( int i = 0; i < alphatestedGroups.Count(); i++ )
	{
		Shader_DrawDynamicChain( pRenderContext, sortList, *alphatestedGroups[i], DepthMode );
	}
}

struct WorldShadowMeshInfo_t
{
	IMesh *m_pMesh;
	CCopyableUtlVectorFixed< const surfacesortgroup_t *, 1024 > m_Groups;
};

// Minimize the number of draw calls - try to have one draw call per shadow mesh (cf g_Meshes build in WorldStaticMeshCreate)
static void Shader_WorldShadowDepthFillFast( IMatRenderContext *pRenderContext, CWorldRenderList *pRenderList, unsigned long flags )
{
	// Draws opaque displacement surfaces along with shadows, overlays, flashlights, etc.
	if ( ( flags & DRAWWORLDLISTS_DRAW_SKIP_DISPLACEMENTS ) == 0 )
	{
		for ( int g = 0; g < MAX_MAT_SORT_GROUPS; ++g )
		{
			if ( ( flags & ( 1 << g ) ) == 0 )
				continue;

			int nSortGroup = s_DrawWorldListsToSortGroup[g];

			ERenderDepthMode_t DepthMode = ( flags & DRAWWORLDLISTS_DRAW_SSAO ) ? DEPTH_MODE_SSA0 : DEPTH_MODE_SHADOW;

			Shader_DrawDispChain( pRenderContext, nSortGroup, pRenderList->m_DispSortList, flags, DepthMode );
		}
	}

	//this bind needs to be before the GetDynamic Mesh call, changes the vertex size. tmauer.

	IMaterial *pDrawMaterial;

	if ( flags & DRAWWORLDLISTS_DRAW_SSAO )
	{
		pDrawMaterial = g_pMaterialSSAODepthWrite[ 0 ][ 1 ];
	}
	else
	{
		pDrawMaterial = g_pMaterialDepthWrite[ 0 ][ 1 ];
	}

	pRenderContext->Bind( pDrawMaterial );

	int nMaxIndices = pRenderContext->GetMaxIndicesToRender();
	
	CUtlVectorFixedGrowable< const surfacesortgroup_t *, 1024 > alphatestedGroups;
	CUtlVectorFixedGrowable< WorldShadowMeshInfo_t, 32 > meshinfos;
	const CMSurfaceSortList &sortList = pRenderList->m_SortList;
	for ( int g = 0; g < MAX_MAT_SORT_GROUPS; ++g )
	{
		if ( ( flags & ( 1 << g ) ) == 0 )
			continue;

		int nSortGroup = s_DrawWorldListsToSortGroup[g];
		MSL_FOREACH_GROUP_BEGIN(sortList, nSortGroup, group )
		{
			// Don't bother with empty groups
			if ( ( group.triangleCount == 0 ) || ( group.vertexCount == 0 ) )
				continue;
			
			SurfaceHandle_t surfID = sortList.GetSurfaceAtHead(group);
			int nFlags = MSurf_Flags( surfID );
			IMaterial *pMaterial = MSurf_TexInfo( surfID )->material;
			int sortID = MSurf_MaterialSortID( surfID );
			IMesh* pMesh = g_DepthMeshForSortID[sortID];
			if ( ( nFlags & SURFDRAW_WATERSURFACE ) || !pMesh || pMaterial->IsTranslucent() )
			{
				continue;
			}

			if ( pMaterial->IsAlphaTested() )
			{
				alphatestedGroups.AddToTail( &group );
				continue;
			}

			// Sort group per mesh
			// (Search by iterating over the vector (since the vector contains very few element - replace with a map if it becomes a bottleneck))
			bool bFound = false;
			for ( int i = 0; i < meshinfos.Count(); ++i )
			{
				if ( meshinfos[i].m_pMesh == pMesh )
				{
					meshinfos[i].m_Groups.AddToTail( &group );
					bFound = true;
				}
			}
			if ( !bFound )
			{
				meshinfos.AddToTail();
				meshinfos.Tail().m_pMesh = pMesh;
				meshinfos.Tail().m_Groups.AddToTail( &group );
			}
		}
		MSL_FOREACH_GROUP_END()

		CMeshBuilder meshBuilder;
		for ( int i = 0; i < meshinfos.Count(); i++ )
		{
			WorldShadowMeshInfo_t& meshinfo = meshinfos[i];

			IMesh *pBuildMesh = pRenderContext->GetDynamicMesh( false );
			meshBuilder.Begin( pBuildMesh, MATERIAL_TRIANGLES, 0, nMaxIndices );

			int nIndexCount = 0;
			int nBatchIndexCount  = nMaxIndices;
			for ( int j = 0; j < meshinfo.m_Groups.Count(); ++j )
			{
				const surfacesortgroup_t &group = *meshinfo.m_Groups[j];
				int nGroupIndexCount = group.triangleCount*3;
				const SurfaceHandle_t surfID = sortList.GetSurfaceAtHead(group);
				NOTE_UNUSED( surfID );

				Assert( IS_SURF_VALID( surfID ) );

				bool bSplitPerSurface = ( nGroupIndexCount > nMaxIndices );
				if ( !bSplitPerSurface )
				{
					// Is there room enough for these surfaces?
					if ( nBatchIndexCount < nGroupIndexCount )
					{
						// Surfaces don't fit, flush the current batch.
						meshBuilder.End( false, false );

						pRenderContext->BeginBatch( pBuildMesh );
						pRenderContext->BindBatch( meshinfo.m_pMesh, pDrawMaterial );
						pRenderContext->Bind( pDrawMaterial, NULL );
						pRenderContext->DrawBatch( MATERIAL_TRIANGLES, 0, nIndexCount );
						pRenderContext->EndBatch();

						nBatchIndexCount  = nMaxIndices;
						nIndexCount = 0;
						pBuildMesh = pRenderContext->GetDynamicMesh( false );
						meshBuilder.Begin( pBuildMesh, MATERIAL_TRIANGLES, 0, nMaxIndices );
					}

					nBatchIndexCount  -= nGroupIndexCount;
					nIndexCount += nGroupIndexCount;
				}

				MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(sortList, group, surfID)
				{
					if ( MSurf_Flags( surfID ) == 0 )
						continue;

					if ( bSplitPerSurface )
					{
						int nSurfaceVertexCount, nSurfaceIndexCount;
						Shader_GetSurfVertexAndIndexCount( surfID, &nSurfaceVertexCount, &nSurfaceIndexCount );

						// Is there room enough for this surface?
						if ( nBatchIndexCount < nSurfaceIndexCount )
						{
							// Surfaces don't fit, flush the current batch.
							meshBuilder.End( false, false );
							
							pRenderContext->BeginBatch( pBuildMesh );
							pRenderContext->BindBatch( meshinfo.m_pMesh, pDrawMaterial );
							pRenderContext->Bind( pDrawMaterial, NULL );
							pRenderContext->DrawBatch( MATERIAL_TRIANGLES, 0, nIndexCount );
							pRenderContext->EndBatch();

							nBatchIndexCount  = nMaxIndices;
							nIndexCount = 0;
							pBuildMesh = pRenderContext->GetDynamicMesh( false );
							meshBuilder.Begin( pBuildMesh, MATERIAL_TRIANGLES, 0, nMaxIndices );
						}

						nBatchIndexCount  -= nSurfaceIndexCount;
						nIndexCount += nSurfaceIndexCount;
					}

					BuildDepthFillIndicesForWorldSurface( meshBuilder, surfID, host_state.worldbrush );
				}
				MSL_FOREACH_SURFACE_IN_GROUP_END()
			}

			meshBuilder.End( false, false );

			pRenderContext->BeginBatch( pBuildMesh );
			pRenderContext->BindBatch( meshinfo.m_pMesh, pDrawMaterial );
			pRenderContext->Bind( pDrawMaterial, NULL );
			pRenderContext->DrawBatch( MATERIAL_TRIANGLES, 0, nIndexCount );
			pRenderContext->EndBatch();

			meshinfo.m_Groups.RemoveAll();
		}
		meshinfos.RemoveAll();
	}
	
	// Now draw the alpha-tested groups we stored away earlier

	ERenderDepthMode_t DepthMode = DEPTH_MODE_SHADOW;
	if ( flags & DRAWWORLDLISTS_DRAW_SSAO )
	{
		DepthMode = DEPTH_MODE_SSA0;
	}

	for ( int i = 0; i < alphatestedGroups.Count(); i++ )
	{
		Shader_DrawDynamicChain( pRenderContext, sortList, *alphatestedGroups[i], DepthMode );
	}
}

//-----------------------------------------------------------------------------
// Performs the shadow depth texture fill
//-----------------------------------------------------------------------------
static void Shader_WorldShadowDepthFillX360( IMatRenderContext *pRenderContext, CWorldRenderList *pRenderList, unsigned long flags )
{
	PIXEVENT( pRenderContext, "Shader_WorldShadowDepthFillX360()" );

	// FIXME: Batch this up with fast path style rendering!
	// Draws opaque displacement surfaces along with shadows, overlays, flashlights, etc.
	for ( int g = 0; g < MAX_MAT_SORT_GROUPS; ++g )
	{
		if ( ( flags & ( 1 << g ) ) == 0 )
			continue;

		int nSortGroup = s_DrawWorldListsToSortGroup[g];
		Shader_DrawDispChain( pRenderContext, nSortGroup, pRenderList->m_DispSortList, flags, DEPTH_MODE_SHADOW );
	}

	// nBatchIndexCount is the number of indices we can fit in this batch
	int nMaxIndices = pRenderContext->GetMaxIndicesToRender();

	// First, count the number of indices and instances
	int nInstanceCount = 0;
	int nIndexCount = 0;

	CUtlVectorFixedGrowable< const surfacesortgroup_t *, 1024 > alphatestedGroups;
	CUtlVectorFixedGrowable< const surfacesortgroup_t *, 1024 > groups;
	CUtlVectorFixedGrowable< const surfacesortgroup_t *, 128 > groupsBlowingIndexBufferLimit;
	const CMSurfaceSortList &sortList = pRenderList->m_SortList;
	for ( int g = 0; g < MAX_MAT_SORT_GROUPS; ++g )
	{
		if ( ( flags & ( 1 << g ) ) == 0 )
			continue;

		int nSortGroup = s_DrawWorldListsToSortGroup[g];
		MSL_FOREACH_GROUP_BEGIN( sortList, nSortGroup, group )
		{
			// Don't bother with empty groups
			if ( ( group.triangleCount == 0 ) || ( group.vertexCount == 0 ) )
				continue;

			SurfaceHandle_t surfID = sortList.GetSurfaceAtHead(group);
			int nFlags = MSurf_Flags( surfID );
			if ( nFlags & ( SURFDRAW_WATERSURFACE | SURFDRAW_TRANS ) )
				continue;
			if ( nFlags & SURFDRAW_ALPHATEST )
			{
				alphatestedGroups.AddToTail( &group );
				continue;
			}

			groups.AddToTail( &group );
			int nGroupIndexCount = group.triangleCount * 3;
			nInstanceCount += nGroupIndexCount / nMaxIndices;
			nInstanceCount += ( nGroupIndexCount % nMaxIndices ) ? 1 : 0;
			nIndexCount += nGroupIndexCount;
		}
		MSL_FOREACH_GROUP_END()
	}

	// Now draw the alpha-tested groups we stored away earlier
	IMaterial *pDrawMaterial = g_pMaterialDepthWrite[ 0 ][ 1 ];

	ERenderDepthMode_t DepthMode = DEPTH_MODE_SHADOW;
	if ( flags & DRAWWORLDLISTS_DRAW_SSAO )
	{
		pDrawMaterial = g_pMaterialSSAODepthWrite[ 0 ][ 1 ];
		DepthMode = DEPTH_MODE_SSA0;
	}

	for ( int i = 0; i < alphatestedGroups.Count(); i++ )
	{
		Shader_DrawDynamicChain( pRenderContext, sortList, *alphatestedGroups[ i ], DepthMode );
	}

	if ( nIndexCount == 0 )
		return;

	// nBatchIndexCount is the number of indices we can fit in this batch	int nMaxIndices = pRenderContext->GetMaxIndicesToRender();
	int nBatchIndexCount  = MIN( nIndexCount,  nMaxIndices  );

	pRenderContext->Bind( pDrawMaterial );

	IIndexBuffer *pBuildIndexBuffer = pRenderContext->GetDynamicIndexBuffer();
	CIndexBuilder indexBuilder( pBuildIndexBuffer, MATERIAL_INDEX_FORMAT_16BIT );
	indexBuilder.Lock( nBatchIndexCount, 0 );
	int nIndexOffset = indexBuilder.Offset() / sizeof(uint16);

	int nCurrInstanceCount = 0;
	CMatRenderData< MeshInstanceData_t > meshInstanceData( pRenderContext, nInstanceCount );
	MeshInstanceData_t *pMeshInstances = meshInstanceData.Base();
	if ( !pMeshInstances )
		return;
	int nCount = groups.Count();
	for ( int g = 0; g < nCount; ++g )
	{
		const surfacesortgroup_t &group = *(groups[g]);

		SurfaceHandle_t nSurfID = sortList.GetSurfaceAtHead( group );
		int nSortID = MSurf_MaterialSortID( nSurfID );
		int nCurrIndexCount = group.triangleCount*3;

		if ( nCurrIndexCount > nMaxIndices )
		{
			//Warning( "Too many indices\n" );
			groupsBlowingIndexBufferLimit.AddToTail( &group );
			continue;
		}

		// Is there room enough for these surfaces?
		if ( nBatchIndexCount < nCurrIndexCount )
		{
			// Nope, fire off the current batch...
			indexBuilder.Unlock();
			pRenderContext->DrawInstances( nCurrInstanceCount, pMeshInstances );
			nBatchIndexCount  = MIN( nIndexCount,  nMaxIndices  );
			indexBuilder.Lock( nBatchIndexCount, 0 );
			pMeshInstances += nCurrInstanceCount;
			nInstanceCount -= nCurrInstanceCount;
			nCurrInstanceCount = 0;
			nIndexOffset = indexBuilder.Offset() / sizeof(uint16);
		}

		nBatchIndexCount  -= nCurrIndexCount;
		nIndexCount       -= nCurrIndexCount;

		Assert( nCurrInstanceCount < nInstanceCount );
		MeshInstanceData_t &currInstance = pMeshInstances[nCurrInstanceCount++];
		memset( &currInstance, 0, sizeof(MeshInstanceData_t) );
		currInstance.m_nPrimType = MATERIAL_TRIANGLES;
		currInstance.m_pIndexBuffer = pBuildIndexBuffer;
		currInstance.m_pVertexBuffer = g_WorldStaticMeshes[ nSortID ];
		currInstance.m_nIndexOffset = indexBuilder.IndexCount();
		currInstance.m_DiffuseModulation.Init( 1.0f, 1.0f, 1.0f, 1.0f ); 
		MSL_FOREACH_SURFACE_IN_GROUP_BEGIN( sortList, group, nSurfID )
		{
			BuildIndicesForWorldSurface( indexBuilder, nSurfID, host_state.worldbrush );
		}
		MSL_FOREACH_SURFACE_IN_GROUP_END()
		currInstance.m_nIndexCount = indexBuilder.IndexCount() - currInstance.m_nIndexOffset;
		currInstance.m_nIndexOffset += nIndexOffset;
		currInstance.m_nLightmapPageId = MATERIAL_SYSTEM_LIGHTMAP_PAGE_INVALID;
	}

	indexBuilder.Unlock();
	pRenderContext->DrawInstances( nCurrInstanceCount, pMeshInstances );
	pMeshInstances += nCurrInstanceCount;
	nInstanceCount -= nCurrInstanceCount;
	nCurrInstanceCount = 0;
	nIndexOffset = indexBuilder.Offset() / sizeof(uint16);

	
	nCount = groupsBlowingIndexBufferLimit.Count();

	for ( int g = 0; g < nCount; ++g )
	{
		const surfacesortgroup_t &group = *(groupsBlowingIndexBufferLimit[g]);

		SurfaceHandle_t nSurfID = sortList.GetSurfaceAtHead( group );
		int nSortID = MSurf_MaterialSortID( nSurfID );

		// Start a new instance for this group

		nBatchIndexCount = MIN( nIndexCount,  nMaxIndices );
		indexBuilder.Lock( nBatchIndexCount, 0 );

		MeshInstanceData_t *pCurrInstance = pMeshInstances;
		memset( pCurrInstance, 0, sizeof(MeshInstanceData_t) );
		pCurrInstance->m_nPrimType = MATERIAL_TRIANGLES;
		pCurrInstance->m_pIndexBuffer = pBuildIndexBuffer;
		pCurrInstance->m_pVertexBuffer = g_WorldStaticMeshes[ nSortID ];
		pCurrInstance->m_nIndexOffset = indexBuilder.IndexCount();
		pCurrInstance->m_DiffuseModulation.Init( 1.0f, 1.0f, 1.0f, 1.0f ); 
		pCurrInstance->m_nLightmapPageId = MATERIAL_SYSTEM_LIGHTMAP_PAGE_INVALID;

		MSL_FOREACH_SURFACE_IN_GROUP_BEGIN( sortList, group, nSurfID )
		{
			int nSurfIndexCount = GetIndexCountForWorldSurface( nSurfID );

			// Is there room enough this surface?
			if ( nBatchIndexCount < nSurfIndexCount )
			{
				// Nope, fire off the current batch...
				int nNumIndicesInIndexBuilder = indexBuilder.IndexCount();
				pCurrInstance->m_nIndexCount = nNumIndicesInIndexBuilder - pCurrInstance->m_nIndexOffset;
				indexBuilder.Unlock();
				pRenderContext->DrawInstances( 1, pMeshInstances );

				// Start a new batch
				nIndexCount       -= nNumIndicesInIndexBuilder;
				nBatchIndexCount  = MIN( nIndexCount,  nMaxIndices );
				indexBuilder.Lock( nBatchIndexCount, 0 );
				pMeshInstances++;
				nInstanceCount--;

				// Start a new instance for the remaining surfaces of this group
				pCurrInstance = pMeshInstances;
				memset( pCurrInstance, 0, sizeof(MeshInstanceData_t) );
				pCurrInstance->m_nPrimType = MATERIAL_TRIANGLES;
				pCurrInstance->m_pIndexBuffer = pBuildIndexBuffer;
				pCurrInstance->m_pVertexBuffer = g_WorldStaticMeshes[ nSortID ];
				pCurrInstance->m_nIndexOffset = indexBuilder.IndexCount();
				pCurrInstance->m_DiffuseModulation.Init( 1.0f, 1.0f, 1.0f, 1.0f ); 
				pCurrInstance->m_nLightmapPageId = MATERIAL_SYSTEM_LIGHTMAP_PAGE_INVALID;
			}

			BuildIndicesForWorldSurface( indexBuilder, nSurfID, host_state.worldbrush );
			nBatchIndexCount -= nSurfIndexCount;
		}
		MSL_FOREACH_SURFACE_IN_GROUP_END()

		// submit the instance
		int nNumIndicesInIndexBuilder = indexBuilder.IndexCount();
		pCurrInstance->m_nIndexCount = nNumIndicesInIndexBuilder - pCurrInstance->m_nIndexOffset;
		indexBuilder.Unlock();
		pRenderContext->DrawInstances( 1, pMeshInstances );

		// Start a new batch
		nIndexCount -= nNumIndicesInIndexBuilder;
		pMeshInstances++;
		nInstanceCount--;
	}
	Assert( nIndexCount == 0 );
	Assert( nInstanceCount == 0 );

	meshInstanceData.Release();
}


//-----------------------------------------------------------------------------
// Performs the z-fill
//-----------------------------------------------------------------------------
static void Shader_WorldZFill( IMatRenderContext *pRenderContext, CWorldRenderList *pRenderList, unsigned long flags )
{
	// First, count the number of vertices + indices
	int nVertexCount = 0;
	int nIndexCount = 0;

	int g;
	const CMSurfaceSortList &sortList = pRenderList->m_SortList;

#ifdef _X360
	bool bFastZRejectDisplacements = s_bFastZRejectDisplacements || ( r_fastzrejectdisp.GetInt() != 0 );
#endif

	for ( g = 0; g < MAX_MAT_SORT_GROUPS; ++g )
	{
		if ( ( flags & ( 1 << g ) ) == 0 )
			continue;

		int nSortGroup = s_DrawWorldListsToSortGroup[g];
		MSL_FOREACH_GROUP_BEGIN(sortList, nSortGroup, group )
		{
			SurfaceHandle_t surfID = sortList.GetSurfaceAtHead(group);
			IMaterial *pMaterial = MSurf_TexInfo( surfID )->material;
			if( pMaterial->IsAlphaTested() || pMaterial->IsTranslucent() )
			{
				continue;
			}
			nVertexCount += group.vertexCountNoDetail;
			nIndexCount += group.indexCountNoDetail;
		}
		MSL_FOREACH_GROUP_END()

#ifdef _X360
		// Draws opaque displacement surfaces along with shadows, overlays, flashlights, etc.
		// NOTE: This only makes sense on the 360, since the extra batches aren't
		// worth it on the PC (I think!)
		if ( bFastZRejectDisplacements )
		{
			Shader_DrawDispChain( pRenderContext, nSortGroup, pRenderList->m_DispSortList, flags, true );
		}
#endif
	}

	if ( nVertexCount == 0 )
		return;

	pRenderContext->Bind( g_pMaterialWriteZ );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( false );

	int nMaxIndices  = pRenderContext->GetMaxIndicesToRender();
	int nMaxVertices = pRenderContext->GetMaxVerticesToRender( g_pMaterialWriteZ );

	// nBatchIndexCount and nBatchVertexCount are the number of indices and vertices we can fit in this batch
	// Each batch must have fewe than nMaxIndices and nMaxVertices or the material system will fail
	int nBatchIndexCount  = MIN( nIndexCount,  nMaxIndices  );
	int nBatchVertexCount = MIN( nVertexCount, nMaxVertices );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nBatchVertexCount, nBatchIndexCount );

	int nStartVert = 0;
	for ( g = 0; g < MAX_MAT_SORT_GROUPS; ++g )
	{
		if ( ( flags & ( 1 << g ) ) == 0 )
			continue;

		int nSortGroup = s_DrawWorldListsToSortGroup[g];
		MSL_FOREACH_GROUP_BEGIN(sortList, nSortGroup, group )
		{
			SurfaceHandle_t surfID = sortList.GetSurfaceAtHead(group);

			// Check to see if we can add this list to the current batch...
			if ( ( group.triangleCount == 0 ) || ( group.vertexCount == 0 ) )
				continue;

			IMaterial *pMaterial = MSurf_TexInfo( surfID )->material;

			if( pMaterial->IsAlphaTested() || pMaterial->IsTranslucent() )
				continue;

			MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(sortList, group, nSurfID)
			{
				// Only draw surfaces on nodes (i.e. no detail surfaces)
				// Skip water surfaces since it may move up or down to fixup water transitions.
				if ( ( MSurf_Flags( nSurfID ) & SURFDRAW_NODE ) == 0 || ( MSurf_Flags( nSurfID ) & SURFDRAW_WATERSURFACE ) != 0 )
					continue;

				int nSurfaceVertexCount, nSurfaceIndexCount;
				Shader_GetSurfVertexAndIndexCount( nSurfID, &nSurfaceVertexCount, &nSurfaceIndexCount );

				if ( nSurfaceVertexCount > nMaxVertices || nSurfaceIndexCount > nMaxIndices )
				{
					// Too many vertices/indices in a batch, no simple way to split the batch
					Error( "Too many vertices (%d, max: %d) or indices (%d, max: %d) in surface.\n", nSurfaceVertexCount, nMaxVertices, nSurfaceIndexCount, nMaxIndices );
					continue;
				}

				if ( nBatchIndexCount < nSurfaceIndexCount || nBatchVertexCount < nSurfaceVertexCount )
				{
					// Surface doesn't fit, flush the current batch.
					meshBuilder.End();
					pMesh->Draw();
					nBatchIndexCount  = MIN( nIndexCount,  nMaxIndices  );
					nBatchVertexCount = MIN( nVertexCount, nMaxVertices );
					pMesh = pRenderContext->GetDynamicMesh( false );
					meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nBatchVertexCount, nBatchIndexCount );
					nStartVert = 0;
				}

				Shader_WorldZFillSurfChain_Single( nSurfID, meshBuilder, nStartVert );
				
				nBatchIndexCount -= nSurfaceIndexCount;
				nBatchVertexCount -= nSurfaceVertexCount;
				nIndexCount -= nSurfaceIndexCount;
				nVertexCount -= nSurfaceVertexCount;
			}
			MSL_FOREACH_SURFACE_IN_GROUP_END()
		}
		MSL_FOREACH_GROUP_END()
	}

	meshBuilder.End();
	pMesh->Draw();

	// FIXME: Do fast z reject on displacements!
}

extern model_t *g_pSimpleWorldModel;
extern model_t *g_pSimpleWorldModelWater;

void DrawSimpleWorldModel( unsigned long flags )
{
	Assert( ( flags & ( DRAWWORLDLISTS_DRAW_SIMPLE_WORLD_MODEL | DRAWWORLDLISTS_DRAW_SIMPLE_WORLD_MODEL_WATER ) ) != 0 );
	if ( ( ( flags & ( DRAWWORLDLISTS_DRAW_SIMPLE_WORLD_MODEL | DRAWWORLDLISTS_DRAW_SIMPLE_WORLD_MODEL_WATER ) ) == 0 ) )
	{
		return;
	}

	// early out if the models that we are trying to draw don't exist
	if ( !( ( ( flags & DRAWWORLDLISTS_DRAW_SIMPLE_WORLD_MODEL ) && g_pSimpleWorldModel ) || 
  		  ( ( flags & DRAWWORLDLISTS_DRAW_SIMPLE_WORLD_MODEL_WATER ) && g_pSimpleWorldModelWater ) ) )
	{
		return;
	}


	DrawModelInfo_t info;

	info.m_Decals = STUDIORENDER_DECAL_INVALID;
	info.m_Skin = 0;
	info.m_Body = 0;
	info.m_HitboxSet = 0;
	info.m_pClientEntity = NULL;
	info.m_Lod = 0;
	info.m_pColorMeshes = NULL;
	info.m_bStaticLighting = false;
	info.m_LightingState.m_nLocalLightCount = 0;
	info.m_LightingState.m_vecAmbientCube[0].Init( 1.0f, 1.0f, 1.0f );
	info.m_LightingState.m_vecAmbientCube[1].Init( 1.0f, 1.0f, 1.0f );
	info.m_LightingState.m_vecAmbientCube[2].Init( 1.0f, 1.0f, 1.0f );
	info.m_LightingState.m_vecAmbientCube[3].Init( 1.0f, 1.0f, 1.0f );
	info.m_LightingState.m_vecAmbientCube[4].Init( 1.0f, 1.0f, 1.0f );
	info.m_LightingState.m_vecAmbientCube[5].Init( 1.0f, 1.0f, 1.0f );

	matrix3x4_t modelToWorld;
	modelToWorld.Init( Vector( 0.0f, -1.0f, 0.0f ), Vector( 1.0f, 0.0f, 0.0f ), Vector( 0.0f, 0.0f, 1.0f ), Vector( 0.0f, 0.0f, 0.0f ) );
	CMatRenderContextPtr pRenderContext( materials );



#if defined( CSTRIKE15 )
	if( !r_skybox_draw_last.GetBool() )
	{
		// Draw the skybox
		if( flags & DRAWWORLDLISTS_DRAW_SKYBOX )
		{
			// [mariod] - leaving this check off as it breaks skybox rendering into reflection texture on some levels, TODO - map fixup?
			// if( Map_VisForceFullSky() )
			{
				if( flags & DRAWWORLDLISTS_DRAW_CLIPSKYBOX )
				{
					R_DrawSkyBox( g_EngineRenderer->GetZFar() );
				}
				else
				{
					// Don't clip the skybox with height clip in this path.
					MaterialHeightClipMode_t nClipMode = pRenderContext->GetHeightClipMode();
					pRenderContext->SetHeightClipMode( MATERIAL_HEIGHTCLIPMODE_DISABLE );
					R_DrawSkyBox( g_EngineRenderer->GetZFar() );
					pRenderContext->SetHeightClipMode( nClipMode );
				}
			}
		}
	}
#endif

	// Have to save and restore these matrices since DrawModelStaticProp seems to mod them.
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();

	g_pShadowMgr->PushSinglePassFlashlightStateEnabled( true );

	if ( ( flags & DRAWWORLDLISTS_DRAW_SIMPLE_WORLD_MODEL ) && g_pSimpleWorldModel && !g_pMDLCache->IsErrorModel( g_pSimpleWorldModel->studio ) )
	{
		info.m_pStudioHdr = g_pMDLCache->GetStudioHdr( g_pSimpleWorldModel->studio );
		info.m_pHardwareData = g_pMDLCache->GetHardwareData( g_pSimpleWorldModel->studio );
		g_pStudioRender->DrawModelStaticProp( info, modelToWorld );
	}
	if ( ( flags & DRAWWORLDLISTS_DRAW_SIMPLE_WORLD_MODEL_WATER ) && g_pSimpleWorldModelWater && !g_pMDLCache->IsErrorModel( g_pSimpleWorldModelWater->studio ) )
	{
		info.m_pStudioHdr = g_pMDLCache->GetStudioHdr( g_pSimpleWorldModelWater->studio );
		info.m_pHardwareData = g_pMDLCache->GetHardwareData( g_pSimpleWorldModelWater->studio );
		g_pStudioRender->DrawModelStaticProp( info, modelToWorld );
	}
	g_pShadowMgr->PopSinglePassFlashlightStateEnabled();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	OverlayMgr()->RenderAllUnlitOverlays( pRenderContext, MAT_SORT_GROUP_STRICTLY_ABOVEWATER );


#if defined( CSTRIKE15 )
	if( r_skybox_draw_last.GetBool() )
	{
		// Draw the skybox
		if( flags & DRAWWORLDLISTS_DRAW_SKYBOX )
		{
			// [mariod] - leaving this check off as it breaks skybox rendering into reflection texture on some levels, TODO - map fixup?
			// if( Map_VisForceFullSky() )
			{
				if( flags & DRAWWORLDLISTS_DRAW_CLIPSKYBOX )
				{
					R_DrawSkyBox( g_EngineRenderer->GetZFar() );
				}
				else
				{
					// Don't clip the skybox with height clip in this path.
					MaterialHeightClipMode_t nClipMode = pRenderContext->GetHeightClipMode();
					pRenderContext->SetHeightClipMode( MATERIAL_HEIGHTCLIPMODE_DISABLE );
					R_DrawSkyBox( g_EngineRenderer->GetZFar() );
					pRenderContext->SetHeightClipMode( nClipMode );
				}
			}
		}
	}
#endif

}

//-----------------------------------------------------------------------------
// Call this after lists of stuff to render are made; it renders opaque surfaces
//-----------------------------------------------------------------------------
static void Shader_WorldEnd( IMatRenderContext *pRenderContext, CWorldRenderList *pRenderList, unsigned long flags, float waterZAdjust )
{
	VPROF("Shader_WorldEnd");

	if ( flags & ( DRAWWORLDLISTS_DRAW_SHADOWDEPTH | DRAWWORLDLISTS_DRAW_SSAO ) )
	{
		// NOTE: Implementations appear to want to be different on the PC + 360 here
		if ( IsX360() )
		{
			Shader_WorldShadowDepthFillX360( pRenderContext, pRenderList, flags );
		}
		else if ( r_csm_fast_path.GetBool() )
		{
			Shader_WorldShadowDepthFillFast( pRenderContext, pRenderList, flags );
		}
		else
		{
			Shader_WorldShadowDepthFill( pRenderContext, pRenderList, flags );
		}
		return;
	}

	if ( !r_skybox_draw_last.GetBool() )
	{
		// Draw the skybox
		if ( flags & DRAWWORLDLISTS_DRAW_SKYBOX )
		{
			if ( pRenderList->m_bSkyVisible || Map_VisForceFullSky() )
			{
				if( flags & DRAWWORLDLISTS_DRAW_CLIPSKYBOX )
				{
					R_DrawSkyBox( g_EngineRenderer->GetZFar() );
				}
				else
				{
					// Don't clip the skybox with height clip in this path.
					MaterialHeightClipMode_t nClipMode = pRenderContext->GetHeightClipMode();
					pRenderContext->SetHeightClipMode( MATERIAL_HEIGHTCLIPMODE_DISABLE );
					R_DrawSkyBox( g_EngineRenderer->GetZFar() );
					pRenderContext->SetHeightClipMode( nClipMode );
				}
			}
		}
	}

	if ( !IsGameConsole() )
	{
		// X360 and PS3 now use a different fast z-reject pass (PS3 emulates X360 behavior)
		// Perform the fast z-fill pass
		bool bFastZReject = (r_fastzreject.GetInt() != 0);
		if ( bFastZReject )
		{
			Shader_WorldZFill( pRenderContext, pRenderList, flags );
		}
	}

	// Gotta draw each sort group
	// Draw the fog volume first, if there is one, because it turns out
	// that we only draw fog volumes if we're in the fog volume, which
	// means it's closer. We want to render closer things first to get
	// fast z-reject.
	int i;
	for ( i = MAX_MAT_SORT_GROUPS; --i >= 0; )
	{
		if ( !( flags & ( 1 << i ) ) )
			continue;

		PIXEVENT( pRenderContext, s_pMatSortGroupsString[ i ] );

		int nSortGroup = s_DrawWorldListsToSortGroup[i];
		if ( nSortGroup == MAT_SORT_GROUP_WATERSURFACE  )
		{
			if ( waterZAdjust != 0.0f )
			{
				pRenderContext->MatrixMode( MATERIAL_MODEL );
				pRenderContext->PushMatrix();
				pRenderContext->LoadIdentity();
				pRenderContext->Translate( 0.0f, 0.0f, waterZAdjust );
			}
			g_pShadowMgr->PushSinglePassFlashlightStateEnabled( true );
		}


		// Don't stencil or scissor the flashlight if we're rendering to an offscreen view
		bool bFlashlightMask = !( (flags & DRAWWORLDLISTS_DRAW_REFRACTION ) || (flags & DRAWWORLDLISTS_DRAW_REFLECTION ));

		// Set masking stencil bits for flashlights
		g_pShadowMgr->SetFlashlightStencilMasks( bFlashlightMask );

		// Draws opaque displacement surfaces along with shadows, overlays, flashlights, etc.
		Shader_DrawDispChain( pRenderContext, nSortGroup, pRenderList->m_DispSortList, flags, DEPTH_MODE_NORMAL );

		// Draws opaque non-displacement surfaces
		// This also add shadows to pRenderList->m_ShadowHandles.
		Shader_DrawChains( pRenderContext, pRenderList, nSortGroup, DEPTH_MODE_NORMAL );

		if ( nSortGroup == MAT_SORT_GROUP_WATERSURFACE  )
		{
			g_pShadowMgr->PopSinglePassFlashlightStateEnabled();
			if ( waterZAdjust != 0.0f )
			{
				pRenderContext->MatrixMode( MATERIAL_MODEL );
				pRenderContext->PopMatrix();
			}
		}
	}

	if ( r_skybox_draw_last.GetBool() )
	{
		// Draw the skybox
		if ( flags & DRAWWORLDLISTS_DRAW_SKYBOX )
		{
			if ( pRenderList->m_bSkyVisible || Map_VisForceFullSky() )
			{
				if( flags & DRAWWORLDLISTS_DRAW_CLIPSKYBOX )
				{
					R_DrawSkyBox( g_EngineRenderer->GetZFar() );
				}
				else
				{
					// Don't clip the skybox with height clip in this path.
					MaterialHeightClipMode_t nClipMode = pRenderContext->GetHeightClipMode();
					pRenderContext->SetHeightClipMode( MATERIAL_HEIGHTCLIPMODE_DISABLE );
					R_DrawSkyBox( g_EngineRenderer->GetZFar() );
					pRenderContext->SetHeightClipMode( nClipMode );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Call this after lists of stuff to render are made; it renders opaque surfaces
//-----------------------------------------------------------------------------
static void Shader_DrawWorldDecalsAndOverlays( IMatRenderContext *pRenderContext, CWorldRenderList *pRenderList, unsigned long flags, float waterZAdjust )
{
	// Gotta draw each sort group
	// Draw the fog volume first, if there is one, because it turns out
	// that we only draw fog volumes if we're in the fog volume, which
	// means it's closer. We want to render closer things first to get
	// fast z-reject.
	int i;
	for ( i = MAX_MAT_SORT_GROUPS; --i >= 0; )
	{
		if ( !( flags & ( 1 << i ) ) )
			continue;

		PIXEVENT( pRenderContext, s_pMatSortGroupsString[ i ] );

		int nSortGroup = s_DrawWorldListsToSortGroup[i];
		if ( nSortGroup == MAT_SORT_GROUP_WATERSURFACE  )
		{
			if ( waterZAdjust != 0.0f )
			{
				pRenderContext->MatrixMode( MATERIAL_MODEL );
				pRenderContext->PushMatrix();
				pRenderContext->LoadIdentity();
				pRenderContext->Translate( 0.0f, 0.0f, waterZAdjust );
			}
		}

		// FIXME: Implement this
		// Draws opaque displacement surfaces along with shadows, overlays, flashlights, etc.
		Shader_DrawDispChainDecalsAndOverlays( pRenderContext, nSortGroup, pRenderList->m_DispSortList, flags );

		AddProjectedTextureDecalsToList( pRenderList, nSortGroup );

		// Adds shadows to render lists
		for ( int j = pRenderList->m_ShadowHandles[nSortGroup].Count()-1; j >= 0; --j )
		{
			g_pShadowMgr->AddShadowsOnSurfaceToRenderList( pRenderList->m_ShadowHandles[nSortGroup].Element(j) );
		}
		pRenderList->m_ShadowHandles[nSortGroup].RemoveAll();

		// Don't stencil or scissor the flashlight if we're rendering to an offscreen view
		bool bFlashlightMask = !( (flags & DRAWWORLDLISTS_DRAW_REFRACTION ) || (flags & DRAWWORLDLISTS_DRAW_REFLECTION ));

		// Set masking stencil bits for flashlights
		g_pShadowMgr->SetFlashlightStencilMasks( bFlashlightMask );

		// Draw shadows and flashlights on world surfaces
		g_pShadowMgr->RenderFlashlights( bFlashlightMask, false );

		// Render the fragments from the surfaces + displacements.
		// FIXME: Actually, this call is irrelevant (for displacements) because it's done from
		// within DrawDispChain currently, but that should change.
		// We need to split out the disp decal rendering from DrawDispChain
		// and do it after overlays are rendered....
		OverlayMgr()->RenderOverlays( pRenderContext, nSortGroup );
		g_pShadowMgr->DrawFlashlightOverlays( pRenderContext, nSortGroup, bFlashlightMask );
		OverlayMgr()->ClearRenderLists( nSortGroup );

		// Draws decals lying on opaque non-displacement surfaces
		DecalSurfaceDraw( pRenderContext, nSortGroup );

		// Draw the flashlight lighting for the decals.
		g_pShadowMgr->DrawFlashlightDecals( pRenderContext, nSortGroup, bFlashlightMask );
		g_pShadowMgr->RenderFlashlights( bFlashlightMask, true );

		// Retire decals on opaque world surfaces
		R_DecalFlushDestroyList();

		// Draw RTT shadows
		g_pShadowMgr->RenderShadows( pRenderContext );
		g_pShadowMgr->ClearShadowRenderList();

		if ( nSortGroup == MAT_SORT_GROUP_WATERSURFACE && waterZAdjust != 0.0f )
		{
			pRenderContext->MatrixMode( MATERIAL_MODEL );
			pRenderContext->PopMatrix();
		}
	}
}


//-----------------------------------------------------------------------------
// Renders translucent surfaces
//-----------------------------------------------------------------------------
bool Shader_LeafContainsTranslucentSurfaces( IWorldRenderList *pRenderListIn, int sortIndex, unsigned long flags )
{
	CWorldRenderList *pRenderList = assert_cast<CWorldRenderList *>(pRenderListIn);

	if ( pRenderList->m_leaves[sortIndex].translucentSurfaceCount > 0 )
	{
		return true;
	}
	return false;
}


struct transsurfacebatch_t
{
	int		firstSurface;
	int		surfaceCount;
	IMaterial *pMaterial;
	int		sortID;
	int		triangleCount;

	void AddSurface( SurfaceHandle_t surfID )
	{
		surfaceCount++;
		triangleCount += (MSurf_VertCount( surfID )-2);
	}
};

void Shader_DrawTranslucentSurfaces( IMatRenderContext *pRenderContext, IWorldRenderList *pRenderListIn, int *pSortList, int sortCount, unsigned long flags )
{
	if ( !r_drawtranslucentworld.GetBool() )
		return;

	CWorldRenderList *pRenderList = assert_cast<CWorldRenderList *>(pRenderListIn);

	bool skipLight = false;
	if ( g_pMaterialSystemConfig->nFullbright == 1 )
	{
		pRenderContext->BindLightmapPage( MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP );
		skipLight = true;
	}


	CUtlVectorFixedGrowable<msurface2_t *, 16> surfaceList;
	CUtlVectorFixedGrowable<msurface2_t *, 16> decalSurfaceList;
	CUtlVectorFixedGrowable<msurface2_t *, 16> flashlightSurfaceList;
	CUtlVectorFixedGrowable<msurface2_t *, 16> dispList;
	CUtlVectorFixedGrowable<transsurfacebatch_t,16> batches;
	transsurfacebatch_t *pLastBatch = NULL;
	bool bFlashlightMask = !( (flags & DRAWWORLDLISTS_DRAW_REFRACTION ) || (flags & DRAWWORLDLISTS_DRAW_REFLECTION ));
	bool bHasDisp = false;
	for ( int i = 0, mask = 1; i < MAX_MAT_SORT_GROUPS; i++, mask<<=1 )
	{
		if ( !(flags & mask) )
			continue;

		for ( int leaf = 0; leaf < sortCount; leaf++ )
		{
			int sortIndex = pSortList[leaf];
			int surfaceIndexStart = pRenderList->m_leaves[sortIndex].firstTranslucentSurface;
			int nextTranslucentSurface = surfaceIndexStart + pRenderList->m_leaves[sortIndex].translucentSurfaceCount;
			for ( int si = nextTranslucentSurface-1; si >= surfaceIndexStart; --si )
			{
				int sortGroup = MSurf_SortGroup(pRenderList->m_AlphaSurfaces[si]);
				if ( sortGroup != i )
					continue;
				SurfaceHandle_t surfID = pRenderList->m_AlphaSurfaces[si];
				if ( surfID->pDispInfo )
				{
					bHasDisp = true;
				}
				else
				{
					int sortID = MSurf_MaterialSortID(surfID);
					if ( !pLastBatch || sortID != pLastBatch->sortID )
					{
						int batchIndex = batches.AddToTail();
						pLastBatch = &batches[batchIndex];
						pLastBatch->firstSurface = surfaceList.Count();
						pLastBatch->surfaceCount = 0;
						pLastBatch->pMaterial = MSurf_TexInfo( surfID )->material;
						pLastBatch->sortID = sortID;
						pLastBatch->triangleCount = 0;
					}
					pLastBatch->AddSurface( surfID );
					surfaceList.AddToTail(surfID);
					if ( MSurf_ShadowDecals(surfID) != SHADOW_DECAL_HANDLE_INVALID )
					{
						flashlightSurfaceList.AddToTail(surfID);
					}
					if ( SurfaceHasDecals(surfID) || MSurf_OverlayFragmentList(surfID) != OVERLAY_FRAGMENT_INVALID )
					{
						decalSurfaceList.AddToTail(surfID);
					}
				}
			}
		}
	}

	for ( int i = 0; i < batches.Count(); i++ )
	{
		transsurfacebatch_t *pBatch = &batches[i];
		SurfaceHandle_t surfID = surfaceList[pBatch->firstSurface];
		pRenderContext->Bind( MSurf_TexInfo( surfID )->material );
		Assert( MSurf_MaterialSortID( surfID ) >= 0 && MSurf_MaterialSortID( surfID ) < g_WorldStaticMeshes.Count() );

		if ( !skipLight )
		{
			int nLightmapPageId = materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID;
			pRenderContext->BindLightmapPage( nLightmapPageId );
		}

		Shader_DrawSurfaceListStatic( pRenderContext, &surfaceList[pBatch->firstSurface], pBatch->surfaceCount, pBatch->triangleCount );
	}

	for ( int i = 0; i < decalSurfaceList.Count(); i++ )
	{
		SurfaceHandle_t surfID = decalSurfaceList[i];
		int sortGroup = MSurf_SortGroup(surfID);

		if ( MSurf_OverlayFragmentList(surfID) != OVERLAY_FRAGMENT_INVALID )
		{
			// Draw overlays on the surface.
			OverlayMgr()->AddFragmentListToRenderList( sortGroup, MSurf_OverlayFragmentList( surfID ), false );
			OverlayMgr()->RenderOverlays( pRenderContext, sortGroup );

			// Draw flashlight overlays
			g_pShadowMgr->DrawFlashlightOverlays( pRenderContext, sortGroup, bFlashlightMask );
			OverlayMgr()->ClearRenderLists( sortGroup );
		}

		// Draw decals on the surface
		if ( SurfaceHasDecals(surfID) )
		{
			DrawDecalsOnSingleSurface( pRenderContext, surfID );
		}
	}
	if ( flashlightSurfaceList.Count() )
	{
		for ( int i = 0; i < flashlightSurfaceList.Count(); i++ )
		{
			SurfaceHandle_t surfID = flashlightSurfaceList[i];
			ShadowDecalHandle_t decalHandle = MSurf_ShadowDecals( surfID );
			if (decalHandle != SHADOW_DECAL_HANDLE_INVALID)
			{
				g_pShadowMgr->AddShadowsOnSurfaceToRenderList( decalHandle );
			}
		}
		g_pShadowMgr->RenderFlashlights( bFlashlightMask, false );
		if ( decalSurfaceList.Count() )
		{
			g_pShadowMgr->DrawFlashlightDecalsOnSurfaceList( pRenderContext, flashlightSurfaceList.Base(), flashlightSurfaceList.Count(), bFlashlightMask );
		}

		// draw shadows
		g_pShadowMgr->RenderShadows( pRenderContext );
		g_pShadowMgr->ClearShadowRenderList();
	}

	// Draw wireframe, etc information
	DrawDebugInformation( pRenderContext, surfaceList.Base(), surfaceList.Count() );
	if ( bHasDisp )
	{
		for ( int i = 0, mask = 1; i < MAX_MAT_SORT_GROUPS; i++, mask<<=1 )
		{
			if ( !(flags & mask) )
				continue;

			for ( int leaf = 0; leaf < sortCount; leaf++ )
			{
				int sortIndex = pSortList[leaf];
				int surfaceIndexStart = pRenderList->m_leaves[sortIndex].firstTranslucentSurface;
				int nextTranslucentSurface = surfaceIndexStart + pRenderList->m_leaves[sortIndex].translucentSurfaceCount;
				dispList.RemoveAll();
				for ( int si = nextTranslucentSurface-1; si >= surfaceIndexStart; --si )
				{
					int sortGroup = MSurf_SortGroup(pRenderList->m_AlphaSurfaces[si]);
					if ( sortGroup != i )
						continue;
					SurfaceHandle_t surfID = pRenderList->m_AlphaSurfaces[si];
					if ( surfID->pDispInfo )
					{
						dispList.AddToTail(surfID);
					}
				}

				if ( dispList.Count() )
				{
					// Now draw the translucent displacements; we need to do these *after* the
					// non-displacement surfaces because most likely the displacement will always
					// be in front (or at least not behind) the non-displacement translucent surfaces
					// that exist in the same leaf.
					
					// Draws translucent displacement surfaces
					DispInfo_RenderListWorld( pRenderContext, i, dispList.Base(), dispList.Count(), g_EngineRenderer->ViewGetCurrent().m_bOrtho, flags, DEPTH_MODE_NORMAL );
					DispInfo_RenderListDecalsAndOverlays( pRenderContext, i, dispList.Base(), dispList.Count(), g_EngineRenderer->ViewGetCurrent().m_bOrtho, flags );
				}
			}
		}
	}
}

//=============================================================
//
//	WORLD MODEL
//
//=============================================================

#if !defined(_PS3)
static void FASTCALL R_DrawSurface( CWorldRenderList *pRenderList, SurfaceHandle_t surfID )
{
	ASSERT_SURF_VALID( surfID );
	Assert( !SurfaceHasDispInfo( surfID ) );
	if ( MSurf_Flags( surfID ) & SURFDRAW_SKY )
	{
		pRenderList->m_bSkyVisible = true;
	}
	else if( MSurf_Flags( surfID ) & SURFDRAW_TRANS )
	{
		Shader_TranslucentWorldSurface( pRenderList, surfID );
	}
	else
	{
		Shader_WorldSurface( pRenderList, surfID );
	}
}
#endif

//-----------------------------------------------------------------------------
// Draws displacements in a leaf
//-----------------------------------------------------------------------------
#if !defined(_PS3)
static inline void DrawDisplacementsInLeaf( CWorldRenderList *pRenderList, mleaf_t* pLeaf )
{
	// add displacement surfaces
	if (!pLeaf->dispCount)
		return;

	CVisitedSurfs &visitedSurfs = pRenderList->m_VisitedSurfs;
	for ( int i = 0; i < pLeaf->dispCount; i++ )
	{
		CDispInfo *pDispInfo = static_cast<CDispInfo *>(MLeaf_Disaplcement( pLeaf, i ));

		// NOTE: We're not using the displacement's touched method here 
		// because we're just using the parent surface's visframe in the
		// surface add methods below...
		SurfaceHandle_t parentSurfID = pDispInfo->m_ParentSurfID;

		// already processed this frame? Then don't do it again!
		if ( visitedSurfs.VisitSurface( parentSurfID ) )
		{
			if ( g_Frustum.CullBox( pDispInfo->m_BBoxMin, pDispInfo->m_BBoxMax ) )
				continue;
			if ( MSurf_Flags( parentSurfID ) & SURFDRAW_TRANS)
			{
				Shader_TranslucentDisplacementSurface( pRenderList, parentSurfID );
			}
			else
			{
				Shader_DisplacementSurface( pRenderList, parentSurfID );
			}
		}
	}
}
#else

static uint32 s_Disp_ParentSurfID_offset;
static uint32 s_Disp_BB_offset;

inline void MLeaf_Displacement_BBs( mleaf_t *pLeaf, int index, Vector*	pBBoxMin, Vector*	pBBoxMax, SurfaceHandle_t *pParentSurfID)
{
	int dispIndex = host_state.worldbrush->m_pDispInfoReferences[pLeaf->dispListStart+index];

	CDispArray *pArray = static_cast<CDispArray*>( host_state.worldbrush->hDispInfos );

	uint8* pInfo = (uint8*)(pArray->m_pDispInfos + dispIndex );

	*pParentSurfID = *((SurfaceHandle_t*)(pInfo + s_Disp_ParentSurfID_offset));

	Vector* pSrcVector = (Vector*)(pInfo + s_Disp_BB_offset);

	*pBBoxMin = pSrcVector[0];
	*pBBoxMax = pSrcVector[1];
}

#define GET_OFFSET(type, field)    ((uint32)&(((type *)0)->field))

static inline void DrawDisplacementsInLeaf( CWorldRenderList *pRenderList, mleaf_t* pLeaf )
{
	// add displacement surfaces
	if (!pLeaf->dispCount)
		return;

	s_Disp_ParentSurfID_offset = GET_OFFSET(CDispInfo, m_ParentSurfID);
	s_Disp_BB_offset		   = GET_OFFSET(CDispInfo, m_BBoxMin);

	CVisitedSurfs &visitedSurfs = pRenderList->m_VisitedSurfs;
	for ( int i = 0; i < pLeaf->dispCount; i++ )
	{
		// 		CDispInfo *pDispInfo = static_cast<CDispInfo *>(MLeaf_Disaplcement( pLeaf, i ));
		// 
		SurfaceHandle_t parentSurfID;
		Vector			bbMin;
		Vector			bbMax;

		MLeaf_Displacement_BBs(pLeaf, i, &bbMin, &bbMax, &parentSurfID);

		// 		if (bbMin != pDispInfo->m_BBoxMin) DebuggerBreak();
		// 		if (bbMax != pDispInfo->m_BBoxMax) DebuggerBreak();
		// 		if (parentSurfID != pDispInfo->m_ParentSurfID) DebuggerBreak();


		// NOTE: We're not using the displacement's touched method here 
		// because we're just using the parent surface's visframe in the
		// surface add methods below...

		// already processed this frame? Then don't do it again!
		if ( visitedSurfs.VisitSurface( parentSurfID ) )
		{
			if ( g_Frustum.CullBox( bbMin, bbMax ) )
				continue;
			if ( MSurf_Flags( parentSurfID ) & SURFDRAW_TRANS)
			{
				Shader_TranslucentDisplacementSurface( pRenderList, parentSurfID );
			}
			else
			{
				Shader_DisplacementSurface( pRenderList, parentSurfID );
			}
		}
	}
}


#endif


int LeafToIndex( mleaf_t* pLeaf );

//-----------------------------------------------------------------------------
// Updates visibility + alpha lists
//-----------------------------------------------------------------------------
static inline void UpdateVisibleLeafLists( CWorldRenderList *pRenderList, mleaf_t* pLeaf )
{
	// Consistency check...
	MEM_ALLOC_CREDIT();
	
	// Add this leaf to the list of visible leafs
	int nLeafIndex = LeafToIndex( pLeaf );
	WorldListLeafData_t * RESTRICT pData = &pRenderList->m_leaves[pRenderList->m_leaves.AddToTail()];
	pData->leafIndex = nLeafIndex;
	pData->waterData = pLeaf->leafWaterDataID;
	pData->firstTranslucentSurface = pRenderList->m_AlphaSurfaces.Count();
	pData->translucentSurfaceCount = 0;
	if ( pLeaf->leafWaterDataID != -1 )
	{
		pRenderList->m_bWaterVisible = true;
	}
}

static ConVar r_frustumcullworld( "r_frustumcullworld", "1" );

//-----------------------------------------------------------------------------
// Set up fog for a particular leaf
//-----------------------------------------------------------------------------
#define INVALID_WATER_HEIGHT 1000000.0f
static inline float R_GetWaterHeight( int nFogVolume )
{
	if( nFogVolume < 0 || nFogVolume > host_state.worldbrush->numleafwaterdata )
		return INVALID_WATER_HEIGHT;

	mleafwaterdata_t* pLeafWaterData = &host_state.worldbrush->leafwaterdata[nFogVolume];
	return pLeafWaterData->surfaceZ;
}

IMaterial *R_GetFogVolumeMaterial( int nFogVolume, bool bEyeInFogVolume )
{
	if( nFogVolume < 0 || nFogVolume > host_state.worldbrush->numleafwaterdata )
		return NULL;

	mleafwaterdata_t* pLeafWaterData = &host_state.worldbrush->leafwaterdata[nFogVolume];
	mtexinfo_t* pTexInfo = &host_state.worldbrush->texinfo[pLeafWaterData->surfaceTexInfoID];

	IMaterial* pMaterial = pTexInfo->material;
	if( bEyeInFogVolume )
	{
		IMaterialVar *pVar = pMaterial->FindVar( "$bottommaterial", NULL );
		if( pVar )
		{
			const char *pMaterialName = pVar->GetStringValue();
			if( pMaterialName )
			{
				pMaterial = materials->FindMaterial( pMaterialName, TEXTURE_GROUP_OTHER );
			}
		}
	}
	return pMaterial;
}

void R_SetFogVolumeState( int fogVolume, bool useHeightFog )
{
	// useHeightFog == eye out of water
	// !useHeightFog == eye in water
	IMaterial *pMaterial = R_GetFogVolumeMaterial( fogVolume, !useHeightFog );
	mleafwaterdata_t* pLeafWaterData = &host_state.worldbrush->leafwaterdata[fogVolume];
	IMaterialVar* pFogColorVar	= pMaterial->FindVar( "$fogcolor", NULL );
	IMaterialVar* pFogEnableVar = pMaterial->FindVar( "$fogenable", NULL );
	IMaterialVar* pFogStartVar	= pMaterial->FindVar( "$fogstart", NULL );
	IMaterialVar* pFogEndVar	= pMaterial->FindVar( "$fogend", NULL );

	CMatRenderContextPtr pRenderContext( materials );

	if( pMaterial && pFogEnableVar->GetIntValueFast() && fog_enable_water_fog.GetBool() )
	{
		pRenderContext->SetFogZ( pLeafWaterData->surfaceZ );
		if( useHeightFog )
		{
			pRenderContext->FogMode( MATERIAL_FOG_LINEAR_BELOW_FOG_Z );
		}
		else
		{
			pRenderContext->FogMode( MATERIAL_FOG_LINEAR );
		}
		float fogColor[3];
		pFogColorVar->GetVecValueFast( fogColor, 3 );

		pRenderContext->FogColor3fv( fogColor );
		pRenderContext->FogStart( pFogStartVar->GetFloatValueFast() );
		pRenderContext->FogEnd( pFogEndVar->GetFloatValueFast() );
		pRenderContext->FogMaxDensity( 1.0 );
	}
	else
	{
		pRenderContext->FogMode( MATERIAL_FOG_NONE );
	}
}



//-----------------------------------------------------------------------------
// Job for building the world rendering list
//-----------------------------------------------------------------------------

class CBuildWorldListsJob : public CJob
{
public:

	CBuildWorldListsJob( 
		CWorldRenderList *pRenderList,
		WorldListInfo_t* pInfo,
		bool bShadowDepth,
		const Vector& currentViewOrigin,
		int visFrameCount,
		bool bDrawTopView,
		bool bTopViewNoBackfaceCulling,
		bool bTopViewNoVisCheck,
		const Vector2D& orthographicCenter,
		const Vector2D& orthographicHalfDiagonal,
		const CVolumeCuller* pTopViewVolumeCuller,
		const Frustum_t* pFrustum,
		const CUtlVector< Frustum_t, CUtlMemoryAligned< Frustum_t,16 > >* pAeraFrustum,
		unsigned char* pRenderAreaBits,
		bool bViewerInSolidSpace,
		const Vector& modelOrigin );

private:

	virtual JobStatus_t	DoExecute();

	void				R_RecursiveWorldNode( CWorldRenderList *pRenderList, mnode_t *node );
	// Fast path for rendering top-views
	void				R_RenderWorldTopView( CWorldRenderList *pRenderList, mnode_t *node );
	void				R_BuildWorldListNoCull( CWorldRenderList *pRenderList, mnode_t *node );

	inline bool			R_CullNode( mnode_t *pNode );
	inline bool			R_CullNodeTopView( mnode_t *pNode );

	void				R_DrawLeaf( CWorldRenderList *pRenderList, mleaf_t *pleaf );
	void				R_DrawTopViewLeaf( CWorldRenderList *pRenderList, mleaf_t *pleaf );
	void				R_DrawLeafNoCull( CWorldRenderList *pRenderList, mleaf_t *pleaf );
	void				R_DrawSurfaceNoCull( CWorldRenderList *pRenderList, SurfaceHandle_t surfID );

	void				DrawDisplacementsInLeaf( CWorldRenderList *pRenderList, mleaf_t* pLeaf );
	void				Shader_WorldSurface( CWorldRenderList *pRenderList, SurfaceHandle_t surfID );
	void				Shader_WorldSurfaceNoCull( CWorldRenderList *pRenderList, SurfaceHandle_t surfID );

	CWorldRenderList*		m_pRenderList;
	WorldListInfo_t*		m_pWorldListInfo;
	bool					m_bShadowDepth;

	Vector					m_currentViewOrigin;
	int						m_visFrameCount;
	bool					m_bDrawTopView;

	bool					m_bTopViewNoBackfaceCulling;
	bool					m_bTopViewNoVisCheck;
	Vector2D				m_OrthographicCenter;
	Vector2D				m_OrthographicHalfDiagonal;
	const CVolumeCuller*	m_pTopViewVolumeCuller;		// No need to copy the data as it is
														// already cached in CConcurrentViewBuilder volume culler cache

	const Frustum_t*		m_pFrustum;
	const CUtlVector< Frustum_t, CUtlMemoryAligned< Frustum_t,16 > >* m_pAreaFrustum;
	unsigned char			m_RenderAreaBits[32];
	bool					m_bViewerInSolidSpace;
	Vector					m_modelOrigin;
};

CBuildWorldListsJob::CBuildWorldListsJob( 
	CWorldRenderList *pRenderList,
	WorldListInfo_t* pInfo,
	bool bShadowDepth,
	const Vector& currentViewOrigin,
	int visFrameCount,
	bool bDrawTopView,
	bool bTopViewNoBackfaceCulling,
	bool bTopViewNoVisCheck,
	const Vector2D& orthographicCenter,
	const Vector2D& orthographicHalfDiagonal,
	const CVolumeCuller* pTopViewVolumeCuller,
	const Frustum_t* pFrustum,
	const CUtlVector< Frustum_t, CUtlMemoryAligned< Frustum_t,16 > >* pAeraFrustum,
	unsigned char* pRenderAreaBits,
	bool bViewerInSolidSpace,
	const Vector& modelOrigin )
:
	m_pRenderList( pRenderList ),
	m_pWorldListInfo( pInfo ),
	m_bShadowDepth( bShadowDepth ),
	m_currentViewOrigin( currentViewOrigin ),
	m_visFrameCount(visFrameCount),
	m_bDrawTopView( bDrawTopView ),
	m_bTopViewNoBackfaceCulling( bTopViewNoBackfaceCulling ),
	m_bTopViewNoVisCheck( bTopViewNoVisCheck ),
	m_OrthographicCenter( orthographicCenter ),
	m_OrthographicHalfDiagonal( orthographicHalfDiagonal ),
	m_pTopViewVolumeCuller( pTopViewVolumeCuller ),
	m_pFrustum( pFrustum ),
	m_bViewerInSolidSpace( bViewerInSolidSpace ),
	m_modelOrigin( modelOrigin )
{
	m_pAreaFrustum = pAeraFrustum;
	memcpy( m_RenderAreaBits, pRenderAreaBits, sizeof(m_RenderAreaBits) );
}

JobStatus_t CBuildWorldListsJob::DoExecute()
{
	if ( !m_bDrawTopView )
	{
		if ( m_bShadowDepth )
		{
			R_BuildWorldListNoCull( m_pRenderList, host_state.worldbrush->nodes );
		}
		else
		{
			R_RecursiveWorldNode( m_pRenderList, host_state.worldbrush->nodes );
		}
	}
	else
	{
		R_RenderWorldTopView( m_pRenderList, host_state.worldbrush->nodes );
	}
	

	m_pRenderList->CountTranslucentSurfaces();

	// Return the back-to-front leaf ordering
	if ( m_pWorldListInfo )
	{
		// Compute fog volume info for rendering
		if ( !m_bShadowDepth )
		{
			FogVolumeInfo_t fogInfo;
			ComputeFogVolumeInfo( &fogInfo, m_currentViewOrigin );
			if( fogInfo.m_InFogVolume )
			{
				m_pWorldListInfo->m_ViewFogVolume = MAT_SORT_GROUP_STRICTLY_UNDERWATER;
			}
			else
			{
				m_pWorldListInfo->m_ViewFogVolume = MAT_SORT_GROUP_STRICTLY_ABOVEWATER;
			}
		}
		else
		{
			m_pWorldListInfo->m_ViewFogVolume = MAT_SORT_GROUP_STRICTLY_ABOVEWATER;
		}
		m_pWorldListInfo->m_LeafCount		= m_pRenderList->m_leaves.Count();
		m_pWorldListInfo->m_pLeafDataList	= m_pRenderList->m_leaves.Base();
		m_pWorldListInfo->m_bHasWater		= m_pRenderList->m_bWaterVisible;
	}
	
	return JOB_OK;
}

//-----------------------------------------------------------------------------
// Purpose: recurse on the BSP tree, calling the surface visitor
// Input  : *node - BSP node
//-----------------------------------------------------------------------------
void CBuildWorldListsJob::R_RecursiveWorldNode( CWorldRenderList *pRenderList, mnode_t *node )
{
	int			side;
	cplane_t	*plane;
	float		dot;

	while (true)
	{
#if defined( _X360 ) || defined( _PS3 )
		PREFETCH_128(node->plane,0);
#endif
		// no polygons in solid nodes
		if (node->contents == CONTENTS_SOLID)
			return;		// solid

		// Check PVS signature
		//if (node->visframe != m_visFrameCount)	// original, causes flicker in rare circumstances, race condition if view m > view n pvs setup (viscache) overwrites this var in main mem
		if (node->visframe < m_visFrameCount) // to protect against race condition, may rarely give false positive - simpler for those rare frames where it occurs rather than locking or duping this member per view
			return;

		// Cull against the screen frustum or the appropriate area's frustum.
		if (node->contents >= -1)
		{
			if ( R_CullNode( node ) )
				return;
		}

		// if a leaf node, draw stuff
		if (node->contents >= 0)
		{
			R_DrawLeaf( pRenderList, (mleaf_t *)node );
			return;
		}
#if defined( _X360 ) || defined( _PS3 )
		PREFETCH_128(node->children[0],0);
		PREFETCH_128(node->children[1],0);
		PREFETCH_128(node->children[0],offsetof(mnode_t,plane));
		PREFETCH_128(node->children[1],offsetof(mnode_t,plane));
#endif

		// node is just a decision point, so go down the appropriate sides

		// find which side of the node we are on
		plane = node->plane;
		if ( plane->type <= PLANE_Z )
		{
			dot = m_modelOrigin[plane->type] - plane->dist;
		}
		else
		{
			dot = DotProduct (m_modelOrigin, plane->normal) - plane->dist;
		}

		// recurse down the children, closer side first.
		// We have to do this because we need to find if the surfaces at this node
		// exist in any visible leaves closer to the camera than the node is. If so,
		// their r_surfacevisframe is set to indicate that we need to render them
		// at this node.
		side = dot >= 0 ? 0 : 1;
		// Recurse down the side closer to the camera
		R_RecursiveWorldNode (pRenderList, node->children[side] );

		// draw stuff on the node

		SurfaceHandle_t surfID = SurfaceHandleFromIndex( node->firstsurface );

		int i = MSurf_Index( surfID );
		int nLastSurface = i + node->numsurfaces;
		CVisitedSurfs &visitedSurfs = pRenderList->m_VisitedSurfs;
		for ( ; i < nLastSurface; ++i, ++surfID )
		{
			// Only render things at this node that have previously been marked as visible
			if ( !visitedSurfs.VisitedSurface( i ) )
				continue;

			// Don't add surfaces that have displacement
			// UNDONE: Don't emit these at nodes in vbsp!
			// UNDONE: Emit them at the end of the surface list
			Assert( !SurfaceHasDispInfo( surfID ) );

			// If a surface is marked to draw at a node, then it's not a func_detail.
			// Only func_detail render at leaves. In the case of normal world surfaces,
			// we only want to render them if they intersect a visible leaf.
			uint32 nFlags = MSurf_Flags( surfID );

			Assert( nFlags & SURFDRAW_NODE );
			Assert( !(nFlags & SURFDRAW_NODRAW) );

			if ( !(nFlags & SURFDRAW_UNDERWATER) && ( side ^ !!(nFlags & SURFDRAW_PLANEBACK)) )
				continue;		// wrong side

			if ( nFlags & SURFDRAW_SKY )
			{
				pRenderList->m_bSkyVisible = true;
			}
			else if( nFlags & SURFDRAW_TRANS )
			{
				Shader_TranslucentWorldSurface( pRenderList, surfID );
			}
			else
			{
				Shader_WorldSurface( pRenderList, surfID );
			}
		}

		// recurse down the side farther from the camera
		// NOTE: With this while loop, this is identical to just calling
		// R_RecursiveWorldNode (node->children[!side]);
		node = node->children[!side];
	}
}

//-----------------------------------------------------------------------------
// Fast path for rendering top-views
//-----------------------------------------------------------------------------
void CBuildWorldListsJob::R_RenderWorldTopView( CWorldRenderList *pRenderList, mnode_t *node )
{
	CVisitedSurfs &visitedSurfs = pRenderList->m_VisitedSurfs;
	do
	{
		// no polygons in solid nodes
		if (node->contents == CONTENTS_SOLID)
			return;		// solid

		// Check PVS signature
		if ( !m_bTopViewNoVisCheck )
		{
			//if (node->visframe != m_visFrameCount)	// original, causes flicker in rare circumstances, race condition if view m > view n pvs setup (viscache) overwrites this var in main mem
			if (node->visframe < m_visFrameCount) // to protect against race condition, may rarely give false positive - simpler for those rare frames where it occurs rather than locking or duping this member per view
				return;
		}

		// Cull against the screen frustum or the appropriate area's frustum.
		if( R_CullNodeTopView( node ) )
			return;

		// if a leaf node, draw stuff
		if (node->contents >= 0)
		{
			R_DrawTopViewLeaf( pRenderList, (mleaf_t *)node );
			return;
		}

#ifdef USE_CONVARS
		if (s_ShaderConvars.m_bDrawWorld)
#endif
		{
			// draw stuff on the node
			SurfaceHandle_t surfID = SurfaceHandleFromIndex( node->firstsurface );
			for ( int i = 0; i < node->numsurfaces; i++, surfID++ )
			{
				if ( !visitedSurfs.VisitSurface( surfID ) )
					continue;

				// Don't add surfaces that have displacement
				if ( SurfaceHasDispInfo( surfID ) )
					continue;

				// If a surface is marked to draw at a node, then it's not a func_detail.
				// Only func_detail render at leaves. In the case of normal world surfaces,
				// we only want to render them if they intersect a visible leaf.
				Assert( (MSurf_Flags( surfID ) & SURFDRAW_NODE) );

				if ( MSurf_Flags( surfID ) & (SURFDRAW_UNDERWATER|SURFDRAW_SKY) )
					continue;

				Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );
				if ( !m_bTopViewNoBackfaceCulling )
				{
					// Back face cull
					if ( (MSurf_Flags( surfID ) & SURFDRAW_NOCULL) == 0 )
					{
						if (MSurf_Plane( surfID ).normal.z <= 0.0f)
							continue;
					}
				}

				// FIXME: For now, blow off translucent world polygons.
				// Gotta solve the problem of how to render them all, unsorted,
				// in a pass after the opaque world polygons, and before the
				// translucent entities.

				if ( !( MSurf_Flags( surfID ) & SURFDRAW_TRANS ) )
//				if ( !surf->texinfo->material->IsTranslucent() )
					Shader_WorldSurface( pRenderList, surfID );
			}
		}

		// Recurse down both children, we don't care the order...
		R_RenderWorldTopView ( pRenderList, node->children[0]);
		node = node->children[1];

	} while (node);
}

//-----------------------------------------------------------------------------
// Purpose: recurse on the BSP tree, calling the surface visitor
// Input  : *node - BSP node
//-----------------------------------------------------------------------------
void CBuildWorldListsJob::R_BuildWorldListNoCull( CWorldRenderList *pRenderList, mnode_t *node )
{
	int leafCount = 0;

	const int NODELIST_MAX = 1024;
	mleaf_t *leafList[NODELIST_MAX];
	mnode_t *nodeList[NODELIST_MAX];
	int nodeReadIndex = 0;
	int nodeWriteIndex = 0;

	while (true)
	{
		// no polygons in solid nodes
		//if (node->contents != CONTENTS_SOLID && node->visframe == r_visframecount )	// original, causes flicer in rare circumstances, race condition if view m > view n pvs setup (viscache) overwrites this var in main mem
		if (node->contents != CONTENTS_SOLID && node->visframe >= m_visFrameCount ) // to protect against race condition, may rarely give false positive - simpler for those rare frames where it occurs rather than locking or duping this member per view
		{
			if ( node->contents < -1 || !R_CullNode( node ) )
			{
				// if a leaf node, draw stuff
				if (node->contents >= 0)
				{
					if ( leafCount < NODELIST_MAX )
					{
						leafList[leafCount++] = (mleaf_t *)node;
					}
				}
				else
				{
#if defined( _X360 ) || defined( _PS3 )
					PREFETCH_128(node->children[0],0);
					PREFETCH_128(node->children[1],0);
#endif
					// node is just a decision point, so go down the appropriate sides
					nodeList[nodeWriteIndex] = node->children[0];
					nodeWriteIndex = (nodeWriteIndex+1) & (NODELIST_MAX-1);
					// check for overflow of the ring buffer
					Assert(nodeWriteIndex != nodeReadIndex);
					node = node->children[1];
					continue;
				}
			}
		}

		if ( nodeReadIndex == nodeWriteIndex )
			break;
		node = nodeList[nodeReadIndex];
		nodeReadIndex = (nodeReadIndex+1) & (NODELIST_MAX-1);
	}
	for ( int i = 0; i < leafCount; i++ )
	{
		R_DrawLeafNoCull( pRenderList, leafList[i] );
	}
}

//-----------------------------------------------------------------------------
// culls a node to the frustum or area frustum
//-----------------------------------------------------------------------------
bool CBuildWorldListsJob::R_CullNode( mnode_t *pNode )
{
	if ( !m_bViewerInSolidSpace && pNode->area > 0 )
	{
		// First make sure its whole area is even visible.
		//if( !R_IsAreaVisible( pNode->area ) )
		if ( ( m_RenderAreaBits[pNode->area>>3] & GetBitForBitnum(pNode->area&7) ) == 0 )
			return true;
		return CullNodeSIMD( m_pAreaFrustum->Element( pNode->area ), pNode );
	}

	return CullNodeSIMD( *m_pFrustum, pNode );
}

bool CBuildWorldListsJob::R_CullNodeTopView( mnode_t *pNode )
{
	if ( m_pTopViewVolumeCuller )
	{
		return !m_pTopViewVolumeCuller->CheckBoxCenterHalfDiagonal( pNode->m_vecCenter, pNode->m_vecHalfDiagonal );
	}

	Vector2D delta, size;
	Vector2DSubtract( pNode->m_vecCenter.AsVector2D(), m_OrthographicCenter, delta );
	Vector2DAdd( pNode->m_vecHalfDiagonal.AsVector2D(), m_OrthographicHalfDiagonal, size );
	return ( FloatMakePositive( delta.x ) > size.x ) ||
		( FloatMakePositive( delta.y ) > size.y );
}

//-----------------------------------------------------------------------------
// Draws all displacements + surfaces in a leaf
//-----------------------------------------------------------------------------
void CBuildWorldListsJob::R_DrawLeaf( CWorldRenderList *pRenderList, mleaf_t *pleaf )
{
	SurfaceHandle_t *pSurfID = &host_state.worldbrush->marksurfaces[pleaf->firstmarksurface];
#if defined( _X360 ) || defined( _PS3 )
	PREFETCH_128(pSurfID,0);
#endif
	// Add this leaf to the list of visible leaves
	UpdateVisibleLeafLists( pRenderList, pleaf );

	// Debugging to only draw at a particular leaf
#ifdef USE_CONVARS
	if ( (s_ShaderConvars.m_nDrawLeaf >= 0) && (s_ShaderConvars.m_nDrawLeaf != LeafToIndex(pleaf)) )
		return;
#endif

	// add displacement surfaces
	DrawDisplacementsInLeaf( pRenderList, pleaf );

#ifdef USE_CONVARS
	if( !s_ShaderConvars.m_bDrawWorld )
		return;
#endif

	// Add non-displacement surfaces
#if defined( _X360 ) || defined( _PS3 )
	int count = MIN(pleaf->nummarksurfaces, 7);
	for ( int i = 0; i < count; ++i )
	{
		PREFETCH_128(pSurfID[i],0);
	}
#endif

	int i;
	int nSurfaceCount = pleaf->nummarknodesurfaces;
	CVisitedSurfs &visitedSurfs = pRenderList->m_VisitedSurfs;
	for ( i = 0; i < nSurfaceCount; ++i )
	{
		SurfaceHandle_t surfID = pSurfID[i];
		ASSERT_SURF_VALID( surfID );
		// there are never any displacements or nodraws in the leaf list
		Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );
		Assert( (MSurf_Flags( surfID ) & SURFDRAW_NODE) );
		Assert( !SurfaceHasDispInfo(surfID) );
		// mark this one to be drawn at the node
		visitedSurfs.MarkSurfaceVisited( surfID );
#if defined( _X360 ) || defined( _PS3 )
		PREFETCH_128(pSurfID[i+7],0);
#endif
	}

#ifdef USE_CONVARS
	if( !s_ShaderConvars.m_bDrawFuncDetail )
		return;
#endif

	for ( ; i < pleaf->nummarksurfaces; i++ )
	{
		SurfaceHandle_t surfID = pSurfID[i];
#if defined( _X360 )// || defined( _PS3 )
		PREFETCH_128(surfID->plane,0);
		PREFETCH_128(pSurfID[i+7],0);
#endif

		// Don't process the same surface twice
		if ( !visitedSurfs.VisitSurface( surfID ) )
			continue;
		uint32 flags = surfID->flags;
		Assert( !(flags & SURFDRAW_NODE) );

		// Back face cull; only func_detail are drawn here
		if ( (flags & SURFDRAW_NOCULL) == 0 )
		{
#if !defined(_PS3)
			if ( (DotProduct(surfID->plane->normal, m_modelOrigin) - surfID->plane->dist ) < BACKFACE_EPSILON )
				continue;
#else
			if ( (DotProduct(surfID->m_plane.normal, m_modelOrigin) - surfID->m_plane.dist ) < BACKFACE_EPSILON )
				continue;
#endif		
		}

		int sortGroup = (flags & SURFDRAW_SORTGROUP_MASK) >> SURFDRAW_SORTGROUP_SHIFT;

		if ( flags & (SURFDRAW_HASLIGHTSYTLES|SURFDRAW_HASDLIGHT) )
		{
			pRenderList->m_DlightSurfaces[sortGroup].AddToTail( surfID );
		}

		if ( flags & SURFDRAW_PAINTED )
		{
			pRenderList->m_PaintedSurfaces->AddToTail( surfID );
		}

		if( flags & SURFDRAW_TRANS )
		{
			pRenderList->m_AlphaSurfaces.AddToTail(surfID);
		}
		else
		{
			// Add decals on non-displacement surfaces
			if( SurfaceHasDecals( surfID ) )
			{
				pRenderList->QueueDecalSurf( surfID, sortGroup );
			}

			int nMaterialSortID = MSurf_MaterialSortID( surfID );

			pRenderList->m_SortList.AddSurfaceToTail( surfID, sortGroup, nMaterialSortID );
		}
	}
}

//-----------------------------------------------------------------------------
// Draws all displacements + surfaces in a leaf
//-----------------------------------------------------------------------------
void CBuildWorldListsJob::R_DrawTopViewLeaf( CWorldRenderList *pRenderList, mleaf_t *pleaf )
{
	// Add this leaf to the list of visible leaves
	UpdateVisibleLeafLists( pRenderList, pleaf );

	// add displacement surfaces
	DrawDisplacementsInLeaf( pRenderList, pleaf );

#ifdef USE_CONVARS
	if( !s_ShaderConvars.m_bDrawWorld )
		return;
#endif

	// Add non-displacement surfaces
	SurfaceHandle_t *pHandle = &host_state.worldbrush->marksurfaces[pleaf->firstmarksurface];
	CVisitedSurfs &visitedSurfs = pRenderList->m_VisitedSurfs;
	for ( int i = 0; i < pleaf->nummarksurfaces; i++ )
	{
		SurfaceHandle_t surfID = pHandle[i];

		// Mark this surface as being in a visible leaf this frame. If this
		// surface is meant to be drawn at a node (SURFDRAW_NODE), 
		// then it will be drawn in the recursive code down below.
		if ( !visitedSurfs.VisitSurface( surfID ) )
			continue;

		// Don't add surfaces that have displacement; they are handled above
		// In fact, don't even set the vis frame; we need it unset for translucent
		// displacement code
		if ( SurfaceHasDispInfo(surfID) )
			continue;

		if ( MSurf_Flags( surfID ) & SURFDRAW_NODE )
			continue;

		Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );

		if ( !m_bTopViewNoBackfaceCulling )
		{
			// Back face cull; only func_detail are drawn here
			if ( (MSurf_Flags( surfID ) & SURFDRAW_NOCULL) == 0 )
			{
				if (MSurf_Plane( surfID ).normal.z <= 0.0f)
					continue;
			}
		}

		// FIXME: For now, blow off translucent world polygons.
		// Gotta solve the problem of how to render them all, unsorted,
		// in a pass after the opaque world polygons, and before the
		// translucent entities.

		if ( !( MSurf_Flags( surfID ) & SURFDRAW_TRANS ))
//		if ( !surf->texinfo->material->IsTranslucent() )
			Shader_WorldSurface( pRenderList, surfID );
	}
}

void CBuildWorldListsJob::R_DrawLeafNoCull( CWorldRenderList *pRenderList, mleaf_t *pleaf )
{
	// Add this leaf to the list of visible leaves
	UpdateVisibleLeafLists( pRenderList, pleaf );

	// add displacement surfaces
	DrawDisplacementsInLeaf( pRenderList, pleaf );
	int i;
	SurfaceHandle_t *pSurfID = &host_state.worldbrush->marksurfaces[pleaf->firstmarksurface];
	CVisitedSurfs &visitedSurfs = pRenderList->m_VisitedSurfs;
	for ( i = 0; i < pleaf->nummarksurfaces; i++ )
	{
		SurfaceHandle_t surfID = pSurfID[i];

		// Don't process the same surface twice
		if ( !visitedSurfs.VisitSurface( surfID ) )
			continue;

		R_DrawSurfaceNoCull( pRenderList, surfID );
	}
}

// The NoCull flavor of this function calls functions which optimize for shadow depth map rendering
void CBuildWorldListsJob::R_DrawSurfaceNoCull( CWorldRenderList *pRenderList, SurfaceHandle_t surfID )
{
	ASSERT_SURF_VALID( surfID );
	if( !(MSurf_Flags( surfID ) & SURFDRAW_TRANS) && !(MSurf_Flags( surfID ) & SURFDRAW_SKY) )
	{
		Shader_WorldSurfaceNoCull( pRenderList, surfID );
	}
}

void CBuildWorldListsJob::DrawDisplacementsInLeaf( CWorldRenderList *pRenderList, mleaf_t* pLeaf )
{
	// add displacement surfaces
	if (!pLeaf->dispCount)
		return;

	CVisitedSurfs &visitedSurfs = pRenderList->m_VisitedSurfs;
	for ( int i = 0; i < pLeaf->dispCount; i++ )
	{
		CDispInfo *pDispInfo = static_cast<CDispInfo *>(MLeaf_Disaplcement( pLeaf, i ));

		// NOTE: We're not using the displacement's touched method here 
		// because we're just using the parent surface's visframe in the
		// surface add methods below...
		SurfaceHandle_t parentSurfID = pDispInfo->m_ParentSurfID;

		// already processed this frame? Then don't do it again!
		if ( visitedSurfs.VisitSurface( parentSurfID ) )
		{
			if ( m_pFrustum->CullBox( pDispInfo->m_BBoxMin, pDispInfo->m_BBoxMax ) )
				continue;
			if ( MSurf_Flags( parentSurfID ) & SURFDRAW_TRANS)
			{
				Shader_TranslucentDisplacementSurface( pRenderList, parentSurfID );
			}
			else
			{
				Shader_DisplacementSurface( pRenderList, parentSurfID );
			}
		}
	}
}

void CBuildWorldListsJob::Shader_WorldSurface( CWorldRenderList *pRenderList, SurfaceHandle_t surfID )
{
	// Hook it into the list of surfaces to render with this material
	// Do it in a way that generates a front-to-back ordering for fast z reject
	Assert( !SurfaceHasDispInfo( surfID ) );

	// Each surface is in exactly one group
	int nSortGroup = MSurf_SortGroup( surfID );

	// Add decals on non-displacement surfaces
	if( SurfaceHasDecals( surfID ) )
	{
		pRenderList->QueueDecalSurf( surfID, nSortGroup );
	}

	int nMaterialSortID = MSurf_MaterialSortID( surfID );

	if ( MSurf_Flags( surfID ) & (SURFDRAW_HASLIGHTSYTLES|SURFDRAW_HASDLIGHT) )
	{
		pRenderList->m_DlightSurfaces[nSortGroup].AddToTail( surfID );
	}

	if ( MSurf_Flags( surfID ) & SURFDRAW_PAINTED )
	{
		pRenderList->m_PaintedSurfaces[nSortGroup].AddToTail( surfID );
	}

	pRenderList->m_SortList.AddSurfaceToTail( surfID, nSortGroup, nMaterialSortID );
}

// The NoCull flavor of this function optimizes for shadow depth map rendering
// No decal work, dlights or material sorting, for example
void CBuildWorldListsJob::Shader_WorldSurfaceNoCull( CWorldRenderList *pRenderList, SurfaceHandle_t surfID )
{
	// Hook it into the list of surfaces to render with this material
	// Do it in a way that generates a front-to-back ordering for fast z reject
	Assert( !SurfaceHasDispInfo( surfID ) );

	// Each surface is in exactly one group
	int nSortGroup = MSurf_SortGroup( surfID );

	int nMaterialSortID = MSurf_MaterialSortID( surfID );
	pRenderList->m_SortList.AddSurfaceToTail( surfID, nSortGroup, nMaterialSortID );
}


//-----------------------------------------------------------------------------
// Main entry points for starting + ending rendering the world
//-----------------------------------------------------------------------------
void R_BuildWorldLists( IWorldRenderList *pRenderListIn, WorldListInfo_t* pInfo, 
	int iForceViewLeaf, const VisOverrideData_t* pVisData, bool bShadowDepth /* = false */, float *pWaterReflectionHeight )
{
	CWorldRenderList *pRenderList = assert_cast<CWorldRenderList *>(pRenderListIn);
	// Safety measure just in case. I haven't seen that we need this, but...
	if ( g_LostVideoMemory )
	{
		if (pInfo)
		{
			pInfo->m_ViewFogVolume = MAT_SORT_GROUP_STRICTLY_ABOVEWATER;
			pInfo->m_LeafCount = 0;
			pInfo->m_pLeafDataList = pRenderList->m_leaves.Base();
			pInfo->m_bHasWater = pRenderList->m_bWaterVisible;
		}
		return;
	}

	VPROF( "R_BuildWorldLists" );

	SNPROF( "R_BuildWorldLists" );

	VectorCopy( g_EngineRenderer->ViewOrigin(), modelorg );

	Shader_WorldBegin( pRenderList );



#if defined( _PS3 )
	extern IBaseClientDLL *g_ClientDLL;
	extern CUtlVector< Frustum_t > g_AreaFrustum;
	extern unsigned char g_RenderAreaBits[32];
	extern bool g_bViewerInSolidSpace;

	if( r_PS3_SPU_buildworldlists.GetInt() && g_ClientDLL->IsSPUBuildWRJobsOn() )
	{
		// Run BuildWorldLists on SPU

		// this goes hand-in-hand with building renderables on SPU and runs the job in parallel while the PPU continues 
		// a sync point is required while drawing and entry into here assumes 2 passes over the rendering
		// lists are built during the 1st pass, drawn during the 2nd (where/when the jobs started in pass 1 are synced)

		Frustum_t *pgFrustum, *pgAreaFrustum;
		unsigned char *pgRenderAreaBits;
		void *pVC = NULL;

		SNPROF("R_BuildWorldLists_SPUpath");

		if ( !r_drawtopview )
		{
			R_SetupAreaBits( iForceViewLeaf, pVisData, pWaterReflectionHeight );
		}

		g_ClientDLL->CacheFrustumData( &g_Frustum, g_AreaFrustum.Base(), g_RenderAreaBits, g_AreaFrustum.Count(), g_bViewerInSolidSpace );

		if( s_pTopViewVolumeCuller )
		{
			pVC = g_ClientDLL->GetBuildViewVolumeCuller();
		}
		pgFrustum		 = g_ClientDLL->GetBuildViewFrustum();
		pgAreaFrustum	 = g_ClientDLL->GetBuildViewAreaFrustum();
		pgRenderAreaBits = g_ClientDLL->GetBuildViewRenderAreaBits();

		int buildViewID  = g_ClientDLL->GetBuildViewID();

		job_buildworldlists::JobParams_t* pParam = job_buildworldlists::GetJobParams( &g_buildWorldListsJobDescriptor[ buildViewID ] );

		pRenderList->EnsureCapacityForSPU( AlignValue( materials->GetNumSortIDs(), 16 ), host_state.worldbrush->numsurfaces );

		pRenderList->FillOutputParamsForSPU( &g_buildWorldListsDMAOutData[ buildViewID ] );

		g_pBuildWorldListsJob->BuildWorldLists_SPU( pParam, &g_buildWorldListsDMAOutData[ buildViewID ], 
													r_drawtopview, pVC/*(void *)s_pTopViewVolumeCuller*/, s_OrthographicCenter.Base(), s_OrthographicHalfDiagonal.Base(), r_bTopViewNoBackfaceCulling, r_bTopViewNoVisCheck, 
													bShadowDepth, pInfo, pRenderList->m_leaves.Base(), g_ClientDLL->GetDrawFlags(), 
													pgFrustum, pgAreaFrustum, pgRenderAreaBits, 
													buildViewID );
	}
	else
#endif
	{
		extern IBaseClientDLL *g_ClientDLL;
		extern CUtlVector< Frustum_t, CUtlMemoryAligned< Frustum_t,16 > > g_AreaFrustum;
		extern unsigned char g_RenderAreaBits[32];
		extern bool g_bViewerInSolidSpace;

		if ( !r_drawtopview )
		{
			R_SetupAreaBits( iForceViewLeaf, pVisData, pWaterReflectionHeight );
		}

		g_ClientDLL->CacheFrustumData( g_Frustum, g_AreaFrustum );

		// TODO get frustum, aera frustums volume culler from g_viewBuilder
		CBuildWorldListsJob* pJob = new CBuildWorldListsJob( 
			pRenderList,
			pInfo,
			bShadowDepth,
			CurrentViewOrigin(),
			r_visframecount,
			r_drawtopview,
			r_bTopViewNoBackfaceCulling, 
			r_bTopViewNoVisCheck, 
			s_OrthographicCenter, 
			s_OrthographicHalfDiagonal, 
			s_pTopViewVolumeCuller,
			g_ClientDLL->GetBuildViewFrustum(),
			g_ClientDLL->GetBuildViewAeraFrustums(),
			g_RenderAreaBits,
			g_bViewerInSolidSpace,
			modelorg );

		g_ClientDLL->QueueBuildWorldListJob( pJob );


		pJob->Release();
	}

}

//-----------------------------------------------------------------------------
// Call this before rendering; it clears out the lists of stuff to render
//-----------------------------------------------------------------------------
void Shader_WorldBegin_Pass2( CWorldRenderList *pRenderList )
{
	// Clear out the decal list
	DecalSurfacesInit( false );

	// Clear out the render lists of overlays
	//	OverlayMgr()->ClearRenderLists();

	// Clear out the render lists of shadows
	//	g_pShadowMgr->ClearShadowRenderList( );
}

#if defined(_PS3)

void R_BuildWorldLists_PS3_Epilogue( IWorldRenderList *pRenderListIn, WorldListInfo_t* pInfo, bool bShadowDepth )
{
	CWorldRenderList *pRenderList = assert_cast<CWorldRenderList *>(pRenderListIn);

	// if doing 2 pass, this epilogue will be on 2nd pass 
	// => need to call a lite version of Shader_WorldBegin that does away with resetting the world lists

	Shader_WorldBegin_Pass2( pRenderList );



	// Don't bother in topview?
	if ( !r_drawtopview && !bShadowDepth )
	{
		// epilogue - add decal surfs
		pRenderList->AddSPUDecalSurfs();

		// This builds all lightmaps, including those for translucent surfaces
		Shader_BuildDynamicLightmaps( pRenderList );
	}

	// 
	if ( pInfo )
	{
		// Compute fog volume info for rendering
		if ( !bShadowDepth )
		{
			FogVolumeInfo_t fogInfo;
			ComputeFogVolumeInfo( &fogInfo, CurrentViewOrigin() );
			if( fogInfo.m_InFogVolume )
			{
				pInfo->m_ViewFogVolume = MAT_SORT_GROUP_STRICTLY_UNDERWATER;
			}
			else
			{
				pInfo->m_ViewFogVolume = MAT_SORT_GROUP_STRICTLY_ABOVEWATER;
			}
		}
		else
		{
			pInfo->m_ViewFogVolume = MAT_SORT_GROUP_STRICTLY_ABOVEWATER;
		}
	}

//	Msg("PPU LeafCount %d\n", pRenderList->m_leaves.Count());


}

#else

void R_BuildWorldLists_Epilogue( IWorldRenderList *pRenderListIn, WorldListInfo_t* pInfo, bool bShadowDepth )
{
	CWorldRenderList *pRenderList = assert_cast<CWorldRenderList *>(pRenderListIn);
	
	// if doing 2 pass, this epilogue will be on 2nd pass 
	// => need to call a lite version of Shader_WorldBegin that does away with resetting the world lists

	Shader_WorldBegin_Pass2( pRenderList );

	// Add decal
	pRenderList->AddDecalSurfs();

	// This builds all lightmaps, including those for translucent surfaces
	// Don't bother in topview?
	if ( !r_drawtopview && !bShadowDepth )
	{
		Shader_BuildDynamicLightmaps( pRenderList );
	}
}

#endif

//-----------------------------------------------------------------------------
// Used to determine visible fog volumes
//-----------------------------------------------------------------------------
class CVisibleFogVolumeQuery
{
public:
	void FindVisibleFogVolume( const Vector &vecViewPoint, const VisOverrideData_t *pVisOverrideData, int *pVisibleFogVolume, int *pVisibleFogVolumeLeaf );

private:
	bool RecursiveGetVisibleFogVolume( mnode_t *node );

	// Input
	Vector m_vecSearchPoint;

	// Output
	int m_nVisibleFogVolume;
	int m_nVisibleFogVolumeLeaf;
};


//-----------------------------------------------------------------------------
// Main entry point for the query
//-----------------------------------------------------------------------------
void CVisibleFogVolumeQuery::FindVisibleFogVolume( const Vector &vecViewPoint, const VisOverrideData_t *pVisOverrideData, int *pVisibleFogVolume, int *pVisibleFogVolumeLeaf )
{
	R_SetupAreaBits( -1, pVisOverrideData );

	m_vecSearchPoint = vecViewPoint;
	m_nVisibleFogVolume = -1;
	m_nVisibleFogVolumeLeaf = -1;

	RecursiveGetVisibleFogVolume( host_state.worldbrush->nodes );

	*pVisibleFogVolume = m_nVisibleFogVolume;
	*pVisibleFogVolumeLeaf = m_nVisibleFogVolumeLeaf;
}

//-----------------------------------------------------------------------------
// return true to continue searching
//-----------------------------------------------------------------------------
bool CVisibleFogVolumeQuery::RecursiveGetVisibleFogVolume( mnode_t *node )
{
	int			side;
	cplane_t	*plane;
	float		dot;

	// no polygons in solid nodes
	if (node->contents == CONTENTS_SOLID)
		return true;		// solid

	// Check PVS signature
	if (node->visframe != r_visframecount)
		return true;

	// Cull against the screen frustum or the appropriate area's frustum.
	if( R_CullNode( node ) )
		return true;

	// if a leaf node, check if we are in a fog volume and get outta here.
	if (node->contents >= 0)
	{
		mleaf_t *pLeaf = (mleaf_t *)node;

		// Don't return a leaf that's not filled with liquid
		if ( pLeaf->leafWaterDataID == -1 )
			return true;

		// Never return SLIME as being visible, as it's opaque
		if ( pLeaf->contents & CONTENTS_SLIME )
			return true;

		m_nVisibleFogVolume = pLeaf->leafWaterDataID;
		m_nVisibleFogVolumeLeaf = pLeaf - host_state.worldbrush->leafs;
		return false;  // found it, so stop searching
	}

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	plane = node->plane;
	if ( plane->type <= PLANE_Z )
	{
		dot = m_vecSearchPoint[plane->type] - plane->dist;
	}
	else
	{
		dot = DotProduct( m_vecSearchPoint, plane->normal ) - plane->dist;
	}

	// recurse down the children, closer side first.
	// We have to do this because we need to find if the surfaces at this node
	// exist in any visible leaves closer to the camera than the node is. If so,
	// their r_surfacevisframe is set to indicate that we need to render them
	// at this node.
	side = (dot >= 0) ? 0 : 1;

	// Recurse down the side closer to the camera
	if( !RecursiveGetVisibleFogVolume (node->children[side]) )
		return false;

	// recurse down the side farther from the camera
	return RecursiveGetVisibleFogVolume (node->children[!side]);
}


static void ClearFogInfo( VisibleFogVolumeInfo_t *pInfo )
{
	pInfo->m_bEyeInFogVolume = false;
	pInfo->m_nVisibleFogVolume = -1;
	pInfo->m_nVisibleFogVolumeLeaf = -1;
	pInfo->m_pFogVolumeMaterial = NULL;
	pInfo->m_flWaterHeight = INVALID_WATER_HEIGHT;
}

ConVar fast_fogvolume("fast_fogvolume", "0");
//-----------------------------------------------------------------------------
// Main entry point from renderer to get the fog volume
//-----------------------------------------------------------------------------
void R_GetVisibleFogVolume( const Vector& vEyePoint, const VisOverrideData_t *pVisOverrideData, VisibleFogVolumeInfo_t *pInfo )
{
	VPROF_BUDGET( "R_GetVisibleFogVolume", VPROF_BUDGETGROUP_WORLD_RENDERING );

	if ( host_state.worldmodel->brush.pShared->numleafwaterdata == 0 )
	{
		ClearFogInfo( pInfo );
		return;
	}

	int nLeafID = CM_PointLeafnum( vEyePoint );
	mleaf_t* pLeaf = &host_state.worldbrush->leafs[nLeafID];

	int nLeafContents = pLeaf->contents;
	if ( pLeaf->leafWaterDataID != -1 )
	{
		Assert( nLeafContents & (CONTENTS_SLIME | CONTENTS_WATER) );
		pInfo->m_bEyeInFogVolume = true;
		pInfo->m_nVisibleFogVolume = pLeaf->leafWaterDataID;
		pInfo->m_nVisibleFogVolumeLeaf = nLeafID;
		pInfo->m_pFogVolumeMaterial = R_GetFogVolumeMaterial( pInfo->m_nVisibleFogVolume, true );
		pInfo->m_flWaterHeight = R_GetWaterHeight( pInfo->m_nVisibleFogVolume );
	}
	else if ( nLeafContents & CONTENTS_TESTFOGVOLUME )
	{
		Assert( (nLeafContents & (CONTENTS_SLIME | CONTENTS_WATER)) == 0 );
		if ( fast_fogvolume.GetBool() && host_state.worldbrush->numleafwaterdata == 1 )
		{
			pInfo->m_nVisibleFogVolume = 0;
			pInfo->m_nVisibleFogVolumeLeaf = host_state.worldbrush->leafwaterdata[0].firstLeafIndex;
		}
		else
		{
			CVisibleFogVolumeQuery query;
			query.FindVisibleFogVolume( vEyePoint, pVisOverrideData, &pInfo->m_nVisibleFogVolume, &pInfo->m_nVisibleFogVolumeLeaf ); 
		}

		pInfo->m_bEyeInFogVolume = false;
		pInfo->m_pFogVolumeMaterial = R_GetFogVolumeMaterial( pInfo->m_nVisibleFogVolume, false );
		pInfo->m_flWaterHeight = R_GetWaterHeight( pInfo->m_nVisibleFogVolume );
	}
	else
	{
		ClearFogInfo( pInfo );
	}

	if( host_state.worldbrush->m_LeafMinDistToWater )
	{
		pInfo->m_flDistanceToWater = ( float )host_state.worldbrush->m_LeafMinDistToWater[nLeafID];
	}
	else
	{
		pInfo->m_flDistanceToWater = 0.0f;
	}
}


//-----------------------------------------------------------------------------
// Draws the list of surfaces build in the BuildWorldLists phase
//-----------------------------------------------------------------------------

// Uncomment this to allow code to draw wireframe over a particular surface for debugging
//#define DEBUG_SURF 1

#ifdef DEBUG_SURF
int g_DebugSurfIndex = -1;
#endif

void R_DrawWorldLists( IMatRenderContext *pRenderContext, IWorldRenderList *pRenderListIn, unsigned long flags, float waterZAdjust )
{
	CWorldRenderList *pRenderList = assert_cast<CWorldRenderList *>(pRenderListIn);
	if ( g_bTextMode || g_LostVideoMemory )
		return;

	VPROF("R_DrawWorldLists");
	if ( flags & DRAWWORLDLISTS_DRAW_WORLD_GEOMETRY )
	{
		Shader_WorldEnd( pRenderContext, pRenderList, flags, waterZAdjust );
	}
	else if ( flags & ( DRAWWORLDLISTS_DRAW_SIMPLE_WORLD_MODEL | DRAWWORLDLISTS_DRAW_SIMPLE_WORLD_MODEL_WATER ) )
	{
		DrawSimpleWorldModel( flags );
	}

	if ( flags & DRAWWORLDLISTS_DRAW_DECALS_AND_OVERLAYS )
	{
		Shader_DrawWorldDecalsAndOverlays( pRenderContext, pRenderList, flags, waterZAdjust );
	}

#ifdef DEBUG_SURF
	{
		VPROF("R_DrawWorldLists (DEBUG_SURF)");
		if (g_pDebugSurf)
		{
			pRenderContext->Bind( g_materialWorldWireframe );
			Shader_DrawSurfaceDynamic( pRenderContext, g_pDebugSurf );
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Counts the total number of indices needed to render a world list
//-----------------------------------------------------------------------------
int R_GetNumIndicesForWorldList( IWorldRenderList *pRenderListIn, unsigned long nFlags )
{
	CWorldRenderList *pRenderList = assert_cast<CWorldRenderList *>( pRenderListIn );

	int nNumIndices = 0;
	for ( int g = 0; g < MAX_MAT_SORT_GROUPS; ++g )
	{
		if ( ( nFlags & ( 1 << g ) ) == 0 )
			continue;

		int nSortGroup = s_DrawWorldListsToSortGroup[ g ];
		MSL_FOREACH_GROUP_BEGIN( pRenderList->m_SortList, nSortGroup, group )
		{
			nNumIndices += group.triangleCount * 3;
		}
		MSL_FOREACH_GROUP_END()
	}

	return nNumIndices;
}


void R_GetWorldListIndicesInfo( WorldListIndicesInfo_t * pInfoOut, IWorldRenderList *pRenderListIn, unsigned long nFlags )
{
	CWorldRenderList *pRenderList = assert_cast<CWorldRenderList *>( pRenderListIn );

	uint nTotalTriangles = 0, nMaxBatchTriangles = 0;
	for ( int g = 0; g < MAX_MAT_SORT_GROUPS; ++g )
	{
		if ( ( nFlags & ( 1 << g ) ) == 0 )
			continue;

		int nSortGroup = s_DrawWorldListsToSortGroup[ g ];
		MSL_FOREACH_GROUP_BEGIN( pRenderList->m_SortList, nSortGroup, group )
		{
			nTotalTriangles += group.triangleCount;
			nMaxBatchTriangles = MAX( nMaxBatchTriangles, group.triangleCount ); 
		}
		MSL_FOREACH_GROUP_END()
	}
	pInfoOut->m_nMaxBatchIndices = nMaxBatchTriangles * 3; 
	pInfoOut->m_nTotalIndices = nTotalTriangles * 3;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void R_SceneBegin( void )
{
	ComputeDebugSettings();
}

void R_SceneEnd( void )
{

#if defined(_PS3)
	R_FrameEndSPURSSync( 0 );
#endif
}

//-----------------------------------------------------------------------------
// Debugging code to draw the lightmap pages
//-----------------------------------------------------------------------------

void Shader_DrawLightmapPageSurface( SurfaceHandle_t surfID, float red, float green, float blue )
{
	Vector2D lightCoords[32][4];

	int bumpID, count;
	if ( MSurf_Flags( surfID ) & SURFDRAW_BUMPLIGHT )
	{
		count = NUM_BUMP_VECTS + 1;
	}
	else
	{
		count = 1;
	}

	BuildMSurfaceVerts( host_state.worldbrush, surfID, NULL, NULL, lightCoords );

	int lightmapPageWidth, lightmapPageHeight;

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->Bind( g_materialWireframe );
	materials->GetLightmapPageSize( 
		SortInfoToLightmapPage(MSurf_MaterialSortID( surfID )), 
		&lightmapPageWidth, &lightmapPageHeight );

	for( bumpID = 0; bumpID < count; bumpID++ )
	{
		// assumes that we are already in ortho mode.
		IMesh* pMesh = pRenderContext->GetDynamicMesh( );
					
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_LINES, MSurf_VertCount( surfID ) );
		
		int i;

		for( i = 0; i < MSurf_VertCount( surfID ); i++ )
		{
			float x, y;
			float *texCoord;

			texCoord = &lightCoords[i][bumpID][0];

			x = lightmapPageWidth * texCoord[0];
			y = lightmapPageHeight * texCoord[1];

			meshBuilder.Position3f( x, y, 0.0f );
			meshBuilder.AdvanceVertex();

			texCoord = &lightCoords[(i+1)%MSurf_VertCount( surfID )][bumpID][0];
			x = lightmapPageWidth * texCoord[0];
			y = lightmapPageHeight * texCoord[1];

			meshBuilder.Position3f( x, y, 0.0f );
			meshBuilder.AdvanceVertex();
		}
		
		meshBuilder.End();
		pMesh->Draw();
	}
}

void Shader_DrawLightmapPageChains( IWorldRenderList *pRenderListIn, int pageId )
{
	CWorldRenderList *pRenderList = assert_cast<CWorldRenderList *>(pRenderListIn);
	for (int j = 0; j < MAX_MAT_SORT_GROUPS; ++j)
	{
		MSL_FOREACH_GROUP_BEGIN( pRenderList->m_SortList, j, group )
		{
			SurfaceHandle_t surfID = pRenderList->m_SortList.GetSurfaceAtHead( group );
			Assert(IS_SURF_VALID(surfID));
			Assert( MSurf_MaterialSortID( surfID ) >= 0 && MSurf_MaterialSortID( surfID ) < g_WorldStaticMeshes.Count() );
			if( materialSortInfoArray[MSurf_MaterialSortID( surfID ) ].lightmapPageID != pageId )
			{
				continue;
			}
			MSL_FOREACH_SURFACE_IN_GROUP_BEGIN( pRenderList->m_SortList, group, surfID )
			{
				Assert( !SurfaceHasDispInfo( surfID ) );
				Shader_DrawLightmapPageSurface( surfID, 0.0f, 1.0f, 0.0f );
			}
			MSL_FOREACH_SURFACE_IN_GROUP_END()
		}
		MSL_FOREACH_GROUP_END()

		// render displacement lightmap page info
		MSL_FOREACH_SURFACE_BEGIN(pRenderList->m_DispSortList, j, surfID)
		{
			surfID->pDispInfo->RenderWireframeInLightmapPage( pageId );
		}
		MSL_FOREACH_SURFACE_END()
	}
}

//-----------------------------------------------------------------------------
//
// All code related to brush model rendering
//
//-----------------------------------------------------------------------------

class CBrushSurface : public IBrushSurface
{
public:
	CBrushSurface( SurfaceHandle_t surfID );

	// Computes texture coordinates + lightmap coordinates given a world position
	virtual void ComputeTextureCoordinate( const Vector& worldPos, Vector2D& texCoord );
	virtual void ComputeLightmapCoordinate( const Vector& worldPos, Vector2D& lightmapCoord );

	// Gets the vertex data for this surface
	virtual int  GetVertexCount() const;
	virtual void GetVertexData( BrushVertex_t* pVerts );

	// Gets at the material properties for this surface
	virtual IMaterial* GetMaterial();

private:
	SurfaceHandle_t m_SurfaceID;
	SurfaceCtx_t m_Ctx;
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CBrushSurface::CBrushSurface( SurfaceHandle_t surfID ) : m_SurfaceID(surfID)
{
	Assert(IS_SURF_VALID(surfID));
	SurfSetupSurfaceContext( m_Ctx, surfID );
}


//-----------------------------------------------------------------------------
// Computes texture coordinates + lightmap coordinates given a world position
//-----------------------------------------------------------------------------
void CBrushSurface::ComputeTextureCoordinate( const Vector& worldPos, Vector2D& texCoord )
{
	SurfComputeTextureCoordinate( m_SurfaceID, worldPos, texCoord.Base() );
}

void CBrushSurface::ComputeLightmapCoordinate( const Vector& worldPos, Vector2D& lightmapCoord )
{
	SurfComputeLightmapCoordinate( m_Ctx, m_SurfaceID, worldPos, lightmapCoord );
}


//-----------------------------------------------------------------------------
// Gets the vertex data for this surface
//-----------------------------------------------------------------------------
int  CBrushSurface::GetVertexCount() const
{
	if( !SurfaceHasPrims( m_SurfaceID ) )
	{
		// Create a temporary vertex array for the data...
		return MSurf_VertCount( m_SurfaceID );
	}
	else
	{
		// not implemented yet
		Assert(0);
		return 0;
	}
}

void CBrushSurface::GetVertexData( BrushVertex_t* pVerts )
{
	Assert( pVerts );

	if( !SurfaceHasPrims( m_SurfaceID ) )
	{
		// Fill in the vertex data
		BuildBrushModelVertexArray( host_state.worldbrush, m_SurfaceID, pVerts );
	}
	else
	{
		// not implemented yet
		Assert(0);
	}
}


//-----------------------------------------------------------------------------
// Activates fast z reject for displacements
//-----------------------------------------------------------------------------
void R_FastZRejectDisplacements( bool bEnable )
{
	s_bFastZRejectDisplacements = bEnable;
}


//-----------------------------------------------------------------------------
// Gets at the material properties for this surface
//-----------------------------------------------------------------------------
IMaterial* CBrushSurface::GetMaterial()
{
	return MSurf_TexInfo( m_SurfaceID )->material;
}

//-----------------------------------------------------------------------------
// Installs a client-side renderer for brush models
//-----------------------------------------------------------------------------
void R_InstallBrushRenderOverride( IBrushRenderer* pBrushRenderer )
{
	s_pBrushRenderOverride = pBrushRenderer;
}

//-----------------------------------------------------------------------------
// Here, we allow the client DLL to render brush surfaces however they'd like
// NOTE: This involves a vertex copy, so don't use this everywhere
//-----------------------------------------------------------------------------

bool Shader_DrawBrushSurfaceOverride( IMatRenderContext *pRenderContext, SurfaceHandle_t surfID, IClientEntity *baseentity )
{
	// Set the lightmap state
	Shader_SetChainLightmapState( pRenderContext, surfID );

	CBrushSurface brushSurface( surfID );
	return s_pBrushRenderOverride->RenderBrushModelSurface( baseentity, &brushSurface );
}



//-----------------------------------------------------------------------------
// Main method to draw brush surfaces
//-----------------------------------------------------------------------------
void Shader_BrushSurfaceOverride( IMatRenderContext *pRenderContext, SurfaceHandle_t surfID, model_t *model, IClientEntity *baseentity )
{
	Assert(s_pBrushRenderOverride);
	bool drawDecals = Shader_DrawBrushSurfaceOverride( pRenderContext, surfID, baseentity );

	// fixme: need to get "allowDecals" from the material
	//	if ( g_BrushProperties.allowDecals && pSurf->pdecals ) 
	if( SurfaceHasDecals( surfID ) && drawDecals )
	{
		DecalSurfaceAdd( surfID, BRUSHMODEL_DECAL_SORT_GROUP );
	}

	// Add overlay fragments to list.
	// FIXME: A little code support is necessary to get overlays working on brush models
//	OverlayMgr()->AddFragmentListToRenderList( MSurf_OverlayFragmentList( surfID ), false );

	// Add shadows too....
	ShadowDecalHandle_t decalHandle = MSurf_ShadowDecals( surfID );
	if (decalHandle != SHADOW_DECAL_HANDLE_INVALID)
	{
		g_pShadowMgr->AddShadowsOnSurfaceToRenderList( decalHandle );
	}
}


void R_Surface_LevelInit()
{
	g_BrushBatchRenderer.LevelInit();
}


void R_Surface_LevelShutdown()
{
	CWorldRenderList::PurgeAll();

#if defined(_PS3)
	for( int lp = 0; lp < MAX_CONCURRENT_BUILDVIEWS; lp++ )
	{
		g_Pool_PS3[ lp ].Purge();
	}
#endif
}

//-----------------------------------------------------------------------------
static void R_DrawBrushModel_Override( IMatRenderContext *pRenderContext, IClientEntity *baseentity, model_t *model )
{
	VPROF( "R_DrawOpaqueBrushModel_Override" );
	SurfaceHandle_t surfID = SurfaceHandleFromIndex( model->brush.firstmodelsurface, model->brush.pShared );
	for (int i=0 ; i<model->brush.nummodelsurfaces ; i++, surfID++)
	{
		Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );

		Shader_BrushSurfaceOverride( pRenderContext, surfID, model, baseentity );
	}
	// now draw debug for each drawn surface
	if ( g_ShaderDebug.anydebug )
	{
		CUtlVector<msurface2_t *> surfaceList;
		surfID = SurfaceHandleFromIndex( model->brush.firstmodelsurface, model->brush.pShared );
		for (int i=0 ; i<model->brush.nummodelsurfaces ; i++, surfID++)
		{
			surfaceList.AddToTail(surfID);
		}
		DrawDebugInformation( pRenderContext, surfaceList.Base(), surfaceList.Count() );
	}
}

int R_MarkDlightsOnBrushModel( model_t *model, IClientRenderable *pRenderable )
{
	int count = 0;
	if ( g_bActiveDlights )
	{
		extern int R_MarkLights (dlight_t *light, int bit, mnode_t *node);

		g_BrushToWorldMatrix.SetupMatrixOrgAngles( pRenderable->GetRenderOrigin(), pRenderable->GetRenderAngles() );
		Vector saveOrigin;

		for (int k=0 ; k<MAX_DLIGHTS ; k++)
		{
			if ((cl_dlights[k].die < GetBaseLocalClient().GetTime()) ||
				(!cl_dlights[k].IsRadiusGreaterThanZero()))
				continue;

			VectorCopy( cl_dlights[k].origin, saveOrigin );
			cl_dlights[k].origin = g_BrushToWorldMatrix.VMul4x3Transpose( saveOrigin );

			mnode_t *node = model->brush.pShared->nodes + model->brush.firstnode;
			if ( IsBoxIntersectingSphereExtents( node->m_vecCenter, node->m_vecHalfDiagonal, cl_dlights[k].origin, cl_dlights[k].GetRadius() ) )
			{
				count += R_MarkLights( &cl_dlights[k], 1<<k, node );
			}
			VectorCopy( saveOrigin, cl_dlights[k].origin );
		}
		if ( count )
		{
			model->flags |= MODELFLAG_HAS_DLIGHT;
		}
		g_BrushToWorldMatrix.Identity();
	}
	return count;
}


//-----------------------------------------------------------------------------
// Stuff to do right before and after brush model rendering
//-----------------------------------------------------------------------------
void Shader_BrushBegin( model_t *model, IClientEntity *baseentity /*=NULL*/ )
{
	// Clear out the render list of decals
	DecalSurfacesInit( true );

	// Clear out the render lists of shadows
	g_pShadowMgr->ClearShadowRenderList( );
}

void Shader_BrushEnd( IMatRenderContext *pRenderContext, VMatrix const* pBrushToWorld, model_t *model, bool bShadowDepth, IClientEntity *baseentity /* = NULL */ )
{
	if ( bShadowDepth )
		return;

	DecalSurfaceDraw( pRenderContext, BRUSHMODEL_DECAL_SORT_GROUP, r_blend );

	// draw the flashlight lighting for the decals on the brush.
	g_pShadowMgr->DrawFlashlightDecals( pRenderContext, BRUSHMODEL_DECAL_SORT_GROUP, false, r_blend );

	// Retire decals on opaque brushmodel surfaces
	R_DecalFlushDestroyList();

	// Draw all shadows on the brush
	g_pShadowMgr->RenderProjectedTextures( pRenderContext, pBrushToWorld );
}

CBrushModelTransform::CBrushModelTransform( const Vector &origin, const QAngle &angles, IMatRenderContext *pRenderContext )
{
	bool rotated = ( angles[0] || angles[1] || angles[2] );
	m_bIdentity = (origin == vec3_origin) && (!rotated);

	// Don't change state if we don't need to
	if (!m_bIdentity)
	{
		m_savedModelorg = modelorg;
		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PushMatrix();
		g_BrushToWorldMatrix.SetupMatrixOrgAngles( origin, angles );
		pRenderContext->LoadMatrix( g_BrushToWorldMatrix );
		modelorg = g_BrushToWorldMatrix.VMul4x3Transpose(g_EngineRenderer->ViewOrigin());
	}
}

CBrushModelTransform::CBrushModelTransform( const matrix3x4a_t &matrix, IMatRenderContext *pRenderContext )
{
	m_bIdentity = MatrixIsIdentity( matrix );

	// Don't change state if we don't need to
	if (!m_bIdentity)
	{
		m_savedModelorg = modelorg;
		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PushMatrix();
		g_BrushToWorldMatrix.Init( matrix );
		pRenderContext->LoadMatrix( g_BrushToWorldMatrix );
		modelorg = g_BrushToWorldMatrix.VMul4x3Transpose(g_EngineRenderer->ViewOrigin());
	}
}

CBrushModelTransform::~CBrushModelTransform()
{
	if ( !m_bIdentity )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PopMatrix();
		g_BrushToWorldMatrix.Identity();
		modelorg = m_savedModelorg;
	}
}

VMatrix *CBrushModelTransform::GetNonIdentityMatrix()
{
	return m_bIdentity ? NULL : &g_BrushToWorldMatrix;
}


//-----------------------------------------------------------------------------
// Draws debug info for a brush model
//-----------------------------------------------------------------------------
void DrawDebugInformation( IMatRenderContext *pRenderContext, const matrix3x4a_t &brushToWorld, SurfaceHandle_t *pList, int listCount )
{
	CBrushModelTransform transform( brushToWorld, pRenderContext );
	DrawDebugInformation( pRenderContext, pList, listCount );
}


//-----------------------------------------------------------------------------
// Purpose: Draws a brush model using the global shader/surfaceVisitor
// Input  : *e - entity to draw
// Output : void R_DrawBrushModel
//-----------------------------------------------------------------------------
void R_DrawBrushModel( IClientEntity *baseentity, model_t *model, 
					   const matrix3x4a_t& brushModelToWorld, ERenderDepthMode_t DepthMode, bool bDrawOpaque, bool bDrawTranslucent )
{
	VPROF( "R_DrawBrushModel" );

#ifdef USE_CONVARS
	if ( !r_drawbrushmodels.GetInt() )
	{
		return;
	}
	bool bWireframe = false;
	if ( r_drawbrushmodels.GetInt() == 2 )
	{
		// save and override
		bWireframe = g_ShaderDebug.wireframe;
		g_ShaderDebug.wireframe = true;
		g_ShaderDebug.anydebug = true;
	}
#endif

	CMatRenderContextPtr pRenderContext( materials );
	CBrushModelTransform brushTransform( brushModelToWorld, pRenderContext );

	Assert(model->brush.firstmodelsurface != 0);

	// Draw the puppy...
	Shader_BrushBegin( model, baseentity );
	if ( s_pBrushRenderOverride )
	{
		R_DrawBrushModel_Override( pRenderContext, baseentity, model );
	}
	else
	{
		if ( model->flags & MODELFLAG_TRANSLUCENT )
		{
			if ( DepthMode == DEPTH_MODE_NORMAL )
			{
				g_BrushBatchRenderer.DrawTranslucentBrushModel( pRenderContext, baseentity, model, DEPTH_MODE_NORMAL, bDrawOpaque, bDrawTranslucent );
			}
		}
		else if ( bDrawOpaque )
		{
			g_BrushBatchRenderer.DrawOpaqueBrushModel( pRenderContext, baseentity, model, DepthMode );
		}
	}

	Shader_BrushEnd( pRenderContext, brushTransform.GetNonIdentityMatrix(), model, DepthMode != DEPTH_MODE_NORMAL, baseentity );

#ifdef USE_CONVARS
	if ( r_drawbrushmodels.GetInt() == 2 )
	{
		// restore
		g_ShaderDebug.wireframe = bWireframe;
		g_ShaderDebug.TestAnyDebug();

	}
#endif
}


void R_DrawBrushModel( IClientEntity *baseentity, model_t *model, 
					   const Vector& origin, const QAngle& angles, ERenderDepthMode_t DepthMode, bool bDrawOpaque, bool bDrawTranslucent )
{
	matrix3x4a_t mat;
	AngleMatrix( angles, origin, mat );
	R_DrawBrushModel( baseentity, model, mat, DepthMode, bDrawOpaque, bDrawTranslucent );
}


//-----------------------------------------------------------------------------
// Purpose: Draws a brush model shadow for render-to-texture shadows
//-----------------------------------------------------------------------------
void R_DrawBrushModelShadow( IClientRenderable *pRenderable )
{
	if( !r_drawbrushmodels.GetInt() )
		return;

	model_t *model = (model_t *)pRenderable->GetModel();
	const Vector& origin = pRenderable->GetRenderOrigin();
	QAngle const& angles = pRenderable->GetRenderAngles();

	CMatRenderContextPtr pRenderContext( materials );
	CBrushModelTransform brushTransform( origin, angles, pRenderContext );
	g_BrushBatchRenderer.DrawBrushModelShadow( pRenderContext, model, pRenderable );
}


void R_DrawIdentityBrushModel( IWorldRenderList *pRenderListIn, model_t *model )
{
	if ( !model )
		return;

	CWorldRenderList *pRenderList = assert_cast<CWorldRenderList *>(pRenderListIn);

	SurfaceHandle_t surfID = SurfaceHandleFromIndex( model->brush.firstmodelsurface, model->brush.pShared );
	
	for (int j=0 ; j<model->brush.nummodelsurfaces ; ++j, surfID++)
	{
		Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );

		// FIXME: Can't insert translucent stuff into the list
		// of translucent surfaces because we don't know what leaf
		// we're in. At the moment, the client doesn't add translucent
		// brushes to the identity brush model list
//		Assert ( (psurf->flags & SURFDRAW_TRANS ) == 0 );

		// OPTIMIZE: Backface cull these guys?!?!?
		if ( MSurf_Flags( surfID ) & SURFDRAW_TRANS)
//		if ( psurf->texinfo->material->IsTranslucent() )
		{
			Shader_TranslucentWorldSurface( pRenderList, surfID );
		}
		else
		{
			Shader_WorldSurface( pRenderList, surfID );
		}
	}
}

//-----------------------------------------------------------------------------
// Draws arrays of brush models
//-----------------------------------------------------------------------------
void R_DrawBrushModelArray( IMatRenderContext* pRenderContext, int nCount, 
						   const BrushArrayInstanceData_t *pInstanceData, int nModelTypeFlags )
{
	// For now, we don't support translucency, as we can't re-order rendering
	Assert( ( nModelTypeFlags & STUDIO_TRANSPARENCY ) == 0 );

	if ( ( nModelTypeFlags & STUDIO_SHADOWDEPTHTEXTURE ) || ( nModelTypeFlags & STUDIO_SSAODEPTHTEXTURE ) )
	{
		g_BrushBatchRenderer.DrawBrushModelShadowArray( pRenderContext, nCount, pInstanceData, nModelTypeFlags );
		return;
	}

	bool bDrawOpaque = false, bDrawTranslucent = false;
	if ( nModelTypeFlags & STUDIO_TWOPASS )
	{
		bDrawTranslucent = ( nModelTypeFlags & STUDIO_TRANSPARENCY ) != 0;
		bDrawOpaque = !bDrawTranslucent;
	}
	else
	{
		// Can't draw both opaque + translucent
		Assert( 0 );
	}

	g_BrushBatchRenderer.DrawBrushModelArray( pRenderContext, nCount, pInstanceData );
}

#endif

//-----------------------------------------------------------------------------
// Converts leaf pointer to index
//-----------------------------------------------------------------------------
inline int LeafToIndex( mleaf_t* pLeaf )
{
	return pLeaf - host_state.worldbrush->leafs;
}


//-----------------------------------------------------------------------------
// Structures to help out with enumeration
//-----------------------------------------------------------------------------
enum
{
	// MUST be in upper 16 bits! Lower 16 are used to filter against mnode->flags
	ENUM_SPHERE_TEST_X = 0x10000000,
	ENUM_SPHERE_TEST_Y = 0x20000000,
	ENUM_SPHERE_TEST_Z = 0x40000000,

	ENUM_SPHERE_TEST_ALL = 0x70000000,
};

struct EnumLeafSphereInfo_t
{
	Vector m_vecCenter;
	float m_flRadius;
	Vector m_vecBoxCenter;
	Vector m_vecBoxHalfDiagonal;
	ISpatialLeafEnumerator *m_pIterator;
	intp m_nContext;
};


// NOTE: These leaf list routines only return non-solid leaves!  Only use them for rendering-related queries!
#ifdef _X360
struct ListLeafBoxInfo_t
{
	VectorAligned m_vecBoxMax;
	VectorAligned m_vecBoxMin;
	VectorAligned m_vecBoxCenter;
	VectorAligned m_vecBoxHalfDiagonal;
};

static fltx4 AlignThatVector(const Vector &vc)
{
	fltx4 out = __loadunalignedvector(vc.Base());

	/*
	out.x = vc.x;
	out.y = vc.y;
	out.z = vc.z;
	*/

	// squelch the w component 
	return __vrlimi( out, __vzero(), 1, 0 );
}

static int ListLeafsInBox( mnode_t * RESTRICT node, ListLeafBoxInfo_t * RESTRICT pInfo, unsigned short * RESTRICT pList, int listMax )
{
	int leafCount = 0;
	const int NODELIST_MAX = 2048;
	mnode_t *nodeList[NODELIST_MAX];
	int nodeReadIndex = 0;
	int nodeWriteIndex = 0;

	while (1)
	{
		// no polygons in solid nodes (don't report these leaves either)
		if (node->contents >= 0)
		{
			if (node->contents != CONTENTS_SOLID)
			{
				// if a leaf node, report it to the iterator...
				if ( leafCount < listMax )
				{
					pList[leafCount++] = LeafToIndex( (mleaf_t *)node );
				}
			}
			if ( nodeReadIndex == nodeWriteIndex )
				return leafCount;
			node = nodeList[nodeReadIndex];
			nodeReadIndex = (nodeReadIndex+1) & (NODELIST_MAX-1);
		}
		else
		{
			// speculatively get the children into the cache
			PREFETCH_128(node->children[0],0);
			PREFETCH_128(node->children[1],0);

			// constructing these here prevents LHS if we spill.
			// it's not quite a quick enough operation to do extemporaneously.
			fltx4 infoBoxCenter = LoadAlignedSIMD(pInfo->m_vecBoxCenter);
			fltx4 infoBoxHalfDiagonal = LoadAlignedSIMD(pInfo->m_vecBoxHalfDiagonal);

			Assert(IsBoxIntersectingBoxExtents(AlignThatVector(node->m_vecCenter), AlignThatVector(node->m_vecHalfDiagonal), 
				LoadAlignedSIMD(pInfo->m_vecBoxCenter), LoadAlignedSIMD(pInfo->m_vecBoxHalfDiagonal)) ==
				IsBoxIntersectingBoxExtents((node->m_vecCenter), node->m_vecHalfDiagonal,
				pInfo->m_vecBoxCenter, pInfo->m_vecBoxHalfDiagonal));


			// rough cull...
			if (IsBoxIntersectingBoxExtents(LoadAlignedSIMD(node->m_vecCenter), LoadAlignedSIMD(node->m_vecHalfDiagonal), 
				infoBoxCenter, infoBoxHalfDiagonal))
			{
				// Does the node plane split the box?
				// find which side of the node we are on
				cplane_t* RESTRICT plane = node->plane;
				if ( plane->type <= PLANE_Z )
				{
					if (pInfo->m_vecBoxMax[plane->type] <= plane->dist)
					{
						node = node->children[1];
					}
					else if (pInfo->m_vecBoxMin[plane->type] >= plane->dist)
					{
						node = node->children[0];
					}
					else
					{
						// Here the box is split by the node
						nodeList[nodeWriteIndex] = node->children[0];
						nodeWriteIndex = (nodeWriteIndex+1) & (NODELIST_MAX-1);
						// check for overflow of the ring buffer
						Assert(nodeWriteIndex != nodeReadIndex);
						node = node->children[1];
					}
				}
				else
				{
					// take advantage of high throughput/high latency
					fltx4 planeNormal = LoadUnaligned3SIMD( plane->normal.Base() );
					fltx4 vecBoxMin = LoadAlignedSIMD(pInfo->m_vecBoxMin);
					fltx4 vecBoxMax = LoadAlignedSIMD(pInfo->m_vecBoxMax);
					fltx4 cornermin, cornermax;
					// by now planeNormal is ready...
					fltx4 control = XMVectorGreaterOrEqual( planeNormal, __vzero() );
					// now control[i] = planeNormal[i] > 0 ? 0xFF : 0x00
					cornermin = XMVectorSelect( vecBoxMax, vecBoxMin, control); // cornermin[i] = control[i] ? vecBoxMin[i] : vecBoxMax[i]
					cornermax = XMVectorSelect( vecBoxMin, vecBoxMax, control);

					// compute dot products
					fltx4 dotCornerMax = __vmsum3fp(planeNormal, cornermax); // vsumfp ignores w component
					fltx4 dotCornerMin = __vmsum3fp(planeNormal, cornermin);
					fltx4 vPlaneDist = ReplicateX4(plane->dist);
					UINT conditionRegister;
					XMVectorGreaterR(&conditionRegister,vPlaneDist,dotCornerMax);
					if (XMComparisonAllTrue(conditionRegister)) // plane->normal . cornermax <= plane->dist
					{
						node = node->children[1];
					}
					else
					{
						XMVectorGreaterOrEqualR(&conditionRegister,dotCornerMin,vPlaneDist);
						if ( XMComparisonAllTrue(conditionRegister) )
						{
							node = node->children[0];
						}
						else
						{
							// Here the box is split by the node
							nodeList[nodeWriteIndex] = node->children[0];
							nodeWriteIndex = (nodeWriteIndex+1) & (NODELIST_MAX-1);
							// check for overflow of the ring buffer
							Assert(nodeWriteIndex != nodeReadIndex);
							node = node->children[1];
						}
					}
				}
			}
			else
			{
				if ( nodeReadIndex == nodeWriteIndex )
					return leafCount;
				node = nodeList[nodeReadIndex];
				nodeReadIndex = (nodeReadIndex+1) & (NODELIST_MAX-1);
			}
		}
	}
}
#else
static int ListLeafsInBox( mnode_t * RESTRICT node, const Vector &center, const Vector &extents, unsigned short * RESTRICT pList, int listMax )
{
	int leafCount = 0;
	const int NODELIST_MAX = 1024;
	mnode_t *nodeList[NODELIST_MAX];
	int nodeReadIndex = 0;
	int nodeWriteIndex = 0;

	while (1)
	{
		if (node->contents >= 0)
		{
			if (node->contents != CONTENTS_SOLID)
			{
				// if a leaf node, report it to the iterator...
				if ( leafCount < listMax )
				{
					pList[leafCount++] = LeafToIndex( (mleaf_t *)node );
				}
			}
			if ( nodeReadIndex == nodeWriteIndex )
				return leafCount;
			node = nodeList[nodeReadIndex];
			nodeReadIndex = (nodeReadIndex+1) & (NODELIST_MAX-1);
		}
		else
		{
			const cplane_t *plane = node->plane;
			//		s = BoxOnPlaneSide (leaf_mins, leaf_maxs, plane);
			//		s = BOX_ON_PLANE_SIDE(*leaf_mins, *leaf_maxs, plane);
			float d0 = DotProduct( plane->normal, center ) - plane->dist;
			float d1 = DotProductAbs( plane->normal, extents );
			if (d0 >= d1)
				node = node->children[0];
			else if (d0 < -d1)
				node = node->children[1];
			else
			{	// go down both
				nodeList[nodeWriteIndex] = node->children[0];
				nodeWriteIndex = (nodeWriteIndex+1) & (NODELIST_MAX-1);
				// check for overflow of the ring buffer
				Assert(nodeWriteIndex != nodeReadIndex);
				node = node->children[1];
			}
		}
	}
}

#endif

//-----------------------------------------------------------------------------
// Returns all leaves that lie within a spherical volume
//-----------------------------------------------------------------------------
template<bool bCheckFlags> bool EnumerateLeafInSphere_R( mnode_t *node, EnumLeafSphereInfo_t& info, int nTestFlags )
{
	while (true)
	{
		// no polygons in solid nodes (don't report these leaves either)
		if (node->contents == CONTENTS_SOLID)
			return true;		// solid

		if (node->contents >= 0)
		{
			// leaf cull...
			// NOTE: using nTestFlags here means that we may be passing in some 
			// leaves that don't actually intersect the sphere, but instead intersect
			// the box that surrounds the sphere.
			if (nTestFlags)
			{
				if (!IsBoxIntersectingSphereExtents (node->m_vecCenter, node->m_vecHalfDiagonal, info.m_vecCenter, info.m_flRadius))
					return true;
			}

			// if a leaf node, report it to the iterator...
			return info.m_pIterator->EnumerateLeaf( LeafToIndex( (mleaf_t *)node ), info.m_nContext ); 
		}
		else if (nTestFlags)
		{
			if (node->contents == -1)
			{
				if ( bCheckFlags )
				{
					if ( ( node->flags & ( nTestFlags ) ) == 0 ) // this is a WORD and
						return true;
				}
				// faster cull...
				if (nTestFlags & ENUM_SPHERE_TEST_X)
				{
					float flDelta = FloatMakePositive( node->m_vecCenter.x - info.m_vecBoxCenter.x );
					float flSize = node->m_vecHalfDiagonal.x + info.m_vecBoxHalfDiagonal.x;
					if ( flDelta > flSize )
						return true;

					// This checks for the node being completely inside the box...
					if ( flDelta + node->m_vecHalfDiagonal.x < info.m_vecBoxHalfDiagonal.x )
						nTestFlags &= ~ENUM_SPHERE_TEST_X;
				}

				if (nTestFlags & ENUM_SPHERE_TEST_Y)
				{
					float flDelta = FloatMakePositive( node->m_vecCenter.y - info.m_vecBoxCenter.y );
					float flSize = node->m_vecHalfDiagonal.y + info.m_vecBoxHalfDiagonal.y;
					if ( flDelta > flSize )
						return true;

					// This checks for the node being completely inside the box...
					if ( flDelta + node->m_vecHalfDiagonal.y < info.m_vecBoxHalfDiagonal.y )
						nTestFlags &= ~ENUM_SPHERE_TEST_Y;
				}

				if (nTestFlags & ENUM_SPHERE_TEST_Z)
				{
					float flDelta = FloatMakePositive( node->m_vecCenter.z - info.m_vecBoxCenter.z );
					float flSize = node->m_vecHalfDiagonal.z + info.m_vecBoxHalfDiagonal.z;
					if ( flDelta > flSize )
						return true;
														   
					if ( flDelta + node->m_vecHalfDiagonal.z < info.m_vecBoxHalfDiagonal.z )
						nTestFlags &= ~ENUM_SPHERE_TEST_Z;
				}
			}
			else if (node->contents == -2)
			{
				// If the box is too small to bother with testing, then blat out the flags
				nTestFlags &= ~( ENUM_SPHERE_TEST_ALL );
			}
		}

		// Does the node plane split the sphere?
		// find which side of the node we are on
		float flNormalDotCenter;
		cplane_t* plane = node->plane;
		if ( plane->type <= PLANE_Z )
		{
			flNormalDotCenter = info.m_vecCenter[plane->type];
		}
		else
		{
			// Here, we've got a plane which is not axis aligned, so we gotta do more work
			flNormalDotCenter = DotProduct( plane->normal, info.m_vecCenter );
		}

		if (flNormalDotCenter + info.m_flRadius <= plane->dist)
		{
			node = node->children[1];
		}
		else if (flNormalDotCenter - info.m_flRadius >= plane->dist)
		{
			node = node->children[0];
		}
		else
		{
			// Here the box is split by the node
			if (!EnumerateLeafInSphere_R<bCheckFlags>( node->children[0], info, nTestFlags ))
				return false;

			node = node->children[1];
		}
	}
}


//-----------------------------------------------------------------------------
// Enumerate leaves along a non-extruded ray
//-----------------------------------------------------------------------------

static bool EnumerateLeavesAlongRay_R( mnode_t *node, Ray_t const& ray, 
	float start, float end, ISpatialLeafEnumerator* pEnum, intp context )
{
	// no polygons in solid nodes (don't report these leaves either)
	if (node->contents == CONTENTS_SOLID)
		return true;		// solid, keep recursing

	// didn't hit anything
	if (node->contents >= 0)
	{
		// if a leaf node, report it to the iterator...
		return pEnum->EnumerateLeaf( LeafToIndex( (mleaf_t *)node ), context ); 
	}
	
	// Determine which side of the node plane our points are on
	cplane_t* plane = node->plane;

	float startDotN,deltaDotN;
	if (plane->type <= PLANE_Z)
	{
		startDotN = ray.m_Start[plane->type];
		deltaDotN = ray.m_Delta[plane->type];
	}
	else
	{
		startDotN = DotProduct( ray.m_Start, plane->normal );
		deltaDotN = DotProduct( ray.m_Delta, plane->normal );
	}

	float front = startDotN + start * deltaDotN - plane->dist;
	float back = startDotN + end * deltaDotN - plane->dist;

	int side = front < 0;
	
	// If they're both on the same side of the plane, don't bother to split
	// just check the appropriate child
	if ( (back < 0) == side )
	{
		return EnumerateLeavesAlongRay_R (node->children[side], ray, start, end, pEnum, context );
	}
	
	// calculate mid point
	float frac = front / (front - back);
	float mid = start * (1.0f - frac) + end * frac;
	
	// go down front side	
	bool ok = EnumerateLeavesAlongRay_R (node->children[side], ray, start, mid, pEnum, context );
	if (!ok)
		return ok;

	// go down back side
	return EnumerateLeavesAlongRay_R (node->children[!side], ray, mid, end, pEnum, context );
}


//-----------------------------------------------------------------------------
// Enumerate leaves along a non-extruded ray
//-----------------------------------------------------------------------------

static bool EnumerateLeavesAlongExtrudedRay_R( mnode_t *node, Ray_t const& ray, 
	float start, float end, ISpatialLeafEnumerator* pEnum, intp context )
{
	// no polygons in solid nodes (don't report these leaves either)
	if (node->contents == CONTENTS_SOLID)
		return true;		// solid, keep recursing

	// didn't hit anything
	if (node->contents >= 0)
	{
		// if a leaf node, report it to the iterator...
		return pEnum->EnumerateLeaf( LeafToIndex( (mleaf_t *)node ), context ); 
	}
	
	// Determine which side of the node plane our points are on
	cplane_t* plane = node->plane;

	//
	float t1, t2, offset;
	float startDotN,deltaDotN;
	if (plane->type <= PLANE_Z)
	{
		startDotN = ray.m_Start[plane->type];
		deltaDotN = ray.m_Delta[plane->type];
		offset = ray.m_Extents[plane->type] + DIST_EPSILON;
	}
	else
	{
		startDotN = DotProduct( ray.m_Start, plane->normal );
		deltaDotN = DotProduct( ray.m_Delta, plane->normal );
		offset = fabs(ray.m_Extents[0]*plane->normal[0]) +
				fabs(ray.m_Extents[1]*plane->normal[1]) +
				fabs(ray.m_Extents[2]*plane->normal[2]) + DIST_EPSILON;
	}
	t1 = startDotN + start * deltaDotN - plane->dist;
	t2 = startDotN + end * deltaDotN - plane->dist;

	// If they're both on the same side of the plane (further than the trace
	// extents), don't bother to split, just check the appropriate child
    if (t1 > offset && t2 > offset )
//	if (t1 >= offset && t2 >= offset)
	{
		return EnumerateLeavesAlongExtrudedRay_R( node->children[0], ray,
			start, end, pEnum, context );
	}
	if (t1 < -offset && t2 < -offset)
	{
		return EnumerateLeavesAlongExtrudedRay_R( node->children[1], ray,
			start, end, pEnum, context );
	}

	// For the segment of the line that we are going to use
	// to test against the back side of the plane, we're going
	// to use the part that goes from start to plane + extent
	// (which causes it to extend somewhat into the front halfspace,
	// since plane + extent is in the front halfspace).
	// Similarly, front the segment which tests against the front side,
	// we use the entire front side part of the ray + a portion of the ray that
	// extends by -extents into the back side.

	if (fabs(t1-t2) < DIST_EPSILON)
	{
		// Parallel case, send entire ray to both children...
		bool ret = EnumerateLeavesAlongExtrudedRay_R( node->children[0], 
			ray, start, end, pEnum, context );
		if (!ret)
			return false;
		return EnumerateLeavesAlongExtrudedRay_R( node->children[1],
			ray, start, end, pEnum, context );
	}
	
	// Compute the two fractions...
	// We need one at plane + extent and another at plane - extent.
	// put the crosspoint DIST_EPSILON pixels on the near side
	float idist, frac2, frac;
	int side;
	if (t1 < t2)
	{
		idist = 1.0/(t1-t2);
		side = 1;
		frac2 = (t1 + offset) * idist;
		frac = (t1 - offset) * idist;
	}
	else if (t1 > t2)
	{
		idist = 1.0/(t1-t2);
		side = 0;
		frac2 = (t1 - offset) * idist;
		frac = (t1 + offset) * idist;
	}
	else
	{
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// move up to the node
	frac = clamp( frac, 0, 1 );
	float midf = start + (end - start)*frac;
	bool ret = EnumerateLeavesAlongExtrudedRay_R( node->children[side], ray, start, midf, pEnum, context );
	if (!ret)
		return ret;

	// go past the node
	frac2 = clamp( frac2, 0, 1 );
	midf = start + (end - start)*frac2;
	return EnumerateLeavesAlongExtrudedRay_R( node->children[!side], ray, midf, end, pEnum, context );
}


//-----------------------------------------------------------------------------
//
// Helper class to iterate over leaves
//
//-----------------------------------------------------------------------------

class CEngineBSPTree : public IEngineSpatialQuery
{
public:
	// Returns the number of leaves
	int LeafCount() const;

	// Enumerates the leaves along a ray, box, etc.
	bool EnumerateLeavesAtPoint( const Vector& pt, ISpatialLeafEnumerator* pEnum, intp context );
	bool EnumerateLeavesInBox( const Vector& mins, const Vector& maxs, ISpatialLeafEnumerator* pEnum, intp context );
	bool EnumerateLeavesInSphere( const Vector& center, float radius, ISpatialLeafEnumerator* pEnum, intp context );
	bool EnumerateLeavesAlongRay( Ray_t const& ray, ISpatialLeafEnumerator* pEnum, intp context );
	bool EnumerateLeavesInSphereWithFlagSet( const Vector& center, float radius, ISpatialLeafEnumerator* pEnum, intp context, int nFlags );

	int ListLeavesInBox( const Vector& mins, const Vector& maxs, unsigned short *pList, int listMax );
	int ListLeavesInSphereWithFlagSet( int *pLeafsInSphere, const Vector& vecCenter, float flRadius, int nLeafCount, const uint16 *pLeafs, int nLeafStride, int nFlagsCheck );
};

//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------

static CEngineBSPTree s_ToolBSPTree;
IEngineSpatialQuery* g_pToolBSPTree = &s_ToolBSPTree;


//-----------------------------------------------------------------------------
// Returns the number of leaves
//-----------------------------------------------------------------------------

int CEngineBSPTree::LeafCount() const
{
	return host_state.worldbrush->numleafs;
}

//-----------------------------------------------------------------------------
// Enumerates the leaves at a point
//-----------------------------------------------------------------------------

bool CEngineBSPTree::EnumerateLeavesAtPoint( const Vector& pt, 
									ISpatialLeafEnumerator* pEnum, intp context )
{
	int leaf = CM_PointLeafnum( pt );
	return pEnum->EnumerateLeaf( leaf, context );
}


int CEngineBSPTree::ListLeavesInBox( const Vector& mins, const Vector& maxs, unsigned short *pList, int listMax )
{
#ifdef _X360
	ListLeafBoxInfo_t info;
	VectorAdd( mins, maxs, info.m_vecBoxCenter );
	info.m_vecBoxCenter *= 0.5f;
	VectorSubtract( maxs, info.m_vecBoxCenter, info.m_vecBoxHalfDiagonal );
	info.m_vecBoxMax = maxs;
	info.m_vecBoxMin = mins;
	return ListLeafsInBox( host_state.worldbrush->nodes, &info, pList, listMax );
#else
	Vector center, extents;
	VectorAdd(mins, maxs, center );
	center *= 0.5f;
	VectorSubtract(maxs, center, extents);
	return ListLeafsInBox( host_state.worldbrush->nodes, center, extents, pList, listMax );
#endif
}

int CEngineBSPTree::ListLeavesInSphereWithFlagSet( int *pLeafsInSphere, const Vector& vecCenter, float flRadius, int nLeafCount, const uint16 *pLeafs, int nLeafStride, int nFlagsCheck )
{
	int nLeavesFound = 0;
	const uint16 *pLeaf = pLeafs;
	for ( int i = 0; i < nLeafCount; ++i, pLeaf = (const uint16*)( (const uint8*)pLeaf + nLeafStride ) )
	{
		mleaf_t& leaf = host_state.worldbrush->leafs[ *pLeaf ];

		if ( ( leaf.flags & nFlagsCheck ) == 0 ) 
			continue;

		if ( !IsBoxIntersectingSphereExtents( leaf.m_vecCenter, leaf.m_vecHalfDiagonal, vecCenter, flRadius ) )
			continue;

		pLeafsInSphere[nLeavesFound++] = i;
	}
	return nLeavesFound;
}

bool CEngineBSPTree::EnumerateLeavesInBox( const Vector& mins, const Vector& maxs, 
									ISpatialLeafEnumerator* pEnum, intp context )
{
	if ( !host_state.worldmodel )
		return false;

	unsigned short list[1024];
	int count = ListLeavesInBox( mins, maxs, list, ARRAYSIZE(list) );
	for ( int i = 0; i < count; i++ )
	{
		if ( !pEnum->EnumerateLeaf(list[i], context) )
			break;
	}
	return true;
}


bool CEngineBSPTree::EnumerateLeavesInSphere( const Vector& center, float radius, 
									ISpatialLeafEnumerator* pEnum, intp context )
{
	EnumLeafSphereInfo_t info;
	info.m_vecCenter = center;
	info.m_flRadius = radius;
	info.m_pIterator = pEnum;
	info.m_nContext = context;
	info.m_vecBoxCenter = center;
	info.m_vecBoxHalfDiagonal.Init( radius, radius, radius );

	return EnumerateLeafInSphere_R<false>( host_state.worldbrush->nodes, info, ENUM_SPHERE_TEST_ALL );
}

bool CEngineBSPTree::EnumerateLeavesInSphereWithFlagSet( const Vector& center, float radius, 
														 ISpatialLeafEnumerator* pEnum,
														 intp context, int nFlags )
{
	EnumLeafSphereInfo_t info;
	info.m_vecCenter = center;
	info.m_flRadius = radius;
	info.m_pIterator = pEnum;
	info.m_nContext = context;
	info.m_vecBoxCenter = center;
	info.m_vecBoxHalfDiagonal.Init( radius, radius, radius );

	return EnumerateLeafInSphere_R<true>( 
		host_state.worldbrush->nodes, info, nFlags | ENUM_SPHERE_TEST_ALL );
}

bool CEngineBSPTree::EnumerateLeavesAlongRay( Ray_t const& ray, ISpatialLeafEnumerator* pEnum, intp context )
{
	if (!ray.m_IsSwept)
	{
		Vector mins, maxs;
		VectorAdd( ray.m_Start, ray.m_Extents, maxs );
		VectorSubtract( ray.m_Start, ray.m_Extents, mins );

		return EnumerateLeavesInBox( mins, maxs, pEnum, context );
	}

	Vector end;
	VectorAdd( ray.m_Start, ray.m_Delta, end );

	if ( ray.m_IsRay )
	{
		return EnumerateLeavesAlongRay_R( host_state.worldbrush->nodes, ray, 0.0f, 1.0f, pEnum, context );
	}
	else
	{
		return EnumerateLeavesAlongExtrudedRay_R( host_state.worldbrush->nodes, ray, 0.0f, 1.0f, pEnum, context );
	}
}

