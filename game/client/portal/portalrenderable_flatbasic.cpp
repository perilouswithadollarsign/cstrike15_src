//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "portalrenderable_flatbasic.h"
#include "precache_register.h"
#include "Portal_DynamicMeshRenderingUtils.h"
#include "portal_shareddefs.h"
#include "view.h"
#include "c_pixel_visibility.h"
#include "glow_overlay.h"
#include "portal_render_targets.h"
#include "materialsystem/ITexture.h"
#include "toolframework/itoolframework.h"
#include "toolframework_client.h"
#include "tier1/keyvalues.h"
#include "view_scene.h"
#include "tier0/vprof.h"
#include "materialsystem/imaterialvar.h"


extern ConVar r_portal_fastpath;

#define PORTALRENDERABLE_FLATBASIC_MINPIXELVIS 0.0f

CUtlStack<Vector4D> CPortalRenderable_FlatBasic::ms_clipPlaneStack;

CPortalRenderable_FlatBasic::CPortalRenderable_FlatBasic( void ) 
: m_pLinkedPortal( NULL ),
	m_ptOrigin( 0.0f, 0.0f, 0.0f ),
	m_vForward( 1.0f, 0.0f, 0.0f ),
	m_vUp( 0.0f, 0.0f, 1.0f ),
	m_vRight( 0.0f, 1.0f, 0.0f ),
	m_bIsPortal2( false )
{
	m_InternallyMaintainedData.m_VisData.m_fDistToAreaPortalTolerance = 64.0f;
	m_InternallyMaintainedData.m_VisData.m_vecVisOrigin = Vector(0,0,0);
	m_InternallyMaintainedData.m_VisData.m_bTrimFrustumToPortalCorners = false;
	m_InternallyMaintainedData.m_VisData.m_vPortalOrigin = m_InternallyMaintainedData.m_VisData.m_vPortalForward = vec3_origin;
	m_InternallyMaintainedData.m_VisData.m_flPortalRadius = 0.0f;
	m_InternallyMaintainedData.m_iViewLeaf = -1;

	for( int i = 0; i != ARRAYSIZE( m_InternallyMaintainedData.m_DepthDoublerTextureView ); ++i )
	{
		m_InternallyMaintainedData.m_DepthDoublerTextureView[i].Identity();
	}

	m_InternallyMaintainedData.m_bUsableDepthDoublerConfiguration = false;
	m_InternallyMaintainedData.m_nSkyboxVisibleFromCorners = SKYBOX_NOT_VISIBLE;

	m_InternallyMaintainedData.m_ptForwardOrigin.Init( 1.0f, 0.0f, 0.0f );
	m_InternallyMaintainedData.m_ptCorners[0] = 
		m_InternallyMaintainedData.m_ptCorners[1] = 
		m_InternallyMaintainedData.m_ptCorners[2] =
		m_InternallyMaintainedData.m_ptCorners[3] =
		Vector( 0.0f, 0.0f, 0.0f );
}

void CPortalRenderable_FlatBasic::PortalMoved( void )
{
	m_InternallyMaintainedData.m_ptForwardOrigin = m_ptOrigin + m_vForward;
	m_InternallyMaintainedData.m_fPlaneDist = m_vForward.Dot( m_ptOrigin );

	// Update the points on the portal which we add to PVS
	{
		Vector vScaledRight = m_vRight * m_fHalfWidth;
		Vector vScaledUp = m_vUp * m_fHalfHeight;

		m_InternallyMaintainedData.m_ptCorners[0] = (m_InternallyMaintainedData.m_ptForwardOrigin + vScaledRight) + vScaledUp;
		m_InternallyMaintainedData.m_ptCorners[1] = (m_InternallyMaintainedData.m_ptForwardOrigin - vScaledRight) + vScaledUp;
		m_InternallyMaintainedData.m_ptCorners[2] = (m_InternallyMaintainedData.m_ptForwardOrigin - vScaledRight) - vScaledUp;
		m_InternallyMaintainedData.m_ptCorners[3] = (m_InternallyMaintainedData.m_ptForwardOrigin + vScaledRight) - vScaledUp;


		m_InternallyMaintainedData.m_VisData.m_vecVisOrigin = m_InternallyMaintainedData.m_ptForwardOrigin;
		m_InternallyMaintainedData.m_VisData.m_fDistToAreaPortalTolerance = 64.0f;				
		m_InternallyMaintainedData.m_VisData.m_bTrimFrustumToPortalCorners = true;
		memcpy( m_InternallyMaintainedData.m_VisData.m_vPortalCorners, m_InternallyMaintainedData.m_ptCorners, sizeof( m_InternallyMaintainedData.m_VisData.m_vPortalCorners ) );

		m_InternallyMaintainedData.m_VisData.m_vPortalOrigin = m_ptOrigin;
		m_InternallyMaintainedData.m_VisData.m_vPortalForward = m_vForward;
		m_InternallyMaintainedData.m_VisData.m_flPortalRadius = sqrtf( m_fHalfWidth * m_fHalfWidth + m_fHalfHeight * m_fHalfHeight );

		m_InternallyMaintainedData.m_iViewLeaf = enginetrace->GetLeafContainingPoint( m_InternallyMaintainedData.m_ptForwardOrigin );
	}

	m_InternallyMaintainedData.m_nSkyboxVisibleFromCorners = engine->IsSkyboxVisibleFromPoint( m_InternallyMaintainedData.m_ptForwardOrigin );
	for( int i = 0; i < 4 && ( m_InternallyMaintainedData.m_nSkyboxVisibleFromCorners != SKYBOX_3DSKYBOX_VISIBLE ); ++i )
	{
		SkyboxVisibility_t nCornerVis = engine->IsSkyboxVisibleFromPoint( m_InternallyMaintainedData.m_ptCorners[i] );
		if ( ( m_InternallyMaintainedData.m_nSkyboxVisibleFromCorners == SKYBOX_NOT_VISIBLE ) || ( nCornerVis != SKYBOX_NOT_VISIBLE ) )
		{
			m_InternallyMaintainedData.m_nSkyboxVisibleFromCorners = nCornerVis;
		}
	}

	//render fix bounding planes
	{
		for( int i = 0; i != PORTALRENDERFIXMESH_OUTERBOUNDPLANES; ++i )
		{
			float fCirclePos = ((float)(i)) * ((M_PI * 2.0f) / (float)PORTALRENDERFIXMESH_OUTERBOUNDPLANES);
			float fUpBlend = cosf( fCirclePos );
			float fRightBlend = sinf( fCirclePos );

			Vector vNormal = -fUpBlend * m_vUp - fRightBlend * m_vRight;
			Vector ptOnPlane = m_ptOrigin + (m_vUp * (fUpBlend * m_fHalfHeight * 1.1f)) + (m_vRight * (fRightBlend * m_fHalfWidth * 1.1f));

			m_InternallyMaintainedData.m_BoundingPlanes[i].Init( vNormal, vNormal.Dot( ptOnPlane ) );
		}

		m_InternallyMaintainedData.m_BoundingPlanes[PORTALRENDERFIXMESH_OUTERBOUNDPLANES].Init( -m_vForward, (-m_vForward).Dot( m_ptOrigin ) );
		m_InternallyMaintainedData.m_BoundingPlanes[PORTALRENDERFIXMESH_OUTERBOUNDPLANES + 1].Init( m_vForward, m_vForward.Dot( m_ptOrigin - (m_vForward * 5.0f) ) );
	}

	//update depth doubler usability flag
	m_InternallyMaintainedData.m_bUsableDepthDoublerConfiguration = 
		( m_pLinkedPortal && //linked to another portal
		( m_vForward.Dot( m_pLinkedPortal->m_ptOrigin - m_ptOrigin ) > 0.0f ) && //this portal looking in the general direction of the other portal
		( m_vForward.Dot( m_pLinkedPortal->m_vForward ) < -0.7071f ) ); //within 45 degrees of facing directly at each other

	if( m_pLinkedPortal )
		m_pLinkedPortal->m_InternallyMaintainedData.m_bUsableDepthDoublerConfiguration = m_InternallyMaintainedData.m_bUsableDepthDoublerConfiguration;

#if 0
	if ( IsToolRecording() && ((m_ptOrigin != m_ptLastRecordedOrigin) || (m_qAbsAngle != m_qLastRecordedAngle)) )
	{
		static EntityTeleportedRecordingState_t state;

		KeyValues *msg = new KeyValues( "entity_teleported" );
		msg->SetPtr( "state", &state );
		state.m_bTeleported = true;
		state.m_bViewOverride = false;
		state.m_vecTo = m_ptOrigin;
		state.m_qaTo = m_qAbsAngle;
		VMatrix mat_OldPosition, mat_InvOldPosition, mat_CurPosition;
		AngleMatrix( m_qLastRecordedAngle, m_ptLastRecordedOrigin, mat_OldPosition.As3x4() );
		MatrixInverseTR( mat_OldPosition, mat_InvOldPosition );
		AngleMatrix( m_qAbsAngle, m_ptOrigin, mat_CurPosition.As3x4() );
		ConcatTransforms( mat_InvOldPosition.As3x4(), mat_CurPosition.As3x4(), state.m_teleportMatrix );
		//state.m_teleportMatrix.Init( Vector( 1.0f, 0.0f, 0.0f ),  Vector( 0.0f, 1.0f, 0.0f ),  Vector( 0.0f, 0.0f, 1.0f ), vec3_origin );
		m_ptLastRecordedOrigin = m_ptOrigin;
		m_qLastRecordedAngle = m_qAbsAngle;
		
		// Post a message back to all IToolSystems
		Assert( (int)GetToolHandle() != 0 );
		ToolFramework_PostToolMessage( GetToolHandle(), msg );

		msg->deleteThis();
	}
#endif
}




bool CPortalRenderable_FlatBasic::WillUseDepthDoublerThisDraw( void ) const
{
	return m_InternallyMaintainedData.m_bUsableDepthDoublerConfiguration && 
		(g_pPortalRender->GetRemainingPortalViewDepth() == 0) && 
		(g_pPortalRender->GetViewRecursionLevel() > 1) &&
		(g_pPortalRender->GetCurrentViewEntryPortal() == this);
}



ConVar r_portal_use_complex_frustums( "r_portal_use_complex_frustums", "1", FCVAR_CLIENTDLL, "View optimization, turn this off if you get odd visual bugs." );

bool CPortalRenderable_FlatBasic::CalcFrustumThroughPortal( const Vector &ptCurrentViewOrigin, Frustum OutputFrustum )
{
	if( r_portal_use_complex_frustums.GetBool() == false )
		return false;

	if( (g_pPortalRender->GetViewRecursionLevel() == 0) && 
		( (ptCurrentViewOrigin - m_ptOrigin).LengthSqr() < (m_fHalfHeight * m_fHalfHeight) ) )//FIXME: Player closeness check might need reimplementation
	{
		//calculations are most likely going to be completely useless, return nothing
		return false;
	}

	if( m_pLinkedPortal == NULL )
		return false;

	if( m_vForward.Dot( ptCurrentViewOrigin ) <= m_InternallyMaintainedData.m_fPlaneDist )
		return false; //looking at portal backface

	return CalcFrustumThroughPolygon( m_InternallyMaintainedData.m_ptCorners, 4, ptCurrentViewOrigin, OutputFrustum );
}

