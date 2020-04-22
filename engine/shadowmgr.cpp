//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//


#include "render_pch.h"
#include "shadowmgr.h"
#include "utllinkedlist.h"
#include "utlvector.h"
#include "interface.h"
#include "mathlib/vmatrix.h"
#include "bsptreedata.h"
#include "materialsystem/itexture.h"
#include "filesystem.h"
#include "utlbidirectionalset.h"
#include "l_studio.h"
#include "shaderapi/ishaderapi.h"
#include "istudiorender.h"
#include "engine/ivmodelrender.h"
#include "collisionutils.h"
#include "debugoverlay.h"
#include "tier0/vprof.h"
#include "disp.h"
#include "gl_rmain.h"
#include "MaterialBuckets.h"
#include "r_decal.h"
#include "cmodel_engine.h"
#include "iclientrenderable.h"
#include "cdll_engine_int.h"
#include "sys_dll.h"
#include "render.h"
#include "vstdlib/jobthread.h"
#include "tier1/utlstack.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Shadow-related functionality exported by the engine
//
// We have two shadow-related caches in this system
//	1) A surface cache. We keep track of which surfaces the shadows can 
//		potentially hit. The computation of the surface cache should be
//		as fast as possible
//	2) A surface vertex cache. Once we know what surfaces the shadow
//		hits, we caompute the actual polygons using a clip. This is only
//		useful for shadows that we know don't change too frequently, so
//		we pass in a flag when making the shadow to indicate whether the
//		vertex cache should be used or not. The assumption is that the client
//		of this system should know whether the shadows are always changing or not
//
//	The first cache is generated when the shadow is initially projected, and
//  the second cache is generated when the surfaces are actually being rendered.
//
// For rendering, I assign a sort order ID to all materials used by shadow
// decals. The sort order serves the identical purpose to the material's EnumID
// but I remap those IDs so I can keep a small list of decals to render with
// that enum ID (the other option would be to allocate an array with a number
// of elements == to the number of material enumeration IDs, which is pretty large).
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// forward decarations
//-----------------------------------------------------------------------------
extern int r_surfacevisframe;


#define SHADOW_BACKFACE_EPSILON	0.01f


// Max number of vertices per shadow decal
enum
{
	SHADOW_VERTEX_SMALL_CACHE_COUNT = 8,
	SHADOW_VERTEX_LARGE_CACHE_COUNT = 32,
	MAX_CLIP_PLANE_COUNT = 4,
	SURFACE_BOUNDS_CACHE_COUNT = 1024,
	//=============================================================================
	// HPE_BEGIN:
	// [smessick] Cache size for the shadow decals. This used to be on the stack.
	//=============================================================================
	SHADOW_DECAL_CACHE_COUNT = 16*1024,
	MAX_SHADOW_DECAL_CACHE_COUNT = 64*1024,
	//=============================================================================
	// HPE_END
	//=============================================================================
};


//-----------------------------------------------------------------------------
// ConVars (must be defined before CShadowMgr is instanced!)
//-----------------------------------------------------------------------------
ConVar r_shadows("r_shadows", "1");
ConVar r_shadows_gamecontrol("r_shadows_gamecontrol", "-1", FCVAR_CHEAT );	// Shadow override controlled by game entities (shadow_controller)
static ConVar r_shadowwireframe("r_shadowwireframe", "0", FCVAR_CHEAT );
static ConVar r_shadowids("r_shadowids", "0", FCVAR_CHEAT );
static ConVar r_flashlightdrawsweptbbox( "r_flashlightdrawsweptbbox", "0" );
static ConVar r_flashlightnodraw( "r_flashlightnodraw", "0" );

static ConVar r_flashlightupdatedepth( "r_flashlightupdatedepth", "1" );
static ConVar r_flashlightdrawdepth( "r_flashlightdrawdepth", "0" );
static ConVar r_flashlightdrawdepthres( "r_flashlightdrawdepthres", "256" );
static ConVar r_flashlightrenderworld(  "r_flashlightrenderworld", "1" );
static ConVar r_flashlightrendermodels(  "r_flashlightrendermodels", "1" );
static ConVar r_flashlightrender( "r_flashlightrender", "1" );
static ConVar r_flashlightculldepth( "r_flashlightculldepth", "1" );

static ConVar r_threaded_shadow_clip( "r_threaded_shadow_clip", "0" );

static ConVar r_flashlight_always_cull_for_single_pass( "r_flashlight_always_cull_for_single_pass", "0" );

//-----------------------------------------------------------------------------
// Implementation of IShadowMgr
//-----------------------------------------------------------------------------
class CShadowMgr : public IShadowMgrInternal, ISpatialLeafEnumerator
{
public:
	// constructor
	CShadowMgr();

	// Methods inherited from IShadowMgr
	virtual ShadowHandle_t CreateShadow( IMaterial* pMaterial, IMaterial* pModelMaterial, void* pBindProxy, int creationFlags );
	virtual ShadowHandle_t CreateShadowEx( IMaterial* pMaterial, IMaterial* pModelMaterial, void* pBindProxy, int creationFlags, int nEntIndex );
	virtual void DestroyShadow( ShadowHandle_t handle );
	virtual void SetShadowMaterial( ShadowHandle_t handle, IMaterial* pMaterial, IMaterial* pModelMaterial, void* pBindProxy );
	virtual void EnableShadow( ShadowHandle_t handle, bool bEnable );
	virtual void ProjectFlashlight( ShadowHandle_t handle, const VMatrix& worldToShadow, int nLeafCount, const int *pLeafList );
	virtual void ProjectShadow( ShadowHandle_t handle, const Vector &origin,
		const Vector& projectionDir, const VMatrix& worldToShadow, const Vector2D& size,
		int nLeafCount, const int *pLeafList,
		float maxHeight, float falloffOffset, float falloffAmount, const Vector &vecCasterOrigin );
	virtual const Frustum_t &GetFlashlightFrustum( ShadowHandle_t handle );
	virtual const FlashlightState_t &GetFlashlightState( ShadowHandle_t handle );
	virtual int ProjectAndClipVertices( ShadowHandle_t handle, int count, 
		Vector** ppPosition, ShadowVertex_t*** ppOutVertex );
	virtual void AddShadowToBrushModel( ShadowHandle_t handle, model_t* pModel, 	
										const Vector& origin, const QAngle& angles );
	virtual void RemoveAllShadowsFromBrushModel( model_t* pModel );
	virtual void AddShadowToModel( ShadowHandle_t shadow, ModelInstanceHandle_t handle );
	virtual void RemoveAllShadowsFromModel( ModelInstanceHandle_t handle );
 	virtual const ShadowInfo_t& GetInfo( ShadowHandle_t handle );
	virtual void SetFlashlightRenderState( ShadowHandle_t handle );
	virtual int GetNumShadowsOnModel( ModelInstanceHandle_t instance );
	virtual int GetShadowsOnModel( ModelInstanceHandle_t instance, ShadowHandle_t* pShadowArray, bool bNormalShadows, bool bFlashlightShadows );
	virtual void FlashlightDrawCallback( ShadowDrawCallbackFn_t pCallback, void *pData );
	virtual void SetSinglePassFlashlightRenderState( ShadowHandle_t handle );
	virtual void PushSinglePassFlashlightStateEnabled( bool bEnable );
	virtual void PopSinglePassFlashlightStateEnabled( void );

	// Methods inherited from IShadowMgrInternal
	virtual void LevelInit( int nSurfCount );
	virtual void LevelShutdown();
	virtual void AddShadowsOnSurfaceToRenderList( ShadowDecalHandle_t decalHandle );
	virtual void ClearShadowRenderList();
	virtual void ComputeRenderInfo( ShadowDecalRenderInfo_t* pInfo, ShadowHandle_t handle ) const;
	virtual void SetModelShadowState( ModelInstanceHandle_t instance );
	virtual unsigned short InvalidShadowIndex( );

	// Methods of ISpatialLeafEnumerator
	virtual bool EnumerateLeaf( int leaf, intp context );

	// Sets the texture coordinate range for a shadow...
	virtual void SetShadowTexCoord( ShadowHandle_t handle, float x, float y, float w, float h );

	// Set extra clip planes related to shadows...
	// These are used to prevent pokethru and back-casting
	virtual void ClearExtraClipPlanes( ShadowHandle_t shadow );
	virtual void AddExtraClipPlane( ShadowHandle_t shadow, const Vector& normal, float dist );

	// Gets the first model associated with a shadow
	unsigned short& FirstModelInShadow( ShadowHandle_t h ) { return m_Shadows[h].m_FirstModel; }
	
	// Set the darkness falloff bias
	virtual void SetFalloffBias( ShadowHandle_t shadow, unsigned char ucBias );

	// Set the number of world material buckets.  This should happen exactly once per level load.
	virtual void SetNumWorldMaterialBuckets( int numMaterialSortBins );

	// Update the state for a flashlight.
	virtual void UpdateFlashlightState( ShadowHandle_t shadowHandle, const FlashlightState_t &lightState );

	virtual void DrawFlashlightDecals( IMatRenderContext *pRenderContext, int sortGroup, bool bDoMasking, float flFade );
	virtual void DrawFlashlightDecalsOnSurfaceList( IMatRenderContext *pRenderContext, SurfaceHandle_t *pList, int listCount, bool bDoMasking );

	virtual void DrawFlashlightOverlays( IMatRenderContext *pRenderContext, int sortGroup, bool bDoMasking );

	virtual void DrawFlashlightDepthTexture( );
	virtual void SetFlashlightDepthTexture( ShadowHandle_t shadowHandle, ITexture *pFlashlightDepthTexture, unsigned char ucShadowStencilBit );

	virtual void DrawFlashlightDecalsOnDisplacements( IMatRenderContext *pRenderContext, int sortGroup, CDispInfo **visibleDisps, int nVisibleDisps, bool bDoMasking );
	virtual bool ModelHasShadows( ModelInstanceHandle_t instance );

	virtual void DrawVolumetrics();
	virtual int ProjectAndClipVerticesEx( ShadowHandle_t handle, int count, 
		Vector** ppPosition, ShadowVertex_t*** ppOutVertex, ShadowClipState_t& clip );

	virtual bool SinglePassFlashlightModeEnabled( void );
	virtual int SetupFlashlightRenderInstanceInfo( ShadowHandle_t *pFlashlights, uint32 *pModelUsageMask, int nUsageStride, int nInstanceCount, const ModelInstanceHandle_t *pInstance );
	virtual void GetFlashlightRenderInfo( FlashlightInstance_t *pFlashlightState, int nCount, const ShadowHandle_t *pHandles );

	virtual void RemoveAllDecalsFromShadow( ShadowHandle_t handle );

	virtual void SkipShadowForEntity( int nEntIndex );

	virtual void PushFlashlightScissorBounds( void );
	virtual void PopFlashlightScissorBounds( void );
	virtual void DisableDropShadows();

private:
	enum
	{
		SHADOW_DISABLED = (SHADOW_LAST_FLAG << 1),
	};

	typedef CUtlFixedLinkedList< ShadowDecalHandle_t >::IndexType_t ShadowSurfaceIndex_t;

	struct SurfaceBounds_t
	{
		fltx4 m_vecMins;
		fltx4 m_vecMaxs;
		Vector m_vecCenter;
		float m_flRadius;
		int m_nSurfaceIndex;
	};

	struct ShadowVertexSmallList_t
	{
		ShadowVertex_t	m_Verts[SHADOW_VERTEX_SMALL_CACHE_COUNT];
	};

	struct ShadowVertexLargeList_t
	{
		ShadowVertex_t	m_Verts[SHADOW_VERTEX_LARGE_CACHE_COUNT];
	};

	// A cache entries' worth of vertices....
	struct ShadowVertexCache_t
	{
		unsigned short	m_Count;
		ShadowHandle_t	m_Shadow;
		unsigned short	m_CachedVerts;
		ShadowVertex_t*	m_pVerts;
	};

	typedef unsigned short FlashlightHandle_t;

	// Shadow state
	struct Shadow_t : public ShadowInfo_t
	{
		Vector			m_ProjectionDir;
		IMaterial*		m_pMaterial;		// material for rendering surfaces
		IMaterial*		m_pModelMaterial;	// material for rendering models
		void*			m_pBindProxy;
		unsigned short	m_Flags;
		unsigned short	m_SortOrder;
		float			m_flSphereRadius;	// Radius of sphere surrounding the shadow
		Ray_t			m_Ray;				// NOTE: Ray needs to be on 16-byte boundaries.
		Vector			m_vecSphereCenter;	// Sphere surrounding the shadow

		FlashlightHandle_t m_FlashlightHandle;
		ITexture		  *m_pFlashlightDepthTexture;

		// Extra clip planes
		unsigned short	m_ClipPlaneCount;
		Vector			m_ClipPlane[MAX_CLIP_PLANE_COUNT];
		float			m_ClipDist[MAX_CLIP_PLANE_COUNT];
		
		// First shadow decal the shadow has
		ShadowSurfaceIndex_t m_FirstDecal;

		// First model the shadow is projected onto
		unsigned short	m_FirstModel;
		
		// Stencil bit used to mask this shadow
		unsigned char	m_ucShadowStencilBit;

		int				m_nEntIndex;
	};

	// Each surface has one of these, they reference the main shadow
	// projector and cached off shadow decals.
	struct ShadowDecal_t
	{
		SurfaceHandle_t		m_SurfID;
		ShadowSurfaceIndex_t m_ShadowListIndex;
		ShadowHandle_t		m_Shadow;
		DispShadowHandle_t	m_DispShadow;
		unsigned short		m_ShadowVerts;

		// This is a handle of the next shadow decal to be rendered
		ShadowDecalHandle_t	m_NextRender;
	};

	// This structure is used when building new shadow information
	struct ShadowBuildInfo_t
	{
		ShadowHandle_t	m_Shadow;
		Vector			m_RayStart;
		Vector			m_ProjectionDirection;
		Vector			m_vecSphereCenter;	// Sphere surrounding the shadow
		float			m_flSphereRadius;	// Radius of sphere surrounding the shadow
		const byte		*m_pVis;				// Vis from the ray start
	};

	// This structure contains rendering information
	struct ShadowRenderInfo_t
	{
		int		m_VertexCount;
		int		m_IndexCount;
		int		m_nMaxVertices;
		int		m_nMaxIndices;
		int		m_Count;
		int*	m_pCache;
		int		m_DispCount;
		const VMatrix* m_pModelToWorld;
		VMatrix m_WorldToModel;
		DispShadowHandle_t*	m_pDispCache;
	};

	// Structures used to assign sort order handles
	struct SortOrderInfo_t
	{
		intp	m_MaterialEnum;
		int	m_RefCount;
	};

	typedef void (*ShadowDebugFunc_t)( ShadowHandle_t shadowHandle, const Vector &vecCentroid );

	// m_FlashlightWorldMaterialBuckets is where surfaces are stored per flashlight each frame.
	typedef CUtlVector<FlashlightHandle_t> WorldMaterialBuckets_t;

#pragma pack(16)
	struct FlashlightInfo_t
	{
		Frustum_t m_Frustum;
		FlashlightState_t m_FlashlightState;
		unsigned short m_Shadow;
		CMaterialsBuckets<SurfaceHandle_t> m_MaterialBuckets;
		CMaterialsBuckets<SurfaceHandle_t> m_OccluderBuckets;

		int m_nSplitscreenOwner;
	};
#pragma pack()

	struct DispDecalWorkItem_t
	{
		ShadowDecalHandle_t h;
		bool bKeepShadow;
		int vertCount;
		int indexCount;
	};

private:
	// Applies a flashlight to all surfaces in the leaf
	void ApplyFlashlightToLeaf( const Shadow_t &shadow, mleaf_t* pLeaf, ShadowBuildInfo_t* pBuild );

	// Applies a shadow to all surfaces in the leaf
	void ApplyShadowToLeaf( const Shadow_t &shadow, mleaf_t* RESTRICT pLeaf, ShadowBuildInfo_t* RESTRICT pBuild );

	// These functions deal with creation of render sort ids
	void SetMaterial( Shadow_t& shadow, IMaterial* pMaterial, IMaterial* pModelMaterial, void* pBindProxy );
	void CleanupMaterial( Shadow_t& shadow );

	// These functions add/remove shadow decals to surfaces
	ShadowDecalHandle_t AddShadowDecalToSurface( SurfaceHandle_t surfID, ShadowHandle_t handle );
	void RemoveShadowDecalFromSurface( SurfaceHandle_t surfID, ShadowDecalHandle_t decalHandle );

	// Adds the surface to the list for this shadow
	bool AddDecalToShadowList( ShadowHandle_t handle, ShadowDecalHandle_t decalHandle );

	// Removes the shadow to the list of surfaces
	void RemoveDecalFromShadowList( ShadowHandle_t handle, ShadowDecalHandle_t decalHandle );

	// Actually projects + clips vertices
	int ProjectAndClipVertices( const Shadow_t& shadow, const VMatrix& worldToShadow, 
		const VMatrix *pWorldToModel, int count, Vector** ppPosition, ShadowVertex_t*** ppOutVertex, ShadowClipState_t& clip );

	// These functions hook/unhook shadows up to surfaces + vice versa
	void AddSurfaceToShadow( ShadowHandle_t handle, SurfaceHandle_t surfID );
	void RemoveSurfaceFromShadow( ShadowHandle_t handle, SurfaceHandle_t surfID );
	void RemoveAllSurfacesFromShadow( ShadowHandle_t handle );
	void RemoveAllShadowsFromSurface( SurfaceHandle_t surfID );

	// Deals with model shadow management
	void RemoveAllModelsFromShadow( ShadowHandle_t handle );

	// Applies the shadow to a surface
	void ApplyShadowToSurface( ShadowBuildInfo_t& build, SurfaceHandle_t surfID );

	// Applies the shadow to a displacement
	void ApplyShadowToDisplacement( ShadowBuildInfo_t& build, IDispInfo *pDispInfo, bool bIsFlashlight );

	// Renders shadows that all share a material enumeration
	void RenderShadowList( IMatRenderContext *pRenderContext, ShadowDecalHandle_t decalHandle, const VMatrix* pModelToWorld );

	// Should we cache vertices?
	bool ShouldCacheVertices( const ShadowDecal_t& decal );

	// Generates a list displacement shadow vertices to render
	bool GenerateDispShadowRenderInfo( ShadowDecal_t& decal, ShadowRenderInfo_t& info );

	// Generates a list shadow vertices to render
	bool GenerateNormalShadowRenderInfo( IMatRenderContext *pRenderContext, ShadowDecal_t& decal, ShadowRenderInfo_t& info );

	// Adds normal shadows to the mesh builder
	int AddNormalShadowsToMeshBuilder( CMeshBuilder& meshBuilder, ShadowRenderInfo_t& info );

	// Adds displacement shadows to the mesh builder
	int AddDisplacementShadowsToMeshBuilder( CMeshBuilder& meshBuilder, 
									ShadowRenderInfo_t& info, int baseIndex );

	// Does the actual work of computing shadow vertices
	bool ComputeShadowVertices( ShadowDecal_t& decal, const VMatrix* pModelToWorld, const VMatrix* pWorldToModel, ShadowVertexCache_t* pVertexCache );

	// Project vertices into shadow space
	bool ProjectVerticesIntoShadowSpace( const VMatrix * RESTRICT modelToShadow, 
		float maxDist, int count, Vector** RESTRICT ppPosition, ShadowClipState_t& clip );

	// Copies vertex info from the clipped vertices
	void CopyClippedVertices( int count, ShadowVertex_t** ppSrcVert, ShadowVertex_t* pDstVert, const Vector &vToAdd );

	// Allocate, free vertices
	ShadowVertex_t* AllocateVertices( ShadowVertexCache_t& cache, int count );
	void FreeVertices( ShadowVertexCache_t& cache );

	// Gets at cache entry...
	ShadowVertex_t* GetCachedVerts( const ShadowVertexCache_t& cache );

	// Clears out vertices in the temporary cache
	void ClearTempCache( );

	// Renders debugging information
	void RenderDebuggingInfo( const ShadowRenderInfo_t &info, ShadowDebugFunc_t func );

	// Methods for dealing with world material buckets for flashlights.
	void ClearAllFlashlightMaterialBuckets( void );
	void AddSurfaceToFlashlightMaterialBuckets( ShadowHandle_t handle, SurfaceHandle_t surfID );
	void AllocFlashlightMaterialBuckets( FlashlightHandle_t flashlightID );

	// Render all projected textures (including shadows and flashlights)
	void RenderProjectedTextures( IMatRenderContext *pRenderContext, const VMatrix* pModelToWorld );

	void RenderFlashlights( bool bDoMasking, bool bDoSimpleProjections, const VMatrix* pModelToWorld );

	void SetFlashlightStencilMasks( bool bDoMasking );

	void SetStencilAndScissor( IMatRenderContext *pRenderContext, FlashlightInfo_t &flashlightInfo, bool bUseStencil );

	void EnableStencilAndScissorMasking( IMatRenderContext *pRenderContext, const FlashlightInfo_t &flashlightInfo, bool bDoMasking );

	void DisableStencilAndScissorMasking( IMatRenderContext *pRenderContext, const FlashlightInfo_t &flashlightInfo, bool bDoMasking );

	void RenderShadows( IMatRenderContext *pRenderContext, const VMatrix* pModelToWorld );

	// Generates a list shadow vertices to render
	void GenerateShadowRenderInfo( IMatRenderContext *pRenderContext, ShadowDecalHandle_t decalHandle, ShadowRenderInfo_t& info );
	void GenerateShadowRenderInfoThreaded( IMatRenderContext *pRenderContext, ShadowDecalHandle_t decalHandle, ShadowRenderInfo_t& info );

	void ProcessDispDecalWorkItem( DispDecalWorkItem_t& wi );

	// Methods related to the surface bounds cache
	void ComputeSurfaceBounds( SurfaceBounds_t* pBounds, SurfaceHandle_t nSurfID );
	const SurfaceBounds_t* GetSurfaceBounds( SurfaceHandle_t nSurfID );
	bool IsShadowNearSurface( ShadowHandle_t h, SurfaceHandle_t nSurfID, const VMatrix* pModelToWorld, const VMatrix* pWorldToModel );

private:
	// List of all shadows (one per cast shadow)
	// Align it so the Ray in the Shadow_t is aligned
	CUtlLinkedList< Shadow_t, ShadowHandle_t, false, int, CUtlMemoryAligned< UtlLinkedListElem_t< Shadow_t, ShadowHandle_t >, 16 > > m_Shadows;
	
	// List of all shadow decals (one per surface hit by a shadow)
	CUtlLinkedList< ShadowDecal_t, ShadowDecalHandle_t, true, int > m_ShadowDecals;

	// List of all shadow decals associated with a particular shadow
	CUtlFixedLinkedList< ShadowDecalHandle_t > m_ShadowSurfaces;

	// List of queued decals waiting to be rendered....
	CUtlVector<ShadowDecalHandle_t>	m_RenderQueue;

	// Used to assign sort order handles
	CUtlLinkedList<SortOrderInfo_t, unsigned short>	m_SortOrderIds;

	// A cache of shadow vertex data...
	CUtlLinkedList<ShadowVertexCache_t, unsigned short>	m_VertexCache;

	// This is temporary, not saved off....
	CUtlVector<ShadowVertexCache_t>	m_TempVertexCache;

	// Vertex data
	CUtlLinkedList<ShadowVertexSmallList_t, unsigned short>	m_SmallVertexList;
	CUtlLinkedList<ShadowVertexLargeList_t, unsigned short>	m_LargeVertexList;

	// Model-shadow association
	CBidirectionalSet< ModelInstanceHandle_t, ShadowHandle_t, unsigned short >	m_ShadowsOnModels;

	// Cache of information for surface bounds
	typedef CUtlLinkedList< SurfaceBounds_t, unsigned short, false, int, CUtlMemoryFixed< UtlLinkedListElem_t< SurfaceBounds_t, unsigned short >, SURFACE_BOUNDS_CACHE_COUNT, 16 > > SurfaceBoundsCache_t;
	typedef SurfaceBoundsCache_t::IndexType_t SurfaceBoundsCacheIndex_t;
	SurfaceBoundsCache_t m_SurfaceBoundsCache;
	SurfaceBoundsCacheIndex_t *m_pSurfaceBounds;

	// The number of decals we're gonna need to render
	int	m_DecalsToRender;

	typedef CUtlLinkedList< FlashlightInfo_t, unsigned short, false, unsigned short, CUtlMemoryAligned< UtlLinkedListElem_t< FlashlightInfo_t,unsigned short > ,16 > > FlashlightList_t;
	FlashlightList_t m_FlashlightStates;
	int m_NumWorldMaterialBuckets;
	bool m_bInitialized;

	//=============================================================================
	// HPE_BEGIN:
	// [smessick] These used to be dynamically allocated on the stack.
	//=============================================================================
	CUtlMemory<int> m_ShadowDecalCache;
	CUtlMemory<DispShadowHandle_t> m_DispShadowDecalCache;
	//=============================================================================
	// HPE_END
	//=============================================================================
	
	ShadowHandle_t	m_hSinglePassFlashlightState;
	bool			m_bSinglePassFlashlightStateEnabled;
	bool			m_bShadowsDisabled;
	CUtlStack<bool> m_bStack_SinglePassFlashlightStateEnabled;

	int m_nSkipShadowForEntIndex;

