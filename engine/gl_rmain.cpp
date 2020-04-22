//===== Copyright (c) Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//==================================================================//


#include "render_pch.h"
#include "client.h"
#include "sound.h"
#include "debug_leafvis.h"
#include "cdll_int.h"
#include "enginestats.h"
#include "ivrenderview.h"
#include "studio.h"
#include "l_studio.h"
#include "r_areaportal.h"
#include "materialsystem/materialsystem_config.h"
#include "materialsystem/itexture.h"
#include "cdll_engine_int.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "istudiorender.h"
#include "staticpropmgr.h"
#include "tier0/vprof.h"
#include "IOcclusionSystem.h"
#include "con_nprint.h"
#include "debugoverlay.h"
#include "demo.h"
#include "ivideomode.h"
#include "sys_dll.h"
#include "collisionutils.h"
#include "tier1/utlstack.h"
#include "r_decal.h"
#include "cl_main.h"
#include "paint.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef _X360
extern ConVar r_waterforceexpensive;
#endif

ConVar r_aspectratio( "r_aspectratio", "0" );
ConVar r_dynamiclighting( "r_dynamiclighting", "1", FCVAR_CHEAT );
extern ConVar building_cubemaps;
extern float scr_demo_override_fov;	

extern colorVec R_LightPoint (Vector& p);

CEngineStats g_EngineStats;

//-----------------------------------------------------------------------------
// view origin
//-----------------------------------------------------------------------------
extern Vector g_CurrentViewOrigin, g_CurrentViewForward, g_CurrentViewRight, g_CurrentViewUp;
extern Vector g_MainViewOrigin[ MAX_SPLITSCREEN_CLIENTS ];
extern Vector g_MainViewForward[ MAX_SPLITSCREEN_CLIENTS ];
extern Vector g_MainViewRight[ MAX_SPLITSCREEN_CLIENTS ];
extern Vector g_MainViewUp[ MAX_SPLITSCREEN_CLIENTS ];
bool g_bCanAccessCurrentView = false;

int	d_lightstyleframe[256];
CUtlVector<LightmapUpdateInfo_t> g_LightmapUpdateList;
CUtlVector<LightmapTransformInfo_t> g_LightmapTransformList;

void ProjectPointOnPlane( Vector& dst, const Vector& p, const Vector& normal )
{
	float d;
	Vector n;
	float inv_denom;

	inv_denom = 1.0F / DotProduct( normal, normal );

	d = DotProduct( normal, p ) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	dst[0] = p[0] - d * n[0];
	dst[1] = p[1] - d * n[1];
	dst[2] = p[2] - d * n[2];
}

/*
** assumes "src" is normalized
*/
void PerpendicularVector( Vector& dst, const Vector& src )
{
	int	pos;
	int i;
	float minelem = 1.0F;
	Vector tempvec;

	/*
	** find the smallest magnitude axially aligned vector
	*/
	for ( pos = 0, i = 0; i < 3; i++ )
	{
		if ( fabs( src[i] ) < minelem )
		{
			pos = i;
			minelem = fabs( src[i] );
		}
	}
	tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	/*
	** project the point onto the plane defined by src
	*/
	ProjectPointOnPlane( dst, tempvec, src );

	/*
	** normalize the result
	*/
	VectorNormalize( dst );
}


//-----------------------------------------------------------------------------
// Returns the PHYSICAL aspect ratio of the screen (not the pixel aspect ratio)
//-----------------------------------------------------------------------------
float GetScreenAspect( int viewportWidth, int viewportHeight )
{
	// use the override if set
	if ( r_aspectratio.GetFloat() > 0.0f )
		return r_aspectratio.GetFloat();

	const AspectRatioInfo_t &aspectRatioInfo = materials->GetAspectRatioInfo();

	// just use the viewport size, but we have to convert from pixels to real-world "size".
	return ( viewportHeight != 0 ) ? ( aspectRatioInfo.m_flFrameBuffertoPhysicalScalar * ( ( float )viewportWidth / ( float )viewportHeight ) ) : 1.0f;
}


/*
====================
CalcFov
====================
*/
void R_DrawScreenRect( float left, float top, float right, float bottom )
{
	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	
	
	IMaterial *pMaterial = materials->FindMaterial( "debug/debugportals", TEXTURE_GROUP_OTHER );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, pMaterial );

	CMeshBuilder builder;
	builder.Begin( pMesh, MATERIAL_LINE_LOOP, 4 );

		Vector v1( left, bottom, 0.5 );
		Vector v2( left, top, 0.5 );
		Vector v3( right, top, 0.5 );
		Vector v4( right, bottom, 0.5 );

		builder.Position3fv( v1.Base() ); 		builder.AdvanceVertex();  
		builder.Position3fv( v2.Base() ); 		builder.AdvanceVertex();  
		builder.Position3fv( v3.Base() ); 		builder.AdvanceVertex();  
		builder.Position3fv( v4.Base() ); 		builder.AdvanceVertex();  

	builder.End( false, true );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
}


