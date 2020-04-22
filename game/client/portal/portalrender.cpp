//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//


#include "cbase.h"
#include "portalrender.h"
#include "precache_register.h"
#include "view.h"
#include "c_pixel_visibility.h"
#include "glow_overlay.h"
#include "portal_render_targets.h" //depth doubler
#include "materialsystem/ITexture.h"
#include "toolframework/itoolframework.h"
#include "tier1/keyvalues.h"
#include "view_scene.h"
#include "viewrender.h"
#include "shaderapi/ishaderapi.h"
#include "vprof.h"
#include "toolframework_client.h"
#include "vgui_int.h"
#include "renderparm.h"

#ifdef PORTAL
#include "c_prop_portal.h"
#include "c_portal_player.h"
#endif

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecachePortalDrawingMaterials )
PRECACHE( MATERIAL, "shadertest/wireframe" )
PRECACHE( MATERIAL, "engine/writez_model" )
PRECACHE( MATERIAL, "engine/TranslucentVertexColor" )
PRECACHE_REGISTER_END()

#define TEMP_DISABLE_PORTAL_VIS_QUERY

static ConVar r_forcecheapwater( "r_forcecheapwater", "0", FCVAR_CLIENTDLL | FCVAR_CHEAT, "Force all water to be cheap water, will show old renders if enabled after water has been seen" );


ConVar r_portal_stencil_depth( "r_portal_stencil_depth", "2", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "When using stencil views, this changes how many views within views we see" );
ConVar	r_portal_use_pvs_optimization( "r_portal_use_pvs_optimization", "1", 0, "Enables an optimization that allows portals to be culled when outside of the PVS." );

ConVar r_portal_fastpath( "r_portal_fastpath", "1", 0 );
ConVar r_portal_fastpath_max_ghost_recursion( "r_portal_fastpath_max_ghost_recursion", "2", 0 );
ConVar r_portal_earlyz( "r_portal_earlyz", "1", 0 );
ConVar r_portalscissor( "r_portalscissor", "0", 0 );

extern ConVar portal_draw_ghosting;

//-----------------------------------------------------------------------------
//
// Portal rendering management class
//
//-----------------------------------------------------------------------------
static CPortalRender s_PortalRender;
CPortalRender* g_pPortalRender = &s_PortalRender;

//CUtlVector<PortalRenderableCreationFunction_t> CPortalRender::m_PortalRenderableCreators;
CPortalRenderableCreator_AutoRegister *CPortalRenderableCreator_AutoRegister::s_pRegisteredTypes = NULL;

//-------------------------------------------
//Portal View ID Node helpers
//-------------------------------------------
PortalViewIDNode_t *AllocPortalViewIDNode( int iChildLinkCount, int nPortalIndex, int nTeam, int nParentID )
{
	PortalViewIDNode_t *pNode = new PortalViewIDNode_t; //for now we just new/delete

	// Generate a new view ID that is completely unique based upon which portal we are, and who our parent is.
	// This only works for ~15 levels of recursion, after which, any other new IDs will alias with portals of previous levels because of overflow.
	// This should be ok, since we limit ourselves to 11 levels of recursion.

	if ( nParentID <= VIEW_ID_COUNT )
	{
		// If we're one of the standard view, we should be view 0
		Assert( nParentID == 0 );
		nParentID = 0; // parentID should be 0, but just in case slam it to 0
	}
	else
	{
		nParentID -= VIEW_ID_COUNT; // Bias and scale
		nParentID /= 2; // Collapse to remove skybox IDs
	}
	
	Assert( nTeam != 1 );
	nTeam %= 2; // Get team id into the 0..1 range.  For single player the team id is 0... for multplayer it's 2 or 3 (TEAM_RED or TEAM_BLUE)

	// Figure out the index to the first child of our parentID.
	// Use a full quadtree numbering scheme.  First child node starts at 1.
	int nFirstChild = nParentID * 4 + 1;
	int nPortalID = nTeam * 2 + nPortalIndex; // 0..3 range
	int nChildID = nFirstChild + nPortalID;
	nChildID *= 2; // Space them out so that viewID + 1 is the skybox view
	nChildID += VIEW_ID_COUNT; // Bias back into the VIEW_ID_COUNT range since we've reserved VIEW_ID_COUNT-1 view for primary viewIDs

	pNode->iPrimaryViewID = nChildID;

	CMatRenderContextPtr pRenderContext( materials );	
#ifndef TEMP_DISABLE_PORTAL_VIS_QUERY
	pNode->occlusionQueryHandle = pRenderContext->CreateOcclusionQueryObject();
#endif
	pNode->iOcclusionQueryPixelsRendered = -5;
	pNode->iWindowPixelsAtQueryTime = 0;
	pNode->fScreenFilledByPortalSurfaceLastFrame_Normalized = -1.0f;

	if( iChildLinkCount != 0 )
	{
		pNode->ChildNodes.SetCount( iChildLinkCount );
		memset( pNode->ChildNodes.Base(), NULL, sizeof( PortalViewIDNode_t * ) * iChildLinkCount );
	}

	return pNode;
}

void FreePortalViewIDNode( PortalViewIDNode_t *pNode )
{
	for( int i = pNode->ChildNodes.Count(); --i >= 0; )
	{
		if( pNode->ChildNodes[i] != NULL )
			FreePortalViewIDNode( pNode->ChildNodes[i] );
	}

	CMatRenderContextPtr pRenderContext( materials );
#ifndef TEMP_DISABLE_PORTAL_VIS_QUERY
	pRenderContext->DestroyOcclusionQueryObject( pNode->occlusionQueryHandle );
#endif

	delete pNode; //for now we just new/delete
}

void IncreasePortalViewIDChildLinkCount( PortalViewIDNode_t *pNode )
{
	for( int i = pNode->ChildNodes.Count(); --i >= 0; )
	{
		if( pNode->ChildNodes[i] != NULL )
			IncreasePortalViewIDChildLinkCount( pNode->ChildNodes[i] );
	}
	pNode->ChildNodes.AddToTail( NULL );
}

void RemovePortalViewIDChildLinkIndex( PortalViewIDNode_t *pNode, int iRemoveIndex )
{
	Assert( pNode->ChildNodes.Count() > iRemoveIndex );

	if( pNode->ChildNodes[iRemoveIndex] != NULL )
	{
		FreePortalViewIDNode( pNode->ChildNodes[iRemoveIndex] );
		pNode->ChildNodes[iRemoveIndex] = NULL;
	}

	//I know the current behavior for CUtlVector::FastRemove() is to move the tail into the removed index. But I need that behavior to be true in the future as well so I'm doing it explicitly
	pNode->ChildNodes[iRemoveIndex] = pNode->ChildNodes.Tail();
	pNode->ChildNodes.Remove( pNode->ChildNodes.Count() - 1 );

	for( int i = pNode->ChildNodes.Count(); --i >= 0; )
	{
		if( pNode->ChildNodes[i] )
			RemovePortalViewIDChildLinkIndex( pNode->ChildNodes[i], iRemoveIndex );
	}
}

//-----------------------------------------------------------------------------
//
// Active Portal class 
//
//-----------------------------------------------------------------------------
CPortalRenderable::CPortalRenderable( void ) : 	
	m_bIsPlaybackPortal( false ),
	m_bIsPropPortal( false )
{
	m_matrixThisToLinked.Identity();
	
	//Portal view ID indexing setup
	IncreasePortalViewIDChildLinkCount( &s_PortalRender.m_HeadPortalViewIDNode );
	m_iPortalViewIDNodeIndex = s_PortalRender.m_AllPortals.AddToTail( this );	
}

CPortalRenderable::~CPortalRenderable( void )
{
	int iLast = s_PortalRender.m_AllPortals.Count() - 1;

	//update the soon-to-be-transplanted portal's index
	s_PortalRender.m_AllPortals[iLast]->m_iPortalViewIDNodeIndex = m_iPortalViewIDNodeIndex;

	//I know the current behavior for CUtlVector::FastRemove() is to move the tail into the removed index. But I need that behavior to be true in the future as well so I'm doing it explicitly
	s_PortalRender.m_AllPortals[m_iPortalViewIDNodeIndex] = s_PortalRender.m_AllPortals.Tail();
	s_PortalRender.m_AllPortals.Remove( iLast );
	
	RemovePortalViewIDChildLinkIndex( &s_PortalRender.m_HeadPortalViewIDNode, m_iPortalViewIDNodeIndex ); //does the same transplant operation as above to all portal view id nodes
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CPortalRender::CPortalRender()
: m_MaterialsAccess( m_Materials )
{
	m_iRemainingPortalViewDepth = 1; //let's portals know that they should do "end of the line" kludges to cover up that portals don't go infinitely recursive
	m_iViewRecursionLevel = 0;
	m_pRenderingViewForPortal = NULL;
	m_pRenderingViewExitPortal = NULL;

	m_PortalViewIDNodeChain[0] = &m_HeadPortalViewIDNode;
	m_pCachedPortalQuadMeshData = NULL;
}

CPortalRender::~CPortalRender()
{
	if ( m_pCachedPortalQuadMeshData )
		m_pCachedPortalQuadMeshData->Free();
}

void CPortalRender::LevelInitPreEntity()
{
	// refresh materials - not sure if this needs to be done every level
	m_Materials.m_Wireframe.Init( "shadertest/wireframe", TEXTURE_GROUP_CLIENT_EFFECTS );
	m_Materials.m_WriteZ_Model.Init( "engine/writez_model", TEXTURE_GROUP_CLIENT_EFFECTS );
	m_Materials.m_TranslucentVertexColor.Init( "engine/TranslucentVertexColor", TEXTURE_GROUP_CLIENT_EFFECTS );
	m_Materials.m_PortalDepthDoubler.Init( "models/portals/portal_depthdoubler", TEXTURE_GROUP_CLIENT_EFFECTS );

	m_Materials.m_nDepthDoubleViewMatrixVarCache = 0;
	m_Materials.m_PortalDepthDoubler->FindVarFast( "$alternateviewmatrix", &m_Materials.m_nDepthDoubleViewMatrixVarCache ); // Warm cache
}

void CPortalRender::LevelShutdownPreEntity()
{
	int nCount = m_RecordedPortals.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		delete m_RecordedPortals[i].m_pActivePortal;
	}
	m_RecordedPortals.RemoveAll();
}


int CPortalRender::ShouldForceCheaperWaterLevel() const
{
	if( r_forcecheapwater.GetBool() )
		return 0;

	if ( IsGameConsole() )
	{
		// Force cheap water in portals more than 1 deep
		// and altogether in co-op
//		if ( m_iViewRecursionLevel > 1 || GameRules()->IsMultiplayer() )
		if ( m_iViewRecursionLevel > 1 )
		{
			return 0;
		}
		else
		{
			return 3;
		}
	}

	if( m_iViewRecursionLevel > 0 )
	{
		if( m_iViewRecursionLevel > 2 )
			return 0;

		PortalViewIDNode_t *pPixelVisNode = m_PortalViewIDNodeChain[m_iViewRecursionLevel - 1]->ChildNodes[m_pRenderingViewForPortal->m_iPortalViewIDNodeIndex];
		
		if( pPixelVisNode->fScreenFilledByPortalSurfaceLastFrame_Normalized >= 0.0f )
		{
			if( pPixelVisNode->fScreenFilledByPortalSurfaceLastFrame_Normalized < 0.005f )
				return 0;

			if( pPixelVisNode->fScreenFilledByPortalSurfaceLastFrame_Normalized < 0.02f )
				return 1;

			if( pPixelVisNode->fScreenFilledByPortalSurfaceLastFrame_Normalized < 0.05f )
				return 2;
		}
	}

	return 3;
}