	struct FlashLightScissorStateBackup_t
	{
		int m_nLeft;
		int m_nTop;
		int m_nRight;
		int m_nBottom;
		bool m_bScissor;
#if defined( DBGFLAG_ASSERT )
		int m_iFlashLightID;
#endif
	};

	CUtlVector<FlashLightScissorStateBackup_t> m_ScissorStateBackups;
	CUtlVector<int> m_ScissorStateEntryStart;
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CShadowMgr s_ShadowMgr;
IShadowMgrInternal* g_pShadowMgr = &s_ShadowMgr;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CShadowMgr, IShadowMgr, 
			ENGINE_SHADOWMGR_INTERFACE_VERSION, s_ShadowMgr);


//-----------------------------------------------------------------------------
// Shadows on model instances
//-----------------------------------------------------------------------------
unsigned short& FirstShadowOnModel( ModelInstanceHandle_t h )
{
	// See l_studio.cpp
	return FirstShadowOnModelInstance( h );
}

unsigned short& FirstModelInShadow( ShadowHandle_t h )
{
	return s_ShadowMgr.FirstModelInShadow(h); 
}


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CShadowMgr::CShadowMgr()
{
	m_bShadowsDisabled = false;
	m_ShadowSurfaces.SetGrowSize( 4096 );
	m_ShadowDecals.SetGrowSize( 4096 );

	m_ShadowsOnModels.Init( ::FirstShadowOnModel, ::FirstModelInShadow );
	m_NumWorldMaterialBuckets = 0;
	m_pSurfaceBounds = NULL;
	m_bInitialized = false;
	m_hSinglePassFlashlightState = SHADOW_HANDLE_INVALID;
	m_bSinglePassFlashlightStateEnabled = IsGameConsole();
	
	m_nSkipShadowForEntIndex = INT_MIN;

	ClearShadowRenderList();

	//=============================================================================
	// HPE_BEGIN:
	// [smessick] Initialize the shadow decal caches. These used to be dynamically
	// allocated on the stack, but we were getting stack overflows.
	//=============================================================================

	m_ShadowDecalCache.SetGrowSize( 4096 );
	m_DispShadowDecalCache.SetGrowSize( 4096 );

	m_ShadowDecalCache.Grow( SHADOW_DECAL_CACHE_COUNT );
	m_DispShadowDecalCache.Grow( SHADOW_DECAL_CACHE_COUNT );

	//=============================================================================
	// HPE_END
	//=============================================================================
}


//-----------------------------------------------------------------------------
// Level init, shutdown
//-----------------------------------------------------------------------------
void CShadowMgr::LevelInit( int nSurfCount )
{
	if ( m_bInitialized )
		return;
	m_bInitialized = true;

	materials->GetRenderContext()->EnableSinglePassFlashlightMode( m_bSinglePassFlashlightStateEnabled );

	m_pSurfaceBounds = new SurfaceBoundsCacheIndex_t[nSurfCount];

	// NOTE: Need to memset to 0 if we switch to integer SurfaceBoundsCacheIndex_t here
	COMPILE_TIME_ASSERT( sizeof(SurfaceBoundsCacheIndex_t) == 2 );
	memset( m_pSurfaceBounds, 0xFF, nSurfCount * sizeof(SurfaceBoundsCacheIndex_t) );
}

void CShadowMgr::LevelShutdown()
{
	if ( !m_bInitialized )
		return;

	if ( m_pSurfaceBounds )
	{
		delete[] m_pSurfaceBounds;
		m_pSurfaceBounds = NULL;
	}

	m_SurfaceBoundsCache.RemoveAll();
	m_bInitialized = false;
}


//-----------------------------------------------------------------------------
// Track growth of shadow-related linked lists, warn when things grow significantly
//-----------------------------------------------------------------------------
#if defined( LEFT4DEAD )
	// GrowSpew is causing unnecessary overhead on the L4D 360 build
	#define GrowSpew( ... )
#else
	static int m_ShadowSurfacesMax  = 0, m_ShadowDecalsMax = 0, m_SortOrderIdsMax = 0, m_SmallVertexListMax = 0, m_LargeVertexListMax = 0;
	static int m_ShadowsOnModelsMax = 0, m_RenderQueueMax  = 0, m_VertexCacheMax  = 0, m_TempVertexCacheMax = 0;
	static void GrowSpew( int numAlloc, int &maxCounter, const char *array, int spewFreq = 1000 )
	{
		if ( numAlloc <= maxCounter ) return;
		if ( ( maxCounter / spewFreq ) != ( numAlloc / spewFreq ) )
			Warning( "Shadow memory (%s) growing [%d]\n", array, numAlloc );
		maxCounter = numAlloc;
	}
#endif


void CShadowMgr::DisableDropShadows()
{
	m_bShadowsDisabled = true;
}

//-----------------------------------------------------------------------------
// Create, destroy material sort order ids...
//-----------------------------------------------------------------------------
void CShadowMgr::SetMaterial( Shadow_t& shadow, IMaterial* pMaterial, IMaterial* pModelMaterial, void *pBindProxy )
{
	shadow.m_pMaterial = pMaterial;
	shadow.m_pModelMaterial = pModelMaterial;
	shadow.m_pBindProxy = pBindProxy;

	// We're holding onto this material
	if ( pMaterial )
	{
		pMaterial->IncrementReferenceCount();
	}
	if ( pModelMaterial )
	{
		pModelMaterial->IncrementReferenceCount();
	}

	// Search the sort order handles for an enumeration id match
	intp materialEnum = (intp)pMaterial;
	for (int i = m_SortOrderIds.Head(); i != m_SortOrderIds.InvalidIndex(); i = m_SortOrderIds.Next(i) )
	{
		// Found a match, lets increment the refcount of this sort order id
		if (m_SortOrderIds[i].m_MaterialEnum == materialEnum)
		{
			++m_SortOrderIds[i].m_RefCount;
			shadow.m_SortOrder = i;
			return;
		}
	}

	// Didn't find it, lets assign a new sort order ID, with a refcount of 1
	GrowSpew( m_SortOrderIds.Count(), m_SortOrderIdsMax, "m_SortOrderIds" );
	shadow.m_SortOrder = m_SortOrderIds.AddToTail();
	m_SortOrderIds[shadow.m_SortOrder].m_MaterialEnum  = materialEnum;
	m_SortOrderIds[shadow.m_SortOrder].m_RefCount = 1;


	// Make sure the render queue has as many entries as the max sort order id.
	int count = m_RenderQueue.Count();
	GrowSpew( m_RenderQueue.Count(), m_RenderQueueMax, "m_RenderQueue" );
	while( count < m_SortOrderIds.MaxElementIndex() )
	{
		MEM_ALLOC_CREDIT();
		m_RenderQueue.AddToTail( SHADOW_DECAL_HANDLE_INVALID );
		++count;
	}
}

void CShadowMgr::CleanupMaterial( Shadow_t& shadow )
{
	// Decrease the sort order reference count
	if (--m_SortOrderIds[shadow.m_SortOrder].m_RefCount <= 0)
	{
		// No one referencing the sort order number?
		// Then lets clean up the sort order id
		m_SortOrderIds.Remove(shadow.m_SortOrder);
	}

	// We're done with this material
	if ( shadow.m_pMaterial )
	{
		shadow.m_pMaterial->DecrementReferenceCount();
	}
	if ( shadow.m_pModelMaterial )
	{
		shadow.m_pModelMaterial->DecrementReferenceCount();
	}
}


//-----------------------------------------------------------------------------
// For the model shadow list
//-----------------------------------------------------------------------------
unsigned short CShadowMgr::InvalidShadowIndex( )
{
	return m_ShadowsOnModels.InvalidIndex();
}

void CShadowMgr::RemoveAllDecalsFromShadow( ShadowHandle_t handle )
{
	RemoveAllSurfacesFromShadow( handle );
	RemoveAllModelsFromShadow( handle );
}

//-----------------------------------------------------------------------------
// Create, destroy shadows
//-----------------------------------------------------------------------------
ShadowHandle_t CShadowMgr::CreateShadow( IMaterial* pMaterial, IMaterial* pModelMaterial, void* pBindProxy, int creationFlags )
{
	return CreateShadowEx( pMaterial, pModelMaterial, pBindProxy, creationFlags, -1 );
}


ShadowHandle_t CShadowMgr::CreateShadowEx( IMaterial* pMaterial, IMaterial* pModelMaterial, void* pBindProxy, int creationFlags, int nEntIndex )
{
#ifndef DEDICATED
	ShadowHandle_t h = m_Shadows.AddToTail();
	//=============================================================================
	// HPE_BEGIN:
	// [smessick] Check for overflow.
	//=============================================================================
	if ( h == m_Shadows.InvalidIndex() )
	{
		ExecuteNTimes( 10, Warning( "CShadowMgr::CreateShadowEx - overflowed m_Shadows linked list!\n" ) );
		return h;
	}
	//=============================================================================
	// HPE_END
	//=============================================================================

	Shadow_t& shadow = m_Shadows[h];
	SetMaterial( shadow, pMaterial, pModelMaterial, pBindProxy );
	shadow.m_Flags = creationFlags;
	shadow.m_FirstDecal = m_ShadowSurfaces.InvalidIndex();
	shadow.m_FirstModel = m_ShadowsOnModels.InvalidIndex(); 
	shadow.m_ProjectionDir.Init( 0, 0, 1 );
	shadow.m_TexOrigin.Init( 0, 0 );
	shadow.m_TexSize.Init( 1, 1 );
	shadow.m_ClipPlaneCount = 0;
	shadow.m_FalloffBias = 0;
	shadow.m_pFlashlightDepthTexture = NULL;
	shadow.m_FlashlightHandle = m_FlashlightStates.InvalidIndex();
	shadow.m_nEntIndex = nEntIndex;

	if ( ( creationFlags & ( SHADOW_FLASHLIGHT | SHADOW_SIMPLE_PROJECTION ) ) != 0 )
	{
		shadow.m_FlashlightHandle = m_FlashlightStates.AddToTail();
		m_FlashlightStates[shadow.m_FlashlightHandle].m_Shadow = h;
		
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		m_FlashlightStates[ shadow.m_FlashlightHandle ].m_nSplitscreenOwner = ( creationFlags & SHADOW_ANY_SPLITSCREEN_SLOT ) ? -1 : GET_ACTIVE_SPLITSCREEN_SLOT();
		
		if ( !m_bSinglePassFlashlightStateEnabled )
		{
			AllocFlashlightMaterialBuckets( shadow.m_FlashlightHandle );
		}
	}

	MatrixSetIdentity( shadow.m_WorldToShadow );
	return h;
#endif
}

void CShadowMgr::DestroyShadow( ShadowHandle_t handle )
{
	CleanupMaterial( m_Shadows[handle] );
	RemoveAllSurfacesFromShadow( handle );
	RemoveAllModelsFromShadow( handle );
	if( m_Shadows[handle].m_FlashlightHandle != m_FlashlightStates.InvalidIndex() )
	{
		m_FlashlightStates.Remove( m_Shadows[handle].m_FlashlightHandle );
	}

	m_Shadows.Remove(handle);
}


//-----------------------------------------------------------------------------
// Resets the shadow material (useful for shadow LOD.. doing blobby at distance) 
//-----------------------------------------------------------------------------
void CShadowMgr::SetShadowMaterial( ShadowHandle_t handle, IMaterial* pMaterial, IMaterial* pModelMaterial, void* pBindProxy )
{
	Shadow_t& shadow = m_Shadows[handle];
	if ( (shadow.m_pMaterial != pMaterial) || (shadow.m_pModelMaterial != pModelMaterial) || (shadow.m_pBindProxy != pBindProxy) )
	{
		CleanupMaterial( shadow );
		SetMaterial( shadow, pMaterial, pModelMaterial, pBindProxy );
	}
}


//-----------------------------------------------------------------------------
// Sets the texture coordinate range for a shadow...
//-----------------------------------------------------------------------------
void CShadowMgr::SetShadowTexCoord( ShadowHandle_t handle, float x, float y, float w, float h )
{
	Shadow_t& shadow = m_Shadows[handle];
	shadow.m_TexOrigin.Init( x, y );
	shadow.m_TexSize.Init( w, h );
}


//-----------------------------------------------------------------------------
// Set extra clip planes related to shadows...
//-----------------------------------------------------------------------------
void CShadowMgr::ClearExtraClipPlanes( ShadowHandle_t h )
{
	m_Shadows[h].m_ClipPlaneCount = 0;
}

void CShadowMgr::AddExtraClipPlane( ShadowHandle_t h, const Vector& normal, float dist )
{
	Shadow_t& shadow = m_Shadows[h];
	Assert( shadow.m_ClipPlaneCount < MAX_CLIP_PLANE_COUNT );

	VectorCopy( normal, shadow.m_ClipPlane[shadow.m_ClipPlaneCount] );
	shadow.m_ClipDist[shadow.m_ClipPlaneCount] = dist;
	++shadow.m_ClipPlaneCount;
}


//-----------------------------------------------------------------------------
// Gets at information about a particular shadow
//-----------------------------------------------------------------------------
const ShadowInfo_t& CShadowMgr::GetInfo( ShadowHandle_t handle )
{
	return m_Shadows[handle];
}


//-----------------------------------------------------------------------------
// Gets at cache entry...
//-----------------------------------------------------------------------------
ShadowVertex_t* CShadowMgr::GetCachedVerts( const ShadowVertexCache_t& cache )
{
	if (cache.m_Count == 0)
		return 0 ;

	if (cache.m_pVerts)
		return cache.m_pVerts;

	if (cache.m_Count <= SHADOW_VERTEX_SMALL_CACHE_COUNT)
		return m_SmallVertexList[cache.m_CachedVerts].m_Verts;

	return m_LargeVertexList[cache.m_CachedVerts].m_Verts;
}

//-----------------------------------------------------------------------------
// Allocates, cleans up vertex cache vertices
//-----------------------------------------------------------------------------
inline ShadowVertex_t* CShadowMgr::AllocateVertices( ShadowVertexCache_t& cache, int count )
{
	cache.m_pVerts = 0;
	if (count <= SHADOW_VERTEX_SMALL_CACHE_COUNT)
	{
		cache.m_Count = count;
		GrowSpew( m_SmallVertexList.NumAllocated(), m_SmallVertexListMax, "m_SmallVertexList" );
		cache.m_CachedVerts = m_SmallVertexList.AddToTail( );
		return m_SmallVertexList[cache.m_CachedVerts].m_Verts;
	}
	else if (count <= SHADOW_VERTEX_LARGE_CACHE_COUNT)
	{
		cache.m_Count = count;
		GrowSpew( m_LargeVertexList.NumAllocated(), m_LargeVertexListMax, "m_LargeVertexList" );
		cache.m_CachedVerts = m_LargeVertexList.AddToTail( );
		return m_LargeVertexList[cache.m_CachedVerts].m_Verts;
	}

	cache.m_Count = count;
	if (count > 0)
	{
		cache.m_pVerts = new ShadowVertex_t[count];
	}
	cache.m_CachedVerts = m_LargeVertexList.InvalidIndex();
	return cache.m_pVerts;
}

inline void CShadowMgr::FreeVertices( ShadowVertexCache_t& cache )
{
	if (cache.m_Count == 0)
		return;

	if (cache.m_pVerts)
	{
		delete[] cache.m_pVerts;
	}
	else if (cache.m_Count <= SHADOW_VERTEX_SMALL_CACHE_COUNT)
	{
		m_SmallVertexList.Remove( cache.m_CachedVerts );
	}
	else
	{
		m_LargeVertexList.Remove( cache.m_CachedVerts );
	}
}


//-----------------------------------------------------------------------------
// Clears out vertices in the temporary cache
//-----------------------------------------------------------------------------
void CShadowMgr::ClearTempCache( )
{
	// Clear out the vertices
	for (int i = m_TempVertexCache.Count(); --i >= 0; ) 
	{
		FreeVertices( m_TempVertexCache[i] );
	}

	m_TempVertexCache.RemoveAll();
}

//-----------------------------------------------------------------------------
// Adds the surface to the list for this shadow
//-----------------------------------------------------------------------------
bool CShadowMgr::AddDecalToShadowList( ShadowHandle_t handle, ShadowDecalHandle_t decalHandle )
{
	// Add the shadow to the list of surfaces affected by this shadow
	GrowSpew( m_ShadowSurfaces.NumAllocated(), m_ShadowSurfacesMax, "m_ShadowSurfaces", 8192 );
  	ShadowSurfaceIndex_t idx = m_ShadowSurfaces.Alloc( true );
	if ( idx == m_ShadowSurfaces.InvalidIndex() )
	{
		ExecuteNTimes( 10, Warning( "CShadowMgr::AddDecalToShadowList - overflowed m_ShadowSurfaces linked list!\n" ) );
		return false;
	}

	m_ShadowSurfaces[idx] = decalHandle;
	if ( m_Shadows[handle].m_FirstDecal != m_ShadowSurfaces.InvalidIndex() )
	{
		m_ShadowSurfaces.LinkBefore( m_Shadows[handle].m_FirstDecal, idx );
	}
	m_Shadows[handle].m_FirstDecal = idx;
	m_ShadowDecals[decalHandle].m_ShadowListIndex = idx;

	return true;
}


//-----------------------------------------------------------------------------
// Removes the shadow to the list of surfaces
//-----------------------------------------------------------------------------
void CShadowMgr::RemoveDecalFromShadowList( ShadowHandle_t handle, ShadowDecalHandle_t decalHandle )
{
	ShadowSurfaceIndex_t idx = m_ShadowDecals[decalHandle].m_ShadowListIndex;

	// Make sure the list of shadow decals for a single shadow is ok
	if ( m_Shadows[handle].m_FirstDecal == idx )
	{
		m_Shadows[handle].m_FirstDecal = m_ShadowSurfaces.Next(idx);
	}

	// Remove it from the shadow surfaces list
	m_ShadowSurfaces.Free(idx);

	// Blat out the decal index
	m_ShadowDecals[decalHandle].m_ShadowListIndex = m_ShadowSurfaces.InvalidIndex();
}


//-----------------------------------------------------------------------------
// Computes spherical bounds for a surface
//-----------------------------------------------------------------------------
void CShadowMgr::ComputeSurfaceBounds( SurfaceBounds_t* pBounds, SurfaceHandle_t nSurfID )
{
	pBounds->m_vecCenter.Init();
	pBounds->m_vecMins = ReplicateX4( FLT_MAX );
	pBounds->m_vecMaxs = ReplicateX4( -FLT_MAX );
	int nCount = MSurf_VertCount( nSurfID );
	for ( int i = 0; i < nCount; ++i )
	{
		int nVertIndex = host_state.worldbrush->vertindices[ MSurf_FirstVertIndex( nSurfID ) + i ];
		const Vector &position = host_state.worldbrush->vertexes[ nVertIndex ].position;
		pBounds->m_vecCenter += position;

		fltx4 pos4 = LoadUnaligned3SIMD( position.Base() );
		pBounds->m_vecMins = MinSIMD( pos4, pBounds->m_vecMins );
		pBounds->m_vecMaxs = MaxSIMD( pos4, pBounds->m_vecMaxs );
	}

	fltx4 eps = ReplicateX4( 1e-3 );
	pBounds->m_vecMins = SetWToZeroSIMD( SubSIMD( pBounds->m_vecMins, eps ) );
	pBounds->m_vecMaxs = SetWToZeroSIMD( AddSIMD( pBounds->m_vecMaxs, eps ) );
	pBounds->m_vecCenter /= nCount;

	pBounds->m_flRadius = 0.0f;
	for ( int i = 0; i < nCount; ++i )
	{
		int nVertIndex = host_state.worldbrush->vertindices[ MSurf_FirstVertIndex( nSurfID ) + i ];
		const Vector &position = host_state.worldbrush->vertexes[ nVertIndex ].position;
		float flDistSq = position.DistToSqr( pBounds->m_vecCenter );
		if ( flDistSq > pBounds->m_flRadius )
		{
			pBounds->m_flRadius = flDistSq;
		}
	}
	pBounds->m_flRadius = sqrt( pBounds->m_flRadius );
}


//-----------------------------------------------------------------------------
// Get spherical bounds for a surface
//-----------------------------------------------------------------------------
const CShadowMgr::SurfaceBounds_t* CShadowMgr::GetSurfaceBounds( SurfaceHandle_t surfID )
{
	int nSurfaceIndex = MSurf_Index( surfID );

	// NOTE: We're not bumping the surface index to the front of the LRU
	// here, but I think if we did the cost doing that would exceed the cost
	// of anything else in this path.
	// If this turns out to not be true, then we should make this a true LRU
	if ( m_pSurfaceBounds[nSurfaceIndex] != m_SurfaceBoundsCache.InvalidIndex() )
		return &m_SurfaceBoundsCache[ m_pSurfaceBounds[nSurfaceIndex] ];

	SurfaceBoundsCacheIndex_t nIndex;
	if ( m_SurfaceBoundsCache.Count() >= SURFACE_BOUNDS_CACHE_COUNT )
	{
		// Retire existing cache entry if we're out of space,
		// move it to the head of the LRU cache
		nIndex = m_SurfaceBoundsCache.Tail( );
		m_SurfaceBoundsCache.Unlink( nIndex );
		m_SurfaceBoundsCache.LinkToHead( nIndex );
		m_pSurfaceBounds[ m_SurfaceBoundsCache[nIndex].m_nSurfaceIndex ] = m_SurfaceBoundsCache.InvalidIndex();
	}
	else
	{
		// Allocate new cache entry if we have more room
		nIndex = m_SurfaceBoundsCache.AddToHead( );
	}
	m_pSurfaceBounds[ nSurfaceIndex ] = nIndex;

	// Computes the surface bounds
	SurfaceBounds_t &bounds = m_SurfaceBoundsCache[nIndex];
	bounds.m_nSurfaceIndex = nSurfaceIndex;
	ComputeSurfaceBounds( &bounds, surfID );
	return &bounds;
}


//-----------------------------------------------------------------------------
// Is the shadow near the surface?
//-----------------------------------------------------------------------------
bool CShadowMgr::IsShadowNearSurface( ShadowHandle_t h, SurfaceHandle_t nSurfID, 
	const VMatrix* pModelToWorld, const VMatrix* pWorldToModel )
{
	const Shadow_t &shadow = m_Shadows[h];
	const SurfaceBounds_t* pBounds = GetSurfaceBounds( nSurfID );
	Vector vecSurfCenter;
	if ( !pModelToWorld )
	{
		vecSurfCenter = pBounds->m_vecCenter;
	}
	else
	{
		Vector3DMultiplyPosition( *pModelToWorld, pBounds->m_vecCenter, vecSurfCenter );
	}

	// Sphere check
	Vector vecDelta;
	VectorSubtract( shadow.m_vecSphereCenter, vecSurfCenter, vecDelta );
	float flDistSqr = vecDelta.LengthSqr();
	float flMinDistSqr = pBounds->m_flRadius + shadow.m_flSphereRadius;
	flMinDistSqr *= flMinDistSqr;
	if ( flDistSqr >= flMinDistSqr )
		return false;

	if ( !pModelToWorld )
		return IsBoxIntersectingRay( pBounds->m_vecMins, pBounds->m_vecMaxs, shadow.m_Ray );

	Ray_t transformedRay;
	Vector3DMultiplyPosition( *pWorldToModel, shadow.m_Ray.m_Start, transformedRay.m_Start );
	Vector3DMultiply( *pWorldToModel, shadow.m_Ray.m_Delta, transformedRay.m_Delta );
	transformedRay.m_StartOffset = shadow.m_Ray.m_StartOffset;
	transformedRay.m_Extents = shadow.m_Ray.m_Extents;
	transformedRay.m_IsRay = shadow.m_Ray.m_IsRay;
	transformedRay.m_IsSwept = shadow.m_Ray.m_IsSwept;
	return IsBoxIntersectingRay( pBounds->m_vecMins, pBounds->m_vecMaxs, transformedRay );
}


//-----------------------------------------------------------------------------
// Adds the shadow decal reference to the surface
//-----------------------------------------------------------------------------
inline ShadowDecalHandle_t CShadowMgr::AddShadowDecalToSurface( SurfaceHandle_t surfID, ShadowHandle_t handle )
{
	GrowSpew( m_ShadowDecals.NumAllocated(), m_ShadowDecalsMax, "m_ShadowDecals", 8192 );
	ShadowDecalHandle_t decalHandle = m_ShadowDecals.Alloc( true );
	if ( decalHandle == m_ShadowDecals.InvalidIndex() )
	{
		ExecuteNTimes( 10, Warning( "CShadowMgr::AddShadowDecalToSurface - overflowed m_ShadowDecals linked list!\n" ) );
		return decalHandle;
	}

	ShadowDecal_t& decal = m_ShadowDecals[decalHandle];
	decal.m_SurfID = surfID;
	m_ShadowDecals.LinkBefore( MSurf_ShadowDecals( surfID ), decalHandle );
	MSurf_ShadowDecals( surfID ) = decalHandle;

	// Hook the shadow into the displacement system....
	if ( !SurfaceHasDispInfo( surfID ) )
	{
		decal.m_DispShadow = DISP_SHADOW_HANDLE_INVALID;
	}
	else
	{
		decal.m_DispShadow = MSurf_DispInfo( surfID )->AddShadowDecal( handle );
	}

	decal.m_Shadow = handle;
	decal.m_ShadowVerts = m_VertexCache.InvalidIndex();
	decal.m_NextRender = SHADOW_DECAL_HANDLE_INVALID;
	decal.m_ShadowListIndex = m_ShadowSurfaces.InvalidIndex();

	//=============================================================================
	// HPE_BEGIN:
	// [smessick] Check the return value of AddDecalToShadowList and make sure 
	// to delete the newly created shadow decal if there is a failure.
	//=============================================================================
	if ( !AddDecalToShadowList( handle, decalHandle ) )
	{
		m_ShadowDecals.Free( decalHandle );
		decalHandle = m_ShadowDecals.InvalidIndex();
	}
	//=============================================================================
	// HPE_END
	//=============================================================================

	return decalHandle;
}