void R_DrawPortals()
{
	// Draw the portals.
	if( !r_DrawPortals.GetInt() )
		return;

	IMaterial *pMaterial = materials->FindMaterial( "debug/debugportals", TEXTURE_GROUP_OTHER );
	CMatRenderContextPtr pRenderContext( materials );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, pMaterial );

	worldbrushdata_t *pBrushData = host_state.worldbrush;
	for( int i=0; i < pBrushData->m_nAreaPortals; i++ )
	{
		dareaportal_t *pAreaPortal = &pBrushData->m_pAreaPortals[i];

		if( !R_IsAreaVisible( pAreaPortal->otherarea ) )
			continue;

		CMeshBuilder builder;
		builder.Begin( pMesh, MATERIAL_LINES, pAreaPortal->m_nClipPortalVerts );

		for( int j=0; j < pAreaPortal->m_nClipPortalVerts; j++ )
		{
			unsigned short iVert;

			iVert = pAreaPortal->m_FirstClipPortalVert + j;
			builder.Position3f( VectorExpand( pBrushData->m_pClipPortalVerts[iVert] ) );
			builder.Color4f( 0, 0, 0, 1 );
			builder.AdvanceVertex();

			iVert = pAreaPortal->m_FirstClipPortalVert + (j+1) % pAreaPortal->m_nClipPortalVerts;
			builder.Position3f( VectorExpand( pBrushData->m_pClipPortalVerts[iVert] ) );
			builder.Color4f( 0, 0, 0, 1 );
			builder.AdvanceVertex();
		}

		builder.End( false, true );
	}

	// Draw the clip rectangles.
	for( int i=0; i < g_PortalRects.Count(); i++ )
	{
		CPortalRect *pRect = &g_PortalRects[i];
		R_DrawScreenRect( pRect->left, pRect->top, pRect->right, pRect->bottom );
	}
	g_PortalRects.Purge();
}


//-----------------------------------------------------------------------------
//
// Loose collection of functions related to rendering the world in a particular view
//
//-----------------------------------------------------------------------------
class CRender : public IRender
{
public:
	CRender();

	void FrameBegin( void );
	void FrameEnd( void );

	void ViewSetupVis( bool novis, int numorigins, const Vector origin[] );
	void ViewSetupVisEx( bool novis, int numorigins, const Vector origin[], unsigned int &returnFlags );

	void ViewEnd( void );

	void ViewDrawFade( byte *color, IMaterial* pMaterial, bool mapFullTextureToScreen = true );

	IWorldRenderList * CreateWorldList();
#if defined(_PS3)
	IWorldRenderList * CreateWorldList_PS3( int viewID );
	void BuildWorldLists_PS3_Epilogue( IWorldRenderList *pList, WorldListInfo_t* pInfo, bool bShadowDepth );
	int GetDrawFlags( void );
	int GetBuildViewID( void );
	bool IsSPUBuildWRJobsOn( void );
	void CacheFrustumData( Frustum_t *pFrustum, Frustum_t *pAreaFrustum, void *pRenderAreaBits, int numArea, bool bViewerInSolidSpace );
#else
	void BuildWorldLists_Epilogue( IWorldRenderList *pList, WorldListInfo_t* pInfo, bool bShadowDepth );
#endif

	void BuildWorldLists( IWorldRenderList *pList, WorldListInfo_t* pInfo, int iForceViewLeaf, const VisOverrideData_t* pVisData, bool bShadowDepth, float *pWaterReflectionHeight );
	void DrawWorldLists( IMatRenderContext *pRenderContext, IWorldRenderList *pList, unsigned long flags, float waterZAdjust );

	void DrawSceneBegin( void );
	void DrawSceneEnd( void );

	// utility functions
	void ExtractMatrices( void );
	void ExtractFrustumPlanes( Frustum frustumPlanes );
	void OrthoExtractFrustumPlanes( Frustum frustumPlanes );
	void OverrideViewFrustum( Frustum custom );

	void SetViewport( int x, int y, int w, int h );
	// UNDONE: these are temporary functions that will end up on the other
	// side of this interface
	const Vector &ViewOrigin( ) { return CurrentView().origin; }
	const QAngle &ViewAngles( ) { return CurrentView().angles; }
	const CViewSetup &ViewGetCurrent( void ) { return CurrentView(); }
	const VMatrix &ViewMatrix( void );
	const VMatrix &WorldToScreenMatrix( void );

	float	GetFramerate( void ) { return m_framerate; }
	virtual float	GetZNear( void ) { return m_zNear; }
	virtual float	GetZFar( void ) { return m_zFar; }

	// Query current fov and view model fov
	float	GetFov( void ) { return CurrentView().fov; };
	float	GetFovY( void ) { return m_yFOV; };
	float	GetFovViewmodel( void ) { return CurrentView().fovViewmodel; };

	virtual bool	ClipTransform( const Vector& point, Vector* pClip );
	virtual bool	ScreenTransform( const Vector& point, Vector* pScreen );

	virtual void Push3DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes );
	virtual void Push3DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes, ITexture* pDepthTexture );
	virtual void Push2DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes );
	virtual void PopView( IMatRenderContext *pRenderContext, Frustum frustumPlanes );
	virtual void SetMainView( const Vector &vecOrigin, const QAngle &angles );

	virtual void UpdateBrushModelLightmap( model_t *model, IClientRenderable *Renderable );
	virtual void BeginUpdateLightmaps( void );
	virtual void EndUpdateLightmaps( void );
	virtual bool InLightmapUpdate( void ) const;