bool CPortalRender::ShouldObeyStencilForClears() const
{
	return (m_iViewRecursionLevel > 0);
}

void CPortalRender::WaterRenderingHandler_PreReflection() const
{
	if( m_iViewRecursionLevel > 0 )
	{
		CMatRenderContextPtr pRenderContext( materials );
		ShaderStencilState_t tempState = m_StencilState;
		tempState.m_bEnable = false;
		pRenderContext->SetStencilState( tempState );
	}
}

void CPortalRender::WaterRenderingHandler_PostReflection() const
{
	if( m_iViewRecursionLevel > 0 )
	{
		CMatRenderContextPtr pRenderContext( materials );
		ShaderStencilState_t tempState = m_StencilState;
		tempState.m_bEnable = true;
		pRenderContext->SetStencilState( tempState );
	}
}

void CPortalRender::WaterRenderingHandler_PreRefraction() const
{
	if( m_iViewRecursionLevel > 0 )
	{
		CMatRenderContextPtr pRenderContext( materials );
		ShaderStencilState_t tempState = m_StencilState;
		tempState.m_bEnable = false;
		pRenderContext->SetStencilState( tempState );
	}
}

void CPortalRender::WaterRenderingHandler_PostRefraction() const
{
	if( m_iViewRecursionLevel > 0 )
	{
		CMatRenderContextPtr pRenderContext( materials );
		ShaderStencilState_t tempState = m_StencilState;
		tempState.m_bEnable = true;
		pRenderContext->SetStencilState( tempState );
	}
}


void Recursive_UpdatePortalPixelVisibility( PortalViewIDNode_t *pNode, IMatRenderContext *pRenderContext )
{
	if( pNode->iWindowPixelsAtQueryTime > 0 )
	{
		if( pNode->iOcclusionQueryPixelsRendered < -1 )
		{
			//First couple queries. We seem to be getting bogus 0's on the first queries sometimes. ignore the results.
			++pNode->iOcclusionQueryPixelsRendered;
			pNode->fScreenFilledByPortalSurfaceLastFrame_Normalized = -1.0f;
		}
		else
		{
			pNode->iOcclusionQueryPixelsRendered = pRenderContext->OcclusionQuery_GetNumPixelsRendered( pNode->occlusionQueryHandle );
			pNode->fScreenFilledByPortalSurfaceLastFrame_Normalized = ((float)pNode->iOcclusionQueryPixelsRendered) / ((float)pNode->iWindowPixelsAtQueryTime);
		}
	}
	else
	{
		pNode->fScreenFilledByPortalSurfaceLastFrame_Normalized = -1.0f;
	}

	pNode->iWindowPixelsAtQueryTime = 0;

	for( int i = pNode->ChildNodes.Count(); --i >= 0; )
	{
		PortalViewIDNode_t *pChildNode = pNode->ChildNodes[i];
		if( pChildNode )
			Recursive_UpdatePortalPixelVisibility( pChildNode, pRenderContext );
	}
}