inline void CShadowMgr::RemoveShadowDecalFromSurface( SurfaceHandle_t surfID, ShadowDecalHandle_t decalHandle )
{
	// Clean up its shadow verts if it has any
	ShadowDecal_t& decal = m_ShadowDecals[decalHandle];
	if (decal.m_ShadowVerts != m_VertexCache.InvalidIndex())
	{
		FreeVertices( m_VertexCache[decal.m_ShadowVerts] );
		m_VertexCache.Remove(decal.m_ShadowVerts);
		decal.m_ShadowVerts = m_VertexCache.InvalidIndex();
	}

	// Clean up displacement...
	if ( decal.m_DispShadow != DISP_SHADOW_HANDLE_INVALID )
	{
		MSurf_DispInfo( decal.m_SurfID )->RemoveShadowDecal( decal.m_DispShadow );
	}

	// Make sure the list of shadow decals on a surface is set up correctly
	if ( MSurf_ShadowDecals( surfID ) == decalHandle )
	{
		MSurf_ShadowDecals( surfID ) = m_ShadowDecals.Next(decalHandle);
	}

	RemoveDecalFromShadowList( decal.m_Shadow, decalHandle );

	// Kill the shadow decal
	m_ShadowDecals.Free( decalHandle );
}

void CShadowMgr::AddSurfaceToFlashlightMaterialBuckets( ShadowHandle_t handle, SurfaceHandle_t surfID )
{
	if ( m_bSinglePassFlashlightStateEnabled )
		return;

	// Make sure that this is a flashlight.
	Assert( m_Shadows[handle].m_Flags & ( SHADOW_FLASHLIGHT | SHADOW_SIMPLE_PROJECTION ) );
	
	// Get the flashlight id for this particular shadow handle and make sure that it's valid.
	FlashlightHandle_t flashlightID = m_Shadows[handle].m_FlashlightHandle;
	Assert( flashlightID != m_FlashlightStates.InvalidIndex() );
	
	if ( m_FlashlightStates[ flashlightID ].m_nSplitscreenOwner >= 0 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		if ( m_FlashlightStates[ flashlightID ].m_nSplitscreenOwner != GET_ACTIVE_SPLITSCREEN_SLOT() )
			return;
	}

	m_FlashlightStates[flashlightID].m_MaterialBuckets.AddElement( MSurf_MaterialSortID( surfID ), surfID );
}


//-----------------------------------------------------------------------------
// Adds the shadow decal reference to the surface
// This causes a shadow decal to be made
//-----------------------------------------------------------------------------
void CShadowMgr::AddSurfaceToShadow( ShadowHandle_t handle, SurfaceHandle_t surfID )
{
	// FIXME: We could make this work, but there's a perf cost...
	// Basically, we'd need to have a separate rendering batch for
	// each translucent material the shadow is projected onto. The
	// material alpha would have to be taken into account, so that
	// no multiplication occurs where the alpha == 0
	// FLASHLIGHTFIXME: get rid of some of these checks for the ones that will work just fine with the flashlight.	
	bool bIsFlashlight = ( ( m_Shadows[handle].m_Flags & ( SHADOW_FLASHLIGHT | SHADOW_SIMPLE_PROJECTION ) ) != 0 );
	if ( !bIsFlashlight && MSurf_Flags(surfID) & (SURFDRAW_TRANS | SURFDRAW_ALPHATEST | SURFDRAW_NOSHADOWS) )
		return;

#if 0
	// Make sure the surface has the shadow on it exactly once...
	ShadowDecalHandle_t	dh = MSurf_ShadowDecals( surfID );
	while (dh != m_ShadowDecals.InvalidIndex() )
	{
		Assert ( m_ShadowDecals[dh].m_Shadow != handle );
		dh = m_ShadowDecals.Next(dh);
	}
#endif

	// Create a shadow decal for this surface and add it to the surface
	AddShadowDecalToSurface( surfID, handle );
}

void CShadowMgr::RemoveSurfaceFromShadow( ShadowHandle_t handle, SurfaceHandle_t surfID )
{
	// Find the decal associated with the handle that lies on the surface

	// FIXME: Linear search; bleah.
	// Luckily the search is probably over only a couple items at most
	// Linear searching over the shadow surfaces so we can remove the entry
	// in the shadow surface list if we find a match
	ASSERT_SURF_VALID( surfID );
	ShadowSurfaceIndex_t i = m_Shadows[handle].m_FirstDecal;
	while ( i != m_ShadowSurfaces.InvalidIndex() )
	{
		ShadowDecalHandle_t decalHandle = m_ShadowSurfaces[i];
		if ( m_ShadowDecals[decalHandle].m_SurfID == surfID )
		{
			// Found a match! There should be at most one shadow decal
			// associated with a particular shadow per surface
			RemoveShadowDecalFromSurface( surfID, decalHandle );

			// FIXME: Could check the shadow doesn't appear again in the list
			return;			
		}

		i = m_ShadowSurfaces.Next(i);
	}

#ifdef _DEBUG
	// Here, the shadow didn't have the surface in its list
	// let's make sure the surface doesn't think it's got the shadow in its list
	ShadowDecalHandle_t	dh = MSurf_ShadowDecals( surfID );
	while (dh != m_ShadowDecals.InvalidIndex() )
	{
		Assert ( m_ShadowDecals[dh].m_Shadow != handle );
		dh = m_ShadowDecals.Next(dh);
	}

#endif
}

void CShadowMgr::RemoveAllSurfacesFromShadow( ShadowHandle_t handle )
{
	// Iterate over all the decals associated with a particular shadow
	// Remove the decals from the surfaces they are associated with
	ShadowSurfaceIndex_t i = m_Shadows[handle].m_FirstDecal;
	ShadowSurfaceIndex_t next;
	while ( i != m_ShadowSurfaces.InvalidIndex() )
	{
		ShadowDecalHandle_t decalHandle = m_ShadowSurfaces[i];

		next = m_ShadowSurfaces.Next(i);

		RemoveShadowDecalFromSurface( m_ShadowDecals[decalHandle].m_SurfID, decalHandle );

		i = next;
	}

	m_Shadows[handle].m_FirstDecal = m_ShadowSurfaces.InvalidIndex();
}

void CShadowMgr::RemoveAllShadowsFromSurface( SurfaceHandle_t surfID )
{
	// Iterate over all the decals associated with a particular shadow
	// Remove the decals from the surfaces they are associated with
	ShadowDecalHandle_t	dh = MSurf_ShadowDecals( surfID );
	while (dh != m_ShadowDecals.InvalidIndex() )
	{
		// Remove this shadow from the surface 
		ShadowDecalHandle_t next = m_ShadowDecals.Next(dh);

		// Remove the surface from the shadow
		RemoveShadowDecalFromSurface( m_ShadowDecals[dh].m_SurfID, dh );

		dh = next;
	}

	MSurf_ShadowDecals( surfID ) = m_ShadowDecals.InvalidIndex();
}


//-----------------------------------------------------------------------------
// Shadow/model association
//-----------------------------------------------------------------------------
void CShadowMgr::AddShadowToModel( ShadowHandle_t handle, ModelInstanceHandle_t model )
{
	// FIXME: Add culling here based on the model bbox
	// and the shadow bbox
	// FIXME:
	/*
	// Trivial bbox reject.
	Vector bbMin, bbMax;
	pDisp->GetBoundingBox( bbMin, bbMax );
	if( decalinfo->m_Position.x - decalinfo->m_Size < bbMax.x && decalinfo->m_Position.x + decalinfo->m_Size > bbMin.x && 
		decalinfo->m_Position.y - decalinfo->m_Size < bbMax.y && decalinfo->m_Position.y + decalinfo->m_Size > bbMin.y && 
		decalinfo->m_Position.z - decalinfo->m_Size < bbMax.z && decalinfo->m_Position.z + decalinfo->m_Size > bbMin.z )
	*/

	if ( model == MODEL_INSTANCE_INVALID )
	{
		// async data not loaded yet
		return;
	}

	if( r_flashlightrender.GetBool()==false )
		return;

	GrowSpew( m_ShadowsOnModels.NumAllocated(), m_ShadowsOnModelsMax, "m_ShadowsOnModels" );
	m_ShadowsOnModels.AddElementToBucket( model, handle );
}

void CShadowMgr::RemoveAllShadowsFromModel( ModelInstanceHandle_t model )
{
	if( model != MODEL_INSTANCE_INVALID )
	{
		m_ShadowsOnModels.RemoveBucket( model );
	}
}

int CShadowMgr::GetNumShadowsOnModel( ModelInstanceHandle_t instance )
{
	if (instance == MODEL_INSTANCE_INVALID || r_shadows.GetInt() == 0 || m_bShadowsDisabled )
	{
		// no shadows
		return 0;
	}

	int i = m_ShadowsOnModels.FirstElement( instance );
	int nCount = 0;

	// TODO: Add method to query num elements in a bucket to CUtlBidirectionalSet
	while ( i != m_ShadowsOnModels.InvalidIndex() )
	{
		nCount++;
		i = m_ShadowsOnModels.NextElement(i);
	}

	return nCount;
}

int CShadowMgr::GetShadowsOnModel( ModelInstanceHandle_t instance, ShadowHandle_t* pShadowArray, bool bNormalShadows, bool bFlashlightShadows )
{
	if (instance == MODEL_INSTANCE_INVALID || r_shadows.GetInt() == 0 || m_bShadowsDisabled )
	{
		// no shadows
		return 0;
	}

	int i = m_ShadowsOnModels.FirstElement( instance );
	int nCount = 0;

	while ( i != m_ShadowsOnModels.InvalidIndex() )
	{
		Shadow_t& shadow = m_Shadows[m_ShadowsOnModels.Element(i)];

		bool bFlashlight = ( ( shadow.m_Flags & ( SHADOW_FLASHLIGHT | SHADOW_SIMPLE_PROJECTION ) ) != 0 );
		if ( ( bFlashlight && bFlashlightShadows ) || 
			 ( !bFlashlight && bNormalShadows ) )
		{
			*pShadowArray = m_ShadowsOnModels.Element(i);
			pShadowArray++;
			nCount++;
		}

		i = m_ShadowsOnModels.NextElement(i);
	}

	return nCount;
}

void CShadowMgr::RemoveAllModelsFromShadow( ShadowHandle_t handle )
{
	m_ShadowsOnModels.RemoveElement( handle );
}						   