private:
	// Called when a particular view becomes active
	void OnViewActive( Frustum frustumPlanes );

	// Clear the view (assumes the render target has already been pushed)
	void ClearView( IMatRenderContext *pRenderContext, CViewSetup &view, int nFlags, ITexture* pRenderTarget, ITexture* pDepthTexture = NULL );

	const CViewSetup &CurrentView() const { return m_ViewStack.Top().m_View; }
	CViewSetup &CurrentView() { return m_ViewStack.Top().m_View; }

	// Stack of view info
	struct ViewStack_t
	{
		CViewSetup m_View;

		// matrices
		VMatrix	m_matrixView;
		VMatrix	m_matrixProjection;
		VMatrix	m_matrixWorldToScreen;

		bool m_bIs2DView;
		bool m_bNoDraw;
	};


	// Y field of view, calculated from X FOV and screen aspect ratio.
	float			m_yFOV;

	// timing
	double		m_frameStartTime;
	float		m_framerate;

	float		m_zNear;
	float		m_zFar;
	
	// matrices
	VMatrix		m_matrixView;
	VMatrix		m_matrixProjection;
	VMatrix		m_matrixWorldToScreen;

	CUtlStack< ViewStack_t > m_ViewStack;
	int m_iLightmapUpdateDepth;
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CRender gRender;
IRender *g_EngineRenderer = &gRender;


//-----------------------------------------------------------------------------
// Called when the engine is about to begin rendering for any reason
//-----------------------------------------------------------------------------
CRender::CRender()
{
	// Make sure the stack isn't empty
	int i = m_ViewStack.Push();
	memset( &m_ViewStack[i], 0, sizeof( CViewSetup ) );
	m_ViewStack[i].m_bIs2DView = true;
	m_iLightmapUpdateDepth = 0;
}

	
//-----------------------------------------------------------------------------
// Called when the engine is about to begin rendering for any reason
//-----------------------------------------------------------------------------
void CRender::FrameBegin( void )
{
	if ( host_state.worldmodel )
	{
		// This has to be before R_AnimateLight because it uses it to
		// set the frame number of changed lightstyles

		// FIXME: Why isn't this being done in DrawSceneBegin 
		// or some other client-side simulation of state?
		r_framecount++;
		if ( g_RendererInLevel )
		{
			R_AnimateLight ();
			R_PushDlights();
		}

		if (!r_norefresh.GetInt())
		{
			m_frameStartTime = Sys_FloatTime ();
		}

		g_LightmapTransformList.RemoveAll();
		int index = g_LightmapTransformList.AddToTail();
		g_LightmapTransformList[index].pModel = host_state.worldmodel;
		SetIdentityMatrix( g_LightmapTransformList[index].xform );
	}

	UpdateStudioRenderConfig();
	g_pStudioRender->BeginFrame();
}

#ifndef _CERT

static void PrintRenderedFaceInfoCallback( int nTopN, IStudioRender::FacesRenderedInfo_t *pFaces, int nTotalFaces )
{
	if ( nTopN > 0 )
	{
		con_nprint_s printdesc;
		printdesc.time_to_live = -1;
		printdesc.color[0] = printdesc.color[1] = printdesc.color[2] = 1.0f;
		printdesc.fixed_width_font = false;

		printdesc.index = 1;
		Con_NXPrintf( &printdesc, "%d total faces in scene", nTotalFaces );
		++ printdesc.index;
		Con_NXPrintf( &printdesc, "Printing %d top offending models", nTopN );
		++ printdesc.index;
		Con_NXPrintf( &printdesc, "%50s%15s%15s%15s", "Model Name", "# Renders", "Total Tris", "Avg Tris" );
		for ( int i = 0; i < nTopN; ++ i )
		{
			++ printdesc.index;
			Con_NXPrintf( &printdesc, "%50s%15d%15d%15d", pFaces[i].pStudioHdr->name, pFaces[i].nRenderCount, pFaces[i].nFaceCount, ( int )( ( float )pFaces[i].nFaceCount / ( float )pFaces[i].nRenderCount ) );
		}
	}
}

#endif // !_CERT

//-----------------------------------------------------------------------------
// Called when the engine has finished rendering
//-----------------------------------------------------------------------------
void CRender::FrameEnd( void )
{
	// A debugging overlay that renders all raycasts.
	// Why, or why is this being done here instead of 
	// where all the other debug overlays are being done in the client DLL?
	EngineTraceRenderRayCasts();

	m_framerate = GetBaseLocalClient().GetFrameTime();
	if ( m_framerate > 0 )
	{
		m_framerate = 1 / m_framerate;
	}

	g_pStudioRender->EndFrame();

#ifndef _CERT
	g_pStudioRender->GatherRenderedFaceInfo( PrintRenderedFaceInfoCallback );
#endif // !_CERT

	g_LightmapTransformList.RemoveAll();
}


const VMatrix &CRender::ViewMatrix( ) 
{ 
	// If we aren't in a valid view, then use the last value cached off into the global variable instead
	if ( m_ViewStack.Count() > 1 )
	{
		return m_ViewStack.Top().m_matrixView;
	}
	return m_matrixView; 
}

const VMatrix &CRender::WorldToScreenMatrix( void ) 
{ 
	// If we aren't in a valid view, then use the last value cached off into the global variable instead
	if ( m_ViewStack.Count() > 1 )
	{
		return m_ViewStack.Top().m_matrixWorldToScreen; 
	}
	return m_matrixWorldToScreen;
}

void CRender::ViewSetupVis( bool novis, int numorigins, const Vector origin[] )
{
	unsigned int returnFlags = 0;
	ViewSetupVisEx( novis, numorigins, origin, returnFlags );
}

void CRender::ViewSetupVisEx( bool novis, int numorigins, const Vector origin[], unsigned int &returnFlags )
{
	Map_VisSetup( host_state.worldmodel, numorigins, origin, novis, returnFlags );
}