ConVar cl_showcomplexfrustum( "cl_showcomplexfrustum", "0" );

bool CPortalRenderable_FlatBasic::CalcFrustumThroughPolygon( const Vector *pPolyVertices, int iPolyVertCount, const Vector &ptCurrentViewOrigin, Frustum OutputFrustum )
{
	int iViewRecursionLevel = g_pPortalRender->GetViewRecursionLevel();
	int iNextViewRecursionLevel = iViewRecursionLevel + 1;
	Vector vTransformedViewOrigin = m_matrixThisToLinked * ptCurrentViewOrigin;

	VPlane *pInputFrustumPlanes = g_pPortalRender->GetRecursiveViewComplexFrustums( iViewRecursionLevel ).Base();
	int iInputFrustumPlanes = g_pPortalRender->GetRecursiveViewComplexFrustums( iViewRecursionLevel ).Count();
	Assert( iInputFrustumPlanes > 0 );

	Vector *pClippedVerts;
	int iClippedVertCount;
	{
		//clip the polygon by the input frustum
		int iAllocSize = iPolyVertCount + iInputFrustumPlanes;

		Vector *pWorkVerts[2];
		pWorkVerts[0] = (Vector *)stackalloc( sizeof( Vector ) * iAllocSize * 2 ); //possible to add 1 point per cut, iPolyVertCount starting points, iInputFrustumPlaneCount cuts
		pWorkVerts[1] = pWorkVerts[0] + iAllocSize;

		//clip by first plane and put output into pInVerts
		iClippedVertCount = ClipPolyToPlane( (Vector *)pPolyVertices, iPolyVertCount, pWorkVerts[0], pInputFrustumPlanes[0].m_Normal, pInputFrustumPlanes[0].m_Dist, 0.01f );

		//clip by other planes and flipflop in and out pointers
		for( int i = 1; i != iInputFrustumPlanes; ++i )
		{
			if( iClippedVertCount < 3 )
				return false; //nothing left in the frustum

			iClippedVertCount = ClipPolyToPlane( pWorkVerts[(i & 1) ^ 1], iClippedVertCount, pWorkVerts[i & 1], pInputFrustumPlanes[i].m_Normal, pInputFrustumPlanes[i].m_Dist, 0.01f );
		}

		if( iClippedVertCount < 3 )
			return false; //nothing left in the frustum

		pClippedVerts = pWorkVerts[iInputFrustumPlanes & 1];
	}

	for( int i = 0; i != iClippedVertCount; ++i )
	{
		pClippedVerts[i] = m_matrixThisToLinked * pClippedVerts[i];
	}

	VPlane plane_NearZ, plane_FarZ;
	
	//Near Z	
	plane_NearZ.Init( m_pLinkedPortal->m_vForward, m_pLinkedPortal->m_InternallyMaintainedData.m_fPlaneDist );

	//Far Z
	{
		Vector vNormal = m_matrixThisToLinked.ApplyRotation( pInputFrustumPlanes[iInputFrustumPlanes - 1].m_Normal );
		Vector ptOnPlane = pInputFrustumPlanes[iInputFrustumPlanes - 1].m_Dist * pInputFrustumPlanes[iInputFrustumPlanes - 1].m_Normal;
		plane_FarZ.Init( vNormal, vNormal.Dot( m_matrixThisToLinked * ptOnPlane ) );
	}


	VPlane *pOutputFrustumPlanes = (VPlane *)stackalloc( sizeof( VPlane ) * MAX( iClippedVertCount, 4 ) );
	
	//calculate and store the complex frustum with an unbounded number of planes
	{
		int iComplexCount = UTIL_CalcFrustumThroughConvexPolygon( pClippedVerts, iClippedVertCount, vTransformedViewOrigin, NULL, 0, pOutputFrustumPlanes, iClippedVertCount, 0 );
		if( iComplexCount == 0 )
			return false;

		g_pPortalRender->GetRecursiveViewComplexFrustums( iNextViewRecursionLevel ).SetCount( iComplexCount + 2 ); //+2 for near and far z planes
		VPlane *pWritePlane = g_pPortalRender->GetRecursiveViewComplexFrustums( iNextViewRecursionLevel ).Base();
		/*for( int i = 0; i != iComplexCount; ++i )
		{
			Vector vPoint = m_matrixThisToLinked * (pOutputFrustumPlanes[i].m_Normal * pOutputFrustumPlanes[i].m_Dist);
			Vector vNormal = m_matrixThisToLinked.ApplyRotation( pOutputFrustumPlanes[i].m_Normal );
			pWritePlane->Init( vNormal, vNormal.Dot( vPoint ) );
			++pWritePlane;
		}*/
		memcpy( pWritePlane, pOutputFrustumPlanes, sizeof( VPlane ) * iComplexCount );
		pWritePlane += iComplexCount;
		*pWritePlane = plane_NearZ;
		++pWritePlane;
		*pWritePlane = plane_FarZ;
	}


	//calculate and store the simple frustum, limited to exactly 6 planes
	{
		int iSimpleCount = UTIL_CalcFrustumThroughConvexPolygon( pClippedVerts, iClippedVertCount, vTransformedViewOrigin, NULL, 0, pOutputFrustumPlanes, 4, 0 );

		VPlane *pWritePlane = &OutputFrustum[0];
		/*for( int i = 0; i != iSimpleCount; ++i )
		{
			Vector vPoint = m_matrixThisToLinked * (pOutputFrustumPlanes[i].m_Normal * pOutputFrustumPlanes[i].m_Dist);
			Vector vNormal = m_matrixThisToLinked.ApplyRotation( pOutputFrustumPlanes[i].m_Normal );
			pWritePlane->Init( vNormal, vNormal.Dot( vPoint ) );
			++pWritePlane;
		}*/
		memcpy( pWritePlane, pOutputFrustumPlanes, sizeof( VPlane ) * iSimpleCount );
		pWritePlane += iSimpleCount;

		while( iSimpleCount < 4 )
		{
			//crap, just copy the near plane into would-be empty slots
			*pWritePlane = plane_NearZ;
			++pWritePlane;

			++iSimpleCount;
		}

		*pWritePlane = plane_NearZ;
		++pWritePlane;
		*pWritePlane = plane_FarZ;
	}

	return true;
}

static void DrawFrustum( Frustum_t &frustum )
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
			debugoverlay->AddLineOverlay( in[c], in[(c+1)%numPoints], 0, 255, 0, false, -1 );
		}
	}
}

ConVar r_drawportalfrustum( "r_drawportalfrustum", "0" );
ConVar r_lockportalfrustum( "r_lockportalfrustum", "0" );

void CPortalRenderable_FlatBasic::RenderPortalViewToBackBuffer( CViewRender *pViewRender, const CViewSetup &cameraView )
{
	VPROF( "CPortalRenderable_FlatBasic::RenderPortalViewToBackBuffer" );

	if( m_pLinkedPortal == NULL ) //not linked to any portal, so we'll be all static anyways
		return;

	VPROF_INCREMENT_COUNTER( "PortalRenders", 1 );

	Frustum FrustumBackup;
	memcpy( FrustumBackup, pViewRender->GetFrustum(), sizeof( Frustum ) );

	Frustum seeThroughFrustum;
	bool bUseSeeThroughFrustum;

	bUseSeeThroughFrustum = CalcFrustumThroughPortal( cameraView.origin, seeThroughFrustum );

	if ( r_drawportalfrustum.GetBool() )
	{
		static Frustum_t tmpFrustum;
		if ( !r_lockportalfrustum.GetBool() )
		{
			tmpFrustum.SetPlanes( seeThroughFrustum );
		}
		if ( bUseSeeThroughFrustum )
		{
			DrawFrustum( tmpFrustum );
		}
	}

	Vector vCameraForward;
	AngleVectors( cameraView.angles, &vCameraForward, NULL, NULL );

	// Setup fog state for the camera.
	Vector ptPOVOrigin = m_matrixThisToLinked * cameraView.origin;	
	Vector vPOVForward = m_matrixThisToLinked.ApplyRotation( vCameraForward );

	Vector ptRemotePortalPosition = m_pLinkedPortal->m_ptOrigin;
	Vector vRemotePortalForward = m_pLinkedPortal->m_vForward;

	CViewSetup portalView = cameraView;

	if( portalView.zNear < 1.0f )
		portalView.zNear = 1.0f;

	QAngle qPOVAngles = TransformAnglesToWorldSpace( cameraView.angles, m_matrixThisToLinked.As3x4() );	
	
	portalView.origin = ptPOVOrigin;
	portalView.angles = qPOVAngles;

	VMatrix matCurrentView;
	if( cameraView.m_bCustomViewMatrix )
	{
		matCurrentView.CopyFrom3x4( cameraView.m_matCustomViewMatrix );
	}
	else
	{
		matCurrentView.Identity();

		//generate the view matrix for the existing position and angle, then wedge our portal matrix onto it as a world transformation that prepends the view
		MatrixRotate( matCurrentView, Vector( 1, 0, 0 ), -cameraView.angles[2] );
		MatrixRotate( matCurrentView, Vector( 0, 1, 0 ), -cameraView.angles[0] );
		MatrixRotate( matCurrentView, Vector( 0, 0, 1 ), -cameraView.angles[1] );
		MatrixTranslate( matCurrentView, -cameraView.origin );
	}

	VMatrix matTemp = matCurrentView * m_pLinkedPortal->m_matrixThisToLinked;
	portalView.m_matCustomViewMatrix = matTemp.As3x4();
	portalView.m_bCustomViewMatrix = true;

	CopyToCurrentView( pViewRender, portalView );

	CMatRenderContextPtr pRenderContext( materials );

	//if we look through multiple unique pairs of portals, we have to take care not to clip too much
	bool bReplaceOldPortalClipPlane = (g_pPortalRender->GetViewRecursionLevel() != 0) && (dynamic_cast<CPortalRenderable_FlatBasic *>(g_pPortalRender->GetCurrentViewExitPortal()) != NULL);
	Vector4D vClipPlaneToRestore( 0.0f, 0.0f, 0.0f, 0.0f );

	Assert( ( g_pPortalRender->GetViewRecursionLevel() == 0 ) ? ( ms_clipPlaneStack.Count() == 0 ) : true );

	{
		Vector4D vCustomClipPlane;
		vCustomClipPlane.x = vRemotePortalForward.x;
		vCustomClipPlane.y = vRemotePortalForward.y;
		vCustomClipPlane.z = vRemotePortalForward.z;
		vCustomClipPlane.w = vRemotePortalForward.Dot( ptRemotePortalPosition ) - 2.0f; //moving it back a smidge to eliminate visual artifacts for half-in objects

		if ( g_pMaterialSystemHardwareConfig->UseFastClipping() )
		{
			// We need to make sure the clip plane never goes behind the eye position, as that would result in a degenerate projection matrix.
			// Note: This clip plane is for the distant side of the portal, so the camera is supposed to stay behind this plane (flCamDist should be negative).
			float flCamDist = DotProduct( cameraView.origin, vRemotePortalForward ) - vCustomClipPlane.w;
			static const float CAMERA_DIST_EPSILON = 1.0f;
			if ( flCamDist > -CAMERA_DIST_EPSILON )
			{
				float flClipPlaneCorrectionOffset = flCamDist + CAMERA_DIST_EPSILON;
				vCustomClipPlane.w += flClipPlaneCorrectionOffset;
			}
		}

		if( bReplaceOldPortalClipPlane )
		{
			pRenderContext->PopCustomClipPlane(); //HACKHACK: We really only want to remove the clip plane of the current portal view. This assumes we're the only ones leaving clip planes on the stack
			ms_clipPlaneStack.Pop( vClipPlaneToRestore );
		}

		pRenderContext->PushCustomClipPlane( vCustomClipPlane.Base() );
		ms_clipPlaneStack.Push( Vector4D( vCustomClipPlane ) );
	}

	shadowmgr->PushFlashlightScissorBounds();


	{
		ViewCustomVisibility_t customVisibility;
		m_pLinkedPortal->AddToVisAsExitPortal( &customVisibility );
		CMatRenderContextPtr pRenderContext( materials );
		render->Push3DView( pRenderContext, portalView, 0, NULL, pViewRender->GetFrustum() );		
		{
			if( bUseSeeThroughFrustum)
				memcpy( pViewRender->GetFrustum(), seeThroughFrustum, sizeof( Frustum ) );

			render->OverrideViewFrustum( pViewRender->GetFrustum() );
			SetViewRecursionLevel( g_pPortalRender->GetViewRecursionLevel() + 1 );

			CPortalRenderable *pRenderingViewForPortalBackup = g_pPortalRender->GetCurrentViewEntryPortal();
			CPortalRenderable *pRenderingViewExitPortalBackup = g_pPortalRender->GetCurrentViewExitPortal();
			SetViewEntranceAndExitPortals( this, m_pLinkedPortal );

			//DRAW!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			{
				PIXEVENT( pRenderContext, "PortalRender" );
				ViewDrawScene_PortalStencil( pViewRender, portalView, &customVisibility );
			}

			SetViewEntranceAndExitPortals( pRenderingViewForPortalBackup, pRenderingViewExitPortalBackup );

			if( m_InternallyMaintainedData.m_bUsableDepthDoublerConfiguration && (g_pPortalRender->GetRemainingPortalViewDepth() == 1) )
			{
				//save the view matrix for usage with the depth doubler. 
				//It's important that we do this AFTER using the depth doubler this frame to compensate for the fact that the front buffer is 1 frame behind the current view matrix
				//otherwise we get a lag effect when the player changes their viewing angles
				pRenderContext->GetMatrix( MATERIAL_VIEW, &m_InternallyMaintainedData.m_DepthDoublerTextureView[GET_ACTIVE_SPLITSCREEN_SLOT()] );
			}

			SetViewRecursionLevel( g_pPortalRender->GetViewRecursionLevel() - 1 );
		}
		render->PopView( pRenderContext, pViewRender->GetFrustum() );

		//restore old frustum
		memcpy( pViewRender->GetFrustum(), FrustumBackup, sizeof( Frustum ) );
		render->OverrideViewFrustum( FrustumBackup );
	}

	shadowmgr->PopFlashlightScissorBounds();

	pRenderContext->PopCustomClipPlane();
	ms_clipPlaneStack.Pop();	// This pops my own clip plane

	if( bReplaceOldPortalClipPlane )
	{
		pRenderContext->PushCustomClipPlane( vClipPlaneToRestore.Base() );
		ms_clipPlaneStack.Push( vClipPlaneToRestore );
	}

	//restore old vis data
	CopyToCurrentView( pViewRender, cameraView );
}


