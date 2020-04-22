//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "render_pch.h"
#include "client.h"
#include "debug_leafvis.h"
#include "con_nprint.h"
#include "tier0/fasttimer.h"
#include "r_areaportal.h"
#include "cmodel_engine.h"
#include "con_nprint.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar r_ClipAreaPortals( "r_ClipAreaPortals", "1", FCVAR_CHEAT );
ConVar r_DrawPortals( "r_DrawPortals", "0", FCVAR_CHEAT );
ConVar r_ClipAreaFrustums( "r_ClipAreaFrustums", "1", FCVAR_CHEAT );

CUtlVector<CPortalRect> g_PortalRects;
bool g_bViewerInSolidSpace = false;		  

// ------------------------------------------------------------------------------------ //
// Classes.
// ------------------------------------------------------------------------------------ //


#define MAX_PORTAL_VERTS	32

// ------------------------------------------------------------------------------------ //
// Globals.
// ------------------------------------------------------------------------------------ //

// Visible areas from the client DLL + occluded areas using area portals.
#if defined(_PS3)
unsigned char g_RenderAreaBits[32] ALIGN16;
#else
unsigned char g_RenderAreaBits[32];
#endif

// Used to prevent it from coming back into portals while flowing through them.
static unsigned char g_AreaStack[32];
static uint32 g_AreaCounter[MAX_MAP_AREAS];
static CPortalRect g_AreaRect[MAX_MAP_AREAS];

// Frustums for each area for the current frame. Used to cull out leaves.
CUtlVector< Frustum_t, CUtlMemoryAligned< Frustum_t,16 > > g_AreaFrustum;

// List of areas marked visible this frame.
static unsigned short g_VisibleAreas[MAX_MAP_AREAS];
static int g_nVisibleAreas;

// Tied to g_AreaCounter.
static uint32 g_GlobalCounter = 1;

// ------------------------------------------------------------------------------------ //
// Functions.
// ------------------------------------------------------------------------------------ //
void R_Areaportal_LevelInit()
{
	g_AreaFrustum.SetCount( host_state.worldbrush->m_nAreas );
	V_memset( g_AreaCounter, 0, sizeof(g_AreaCounter) );
	g_GlobalCounter = 1;
}

void R_Areaportal_LevelShutdown()
{
	g_AreaFrustum.Purge();
	g_PortalRects.Purge();
}

static inline void R_SetBit( unsigned char *pBits, int bit )
{
	pBits[bit>>3] |= (1 << (bit&7));
}

static inline void R_ClearBit( unsigned char *pBits, int bit )
{
	pBits[bit>>3] &= ~(1 << (bit&7));
}

static inline unsigned char R_TestBit( unsigned char *pBits, int bit )
{
	return pBits[bit>>3] & (1 << (bit&7));
}

struct portalclip_t
{
	portalclip_t()
	{
		lists[0] = v0;
		lists[1] = v1;
	}
	Vector v0[MAX_PORTAL_VERTS];
	Vector v1[MAX_PORTAL_VERTS];
	Vector *lists[2];
};

