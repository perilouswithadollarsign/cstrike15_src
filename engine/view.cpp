//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "quakedef.h"
#include "gl_model_private.h"
#include "console.h"
#include "cdll_engine_int.h"
#include "gl_cvars.h"
#include "ivrenderview.h"
#include "gl_matsysiface.h"
#include "gl_drawlights.h"
#include "gl_rsurf.h"
#include "r_local.h"
#include "debugoverlay.h"
#include "vgui_baseui_interface.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "demo.h"
#include "istudiorender.h"
#include "materialsystem/imesh.h"
#include "tier0/vprof.h"
#include "host.h"
#include "view.h"
#include "client.h"
#include "sys.h"
#include "cl_main.h"
#include "l_studio.h"
#include "IOcclusionSystem.h"
#include "cl_demouipanel.h"
#include "mod_vis.h"
#include "ivideomode.h"
#include "gl_shader.h"
#include "gl_rmain.h"
#include "engine/view_sharedv1.h"
#include "ispatialpartitioninternal.h"
#include "toolframework/itoolframework.h"
#include "tier1/callqueue.h"
#include "filesystem/IQueuedLoader.h"
#include "r_decal.h"

#ifdef _PS3
#include "tls_ps3.h"
#endif

#if defined( INCLUDE_SCALEFORM )
#include "scaleformui/scaleformui.h"
#endif
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class IClientEntity;

float r_blend;
float r_colormod[3] = { 1, 1, 1 };
bool g_bIsBlendingOrModulating = false;

bool g_bIsRenderingVGuiOnly = false;

colorVec R_LightPoint (Vector& p);
void R_DrawLightmaps( IWorldRenderList *pList, int pageId );
void R_DrawIdentityBrushModel( IWorldRenderList *pRenderList, model_t *model );
static ConVar r_brush_queue_mode( "r_brush_queue_mode", "0", FCVAR_CHEAT );

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

extern ConVar r_avglightmap;