void CPortalRenderable_FlatBasic::RenderPortalViewToTexture( CViewRender *pViewRender, const CViewSetup &cameraView )
{
	if( m_pLinkedPortal == NULL ) //not linked to any portal, so we'll be all static anyways
		return;

	float fPixelVisibilty = g_pPortalRender->GetPixelVisilityForPortalSurface( this );
	if( (fPixelVisibilty >= 0.0f) && (fPixelVisibilty <= PORTALRENDERABLE_FLATBASIC_MINPIXELVIS) )
		return;

	ITexture *pRenderTarget;
	if( m_bIsPortal2 )
		pRenderTarget = portalrendertargets->GetPortal2Texture();
	else
		pRenderTarget = portalrendertargets->GetPortal1Texture();

	// Require that we have render textures for drawing
	AssertMsg( pRenderTarget, "Portal render targets not initialized properly" );

	// We're about to dereference this, so just bail if we can't
	if ( !pRenderTarget )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	Vector vCameraForward;
	AngleVectors( cameraView.angles, &vCameraForward, NULL, NULL );

	Frustum seeThroughFrustum;
	bool bUseSeeThroughFrustum = CalcFrustumThroughPortal( cameraView.origin, seeThroughFrustum );

	// Setup fog state for the camera.
	Vector ptPOVOrigin = m_matrixThisToLinked * cameraView.origin;	
	Vector vPOVForward = m_matrixThisToLinked.ApplyRotation( vCameraForward );

	Vector vCameraToPortal = m_ptOrigin - cameraView.origin;

	CViewSetup portalView = cameraView;
	Frustum frustumBackup;
	memcpy( frustumBackup, pViewRender->GetFrustum(), sizeof( Frustum ) );

	QAngle qPOVAngles = TransformAnglesToWorldSpace( cameraView.angles, m_matrixThisToLinked.As3x4() );	

	portalView.width = pRenderTarget->GetActualWidth();
	portalView.height = pRenderTarget->GetActualHeight();
	portalView.x = 0;
	portalView.y = 0;
	portalView.origin = ptPOVOrigin;
	portalView.angles = qPOVAngles;
	portalView.fov = cameraView.fov;
	portalView.m_bOrtho = false;
	portalView.m_flAspectRatio = (float)cameraView.width / (float)cameraView.height; //use the screen aspect ratio, 0.0f doesn't work as advertised

	//pRenderContext->Flush( false );

	float fCustomClipPlane[4];
	fCustomClipPlane[0] = m_pLinkedPortal->m_vForward.x;
	fCustomClipPlane[1] = m_pLinkedPortal->m_vForward.y;
	fCustomClipPlane[2] = m_pLinkedPortal->m_vForward.z;
	fCustomClipPlane[3] = m_pLinkedPortal->m_vForward.Dot( m_pLinkedPortal->m_ptOrigin - (m_pLinkedPortal->m_vForward * 2.0f) ); //moving it back a smidge to eliminate visual artifacts for half-in objects

	pRenderContext->PushCustomClipPlane( fCustomClipPlane );

	{
		render->Push3DView( pRenderContext, portalView, VIEW_CLEAR_DEPTH, pRenderTarget, pViewRender->GetFrustum() );

		{
			ViewCustomVisibility_t customVisibility;
			m_pLinkedPortal->AddToVisAsExitPortal( &customVisibility );

			SetRemainingViewDepth( 0 );
			SetViewRecursionLevel( 1 );

			CPortalRenderable *pRenderingViewForPortalBackup = g_pPortalRender->GetCurrentViewEntryPortal();
			CPortalRenderable *pRenderingViewExitPortalBackup = g_pPortalRender->GetCurrentViewExitPortal();
			SetViewEntranceAndExitPortals( this, m_pLinkedPortal );

			bool bDrew3dSkybox = false;
			SkyboxVisibility_t nSkyboxVisible = SKYBOX_NOT_VISIBLE;

			// if the 3d skybox world is drawn, then don't draw the normal skybox
			int nClearFlags = 0;
			Draw3dSkyboxworld_Portal( pViewRender, portalView, nClearFlags, bDrew3dSkybox, nSkyboxVisible, pRenderTarget );

			if( bUseSeeThroughFrustum )
			{
				memcpy( pViewRender->GetFrustum(), seeThroughFrustum, sizeof( Frustum ) );
			}

			render->OverrideViewFrustum( pViewRender->GetFrustum() );

			pRenderContext->EnableUserClipTransformOverride( false );

			ViewDrawScene( pViewRender, bDrew3dSkybox, nSkyboxVisible, portalView, nClearFlags, (view_id_t)g_pPortalRender->GetCurrentViewId(), false, 0, &customVisibility );

			SetViewEntranceAndExitPortals( pRenderingViewForPortalBackup, pRenderingViewExitPortalBackup );

			SetRemainingViewDepth( 1 );
			SetViewRecursionLevel( 0 );

			memcpy( pViewRender->GetFrustum(), frustumBackup, sizeof( Frustum ) );
			render->OverrideViewFrustum( pViewRender->GetFrustum() );
		}

		render->PopView( pRenderContext, pViewRender->GetFrustum() );
	}

	pRenderContext->PopCustomClipPlane();

	//pRenderContext->Flush( false );

	CopyToCurrentView( pViewRender, cameraView );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AddToVisAsExitPortal
// input -  pViewRender: pointer to the CViewRender class used to render this scene.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CPortalRenderable_FlatBasic::AddToVisAsExitPortal( ViewCustomVisibility_t *pCustomVisibility )
{
	if ( !pCustomVisibility )
		return;

	// Add four corners of the portal to the renderer as visibility origins
	for ( int i = 0; i < 4; ++i )
	{
		if( enginetrace->GetLeafContainingPoint( m_InternallyMaintainedData.m_ptCorners[i] ) != -1 )
			pCustomVisibility->AddVisOrigin( m_InternallyMaintainedData.m_ptCorners[i] );
	}

	// Add a point forward from the center of the portal in case the corners are outside of the world
	if( enginetrace->GetLeafContainingPoint( m_InternallyMaintainedData.m_ptForwardOrigin ) != -1 )
		pCustomVisibility->AddVisOrigin( m_InternallyMaintainedData.m_ptForwardOrigin );

	// Specify which leaf to use for area portal culling
	pCustomVisibility->ForceVisOverride( m_InternallyMaintainedData.m_VisData );
	pCustomVisibility->ForceViewLeaf( m_InternallyMaintainedData.m_iViewLeaf );
}

void CPortalRenderable_FlatBasic::DrawStencilMask( IMatRenderContext *pRenderContext )
{
	DrawSimplePortalMesh( pRenderContext, g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model );
	DrawRenderFixMesh( pRenderContext, g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model );
}

void CPortalRenderable_FlatBasic::DrawPostStencilFixes( IMatRenderContext *pRenderContext )
{
	//fast clipping may have hosed depth, reset it
	if ( !r_portal_fastpath.GetBool() )
	{
		// TODO: Figure out if I actually need to do this
		pRenderContext->ClearBuffersObeyStencil( false, true );
	}

	//replace the fog we overwrote
	//RenderFogQuad(); //technically the fog quad is incorrect for all types of fog. But offers no usefulness when the local fog is <= the remote fog

	//replace depth
	DrawSimplePortalMesh( pRenderContext, g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model );
	DrawRenderFixMesh( pRenderContext, g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model );
}

void CPortalRenderable_FlatBasic::DrawPortal( IMatRenderContext *pRenderContext )
{
	//stencil-based rendering
	if( g_pPortalRender->GetCurrentViewExitPortal() != this )
	{
		if( (m_InternallyMaintainedData.m_bUsableDepthDoublerConfiguration) 
			&& (g_pPortalRender->GetRemainingPortalViewDepth() == 0) 
			&& (g_pPortalRender->GetViewRecursionLevel() > 1) )
		{
			DrawDepthDoublerMesh( pRenderContext );
		}
	}
}

int CPortalRenderable_FlatBasic::BindPortalMaterial( IMatRenderContext *pRenderContext, int nPassIndex, bool *pAllowRingMeshOptimizationOut )
{
	VPROF_BUDGET( __FUNCTION__, "FlatBasic::BindPortalMaterial" );

	*pAllowRingMeshOptimizationOut = true;
	//stencil-based rendering
	if( g_pPortalRender->GetCurrentViewExitPortal() != this )
	{
		if( (m_InternallyMaintainedData.m_bUsableDepthDoublerConfiguration) 
			&& (g_pPortalRender->GetRemainingPortalViewDepth() == 0) 
			&& (g_pPortalRender->GetViewRecursionLevel() > 1)
			&& (g_pPortalRender->GetCurrentViewEntryPortal() == this) )
		{
			if( CPortalRender::DepthDoublerPIPDisableCheck() )
				return 0;

			if ( g_pPortalRender->m_MaterialsAccess.m_PortalDepthDoubler.IsValid() )
			{
				IMaterialVar *pVar = g_pPortalRender->m_MaterialsAccess.m_PortalDepthDoubler->FindVarFast( "$alternateviewmatrix", &g_pPortalRender->m_MaterialsAccess.m_nDepthDoubleViewMatrixVarCache );
				if ( pVar != NULL )
				{
					pVar->SetMatrixValue( m_InternallyMaintainedData.m_DepthDoublerTextureView[GET_ACTIVE_SPLITSCREEN_SLOT()] );
				}
			}

			pRenderContext->Bind( g_pPortalRender->m_MaterialsAccess.m_PortalDepthDoubler, GetClientRenderable() );
			*pAllowRingMeshOptimizationOut = false;
			return 1;
		}
	}
	return 0;
}

void CPortalRenderable_FlatBasic::DrawDepthDoublerMesh( IMatRenderContext *pRenderContext, float fForwardOffsetModifier )
{
	if( CPortalRender::DepthDoublerPIPDisableCheck() )
		return;

	if ( g_pPortalRender->m_MaterialsAccess.m_PortalDepthDoubler.IsValid() )
	{
		IMaterialVar *pVar = g_pPortalRender->m_MaterialsAccess.m_PortalDepthDoubler->FindVarFast( "$alternateviewmatrix", &g_pPortalRender->m_MaterialsAccess.m_nDepthDoubleViewMatrixVarCache );
		if ( pVar != NULL )
		{
			pVar->SetMatrixValue( m_InternallyMaintainedData.m_DepthDoublerTextureView[GET_ACTIVE_SPLITSCREEN_SLOT()] );
		}
	}

	DrawSimplePortalMesh( pRenderContext, g_pPortalRender->m_MaterialsAccess.m_PortalDepthDoubler, fForwardOffsetModifier );
}

bool CPortalRenderable_FlatBasic::ShouldUpdateDepthDoublerTexture( const CViewSetup &viewSetup )
{
	return	( (m_InternallyMaintainedData.m_bUsableDepthDoublerConfiguration) && (m_pLinkedPortal != NULL) );
}


void CPortalRenderable_FlatBasic::GetToolRecordingState( KeyValues *msg )
{
	if ( !ToolsEnabled() )
		return;

	VPROF_BUDGET( "CPortalRenderable_FlatBasic::GetToolRecordingState", VPROF_BUDGETGROUP_TOOLS );

	BaseClass::GetToolRecordingState( msg );

	C_Prop_Portal *pLinkedPortal = static_cast<C_Prop_Portal*>( m_pLinkedPortal );

	static PortalRecordingState_t state;
	state.m_nPortalId = static_cast<C_Prop_Portal*>( this )->index;
	state.m_nLinkedPortalId = pLinkedPortal ? pLinkedPortal->index : -1;
	state.m_fHalfWidth = m_fHalfWidth;
	state.m_fHalfHeight = m_fHalfHeight;
	state.m_bIsPortal2 = m_bIsPortal2;
	state.m_portalType = "Flat Basic";
	msg->SetPtr( "portal", &state );

	RemoveEffects( EF_NOINTERP );
}

void CPortalRenderable_FlatBasic::HandlePortalPlaybackMessage( KeyValues *pKeyValues )
{
	int nLinkedPortalId = pKeyValues->GetInt( "linkedPortalId" );
	m_pLinkedPortal = nLinkedPortalId >= 0 ? (CPortalRenderable_FlatBasic *)FindRecordedPortal( nLinkedPortalId ) : NULL;
	if( m_pLinkedPortal )
	{
		m_pLinkedPortal->m_pLinkedPortal = this;
	}
	m_bIsPortal2 = pKeyValues->GetInt( "isPortal2" ) != 0;
	matrix3x4_t *pMat = (matrix3x4_t*)pKeyValues->GetPtr( "portalToWorld" );

	MatrixGetColumn( *pMat, 3, m_ptOrigin );
	MatrixGetColumn( *pMat, 0, m_vForward );
	MatrixGetColumn( *pMat, 1, m_vRight );
	MatrixGetColumn( *pMat, 2, m_vUp );
	m_vRight *= -1.0f;

	SetHalfSizes( pKeyValues->GetFloat( "halfWidth", 0.0f ), pKeyValues->GetFloat( "halfHeight", 0.0f ) );
	PortalMoved();

	UTIL_Portal_ComputeMatrix( this, m_pLinkedPortal );
}

extern ConVar mat_wireframe;
static void DrawComplexPortalMesh_SubQuad( Vector &ptBottomLeft, Vector &vUp, Vector &vRight, float *fSubQuadRect, void *pBindEnt, const IMaterial *pMaterial, const VMatrix *pReplacementViewMatrixForTexCoords = NULL )
{
	PortalMeshPoint_t Vertices[4];

	Vertices[0].vWorldSpacePosition = ptBottomLeft + (vRight * fSubQuadRect[2]) + (vUp * fSubQuadRect[3]);
	Vertices[0].texCoord.x = fSubQuadRect[2];
	Vertices[0].texCoord.y = fSubQuadRect[3];

	Vertices[1].vWorldSpacePosition = ptBottomLeft + (vRight * fSubQuadRect[2]) + (vUp * fSubQuadRect[1]);
	Vertices[1].texCoord.x = fSubQuadRect[2];
	Vertices[1].texCoord.y = fSubQuadRect[1];

	Vertices[2].vWorldSpacePosition = ptBottomLeft + (vRight * fSubQuadRect[0]) + (vUp * fSubQuadRect[1]);
	Vertices[2].texCoord.x = fSubQuadRect[0];
	Vertices[2].texCoord.y = fSubQuadRect[1];

	Vertices[3].vWorldSpacePosition = ptBottomLeft + (vRight * fSubQuadRect[0]) + (vUp * fSubQuadRect[3]);
	Vertices[3].texCoord.x = fSubQuadRect[0];
	Vertices[3].texCoord.y = fSubQuadRect[3];	

	Clip_And_Render_Convex_Polygon( Vertices, 4, pMaterial, pBindEnt );
}

#define PORTAL_PROJECTION_MESH_SUBDIVIDE_HEIGHTCHUNKS 8
#define PORTAL_PROJECTION_MESH_SUBDIVIDE_WIDTHCHUNKS 6

void CPortalRenderable_FlatBasic::DrawComplexPortalMesh( IMatRenderContext *pRenderContext, const IMaterial *pMaterial, float fForwardOffsetModifier ) //generates and draws the portal mesh (Needed for compatibility with fixed function rendering)
{
	PortalMeshPoint_t BaseVertices[4];

	Vector ptBottomLeft = m_ptOrigin + (m_vForward * (fForwardOffsetModifier)) - (m_vRight * m_fHalfWidth) - (m_vUp * m_fHalfHeight);
	Vector vScaledUp = m_vUp * (2.0f * m_fHalfHeight);
	Vector vScaledRight = m_vRight * (2.0f * m_fHalfWidth);

	VMatrix matView;
	pRenderContext->GetMatrix( MATERIAL_VIEW, &matView );

	float fSubQuadRect[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

	float fHeightBegin = 0.0f;
	for( int i = 0; i != PORTAL_PROJECTION_MESH_SUBDIVIDE_HEIGHTCHUNKS; ++i )
	{
		float fHeightEnd = fHeightBegin + (1.0f / ((float)PORTAL_PROJECTION_MESH_SUBDIVIDE_HEIGHTCHUNKS));

		fSubQuadRect[1] = fHeightBegin;
		fSubQuadRect[3] = fHeightEnd;

		float fWidthBegin = 0.0f;
		for( int j = 0; j != PORTAL_PROJECTION_MESH_SUBDIVIDE_WIDTHCHUNKS; ++j )
		{
			float fWidthEnd = fWidthBegin + (1.0f / ((float)PORTAL_PROJECTION_MESH_SUBDIVIDE_WIDTHCHUNKS));

			fSubQuadRect[0] = fWidthBegin;
			fSubQuadRect[2] = fWidthEnd;

			DrawComplexPortalMesh_SubQuad( ptBottomLeft, vScaledUp, vScaledRight, fSubQuadRect, GetClientRenderable(), pMaterial );	

			fWidthBegin = fWidthEnd;
		}
		fHeightBegin = fHeightEnd; 
	}

	//pRenderContext->Flush( false );	
}

static void CreateRingMesh( CMeshBuilder &meshBuilder, int nSegs, float flHalfWidth, float flHalfHeight, float flInnerScale, int *pStartVertex, int *pIndexCountOut )
{
	float vTangent[4] = { -1.0f, 0.0f, 0.0f, 1.0f };

	for ( int i = 0; i < nSegs; i++ )
	{
		float flRadians = 2.0f * M_PI * float( i ) / float( nSegs );
		Vector2D vUvOuter( sinf( flRadians), cosf( flRadians ) );
		Vector vPosOuter( -vUvOuter.x * flHalfWidth, -vUvOuter.y * flHalfHeight, 0.0f );
		Vector vPosInner( flInnerScale * vPosOuter );
		Vector2D vUvInner( flInnerScale * vUvOuter );

		vPosOuter *= 1.05f;	// Scale factor that hides the poly edge on the portal ring effect
		vUvOuter *= 1.05f;

		vUvOuter = 0.5f * vUvOuter;
		vUvOuter.x += 0.5f;
		vUvOuter.y += 0.5f;
		vUvInner = 0.5f * vUvInner;
		vUvInner.x += 0.5f;
		vUvInner.y += 0.5f;

		meshBuilder.Position3fv( &vPosOuter.x );
		meshBuilder.TexCoord2f( 0, vUvOuter.x, vUvOuter.y );
		meshBuilder.TexCoord2f( 1, 0.25f, 0.0f );
		meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
		meshBuilder.Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
		meshBuilder.UserData( vTangent );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &vPosInner.x );
		meshBuilder.TexCoord2f( 0, vUvInner.x, vUvInner.y );
		meshBuilder.TexCoord2f( 1, 0.25f, 0.0f );
		meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
		meshBuilder.Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
		meshBuilder.UserData( vTangent );
		meshBuilder.AdvanceVertex();

		int nSegmentIndex = *pStartVertex + 2 * i;
		int nNextSegmentIndex = *pStartVertex + 2 * ( ( i + 1 ) % nSegs );
		meshBuilder.FastIndex( nSegmentIndex );
		meshBuilder.FastIndex( nSegmentIndex + 1 );
		meshBuilder.FastIndex( nNextSegmentIndex + 1);
		meshBuilder.FastIndex( nSegmentIndex );
		meshBuilder.FastIndex( nNextSegmentIndex + 1 );
		meshBuilder.FastIndex( nNextSegmentIndex );
	}

	*pStartVertex += nSegs * 2;
	*pIndexCountOut = nSegs * 6;
}

inline float ComputePointToPortalDistance( const Vector &vPt, const CPortalRenderable_FlatBasic *pPortal, const Vector &vAdjustedOrigin )
{
	// Transform point into portal space

	matrix3x4_t matPortalToWorld( pPortal->m_vRight, pPortal->m_vUp, pPortal->m_vForward, vAdjustedOrigin );
	Vector vPtPortalSpace;
	VectorITransform( vPt, matPortalToWorld, vPtPortalSpace );

	// Compute distance from point to portal bounding rectangle
	Vector vClamped;
	vClamped.x = clamp( vPtPortalSpace.x, -pPortal->GetHalfWidth(), pPortal->GetHalfWidth() );
	vClamped.y = clamp( vPtPortalSpace.y, -pPortal->GetHalfHeight(), pPortal->GetHalfHeight() );
	vClamped.z = 0.0f;

	return ( vPtPortalSpace - vClamped ).Length();
}

IMesh *CPortalRenderable_FlatBasic::CreateMeshForPortals( IMatRenderContext *pRenderContext, int nPortalCount, CPortalRenderable **ppPortals, 
														  CUtlVector< ClampedPortalMeshRenderInfo_t > &clampedPortalMeshRenderInfos )
{
	VPROF_BUDGET( "CreateMeshForPortals", "CreateMeshForPortals" );

	VertexFormat_t unionVertexFmt = (VertexFormat_t)VERTEX_POSITION | VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD0_2D | VERTEX_USERDATA_SIZE(4) |
													VERTEX_TEXCOORD_SIZE( 0, 2 ) | VERTEX_TEXCOORD_SIZE( 1, 2 );

	CMeshBuilder meshBuilder;
	IMesh* pMesh = pRenderContext->GetDynamicMeshEx( unionVertexFmt );
	const int nRingMeshSegmentCount = 12;
	int nMaxTriangles = ( nPortalCount * 2 ) + ( nPortalCount * 10 ) + ( nRingMeshSegmentCount * 2 );	// One quad per portal, plus a near cap poly per portal worst case
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 3 * nMaxTriangles, 3 * nMaxTriangles );

	Vector *pTmpVerts = (Vector*)stackalloc( nPortalCount * 4 * sizeof( Vector ) );
	Vector4D *pGhostColors = (Vector4D*)stackalloc( nPortalCount * sizeof( Vector4D ) );
	float *pMaxDecalOffsets = (float*)stackalloc( nPortalCount * sizeof( float ) );
	Vector vecDeltaRight, vecDeltaUp;
	float vTangent[4];
	const Vector vCameraPos( view->GetViewSetup()->origin );

	int nStartVertex = 0;

	// Create basic quad meshes
	for ( int i = 0; i < nPortalCount; i++ )
	{
		const CPortalRenderable_FlatBasic *pPortal = assert_cast< const CPortalRenderable_FlatBasic* >( ppPortals[i] );

		Vector ptCenter = pPortal->m_ptOrigin;

		//  Math below implements this
		//	verts[0] = ptCenter + (m_vRight * m_fHalfWidth) - (m_vUp * m_fHalfHeight);
		//	verts[1] = ptCenter + (m_vRight * m_fHalfWidth) + (m_vUp * m_fHalfHeight);	
		//	verts[2] = ptCenter - (m_vRight * m_fHalfWidth) - (m_vUp * m_fHalfHeight);
		//	verts[3] = ptCenter - (m_vRight * m_fHalfWidth) + (m_vUp * m_fHalfHeight);

		Vector *pVerts = pTmpVerts + ( 4 * i );

		VectorMultiply( pPortal->m_vRight, pPortal->m_fHalfWidth, vecDeltaRight );
		VectorMultiply( pPortal->m_vUp, pPortal->m_fHalfHeight, vecDeltaUp );
		VectorSubtract( ptCenter, vecDeltaUp, pVerts[0] );
		vecDeltaUp *= 2.0f;
		pVerts[0] += vecDeltaRight;
		vecDeltaRight *= 2.0f;
		VectorAdd( pVerts[0], vecDeltaUp, pVerts[1] );
		VectorSubtract( pVerts[0], vecDeltaRight, pVerts[2] );
		VectorSubtract( pVerts[1], vecDeltaRight, pVerts[3] );

		// Compute max decal offset. We don't want the decal to be offset past the camera position
		Vector vDir( vCameraPos - pPortal->m_ptOrigin );
		float flCameraDistToPortalPlane = DotProduct( vDir, pPortal->m_vForward );
		pMaxDecalOffsets[i] = clamp( flCameraDistToPortalPlane, 0.02f, 0.251f ) - 0.01f;

		Color clrPortal = UTIL_Portal_Color( pPortal->m_bIsPortal2 ? 2 : 1, pPortal->GetTeamNumber() );
		pGhostColors[i].x = float( clrPortal.r() ) / 255.0f;
		pGhostColors[i].y = float( clrPortal.g() ) / 255.0f;
		pGhostColors[i].z = float( clrPortal.b() ) / 255.0f;
		pGhostColors[i].w = pPortal->GetPortalGhostAlpha();
		
		vTangent[0] = -pPortal->m_vRight.x;
		vTangent[1] = -pPortal->m_vRight.y;
		vTangent[2] = -pPortal->m_vRight.z;
		vTangent[3] = 1.0f;

		meshBuilder.Position3fv( &pVerts[0].x );
		meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
		meshBuilder.TexCoord2f( 1, pMaxDecalOffsets[i], 0.0f );
		meshBuilder.Normal3fv( &( pPortal->m_vForward.x ) );
		meshBuilder.Color4fv( &pGhostColors[i].x );
		meshBuilder.UserData( vTangent );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &pVerts[1].x );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.TexCoord2f( 1, pMaxDecalOffsets[i], 0.0f );
		meshBuilder.Normal3fv( &( pPortal->m_vForward.x ) );
		meshBuilder.Color4fv( &pGhostColors[i].x );
		meshBuilder.UserData( vTangent );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &pVerts[3].x );
		meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
		meshBuilder.TexCoord2f( 1, pMaxDecalOffsets[i], 0.0f );
		meshBuilder.Normal3fv( &( pPortal->m_vForward.x ) );
		meshBuilder.Color4fv( &pGhostColors[i].x );
		meshBuilder.UserData( vTangent );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &pVerts[2].x );
		meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
		meshBuilder.TexCoord2f( 1, pMaxDecalOffsets[i], 0.0f );
		meshBuilder.Normal3fv( &( pPortal->m_vForward.x ) );
		meshBuilder.Color4fv( &pGhostColors[i].x );
		meshBuilder.UserData( vTangent );
		meshBuilder.AdvanceVertex();

		meshBuilder.FastQuad( nStartVertex );
		nStartVertex += 4;
	}

	PortalMeshPoint_t portalQuad[16];

	int nInsideVertCount = 0;
	PortalMeshPoint_t insidePoly[16];

	// Make a frustum enclosing the pyramid between the camera position and the near plane.
	VPlane nearPlaneFrustum[5];
	V_memcpy( nearPlaneFrustum, view->GetFrustum(), 5 * sizeof( VPlane ) );
	// flip near plane
	nearPlaneFrustum[ FRUSTUM_NEARZ ].m_Normal = -nearPlaneFrustum[ FRUSTUM_NEARZ ].m_Normal;
	nearPlaneFrustum[ FRUSTUM_NEARZ ].m_Dist = -nearPlaneFrustum[ FRUSTUM_NEARZ ].m_Dist;

	clampedPortalMeshRenderInfos.SetCount( nPortalCount );

	int nStartIndex = 6 * nPortalCount;

	// Compute inverse view-projectio matrix
	VMatrix matView;
	VMatrix matProj;
	pRenderContext->GetMatrix( MATERIAL_VIEW, &matView );
	pRenderContext->GetMatrix( MATERIAL_PROJECTION, &matProj );

	VMatrix matViewProj;
	MatrixMultiply( matProj, matView, matViewProj );
	VMatrix matInvViewProj;
	MatrixInverseGeneral( matViewProj, matInvViewProj );

	// Create meshes with a z-near-cap for portals that intersect the near plane
	for ( int i = 0; i < nPortalCount; i++ )
	{
		const CPortalRenderable_FlatBasic *pPortal = assert_cast< const CPortalRenderable_FlatBasic* >( ppPortals[i] );

		clampedPortalMeshRenderInfos[i].nStartIndex = -1;
		clampedPortalMeshRenderInfos[i].nIndexCount = -1;

		if ( R_CullSphere( nearPlaneFrustum, 5, &pPortal->m_ptOrigin, MAX( pPortal->m_fHalfWidth, pPortal->m_fHalfHeight ) ) )
		{
			// Portal definitely doesn't need a near plane cap
			continue;
		}

		static const float CLIP_NEARPLANE_OFFSET = 0.3f;	// push near plane back a bit so that the near plane cap overlaps the clip seam of the portal quad on the actual near plane
		static const float PROJECT_NEARPLANE_OFFSET = 0.01f;	// push near plane back a bit so that the near plane cap overlaps the clip seam of the portal quad on the actual near plane
		static const float PORTAL_DISTANCE_EPSILON = 0.4f;

		float flTweakedDecalOffset = pMaxDecalOffsets[i] + 0.05f;
		float flDistToPortal = ComputePointToPortalDistance( vCameraPos, pPortal, pPortal->m_ptOrigin + flTweakedDecalOffset * pPortal->m_vForward );
		if ( flDistToPortal < PORTAL_DISTANCE_EPSILON )
		{
			// We're too close to the portal to perform reliable clipping and reprojection to the near plane (the portal plane basically goes through the eye position.
			// Instead, we clip the camera near plane by the portal plane and use the result as our near cap poly.

			// Init portal quad with near plane. Offset a tiny bit from 1.0 to avoid precision errors that can leave a 1-pixel border around the screen uncovered by the quad.
			matInvViewProj.V3Mul( Vector( -1.01f, -1.01f, 0.0f ), portalQuad[0].vWorldSpacePosition );
			matInvViewProj.V3Mul( Vector( -1.01f,  1.01f, 0.0f ), portalQuad[1].vWorldSpacePosition );
			matInvViewProj.V3Mul( Vector(  1.01f,  1.01f, 0.0f ), portalQuad[2].vWorldSpacePosition );
			matInvViewProj.V3Mul( Vector(  1.01f, -1.01f, 0.0f ), portalQuad[3].vWorldSpacePosition );
			portalQuad[0].texCoord.Init( 0.5f, 0.5f );
			portalQuad[1].texCoord.Init( 0.5f, 0.5f );
			portalQuad[2].texCoord.Init( 0.5f, 0.5f );
			portalQuad[3].texCoord.Init( 0.5f, 0.5f );

			// Clip against portal plane
			float flPortalPlaneDist = DotProduct( pPortal->m_vForward, pPortal->m_ptOrigin + flTweakedDecalOffset * pPortal->m_vForward );
			ClipPortalPolyToPlane( portalQuad, 4, insidePoly, &nInsideVertCount, -pPortal->m_vForward, -flPortalPlaneDist );
		}
		else
		{
			// Clip portal quad against view frustum.
			portalQuad[0].vWorldSpacePosition = pTmpVerts[ 4 * i     ] + flTweakedDecalOffset * pPortal->m_vForward;
			portalQuad[1].vWorldSpacePosition = pTmpVerts[ 4 * i + 1 ] + flTweakedDecalOffset * pPortal->m_vForward;
			portalQuad[2].vWorldSpacePosition = pTmpVerts[ 4 * i + 3 ] + flTweakedDecalOffset * pPortal->m_vForward;
			portalQuad[3].vWorldSpacePosition = pTmpVerts[ 4 * i + 2 ] + flTweakedDecalOffset * pPortal->m_vForward;
			portalQuad[0].texCoord.Init( 0.5f, 0.5f );
			portalQuad[1].texCoord.Init( 0.5f, 0.5f );
			portalQuad[2].texCoord.Init( 0.5f, 0.5f );
			portalQuad[3].texCoord.Init( 0.5f, 0.5f );
			/*
			portalQuad[0].texCoord.Init( 0.0f, 1.0f );
			portalQuad[1].texCoord.Init( 0.0f, 0.0f );
			portalQuad[2].texCoord.Init( 1.0f, 0.0f );
			portalQuad[3].texCoord.Init( 1.0f, 1.0f );
			*/

			// Clip against flipped near plane so that we get the part of the portal quad that would be clipped by the near plane. Offset the near plane so that our cap poly will cover up
			// the clip intersection line.
			bool bClippedSomething = ClipPortalPolyToPlane( portalQuad, 4, insidePoly, &nInsideVertCount, nearPlaneFrustum[ FRUSTUM_NEARZ ].m_Normal, 
				nearPlaneFrustum[ FRUSTUM_NEARZ ].m_Dist - CLIP_NEARPLANE_OFFSET );

			if ( !bClippedSomething )
			{
				continue;
			}

			// Clip against the four remaining frustum planes. Offset the planes a tiny bit so that we  have some slack when reprojecting the clipped verts back onto the near plane.
			// This hack is only acceptable because we're not passing the proper texture coordinates for the quad anyways.
			ClipPortalPolyToPlane( insidePoly, nInsideVertCount, portalQuad, &nInsideVertCount, nearPlaneFrustum[ FRUSTUM_RIGHT ].m_Normal, nearPlaneFrustum[ FRUSTUM_RIGHT ].m_Dist - 0.01f );
			ClipPortalPolyToPlane( portalQuad, nInsideVertCount, insidePoly, &nInsideVertCount, nearPlaneFrustum[ FRUSTUM_LEFT ].m_Normal, nearPlaneFrustum[ FRUSTUM_LEFT ].m_Dist - 0.01f );
			ClipPortalPolyToPlane( insidePoly, nInsideVertCount, portalQuad, &nInsideVertCount, nearPlaneFrustum[ FRUSTUM_TOP ].m_Normal, nearPlaneFrustum[ FRUSTUM_TOP ].m_Dist - 0.01f );
			ClipPortalPolyToPlane( portalQuad, nInsideVertCount, insidePoly, &nInsideVertCount, nearPlaneFrustum[ FRUSTUM_BOTTOM ].m_Normal, nearPlaneFrustum[ FRUSTUM_BOTTOM ].m_Dist - 0.01f );
		}

		if ( nInsideVertCount < 3 )
		{
			continue;
		}

		ProjectPortalPolyToPlane( insidePoly, nInsideVertCount, nearPlaneFrustum[ FRUSTUM_NEARZ ].m_Normal, nearPlaneFrustum[ FRUSTUM_NEARZ ].m_Dist - PROJECT_NEARPLANE_OFFSET, vCameraPos );

		vTangent[0] = -pPortal->m_vRight.x;
		vTangent[1] = -pPortal->m_vRight.y;
		vTangent[2] = -pPortal->m_vRight.z;
		vTangent[3] = 1.0f;

		nInsideVertCount = ( nInsideVertCount < 3 ) ? 0 : nInsideVertCount;
		int nInsideTriCount = MAX( nInsideVertCount - 2, 0 );

		clampedPortalMeshRenderInfos[i].nStartIndex = nStartIndex;
		clampedPortalMeshRenderInfos[i].nIndexCount = nInsideTriCount * 3;

		for ( int j = 0; j < nInsideVertCount; j++ )
		{
			meshBuilder.Position3fv( &( insidePoly[j].vWorldSpacePosition.x ) );
			meshBuilder.TexCoord2fv( 0, &( insidePoly[j].texCoord.x ) );
			meshBuilder.TexCoord2f( 1, 0.0f, 0.0f );	// No decal offset for near plane cap poly
			meshBuilder.Normal3fv( &( pPortal->m_vForward.x ) );
			meshBuilder.Color4fv( &pGhostColors[i].x );
			meshBuilder.UserData( vTangent );
			meshBuilder.AdvanceVertex();
		}
		for ( int j = 0; j < nInsideTriCount; j++ )
		{
			meshBuilder.FastIndex( nStartVertex );
			meshBuilder.FastIndex( nStartVertex + j + 1 );
			meshBuilder.FastIndex( nStartVertex + j + 2 );
		}
		nStartVertex += nInsideVertCount;
		nStartIndex += clampedPortalMeshRenderInfos[i].nIndexCount;
	}

	int nRingMeshIndexCount = 0;
	CreateRingMesh( meshBuilder, nRingMeshSegmentCount, 32.0f, 56.0f, 0.65f, &nStartVertex, &nRingMeshIndexCount );

	clampedPortalMeshRenderInfos.AddToTail();
	clampedPortalMeshRenderInfos.Tail().nStartIndex = nStartIndex;
	clampedPortalMeshRenderInfos.Tail().nIndexCount = nRingMeshIndexCount;

	meshBuilder.End();

	return pMesh;
}