// Transforms and clips the portal's verts to the view frustum. Returns false
// if the verts lie outside the frustum.
static inline bool GetPortalScreenExtents( dareaportal_t *pPortal, 
	portalclip_t * RESTRICT clip, CPortalRect &portalRect , float *pReflectionWaterHeight, VPlane *pFrustumPlanes )
{
	portalRect.left = portalRect.bottom = 1e24;
	portalRect.right = portalRect.top   = -1e24;
	bool bValidExtents = false;
	worldbrushdata_t *pBrushData = host_state.worldbrush;
	
	int nStartVerts = MIN( pPortal->m_nClipPortalVerts, MAX_PORTAL_VERTS );

	// NOTE: We need two passes to deal with reflection. We need to compute
	// the screen extents for both the reflected + non-reflected area portals
	// and make bounds that surrounds them both.
	int nPassCount = ( pReflectionWaterHeight != NULL ) ? 2 : 1;
	for ( int j = 0; j < nPassCount; ++j )
	{
		int i;
		for( i=0; i < nStartVerts; i++ )
		{
			clip->v0[i] = pBrushData->m_pClipPortalVerts[pPortal->m_FirstClipPortalVert+i];

			// 2nd pass is to compute the reflected areaportal position
			if ( j == 1 )
			{
				clip->v0[i].z = 2.0f * ( *pReflectionWaterHeight ) - clip->v0[i].z;
			}
		}

		int iCurList = 0;
		bool bAllClipped = false;
		for( int iPlane=0; iPlane < 4; iPlane++ )
		{
			Vector *pIn = clip->lists[iCurList];
			Vector *pOut = clip->lists[!iCurList];

			int nOutVerts = 0;
			int iPrev = nStartVerts - 1;
			float flPrevDot = pFrustumPlanes[iPlane].m_Normal.Dot( pIn[iPrev] ) - pFrustumPlanes[iPlane].m_Dist;
			for( int iCur=0; iCur < nStartVerts; iCur++ )
			{
				float flCurDot = pFrustumPlanes[iPlane].m_Normal.Dot( pIn[iCur] ) - pFrustumPlanes[iPlane].m_Dist;

				if( (flCurDot > 0) != (flPrevDot > 0) )
				{
					if( nOutVerts < MAX_PORTAL_VERTS )
					{
						// Add the vert at the intersection.
						float t = flPrevDot / (flPrevDot - flCurDot);
						VectorLerp( pIn[iPrev], pIn[iCur], t, pOut[nOutVerts] );

						++nOutVerts;
					}
				}

				// Add this vert?
				if( flCurDot > 0 )
				{
					if( nOutVerts < MAX_PORTAL_VERTS )
					{
						pOut[nOutVerts] = pIn[iCur];
						++nOutVerts;
					}
				}

				flPrevDot = flCurDot;
				iPrev = iCur;
			}

			if( nOutVerts == 0 )
			{
				// If they're all behind, then this portal is clipped out.
				bAllClipped = true;
				break;
			}

			nStartVerts = nOutVerts;
			iCurList = !iCurList;
		}

		if ( bAllClipped )
			continue;

		// Project all the verts and figure out the screen extents.
		Vector screenPos;
		Assert( iCurList == 0 );
		for( i=0; i < nStartVerts; i++ )
		{
			Vector &point = clip->v0[i];

			g_EngineRenderer->ClipTransform( point, &screenPos );

			portalRect.left   = fpmin( screenPos.x, portalRect.left );
			portalRect.bottom = fpmin( screenPos.y, portalRect.bottom );
			portalRect.top    = fpmax( screenPos.y, portalRect.top );
			portalRect.right  = fpmax( screenPos.x, portalRect.right );
		}
		bValidExtents = true;
	}

	if ( !bValidExtents )
	{
		portalRect.left = portalRect.bottom = 0;
		portalRect.right = portalRect.top   = 0;
	}

	return bValidExtents;
}


// Fill in the intersection between the two rectangles.
inline bool GetRectIntersection( CPortalRect const *pRect1, CPortalRect const *pRect2, CPortalRect *pOut )
{
	pOut->left  = fpmax( pRect1->left, pRect2->left );
	pOut->right = fpmin( pRect1->right, pRect2->right );
	if( pOut->left >= pOut->right )
		return false;

	pOut->bottom = fpmax( pRect1->bottom, pRect2->bottom );
	pOut->top    = fpmin( pRect1->top, pRect2->top );
	if( pOut->bottom >= pOut->top )
		return false;

	return true;
}