void CPortalRender::UpdatePortalPixelVisibility( void )
{
#ifndef TEMP_DISABLE_PORTAL_VIS_QUERY
	return;
#endif

	if( m_iViewRecursionLevel != 0 )
		return;

	IMatRenderContext *pRenderContext = materials->GetRenderContext();
	//CMatRenderContextPtr pRenderContext( materials );

	for( int i = m_HeadPortalViewIDNode.ChildNodes.Count(); --i >= 0; )
	{
		PortalViewIDNode_t *pChildNode = m_HeadPortalViewIDNode.ChildNodes[i];
		if( pChildNode )
			Recursive_UpdatePortalPixelVisibility( pChildNode, pRenderContext );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Invalidates pixel visibility data for all portals for this next frame.
//-----------------------------------------------------------------------------
void Recursive_InvalidatePortalPixelVis( PortalViewIDNode_t *pNode )
{
	pNode->fScreenFilledByPortalSurfaceLastFrame_Normalized = -1.0f;
	pNode->iOcclusionQueryPixelsRendered = -5;
	pNode->iWindowPixelsAtQueryTime = 0;
	
	for( int i = pNode->ChildNodes.Count(); --i >= 0; )
	{
		PortalViewIDNode_t *pChildNode = pNode->ChildNodes[i];
		if( pChildNode )
			Recursive_InvalidatePortalPixelVis( pChildNode );
	}
}

ConVar cl_useOldSwapPortalVisibilityCode( "cl_useoldswapportalvisibilitycode", "0" );
//-----------------------------------------------------------------------------
// Purpose: Preserves pixel visibility data when view id's are getting swapped around
//-----------------------------------------------------------------------------
void CPortalRender::EnteredPortal( int nPlayerSlot, CPortalRenderable *pEnteredPortal )
{
	CPortalRenderable *pExitPortal = pEnteredPortal->GetLinkedPortal();
	Assert( pExitPortal != NULL );

	if ( pExitPortal == NULL )
		return;

	if ( cl_useOldSwapPortalVisibilityCode.GetInt() )
	{
		// The following code doesn't seem to function as intended.  It adds an extra portal view id to the head of the exit portal's head and tries to transfer the visibility from
		// the main views to it.  Then it tries to transfer the visibility from the entered portal view to the main view.  Unfortunately, this still doesn't give us valid
		// visibility queries and we get query popping when passing through a portal and is overly complicated.  It also messes up the the nicely ordered viewIDs that reduce query results popping
		// when view ids change (due to a new portal coming into view).
	
		int iNodeLinkCount = m_HeadPortalViewIDNode.ChildNodes.Count();

		PortalViewIDNode_t *pNewHead = m_HeadPortalViewIDNode.ChildNodes[pEnteredPortal->m_iPortalViewIDNodeIndex];
		m_HeadPortalViewIDNode.ChildNodes[pEnteredPortal->m_iPortalViewIDNodeIndex] = NULL;

		//Create a new node that will preserve main's visibility. This new node will be linked to the new head node at the exit portal's index (imagine entering a portal walking backwards)
		C_Prop_Portal *pPropPortal = static_cast<C_Prop_Portal*>( pExitPortal );
		int nPortalIndex = pPropPortal->m_bIsPortal2 ? 1 : 0;
		int nTeamIndex = pPropPortal->GetTeamNumber();
		PortalViewIDNode_t *pExitPortalsNewNode = AllocPortalViewIDNode( iNodeLinkCount, nPortalIndex, nTeamIndex, VIEW_MAIN );
		{
			for( int i = 0; i != iNodeLinkCount; ++i )
			{
				pExitPortalsNewNode->ChildNodes[i] = m_HeadPortalViewIDNode.ChildNodes[i];
				m_HeadPortalViewIDNode.ChildNodes[i] = NULL;
			}

			PixelVisibility_ShiftVisibilityViews( nPlayerSlot, VIEW_MAIN, pExitPortalsNewNode->iPrimaryViewID );
			PixelVisibility_ShiftVisibilityViews( nPlayerSlot, VIEW_3DSKY, pExitPortalsNewNode->iPrimaryViewID + 1 );
		}

		

		if( pNewHead ) //it's possible we entered a portal we couldn't see through
		{
			Assert( pNewHead->ChildNodes.Count() == m_HeadPortalViewIDNode.ChildNodes.Count() );
			Assert( pNewHead->ChildNodes[pExitPortal->m_iPortalViewIDNodeIndex] == NULL ); //seeing out of an exit portal back into itself should be impossible

			for( int i = 0; i != iNodeLinkCount; ++i )
			{
				m_HeadPortalViewIDNode.ChildNodes[i] = pNewHead->ChildNodes[i];
				pNewHead->ChildNodes[i] = NULL; //going to be freeing the node in a minute, don't want to kill transplanted children
			}

			//Since the primary views will always be 0 and 1, we have to shift results instead of replacing the id's
			PixelVisibility_ShiftVisibilityViews( nPlayerSlot, pNewHead->iPrimaryViewID, VIEW_MAIN );
			PixelVisibility_ShiftVisibilityViews( nPlayerSlot, pNewHead->iPrimaryViewID + 1, VIEW_3DSKY );

			FreePortalViewIDNode( pNewHead );
		}

		Assert( m_HeadPortalViewIDNode.ChildNodes[pExitPortal->m_iPortalViewIDNodeIndex] == NULL ); //asserted above in pNewHead code, but call me paranoid
		m_HeadPortalViewIDNode.ChildNodes[pExitPortal->m_iPortalViewIDNodeIndex] = pExitPortalsNewNode;
	}
	else
	{
		// This code is much simpler, does essentially the same thing as the code above, and doesn't have to create a new view.  
		// It still causes some popping, but doesn't create a new view or ID.
		PortalViewIDNode_t *pEnteredView = m_HeadPortalViewIDNode.ChildNodes[ pEnteredPortal->m_iPortalViewIDNodeIndex ];
		if ( pEnteredView )
		{
			C_Prop_Portal *pPropPortal = static_cast<C_Prop_Portal*>( pExitPortal );
			int nToExitPortal = pPropPortal->m_bIsPortal2 ? 2 : -2;
			int nExitPortalID = pEnteredView->iPrimaryViewID + nToExitPortal;

			// Shift queries from the main view to the exit portal view... which is just 2 more or less than the current portal view
			PixelVisibility_ShiftVisibilityViews( nPlayerSlot, VIEW_MAIN, nExitPortalID );
			PixelVisibility_ShiftVisibilityViews( nPlayerSlot, VIEW_3DSKY, nExitPortalID + 1 );

			// Shift visibility from the entered view to the main view
			PixelVisibility_ShiftVisibilityViews( nPlayerSlot, pEnteredView->iPrimaryViewID, VIEW_MAIN );
			PixelVisibility_ShiftVisibilityViews( nPlayerSlot, pEnteredView->iPrimaryViewID + 1, VIEW_3DSKY );
		}
	}

	//Because pixel visibility is based off of *last* frame's visibility. We can get cases where a certain portal
	//wasn't visible last frame, but is takes up most of the screen this frame.
	//Set all portal pixel visibility to unknown visibility.
	for( int i = m_HeadPortalViewIDNode.ChildNodes.Count(); --i >= 0; )
	{
		PortalViewIDNode_t *pChildNode = m_HeadPortalViewIDNode.ChildNodes[i];
		if( pChildNode )
			Recursive_InvalidatePortalPixelVis( pChildNode );
	}
}

static inline int ComputeStencilRefValue( int nViewRecursionLevel, int nPortalIdx, int nParentPortalIdx )
{
	Assert( nViewRecursionLevel < 2 );
	if ( nViewRecursionLevel > 0 )
	{
		return ( ( ( 1 << nPortalIdx ) & 0xF ) << 4 ) | ( ( 1 << nParentPortalIdx ) & 0xF );
	}
	return ( 1 << nPortalIdx ) & 0xF;
}

static inline int ComputeStencilRestoreTestMask( int nViewRecursionLevel, int nPortalIdx )
{
	Assert( nViewRecursionLevel < 2 );

	return ( ( 1 << nPortalIdx ) & 0xF ) << ( nViewRecursionLevel * 4 );
}

ConVar r_portalstencildisable( "r_portalstencildisable", "0" );

//-----------------------------------------------------------------------------------------------------------------------------------
void CPortalRender::DrawPortalGhostLocations( IMatRenderContext *pRenderContext, IMesh *pPortalQuadMesh, const GhostPortalRenderInfo_t *pGhostPortalRenderInfos, int nPortalCount ) const
{
	VPROF_BUDGET( "PortalGhosts", "PortalGhosts" );

	if( !ToolsEnabled() && 
		portal_draw_ghosting.GetBool() &&
		( r_portal_fastpath_max_ghost_recursion.GetInt() > m_iViewRecursionLevel ) )
	{
		if ( nPortalCount == 0 )
		{
			return;
		}

		pRenderContext->BeginPIXEvent( PIX_VALVE_ORANGE, "Portal Ghosts" );

		const CPortalRenderable *pExitView = GetCurrentViewExitPortal();

		pRenderContext->MatrixMode( MATERIAL_MODEL ); //just in case
		pRenderContext->PushMatrix();
		pRenderContext->LoadIdentity();

		for ( int i = 0; i < nPortalCount; i++ )
		{
			if ( pGhostPortalRenderInfos[i].m_pPortal == pExitView )
			{
				continue;
			}

			pRenderContext->Bind( pGhostPortalRenderInfos[i].m_pGhostMaterial, pGhostPortalRenderInfos[i].m_pPortal->GetClientRenderable() );
			pPortalQuadMesh->Draw( pGhostPortalRenderInfos[i].m_nGhostPortalQuadIndex * 6, 6 );
		}

		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PopMatrix();

		pRenderContext->EndPIXEvent();
	}
}

#ifdef _PS3
void CPortalRender::ReloadZcullMemory()
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->ReloadZcullMemory( m_StencilState.m_nReferenceValue );
	int nOldWriteMask = m_StencilState.m_nWriteMask;
	m_StencilState.m_nWriteMask = 0;
	pRenderContext->SetStencilState( m_StencilState );
	m_StencilState.m_nWriteMask = nOldWriteMask;
}
#endif // _PS3




void CPortalRender::DrawEarlyZPortals( CViewRender *pViewRender )
{	 
	VPROF_BUDGET( "CPortalRender::DrawEarlyZPortals", "DrawEarlyZPortals" );

	if ( !r_portal_earlyz.GetBool() || !r_portal_fastpath.GetBool() || ( g_pMaterialSystem->GetThreadMode() == MATERIAL_SINGLE_THREADED ) )
	{
		return;
	}

	int iDrawFlags = pViewRender->GetDrawFlags();

	if ( (iDrawFlags & DF_RENDER_REFLECTION) != 0 )
		return;

	if ( ((iDrawFlags & DF_CLIP_Z) != 0) && ((iDrawFlags & DF_CLIP_BELOW) == 0) ) //clipping above the water height
		return;

	CMatRenderContextPtr pRenderContext( materials );

	int iNumRenderablePortals = m_ActivePortals.Count();

	if ( iNumRenderablePortals == 0 )
	{
		return;
	}

	if ( m_iViewRecursionLevel > 0 )
	{
		return;
	}

	IMesh *pPortalQuadMesh = NULL;

	// Make a vertex buffer with all portal quads
	if ( m_pCachedPortalQuadMeshData ) // Free it from the last time
	{
		m_pCachedPortalQuadMeshData->Free();
	}
	m_pCachedPortalQuadMeshData = NULL;
	pPortalQuadMesh = CPortalRenderable_FlatBasic::CreateMeshForPortals( pRenderContext, m_ActivePortals.Count(), m_ActivePortals.Base(), m_clampedPortalMeshRenderInfos );
	m_portalIsOpening.SetCount( iNumRenderablePortals );
	for ( int i = 0; i < m_portalIsOpening.Count(); i++ )
	{
		m_portalIsOpening[i] = m_ActivePortals[i]->IsPropPortal() && static_cast<C_Prop_Portal *>( m_ActivePortals[i] )->IsPortalOpening();
	}

	CUtlVector< CPortalRenderable* > actualActivePortals( 0, iNumRenderablePortals );
	CUtlVector< int > actualActivePortalQuadVBIndex( 0, iNumRenderablePortals );

	if ( true ) //( ToolsEnabled() )
	{
		// This loop is necessary because tools can suppress rendering without telling the portal system
		for ( int i = 0; i < iNumRenderablePortals; ++i )
		{
			CPortalRenderable *pPortalRenderable = m_ActivePortals[i];
			C_BaseEntity *pPairedEntity = pPortalRenderable->PortalRenderable_GetPairedEntity();
			bool bIsVisible = (pPairedEntity == NULL) || (pPairedEntity->IsVisible() && pPairedEntity->ShouldDraw()); //either unknown visibility or definitely visible.

			// If the portal is associated with an entity, check to see if that entity is even in the PVS before rendering the portal
			if ( pPairedEntity != NULL && r_portal_use_pvs_optimization.GetBool() )
			{
				IClientRenderable *pCR = pPairedEntity->GetClientRenderable();
				bool bIsRenderable = g_pClientLeafSystem->IsRenderableInPVS( pCR );
				bIsVisible &= bIsRenderable;
			}

			if ( !pPortalRenderable->m_bIsPlaybackPortal )
			{
				if ( !bIsVisible )
				{
					//can't see through the portal, free up it's view id node for use elsewhere
					if( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pPortalRenderable->m_iPortalViewIDNodeIndex] != NULL )
					{
						FreePortalViewIDNode( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pPortalRenderable->m_iPortalViewIDNodeIndex] );
						m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pPortalRenderable->m_iPortalViewIDNodeIndex] = NULL;
					}

					continue;
				}
			}

			if ( m_portalIsOpening[i] )
			{
				// Don't draw, otherwise the refract effect will have no pixels to pull from
				continue;
			}

			actualActivePortals.AddToTail( m_ActivePortals[i] );
			actualActivePortalQuadVBIndex.AddToTail( i );
		}
	}
	else
	{
		// Waste some time on the consoles to make the code below work the same
		actualActivePortals.AddVectorToTail( m_ActivePortals );
		for ( int i = 0; i < iNumRenderablePortals; i++ )
		{
			actualActivePortalQuadVBIndex.AddToTail( i );
		}
	}

	iNumRenderablePortals = actualActivePortals.Count();
	if( iNumRenderablePortals == 0 )
	{
		if ( pPortalQuadMesh )
		{
			pPortalQuadMesh->MarkAsDrawn();
		}
		return;
	}

	if( m_RecursiveViewComplexFrustums[m_iViewRecursionLevel].Count() == 0 )
	{
		//nothing in the complex frustum from the current view, copy the standard frustum in
		m_RecursiveViewComplexFrustums[m_iViewRecursionLevel].AddMultipleToTail( FRUSTUM_NUMPLANES, pViewRender->GetFrustum() );
	}

	//step 1, write out the stencil values (and colors if you want, but really not necessary)
	{
		pRenderContext->BeginPIXEvent( PIX_VALVE_ORANGE, "Portal_EarlyZ" );

		for ( int i = 0; i < actualActivePortals.Count(); i++ )
		{
			CPortalRenderable *pCurrentPortal = actualActivePortals[i];

			IMaterial *pMat = pCurrentPortal->IsPropPortal() ? C_Prop_Portal::m_Materials.m_Portal_Stencil_Hole : C_Portal_Base2D::m_Materials.m_Portal_Stencil_Hole;
			pRenderContext->Bind( pMat, assert_cast< CPortalRenderable_FlatBasic* >( pCurrentPortal )->GetClientRenderable() );

			int nStartIndex = actualActivePortalQuadVBIndex[i] * 6;
			int nIndexCount = 6;
			pPortalQuadMesh->Draw( nStartIndex, nIndexCount );
			if ( ( m_iViewRecursionLevel == 0 ) && ( m_clampedPortalMeshRenderInfos[ actualActivePortalQuadVBIndex[i] ].nStartIndex >= 0 ) )
			{
				// Draw near plane cap
				nStartIndex = m_clampedPortalMeshRenderInfos[ actualActivePortalQuadVBIndex[i] ].nStartIndex;
				nIndexCount = m_clampedPortalMeshRenderInfos[ actualActivePortalQuadVBIndex[i] ].nIndexCount;
				pPortalQuadMesh->Draw( nStartIndex, nIndexCount );
			}
		}

		pRenderContext->EndPIXEvent();
	}

	if ( pPortalQuadMesh )
	{
		pPortalQuadMesh->MarkAsDrawn();
	}
}