bool CPortalRenderable_FlatBasic::ComputeClipSpacePortalCorners( Vector4D *pClipSpacePortalCornersOut, const VMatrix &matViewProj ) const
{
	//  Math below implements this
	//	verts[0] = ptCenter + (m_vRight * m_fHalfWidth) - (m_vUp * m_fHalfHeight);
	//	verts[1] = ptCenter + (m_vRight * m_fHalfWidth) + (m_vUp * m_fHalfHeight);	
	//	verts[2] = ptCenter - (m_vRight * m_fHalfWidth) - (m_vUp * m_fHalfHeight);
	//	verts[3] = ptCenter - (m_vRight * m_fHalfWidth) + (m_vUp * m_fHalfHeight);

	Vector4D verts[4];
	Vector vecDeltaRight, vecDeltaUp;

	VectorMultiply( m_vRight, m_fHalfWidth, vecDeltaRight );
	VectorMultiply( m_vUp, m_fHalfHeight, vecDeltaUp );
	VectorSubtract( m_ptOrigin.Base(), vecDeltaUp.Base(), verts[0].Base() );
	vecDeltaUp *= 2.0f;
	VectorAdd( verts[0].Base(), vecDeltaRight.Base(), verts[0].Base() );
	vecDeltaRight *= 2.0f;
	VectorAdd( verts[0].Base(), vecDeltaUp.Base(), verts[1].Base() );
	VectorSubtract( verts[0].Base(), vecDeltaRight.Base(), verts[2].Base() );
	VectorSubtract( verts[1].Base(), vecDeltaRight.Base(), verts[3].Base() );

	bool bClipsNearPlane = false;
	for ( int i = 0; i < 4; i++ )
	{
		verts[i].w = 1.0f;
		matViewProj.V4Mul( verts[i], pClipSpacePortalCornersOut[i] );
		bClipsNearPlane |= ( pClipSpacePortalCornersOut[i].z <= 0.0f );
	}
	return !bClipsNearPlane;
}