//-----------------------------------------------------------------------------
// Called when a particular view becomes active
//-----------------------------------------------------------------------------
void CRender::OnViewActive( Frustum frustumPlanes )
{
	const CViewSetup &view = CurrentView();

	m_yFOV = CalcFovY( view.fov, view.m_flAspectRatio );

	// build the transformation matrix for the given view angles
	VectorCopy( view.origin, g_CurrentViewOrigin );
	AngleVectors( view.angles, &g_CurrentViewForward, &g_CurrentViewRight, &g_CurrentViewUp );
//	g_CurrentViewUp = -g_CurrentViewUp;
	g_bCanAccessCurrentView = true;

	if ( frustumPlanes )
	{
		if ( view.m_bOrtho )
		{
			OrthoExtractFrustumPlanes( frustumPlanes );
		}
		else
		{
			ExtractFrustumPlanes( frustumPlanes );
		}

		OcclusionSystem()->SetView( view.origin, view.fov, m_matrixView, m_matrixProjection, frustumPlanes[ FRUSTUM_NEARZ ] );
	}

	if ( !m_ViewStack.Top().m_bNoDraw )
	{
		R_SceneBegin( );
	}

	// debug, build leaf volume
	// NOTE: This is pretty hacky, but I want the leaf based on the main view.  The skybox view is reseting
	// the g_LeafVis here because it is global.  This need to be resolved more correctly some other way!
	if ( VectorCompare( MainViewOrigin(), view.origin ) )
	{
		LeafVisBuild( view.origin );
	}
}


//-----------------------------------------------------------------------------
// Clear the view (assumes the render target has already been pushed)
//-----------------------------------------------------------------------------
void CRender::ClearView( IMatRenderContext *pRenderContext, CViewSetup &view, int nFlags, ITexture* pRenderTarget, ITexture* pDepthTexture /* = NULL */ )
{
	bool bClearColor = (nFlags & VIEW_CLEAR_COLOR) != 0;
	bool bClearDepth = (nFlags & VIEW_CLEAR_DEPTH) != 0;
	bool bClearStencil = (nFlags & VIEW_CLEAR_STENCIL) != 0;
	bool bForceClearWholeRenderTarget = (nFlags & VIEW_CLEAR_FULL_TARGET) != 0;
	bool bObeyStencil = (nFlags & VIEW_CLEAR_OBEY_STENCIL) != 0;

	// Handle an initial clear request if asked for
	if ( !bClearColor && !bClearDepth && !bClearStencil )
		return;

	if ( !bForceClearWholeRenderTarget )
	{
		if( bObeyStencil )
		{
			pRenderContext->ClearBuffersObeyStencil( bClearColor, bClearDepth );
		}
		else
		{
			pRenderContext->ClearBuffers( bClearColor, bClearDepth, bClearStencil );
		}
	}
	else
	{
		// Get the render target dimensions
		int nWidth, nHeight;
		if ( pRenderTarget )
		{
			nWidth = pRenderTarget->GetActualWidth();
			nHeight = pRenderTarget->GetActualHeight();
		}
		else
		{
			materials->GetBackBufferDimensions( nWidth, nHeight );
		}

		pRenderContext->PushRenderTargetAndViewport( pRenderTarget, pDepthTexture, 0, 0, nWidth, nHeight );

		if( bObeyStencil )
		{
			pRenderContext->ClearBuffersObeyStencil( bClearColor, bClearDepth );
		}
		else
		{
			pRenderContext->ClearBuffers( bClearColor, bClearDepth, bClearStencil );
		}

		pRenderContext->PopRenderTargetAndViewport( );
	}
}


//-----------------------------------------------------------------------------
// Push, pop views
//-----------------------------------------------------------------------------
void CRender::Push3DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes )
{
	Push3DView( pRenderContext, view, nFlags, pRenderTarget, frustumPlanes, NULL );
}

// Flip y, screen y goes down
static VMatrix g_ProjectionToOffset( 0.5f,  0.0f, 0.0f, 0.5f,  
									 0.0f, -0.5f, 0.0f, 0.5f,
									 0.0f,  0.0f, 1.0f, 0.0f,
									 0.0f,  0.0f, 0.0f, 1.0f );

// NOTE: Screen coordinates go from 0->w, 0->h
void ComputeWorldToScreenMatrix( VMatrix *pWorldToScreen, const VMatrix &worldToProjection, const CViewSetup &viewSetup )
{
	// First need to transform -1 -> 1 to 0 -> 1 in x and y
	// Then transform from 0->1 to x->w+x in x, and 0->1 to y->y+h in y.
	VMatrix offsetToPixels( viewSetup.width, 0.0f, 0.0f, viewSetup.x,
							0.0f, viewSetup.height, 0.0f, viewSetup.y,
							0.0f, 0.0f, 1.0f, 0.0f,
							0.0f, 0.0f, 0.0f, 1.0f );

	VMatrix projectionToPixels;
	MatrixMultiply( offsetToPixels, g_ProjectionToOffset, projectionToPixels );
	MatrixMultiply( projectionToPixels, worldToProjection, *pWorldToScreen );
}