static void R_FlowThroughArea( int area, const Vector &vecVisOrigin, const CPortalRect *pClipRect, 
	const VisOverrideData_t* pVisData, float *pReflectionWaterHeight )
{
#ifndef DEDICATED
	// Update this area's frustum.
	if( g_AreaCounter[area] != g_GlobalCounter )
	{
		g_VisibleAreas[g_nVisibleAreas] = area;
		++g_nVisibleAreas;

		g_AreaCounter[area] = g_GlobalCounter;
		g_AreaRect[area] = *pClipRect;
	}
	else
	{
		// Expand the areaportal's rectangle to include the new cliprect.
		CPortalRect *pFrustumRect = &g_AreaRect[area];
		pFrustumRect->left   = fpmin( pFrustumRect->left, pClipRect->left );
		pFrustumRect->bottom = fpmin( pFrustumRect->bottom, pClipRect->bottom );
		pFrustumRect->top    = fpmax( pFrustumRect->top, pClipRect->top );
		pFrustumRect->right  = fpmax( pFrustumRect->right, pClipRect->right );
	}
	
	// Mark this area as visible.
	R_SetBit( g_RenderAreaBits, area );

	// Set that we're in this area on the stack.
	R_SetBit( g_AreaStack, area );

	worldbrushdata_t *pBrushData = host_state.worldbrush;

	Assert( area < host_state.worldbrush->m_nAreas );
	darea_t *pArea = &host_state.worldbrush->m_pAreas[area];
	// temp buffer for clipping
	portalclip_t clipTmp;
	VPlane frustumPlanes[FRUSTUM_NUMPLANES];
	g_Frustum.GetPlanes( frustumPlanes );
	// Check all areas that connect to this area.
	for( int iAreaPortal=0; iAreaPortal < pArea->numareaportals; iAreaPortal++ )
	{
		Assert( pArea->firstareaportal + iAreaPortal < pBrushData->m_nAreaPortals );
		dareaportal_t *pAreaPortal = &pBrushData->m_pAreaPortals[ pArea->firstareaportal + iAreaPortal ];

		// Don't flow back into a portal on the stack.
		if( R_TestBit( g_AreaStack, pAreaPortal->otherarea ) )
			continue;

		// If this portal is closed, don't go through it.
		if ( !R_TestBit( GetBaseLocalClient().m_chAreaPortalBits, pAreaPortal->m_PortalKey ) )
			continue;

		// Make sure the viewer is on the right side of the portal to see through it.
		cplane_t *pPlane = &pBrushData->planes[ pAreaPortal->planenum ];
		// Use the specified vis origin to test backface culling, or the main view if none was specified
		float flDist = pPlane->normal.Dot( vecVisOrigin ) - pPlane->dist;
		if( flDist < -0.1f )
			continue;

		// If the client doesn't want this area visible, don't try to flow into it.
		if( !R_TestBit( GetBaseLocalClient().m_chAreaBits, pAreaPortal->otherarea ) )
			continue;

		CPortalRect portalRect;
		bool portalVis = true;

		// don't try to clip portals if the viewer is practically in the plane
		float fDistTolerance = (pVisData)?(pVisData->m_fDistToAreaPortalTolerance):(0.1f);
		if ( flDist > fDistTolerance )
		{
			portalVis = GetPortalScreenExtents( pAreaPortal, &clipTmp, portalRect, pReflectionWaterHeight, frustumPlanes );
		}
		else
		{
			portalRect.left = -1;
			portalRect.top = 1;
			portalRect.right = 1;
			portalRect.bottom = -1;	// note top/bottom reversed!
			//portalVis=true - not needed, default
		}
		if( portalVis )
		{
			CPortalRect intersection;
			if( GetRectIntersection( &portalRect, pClipRect, &intersection ) )
			{
#ifdef USE_CONVARS
				if( r_DrawPortals.GetInt() )
				{
					g_PortalRects.AddToTail( intersection );
				}
#endif

				// Ok, we can see into this area.
				R_FlowThroughArea( pAreaPortal->otherarea, vecVisOrigin, &intersection, pVisData, pReflectionWaterHeight );
			}
		}
	}
	
	// Mark that we're leaving this area.
	R_ClearBit( g_AreaStack, area );
#endif
}