void CPortalRenderable_FlatBasic::DrawSimplePortalMesh( IMatRenderContext *pRenderContext, const IMaterial *pMaterial, float fForwardOffsetModifier, float flAlpha, const Vector *pVertexColor )
{
	VPROF_BUDGET( "DrawSimplePortalMesh", "DrawSimplePortalMesh" );
	pRenderContext->Bind( (IMaterial *)pMaterial, GetClientRenderable() );
	
	// This can depend on the Bind command above, so keep this after!
	UpdateFrontBufferTexturesForMaterial( (IMaterial *)pMaterial );

	pRenderContext->MatrixMode( MATERIAL_MODEL ); //just in case
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	// Disable offset modifier, put it in shader code in portal and portal_refract shaders instead
	// FIXME: Remove this from the code
	fForwardOffsetModifier = 0;

	Vector ptCenter = m_ptOrigin + (m_vForward * fForwardOffsetModifier);

//  Math below implements this
//	verts[0] = ptCenter + (m_vRight * m_fHalfWidth) - (m_vUp * m_fHalfHeight);
//	verts[1] = ptCenter + (m_vRight * m_fHalfWidth) + (m_vUp * m_fHalfHeight);	
//	verts[2] = ptCenter - (m_vRight * m_fHalfWidth) - (m_vUp * m_fHalfHeight);
//	verts[3] = ptCenter - (m_vRight * m_fHalfWidth) + (m_vUp * m_fHalfHeight);

	Vector verts[4];
	Vector vecDeltaRight, vecDeltaUp;
	VectorMultiply( m_vRight, m_fHalfWidth, vecDeltaRight );
	VectorMultiply( m_vUp, m_fHalfHeight, vecDeltaUp );
	VectorSubtract( ptCenter, vecDeltaUp, verts[0] );
	vecDeltaUp *= 2.0f;
	verts[0] += vecDeltaRight;
	vecDeltaRight *= 2.0f;
	VectorAdd( verts[0], vecDeltaUp, verts[1] );
	VectorSubtract( verts[0], vecDeltaRight, verts[2] );
	VectorSubtract( verts[1], vecDeltaRight, verts[3] );

	float pColor[4] = { 1.0f, 1.0f, 1.0f, flAlpha };
	if( pVertexColor )
	{
		memcpy( pColor, pVertexColor, sizeof( float ) * 3 );
	}

	float vTangent[4] = { -m_vRight.x, -m_vRight.y, -m_vRight.z, 1.0f };

	CMeshBuilder meshBuilder;
	IMesh* pMesh = pRenderContext->GetDynamicMesh( false );
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, 2 );

	meshBuilder.Position3fv( &verts[0].x );
	meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
	meshBuilder.TexCoord2f( 1, 0.25f, 0.0f );
	meshBuilder.Normal3f( m_vForward.x, m_vForward.y, m_vForward.z );
	meshBuilder.Color4fv( pColor );
	meshBuilder.UserData( vTangent );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv( &verts[1].x );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.TexCoord2f( 1, 0.25f, 0.0f );
	meshBuilder.Normal3f( m_vForward.x, m_vForward.y, m_vForward.z );
	meshBuilder.Color4fv( pColor );
	meshBuilder.UserData( vTangent );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv( &verts[2].x );
	meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
	meshBuilder.TexCoord2f( 1, 0.25f, 0.0f );
	meshBuilder.Normal3f( m_vForward.x, m_vForward.y, m_vForward.z );
	meshBuilder.Color4fv( pColor );
	meshBuilder.UserData( vTangent );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv( &verts[3].x );
	meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
	meshBuilder.TexCoord2f( 1, 0.25f, 0.0f );
	meshBuilder.Normal3f( m_vForward.x, m_vForward.y, m_vForward.z );
	meshBuilder.Color4fv( pColor );
	meshBuilder.UserData( vTangent );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
	
	if( mat_wireframe.GetBool() )
	{
		pRenderContext->Bind( (IMaterial *)(const IMaterial *)g_pPortalRender->m_MaterialsAccess.m_Wireframe, (CPortalRenderable*)this );

		IMesh* pMesh = pRenderContext->GetDynamicMesh( false );
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, 2 );

		meshBuilder.Position3fv( &verts[0].x );
		meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
		meshBuilder.TexCoord2f( 1, 0.0f, 1.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &verts[1].x );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.TexCoord2f( 1, 0.0f, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &verts[2].x );
		meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
		meshBuilder.TexCoord2f( 1, 1.0f, 1.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &verts[3].x );
		meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
		meshBuilder.TexCoord2f( 1, 1.0f, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
		pMesh->Draw();
	}

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
}