//-----------------------------------------------------------------------------
// Push, pop views
//-----------------------------------------------------------------------------
void CRender::Push3DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes, ITexture* pDepthTexture )
{
	Assert( !IsX360() || (pDepthTexture == NULL) ); //Don't render to a depth texture on the 360. Instead, render using a normal depth buffer and use IDirect3DDevice9::Resolve()

	int i = m_ViewStack.Push( );
	m_ViewStack[i].m_View = view;
	m_ViewStack[i].m_bIs2DView = false;
	m_ViewStack[i].m_bNoDraw = ( ( nFlags & VIEW_NO_DRAW ) != 0 );

	CViewSetup &topView = m_ViewStack[i].m_View;

	// Compute aspect ratio if asked for
	if ( topView.m_flAspectRatio == 0.0f )
	{
		topView.m_flAspectRatio = (topView.height != 0) ? ( (float)topView.width / (float)topView.height ) : 1.0f;
	}

	ViewStack_t &viewStack = m_ViewStack.Top();
	topView.m_flAspectRatio = topView.ComputeViewMatrices( &viewStack.m_matrixView, 
		&viewStack.m_matrixProjection, &viewStack.m_matrixWorldToScreen );

	m_zNear = topView.zNear;
	m_zFar = topView.zFar;	// cache this for queries

	ExtractMatrices();

	if ( !m_ViewStack[i].m_bNoDraw )
	{
		if ( !pRenderTarget )
		{
			pRenderTarget = pRenderContext->GetRenderTarget();
		}

		// Push render target and viewport 
		pRenderContext->PushRenderTargetAndViewport( pRenderTarget, pDepthTexture, topView.x, topView.y, topView.width, topView.height );

		// Handle an initial clear request if asked for
		ClearView( pRenderContext, topView, nFlags, pRenderTarget, pDepthTexture );

		pRenderContext->DepthRange( 0, 1 );

		pRenderContext->MatrixMode( MATERIAL_PROJECTION );
		pRenderContext->PushMatrix();
		pRenderContext->LoadMatrix( m_matrixProjection );

		pRenderContext->MatrixMode( MATERIAL_VIEW );
		pRenderContext->PushMatrix();
		pRenderContext->LoadMatrix( m_matrixView );

		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PushMatrix();

		OnViewActive( frustumPlanes );
	}
}

void CRender::Push2DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes )
{
	int i = m_ViewStack.Push( );
	m_ViewStack[i].m_View = view;
	m_ViewStack[i].m_bIs2DView = true;
	m_ViewStack[i].m_bNoDraw = ( ( nFlags & VIEW_NO_DRAW ) != 0 );
	m_ViewStack[i].m_matrixView = m_matrixView;
	m_ViewStack[i].m_matrixProjection = m_matrixProjection;
	m_ViewStack[i].m_matrixWorldToScreen = m_matrixWorldToScreen;

	CViewSetup &topView = m_ViewStack[i].m_View;
	g_bCanAccessCurrentView = false;

	if ( !pRenderTarget )
	{
		pRenderTarget = pRenderContext->GetRenderTarget();
	}

	// Push render target and viewport 
	pRenderContext->PushRenderTargetAndViewport( pRenderTarget, topView.x, topView.y, topView.width, topView.height );

	// Handle an initial clear request if asked for
	ClearView( pRenderContext, topView, nFlags, pRenderTarget );

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	pRenderContext->Scale( 1, -1, 1 );
	pRenderContext->Ortho( 0, 0, topView.width, topView.height, -99999, 99999 );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
}

void CRender::PopView( IMatRenderContext *pRenderContext, Frustum frustumPlanes )
{
	if ( !m_ViewStack.Top().m_bNoDraw )
	{
		pRenderContext->MatrixMode( MATERIAL_PROJECTION );
		pRenderContext->PopMatrix();

		pRenderContext->MatrixMode( MATERIAL_VIEW );
		pRenderContext->PopMatrix();

		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PopMatrix();

		pRenderContext->PopRenderTargetAndViewport( );
	}

	bool bReset = ( m_ViewStack.Count() > 1 ) ? true : false;
	m_ViewStack.Pop();

	// Don't pop off the very last view
	g_bCanAccessCurrentView = false;

	if ( bReset )
	{
		if ( !m_ViewStack.Top().m_bIs2DView )
		{
			ExtractMatrices();
			
			m_zNear = m_ViewStack.Top().m_View.zNear;
			m_zFar = m_ViewStack.Top().m_View.zFar;

			OnViewActive( frustumPlanes );
		}
	}
}

	
//-----------------------------------------------------------------------------
// Sets the main 3D view (for console commands, sound, etc.)
//-----------------------------------------------------------------------------
void CRender::SetMainView( const Vector &vecOrigin, const QAngle &angles )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	VectorCopy( vecOrigin, g_MainViewOrigin[ nSlot ] );
	AngleVectors( angles, &g_MainViewForward[ nSlot ], &g_MainViewRight[ nSlot ], &g_MainViewUp[ nSlot ] );
}

void CRender::BeginUpdateLightmaps( void )
{
	if ( ++m_iLightmapUpdateDepth  == 1)
	{
		Assert( g_LightmapUpdateList.Count() == 0 );
		materials->BeginUpdateLightmaps();
	}
}

void CRender::UpdateBrushModelLightmap( model_t *model, IClientRenderable *pRenderable )
{
	AssertOnce( m_iLightmapUpdateDepth );

	if( !r_drawbrushmodels.GetBool() || !m_iLightmapUpdateDepth )
		return;

	R_MarkDlightsOnBrushModel( model, pRenderable );
	bool bLightingChanged = Mod_NeedsLightstyleUpdate( model );

	if ( (model->flags & MODELFLAG_HAS_DLIGHT) || bLightingChanged )
	{
		int transformIndex = g_LightmapTransformList.AddToTail();
		LightmapTransformInfo_t &transform = g_LightmapTransformList[transformIndex];
		transform.pModel = model;
		AngleMatrix( pRenderable->GetRenderAngles(), pRenderable->GetRenderOrigin(), transform.xform );
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( model->brush.firstmodelsurface, model->brush.pShared );
		bool bLight = false;
		for (int i=0 ; i<model->brush.nummodelsurfaces ; i++, surfID++)
		{
			if ( MSurf_Flags(surfID) & (SURFDRAW_HASDLIGHT|SURFDRAW_HASLIGHTSYTLES) )
			{
				R_CheckForLightmapUpdates( surfID, transformIndex );
				if ( MSurf_Flags(surfID) & SURFDRAW_HASDLIGHT )
				{
					bLight = true;
				}
			}
		}
		if ( !bLight )
		{
			model->flags &= ~MODELFLAG_HAS_DLIGHT; // don't need to check again unless a dlight hits us
		}
	}
	if ( bLightingChanged )
	{
		model->brush.nLightstyleLastComputedFrame = r_framecount;
	}
}