//-----------------------------------------------------------------------------
// Shadow state...
//-----------------------------------------------------------------------------
void CShadowMgr::SetModelShadowState( ModelInstanceHandle_t instance )
{
#ifndef DEDICATED
	VPROF_( "CShadowMgr::SetModelShadowState", 2, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	g_pStudioRender->ClearAllShadows();
	if ( ( instance == MODEL_INSTANCE_INVALID ) || ( r_shadows.GetInt() == 0 ) || m_bShadowsDisabled )
		return;

	bool bWireframe = r_shadowwireframe.GetBool();
	for ( int i = m_ShadowsOnModels.FirstElement( instance ); i != m_ShadowsOnModels.InvalidIndex(); i = m_ShadowsOnModels.NextElement( i ) )
	{
		Shadow_t& shadow = m_Shadows[m_ShadowsOnModels.Element(i)];

		if ( shadow.m_Flags & ( SHADOW_FLASHLIGHT | SHADOW_SIMPLE_PROJECTION ) )
		{
			if ( m_FlashlightStates[ shadow.m_FlashlightHandle ].m_nSplitscreenOwner >= 0 )
			{
				ASSERT_LOCAL_PLAYER_RESOLVABLE();
				if ( m_FlashlightStates[ shadow.m_FlashlightHandle ].m_nSplitscreenOwner != GET_ACTIVE_SPLITSCREEN_SLOT() )
				{
					continue;
				}
			}
		}

		if ( !bWireframe )
		{
			if ( shadow.m_Flags & ( SHADOW_FLASHLIGHT | SHADOW_SIMPLE_PROJECTION ) )
			{
				// NULL means that the models material should be used.
				// This is what we want in the case of the flashlight
				// since we need to render the models material again with different lighting.
				// Need to add something here to specify which flashlight.
				g_pStudioRender->AddShadow( NULL, NULL, &m_FlashlightStates[shadow.m_FlashlightHandle].m_FlashlightState, &shadow.m_WorldToShadow, shadow.m_pFlashlightDepthTexture );
			}
			else if ( r_shadows_gamecontrol.GetInt() != 0 )
			{
				g_pStudioRender->AddShadow( shadow.m_pModelMaterial, shadow.m_pBindProxy );
			}
		}
		else if ( ( shadow.m_Flags & ( SHADOW_FLASHLIGHT | SHADOW_SIMPLE_PROJECTION ) ) || r_shadows_gamecontrol.GetInt() != 0 )
		{
			g_pStudioRender->AddShadow( g_pMaterialMRMWireframe, NULL );
		}
	}
#endif
}

bool CShadowMgr::ModelHasShadows( ModelInstanceHandle_t instance )
{
	if ( instance != MODEL_INSTANCE_INVALID )
	{
		if ( m_ShadowsOnModels.FirstElement(instance) != m_ShadowsOnModels.InvalidIndex() )
			return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Builds shadow state information for a set of instances
//-----------------------------------------------------------------------------
int CShadowMgr::SetupFlashlightRenderInstanceInfo( ShadowHandle_t *pShadowHandle, uint32 *pModelUsageMask, int nUsageStride, int nInstanceCount, const ModelInstanceHandle_t *pInstance )
{
	CMatRenderContextPtr pRenderContext( materials );
	int nFlashlightCount = 0;
#ifndef DEDICATED
	if ( ( SinglePassFlashlightModeEnabled() && !pRenderContext->IsCullingEnabledForSinglePassFlashlight() ) || !r_shadows.GetInt() )
		return 0;

	for ( int i = 0; i < nInstanceCount; ++i, pModelUsageMask = (uint32*)( (char*)pModelUsageMask + nUsageStride ) )
	{
		ModelInstanceHandle_t instance = pInstance[i];
		*pModelUsageMask = 0;
		if ( instance == MODEL_INSTANCE_INVALID )
			continue;

		int j = m_ShadowsOnModels.FirstElement( instance );
		for ( ; j != m_ShadowsOnModels.InvalidIndex(); j = m_ShadowsOnModels.NextElement(j) )
		{
			ShadowHandle_t hFlashlight = m_ShadowsOnModels.Element(j);
			Shadow_t& shadow = m_Shadows[ hFlashlight ];
			if ( ( shadow.m_Flags & ( SHADOW_FLASHLIGHT | SHADOW_SIMPLE_PROJECTION ) ) == 0 )
				continue;

			if ( m_FlashlightStates[ shadow.m_FlashlightHandle ].m_nSplitscreenOwner >= 0 )
			{
				ASSERT_LOCAL_PLAYER_RESOLVABLE();
				if ( m_FlashlightStates[ shadow.m_FlashlightHandle ].m_nSplitscreenOwner != GET_ACTIVE_SPLITSCREEN_SLOT() )
					continue;
			}

			// FIXME: is there a faster method?
			int nFlashlightIndex;
			for ( nFlashlightIndex = 0; nFlashlightIndex < nFlashlightCount; ++nFlashlightIndex )
			{
				if ( pShadowHandle[nFlashlightIndex] == hFlashlight )
					break;
			}

			// Indicate the model is using this flashlight
			*pModelUsageMask |= (1 << nFlashlightIndex);

			if ( nFlashlightIndex != nFlashlightCount )
				continue;

			// Flashlight not found, add unique flashlight
			pShadowHandle[nFlashlightIndex] = hFlashlight;
			++nFlashlightCount;
		}
	}
#endif
	return nFlashlightCount;
}


void CShadowMgr::GetFlashlightRenderInfo( FlashlightInstance_t *pFlashlightState, int nCount, const ShadowHandle_t *pHandles )
{
	bool bWireframe = r_shadowwireframe.GetBool();
	for ( int i = 0; i < nCount; ++i )
	{
		const Shadow_t& shadow = m_Shadows[ pHandles[i] ];
		FlashlightInstance_t &flashlight = pFlashlightState[i];
		flashlight.m_FlashlightState = m_FlashlightStates[ shadow.m_FlashlightHandle ].m_FlashlightState;
		flashlight.m_WorldToTexture = shadow.m_WorldToShadow; 
		flashlight.m_pDebugMaterial = bWireframe ? g_pMaterialMRMWireframe : NULL;
		flashlight.m_pFlashlightDepthTexture = shadow.m_pFlashlightDepthTexture;
	}
}


//-----------------------------------------------------------------------------
// Applies the shadow to a surface
//-----------------------------------------------------------------------------
void CShadowMgr::ApplyShadowToSurface( ShadowBuildInfo_t& build, SurfaceHandle_t surfID )
{
	// We've found a potential surface to add to the shadow
	// At this point, we want to do fast culling to see whether we actually
	// should apply the shadow or not before actually adding it to any lists

	// FIXME: implement
	// Put the texture extents into shadow space; see if there's an intersection
	// If not, we can early out


	// To do this, we're gonna want to project the surface into the space of the decal
	// Therefore, we want to produce a surface->world transformation, and a
	// world->shadow/light space transformation
	// Then we transform the surface points into shadow space and apply the projection
	// in shadow space.

	/*
	// Get the texture associated with this surface
	mtexinfo_t* tex = pSurface->texinfo;

	Vector4D &textureU = tex->textureVecsTexelsPerWorldUnits[0];
	Vector4D &textureV = tex->textureVecsTexelsPerWorldUnits[1];

	// project decal center into the texture space of the surface
	float s = DotProduct( decalinfo->m_Position, textureU.AsVector3D() ) + 
		textureU.w - surf->textureMins[0];
	float t = DotProduct( decalinfo->m_Position, textureV.AsVector3D() ) + 
		textureV.w - surf->textureMins[1];
	*/

	// Don't do any more computation at the moment, only do it if
	// we end up rendering the surface later on
	AddSurfaceToShadow( build.m_Shadow, surfID );
}


//-----------------------------------------------------------------------------
// Applies the shadow to a displacement
//-----------------------------------------------------------------------------
void CShadowMgr::ApplyShadowToDisplacement( ShadowBuildInfo_t& build, IDispInfo *pDispInfo, bool bIsFlashlight )
{
	// Avoid noshadow displacements
	if ( !bIsFlashlight && ( MSurf_Flags( pDispInfo->GetParent() ) & SURFDRAW_NOSHADOWS ) )
		return;

	// Trivial bbox reject.
	Vector bbMin, bbMax;
	pDispInfo->GetBoundingBox( bbMin, bbMax );
	if ( !bIsFlashlight )
	{
		if ( !IsBoxIntersectingSphere( bbMin, bbMax, build.m_vecSphereCenter, build.m_flSphereRadius ) )
			return;
	}
	else
	{
		if( GetFlashlightFrustum( build.m_Shadow ).CullBox( bbMin, bbMax ) )
			return;
	}

	SurfaceHandle_t surfID = pDispInfo->GetParent();

	if ( surfID->m_bDynamicShadowsEnabled == false && !bIsFlashlight )
		return;

	AddSurfaceToShadow( build.m_Shadow, surfID );
}


//-----------------------------------------------------------------------------
// Allows us to disable particular shadows
//-----------------------------------------------------------------------------
void CShadowMgr::EnableShadow( ShadowHandle_t handle, bool bEnable )
{
	if (!bEnable)
	{
		// We need to remove the shadow from all surfaces it may currently be in
		RemoveAllSurfacesFromShadow( handle );
		RemoveAllModelsFromShadow( handle );

		m_Shadows[handle].m_Flags |= SHADOW_DISABLED;
	}
	else
	{
		// FIXME: Could make this recompute the cache...
		m_Shadows[handle].m_Flags &= ~SHADOW_DISABLED;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the darkness falloff bias
// Input  : shadow - 
//			ucBias - 
//-----------------------------------------------------------------------------
void CShadowMgr::SetFalloffBias( ShadowHandle_t shadow, unsigned char ucBias )
{
	m_Shadows[shadow].m_FalloffBias = ucBias;
}

//-----------------------------------------------------------------------------
// Recursive routine to find surface to apply a decal to.  World coordinates of 
// the decal are passed in r_recalpos like the rest of the engine.  This should 
// be called through R_DecalShoot()
//-----------------------------------------------------------------------------
void CShadowMgr::ProjectShadow( ShadowHandle_t handle, const Vector &origin,
	const Vector& projectionDir, const VMatrix& worldToShadow, const Vector2D& size,
	int nLeafCount, const int *pLeafList,
	float maxHeight, float falloffOffset, float falloffAmount, const Vector &vecCasterOrigin )
{
	VPROF_BUDGET( "CShadowMgr::ProjectShadow", VPROF_BUDGETGROUP_SHADOW_RENDERING );

	// First, we need to remove the shadow from all surfaces it may
	// currently be in; in other words we're invalidating the shadow surface cache
	RemoveAllSurfacesFromShadow( handle );
	RemoveAllModelsFromShadow( handle );

	// Don't bother with this shadow if it's disabled
	Shadow_t &shadow = m_Shadows[handle];
	if ( shadow.m_Flags & SHADOW_DISABLED )
		return;

	// Don't compute the surface cache if shadows are off..
	if ( !r_shadows.GetInt() || m_bShadowsDisabled )
		return;

	// Set the falloff coefficient
	shadow.m_FalloffOffset = falloffOffset;
	VectorCopy( projectionDir, shadow.m_ProjectionDir );

	// We need to know about surfaces in leaves hit by the ray...
	// We'd like to stop iterating as soon as the entire swept volume
	// enters a solid leaf; that may be hard to determine. Instead,
	// we should stop iterating when the ray center enters a solid leaf?
	AssertFloatEquals( projectionDir.LengthSqr(), 1.0f, 1e-3 );

	// The maximum ray distance is equal to the distance it takes the
	// falloff to get to 15%.
	shadow.m_MaxDist = maxHeight; //sqrt( coeff / 0.10f ) + falloffOffset;
	shadow.m_FalloffAmount = falloffAmount;
	MatrixCopy( worldToShadow, shadow.m_WorldToShadow );

	// Compute a rough bounding sphere for the ray
	float flRadius = sqrt( size.x * size.x + size.y * size.y ) * 0.5f;
	VectorMA( origin, 0.5f * maxHeight, projectionDir, shadow.m_vecSphereCenter );
	shadow.m_flSphereRadius = 0.5f * maxHeight + flRadius;

	Vector vecEndPoint;
	Vector vecMins(	-flRadius, -flRadius, -flRadius );
	Vector vecMaxs(	 flRadius,  flRadius,  flRadius );
	VectorMA( origin, maxHeight, projectionDir, vecEndPoint );
	shadow.m_Ray.Init( origin, vecEndPoint, vecMins, vecMaxs );

	// No more work necessary if it hits no leaves
	if ( nLeafCount == 0 )
		return;

	// We're hijacking the surface vis frame to make sure we enumerate
	// surfaces only once; 
	++r_surfacevisframe;

	// Clear out the displacement tags also
	DispInfo_ClearAllTags( host_state.worldbrush->hDispInfos );

	ShadowBuildInfo_t build;
	build.m_Shadow = handle;
	build.m_RayStart = origin;
	build.m_pVis = NULL;
	build.m_vecSphereCenter = shadow.m_vecSphereCenter;
	build.m_flSphereRadius = shadow.m_flSphereRadius;
	VectorCopy( projectionDir, build.m_ProjectionDirection );

	// Enumerate leaves
	for ( int i  = 0; i < nLeafCount; ++i )
	{
		// NOTE: Scope specifier eliminates virtual function call
		CShadowMgr::EnumerateLeaf( pLeafList[i], (intp)&build );
	}
}

void DrawFrustum( Frustum_t &frustum )
{
	const int maxPoints = 8;
	int i;
	for( i = 0; i < FRUSTUM_NUMPLANES; i++ )
	{
		Vector points[maxPoints];
		Vector points2[maxPoints];
		Vector normal;
		float dist;
		frustum.GetPlane( i, &normal, &dist );
		int numPoints = PolyFromPlane( points, normal, dist );
		Assert( numPoints <= maxPoints );
		Vector *in, *out;
		in = points;
		out = points2;
		int j;
		for( j = 0; j < FRUSTUM_NUMPLANES; j++ )
		{
			if( i == j )
			{
				continue;
			}
			frustum.GetPlane( j, &normal, &dist );
			numPoints = ClipPolyToPlane( in, numPoints, out, normal, dist );
			Assert( numPoints <= maxPoints );
			V_swap( in, out );
		}
		int c;
		for( c = 0; c < numPoints; c++ )
		{
			CDebugOverlay::AddLineOverlay( in[c], in[(c+1)%numPoints], 0, 255, 0, 255, true, 0.0f );
		}
	}
}

//static void LineDrawHelper( const Vector &startShadowSpace, const Vector &endShadowSpace, 
//						   const VMatrix &shadowToWorld, unsigned char r, unsigned char g, 
//						   unsigned char b, bool ignoreZ )
//{
//	Vector startWorldSpace, endWorldSpace;
//	Vector3DMultiplyPositionProjective( shadowToWorld, startShadowSpace, startWorldSpace );
//	Vector3DMultiplyPositionProjective( shadowToWorld, endShadowSpace, endWorldSpace );
//
//	CDebugOverlay::AddLineOverlay( startWorldSpace, 
//		endWorldSpace, 
//		r, g, b, ignoreZ
//		, 0.0 );
//}

void CShadowMgr::ProjectFlashlight( ShadowHandle_t handle, const VMatrix& worldToShadow, int nLeafCount, const int *pLeafList )
{
	VPROF_BUDGET( "CShadowMgr::ProjectFlashlight", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

	CMatRenderContextPtr pRenderContext( materials );
	Shadow_t& shadow = m_Shadows[handle];

	if ( !m_bSinglePassFlashlightStateEnabled || pRenderContext->IsCullingEnabledForSinglePassFlashlight() )
	{
		// First, we need to remove the shadow from all surfaces it may
		// currently be in; in other words we're invalidating the shadow surface cache
		RemoveAllSurfacesFromShadow( handle );
		RemoveAllModelsFromShadow( handle );

		m_FlashlightStates[ shadow.m_FlashlightHandle ].m_OccluderBuckets.Flush();
	}

	// Don't bother with this shadow if it's disabled
	if ( m_Shadows[handle].m_Flags & SHADOW_DISABLED )
		return;

	// Don't compute the surface cache if shadows are off..
	if ( !r_shadows.GetInt() )
		return;

	MatrixCopy( worldToShadow, shadow.m_WorldToShadow );

	// We need this for our various bounding computations
	VMatrix shadowToWorld;
	MatrixInverseGeneral( shadow.m_WorldToShadow, shadowToWorld );

	// Set up the frustum for the flashlight so that we can cull each leaf against it.
	Assert( shadow.m_Flags & ( SHADOW_FLASHLIGHT | SHADOW_SIMPLE_PROJECTION ) );
	Frustum_t &frustum = m_FlashlightStates[shadow.m_FlashlightHandle].m_Frustum;
	FrustumPlanesFromMatrix( shadowToWorld, frustum );
	CalculateSphereFromProjectionMatrixInverse( shadowToWorld, &shadow.m_vecSphereCenter, &shadow.m_flSphereRadius );

	if ( nLeafCount == 0 )
		return;

	// Don't need to walk the surface list w/ single pass flashlights
	if ( m_bSinglePassFlashlightStateEnabled )
		return;

	// We're hijacking the surface vis frame to make sure we enumerate
	// surfaces only once; 
	++r_surfacevisframe;

	// Clear out the displacement tags also
	DispInfo_ClearAllTags( host_state.worldbrush->hDispInfos );

	ShadowBuildInfo_t build;
	build.m_Shadow = handle;
	build.m_RayStart = m_FlashlightStates[shadow.m_FlashlightHandle].m_FlashlightState.m_vecLightOrigin;
	build.m_pVis = NULL;
	build.m_vecSphereCenter = shadow.m_vecSphereCenter;
	build.m_flSphereRadius = shadow.m_flSphereRadius;
	
	for ( int i = 0; i < nLeafCount; ++i )
	{
		// NOTE: Scope specifier eliminates virtual function call
		CShadowMgr::EnumerateLeaf( pLeafList[i], (intp)&build );
	}
}


//-----------------------------------------------------------------------------
// Applies the flashlight to all surfaces in the leaf
//-----------------------------------------------------------------------------
void CShadowMgr::ApplyFlashlightToLeaf( const Shadow_t &shadow, mleaf_t* pLeaf, ShadowBuildInfo_t* pBuild )
{
	// Get the bounds of the leaf so that we can test it against the flashlight frustum.
	Vector leafMins, leafMaxs;
	VectorAdd( pLeaf->m_vecCenter, pLeaf->m_vecHalfDiagonal, leafMaxs );
	VectorSubtract( pLeaf->m_vecCenter, pLeaf->m_vecHalfDiagonal, leafMins );

	// The flashlight frustum didn't intersect the bounding box for this leaf!  Get outta here!
	if(  GetFlashlightFrustum( pBuild->m_Shadow ).CullBox( leafMins, leafMaxs ) )
		return;

	// Iterate over all surfaces in the leaf, check for backfacing
	// and apply the shadow to the surface if it's not backfaced.
	// Note that this really only indicates that the shadow may potentially
	// sit on the surface; when we render, we'll actually do the clipping
	// computation and at that point we'll remove surfaces that don't
	// actually hit the surface

	bool bCullDepth = r_flashlightculldepth.GetBool();

	SurfaceHandle_t *pHandle = &host_state.worldbrush->marksurfaces[pLeaf->firstmarksurface];
	for ( int i = 0; i < pLeaf->nummarksurfaces; i++ )
	{
		SurfaceHandle_t surfID = pHandle[i];
		
		// only process each surface once;
		if( MSurf_VisFrame( surfID ) == r_surfacevisframe )
			continue;
		
		MSurf_VisFrame( surfID ) = r_surfacevisframe;
		Assert( !MSurf_DispInfo( surfID ) );
		
		// perspective projection

		// world-space vertex
		int vertIndex = host_state.worldbrush->vertindices[MSurf_FirstVertIndex( surfID )];
		Vector& worldPos = host_state.worldbrush->vertexes[vertIndex].position;

		// Get the lookdir
		Vector lookdir;
		VectorSubtract( worldPos, pBuild->m_RayStart, lookdir );
		VectorNormalize( lookdir );

		const cplane_t &surfPlane = MSurf_Plane( surfID );

		// Now apply the spherical cull
		float flDist = DotProduct( surfPlane.normal, pBuild->m_vecSphereCenter ) - surfPlane.dist;
		if ( fabs(flDist) >= pBuild->m_flSphereRadius )
			continue;

		ApplyShadowToSurface( *pBuild, surfID );

		// Backface cull

		if( bCullDepth )
		{
			if ( (MSurf_Flags( surfID ) & SURFDRAW_NOCULL) == 0 )
			{
				if ( DotProduct(surfPlane.normal, lookdir) < SHADOW_BACKFACE_EPSILON )
					continue;
			}
			else
			{
				// Avoid edge-on shadows regardless.
				float dot = DotProduct(surfPlane.normal, lookdir);
				if (fabs(dot) < SHADOW_BACKFACE_EPSILON)
				continue;
			}
		}

		FlashlightInfo_t &flashlightInfo = m_FlashlightStates[ shadow.m_FlashlightHandle ];
  		flashlightInfo.m_OccluderBuckets.AddElement( MSurf_MaterialSortID( surfID ), surfID );
	}
}


//-----------------------------------------------------------------------------
// Applies a shadow to all surfaces in the leaf
//-----------------------------------------------------------------------------
void CShadowMgr::ApplyShadowToLeaf( const Shadow_t &shadow, mleaf_t* RESTRICT pLeaf, ShadowBuildInfo_t* RESTRICT pBuild )
{
	// Iterate over all surfaces in the leaf, check for backfacing
	// and apply the shadow to the surface if it's not backfaced.
	// Note that this really only indicates that the shadow may potentially
	// sit on the surface; when we render, we'll actually do the clipping
	// computation and at that point we'll remove surfaces that don't
	// actually hit the surface
	SurfaceHandle_t *pHandle = &host_state.worldbrush->marksurfaces[pLeaf->firstmarksurface];
	for ( int i = 0; i < pLeaf->nummarksurfaces; i++ )
	{
		SurfaceHandleRestrict_t surfID = pHandle[i];
		
		// only process each surface once;
		if( MSurf_VisFrame( surfID ) == r_surfacevisframe )
			continue;
		
		MSurf_VisFrame( surfID ) = r_surfacevisframe;
		Assert( !MSurf_DispInfo( surfID ) );

		// If this surface has specifically had dynamic shadows disabled on it, then get out!
		if ( !MSurf_AreDynamicShadowsEnabled( surfID ) )
			continue;
		
		// Backface cull
		const cplane_t * RESTRICT pSurfPlane = &MSurf_Plane( surfID );
		bool bInFront;
		if ( (MSurf_Flags( surfID ) & SURFDRAW_NOCULL) == 0 )
		{
			if ( DotProduct( pSurfPlane->normal, pBuild->m_ProjectionDirection) > -SHADOW_BACKFACE_EPSILON )
				continue;
			
			bInFront = true;
		}
		else
		{
			// Avoid edge-on shadows regardless.
			float dot = DotProduct( pSurfPlane->normal, pBuild->m_ProjectionDirection );
			if (fabs(dot) < SHADOW_BACKFACE_EPSILON)
				continue;
			
			bInFront = (dot < 0); 
		}
		
		// Here, it's front facing...
		// Discard stuff on the wrong side of the ray start
		if (bInFront)
		{
			if ( DotProduct( pSurfPlane->normal, pBuild->m_RayStart) < pSurfPlane->dist )
				continue;
		}
		else
		{
			if ( DotProduct( pSurfPlane->normal, pBuild->m_RayStart) > pSurfPlane->dist )
				continue;
		}

		// Now apply the spherical cull
		float flDist = DotProduct( pSurfPlane->normal, pBuild->m_vecSphereCenter ) - pSurfPlane->dist;
		if ( fabs(flDist) >= pBuild->m_flSphereRadius )
			continue;

		ApplyShadowToSurface( *pBuild, surfID );
	}
}


#define BIT_SET( a, b ) ((a)[(b)>>3] & (1<<((b)&7)))

//-----------------------------------------------------------------------------
// Applies a projected texture to all surfaces in the leaf
//-----------------------------------------------------------------------------
bool CShadowMgr::EnumerateLeaf( int leaf, intp context )
{
	VPROF( "CShadowMgr::EnumerateLeaf" );
	ShadowBuildInfo_t* pBuild = (ShadowBuildInfo_t*)context;

	// Skip this leaf if it's not visible from the shadow caster
	if ( pBuild->m_pVis )
	{
		int cluster = CM_LeafCluster( leaf );
		if ( !BIT_SET( pBuild->m_pVis, cluster ) )
			return true;
	}

	const Shadow_t &shadow = m_Shadows[pBuild->m_Shadow];
	
	mleaf_t* pLeaf = &host_state.worldbrush->leafs[leaf];

	bool bIsFlashlight;
	if( shadow.m_Flags & ( SHADOW_FLASHLIGHT | SHADOW_SIMPLE_PROJECTION ) )
	{
		bIsFlashlight = true;
		ApplyFlashlightToLeaf( shadow, pLeaf, pBuild );
	}
	else
	{
		bIsFlashlight = false;
		ApplyShadowToLeaf( shadow, pLeaf, pBuild );
	}

	// Add the decal to each displacement in the leaf it touches.
	for ( int i = 0; i < pLeaf->dispCount; i++ )
	{
		IDispInfo *pDispInfo = MLeaf_Disaplcement( pLeaf, i );
		
		// Make sure the decal hasn't already been added to it.
		if( pDispInfo->GetTag() )
			continue;

		pDispInfo->SetTag();
		
		ApplyShadowToDisplacement( *pBuild, pDispInfo, bIsFlashlight );
	}
	
	return true;
}


//-----------------------------------------------------------------------------
// Adds a shadow to a brush model
//-----------------------------------------------------------------------------
void CShadowMgr::AddShadowToBrushModel( ShadowHandle_t handle, model_t* pModel, 	
									const Vector& origin, const QAngle& angles )
{
	// Don't compute the surface cache if shadows are off..
	if ( !r_shadows.GetInt() )
		return;

	const Shadow_t * RESTRICT pShadow = &m_Shadows[handle];

	// Transform the shadow ray direction into model space
	Vector shadowDirInModelSpace;
	bool bIsFlashlight = ( pShadow->m_Flags & ( SHADOW_FLASHLIGHT | SHADOW_SIMPLE_PROJECTION ) ) != 0;
	if( !bIsFlashlight )
	{
		// FLASHLIGHTFIXME: should do backface culling for projective light sources.
		matrix3x4_t worldToModel;
		AngleIMatrix( angles, worldToModel );
		VectorRotate( pShadow->m_ProjectionDir, worldToModel, shadowDirInModelSpace );
	}

	// Just add all non-backfacing brush surfaces to the list of potential
	// surfaces that we may be casting a shadow onto.
	SurfaceHandleRestrict_t surfID = SurfaceHandleFromIndex( pModel->brush.firstmodelsurface, pModel->brush.pShared );
	for (int i=0; i<pModel->brush.nummodelsurfaces; ++i, ++surfID)
	{
		// Don't bother with nodraw surfaces
		int nFlags = MSurf_Flags( surfID );
		if ( nFlags & SURFDRAW_NODRAW )
			continue;
			
		if( !bIsFlashlight )
		{
			// FLASHLIGHTFIXME: should do backface culling for projective light sources.
			// Don't bother with backfacing surfaces
			if ( (nFlags & SURFDRAW_NOCULL) == 0 )
			{
				const cplane_t * RESTRICT pSurfPlane = &MSurf_Plane( surfID );
				float dot = DotProduct( shadowDirInModelSpace, pSurfPlane->normal );
				if ( dot > 0 )
					continue;
			}
		}

		// FIXME: We may want to do some more high-level per-surface culling
		// If so, it'll be added to ApplyShadowToSurface. Call it instead.
		AddSurfaceToShadow( handle, surfID );
	}
}

				 
//-----------------------------------------------------------------------------
// Removes all shadows from a brush model
//-----------------------------------------------------------------------------
void CShadowMgr::RemoveAllShadowsFromBrushModel( model_t* pModel )
{
	SurfaceHandle_t surfID = SurfaceHandleFromIndex( pModel->brush.firstmodelsurface, pModel->brush.pShared );
	for (int i=0; i<pModel->brush.nummodelsurfaces; ++i, ++surfID)
	{
		RemoveAllShadowsFromSurface( surfID );
	}
}


//-----------------------------------------------------------------------------
// Adds the shadow decals on the surface to a queue of things to render
//-----------------------------------------------------------------------------
void CShadowMgr::AddShadowsOnSurfaceToRenderList( ShadowDecalHandle_t decalHandle )
{
	// Don't compute the surface cache if shadows are off..
	if  ( !r_shadows.GetInt() )
		return;

	// Add all surface decals into the appropriate render lists
	while ( decalHandle != m_ShadowDecals.InvalidIndex() )
	{
		ShadowDecal_t& shadowDecal = m_ShadowDecals[decalHandle];

		if ( m_Shadows[shadowDecal.m_Shadow].m_Flags & ( SHADOW_FLASHLIGHT | SHADOW_SIMPLE_PROJECTION ) )
		{
			AddSurfaceToFlashlightMaterialBuckets( shadowDecal.m_Shadow, shadowDecal.m_SurfID );

			// We've got one more decal to render
			++m_DecalsToRender;
		}
		else if (	( r_shadows_gamecontrol.GetInt() != 0 ) &&
					( m_nSkipShadowForEntIndex != m_Shadows[shadowDecal.m_Shadow].m_nEntIndex ) )
		{
			// For shadow rendering, hook the decal into the render list based on the shadow material, not the surface material.
			int sortOrder = m_Shadows[shadowDecal.m_Shadow].m_SortOrder;
			m_ShadowDecals[decalHandle].m_NextRender = m_RenderQueue[sortOrder];
			m_RenderQueue[sortOrder] = decalHandle;

			// We've got one more decal to render
			++m_DecalsToRender;
		}
		decalHandle = m_ShadowDecals.Next(decalHandle);
	}
}

void CShadowMgr::ClearShadowRenderList()
{
	COMPILE_TIME_ASSERT( sizeof(ShadowDecalHandle_t) == 2 );

	// Clear out the render list
	if (m_RenderQueue.Count() > 0)
	{
		memset( m_RenderQueue.Base(), 0xFF, m_RenderQueue.Count() * sizeof(ShadowDecalHandle_t) );
	}
	m_DecalsToRender = 0;
	// Clear all lists pertaining to flashlight decals that need to be rendered.
	ClearAllFlashlightMaterialBuckets();
}

void CShadowMgr::RenderShadows( IMatRenderContext *pRenderContext, const VMatrix* pModelToWorld )
{
	VPROF_BUDGET( "CShadowMgr::RenderShadows", VPROF_BUDGETGROUP_SHADOW_RENDERING );
	// Iterate through all sort ids and render for regular shadows, which get their materials from the shadow material.
	
	#if PIX_ENABLE
		bool bFound = false;
	#endif

	for ( int i = 0; i < m_RenderQueue.Count(); i++ )
	{
		if ( m_RenderQueue[i] != m_ShadowDecals.InvalidIndex() )
		{
			#if PIX_ENABLE
				if ( !bFound )
				{
					bFound = true;
					pRenderContext->BeginPIXEvent( PIX_VALVE_ORANGE, "DECAL_SHADOWS" );
				}
			#endif

			RenderShadowList( pRenderContext, m_RenderQueue[i], pModelToWorld );
		}
	}

	#if PIX_ENABLE
		if ( bFound )
		{
			pRenderContext->EndPIXEvent();
		}
	#endif
}

void CShadowMgr::RenderProjectedTextures( IMatRenderContext *pRenderContext, const VMatrix* pModelToWorld )
{
	VPROF_BUDGET( "CShadowMgr::RenderProjectedTextures", VPROF_BUDGETGROUP_SHADOW_RENDERING );

	RenderFlashlights( true, false, pModelToWorld );
	RenderShadows( pRenderContext, pModelToWorld );

	// Clear out the render list, we've rendered it now
	ClearShadowRenderList();
}


//-----------------------------------------------------------------------------
// A 2D sutherland-hodgman clipper
//-----------------------------------------------------------------------------
class CClipTop
{
public:
	static inline bool Inside( ShadowVertex_t const& vert )				{ return vert.m_ShadowSpaceTexCoord.y < 1;}
	static inline float Clip( const Vector& one, const Vector& two )	{ return (1 - one.y) / (two.y - one.y);}
	static inline bool IsPlane()	{return false;}
	static inline bool IsAbove()	{return false;}
};

class CClipLeft
{
public:
	static inline bool Inside( ShadowVertex_t const& vert )				{ return vert.m_ShadowSpaceTexCoord.x > 0;}
	static inline float Clip( const Vector& one, const Vector& two )	{ return one.x / (one.x - two.x);}
	static inline bool IsPlane()	{return false;}
	static inline bool IsAbove()	{return false;}
};

class CClipRight
{
public:
	static inline bool Inside( ShadowVertex_t const& vert )				{return vert.m_ShadowSpaceTexCoord.x < 1;}
	static inline float Clip( const Vector& one, const Vector& two )	{return (1 - one.x) / (two.x - one.x);}
	static inline bool IsPlane()	{return false;}
	static inline bool IsAbove()	{return false;}
};

class CClipBottom
{
public:
	static inline bool Inside( ShadowVertex_t const& vert )				{return vert.m_ShadowSpaceTexCoord.y > 0;}
	static inline float Clip( const Vector& one, const Vector& two )	{return one.y / (one.y - two.y);}
	static inline bool IsPlane()	{return false;}
	static inline bool IsAbove()	{return false;}
};

class CClipAbove
{
public:
	static inline bool Inside( ShadowVertex_t const& vert )				{return vert.m_ShadowSpaceTexCoord.z > 0;}
	static inline float Clip( const Vector& one, const Vector& two )	{return one.z / (one.z - two.z);}
	static inline bool IsPlane()	{return false;}
	static inline bool IsAbove()	{return true;}
};

class CClipPlane
{
public:
	inline bool Inside( ShadowVertex_t const& vert )						
	{
		return DotProduct( vert.m_Position, *m_pNormal ) < m_Dist;
	}

	inline float Clip( const Vector& one, const Vector& two )	
	{
		Vector dir;
		VectorSubtract( two, one, dir );
		return IntersectRayWithPlane( one, dir, *m_pNormal, m_Dist );
	}

	static inline bool IsAbove()	{return false;}
	static inline bool IsPlane()	{return true;}

	void SetPlane( const Vector& normal, float dist )
	{
		m_pNormal = &normal;
		m_Dist = dist;
	}


private:
	const Vector *m_pNormal;
	float  m_Dist;
};

static inline void ClampTexCoord( ShadowVertex_t *pInVertex, ShadowVertex_t *pOutVertex )
{
	if ( fabs(pInVertex->m_ShadowSpaceTexCoord[0]) < 1e-3 )
		pOutVertex->m_ShadowSpaceTexCoord[0] = 0.0f;
	else if ( fabs(pInVertex->m_ShadowSpaceTexCoord[0] - 1.0f) < 1e-3 )
		pOutVertex->m_ShadowSpaceTexCoord[0] = 1.0f;

	if ( fabs(pInVertex->m_ShadowSpaceTexCoord[1]) < 1e-3 )
		pOutVertex->m_ShadowSpaceTexCoord[1] = 0.0f;
	else if ( fabs(pInVertex->m_ShadowSpaceTexCoord[1] - 1.0f) < 1e-3 )
		pOutVertex->m_ShadowSpaceTexCoord[1] = 1.0f;
}
template <class Clipper>
static inline void Intersect( ShadowVertex_t* pStart, ShadowVertex_t* pEnd, ShadowVertex_t* pOut, bool startInside, Clipper& clipper )
{
	// Clip the edge to the clip plane
	float t;
	if (!Clipper::IsPlane())
	{
		if (!Clipper::IsAbove())
		{
			// This is the path the we always take for perspective light volumes.
			t = clipper.Clip( pStart->m_ShadowSpaceTexCoord, pEnd->m_ShadowSpaceTexCoord );

			VectorLerp( pStart->m_ShadowSpaceTexCoord, pEnd->m_ShadowSpaceTexCoord, t, pOut->m_ShadowSpaceTexCoord );
		}
		else
		{
			t = clipper.Clip( pStart->m_ShadowSpaceTexCoord, pEnd->m_ShadowSpaceTexCoord );
			VectorLerp( pStart->m_ShadowSpaceTexCoord, pEnd->m_ShadowSpaceTexCoord, t, pOut->m_ShadowSpaceTexCoord );

			// This is a special thing we do here to avoid hard-edged shadows
			if (startInside)
				ClampTexCoord( pEnd, pOut );
			else
				ClampTexCoord( pStart, pOut );
		}
	}
	else
	{
		t = clipper.Clip( pStart->m_Position, pEnd->m_Position );
		VectorLerp( pStart->m_ShadowSpaceTexCoord, pEnd->m_ShadowSpaceTexCoord, t, pOut->m_ShadowSpaceTexCoord );
	}

	VectorLerp( pStart->m_Position, pEnd->m_Position, t, pOut->m_Position );
}

template <class Clipper>
static void ShadowClip( ShadowClipState_t& clip, Clipper& clipper )
{
	if ( clip.m_ClipCount == 0 )
		return;

	// Ye Olde Sutherland-Hodgman clipping algorithm
	int numOutVerts = 0;
	ShadowVertex_t** pSrcVert = (ShadowVertex_t **)clip.m_ppClipVertices[clip.m_CurrVert];
	ShadowVertex_t** pDestVert = (ShadowVertex_t **)clip.m_ppClipVertices[!clip.m_CurrVert];

	int numVerts = clip.m_ClipCount;
	ShadowVertex_t* pStart = pSrcVert[numVerts-1];
	bool startInside = clipper.Inside( *pStart );
	for (int i = 0; i < numVerts; ++i)
	{
		ShadowVertex_t* pEnd = pSrcVert[i];
		bool endInside = clipper.Inside( *pEnd );
		if (endInside)
		{
			if (!startInside)
			{
				// Started outside, ended inside, need to clip the edge
				if ( clip.m_TempCount >= SHADOW_VERTEX_TEMP_COUNT )
					return;
				
				// Allocate a new clipped vertex 
				pDestVert[numOutVerts] = &clip.m_pTempVertices[clip.m_TempCount++];

				// Clip the edge to the clip plane
				Intersect( pStart, pEnd, pDestVert[numOutVerts], startInside, clipper ); 
				++numOutVerts;
			}
			pDestVert[numOutVerts++] = pEnd;
		}
		else
		{
			if (startInside)
			{
				// Started inside, ended outside, need to clip the edge
				if ( clip.m_TempCount >= SHADOW_VERTEX_TEMP_COUNT )
					return;

				// Allocate a new clipped vertex 
				pDestVert[numOutVerts] = &clip.m_pTempVertices[clip.m_TempCount++];

				// Clip the edge to the clip plane
				Intersect( pStart, pEnd, pDestVert[numOutVerts], startInside, clipper ); 
				++numOutVerts;
			}
		}
		pStart = pEnd;
		startInside = endInside;
	}

	// Switch source lists
	clip.m_CurrVert = 1 - clip.m_CurrVert;
	clip.m_ClipCount = numOutVerts;
	Assert( clip.m_ClipCount <= SHADOW_VERTEX_TEMP_COUNT ); 
}



//-----------------------------------------------------------------------------
// Project vertices into shadow space
//-----------------------------------------------------------------------------

// a version of this function where I promise that src1, dst, and src2 are all different
FORCEINLINE void Vector3DMultiplyPositionNoAlias( const VMatrix * RESTRICT src1, 
											const Vector * RESTRICT src2,
											Vector * RESTRICT dst )
{
	(*dst)[0] = (*src1)[0][0] * (*src2).x + (*src1)[0][1] * (*src2).y + (*src1)[0][2] * (*src2).z + (*src1)[0][3];
	(*dst)[1] = (*src1)[1][0] * (*src2).x + (*src1)[1][1] * (*src2).y + (*src1)[1][2] * (*src2).z + (*src1)[1][3];
	(*dst)[2] = (*src1)[2][0] * (*src2).x + (*src1)[2][1] * (*src2).y + (*src1)[2][2] * (*src2).z + (*src1)[2][3];
}
bool CShadowMgr::ProjectVerticesIntoShadowSpace( const VMatrix * RESTRICT modelToShadow, 
	float maxDist, int count, Vector** RESTRICT ppPosition, ShadowClipState_t& clip )
{
	bool insideVolume = false;

	// init to frustum box
	Vector mins( 1.0f, 1.0f, maxDist );
	Vector maxs( 0.0f, 0.0f, 0.0f );

	// Create vertices to clip to...
	for (int i = 0; i < count; ++i )
	{
		Assert( ppPosition[i] );

		Vector * RESTRICT pPos = ppPosition[i];
		VectorCopy( *pPos, clip.m_pTempVertices[i].m_Position );

		// Project the points into shadow texture space
		Vector * RESTRICT pShadowSpacePos = &clip.m_pTempVertices[i].m_ShadowSpaceTexCoord;
		Vector3DMultiplyPositionNoAlias( modelToShadow, pPos, pShadowSpacePos );

		// Update AABB for polygon
#ifdef _X360
		// This should be a little better for the 360 than VectorMin()/Max()
		mins.x = fsel( pShadowSpacePos->x - mins.x, mins.x, pShadowSpacePos->x );
		mins.y = fsel( pShadowSpacePos->y - mins.y, mins.x, pShadowSpacePos->y );
		mins.z = fsel( pShadowSpacePos->z - mins.z, mins.x, pShadowSpacePos->z );
		maxs.x = fsel( pShadowSpacePos->x - maxs.x, pShadowSpacePos->x, maxs.x );
		maxs.y = fsel( pShadowSpacePos->y - maxs.y, pShadowSpacePos->y, maxs.y );
		maxs.z = fsel( pShadowSpacePos->z - maxs.z, pShadowSpacePos->z, maxs.z );
#else
		VectorMin( mins, *pShadowSpacePos, mins );
		VectorMax( maxs, *pShadowSpacePos, maxs );
#endif
		// Set up clipping coords...
		clip.m_ppClipVertices[0][i] = &clip.m_pTempVertices[i];
	}

	// early out if AABB doesn't intersect frustum box
	insideVolume = !( ( mins.x >= 1.0f ) || ( maxs.x <= 0.0f ) || ( mins.y >= 1.0f ) ||
		( maxs.y <= 0.0f ) || ( mins.z >= maxDist ) || ( maxs.z <= 0.0f ) );

	clip.m_TempCount = clip.m_ClipCount = count;
	clip.m_CurrVert = 0;

	return insideVolume;
}

//-----------------------------------------------------------------------------
// Projects + clips shadows
//-----------------------------------------------------------------------------
int CShadowMgr::ProjectAndClipVertices( const Shadow_t& shadow, const VMatrix& worldToShadow,
						 const VMatrix *pWorldToModel, int count, Vector** ppPosition, ShadowVertex_t*** ppOutVertex, ShadowClipState_t& clip )
{
	VPROF( "ProjectAndClipVertices" );

	if ( !ProjectVerticesIntoShadowSpace( &worldToShadow, shadow.m_MaxDist, count, ppPosition, clip ) )
		return 0;

	// Clippers...
	CClipTop top;
	CClipBottom bottom;
	CClipLeft left;
	CClipRight right;
	CClipAbove above;
	CClipPlane plane;

	// Sutherland-hodgman clip
	ShadowClip( clip, top );
	ShadowClip( clip, bottom );
	ShadowClip( clip, left );
	ShadowClip( clip, right );
	if ( shadow.m_ClipPlaneCount == 0 )
	{
		// only clip above if we don't have any extra clip planes that prevent back-casting
		ShadowClip( clip, above );
	}

	// Planes to suppress back-casting
	for (int i = 0; i < shadow.m_ClipPlaneCount; ++i)
	{
		if ( pWorldToModel )
		{
			cplane_t worldPlane, modelPlane;
			worldPlane.normal = shadow.m_ClipPlane[i];
			worldPlane.dist = shadow.m_ClipDist[i]; 
			MatrixTransformPlane( *pWorldToModel, worldPlane, modelPlane );
			plane.SetPlane( modelPlane.normal, modelPlane.dist );
		}
		else
		{
			plane.SetPlane( shadow.m_ClipPlane[i], shadow.m_ClipDist[i] );
		}
		ShadowClip( clip, plane );
	}

	if (clip.m_ClipCount < 3)
		return 0;

	// Return a pointer to the array of clipped vertices...
	Assert(ppOutVertex);
	*ppOutVertex = (ShadowVertex_t **)clip.m_ppClipVertices[clip.m_CurrVert];
	return clip.m_ClipCount;
}


//-----------------------------------------------------------------------------
// Accessor for use by the displacements
//-----------------------------------------------------------------------------
int CShadowMgr::ProjectAndClipVertices( ShadowHandle_t handle, int count, 
	Vector** ppPosition, ShadowVertex_t*** ppOutVertex )
{
	static ShadowClipState_t clip;
	return ProjectAndClipVertices( m_Shadows[handle], 
		m_Shadows[handle].m_WorldToShadow, NULL, count, ppPosition, ppOutVertex, clip );
}

//-----------------------------------------------------------------------------
// Thread-safe version
//-----------------------------------------------------------------------------
int CShadowMgr::ProjectAndClipVerticesEx( ShadowHandle_t handle, int count, 
									 Vector** ppPosition, ShadowVertex_t*** ppOutVertex, ShadowClipState_t& clip )
{
	return ProjectAndClipVertices( m_Shadows[handle], 
		m_Shadows[handle].m_WorldToShadow, NULL, count, ppPosition, ppOutVertex, clip );
}

//-----------------------------------------------------------------------------
// Copies vertex info from the clipped vertices
//-----------------------------------------------------------------------------
// This version treats texcoords as Vector
inline void CShadowMgr::CopyClippedVertices( int count, ShadowVertex_t** ppSrcVert, ShadowVertex_t* pDstVert, const Vector &vToAdd )
{
	for (int i = 0; i < count; ++i)
	{
		pDstVert[i].m_Position = ppSrcVert[i]->m_Position + vToAdd;
		pDstVert[i].m_ShadowSpaceTexCoord = ppSrcVert[i]->m_ShadowSpaceTexCoord;

		// Make sure it's been clipped
		Assert( ppSrcVert[i]->m_ShadowSpaceTexCoord[0] >= -1e-3f );
		Assert( ppSrcVert[i]->m_ShadowSpaceTexCoord[0] - 1.0f <= 1e-3f );
		Assert( ppSrcVert[i]->m_ShadowSpaceTexCoord[1] >= -1e-3f );
		Assert( ppSrcVert[i]->m_ShadowSpaceTexCoord[1] - 1.0f <= 1e-3f );
	}
}


//-----------------------------------------------------------------------------
// Does the actual work of computing shadow vertices
//-----------------------------------------------------------------------------
bool CShadowMgr::ComputeShadowVertices( ShadowDecal_t& decal, 
	const VMatrix* pModelToWorld, const VMatrix *pWorldToModel, ShadowVertexCache_t* pVertexCache )
{
	VPROF( "CShadowMgr::ComputeShadowVertices" );
	// Prepare for the clipping
	Vector **ppVec = (Vector**)stackalloc( MSurf_VertCount( decal.m_SurfID ) * sizeof(Vector*) );
	for (int i = 0; i < MSurf_VertCount( decal.m_SurfID ); ++i )
	{
		int vertIndex = host_state.worldbrush->vertindices[MSurf_FirstVertIndex( decal.m_SurfID )+i];
		ppVec[i] = &host_state.worldbrush->vertexes[vertIndex].position;
	}

	// Compute the modelToShadow transform.
	// In the case of the world, just use worldToShadow...
	VMatrix* pModelToShadow = &m_Shadows[decal.m_Shadow].m_WorldToShadow;

	VMatrix temp;
	if ( pModelToWorld )
	{
		MatrixMultiply( *pModelToShadow, *pModelToWorld, temp );
		pModelToShadow = &temp;
	}
	else
	{
		pWorldToModel = NULL;
	}

	// Create vertices to clip to...
	ShadowVertex_t** ppSrcVert;
	ShadowClipState_t clip;
	int clipCount = ProjectAndClipVertices( m_Shadows[decal.m_Shadow], *pModelToShadow, pWorldToModel, 
		MSurf_VertCount( decal.m_SurfID ), ppVec, &ppSrcVert, clip );
	if (clipCount == 0)
	{
		pVertexCache->m_Count = 0;
		return false;
	}
	
	// Allocate the vertices we're going to use for the decal
	ShadowVertex_t* pDstVert = AllocateVertices( *pVertexCache, clipCount );
	Assert( pDstVert );

	// Copy the clipped vertices into the cache
	const Vector &vNormal = MSurf_Plane( decal.m_SurfID ).normal;
	CopyClippedVertices( clipCount, ppSrcVert, pDstVert, vNormal * OVERLAY_AVOID_FLICKER_NORMAL_OFFSET );

	// Indicate which shadow this is related to
	pVertexCache->m_Shadow = decal.m_Shadow;

	return true;
}



//-----------------------------------------------------------------------------
// Should we cache vertices?
//-----------------------------------------------------------------------------
inline bool CShadowMgr::ShouldCacheVertices( const ShadowDecal_t& decal )
{
	return (m_Shadows[decal.m_Shadow].m_Flags & SHADOW_CACHE_VERTS) != 0;
}


//-----------------------------------------------------------------------------
// Generates a list displacement shadow vertices to render
//-----------------------------------------------------------------------------
inline bool CShadowMgr::GenerateDispShadowRenderInfo( ShadowDecal_t& decal, ShadowRenderInfo_t& info )
{
	//=============================================================================
	// HPE_BEGIN:
	// [smessick] Added an overflow condition for the max disp decal cache.
	//=============================================================================
	if ( info.m_DispCount >= MAX_SHADOW_DECAL_CACHE_COUNT )
	{
		info.m_DispCount = MAX_SHADOW_DECAL_CACHE_COUNT;
		return true;
	}
	//=============================================================================
	// HPE_END
	//=============================================================================

	int v, i;
	if ( !MSurf_DispInfo( decal.m_SurfID )->ComputeShadowFragments( decal.m_DispShadow, v, i ) )
		return false;

	// Catch overflows....
	if ( ( info.m_VertexCount + v >= info.m_nMaxVertices ) || ( info.m_IndexCount + i >= info.m_nMaxIndices ) )
		return true;

	info.m_VertexCount += v;
	info.m_IndexCount += i;
	info.m_pDispCache[info.m_DispCount++] = decal.m_DispShadow;
	return true;
}


//-----------------------------------------------------------------------------
// Generates a list shadow vertices to render
//-----------------------------------------------------------------------------
inline bool CShadowMgr::GenerateNormalShadowRenderInfo( IMatRenderContext *pRenderContext, ShadowDecal_t& decal, ShadowRenderInfo_t& info )
{
	//=============================================================================
	// HPE_BEGIN:
	// [smessick] Check for cache overflow.
	//=============================================================================
	if ( info.m_Count >= MAX_SHADOW_DECAL_CACHE_COUNT )
	{
		info.m_Count = MAX_SHADOW_DECAL_CACHE_COUNT;
		return true;
	}
	//=============================================================================
	// HPE_END
	//=============================================================================

	// Look for a cache hit
	ShadowVertexCache_t* pVertexCache;
	if (decal.m_ShadowVerts != m_VertexCache.InvalidIndex())
	{
		// Ok, we've already computed the data, lets use it
		info.m_pCache[info.m_Count] = decal.m_ShadowVerts;
		pVertexCache = &m_VertexCache[decal.m_ShadowVerts];
	}
	else
	{
		// Attempt to cull the surface
		bool bIsNear = IsShadowNearSurface( decal.m_Shadow, decal.m_SurfID, info.m_pModelToWorld, &info.m_WorldToModel );
		if ( !bIsNear )
			return false;

		// In this case, we gotta recompute the shadow decal vertices
		// and maybe even store it into the cache....
		bool shouldCacheVerts = ShouldCacheVertices( decal );
		if (shouldCacheVerts)
		{
			GrowSpew( m_VertexCache.NumAllocated(), m_VertexCacheMax, "m_VertexCache" );
			decal.m_ShadowVerts = m_VertexCache.AddToTail();
			info.m_pCache[info.m_Count] = decal.m_ShadowVerts;
			pVertexCache = &m_VertexCache[decal.m_ShadowVerts];
		}
		else
		{
			GrowSpew( m_TempVertexCache.NumAllocated(), m_TempVertexCacheMax, "m_TempVertexCache" );
			int i = m_TempVertexCache.AddToTail();
			info.m_pCache[info.m_Count] = -i-1;
			pVertexCache = &m_TempVertexCache[i];
			Assert( info.m_pCache[info.m_Count] < 0 );
		}

		// Compute the shadow vertices
		// If no vertices were created, indicate this surface should be removed from the cache
		if ( !ComputeShadowVertices( decal, info.m_pModelToWorld, &info.m_WorldToModel, pVertexCache ) )
			return false;
	}

	// Catch overflows....
	int nAdditionalIndices = 3 * (pVertexCache->m_Count - 2);
	if ( ( info.m_VertexCount + pVertexCache->m_Count >= info.m_nMaxVertices ) || 
		( info.m_IndexCount + nAdditionalIndices >= info.m_nMaxIndices ) )
	{
		return true;
	}

	// Update vertex, index, and decal counts
	info.m_VertexCount += pVertexCache->m_Count;
	info.m_IndexCount += nAdditionalIndices;
	++info.m_Count;
	
	return true;
}


//-----------------------------------------------------------------------------
// Generates a list shadow vertices to render
//-----------------------------------------------------------------------------
void CShadowMgr::GenerateShadowRenderInfo( IMatRenderContext *pRenderContext, ShadowDecalHandle_t decalHandle, ShadowRenderInfo_t& info )
{
	VPROF_BUDGET( __FUNCTION__, VPROF_BUDGETGROUP_SHADOW_RENDERING );

	info.m_VertexCount = 0;
	info.m_IndexCount = 0;
	info.m_Count = 0;
	info.m_DispCount = 0;

	// Keep the lists only full of valid decals; that way we can preserve
	// the render lists in the case that we discover a shadow isn't needed.
	ShadowDecalHandle_t next;
	for ( ; decalHandle != m_ShadowDecals.InvalidIndex(); decalHandle = next )
	{
		ShadowDecal_t& decal = m_ShadowDecals[decalHandle];
		next = m_ShadowDecals[decalHandle].m_NextRender;
					 
		// Skip translucent shadows [ don't add their verts + indices to the render lists ]
		Shadow_t &shadow = m_Shadows[ decal.m_Shadow ];
		if ( shadow.m_FalloffBias == 255 )
			continue;

		bool keepShadow = true;
		if ( decal.m_DispShadow != DISP_SHADOW_HANDLE_INVALID )
		{
			// Handle shadows on displacements...
			keepShadow = GenerateDispShadowRenderInfo( decal, info );
		}
		else
		{
			// Handle shadows on normal surfaces
			keepShadow = GenerateNormalShadowRenderInfo( pRenderContext, decal, info );
		}
		
		// Retire the surface if the shadow didn't actually hit it
		if ( !keepShadow && ShouldCacheVertices( decal ) )
		{
			// If no triangles were generated
			// (the decal was completely clipped off)
			// In this case, remove the decal from the surface cache
			// so next time it'll be faster (for cached decals)
			RemoveShadowDecalFromSurface( decal.m_SurfID, decalHandle );
		}
	}
}

void CShadowMgr::GenerateShadowRenderInfoThreaded( IMatRenderContext *pRenderContext, ShadowDecalHandle_t decalHandle, ShadowRenderInfo_t& info )
{
	info.m_VertexCount = 0;
	info.m_IndexCount = 0;
	info.m_Count = 0;
	info.m_DispCount = 0;

	DispDecalWorkItem_t* pDispDecalWorkItems = static_cast<DispDecalWorkItem_t*>( stackalloc( m_DecalsToRender * sizeof( DispDecalWorkItem_t ) ) );
	int nNumDispDecals = 0;

	// Keep the lists only full of valid decals; that way we can preserve
	// the render lists in the case that we discover a shadow isn't needed.
	ShadowDecalHandle_t next;
	for ( ; decalHandle != m_ShadowDecals.InvalidIndex(); decalHandle = next )
	{
		ShadowDecal_t& decal = m_ShadowDecals[decalHandle];
		next = m_ShadowDecals[decalHandle].m_NextRender;

		// Skip translucent shadows [ don't add their verts + indices to the render lists ]
		Shadow_t &shadow = m_Shadows[ decal.m_Shadow ];
		if ( shadow.m_FalloffBias == 255 )
			continue;

		bool keepShadow = false;
		if ( decal.m_DispShadow != DISP_SHADOW_HANDLE_INVALID )
		{
			pDispDecalWorkItems[nNumDispDecals].h = decalHandle;
			nNumDispDecals++;
			continue;
		}
		else
		{
			// Handle shadows on normal surfaces
			keepShadow = GenerateNormalShadowRenderInfo( pRenderContext, decal, info );
		}

		// Retire the surface if the shadow didn't actually hit it
		if ( !keepShadow && ShouldCacheVertices( decal ) )
		{
			// If no triangles were generated
			// (the decal was completely clipped off)
			// In this case, remove the decal from the surface cache
			// so next time it'll be faster (for cached decals)
			RemoveShadowDecalFromSurface( decal.m_SurfID, decalHandle );
		}
	}

	// threaded displacement decal processing
	ParallelProcess( pDispDecalWorkItems, nNumDispDecals, this, &CShadowMgr::ProcessDispDecalWorkItem );

	// now go through list of results and do the additional processing from GenereateDispShadowRenderInfo
	for ( int i = 0; i < nNumDispDecals; i++ )
	{
		DispDecalWorkItem_t& wi = pDispDecalWorkItems[i];
		//ProcessDispDecalWorkItem( wi );

		ShadowDecal_t& decal = m_ShadowDecals[ wi.h ];

		if ( pDispDecalWorkItems[i].bKeepShadow )
		{
			// Catch overflows....
			if ( ( info.m_VertexCount + wi.vertCount >= info.m_nMaxVertices ) || ( info.m_IndexCount + wi.indexCount >= info.m_nMaxIndices ) )
				continue;

			info.m_VertexCount += wi.vertCount;
			info.m_IndexCount += wi.indexCount;
			info.m_pDispCache[info.m_DispCount++] = decal.m_DispShadow;
		}
		else
		{
			if ( ShouldCacheVertices( decal ) )
			{
				RemoveShadowDecalFromSurface( decal.m_SurfID, wi.h );
			}
		}
	}
}

void CShadowMgr::ProcessDispDecalWorkItem( DispDecalWorkItem_t& wi )
{
	// Handle shadows on displacements...
	ShadowDecal_t& decal = m_ShadowDecals[wi.h];
	wi.bKeepShadow = MSurf_DispInfo( decal.m_SurfID )->ComputeShadowFragments( decal.m_DispShadow, wi.vertCount, wi.indexCount );

	/*
	int v, i;
	if ( ! )
	return false;

	}	*/
}

//-----------------------------------------------------------------------------
// Computes information for rendering
//-----------------------------------------------------------------------------
void CShadowMgr::ComputeRenderInfo( ShadowDecalRenderInfo_t* pInfo, ShadowHandle_t handle ) const
{
	const ShadowInfo_t& i = m_Shadows[handle];
	pInfo->m_vTexOrigin = i.m_TexOrigin;
	pInfo->m_vTexSize = i.m_TexSize;
	pInfo->m_flFalloffOffset = 0.7f * i.m_FalloffOffset;	// pull the offset in a little to hide the shadow darkness discontinuity
	pInfo->m_flFalloffAmount = i.m_FalloffAmount;
	pInfo->m_flFalloffBias = i.m_FalloffBias;

	float flFalloffDist = i.m_MaxDist - pInfo->m_flFalloffOffset;
	pInfo->m_flOOZFalloffDist = ( flFalloffDist > 0.0f ) ? 1.0f / flFalloffDist : 1.0f;

	// for use in the shader
	pInfo->m_vShadowFalloffParams.x = -pInfo->m_flFalloffOffset * pInfo->m_flOOZFalloffDist;
	pInfo->m_vShadowFalloffParams.y = pInfo->m_flOOZFalloffDist;
	pInfo->m_vShadowFalloffParams.z = 1.0f/255.0f * i.m_FalloffBias;
	// we assume that shadow.m_flFalloffAmount is constant
}


//-----------------------------------------------------------------------------
// Adds normal shadows to the mesh builder
//-----------------------------------------------------------------------------
int CShadowMgr::AddNormalShadowsToMeshBuilder( CMeshBuilder& meshBuilder, ShadowRenderInfo_t& info )
{
	// Step through the cache and add all shadows on normal surfaces
	ShadowDecalRenderInfo_t shadow;
	int baseIndex = 0;
	for (int i = 0; i < info.m_Count; ++i)
	{
		// Two loops here, basically to minimize the # of if statements we need
		ShadowVertexCache_t* pVertexCache;
		if (info.m_pCache[i] < 0)
		{
			pVertexCache = &m_TempVertexCache[-info.m_pCache[i]-1];
		}
		else
		{
			pVertexCache = &m_VertexCache[info.m_pCache[i]];
		}

		ShadowVertex_t* pVerts = GetCachedVerts( *pVertexCache );
		g_pShadowMgr->ComputeRenderInfo( &shadow, pVertexCache->m_Shadow );

		int j;
		Vector2D texCoord;
		int vCount = pVertexCache->m_Count - 2;
		if ( vCount <= 0 )
			continue;

		for ( j = 0; j < vCount; ++j, ++pVerts )
		{
			// Transform + offset the texture coords
			Vector2DMultiply( pVerts->m_ShadowSpaceTexCoord.AsVector2D(), shadow.m_vTexSize, texCoord );
			texCoord += shadow.m_vTexOrigin;

			meshBuilder.Position3fv( pVerts->m_Position.Base() );
			meshBuilder.TexCoord3f( 0, texCoord.x, texCoord.y, pVerts->m_ShadowSpaceTexCoord.z );
			meshBuilder.TexCoord3fv( 1, shadow.m_vShadowFalloffParams.Base() );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 2>();

			meshBuilder.FastIndex( baseIndex );
			meshBuilder.FastIndex( j + baseIndex + 1 );
			meshBuilder.FastIndex( j + baseIndex + 2 );
		}

		Vector2DMultiply( pVerts->m_ShadowSpaceTexCoord.AsVector2D(), shadow.m_vTexSize, texCoord );
		texCoord += shadow.m_vTexOrigin;
		meshBuilder.Position3fv( pVerts->m_Position.Base() );
		meshBuilder.TexCoord3f( 0, texCoord.x, texCoord.y, pVerts->m_ShadowSpaceTexCoord.z );
		meshBuilder.TexCoord3fv( 1, shadow.m_vShadowFalloffParams.Base() );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 2>();
		++pVerts;

		Vector2DMultiply( pVerts->m_ShadowSpaceTexCoord.AsVector2D(), shadow.m_vTexSize, texCoord );
		texCoord += shadow.m_vTexOrigin;
		meshBuilder.Position3fv( pVerts->m_Position.Base() );
		meshBuilder.TexCoord3f( 0, texCoord.x, texCoord.y, pVerts->m_ShadowSpaceTexCoord.z );
		meshBuilder.TexCoord3fv( 1, shadow.m_vShadowFalloffParams.Base() );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 2>();

		// Update the base index
		baseIndex += vCount + 2; 
	}
	
	return baseIndex;
}


//-----------------------------------------------------------------------------
// Adds displacement shadows to the mesh builder
//-----------------------------------------------------------------------------
int CShadowMgr::AddDisplacementShadowsToMeshBuilder( CMeshBuilder& meshBuilder, 
								ShadowRenderInfo_t& info, int baseIndex )
{
	if ( !r_DrawDisp.GetBool() )
		return baseIndex;

	// Step through the cache and add all shadows on displacement surfaces
	for (int i = 0; i < info.m_DispCount; ++i)
	{
		baseIndex = DispInfo_AddShadowsToMeshBuilder( meshBuilder, info.m_pDispCache[i], baseIndex );
	}

	return baseIndex;
}


//-----------------------------------------------------------------------------
// The following methods will display debugging info in the middle of each shadow decal
//-----------------------------------------------------------------------------
static void DrawShadowID( ShadowHandle_t shadowHandle, const Vector &vecCentroid )
{
#ifndef DEDICATED
	char buf[32];
	Q_snprintf(buf, sizeof( buf ), "%d", shadowHandle );
	CDebugOverlay::AddTextOverlay( vecCentroid, 0, buf );
#endif
}

void CShadowMgr::RenderDebuggingInfo( const ShadowRenderInfo_t &info, ShadowDebugFunc_t func )
{
	// Step through the cache and add all shadows on normal surfaces
	for (int i = 0; i < info.m_Count; ++i)
	{
		ShadowVertexCache_t* pVertexCache;
		if (info.m_pCache[i] < 0)
		{
			pVertexCache = &m_TempVertexCache[-info.m_pCache[i]-1];
		}
		else
		{
			pVertexCache = &m_VertexCache[info.m_pCache[i]];
		}

		ShadowVertex_t* pVerts = GetCachedVerts( *pVertexCache );

		Vector vecNormal;
		float flTotalArea = 0.0f;
		Vector vecCentroid(0,0,0);
		Vector vecApex = pVerts[0].m_Position;
		int vCount = pVertexCache->m_Count;

		for ( int j = 0; j < vCount - 2; ++j )
		{
			Vector v1 = pVerts[j + 1].m_Position;
			Vector v2 = pVerts[j + 2].m_Position;
			CrossProduct( v2 - v1, v1 - vecApex, vecNormal );
			float flArea = vecNormal.Length();
			flTotalArea += flArea;
			vecCentroid += (vecApex + v1 + v2) * flArea / 3.0f;
		}

		if (flTotalArea)
		{
			vecCentroid /= flTotalArea;
		}

		func( pVertexCache->m_Shadow, vecCentroid );
	}
}


//-----------------------------------------------------------------------------
// Renders shadows that all share a material enumeration
//-----------------------------------------------------------------------------
void CShadowMgr::RenderShadowList( IMatRenderContext *pRenderContext, ShadowDecalHandle_t decalHandle, const VMatrix* pModelToWorld )
{
	//=============================================================================
	// HPE_BEGIN:
	// [smessick] Make sure we don't overflow our caches.
	//=============================================================================

	if ( m_DecalsToRender > m_ShadowDecalCache.Count() )
	{
		// Don't grow past the MAX_SHADOW_DECAL_CACHE_COUNT cap.
		int diff = MIN( m_DecalsToRender, MAX_SHADOW_DECAL_CACHE_COUNT ) - m_ShadowDecalCache.Count();
		if ( diff > 0 )
		{
			// Grow the cache.
			m_ShadowDecalCache.Grow( diff );
			DevMsg( "[CShadowMgr::RenderShadowList] growing shadow decal cache (decals: %d, cache: %d, diff: %d).\n", m_DecalsToRender, m_ShadowDecalCache.Count(), diff );
		}
	}

	if ( m_DecalsToRender > m_DispShadowDecalCache.Count() )
	{
		// Don't grow past the MAX_SHADOW_DECAL_CACHE_COUNT cap.
		int diff = MIN( m_DecalsToRender, MAX_SHADOW_DECAL_CACHE_COUNT ) - m_DispShadowDecalCache.Count();
		if ( diff > 0 )
		{
			// Grow the cache.
			m_DispShadowDecalCache.Grow( diff );
			DevMsg( "[CShadowMgr::RenderShadowList] growing disp shadow decal cache (decals: %d, cache: %d, diff: %d).\n", m_DecalsToRender, m_DispShadowDecalCache.Count(), diff );
		}
	}

	//=============================================================================
	// HPE_END
	//=============================================================================

	// Set the render state...
	Shadow_t& shadow = m_Shadows[m_ShadowDecals[decalHandle].m_Shadow];

	if ( r_shadowwireframe.GetInt() == 0 )
	{
		pRenderContext->Bind( shadow.m_pMaterial, shadow.m_pBindProxy );
	}
	else
	{
		pRenderContext->Bind( g_materialWorldWireframe );
	}

	// Blow away the temporary vertex cache (for normal surfaces)
	ClearTempCache();

	// Set up rendering info structure
	ShadowRenderInfo_t info;

	//=============================================================================
	// HPE_BEGIN:
	// [smessick] This code used to create the cache dynamically on the stack.
	//=============================================================================
	info.m_pCache = m_ShadowDecalCache.Base();
	info.m_pDispCache = m_DispShadowDecalCache.Base();
	//=============================================================================
	// HPE_END
	//=============================================================================

	info.m_pModelToWorld = pModelToWorld;
	if ( pModelToWorld )
	{
		MatrixInverseTR( *pModelToWorld, info.m_WorldToModel );
	}
	info.m_nMaxIndices = pRenderContext->GetMaxIndicesToRender();
	info.m_nMaxVertices = pRenderContext->GetMaxVerticesToRender( shadow.m_pMaterial );

	// Iterate over all decals in the decal list and generate polygon lists
	// Creating them from scratch if their shadow poly cache is invalid
	if ( r_threaded_shadow_clip.GetBool() )
	{
		GenerateShadowRenderInfoThreaded(pRenderContext, decalHandle, info);
	}
	else
	{
		GenerateShadowRenderInfo(pRenderContext, decalHandle, info);
	}
	Assert( info.m_Count <= m_DecalsToRender );
	Assert( info.m_DispCount <= m_DecalsToRender );
	//=============================================================================
	// HPE_BEGIN:
	// [smessick] Also check against the max.
	//=============================================================================
	Assert( info.m_Count <= m_ShadowDecalCache.Count() &&
			info.m_Count <= MAX_SHADOW_DECAL_CACHE_COUNT );
	Assert( info.m_DispCount <= m_DispShadowDecalCache.Count() &&
			info.m_DispCount <= MAX_SHADOW_DECAL_CACHE_COUNT );
	//=============================================================================
	// HPE_END
	//=============================================================================

	// Now that the vertex lists are created, render them
	IMesh* pMesh = pRenderContext->GetDynamicMesh();
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, info.m_VertexCount, info.m_IndexCount );

	// Add in shadows from both normal surfaces + displacement surfaces
	int baseIndex = AddNormalShadowsToMeshBuilder( meshBuilder, info );
	AddDisplacementShadowsToMeshBuilder( meshBuilder, info, baseIndex );

	meshBuilder.End();
	pMesh->Draw();

	if (r_shadowids.GetInt() != 0)
	{
		RenderDebuggingInfo( info, DrawShadowID );
	}
}

//-----------------------------------------------------------------------------
// Set the number of world material buckets.  This should get called on level load.
//-----------------------------------------------------------------------------
void CShadowMgr::SetNumWorldMaterialBuckets( int numMaterialSortBins )
{
	m_NumWorldMaterialBuckets = numMaterialSortBins;
	FlashlightHandle_t flashlightID;
	for( flashlightID = m_FlashlightStates.Head(); 
	     flashlightID != m_FlashlightStates.InvalidIndex(); 
		 flashlightID = m_FlashlightStates.Next( flashlightID ) )
	{
		m_FlashlightStates[flashlightID].m_MaterialBuckets.SetNumMaterialSortIDs( numMaterialSortBins );
		m_FlashlightStates[flashlightID].m_OccluderBuckets.SetNumMaterialSortIDs( numMaterialSortBins );
	}	
	ClearAllFlashlightMaterialBuckets();
}

//-----------------------------------------------------------------------------
// Per frame call to clear all of the flashlight world material buckets.
//-----------------------------------------------------------------------------
void CShadowMgr::ClearAllFlashlightMaterialBuckets( void )
{
	if ( m_bSinglePassFlashlightStateEnabled )
		return;

	FlashlightHandle_t flashlightID;
	for( flashlightID = m_FlashlightStates.Head(); 
	     flashlightID != m_FlashlightStates.InvalidIndex(); 
		 flashlightID = m_FlashlightStates.Next( flashlightID ) )
	{
		m_FlashlightStates[flashlightID].m_MaterialBuckets.Flush();
	}
}

//-----------------------------------------------------------------------------
// Allocate world material buckets for a particular flashlight.  This should get called on flashlight creation.
//-----------------------------------------------------------------------------
void CShadowMgr::AllocFlashlightMaterialBuckets( FlashlightHandle_t flashlightID )
{
	Assert( m_FlashlightStates.MaxElementIndex() >= flashlightID );
	m_FlashlightStates[flashlightID].m_MaterialBuckets.SetNumMaterialSortIDs( m_NumWorldMaterialBuckets );
	m_FlashlightStates[flashlightID].m_OccluderBuckets.SetNumMaterialSortIDs( m_NumWorldMaterialBuckets );
}

//-----------------------------------------------------------------------------
// Update a particular flashlight's state.
//-----------------------------------------------------------------------------
void CShadowMgr::UpdateFlashlightState( ShadowHandle_t shadowHandle, const FlashlightState_t &lightState )
{
	m_FlashlightStates[m_Shadows[shadowHandle].m_FlashlightHandle].m_FlashlightState = lightState;
}

void CShadowMgr::SetFlashlightDepthTexture( ShadowHandle_t shadowHandle, ITexture *pFlashlightDepthTexture, unsigned char ucShadowStencilBit )
{
	m_Shadows[shadowHandle].m_pFlashlightDepthTexture = pFlashlightDepthTexture;
	m_Shadows[shadowHandle].m_ucShadowStencilBit = ucShadowStencilBit;
}

bool ScreenSpaceRectFromPoints( IMatRenderContext *pRenderContext, Vector vClippedPolygons[8][10], int *pNumPoints, int nNumPolygons, int *nLeft, int *nTop, int *nRight, int *nBottom )
{
	if( nNumPolygons == 0 )
		return false;

	VMatrix matView, matProj, matViewProj;
	pRenderContext->GetMatrix( MATERIAL_VIEW, &matView );
	pRenderContext->GetMatrix( MATERIAL_PROJECTION, &matProj );
	MatrixMultiply( matProj, matView, matViewProj );

	float fMinX, fMaxX, fMinY, fMaxY;								// Init bounding rect
	fMinX = fMinY =  FLT_MAX;
	fMaxX = fMaxY = -FLT_MAX;

	for ( int i=0; i<nNumPolygons; i++ )
	{
		for ( int j=0; j<pNumPoints[i]; j++ )
		{
			Vector vScreenSpacePoint;
			matViewProj.V3Mul( vClippedPolygons[i][j], vScreenSpacePoint );	// Transform from World to screen space

			fMinX = fpmin( fMinX, vScreenSpacePoint.x );				// Update mins/maxes
			fMaxX = fpmax( fMaxX, vScreenSpacePoint.x );				//
			fMinY = fpmin( fMinY, -vScreenSpacePoint.y );				// These are in -1 to +1 range
			fMaxY = fpmax( fMaxY, -vScreenSpacePoint.y );				//
		}
	}

	// Get render target dimensions
	int nWidth, nHeight;
	g_pMaterialSystem->GetBackBufferDimensions( nWidth, nHeight );

	// Convert to render target pixel units
	*nLeft	 = ((fMinX * 0.5f + 0.5f) * (float) nWidth ) - 1;
	*nTop    = ((fMinY * 0.5f + 0.5f) * (float) nHeight) - 1; 
	*nRight  = ((fMaxX * 0.5f + 0.5f) * (float) nWidth ) + 1;
	*nBottom = ((fMaxY * 0.5f + 0.5f) * (float) nHeight) + 1;  

	// Clamp to render target dimensions
	*nLeft   = clamp( *nLeft,   0, nWidth  );
	*nTop    = clamp( *nTop,    0, nHeight );
	*nRight  = clamp( *nRight,  0, nWidth  );
	*nBottom = clamp( *nBottom, 0, nHeight );

	// Scale and bias to fit current viewport
	int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
	pRenderContext->GetViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );

	float flScaleX = ( float )( nViewportWidth ) / ( float )nWidth;
	float flScaleY = ( float )( nViewportHeight ) / ( float )nHeight;
	*nLeft   = ( int )( ( float )( *nLeft )   * flScaleX ) + nViewportX;
	*nRight  = ( int )( ( float )( *nRight )  * flScaleX ) + nViewportX;
	*nTop    = ( int )( ( float )( *nTop )    * flScaleY ) + nViewportY;
	*nBottom = ( int )( ( float )( *nBottom ) * flScaleY ) + nViewportY;

	Assert( (*nLeft <= *nRight) && (*nTop <= *nBottom) );

	// Do we have an actual subrect of the whole screen?
	bool bWithinBounds = ((*nLeft > 0 ) || (*nTop > 0) || (*nRight < nWidth) || (*nBottom < nHeight));	

	// Compute valid area
	nWidth  = (*nRight - *nLeft);
	nHeight = (*nBottom - *nTop);
	int nArea = ( nWidth > 0 ) && ( nHeight > 0 ) ? nWidth * nHeight : 0;

	// Valid rect?
	return bWithinBounds && (nArea > 0);
}

// Turn this optimization off by default
static ConVar r_flashlightclip("r_flashlightclip", "0", FCVAR_CHEAT );
static ConVar r_flashlightdrawclip("r_flashlightdrawclip", "0", FCVAR_CHEAT );
static ConVar r_flashlightscissor( "r_flashlightscissor", "0", FCVAR_MATERIAL_SYSTEM_THREAD );

void ConstructNearAndFarPolygons( Vector *pVecNearPlane, Vector *pVecFarPlane, float flPlaneEpsilon )
{
	const CViewSetup &view = g_EngineRenderer->ViewGetCurrent();

	float fovY = CalcFovY( view.fov, view.m_flAspectRatio );

	// Compute near and far plane half-width and half-height
	float flTanHalfAngleRadians = tan( view.fov * ( 0.5f * M_PI / 180.0f ) );
	float flHalfNearWidth = flTanHalfAngleRadians * ( view.zNear + flPlaneEpsilon );
	float flHalfFarWidth  = flTanHalfAngleRadians * ( view.zFar  - flPlaneEpsilon );
	flTanHalfAngleRadians = tan( fovY * ( 0.5f * M_PI / 180.0f ) );
	float flHalfNearHeight = flTanHalfAngleRadians * ( view.zNear + flPlaneEpsilon );
	float flHalfFarHeight  = flTanHalfAngleRadians * ( view.zFar  - flPlaneEpsilon );

	// World-space orientation of viewer
	Vector vForward, vRight, vUp;
	AngleVectors( view.angles, &vForward, &vRight, &vUp );
	vForward.NormalizeInPlace();
	vRight.NormalizeInPlace();
	vUp.NormalizeInPlace();

	// Center of near and far planes in world space
	Vector vCenterNear = view.origin + vForward * ( view.zNear + flPlaneEpsilon );
	Vector vCenterFar  = view.origin + vForward * ( view.zFar  - flPlaneEpsilon );

	pVecNearPlane[0] = vCenterNear - ( vRight * flHalfNearWidth ) - ( vUp * flHalfNearHeight );
	pVecNearPlane[1] = vCenterNear - ( vRight * flHalfNearWidth ) + ( vUp * flHalfNearHeight );
	pVecNearPlane[2] = vCenterNear + ( vRight * flHalfNearWidth ) + ( vUp * flHalfNearHeight );
	pVecNearPlane[3] = vCenterNear + ( vRight * flHalfNearWidth ) - ( vUp * flHalfNearHeight );

	pVecFarPlane[0]  = vCenterNear - ( vRight * flHalfFarWidth )  - ( vUp * flHalfFarHeight );
	pVecFarPlane[1]  = vCenterNear + ( vRight * flHalfFarWidth )  - ( vUp * flHalfFarHeight );
	pVecFarPlane[2]  = vCenterNear + ( vRight * flHalfFarWidth )  + ( vUp * flHalfFarHeight );
	pVecFarPlane[3]  = vCenterNear - ( vRight * flHalfFarWidth )  + ( vUp * flHalfFarHeight );
}

void DrawDebugPolygon( int nNumVerts, Vector *pVecPoints, bool bFrontFacing, bool bNearPlane )
{
	int r=0, g=0, b=0;
	if ( bFrontFacing )
		b = 255;
	else
		r = 255; 

	if ( bNearPlane )	// Draw near plane green for visualization
	{
		r = b = 0;
		g = 255;
	}

	// Draw triangles fanned out from vertex zero
	for (int i=1; i<(nNumVerts-1); i++)
	{
		Vector v0 = pVecPoints[0];
		Vector v1 = pVecPoints[bFrontFacing ? i : i+1];
		Vector v2 = pVecPoints[bFrontFacing ? i+1 : i];

		CDebugOverlay::AddTriangleOverlay(v0, v1, v2, r, g, b, 20, true, 0 );
	}

	// Draw solid lines around the polygon
	for (int i=0; i<nNumVerts; i++)
	{
		Vector v0 = pVecPoints[i];
		Vector v1 = pVecPoints[ (i+1) % nNumVerts];

		CDebugOverlay::AddLineOverlay( v0, v1, 255, 255, 255, 255, false, 0);
	}
}

void DrawPolygonToStencil( IMatRenderContext *pRenderContext, int nNumVerts, Vector *pVecPoints, bool bFrontFacing, bool bNearPlane )
{
	IMaterial *pMaterial = materials->FindMaterial( "engine/writestencil", TEXTURE_GROUP_OTHER, true );

	pRenderContext->Bind( pMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nNumVerts-2 );

	// Fan out from vertex zero
	for (int i=1; i<(nNumVerts-1); i++)
	{
		meshBuilder.Position3f( pVecPoints[0].x, pVecPoints[0].y, pVecPoints[0].z );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 0>();

		int index = bFrontFacing ? i : i+1;
		meshBuilder.Position3f( pVecPoints[index].x, pVecPoints[index].y, pVecPoints[index].z );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 0>();

		index = bFrontFacing ? i+1 : i;
		meshBuilder.Position3f( pVecPoints[index].x, pVecPoints[index].y, pVecPoints[index].z );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 0>();
	}

	meshBuilder.End( false, true );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
}

// Determine if two Vectors are sufficiently close (Manhattan-ish distance, not Euclidean)
bool SufficientlyClose( Vector v1, Vector v2, float flEpsilon )
{
	if ( fabs( v1.x - v2.x ) > flEpsilon )			// Bail if x components are sufficiently different
		return false;

	if ( fabs( v1.y - v2.y ) > flEpsilon )			// Bail if y components are sufficiently different
		return false;

	if ( fabs( v1.z - v2.z ) > flEpsilon )			// Bail if z components are sufficiently different
		return false;

	return true;
}


int ClipPlaneToFrustum( Vector *pInPoints, Vector *pOutPoints, Vector *pVecWorldFrustumPoints )
{
	Vector vClipPing[10];			// Vector lists to ping-pong between while clipping
	Vector vClipPong[10];			//
	bool bPing = true;				// Ping holds the latest polygon

	vClipPing[0] = pInPoints[0];	// Copy into Ping
	vClipPing[1] = pInPoints[1];
	vClipPing[2] = pInPoints[2];
	vClipPing[3] = pInPoints[3];

	int nNumPoints = 4;

	for ( int i=0; i < 6; i++ )
	{
		Vector vNormal;
		float flDist;
		
		if ( nNumPoints < 3 )	// If we're already clipped away, bail out entirely
			break;

		Vector *pClipPolygon = pVecWorldFrustumPoints+(4*i);										// Polygon defining clip plane
		ComputeTrianglePlane( pClipPolygon[0], pClipPolygon[1], pClipPolygon[2], vNormal, flDist );	// Compute plane normal and dist

		if ( bPing )
			nNumPoints = ClipPolyToPlane( vClipPing, nNumPoints, vClipPong, vNormal, flDist ); // Clip Ping into Pong
		else
			nNumPoints = ClipPolyToPlane( vClipPong, nNumPoints, vClipPing, vNormal, flDist ); // Clip Pong into Ping

		bPing = !bPing;	// Flip buffers
	}

	if ( nNumPoints < 3)
		return 0;

	if ( bPing )
		memcpy( pOutPoints, vClipPing, nNumPoints * sizeof(Vector) );
	else
		memcpy( pOutPoints, vClipPong, nNumPoints * sizeof(Vector) );

	return nNumPoints;
}



void CShadowMgr::SetStencilAndScissor( IMatRenderContext *pRenderContext, FlashlightInfo_t &flashlightInfo, bool bUseStencil )
{
	VMatrix matFlashlightToWorld;
	MatrixInverseGeneral( m_Shadows[flashlightInfo.m_Shadow].m_WorldToShadow, matFlashlightToWorld );

	// Eight points defining the frustum in Flashlight space
	Vector vFrustumPoints[24] = { Vector(0.0f, 0.0f, 0.0f), Vector(1.0f, 0.0f, 0.0f), Vector(1.0f, 1.0f, 0.0f), Vector(0.0f, 1.0f, 0.0f),	// Near
								  Vector(0.0f, 0.0f, 1.0f), Vector(0.0f, 1.0f, 1.0f), Vector(1.0f, 1.0f, 1.0f), Vector(1.0f, 0.0f, 1.0f),	// Far
								  Vector(1.0f, 0.0f, 0.0f), Vector(1.0f, 0.0f, 1.0f), Vector(1.0f, 1.0f, 1.0f), Vector(1.0f, 1.0f, 0.0f),	// Right
								  Vector(0.0f, 0.0f, 0.0f), Vector(0.0f, 1.0f, 0.0f), Vector(0.0f, 1.0f, 1.0f), Vector(0.0f, 0.0f, 1.0f),	// Left
								  Vector(0.0f, 1.0f, 0.0f), Vector(1.0f, 1.0f, 0.0f), Vector(1.0f, 1.0f, 1.0f), Vector(0.0f, 1.0f, 1.0f),	// Bottom
								  Vector(0.0f, 0.0f, 0.0f), Vector(0.0f, 0.0f, 1.0f), Vector(1.0f, 0.0f, 1.0f), Vector(1.0f, 0.0f, 0.0f)};	// Top

	// Transform points to world space
	Vector vWorldFrustumPoints[24];
	for ( int i=0; i < 24; i++ )
	{
		matFlashlightToWorld.V3Mul( vFrustumPoints[i], vWorldFrustumPoints[i] );
	}

	// Express near and far planes of View frustum in world space
	Vector vNearNormal, vFarNormal;
	float flNearDist, flFarDist;
	const float flPlaneEpsilon = 0.4f;

	const CViewSetup &view = g_EngineRenderer->ViewGetCurrent();
	Vector vForward;
	AngleVectors( view.angles, &vForward, NULL, NULL );
	float flIntercept = DotProduct( view.origin, vForward );
	vFarNormal = -vForward;
	vNearNormal = vForward;
	flNearDist = (view.zNear + flPlaneEpsilon) + flIntercept;
	flFarDist = -(view.zFar - flPlaneEpsilon + flIntercept);

	Vector	vTempFace[5];
	Vector	vClippedFace[6];
	Vector	vClippedPolygons[8][10];	// Array of up to eight polygons (10 verts is more than enough for each)
	int nNumVertices[8];				// Number vertices on each of the of clipped polygons
	int nNumPolygons = 0;				// How many polygons have survived the clip

	// Clip each face individually to near and far planes
	for ( int i=0; i < 6; i++ )
	{
		Vector *inVerts   = vWorldFrustumPoints+(4*i);	// Series of quadrilateral inputs
		Vector *tempVerts = vTempFace;
		Vector *outVerts  = vClippedFace;

		int nClipCount = ClipPolyToPlane( inVerts, 4, tempVerts, vNearNormal, flNearDist );			// need to set fOnPlaneEpsilon?

		if ( nClipCount > 2 )	// If the polygon survived the near clip, try the far as well
		{
			nClipCount = ClipPolyToPlane( tempVerts, nClipCount, outVerts, vFarNormal, flFarDist );	// need to set fOnPlaneEpsilon?

			if ( nClipCount > 2 )	// If we still have a poly after clipping to both planes, add it to the list
			{
				memcpy( vClippedPolygons[nNumPolygons], outVerts, nClipCount * sizeof (Vector) );
				nNumVertices[nNumPolygons] = nClipCount;
				nNumPolygons++;
			}
		}
	}

	// Construct polygons for near and far planes
	Vector	vNearPlane[4], vFarPlane[4];
	ConstructNearAndFarPolygons( vNearPlane, vFarPlane, flPlaneEpsilon );
	bool bNearPlane = false;

	// Clip near plane to flashlight frustum and tack on to list
	int nClipCount = ClipPlaneToFrustum( vNearPlane, vClippedPolygons[nNumPolygons], vWorldFrustumPoints );
	if ( nClipCount > 2 )		// If the near plane clipped and resulted in a polygon, take note in the polygon list
	{
		nNumVertices[nNumPolygons] = nClipCount;
		nNumPolygons++;
		bNearPlane = true;
	}

/*
TODO: do we even need to do the far plane?

		// Clip near plane to flashlight frustum and tack on to list
		nClipCount = ClipPlaneToFrustum( vFarPlane, vClippedPolygons[nNumPolygons], vWorldFrustumPoints );
		if ( nClipCount > 2 )		// If the near plane clipped and resulted in a polygon, take note in the polygon list
		{
			nNumVertices[nNumPolygons] = nClipCount;
			nNumPolygons++;
		}
*/
	// Fuse positions of any verts which are within epsilon
	for (int i=0; i<nNumPolygons; i++)							// For each polygon
	{
		for (int j=0; j<nNumVertices[i]; j++)					// For each vertex
		{
			for (int k=i+1; k<nNumPolygons; k++)				// For each later polygon
			{
				for (int m=0; m<nNumVertices[k]; m++)			// For each vertex
				{
					if ( SufficientlyClose(vClippedPolygons[i][j], vClippedPolygons[k][m], 0.1f) )
					{
						vClippedPolygons[k][m] = vClippedPolygons[i][j];
					}
				}
			}
		}
	}

	// Calculate scissoring rect
	flashlightInfo.m_FlashlightState.m_bScissor = false;
	if ( r_flashlightscissor.GetBool() && (nNumPolygons > 0) )
	{
		int nLeft, nTop, nRight, nBottom;
		flashlightInfo.m_FlashlightState.m_bScissor = ScreenSpaceRectFromPoints( pRenderContext, vClippedPolygons, nNumVertices, nNumPolygons, &nLeft, &nTop, &nRight, &nBottom );
		if ( flashlightInfo.m_FlashlightState.m_bScissor )
		{
			flashlightInfo.m_FlashlightState.m_nLeft   = nLeft;
			flashlightInfo.m_FlashlightState.m_nTop    = nTop;
			flashlightInfo.m_FlashlightState.m_nRight  = nRight;
			flashlightInfo.m_FlashlightState.m_nBottom = nBottom;
		}
	}

	if ( r_flashlightdrawclip.GetBool() && r_flashlightclip.GetBool() && bUseStencil )
	{
		// Draw back facing debug polygons
		for (int i=0; i<nNumPolygons; i++)
		{
			DrawDebugPolygon( nNumVertices[i], vClippedPolygons[i], false, false );
		}
/*
		// Draw front facing debug polygons
		for (int i=0; i<nNumPolygons; i++)
		{
			DrawDebugPolygon( nNumVertices[i], vClippedPolygons[i], true, bNearPlane && (i == nNumPolygons-1)  );
		}
*/
	}

	if ( r_flashlightclip.GetBool() && bUseStencil )
	{
/*
		// The traditional settings...

		// Set up to set stencil bit on front facing polygons
		pRenderContext->SetStencilEnable( true );
		pRenderContext->SetStencilFailOperation( STENCILOPERATION_KEEP );						// Stencil fails
		pRenderContext->SetStencilZFailOperation( STENCILOPERATION_KEEP );						// Stencil passes but depth fails
		pRenderContext->SetStencilPassOperation( STENCILOPERATION_REPLACE );					// Z and stencil both pass
		pRenderContext->SetStencilCompareFunction( STENCILCOMPARISONFUNCTION_ALWAYS );			// Stencil always pass
		pRenderContext->SetStencilReferenceValue( m_Shadows[flashlightInfo.m_Shadow].m_ucShadowStencilBit );
		pRenderContext->SetStencilTestMask( m_Shadows[flashlightInfo.m_Shadow].m_ucShadowStencilBit );
		pRenderContext->SetStencilWriteMask( m_Shadows[flashlightInfo.m_Shadow].m_ucShadowStencilBit );		// Bit mask which is specific to this shadow
*/

		// Just blast front faces into the stencil buffer no matter what...
		ShaderStencilState_t state;
		state.m_bEnable = true;																	
		state.m_FailOp = SHADER_STENCILOP_KEEP;										// Stencil fails
		state.m_ZFailOp = SHADER_STENCILOP_SET_TO_REFERENCE;									// Stencil passes but depth fails
		state.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;										// Z and stencil both pass
		state.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;										// Stencil always pass
		state.m_nReferenceValue = m_Shadows[flashlightInfo.m_Shadow].m_ucShadowStencilBit;
		state.m_nTestMask = m_Shadows[flashlightInfo.m_Shadow].m_ucShadowStencilBit;
		state.m_nWriteMask =m_Shadows[flashlightInfo.m_Shadow].m_ucShadowStencilBit;			// Bit mask which is specific to this shadow
		pRenderContext->SetStencilState( state );

		for ( int i=0; i<nNumPolygons; i++ )													// Set the stencil bit on front facing
		{
			DrawPolygonToStencil( pRenderContext, nNumVertices[i], vClippedPolygons[i], true, false );
		}

/*
		pRenderContext->SetStencilReferenceValue( 0x00000000 );									// All bits cleared

		for (int i=0; i<nNumPolygons; i++)														// Clear the stencil bit on back facing
		{
			DrawPolygonToStencil( nNumVertices[i], vClippedPolygons[i], false, false );
		}
*/

		ShaderStencilState_t stateDisable;
		stateDisable.m_bEnable = false;
		pRenderContext->SetStencilState( stateDisable );
	}
}

//---------------------------------------------------------------------------------------
// Set masking stencil bits for all flashlights
//---------------------------------------------------------------------------------------
void CShadowMgr::SetFlashlightStencilMasks( bool bDoMasking )
{
	VPROF_BUDGET( "CShadowMgr::SetFlashlightStencilMasks", VPROF_BUDGETGROUP_SHADOW_RENDERING );

	if ( m_bSinglePassFlashlightStateEnabled )
		return;

	// Bail out if we're not doing any of these optimizations
	if ( !( r_flashlightclip.GetBool() || r_flashlightscissor.GetBool()) )
		return;

	FlashlightHandle_t flashlightID = m_FlashlightStates.Head();
	if ( flashlightID == m_FlashlightStates.InvalidIndex() )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	for( ; 
		 flashlightID != m_FlashlightStates.InvalidIndex(); 
		 flashlightID = m_FlashlightStates.Next( flashlightID ) )
	{
		FlashlightInfo_t &flashlightInfo = m_FlashlightStates[flashlightID];

		if ( flashlightInfo.m_nSplitscreenOwner >= 0 )
		{
			ASSERT_LOCAL_PLAYER_RESOLVABLE();
			if ( flashlightInfo.m_nSplitscreenOwner != GET_ACTIVE_SPLITSCREEN_SLOT() )
				continue;
		}

		SetStencilAndScissor( pRenderContext, flashlightInfo, m_Shadows[flashlightInfo.m_Shadow].m_pFlashlightDepthTexture != NULL );
	}
}


void CShadowMgr::PushFlashlightScissorBounds( void )
{
	m_ScissorStateEntryStart.AddToTail( m_ScissorStateBackups.Count() ); //push the location of the end of our current backup set

	FlashlightHandle_t flashlightID = m_FlashlightStates.Head();
	if ( flashlightID != m_FlashlightStates.InvalidIndex() )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		int iSplitScreenSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
		
		//count the number of entries we need
		int iStateCount = 0;
		for( ; 
			flashlightID != m_FlashlightStates.InvalidIndex(); 
			flashlightID = m_FlashlightStates.Next( flashlightID ) )
		{
			FlashlightInfo_t &flashlightInfo = m_FlashlightStates[flashlightID];

			if ( flashlightInfo.m_nSplitscreenOwner >= 0 )
			{
				if ( flashlightInfo.m_nSplitscreenOwner != iSplitScreenSlot )
					continue;
			}

			++iStateCount;
		}

		//add the entry space
		int iWriteSlot = m_ScissorStateBackups.Count();
		m_ScissorStateBackups.AddMultipleToTail( iStateCount );
		FlashLightScissorStateBackup_t *pBackups = m_ScissorStateBackups.Base();
		
		//write out the entries
		flashlightID = m_FlashlightStates.Head();
		for( ; 
			flashlightID != m_FlashlightStates.InvalidIndex(); 
			flashlightID = m_FlashlightStates.Next( flashlightID ) )
		{
			FlashlightInfo_t &flashlightInfo = m_FlashlightStates[flashlightID];

			if ( flashlightInfo.m_nSplitscreenOwner >= 0 )
			{
				if ( flashlightInfo.m_nSplitscreenOwner != iSplitScreenSlot )
					continue;
			}

			//write out state here
			pBackups[iWriteSlot].m_bScissor = flashlightInfo.m_FlashlightState.m_bScissor;
			pBackups[iWriteSlot].m_nLeft = flashlightInfo.m_FlashlightState.m_nLeft;
			pBackups[iWriteSlot].m_nTop = flashlightInfo.m_FlashlightState.m_nTop;
			pBackups[iWriteSlot].m_nRight = flashlightInfo.m_FlashlightState.m_nRight;
			pBackups[iWriteSlot].m_nBottom = flashlightInfo.m_FlashlightState.m_nBottom;
#if defined( DBGFLAG_ASSERT )
			pBackups[iWriteSlot].m_iFlashLightID = flashlightID;
#endif

			++iWriteSlot;
		}
	}
}
void CShadowMgr::PopFlashlightScissorBounds( void )
{
	int iPopStart = m_ScissorStateEntryStart.Tail();
	m_ScissorStateEntryStart.RemoveMultipleFromTail( 1 );

	if( iPopStart == m_ScissorStateBackups.Count() )
		return; //nothing in the backup

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int iSplitScreenSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	int iReadSlot = iPopStart;
	FlashLightScissorStateBackup_t *pBackups = m_ScissorStateBackups.Base();

	//read back the entries
	FlashlightHandle_t flashlightID = m_FlashlightStates.Head();
	for( ; 
		flashlightID != m_FlashlightStates.InvalidIndex(); 
		flashlightID = m_FlashlightStates.Next( flashlightID ) )
	{
		FlashlightInfo_t &flashlightInfo = m_FlashlightStates[flashlightID];

		if ( flashlightInfo.m_nSplitscreenOwner >= 0 )
		{
			if ( flashlightInfo.m_nSplitscreenOwner != iSplitScreenSlot )
				continue;
		}

		//need a more complex restore system if this ever fails
		Assert( pBackups[iReadSlot].m_iFlashLightID == flashlightID );

		//read back state here
		flashlightInfo.m_FlashlightState.m_bScissor = pBackups[iReadSlot].m_bScissor;
		flashlightInfo.m_FlashlightState.m_nLeft = pBackups[iReadSlot].m_nLeft;
		flashlightInfo.m_FlashlightState.m_nTop = pBackups[iReadSlot].m_nTop;
		flashlightInfo.m_FlashlightState.m_nRight = pBackups[iReadSlot].m_nRight;
		flashlightInfo.m_FlashlightState.m_nBottom = pBackups[iReadSlot].m_nBottom;
		
		++iReadSlot;
	}

	m_ScissorStateBackups.RemoveMultipleFromTail( m_ScissorStateBackups.Count() - iPopStart );
}


void CShadowMgr::DisableStencilAndScissorMasking( IMatRenderContext *pRenderContext, const FlashlightInfo_t &flashlightInfo, bool bDoMasking )
{
	if ( r_flashlightclip.GetBool() )
	{
		ShaderStencilState_t state;
		state.m_bEnable = false;
		pRenderContext->SetStencilState( state );
	}

	// Bail out if we're not doing any of these optimizations
	if ( !( r_flashlightclip.GetBool() || r_flashlightscissor.GetBool()) || !bDoMasking )
		return;

	// We only scissor when rendering to the back buffer
	if ( pRenderContext->GetRenderTarget() == NULL )
	{
		if ( r_flashlightscissor.GetBool() && flashlightInfo.m_FlashlightState.m_bScissor )
		{
			pRenderContext->PopScissorRect();
		}
	}
}


//---------------------------------------------------------------------------------------
// Enable/Disable masking based on stencil bit
//---------------------------------------------------------------------------------------
void CShadowMgr::EnableStencilAndScissorMasking( IMatRenderContext *pRenderContext, const FlashlightInfo_t &flashlightInfo, bool bDoMasking )
{
	// Bail out if we're not doing any of these optimizations
	if ( !( r_flashlightclip.GetBool() || r_flashlightscissor.GetBool()) || !bDoMasking )
		return;

	// Only turn on scissor when rendering to the back buffer
	if ( pRenderContext->GetRenderTarget() == NULL )
	{
		// Only do the stencil optimization when shadow depth mapping
		if ( r_flashlightclip.GetBool() && m_Shadows[flashlightInfo.m_Shadow].m_pFlashlightDepthTexture != NULL )
		{
			unsigned char ucShadowStencilBit = m_Shadows[flashlightInfo.m_Shadow].m_ucShadowStencilBit;

			ShaderStencilState_t state;
			state.m_bEnable = true;																	
			state.m_FailOp = SHADER_STENCILOP_KEEP;											// Stencil fails
			state.m_ZFailOp = SHADER_STENCILOP_KEEP;										// Stencil passes but depth fails
			state.m_PassOp = SHADER_STENCILOP_KEEP;											// Z and stencil both pass
			state.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;									// Stencil always pass

			state.m_nReferenceValue = ucShadowStencilBit;									// Bit must be set
			state.m_nTestMask = ucShadowStencilBit;											// Specific bit
			state.m_nWriteMask = 0x00000000;												// Specific bit
			pRenderContext->SetStencilState( state );

		}

		// Scissor even if we're not shadow depth mapping
		if ( r_flashlightscissor.GetBool() && flashlightInfo.m_FlashlightState.m_bScissor )
		{
			pRenderContext->PushScissorRect( flashlightInfo.m_FlashlightState.m_nLeft, flashlightInfo.m_FlashlightState.m_nTop,
											flashlightInfo.m_FlashlightState.m_nRight, flashlightInfo.m_FlashlightState.m_nBottom );
		}
	}
}


//---------------------------------------------------------------------------------------
// Sets the render states necessary to render a flashlight
//---------------------------------------------------------------------------------------
void CShadowMgr::SetFlashlightRenderState( ShadowHandle_t handle )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	if ( handle == SHADOW_HANDLE_INVALID )
	{
		pRenderContext->SetFlashlightMode( false );
		return;
	}

	const Shadow_t &shadow = m_Shadows[handle];
	pRenderContext->SetFlashlightMode( true );
	const FlashlightInfo_t &flashlightInfo = m_FlashlightStates[ shadow.m_FlashlightHandle ];
	pRenderContext->SetFlashlightStateEx( flashlightInfo.m_FlashlightState, shadow.m_WorldToShadow, shadow.m_pFlashlightDepthTexture );
}


//---------------------------------------------------------------------------------------
// Render all of the world and displacement surfaces that need to be drawn for flashlights
//---------------------------------------------------------------------------------------
void CShadowMgr::RenderFlashlights( bool bDoMasking, bool bDoSimpleProjections, const VMatrix* pModelToWorld )
{
#ifndef DEDICATED
	VPROF_BUDGET( "CShadowMgr::RenderFlashlights", VPROF_BUDGETGROUP_SHADOW_RENDERING );

	if ( m_bSinglePassFlashlightStateEnabled )
		return;

	if( r_flashlightrender.GetBool()==false && bDoSimpleProjections == false )
		return;

	// Draw the projective light sources, which get their material
	// from the surface and not from the shadow.
	// Tell the materialsystem that we are drawing additive flashlight lighting.
	FlashlightHandle_t flashlightID = m_FlashlightStates.Head();
	if ( flashlightID == m_FlashlightStates.InvalidIndex() )
		return;

	bool bWireframe = r_shadowwireframe.GetBool();

	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "CShadowMgr::RenderFlashlights()" );

	int nSearchFlags = SHADOW_SIMPLE_PROJECTION;
	if ( bDoSimpleProjections == false )
	{
		pRenderContext->SetFlashlightMode( true );
		nSearchFlags = SHADOW_FLASHLIGHT;
	}
	else
	{
		pRenderContext->SetFlashlightMode( false );
		bDoMasking = false;
	}

	// PORTAL 2 PAINT RENDERING
	CUtlVectorFixedGrowable< SurfaceHandle_t, 64 > paintableSurfaces;
	CUtlVectorFixedGrowable< int, 16 > batchPaintableSurfaceCount;
	CUtlVectorFixedGrowable< int, 16 > batchPaintableSurfaceIndexCount;
	CUtlVectorFixedGrowable< FlashlightInfo_t *, 16 > flashlightInfos;

	for( ; 
		 flashlightID != m_FlashlightStates.InvalidIndex(); 
		 flashlightID = m_FlashlightStates.Next( flashlightID ) )
	{
		FlashlightInfo_t &flashlightInfo = m_FlashlightStates[flashlightID];

		if ( ( m_Shadows[flashlightInfo.m_Shadow].m_Flags & ( SHADOW_FLASHLIGHT | SHADOW_SIMPLE_PROJECTION ) ) != nSearchFlags )
		{
			continue;
		}

		if ( flashlightInfo.m_nSplitscreenOwner >= 0 )
		{
			ASSERT_LOCAL_PLAYER_RESOLVABLE();
			if ( flashlightInfo.m_nSplitscreenOwner != GET_ACTIVE_SPLITSCREEN_SLOT() )
				continue;
		}

		CMaterialsBuckets<SurfaceHandle_t> &materialBuckets = flashlightInfo.m_MaterialBuckets;
		CMaterialsBuckets<SurfaceHandle_t>::SortIDHandle_t sortIDHandle = materialBuckets.GetFirstUsedSortID();
		if ( sortIDHandle == materialBuckets.InvalidSortIDHandle() )
			continue;

		flashlightInfos.AddToTail( &flashlightInfo );
		
		pRenderContext->SetFlashlightStateEx(flashlightInfo.m_FlashlightState, m_Shadows[flashlightInfo.m_Shadow].m_WorldToShadow, m_Shadows[flashlightInfo.m_Shadow].m_pFlashlightDepthTexture );
		EnableStencilAndScissorMasking( pRenderContext, flashlightInfo, bDoMasking );

		for( ; sortIDHandle != materialBuckets.InvalidSortIDHandle();
			 sortIDHandle = materialBuckets.GetNextUsedSortID( sortIDHandle ) )
		{
			int nBatchIndex = batchPaintableSurfaceCount.Count();
			batchPaintableSurfaceCount.AddToTail( 0 );
			batchPaintableSurfaceIndexCount.AddToTail( 0 );

			int sortID = materialBuckets.GetSortID( sortIDHandle );

			if( bWireframe )
			{
				pRenderContext->Bind( g_materialWorldWireframe );
			}
			else
			{
				if ( bDoSimpleProjections == false )
				{
					pRenderContext->Bind( materialSortInfoArray[sortID].material );
					pRenderContext->BindLightmapPage( materialSortInfoArray[sortID].lightmapPageID );
				}
				else
				{
					pRenderContext->Bind( flashlightInfo.m_FlashlightState.m_pProjectedMaterial );
				}
			}

			CMaterialsBuckets<SurfaceHandle_t>::ElementHandle_t elemHandle;
			// Figure out how many indices we have.
			int numIndices = 0;
			for( elemHandle = materialBuckets.GetElementListHead( sortID );
			     elemHandle != materialBuckets.InvalidElementHandle();
				 elemHandle = materialBuckets.GetElementListNext( elemHandle ) )
			{
				SurfaceHandle_t surfID = materialBuckets.GetElement( elemHandle );

				// PORTAL 2 PAINT RENDERING
				if ( !SurfaceHasDispInfo( surfID ) && !bWireframe && ( MSurf_Flags( surfID ) & SURFDRAW_PAINTED ) )
				{
					paintableSurfaces.AddToTail( surfID );
					++ batchPaintableSurfaceCount[ nBatchIndex ];

					int nVertCount, nIndexCount;

					Shader_GetSurfVertexAndIndexCount( surfID, &nVertCount, &nIndexCount );
					batchPaintableSurfaceIndexCount[ nBatchIndex ] += nIndexCount;
				}

				if( !SurfaceHasDispInfo( surfID ) )
				{
					numIndices += 3 * ( MSurf_VertCount( surfID ) - 2 );
				}
			}

			if( numIndices > 0 )
			{
				// NOTE: If we ever need to make this faster, we could get larger
				// batches here.
				// Draw this batch.
				IMesh *pMesh = pRenderContext->GetDynamicMesh( false, g_WorldStaticMeshes[sortID], 0 );
				CMeshBuilder meshBuilder;
				meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 0, numIndices );

				for( elemHandle = materialBuckets.GetElementListHead( sortID );
					 elemHandle != materialBuckets.InvalidElementHandle();
					 elemHandle = materialBuckets.GetElementListNext( elemHandle ) )
				{
					SurfaceHandle_t surfID = materialBuckets.GetElement( elemHandle );
					if( !SurfaceHasDispInfo( surfID ) )
					{
						BuildIndicesForWorldSurface( meshBuilder, surfID, host_state.worldbrush );
					}
				}
				// close out the index buffer
				meshBuilder.End( false, true );
			}

			// NOTE: If we ever need to make this faster, we could get larger batches here.
			// Draw displacements
			for( elemHandle = materialBuckets.GetElementListHead( sortID );
			     elemHandle != materialBuckets.InvalidElementHandle();
				 elemHandle = materialBuckets.GetElementListNext( elemHandle ) )
			{
				SurfaceHandle_t surfID = materialBuckets.GetElement( elemHandle );
				if( SurfaceHasDispInfo( surfID ) )
				{
					CDispInfo *pDisp = ( CDispInfo * )MSurf_DispInfo( surfID );
					Assert( pDisp );
					if( bWireframe )
					{
						pDisp->SpecifyDynamicMesh();
					}
					else
					{
						Assert( pDisp && pDisp->m_pMesh && pDisp->m_pMesh->m_pMesh );
						pDisp->m_pMesh->m_pMesh->Draw( pDisp->m_iIndexOffset, pDisp->m_nIndices );
					}
				}
			}
		 }

		DisableStencilAndScissorMasking( pRenderContext, flashlightInfo, bDoMasking );
	}

	// PORTAL 2 PAINT RENDERING
	if ( paintableSurfaces.Count() )
	{
		pRenderContext->SetRenderingPaint( true );
		PIXEVENT( pRenderContext, "Paint" );

		int nBatchIndex = 0;
		int nSurfaceIndex = 0;
		for ( int i = 0; i < flashlightInfos.Count(); i++ )
		{
			FlashlightInfo_t &flashlightInfo = *( flashlightInfos[ i ] );

			CMaterialsBuckets<SurfaceHandle_t> &materialBuckets = flashlightInfo.m_MaterialBuckets;
			CMaterialsBuckets<SurfaceHandle_t>::SortIDHandle_t sortIDHandle = materialBuckets.GetFirstUsedSortID();
			
			pRenderContext->SetFlashlightStateEx(flashlightInfo.m_FlashlightState, m_Shadows[flashlightInfo.m_Shadow].m_WorldToShadow, m_Shadows[flashlightInfo.m_Shadow].m_pFlashlightDepthTexture );
			EnableStencilAndScissorMasking( pRenderContext, flashlightInfo, bDoMasking );

			for( ; sortIDHandle != materialBuckets.InvalidSortIDHandle();
				sortIDHandle = materialBuckets.GetNextUsedSortID( sortIDHandle ) )
			{
				int sortID = materialBuckets.GetSortID( sortIDHandle );
				int nSurfaceCount = batchPaintableSurfaceCount[ nBatchIndex ];
				if ( nSurfaceCount > 0 )
				{
					if ( bDoSimpleProjections == false )
					{
						pRenderContext->Bind( materialSortInfoArray[sortID].material );
						pRenderContext->BindLightmapPage( materialSortInfoArray[sortID].lightmapPageID );
					}
					else
					{
						pRenderContext->Bind( flashlightInfo.m_FlashlightState.m_pProjectedMaterial );
					}

					IMesh *pMesh = pRenderContext->GetDynamicMesh( false, g_WorldStaticMeshes[sortID], 0 );
					CMeshBuilder meshBuilder;
					meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 0, batchPaintableSurfaceIndexCount[ nBatchIndex ] );

					for ( int i = 0; i < nSurfaceCount; ++ i, ++ nSurfaceIndex )
					{
						BuildIndicesForWorldSurface( meshBuilder, paintableSurfaces[ nSurfaceIndex ], host_state.worldbrush );
					}

					meshBuilder.End( false, true );
				}

				++ nBatchIndex;
			}

			DisableStencilAndScissorMasking( pRenderContext, flashlightInfo, bDoMasking );
		}
		pRenderContext->SetRenderingPaint( false );
	}

	// Tell the materialsystem that we are finished drawing additive flashlight lighting.
	if ( bDoSimpleProjections == false )
	{
		pRenderContext->SetFlashlightMode( false );
	}
	else
	{
		pRenderContext->SetFlashlightMode( false );
	}