void CPortalRenderable_FlatBasic::DrawRenderFixMesh( IMatRenderContext *pRenderContext, const IMaterial *pMaterial, float fFrontClipDistance )
{
	if( g_pPortalRender->GetViewRecursionLevel() != 0 )
		return; //a render fix should only ever be necessary in the primary view

	Vector ptCameraOrigin = CurrentViewOrigin();

	Vector vPortalCenterToCamera = ptCameraOrigin - m_ptOrigin;
	if( (vPortalCenterToCamera.Dot( m_vForward ) < -1.0f) ) //camera coplanar (to 1.0 units) or in front of portal plane
		return;

	if( vPortalCenterToCamera.LengthSqr() < (m_fHalfHeight * m_fHalfHeight) ) //FIXME: Player closeness check might need reimplementation
	{
		float fOldDist = m_InternallyMaintainedData.m_BoundingPlanes[PORTALRENDERFIXMESH_OUTERBOUNDPLANES].m_Dist;

		float fCameraDist = m_vForward.Dot(ptCameraOrigin - m_ptOrigin);

		if( fFrontClipDistance > fCameraDist ) //never clip further out than the camera, we can see into the garbage space of the portal view's texture
			fFrontClipDistance = fCameraDist;

		m_InternallyMaintainedData.m_BoundingPlanes[PORTALRENDERFIXMESH_OUTERBOUNDPLANES].m_Dist -= fFrontClipDistance;
		Internal_DrawRenderFixMesh( pRenderContext, pMaterial );
		m_InternallyMaintainedData.m_BoundingPlanes[PORTALRENDERFIXMESH_OUTERBOUNDPLANES].m_Dist = fOldDist;
	}


}