//-----------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------
bool CPortalRender::DrawPortalsUsingStencils( CViewRender *pViewRender )
{	  
	VPROF_BUDGET( "CPortalRender::DrawPortalsUsingStencils", "DrawPortalsUsingStencils" );

	if ( !r_portal_fastpath.GetBool() || 
		 ( g_pMaterialSystem->GetThreadMode() == MATERIAL_SINGLE_THREADED ) )	// only QMS supports the VB restores I'm doing right now
	{
		return DrawPortalsUsingStencils_Old( pViewRender );
	}

	int iDrawFlags = pViewRender->GetDrawFlags();

	if ( (iDrawFlags & DF_RENDER_REFLECTION) != 0 )
		return false;

	if ( ((iDrawFlags & DF_CLIP_Z) != 0) && ((iDrawFlags & DF_CLIP_BELOW) == 0) ) //clipping above the water height
		return false;

	if ( m_AllPortals.Count() == 0 )
	{
		return false;
	}

	CMatRenderContextPtr pRenderContext( materials );
	IMesh *pPortalQuadMesh = NULL;

	if ( m_iViewRecursionLevel == 0 )
	{
		m_portalGhostRenderInfos.RemoveAll();
		C_Prop_Portal::BuildPortalGhostRenderInfo( m_AllPortals, m_portalGhostRenderInfos );
	}

	// Make a vertex buffer with all portal quads
	if ( m_iViewRecursionLevel == 0 )
	{
		if ( m_pCachedPortalQuadMeshData ) // Free it from the last time
		{
			m_pCachedPortalQuadMeshData->Free();
		}

		m_pCachedPortalQuadMeshData = NULL;
		pPortalQuadMesh = CPortalRenderable_FlatBasic::CreateMeshForPortals( pRenderContext, m_AllPortals.Count(), m_AllPortals.Base(), m_clampedPortalMeshRenderInfos );
		
		m_pCachedPortalQuadMeshData = pPortalQuadMesh->GetCachedPerFrameMeshData();
		Assert( m_pCachedPortalQuadMeshData );
		m_portalQuadMeshVertexFmt = pPortalQuadMesh->GetVertexFormat();

		m_portalIsOpening.SetCount( m_AllPortals.Count() );
		for ( int i = 0; i < m_portalIsOpening.Count(); i++ )
		{
			m_portalIsOpening[i] = m_AllPortals[i]->IsPropPortal() && static_cast<C_Prop_Portal *>( m_AllPortals[i] )->IsPortalOpening();
		}

	}

#ifdef PORTAL
	{
		if ( pPortalQuadMesh == NULL )
		{
			Assert( m_pCachedPortalQuadMeshData );
			pPortalQuadMesh = pRenderContext->GetDynamicMeshEx( m_portalQuadMeshVertexFmt );
			pPortalQuadMesh->ReconstructFromCachedPerFrameMeshData( m_pCachedPortalQuadMeshData );
		}
		DrawPortalGhostLocations( pRenderContext, pPortalQuadMesh, m_portalGhostRenderInfos.Base(), m_portalGhostRenderInfos.Count() );
	}
#endif

	if ( m_ActivePortals.Count() == 0 )
	{
		if ( pPortalQuadMesh )
		{
			pPortalQuadMesh->MarkAsDrawn();
		}
		return false;
	}
	
	CUtlVector< CPortalRenderable* > actualActivePortals( 0, m_ActivePortals.Count() );
	CUtlVector< int > actualActivePortalQuadVBIndex( 0, m_ActivePortals.Count() );

	{
		// This loop is necessary because tools can suppress rendering without telling the portal system
		for ( int i = 0; i < m_ActivePortals.Count(); ++i )
		{
			CPortalRenderable *pPortalRenderable = m_ActivePortals[i];
			C_BaseEntity *pPairedEntity = pPortalRenderable->PortalRenderable_GetPairedEntity();
			bool bIsVisible = (pPairedEntity == NULL) || (pPairedEntity->IsVisible() && pPairedEntity->ShouldDraw()); //either unknown visibility or definitely visible.

			// If the portal is associated with an entity, check to see if that entity is even in the PVS before rendering the portal
			if ( pPairedEntity != NULL && r_portal_use_pvs_optimization.GetBool() )
			{
				IClientRenderable *pCR = pPairedEntity->GetClientRenderable();
				bool bIsRenderable = g_pClientLeafSystem->IsRenderableInPVS( pCR );
				bIsVisible &= bIsRenderable;
			}

			if ( !pPortalRenderable->m_bIsPlaybackPortal )
			{
				if ( !bIsVisible )
				{
					//can't see through the portal, free up it's view id node for use elsewhere
					if( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pPortalRenderable->m_iPortalViewIDNodeIndex] != NULL )
					{
						FreePortalViewIDNode( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pPortalRenderable->m_iPortalViewIDNodeIndex] );
						m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pPortalRenderable->m_iPortalViewIDNodeIndex] = NULL;
					}

					continue;
				}
			}

			actualActivePortals.AddToTail( m_ActivePortals[i] );
			actualActivePortalQuadVBIndex.AddToTail( m_ActivePortals[i]->m_iPortalViewIDNodeIndex );
		}
	}


	if( actualActivePortals.Count() == 0 )
	{
		if ( pPortalQuadMesh )
		{
			pPortalQuadMesh->MarkAsDrawn();
		}
		return false;
	}

	const int iMaxDepth = MIN( r_portal_stencil_depth.GetInt(), MIN( MAX_PORTAL_RECURSIVE_VIEWS, (1 << materials->StencilBufferBits() ) ) - 1 );

	if ( pPortalQuadMesh == NULL )
	{
		Assert( m_pCachedPortalQuadMeshData );
		pPortalQuadMesh = pRenderContext->GetDynamicMeshEx( m_portalQuadMeshVertexFmt );
		pPortalQuadMesh->ReconstructFromCachedPerFrameMeshData( m_pCachedPortalQuadMeshData );
	}

	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_PORTAL_RECURSION_DEPTH, m_iViewRecursionLevel );

	if( m_iViewRecursionLevel >= iMaxDepth ) //can't support any more views	
	{
		if ( m_stencilValueStack.Count() > 0 )
		{
			int nDummy;
			m_stencilValueStack.Pop( nDummy );
		}
		if ( m_parentPortalIdStack.Count() > 0 )
		{
			int nDummy;
			m_parentPortalIdStack.Pop( nDummy );
		}

		m_iRemainingPortalViewDepth = 0; //special case handler for max depth 0 cases
		pRenderContext->BeginPIXEvent( PIX_VALVE_ORANGE, "Portal_maxrecursion_reached" );

		RenderPortalEffects( pRenderContext, pPortalQuadMesh, actualActivePortals, actualActivePortalQuadVBIndex );

		pRenderContext->EndPIXEvent();
		if ( pPortalQuadMesh )
		{
			pPortalQuadMesh->MarkAsDrawn();
		}
		return false;
	}

	m_iRemainingPortalViewDepth = (iMaxDepth - m_iViewRecursionLevel) - 1;

	pRenderContext->Flush( true ); //to prevent screwing up the last opaque object

	const CViewSetup *pViewSetup = pViewRender->GetViewSetup();
	m_RecursiveViewSetups[m_iViewRecursionLevel] = *pViewSetup;

	CViewSetup ViewBackup;// = *pViewSetup; //backup the view, we'll need to restore it
	memcpy( &ViewBackup, pViewSetup, sizeof( CViewSetup ) );

	Vector ptCameraOrigin = pViewSetup->origin;
	Vector vCameraForward;
	AngleVectors( pViewSetup->angles, &vCameraForward, NULL, NULL );

	int iX, iY, iWidth, iHeight;
	pRenderContext->GetViewport( iX, iY, iWidth, iHeight );
#ifndef TEMP_DISABLE_PORTAL_VIS_QUERY
	int iScreenPixelCount = iWidth * iHeight;