#endif
}


const Frustum_t &CShadowMgr::GetFlashlightFrustum( ShadowHandle_t handle )
{
	Assert( m_Shadows[handle].m_Flags & ( SHADOW_FLASHLIGHT | SHADOW_SIMPLE_PROJECTION ) );
	Assert( m_Shadows[handle].m_FlashlightHandle != m_Shadows.InvalidIndex() );
	return m_FlashlightStates[m_Shadows[handle].m_FlashlightHandle].m_Frustum;
}

const FlashlightState_t &CShadowMgr::GetFlashlightState( ShadowHandle_t handle )
{
	Assert( m_Shadows[handle].m_Flags & ( SHADOW_FLASHLIGHT | SHADOW_SIMPLE_PROJECTION ) );
	Assert( m_Shadows[handle].m_FlashlightHandle != m_Shadows.InvalidIndex() );
	return m_FlashlightStates[m_Shadows[handle].m_FlashlightHandle].m_FlashlightState;
}



void CShadowMgr::DrawVolumetrics()
{
	VPROF_BUDGET( "CShadowMgr::DrawVolumetrics", VPROF_BUDGETGROUP_SHADOW_RENDERING );

	FlashlightHandle_t flashlightID = m_FlashlightStates.Head();
	if ( flashlightID == m_FlashlightStates.InvalidIndex() )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	for( ; flashlightID != m_FlashlightStates.InvalidIndex(); 
		flashlightID = m_FlashlightStates.Next( flashlightID ) )
	{
		FlashlightInfo_t &flashlightInfo = m_FlashlightStates[flashlightID];

		if ( flashlightInfo.m_nSplitscreenOwner >= 0 )
		{
			ASSERT_LOCAL_PLAYER_RESOLVABLE();
			if ( flashlightInfo.m_nSplitscreenOwner != GET_ACTIVE_SPLITSCREEN_SLOT() )
				continue;
		}

		FlashlightState_t &flashlightState = flashlightInfo.m_FlashlightState;

		// If this flashlight isn't volumetric, bail out here
		if ( !flashlightState.m_bVolumetric || ( flashlightState.m_flVolumetricIntensity <= 0.0f ) )
			continue;

		bool foundVar;
		IMaterial *pMaterial = materials->FindMaterial( "engine/lightshaft", TEXTURE_GROUP_OTHER, true );
		IMaterialVar *pCookieTextureVar = pMaterial->FindVar( "$COOKIETEXTURE", &foundVar, false );
		IMaterialVar *pNoiseTextureVar = pMaterial->FindVar( "$NOISETEXTURE", &foundVar, false );
		IMaterialVar *pCookieFrameNumVar = pMaterial->FindVar( "$COOKIEFRAMENUM", &foundVar, false );
		IMaterialVar *pShadowDepthTextureVar = pMaterial->FindVar( "$SHADOWDEPTHTEXTURE", &foundVar, false );
		IMaterialVar *pWorldToTextureVar = pMaterial->FindVar( "$WORLDTOTEXTURE", &foundVar, false );
		IMaterialVar *pFlashlightColorVar = pMaterial->FindVar( "$FLASHLIGHTCOLOR", &foundVar, false );
		IMaterialVar *pAttenFactorsVar = pMaterial->FindVar( "$ATTENFACTORS", &foundVar, false );
		IMaterialVar *pOriginFarZVar = pMaterial->FindVar( "$ORIGINFARZ", &foundVar, false );
		IMaterialVar *pQuatOrientation = pMaterial->FindVar( "$QUATORIENTATION", &foundVar, false );
		IMaterialVar *pShadowFilterSizeVar = pMaterial->FindVar( "$SHADOWFILTERSIZE", &foundVar, false );
		IMaterialVar *pShadowAttenVar = pMaterial->FindVar( "$SHADOWATTEN", &foundVar, false );
		IMaterialVar *pShadowJitterSeedVar = pMaterial->FindVar( "$SHADOWJITTERSEED", &foundVar, false );
		IMaterialVar *pUberlightVar = pMaterial->FindVar( "$UBERLIGHT", &foundVar, false );
		IMaterialVar *pEnableShadowsVar = pMaterial->FindVar( "$ENABLESHADOWS", &foundVar, false );
		IMaterialVar *pUberNearFarVar = pMaterial->FindVar( "$UBERNEARFAR", &foundVar, false );
		IMaterialVar *pUberHeightWidthVar = pMaterial->FindVar( "$UBERHEIGHTWIDTH", &foundVar, false );
		IMaterialVar *pUberRoundnessVar = pMaterial->FindVar( "$UBERROUNDNESS", &foundVar, false );
		IMaterialVar *pNoiseStrengthVar = pMaterial->FindVar( "$NOISESTRENGTH", &foundVar, false );
		IMaterialVar *pFlashlighTimeVar = pMaterial->FindVar( "$FLASHLIGHTTIME", &foundVar, false );
		IMaterialVar *pNumPlanesVar = pMaterial->FindVar( "$NUMPLANES", &foundVar, false );
		IMaterialVar *pVolumetricIntensityVar = pMaterial->FindVar( "$VOLUMETRICINTENSITY", &foundVar, false );

		if ( m_Shadows[flashlightInfo.m_Shadow].m_pFlashlightDepthTexture && pShadowDepthTextureVar )
		{
			pShadowDepthTextureVar->SetTextureValue( m_Shadows[flashlightInfo.m_Shadow].m_pFlashlightDepthTexture );
		}

		if ( flashlightState.m_pSpotlightTexture && pCookieTextureVar )
		{
			pCookieTextureVar->SetTextureValue( flashlightState.m_pSpotlightTexture );
		}

		if ( pNoiseTextureVar )
		{
			pNoiseTextureVar->SetTextureValue( materials->FindTexture( "effects/noise_rg", TEXTURE_GROUP_OTHER, true ) );
		}

		if ( pCookieFrameNumVar )
		{
			pCookieFrameNumVar->SetIntValue( flashlightState.m_nSpotlightTextureFrame );
		}

		if ( pWorldToTextureVar )
		{
			pWorldToTextureVar->SetMatrixValue( m_Shadows[flashlightInfo.m_Shadow].m_WorldToShadow );
		}

		if ( pFlashlightColorVar )
		{
			pFlashlightColorVar->SetVecValue( &(flashlightState.m_Color[0]), 4 );
		}

		if ( pAttenFactorsVar )
		{
			pAttenFactorsVar->SetVecValue( flashlightState.m_fConstantAtten, flashlightState.m_fLinearAtten, flashlightState.m_fQuadraticAtten, flashlightState.m_FarZAtten );
		}

		if ( pOriginFarZVar )
		{
			pOriginFarZVar->SetVecValue( flashlightState.m_vecLightOrigin[0], flashlightState.m_vecLightOrigin[1], flashlightState.m_vecLightOrigin[2], flashlightState.m_FarZ );
		}

		if ( pQuatOrientation )
		{
			pQuatOrientation->SetVecValue( flashlightState.m_quatOrientation.Base(), 4 );
		}

		if ( pShadowFilterSizeVar )
		{
			pShadowFilterSizeVar->SetFloatValue( flashlightState.m_flShadowFilterSize );
		}

		if ( pShadowAttenVar )
		{
			pShadowAttenVar->SetFloatValue( flashlightState.m_flShadowAtten );
		}

		if ( pShadowJitterSeedVar )
		{
			pShadowJitterSeedVar->SetFloatValue( flashlightState.m_flShadowJitterSeed );
		}

		if ( pUberlightVar )
		{
			pUberlightVar->SetIntValue( flashlightState.m_bUberlight ? 1 : 0 );
		}

		if ( pEnableShadowsVar )
		{
			pEnableShadowsVar->SetIntValue( flashlightState.m_bEnableShadows ? 1 : 0 );
		}

		if ( pUberNearFarVar )
		{
			pUberNearFarVar->SetVecValue( flashlightState.m_uberlightState.m_fNearEdge,	flashlightState.m_uberlightState.m_fFarEdge,
										  flashlightState.m_uberlightState.m_fCutOn, flashlightState.m_uberlightState.m_fCutOff );
		}

		if ( pUberHeightWidthVar )
		{
			pUberHeightWidthVar->SetVecValue( flashlightState.m_uberlightState.m_fWidth, flashlightState.m_uberlightState.m_fWedge,
											  flashlightState.m_uberlightState.m_fHeight, flashlightState.m_uberlightState.m_fHedge );
		}

		if ( pUberRoundnessVar )
		{
			pUberRoundnessVar->SetFloatValue( flashlightState.m_uberlightState.m_fRoundness );
		}

		if ( pNoiseStrengthVar )
		{
			pNoiseStrengthVar->SetFloatValue( flashlightState.m_flNoiseStrength );
		}

		if ( pFlashlighTimeVar )
		{
			pFlashlighTimeVar->SetFloatValue( flashlightState.m_flFlashlightTime );
		}

		if ( pNumPlanesVar )
		{
			pNumPlanesVar->SetIntValue( flashlightState.m_nNumPlanes );
		}

		if ( pVolumetricIntensityVar )
		{
 			pVolumetricIntensityVar->SetFloatValue( flashlightState.m_flVolumetricIntensity );
		}


		pRenderContext->Bind( pMaterial );

		// Set up user clip planes to clip to this flashlight's frustum
		VPlane planes[FRUSTUM_NUMPLANES];
		flashlightInfo.m_Frustum.GetPlanes( planes );

		for ( int i = 0; i < 6; i++ )
		{
			pRenderContext->PushCustomClipPlane( planes[i].m_Normal.Base() );
		}

		// Flashlight space to world space, then view space
		VMatrix matWorldToView, matViewToWorld, matFlashlightToView, matViewToFlashlight;
		pRenderContext->GetMatrix( MATERIAL_VIEW, &matWorldToView );
		MatrixInverseGeneral( matWorldToView, matViewToWorld );
		MatrixMultiply( m_Shadows[flashlightInfo.m_Shadow].m_WorldToShadow, matViewToWorld, matViewToFlashlight );
		MatrixInverseGeneral( matViewToFlashlight, matFlashlightToView );

		// View space AABB
		Vector vView, vWorld, vViewMins, vViewMaxs;
		CalculateAABBFromProjectionMatrixInverse( matFlashlightToView, &vViewMins, &vViewMaxs );

		// Distance between planes
		float zIncrement = ( vViewMaxs.z - vViewMins.z ) / (float) flashlightState.m_nNumPlanes;

		// Base offset for this set of planes (progressive refinement sets this in SFM)
		vViewMins.z += zIncrement * flashlightState.m_flPlaneOffset;

		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PushMatrix();
		pRenderContext->LoadIdentity();

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, flashlightState.m_nNumPlanes );

		for (int i = 0; i < flashlightState.m_nNumPlanes; i++ )
		{
			vView = Vector( vViewMins.x, vViewMins.y, vViewMins.z + (float)i * zIncrement );// View space
			Vector3DMultiplyPosition( matViewToWorld, vView, vWorld );						// Transform to world space
			meshBuilder.Position3fv( vWorld.Base() );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 0>();

			vView = Vector( vViewMins.x, vViewMaxs.y, vViewMins.z + (float)i * zIncrement );// View space
			Vector3DMultiplyPosition( matViewToWorld, vView, vWorld );						// Transform to world space
			meshBuilder.Position3fv( vWorld.Base() );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 0>();

			vView = Vector( vViewMaxs.x, vViewMaxs.y, vViewMins.z + (float)i * zIncrement );// View space
			Vector3DMultiplyPosition( matViewToWorld, vView, vWorld );						// Transform to world space
			meshBuilder.Position3fv( vWorld.Base() );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 0>();

			vView = Vector( vViewMaxs.x, vViewMins.y, vViewMins.z + (float)i * zIncrement );// View space
			Vector3DMultiplyPosition( matViewToWorld, vView, vWorld );						// Transform to world space
			meshBuilder.Position3fv( vWorld.Base() );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 0>();
		}

		meshBuilder.End( false, true );

		pRenderContext->PopMatrix();

		// Pop the custom clip planes
		for ( int i=0; i<6; i++ )
		{
			pRenderContext->PopCustomClipPlane();
		}
	}
}