void CRender::EndUpdateLightmaps( void )
{
	Assert( m_iLightmapUpdateDepth > 0 );
	if ( --m_iLightmapUpdateDepth == 0 )
	{
		VPROF_BUDGET( "EndUpdateLightmaps", VPROF_BUDGETGROUP_DLIGHT_RENDERING );
		if ( g_LightmapUpdateList.Count() && r_dynamiclighting.GetBool() && !r_unloadlightmaps.GetBool() )
		{
			VPROF_("R_BuildLightmapUpdateList", 1, VPROF_BUDGETGROUP_DLIGHT_RENDERING, false, 0);
			R_BuildLightmapUpdateList();
		}
		{
			VPROF_("materials_EndUpdateLightmaps", 1, VPROF_BUDGETGROUP_DLIGHT_RENDERING, false, 0);
			materials->EndUpdateLightmaps();
		}
		{
			g_LightmapUpdateList.RemoveAll();
			VPROF_("lightmap_RemoveAll", 1, VPROF_BUDGETGROUP_DLIGHT_RENDERING, false, 0);
		}
	}
}
	
bool CRender::InLightmapUpdate( void ) const
{
	return ( m_iLightmapUpdateDepth != 0 );
}


//-----------------------------------------------------------------------------
// Compute the scene coordinates of a point in 3D
//-----------------------------------------------------------------------------
bool CRender::ClipTransform( const Vector& point, Vector* pClip )
{
// UNDONE: Clean this up some, handle off-screen vertices
	float w;
	const VMatrix &worldToScreen = g_EngineRenderer->WorldToScreenMatrix();

	pClip->x = worldToScreen[0][0] * point[0] + worldToScreen[0][1] * point[1] + worldToScreen[0][2] * point[2] + worldToScreen[0][3];
	pClip->y = worldToScreen[1][0] * point[0] + worldToScreen[1][1] * point[1] + worldToScreen[1][2] * point[2] + worldToScreen[1][3];
//	z		 = worldToScreen[2][0] * point[0] + worldToScreen[2][1] * point[1] + worldToScreen[2][2] * point[2] + worldToScreen[2][3];
	w		 = worldToScreen[3][0] * point[0] + worldToScreen[3][1] * point[1] + worldToScreen[3][2] * point[2] + worldToScreen[3][3];

	// Just so we have something valid here
	pClip->z = 0.0f;

	bool behind;
	if( w < 0.001f )
	{
		behind = true;
		pClip->x *= 100000;
		pClip->y *= 100000;
	}
	else
	{
		behind = false;
		float invw = 1.0f / w;
		pClip->x *= invw;
		pClip->y *= invw;
	}

	return behind;
}

//-----------------------------------------------------------------------------
// Purpose: Given a point, return the screen position in pixels
//-----------------------------------------------------------------------------
bool CRender::ScreenTransform( const Vector& point, Vector* pScreen )
{
	bool retval = ClipTransform( point, pScreen );

	pScreen->x = 0.5f * ( pScreen->x + 1.0f ) * CurrentView().width + CurrentView().x;
	pScreen->y = 0.5f * ( pScreen->y + 1.0f ) * CurrentView().height + CurrentView().y;

	return retval;
}