static void IncrementGlobalCounter()
{
	if( g_GlobalCounter == 0xFFFFFFFF )
	{
		for( int i=0; i < g_AreaFrustum.Count(); i++ )
			g_AreaCounter[i] = 0;
	
		g_GlobalCounter = 1;
	}
	else
	{
		g_GlobalCounter++;
	}
}

ConVar r_snapportal( "r_snapportal", "-1" );
static void R_SetupVisibleAreaFrustums()
{
#ifndef DEDICATED
	const CViewSetup &viewSetup = g_EngineRenderer->ViewGetCurrent();
	
	CPortalRect viewWindow;
	if( viewSetup.m_bOrtho )
	{
		viewWindow.right	= viewSetup.m_OrthoRight;
		viewWindow.left		= viewSetup.m_OrthoLeft;
		viewWindow.top		= viewSetup.m_OrthoTop;
		viewWindow.bottom	= viewSetup.m_OrthoBottom;
	}
	else
	{
		// Assuming a view plane distance of 1, figure out the boundaries of a window
		// the view would project into given the FOV.
		float xFOV = g_EngineRenderer->GetFov() * 0.5f;
		float yFOV = g_EngineRenderer->GetFovY() * 0.5f;

		viewWindow.right	= tan( DEG2RAD( xFOV ) );
		viewWindow.left		= -viewWindow.right;
		viewWindow.top		= tan( DEG2RAD( yFOV ) );
		viewWindow.bottom	= -viewWindow.top;
	}

	Vector viewOrigin = CurrentViewOrigin();
	Vector forward = CurrentViewForward();
	Vector right = CurrentViewRight();
	Vector up = CurrentViewUp();
	VPlane planes[FRUSTUM_NUMPLANES];

	// Now scale the portals as specified in the normalized view frustum (-1,-1,1,1)
	// into our view window and generate planes out of that.
	for( int i=0; i < g_nVisibleAreas; i++ )
	{
		CPortalRect *pRect = &g_AreaRect[ g_VisibleAreas[i] ];
		Frustum_t *pFrustum = &g_AreaFrustum[g_VisibleAreas[i]];
		CPortalRect portalWindow;
		portalWindow.left    = RemapVal( pRect->left,   -1, 1, viewWindow.left,   viewWindow.right );
		portalWindow.right   = RemapVal( pRect->right,  -1, 1, viewWindow.left,   viewWindow.right );
		portalWindow.top     = RemapVal( pRect->top,    -1, 1, viewWindow.bottom, viewWindow.top );
		portalWindow.bottom  = RemapVal( pRect->bottom, -1, 1, viewWindow.bottom, viewWindow.top );
		
		if( viewSetup.m_bOrtho )
		{
			// Left and right planes...
			float orgOffset = DotProduct(viewOrigin, right);
			planes[FRUSTUM_LEFT].Init( right, portalWindow.left + orgOffset );
			planes[FRUSTUM_RIGHT].Init( -right, -portalWindow.right - orgOffset );

			// Top and bottom planes...
			orgOffset = DotProduct(viewOrigin, up);
			planes[FRUSTUM_TOP].Init( up, portalWindow.top + orgOffset );
			planes[FRUSTUM_BOTTOM].Init( -up, -portalWindow.bottom - orgOffset );
			planes[FRUSTUM_NEARZ].Init( forward, 0 );
			planes[FRUSTUM_FARZ].Init( -forward, -1e6f );
			pFrustum->SetPlanes(planes);
		}
		else
		{
			Vector normal;

			// right side
			normal = portalWindow.right * forward - right;
			VectorNormalize(normal);
			planes[FRUSTUM_RIGHT].Init( normal, DotProduct(normal,viewOrigin) );
			// left side
			normal = CurrentViewRight() - portalWindow.left * forward;
			VectorNormalize(normal);
			planes[FRUSTUM_LEFT].Init( normal, DotProduct(normal,viewOrigin) );

			// top
			normal = portalWindow.top * forward - up;
			VectorNormalize(normal);
			planes[FRUSTUM_TOP].Init( normal, DotProduct(normal,viewOrigin) );

			// bottom
			normal = up - portalWindow.bottom * forward;
			VectorNormalize(normal);
			planes[FRUSTUM_BOTTOM].Init( normal, DotProduct(normal,viewOrigin) );

			// nearz
			planes[FRUSTUM_NEARZ].Init( forward, DotProduct(forward, viewOrigin) + viewSetup.zNear );
			
			// farz
			planes[FRUSTUM_FARZ].Init( -forward, DotProduct(-forward, viewOrigin) - viewSetup.zFar );
			pFrustum->SetPlanes(planes);
		}
	}
	// DEBUG: Code to visualize the areaportal frustums in 3D
	// Useful for debugging
	if ( r_snapportal.GetInt() >= 0 )
	{
		extern void CSGFrustum( Frustum_t &frustum );
		for ( int i = 0; i < g_nVisibleAreas; i++ )
		{
			if ( g_VisibleAreas[i] == r_snapportal.GetInt() )
			{
				Frustum_t *pFrustum = &g_AreaFrustum[ g_VisibleAreas[i] ];

				pFrustum->SetPlane( FRUSTUM_NEARZ, forward, 
					DotProduct(forward, viewOrigin) );
				pFrustum->SetPlane( FRUSTUM_FARZ, -forward, 
					DotProduct(-forward, viewOrigin + forward*500) );
				r_snapportal.SetValue( -1 );
				CSGFrustum( *pFrustum );
			}
		}
	}

#endif
}