#endif

	bool bRebuildDrawListsWhenDone = false;


	int iParentLevelStencilReferenceValue = m_iViewRecursionLevel;

	if( m_iViewRecursionLevel == 0 ) //first entry into the stencil drawing
	{
		m_StencilState.m_bEnable = true;
		m_StencilState.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
		m_StencilState.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
		m_StencilState.m_FailOp = SHADER_STENCILOP_KEEP;
		m_StencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
		m_StencilState.m_nTestMask = 0xFF;
		m_StencilState.m_nWriteMask = 0xFF;
		m_StencilState.m_nReferenceValue = 0;
		pRenderContext->SetStencilState( m_StencilState );
		pRenderContext->PerformFullScreenStencilOperation();	// Clear stencil. Is this really necessary?
		pPortalQuadMesh = NULL;	// NULL it out so we know to restore our dynamic mesh later

        m_RecursiveViewComplexFrustums[0].RemoveAll(); //clear any garbage leftover in the complex frustums from last frame

		// Properly prime our stacks of stuff
		m_stencilValueStack.Push( m_StencilState.m_nReferenceValue );
		m_parentPortalIdStack.Push( -1 );
	}

	if( m_RecursiveViewComplexFrustums[m_iViewRecursionLevel].Count() == 0 )
	{
		//nothing in the complex frustum from the current view, copy the standard frustum in
		m_RecursiveViewComplexFrustums[m_iViewRecursionLevel].AddMultipleToTail( FRUSTUM_NUMPLANES, pViewRender->GetFrustum() );
	}

	CUtlVector< CPortalRenderable* > portalRenderablesToDraw( 0, actualActivePortals.Count() );
	CUtlVector< int > portalsRenderablesToDrawVBIndex( 0, actualActivePortals.Count() );

	for( int i = 0; i < actualActivePortals.Count(); ++i )
	{
		CPortalRenderable *pCurrentPortal = actualActivePortals[i];

		m_RecursiveViewComplexFrustums[m_iViewRecursionLevel + 1].RemoveAll(); //clear any previously stored complex frustum
		
		if( (pCurrentPortal->GetLinkedPortal() == NULL) ||
			(pCurrentPortal == m_pRenderingViewExitPortal) ||
			(pCurrentPortal->ShouldUpdatePortalView_BasedOnView( *pViewSetup, m_RecursiveViewComplexFrustums[m_iViewRecursionLevel] ) == false) )
		{
			//can't see through the portal, free up it's view id node for use elsewhere
			if( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] != NULL )
			{
				FreePortalViewIDNode( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] );
				m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] = NULL;
			}
			continue;
		}

		Assert( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes.Count() > pCurrentPortal->m_iPortalViewIDNodeIndex );

		if( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] == NULL )
		{
			C_Prop_Portal *pPropPortal = static_cast<C_Prop_Portal*>( pCurrentPortal );
			int nPortalIndex = pPropPortal->m_bIsPortal2 ? 1 : 0;
			int nTeamIndex = pPropPortal->GetTeamNumber();
			m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] = AllocPortalViewIDNode( m_HeadPortalViewIDNode.ChildNodes.Count(), nPortalIndex, nTeamIndex, CurrentViewID() );
		}

		//		PortalViewIDNode_t *pCurrentPortalViewNode = m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex];

		portalRenderablesToDraw.AddToTail( pCurrentPortal );
		portalsRenderablesToDrawVBIndex.AddToTail( actualActivePortalQuadVBIndex[ i ] );
	}

	if ( portalRenderablesToDraw.Count() > 4 )
	{
		Warning( "More than 4 visible portals (recursion level %d)\n", m_iViewRecursionLevel );
	}

	// Step 0, Allow for special effects to happen before cutting a hole
	{
		pRenderContext->BeginPIXEvent( PIX_VALVE_ORANGE, "Portal_Step0_prestencil_effects" );

		m_StencilState.m_bEnable = true;
		m_StencilState.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
		m_StencilState.m_PassOp = SHADER_STENCILOP_KEEP;
		m_StencilState.m_FailOp = SHADER_STENCILOP_KEEP;
		m_StencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
		m_StencilState.m_nTestMask = 0xFF;
		m_StencilState.m_nWriteMask = 0xFF;
		m_StencilState.m_nReferenceValue = iParentLevelStencilReferenceValue;	// FIXME
		pRenderContext->SetStencilState( m_StencilState );

		if ( pPortalQuadMesh == NULL )
		{
			Assert( m_pCachedPortalQuadMeshData );
			pPortalQuadMesh = pRenderContext->GetDynamicMeshEx( m_portalQuadMeshVertexFmt );
			pPortalQuadMesh->ReconstructFromCachedPerFrameMeshData( m_pCachedPortalQuadMeshData );
		}

		for ( int i = 0; i < portalRenderablesToDraw.Count(); i++ )
		{
			bool bRefractTextureInitialized = false;

			if ( m_portalIsOpening[ portalsRenderablesToDrawVBIndex[i] ] )
			{
				CPortalRenderable *pCurrentPortal = portalRenderablesToDraw[i];
				//pCurrentPortal->DrawPreStencilMask( pRenderContext );

				pRenderContext->Bind( C_Prop_Portal::m_Materials.m_Portal_Refract, assert_cast< CPortalRenderable_FlatBasic* >( pCurrentPortal )->GetClientRenderable() );

				if ( !bRefractTextureInitialized )
				{
					// This can depend on the Bind command above, so keep this after!
					UpdateFrontBufferTexturesForMaterial( C_Prop_Portal::m_Materials.m_Portal_Refract );
					bRefractTextureInitialized = true;
				}

				int nStartIndex = portalsRenderablesToDrawVBIndex[i] * 6;
				int nIndexCount = 6;
				pPortalQuadMesh->Draw( nStartIndex, nIndexCount );
			}
		}

		pRenderContext->EndPIXEvent();
	}

	int nParentPortalIdx = -1;
	m_parentPortalIdStack.Pop( nParentPortalIdx );

	//step 1, write out the stencil values (and colors if you want, but really not necessary)
	{
		pRenderContext->BeginPIXEvent( PIX_VALVE_ORANGE, "Portal_Step1_writestencil" );
		if ( m_iViewRecursionLevel == 0 )
		{
			m_StencilState.m_bEnable = true;
			m_StencilState.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
			m_StencilState.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
			m_StencilState.m_FailOp = SHADER_STENCILOP_KEEP;
			m_StencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
			m_StencilState.m_nTestMask = 0xFF;
			m_StencilState.m_nWriteMask = 0xFF;
		}
		else
		{
			Assert( m_iViewRecursionLevel == 1 );	// We only support two-level recursion
			m_StencilState.m_bEnable = true;
			m_StencilState.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
			m_StencilState.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
			m_StencilState.m_FailOp = SHADER_STENCILOP_KEEP;
			m_StencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
			m_StencilState.m_nTestMask = 0x0F;
			m_StencilState.m_nWriteMask = 0xF0;			
		}

		if ( pPortalQuadMesh == NULL )
		{
			Assert( m_pCachedPortalQuadMeshData );
			pPortalQuadMesh = pRenderContext->GetDynamicMeshEx( m_portalQuadMeshVertexFmt );
			pPortalQuadMesh->ReconstructFromCachedPerFrameMeshData( m_pCachedPortalQuadMeshData );
		}

		for ( int i = 0; i < portalRenderablesToDraw.Count(); i++ )
		{
			CPortalRenderable *pCurrentPortal = portalRenderablesToDraw[i];

			m_StencilState.m_nReferenceValue = ComputeStencilRefValue( m_iViewRecursionLevel, i, nParentPortalIdx );
			pRenderContext->SetStencilState( m_StencilState );
			
			IMaterial *pMat = pCurrentPortal->IsPropPortal() ? C_Prop_Portal::m_Materials.m_Portal_Stencil_Hole : C_Portal_Base2D::m_Materials.m_Portal_Stencil_Hole;
			pRenderContext->Bind( pMat, assert_cast< CPortalRenderable_FlatBasic* >( pCurrentPortal )->GetClientRenderable() );
			
			int nStartIndex = portalsRenderablesToDrawVBIndex[i] * 6;
			int nIndexCount = 6;
			pPortalQuadMesh->Draw( nStartIndex, nIndexCount );
			if ( ( m_iViewRecursionLevel == 0 ) && ( m_clampedPortalMeshRenderInfos[ portalsRenderablesToDrawVBIndex[i] ].nStartIndex >= 0 ) )
			{
				// Draw near plane cap
				nStartIndex = m_clampedPortalMeshRenderInfos[ portalsRenderablesToDrawVBIndex[i] ].nStartIndex;
				nIndexCount = m_clampedPortalMeshRenderInfos[ portalsRenderablesToDrawVBIndex[i] ].nIndexCount;
				pRenderContext->OverrideDepthEnable( true, true, false );
				pPortalQuadMesh->Draw( nStartIndex, nIndexCount );
				pRenderContext->OverrideDepthEnable( false, true, true );
			}
		}

		pRenderContext->EndPIXEvent();
	}

	// Compute inverse view-projection matrix
	VMatrix matView;
	VMatrix matProj;
	pRenderContext->GetMatrix( MATERIAL_VIEW, &matView );
	pRenderContext->GetMatrix( MATERIAL_PROJECTION, &matProj );

	VMatrix matViewProj;
	MatrixMultiply( matProj, matView, matViewProj );
	float flViewportX, flViewportY, flViewportWidth, flViewportHeight;
	{
		int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
		pRenderContext->GetViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );
		flViewportX = float( nViewportX );
		flViewportY = float( nViewportY );
		flViewportWidth = 0.5f * float( nViewportWidth );
		flViewportHeight = 0.5f * float( nViewportHeight );
	}

	//step 3, fill in stencil views (remember that in multiple depth situations that any subportals will run through this function again before this section completes, thereby screwing with stencil settings)
	for ( int i = 0; i < portalRenderablesToDraw.Count(); i++ )
	{
		CPortalRenderable *pCurrentPortal = portalRenderablesToDraw[i];

		// Hackity hack?
		m_RecursiveViewComplexFrustums[m_iViewRecursionLevel + 1].RemoveAll(); //clear any previously stored complex frustum

		m_StencilState.m_bEnable = true;
		m_StencilState.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
		m_StencilState.m_PassOp = SHADER_STENCILOP_KEEP;
		m_StencilState.m_FailOp = SHADER_STENCILOP_KEEP;
		m_StencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
		m_StencilState.m_nTestMask = 0xFF;
		m_StencilState.m_nWriteMask = 0xFF;
		m_StencilState.m_nReferenceValue = ComputeStencilRefValue( m_iViewRecursionLevel, i, nParentPortalIdx );

		pRenderContext->SetStencilState( m_StencilState );
		m_stencilValueStack.Push( m_StencilState.m_nReferenceValue );
		m_parentPortalIdStack.Push( i );

		bool bPushedScissor = false;
		if ( r_portalscissor.GetBool() )
		{
			Vector4D clipSpacePortalCorners[4];
			float flMinX = FLT_MAX;
			float flMinY = FLT_MAX;
			float flMaxX = -FLT_MAX;
			float flMaxY = -FLT_MAX;

			// Compute portal positions in clip space
			if ( assert_cast< CPortalRenderable_FlatBasic* >( pCurrentPortal )->ComputeClipSpacePortalCorners( clipSpacePortalCorners, matViewProj ) )
			{
				// The portal didn't clip the near plane, continue
				for ( int i = 0; i < 4; i++ )
				{
					float flX = clipSpacePortalCorners[i].x / clipSpacePortalCorners[i].w;
					float flY = -clipSpacePortalCorners[i].y / clipSpacePortalCorners[i].w;
					flMinX = MIN( flX, flMinX );
					flMinY = MIN( flY, flMinY );
					flMaxX = MAX( flX, flMaxX );
					flMaxY = MAX( flY, flMaxY );
				}

				flMinX = clamp( flMinX, -1.0f, 1.0f );
				flMaxX = clamp( flMaxX, -1.0f, 1.0f );
				flMinY = clamp( flMinY, -1.0f, 1.0f );
				flMaxY = clamp( flMaxY, -1.0f, 1.0f );

				flMinX = flViewportWidth * flMinX + flViewportWidth + flViewportX;
				flMaxX = flViewportWidth * flMaxX + flViewportWidth + flViewportX;
				flMinY = flViewportHeight * flMinY + flViewportHeight + flViewportY;
				flMaxY = flViewportHeight * flMaxY + flViewportHeight + flViewportY;
				pRenderContext->PushScissorRect( int( flMinX ), int( flMinY ), int( flMaxX ), int( flMaxY ) );
				bPushedScissor = true;
			}
		}

		//step 2, clear the depth buffer in stencil areas so we can render a new scene to them
		{
			// TODO: We could render the portal quads instead of a fullscreen quad to save fill.
			pRenderContext->BeginPIXEvent( PIX_VALVE_ORANGE, "Portal_Step2_clear_portal_depth" );
			pRenderContext->ClearBuffersObeyStencil( false, true );
			pPortalQuadMesh = NULL;	// NULL it out so we know to restore our dynamic mesh later
			pRenderContext->EndPIXEvent();
		}

#ifdef _PS3
		// step 2.5
		ReloadZcullMemory();
		pPortalQuadMesh = NULL;	// NULL it out so we know to restore our dynamic mesh later
#endif // _PS3

		{
			pRenderContext->BeginPIXEvent( PIX_VALVE_ORANGE, "Portal_Step3_recursive_draw" );
			VPROF_BUDGET( "Portal_FillInStencilViews", "Portal_FillInStencilViews" );
			bRebuildDrawListsWhenDone = true;

			MaterialFogMode_t fogModeBackup = pRenderContext->GetFogMode();
			unsigned char fogColorBackup[4];
			pRenderContext->GetFogColor( fogColorBackup );
			float fFogStartBackup, fFogEndBackup, fFogZBackup;
			pRenderContext->GetFogDistances( &fFogStartBackup, &fFogEndBackup, &fFogZBackup );
			CGlowOverlay::BackupSkyOverlayData( m_iViewRecursionLevel );

			Assert( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes.Count() > pCurrentPortal->m_iPortalViewIDNodeIndex );

			m_PortalViewIDNodeChain[m_iViewRecursionLevel + 1] = m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex];

			pCurrentPortal->RenderPortalViewToBackBuffer( pViewRender, *pViewSetup );
			pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_PORTAL_RECURSION_DEPTH, m_iViewRecursionLevel );	// restore portal recursion depth param

			m_PortalViewIDNodeChain[m_iViewRecursionLevel + 1] = NULL;

			// This value is incremented to signal that we need to re-copy the refract texture 
			// because we've finished rendering a child portal and its translucent pass, and we're about
			// to render the parent portal's translucent pass or another portal
			++ g_nCurrentPortalRender;

			CGlowOverlay::RestoreSkyOverlayData( m_iViewRecursionLevel );
			memcpy( (void *)pViewSetup, &ViewBackup, sizeof( CViewSetup ) );
			pViewRender->m_pActiveRenderer->EnableWorldFog();

			pRenderContext->FogMode( fogModeBackup );
			pRenderContext->FogColor3ubv( fogColorBackup );
			pRenderContext->FogStart( fFogStartBackup );
			pRenderContext->FogEnd( fFogEndBackup );
			pRenderContext->SetFogZ( fFogZBackup );

			pRenderContext->EndPIXEvent();
		}

		//step 4, patch up the fact that we just made a hole in the wall because it's not *really* a hole at all
		//step 5, restore the stencil mask to the parent level
		{
			// We restore it right away instead of after opening all holes for 360 perf. Apparently the 360 runs the shaders when Z passes even if stencil fails later.

			pRenderContext->BeginPIXEvent( PIX_VALVE_ORANGE, "Portal_Step4_restore_depthstencil" );

			m_StencilState.m_bEnable = true;
			m_StencilState.m_CompareFunc = SHADER_STENCILFUNC_NOTEQUAL;
			m_StencilState.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
			m_StencilState.m_FailOp = SHADER_STENCILOP_KEEP;
			m_StencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
			m_StencilState.m_nTestMask = ComputeStencilRestoreTestMask( m_iViewRecursionLevel, i );
			m_StencilState.m_nWriteMask = m_StencilState.m_nTestMask;
			m_StencilState.m_nReferenceValue = 0;
			pRenderContext->SetStencilState( m_StencilState );

			if ( pPortalQuadMesh == NULL )
			{
				Assert( m_pCachedPortalQuadMeshData );
				pPortalQuadMesh = pRenderContext->GetDynamicMeshEx( m_portalQuadMeshVertexFmt );
				pPortalQuadMesh->ReconstructFromCachedPerFrameMeshData( m_pCachedPortalQuadMeshData );
			}

			pRenderContext->Bind( (IMaterial*)( const IMaterial *)( m_MaterialsAccess.m_WriteZ_Model ), static_cast< CPortalRenderable_FlatBasic* >( pCurrentPortal )->GetClientRenderable() );

			int nStartIndex = portalsRenderablesToDrawVBIndex[i] * 6;
			int nIndexCount = 6;
			pRenderContext->OverrideDepthEnable( true, true, false );	// Force depth writes on, but depth test off
			pPortalQuadMesh->Draw( nStartIndex, nIndexCount );
			if ( ( m_iViewRecursionLevel == 0 ) && ( m_clampedPortalMeshRenderInfos[ portalsRenderablesToDrawVBIndex[i] ].nStartIndex >= 0 ) )
			{
				// Draw near plane cap
				nStartIndex = m_clampedPortalMeshRenderInfos[ portalsRenderablesToDrawVBIndex[i] ].nStartIndex;
				nIndexCount = m_clampedPortalMeshRenderInfos[ portalsRenderablesToDrawVBIndex[i] ].nIndexCount;
				pPortalQuadMesh->Draw( nStartIndex, nIndexCount );
			}
			pRenderContext->OverrideDepthEnable( false, true, true );

			pRenderContext->EndPIXEvent();
		}