void CRender::ViewDrawFade( byte *color, IMaterial* pFadeMaterial, bool mapFullTextureToScreen )
{		
	if ( !color || !color[3] )
		return;

	if( !pFadeMaterial )
		return;

	const CViewSetup &view = CurrentView();

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->PushRenderTargetAndViewport();

	pRenderContext->Bind( pFadeMaterial );
	pFadeMaterial->AlphaModulate( color[3] * ( 1.0f / 255.0f ) );
	pFadeMaterial->ColorModulate( color[0] * ( 1.0f / 255.0f ),
		color[1] * ( 1.0f / 255.0f ),
		color[2] * ( 1.0f / 255.0f ) );
	
	bool bOldIgnoreZ = pFadeMaterial->GetMaterialVarFlag( MATERIAL_VAR_IGNOREZ );
	pFadeMaterial->SetMaterialVarFlag( MATERIAL_VAR_IGNOREZ, true );

	int nTexWidth, nTexHeight;
	nTexWidth = pFadeMaterial->GetMappingWidth();
	nTexHeight = pFadeMaterial->GetMappingHeight();
	float flUOffset = 0.5f / nTexWidth;
	float flVOffset = 0.5f / nTexHeight;

	int width, height;
	pRenderContext->GetRenderTargetDimensions( width, height );
	pRenderContext->Viewport( 0, 0, width, height );

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );

	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->Scale( 1, -1, 1 );
	pRenderContext->Ortho( 0, 0, width, height, -99999, 99999 );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();	

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();	

	IMesh* pMesh = pRenderContext->GetDynamicMesh();
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	// adjusted xys
	float x1=view.x-.5;
	float x2=view.x+view.width;
	float y1=view.y-.5;
	float y2=view.y+view.height;

	float u1, u2, v1, v2;

	if ( mapFullTextureToScreen )
	{
		// adjust nominal uvs to reflect adjusted xys
		u1=FLerp(flUOffset, 1-flUOffset,view.x,view.x+view.width,x1);
		u2=FLerp(flUOffset, 1-flUOffset,view.x,view.x+view.width,x2);
		v1=FLerp(flVOffset, 1-flVOffset,view.y,view.y+view.height,y1);
		v2=FLerp(flVOffset, 1-flVOffset,view.y,view.y+view.height,y2);
	}
	else
	{
		// Match up the view port window with a corresponding window in the fade texture.
		// This is mainly for split screen support.
		u1 = Lerp( x1 / (float)width, flUOffset, 1-flUOffset );
		u2 = Lerp( x2 / (float)width, flUOffset, 1-flUOffset );
		v1 = Lerp( y1 / (float)height, flVOffset, 1-flVOffset );
		v2 = Lerp( y2 / (float)height, flVOffset, 1-flVOffset );
	}

	for ( int corner=0; corner<4; corner++ )
	{
		bool left=(corner==0) || (corner==3);
		meshBuilder.Position3f( (left) ? x1 : x2, (corner & 2) ? y2 : y1, 0.0f );
		meshBuilder.TexCoord2f( 0, (left) ? u1 : u2, (corner & 2) ? v2 : v1 );
		meshBuilder.AdvanceVertex();
	}
	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
    pRenderContext->PopMatrix();
	pRenderContext->MatrixMode( MATERIAL_VIEW );
    pRenderContext->PopMatrix();
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
    pRenderContext->PopMatrix();
    
	pFadeMaterial->SetMaterialVarFlag( MATERIAL_VAR_IGNOREZ, bOldIgnoreZ );

	pRenderContext->PopRenderTargetAndViewport();
}

void CRender::ExtractFrustumPlanes( Frustum frustumPlanes )
{
	const CViewSetup &view = CurrentView();

	GeneratePerspectiveFrustum( CurrentViewOrigin(), 
		CurrentViewForward(), CurrentViewRight(), CurrentViewUp(),
		view.zNear, view.zFar, view.fov, m_yFOV, frustumPlanes );
	g_Frustum.SetPlanes(frustumPlanes);
}

void CRender::OrthoExtractFrustumPlanes( Frustum frustumPlanes )
{
	const CViewSetup &view = CurrentView();

	// Setup the near and far planes.
	float orgOffset = DotProduct(CurrentViewOrigin(), CurrentViewForward());
	frustumPlanes[FRUSTUM_FARZ].m_Normal = -CurrentViewForward();
	frustumPlanes[FRUSTUM_FARZ].m_Dist = -view.zFar - orgOffset;

	frustumPlanes[FRUSTUM_NEARZ].m_Normal = CurrentViewForward();
	frustumPlanes[FRUSTUM_NEARZ].m_Dist = view.zNear + orgOffset;

	// Left and right planes...
	orgOffset = DotProduct(CurrentViewOrigin(), CurrentViewRight());
	frustumPlanes[FRUSTUM_LEFT].m_Normal = CurrentViewRight();
	frustumPlanes[FRUSTUM_LEFT].m_Dist = view.m_OrthoLeft + orgOffset;

	frustumPlanes[FRUSTUM_RIGHT].m_Normal = -CurrentViewRight();
	frustumPlanes[FRUSTUM_RIGHT].m_Dist = -view.m_OrthoRight - orgOffset;

	// Top and buttom planes...
	orgOffset = DotProduct(CurrentViewOrigin(), CurrentViewUp());
	frustumPlanes[FRUSTUM_TOP].m_Normal = CurrentViewUp();
	frustumPlanes[FRUSTUM_TOP].m_Dist = view.m_OrthoTop + orgOffset;

	frustumPlanes[FRUSTUM_BOTTOM].m_Normal = -CurrentViewUp();
	frustumPlanes[FRUSTUM_BOTTOM].m_Dist = -view.m_OrthoBottom - orgOffset;

	g_Frustum.SetPlanes( frustumPlanes );
}

void CRender::OverrideViewFrustum( Frustum custom )
{
	g_Frustum.SetPlanes( custom );
}

void CRender::ExtractMatrices( void )
{
	m_matrixView = m_ViewStack.Top().m_matrixView;
	m_matrixProjection = m_ViewStack.Top().m_matrixProjection;
	m_matrixWorldToScreen = m_ViewStack.Top().m_matrixWorldToScreen;
}

void CRender::SetViewport( int x, int y, int w, int h )
{
	int	x2, y2;
	int windowWidth = w, windowHeight = h;

	CMatRenderContextPtr pRenderContext( materials );

	// set the viewport to be out to the size of the render target, unless explicitly told not to
	if (!CurrentView().m_bRenderToSubrectOfLargerScreen)
	{
		pRenderContext->GetRenderTargetDimensions( windowWidth, windowHeight );
	}

	x2 = (x + w);
	y2 = (windowHeight - (y + h));
	y = (windowHeight - y);

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < windowWidth)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < windowHeight)
		y++;

	w = x2 - x;
	h = y - y2;

	pRenderContext->Viewport( x, y2, w, h );
}

void DrawLightmapPage( int lightmapPageID )
{
	// assumes that we are already in ortho mode.
	int lightmapPageWidth, lightmapPageHeight;

	CMatRenderContextPtr pRenderContext( materials );

	IMesh* pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, g_materialDebugLightmap );