void CShadowMgr::DrawFlashlightDecals( IMatRenderContext *pRenderContext, int sortGroup, bool bDoMasking, float flFade )
{
	VPROF_BUDGET( "CShadowMgr::DrawFlashlightDecals", VPROF_BUDGETGROUP_SHADOW_RENDERING );

	if ( m_bSinglePassFlashlightStateEnabled )
		return;

	FlashlightHandle_t flashlightID = m_FlashlightStates.Head();
	if ( flashlightID == m_FlashlightStates.InvalidIndex() )
		return;

	pRenderContext->SetFlashlightMode( true );

	for( ; 
		 flashlightID != m_FlashlightStates.InvalidIndex(); 
		 flashlightID = m_FlashlightStates.Next( flashlightID ) )
	{
		FlashlightInfo_t &flashlightInfo = m_FlashlightStates[flashlightID];
		if ( ( m_Shadows[flashlightInfo.m_Shadow].m_Flags & ( SHADOW_SIMPLE_PROJECTION ) ) != 0 )
		{
			continue;
		}

		if ( flashlightInfo.m_nSplitscreenOwner >= 0 )
		{
			ASSERT_LOCAL_PLAYER_RESOLVABLE();
			if ( flashlightInfo.m_nSplitscreenOwner != GET_ACTIVE_SPLITSCREEN_SLOT() )
				continue;
		}

		pRenderContext->SetFlashlightStateEx( flashlightInfo.m_FlashlightState, m_Shadows[flashlightInfo.m_Shadow].m_WorldToShadow, m_Shadows[flashlightInfo.m_Shadow].m_pFlashlightDepthTexture );

		EnableStencilAndScissorMasking( pRenderContext, flashlightInfo, bDoMasking );

		DecalSurfaceDraw( pRenderContext, sortGroup, flFade );

		DisableStencilAndScissorMasking( pRenderContext, flashlightInfo, bDoMasking );
	}
	
	// Tell the materialsystem that we are finished drawing additive flashlight lighting.
	pRenderContext->SetFlashlightMode( false );
}