void CPortalRenderable_FlatBasic::Internal_DrawRenderFixMesh( IMatRenderContext *pRenderContext, const IMaterial *pMaterial )
{
	PortalMeshPoint_t WorkVertices[12];
	PortalMeshPoint_t clippedVerts[12];
	int nClippedVertCount = 0;
	
	pRenderContext->MatrixMode( MATERIAL_MODEL ); //just in case
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	// Compute inverse view-projection matrix
	VMatrix matView;
	VMatrix matProj;
	pRenderContext->GetMatrix( MATERIAL_VIEW, &matView );
	pRenderContext->GetMatrix( MATERIAL_PROJECTION, &matProj );

	VMatrix matViewProj;
	MatrixMultiply( matProj, matView, matViewProj );
	VMatrix matInvViewProj;
	MatrixInverseGeneral( matViewProj, matInvViewProj );

	VPlane nearPlaneFrustum[5];
	V_memcpy( nearPlaneFrustum, view->GetFrustum(), 5 * sizeof( VPlane ) );
	// flip near plane
	nearPlaneFrustum[ FRUSTUM_NEARZ ].m_Normal = -nearPlaneFrustum[ FRUSTUM_NEARZ ].m_Normal;
	nearPlaneFrustum[ FRUSTUM_NEARZ ].m_Dist = -nearPlaneFrustum[ FRUSTUM_NEARZ ].m_Dist;

	static const float PORTAL_OFFSET = 0.275f;
	static const float CLIP_NEARPLANE_OFFSET = 0.3f;	// push near plane back a bit so that the near plane cap overlaps the clip seam of the portal quad on the actual near plane
	static const float PROJECT_NEARPLANE_OFFSET = 0.01f;	// push near plane back a bit so that the near plane cap overlaps the clip seam of the portal quad on the actual near plane
	static const float PORTAL_DISTANCE_EPSILON = 0.4f;
	const Vector vCameraPos( view->GetViewSetup()->origin );

	Vector vOffsetOrigin( m_ptOrigin + PORTAL_OFFSET * m_vForward );
	float flDistToPortal = ComputePointToPortalDistance( vCameraPos, this, vOffsetOrigin );
	if ( flDistToPortal < PORTAL_DISTANCE_EPSILON )
	{

		// We're too close to the portal to perform reliable clipping and reprojection to the near plane (the portal plane basically goes through the eye position.
		// Instead, we clip the camera near plane by the portal plane and use the result as our near cap poly.

		// Init portal quad with near plane. Offset a tiny bit from 1.0 to avoid precision errors that can leave a 1-pixel border around the screen uncovered by the quad.
		matInvViewProj.V3Mul( Vector( -1.01f, -1.01f, 0.0f ), WorkVertices[0].vWorldSpacePosition );
		matInvViewProj.V3Mul( Vector(  1.01f, -1.01f, 0.0f ), WorkVertices[1].vWorldSpacePosition );
		matInvViewProj.V3Mul( Vector(  1.01f,  1.01f, 0.0f ), WorkVertices[2].vWorldSpacePosition );
		matInvViewProj.V3Mul( Vector( -1.01f,  1.01f, 0.0f ), WorkVertices[3].vWorldSpacePosition );
		WorkVertices[0].texCoord.Init( 0.5f, 0.5f );
		WorkVertices[1].texCoord.Init( 0.5f, 0.5f );
		WorkVertices[2].texCoord.Init( 0.5f, 0.5f );
		WorkVertices[3].texCoord.Init( 0.5f, 0.5f );

		// Clip against portal plane
		float flPortalPlaneDist = DotProduct( m_vForward, vOffsetOrigin );
		ClipPortalPolyToPlane( WorkVertices, 4, clippedVerts, &nClippedVertCount, -m_vForward, -flPortalPlaneDist );
	}
	else
	{

		// Clip portal quad against view frustum.
		WorkVertices[0].vWorldSpacePosition = vOffsetOrigin - (m_vRight * m_fHalfWidth) + (m_vUp * m_fHalfHeight);
		WorkVertices[1].vWorldSpacePosition = vOffsetOrigin + (m_vRight * m_fHalfWidth) + (m_vUp * m_fHalfHeight);
		WorkVertices[2].vWorldSpacePosition = vOffsetOrigin + (m_vRight * m_fHalfWidth) - (m_vUp * m_fHalfHeight);
		WorkVertices[3].vWorldSpacePosition = vOffsetOrigin - (m_vRight * m_fHalfWidth) - (m_vUp * m_fHalfHeight);
		WorkVertices[0].texCoord.Init( 0.5f, 0.5f );
		WorkVertices[1].texCoord.Init( 0.5f, 0.5f );
		WorkVertices[2].texCoord.Init( 0.5f, 0.5f );
		WorkVertices[3].texCoord.Init( 0.5f, 0.5f );

		// Clip against flipped near plane so that we get the part of the portal quad that would be clipped by the near plane. Offset the near plane so that our cap poly will cover up
		// the clip intersection line.
		bool bClippedSomething = ClipPortalPolyToPlane( WorkVertices, 4, clippedVerts, &nClippedVertCount, nearPlaneFrustum[ FRUSTUM_NEARZ ].m_Normal, 
			nearPlaneFrustum[ FRUSTUM_NEARZ ].m_Dist - CLIP_NEARPLANE_OFFSET );

		if ( !bClippedSomething )
		{
			pRenderContext->MatrixMode( MATERIAL_MODEL );
			pRenderContext->PopMatrix();
			return;
		}

		// Clip against the four remaining frustum planes. Offset the planes a tiny bit so that we  have some slack when reprojecting the clipped verts back onto the near plane.
		// This hack is only acceptable because we're not passing the proper texture coordinates for the quad anyways.
		ClipPortalPolyToPlane( clippedVerts, nClippedVertCount, WorkVertices, &nClippedVertCount, nearPlaneFrustum[ FRUSTUM_RIGHT ].m_Normal, nearPlaneFrustum[ FRUSTUM_RIGHT ].m_Dist - 0.01f );
		ClipPortalPolyToPlane( WorkVertices, nClippedVertCount, clippedVerts, &nClippedVertCount, nearPlaneFrustum[ FRUSTUM_LEFT ].m_Normal, nearPlaneFrustum[ FRUSTUM_LEFT ].m_Dist - 0.01f );
		ClipPortalPolyToPlane( clippedVerts, nClippedVertCount, WorkVertices, &nClippedVertCount, nearPlaneFrustum[ FRUSTUM_TOP ].m_Normal, nearPlaneFrustum[ FRUSTUM_TOP ].m_Dist - 0.01f );
		ClipPortalPolyToPlane( WorkVertices, nClippedVertCount, clippedVerts, &nClippedVertCount, nearPlaneFrustum[ FRUSTUM_BOTTOM ].m_Normal, nearPlaneFrustum[ FRUSTUM_BOTTOM ].m_Dist - 0.01f );
	}

	if ( nClippedVertCount < 3 )
	{
		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PopMatrix();
		return;
	}

	ProjectPortalPolyToPlane( clippedVerts, nClippedVertCount, nearPlaneFrustum[ FRUSTUM_NEARZ ].m_Normal, nearPlaneFrustum[ FRUSTUM_NEARZ ].m_Dist - PROJECT_NEARPLANE_OFFSET, vCameraPos );
	pRenderContext->OverrideDepthEnable( true, true, false );
	RenderPortalMeshConvexPolygon( clippedVerts, nClippedVertCount, pMaterial, this );
	pRenderContext->OverrideDepthEnable( false, true, true );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
}