#ifdef _PS3
		// step 2.5
		ReloadZcullMemory();
		pPortalQuadMesh = NULL;	// NULL it out so we know to restore our dynamic mesh later
#endif // _PS3

		if ( bPushedScissor )
		{
			pRenderContext->PopScissorRect();
		}
	}

	int nParentStencilValue = 0;
	m_stencilValueStack.Pop( nParentStencilValue );

	//step 6, go back to non-stencil rendering mode in preparation to resume normal scene rendering
	if( m_iViewRecursionLevel == 0 )
	{
		Assert( m_pRenderingViewForPortal == NULL );
		Assert( m_pRenderingViewExitPortal == NULL );
		m_pRenderingViewExitPortal = NULL;
		m_pRenderingViewForPortal = NULL;

		m_StencilState.m_bEnable = false;
		m_StencilState.m_CompareFunc = SHADER_STENCILFUNC_NEVER;
		m_StencilState.m_PassOp = SHADER_STENCILOP_KEEP;
		m_StencilState.m_FailOp = SHADER_STENCILOP_KEEP;
		m_StencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
		m_StencilState.m_nTestMask = 0;
		m_StencilState.m_nWriteMask = 0;
		m_StencilState.m_nReferenceValue = 0;
		pRenderContext->SetStencilState( m_StencilState );

		m_RecursiveViewComplexFrustums[0].RemoveAll();
	}
	else
	{
		m_StencilState.m_bEnable = true;
		m_StencilState.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
		m_StencilState.m_PassOp = SHADER_STENCILOP_KEEP;
		m_StencilState.m_FailOp = SHADER_STENCILOP_KEEP;
		m_StencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
		m_StencilState.m_nTestMask = 0xFF;
		m_StencilState.m_nWriteMask = 0xFF;
		m_StencilState.m_nReferenceValue = nParentStencilValue;
		pRenderContext->SetStencilState( m_StencilState );
	}

	if( bRebuildDrawListsWhenDone )
	{
		memcpy( (void *)pViewSetup, &ViewBackup, sizeof( CViewSetup ) ); //if we don't restore this, the view is permanently altered (in mid render of an existing scene)
	}

	pRenderContext->Flush( true ); //just in case

	++m_iRemainingPortalViewDepth;
		  
	pRenderContext->BeginPIXEvent( PIX_VALVE_ORANGE, "Portal_DrawPortalsEnd" );

	if ( pPortalQuadMesh == NULL )
	{
		Assert( m_pCachedPortalQuadMeshData );
		pPortalQuadMesh = pRenderContext->GetDynamicMeshEx( m_portalQuadMeshVertexFmt );
		pPortalQuadMesh->ReconstructFromCachedPerFrameMeshData( m_pCachedPortalQuadMeshData );
	}

	RenderPortalEffects( pRenderContext, pPortalQuadMesh, actualActivePortals, actualActivePortalQuadVBIndex );

	pRenderContext->EndPIXEvent();

	if ( pPortalQuadMesh )
	{
		pPortalQuadMesh->MarkAsDrawn();
	}
	return bRebuildDrawListsWhenDone;
}

void CPortalRender::RenderPortalEffects( IMatRenderContext *pRenderContext, IMesh *pPortalQuadMesh, const CUtlVector< CPortalRenderable* > &actualActivePortals, const CUtlVector< int > &actualActivePortalQuadVBIndex ) const
{
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	for( int i = 0; i < actualActivePortals.Count(); ++i )
	{
		CPortalRenderable *pCurrentPortal = actualActivePortals[i];

		if ( !pCurrentPortal->IsPropPortal() )
		{
			continue;
		}

		C_Prop_Portal *pPropPortal = (C_Prop_Portal *)pCurrentPortal;

		bool bDrawRing = true;
		int nRenderPassCount = pCurrentPortal->BindPortalMaterial( pRenderContext, 0, &bDrawRing );

		bDrawRing = bDrawRing && ( pPropPortal->ComputeStaticAmountForRendering() == 0.0f ) && ( pPropPortal->m_fOpenAmount >= 0.99f );

		int nStartIndex = bDrawRing ? m_clampedPortalMeshRenderInfos.Tail().nStartIndex : actualActivePortalQuadVBIndex[i] * 6;
		int nIndexCount = bDrawRing ? m_clampedPortalMeshRenderInfos.Tail().nIndexCount : 6;

		if ( bDrawRing )
		{
			matrix3x4_t matObjToWorld( pPropPortal->m_vRight, pPropPortal->m_vUp, pPropPortal->m_vForward, pPropPortal->m_ptOrigin );
			pRenderContext->LoadMatrix( matObjToWorld );
		}
		else
		{
			pRenderContext->LoadIdentity();
		}

		if ( nRenderPassCount > 0 )
		{
			// Draw first pass
			pPortalQuadMesh->Draw( nStartIndex, nIndexCount );
		}
		if ( nRenderPassCount > 1 )
		{
			bool bDummy;
			pCurrentPortal->BindPortalMaterial( pRenderContext, 1, &bDummy );
			// Draw 2nd pass
			pPortalQuadMesh->Draw( nStartIndex, nIndexCount );
		}
		Assert( nRenderPassCount <= 2 );
	}
	pRenderContext->PopMatrix();

}

//-----------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------
bool CPortalRender::DrawPortalsUsingStencils_Old( CViewRender *pViewRender )
{
	int iDrawFlags = pViewRender->GetDrawFlags();

	if ( (iDrawFlags & DF_RENDER_REFLECTION) != 0 )
		return false;

	if ( ((iDrawFlags & DF_CLIP_Z) != 0) && ((iDrawFlags & DF_CLIP_BELOW) == 0) ) //clipping above the water height
		return false;

	CMatRenderContextPtr pRenderContext( materials );

	int iNumRenderablePortals = m_ActivePortals.Count();

	// This loop is necessary because tools can suppress rendering without telling the portal system
	CUtlVector< CPortalRenderable* > actualActivePortals( 0, iNumRenderablePortals );
	for ( int i = 0; i < iNumRenderablePortals; ++i )
	{
		CPortalRenderable *pPortalRenderable = m_ActivePortals[i];
		C_BaseEntity *pPairedEntity = pPortalRenderable->PortalRenderable_GetPairedEntity();
		bool bIsVisible = (pPairedEntity == NULL) || (pPairedEntity->IsVisible() && pPairedEntity->ShouldDraw()); //either unknown visibility or definitely visible.

		// If the portal is associated with an entity, check to see if that entity is even in the PVS before rendering the portal
		if ( pPairedEntity != NULL && r_portal_use_pvs_optimization.GetBool() )
		{
			IClientRenderable *pCR = pPairedEntity->GetClientRenderable();
			bool bIsRenderable = g_pClientLeafSystem->IsRenderableInPVS( pCR );
			bIsVisible &= bIsRenderable;
		}

		if ( !pPortalRenderable->m_bIsPlaybackPortal )
		{
			if ( !bIsVisible )
			{
				//can't see through the portal, free up it's view id node for use elsewhere
				if( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pPortalRenderable->m_iPortalViewIDNodeIndex] != NULL )
				{
					FreePortalViewIDNode( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pPortalRenderable->m_iPortalViewIDNodeIndex] );
					m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pPortalRenderable->m_iPortalViewIDNodeIndex] = NULL;
				}

				continue;
			}
		}

		actualActivePortals.AddToTail( m_ActivePortals[i] );
	}
	iNumRenderablePortals = actualActivePortals.Count();

#ifdef PORTAL
	if( !ToolsEnabled() )
	{
		VPROF_BUDGET( "PortalGhosts", "PortalGhosts" );
		pRenderContext->BeginPIXEvent( PIX_VALVE_ORANGE, "Portal Ghosts" );
		C_Prop_Portal::DrawPortalGhostLocations( pRenderContext ); //we want to show where the portals are through walls, but not if occluded by a portal view, otherwise we're showing the player confusing data. This is the best place to wedge this
		pRenderContext->EndPIXEvent();
	}
#endif

	if( iNumRenderablePortals == 0 )
		return false;

	const int iMaxDepth = MIN( r_portal_stencil_depth.GetInt(), MIN( MAX_PORTAL_RECURSIVE_VIEWS, (1 << materials->StencilBufferBits()) ) - 1 );

	if( m_iViewRecursionLevel >= iMaxDepth ) //can't support any more views	
	{
		m_iRemainingPortalViewDepth = 0; //special case handler for max depth 0 cases
		pRenderContext->BeginPIXEvent( PIX_VALVE_ORANGE, "Portal_maxrecursion_reached" );
		for( int i = 0; i != iNumRenderablePortals; ++i )
		{
			CPortalRenderable *pCurrentPortal = actualActivePortals[i];
			pCurrentPortal->DrawPortal( pRenderContext );
		}
		pRenderContext->EndPIXEvent();
		return false;
	}

	m_iRemainingPortalViewDepth = (iMaxDepth - m_iViewRecursionLevel) - 1;

	pRenderContext->Flush( true ); //to prevent screwing up the last opaque object

	const CViewSetup *pViewSetup = pViewRender->GetViewSetup();
	m_RecursiveViewSetups[m_iViewRecursionLevel] = *pViewSetup;

	CViewSetup ViewBackup;// = *pViewSetup; //backup the view, we'll need to restore it
	memcpy( &ViewBackup, pViewSetup, sizeof( CViewSetup ) );

	Vector ptCameraOrigin = pViewSetup->origin;
	Vector vCameraForward;
	AngleVectors( pViewSetup->angles, &vCameraForward, NULL, NULL );

	int iX, iY, iWidth, iHeight;
	pRenderContext->GetViewport( iX, iY, iWidth, iHeight );
#ifndef TEMP_DISABLE_PORTAL_VIS_QUERY
	int iScreenPixelCount = iWidth * iHeight;