/*
=================
V_CheckGamma

FIXME:  Define this as a change function to the ConVar's below rather than polling it
 every frame.  Note, still need to make sure it gets called very first time through frame loop.
=================
*/
bool V_CheckGamma( void )
{
	if ( IsX360() )
		return false;

	static int lastLightmap = -1;
	extern void GL_RebuildLightmaps( void );
	
	// Refresh all lightmaps if r_avglightmap changes
	if ( r_avglightmap.GetInt() != lastLightmap )
	{
		lastLightmap = r_avglightmap.GetInt();
		GL_RebuildLightmaps();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the view renderer
// Output : void V_Init
//-----------------------------------------------------------------------------
void V_Init( void )
{
	BuildGammaTable( 2.2f, 2.2f, 0.0f, 2 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void V_Shutdown( void )
{
	// TODO, cleanup
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void V_RenderVGuiOnly_NoSwap()
{
	// Need to clear the screen in this case, cause we're not drawing
	// the loading screen.
	UpdateMaterialSystemConfig();

	CMatRenderContextPtr pRenderContext( materials );
	
	pRenderContext->AntiAliasingHint( AA_HINT_MENU ); // would be better to do "Disable MLAA" here
		   
	pRenderContext->ClearBuffers( true, true );

#if defined( INCLUDE_SCALEFORM )
	// Render scaleform before vgui
	pRenderContext->AdvanceAndRenderScaleformSlot( SF_FULL_SCREEN_SLOT );
#endif

	EngineVGui()->Paint( PAINT_UIPANELS );

#if defined( INCLUDE_SCALEFORM )
	// Render cursor after vgui
	pRenderContext->AdvanceAndRenderScaleformCursor();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Renders only vgui (for loading progress) including buffer swapping and vgui simulation
//-----------------------------------------------------------------------------

bool s_bTriggeredHostError = false;

void V_RenderVGuiOnly( void )
{
	materials->BeginFrame( host_frametime );

	CMatRenderContextPtr pRenderContext;
	pRenderContext.GetFrom( materials );
	pRenderContext->RenderScaleformSlot(SF_RESERVED_BEGINFRAME_SLOT);
	pRenderContext.SafeRelease();

	EngineVGui()->Simulate();

	g_EngineRenderer->FrameBegin();

	toolframework->RenderFrameBegin();

	V_RenderVGuiOnly_NoSwap();

	toolframework->RenderFrameEnd();

	g_EngineRenderer->FrameEnd( );

	pRenderContext.GetFrom( materials );
	pRenderContext->RenderScaleformSlot(SF_RESERVED_ENDFRAME_SLOT);
	pRenderContext.SafeRelease();

	materials->EndFrame();

	Shader_SwapBuffers();

#ifdef _PS3
	if ( GetTLSGlobals()->bNormalQuitRequested )
	{
		// hack to prevent PS/3 deadlock on queued loader render mutex when quitting during loading a map
		uint nUnlockedQueuedRenderer = g_pQueuedLoader ? g_pQueuedLoader->UnlockProgressBarMutex() : 0;
		
		if ( !s_bTriggeredHostError )
		{
			Assert( ThreadInMainThread() );
			s_bTriggeredHostError = true;
			Host_Error( "SystemQuitRequest" );
		}
		// hack to prevent PS/3 deadlock on queued loader render mutex when quitting during loading a map
		if( g_pQueuedLoader )
		{
			g_pQueuedLoader->LockProgressBarMutex( nUnlockedQueuedRenderer );
		}
	}
#endif
}



//-----------------------------------------------------------------------------
// Purpose: Render the world
//-----------------------------------------------------------------------------
void V_RenderView( void )
{
	VPROF( "V_RenderView" );
	MDLCACHE_COARSE_LOCK_(g_pMDLCache);

	bool bCanRenderWorld = ( host_state.worldmodel != NULL ) && GetBaseLocalClient().IsActive();

	bCanRenderWorld = bCanRenderWorld && !EngineVGui()->IsPlayingFullScreenVideo();

	bCanRenderWorld = bCanRenderWorld && toolframework->ShouldGameRenderView();

	if ( IsPC() && bCanRenderWorld && g_bTextMode )
	{	
		// Sleep to let the other textmode clients get some cycles.
		Sys_Sleep( 15 );
		bCanRenderWorld = false;
	}

	// Update stereo layer
	if ( !g_LostVideoMemory )
	{
		if ( materials && materials->IsStereoSupported() )
		{
			materials->NVStereoUpdate();
		}
	}

	if ( !bCanRenderWorld )
	{
		// Because we now do a lot of downloading before spawning map, don't render anything world related 
		// until we are an active client.
		V_RenderVGuiOnly_NoSwap();
	}
	else if ( !g_LostVideoMemory )
	{
		// since we know we're going to render the world, check for lightmap updates while it is easy
		// to tear down and rebuild
		R_CheckForLightingConfigChanges();
		R_CheckForPaintmapChanges();

		// Pass game time to shader api
		{
			CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
			pRenderContext->UpdateGameTime( g_ClientGlobalVariables.curtime );
		}

		// We can get into situations where some other material system app
		// is trying to start up; in those cases, we shouldn't render...
		vrect_t scr_vrect = videomode->GetClientViewRect();
		g_ClientDLL->View_Render( &scr_vrect );

		// Cleanup all of the immediate cleanup decals here, after we've rendered the views.
		R_DecalFlushDestroyList( true );
	}
}

void Linefile_Draw( void );


//-----------------------------------------------------------------------------
// Purpose: Expose rendering interface to client .dll
//-----------------------------------------------------------------------------
class CVRenderView : public IVRenderView, public ISpatialLeafEnumerator
{
public:
	void TouchLight( dlight_t *light )
	{
		int i;
		
		i = light - cl_dlights;
		if (i >= 0 && i < MAX_DLIGHTS)
		{
			r_dlightchanged |= (1 << i);
		}
	}

	void DrawBrushModel( 
		IClientEntity *baseentity, 
		model_t *model, 
		const Vector& origin, 
		const QAngle& angles, 
		bool bUnused )
	{
		R_DrawBrushModel( baseentity, model, origin, angles, DEPTH_MODE_NORMAL, true, true );
	}

	virtual void DrawBrushModelEx( IClientEntity *baseentity, model_t *model, const Vector& origin, const QAngle& angles, DrawBrushModelMode_t mode )
	{
		bool bDrawOpaque = ( mode != DBM_DRAW_TRANSLUCENT_ONLY );
		bool bDrawTranslucent = ( mode != DBM_DRAW_OPAQUE_ONLY );
		R_DrawBrushModel( baseentity, model, origin, angles, DEPTH_MODE_NORMAL, bDrawOpaque, bDrawTranslucent );
	}

	virtual void DrawBrushModelArray( IMatRenderContext* pRenderContext, int nCount, 
		const BrushArrayInstanceData_t *pInstanceData, int nModelTypeFlags )
	{
#ifdef _DEBUG
		for ( int i = 0; i < nCount; ++i )
		{
			Assert( pRenderContext->IsRenderData( pInstanceData[i].m_pBrushToWorld ) );
		}
#endif

		ICallQueue *pCallQueue = pRenderContext->GetCallQueue();
		if ( !pCallQueue || r_brush_queue_mode.GetInt() == 0 )
		{
			R_DrawBrushModelArray( pRenderContext, nCount, pInstanceData, nModelTypeFlags );
		}
		else
		{
			CMatRenderData< BrushArrayInstanceData_t > brushArrayData( pRenderContext );
			if ( !pRenderContext->IsRenderData( pInstanceData ) )
			{
				pInstanceData = brushArrayData.Lock( nCount, pInstanceData );
			}

			pCallQueue->QueueCall( R_DrawBrushModelArray, nCount, pInstanceData, nModelTypeFlags );
		}
	}

	// Draw brush model shadow
	void DrawBrushModelShadow( IClientRenderable *pRenderable )
	{
		R_DrawBrushModelShadow( pRenderable );
	}

	void DrawIdentityBrushModel( IWorldRenderList *pList, model_t *model )
	{
		R_DrawIdentityBrushModel( pList, model );
	}

	void Draw3DDebugOverlays( void )
	{
		DrawSavedModelDebugOverlays();

		if ( g_pDemoUI )
		{
			g_pDemoUI->DrawDebuggingInfo();
		}

		//if ( g_pDemoUI2 )
		//{
		//	g_pDemoUI2->DrawDebuggingInfo();
		//}

		SpatialPartition()->DrawDebugOverlays();

		CDebugOverlay::Draw3DOverlays();

		// Render occlusion debugging info
		OcclusionSystem()->DrawDebugOverlays();
	}

	FORCEINLINE void CheckBlend( void )
	{
		g_bIsBlendingOrModulating = ( r_blend != 1.0 ) || 
			( r_colormod[0] != 1.0 ) || ( r_colormod[1] != 1.0 ) || ( r_colormod[2] != 1.0 );

	}
	void SetBlend( float blend )
	{
		r_blend = blend;
		CheckBlend();
	}

	float GetBlend( void )
	{
		return r_blend;
	}

	void SetColorModulation( float const* blend )
	{
		VectorCopy( blend, r_colormod );
		CheckBlend();
	}

	void GetColorModulation( float* blend )
	{
		VectorCopy( r_colormod, blend );
	}

	void SceneBegin( void )
	{
		g_EngineRenderer->DrawSceneBegin();
	}

	void SceneEnd( void )
	{
		g_EngineRenderer->DrawSceneEnd();
	}
	 
	void GetVisibleFogVolume( const Vector& vEyePoint, const VisOverrideData_t *pVisOverrideData, VisibleFogVolumeInfo_t *pInfo )
	{
		R_GetVisibleFogVolume( vEyePoint, pVisOverrideData, pInfo );
	}
	
	IWorldRenderList * CreateWorldList()
	{
		return g_EngineRenderer->CreateWorldList();
	}

#if defined(_PS3)
	IWorldRenderList * CreateWorldList_PS3( int viewID )
	{
		return g_EngineRenderer->CreateWorldList_PS3( viewID );
	}

	void BuildWorldLists_PS3_Epilogue( IWorldRenderList *pList, WorldListInfo_t* pInfo, bool bShadowDepth )
	{
		g_EngineRenderer->BuildWorldLists_PS3_Epilogue( pList, pInfo, bShadowDepth );
	}
#else
	void BuildWorldLists_Epilogue( IWorldRenderList *pList, WorldListInfo_t* pInfo, bool bShadowDepth )
	{
		g_EngineRenderer->BuildWorldLists_Epilogue( pList, pInfo, bShadowDepth );
	}
#endif

	void BuildWorldLists( IWorldRenderList *pList, WorldListInfo_t* pInfo, int iForceFViewLeaf, const VisOverrideData_t* pVisData, bool bShadowDepth, float *pReflectionWaterHeight )
	{
		g_EngineRenderer->BuildWorldLists( pList, pInfo, iForceFViewLeaf, pVisData, bShadowDepth, pReflectionWaterHeight );
	}

	void DrawWorldLists( IMatRenderContext *pRenderContext, IWorldRenderList *pList, unsigned long flags, float waterZAdjust )
	{
		g_EngineRenderer->DrawWorldLists( pRenderContext, pList, flags, waterZAdjust );
	}

	void GetWorldListIndicesInfo( WorldListIndicesInfo_t * pIndicesInfoOut, IWorldRenderList *pList, unsigned long nFlags )
	{
		return R_GetWorldListIndicesInfo( pIndicesInfoOut, pList, nFlags );
	}

	// Optimization for top view
	void DrawTopView( bool enable )
	{
		R_DrawTopView( enable );
	}

	void TopViewNoBackfaceCulling( bool bDisable )
	{
		R_TopViewNoBackfaceCulling( bDisable );
	}

	void TopViewNoVisCheck( bool bDisable )
	{
		R_TopViewNoVisCheck( bDisable );
	}

	void TopViewBounds( Vector2D const& mins, Vector2D const& maxs )
	{
		R_TopViewBounds( mins, maxs );
	}

	void SetTopViewVolumeCuller( const CVolumeCuller *pVolumeCuller )
	{
		R_SetTopViewVolumeCuller( pVolumeCuller );
	}

	void DrawLights( void )
	{
		DrawLightSprites();

#ifdef USE_CONVARS
		DrawLightDebuggingInfo();
#endif
	}

	void DrawMaskEntities( void )
	{
		// UNDONE: Don't do this with masked brush models, they should probably be in a separate list
		// R_DrawMaskEntities()
	}

	void DrawTranslucentSurfaces( IMatRenderContext *pRenderContext, IWorldRenderList *pList, int *pSortList, int sortCount, unsigned long flags )
	{
		Shader_DrawTranslucentSurfaces( pRenderContext, pList, pSortList, sortCount, flags );
	}

	bool LeafContainsTranslucentSurfaces( IWorldRenderList *pList, int sortIndex, unsigned long flags )
	{
		return Shader_LeafContainsTranslucentSurfaces( pList, sortIndex, flags );
	}

	void DrawLineFile( void )
	{
		Linefile_Draw();
	}

	void DrawLightmaps( IWorldRenderList *pList, int pageId )
	{
		R_DrawLightmaps( pList, pageId );
	}

	void ViewSetupVis( bool novis, int numorigins, const Vector origin[] )
	{
		g_EngineRenderer->ViewSetupVis( novis, numorigins, origin );
	}

	void ViewSetupVisEx( bool novis, int numorigins, const Vector origin[], unsigned int &returnFlags )
	{
		g_EngineRenderer->ViewSetupVisEx( novis, numorigins, origin, returnFlags );
	}

	bool AreAnyLeavesVisible( int *leafList, int nLeaves )
	{
		return Map_AreAnyLeavesVisible( *host_state.worldbrush, leafList, nLeaves );
	}

	// For backward compatibility only!!!
	void VguiPaint( void )
	{
		EngineVGui()->BackwardCompatibility_Paint();
	}

	void VGui_Paint( int mode )
	{
		EngineVGui()->Paint( (PaintMode_t)mode );
	}

	void ViewDrawFade( byte *color, IMaterial* pFadeMaterial, bool mapFullTextureToScreen )
	{
		VPROF_BUDGET( "ViewDrawFade", VPROF_BUDGETGROUP_WORLD_RENDERING );
		g_EngineRenderer->ViewDrawFade( color, pFadeMaterial, mapFullTextureToScreen );
	}

	void OLD_SetProjectionMatrix( float fov, float zNear, float zFar )
	{
		// Here to preserve backwards compat
	}

	void OLD_SetOffCenterProjectionMatrix( float fov, float zNear, float zFar, float flAspectRatio,
		float flBottom, float flTop, float flLeft, float flRight )
	{
		// Here to preserve backwards compat
	}

	void OLD_SetProjectionMatrixOrtho( float left, float top, float right, float bottom, float zNear, float zFar )
	{
		// Here to preserve backwards compat
	}

	colorVec GetLightAtPoint( Vector& pos )
	{
		return R_LightPoint( pos );
	}

	int GetViewEntity( void )
	{
		return GetLocalClient().GetViewEntity();
	}

	bool IsViewEntity( int entindex )
	{
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
		{
			if ( GetLocalClient( i ).GetViewEntity() == entindex )
				return true;
		}
		return false;
	}

	float GetFieldOfView( void )
	{
		return g_EngineRenderer->GetFov();
	}

	unsigned char **GetAreaBits( void )
	{
		return GetBaseLocalClient().GetAreaBits_BackwardCompatibility();
	}

	virtual void SetAreaState( 
			unsigned char chAreaBits[MAX_AREA_STATE_BYTES],
			unsigned char chAreaPortalBits[MAX_AREA_PORTAL_STATE_BYTES] )
	{
		*GetBaseLocalClient().GetAreaBits_BackwardCompatibility() = 0; // Clear the b/w compatibiltiy thing.
		memcpy( GetBaseLocalClient().m_chAreaBits, chAreaBits, MAX_AREA_STATE_BYTES );
		memcpy( GetBaseLocalClient().m_chAreaPortalBits, chAreaPortalBits, MAX_AREA_PORTAL_STATE_BYTES );
		GetBaseLocalClient().m_bAreaBitsValid = true;
	}

	// World fog for world rendering
	void SetFogVolumeState( int fogVolume, bool useHeightFog )
	{
		R_SetFogVolumeState(fogVolume, useHeightFog );
	}

	virtual void InstallBrushSurfaceRenderer( IBrushRenderer* pBrushRenderer )
	{
		R_InstallBrushRenderOverride( pBrushRenderer );
	}

	struct BoxIntersectWaterContext_t
	{
		bool m_bFoundWaterLeaf;
		int m_nLeafWaterDataID;
	};
	
	bool EnumerateLeaf( int leaf, intp context )
	{
		BoxIntersectWaterContext_t *pSearchContext = ( BoxIntersectWaterContext_t * )context;
		mleaf_t *pLeaf = &host_state.worldmodel->brush.pShared->leafs[leaf];
		if( pLeaf->leafWaterDataID == pSearchContext->m_nLeafWaterDataID )
		{
			pSearchContext->m_bFoundWaterLeaf = true;
			// found it . . stop enumeration
			return false;
		}
		return true;
	}

	bool DoesBoxIntersectWaterVolume( const Vector &mins, const Vector &maxs, int leafWaterDataID )
	{
		BoxIntersectWaterContext_t context;
		context.m_bFoundWaterLeaf = false;
		context.m_nLeafWaterDataID = leafWaterDataID;
		g_pToolBSPTree->EnumerateLeavesInBox( mins, maxs, this, ( intp )&context );
		return context.m_bFoundWaterLeaf;
	}

	// Push, pop views
	virtual void Push3DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes )
	{
		g_EngineRenderer->Push3DView( pRenderContext, view, nFlags, pRenderTarget, frustumPlanes, NULL );
	}

	virtual void Push2DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes )
	{
		g_EngineRenderer->Push2DView( pRenderContext, view, nFlags, pRenderTarget, frustumPlanes );
	}

	virtual void PopView( IMatRenderContext *pRenderContext, Frustum frustumPlanes )
 	{
		g_EngineRenderer->PopView( pRenderContext, frustumPlanes );
	}

	virtual void SetMainView( const Vector &vecOrigin, const QAngle &angles )
	{
		g_EngineRenderer->SetMainView( vecOrigin, angles );
	}

	void OverrideViewFrustum( Frustum custom )
	{
		g_EngineRenderer->OverrideViewFrustum( custom );
	}

	void DrawBrushModelShadowDepth( 
		IClientEntity *baseentity, 
		model_t *model, 
		const Vector& origin, 
		const QAngle& angles, 
		ERenderDepthMode_t DepthMode )
	{
		R_DrawBrushModel( baseentity, model, origin, angles, DEPTH_MODE_SHADOW, true, true );
	}

	void UpdateBrushModelLightmap( model_t *model, IClientRenderable *pRenderable )
	{
		g_EngineRenderer->UpdateBrushModelLightmap( model, pRenderable );
	}
	
	void BeginUpdateLightmaps( void )
	{
		g_EngineRenderer->BeginUpdateLightmaps();
	}

	void EndUpdateLightmaps()
	{
		g_EngineRenderer->EndUpdateLightmaps();
	}

	virtual void Push3DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes, ITexture* pDepthTexture )
	{
		g_EngineRenderer->Push3DView( pRenderContext, view, nFlags, pRenderTarget, frustumPlanes, pDepthTexture );
	}

	void GetMatricesForView( const CViewSetup &view, VMatrix *pWorldToView, VMatrix *pViewToProjection, VMatrix *pWorldToProjection, VMatrix *pWorldToPixels )
	{
		view.ComputeViewMatrices( pWorldToView, pViewToProjection, pWorldToProjection );
		ComputeWorldToScreenMatrix( pWorldToPixels, *pWorldToProjection, view );
	}

	virtual bool DoesBrushModelNeedPowerOf2Framebuffer( const model_t *model )
	{
		return !!( model->flags & MODELFLAG_FRAMEBUFFER_TEXTURE );
	}
};

static CVRenderView s_RenderView;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CVRenderView, IVRenderView, VENGINE_RENDERVIEW_INTERFACE_VERSION, s_RenderView );