void CPortalRenderable_FlatBasic::ClipFixToBoundingAreaAndDraw( PortalMeshPoint_t *pVerts, const IMaterial *pMaterial )
{
	PortalMeshPoint_t *pInVerts = (PortalMeshPoint_t *)stackalloc( 4 * (PORTALRENDERFIXMESH_OUTERBOUNDPLANES + 2) * 16 * sizeof( PortalMeshPoint_t ) ); //really only should need 4x points, but I'm paranoid
	PortalMeshPoint_t *pOutVerts = (PortalMeshPoint_t *)stackalloc( 4 * (PORTALRENDERFIXMESH_OUTERBOUNDPLANES + 2) * 16 * sizeof( PortalMeshPoint_t ) );
	PortalMeshPoint_t *pTempVerts;

	memcpy( pInVerts, pVerts, sizeof( PortalMeshPoint_t ) * 4 );
	int iVertCount = 4;
	
	//clip by bounding area planes
	{
		for( int i = 0; i != (PORTALRENDERFIXMESH_OUTERBOUNDPLANES + 1); ++i )
		{
			if( iVertCount < 3 )
				return; //nothing to draw

			iVertCount = ClipPolyToPlane_LerpTexCoords( pInVerts, iVertCount, pOutVerts, m_InternallyMaintainedData.m_BoundingPlanes[i].m_Normal, m_InternallyMaintainedData.m_BoundingPlanes[i].m_Dist, 0.01f );
			pTempVerts = pInVerts; pInVerts = pOutVerts; pOutVerts = pTempVerts; //swap vertex pointers
		}

		if( iVertCount < 3 )
			return; //nothing to draw
	}


	//clip by the viewing frustum
	{
		VPlane *pFrustum = view->GetFrustum();

		for( int i = 0; i != FRUSTUM_NUMPLANES; ++i )
		{
			if( iVertCount < 3 )
				return; //nothing to draw

			iVertCount = ClipPolyToPlane_LerpTexCoords( pInVerts, iVertCount, pOutVerts, pFrustum[i].m_Normal, pFrustum[i].m_Dist, 0.01f );
			pTempVerts = pInVerts; pInVerts = pOutVerts; pOutVerts = pTempVerts; //swap vertex pointers
		}

		if( iVertCount < 3 )
			return; //nothing to draw
	}

	CMatRenderContextPtr pRenderContext( materials );

	//project the points so we can fudge the numbers a bit and move them to exactly 0.0f depth
	{
		VMatrix matProj, matView, matViewProj;
		pRenderContext->GetMatrix( MATERIAL_PROJECTION, &matProj );
		pRenderContext->GetMatrix( MATERIAL_VIEW, &matView );
		MatrixMultiply( matProj, matView, matViewProj );

		for( int i = 0; i != iVertCount; ++i )
		{
			float W, inverseW;

			W = matViewProj.m[3][0] * pInVerts[i].vWorldSpacePosition.x;
			W += matViewProj.m[3][1] * pInVerts[i].vWorldSpacePosition.y;
			W += matViewProj.m[3][2] * pInVerts[i].vWorldSpacePosition.z;
			W += matViewProj.m[3][3];
			
			inverseW = 1.0f / W;


			pInVerts[i].vWorldSpacePosition = matViewProj * pInVerts[i].vWorldSpacePosition;
			pInVerts[i].vWorldSpacePosition *= inverseW;
			pInVerts[i].vWorldSpacePosition.z = 0.00001f; //the primary reason we're projecting on the CPU to begin with
		}
	}

	//render with identity transforms and clipping disabled
	{
		bool bClippingEnabled = pRenderContext->EnableClipping( false );

		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PushMatrix();
		pRenderContext->LoadIdentity();

		pRenderContext->MatrixMode( MATERIAL_VIEW );
		pRenderContext->PushMatrix();
		pRenderContext->LoadIdentity();

		pRenderContext->MatrixMode( MATERIAL_PROJECTION );
		pRenderContext->PushMatrix();
		pRenderContext->LoadIdentity();

		RenderPortalMeshConvexPolygon( pInVerts, iVertCount, pMaterial, this );
		if( mat_wireframe.GetBool() )
			RenderPortalMeshConvexPolygon( pInVerts, iVertCount, materials->FindMaterial( "shadertest/wireframe", TEXTURE_GROUP_CLIENT_EFFECTS, false ), this );

		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PopMatrix();

		pRenderContext->MatrixMode( MATERIAL_VIEW );
		pRenderContext->PopMatrix();

		pRenderContext->MatrixMode( MATERIAL_PROJECTION );
		pRenderContext->PopMatrix();

		pRenderContext->EnableClipping( bClippingEnabled );
	}
}


//-----------------------------------------------------------------------------
// renders a quad that simulates fog as an overlay for something else (most notably the hole we create for stencil mode portals)
//-----------------------------------------------------------------------------
void CPortalRenderable_FlatBasic::RenderFogQuad( void )
{
	CMatRenderContextPtr pRenderContext( materials );
	if( pRenderContext->GetFogMode() == MATERIAL_FOG_NONE )
		return;

	float fFogStart, fFogEnd;
	pRenderContext->GetFogDistances( &fFogStart, &fFogEnd, NULL );
	float vertexColor[4];
	unsigned char fogColor[3];
	pRenderContext->GetFogColor( fogColor );
	float fFogMaxDensity = 1.0f; //pRenderContext->GetFogMaxDensity(); //function only exists in a pickled changelist for now

	/*float fColorScale = LinearToGammaFullRange( pRenderContext->GetToneMappingScaleLinear().x );

	fogColor[0] *= fColorScale;
	fogColor[1] *= fColorScale;
	fogColor[2] *= fColorScale;*/

	vertexColor[0] = fogColor[0] * (1.0f / 255.0f);
	vertexColor[1] = fogColor[1] * (1.0f / 255.0f);
	vertexColor[2] = fogColor[2] * (1.0f / 255.0f);

	float ooFogRange = 1.0f;
	if ( fFogEnd != fFogStart )
	{
		ooFogRange = 1.0f / (fFogEnd - fFogStart);
	}

	float FogEndOverFogRange = ooFogRange * fFogEnd;

	VMatrix matView, matViewProj, matProj;
	pRenderContext->GetMatrix( MATERIAL_VIEW, &matView );
	pRenderContext->GetMatrix( MATERIAL_PROJECTION, &matProj );
	MatrixMultiply( matProj, matView, matViewProj );

	Vector vUp = m_vUp * (m_fHalfHeight * 2.0f);
	Vector vRight = m_vRight * (m_fHalfWidth * 2.0f);

	Vector ptCorners[4];
	ptCorners[0] = (m_ptOrigin + vUp) + vRight;
	ptCorners[1] = (m_ptOrigin + vUp) - vRight;
	ptCorners[2] = (m_ptOrigin - vUp) + vRight;
	ptCorners[3] = (m_ptOrigin - vUp) - vRight;


	pRenderContext->Bind( (IMaterial *)(const IMaterial *)g_pPortalRender->m_MaterialsAccess.m_TranslucentVertexColor );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, 2 );

	for( int i = 0; i != 4; ++i )
	{
		float projZ;
		projZ = matViewProj.m[2][0] * ptCorners[i].x;
		projZ += matViewProj.m[2][1] * ptCorners[i].y;
		projZ += matViewProj.m[2][2] * ptCorners[i].z;
		projZ += matViewProj.m[2][3];

		float fFogAmount = ((-projZ * ooFogRange) + FogEndOverFogRange); //projZ should be negative

		//range fix
		if( fFogAmount >= fFogMaxDensity )
			fFogAmount = fFogMaxDensity;

		if( fFogAmount <= 0.0f )
			fFogAmount = 0.0f;

		vertexColor[3] = fFogMaxDensity - fFogAmount; //alpha is inverse fog

		meshBuilder.Position3fv( &ptCorners[i].x );
		meshBuilder.Color4fv( vertexColor );
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	pMesh->Draw();
}


bool CPortalRenderable_FlatBasic::DoesExitViewIntersectWaterPlane( float waterZ, int leafWaterDataID ) const
{
	bool bAboveWater = false, bBelowWater = false;
	for( int i = 0; i != 4; ++i )
	{
		bAboveWater |= (m_InternallyMaintainedData.m_ptCorners[i].z >= waterZ);
		bBelowWater |= (m_InternallyMaintainedData.m_ptCorners[i].z <= waterZ);
	}

	if( !(bAboveWater && bBelowWater) )
	{
		RANDOM_CEG_TEST_SECRET();
		return false;
	}

	Vector vMins, vMaxs;
	vMins = vMaxs = m_InternallyMaintainedData.m_ptCorners[0];
	for( int i = 1; i != 4; ++i )
	{
		vMins = VectorMin( vMins, m_InternallyMaintainedData.m_ptCorners[i] );
		vMaxs = VectorMax( vMaxs, m_InternallyMaintainedData.m_ptCorners[i] );
	}

	return render->DoesBoxIntersectWaterVolume( vMins, vMaxs, leafWaterDataID );
}

float CPortalRenderable_FlatBasic::GetPortalDistanceBias() const
{
	Vector vCameraToExitPortal = m_ptOrigin - CurrentViewOrigin();
	float flDistModifier = vCameraToExitPortal.Length();
	flDistModifier = MAX( flDistModifier, 0.01f );
	
	// Increase the distance factor based on angle (so we drop detail more aggressively when viewed at an angle)
//	Vector vNormalizedCameraToExitPortal = vCameraToExitPortal * ( 1.0f / flDistModifier );	
//	float flAngleModifier = vNormalizedCameraToExitPortal.Dot( m_vForward );
//	flAngleModifier = clamp( flAngleModifier, 0.01f, 1.0f );
//	flAngleModifier = sqrtf( flAngleModifier ); // Use a square root to reduce the multiplier effect
//	flDistModifier /= flAngleModifier;

	return flDistModifier;
}

bool CPortalRenderable_FlatBasic::ShouldUpdatePortalView_BasedOnView( const CViewSetup &currentView, const CUtlVector<VPlane> &currentComplexFrustum  )
{
	if( m_pLinkedPortal == NULL )
		return false;

	Vector vCameraPos = currentView.origin;

	if( (g_pPortalRender->GetViewRecursionLevel() == 0) &&
		((m_ptOrigin - vCameraPos).LengthSqr() < (m_fHalfHeight * m_fHalfHeight)) ) //FIXME: Player closeness check might need reimplementation
	{
		return true; //fudgery time. The player might not be able to see the surface, but they can probably see the render fix
	}

	if( m_vForward.Dot( vCameraPos ) <= m_InternallyMaintainedData.m_fPlaneDist )
		return false; //looking at portal backface

	int iCurrentFrustmPlanes = currentComplexFrustum.Count();

	//now slice up the portal quad and see if any is visible within the frustum
	int allocSize = (6 + currentComplexFrustum.Count()); //possible to add 1 point per cut, 4 starting points, N plane cuts, 2 extra because I'm paranoid
	Vector *pInVerts = (Vector *)stackalloc( sizeof( Vector ) * allocSize * 2 );
	Vector *pOutVerts = pInVerts + allocSize;
	Vector *pTempVerts;

	//clip by first plane and put output into pInVerts
	int iVertCount = ClipPolyToPlane( m_InternallyMaintainedData.m_ptCorners, 4, pInVerts, currentComplexFrustum[0].m_Normal, currentComplexFrustum[0].m_Dist, 0.01f );

	//clip by other planes and flipflop in and out pointers
	for( int i = 1; i != iCurrentFrustmPlanes; ++i )
	{
		if( iVertCount < 3 )
			return false; //nothing left in the frustum

		iVertCount = ClipPolyToPlane( pInVerts, iVertCount, pOutVerts, currentComplexFrustum[i].m_Normal, currentComplexFrustum[i].m_Dist, 0.01f );
		pTempVerts = pInVerts; pInVerts = pOutVerts; pOutVerts = pTempVerts; //swap vertex pointers
	}

	if( iVertCount < 3 )
		return false; //nothing left in the frustum

	return true;
}

bool CPortalRenderable_FlatBasic::ShouldUpdatePortalView_BasedOnPixelVisibility( float fScreenFilledByStencilMaskLastFrame_Normalized )
{
	return (fScreenFilledByStencilMaskLastFrame_Normalized < 0.0f) || // < 0 is an error value
			(fScreenFilledByStencilMaskLastFrame_Normalized > PORTALRENDERABLE_FLATBASIC_MINPIXELVIS );
}

CPortalRenderable *CreatePortal_FlatBasic_Fn( void )
{
	return new CPortalRenderable_FlatBasic;
}

static CPortalRenderableCreator_AutoRegister CreatePortal_FlatBasic( "Flat Basic", CreatePortal_FlatBasic_Fn );