#endif

	bool bRebuildDrawListsWhenDone = false;


	int iParentLevelStencilReferenceValue = m_iViewRecursionLevel;
	int iStencilReferenceValue = iParentLevelStencilReferenceValue + 1;
	
	if( m_iViewRecursionLevel == 0 ) //first entry into the stencil drawing
	{
		m_StencilState.m_bEnable = true;
		m_StencilState.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
		m_StencilState.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
		m_StencilState.m_FailOp = SHADER_STENCILOP_KEEP;
		m_StencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
		m_StencilState.m_nTestMask = 0xFF;
		m_StencilState.m_nWriteMask = 0xFF;
		m_StencilState.m_nReferenceValue = 0;
		pRenderContext->SetStencilState( m_StencilState );
		pRenderContext->PerformFullScreenStencilOperation();

        m_RecursiveViewComplexFrustums[0].RemoveAll(); //clear any garbage leftover in the complex frustums from last frame
	}

	if( m_RecursiveViewComplexFrustums[m_iViewRecursionLevel].Count() == 0 )
	{
		//nothing in the complex frustum from the current view, copy the standard frustum in
		m_RecursiveViewComplexFrustums[m_iViewRecursionLevel].AddMultipleToTail( FRUSTUM_NUMPLANES, pViewRender->GetFrustum() );
	}

	for( int i = 0; i < iNumRenderablePortals; ++i )
	{
		CPortalRenderable *pCurrentPortal = actualActivePortals[i];

		m_RecursiveViewComplexFrustums[m_iViewRecursionLevel + 1].RemoveAll(); //clear any previously stored complex frustum
		
		if( (pCurrentPortal->GetLinkedPortal() == NULL) ||
			(pCurrentPortal == m_pRenderingViewExitPortal) ||
			(pCurrentPortal->ShouldUpdatePortalView_BasedOnView( *pViewSetup, m_RecursiveViewComplexFrustums[m_iViewRecursionLevel] ) == false) )
		{
			//can't see through the portal, free up it's view id node for use elsewhere
			if( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] != NULL )
			{
				FreePortalViewIDNode( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] );
				m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] = NULL;
			}
			continue;
		}

		Assert( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes.Count() > pCurrentPortal->m_iPortalViewIDNodeIndex );

		if( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] == NULL )
		{
			C_Prop_Portal *pPropPortal = static_cast<C_Prop_Portal*>( pCurrentPortal );
			int nPortalIndex = pPropPortal->m_bIsPortal2 ? 1 : 0;
			int nTeamIndex = pPropPortal->GetTeamNumber();
			m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] = AllocPortalViewIDNode( m_HeadPortalViewIDNode.ChildNodes.Count(), nPortalIndex, nTeamIndex, CurrentViewID() );
		}

//		PortalViewIDNode_t *pCurrentPortalViewNode = m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex];
		
		// Step 0, Allow for special effects to happen before cutting a hole
		{
			pRenderContext->BeginPIXEvent( PIX_VALVE_ORANGE, "Portal_Step0" );
			m_StencilState.m_bEnable = true;
			m_StencilState.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
			m_StencilState.m_PassOp = SHADER_STENCILOP_KEEP;
			m_StencilState.m_FailOp = SHADER_STENCILOP_KEEP;
			m_StencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
			m_StencilState.m_nTestMask = 0xFF;
			m_StencilState.m_nWriteMask = 0xFF;
			m_StencilState.m_nReferenceValue = iParentLevelStencilReferenceValue;
			pRenderContext->SetStencilState( m_StencilState );

			pCurrentPortal->DrawPreStencilMask( pRenderContext );
			pRenderContext->EndPIXEvent();
		}

		//step 1, write out the stencil values (and colors if you want, but really not necessary)
		{
			pRenderContext->BeginPIXEvent( PIX_VALVE_ORANGE, "Portal_Step1" );
			m_StencilState.m_PassOp = SHADER_STENCILOP_INCREMENT_CLAMP;
			pRenderContext->SetStencilState( m_StencilState );

#ifndef TEMP_DISABLE_PORTAL_VIS_QUERY
			pRenderContext->BeginOcclusionQueryDrawing( pCurrentPortalViewNode->occlusionQueryHandle );
			pCurrentPortalViewNode->iWindowPixelsAtQueryTime = iScreenPixelCount;
#endif

			pCurrentPortal->DrawStencilMask( pRenderContext );

#ifndef TEMP_DISABLE_PORTAL_VIS_QUERY
			pRenderContext->EndOcclusionQueryDrawing( pCurrentPortalViewNode->occlusionQueryHandle );
#endif
			pRenderContext->EndPIXEvent();
		}

		{
			//step 2, clear the depth buffer in stencil areas so we can render a new scene to them
			{
				pRenderContext->BeginPIXEvent( PIX_VALVE_ORANGE, "Portal_Step2" );
				m_StencilState.m_PassOp = SHADER_STENCILOP_KEEP;
				m_StencilState.m_nReferenceValue = iStencilReferenceValue;
				pRenderContext->SetStencilState( m_StencilState );

				pRenderContext->ClearBuffersObeyStencil( false, true );
				pRenderContext->EndPIXEvent();
			}

#ifdef _PS3
			// step 2.5
			ReloadZcullMemory();
#endif // _PS3

			//step 3, fill in stencil views (remember that in multiple depth situations that any subportals will run through this function again before this section completes, thereby screwing with stencil settings)
			{
				pRenderContext->BeginPIXEvent( PIX_VALVE_ORANGE, "Portal_Step3" );
//				VPROF_BUDGET( "Portal_FillInStencilViews", "Portal_FillInStencilViews" );
				bRebuildDrawListsWhenDone = true;

				MaterialFogMode_t fogModeBackup = pRenderContext->GetFogMode();
				unsigned char fogColorBackup[4];
				pRenderContext->GetFogColor( fogColorBackup );
				float fFogStartBackup, fFogEndBackup, fFogZBackup;
				pRenderContext->GetFogDistances( &fFogStartBackup, &fFogEndBackup, &fFogZBackup );
				CGlowOverlay::BackupSkyOverlayData( m_iViewRecursionLevel );

				Assert( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes.Count() > pCurrentPortal->m_iPortalViewIDNodeIndex );

				m_PortalViewIDNodeChain[m_iViewRecursionLevel + 1] = m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex];
				
				if ( r_portalstencildisable.GetBool() )
				{
					m_StencilState.m_bEnable = true;
					m_StencilState.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
					m_StencilState.m_PassOp = SHADER_STENCILOP_KEEP;
					m_StencilState.m_FailOp = SHADER_STENCILOP_KEEP;
					m_StencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
					m_StencilState.m_nTestMask = 0xFF;
					m_StencilState.m_nWriteMask = 0xFF;
					m_StencilState.m_nReferenceValue = iStencilReferenceValue;
					pRenderContext->SetStencilState( m_StencilState );

					pRenderContext->ClearBuffers( true, true );
				}

				pCurrentPortal->RenderPortalViewToBackBuffer( pViewRender, *pViewSetup );
				
				m_PortalViewIDNodeChain[m_iViewRecursionLevel + 1] = NULL;

				// This value is incremented to signal that we need to re-copy the refract texture 
				// because we've finished rendering a child portal and its translucent pass, and we're about
				// to render the parent portal's translucent pass or another portal
				++ g_nCurrentPortalRender;
				
				CGlowOverlay::RestoreSkyOverlayData( m_iViewRecursionLevel );
				memcpy( (void *)pViewSetup, &ViewBackup, sizeof( CViewSetup ) );
				pViewRender->m_pActiveRenderer->EnableWorldFog();

				pRenderContext->FogMode( fogModeBackup );
				pRenderContext->FogColor3ubv( fogColorBackup );
				pRenderContext->FogStart( fFogStartBackup );
				pRenderContext->FogEnd( fFogEndBackup );
				pRenderContext->SetFogZ( fFogZBackup );

				
				//do a full reset of what we think the stencil operations are in case the recursive calls got weird
				m_StencilState.m_bEnable = true;
				m_StencilState.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
				m_StencilState.m_PassOp = SHADER_STENCILOP_KEEP;
				m_StencilState.m_FailOp = SHADER_STENCILOP_KEEP;
				m_StencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
				m_StencilState.m_nTestMask = 0xFF;
				m_StencilState.m_nWriteMask = 0xFF;
				m_StencilState.m_nReferenceValue = iStencilReferenceValue;
				pRenderContext->SetStencilState( m_StencilState );
				pRenderContext->EndPIXEvent();
			}

			//step 4, patch up the fact that we just made a hole in the wall because it's not *really* a hole at all
			{
				pRenderContext->BeginPIXEvent( PIX_VALVE_ORANGE, "Portal_Step4" );
				pCurrentPortal->DrawPostStencilFixes( pRenderContext );
				pRenderContext->EndPIXEvent();
			}
		}

		//step 5, restore the stencil mask to the parent level
		{
			pRenderContext->BeginPIXEvent( PIX_VALVE_ORANGE, "Portal_Step5" );
			m_StencilState.m_PassOp = SHADER_STENCILOP_DECREMENT_CLAMP;
			pRenderContext->SetStencilState( m_StencilState );

			pRenderContext->PerformFullScreenStencilOperation();
			pRenderContext->EndPIXEvent();
		}
	}

	//step 6, go back to non-stencil rendering mode in preparation to resume normal scene rendering
	if( m_iViewRecursionLevel == 0 )
	{
		Assert( m_pRenderingViewForPortal == NULL );
		Assert( m_pRenderingViewExitPortal == NULL );
		m_pRenderingViewExitPortal = NULL;
		m_pRenderingViewForPortal = NULL;

		m_StencilState.m_bEnable = false;
		m_StencilState.m_CompareFunc = SHADER_STENCILFUNC_NEVER;
		m_StencilState.m_PassOp = SHADER_STENCILOP_KEEP;
		m_StencilState.m_FailOp = SHADER_STENCILOP_KEEP;
		m_StencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
		m_StencilState.m_nTestMask = 0;
		m_StencilState.m_nWriteMask = 0;
		m_StencilState.m_nReferenceValue = 0;
		pRenderContext->SetStencilState( m_StencilState );

		m_RecursiveViewComplexFrustums[0].RemoveAll();
	}
	else
	{
		m_StencilState.m_PassOp = SHADER_STENCILOP_KEEP;		
		m_StencilState.m_nReferenceValue = iParentLevelStencilReferenceValue;
		pRenderContext->SetStencilState( m_StencilState );
	}

	if( bRebuildDrawListsWhenDone )
	{
		memcpy( (void *)pViewSetup, &ViewBackup, sizeof( CViewSetup ) ); //if we don't restore this, the view is permanently altered (in mid render of an existing scene)
	}

	pRenderContext->Flush( true ); //just in case

	++m_iRemainingPortalViewDepth;

	pRenderContext->BeginPIXEvent( PIX_VALVE_ORANGE, "Portal_DrawPortalsEnd" );
	for( int i = 0; i < iNumRenderablePortals; ++i )
	{
		CPortalRenderable *pCurrentPortal = actualActivePortals[i];
		pCurrentPortal->DrawPortal( pRenderContext );
	}
	pRenderContext->EndPIXEvent();

	return bRebuildDrawListsWhenDone;
}
//-----------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------

void CPortalRender::AddPortal( CPortalRenderable *pPortal )
{
	for( int i = m_ActivePortals.Count(); --i >= 0; )
	{
		if( m_ActivePortals[i] == pPortal )
			return;
	}

	m_ActivePortals.AddToTail( pPortal );
}

void CPortalRender::RemovePortal( CPortalRenderable *pPortal )
{
	for( int i = m_ActivePortals.Count(); --i >= 0; )
	{
		if( m_ActivePortals[i] == pPortal )
		{
			m_ActivePortals.FastRemove( i );
			return;
		}
	}
}


//-----------------------------------------------------------------------------
// Are we currently rendering a portal?
//-----------------------------------------------------------------------------
bool CPortalRender::IsRenderingPortal() const
{
	return m_pRenderingViewForPortal != NULL;
}


//-----------------------------------------------------------------------------
// Returns view recursion level
//-----------------------------------------------------------------------------
int CPortalRender::GetViewRecursionLevel() const
{
	return m_iViewRecursionLevel;
}

//-----------------------------------------------------------------------------
//normalized for how many of the screen's possible pixels it takes up, less than zero indicates a lack of data from last frame
//-----------------------------------------------------------------------------
float CPortalRender::GetPixelVisilityForPortalSurface( const CPortalRenderable *pPortal ) const
{
	PortalViewIDNode_t *pNode = m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pPortal->m_iPortalViewIDNodeIndex];
	if( pNode )
		return pNode->fScreenFilledByPortalSurfaceLastFrame_Normalized;

	return -1.0f;
}