void CShadowMgr::FlashlightDrawCallback( ShadowDrawCallbackFn_t pCallback, void *pData )
{
	VPROF_BUDGET( "CShadowMgr::FlashlightDrawCallback", VPROF_BUDGETGROUP_SHADOW_RENDERING );

	if ( m_bSinglePassFlashlightStateEnabled )
		return;

	FlashlightHandle_t flashlightID = m_FlashlightStates.Head();
	if ( flashlightID == m_FlashlightStates.InvalidIndex() )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->SetFlashlightMode( true );

	for( ; 
		flashlightID != m_FlashlightStates.InvalidIndex(); 
		flashlightID = m_FlashlightStates.Next( flashlightID ) )
	{
		FlashlightInfo_t &flashlightInfo = m_FlashlightStates[flashlightID];

		if ( flashlightInfo.m_nSplitscreenOwner >= 0 )
		{
			ASSERT_LOCAL_PLAYER_RESOLVABLE();
			if ( flashlightInfo.m_nSplitscreenOwner != GET_ACTIVE_SPLITSCREEN_SLOT() )
				continue;
		}

		pRenderContext->SetFlashlightStateEx( flashlightInfo.m_FlashlightState, m_Shadows[flashlightInfo.m_Shadow].m_WorldToShadow, m_Shadows[flashlightInfo.m_Shadow].m_pFlashlightDepthTexture );

		EnableStencilAndScissorMasking( pRenderContext, flashlightInfo, false );

		pCallback( pData );

		DisableStencilAndScissorMasking( pRenderContext, flashlightInfo, false );
	}

	// Tell the materialsystem that we are finished drawing additive flashlight lighting.
	pRenderContext->SetFlashlightMode( false );
}