//	pRenderContext->Bind( g_materialWireframe );
//	IMesh* pMesh = pRenderContext->GetDynamicMesh( g_materialWireframe );

	materials->GetLightmapPageSize( lightmapPageID, &lightmapPageWidth, &lightmapPageHeight );
	pRenderContext->BindLightmapPage( lightmapPageID );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	int x = 0;
	int y = 0;

	float s = 1.0f;
	float t = 1.0f;

	// texcoord 1 is lightmaptexcoord for fixed function.
	meshBuilder.TexCoord2f( 1, 0.0f, 0.0f );
	meshBuilder.Position3f( x, y, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f( 1, s, 0.0f );
	meshBuilder.Position3f( x+lightmapPageWidth, y, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f( 1, s, t );
	meshBuilder.Position3f( x+lightmapPageWidth, y+lightmapPageHeight, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f( 1, 0.0f, t );
	meshBuilder.Position3f( x, y+lightmapPageHeight, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

//hack
extern void DebugDrawLightmapAtCrossHair();

void R_DrawLightmaps( IWorldRenderList *pList, int pageId )
{
#ifdef USE_CONVARS
	if ( pageId != -1 )
	{
		DrawLightmapPage( pageId );
		Shader_DrawLightmapPageChains( pList, pageId );
	}
#endif
}

void R_CheckForLightingConfigChanges()
{
	if ( !materials->CanDownloadTextures() )
		return;

	UpdateStudioRenderConfig();
	UpdateMaterialSystemConfig();
	if( MaterialConfigLightingChanged() || g_RebuildLightmaps )
	{
		ClearMaterialConfigLightingChanged();
		DevMsg( "Redownloading all lightmaps\n" );
		BuildGammaTable( 2.2f, 2.2f, 0.0f, OVERBRIGHT );
		R_RedownloadAllLightmaps();
		StaticPropMgr()->RecomputeStaticLighting();
	}
}


ConVar r_redownloadallpaintmaps("r_redownloadallpaintmaps", "0", FCVAR_DEVELOPMENTONLY);
void R_CheckForPaintmapChanges()
{
	if ( !g_PaintManager.m_bShouldRegister )
		return;

	if ( !materials->CanDownloadTextures() )
		return;

	if ( r_redownloadallpaintmaps.GetBool() )
	{
		R_RedownloadAllPaintmaps();
		r_redownloadallpaintmaps.SetValue(0);
		return;
	}

	{
		VPROF_BUDGET( "R_CheckForPaintmapChanges", "paint" );
		g_PaintManager.UpdatePaintmapTextures();
	}
}


void CRender::DrawSceneBegin( void )
{
	R_CheckForLightingConfigChanges();
}

void CRender::DrawSceneEnd( void )
{
	R_SceneEnd();
	LeafVisDraw();
}

IWorldRenderList * CRender::CreateWorldList()
{
	return AllocWorldRenderList();
}

#if defined(_PS3)
IWorldRenderList * CRender::CreateWorldList_PS3( int viewID )
{
	return AllocWorldRenderList_PS3( viewID );
}
#endif


// JasonM TODO: optimize in the case of shadow depth mapping (i.e. don't update lightmaps)
void CRender::BuildWorldLists( IWorldRenderList *pList, WorldListInfo_t* pInfo, int iForceViewLeaf, const VisOverrideData_t* pVisData, bool bShadowDepth, float *pWaterReflectionHeight )
{	
	VPROF_INCREMENT_COUNTER( "BuildWorldLists", 1 );

	Assert( pList );
	Assert( m_iLightmapUpdateDepth > 0 || g_LightmapUpdateList.Count() == 0 );

	if ( !bShadowDepth )
	{
		BeginUpdateLightmaps();
	}

	R_BuildWorldLists( pList, pInfo, iForceViewLeaf, pVisData, bShadowDepth, pWaterReflectionHeight );

	if ( !bShadowDepth )
	{
		EndUpdateLightmaps();
	}

	Assert( m_iLightmapUpdateDepth > 0 || g_LightmapUpdateList.Count() == 0 );
}

#if defined(_PS3)
void CRender::BuildWorldLists_PS3_Epilogue( IWorldRenderList *pList, WorldListInfo_t* pInfo, bool bShadowDepth )
{	
	Assert( pList );

	R_BuildWorldLists_PS3_Epilogue( pList, pInfo, bShadowDepth );
}
#else
void CRender::BuildWorldLists_Epilogue( IWorldRenderList *pList, WorldListInfo_t* pInfo, bool bShadowDepth )
{	
	Assert( pList );
	Assert( m_iLightmapUpdateDepth > 0 || g_LightmapUpdateList.Count() == 0 );

	if ( !bShadowDepth )
	{
		BeginUpdateLightmaps();
	}

	R_BuildWorldLists_Epilogue( pList, pInfo, bShadowDepth );

	if ( !bShadowDepth )
	{
		EndUpdateLightmaps();
	}

	Assert( m_iLightmapUpdateDepth > 0 || g_LightmapUpdateList.Count() == 0 );
}
#endif

void CRender::DrawWorldLists( IMatRenderContext *pRenderContext, IWorldRenderList *pList, unsigned long flags, float flWaterZAdjust )
{
	Assert( ( flags & DRAWWORLDLISTS_DRAW_SIMPLE_WORLD_MODEL ) || pList );
	R_DrawWorldLists( pRenderContext, pList, flags, flWaterZAdjust );
}