//-----------------------------------------------------------------------------
// Methods to query about the exit portal associated with the currently rendering portal
//-----------------------------------------------------------------------------
const Vector &CPortalRender::GetExitPortalFogOrigin() const
{
	return m_pRenderingViewExitPortal->GetFogOrigin();
}

void CPortalRender::ShiftFogForExitPortalView() const
{
	if ( m_pRenderingViewExitPortal )
	{
		m_pRenderingViewExitPortal->ShiftFogForExitPortalView();
	}
}

float CPortalRender::GetCurrentPortalDistanceBias() const
{
	if ( m_pRenderingViewExitPortal )
	{
		return m_pRenderingViewExitPortal->GetPortalDistanceBias();
	}
	return 0.0f;
}

void CPortalRenderable::ShiftFogForExitPortalView() const
{
	CMatRenderContextPtr pRenderContext( materials );
	float fFogStart, fFogEnd, fFogZ;
	pRenderContext->GetFogDistances( &fFogStart, &fFogEnd, &fFogZ );

	Vector vFogOrigin = GetFogOrigin();
	Vector vCameraToExitPortal = vFogOrigin - CurrentViewOrigin();
	float fDistModifier = vCameraToExitPortal.Dot( CurrentViewForward() );

	fFogStart += fDistModifier;
	fFogEnd += fDistModifier;
	//fFogZ += something; //FIXME: find out what the hell to do with this

	pRenderContext->FogStart( fFogStart );
	pRenderContext->FogEnd( fFogEnd );
	pRenderContext->SetFogZ( fFogZ );
}

SkyboxVisibility_t CPortalRender::IsSkyboxVisibleFromExitPortal() const
{
	return m_pRenderingViewExitPortal->SkyBoxVisibleFromPortal();
}

bool CPortalRender::DoesExitPortalViewIntersectWaterPlane( float waterZ, int leafWaterDataID ) const
{
	return m_pRenderingViewExitPortal->DoesExitViewIntersectWaterPlane( waterZ, leafWaterDataID );
}


//-----------------------------------------------------------------------------
// Returns the remaining number of portals to render within other portals
// lets portals know that they should do "end of the line" kludges to cover up that portals don't go infinitely recursive
//-----------------------------------------------------------------------------
int	CPortalRender::GetRemainingPortalViewDepth() const
{
	return m_iRemainingPortalViewDepth;
}


//-----------------------------------------------------------------------------
// Returns the current View IDs 
//-----------------------------------------------------------------------------
int CPortalRender::GetCurrentViewId() const
{
	Assert( m_PortalViewIDNodeChain[m_iViewRecursionLevel] != NULL );
#ifdef _DEBUG
	for( int i = 0; i != m_iViewRecursionLevel; ++i )
	{
		Assert( m_PortalViewIDNodeChain[i]->iPrimaryViewID != m_PortalViewIDNodeChain[m_iViewRecursionLevel]->iPrimaryViewID );
	}
#endif

	return m_PortalViewIDNodeChain[m_iViewRecursionLevel]->iPrimaryViewID;
}

int CPortalRender::GetCurrentSkyboxViewId() const
{
	Assert( m_PortalViewIDNodeChain[m_iViewRecursionLevel] != NULL );
	return m_PortalViewIDNodeChain[m_iViewRecursionLevel]->iPrimaryViewID + 1;
}


void OverlayCameraRenderTarget( const char *pszMaterialName, float flX, float flY, float w, float h ); //implemented in view_scene.cpp

void CPortalRender::OverlayPortalRenderTargets( float w, float h )
{
	OverlayCameraRenderTarget( "engine/debug_portal_1", 0,0, w,h );
	OverlayCameraRenderTarget( "engine/debug_portal_2", w+10,0, w,h );

	OverlayCameraRenderTarget( "engine/debug_water_reflect_0", 0, h+10, w,h );
	OverlayCameraRenderTarget( "engine/debug_water_reflect_1", w+10, h+10, w,h );
	OverlayCameraRenderTarget( "engine/debug_water_reflect_2", (w+10) * 2, h+10, w,h );

	OverlayCameraRenderTarget( "engine/debug_water_refract_0", 0, (h+10) * 2, w,h );
	OverlayCameraRenderTarget( "engine/debug_water_refract_1", w+10, (h+10) * 2, w,h );
	OverlayCameraRenderTarget( "engine/debug_water_refract_2", (w+10) * 2, (h+10) * 2, w,h );
}

void CPortalRender::UpdateDepthDoublerTexture( const CViewSetup &viewSetup )
{
	if( DepthDoublerPIPDisableCheck() )
		return;

	bool bShouldUpdate = false;

	for( int i = m_ActivePortals.Count(); --i >= 0; )
	{
		CPortalRenderable *pPortal = m_ActivePortals[i];

		if( pPortal->ShouldUpdateDepthDoublerTexture( viewSetup ) )
		{
			bShouldUpdate = true;
			break;
		}
	}
	
	if( bShouldUpdate )
	{
		Rect_t srcRect;
		srcRect.x = viewSetup.x;
		srcRect.y = viewSetup.y;
		srcRect.width = viewSetup.width;
		srcRect.height = viewSetup.height;

		ITexture *pTexture = portalrendertargets->GetDepthDoublerTexture();

		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->CopyRenderTargetToTextureEx( pTexture, 0, &srcRect, &srcRect );
	}
}

bool CPortalRender::DepthDoublerPIPDisableCheck( void )
{
	int slot = GET_ACTIVE_SPLITSCREEN_SLOT();
	return (slot != 0) && VGui_IsSplitScreenPIP();
}

//-----------------------------------------------------------------------------
// Finds a recorded portal
//-----------------------------------------------------------------------------
int CPortalRender::FindRecordedPortalIndex( int nPortalId )
{
	int nCount = m_RecordedPortals.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_RecordedPortals[i].m_nPortalId == nPortalId )
			return i;
	}
	return -1;
}

CPortalRenderable* CPortalRender::FindRecordedPortal( int nPortalId )
{
	int nIndex = FindRecordedPortalIndex( nPortalId );
	return ( nIndex >= 0 ) ? m_RecordedPortals[nIndex].m_pActivePortal : NULL;
}

CPortalRenderable* CPortalRender::FindRecordedPortal( IClientRenderable *pRenderable )
{
	int nCount = m_RecordedPortals.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_RecordedPortals[i].m_pPlaybackRenderable == pRenderable )
			return m_RecordedPortals[i].m_pActivePortal;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Handles a portal update message
//-----------------------------------------------------------------------------
void CPortalRender::HandlePortalPlaybackMessage( KeyValues *pKeyValues )
{
	// Iterate through all the portal ids of all the portals in the keyvalues message
	CUtlVector<int> foundIds;
	for ( KeyValues *pCurr = pKeyValues->GetFirstTrueSubKey(); pCurr; pCurr = pCurr->GetNextTrueSubKey() )
	{
		// Create new area portals for those ids that don't exist
        int nPortalId = pCurr->GetInt( "portalId" );
		IClientRenderable *pRenderable = (IClientRenderable*)pCurr->GetPtr( "clientRenderable" );
		int nIndex = FindRecordedPortalIndex( nPortalId );
		if ( nIndex < 0 )
		{
			CPortalRenderable *pPortal = NULL;
			const char *szType = pCurr->GetString( "portalType", "Flat Basic" ); //"Flat Basic" being the type commonly found in "Portal" mod
			//search through registered creation functions for one that makes this type of portal
			const CPortalRenderableCreator_AutoRegister *pCreationFuncs = CPortalRenderableCreator_AutoRegister::s_pRegisteredTypes;
			while( pCreationFuncs != NULL )
			{
				if( FStrEq( szType, pCreationFuncs->m_szPortalType ) )
				{
					pPortal = pCreationFuncs->m_creationFunc();
					break;
				}

				pCreationFuncs = pCreationFuncs->m_pNext;
			}

			if( pPortal == NULL )
			{
				AssertMsg( false, "Unable to find creation function for portal type." );
				Warning( "CPortalRender::HandlePortalPlaybackMessage() unable to find creation function for portal type: %s\n", szType );
			}
			else
			{
				pPortal->m_bIsPlaybackPortal = true;
				int k = m_RecordedPortals.AddToTail( );
				m_RecordedPortals[k].m_pActivePortal = pPortal;
				m_RecordedPortals[k].m_nPortalId = nPortalId;
				m_RecordedPortals[k].m_pPlaybackRenderable = pRenderable;
				AddPortal( pPortal );
			}
		}
		else
		{
			m_RecordedPortals[nIndex].m_pPlaybackRenderable = pRenderable;
		}
		foundIds.AddToTail( nPortalId );
	}

	// Delete portals that didn't appear in the list
	int nFoundCount = foundIds.Count();
	int nCount = m_RecordedPortals.Count();
	for ( int i = nCount; --i >= 0; )
	{
		int j;
		for ( j = 0; j < nFoundCount; ++j )
		{
			if ( foundIds[j] == m_RecordedPortals[i].m_nPortalId )
				break;
		}

		if ( j == nFoundCount )
		{
			RemovePortal( m_RecordedPortals[i].m_pActivePortal );
			delete m_RecordedPortals[i].m_pActivePortal;
			m_RecordedPortals.FastRemove(i);
		}
	}

	// Iterate through all the portal ids of all the portals in the keyvalues message
	for ( KeyValues *pCurr = pKeyValues->GetFirstTrueSubKey(); pCurr; pCurr = pCurr->GetNextTrueSubKey() )
	{
		// Update the state of the portals based on the recorded info
		int nPortalId = pCurr->GetInt( "portalId" );
		CPortalRenderable *pPortal = FindRecordedPortal( nPortalId );
		Assert( pPortal );

		pPortal->HandlePortalPlaybackMessage( pCurr );
	}

	// Make the portals update their internal state
	/*nCount = m_RecordedPortals.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		m_RecordedPortals[i].m_pActivePortal->PortalMoved();
		m_RecordedPortals[i].m_pActivePortal->ComputeLinkMatrix();
	}*/
}



bool Recursive_IsPortalViewID( PortalViewIDNode_t *pNode, view_id_t id )
{	
	if ( pNode->iPrimaryViewID == id )
		return true;

	for( int i = pNode->ChildNodes.Count(); --i >= 0; )
	{
		PortalViewIDNode_t *pChildNode = pNode->ChildNodes[i];
		if( pChildNode )
		{
			return Recursive_IsPortalViewID( pChildNode, id );
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Tests the parameter view ID against ID's used by portal pixel vis queries
// Input  : id - id tested against used portal view ids
// Output : Returns true if id matches an ID used by a portal, or it's recursive sub portals
//-----------------------------------------------------------------------------
bool CPortalRender::IsPortalViewID( view_id_t id )
{
	if ( id == m_HeadPortalViewIDNode.iPrimaryViewID )
		return true;

	for ( int i = 0; i < MAX_PORTAL_RECURSIVE_VIEWS; ++i )
	{
		PortalViewIDNode_t* pNode = m_PortalViewIDNodeChain[i];
		if ( pNode )
		{
			// recursively search child nodes, they get their own ids.
			if ( Recursive_IsPortalViewID( pNode, id ) )
				return true;
		}
	}

	return false;
}