// culls a node to the frustum or area frustum
bool R_CullNode( mnode_t *pNode )
{
	if ( !g_bViewerInSolidSpace && pNode->area > 0 )
	{
		// First make sure its whole area is even visible.
		if( !R_IsAreaVisible( pNode->area ) )
			return true;
		return CullNodeSIMD( g_AreaFrustum[pNode->area], pNode );
	}

	return CullNodeSIMD( g_Frustum, pNode );
}

static ConVar r_portalscloseall( "r_portalscloseall", "0" );
static ConVar r_portalsopenall( "r_portalsopenall", "0", FCVAR_CHEAT, "Open all portals" );
static ConVar r_ShowViewerArea( "r_ShowViewerArea", "0" );

void R_SetupAreaBits( int iForceViewLeaf /* = -1 */, const VisOverrideData_t* pVisData /* = NULL */, float *pWaterReflectionHeight /* = NULL */ )
{
	IncrementGlobalCounter();

	Vector vVisOrigin =  pVisData ? pVisData->m_vecVisOrigin : g_EngineRenderer->ViewOrigin();

	// Clear the visible area bits.
	memset( g_RenderAreaBits, 0, sizeof( g_RenderAreaBits ) );
	memset( g_AreaStack, 0, sizeof( g_AreaStack ) );

	// Our initial clip rect is the whole screen.
	CPortalRect rect;
	rect.left = rect.bottom = -1;
	rect.top = rect.right = 1;

	// Flow through areas starting at the one we're in.
	int leaf = iForceViewLeaf;
	// If view point override wasn't specified, use the current view origin
	if ( iForceViewLeaf == -1  ) 
	{
		leaf = CM_PointLeafnum( vVisOrigin );
	}

	g_bViewerInSolidSpace = false;
	if( r_portalscloseall.GetBool() )
	{
		if ( GetBaseLocalClient().m_bAreaBitsValid )
		{
			// Clear the visible area bits.
			memset( g_RenderAreaBits, 0, sizeof( g_RenderAreaBits ) );
			int area = host_state.worldbrush->leafs[leaf].area;
			R_SetBit( g_RenderAreaBits, area );
			
			g_VisibleAreas[0] = area;
			g_nVisibleAreas = 1;

			g_AreaCounter[area] = g_GlobalCounter;
			g_AreaRect[area] = rect;
		}
		else
		{
			g_bViewerInSolidSpace = true;
		}
	}
	else
	{
		int ss_Slot = GET_ACTIVE_SPLITSCREEN_SLOT();

		if ( host_state.worldbrush->leafs[leaf].contents & CONTENTS_SOLID ||
			 GetBaseLocalClient().ishltv || 
#if defined( REPLAY_ENABLED )
			 GetBaseLocalClient().isreplay ||
#endif
			 !GetBaseLocalClient().m_bAreaBitsValid || 
			 r_portalsopenall.GetBool()  )
		{
			// Draw everything if we're in solid space or if r_portalsopenall is true (used for building cubemaps)
			g_bViewerInSolidSpace = true;

			if ( r_ShowViewerArea.GetInt() )
				Con_NPrintf( 3 + ss_Slot, "(%d), Viewer area: (solid space)", ss_Slot );
		}
		else
		{
			int area = host_state.worldbrush->leafs[leaf].area;
			
			if ( r_ShowViewerArea.GetInt() )
				Con_NPrintf( 3 + ss_Slot, "(%d) Viewer area: %d", ss_Slot, area );

			g_nVisibleAreas = 0;		
			
			if ( pVisData && pVisData->m_bTrimFrustumToPortalCorners && r_ClipAreaFrustums.GetBool() )
			{
				const float flDistToPortalTolerance = 16.0f;
				// If the current view origin is within some perpendicular distance of the exit portal AND within the radius of the portal,
				// don't attempt to optimize the area rect/frustum
				Vector vViewOriginToPortalOrigin = CurrentViewOrigin() - pVisData->m_vPortalOrigin;
				if ( fabsf( vViewOriginToPortalOrigin.Dot( pVisData->m_vPortalForward ) ) > flDistToPortalTolerance ||
					 vViewOriginToPortalOrigin.Length() > pVisData->m_flPortalRadius )
				{
					// invert the rectangle and then grow it to fit the portal
					rect.left = rect.bottom = 1;
					rect.right = rect.top = -1;
					for ( int i = 0; i < 4; ++ i )
					{
						Vector vScreenPos;
						g_EngineRenderer->ClipTransform( pVisData->m_vPortalCorners[ i ], &vScreenPos );

						rect.left   = fpmin( vScreenPos.x, rect.left );
						rect.bottom = fpmin( vScreenPos.y, rect.bottom );
						rect.right  = fpmax( vScreenPos.x, rect.right );
						rect.top    = fpmax( vScreenPos.y, rect.top );					
					}
					rect.left = fpmax( rect.left, -1.0f );
					rect.bottom = fpmax( rect.bottom, -1.0f );
					rect.right = fpmin( rect.right, 1.0f );
					rect.top = fpmin( rect.top, 1.0f );
				}
			}
			R_FlowThroughArea( area, vVisOrigin, &rect, pVisData, pWaterReflectionHeight );
		}
	}
	if ( !g_bViewerInSolidSpace )
	{
		R_SetupVisibleAreaFrustums();
	}
}


bool R_ShouldUseAreaFrustum( int area )
{
	if ( g_AreaCounter[area] == g_GlobalCounter )
		return true;
	else
		return false;
}

const Frustum_t* GetAreaFrustum( int area )
{
	if ( g_AreaCounter[area] == g_GlobalCounter )
		return &g_AreaFrustum[area];
	else
		return &g_Frustum;
}


int GetAllAreaFrustums( Frustum_t **pFrustumList, int listMax )
{
	int count = g_AreaFrustum.Count();
	count = MIN(listMax,count);
	for ( int i = 0; i < count; i++ )
	{
		if ( g_AreaCounter[i] == g_GlobalCounter )
		{
			pFrustumList[i] = &g_AreaFrustum[i];
		}
		else
		{
			pFrustumList[i] = &g_Frustum;
		}
	}
	return count;
}