void CShadowMgr::SetSinglePassFlashlightRenderState( ShadowHandle_t handle )
{
	m_hSinglePassFlashlightState = handle;
	//materials->GetRenderContext()->Flush();
	if( m_bSinglePassFlashlightStateEnabled )
	{
		SetFlashlightRenderState( m_hSinglePassFlashlightState );
	}
	else
	{
		//TODO: Look for headaches of changing the handle while we're not supposed to be in control
		SetFlashlightRenderState( SHADOW_HANDLE_INVALID );
	}
}

void CShadowMgr::PushSinglePassFlashlightStateEnabled( bool bEnable )
{
	m_bStack_SinglePassFlashlightStateEnabled.Push( bEnable );	
	if( m_bSinglePassFlashlightStateEnabled != bEnable )
	{
		materials->GetRenderContext()->EnableSinglePassFlashlightMode( bEnable );

		if( bEnable )
			SetFlashlightRenderState( m_hSinglePassFlashlightState );
		else
			SetFlashlightRenderState( SHADOW_HANDLE_INVALID );

		m_bSinglePassFlashlightStateEnabled = bEnable;
	}
	if ( m_bSinglePassFlashlightStateEnabled )
	{
		// Only enable culling for single pass flashlight if NOT in splitscreen
		materials->GetRenderContext()->EnableCullingForSinglePassFlashlight( r_flashlight_always_cull_for_single_pass.GetBool() || ( GET_NUM_SPLIT_SCREEN_PLAYERS() < 2 ) );
	}
}

void CShadowMgr::PopSinglePassFlashlightStateEnabled( void )
{
	m_bStack_SinglePassFlashlightStateEnabled.Pop();
	bool bEnable = IsGameConsole();
	if( m_bStack_SinglePassFlashlightStateEnabled.Count() != 0 )
	{
		bEnable = m_bStack_SinglePassFlashlightStateEnabled.Top();
	}

	if( m_bSinglePassFlashlightStateEnabled != bEnable )
	{
		materials->GetRenderContext()->EnableSinglePassFlashlightMode( bEnable );

		if( bEnable )
			SetFlashlightRenderState( m_hSinglePassFlashlightState );
		else
			SetFlashlightRenderState( SHADOW_HANDLE_INVALID );

		m_bSinglePassFlashlightStateEnabled = bEnable;
	}
	if ( m_bSinglePassFlashlightStateEnabled )
	{
		// Only enable culling for single pass flashlight if NOT in splitscreen
		materials->GetRenderContext()->EnableCullingForSinglePassFlashlight( r_flashlight_always_cull_for_single_pass.GetBool() || ( GET_NUM_SPLIT_SCREEN_PLAYERS() < 2 ) );
	}
}

bool CShadowMgr::SinglePassFlashlightModeEnabled( void )
{
	return m_bSinglePassFlashlightStateEnabled;
}

void CShadowMgr::DrawFlashlightDecalsOnDisplacements( IMatRenderContext *pRenderContext, int sortGroup, CDispInfo *visibleDisps[MAX_MAP_DISPINFO], int nVisibleDisps, bool bDoMasking )
{
	VPROF_BUDGET( "CShadowMgr::DrawFlashlightDecalsOnDisplacements", VPROF_BUDGETGROUP_SHADOW_RENDERING );

	if ( m_bSinglePassFlashlightStateEnabled )
		return;

	FlashlightHandle_t flashlightID = m_FlashlightStates.Head();
	if ( flashlightID == m_FlashlightStates.InvalidIndex() )
		return;

	pRenderContext->SetFlashlightMode( true );

	DispInfo_BatchDecals( visibleDisps, nVisibleDisps );

	for( ; 
		flashlightID != m_FlashlightStates.InvalidIndex(); 
		flashlightID = m_FlashlightStates.Next( flashlightID ) )
	{
		FlashlightInfo_t &flashlightInfo = m_FlashlightStates[flashlightID];

		if ( flashlightInfo.m_nSplitscreenOwner >= 0 )
		{
			ASSERT_LOCAL_PLAYER_RESOLVABLE();
			if ( flashlightInfo.m_nSplitscreenOwner != GET_ACTIVE_SPLITSCREEN_SLOT() )
				continue;
		}

		pRenderContext->SetFlashlightStateEx( flashlightInfo.m_FlashlightState, m_Shadows[flashlightInfo.m_Shadow].m_WorldToShadow, m_Shadows[flashlightInfo.m_Shadow].m_pFlashlightDepthTexture );

		EnableStencilAndScissorMasking( pRenderContext, flashlightInfo, bDoMasking );

		DispInfo_DrawDecals( pRenderContext, visibleDisps, nVisibleDisps );

		DisableStencilAndScissorMasking( pRenderContext, flashlightInfo, bDoMasking );
	}

	// Tell the materialsystem that we are finished drawing additive flashlight lighting.
	pRenderContext->SetFlashlightMode( false );
}

void CShadowMgr::DrawFlashlightDecalsOnSurfaceList( IMatRenderContext *pRenderContext, SurfaceHandle_t *pList, int listCount, bool bDoMasking )
{
	VPROF_BUDGET( "CShadowMgr::DrawFlashlightDecalsOnSurfaceList", VPROF_BUDGETGROUP_SHADOW_RENDERING );

	if ( m_bSinglePassFlashlightStateEnabled )
		return;

	FlashlightHandle_t flashlightID = m_FlashlightStates.Head();
	if ( flashlightID == m_FlashlightStates.InvalidIndex() )
		return;

	pRenderContext->SetFlashlightMode( true );

	for( ; 
		 flashlightID != m_FlashlightStates.InvalidIndex(); 
		 flashlightID = m_FlashlightStates.Next( flashlightID ) )
	{
		FlashlightInfo_t &flashlightInfo = m_FlashlightStates[flashlightID];

		if ( flashlightInfo.m_nSplitscreenOwner >= 0 )
		{
			ASSERT_LOCAL_PLAYER_RESOLVABLE();
			if ( flashlightInfo.m_nSplitscreenOwner != GET_ACTIVE_SPLITSCREEN_SLOT() )
				continue;
		}

		pRenderContext->SetFlashlightStateEx( flashlightInfo.m_FlashlightState, m_Shadows[flashlightInfo.m_Shadow].m_WorldToShadow, m_Shadows[flashlightInfo.m_Shadow].m_pFlashlightDepthTexture );

		EnableStencilAndScissorMasking( pRenderContext, flashlightInfo, bDoMasking );

		for ( int i = 0; i < listCount; i++ )
		{
			if ( MSurf_Decals(pList[i]) != WORLD_DECAL_HANDLE_INVALID )
			{
				DrawDecalsOnSingleSurface( pRenderContext, pList[i] );
			}
		}

		DisableStencilAndScissorMasking( pRenderContext, flashlightInfo, bDoMasking );
	}
	
	// Tell the materialsystem that we are finished drawing additive flashlight lighting.
	pRenderContext->SetFlashlightMode( false );
}

void CShadowMgr::DrawFlashlightOverlays( IMatRenderContext *pRenderContext, int nSortGroup, bool bDoMasking )
{
	VPROF_BUDGET( "CShadowMgr::DrawFlashlightOverlays", VPROF_BUDGETGROUP_SHADOW_RENDERING );

	if ( m_bSinglePassFlashlightStateEnabled )
		return;

	FlashlightHandle_t flashlightID = m_FlashlightStates.Head();
	if ( flashlightID == m_FlashlightStates.InvalidIndex() )
		return;

	if ( r_flashlightrender.GetBool()==false )
		return;

	pRenderContext->SetFlashlightMode( true );

	for( ; 
		 flashlightID != m_FlashlightStates.InvalidIndex(); 
		 flashlightID = m_FlashlightStates.Next( flashlightID ) )
	{
		FlashlightInfo_t &flashlightInfo = m_FlashlightStates[flashlightID];

		if ( flashlightInfo.m_nSplitscreenOwner >= 0 )
		{
			ASSERT_LOCAL_PLAYER_RESOLVABLE();
			if ( flashlightInfo.m_nSplitscreenOwner != GET_ACTIVE_SPLITSCREEN_SLOT() )
				continue;
		}

		pRenderContext->SetFlashlightStateEx( flashlightInfo.m_FlashlightState, m_Shadows[flashlightInfo.m_Shadow].m_WorldToShadow, m_Shadows[flashlightInfo.m_Shadow].m_pFlashlightDepthTexture );

		EnableStencilAndScissorMasking( pRenderContext, flashlightInfo, bDoMasking );

		OverlayMgr()->RenderOverlays( pRenderContext, nSortGroup );

		DisableStencilAndScissorMasking( pRenderContext, flashlightInfo, bDoMasking );
	}
	
	// Tell the materialsystem that we are finished drawing additive flashlight lighting.
	pRenderContext->SetFlashlightMode( false );
}

void CShadowMgr::DrawFlashlightDepthTexture( )
{
	int i = 0;
	for ( FlashlightHandle_t flashlightID = m_FlashlightStates.Head();
		  flashlightID != m_FlashlightStates.InvalidIndex();
		  flashlightID = m_FlashlightStates.Next( flashlightID ) ) // Count up the shadows
	{
		FlashlightInfo_t &flashlightInfo = m_FlashlightStates[ flashlightID ];
		
		if ( flashlightInfo.m_nSplitscreenOwner >= 0 )
		{
			ASSERT_LOCAL_PLAYER_RESOLVABLE();
			if ( flashlightInfo.m_nSplitscreenOwner != GET_ACTIVE_SPLITSCREEN_SLOT() )
			{
				continue;
			}
		}

		if ( m_Shadows[ flashlightInfo.m_Shadow ].m_pFlashlightDepthTexture )
		{
			bool foundVar;
			IMaterial *pMaterial = materials->FindMaterial( "debug/showz", TEXTURE_GROUP_OTHER, true );
			IMaterialVar *BaseTextureVar = pMaterial->FindVar( "$basetexture", &foundVar, false );
			if (!foundVar)
				return;
			IMaterialVar *FrameVar = pMaterial->FindVar( "$frame", &foundVar, false );
			if (!foundVar)
				return;

			float w, h;
			w = h = r_flashlightdrawdepthres.GetFloat();

			float wOffset = (i % 2) * 256.0f;	// Even|Odd go left|right
			float hOffset = (i / 2) * 256.0f;	// Rows of two

			BaseTextureVar->SetTextureValue( m_Shadows[ flashlightInfo.m_Shadow ].m_pFlashlightDepthTexture );
			FrameVar->SetIntValue( 0 );

			CMatRenderContextPtr pRenderContext( materials );

			pRenderContext->Bind( pMaterial );
			IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

			CMeshBuilder meshBuilder;
			meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

			meshBuilder.Position3f( wOffset, hOffset, 0.0f );
#ifdef DX_TO_GL_ABSTRACTION
			meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );					// Posix is rotated due to render target origin differences
#else
			meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
#endif
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 1>();

			meshBuilder.Position3f( wOffset + w, hOffset, 0.0f );
#ifdef DX_TO_GL_ABSTRACTION
			meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
#else
			meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
#endif
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 1>();

			meshBuilder.Position3f( wOffset + w, hOffset + h, 0.0f );
#ifdef DX_TO_GL_ABSTRACTION
			meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
#else
			meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
#endif
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 1>();

			meshBuilder.Position3f( wOffset, hOffset + h, 0.0f );
#ifdef DX_TO_GL_ABSTRACTION
			meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
#else
			meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
#endif			
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 1>();

			meshBuilder.End();
			pMesh->Draw();

			i++;
		}
	}
}

void CShadowMgr::SkipShadowForEntity( int nEntIndex )
{
	m_nSkipShadowForEntIndex = nEntIndex;
}
